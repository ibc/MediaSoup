use futures_lite::future;
use hash_hasher::HashedSet;
use mediasoup::data_consumer::DataConsumerOptions;
use mediasoup::data_producer::{DataProducer, DataProducerOptions};
use mediasoup::data_structures::{AppData, WebRtcMessage};
use mediasoup::direct_transport::{DirectTransport, DirectTransportOptions};
use mediasoup::prelude::*;
use mediasoup::router::{Router, RouterOptions};
use mediasoup::worker::{Worker, WorkerSettings};
use mediasoup::worker_manager::WorkerManager;
use parking_lot::Mutex;
use std::borrow::Cow;
use std::env;
use std::sync::atomic::{AtomicUsize, Ordering};
use std::sync::Arc;

struct CustomAppData {
    foo: &'static str,
}

async fn init() -> (Worker, Router, DirectTransport) {
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
        .create_direct_transport(DirectTransportOptions::default())
        .await
        .expect("Failed to create transport1");

    (worker, router, transport)
}

#[test]
fn create_succeeds() {
    future::block_on(async move {
        let (_worker, router, transport) = init().await;

        {
            let dump = router.dump().await.expect("Failed to dump router");

            assert_eq!(dump.transport_ids, {
                let mut set = HashedSet::default();
                set.insert(transport.id());
                set
            });
        }

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
            .create_direct_transport({
                let mut direct_transport_options = DirectTransportOptions::default();
                direct_transport_options.max_message_size = 1024;
                direct_transport_options.app_data = AppData::new(CustomAppData { foo: "bar" });

                direct_transport_options
            })
            .await
            .expect("Failed to create Direct transport");

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

        {
            let dump = transport1.dump().await.expect("Failed to dump transport");

            assert_eq!(dump.id, transport1.id());
            assert!(dump.direct);
            assert_eq!(dump.producer_ids, vec![]);
            assert_eq!(dump.consumer_ids, vec![]);
            assert_eq!(dump.data_producer_ids, vec![]);
            assert_eq!(dump.data_consumer_ids, vec![]);
        }
    });
}

#[test]
fn weak() {
    future::block_on(async move {
        let (_worker, _router, transport) = init().await;

        let weak_transport = transport.downgrade();

        assert!(weak_transport.upgrade().is_some());

        drop(transport);

        assert!(weak_transport.upgrade().is_none());
    });
}

#[test]
fn get_stats_succeeds() {
    future::block_on(async move {
        let (_worker, _router, transport) = init().await;

        let stats = transport
            .get_stats()
            .await
            .expect("Failed to get stats on Direct transport");

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
    });
}

