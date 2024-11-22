#[cfg(test)]
mod tests;

use crate::consumer::{Consumer, ConsumerId, ConsumerOptions};
use crate::data_consumer::{DataConsumer, DataConsumerId, DataConsumerOptions, DataConsumerType};
use crate::data_producer::{DataProducer, DataProducerId, DataProducerOptions, DataProducerType};
use crate::data_structures::{
    AppData, DtlsParameters, DtlsState, IceCandidate, IceParameters, IceRole, IceState, ListenInfo,
    SctpState, TransportTuple,
};
use crate::messages::{
    TransportCloseRequest, TransportRestartIceRequest, WebRtcTransportConnectRequest,
    WebRtcTransportData,
};
use crate::producer::{Producer, ProducerId, ProducerOptions};
use crate::router::transport::{TransportImpl, TransportType};
use crate::router::Router;
use crate::sctp_parameters::{NumSctpStreams, SctpParameters};
use crate::transport::{
    ConsumeDataError, ConsumeError, ProduceDataError, ProduceError, RecvRtpHeaderExtensions,
    RtpListener, SctpListener, Transport, TransportGeneric, TransportId, TransportTraceEventData,
    TransportTraceEventType,
};
use crate::webrtc_server::WebRtcServer;
use crate::worker::{Channel, NotificationParseError, RequestError, SubscriptionHandler};
use async_executor::Executor;
use async_trait::async_trait;
use event_listener_primitives::{Bag, BagOnce, HandlerId};
use log::{debug, error};
use mediasoup_sys::fbs::{notification, response, transport, web_rtc_transport};
use nohash_hasher::IntMap;
use parking_lot::Mutex;
use serde::{Deserialize, Serialize};
use std::convert::TryFrom;
use std::error::Error;
use std::fmt;
use std::ops::Deref;
use std::sync::atomic::{AtomicBool, AtomicUsize, Ordering};
use std::sync::{Arc, Weak};
use thiserror::Error;

/// Struct that protects an invariant of having non-empty list of listen IPs
#[derive(Debug, Clone, Eq, PartialEq, Serialize)]
pub struct WebRtcTransportListenInfos(Vec<ListenInfo>);

impl WebRtcTransportListenInfos {
    /// Create transport listen IPs with given IP populated initially.
    #[must_use]
    pub fn new(listen_info: ListenInfo) -> Self {
        Self(vec![listen_info])
    }

    /// Insert another listen IP.
    #[must_use]
    pub fn insert(mut self, listen_info: ListenInfo) -> Self {
        self.0.push(listen_info);
        self
    }
}

impl Deref for WebRtcTransportListenInfos {
    type Target = Vec<ListenInfo>;

    fn deref(&self) -> &Self::Target {
        &self.0
    }
}

/// Empty list of listen IPs provided, should have at least one element
#[derive(Error, Debug, Eq, PartialEq)]
#[error("Empty list of listen IPs provided, should have at least one element")]
pub struct EmptyListError;

impl TryFrom<Vec<ListenInfo>> for WebRtcTransportListenInfos {
    type Error = EmptyListError;

    fn try_from(listen_infos: Vec<ListenInfo>) -> Result<Self, Self::Error> {
        if listen_infos.is_empty() {
            Err(EmptyListError)
        } else {
            Ok(Self(listen_infos))
        }
    }
}

/// How [`WebRtcTransport`] should listen on interfaces.
///
/// # Notes on usage
/// * Do not use "0.0.0.0" into `listen_infos`. Values in `listen_infos` must be specific bindable IPs
///   on the host.
/// * If you use "0.0.0.0" or "::" into `listen_infos`, then you need to also provide
/// `announced_address` in the corresponding entry in `listen_infos`.
#[derive(Debug, Clone)]
pub enum WebRtcTransportListen {
    /// Listen on individual protocol/IP/port combinations specific to this transport.
    Individual {
        /// Listening infos in order of preference (first one is the preferred one).
        listen_infos: WebRtcTransportListenInfos,
    },
    /// Share [`WebRtcServer`] with other transports withing the same worker.
    Server {
        /// [`WebRtcServer`] to use.
        webrtc_server: WebRtcServer,
    },
}

/// [`WebRtcTransport`] options.
///
/// # Notes on usage
/// * `initial_available_outgoing_bitrate` is just applied when the consumer endpoint supports REMB
///   or Transport-CC.
#[derive(Debug, Clone)]
#[non_exhaustive]
pub struct WebRtcTransportOptions {
    /// How [`WebRtcTransport`] should listen on interfaces.
    pub listen: WebRtcTransportListen,
    /// Initial available outgoing bitrate (in bps).
    /// Default 600000.
    pub initial_available_outgoing_bitrate: u32,
    /// Enable UDP.
    /// Default true.
    pub enable_udp: bool,
    /// Enable TCP.
    /// Default true if webrtc_server is given, false otherwise.
    pub enable_tcp: bool,
    /// Prefer UDP.
    /// Default false.
    pub prefer_udp: bool,
    /// Prefer TCP.
    /// Default false.
    pub prefer_tcp: bool,
    /// ICE consent timeout (in seconds). If 0 it is disabled.
    /// Default 30.
    pub ice_consent_timeout: u8,
    /// Create a SCTP association.
    /// Default false.
    pub enable_sctp: bool,
    /// SCTP streams number.
    pub num_sctp_streams: NumSctpStreams,
    /// Maximum allowed size for SCTP messages sent by DataProducers.
    // 	Default 262144.
    pub max_sctp_message_size: u32,
    /// Maximum SCTP send buffer used by DataConsumers.
    /// Default 262144.
    pub sctp_send_buffer_size: u32,
    /// Custom application data.
    pub app_data: AppData,
}

