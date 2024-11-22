#[cfg(test)]
mod tests;

use crate::data_producer::{DataProducer, DataProducerId, WeakDataProducer};
use crate::data_structures::{AppData, WebRtcMessage};
use crate::messages::{
    DataConsumerAddSubchannelRequest, DataConsumerCloseRequest, DataConsumerDumpRequest,
    DataConsumerGetBufferedAmountRequest, DataConsumerGetStatsRequest, DataConsumerPauseRequest,
    DataConsumerRemoveSubchannelRequest, DataConsumerResumeRequest, DataConsumerSendRequest,
    DataConsumerSetBufferedAmountLowThresholdRequest, DataConsumerSetSubchannelsRequest,
};
use crate::sctp_parameters::SctpStreamParameters;
use crate::transport::Transport;
use crate::uuid_based_wrapper_type;
use crate::worker::{Channel, NotificationParseError, RequestError, SubscriptionHandler};
use async_executor::Executor;
use event_listener_primitives::{Bag, BagOnce, HandlerId};
use log::{debug, error};
use mediasoup_sys::fbs::{data_consumer, data_producer, notification, response};
use parking_lot::Mutex;
use serde::{Deserialize, Serialize};
use std::borrow::Cow;
use std::error::Error;
// TODO.
// use std::borrow::Cow;
use std::fmt;
use std::fmt::Debug;
use std::sync::atomic::{AtomicBool, Ordering};
use std::sync::{Arc, Weak};

uuid_based_wrapper_type!(
    /// [`DataConsumer`] identifier.
    DataConsumerId
);

/// [`DataConsumer`] options.
#[derive(Debug, Clone)]
#[non_exhaustive]
pub struct DataConsumerOptions {
    // The id of the [`DataProducer`](crate::data_producer::DataProducer) to consume.
    pub(super) data_producer_id: DataProducerId,
    /// Just if consuming over SCTP.
    /// Whether data messages must be received in order. If true the messages will be sent reliably.
    /// Defaults to the value in the DataProducer if it has type `Sctp` or to true if it has type
    /// `Direct`.
    pub(super) ordered: Option<bool>,
    /// Just if consuming over SCTP.
    /// When ordered is false indicates the time (in milliseconds) after which a SCTP packet will
    /// stop being retransmitted.
    /// Defaults to the value in the DataProducer if it has type `Sctp` or unset if it has type
    /// `Direct`.
    pub(super) max_packet_life_time: Option<u16>,
    /// Just if consuming over SCTP.
    /// When ordered is false indicates the maximum number of times a packet will be retransmitted.
    /// Defaults to the value in the [`DataProducer`](crate::data_producer::DataProducer) if it
    /// has type `Sctp` or unset if it has type `Direct`.
    pub(super) max_retransmits: Option<u16>,
    /// Whether the DataConsumer must start in paused mode. Default false.
    pub paused: bool,
    /// Subchannels this DataConsumer initially subscribes to.
    /// Only used in case this DataConsumer receives messages from a local DataProducer
    /// that specifies subchannel(s) when calling send().
    pub subchannels: Option<Vec<u16>>,
    /// Custom application data.
    pub app_data: AppData,
}

impl DataConsumerOptions {
    /// Inherits parameters of corresponding
    /// [`DataProducer`](crate::data_producer::DataProducer).
    #[must_use]
    pub fn new_sctp(data_producer_id: DataProducerId) -> Self {
        Self {
            data_producer_id,
            ordered: None,
            max_packet_life_time: None,
            max_retransmits: None,
            subchannels: None,
            paused: false,
            app_data: AppData::default(),
        }
    }

    /// For [`DirectTransport`](crate::direct_transport::DirectTransport).
    #[must_use]
    pub fn new_direct(data_producer_id: DataProducerId, subchannels: Option<Vec<u16>>) -> Self {
        Self {
            data_producer_id,
            ordered: Some(true),
            max_packet_life_time: None,
            max_retransmits: None,
            paused: false,
            subchannels,
            app_data: AppData::default(),
        }
    }

