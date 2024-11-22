#include "common.hpp"
#include "RTC/RTCP/FeedbackPsRemb.hpp"
#include <catch2/catch_test_macros.hpp>
#include <cstring> // std::memcmp()

using namespace RTC::RTCP;

namespace TestFeedbackPsRemb
{
	// RTCP REMB packet.

	// clang-format off
	uint8_t buffer[] =
	{
		0x8f, 0xce, 0x00, 0x06, // Type: 206 (Payload Specific), Count: 15 (AFB), Length: 6
		0xfa, 0x17, 0xfa, 0x17, // Sender SSRC: 0xfa17fa17
		0x00, 0x00, 0x00, 0x00, // Media source SSRC: 0x00000000
		0x52, 0x45, 0x4d, 0x42, // Unique Identifier: REMB
		0x02, 0x01, 0xdf, 0x82, // SSRCs: 2, BR exp: 0, Mantissa: 122754
		0x02, 0xd0, 0x37, 0x02, // SSRC1: 0x02d03702
		0x04, 0xa7, 0x67, 0x47  // SSRC2: 0x04a76747
	};
	// clang-format on

	// REMB values.
	uint32_t senderSsrc{ 0xfa17fa17 };
	uint32_t mediaSsrc{ 0u };
	uint64_t bitrate{ 122754u };
	std::vector<uint32_t> ssrcs{ 0x02d03702, 0x04a76747 };

	void verify(FeedbackPsRembPacket* packet)
	{
		REQUIRE(packet->GetSenderSsrc() == senderSsrc);
		REQUIRE(packet->GetMediaSsrc() == mediaSsrc);
		REQUIRE(packet->GetBitrate() == bitrate);
		REQUIRE(packet->GetSsrcs() == ssrcs);
	}
} // namespace TestFeedbackPsRemb

SCENARIO("RTCP Feedback PS parsing", "[parser][rtcp][feedback-ps][remb]")
{
	using namespace TestFeedbackPsRemb;

	SECTION("parse FeedbackPsRembPacket")
	{
		std::unique_ptr<FeedbackPsRembPacket> packet{ FeedbackPsRembPacket::Parse(buffer, sizeof(buffer)) };

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

	SECTION("create FeedbackPsRembPacket")
	{
		FeedbackPsRembPacket packet(senderSsrc, mediaSsrc);

		packet.SetSsrcs(ssrcs);
		packet.SetBitrate(bitrate);

		verify(&packet);
	}
}
