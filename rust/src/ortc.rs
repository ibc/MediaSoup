use crate::rtp_parameters::{
    MediaKind, MimeType, MimeTypeAudio, MimeTypeVideo, RtcpFeedback, RtcpParameters,
    RtpCapabilities, RtpCapabilitiesFinalized, RtpCodecCapability, RtpCodecCapabilityFinalized,
    RtpCodecParameters, RtpCodecParametersParameters, RtpCodecParametersParametersValue,
    RtpEncodingParameters, RtpEncodingParametersRtx, RtpHeaderExtensionDirection,
    RtpHeaderExtensionParameters, RtpHeaderExtensionUri, RtpParameters,
};
use crate::scalability_modes::ScalabilityMode;
use crate::supported_rtp_capabilities;
use mediasoup_sys::fbs::rtp_parameters;
use serde::{Deserialize, Serialize};
use std::borrow::Cow;
use std::collections::BTreeMap;
use std::convert::TryFrom;
use std::error::Error;
use std::mem;
use std::num::{NonZeroU32, NonZeroU8};
use std::ops::Deref;
use thiserror::Error;

#[cfg(test)]
mod tests;

const DYNAMIC_PAYLOAD_TYPES: &[u8] = &[
    100, 101, 102, 103, 104, 105, 106, 107, 108, 109, 110, 111, 112, 113, 114, 115, 116, 117, 118,
    119, 120, 121, 122, 123, 124, 125, 126, 127, 96, 97, 98, 99,
];

#[doc(hidden)]
#[derive(Debug, Default, Copy, Clone, Ord, PartialOrd, Eq, PartialEq, Deserialize, Serialize)]
#[serde(rename_all = "camelCase")]
pub struct RtpMappingCodec {
    pub payload_type: u8,
    pub mapped_payload_type: u8,
}

#[doc(hidden)]
#[derive(Debug, Default, Clone, Ord, PartialOrd, Eq, PartialEq, Deserialize, Serialize)]
#[serde(rename_all = "camelCase")]
pub struct RtpMappingEncoding {
    #[serde(skip_serializing_if = "Option::is_none")]
    pub ssrc: Option<u32>,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub rid: Option<String>,
    #[serde(default, skip_serializing_if = "ScalabilityMode::is_none")]
    pub scalability_mode: ScalabilityMode,
    pub mapped_ssrc: u32,
}

#[doc(hidden)]
#[derive(Debug, Default, Clone, Ord, PartialOrd, Eq, PartialEq, Deserialize, Serialize)]
pub struct RtpMapping {
    pub codecs: Vec<RtpMappingCodec>,
    pub encodings: Vec<RtpMappingEncoding>,
}

impl RtpMapping {
    pub(crate) fn to_fbs(&self) -> rtp_parameters::RtpMapping {
        rtp_parameters::RtpMapping {
            codecs: self
                .codecs
                .iter()
                .map(|mapping| rtp_parameters::CodecMapping {
                    payload_type: mapping.payload_type,
                    mapped_payload_type: mapping.mapped_payload_type,
                })
                .collect(),
            encodings: self
                .encodings
                .iter()
                .map(|mapping| rtp_parameters::EncodingMapping {
                    rid: mapping.rid.clone().map(|rid| rid.to_string()),
                    ssrc: mapping.ssrc,
                    scalability_mode: Some(mapping.scalability_mode.to_string()),
                    mapped_ssrc: mapping.mapped_ssrc,
                })
                .collect(),
        }
    }

    pub(crate) fn from_fbs_ref(
        mapping: rtp_parameters::RtpMappingRef<'_>,
    ) -> Result<Self, Box<dyn Error + Send + Sync>> {
        Ok(Self {
            codecs: mapping
                .codecs()?
                .iter()
                .map(|mapping| {
                    Ok(RtpMappingCodec {
                        payload_type: mapping?.payload_type()?,
                        mapped_payload_type: mapping?.mapped_payload_type()?,
                    })
                })
                .collect::<Result<Vec<_>, Box<dyn Error + Send + Sync>>>()?,
            encodings: mapping
                .encodings()?
                .iter()
                .map(|mapping| {
                    Ok(RtpMappingEncoding {
                        rid: mapping?.rid()?.map(|rid| rid.to_string()),
                        ssrc: mapping?.ssrc()?,
                        scalability_mode: mapping?
                            .scalability_mode()?
                            .map(|maybe_scalability_mode| maybe_scalability_mode.parse())
                            .transpose()?
                            .unwrap_or_default(),
                        mapped_ssrc: mapping?.mapped_ssrc()?,
                    })
                })
                .collect::<Result<Vec<_>, Box<dyn Error + Send + Sync>>>()?,
        })
    }
}

