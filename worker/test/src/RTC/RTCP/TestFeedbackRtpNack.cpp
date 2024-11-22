#include "common.hpp"
#include "RTC/RTCP/FeedbackRtpNack.hpp"
#include <catch2/catch_test_macros.hpp>
#include <cstring> // std::memcmp()

using namespace RTC::RTCP;

namespace TestFeedbackRtpNack
{
	// RTCP NACK packet.

	// clang-format off
	uint8_t buffer[] =
	{
		0x81, 0xcd, 0x00, 0x03, // Type: 205 (Generic RTP Feedback), Length: 3
		0x00, 0x00, 0x00, 0x01, // Sender SSRC: 0x00000001
		0x03, 0x30, 0xbd, 0xee, // Media source SSRC: 0x0330bdee
		0x0b, 0x8f, 0x00, 0x03  // NACK PID: 2959, NACK BLP: 0x0003
	};
	// clang-format on

	// NACK values.
	uint32_t senderSsrc{ 0x00000001 };
	uint32_t mediaSsrc{ 0x0330bdee };
	uint16_t pid{ 2959 };
	uint16_t lostPacketBitmask{ 0x0003 };

	void verify(FeedbackRtpNackPacket* packet)
	{
		REQUIRE(packet->GetSenderSsrc() == senderSsrc);
		REQUIRE(packet->GetMediaSsrc() == mediaSsrc);

		auto it   = packet->Begin();
		auto item = *it;

		REQUIRE(item->GetPacketId() == pid);
		REQUIRE(item->GetLostPacketBitmask() == lostPacketBitmask);
		REQUIRE(item->CountRequestedPackets() == 3);
	}
} // namespace TestFeedbackRtpNack

SCENARIO("RTCP Feeback RTP NACK parsing", "[parser][rtcp][feedback-rtp][nack]")
{
	using namespace TestFeedbackRtpNack;

	SECTION("parse FeedbackRtpNackItem")
	{
		std::unique_ptr<FeedbackRtpNackPacket> packet{ FeedbackRtpNackPacket::Parse(
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

	SECTION("create FeedbackRtpNackPacket")
	{
		FeedbackRtpNackPacket packet(senderSsrc, mediaSsrc);
		auto* item = new FeedbackRtpNackItem(pid, lostPacketBitmask);

		packet.AddItem(item);

		verify(&packet);
	}
}
