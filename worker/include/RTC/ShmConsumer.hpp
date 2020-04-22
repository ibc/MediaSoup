#ifndef MS_RTC_SHM_CONSUMER_HPP
#define MS_RTC_SHM_CONSUMER_HPP

#include "json.hpp"
#include "DepLibSfuShm.hpp"
#include "RTC/Consumer.hpp"
#include "RTC/RtpStreamSend.hpp"
#include "RTC/SeqManager.hpp"
#include "RTC/RateCalculator.hpp"

using json = nlohmann::json;

namespace RTC
{
	class ShmConsumer : public RTC::Consumer, public RTC::RtpStreamSend::Listener
	{
	public:
		ShmConsumer(const std::string& id, RTC::Consumer::Listener* listener, json& data, DepLibSfuShm::SfuShmCtx *shmCtx);
		~ShmConsumer() override;

	public:
		void FillJson(json& jsonObject) const override;
		void FillJsonStats(json& jsonArray) const override;
		void FillJsonScore(json& jsonObject) const override;
		void HandleRequest(Channel::Request* request) override;
		bool IsActive() const override;
		void ProducerRtpStream(RTC::RtpStream* rtpStream, uint32_t mappedSsrc) override;
		void ProducerNewRtpStream(RTC::RtpStream* rtpStream, uint32_t mappedSsrc) override;
		void ProducerRtpStreamScore(RTC::RtpStream* rtpStream, uint8_t score, uint8_t previousScore) override;
		void ProducerRtcpSenderReport(RTC::RtpStream* rtpStream, bool first) override;
		uint8_t GetBitratePriority() const override;
		uint32_t IncreaseLayer(uint32_t bitrate, bool considerLoss) override;
		void ApplyLayers() override;
		uint32_t GetDesiredBitrate() const override;

		void SendRtpPacket(RTC::RtpPacket* packet) override;
		void GetRtcp(RTC::RTCP::CompoundPacket* packet, RTC::RtpStreamSend* rtpStream, uint64_t now) override;
		std::vector<RTC::RtpStreamSend*> GetRtpStreams() override;
		void NeedWorstRemoteFractionLost(uint32_t mappedSsrc, uint8_t& worstRemoteFractionLost) override;
		void ReceiveNack(RTC::RTCP::FeedbackRtpNackPacket* nackPacket) override;
		void ReceiveKeyFrameRequest(RTC::RTCP::FeedbackPs::MessageType messageType, uint32_t ssrc) override;
		void ReceiveRtcpReceiverReport(RTC::RTCP::ReceiverReport* report) override;
		uint32_t GetTransmissionRate(uint64_t now) override;
		float GetRtt() const override;
		uint32_t GetBitrate(uint64_t nowMs);

	private:
		void UserOnTransportConnected() override;
		void UserOnTransportDisconnected() override;
		void UserOnPaused() override;
		void UserOnResumed() override;
		void CreateRtpStream();
		void RequestKeyFrame();

		bool WritePacketToShm(RTC::RtpPacket* packet);

		/* Pure virtual methods inherited from RtpStreamSend::Listener. */
	public:
		void OnRtpStreamScore(RTC::RtpStream* rtpStream, uint8_t score, uint8_t previousScore) override;
		void OnRtpStreamRetransmitRtpPacket(RTC::RtpStreamSend* rtpStream, RTC::RtpPacket* packet) override;

	private:
		// Allocated by this.
		RTC::RtpStreamSend* rtpStream{ nullptr };
		// Others.
		std::vector<RTC::RtpStreamSend*> rtpStreams;
		RTC::RtpStream* producerRtpStream{ nullptr };
		bool keyFrameSupported{ false };
		bool syncRequired{ false };

		bool srReceived{ false }; // do not send until SR received

		RTC::SeqManager<uint16_t> rtpSeqManager;

		// Shm writing: a consumer will "send" RTP packets (either audio or video) into shm
		// RTCP packets and "app metadata" will be "sent" into shm by ShmTransport object
		DepLibSfuShm::SfuShmCtx *shmCtx{ nullptr }; // A handle to shm context which will be received from ShmTransport during transport.consume()
		sfushm_av_frame_frag_t   chunk;             // structure holding current chunk being written into shm, convenient to reuse timestamps data sometimes

		RTC::RtpDataCounter shmWriterCounter;      // Use to collect and report shm writing stats, for RTP only (RTCP is not handled by ShmConsumer) TODO: move into ShmCtx
	};

	/* Inline methods. */

	inline bool ShmConsumer::IsActive() const
	{
		return (RTC::Consumer::IsActive() && this->producerRtpStream);
	}

	inline std::vector<RTC::RtpStreamSend*> ShmConsumer::GetRtpStreams()
	{
		return this->rtpStreams;
	}

	/* Copied from RtpStreamSend */
	inline uint32_t ShmConsumer::GetBitrate(uint64_t nowMs)
	{
		return this->shmWriterCounter.GetBitrate(nowMs);
	}
} // namespace RTC

#endif
