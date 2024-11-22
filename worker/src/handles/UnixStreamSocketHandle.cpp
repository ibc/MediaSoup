/**
 * NOTE: This code cannot log to the Channel since this is the base code of the
 * Channel.
 */

#define MS_CLASS "UnixStreamSocketHandle"
// #define MS_LOG_DEV_LEVEL 3

#include "handles/UnixStreamSocketHandle.hpp"
#include "DepLibUV.hpp"
#include "Logger.hpp"
#include "MediaSoupErrors.hpp"
#include <cstring> // std::memcpy()

/* Static methods for UV callbacks. */

inline static void onAlloc(uv_handle_t* handle, size_t suggestedSize, uv_buf_t* buf)
{
	auto* socket = static_cast<UnixStreamSocketHandle*>(handle->data);

	if (socket)
	{
		socket->OnUvReadAlloc(suggestedSize, buf);
	}
}

inline static void onRead(uv_stream_t* handle, ssize_t nread, const uv_buf_t* buf)
{
	auto* socket = static_cast<UnixStreamSocketHandle*>(handle->data);

	if (socket)
	{
		socket->OnUvRead(nread, buf);
	}
}

inline static void onWrite(uv_write_t* req, int status)
{
	auto* writeData = static_cast<UnixStreamSocketHandle::UvWriteData*>(req->data);
	auto* handle    = req->handle;
	auto* socket    = static_cast<UnixStreamSocketHandle*>(handle->data);

	// Just notify the UnixStreamSocketHandle when error.
	if (socket && status != 0)
	{
		socket->OnUvWriteError(status);
	}

	// Delete the UvWriteData struct.
	delete writeData;
}

// NOTE: We have different onCloseXxx() callbacks to avoid an ASAN warning by
// ensuring that we call `delete xxx` with same type as `new xxx` before.
inline static void onClosePipe(uv_handle_t* handle)
{
	delete reinterpret_cast<uv_pipe_t*>(handle);
}

inline static void onShutdown(uv_shutdown_t* req, int /*status*/)
{
	auto* handle = req->handle;

	delete req;

	// Now do close the handle.
	uv_close(reinterpret_cast<uv_handle_t*>(handle), static_cast<uv_close_cb>(onClosePipe));
}

/* Instance methods. */

UnixStreamSocketHandle::UnixStreamSocketHandle(
  int fd, size_t bufferSize, UnixStreamSocketHandle::Role role)
  : uvHandle(new uv_pipe_t), bufferSize(bufferSize), role(role)
{
	MS_TRACE_STD();

	int err;

	this->uvHandle->data = static_cast<void*>(this);

	err = uv_pipe_init(DepLibUV::GetLoop(), this->uvHandle, 0);

	if (err != 0)
	{
		delete this->uvHandle;
		this->uvHandle = nullptr;

		MS_THROW_ERROR_STD("uv_pipe_init() failed: %s", uv_strerror(err));
	}

	err = uv_pipe_open(this->uvHandle, fd);

	if (err != 0)
	{
		uv_close(reinterpret_cast<uv_handle_t*>(this->uvHandle), static_cast<uv_close_cb>(onClosePipe));

		MS_THROW_ERROR_STD("uv_pipe_open() failed: %s", uv_strerror(err));
	}

	if (this->role == UnixStreamSocketHandle::Role::CONSUMER)
	{
		// Start reading.
		err = uv_read_start(
		  reinterpret_cast<uv_stream_t*>(this->uvHandle),
		  static_cast<uv_alloc_cb>(onAlloc),
		  static_cast<uv_read_cb>(onRead));

		if (err != 0)
		{
			uv_close(reinterpret_cast<uv_handle_t*>(this->uvHandle), static_cast<uv_close_cb>(onClosePipe));

			MS_THROW_ERROR_STD("uv_read_start() failed: %s", uv_strerror(err));
		}
	}

	// NOTE: Don't allocate the buffer here. Instead wait for the first uv_alloc_cb().
}

