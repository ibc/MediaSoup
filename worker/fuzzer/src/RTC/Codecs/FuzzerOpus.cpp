#include "RTC/Codecs/FuzzerOpus.hpp"
#include "RTC/Codecs/Opus.hpp"

void Fuzzer::RTC::Codecs::Opus::Fuzz(const uint8_t* data, size_t len)
{
	::RTC::Codecs::Opus::PayloadDescriptor* descriptor = ::RTC::Codecs::Opus::Parse(data, len);

	if (!descriptor)
	{
		return;
	}

	delete descriptor;
}
