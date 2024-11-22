#ifndef MS_RTC_ACTIVE_SPEAKER_OBSERVER_HPP
#define MS_RTC_ACTIVE_SPEAKER_OBSERVER_HPP

#include "RTC/RtpObserver.hpp"
#include "RTC/Shared.hpp"
#include "handles/TimerHandle.hpp"
#include <absl/container/flat_hash_map.h>
#include <utility>
#include <vector>

// Implementation of Dominant Speaker Identification for Multipoint
// Videoconferencing by Ilana Volfin and Israel Cohen. This implementation uses
// the RTP Audio Level extension from RFC-6464 for the input signal. This has
// been ported from DominantSpeakerIdentification.java in Jitsi:
// https://github.com/jitsi/jitsi-utils/blob/master/src/main/java/org/jitsi/utils/dsi/DominantSpeakerIdentification.java
namespace RTC
{
	class ActiveSpeakerObserver : public RTC::RtpObserver, public TimerHandle::Listener
	{
	private:
		class Speaker
		{
		public:
			Speaker();
			void EvalActivityScores();
			double GetActivityScore(uint8_t interval) const;
			void LevelChanged(uint32_t level, uint64_t now);
			void LevelTimedOut(uint64_t now);

		private:
			bool ComputeImmediates();
			bool ComputeLongs();
			bool ComputeMediums();
			void EvalImmediateActivityScore();
			void EvalMediumActivityScore();
			void EvalLongActivityScore();
			void UpdateMinLevel(int8_t level);

		public:
			bool paused{ false };
			double immediateActivityScore{ 0 };
			double mediumActivityScore{ 0 };
			double longActivityScore{ 0 };
			uint64_t lastLevelChangeTime{ 0 };

		private:
			uint8_t minLevel{ 0u };
			uint8_t nextMinLevel{ 0u };
			uint32_t nextMinLevelWindowLen{ 0u };
			std::vector<uint8_t> immediates;
			std::vector<uint8_t> mediums;
			std::vector<uint8_t> longs;
			std::vector<uint8_t> levels;
			size_t nextLevelIndex{ 0u };
		};

		class ProducerSpeaker
		{
		public:
			explicit ProducerSpeaker(RTC::Producer* producer);
			~ProducerSpeaker();

		public:
			RTC::Producer* producer;
			Speaker* speaker;
		};

	private:
		static const size_t RelativeSpeachActivitiesLen{ 3u };

	public:
		ActiveSpeakerObserver(
		  RTC::Shared* shared,
		  const std::string& id,
		  RTC::RtpObserver::Listener* listener,
		  const FBS::ActiveSpeakerObserver::ActiveSpeakerObserverOptions* options);
		~ActiveSpeakerObserver() override;

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
		bool CalculateActiveSpeaker();
		void TimeoutIdleLevels(uint64_t now);

		/* Pure virtual methods inherited from TimerHandle. */
	protected:
		void OnTimer(TimerHandle* timer) override;

	private:
		double relativeSpeachActivities[RelativeSpeachActivitiesLen]{};
		std::string dominantId;
		TimerHandle* periodicTimer{ nullptr };
		uint16_t interval{ 300u };
		// Map of ProducerSpeakers indexed by Producer id.
		absl::flat_hash_map<std::string, ProducerSpeaker*> mapProducerSpeakers;
		uint64_t lastLevelIdleTime{ 0u };
	};
} // namespace RTC

#endif