    /// Messages will be sent reliably in order.
    #[must_use]
    pub fn new_sctp_ordered(data_producer_id: DataProducerId) -> Self {
        Self {
            data_producer_id,
            ordered: None,
            max_packet_life_time: None,
            max_retransmits: None,
            paused: false,
            subchannels: None,
            app_data: AppData::default(),
        }
    }

    /// Messages will be sent unreliably with time (in milliseconds) after which a SCTP packet will
    /// stop being retransmitted.
    #[must_use]
    pub fn new_sctp_unordered_with_life_time(
        data_producer_id: DataProducerId,
        max_packet_life_time: u16,
    ) -> Self {
        Self {
            data_producer_id,
            ordered: None,
            max_packet_life_time: Some(max_packet_life_time),
            max_retransmits: None,
            paused: false,
            subchannels: None,
            app_data: AppData::default(),
        }
    }

    /// Messages will be sent unreliably with a limited number of retransmission attempts.
    #[must_use]
    pub fn new_sctp_unordered_with_retransmits(
        data_producer_id: DataProducerId,
        max_retransmits: u16,
    ) -> Self {
        Self {
            data_producer_id,
            ordered: None,
            max_packet_life_time: None,
            max_retransmits: Some(max_retransmits),
            paused: false,
            subchannels: None,
            app_data: AppData::default(),
        }
    }
}

#[derive(Debug, Clone, Eq, PartialEq, Deserialize, Serialize)]
#[serde(rename_all = "camelCase")]
#[doc(hidden)]
#[non_exhaustive]
pub struct DataConsumerDump {
    pub id: DataConsumerId,
    pub data_producer_id: DataProducerId,
    pub r#type: DataConsumerType,
    pub label: String,
    pub protocol: String,
    pub sctp_stream_parameters: Option<SctpStreamParameters>,
    pub buffered_amount_low_threshold: u32,
    pub paused: bool,
    pub subchannels: Vec<u16>,
    pub data_producer_paused: bool,
}

impl DataConsumerDump {
    pub(crate) fn from_fbs(
        dump: data_consumer::DumpResponse,
    ) -> Result<Self, Box<dyn Error + Send + Sync>> {
        Ok(Self {
            id: dump.id.parse()?,
            data_producer_id: dump.data_producer_id.parse()?,
            r#type: if dump.type_ == data_producer::Type::Sctp {
                DataConsumerType::Sctp
            } else {
                DataConsumerType::Direct
            },
            label: dump.label,
            protocol: dump.protocol,
            sctp_stream_parameters: dump
                .sctp_stream_parameters
                .map(|parameters| SctpStreamParameters::from_fbs(*parameters)),
            buffered_amount_low_threshold: dump.buffered_amount_low_threshold,
            paused: dump.paused,
            subchannels: dump.subchannels,
            data_producer_paused: dump.data_producer_paused,
        })
    }
}

/// RTC statistics of the data consumer.
#[derive(Debug, Clone, PartialOrd, Eq, PartialEq, Deserialize, Serialize)]
#[serde(rename_all = "camelCase")]
#[non_exhaustive]
#[allow(missing_docs)]
pub struct DataConsumerStat {
    // `type` field is present in worker, but ignored here
    pub timestamp: u64,
    pub label: String,
    pub protocol: String,
    pub messages_sent: u64,
    pub bytes_sent: u64,
    pub buffered_amount: u32,
}

impl DataConsumerStat {
    pub(crate) fn from_fbs(stats: &data_consumer::GetStatsResponse) -> Self {
        Self {
            timestamp: stats.timestamp,
            label: stats.label.to_string(),
            protocol: stats.protocol.to_string(),
            messages_sent: stats.messages_sent,
            bytes_sent: stats.bytes_sent,
            buffered_amount: stats.buffered_amount,
        }
    }
}

/// Data consumer type.
#[derive(Debug, Copy, Clone, Eq, PartialEq, Ord, PartialOrd, Hash, Deserialize, Serialize)]
#[serde(rename_all = "lowercase")]
pub enum DataConsumerType {
    /// The endpoint receives messages using the SCTP protocol.
    Sctp,
    /// Messages are received directly by the Rust process over a direct transport.
    Direct,
}

