use crate::consumer::{Consumer, ConsumerId, ConsumerOptions, ConsumerType};
use crate::data_consumer::{DataConsumer, DataConsumerId, DataConsumerOptions, DataConsumerType};
use crate::data_producer::{DataProducer, DataProducerId, DataProducerOptions, DataProducerType};
use crate::data_structures::{AppData, BweTraceInfo, RtpPacketTraceInfo, TraceEventDirection};
use crate::messages::{
    TransportConsumeDataRequest, TransportConsumeRequest, TransportDumpRequest,
    TransportEnableTraceEventRequest, TransportGetStatsRequest, TransportProduceDataRequest,
    TransportProduceRequest, TransportSetMaxIncomingBitrateRequest,
    TransportSetMaxOutgoingBitrateRequest, TransportSetMinOutgoingBitrateRequest,
};
pub use crate::ortc::{
    ConsumerRtpParametersError, RtpCapabilitiesError, RtpParametersError, RtpParametersMappingError,
};
use crate::producer::{Producer, ProducerId, ProducerOptions};
use crate::router::Router;
use crate::rtp_parameters::{MediaKind, RtpEncodingParameters};
use crate::sctp_parameters::SctpStreamParameters;
use crate::worker::{Channel, RequestError};
use crate::{ortc, uuid_based_wrapper_type};
use async_executor::Executor;
use async_trait::async_trait;
use event_listener_primitives::HandlerId;
use log::{error, warn};
use mediasoup_sys::fbs::{response, transport};
use nohash_hasher::IntMap;
use parking_lot::Mutex;
use serde::{Deserialize, Serialize};
use std::error::Error;
use std::fmt::Debug;
use std::sync::atomic::{AtomicUsize, Ordering};
use std::sync::Arc;
use thiserror::Error;
use uuid::Uuid;

uuid_based_wrapper_type!(
    /// Transport identifier.
    TransportId
);

/// Data contained in transport trace events.
///
/// See also "trace" event in the [Debugging](https://mediasoup.org/documentation/v3/mediasoup/debugging#trace-Event)
/// section (TypeScript-oriented, but concepts apply here as well).
#[derive(Debug, Clone, Deserialize, Serialize)]
#[serde(tag = "type", rename_all = "lowercase")]
pub enum TransportTraceEventData {
    /// RTP probation packet.
    Probation {
        /// Event timestamp.
        timestamp: u64,
        /// Event direction.
        direction: TraceEventDirection,
        /// RTP packet info.
        info: RtpPacketTraceInfo,
    },
    /// Transport bandwidth estimation changed.
    Bwe {
        /// Event timestamp.
        timestamp: u64,
        /// Event direction.
        direction: TraceEventDirection,
        /// BWE info.
        info: BweTraceInfo,
    },
}

impl TransportTraceEventData {
    pub(crate) fn from_fbs(data: transport::TraceNotification) -> Self {
        match data.type_ {
            transport::TraceEventType::Probation => unimplemented!(),
            transport::TraceEventType::Bwe => TransportTraceEventData::Bwe {
                timestamp: data.timestamp,
                direction: TraceEventDirection::from_fbs(data.direction),
                info: {
                    let Some(transport::TraceInfo::BweTraceInfo(info)) = data.info else {
                        panic!("Wrong message from worker: {data:?}");
                    };

                    BweTraceInfo::from_fbs(*info)
                },
            },
        }
    }
}

/// Valid types for "trace" event.
#[derive(Debug, Copy, Clone, Eq, PartialEq, Ord, PartialOrd, Hash, Deserialize, Serialize)]
#[serde(rename_all = "lowercase")]
pub enum TransportTraceEventType {
    /// RTP probation packet.
    Probation,
    /// Transport bandwidth estimation changed.
    Bwe,
}

impl TransportTraceEventType {
    pub(crate) fn to_fbs(self) -> transport::TraceEventType {
        match self {
            TransportTraceEventType::Probation => transport::TraceEventType::Probation,
            TransportTraceEventType::Bwe => transport::TraceEventType::Bwe,
        }
    }

    pub(crate) fn from_fbs(event_type: &transport::TraceEventType) -> Self {
        match event_type {
            transport::TraceEventType::Probation => TransportTraceEventType::Probation,
            transport::TraceEventType::Bwe => TransportTraceEventType::Bwe,
        }
    }
}

