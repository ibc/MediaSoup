#include "common.hpp"
#include "RTC/RTCP/FeedbackRtpSrReq.hpp"
#include <catch2/catch_test_macros.hpp>
#include <cstring> // std::memcmp()

using namespace RTC::RTCP;

namespace TestFeedbackRtpSrReq
{
	// RTCP SR-REQ packet.

	// clang-format off
	uint8_t buffer[] =
	{
		0x85, 0xcd, 0x00, 0x02, // Type: 205 (Generic RTP Feedback), Count: 5 (SR-REQ) Length: 3
		0x00, 0x00, 0x00, 0x01, // Sender SSRC: 0x00000001
		0x03, 0x30, 0xbd, 0xee, // Media source SSRC: 0x0330bdee
	};
	// clang-format on

	// SR-REQ values.
	uint32_t senderSsrc{ 0x00000001 };
	uint32_t mediaSsrc{ 0x0330bdee };

	void verify(FeedbackRtpSrReqPacket* packet)
	{
		REQUIRE(packet->GetSenderSsrc() == senderSsrc);
		REQUIRE(packet->GetMediaSsrc() == mediaSsrc);
	}
} // namespace TestFeedbackRtpSrReq

SCENARIO("RTCP Feeback RTP SR-REQ parsing", "[parser][rtcp][feedback-rtp][sr-req]")
{
	using namespace TestFeedbackRtpSrReq;

	SECTION("parse FeedbackRtpSrReqPacket")
	{
		std::unique_ptr<FeedbackRtpSrReqPacket> packet{ FeedbackRtpSrReqPacket::Parse(
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

	SECTION("create FeedbackRtpSrReqPacket")
	{
		FeedbackRtpSrReqPacket packet(senderSsrc, mediaSsrc);

		verify(&packet);
	}
}
