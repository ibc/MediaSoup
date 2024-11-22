pub(super) use mediasoup_sys::{ChannelWriteCtx, ChannelWriteFn};
use std::os::raw::c_void;
use std::slice;

/// TypeAlias to silience clippy::type_complexity warnings
type CallbackType = Box<dyn FnMut(&[u8]) + Send + 'static>;

pub(super) struct ChannelReadCallback {
    // Silence clippy warnings
    _callback: CallbackType,
}

impl ChannelReadCallback {
    pub(super) fn new(_callback: CallbackType) -> Self {
        Self { _callback }
    }
}

pub(crate) struct PreparedChannelWrite {
    channel_write_fn: ChannelWriteFn,
    channel_write_ctx: ChannelWriteCtx,
    read_callback: ChannelReadCallback,
}

unsafe impl Send for PreparedChannelWrite {}

impl PreparedChannelWrite {
    /// SAFETY:
    /// 1) `ChannelReadCallback` returned must be dropped AFTER last usage of returned function and
    ///    context pointers
    /// 2) `ChannelWriteCtx` should not be called from multiple threads concurrently
    pub(super) unsafe fn deconstruct(
        self,
    ) -> (ChannelWriteFn, ChannelWriteCtx, ChannelReadCallback) {
        let Self {
            channel_write_fn,
            channel_write_ctx,
            read_callback,
        } = self;
        (channel_write_fn, channel_write_ctx, read_callback)
    }
}

/// Given callback function, prepares a pair of channel write function and context, which can be
/// provided to of C++ worker and worker will effectively call the callback whenever it needs to
/// send something to Rust (so it is writing from C++ point of view and reading from Rust).
pub(crate) fn prepare_channel_write_fn<F>(read_callback: F) -> PreparedChannelWrite
where
    F: FnMut(&[u8]) + Send + 'static,
{
    unsafe extern "C" fn wrapper<F>(
        message: *const u8,
        message_len: u32,
        ChannelWriteCtx(ctx): ChannelWriteCtx,
    ) where
        F: FnMut(&[u8]) + Send + 'static,
    {
        let message = slice::from_raw_parts(message, message_len as usize);
        (*(ctx as *mut F))(message);
    }

    // Move to heap to make sure it doesn't change address later on
    let read_callback = Box::new(read_callback);

    PreparedChannelWrite {
        channel_write_fn: wrapper::<F>,
        channel_write_ctx: ChannelWriteCtx(read_callback.as_ref() as *const F as *const c_void),
        read_callback: ChannelReadCallback::new(read_callback),
    }
}
