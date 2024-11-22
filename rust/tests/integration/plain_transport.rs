use futures_lite::future;
use hash_hasher::HashedSet;
#[cfg(not(target_os = "windows"))]
use mediasoup::data_structures::SocketFlags;
use mediasoup::data_structures::{AppData, ListenInfo, Protocol, SctpState, TransportTuple};
use mediasoup::plain_transport::{PlainTransportOptions, PlainTransportRemoteParameters};
use mediasoup::prelude::*;
use mediasoup::router::{Router, RouterOptions};
use mediasoup::rtp_parameters::{
    MimeTypeAudio, MimeTypeVideo, RtpCodecCapability, RtpCodecParametersParameters,
};
use mediasoup::sctp_parameters::SctpParameters;
use mediasoup::srtp_parameters::{SrtpCryptoSuite, SrtpParameters};
use mediasoup::worker::{RequestError, Worker, WorkerSettings};
use mediasoup::worker_manager::WorkerManager;
use portpicker::pick_unused_port;
use std::env;
use std::net::{IpAddr, Ipv4Addr};
use std::num::{NonZeroU32, NonZeroU8};
use std::sync::atomic::{AtomicUsize, Ordering};
use std::sync::Arc;

struct CustomAppData {
    foo: &'static str,
}

fn media_codecs() -> Vec<RtpCodecCapability> {
    vec![
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
            rtcp_feedback: vec![], // Will be ignored.
        },
    ]
}

async fn init() -> (Worker, Router) {
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

    (worker, router)
}

