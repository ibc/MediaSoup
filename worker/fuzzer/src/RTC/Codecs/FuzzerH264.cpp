#include "RTC/Codecs/FuzzerH264.hpp"
#include "RTC/Codecs/H264.hpp"

void Fuzzer::RTC::Codecs::H264::Fuzz(const uint8_t* data, size_t len)
{
	::RTC::Codecs::H264::PayloadDescriptor* descriptor = ::RTC::Codecs::H264::Parse(data, len);

	if (!descriptor)
	{
		return;
	}

	delete descriptor;
}
