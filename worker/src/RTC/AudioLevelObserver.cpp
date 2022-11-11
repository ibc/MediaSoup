#define MS_CLASS "RTC::AudioLevelObserver"
// #define MS_LOG_DEV_LEVEL 3

#include "RTC/AudioLevelObserver.hpp"
#include "ChannelMessageHandlers.hpp"
#include "Logger.hpp"
#include "MediaSoupErrors.hpp"
#include "Utils.hpp"
#include "Channel/ChannelNotifier.hpp"
#include "RTC/RtpDictionaries.hpp"
#include <cmath> // std::lround()
#include <map>

namespace RTC
{
	/* Instance methods. */

	AudioLevelObserver::AudioLevelObserver(
	  const std::string& id, RTC::RtpObserver::Listener* listener,
	  const FBS::Router::AudioLevelObserverOptions* options)
	  : RTC::RtpObserver(id, listener)
	{
		MS_TRACE();

		this->maxEntries = options->maxEntries();
		this->threshold = options->threshold();
		this->interval = options->interval();

		if (this->interval < 250)
			this->interval = 250;
		else if (this->interval > 5000)
			this->interval = 5000;

		this->periodicTimer = new Timer(this);

		this->periodicTimer->Start(this->interval, this->interval);

		// NOTE: This may throw.
		ChannelMessageHandlers::RegisterHandler(
		  this->id,
		  /*channelRequestHandler*/ this,
		  /*payloadChannelRequestHandler*/ nullptr,
		  /*payloadChannelNotificationHandler*/ nullptr);
	}

	AudioLevelObserver::~AudioLevelObserver()
	{
		MS_TRACE();

		ChannelMessageHandlers::UnregisterHandler(this->id);

		delete this->periodicTimer;
	}

	void AudioLevelObserver::AddProducer(RTC::Producer* producer)
	{
		MS_TRACE();

		if (producer->GetKind() != RTC::Media::Kind::AUDIO)
			MS_THROW_TYPE_ERROR("not an audio Producer");

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
			return;

		uint8_t volume;
		bool voice;

		if (!packet->ReadSsrcAudioLevel(volume, voice))
			return;

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

			Channel::ChannelNotifier::Emit(this->id, "silence");
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

		absl::btree_map<int8_t, RTC::Producer*> mapDBovsProducer;

		for (auto& kv : this->mapProducerDBovs)
		{
			auto* producer = kv.first;
			auto& dBovs    = kv.second;

			if (dBovs.count < 10)
				continue;

			auto avgDBov = -1 * static_cast<int8_t>(std::lround(dBovs.totalSum / dBovs.count));

			if (avgDBov >= this->threshold)
				mapDBovsProducer[avgDBov] = producer;
		}

		// Clear the map.
		ResetMapProducerDBovs();

		if (!mapDBovsProducer.empty())
		{
			this->silence = false;

			uint16_t idx{ 0 };
			auto rit  = mapDBovsProducer.crbegin();
			json data = json::array();

			for (; idx < this->maxEntries && rit != mapDBovsProducer.crend(); ++idx, ++rit)
			{
				data.emplace_back(json::value_t::object);

				auto& jsonEntry = data[idx];

				jsonEntry["producerId"] = rit->second->id;
				jsonEntry["volume"]     = rit->first;
			}

			Channel::ChannelNotifier::Emit(this->id, "volumes", data);
		}
		else if (!this->silence)
		{
			this->silence = true;

			Channel::ChannelNotifier::Emit(this->id, "silence");
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

	inline void AudioLevelObserver::OnTimer(Timer* /*timer*/)
	{
		MS_TRACE();

		Update();
	}
} // namespace RTC
