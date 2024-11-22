#include "common.hpp"
#include "RTC/RTCP/FeedbackPsTst.hpp"
#include <catch2/catch_test_macros.hpp>
#include <cstring> // std::memcmp()

using namespace RTC::RTCP;

namespace TestFeedbackPsTstn
{
	// RTCP TSTN packet.

	// clang-format off
	uint8_t buffer[] =
	{
		0x84, 0xce, 0x00, 0x04, // Type: 206 (Payload Specific), Count: 4 (TST), Length: 4
		0xfa, 0x17, 0xfa, 0x17, // Sender SSRC: 0xfa17fa17
		0x00, 0x00, 0x00, 0x00, // Media source SSRC: 0x00000000
		0x02, 0xd0, 0x37, 0x02, // SSRC: 0x02d03702
		0x08, 0x00, 0x00, 0x01  // Seq: 8, Reserved, Index: 1
	};
	// clang-format on

	// TSTN values.
	uint32_t senderSsrc{ 0xfa17fa17 };
	uint32_t mediaSsrc{ 0 };
	uint32_t ssrc{ 0x02d03702 };
	uint8_t seq{ 8 };
	uint8_t index{ 1 };

	void verify(FeedbackPsTstnPacket* packet)
	{
		REQUIRE(packet->GetSenderSsrc() == senderSsrc);
		REQUIRE(packet->GetMediaSsrc() == mediaSsrc);

		FeedbackPsTstnItem* item = *(packet->Begin());

		REQUIRE(item->GetSsrc() == ssrc);
		REQUIRE(item->GetSequenceNumber() == seq);
	}
} // namespace TestFeedbackPsTstn

SCENARIO("RTCP Feedback PS TSTN parsing", "[parser][rtcp][feedback-ps][tstn]")
{
	using namespace TestFeedbackPsTstn;

	SECTION("parse FeedbackPsTstPacket")
	{
		std::unique_ptr<FeedbackPsTstnPacket> packet{ FeedbackPsTstnPacket::Parse(buffer, sizeof(buffer)) };

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

	SECTION("create FeedbackPsTstPacket")
	{
		FeedbackPsTstnPacket packet(senderSsrc, mediaSsrc);

		auto* item = new FeedbackPsTstnItem(ssrc, seq, TestFeedbackPsTstn::index);

		packet.AddItem(item);

		verify(&packet);
	}
}