impl WebRtcTransportOptions {
    /// Create [`WebRtcTransport`] options with given listen infos.
    #[must_use]
    pub fn new(listen_infos: WebRtcTransportListenInfos) -> Self {
        Self {
            listen: WebRtcTransportListen::Individual { listen_infos },
            initial_available_outgoing_bitrate: 600_000,
            enable_udp: true,
            enable_tcp: false,
            prefer_udp: false,
            prefer_tcp: false,
            ice_consent_timeout: 30,
            enable_sctp: false,
            num_sctp_streams: NumSctpStreams::default(),
            max_sctp_message_size: 262_144,
            sctp_send_buffer_size: 262_144,
            app_data: AppData::default(),
        }
    }
    /// Create [`WebRtcTransport`] options with given [`WebRtcServer`].
    #[must_use]
    pub fn new_with_server(webrtc_server: WebRtcServer) -> Self {
        Self {
            listen: WebRtcTransportListen::Server { webrtc_server },
            initial_available_outgoing_bitrate: 600_000,
            enable_udp: true,
            enable_tcp: true,
            prefer_udp: false,
            prefer_tcp: false,
            ice_consent_timeout: 30,
            enable_sctp: false,
            num_sctp_streams: NumSctpStreams::default(),
            max_sctp_message_size: 262_144,
            sctp_send_buffer_size: 262_144,
            app_data: AppData::default(),
        }
    }
}

#[derive(Debug, Clone, Eq, PartialEq, Deserialize, Serialize)]
#[serde(rename_all = "camelCase")]
#[doc(hidden)]
#[non_exhaustive]
pub struct WebRtcTransportDump {
    // Common to all Transports.
    pub id: TransportId,
    pub direct: bool,
    pub producer_ids: Vec<ProducerId>,
    pub consumer_ids: Vec<ConsumerId>,
    pub map_ssrc_consumer_id: IntMap<u32, ConsumerId>,
    pub map_rtx_ssrc_consumer_id: IntMap<u32, ConsumerId>,
    pub data_producer_ids: Vec<DataProducerId>,
    pub data_consumer_ids: Vec<DataConsumerId>,
    pub recv_rtp_header_extensions: RecvRtpHeaderExtensions,
    pub rtp_listener: RtpListener,
    pub max_message_size: u32,
    pub sctp_parameters: Option<SctpParameters>,
    pub sctp_state: Option<SctpState>,
    pub sctp_listener: Option<SctpListener>,
    pub trace_event_types: Vec<TransportTraceEventType>,
    // WebRtcTransport specific.
    pub dtls_parameters: DtlsParameters,
    pub dtls_state: DtlsState,
    pub ice_candidates: Vec<IceCandidate>,
    pub ice_parameters: IceParameters,
    pub ice_role: IceRole,
    pub ice_state: IceState,
    pub ice_selected_tuple: Option<TransportTuple>,
}

impl WebRtcTransportDump {
    pub(crate) fn from_fbs(
        dump: web_rtc_transport::DumpResponse,
    ) -> Result<Self, Box<dyn Error + Send + Sync>> {
        Ok(Self {
            // Common to all Transports.
            id: dump.base.id.parse()?,
            direct: false,
            producer_ids: dump
                .base
                .producer_ids
                .iter()
                .map(|producer_id| Ok(producer_id.parse()?))
                .collect::<Result<_, Box<dyn Error + Send + Sync>>>()?,
            consumer_ids: dump
                .base
                .consumer_ids
                .iter()
                .map(|consumer_id| Ok(consumer_id.parse()?))
                .collect::<Result<_, Box<dyn Error + Send + Sync>>>()?,
            map_ssrc_consumer_id: dump
                .base
                .map_ssrc_consumer_id
                .iter()
                .map(|key_value| Ok((key_value.key, key_value.value.parse()?)))
                .collect::<Result<_, Box<dyn Error + Send + Sync>>>()?,
            map_rtx_ssrc_consumer_id: dump
                .base
                .map_rtx_ssrc_consumer_id
                .iter()
                .map(|key_value| Ok((key_value.key, key_value.value.parse()?)))
                .collect::<Result<_, Box<dyn Error + Send + Sync>>>()?,
            data_producer_ids: dump
                .base
                .data_producer_ids
                .iter()
                .map(|data_producer_id| Ok(data_producer_id.parse()?))
                .collect::<Result<_, Box<dyn Error + Send + Sync>>>()?,
            data_consumer_ids: dump
                .base
                .data_consumer_ids
                .iter()
                .map(|data_consumer_id| Ok(data_consumer_id.parse()?))
                .collect::<Result<_, Box<dyn Error + Send + Sync>>>()?,
            recv_rtp_header_extensions: RecvRtpHeaderExtensions::from_fbs(
                dump.base.recv_rtp_header_extensions.as_ref(),
            ),
            rtp_listener: RtpListener::from_fbs(dump.base.rtp_listener.as_ref())?,
            max_message_size: dump.base.max_message_size,
            sctp_parameters: dump
                .base
                .sctp_parameters
                .as_ref()
                .map(|parameters| SctpParameters::from_fbs(parameters.as_ref())),
            sctp_state: dump
                .base
                .sctp_state
                .map(|state| SctpState::from_fbs(&state)),
            sctp_listener: dump.base.sctp_listener.as_ref().map(|listener| {
                SctpListener::from_fbs(listener.as_ref()).expect("Error parsing SctpListner")
            }),
            trace_event_types: dump
                .base
                .trace_event_types
                .iter()
                .map(TransportTraceEventType::from_fbs)
                .collect(),
            // WebRtcTransport specific.
            dtls_parameters: DtlsParameters::from_fbs(*dump.dtls_parameters),
            dtls_state: DtlsState::from_fbs(dump.dtls_state),
            ice_candidates: dump
                .ice_candidates
                .iter()
                .map(IceCandidate::from_fbs)
                .collect(),
            ice_parameters: IceParameters::from_fbs(*dump.ice_parameters),
            ice_role: IceRole::from_fbs(dump.ice_role),
            ice_state: IceState::from_fbs(dump.ice_state),
            ice_selected_tuple: dump
                .ice_selected_tuple
                .map(|tuple| TransportTuple::from_fbs(tuple.as_ref())),
        })
    }
}

