#define MS_CLASS "KeyFrameRequestManager"
// #define MS_LOG_DEV_LEVEL 3

#include "RTC/KeyFrameRequestManager.hpp"
#include "Logger.hpp"

static constexpr uint32_t KeyFrameRetransmissionWaitTime{ 1000u };

/* PendingKeyFrameInfo methods. */

RTC::PendingKeyFrameInfo::PendingKeyFrameInfo(PendingKeyFrameInfo::Listener* listener, uint32_t ssrc)
  : listener(listener), ssrc(ssrc), timer(new TimerHandle(this))
{
	MS_TRACE();

	this->timer->Start(KeyFrameRetransmissionWaitTime);
}

RTC::PendingKeyFrameInfo::~PendingKeyFrameInfo()
{
	MS_TRACE();

	this->timer->Stop();
	delete this->timer;
}

inline void RTC::PendingKeyFrameInfo::OnTimer(TimerHandle* timer)
{
	MS_TRACE();

	if (timer == this->timer)
	{
		this->listener->OnKeyFrameRequestTimeout(this);
	}
}

/* KeyFrameRequestDelayer methods. */

RTC::KeyFrameRequestDelayer::KeyFrameRequestDelayer(
  KeyFrameRequestDelayer::Listener* listener, uint32_t ssrc, uint32_t delay)
  : listener(listener), ssrc(ssrc), timer(new TimerHandle(this))
{
	MS_TRACE();

	this->timer->Start(delay);
}

RTC::KeyFrameRequestDelayer::~KeyFrameRequestDelayer()
{
	MS_TRACE();

	this->timer->Stop();
	delete this->timer;
}

inline void RTC::KeyFrameRequestDelayer::OnTimer(TimerHandle* timer)
{
	MS_TRACE();

	if (timer == this->timer)
	{
		this->listener->OnKeyFrameDelayTimeout(this);
	}
}

/* KeyFrameRequestManager methods. */

RTC::KeyFrameRequestManager::KeyFrameRequestManager(
  KeyFrameRequestManager::Listener* listener, uint32_t keyFrameRequestDelay)
  : listener(listener), keyFrameRequestDelay(keyFrameRequestDelay)
{
	MS_TRACE();
}

RTC::KeyFrameRequestManager::~KeyFrameRequestManager()
{
	MS_TRACE();

	for (auto& kv : this->mapSsrcPendingKeyFrameInfo)
	{
		auto* pendingKeyFrameInfo = kv.second;

		delete pendingKeyFrameInfo;
	}
	this->mapSsrcPendingKeyFrameInfo.clear();

	for (auto& kv : this->mapSsrcKeyFrameRequestDelayer)
	{
		auto* keyFrameRequestDelayer = kv.second;

		delete keyFrameRequestDelayer;
	}
	this->mapSsrcKeyFrameRequestDelayer.clear();
}

void RTC::KeyFrameRequestManager::KeyFrameNeeded(uint32_t ssrc)
{
	MS_TRACE();

	if (this->keyFrameRequestDelay > 0u)
	{
		auto it = this->mapSsrcKeyFrameRequestDelayer.find(ssrc);

		// There is a delayer for the given ssrc, so enable it and return.
		if (it != this->mapSsrcKeyFrameRequestDelayer.end())
		{
			MS_DEBUG_DEV("there is a delayer for the given ssrc, enabling it and returning");

			auto* keyFrameRequestDelayer = it->second;

			keyFrameRequestDelayer->SetKeyFrameRequested(true);

			return;
		}
		// Otherwise create a delayer (not yet enabled) and continue.
		else
		{
			MS_DEBUG_DEV("creating a delayer for the given ssrc");

			this->mapSsrcKeyFrameRequestDelayer[ssrc] =
			  new KeyFrameRequestDelayer(this, ssrc, this->keyFrameRequestDelay);
		}
	}

	auto it = this->mapSsrcPendingKeyFrameInfo.find(ssrc);

	// There is a pending key frame for the given ssrc.
	if (it != this->mapSsrcPendingKeyFrameInfo.end())
	{
		auto* pendingKeyFrameInfo = it->second;

		// Re-request the key frame if not received on time.
		pendingKeyFrameInfo->SetRetryOnTimeout(true);

		return;
	}

	this->mapSsrcPendingKeyFrameInfo[ssrc] = new PendingKeyFrameInfo(this, ssrc);

	this->listener->OnKeyFrameNeeded(this, ssrc);
}

