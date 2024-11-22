#ifndef MS_RTC_RTCP_COMPOUND_PACKET_HPP
#define MS_RTC_RTCP_COMPOUND_PACKET_HPP

#include "common.hpp"
#include "RTC/RTCP/ReceiverReport.hpp"
#include "RTC/RTCP/Sdes.hpp"
#include "RTC/RTCP/SenderReport.hpp"
#include "RTC/RTCP/XrDelaySinceLastRr.hpp"
#include "RTC/RTCP/XrReceiverReferenceTime.hpp"
#include "RTC/RtpPacket.hpp" // MtuSize.
#include <vector>

namespace RTC
{
	namespace RTCP
	{
		class CompoundPacket
		{
		public:
			// Maximum size for a CompundPacket.
			// * IPv4|Ipv6 header size: 20|40 bytes. IPv6 considered.
			// * UDP|TCP header size:   8|20  bytes. TCP considered.
			// * SRTP Encryption: 148 bytes.
			//    SRTP_MAX_TRAILER_LEN+4 is the maximum number of octects that will be
			//    added to an RTCP packet by srtp_protect_rtcp().
			//    srtp.h: SRTP_MAX_TRAILER_LEN (SRTP_MAX_TAG_LEN + SRTP_MAX_MKI_LEN)
			constexpr static size_t MaxSize{ RTC::MtuSize - 40u - 20u - 148u };

		public:
			CompoundPacket() = default;

		public:
			const uint8_t* GetData() const
			{
				return this->header;
			}
			size_t GetSize();
			size_t GetSenderReportCount() const
			{
				return this->senderReportPacket.GetCount();
			}
			size_t GetReceiverReportCount() const
			{
				return this->receiverReportPacket.GetCount();
			}
			void Dump();
			// RTCP additions per Consumer (non pipe).
			// Adds the given data and returns true if there is enough space to hold it,
			// false otherwise.
			bool Add(
			  SenderReport* senderReport,
			  SdesChunk* sdesChunk,
			  DelaySinceLastRr::SsrcInfo* delaySinceLastRrSsrcInfo);
			// RTCP additions per Consumer (pipe).
			// Adds the given data and returns true if there is enough space to hold it,
			// false otherwise.
			bool Add(
			  std::vector<SenderReport*>& senderReports,
			  std::vector<SdesChunk*>& sdesChunks,
			  std::vector<DelaySinceLastRr::SsrcInfo*>& delaySinceLastRrSsrcInfos);
			// RTCP additions per Producer.
			// Adds the given data and returns true if there is enough space to hold it,
			// false otherwise.
			bool Add(std::vector<ReceiverReport*>&, ReceiverReferenceTime*);
			void AddSenderReport(SenderReport* report);
			void AddReceiverReport(ReceiverReport* report);
			void AddSdesChunk(SdesChunk* chunk);
			bool HasSenderReport()
			{
				return this->senderReportPacket.Begin() != this->senderReportPacket.End();
			}
			bool HasReceiverReferenceTime()
			{
				return std::any_of(
				  this->xrPacket.Begin(),
				  this->xrPacket.End(),
				  [](const ExtendedReportBlock* report)
				  { return report->GetType() == ExtendedReportBlock::Type::RRT; });
			}
			bool HasDelaySinceLastRr()
			{
				return std::any_of(
				  this->xrPacket.Begin(),
				  this->xrPacket.End(),
				  [](const ExtendedReportBlock* report)
				  { return report->GetType() == ExtendedReportBlock::Type::DLRR; });
			}
			void Serialize(uint8_t* data);

		private:
			uint8_t* header{ nullptr };
			SenderReportPacket senderReportPacket;
			ReceiverReportPacket receiverReportPacket;
			SdesPacket sdesPacket;
			ExtendedReportPacket xrPacket;
			DelaySinceLastRr* delaySinceLastRr{ nullptr };
		};
	} // namespace RTCP
} // namespace RTC

#endif
