#define MS_CLASS "PortManager"
// #define MS_LOG_DEV_LEVEL 3

#include "RTC/PortManager.hpp"
#include "DepLibUV.hpp"
#include "Logger.hpp"
#include "MediaSoupErrors.hpp"
#include "Settings.hpp"
#include "Utils.hpp"
#include <tuple>   // std:make_tuple()
#include <utility> // std::piecewise_construct

/* Static methods for UV callbacks. */

// NOTE: We have different onCloseXxx() callbacks to avoid an ASAN warning by
// ensuring that we call `delete xxx` with same type as `new xxx` before.
static inline void onCloseUdp(uv_handle_t* handle)
{
	delete reinterpret_cast<uv_udp_t*>(handle);
}

static inline void onCloseTcp(uv_handle_t* handle)
{
	delete reinterpret_cast<uv_tcp_t*>(handle);
}

inline static void onFakeConnection(uv_stream_t* /*handle*/, int /*status*/)
{
	// Do nothing.
}

namespace RTC
{
	/* Class variables. */

	thread_local absl::flat_hash_map<uint64_t, PortManager::PortRange> PortManager::mapPortRanges;

	/* Class methods. */

	uv_handle_t* PortManager::Bind(
	  Protocol protocol, std::string& ip, uint16_t port, RTC::Transport::SocketFlags& flags)
	{
		MS_TRACE();

		// First normalize the IP. This may throw if invalid IP.
		Utils::IP::NormalizeIp(ip);

		int err;
		const int family = Utils::IP::GetFamily(ip);
		struct sockaddr_storage bindAddr
		{
		};
		uv_handle_t* uvHandle{ nullptr };
		std::string protocolStr;
		const uint8_t bitFlags = ConvertSocketFlags(flags, protocol, family);

		switch (protocol)
		{
			case Protocol::UDP:
			{
				protocolStr.assign("udp");

				break;
			}

			case Protocol::TCP:
			{
				protocolStr.assign("tcp");

				break;
			}
		}

		switch (family)
		{
			case AF_INET:
			{
				err = uv_ip4_addr(ip.c_str(), 0, reinterpret_cast<struct sockaddr_in*>(&bindAddr));

				if (err != 0)
				{
					MS_THROW_ERROR("uv_ip4_addr() failed: %s", uv_strerror(err));
				}

				break;
			}

			case AF_INET6:
			{
				err = uv_ip6_addr(ip.c_str(), 0, reinterpret_cast<struct sockaddr_in6*>(&bindAddr));

				if (err != 0)
				{
					MS_THROW_ERROR("uv_ip6_addr() failed: %s", uv_strerror(err));
				}

				break;
			}

			// This cannot happen.
			default:
			{
				MS_THROW_ERROR("unknown IP family");
			}
		}

		// Set the port into the sockaddr struct.
		switch (family)
		{
			case AF_INET:
			{
				(reinterpret_cast<struct sockaddr_in*>(&bindAddr))->sin_port = htons(port);

				break;
			}

			case AF_INET6:
			{
				(reinterpret_cast<struct sockaddr_in6*>(&bindAddr))->sin6_port = htons(port);

				break;
			}
		}

		// Try to bind on it.
		switch (protocol)
		{
			case Protocol::UDP:
			{
				uvHandle = reinterpret_cast<uv_handle_t*>(new uv_udp_t());
				err      = uv_udp_init_ex(
          DepLibUV::GetLoop(), reinterpret_cast<uv_udp_t*>(uvHandle), UV_UDP_RECVMMSG);

				break;
			}

			case Protocol::TCP:
			{
				uvHandle = reinterpret_cast<uv_handle_t*>(new uv_tcp_t());
				err      = uv_tcp_init(DepLibUV::GetLoop(), reinterpret_cast<uv_tcp_t*>(uvHandle));

				break;
			}
		}

		if (err != 0)
		{
			switch (protocol)
			{
				case Protocol::UDP:
				{
					delete reinterpret_cast<uv_udp_t*>(uvHandle);

					MS_THROW_ERROR("uv_udp_init_ex() failed: %s", uv_strerror(err));

					break;
				}

				case Protocol::TCP:
				{
					delete reinterpret_cast<uv_tcp_t*>(uvHandle);

					MS_THROW_ERROR("uv_tcp_init() failed: %s", uv_strerror(err));

					break;
				}
			}
		}

		switch (protocol)
		{
			case Protocol::UDP:
			{
				err = uv_udp_bind(
				  reinterpret_cast<uv_udp_t*>(uvHandle),
				  reinterpret_cast<const struct sockaddr*>(&bindAddr),
				  bitFlags);

				if (err != 0)
				{
					// If it failed, close the handle and check the reason.
					uv_close(reinterpret_cast<uv_handle_t*>(uvHandle), static_cast<uv_close_cb>(onCloseUdp));

					MS_THROW_ERROR(
					  "uv_udp_bind() failed [protocol:%s, ip:'%s', port:%" PRIu16 "]: %s",
					  protocolStr.c_str(),
					  ip.c_str(),
					  port,
					  uv_strerror(err));
				}

				break;
			}

			case Protocol::TCP:
			{
				err = uv_tcp_bind(
				  reinterpret_cast<uv_tcp_t*>(uvHandle),
				  reinterpret_cast<const struct sockaddr*>(&bindAddr),
				  bitFlags);

				if (err != 0)
				{
					// If it failed, close the handle and check the reason.
					uv_close(reinterpret_cast<uv_handle_t*>(uvHandle), static_cast<uv_close_cb>(onCloseTcp));

					MS_THROW_ERROR(
					  "uv_tcp_bind() failed [protocol:%s, ip:'%s', port:%" PRIu16 "]: %s",
					  protocolStr.c_str(),
					  ip.c_str(),
					  port,
					  uv_strerror(err));
				}

				// uv_tcp_bind() may succeed even if later uv_listen() fails, so
				// double check it.
				err = uv_listen(
				  reinterpret_cast<uv_stream_t*>(uvHandle),
				  256,
				  static_cast<uv_connection_cb>(onFakeConnection));

				if (err != 0)
				{
					// If it failed, close the handle and check the reason.
					uv_close(reinterpret_cast<uv_handle_t*>(uvHandle), static_cast<uv_close_cb>(onCloseTcp));

					MS_THROW_ERROR(
					  "uv_listen() failed [protocol:%s, ip:'%s', port:%" PRIu16 "]: %s",
					  protocolStr.c_str(),
					  ip.c_str(),
					  port,
					  uv_strerror(err));
				}

				break;
			}
		}

		MS_DEBUG_DEV(
		  "bind succeeded [protocol:%s, ip:'%s', port:%" PRIu16 "]", protocolStr.c_str(), ip.c_str(), port);

		return static_cast<uv_handle_t*>(uvHandle);
	}

