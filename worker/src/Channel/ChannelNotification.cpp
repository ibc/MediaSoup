#define MS_CLASS "Channel::ChannelNotification"
// #define MS_LOG_DEV_LEVEL 3

#include "Channel/ChannelNotification.hpp"
#include "Logger.hpp"
#include "MediaSoupErrors.hpp"

namespace Channel
{
	/* Class variables. */

	// clang-format off
	absl::flat_hash_map<FBS::Notification::Event, const char*> ChannelNotification::event2String =
	{
		{ FBS::Notification::Event::TRANSPORT_SEND_RTCP, "transport.sendRtcp" },
		{ FBS::Notification::Event::PRODUCER_SEND,       "producer.send"      },
		{ FBS::Notification::Event::DATAPRODUCER_SEND,   "dataProducer.send"  },
	};
	// clang-format on

	/* Instance methods. */

	ChannelNotification::ChannelNotification(const FBS::Notification::Notification* notification)
	{
		MS_TRACE();

		this->data  = notification;
		this->event = notification->event();

		auto eventCStrIt = ChannelNotification::event2String.find(this->event);

		if (eventCStrIt == ChannelNotification::event2String.end())
		{
			MS_THROW_ERROR("unknown event '%" PRIu8 "'", static_cast<uint8_t>(this->event));
		}

		this->eventCStr = eventCStrIt->second;
		this->handlerId = this->data->handlerId()->str();
	}
} // namespace Channel
