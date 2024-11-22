#ifndef MS_CHANNEL_NOTIFICATION_HPP
#define MS_CHANNEL_NOTIFICATION_HPP

#include "common.hpp"
#include "FBS/notification.h"
#include <absl/container/flat_hash_map.h>
#include <string>

namespace Channel
{
	class ChannelNotification
	{
	public:
		using Event = FBS::Notification::Event;

	private:
		static absl::flat_hash_map<FBS::Notification::Event, const char*> event2String;

	public:
		explicit ChannelNotification(const FBS::Notification::Notification* notification);
		~ChannelNotification() = default;

	public:
		// Passed by argument.
		Event event;
		// Others.
		const char* eventCStr;
		std::string handlerId;
		const FBS::Notification::Notification* data{ nullptr };
	};
} // namespace Channel

#endif
