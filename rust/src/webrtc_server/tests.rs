use crate::data_structures::{ListenInfo, Protocol};
use crate::webrtc_server::{WebRtcServerListenInfos, WebRtcServerOptions};
use crate::worker::{Worker, WorkerSettings};
use crate::worker_manager::WorkerManager;
use futures_lite::future;
use portpicker::pick_unused_port;
use std::env;
use std::net::{IpAddr, Ipv4Addr};

async fn init() -> Worker {
    {
        let mut builder = env_logger::builder();
        if env::var(env_logger::DEFAULT_FILTER_ENV).is_err() {
            builder.filter_level(log::LevelFilter::Off);
        }
        let _ = builder.is_test(true).try_init();
    }

    let worker_manager = WorkerManager::new();

    worker_manager
        .create_worker(WorkerSettings::default())
        .await
        .expect("Failed to create worker")
}

#[test]
fn worker_close_event() {
    future::block_on(async move {
        let worker = init().await;

        let port = pick_unused_port().unwrap();

        let webrtc_server = worker
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

        let (mut close_tx, close_rx) = async_oneshot::oneshot::<()>();
        let _handler = webrtc_server.on_close(move || {
            let _ = close_tx.send(());
        });

        let (mut worker_close_tx, worker_close_rx) = async_oneshot::oneshot::<()>();
        let _handler = webrtc_server.on_worker_close(move || {
            let _ = worker_close_tx.send(());
        });

        worker.close();

        worker_close_rx
            .await
            .expect("Failed to receive worker_close event");
        close_rx.await.expect("Failed to receive close event");

        assert!(webrtc_server.closed());
    });
}
