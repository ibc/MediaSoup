#define MS_CLASS "RTC::RTCP::FeedbackRtpTransport"
// #define MS_LOG_DEV

#include "RTC/RTCP/FeedbackRtpTransport.hpp"
#include "Logger.hpp"
#include "Utils.hpp"
#include "RTC/SeqManager.hpp"
#include <limits> // std::numeric_limits()
#include <sstream>

// Code taken and adapted from libwebrtc (byte_io.h).
inline static int32_t parseReferencetime(uint8_t* buffer)
{
	int32_t referenceTime;
	uint32_t unsignedVal = Utils::Byte::Get3Bytes(buffer, 0);

	const uint8_t msb = static_cast<uint8_t>(unsignedVal >> ((3 - 1) * 8));

	if ((msb & 0x80) != 0)
	{
		// Create a mask where all bits used by the 3 bytes are set to one, for
		// instance 0x00FFFFFF. Bit-wise inverts that mask to 0xFF000000 and adds
		// it to the input value.
		static uint32_t usedBitMask = (1 << ((3 % sizeof(int32_t)) * 8)) - 1;

		unsignedVal = ~usedBitMask | unsignedVal;
	}

	// An unsigned value with only the highest order bit set (ex 0x80).
	static uint32_t unsignedHighestBitMask = static_cast<uint32_t>(1) << ((sizeof(uint32_t) * 8) - 1);

	// A signed value with only the highest bit set. Since this is two's
	// complement form, we can use the min value from std::numeric_limits.
	static int32_t signedHighestBitMask = std::numeric_limits<int32_t>::min();

	if ((unsignedVal & unsignedHighestBitMask) != 0)
	{
		// Casting is only safe when unsigned value can be represented in the
		// signed target type, so mask out highest bit and mask it back manually.
		referenceTime = static_cast<int32_t>(unsignedVal & ~unsignedHighestBitMask);
		referenceTime |= signedHighestBitMask;
	}
	else
	{
		referenceTime = static_cast<int32_t>(unsignedVal);
	}

	return referenceTime;
}

namespace RTC
{
	namespace RTCP
	{
		/* Static members. */

		size_t FeedbackRtpTransportPacket::fixedHeaderSize{ 8u };
		uint16_t FeedbackRtpTransportPacket::maxMissingPackets{ (1 << 13) - 1 };
		uint16_t FeedbackRtpTransportPacket::maxPacketStatusCount{ (1 << 16) - 1 };
		int16_t FeedbackRtpTransportPacket::maxPacketDelta{ 0x7FFF };

		// clang-format off
		std::map<FeedbackRtpTransportPacket::Status, std::string> FeedbackRtpTransportPacket::Status2String =
		{
			{ FeedbackRtpTransportPacket::Status::NotReceived, "NR" },
			{ FeedbackRtpTransportPacket::Status::SmallDelta,  "SD" },
			{ FeedbackRtpTransportPacket::Status::LargeDelta,  "LD" }
		};
		// clang-format on

		/* Class methods. */

		FeedbackRtpTransportPacket* FeedbackRtpTransportPacket::Parse(const uint8_t* data, size_t len)
		{
			MS_TRACE();

			if (len < sizeof(CommonHeader) + sizeof(FeedbackPacket::Header) + FeedbackRtpTransportPacket::fixedHeaderSize)
			{
				MS_WARN_TAG(rtcp, "not enough space for Feedback packet, discarded");

				return nullptr;
			}

			auto* commonHeader = const_cast<CommonHeader*>(reinterpret_cast<const CommonHeader*>(data));

			std::unique_ptr<FeedbackRtpTransportPacket> packet(
			  new FeedbackRtpTransportPacket(commonHeader, len));

			if (!packet->IsCorrect())
				return nullptr;

			return packet.release();
		}

		/* Instance methods. */

