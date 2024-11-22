#define MS_CLASS "UdpSocketHandle"
// #define MS_LOG_DEV_LEVEL 3

#include "handles/UdpSocketHandle.hpp"
#ifdef MS_LIBURING_SUPPORTED
#include "DepLibUring.hpp"
#endif
#include "Logger.hpp"
#include "MediaSoupErrors.hpp"
#include "Utils.hpp"
#include <cstring> // std::memcpy()

/* Static. */

static constexpr size_t ReadBufferSize{ 65536 };
thread_local static uint8_t ReadBuffer[ReadBufferSize];

/* Static methods for UV callbacks. */

inline static void onAlloc(uv_handle_t* handle, size_t suggestedSize, uv_buf_t* buf)
{
	auto* socket = static_cast<UdpSocketHandle*>(handle->data);

	if (socket)
	{
		socket->OnUvRecvAlloc(suggestedSize, buf);
	}
}

inline static void onRecv(
  uv_udp_t* handle, ssize_t nread, const uv_buf_t* buf, const struct sockaddr* addr, unsigned int flags)
{
	auto* socket = static_cast<UdpSocketHandle*>(handle->data);

	if (socket)
	{
		socket->OnUvRecv(nread, buf, addr, flags);
	}
}

inline static void onSend(uv_udp_send_t* req, int status)
{
	auto* sendData = static_cast<UdpSocketHandle::UvSendData*>(req->data);
	auto* handle   = req->handle;
	auto* socket   = static_cast<UdpSocketHandle*>(handle->data);
	const auto* cb = sendData->cb;

	if (socket)
	{
		socket->OnUvSend(status, cb);
	}

	// Delete the UvSendData struct (it will delete the store and cb too).
	delete sendData;
}

inline static void onCloseUdp(uv_handle_t* handle)
{
	delete reinterpret_cast<uv_udp_t*>(handle);
}

/* Instance methods. */

// NOLINTNEXTLINE(cppcoreguidelines-pro-type-member-init)
UdpSocketHandle::UdpSocketHandle(uv_udp_t* uvHandle) : uvHandle(uvHandle)
{
	MS_TRACE();

	this->uvHandle->data = static_cast<void*>(this);

	// NOLINTNEXTLINE(misc-const-correctness)
	int err = uv_udp_recv_start(
	  this->uvHandle, static_cast<uv_alloc_cb>(onAlloc), static_cast<uv_udp_recv_cb>(onRecv));

	if (err != 0)
	{
		uv_close(reinterpret_cast<uv_handle_t*>(this->uvHandle), static_cast<uv_close_cb>(onCloseUdp));

		MS_THROW_ERROR("uv_udp_recv_start() failed: %s", uv_strerror(err));
	}

	// Set local address.
	if (!SetLocalAddress())
	{
		uv_close(reinterpret_cast<uv_handle_t*>(this->uvHandle), static_cast<uv_close_cb>(onCloseUdp));

		MS_THROW_ERROR("error setting local IP and port");
	}

#ifdef MS_LIBURING_SUPPORTED
	if (DepLibUring::IsEnabled())
	{
		err = uv_fileno(reinterpret_cast<uv_handle_t*>(this->uvHandle), std::addressof(this->fd));

		if (err != 0)
		{
			MS_THROW_ERROR("uv_fileno() failed: %s", uv_strerror(err));
		}
	}
#endif
}

UdpSocketHandle::~UdpSocketHandle()
{
	MS_TRACE();

	if (!this->closed)
	{
		InternalClose();
	}
}

void UdpSocketHandle::Dump() const
{
	MS_DUMP("<UdpSocketHandle>");
	MS_DUMP("  localIp: %s", this->localIp.c_str());
	MS_DUMP("  localPort: %" PRIu16, static_cast<uint16_t>(this->localPort));
	MS_DUMP("  closed: %s", this->closed ? "yes" : "no");
	MS_DUMP("</UdpSocketHandle>");
}

