#ifndef MS_CHANNEL_MESSAGE_REGISTRATOR_HPP
#define MS_CHANNEL_MESSAGE_REGISTRATOR_HPP

#include "common.hpp"
#include "Channel/ChannelSocket.hpp"
#include <absl/container/flat_hash_map.h>
#include <string>

class ChannelMessageRegistrator
{
public:
	explicit ChannelMessageRegistrator();
	~ChannelMessageRegistrator();

public:
	flatbuffers::Offset<FBS::Worker::ChannelMessageHandlers> FillBuffer(
	  flatbuffers::FlatBufferBuilder& builder);
	void RegisterHandler(
	  const std::string& id,
	  Channel::ChannelSocket::RequestHandler* channelRequestHandler,
	  Channel::ChannelSocket::NotificationHandler* channelNotificationHandler);
	void UnregisterHandler(const std::string& id);
	Channel::ChannelSocket::RequestHandler* GetChannelRequestHandler(const std::string& id);
	Channel::ChannelSocket::NotificationHandler* GetChannelNotificationHandler(const std::string& id);

private:
	absl::flat_hash_map<std::string, Channel::ChannelSocket::RequestHandler*> mapChannelRequestHandlers;
	absl::flat_hash_map<std::string, Channel::ChannelSocket::NotificationHandler*> mapChannelNotificationHandlers;
};

#endif