#[derive(Debug, Clone, Eq, PartialEq, Deserialize, Serialize)]
#[serde(rename_all = "camelCase")]
#[doc(hidden)]
pub struct RtpListener {
    /// Vector of mid and producer ID
    pub mid_table: Vec<(String, ProducerId)>,
    /// Vector of rid and producer ID
    pub rid_table: Vec<(String, ProducerId)>,
    /// Vector of Ssrc and producer ID
    pub ssrc_table: Vec<(u32, ProducerId)>,
}

impl RtpListener {
    pub(crate) fn from_fbs(
        rtp_listener: &transport::RtpListener,
    ) -> Result<Self, Box<dyn Error + Send + Sync>> {
        Ok(Self {
            mid_table: rtp_listener
                .mid_table
                .iter()
                .map(|key_value| Ok((key_value.key.to_string(), key_value.value.parse()?)))
                .collect::<Result<_, Box<dyn Error + Send + Sync>>>()?,
            rid_table: rtp_listener
                .rid_table
                .iter()
                .map(|key_value| Ok((key_value.key.to_string(), key_value.value.parse()?)))
                .collect::<Result<_, Box<dyn Error + Send + Sync>>>()?,
            ssrc_table: rtp_listener
                .ssrc_table
                .iter()
                .map(|key_value| Ok((key_value.key, key_value.value.parse()?)))
                .collect::<Result<_, Box<dyn Error + Send + Sync>>>()?,
        })
    }
}

#[derive(Debug, Copy, Clone, Ord, PartialOrd, Eq, PartialEq, Hash, Deserialize, Serialize)]
#[serde(rename_all = "camelCase")]
#[doc(hidden)]
pub struct RecvRtpHeaderExtensions {
    mid: Option<u8>,
    rid: Option<u8>,
    rrid: Option<u8>,
    abs_send_time: Option<u8>,
    transport_wide_cc01: Option<u8>,
}

impl RecvRtpHeaderExtensions {
    pub(crate) fn from_fbs(extensions: &transport::RecvRtpHeaderExtensions) -> Self {
        Self {
            mid: extensions.mid,
            rid: extensions.rid,
            rrid: extensions.rrid,
            abs_send_time: extensions.abs_send_time,
            transport_wide_cc01: extensions.transport_wide_cc01,
        }
    }
}

#[derive(Debug, Clone, Eq, PartialEq, Deserialize, Serialize)]
#[serde(rename_all = "camelCase")]
#[doc(hidden)]
pub struct SctpListener {
    /// Vector of stream ID (as string) to data producer ID
    stream_id_table: Vec<(u16, DataProducerId)>,
}

impl SctpListener {
    pub(crate) fn from_fbs(
        listener: &transport::SctpListener,
    ) -> Result<Self, Box<dyn Error + Send + Sync>> {
        Ok(Self {
            stream_id_table: listener
                .stream_id_table
                .iter()
                .map(|key_value| Ok((key_value.key, key_value.value.parse()?)))
                .collect::<Result<_, Box<dyn Error + Send + Sync>>>()?,
        })
    }
}

#[derive(Debug, Copy, Clone, PartialEq)]
pub(super) enum TransportType {
    Direct,
    Pipe,
    Plain,
    WebRtc,
}

/// A transport connects an endpoint with a mediasoup router and enables transmission of media in
/// both directions by means of [`Producer`], [`Consumer`], [`DataProducer`] and [`DataConsumer`]
/// instances created on it.
///
/// mediasoup implements the following transports:
/// * [`WebRtcTransport`](crate::webrtc_transport::WebRtcTransport)
/// * [`PlainTransport`](crate::plain_transport::PlainTransport)
/// * [`PipeTransport`](crate::pipe_transport::PipeTransport)
/// * [`DirectTransport`](crate::direct_transport::DirectTransport)
///
/// For additional methods see [`TransportGeneric`].
#[async_trait]
pub trait Transport: Debug + Send + Sync {
    /// Transport id.
    #[must_use]
    fn id(&self) -> TransportId;

    /// Router to which transport belongs.
    fn router(&self) -> &Router;

    /// Custom application data.
    #[must_use]
    fn app_data(&self) -> &AppData;

    /// Whether the transport is closed.
    #[must_use]
    fn closed(&self) -> bool;

