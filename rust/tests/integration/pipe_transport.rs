use futures_lite::future;
use mediasoup::consumer::{ConsumerOptions, ConsumerScore, ConsumerType};
use mediasoup::data_consumer::{DataConsumerOptions, DataConsumerType};
use mediasoup::data_producer::{DataProducerOptions, DataProducerType};
use mediasoup::data_structures::{AppData, ListenInfo, Protocol};
use mediasoup::pipe_transport::{PipeTransportOptions, PipeTransportRemoteParameters};
use mediasoup::prelude::*;
use mediasoup::producer::ProducerOptions;
use mediasoup::router::{
    PipeDataProducerToRouterPair, PipeProducerToRouterPair, PipeToRouterOptions, Router,
    RouterOptions,
};
use mediasoup::rtp_parameters::{
    MediaKind, MimeTypeAudio, MimeTypeVideo, RtcpFeedback, RtcpParameters, RtpCapabilities,
    RtpCodecCapability, RtpCodecParameters, RtpCodecParametersParameters, RtpEncodingParameters,
    RtpHeaderExtension, RtpHeaderExtensionDirection, RtpHeaderExtensionParameters,
    RtpHeaderExtensionUri, RtpParameters,
};
use mediasoup::sctp_parameters::SctpStreamParameters;
use mediasoup::srtp_parameters::{SrtpCryptoSuite, SrtpParameters};
use mediasoup::transport::ProduceError;
use mediasoup::webrtc_transport::{
    WebRtcTransport, WebRtcTransportListenInfos, WebRtcTransportOptions,
};
use mediasoup::worker::{RequestError, Worker, WorkerSettings};
use mediasoup::worker_manager::WorkerManager;
use parking_lot::Mutex;
use portpicker::pick_unused_port;
use std::env;
use std::net::{IpAddr, Ipv4Addr};
use std::num::{NonZeroU32, NonZeroU8};

struct CustomAppData {
    _foo: &'static str,
}

fn media_codecs() -> Vec<RtpCodecCapability> {
    vec![
        RtpCodecCapability::Audio {
            mime_type: MimeTypeAudio::Opus,
            preferred_payload_type: None,
            clock_rate: NonZeroU32::new(48000).unwrap(),
            channels: NonZeroU8::new(2).unwrap(),
            parameters: RtpCodecParametersParameters::default(),
            rtcp_feedback: vec![],
        },
        RtpCodecCapability::Video {
            mime_type: MimeTypeVideo::Vp8,
            preferred_payload_type: None,
            clock_rate: NonZeroU32::new(90000).unwrap(),
            parameters: RtpCodecParametersParameters::default(),
            rtcp_feedback: vec![],
        },
    ]
}

fn audio_producer_options() -> ProducerOptions {
    let mut options = ProducerOptions::new(
        MediaKind::Audio,
        RtpParameters {
            mid: Some("AUDIO".to_string()),
            codecs: vec![RtpCodecParameters::Audio {
                mime_type: MimeTypeAudio::Opus,
                payload_type: 111,
                clock_rate: NonZeroU32::new(48000).unwrap(),
                channels: NonZeroU8::new(2).unwrap(),
                parameters: RtpCodecParametersParameters::from([
                    ("useinbandfec", 1_u32.into()),
                    ("foo", "bar1".into()),
                ]),
                rtcp_feedback: vec![],
            }],
            header_extensions: vec![RtpHeaderExtensionParameters {
                uri: RtpHeaderExtensionUri::Mid,
                id: 10,
                encrypt: false,
            }],
            encodings: vec![RtpEncodingParameters {
                ssrc: Some(11111111),
                ..RtpEncodingParameters::default()
            }],
            rtcp: RtcpParameters {
                cname: Some("FOOBAR".to_string()),
                ..RtcpParameters::default()
            },
        },
    );

    options.app_data = AppData::new(CustomAppData { _foo: "bar1" });

    options
}

