#define MS_CLASS "RTC::RtpStream"
// #define MS_LOG_DEV_LEVEL 3

#include "RTC/RtpStream.hpp"
#include "Logger.hpp"
#include "RTC/SeqManager.hpp"

namespace RTC
{
	/* Static. */

	static constexpr uint16_t MaxDropout{ 3000 };
	static constexpr uint16_t MaxMisorder{ 1500 };
	static constexpr uint32_t RtpSeqMod{ 1 << 16 };
	static constexpr size_t ScoreHistogramLength{ 24 };

	/* Instance methods. */

	RtpStream::RtpStream(
	  RTC::RtpStream::Listener* listener, RTC::RtpStream::Params& params, uint8_t initialScore)
	  : listener(listener), params(params), score(initialScore), activeSinceMs(DepLibUV::GetTimeMs())
	{
		MS_TRACE();
	}

	RtpStream::~RtpStream()
	{
		MS_TRACE();

		delete this->rtxStream;
		this->rtxStream = nullptr;
	}

	flatbuffers::Offset<FBS::RtpStream::Dump> RtpStream::FillBuffer(
	  flatbuffers::FlatBufferBuilder& builder) const
	{
		MS_TRACE();

		// Add params.
		auto params = this->params.FillBuffer(builder);

		// Add rtxStream.
		flatbuffers::Offset<FBS::RtxStream::RtxDump> rtxStream;

		if (HasRtx())
		{
			rtxStream = this->rtxStream->FillBuffer(builder);
		}

		return FBS::RtpStream::CreateDump(builder, params, this->score, rtxStream);
	}

	flatbuffers::Offset<FBS::RtpStream::Stats> RtpStream::FillBufferStats(
	  flatbuffers::FlatBufferBuilder& builder)
	{
		MS_TRACE();

		const uint64_t nowMs = DepLibUV::GetTimeMs();
		const auto mediaKind = this->params.mimeType.type == RTC::RtpCodecMimeType::Type::AUDIO
		                         ? FBS::RtpParameters::MediaKind::AUDIO
		                         : FBS::RtpParameters::MediaKind::VIDEO;

		auto baseStats = FBS::RtpStream::CreateBaseStatsDirect(
		  builder,
		  nowMs,
		  this->params.ssrc,
		  mediaKind,
		  this->params.mimeType.ToString().c_str(),
		  this->packetsLost,
		  this->fractionLost,
		  this->packetsDiscarded,
		  this->packetsRetransmitted,
		  this->packetsRepaired,
		  this->nackCount,
		  this->nackPacketCount,
		  this->pliCount,
		  this->firCount,
		  this->score,
		  !this->params.rid.empty() ? this->params.rid.c_str() : nullptr,
		  this->params.rtxSsrc ? flatbuffers::Optional<uint32_t>(this->params.rtxSsrc)
		                       : flatbuffers::nullopt,
		  this->rtxStream ? this->rtxStream->GetPacketsDiscarded() : 0,
		  this->rtt > 0.0f ? this->rtt : 0);

		return FBS::RtpStream::CreateStats(
		  builder, FBS::RtpStream::StatsData::BaseStats, baseStats.Union());
	}

	void RtpStream::SetRtx(uint8_t payloadType, uint32_t ssrc)
	{
		MS_TRACE();

		this->params.rtxPayloadType = payloadType;
		this->params.rtxSsrc        = ssrc;

		if (HasRtx())
		{
			delete this->rtxStream;
			this->rtxStream = nullptr;
		}

		// Set RTX stream params.
		RTC::RtxStream::Params params;

		params.ssrc             = ssrc;
		params.payloadType      = payloadType;
		params.mimeType.type    = GetMimeType().type;
		params.mimeType.subtype = RTC::RtpCodecMimeType::Subtype::RTX;
		params.clockRate        = GetClockRate();
		params.rrid             = GetRid();
		params.cname            = GetCname();

		// Tell the RtpCodecMimeType to update its string based on current type and subtype.
		params.mimeType.UpdateMimeType();

		this->rtxStream = new RTC::RtxStream(params);
	}

