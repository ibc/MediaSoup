#define MS_CLASS "DepLibUring"
// #define MS_LOG_DEV_LEVEL 3

#include "DepLibUring.hpp"
#include "Logger.hpp"
#include "MediaSoupErrors.hpp"
#include "Settings.hpp"
#include "Utils.hpp"
#include <sys/eventfd.h>
#include <sys/resource.h>
#include <sys/utsname.h>

/* Static variables. */
thread_local bool DepLibUring::enabled{ false };
// liburing instance per thread.
thread_local DepLibUring::LibUring* DepLibUring::liburing{ nullptr };
// Completion queue entry array used to retrieve processes tasks.
thread_local struct io_uring_cqe* cqes[DepLibUring::QueueDepth];

/* Static methods for UV callbacks. */

inline static void onCloseFd(uv_handle_t* handle)
{
	delete reinterpret_cast<uv_poll_t*>(handle);
}

inline static void onFdEvent(uv_poll_t* handle, int status, int events)
{
	auto* liburing = static_cast<DepLibUring::LibUring*>(handle->data);
	auto count     = io_uring_peek_batch_cqe(liburing->GetRing(), cqes, DepLibUring::QueueDepth);

	// libuv uses level triggering, so we need to read from the socket to reset
	// the counter in order to avoid libuv calling this callback indefinitely.
	eventfd_t v;
	int err = eventfd_read(liburing->GetEventFd(), std::addressof(v));

	if (err < 0)
	{
		// Get positive errno.
		int error = -err;

		MS_ABORT("eventfd_read() failed: %s", std::strerror(error));
	};

	for (unsigned int i{ 0 }; i < count; ++i)
	{
		struct io_uring_cqe* cqe = cqes[i];
		auto* userData           = static_cast<DepLibUring::UserData*>(io_uring_cqe_get_data(cqe));

		if (liburing->IsZeroCopyEnabled())
		{
			// CQE notification for a zero-copy submission.
			if (cqe->flags & IORING_CQE_F_NOTIF)
			{
				// The send buffer is now in the network card, run the send callback.
				if (userData->cb)
				{
					(*userData->cb)(true);
					delete userData->cb;
					userData->cb = nullptr;
				}

				liburing->ReleaseUserDataEntry(userData->idx);
				io_uring_cqe_seen(liburing->GetRing(), cqe);

				continue;
			}

			// CQE for a zero-copy submission, a CQE notification will follow.
			if (cqe->flags & IORING_CQE_F_MORE)
			{
				if (cqe->res < 0)
				{
					if (userData->cb)
					{
						(*userData->cb)(false);
						delete userData->cb;
						userData->cb = nullptr;
					}
				}

				// NOTE: Do not release the user data as it will be done upon reception
				// of CQE notification.
				io_uring_cqe_seen(liburing->GetRing(), cqe);

				continue;
			}
		}

		// Successfull SQE.
		if (cqe->res >= 0)
		{
			if (userData->cb)
			{
				(*userData->cb)(true);
				delete userData->cb;
				userData->cb = nullptr;
			}
		}
		// Failed SQE.
		else
		{
			if (userData->cb)
			{
				(*userData->cb)(false);
				delete userData->cb;
				userData->cb = nullptr;
			}
		}

		liburing->ReleaseUserDataEntry(userData->idx);
		io_uring_cqe_seen(liburing->GetRing(), cqe);
	}
}

/* Static class methods */

void DepLibUring::ClassInit()
{
	const auto mayor = io_uring_major_version();
	const auto minor = io_uring_minor_version();

	MS_DEBUG_TAG(info, "io_uring version: \"%i.%i\"", mayor, minor);

	if (Settings::configuration.liburingDisabled)
	{
		MS_DEBUG_TAG(info, "io_uring disabled by user settings");

		return;
	}

	// This must be called first.
	if (DepLibUring::CheckRuntimeSupport())
	{
		try
		{
			DepLibUring::liburing = new LibUring();

			MS_DEBUG_TAG(info, "io_uring enabled");

			DepLibUring::enabled = true;
		}
		catch (const MediaSoupError& error)
		{
			MS_DEBUG_TAG(info, "io_uring initialization failed, io_uring not enabled");
		}
	}
	else
	{
		MS_DEBUG_TAG(info, "io_uring not enabled");
	}
}

