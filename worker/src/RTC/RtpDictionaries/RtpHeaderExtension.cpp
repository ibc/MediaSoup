#define MS_CLASS "RTC::RtpHeaderExtension"
// #define MS_LOG_DEV

#include "RTC/RtpDictionaries.hpp"
#include "MediaSoupError.hpp"
#include "Logger.hpp"

namespace RTC
{
	/* Instance methods. */

	RtpHeaderExtension::RtpHeaderExtension(Json::Value& data)
	{
		MS_TRACE();

		static const Json::StaticString k_kind("kind");
		static const Json::StaticString k_uri("uri");
		static const Json::StaticString k_preferredId("preferredId");
		static const Json::StaticString k_preferredEncrypt("preferredEncrypt");

		if (!data.isObject())
			MS_THROW_ERROR("RtpHeaderExtension is not an object");

		// `kind` is mandatory.
		if (!data[k_kind].isString())
			MS_THROW_ERROR("missing RtpCodecCapability.kind");

		std::string kind = data[k_kind].asString();

		// NOTE: This may throw.
		this->kind = RTC::Media::GetKind(kind);

		// `uri` is mandatory.
		if (!data[k_uri].isString())
			MS_THROW_ERROR("missing RtpHeaderExtension.uri");

		this->uri = data[k_uri].asString();
		if (this->uri.empty())
			MS_THROW_ERROR("empty RtpHeaderExtension.uri");

		// Get the type.
		this->type = RTC::RtpHeaderExtensionUri::GetType(this->uri);

		// `preferredId` is mandatory.
		if (!data[k_preferredId].isUInt())
			MS_THROW_ERROR("missing RtpHeaderExtension.preferredId");

		this->preferredId = (uint8_t)data[k_preferredId].asUInt();

		// `preferredEncrypt` is optional.
		if (data[k_preferredEncrypt].isBool())
			this->preferredEncrypt = data[k_preferredEncrypt].asBool();
	}

	Json::Value RtpHeaderExtension::toJson() const
	{
		MS_TRACE();

		static const Json::StaticString k_kind("kind");
		static const Json::StaticString k_uri("uri");
		static const Json::StaticString k_preferredId("preferredId");
		static const Json::StaticString k_preferredEncrypt("preferredEncrypt");

		Json::Value json(Json::objectValue);

		// Add `kind`.
		json[k_kind] = RTC::Media::GetJsonString(this->kind);

		// Add `uri`.
		json[k_uri] = this->uri;

		// Add `preferredId`.
		json[k_preferredId] = (Json::UInt)this->preferredId;

		// Add `preferredEncrypt`.
		json[k_preferredEncrypt] = this->preferredEncrypt;

		return json;
	}
}
