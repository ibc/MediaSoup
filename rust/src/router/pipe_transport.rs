#[cfg(test)]
mod tests;

use crate::consumer::{Consumer, ConsumerId, ConsumerOptions};
use crate::data_consumer::{DataConsumer, DataConsumerId, DataConsumerOptions, DataConsumerType};
use crate::data_producer::{DataProducer, DataProducerId, DataProducerOptions, DataProducerType};
use crate::data_structures::{AppData, ListenInfo, SctpState, TransportTuple};
use crate::messages::{PipeTransportConnectRequest, PipeTransportData, TransportCloseRequest};
use crate::producer::{Producer, ProducerId, ProducerOptions};
use crate::router::transport::{TransportImpl, TransportType};
use crate::router::Router;
use crate::sctp_parameters::{NumSctpStreams, SctpParameters};
use crate::srtp_parameters::SrtpParameters;
use crate::transport::{
    ConsumeDataError, ConsumeError, ProduceDataError, ProduceError, RecvRtpHeaderExtensions,
    RtpListener, SctpListener, Transport, TransportGeneric, TransportId, TransportTraceEventData,
    TransportTraceEventType,
};
use crate::worker::{Channel, NotificationParseError, RequestError, SubscriptionHandler};
use async_executor::Executor;
use async_trait::async_trait;
use event_listener_primitives::{Bag, BagOnce, HandlerId};
use log::{debug, error};
use mediasoup_sys::fbs::{notification, pipe_transport, response, transport};
use nohash_hasher::IntMap;
use parking_lot::Mutex;
use serde::{Deserialize, Serialize};
use std::error::Error;
use std::fmt;
use std::net::IpAddr;
use std::sync::atomic::{AtomicBool, AtomicUsize, Ordering};
use std::sync::{Arc, Weak};

/// [`PipeTransport`] options.
#[derive(Debug, Clone)]
#[non_exhaustive]
pub struct PipeTransportOptions {
    /// Listening info.
    pub listen_info: ListenInfo,
    /// Create a SCTP association.
    /// Default false.
    pub enable_sctp: bool,
    /// SCTP streams number.
    pub num_sctp_streams: NumSctpStreams,
    /// Maximum allowed size for SCTP messages sent by DataProducers.
    /// Default 268_435_456.
    pub max_sctp_message_size: u32,
    /// Maximum SCTP send buffer used by DataConsumers.
    /// Default 268_435_456.
    pub sctp_send_buffer_size: u32,
    /// Enable RTX and NACK for RTP retransmission. Useful if both Routers are located in different
    /// hosts and there is packet lost in the link. For this to work, both PipeTransports must
    /// enable this setting.
    /// Default false.
    pub enable_rtx: bool,
    /// Enable SRTP. Useful to protect the RTP and RTCP traffic if both Routers are located in
    /// different hosts. For this to work, connect() must be called with remote SRTP parameters.
    /// Default false.
    pub enable_srtp: bool,
    /// Custom application data.
    pub app_data: AppData,
}

impl PipeTransportOptions {
    /// Create Pipe transport options with given listen IP.
    #[must_use]
    pub fn new(listen_info: ListenInfo) -> Self {
        Self {
            listen_info,
            enable_sctp: false,
            num_sctp_streams: NumSctpStreams::default(),
            max_sctp_message_size: 268_435_456,
            sctp_send_buffer_size: 268_435_456,
            enable_rtx: false,
            enable_srtp: false,
            app_data: AppData::default(),
        }
    }
}

#[derive(Debug, Clone, Eq, PartialEq, Deserialize, Serialize)]
#[serde(rename_all = "camelCase")]
#[doc(hidden)]
#[non_exhaustive]
pub struct PipeTransportDump {
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
    // PipeTransport specific.
    pub tuple: TransportTuple,
    pub rtx: bool,
    pub srtp_parameters: Option<SrtpParameters>,
}

impl PipeTransportDump {
    pub(crate) fn from_fbs(
        dump: pipe_transport::DumpResponse,
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
            // PipeTransport specific.
            tuple: TransportTuple::from_fbs(dump.tuple.as_ref()),
            rtx: dump.rtx,
            srtp_parameters: dump
                .srtp_parameters
                .map(|parameters| SrtpParameters::from_fbs(parameters.as_ref())),
        })
    }
}

