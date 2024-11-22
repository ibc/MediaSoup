#ifndef MS_MEDIASOUP_ERRORS_HPP
#define MS_MEDIASOUP_ERRORS_HPP

#include "Logger.hpp"
#include <cstdio> // std::snprintf()
#include <stdexcept>

class MediaSoupError : public std::runtime_error
{
public:
	explicit MediaSoupError(const char* description) : std::runtime_error(description)
	{
	}

public:
	static const size_t BufferSize{ 2000 };
	thread_local static char buffer[];
};

class MediaSoupTypeError : public MediaSoupError
{
public:
	explicit MediaSoupTypeError(const char* description) : MediaSoupError(description)
	{
	}
};

// clang-format off
#define MS_THROW_ERROR(desc, ...) \
	do \
	{ \
		MS_ERROR("throwing MediaSoupError: " desc, ##__VA_ARGS__); \
		std::snprintf(MediaSoupError::buffer, MediaSoupError::BufferSize, desc, ##__VA_ARGS__); \
		throw MediaSoupError(MediaSoupError::buffer); \
	} while (false)

#define MS_THROW_ERROR_STD(desc, ...) \
	do \
	{ \
		MS_ERROR_STD("throwing MediaSoupError: " desc, ##__VA_ARGS__); \
		std::snprintf(MediaSoupError::buffer, MediaSoupError::BufferSize, desc, ##__VA_ARGS__); \
		throw MediaSoupError(MediaSoupError::buffer); \
	} while (false)

#define MS_THROW_TYPE_ERROR(desc, ...) \
	do \
	{ \
		MS_ERROR("throwing MediaSoupTypeError: " desc, ##__VA_ARGS__); \
		std::snprintf(MediaSoupError::buffer, MediaSoupError::BufferSize, desc, ##__VA_ARGS__); \
		throw MediaSoupTypeError(MediaSoupError::buffer); \
	} while (false)

#define MS_THROW_TYPE_ERROR_STD(desc, ...) \
	do \
	{ \
		MS_ERROR_STD("throwing MediaSoupTypeError: " desc, ##__VA_ARGS__); \
		std::snprintf(MediaSoupError::buffer, MediaSoupError::BufferSize, desc, ##__VA_ARGS__); \
		throw MediaSoupTypeError(MediaSoupError::buffer); \
	} while (false)
// clang-format on

#endif
