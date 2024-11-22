#ifndef MS_SIGNAL_HANDLE_HPP
#define MS_SIGNAL_HANDLE_HPP

#include <uv.h>
#include <string>
#include <vector>

class SignalHandle
{
public:
	class Listener
	{
	public:
		virtual ~Listener() = default;

	public:
		virtual void OnSignal(SignalHandle* signalsHandler, int signum) = 0;
	};

public:
	explicit SignalHandle(Listener* listener);
	~SignalHandle();

public:
	void AddSignal(int signum, const std::string& name);

private:
	void InternalClose();

	/* Callbacks fired by UV events. */
public:
	void OnUvSignal(int signum);

private:
	// Passed by argument.
	Listener* listener{ nullptr };
	// Allocated by this.
	std::vector<uv_signal_t*> uvHandles;
	// Others.
	bool closed{ false };
};

#endif
