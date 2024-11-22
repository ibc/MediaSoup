#define MS_CLASS "RTC::TransportTuple"
// #define MS_LOG_DEV_LEVEL 3

#include "RTC/TransportTuple.hpp"
#include "Logger.hpp"
#include <string>

namespace RTC
{
	/* Static methods. */

	TransportTuple::Protocol TransportTuple::ProtocolFromFbs(FBS::Transport::Protocol protocol)
	{
		MS_TRACE();

		switch (protocol)
		{
			case FBS::Transport::Protocol::UDP:
				return TransportTuple::Protocol::UDP;

			case FBS::Transport::Protocol::TCP:
				return TransportTuple::Protocol::TCP;
		}
	}

	FBS::Transport::Protocol TransportTuple::ProtocolToFbs(TransportTuple::Protocol protocol)
	{
		MS_TRACE();

		switch (protocol)
		{
			case TransportTuple::Protocol::UDP:
				return FBS::Transport::Protocol::UDP;

			case TransportTuple::Protocol::TCP:
				return FBS::Transport::Protocol::TCP;
		}
	}

	/* Instance methods. */

	void TransportTuple::CloseTcpConnection()
	{
		MS_TRACE();

		if (this->protocol == Protocol::UDP)
		{
			MS_ABORT("cannot delete a UDP socket");
		}

		this->tcpConnection->TriggerClose();
	}

	flatbuffers::Offset<FBS::Transport::Tuple> TransportTuple::FillBuffer(
	  flatbuffers::FlatBufferBuilder& builder) const
	{
		MS_TRACE();

		int family;
		std::string localIp;
		uint16_t localPort;

		Utils::IP::GetAddressInfo(GetLocalAddress(), family, localIp, localPort);

		std::string remoteIp;
		uint16_t remotePort;

		Utils::IP::GetAddressInfo(GetRemoteAddress(), family, remoteIp, remotePort);

		auto protocol = TransportTuple::ProtocolToFbs(GetProtocol());

		return FBS::Transport::CreateTupleDirect(
		  builder,
		  (this->localAnnouncedAddress.empty() ? localIp : this->localAnnouncedAddress).c_str(),
		  localPort,
		  remoteIp.c_str(),
		  remotePort,
		  protocol);
	}

	void TransportTuple::Dump() const
	{
		MS_TRACE();

		MS_DUMP("<TransportTuple>");

		int family;
		std::string ip;
		uint16_t port;

		Utils::IP::GetAddressInfo(GetLocalAddress(), family, ip, port);

		MS_DUMP("  localIp: %s", ip.c_str());
		MS_DUMP("  localPort: %" PRIu16, port);

		Utils::IP::GetAddressInfo(GetRemoteAddress(), family, ip, port);

		MS_DUMP("  remoteIp: %s", ip.c_str());
		MS_DUMP("  remotePort: %" PRIu16, port);

		switch (GetProtocol())
		{
			case Protocol::UDP:
				MS_DUMP("  protocol: udp");
				break;

			case Protocol::TCP:
				MS_DUMP("  protocol: tcp");
				break;
		}

		MS_DUMP("</TransportTuple>");
	}

	/*
	 * Hash for IPv4.
	 *
	 *  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
	 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
	 * |              PORT             |             IP                |
	 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
	 * |              IP               |                           |F|P|
	 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
	 *
	 * Hash for IPv6.
	 *
	 *  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
	 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
	 * |              PORT             | IP[0] ^  IP[1] ^ IP[2] ^ IP[3]|
	 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
	 * |IP[0] ^  IP[1] ^ IP[2] ^ IP[3] |          IP[0] >> 16      |F|P|
	 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
	 */
	void TransportTuple::SetHash()
	{
		MS_TRACE();

		const struct sockaddr* remoteSockAddr = GetRemoteAddress();

		switch (remoteSockAddr->sa_family)
		{
			case AF_INET:
			{
				const auto* remoteSockAddrIn = reinterpret_cast<const struct sockaddr_in*>(remoteSockAddr);

				const uint64_t address = ntohl(remoteSockAddrIn->sin_addr.s_addr);
				const uint64_t port    = ntohs(remoteSockAddrIn->sin_port);

				this->hash = port << 48;
				this->hash |= address << 16;
				this->hash |= 0x0000; // AF_INET.

				break;
			}

			case AF_INET6:
			{
				const auto* remoteSockAddrIn6 = reinterpret_cast<const struct sockaddr_in6*>(remoteSockAddr);
				const auto* a =
				  reinterpret_cast<const uint32_t*>(std::addressof(remoteSockAddrIn6->sin6_addr));

				const auto address1 = a[0] ^ a[1] ^ a[2] ^ a[3];
				const auto address2 = a[0];
				const uint64_t port = ntohs(remoteSockAddrIn6->sin6_port);

				this->hash = port << 48;
				this->hash |= static_cast<uint64_t>(address1) << 16;
				this->hash |= address2 >> 16 & 0xFFFC;
				this->hash |= 0x0002; // AF_INET6.

				break;
			}
		}

		// Override least significant bit with protocol information:
		// - If UDP, start with 0.
		// - If TCP, start with 1.
		if (this->protocol == Protocol::UDP)
		{
			this->hash |= 0x0000;
		}
		else
		{
			this->hash |= 0x0001;
		}
	}
} // namespace RTC
