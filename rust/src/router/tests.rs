use crate::router::RouterOptions;
use crate::worker::{Worker, WorkerSettings};
use crate::worker_manager::WorkerManager;
use futures_lite::future;
use std::env;

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
        .create_worker(WorkerSettings {
            enable_liburing: false,
            ..WorkerSettings::default()
        })
        .await
        .expect("Failed to create worker")
}

#[test]
fn worker_close_event() {
    future::block_on(async move {
        let worker = init().await;

        let router = worker
            .create_router(RouterOptions::default())
            .await
            .expect("Failed to create router");

        let (mut close_tx, close_rx) = async_oneshot::oneshot::<()>();
        let _handler = router.on_close(move || {
            let _ = close_tx.send(());
        });

        let (mut worker_close_tx, worker_close_rx) = async_oneshot::oneshot::<()>();
        let _handler = router.on_worker_close(move || {
            let _ = worker_close_tx.send(());
        });

        worker.close();

        worker_close_rx
            .await
            .expect("Failed to receive worker_close event");
        close_rx.await.expect("Failed to receive close event");

        assert!(router.closed());
    });
}
