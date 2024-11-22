use async_io::Timer;
use futures_lite::future;
use hash_hasher::{HashedMap, HashedSet};
use mediasoup::data_producer::{DataProducerOptions, DataProducerType};
use mediasoup::data_structures::{AppData, ListenInfo, Protocol};
use mediasoup::plain_transport::{PlainTransport, PlainTransportOptions};
use mediasoup::prelude::*;
use mediasoup::router::{Router, RouterOptions};
use mediasoup::sctp_parameters::SctpStreamParameters;
use mediasoup::transport::ProduceDataError;
use mediasoup::webrtc_transport::{
    WebRtcTransport, WebRtcTransportListenInfos, WebRtcTransportOptions,
};
use mediasoup::worker::{RequestError, Worker, WorkerSettings};
use mediasoup::worker_manager::WorkerManager;
use std::env;
use std::net::{IpAddr, Ipv4Addr};
use std::sync::atomic::{AtomicUsize, Ordering};
use std::sync::Arc;
use std::time::Duration;

#[derive(Debug, PartialEq)]
struct CustomAppData {
    foo: u8,
    baz: &'static str,
}

async fn init() -> (Worker, Router, WebRtcTransport, PlainTransport) {
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

    let transport1 = router
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

    (worker, router, transport1, transport2)
}

