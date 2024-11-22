use super::*;
use crate::rtp_parameters::{MimeTypeAudio, RtpHeaderExtension};
use std::iter;

#[test]
fn generate_router_rtp_capabilities_succeeds() {
    let media_codecs = vec![
        RtpCodecCapability::Audio {
            mime_type: MimeTypeAudio::Opus,
            preferred_payload_type: None,
            clock_rate: NonZeroU32::new(48000).unwrap(),
            channels: NonZeroU8::new(2).unwrap(),
            parameters: RtpCodecParametersParameters::from([
                ("useinbandfec", 1_u32.into()),
                ("foo", "bar".into()),
            ]),
            rtcp_feedback: vec![],
        },
        RtpCodecCapability::Video {
            mime_type: MimeTypeVideo::Vp8,
            preferred_payload_type: Some(125), // Let's force it.
            clock_rate: NonZeroU32::new(90000).unwrap(),
            parameters: RtpCodecParametersParameters::default(),
            rtcp_feedback: vec![],
        },
        RtpCodecCapability::Video {
            mime_type: MimeTypeVideo::H264,
            preferred_payload_type: None,
            clock_rate: NonZeroU32::new(90000).unwrap(),
            parameters: RtpCodecParametersParameters::from([
                ("level-asymmetry-allowed", 1_u32.into()),
                ("profile-level-id", "42e01f".into()),
                ("foo", "bar".into()),
            ]),
            rtcp_feedback: vec![], // Will be ignored.
        },
    ];

    let rtp_capabilities = generate_router_rtp_capabilities(media_codecs)
        .expect("Failed to generate router RTP capabilities");

    assert_eq!(
        rtp_capabilities.codecs,
        vec![
            RtpCodecCapabilityFinalized::Audio {
                mime_type: MimeTypeAudio::Opus,
                preferred_payload_type: 100, // 100 is the first available dynamic PT.
                clock_rate: NonZeroU32::new(48000).unwrap(),
                channels: NonZeroU8::new(2).unwrap(),
                parameters: RtpCodecParametersParameters::from([
                    ("useinbandfec", 1_u32.into()),
                    ("foo", "bar".into()),
                ]),
                rtcp_feedback: vec![RtcpFeedback::Nack, RtcpFeedback::TransportCc,],
            },
            RtpCodecCapabilityFinalized::Video {
                mime_type: MimeTypeVideo::Vp8,
                preferred_payload_type: 125,
                clock_rate: NonZeroU32::new(90000).unwrap(),
                parameters: RtpCodecParametersParameters::default(),
                rtcp_feedback: vec![
                    RtcpFeedback::Nack,
                    RtcpFeedback::NackPli,
                    RtcpFeedback::CcmFir,
                    RtcpFeedback::GoogRemb,
                    RtcpFeedback::TransportCc
                ],
            },
            RtpCodecCapabilityFinalized::Video {
                mime_type: MimeTypeVideo::Rtx,
                preferred_payload_type: 101, // 101 is the second available dynamic PT.
                clock_rate: NonZeroU32::new(90000).unwrap(),
                parameters: RtpCodecParametersParameters::from([("apt", 125u32.into())]),
                rtcp_feedback: vec![],
            },
            RtpCodecCapabilityFinalized::Video {
                mime_type: MimeTypeVideo::H264,
                preferred_payload_type: 102, // 102 is the third available dynamic PT.
                clock_rate: NonZeroU32::new(90000).unwrap(),
                parameters: RtpCodecParametersParameters::from([
                    // Since packetization-mode param was not included in the
                    // H264 codec and it's default value is 0, it's not added
                    // by ortc file.
                    // ("packetization-mode", 0_u32.into()),
                    ("level-asymmetry-allowed", 1_u32.into()),
                    ("profile-level-id", "42e01f".into()),
                    ("foo", "bar".into()),
                ]),
                rtcp_feedback: vec![
                    RtcpFeedback::Nack,
                    RtcpFeedback::NackPli,
                    RtcpFeedback::CcmFir,
                    RtcpFeedback::GoogRemb,
                    RtcpFeedback::TransportCc,
                ],
            },
            RtpCodecCapabilityFinalized::Video {
                mime_type: MimeTypeVideo::Rtx,
                preferred_payload_type: 103,
                clock_rate: NonZeroU32::new(90000).unwrap(),
                parameters: RtpCodecParametersParameters::from([("apt", 102u32.into())]),
                rtcp_feedback: vec![],
            },
        ]
    );
}

