#ifndef MS_PAYLOAD_CHANNEL_REQUEST_HPP
#define MS_PAYLOAD_CHANNEL_REQUEST_HPP

#include "common.hpp"
#include <absl/container/flat_hash_map.h>
#include <nlohmann/json.hpp>
#include <string>

using json = nlohmann::json;

namespace PayloadChannel
{
	// Avoid cyclic #include problem by declaring classes instead of including
	// the corresponding header files.
	class PayloadChannelSocket;

	class PayloadChannelRequest
	{
	public:
		enum class MethodId
		{
			DATA_CONSUMER_SEND
		};

	public:
		static bool IsRequest(const char* msg);

	private:
		static absl::flat_hash_map<std::string, MethodId> string2MethodId;

	public:
		PayloadChannelRequest(PayloadChannel::PayloadChannelSocket* channel, char* msg, size_t msgLen);
		virtual ~PayloadChannelRequest();

	public:
		void Accept();
		void Accept(json& data);
		void Error(const char* reason = nullptr);
		void TypeError(const char* reason = nullptr);
		void SetPayload(const uint8_t* payload, size_t payloadLen);
		const std::string& GetNextInternalRoutingId();

	public:
		// Passed by argument.
		PayloadChannel::PayloadChannelSocket* channel{ nullptr };
		uint32_t id{ 0u };
		std::string method;
		MethodId methodId;
		std::vector<std::string> internal;
		std::string data;
		const uint8_t* payload{ nullptr };
		size_t payloadLen{ 0u };
		// Others.
		bool replied{ false };

	private:
		uint8_t nextRoutingLevel{ 0 };
	};
} // namespace PayloadChannel

#endif