void DepLibUring::ClassDestroy()
{
	MS_TRACE();

	delete DepLibUring::liburing;
}

bool DepLibUring::CheckRuntimeSupport()
{
	// clang-format off
	struct utsname buffer{};
	// clang-format on

	auto err = uname(std::addressof(buffer));

	if (err != 0)
	{
		MS_THROW_ERROR("uname() failed: %s", std::strerror(err));
	}

	MS_DEBUG_TAG(info, "kernel version: %s", buffer.version);

	auto* kernelMayorCstr = buffer.release;
	auto kernelMayorLong  = strtol(kernelMayorCstr, &kernelMayorCstr, 10);

	// liburing `sento` capabilities are supported for kernel versions greather
	// than or equal to 6.
	if (kernelMayorLong < 6)
	{
		MS_DEBUG_TAG(info, "kernel doesn't support io_uring");

		return false;
	}

	return true;
}

bool DepLibUring::IsEnabled()
{
	return DepLibUring::enabled;
}

flatbuffers::Offset<FBS::LibUring::Dump> DepLibUring::FillBuffer(flatbuffers::FlatBufferBuilder& builder)
{
	MS_TRACE();

	MS_ASSERT(DepLibUring::enabled, "io_uring not enabled");

	return DepLibUring::liburing->FillBuffer(builder);
}

void DepLibUring::StartPollingCQEs()
{
	MS_TRACE();

	MS_ASSERT(DepLibUring::enabled, "io_uring not enabled");

	DepLibUring::liburing->StartPollingCQEs();
}

void DepLibUring::StopPollingCQEs()
{
	MS_TRACE();

	MS_ASSERT(DepLibUring::enabled, "io_uring not enabled");

	DepLibUring::liburing->StopPollingCQEs();
}

uint8_t* DepLibUring::GetSendBuffer()
{
	MS_TRACE();

	MS_ASSERT(DepLibUring::enabled, "io_uring not enabled");

	return DepLibUring::liburing->GetSendBuffer();
}

bool DepLibUring::PrepareSend(
  int sockfd, const uint8_t* data, size_t len, const struct sockaddr* addr, onSendCallback* cb)
{
	MS_TRACE();

	MS_ASSERT(DepLibUring::enabled, "io_uring not enabled");

	return DepLibUring::liburing->PrepareSend(sockfd, data, len, addr, cb);
}

bool DepLibUring::PrepareWrite(
  int sockfd, const uint8_t* data1, size_t len1, const uint8_t* data2, size_t len2, onSendCallback* cb)
{
	MS_TRACE();

	MS_ASSERT(DepLibUring::enabled, "io_uring not enabled");

	return DepLibUring::liburing->PrepareWrite(sockfd, data1, len1, data2, len2, cb);
}

void DepLibUring::Submit()
{
	MS_TRACE();

	MS_ASSERT(DepLibUring::enabled, "io_uring not enabled");

	DepLibUring::liburing->Submit();
}

void DepLibUring::SetActive()
{
	MS_TRACE();

	MS_ASSERT(DepLibUring::enabled, "io_uring not enabled");

	DepLibUring::liburing->SetActive();
}

bool DepLibUring::IsActive()
{
	MS_TRACE();

	MS_ASSERT(DepLibUring::enabled, "io_uring not enabled");

	return DepLibUring::liburing->IsActive();
}

/* Instance methods. */