void RTC::KeyFrameRequestManager::ForceKeyFrameNeeded(uint32_t ssrc)
{
	MS_TRACE();

	if (this->keyFrameRequestDelay > 0u)
	{
		// Create/replace a delayer for this ssrc.
		auto it = this->mapSsrcKeyFrameRequestDelayer.find(ssrc);

		// There is a delayer for the given ssrc, so enable it and return.
		if (it != this->mapSsrcKeyFrameRequestDelayer.end())
		{
			auto* keyFrameRequestDelayer = it->second;

			delete keyFrameRequestDelayer;
		}

		this->mapSsrcKeyFrameRequestDelayer[ssrc] =
		  new KeyFrameRequestDelayer(this, ssrc, this->keyFrameRequestDelay);
	}

	auto it = this->mapSsrcPendingKeyFrameInfo.find(ssrc);

	// There is a pending key frame for the given ssrc.
	if (it != this->mapSsrcPendingKeyFrameInfo.end())
	{
		auto* pendingKeyFrameInfo = it->second;

		pendingKeyFrameInfo->SetRetryOnTimeout(true);
		pendingKeyFrameInfo->Restart();
	}
	else
	{
		this->mapSsrcPendingKeyFrameInfo[ssrc] = new PendingKeyFrameInfo(this, ssrc);
	}

	this->listener->OnKeyFrameNeeded(this, ssrc);
}

void RTC::KeyFrameRequestManager::KeyFrameReceived(uint32_t ssrc)
{
	MS_TRACE();

	auto it = this->mapSsrcPendingKeyFrameInfo.find(ssrc);

	// There is no pending key frame for the given ssrc.
	if (it == this->mapSsrcPendingKeyFrameInfo.end())
	{
		return;
	}

	auto* pendingKeyFrameInfo = it->second;

	delete pendingKeyFrameInfo;

	this->mapSsrcPendingKeyFrameInfo.erase(it);
}

inline void RTC::KeyFrameRequestManager::OnKeyFrameRequestTimeout(PendingKeyFrameInfo* pendingKeyFrameInfo)
{
	MS_TRACE();

	auto it = this->mapSsrcPendingKeyFrameInfo.find(pendingKeyFrameInfo->GetSsrc());

	MS_ASSERT(
	  it != this->mapSsrcPendingKeyFrameInfo.end(), "PendingKeyFrameInfo not present in the map");

	if (!pendingKeyFrameInfo->GetRetryOnTimeout())
	{
		delete pendingKeyFrameInfo;

		this->mapSsrcPendingKeyFrameInfo.erase(it);

		return;
	}

	// Best effort in case the PLI/FIR was lost. Do not retry on timeout.
	pendingKeyFrameInfo->SetRetryOnTimeout(false);
	pendingKeyFrameInfo->Restart();

	MS_DEBUG_DEV("requesting key frame on timeout");

	this->listener->OnKeyFrameNeeded(this, pendingKeyFrameInfo->GetSsrc());
}

inline void RTC::KeyFrameRequestManager::OnKeyFrameDelayTimeout(
  KeyFrameRequestDelayer* keyFrameRequestDelayer)
{
	MS_TRACE();

	auto it = this->mapSsrcKeyFrameRequestDelayer.find(keyFrameRequestDelayer->GetSsrc());

	MS_ASSERT(
	  it != this->mapSsrcKeyFrameRequestDelayer.end(), "KeyFrameRequestDelayer not present in the map");

	auto ssrc              = keyFrameRequestDelayer->GetSsrc();
	auto keyFrameRequested = keyFrameRequestDelayer->GetKeyFrameRequested();

	delete keyFrameRequestDelayer;

	this->mapSsrcKeyFrameRequestDelayer.erase(it);

	// Ask for a new key frame as normal if needed.
	if (keyFrameRequested)
	{
		MS_DEBUG_DEV("requesting key frame after delay timeout");

		KeyFrameNeeded(ssrc);
	}
}