fn video_producer_options() -> ProducerOptions {
    let mut options = ProducerOptions::new(
        MediaKind::Video,
        RtpParameters {
            mid: Some("VIDEO".to_string()),
            codecs: vec![RtpCodecParameters::Video {
                mime_type: MimeTypeVideo::Vp8,
                payload_type: 112,
                clock_rate: NonZeroU32::new(90000).unwrap(),
                parameters: RtpCodecParametersParameters::default(),
                rtcp_feedback: vec![
                    RtcpFeedback::Nack,
                    RtcpFeedback::NackPli,
                    RtcpFeedback::GoogRemb,
                ],
            }],
            header_extensions: vec![
                RtpHeaderExtensionParameters {
                    uri: RtpHeaderExtensionUri::Mid,
                    id: 10,
                    encrypt: false,
                },
                RtpHeaderExtensionParameters {
                    uri: RtpHeaderExtensionUri::AbsSendTime,
                    id: 11,
                    encrypt: false,
                },
                RtpHeaderExtensionParameters {
                    uri: RtpHeaderExtensionUri::VideoOrientation,
                    id: 13,
                    encrypt: false,
                },
            ],
            encodings: vec![
                RtpEncodingParameters {
                    ssrc: Some(22222222),
                    ..RtpEncodingParameters::default()
                },
                RtpEncodingParameters {
                    ssrc: Some(22222223),
                    ..RtpEncodingParameters::default()
                },
                RtpEncodingParameters {
                    ssrc: Some(22222224),
                    ..RtpEncodingParameters::default()
                },
            ],
            rtcp: RtcpParameters {
                cname: Some("FOOBAR".to_string()),
                ..RtcpParameters::default()
            },
        },
    );

    options.app_data = AppData::new(CustomAppData { _foo: "bar2" });

    options
}

fn data_producer_options() -> DataProducerOptions {
    let mut options = DataProducerOptions::new_sctp(
        SctpStreamParameters::new_unordered_with_life_time(666, 5000),
    );

    options.label = "foo".to_string();
    options.protocol = "bar".to_string();

    options
}

