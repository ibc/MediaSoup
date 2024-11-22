use futures_lite::future;
use mediasoup::data_structures::AppData;
use mediasoup::worker::{
    ChannelMessageHandlers, WorkerDtlsFiles, WorkerLogLevel, WorkerLogTag, WorkerSettings,
    WorkerUpdateSettings,
};
use mediasoup::worker_manager::WorkerManager;
use std::{env, io};

async fn init() -> WorkerManager {
    {
        let mut builder = env_logger::builder();
        if env::var(env_logger::DEFAULT_FILTER_ENV).is_err() {
            builder.filter_level(log::LevelFilter::Off);
        }
        let _ = builder.is_test(true).try_init();
    }

    WorkerManager::new()
}

#[test]
fn create_worker_succeeds() {
    future::block_on(async move {
        let worker_manager = init().await;

        let worker = worker_manager
            .create_worker(WorkerSettings::default())
            .await
            .expect("Failed to create worker with default settings");

        drop(worker);

        #[derive(Debug, PartialEq)]
        struct CustomAppData {
            bar: u16,
        }

        let worker = worker_manager
            .create_worker({
                let mut settings = WorkerSettings::default();

                settings.log_level = WorkerLogLevel::Debug;
                settings.log_tags = vec![WorkerLogTag::Info];
                settings.rtc_port_range = 0..=9999;
                settings.dtls_files = Some(WorkerDtlsFiles {
                    certificate: "tests/integration/data/dtls-cert.pem".into(),
                    private_key: "tests/integration/data/dtls-key.pem".into(),
                });
                settings.libwebrtc_field_trials =
                    Some("WebRTC-Bwe-AlrLimitedBackoff/Disabled/".to_string());
                settings.app_data = AppData::new(CustomAppData { bar: 456 });

                settings
            })
            .await
            .expect("Failed to create worker with custom settings");

        assert!(!worker.closed());
        assert_eq!(
            worker.app_data().downcast_ref::<CustomAppData>(),
            Some(&CustomAppData { bar: 456 }),
        );

        drop(worker);
    });
}

#[test]
fn create_worker_wrong_settings() {
    future::block_on(async move {
        let worker_manager = init().await;

        {
            let worker_result = worker_manager
                .create_worker({
                    let mut settings = WorkerSettings::default();

                    // Intentionally incorrect range
                    #[allow(clippy::reversed_empty_ranges)]
                    {
                        settings.rtc_port_range = 1000..=999;
                    }

                    settings
                })
                .await;

            assert!(matches!(worker_result, Err(io::Error { .. })));
        }

        {
            let worker_result = worker_manager
                .create_worker({
                    let mut settings = WorkerSettings::default();

                    settings.dtls_files = Some(WorkerDtlsFiles {
                        certificate: "/notfound/cert.pem".into(),
                        private_key: "/notfound/priv.pem".into(),
                    });

                    settings
                })
                .await;

            assert!(matches!(worker_result, Err(io::Error { .. })));
        }
    });
}

#[test]
fn update_settings_succeeds() {
    future::block_on(async move {
        let worker_manager = init().await;

        let worker = worker_manager
            .create_worker(WorkerSettings::default())
            .await
            .expect("Failed to create worker with default settings");

        worker
            .update_settings({
                let mut settings = WorkerUpdateSettings::default();

                settings.log_level = Some(WorkerLogLevel::Debug);
                settings.log_tags = Some(vec![WorkerLogTag::Ice]);

                settings
            })
            .await
            .expect("Failed to update worker settings");
    });
}

#[test]
fn dump_succeeds() {
    future::block_on(async move {
        let worker_manager = init().await;

        let worker = worker_manager
            .create_worker(WorkerSettings::default())
            .await
            .expect("Failed to create worker with default settings");

        let dump = worker.dump().await.expect("Failed to dump worker");

        assert_eq!(dump.router_ids, vec![]);
        assert_eq!(dump.webrtc_server_ids, vec![]);
        assert_eq!(
            dump.channel_message_handlers,
            ChannelMessageHandlers {
                channel_request_handlers: vec![],
                channel_notification_handlers: vec![]
            }
        );
    });
}

#[test]
fn close_event() {
    future::block_on(async move {
        let worker_manager = init().await;

        let worker = worker_manager
            .create_worker(WorkerSettings::default())
            .await
            .expect("Failed to create worker with default settings");

        let (mut tx, rx) = async_oneshot::oneshot::<()>();
        let _handler = worker.on_close(move || {
            let _ = tx.send(());
        });
        drop(worker);

        rx.await.expect("Failed to receive close event");
    });
}
