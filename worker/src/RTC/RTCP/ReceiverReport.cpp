#define MS_CLASS "RTC::RTCP::ReceiverReport"
// #define MS_LOG_DEV_LEVEL 3

#include "RTC/RTCP/ReceiverReport.hpp"
#include "Logger.hpp"
#include "Utils.hpp"
#include <cstring> // std::memcpy

namespace RTC
{
	namespace RTCP
	{
		/* Class methods. */

		ReceiverReport* ReceiverReport::Parse(const uint8_t* data, size_t len)
		{
			MS_TRACE();

			// Get the header.
			auto* header = const_cast<Header*>(reinterpret_cast<const Header*>(data));

			// Packet size must be >= header size.
			if (len < HeaderSize)
			{
				MS_WARN_TAG(rtcp, "not enough space for receiver report, packet discarded");

				return nullptr;
			}

			return new ReceiverReport(header);
		}

		/* Instance methods. */

		void ReceiverReport::Dump() const
		{
			MS_TRACE();

			MS_DUMP("<ReceiverReport>");
			MS_DUMP("  ssrc: %" PRIu32, GetSsrc());
			MS_DUMP("  fraction lost: %" PRIu8, GetFractionLost());
			MS_DUMP("  total lost: %" PRIu32, GetTotalLost());
			MS_DUMP("  last seq: %" PRIu32, GetLastSeq());
			MS_DUMP("  jitter: %" PRIu32, GetJitter());
			MS_DUMP("  lsr: %" PRIu32, GetLastSenderReport());
			MS_DUMP("  dlsr: %" PRIu32, GetDelaySinceLastSenderReport());
			MS_DUMP("</ReceiverReport>");
		}

		size_t ReceiverReport::Serialize(uint8_t* buffer)
		{
			MS_TRACE();

			// Copy the header.
			std::memcpy(buffer, this->header, HeaderSize);

			return HeaderSize;
		}

		/* Static Class members */

		size_t ReceiverReportPacket::MaxReportsPerPacket = 31;

		/* Class methods. */

		/**
		 * ReceiverReportPacket::Parse()
		 * @param  data   - Points to the begining of the incoming RTCP packet.
		 * @param  len    - Total length of the packet.
		 * @param  offset - points to the first Receiver Report in the incoming packet.
		 */
		ReceiverReportPacket* ReceiverReportPacket::Parse(const uint8_t* data, size_t len, size_t offset)
		{
			MS_TRACE();

			// Get the header.
			auto* header = const_cast<CommonHeader*>(reinterpret_cast<const CommonHeader*>(data));

			// Ensure there is space for the common header and the SSRC of packet sender.
			if (len < Packet::CommonHeaderSize + 4u /* ssrc */)
			{
				MS_WARN_TAG(rtcp, "not enough space for receiver report packet, packet discarded");

				return nullptr;
			}

			std::unique_ptr<ReceiverReportPacket> packet(new ReceiverReportPacket(header));

			const uint32_t ssrc =
			  Utils::Byte::Get4Bytes(reinterpret_cast<uint8_t*>(header), Packet::CommonHeaderSize);

			packet->SetSsrc(ssrc);

			if (offset == 0)
			{
				offset = Packet::CommonHeaderSize + 4u /* ssrc */;
			}

			uint8_t count = header->count;

			while ((count-- != 0u) && (len > offset))
			{
				ReceiverReport* report = ReceiverReport::Parse(data + offset, len - offset);

				if (report != nullptr)
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

		size_t ReceiverReportPacket::Serialize(uint8_t* buffer)
		{
			MS_TRACE();

			size_t offset   = 0;
			uint8_t* header = { nullptr };

			for (size_t i = 0; i < this->GetCount(); i++)
			{
				// Create a new RR packet header for each 31 reports.
				if (i % MaxReportsPerPacket == 0)
				{
					// Reference current common header.
					header = buffer + offset;
					offset += Packet::Serialize(buffer + offset);

					// Copy the SSRC.
					Utils::Byte::Set4Bytes(header, Packet::CommonHeaderSize, this->ssrc);
					offset += 4u;
				}

				// Serialize next report.
				offset += this->reports[i]->Serialize(buffer + offset);

				// Adjust the header count field.
				reinterpret_cast<Packet::CommonHeader*>(header)->count =
				  static_cast<uint8_t>((i % MaxReportsPerPacket) + 1);

				// Adjust the header length field.
				size_t length = (Packet::CommonHeaderSize + 4u /* this->ssrc */);
				length += ReceiverReport::HeaderSize * ((i % MaxReportsPerPacket) + 1);

				reinterpret_cast<Packet::CommonHeader*>(header)->length = uint16_t{ htons((length / 4) - 1) };
			}

			return offset;
		}

		void ReceiverReportPacket::Dump() const
		{
			MS_TRACE();

			MS_DUMP("<ReceiverReportPacket>");
			MS_DUMP("  ssrc: %" PRIu32, this->ssrc);
			for (auto* report : this->reports)
			{
				report->Dump();
			}
			MS_DUMP("</ReceiverReportPacket>");
		}
	} // namespace RTCP
} // namespace RTC
