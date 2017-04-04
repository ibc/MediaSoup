#ifndef MS_CHANNEL_REQUEST_HPP
#define MS_CHANNEL_REQUEST_HPP

#include "common.hpp"
#include <string>
#include <unordered_map>
#include <json/json.h>

namespace Channel
{
	// Avoid cyclic #include problem by declaring classes instead of including
	// the corresponding header files.
	class UnixStreamSocket;

	class Request
	{
	public:
		enum class MethodId
		{
			worker_dump = 1,
			worker_updateSettings,
			worker_createRoom,
			room_close,
			room_dump,
			room_createPeer,
			peer_close,
			peer_dump,
			peer_setCapabilities,
			peer_createTransport,
			peer_createRtpReceiver,
			transport_close,
			transport_dump,
			transport_setRemoteDtlsParameters,
			transport_setMaxBitrate,
			rtpReceiver_close,
			rtpReceiver_dump,
			rtpReceiver_receive,
			rtpReceiver_setRtpRawEvent,
			rtpReceiver_setRtpObjectEvent,
			rtpReceiver_receiveRtpPacket,
			rtpSender_dump,
			rtpSender_setTransport,
			rtpSender_disable
		};

	private:
		static std::unordered_map<std::string, MethodId> string2MethodId;

	public:
		Request(Channel::UnixStreamSocket* channel, Json::Value& json, const uint8_t* binary, size_t len);
		virtual ~Request();

		void Accept();
		void Accept(Json::Value &data);
		void Reject(std::string& reason);
		void Reject(const char* reason = nullptr);

	public:
		// Passed by argument.
		Channel::UnixStreamSocket* channel = nullptr;
		uint32_t id;
		std::string method;
		MethodId methodId;
		Json::Value internal;
		Json::Value data;
		const uint8_t* binary;
		size_t len;
		// Others.
		bool replied = false;
	};
}

#endif