	uv_handle_t* PortManager::Bind(
	  Protocol protocol,
	  std::string& ip,
	  uint16_t minPort,
	  uint16_t maxPort,
	  RTC::Transport::SocketFlags& flags,
	  uint64_t& hash)
	{
		MS_TRACE();

		if (maxPort < minPort)
		{
			MS_THROW_TYPE_ERROR("maxPort cannot be less than minPort");
		}

		// First normalize the IP. This may throw if invalid IP.
		Utils::IP::NormalizeIp(ip);

		int err;
		const int family = Utils::IP::GetFamily(ip);
		struct sockaddr_storage bindAddr
		{
		};
		std::string protocolStr;

		switch (protocol)
		{
			case Protocol::UDP:
			{
				protocolStr.assign("udp");

				break;
			}

			case Protocol::TCP:
			{
				protocolStr.assign("tcp");

				break;
			}
		}

		switch (family)
		{
			case AF_INET:
			{
				err = uv_ip4_addr(ip.c_str(), 0, reinterpret_cast<struct sockaddr_in*>(&bindAddr));

				if (err != 0)
				{
					MS_THROW_ERROR("uv_ip4_addr() failed: %s", uv_strerror(err));
				}

				break;
			}

			case AF_INET6:
			{
				err = uv_ip6_addr(ip.c_str(), 0, reinterpret_cast<struct sockaddr_in6*>(&bindAddr));

				if (err != 0)
				{
					MS_THROW_ERROR("uv_ip6_addr() failed: %s", uv_strerror(err));
				}

				break;
			}

			// This cannot happen.
			default:
			{
				MS_THROW_ERROR("unknown IP family");
			}
		}

		hash = GeneratePortRangeHash(protocol, std::addressof(bindAddr), minPort, maxPort);

		auto& portRange          = PortManager::GetOrCreatePortRange(hash, minPort, maxPort);
		const size_t numPorts    = portRange.ports.size();
		const size_t numAttempts = numPorts;
		size_t attempt{ 0u };
		size_t portIdx;
		uint16_t port;
		uv_handle_t* uvHandle{ nullptr };
		const uint8_t bitFlags = ConvertSocketFlags(flags, protocol, family);

		// Choose a random port index to start from.
		portIdx = static_cast<size_t>(
		  Utils::Crypto::GetRandomUInt(static_cast<uint32_t>(0), static_cast<uint32_t>(numPorts - 1)));

		// Iterate all ports until getting one available. Fail if none found and also
		// if bind() fails N times in theoretically available ports.
		while (true)
		{
			// Increase attempt number.
			++attempt;

			// If we have tried all the ports in the range throw.
			if (attempt > numAttempts)
			{
				MS_THROW_ERROR(
				  "no more available ports [protocol:%s, ip:'%s', numAttempt:%zu]",
				  protocolStr.c_str(),
				  ip.c_str(),
				  numAttempts);
			}

			// Increase current port index.
			portIdx = (portIdx + 1) % numPorts;

			// So the corresponding port is the vector position plus the RTC minimum port.
			port = static_cast<uint16_t>(portIdx + minPort);

			MS_DEBUG_DEV(
			  "testing port [protocol:%s, ip:'%s', port:%" PRIu16 ", attempt:%zu/%zu]",
			  protocolStr.c_str(),
			  ip.c_str(),
			  port,
			  attempt,
			  numAttempts);

			// Check whether this port is not available.
			if (portRange.ports[portIdx])
			{
				MS_DEBUG_DEV(
				  "port in use, trying again [protocol:%s, ip:'%s', port:%" PRIu16 ", attempt:%zu/%zu]",
				  protocolStr.c_str(),
				  ip.c_str(),
				  port,
				  attempt,
				  numAttempts);

				continue;
			}

			// Here we already have a theoretically available port. Now let's check
			// whether no other process is binding into it.

			// Set the chosen port into the sockaddr struct.
			switch (family)
			{
				case AF_INET:
				{
					(reinterpret_cast<struct sockaddr_in*>(&bindAddr))->sin_port = htons(port);

					break;
				}

				case AF_INET6:
				{
					(reinterpret_cast<struct sockaddr_in6*>(&bindAddr))->sin6_port = htons(port);

					break;
				}
			}

			// Try to bind on it.
			switch (protocol)
			{
				case Protocol::UDP:
				{
					uvHandle = reinterpret_cast<uv_handle_t*>(new uv_udp_t());
					err      = uv_udp_init_ex(
            DepLibUV::GetLoop(), reinterpret_cast<uv_udp_t*>(uvHandle), UV_UDP_RECVMMSG);

					break;
				}

				case Protocol::TCP:
				{
					uvHandle = reinterpret_cast<uv_handle_t*>(new uv_tcp_t());
					err      = uv_tcp_init(DepLibUV::GetLoop(), reinterpret_cast<uv_tcp_t*>(uvHandle));

					break;
				}
			}

			if (err != 0)
			{
				switch (protocol)
				{
					case Protocol::UDP:
					{
						delete reinterpret_cast<uv_udp_t*>(uvHandle);

						MS_THROW_ERROR("uv_udp_init_ex() failed: %s", uv_strerror(err));

						break;
					}

					case Protocol::TCP:
					{
						delete reinterpret_cast<uv_tcp_t*>(uvHandle);

						MS_THROW_ERROR("uv_tcp_init() failed: %s", uv_strerror(err));

						break;
					}
				}
			}

			switch (protocol)
			{
				case Protocol::UDP:
				{
					err = uv_udp_bind(
					  reinterpret_cast<uv_udp_t*>(uvHandle),
					  reinterpret_cast<const struct sockaddr*>(&bindAddr),
					  bitFlags);

					if (err != 0)
					{
						MS_WARN_DEV(
						  "uv_udp_bind() failed [protocol:%s, ip:'%s', port:%" PRIu16 ", attempt:%zu/%zu]: %s",
						  protocolStr.c_str(),
						  ip.c_str(),
						  port,
						  attempt,
						  numAttempts,
						  uv_strerror(err));
					}

					break;
				}

				case Protocol::TCP:
				{
					err = uv_tcp_bind(
					  reinterpret_cast<uv_tcp_t*>(uvHandle),
					  reinterpret_cast<const struct sockaddr*>(&bindAddr),
					  bitFlags);

					if (err != 0)
					{
						MS_WARN_DEV(
						  "uv_tcp_bind() failed [protocol:%s, ip:'%s', port:%" PRIu16 ", attempt:%zu/%zu]: %s",
						  protocolStr.c_str(),
						  ip.c_str(),
						  port,
						  attempt,
						  numAttempts,
						  uv_strerror(err));
					}

					// uv_tcp_bind() may succeed even if later uv_listen() fails, so
					// double check it.
					if (err == 0)
					{
						err = uv_listen(
						  reinterpret_cast<uv_stream_t*>(uvHandle),
						  256,
						  static_cast<uv_connection_cb>(onFakeConnection));

						MS_WARN_DEV(
						  "uv_listen() failed [protocol:%s, ip:'%s', port:%" PRIu16 ", attempt:%zu/%zu]: %s",
						  protocolStr.c_str(),
						  ip.c_str(),
						  port,
						  attempt,
						  numAttempts,
						  uv_strerror(err));
					}

					break;
				}
			}

			// If it succeeded, exit the loop here.
			if (err == 0)
			{
				break;
			}

			// If it failed, close the handle and check the reason.
			switch (protocol)
			{
				case Protocol::UDP:
				{
					uv_close(reinterpret_cast<uv_handle_t*>(uvHandle), static_cast<uv_close_cb>(onCloseUdp));

					break;
				};

				case Protocol::TCP:
				{
					uv_close(reinterpret_cast<uv_handle_t*>(uvHandle), static_cast<uv_close_cb>(onCloseTcp));

					break;
				}
			}

			switch (err)
			{
				// If bind() fails due to "too many open files" just throw.
				case UV_EMFILE:
				{
					MS_THROW_ERROR(
					  "port bind failed due to too many open files [protocol:%s, ip:'%s', port:%" PRIu16
					  ", attempt:%zu/%zu]",
					  protocolStr.c_str(),
					  ip.c_str(),
					  port,
					  attempt,
					  numAttempts);

					break;
				}

				// If cannot bind in the given IP, throw.
				case UV_EADDRNOTAVAIL:
				{
					MS_THROW_ERROR(
					  "port bind failed due to address not available [protocol:%s, ip:'%s', port:%" PRIu16
					  ", attempt:%zu/%zu]",
					  protocolStr.c_str(),
					  ip.c_str(),
					  port,
					  attempt,
					  numAttempts);

					break;
				}

				default:
				{
					// Otherwise continue in the loop to try again with next port.
				}
			}
		}

		// If here, we got an available port. Mark it as unavailable.
		portRange.ports[portIdx] = true;

		// Increase number of used ports in the range.
		portRange.numUsedPorts++;

		MS_DEBUG_DEV(
		  "bind succeeded [protocol:%s, ip:'%s', port:%" PRIu16 ", attempt:%zu/%zu]",
		  protocolStr.c_str(),
		  ip.c_str(),
		  port,
		  attempt,
		  numAttempts);

		return static_cast<uv_handle_t*>(uvHandle);
	}

