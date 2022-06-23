#include "common.hpp"
#include "RTC/RTCP/FeedbackRtpNack.hpp"
#include "RTC/RtpPacket.hpp"
#include "RTC/RtpStream.hpp"
#include "RTC/RtpStreamSend.hpp"
#include <catch2/catch.hpp>
#include <vector>

using namespace RTC;

static std::shared_ptr<RtpPacket> CreateRtpPacket(uint8_t* buffer, uint16_t seq, uint32_t timestamp)
{
	auto* packet = RtpPacket::Parse(buffer, 1500);

	packet->SetSequenceNumber(seq);
	packet->SetTimestamp(timestamp);

	std::shared_ptr<RtpPacket> shared(packet);

	return shared;
}

SCENARIO("NACK and RTP packets retransmission", "[rtp][rtcp]")
{
	class TestRtpStreamListener : public RtpStreamSend::Listener
	{
	public:
		void OnRtpStreamScore(RtpStream* /*rtpStream*/, uint8_t /*score*/, uint8_t /*previousScore*/) override
		{
		}

		void OnRtpStreamRetransmitRtpPacket(RtpStreamSend* /*rtpStream*/, RtpPacket* packet) override
		{
			this->retransmittedPackets.push_back(packet);
		}

	public:
		std::vector<RtpPacket*> retransmittedPackets;
	};

	TestRtpStreamListener testRtpStreamListener;

	SECTION("receive NACK and get retransmitted packets")
	{
		// clang-format off
		uint8_t rtpBuffer1[] =
		{
			0b10000000, 0b01111011, 0b01010010, 0b00001110,
			0b01011011, 0b01101011, 0b11001010, 0b10110101,
			0, 0, 0, 2
		};
		// clang-format on

		uint8_t rtpBuffer2[1500];
		uint8_t rtpBuffer3[1500];
		uint8_t rtpBuffer4[1500];
		uint8_t rtpBuffer5[1500];

		std::memcpy(rtpBuffer2, rtpBuffer1, sizeof(rtpBuffer1));
		std::memcpy(rtpBuffer3, rtpBuffer1, sizeof(rtpBuffer1));
		std::memcpy(rtpBuffer4, rtpBuffer1, sizeof(rtpBuffer1));
		std::memcpy(rtpBuffer5, rtpBuffer1, sizeof(rtpBuffer1));

		// packet1 [pt:123, seq:21006, timestamp:1533790901]
		auto packet1 = CreateRtpPacket(rtpBuffer1, 21006, 1533790901);
		// packet2 [pt:123, seq:21007, timestamp:1533790901]
		auto packet2 = CreateRtpPacket(rtpBuffer2, 21007, 1533790901);
		// packet3 [pt:123, seq:21008, timestamp:1533793871]
		auto packet3 = CreateRtpPacket(rtpBuffer3, 21008, 1533793871);
		// packet4 [pt:123, seq:21009, timestamp:1533793871]
		auto packet4 = CreateRtpPacket(rtpBuffer4, 21009, 1533793871);
		// packet5 [pt:123, seq:21010, timestamp:1533796931]
		auto packet5 = CreateRtpPacket(rtpBuffer5, 21010, 1533796931);

		RtpStream::Params params;

		params.ssrc      = packet1->GetSsrc();
		params.clockRate = 90000;
		params.useNack   = true;

		// Create a RtpStreamSend.
		std::string mid{ "" };
		RtpStreamSend* stream = new RtpStreamSend(&testRtpStreamListener, params, mid);

		// Receive all the packets (some of them not in order and/or duplicated).
		stream->ReceivePacket(packet1);
		stream->ReceivePacket(packet3);
		stream->ReceivePacket(packet2);
		stream->ReceivePacket(packet3);
		stream->ReceivePacket(packet4);
		stream->ReceivePacket(packet4);
		stream->ReceivePacket(packet5);
		stream->ReceivePacket(packet5);

		// Create a NACK item that request for all the packets.
		RTCP::FeedbackRtpNackPacket nackPacket(0, params.ssrc);
		auto* nackItem = new RTCP::FeedbackRtpNackItem(21006, 0b0000000000001111);

		nackPacket.AddItem(nackItem);

		REQUIRE(nackItem->GetPacketId() == 21006);
		REQUIRE(nackItem->GetLostPacketBitmask() == 0b0000000000001111);

		stream->ReceiveNack(&nackPacket);

		REQUIRE(testRtpStreamListener.retransmittedPackets.size() == 5);

		auto rtxPacket1 = testRtpStreamListener.retransmittedPackets[0];
		auto rtxPacket2 = testRtpStreamListener.retransmittedPackets[1];
		auto rtxPacket3 = testRtpStreamListener.retransmittedPackets[2];
		auto rtxPacket4 = testRtpStreamListener.retransmittedPackets[3];
		auto rtxPacket5 = testRtpStreamListener.retransmittedPackets[4];

		testRtpStreamListener.retransmittedPackets.clear();

		REQUIRE(rtxPacket1);
		REQUIRE(rtxPacket1->GetSequenceNumber() == packet1->GetSequenceNumber());
		REQUIRE(rtxPacket1->GetTimestamp() == packet1->GetTimestamp());

		REQUIRE(rtxPacket2);
		REQUIRE(rtxPacket2->GetSequenceNumber() == packet2->GetSequenceNumber());
		REQUIRE(rtxPacket2->GetTimestamp() == packet2->GetTimestamp());

		REQUIRE(rtxPacket3);
		REQUIRE(rtxPacket3->GetSequenceNumber() == packet3->GetSequenceNumber());
		REQUIRE(rtxPacket3->GetTimestamp() == packet3->GetTimestamp());

		REQUIRE(rtxPacket4);
		REQUIRE(rtxPacket4->GetSequenceNumber() == packet4->GetSequenceNumber());
		REQUIRE(rtxPacket4->GetTimestamp() == packet4->GetTimestamp());

		REQUIRE(rtxPacket5);
		REQUIRE(rtxPacket5->GetSequenceNumber() == packet5->GetSequenceNumber());
		REQUIRE(rtxPacket5->GetTimestamp() == packet5->GetTimestamp());

		delete stream;
	}
}
