#define MS_CLASS "RTC::RTCP::Bye"
// #define MS_LOG_DEV_LEVEL 3

#include "RTC/RTCP/Bye.hpp"
#include "Logger.hpp"
#include "Utils.hpp"
#include <cstring>

namespace RTC
{
	namespace RTCP
	{
		/* Class methods. */

		ByePacket* ByePacket::Parse(const uint8_t* data, size_t len)
		{
			MS_TRACE();

			// Get the header.
			auto* header = const_cast<CommonHeader*>(reinterpret_cast<const CommonHeader*>(data));
			std::unique_ptr<ByePacket> packet(new ByePacket(header));
			size_t offset = Packet::CommonHeaderSize;
			uint8_t count = header->count;

			while (((count--) != 0u) && (len > offset))
			{
				if (len - offset < 4u)
				{
					MS_WARN_TAG(rtcp, "not enough space for SSRC in RTCP Bye message");

					return nullptr;
				}

				packet->AddSsrc(Utils::Byte::Get4Bytes(data, offset));
				offset += 4u;
			}

			if (len > offset)
			{
				auto length = size_t{ Utils::Byte::Get1Byte(data, offset) };

				offset += 1u;

				if (length <= len - offset)
				{
					packet->SetReason(std::string(reinterpret_cast<const char*>(data) + offset, length));
				}
			}

			return packet.release();
		}

		/* Instance methods. */

		size_t ByePacket::Serialize(uint8_t* buffer)
		{
			MS_TRACE();

			size_t offset = Packet::Serialize(buffer);

			// SSRCs.
			for (auto ssrc : this->ssrcs)
			{
				Utils::Byte::Set4Bytes(buffer, offset, ssrc);
				offset += 4u;
			}

			if (!this->reason.empty())
			{
				// Length field.
				Utils::Byte::Set1Byte(buffer, offset, this->reason.length());
				offset += 1u;

				// Reason field.
				std::memcpy(buffer + offset, this->reason.c_str(), this->reason.length());
				offset += this->reason.length();
			}

			// 32 bits padding.
			const size_t padding = (-offset) & 3;

			for (size_t i{ 0 }; i < padding; ++i)
			{
				buffer[offset + i] = 0;
			}

			return offset + padding;
		}

		void ByePacket::Dump() const
		{
			MS_TRACE();

			MS_DUMP("<ByePacket>");
			for (auto ssrc : this->ssrcs)
			{
				MS_DUMP("  ssrc: %" PRIu32, ssrc);
			}
			if (!this->reason.empty())
			{
				MS_DUMP("  reason: %s", this->reason.c_str());
			}
			MS_DUMP("</ByePacket>");
		}
	} // namespace RTCP
} // namespace RTC