    /// Instructs the router to receive audio or video RTP (or SRTP depending on the transport).
    /// This is the way to inject media into mediasoup.
    ///
    /// Transport will be kept alive as long as at least one producer instance is alive.
    ///
    /// # Notes on usage
    /// Check the [RTP Parameters and Capabilities](https://mediasoup.org/documentation/v3/mediasoup/rtp-parameters-and-capabilities/)
    /// section for more details (TypeScript-oriented, but concepts apply here as well).
    async fn produce(&self, producer_options: ProducerOptions) -> Result<Producer, ProduceError>;

    /// Instructs the router to send audio or video RTP (or SRTP depending on the transport).
    /// This is the way to extract media from mediasoup.
    ///
    /// Transport will be kept alive as long as at least one consumer instance is alive.
    ///
    /// # Notes on usage
    /// Check the [RTP Parameters and Capabilities](https://mediasoup.org/documentation/v3/mediasoup/rtp-parameters-and-capabilities/)
    /// section for more details (TypeScript-oriented, but concepts apply here as well).
    ///
    /// When creating a consumer it's recommended to set [`ConsumerOptions::paused`] to `true`, then
    /// transmit the consumer parameters to the consuming endpoint and, once the consuming endpoint
    /// has created its local side consumer, unpause the server side consumer using the
    /// [`Consumer::resume()`] method.
    ///
    /// Reasons for create the server side consumer in `paused` mode:
    /// * If the remote endpoint is a WebRTC browser or application and it receives a RTP packet of
    ///   the new consumer before the remote [`RTCPeerConnection`](https://developer.mozilla.org/en-US/docs/Web/API/RTCPeerConnection)
    ///   is ready to process it (this is, before the remote consumer is created in the remote
    ///   endpoint) it may happen that the `RTCPeerConnection` will wrongly associate the SSRC of
    ///   the received packet to an already existing SDP `m=` section, so the imminent creation of
    ///   the new consumer and its associated `m=` section will fail.
    ///   * Related [issue](https://github.com/versatica/libmediasoupclient/issues/57).
    /// * Also, when creating a video consumer, this is an optimization to make it possible for the
    ///   consuming endpoint to render the video as far as possible. If the server side consumer was
    ///   created with `paused: false`, mediasoup will immediately request a key frame to the
    ///   producer and that key frame may reach the consuming endpoint even before it's ready to
    ///   consume it, generating "black" video until the device requests a keyframe by itself.
    async fn consume(&self, consumer_options: ConsumerOptions) -> Result<Consumer, ConsumeError>;

    /// Instructs the router to receive data messages. Those messages can be delivered by an
    /// endpoint via SCTP protocol (AKA [`DataChannel`](https://developer.mozilla.org/en-US/docs/Web/API/RTCDataChannel)
    /// in WebRTC) or can be directly sent from the Rust application if the transport is a
    /// [`DirectTransport`](crate::direct_transport::DirectTransport).
    ///
    /// Transport will be kept alive as long as at least one data producer instance is alive.
    async fn produce_data(
        &self,
        data_producer_options: DataProducerOptions,
    ) -> Result<DataProducer, ProduceDataError>;

    /// Instructs the router to send data messages to the endpoint via SCTP protocol (AKA
    /// [`DataChannel`](https://developer.mozilla.org/en-US/docs/Web/API/RTCDataChannel) in WebRTC)
    /// or directly to the Rust process if the transport is a
    /// [`DirectTransport`](crate::direct_transport::DirectTransport).
    ///
    /// Transport will be kept alive as long as at least one data consumer instance is alive.
    async fn consume_data(
        &self,
        data_consumer_options: DataConsumerOptions,
    ) -> Result<DataConsumer, ConsumeDataError>;

    /// Instructs the transport to emit "trace" events. For monitoring purposes. Use with caution.
    async fn enable_trace_event(
        &self,
        types: Vec<TransportTraceEventType>,
    ) -> Result<(), RequestError>;

    /// Callback is called when a new producer is created.
    fn on_new_producer(
        &self,
        callback: Arc<dyn Fn(&Producer) + Send + Sync + 'static>,
    ) -> HandlerId;

    /// Callback is called when a new consumer is created.
    fn on_new_consumer(
        &self,
        callback: Arc<dyn Fn(&Consumer) + Send + Sync + 'static>,
    ) -> HandlerId;

    /// Callback is called when a new data producer is created.
    fn on_new_data_producer(
        &self,
        callback: Arc<dyn Fn(&DataProducer) + Send + Sync + 'static>,
    ) -> HandlerId;

