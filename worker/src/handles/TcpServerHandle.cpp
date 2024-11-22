#define MS_CLASS "TcpServerHandle"
// #define MS_LOG_DEV_LEVEL 3

#include "handles/TcpServerHandle.hpp"
#include "Logger.hpp"
#include "MediaSoupErrors.hpp"
#include "Utils.hpp"

/* Static. */

static constexpr int ListenBacklog{ 512 };

/* Static methods for UV callbacks. */

inline static void onConnection(uv_stream_t* handle, int status)
{
	auto* server = static_cast<TcpServerHandle*>(handle->data);

	if (server)
	{
		server->OnUvConnection(status);
	}
}

inline static void onCloseTcp(uv_handle_t* handle)
{
	delete reinterpret_cast<uv_tcp_t*>(handle);
}

/* Instance methods. */

// NOLINTNEXTLINE(cppcoreguidelines-pro-type-member-init)
TcpServerHandle::TcpServerHandle(uv_tcp_t* uvHandle) : uvHandle(uvHandle)
{
	MS_TRACE();

	int err;

	this->uvHandle->data = static_cast<void*>(this);

	err = uv_listen(
	  reinterpret_cast<uv_stream_t*>(this->uvHandle),
	  ListenBacklog,
	  static_cast<uv_connection_cb>(onConnection));

	if (err != 0)
	{
		uv_close(reinterpret_cast<uv_handle_t*>(this->uvHandle), static_cast<uv_close_cb>(onCloseTcp));

		MS_THROW_ERROR("uv_listen() failed: %s", uv_strerror(err));
	}

	// Set local address.
	if (!SetLocalAddress())
	{
		uv_close(reinterpret_cast<uv_handle_t*>(this->uvHandle), static_cast<uv_close_cb>(onCloseTcp));

		MS_THROW_ERROR("error setting local IP and port");
	}
}

TcpServerHandle::~TcpServerHandle()
{
	MS_TRACE();

	if (!this->closed)
	{
		InternalClose();
	}
}

void TcpServerHandle::Dump() const
{
	MS_DUMP("<TcpServerHandle>");
	MS_DUMP("  localIp: %s", this->localIp.c_str());
	MS_DUMP("  localPort: %" PRIu16, static_cast<uint16_t>(this->localPort));
	MS_DUMP("  num connections: %zu", this->connections.size());
	MS_DUMP("  closed: %s", this->closed ? "yes" : "no");
	MS_DUMP("</TcpServerHandle>");
}

uint32_t TcpServerHandle::GetSendBufferSize() const
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

void TcpServerHandle::SetSendBufferSize(uint32_t size)
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

uint32_t TcpServerHandle::GetRecvBufferSize() const
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

void TcpServerHandle::SetRecvBufferSize(uint32_t size)
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

void TcpServerHandle::AcceptTcpConnection(TcpConnectionHandle* connection)
{
	MS_TRACE();

	MS_ASSERT(connection != nullptr, "TcpConnectionHandle pointer was not allocated by the user");

	try
	{
		connection->Setup(this, &(this->localAddr), this->localIp, this->localPort);
	}
	catch (const MediaSoupError& error)
	{
		delete connection;

		return;
	}

	// Accept the connection.
	const int err = uv_accept(
	  reinterpret_cast<uv_stream_t*>(this->uvHandle),
	  reinterpret_cast<uv_stream_t*>(connection->GetUvHandle()));

	if (err != 0)
	{
		MS_ABORT("uv_accept() failed: %s", uv_strerror(err));
	}

	// Start receiving data.
	try
	{
		// NOTE: This may throw.
		connection->Start();
	}
	catch (const MediaSoupError& error)
	{
		delete connection;

		return;
	}

	// Store it.
	this->connections.insert(connection);
}

void TcpServerHandle::InternalClose()
{
	MS_TRACE();

	if (this->closed)
	{
		return;
	}

	this->closed = true;

	// Tell the UV handle that the TcpServerHandle has been closed.
	this->uvHandle->data = nullptr;

	MS_DEBUG_DEV("closing %zu active connections", this->connections.size());

	for (auto* connection : this->connections)
	{
		delete connection;
	}

	uv_close(reinterpret_cast<uv_handle_t*>(this->uvHandle), static_cast<uv_close_cb>(onCloseTcp));
}

bool TcpServerHandle::SetLocalAddress()
{
	MS_TRACE();

	int err;
	int len = sizeof(this->localAddr);

	err =
	  uv_tcp_getsockname(this->uvHandle, reinterpret_cast<struct sockaddr*>(&this->localAddr), &len);

	if (err != 0)
	{
		MS_ERROR("uv_tcp_getsockname() failed: %s", uv_strerror(err));

		return false;
	}

	int family;

	Utils::IP::GetAddressInfo(
	  reinterpret_cast<const struct sockaddr*>(&this->localAddr), family, this->localIp, this->localPort);

	return true;
}

inline void TcpServerHandle::OnUvConnection(int status)
{
	MS_TRACE();

	if (this->closed)
	{
		return;
	}

	if (status != 0)
	{
		MS_ERROR("error while receiving a new TCP connection: %s", uv_strerror(status));

		return;
	}

	// Notify the subclass about a new TCP connection attempt.
	UserOnTcpConnectionAlloc();
}

inline void TcpServerHandle::OnTcpConnectionClosed(TcpConnectionHandle* connection)
{
	MS_TRACE();

	MS_DEBUG_DEV("TCP connection closed");

	// Remove the TcpConnectionHandle from the set.
	this->connections.erase(connection);

	// Notify the subclass.
	UserOnTcpConnectionClosed(connection);

	// Delete it.
	delete connection;
}
