#ifndef MS_FUZZER_RTC_RTP_RETRANSMISSION_BUFFER_HPP
#define MS_FUZZER_RTC_RTP_RETRANSMISSION_BUFFER_HPP

#include "common.hpp"

namespace Fuzzer
{
	namespace RTC
	{
		namespace RtpRetransmissionBuffer
		{
			void Fuzz(const uint8_t* data, size_t len);
		}
	} // namespace RTC
} // namespace Fuzzer

#endif
