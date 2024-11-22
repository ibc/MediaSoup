use futures_lite::future;
use mediasoup::data_structures::{ListenInfo, Protocol};
use mediasoup::prelude::*;
use mediasoup::producer::ProducerOptions;
use mediasoup::router::{Router, RouterOptions};
use mediasoup::rtp_parameters::{
    MediaKind, MimeTypeAudio, RtpCodecCapability, RtpCodecParameters, RtpCodecParametersParameters,
    RtpHeaderExtension, RtpHeaderExtensionDirection, RtpHeaderExtensionParameters,
    RtpHeaderExtensionUri, RtpParameters,
};
use mediasoup::webrtc_transport::{
    WebRtcTransport, WebRtcTransportListenInfos, WebRtcTransportOptions,
};
use mediasoup::worker::WorkerSettings;
use mediasoup::worker_manager::WorkerManager;
use std::env;
use std::net::{IpAddr, Ipv4Addr};
use std::num::{NonZeroU32, NonZeroU8};

fn media_codecs() -> Vec<RtpCodecCapability> {
    vec![RtpCodecCapability::Audio {
        mime_type: MimeTypeAudio::MultiChannelOpus,
        preferred_payload_type: None,
        clock_rate: NonZeroU32::new(48000).unwrap(),
        channels: NonZeroU8::new(6).unwrap(),
        parameters: RtpCodecParametersParameters::from([
            ("useinbandfec", 1_u32.into()),
            ("channel_mapping", "0,4,1,2,3,5".into()),
            ("num_streams", 4_u32.into()),
            ("coupled_streams", 2_u32.into()),
        ]),
        rtcp_feedback: vec![],
    }]
}

fn audio_producer_options() -> ProducerOptions {
    ProducerOptions::new(
        MediaKind::Audio,
        RtpParameters {
            mid: Some("AUDIO".to_string()),
            codecs: vec![RtpCodecParameters::Audio {
                mime_type: MimeTypeAudio::MultiChannelOpus,
                payload_type: 0,
                clock_rate: NonZeroU32::new(48000).unwrap(),
                channels: NonZeroU8::new(6).unwrap(),
                parameters: RtpCodecParametersParameters::from([
                    ("useinbandfec", 1_u32.into()),
                    ("channel_mapping", "0,4,1,2,3,5".into()),
                    ("num_streams", 4_u32.into()),
                    ("coupled_streams", 2_u32.into()),
                ]),
                rtcp_feedback: vec![],
            }],
            header_extensions: vec![
                RtpHeaderExtensionParameters {
                    uri: RtpHeaderExtensionUri::Mid,
                    id: 10,
                    encrypt: false,
                },
                RtpHeaderExtensionParameters {
                    uri: RtpHeaderExtensionUri::AudioLevel,
                    id: 12,
                    encrypt: false,
                },
            ],
            ..RtpParameters::default()
        },
    )
}

fn consumer_device_capabilities() -> RtpCapabilities {
    RtpCapabilities {
        codecs: vec![RtpCodecCapability::Audio {
            mime_type: MimeTypeAudio::MultiChannelOpus,
            preferred_payload_type: Some(100),
            clock_rate: NonZeroU32::new(48000).unwrap(),
            channels: NonZeroU8::new(6).unwrap(),
            parameters: RtpCodecParametersParameters::from([
                ("channel_mapping", "0,4,1,2,3,5".into()),
                ("num_streams", 4_u32.into()),
                ("coupled_streams", 2_u32.into()),
            ]),
            rtcp_feedback: vec![],
        }],
        header_extensions: vec![
            RtpHeaderExtension {
                kind: MediaKind::Audio,
                uri: RtpHeaderExtensionUri::Mid,
                preferred_id: 1,
                preferred_encrypt: false,
                direction: RtpHeaderExtensionDirection::default(),
            },
            RtpHeaderExtension {
                kind: MediaKind::Audio,
                uri: RtpHeaderExtensionUri::AbsSendTime,
                preferred_id: 4,
                preferred_encrypt: false,
                direction: RtpHeaderExtensionDirection::default(),
            },
            RtpHeaderExtension {
                kind: MediaKind::Audio,
                uri: RtpHeaderExtensionUri::AudioLevel,
                preferred_id: 10,
                preferred_encrypt: false,
                direction: RtpHeaderExtensionDirection::default(),
            },
        ],
    }
}

async fn init() -> (Router, WebRtcTransport) {
    {
        let mut builder = env_logger::builder();
        if env::var(env_logger::DEFAULT_FILTER_ENV).is_err() {
            builder.filter_level(log::LevelFilter::Off);
        }
        let _ = builder.is_test(true).try_init();
    }

    let worker_manager = WorkerManager::new();

    let worker = worker_manager
        .create_worker(WorkerSettings::default())
        .await
        .expect("Failed to create worker");

    let router = worker
        .create_router(RouterOptions::new(media_codecs()))
        .await
        .expect("Failed to create router");

    let transport_options =
        WebRtcTransportOptions::new(WebRtcTransportListenInfos::new(ListenInfo {
            protocol: Protocol::Udp,
            ip: IpAddr::V4(Ipv4Addr::LOCALHOST),
            announced_address: None,
            port: None,
            port_range: None,
            flags: None,
            send_buffer_size: None,
            recv_buffer_size: None,
        }));

    let transport = router
        .create_webrtc_transport(transport_options.clone())
        .await
        .expect("Failed to create transport");

    (router, transport)
}