#[test]
fn create_succeeds() {
    future::block_on(async move {
        let (_worker, router) = init().await;

        {
            let transport = router
                .create_plain_transport({
                    let mut plain_transport_options = PlainTransportOptions::new(ListenInfo {
                        protocol: Protocol::Udp,
                        ip: IpAddr::V4(Ipv4Addr::LOCALHOST),
                        announced_address: Some("4.4.4.4".to_string()),
                        port: None,
                        port_range: None,
                        flags: None,
                        send_buffer_size: None,
                        recv_buffer_size: None,
                    });
                    plain_transport_options.rtcp_mux = false;

                    plain_transport_options
                })
                .await
                .expect("Failed to create Plain transport");

            let router_dump = router.dump().await.expect("Failed to dump router");
            assert_eq!(router_dump.transport_ids, {
                let mut set = HashedSet::default();
                set.insert(transport.id());
                set
            });
        }

        {
            let new_transports_count = Arc::new(AtomicUsize::new(0));

            router
                .on_new_transport({
                    let new_transports_count = Arc::clone(&new_transports_count);

                    move |_transport| {
                        new_transports_count.fetch_add(1, Ordering::SeqCst);
                    }
                })
                .detach();

            let transport1 = router
                .create_plain_transport({
                    let mut plain_transport_options = PlainTransportOptions::new(ListenInfo {
                        protocol: Protocol::Udp,
                        ip: IpAddr::V4(Ipv4Addr::LOCALHOST),
                        announced_address: Some("9.9.9.1".to_string()),
                        port: None,
                        port_range: None,
                        flags: None,
                        send_buffer_size: None,
                        recv_buffer_size: None,
                    });
                    plain_transport_options.rtcp_mux = true;
                    plain_transport_options.enable_sctp = true;
                    plain_transport_options.app_data = AppData::new(CustomAppData { foo: "bar" });

                    plain_transport_options
                })
                .await
                .expect("Failed to create Plain transport");

            assert_eq!(new_transports_count.load(Ordering::SeqCst), 1);
            assert!(!transport1.closed());
            assert_eq!(
                transport1
                    .app_data()
                    .downcast_ref::<CustomAppData>()
                    .unwrap()
                    .foo,
                "bar",
            );

            assert!(matches!(
                transport1.tuple(),
                TransportTuple::LocalOnly { .. },
            ));
            if let TransportTuple::LocalOnly {
                local_address,
                protocol,
                ..
            } = transport1.tuple()
            {
                assert_eq!(local_address, "9.9.9.1");
                assert_eq!(protocol, Protocol::Udp);
            }
            assert_eq!(transport1.rtcp_tuple(), None);
            assert_eq!(
                transport1.sctp_parameters(),
                Some(SctpParameters {
                    port: 5000,
                    os: 1024,
                    mis: 1024,
                    max_message_size: 262_144,
                }),
            );
            assert_eq!(transport1.sctp_state(), Some(SctpState::New));
            assert_eq!(transport1.srtp_parameters(), None);

            {
                let transport_dump = transport1
                    .dump()
                    .await
                    .expect("Failed to dump Plain transport");

                assert_eq!(transport_dump.id, transport1.id());
                assert!(!transport_dump.direct);
                assert_eq!(transport_dump.producer_ids, vec![]);
                assert_eq!(transport_dump.consumer_ids, vec![]);
                assert_eq!(transport_dump.tuple, transport1.tuple());
                assert_eq!(transport_dump.rtcp_tuple, transport1.rtcp_tuple());
                assert_eq!(transport_dump.srtp_parameters, transport1.srtp_parameters());
                assert_eq!(transport_dump.sctp_state, transport1.sctp_state());
            }
        }

        {
            let rtcp_port = pick_unused_port().unwrap();
            let transport2 = router
                .create_plain_transport({
                    let mut plain_transport_options = PlainTransportOptions::new(ListenInfo {
                        protocol: Protocol::Udp,
                        ip: IpAddr::V4(Ipv4Addr::LOCALHOST),
                        announced_address: None,
                        port: None,
                        port_range: None,
                        flags: None,
                        send_buffer_size: None,
                        recv_buffer_size: None,
                    });
                    plain_transport_options.rtcp_mux = false;

                    plain_transport_options.rtcp_listen_info = Some(ListenInfo {
                        protocol: Protocol::Udp,
                        ip: IpAddr::V4(Ipv4Addr::LOCALHOST),
                        announced_address: None,
                        port: Some(rtcp_port),
                        port_range: None,
                        flags: None,
                        send_buffer_size: None,
                        recv_buffer_size: None,
                    });

                    plain_transport_options
                })
                .await
                .expect("Failed to create Plain transport");

            assert!(!transport2.closed());
            assert_eq!(transport2.app_data().downcast_ref::<()>().unwrap(), &(),);
            assert!(matches!(
                transport2.tuple(),
                TransportTuple::LocalOnly { .. },
            ));
            if let TransportTuple::LocalOnly {
                local_address,
                protocol,
                ..
            } = transport2.tuple()
            {
                assert_eq!(local_address, "127.0.0.1");
                assert_eq!(protocol, Protocol::Udp);
            }
            assert!(transport2.rtcp_tuple().is_some());
            if let TransportTuple::LocalOnly {
                local_address,
                local_port,
                protocol,
                ..
            } = transport2.rtcp_tuple().unwrap()
            {
                assert_eq!(local_address, "127.0.0.1");
                assert_eq!(local_port, rtcp_port);
                assert_eq!(protocol, Protocol::Udp);
            }
            assert_eq!(transport2.srtp_parameters(), None);
            assert_eq!(transport2.sctp_state(), None);

            {
                let transport_dump = transport2
                    .dump()
                    .await
                    .expect("Failed to dump Plain transport");

                assert_eq!(transport_dump.id, transport2.id());
                assert!(!transport_dump.direct);
                assert_eq!(transport_dump.tuple, transport2.tuple());
                assert_eq!(transport_dump.rtcp_tuple, transport2.rtcp_tuple());
                assert_eq!(transport_dump.sctp_state, transport2.sctp_state());
            }
        }
    });
}

#[test]
fn create_with_fixed_port_succeeds() {
    future::block_on(async move {
        let (_worker, router) = init().await;

        let port = pick_unused_port().unwrap();

        let transport = router
            .create_plain_transport({
                PlainTransportOptions::new(ListenInfo {
                    protocol: Protocol::Udp,
                    ip: IpAddr::V4(Ipv4Addr::LOCALHOST),
                    announced_address: Some("4.4.4.4".to_string()),
                    port: Some(port),
                    port_range: None,
                    flags: None,
                    send_buffer_size: None,
                    recv_buffer_size: None,
                })
            })
            .await
            .expect("Failed to create Plain transport");

        assert_eq!(transport.tuple().local_port(), port);
    });
}

#[test]
fn weak() {
    future::block_on(async move {
        let (_worker, router) = init().await;

        let transport = router
            .create_plain_transport({
                let mut plain_transport_options = PlainTransportOptions::new(ListenInfo {
                    protocol: Protocol::Udp,
                    ip: IpAddr::V4(Ipv4Addr::LOCALHOST),
                    announced_address: Some("4.4.4.4".to_string()),
                    port: None,
                    port_range: None,
                    flags: None,
                    send_buffer_size: None,
                    recv_buffer_size: None,
                });
                plain_transport_options.rtcp_mux = false;

                plain_transport_options
            })
            .await
            .expect("Failed to create Plain transport");

        let weak_transport = transport.downgrade();

        assert!(weak_transport.upgrade().is_some());

        drop(transport);

        assert!(weak_transport.upgrade().is_none());
    });
}