#[derive(Debug, Deserialize)]
#[serde(tag = "event", rename_all = "lowercase", content = "data")]
enum Notification {
    DataProducerClose,
    DataProducerPause,
    DataProducerResume,
    SctpSendBufferFull,
    Message {
        ppid: u32,
        data: Vec<u8>,
    },
    #[serde(rename_all = "camelCase")]
    BufferedAmountLow {
        buffered_amount: u32,
    },
}

impl Notification {
    pub(crate) fn from_fbs(
        notification: notification::NotificationRef<'_>,
    ) -> Result<Self, NotificationParseError> {
        match notification.event().unwrap() {
            notification::Event::DataconsumerDataproducerClose => {
                Ok(Notification::DataProducerClose)
            }
            notification::Event::DataconsumerDataproducerPause => {
                Ok(Notification::DataProducerPause)
            }
            notification::Event::DataconsumerDataproducerResume => {
                Ok(Notification::DataProducerResume)
            }
            notification::Event::DataconsumerSctpSendbufferFull => {
                Ok(Notification::SctpSendBufferFull)
            }
            notification::Event::DataconsumerMessage => {
                let Ok(Some(notification::BodyRef::DataConsumerMessageNotification(body))) =
                    notification.body()
                else {
                    panic!("Wrong message from worker: {notification:?}");
                };

                Ok(Notification::Message {
                    ppid: body.ppid().unwrap(),
                    data: body.data().unwrap().into(),
                })
            }
            notification::Event::DataconsumerBufferedAmountLow => {
                let Ok(Some(notification::BodyRef::DataConsumerBufferedAmountLowNotification(
                    body,
                ))) = notification.body()
                else {
                    panic!("Wrong message from worker: {notification:?}");
                };

                Ok(Notification::BufferedAmountLow {
                    buffered_amount: body.buffered_amount().unwrap(),
                })
            }
            _ => Err(NotificationParseError::InvalidEvent),
        }
    }
}

#[derive(Default)]
#[allow(clippy::type_complexity)]
struct Handlers {
    message: Bag<Arc<dyn Fn(&WebRtcMessage<'_>) + Send + Sync>>,
    sctp_send_buffer_full: Bag<Arc<dyn Fn() + Send + Sync>>,
    buffered_amount_low: Bag<Arc<dyn Fn(u32) + Send + Sync>>,
    data_producer_close: BagOnce<Box<dyn FnOnce() + Send>>,
    pause: Bag<Arc<dyn Fn() + Send + Sync>>,
    resume: Bag<Arc<dyn Fn() + Send + Sync>>,
    data_producer_pause: Bag<Arc<dyn Fn() + Send + Sync>>,
    data_producer_resume: Bag<Arc<dyn Fn() + Send + Sync>>,
    transport_close: BagOnce<Box<dyn FnOnce() + Send>>,
    close: BagOnce<Box<dyn FnOnce() + Send>>,
}

struct Inner {
    id: DataConsumerId,
    r#type: DataConsumerType,
    sctp_stream_parameters: Option<SctpStreamParameters>,
    label: String,
    protocol: String,
    data_producer_id: DataProducerId,
    direct: bool,
    paused: Arc<Mutex<bool>>,
    subchannels: Arc<Mutex<Vec<u16>>>,
    data_producer_paused: Arc<Mutex<bool>>,
    executor: Arc<Executor<'static>>,
    channel: Channel,
    handlers: Arc<Handlers>,
    app_data: AppData,
    transport: Arc<dyn Transport>,
    weak_data_producer: WeakDataProducer,
    closed: Arc<AtomicBool>,
    // Drop subscription to data consumer-specific notifications when data consumer itself is
    // dropped
    _subscription_handlers: Mutex<Vec<Option<SubscriptionHandler>>>,
    _on_transport_close_handler: Mutex<HandlerId>,
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
                let transport_id = self.transport.id();
                let request = DataConsumerCloseRequest {
                    data_consumer_id: self.id,
                };
                let weak_data_producer = self.weak_data_producer.clone();