	bool RtpStream::ReceiveStreamPacket(RTC::RtpPacket* packet)
	{
		MS_TRACE();

		const uint16_t seq = packet->GetSequenceNumber();

		// If this is the first packet seen, initialize stuff.
		if (!this->started)
		{
			InitSeq(seq);

			this->started     = true;
			this->maxSeq      = seq - 1;
			this->maxPacketTs = packet->GetTimestamp();
			this->maxPacketMs = DepLibUV::GetTimeMs();
		}

		// If not a valid packet ignore it.
		if (!UpdateSeq(packet))
		{
			MS_WARN_TAG(
			  rtp,
			  "invalid packet [ssrc:%" PRIu32 ", seq:%" PRIu16 "]",
			  packet->GetSsrc(),
			  packet->GetSequenceNumber());

			return false;
		}

		// Update highest seen RTP timestamp.
		if (RTC::SeqManager<uint32_t>::IsSeqHigherThan(packet->GetTimestamp(), this->maxPacketTs))
		{
			this->maxPacketTs = packet->GetTimestamp();
			this->maxPacketMs = DepLibUV::GetTimeMs();
		}

		return true;
	}

	void RtpStream::ResetScore(uint8_t score, bool notify)
	{
		MS_TRACE();

		this->scores.clear();

		if (this->score != score)
		{
			auto previousScore = this->score;

			this->score = score;

			// If previous score was 0 (and new one is not 0) then update activeSinceMs.
			if (previousScore == 0u)
			{
				this->activeSinceMs = DepLibUV::GetTimeMs();
			}

			// Notify the listener.
			if (notify)
			{
				this->listener->OnRtpStreamScore(this, score, previousScore);
			}
		}
	}

	bool RtpStream::UpdateSeq(RTC::RtpPacket* packet)
	{
		MS_TRACE();

		const uint16_t seq    = packet->GetSequenceNumber();
		const uint16_t udelta = seq - this->maxSeq;

		// If the new packet sequence number is greater than the max seen but not
		// "so much bigger", accept it.
		// NOTE: udelta also handles the case of a new cycle, this is:
		//    maxSeq:65536, seq:0 => udelta:1
		if (udelta < MaxDropout)
		{
			// In order, with permissible gap.
			if (seq < this->maxSeq)
			{
				// Sequence number wrapped: count another 64K cycle.
				this->cycles += RtpSeqMod;
			}

			this->maxSeq = seq;
		}
		// Too old packet received (older than the allowed misorder).
		// Or too new packet (more than acceptable dropout).
		else if (udelta <= RtpSeqMod - MaxMisorder)
		{
			// The sequence number made a very large jump. If two sequential packets
			// arrive, accept the latter.
			if (seq == this->badSeq)
			{
				// Two sequential packets. Assume that the other side restarted without
				// telling us so just re-sync (i.e., pretend this was the first packet).
				MS_WARN_TAG(
				  rtp,
				  "too bad sequence number, re-syncing RTP [ssrc:%" PRIu32 ", seq:%" PRIu16 "]",
				  packet->GetSsrc(),
				  packet->GetSequenceNumber());

				InitSeq(seq);

				this->maxPacketTs = packet->GetTimestamp();
				this->maxPacketMs = DepLibUV::GetTimeMs();

				// Notify the subclass about it.
				UserOnSequenceNumberReset();
			}
			else
			{
				MS_WARN_TAG(
				  rtp,
				  "bad sequence number, ignoring packet [ssrc:%" PRIu32 ", seq:%" PRIu16 "]",
				  packet->GetSsrc(),
				  packet->GetSequenceNumber());

				this->badSeq = (seq + 1) & (RtpSeqMod - 1);

				// Packet discarded due to late or early arriving.
				this->packetsDiscarded++;

				return false;
			}
		}
		// Acceptable misorder.
		else
		{
			// Do nothing.
		}

		return true;
	}

