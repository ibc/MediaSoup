#ifndef MS_CHANNEL_UNIX_STREAM_SOCKET_HPP
#define MS_CHANNEL_UNIX_STREAM_SOCKET_HPP

#include "common.hpp"
#include "handles/UnixStreamSocket.hpp"
#include "Channel/Request.hpp"
#include <json/json.h>

namespace Channel
{
	class UnixStreamSocket :
		public ::UnixStreamSocket
	{
	public:
		class Listener
		{
		public:
			virtual void onChannelRequest(Channel::UnixStreamSocket* channel, Channel::Request* request) = 0;
			virtual void onChannelUnixStreamSocketRemotelyClosed(Channel::UnixStreamSocket* channel) = 0;
		};

	private:
		static uint8_t writeBuffer[];

	public:
		explicit UnixStreamSocket(int fd);
		virtual ~UnixStreamSocket();

		void SetListener(Listener* listener);
		void Send(Json::Value &json);
		void SendLog(char* ns_payload, size_t ns_payload_len);
		void SendBinary(const uint8_t* ns_payload, size_t ns_payload_len);

	/* Pure virtual methods inherited from ::UnixStreamSocket. */
	public:
		virtual void userOnUnixStreamRead() override;
		virtual void userOnUnixStreamSocketClosed(bool is_closed_by_peer) override;

	private:
    void HandleRequest(Json::Value& json, const uint8_t* binary = nullptr, size_t len = 0);

		// Passed by argument.
		Listener* listener = nullptr;
		// Others.
		Json::CharReader* jsonReader = nullptr;
		Json::StreamWriter* jsonWriter = nullptr;
		size_t msgStart = 0; // Where the latest message starts.
    Json::Value lastBinaryRequest;
		bool closed = false;
	};
}

#endif
