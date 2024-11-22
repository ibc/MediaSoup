#ifndef MS_KEY_FRAME_REQUEST_MANAGER_HPP
#define MS_KEY_FRAME_REQUEST_MANAGER_HPP

#include "handles/TimerHandle.hpp"
#include <absl/container/flat_hash_map.h>

namespace RTC
{
	class PendingKeyFrameInfo : public TimerHandle::Listener
	{
	public:
		class Listener
		{
		public:
			virtual ~Listener() = default;

		public:
			virtual void OnKeyFrameRequestTimeout(PendingKeyFrameInfo* keyFrameRequestInfo) = 0;
		};

	public:
		PendingKeyFrameInfo(Listener* listener, uint32_t ssrc);
		~PendingKeyFrameInfo() override;

		uint32_t GetSsrc() const
		{
			return this->ssrc;
		}
		void SetRetryOnTimeout(bool notify)
		{
			this->retryOnTimeout = notify;
		}
		bool GetRetryOnTimeout() const
		{
			return this->retryOnTimeout;
		}
		void Restart()
		{
			return this->timer->Restart();
		}

		/* Pure virtual methods inherited from TimerHandle::Listener. */
	public:
		void OnTimer(TimerHandle* timer) override;

	private:
		Listener* listener{ nullptr };
		uint32_t ssrc;
		TimerHandle* timer{ nullptr };
		bool retryOnTimeout{ true };
	};

	class KeyFrameRequestDelayer : public TimerHandle::Listener
	{
	public:
		class Listener
		{
		public:
			virtual ~Listener() = default;

		public:
			virtual void OnKeyFrameDelayTimeout(KeyFrameRequestDelayer* keyFrameRequestDelayer) = 0;
		};

	public:
		KeyFrameRequestDelayer(Listener* listener, uint32_t ssrc, uint32_t delay);
		~KeyFrameRequestDelayer() override;

		uint32_t GetSsrc() const
		{
			return this->ssrc;
		}
		bool GetKeyFrameRequested() const
		{
			return this->keyFrameRequested;
		}
		void SetKeyFrameRequested(bool flag)
		{
			this->keyFrameRequested = flag;
		}

		/* Pure virtual methods inherited from TimerHandle::Listener. */
	public:
		void OnTimer(TimerHandle* timer) override;

	private:
		Listener* listener{ nullptr };
		uint32_t ssrc;
		TimerHandle* timer{ nullptr };
		bool keyFrameRequested{ false };
	};

	class KeyFrameRequestManager : public PendingKeyFrameInfo::Listener,
	                               public KeyFrameRequestDelayer::Listener
	{
	public:
		class Listener
		{
		public:
			virtual ~Listener() = default;

		public:
			virtual void OnKeyFrameNeeded(KeyFrameRequestManager* keyFrameRequestManager, uint32_t ssrc) = 0;
		};

	public:
		explicit KeyFrameRequestManager(Listener* listener, uint32_t keyFrameRequestDelay);
		~KeyFrameRequestManager() override;

		void KeyFrameNeeded(uint32_t ssrc);
		void ForceKeyFrameNeeded(uint32_t ssrc);
		void KeyFrameReceived(uint32_t ssrc);

		/* Pure virtual methods inherited from PendingKeyFrameInfo::Listener. */
	public:
		void OnKeyFrameRequestTimeout(PendingKeyFrameInfo* pendingKeyFrameInfo) override;

		/* Pure virtual methods inherited from PendingKeyFrameInfo::Listener. */
	public:
		void OnKeyFrameDelayTimeout(KeyFrameRequestDelayer* keyFrameRequestDelayer) override;

	private:
		Listener* listener{ nullptr };
		uint32_t keyFrameRequestDelay{ 0u }; // 0 means disabled.
		absl::flat_hash_map<uint32_t, PendingKeyFrameInfo*> mapSsrcPendingKeyFrameInfo;
		absl::flat_hash_map<uint32_t, KeyFrameRequestDelayer*> mapSsrcKeyFrameRequestDelayer;
	};
} // namespace RTC

#endif
