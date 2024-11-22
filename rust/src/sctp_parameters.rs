//! Collection of SCTP-related data structures that are used to specify SCTP association parameters.

use mediasoup_sys::fbs::sctp_parameters;
use serde::{Deserialize, Serialize};

/// Number of SCTP streams.
///
/// Both OS and MIS are part of the SCTP INIT+ACK handshake. OS refers to the initial number of
/// outgoing SCTP streams that the server side transport creates (to be used by
/// [DataConsumer](crate::data_consumer::DataConsumer)s), while MIS refers to the maximum number of
/// incoming SCTP streams that the server side transport can receive (to be used by
/// [DataProducer](crate::data_producer::DataProducer)s). So, if the server side transport will just
/// be used to create data producers (but no data consumers), OS can be low (~1). However, if data
/// consumers are desired on the server side transport, OS must have a proper value and such a
/// proper value depends on whether the remote endpoint supports  `SCTP_ADD_STREAMS` extension or
/// not.
///
/// libwebrtc (Chrome, Safari, etc) does not enable `SCTP_ADD_STREAMS` so, if data consumers are
/// required,  OS should be 1024 (the maximum number of DataChannels that libwebrtc enables).
///
/// Firefox does enable `SCTP_ADD_STREAMS` so, if data consumers are required, OS can be lower (16
/// for instance). The mediasoup transport will allocate and announce more outgoing SCTP streams
/// when needed.
///
/// mediasoup-client provides specific per browser/version OS and MIS values via the
/// device.sctpCapabilities getter.
#[derive(Debug, Serialize, Copy, Clone)]
pub struct NumSctpStreams {
    /// Initially requested number of outgoing SCTP streams.
    #[serde(rename = "OS")]
    pub os: u16,
    /// Maximum number of incoming SCTP streams.
    #[serde(rename = "MIS")]
    pub mis: u16,
}

impl Default for NumSctpStreams {
    fn default() -> Self {
        Self {
            os: 1024,
            mis: 1024,
        }
    }
}

impl NumSctpStreams {
    pub(crate) fn to_fbs(self) -> sctp_parameters::NumSctpStreams {
        sctp_parameters::NumSctpStreams {
            os: self.os,
            mis: self.mis,
        }
    }
}

/// Parameters of the SCTP association.
#[derive(Debug, Copy, Clone, Ord, PartialOrd, Eq, PartialEq, Hash, Deserialize, Serialize)]
#[serde(rename_all = "camelCase")]
pub struct SctpParameters {
    /// Must always equal 5000.
    pub port: u16,
    /// Initially requested number of outgoing SCTP streams.
    #[serde(rename = "OS")]
    pub os: u16,
    /// Maximum number of incoming SCTP streams.
    #[serde(rename = "MIS")]
    pub mis: u16,
    /// Maximum allowed size for SCTP messages.
    pub max_message_size: u32,
}

impl SctpParameters {
    pub(crate) fn from_fbs(parameters: &sctp_parameters::SctpParameters) -> Self {
        Self {
            port: parameters.port,
            os: parameters.os,
            mis: parameters.mis,
            max_message_size: parameters.max_message_size,
        }
    }
}

/// SCTP stream parameters describe the reliability of a certain SCTP stream.
///
/// If ordered is true then `max_packet_life_time` and `max_retransmits` must be `false`.
/// If ordered if false, only one of `max_packet_life_time` or max_retransmits can be `true`.
#[derive(Debug, Copy, Clone, Ord, PartialOrd, Eq, PartialEq, Hash, Deserialize, Serialize)]
#[serde(rename_all = "camelCase")]
pub struct SctpStreamParameters {
    /// SCTP stream id.
    pub(crate) stream_id: u16,
    /// Whether data messages must be received in order. If `true` the messages will be sent
    /// reliably.
    /// Default true.
    pub(crate) ordered: bool,
    /// When `ordered` is `false` indicates the time (in milliseconds) after which a SCTP packet
    /// will stop being retransmitted.
    #[serde(skip_serializing_if = "Option::is_none")]
    pub(crate) max_packet_life_time: Option<u16>,
    /// When `ordered` is `false` indicates the maximum number of times a packet will be
    /// retransmitted.
    #[serde(skip_serializing_if = "Option::is_none")]
    pub(crate) max_retransmits: Option<u16>,
}

impl SctpStreamParameters {
    /// SCTP stream id.
    #[must_use]
    pub fn stream_id(&self) -> u16 {
        self.stream_id
    }

    /// Whether data messages must be received in order. If `true` the messages will be sent
    /// reliably.
    #[must_use]
    pub fn ordered(&self) -> bool {
        self.ordered
    }

    /// When `ordered` is `false` indicates the time (in milliseconds) after which a SCTP packet
    /// will stop being retransmitted.
    #[must_use]
    pub fn max_packet_life_time(&self) -> Option<u16> {
        self.max_packet_life_time
    }

    /// When `ordered` is `false` indicates the maximum number of times a packet will be
    /// retransmitted.
    #[must_use]
    pub fn max_retransmits(&self) -> Option<u16> {
        self.max_retransmits
    }
}

impl SctpStreamParameters {
    pub(crate) fn to_fbs(self) -> sctp_parameters::SctpStreamParameters {
        sctp_parameters::SctpStreamParameters {
            stream_id: self.stream_id,
            ordered: Some(self.ordered),
            max_packet_life_time: self.max_packet_life_time,
            max_retransmits: self.max_retransmits,
        }
    }

    pub(crate) fn from_fbs(stream_parameters: sctp_parameters::SctpStreamParameters) -> Self {
        Self {
            stream_id: stream_parameters.stream_id,
            ordered: stream_parameters.ordered.unwrap_or(false),
            max_packet_life_time: stream_parameters.max_packet_life_time,
            max_retransmits: stream_parameters.max_retransmits,
        }
    }
}

impl SctpStreamParameters {
    /// Messages will be sent reliably in order.
    #[must_use]
    pub fn new_ordered(stream_id: u16) -> Self {
        Self {
            stream_id,
            ordered: true,
            max_packet_life_time: None,
            max_retransmits: None,
        }
    }

    /// Messages will be sent unreliably with time (in milliseconds) after which a SCTP packet will
    /// stop being retransmitted.
    #[must_use]
    pub fn new_unordered_with_life_time(stream_id: u16, max_packet_life_time: u16) -> Self {
        Self {
            stream_id,
            ordered: false,
            max_packet_life_time: Some(max_packet_life_time),
            max_retransmits: None,
        }
    }

    /// Messages will be sent unreliably with a limited number of retransmission attempts.
    #[must_use]
    pub fn new_unordered_with_retransmits(stream_id: u16, max_retransmits: u16) -> Self {
        Self {
            stream_id,
            ordered: false,
            max_packet_life_time: None,
            max_retransmits: Some(max_retransmits),
        }
    }
}
