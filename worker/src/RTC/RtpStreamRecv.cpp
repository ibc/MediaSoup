#define MS_CLASS "RTC::RtpStreamRecv"
// #define MS_LOG_DEV

#include "RTC/RtpStreamRecv.hpp"
#include "DepLibUV.hpp"
#include "Logger.hpp"

namespace RTC
{
	/* Instance methods. */

	RtpStreamRecv::RtpStreamRecv(Listener* listener, RTC::RtpStream::Params& params)
	    : RtpStream::RtpStream(params)
	    , listener(listener)
	{
		MS_TRACE();
	}

	RtpStreamRecv::~RtpStreamRecv()
	{
		MS_TRACE();
	}

	Json::Value RtpStreamRecv::toJson() const
	{
		MS_TRACE();

		static const Json::StaticString k_params("params");
		static const Json::StaticString k_received("received");
		static const Json::StaticString k_maxTimestamp("maxTimestamp");
		static const Json::StaticString k_transit("transit");
		static const Json::StaticString k_jitter("jitter");

		Json::Value json(Json::objectValue);

		json[k_params]       = this->params.toJson();
		json[k_received]     = (Json::UInt)this->received;
		json[k_maxTimestamp] = (Json::UInt)this->maxTimestamp;
		json[k_transit]      = (Json::UInt)this->transit;
		json[k_jitter]       = (Json::UInt)this->jitter;

		return json;
	}

	bool RtpStreamRecv::ReceivePacket(RTC::RtpPacket* packet)
	{
		MS_TRACE();

		// Call the parent method.
		if (!RtpStream::ReceivePacket(packet))
			return false;

		// Calculate Jitter.
		CalculateJitter(packet->GetTimestamp());

		// Set RTP header extension ids.
		if (this->params.absSendTimeId)
		{
			packet->AddExtensionMapping(
			    RtpHeaderExtensionUri::Type::ABS_SEND_TIME, this->params.absSendTimeId);
		}

		// Pass the packet to the NackGenerator.
		if (this->params.useNack)
			this->nackGenerator->ReceivePacket(packet);

		return true;
	}

	RTC::RTCP::ReceiverReport* RtpStreamRecv::GetRtcpReceiverReport()
	{
		MS_TRACE();

		RTC::RTCP::ReceiverReport* report = new RTC::RTCP::ReceiverReport();

		// Calculate Packets Expected and Lost.
		uint32_t expected = (this->cycles + this->maxSeq) - this->baseSeq + 1;
		int32_t totalLost = expected - this->received;

		report->SetTotalLost(totalLost);

		// Calculate Fraction Lost.
		uint32_t expectedInterval = expected - this->expectedPrior;

		this->expectedPrior = expected;

		uint32_t receivedInterval = this->received - this->receivedPrior;

		this->receivedPrior = this->received;

		int32_t lostInterval = expectedInterval - receivedInterval;
		uint8_t fractionLost;

		if (expectedInterval == 0 || lostInterval <= 0)
			fractionLost = 0;
		else
			fractionLost = (lostInterval << 8) / expectedInterval;

		report->SetFractionLost(fractionLost);

		// Fill the rest of the report.
		report->SetLastSeq((uint32_t)this->maxSeq + this->cycles);
		report->SetJitter(this->jitter);

		if (this->lastSrReceived)
		{
			// Get delay in milliseconds.
			uint32_t delayMs = (DepLibUV::GetTime() - this->lastSrReceived);
			// Express delay in units of 1/65536 seconds.
			uint32_t dlsr = (delayMs / 1000) << 16;

			dlsr |= (uint32_t)((delayMs % 1000) * 65536 / 1000);
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

	void RtpStreamRecv::ReceiveRtcpSenderReport(RTC::RTCP::SenderReport* report)
	{
		MS_TRACE();

		this->lastSrReceived  = DepLibUV::GetTime();
		this->lastSrTimestamp = report->GetNtpSec() << 16;
		this->lastSrTimestamp += report->GetNtpFrac() >> 16;
	}

	void RtpStreamRecv::RequestFullFrame()
	{
		MS_TRACE();

		if (this->params.usePli)
		{
			// Reset NackGenerator.
			if (this->params.useNack)
				this->nackGenerator.reset(new RTC::NackGenerator(this));

			this->listener->onPliRequired(this);
		}
	}

	void RtpStreamRecv::CalculateJitter(uint32_t rtpTimestamp)
	{
		MS_TRACE();

		if (!this->params.clockRate)
			return;

		int transit = DepLibUV::GetTime() - (rtpTimestamp * 1000 / this->params.clockRate);
		int d       = transit - this->transit;

		this->transit = transit;
		if (d < 0)
			d = -d;
		this->jitter += (1. / 16.) * ((double)d - this->jitter);
	}

	void RtpStreamRecv::onInitSeq()
	{
		MS_TRACE();

		// Reset NackGenerator.
		if (this->params.useNack)
			this->nackGenerator.reset(new RTC::NackGenerator(this));

		// Request a full frame so dropped video packets don't cause lag.
		if (this->params.mime.type == RTC::RtpCodecMime::Type::VIDEO)
		{
			MS_DEBUG_TAG(rtx, "stream initialized, triggering PLI [ssrc:%" PRIu32 "]",
				this->params.ssrc);

			this->listener->onPliRequired(this);
		}
	}

	void RtpStreamRecv::onNackRequired(const std::vector<uint16_t>& seqNumbers)
	{
		MS_TRACE();

		MS_ASSERT(this->params.useNack, "NACK required but not supported");

		MS_WARN_TAG(
		    rtx,
		    "triggering NACK [ssrc:%" PRIu32 ", first seq:%" PRIu16 ", num packets:%zu]",
		    this->params.ssrc,
		    seqNumbers[0],
		    seqNumbers.size());

		this->listener->onNackRequired(this, seqNumbers);
	}

	void RtpStreamRecv::onFullFrameRequired()
	{
		MS_TRACE();

		if (!this->params.usePli)
		{
			MS_WARN_TAG(rtx, "PLI required but not supported by the endpoint");

			return;
		}

		MS_DEBUG_TAG(rtx, "triggering PLI [ssrc:%" PRIu32 "]", this->params.ssrc);

		this->listener->onPliRequired(this);
	}
}
