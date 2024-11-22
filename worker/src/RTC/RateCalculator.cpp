#define MS_CLASS "RTC::RateCalculator"
// #define MS_LOG_DEV_LEVEL 3

#include "RTC/RateCalculator.hpp"
#include "Logger.hpp"
#include <cmath> // std::trunc()

namespace RTC
{
	void RateCalculator::Update(size_t size, uint64_t nowMs)
	{
		MS_TRACE();

		// Ignore too old data. Should never happen.
		if (nowMs < this->oldestItemStartTime)
		{
			return;
		}

		// Increase bytes.
		this->bytes += size;

		RemoveOldData(nowMs);

		// If the elapsed time from the newest item start time is greater than the
		// item size (in milliseconds), increase the item index.
		if (this->newestItemIndex < 0 || nowMs - this->newestItemStartTime >= this->itemSizeMs)
		{
			this->newestItemIndex++;
			this->newestItemStartTime = nowMs;

			if (this->newestItemIndex >= this->windowItems)
			{
				this->newestItemIndex = 0;
			}

			MS_ASSERT(
			  this->newestItemIndex != this->oldestItemIndex || this->oldestItemIndex == -1,
			  "newest index overlaps with the oldest one [newestItemIndex:%" PRId32
			  ", oldestItemIndex:%" PRId32 "]",
			  this->newestItemIndex,
			  this->oldestItemIndex);

			// Set the newest item.
			BufferItem& item = this->buffer[this->newestItemIndex];
			item.count       = size;
			item.time        = nowMs;
		}
		else
		{
			// Update the newest item.
			BufferItem& item = this->buffer[this->newestItemIndex];
			item.count += size;
		}

		// Set the oldest item index and time, if not set.
		if (this->oldestItemIndex < 0)
		{
			this->oldestItemIndex     = this->newestItemIndex;
			this->oldestItemStartTime = nowMs;
		}

		this->totalCount += size;

		// Reset lastRate and lastTime so GetRate() will calculate rate again even
		// if called with same now in the same loop iteration.
		this->lastRate = 0;
		this->lastTime = 0;
	}

	uint32_t RateCalculator::GetRate(uint64_t nowMs)
	{
		MS_TRACE();

		if (nowMs == this->lastTime)
		{
			return this->lastRate;
		}

		RemoveOldData(nowMs);

		const float scale = this->scale / this->windowSizeMs;

		this->lastTime = nowMs;
		this->lastRate = static_cast<uint32_t>(std::trunc(this->totalCount * scale + 0.5f));

		return this->lastRate;
	}

	inline void RateCalculator::RemoveOldData(uint64_t nowMs)
	{
		MS_TRACE();

		// No item set.
		if (this->newestItemIndex < 0 || this->oldestItemIndex < 0)
		{
			return;
		}

		const uint64_t newOldestTime = nowMs - this->windowSizeMs;

		// Oldest item already removed.
		if (newOldestTime < this->oldestItemStartTime)
		{
			return;
		}

		// A whole window size time has elapsed since last entry. Reset the buffer.
		if (newOldestTime >= this->newestItemStartTime)
		{
			Reset();

			return;
		}

		while (newOldestTime >= this->oldestItemStartTime)
		{
			BufferItem& oldestItem = this->buffer[this->oldestItemIndex];
			this->totalCount -= oldestItem.count;
			oldestItem.count = 0u;
			oldestItem.time  = 0u;

			if (++this->oldestItemIndex >= this->windowItems)
			{
				this->oldestItemIndex = 0;
			}

			const BufferItem& newOldestItem = this->buffer[this->oldestItemIndex];
			this->oldestItemStartTime       = newOldestItem.time;
		}
	}

	void RtpDataCounter::Update(RTC::RtpPacket* packet)
	{
		const uint64_t nowMs = DepLibUV::GetTimeMs();

		this->packets++;
		this->rate.Update(packet->GetSize(), nowMs);
	}
} // namespace RTC