/// RTC statistics of the pipe transport.
#[derive(Debug, Clone, PartialOrd, PartialEq, Deserialize, Serialize)]
#[serde(rename_all = "camelCase")]
#[non_exhaustive]
#[allow(missing_docs)]
pub struct PipeTransportStat {
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
    // PipeTransport specific.
    pub tuple: TransportTuple,
}

impl PipeTransportStat {
    pub(crate) fn from_fbs(
        stats: pipe_transport::GetStatsResponse,
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
            // PlainTransport specific.
            tuple: TransportTuple::from_fbs(stats.tuple.as_ref()),
        })
    }
}

/// Remote parameters for pipe transport.
#[derive(Debug, Clone, Deserialize, Serialize)]
#[serde(rename_all = "camelCase")]
pub struct PipeTransportRemoteParameters {
    /// Remote IPv4 or IPv6.
    pub ip: IpAddr,
    /// Remote port.
    pub port: u16,
    /// SRTP parameters used by the paired `PipeTransport` to encrypt its RTP and RTCP.
    pub srtp_parameters: Option<SrtpParameters>,
}

#[derive(Default)]
#[allow(clippy::type_complexity)]
struct Handlers {
    new_producer: Bag<Arc<dyn Fn(&Producer) + Send + Sync>, Producer>,
    new_consumer: Bag<Arc<dyn Fn(&Consumer) + Send + Sync>, Consumer>,
    new_data_producer: Bag<Arc<dyn Fn(&DataProducer) + Send + Sync>, DataProducer>,
    new_data_consumer: Bag<Arc<dyn Fn(&DataConsumer) + Send + Sync>, DataConsumer>,
    tuple: Bag<Arc<dyn Fn(&TransportTuple) + Send + Sync>, TransportTuple>,
    sctp_state_change: Bag<Arc<dyn Fn(SctpState) + Send + Sync>>,
    trace: Bag<Arc<dyn Fn(&TransportTraceEventData) + Send + Sync>, TransportTraceEventData>,
    router_close: BagOnce<Box<dyn FnOnce() + Send>>,
    close: BagOnce<Box<dyn FnOnce() + Send>>,
}

#[derive(Debug, Deserialize)]
#[serde(tag = "event", rename_all = "lowercase", content = "data")]
enum Notification {
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
    data: Arc<PipeTransportData>,
    app_data: AppData,
    // Make sure router is not dropped until this transport is not dropped
    router: Router,
    closed: AtomicBool,
    // Drop subscription to transport-specific notifications when transport itself is dropped
    _subscription_handler: Mutex<Option<SubscriptionHandler>>,
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

/// A pipe transport represents a network path through which RTP, RTCP (optionally secured with
/// SRTP) and SCTP (DataChannel) is transmitted. Pipe transports are intended to intercommunicate
/// two [`Router`] instances collocated on the same host or on separate hosts.
///
/// # Notes on usage
/// When calling [`PipeTransport::consume`], all RTP streams of the [`Producer`] are transmitted
/// verbatim (in contrast to what happens in [`WebRtcTransport`](crate::webrtc_transport::WebRtcTransport)
/// and [`PlainTransport`](crate::plain_transport::PlainTransport) in which a single and continuous
/// RTP stream is sent to the consuming endpoint).
#[derive(Clone)]
#[must_use = "Transport will be closed on drop, make sure to keep it around for as long as needed"]
pub struct PipeTransport {
    inner: Arc<Inner>,
}

impl fmt::Debug for PipeTransport {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        f.debug_struct("PipeTransport")
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
impl Transport for PipeTransport {
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
            .produce_impl(producer_options, TransportType::Pipe)
            .await?;

        self.inner.handlers.new_producer.call_simple(&producer);

        Ok(producer)
    }