/// Error caused by invalid RTP parameters.
#[derive(Debug, Error, Eq, PartialEq)]
pub enum RtpParametersError {
    /// Invalid codec apt parameter.
    #[error("Invalid codec apt parameter {0}")]
    InvalidAptParameter(Cow<'static, str>),
}

/// Error caused by invalid RTP capabilities.
#[derive(Debug, Error, Eq, PartialEq)]
pub enum RtpCapabilitiesError {
    /// Media codec not supported.
    #[error("Media codec not supported [mime_type:{mime_type:?}")]
    UnsupportedCodec {
        /// Mime type
        mime_type: MimeType,
    },
    /// Cannot allocate more dynamic codec payload types.
    #[error("Cannot allocate more dynamic codec payload types")]
    CannotAllocate,
    /// Invalid codec apt parameter.
    #[error("Invalid codec apt parameter {0}")]
    InvalidAptParameter(Cow<'static, str>),
    /// Duplicated preferred payload type
    #[error("Duplicated preferred payload type {0}")]
    DuplicatedPreferredPayloadType(u8),
}

/// Error caused by invalid or unsupported RTP parameters given.
#[derive(Debug, Error, Eq, PartialEq)]
pub enum RtpParametersMappingError {
    /// Unsupported codec.
    #[error("Unsupported codec [mime_type:{mime_type:?}, payloadType:{payload_type}]")]
    UnsupportedCodec {
        /// Mime type.
        mime_type: MimeType,
        /// Payload type.
        payload_type: u8,
    },
    /// No RTX codec for capability codec PT.
    #[error("No RTX codec for capability codec PT {preferred_payload_type}")]
    UnsupportedRtxCodec {
        /// Preferred payload type.
        preferred_payload_type: u8,
    },
    /// Missing media codec found for RTX PT.
    #[error("Missing media codec found for RTX PT {payload_type}")]
    MissingMediaCodecForRtx {
        /// Payload type.
        payload_type: u8,
    },
}

/// Error caused by bad consumer RTP parameters.
#[derive(Debug, Error, Eq, PartialEq)]
pub enum ConsumerRtpParametersError {
    /// Invalid capabilities
    #[error("Invalid capabilities: {0}")]
    InvalidCapabilities(RtpCapabilitiesError),
    /// No compatible media codecs
    #[error("No compatible media codecs")]
    NoCompatibleMediaCodecs,
}

fn generate_ssrc() -> u32 {
    fastrand::u32(100_000_000..999_999_999)
}

/// Validates [`RtpParameters`].
pub(crate) fn validate_rtp_parameters(
    rtp_parameters: &RtpParameters,
) -> Result<(), RtpParametersError> {
    for codec in &rtp_parameters.codecs {
        validate_rtp_codec_parameters(codec)?;
    }

    Ok(())
}

/// Validates [`RtpCodecParameters`].
fn validate_rtp_codec_parameters(codec: &RtpCodecParameters) -> Result<(), RtpParametersError> {
    for (key, value) in codec.parameters().iter() {
        // Specific parameters validation.
        if key.as_ref() == "apt" {
            match value {
                RtpCodecParametersParametersValue::Number(_) => {
                    // Good
                }
                RtpCodecParametersParametersValue::String(string) => {
                    return Err(RtpParametersError::InvalidAptParameter(string.clone()));
                }
            }
        }
    }

    Ok(())
}

// Validates [`RtpCodecCapability`].
fn validate_rtp_codec_capability(codec: &RtpCodecCapability) -> Result<(), RtpCapabilitiesError> {
    for (key, value) in codec.parameters().iter() {
        // Specific parameters validation.
        if key.as_ref() == "apt" {
            match value {
                RtpCodecParametersParametersValue::Number(_) => {
                    // Good
                }
                RtpCodecParametersParametersValue::String(string) => {
                    return Err(RtpCapabilitiesError::InvalidAptParameter(string.clone()));
                }
            }
        }
    }

    Ok(())
}

/// Validates [`RtpCapabilities`].
pub(crate) fn validate_rtp_capabilities(
    caps: &RtpCapabilities,
) -> Result<(), RtpCapabilitiesError> {
    for codec in &caps.codecs {
        validate_rtp_codec_capability(codec)?;
    }

    Ok(())
}

/// Generate RTP capabilities for the Router based on the given media codecs and mediasoup supported
/// RTP capabilities.
pub(crate) fn generate_router_rtp_capabilities(
    mut media_codecs: Vec<RtpCodecCapability>,
) -> Result<RtpCapabilitiesFinalized, RtpCapabilitiesError> {
    let supported_rtp_capabilities = supported_rtp_capabilities::get_supported_rtp_capabilities();

    validate_rtp_capabilities(&supported_rtp_capabilities)?;

    let mut dynamic_payload_types = Vec::from(DYNAMIC_PAYLOAD_TYPES);
    let mut caps = RtpCapabilitiesFinalized {
        codecs: vec![],
        header_extensions: supported_rtp_capabilities.header_extensions,
    };

    for media_codec in &mut media_codecs {
        validate_rtp_codec_capability(media_codec)?;

        let codec = match supported_rtp_capabilities
            .codecs
            .iter()
            .find(|supported_codec| {
                match_codecs(media_codec.deref().into(), (*supported_codec).into(), false).is_ok()
            }) {
            Some(codec) => codec,
            None => {
                return Err(RtpCapabilitiesError::UnsupportedCodec {
                    mime_type: media_codec.mime_type(),
                });
            }
        };

        let preferred_payload_type = match media_codec.preferred_payload_type() {
            Some(preferred_payload_type) => {
                // If the given media codec has preferred_payload_type, keep it.
                // Also remove the payload_type from the list of available dynamic values.
                dynamic_payload_types.retain(|&pt| pt != preferred_payload_type);

                preferred_payload_type
            }
            None => {
                if let Some(preferred_payload_type) = codec.preferred_payload_type() {
                    // Otherwise if the supported codec has preferredPayloadType, use it.
                    // No need to remove it from the list since it's not a dynamic value.
                    preferred_payload_type
                } else {
                    // Otherwise choose a dynamic one.
                    if dynamic_payload_types.is_empty() {
                        return Err(RtpCapabilitiesError::CannotAllocate);
                    }
                    // Take the first available payload type and remove it from the list.
                    dynamic_payload_types.remove(0)
                }
            }
        };

        // Ensure there is not duplicated preferredPayloadType values.
        for codec in &caps.codecs {
            if codec.preferred_payload_type() == preferred_payload_type {
                return Err(RtpCapabilitiesError::DuplicatedPreferredPayloadType(
                    preferred_payload_type,
                ));
            }
        }

        let codec_finalized = match codec {
            RtpCodecCapability::Audio {
                mime_type,
                preferred_payload_type: _,
                clock_rate,
                channels,
                parameters,
                rtcp_feedback,
            } => RtpCodecCapabilityFinalized::Audio {
                mime_type: *mime_type,
                preferred_payload_type,
                clock_rate: *clock_rate,
                channels: *channels,
                parameters: {
                    // Merge the media codec parameters.
                    let mut parameters = parameters.clone();
                    parameters.extend(mem::take(media_codec.parameters_mut()));
                    parameters
                },
                rtcp_feedback: rtcp_feedback.clone(),
            },
            RtpCodecCapability::Video {
                mime_type,
                preferred_payload_type: _,
                clock_rate,
                parameters,
                rtcp_feedback,
            } => RtpCodecCapabilityFinalized::Video {
                mime_type: *mime_type,
                preferred_payload_type,
                clock_rate: *clock_rate,
                parameters: {
                    // Merge the media codec parameters.
                    let mut parameters = parameters.clone();
                    parameters.extend(mem::take(media_codec.parameters_mut()));
                    parameters
                },
                rtcp_feedback: rtcp_feedback.clone(),
            },
        };

        // Add a RTX video codec if video.
        if matches!(codec_finalized, RtpCodecCapabilityFinalized::Video { .. }) {
            if dynamic_payload_types.is_empty() {
                return Err(RtpCapabilitiesError::CannotAllocate);
            }
            // Take the first available payload_type and remove it from the list.
            let payload_type = dynamic_payload_types.remove(0);

            let rtx_codec = RtpCodecCapabilityFinalized::Video {
                mime_type: MimeTypeVideo::Rtx,
                preferred_payload_type: payload_type,
                clock_rate: codec_finalized.clock_rate(),
                parameters: RtpCodecParametersParameters::from([(
                    "apt",
                    codec_finalized.preferred_payload_type().into(),
                )]),
                rtcp_feedback: vec![],
            };

            // Append to the codec list.
            caps.codecs.push(codec_finalized);
            caps.codecs.push(rtx_codec);
        } else {
            // Append to the codec list.
            caps.codecs.push(codec_finalized);
        }
    }

    Ok(caps)
}

/// Get a mapping of codec payloads and encodings of the given Producer RTP parameters as values
/// expected by the Router.
pub(crate) fn get_producer_rtp_parameters_mapping(
    rtp_parameters: &RtpParameters,
    rtp_capabilities: &RtpCapabilitiesFinalized,
) -> Result<RtpMapping, RtpParametersMappingError> {
    let mut rtp_mapping = RtpMapping::default();

    // Match parameters media codecs to capabilities media codecs.
    let mut codec_to_cap_codec =
        BTreeMap::<&RtpCodecParameters, Cow<'_, RtpCodecCapabilityFinalized>>::new();

    for codec in &rtp_parameters.codecs {
        if codec.is_rtx() {
            continue;
        }

        // Search for the same media codec in capabilities.
        match rtp_capabilities.codecs.iter().find_map(|cap_codec| {
            match_codecs(codec.into(), cap_codec.into(), true)
                .ok()
                .map(|profile_level_id| {
                    // This is rather ugly, but we need to fix `profile-level-id` and this was the
                    // quickest way to do it
                    profile_level_id.map_or(Cow::Borrowed(cap_codec), |profile_level_id| {
                        let mut cap_codec = cap_codec.clone();
                        cap_codec
                            .parameters_mut()
                            .insert("profile-level-id", profile_level_id);
                        Cow::Owned(cap_codec)
                    })
                })
        }) {
            Some(matched_codec_capability) => {
                codec_to_cap_codec.insert(codec, matched_codec_capability);
            }
            None => {
                return Err(RtpParametersMappingError::UnsupportedCodec {
                    mime_type: codec.mime_type(),
                    payload_type: codec.payload_type(),
                });
            }
        }
    }

    // Match parameters RTX codecs to capabilities RTX codecs.
    for codec in &rtp_parameters.codecs {
        if !codec.is_rtx() {
            continue;
        }

        // Search for the associated media codec.
        let associated_media_codec = rtp_parameters.codecs.iter().find(|media_codec| {
            let media_codec_payload_type = media_codec.payload_type();
            let codec_parameters_apt = codec.parameters().get("apt");

            match codec_parameters_apt {
                Some(RtpCodecParametersParametersValue::Number(apt)) => {
                    u32::from(media_codec_payload_type) == *apt
                }
                _ => false,
            }
        });

        match associated_media_codec {
            Some(associated_media_codec) => {
                let cap_media_codec = codec_to_cap_codec.get(associated_media_codec).unwrap();

                // Ensure that the capabilities media codec has a RTX codec.
                let associated_cap_rtx_codec = rtp_capabilities.codecs.iter().find(|cap_codec| {
                    if !cap_codec.is_rtx() {
                        return false;
                    }

                    let cap_codec_parameters_apt = cap_codec.parameters().get("apt");
                    match cap_codec_parameters_apt {
                        Some(RtpCodecParametersParametersValue::Number(apt)) => {
                            u32::from(cap_media_codec.preferred_payload_type()) == *apt
                        }
                        _ => false,
                    }
                });

                match associated_cap_rtx_codec {
                    Some(associated_cap_rtx_codec) => {
                        codec_to_cap_codec.insert(codec, Cow::Borrowed(associated_cap_rtx_codec));
                    }
                    None => {
                        return Err(RtpParametersMappingError::UnsupportedRtxCodec {
                            preferred_payload_type: cap_media_codec.preferred_payload_type(),
                        });
                    }
                }
            }
            None => {
                return Err(RtpParametersMappingError::MissingMediaCodecForRtx {
                    payload_type: codec.payload_type(),
                });
            }
        }
    }

    // Generate codecs mapping.
    for (codec, cap_codec) in codec_to_cap_codec {
        rtp_mapping.codecs.push(RtpMappingCodec {
            payload_type: codec.payload_type(),
            mapped_payload_type: cap_codec.preferred_payload_type(),
        });
    }

    // Generate encodings mapping.
    let mut mapped_ssrc: u32 = generate_ssrc();

    for encoding in &rtp_parameters.encodings {
        rtp_mapping.encodings.push(RtpMappingEncoding {
            ssrc: encoding.ssrc,
            rid: encoding.rid.clone(),
            scalability_mode: encoding.scalability_mode.clone(),
            mapped_ssrc,
        });

        mapped_ssrc += 1;
    }

    Ok(rtp_mapping)
}

// Generate RTP parameters to be internally used by Consumers given the RTP parameters of a Producer
// and the RTP capabilities of the Router.
pub(crate) fn get_consumable_rtp_parameters(
    kind: MediaKind,
    params: &RtpParameters,
    caps: &RtpCapabilitiesFinalized,
    rtp_mapping: &RtpMapping,
) -> RtpParameters {
    let mut consumable_params = RtpParameters::default();

    for codec in &params.codecs {
        if codec.is_rtx() {
            continue;
        }

        let consumable_codec_pt = rtp_mapping
            .codecs
            .iter()
            .find(|entry| entry.payload_type == codec.payload_type())
            .unwrap()
            .mapped_payload_type;

        let consumable_codec = match caps
            .codecs
            .iter()
            .find(|cap_codec| cap_codec.preferred_payload_type() == consumable_codec_pt)
            .unwrap()
        {
            RtpCodecCapabilityFinalized::Audio {
                mime_type,
                preferred_payload_type,
                clock_rate,
                channels,
                parameters: _,
                rtcp_feedback,
            } => {
                RtpCodecParameters::Audio {
                    mime_type: *mime_type,
                    payload_type: *preferred_payload_type,
                    clock_rate: *clock_rate,
                    channels: *channels,
                    // Keep the Producer codec parameters.
                    parameters: codec.parameters().clone(),
                    rtcp_feedback: rtcp_feedback.clone(),
                }
            }
            RtpCodecCapabilityFinalized::Video {
                mime_type,
                preferred_payload_type,
                clock_rate,
                parameters: _,
                rtcp_feedback,
            } => {
                RtpCodecParameters::Video {
                    mime_type: *mime_type,
                    payload_type: *preferred_payload_type,
                    clock_rate: *clock_rate,
                    // Keep the Producer codec parameters.
                    parameters: codec.parameters().clone(),
                    rtcp_feedback: rtcp_feedback.clone(),
                }
            }
        };

        let consumable_cap_rtx_codec = caps.codecs.iter().find(|cap_rtx_codec| {
            if !cap_rtx_codec.is_rtx() {
                return false;
            }

            let cap_rtx_codec_parameters_apt = cap_rtx_codec.parameters().get("apt");

            match cap_rtx_codec_parameters_apt {
                Some(RtpCodecParametersParametersValue::Number(apt)) => {
                    u8::try_from(*apt).map_or(false, |apt| apt == consumable_codec.payload_type())
                }
                _ => false,
            }
        });

        consumable_params.codecs.push(consumable_codec);

        if let Some(consumable_cap_rtx_codec) = consumable_cap_rtx_codec {
            let consumable_rtx_codec = match consumable_cap_rtx_codec {
                RtpCodecCapabilityFinalized::Audio {
                    mime_type,
                    preferred_payload_type,
                    clock_rate,
                    channels,
                    parameters,
                    rtcp_feedback,
                } => RtpCodecParameters::Audio {
                    mime_type: *mime_type,
                    payload_type: *preferred_payload_type,
                    clock_rate: *clock_rate,
                    channels: *channels,
                    parameters: parameters.clone(),
                    rtcp_feedback: rtcp_feedback.clone(),
                },
                RtpCodecCapabilityFinalized::Video {
                    mime_type,
                    preferred_payload_type,
                    clock_rate,
                    parameters,
                    rtcp_feedback,
                } => RtpCodecParameters::Video {
                    mime_type: *mime_type,
                    payload_type: *preferred_payload_type,
                    clock_rate: *clock_rate,
                    parameters: parameters.clone(),
                    rtcp_feedback: rtcp_feedback.clone(),
                },
            };

            consumable_params.codecs.push(consumable_rtx_codec);
        }
    }

    for cap_ext in &caps.header_extensions {
        // Just take RTP header extension that can be used in Consumers.
        if cap_ext.kind != kind {
            continue;
        }
        if !matches!(
            cap_ext.direction,
            RtpHeaderExtensionDirection::SendRecv | RtpHeaderExtensionDirection::SendOnly
        ) {
            continue;
        }

        let consumable_ext = RtpHeaderExtensionParameters {
            uri: cap_ext.uri,
            id: cap_ext.preferred_id,
            encrypt: cap_ext.preferred_encrypt,
        };

        consumable_params.header_extensions.push(consumable_ext);
    }

    for (consumable_encoding, mapped_ssrc) in params.encodings.iter().zip(
        rtp_mapping
            .encodings
            .iter()
            .map(|encoding| encoding.mapped_ssrc),
    ) {
        let mut consumable_encoding = consumable_encoding.clone();
        // Remove useless fields.
        consumable_encoding.rid.take();
        consumable_encoding.rtx.take();
        consumable_encoding.codec_payload_type.take();

        // Set the mapped ssrc.
        consumable_encoding.ssrc = Some(mapped_ssrc);

        consumable_params.encodings.push(consumable_encoding);
    }

    consumable_params.rtcp = RtcpParameters {
        cname: params.rtcp.cname.clone(),
        reduced_size: true,
    };

    consumable_params
}

/// Check whether the given RTP capabilities can consume the given Producer.
pub(crate) fn can_consume(
    consumable_params: &RtpParameters,
    caps: &RtpCapabilities,
) -> Result<bool, RtpCapabilitiesError> {
    validate_rtp_capabilities(caps)?;

    let mut matching_codecs = Vec::<&RtpCodecParameters>::new();

    for codec in &consumable_params.codecs {
        if caps
            .codecs
            .iter()
            .any(|cap_codec| match_codecs(cap_codec.into(), codec.into(), true).is_ok())
        {
            matching_codecs.push(codec);
        }
    }

    // Ensure there is at least one media codec.
    Ok(matching_codecs
        .first()
        .map(|codec| !codec.is_rtx())
        .unwrap_or_default())
}

/// Generate RTP parameters for a specific Consumer.
///
/// It reduces encodings to just one and takes into account given RTP capabilities to reduce codecs,
/// codecs' RTCP feedback and header extensions, and also enables or disabled RTX.
#[allow(clippy::suspicious_operation_groupings)]
pub(crate) fn get_consumer_rtp_parameters(
    consumable_rtp_parameters: &RtpParameters,
    remote_rtp_capabilities: &RtpCapabilities,
    pipe: bool,
    enable_rtx: bool,
) -> Result<RtpParameters, ConsumerRtpParametersError> {
    let mut consumer_params = RtpParameters {
        rtcp: consumable_rtp_parameters.rtcp.clone(),
        ..RtpParameters::default()
    };

    for cap_codec in &remote_rtp_capabilities.codecs {
        validate_rtp_codec_capability(cap_codec)
            .map_err(ConsumerRtpParametersError::InvalidCapabilities)?;
    }

    let mut rtx_supported = false;

    for mut codec in consumable_rtp_parameters.codecs.clone() {
        if !enable_rtx && codec.is_rtx() {
            continue;
        }

        if let Some(matched_cap_codec) = remote_rtp_capabilities
            .codecs
            .iter()
            .find(|cap_codec| match_codecs((*cap_codec).into(), (&codec).into(), true).is_ok())
        {
            *codec.rtcp_feedback_mut() = matched_cap_codec
                .rtcp_feedback()
                .iter()
                .filter(|&&fb| enable_rtx || fb != RtcpFeedback::Nack)
                .copied()
                .collect();

            consumer_params.codecs.push(codec);
        }
    }
    // Must sanitize the list of matched codecs by removing useless RTX codecs.
    let mut remove_codecs = Vec::new();
    for (idx, codec) in consumer_params.codecs.iter().enumerate() {
        if codec.is_rtx() {
            // Search for the associated media codec.
            let associated_media_codec = consumer_params.codecs.iter().find(|media_codec| {
                match codec.parameters().get("apt") {
                    Some(RtpCodecParametersParametersValue::Number(apt)) => {
                        u8::try_from(*apt).map_or(false, |apt| media_codec.payload_type() == apt)
                    }
                    _ => false,
                }
            });

            if associated_media_codec.is_some() {
                rtx_supported = true;
            } else {
                remove_codecs.push(idx);
            }
        }
    }
    for idx in remove_codecs.into_iter().rev() {
        consumer_params.codecs.remove(idx);
    }

    // Ensure there is at least one media codec.
    if consumer_params.codecs.is_empty() || consumer_params.codecs[0].is_rtx() {
        return Err(ConsumerRtpParametersError::NoCompatibleMediaCodecs);
    }

    consumer_params.header_extensions = consumable_rtp_parameters
        .header_extensions
        .iter()
        .filter(|ext| {
            remote_rtp_capabilities
                .header_extensions
                .iter()
                .any(|cap_ext| cap_ext.preferred_id == ext.id && cap_ext.uri == ext.uri)
        })
        .cloned()
        .collect();

    // Reduce codecs' RTCP feedback. Use Transport-CC if available, REMB otherwise.
    if consumer_params
        .header_extensions
        .iter()
        .any(|ext| ext.uri == RtpHeaderExtensionUri::TransportWideCcDraft01)
    {
        for codec in &mut consumer_params.codecs {
            codec
                .rtcp_feedback_mut()
                .retain(|fb| fb != &RtcpFeedback::GoogRemb);
        }
    } else if consumer_params
        .header_extensions
        .iter()
        .any(|ext| ext.uri == RtpHeaderExtensionUri::AbsSendTime)
    {
        for codec in &mut consumer_params.codecs {
            codec
                .rtcp_feedback_mut()
                .retain(|fb| fb != &RtcpFeedback::TransportCc);
        }
    } else {
        for codec in &mut consumer_params.codecs {
            codec
                .rtcp_feedback_mut()
                .retain(|fb| !matches!(fb, RtcpFeedback::GoogRemb | RtcpFeedback::TransportCc));
        }
    }

    if pipe {
        for ((encoding, ssrc), rtx_ssrc) in consumable_rtp_parameters
            .encodings
            .iter()
            .zip(generate_ssrc()..)
            .zip(generate_ssrc()..)
        {
            consumer_params.encodings.push(RtpEncodingParameters {
                ssrc: Some(ssrc),
                rtx: if rtx_supported {
                    Some(RtpEncodingParametersRtx { ssrc: rtx_ssrc })
                } else {
                    None
                },
                ..encoding.clone()
            });
        }
    } else {
        let mut consumer_encoding = RtpEncodingParameters {
            ssrc: Some(generate_ssrc()),
            ..RtpEncodingParameters::default()
        };

        if rtx_supported {
            consumer_encoding.rtx = Some(RtpEncodingParametersRtx {
                ssrc: consumer_encoding.ssrc.unwrap() + 1,
            });
        }

        // If any of the consumable_rtp_parameters.encodings has scalability_mode, process it
        // (assume all encodings have the same value).
        let mut scalability_mode = consumable_rtp_parameters
            .encodings
            .first()
            .map(|encoding| encoding.scalability_mode.clone())
            .unwrap_or_default();

        // If there is simulcast, mangle spatial layers in scalabilityMode.
        if consumable_rtp_parameters.encodings.len() > 1 {
            scalability_mode = format!(
                "L{}T{}",
                consumable_rtp_parameters.encodings.len(),
                scalability_mode.temporal_layers()
            )
            .parse()
            .unwrap();
        }

        consumer_encoding.scalability_mode = scalability_mode;

        // Use the maximum max_bitrate in any encoding and honor it in the Consumer's encoding.
        consumer_encoding.max_bitrate = consumable_rtp_parameters
            .encodings
            .iter()
            .map(|encoding| encoding.max_bitrate)
            .max()
            .flatten();

        // Set a single encoding for the Consumer.
        consumer_params.encodings.push(consumer_encoding);
    }

    Ok(consumer_params)
}

/// Generate RTP parameters for a pipe Consumer.
///
/// It keeps all original consumable encodings and removes support for BWE. If
/// enableRtx is false, it also removes RTX and NACK support.
pub(crate) fn get_pipe_consumer_rtp_parameters(
    consumable_rtp_parameters: &RtpParameters,
    enable_rtx: bool,
) -> RtpParameters {
    let mut consumer_params = RtpParameters {
        mid: None,
        codecs: vec![],
        header_extensions: vec![],
        encodings: vec![],
        rtcp: consumable_rtp_parameters.rtcp.clone(),
    };

    for codec in &consumable_rtp_parameters.codecs {
        if !enable_rtx && codec.is_rtx() {
            continue;
        }

        let mut codec = codec.clone();

        codec.rtcp_feedback_mut().retain(|fb| {
            matches!(fb, RtcpFeedback::NackPli | RtcpFeedback::CcmFir)
                || (enable_rtx && fb == &RtcpFeedback::Nack)
        });

        consumer_params.codecs.push(codec);
    }

    // Reduce RTP extensions by disabling transport MID and BWE related ones.
    consumer_params.header_extensions = consumable_rtp_parameters
        .header_extensions
        .iter()
        .filter(|ext| {
            !matches!(
                ext.uri,
                RtpHeaderExtensionUri::Mid
                    | RtpHeaderExtensionUri::AbsSendTime
                    | RtpHeaderExtensionUri::TransportWideCcDraft01
            )
        })
        .cloned()
        .collect();

    for ((encoding, ssrc), rtx_ssrc) in consumable_rtp_parameters
        .encodings
        .iter()
        .zip(generate_ssrc()..)
        .zip(generate_ssrc()..)
    {
        consumer_params.encodings.push(RtpEncodingParameters {
            ssrc: Some(ssrc),
            rtx: if enable_rtx {
                Some(RtpEncodingParametersRtx { ssrc: rtx_ssrc })
            } else {
                None
            },
            ..encoding.clone()
        });
    }

    consumer_params
}

struct CodecToMatch<'a> {
    channels: Option<NonZeroU8>,
    clock_rate: NonZeroU32,
    mime_type: MimeType,
    parameters: &'a RtpCodecParametersParameters,
}

impl<'a> From<&'a RtpCodecCapability> for CodecToMatch<'a> {
    fn from(rtp_codec_capability: &'a RtpCodecCapability) -> Self {
        match rtp_codec_capability {
            RtpCodecCapability::Audio {
                mime_type,
                channels,
                clock_rate,
                parameters,
                ..
            } => Self {
                channels: Some(*channels),
                clock_rate: *clock_rate,
                mime_type: MimeType::Audio(*mime_type),
                parameters,
            },
            RtpCodecCapability::Video {
                mime_type,
                clock_rate,
                parameters,
                ..
            } => Self {
                channels: None,
                clock_rate: *clock_rate,
                mime_type: MimeType::Video(*mime_type),
                parameters,
            },
        }
    }
}

