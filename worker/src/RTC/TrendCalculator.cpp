#define MS_CLASS "RTC::TrendCalculator"
// #define MS_LOG_DEV_LEVEL 3

#include "RTC/TrendCalculator.hpp"
#include "Logger.hpp"

namespace RTC
{
	TrendCalculator::TrendCalculator(float decreaseFactor) : decreaseFactor(decreaseFactor)
	{
		MS_TRACE();
	}

	void TrendCalculator::Update(uint32_t value, uint64_t nowMs)
	{
		MS_TRACE();

		if (this->value == 0u)
		{
			this->value                   = value;
			this->highestValue            = value;
			this->highestValueUpdatedAtMs = nowMs;

			return;
		}

		// If new value is bigger or equal than current one, use it.
		if (value >= this->value)
		{
			this->value                   = value;
			this->highestValue            = value;
			this->highestValueUpdatedAtMs = nowMs;
		}
		// Otherwise decrease current value.
		else
		{
			const uint64_t elapsedMs = nowMs - this->highestValueUpdatedAtMs;
			auto subtraction =
			  static_cast<uint32_t>(this->highestValue * this->decreaseFactor * (elapsedMs / 1000.0));

			this->value = std::max<uint32_t>(
			  value, this->highestValue > subtraction ? (this->highestValue - subtraction) : value);
		}
	}

	void TrendCalculator::ForceUpdate(uint32_t value, uint64_t nowMs)
	{
		MS_TRACE();

		this->value                   = value;
		this->highestValue            = value;
		this->highestValueUpdatedAtMs = nowMs;
	}
} // namespace RTC