		FeedbackRtpTransportPacket::FeedbackRtpTransportPacket(CommonHeader* commonHeader, size_t availableLen)
		  : FeedbackRtpPacket(commonHeader)
		{
			MS_TRACE();

			size_t len = static_cast<size_t>(ntohs(commonHeader->length) + 1) * 4;

			if (len > availableLen)
			{
				MS_WARN_TAG(rtcp, "packet announced length exceeds the available buffer length, discarded");

				this->isCorrect = false;

				return;
			}

			// Make data point to the packet specific info.
			auto* data = reinterpret_cast<uint8_t*>(commonHeader) + sizeof(CommonHeader) +
			             sizeof(FeedbackPacket::Header);

			this->baseSequenceNumber  = Utils::Byte::Get2Bytes(data, 0);
			this->packetStatusCount   = Utils::Byte::Get2Bytes(data, 2);
			this->referenceTime       = parseReferencetime(data + 4u);
			this->feedbackPacketCount = Utils::Byte::Get1Byte(data, 7);

			// Make contentData point to the beginning of the chunks.
			uint8_t* contentData = data + FeedbackRtpTransportPacket::fixedHeaderSize;
			// Make contentLen be the available length for chunks.
			size_t contentLen = len - sizeof(CommonHeader) - sizeof(FeedbackPacket::Header) -
			                    FeedbackRtpTransportPacket::fixedHeaderSize;
			size_t offset{ 0u };
			size_t count{ 0u };
			size_t receivedPacketStatusCount{ 0u };

			while (count < this->packetStatusCount && contentLen > offset)
			{
				if (contentLen - offset < 2u)
				{
					MS_WARN_TAG(rtcp, "not enough space for chunk");

					this->isCorrect = false;

					return;
				}

				auto* chunk =
				  Chunk::Parse(contentData + offset, contentLen - offset, this->packetStatusCount - count);

				if (!chunk)
				{
					MS_WARN_TAG(rtcp, "invalid chunk");

					this->isCorrect = false;

					return;
				}

				this->chunks.push_back(chunk);
				this->deltasAndChunksSize += 2u;

				offset += 2u;
				count += chunk->GetCount();
				receivedPacketStatusCount += chunk->GetReceivedStatusCount();
			}

			if (this->packetStatusCount != count)
			{
				MS_WARN_TAG(rtcp, "provided packet status count does not match with content");
				this->isCorrect = false;

				return;
			}

			auto chunksIt = this->chunks.begin();

			while (chunksIt != this->chunks.end() && contentLen > offset)
			{
				size_t deltasOffset{ 0u };
				auto* chunk = *chunksIt;

				if (!chunk->AddDeltas(contentData + offset, contentLen - offset, this->deltas, deltasOffset))
				{
					MS_WARN_TAG(rtcp, "not enough space for deltas");

					this->isCorrect = false;

					// Delete all created chunks.
					for (auto* chunk : this->chunks)
					{
						delete chunk;
					}
					this->chunks.clear();

					return;
				}

				offset += deltasOffset;
				this->deltasAndChunksSize += deltasOffset;

				++chunksIt;
			}

			if (this->deltas.size() != receivedPacketStatusCount)
			{
				MS_WARN_TAG(rtcp, "received deltas does not match received status count");

				this->isCorrect = false;

				return;
			}
		}

		FeedbackRtpTransportPacket::~FeedbackRtpTransportPacket()
		{
			MS_TRACE();

			for (auto* chunk : this->chunks)
			{
				delete chunk;
			}
			this->chunks.clear();
		}

