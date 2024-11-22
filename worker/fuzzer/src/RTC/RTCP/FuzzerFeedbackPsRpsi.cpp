#include "RTC/RTCP/FuzzerFeedbackPsRpsi.hpp"

void Fuzzer::RTC::RTCP::FeedbackPsRpsi::Fuzz(::RTC::RTCP::FeedbackPsRpsiPacket* packet)
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
		item->IsCorrect();
		item->GetPayloadType();
		item->GetBitString();
		item->GetLength();
	}
}
