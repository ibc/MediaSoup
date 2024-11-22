#include "RTC/Codecs/FuzzerH264_SVC.hpp"
#include "RTC/Codecs/H264_SVC.hpp"

void Fuzzer::RTC::Codecs::H264_SVC::Fuzz(const uint8_t* data, size_t len)
{
	::RTC::Codecs::H264_SVC::PayloadDescriptor* descriptor = ::RTC::Codecs::H264_SVC::Parse(data, len);

	if (!descriptor)
	{
		return;
	}

	delete descriptor;
}
