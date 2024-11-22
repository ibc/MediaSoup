#ifndef MS_TIMER_HANDLE_HPP
#define MS_TIMER_HANDLE_HPP

#include "common.hpp"
#include <uv.h>

class TimerHandle
{
public:
	class Listener
	{
	public:
		virtual ~Listener() = default;

	public:
		virtual void OnTimer(TimerHandle* timer) = 0;
	};

public:
	explicit TimerHandle(Listener* listener);
	TimerHandle& operator=(const TimerHandle&) = delete;
	TimerHandle(const TimerHandle&)            = delete;
	~TimerHandle();

public:
	void Start(uint64_t timeout, uint64_t repeat = 0);
	void Stop();
	void Reset();
	void Restart();
	uint64_t GetTimeout() const
	{
		return this->timeout;
	}
	uint64_t GetRepeat() const
	{
		return this->repeat;
	}
	bool IsActive() const
	{
		return uv_is_active(reinterpret_cast<uv_handle_t*>(this->uvHandle)) != 0;
	}

private:
	void InternalClose();

	/* Callbacks fired by UV events. */
public:
	void OnUvTimer();

private:
	// Passed by argument.
	Listener* listener{ nullptr };
	// Allocated by this.
	uv_timer_t* uvHandle{ nullptr };
	// Others.
	bool closed{ false };
	uint64_t timeout{ 0u };
	uint64_t repeat{ 0u };
};

#endif
