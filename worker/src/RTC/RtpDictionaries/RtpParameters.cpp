#define MS_CLASS "RTC::RtpParameters"
// #define MS_LOG_DEV_LEVEL 3

#include "Logger.hpp"
#include "MediaSoupErrors.hpp"
#include "RTC/Codecs/Tools.hpp"
#include "RTC/RtpDictionaries.hpp"
#include <absl/container/flat_hash_set.h>

namespace RTC
{
	/* Class variables. */

	absl::flat_hash_map<RtpParameters::Type, std::string> RtpParameters::type2String = {
		{ RtpParameters::Type::SIMPLE, "simple" },
		{ RtpParameters::Type::SIMULCAST, "simulcast" },
		{ RtpParameters::Type::SVC, "svc" },
		{ RtpParameters::Type::PIPE, "pipe" }
	};
	// clang-format on

	/* Class methods. */

	std::optional<RtpParameters::Type> RtpParameters::GetType(const RtpParameters& rtpParameters)
	{
		MS_TRACE();

		std::optional<RtpParameters::Type> type;

		if (rtpParameters.encodings.size() == 1)
		{
			const auto& encoding = rtpParameters.encodings[0];
			const auto* mediaCodec =
			  rtpParameters.GetCodecForEncoding(const_cast<RTC::RtpEncodingParameters&>(encoding));

			if (encoding.spatialLayers > 1 || encoding.temporalLayers > 1)
			{
				if (RTC::Codecs::Tools::IsValidTypeForCodec(RtpParameters::Type::SVC, mediaCodec->mimeType))
				{
					type.emplace(RtpParameters::Type::SVC);
				}
				else if (RTC::Codecs::Tools::IsValidTypeForCodec(
				           RtpParameters::Type::SIMULCAST, mediaCodec->mimeType))
				{
					type.emplace(RtpParameters::Type::SIMULCAST);
				}
			}
			else
			{
				type.emplace(RtpParameters::Type::SIMPLE);
			}
		}
		else if (rtpParameters.encodings.size() > 1)
		{
			type.emplace(RtpParameters::Type::SIMULCAST);
		}

		return type;
	}

	std::string& RtpParameters::GetTypeString(RtpParameters::Type type)
	{
		MS_TRACE();

		return RtpParameters::type2String.at(type);
	}

	FBS::RtpParameters::Type RtpParameters::TypeToFbs(RtpParameters::Type type)
	{
		MS_TRACE();

		switch (type)
		{
			case Type::SIMPLE:
			{
				return FBS::RtpParameters::Type::SIMPLE;
			}

			case Type::SIMULCAST:
			{
				return FBS::RtpParameters::Type::SIMULCAST;
			}

			case Type::SVC:
			{
				return FBS::RtpParameters::Type::SVC;
			}

			case Type::PIPE:
			{
				return FBS::RtpParameters::Type::PIPE;
			}
		}
	}

	/* Instance methods. */

	RtpParameters::RtpParameters(const FBS::RtpParameters::RtpParameters* data)
	{
		MS_TRACE();

		// mid is optional.
		if (data->mid())
		{
			this->mid = data->mid()->str();

			if (this->mid.empty())
			{
				MS_THROW_TYPE_ERROR("empty mid");
			}
		}

		this->codecs.reserve(data->codecs()->size());

		for (const auto* entry : *data->codecs())
		{
			// This may throw due the constructor of RTC::RtpCodecParameters.
			this->codecs.emplace_back(entry);
		}

		if (this->codecs.empty())
		{
			MS_THROW_TYPE_ERROR("empty codecs");
		}

		this->encodings.reserve(data->encodings()->size());

		for (const auto* entry : *data->encodings())
		{
			// This may throw due the constructor of RTC::RtpEncodingParameters.
			this->encodings.emplace_back(entry);
		}

		if (this->encodings.empty())
		{
			MS_THROW_TYPE_ERROR("empty encodings");
		}

		this->headerExtensions.reserve(data->headerExtensions()->size());

		for (const auto* entry : *data->headerExtensions())
		{
			// This may throw due the constructor of RTC::RtpHeaderExtensionParameters.
			this->headerExtensions.emplace_back(entry);
		}

		// This may throw.
		this->rtcp = RTC::RtcpParameters(data->rtcp());

		// Validate RTP parameters.
		ValidateCodecs();
		ValidateEncodings();
	}

	flatbuffers::Offset<FBS::RtpParameters::RtpParameters> RtpParameters::FillBuffer(
	  flatbuffers::FlatBufferBuilder& builder) const
	{
		MS_TRACE();

		// Add codecs.
		std::vector<flatbuffers::Offset<FBS::RtpParameters::RtpCodecParameters>> codecs;
		codecs.reserve(this->codecs.size());

		for (const auto& codec : this->codecs)
		{
			codecs.emplace_back(codec.FillBuffer(builder));
		}

		// Add encodings.
		std::vector<flatbuffers::Offset<FBS::RtpParameters::RtpEncodingParameters>> encodings;
		encodings.reserve(this->encodings.size());

		for (const auto& encoding : this->encodings)
		{
			encodings.emplace_back(encoding.FillBuffer(builder));
		}

		// Add headerExtensions.
		std::vector<flatbuffers::Offset<FBS::RtpParameters::RtpHeaderExtensionParameters>> headerExtensions;
		headerExtensions.reserve(this->headerExtensions.size());

		for (const auto& headerExtension : this->headerExtensions)
		{
			headerExtensions.emplace_back(headerExtension.FillBuffer(builder));
		}

		// Add rtcp.
		flatbuffers::Offset<FBS::RtpParameters::RtcpParameters> rtcp;

		rtcp = this->rtcp.FillBuffer(builder);

		return FBS::RtpParameters::CreateRtpParametersDirect(
		  builder, mid.c_str(), &codecs, &headerExtensions, &encodings, rtcp);
	}

