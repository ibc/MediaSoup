#define MS_CLASS "RTC::AudioLevelObserver"
// #define MS_LOG_DEV_LEVEL 3

#include "RTC/AudioLevelObserver.hpp"
#include "Logger.hpp"
#include "MediaSoupErrors.hpp"
#include "RTC/RtpDictionaries.hpp"
#include <absl/container/btree_map.h>
#include <cmath> // std::lround()

namespace RTC
{
	/* Instance methods. */

	AudioLevelObserver::AudioLevelObserver(
	  RTC::Shared* shared,
	  const std::string& id,
	  RTC::RtpObserver::Listener* listener,
	  const FBS::AudioLevelObserver::AudioLevelObserverOptions* options)
	  : RTC::RtpObserver(shared, id, listener)
	{
		MS_TRACE();

		this->maxEntries = options->maxEntries();
		this->threshold  = options->threshold();
		this->interval   = options->interval();

		if (this->threshold > 0)
		{
			MS_THROW_TYPE_ERROR("invalid threshold value %" PRIi8, this->threshold);
		}

		if (this->interval < 250)
		{
			this->interval = 250;
		}
		else if (this->interval > 5000)
		{
			this->interval = 5000;
		}

		this->periodicTimer = new TimerHandle(this);

		this->periodicTimer->Start(this->interval, this->interval);

		// NOTE: This may throw.
		this->shared->channelMessageRegistrator->RegisterHandler(
		  this->id,
		  /*channelRequestHandler*/ this,
		  /*channelNotificationHandler*/ nullptr);
	}

	AudioLevelObserver::~AudioLevelObserver()
	{
		MS_TRACE();

		this->shared->channelMessageRegistrator->UnregisterHandler(this->id);

		delete this->periodicTimer;
	}

	void AudioLevelObserver::AddProducer(RTC::Producer* producer)
	{
		MS_TRACE();

		if (producer->GetKind() != RTC::Media::Kind::AUDIO)
		{
			MS_THROW_TYPE_ERROR("not an audio Producer");
		}

		// Insert into the map.
		this->mapProducerDBovs[producer];
	}

	void AudioLevelObserver::RemoveProducer(RTC::Producer* producer)
	{
		MS_TRACE();

		// Remove from the map.
		this->mapProducerDBovs.erase(producer);
	}

	void AudioLevelObserver::ReceiveRtpPacket(RTC::Producer* producer, RTC::RtpPacket* packet)
	{
		MS_TRACE();

		if (IsPaused())
		{
			return;
		}

		uint8_t volume;
		bool voice;

		if (!packet->ReadSsrcAudioLevel(volume, voice))
		{
			return;
		}

		auto& dBovs = this->mapProducerDBovs.at(producer);

		dBovs.totalSum += volume;
		dBovs.count++;
	}

	void AudioLevelObserver::ProducerPaused(RTC::Producer* producer)
	{
		// Remove from the map.
		this->mapProducerDBovs.erase(producer);
	}

	void AudioLevelObserver::ProducerResumed(RTC::Producer* producer)
	{
		// Insert into the map.
		this->mapProducerDBovs[producer];
	}

	void AudioLevelObserver::Paused()
	{
		MS_TRACE();

		this->periodicTimer->Stop();

		ResetMapProducerDBovs();

		if (!this->silence)
		{
			this->silence = true;

			this->shared->channelNotifier->Emit(
			  this->id, FBS::Notification::Event::AUDIOLEVELOBSERVER_SILENCE);
		}
	}

	void AudioLevelObserver::Resumed()
	{
		MS_TRACE();

		this->periodicTimer->Restart();
	}

	void AudioLevelObserver::Update()
	{
		MS_TRACE();

		absl::btree_multimap<int8_t, RTC::Producer*> mapDBovsProducer;

		for (auto& kv : this->mapProducerDBovs)
		{
			auto* producer = kv.first;
			auto& dBovs    = kv.second;

			if (dBovs.count < 10)
			{
				continue;
			}

			auto avgDBov = -1 * static_cast<int8_t>(std::lround(dBovs.totalSum / dBovs.count));

			if (avgDBov >= this->threshold)
			{
				mapDBovsProducer.insert({ avgDBov, producer });
			}
		}

		// Clear the map.
		ResetMapProducerDBovs();

		if (!mapDBovsProducer.empty())
		{
			this->silence = false;

			uint16_t idx{ 0 };
			auto rit = mapDBovsProducer.crbegin();

			std::vector<flatbuffers::Offset<FBS::AudioLevelObserver::Volume>> volumes;

			for (; idx < this->maxEntries && rit != mapDBovsProducer.crend(); ++idx, ++rit)
			{
				volumes.emplace_back(FBS::AudioLevelObserver::CreateVolumeDirect(
				  this->shared->channelNotifier->GetBufferBuilder(), rit->second->id.c_str(), rit->first));
			}

			auto notification = FBS::AudioLevelObserver::CreateVolumesNotificationDirect(
			  this->shared->channelNotifier->GetBufferBuilder(), &volumes);

			this->shared->channelNotifier->Emit(
			  this->id,
			  FBS::Notification::Event::AUDIOLEVELOBSERVER_VOLUMES,
			  FBS::Notification::Body::AudioLevelObserver_VolumesNotification,
			  notification);
		}
		else if (!this->silence)
		{
			this->silence = true;

			this->shared->channelNotifier->Emit(
			  this->id, FBS::Notification::Event::AUDIOLEVELOBSERVER_SILENCE);
		}
	}

	void AudioLevelObserver::ResetMapProducerDBovs()
	{
		MS_TRACE();

		for (auto& kv : this->mapProducerDBovs)
		{
			auto& dBovs = kv.second;

			dBovs.totalSum = 0;
			dBovs.count    = 0;
		}
	}

	inline void AudioLevelObserver::OnTimer(TimerHandle* /*timer*/)
	{
		MS_TRACE();

		Update();
	}
} // namespace RTC