/// RTC statistics of the [`WebRtcTransport`].
#[derive(Debug, Clone, PartialOrd, PartialEq, Deserialize, Serialize)]
#[serde(rename_all = "camelCase")]
#[non_exhaustive]
#[allow(missing_docs)]
pub struct WebRtcTransportStat {
    // Common to all Transports.
    // `type` field is present in worker, but ignored here
    pub transport_id: TransportId,
    pub timestamp: u64,
    pub sctp_state: Option<SctpState>,
    pub bytes_received: u64,
    pub recv_bitrate: u32,
    pub bytes_sent: u64,
    pub send_bitrate: u32,
    pub rtp_bytes_received: u64,
    pub rtp_recv_bitrate: u32,
    pub rtp_bytes_sent: u64,
    pub rtp_send_bitrate: u32,
    pub rtx_bytes_received: u64,
    pub rtx_recv_bitrate: u32,
    pub rtx_bytes_sent: u64,
    pub rtx_send_bitrate: u32,
    pub probation_bytes_sent: u64,
    pub probation_send_bitrate: u32,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub available_outgoing_bitrate: Option<u32>,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub available_incoming_bitrate: Option<u32>,
    pub max_incoming_bitrate: Option<u32>,
    pub max_outgoing_bitrate: Option<u32>,
    pub min_outgoing_bitrate: Option<u32>,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub rtp_packet_loss_received: Option<f64>,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub rtp_packet_loss_sent: Option<f64>,
    // WebRtcTransport specific.
    pub ice_role: IceRole,
    pub ice_state: IceState,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub ice_selected_tuple: Option<TransportTuple>,
    pub dtls_state: DtlsState,
}

impl WebRtcTransportStat {
    pub(crate) fn from_fbs(
        stats: web_rtc_transport::GetStatsResponse,
    ) -> Result<Self, Box<dyn Error + Send + Sync>> {
        Ok(Self {
            transport_id: stats.base.transport_id.parse()?,
            timestamp: stats.base.timestamp,
            sctp_state: stats.base.sctp_state.as_ref().map(SctpState::from_fbs),
            bytes_received: stats.base.bytes_received,
            recv_bitrate: stats.base.recv_bitrate,
            bytes_sent: stats.base.bytes_sent,
            send_bitrate: stats.base.send_bitrate,
            rtp_bytes_received: stats.base.rtp_bytes_received,
            rtp_recv_bitrate: stats.base.rtp_recv_bitrate,
            rtp_bytes_sent: stats.base.rtp_bytes_sent,
            rtp_send_bitrate: stats.base.rtp_send_bitrate,
            rtx_bytes_received: stats.base.rtx_bytes_received,
            rtx_recv_bitrate: stats.base.rtx_recv_bitrate,
            rtx_bytes_sent: stats.base.rtx_bytes_sent,
            rtx_send_bitrate: stats.base.rtx_send_bitrate,
            probation_bytes_sent: stats.base.probation_bytes_sent,
            probation_send_bitrate: stats.base.probation_send_bitrate,
            available_outgoing_bitrate: stats.base.available_outgoing_bitrate,
            available_incoming_bitrate: stats.base.available_incoming_bitrate,
            max_incoming_bitrate: stats.base.max_incoming_bitrate,
            max_outgoing_bitrate: stats.base.max_outgoing_bitrate,
            min_outgoing_bitrate: stats.base.min_outgoing_bitrate,
            rtp_packet_loss_received: stats.base.rtp_packet_loss_received,
            rtp_packet_loss_sent: stats.base.rtp_packet_loss_sent,
            // WebRtcTransport specific.
            ice_role: IceRole::from_fbs(stats.ice_role),
            ice_state: IceState::from_fbs(stats.ice_state),
            ice_selected_tuple: stats
                .ice_selected_tuple
                .map(|tuple| TransportTuple::from_fbs(tuple.as_ref())),
            dtls_state: DtlsState::from_fbs(stats.dtls_state),
        })
    }
}
/// Remote parameters for [`WebRtcTransport`].
#[derive(Debug, Clone, PartialOrd, Eq, PartialEq, Deserialize, Serialize)]
#[serde(rename_all = "camelCase")]
pub struct WebRtcTransportRemoteParameters {
    /// Remote DTLS parameters.
    pub dtls_parameters: DtlsParameters,
}