		bool FeedbackRtpTransportPacket::AddPacket(
		  uint16_t sequenceNumber, uint64_t timestamp, size_t maxRtcpPacketLen)
		{
			MS_TRACE();

			MS_ASSERT(!IsFull(), "packet is full");

			// Let's see if we must set our base.
			if (this->highestTimestamp == 0u)
			{
				MS_DEBUG_DEV("setting base");

				this->baseSequenceNumber    = sequenceNumber + 1;
				this->referenceTime         = static_cast<int32_t>((timestamp & 0x1FFFFFC0) / 64);
				this->highestSequenceNumber = sequenceNumber;
				this->highestTimestamp      = (timestamp >> 6) * 64; // IMPORTANT: Loose precision.

				return true;
			}

			// If the wide sequence number of the new packet is lower than the highest seen,
			// ignore it.
			// NOTE: Not very spec compliant but libwebrtc does it.
			// Also ignore if the sequence number matches the highest seen.
			if (!RTC::SeqManager<uint16_t>::IsSeqHigherThan(sequenceNumber, this->highestSequenceNumber))
			{
				return true;
			}

			// Check if there are too many missing packets.
			{
				auto missingPackets = sequenceNumber - (this->highestSequenceNumber + 1);

				if (missingPackets > FeedbackRtpTransportPacket::maxMissingPackets)
				{
					MS_WARN_DEV("RTP missing packet number exceeded");

					return false;
				}
			}

			// Deltas are represented as multiples of 250us.
			// NOTE: Read it as uint 64 to detect long elapsed times.
			uint64_t delta64 = (timestamp - this->highestTimestamp) * 4;

			if (delta64 > static_cast<uint64_t>(FeedbackRtpTransportPacket::maxPacketDelta))
			{
				MS_WARN_DEV(
				  "RTP packet delta exceeded [highestTimestamp:%" PRIu64 ", timestamp:%" PRIu64 "]",
				  this->highestTimestamp,
				  timestamp);

				return false;
			}

			// Delta in 16 bits signed.
			auto delta = static_cast<int16_t>(delta64);

			// Check whether another chunks and corresponding delta infos could be added.
			{
				// Fixed packet size.
				size_t size = FeedbackRtpPacket::GetSize();

				size += FeedbackRtpTransportPacket::fixedHeaderSize;
				size += this->deltasAndChunksSize;

				// Maximum size needed for another chunk and its delta infos.
				size += 2u;
				size += 2u;

				// 32 bits padding.
				size += (-size) & 3;

				if (size > maxRtcpPacketLen)
				{
					MS_WARN_DEV("maximum packet size exceeded");

					return false;
				}
			}

			// Fill a chunk.
			FillChunk(this->highestSequenceNumber, sequenceNumber, delta);

			// Update highest sequence number and timestamp seen.
			this->highestSequenceNumber = sequenceNumber;
			this->highestTimestamp      = timestamp;

			// Add entry to received packets container.
			this->receivedPackets.emplace_back(sequenceNumber, delta);

			return true;
		}

		void FeedbackRtpTransportPacket::Dump() const
		{
			MS_TRACE();

			MS_DUMP("<FeedbackRtpTransportPacket>");
			MS_DUMP("  base sequence         : %" PRIu16, this->baseSequenceNumber);
			MS_DUMP("  packet status count   : %" PRIu16, this->packetStatusCount);
			MS_DUMP("  reference time        : %" PRIi32, this->referenceTime);
			MS_DUMP("  feedback packet count : %" PRIu8, this->feedbackPacketCount);
			MS_DUMP("  size                  : %zu", GetSize());

			for (auto* chunk : this->chunks)
			{
				chunk->Dump();
			}

			if (this->receivedPackets.size() != this->deltas.size())
			{
				MS_ERROR(
				  "received packets and number of deltas mismatch [packets:%zu, deltas:%zu]",
				  this->receivedPackets.size(),
				  this->deltas.size());

				for (auto& packet : this->receivedPackets)
				{
					MS_DUMP(
					  "  seqNumber:%" PRIu16 ", delta(ms):%" PRIi16,
					  packet.sequenceNumber,
					  static_cast<int16_t>(packet.delta / 4));
				}
			}
			else
			{
				auto receivedPacketIt = this->receivedPackets.begin();
				auto deltaIt          = this->deltas.begin();

				MS_DUMP("  <Deltas>");

				while (receivedPacketIt != this->receivedPackets.end())
				{
					MS_DUMP(
					  "    seqNumber:%" PRIu16 ", delta(ms):%" PRIi16,
					  receivedPacketIt->sequenceNumber,
					  static_cast<int16_t>(receivedPacketIt->delta / 4));

					if (receivedPacketIt->delta != *deltaIt)
						MS_ERROR("delta block does not coincide with the received value");

					receivedPacketIt++;
					deltaIt++;
				}

				MS_DUMP("  </Deltas>");
			}

			MS_DUMP("</FeedbackRtpTransportPacket>");
		}