    /// Callback is called when a new data consumer is created.
    fn on_new_data_consumer(
        &self,
        callback: Arc<dyn Fn(&DataConsumer) + Send + Sync + 'static>,
    ) -> HandlerId;

    /// See [`Transport::enable_trace_event()`]
    fn on_trace(
        &self,
        callback: Arc<dyn Fn(&TransportTraceEventData) + Send + Sync + 'static>,
    ) -> HandlerId;

    /// Callback is called when the router this transport belongs to is closed for whatever reason.
    /// The transport itself is also closed. `on_transport_close` callbacks are also called on all
    /// its producers and consumers.
    fn on_router_close(&self, callback: Box<dyn FnOnce() + Send + 'static>) -> HandlerId;

    /// Callback is called when the router is closed for whatever reason.
    ///
    /// NOTE: Callback will be called in place if transport is already closed.
    fn on_close(&self, callback: Box<dyn FnOnce() + Send + 'static>) -> HandlerId;
}

/// Generic transport trait with methods available on all transports in addition to [`Transport`].
#[async_trait]
pub trait TransportGeneric: Transport + Clone + 'static {
    /// Dump data structure specific to each transport.
    #[doc(hidden)]
    type Dump: Debug + 'static;
    /// Stats data structure specific to each transport.
    type Stat: Debug + 'static;

    /// Dump Transport.
    async fn dump(&self) -> Result<Self::Dump, RequestError>;

    /// Returns current RTC statistics of the transport. Each transport class produces a different
    /// set of statistics.
    async fn get_stats(&self) -> Result<Vec<Self::Stat>, RequestError>;
}

/// Error that caused [`Transport::produce`] to fail.
#[derive(Debug, Error)]
pub enum ProduceError {
    /// Producer with the same id already exists.
    #[error("Producer with the same id \"{0}\" already exists")]
    AlreadyExists(ProducerId),
    /// Incorrect RTP parameters.
    #[error("Incorrect RTP parameters: {0}")]
    IncorrectRtpParameters(RtpParametersError),
    /// RTP mapping error.
    #[error("RTP mapping error: {0}")]
    FailedRtpParametersMapping(RtpParametersMappingError),
    /// Request to worker failed.
    #[error("Request to worker failed: {0}")]
    Request(RequestError),
}

/// Error that caused [`Transport::consume`] to fail.
#[derive(Debug, Error)]
pub enum ConsumeError {
    /// Producer with specified id not found.
    #[error("Producer with id \"{0}\" not found")]
    ProducerNotFound(ProducerId),
    /// RTP capabilities error.
    #[error("RTP capabilities error: {0}")]
    FailedRtpCapabilitiesValidation(RtpCapabilitiesError),
    /// Bad consumer RTP parameters.
    #[error("Bad consumer RTP parameters: {0}")]
    BadConsumerRtpParameters(ConsumerRtpParametersError),
    /// Request to worker failed.
    #[error("Request to worker failed: {0}")]
    Request(RequestError),
}

/// Error that caused [`Transport::produce_data`] to fail.
#[derive(Debug, Error)]
pub enum ProduceDataError {
    /// Data producer with the same id already exists.
    #[error("Data producer with the same id \"{0}\" already exists")]
    AlreadyExists(DataProducerId),
    /// SCTP stream parameters are required for this transport.
    #[error("SCTP stream parameters are required for this transport")]
    SctpStreamParametersRequired,
    /// Request to worker failed.
    #[error("Request to worker failed: {0}")]
    Request(RequestError),
}

/// Error that caused [`Transport::consume_data`] to fail.
#[derive(Debug, Error)]
pub enum ConsumeDataError {
    /// Data producer with specified id not found
    #[error("Data producer with id \"{0}\" not found")]
    DataProducerNotFound(DataProducerId),
    /// No free `sctp_stream_id` available in transport.
    #[error("No free sctp_stream_id available in transport")]
    NoSctpStreamId,
    /// Request to worker failed.
    #[error("Request to worker failed: {0}")]
    Request(RequestError),
}

#[async_trait]
pub(super) trait TransportImpl: TransportGeneric {
    fn channel(&self) -> &Channel;

    fn executor(&self) -> &Arc<Executor<'static>>;

    fn next_mid_for_consumers(&self) -> &AtomicUsize;