fn consumer_device_capabilities() -> RtpCapabilities {
    RtpCapabilities {
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
                mime_type: MimeTypeVideo::Vp8,
                preferred_payload_type: Some(101),
                clock_rate: NonZeroU32::new(90000).unwrap(),
                parameters: RtpCodecParametersParameters::default(),
                rtcp_feedback: vec![
                    RtcpFeedback::Nack,
                    RtcpFeedback::CcmFir,
                    RtcpFeedback::TransportCc,
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
                kind: MediaKind::Video,
                uri: RtpHeaderExtensionUri::AbsSendTime,
                preferred_id: 4,
                preferred_encrypt: false,
                direction: RtpHeaderExtensionDirection::SendRecv,
            },
            RtpHeaderExtension {
                kind: MediaKind::Video,
                uri: RtpHeaderExtensionUri::TransportWideCcDraft01,
                preferred_id: 5,
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

async fn init() -> (
    Worker,
    Worker,
    Router,
    Router,
    WebRtcTransport,
    WebRtcTransport,
) {
    {
        let mut builder = env_logger::builder();
        if env::var(env_logger::DEFAULT_FILTER_ENV).is_err() {
            builder.filter_level(log::LevelFilter::Off);
        }
        let _ = builder.is_test(true).try_init();
    }

    let worker_manager = WorkerManager::new();

    let worker1 = worker_manager
        .create_worker(WorkerSettings::default())
        .await
        .expect("Failed to create worker");

    let worker2 = worker_manager
        .create_worker(WorkerSettings::default())
        .await
        .expect("Failed to create worker");

    let router1 = worker1
        .create_router(RouterOptions::new(media_codecs()))
        .await
        .expect("Failed to create router");

    let router2 = worker2
        .create_router(RouterOptions::new(media_codecs()))
        .await
        .expect("Failed to create router");

    let mut transport_options =
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
    transport_options.enable_sctp = true;

    let transport_1 = router1
        .create_webrtc_transport(transport_options.clone())
        .await
        .expect("Failed to create transport1");

    let transport_2 = router2
        .create_webrtc_transport(transport_options)
        .await
        .expect("Failed to create transport2");

    (worker1, worker2, router1, router2, transport_1, transport_2)
}

#[test]
fn pipe_to_router_succeeds_with_audio() {
    future::block_on(async move {
        let (_worker1, _worker2, router1, router2, transport1, _transport2) = init().await;

        let audio_producer = transport1
            .produce(audio_producer_options())
            .await
            .expect("Failed to produce audio");

        let PipeProducerToRouterPair {
            pipe_consumer,
            pipe_producer,
        } = router1
            .pipe_producer_to_router(
                audio_producer.id(),
                PipeToRouterOptions::new(router2.clone()),
            )
            .await
            .expect("Failed to pipe audio producer to router");

        let pipe_producer = pipe_producer.into_inner();

        {
            let dump = router1.dump().await.expect("Failed to dump router");

            // There should be two Transports in router1:
            // - WebRtcTransport for audio_producer.
            // - PipeTransport between router1 and router2.
            assert_eq!(dump.transport_ids.len(), 2);
        }

        {
            let dump = router2.dump().await.expect("Failed to dump router");

            // There should be two Transports in router2:
            // - WebRtcTransport for audio_consumer.
            // - pipeTransport between router2 and router1.
            assert_eq!(dump.transport_ids.len(), 2);
        }

        assert_eq!(pipe_consumer.kind(), MediaKind::Audio);
        assert_eq!(pipe_consumer.rtp_parameters().mid, None);
        assert_eq!(
            pipe_consumer.rtp_parameters().codecs,
            vec![RtpCodecParameters::Audio {
                mime_type: MimeTypeAudio::Opus,
                payload_type: 100,
                clock_rate: NonZeroU32::new(48000).unwrap(),
                channels: NonZeroU8::new(2).unwrap(),
                parameters: RtpCodecParametersParameters::from([
                    ("useinbandfec", 1_u32.into()),
                    ("foo", "bar1".into()),
                ]),
                rtcp_feedback: vec![],
            }],
        );
        assert_eq!(
            pipe_consumer.rtp_parameters().header_extensions,
            vec![
                RtpHeaderExtensionParameters {
                    uri: RtpHeaderExtensionUri::AudioLevel,
                    id: 10,
                    encrypt: false,
                },
                RtpHeaderExtensionParameters {
                    uri: RtpHeaderExtensionUri::AbsCaptureTime,
                    id: 13,
                    encrypt: false,
                },
                RtpHeaderExtensionParameters {
                    uri: RtpHeaderExtensionUri::PlayoutDelay,
                    id: 14,
                    encrypt: false,
                }
            ],
        );
        assert_eq!(pipe_consumer.r#type(), ConsumerType::Pipe);
        assert!(!pipe_consumer.paused());
        assert!(!pipe_consumer.producer_paused());
        assert_eq!(
            pipe_consumer.score(),
            ConsumerScore {
                score: 10,
                producer_score: 10,
                producer_scores: vec![0],
            },
        );
        assert_eq!(pipe_consumer.app_data().downcast_ref::<()>().unwrap(), &());

        assert_eq!(pipe_producer.id(), audio_producer.id());
        assert_eq!(pipe_producer.kind(), MediaKind::Audio);
        assert_eq!(pipe_producer.rtp_parameters().mid, None);
        assert_eq!(
            pipe_producer.rtp_parameters().codecs,
            vec![RtpCodecParameters::Audio {
                mime_type: MimeTypeAudio::Opus,
                payload_type: 100,
                clock_rate: NonZeroU32::new(48000).unwrap(),
                channels: NonZeroU8::new(2).unwrap(),
                parameters: RtpCodecParametersParameters::from([
                    ("useinbandfec", 1_u32.into()),
                    ("foo", "bar1".into()),
                ]),
                rtcp_feedback: vec![],
            }],
        );
        assert_eq!(
            pipe_producer.rtp_parameters().header_extensions,
            vec![
                RtpHeaderExtensionParameters {
                    uri: RtpHeaderExtensionUri::AudioLevel,
                    id: 10,
                    encrypt: false,
                },
                RtpHeaderExtensionParameters {
                    uri: RtpHeaderExtensionUri::AbsCaptureTime,
                    id: 13,
                    encrypt: false,
                },
                RtpHeaderExtensionParameters {
                    uri: RtpHeaderExtensionUri::PlayoutDelay,
                    id: 14,
                    encrypt: false,
                },
            ],
        );
        assert!(!pipe_producer.paused());
    });
}

#[test]
fn pipe_to_router_succeeds_with_video() {
    future::block_on(async move {
        let (_worker1, _worker2, router1, router2, transport1, _transport2) = init().await;

        let audio_producer = transport1
            .produce(audio_producer_options())
            .await
            .expect("Failed to produce audio");

        router1
            .pipe_producer_to_router(
                audio_producer.id(),
                PipeToRouterOptions::new(router2.clone()),
            )
            .await
            .expect("Failed to pipe audio producer to router");

        let video_producer = transport1
            .produce(video_producer_options())
            .await
            .expect("Failed to produce video");

        // Pause the videoProducer.
        video_producer
            .pause()
            .await
            .expect("Failed to pause video producer");

        let PipeProducerToRouterPair {
            pipe_consumer,
            pipe_producer,
        } = router1
            .pipe_producer_to_router(
                video_producer.id(),
                PipeToRouterOptions::new(router2.clone()),
            )
            .await
            .expect("Failed to pipe video producer to router");

        let pipe_producer = pipe_producer.into_inner();

        {
            let dump = router1.dump().await.expect("Failed to dump router");

            // No new PipeTransport should has been created. The existing one is used.
            assert_eq!(dump.transport_ids.len(), 2);
        }

        {
            let dump = router2.dump().await.expect("Failed to dump router");

            // No new PipeTransport should has been created. The existing one is used.
            assert_eq!(dump.transport_ids.len(), 2);
        }

        assert_eq!(pipe_consumer.kind(), MediaKind::Video);
        assert_eq!(pipe_consumer.rtp_parameters().mid, None);
        assert_eq!(
            pipe_consumer.rtp_parameters().codecs,
            vec![RtpCodecParameters::Video {
                mime_type: MimeTypeVideo::Vp8,
                payload_type: 101,
                clock_rate: NonZeroU32::new(90000).unwrap(),
                parameters: RtpCodecParametersParameters::default(),
                rtcp_feedback: vec![RtcpFeedback::NackPli, RtcpFeedback::CcmFir],
            }],
        );
        assert_eq!(
            pipe_consumer.rtp_parameters().header_extensions,
            vec![
                // NOTE: Remove this once framemarking draft becomes RFC.
                RtpHeaderExtensionParameters {
                    uri: RtpHeaderExtensionUri::FrameMarkingDraft07,
                    id: 6,
                    encrypt: false,
                },
                RtpHeaderExtensionParameters {
                    uri: RtpHeaderExtensionUri::FrameMarking,
                    id: 7,
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
                RtpHeaderExtensionParameters {
                    uri: RtpHeaderExtensionUri::AbsCaptureTime,
                    id: 13,
                    encrypt: false,
                },
                RtpHeaderExtensionParameters {
                    uri: RtpHeaderExtensionUri::PlayoutDelay,
                    id: 14,
                    encrypt: false,
                },
            ],
        );
        assert_eq!(pipe_consumer.r#type(), ConsumerType::Pipe);
        assert!(!pipe_consumer.paused());
        assert!(pipe_consumer.producer_paused());
        assert_eq!(
            pipe_consumer.score(),
            ConsumerScore {
                score: 10,
                producer_score: 10,
                producer_scores: vec![0, 0, 0],
            },
        );
        assert_eq!(pipe_consumer.app_data().downcast_ref::<()>().unwrap(), &());

        assert_eq!(pipe_producer.id(), video_producer.id());
        assert_eq!(pipe_producer.kind(), MediaKind::Video);
        assert_eq!(pipe_producer.rtp_parameters().mid, None);
        assert_eq!(
            pipe_consumer.rtp_parameters().codecs,
            vec![RtpCodecParameters::Video {
                mime_type: MimeTypeVideo::Vp8,
                payload_type: 101,
                clock_rate: NonZeroU32::new(90000).unwrap(),
                parameters: RtpCodecParametersParameters::default(),
                rtcp_feedback: vec![RtcpFeedback::NackPli, RtcpFeedback::CcmFir],
            }],
        );
        assert_eq!(
            pipe_consumer.rtp_parameters().header_extensions,
            vec![
                // NOTE: Remove this once framemarking draft becomes RFC.
                RtpHeaderExtensionParameters {
                    uri: RtpHeaderExtensionUri::FrameMarkingDraft07,
                    id: 6,
                    encrypt: false,
                },
                RtpHeaderExtensionParameters {
                    uri: RtpHeaderExtensionUri::FrameMarking,
                    id: 7,
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
                RtpHeaderExtensionParameters {
                    uri: RtpHeaderExtensionUri::AbsCaptureTime,
                    id: 13,
                    encrypt: false,
                },
                RtpHeaderExtensionParameters {
                    uri: RtpHeaderExtensionUri::PlayoutDelay,
                    id: 14,
                    encrypt: false,
                },
            ],
        );
        assert!(pipe_producer.paused());
    });
}

#[test]
fn pipe_to_router_fails_if_both_routers_belong_to_the_same_worker() {
    future::block_on(async move {
        let (worker1, _worker2, router1, _router2, transport1, _transport2) = init().await;

        let router1bis = worker1
            .create_router(RouterOptions::new(media_codecs()))
            .await
            .expect("Failed to create router");

        let video_producer = transport1
            .produce(video_producer_options())
            .await
            .expect("Failed to produce video");

        let result = router1
            .pipe_producer_to_router(
                video_producer.id(),
                PipeToRouterOptions::new(router1bis.clone()),
            )
            .await;

        if let Err(PipeProducerToRouterError::ProduceFailed(ProduceError::Request(
            RequestError::Response { reason },
        ))) = result
        {
            assert!(reason.contains("already exists [method:transport.produce]"));
        } else {
            panic!("Unexpected result: {result:?}");
        }
    });
}

#[test]
fn weak() {
    future::block_on(async move {
        let (_worker1, _worker2, router1, _router2, _transport1, _transport2) = init().await;

        let pipe_transport = router1
            .create_pipe_transport({
                let mut options = PipeTransportOptions::new(ListenInfo {
                    protocol: Protocol::Udp,
                    ip: IpAddr::V4(Ipv4Addr::LOCALHOST),
                    announced_address: None,
                    port: None,
                    port_range: None,
                    flags: None,
                    send_buffer_size: None,
                    recv_buffer_size: None,
                });
                options.enable_rtx = true;

                options
            })
            .await
            .expect("Failed to create Pipe transport");

        let weak_pipe_transport = pipe_transport.downgrade();

        assert!(weak_pipe_transport.upgrade().is_some());

        drop(pipe_transport);

        assert!(weak_pipe_transport.upgrade().is_none());
    });
}

#[test]
fn create_with_fixed_port_succeeds() {
    future::block_on(async move {
        let (_worker1, _worker2, router1, _router2, _transport1, _transport2) = init().await;

        let port = pick_unused_port().unwrap();

        let pipe_transport = router1
            .create_pipe_transport({
                PipeTransportOptions::new(ListenInfo {
                    protocol: Protocol::Udp,
                    ip: IpAddr::V4(Ipv4Addr::LOCALHOST),
                    announced_address: None,
                    port: Some(port),
                    port_range: None,
                    flags: None,
                    send_buffer_size: None,
                    recv_buffer_size: None,
                })
            })
            .await
            .expect("Failed to create Pipe transport");

        assert_eq!(pipe_transport.tuple().local_port(), port);
    });
}

#[test]
fn create_with_enable_rtx_succeeds() {
    future::block_on(async move {
        let (_worker1, _worker2, router1, _router2, transport1, _transport2) = init().await;

        let pipe_transport = router1
            .create_pipe_transport({
                let mut options = PipeTransportOptions::new(ListenInfo {
                    protocol: Protocol::Udp,
                    ip: IpAddr::V4(Ipv4Addr::LOCALHOST),
                    announced_address: None,
                    port: None,
                    port_range: None,
                    flags: None,
                    send_buffer_size: None,
                    recv_buffer_size: None,
                });
                options.enable_rtx = true;

                options
            })
            .await
            .expect("Failed to create Pipe transport");

        let video_producer = transport1
            .produce(video_producer_options())
            .await
            .expect("Failed to produce video");

        // Pause the videoProducer.
        video_producer
            .pause()
            .await
            .expect("Failed to pause video producer");

        let pipe_consumer = pipe_transport
            .consume(ConsumerOptions::new(
                video_producer.id(),
                RtpCapabilities::default(),
            ))
            .await
            .expect("Failed to create pipe consumer");

        assert_eq!(pipe_consumer.kind(), MediaKind::Video);
        assert_eq!(pipe_consumer.rtp_parameters().mid, None);
        assert_eq!(
            pipe_consumer.rtp_parameters().codecs,
            vec![
                RtpCodecParameters::Video {
                    mime_type: MimeTypeVideo::Vp8,
                    payload_type: 101,
                    clock_rate: NonZeroU32::new(90000).unwrap(),
                    parameters: RtpCodecParametersParameters::default(),
                    rtcp_feedback: vec![
                        RtcpFeedback::Nack,
                        RtcpFeedback::NackPli,
                        RtcpFeedback::CcmFir
                    ],
                },
                RtpCodecParameters::Video {
                    mime_type: MimeTypeVideo::Rtx,
                    payload_type: 102,
                    clock_rate: NonZeroU32::new(90000).unwrap(),
                    parameters: RtpCodecParametersParameters::from([("apt", 101_u32.into())]),
                    rtcp_feedback: vec![],
                },
            ],
        );
        assert_eq!(
            pipe_consumer.rtp_parameters().header_extensions,
            vec![
                // NOTE: Remove this once framemarking draft becomes RFC.
                RtpHeaderExtensionParameters {
                    uri: RtpHeaderExtensionUri::FrameMarkingDraft07,
                    id: 6,
                    encrypt: false,
                },
                RtpHeaderExtensionParameters {
                    uri: RtpHeaderExtensionUri::FrameMarking,
                    id: 7,
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
                RtpHeaderExtensionParameters {
                    uri: RtpHeaderExtensionUri::AbsCaptureTime,
                    id: 13,
                    encrypt: false,
                },
                RtpHeaderExtensionParameters {
                    uri: RtpHeaderExtensionUri::PlayoutDelay,
                    id: 14,
                    encrypt: false,
                },
            ],
        );
        assert_eq!(pipe_consumer.r#type(), ConsumerType::Pipe);
        assert!(!pipe_consumer.paused());
        assert!(pipe_consumer.producer_paused());
        assert_eq!(
            pipe_consumer.score(),
            ConsumerScore {
                score: 10,
                producer_score: 10,
                producer_scores: vec![0, 0, 0],
            },
        );
        assert_eq!(pipe_consumer.app_data().downcast_ref::<()>().unwrap(), &());
    });
}

#[test]
fn create_with_enable_srtp_succeeds() {
    future::block_on(async move {
        let (_worker1, _worker2, router1, _router2, _transport1, _transport2) = init().await;

        let pipe_transport = router1
            .create_pipe_transport({
                let mut options = PipeTransportOptions::new(ListenInfo {
                    protocol: Protocol::Udp,
                    ip: IpAddr::V4(Ipv4Addr::LOCALHOST),
                    announced_address: None,
                    port: None,
                    port_range: None,
                    flags: None,
                    send_buffer_size: None,
                    recv_buffer_size: None,
                });
                options.enable_srtp = true;

                options
            })
            .await
            .expect("Failed to create Pipe transport");

        assert!(pipe_transport.srtp_parameters().is_some());
        assert_eq!(
            pipe_transport.srtp_parameters().unwrap().key_base64.len(),
            60
        );

        // Missing srtp_parameters.
        assert!(matches!(
            pipe_transport
                .connect(PipeTransportRemoteParameters {
                    ip: "127.0.0.2".parse().unwrap(),
                    port: 9999,
                    srtp_parameters: None,
                })
                .await,
            Err(RequestError::Response { .. }),
        ));

        // Valid srtp_parameters.
        pipe_transport
            .connect(PipeTransportRemoteParameters {
                ip: "127.0.0.2".parse().unwrap(),
                port: 9999,
                srtp_parameters: Some(SrtpParameters {
                    crypto_suite: SrtpCryptoSuite::AeadAes256Gcm,
                    key_base64: "YTdjcDBvY2JoMGY5YXNlNDc0eDJsdGgwaWRvNnJsamRrdG16aWVpZHphdHo="
                        .to_string(),
                }),
            })
            .await
            .expect("Failed to establish Pipe transport connection");
    });
}

#[test]
fn create_with_invalid_srtp_parameters_fails() {
    future::block_on(async move {
        let (_worker1, _worker2, router1, _router2, _transport1, _transport2) = init().await;

        let pipe_transport = router1
            .create_pipe_transport(PipeTransportOptions::new(ListenInfo {
                protocol: Protocol::Udp,
                ip: IpAddr::V4(Ipv4Addr::LOCALHOST),
                announced_address: None,
                port: None,
                port_range: None,
                flags: None,
                send_buffer_size: None,
                recv_buffer_size: None,
            }))
            .await
            .expect("Failed to create Pipe transport");

        // No SRTP enabled so passing srtp_parameters must fail.
        assert!(matches!(
            pipe_transport
                .connect(PipeTransportRemoteParameters {
                    ip: "127.0.0.2".parse().unwrap(),
                    port: 9999,
                    srtp_parameters: Some(SrtpParameters {
                        crypto_suite: SrtpCryptoSuite::AeadAes256Gcm,
                        key_base64: "YTdjcDBvY2JoMGY5YXNlNDc0eDJsdGgwaWRvNnJsamRrdG16aWVpZHphdHo="
                            .to_string(),
                    }),
                })
                .await,
            Err(RequestError::Response { .. }),
        ));
    });
}

#[test]
fn consume_for_pipe_producer_succeeds() {
    future::block_on(async move {
        let (_worker1, _worker2, router1, router2, transport1, transport2) = init().await;

        let video_producer = transport1
            .produce(video_producer_options())
            .await
            .expect("Failed to produce video");

        // Pause the videoProducer.
        video_producer
            .pause()
            .await
            .expect("Failed to pause video producer");

        router1
            .pipe_producer_to_router(
                video_producer.id(),
                PipeToRouterOptions::new(router2.clone()),
            )
            .await
            .expect("Failed to pipe video producer to router");

        let video_consumer = transport2
            .consume(ConsumerOptions::new(
                video_producer.id(),
                consumer_device_capabilities(),
            ))
            .await
            .expect("Failed to consume video");

        assert_eq!(video_consumer.kind(), MediaKind::Video);
        assert_eq!(video_consumer.rtp_parameters().mid, Some("0".to_string()));
        assert_eq!(
            video_consumer.rtp_parameters().codecs,
            vec![
                RtpCodecParameters::Video {
                    mime_type: MimeTypeVideo::Vp8,
                    payload_type: 101,
                    clock_rate: NonZeroU32::new(90000).unwrap(),
                    parameters: RtpCodecParametersParameters::default(),
                    rtcp_feedback: vec![
                        RtcpFeedback::Nack,
                        RtcpFeedback::CcmFir,
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
            ],
        );
        assert_eq!(
            video_consumer.rtp_parameters().header_extensions,
            vec![
                RtpHeaderExtensionParameters {
                    uri: RtpHeaderExtensionUri::AbsSendTime,
                    id: 4,
                    encrypt: false,
                },
                RtpHeaderExtensionParameters {
                    uri: RtpHeaderExtensionUri::TransportWideCcDraft01,
                    id: 5,
                    encrypt: false,
                },
            ],
        );
        assert_eq!(video_consumer.rtp_parameters().encodings.len(), 1);
        assert!(video_consumer.rtp_parameters().encodings[0].ssrc.is_some());
        assert!(video_consumer.rtp_parameters().encodings[0].rtx.is_some());
        assert_eq!(video_consumer.r#type(), ConsumerType::Simulcast);
        assert!(!video_consumer.paused());
        assert!(video_consumer.producer_paused());
        assert_eq!(
            video_consumer.score(),
            ConsumerScore {
                score: 10,
                producer_score: 0,
                producer_scores: vec![0, 0, 0],
            },
        );
        assert_eq!(video_consumer.app_data().downcast_ref::<()>().unwrap(), &());
    });
}

#[test]
fn producer_pause_resume_are_transmitted_to_pipe_consumer() {
    future::block_on(async move {
        let (_worker1, _worker2, router1, router2, transport1, transport2) = init().await;

        let video_producer = transport1
            .produce(video_producer_options())
            .await
            .expect("Failed to produce video");

        // Pause the videoProducer.
        video_producer
            .pause()
            .await
            .expect("Failed to pause video producer");

        router1
            .pipe_producer_to_router(
                video_producer.id(),
                PipeToRouterOptions::new(router2.clone()),
            )
            .await
            .expect("Failed to pipe video producer to router");

        let video_consumer = transport2
            .consume(ConsumerOptions::new(
                video_producer.id(),
                consumer_device_capabilities(),
            ))
            .await
            .expect("Failed to consume video");

        assert!(video_producer.paused());
        assert!(video_consumer.producer_paused());
        assert!(!video_consumer.paused());

        let (producer_resume_tx, producer_resume_rx) = async_oneshot::oneshot::<()>();
        let _handler = video_consumer.on_producer_resume({
            let producer_resume_tx = Mutex::new(Some(producer_resume_tx));

            move || {
                let _ = producer_resume_tx.lock().take().unwrap().send(());
            }
        });

        video_producer
            .resume()
            .await
            .expect("Failed to resume video producer");

        producer_resume_rx
            .await
            .expect("Failed to receive producer resume event");

        assert!(!video_consumer.producer_paused());
        assert!(!video_consumer.paused());

        let (producer_pause_tx, producer_pause_rx) = async_oneshot::oneshot::<()>();
        let _handler = video_consumer.on_producer_pause({
            let producer_pause_tx = Mutex::new(Some(producer_pause_tx));

            move || {
                let _ = producer_pause_tx.lock().take().unwrap().send(());
            }
        });

        video_producer
            .pause()
            .await
            .expect("Failed to pause video producer");

        producer_pause_rx
            .await
            .expect("Failed to receive producer pause event");

        assert!(video_consumer.producer_paused());
        assert!(!video_consumer.paused());
    });
}

#[test]
fn pipe_to_router_succeeds_with_data() {
    future::block_on(async move {
        let (_worker1, _worker2, router1, router2, transport1, _transport2) = init().await;

        let data_producer = transport1
            .produce_data(data_producer_options())
            .await
            .expect("Failed to produce data");

        let PipeDataProducerToRouterPair {
            pipe_data_consumer,
            pipe_data_producer,
        } = router1
            .pipe_data_producer_to_router(
                data_producer.id(),
                PipeToRouterOptions::new(router2.clone()),
            )
            .await
            .expect("Failed to pipe data producer to router");

        let pipe_data_producer = pipe_data_producer.into_inner();

        {
            let dump = router1.dump().await.expect("Failed to dump router");

            // There should be two Transports in router1:
            // - WebRtcTransport for data_producer.
            // - PipeTransport between router1 and router2.
            assert_eq!(dump.transport_ids.len(), 2);
        }

        {
            let dump = router2.dump().await.expect("Failed to dump router");

            // There should be two Transports in router2:
            // - WebRtcTransport for data_consumer.
            // - PipeTransport between router2 and router1.
            assert_eq!(dump.transport_ids.len(), 2);
        }

        assert_eq!(pipe_data_consumer.r#type(), DataConsumerType::Sctp);
        {
            let sctp_stream_parameters = pipe_data_consumer.sctp_stream_parameters();
            assert!(sctp_stream_parameters.is_some());
            assert!(!sctp_stream_parameters.unwrap().ordered());
            assert_eq!(
                sctp_stream_parameters.unwrap().max_packet_life_time(),
                Some(5000),
            );
            assert_eq!(sctp_stream_parameters.unwrap().max_retransmits(), None);
        }
        assert_eq!(pipe_data_consumer.label().as_str(), "foo");
        assert_eq!(pipe_data_consumer.protocol().as_str(), "bar");

        assert_eq!(pipe_data_producer.id(), data_producer.id());
        assert_eq!(pipe_data_producer.r#type(), DataProducerType::Sctp);
        {
            let sctp_stream_parameters = pipe_data_producer.sctp_stream_parameters();
            assert!(sctp_stream_parameters.is_some());
            assert!(!sctp_stream_parameters.unwrap().ordered());
            assert_eq!(
                sctp_stream_parameters.unwrap().max_packet_life_time(),
                Some(5000),
            );
            assert_eq!(sctp_stream_parameters.unwrap().max_retransmits(), None);
        }
        assert_eq!(pipe_data_producer.label().as_str(), "foo");
        assert_eq!(pipe_data_producer.protocol().as_str(), "bar");
    });
}

#[test]
fn data_consume_for_pipe_data_producer_succeeds() {
    future::block_on(async move {
        let (_worker1, _worker2, router1, router2, transport1, transport2) = init().await;

        let data_producer = transport1
            .produce_data(data_producer_options())
            .await
            .expect("Failed to produce data");

        router1
            .pipe_data_producer_to_router(
                data_producer.id(),
                PipeToRouterOptions::new(router2.clone()),
            )
            .await
            .expect("Failed to pipe data producer to router");

        let data_consumer = transport2
            .consume_data(DataConsumerOptions::new_sctp(data_producer.id()))
            .await
            .expect("Failed to create data consumer");

        assert_eq!(data_consumer.r#type(), DataConsumerType::Sctp);
        {
            let sctp_stream_parameters = data_consumer.sctp_stream_parameters();
            assert!(sctp_stream_parameters.is_some());
            assert!(!sctp_stream_parameters.unwrap().ordered());
            assert_eq!(
                sctp_stream_parameters.unwrap().max_packet_life_time(),
                Some(5000),
            );
            assert_eq!(sctp_stream_parameters.unwrap().max_retransmits(), None);
        }
        assert_eq!(data_consumer.label().as_str(), "foo");
        assert_eq!(data_consumer.protocol().as_str(), "bar");
    });
}

#[test]
fn pipe_to_router_called_twice_generates_single_pair() {
    future::block_on(async move {
        let (worker1, worker2, _router1, _router2, _transport1, _transport2) = init().await;

        let router_a = worker1
            .create_router(RouterOptions::new(media_codecs()))
            .await
            .expect("Failed to create router");

        let router_b = worker2
            .create_router(RouterOptions::new(media_codecs()))
            .await
            .expect("Failed to create router");

        let mut transport_options =
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
        transport_options.enable_sctp = true;

        let transport1 = router_a
            .create_webrtc_transport(transport_options.clone())
            .await
            .expect("Failed to create transport1");

        let transport2 = router_a
            .create_webrtc_transport(transport_options)
            .await
            .expect("Failed to create transport2");

        let audio_producer1 = transport1
            .produce(audio_producer_options())
            .await
            .expect("Failed to produce audio");

        let audio_producer2 = transport2
            .produce(audio_producer_options())
            .await
            .expect("Failed to produce audio");

        router_a
            .pipe_producer_to_router(
                audio_producer1.id(),
                PipeToRouterOptions::new(router_b.clone()),
            )
            .await
            .expect("Failed to pipe audio producer to router");

        router_a
            .pipe_producer_to_router(
                audio_producer2.id(),
                PipeToRouterOptions::new(router_b.clone()),
            )
            .await
            .expect("Failed to pipe audio producer to router");

        {
            let dump = router_a.dump().await.expect("Failed to dump router");

            // There should be 3 Transports in router_b:
            // - WebRtcTransport for audio_producer1 and audio_producer2.
            // - PipeTransport between router_a and router_b.
            assert_eq!(dump.transport_ids.len(), 3);
        }

        {
            let dump = router_b.dump().await.expect("Failed to dump router");

            // There should be 1 Transport in router_b:
            // - PipeTransport between router_a and router_b.
            assert_eq!(dump.transport_ids.len(), 1);
        }
    });
}
