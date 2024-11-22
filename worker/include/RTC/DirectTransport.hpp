#ifndef MS_RTC_DIRECT_TRANSPORT_HPP
#define MS_RTC_DIRECT_TRANSPORT_HPP

#include "RTC/Shared.hpp"
#include "RTC/Transport.hpp"

namespace RTC
{
	class DirectTransport : public RTC::Transport
	{
	public:
		DirectTransport(
		  RTC::Shared* shared,
		  const std::string& id,
		  RTC::Transport::Listener* listener,
		  const FBS::DirectTransport::DirectTransportOptions* options);
		~DirectTransport() override;

	public:
		flatbuffers::Offset<FBS::DirectTransport::GetStatsResponse> FillBufferStats(
		  flatbuffers::FlatBufferBuilder& builder);
		flatbuffers::Offset<FBS::DirectTransport::DumpResponse> FillBuffer(
		  flatbuffers::FlatBufferBuilder& builder) const;

	private:
		bool IsConnected() const override;
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

		/* Methods inherited from Channel::ChannelSocket::RequestHandler. */
	public:
		void HandleRequest(Channel::ChannelRequest* request) override;

		/* Methods inherited from Channel::ChannelSocket::NotificationHandler. */
	public:
		void HandleNotification(Channel::ChannelNotification* notification) override;
	};
} // namespace RTC

#endif
