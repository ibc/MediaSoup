#include "common.hpp"
#include "RTC/RTCP/FeedbackPsAfb.hpp"
#include <catch2/catch_test_macros.hpp>
#include <cstring> // std::memcmp()

using namespace RTC::RTCP;

namespace TestFeedbackPsAfb
{
	// RTCP AFB packet.

	// clang-format off
	uint8_t buffer[] =
	{
		0x8f, 0xce, 0x00, 0x03, // Type: 206 (Payload Specific), Count: 15 (AFB) Length: 3
		0xfa, 0x17, 0xfa, 0x17, // Sender SSRC: 0xfa17fa17
		0x00, 0x00, 0x00, 0x00, // Media source SSRC: 0x00000000
		0x00, 0x00, 0x00, 0x01  // Data
	};
	// clang-format on

	// AFB values.
	uint32_t senderSsrc{ 0xfa17fa17 };
	uint32_t mediaSsrc{ 0 };
	size_t dataSize{ 4 };
	uint8_t dataBitmask{ 1 };

	void verify(FeedbackPsAfbPacket* packet)
	{
		REQUIRE(packet->GetSenderSsrc() == senderSsrc);
		REQUIRE(packet->GetMediaSsrc() == mediaSsrc);
		REQUIRE(packet->GetApplication() == FeedbackPsAfbPacket::Application::UNKNOWN);
	}
} // namespace TestFeedbackPsAfb

SCENARIO("RTCP Feedback PS AFB parsing", "[parser][rtcp][feedback-ps][afb]")
{
	SECTION("parse FeedbackPsAfbPacket")
	{
		using namespace TestFeedbackPsAfb;

		std::unique_ptr<FeedbackPsAfbPacket> packet{ FeedbackPsAfbPacket::Parse(buffer, sizeof(buffer)) };

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
