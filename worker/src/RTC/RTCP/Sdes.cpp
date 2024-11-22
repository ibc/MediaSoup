#define MS_CLASS "RTC::RTCP::Sdes"
// #define MS_LOG_DEV_LEVEL 3

#include "RTC/RTCP/Sdes.hpp"
#include "Logger.hpp"
#include "Utils.hpp"
#include <cstring> // std::memcpy

namespace RTC
{
	namespace RTCP
	{
		/* Item Class variables. */

		// clang-format off
		absl::flat_hash_map<SdesItem::Type, std::string> SdesItem::type2String =
		{
			{ SdesItem::Type::END,   "END"   },
			{ SdesItem::Type::CNAME, "CNAME" },
			{ SdesItem::Type::NAME,  "NAME"  },
			{ SdesItem::Type::EMAIL, "EMAIL" },
			{ SdesItem::Type::PHONE, "PHONE" },
			{ SdesItem::Type::LOC,   "LOC"   },
			{ SdesItem::Type::TOOL,  "TOOL"  },
			{ SdesItem::Type::NOTE,  "NOTE"  },
			{ SdesItem::Type::PRIV,  "PRIV"  }
		};
		// clang-format on

		/* Class methods. */

		SdesItem* SdesItem::Parse(const uint8_t* data, size_t len)
		{
			MS_TRACE();

			// Get the header.
			auto* header = const_cast<Header*>(reinterpret_cast<const Header*>(data));

			// If item type is 0, there is no need for length field (unless padding
			// is needed).
			if (len > 0 && header->type == SdesItem::Type::END)
			{
				return nullptr;
			}

			// data size must be >= header + length value.
			if (len < HeaderSize || len < HeaderSize + header->length)
			{
				MS_WARN_TAG(rtcp, "not enough space for SDES item, discarded");

				return nullptr;
			}

			return new SdesItem(header);
		}

		const std::string& SdesItem::Type2String(SdesItem::Type type)
		{
			static const std::string Unknown("UNKNOWN");

			auto it = SdesItem::type2String.find(type);

			if (it == SdesItem::type2String.end())
			{
				return Unknown;
			}

			return it->second;
		}

		/* Instance methods. */

		SdesItem::SdesItem(SdesItem::Type type, size_t len, const char* value)
		{
			MS_TRACE();

			// Allocate memory.
			this->raw.reset(new uint8_t[2 + len]);

			// Update the header pointer.
			this->header = reinterpret_cast<Header*>(this->raw.get());

			this->header->type   = type;
			this->header->length = len;

			// Copy the value into raw.
			std::memcpy(this->header->value, value, this->header->length);
		}

		void SdesItem::Dump() const
		{
			MS_TRACE();

			MS_DUMP("<SdesItem>");
			MS_DUMP("  type: %s", SdesItem::Type2String(this->GetType()).c_str());
			MS_DUMP("  length: %" PRIu8, this->header->length);
			MS_DUMP("  value: %.*s", this->header->length, this->header->value);
			MS_DUMP("</SdesItem>");
		}

		size_t SdesItem::Serialize(uint8_t* buffer)
		{
			MS_TRACE();

			// Add minimum header.
			std::memcpy(buffer, this->header, 2);

			// Copy the content.
			std::memcpy(buffer + 2, this->header->value, this->header->length);

			return 2 + this->header->length;
		}

		/* Class methods. */

		SdesChunk* SdesChunk::Parse(const uint8_t* data, size_t len)
		{
			MS_TRACE();

			// data size must be > SSRC field.
			if (len < 4u /* ssrc */)
			{
				MS_WARN_TAG(rtcp, "not enough space for SDES chunk, discarded");

				return nullptr;
			}

			const uint32_t ssrc = Utils::Byte::Get4Bytes(data, 0);

			std::unique_ptr<SdesChunk> chunk(new SdesChunk(ssrc));

			size_t offset{ 4u }; /* ssrc */
			size_t chunkLength{ 4u };

			while (len > offset)
			{
				auto* item = SdesItem::Parse(data + offset, len - offset);

				if (!item)
				{
					break;
				}

				chunk->AddItem(item);
				chunkLength += item->GetSize();
				offset += item->GetSize();
			}

			// Once all items have been parsed, there must be 1, 2, 3 or 4 null octets
			// (this is mandatory). The first one (AKA item type 0) means "end of
			// items", and the others maybe needed for padding the chunk to 4 bytes.

			// First ensure that there is at least one null octet.
			if (offset == len || (offset < len && Utils::Byte::Get1Byte(data, offset) != 0u))
			{
				MS_WARN_TAG(rtcp, "invalid SDES chunk (missing mandatory null octet), discarded");

				return nullptr;
			}

			// So we have the mandatory null octet.
			++chunkLength;
			++offset;

			// Then check that there are 0, 1, 2 or 3 (no more) null octets that pad
			// the chunk to 4 bytes.
			uint16_t neededAdditionalNullOctets =
			  Utils::Byte::PadTo4Bytes(static_cast<uint16_t>(chunkLength)) -
			  static_cast<uint16_t>(chunkLength);
			uint16_t foundAdditionalNullOctets{ 0u };

			for (uint16_t i{ 0u }; len > offset && i < neededAdditionalNullOctets; ++i)
			{
				if (Utils::Byte::Get1Byte(data, offset) != 0u)
				{
					MS_WARN_TAG(
					  rtcp,
					  "invalid SDES chunk (missing additional null octets [needed:%" PRIu16 ", found:%" PRIu16
					  "]), discarded",
					  neededAdditionalNullOctets,
					  foundAdditionalNullOctets);

					return nullptr;
				}

				++foundAdditionalNullOctets;
				++chunkLength;
				++offset;
			}

			if (foundAdditionalNullOctets != neededAdditionalNullOctets)
			{
				MS_WARN_TAG(
				  rtcp,
				  "invalid SDES chunk (missing additional null octets [needed:%" PRIu16 ", found:%" PRIu16
				  "]), discarded",
				  neededAdditionalNullOctets,
				  foundAdditionalNullOctets);

				return nullptr;
			}

			return chunk.release();
		}

