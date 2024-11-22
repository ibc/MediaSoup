#include "common.hpp"
#include "RTC/RtpPacket.hpp"
#include "RTC/RtpRetransmissionBuffer.hpp"
#include <catch2/catch_test_macros.hpp>
#include <vector>

using namespace RTC;

// Class inheriting from RtpRetransmissionBuffer so we can access its protected
// buffer member.
class RtpMyRetransmissionBuffer : public RtpRetransmissionBuffer
{
public:
	struct VerificationItem
	{
		bool isPresent;
		uint16_t sequenceNumber;
		uint32_t timestamp;
	};

public:
	RtpMyRetransmissionBuffer(uint16_t maxItems, uint32_t maxRetransmissionDelayMs, uint32_t clockRate)
	  : RtpRetransmissionBuffer(maxItems, maxRetransmissionDelayMs, clockRate)
	{
	}

public:
	void Insert(uint16_t seq, uint32_t timestamp)
	{
		// clang-format off
		uint8_t rtpBuffer[] =
		{
			0b10000000, 0b01111011, 0b01010010, 0b00001110,
			0b01011011, 0b01101011, 0b11001010, 0b10110101,
			0, 0, 0, 2
		};
		// clang-format on

		std::unique_ptr<RtpPacket> packet{ RtpPacket::Parse(rtpBuffer, sizeof(rtpBuffer)) };

		packet->SetSequenceNumber(seq);
		packet->SetTimestamp(timestamp);

		std::shared_ptr<RtpPacket> sharedPacket;

		RtpRetransmissionBuffer::Insert(packet.get(), sharedPacket);
	}

	void AssertBuffer(std::vector<VerificationItem> verificationBuffer)
	{
		REQUIRE(verificationBuffer.size() == this->buffer.size());

		for (size_t idx{ 0u }; idx < verificationBuffer.size(); ++idx)
		{
			auto& verificationItem = verificationBuffer.at(idx);
			auto* item             = this->buffer.at(idx);

			REQUIRE(verificationItem.isPresent == !!item);

			if (item)
			{
				REQUIRE(verificationItem.sequenceNumber == item->sequenceNumber);
				REQUIRE(verificationItem.timestamp == item->timestamp);
			}
		}
	}
};

