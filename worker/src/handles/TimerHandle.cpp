#define MS_CLASS "TimerHandle"
// #define MS_LOG_DEV_LEVEL 3

#include "handles/TimerHandle.hpp"
#include "DepLibUV.hpp"
#include "Logger.hpp"
#include "MediaSoupErrors.hpp"

/* Static methods for UV callbacks. */

inline static void onTimer(uv_timer_t* handle)
{
	static_cast<TimerHandle*>(handle->data)->OnUvTimer();
}

inline static void onCloseTimer(uv_handle_t* handle)
{
	delete reinterpret_cast<uv_timer_t*>(handle);
}

/* Instance methods. */

TimerHandle::TimerHandle(Listener* listener) : listener(listener), uvHandle(new uv_timer_t)
{
	MS_TRACE();

	this->uvHandle->data = static_cast<void*>(this);

	const int err = uv_timer_init(DepLibUV::GetLoop(), this->uvHandle);

	if (err != 0)
	{
		delete this->uvHandle;
		this->uvHandle = nullptr;

		MS_THROW_ERROR("uv_timer_init() failed: %s", uv_strerror(err));
	}
}

TimerHandle::~TimerHandle()
{
	MS_TRACE();

	if (!this->closed)
	{
		InternalClose();
	}
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

	if (uv_is_active(reinterpret_cast<uv_handle_t*>(this->uvHandle)) != 0)
	{
		Stop();
	}

	const int err = uv_timer_start(this->uvHandle, static_cast<uv_timer_cb>(onTimer), timeout, repeat);

	if (err != 0)
	{
		MS_THROW_ERROR("uv_timer_start() failed: %s", uv_strerror(err));
	}
}

void TimerHandle::Stop()
{
	MS_TRACE();

	if (this->closed)
	{
		MS_THROW_ERROR("closed");
	}

	const int err = uv_timer_stop(this->uvHandle);

	if (err != 0)
	{
		MS_THROW_ERROR("uv_timer_stop() failed: %s", uv_strerror(err));
	}
}

void TimerHandle::Reset()
{
	MS_TRACE();

	if (this->closed)
	{
		MS_THROW_ERROR("closed");
	}

	if (uv_is_active(reinterpret_cast<uv_handle_t*>(this->uvHandle)) == 0)
	{
		return;
	}

	if (this->repeat == 0u)
	{
		return;
	}

	const int err =
	  uv_timer_start(this->uvHandle, static_cast<uv_timer_cb>(onTimer), this->repeat, this->repeat);

	if (err != 0)
	{
		MS_THROW_ERROR("uv_timer_start() failed: %s", uv_strerror(err));
	}
}

void TimerHandle::Restart()
{
	MS_TRACE();

	if (this->closed)
	{
		MS_THROW_ERROR("closed");
	}

	if (uv_is_active(reinterpret_cast<uv_handle_t*>(this->uvHandle)) != 0)
	{
		Stop();
	}

	const int err =
	  uv_timer_start(this->uvHandle, static_cast<uv_timer_cb>(onTimer), this->timeout, this->repeat);

	if (err != 0)
	{
		MS_THROW_ERROR("uv_timer_start() failed: %s", uv_strerror(err));
	}
}

void TimerHandle::InternalClose()
{
	MS_TRACE();

	if (this->closed)
	{
		return;
	}

	this->closed = true;

	uv_close(reinterpret_cast<uv_handle_t*>(this->uvHandle), static_cast<uv_close_cb>(onCloseTimer));
}

inline void TimerHandle::OnUvTimer()
{
	MS_TRACE();

	// Notify the listener.
	this->listener->OnTimer(this);
}