	void PortManager::Unbind(uint64_t hash, uint16_t port)
	{
		MS_TRACE();

		auto it = PortManager::mapPortRanges.find(hash);

		// This should not happen.
		if (it == PortManager::mapPortRanges.end())
		{
			MS_ERROR("hash %" PRIu64 " doesn't exist in the map", hash);

			return;
		}

		auto& portRange    = it->second;
		const auto portIdx = static_cast<size_t>(port - portRange.minPort);

		// This should not happen.
		MS_ASSERT(portRange.ports.at(portIdx) == true, "port %" PRIu16 " is not used", port);
		MS_ASSERT(portRange.numUsedPorts > 0u, "number of used ports is 0");

		// Mark the port as available.
		portRange.ports[portIdx] = false;

		// Decrease number of used ports in the range.
		portRange.numUsedPorts--;

		// Remove vector if there are no used ports.
		if (portRange.numUsedPorts == 0u)
		{
			PortManager::mapPortRanges.erase(it);
		}
	}

	void PortManager::Dump()
	{
		MS_DUMP("<PortManager>");

		for (auto& kv : PortManager::mapPortRanges)
		{
			auto hash      = kv.first;
			auto portRange = kv.second;

			MS_DUMP("  <PortRange>");
			MS_DUMP("    hash: %" PRIu64, hash);
			MS_DUMP("    minPort: %" PRIu16, portRange.minPort);
			MS_DUMP("    maxPort: %zu", portRange.minPort + portRange.ports.size() - 1);
			MS_DUMP("    numUsedPorts: %" PRIu16, portRange.numUsedPorts);
			MS_DUMP("  </PortRange>");
		}

		MS_DUMP("</PortManager>");
	}

