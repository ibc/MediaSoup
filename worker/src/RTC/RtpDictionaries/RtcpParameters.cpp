#define MS_CLASS "RTC::RtcpParameters"
// #define MS_LOG_DEV_LEVEL 3

#include "Logger.hpp"
#include "RTC/RtpDictionaries.hpp"

namespace RTC
{
	/* Instance methods. */

	RtcpParameters::RtcpParameters(const FBS::RtpParameters::RtcpParameters* data)
	{
		MS_TRACE();

		// cname is optional.
		if (flatbuffers::IsFieldPresent(data, FBS::RtpParameters::RtcpParameters::VT_CNAME))
		{
			this->cname = data->cname()->str();
		}

		// reducedSize is optional, default value is true.
		this->reducedSize = data->reducedSize();
	}

	flatbuffers::Offset<FBS::RtpParameters::RtcpParameters> RtcpParameters::FillBuffer(
	  flatbuffers::FlatBufferBuilder& builder) const
	{
		MS_TRACE();

		return FBS::RtpParameters::CreateRtcpParametersDirect(
		  builder, this->cname.c_str(), this->reducedSize);
	}
} // namespace RTC