		/* Instance methods. */

		size_t SdesChunk::Serialize(uint8_t* buffer)
		{
			MS_TRACE();

			// Copy the SSRC.
			Utils::Byte::Set4Bytes(buffer, 0, this->ssrc);

			size_t offset{ 4u }; // ssrc.

			for (auto* item : this->items)
			{
				offset += item->Serialize(buffer + offset);
			}

			// Add the mandatory null octet.
			buffer[offset] = 0;

			++offset;

			// 32 bits padding.
			const size_t padding = (-offset) & 3;

			for (size_t i{ 0u }; i < padding; ++i)
			{
				buffer[offset + i] = 0;
			}

			return offset + padding;
		}

		void SdesChunk::Dump() const
		{
			MS_TRACE();

			MS_DUMP("<SdesChunk>");
			MS_DUMP("  ssrc: %" PRIu32, this->ssrc);
			for (auto* item : this->items)
			{
				item->Dump();
			}
			MS_DUMP("</SdesChunk>");
		}

		/* Static Class members */

		size_t SdesPacket::MaxChunksPerPacket = 31;

		/* Class methods. */

		SdesPacket* SdesPacket::Parse(const uint8_t* data, size_t len)
		{
			MS_TRACE();

			// Get the header.
			auto* header = const_cast<CommonHeader*>(reinterpret_cast<const CommonHeader*>(data));
			std::unique_ptr<SdesPacket> packet(new SdesPacket(header));
			size_t offset = Packet::CommonHeaderSize;
			uint8_t count = header->count;

			while (count-- && (len > offset))
			{
				auto* chunk = SdesChunk::Parse(data + offset, len - offset);

				if (chunk != nullptr)
				{
					packet->AddChunk(chunk);
					offset += chunk->GetSize();
				}
				else
				{
					break;
				}
			}

			if (packet->GetCount() != header->count)
			{
				MS_WARN_TAG(rtcp, "RTCP count value doesn't match found number of chunks, discarded");

				return nullptr;
			}

			return packet.release();
		}

		/* Instance methods. */

		size_t SdesPacket::Serialize(uint8_t* buffer)
		{
			MS_TRACE();

			size_t offset   = 0;
			size_t length   = 0;
			uint8_t* header = { nullptr };

			for (size_t i{ 0u }; i < this->GetCount(); ++i)
			{
				// Create a new SDES packet header for each 31 chunks.
				if (i % MaxChunksPerPacket == 0)
				{
					// Reference current common header.
					header = buffer + offset;
					offset += Packet::Serialize(buffer + offset);

					length = Packet::CommonHeaderSize;
				}

				// Serialize the next chunk.
				auto chunkSize = chunks[i]->Serialize(buffer + offset);

				offset += chunkSize;
				length += chunkSize;

				// Adjust the header count field.
				reinterpret_cast<Packet::CommonHeader*>(header)->count =
				  static_cast<uint8_t>((i % MaxChunksPerPacket) + 1);

				// Adjust the header length field.
				reinterpret_cast<Packet::CommonHeader*>(header)->length = uint16_t{ htons((length / 4) - 1) };
			}

			return offset;
		}

		void SdesPacket::Dump() const
		{
			MS_TRACE();

			MS_DUMP("<SdesPacket>");
			for (auto* chunk : this->chunks)
			{
				chunk->Dump();
			}
			MS_DUMP("</SdesPacket>");
		}
	} // namespace RTCP
} // namespace RTC