	/*
	 * Hash for IPv4.
	 *
	 *  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
	 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
	 * |           MIN PORT            |           MAX PORT            |
	 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
	 * |              IP               |           IP >> 2         |F|P|
	 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
	 *
	 * Hash for IPv6.
	 *
	 *  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
	 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
	 * |           MIN PORT            |           MAX PORT            |
	 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
	 * |IP[0] ^  IP[1] ^ IP[2] ^ IP[3] |           same >> 2       |F|P|
	 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
	 */
	uint64_t PortManager::GeneratePortRangeHash(
	  Protocol protocol, sockaddr_storage* bindAddr, uint16_t minPort, uint16_t maxPort)
	{
		MS_TRACE();

		uint64_t hash{ 0u };

		switch (bindAddr->ss_family)
		{
			case AF_INET:
			{
				auto* bindAddrIn = reinterpret_cast<struct sockaddr_in*>(bindAddr);

				// We want it in network order.
				const uint64_t address = bindAddrIn->sin_addr.s_addr;

				hash |= static_cast<uint64_t>(minPort) << 48;
				hash |= static_cast<uint64_t>(maxPort) << 32;
				hash |= (address >> 2) << 2;
				hash |= 0x0000; // AF_INET.

				break;
			}

			case AF_INET6:
			{
				auto* bindAddrIn6 = reinterpret_cast<struct sockaddr_in6*>(bindAddr);
				auto* a           = reinterpret_cast<uint32_t*>(std::addressof(bindAddrIn6->sin6_addr));

				const auto address = a[0] ^ a[1] ^ a[2] ^ a[3];

				hash |= static_cast<uint64_t>(minPort) << 48;
				hash |= static_cast<uint64_t>(maxPort) << 32;
				hash |= static_cast<uint64_t>(address) << 16;
				hash |= (static_cast<uint64_t>(address) >> 2) << 2;
				hash |= 0x0002; // AF_INET6.

				break;
			}
		}

		// Override least significant bit with protocol information:
		// - If UDP, start with 0.
		// - If TCP, start with 1.
		if (protocol == Protocol::UDP)
		{
			hash |= 0x0000;
		}
		else
		{
			hash |= 0x0001;
		}

		return hash;
	}

