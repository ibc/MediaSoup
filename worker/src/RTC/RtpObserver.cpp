#define MS_CLASS "RTC::RtpObserver"
// #define MS_LOG_DEV_LEVEL 3

#include "RTC/RtpObserver.hpp"
#include "Logger.hpp"
#include "MediaSoupErrors.hpp"

namespace RTC
{
	/* Instance methods. */

	RtpObserver::RtpObserver(RTC::Shared* shared, const std::string& id, RTC::RtpObserver::Listener* listener)
	  : id(id), shared(shared), listener(listener)
	{
		MS_TRACE();
	}

	RtpObserver::~RtpObserver()
	{
		MS_TRACE();
	}

	void RtpObserver::HandleRequest(Channel::ChannelRequest* request)
	{
		MS_TRACE();

		switch (request->method)
		{
			case Channel::ChannelRequest::Method::RTPOBSERVER_PAUSE:
			{
				this->Pause();

				request->Accept();

				break;
			}

			case Channel::ChannelRequest::Method::RTPOBSERVER_RESUME:
			{
				this->Resume();

				request->Accept();

				break;
			}

			case Channel::ChannelRequest::Method::RTPOBSERVER_ADD_PRODUCER:
			{
				const auto* body = request->data->body_as<FBS::RtpObserver::AddProducerRequest>();
				auto producerId  = body->producerId()->str();

				RTC::Producer* producer = this->listener->RtpObserverGetProducer(this, producerId);

				this->AddProducer(producer);

				this->listener->OnRtpObserverAddProducer(this, producer);

				request->Accept();

				break;
			}

			case Channel::ChannelRequest::Method::RTPOBSERVER_REMOVE_PRODUCER:
			{
				const auto* body = request->data->body_as<FBS::RtpObserver::RemoveProducerRequest>();
				auto producerId  = body->producerId()->str();

				RTC::Producer* producer = this->listener->RtpObserverGetProducer(this, producerId);

				this->RemoveProducer(producer);

				// Remove from the map.
				this->listener->OnRtpObserverRemoveProducer(this, producer);

				request->Accept();

				break;
			}

			default:
			{
				MS_THROW_ERROR("unknown method '%s'", request->methodCStr);
			}
		}
	}

	void RtpObserver::Pause()
	{
		MS_TRACE();

		if (this->paused)
		{
			return;
		}

		this->paused = true;

		Paused();
	}

	void RtpObserver::Resume()
	{
		MS_TRACE();

		if (!this->paused)
		{
			return;
		}

		this->paused = false;

		Resumed();
	}
} // namespace RTC