impl<'a> From<&'a RtpCodecCapabilityFinalized> for CodecToMatch<'a> {
    fn from(rtp_codec_capability: &'a RtpCodecCapabilityFinalized) -> Self {
        match rtp_codec_capability {
            RtpCodecCapabilityFinalized::Audio {
                mime_type,
                channels,
                clock_rate,
                parameters,
                ..
            } => Self {
                channels: Some(*channels),
                clock_rate: *clock_rate,
                mime_type: MimeType::Audio(*mime_type),
                parameters,
            },
            RtpCodecCapabilityFinalized::Video {
                mime_type,
                clock_rate,
                parameters,
                ..
            } => Self {
                channels: None,
                clock_rate: *clock_rate,
                mime_type: MimeType::Video(*mime_type),
                parameters,
            },
        }
    }
}

impl<'a> From<&'a RtpCodecParameters> for CodecToMatch<'a> {
    fn from(rtp_codec_parameters: &'a RtpCodecParameters) -> Self {
        match rtp_codec_parameters {
            RtpCodecParameters::Audio {
                mime_type,
                channels,
                clock_rate,
                parameters,
                ..
            } => Self {
                channels: Some(*channels),
                clock_rate: *clock_rate,
                mime_type: MimeType::Audio(*mime_type),
                parameters,
            },
            RtpCodecParameters::Video {
                mime_type,
                clock_rate,
                parameters,
                ..
            } => Self {
                channels: None,
                clock_rate: *clock_rate,
                mime_type: MimeType::Video(*mime_type),
                parameters,
            },
        }
    }
}

