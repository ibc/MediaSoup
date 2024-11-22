#include "common.hpp"
#include "RTC/RTCP/FeedbackRtpTmmb.hpp"
#include <catch2/catch_test_macros.hpp>
#include <cstring> // std::memcmp()

using namespace RTC::RTCP;

namespace TestFeedbackRtpTmmbr
{
	// RTCP TMMBR packet.

	// clang-format off
	uint8_t buffer[] =
	{
		0x83, 0xcd, 0x00, 0x04, // Type: 205 (Generic RTP Feedback), Count: 8 (TMMBR) Length: 7
		0x00, 0x00, 0x00, 0x01, // Sender SSRC: 0x00000001
		0x03, 0x30, 0xbd, 0xee, // Media source SSRC: 0x0330bdee
		0x02, 0xd0, 0x37, 0x02, // SSRC: 0x02d03702
		0x18, 0x2c, 0x9e, 0x00
	};
	// clang-format on

	// TMMBR values.
	uint32_t senderSsrc{ 0x00000001 };
	uint32_t mediaSsrc{ 0x0330bdee };
	uint32_t ssrc{ 0x02d03702 };
	uint64_t bitrate{ 365504 };
	uint16_t overhead{ 0 };

	void verify(FeedbackRtpTmmbrPacket* packet)
	{
		REQUIRE(packet->GetSenderSsrc() == senderSsrc);
		REQUIRE(packet->GetMediaSsrc() == mediaSsrc);

		auto it   = packet->Begin();
		auto item = *it;

		REQUIRE(item);
		REQUIRE(item->GetSsrc() == ssrc);
		REQUIRE(item->GetBitrate() == bitrate);
		REQUIRE(item->GetOverhead() == overhead);
	}
} // namespace TestFeedbackRtpTmmbr

SCENARIO("RTCP Feeback RTP TMMBR parsing", "[parser][rtcp][feedback-rtp][tmmb]")
{
	using namespace TestFeedbackRtpTmmbr;

	SECTION("parse FeedbackRtpTmmbrPacket")
	{
		std::unique_ptr<FeedbackRtpTmmbrPacket> packet{ FeedbackRtpTmmbrPacket::Parse(
			buffer, sizeof(buffer)) };

		REQUIRE(packet);

		verify(packet.get());

		SECTION("serialize packet instance")
		{
			uint8_t serialized[sizeof(buffer)] = { 0 };

			packet->Serialize(serialized);

			// NOTE: Do not compare byte by byte since different binary values can
			// represent the same content.
			SECTION("create a packet out of the serialized buffer")
			{
				std::unique_ptr<FeedbackRtpTmmbrPacket> packet{ FeedbackRtpTmmbrPacket::Parse(
					buffer, sizeof(buffer)) };

				verify(packet.get());
			}
		}
	}

	SECTION("create FeedbackRtpTmmbrPacket")
	{
		FeedbackRtpTmmbrPacket packet(senderSsrc, mediaSsrc);
		auto* item = new FeedbackRtpTmmbrItem();

		item->SetSsrc(ssrc);
		item->SetBitrate(bitrate);
		item->SetOverhead(overhead);

		packet.AddItem(item);

		verify(&packet);
	}
}
