#ifndef MS_CHANNEL_NOTIFIER_HPP
#define MS_CHANNEL_NOTIFIER_HPP

#include "common.hpp"
#include "Channel/ChannelSocket.hpp"
#include <string>

namespace Channel
{
	class ChannelNotifier
	{
	public:
		explicit ChannelNotifier(Channel::ChannelSocket* channel);

	public:
		flatbuffers::FlatBufferBuilder& GetBufferBuilder()
		{
			return this->bufferBuilder;
		}

		template<class Body>
		void Emit(
		  const std::string& targetId,
		  FBS::Notification::Event event,
		  FBS::Notification::Body type,
		  flatbuffers::Offset<Body>& body)
		{
			auto& builder     = this->bufferBuilder;
			auto notification = FBS::Notification::CreateNotificationDirect(
			  builder, targetId.c_str(), event, type, body.Union());

			auto message =
			  FBS::Message::CreateMessage(builder, FBS::Message::Body::Notification, notification.Union());

			builder.FinishSizePrefixed(message);
			this->channel->Send(builder.GetBufferPointer(), builder.GetSize());
			builder.Clear();
		}

		void Emit(const std::string& targetId, FBS::Notification::Event event)
		{
			auto& builder = ChannelNotifier::bufferBuilder;
			auto notification =
			  FBS::Notification::CreateNotificationDirect(builder, targetId.c_str(), event);

			auto message =
			  FBS::Message::CreateMessage(builder, FBS::Message::Body::Notification, notification.Union());

			builder.FinishSizePrefixed(message);
			this->channel->Send(builder.GetBufferPointer(), builder.GetSize());
			builder.Clear();
		}

	private:
		// Passed by argument.
		Channel::ChannelSocket* channel{ nullptr };
		// Others.
		flatbuffers::FlatBufferBuilder bufferBuilder{};
	};
} // namespace Channel

#endif