                self.executor
                    .spawn(async move {
                        if weak_data_producer.upgrade().is_some() {
                            if let Err(error) = channel.request(transport_id, request).await {
                                error!("consumer closing failed on drop: {}", error);
                            }
                        }
                    })
                    .detach();
            }
        }
    }
}

/// Data consumer created on transport other than
/// [`DirectTransport`](crate::direct_transport::DirectTransport).
#[derive(Clone)]
#[must_use = "Data consumer will be closed on drop, make sure to keep it around for as long as needed"]
pub struct RegularDataConsumer {
    inner: Arc<Inner>,
}

impl fmt::Debug for RegularDataConsumer {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        f.debug_struct("RegularDataConsumer")
            .field("id", &self.inner.id)
            .field("type", &self.inner.r#type)
            .field("sctp_stream_parameters", &self.inner.sctp_stream_parameters)
            .field("label", &self.inner.label)
            .field("protocol", &self.inner.protocol)
            .field("data_producer_id", &self.inner.data_producer_id)
            .field("paused", &self.inner.paused)
            .field("data_producer_paused", &self.inner.data_producer_paused)
            .field("subchannels", &self.inner.subchannels)
            .field("transport", &self.inner.transport)
            .field("closed", &self.inner.closed)
            .finish()
    }
}

impl From<RegularDataConsumer> for DataConsumer {
    fn from(producer: RegularDataConsumer) -> Self {
        DataConsumer::Regular(producer)
    }
}

/// Data consumer created on [`DirectTransport`](crate::direct_transport::DirectTransport).
#[derive(Clone)]
#[must_use = "Data consumer will be closed on drop, make sure to keep it around for as long as needed"]
pub struct DirectDataConsumer {
    inner: Arc<Inner>,
}

impl fmt::Debug for DirectDataConsumer {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        f.debug_struct("DirectDataConsumer")
            .field("id", &self.inner.id)
            .field("type", &self.inner.r#type)
            .field("sctp_stream_parameters", &self.inner.sctp_stream_parameters)
            .field("label", &self.inner.label)
            .field("protocol", &self.inner.protocol)
            .field("data_producer_id", &self.inner.data_producer_id)
            .field("paused", &self.inner.paused)
            .field("data_producer_paused", &self.inner.data_producer_paused)
            .field("subchannels", &self.inner.subchannels)
            .field("transport", &self.inner.transport)
            .field("closed", &self.inner.closed)
            .finish()
    }
}

impl From<DirectDataConsumer> for DataConsumer {
    fn from(producer: DirectDataConsumer) -> Self {
        DataConsumer::Direct(producer)
    }
}

/// A data consumer represents an endpoint capable of receiving data messages from a mediasoup
/// [`Router`](crate::router::Router).
///
/// A data consumer can use [SCTP](https://tools.ietf.org/html/rfc4960) (AKA
/// DataChannel) to receive those messages, or can directly receive them in the Rust application if
/// the data consumer was created on top of a
/// [`DirectTransport`](crate::direct_transport::DirectTransport).
#[derive(Clone)]
#[non_exhaustive]
#[must_use = "Data consumer will be closed on drop, make sure to keep it around for as long as needed"]
pub enum DataConsumer {
    /// Data consumer created on transport other than
    /// [`DirectTransport`](crate::direct_transport::DirectTransport).
    Regular(RegularDataConsumer),
    /// Data consumer created on [`DirectTransport`](crate::direct_transport::DirectTransport).
    Direct(DirectDataConsumer),
}

impl fmt::Debug for DataConsumer {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        match &self {
            DataConsumer::Regular(producer) => f.debug_tuple("Regular").field(&producer).finish(),
            DataConsumer::Direct(producer) => f.debug_tuple("Direct").field(&producer).finish(),
        }
    }
}

