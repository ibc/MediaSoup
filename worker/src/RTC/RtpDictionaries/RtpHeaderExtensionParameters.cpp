#define MS_CLASS "RTC::RtpHeaderExtensionParameters"
// #define MS_LOG_DEV_LEVEL 3

#include "Logger.hpp"
#include "MediaSoupErrors.hpp"
#include "RTC/RtpDictionaries.hpp"

namespace RTC
{
	/* Instance methods. */

	RtpHeaderExtensionParameters::RtpHeaderExtensionParameters(
	  const FBS::RtpParameters::RtpHeaderExtensionParameters* const data)
	{
		MS_TRACE();

		// Get the type.
		this->type = RTC::RtpHeaderExtensionUri::TypeFromFbs(data->uri());

		this->id = data->id();

		// Don't allow id 0.
		if (this->id == 0u)
		{
			MS_THROW_TYPE_ERROR("invalid id 0");
		}

		// encrypt is false by default.
		this->encrypt = data->encrypt();

		// parameters is optional.
		if (flatbuffers::IsFieldPresent(
		      data, FBS::RtpParameters::RtpHeaderExtensionParameters::VT_PARAMETERS))
		{
			this->parameters.Set(data->parameters());
		}
	}

	flatbuffers::Offset<FBS::RtpParameters::RtpHeaderExtensionParameters> RtpHeaderExtensionParameters::FillBuffer(
	  flatbuffers::FlatBufferBuilder& builder) const
	{
		MS_TRACE();

		auto parameters = this->parameters.FillBuffer(builder);

		return FBS::RtpParameters::CreateRtpHeaderExtensionParametersDirect(
		  builder, RTC::RtpHeaderExtensionUri::TypeToFbs(this->type), this->id, this->encrypt, &parameters);
	}
} // namespace RTC
