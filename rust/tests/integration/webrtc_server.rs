use futures_lite::future;
use hash_hasher::HashedSet;
use mediasoup::data_structures::{AppData, ListenInfo, Protocol};
use mediasoup::webrtc_server::{WebRtcServerIpPort, WebRtcServerListenInfos, WebRtcServerOptions};
use mediasoup::worker::{ChannelMessageHandlers, CreateWebRtcServerError, Worker, WorkerSettings};
use mediasoup::worker_manager::WorkerManager;
use portpicker::pick_unused_port;
use std::env;
use std::net::{IpAddr, Ipv4Addr};
use std::sync::atomic::{AtomicUsize, Ordering};
use std::sync::Arc;

#[derive(Debug, PartialEq)]
struct CustomAppData {
    foo: u32,
}

async fn init() -> (Worker, Worker) {
    {
        let mut builder = env_logger::builder();
        if env::var(env_logger::DEFAULT_FILTER_ENV).is_err() {
            builder.filter_level(log::LevelFilter::Off);
        }
        let _ = builder.is_test(true).try_init();
    }

    let worker_manager = WorkerManager::new();

    (
        worker_manager
            .create_worker(WorkerSettings::default())
            .await
            .expect("Failed to create worker"),
        worker_manager
            .create_worker(WorkerSettings::default())
            .await
            .expect("Failed to create worker"),
    )
}

#[test]
fn create_webrtc_server_succeeds() {
    future::block_on(async move {
        let (worker1, _worker2) = init().await;

        let new_webrtc_server_count = Arc::new(AtomicUsize::new(0));

        let port1 = pick_unused_port().unwrap();
        let port2 = pick_unused_port().unwrap();
        worker1
            .on_new_webrtc_server({
                let new_webrtc_server_count = Arc::clone(&new_webrtc_server_count);

                move |_webrtc_server| {
                    new_webrtc_server_count.fetch_add(1, Ordering::SeqCst);
                }
            })
            .detach();

        let webrtc_server = worker1
            .create_webrtc_server({
                let listen_infos = WebRtcServerListenInfos::new(ListenInfo {
                    protocol: Protocol::Udp,
                    ip: IpAddr::V4(Ipv4Addr::LOCALHOST),
                    announced_address: None,
                    port: Some(port1),
                    port_range: None,
                    flags: None,
                    send_buffer_size: None,
                    recv_buffer_size: None,
                });
                let listen_infos = listen_infos.insert(ListenInfo {
                    protocol: Protocol::Tcp,
                    ip: IpAddr::V4(Ipv4Addr::LOCALHOST),
                    announced_address: Some("foo.bar.org".to_string()),
                    port: Some(port2),
                    port_range: None,
                    flags: None,
                    send_buffer_size: None,
                    recv_buffer_size: None,
                });
                let mut webrtc_server_options = WebRtcServerOptions::new(listen_infos);

                webrtc_server_options.app_data = AppData::new(CustomAppData { foo: 123 });

                webrtc_server_options
            })
            .await
            .expect("Failed to create router");

        assert_eq!(new_webrtc_server_count.load(Ordering::SeqCst), 1);
        assert!(!webrtc_server.closed());
        assert_eq!(
            webrtc_server.app_data().downcast_ref::<CustomAppData>(),
            Some(&CustomAppData { foo: 123 }),
        );

        let worker_dump = worker1.dump().await.expect("Failed to dump worker");

        assert_eq!(worker_dump.router_ids, vec![]);
        assert_eq!(worker_dump.webrtc_server_ids, vec![webrtc_server.id()]);
        assert_eq!(
            worker_dump.channel_message_handlers,
            ChannelMessageHandlers {
                channel_request_handlers: vec![webrtc_server.id().into()],
                channel_notification_handlers: vec![]
            }
        );

        let dump = webrtc_server
            .dump()
            .await
            .expect("Failed to dump WebRTC server");

        assert_eq!(dump.id, webrtc_server.id());
        assert_eq!(
            dump.udp_sockets,
            vec![WebRtcServerIpPort {
                ip: IpAddr::V4(Ipv4Addr::LOCALHOST),
                port: port1
            }]
        );
        assert_eq!(
            dump.tcp_servers,
            vec![WebRtcServerIpPort {
                ip: IpAddr::V4(Ipv4Addr::LOCALHOST),
                port: port2
            }]
        );
        assert_eq!(dump.webrtc_transport_ids, HashedSet::default());
        assert_eq!(dump.local_ice_username_fragments, vec![]);
        assert_eq!(dump.tuple_hashes, vec![]);
    });
}

