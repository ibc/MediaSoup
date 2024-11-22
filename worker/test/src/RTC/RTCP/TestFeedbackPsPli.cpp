#include "common.hpp"
#include "RTC/RTCP/FeedbackPsPli.hpp"
#include <catch2/catch_test_macros.hpp>
#include <cstring> // std::memcmp()

using namespace RTC::RTCP;

namespace TestFeedbackPsPli
{
	// RTCP PLI packet.

	// clang-format off
	uint8_t buffer[] =
	{
		0x81, 0xce, 0x00, 0x02, // Type: 206 (Payload Specific), Count: 1 (PLI), Length: 2
		0x00, 0x00, 0x00, 0x01, // Sender SSRC: 0x00000001
		0x03, 0x30, 0xbd, 0xee, // Media source SSRC: 0x0330bdee
	};
	// clang-format on

	// PLI values.
	uint32_t senderSsrc{ 0x00000001 };
	uint32_t mediaSsrc{ 0x0330bdee };

	void verify(FeedbackPsPliPacket* packet)
	{
		REQUIRE(packet->GetSenderSsrc() == senderSsrc);
		REQUIRE(packet->GetMediaSsrc() == mediaSsrc);
	}
} // namespace TestFeedbackPsPli

SCENARIO("RTCP Feeback RTP PLI parsing", "[parser][rtcp][feedback-ps][pli]")
{
	using namespace TestFeedbackPsPli;

	SECTION("parse FeedbackPsPliPacket")
	{
		std::unique_ptr<FeedbackPsPliPacket> packet{ FeedbackPsPliPacket::Parse(buffer, sizeof(buffer)) };

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

	SECTION("create FeedbackPsPliPacket")
	{
		FeedbackPsPliPacket packet(senderSsrc, mediaSsrc);

		verify(&packet);
	}
}