void UdpSocketHandle::Send(
  const uint8_t* data, size_t len, const struct sockaddr* addr, UdpSocketHandle::onSendCallback* cb)
{
	MS_TRACE();

	if (this->closed)
	{
		if (cb)
		{
			(*cb)(false);
			delete cb;
		}

		return;
	}

	if (len == 0)
	{
		if (cb)
		{
			(*cb)(false);
			delete cb;
		}

		return;
	}

#ifdef MS_LIBURING_SUPPORTED
	if (DepLibUring::IsEnabled())
	{
		if (!DepLibUring::IsActive())
		{
			goto send_libuv;
		}

		// Prepare the data to be sent.
		// NOTE: If all SQEs are currently in use or no UserData entry is available we'll
		// fall back to libuv.
		auto prepared = DepLibUring::PrepareSend(this->fd, data, len, addr, cb);

		if (!prepared)
		{
			MS_DEBUG_DEV("cannot send via liburing, fallback to libuv");

			goto send_libuv;
		}

		return;
	}

send_libuv:
#endif

	// First try uv_udp_try_send(). In case it can not directly send the datagram
	// then build a uv_req_t and use uv_udp_send().

	uv_buf_t buffer = uv_buf_init(reinterpret_cast<char*>(const_cast<uint8_t*>(data)), len);
	const int sent  = uv_udp_try_send(this->uvHandle, &buffer, 1, addr);

	// Entire datagram was sent. Done.
	if (sent == static_cast<int>(len))
	{
		// Update sent bytes.
		this->sentBytes += sent;

		if (cb)
		{
			(*cb)(true);
			delete cb;
		}

		return;
	}
	else if (sent >= 0)
	{
		MS_WARN_DEV("datagram truncated (just %d of %zu bytes were sent)", sent, len);

		// Update sent bytes.
		this->sentBytes += sent;

		if (cb)
		{
			(*cb)(false);
			delete cb;
		}

		return;
	}
	// Any error but legit EAGAIN. Use uv_udp_send().
	else if (sent != UV_EAGAIN)
	{
		MS_WARN_DEV("uv_udp_try_send() failed, trying uv_udp_send(): %s", uv_strerror(sent));
	}

	auto* sendData = new UvSendData(len);

	sendData->req.data = static_cast<void*>(sendData);
	std::memcpy(sendData->store, data, len);
	sendData->cb = cb;

	buffer = uv_buf_init(reinterpret_cast<char*>(sendData->store), len);

	const int err = uv_udp_send(
	  &sendData->req, this->uvHandle, &buffer, 1, addr, static_cast<uv_udp_send_cb>(onSend));

	if (err != 0)
	{
		// NOTE: uv_udp_send() returns error if a wrong INET family is given
		// (IPv6 destination on a IPv4 binded socket), so be ready.
		MS_WARN_DEV("uv_udp_send() failed: %s", uv_strerror(err));

		if (cb)
		{
			(*cb)(false);
		}

		// Delete the UvSendData struct (it will delete the store and cb too).
		delete sendData;
	}
	else
	{
		// Update sent bytes.
		this->sentBytes += len;
	}
}

uint32_t UdpSocketHandle::GetSendBufferSize() const
{
	MS_TRACE();

	int size{ 0 };
	const int err =
	  uv_send_buffer_size(reinterpret_cast<uv_handle_t*>(this->uvHandle), std::addressof(size));

	if (err)
	{
		MS_THROW_ERROR("uv_send_buffer_size() failed: %s", uv_strerror(err));
	}

	return static_cast<uint32_t>(size);
}

void UdpSocketHandle::SetSendBufferSize(uint32_t size)
{
	MS_TRACE();

	auto sizeInt = static_cast<int>(size);

	if (sizeInt <= 0)
	{
		MS_THROW_TYPE_ERROR("invalid size: %d", sizeInt);
	}

	const int err =
	  uv_send_buffer_size(reinterpret_cast<uv_handle_t*>(this->uvHandle), std::addressof(sizeInt));

	if (err)
	{
		MS_THROW_ERROR("uv_send_buffer_size() failed: %s", uv_strerror(err));
	}
}

