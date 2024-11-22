#[cfg(test)]
mod tests;

use crate::consumer::{Consumer, ConsumerId, ConsumerOptions};
use crate::data_consumer::{DataConsumer, DataConsumerId, DataConsumerOptions, DataConsumerType};
use crate::data_producer::{DataProducer, DataProducerId, DataProducerOptions, DataProducerType};
use crate::data_structures::{AppData, SctpState};
use crate::messages::{TransportCloseRequest, TransportSendRtcpNotification};
use crate::producer::{Producer, ProducerId, ProducerOptions};
use crate::router::transport::{TransportImpl, TransportType};
use crate::router::Router;
use crate::sctp_parameters::SctpParameters;
use crate::transport::{
    ConsumeDataError, ConsumeError, ProduceDataError, ProduceError, RecvRtpHeaderExtensions,
    RtpListener, SctpListener, Transport, TransportGeneric, TransportId, TransportTraceEventData,
    TransportTraceEventType,
};
use crate::worker::{
    Channel, NotificationError, NotificationParseError, RequestError, SubscriptionHandler,
};
use async_executor::Executor;
use async_trait::async_trait;
use event_listener_primitives::{Bag, BagOnce, HandlerId};
use log::{debug, error};
use mediasoup_sys::fbs::{direct_transport, notification, response, transport};
use nohash_hasher::IntMap;
use parking_lot::Mutex;
use serde::{Deserialize, Serialize};
use std::error::Error;
use std::fmt;
use std::sync::atomic::{AtomicBool, AtomicUsize, Ordering};
use std::sync::{Arc, Weak};

/// [`DirectTransport`] options.
#[derive(Debug, Clone)]
#[non_exhaustive]
pub struct DirectTransportOptions {
    /// Maximum allowed size for direct messages sent from DataProducers.
    /// Default 262_144.
    pub max_message_size: u32,
    /// Custom application data.
    pub app_data: AppData,
}

impl Default for DirectTransportOptions {
    fn default() -> Self {
        Self {
            max_message_size: 262_144,
            app_data: AppData::default(),
        }
    }
}

#[derive(Debug, Clone, Eq, PartialEq, Deserialize, Serialize)]
#[serde(rename_all = "camelCase")]
#[doc(hidden)]
#[non_exhaustive]
pub struct DirectTransportDump {
    // Common to all Transports.
    pub id: TransportId,
    pub direct: bool,
    pub producer_ids: Vec<ProducerId>,
    pub consumer_ids: Vec<ConsumerId>,
    pub map_ssrc_consumer_id: Vec<(u32, ConsumerId)>,
    pub map_rtx_ssrc_consumer_id: Vec<(u32, ConsumerId)>,
    pub data_producer_ids: Vec<DataProducerId>,
    pub data_consumer_ids: Vec<DataConsumerId>,
    pub recv_rtp_header_extensions: RecvRtpHeaderExtensions,
    pub rtp_listener: RtpListener,
    pub max_message_size: u32,
    pub sctp_parameters: Option<SctpParameters>,
    pub sctp_state: Option<SctpState>,
    pub sctp_listener: Option<SctpListener>,
    pub trace_event_types: Vec<TransportTraceEventType>,
}

impl DirectTransportDump {
    pub(crate) fn from_fbs(
        dump: direct_transport::DumpResponse,
    ) -> Result<Self, Box<dyn Error + Send + Sync>> {
        Ok(Self {
            id: dump.base.id.parse()?,
            direct: true,
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
        })
    }
}

/// RTC statistics of the direct transport.
#[derive(Debug, Clone, PartialEq, Deserialize, Serialize)]
#[serde(rename_all = "camelCase")]
#[non_exhaustive]
#[allow(missing_docs)]
pub struct DirectTransportStat {
    // Common to all Transports.
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
}

impl DirectTransportStat {
    pub(crate) fn from_fbs(
        stats: direct_transport::GetStatsResponse,
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
        })
    }
}