#[derive(Default)]
#[allow(clippy::type_complexity)]
struct Handlers {
    new_producer: Bag<Arc<dyn Fn(&Producer) + Send + Sync>, Producer>,
    new_consumer: Bag<Arc<dyn Fn(&Consumer) + Send + Sync>, Consumer>,
    new_data_producer: Bag<Arc<dyn Fn(&DataProducer) + Send + Sync>, DataProducer>,
    new_data_consumer: Bag<Arc<dyn Fn(&DataConsumer) + Send + Sync>, DataConsumer>,
    ice_state_change: Bag<Arc<dyn Fn(IceState) + Send + Sync>>,
    ice_selected_tuple_change: Bag<Arc<dyn Fn(&TransportTuple) + Send + Sync>, TransportTuple>,
    dtls_state_change: Bag<Arc<dyn Fn(DtlsState) + Send + Sync>>,
    sctp_state_change: Bag<Arc<dyn Fn(SctpState) + Send + Sync>>,
    trace: Bag<Arc<dyn Fn(&TransportTraceEventData) + Send + Sync>, TransportTraceEventData>,
    router_close: BagOnce<Box<dyn FnOnce() + Send>>,
    webrtc_server_close: BagOnce<Box<dyn FnOnce() + Send>>,
    close: BagOnce<Box<dyn FnOnce() + Send>>,
}

#[derive(Debug, Deserialize)]
#[serde(tag = "event", rename_all = "lowercase", content = "data")]
enum Notification {
    #[serde(rename_all = "camelCase")]
    IceStateChange {
        ice_state: IceState,
    },
    #[serde(rename_all = "camelCase")]
    IceSelectedTupleChange {
        ice_selected_tuple: TransportTuple,
    },
    #[serde(rename_all = "camelCase")]
    DtlsStateChange {
        dtls_state: DtlsState,
        dtls_remote_cert: Option<String>,
    },
    #[serde(rename_all = "camelCase")]
    SctpStateChange {
        sctp_state: SctpState,
    },
    Trace(TransportTraceEventData),
}

impl Notification {
    pub(crate) fn from_fbs(
        notification: notification::NotificationRef<'_>,
    ) -> Result<Self, NotificationParseError> {
        match notification.event().unwrap() {
            notification::Event::WebrtctransportIceStateChange => {
                let Ok(Some(notification::BodyRef::WebRtcTransportIceStateChangeNotification(
                    body,
                ))) = notification.body()
                else {
                    panic!("Wrong message from worker: {notification:?}");
                };

                let ice_state = IceState::from_fbs(body.ice_state().unwrap());

                Ok(Notification::IceStateChange { ice_state })
            }
            notification::Event::WebrtctransportIceSelectedTupleChange => {
                let Ok(Some(
                    notification::BodyRef::WebRtcTransportIceSelectedTupleChangeNotification(body),
                )) = notification.body()
                else {
                    panic!("Wrong message from worker: {notification:?}");
                };

                let ice_selected_tuple_fbs =
                    transport::Tuple::try_from(body.tuple().unwrap()).unwrap();
                let ice_selected_tuple = TransportTuple::from_fbs(&ice_selected_tuple_fbs);

                Ok(Notification::IceSelectedTupleChange { ice_selected_tuple })
            }
            notification::Event::WebrtctransportDtlsStateChange => {
                let Ok(Some(notification::BodyRef::WebRtcTransportDtlsStateChangeNotification(
                    body,
                ))) = notification.body()
                else {
                    panic!("Wrong message from worker: {notification:?}");
                };

                let dtls_state = DtlsState::from_fbs(body.dtls_state().unwrap());

                Ok(Notification::DtlsStateChange {
                    dtls_state,
                    dtls_remote_cert: None,
                })
            }
            notification::Event::TransportSctpStateChange => {
                let Ok(Some(notification::BodyRef::TransportSctpStateChangeNotification(body))) =
                    notification.body()
                else {
                    panic!("Wrong message from worker: {notification:?}");
                };

                let sctp_state = SctpState::from_fbs(&body.sctp_state().unwrap());

                Ok(Notification::SctpStateChange { sctp_state })
            }
            notification::Event::TransportTrace => {
                let Ok(Some(notification::BodyRef::TransportTraceNotification(body))) =
                    notification.body()
                else {
                    panic!("Wrong message from worker: {notification:?}");
                };

                let trace_notification_fbs = transport::TraceNotification::try_from(body).unwrap();
                let trace_notification = TransportTraceEventData::from_fbs(trace_notification_fbs);

                Ok(Notification::Trace(trace_notification))
            }
            _ => Err(NotificationParseError::InvalidEvent),
        }
    }
}