#[test]
fn send_succeeds() {
    future::block_on(async move {
        let (_worker, _router, transport) = init().await;

        let data_producer = transport
            .produce_data({
                let mut options = DataProducerOptions::new_direct();

                options.label = "foo".to_string();
                options.protocol = "bar".to_string();
                options.app_data = AppData::new(CustomAppData { foo: "bar" });

                options
            })
            .await
            .expect("Failed to produce data");

        let data_consumer = transport
            .consume_data(DataConsumerOptions::new_direct(data_producer.id(), None))
            .await
            .expect("Failed to consume data");

        let num_messages = 200_usize;
        let pause_sending_at_message = 10_usize;
        let resume_sending_at_message = 20_usize;
        let pause_receiving_at_message = 40_usize;
        let resume_receiving_at_message = 60_usize;
        let expected_received_num_messages = num_messages
            - (resume_sending_at_message - pause_sending_at_message)
            - (resume_receiving_at_message - pause_receiving_at_message);

        let mut sent_message_bytes = 0_usize;
        let mut effectively_sent_message_bytes = 0_usize;
        let recv_message_bytes = Arc::new(AtomicUsize::new(0));
        let mut last_sent_message_id = 0_usize;
        let last_recv_message_id = Arc::new(AtomicUsize::new(0));

        let (received_messages_tx, received_messages_rx) = async_oneshot::oneshot::<()>();
        let _handler = data_consumer.on_message({
            let received_messages_tx = Mutex::new(Some(received_messages_tx));
            let recv_message_bytes = Arc::clone(&recv_message_bytes);
            let last_recv_message_id = Arc::clone(&last_recv_message_id);

            move |message| {
                let id: usize = match message {
                    WebRtcMessage::String(binary) => {
                        recv_message_bytes.fetch_add(binary.len(), Ordering::SeqCst);
                        String::from_utf8(binary.to_vec()).unwrap().parse().unwrap()
                    }
                    WebRtcMessage::Binary(binary) => {
                        recv_message_bytes.fetch_add(binary.len(), Ordering::SeqCst);
                        String::from_utf8(binary.to_vec()).unwrap().parse().unwrap()
                    }
                    WebRtcMessage::EmptyString => {
                        panic!("Unexpected empty message!");
                    }
                    WebRtcMessage::EmptyBinary => {
                        panic!("Unexpected empty message!");
                    }
                };

                if id < num_messages / 2 {
                    assert!(matches!(message, &WebRtcMessage::String(_)));
                } else {
                    assert!(matches!(message, &WebRtcMessage::Binary(_)));
                }

                last_recv_message_id.fetch_add(1, Ordering::SeqCst);

                if id == num_messages {
                    let _ = received_messages_tx.lock().take().unwrap().send(());
                }
            }
        });

        let direct_data_producer = match &data_producer {
            DataProducer::Direct(direct_data_producer) => direct_data_producer,
            _ => {
                panic!("Expected direct data producer")
            }
        };

        loop {
            last_sent_message_id += 1;
            let id = last_sent_message_id;

            if id == pause_sending_at_message {
                data_producer
                    .pause()
                    .await
                    .expect("Failed to pause data producer");
            } else if id == resume_sending_at_message {
                data_producer
                    .resume()
                    .await
                    .expect("Failed to resume data producer");
            } else if id == pause_receiving_at_message {
                data_consumer
                    .pause()
                    .await
                    .expect("Failed to pause data consumer");
            } else if id == resume_receiving_at_message {
                data_consumer
                    .resume()
                    .await
                    .expect("Failed to resume data consumer");
            }

            let message = if id < num_messages / 2 {
                let content = id.to_string().into_bytes();
                sent_message_bytes += content.len();

                if !data_producer.paused() && !data_consumer.paused() {
                    effectively_sent_message_bytes += content.len();
                }

                WebRtcMessage::String(Cow::from(content))
            } else {
                let content = id.to_string().into_bytes();
                sent_message_bytes += content.len();

                if !data_producer.paused() && !data_consumer.paused() {
                    effectively_sent_message_bytes += content.len();
                }

                WebRtcMessage::Binary(Cow::from(content))
            };

            direct_data_producer
                .send(message, None, None)
                .expect("Failed to send message");

            if id == num_messages {
                break;
            }
        }

        received_messages_rx
            .await
            .expect("Failed tor receive all messages");

        assert_eq!(last_sent_message_id, num_messages);
        assert_eq!(
            last_recv_message_id.load(Ordering::SeqCst),
            expected_received_num_messages
        );
        assert_eq!(
            recv_message_bytes.load(Ordering::SeqCst),
            effectively_sent_message_bytes,
        );

        {
            let stats = data_producer
                .get_stats()
                .await
                .expect("Failed to get stats on data producer");

            assert_eq!(stats.len(), 1);
            assert_eq!(&stats[0].label, data_producer.label());
            assert_eq!(&stats[0].protocol, data_producer.protocol());
            assert_eq!(stats[0].messages_received, num_messages as u64);
            assert_eq!(stats[0].bytes_received, sent_message_bytes as u64);
        }

        {
            let stats = data_consumer
                .get_stats()
                .await
                .expect("Failed to get stats on data consumer");

            assert_eq!(stats.len(), 1);
            assert_eq!(&stats[0].label, data_consumer.label());
            assert_eq!(&stats[0].protocol, data_consumer.protocol());
            assert_eq!(
                stats[0].messages_sent,
                expected_received_num_messages as u64
            );
            assert_eq!(
                stats[0].bytes_sent,
                recv_message_bytes.load(Ordering::SeqCst) as u64,
            );
        }
    });
}

