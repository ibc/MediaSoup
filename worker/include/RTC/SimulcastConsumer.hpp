#ifndef MS_RTC_SIMULCAST_CONSUMER_HPP
#define MS_RTC_SIMULCAST_CONSUMER_HPP

#include "FBS/consumer.h"
#include "RTC/Codecs/PayloadDescriptorHandler.hpp"
#include "RTC/Consumer.hpp"
#include "RTC/SeqManager.hpp"
#include "RTC/Shared.hpp"

namespace RTC
{
	class SimulcastConsumer : public RTC::Consumer, public RTC::RtpStreamSend::Listener
	{
	public:
		SimulcastConsumer(
		  RTC::Shared* shared,
		  const std::string& id,
		  const std::string& producerId,
		  RTC::Consumer::Listener* listener,
		  const FBS::Transport::ConsumeRequest* data);
		~SimulcastConsumer() override;

	public:
		flatbuffers::Offset<FBS::Consumer::DumpResponse> FillBuffer(
		  flatbuffers::FlatBufferBuilder& builder) const;
		flatbuffers::Offset<FBS::Consumer::GetStatsResponse> FillBufferStats(
		  flatbuffers::FlatBufferBuilder& builder) override;
		flatbuffers::Offset<FBS::Consumer::ConsumerScore> FillBufferScore(
		  flatbuffers::FlatBufferBuilder& builder) const override;
		RTC::Consumer::Layers GetPreferredLayers() const override
		{
			RTC::Consumer::Layers layers;

			layers.spatial  = this->preferredSpatialLayer;
			layers.temporal = this->preferredTemporalLayer;

			return layers;
		}
		bool IsActive() const override
		{
			// clang-format off
			return (
				RTC::Consumer::IsActive() &&
				std::any_of(
					this->producerRtpStreams.begin(),
					this->producerRtpStreams.end(),
					[](const RTC::RtpStreamRecv* rtpStream)
					{
						// If there is no RTP inactivity check do not consider the stream
						// inactive despite it has score 0.
						return (rtpStream != nullptr && (rtpStream->GetScore() > 0u || !rtpStream->HasRtpInactivityCheckEnabled()));
					}
				)
			);
			// clang-format on
		}
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
		void CreateRtpStream();
		void RequestKeyFrames();
		void RequestKeyFrameForTargetSpatialLayer();
		void RequestKeyFrameForCurrentSpatialLayer();
		void MayChangeLayers(bool force = false);
		bool RecalculateTargetLayers(int16_t& newTargetSpatialLayer, int16_t& newTargetTemporalLayer) const;
		void UpdateTargetLayers(int16_t newTargetSpatialLayer, int16_t newTargetTemporalLayer);
		bool CanSwitchToSpatialLayer(int16_t spatialLayer) const;
		void EmitScore() const;
		void EmitLayersChange() const;
		RTC::RtpStreamRecv* GetProducerCurrentRtpStream() const;
		RTC::RtpStreamRecv* GetProducerTargetRtpStream() const;
		RTC::RtpStreamRecv* GetProducerTsReferenceRtpStream() const;

		/* Pure virtual methods inherited from RtpStreamSend::Listener. */
	public:
		void OnRtpStreamScore(RTC::RtpStream* rtpStream, uint8_t score, uint8_t previousScore) override;
		void OnRtpStreamRetransmitRtpPacket(RTC::RtpStreamSend* rtpStream, RTC::RtpPacket* packet) override;

	private:
		// Allocated by this.
		RTC::RtpStreamSend* rtpStream{ nullptr };
		// Others.
		absl::flat_hash_map<uint32_t, int16_t> mapMappedSsrcSpatialLayer;
		std::vector<RTC::RtpStreamSend*> rtpStreams;
		std::vector<RTC::RtpStreamRecv*> producerRtpStreams; // Indexed by spatial layer.
		bool syncRequired{ false };
		int16_t spatialLayerToSync{ -1 };
		bool lastSentPacketHasMarker{ false };
		std::unique_ptr<RTC::SeqManager<uint16_t>> rtpSeqManager;
		int16_t preferredSpatialLayer{ -1 };
		int16_t preferredTemporalLayer{ -1 };
		int16_t provisionalTargetSpatialLayer{ -1 };
		int16_t provisionalTargetTemporalLayer{ -1 };
		int16_t targetSpatialLayer{ -1 };
		int16_t targetTemporalLayer{ -1 };
		int16_t currentSpatialLayer{ -1 };
		int16_t tsReferenceSpatialLayer{ -1 }; // Used for RTP TS sync.
		uint16_t snReferenceSpatialLayer{ 0 };
		bool checkingForOldPacketsInSpatialLayer{ false };
		std::unique_ptr<RTC::Codecs::EncodingContext> encodingContext;
		uint32_t tsOffset{ 0u }; // RTP Timestamp offset.
		bool keyFrameForTsOffsetRequested{ false };
		uint64_t lastBweDowngradeAtMs{ 0u }; // Last time we moved to lower spatial layer due to BWE.
	};
} // namespace RTC

#endif