	const RTC::RtpCodecParameters* RtpParameters::GetCodecForEncoding(RtpEncodingParameters& encoding) const
	{
		MS_TRACE();

		uint8_t payloadType = encoding.codecPayloadType;
		auto it             = this->codecs.begin();

		for (; it != this->codecs.end(); ++it)
		{
			const auto& codec = *it;

			if (codec.payloadType == payloadType)
			{
				return std::addressof(codec);
			}
		}

		// This should never happen.
		if (it == this->codecs.end())
		{
			MS_ABORT("no valid codec payload type for the given encoding");
		}

		return nullptr;
	}

	const RTC::RtpCodecParameters* RtpParameters::GetRtxCodecForEncoding(RtpEncodingParameters& encoding) const
	{
		MS_TRACE();

		static const std::string AptString{ "apt" };

		uint8_t payloadType = encoding.codecPayloadType;

		for (const auto& codec : this->codecs)
		{
			if (codec.mimeType.IsFeatureCodec() && codec.parameters.GetInteger(AptString) == payloadType)
			{
				return std::addressof(codec);
			}
		}

		return nullptr;
	}

	void RtpParameters::ValidateCodecs()
	{
		MS_TRACE();

		static const std::string AptString{ "apt" };

		absl::flat_hash_set<uint8_t> payloadTypes;

		for (auto& codec : this->codecs)
		{
			if (payloadTypes.find(codec.payloadType) != payloadTypes.end())
			{
				MS_THROW_TYPE_ERROR("duplicated payloadType");
			}

			payloadTypes.insert(codec.payloadType);

			switch (codec.mimeType.subtype)
			{
				// A RTX codec must have 'apt' parameter pointing to a non RTX codec.
				case RTC::RtpCodecMimeType::Subtype::RTX:
				{
					// NOTE: RtpCodecParameters already asserted that there is apt parameter.
					int32_t apt = codec.parameters.GetInteger(AptString);
					auto it     = this->codecs.begin();

					for (; it != this->codecs.end(); ++it)
					{
						auto codec = *it;

						if (static_cast<int32_t>(codec.payloadType) == apt)
						{
							if (codec.mimeType.subtype == RTC::RtpCodecMimeType::Subtype::RTX)
							{
								MS_THROW_TYPE_ERROR("apt in RTX codec points to a RTX codec");
							}
							else if (codec.mimeType.subtype == RTC::RtpCodecMimeType::Subtype::ULPFEC)
							{
								MS_THROW_TYPE_ERROR("apt in RTX codec points to a ULPFEC codec");
							}
							else if (codec.mimeType.subtype == RTC::RtpCodecMimeType::Subtype::FLEXFEC)
							{
								MS_THROW_TYPE_ERROR("apt in RTX codec points to a FLEXFEC codec");
							}
							else
							{
								break;
							}
						}
					}

					if (it == this->codecs.end())
					{
						MS_THROW_TYPE_ERROR("apt in RTX codec points to a non existing codec");
					}

					break;
				}

				default:;
			}
		}
	}

	void RtpParameters::ValidateEncodings()
	{
		uint8_t firstMediaPayloadType{ 0 };

		{
			auto it = this->codecs.begin();

			for (; it != this->codecs.end(); ++it)
			{
				auto& codec = *it;

				// Must be a media codec.
				if (codec.mimeType.IsMediaCodec())
				{
					firstMediaPayloadType = codec.payloadType;

					break;
				}
			}

			if (it == this->codecs.end())
			{
				MS_THROW_TYPE_ERROR("no media codecs found");
			}
		}

		// Iterate all the encodings, set the first payloadType in all of them with
		// codecPayloadType unset, and check that others point to a media codec.
		//
		// Also, don't allow multiple SVC spatial layers into an encoding if there
		// are more than one encoding (simulcast).
		for (auto& encoding : this->encodings)
		{
			if (encoding.spatialLayers > 1 && this->encodings.size() > 1)
			{
				MS_THROW_TYPE_ERROR(
				  "cannot use both simulcast and encodings with multiple SVC spatial layers");
			}

			if (!encoding.hasCodecPayloadType)
			{
				encoding.codecPayloadType    = firstMediaPayloadType;
				encoding.hasCodecPayloadType = true;
			}
			else
			{
				auto it = this->codecs.begin();

				for (; it != this->codecs.end(); ++it)
				{
					auto codec = *it;

					if (codec.payloadType == encoding.codecPayloadType)
					{
						// Must be a media codec.
						if (codec.mimeType.IsMediaCodec())
						{
							break;
						}

						MS_THROW_TYPE_ERROR("invalid codecPayloadType");
					}
				}

				if (it == this->codecs.end())
				{
					MS_THROW_TYPE_ERROR("unknown codecPayloadType");
				}
			}
		}
	}
} // namespace RTC