struct Inner {
    id: TransportId,
    next_mid_for_consumers: AtomicUsize,
    used_sctp_stream_ids: Mutex<IntMap<u16, bool>>,
    cname_for_producers: Mutex<Option<String>>,
    executor: Arc<Executor<'static>>,
    channel: Channel,
    handlers: Arc<Handlers>,
    data: Arc<WebRtcTransportData>,
    app_data: AppData,
    // Make sure WebRTC server is not dropped until this transport is not dropped
    webrtc_server: Option<WebRtcServer>,
    // Make sure router is not dropped until this transport is not dropped
    router: Router,
    closed: AtomicBool,
    // Drop subscription to transport-specific notifications when transport itself is dropped
    _subscription_handler: Mutex<Option<SubscriptionHandler>>,
    _on_webrtc_server_close_handler: Mutex<Option<HandlerId>>,
    _on_router_close_handler: Mutex<HandlerId>,
}

impl Drop for Inner {
    fn drop(&mut self) {
        debug!("drop()");

        self.close(true);
    }
}

impl Inner {
    fn close(&self, close_request: bool) {
        if !self.closed.swap(true, Ordering::SeqCst) {
            debug!("close()");

            self.handlers.close.call_simple();

            if close_request {
                let channel = self.channel.clone();
                let router_id = self.router.id();
                let request = TransportCloseRequest {
                    transport_id: self.id,
                };

                self.executor
                    .spawn(async move {
                        if let Err(error) = channel.request(router_id, request).await {
                            error!("transport closing failed on drop: {}", error);
                        }
                    })
                    .detach();
            }
        }
    }
}

/// A [`WebRtcTransport`] represents a network path negotiated by both, a WebRTC endpoint and
/// mediasoup, via ICE and DTLS procedures. A [`WebRtcTransport`] may be used to receive media, to
/// send media or to both receive and send. There is no limitation in mediasoup. However, due to
/// their design, mediasoup-client and libmediasoupclient require separate [`WebRtcTransport`]s for
/// sending and receiving.
///
/// # Notes on usage
/// The [`WebRtcTransport`] implementation of mediasoup is
/// [ICE Lite](https://tools.ietf.org/html/rfc5245#section-2.7), meaning that it does not initiate
/// ICE connections but expects ICE Binding Requests from endpoints.
#[derive(Clone)]
#[must_use = "Transport will be closed on drop, make sure to keep it around for as long as needed"]
pub struct WebRtcTransport {
    inner: Arc<Inner>,
}

impl fmt::Debug for WebRtcTransport {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        f.debug_struct("WebRtcTransport")
            .field("id", &self.inner.id)
            .field("next_mid_for_consumers", &self.inner.next_mid_for_consumers)
            .field("used_sctp_stream_ids", &self.inner.used_sctp_stream_ids)
            .field("cname_for_producers", &self.inner.cname_for_producers)
            .field("router", &self.inner.router)
            .field("closed", &self.inner.closed)
            .finish()
    }
}

#[async_trait]
impl Transport for WebRtcTransport {
    fn id(&self) -> TransportId {
        self.inner.id
    }

    fn router(&self) -> &Router {
        &self.inner.router
    }

    fn app_data(&self) -> &AppData {
        &self.inner.app_data
    }

    fn closed(&self) -> bool {
        self.inner.closed.load(Ordering::SeqCst)
    }

    async fn produce(&self, producer_options: ProducerOptions) -> Result<Producer, ProduceError> {
        debug!("produce()");

        let producer = self
            .produce_impl(producer_options, TransportType::WebRtc)
            .await?;

        self.inner.handlers.new_producer.call_simple(&producer);

        Ok(producer)
    }

    async fn consume(&self, consumer_options: ConsumerOptions) -> Result<Consumer, ConsumeError> {
        debug!("consume()");

        let consumer = self
            .consume_impl(consumer_options, TransportType::WebRtc, false)
            .await?;

        self.inner.handlers.new_consumer.call_simple(&consumer);

        Ok(consumer)
    }

    async fn produce_data(
        &self,
        data_producer_options: DataProducerOptions,
    ) -> Result<DataProducer, ProduceDataError> {
        debug!("produce_data()");

        let data_producer = self
            .produce_data_impl(
                DataProducerType::Sctp,
                data_producer_options,
                TransportType::WebRtc,
            )
            .await?;

        self.inner
            .handlers
            .new_data_producer
            .call_simple(&data_producer);

        Ok(data_producer)
    }

    async fn consume_data(
        &self,
        data_consumer_options: DataConsumerOptions,
    ) -> Result<DataConsumer, ConsumeDataError> {
        debug!("consume_data()");

        let data_consumer = self
            .consume_data_impl(
                DataConsumerType::Sctp,
                data_consumer_options,
                TransportType::WebRtc,
            )
            .await?;

        self.inner
            .handlers
            .new_data_consumer
            .call_simple(&data_consumer);

        Ok(data_consumer)
    }

    async fn enable_trace_event(
        &self,
        types: Vec<TransportTraceEventType>,
    ) -> Result<(), RequestError> {
        debug!("enable_trace_event()");

        self.enable_trace_event_impl(types).await
    }

    fn on_new_producer(
        &self,
        callback: Arc<dyn Fn(&Producer) + Send + Sync + 'static>,
    ) -> HandlerId {
        self.inner.handlers.new_producer.add(callback)
    }

    fn on_new_consumer(
        &self,
        callback: Arc<dyn Fn(&Consumer) + Send + Sync + 'static>,
    ) -> HandlerId {
        self.inner.handlers.new_consumer.add(callback)
    }