impl DataConsumer {
    #[allow(clippy::too_many_arguments)]
    pub(super) fn new(
        id: DataConsumerId,
        r#type: DataConsumerType,
        sctp_stream_parameters: Option<SctpStreamParameters>,
        label: String,
        protocol: String,
        paused: bool,
        data_producer: DataProducer,
        executor: Arc<Executor<'static>>,
        channel: Channel,
        data_producer_paused: bool,
        subchannels: Vec<u16>,
        app_data: AppData,
        transport: Arc<dyn Transport>,
        direct: bool,
    ) -> Self {
        debug!("new()");

        let handlers = Arc::<Handlers>::default();
        let closed = Arc::new(AtomicBool::new(false));
        let paused = Arc::new(Mutex::new(paused));
        #[allow(clippy::mutex_atomic)]
        let data_producer_paused = Arc::new(Mutex::new(data_producer_paused));
        let subchannels = Arc::new(Mutex::new(subchannels));

        let inner_weak = Arc::<Mutex<Option<Weak<Inner>>>>::default();
        let subscription_handler = {
            let handlers = Arc::clone(&handlers);
            let closed = Arc::clone(&closed);
            let paused = Arc::clone(&paused);
            let data_producer_paused = Arc::clone(&data_producer_paused);
            let inner_weak = Arc::clone(&inner_weak);

            channel.subscribe_to_notifications(id.into(), move |notification| {
                match Notification::from_fbs(notification) {
                    Ok(notification) => match notification {
                        Notification::DataProducerClose => {
                            if !closed.load(Ordering::SeqCst) {
                                handlers.data_producer_close.call_simple();

                                let maybe_inner =
                                    inner_weak.lock().as_ref().and_then(Weak::upgrade);
                                if let Some(inner) = maybe_inner {
                                    inner
                                        .executor
                                        .clone()
                                        .spawn(async move {
                                            // Potential drop needs to happen from a different
                                            // thread to prevent potential deadlock
                                            inner.close(false);
                                        })
                                        .detach();
                                }
                            }
                        }
                        Notification::DataProducerPause => {
                            let mut data_producer_paused = data_producer_paused.lock();
                            let paused = *paused.lock();
                            *data_producer_paused = true;

                            handlers.data_producer_pause.call_simple();

                            if !paused {
                                handlers.pause.call_simple();
                            }
                        }
                        Notification::DataProducerResume => {
                            let mut data_producer_paused = data_producer_paused.lock();
                            let paused = *paused.lock();
                            *data_producer_paused = false;

                            handlers.data_producer_resume.call_simple();

                            if !paused {
                                handlers.resume.call_simple();
                            }
                        }
                        Notification::SctpSendBufferFull => {
                            handlers.sctp_send_buffer_full.call_simple();
                        }
                        Notification::Message { ppid, data } => {
                            match WebRtcMessage::new(ppid, Cow::from(data)) {
                                Ok(message) => {
                                    handlers.message.call(|callback| {
                                        callback(&message);
                                    });
                                }
                                Err(ppid) => {
                                    error!("Bad ppid {}", ppid);
                                }
                            }
                        }
                        Notification::BufferedAmountLow { buffered_amount } => {
                            handlers.buffered_amount_low.call(|callback| {
                                callback(buffered_amount);
                            });
                        }
                    },
                    Err(error) => {
                        error!("Failed to parse notification: {}", error);
                    }
                }
            })
        };

        let on_transport_close_handler = transport.on_close({
            let inner_weak = Arc::clone(&inner_weak);

            Box::new(move || {
                let maybe_inner = inner_weak.lock().as_ref().and_then(Weak::upgrade);
                if let Some(inner) = maybe_inner {
                    inner.handlers.transport_close.call_simple();
                    inner.close(false);
                }
            })
        });
        let inner = Arc::new(Inner {
            id,
            r#type,
            sctp_stream_parameters,
            label,
            protocol,
            data_producer_id: data_producer.id(),
            paused,
            data_producer_paused,
            direct,
            executor,
            channel,
            handlers,
            subchannels,
            app_data,
            transport,
            weak_data_producer: data_producer.downgrade(),
            closed,
            _subscription_handlers: Mutex::new(vec![subscription_handler]),
            _on_transport_close_handler: Mutex::new(on_transport_close_handler),
        });

        inner_weak.lock().replace(Arc::downgrade(&inner));

        if direct {
            Self::Direct(DirectDataConsumer { inner })
        } else {
            Self::Regular(RegularDataConsumer { inner })
        }
    }

