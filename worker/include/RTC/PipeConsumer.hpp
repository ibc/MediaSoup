#ifndef MS_RTC_PIPECONSUMER_HPP
#define MS_RTC_PIPECONSUMER_HPP

#include "RTC/Consumer.hpp"
#include "RTC/SeqManager.hpp"
#include "RTC/Shared.hpp"

namespace RTC
{
	class PipeConsumer : public RTC::Consumer, public RTC::RtpStreamSend::Listener
	{
	public:
		PipeConsumer(
		  RTC::Shared* shared,
		  const std::string& id,
		  const std::string& producerId,
		  RTC::Consumer::Listener* listener,
		  const FBS::Transport::ConsumeRequest* data);
		~PipeConsumer() override;

	public:
		flatbuffers::Offset<FBS::Consumer::DumpResponse> FillBuffer(
		  flatbuffers::FlatBufferBuilder& builder) const;
		flatbuffers::Offset<FBS::Consumer::GetStatsResponse> FillBufferStats(
		  flatbuffers::FlatBufferBuilder& builder) override;
		flatbuffers::Offset<FBS::Consumer::ConsumerScore> FillBufferScore(
		  flatbuffers::FlatBufferBuilder& builder) const override;
		void ProducerRtpStream(RTC::RtpStreamRecv* rtpStream, uint32_t mappedSsrc) override;
		void ProducerNewRtpStream(RTC::RtpStreamRecv* rtpStream, uint32_t mappedSsrc) override;
		void ProducerRtpStreamScore(
		  RTC::RtpStreamRecv* rtpStream, uint8_t score, uint8_t previousScore) override;
		void ProducerRtcpSenderReport(RTC::RtpStreamRecv* rtpStream, bool first) override;
		uint8_t GetBitratePriority() const override;
		uint32_t IncreaseLayer(uint32_t bitrate, bool considerLoss) override;
		void ApplyLayers() override;
		uint32_t GetDesiredBitrate() const override;
		void SendRtpPacket(RTC::RtpPacket* packet, std::shared_ptr<RTC::RtpPacket>& sharedPacket) override;
		bool GetRtcp(RTC::RTCP::CompoundPacket* packet, uint64_t nowMs) override;
		const std::vector<RTC::RtpStreamSend*>& GetRtpStreams() const override
		{
			return this->rtpStreams;
		}
		void NeedWorstRemoteFractionLost(uint32_t mappedSsrc, uint8_t& worstRemoteFractionLost) override;
		void ReceiveNack(RTC::RTCP::FeedbackRtpNackPacket* nackPacket) override;
		void ReceiveKeyFrameRequest(RTC::RTCP::FeedbackPs::MessageType messageType, uint32_t ssrc) override;
		void ReceiveRtcpReceiverReport(RTC::RTCP::ReceiverReport* report) override;
		void ReceiveRtcpXrReceiverReferenceTime(RTC::RTCP::ReceiverReferenceTime* report) override;
		uint32_t GetTransmissionRate(uint64_t nowMs) override;
		float GetRtt() const override;

		/* Methods inherited from Channel::ChannelSocket::RequestHandler. */
	public:
		void HandleRequest(Channel::ChannelRequest* request) override;

	private:
		void UserOnTransportConnected() override;
		void UserOnTransportDisconnected() override;
		void UserOnPaused() override;
		void UserOnResumed() override;
		void CreateRtpStreams();
		void RequestKeyFrame();

		/* Pure virtual methods inherited from RtpStreamSend::Listener. */
	public:
		void OnRtpStreamScore(RTC::RtpStream* rtpStream, uint8_t score, uint8_t previousScore) override;
		void OnRtpStreamRetransmitRtpPacket(RTC::RtpStreamSend* rtpStream, RTC::RtpPacket* packet) override;

	private:
		// Allocated by this.
		std::vector<RTC::RtpStreamSend*> rtpStreams;
		// Others.
		absl::flat_hash_map<uint32_t, uint32_t> mapMappedSsrcSsrc;
		absl::flat_hash_map<uint32_t, RTC::RtpStreamSend*> mapSsrcRtpStream;
		bool keyFrameSupported{ false };
		absl::flat_hash_map<RTC::RtpStreamSend*, bool> mapRtpStreamSyncRequired;
		absl::flat_hash_map<RTC::RtpStreamSend*, std::unique_ptr<RTC::SeqManager<uint16_t>>>
		  mapRtpStreamRtpSeqManager;
	};
} // namespace RTC

#endif
