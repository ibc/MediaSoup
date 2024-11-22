#ifndef MS_RTC_RTP_STREAM_HPP
#define MS_RTC_RTP_STREAM_HPP

#include "common.hpp"
#include "DepLibUV.hpp"
#include "FBS/rtpStream.h"
#include "RTC/RTCP/FeedbackPsFir.hpp"
#include "RTC/RTCP/FeedbackPsPli.hpp"
#include "RTC/RTCP/FeedbackRtpNack.hpp"
#include "RTC/RTCP/Packet.hpp"
#include "RTC/RTCP/ReceiverReport.hpp"
#include "RTC/RTCP/Sdes.hpp"
#include "RTC/RTCP/SenderReport.hpp"
#include "RTC/RTCP/XrDelaySinceLastRr.hpp"
#include "RTC/RTCP/XrReceiverReferenceTime.hpp"
#include "RTC/RtpDictionaries.hpp"
#include "RTC/RtpPacket.hpp"
#include "RTC/RtxStream.hpp"
#include <string>
#include <vector>

namespace RTC
{
	class RtpStream
	{
	protected:
		class Listener
		{
		public:
			virtual ~Listener() = default;

		public:
			virtual void OnRtpStreamScore(RTC::RtpStream* rtpStream, uint8_t score, uint8_t previousScore) = 0;
		};

	public:
		struct Params
		{
			flatbuffers::Offset<FBS::RtpStream::Params> FillBuffer(
			  flatbuffers::FlatBufferBuilder& builder) const;

			size_t encodingIdx{ 0u };
			uint32_t ssrc{ 0u };
			uint8_t payloadType{ 0u };
			RTC::RtpCodecMimeType mimeType;
			uint32_t clockRate{ 0u };
			std::string rid;
			std::string cname;
			uint32_t rtxSsrc{ 0u };
			uint8_t rtxPayloadType{ 0u };
			bool useNack{ false };
			bool usePli{ false };
			bool useFir{ false };
			bool useInBandFec{ false };
			bool useDtx{ false };
			uint8_t spatialLayers{ 1u };
			uint8_t temporalLayers{ 1u };
		};

	public:
		RtpStream(RTC::RtpStream::Listener* listener, RTC::RtpStream::Params& params, uint8_t initialScore);
		virtual ~RtpStream();

		flatbuffers::Offset<FBS::RtpStream::Dump> FillBuffer(flatbuffers::FlatBufferBuilder& builder) const;
		virtual flatbuffers::Offset<FBS::RtpStream::Stats> FillBufferStats(
		  flatbuffers::FlatBufferBuilder& builder);
		uint32_t GetEncodingIdx() const
		{
			return this->params.encodingIdx;
		}
		uint32_t GetSsrc() const
		{
			return this->params.ssrc;
		}
		uint8_t GetPayloadType() const
		{
			return this->params.payloadType;
		}
		const RTC::RtpCodecMimeType& GetMimeType() const
		{
			return this->params.mimeType;
		}
		uint32_t GetClockRate() const
		{
			return this->params.clockRate;
		}
		const std::string& GetRid() const
		{
			return this->params.rid;
		}
		const std::string& GetCname() const
		{
			return this->params.cname;
		}
		bool HasRtx() const
		{
			return this->rtxStream != nullptr;
		}
		virtual void SetRtx(uint8_t payloadType, uint32_t ssrc);
		uint32_t GetRtxSsrc() const
		{
			return this->params.rtxSsrc;
		}
		uint8_t GetRtxPayloadType() const
		{
			return this->params.rtxPayloadType;
		}
		uint8_t GetSpatialLayers() const
		{
			return this->params.spatialLayers;
		}
		bool HasDtx() const
		{
			return this->params.useDtx;
		}
		uint8_t GetTemporalLayers() const
		{
			return this->params.temporalLayers;
		}
		virtual bool ReceiveStreamPacket(RTC::RtpPacket* packet);
		virtual void Pause()                                                                     = 0;
		virtual void Resume()                                                                    = 0;
		virtual uint32_t GetBitrate(uint64_t nowMs)                                              = 0;
		virtual uint32_t GetBitrate(uint64_t nowMs, uint8_t spatialLayer, uint8_t temporalLayer) = 0;
		virtual uint32_t GetSpatialLayerBitrate(uint64_t nowMs, uint8_t spatialLayer)            = 0;
		virtual uint32_t GetLayerBitrate(uint64_t nowMs, uint8_t spatialLayer, uint8_t temporalLayer) = 0;
		void ResetScore(uint8_t score, bool notify);
		uint8_t GetFractionLost() const
		{
			return this->fractionLost;
		}
		float GetLossPercentage() const
		{
			return static_cast<float>(this->fractionLost) * 100 / 256;
		}
		float GetRtt() const
		{
			return this->rtt;
		}
		uint64_t GetMaxPacketMs() const
		{
			return this->maxPacketMs;
		}
		uint32_t GetMaxPacketTs() const
		{
			return this->maxPacketTs;
		}
		uint64_t GetSenderReportNtpMs() const
		{
			return this->lastSenderReportNtpMs;
		}
		uint32_t GetSenderReportTs() const
		{
			return this->lastSenderReportTs;
		}
		uint8_t GetScore() const
		{
			return this->score;
		}
		uint64_t GetActiveMs() const
		{
			return DepLibUV::GetTimeMs() - this->activeSinceMs;
		}