/// Returns selected `Ok(Some(profile-level-id))` for H264 codec and `Ok(None)` for others
fn match_codecs(
    codec_a: CodecToMatch<'_>,
    codec_b: CodecToMatch<'_>,
    strict: bool,
) -> Result<Option<String>, ()> {
    if codec_a.mime_type != codec_b.mime_type {
        return Err(());
    }

    if codec_a.channels != codec_b.channels {
        return Err(());
    }

    if codec_a.clock_rate != codec_b.clock_rate {
        return Err(());
    }
    // Per codec special checks.
    match codec_a.mime_type {
        MimeType::Audio(MimeTypeAudio::MultiChannelOpus) => {
            let num_streams_a = codec_a.parameters.get("num_streams");
            let num_streams_b = codec_b.parameters.get("num_streams");

            if num_streams_a != num_streams_b {
                return Err(());
            }

            let coupled_streams_a = codec_a.parameters.get("coupled_streams");
            let coupled_streams_b = codec_b.parameters.get("coupled_streams");

            if coupled_streams_a != coupled_streams_b {
                return Err(());
            }
        }
        MimeType::Video(MimeTypeVideo::H264 | MimeTypeVideo::H264Svc) => {
            if strict {
                let packetization_mode_a = codec_a
                    .parameters
                    .get("packetization-mode")
                    .unwrap_or(&RtpCodecParametersParametersValue::Number(0));
                let packetization_mode_b = codec_b
                    .parameters
                    .get("packetization-mode")
                    .unwrap_or(&RtpCodecParametersParametersValue::Number(0));

                if packetization_mode_a != packetization_mode_b {
                    return Err(());
                }

                let profile_level_id_a =
                    codec_a
                        .parameters
                        .get("profile-level-id")
                        .and_then(|p| match p {
                            RtpCodecParametersParametersValue::String(s) => Some(s.as_ref()),
                            RtpCodecParametersParametersValue::Number(_) => None,
                        });
                let profile_level_id_b =
                    codec_b
                        .parameters
                        .get("profile-level-id")
                        .and_then(|p| match p {
                            RtpCodecParametersParametersValue::String(s) => Some(s.as_ref()),
                            RtpCodecParametersParametersValue::Number(_) => None,
                        });

                let (profile_level_id_a, profile_level_id_b) =
                    match h264_profile_level_id::is_same_profile(
                        profile_level_id_a,
                        profile_level_id_b,
                    ) {
                        Some((profile_level_id_a, profile_level_id_b)) => {
                            (profile_level_id_a, profile_level_id_b)
                        }
                        None => {
                            return Err(());
                        }
                    };

                let selected_profile_level_id =
                    h264_profile_level_id::generate_profile_level_id_for_answer(
                        Some(profile_level_id_a),
                        codec_a
                            .parameters
                            .get("level-asymmetry-allowed")
                            .map(|p| p == &RtpCodecParametersParametersValue::Number(1))
                            .unwrap_or_default(),
                        Some(profile_level_id_b),
                        codec_b
                            .parameters
                            .get("level-asymmetry-allowed")
                            .map(|p| p == &RtpCodecParametersParametersValue::Number(1))
                            .unwrap_or_default(),
                    );

                return match selected_profile_level_id {
                    Ok(selected_profile_level_id) => {
                        Ok(Some(selected_profile_level_id.to_string()))
                    }
                    Err(_) => Err(()),
                };
            }
        }
        MimeType::Video(MimeTypeVideo::Vp9) => {
            // If strict matching check profile-id.
            if strict {
                let profile_id_a = codec_a
                    .parameters
                    .get("profile-id")
                    .unwrap_or(&RtpCodecParametersParametersValue::Number(0));
                let profile_id_b = codec_b
                    .parameters
                    .get("profile-id")
                    .unwrap_or(&RtpCodecParametersParametersValue::Number(0));

                if profile_id_a != profile_id_b {
                    return Err(());
                }
            }
        }

        _ => {}
    }

    Ok(None)
}
