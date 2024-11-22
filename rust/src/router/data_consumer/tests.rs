use crate::data_consumer::DataConsumerOptions;
use crate::data_producer::{DataProducer, DataProducerOptions};
use crate::data_structures::{ListenInfo, Protocol};
use crate::plain_transport::PlainTransportOptions;
use crate::router::{Router, RouterOptions};
use crate::sctp_parameters::SctpStreamParameters;
use crate::transport::Transport;
use crate::webrtc_transport::{WebRtcTransportListenInfos, WebRtcTransportOptions};
use crate::worker::WorkerSettings;
use crate::worker_manager::WorkerManager;
use futures_lite::future;
use std::env;
use std::net::{IpAddr, Ipv4Addr};

async fn init() -> (Router, DataProducer) {
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

    let transport = router
        .create_webrtc_transport({
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

            transport_options
        })
        .await
        .expect("Failed to create transport1");

    let data_producer = transport
        .produce_data(DataProducerOptions::new_sctp(
            SctpStreamParameters::new_unordered_with_life_time(12345, 5000),
        ))
        .await
        .expect("Failed to create data producer");

    (router, data_producer)
}

#[test]
fn data_producer_close_event() {
    future::block_on(async move {
        let (router, data_producer) = init().await;

        let transport2 = router
            .create_plain_transport({
                let mut transport_options = PlainTransportOptions::new(ListenInfo {
                    protocol: Protocol::Udp,
                    ip: IpAddr::V4(Ipv4Addr::LOCALHOST),
                    announced_address: None,
                    port: None,
                    port_range: None,
                    flags: None,
                    send_buffer_size: None,
                    recv_buffer_size: None,
                });

                transport_options.enable_sctp = true;

                transport_options
            })
            .await
            .expect("Failed to create transport1");

        let data_consumer = transport2
            .consume_data(DataConsumerOptions::new_sctp_unordered_with_life_time(
                data_producer.id(),
                4000,
            ))
            .await
            .expect("Failed to consume data");

        let (mut close_tx, close_rx) = async_oneshot::oneshot::<()>();
        let _handler = data_consumer.on_close(move || {
            let _ = close_tx.send(());
        });

        let (mut data_producer_close_tx, data_producer_close_rx) = async_oneshot::oneshot::<()>();
        let _handler = data_consumer.on_data_producer_close(move || {
            let _ = data_producer_close_tx.send(());
        });

        drop(data_producer);

        data_producer_close_rx
            .await
            .expect("Failed to receive data_producer_close event");

        close_rx.await.expect("Failed to receive close event");

        assert!(data_consumer.closed());
    });
}

#[test]
fn transport_close_event() {
    future::block_on(async move {
        let (router, data_producer) = init().await;

        let transport2 = router
            .create_plain_transport({
                let mut transport_options = PlainTransportOptions::new(ListenInfo {
                    protocol: Protocol::Udp,
                    ip: IpAddr::V4(Ipv4Addr::LOCALHOST),
                    announced_address: None,
                    port: None,
                    port_range: None,
                    flags: None,
                    send_buffer_size: None,
                    recv_buffer_size: None,
                });

                transport_options.enable_sctp = true;

                transport_options
            })
            .await
            .expect("Failed to create transport1");

        let data_consumer = transport2
            .consume_data(DataConsumerOptions::new_sctp_unordered_with_life_time(
                data_producer.id(),
                4000,
            ))
            .await
            .expect("Failed to consume data");

        let (mut close_tx, close_rx) = async_oneshot::oneshot::<()>();
        let _handler = data_consumer.on_close(move || {
            let _ = close_tx.send(());
        });

        let (mut transport_close_tx, transport_close_rx) = async_oneshot::oneshot::<()>();
        let _handler = data_consumer.on_transport_close(move || {
            let _ = transport_close_tx.send(());
        });

        router.close();

        transport_close_rx
            .await
            .expect("Failed to receive transport_close event");
        close_rx.await.expect("Failed to receive close event");

        assert!(data_consumer.closed());
    });
}
