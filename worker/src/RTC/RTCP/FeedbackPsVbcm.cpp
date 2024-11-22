#define MS_CLASS "RTC::RTCP::FeedbackPsVbcm"
// #define MS_LOG_DEV_LEVEL 3

#include "RTC/RTCP/FeedbackPsVbcm.hpp"
#include "Logger.hpp"
#include <cstring> // std::memcpy

namespace RTC
{
	namespace RTCP
	{
		/* Instance methods. */
		FeedbackPsVbcmItem::FeedbackPsVbcmItem(
		  uint32_t ssrc, uint8_t sequenceNumber, uint8_t payloadType, uint16_t length, uint8_t* value)
		{
			this->raw    = new uint8_t[8 + length];
			this->header = reinterpret_cast<Header*>(this->raw);

			this->header->ssrc           = uint32_t{ htonl(ssrc) };
			this->header->sequenceNumber = sequenceNumber;
			this->header->zero           = 0;
			this->header->payloadType    = payloadType;
			this->header->length         = uint16_t{ htons(length) };
			std::memcpy(this->header->value, value, sizeof(length));
		}

		size_t FeedbackPsVbcmItem::Serialize(uint8_t* buffer)
		{
			MS_TRACE();

			// Add minimum header.
			std::memcpy(buffer, this->header, 8);

			// Copy the content.
			std::memcpy(buffer + 8, this->header->value, GetLength());

			const size_t offset = 8 + GetLength();
			// 32 bits padding.
			const size_t padding = (-offset) & 3;

			for (size_t i{ 0 }; i < padding; ++i)
			{
				buffer[offset + i] = 0;
			}

			return offset + padding;
		}

		void FeedbackPsVbcmItem::Dump() const
		{
			MS_TRACE();

			MS_DUMP("<FeedbackPsVbcmItem>");
			MS_DUMP("  ssrc: %" PRIu32, this->GetSsrc());
			MS_DUMP("  sequence number: %" PRIu8, this->GetSequenceNumber());
			MS_DUMP("  payload type: %" PRIu8, this->GetPayloadType());
			MS_DUMP("  length: %" PRIu16, this->GetLength());
			MS_DUMP("</FeedbackPsVbcmItem>");
		}
	} // namespace RTCP
} // namespace RTC
