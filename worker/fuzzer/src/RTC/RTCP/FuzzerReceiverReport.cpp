#include "RTC/RTCP/FuzzerReceiverReport.hpp"
#include "RTC/RTCP/Packet.hpp"

void Fuzzer::RTC::RTCP::ReceiverReport::Fuzz(::RTC::RTCP::ReceiverReportPacket* packet)
{
	// packet->Dump();
	packet->Serialize(::RTC::RTCP::Buffer);
	packet->GetCount();
	packet->GetSize();

	packet->GetSsrc();
	packet->SetSsrc(1111);

	// TODO.
	// AddReport(ReceiverReport* report);

	for (auto it = packet->Begin(); it != packet->End(); ++it)
	{
		auto& report = (*it);

		// report->Dump();
		report->Serialize(::RTC::RTCP::Buffer);
		report->GetSize();
		report->GetSsrc();
		report->SetSsrc(1111);
		report->GetFractionLost();
		report->SetFractionLost(64);
		report->GetTotalLost();
		report->SetTotalLost(2222);
		report->GetLastSeq();
		report->SetLastSeq(3333);
		report->GetJitter();
		report->SetJitter(4444);
		report->GetLastSenderReport();
		report->SetLastSenderReport(5555);
		report->GetDelaySinceLastSenderReport();
		report->SetDelaySinceLastSenderReport(6666);
	}
}