    /// Data consumer identifier.
    #[must_use]
    pub fn id(&self) -> DataConsumerId {
        self.inner().id
    }

    /// The associated data producer identifier.
    #[must_use]
    pub fn data_producer_id(&self) -> DataProducerId {
        self.inner().data_producer_id
    }

    /// Transport to which data consumer belongs.
    pub fn transport(&self) -> &Arc<dyn Transport> {
        &self.inner().transport
    }

    /// The type of the data consumer.
    #[must_use]
    pub fn r#type(&self) -> DataConsumerType {
        self.inner().r#type
    }

    /// Whether the data consumer is paused. It does not take into account whether the
    /// associated data producer is paused.
    #[must_use]
    pub fn paused(&self) -> bool {
        *self.inner().paused.lock()
    }

    /// Whether the associate data producer is paused.
    #[must_use]
    pub fn producer_paused(&self) -> bool {
        *self.inner().data_producer_paused.lock()
    }

    /// The SCTP stream parameters (just if the data consumer type is `Sctp`).
    #[must_use]
    pub fn sctp_stream_parameters(&self) -> Option<SctpStreamParameters> {
        self.inner().sctp_stream_parameters
    }

    /// The data consumer label.
    #[must_use]
    pub fn label(&self) -> &String {
        &self.inner().label
    }

    /// The data consumer sub-protocol.
    #[must_use]
    pub fn protocol(&self) -> &String {
        &self.inner().protocol
    }

    /// The data consumer subchannels.
    #[must_use]
    pub fn subchannels(&self) -> Vec<u16> {
        self.inner().subchannels.lock().clone()
    }

    /// Custom application data.
    #[must_use]
    pub fn app_data(&self) -> &AppData {
        &self.inner().app_data
    }

    /// Whether the data consumer is closed.
    #[must_use]
    pub fn closed(&self) -> bool {
        self.inner().closed.load(Ordering::SeqCst)
    }

    /// Dump DataConsumer.
    #[doc(hidden)]
    pub async fn dump(&self) -> Result<DataConsumerDump, RequestError> {
        debug!("dump()");

        let response = self
            .inner()
            .channel
            .request(self.id(), DataConsumerDumpRequest {})
            .await?;

        if let response::Body::DataConsumerDumpResponse(data) = response {
            Ok(DataConsumerDump::from_fbs(*data).expect("Error parsing dump response"))
        } else {
            panic!("Wrong message from worker");
        }
    }

    /// Returns current statistics of the data consumer.
    ///
    /// Check the [RTC Statistics](https://mediasoup.org/documentation/v3/mediasoup/rtc-statistics/)
    /// section for more details (TypeScript-oriented, but concepts apply here as well).
    pub async fn get_stats(&self) -> Result<Vec<DataConsumerStat>, RequestError> {
        debug!("get_stats()");

        let response = self
            .inner()
            .channel
            .request(self.id(), DataConsumerGetStatsRequest {})
            .await?;

        if let response::Body::DataConsumerGetStatsResponse(data) = response {
            Ok(vec![DataConsumerStat::from_fbs(&data)])
        } else {
            panic!("Wrong message from worker");
        }
    }

    /// Pauses the data consumer (no mossage is sent to the consuming endpoint).
    pub async fn pause(&self) -> Result<(), RequestError> {
        debug!("pause()");

        self.inner()
            .channel
            .request(self.id(), DataConsumerPauseRequest {})
            .await?;

        let mut paused = self.inner().paused.lock();
        let was_paused = *paused || *self.inner().data_producer_paused.lock();
        *paused = true;

        if !was_paused {
            self.inner().handlers.pause.call_simple();
        }

        Ok(())
    }

    /// Resumes the data consumer (messages are sent again to the consuming endpoint).
    pub async fn resume(&self) -> Result<(), RequestError> {
        debug!("resume()");

        self.inner()
            .channel
            .request(self.id(), DataConsumerResumeRequest {})
            .await?;

        let mut paused = self.inner().paused.lock();
        let was_paused = *paused || *self.inner().data_producer_paused.lock();
        *paused = false;

        if was_paused {
            self.inner().handlers.resume.call_simple();
        }

        Ok(())
    }

    /// Returns the number of bytes of data currently buffered to be sent over the underlying SCTP
    /// association.
    ///
    /// # Notes on usage
    /// The underlying SCTP association uses a common send buffer for all data consumers, hence the
    /// value given by this method indicates the data buffered for all data consumers in the
    /// transport.
    pub async fn get_buffered_amount(&self) -> Result<u32, RequestError> {
        debug!("get_buffered_amount()");

        let response = self
            .inner()
            .channel
            .request(self.id(), DataConsumerGetBufferedAmountRequest {})
            .await?;

        Ok(response.buffered_amount)
    }

    /// Whenever the underlying SCTP association buffered bytes drop to this value,
    /// `on_buffered_amount_low` callback is called.
    pub async fn set_buffered_amount_low_threshold(
        &self,
        threshold: u32,
    ) -> Result<(), RequestError> {
        debug!(
            "set_buffered_amount_low_threshold() [threshold:{}]",
            threshold
        );

        self.inner()
            .channel
            .request(
                self.id(),
                DataConsumerSetBufferedAmountLowThresholdRequest { threshold },
            )
            .await
    }

    /// Sets subchannels to the worker DataConsumer.
    pub async fn set_subchannels(&self, subchannels: Vec<u16>) -> Result<(), RequestError> {
        let response = self
            .inner()
            .channel
            .request(self.id(), DataConsumerSetSubchannelsRequest { subchannels })
            .await?;

        *self.inner().subchannels.lock() = response.subchannels;

        Ok(())
    }

    /// Adds a subchannel to the worker DataConsumer.
    pub async fn add_subchannel(&self, subchannel: u16) -> Result<(), RequestError> {
        let response = self
            .inner()
            .channel
            .request(self.id(), DataConsumerAddSubchannelRequest { subchannel })
            .await?;

        *self.inner().subchannels.lock() = response.subchannels;

        Ok(())
    }

    /// Removes a subchannel to the worker DataConsumer.
    pub async fn remove_subchannel(&self, subchannel: u16) -> Result<(), RequestError> {
        let response = self
            .inner()
            .channel
            .request(
                self.id(),
                DataConsumerRemoveSubchannelRequest { subchannel },
            )
            .await?;

        *self.inner().subchannels.lock() = response.subchannels;

        Ok(())
    }

    /// Callback is called when a message has been received from the corresponding data producer.
    ///
    /// # Notes on usage
    /// Just available in direct transports, this is, those created via
    /// [`Router::create_direct_transport`](crate::router::Router::create_direct_transport).
    pub fn on_message<F: Fn(&WebRtcMessage<'_>) + Send + Sync + 'static>(
        &self,
        callback: F,
    ) -> HandlerId {
        self.inner().handlers.message.add(Arc::new(callback))
    }

    /// Callback is called when a message could not be sent because the SCTP send buffer was full.
    pub fn on_sctp_send_buffer_full<F: Fn() + Send + Sync + 'static>(
        &self,
        callback: F,
    ) -> HandlerId {
        self.inner()
            .handlers
            .sctp_send_buffer_full
            .add(Arc::new(callback))
    }

    /// Emitted when the underlying SCTP association buffered bytes drop down to the value set with
    /// [`DataConsumer::set_buffered_amount_low_threshold`].
    ///
    /// # Notes on usage
    /// Only applicable for consumers of type `Sctp`.
    pub fn on_buffered_amount_low<F: Fn(u32) + Send + Sync + 'static>(
        &self,
        callback: F,
    ) -> HandlerId {
        self.inner()
            .handlers
            .buffered_amount_low
            .add(Arc::new(callback))
    }

    /// Callback is called when the associated data producer is closed for whatever reason. The data
    /// consumer itself is also closed.
    pub fn on_data_producer_close<F: FnOnce() + Send + 'static>(&self, callback: F) -> HandlerId {
        self.inner()
            .handlers
            .data_producer_close
            .add(Box::new(callback))
    }

    /// Callback is called when the data consumer or its associated data producer is
    /// paused and, as result, the data consumer becomes paused.
    pub fn on_pause<F: Fn() + Send + Sync + 'static>(&self, callback: F) -> HandlerId {
        self.inner().handlers.pause.add(Arc::new(callback))
    }

    /// Callback is called when the data consumer or its associated data producer is
    /// resumed and, as result, the data consumer is no longer paused.
    pub fn on_resume<F: Fn() + Send + Sync + 'static>(&self, callback: F) -> HandlerId {
        self.inner().handlers.resume.add(Arc::new(callback))
    }

    /// Callback is called when the associated data producer is paused.
    pub fn on_data_producer_pause<F: Fn() + Send + Sync + 'static>(
        &self,
        callback: F,
    ) -> HandlerId {
        self.inner()
            .handlers
            .data_producer_pause
            .add(Arc::new(callback))
    }

    /// Callback is called when the associated data producer is resumed.
    pub fn on_data_producer_resume<F: Fn() + Send + Sync + 'static>(
        &self,
        callback: F,
    ) -> HandlerId {
        self.inner()
            .handlers
            .data_producer_resume
            .add(Arc::new(callback))
    }

    /// Callback is called when the transport this data consumer belongs to is closed for whatever
    /// reason. The data consumer itself is also closed.
    pub fn on_transport_close<F: FnOnce() + Send + 'static>(&self, callback: F) -> HandlerId {
        self.inner()
            .handlers
            .transport_close
            .add(Box::new(callback))
    }

    /// Callback is called when the data consumer is closed for whatever reason.
    ///
    /// NOTE: Callback will be called in place if data consumer is already closed.
    pub fn on_close<F: FnOnce() + Send + 'static>(&self, callback: F) -> HandlerId {
        let handler_id = self.inner().handlers.close.add(Box::new(callback));
        if self.inner().closed.load(Ordering::Relaxed) {
            self.inner().handlers.close.call_simple();
        }
        handler_id
    }

    /// Downgrade `DataConsumer` to [`WeakDataConsumer`] instance.
    #[must_use]
    pub fn downgrade(&self) -> WeakDataConsumer {
        WeakDataConsumer {
            inner: Arc::downgrade(self.inner()),
        }
    }

    fn inner(&self) -> &Arc<Inner> {
        match self {
            DataConsumer::Regular(data_consumer) => &data_consumer.inner,
            DataConsumer::Direct(data_consumer) => &data_consumer.inner,
        }
    }
}

