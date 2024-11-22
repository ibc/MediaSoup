#include "common.hpp"
#include "RTC/RTCP/SenderReport.hpp"
#include <catch2/catch_test_macros.hpp>
#include <cstring> // std::memcmp()

using namespace RTC::RTCP;

namespace TestSenderReport
{
	// RTCP Packet. Sender Report and Receiver Report.

	// clang-format off
	uint8_t buffer[] =
	{
		0x80, 0xc8, 0x00, 0x06, // Type: 200 (Sender Report), Count: 0, Length: 6
		0x5d, 0x93, 0x15, 0x34, // SSRC: 0x5d931534
		0xdd, 0x3a, 0xc1, 0xb4, // NTP Sec: 3711615412
		0x76, 0x54, 0x71, 0x71, // NTP Frac: 1985245553
		0x00, 0x08, 0xcf, 0x00, // RTP timestamp: 577280
		0x00, 0x00, 0x0e, 0x18, // Packet count: 3608
		0x00, 0x08, 0xcf, 0x00  // Octet count: 577280
	};
	// clang-format on

	// Sender Report buffer start point.
	uint8_t* srBuffer = buffer + Packet::CommonHeaderSize;

	// SR values.
	uint32_t ssrc{ 0x5d931534 };
	uint32_t ntpSec{ 3711615412 };
	uint32_t ntpFrac{ 1985245553 };
	uint32_t rtpTs{ 577280 };
	uint32_t packetCount{ 3608 };
	uint32_t octetCount{ 577280 };

	void verify(SenderReport* report)
	{
		REQUIRE(report->GetSsrc() == ssrc);
		REQUIRE(report->GetNtpSec() == ntpSec);
		REQUIRE(report->GetNtpFrac() == ntpFrac);
		REQUIRE(report->GetRtpTs() == rtpTs);
		REQUIRE(report->GetPacketCount() == packetCount);
		REQUIRE(report->GetOctetCount() == octetCount);
	}
} // namespace TestSenderReport

using namespace TestSenderReport;

SCENARIO("RTCP SR parsing", "[parser][rtcp][sr]")
{
	SECTION("parse SR packet")
	{
		std::unique_ptr<SenderReportPacket> packet{ SenderReportPacket::Parse(buffer, sizeof(buffer)) };

		auto* report = *(packet->Begin());

		verify(report);

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

	SECTION("parse SR")
	{
		std::unique_ptr<SenderReport> report{ SenderReport::Parse(srBuffer, SenderReport::HeaderSize) };

		REQUIRE(report);

		verify(report.get());

		SECTION("serialize SenderReport instance")
		{
			uint8_t serialized[SenderReport::HeaderSize] = { 0 };

			report->Serialize(serialized);

			SECTION("compare serialized SenderReport with original buffer")
			{
				REQUIRE(std::memcmp(srBuffer, serialized, SenderReport::HeaderSize) == 0);
			}
		}
	}

	SECTION("create SR packet multiple reports")
	{
		const size_t count = 3;

		SenderReportPacket packet;

		for (auto i = 1; i <= count; i++)
		{
			// Create report and add to packet.
			SenderReport* report = new SenderReport();

			report->SetSsrc(i);
			report->SetNtpSec(i);
			report->SetNtpFrac(i);
			report->SetRtpTs(i);
			report->SetPacketCount(i);
			report->SetOctetCount(i);

			packet.AddReport(report);
		}

		uint8_t buffer[1500] = { 0 };

		// Serialization must contain 3 SR packets.
		packet.Serialize(buffer);

		SenderReport* reports[count]{ nullptr };

		std::unique_ptr<SenderReportPacket> packet2{ static_cast<SenderReportPacket*>(
			Packet::Parse(buffer, sizeof(buffer))) };

		REQUIRE(packet2 != nullptr);

		reports[0] = *(packet2->Begin());

		auto* packet3 = static_cast<SenderReportPacket*>(packet2->GetNext());

		REQUIRE(packet3 != nullptr);

		reports[1] = *(packet3->Begin());

		auto* packet4 = static_cast<SenderReportPacket*>(packet3->GetNext());

		REQUIRE(packet4 != nullptr);

		reports[2] = *(packet4->Begin());

		for (auto i = 1; i <= count; i++)
		{
			auto* report = reports[i - 1];

			REQUIRE(report != nullptr);

			REQUIRE(report->GetSsrc() == i);
			REQUIRE(report->GetNtpSec() == i);
			REQUIRE(report->GetNtpFrac() == i);
			REQUIRE(report->GetRtpTs() == i);
			REQUIRE(report->GetPacketCount() == i);
			REQUIRE(report->GetOctetCount() == i);
		}

		delete packet3;
		delete packet4;
	}

	SECTION("create SR")
	{
		// Create local report and check content.
		SenderReport report1;

		report1.SetSsrc(ssrc);
		report1.SetNtpSec(ntpSec);
		report1.SetNtpFrac(ntpFrac);
		report1.SetRtpTs(rtpTs);
		report1.SetPacketCount(packetCount);
		report1.SetOctetCount(octetCount);

		verify(&report1);

		SECTION("create a report out of the existing one")
		{
			SenderReport report2(&report1);

			verify(&report2);
		}
	}
}