#[test]
fn produce_and_consume_succeeds() {
    future::block_on(async move {
        let (router, transport) = init().await;

        let audio_producer = transport
            .produce(audio_producer_options())
            .await
            .expect("Failed to produce audio");

        assert_eq!(
            audio_producer.rtp_parameters().codecs,
            vec![RtpCodecParameters::Audio {
                mime_type: MimeTypeAudio::MultiChannelOpus,
                payload_type: 0,
                clock_rate: NonZeroU32::new(48000).unwrap(),
                channels: NonZeroU8::new(6).unwrap(),
                parameters: RtpCodecParametersParameters::from([
                    ("useinbandfec", 1_u32.into()),
                    ("channel_mapping", "0,4,1,2,3,5".into()),
                    ("num_streams", 4_u32.into()),
                    ("coupled_streams", 2_u32.into()),
                ]),
                rtcp_feedback: vec![],
            }]
        );

        let consumer_device_capabilities = consumer_device_capabilities();

        assert!(router.can_consume(&audio_producer.id(), &consumer_device_capabilities));

        let audio_consumer = transport
            .consume(ConsumerOptions::new(
                audio_producer.id(),
                consumer_device_capabilities.clone(),
            ))
            .await
            .expect("Failed to consume audio");

        assert_eq!(
            audio_consumer.rtp_parameters().codecs,
            vec![RtpCodecParameters::Audio {
                mime_type: MimeTypeAudio::MultiChannelOpus,
                payload_type: 100,
                clock_rate: NonZeroU32::new(48000).unwrap(),
                channels: NonZeroU8::new(6).unwrap(),
                parameters: RtpCodecParametersParameters::from([
                    ("useinbandfec", 1_u32.into()),
                    ("channel_mapping", "0,4,1,2,3,5".into()),
                    ("num_streams", 4_u32.into()),
                    ("coupled_streams", 2_u32.into()),
                ]),
                rtcp_feedback: vec![],
            }]
        );
    });
}

#[test]
fn fails_to_consume_wrong_parameters() {
    future::block_on(async move {
        let (_router, transport) = init().await;

        assert!(transport
            .produce(ProducerOptions::new(
                MediaKind::Audio,
                RtpParameters {
                    mid: Some("AUDIO".to_string()),
                    codecs: vec![RtpCodecParameters::Audio {
                        mime_type: MimeTypeAudio::MultiChannelOpus,
                        payload_type: 0,
                        clock_rate: NonZeroU32::new(48000).unwrap(),
                        channels: NonZeroU8::new(6).unwrap(),
                        parameters: RtpCodecParametersParameters::from([
                            ("channel_mapping", "0,4,1,2,3,5".into()),
                            ("num_streams", 2_u32.into()),
                            ("coupled_streams", 2_u32.into()),
                        ]),
                        rtcp_feedback: vec![],
                    }],
                    ..RtpParameters::default()
                },
            ))
            .await
            .is_err());

        assert!(transport
            .produce(ProducerOptions::new(
                MediaKind::Audio,
                RtpParameters {
                    mid: Some("AUDIO".to_string()),
                    codecs: vec![RtpCodecParameters::Audio {
                        mime_type: MimeTypeAudio::MultiChannelOpus,
                        payload_type: 0,
                        clock_rate: NonZeroU32::new(48000).unwrap(),
                        channels: NonZeroU8::new(6).unwrap(),
                        parameters: RtpCodecParametersParameters::from([
                            ("channel_mapping", "0,4,1,2,3,5".into()),
                            ("num_streams", 4_u32.into()),
                            ("coupled_streams", 1_u32.into()),
                        ]),
                        rtcp_feedback: vec![],
                    }],
                    ..RtpParameters::default()
                },
            ))
            .await
            .is_err());
    });
}

#[test]
fn fails_to_consume_wrong_channels() {
    future::block_on(async move {
        let (router, transport) = init().await;

        let audio_producer = transport
            .produce(audio_producer_options())
            .await
            .expect("Failed to produce audio");

        let consumer_device_capabilities = RtpCapabilities {
            codecs: vec![RtpCodecCapability::Audio {
                mime_type: MimeTypeAudio::MultiChannelOpus,
                preferred_payload_type: Some(100),
                clock_rate: NonZeroU32::new(48000).unwrap(),
                channels: NonZeroU8::new(8).unwrap(),
                parameters: RtpCodecParametersParameters::default(),
                rtcp_feedback: vec![],
            }],
            header_extensions: Vec::new(),
        };

        assert!(!router.can_consume(&audio_producer.id(), &consumer_device_capabilities));

        let audio_consumer_result = transport
            .consume(ConsumerOptions::new(
                audio_producer.id(),
                consumer_device_capabilities.clone(),
            ))
            .await;

        assert!(audio_consumer_result.is_err());
    });
}