#[test]
fn create_enable_srtp_succeeds() {
    future::block_on(async move {
        let (_worker, router) = init().await;

        // Use default cryptoSuite: 'AES_CM_128_HMAC_SHA1_80'.
        let transport1 = router
            .create_plain_transport({
                let mut plain_transport_options = PlainTransportOptions::new(ListenInfo {
                    protocol: Protocol::Udp,
                    ip: IpAddr::V4(Ipv4Addr::LOCALHOST),
                    announced_address: Some("9.9.9.1".to_string()),
                    port: None,
                    port_range: None,
                    flags: None,
                    send_buffer_size: None,
                    recv_buffer_size: None,
                });
                plain_transport_options.enable_srtp = true;

                plain_transport_options
            })
            .await
            .expect("Failed to create Plain transport");

        assert!(transport1.srtp_parameters().is_some());
        assert_eq!(
            transport1.srtp_parameters().unwrap().crypto_suite,
            SrtpCryptoSuite::AesCm128HmacSha180,
        );
        assert_eq!(transport1.srtp_parameters().unwrap().key_base64.len(), 40);

        // Missing srtp_parameters.
        assert!(matches!(
            transport1
                .connect(PlainTransportRemoteParameters {
                    ip: Some("127.0.0.2".parse().unwrap()),
                    port: Some(9999),
                    rtcp_port: None,
                    srtp_parameters: None
                })
                .await,
            Err(RequestError::Response { .. }),
        ));

        // Valid srtp_parameters. And let's update the crypto suite.
        transport1
            .connect(PlainTransportRemoteParameters {
                ip: Some("127.0.0.2".parse().unwrap()),
                port: Some(9999),
                rtcp_port: None,
                srtp_parameters: Some(SrtpParameters {
                    crypto_suite: SrtpCryptoSuite::AeadAes256Gcm,
                    key_base64: "YTdjcDBvY2JoMGY5YXNlNDc0eDJsdGgwaWRvNnJsamRrdG16aWVpZHphdHo="
                        .to_string(),
                }),
            })
            .await
            .expect("Failed to establish Plain transport connection");

        assert_eq!(
            transport1.srtp_parameters().unwrap().crypto_suite,
            SrtpCryptoSuite::AeadAes256Gcm,
        );
        assert_eq!(transport1.srtp_parameters().unwrap().key_base64.len(), 60);
    });
}

#[test]
fn create_non_bindable_ip() {
    future::block_on(async move {
        let (_worker, router) = init().await;

        assert!(matches!(
            router
                .create_plain_transport(PlainTransportOptions::new(ListenInfo {
                    protocol: Protocol::Udp,
                    ip: "8.8.8.8".parse().unwrap(),
                    announced_address: None,
                    port: None,
                    port_range: None,
                    flags: None,
                    send_buffer_size: None,
                    recv_buffer_size: None,
                }))
                .await,
            Err(RequestError::Response { .. }),
        ));
    });
}

#[cfg(not(target_os = "windows"))]
#[test]
fn create_two_transports_binding_to_same_ip_port_with_udp_reuse_port_flag_succeed() {
    future::block_on(async move {
        let (_worker, router) = init().await;

        let multicast_ip = "224.0.0.1".parse().unwrap();
        let port = pick_unused_port().unwrap();

        let transport1 = router
            .create_plain_transport({
                PlainTransportOptions::new(ListenInfo {
                    protocol: Protocol::Udp,
                    ip: multicast_ip,
                    announced_address: None,
                    port: Some(port),
                    port_range: None,
                    // NOTE: ipv6Only flag will be ignored since ip is IPv4.
                    flags: Some(SocketFlags {
                        ipv6_only: true,
                        udp_reuse_port: true,
                    }),
                    send_buffer_size: None,
                    recv_buffer_size: None,
                })
            })
            .await
            .expect("Failed to create first Plain transport");

        let transport2 = router
            .create_plain_transport({
                PlainTransportOptions::new(ListenInfo {
                    protocol: Protocol::Udp,
                    ip: multicast_ip,
                    announced_address: None,
                    port: Some(port),
                    port_range: None,
                    flags: Some(SocketFlags {
                        ipv6_only: false,
                        udp_reuse_port: true,
                    }),
                    send_buffer_size: None,
                    recv_buffer_size: None,
                })
            })
            .await
            .expect("Failed to create second Plain transport");

        assert_eq!(transport1.tuple().local_port(), port);
        assert_eq!(transport2.tuple().local_port(), port);
    });
}

