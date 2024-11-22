#ifndef MS_RTC_RTCP_FEEDBACK_PS_RPSI_HPP
#define MS_RTC_RTCP_FEEDBACK_PS_RPSI_HPP

#include "common.hpp"
#include "RTC/RTCP/FeedbackPs.hpp"

/* RFC 4585
 * Reference Picture Selection Indication (RPSI)
 *

   0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
  |      PB       |0| Payload Type|    Native RPSI bit string     |
  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
  |   defined per codec          ...                | Padding (0) |
  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 */

namespace RTC
{
	namespace RTCP
	{
		class FeedbackPsRpsiItem : public FeedbackItem
		{
			const static size_t maxBitStringSize{ 6 };

		public:
			struct Header
			{
				uint8_t paddingBits;
#if defined(MS_LITTLE_ENDIAN)
				uint8_t payloadType : 7;
				uint8_t zero : 1;
#elif defined(MS_BIG_ENDIAN)
				uint8_t zero : 1;
				uint8_t payloadType : 7;
#endif
				uint8_t bitString[maxBitStringSize];
			};

		public:
			static const size_t HeaderSize = 8;
			static const FeedbackPs::MessageType messageType{ FeedbackPs::MessageType::RPSI };

		public:
			explicit FeedbackPsRpsiItem(Header* header);
			explicit FeedbackPsRpsiItem(FeedbackPsRpsiItem* item) : header(item->header)
			{
			}
			FeedbackPsRpsiItem(uint8_t payloadType, uint8_t* bitString, size_t length);
			~FeedbackPsRpsiItem() override = default;

			bool IsCorrect() const
			{
				return this->isCorrect;
			}
			uint8_t GetPayloadType() const
			{
				return this->header->payloadType;
			}
			uint8_t* GetBitString() const
			{
				return this->header->bitString;
			}
			size_t GetLength() const
			{
				return this->length;
			}

			/* Virtual methods inherited from FeedbackItem. */
		public:
			void Dump() const override;
			size_t Serialize(uint8_t* buffer) override;
			size_t GetSize() const override
			{
				return FeedbackPsRpsiItem::HeaderSize;
			}

		private:
			Header* header{ nullptr };
			size_t length{ 0 };
		};

		// Rpsi packet declaration.
		using FeedbackPsRpsiPacket = FeedbackPsItemsPacket<FeedbackPsRpsiItem>;
	} // namespace RTCP
} // namespace RTC

#endif
