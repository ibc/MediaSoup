#define MS_CLASS "RTC::RTCP::FeedbackPsPsLei"
// #define MS_LOG_DEV_LEVEL 3

#include "RTC/RTCP/FeedbackPsLei.hpp"
#include "Logger.hpp"
#include <cstring>

namespace RTC
{
	namespace RTCP
	{
		/* Instance methods. */
		FeedbackPsLeiItem::FeedbackPsLeiItem(uint32_t ssrc)
		{
			MS_TRACE();

			this->raw          = new uint8_t[HeaderSize];
			this->header       = reinterpret_cast<Header*>(this->raw);
			this->header->ssrc = uint32_t{ htonl(ssrc) };
		}

		size_t FeedbackPsLeiItem::Serialize(uint8_t* buffer)
		{
			MS_TRACE();

			// Add minimum header.
			std::memcpy(buffer, this->header, HeaderSize);

			return HeaderSize;
		}

		void FeedbackPsLeiItem::Dump() const
		{
			MS_TRACE();

			MS_DUMP("<FeedbackPsLeiItem>");
			MS_DUMP("  ssrc: %" PRIu32, this->GetSsrc());
			MS_DUMP("</FeedbackPsLeiItem>");
		}
	} // namespace RTCP
} // namespace RTC
