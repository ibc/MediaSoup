#define MS_CLASS "RTC::RtxStream"
// #define MS_LOG_DEV_LEVEL 3

#include "RTC/RtxStream.hpp"
#include "Logger.hpp"
#include "RTC/SeqManager.hpp"

namespace RTC
{
	/* Static. */

	static constexpr uint16_t MaxDropout{ 3000 };
	static constexpr uint16_t MaxMisorder{ 1500 };
	static constexpr uint32_t RtpSeqMod{ 1 << 16 };

	/* Instance methods. */

	RtxStream::RtxStream(RTC::RtxStream::Params& params) : params(params)
	{
		MS_TRACE();

		MS_ASSERT(
		  params.mimeType.subtype == RTC::RtpCodecMimeType::Subtype::RTX, "mimeType.subtype is not RTX");
	}

	RtxStream::~RtxStream()
	{
		MS_TRACE();
	}

	flatbuffers::Offset<FBS::RtxStream::RtxDump> RtxStream::FillBuffer(
	  flatbuffers::FlatBufferBuilder& builder) const
	{
		MS_TRACE();

		// Add params.
		auto params = this->params.FillBuffer(builder);

		return FBS::RtxStream::CreateRtxDump(builder, params);
	}

	bool RtxStream::ReceivePacket(RTC::RtpPacket* packet)
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
			  rtx,
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

		// Increase packet count.
		this->packetsCount++;

		return true;
	}

	RTC::RTCP::ReceiverReport* RtxStream::GetRtcpReceiverReport()
	{
		MS_TRACE();

		auto* report = new RTC::RTCP::ReceiverReport();

		report->SetSsrc(GetSsrc());

		const uint32_t prevPacketsLost = this->packetsLost;

		// Calculate Packets Expected and Lost.
		auto expected = GetExpectedPackets();

		if (expected > this->packetsCount)
		{
			this->packetsLost = expected - this->packetsCount;
		}
		else
		{
			this->packetsLost = 0u;
		}

		// Calculate Fraction Lost.
		const uint32_t expectedInterval = expected - this->expectedPrior;

		this->expectedPrior = expected;

		const uint32_t receivedInterval = this->packetsCount - this->receivedPrior;

		this->receivedPrior = this->packetsCount;

		const int32_t lostInterval = expectedInterval - receivedInterval;

		if (expectedInterval == 0 || lostInterval <= 0)
		{
			this->fractionLost = 0;
		}
		else
		{
			this->fractionLost = std::round((static_cast<double>(lostInterval << 8) / expectedInterval));
		}

		this->reportedPacketLost += (this->packetsLost - prevPacketsLost);

		report->SetTotalLost(this->reportedPacketLost);
		report->SetFractionLost(this->fractionLost);

		// Fill the rest of the report.
		report->SetLastSeq(static_cast<uint32_t>(this->maxSeq) + this->cycles);

		// NOTE: Do not calculate any jitter.
		report->SetJitter(0);

		if (this->lastSrReceived != 0)
		{
			// Get delay in milliseconds.
			const uint32_t delayMs = DepLibUV::GetTimeMs() - this->lastSrReceived;
			// Express delay in units of 1/65536 seconds.
			uint32_t dlsr = (delayMs / 1000) << 16;

			dlsr |= uint32_t{ (delayMs % 1000) * 65536 / 1000 };

			report->SetDelaySinceLastSenderReport(dlsr);
			report->SetLastSenderReport(this->lastSrTimestamp);
		}
		else
		{
			report->SetDelaySinceLastSenderReport(0);
			report->SetLastSenderReport(0);
		}

		return report;
	}

	void RtxStream::ReceiveRtcpSenderReport(RTC::RTCP::SenderReport* report)
	{
		MS_TRACE();

		this->lastSrReceived  = DepLibUV::GetTimeMs();
		this->lastSrTimestamp = report->GetNtpSec() << 16;
		this->lastSrTimestamp += report->GetNtpFrac() >> 16;
	}

	bool RtxStream::UpdateSeq(RTC::RtpPacket* packet)
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
		// Or to new packet (more than acceptable dropout).
		else if (udelta <= RtpSeqMod - MaxMisorder)
		{
			// The sequence number made a very large jump. If two sequential packets
			// arrive, accept the latter.
			if (seq == this->badSeq)
			{
				// Two sequential packets. Assume that the other side restarted without
				// telling us so just re-sync (i.e., pretend this was the first packet).
				MS_WARN_TAG(
				  rtx,
				  "too bad sequence number, re-syncing RTP [ssrc:%" PRIu32 ", seq:%" PRIu16 "]",
				  packet->GetSsrc(),
				  packet->GetSequenceNumber());

				InitSeq(seq);

				this->maxPacketTs = packet->GetTimestamp();
				this->maxPacketMs = DepLibUV::GetTimeMs();
			}
			else
			{
				MS_WARN_TAG(
				  rtx,
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

	inline void RtxStream::InitSeq(uint16_t seq)
	{
		MS_TRACE();

		// Initialize/reset RTP counters.
		this->baseSeq = seq;
		this->maxSeq  = seq;
		this->badSeq  = RtpSeqMod + 1; // So seq == badSeq is false.
	}

	flatbuffers::Offset<FBS::RtxStream::Params> RtxStream::Params::FillBuffer(
	  flatbuffers::FlatBufferBuilder& builder) const
	{
		MS_TRACE();

		return FBS::RtxStream::CreateParamsDirect(
		  builder,
		  this->ssrc,
		  this->payloadType,
		  this->mimeType.ToString().c_str(),
		  this->clockRate,
		  this->rrid.c_str(),
		  this->cname.c_str());
	}
} // namespace RTC
