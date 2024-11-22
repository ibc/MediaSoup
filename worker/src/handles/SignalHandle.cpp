#define MS_CLASS "SignalHandle"
// #define MS_LOG_DEV_LEVEL 3

#include "handles/SignalHandle.hpp"
#include "DepLibUV.hpp"
#include "Logger.hpp"
#include "MediaSoupErrors.hpp"

/* Static methods for UV callbacks. */

inline static void onSignal(uv_signal_t* handle, int signum)
{
	static_cast<SignalHandle*>(handle->data)->OnUvSignal(signum);
}

inline static void onCloseSignal(uv_handle_t* handle)
{
	delete reinterpret_cast<uv_signal_t*>(handle);
}

/* Instance methods. */

SignalHandle::SignalHandle(Listener* listener) : listener(listener)
{
	MS_TRACE();
}

SignalHandle::~SignalHandle()
{
	MS_TRACE();

	if (!this->closed)
	{
		InternalClose();
	}
}

void SignalHandle::AddSignal(int signum, const std::string& name)
{
	MS_TRACE();

	if (this->closed)
	{
		MS_THROW_ERROR("closed");
	}

	int err;
	auto* uvHandle = new uv_signal_t;

	uvHandle->data = static_cast<void*>(this);

	err = uv_signal_init(DepLibUV::GetLoop(), uvHandle);

	if (err != 0)
	{
		delete uvHandle;

		MS_THROW_ERROR("uv_signal_init() failed for signal %s: %s", name.c_str(), uv_strerror(err));
	}

	err = uv_signal_start(uvHandle, static_cast<uv_signal_cb>(onSignal), signum);

	if (err != 0)
	{
		MS_THROW_ERROR("uv_signal_start() failed for signal %s: %s", name.c_str(), uv_strerror(err));
	}

	// Enter the UV handle into the vector.
	this->uvHandles.push_back(uvHandle);
}

void SignalHandle::InternalClose()
{
	MS_TRACE();

	if (this->closed)
	{
		return;
	}

	this->closed = true;

	for (auto* uvHandle : this->uvHandles)
	{
		uv_close(reinterpret_cast<uv_handle_t*>(uvHandle), static_cast<uv_close_cb>(onCloseSignal));
	}
}

void SignalHandle::OnUvSignal(int signum)
{
	MS_TRACE();

	// Notify the listener.
	this->listener->OnSignal(this, signum);
}
