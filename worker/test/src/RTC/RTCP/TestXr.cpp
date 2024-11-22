#include "common.hpp"
#include "RTC/RTCP/XR.hpp"
#include "RTC/RTCP/XrDelaySinceLastRr.hpp"
#include "RTC/RTCP/XrReceiverReferenceTime.hpp"
#include <catch2/catch_test_macros.hpp>
#include <cstring> // std::memcmp(), std::memcpy()

using namespace RTC::RTCP;

SCENARIO("RTCP XR parsing", "[parser][rtcp][xr]")
{
	// clang-format off
	uint8_t buffer[] =
	{
		0xa0, 0xcf, 0x00, 0x09, // Padding, Type: 207 (XR), Length: 9
		0x5d, 0x93, 0x15, 0x34, // Sender SSRC: 0x5d931534
		// Extended Report DLRR
		0x05, 0x00, 0x00, 0x06, // BT: 5 (DLRR), Block Length: 6
		0x11, 0x12, 0x13, 0x14, // SSRC 1
		0x00, 0x11, 0x00, 0x11, // LRR 1
		0x11, 0x00, 0x11, 0x00, // DLRR 1
		0x21, 0x22, 0x23, 0x24, // SSRC 2
		0x00, 0x22, 0x00, 0x22, // LRR 2
		0x22, 0x00, 0x22, 0x00, // DLRR 2
		0x00, 0x00, 0x00, 0x04 // Padding (4 bytes)
	};
	// clang-format on

	SECTION("parse XR packet")
	{
		std::unique_ptr<ExtendedReportPacket> packet(ExtendedReportPacket::Parse(buffer, sizeof(buffer)));

		REQUIRE(packet);
		// Despite total buffer size is 40 bytes, our GetSize() method doesn't not
		// consider RTCP padding (4 bytes in this case).
		// https://github.com/versatica/mediasoup/issues/1233
		REQUIRE(packet->GetSize() == 36);
		REQUIRE(packet->GetCount() == 0);
		REQUIRE(packet->GetSsrc() == 0x5d931534);

		size_t blockIdx{ 0u };

		for (auto it = packet->Begin(); it != packet->End(); ++it, ++blockIdx)
		{
			auto* block = *it;

			switch (blockIdx)
			{
				case 0:
				{
					REQUIRE(block->GetSize() == 28);

					size_t ssrcInfoIdx{ 0u };
					auto* dlrrBlock = reinterpret_cast<DelaySinceLastRr*>(block);

					for (auto it2 = dlrrBlock->Begin(); it2 != dlrrBlock->End(); ++it2, ++ssrcInfoIdx)
					{
						auto* ssrcInfo = *it2;

						switch (ssrcInfoIdx)
						{
							case 0:
							{
								REQUIRE(ssrcInfo->GetSsrc() == 0x11121314);
								REQUIRE(ssrcInfo->GetLastReceiverReport() == 0x00110011);
								REQUIRE(ssrcInfo->GetDelaySinceLastReceiverReport() == 0x11001100);

								break;
							}

							case 1:
							{
								REQUIRE(ssrcInfo->GetSsrc() == 0x21222324);
								REQUIRE(ssrcInfo->GetLastReceiverReport() == 0x00220022);
								REQUIRE(ssrcInfo->GetDelaySinceLastReceiverReport() == 0x22002200);

								break;
							}
						}
					}

					// There are 2 SSRC infos.
					REQUIRE(ssrcInfoIdx == 2);

					break;
				}
			}
		}

		// There are 1 block (the DLRR block).
		REQUIRE(blockIdx == 1);

		SECTION("serialize packet instance")
		{
			// NOTE: Padding in RTCP is removed (if not needed) when serializing the
			// packet, so we must mangle the buffer content (padding bit) and the
			// buffer length before comparing the serialized packet with and original
			// buffer.

			const size_t paddingBytes{ 4 };
			const size_t serializedBufferLength        = sizeof(buffer) - paddingBytes;
			uint8_t serialized[serializedBufferLength] = { 0 };

			// Clone the original buffer into a new buffer without padding.
			uint8_t clonedBuffer[serializedBufferLength] = { 0 };
			std::memcpy(clonedBuffer, buffer, serializedBufferLength);

			// Remove the padding bit in the first byte of the cloned buffer.
			clonedBuffer[0] = 0x80;

			// Change RTCP length field in the cloned buffer.
			clonedBuffer[3] = clonedBuffer[3] - 1;

			packet->Serialize(serialized);

			std::unique_ptr<ExtendedReportPacket> packet2(
			  ExtendedReportPacket::Parse(serialized, serializedBufferLength));

			REQUIRE(packet2->GetType() == Type::XR);
			REQUIRE(packet2->GetCount() == 0);
			REQUIRE(packet2->GetSize() == 36);

			REQUIRE(std::memcmp(clonedBuffer, serialized, serializedBufferLength) == 0);
		}
	}
}

