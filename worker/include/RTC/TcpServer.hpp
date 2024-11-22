#ifndef MS_RTC_TCP_SERVER_HPP
#define MS_RTC_TCP_SERVER_HPP

#include "common.hpp"
#include "RTC/TcpConnection.hpp"
#include "RTC/Transport.hpp"
#include "handles/TcpConnectionHandle.hpp"
#include "handles/TcpServerHandle.hpp"
#include <string>

namespace RTC
{
	class TcpServer : public ::TcpServerHandle
	{
	public:
		class Listener
		{
		public:
			virtual ~Listener() = default;

		public:
			virtual void OnRtcTcpConnectionClosed(
			  RTC::TcpServer* tcpServer, RTC::TcpConnection* connection) = 0;
		};

	public:
		TcpServer(
		  Listener* listener,
		  RTC::TcpConnection::Listener* connListener,
		  std::string& ip,
		  uint16_t port,
		  RTC::Transport::SocketFlags& flags);
		TcpServer(
		  Listener* listener,
		  RTC::TcpConnection::Listener* connListener,
		  std::string& ip,
		  uint16_t minPort,
		  uint16_t maxPort,
		  RTC::Transport::SocketFlags& flags,
		  uint64_t& portRangeHash);
		~TcpServer() override;

		/* Pure virtual methods inherited from ::TcpServerHandle. */
	public:
		void UserOnTcpConnectionAlloc() override;
		void UserOnTcpConnectionClosed(::TcpConnectionHandle* connection) override;

	private:
		// Passed by argument.
		Listener* listener{ nullptr };
		RTC::TcpConnection::Listener* connListener{ nullptr };
		bool fixedPort{ false };
		uint64_t portRangeHash{ 0u };
	};
} // namespace RTC

#endif
