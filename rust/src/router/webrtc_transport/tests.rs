use crate::data_structures::{
    IceCandidateTcpType, IceCandidateType, IceState, ListenInfo, Protocol,
};
use crate::prelude::WebRtcTransport;
use crate::router::{NewTransport, Router, RouterOptions};
use crate::transport::Transport;
use crate::webrtc_server::{WebRtcServerIpPort, WebRtcServerListenInfos, WebRtcServerOptions};
use crate::webrtc_transport::{WebRtcTransportListenInfos, WebRtcTransportOptions};
use crate::worker::{Worker, WorkerSettings};
use crate::worker_manager::WorkerManager;
use async_io::Timer;
use futures_lite::future;
use hash_hasher::HashedSet;
use parking_lot::Mutex;
use portpicker::pick_unused_port;
use std::env;
use std::net::{IpAddr, Ipv4Addr};
use std::sync::Arc;
use std::time::Duration;

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
        .create_router(RouterOptions::default())
        .await
        .expect("Failed to create router");

    (worker, router)
}

#[test]
fn create_with_webrtc_server_succeeds() {
    future::block_on(async move {
        let (worker, router) = init().await;

        let port1 = pick_unused_port().unwrap();
        let port2 = pick_unused_port().unwrap();

        let webrtc_server = worker
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
                    announced_address: None,
                    port: Some(port2),
                    port_range: None,
                    flags: None,
                    send_buffer_size: None,
                    recv_buffer_size: None,
                });
                WebRtcServerOptions::new(listen_infos)
            })
            .await
            .expect("Failed to create WebRTC server");

        let (new_server_transport_tx, new_server_transport_rx) =
            async_oneshot::oneshot::<WebRtcTransport>();
        let _handler = webrtc_server.on_new_webrtc_transport({
            let new_server_transport_tx = Arc::new(Mutex::new(Some(new_server_transport_tx)));

            move |transport| {
                let _ = new_server_transport_tx
                    .lock()
                    .take()
                    .unwrap()
                    .send(transport.clone());
            }
        });

        let (new_router_transport_tx, new_router_transport_rx) =
            async_oneshot::oneshot::<WebRtcTransport>();
        let _handler = router.on_new_transport({
            let new_router_transport_tx = Arc::new(Mutex::new(Some(new_router_transport_tx)));

            move |transport| {
                if let NewTransport::WebRtc(transport) = transport {
                    let _ = new_router_transport_tx
                        .lock()
                        .take()
                        .unwrap()
                        .send(transport.clone());
                }
            }
        });

        let transport = router
            .create_webrtc_transport({
                let mut webrtc_transport_options =
                    WebRtcTransportOptions::new_with_server(webrtc_server.clone());
                // Let's disable UDP so resulting ICE candidates should only contain TCP.
                webrtc_transport_options.enable_udp = false;

                webrtc_transport_options
            })
            .await
            .expect("Failed to create WebRTC transport");

        let router_dump = router.dump().await.expect("Failed to dump router");
        assert_eq!(router_dump.transport_ids, {
            let mut set = HashedSet::default();
            set.insert(transport.id());
            set
        });

        assert_eq!(new_server_transport_rx.await.unwrap().id(), transport.id());
        assert_eq!(new_router_transport_rx.await.unwrap().id(), transport.id());

        assert!(!transport.closed());

        {
            let ice_candidates = transport.ice_candidates();
            assert_eq!(ice_candidates.len(), 1);
            assert_eq!(ice_candidates[0].address, "127.0.0.1");
            assert_eq!(ice_candidates[0].protocol, Protocol::Tcp);
            assert_eq!(ice_candidates[0].port, port2);
            assert_eq!(ice_candidates[0].r#type, IceCandidateType::Host);
            assert_eq!(
                ice_candidates[0].tcp_type,
                Some(IceCandidateTcpType::Passive)
            );
        }

        assert_eq!(transport.ice_state(), IceState::New);
        assert_eq!(transport.ice_selected_tuple(), None);

        {
            let webrtc_server_dump = webrtc_server
                .dump()
                .await
                .expect("Failed to dump WebRTC server");

            assert_eq!(webrtc_server_dump.id, webrtc_server.id());
            assert_eq!(
                webrtc_server_dump.udp_sockets,
                vec![WebRtcServerIpPort {
                    ip: IpAddr::V4(Ipv4Addr::LOCALHOST),
                    port: port1
                }]
            );
            assert_eq!(
                webrtc_server_dump.tcp_servers,
                vec![WebRtcServerIpPort {
                    ip: IpAddr::V4(Ipv4Addr::LOCALHOST),
                    port: port2
                }]
            );
            assert_eq!(webrtc_server_dump.webrtc_transport_ids, {
                let mut set = HashedSet::default();
                set.insert(transport.id());
                set
            });
            assert_eq!(webrtc_server_dump.local_ice_username_fragments.len(), 1);
            assert_eq!(
                webrtc_server_dump.local_ice_username_fragments[0].webrtc_transport_id,
                transport.id()
            );
            assert_eq!(webrtc_server_dump.tuple_hashes, vec![]);
        }

        {
            let (mut tx, rx) = async_oneshot::oneshot::<()>();
            let _handler = transport.on_close(Box::new(move || {
                let _ = tx.send(());
            }));
            drop(transport);

            rx.await.expect("Failed to receive close event");
        }

        // Drop is async, give consumer a bit of time to finish
        Timer::after(Duration::from_millis(200)).await;

        {
            let webrtc_server_dump = webrtc_server
                .dump()
                .await
                .expect("Failed to dump WebRTC server");

            assert_eq!(webrtc_server_dump.id, webrtc_server.id());
            assert_eq!(
                webrtc_server_dump.udp_sockets,
                vec![WebRtcServerIpPort {
                    ip: IpAddr::V4(Ipv4Addr::LOCALHOST),
                    port: port1
                }]
            );
            assert_eq!(
                webrtc_server_dump.tcp_servers,
                vec![WebRtcServerIpPort {
                    ip: IpAddr::V4(Ipv4Addr::LOCALHOST),
                    port: port2
                }]
            );
            assert_eq!(
                webrtc_server_dump.webrtc_transport_ids,
                HashedSet::default()
            );
            assert_eq!(webrtc_server_dump.local_ice_username_fragments, vec![]);
            assert_eq!(webrtc_server_dump.tuple_hashes, vec![]);
        }
    });
}

