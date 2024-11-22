#define MS_CLASS "RTC::RtpEncodingParameters"
// #define MS_LOG_DEV_LEVEL 3

#include "Logger.hpp"
#include "MediaSoupErrors.hpp"
#include "RTC/RtpDictionaries.hpp"
#include <exception>
#include <regex>

namespace RTC
{
	/* Instance methods. */

	RtpEncodingParameters::RtpEncodingParameters(const FBS::RtpParameters::RtpEncodingParameters* data)
	{
		MS_TRACE();

		// ssrc is optional.
		if (data->ssrc().has_value())
		{
			this->ssrc = data->ssrc().value();
		}

		// rid is optional.
		if (flatbuffers::IsFieldPresent(data, FBS::RtpParameters::RtpEncodingParameters::VT_RID))
		{
			this->rid = data->rid()->str();
		}

		// codecPayloadType is optional.
		if (data->codecPayloadType().has_value())
		{
			this->codecPayloadType    = data->codecPayloadType().value();
			this->hasCodecPayloadType = true;
		}

		// rtx is optional.
		if (flatbuffers::IsFieldPresent(data, FBS::RtpParameters::RtpEncodingParameters::VT_RTX))
		{
			this->rtx    = RtpRtxParameters(data->rtx());
			this->hasRtx = true;
		}

		// maxBitrate is optional.
		if (data->maxBitrate().has_value())
		{
			this->maxBitrate = data->maxBitrate().value();
		}

		// dtx is optional, default is false.
		this->dtx = data->dtx();

		// scalabilityMode is optional.
		if (flatbuffers::IsFieldPresent(
		      data, FBS::RtpParameters::RtpEncodingParameters::VT_SCALABILITYMODE))
		{
			const std::string scalabilityMode = data->scalabilityMode()->str();

			static const std::regex ScalabilityModeRegex(
			  "^[LS]([1-9]\\d{0,1})T([1-9]\\d{0,1})(_KEY)?.*", std::regex_constants::ECMAScript);

			std::smatch match;

			std::regex_match(scalabilityMode, match, ScalabilityModeRegex);

			if (!match.empty())
			{
				this->scalabilityMode = scalabilityMode;

				try
				{
					this->spatialLayers  = std::stoul(match[1].str());
					this->temporalLayers = std::stoul(match[2].str());
					this->ksvc           = match.size() >= 4 && match[3].str() == "_KEY";
				}
				catch (std::exception& e)
				{
					MS_THROW_TYPE_ERROR("invalid scalabilityMode: %s", e.what());
				}
			}
		}
	}

	flatbuffers::Offset<FBS::RtpParameters::RtpEncodingParameters> RtpEncodingParameters::FillBuffer(
	  flatbuffers::FlatBufferBuilder& builder) const
	{
		MS_TRACE();

		return FBS::RtpParameters::CreateRtpEncodingParametersDirect(
		  builder,
		  this->ssrc != 0u ? flatbuffers::Optional<uint32_t>(this->ssrc) : flatbuffers::nullopt,
		  this->rid.size() > 0 ? this->rid.c_str() : nullptr,
		  this->hasCodecPayloadType ? flatbuffers::Optional<uint8_t>(this->codecPayloadType)
		                            : flatbuffers::nullopt,
		  this->hasRtx ? this->rtx.FillBuffer(builder) : 0u,
		  this->dtx,
		  this->scalabilityMode.c_str());
	}
} // namespace RTC
