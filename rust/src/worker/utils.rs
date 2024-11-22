mod channel_read_fn;
mod channel_write_fn;

use crate::worker::channel::BufferMessagesGuard;
use crate::worker::{Channel, SubscriptionTarget, WorkerId};
pub(super) use channel_read_fn::{prepare_channel_read_fn, PreparedChannelRead};
pub(super) use channel_write_fn::{prepare_channel_write_fn, PreparedChannelWrite};
use std::ffi::CString;
use std::os::raw::{c_char, c_int};
use std::sync::atomic::AtomicBool;
use std::sync::Arc;
use thiserror::Error;

/// Worker exit error
#[derive(Debug, Copy, Clone, Error)]
pub enum ExitError {
    /// Generic error.
    #[error("Worker exited with generic error")]
    Generic,
    /// Settings error.
    #[error("Worker exited with settings error")]
    Settings,
    /// Unknown error.
    #[error("Worker exited with unknown error and status code {status_code}")]
    Unknown {
        /// Status code returned by worker
        status_code: i32,
    },
    /// Unexpected error.
    #[error("Worker exited unexpectedly")]
    Unexpected,
}

pub(super) struct WorkerRunResult {
    pub(super) channel: Channel,
    pub(super) buffer_worker_messages_guard: BufferMessagesGuard,
}

pub(super) fn run_worker_with_channels<OE>(
    id: WorkerId,
    thread_initializer: Option<Arc<dyn Fn() + Send + Sync>>,
    args: Vec<String>,
    worker_closed: Arc<AtomicBool>,
    on_exit: OE,
) -> WorkerRunResult
where
    OE: FnOnce(Result<(), ExitError>) + Send + 'static,
{
    let (channel, prepared_channel_read, prepared_channel_write) =
        Channel::new(Arc::clone(&worker_closed));
    let buffer_worker_messages_guard =
        channel.buffer_messages_for(SubscriptionTarget::String(std::process::id().to_string()));

    std::thread::Builder::new()
        .name(format!("mediasoup-worker-{id}"))
        .spawn(move || {
            if let Some(thread_initializer) = thread_initializer {
                thread_initializer();
            }
            let argc = args.len() as c_int;
            let args_cstring = args
                .into_iter()
                .map(|s| -> CString { CString::new(s).unwrap() })
                .collect::<Vec<_>>();
            let argv = args_cstring
                .iter()
                .map(|arg| arg.as_ptr().cast::<c_char>())
                .collect::<Vec<_>>();
            let version = CString::new(env!("CARGO_PKG_VERSION")).unwrap();

            let status_code = unsafe {
                let (channel_read_fn, channel_read_ctx, _channel_write_callback) =
                    prepared_channel_read.deconstruct();
                let (channel_write_fn, channel_write_ctx, _channel_read_callback) =
                    prepared_channel_write.deconstruct();

                mediasoup_sys::mediasoup_worker_run(
                    argc,
                    argv.as_ptr(),
                    version.as_ptr(),
                    0,
                    0,
                    channel_read_fn,
                    channel_read_ctx,
                    channel_write_fn,
                    channel_write_ctx,
                )
            };

            on_exit(match status_code {
                0 => Ok(()),
                1 => Err(ExitError::Generic),
                42 => Err(ExitError::Settings),
                status_code => Err(ExitError::Unknown { status_code }),
            });
        })
        .expect("Failed to spawn mediasoup-worker thread");

    WorkerRunResult {
        channel,
        buffer_worker_messages_guard,
    }
}
