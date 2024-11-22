use async_io::Timer;
use futures_lite::future;
use hash_hasher::{HashedMap, HashedSet};
use mediasoup::data_structures::{AppData, ListenInfo, Protocol};
use mediasoup::prelude::*;
use mediasoup::producer::{ProducerOptions, ProducerTraceEventType, ProducerType};
use mediasoup::router::{Router, RouterOptions};
use mediasoup::rtp_parameters::{
    MediaKind, MimeTypeAudio, MimeTypeVideo, RtcpFeedback, RtcpParameters, RtpCodecCapability,
    RtpCodecParameters, RtpCodecParametersParameters, RtpEncodingParameters,
    RtpEncodingParametersRtx, RtpHeaderExtensionParameters, RtpHeaderExtensionUri, RtpParameters,
};
use mediasoup::scalability_modes::ScalabilityMode;
use mediasoup::transport::ProduceError;
use mediasoup::webrtc_transport::{
    WebRtcTransport, WebRtcTransportListenInfos, WebRtcTransportOptions,
};
use mediasoup::worker::{Worker, WorkerSettings};
use mediasoup::worker_manager::WorkerManager;
use std::env;
use std::net::{IpAddr, Ipv4Addr};
use std::num::{NonZeroU32, NonZeroU8};
use std::sync::atomic::{AtomicUsize, Ordering};
use std::sync::Arc;
use std::time::Duration;

struct ProducerAppData {
    foo: i32,
    bar: &'static str,
}

fn media_codecs() -> Vec<RtpCodecCapability> {
    vec![
        RtpCodecCapability::Audio {
            mime_type: MimeTypeAudio::Opus,
            preferred_payload_type: None,
            clock_rate: NonZeroU32::new(48000).unwrap(),
            channels: NonZeroU8::new(2).unwrap(),
            parameters: RtpCodecParametersParameters::from([("foo", "111".into())]),
            rtcp_feedback: vec![],
        },
        RtpCodecCapability::Video {
            mime_type: MimeTypeVideo::Vp8,
            preferred_payload_type: None,
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
                ("packetization-mode", 1_u32.into()),
                ("profile-level-id", "4d0032".into()),
                ("foo", "bar".into()),
            ]),
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
                payload_type: 0,
                clock_rate: NonZeroU32::new(48000).unwrap(),
                channels: NonZeroU8::new(2).unwrap(),
                parameters: RtpCodecParametersParameters::from([
                    ("useinbandfec", 1_u32.into()),
                    ("usedtx", 1_u32.into()),
                    ("foo", "222.222".into()),
                    ("bar", "333".into()),
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
            // Missing encodings on purpose.
            encodings: vec![],
            rtcp: RtcpParameters {
                cname: Some("audio-1".to_string()),
                ..RtcpParameters::default()
            },
        },
    );

    options.app_data = AppData::new(ProducerAppData { foo: 1, bar: "2" });

    options
}

