#ifndef MS_RTC_SHARED_HPP
#define MS_RTC_SHARED_HPP

#include "ChannelMessageRegistrator.hpp"
#include "Channel/ChannelNotifier.hpp"

namespace RTC
{
	class Shared
	{
	public:
		explicit Shared(
		  ChannelMessageRegistrator* channelMessageRegistrator,
		  Channel::ChannelNotifier* channelNotifier);
		~Shared();

	public:
		ChannelMessageRegistrator* channelMessageRegistrator{ nullptr };
		Channel::ChannelNotifier* channelNotifier{ nullptr };
	};
} // namespace RTC

#endif
