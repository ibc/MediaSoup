#include "RTC/FuzzerRateCalculator.hpp"
#include "DepLibUV.hpp"
#include "Utils.hpp"
#include "RTC/RateCalculator.hpp"
#include "RTC/RtpPacket.hpp" // RTC::MtuSize

static ::RTC::RateCalculator rateCalculator;
static uint64_t nowMs;

// This Init() function must be declared static, otherwise linking will fail if
// another source file defines same non static Init() function.
static int Init();

void Fuzzer::RTC::RateCalculator::Fuzz(const uint8_t* data, size_t len)
{
	// Trick to initialize our stuff just once.
	static int unused = Init();

	// Avoid [-Wunused-variable].
	unused++;

	// We need at least 2 bytes of |data|.
	if (len < 2)
	{
		return;
	}

	auto size =
	  static_cast<size_t>(Utils::Crypto::GetRandomUInt(0u, static_cast<uint32_t>(::RTC::MtuSize)));

	nowMs += Utils::Crypto::GetRandomUInt(0u, 1000u);

	rateCalculator.Update(size, nowMs);

	// Only get rate from time to time.
	if (Utils::Byte::Get2Bytes(data, 0) % 100 == 0)
	{
		rateCalculator.GetRate(nowMs);
	}
}

int Init()
{
	nowMs = DepLibUV::GetTimeMs();

	return 0;
}