    fn used_sctp_stream_ids(&self) -> &Mutex<IntMap<u16, bool>>;

    fn cname_for_producers(&self) -> &Mutex<Option<String>>;

    fn allocate_sctp_stream_id(&self) -> Option<u16> {
        let mut used_sctp_stream_ids = self.used_sctp_stream_ids().lock();
        // This is simple, but not the fastest implementation, maybe worth improving
        for (index, used) in used_sctp_stream_ids.iter_mut() {
            if !*used {
                *used = true;
                return Some(*index);
            }
        }

        None
    }

    fn deallocate_sctp_stream_id(&self, sctp_stream_id: u16) {
        let used_sctp_stream_ids = self.used_sctp_stream_ids();
        if let Some(used) = used_sctp_stream_ids.lock().get_mut(&sctp_stream_id) {
            *used = false;
        }
    }

    async fn dump_impl(&self) -> Result<response::Body, RequestError> {
        self.channel()
            .request(self.id(), TransportDumpRequest {})
            .await
    }

    async fn get_stats_impl(&self) -> Result<response::Body, RequestError> {
        self.channel()
            .request(self.id(), TransportGetStatsRequest {})
            .await
    }

    async fn set_max_incoming_bitrate_impl(&self, bitrate: u32) -> Result<(), RequestError> {
        self.channel()
            .request(self.id(), TransportSetMaxIncomingBitrateRequest { bitrate })
            .await
    }

    async fn set_max_outgoing_bitrate_impl(&self, bitrate: u32) -> Result<(), RequestError> {
        self.channel()
            .request(self.id(), TransportSetMaxOutgoingBitrateRequest { bitrate })
            .await
    }

    async fn enable_trace_event_impl(
        &self,
        types: Vec<TransportTraceEventType>,
    ) -> Result<(), RequestError> {
        self.channel()
            .request(self.id(), TransportEnableTraceEventRequest { types })
            .await
    }

    async fn set_min_outgoing_bitrate_impl(&self, bitrate: u32) -> Result<(), RequestError> {
        self.channel()
            .request(self.id(), TransportSetMinOutgoingBitrateRequest { bitrate })
            .await
    }

    async fn produce_impl(
        &self,
        producer_options: ProducerOptions,
        transport_type: TransportType,
    ) -> Result<Producer, ProduceError> {
        if let Some(id) = &producer_options.id {
            if self.router().has_producer(id) {
                return Err(ProduceError::AlreadyExists(*id));
            }
        }

        let ProducerOptions {
            id,
            kind,
            mut rtp_parameters,
            paused,
            key_frame_request_delay,
            app_data,
        } = producer_options;

        ortc::validate_rtp_parameters(&rtp_parameters)
            .map_err(ProduceError::IncorrectRtpParameters)?;

        if rtp_parameters.encodings.is_empty() {
            rtp_parameters
                .encodings
                .push(RtpEncodingParameters::default());
        }

        // Don't do this in PipeTransports since there we must keep CNAME value in each Producer.
        if transport_type != TransportType::Pipe {
            let mut cname_for_producers = self.cname_for_producers().lock();
            if let Some(cname_for_producers) = cname_for_producers.as_ref() {
                rtp_parameters.rtcp.cname = Some(cname_for_producers.clone());
            } else if let Some(cname) = rtp_parameters.rtcp.cname.as_ref() {
                // If CNAME is given and we don't have yet a CNAME for Producers in this
                // Transport, take it.
                cname_for_producers.replace(cname.clone());
            } else {
                // Otherwise if we don't have yet a CNAME for Producers and the RTP parameters
                // do not include CNAME, create a random one.
                let cname = Uuid::new_v4().to_string();
                cname_for_producers.replace(cname.clone());

                // Override Producer's CNAME.
                rtp_parameters.rtcp.cname = Some(cname);
            }
        }

        let router_rtp_capabilities = self.router().rtp_capabilities();

        let rtp_mapping =
            ortc::get_producer_rtp_parameters_mapping(&rtp_parameters, router_rtp_capabilities)
                .map_err(ProduceError::FailedRtpParametersMapping)?;

        let consumable_rtp_parameters = ortc::get_consumable_rtp_parameters(
            kind,
            &rtp_parameters,
            router_rtp_capabilities,
            &rtp_mapping,
        );

        let producer_id = id.unwrap_or_else(ProducerId::new);

        let _buffer_guard = self.channel().buffer_messages_for(producer_id.into());

        let response = self
            .channel()
            .request(
                self.id(),
                TransportProduceRequest {
                    producer_id,
                    kind,
                    rtp_parameters: rtp_parameters.clone(),
                    rtp_mapping,
                    key_frame_request_delay,
                    paused,
                },
            )
            .await
            .map_err(ProduceError::Request)?;

        let producer_fut = Producer::new(
            producer_id,
            kind,
            response.r#type,
            rtp_parameters,
            consumable_rtp_parameters,
            paused,
            Arc::clone(self.executor()),
            self.channel().clone(),
            app_data,
            Arc::new(self.clone()),
            transport_type == TransportType::Direct,
        );

        Ok(producer_fut.await)
    }