    fn on_new_data_producer(
        &self,
        callback: Arc<dyn Fn(&DataProducer) + Send + Sync + 'static>,
    ) -> HandlerId {
        self.inner.handlers.new_data_producer.add(callback)
    }

    fn on_new_data_consumer(
        &self,
        callback: Arc<dyn Fn(&DataConsumer) + Send + Sync + 'static>,
    ) -> HandlerId {
        self.inner.handlers.new_data_consumer.add(callback)
    }

    fn on_trace(
        &self,
        callback: Arc<dyn Fn(&TransportTraceEventData) + Send + Sync + 'static>,
    ) -> HandlerId {
        self.inner.handlers.trace.add(callback)
    }

    fn on_router_close(&self, callback: Box<dyn FnOnce() + Send + 'static>) -> HandlerId {
        self.inner.handlers.router_close.add(callback)
    }

    fn on_close(&self, callback: Box<dyn FnOnce() + Send + 'static>) -> HandlerId {
        let handler_id = self.inner.handlers.close.add(Box::new(callback));
        if self.inner.closed.load(Ordering::Relaxed) {
            self.inner.handlers.close.call_simple();
        }
        handler_id
    }
}

#[async_trait]
impl TransportGeneric for WebRtcTransport {
    type Dump = WebRtcTransportDump;
    type Stat = WebRtcTransportStat;

    #[doc(hidden)]
    async fn dump(&self) -> Result<Self::Dump, RequestError> {
        debug!("dump()");

        if let response::Body::WebRtcTransportDumpResponse(data) = self.dump_impl().await? {
            Ok(WebRtcTransportDump::from_fbs(*data).expect("Error parsing dump response"))
        } else {
            panic!("Wrong message from worker");
        }
    }

    async fn get_stats(&self) -> Result<Vec<Self::Stat>, RequestError> {
        debug!("get_stats()");

        if let response::Body::WebRtcTransportGetStatsResponse(data) = self.get_stats_impl().await?
        {
            Ok(vec![
                WebRtcTransportStat::from_fbs(*data).expect("Error parsing dump response")
            ])
        } else {
            panic!("Wrong message from worker");
        }
    }
}

impl TransportImpl for WebRtcTransport {
    fn channel(&self) -> &Channel {
        &self.inner.channel
    }

    fn executor(&self) -> &Arc<Executor<'static>> {
        &self.inner.executor
    }

    fn next_mid_for_consumers(&self) -> &AtomicUsize {
        &self.inner.next_mid_for_consumers
    }

    fn used_sctp_stream_ids(&self) -> &Mutex<IntMap<u16, bool>> {
        &self.inner.used_sctp_stream_ids
    }

    fn cname_for_producers(&self) -> &Mutex<Option<String>> {
        &self.inner.cname_for_producers
    }
}

impl WebRtcTransport {
    #[allow(clippy::too_many_arguments)]
    pub(super) fn new(
        id: TransportId,
        executor: Arc<Executor<'static>>,
        channel: Channel,
        data: WebRtcTransportData,
        app_data: AppData,
        router: Router,
        webrtc_server: Option<WebRtcServer>,
    ) -> Self {
        debug!("new()");

        let handlers = Arc::<Handlers>::default();
        let data = Arc::new(data);

        let subscription_handler = {
            let handlers = Arc::clone(&handlers);
            let data = Arc::clone(&data);

            channel.subscribe_to_notifications(id.into(), move |notification| {
                match Notification::from_fbs(notification) {
                    Ok(notification) => match notification {
                        Notification::IceStateChange { ice_state } => {
                            *data.ice_state.lock() = ice_state;
                            handlers.ice_state_change.call(|callback| {
                                callback(ice_state);
                            });
                        }
                        Notification::IceSelectedTupleChange { ice_selected_tuple } => {
                            data.ice_selected_tuple
                                .lock()
                                .replace(ice_selected_tuple.clone());
                            handlers
                                .ice_selected_tuple_change
                                .call_simple(&ice_selected_tuple);
                        }
                        Notification::DtlsStateChange {
                            dtls_state,
                            dtls_remote_cert,
                        } => {
                            *data.dtls_state.lock() = dtls_state;

                            if let Some(dtls_remote_cert) = dtls_remote_cert {
                                data.dtls_remote_cert.lock().replace(dtls_remote_cert);
                            }

                            handlers.dtls_state_change.call(|callback| {
                                callback(dtls_state);
                            });
                        }
                        Notification::SctpStateChange { sctp_state } => {
                            data.sctp_state.lock().replace(sctp_state);

                            handlers.sctp_state_change.call(|callback| {
                                callback(sctp_state);
                            });
                        }
                        Notification::Trace(trace_event_data) => {
                            handlers.trace.call_simple(&trace_event_data);
                        }
                    },
                    Err(error) => {
                        error!("Failed to parse notification: {}", error);
                    }
                }
            })
        };

        let next_mid_for_consumers = AtomicUsize::default();
        let used_sctp_stream_ids = Mutex::new({
            let mut used_used_sctp_stream_ids = IntMap::default();
            if let Some(sctp_parameters) = &data.sctp_parameters {
                for i in 0..sctp_parameters.mis {
                    used_used_sctp_stream_ids.insert(i, false);
                }
            }
            used_used_sctp_stream_ids
        });
        let cname_for_producers = Mutex::new(None);
        let inner_weak = Arc::<Mutex<Option<Weak<Inner>>>>::default();
        let on_webrtc_server_close_handler = webrtc_server.as_ref().map(|webrtc_server| {
            webrtc_server.on_close({
                let inner_weak = Arc::clone(&inner_weak);

                move || {
                    let maybe_inner = inner_weak.lock().as_ref().and_then(Weak::upgrade);
                    if let Some(inner) = maybe_inner {
                        inner.handlers.webrtc_server_close.call_simple();
                        inner.close(true);
                    }
                }
            })
        });
        let on_router_close_handler = router.on_close({
            let inner_weak = Arc::clone(&inner_weak);

            move || {
                let maybe_inner = inner_weak.lock().as_ref().and_then(Weak::upgrade);
                if let Some(inner) = maybe_inner {
                    inner.handlers.router_close.call_simple();
                    inner.close(false);
                }
            }
        });
        let inner = Arc::new(Inner {
            id,
            next_mid_for_consumers,
            used_sctp_stream_ids,
            cname_for_producers,
            executor,
            channel,
            handlers,
            data,
            app_data,
            webrtc_server,
            router,
            closed: AtomicBool::new(false),
            _subscription_handler: Mutex::new(subscription_handler),
            _on_webrtc_server_close_handler: Mutex::new(on_webrtc_server_close_handler),
            _on_router_close_handler: Mutex::new(on_router_close_handler),
        });

        inner_weak.lock().replace(Arc::downgrade(&inner));

        let webrtc_transport = Self { inner };

        // Notify WebRTC server that new transport was created.
        if let Some(webrtc_server) = &webrtc_transport.inner.webrtc_server {
            webrtc_server.notify_new_webrtc_transport(&webrtc_transport);
        }

        webrtc_transport
    }

