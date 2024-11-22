#include "common.hpp"
#include "RTC/RTCP/Packet.hpp"
#include "RTC/RTCP/Sdes.hpp"
#include <catch2/catch_test_macros.hpp>
#include <cstring> // std::memcmp()
#include <string>

using namespace RTC::RTCP;

namespace TestSdes
{
	// RTCP Sdes Packet.

	// clang-format off
	uint8_t buffer1[] =
	{
		0x81, 0xca, 0x00, 0x06, // Type: 202 (SDES), Count: 1, Length: 6
		0x9f, 0x65, 0xe7, 0x42, // SSRC: 0x9f65e742
		// Chunk 1
		0x01, 0x10, 0x74, 0x37, // Item Type: 1 (CNAME), Length: 16, Value: t7mkYnCm46OcINy/
		0x6d, 0x6b, 0x59, 0x6e,
		0x43, 0x6d, 0x34, 0x36,
		0x4f, 0x63, 0x49, 0x4e,
		0x79, 0x2f, 0x00, 0x00  // 2 null octets
	};
	// clang-format on

	// First chunk (chunk 1).
	uint32_t ssrc1{ 0x9f65e742 };
	// First item (item 1).
	SdesItem::Type item1Type{ SdesItem::Type::CNAME };
	std::string item1Value{ "t7mkYnCm46OcINy/" };
	size_t item1Length{ 16u };

	// clang-format off
	uint8_t buffer2[] =
	{
		0xa2, 0xca, 0x00, 0x0d, // Padding, Type: 202 (SDES), Count: 2, Length: 13
		// Chunk 2
		0x00, 0x00, 0x04, 0xd2, // SSRC: 1234
		0x01, 0x06, 0x71, 0x77, // Item Type: 1 (CNAME), Length: 6, Text: "qwerty"
		0x65, 0x72, 0x74, 0x79,
		0x06, 0x06, 0x69, 0xc3, // Item Type: 6 (TOOL), Length: 6, Text: "iñaki"
		0xb1, 0x61, 0x6b, 0x69,
		0x00, 0x00, 0x00, 0x00, // 4 null octets
		// Chunk 3
		0x00, 0x00, 0x16, 0x2e, // SSRC: 5678
		0x05, 0x11, 0x73, 0x6f, // Item Type: 5 (LOC), Length: 17, Text: "somewhere œæ€"
		0x6d, 0x65, 0x77, 0x68,
		0x65, 0x72, 0x65, 0x20,
		0xc5, 0x93, 0xc3, 0xa6,
		0xe2, 0x82, 0xac, 0x00,  // 1 null octet
		0x00, 0x00, 0x00, 0x00  // Pading (4 bytes)
	};
	// clang-format on

	// First chunk (chunk 2).
	uint32_t ssrc2{ 1234 };
	// First item (item 2).
	SdesItem::Type item2Type{ SdesItem::Type::CNAME };
	std::string item2Value{ "qwerty" };
	size_t item2Length{ 6u };
	// First item (item 3).
	SdesItem::Type item3Type{ SdesItem::Type::TOOL };
	std::string item3Value{ "iñaki" };
	size_t item3Length{ 6u };

	// Second chunk (chunk 3).
	uint32_t ssrc3{ 5678 };
	// First item (item 4).
	SdesItem::Type item4Type{ SdesItem::Type::LOC };
	std::string item4Value{ "somewhere œæ€" };
	size_t item4Length{ 17u };

	// clang-format off
	uint8_t buffer3[] =
	{
		0x81, 0xca, 0x00, 0x03, // Type: 202 (SDES), Count: 1, Length: 3
		// Chunk
		0x11, 0x22, 0x33, 0x44, // SSRC: 0x11223344
		0x05, 0x02, 0x61, 0x62, // Item Type: 5 (LOC), Length: 2, Text: "ab"
		0x00, 0x00, 0x00, 0x00  // 4 null octets
	};
	// clang-format on

	// First chunk (chunk 4).
	uint32_t ssrc4{ 0x11223344 };
	// First item (item 5).
	SdesItem::Type item5Type{ SdesItem::Type::LOC };
	std::string item5Value{ "ab" };
	size_t item5Length{ 2u };
} // namespace TestSdes

using namespace TestSdes;