DepLibUring::LibUring::LibUring()
{
	MS_TRACE();

	/**
	 * IORING_SETUP_SINGLE_ISSUER: A hint to the kernel that only a single task
	 * (or thread) will submit requests, which is used for internal optimisations.
	 */

	unsigned int flags = IORING_SETUP_SINGLE_ISSUER;

	// Initialize io_uring.
	auto err = io_uring_queue_init(DepLibUring::QueueDepth, std::addressof(this->ring), flags);

	if (err < 0)
	{
		// Get positive errno.
		int error = -err;

		MS_THROW_ERROR("io_uring_queue_init() failed: %s", std::strerror(error));
	}

	// Create an eventfd instance.
	this->efd = eventfd(0, 0);

	if (this->efd < 0)
	{
		MS_THROW_ERROR("eventfd() failed: %s", std::strerror(-this->efd));
	}

	err = io_uring_register_eventfd(std::addressof(this->ring), this->efd);

	if (err < 0)
	{
		// Get positive errno.
		int error = -err;

		MS_THROW_ERROR("io_uring_register_eventfd() failed: %s", std::strerror(error));
	}

	// Initialize available UserData entries.
	for (size_t i{ 0 }; i < DepLibUring::QueueDepth; ++i)
	{
		this->userDatas[i].store = this->sendBuffers[i];
		this->availableUserDataEntries.push(i);
	}

	// Initialize iovecs.
	for (size_t i{ 0 }; i < DepLibUring::QueueDepth; ++i)
	{
		this->iovecs[i].iov_base = this->sendBuffers[i];
		this->iovecs[i].iov_len  = DepLibUring::SendBufferSize;
	}

	err = io_uring_register_buffers(std::addressof(this->ring), this->iovecs, DepLibUring::QueueDepth);

	if (err < 0)
	{
		// Get positive errno.
		int error = -err;

		if (error == ENOMEM)
		{
			this->zeroCopyEnabled = false;

			struct rlimit l = {};

			if (getrlimit(RLIMIT_MEMLOCK, std::addressof(l)) == -1)
			{
				MS_WARN_TAG(info, "getrlimit() failed: %s", std::strerror(errno));
				MS_WARN_TAG(
				  info,
				  "io_uring_register_buffers() failed due to low RLIMIT_MEMLOCK, disabling zero copy: %s",
				  std::strerror(error));
			}
			else
			{
				MS_WARN_TAG(
				  info,
				  "io_uring_register_buffers() failed due to low RLIMIT_MEMLOCK (soft:%lu, hard:%lu), disabling zero copy: %s",
				  l.rlim_cur,
				  l.rlim_max,
				  std::strerror(error));
			}
		}
		else
		{
			MS_THROW_ERROR("io_uring_register_buffers() failed: %s", std::strerror(error));
		}
	}
}

DepLibUring::LibUring::~LibUring()
{
	MS_TRACE();

	// Close the event file descriptor.
	const auto err = close(this->efd);

	if (err != 0)
	{
		// Get positive errno.
		int error = -err;

		MS_ABORT("close() failed: %s", std::strerror(error));
	}

	// Close the ring.
	io_uring_queue_exit(std::addressof(this->ring));
}

flatbuffers::Offset<FBS::LibUring::Dump> DepLibUring::LibUring::FillBuffer(
  flatbuffers::FlatBufferBuilder& builder) const
{
	MS_TRACE();

	return FBS::LibUring::CreateDump(
	  builder, this->sqeProcessCount, this->sqeMissCount, this->userDataMissCount);
}

void DepLibUring::LibUring::StartPollingCQEs()
{
	MS_TRACE();

	// Watch the event file descriptor for completions.
	this->uvHandle = new uv_poll_t;

	auto err = uv_poll_init(DepLibUV::GetLoop(), this->uvHandle, this->efd);

	if (err != 0)
	{
		delete this->uvHandle;

		MS_THROW_ERROR("uv_poll_init() failed: %s", uv_strerror(err));
	}

	this->uvHandle->data = this;

	err = uv_poll_start(this->uvHandle, UV_READABLE, static_cast<uv_poll_cb>(onFdEvent));

	if (err != 0)
	{
		MS_THROW_ERROR("uv_poll_start() failed: %s", uv_strerror(err));
	}
}

void DepLibUring::LibUring::StopPollingCQEs()
{
	MS_TRACE();

	this->uvHandle->data = nullptr;

	// Stop polling the event file descriptor.
	auto err = uv_poll_stop(this->uvHandle);

	if (err != 0)
	{
		MS_ABORT("uv_poll_stop() failed: %s", uv_strerror(err));
	}

	// NOTE: Handles that wrap file descriptors are clossed immediately.
	uv_close(reinterpret_cast<uv_handle_t*>(this->uvHandle), static_cast<uv_close_cb>(onCloseFd));
}

uint8_t* DepLibUring::LibUring::GetSendBuffer()
{
	MS_TRACE();

	if (this->availableUserDataEntries.empty())
	{
		MS_DEBUG_DEV("no user data entry available");

		return nullptr;
	}

	auto idx = this->availableUserDataEntries.front();

	return this->userDatas[idx].store;
}

