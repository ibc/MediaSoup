#include "RTC/RTCP/FuzzerFeedbackRtpTllei.hpp"

void Fuzzer::RTC::RTCP::FeedbackRtpTllei::Fuzz(::RTC::RTCP::FeedbackRtpTlleiPacket* packet)
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
		item->GetPacketId();
		item->GetLostPacketBitmask();
	}
}