    /// Provide the [`WebRtcTransport`] with remote parameters.
    ///
    /// # Example
    /// ```rust
    /// use mediasoup::data_structures::{DtlsParameters, DtlsRole, DtlsFingerprint};
    /// use mediasoup::webrtc_transport::WebRtcTransportRemoteParameters;
    ///
    /// # async fn f(
    /// #     webrtc_transport: mediasoup::webrtc_transport::WebRtcTransport,
    /// # ) -> Result<(), Box<dyn std::error::Error>> {
    /// // Calling connect() on a PlainTransport created with comedia and rtcp_mux set.
    /// webrtc_transport
    ///     .connect(WebRtcTransportRemoteParameters {
    ///         dtls_parameters: DtlsParameters {
    ///             role: DtlsRole::Server,
    ///             fingerprints: vec![
    ///                 DtlsFingerprint::Sha256 {
    ///                     value: [
    ///                         0xE5, 0xF5, 0xCA, 0xA7, 0x2D, 0x93, 0xE6, 0x16, 0xAC, 0x21, 0x09,
    ///                         0x9F, 0x23, 0x51, 0x62, 0x8C, 0xD0, 0x66, 0xE9, 0x0C, 0x22, 0x54,
    ///                         0x2B, 0x82, 0x0C, 0xDF, 0xE0, 0xC5, 0x2C, 0x7E, 0xCD, 0x53,
    ///                     ],
    ///                 },
    ///             ],
    ///         },
    ///     })
    ///     .await?;
    /// # Ok(())
    /// # }
    /// ```
    pub async fn connect(
        &self,
        remote_parameters: WebRtcTransportRemoteParameters,
    ) -> Result<(), RequestError> {
        debug!("connect()");

        let response = self
            .inner
            .channel
            .request(
                self.id(),
                WebRtcTransportConnectRequest {
                    dtls_parameters: remote_parameters.dtls_parameters,
                },
            )
            .await?;

        self.inner.data.dtls_parameters.lock().role = response.dtls_local_role;

        Ok(())
    }

    /// WebRTC server used during creation of this transport.
    pub fn webrtc_server(&self) -> &Option<WebRtcServer> {
        &self.inner.webrtc_server
    }

    /// Set maximum incoming bitrate for media streams sent by the remote endpoint over this
    /// transport.
    pub async fn set_max_incoming_bitrate(&self, bitrate: u32) -> Result<(), RequestError> {
        debug!("set_max_incoming_bitrate() [bitrate:{}]", bitrate);

        self.set_max_incoming_bitrate_impl(bitrate).await
    }

    /// Set maximum outgoing bitrate for media streams sent by the remote endpoint over this
    /// transport.
    pub async fn set_max_outgoing_bitrate(&self, bitrate: u32) -> Result<(), RequestError> {
        debug!("set_max_outgoing_bitrate() [bitrate:{}]", bitrate);

        self.set_max_outgoing_bitrate_impl(bitrate).await
    }

    /// Set minimum outgoing bitrate for media streams sent by the remote endpoint over this
    /// transport.
    pub async fn set_min_outgoing_bitrate(&self, bitrate: u32) -> Result<(), RequestError> {
        debug!("set_min_outgoing_bitrate() [bitrate:{}]", bitrate);

        self.set_min_outgoing_bitrate_impl(bitrate).await
    }

    /// Local ICE role. Due to the mediasoup ICE Lite design, this is always `Controlled`.
    #[must_use]
    pub fn ice_role(&self) -> IceRole {
        self.inner.data.ice_role
    }