		size_t FeedbackRtpTransportPacket::Serialize(uint8_t* buffer)
		{
			MS_TRACE();

			// Add chunks for status packets that may not be represented yet.
			AddPendingChunks();

			size_t offset = FeedbackPacket::Serialize(buffer);

			// Base sequence number.
			Utils::Byte::Set2Bytes(buffer, offset, this->baseSequenceNumber);
			offset += 2;

			// Packet status count.
			Utils::Byte::Set2Bytes(buffer, offset, this->packetStatusCount);
			offset += 2;

			// Reference time.
			Utils::Byte::Set3Bytes(buffer, offset, static_cast<uint32_t>(this->referenceTime));
			offset += 3;

			// Feedback packet count.
			Utils::Byte::Set1Byte(buffer, offset, this->feedbackPacketCount);
			offset += 1;

			// Serialize chunks.
			for (auto* chunk : this->chunks)
			{
				offset += chunk->Serialize(buffer + offset);
			}

			// Serialize deltas.
			for (auto delta : this->deltas)
			{
				if (delta <= 255)
				{
					Utils::Byte::Set1Byte(buffer, offset, delta);
					offset += 1u;
				}
				else
				{
					Utils::Byte::Set2Bytes(buffer, offset, delta);
					offset += 2u;
				}
			}

			// 32 bits padding.
			size_t padding = (-offset) & 3;

			for (size_t i{ 0u }; i < padding; ++i)
			{
				buffer[offset + i] = 0u;
			}

			offset += padding;

			return offset;
		}

		void FeedbackRtpTransportPacket::FillChunk(
		  uint16_t previousSequenceNumber, uint16_t sequenceNumber, int16_t delta)
		{
			MS_TRACE();

			auto missingPackets = static_cast<uint16_t>(sequenceNumber - (previousSequenceNumber + 1));

			if (missingPackets > 0)
			{
				// Create a long run chunk before processing this packet, if needed.
				if (this->context.statuses.size() >= 7 && this->context.allSameStatus)
				{
					CreateRunLengthChunk(this->context.currentStatus, this->context.statuses.size());

					this->context.statuses.clear();
					this->context.currentStatus = Status::None;
				}

				this->context.currentStatus = Status::NotReceived;
				size_t representedPackets{ 0u };

				// Fill statuses vector.
				for (uint8_t i{ 0u }; i < missingPackets && this->context.statuses.size() < 7; ++i)
				{
					this->context.statuses.emplace_back(Status::NotReceived);
					representedPackets++;
				}

				// Create a two bit vector if needed.
				if (this->context.statuses.size() == 7)
				{
					// Fill a vector chunk.
					CreateTwoBitVectorChunk(this->context.statuses);

					this->context.statuses.clear();
					this->context.currentStatus = Status::None;
				}

				missingPackets -= representedPackets;

				// Not all missing packets have been represented.
				if (missingPackets != 0)
				{
					// Fill a run length chunk with the remaining missing packets.
					CreateRunLengthChunk(Status::NotReceived, missingPackets);

					this->context.statuses.clear();
					this->context.currentStatus = Status::None;
				}
			}

			Status status = (delta <= 255) ? Status::SmallDelta : Status::LargeDelta;

			// Create a long run chunk before processing this packet, if needed.
			// clang-format off
			if (
				this->context.statuses.size() >= 7 &&
				this->context.allSameStatus &&
				status != this->context.currentStatus
			)
			// clang-format on
			{
				CreateRunLengthChunk(this->context.currentStatus, this->context.statuses.size());

				this->context.statuses.clear();
			}

			this->context.statuses.emplace_back(status);
			this->deltas.emplace_back(delta);
			this->deltasAndChunksSize += (status == Status::SmallDelta) ? 1u : 2u;

			// Update context info.

			// clang-format off
			if (
				this->context.currentStatus == Status::None ||
				(this->context.allSameStatus && this->context.currentStatus == status)
			)
			// clang-format on
			{
				this->context.allSameStatus = true;
			}
			else
			{
				this->context.allSameStatus = false;
			}

			this->context.currentStatus = status;

			// Not enough packet infos for creating a chunk.
			if (this->context.statuses.size() < 7)
			{
				return;
			}
			// 7 packet infos with heterogeneous status, create the chunk.
			else if (this->context.statuses.size() == 7 && !this->context.allSameStatus)
			{
				// Reset current status.
				this->context.currentStatus = Status::None;

				// Fill a vector chunk and return.
				CreateTwoBitVectorChunk(this->context.statuses);

				this->context.statuses.clear();
			}
		}

