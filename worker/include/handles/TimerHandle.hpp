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
	void Close();
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
		return alarm_id != 0;
	}

public:
	typedef std::function<uint64_t ()> Handler;
	typedef std::function<uint64_t(const Handler&, uint64_t)> TimerStart;
	typedef std::function<bool(uint64_t)> TimerStop;
	static void SetTimerProc(TimerStart start, TimerStop stop) {
		timer_start_ = start;
		timer_stop_ = stop;
	}

private:
	void DoStart();
	void DoStop();
	static thread_local TimerStart timer_start_;
	static thread_local TimerStop timer_stop_;
	// Passed by argument.
	Listener* listener{ nullptr };
	// Others.
	uint64_t alarm_id{ 0u };
	bool closed{ false };
	uint64_t timeout{ 0u };
	uint64_t repeat{ 0u };
};

#endif
