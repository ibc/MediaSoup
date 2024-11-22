#define MS_CLASS "RTC::RtpRtxParameters"
// #define MS_LOG_DEV_LEVEL 3

#include "Logger.hpp"
#include "RTC/RtpDictionaries.hpp"

namespace RTC
{
	/* Instance methods. */

	RtpRtxParameters::RtpRtxParameters(const FBS::RtpParameters::Rtx* data)
	{
		MS_TRACE();

		this->ssrc = data->ssrc();
	}

	flatbuffers::Offset<FBS::RtpParameters::Rtx> RtpRtxParameters::FillBuffer(
	  flatbuffers::FlatBufferBuilder& builder) const
	{
		MS_TRACE();

		return FBS::RtpParameters::CreateRtx(builder, this->ssrc);
	}
} // namespace RTC
