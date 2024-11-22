#ifndef MS_RTC_RTCP_SDES_HPP
#define MS_RTC_RTCP_SDES_HPP

#include "common.hpp"
#include "RTC/RTCP/Packet.hpp"
#include <string>
#include <vector>

namespace RTC
{
	namespace RTCP
	{
		/* SDES Item. */
		class SdesItem
		{
		public:
			enum class Type : uint8_t
			{
				END = 0,
				CNAME,
				NAME,
				EMAIL,
				PHONE,
				LOC,
				TOOL,
				NOTE,
				PRIV
			};

		private:
			struct Header
			{
				SdesItem::Type type;
				uint8_t length;
				char value[];
			};

		public:
			static const size_t HeaderSize = 2;
			static SdesItem* Parse(const uint8_t* data, size_t len);
			static const std::string& Type2String(SdesItem::Type type);

		public:
			explicit SdesItem(Header* header) : header(header)
			{
			}
			explicit SdesItem(SdesItem* item) : header(item->header)
			{
			}
			SdesItem(SdesItem::Type type, size_t len, const char* value);
			~SdesItem() = default;

			void Dump() const;
			size_t Serialize(uint8_t* buffer);
			size_t GetSize() const
			{
				return 2 + size_t{ this->header->length };
			}

			SdesItem::Type GetType() const
			{
				return this->header->type;
			}
			uint8_t GetLength() const
			{
				return this->header->length;
			}
			char* GetValue() const
			{
				return this->header->value;
			}

		private:
			// Passed by argument.
			Header* header{ nullptr };
			std::unique_ptr<uint8_t[]> raw;

		private:
			static absl::flat_hash_map<SdesItem::Type, std::string> type2String;
		};

		class SdesChunk
		{
		public:
			using Iterator = std::vector<SdesItem*>::iterator;

		public:
			static SdesChunk* Parse(const uint8_t* data, size_t len);

		public:
			explicit SdesChunk(uint32_t ssrc)
			{
				this->ssrc = ssrc;
			}
			explicit SdesChunk(SdesChunk* chunk)
			{
				this->ssrc = chunk->ssrc;

				for (auto it = chunk->Begin(); it != chunk->End(); ++it)
				{
					this->AddItem(new SdesItem(*it));
				}
			}
			~SdesChunk()
			{
				for (auto* item : this->items)
				{
					delete item;
				}
			}

			void Dump() const;
			void Serialize();
			size_t Serialize(uint8_t* buffer);
			size_t GetSize() const
			{
				size_t size = 4u /*ssrc*/;

				for (auto* item : this->items)
				{
					size += item->GetSize();
				}

				// Add the mandatory null octet.
				++size;

				// Consider pading to 32 bits (4 bytes) boundary.
				// http://stackoverflow.com/questions/11642210/computing-padding-required-for-n-byte-alignment
				return (size + 3) & ~3;
			}
			uint32_t GetSsrc() const
			{
				return this->ssrc;
			}
			void SetSsrc(uint32_t ssrc)
			{
				this->ssrc = htonl(ssrc);
			}
			void AddItem(SdesItem* item)
			{
				this->items.push_back(item);
			}
			Iterator Begin()
			{
				return this->items.begin();
			}
			Iterator End()
			{
				return this->items.end();
			}

		private:
			uint32_t ssrc{ 0u };
			std::vector<SdesItem*> items;
		};

		class SdesPacket : public Packet
		{
		public:
			static size_t MaxChunksPerPacket;

			using Iterator = std::vector<SdesChunk*>::iterator;

		public:
			static SdesPacket* Parse(const uint8_t* data, size_t len);

		public:
			SdesPacket() : Packet(RTCP::Type::SDES)
			{
			}
			explicit SdesPacket(CommonHeader* commonHeader) : Packet(commonHeader)
			{
			}
			~SdesPacket() override
			{
				for (auto* chunk : this->chunks)
				{
					delete chunk;
				}
			}

			void AddChunk(SdesChunk* chunk)
			{
				this->chunks.push_back(chunk);
			}
			void RemoveChunk(SdesChunk* chunk)
			{
				auto it = std::find(this->chunks.begin(), this->chunks.end(), chunk);

				if (it != this->chunks.end())
				{
					this->chunks.erase(it);
				}
			}
			Iterator Begin()
			{
				return this->chunks.begin();
			}
			Iterator End()
			{
				return this->chunks.end();
			}

			/* Pure virtual methods inherited from Packet. */
		public:
			void Dump() const override;
			size_t Serialize(uint8_t* buffer) override;
			size_t GetCount() const override
			{
				return this->chunks.size();
			}
			size_t GetSize() const override
			{
				// A serialized packet can contain a maximum of 31 chunks.
				// If number of chunks exceeds 31 then the required number of packets
				// will be serialized which will take the size calculated below.
				size_t size = Packet::CommonHeaderSize * ((this->GetCount() / (MaxChunksPerPacket + 1)) + 1);

				for (auto* chunk : this->chunks)
				{
					size += chunk->GetSize();
				}

				return size;
			}

		private:
			std::vector<SdesChunk*> chunks;
		};
	} // namespace RTCP
} // namespace RTC

#endif
