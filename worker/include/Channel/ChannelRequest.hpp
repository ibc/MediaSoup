#ifndef MS_CHANNEL_REQUEST_HPP
#define MS_CHANNEL_REQUEST_HPP

#include "common.hpp"
#include "FBS/message.h"
#include "FBS/request.h"
#include "FBS/response.h"
#include <flatbuffers/minireflect.h>
#include <absl/container/flat_hash_map.h>
#include <string>

namespace Channel
{
	// Avoid cyclic #include problem by declaring classes instead of including
	// the corresponding header files.
	class ChannelSocket;

	class ChannelRequest
	{
	public:
		using Method = FBS::Request::Method;

	public:
		static absl::flat_hash_map<FBS::Request::Method, const char*> method2String;
		thread_local static flatbuffers::FlatBufferBuilder bufferBuilder;

	public:
		ChannelRequest(Channel::ChannelSocket* channel, const FBS::Request::Request* request);
		~ChannelRequest() = default;

		flatbuffers::FlatBufferBuilder& GetBufferBuilder()
		{
			return ChannelRequest::bufferBuilder;
		}
		void Accept();
		template<class Body>
		void Accept(FBS::Response::Body type, flatbuffers::Offset<Body>& body)
		{
			assert(!this->replied);

			this->replied = true;

			auto& builder = ChannelRequest::bufferBuilder;
			auto response = FBS::Response::CreateResponse(builder, this->id, true, type, body.Union());

			auto message =
			  FBS::Message::CreateMessage(builder, FBS::Message::Body::Response, response.Union());

			builder.FinishSizePrefixed(message);
			this->Send(builder.GetBufferPointer(), builder.GetSize());
			builder.Clear();
		}
		void Error(const char* reason = nullptr);
		void TypeError(const char* reason = nullptr);

	private:
		void Send(uint8_t* buffer, size_t size) const;
		void SendResponse(const flatbuffers::Offset<FBS::Response::Response>& response);

	public:
		// Passed by argument.
		Channel::ChannelSocket* channel{ nullptr };
		const FBS::Request::Request* data{ nullptr };
		// Others.
		uint32_t id{ 0u };
		Method method;
		const char* methodCStr;
		std::string handlerId;
		bool replied{ false };
	};
} // namespace Channel

#endif
