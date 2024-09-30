#define MS_CLASS "TimerHandle"
// #define MS_LOG_DEV_LEVEL 3

#include "handles/TimerHandle.hpp"
#include "DepLibUV.hpp"
#include "Logger.hpp"
#include "MediaSoupErrors.hpp"

/* Instance methods. */

TimerHandle::TimerHandle(Listener* listener) : listener(listener)
{
	MS_TRACE();
}

TimerHandle::~TimerHandle()
{
	MS_TRACE();

	if (!this->closed)
	{
		Close();
	}
}

thread_local TimerHandle::TimerStart TimerHandle::timer_start_;
thread_local TimerHandle::TimerStop TimerHandle::timer_stop_;

void TimerHandle::DoStop() {
	if (this->alarm_id != 0u)
	{
		if (!timer_stop_(this->alarm_id)) {
			MS_THROW_ERROR("timer_stop_ failed:");
		}
		this->alarm_id = 0;
	}
}

void TimerHandle::DoStart() {
	if (this->alarm_id == 0u) {
		alarm_id = timer_start_([this]() {
			// set alarm_id to 0 to prevent Stop() which is called in OnTimer() from calling AlarmProcessor::Cancal(),
			// which causes assertion in TimerScheduler::Stop(). the alarm will be canceled by return value (return 0)
			if (this->repeat == 0u) {
				this->alarm_id = 0;
			}
			// this may free in OnTimer (e.g. KeyFrameRequestManager::OnKeyFrameDelayTimeout)
			auto repeat = this->repeat;
			this->listener->OnTimer(this);
			return repeat;
		}, timeout);

		if (alarm_id == 0u)
		{
			MS_THROW_ERROR("timer_start_() failed:");
		}
	}
}

void TimerHandle::Close()
{
	MS_TRACE();

	if (this->closed)
	{
		return;
	}

	this->closed = true;

	DoStop();
}

void TimerHandle::Start(uint64_t timeout, uint64_t repeat)
{
	MS_TRACE();

	if (this->closed)
	{
		MS_THROW_ERROR("closed");
	}

	this->timeout = timeout;
	this->repeat  = repeat;

	DoStop();
	DoStart();
}

void TimerHandle::Stop()
{
	MS_TRACE();

	if (this->closed)
	{
		MS_THROW_ERROR("closed");
	}

	DoStop();
}

void TimerHandle::Reset()
{
	MS_TRACE();

	if (this->closed)
	{
		MS_THROW_ERROR("closed");
	}

	if (this->repeat == 0u)
	{
		return;
	}

	DoStart();
}

void TimerHandle::Restart()
{
	MS_TRACE();

	if (this->closed)
	{
		MS_THROW_ERROR("closed");
	}

	DoStop();
	DoStart();
}
