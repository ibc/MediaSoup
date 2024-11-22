#define MS_CLASS "RTC::RTCP::XrDelaySinceLastRr"
// #define MS_LOG_DEV_LEVEL 3

#include "RTC/RTCP/XrDelaySinceLastRr.hpp"
#include "Logger.hpp"
#include <cstring> // std::memcpy

namespace RTC
{
	namespace RTCP
	{
		/* Class methods. */

		DelaySinceLastRr::SsrcInfo* DelaySinceLastRr::SsrcInfo::Parse(const uint8_t* data, size_t len)
		{
			MS_TRACE();

			// Ensure there is space for the common header and the SSRC of packet sender.
			if (len < BodySize)
			{
				MS_WARN_TAG(rtcp, "not enough space for a extended DSLR sub-block, sub-block discarded");

				return nullptr;
			}

			// Get the header.
			auto* body = const_cast<SsrcInfo::Body*>(reinterpret_cast<const SsrcInfo::Body*>(data));

			auto* ssrcInfo = new DelaySinceLastRr::SsrcInfo(body);

			return ssrcInfo;
		}

		/* Instance methods. */

		void DelaySinceLastRr::SsrcInfo::Dump() const
		{
			MS_TRACE();

			MS_DUMP("  <SsrcInfo>");
			MS_DUMP("    ssrc: %" PRIu32, GetSsrc());
			MS_DUMP("    lrr: %" PRIu32, GetLastReceiverReport());
			MS_DUMP("    dlrr: %" PRIu32, GetDelaySinceLastReceiverReport());
			MS_DUMP("  <SsrcInfo>");
		}

		size_t DelaySinceLastRr::SsrcInfo::Serialize(uint8_t* buffer)
		{
			MS_TRACE();

			// Copy the body.
			std::memcpy(buffer, this->body, DelaySinceLastRr::SsrcInfo::BodySize);

			return DelaySinceLastRr::SsrcInfo::BodySize;
		}

		/* Class methods. */

		DelaySinceLastRr* DelaySinceLastRr::Parse(const uint8_t* data, size_t len)
		{
			MS_TRACE();

			auto* header = const_cast<ExtendedReportBlock::CommonHeader*>(
			  reinterpret_cast<const ExtendedReportBlock::CommonHeader*>(data));
			std::unique_ptr<DelaySinceLastRr> report(new DelaySinceLastRr(header));
			size_t offset{ ExtendedReportBlock::CommonHeaderSize };
			uint16_t reportLength = ntohs(header->length) * 4;

			while (len > offset && reportLength >= DelaySinceLastRr::SsrcInfo::BodySize)
			{
				DelaySinceLastRr::SsrcInfo* ssrcInfo =
				  DelaySinceLastRr::SsrcInfo::Parse(data + offset, len - offset);

				if (ssrcInfo)
				{
					report->AddSsrcInfo(ssrcInfo);
					offset += ssrcInfo->GetSize();
					reportLength -= ssrcInfo->GetSize();
				}
				else
				{
					return report.release();
				}
			}

			return report.release();
		}

		/* Instance methods. */

		size_t DelaySinceLastRr::Serialize(uint8_t* buffer)
		{
			MS_TRACE();

			const size_t length = static_cast<uint16_t>((SsrcInfo::BodySize * this->ssrcInfos.size() / 4));

			// Fill the common header.
			this->header->blockType = static_cast<uint8_t>(this->type);
			this->header->reserved  = 0;
			this->header->length    = uint16_t{ htons(length) };

			std::memcpy(buffer, this->header, ExtendedReportBlock::CommonHeaderSize);

			size_t offset{ ExtendedReportBlock::CommonHeaderSize };

			// Serialize sub-blocks.
			for (auto* ssrcInfo : this->ssrcInfos)
			{
				offset += ssrcInfo->Serialize(buffer + offset);
			}

			return offset;
		}

		void DelaySinceLastRr::Dump() const
		{
			MS_TRACE();

			MS_DUMP("<DelaySinceLastRr>");
			MS_DUMP("  block type: %" PRIu8, (uint8_t)this->type);
			MS_DUMP("  reserved: 0");
			MS_DUMP(
			  "  length: %" PRIu16,
			  static_cast<uint16_t>((SsrcInfo::BodySize * this->ssrcInfos.size() / 4)));
			for (auto* ssrcInfo : this->ssrcInfos)
			{
				ssrcInfo->Dump();
			}
			MS_DUMP("</DelaySinceLastRr>");
		}
	} // namespace RTCP
} // namespace RTC
