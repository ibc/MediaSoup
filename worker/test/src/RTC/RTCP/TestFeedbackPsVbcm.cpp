#include "common.hpp"
#include "RTC/RTCP/FeedbackPsVbcm.hpp"
#include <catch2/catch_test_macros.hpp>
#include <cstring> // std::memcmp()

using namespace RTC::RTCP;

namespace TestFeedbackPsVbcm
{
	// RTCP VBCM packet.

	// clang-format off
	uint8_t buffer[] =
	{
		0x84, 0xce, 0x00, 0x05, // Type: 206 (Payload Specific), Count: 4 (VBCM), Length: 5
		0xfa, 0x17, 0xfa, 0x17, // Sender SSRC: 0xfa17fa17
		0x00, 0x00, 0x00, 0x00, // Media source SSRC: 0x00000000
		0x02, 0xd0, 0x37, 0x02, // SSRC: 0x02d03702
		0x08,                   // Seq: 8
		      0x02,             // Zero | Payload Vbcm
		            0x00, 0x01, // Length
		0x01,                   // VBCM Octet String
		      0x00, 0x00, 0x00  // Padding
	};
	// clang-format on

	// VBCM values.
	uint32_t senderSsrc{ 0xfa17fa17 };
	uint32_t mediaSsrc{ 0 };
	uint32_t ssrc{ 0x02d03702 };
	uint8_t seq{ 8 };
	uint8_t payloadType{ 2 };
	uint16_t length{ 1 };
	uint8_t valueMask{ 1 };

	void verify(FeedbackPsVbcmPacket* packet)
	{
		REQUIRE(packet->GetSenderSsrc() == senderSsrc);
		REQUIRE(packet->GetMediaSsrc() == mediaSsrc);

		FeedbackPsVbcmItem* item = *(packet->Begin());

		REQUIRE(item);
		REQUIRE(item->GetSsrc() == ssrc);
		REQUIRE(item->GetSequenceNumber() == seq);
		REQUIRE(item->GetPayloadType() == payloadType);
		REQUIRE(item->GetLength() == length);
		REQUIRE((item->GetValue()[item->GetLength() - 1] & 1) == valueMask);
	}
} // namespace TestFeedbackPsVbcm

SCENARIO("RTCP Feedback PS VBCM parsing", "[parser][rtcp][feedback-ps][vbcm]")
{
	using namespace TestFeedbackPsVbcm;

	SECTION("parse FeedbackPsVbcmPacket")
	{
		std::unique_ptr<FeedbackPsVbcmPacket> packet{ FeedbackPsVbcmPacket::Parse(buffer, sizeof(buffer)) };

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