#[test]
fn generate_router_rtp_capabilities_unsupported() {
    assert!(matches!(
        generate_router_rtp_capabilities(vec![RtpCodecCapability::Audio {
            mime_type: MimeTypeAudio::Opus,
            preferred_payload_type: None,
            clock_rate: NonZeroU32::new(48000).unwrap(),
            channels: NonZeroU8::new(1).unwrap(),
            parameters: RtpCodecParametersParameters::default(),
            rtcp_feedback: vec![],
        }]),
        Err(RtpCapabilitiesError::UnsupportedCodec { .. })
    ));
}

#[test]
fn generate_router_rtp_capabilities_too_many_codecs() {
    assert!(matches!(
        generate_router_rtp_capabilities(
            iter::repeat(RtpCodecCapability::Audio {
                mime_type: MimeTypeAudio::Opus,
                preferred_payload_type: None,
                clock_rate: NonZeroU32::new(48000).unwrap(),
                channels: NonZeroU8::new(2).unwrap(),
                parameters: RtpCodecParametersParameters::default(),
                rtcp_feedback: vec![],
            })
            .take(100)
            .collect::<Vec<_>>()
        ),
        Err(RtpCapabilitiesError::CannotAllocate)
    ));
}

#[test]
fn get_producer_rtp_parameters_mapping_get_consumable_rtp_parameters_get_consumer_rtp_parameters_get_pipe_consumer_rtp_parameters_succeeds(
) {
    let media_codecs = vec![
        RtpCodecCapability::Audio {
            mime_type: MimeTypeAudio::Opus,
            preferred_payload_type: None,
            clock_rate: NonZeroU32::new(48000).unwrap(),
            channels: NonZeroU8::new(2).unwrap(),
            parameters: RtpCodecParametersParameters::from([
                ("useinbandfec", 1_u32.into()),
                ("foo", "bar".into()),
            ]),
            rtcp_feedback: vec![],
        },
        RtpCodecCapability::Video {
            mime_type: MimeTypeVideo::H264,
            preferred_payload_type: None,
            clock_rate: NonZeroU32::new(90000).unwrap(),
            parameters: RtpCodecParametersParameters::from([
                ("level-asymmetry-allowed", 1_u32.into()),
                ("packetization-mode", 1_u32.into()),
                ("profile-level-id", "4d0032".into()),
                ("foo", "lalala".into()),
            ]),
            rtcp_feedback: vec![],
        },
    ];

    let router_rtp_capabilities = generate_router_rtp_capabilities(media_codecs)
        .expect("Failed to generate router RTP capabilities");

    let rtp_parameters = RtpParameters {
        mid: None,
        codecs: vec![
            RtpCodecParameters::Video {
                mime_type: MimeTypeVideo::H264,
                payload_type: 111,
                clock_rate: NonZeroU32::new(90000).unwrap(),
                parameters: RtpCodecParametersParameters::from([
                    ("foo", 1234u32.into()),
                    ("packetization-mode", 1_u32.into()),
                    ("profile-level-id", "4d0032".into()),
                ]),
                rtcp_feedback: vec![
                    RtcpFeedback::Nack,
                    RtcpFeedback::NackPli,
                    RtcpFeedback::GoogRemb,
                ],
            },
            RtpCodecParameters::Video {
                mime_type: MimeTypeVideo::Rtx,
                payload_type: 112,
                clock_rate: NonZeroU32::new(90000).unwrap(),
                parameters: RtpCodecParametersParameters::from([("apt", 111_u32.into())]),
                rtcp_feedback: vec![],
            },
        ],
        header_extensions: vec![
            RtpHeaderExtensionParameters {
                uri: RtpHeaderExtensionUri::Mid,
                id: 1,
                encrypt: false,
            },
            RtpHeaderExtensionParameters {
                uri: RtpHeaderExtensionUri::VideoOrientation,
                id: 2,
                encrypt: false,
            },
        ],
        encodings: vec![
            RtpEncodingParameters {
                ssrc: Some(11111111),
                rtx: Some(RtpEncodingParametersRtx { ssrc: 11111112 }),
                scalability_mode: ScalabilityMode::L1T3,
                max_bitrate: Some(111111),
                ..RtpEncodingParameters::default()
            },
            RtpEncodingParameters {
                ssrc: Some(21111111),
                rtx: Some(RtpEncodingParametersRtx { ssrc: 21111112 }),
                scalability_mode: ScalabilityMode::L1T3,
                max_bitrate: Some(222222),
                ..RtpEncodingParameters::default()
            },
            RtpEncodingParameters {
                rid: Some("high".to_string()),
                scalability_mode: ScalabilityMode::L1T3,
                max_bitrate: Some(333333),
                ..RtpEncodingParameters::default()
            },
        ],
        rtcp: RtcpParameters {
            cname: Some("qwerty1234".to_string()),
            ..RtcpParameters::default()
        },
    };

    let rtp_mapping =
        get_producer_rtp_parameters_mapping(&rtp_parameters, &router_rtp_capabilities)
            .expect("Failed to get producer RTP parameters mapping");

    assert_eq!(
        rtp_mapping.codecs,
        vec![
            RtpMappingCodec {
                payload_type: 111,
                mapped_payload_type: 101
            },
            RtpMappingCodec {
                payload_type: 112,
                mapped_payload_type: 102
            },
        ]
    );

    assert_eq!(rtp_mapping.encodings.first().unwrap().ssrc, Some(11111111));
    assert_eq!(rtp_mapping.encodings.first().unwrap().rid, None);
    assert_eq!(rtp_mapping.encodings.get(1).unwrap().ssrc, Some(21111111));
    assert_eq!(rtp_mapping.encodings.get(1).unwrap().rid, None);
    assert_eq!(rtp_mapping.encodings.get(2).unwrap().ssrc, None);
    assert_eq!(
        rtp_mapping.encodings.get(2).unwrap().rid,
        Some("high".to_string())
    );

    let consumable_rtp_parameters = get_consumable_rtp_parameters(
        MediaKind::Video,
        &rtp_parameters,
        &router_rtp_capabilities,
        &rtp_mapping,
    );

    assert_eq!(
        consumable_rtp_parameters.codecs,
        vec![
            RtpCodecParameters::Video {
                mime_type: MimeTypeVideo::H264,
                payload_type: 101,
                clock_rate: NonZeroU32::new(90000).unwrap(),
                parameters: RtpCodecParametersParameters::from([
                    ("foo", 1234u32.into()),
                    ("packetization-mode", 1_u32.into()),
                    ("profile-level-id", "4d0032".into()),
                ]),
                rtcp_feedback: vec![
                    RtcpFeedback::Nack,
                    RtcpFeedback::NackPli,
                    RtcpFeedback::CcmFir,
                    RtcpFeedback::GoogRemb,
                    RtcpFeedback::TransportCc,
                ],
            },
            RtpCodecParameters::Video {
                mime_type: MimeTypeVideo::Rtx,
                payload_type: 102,
                clock_rate: NonZeroU32::new(90000).unwrap(),
                parameters: RtpCodecParametersParameters::from([("apt", 101_u32.into())]),
                rtcp_feedback: vec![],
            },
        ]
    );

    assert_eq!(
        consumable_rtp_parameters.encodings.first().unwrap().ssrc,
        Some(rtp_mapping.encodings.first().unwrap().mapped_ssrc),
    );
    assert_eq!(
        consumable_rtp_parameters
            .encodings
            .first()
            .unwrap()
            .max_bitrate,
        Some(111111),
    );
    assert_eq!(
        consumable_rtp_parameters
            .encodings
            .first()
            .unwrap()
            .scalability_mode,
        ScalabilityMode::L1T3,
    );
    assert_eq!(
        consumable_rtp_parameters.encodings.get(1).unwrap().ssrc,
        Some(rtp_mapping.encodings.get(1).unwrap().mapped_ssrc),
    );
    assert_eq!(
        consumable_rtp_parameters
            .encodings
            .get(1)
            .unwrap()
            .max_bitrate,
        Some(222222),
    );
    assert_eq!(
        consumable_rtp_parameters
            .encodings
            .get(1)
            .unwrap()
            .scalability_mode,
        ScalabilityMode::L1T3,
    );
    assert_eq!(
        consumable_rtp_parameters.encodings.get(2).unwrap().ssrc,
        Some(rtp_mapping.encodings.get(2).unwrap().mapped_ssrc),
    );
    assert_eq!(
        consumable_rtp_parameters
            .encodings
            .get(2)
            .unwrap()
            .max_bitrate,
        Some(333333),
    );
    assert_eq!(
        consumable_rtp_parameters
            .encodings
            .get(2)
            .unwrap()
            .scalability_mode,
        ScalabilityMode::L1T3,
    );

    assert_eq!(
        consumable_rtp_parameters.rtcp,
        RtcpParameters {
            cname: rtp_parameters.rtcp.cname.clone(),
            reduced_size: true,
        }
    );

    let remote_rtp_capabilities = RtpCapabilities {
        codecs: vec![
            RtpCodecCapability::Audio {
                mime_type: MimeTypeAudio::Opus,
                preferred_payload_type: Some(100),
                clock_rate: NonZeroU32::new(48000).unwrap(),
                channels: NonZeroU8::new(2).unwrap(),
                parameters: RtpCodecParametersParameters::default(),
                rtcp_feedback: vec![],
            },
            RtpCodecCapability::Video {
                mime_type: MimeTypeVideo::H264,
                preferred_payload_type: Some(101),
                clock_rate: NonZeroU32::new(90000).unwrap(),
                parameters: RtpCodecParametersParameters::from([
                    ("packetization-mode", 1_u32.into()),
                    ("profile-level-id", "4d0032".into()),
                    ("baz", "LOLOLO".into()),
                ]),
                rtcp_feedback: vec![
                    RtcpFeedback::Nack,
                    RtcpFeedback::NackPli,
                    RtcpFeedback::Unsupported,
                ],
            },
            RtpCodecCapability::Video {
                mime_type: MimeTypeVideo::Rtx,
                preferred_payload_type: Some(102),
                clock_rate: NonZeroU32::new(90000).unwrap(),
                parameters: RtpCodecParametersParameters::from([("apt", 101_u32.into())]),
                rtcp_feedback: vec![],
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
                direction: RtpHeaderExtensionDirection::SendRecv,
            },
            RtpHeaderExtension {
                kind: MediaKind::Audio,
                uri: RtpHeaderExtensionUri::AudioLevel,
                preferred_id: 8,
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
        ],
    };

    let consumer_rtp_parameters = get_consumer_rtp_parameters(
        &consumable_rtp_parameters,
        &remote_rtp_capabilities,
        false,
        true,
    )
    .expect("Failed to get consumer RTP parameters");

    assert_eq!(
        consumer_rtp_parameters.codecs,
        vec![
            RtpCodecParameters::Video {
                mime_type: MimeTypeVideo::H264,
                payload_type: 101,
                clock_rate: NonZeroU32::new(90000).unwrap(),
                parameters: RtpCodecParametersParameters::from([
                    ("foo", 1234u32.into()),
                    ("packetization-mode", 1_u32.into()),
                    ("profile-level-id", "4d0032".into()),
                ]),
                rtcp_feedback: vec![
                    RtcpFeedback::Nack,
                    RtcpFeedback::NackPli,
                    RtcpFeedback::Unsupported,
                ],
            },
            RtpCodecParameters::Video {
                mime_type: MimeTypeVideo::Rtx,
                payload_type: 102,
                clock_rate: NonZeroU32::new(90000).unwrap(),
                parameters: RtpCodecParametersParameters::from([("apt", 101_u32.into())]),
                rtcp_feedback: vec![],
            },
        ]
    );

    assert_eq!(consumer_rtp_parameters.encodings.len(), 1);
    assert!(consumer_rtp_parameters
        .encodings
        .first()
        .unwrap()
        .ssrc
        .is_some());
    assert!(consumer_rtp_parameters
        .encodings
        .first()
        .unwrap()
        .rtx
        .is_some());
    assert_eq!(
        consumer_rtp_parameters
            .encodings
            .first()
            .unwrap()
            .scalability_mode,
        ScalabilityMode::L3T3,
    );
    assert_eq!(
        consumer_rtp_parameters
            .encodings
            .first()
            .unwrap()
            .max_bitrate,
        Some(333333),
    );

    assert_eq!(
        consumer_rtp_parameters.header_extensions,
        vec![
            RtpHeaderExtensionParameters {
                uri: RtpHeaderExtensionUri::Mid,
                id: 1,
                encrypt: false,
            },
            RtpHeaderExtensionParameters {
                uri: RtpHeaderExtensionUri::VideoOrientation,
                id: 11,
                encrypt: false,
            },
            RtpHeaderExtensionParameters {
                uri: RtpHeaderExtensionUri::TimeOffset,
                id: 12,
                encrypt: false,
            },
        ],
    );

    assert_eq!(
        consumer_rtp_parameters.rtcp,
        RtcpParameters {
            cname: rtp_parameters.rtcp.cname.clone(),
            reduced_size: true,
        },
    );

    let pipe_consumer_rtp_parameters =
        get_pipe_consumer_rtp_parameters(&consumable_rtp_parameters, false);

    assert_eq!(
        pipe_consumer_rtp_parameters.codecs,
        vec![RtpCodecParameters::Video {
            mime_type: MimeTypeVideo::H264,
            payload_type: 101,
            clock_rate: NonZeroU32::new(90000).unwrap(),
            parameters: RtpCodecParametersParameters::from([
                ("foo", 1234u32.into()),
                ("packetization-mode", 1_u32.into()),
                ("profile-level-id", "4d0032".into()),
            ]),
            rtcp_feedback: vec![RtcpFeedback::NackPli, RtcpFeedback::CcmFir],
        }],
    );

    assert_eq!(pipe_consumer_rtp_parameters.encodings.len(), 3);
    assert!(pipe_consumer_rtp_parameters
        .encodings
        .first()
        .unwrap()
        .ssrc
        .is_some());
    assert!(pipe_consumer_rtp_parameters
        .encodings
        .first()
        .unwrap()
        .rtx
        .is_none());
    assert!(pipe_consumer_rtp_parameters
        .encodings
        .first()
        .unwrap()
        .max_bitrate
        .is_some());
    assert_eq!(
        pipe_consumer_rtp_parameters
            .encodings
            .first()
            .unwrap()
            .scalability_mode,
        ScalabilityMode::L1T3,
    );
    assert!(pipe_consumer_rtp_parameters
        .encodings
        .get(1)
        .unwrap()
        .ssrc
        .is_some());
    assert!(pipe_consumer_rtp_parameters
        .encodings
        .get(1)
        .unwrap()
        .rtx
        .is_none());
    assert!(pipe_consumer_rtp_parameters
        .encodings
        .get(1)
        .unwrap()
        .max_bitrate
        .is_some());
    assert_eq!(
        pipe_consumer_rtp_parameters
            .encodings
            .get(1)
            .unwrap()
            .scalability_mode,
        ScalabilityMode::L1T3,
    );
    assert!(pipe_consumer_rtp_parameters
        .encodings
        .get(2)
        .unwrap()
        .ssrc
        .is_some());
    assert!(pipe_consumer_rtp_parameters
        .encodings
        .get(2)
        .unwrap()
        .rtx
        .is_none());
    assert!(pipe_consumer_rtp_parameters
        .encodings
        .get(2)
        .unwrap()
        .max_bitrate
        .is_some());
    assert_eq!(
        pipe_consumer_rtp_parameters
            .encodings
            .get(2)
            .unwrap()
            .scalability_mode,
        ScalabilityMode::L1T3,
    );

    assert_eq!(
        pipe_consumer_rtp_parameters.rtcp,
        RtcpParameters {
            cname: rtp_parameters.rtcp.cname,
            reduced_size: true,
        },
    );
}

#[test]
fn get_producer_rtp_parameters_mapping_unsupported() {
    let media_codecs = vec![
        RtpCodecCapability::Audio {
            mime_type: MimeTypeAudio::Opus,
            preferred_payload_type: None,
            clock_rate: NonZeroU32::new(48000).unwrap(),
            channels: NonZeroU8::new(2).unwrap(),
            parameters: RtpCodecParametersParameters::default(),
            rtcp_feedback: vec![],
        },
        RtpCodecCapability::Video {
            mime_type: MimeTypeVideo::H264,
            preferred_payload_type: None,
            clock_rate: NonZeroU32::new(90000).unwrap(),
            parameters: RtpCodecParametersParameters::from([
                ("packetization-mode", 1_u32.into()),
                ("profile-level-id", "640032".into()),
            ]),
            rtcp_feedback: vec![],
        },
    ];

    let router_rtp_capabilities = generate_router_rtp_capabilities(media_codecs)
        .expect("Failed to generate router RTP capabilities");

    let rtp_parameters = RtpParameters {
        mid: None,
        codecs: vec![RtpCodecParameters::Video {
            mime_type: MimeTypeVideo::Vp8,
            payload_type: 120,
            clock_rate: NonZeroU32::new(90000).unwrap(),
            parameters: RtpCodecParametersParameters::default(),
            rtcp_feedback: vec![RtcpFeedback::Nack, RtcpFeedback::Unsupported],
        }],
        header_extensions: vec![],
        encodings: vec![RtpEncodingParameters {
            ssrc: Some(11111111),
            ..RtpEncodingParameters::default()
        }],
        rtcp: RtcpParameters {
            cname: Some("qwerty1234".to_string()),
            ..RtcpParameters::default()
        },
    };

    assert!(matches!(
        get_producer_rtp_parameters_mapping(&rtp_parameters, &router_rtp_capabilities),
        Err(RtpParametersMappingError::UnsupportedCodec { .. }),
    ));
}