fn video_producer_options() -> ProducerOptions {
    let mut options = ProducerOptions::new(
        MediaKind::Video,
        RtpParameters {
            mid: Some("VIDEO".to_string()),
            codecs: vec![
                RtpCodecParameters::Video {
                    mime_type: MimeTypeVideo::H264,
                    payload_type: 112,
                    clock_rate: NonZeroU32::new(90000).unwrap(),
                    parameters: RtpCodecParametersParameters::from([
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
                    payload_type: 113,
                    clock_rate: NonZeroU32::new(90000).unwrap(),
                    parameters: RtpCodecParametersParameters::from([("apt", 112u32.into())]),
                    rtcp_feedback: vec![],
                },
            ],
            header_extensions: vec![
                RtpHeaderExtensionParameters {
                    uri: RtpHeaderExtensionUri::Mid,
                    id: 10,
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
                    rtx: Some(RtpEncodingParametersRtx { ssrc: 22222223 }),
                    scalability_mode: "L1T3".parse().unwrap(),
                    ..RtpEncodingParameters::default()
                },
                RtpEncodingParameters {
                    ssrc: Some(22222224),
                    rtx: Some(RtpEncodingParametersRtx { ssrc: 22222225 }),
                    ..RtpEncodingParameters::default()
                },
                RtpEncodingParameters {
                    ssrc: Some(22222226),
                    rtx: Some(RtpEncodingParametersRtx { ssrc: 22222227 }),
                    ..RtpEncodingParameters::default()
                },
                RtpEncodingParameters {
                    ssrc: Some(22222228),
                    rtx: Some(RtpEncodingParametersRtx { ssrc: 22222229 }),
                    ..RtpEncodingParameters::default()
                },
            ],
            rtcp: RtcpParameters {
                cname: Some("video-1".to_string()),
                ..RtcpParameters::default()
            },
        },
    );

    options.app_data = AppData::new(ProducerAppData { foo: 1, bar: "2" });

    options
}

async fn init() -> (Worker, Router, WebRtcTransport, WebRtcTransport) {
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

    let transport_1 = router
        .create_webrtc_transport(transport_options.clone())
        .await
        .expect("Failed to create transport1");

    let transport_2 = router
        .create_webrtc_transport(transport_options)
        .await
        .expect("Failed to create transport2");

    (worker, router, transport_1, transport_2)
}

#[test]
fn produce_succeeds_1() {
    future::block_on(async move {
        let (_worker, router, transport_1, _transport_2) = init().await;

        let new_producers_count = Arc::new(AtomicUsize::new(0));

        transport_1
            .on_new_producer({
                let new_producers_count = Arc::clone(&new_producers_count);

                Arc::new(move |_producer| {
                    new_producers_count.fetch_add(1, Ordering::SeqCst);
                })
            })
            .detach();

        let audio_producer = transport_1
            .produce(audio_producer_options())
            .await
            .expect("Failed to produce audio");

        assert_eq!(new_producers_count.load(Ordering::SeqCst), 1);
        assert!(!audio_producer.closed());
        assert_eq!(audio_producer.kind(), MediaKind::Audio);
        assert_eq!(audio_producer.r#type(), ProducerType::Simple);
        assert!(!audio_producer.paused());
        assert_eq!(audio_producer.score(), vec![]);
        assert_eq!(
            audio_producer
                .app_data()
                .downcast_ref::<ProducerAppData>()
                .unwrap()
                .foo,
            1
        );
        assert_eq!(
            audio_producer
                .app_data()
                .downcast_ref::<ProducerAppData>()
                .unwrap()
                .bar,
            "2"
        );

        let router_dump = router.dump().await.expect("Failed to get router dump");

        assert_eq!(router_dump.map_producer_id_consumer_ids, {
            let mut map = HashedMap::default();
            map.insert(audio_producer.id(), HashedSet::default());
            map
        });
        assert_eq!(
            router_dump.map_consumer_id_producer_id,
            HashedMap::default()
        );

        let transport_1_dump = transport_1
            .dump()
            .await
            .expect("Failed to get transport 1 dump");

        assert_eq!(transport_1_dump.producer_ids, vec![audio_producer.id()]);
        assert_eq!(transport_1_dump.consumer_ids, vec![]);

        let (mut tx, rx) = async_oneshot::oneshot::<()>();
        transport_1
            .on_close(Box::new(move || {
                let _ = tx.send(());
            }))
            .detach();

        drop(audio_producer);
        drop(transport_1);

        // This means producer was definitely dropped
        rx.await.expect("Failed to receive transport close event");
    });
}

#[test]
fn produce_succeeds_2() {
    future::block_on(async move {
        let (_worker, router, _transport_1, transport_2) = init().await;

        let new_producers_count = Arc::new(AtomicUsize::new(0));

        transport_2
            .on_new_producer({
                let new_producers_count = Arc::clone(&new_producers_count);

                Arc::new(move |_producer| {
                    new_producers_count.fetch_add(1, Ordering::SeqCst);
                })
            })
            .detach();

        let video_producer = transport_2
            .produce(video_producer_options())
            .await
            .expect("Failed to produce video");

        assert_eq!(new_producers_count.load(Ordering::SeqCst), 1);
        assert!(!video_producer.closed());
        assert_eq!(video_producer.kind(), MediaKind::Video);
        assert_eq!(video_producer.r#type(), ProducerType::Simulcast);
        assert!(!video_producer.paused());
        assert_eq!(video_producer.score(), vec![]);
        assert_eq!(
            video_producer
                .app_data()
                .downcast_ref::<ProducerAppData>()
                .unwrap()
                .foo,
            1
        );
        assert_eq!(
            video_producer
                .app_data()
                .downcast_ref::<ProducerAppData>()
                .unwrap()
                .bar,
            "2"
        );

        let router_dump = router.dump().await.expect("Failed to get router dump");

        assert_eq!(router_dump.map_producer_id_consumer_ids, {
            let mut map = HashedMap::default();
            map.insert(video_producer.id(), HashedSet::default());
            map
        });
        assert_eq!(
            router_dump.map_consumer_id_producer_id,
            HashedMap::default()
        );

        let transport_2_dump = transport_2
            .dump()
            .await
            .expect("Failed to get transport 2 dump");

        assert_eq!(transport_2_dump.producer_ids, vec![video_producer.id()]);
        assert_eq!(transport_2_dump.consumer_ids, vec![]);
    });
}

#[test]
fn weak() {
    future::block_on(async move {
        let (_worker, _router, transport_1, _transport_2) = init().await;

        let producer = transport_1
            .produce(audio_producer_options())
            .await
            .expect("Failed to produce audio");

        let weak_producer = producer.downgrade();

        assert!(weak_producer.upgrade().is_some());

        drop(producer);

        assert!(weak_producer.upgrade().is_none());
    });
}

#[test]
fn produce_wrong_arguments() {
    future::block_on(async move {
        let (_worker, _router, transport_1, _transport_2) = init().await;

        // Empty rtp_parameters.codecs.
        assert!(matches!(
            transport_1
                .produce(ProducerOptions::new(
                    MediaKind::Audio,
                    RtpParameters::default()
                ))
                .await,
            Err(ProduceError::Request(_)),
        ));

        {
            // Empty rtp_parameters.encodings.
            let produce_result = transport_1
                .produce(ProducerOptions::new(MediaKind::Video, {
                    let mut parameters = RtpParameters::default();
                    parameters.codecs = vec![
                        RtpCodecParameters::Video {
                            mime_type: MimeTypeVideo::H264,
                            payload_type: 112,
                            clock_rate: NonZeroU32::new(90000).unwrap(),
                            parameters: RtpCodecParametersParameters::from([
                                ("packetization-mode", 1_u32.into()),
                                ("profile-level-id", "4d0032".into()),
                            ]),
                            rtcp_feedback: vec![],
                        },
                        RtpCodecParameters::Video {
                            mime_type: MimeTypeVideo::Rtx,
                            payload_type: 113,
                            clock_rate: NonZeroU32::new(90000).unwrap(),
                            parameters: RtpCodecParametersParameters::from([(
                                "apt",
                                112u32.into(),
                            )]),
                            rtcp_feedback: vec![],
                        },
                    ];
                    parameters.rtcp = RtcpParameters {
                        cname: Some("qwerty".to_string()),
                        ..RtcpParameters::default()
                    };
                    parameters
                }))
                .await;

            assert!(matches!(produce_result, Err(ProduceError::Request(_))));
        }

        {
            // Wrong apt in RTX codec.
            let produce_result = transport_1
                .produce(ProducerOptions::new(MediaKind::Video, {
                    let mut parameters = RtpParameters::default();
                    parameters.codecs = vec![
                        RtpCodecParameters::Video {
                            mime_type: MimeTypeVideo::H264,
                            payload_type: 112,
                            clock_rate: NonZeroU32::new(90000).unwrap(),
                            parameters: RtpCodecParametersParameters::from([
                                ("packetization-mode", 1_u32.into()),
                                ("profile-level-id", "4d0032".into()),
                            ]),
                            rtcp_feedback: vec![],
                        },
                        RtpCodecParameters::Video {
                            mime_type: MimeTypeVideo::Rtx,
                            payload_type: 113,
                            clock_rate: NonZeroU32::new(90000).unwrap(),
                            parameters: RtpCodecParametersParameters::from([(
                                "apt",
                                111_u32.into(),
                            )]),
                            rtcp_feedback: vec![],
                        },
                    ];
                    parameters.encodings = vec![RtpEncodingParameters {
                        ssrc: Some(6666),
                        rtx: Some(RtpEncodingParametersRtx { ssrc: 6667 }),
                        ..RtpEncodingParameters::default()
                    }];
                    parameters.rtcp = RtcpParameters {
                        cname: Some("video-1".to_string()),
                        ..RtcpParameters::default()
                    };
                    parameters
                }))
                .await;

            assert!(matches!(
                produce_result,
                Err(ProduceError::FailedRtpParametersMapping(_))
            ));
        }
    });
}

#[test]
fn produce_unsupported_codecs() {
    future::block_on(async move {
        let (_worker, _router, transport_1, _transport_2) = init().await;

        // Empty rtp_parameters.codecs.
        assert!(matches!(
            transport_1
                .produce(ProducerOptions::new(MediaKind::Audio, {
                    let mut parameters = RtpParameters::default();
                    parameters.codecs = vec![RtpCodecParameters::Audio {
                        mime_type: MimeTypeAudio::Isac,
                        payload_type: 108,
                        clock_rate: NonZeroU32::new(32000).unwrap(),
                        channels: NonZeroU8::new(1).unwrap(),
                        parameters: RtpCodecParametersParameters::default(),
                        rtcp_feedback: vec![],
                    }];
                    parameters.header_extensions = vec![];
                    parameters.encodings = vec![RtpEncodingParameters {
                        ssrc: Some(1111),
                        ..RtpEncodingParameters::default()
                    }];
                    parameters.rtcp = RtcpParameters {
                        cname: Some("audio".to_string()),
                        ..RtcpParameters::default()
                    };
                    parameters
                }))
                .await,
            Err(ProduceError::FailedRtpParametersMapping(_)),
        ));

        {
            // Invalid H264 profile-level-id.
            let produce_result = transport_1
                .produce(ProducerOptions::new(MediaKind::Video, {
                    let mut parameters = RtpParameters::default();
                    parameters.codecs = vec![
                        RtpCodecParameters::Video {
                            mime_type: MimeTypeVideo::H264,
                            payload_type: 112,
                            clock_rate: NonZeroU32::new(90000).unwrap(),
                            parameters: RtpCodecParametersParameters::from([
                                ("packetization-mode", 1_u32.into()),
                                ("profile-level-id", "CHICKEN".into()),
                            ]),
                            rtcp_feedback: vec![],
                        },
                        RtpCodecParameters::Video {
                            mime_type: MimeTypeVideo::Rtx,
                            payload_type: 113,
                            clock_rate: NonZeroU32::new(90000).unwrap(),
                            parameters: RtpCodecParametersParameters::from([(
                                "apt",
                                112u32.into(),
                            )]),
                            rtcp_feedback: vec![],
                        },
                    ];
                    parameters.encodings = vec![RtpEncodingParameters {
                        ssrc: Some(6666),
                        rtx: Some(RtpEncodingParametersRtx { ssrc: 6667 }),
                        ..RtpEncodingParameters::default()
                    }];
                    parameters
                }))
                .await;

            assert!(matches!(
                produce_result,
                Err(ProduceError::FailedRtpParametersMapping(_))
            ));
        }
    });
}

#[test]
fn produce_already_used_mid_ssrc() {
    future::block_on(async move {
        let (_worker, _router, transport_1, transport_2) = init().await;

        {
            let _first_producer = transport_1
                .produce(audio_producer_options())
                .await
                .expect("Failed to produce audio");

            let produce_result = transport_1
                .produce(ProducerOptions::new(MediaKind::Audio, {
                    let mut parameters = RtpParameters::default();
                    parameters.mid = Some("AUDIO".to_string());
                    parameters.codecs = vec![RtpCodecParameters::Audio {
                        mime_type: MimeTypeAudio::Opus,
                        payload_type: 0,
                        clock_rate: NonZeroU32::new(48000).unwrap(),
                        channels: NonZeroU8::new(2).unwrap(),
                        parameters: RtpCodecParametersParameters::default(),
                        rtcp_feedback: vec![],
                    }];
                    parameters.header_extensions = vec![];
                    parameters.encodings = vec![RtpEncodingParameters {
                        ssrc: Some(33333333),
                        ..RtpEncodingParameters::default()
                    }];
                    parameters.rtcp = RtcpParameters {
                        cname: Some("audio-2".to_string()),
                        ..RtcpParameters::default()
                    };
                    parameters
                }))
                .await;

            assert!(matches!(produce_result, Err(ProduceError::Request(_)),));
        }

        {
            let _first_producer = transport_2
                .produce(video_producer_options())
                .await
                .expect("Failed to produce video");

            // Invalid H264 profile-level-id.
            let produce_result = transport_2
                .produce(ProducerOptions::new(MediaKind::Video, {
                    let mut parameters = RtpParameters::default();
                    parameters.mid = Some("VIDEO2".to_string());
                    parameters.codecs = vec![RtpCodecParameters::Video {
                        mime_type: MimeTypeVideo::Vp8,
                        payload_type: 112,
                        clock_rate: NonZeroU32::new(90000).unwrap(),
                        parameters: RtpCodecParametersParameters::default(),
                        rtcp_feedback: vec![],
                    }];
                    parameters.encodings = vec![RtpEncodingParameters {
                        ssrc: Some(22222222),
                        rtx: Some(RtpEncodingParametersRtx { ssrc: 6667 }),
                        ..RtpEncodingParameters::default()
                    }];
                    parameters
                }))
                .await;

            assert!(matches!(produce_result, Err(ProduceError::Request(_))));
        }
    });
}

#[test]
fn produce_no_mid_single_encoding_without_rid_or_ssrc() {
    future::block_on(async move {
        let (_worker, _router, transport_1, _transport_2) = init().await;

        let produce_result = transport_1
            .produce(ProducerOptions::new(MediaKind::Audio, {
                let mut parameters = RtpParameters::default();
                parameters.codecs = vec![RtpCodecParameters::Audio {
                    mime_type: MimeTypeAudio::Opus,
                    payload_type: 111,
                    clock_rate: NonZeroU32::new(48000).unwrap(),
                    channels: NonZeroU8::new(2).unwrap(),
                    parameters: RtpCodecParametersParameters::default(),
                    rtcp_feedback: vec![],
                }];
                parameters.header_extensions = vec![];
                parameters.encodings = vec![RtpEncodingParameters::default()];
                parameters.rtcp = RtcpParameters {
                    cname: Some("audio-2".to_string()),
                    ..RtcpParameters::default()
                };
                parameters
            }))
            .await;

        assert!(matches!(produce_result, Err(ProduceError::Request(_)),));
    });
}

#[test]
fn dump_succeeds() {
    future::block_on(async move {
        let (_worker, _router, transport_1, transport_2) = init().await;

        {
            let audio_producer = transport_1
                .produce(audio_producer_options())
                .await
                .expect("Failed to produce audio");

            let dump = audio_producer
                .dump()
                .await
                .expect("Failed to dump audio producer");

            assert_eq!(dump.id, audio_producer.id());
            assert_eq!(dump.kind, audio_producer.kind());
            assert_eq!(
                dump.rtp_parameters.codecs,
                audio_producer_options().rtp_parameters.codecs,
            );
            assert_eq!(
                dump.rtp_parameters.header_extensions,
                audio_producer_options().rtp_parameters.header_extensions,
            );
            assert_eq!(
                dump.rtp_parameters.encodings,
                vec![RtpEncodingParameters {
                    ssrc: None,
                    rid: None,
                    codec_payload_type: Some(0),
                    rtx: None,
                    dtx: None,
                    scalability_mode: ScalabilityMode::None,
                    max_bitrate: None
                }],
            );
            assert_eq!(dump.r#type, ProducerType::Simple);
        }

        {
            let video_producer = transport_2
                .produce(video_producer_options())
                .await
                .expect("Failed to produce video");

            let dump = video_producer
                .dump()
                .await
                .expect("Failed to dump video producer");

            assert_eq!(dump.id, video_producer.id());
            assert_eq!(dump.kind, video_producer.kind());
            assert_eq!(
                dump.rtp_parameters.codecs,
                video_producer_options().rtp_parameters.codecs,
            );
            assert_eq!(
                dump.rtp_parameters.header_extensions,
                video_producer_options().rtp_parameters.header_extensions,
            );
            assert_eq!(
                dump.rtp_parameters.encodings,
                vec![
                    RtpEncodingParameters {
                        ssrc: Some(22222222),
                        rid: None,
                        codec_payload_type: Some(112),
                        rtx: Some(RtpEncodingParametersRtx { ssrc: 22222223 }),
                        dtx: None,
                        scalability_mode: "L1T3".parse().unwrap(),
                        max_bitrate: None
                    },
                    RtpEncodingParameters {
                        ssrc: Some(22222224),
                        rid: None,
                        codec_payload_type: Some(112),
                        rtx: Some(RtpEncodingParametersRtx { ssrc: 22222225 }),
                        dtx: None,
                        scalability_mode: ScalabilityMode::None,
                        max_bitrate: None
                    },
                    RtpEncodingParameters {
                        ssrc: Some(22222226),
                        rid: None,
                        codec_payload_type: Some(112),
                        rtx: Some(RtpEncodingParametersRtx { ssrc: 22222227 }),
                        dtx: None,
                        scalability_mode: ScalabilityMode::None,
                        max_bitrate: None
                    },
                    RtpEncodingParameters {
                        ssrc: Some(22222228),
                        rid: None,
                        codec_payload_type: Some(112),
                        rtx: Some(RtpEncodingParametersRtx { ssrc: 22222229 }),
                        dtx: None,
                        scalability_mode: ScalabilityMode::None,
                        max_bitrate: None
                    },
                ],
            );
            assert_eq!(dump.r#type, ProducerType::Simulcast);
        }
    });
}

#[test]
fn get_stats_succeeds() {
    future::block_on(async move {
        let (_worker, _router, transport_1, transport_2) = init().await;

        {
            let audio_producer = transport_1
                .produce(audio_producer_options())
                .await
                .expect("Failed to produce audio");

            let stats = audio_producer
                .get_stats()
                .await
                .expect("Failed to get stats on audio producer");

            assert_eq!(stats, vec![]);
        }

        {
            let video_producer = transport_2
                .produce(video_producer_options())
                .await
                .expect("Failed to produce video");

            let stats = video_producer
                .get_stats()
                .await
                .expect("Failed to get stats on video producer");

            assert_eq!(stats, vec![]);
        }
    });
}

#[test]
fn pause_resume_succeeds() {
    future::block_on(async move {
        let (_worker, _router, transport_1, _transport_2) = init().await;

        let audio_producer = transport_1
            .produce(audio_producer_options())
            .await
            .expect("Failed to produce audio");

        {
            audio_producer
                .pause()
                .await
                .expect("Failed to pause audio producer");

            assert!(audio_producer.paused());

            let dump = audio_producer
                .dump()
                .await
                .expect("Failed to dump audio producer");

            assert!(dump.paused);
        }

        {
            audio_producer
                .resume()
                .await
                .expect("Failed to resume audio producer");

            assert!(!audio_producer.paused());

            let dump = audio_producer
                .dump()
                .await
                .expect("Failed to dump audio producer");

            assert!(!dump.paused);
        }
    });
}

#[test]
fn enable_trace_event_succeeds() {
    future::block_on(async move {
        let (_worker, _router, transport_1, _transport_2) = init().await;

        let audio_producer = transport_1
            .produce(audio_producer_options())
            .await
            .expect("Failed to produce audio");

        {
            audio_producer
                .enable_trace_event(vec![
                    ProducerTraceEventType::Rtp,
                    ProducerTraceEventType::Pli,
                ])
                .await
                .expect("Failed to enable trace event");

            let dump = audio_producer
                .dump()
                .await
                .expect("Failed to dump audio producer");

            assert_eq!(
                dump.trace_event_types,
                vec![ProducerTraceEventType::Rtp, ProducerTraceEventType::Pli]
            );
        }

        {
            audio_producer
                .enable_trace_event(vec![])
                .await
                .expect("Failed to enable trace event");

            let dump = audio_producer
                .dump()
                .await
                .expect("Failed to dump audio producer");

            assert_eq!(dump.trace_event_types, vec![]);
        }
    });
}

#[test]
fn close_event() {
    future::block_on(async move {
        let (_worker, router, transport_1, _transport_2) = init().await;

        let audio_producer = transport_1
            .produce(audio_producer_options())
            .await
            .expect("Failed to produce audio");

        {
            let (mut tx, rx) = async_oneshot::oneshot::<()>();
            let _handler = audio_producer.on_close(move || {
                let _ = tx.send(());
            });
            drop(audio_producer);

            rx.await.expect("Failed to receive close event");
        }

        // Drop is async, give producer a bit of time to finish
        Timer::after(Duration::from_millis(200)).await;

        {
            let dump = router.dump().await.expect("Failed to dump router");

            assert_eq!(dump.map_producer_id_consumer_ids, HashedMap::default());
            assert_eq!(dump.map_consumer_id_producer_id, HashedMap::default());
        }

        {
            let dump = transport_1.dump().await.expect("Failed to dump transport");

            assert_eq!(dump.producer_ids, vec![]);
            assert_eq!(dump.consumer_ids, vec![]);
        }
    });
}
