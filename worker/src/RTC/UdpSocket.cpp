#define MS_CLASS "RTC::UdpSocket"
// #define MS_LOG_DEV_LEVEL 3

#include "RTC/UdpSocket.hpp"
#include "Logger.hpp"
#include "RTC/PortManager.hpp"
#include <string>

namespace RTC
{
	/* Instance methods. */

	UdpSocket::UdpSocket(
	  Listener* listener, std::string& ip, uint16_t port, RTC::Transport::SocketFlags& flags)
	  : // This may throw.
	    ::UdpSocketHandle::UdpSocketHandle(RTC::PortManager::BindUdp(ip, port, flags)),
	    listener(listener), fixedPort(true)
	{
		MS_TRACE();
	}

	UdpSocket::UdpSocket(
	  Listener* listener,
	  std::string& ip,
	  uint16_t minPort,
	  uint16_t maxPort,
	  RTC::Transport::SocketFlags& flags,
	  uint64_t& portRangeHash)
	  : // This may throw.
	    ::UdpSocketHandle::UdpSocketHandle(
	      RTC::PortManager::BindUdp(ip, minPort, maxPort, flags, portRangeHash)),
	    listener(listener), fixedPort(false)
	{
		MS_TRACE();

		this->portRangeHash = portRangeHash;
	}

	UdpSocket::~UdpSocket()
	{
		MS_TRACE();

		if (!this->fixedPort)
		{
			RTC::PortManager::Unbind(this->portRangeHash, this->localPort);
		}
	}

	void UdpSocket::UserOnUdpDatagramReceived(const uint8_t* data, size_t len, const struct sockaddr* addr)
	{
		MS_TRACE();

		if (!this->listener)
		{
			MS_ERROR("no listener set");

			return;
		}

		// Notify the reader.
		this->listener->OnUdpSocketPacketReceived(this, data, len, addr);
	}
} // namespace RTC
