#include "common.hpp"
#include "RTC/RTCP/FeedbackRtpEcn.hpp"
#include <catch2/catch_test_macros.hpp>
#include <cstring> // std::memcmp()

using namespace RTC::RTCP;

namespace TestFeedbackRtpEcn
{
	// RTCP ECN packet.

	// clang-format off
	uint8_t buffer[] =
	{
		0x88, 0xcd, 0x00, 0x07, // Type: 205 (Generic RTP Feedback), Count: 8 (ECN) Length: 7
		0x00, 0x00, 0x00, 0x01, // Sender SSRC: 0x00000001
		0x03, 0x30, 0xbd, 0xee, // Media source SSRC: 0x0330bdee
		0x00, 0x00, 0x00, 0x01, // Extended Highest Sequence Number
		0x00, 0x00, 0x00, 0x01, // ECT (0) Counter
		0x00, 0x00, 0x00, 0x01, // ECT (1) Counter
		0x00, 0x01,             // ECN-CE Counter
		            0x00, 0x01, // not-ECT Counter
		0x00, 0x01,             // Lost Packets Counter
		            0x00, 0x01  // Duplication Counter
	};
	// clang-format on

	// ECN values.
	uint32_t senderSsrc{ 0x00000001 };
	uint32_t mediaSsrc{ 0x0330bdee };
	uint32_t sequenceNumber{ 1 };
	uint32_t ect0Counter{ 1 };
	uint32_t ect1Counter{ 1 };
	uint16_t ecnCeCounter{ 1 };
	uint16_t notEctCounter{ 1 };
	uint16_t lostPackets{ 1 };
	uint16_t duplicatedPackets{ 1 };

	void verify(FeedbackRtpEcnPacket* packet)
	{
		REQUIRE(packet->GetSenderSsrc() == senderSsrc);
		REQUIRE(packet->GetMediaSsrc() == mediaSsrc);

		auto it   = packet->Begin();
		auto item = *it;

		REQUIRE(item);
		REQUIRE(item->GetSequenceNumber() == sequenceNumber);
		REQUIRE(item->GetEct0Counter() == ect0Counter);
		REQUIRE(item->GetEct1Counter() == ect1Counter);
		REQUIRE(item->GetEcnCeCounter() == ecnCeCounter);
		REQUIRE(item->GetNotEctCounter() == notEctCounter);
		REQUIRE(item->GetLostPackets() == lostPackets);
		REQUIRE(item->GetDuplicatedPackets() == duplicatedPackets);
	}
} // namespace TestFeedbackRtpEcn

SCENARIO("RTCP Feeback RTP ECN parsing", "[parser][rtcp][feedback-rtp][ecn]")
{
	SECTION("parse FeedbackRtpEcnPacket")
	{
		using namespace TestFeedbackRtpEcn;

		std::unique_ptr<FeedbackRtpEcnPacket> packet{ FeedbackRtpEcnPacket::Parse(buffer, sizeof(buffer)) };

		REQUIRE(packet);

		verify(packet.get());

		SECTION("serialize packet instance")
		{
			uint8_t serialized[sizeof(buffer)] = { 0 };

			packet->Serialize(serialized);

			SECTION("compare serialized packet with original buffer")
			{
				REQUIRE(std::memcmp(buffer, serialized, sizeof(buffer)) == 0);
			}
		}
	}
}