	void RtpStream::UpdateScore(uint8_t score)
	{
		MS_TRACE();

		// Add the score into the histogram.
		if (this->scores.size() == ScoreHistogramLength)
		{
			this->scores.erase(this->scores.begin());
		}

		auto previousScore = this->score;

		// Compute new effective score taking into accout entries in the histogram.
		this->scores.push_back(score);

		/*
		 * Scoring mechanism is a weighted average.
		 *
		 * The more recent the score is, the more weight it has.
		 * The oldest score has a weight of 1 and subsequent scores weight is
		 * increased by one sequentially.
		 *
		 * Ie:
		 * - scores: [1,2,3,4]
		 * - this->scores = ((1) + (2+2) + (3+3+3) + (4+4+4+4)) / 10 = 2.8 => 3
		 */

		size_t weight{ 0 };
		size_t samples{ 0 };
		size_t totalScore{ 0 };

		for (auto score : this->scores)
		{
			weight++;
			samples += weight;
			totalScore += weight * score;
		}

		// clang-tidy "thinks" that this can lead to division by zero but we are
		// smarter.
		// NOLINTNEXTLINE(clang-analyzer-core.DivideZero)
		this->score = static_cast<uint8_t>(std::round(static_cast<double>(totalScore) / samples));

		// Call the listener if the global score has changed.
		if (this->score != previousScore)
		{
			MS_DEBUG_TAG(
			  score,
			  "[added score:%" PRIu8 ", previous computed score:%" PRIu8 ", new computed score:%" PRIu8
			  "] (calling listener)",
			  score,
			  previousScore,
			  this->score);

			// If previous score was 0 (and new one is not 0) then update activeSinceMs.
			if (previousScore == 0u)
			{
				this->activeSinceMs = DepLibUV::GetTimeMs();
			}

			this->listener->OnRtpStreamScore(this, this->score, previousScore);
		}
		else
		{
#if MS_LOG_DEV_LEVEL == 3
			MS_DEBUG_TAG(
			  score,
			  "[added score:%" PRIu8 ", previous computed score:%" PRIu8 ", new computed score:%" PRIu8
			  "] (no change)",
			  score,
			  previousScore,
			  this->score);
#endif
		}
	}

	void RtpStream::PacketRetransmitted(RTC::RtpPacket* /*packet*/)
	{
		MS_TRACE();

		this->packetsRetransmitted++;
	}

	void RtpStream::PacketRepaired(RTC::RtpPacket* /*packet*/)
	{
		MS_TRACE();

		this->packetsRepaired++;
	}

	inline void RtpStream::InitSeq(uint16_t seq)
	{
		MS_TRACE();

		// Initialize/reset RTP counters.
		this->baseSeq = seq;
		this->maxSeq  = seq;
		this->badSeq  = RtpSeqMod + 1; // So seq == badSeq is false.
	}

	flatbuffers::Offset<FBS::RtpStream::Params> RtpStream::Params::FillBuffer(
	  flatbuffers::FlatBufferBuilder& builder) const
	{
		MS_TRACE();

		return FBS::RtpStream::CreateParamsDirect(
		  builder,
		  this->encodingIdx,
		  this->ssrc,
		  this->payloadType,
		  this->mimeType.ToString().c_str(),
		  this->clockRate,
		  this->rid.c_str(),
		  this->cname.c_str(),
		  this->rtxSsrc != 0 ? flatbuffers::Optional<uint32_t>(this->rtxSsrc) : flatbuffers::nullopt,
		  this->rtxSsrc != 0 ? flatbuffers::Optional<uint8_t>(this->rtxPayloadType) : flatbuffers::nullopt,
		  this->useNack,
		  this->usePli,
		  this->useFir,
		  this->useInBandFec,
		  this->useDtx,
		  this->spatialLayers,
		  this->temporalLayers);
	}
} // namespace RTC
