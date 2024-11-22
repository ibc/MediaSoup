#define MS_CLASS "Channel::Notifier"
// #define MS_LOG_DEV_LEVEL 3

#include "Channel/ChannelNotifier.hpp"
#include "Logger.hpp"

namespace Channel
{
	ChannelNotifier::ChannelNotifier(Channel::ChannelSocket* channel) : channel(channel)
	{
		MS_TRACE();
	}
} // namespace Channel