bool DepLibUring::LibUring::PrepareSend(
  int sockfd, const uint8_t* data, size_t len, const struct sockaddr* addr, onSendCallback* cb)
{
	MS_TRACE();

	auto* userData = this->GetUserData();

	if (!userData)
	{
		MS_DEBUG_DEV("no user data entry available");

		this->userDataMissCount++;

		return false;
	}

	auto* sqe = io_uring_get_sqe(std::addressof(this->ring));

	if (!sqe)
	{
		MS_DEBUG_DEV("no sqe available");

		this->sqeMissCount++;

		return false;
	}

	// The send data buffer belongs to us, no need to memcpy.
	if (this->IsDataInSendBuffers(data))
	{
		MS_ASSERT(data == userData->store, "send buffer does not match userData store");
	}
	else
	{
		std::memcpy(userData->store, data, len);
	}

	userData->cb = cb;

	io_uring_sqe_set_data(sqe, userData);

	socklen_t addrlen = Utils::IP::GetAddressLen(addr);

	if (this->zeroCopyEnabled)
	{
		auto iovec    = this->iovecs[userData->idx];
		iovec.iov_len = len;

		io_uring_prep_send_zc(sqe, sockfd, iovec.iov_base, iovec.iov_len, 0, 0);
		io_uring_prep_send_set_addr(sqe, addr, addrlen);

		// Tell io_uring that we are providing the already registered send buffer
		// for zero copy.
		sqe->ioprio |= IORING_RECVSEND_FIXED_BUF;
		sqe->buf_index = userData->idx;
	}
	else
	{
		io_uring_prep_sendto(sqe, sockfd, userData->store, len, 0, addr, addrlen);
	}

	this->sqeProcessCount++;

	return true;
}

bool DepLibUring::LibUring::PrepareWrite(
  int sockfd, const uint8_t* data1, size_t len1, const uint8_t* data2, size_t len2, onSendCallback* cb)
{
	MS_TRACE();

	auto* userData = this->GetUserData();

	if (!userData)
	{
		MS_DEBUG_DEV("no user data entry available");

		this->userDataMissCount++;

		return false;
	}

	auto* sqe = io_uring_get_sqe(std::addressof(this->ring));

	if (!sqe)
	{
		MS_DEBUG_DEV("no sqe available");

		this->sqeMissCount++;

		return false;
	}

	// The send data buffer belongs to us, no need to memcpy.
	// NOTE: data1 contains the TCP framing buffer and data2 the actual payload.
	if (this->IsDataInSendBuffers(data2))
	{
		MS_ASSERT(data2 == userData->store, "send buffer does not match userData store");

		// Always memcpy the frame len as it resides in the stack memory.
		std::memcpy(userData->frameLen, data1, len1);

		userData->iov[0].iov_base = userData->frameLen;
		userData->iov[0].iov_len  = len1;
		userData->iov[1].iov_base = userData->store;
		userData->iov[1].iov_len  = len2;
	}
	else
	{
		std::memcpy(userData->store, data1, len1);
		std::memcpy(userData->store + len1, data2, len2);

		userData->iov[0].iov_base = userData->store;
		userData->iov[0].iov_len  = len1;
		userData->iov[1].iov_base = userData->store + len1;
		userData->iov[1].iov_len  = len2;
	}

	userData->cb = cb;

	io_uring_sqe_set_data(sqe, userData);

	io_uring_prep_writev(sqe, sockfd, userData->iov, 2, 0);

	this->sqeProcessCount++;

	return true;
}

void DepLibUring::LibUring::Submit()
{
	MS_TRACE();

	// Unset active flag.
	SetInactive();

	auto err = io_uring_submit(std::addressof(this->ring));

	if (err >= 0)
	{
		MS_DEBUG_DEV("%i submission queue entries submitted", err);
	}
	else
	{
		// Get positive errno.
		int error = -err;

		MS_ERROR("io_uring_submit() failed: %s", std::strerror(error));
	}
}

DepLibUring::UserData* DepLibUring::LibUring::GetUserData()
{
	MS_TRACE();

	if (this->availableUserDataEntries.empty())
	{
		return nullptr;
	}

	auto idx = this->availableUserDataEntries.front();

	this->availableUserDataEntries.pop();

	auto* userData = std::addressof(this->userDatas[idx]);
	userData->idx  = idx;

	return userData;
}
