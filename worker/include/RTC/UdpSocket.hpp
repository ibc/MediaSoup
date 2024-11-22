#ifndef MS_RTC_UDP_SOCKET_HPP
#define MS_RTC_UDP_SOCKET_HPP

#include "common.hpp"
#include "RTC/Transport.hpp"
#include "handles/UdpSocketHandle.hpp"
#include <string>

namespace RTC
{
	class UdpSocket : public ::UdpSocketHandle
	{
	public:
		class Listener
		{
		public:
			virtual ~Listener() = default;

		public:
			virtual void OnUdpSocketPacketReceived(
			  RTC::UdpSocket* socket, const uint8_t* data, size_t len, const struct sockaddr* remoteAddr) = 0;
		};

	public:
		UdpSocket(Listener* listener, std::string& ip, uint16_t port, RTC::Transport::SocketFlags& flags);
		UdpSocket(
		  Listener* listener,
		  std::string& ip,
		  uint16_t minPort,
		  uint16_t maxPort,
		  RTC::Transport::SocketFlags& flags,
		  uint64_t& portRangeHash);
		~UdpSocket() override;

		/* Pure virtual methods inherited from ::UdpSocketHandle. */
	public:
		void UserOnUdpDatagramReceived(const uint8_t* data, size_t len, const struct sockaddr* addr) override;

	private:
		// Passed by argument.
		Listener* listener{ nullptr };
		bool fixedPort{ false };
		uint64_t portRangeHash{ 0u };
	};
} // namespace RTC

#endif
