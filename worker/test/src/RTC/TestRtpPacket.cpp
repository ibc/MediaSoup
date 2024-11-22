#include "common.hpp"
#include "helpers.hpp"
#include "RTC/RtpPacket.hpp"
#include <catch2/catch_test_macros.hpp>
#include <cstring> // std::memset()
#include <string>
#include <vector>

using namespace RTC;

static uint8_t buffer[65536];

SCENARIO("parse RTP packets", "[parser][rtp]")
{
	SECTION("parse packet1.raw")
	{
		size_t len;
		uint8_t extenLen;
		uint8_t* extenValue;
		std::string rid;

		if (!helpers::readBinaryFile("data/packet1.raw", buffer, &len))
		{
			FAIL("cannot open file");
		}

		std::unique_ptr<RtpPacket> packet{ RtpPacket::Parse(buffer, len) };

		if (!packet)
		{
			FAIL("not a RTP packet");
		}

		REQUIRE(packet->HasMarker() == false);
		REQUIRE(packet->HasHeaderExtension() == true);
		REQUIRE(packet->GetPayloadType() == 111);
		REQUIRE(packet->GetSequenceNumber() == 23617);
		REQUIRE(packet->GetTimestamp() == 1660241882);
		REQUIRE(packet->GetSsrc() == 2674985186);
		REQUIRE(packet->GetHeaderExtensionId() == 0xBEDE);
		REQUIRE(packet->GetHeaderExtensionLength() == 4);
		REQUIRE(packet->HasOneByteExtensions());
		REQUIRE(packet->HasTwoBytesExtensions() == false);

		packet->SetRidExtensionId(10);
		extenValue = packet->GetExtension(10, extenLen);

		REQUIRE(packet->HasExtension(10) == false);
		REQUIRE(extenLen == 0);
		REQUIRE(extenValue == nullptr);
		REQUIRE(packet->ReadRid(rid) == false);
		REQUIRE(rid == "");
	}

	SECTION("parse packet2.raw")
	{
		size_t len;

		if (!helpers::readBinaryFile("data/packet2.raw", buffer, &len))
		{
			FAIL("cannot open file");
		}

		std::unique_ptr<RtpPacket> packet{ RtpPacket::Parse(buffer, len) };

		if (!packet)
		{
			FAIL("not a RTP packet");
		}

		REQUIRE(packet->HasMarker() == false);
		REQUIRE(packet->HasHeaderExtension() == false);
		REQUIRE(packet->GetPayloadType() == 100);
		REQUIRE(packet->GetSequenceNumber() == 28478);
		REQUIRE(packet->GetTimestamp() == 172320136);
		REQUIRE(packet->GetSsrc() == 3316375386);
		REQUIRE(packet->GetHeaderExtensionId() == 0);
		REQUIRE(packet->GetHeaderExtensionLength() == 0);
		REQUIRE(packet->HasOneByteExtensions() == false);
		REQUIRE(packet->HasTwoBytesExtensions() == false);
	}

	SECTION("parse packet3.raw")
	{
		size_t len;
		uint8_t extenLen;
		uint8_t* extenValue;
		bool voice;
		uint8_t volume;
		uint32_t absSendTime;

		if (!helpers::readBinaryFile("data/packet3.raw", buffer, &len))
		{
			FAIL("cannot open file");
		}

		std::unique_ptr<RtpPacket> packet{ RtpPacket::Parse(buffer, len) };

		if (!packet)
		{
			FAIL("not a RTP packet");
		}

		REQUIRE(packet->HasMarker() == false);
		REQUIRE(packet->HasHeaderExtension() == true);
		REQUIRE(packet->GetPayloadType() == 111);
		REQUIRE(packet->GetSequenceNumber() == 19354);
		REQUIRE(packet->GetTimestamp() == 863466045);
		REQUIRE(packet->GetSsrc() == 235797202);
		REQUIRE(packet->GetHeaderExtensionId() == 0xBEDE);
		REQUIRE(packet->GetHeaderExtensionLength() == 8);
		REQUIRE(packet->HasOneByteExtensions());
		REQUIRE(packet->HasTwoBytesExtensions() == false);

		packet->SetSsrcAudioLevelExtensionId(1);
		extenValue = packet->GetExtension(1, extenLen);

		REQUIRE(packet->HasExtension(1) == true);
		REQUIRE(extenLen == 1);
		REQUIRE(extenValue);
		REQUIRE(extenValue[0] == 0xd0);
		REQUIRE(packet->ReadSsrcAudioLevel(volume, voice) == true);
		REQUIRE(volume == 0b1010000);
		REQUIRE(voice == true);

		packet->SetAbsSendTimeExtensionId(3);
		extenValue = packet->GetExtension(3, extenLen);

		REQUIRE(packet->HasExtension(3) == true);
		REQUIRE(extenLen == 3);
		REQUIRE(extenValue);
		REQUIRE(extenValue[0] == 0x65);
		REQUIRE(extenValue[1] == 0x34);
		REQUIRE(extenValue[2] == 0x1e);
		REQUIRE(packet->ReadAbsSendTime(absSendTime) == true);
		REQUIRE(absSendTime == 0x65341e);

		std::unique_ptr<RtpPacket> clonedPacket{ packet->Clone() };

		std::memset(buffer, '0', sizeof(buffer));

		REQUIRE(clonedPacket->HasMarker() == false);
		REQUIRE(clonedPacket->HasHeaderExtension() == true);
		REQUIRE(clonedPacket->GetPayloadType() == 111);
		REQUIRE(clonedPacket->GetSequenceNumber() == 19354);
		REQUIRE(clonedPacket->GetTimestamp() == 863466045);
		REQUIRE(clonedPacket->GetSsrc() == 235797202);
		REQUIRE(clonedPacket->GetHeaderExtensionId() == 0xBEDE);
		REQUIRE(clonedPacket->GetHeaderExtensionLength() == 8);
		REQUIRE(clonedPacket->HasOneByteExtensions());
		REQUIRE(clonedPacket->HasTwoBytesExtensions() == false);

		extenValue = clonedPacket->GetExtension(1, extenLen);

		REQUIRE(packet->HasExtension(1) == false);
		REQUIRE(extenLen == 1);
		REQUIRE(extenValue);
		REQUIRE(extenValue[0] == 0xd0);
		REQUIRE(clonedPacket->ReadSsrcAudioLevel(volume, voice) == true);
		REQUIRE(volume == 0b1010000);
		REQUIRE(voice == true);

		extenValue = clonedPacket->GetExtension(3, extenLen);

		REQUIRE(packet->HasExtension(3) == false);
		REQUIRE(extenLen == 3);
		REQUIRE(extenValue);
		REQUIRE(extenValue[0] == 0x65);
		REQUIRE(extenValue[1] == 0x34);
		REQUIRE(extenValue[2] == 0x1e);
		REQUIRE(clonedPacket->ReadAbsSendTime(absSendTime) == true);
		REQUIRE(absSendTime == 0x65341e);
	}

	SECTION("create RtpPacket without header extension")
	{
		// clang-format off
		uint8_t buffer[] =
		{
			0x80, 0x01, 0x00, 0x08,
			0x00, 0x00, 0x00, 0x04,
			0x00, 0x00, 0x00, 0x05
		};
		// clang-format on

		std::unique_ptr<RtpPacket> packet{ RtpPacket::Parse(buffer, sizeof(buffer)) };

		if (!packet)
		{
			FAIL("not a RTP packet");
		}

		REQUIRE(packet->HasMarker() == false);
		REQUIRE(packet->HasHeaderExtension() == false);
		REQUIRE(packet->GetPayloadType() == 1);
		REQUIRE(packet->GetSequenceNumber() == 8);
		REQUIRE(packet->GetTimestamp() == 4);
		REQUIRE(packet->HasOneByteExtensions() == false);
		REQUIRE(packet->HasTwoBytesExtensions() == false);
		REQUIRE(packet->GetSsrc() == 5);
	}

	SECTION("create RtpPacket with One-Byte header extension")
	{
		// clang-format off
		uint8_t buffer[] =
		{
			0x90, 0x01, 0x00, 0x08,
			0x00, 0x00, 0x00, 0x04,
			0x00, 0x00, 0x00, 0x05,
			0xbe, 0xde, 0x00, 0x03, // Header Extension
			0x10, 0xff, 0x21, 0xff,
			0xff, 0x00, 0x00, 0x33,
			0xff, 0xff, 0xff, 0xff
		};
		// clang-format on

		std::unique_ptr<RtpPacket> packet{ RtpPacket::Parse(buffer, sizeof(buffer)) };

		if (!packet)
		{
			FAIL("not a RTP packet");
		}

		REQUIRE(packet->HasMarker() == false);
		REQUIRE(packet->HasHeaderExtension() == true);
		REQUIRE(packet->GetPayloadType() == 1);
		REQUIRE(packet->GetSequenceNumber() == 8);
		REQUIRE(packet->GetTimestamp() == 4);
		REQUIRE(packet->GetSsrc() == 5);
		REQUIRE(packet->GetHeaderExtensionId() == 0xBEDE);
		REQUIRE(packet->GetHeaderExtensionLength() == 12);
		REQUIRE(packet->HasOneByteExtensions());
		REQUIRE(packet->HasTwoBytesExtensions() == false);
		REQUIRE(packet->GetPayloadLength() == 0);
		REQUIRE(packet->GetSize() == 28);

		packet->SetPayloadLength(1000);

		REQUIRE(packet->GetPayloadLength() == 1000);
		REQUIRE(packet->GetSize() == 1028);
	}

	SECTION("create RtpPacket with Two-Bytes header extension")
	{
		// clang-format off
		uint8_t buffer[] =
		{
			0x90, 0x01, 0x00, 0x08,
			0x00, 0x00, 0x00, 0x04,
			0x00, 0x00, 0x00, 0x05,
			0x10, 0x00, 0x00, 0x04, // Header Extension
			0x00, 0x00, 0x01, 0x00,
			0x02, 0x01, 0x42, 0x00,
			0x03, 0x02, 0x11, 0x22,
			0x00, 0x00, 0x04, 0x00
		};
		// clang-format on

		uint8_t extenLen;
		uint8_t* extenValue;

		std::unique_ptr<RtpPacket> packet{ RtpPacket::Parse(buffer, sizeof(buffer)) };

		if (!packet)
		{
			FAIL("not a RTP packet");
		}

		REQUIRE(packet->HasMarker() == false);
		REQUIRE(packet->HasHeaderExtension() == true);
		REQUIRE(packet->GetPayloadType() == 1);
		REQUIRE(packet->GetSequenceNumber() == 8);
		REQUIRE(packet->GetTimestamp() == 4);
		REQUIRE(packet->GetSsrc() == 5);
		REQUIRE(packet->GetHeaderExtensionLength() == 16);
		REQUIRE(packet->HasOneByteExtensions() == false);
		REQUIRE(packet->HasTwoBytesExtensions());
		REQUIRE(packet->GetPayloadLength() == 0);

		extenValue = packet->GetExtension(1, extenLen);
		REQUIRE(packet->HasExtension(1) == false);
		REQUIRE(extenValue == nullptr);
		REQUIRE(extenLen == 0);

		extenValue = packet->GetExtension(2, extenLen);
		REQUIRE(packet->HasExtension(2) == true);
		REQUIRE(extenValue != nullptr);
		REQUIRE(extenLen == 1);
		REQUIRE(extenValue[0] == 0x42);

		extenValue = packet->GetExtension(3, extenLen);
		REQUIRE(packet->HasExtension(3) == true);
		REQUIRE(extenValue != nullptr);
		REQUIRE(extenLen == 2);
		REQUIRE(extenValue[0] == 0x11);
		REQUIRE(extenValue[1] == 0x22);

		extenValue = packet->GetExtension(4, extenLen);
		REQUIRE(packet->HasExtension(4) == false);
		REQUIRE(extenValue == nullptr);
		REQUIRE(extenLen == 0);

		extenValue = packet->GetExtension(5, extenLen);
		REQUIRE(packet->HasExtension(5) == false);
		REQUIRE(extenValue == nullptr);
		REQUIRE(extenLen == 0);
	}

	SECTION("rtx encryption-decryption")
	{
		// clang-format off
		uint8_t buffer[] =
		{
			0x90, 0x01, 0x00, 0x08,
			0x00, 0x00, 0x00, 0x04,
			0x00, 0x00, 0x00, 0x05,
			0x10, 0x00, 0x00, 0x03, // Header Extension
			0x01, 0x00, 0x02, 0x01,
			0xff, 0x00, 0x03, 0x04,
			0xff, 0xff, 0xff, 0xff,
			0x11, 0x11, 0x11, 0x11 // payload
		};
		// clang-format on

		uint8_t rtxPayloadType{ 102 };
		uint32_t rtxSsrc{ 6 };
		uint16_t rtxSeq{ 80 };

		std::unique_ptr<RtpPacket> packet{ RtpPacket::Parse(buffer, sizeof(buffer)) };

		if (!packet)
		{
			FAIL("not a RTP packet");
		}

		REQUIRE(packet->HasMarker() == false);
		REQUIRE(packet->HasHeaderExtension() == true);
		REQUIRE(packet->GetPayloadType() == 1);
		REQUIRE(packet->GetSequenceNumber() == 8);
		REQUIRE(packet->GetTimestamp() == 4);
		REQUIRE(packet->GetSsrc() == 5);
		REQUIRE(packet->GetPayloadLength() == 4);
		REQUIRE(packet->GetHeaderExtensionLength() == 12);
		REQUIRE(packet->HasOneByteExtensions() == false);
		REQUIRE(packet->HasTwoBytesExtensions());

		std::unique_ptr<RtpPacket> rtxPacket{ packet->Clone() };

		std::memset(buffer, '0', sizeof(buffer));

		rtxPacket->RtxEncode(rtxPayloadType, rtxSsrc, rtxSeq);

		REQUIRE(rtxPacket->HasMarker() == false);
		REQUIRE(rtxPacket->HasHeaderExtension() == true);
		REQUIRE(rtxPacket->GetPayloadType() == rtxPayloadType);
		REQUIRE(rtxPacket->GetSequenceNumber() == rtxSeq);
		REQUIRE(rtxPacket->GetTimestamp() == 4);
		REQUIRE(rtxPacket->GetSsrc() == rtxSsrc);
		REQUIRE(rtxPacket->GetPayloadLength() == 6);
		REQUIRE(rtxPacket->GetHeaderExtensionLength() == 12);
		REQUIRE(rtxPacket->HasOneByteExtensions() == false);
		REQUIRE(rtxPacket->HasTwoBytesExtensions());

		rtxPacket->RtxDecode(1, 5);

		REQUIRE(rtxPacket->HasMarker() == false);
		REQUIRE(rtxPacket->HasHeaderExtension() == true);
		REQUIRE(rtxPacket->GetPayloadType() == 1);
		REQUIRE(rtxPacket->GetSequenceNumber() == 8);
		REQUIRE(rtxPacket->GetTimestamp() == 4);
		REQUIRE(rtxPacket->GetSsrc() == 5);
		REQUIRE(rtxPacket->GetPayloadLength() == 4);
		REQUIRE(rtxPacket->GetHeaderExtensionLength() == 12);
		REQUIRE(rtxPacket->HasOneByteExtensions() == false);
		REQUIRE(rtxPacket->HasTwoBytesExtensions());
	}

	SECTION("create RtpPacket and apply payload shift to it")
	{
		// clang-format off
		uint8_t buffer[] =
		{
			0xb0, 0x01, 0x00, 0x08,
			0x00, 0x00, 0x00, 0x04,
			0x00, 0x00, 0x00, 0x05,
			0xbe, 0xde, 0x00, 0x03, // Header Extension
			0x10, 0xff, 0x21, 0xff,
			0xff, 0x00, 0x00, 0x33,
			0xff, 0xff, 0xff, 0xff,
			0x00, 0x01, 0x02, 0x03, // Payload
			0x04, 0x05, 0x06, 0x07,
			0x00, 0x00, 0x00, 0x04, // 4 padding bytes
			0x00, 0x00, 0x00, 0x00, // Free buffer
			0x00, 0x00, 0x00, 0x00,
			0x00, 0x00, 0x00, 0x00,
			0x00, 0x00, 0x00, 0x00
		};
		// clang-format on

		size_t len = 40;
		std::unique_ptr<RtpPacket> packet{ RtpPacket::Parse(buffer, len) };

		if (!packet)
		{
			FAIL("not a RTP packet");
		}

		REQUIRE(packet->HasMarker() == false);
		REQUIRE(packet->GetPayloadType() == 1);
		REQUIRE(packet->GetSequenceNumber() == 8);
		REQUIRE(packet->GetTimestamp() == 4);
		REQUIRE(packet->GetSsrc() == 5);
		REQUIRE(packet->HasHeaderExtension() == true);
		REQUIRE(packet->GetHeaderExtensionId() == 0xBEDE);
		REQUIRE(packet->GetHeaderExtensionLength() == 12);
		REQUIRE(packet->HasOneByteExtensions());
		REQUIRE(packet->HasTwoBytesExtensions() == false);
		REQUIRE(packet->GetPayloadLength() == 8);
		REQUIRE(packet->GetPayloadPadding() == 4);
		REQUIRE(packet->GetPayload()[packet->GetPayloadLength() + packet->GetPayloadPadding() - 1] == 4);
		REQUIRE(packet->GetSize() == 40);

		auto* payload = packet->GetPayload();

		REQUIRE(payload[0] == 0x00);
		REQUIRE(payload[1] == 0x01);
		REQUIRE(payload[2] == 0x02);
		REQUIRE(payload[3] == 0x03);
		REQUIRE(payload[4] == 0x04);
		REQUIRE(payload[5] == 0x05);
		REQUIRE(payload[6] == 0x06);
		REQUIRE(payload[7] == 0x07);

		// NOTE: This will remove padding.
		packet->ShiftPayload(0, 2, true);

		REQUIRE(packet->GetPayloadLength() == 10);
		REQUIRE(packet->GetPayloadPadding() == 0);
		REQUIRE(packet->GetSize() == 38);
		REQUIRE(payload[2] == 0x00);
		REQUIRE(payload[3] == 0x01);
		REQUIRE(payload[4] == 0x02);
		REQUIRE(payload[5] == 0x03);
		REQUIRE(payload[6] == 0x04);
		REQUIRE(payload[7] == 0x05);
		REQUIRE(payload[8] == 0x06);
		REQUIRE(payload[9] == 0x07);

		packet->ShiftPayload(0, 2, false);

		REQUIRE(packet->GetPayloadLength() == 8);
		REQUIRE(packet->GetPayloadPadding() == 0);
		REQUIRE(packet->GetSize() == 36);
		REQUIRE(payload[0] == 0x00);
		REQUIRE(payload[1] == 0x01);
		REQUIRE(payload[2] == 0x02);
		REQUIRE(payload[3] == 0x03);
		REQUIRE(payload[4] == 0x04);
		REQUIRE(payload[5] == 0x05);
		REQUIRE(payload[6] == 0x06);
		REQUIRE(payload[7] == 0x07);

		// NOTE: This will remove padding.
		packet->SetPayloadLength(14);

		REQUIRE(packet->GetPayloadLength() == 14);
		REQUIRE(packet->GetPayloadPadding() == 0);
		REQUIRE(packet->GetSize() == 42);

		packet->ShiftPayload(4, 4, true);

		REQUIRE(packet->GetPayloadLength() == 18);
		REQUIRE(packet->GetPayloadPadding() == 0);
		REQUIRE(packet->GetSize() == 46);
		REQUIRE(payload[0] == 0x00);
		REQUIRE(payload[1] == 0x01);
		REQUIRE(payload[2] == 0x02);
		REQUIRE(payload[3] == 0x03);
		REQUIRE(payload[8] == 0x04);
		REQUIRE(payload[9] == 0x05);
		REQUIRE(payload[10] == 0x06);
		REQUIRE(payload[11] == 0x07);

		packet->SetPayloadLength(1000);

		REQUIRE(packet->GetPayloadLength() == 1000);
		REQUIRE(packet->GetPayloadPadding() == 0);
		REQUIRE(packet->GetSize() == 1028);
	}

	SECTION("set One-Byte header extensions")
	{
		// clang-format off
		uint8_t buffer[] =
		{
			0xa0, 0x01, 0x00, 0x08,
			0x00, 0x00, 0x00, 0x04,
			0x00, 0x00, 0x00, 0x05,
			0x11, 0x22, 0x33, 0x44, // Payload
			0x55, 0x66, 0x77, 0x88,
			0x99, 0xaa, 0xbb, 0xcc,
			0x00, 0x00, 0x00, 0x04, // 4 padding bytes
			// Extra buffer
			0x00, 0x00, 0x00, 0x00,
			0x00, 0x00, 0x00, 0x00,
			0x00, 0x00, 0x00, 0x00,
			0x00, 0x00, 0x00, 0x00,
			0x00, 0x00, 0x00, 0x00,
			0x00, 0x00, 0x00, 0x00
		};
		// clang-format on

		std::unique_ptr<RtpPacket> packet{ RtpPacket::Parse(buffer, 28) };
		std::vector<RTC::RtpPacket::GenericExtension> extensions;
		uint8_t extenLen;
		uint8_t* extenValue;

		if (!packet)
		{
			FAIL("not a RTP packet");
		}

		REQUIRE(packet->GetSize() == 28);
		REQUIRE(packet->HasHeaderExtension() == false);
		REQUIRE(packet->GetHeaderExtensionId() == 0);
		REQUIRE(packet->GetHeaderExtensionLength() == 0);
		REQUIRE(packet->HasOneByteExtensions() == false);
		REQUIRE(packet->HasTwoBytesExtensions() == false);
		REQUIRE(packet->GetPayloadLength() == 12);
		REQUIRE(packet->GetPayloadPadding() == 4);
		REQUIRE(packet->GetPayload()[packet->GetPayloadLength() + packet->GetPayloadPadding() - 1] == 4);
		REQUIRE(packet->GetPayload()[0] == 0x11);
		REQUIRE(packet->GetPayload()[packet->GetPayloadLength() - 1] == 0xCC);

		extensions.clear();

		packet->SetExtensions(1, extensions);

		REQUIRE(packet->GetSize() == 32);
		REQUIRE(packet->HasHeaderExtension() == true);
		REQUIRE(packet->GetHeaderExtensionId() == 0xBEDE);
		REQUIRE(packet->GetHeaderExtensionLength() == 0);
		REQUIRE(packet->HasOneByteExtensions() == true);
		REQUIRE(packet->HasTwoBytesExtensions() == false);
		REQUIRE(packet->GetPayloadLength() == 12);
		REQUIRE(packet->GetPayloadPadding() == 4);
		REQUIRE(packet->GetPayload()[packet->GetPayloadLength() + packet->GetPayloadPadding() - 1] == 4);
		REQUIRE(packet->GetPayload()[0] == 0x11);
		REQUIRE(packet->GetPayload()[packet->GetPayloadLength() - 1] == 0xCC);

		extensions.clear();

		uint8_t value1[] = { 0x01, 0x02, 0x03, 0x04 };

		// This must be ignored due to id=0.
		extensions.emplace_back(
		  0,     // id
		  4,     // len
		  value1 // value
		);

		// This must be ignored due to id>14.
		extensions.emplace_back(
		  15,    // id
		  4,     // len
		  value1 // value
		);

		// This must be ignored due to id>14.
		extensions.emplace_back(
		  22,    // id
		  4,     // len
		  value1 // value
		);

		extensions.emplace_back(
		  1,     // id
		  4,     // len
		  value1 // value
		);

		uint8_t value2[] = { 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x10, 0x11 };

		extensions.emplace_back(
		  2,     // id
		  11,    // len
		  value2 // value
		);

		packet->SetExtensions(1, extensions);

		REQUIRE(packet->GetSize() == 52); // 49 + 3 bytes for padding in header extension.
		REQUIRE(packet->HasHeaderExtension() == true);
		REQUIRE(packet->GetHeaderExtensionId() == 0xBEDE);
		REQUIRE(packet->GetHeaderExtensionLength() == 20); // 17 + 3 bytes for padding.
		REQUIRE(packet->HasOneByteExtensions() == true);
		REQUIRE(packet->HasTwoBytesExtensions() == false);
		REQUIRE(packet->GetPayloadLength() == 12);
		REQUIRE(packet->GetPayloadPadding() == 4);
		REQUIRE(packet->GetPayload()[packet->GetPayloadLength() + packet->GetPayloadPadding() - 1] == 4);
		REQUIRE(packet->GetPayload()[0] == 0x11);
		REQUIRE(packet->GetPayload()[packet->GetPayloadLength() - 1] == 0xCC);
		REQUIRE(packet->GetExtension(0, extenLen) == nullptr);
		REQUIRE(packet->HasExtension(0) == false);
		REQUIRE(packet->GetExtension(15, extenLen) == nullptr);
		REQUIRE(packet->HasExtension(15) == false);
		REQUIRE(packet->GetExtension(22, extenLen) == nullptr);
		REQUIRE(packet->HasExtension(22) == false);
		REQUIRE(packet->GetExtension(1, extenLen));
		REQUIRE(packet->HasExtension(1) == true);
		REQUIRE(extenLen == 4);
		REQUIRE(packet->GetExtension(2, extenLen));
		REQUIRE(packet->HasExtension(2) == true);
		REQUIRE(extenLen == 11);

		extensions.clear();

		uint8_t value3[] = { 0x01, 0x02, 0x03, 0x04 };

		extensions.emplace_back(
		  14,    // id
		  4,     // len
		  value3 // value
		);

		packet->SetExtensions(1, extensions);

		REQUIRE(packet->GetSize() == 40);
		REQUIRE(packet->HasHeaderExtension() == true);
		REQUIRE(packet->GetHeaderExtensionId() == 0xBEDE);
		REQUIRE(packet->GetHeaderExtensionLength() == 8); // 5 + 3 bytes for padding.
		REQUIRE(packet->HasOneByteExtensions() == true);
		REQUIRE(packet->HasTwoBytesExtensions() == false);
		REQUIRE(packet->GetPayloadLength() == 12);
		REQUIRE(packet->GetPayloadPadding() == 4);
		REQUIRE(packet->GetPayload()[packet->GetPayloadLength() + packet->GetPayloadPadding() - 1] == 4);
		REQUIRE(packet->GetPayload()[0] == 0x11);
		REQUIRE(packet->GetPayload()[packet->GetPayloadLength() - 1] == 0xCC);
		REQUIRE(packet->GetExtension(1, extenLen) == nullptr);
		REQUIRE(packet->HasExtension(1) == false);
		REQUIRE(packet->GetExtension(2, extenLen) == nullptr);
		REQUIRE(packet->HasExtension(2) == false);
		REQUIRE((extenValue = packet->GetExtension(14, extenLen)));
		REQUIRE(packet->HasExtension(14) == true);
		REQUIRE(extenLen == 4);
		REQUIRE(extenValue[0] == 0x01);
		REQUIRE(extenValue[1] == 0x02);
		REQUIRE(extenValue[2] == 0x03);
		REQUIRE(extenValue[3] == 0x04);
		REQUIRE(packet->SetExtensionLength(14, 3) == true);
		REQUIRE((extenValue = packet->GetExtension(14, extenLen)));
		REQUIRE(packet->HasExtension(14) == true);
		REQUIRE(extenLen == 3);
		REQUIRE(extenValue[0] == 0x01);
		REQUIRE(extenValue[1] == 0x02);
		REQUIRE(extenValue[2] == 0x03);
		REQUIRE(extenValue[3] == 0x00);
	}

	SECTION("set Two-Bytes header extensions")
	{
		// clang-format off
		uint8_t buffer[] =
		{
			0xa0, 0x01, 0x00, 0x08,
			0x00, 0x00, 0x00, 0x04,
			0x00, 0x00, 0x00, 0x05,
			0x11, 0x22, 0x33, 0x44, // Payload
			0x55, 0x66, 0x77, 0x88,
			0x99, 0xaa, 0xbb, 0xcc,
			0x00, 0x00, 0x00, 0x04, // 4 padding bytes
			// Extra buffer
			0x00, 0x00, 0x00, 0x00,
			0x00, 0x00, 0x00, 0x00,
			0x00, 0x00, 0x00, 0x00,
			0x00, 0x00, 0x00, 0x00,
			0x00, 0x00, 0x00, 0x00,
			0x00, 0x00, 0x00, 0x00,
			0x00, 0x00, 0x00, 0x00,
			0x00, 0x00, 0x00, 0x00
		};
		// clang-format on

		std::unique_ptr<RtpPacket> packet{ RtpPacket::Parse(buffer, 28) };
		std::vector<RTC::RtpPacket::GenericExtension> extensions;
		uint8_t extenLen;
		uint8_t* extenValue;

		if (!packet)
		{
			FAIL("not a RTP packet");
		}

		REQUIRE(packet->GetSize() == 28);
		REQUIRE(packet->HasHeaderExtension() == false);
		REQUIRE(packet->GetHeaderExtensionId() == 0);
		REQUIRE(packet->GetHeaderExtensionLength() == 0);
		REQUIRE(packet->HasOneByteExtensions() == false);
		REQUIRE(packet->HasTwoBytesExtensions() == false);
		REQUIRE(packet->GetPayloadLength() == 12);
		REQUIRE(packet->GetPayloadPadding() == 4);
		REQUIRE(packet->GetPayload()[packet->GetPayloadLength() + packet->GetPayloadPadding() - 1] == 4);
		REQUIRE(packet->GetPayload()[0] == 0x11);
		REQUIRE(packet->GetPayload()[packet->GetPayloadLength() - 1] == 0xCC);

		extensions.clear();

		packet->SetExtensions(2, extensions);

		REQUIRE(packet->GetSize() == 32);
		REQUIRE(packet->HasHeaderExtension() == true);
		REQUIRE(packet->GetHeaderExtensionId() == 0b0001000000000000);
		REQUIRE(packet->GetHeaderExtensionLength() == 0);
		REQUIRE(packet->HasOneByteExtensions() == false);
		REQUIRE(packet->HasTwoBytesExtensions() == true);
		REQUIRE(packet->GetPayloadLength() == 12);
		REQUIRE(packet->GetPayloadPadding() == 4);
		REQUIRE(packet->GetPayload()[packet->GetPayloadLength() + packet->GetPayloadPadding() - 1] == 4);
		REQUIRE(packet->GetPayload()[0] == 0x11);
		REQUIRE(packet->GetPayload()[packet->GetPayloadLength() - 1] == 0xCC);

		extensions.clear();

		uint8_t value1[] = { 0x01, 0x02, 0x03, 0x04 };

		// This must be ignored due to id=0.
		extensions.emplace_back(
		  0,     // id
		  4,     // len
		  value1 // value
		);

		extensions.emplace_back(
		  1,     // id
		  4,     // len
		  value1 // value
		);

		uint8_t value2[] = { 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x10, 0x11 };

		extensions.emplace_back(
		  22,    // id
		  11,    // len
		  value2 // value
		);

		packet->SetExtensions(2, extensions);

		REQUIRE(packet->GetSize() == 52); // 51 + 1 byte for padding in header extension.
		REQUIRE(packet->HasHeaderExtension() == true);
		REQUIRE(packet->GetHeaderExtensionId() == 0b0001000000000000);
		REQUIRE(packet->GetHeaderExtensionLength() == 20); // 19 + 1 byte for padding.
		REQUIRE(packet->HasOneByteExtensions() == false);
		REQUIRE(packet->HasTwoBytesExtensions() == true);
		REQUIRE(packet->GetPayloadLength() == 12);
		REQUIRE(packet->GetPayloadPadding() == 4);
		REQUIRE(packet->GetPayload()[packet->GetPayloadLength() + packet->GetPayloadPadding() - 1] == 4);
		REQUIRE(packet->GetPayload()[0] == 0x11);
		REQUIRE(packet->GetPayload()[packet->GetPayloadLength() - 1] == 0xCC);
		REQUIRE(packet->GetExtension(0, extenLen) == nullptr);
		REQUIRE(packet->HasExtension(0) == false);
		REQUIRE((extenValue = packet->GetExtension(1, extenLen)));
		REQUIRE(packet->HasExtension(1) == true);
		REQUIRE(extenLen == 4);
		REQUIRE(extenValue[0] == 0x01);
		REQUIRE(extenValue[1] == 0x02);
		REQUIRE(extenValue[2] == 0x03);
		REQUIRE(extenValue[3] == 0x04);
		REQUIRE(packet->SetExtensionLength(1, 2) == true);
		REQUIRE((extenValue = packet->GetExtension(1, extenLen)));
		REQUIRE(packet->HasExtension(1) == true);
		REQUIRE(extenLen == 2);
		REQUIRE(extenValue[0] == 0x01);
		REQUIRE(extenValue[1] == 0x02);
		REQUIRE(extenValue[2] == 0x00);
		REQUIRE(extenValue[3] == 0x00);
		REQUIRE(packet->GetExtension(22, extenLen));
		REQUIRE(packet->HasExtension(22) == true);
		REQUIRE(extenLen == 11);

		extensions.clear();

		uint8_t value3[] = { 0x01, 0x02, 0x03, 0x04 };

		extensions.emplace_back(
		  24,    // id
		  4,     // len
		  value3 // value
		);

		packet->SetExtensions(2, extensions);

		REQUIRE(packet->GetSize() == 40);
		REQUIRE(packet->HasHeaderExtension() == true);
		REQUIRE(packet->GetHeaderExtensionId() == 0b0001000000000000);
		REQUIRE(packet->GetHeaderExtensionLength() == 8);
		REQUIRE(packet->HasOneByteExtensions() == false);
		REQUIRE(packet->HasTwoBytesExtensions() == true);
		REQUIRE(packet->GetPayloadLength() == 12);
		REQUIRE(packet->GetPayloadPadding() == 4);
		REQUIRE(packet->GetPayload()[packet->GetPayloadLength() + packet->GetPayloadPadding() - 1] == 4);
		REQUIRE(packet->GetPayload()[0] == 0x11);
		REQUIRE(packet->GetPayload()[packet->GetPayloadLength() - 1] == 0xCC);
		REQUIRE(packet->GetExtension(1, extenLen) == nullptr);
		REQUIRE(packet->HasExtension(1) == false);
		REQUIRE(packet->GetExtension(22, extenLen) == nullptr);
		REQUIRE(packet->HasExtension(22) == false);
		REQUIRE(packet->GetExtension(24, extenLen));
		REQUIRE(packet->HasExtension(24) == true);
		REQUIRE(extenLen == 4);
	}

	SECTION("read frame-marking extension")
	{
		// clang-format off
		uint8_t buffer[] =
		{
			0x90, 0x01, 0x00, 0x08,
			0x00, 0x00, 0x00, 0x04,
			0x00, 0x00, 0x00, 0x05,
			0xbe, 0xde, 0x00, 0x01, // Header Extension
			0x32, 0xab, 0x01, 0x05,
			0x01, 0x02, 0x03, 0x04
		};
		// clang-format on

		std::unique_ptr<RtpPacket> packet{ RtpPacket::Parse(buffer, sizeof(buffer)) };

		if (!packet)
		{
			FAIL("not a RTP packet");
		}

		REQUIRE(packet->HasMarker() == false);
		REQUIRE(packet->HasHeaderExtension() == true);
		REQUIRE(packet->GetPayloadType() == 1);
		REQUIRE(packet->GetSequenceNumber() == 8);
		REQUIRE(packet->GetTimestamp() == 4);
		REQUIRE(packet->GetSsrc() == 5);
		REQUIRE(packet->GetHeaderExtensionId() == 0xBEDE);
		REQUIRE(packet->GetHeaderExtensionLength() == 4);
		REQUIRE(packet->HasOneByteExtensions());
		REQUIRE(packet->HasTwoBytesExtensions() == false);
		REQUIRE(packet->GetPayloadLength() == 4);

		packet->SetFrameMarkingExtensionId(3);

		RtpPacket::FrameMarking* frameMarking;
		uint8_t frameMarkingLen;

		REQUIRE(packet->ReadFrameMarking(&frameMarking, frameMarkingLen) == true);
		REQUIRE(frameMarkingLen == 3);
		REQUIRE(frameMarking->start == 1);
		REQUIRE(frameMarking->end == 0);
		REQUIRE(frameMarking->independent == 1);
		REQUIRE(frameMarking->discardable == 0);
		REQUIRE(frameMarking->base == 1);
		REQUIRE(frameMarking->tid == 3);
		REQUIRE(frameMarking->lid == 1);
		REQUIRE(frameMarking->tl0picidx == 5);
	}
}