    async fn consume(&self, consumer_options: ConsumerOptions) -> Result<Consumer, ConsumeError> {
        debug!("consume()");

        let consumer = self
            .consume_impl(consumer_options, TransportType::Pipe, self.inner.data.rtx)
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
                TransportType::Pipe,
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
                TransportType::Pipe,
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
impl TransportGeneric for PipeTransport {
    type Dump = PipeTransportDump;
    type Stat = PipeTransportStat;

    #[doc(hidden)]
    async fn dump(&self) -> Result<Self::Dump, RequestError> {
        debug!("dump()");

        if let response::Body::PipeTransportDumpResponse(data) = self.dump_impl().await? {
            Ok(PipeTransportDump::from_fbs(*data).expect("Error parsing dump response"))
        } else {
            panic!("Wrong message from worker");
        }
    }

    async fn get_stats(&self) -> Result<Vec<Self::Stat>, RequestError> {
        debug!("get_stats()");

        if let response::Body::PipeTransportGetStatsResponse(data) = self.get_stats_impl().await? {
            Ok(vec![
                PipeTransportStat::from_fbs(*data).expect("Error parsing dump response")
            ])
        } else {
            panic!("Wrong message from worker");
        }
    }
}

impl TransportImpl for PipeTransport {
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

impl PipeTransport {
    pub(super) fn new(
        id: TransportId,
        executor: Arc<Executor<'static>>,
        channel: Channel,
        data: PipeTransportData,
        app_data: AppData,
        router: Router,
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
            router,
            closed: AtomicBool::new(false),
            _subscription_handler: Mutex::new(subscription_handler),
            _on_router_close_handler: Mutex::new(on_router_close_handler),
        });

        inner_weak.lock().replace(Arc::downgrade(&inner));

        Self { inner }
    }

    /// Provide the [`PipeTransport`] with remote parameters.
    pub async fn connect(
        &self,
        remote_parameters: PipeTransportRemoteParameters,
    ) -> Result<(), RequestError> {
        debug!("connect()");

        let response = self
            .inner
            .channel
            .request(
                self.id(),
                PipeTransportConnectRequest {
                    ip: remote_parameters.ip,
                    port: remote_parameters.port,
                    srtp_parameters: remote_parameters.srtp_parameters,
                },
            )
            .await?;

        *self.inner.data.tuple.lock() = response.tuple;

        Ok(())
    }

    /// Set maximum incoming bitrate for media streams sent by the remote endpoint over this
    /// transport.
    pub async fn set_max_incoming_bitrate(&self, bitrate: u32) -> Result<(), RequestError> {
        debug!("set_max_incoming_bitrate() [bitrate:{}]", bitrate);

        self.set_max_incoming_bitrate_impl(bitrate).await
    }

    /// The transport tuple. It refers to both RTP and RTCP since pipe transports use RTCP-mux by
    /// design.
    ///
    /// # Notes on usage
    /// * Once the pipe transport is created, `transport.tuple()` will contain information about
    ///   its `local_address`, `local_port` and `protocol`.
    /// * Information about `remote_ip` and `remote_port` will be set after calling `connect()`
    ///   method.
    #[must_use]
    pub fn tuple(&self) -> TransportTuple {
        self.inner.data.tuple.lock().clone()
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

    /// Local SRTP parameters representing the crypto suite and key material used to encrypt sending
    /// RTP and SRTP. Those parameters must be given to the paired `PipeTransport` in the
    /// `connect()` method.
    #[must_use]
    pub fn srtp_parameters(&self) -> Option<SrtpParameters> {
        self.inner.data.srtp_parameters.lock().clone()
    }

    /// Callback is called after the remote RTP origin has been discovered. Only if `comedia` mode
    /// was set.
    pub fn on_tuple<F: Fn(&TransportTuple) + Send + Sync + 'static>(
        &self,
        callback: F,
    ) -> HandlerId {
        self.inner.handlers.tuple.add(Arc::new(callback))
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

    /// Downgrade `PipeTransport` to [`WeakPipeTransport`] instance.
    #[must_use]
    pub fn downgrade(&self) -> WeakPipeTransport {
        WeakPipeTransport {
            inner: Arc::downgrade(&self.inner),
        }
    }
}

/// [`WeakPipeTransport`] doesn't own pipe transport instance on mediasoup-worker and will not
/// prevent one from being destroyed once last instance of regular [`PipeTransport`] is dropped.
///
/// [`WeakPipeTransport`] vs [`PipeTransport`] is similar to [`Weak`] vs [`Arc`].
#[derive(Clone)]
pub struct WeakPipeTransport {
    inner: Weak<Inner>,
}

impl fmt::Debug for WeakPipeTransport {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        f.debug_struct("WeakPipeTransport").finish()
    }
}

impl WeakPipeTransport {
    /// Attempts to upgrade `WeakPipeTransport` to [`PipeTransport`] if last instance of one
    /// wasn't dropped yet.
    #[must_use]
    pub fn upgrade(&self) -> Option<PipeTransport> {
        let inner = self.inner.upgrade()?;

        Some(PipeTransport { inner })
    }
}