#[test]
fn transport_1_produce_data_succeeds() {
    future::block_on(async move {
        let (_worker, router, transport1, _transport2) = init().await;

        let new_data_producer_count = Arc::new(AtomicUsize::new(0));

        transport1
            .on_new_data_producer({
                let new_data_producer_count = Arc::clone(&new_data_producer_count);

                Arc::new(move |_data_producer| {
                    new_data_producer_count.fetch_add(1, Ordering::SeqCst);
                })
            })
            .detach();

        let data_producer1 = transport1
            .produce_data({
                let mut options =
                    DataProducerOptions::new_sctp(SctpStreamParameters::new_ordered(666));

                options.label = "foo".to_string();
                options.protocol = "bar".to_string();
                options.app_data = AppData::new(CustomAppData { foo: 1, baz: "2" });

                options
            })
            .await
            .expect("Failed to produce data");

        assert_eq!(new_data_producer_count.load(Ordering::SeqCst), 1);
        assert!(!data_producer1.closed());
        assert_eq!(data_producer1.r#type(), DataProducerType::Sctp);
        {
            let sctp_stream_parameters = data_producer1.sctp_stream_parameters();
            assert!(sctp_stream_parameters.is_some());
            assert_eq!(sctp_stream_parameters.unwrap().stream_id(), 666);
            assert!(sctp_stream_parameters.unwrap().ordered());
            assert_eq!(sctp_stream_parameters.unwrap().max_packet_life_time(), None);
            assert_eq!(sctp_stream_parameters.unwrap().max_retransmits(), None);
        }
        assert_eq!(data_producer1.label().as_str(), "foo");
        assert_eq!(data_producer1.protocol().as_str(), "bar");
        assert!(!data_producer1.paused());
        assert_eq!(
            data_producer1
                .app_data()
                .downcast_ref::<CustomAppData>()
                .unwrap(),
            &CustomAppData { foo: 1, baz: "2" },
        );

        {
            let dump = router.dump().await.expect("Failed to dump router");

            assert_eq!(dump.map_data_producer_id_data_consumer_ids, {
                let mut map = HashedMap::default();
                map.insert(data_producer1.id(), HashedSet::default());
                map
            });
            assert_eq!(
                dump.map_data_consumer_id_data_producer_id,
                HashedMap::default()
            );
        }

        {
            let dump = transport1.dump().await.expect("Failed to dump transport");

            assert_eq!(dump.id, transport1.id());
            assert_eq!(dump.data_producer_ids, vec![data_producer1.id()]);
            assert_eq!(dump.data_consumer_ids, vec![]);
        }
    });
}

#[test]
fn transport_2_produce_data_succeeds() {
    future::block_on(async move {
        let (_worker, router, _transport1, transport2) = init().await;

        let new_data_producer_count = Arc::new(AtomicUsize::new(0));

        transport2
            .on_new_data_producer({
                let new_data_producer_count = Arc::clone(&new_data_producer_count);

                Arc::new(move |_data_producer| {
                    new_data_producer_count.fetch_add(1, Ordering::SeqCst);
                })
            })
            .detach();

        let data_producer2 = transport2
            .produce_data({
                let mut options = DataProducerOptions::new_sctp(
                    SctpStreamParameters::new_unordered_with_retransmits(777, 3),
                );

                options.label = "foo".to_string();
                options.protocol = "bar".to_string();
                options.paused = true;
                options.app_data = AppData::new(CustomAppData { foo: 1, baz: "2" });

                options
            })
            .await
            .expect("Failed to produce data");

        assert_eq!(new_data_producer_count.load(Ordering::SeqCst), 1);
        assert!(!data_producer2.closed());
        assert_eq!(data_producer2.r#type(), DataProducerType::Sctp);
        {
            let sctp_stream_parameters = data_producer2.sctp_stream_parameters();
            assert!(sctp_stream_parameters.is_some());
            assert_eq!(sctp_stream_parameters.unwrap().stream_id(), 777);
            assert!(!sctp_stream_parameters.unwrap().ordered());
            assert_eq!(sctp_stream_parameters.unwrap().max_packet_life_time(), None);
            assert_eq!(sctp_stream_parameters.unwrap().max_retransmits(), Some(3));
        }
        assert_eq!(data_producer2.label().as_str(), "foo");
        assert_eq!(data_producer2.protocol().as_str(), "bar");
        assert!(data_producer2.paused());
        assert_eq!(
            data_producer2
                .app_data()
                .downcast_ref::<CustomAppData>()
                .unwrap(),
            &CustomAppData { foo: 1, baz: "2" },
        );

        {
            let dump = router.dump().await.expect("Failed to dump router");

            assert_eq!(dump.map_data_producer_id_data_consumer_ids, {
                let mut map = HashedMap::default();
                map.insert(data_producer2.id(), HashedSet::default());
                map
            });
            assert_eq!(
                dump.map_data_consumer_id_data_producer_id,
                HashedMap::default()
            );
        }

        {
            let dump = transport2.dump().await.expect("Failed to dump transport");

            assert_eq!(dump.id, transport2.id());
            assert_eq!(dump.data_producer_ids, vec![data_producer2.id()]);
            assert_eq!(dump.data_consumer_ids, vec![]);
        }
    });
}

#[test]
fn weak() {
    future::block_on(async move {
        let (_worker, _router, transport1, _transport2) = init().await;

        let data_producer = transport1
            .produce_data({
                let mut options =
                    DataProducerOptions::new_sctp(SctpStreamParameters::new_ordered(666));

                options.label = "foo".to_string();
                options.protocol = "bar".to_string();
                options.app_data = AppData::new(CustomAppData { foo: 1, baz: "2" });

                options
            })
            .await
            .expect("Failed to produce data");

        let weak_data_producer = data_producer.downgrade();

        assert!(weak_data_producer.upgrade().is_some());

        drop(data_producer);

        assert!(weak_data_producer.upgrade().is_none());
    });
}

#[test]
fn produce_data_used_stream_id_rejects() {
    future::block_on(async move {
        let (_worker, _router, transport1, _transport2) = init().await;

        let _data_producer1 = transport1
            .produce_data(DataProducerOptions::new_sctp(
                SctpStreamParameters::new_ordered(666),
            ))
            .await
            .expect("Failed to produce data");

        assert!(matches!(
            transport1
                .produce_data(DataProducerOptions::new_sctp(
                    SctpStreamParameters::new_ordered(666),
                ))
                .await,
            Err(ProduceDataError::Request(RequestError::Response { .. })),
        ));
    });
}

#[test]
fn dump_succeeds() {
    future::block_on(async move {
        let (_worker, _router, transport1, transport2) = init().await;

        {
            let data_producer1 = transport1
                .produce_data({
                    let mut options =
                        DataProducerOptions::new_sctp(SctpStreamParameters::new_ordered(666));

                    options.label = "foo".to_string();
                    options.protocol = "bar".to_string();
                    options.app_data = AppData::new(CustomAppData { foo: 1, baz: "2" });

                    options
                })
                .await
                .expect("Failed to produce data");

            let dump = data_producer1
                .dump()
                .await
                .expect("Data producer dump failed");

            assert_eq!(dump.id, data_producer1.id());
            assert_eq!(dump.r#type, DataProducerType::Sctp);
            {
                let sctp_stream_parameters = dump.sctp_stream_parameters;
                assert!(sctp_stream_parameters.is_some());
                assert_eq!(sctp_stream_parameters.unwrap().stream_id(), 666);
                assert!(sctp_stream_parameters.unwrap().ordered());
                assert_eq!(sctp_stream_parameters.unwrap().max_packet_life_time(), None);
                assert_eq!(sctp_stream_parameters.unwrap().max_retransmits(), None);
            }
            assert_eq!(dump.label.as_str(), "foo");
            assert_eq!(dump.protocol.as_str(), "bar");
            assert!(!dump.paused);
        }

        {
            let data_producer2 = transport2
                .produce_data({
                    let mut options = DataProducerOptions::new_sctp(
                        SctpStreamParameters::new_unordered_with_retransmits(777, 3),
                    );

                    options.label = "foo".to_string();
                    options.protocol = "bar".to_string();
                    options.paused = true;
                    options.app_data = AppData::new(CustomAppData { foo: 1, baz: "2" });

                    options
                })
                .await
                .expect("Failed to produce data");

            let dump = data_producer2
                .dump()
                .await
                .expect("Data producer dump failed");

            assert_eq!(dump.id, data_producer2.id());
            assert_eq!(dump.r#type, DataProducerType::Sctp);
            {
                let sctp_stream_parameters = dump.sctp_stream_parameters;
                assert!(sctp_stream_parameters.is_some());
                assert_eq!(sctp_stream_parameters.unwrap().stream_id(), 777);
                assert!(!sctp_stream_parameters.unwrap().ordered());
                assert_eq!(sctp_stream_parameters.unwrap().max_packet_life_time(), None);
                assert_eq!(sctp_stream_parameters.unwrap().max_retransmits(), Some(3));
            }
            assert_eq!(dump.label.as_str(), "foo");
            assert_eq!(dump.protocol.as_str(), "bar");
            assert!(dump.paused);
        }
    });
}

#[test]
fn get_stats_succeeds() {
    future::block_on(async move {
        let (_worker, _router, transport1, transport2) = init().await;

        {
            let data_producer1 = transport1
                .produce_data({
                    let mut options =
                        DataProducerOptions::new_sctp(SctpStreamParameters::new_ordered(666));

                    options.label = "foo".to_string();
                    options.protocol = "bar".to_string();
                    options.app_data = AppData::new(CustomAppData { foo: 1, baz: "2" });

                    options
                })
                .await
                .expect("Failed to produce data");

            let stats = data_producer1
                .get_stats()
                .await
                .expect("Failed to get data producer stats");

            assert_eq!(stats.len(), 1);
            assert_eq!(&stats[0].label, data_producer1.label());
            assert_eq!(&stats[0].protocol, data_producer1.protocol());
            assert_eq!(stats[0].messages_received, 0);
            assert_eq!(stats[0].bytes_received, 0);
        }

        {
            let data_producer2 = transport2
                .produce_data({
                    let mut options = DataProducerOptions::new_sctp(
                        SctpStreamParameters::new_unordered_with_retransmits(777, 3),
                    );

                    options.label = "foo".to_string();
                    options.protocol = "bar".to_string();
                    options.app_data = AppData::new(CustomAppData { foo: 1, baz: "2" });

                    options
                })
                .await
                .expect("Failed to produce data");

            let stats = data_producer2
                .get_stats()
                .await
                .expect("Failed to get data producer stats");

            assert_eq!(stats.len(), 1);
            assert_eq!(&stats[0].label, data_producer2.label());
            assert_eq!(&stats[0].protocol, data_producer2.protocol());
            assert_eq!(stats[0].messages_received, 0);
            assert_eq!(stats[0].bytes_received, 0);
        }
    });
}

#[test]
fn pause_and_resume_succeed() {
    future::block_on(async move {
        let (_worker, _router, transport1, _) = init().await;

        {
            let data_producer1 = transport1
                .produce_data({
                    let mut options =
                        DataProducerOptions::new_sctp(SctpStreamParameters::new_ordered(666));

                    options.label = "foo".to_string();
                    options.protocol = "bar".to_string();
                    options.app_data = AppData::new(CustomAppData { foo: 1, baz: "2" });

                    options
                })
                .await
                .expect("Failed to produce data");

            {
                data_producer1
                    .pause()
                    .await
                    .expect("Failed to pause data producer");

                assert!(data_producer1.paused());

                let dump = data_producer1
                    .dump()
                    .await
                    .expect("Failed to dump data producer");

                assert!(dump.paused);
            }

            {
                data_producer1
                    .resume()
                    .await
                    .expect("Failed to resume data producer");

                assert!(!data_producer1.paused());

                let dump = data_producer1
                    .dump()
                    .await
                    .expect("Failed to dump data producer");

                assert!(!dump.paused);
            }
        }
    });
}

#[test]
fn close_event() {
    future::block_on(async move {
        let (_worker, router, transport1, _transport2) = init().await;

        let data_producer = transport1
            .produce_data({
                let mut options =
                    DataProducerOptions::new_sctp(SctpStreamParameters::new_ordered(666));

                options.label = "foo".to_string();
                options.protocol = "bar".to_string();
                options.app_data = AppData::new(CustomAppData { foo: 1, baz: "2" });

                options
            })
            .await
            .expect("Failed to produce data");

        {
            let (mut tx, rx) = async_oneshot::oneshot::<()>();
            let _handler = data_producer.on_close(move || {
                let _ = tx.send(());
            });
            drop(data_producer);

            rx.await.expect("Failed to receive close event");
        }

        // Drop is async, give consumer a bit of time to finish
        Timer::after(Duration::from_millis(200)).await;

        {
            let dump = router.dump().await.expect("Failed to dump router");

            assert_eq!(
                dump.map_data_producer_id_data_consumer_ids,
                HashedMap::default()
            );
            assert_eq!(
                dump.map_data_consumer_id_data_producer_id,
                HashedMap::default()
            );
        }

        {
            let dump = transport1.dump().await.expect("Failed to dump transport");

            assert_eq!(dump.data_producer_ids, vec![]);
            assert_eq!(dump.data_consumer_ids, vec![]);
        }
    });
}
