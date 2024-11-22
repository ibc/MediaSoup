#define MS_CLASS "RTC::RtpCodecParameters"
// #define MS_LOG_DEV_LEVEL 3

#include "Logger.hpp"
#include "MediaSoupErrors.hpp"
#include "RTC/RtpDictionaries.hpp"

namespace RTC
{
	/* Instance methods. */

	RtpCodecParameters::RtpCodecParameters(const FBS::RtpParameters::RtpCodecParameters* data)
	{
		MS_TRACE();

		// Set MIME field.
		// This may throw.
		this->mimeType.SetMimeType(data->mimeType()->str());

		// payloadType.
		this->payloadType = data->payloadType();

		// clockRate.
		this->clockRate = data->clockRate();

		// channels is optional.
		if (data->channels().has_value())
		{
			this->channels = data->channels().value();
		}

		// parameters is optional.
		if (flatbuffers::IsFieldPresent(data, FBS::RtpParameters::RtpCodecParameters::VT_PARAMETERS))
		{
			this->parameters.Set(data->parameters());
		}

		// rtcpFeedback is optional.
		if (flatbuffers::IsFieldPresent(data, FBS::RtpParameters::RtpCodecParameters::VT_RTCPFEEDBACK))
		{
			this->rtcpFeedback.reserve(data->rtcpFeedback()->size());

			for (const auto* entry : *data->rtcpFeedback())
			{
				this->rtcpFeedback.emplace_back(entry);
			}
		}

		// Check codec.
		CheckCodec();
	}

	flatbuffers::Offset<FBS::RtpParameters::RtpCodecParameters> RtpCodecParameters::FillBuffer(
	  flatbuffers::FlatBufferBuilder& builder) const
	{
		MS_TRACE();

		auto parameters = this->parameters.FillBuffer(builder);

		std::vector<flatbuffers::Offset<FBS::RtpParameters::RtcpFeedback>> rtcpFeedback;
		rtcpFeedback.reserve(this->rtcpFeedback.size());

		for (const auto& fb : this->rtcpFeedback)
		{
			rtcpFeedback.emplace_back(fb.FillBuffer(builder));
		}

		return FBS::RtpParameters::CreateRtpCodecParametersDirect(
		  builder,
		  this->mimeType.ToString().c_str(),
		  this->payloadType,
		  this->clockRate,
		  this->channels > 1 ? flatbuffers::Optional<uint8_t>(this->channels) : flatbuffers::nullopt,
		  &parameters,
		  &rtcpFeedback);
	}

	inline void RtpCodecParameters::CheckCodec()
	{
		MS_TRACE();

		static std::string aptString{ "apt" };

		// Check per MIME parameters and set default values.
		switch (this->mimeType.subtype)
		{
			case RTC::RtpCodecMimeType::Subtype::RTX:
			{
				// A RTX codec must have 'apt' parameter.
				if (!this->parameters.HasPositiveInteger(aptString))
				{
					MS_THROW_TYPE_ERROR("missing apt parameter in RTX codec");
				}

				break;
			}

			default:;
		}
	}
} // namespace RTC