UnixStreamSocketHandle::~UnixStreamSocketHandle()
{
	MS_TRACE_STD();

	if (!this->closed)
	{
		Close();
	}

	delete[] this->buffer;
}

// NOTE: In UnixStreamSocketHandle we need a poublic Close() method and cannot
// just rely on the destructor plus a private InternalClose() method.
void UnixStreamSocketHandle::Close()
{
	MS_TRACE_STD();

	if (this->closed)
	{
		return;
	}

	int err;

	this->closed = true;

	// Tell the UV handle that the UnixStreamSocketHandle has been closed.
	this->uvHandle->data = nullptr;

	if (this->role == UnixStreamSocketHandle::Role::CONSUMER)
	{
		// Don't read more.
		err = uv_read_stop(reinterpret_cast<uv_stream_t*>(this->uvHandle));

		if (err != 0)
		{
			MS_ABORT("uv_read_stop() failed: %s", uv_strerror(err));
		}
	}

	// If there is no error and the peer didn't close its pipe side then close gracefully.
	if (this->role == UnixStreamSocketHandle::Role::PRODUCER && !this->hasError && !this->isClosedByPeer)
	{
		// Use uv_shutdown() so pending data to be written will be sent to the peer before closing.
		auto* req = new uv_shutdown_t;
		req->data = static_cast<void*>(this);
		err       = uv_shutdown(
      req, reinterpret_cast<uv_stream_t*>(this->uvHandle), static_cast<uv_shutdown_cb>(onShutdown));

		if (err != 0)
		{
			MS_ABORT("uv_shutdown() failed: %s", uv_strerror(err));
		}
	}
	// Otherwise directly close the socket.
	else
	{
		uv_close(reinterpret_cast<uv_handle_t*>(this->uvHandle), static_cast<uv_close_cb>(onClosePipe));
	}
}

void UnixStreamSocketHandle::Write(const uint8_t* data, size_t len)
{
	MS_TRACE_STD();

	if (this->closed)
	{
		return;
	}

	if (len == 0)
	{
		return;
	}

	// First try uv_try_write(). In case it can not directly send all the given data
	// then build a uv_req_t and use uv_write().

	uv_buf_t buffer = uv_buf_init(reinterpret_cast<char*>(const_cast<uint8_t*>(data)), len);
	int written     = uv_try_write(reinterpret_cast<uv_stream_t*>(this->uvHandle), &buffer, 1);

	// All the data was written. Done.
	if (written == static_cast<int>(len))
	{
		return;
	}
	// Cannot write any data at first time. Use uv_write().
	else if (written == UV_EAGAIN || written == UV_ENOSYS)
	{
		// Set written to 0 so pendingLen can be properly calculated.
		written = 0;
	}
	// Any other error.
	else if (written < 0)
	{
		MS_ERROR_STD("uv_try_write() failed, trying uv_write(): %s", uv_strerror(written));

		// Set written to 0 so pendingLen can be properly calculated.
		written = 0;
	}

	const size_t pendingLen = len - written;
	auto* writeData         = new UvWriteData(pendingLen);

	writeData->req.data = static_cast<void*>(writeData);
	std::memcpy(writeData->store, data + written, pendingLen);

	buffer = uv_buf_init(reinterpret_cast<char*>(writeData->store), pendingLen);

	const int err = uv_write(
	  &writeData->req,
	  reinterpret_cast<uv_stream_t*>(this->uvHandle),
	  &buffer,
	  1,
	  static_cast<uv_write_cb>(onWrite));

	if (err != 0)
	{
		MS_ERROR_STD("uv_write() failed: %s", uv_strerror(err));

		// Delete the UvSendData struct.
		delete writeData;
	}
}

uint32_t UnixStreamSocketHandle::GetSendBufferSize() const
{
	MS_TRACE();

	int size{ 0 };
	const int err =
	  uv_send_buffer_size(reinterpret_cast<uv_handle_t*>(this->uvHandle), std::addressof(size));

	if (err)
	{
		MS_THROW_ERROR_STD("uv_send_buffer_size() failed: %s", uv_strerror(err));
	}

	return static_cast<uint32_t>(size);
}

