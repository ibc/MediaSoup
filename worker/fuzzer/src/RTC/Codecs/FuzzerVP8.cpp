#include "RTC/Codecs/FuzzerVP8.hpp"
#include "RTC/Codecs/VP8.hpp"

void Fuzzer::RTC::Codecs::VP8::Fuzz(const uint8_t* data, size_t len)
{
	::RTC::Codecs::VP8::PayloadDescriptor* descriptor = ::RTC::Codecs::VP8::Parse(data, len);

	if (!descriptor)
	{
		return;
	}

	delete descriptor;
}
