#define MS_CLASS "RTC::RTCP::XR"
// #define MS_LOG_DEV_LEVEL 3

#include "Logger.hpp"
#include "Utils.hpp"
#include "RTC/RTCP/XrDelaySinceLastRr.hpp"
#include "RTC/RTCP/XrReceiverReferenceTime.hpp"

namespace RTC
{
	namespace RTCP
	{
		/* Class methods. */

		ExtendedReportBlock* ExtendedReportBlock::Parse(const uint8_t* data, size_t len)
		{
			MS_TRACE();

			// Get the header.
			auto* header = const_cast<CommonHeader*>(reinterpret_cast<const CommonHeader*>(data));

			// Ensure there is space for the common header and the SSRC of packet sender.
			if (len < Packet::CommonHeaderSize)
			{
				MS_WARN_TAG(rtcp, "not enough space for a extended report block, report discarded");

				return nullptr;
			}

			switch (ExtendedReportBlock::Type(header->blockType))
			{
				case RTC::RTCP::ExtendedReportBlock::Type::RRT:
				{
					return ReceiverReferenceTime::Parse(data, len);
				}

				case RTC::RTCP::ExtendedReportBlock::Type::DLRR:
				{
					return DelaySinceLastRr::Parse(data, len);
				}

				default:
				{
					MS_DEBUG_TAG(rtcp, "unknown RTCP XR block type [blockType:%" PRIu8 "]", header->blockType);

					break;
				}
			}

			return nullptr;
		}

		/* Instance methods. */

		/* Class methods. */

		ExtendedReportPacket* ExtendedReportPacket::Parse(const uint8_t* data, size_t len)
		{
			MS_TRACE();

			// Get the header.
			auto* header = const_cast<CommonHeader*>(reinterpret_cast<const CommonHeader*>(data));

			// Ensure there is space for the common header and the SSRC of packet sender.
			if (len < Packet::CommonHeaderSize + 4u)
			{
				MS_WARN_TAG(rtcp, "not enough space for a extended report packet, packet discarded");

				return nullptr;
			}

			std::unique_ptr<ExtendedReportPacket> packet(new ExtendedReportPacket(header));

			const uint32_t ssrc =
			  Utils::Byte::Get4Bytes(reinterpret_cast<uint8_t*>(header), Packet::CommonHeaderSize);

			packet->SetSsrc(ssrc);

			auto offset = Packet::CommonHeaderSize + 4u /* ssrc */;

			while (len > offset)
			{
				ExtendedReportBlock* report = ExtendedReportBlock::Parse(data + offset, len - offset);

				if (report)
				{
					packet->AddReport(report);
					offset += report->GetSize();
				}
				else
				{
					return packet.release();
				}
			}

			return packet.release();
		}

		/* Instance methods. */

		size_t ExtendedReportPacket::Serialize(uint8_t* buffer)
		{
			MS_TRACE();

			size_t offset = Packet::Serialize(buffer);

			// Copy the SSRC.
			Utils::Byte::Set4Bytes(buffer, Packet::CommonHeaderSize, this->ssrc);
			offset += 4u /*ssrc*/;

			// Serialize reports.
			for (auto* report : this->reports)
			{
				offset += report->Serialize(buffer + offset);
			}

			return offset;
		}

		void ExtendedReportPacket::Dump() const
		{
			MS_TRACE();

			MS_DUMP("<ExtendedReportPacket>");
			MS_DUMP("  ssrc: %" PRIu32, this->ssrc);
			for (auto* report : this->reports)
			{
				report->Dump();
			}
			MS_DUMP("</ExtendedReportPacket>");
		}
	} // namespace RTCP
} // namespace RTC
