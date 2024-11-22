#define MS_CLASS "RTC::RTCP::SenderReport"
// #define MS_LOG_DEV_LEVEL 3

#include "RTC/RTCP/SenderReport.hpp"
#include "Logger.hpp"
#include <cstring> // std::memcpy

namespace RTC
{
	namespace RTCP
	{
		/* Class methods. */

		SenderReport* SenderReport::Parse(const uint8_t* data, size_t len)
		{
			MS_TRACE();

			// Get the header.
			auto* header = const_cast<Header*>(reinterpret_cast<const Header*>(data));

			// Packet size must be >= header size.
			if (len < HeaderSize)
			{
				MS_WARN_TAG(rtcp, "not enough space for sender report, packet discarded");

				return nullptr;
			}

			return new SenderReport(header);
		}

		/* Instance methods. */

		void SenderReport::Dump() const
		{
			MS_TRACE();

			MS_DUMP("<SenderReport>");
			MS_DUMP("  ssrc: %" PRIu32, GetSsrc());
			MS_DUMP("  ntp sec: %" PRIu32, GetNtpSec());
			MS_DUMP("  ntp frac: %" PRIu32, GetNtpFrac());
			MS_DUMP("  rtp ts: %" PRIu32, GetRtpTs());
			MS_DUMP("  packet count: %" PRIu32, GetPacketCount());
			MS_DUMP("  octet count: %" PRIu32, GetOctetCount());
			MS_DUMP("</SenderReport>");
		}

		size_t SenderReport::Serialize(uint8_t* buffer)
		{
			MS_TRACE();

			// Copy the header.
			std::memcpy(buffer, this->header, HeaderSize);

			return HeaderSize;
		}

		/* Class methods. */

		SenderReportPacket* SenderReportPacket::Parse(const uint8_t* data, size_t len)
		{
			MS_TRACE();

			// Get the header.
			auto* header = const_cast<CommonHeader*>(reinterpret_cast<const CommonHeader*>(data));

			std::unique_ptr<SenderReportPacket> packet(new SenderReportPacket(header));
			const size_t offset = Packet::CommonHeaderSize;

			SenderReport* report = SenderReport::Parse(data + offset, len - offset);

			if (report)
			{
				packet->AddReport(report);
			}

			return packet.release();
		}

		/* Instance methods. */

		size_t SenderReportPacket::Serialize(uint8_t* buffer)
		{
			MS_TRACE();

			size_t offset{ 0 };
			uint8_t* header = { nullptr };

			// Serialize packets (common header + 1 report) each.
			for (auto* report : this->reports)
			{
				// Reference current common header.
				header = buffer + offset;

				offset += Packet::Serialize(buffer + offset);
				offset += report->Serialize(buffer + offset);

				// Adjust the header count field.
				reinterpret_cast<Packet::CommonHeader*>(header)->count = 0;

				// Adjust the header length field.
				size_t length = Packet::CommonHeaderSize;
				length += SenderReport::HeaderSize;

				reinterpret_cast<Packet::CommonHeader*>(header)->length = uint16_t{ htons((length / 4) - 1) };
			}

			return offset;
		}

		void SenderReportPacket::Dump() const
		{
			MS_TRACE();

			MS_DUMP("<SenderReportPacket>");
			for (auto* report : this->reports)
			{
				report->Dump();
			}
			MS_DUMP("</SenderReportPacket>");
		}
	} // namespace RTCP
} // namespace RTC