SCENARIO("RTCP SDES parsing", "[parser][rtcp][sdes]")
{
	SECTION("parse packet 1")
	{
		std::unique_ptr<SdesPacket> packet{ SdesPacket::Parse(buffer1, sizeof(buffer1)) };
		auto* header = reinterpret_cast<RTC::RTCP::Packet::CommonHeader*>(buffer1);

		REQUIRE(packet);
		REQUIRE(ntohs(header->length) == 6);
		REQUIRE(packet->GetSize() == 28);
		REQUIRE(packet->GetCount() == 1);

		size_t chunkIdx{ 0u };

		for (auto it = packet->Begin(); it != packet->End(); ++it, ++chunkIdx)
		{
			auto* chunk = *it;

			switch (chunkIdx)
			{
				/* First chunk (chunk 1). */
				case 0:
				{
					// Chunk size must be 24 bytes (including 4 null octets).
					REQUIRE(chunk->GetSize() == 24);
					REQUIRE(chunk->GetSsrc() == ssrc1);

					size_t itemIdx{ 0u };

					for (auto it2 = chunk->Begin(); it2 != chunk->End(); ++it2, ++itemIdx)
					{
						auto* item = *it2;

						switch (itemIdx)
						{
							/* First item (item 1). */
							case 0:
							{
								REQUIRE(item->GetType() == item1Type);
								REQUIRE(item->GetLength() == item1Length);
								REQUIRE(std::string(item->GetValue(), item1Length) == item1Value);

								break;
							}
						}
					}

					// There is 1 item.
					REQUIRE(itemIdx == 1);

					break;
				}
			}
		}

		// There is 1 chunk.
		REQUIRE(chunkIdx == 1);

		SECTION("serialize SdesChunk instance")
		{
			auto it               = packet->Begin();
			auto* chunk1          = *it;
			uint8_t* chunk1Buffer = buffer1 + Packet::CommonHeaderSize;

			// NOTE: Length of first chunk (including null octets) is 24.
			uint8_t serialized1[24] = { 0 };

			chunk1->Serialize(serialized1);

			REQUIRE(std::memcmp(chunk1Buffer, serialized1, 24) == 0);
		}
	}

	SECTION("parse packet 2")
	{
		std::unique_ptr<SdesPacket> packet{ SdesPacket::Parse(buffer2, sizeof(buffer2)) };
		auto* header = reinterpret_cast<RTC::RTCP::Packet::CommonHeader*>(buffer2);

		REQUIRE(packet);
		REQUIRE(ntohs(header->length) == 13);
		// Despite total buffer size is 56 bytes, our GetSize() method doesn't not
		// consider RTCP padding (4 bytes in this case).
		REQUIRE(packet->GetSize() == 52);
		REQUIRE(packet->GetCount() == 2);

		size_t chunkIdx{ 0u };

		for (auto it = packet->Begin(); it != packet->End(); ++it, ++chunkIdx)
		{
			auto* chunk = *it;

			switch (chunkIdx)
			{
				/* First chunk (chunk 2). */
				case 0:
				{
					// Chunk size must be 24 bytes (including 4 null octets).
					REQUIRE(chunk->GetSize() == 24);
					REQUIRE(chunk->GetSsrc() == ssrc2);

					size_t itemIdx{ 0u };

					for (auto it2 = chunk->Begin(); it2 != chunk->End(); ++it2, ++itemIdx)
					{
						auto* item = *it2;

						switch (itemIdx)
						{
							/* First item (item 2). */
							case 0:
							{
								REQUIRE(item->GetType() == item2Type);
								REQUIRE(item->GetLength() == item2Length);
								REQUIRE(std::string(item->GetValue(), item2Length) == item2Value);

								break;
							}

							/* Second item (item 3). */
							case 1:
							{
								REQUIRE(item->GetType() == item3Type);
								REQUIRE(item->GetLength() == item3Length);
								REQUIRE(std::string(item->GetValue(), item3Length) == item3Value);

								break;
							}
						}
					}

					// There are 2 items.
					REQUIRE(itemIdx == 2);

					break;
				}

				/* Second chunk (chunk 3). */
				case 1:
				{
					// Chunk size must be 24 bytes (including 1 null octet).
					REQUIRE(chunk->GetSize() == 24);
					REQUIRE(chunk->GetSsrc() == ssrc3);

					size_t itemIdx{ 0u };

					for (auto it2 = chunk->Begin(); it2 != chunk->End(); ++it2, ++itemIdx)
					{
						auto* item = *it2;

						switch (itemIdx)
						{
							/* First item (item 4). */
							case 0:
							{
								REQUIRE(item->GetType() == item4Type);
								REQUIRE(item->GetLength() == item4Length);
								REQUIRE(std::string(item->GetValue(), item4Length) == item4Value);

								break;
							}
						}
					}

					// There is 1 item.
					REQUIRE(itemIdx == 1);

					break;
				}
			}
		}

		// There are 2 chunks.
		REQUIRE(chunkIdx == 2);

		SECTION("serialize SdesChunk instances")
		{
			auto it               = packet->Begin();
			auto* chunk1          = *it;
			uint8_t* chunk1Buffer = buffer2 + Packet::CommonHeaderSize;

			// NOTE: Length of first chunk (including null octets) is 24.
			uint8_t serialized1[24] = { 0 };

			chunk1->Serialize(serialized1);

			REQUIRE(std::memcmp(chunk1Buffer, serialized1, 24) == 0);

			auto* chunk2          = *(++it);
			uint8_t* chunk2Buffer = buffer2 + Packet::CommonHeaderSize + 24;

			// NOTE: Length of second chunk (including null octets) is 24.
			uint8_t serialized2[24] = { 0 };

			chunk2->Serialize(serialized2);

			REQUIRE(std::memcmp(chunk2Buffer, serialized2, 24) == 0);
		}
	}

	SECTION("parse packet 3")
	{
		std::unique_ptr<SdesPacket> packet{ SdesPacket::Parse(buffer3, sizeof(buffer3)) };
		auto* header = reinterpret_cast<RTC::RTCP::Packet::CommonHeader*>(buffer3);

		REQUIRE(packet);
		REQUIRE(ntohs(header->length) == 3);
		REQUIRE(packet->GetSize() == 16);
		REQUIRE(packet->GetCount() == 1);

		size_t chunkIdx{ 0u };

		for (auto it = packet->Begin(); it != packet->End(); ++it, ++chunkIdx)
		{
			auto* chunk = *it;

			switch (chunkIdx)
			{
				/* First chunk (chunk 4). */
				case 0:
				{
					REQUIRE(chunk->GetSize() == 12);
					REQUIRE(chunk->GetSsrc() == ssrc4);

					size_t itemIdx{ 0u };

					for (auto it2 = chunk->Begin(); it2 != chunk->End(); ++it2, ++itemIdx)
					{
						auto* item = *it2;

						switch (itemIdx)
						{
							/* First item (item 5). */
							case 0:
							{
								REQUIRE(item->GetType() == item5Type);
								REQUIRE(item->GetLength() == item5Length);
								REQUIRE(std::string(item->GetValue(), item5Length) == item5Value);

								break;
							}
						}
					}

					// There is 1 item.
					REQUIRE(itemIdx == 1);

					break;
				}
			}
		}

		// There is 1 chunk.
		REQUIRE(chunkIdx == 1);

		SECTION("serialize SdesChunk instance")
		{
			auto it               = packet->Begin();
			auto* chunk1          = *it;
			uint8_t* chunk1Buffer = buffer3 + Packet::CommonHeaderSize;

			// NOTE: Length of first chunk (including null octets) is 12.
			uint8_t serialized1[12] = { 0 };

			chunk1->Serialize(serialized1);

			REQUIRE(std::memcmp(chunk1Buffer, serialized1, 12) == 0);
		}
	}

	SECTION("parsing a packet with missing null octects fails")
	{
		// clang-format off
		uint8_t buffer[] =
		{
			0x81, 0xca, 0x00, 0x02, // Type: 202 (SDES), Count: 1, Length: 2
			// Chunk
			0x11, 0x22, 0x33, 0x44, // SSRC: 0x11223344
			0x08, 0x02, 0x61, 0x62  // Item Type: 8 (PRIV), Length: 2, Text: "ab"
		};

		SdesPacket* packet = SdesPacket::Parse(buffer, sizeof(buffer));

		REQUIRE(!packet);
	}

	SECTION("create SDES packet with 31 chunks")
	{
		const size_t count = 31;

		SdesPacket packet;
		// Create a chunk and an item to obtain their size.
		auto chunk = std::make_unique<SdesChunk>(1234 /*ssrc*/);
		auto* item1 =
		  new RTC::RTCP::SdesItem(SdesItem::Type::CNAME, item1Value.size(), item1Value.c_str());

		chunk->AddItem(item1);

		auto chunkSize = chunk->GetSize();

		for (auto i{ 1 }; i <= count; ++i)
		{
			// Create chunk and add to packet.
			SdesChunk* chunk = new SdesChunk(i /*ssrc*/);

			auto* item1 =
			  new RTC::RTCP::SdesItem(SdesItem::Type::CNAME, item1Value.size(), item1Value.c_str());

			chunk->AddItem(item1);

			packet.AddChunk(chunk);
		}

		REQUIRE(packet.GetCount() == count);
		REQUIRE(packet.GetSize() == Packet::CommonHeaderSize + (count * chunkSize));

		uint8_t buffer1[1500] = { 0 };

		// Serialization must contain 1 SDES packet since report count doesn't
		// exceed 31.
		packet.Serialize(buffer1);

		std::unique_ptr<SdesPacket> packet2{static_cast<SdesPacket*>(Packet::Parse(buffer1, sizeof(buffer1)))};

		REQUIRE(packet2 != nullptr);
		REQUIRE(packet2->GetCount() == count);
		REQUIRE(packet2->GetSize() == Packet::CommonHeaderSize + (count * chunkSize));

		auto reportIt = packet2->Begin();

		for (auto i{ 1 }; i <= 31; ++i, reportIt++)
		{
			auto* chunk = *reportIt;

			REQUIRE(chunk->GetSsrc() == i);

			auto* item = *(chunk->Begin());

			REQUIRE(item->GetType() == SdesItem::Type::CNAME);
			REQUIRE(item->GetSize() == 2 + item1Value.size());
			REQUIRE(std::string(item->GetValue()) == item1Value);
		}

		std::unique_ptr<SdesPacket> packet3{static_cast<SdesPacket*>(packet2->GetNext())};

		REQUIRE(packet3 == nullptr);

	}

	SECTION("create SDES packet with more than 31 chunks")
	{
		const size_t count = 33;

		SdesPacket packet;
		// Create a chunk and an item to obtain their size.
		auto chunk = std::make_unique<SdesChunk>(1234 /*ssrc*/);
		auto* item1 =
		  new RTC::RTCP::SdesItem(SdesItem::Type::CNAME, item1Value.size(), item1Value.c_str());

		chunk->AddItem(item1);

		auto chunkSize = chunk->GetSize();

		for (auto i{ 1 }; i <= count; ++i)
		{
			// Create chunk and add to packet.
			SdesChunk* chunk = new SdesChunk(i /*ssrc*/);

			auto* item1 =
			  new RTC::RTCP::SdesItem(SdesItem::Type::CNAME, item1Value.size(), item1Value.c_str());

			chunk->AddItem(item1);

			packet.AddChunk(chunk);
		}

		REQUIRE(packet.GetCount() == count);
		REQUIRE(packet.GetSize() == Packet::CommonHeaderSize + (31 * chunkSize) + Packet::CommonHeaderSize + ((count - 31) * chunkSize));

		uint8_t buffer1[1500] = { 0 };

		// Serialization must contain 2 SDES packets since report count exceeds 31.
		packet.Serialize(buffer1);

		std::unique_ptr<SdesPacket> packet2 {static_cast<SdesPacket*>(Packet::Parse(buffer1, sizeof(buffer1)))};

		REQUIRE(packet2 != nullptr);
		REQUIRE(packet2->GetCount() == 31);
		REQUIRE(packet2->GetSize() == Packet::CommonHeaderSize + (31 * chunkSize));

		auto reportIt = packet2->Begin();

		for (auto i{ 1 }; i <= 31; ++i, reportIt++)
		{
			auto* chunk = *reportIt;

			REQUIRE(chunk->GetSsrc() == i);

			auto* item = *(chunk->Begin());

			REQUIRE(item->GetType() == SdesItem::Type::CNAME);
			REQUIRE(item->GetSize() == 2 + item1Value.size());
			REQUIRE(std::string(item->GetValue()) == item1Value);
		}

		SdesPacket* packet3 = static_cast<SdesPacket*>(packet2->GetNext());

		REQUIRE(packet3 != nullptr);
		REQUIRE(packet3->GetCount() == count - 31);
		REQUIRE(packet3->GetSize() == Packet::CommonHeaderSize + ((count - 31) * chunkSize));

		reportIt = packet3->Begin();

		for (auto i{ 1 }; i <= 2; ++i, reportIt++)
		{
			auto* chunk = *reportIt;

			REQUIRE(chunk->GetSsrc() == 31 + i);

			auto* item = *(chunk->Begin());

			REQUIRE(item->GetType() == SdesItem::Type::CNAME);
			REQUIRE(item->GetSize() == 2 + item1Value.size());
			REQUIRE(std::string(item->GetValue()) == item1Value);
		}

		delete packet3;
	}

	SECTION("create SdesChunk")
	{
		auto* item = new SdesItem(item1Type, item1Length, item1Value.c_str());

		// Create sdes chunk.
		SdesChunk chunk(ssrc1);

		chunk.AddItem(item);

		REQUIRE(chunk.GetSsrc() == ssrc1);

		SdesItem* item1 = *(chunk.Begin());

		REQUIRE(item1->GetType() == item1Type);
		REQUIRE(item1->GetLength() == item1Length);
		REQUIRE(std::string(item1->GetValue(), item1Length) == item1Value);
	}
}
