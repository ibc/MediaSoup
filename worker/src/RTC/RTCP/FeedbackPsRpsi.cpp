#define MS_CLASS "RTC::RTCP::FeedbackPsRpsi"
// #define MS_LOG_DEV_LEVEL 3

#include "RTC/RTCP/FeedbackPsRpsi.hpp"
#include "Logger.hpp"
#include <cstring>

namespace RTC
{
	namespace RTCP
	{
		/* Instance methods. */

		FeedbackPsRpsiItem::FeedbackPsRpsiItem(Header* header)
		{
			MS_TRACE();

			this->header = header;

			// Calculate bitString length.
			if (this->header->paddingBits % 8 != 0)
			{
				MS_WARN_TAG(rtcp, "invalid Rpsi packet with fractional padding bytes value");

				isCorrect = false;
			}

			const size_t paddingBytes = this->header->paddingBits / 8;

			if (paddingBytes > FeedbackPsRpsiItem::maxBitStringSize)
			{
				MS_WARN_TAG(rtcp, "invalid Rpsi packet with too many padding bytes");

				isCorrect = false;
			}

			this->length = FeedbackPsRpsiItem::maxBitStringSize - paddingBytes;
		}

		FeedbackPsRpsiItem::FeedbackPsRpsiItem(uint8_t payloadType, uint8_t* bitString, size_t length)
		{
			MS_TRACE();

			MS_ASSERT(payloadType <= 0x7f, "rpsi payload type exceeds the maximum value");
			MS_ASSERT(
			  length <= FeedbackPsRpsiItem::maxBitStringSize,
			  "rpsi bit string length exceeds the maximum value");

			this->raw    = new uint8_t[HeaderSize];
			this->header = reinterpret_cast<Header*>(this->raw);

			// 32 bits padding.
			const size_t padding = (-length) & 3;

			this->header->paddingBits = padding * 8;
			this->header->zero        = 0;
			std::memcpy(this->header->bitString, bitString, length);

			// Fill padding.
			for (size_t i{ 0 }; i < padding; ++i)
			{
				this->raw[HeaderSize + i - 1] = 0;
			}
		}

		size_t FeedbackPsRpsiItem::Serialize(uint8_t* buffer)
		{
			MS_TRACE();

			std::memcpy(buffer, this->header, HeaderSize);

			return HeaderSize;
		}

		void FeedbackPsRpsiItem::Dump() const
		{
			MS_TRACE();

			MS_DUMP("<FeedbackPsRpsiItem>");
			MS_DUMP("  padding bits %" PRIu8, this->header->paddingBits);
			MS_DUMP("  payload type: %" PRIu8, this->GetPayloadType());
			MS_DUMP("  length: %zu", this->GetLength());
			MS_DUMP("</FeedbackPsRpsiItem>");
		}
	} // namespace RTCP
} // namespace RTC