	PortManager::PortRange& PortManager::GetOrCreatePortRange(
	  uint64_t hash, uint16_t minPort, uint16_t maxPort)
	{
		MS_TRACE();

		auto it = PortManager::mapPortRanges.find(hash);

		// If the hash is already handled, return its port range.
		if (it != PortManager::mapPortRanges.end())
		{
			auto& portRange = it->second;

			return portRange;
		}

		const uint16_t numPorts = maxPort - minPort + 1;

		// Emplace a new vector filled with numPorts false values, meaning that
		// all ports are available.
		auto pair = PortManager::mapPortRanges.emplace(
		  std::piecewise_construct, std::make_tuple(hash), std::make_tuple(numPorts, minPort));

		// pair.first is an iterator to the inserted value.
		auto& portRange = pair.first->second;

		return portRange;
	}

	uint8_t PortManager::ConvertSocketFlags(RTC::Transport::SocketFlags& flags, Protocol protocol, int family)
	{
		MS_TRACE();

		uint8_t bitFlags{ 0b00000000 };

		// Ignore ipv6Only in IPv4, otherwise libuv will throw.
		if (flags.ipv6Only && family == AF_INET6)
		{
			switch (protocol)
			{
				case Protocol::UDP:
				{
					bitFlags |= UV_UDP_IPV6ONLY;

					break;
				}

				case Protocol::TCP:
				{
					bitFlags |= UV_TCP_IPV6ONLY;

					break;
				}
			}
		}

		// Ignore udpReusePort in TCP, otherwise libuv will throw.
		if (flags.udpReusePort && protocol == Protocol::UDP)
		{
			bitFlags |= UV_UDP_REUSEADDR;
		}

		return bitFlags;
	}
} // namespace RTC