#[cfg(not(target_os = "windows"))]
#[test]
fn create_two_transports_binding_to_same_ip_port_without_udp_reuse_port_flag_fails() {
    future::block_on(async move {
        let (_worker, router) = init().await;

        let multicast_ip = "224.0.0.1".parse().unwrap();
        let port = pick_unused_port().unwrap();

        let transport1 = router
            .create_plain_transport({
                PlainTransportOptions::new(ListenInfo {
                    protocol: Protocol::Udp,
                    ip: multicast_ip,
                    announced_address: None,
                    port: Some(port),
                    port_range: None,
                    flags: Some(SocketFlags {
                        ipv6_only: false,
                        udp_reuse_port: false,
                    }),
                    send_buffer_size: None,
                    recv_buffer_size: None,
                })
            })
            .await
            .expect("Failed to create first Plain transport");

        assert!(matches!(
            router
                .create_plain_transport(PlainTransportOptions::new(ListenInfo {
                    protocol: Protocol::Udp,
                    ip: multicast_ip,
                    announced_address: None,
                    port: Some(port),
                    port_range: None,
                    flags: Some(SocketFlags {
                        ipv6_only: false,
                        udp_reuse_port: false,
                    }),
                    send_buffer_size: None,
                    recv_buffer_size: None,
                }))
                .await,
            Err(RequestError::Response { .. }),
        ));

        assert_eq!(transport1.tuple().local_port(), port);
    });
}

#[test]
fn get_stats_succeeds() {
    future::block_on(async move {
        let (_worker, router) = init().await;

        let transport = router
            .create_plain_transport({
                let mut plain_transport_options = PlainTransportOptions::new(ListenInfo {
                    protocol: Protocol::Udp,
                    ip: IpAddr::V4(Ipv4Addr::LOCALHOST),
                    announced_address: Some("4.4.4.4".to_string()),
                    port: None,
                    port_range: None,
                    flags: None,
                    send_buffer_size: None,
                    recv_buffer_size: None,
                });
                plain_transport_options.rtcp_mux = false;

                plain_transport_options
            })
            .await
            .expect("Failed to create Plain transport");

        let stats = transport
            .get_stats()
            .await
            .expect("Failed to get stats on Plain transport");

        assert_eq!(stats.len(), 1);
        assert_eq!(stats[0].transport_id, transport.id());
        assert_eq!(stats[0].bytes_received, 0);
        assert_eq!(stats[0].recv_bitrate, 0);
        assert_eq!(stats[0].bytes_sent, 0);
        assert_eq!(stats[0].send_bitrate, 0);
        assert_eq!(stats[0].rtp_bytes_received, 0);
        assert_eq!(stats[0].rtp_recv_bitrate, 0);
        assert_eq!(stats[0].rtp_bytes_sent, 0);
        assert_eq!(stats[0].rtp_send_bitrate, 0);
        assert_eq!(stats[0].rtx_bytes_received, 0);
        assert_eq!(stats[0].rtx_recv_bitrate, 0);
        assert_eq!(stats[0].rtx_bytes_sent, 0);
        assert_eq!(stats[0].rtx_send_bitrate, 0);
        assert_eq!(stats[0].probation_bytes_sent, 0);
        assert_eq!(stats[0].probation_send_bitrate, 0);
        assert_eq!(stats[0].rtp_packet_loss_received, None);
        assert_eq!(stats[0].rtp_packet_loss_sent, None);
        assert!(matches!(stats[0].tuple, TransportTuple::LocalOnly { .. },));
        if let TransportTuple::LocalOnly {
            local_address,
            protocol,
            ..
        } = &stats[0].tuple
        {
            assert_eq!(local_address, "4.4.4.4");
            assert_eq!(*protocol, Protocol::Udp);
        }
        assert_eq!(stats[0].rtcp_tuple, None);
    });
}

