/**
 * NOTE: The UsrSctpChecker singleton must run in its own thread so it cannot
 * use Logger.hpp since Logger communicates via UnixSocket with Node or Rust
 * and there must be a single thread writing into that socket in worker side.
 */

#include "SCTP/UsrSctpChecker.hpp"
#include "DepLibUV.hpp"
#include <usrsctp.h>

namespace SCTP
{
	/* Static. */

	static constexpr size_t CheckerInterval{ 10u }; // In ms.

	/* UsrSctpChecker instance methods. */

	UsrSctpChecker::UsrSctpChecker()
	{
		this->timer = new TimerHandle(this);

		DepLibUV::RunLoop();
	}

	UsrSctpChecker::~UsrSctpChecker()
	{
		delete this->timer;
	}

	void UsrSctpChecker::Start()
	{
		this->lastCalledAtMs = 0u;

		this->timer->Start(CheckerInterval, CheckerInterval);
	}

	void UsrSctpChecker::Stop()
	{
		this->lastCalledAtMs = 0u;

		this->timer->Stop();
	}

	void UsrSctpChecker::OnTimer(TimerHandle* /*timer*/)
	{
		auto nowMs          = DepLibUV::GetTimeMs();
		const int elapsedMs = this->lastCalledAtMs ? static_cast<int>(nowMs - this->lastCalledAtMs) : 0;

		// TODO: This must run in the worker thread obviously. How3ver this is not easy
		// at all. Note that usrsctp_handle_timers() may trigger many calls to the
		// usrsctp onSendSctpData() callback, but each of those call may be intended
		// for a SctpAssociation in whatever other worker/thread, so we cannot just
		// try to group all them together.
		// #ifdef MS_LIBURING_SUPPORTED
		// Activate liburing usage.
		// 'usrsctp_handle_timers()' will synchronously call the send/recv
		// callbacks for the pending data. If there are multiple messages to be
		// sent over the network then we will send those messages within a single
		// system call.
		// DepLibUring::SetActive();
		// #endif

		usrsctp_handle_timers(elapsedMs);

		// TODO: This must run in the worker thread obviously.
		// #ifdef MS_LIBURING_SUPPORTED
		// Submit all prepared submission entries.
		// DepLibUring::Submit();
		// #endif

		this->lastCalledAtMs = nowMs;
	}
} // namespace SCTP
