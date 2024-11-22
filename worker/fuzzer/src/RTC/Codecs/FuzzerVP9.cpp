#include "RTC/Codecs/FuzzerVP9.hpp"
#include "RTC/Codecs/VP9.hpp"

void Fuzzer::RTC::Codecs::VP9::Fuzz(const uint8_t* data, size_t len)
{
	::RTC::Codecs::VP9::PayloadDescriptor* descriptor = ::RTC::Codecs::VP9::Parse(data, len);

	if (!descriptor)
	{
		return;
	}

	delete descriptor;
}