    async fn consume_impl(
        &self,
        consumer_options: ConsumerOptions,
        transport_type: TransportType,
        rtx: bool,
    ) -> Result<Consumer, ConsumeError> {
        let ConsumerOptions {
            producer_id,
            rtp_capabilities,
            paused,
            mid,
            preferred_layers,
            enable_rtx,
            ignore_dtx,
            pipe,
            app_data,
        } = consumer_options;
        ortc::validate_rtp_capabilities(&rtp_capabilities)
            .map_err(ConsumeError::FailedRtpCapabilitiesValidation)?;

        let producer = match self.router().get_producer(&producer_id) {
            Some(producer) => producer,
            None => {
                return Err(ConsumeError::ProducerNotFound(producer_id));
            }
        };

        let enable_rtx = enable_rtx.unwrap_or(producer.kind() == MediaKind::Video);

        let rtp_parameters = if transport_type == TransportType::Pipe {
            ortc::get_pipe_consumer_rtp_parameters(producer.consumable_rtp_parameters(), rtx)
        } else {
            let mut rtp_parameters = ortc::get_consumer_rtp_parameters(
                producer.consumable_rtp_parameters(),
                &rtp_capabilities,
                pipe,
                enable_rtx,
            )
            .map_err(ConsumeError::BadConsumerRtpParameters)?;

            if !pipe {
                // Set MID.
                rtp_parameters.mid = mid.or_else(|| {
                    // We use up to 8 bytes for MID (string).
                    let next_mid_for_consumers = self
                        .next_mid_for_consumers()
                        .fetch_add(1, Ordering::Relaxed);
                    let mid = next_mid_for_consumers % 100_000_000;
                    Some(format!("{mid}"))
                })
            }

            rtp_parameters
        };

        let consumer_id = ConsumerId::new();

        let r#type = if transport_type == TransportType::Pipe || pipe {
            ConsumerType::Pipe
        } else {
            producer.r#type().into()
        };

        let _buffer_guard = self.channel().buffer_messages_for(consumer_id.into());

        let response = self
            .channel()
            .request(
                self.id(),
                TransportConsumeRequest {
                    consumer_id,
                    producer_id: producer.id(),
                    kind: producer.kind(),
                    rtp_parameters: rtp_parameters.clone(),
                    r#type,
                    consumable_rtp_encodings: producer
                        .consumable_rtp_parameters()
                        .encodings
                        .clone(),
                    paused,
                    preferred_layers,
                    ignore_dtx,
                },
            )
            .await
            .map_err(ConsumeError::Request)?;

        Ok(Consumer::new(
            consumer_id,
            producer,
            r#type,
            rtp_parameters,
            response.paused,
            Arc::clone(self.executor()),
            self.channel().clone(),
            response.producer_paused,
            response.score,
            response.preferred_layers,
            app_data,
            Arc::new(self.clone()),
        ))
    }

    async fn produce_data_impl(
        &self,
        r#type: DataProducerType,
        data_producer_options: DataProducerOptions,
        transport_type: TransportType,
    ) -> Result<DataProducer, ProduceDataError> {
        if let Some(id) = &data_producer_options.id {
            if self.router().has_data_producer(id) {
                return Err(ProduceDataError::AlreadyExists(*id));
            }
        }

        match r#type {
            DataProducerType::Sctp => {
                if data_producer_options.sctp_stream_parameters.is_none() {
                    return Err(ProduceDataError::SctpStreamParametersRequired);
                }
            }
            DataProducerType::Direct => {
                if data_producer_options.sctp_stream_parameters.is_some() {
                    warn!(
                        "sctp_stream_parameters are ignored when producing data on a DirectTransport",
                    );
                }
            }
        }