#[test]
fn send_with_subchannels_succeeds() {
    future::block_on(async move {
        let (_worker, _router, transport) = init().await;

        let data_producer = transport
            .produce_data({
                let mut options = DataProducerOptions::new_direct();

                options.label = "foo".to_string();
                options.protocol = "bar".to_string();
                options.app_data = AppData::new(CustomAppData { foo: "bar" });

                options
            })
            .await
            .expect("Failed to produce data");

        let data_consumer_1 = transport
            .consume_data(DataConsumerOptions::new_direct(
                data_producer.id(),
                Some(vec![1, 11, 666]),
            ))
            .await
            .expect("Failed to consume data");

        let data_consumer_2 = transport
            .consume_data(DataConsumerOptions::new_direct(
                data_producer.id(),
                Some(vec![2, 22, 666]),
            ))
            .await
            .expect("Failed to consume data");

        let expected_received_num_messages_1 = 7;
        let expected_received_num_messages_2 = 5;
        let received_messages_1: Arc<Mutex<Vec<String>>> = Arc::new(Mutex::new(vec![]));
        let received_messages_2: Arc<Mutex<Vec<String>>> = Arc::new(Mutex::new(vec![]));

        let (received_messages_tx_1, received_messages_rx_1) = async_oneshot::oneshot::<()>();
        let _handler_1 = data_consumer_1.on_message({
            let received_messages_tx_1 = Mutex::new(Some(received_messages_tx_1));
            let received_messages_1 = Arc::clone(&received_messages_1);

            move |message| {
                let text: String = match message {
                    WebRtcMessage::String(binary) => String::from_utf8(binary.to_vec()).unwrap(),
                    _ => {
                        panic!("Unexpected empty message!");
                    }
                };

                received_messages_1.lock().push(text);

                if received_messages_1.lock().len() == expected_received_num_messages_1 {
                    let _ = received_messages_tx_1.lock().take().unwrap().send(());
                }
            }
        });

        let (received_messages_tx_2, received_messages_rx_2) = async_oneshot::oneshot::<()>();
        let _handler_2 = data_consumer_2.on_message({
            let received_messages_tx_2 = Mutex::new(Some(received_messages_tx_2));
            let received_messages_2 = Arc::clone(&received_messages_2);

            move |message| {
                let text: String = match message {
                    WebRtcMessage::String(binary) => String::from_utf8(binary.to_vec()).unwrap(),
                    _ => {
                        panic!("Unexpected empty message!");
                    }
                };

                received_messages_2.lock().push(text);

                if received_messages_2.lock().len() == expected_received_num_messages_2 {
                    let _ = received_messages_tx_2.lock().take().unwrap().send(());
                }
            }
        });

        let direct_data_producer = match &data_producer {
            DataProducer::Direct(direct_data_producer) => direct_data_producer,
            _ => {
                panic!("Expected direct data producer")
            }
        };

        let _ = match &data_consumer_2 {
            DataConsumer::Direct(direct_data_consumer) => direct_data_consumer,
            _ => {
                panic!("Expected direct data consumer")
            }
        };

        let both: &'static str = "both";
        let none: &'static str = "none";
        let dc1: &'static str = "dc1";
        let dc2: &'static str = "dc2";

        // Must be received by dataConsumer1 and dataConsumer2.
        direct_data_producer
            .send(
                WebRtcMessage::String(Cow::Borrowed(both.as_bytes())),
                None,
                None,
            )
            .expect("Failed to send message");

        // Must be received by dataConsumer1 and dataConsumer2.
        direct_data_producer
            .send(
                WebRtcMessage::String(Cow::Borrowed(both.as_bytes())),
                Some(vec![1, 2]),
                None,
            )
            .expect("Failed to send message");

        // Must be received by dataConsumer1 and dataConsumer2.
        direct_data_producer
            .send(
                WebRtcMessage::String(Cow::Borrowed(both.as_bytes())),
                Some(vec![11, 22, 33]),
                Some(666),
            )
            .expect("Failed to send message");

        // Must not be received by neither dataConsumer1 nor dataConsumer2.
        direct_data_producer
            .send(
                WebRtcMessage::String(Cow::Borrowed(none.as_bytes())),
                Some(vec![3]),
                Some(666),
            )
            .expect("Failed to send message");

        // Must not be received by neither dataConsumer1 nor dataConsumer2.
        direct_data_producer
            .send(
                WebRtcMessage::String(Cow::Borrowed(none.as_bytes())),
                Some(vec![666]),
                Some(3),
            )
            .expect("Failed to send message");

        // Must be received by dataConsumer1.
        direct_data_producer
            .send(
                WebRtcMessage::String(Cow::Borrowed(dc1.as_bytes())),
                Some(vec![1]),
                None,
            )
            .expect("Failed to send message");

        // Must be received by dataConsumer1.
        direct_data_producer
            .send(
                WebRtcMessage::String(Cow::Borrowed(dc1.as_bytes())),
                Some(vec![11]),
                None,
            )
            .expect("Failed to send message");

        // Must be received by dataConsumer1.
        direct_data_producer
            .send(
                WebRtcMessage::String(Cow::Borrowed(dc1.as_bytes())),
                Some(vec![666]),
                Some(11),
            )
            .expect("Failed to send message");

        // Must be received by dataConsumer2.
        direct_data_producer
            .send(
                WebRtcMessage::String(Cow::Borrowed(dc2.as_bytes())),
                Some(vec![666]),
                Some(2),
            )
            .expect("Failed to send message");

        let mut subchannels = data_consumer_2.subchannels();
        subchannels.push(1);

        data_consumer_2
            .set_subchannels(subchannels)
            .await
            .expect("Failed to set subchannels");

        // Must be received by dataConsumer2.
        direct_data_producer
            .send(
                WebRtcMessage::String(Cow::Borrowed(both.as_bytes())),
                Some(vec![1]),
                Some(666),
            )
            .expect("Failed to send message");

        received_messages_rx_1
            .await
            .expect("Failed tor receive all messages");

        received_messages_rx_2
            .await
            .expect("Failed tor receive all messages");

        let received_messages_1 = received_messages_1.lock().clone();
        assert!(received_messages_1.contains(&both.to_string()));
        assert!(received_messages_1.contains(&dc1.to_string()));
        assert!(!received_messages_1.contains(&dc2.to_string()));

        let received_messages_2 = received_messages_2.lock().clone();
        assert!(received_messages_2.contains(&both.to_string()));
        assert!(!received_messages_2.contains(&dc1.to_string()));
        assert!(received_messages_2.contains(&dc2.to_string()));
    });
}

#[test]
fn close_event() {
    future::block_on(async move {
        let (_worker, _router, transport) = init().await;

        let (mut close_tx, close_rx) = async_oneshot::oneshot::<()>();
        let _handler = transport.on_close(Box::new(move || {
            let _ = close_tx.send(());
        }));

        drop(transport);

        close_rx.await.expect("Failed to receive close event");
    });
}
