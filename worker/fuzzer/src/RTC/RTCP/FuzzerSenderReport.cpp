#include "RTC/RTCP/FuzzerSenderReport.hpp"
#include "RTC/RTCP/Packet.hpp"

void Fuzzer::RTC::RTCP::SenderReport::Fuzz(::RTC::RTCP::SenderReportPacket* packet)
{
	// A well formed packet must have a single report.
	if (packet->GetCount() == 1)
	{
		packet->Serialize(::RTC::RTCP::Buffer);
	}

	// packet->Dump();
	packet->GetCount();
	packet->GetSize();

	// TODO.
	// AddReport(SenderReport* report);

	for (auto it = packet->Begin(); it != packet->End(); ++it)
	{
		auto& report = (*it);

		// report->Dump();
		report->Serialize(::RTC::RTCP::Buffer);
		report->GetSize();
		report->GetSsrc();
		report->SetSsrc(1111);
		report->GetNtpSec();
		report->SetNtpSec(2222);
		report->GetNtpFrac();
		report->SetNtpFrac(3333);
		report->GetRtpTs();
		report->SetRtpTs(4444);
		report->GetPacketCount();
		report->SetPacketCount(1024);
		report->GetOctetCount();
		report->SetOctetCount(11223344);
	}
}
