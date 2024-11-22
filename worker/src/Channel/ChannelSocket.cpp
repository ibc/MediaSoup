#define MS_CLASS "Channel::ChannelSocket"
// #define MS_LOG_DEV_LEVEL 3

#include "Channel/ChannelSocket.hpp"
#include "DepLibUV.hpp"
#include "Logger.hpp"
#include "MediaSoupErrors.hpp"
#include <cstring> // std::memcpy(), std::memmove()

namespace Channel
{
	// Binary length for a 4194304 bytes payload.
	static constexpr size_t MessageMaxLen{ 4194308 };
	static constexpr size_t PayloadMaxLen{ 4194304 };

	/* Static methods for UV callbacks. */

	inline static void onAsync(uv_handle_t* handle)
	{
		while (static_cast<ChannelSocket*>(handle->data)->CallbackRead())
		{
			// Read while there are new messages.
		}
	}

	inline static void onCloseAsync(uv_handle_t* handle)
	{
		delete reinterpret_cast<uv_async_t*>(handle);
	}

	/* Instance methods. */

	ChannelSocket::ChannelSocket(int consumerFd, int producerFd)
	  : consumerSocket(new ConsumerSocket(consumerFd, MessageMaxLen, this)),
	    producerSocket(new ProducerSocket(producerFd, MessageMaxLen))
	{
		MS_TRACE_STD();
	}

	ChannelSocket::ChannelSocket(
	  ChannelReadFn channelReadFn,
	  ChannelReadCtx channelReadCtx,
	  ChannelWriteFn channelWriteFn,
	  ChannelWriteCtx channelWriteCtx)
	  : channelReadFn(channelReadFn), channelReadCtx(channelReadCtx), channelWriteFn(channelWriteFn),
	    channelWriteCtx(channelWriteCtx), uvReadHandle(new uv_async_t)
	{
		MS_TRACE_STD();

		int err;

		this->uvReadHandle->data = static_cast<void*>(this);

		err =
		  uv_async_init(DepLibUV::GetLoop(), this->uvReadHandle, reinterpret_cast<uv_async_cb>(onAsync));

		if (err != 0)
		{
			delete this->uvReadHandle;
			this->uvReadHandle = nullptr;

			MS_THROW_ERROR_STD("uv_async_init() failed: %s", uv_strerror(err));
		}

		err = uv_async_send(this->uvReadHandle);

		if (err != 0)
		{
			delete this->uvReadHandle;
			this->uvReadHandle = nullptr;

			MS_THROW_ERROR_STD("uv_async_send() failed: %s", uv_strerror(err));
		}
	}

	ChannelSocket::~ChannelSocket()
	{
		MS_TRACE_STD();

		if (!this->closed)
		{
			Close();
		}

		delete this->consumerSocket;
		delete this->producerSocket;
	}

	void ChannelSocket::Close()
	{
		MS_TRACE_STD();

		if (this->closed)
		{
			return;
		}

		this->closed = true;

		if (this->uvReadHandle)
		{
			uv_close(
			  reinterpret_cast<uv_handle_t*>(this->uvReadHandle), static_cast<uv_close_cb>(onCloseAsync));
		}

		if (this->consumerSocket)
		{
			this->consumerSocket->Close();
		}

		if (this->producerSocket)
		{
			this->producerSocket->Close();
		}
	}

	void ChannelSocket::SetListener(Listener* listener)
	{
		MS_TRACE_STD();

		this->listener = listener;
	}

	void ChannelSocket::Send(const uint8_t* data, uint32_t dataLen)
	{
		MS_TRACE_STD();

		if (this->closed)
		{
			return;
		}

		if (dataLen > PayloadMaxLen)
		{
			MS_ERROR_STD("message too big");

			return;
		}

		SendImpl(data, dataLen);
	}

	void ChannelSocket::SendLog(const char* data, uint32_t dataLen)
	{
		MS_TRACE_STD();

		if (this->closed)
		{
			return;
		}

		if (dataLen > PayloadMaxLen)
		{
			MS_ERROR_STD("message too big");

			return;
		}

		auto log = FBS::Log::CreateLogDirect(this->bufferBuilder, data);
		auto message =
		  FBS::Message::CreateMessage(this->bufferBuilder, FBS::Message::Body::Log, log.Union());

		this->bufferBuilder.FinishSizePrefixed(message);
		this->Send(this->bufferBuilder.GetBufferPointer(), this->bufferBuilder.GetSize());
		this->bufferBuilder.Clear();
	}

