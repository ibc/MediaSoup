#include "RTC/FuzzerSeqManager.hpp"
#include "Utils.hpp"
#include "RTC/SeqManager.hpp"
#include <iostream>

void Fuzzer::RTC::SeqManager::Fuzz(const uint8_t* data, size_t len)
{
	if (len < 10)
	{
		return;
	}

	::RTC::SeqManager<uint16_t> seqManager;

	uint16_t output;

	for (size_t count = 0; count < 7; count++)
	{
		seqManager.Input(Utils::Byte::Get2Bytes(data, count), output);
		seqManager.Drop(Utils::Byte::Get2Bytes(data, count + 2));
	}
}
