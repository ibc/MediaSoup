#define MS_CLASS "RTC::TcpServer"
// #define MS_LOG_DEV_LEVEL 3

#include "RTC/TcpServer.hpp"
#include "Logger.hpp"
#include "RTC/PortManager.hpp"
#include <string>

namespace RTC
{
	/* Instance methods. */

	TcpServer::TcpServer(
	  Listener* listener,
	  RTC::TcpConnection::Listener* connListener,
	  std::string& ip,
	  uint16_t port,
	  RTC::Transport::SocketFlags& flags)
	  : // This may throw.
	    ::TcpServerHandle::TcpServerHandle(RTC::PortManager::BindTcp(ip, port, flags)),
	    listener(listener), connListener(connListener), fixedPort(true)
	{
		MS_TRACE();
	}

	TcpServer::TcpServer(
	  Listener* listener,
	  RTC::TcpConnection::Listener* connListener,
	  std::string& ip,
	  uint16_t minPort,
	  uint16_t maxPort,
	  RTC::Transport::SocketFlags& flags,
	  uint64_t& portRangeHash)
	  : // This may throw.
	    ::TcpServerHandle::TcpServerHandle(
	      RTC::PortManager::BindTcp(ip, minPort, maxPort, flags, portRangeHash)),
	    listener(listener), connListener(connListener), fixedPort(false)
	{
		MS_TRACE();

		this->portRangeHash = portRangeHash;
	}

	TcpServer::~TcpServer()
	{
		MS_TRACE();

		if (!this->fixedPort)
		{
			RTC::PortManager::Unbind(this->portRangeHash, this->localPort);
		}
	}

	void TcpServer::UserOnTcpConnectionAlloc()
	{
		MS_TRACE();

		// Allocate a new RTC::TcpConnection for the TcpServer to handle it.
		auto* connection = new RTC::TcpConnection(this->connListener, 65536);

		// Accept it.
		AcceptTcpConnection(connection);
	}

	void TcpServer::UserOnTcpConnectionClosed(::TcpConnectionHandle* connection)
	{
		MS_TRACE();

		this->listener->OnRtcTcpConnectionClosed(this, static_cast<RTC::TcpConnection*>(connection));
	}
} // namespace RTC
