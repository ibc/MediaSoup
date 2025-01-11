#ifndef MS_SCTP_USRSCTP_CHECKER_HPP
#define MS_SCTP_USRSCTP_CHECKER_HPP

#include "common.hpp"
#include "handles/TimerHandle.hpp"
#include <uv.h>

namespace SCTP
{
	class UsrSctpChecker : public TimerHandle::Listener
	{
	public:
		UsrSctpChecker();
		~UsrSctpChecker() override;

	public:
		void Start();
		void Stop();

		/* Pure virtual methods inherited from TimerHandle::Listener. */
	public:
		void OnTimer(TimerHandle* timer) override;

	private:
		TimerHandle* timer{ nullptr };
		uint64_t lastCalledAtMs{ 0u };
	};
} // namespace SCTP

#endif
