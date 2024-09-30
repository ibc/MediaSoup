#ifndef MS_UNIX_STREAM_SOCKET_HANDLE_HPP
#define MS_UNIX_STREAM_SOCKET_HANDLE_HPP

#include "common.hpp"
#include <uv.h>
#include <string>

class UnixStreamSocketHandle
{
public:
	/* Struct for the data field of uv_req_t when writing data. */
	struct UvWriteData
	{
		explicit UvWriteData(size_t storeSize) : store(new uint8_t[storeSize])
		{
		}

		// Disable copy constructor because of the dynamically allocated data (store).
		UvWriteData(const UvWriteData&) = delete;

		~UvWriteData()
		{
			delete[] this->store;
		}

		uv_write_t req{};
		uint8_t* store{ nullptr };
	};

	enum class Role
	{
		PRODUCER = 1,
		CONSUMER
	};

	typedef std::function<void(const uint8_t* data, size_t len)> Writer;

public:
	UnixStreamSocketHandle(int fd, size_t bufferSize, UnixStreamSocketHandle::Role role) {
		this->bufferSize = bufferSize;
		this->role       = role;
		this->buffer     = new uint8_t[bufferSize];
	}
	UnixStreamSocketHandle& operator=(const UnixStreamSocketHandle&) = delete;
	UnixStreamSocketHandle(const UnixStreamSocketHandle&)            = delete;
	virtual ~UnixStreamSocketHandle() {
		delete[] this->buffer;
	}

public:
	void Close() {}
	bool IsClosed() const { return true; }
	static void SetWriter(Writer w) { writer_ = w; }
	void Write(const uint8_t* data, size_t len) { writer_(data, len); }

protected:
	virtual void UserOnUnixStreamRead()         = 0;
	virtual void UserOnUnixStreamSocketClosed() = 0;
	static Writer writer_;

protected:
	// Passed by argument.
	size_t bufferSize{ 0u };
	UnixStreamSocketHandle::Role role;
	// Allocated by this.
	uint8_t* buffer{ nullptr };
	// Others.
	size_t bufferDataLen{ 0u };
};

#endif