#[derive(Default)]
#[allow(clippy::type_complexity)]
struct Handlers {
    rtcp: Bag<Arc<dyn Fn(&[u8]) + Send + Sync>>,
    new_producer: Bag<Arc<dyn Fn(&Producer) + Send + Sync>, Producer>,
    new_consumer: Bag<Arc<dyn Fn(&Consumer) + Send + Sync>, Consumer>,
    new_data_producer: Bag<Arc<dyn Fn(&DataProducer) + Send + Sync>, DataProducer>,
    new_data_consumer: Bag<Arc<dyn Fn(&DataConsumer) + Send + Sync>, DataConsumer>,
    trace: Bag<Arc<dyn Fn(&TransportTraceEventData) + Send + Sync>, TransportTraceEventData>,
    router_close: BagOnce<Box<dyn FnOnce() + Send>>,
    close: BagOnce<Box<dyn FnOnce() + Send>>,
}

#[derive(Debug, Deserialize)]
#[serde(tag = "event", rename_all = "lowercase", content = "data")]
enum Notification {
    Trace(TransportTraceEventData),
    Rtcp(Vec<u8>),
}

impl Notification {
    pub(crate) fn from_fbs(
        notification: notification::NotificationRef<'_>,
    ) -> Result<Self, NotificationParseError> {
        match notification.event().unwrap() {
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
            notification::Event::DirecttransportRtcp => {
                let Ok(Some(notification::BodyRef::DirectTransportRtcpNotification(body))) =
                    notification.body()
                else {
                    panic!("Wrong message from worker: {notification:?}");
                };

                let rtcp_notification_fbs =
                    direct_transport::RtcpNotification::try_from(body).unwrap();

                Ok(Notification::Rtcp(rtcp_notification_fbs.data))
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
    app_data: AppData,
    // Make sure router is not dropped until this transport is not dropped
    router: Router,
    closed: AtomicBool,
    // Drop subscription to transport-specific notifications when transport itself is dropped
    _subscription_handlers: Mutex<Vec<Option<SubscriptionHandler>>>,
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

/// A direct transport represents a direct connection between the mediasoup Rust process and a
/// [`Router`] instance in a mediasoup-worker thread.
///
/// A direct transport can be used to directly send and receive data messages from/to Rust by means
/// of [`DataProducer`]s and [`DataConsumer`]s of type `Direct` created on a direct transport.
/// Direct messages sent by a [`DataProducer`] in a direct transport can be consumed by endpoints
/// connected through a SCTP capable transport ([`WebRtcTransport`](crate::webrtc_transport::WebRtcTransport),
/// [`PlainTransport`](crate::plain_transport::PlainTransport), [`PipeTransport`](crate::pipe_transport::PipeTransport)
/// and also by the Rust application by means of a [`DataConsumer`] created on a [`DirectTransport`]
/// (and vice-versa: messages sent over SCTP/DataChannel can be consumed by the Rust application by
/// means of a [`DataConsumer`] created on a [`DirectTransport`]).
///
/// A direct transport can also be used to inject and directly consume RTP and RTCP packets in Rust
/// by using the [`DirectProducer::send`](crate::producer::DirectProducer::send) and
/// [`Consumer::on_rtp`] API (plus [`DirectTransport::send_rtcp`] and [`DirectTransport::on_rtcp`]
/// API).
#[derive(Clone)]
#[must_use = "Transport will be closed on drop, make sure to keep it around for as long as needed"]
pub struct DirectTransport {
    inner: Arc<Inner>,
}

impl fmt::Debug for DirectTransport {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        f.debug_struct("DirectTransport")
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
impl Transport for DirectTransport {
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
            .produce_impl(producer_options, TransportType::Direct)
            .await?;

        self.inner.handlers.new_producer.call_simple(&producer);

        Ok(producer)
    }

    async fn consume(&self, consumer_options: ConsumerOptions) -> Result<Consumer, ConsumeError> {
        debug!("consume()");

        let consumer = self
            .consume_impl(consumer_options, TransportType::Direct, false)
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
                DataProducerType::Direct,
                data_producer_options,
                TransportType::Direct,
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
                DataConsumerType::Direct,
                data_consumer_options,
                TransportType::Direct,
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
impl TransportGeneric for DirectTransport {
    type Dump = DirectTransportDump;
    type Stat = DirectTransportStat;

    /// Dump Transport.
    #[doc(hidden)]
    async fn dump(&self) -> Result<Self::Dump, RequestError> {
        debug!("dump()");

        if let response::Body::DirectTransportDumpResponse(data) = self.dump_impl().await? {
            Ok(DirectTransportDump::from_fbs(*data).expect("Error parsing dump response"))
        } else {
            panic!("Wrong message from worker");
        }
    }

    /// Returns current RTC statistics of the transport.
    ///
    /// Check the [RTC Statistics](https://mediasoup.org/documentation/v3/mediasoup/rtc-statistics/)
    /// section for more details (TypeScript-oriented, but concepts apply here as well).
    async fn get_stats(&self) -> Result<Vec<Self::Stat>, RequestError> {
        debug!("get_stats()");

        if let response::Body::DirectTransportGetStatsResponse(data) = self.get_stats_impl().await?
        {
            Ok(vec![
                DirectTransportStat::from_fbs(*data).expect("Error parsing dump response")
            ])
        } else {
            panic!("Wrong message from worker");
        }
    }
}

impl TransportImpl for DirectTransport {
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

impl DirectTransport {
    pub(super) fn new(
        id: TransportId,
        executor: Arc<Executor<'static>>,
        channel: Channel,
        app_data: AppData,
        router: Router,
    ) -> Self {
        debug!("new()");

        let handlers = Arc::<Handlers>::default();

        let subscription_handler = {
            let handlers = Arc::clone(&handlers);

            channel.subscribe_to_notifications(id.into(), move |notification| {
                match Notification::from_fbs(notification) {
                    Ok(notification) => match notification {
                        Notification::Trace(trace_event_data) => {
                            handlers.trace.call_simple(&trace_event_data);
                        }
                        Notification::Rtcp(data) => {
                            handlers.rtcp.call(|callback| {
                                callback(&data);
                            });
                        }
                    },
                    Err(error) => {
                        error!("Failed to parse notification: {}", error);
                    }
                }
            })
        };

        let next_mid_for_consumers = AtomicUsize::default();
        let used_sctp_stream_ids = Mutex::new(IntMap::default());
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
            app_data,
            router,
            closed: AtomicBool::new(false),
            _subscription_handlers: Mutex::new(vec![subscription_handler]),
            _on_router_close_handler: Mutex::new(on_router_close_handler),
        });

        inner_weak.lock().replace(Arc::downgrade(&inner));

        Self { inner }
    }

    /// Send a RTCP packet from the Rust process.
    ///
    /// * `rtcp_packet` - Bytes containing a valid RTCP packet (can be a compound packet).
    pub fn send_rtcp(&self, rtcp_packet: Vec<u8>) -> Result<(), NotificationError> {
        self.inner
            .channel
            .notify(self.id(), TransportSendRtcpNotification { rtcp_packet })
    }

    /// Callback is called when the direct transport receives a RTCP packet from its router.
    pub fn on_rtcp<F: Fn(&[u8]) + Send + Sync + 'static>(&self, callback: F) -> HandlerId {
        self.inner.handlers.rtcp.add(Arc::new(callback))
    }

    /// Downgrade `DirectTransport` to [`WeakDirectTransport`] instance.
    #[must_use]
    pub fn downgrade(&self) -> WeakDirectTransport {
        WeakDirectTransport {
            inner: Arc::downgrade(&self.inner),
        }
    }
}

/// [`WeakDirectTransport`] doesn't own direct transport instance on mediasoup-worker and will not
/// prevent one from being destroyed once last instance of regular [`DirectTransport`] is dropped.
///
/// [`WeakDirectTransport`] vs [`DirectTransport`] is similar to [`Weak`] vs [`Arc`].
#[derive(Clone)]
pub struct WeakDirectTransport {
    inner: Weak<Inner>,
}

impl fmt::Debug for WeakDirectTransport {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        f.debug_struct("WeakDirectTransport").finish()
    }
}

impl WeakDirectTransport {
    /// Attempts to upgrade `WeakDirectTransport` to [`DirectTransport`] if last instance of one
    /// wasn't dropped yet.
    #[must_use]
    pub fn upgrade(&self) -> Option<DirectTransport> {
        let inner = self.inner.upgrade()?;

        Some(DirectTransport { inner })
    }
}
