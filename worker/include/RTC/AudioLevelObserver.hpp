#ifndef MS_RTC_AUDIO_LEVEL_OBSERVER_HPP
#define MS_RTC_AUDIO_LEVEL_OBSERVER_HPP

#include "RTC/RtpObserver.hpp"
#include "RTC/Shared.hpp"
#include "handles/TimerHandle.hpp"
#include <absl/container/flat_hash_map.h>

namespace RTC
{
	class AudioLevelObserver : public RTC::RtpObserver, public TimerHandle::Listener
	{
	private:
		struct DBovs
		{
			uint16_t totalSum{ 0u }; // Sum of dBvos (positive integer).
			size_t count{ 0u };      // Number of dBvos entries in totalSum.
		};

	public:
		AudioLevelObserver(
		  RTC::Shared* shared,
		  const std::string& id,
		  RTC::RtpObserver::Listener* listener,
		  const FBS::AudioLevelObserver::AudioLevelObserverOptions* options);
		~AudioLevelObserver() override;

	public:
		void AddProducer(RTC::Producer* producer) override;
		void RemoveProducer(RTC::Producer* producer) override;
		void ReceiveRtpPacket(RTC::Producer* producer, RTC::RtpPacket* packet) override;
		void ProducerPaused(RTC::Producer* producer) override;
		void ProducerResumed(RTC::Producer* producer) override;

	private:
		void Paused() override;
		void Resumed() override;
		void Update();
		void ResetMapProducerDBovs();

		/* Pure virtual methods inherited from TimerHandle. */
	protected:
		void OnTimer(TimerHandle* timer) override;

	private:
		// Passed by argument.
		uint16_t maxEntries{ 1u };
		int8_t threshold{ -80 };
		uint16_t interval{ 1000u };
		// Allocated by this.
		TimerHandle* periodicTimer{ nullptr };
		// Others.
		absl::flat_hash_map<RTC::Producer*, DBovs> mapProducerDBovs;
		bool silence{ true };
	};
} // namespace RTC

#endif