impl DirectDataConsumer {
    /// Sends direct messages from the Rust process.
    pub async fn send(&self, message: WebRtcMessage<'_>) -> Result<(), RequestError> {
        let (ppid, payload) = message.into_ppid_and_payload();

        self.inner
            .channel
            .request(
                self.inner.id,
                DataConsumerSendRequest {
                    ppid,
                    payload: payload.into_owned(),
                },
            )
            .await
    }
}

/// [`WeakDataConsumer`] doesn't own data consumer instance on mediasoup-worker and will not prevent
/// one from being destroyed once last instance of regular [`DataConsumer`] is dropped.
///
/// [`WeakDataConsumer`] vs [`DataConsumer`] is similar to [`Weak`] vs [`Arc`].
#[derive(Clone)]
pub struct WeakDataConsumer {
    inner: Weak<Inner>,
}

impl fmt::Debug for WeakDataConsumer {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        f.debug_struct("WeakDataConsumer").finish()
    }
}

impl WeakDataConsumer {
    /// Attempts to upgrade `WeakDataConsumer` to [`DataConsumer`] if last instance of one wasn't
    /// dropped yet.
    #[must_use]
    pub fn upgrade(&self) -> Option<DataConsumer> {
        let inner = self.inner.upgrade()?;

        let data_consumer = if inner.direct {
            DataConsumer::Direct(DirectDataConsumer { inner })
        } else {
            DataConsumer::Regular(RegularDataConsumer { inner })
        };

        Some(data_consumer)
    }
}