#[test]
fn create_webrtc_server_without_specifying_port_succeeds() {
    future::block_on(async move {
        let (worker1, _worker2) = init().await;

        let new_webrtc_server_count = Arc::new(AtomicUsize::new(0));

        worker1
            .on_new_webrtc_server({
                let new_webrtc_server_count = Arc::clone(&new_webrtc_server_count);

                move |_webrtc_server| {
                    new_webrtc_server_count.fetch_add(1, Ordering::SeqCst);
                }
            })
            .detach();

        let webrtc_server = worker1
            .create_webrtc_server({
                let listen_infos = WebRtcServerListenInfos::new(ListenInfo {
                    protocol: Protocol::Udp,
                    ip: IpAddr::V4(Ipv4Addr::LOCALHOST),
                    announced_address: None,
                    port: None,
                    port_range: None,
                    flags: None,
                    send_buffer_size: None,
                    recv_buffer_size: None,
                });
                let listen_infos = listen_infos.insert(ListenInfo {
                    protocol: Protocol::Tcp,
                    ip: IpAddr::V4(Ipv4Addr::LOCALHOST),
                    announced_address: Some("1.2.3.4".to_string()),
                    port: None,
                    port_range: None,
                    flags: None,
                    send_buffer_size: None,
                    recv_buffer_size: None,
                });
                let mut webrtc_server_options = WebRtcServerOptions::new(listen_infos);

                webrtc_server_options.app_data = AppData::new(CustomAppData { foo: 123 });

                webrtc_server_options
            })
            .await
            .expect("Failed to create router");

        assert_eq!(new_webrtc_server_count.load(Ordering::SeqCst), 1);
        assert!(!webrtc_server.closed());
        assert_eq!(
            webrtc_server.app_data().downcast_ref::<CustomAppData>(),
            Some(&CustomAppData { foo: 123 }),
        );

        let worker_dump = worker1.dump().await.expect("Failed to dump worker");

        assert_eq!(worker_dump.router_ids, vec![]);
        assert_eq!(worker_dump.webrtc_server_ids, vec![webrtc_server.id()]);

        let dump = webrtc_server
            .dump()
            .await
            .expect("Failed to dump WebRTC server");

        assert_eq!(dump.id, webrtc_server.id());

        assert_eq!(dump.udp_sockets[0].ip, IpAddr::V4(Ipv4Addr::LOCALHOST));
        assert_eq!(dump.tcp_servers[0].ip, IpAddr::V4(Ipv4Addr::LOCALHOST));

        assert_eq!(dump.webrtc_transport_ids, HashedSet::default());
        assert_eq!(dump.local_ice_username_fragments, vec![]);
        assert_eq!(dump.tuple_hashes, vec![]);
    });
}