	bool ChannelSocket::CallbackRead()
	{
		MS_TRACE_STD();

		if (this->closed)
		{
			return false;
		}

		uint8_t* msg{ nullptr };
		uint32_t msgLen;
		size_t msgCtx;

		// Try to read next message using `channelReadFn`, message, its length and context will be
		// stored in provided arguments.
		auto free = this->channelReadFn(&msg, &msgLen, &msgCtx, this->uvReadHandle, this->channelReadCtx);

		// Non-null free function pointer means message was successfully read above and will need to be
		// freed later.
		if (free)
		{
			const auto* message = FBS::Message::GetMessage(msg);

#if MS_LOG_DEV_LEVEL == 3
			auto s = flatbuffers::FlatBufferToString(
			  reinterpret_cast<uint8_t*>(msg), FBS::Message::MessageTypeTable());
			MS_DUMP("%s", s.c_str());
#endif

			if (message->data_type() == FBS::Message::Body::Request)
			{
				ChannelRequest* request;

				try
				{
					request = new ChannelRequest(this, message->data_as<FBS::Request::Request>());

					// Notify the listener.
					this->listener->HandleRequest(request);
				}
				catch (const MediaSoupTypeError& error)
				{
					request->TypeError(error.what());
				}
				catch (const MediaSoupError& error)
				{
					request->Error(error.what());
				}

				delete request;
			}
			else if (message->data_type() == FBS::Message::Body::Notification)
			{
				ChannelNotification* notification;

				try
				{
					notification = new ChannelNotification(message->data_as<FBS::Notification::Notification>());

					// Notify the listener.
					this->listener->HandleNotification(notification);
				}
				catch (const MediaSoupError& error)
				{
					MS_ERROR("notification failed: %s", error.what());
				}

				delete notification;
			}
			else
			{
				MS_ERROR("discarding wrong Channel data");
			}

			// Message needs to be freed using stored function pointer.
			free(msg, msgLen, msgCtx);
		}

		// Return `true` if something was processed.
		return free != nullptr;
	}

	void ChannelSocket::SendImpl(const uint8_t* payload, uint32_t payloadLen)
	{
		MS_TRACE_STD();

		// Write using function call if provided.
		if (this->channelWriteFn)
		{
			this->channelWriteFn(payload, payloadLen, this->channelWriteCtx);
		}
		else
		{
			this->producerSocket->Write(payload, payloadLen);
		}
	}

	void ChannelSocket::OnConsumerSocketMessage(
	  ConsumerSocket* /*consumerSocket*/, char* msg, size_t /*msgLen*/)
	{
		MS_TRACE();

		const auto* message = FBS::Message::GetMessage(msg);

#if MS_LOG_DEV_LEVEL == 3
		auto s = flatbuffers::FlatBufferToString(
		  reinterpret_cast<uint8_t*>(msg), FBS::Message::MessageTypeTable());
		MS_DUMP("%s", s.c_str());
#endif

		if (message->data_type() == FBS::Message::Body::Request)
		{
			ChannelRequest* request;

			try
			{
				request = new ChannelRequest(this, message->data_as<FBS::Request::Request>());

				// Notify the listener.
				this->listener->HandleRequest(request);
			}
			catch (const MediaSoupTypeError& error)
			{
				request->TypeError(error.what());
			}
			catch (const MediaSoupError& error)
			{
				request->Error(error.what());
			}

			delete request;
		}
		else if (message->data_type() == FBS::Message::Body::Notification)
		{
			ChannelNotification* notification;

			try
			{
				notification = new ChannelNotification(message->data_as<FBS::Notification::Notification>());

				// Notify the listener.
				this->listener->HandleNotification(notification);
			}
			catch (const MediaSoupError& error)
			{
				MS_ERROR("notification failed: %s", error.what());
			}

			delete notification;
		}
		else
		{
			MS_ERROR("discarding wrong Channel data");
		}
	}

	void ChannelSocket::OnConsumerSocketClosed(ConsumerSocket* /*consumerSocket*/)
	{
		MS_TRACE_STD();

		this->listener->OnChannelClosed(this);
	}

	/* Instance methods. */

	ConsumerSocket::ConsumerSocket(int fd, size_t bufferSize, Listener* listener)
	  : ::UnixStreamSocketHandle(fd, bufferSize, ::UnixStreamSocketHandle::Role::CONSUMER),
	    listener(listener)
	{
		MS_TRACE_STD();
	}

	ConsumerSocket::~ConsumerSocket()
	{
		MS_TRACE_STD();
	}

	void ConsumerSocket::UserOnUnixStreamRead()
	{
		MS_TRACE_STD();

		size_t msgStart{ 0 };

		// Be ready to parse more than a single message in a single chunk.
		while (true)
		{
			if (IsClosed())
			{
				return;
			}

			const size_t readLen = this->bufferDataLen - msgStart;

			if (readLen < sizeof(uint32_t))
			{
				// Incomplete data.
				break;
			}

			uint32_t msgLen;
			// Read message length.
			std::memcpy(&msgLen, this->buffer + msgStart, sizeof(uint32_t));

			if (readLen < sizeof(uint32_t) + static_cast<size_t>(msgLen))
			{
				// Incomplete data.
				break;
			}

			this->listener->OnConsumerSocketMessage(
			  this,
			  reinterpret_cast<char*>(this->buffer + msgStart + sizeof(uint32_t)),
			  static_cast<size_t>(msgLen));

			msgStart += sizeof(uint32_t) + static_cast<size_t>(msgLen);
		}

		if (msgStart != 0)
		{
			this->bufferDataLen = this->bufferDataLen - msgStart;

			if (this->bufferDataLen != 0)
			{
				std::memmove(this->buffer, this->buffer + msgStart, this->bufferDataLen);
			}
		}
	}

	void ConsumerSocket::UserOnUnixStreamSocketClosed()
	{
		MS_TRACE_STD();

		// Notify the listener.
		this->listener->OnConsumerSocketClosed(this);
	}

	/* Instance methods. */

	ProducerSocket::ProducerSocket(int fd, size_t bufferSize)
	  : ::UnixStreamSocketHandle(fd, bufferSize, ::UnixStreamSocketHandle::Role::PRODUCER)
	{
		MS_TRACE_STD();
	}
} // namespace Channel
