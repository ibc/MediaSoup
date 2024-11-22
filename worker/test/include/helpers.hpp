#ifndef MS_TEST_HELPERS_HPP
#define MS_TEST_HELPERS_HPP

#include "common.hpp"
#include <fstream>
#include <string>

static int BUFFER_SIZE = 65536;

namespace helpers
{
	inline bool readBinaryFile(const char* file, uint8_t* buffer, size_t* len)
	{
		std::string filePath = "test/" + std::string(file);
#ifdef _WIN32
		std::replace(filePath.begin(), filePath.end(), '/', '\\');
#endif
		std::ifstream in(filePath, std::ios::ate | std::ios::binary);

		if (!in)
		{
			return false;
		}

		*len = static_cast<size_t>(in.tellg()) - 1;
		in.seekg(0, std::ios::beg);
		in.read(reinterpret_cast<char*>(buffer), *len);
		in.close();

		return true;
	}

	inline bool addToBuffer(uint8_t* buf, int* size, uint8_t* data, size_t len)
	{
		if (*size + len > BUFFER_SIZE)
		{
			return false;
		}

		int i = 0;

		if (len == 1)
		{
			buf[*size] = *data;
		}
		else
		{
			for (i = 0; i < len; i++)
			{
				buf[*size + i] = data[i];
			}
		}
		*size += len;

		return true;
	}

	inline bool readPayloadData(const char* file, int pos, int bytes, uint8_t* payload)
	{
		std::string filePath = "test/" + std::string(file);
#ifdef _WIN32
		std::replace(filePath.begin(), filePath.end(), '/', '\\');
#endif
		std::ifstream in(filePath, std::ios::ate | std::ios::binary);

		if (!in)
		{
			return false;
		}

		in.seekg(pos, std::ios::beg);
		in.read(reinterpret_cast<char*>(payload), bytes);

		in.close();

		return true;
	}

	inline bool writeRtpPacket(
	  const char* file,
	  uint8_t nalType,
	  size_t nalLength,
	  int32_t sid,
	  int32_t tid,
	  int32_t isIdr,
	  int32_t firstSliceId,
	  int32_t lastSliceId,
	  uint8_t* payload,
	  uint8_t* buf,
	  size_t* len)
	{
		std::string filePath = "test/" + std::string(file);
#ifdef _WIN32
		std::replace(filePath.begin(), filePath.end(), '/', '\\');
#endif

		uint8_t buffer[16] = { 144, 111, 92, 65, 98, 245, 71, 218, 159, 113, 8, 226, 190, 222, 0, 1 };

		int packet_size = 0;
		uint8_t oneByte = 0;

		// write the RTP header
		if (!addToBuffer(buf, &packet_size, buffer, 16))
		{
			return false;
		}

		// write ID and length of frame marking extension
		// if first layer then length should be 0, else 1
		oneByte = oneByte | 1 << 4;

		if (sid != -1)
		{
			oneByte = oneByte | 0x01;
		}

		if (!addToBuffer(buf, &packet_size, &oneByte, 1))
		{
			return false;
		}

		// write SEIDB TID bits
		oneByte = 0;

		if (firstSliceId == 1)
		{
			oneByte = oneByte | 1 << 7;
		}

		if (lastSliceId == 1)
		{
			oneByte = oneByte | 1 << 6;
		}

		if (isIdr == 1)
		{
			oneByte = oneByte | 1 << 5;
		}

		if (tid != -1)
		{
			oneByte = oneByte | tid;
		}

		if (!addToBuffer(buf, &packet_size, &oneByte, 1))
		{
			return false;
		}

		// write DID QID bits
		oneByte = 0;

		if (sid != -1)
		{
			oneByte = oneByte | sid << 6;
		}

		if (!addToBuffer(buf, &packet_size, &oneByte, 1))
		{
			return false;
		}

		// write TL0PICIDX
		oneByte = 0;

		if (!addToBuffer(buf, &packet_size, &oneByte, 1))
		{
			return false;
		}

		// write payload
		if (!addToBuffer(buf, &packet_size, payload, nalLength))
		{
			return false;
		}

		*len = packet_size;

		return true;
	}
} // namespace helpers

#endif