#[test]
fn unavailable_infos_fails() {
    future::block_on(async move {
        let (worker1, worker2) = init().await;

        let new_webrtc_server_count = Arc::new(AtomicUsize::new(0));

        let port1 = pick_unused_port().unwrap();
        let port2 = pick_unused_port().unwrap();
        worker1
            .on_new_webrtc_server({
                let new_webrtc_server_count = Arc::clone(&new_webrtc_server_count);

                move |_webrtc_server| {
                    new_webrtc_server_count.fetch_add(1, Ordering::SeqCst);
                }
            })
            .detach();

        // Using an unavailable listen IP.
        {
            let create_result = worker1
                .create_webrtc_server({
                    let listen_infos = WebRtcServerListenInfos::new(ListenInfo {
                        protocol: Protocol::Udp,
                        ip: IpAddr::V4(Ipv4Addr::LOCALHOST),
                        announced_address: None,
                        port: Some(port1),
                        port_range: None,
                        flags: None,
                        send_buffer_size: None,
                        recv_buffer_size: None,
                    });
                    let listen_infos = listen_infos.insert(ListenInfo {
                        protocol: Protocol::Udp,
                        ip: IpAddr::V4(Ipv4Addr::new(1, 2, 3, 4)),
                        announced_address: None,
                        port: Some(port2),
                        port_range: None,
                        flags: None,
                        send_buffer_size: None,
                        recv_buffer_size: None,
                    });

                    WebRtcServerOptions::new(listen_infos)
                })
                .await;

            assert!(matches!(
                create_result,
                Err(CreateWebRtcServerError::Request(_))
            ));
        }

        // Using the same UDP port twice.
        {
            let create_result = worker1
                .create_webrtc_server({
                    let listen_infos = WebRtcServerListenInfos::new(ListenInfo {
                        protocol: Protocol::Udp,
                        ip: IpAddr::V4(Ipv4Addr::LOCALHOST),
                        announced_address: None,
                        port: Some(port1),
                        port_range: None,
                        flags: None,
                        send_buffer_size: None,
                        recv_buffer_size: None,
                    });
                    let listen_infos = listen_infos.insert(ListenInfo {
                        protocol: Protocol::Udp,
                        ip: IpAddr::V4(Ipv4Addr::new(127, 0, 0, 1)),
                        announced_address: Some("1.2.3.4".to_string()),
                        port: Some(port1),
                        port_range: None,
                        flags: None,
                        send_buffer_size: None,
                        recv_buffer_size: None,
                    });

                    WebRtcServerOptions::new(listen_infos)
                })
                .await;

            assert!(matches!(
                create_result,
                Err(CreateWebRtcServerError::Request(_))
            ));
        }

        // Using the same UDP port in a second server.
        {
            let _webrtc_server = worker1
                .create_webrtc_server(WebRtcServerOptions::new(WebRtcServerListenInfos::new(
                    ListenInfo {
                        protocol: Protocol::Udp,
                        ip: IpAddr::V4(Ipv4Addr::LOCALHOST),
                        announced_address: None,
                        port: Some(port1),
                        port_range: None,
                        flags: None,
                        send_buffer_size: None,
                        recv_buffer_size: None,
                    },
                )))
                .await
                .expect("Failed to dump WebRTC server");

            let create_result = worker2
                .create_webrtc_server(WebRtcServerOptions::new(WebRtcServerListenInfos::new(
                    ListenInfo {
                        protocol: Protocol::Udp,
                        ip: IpAddr::V4(Ipv4Addr::LOCALHOST),
                        announced_address: None,
                        port: Some(port1),
                        port_range: None,
                        flags: None,
                        send_buffer_size: None,
                        recv_buffer_size: None,
                    },
                )))
                .await;

            assert!(matches!(
                create_result,
                Err(CreateWebRtcServerError::Request(_))
            ));
        }
    });
}

#[test]
fn close_event() {
    future::block_on(async move {
        let (worker1, _worker2) = init().await;

        let port = pick_unused_port().unwrap();

        let webrtc_server = worker1
            .create_webrtc_server(WebRtcServerOptions::new(WebRtcServerListenInfos::new(
                ListenInfo {
                    protocol: Protocol::Udp,
                    ip: IpAddr::V4(Ipv4Addr::LOCALHOST),
                    announced_address: None,
                    port: Some(port),
                    port_range: None,
                    flags: None,
                    send_buffer_size: None,
                    recv_buffer_size: None,
                },
            )))
            .await
            .expect("Failed to create WebRTC server");

        let (mut tx, rx) = async_oneshot::oneshot::<()>();
        let _handler = webrtc_server.on_close(move || {
            let _ = tx.send(());
        });
        drop(webrtc_server);

        rx.await.expect("Failed to receive close event");
    });
}
