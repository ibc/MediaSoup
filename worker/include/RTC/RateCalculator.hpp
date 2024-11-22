#ifndef MS_RTC_RATE_CALCULATOR_HPP
#define MS_RTC_RATE_CALCULATOR_HPP

#include "common.hpp"
#include "DepLibUV.hpp"
#include "RTC/RtpPacket.hpp"

namespace RTC
{
	// It is considered that the time source increases monotonically.
	// ie: the current timestamp can never be minor than a timestamp in the past.
	class RateCalculator
	{
	public:
		static constexpr size_t DefaultWindowSize{ 1000u };
		static constexpr float DefaultBpsScale{ 8000.0f };
		static constexpr uint16_t DefaultWindowItems{ 100u };

	public:
		explicit RateCalculator(
		  size_t windowSizeMs  = DefaultWindowSize,
		  float scale          = DefaultBpsScale,
		  uint16_t windowItems = DefaultWindowItems)
		  : windowSizeMs(windowSizeMs), scale(scale), windowItems(windowItems)
		{
			this->itemSizeMs = std::max(windowSizeMs / windowItems, static_cast<size_t>(1));
			this->buffer.resize(windowItems);
		}
		void Update(size_t size, uint64_t nowMs);
		uint32_t GetRate(uint64_t nowMs);
		size_t GetBytes() const
		{
			return this->bytes;
		}

	private:
		void RemoveOldData(uint64_t nowMs);
		void Reset()
		{
			std::memset(
			  static_cast<void*>(&this->buffer.front()), 0, sizeof(BufferItem) * this->buffer.size());

			this->newestItemStartTime = 0u;
			this->newestItemIndex     = -1;
			this->oldestItemStartTime = 0u;
			this->oldestItemIndex     = -1;
			this->totalCount          = 0u;
			this->lastRate            = 0u;
			this->lastTime            = 0u;
		}

	private:
		struct BufferItem
		{
			size_t count{ 0u };
			uint64_t time{ 0u };
		};

	private:
		// Window Size (in milliseconds).
		size_t windowSizeMs{ DefaultWindowSize };
		// Scale in which the rate is represented.
		float scale{ DefaultBpsScale };
		// Window Size (number of items).
		uint16_t windowItems{ DefaultWindowItems };
		// Item Size (in milliseconds), calculated as: windowSizeMs / windowItems.
		size_t itemSizeMs{ 0u };
		// Buffer to keep data.
		std::vector<BufferItem> buffer;
		// Time (in milliseconds) for last item in the time window.
		uint64_t newestItemStartTime{ 0u };
		// Index for the last item in the time window.
		int32_t newestItemIndex{ -1 };
		// Time (in milliseconds) for oldest item in the time window.
		uint64_t oldestItemStartTime{ 0u };
		// Index for the oldest item in the time window.
		int32_t oldestItemIndex{ -1 };
		// Total count in the time window.
		size_t totalCount{ 0u };
		// Total bytes transmitted.
		size_t bytes{ 0u };
		// Last value calculated by GetRate().
		uint32_t lastRate{ 0u };
		// Last time GetRate() was called.
		uint64_t lastTime{ 0u };
	};

	class RtpDataCounter
	{
	public:
		explicit RtpDataCounter(size_t windowSizeMs = 2500) : rate(windowSizeMs)
		{
		}

	public:
		void Update(RTC::RtpPacket* packet);
		uint32_t GetBitrate(uint64_t nowMs)
		{
			return this->rate.GetRate(nowMs);
		}
		size_t GetPacketCount() const
		{
			return this->packets;
		}
		size_t GetBytes() const
		{
			return this->rate.GetBytes();
		}

	private:
		RateCalculator rate;
		size_t packets{ 0u };
	};
} // namespace RTC

#endif
