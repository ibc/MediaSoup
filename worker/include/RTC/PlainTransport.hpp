#ifndef MS_RTC_PLAIN_TRANSPORT_HPP
#define MS_RTC_PLAIN_TRANSPORT_HPP

#include "FBS/plainTransport.h"
#include "RTC/Shared.hpp"
#include "RTC/SrtpSession.hpp"
#include "RTC/Transport.hpp"
#include "RTC/TransportTuple.hpp"
#include "RTC/UdpSocket.hpp"
#include <absl/container/flat_hash_map.h>

namespace RTC
{
	class PlainTransport : public RTC::Transport, public RTC::UdpSocket::Listener
	{
	public:
		PlainTransport(
		  RTC::Shared* shared,
		  const std::string& id,
		  RTC::Transport::Listener* listener,
		  const FBS::PlainTransport::PlainTransportOptions* options);
		~PlainTransport() override;

	public:
		flatbuffers::Offset<FBS::PlainTransport::GetStatsResponse> FillBufferStats(
		  flatbuffers::FlatBufferBuilder& builder);
		flatbuffers::Offset<FBS::PlainTransport::DumpResponse> FillBuffer(
		  flatbuffers::FlatBufferBuilder& builder) const;

		/* Methods inherited from Channel::ChannelSocket::RequestHandler. */
	public:
		void HandleRequest(Channel::ChannelRequest* request) override;
		void HandleNotification(Channel::ChannelNotification* notification) override;

	private:
		bool IsConnected() const override;
		bool HasSrtp() const;
		bool IsSrtpReady() const;
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
		void EmitTuple() const;
		void EmitRtcpTuple() const;

		/* Pure virtual methods inherited from RTC::UdpSocket::Listener. */
	public:
		void OnUdpSocketPacketReceived(
		  RTC::UdpSocket* socket, const uint8_t* data, size_t len, const struct sockaddr* remoteAddr) override;

	private:
		// Allocated by this.
		RTC::UdpSocket* udpSocket{ nullptr };
		RTC::UdpSocket* rtcpUdpSocket{ nullptr };
		RTC::TransportTuple* tuple{ nullptr };
		RTC::TransportTuple* rtcpTuple{ nullptr };
		RTC::SrtpSession* srtpRecvSession{ nullptr };
		RTC::SrtpSession* srtpSendSession{ nullptr };
		// Others.
		ListenInfo listenInfo;
		ListenInfo rtcpListenInfo;
		bool rtcpMux{ true };
		bool comedia{ false };
		struct sockaddr_storage remoteAddrStorage
		{
		};
		struct sockaddr_storage rtcpRemoteAddrStorage
		{
		};
		RTC::SrtpSession::CryptoSuite srtpCryptoSuite{
			RTC::SrtpSession::CryptoSuite::AES_CM_128_HMAC_SHA1_80
		};
		std::string srtpKey;
		size_t srtpMasterLength{ 0 };
		std::string srtpKeyBase64;
		bool connectCalled{ false }; // Whether connect() was succesfully called.
	};
} // namespace RTC

#endif
