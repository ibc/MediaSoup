#include "RTC/RTCP/FuzzerPacket.hpp"
#include "RTC/RTCP/FuzzerBye.hpp"
#include "RTC/RTCP/FuzzerFeedbackPs.hpp"
#include "RTC/RTCP/FuzzerFeedbackRtp.hpp"
#include "RTC/RTCP/FuzzerReceiverReport.hpp"
#include "RTC/RTCP/FuzzerSdes.hpp"
#include "RTC/RTCP/FuzzerSenderReport.hpp"
#include "RTC/RTCP/FuzzerXr.hpp"
#include "RTC/RTCP/Packet.hpp"

void Fuzzer::RTC::RTCP::Packet::Fuzz(const uint8_t* data, size_t len)
{
	if (!::RTC::RTCP::Packet::IsRtcp(data, len))
	{
		return;
	}

	// We need to clone the given data into a separate buffer because setters
	// below will try to write into packet memory.
	uint8_t data2[len];

	std::memcpy(data2, data, len);

	::RTC::RTCP::Packet* packet = ::RTC::RTCP::Packet::Parse(data2, len);

	if (!packet)
	{
		return;
	}

	while (packet != nullptr)
	{
		auto* previousPacket = packet;

		switch (::RTC::RTCP::Type(packet->GetType()))
		{
			case ::RTC::RTCP::Type::SR:
			{
				auto* sr = dynamic_cast<::RTC::RTCP::SenderReportPacket*>(packet);

				RTC::RTCP::SenderReport::Fuzz(sr);

				break;
			}

			case ::RTC::RTCP::Type::RR:
			{
				auto* rr = dynamic_cast<::RTC::RTCP::ReceiverReportPacket*>(packet);

				RTC::RTCP::ReceiverReport::Fuzz(rr);

				break;
			}

			case ::RTC::RTCP::Type::SDES:
			{
				auto* sdes = dynamic_cast<::RTC::RTCP::SdesPacket*>(packet);

				RTC::RTCP::Sdes::Fuzz(sdes);

				break;
			}

			case ::RTC::RTCP::Type::BYE:
			{
				auto* bye = dynamic_cast<::RTC::RTCP::ByePacket*>(packet);

				RTC::RTCP::Bye::Fuzz(bye);

				break;
			}

			case ::RTC::RTCP::Type::RTPFB:
			{
				RTC::RTCP::FeedbackRtp::Fuzz(packet);

				break;
			}

			case ::RTC::RTCP::Type::PSFB:
			{
				RTC::RTCP::FeedbackPs::Fuzz(packet);

				break;
			}

			case ::RTC::RTCP::Type::XR:
			{
				auto* xr = dynamic_cast<::RTC::RTCP::ExtendedReportPacket*>(packet);

				RTC::RTCP::ExtendedReport::Fuzz(xr);

				break;
			}

			default:
			{
			}
		}

		packet = packet->GetNext();

		delete previousPacket;
	}
}
