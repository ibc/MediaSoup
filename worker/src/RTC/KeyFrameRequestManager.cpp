#define MS_CLASS "KeyFrameRequestManager"

#include "RTC/KeyFrameRequestManager.hpp"
#include "DepLibUV.hpp"
#include "Logger.hpp"

static uint16_t KeyFrameWaitTime{ 2000 };

/* PendingKeyFrameInfo methods */

RTC::PendingKeyFrameInfo::PendingKeyFrameInfo(PendingKeyFrameInfo::Listener* listener, uint32_t ssrc)
  : listener(listener), ssrc(ssrc)
{
	MS_TRACE();

	this->timer = new Timer(this);
	this->timer->Start(KeyFrameWaitTime);
}

RTC::PendingKeyFrameInfo::~PendingKeyFrameInfo()
{
	MS_TRACE();

	this->timer->Stop();
	delete this->timer;
}

void RTC::PendingKeyFrameInfo::OnTimer(Timer* timer)
{
	MS_TRACE();

	if (timer == this->timer)
		this->listener->OnKeyFrameRequestTimeout(this);
}

/* KeyFrameRequestManager methods */

RTC::KeyFrameRequestManager::KeyFrameRequestManager(KeyFrameRequestManager::Listener* listener)
  : listener(listener)
{
	MS_TRACE();
}

RTC::KeyFrameRequestManager::~KeyFrameRequestManager()
{
	MS_TRACE();

	for (auto it : this->mapSsrcPendingKeyFrameInfo)
	{
		auto* pendingKeyFrameInfo = it.second;

		delete pendingKeyFrameInfo;
	}

	this->mapSsrcPendingKeyFrameInfo.clear();
}

void RTC::KeyFrameRequestManager::KeyFrameNeeded(uint32_t ssrc)
{
	MS_TRACE();

	auto it = this->mapSsrcPendingKeyFrameInfo.find(ssrc);

	// There is a pending key frame for the given ssrc.
	if (it != this->mapSsrcPendingKeyFrameInfo.end())
		return;

	this->mapSsrcPendingKeyFrameInfo[ssrc] = new PendingKeyFrameInfo(this, ssrc);

	this->listener->OnKeyFrameNeeded(this, ssrc);
}

void RTC::KeyFrameRequestManager::ForceKeyFrameNeeded(uint32_t ssrc)
{
	MS_TRACE();

	auto it = this->mapSsrcPendingKeyFrameInfo.find(ssrc);

	// There is a pending key frame for the given ssrc.
	if (it != this->mapSsrcPendingKeyFrameInfo.end())
	{
		auto* pendingKeyFrameInfo = it->second;
		delete pendingKeyFrameInfo;

		this->mapSsrcPendingKeyFrameInfo.erase(it);
	}

	this->mapSsrcPendingKeyFrameInfo[ssrc] = new PendingKeyFrameInfo(this, ssrc);

	this->listener->OnKeyFrameNeeded(this, ssrc);
}

void RTC::KeyFrameRequestManager::KeyFrameReceived(uint32_t ssrc)
{
	MS_TRACE();

	auto it = this->mapSsrcPendingKeyFrameInfo.find(ssrc);

	// There is no pending key frame for the given ssrc.
	if (it == this->mapSsrcPendingKeyFrameInfo.end())
		return;

	auto* pendingKeyFrameInfo = it->second;
	delete pendingKeyFrameInfo;

	this->mapSsrcPendingKeyFrameInfo.erase(it);
}

void RTC::KeyFrameRequestManager::OnKeyFrameRequestTimeout(PendingKeyFrameInfo* pendingKeyFrameInfo)
{
	MS_TRACE();

	auto it = this->mapSsrcPendingKeyFrameInfo.find(pendingKeyFrameInfo->GetSsrc());

	MS_ASSERT(it != this->mapSsrcPendingKeyFrameInfo.end(), "PendingKeyFrameInfo not present in the map")

	auto ssrc = pendingKeyFrameInfo->GetSsrc();

	delete pendingKeyFrameInfo;

	this->mapSsrcPendingKeyFrameInfo.erase(it);

	// Best effort in case the PLI/FIR was lost.
	this->listener->OnKeyFrameNeeded(this, ssrc);
}