		void FeedbackRtpTransportPacket::CreateRunLengthChunk(Status status, uint16_t count)
		{
			auto* chunk = new RunLengthChunk(status, count);

			this->chunks.push_back(chunk);
			this->packetStatusCount += count;
			this->deltasAndChunksSize += 2u;
		}

		void FeedbackRtpTransportPacket::CreateOneBitVectorChunk(std::vector<Status>& statuses)
		{
			auto* chunk = new OneBitVectorChunk(statuses);

			this->chunks.push_back(chunk);
			this->packetStatusCount += statuses.size();
			this->deltasAndChunksSize += 2u;
		}

		void FeedbackRtpTransportPacket::CreateTwoBitVectorChunk(std::vector<Status>& statuses)
		{
			auto* chunk = new TwoBitVectorChunk(statuses);

			this->chunks.push_back(chunk);
			this->packetStatusCount += statuses.size();
			this->deltasAndChunksSize += 2u;
		}

		void FeedbackRtpTransportPacket::AddPendingChunks()
		{
			// No pending status packets.
			if (this->context.statuses.size() == 0)
				return;

			if (this->context.allSameStatus)
			{
				CreateRunLengthChunk(this->context.currentStatus, this->context.statuses.size());

				this->context.statuses.clear();
			}
			else
			{
				MS_ASSERT(this->context.statuses.size() < 7, "already 7 status packets present");

				CreateTwoBitVectorChunk(this->context.statuses);

				this->context.statuses.clear();
			}
		}

		FeedbackRtpTransportPacket::Chunk* FeedbackRtpTransportPacket::Chunk::Parse(
		  const uint8_t* data, size_t len, size_t count)
		{
			MS_TRACE();

			if (len < 2u)
			{
				MS_WARN_TAG(rtcp, "not enough space for FeedbackRtpTransportPacket chunk, discarded");

				return nullptr;
			}

			auto bytes        = Utils::Byte::Get2Bytes(data, 0);
			uint8_t chunkType = (bytes >> 15) & 0x01;

			// Run length chunk.
			if (chunkType == 0)
			{
				auto* chunk = new RunLengthChunk(bytes);

				// Verify that the status is a valid one.
				switch (chunk->GetStatus())
				{
					case Status::NotReceived:
					case Status::SmallDelta:
					case Status::LargeDelta:
					{
						return chunk;
					}

					default:
					{
						MS_WARN_DEV("invalid status for a run length chunk");

						delete chunk;

						return nullptr;
					}
				}
			}
			// Vector chunk.
			else
			{
				uint8_t symbolSize = data[0] & 0x40;

				if (symbolSize == 0)
					return new OneBitVectorChunk(bytes, count);
				else
					return new TwoBitVectorChunk(bytes, count);
			}

			return nullptr;
		}

		FeedbackRtpTransportPacket::RunLengthChunk::RunLengthChunk(uint16_t buffer)
		{
			MS_TRACE();

			this->status = static_cast<Status>((buffer >> 13) & 0x03);
			this->count  = buffer & 0x1FFF;
		}

