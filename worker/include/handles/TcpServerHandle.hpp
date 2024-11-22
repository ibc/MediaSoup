#ifndef MS_TCP_SERVER_HANDLE_HPP
#define MS_TCP_SERVER_HANDLE_HPP

#include "common.hpp"
#include "handles/TcpConnectionHandle.hpp"
#include <uv.h>
#include <absl/container/flat_hash_set.h>
#include <string>

class TcpServerHandle : public TcpConnectionHandle::Listener
{
public:
	/**
	 * uvHandle must be an already initialized and binded uv_tcp_t pointer.
	 */
	explicit TcpServerHandle(uv_tcp_t* uvHandle);
	~TcpServerHandle() override;

public:
	void Dump() const;
	const struct sockaddr* GetLocalAddress() const
	{
		return reinterpret_cast<const struct sockaddr*>(&this->localAddr);
	}
	int GetLocalFamily() const
	{
		return reinterpret_cast<const struct sockaddr*>(&this->localAddr)->sa_family;
	}
	const std::string& GetLocalIp() const
	{
		return this->localIp;
	}
	uint16_t GetLocalPort() const
	{
		return this->localPort;
	}
	size_t GetNumConnections() const
	{
		return this->connections.size();
	}
	uint32_t GetSendBufferSize() const;
	void SetSendBufferSize(uint32_t size);
	uint32_t GetRecvBufferSize() const;
	void SetRecvBufferSize(uint32_t size);

protected:
	void AcceptTcpConnection(TcpConnectionHandle* connection);

private:
	void InternalClose();
	bool SetLocalAddress();

	/* Pure virtual methods that must be implemented by the subclass. */
protected:
	virtual void UserOnTcpConnectionAlloc()                                 = 0;
	virtual void UserOnTcpConnectionClosed(TcpConnectionHandle* connection) = 0;

	/* Callbacks fired by UV events. */
public:
	void OnUvConnection(int status);

	/* Methods inherited from TcpConnectionHandle::Listener. */
public:
	void OnTcpConnectionClosed(TcpConnectionHandle* connection) override;

protected:
	struct sockaddr_storage localAddr
	{
	};
	std::string localIp;
	uint16_t localPort{ 0u };

private:
	// Allocated by this (may be passed by argument).
	uv_tcp_t* uvHandle{ nullptr };
	// Others.
	absl::flat_hash_set<TcpConnectionHandle*> connections;
	bool closed{ false };
};

#endif
