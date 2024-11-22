#ifndef MS_RTC_RTCP_FEEDBACK_ITEM_HPP
#define MS_RTC_RTCP_FEEDBACK_ITEM_HPP

#include "common.hpp"

namespace RTC
{
	namespace RTCP
	{
		class FeedbackItem
		{
		public:
			template<typename Item>
			static Item* Parse(const uint8_t* data, size_t len)
			{
				// data size must be >= header.
				if (Item::HeaderSize > len)
				{
					return nullptr;
				}

				auto* header =
				  const_cast<typename Item::Header*>(reinterpret_cast<const typename Item::Header*>(data));

				return new Item(header);
			}

		public:
			bool IsCorrect() const
			{
				return this->isCorrect;
			}

		protected:
			virtual ~FeedbackItem()
			{
				delete[] this->raw;
			}

		public:
			virtual void Dump() const = 0;
			virtual void Serialize()
			{
				delete[] this->raw;

				this->raw = new uint8_t[this->GetSize()];
				this->Serialize(this->raw);
			}
			virtual size_t Serialize(uint8_t* buffer) = 0;
			virtual size_t GetSize() const            = 0;

		protected:
			uint8_t* raw{ nullptr };
			bool isCorrect{ true };
		};
	} // namespace RTCP
} // namespace RTC

#endif