void UnixStreamSocketHandle::SetSendBufferSize(uint32_t size)
{
	MS_TRACE();

	auto sizeInt = static_cast<int>(size);

	if (sizeInt <= 0)
	{
		MS_THROW_TYPE_ERROR_STD("invalid size: %d", sizeInt);
	}

	const int err =
	  uv_send_buffer_size(reinterpret_cast<uv_handle_t*>(this->uvHandle), std::addressof(sizeInt));

	if (err)
	{
		MS_THROW_ERROR_STD("uv_send_buffer_size() failed: %s", uv_strerror(err));
	}
}

uint32_t UnixStreamSocketHandle::GetRecvBufferSize() const
{
	MS_TRACE();

	int size{ 0 };
	const int err =
	  uv_recv_buffer_size(reinterpret_cast<uv_handle_t*>(this->uvHandle), std::addressof(size));

	if (err)
	{
		MS_THROW_ERROR_STD("uv_recv_buffer_size() failed: %s", uv_strerror(err));
	}

	return static_cast<uint32_t>(size);
}

void UnixStreamSocketHandle::SetRecvBufferSize(uint32_t size)
{
	MS_TRACE();

	auto sizeInt = static_cast<int>(size);

	if (sizeInt <= 0)
	{
		MS_THROW_TYPE_ERROR_STD("invalid size: %d", sizeInt);
	}

	const int err =
	  uv_recv_buffer_size(reinterpret_cast<uv_handle_t*>(this->uvHandle), std::addressof(sizeInt));

	if (err)
	{
		MS_THROW_ERROR_STD("uv_recv_buffer_size() failed: %s", uv_strerror(err));
	}
}

inline void UnixStreamSocketHandle::OnUvReadAlloc(size_t /*suggestedSize*/, uv_buf_t* buf)
{
	MS_TRACE_STD();

	// If this is the first call to onUvReadAlloc() then allocate the receiving buffer now.
	if (!this->buffer)
	{
		this->buffer = new uint8_t[this->bufferSize];
	}

	// Tell UV to write after the last data byte in the buffer.
	buf->base = reinterpret_cast<char*>(this->buffer + this->bufferDataLen);

	// Give UV all the remaining space in the buffer.
	if (this->bufferSize > this->bufferDataLen)
	{
		buf->len = this->bufferSize - this->bufferDataLen;
	}
	else
	{
		buf->len = 0;

		MS_ERROR_STD("no available space in the buffer");
	}
}

inline void UnixStreamSocketHandle::OnUvRead(ssize_t nread, const uv_buf_t* /*buf*/)
{
	MS_TRACE_STD();

	if (nread == 0)
	{
		return;
	}

	// Data received.
	if (nread > 0)
	{
		// Update the buffer data length.
		this->bufferDataLen += static_cast<size_t>(nread);

		// Notify the subclass.
		UserOnUnixStreamRead();
	}
	// Peer disconnected.
	else if (nread == UV_EOF || nread == UV_ECONNRESET)
	{
		this->isClosedByPeer = true;

		// Close local side of the pipe.
		Close();

		// Notify the subclass.
		UserOnUnixStreamSocketClosed();
	}
	// Some error.
	else
	{
		MS_ERROR_STD("read error, closing the pipe: %s", uv_strerror(nread));

		this->hasError = true;

		// Close the socket.
		Close();

		// Notify the subclass.
		UserOnUnixStreamSocketClosed();
	}
}

inline void UnixStreamSocketHandle::OnUvWriteError(int error)
{
	MS_TRACE_STD();

	if (error != UV_EPIPE && error != UV_ENOTCONN)
	{
		this->hasError = true;
	}

	MS_ERROR_STD("write error, closing the pipe: %s", uv_strerror(error));

	Close();

	// Notify the subclass.
	UserOnUnixStreamSocketClosed();
}
