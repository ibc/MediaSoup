#include "RTC/RTCP/FuzzerFeedbackRtpTmmb.hpp"

void Fuzzer::RTC::RTCP::FeedbackRtpTmmbn::Fuzz(::RTC::RTCP::FeedbackRtpTmmbnPacket* packet)
{
	// packet->Dump();
	packet->Serialize(::RTC::RTCP::Buffer);
	packet->GetCount();
	packet->GetSize();

	// TODO.
	// AddItem(Item* item);

	for (auto it = packet->Begin(); it != packet->End(); ++it)
	{
		auto& item = (*it);

		// item->Dump();
		item->Serialize(::RTC::RTCP::Buffer);
		item->GetSize();
		item->GetSsrc();
		item->SetSsrc(1111);
		item->GetBitrate();
		item->SetBitrate(2222);
		item->GetOverhead();
		item->SetOverhead(3333);
	}
}

void Fuzzer::RTC::RTCP::FeedbackRtpTmmbr::Fuzz(::RTC::RTCP::FeedbackRtpTmmbrPacket* packet)
{
	// packet->Dump();
	packet->Serialize(::RTC::RTCP::Buffer);
	packet->GetCount();
	packet->GetSize();

	// TODO.
	// AddItem(Item* item);

	for (auto it = packet->Begin(); it != packet->End(); ++it)
	{
		auto& item = (*it);

		// item->Dump();
		item->Serialize(::RTC::RTCP::Buffer);
		item->GetSize();
		item->GetSsrc();
		item->SetSsrc(1111);
		item->GetBitrate();
		item->SetBitrate(2222);
		item->GetOverhead();
		item->SetOverhead(3333);
	}
}