uint32_t UdpSocketHandle::GetRecvBufferSize() const
{
	MS_TRACE();

	int size{ 0 };
	const int err =
	  uv_recv_buffer_size(reinterpret_cast<uv_handle_t*>(this->uvHandle), std::addressof(size));

	if (err)
	{
		MS_THROW_ERROR("uv_recv_buffer_size() failed: %s", uv_strerror(err));
	}

	return static_cast<uint32_t>(size);
}

void UdpSocketHandle::SetRecvBufferSize(uint32_t size)
{
	MS_TRACE();

	auto sizeInt = static_cast<int>(size);

	if (sizeInt <= 0)
	{
		MS_THROW_TYPE_ERROR("invalid size: %d", sizeInt);
	}

	const int err =
	  uv_recv_buffer_size(reinterpret_cast<uv_handle_t*>(this->uvHandle), std::addressof(sizeInt));

	if (err)
	{
		MS_THROW_ERROR("uv_recv_buffer_size() failed: %s", uv_strerror(err));
	}
}

void UdpSocketHandle::InternalClose()
{
	MS_TRACE();

	if (this->closed)
	{
		return;
	}

	this->closed = true;

	// Tell the UV handle that the UdpSocketHandle has been closed.
	this->uvHandle->data = nullptr;

	// Don't read more.
	const int err = uv_udp_recv_stop(this->uvHandle);

	if (err != 0)
	{
		MS_ABORT("uv_udp_recv_stop() failed: %s", uv_strerror(err));
	}

	uv_close(reinterpret_cast<uv_handle_t*>(this->uvHandle), static_cast<uv_close_cb>(onCloseUdp));
}

bool UdpSocketHandle::SetLocalAddress()
{
	MS_TRACE();

	int err;
	int len = sizeof(this->localAddr);

	err =
	  uv_udp_getsockname(this->uvHandle, reinterpret_cast<struct sockaddr*>(&this->localAddr), &len);

	if (err != 0)
	{
		MS_ERROR("uv_udp_getsockname() failed: %s", uv_strerror(err));

		return false;
	}

	int family;

	Utils::IP::GetAddressInfo(
	  reinterpret_cast<const struct sockaddr*>(&this->localAddr), family, this->localIp, this->localPort);

	return true;
}

// NOLINTNEXTLINE(readability-convert-member-functions-to-static)
inline void UdpSocketHandle::OnUvRecvAlloc(size_t /*suggestedSize*/, uv_buf_t* buf)
{
	MS_TRACE();

	// Tell UV to write into the static buffer.
	buf->base = reinterpret_cast<char*>(ReadBuffer);
	// Give UV all the buffer space.
	buf->len = ReadBufferSize;
}

inline void UdpSocketHandle::OnUvRecv(
  ssize_t nread, const uv_buf_t* buf, const struct sockaddr* addr, unsigned int flags)
{
	MS_TRACE();

	// NOTE: Ignore if there is nothing to read or if it was an empty datagram.
	if (nread == 0)
	{
		return;
	}

	// Check flags.
	if ((flags & UV_UDP_PARTIAL) != 0u)
	{
		MS_ERROR("received datagram was truncated due to insufficient buffer, ignoring it");

		return;
	}

	// Data received.
	if (nread > 0)
	{
		// Update received bytes.
		this->recvBytes += nread;

		// Notify the subclass.
		UserOnUdpDatagramReceived(reinterpret_cast<uint8_t*>(buf->base), nread, addr);
	}
	// Some error.
	else
	{
		MS_DEBUG_DEV("read error: %s", uv_strerror(nread));
	}
}

// NOLINTNEXTLINE(readability-convert-member-functions-to-static)
inline void UdpSocketHandle::OnUvSend(int status, UdpSocketHandle::onSendCallback* cb)
{
	MS_TRACE();

	// NOTE: Do not delete cb here since it will be delete in onSend() above.

	if (status == 0)
	{
		if (cb)
		{
			(*cb)(true);
		}
	}
	else
	{
#if MS_LOG_DEV_LEVEL == 3
		MS_DEBUG_DEV("send error: %s", uv_strerror(status));
#endif

		if (cb)
		{
			(*cb)(false);
		}
	}
}
