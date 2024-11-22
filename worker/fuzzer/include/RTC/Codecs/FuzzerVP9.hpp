#ifndef MS_FUZZER_RTC_CODECS_VP9_HPP
#define MS_FUZZER_RTC_CODECS_VP9_HPP

#include "common.hpp"

namespace Fuzzer
{
	namespace RTC
	{
		namespace Codecs
		{
			namespace VP9
			{
				void Fuzz(const uint8_t* data, size_t len);
			}
		} // namespace Codecs
	}   // namespace RTC
} // namespace Fuzzer

#endif