		bool FeedbackRtpTransportPacket::RunLengthChunk::AddDeltas(
		  const uint8_t* data, size_t len, std::vector<int16_t>& deltas, size_t& offset)
		{
			MS_TRACE();

			// No delta to be added.
			if (this->status == Status::NotReceived)
			{
				return true;
			}
			else if (this->status == Status::SmallDelta)
			{
				if (len < this->count * 1u)
				{
					MS_WARN_TAG(rtcp, "not enough space for small deltas");

					return false;
				}

				for (size_t i{ 0 }; i < this->count; ++i)
				{
					auto delta = static_cast<int16_t>(Utils::Byte::Get1Byte(data, offset));

					deltas.push_back(delta);
					offset += 1u;
				}
			}
			else if (this->status == Status::LargeDelta)
			{
				if (len < this->count * 2u)
				{
					MS_WARN_TAG(rtcp, "not enough space for large deltas");

					return false;
				}

				for (size_t i{ 0 }; i < this->count; ++i)
				{
					auto delta = static_cast<int16_t>(Utils::Byte::Get2Bytes(data, offset));

					deltas.push_back(delta);
					offset += 2u;
				}
			}

			return true;
		}

		void FeedbackRtpTransportPacket::RunLengthChunk::Dump() const
		{
			MS_TRACE();

			MS_DUMP("  <FeedbackRtpTransportPacket::RunLengthChunk>");
			MS_DUMP("    status : %s", FeedbackRtpTransportPacket::Status2String[this->status].c_str());
			MS_DUMP("    count  : %" PRIu16, this->count);
			MS_DUMP("  </FeedbackRtpTransportPacket::RunLengthChunk>");
		}

		size_t FeedbackRtpTransportPacket::RunLengthChunk::GetReceivedStatusCount() const
		{
			MS_TRACE();

			if (this->status == Status::SmallDelta || this->status == Status::LargeDelta)
			{
				return this->count;
			}
			else
			{
				return 0;
			}
		}

		size_t FeedbackRtpTransportPacket::RunLengthChunk::Serialize(uint8_t* buffer)
		{
			MS_TRACE();

			uint16_t bytes{ 0x0000 };

			bytes |= this->status << 13;
			bytes |= this->count & 0x1FFF;

			Utils::Byte::Set2Bytes(buffer, 0, bytes);

			return 2u;
		}

		FeedbackRtpTransportPacket::OneBitVectorChunk::OneBitVectorChunk(uint16_t buffer, size_t count)
		{
			MS_TRACE();

			MS_ASSERT(buffer & 0x8000, "invalid one bit vector chunk");

			for (size_t i{ 0u }; i < 14 && count > 0; ++i, --count)
			{
				auto status = static_cast<Status>((buffer >> (14 - 1 - i)) & 0x01);

				this->statuses.emplace_back(status);
			}
		}

		bool FeedbackRtpTransportPacket::OneBitVectorChunk::AddDeltas(
		  const uint8_t* data, size_t len, std::vector<int16_t>& deltas, size_t& offset)
		{
			MS_TRACE();

			for (auto status : this->statuses)
			{
				if (status == Status::NotReceived)
				{
					continue;
				}
				else if (status == Status::SmallDelta)
				{
					if (len < 1u)
					{
						MS_WARN_TAG(rtcp, "not enough space for small delta");

						return false;
					}

					auto delta = static_cast<int16_t>(Utils::Byte::Get1Byte(data, offset));

					deltas.push_back(delta);
					offset += 1u;
					len -= 1u;

					continue;
				}
				else
				{
					MS_WARN_TAG(rtcp, "invalid status for one bit vector chunk");

					return false;
				}
			}

			return true;
		}

		void FeedbackRtpTransportPacket::OneBitVectorChunk::Dump() const
		{
			MS_TRACE();

			std::ostringstream out;

			// Dump status slots.
			for (auto status : this->statuses)
			{
				out << "|" << FeedbackRtpTransportPacket::Status2String[status];
			}

			// Dump empty slots.
			for (size_t i{ this->statuses.size() }; i < 14; ++i)
			{
				out << "|--";
			}

			out << "|";

			MS_DUMP("  <FeedbackRtpTransportPacket::OneBitVectorChunk>");
			MS_DUMP("    %s", out.str().c_str());
			MS_DUMP("  </FeedbackRtpTransportPacket::OneBitVectorChunk>");
		}

