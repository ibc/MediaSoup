use criterion::{criterion_group, criterion_main, Criterion};
use mediasoup::prelude::*;
use std::env;
use std::net::{IpAddr, Ipv4Addr};
use std::num::{NonZeroU32, NonZeroU8};

fn create_ssrc() -> u32 {
    fastrand::u32(100_000_000..999_999_999)
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

fn audio_producer_options() -> ProducerOptions {
    ProducerOptions::new(
        MediaKind::Audio,
        RtpParameters {
            mid: Some(fastrand::u32(100_000_000..999_999_999).to_string()),
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
    )
}

fn video_producer_options() -> ProducerOptions {
    ProducerOptions::new(
        MediaKind::Video,
        RtpParameters {
            mid: Some(fastrand::u32(100_000_000..999_999_999).to_string()),
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
                    ssrc: Some(create_ssrc()),
                    rtx: Some(RtpEncodingParametersRtx {
                        ssrc: create_ssrc(),
                    }),
                    scalability_mode: "L1T3".parse().unwrap(),
                    ..RtpEncodingParameters::default()
                },
                RtpEncodingParameters {
                    ssrc: Some(create_ssrc()),
                    rtx: Some(RtpEncodingParametersRtx {
                        ssrc: create_ssrc(),
                    }),
                    ..RtpEncodingParameters::default()
                },
                RtpEncodingParameters {
                    ssrc: Some(create_ssrc()),
                    rtx: Some(RtpEncodingParametersRtx {
                        ssrc: create_ssrc(),
                    }),
                    ..RtpEncodingParameters::default()
                },
                RtpEncodingParameters {
                    ssrc: Some(create_ssrc()),
                    rtx: Some(RtpEncodingParametersRtx {
                        ssrc: create_ssrc(),
                    }),
                    ..RtpEncodingParameters::default()
                },
            ],
            rtcp: RtcpParameters {
                cname: Some("video-1".to_string()),
                ..RtcpParameters::default()
            },
        },
    )
}

pub fn criterion_benchmark(c: &mut Criterion) {
    let mut group = c.benchmark_group("producer");

    {
        let (_worker, _router, transport_1, _transport_2) =
            futures_lite::future::block_on(async { init().await });

        {
            let audio_producer = futures_lite::future::block_on(async {
                let (_worker, _router, transport_1, _transport_2) = init().await;
                transport_1
                    .produce(audio_producer_options())
                    .await
                    .expect("Failed to produce audio")
            });

            group.bench_function("create/audio", |b| {
                b.iter(|| {
                    let _ = futures_lite::future::block_on(async {
                        transport_1
                            .produce(audio_producer_options())
                            .await
                            .expect("Failed to produce audio")
                    });
                })
            });

            group.bench_function("dump/audio", |b| {
                b.iter(|| {
                    let _ = futures_lite::future::block_on(async { audio_producer.dump().await });
                })
            });

            group.bench_function("stats/audio", |b| {
                b.iter(|| {
                    let _ =
                        futures_lite::future::block_on(async { audio_producer.get_stats().await });
                })
            });
        }

        {
            let video_producer = futures_lite::future::block_on(async {
                let (_worker, _router, transport_1, _transport_2) = init().await;
                transport_1
                    .produce(video_producer_options())
                    .await
                    .expect("Failed to produce video")
            });

            group.bench_function("create/video", |b| {
                b.iter(|| {
                    let _ = futures_lite::future::block_on(async {
                        transport_1
                            .produce(video_producer_options())
                            .await
                            .expect("Failed to produce video")
                    });
                })
            });

            group.bench_function("dump/video", |b| {
                b.iter(|| {
                    let _ = futures_lite::future::block_on(async { video_producer.dump().await });
                })
            });

            group.bench_function("stats/video", |b| {
                b.iter(|| {
                    let _ =
                        futures_lite::future::block_on(async { video_producer.get_stats().await });
                })
            });
        }
    }

    group.finish();
}

criterion_group!(benches, criterion_benchmark);
criterion_main!(benches);
