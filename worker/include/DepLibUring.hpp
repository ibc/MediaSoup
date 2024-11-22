#ifndef MS_DEP_LIBURING_HPP
#define MS_DEP_LIBURING_HPP

#include "DepLibUV.hpp"
#include "FBS/liburing.h"
#include <functional>
#include <liburing.h>
#include <queue>

class DepLibUring
{
public:
	using onSendCallback = const std::function<void(bool sent)>;

	/* Struct for the user data field of SQE and CQE. */
	struct UserData
	{
		// Pointer to send buffer.
		uint8_t* store{ nullptr };
		// Frame len buffer for TCP.
		uint8_t frameLen[2] = { 0 };
		// iovec for TCP, first item for framing, second item for payload.
		struct iovec iov[2];
		// Send callback.
		onSendCallback* cb{ nullptr };
		// Index in userDatas array.
		size_t idx{ 0 };
	};

	/* Number of submission queue entries (SQE). */
	static constexpr size_t QueueDepth{ 1024 * 4 };
	static constexpr size_t SendBufferSize{ 1500 };

	using SendBuffer = uint8_t[SendBufferSize];

	static void ClassInit();
	static void ClassDestroy();
	static bool CheckRuntimeSupport();
	static bool IsEnabled();
	static flatbuffers::Offset<FBS::LibUring::Dump> FillBuffer(flatbuffers::FlatBufferBuilder& builder);
	static void StartPollingCQEs();
	static void StopPollingCQEs();
	static uint8_t* GetSendBuffer();
	static bool PrepareSend(
	  int sockfd, const uint8_t* data, size_t len, const struct sockaddr* addr, onSendCallback* cb);
	static bool PrepareWrite(
	  int sockfd, const uint8_t* data1, size_t len1, const uint8_t* data2, size_t len2, onSendCallback* cb);
	static void Submit();
	static void SetActive();
	static bool IsActive();

	class LibUring;

	// Whether liburing is enabled or not after runtime checks.
	thread_local static bool enabled;
	thread_local static LibUring* liburing;

public:
	// Singleton.
	class LibUring
	{
	public:
		LibUring();
		~LibUring();
		flatbuffers::Offset<FBS::LibUring::Dump> FillBuffer(flatbuffers::FlatBufferBuilder& builder) const;
		void StartPollingCQEs();
		void StopPollingCQEs();
		uint8_t* GetSendBuffer();
		bool PrepareSend(
		  int sockfd, const uint8_t* data, size_t len, const struct sockaddr* addr, onSendCallback* cb);
		bool PrepareWrite(
		  int sockfd,
		  const uint8_t* data1,
		  size_t len1,
		  const uint8_t* data2,
		  size_t len2,
		  onSendCallback* cb);
		void Submit();
		void SetActive()
		{
			this->active = true;
		}
		bool IsActive() const
		{
			return this->active;
		}
		bool IsZeroCopyEnabled() const
		{
			return this->zeroCopyEnabled;
		}
		io_uring* GetRing()
		{
			return std::addressof(this->ring);
		}
		int GetEventFd() const
		{
			return this->efd;
		}
		void ReleaseUserDataEntry(size_t idx)
		{
			this->availableUserDataEntries.push(idx);
		}

	private:
		void SetInactive()
		{
			this->active = false;
		}
		UserData* GetUserData();
		bool IsDataInSendBuffers(const uint8_t* data) const
		{
			return data >= this->sendBuffers[0] && data <= this->sendBuffers[DepLibUring::QueueDepth - 1];
		}

	private:
		// io_uring instance.
		io_uring ring;
		// Event file descriptor to watch for io_uring completions.
		int efd;
		// libuv handle used to poll io_uring completions.
		uv_poll_t* uvHandle{ nullptr };
		// Whether we are currently sending RTP over io_uring.
		bool active{ false };
		// Whether Zero Copy feature is enabled.
		bool zeroCopyEnabled{ true };
		// Pre-allocated UserData's.
		UserData userDatas[QueueDepth]{};
		// Indexes of available UserData entries.
		std::queue<size_t> availableUserDataEntries;
		// Pre-allocated SendBuffer's.
		SendBuffer sendBuffers[QueueDepth];
		// iovec structs to be registered for Zero Copy.
		struct iovec iovecs[QueueDepth];
		// Submission queue entry process count.
		uint64_t sqeProcessCount{ 0u };
		// Submission queue entry miss count.
		uint64_t sqeMissCount{ 0u };
		// User data miss count.
		uint64_t userDataMissCount{ 0u };
	};
};

#endif
