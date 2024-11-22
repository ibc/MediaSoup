#define MS_CLASS "Utils::IP"
// #define MS_LOG_DEV_LEVEL 3

#include "Logger.hpp"
#include "MediaSoupErrors.hpp"
#include "Utils.hpp"
#include <uv.h>

namespace Utils
{
	int IP::GetFamily(const std::string& ip)
	{
		MS_TRACE();

		if (ip.size() >= INET6_ADDRSTRLEN)
		{
			return AF_UNSPEC;
		}

		const auto* ipPtr               = ip.c_str();
		char ipBuffer[INET6_ADDRSTRLEN] = { 0 };

		if (uv_inet_pton(AF_INET, ipPtr, ipBuffer) == 0)
		{
			return AF_INET;
		}
		else if (uv_inet_pton(AF_INET6, ipPtr, ipBuffer) == 0)
		{
			return AF_INET6;
		}
		else
		{
			return AF_UNSPEC;
		}
	}

	void IP::GetAddressInfo(const struct sockaddr* addr, int& family, std::string& ip, uint16_t& port)
	{
		MS_TRACE();

		char ipBuffer[INET6_ADDRSTRLEN] = { 0 };
		int err;

		switch (addr->sa_family)
		{
			case AF_INET:
			{
				err = uv_inet_ntop(
				  AF_INET,
				  std::addressof(reinterpret_cast<const struct sockaddr_in*>(addr)->sin_addr),
				  ipBuffer,
				  sizeof(ipBuffer));

				if (err)
				{
					MS_ABORT("uv_inet_ntop() failed: %s", uv_strerror(err));
				}

				port =
				  static_cast<uint16_t>(ntohs(reinterpret_cast<const struct sockaddr_in*>(addr)->sin_port));

				break;
			}

			case AF_INET6:
			{
				err = uv_inet_ntop(
				  AF_INET6,
				  std::addressof(reinterpret_cast<const struct sockaddr_in6*>(addr)->sin6_addr),
				  ipBuffer,
				  sizeof(ipBuffer));

				if (err)
				{
					MS_ABORT("uv_inet_ntop() failed: %s", uv_strerror(err));
				}

				port =
				  static_cast<uint16_t>(ntohs(reinterpret_cast<const struct sockaddr_in6*>(addr)->sin6_port));

				break;
			}

			default:
			{
				MS_ABORT("unknown network family: %d", static_cast<int>(addr->sa_family));
			}
		}

		family = addr->sa_family;
		ip.assign(ipBuffer);
	}

	size_t IP::GetAddressLen(const struct sockaddr* addr)
	{
		MS_TRACE();

		switch (addr->sa_family)
		{
			case AF_INET:
			{
				return sizeof(struct sockaddr_in);
			}

			case AF_INET6:
			{
				return sizeof(struct sockaddr_in6);
			}

			default:
			{
				MS_ABORT("unknown network family: %d", static_cast<int>(addr->sa_family));
			}
		}
	}

	void IP::NormalizeIp(std::string& ip)
	{
		MS_TRACE();

		sockaddr_storage addrStorage{};
		char ipBuffer[INET6_ADDRSTRLEN] = { 0 };
		int err;

		switch (IP::GetFamily(ip))
		{
			case AF_INET:
			{
				err = uv_ip4_addr(ip.c_str(), 0, reinterpret_cast<struct sockaddr_in*>(&addrStorage));

				if (err != 0)
				{
					MS_THROW_TYPE_ERROR("uv_ip4_addr() failed [ip:'%s']: %s", ip.c_str(), uv_strerror(err));
				}

				err = uv_ip4_name(
				  reinterpret_cast<const struct sockaddr_in*>(std::addressof(addrStorage)),
				  ipBuffer,
				  sizeof(ipBuffer));

				if (err != 0)
				{
					MS_THROW_TYPE_ERROR("uv_ipv4_name() failed [ip:'%s']: %s", ip.c_str(), uv_strerror(err));
				}

				ip.assign(ipBuffer);

				break;
			}

			case AF_INET6:
			{
				err = uv_ip6_addr(ip.c_str(), 0, reinterpret_cast<struct sockaddr_in6*>(&addrStorage));

				if (err != 0)
				{
					MS_THROW_TYPE_ERROR("uv_ip6_addr() failed [ip:'%s']: %s", ip.c_str(), uv_strerror(err));
				}

				err = uv_ip6_name(
				  reinterpret_cast<const struct sockaddr_in6*>(std::addressof(addrStorage)),
				  ipBuffer,
				  sizeof(ipBuffer));

				if (err != 0)
				{
					MS_THROW_TYPE_ERROR("uv_ipv6_name() failed [ip:'%s']: %s", ip.c_str(), uv_strerror(err));
				}

				ip.assign(ipBuffer);

				break;
			}

			default:
			{
				MS_THROW_TYPE_ERROR("invalid IP '%s'", ip.c_str());
			}
		}
	}
} // namespace Utils
