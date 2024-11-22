//! RTP capabilities supported by mediasoup.

use crate::rtp_parameters::{
    MediaKind, MimeTypeAudio, MimeTypeVideo, RtcpFeedback, RtpCapabilities, RtpCodecCapability,
    RtpCodecParametersParameters, RtpHeaderExtension, RtpHeaderExtensionDirection,
    RtpHeaderExtensionUri,
};
use std::num::{NonZeroU32, NonZeroU8};

/// Get a mediasoup supported RTP capabilities.
///
/// # Notes on usage
/// Those are NOT the RTP capabilities needed by mediasoup-client's
/// [device.load()](https://mediasoup.org/documentation/v3/mediasoup-client/api/#device-load) and
/// libmediasoupclient's
/// [device.Load()](https://mediasoup.org/documentation/v3/libmediasoupclient/api/#device-Load)
/// methods. There you must use
/// [`Router::rtp_capabilities`](crate::router::Router::rtp_capabilities) getter instead.
#[must_use]
pub fn get_supported_rtp_capabilities() -> RtpCapabilities {
    RtpCapabilities {
        codecs: vec![
            RtpCodecCapability::Audio {
                mime_type: MimeTypeAudio::Opus,
                preferred_payload_type: None,
                clock_rate: NonZeroU32::new(48000).unwrap(),
                channels: NonZeroU8::new(2).unwrap(),
                parameters: RtpCodecParametersParameters::default(),
                rtcp_feedback: vec![RtcpFeedback::Nack, RtcpFeedback::TransportCc],
            },
            RtpCodecCapability::Audio {
                mime_type: MimeTypeAudio::MultiChannelOpus,
                preferred_payload_type: None,
                clock_rate: NonZeroU32::new(48000).unwrap(),
                channels: NonZeroU8::new(4).unwrap(),
                // Quad channel
                parameters: RtpCodecParametersParameters::from([
                    ("channel_mapping", "0,1,2,3".into()),
                    ("num_streams", 2_u32.into()),
                    ("coupled_streams", 2_u32.into()),
                ]),
                rtcp_feedback: vec![RtcpFeedback::Nack, RtcpFeedback::TransportCc],
            },
            RtpCodecCapability::Audio {
                mime_type: MimeTypeAudio::MultiChannelOpus,
                preferred_payload_type: None,
                clock_rate: NonZeroU32::new(48000).unwrap(),
                channels: NonZeroU8::new(6).unwrap(),
                // 5.1
                parameters: RtpCodecParametersParameters::from([
                    ("channel_mapping", "0,4,1,2,3,5".into()),
                    ("num_streams", 4_u32.into()),
                    ("coupled_streams", 2_u32.into()),
                ]),
                rtcp_feedback: vec![RtcpFeedback::Nack, RtcpFeedback::TransportCc],
            },
            RtpCodecCapability::Audio {
                mime_type: MimeTypeAudio::MultiChannelOpus,
                preferred_payload_type: None,
                clock_rate: NonZeroU32::new(48000).unwrap(),
                channels: NonZeroU8::new(8).unwrap(),
                // 7.1
                parameters: RtpCodecParametersParameters::from([
                    ("channel_mapping", "0,6,1,2,3,4,5,7".into()),
                    ("num_streams", 5_u32.into()),
                    ("coupled_streams", 3_u32.into()),
                ]),
                rtcp_feedback: vec![RtcpFeedback::Nack, RtcpFeedback::TransportCc],
            },
            RtpCodecCapability::Audio {
                mime_type: MimeTypeAudio::Pcmu,
                preferred_payload_type: Some(0),
                clock_rate: NonZeroU32::new(8000).unwrap(),
                channels: NonZeroU8::new(1).unwrap(),
                parameters: RtpCodecParametersParameters::default(),
                rtcp_feedback: vec![RtcpFeedback::TransportCc],
            },
            RtpCodecCapability::Audio {
                mime_type: MimeTypeAudio::Pcma,
                preferred_payload_type: Some(8),
                clock_rate: NonZeroU32::new(8000).unwrap(),
                channels: NonZeroU8::new(1).unwrap(),
                parameters: RtpCodecParametersParameters::default(),
                rtcp_feedback: vec![RtcpFeedback::TransportCc],
            },
            RtpCodecCapability::Audio {
                mime_type: MimeTypeAudio::Isac,
                preferred_payload_type: None,
                clock_rate: NonZeroU32::new(32000).unwrap(),
                channels: NonZeroU8::new(1).unwrap(),
                parameters: RtpCodecParametersParameters::default(),
                rtcp_feedback: vec![RtcpFeedback::TransportCc],
            },
            RtpCodecCapability::Audio {
                mime_type: MimeTypeAudio::Isac,
                preferred_payload_type: None,
                clock_rate: NonZeroU32::new(16000).unwrap(),
                channels: NonZeroU8::new(1).unwrap(),
                parameters: RtpCodecParametersParameters::default(),
                rtcp_feedback: vec![RtcpFeedback::TransportCc],
            },
            RtpCodecCapability::Audio {
                mime_type: MimeTypeAudio::G722,
                preferred_payload_type: Some(9),
                clock_rate: NonZeroU32::new(8000).unwrap(),
                channels: NonZeroU8::new(1).unwrap(),
                parameters: RtpCodecParametersParameters::default(),
                rtcp_feedback: vec![RtcpFeedback::TransportCc],
            },
            RtpCodecCapability::Audio {
                mime_type: MimeTypeAudio::Ilbc,
                preferred_payload_type: None,
                clock_rate: NonZeroU32::new(8000).unwrap(),
                channels: NonZeroU8::new(1).unwrap(),
                parameters: RtpCodecParametersParameters::default(),
                rtcp_feedback: vec![RtcpFeedback::TransportCc],
            },
            RtpCodecCapability::Audio {
                mime_type: MimeTypeAudio::Silk,
                preferred_payload_type: None,
                clock_rate: NonZeroU32::new(24000).unwrap(),
                channels: NonZeroU8::new(1).unwrap(),
                parameters: RtpCodecParametersParameters::default(),
                rtcp_feedback: vec![RtcpFeedback::TransportCc],
            },
            RtpCodecCapability::Audio {
                mime_type: MimeTypeAudio::Silk,
                preferred_payload_type: None,
                clock_rate: NonZeroU32::new(16000).unwrap(),
                channels: NonZeroU8::new(1).unwrap(),
                parameters: RtpCodecParametersParameters::default(),
                rtcp_feedback: vec![RtcpFeedback::TransportCc],
            },
            RtpCodecCapability::Audio {
                mime_type: MimeTypeAudio::Silk,
                preferred_payload_type: None,
                clock_rate: NonZeroU32::new(12000).unwrap(),
                channels: NonZeroU8::new(1).unwrap(),
                parameters: RtpCodecParametersParameters::default(),
                rtcp_feedback: vec![RtcpFeedback::TransportCc],
            },
            RtpCodecCapability::Audio {
                mime_type: MimeTypeAudio::Silk,
                preferred_payload_type: None,
                clock_rate: NonZeroU32::new(8000).unwrap(),
                channels: NonZeroU8::new(1).unwrap(),
                parameters: RtpCodecParametersParameters::default(),
                rtcp_feedback: vec![RtcpFeedback::TransportCc],
            },
            RtpCodecCapability::Audio {
                mime_type: MimeTypeAudio::Cn,
                preferred_payload_type: Some(13),
                clock_rate: NonZeroU32::new(32000).unwrap(),
                channels: NonZeroU8::new(1).unwrap(),
                parameters: RtpCodecParametersParameters::default(),
                rtcp_feedback: vec![],
            },
            RtpCodecCapability::Audio {
                mime_type: MimeTypeAudio::Cn,
                preferred_payload_type: Some(13),
                clock_rate: NonZeroU32::new(16000).unwrap(),
                channels: NonZeroU8::new(1).unwrap(),
                parameters: RtpCodecParametersParameters::default(),
                rtcp_feedback: vec![],
            },
            RtpCodecCapability::Audio {
                mime_type: MimeTypeAudio::Cn,
                preferred_payload_type: Some(13),
                clock_rate: NonZeroU32::new(8000).unwrap(),
                channels: NonZeroU8::new(1).unwrap(),
                parameters: RtpCodecParametersParameters::default(),
                rtcp_feedback: vec![],
            },
            RtpCodecCapability::Audio {
                mime_type: MimeTypeAudio::TelephoneEvent,
                preferred_payload_type: None,
                clock_rate: NonZeroU32::new(48000).unwrap(),
                channels: NonZeroU8::new(1).unwrap(),
                parameters: RtpCodecParametersParameters::default(),
                rtcp_feedback: vec![],
            },
            RtpCodecCapability::Audio {
                mime_type: MimeTypeAudio::TelephoneEvent,
                preferred_payload_type: None,
                clock_rate: NonZeroU32::new(32000).unwrap(),
                channels: NonZeroU8::new(1).unwrap(),
                parameters: RtpCodecParametersParameters::default(),
                rtcp_feedback: vec![],
            },
            RtpCodecCapability::Audio {
                mime_type: MimeTypeAudio::TelephoneEvent,
                preferred_payload_type: None,
                clock_rate: NonZeroU32::new(16000).unwrap(),
                channels: NonZeroU8::new(1).unwrap(),
                parameters: RtpCodecParametersParameters::default(),
                rtcp_feedback: vec![],
            },
            RtpCodecCapability::Audio {
                mime_type: MimeTypeAudio::TelephoneEvent,
                preferred_payload_type: None,
                clock_rate: NonZeroU32::new(8000).unwrap(),
                channels: NonZeroU8::new(1).unwrap(),
                parameters: RtpCodecParametersParameters::default(),
                rtcp_feedback: vec![],
            },
            RtpCodecCapability::Video {
                mime_type: MimeTypeVideo::Vp8,
                preferred_payload_type: None,
                clock_rate: NonZeroU32::new(90000).unwrap(),
                parameters: RtpCodecParametersParameters::default(),
                rtcp_feedback: vec![
                    RtcpFeedback::Nack,
                    RtcpFeedback::NackPli,
                    RtcpFeedback::CcmFir,
                    RtcpFeedback::GoogRemb,
                    RtcpFeedback::TransportCc,
                ],
            },
            RtpCodecCapability::Video {
                mime_type: MimeTypeVideo::Vp9,
                preferred_payload_type: None,
                clock_rate: NonZeroU32::new(90000).unwrap(),
                parameters: RtpCodecParametersParameters::default(),
                rtcp_feedback: vec![
                    RtcpFeedback::Nack,
                    RtcpFeedback::NackPli,
                    RtcpFeedback::CcmFir,
                    RtcpFeedback::GoogRemb,
                    RtcpFeedback::TransportCc,
                ],
            },
            RtpCodecCapability::Video {
                mime_type: MimeTypeVideo::H264,
                preferred_payload_type: None,
                clock_rate: NonZeroU32::new(90000).unwrap(),
                parameters: RtpCodecParametersParameters::from([(
                    "level-asymmetry-allowed",
                    1_u32.into(),
                )]),
                rtcp_feedback: vec![
                    RtcpFeedback::Nack,
                    RtcpFeedback::NackPli,
                    RtcpFeedback::CcmFir,
                    RtcpFeedback::GoogRemb,
                    RtcpFeedback::TransportCc,
                ],
            },
            RtpCodecCapability::Video {
                mime_type: MimeTypeVideo::H264Svc,
                preferred_payload_type: None,
                clock_rate: NonZeroU32::new(90000).unwrap(),
                parameters: RtpCodecParametersParameters::from([(
                    "level-asymmetry-allowed",
                    1_u32.into(),
                )]),
                rtcp_feedback: vec![
                    RtcpFeedback::Nack,
                    RtcpFeedback::NackPli,
                    RtcpFeedback::CcmFir,
                    RtcpFeedback::GoogRemb,
                    RtcpFeedback::TransportCc,
                ],
            },
            RtpCodecCapability::Video {
                mime_type: MimeTypeVideo::H265,
                preferred_payload_type: None,
                clock_rate: NonZeroU32::new(90000).unwrap(),
                parameters: RtpCodecParametersParameters::from([(
                    "level-asymmetry-allowed",
                    1_u32.into(),
                )]),
                rtcp_feedback: vec![
                    RtcpFeedback::Nack,
                    RtcpFeedback::NackPli,
                    RtcpFeedback::CcmFir,
                    RtcpFeedback::GoogRemb,
                    RtcpFeedback::TransportCc,
                ],
            },
        ],
        header_extensions: vec![
            RtpHeaderExtension {
                kind: MediaKind::Audio,
                uri: RtpHeaderExtensionUri::Mid,
                preferred_id: 1,
                preferred_encrypt: false,
                direction: RtpHeaderExtensionDirection::SendRecv,
            },
            RtpHeaderExtension {
                kind: MediaKind::Video,
                uri: RtpHeaderExtensionUri::Mid,
                preferred_id: 1,
                preferred_encrypt: false,
                direction: RtpHeaderExtensionDirection::SendRecv,
            },
            RtpHeaderExtension {
                kind: MediaKind::Video,
                uri: RtpHeaderExtensionUri::RtpStreamId,
                preferred_id: 2,
                preferred_encrypt: false,
                direction: RtpHeaderExtensionDirection::RecvOnly,
            },
            RtpHeaderExtension {
                kind: MediaKind::Video,
                uri: RtpHeaderExtensionUri::RepairRtpStreamId,
                preferred_id: 3,
                preferred_encrypt: false,
                direction: RtpHeaderExtensionDirection::RecvOnly,
            },
            RtpHeaderExtension {
                kind: MediaKind::Audio,
                uri: RtpHeaderExtensionUri::AbsSendTime,
                preferred_id: 4,
                preferred_encrypt: false,
                direction: RtpHeaderExtensionDirection::SendRecv,
            },
            RtpHeaderExtension {
                kind: MediaKind::Video,
                uri: RtpHeaderExtensionUri::AbsSendTime,
                preferred_id: 4,
                preferred_encrypt: false,
                direction: RtpHeaderExtensionDirection::SendRecv,
            },
            // NOTE: For audio we just enable transport-wide-cc-01 when receiving media.
            RtpHeaderExtension {
                kind: MediaKind::Audio,
                uri: RtpHeaderExtensionUri::TransportWideCcDraft01,
                preferred_id: 5,
                preferred_encrypt: false,
                direction: RtpHeaderExtensionDirection::RecvOnly,
            },
            RtpHeaderExtension {
                kind: MediaKind::Video,
                uri: RtpHeaderExtensionUri::TransportWideCcDraft01,
                preferred_id: 5,
                preferred_encrypt: false,
                direction: RtpHeaderExtensionDirection::SendRecv,
            },
            // NOTE: Remove this once framemarking draft becomes RFC.
            RtpHeaderExtension {
                kind: MediaKind::Video,
                uri: RtpHeaderExtensionUri::FrameMarkingDraft07,
                preferred_id: 6,
                preferred_encrypt: false,
                direction: RtpHeaderExtensionDirection::SendRecv,
            },
            RtpHeaderExtension {
                kind: MediaKind::Video,
                uri: RtpHeaderExtensionUri::FrameMarking,
                preferred_id: 7,
                preferred_encrypt: false,
                direction: RtpHeaderExtensionDirection::SendRecv,
            },
            RtpHeaderExtension {
                kind: MediaKind::Audio,
                uri: RtpHeaderExtensionUri::AudioLevel,
                preferred_id: 10,
                preferred_encrypt: false,
                direction: RtpHeaderExtensionDirection::SendRecv,
            },
            RtpHeaderExtension {
                kind: MediaKind::Video,
                uri: RtpHeaderExtensionUri::VideoOrientation,
                preferred_id: 11,
                preferred_encrypt: false,
                direction: RtpHeaderExtensionDirection::SendRecv,
            },
            RtpHeaderExtension {
                kind: MediaKind::Video,
                uri: RtpHeaderExtensionUri::TimeOffset,
                preferred_id: 12,
                preferred_encrypt: false,
                direction: RtpHeaderExtensionDirection::SendRecv,
            },
            RtpHeaderExtension {
                kind: MediaKind::Audio,
                uri: RtpHeaderExtensionUri::AbsCaptureTime,
                preferred_id: 13,
                preferred_encrypt: false,
                direction: RtpHeaderExtensionDirection::SendRecv,
            },
            RtpHeaderExtension {
                kind: MediaKind::Video,
                uri: RtpHeaderExtensionUri::AbsCaptureTime,
                preferred_id: 13,
                preferred_encrypt: false,
                direction: RtpHeaderExtensionDirection::SendRecv,
            },
            RtpHeaderExtension {
                kind: MediaKind::Audio,
                uri: RtpHeaderExtensionUri::PlayoutDelay,
                preferred_id: 14,
                preferred_encrypt: false,
                direction: RtpHeaderExtensionDirection::SendRecv,
            },
            RtpHeaderExtension {
                kind: MediaKind::Video,
                uri: RtpHeaderExtensionUri::PlayoutDelay,
                preferred_id: 14,
                preferred_encrypt: false,
                direction: RtpHeaderExtensionDirection::SendRecv,
            },
        ],
    }
}
