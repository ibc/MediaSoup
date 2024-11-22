/*
 * Code from http://web.mit.edu/freebsd/head/contrib/wpa/src/utils/base64.c
 *
 * Base64 encoding/decoding (RFC1341)
 * Copyright (c) 2005-2011, Jouni Malinen <j@w1.fi>
 *
 * This software may be distributed under the terms of the BSD license.
 * See README_BASE64_UTILS for more details.
 */

#define MS_CLASS "Utils::String"
// #define MS_LOG_DEV_LEVEL 3

#include "Logger.hpp"
#include "MediaSoupErrors.hpp"
#include "Utils.hpp"
#include <cstring> // std::memset()

/* Static. */

static constexpr size_t BufferOutSize{ 65536 };
thread_local static uint8_t BufferOut[BufferOutSize];
static const uint8_t Base64Table[65] =
  "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

namespace Utils
{
	std::string Utils::String::Base64Encode(const uint8_t* data, size_t len)
	{
		MS_TRACE();

		const uint8_t* out = BufferOut;
		uint8_t* pos;
		const uint8_t* end;
		const uint8_t* in;
		size_t olen;

		olen = len * 4 / 3 + 4; // 3-byte blocks to 4-byte.

		if (olen < len)
		{
			MS_THROW_TYPE_ERROR("integer overflow");
		}
		else if (olen > BufferOutSize - 1)
		{
			MS_THROW_TYPE_ERROR("data too big");
		}

		end = data + len;
		in  = data;
		pos = const_cast<uint8_t*>(out);

		while (end - in >= 3)
		{
			*pos++ = Base64Table[in[0] >> 2];
			*pos++ = Base64Table[((in[0] & 0x03) << 4) | (in[1] >> 4)];
			*pos++ = Base64Table[((in[1] & 0x0f) << 2) | (in[2] >> 6)];
			*pos++ = Base64Table[in[2] & 0x3f];
			in += 3;
		}

		if (end - in)
		{
			*pos++ = Base64Table[in[0] >> 2];

			if (end - in == 1)
			{
				*pos++ = Base64Table[(in[0] & 0x03) << 4];
				*pos++ = '=';
			}
			else
			{
				*pos++ = Base64Table[((in[0] & 0x03) << 4) | (in[1] >> 4)];
				*pos++ = Base64Table[(in[1] & 0x0f) << 2];
			}

			*pos++ = '=';
		}

		return { reinterpret_cast<const char*>(out), static_cast<size_t>(pos - out) };
	}

	std::string Utils::String::Base64Encode(const std::string& str)
	{
		MS_TRACE();

		const auto* data = reinterpret_cast<const uint8_t*>(str.c_str());

		return Base64Encode(data, str.size());
	}

	uint8_t* Utils::String::Base64Decode(const uint8_t* data, size_t len, size_t& outLen)
	{
		MS_TRACE();

		uint8_t dtable[256];
		uint8_t* out = BufferOut;
		uint8_t* pos;
		uint8_t block[4];
		uint8_t tmp;
		size_t i;
		size_t count;
		int pad{ 0 };

		// NOTE: This is not really accurate but anyway.
		if (len > BufferOutSize - 1)
		{
			MS_THROW_TYPE_ERROR("data too big");
		}

		std::memset(dtable, 0x80, 256);

		for (i = 0; i < sizeof(Base64Table) - 1; ++i)
		{
			dtable[Base64Table[i]] = static_cast<uint8_t>(i);
		}
		dtable[uint8_t{ '=' }] = 0;

		count = 0;
		for (i = 0; i < len; ++i)
		{
			if (dtable[data[i]] != 0x80)
			{
				count++;
			}
		}

		if (count == 0 || count % 4)
		{
			MS_THROW_TYPE_ERROR("invalid data");
		}

		pos   = out;
		count = 0;

		for (i = 0; i < len; ++i)
		{
			tmp = dtable[data[i]];

			if (tmp == 0x80)
			{
				continue;
			}

			if (data[i] == '=')
			{
				pad++;
			}

			block[count] = tmp;
			count++;

			if (count == 4)
			{
				*pos++ = (block[0] << 2) | (block[1] >> 4);
				*pos++ = (block[1] << 4) | (block[2] >> 2);
				*pos++ = (block[2] << 6) | block[3];
				count  = 0;

				if (pad)
				{
					if (pad == 1)
					{
						pos--;
					}
					else if (pad == 2)
					{
						pos -= 2;
					}
					else
					{
						MS_THROW_TYPE_ERROR("integer padding");
					}

					break;
				}
			}
		}

		outLen = pos - out;

		return out;
	}

	uint8_t* Utils::String::Base64Decode(const std::string& str, size_t& outLen)
	{
		MS_TRACE();

		const auto* data = reinterpret_cast<const uint8_t*>(str.c_str());

		return Base64Decode(data, str.size(), outLen);
	}
} // namespace Utils