SCENARIO("RTCP XrDelaySinceLastRt parsing", "[parser][rtcp][xr-dlrr]")
{
	SECTION("create RRT")
	{
		// Create local report and check content.
		// NOTE: We cannot use unique_ptr here since the instance lifecycle will be
		// managed by the packet.
		auto* report1 = new ReceiverReferenceTime();

		report1->SetNtpSec(11111111);
		report1->SetNtpFrac(22222222);

		REQUIRE(report1->GetType() == ExtendedReportBlock::Type::RRT);
		REQUIRE(report1->GetNtpSec() == 11111111);
		REQUIRE(report1->GetNtpFrac() == 22222222);

		// Serialize the report into an external buffer.
		uint8_t bufferReport1[256]{ 0 };

		report1->Serialize(bufferReport1);

		// Create a new report out of the external buffer.
		// NOTE: We cannot use unique_ptr here since the instance lifecycle will be
		// managed by the packet.
		auto report2 = ReceiverReferenceTime::Parse(bufferReport1, report1->GetSize());

		REQUIRE(report1->GetType() == report2->GetType());
		REQUIRE(report1->GetNtpSec() == report2->GetNtpSec());
		REQUIRE(report1->GetNtpFrac() == report2->GetNtpFrac());

		// Create a local packet.
		std::unique_ptr<ExtendedReportPacket> packet1(new ExtendedReportPacket());

		packet1->SetSsrc(2222);
		packet1->AddReport(report1);
		packet1->AddReport(report2);

		REQUIRE(packet1->GetType() == Type::XR);
		REQUIRE(packet1->GetCount() == 0);
		REQUIRE(packet1->GetSsrc() == 2222);

		// Total size:
		// -  RTCP common header
		// -  SSRC
		// -  block 1
		// -  block 2
		REQUIRE(packet1->GetSize() == 4 + 4 + 12 + 12);

		// Serialize the packet into an external buffer.
		uint8_t bufferPacket1[256]{ 0 };
		uint8_t bufferPacket2[256]{ 0 };

		packet1->Serialize(bufferPacket1);

		// Create a new packet out of the external buffer.
		std::unique_ptr<ExtendedReportPacket> packet2(
		  ExtendedReportPacket::Parse(bufferPacket1, packet1->GetSize()));

		REQUIRE(packet2->GetType() == packet1->GetType());
		REQUIRE(packet2->GetCount() == packet1->GetCount());
		REQUIRE(packet2->GetSsrc() == packet1->GetSsrc());
		REQUIRE(packet2->GetSize() == packet1->GetSize());

		packet2->Serialize(bufferPacket2);

		REQUIRE(std::memcmp(bufferPacket1, bufferPacket2, packet1->GetSize()) == 0);
	}

	SECTION("create DLRR")
	{
		// Create local report and check content.
		// NOTE: We cannot use unique_ptr here since the instance lifecycle will be
		// managed by the packet.
		auto* report1 = new DelaySinceLastRr();
		// NOTE: We cannot use unique_ptr here since the instance lifecycle will be
		// managed by the report.
		auto* ssrcInfo1 = new DelaySinceLastRr::SsrcInfo();

		ssrcInfo1->SetSsrc(1234);
		ssrcInfo1->SetLastReceiverReport(11111111);
		ssrcInfo1->SetDelaySinceLastReceiverReport(22222222);

		REQUIRE(ssrcInfo1->GetSsrc() == 1234);
		REQUIRE(ssrcInfo1->GetLastReceiverReport() == 11111111);
		REQUIRE(ssrcInfo1->GetDelaySinceLastReceiverReport() == 22222222);
		REQUIRE(ssrcInfo1->GetSize() == sizeof(DelaySinceLastRr::SsrcInfo::Body));

		report1->AddSsrcInfo(ssrcInfo1);

		// Serialize the report into an external buffer.
		uint8_t bufferReport1[256]{ 0 };

		report1->Serialize(bufferReport1);

		// Create a new report out of the external buffer.
		// NOTE: We cannot use unique_ptr here since the instance lifecycle will be
		// managed by the packet.
		auto report2 = DelaySinceLastRr::Parse(bufferReport1, report1->GetSize());

		REQUIRE(report1->GetType() == report2->GetType());

		auto ssrcInfoIt = report2->Begin();
		auto* ssrcInfo2 = *ssrcInfoIt;

		REQUIRE(ssrcInfo1->GetSsrc() == ssrcInfo2->GetSsrc());
		REQUIRE(ssrcInfo1->GetLastReceiverReport() == ssrcInfo2->GetLastReceiverReport());
		REQUIRE(
		  ssrcInfo1->GetDelaySinceLastReceiverReport() == ssrcInfo2->GetDelaySinceLastReceiverReport());
		REQUIRE(ssrcInfo1->GetSize() == ssrcInfo2->GetSize());

		// Create a local packet.
		std::unique_ptr<ExtendedReportPacket> packet1(new ExtendedReportPacket());

		packet1->SetSsrc(2222);
		packet1->AddReport(report1);
		packet1->AddReport(report2);

		REQUIRE(packet1->GetType() == Type::XR);
		REQUIRE(packet1->GetCount() == 0);
		REQUIRE(packet1->GetSsrc() == 2222);

		// Total size:
		// -  RTCP common header
		// -  SSRC
		// -  block 1
		// -  block 2
		REQUIRE(packet1->GetSize() == 4 + 4 + 16 + 16);

		// Serialize the packet into an external buffer.
		uint8_t bufferPacket1[256]{ 0 };
		uint8_t bufferPacket2[256]{ 0 };

		packet1->Serialize(bufferPacket1);

		// Create a new packet out of the external buffer.
		std::unique_ptr<ExtendedReportPacket> packet2(
		  ExtendedReportPacket::Parse(bufferPacket1, packet1->GetSize()));

		REQUIRE(packet2->GetType() == packet1->GetType());
		REQUIRE(packet2->GetCount() == packet1->GetCount());
		REQUIRE(packet2->GetSsrc() == packet1->GetSsrc());
		REQUIRE(packet2->GetSize() == packet1->GetSize());

		packet2->Serialize(bufferPacket2);

		REQUIRE(std::memcmp(bufferPacket1, bufferPacket2, packet1->GetSize()) == 0);
	}
}
