#ifndef MS_RTC_PIPE_TRANSPORT_HPP
#define MS_RTC_PIPE_TRANSPORT_HPP

#include "FBS/pipeTransport.h"
#include "RTC/Shared.hpp"
#include "RTC/SrtpSession.hpp"
#include "RTC/Transport.hpp"
#include "RTC/TransportTuple.hpp"
#include "RTC/UdpSocket.hpp"

namespace RTC
{
	class PipeTransport : public RTC::Transport, public RTC::UdpSocket::Listener
	{
	private:
		static RTC::SrtpSession::CryptoSuite srtpCryptoSuite;
		static std::string srtpCryptoSuiteString;
		static size_t srtpMasterLength;

	public:
		PipeTransport(
		  RTC::Shared* shared,
		  const std::string& id,
		  RTC::Transport::Listener* listener,
		  const FBS::PipeTransport::PipeTransportOptions* options);
		~PipeTransport() override;

	public:
		flatbuffers::Offset<FBS::PipeTransport::GetStatsResponse> FillBufferStats(
		  flatbuffers::FlatBufferBuilder& builder);
		flatbuffers::Offset<FBS::PipeTransport::DumpResponse> FillBuffer(
		  flatbuffers::FlatBufferBuilder& builder) const;

		/* Methods inherited from Channel::ChannelSocket::RequestHandler. */
	public:
		void HandleRequest(Channel::ChannelRequest* request) override;

		/* Methods inherited from Channel::ChannelSocket::NotificationHandler. */
	public:
		void HandleNotification(Channel::ChannelNotification* notification) override;

	private:
		bool IsConnected() const override;
		bool HasSrtp() const;
		void SendRtpPacket(
		  RTC::Consumer* consumer,
		  RTC::RtpPacket* packet,
		  RTC::Transport::onSendCallback* cb = nullptr) override;
		void SendRtcpPacket(RTC::RTCP::Packet* packet) override;
		void SendRtcpCompoundPacket(RTC::RTCP::CompoundPacket* packet) override;
		void SendMessage(
		  RTC::DataConsumer* dataConsumer,
		  const uint8_t* msg,
		  size_t len,
		  uint32_t ppid,
		  onQueuedCallback* cb = nullptr) override;
		void SendSctpData(const uint8_t* data, size_t len) override;
		void RecvStreamClosed(uint32_t ssrc) override;
		void SendStreamClosed(uint32_t ssrc) override;
		void OnPacketReceived(RTC::TransportTuple* tuple, const uint8_t* data, size_t len);
		void OnRtpDataReceived(RTC::TransportTuple* tuple, const uint8_t* data, size_t len);
		void OnRtcpDataReceived(RTC::TransportTuple* tuple, const uint8_t* data, size_t len);
		void OnSctpDataReceived(RTC::TransportTuple* tuple, const uint8_t* data, size_t len);

		/* Pure virtual methods inherited from RTC::UdpSocket::Listener. */
	public:
		void OnUdpSocketPacketReceived(
		  RTC::UdpSocket* socket, const uint8_t* data, size_t len, const struct sockaddr* remoteAddr) override;

	private:
		// Allocated by this.
		RTC::UdpSocket* udpSocket{ nullptr };
		RTC::TransportTuple* tuple{ nullptr };
		RTC::SrtpSession* srtpRecvSession{ nullptr };
		RTC::SrtpSession* srtpSendSession{ nullptr };
		// Others.
		ListenInfo listenInfo;
		struct sockaddr_storage remoteAddrStorage
		{
		};
		bool rtx{ false };
		std::string srtpKey;
		std::string srtpKeyBase64;
	};
} // namespace RTC

#endif
