#ifndef MS_RTC_PRODUCER_HPP
#define MS_RTC_PRODUCER_HPP

#include "common.hpp"
#include "Channel/ChannelRequest.hpp"
#include "Channel/ChannelSocket.hpp"
#include "RTC/KeyFrameRequestManager.hpp"
#include "RTC/RTCP/CompoundPacket.hpp"
#include "RTC/RTCP/Packet.hpp"
#include "RTC/RTCP/SenderReport.hpp"
#include "RTC/RTCP/XrDelaySinceLastRr.hpp"
#include "RTC/RtpDictionaries.hpp"
#include "RTC/RtpHeaderExtensionIds.hpp"
#include "RTC/RtpPacket.hpp"
#include "RTC/RtpStreamRecv.hpp"
#include "RTC/Shared.hpp"
#include <string>
#include <vector>

namespace RTC
{
	class Producer : public RTC::RtpStreamRecv::Listener,
	                 public RTC::KeyFrameRequestManager::Listener,
	                 public Channel::ChannelSocket::RequestHandler,
	                 public Channel::ChannelSocket::NotificationHandler
	{
	public:
		class Listener
		{
		public:
			virtual ~Listener() = default;

		public:
			virtual void OnProducerReceiveData(RTC::Producer* producer, size_t len)                  = 0;
			virtual void OnProducerReceiveRtpPacket(RTC::Producer* producer, RTC::RtpPacket* packet) = 0;
			virtual void OnProducerPaused(RTC::Producer* producer)                                   = 0;
			virtual void OnProducerResumed(RTC::Producer* producer)                                  = 0;
			virtual void OnProducerNewRtpStream(
			  RTC::Producer* producer, RTC::RtpStreamRecv* rtpStream, uint32_t mappedSsrc) = 0;
			virtual void OnProducerRtpStreamScore(
			  RTC::Producer* producer,
			  RTC::RtpStreamRecv* rtpStream,
			  uint8_t score,
			  uint8_t previousScore) = 0;
			virtual void OnProducerRtcpSenderReport(
			  RTC::Producer* producer, RTC::RtpStreamRecv* rtpStream, bool first)                     = 0;
			virtual void OnProducerRtpPacketReceived(RTC::Producer* producer, RTC::RtpPacket* packet) = 0;
			virtual void OnProducerSendRtcpPacket(RTC::Producer* producer, RTC::RTCP::Packet* packet) = 0;
			virtual void OnProducerNeedWorstRemoteFractionLost(
			  RTC::Producer* producer, uint32_t mappedSsrc, uint8_t& worstRemoteFractionLost) = 0;
		};

	private:
		struct RtpEncodingMapping
		{
			std::string rid;
			uint32_t ssrc{ 0 };
			uint32_t mappedSsrc{ 0 };
		};

	private:
		struct RtpMapping
		{
			absl::flat_hash_map<uint8_t, uint8_t> codecs;
			std::vector<RtpEncodingMapping> encodings;
		};

	private:
		struct VideoOrientation
		{
			bool camera{ false };
			bool flip{ false };
			uint16_t rotation{ 0 };
		};

	public:
		enum class ReceiveRtpPacketResult
		{
			DISCARDED = 0,
			MEDIA     = 1,
			RETRANSMISSION
		};

	private:
		struct TraceEventTypes
		{
			bool rtp{ false };
			bool keyframe{ false };
			bool nack{ false };
			bool pli{ false };
			bool fir{ false };
			bool sr{ false };
		};

	public:
		Producer(
		  RTC::Shared* shared,
		  const std::string& id,
		  RTC::Producer::Listener* listener,
		  const FBS::Transport::ProduceRequest* data);
		~Producer() override;

	public:
		flatbuffers::Offset<FBS::Producer::DumpResponse> FillBuffer(
		  flatbuffers::FlatBufferBuilder& builder) const;
		flatbuffers::Offset<FBS::Producer::GetStatsResponse> FillBufferStats(
		  flatbuffers::FlatBufferBuilder& builder);
		RTC::Media::Kind GetKind() const
		{
			return this->kind;
		}
		const RTC::RtpParameters& GetRtpParameters() const
		{
			return this->rtpParameters;
		}
		const struct RTC::RtpHeaderExtensionIds& GetRtpHeaderExtensionIds() const
		{
			return this->rtpHeaderExtensionIds;
		}
		RTC::RtpParameters::Type GetType() const
		{
			return this->type;
		}
		bool IsPaused() const
		{
			return this->paused;
		}
		const absl::flat_hash_map<RTC::RtpStreamRecv*, uint32_t>& GetRtpStreams()
		{
			return this->mapRtpStreamMappedSsrc;
		}
		const std::vector<uint8_t>* GetRtpStreamScores() const
		{
			return std::addressof(this->rtpStreamScores);
		}
		ReceiveRtpPacketResult ReceiveRtpPacket(RTC::RtpPacket* packet);
		void ReceiveRtcpSenderReport(RTC::RTCP::SenderReport* report);
		void ReceiveRtcpXrDelaySinceLastRr(RTC::RTCP::DelaySinceLastRr::SsrcInfo* ssrcInfo);
		bool GetRtcp(RTC::RTCP::CompoundPacket* packet, uint64_t nowMs);
		void RequestKeyFrame(uint32_t mappedSsrc);

		/* Methods inherited from Channel::ChannelSocket::RequestHandler. */
	public:
		void HandleRequest(Channel::ChannelRequest* request) override;

		/* Methods inherited from Channel::ChannelSocket::NotificationHandler. */
	public:
		void HandleNotification(Channel::ChannelNotification* notification) override;

	private:
		RTC::RtpStreamRecv* GetRtpStream(RTC::RtpPacket* packet);
		RTC::RtpStreamRecv* CreateRtpStream(
		  RTC::RtpPacket* packet, const RTC::RtpCodecParameters& mediaCodec, size_t encodingIdx);
		void NotifyNewRtpStream(RTC::RtpStreamRecv* rtpStream);
		void PreProcessRtpPacket(RTC::RtpPacket* packet);
		bool MangleRtpPacket(RTC::RtpPacket* packet, RTC::RtpStreamRecv* rtpStream) const;
		void PostProcessRtpPacket(RTC::RtpPacket* packet);
		void EmitScore() const;
		void EmitTraceEventRtpAndKeyFrameTypes(RTC::RtpPacket* packet, bool isRtx = false) const;
		void EmitTraceEventKeyFrameType(RTC::RtpPacket* packet, bool isRtx = false) const;
		void EmitTraceEventPliType(uint32_t ssrc) const;
		void EmitTraceEventFirType(uint32_t ssrc) const;
		void EmitTraceEventNackType() const;
		void EmitTraceEventSrType(RTC::RTCP::SenderReport* report) const;
		void EmitTraceEvent(flatbuffers::Offset<FBS::Producer::TraceNotification>& notification) const;

		/* Pure virtual methods inherited from RTC::RtpStreamRecv::Listener. */
	public:
		void OnRtpStreamScore(RTC::RtpStream* rtpStream, uint8_t score, uint8_t previousScore) override;
		void OnRtpStreamSendRtcpPacket(RTC::RtpStreamRecv* rtpStream, RTC::RTCP::Packet* packet) override;
		void OnRtpStreamNeedWorstRemoteFractionLost(
		  RTC::RtpStreamRecv* rtpStream, uint8_t& worstRemoteFractionLost) override;

		/* Pure virtual methods inherited from RTC::KeyFrameRequestManager::Listener. */
	public:
		void OnKeyFrameNeeded(RTC::KeyFrameRequestManager* keyFrameRequestManager, uint32_t ssrc) override;

	public:
		// Passed by argument.
		const std::string id;

	private:
		// Passed by argument.
		RTC::Shared* shared{ nullptr };
		RTC::Producer::Listener* listener{ nullptr };
		// Allocated by this.
		absl::flat_hash_map<uint32_t, RTC::RtpStreamRecv*> mapSsrcRtpStream;
		RTC::KeyFrameRequestManager* keyFrameRequestManager{ nullptr };
		// Others.
		RTC::Media::Kind kind;
		RTC::RtpParameters rtpParameters;
		RTC::RtpParameters::Type type;
		struct RtpMapping rtpMapping;
		std::vector<RTC::RtpStreamRecv*> rtpStreamByEncodingIdx;
		std::vector<uint8_t> rtpStreamScores;
		absl::flat_hash_map<uint32_t, RTC::RtpStreamRecv*> mapRtxSsrcRtpStream;
		absl::flat_hash_map<RTC::RtpStreamRecv*, uint32_t> mapRtpStreamMappedSsrc;
		absl::flat_hash_map<uint32_t, uint32_t> mapMappedSsrcSsrc;
		struct RTC::RtpHeaderExtensionIds rtpHeaderExtensionIds;
		bool paused{ false };
		RTC::RtpPacket* currentRtpPacket{ nullptr };
		// Timestamp when last RTCP was sent.
		uint64_t lastRtcpSentTime{ 0u };
		uint16_t maxRtcpInterval{ 0u };
		// Video orientation.
		bool videoOrientationDetected{ false };
		struct VideoOrientation videoOrientation;
		struct TraceEventTypes traceEventTypes;
		// Static buffer.
		thread_local static uint8_t* buffer;
	};
} // namespace RTC

#endif