		size_t FeedbackRtpTransportPacket::OneBitVectorChunk::GetReceivedStatusCount() const
		{
			MS_TRACE();

			size_t count{ 0 };

			for (auto status : statuses)
			{
				if (status == Status::SmallDelta || status == Status::LargeDelta)
					count++;
			}

			return count;
		}

		size_t FeedbackRtpTransportPacket::OneBitVectorChunk::Serialize(uint8_t* buffer)
		{
			MS_TRACE();

			MS_ASSERT(this->statuses.size() <= 14, "packet info size must be 14 or less");

			uint16_t bytes{ 0x8000 };
			uint8_t i{ 13u };

			for (auto status : this->statuses)
			{
				bytes |= status << i;
				i -= 1;
			}

			Utils::Byte::Set2Bytes(buffer, 0, bytes);

			return 2u;
		}

		FeedbackRtpTransportPacket::TwoBitVectorChunk::TwoBitVectorChunk(uint16_t buffer, size_t count)
		{
			MS_TRACE();

			MS_ASSERT(buffer & 0xC000, "invalid two bit vector chunk");

			for (size_t i{ 0u }; i < 7 && count > 0; ++i, --count)
			{
				auto status = static_cast<Status>((buffer >> 2 * (7 - 1 - i)) & 0x03);

				this->statuses.emplace_back(status);
			}
		}

		bool FeedbackRtpTransportPacket::TwoBitVectorChunk::AddDeltas(
		  const uint8_t* data, size_t len, std::vector<int16_t>& deltas, size_t& offset)
		{
			MS_TRACE();

			for (auto status : this->statuses)
			{
				if (status == Status::NotReceived)
				{
					continue;
				}
				else if (status == Status::SmallDelta)
				{
					if (len < 1u)
					{
						MS_WARN_TAG(rtcp, "not enough space for small delta");

						return false;
					}

					auto delta = static_cast<int16_t>(Utils::Byte::Get1Byte(data, offset));

					deltas.push_back(delta);
					offset += 1u;
					len -= 1u;

					continue;
				}
				else if (status == Status::LargeDelta)
				{
					if (len < 2u)
					{
						MS_WARN_TAG(rtcp, "not enough space for large delta");

						return false;
					}

					auto delta = static_cast<int16_t>(Utils::Byte::Get2Bytes(data, offset));

					deltas.push_back(delta);
					offset += 2u;
					len -= 2u;

					continue;
				}
			}

			return true;
		}

		void FeedbackRtpTransportPacket::TwoBitVectorChunk::Dump() const
		{
			MS_TRACE();

			std::ostringstream out;

			// Dump status slots.
			for (auto status : this->statuses)
			{
				out << "|" << FeedbackRtpTransportPacket::Status2String[status];
			}

			// Dump empty slots.
			for (size_t i{ this->statuses.size() }; i < 7; ++i)
			{
				out << "|--";
			}

			out << "|";

			MS_DUMP("  <FeedbackRtpTransportPacket::TwoBitVectorChunk>");
			MS_DUMP("    %s", out.str().c_str());
			MS_DUMP("  </FeedbackRtpTransportPacket::TwoBitVectorChunk>");
		}

		size_t FeedbackRtpTransportPacket::TwoBitVectorChunk::GetReceivedStatusCount() const
		{
			MS_TRACE();

			size_t count{ 0 };

			for (auto status : statuses)
			{
				if (status == Status::SmallDelta || status == Status::LargeDelta)
					count++;
			}

			return count;
		}
		size_t FeedbackRtpTransportPacket::TwoBitVectorChunk::Serialize(uint8_t* buffer)
		{
			MS_TRACE();

			MS_ASSERT(this->statuses.size() <= 7, "packet info size must be 7 or less");

			uint16_t bytes{ 0x8000 };
			uint8_t i{ 12u };

			bytes |= 0x01 << 14;

			for (auto status : this->statuses)
			{
				bytes |= status << i;
				i -= 2;
			}

			Utils::Byte::Set2Bytes(buffer, 0, bytes);

			return 2u;
		}
	} // namespace RTCP
} // namespace RTC