SCENARIO("RtpRetransmissionBuffer", "[rtp][rtx]")
{
	SECTION("proper packets received in order")
	{
		uint16_t maxItems{ 4 };
		uint32_t maxRetransmissionDelayMs{ 2000u };
		uint32_t clockRate{ 90000 };

		RtpMyRetransmissionBuffer myRetransmissionBuffer(maxItems, maxRetransmissionDelayMs, clockRate);

		myRetransmissionBuffer.Insert(10001, 1000000000);
		myRetransmissionBuffer.Insert(10002, 1000000000);
		myRetransmissionBuffer.Insert(10003, 1000000200);
		myRetransmissionBuffer.Insert(10004, 1000000200);

		// clang-format off
		myRetransmissionBuffer.AssertBuffer(
			{
				{ true, 10001, 1000000000 },
				{ true, 10002, 1000000000 },
				{ true, 10003, 1000000200 },
				{ true, 10004, 1000000200 }
			}
		);
		// clang-format on
	}

	SECTION("proper packets received out of order")
	{
		uint16_t maxItems{ 4 };
		uint32_t maxRetransmissionDelayMs{ 2000u };
		uint32_t clockRate{ 90000 };

		RtpMyRetransmissionBuffer myRetransmissionBuffer(maxItems, maxRetransmissionDelayMs, clockRate);

		myRetransmissionBuffer.Insert(20004, 2000000200);
		myRetransmissionBuffer.Insert(20001, 2000000000);
		myRetransmissionBuffer.Insert(20003, 2000000200);
		myRetransmissionBuffer.Insert(20002, 2000000000);

		// clang-format off
		myRetransmissionBuffer.AssertBuffer(
			{
				{ true, 20001, 2000000000 },
				{ true, 20002, 2000000000 },
				{ true, 20003, 2000000200 },
				{ true, 20004, 2000000200 }
			}
		);
		// clang-format on
	}

	SECTION("packet with too new sequence number produces buffer emptying")
	{
		uint16_t maxItems{ 4 };
		uint32_t maxRetransmissionDelayMs{ 2000u };
		uint32_t clockRate{ 90000 };

		RtpMyRetransmissionBuffer myRetransmissionBuffer(maxItems, maxRetransmissionDelayMs, clockRate);

		myRetransmissionBuffer.Insert(30001, 3000000000);
		myRetransmissionBuffer.Insert(30002, 3000000000);
		myRetransmissionBuffer.Insert(30003, 3000000200);
		myRetransmissionBuffer.Insert(40000, 3000003000);

		// clang-format off
		myRetransmissionBuffer.AssertBuffer(
			{
				{ true, 40000, 3000003000 }
			}
		);
		// clang-format on
	}

	SECTION("blank slots are properly created")
	{
		uint16_t maxItems{ 10 };
		uint32_t maxRetransmissionDelayMs{ 2000u };
		uint32_t clockRate{ 90000 };

		RtpMyRetransmissionBuffer myRetransmissionBuffer(maxItems, maxRetransmissionDelayMs, clockRate);

		myRetransmissionBuffer.Insert(40002, 4000000002);
		// Packet must be discarded since its timestamp is lower than in seq 40002.
		myRetransmissionBuffer.Insert(40003, 4000000001);
		// Must produce 1 blank slot.
		myRetransmissionBuffer.Insert(40004, 4000000004);
		// Discarded (duplicated).
		myRetransmissionBuffer.Insert(40002, 4000000002);
		// Must produce 4 blank slot.
		myRetransmissionBuffer.Insert(40008, 4000000008);
		myRetransmissionBuffer.Insert(40006, 4000000006);
		// Must produce 1 blank slot at the front.
		myRetransmissionBuffer.Insert(40000, 4000000000);

		// clang-format off
		myRetransmissionBuffer.AssertBuffer(
			{
				{ true, 40000, 4000000000 },
				{ false, 0, 0 },
				{ true, 40002, 4000000002 },
				{ false, 0, 0 },
				{ true, 40004, 4000000004 },
				{ false, 0, 0 },
				{ true, 40006, 4000000006 },
				{ false, 0, 0 },
				{ true, 40008, 4000000008 }
			}
		);
		// clang-format on
	}

	SECTION("packet with too old sequence number is discarded")
	{
		uint16_t maxItems{ 4 };
		uint32_t maxRetransmissionDelayMs{ 2000u };
		uint32_t clockRate{ 90000 };

		RtpMyRetransmissionBuffer myRetransmissionBuffer(maxItems, maxRetransmissionDelayMs, clockRate);

		myRetransmissionBuffer.Insert(10001, 1000000001);
		myRetransmissionBuffer.Insert(10002, 1000000002);
		myRetransmissionBuffer.Insert(10003, 1000000003);
		// Too old seq.
		myRetransmissionBuffer.Insert(40000, 1000000000);

		// clang-format off
		myRetransmissionBuffer.AssertBuffer(
			{
				{ true, 10001, 1000000001 },
				{ true, 10002, 1000000002 },
				{ true, 10003, 1000000003 }
			}
		);
		// clang-format on
	}

	SECTION("packet with too old timestamp is discarded")
	{
		uint16_t maxItems{ 4 };
		uint32_t maxRetransmissionDelayMs{ 2000u };
		uint32_t clockRate{ 90000 };

		RtpMyRetransmissionBuffer myRetransmissionBuffer(maxItems, maxRetransmissionDelayMs, clockRate);

		auto maxDiffTs = static_cast<uint32_t>(maxRetransmissionDelayMs * clockRate / 1000);

		myRetransmissionBuffer.Insert(10001, 1000000001);
		myRetransmissionBuffer.Insert(10002, 1000000002);
		myRetransmissionBuffer.Insert(10003, 1000000003);
		// Too old timestamp (subtract 100 to avoid math issues).
		myRetransmissionBuffer.Insert(10000, 1000000003 - maxDiffTs - 100);

		// clang-format off
		myRetransmissionBuffer.AssertBuffer(
			{
				{ true, 10001, 1000000001 },
				{ true, 10002, 1000000002 },
				{ true, 10003, 1000000003 }
			}
		);
		// clang-format on
	}

	SECTION("packet with very newest timestamp is inserted as newest item despite its seq is old")
	{
		uint16_t maxItems{ 4 };
		uint32_t maxRetransmissionDelayMs{ 2000u };
		uint32_t clockRate{ 90000 };

		RtpMyRetransmissionBuffer myRetransmissionBuffer(maxItems, maxRetransmissionDelayMs, clockRate);

		// Scenario based on https://github.com/versatica/mediasoup/issues/1037.

		myRetransmissionBuffer.Insert(24816, 1024930187);
		myRetransmissionBuffer.Insert(24980, 1025106407);
		myRetransmissionBuffer.Insert(18365, 1026593387);

		// clang-format off
		myRetransmissionBuffer.AssertBuffer(
			{
				{ true, 18365, 1026593387 }
			}
		);
		// clang-format on
	}

	SECTION(
	  "packet with lower seq than newest packet in the buffer and higher timestamp forces buffer emptying")
	{
		uint16_t maxItems{ 4 };
		uint32_t maxRetransmissionDelayMs{ 2000u };
		uint32_t clockRate{ 90000 };

		RtpMyRetransmissionBuffer myRetransmissionBuffer(maxItems, maxRetransmissionDelayMs, clockRate);

		myRetransmissionBuffer.Insert(33331, 1000000001);
		myRetransmissionBuffer.Insert(33332, 1000000002);
		myRetransmissionBuffer.Insert(33330, 1000000003);

		// clang-format off
		myRetransmissionBuffer.AssertBuffer(
			{
				{ true, 33330, 1000000003 }
			}
		);
		// clang-format on
	}

	SECTION("fuzzer generated packets")
	{
		uint16_t maxItems{ 2500u };
		uint32_t maxRetransmissionDelayMs{ 2000u };
		uint32_t clockRate{ 90000 };

		RtpMyRetransmissionBuffer myRetransmissionBuffer(maxItems, maxRetransmissionDelayMs, clockRate);

		// These packets reproduce an already fixed crash reported here:
		// https://github.com/versatica/mediasoup/issues/1027#issuecomment-1478464584
		// I've commented first packets and just left those that produce the crash.

		// myRetransmissionBuffer.Insert(14906, 976891962);
		// myRetransmissionBuffer.Insert(14906, 976891962);
		// myRetransmissionBuffer.Insert(14906, 976892730);
		// myRetransmissionBuffer.Insert(13157, 862283031);
		// myRetransmissionBuffer.Insert(13114, 859453491);
		// myRetransmissionBuffer.Insert(14906, 976892264);
		// myRetransmissionBuffer.Insert(14906, 976897098);
		// myRetransmissionBuffer.Insert(13114, 859464290);
		// myRetransmissionBuffer.Insert(14906, 976889088);
		// myRetransmissionBuffer.Insert(13056, 855638184);
		// myRetransmissionBuffer.Insert(14906, 976891950);
		// myRetransmissionBuffer.Insert(17722, 1161443894);
		// myRetransmissionBuffer.Insert(12846, 841888049);
		// myRetransmissionBuffer.Insert(14906, 976905830);
		// myRetransmissionBuffer.Insert(15677, 1027420485);
		// myRetransmissionBuffer.Insert(33742, 2211317269);
		// myRetransmissionBuffer.Insert(14906, 976892672);
		// myRetransmissionBuffer.Insert(13102, 858665774);
		// myRetransmissionBuffer.Insert(12850, 842150702);
		// myRetransmissionBuffer.Insert(14906, 976891941);
		// myRetransmissionBuffer.Insert(15677, 1027423549);
		// myRetransmissionBuffer.Insert(12346, 809120580);
		// myRetransmissionBuffer.Insert(12645, 828715313);
		myRetransmissionBuffer.Insert(12645, 828702743);
		myRetransmissionBuffer.Insert(33998, 2228092928);
		myRetransmissionBuffer.Insert(33998, 2228092928);
	}
}
