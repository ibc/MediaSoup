#include "common.hpp"
#include "RTC/RTCP/FeedbackPsFir.hpp"
#include <catch2/catch_test_macros.hpp>
#include <cstring> // std::memcmp()

using namespace RTC::RTCP;

namespace TestFeedbackPsFir
{
	// RTCP FIR packet.

	// clang-format off
	uint8_t buffer[] =
	{
		0x84, 0xce, 0x00, 0x04, // Type: 206 (Payload Specific), Count: 4 (FIR), Length: 4
		0xfa, 0x17, 0xfa, 0x17, // Sender SSRC: 0xfa17fa17
		0x00, 0x00, 0x00, 0x00, // Media source SSRC: 0x00000000
		0x02, 0xd0, 0x37, 0x02, // SSRC: 0x02d03702
		0x04, 0x00, 0x00, 0x00  // Seq: 0x04
	};
	// clang-format on

	// FIR values.
	uint32_t senderSsrc{ 0xfa17fa17 };
	uint32_t mediaSsrc{ 0 };
	uint32_t ssrc{ 0x02d03702 };
	uint8_t seq{ 4 };

	void verify(FeedbackPsFirPacket* packet)
	{
		REQUIRE(packet->GetSenderSsrc() == senderSsrc);
		REQUIRE(packet->GetMediaSsrc() == mediaSsrc);

		FeedbackPsFirItem* item = *(packet->Begin());

		REQUIRE(item->GetSsrc() == ssrc);
		REQUIRE(item->GetSequenceNumber() == seq);
	}
} // namespace TestFeedbackPsFir

SCENARIO("RTCP Feedback PS FIR parsing", "[parser][rtcp][feedback-ps][fir]")
{
	using namespace TestFeedbackPsFir;

	SECTION("parse FeedbackPsFirPacket")
	{
		std::unique_ptr<FeedbackPsFirPacket> packet{ FeedbackPsFirPacket::Parse(buffer, sizeof(buffer)) };

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

	SECTION("create FeedbackPsFirPacket")
	{
		FeedbackPsFirPacket packet(senderSsrc, mediaSsrc);

		auto* item = new FeedbackPsFirItem(ssrc, seq);

		packet.AddItem(item);

		verify(&packet);
	}
}
