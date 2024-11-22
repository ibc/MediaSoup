#define MS_CLASS "ChannelMessageRegistrator"
// #define MS_LOG_DEV_LEVEL 3

#include "ChannelMessageRegistrator.hpp"
#include "Logger.hpp"
#include "MediaSoupErrors.hpp"

ChannelMessageRegistrator::ChannelMessageRegistrator()
{
	MS_TRACE();
}

ChannelMessageRegistrator::~ChannelMessageRegistrator()
{
	MS_TRACE();

	this->mapChannelRequestHandlers.clear();
	this->mapChannelNotificationHandlers.clear();
}

flatbuffers::Offset<FBS::Worker::ChannelMessageHandlers> ChannelMessageRegistrator::FillBuffer(
  flatbuffers::FlatBufferBuilder& builder)
{
	// Add channelRequestHandlerIds.
	std::vector<flatbuffers::Offset<flatbuffers::String>> channelRequestHandlerIds;
	for (const auto& kv : this->mapChannelRequestHandlers)
	{
		const auto& handlerId = kv.first;

		channelRequestHandlerIds.push_back(builder.CreateString(handlerId));
	}

	// Add channelNotificationHandlerIds.
	std::vector<flatbuffers::Offset<flatbuffers::String>> channelNotificationHandlerIds;
	for (const auto& kv : this->mapChannelNotificationHandlers)
	{
		const auto& handlerId = kv.first;

		channelNotificationHandlerIds.push_back(builder.CreateString(handlerId));
	}

	return FBS::Worker::CreateChannelMessageHandlersDirect(
	  builder, &channelRequestHandlerIds, &channelNotificationHandlerIds);
}

void ChannelMessageRegistrator::RegisterHandler(
  const std::string& id,
  Channel::ChannelSocket::RequestHandler* channelRequestHandler,
  Channel::ChannelSocket::NotificationHandler* channelNotificationHandler)
{
	MS_TRACE();

	if (channelRequestHandler != nullptr)
	{
		if (this->mapChannelRequestHandlers.find(id) != this->mapChannelRequestHandlers.end())
		{
			MS_THROW_ERROR("Channel request handler with ID %s already exists", id.c_str());
		}

		this->mapChannelRequestHandlers[id] = channelRequestHandler;
	}

	if (channelNotificationHandler != nullptr)
	{
		if (this->mapChannelNotificationHandlers.find(id) != this->mapChannelNotificationHandlers.end())
		{
			if (channelRequestHandler != nullptr)
			{
				this->mapChannelRequestHandlers.erase(id);
			}

			MS_THROW_ERROR("Channel notification handler with ID %s already exists", id.c_str());
		}

		this->mapChannelNotificationHandlers[id] = channelNotificationHandler;
	}
}

void ChannelMessageRegistrator::UnregisterHandler(const std::string& id)
{
	MS_TRACE();

	this->mapChannelRequestHandlers.erase(id);
	this->mapChannelNotificationHandlers.erase(id);
}

Channel::ChannelSocket::RequestHandler* ChannelMessageRegistrator::GetChannelRequestHandler(
  const std::string& id)
{
	MS_TRACE();

	auto it = this->mapChannelRequestHandlers.find(id);

	if (it != this->mapChannelRequestHandlers.end())
	{
		return it->second;
	}
	else
	{
		return nullptr;
	}
}

Channel::ChannelSocket::NotificationHandler* ChannelMessageRegistrator::GetChannelNotificationHandler(
  const std::string& id)
{
	MS_TRACE();

	auto it = this->mapChannelNotificationHandlers.find(id);

	if (it != this->mapChannelNotificationHandlers.end())
	{
		return it->second;
	}
	else
	{
		return nullptr;
	}
}