    /// Local ICE parameters.
    #[must_use]
    pub fn ice_parameters(&self) -> &IceParameters {
        &self.inner.data.ice_parameters
    }

    /// Local ICE candidates.
    #[must_use]
    pub fn ice_candidates(&self) -> &Vec<IceCandidate> {
        &self.inner.data.ice_candidates
    }

    /// Current ICE state.
    #[must_use]
    pub fn ice_state(&self) -> IceState {
        *self.inner.data.ice_state.lock()
    }

    /// The selected transport tuple if ICE is in `Connected` or `Completed` state. It is `None` if
    /// ICE is not established (no working candidate pair was found).
    #[must_use]
    pub fn ice_selected_tuple(&self) -> Option<TransportTuple> {
        self.inner.data.ice_selected_tuple.lock().clone()
    }

    /// Local DTLS parameters.
    #[must_use]
    pub fn dtls_parameters(&self) -> DtlsParameters {
        self.inner.data.dtls_parameters.lock().clone()
    }

    /// Current DTLS state.
    #[must_use]
    pub fn dtls_state(&self) -> DtlsState {
        *self.inner.data.dtls_state.lock()
    }

    /// The remote certificate in PEM format. It is `Some` once the DTLS state becomes `Connected`.
    ///
    /// # Notes on usage
    /// The application may want to inspect the remote certificate for authorization purposes by
    /// using some certificates utility.
    #[must_use]
    pub fn dtls_remote_cert(&self) -> Option<String> {
        self.inner.data.dtls_remote_cert.lock().clone()
    }

    /// Local SCTP parameters. Or `None` if SCTP is not enabled.
    #[must_use]
    pub fn sctp_parameters(&self) -> Option<SctpParameters> {
        self.inner.data.sctp_parameters
    }

    /// Current SCTP state. Or `None` if SCTP is not enabled.
    #[must_use]
    pub fn sctp_state(&self) -> Option<SctpState> {
        *self.inner.data.sctp_state.lock()
    }

    /// Restarts the ICE layer by generating new local ICE parameters that must be signaled to the
    /// remote endpoint.
    pub async fn restart_ice(&self) -> Result<IceParameters, RequestError> {
        debug!("restart_ice()");

        self.inner
            .channel
            .request(self.id(), TransportRestartIceRequest {})
            .await
    }

    /// Callback is called when the WebRTC server used during creation of this transport is closed
    /// for whatever reason.
    /// The transport itself is also closed. `on_transport_close` callbacks are also called on all
    /// its producers and consumers.
    pub fn on_webrtc_server_close(
        &self,
        callback: Box<dyn FnOnce() + Send + 'static>,
    ) -> HandlerId {
        self.inner.handlers.webrtc_server_close.add(callback)
    }

    /// Callback is called when the transport ICE state changes.
    pub fn on_ice_state_change<F: Fn(IceState) + Send + Sync + 'static>(
        &self,
        callback: F,
    ) -> HandlerId {
        self.inner.handlers.ice_state_change.add(Arc::new(callback))
    }

    /// Callback is called after ICE state becomes `Completed` and when the ICE selected tuple
    /// changes.
    pub fn on_ice_selected_tuple_change<F: Fn(&TransportTuple) + Send + Sync + 'static>(
        &self,
        callback: F,
    ) -> HandlerId {
        self.inner
            .handlers
            .ice_selected_tuple_change
            .add(Arc::new(callback))
    }

    /// Callback is called when the transport DTLS state changes.
    pub fn on_dtls_state_change<F: Fn(DtlsState) + Send + Sync + 'static>(
        &self,
        callback: F,
    ) -> HandlerId {
        self.inner
            .handlers
            .dtls_state_change
            .add(Arc::new(callback))
    }

    /// Callback is called when the transport SCTP state changes.
    pub fn on_sctp_state_change<F: Fn(SctpState) + Send + Sync + 'static>(
        &self,
        callback: F,
    ) -> HandlerId {
        self.inner
            .handlers
            .sctp_state_change
            .add(Arc::new(callback))
    }

    /// Downgrade `WebRtcTransport` to [`WeakWebRtcTransport`] instance.
    #[must_use]
    pub fn downgrade(&self) -> WeakWebRtcTransport {
        WeakWebRtcTransport {
            inner: Arc::downgrade(&self.inner),
        }
    }
}

/// [`WeakWebRtcTransport`] doesn't own [`WebRtcTransport`] instance on mediasoup-worker and will
/// not prevent one from being destroyed once last instance of regular [`WebRtcTransport`] is
/// dropped.
///
/// [`WeakWebRtcTransport`] vs [`WebRtcTransport`] is similar to [`Weak`] vs [`Arc`].
#[derive(Clone)]
pub struct WeakWebRtcTransport {
    inner: Weak<Inner>,
}

impl fmt::Debug for WeakWebRtcTransport {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        f.debug_struct("WeakWebRtcTransport").finish()
    }
}

impl WeakWebRtcTransport {
    /// Attempts to upgrade `WeakWebRtcTransport` to [`WebRtcTransport`] if last instance of one
    /// wasn't dropped yet.
    #[must_use]
    pub fn upgrade(&self) -> Option<WebRtcTransport> {
        let inner = self.inner.upgrade()?;

        Some(WebRtcTransport { inner })
    }
}
