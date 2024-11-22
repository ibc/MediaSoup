//! mediasoup prelude.
//!
//! Re-exports commonly used traits and structs from this crate.
//!
//! # Examples
//!
//! Import the prelude with:
//!
//! ```
//! # #[allow(unused_imports)]
//! use mediasoup::prelude::*;
//! ```
pub use crate::worker_manager::WorkerManager;

pub use crate::worker::{Worker, WorkerSettings};

pub use crate::router::{
    PipeDataProducerToRouterError, PipeDataProducerToRouterPair, PipeProducerToRouterError,
    PipeProducerToRouterPair, PipeToRouterOptions, Router, RouterOptions,
};

pub use crate::webrtc_server::{
    WebRtcServer, WebRtcServerId, WebRtcServerListenInfos, WebRtcServerOptions,
};

pub use crate::direct_transport::{DirectTransport, DirectTransportOptions, WeakDirectTransport};
pub use crate::pipe_transport::{
    PipeTransport, PipeTransportOptions, PipeTransportRemoteParameters, WeakPipeTransport,
};
pub use crate::plain_transport::{
    PlainTransport, PlainTransportOptions, PlainTransportRemoteParameters, WeakPlainTransport,
};
pub use crate::transport::{
    ConsumeDataError, ConsumeError, ProduceDataError, ProduceError, Transport, TransportGeneric,
    TransportId,
};
pub use crate::webrtc_transport::{
    WebRtcTransport, WebRtcTransportListenInfos, WebRtcTransportOptions,
    WebRtcTransportRemoteParameters,
};

pub use crate::active_speaker_observer::{
    ActiveSpeakerObserver, ActiveSpeakerObserverDominantSpeaker, ActiveSpeakerObserverOptions,
    WeakActiveSpeakerObserver,
};
pub use crate::audio_level_observer::{
    AudioLevelObserver, AudioLevelObserverOptions, AudioLevelObserverVolume, WeakAudioLevelObserver,
};
pub use crate::rtp_observer::{RtpObserver, RtpObserverAddProducerOptions, RtpObserverId};

pub use crate::consumer::{Consumer, ConsumerId, ConsumerLayers, ConsumerOptions, WeakConsumer};
pub use crate::data_consumer::{
    DataConsumer, DataConsumerId, DataConsumerOptions, DirectDataConsumer, RegularDataConsumer,
    WeakDataConsumer,
};
pub use crate::data_producer::{
    DataProducer, DataProducerId, DataProducerOptions, DirectDataProducer, NonClosingDataProducer,
    RegularDataProducer, WeakDataProducer,
};
pub use crate::producer::{Producer, ProducerId, ProducerOptions, WeakProducer};

pub use crate::data_structures::{
    AppData, DtlsParameters, IceCandidate, IceParameters, ListenInfo, Protocol, WebRtcMessage,
};
pub use crate::rtp_parameters::{
    MediaKind, MimeTypeAudio, MimeTypeVideo, RtcpFeedback, RtcpParameters, RtpCapabilities,
    RtpCapabilitiesFinalized, RtpCodecCapability, RtpCodecParameters, RtpCodecParametersParameters,
    RtpEncodingParameters, RtpEncodingParametersRtx, RtpHeaderExtensionParameters,
    RtpHeaderExtensionUri, RtpParameters,
};
pub use crate::sctp_parameters::SctpStreamParameters;
pub use crate::srtp_parameters::SrtpCryptoSuite;