	protected:
		bool UpdateSeq(RTC::RtpPacket* packet);
		void UpdateScore(uint8_t score);
		void PacketRetransmitted(RTC::RtpPacket* packet);
		void PacketRepaired(RTC::RtpPacket* packet);
		uint32_t GetExpectedPackets() const
		{
			return (this->cycles + this->maxSeq) - this->baseSeq + 1;
		}

	private:
		void InitSeq(uint16_t seq);

		/* Pure virtual method that must be implemented by the subclass. */
	protected:
		virtual void UserOnSequenceNumberReset() = 0;

	protected:
		// Given as argument.
		RTC::RtpStream::Listener* listener{ nullptr };
		Params params;
		// Others.
		//   https://tools.ietf.org/html/rfc3550#appendix-A.1 stuff.
		// Highest seq. number seen.
		uint16_t maxSeq{ 0u };
		// Shifted count of seq. number cycles.
		uint32_t cycles{ 0u };
		// Base seq number.
		uint32_t baseSeq{ 0u };
		// Last 'bad' seq number + 1.
		uint32_t badSeq{ 0u };
		// Highest timestamp seen.
		uint32_t maxPacketTs{ 0u };
		// When the packet with highest timestammp was seen.
		uint64_t maxPacketMs{ 0u };
		uint32_t packetsLost{ 0u };
		uint8_t fractionLost{ 0u };
		size_t packetsDiscarded{ 0u };
		size_t packetsRetransmitted{ 0u };
		size_t packetsRepaired{ 0u };
		size_t nackCount{ 0u };
		size_t nackPacketCount{ 0u };
		size_t pliCount{ 0u };
		size_t firCount{ 0u };
		// Packets repaired at last interval for score calculation.
		size_t repairedPriorScore{ 0u };
		// Packets retransmitted at last interval for score calculation.
		size_t retransmittedPriorScore{ 0u };
		// NTP timestamp in last Sender Report (in ms).
		uint64_t lastSenderReportNtpMs{ 0u };
		// RTP timestamp in last Sender Report.
		uint32_t lastSenderReportTs{ 0u };
		float rtt{ 0.0f };
		// Instance of RtxStream.
		RTC::RtxStream* rtxStream{ nullptr };

	private:
		// Score related.
		uint8_t score{ 0u };
		std::vector<uint8_t> scores;
		// Whether at least a RTP packet has been received.
		bool started{ false };
		// Last time since the stream is active.
		uint64_t activeSinceMs{ 0u };
	};
} // namespace RTC

#endif
