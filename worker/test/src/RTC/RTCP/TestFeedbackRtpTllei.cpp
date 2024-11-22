#include "common.hpp"
#include "RTC/RTCP/FeedbackRtpTllei.hpp"
#include <catch2/catch_test_macros.hpp>
#include <cstring> // std::memcmp()

using namespace RTC::RTCP;

namespace TestFeedbackRtpTllei
{
	// RTCP TLLEI packet.

	// clang-format off
	uint8_t buffer[] =
	{
		0x87, 0xcd, 0x00, 0x03, // Type: 205 (Generic RTP Feedback), Count: 7 (TLLEI) Length: 3
		0x00, 0x00, 0x00, 0x01, // Sender SSRC: 0x00000001
		0x03, 0x30, 0xbd, 0xee, // Media source SSRC: 0x0330bdee
		0x00, 0x01, 0xaa, 0x55
	};
	// clang-format on

	// TLLEI values.
	uint32_t senderSsrc{ 0x00000001 };
	uint32_t mediaSsrc{ 0x0330bdee };
	uint16_t packetId{ 1 };
	uint16_t lostPacketBitmask{ 0b1010101001010101 };

	void verify(FeedbackRtpTlleiPacket* packet)
	{
		REQUIRE(packet->GetSenderSsrc() == senderSsrc);
		REQUIRE(packet->GetMediaSsrc() == mediaSsrc);

		auto it   = packet->Begin();
		auto item = *it;

		REQUIRE(item);
		REQUIRE(item->GetPacketId() == packetId);
		REQUIRE(item->GetLostPacketBitmask() == lostPacketBitmask);
	}
} // namespace TestFeedbackRtpTllei

SCENARIO("RTCP Feeback RTP TLLEI parsing", "[parser][rtcp][feedback-rtp][tllei]")
{
	SECTION("parse FeedbackRtpTlleiPacket")
	{
		using namespace TestFeedbackRtpTllei;

		std::unique_ptr<FeedbackRtpTlleiPacket> packet{ FeedbackRtpTlleiPacket::Parse(
			buffer, sizeof(buffer)) };

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
