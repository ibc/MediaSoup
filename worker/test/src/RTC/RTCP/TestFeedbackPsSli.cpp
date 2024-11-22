#include "common.hpp"
#include "RTC/RTCP/FeedbackPsSli.hpp"
#include <catch2/catch_test_macros.hpp>
#include <cstring> // std::memcmp()

using namespace RTC::RTCP;

namespace TestFeedbackPsSli
{
	// RTCP SLI packet.

	// clang-format off
	uint8_t buffer[] =
	{
		0x82, 0xce, 0x00, 0x03, // Type: 206 (Payload Specific), Count: 2 (SLI), Length: 3
		0xfa, 0x17, 0xfa, 0x17, // Sender SSRC: 0xfa17fa17
		0x00, 0x00, 0x00, 0x00, // Media source SSRC: 0x00000000
		0x00, 0x08, 0x01, 0x01  // First: 1, Number: 4, PictureId: 1
	};
	// clang-format on

	// SLI values.
	uint32_t senderSsrc{ 0xfa17fa17 };
	uint32_t mediaSsrc{ 0 };
	uint16_t first{ 1 };
	uint16_t number{ 4 };
	uint8_t pictureId{ 1 };

	void verify(FeedbackPsSliPacket* packet)
	{
		REQUIRE(packet->GetSenderSsrc() == senderSsrc);
		REQUIRE(packet->GetMediaSsrc() == mediaSsrc);

		FeedbackPsSliItem* item = *(packet->Begin());

		REQUIRE(item);
		REQUIRE(item->GetFirst() == first);
		REQUIRE(item->GetNumber() == number);
		REQUIRE(item->GetPictureId() == pictureId);
	}
} // namespace TestFeedbackPsSli

SCENARIO("RTCP Feedback PS SLI parsing", "[parser][rtcp][feedback-ps][sli]")
{
	using namespace TestFeedbackPsSli;

	SECTION("parse FeedbackPsSliPacket")
	{
		std::unique_ptr<FeedbackPsSliPacket> packet{ FeedbackPsSliPacket::Parse(buffer, sizeof(buffer)) };

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