#[test]
fn connect_succeeds() {
    future::block_on(async move {
        let (_worker, router) = init().await;

        let transport = router
            .create_plain_transport({
                let mut plain_transport_options = PlainTransportOptions::new(ListenInfo {
                    protocol: Protocol::Udp,
                    ip: IpAddr::V4(Ipv4Addr::LOCALHOST),
                    announced_address: Some("4.4.4.4".to_string()),
                    port: None,
                    port_range: None,
                    flags: None,
                    send_buffer_size: None,
                    recv_buffer_size: None,
                });
                plain_transport_options.rtcp_mux = false;

                plain_transport_options
            })
            .await
            .expect("Failed to create Plain transport");

        transport
            .connect(PlainTransportRemoteParameters {
                ip: Some("1.2.3.4".parse().unwrap()),
                port: Some(1234),
                rtcp_port: Some(1235),
                srtp_parameters: None,
            })
            .await
            .expect("Failed to establish Plain transport connection");

        // Must fail if connected.
        assert!(matches!(
            transport
                .connect(PlainTransportRemoteParameters {
                    ip: Some("1.2.3.4".parse().unwrap()),
                    port: Some(1234),
                    rtcp_port: Some(1235),
                    srtp_parameters: None,
                })
                .await,
            Err(RequestError::Response { .. }),
        ));

        assert!(matches!(
            transport.tuple(),
            TransportTuple::WithRemote { .. },
        ));
        if let TransportTuple::WithRemote {
            remote_ip,
            remote_port,
            protocol,
            ..
        } = transport.tuple()
        {
            assert_eq!(remote_ip, "1.2.3.4".parse::<IpAddr>().unwrap());
            assert_eq!(remote_port, 1234);
            assert_eq!(protocol, Protocol::Udp);
        }
        assert!(transport.rtcp_tuple().is_some());
        if let TransportTuple::WithRemote {
            remote_ip,
            remote_port,
            protocol,
            ..
        } = transport.rtcp_tuple().unwrap()
        {
            assert_eq!(remote_ip, "1.2.3.4".parse::<IpAddr>().unwrap());
            assert_eq!(remote_port, 1235);
            assert_eq!(protocol, Protocol::Udp);
        }
    });
}

#[test]
fn connect_wrong_arguments() {
    future::block_on(async move {
        let (_worker, router) = init().await;

        let transport = router
            .create_plain_transport({
                let mut plain_transport_options = PlainTransportOptions::new(ListenInfo {
                    protocol: Protocol::Udp,
                    ip: IpAddr::V4(Ipv4Addr::LOCALHOST),
                    announced_address: Some("4.4.4.4".to_string()),
                    port: None,
                    port_range: None,
                    flags: None,
                    send_buffer_size: None,
                    recv_buffer_size: None,
                });
                plain_transport_options.rtcp_mux = false;

                plain_transport_options
            })
            .await
            .expect("Failed to create Plain transport");

        // No SRTP enabled so passing srtp_parameters must fail.
        assert!(matches!(
            transport
                .connect(PlainTransportRemoteParameters {
                    ip: Some("127.0.0.2".parse().unwrap()),
                    port: Some(9998),
                    rtcp_port: Some(9999),
                    srtp_parameters: Some(SrtpParameters {
                        crypto_suite: SrtpCryptoSuite::AesCm128HmacSha180,
                        key_base64: "ZnQ3eWJraDg0d3ZoYzM5cXN1Y2pnaHU5NWxrZTVv".to_string(),
                    }),
                })
                .await,
            Err(RequestError::Response { .. }),
        ));
    });
}

#[test]
fn close_event() {
    future::block_on(async move {
        let (_worker, router) = init().await;

        let transport = router
            .create_plain_transport({
                let mut plain_transport_options = PlainTransportOptions::new(ListenInfo {
                    protocol: Protocol::Udp,
                    ip: IpAddr::V4(Ipv4Addr::LOCALHOST),
                    announced_address: Some("4.4.4.4".to_string()),
                    port: None,
                    port_range: None,
                    flags: None,
                    send_buffer_size: None,
                    recv_buffer_size: None,
                });
                plain_transport_options.rtcp_mux = false;

                plain_transport_options
            })
            .await
            .expect("Failed to create Plain transport");

        let (mut close_tx, close_rx) = async_oneshot::oneshot::<()>();
        let _handler = transport.on_close(Box::new(move || {
            let _ = close_tx.send(());
        }));

        drop(transport);

        close_rx.await.expect("Failed to receive close event");
    });
}