        let DataProducerOptions {
            id,
            sctp_stream_parameters,
            label,
            protocol,
            paused,
            app_data,
        } = data_producer_options;

        let data_producer_id = id.unwrap_or_else(DataProducerId::new);

        let _buffer_guard = self.channel().buffer_messages_for(data_producer_id.into());

        let response = self
            .channel()
            .request(
                self.id(),
                TransportProduceDataRequest {
                    data_producer_id,
                    r#type,
                    sctp_stream_parameters,
                    label,
                    protocol,
                    paused,
                },
            )
            .await
            .map_err(ProduceDataError::Request)?;

        Ok(DataProducer::new(
            data_producer_id,
            response.r#type,
            response.sctp_stream_parameters,
            response.label,
            response.protocol,
            response.paused,
            Arc::clone(self.executor()),
            self.channel().clone(),
            app_data,
            Arc::new(self.clone()),
            transport_type == TransportType::Direct,
        ))
    }

    async fn consume_data_impl(
        &self,
        r#type: DataConsumerType,
        data_consumer_options: DataConsumerOptions,
        transport_type: TransportType,
    ) -> Result<DataConsumer, ConsumeDataError> {
        let DataConsumerOptions {
            data_producer_id,
            ordered,
            max_packet_life_time,
            max_retransmits,
            paused,
            subchannels,
            app_data,
        } = data_consumer_options;

        let data_producer = match self.router().get_data_producer(&data_producer_id) {
            Some(data_producer) => data_producer,
            None => {
                return Err(ConsumeDataError::DataProducerNotFound(data_producer_id));
            }
        };

        let sctp_stream_parameters = match r#type {
            DataConsumerType::Sctp => {
                let stream_id = self
                    .allocate_sctp_stream_id()
                    .ok_or(ConsumeDataError::NoSctpStreamId)?;
                let mut sctp_stream_parameters = data_producer.sctp_stream_parameters().map_or(
                    SctpStreamParameters {
                        stream_id,
                        ordered: true,
                        max_packet_life_time,
                        max_retransmits,
                    },
                    |mut sctp_parameters| {
                        sctp_parameters.stream_id = stream_id;

                        sctp_parameters
                    },
                );
                if let Some(ordered) = ordered {
                    sctp_stream_parameters.ordered = ordered;
                }
                if let Some(max_packet_life_time) = max_packet_life_time {
                    sctp_stream_parameters.max_packet_life_time = Some(max_packet_life_time);
                }
                if let Some(max_retransmits) = max_retransmits {
                    sctp_stream_parameters.max_retransmits = Some(max_retransmits);
                }

                Some(sctp_stream_parameters)
            }
            DataConsumerType::Direct => {
                if ordered.is_some() || max_packet_life_time.is_some() || max_retransmits.is_some()
                {
                    warn!("ordered, max_packet_life_time and max_retransmits are ignored when consuming data on a DirectTransport");
                }
                None
            }
        };

        let data_consumer_id = DataConsumerId::new();

        let _buffer_guard = self.channel().buffer_messages_for(data_consumer_id.into());

        let response = self
            .channel()
            .request(
                self.id(),
                TransportConsumeDataRequest {
                    data_consumer_id,
                    data_producer_id: data_producer.id(),
                    r#type,
                    sctp_stream_parameters,
                    label: data_producer.label().clone(),
                    protocol: data_producer.protocol().clone(),
                    subchannels,
                    paused,
                },
            )
            .await
            .map_err(ConsumeDataError::Request)?;

        let data_consumer = DataConsumer::new(
            data_consumer_id,
            response.r#type,
            response.sctp_stream_parameters,
            response.label,
            response.protocol,
            response.paused,
            data_producer,
            Arc::clone(self.executor()),
            self.channel().clone(),
            response.data_producer_paused,
            response.subchannels,
            app_data,
            Arc::new(self.clone()),
            transport_type == TransportType::Direct,
        );

        if let Some(sctp_stream_parameters) = data_consumer.sctp_stream_parameters() {
            let stream_id = sctp_stream_parameters.stream_id;
            let transport = self.clone();
            data_consumer
                .on_close(move || {
                    transport.deallocate_sctp_stream_id(stream_id);
                })
                .detach();
        }

        Ok(data_consumer)
    }
}
