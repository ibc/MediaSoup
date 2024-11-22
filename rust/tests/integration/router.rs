use futures_lite::future;
use hash_hasher::{HashedMap, HashedSet};
use mediasoup::data_structures::AppData;
use mediasoup::router::RouterOptions;
use mediasoup::rtp_parameters::{
    MimeTypeAudio, MimeTypeVideo, RtpCodecCapability, RtpCodecParametersParameters,
};
use mediasoup::worker::{ChannelMessageHandlers, Worker, WorkerSettings};
use mediasoup::worker_manager::WorkerManager;
use std::env;
use std::num::{NonZeroU32, NonZeroU8};
use std::sync::atomic::{AtomicUsize, Ordering};
use std::sync::Arc;

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
            ]),
            rtcp_feedback: vec![], // Will be ignored.
        },
    ]
}

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
fn create_router_succeeds() {
    future::block_on(async move {
        let worker = init().await;

        let new_router_count = Arc::new(AtomicUsize::new(0));

        worker
            .on_new_router({
                let new_router_count = Arc::clone(&new_router_count);

                move |_router| {
                    new_router_count.fetch_add(1, Ordering::SeqCst);
                }
            })
            .detach();

        #[derive(Debug, PartialEq)]
        struct CustomAppData {
            foo: u32,
        }

        let router = worker
            .create_router({
                let mut router_options = RouterOptions::new(media_codecs());

                router_options.app_data = AppData::new(CustomAppData { foo: 123 });

                router_options
            })
            .await
            .expect("Failed to create router");

        assert_eq!(new_router_count.load(Ordering::SeqCst), 1);
        assert!(!router.closed());
        assert_eq!(
            router.app_data().downcast_ref::<CustomAppData>(),
            Some(&CustomAppData { foo: 123 }),
        );

        let worker_dump = worker.dump().await.expect("Failed to dump worker");

        assert_eq!(worker_dump.router_ids, vec![router.id()]);
        assert_eq!(worker_dump.webrtc_server_ids, vec![]);
        assert_eq!(
            worker_dump.channel_message_handlers,
            ChannelMessageHandlers {
                channel_request_handlers: vec![router.id().into()],
                channel_notification_handlers: vec![]
            }
        );

        let dump = router.dump().await.expect("Failed to dump router");

        assert_eq!(dump.id, router.id());
        assert_eq!(dump.transport_ids, HashedSet::default());
        assert_eq!(dump.rtp_observer_ids, HashedSet::default());
        assert_eq!(dump.map_producer_id_consumer_ids, HashedMap::default());
        assert_eq!(dump.map_consumer_id_producer_id, HashedMap::default());
        assert_eq!(dump.map_producer_id_observer_ids, HashedMap::default());
        assert_eq!(
            dump.map_data_producer_id_data_consumer_ids,
            HashedMap::default()
        );
        assert_eq!(
            dump.map_data_consumer_id_data_producer_id,
            HashedMap::default()
        );
    });
}

#[test]
fn close_event() {
    future::block_on(async move {
        let worker = init().await;

        let router = worker
            .create_router(RouterOptions::new(media_codecs()))
            .await
            .expect("Failed to create router");

        let (mut tx, rx) = async_oneshot::oneshot::<()>();
        let _handler = router.on_close(move || {
            let _ = tx.send(());
        });
        drop(router);

        rx.await.expect("Failed to receive close event");
    });
}