#[test]
fn router_close_event() {
    future::block_on(async move {
        let (_worker, router) = init().await;

        let transport = router
            .create_webrtc_transport(WebRtcTransportOptions::new(
                WebRtcTransportListenInfos::new(ListenInfo {
                    protocol: Protocol::Udp,
                    ip: IpAddr::V4(Ipv4Addr::LOCALHOST),
                    announced_address: Some("9.9.9.1".to_string()),
                    port: None,
                    port_range: None,
                    flags: None,
                    send_buffer_size: None,
                    recv_buffer_size: None,
                }),
            ))
            .await
            .expect("Failed to create WebRTC transport");

        let (mut close_tx, close_rx) = async_oneshot::oneshot::<()>();
        let _handler = transport.on_close(Box::new(move || {
            let _ = close_tx.send(());
        }));

        let (mut router_close_tx, router_close_rx) = async_oneshot::oneshot::<()>();
        let _handler = transport.on_router_close(Box::new(move || {
            let _ = router_close_tx.send(());
        }));

        router.close();

        router_close_rx
            .await
            .expect("Failed to receive router_close event");
        close_rx.await.expect("Failed to receive close event");
    });
}

#[test]
fn webrtc_server_close_event() {
    future::block_on(async move {
        let (worker, router) = init().await;

        let port1 = pick_unused_port().unwrap();
        let port2 = pick_unused_port().unwrap();

        let webrtc_server = worker
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
                    announced_address: None,
                    port: Some(port2),
                    port_range: None,
                    flags: None,
                    send_buffer_size: None,
                    recv_buffer_size: None,
                });
                WebRtcServerOptions::new(listen_infos)
            })
            .await
            .expect("Failed to create WebRTC server");

        let transport = router
            .create_webrtc_transport(WebRtcTransportOptions::new_with_server(
                webrtc_server.clone(),
            ))
            .await
            .expect("Failed to create WebRTC transport");

        let (mut close_tx, close_rx) = async_oneshot::oneshot::<()>();
        let _handler = transport.on_close(Box::new(move || {
            let _ = close_tx.send(());
        }));

        let (mut webrtc_server_close_tx, webrtc_server_close_rx) = async_oneshot::oneshot::<()>();
        let _handler = transport.on_webrtc_server_close(Box::new(move || {
            let _ = webrtc_server_close_tx.send(());
        }));

        webrtc_server.close();

        webrtc_server_close_rx
            .await
            .expect("Failed to receive webrtc_server_close event");
        close_rx.await.expect("Failed to receive close event");
    });
}
