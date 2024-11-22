#ifndef MS_RTC_PORT_MANAGER_HPP
#define MS_RTC_PORT_MANAGER_HPP

#include "common.hpp"
#include "RTC/Transport.hpp"
#include <uv.h>
#include <absl/container/flat_hash_map.h>
#include <string>
#include <vector>

namespace RTC
{
	class PortManager
	{
	private:
		enum class Protocol : uint8_t
		{
			UDP = 1,
			TCP
		};

	private:
		struct PortRange
		{
			explicit PortRange(uint16_t numPorts, uint16_t minPort)
			  : ports(numPorts, false), minPort(minPort)
			{
			}

			std::vector<bool> ports;
			uint16_t minPort{ 0u };
			uint16_t numUsedPorts{ 0u };
		};

	public:
		static uv_udp_t* BindUdp(std::string& ip, uint16_t port, RTC::Transport::SocketFlags& flags)
		{
			return reinterpret_cast<uv_udp_t*>(Bind(Protocol::UDP, ip, port, flags));
		}
		static uv_udp_t* BindUdp(
		  std::string& ip,
		  uint16_t minPort,
		  uint16_t maxPort,
		  RTC::Transport::SocketFlags& flags,
		  uint64_t& hash)
		{
			return reinterpret_cast<uv_udp_t*>(Bind(Protocol::UDP, ip, minPort, maxPort, flags, hash));
		}
		static uv_tcp_t* BindTcp(std::string& ip, uint16_t port, RTC::Transport::SocketFlags& flags)
		{
			return reinterpret_cast<uv_tcp_t*>(Bind(Protocol::TCP, ip, port, flags));
		}
		static uv_tcp_t* BindTcp(
		  std::string& ip,
		  uint16_t minPort,
		  uint16_t maxPort,
		  RTC::Transport::SocketFlags& flags,
		  uint64_t& hash)
		{
			return reinterpret_cast<uv_tcp_t*>(Bind(Protocol::TCP, ip, minPort, maxPort, flags, hash));
		}
		static void Unbind(uint64_t hash, uint16_t port);
		static void Dump();

	private:
		static uv_handle_t* Bind(
		  Protocol protocol, std::string& ip, uint16_t port, RTC::Transport::SocketFlags& flags);
		static uv_handle_t* Bind(
		  Protocol protocol,
		  std::string& ip,
		  uint16_t minPort,
		  uint16_t maxPort,
		  RTC::Transport::SocketFlags& flags,
		  uint64_t& hash);
		static uint64_t GeneratePortRangeHash(
		  Protocol protocol, sockaddr_storage* bindAddr, uint16_t minPort, uint16_t maxPort);
		static PortRange& GetOrCreatePortRange(uint64_t hash, uint16_t minPort, uint16_t maxPort);
		static uint8_t ConvertSocketFlags(RTC::Transport::SocketFlags& flags, Protocol protocol, int family);

	private:
		thread_local static absl::flat_hash_map<uint64_t, PortRange> mapPortRanges;
	};
} // namespace RTC

#endif
