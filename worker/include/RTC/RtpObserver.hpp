#ifndef MS_RTC_RTP_OBSERVER_HPP
#define MS_RTC_RTP_OBSERVER_HPP

#include "common.hpp"
#include "RTC/Producer.hpp"
#include "RTC/RtpPacket.hpp"
#include "RTC/Shared.hpp"
#include <string>

namespace RTC
{
	class RtpObserver : public Channel::ChannelSocket::RequestHandler
	{
	public:
		class Listener
		{
		public:
			virtual ~Listener() = default;

		public:
			virtual RTC::Producer* RtpObserverGetProducer(
			  RTC::RtpObserver* rtpObserver, const std::string& id) = 0;
			virtual void OnRtpObserverAddProducer(RTC::RtpObserver* rtpObserver, RTC::Producer* producer) = 0;
			virtual void OnRtpObserverRemoveProducer(
			  RTC::RtpObserver* rtpObserver, RTC::Producer* producer) = 0;
		};

	public:
		RtpObserver(RTC::Shared* shared, const std::string& id, RTC::RtpObserver::Listener* listener);
		~RtpObserver() override;

	public:
		void Pause();
		void Resume();
		bool IsPaused() const
		{
			return this->paused;
		}
		virtual void AddProducer(RTC::Producer* producer)                              = 0;
		virtual void RemoveProducer(RTC::Producer* producer)                           = 0;
		virtual void ReceiveRtpPacket(RTC::Producer* producer, RTC::RtpPacket* packet) = 0;
		virtual void ProducerPaused(RTC::Producer* producer)                           = 0;
		virtual void ProducerResumed(RTC::Producer* producer)                          = 0;

		/* Methods inherited from Channel::ChannelSocket::RequestHandler. */
	public:
		void HandleRequest(Channel::ChannelRequest* request) override;

	protected:
		virtual void Paused()  = 0;
		virtual void Resumed() = 0;

	public:
		// Passed by argument.
		const std::string id;

	protected:
		// Passed by argument.
		RTC::Shared* shared{ nullptr };

	private:
		// Passed by argument.
		RTC::RtpObserver::Listener* listener{ nullptr };
		// Others.
		bool paused{ false };
	};
} // namespace RTC

#endif
