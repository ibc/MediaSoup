#define MS_CLASS "RTC::RTCP::FeedbackRtpEcn"
// #define MS_LOG_DEV_LEVEL 3

#include "RTC/RTCP/FeedbackRtpEcn.hpp"
#include "Logger.hpp"
#include <cstring> // std::memcpy

namespace RTC
{
	namespace RTCP
	{
		size_t FeedbackRtpEcnItem::Serialize(uint8_t* buffer)
		{
			MS_TRACE();

			// Add minimum header.
			std::memcpy(buffer, this->header, HeaderSize);

			return HeaderSize;
		}

		void FeedbackRtpEcnItem::Dump() const
		{
			MS_TRACE();

			MS_DUMP("<FeedbackRtpEcnItem>");
			MS_DUMP("  sequence number: %" PRIu32, this->GetSequenceNumber());
			MS_DUMP("  ect0 counter: %" PRIu32, this->GetEct0Counter());
			MS_DUMP("  ect1 counter: %" PRIu32, this->GetEct1Counter());
			MS_DUMP("  ecn ce counter: %" PRIu16, this->GetEcnCeCounter());
			MS_DUMP("  not ect counter: %" PRIu16, this->GetNotEctCounter());
			MS_DUMP("  lost packets: %" PRIu16, this->GetLostPackets());
			MS_DUMP("  duplicated packets: %" PRIu16, this->GetDuplicatedPackets());
			MS_DUMP("</FeedbackRtpEcnItem>");
		}
	} // namespace RTCP
} // namespace RTC
