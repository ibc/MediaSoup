#include "RTC/RTCP/FuzzerFeedbackPsVbcm.hpp"

void Fuzzer::RTC::RTCP::FeedbackPsVbcm::Fuzz(::RTC::RTCP::FeedbackPsVbcmPacket* packet)
{
	// packet->Dump();
	// Triggers a crash in fuzzer.
	// TODO: Verify that there is buffer enough for the announce length.
	// packet->Serialize(::RTC::RTCP::Buffer);
	packet->GetCount();
	packet->GetSize();

	// TODO.
	// AddItem(Item* item);

	for (auto it = packet->Begin(); it != packet->End(); ++it)
	{
		auto& item = (*it);

		// item->Dump();
		// Triggers a crash in fuzzer.
		// item->Serialize(::RTC::RTCP::Buffer);
		item->GetSize();
		item->GetSsrc();
		item->GetSequenceNumber();
		item->GetPayloadType();
		item->GetLength();
		item->GetValue();
	}
}
