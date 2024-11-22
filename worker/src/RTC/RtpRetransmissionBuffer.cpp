#define MS_CLASS "RTC::RtpRetransmissionBuffer"
// #define MS_LOG_DEV_LEVEL 3

#include "RTC/RtpRetransmissionBuffer.hpp"
#include "Logger.hpp"
#include "RTC/SeqManager.hpp"

namespace RTC
{
	/* Class methods. */

	RtpRetransmissionBuffer::Item* RtpRetransmissionBuffer::FillItem(
	  RtpRetransmissionBuffer::Item* item,
	  RTC::RtpPacket* packet,
	  std::shared_ptr<RTC::RtpPacket>& sharedPacket)
	{
		MS_TRACE();

		// Store original packet into the item. Only clone once and only if
		// necessary.
		//
		// NOTE: This must be done BEFORE assigning item->packet = sharedPacket,
		// otherwise the value being copied in item->packet will remain nullptr.
		// This is because we are copying an **empty** shared_ptr into another
		// shared_ptr (item->packet), so future value assigned via reset() in the
		// former doesn't update the value in the copy.
		if (!sharedPacket)
		{
			sharedPacket.reset(packet->Clone());
		}

		// Store original packet and some extra info into the item.
		item->packet         = sharedPacket;
		item->ssrc           = packet->GetSsrc();
		item->sequenceNumber = packet->GetSequenceNumber();
		item->timestamp      = packet->GetTimestamp();

		return item;
	}

	/* Instance methods. */

	RtpRetransmissionBuffer::RtpRetransmissionBuffer(
	  uint16_t maxItems, uint32_t maxRetransmissionDelayMs, uint32_t clockRate)
	  : maxItems(maxItems), maxRetransmissionDelayMs(maxRetransmissionDelayMs), clockRate(clockRate)
	{
		MS_TRACE();

		MS_ASSERT(maxItems > 0u, "maxItems must be greater than 0");
	}

	RtpRetransmissionBuffer::~RtpRetransmissionBuffer()
	{
		MS_TRACE();

		Clear();
	}

	RtpRetransmissionBuffer::Item* RtpRetransmissionBuffer::Get(uint16_t seq) const
	{
		MS_TRACE();

		const auto* oldestItem = GetOldest();

		if (!oldestItem)
		{
			return nullptr;
		}

		if (RTC::SeqManager<uint16_t>::IsSeqLowerThan(seq, oldestItem->sequenceNumber))
		{
			return nullptr;
		}

		const uint16_t idx = seq - oldestItem->sequenceNumber;

		if (static_cast<size_t>(idx) > this->buffer.size() - 1)
		{
			return nullptr;
		}

		return this->buffer.at(idx);
	}

	/**
	 * This method tries to insert given packet into the buffer. Here we assume
	 * that packet seq number is legitimate according to the content of the buffer.
	 * We discard the packet if too old and also discard it if its timestamp does
	 * not properly fit (by ensuring that elements in the buffer are not only
	 * ordered by increasing seq but also that their timestamp are incremental).
	 */
	void RtpRetransmissionBuffer::Insert(
	  RTC::RtpPacket* packet, std::shared_ptr<RTC::RtpPacket>& sharedPacket)
	{
		MS_TRACE();

		const auto ssrc      = packet->GetSsrc();
		const auto seq       = packet->GetSequenceNumber();
		const auto timestamp = packet->GetTimestamp();

		MS_DEBUG_DEV("packet [seq:%" PRIu16 ", timestamp:%" PRIu32 "]", seq, timestamp);

		// Buffer is empty, so just insert new item.
		if (this->buffer.empty())
		{
			MS_DEBUG_DEV("buffer empty [seq:%" PRIu16 ", timestamp:%" PRIu32 "]", seq, timestamp);

			auto* item = new Item();

			this->buffer.push_back(RtpRetransmissionBuffer::FillItem(item, packet, sharedPacket));

			return;
		}

		auto* oldestItem = GetOldest();
		auto* newestItem = GetNewest();

		// Special case: Received packet has lower seq than newest packet in the
		// buffer, however its timestamp is higher. If so, clear the whole buffer.
		if (
		  RTC::SeqManager<uint16_t>::IsSeqLowerThan(seq, newestItem->sequenceNumber) &&
		  RTC::SeqManager<uint32_t>::IsSeqHigherThan(timestamp, newestItem->timestamp))
		{
			MS_WARN_TAG(
			  rtp,
			  "packet has lower seq but higher timestamp than newest packet in the buffer, emptying the buffer [ssrc:%" PRIu32
			  ", seq:%" PRIu16 ", timestamp:%" PRIu32 "]",
			  ssrc,
			  seq,
			  timestamp);

			Clear();

			auto* item = new Item();

			this->buffer.push_back(RtpRetransmissionBuffer::FillItem(item, packet, sharedPacket));

			return;
		}

		// Clear too old packets in the buffer.
		// NOTE: Here we must consider the case in which, due for example to huge
		// packet loss, received packet has higher timestamp but "older" seq number
		// than the newest packet in the buffer and, if so, use it to clear too old
		// packets rather than the newest packet in the buffer.
		auto newestTimestamp =
		  RTC::SeqManager<uint32_t>::IsSeqHigherThan(timestamp, newestItem->timestamp)
		    ? timestamp
		    : newestItem->timestamp;

		// ClearTooOldByTimestamp() returns true if at least one packet has been
		// removed from the front.
		if (ClearTooOldByTimestamp(newestTimestamp))
		{
			// Buffer content has been modified so we must check it again.
			if (this->buffer.empty())
			{
				MS_WARN_TAG(
				  rtp,
				  "buffer empty after clearing too old packets [seq:%" PRIu16 ", timestamp:%" PRIu32 "]",
				  seq,
				  timestamp);

				auto* item = new Item();

				this->buffer.push_back(RtpRetransmissionBuffer::FillItem(item, packet, sharedPacket));

				return;
			}

			oldestItem = GetOldest();
			newestItem = GetNewest();
		}

		MS_ASSERT(oldestItem != nullptr, "oldest item doesn't exist");
		MS_ASSERT(newestItem != nullptr, "newest item doesn't exist");

		// Packet arrived in order (its seq is higher than seq of the newest stored
		// packet) so will become the newest one in the buffer.
		if (RTC::SeqManager<uint16_t>::IsSeqHigherThan(seq, newestItem->sequenceNumber))
		{
			MS_DEBUG_DEV("packet in order [seq:%" PRIu16 ", timestamp:%" PRIu32 "]", seq, timestamp);

			// Ensure that the timestamp of the packet is equal or higher than the
			// timestamp of the newest stored packet.
			if (RTC::SeqManager<uint32_t>::IsSeqLowerThan(timestamp, newestItem->timestamp))
			{
				MS_WARN_TAG(
				  rtp,
				  "packet has higher seq but lower timestamp than newest packet in the buffer, discarding it [ssrc:%" PRIu32
				  ", seq:%" PRIu16 ", timestamp:%" PRIu32 "]",
				  ssrc,
				  seq,
				  timestamp);

				return;
			}

			// Calculate how many blank slots it would be necessary to add when
			// pushing new item to the back of the buffer.
			uint16_t numBlankSlots = seq - newestItem->sequenceNumber - 1;

			// We may have to remove oldest items not to exceed the maximum size of
			// the buffer.
			if (this->buffer.size() + numBlankSlots + 1 > this->maxItems)
			{
				const uint16_t numItemsToRemove = this->buffer.size() + numBlankSlots + 1 - this->maxItems;

				// If num of items to be removed exceed buffer size minus one (needed to
				// allocate current packet) then we must clear the entire buffer.
				if (numItemsToRemove > this->buffer.size() - 1)
				{
					MS_WARN_TAG(
					  rtp,
					  "packet has too high seq and forces buffer emptying [ssrc:%" PRIu32 ", seq:%" PRIu16
					  ", timestamp:%" PRIu32 "]",
					  ssrc,
					  seq,
					  timestamp);

					numBlankSlots = 0u;

					Clear();
				}
				else
				{
					MS_DEBUG_DEV(
					  "calling RemoveOldest(%" PRIu16 ") [bufferSize:%zu, numBlankSlots:%" PRIu16
					  ", maxItems:%" PRIu16 "]",
					  numItemsToRemove,
					  this->buffer.size(),
					  numBlankSlots,
					  this->maxItems);

					RemoveOldest(numItemsToRemove);
				}
			}

			// Push blank slots to the back.
			for (uint16_t i{ 0u }; i < numBlankSlots; ++i)
			{
				this->buffer.push_back(nullptr);
			}

			// Push the packet, which becomes the newest one in the buffer.
			auto* item = new Item();

			this->buffer.push_back(RtpRetransmissionBuffer::FillItem(item, packet, sharedPacket));
		}
		// Packet arrived out order and its seq is less than seq of the oldest
		// stored packet, so will become the oldest one in the buffer.
		else if (RTC::SeqManager<uint16_t>::IsSeqLowerThan(seq, oldestItem->sequenceNumber))
		{
			MS_DEBUG_DEV(
			  "packet out of order and older than oldest packet in the buffer [seq:%" PRIu16
			  ", timestamp:%" PRIu32 "]",
			  seq,
			  timestamp);

			// Ensure that packet is not too old to be stored.
			if (IsTooOldTimestamp(timestamp, newestItem->timestamp))
			{
				MS_WARN_DEV(
				  "packet's timestamp too old, discarding it [seq:%" PRIu16 ", timestamp:%" PRIu32 "]",
				  seq,
				  timestamp);

				return;
			}

			// Ensure that the timestamp of the packet is equal or less than the
			// timestamp of the oldest stored packet.
			if (RTC::SeqManager<uint32_t>::IsSeqHigherThan(timestamp, oldestItem->timestamp))
			{
				MS_WARN_TAG(
				  rtp,
				  "packet has lower seq but higher timestamp than oldest packet in the buffer, discarding it [ssrc:%" PRIu32
				  ", seq:%" PRIu16 ", timestamp:%" PRIu32 "]",
				  ssrc,
				  seq,
				  timestamp);

				return;
			}

			// Calculate how many blank slots it would be necessary to add when
			// pushing new item to the fton of the buffer.
			const uint16_t numBlankSlots = oldestItem->sequenceNumber - seq - 1;

			// If adding this packet (and needed blank slots) to the front makes the
			// buffer exceed its max size, discard this packet.
			if (this->buffer.size() + numBlankSlots + 1 > this->maxItems)
			{
				MS_WARN_TAG(
				  rtp,
				  "discarding received old packet to not exceed max buffer size [ssrc:%" PRIu32
				  ", seq:%" PRIu16 ", timestamp:%" PRIu32 "]",
				  ssrc,
				  seq,
				  timestamp);

				return;
			}

			// Push blank slots to the front.
			for (uint16_t i{ 0u }; i < numBlankSlots; ++i)
			{
				this->buffer.push_front(nullptr);
			}

			// Insert the packet, which becomes the oldest one in the buffer.
			auto* item = new Item();

			this->buffer.push_front(RtpRetransmissionBuffer::FillItem(item, packet, sharedPacket));
		}
		// Otherwise packet must be inserted between oldest and newest stored items
		// so there is already an allocated slot for it.
		else
		{
			MS_DEBUG_DEV(
			  "packet out of order and in between oldest and newest packets in the buffer [seq:%" PRIu16
			  ", timestamp:%" PRIu32 "]",
			  seq,
			  timestamp);

			// Let's check if an item already exist in same position. If so, assume
			// it's duplicated.
			auto* item = Get(seq);

			if (item)
			{
				MS_DEBUG_DEV(
				  "packet already in the buffer, discarding [seq:%" PRIu16 ", timestamp:%" PRIu32 "]",
				  seq,
				  timestamp);

				return;
			}

			// idx is the intended position of the received packet in the buffer.
			const uint16_t idx = seq - oldestItem->sequenceNumber;

			// Validate that packet timestamp is equal or higher than the timestamp of
			// the immediate older packet (if any).
			for (int32_t idx2 = idx - 1; idx2 >= 0; --idx2)
			{
				const auto* olderItem = this->buffer.at(idx2);

				// Blank slot, continue.
				if (!olderItem)
				{
					continue;
				}

				// We are done.
				if (timestamp >= olderItem->timestamp)
				{
					break;
				}
				else
				{
					MS_WARN_TAG(
					  rtp,
					  "packet timestamp is lower than timestamp of immediate older packet in the buffer, discarding it [ssrc:%" PRIu32
					  ", seq:%" PRIu16 ", timestamp:%" PRIu32 "]",
					  ssrc,
					  seq,
					  timestamp);

					return;
				}
			}

			// Validate that packet timestamp is equal or less than the timestamp of
			// the immediate newer packet (if any).
			for (size_t idx2 = idx + 1; idx2 < this->buffer.size(); ++idx2)
			{
				const auto* newerItem = this->buffer.at(idx2);

				// Blank slot, continue.
				if (!newerItem)
				{
					continue;
				}

				// We are done.
				if (timestamp <= newerItem->timestamp)
				{
					break;
				}
				else
				{
					MS_WARN_TAG(
					  rtp,
					  "packet timestamp is higher than timestamp of immediate newer packet in the buffer, discarding it [ssrc:%" PRIu32
					  ", seq:%" PRIu16 ", timestamp:%" PRIu32 "]",
					  ssrc,
					  seq,
					  timestamp);

					return;
				}
			}

			// Store the packet.
			item = new Item();

			this->buffer[idx] = RtpRetransmissionBuffer::FillItem(item, packet, sharedPacket);
		}

		MS_ASSERT(
		  this->buffer.size() <= this->maxItems,
		  "buffer contains %zu items (more than %" PRIu16 " max items)",
		  this->buffer.size(),
		  this->maxItems);
	}

	void RtpRetransmissionBuffer::Clear()
	{
		MS_TRACE();

		for (auto* item : this->buffer)
		{
			if (!item)
			{
				continue;
			}

			// Reset the stored item (decrease RTP packet shared pointer counter).
			item->Reset();

			delete item;
		}

		this->buffer.clear();
	}

	void RtpRetransmissionBuffer::Dump() const
	{
		MS_TRACE();

		MS_DUMP("<RtpRetransmissionBuffer>");
		MS_DUMP("  buffer [size:%zu, maxSize:%" PRIu16 "]", this->buffer.size(), this->maxItems);
		if (!this->buffer.empty())
		{
			const auto* oldestItem = GetOldest();
			const auto* newestItem = GetNewest();

			MS_DUMP(
			  "  oldest item [seq:%" PRIu16 ", timestamp:%" PRIu32 "]",
			  oldestItem->sequenceNumber,
			  oldestItem->timestamp);
			MS_DUMP(
			  "  newest item [seq:%" PRIu16 ", timestamp:%" PRIu32 "]",
			  newestItem->sequenceNumber,
			  newestItem->timestamp);
			MS_DUMP(
			  "  buffer window: %" PRIu32 "ms",
			  static_cast<uint32_t>(newestItem->timestamp * 1000 / this->clockRate) -
			    static_cast<uint32_t>(oldestItem->timestamp * 1000 / this->clockRate));
		}
		MS_DUMP("</RtpRetransmissionBuffer>");
	}

	RtpRetransmissionBuffer::Item* RtpRetransmissionBuffer::GetOldest() const
	{
		MS_TRACE();

		if (this->buffer.empty())
		{
			return nullptr;
		}

		return this->buffer.front();
	}

	RtpRetransmissionBuffer::Item* RtpRetransmissionBuffer::GetNewest() const
	{
		MS_TRACE();

		if (this->buffer.empty())
		{
			return nullptr;
		}

		return this->buffer.back();
	}

	void RtpRetransmissionBuffer::RemoveOldest()
	{
		MS_TRACE();

		if (this->buffer.empty())
		{
			return;
		}

		auto* item = this->buffer.front();

		// Reset the stored item (decrease RTP packet shared pointer counter).
		item->Reset();

		delete item;

		this->buffer.pop_front();

		MS_DEBUG_DEV("removed 1 item from the front");

		// Remove all nullptr elements from the beginning of the buffer.
		// NOTE: Calling front on an empty container is undefined.
		size_t numItemsRemoved{ 0u };

		while (!this->buffer.empty() && this->buffer.front() == nullptr)
		{
			this->buffer.pop_front();

			++numItemsRemoved;
		}

		if (numItemsRemoved)
		{
			MS_DEBUG_DEV("removed %zu blank slots from the front", numItemsRemoved);
		}
	}

	void RtpRetransmissionBuffer::RemoveOldest(uint16_t numItems)
	{
		MS_TRACE();

		MS_ASSERT(
		  numItems <= this->buffer.size(),
		  "attempting to remove more items than current buffer size [numItems:%" PRIu16
		  ", bufferSize:%zu]",
		  numItems,
		  this->buffer.size());

		const size_t intendedBufferSize = this->buffer.size() - numItems;

		while (this->buffer.size() > intendedBufferSize)
		{
			RemoveOldest();
		}
	}

	bool RtpRetransmissionBuffer::ClearTooOldByTimestamp(uint32_t newestTimestamp)
	{
		MS_TRACE();

		RtpRetransmissionBuffer::Item* oldestItem{ nullptr };
		bool itemsRemoved{ false };

		// Go through all buffer items starting with the first and free all items
		// that contain too old packets.
		while ((oldestItem = GetOldest()))
		{
			if (IsTooOldTimestamp(oldestItem->timestamp, newestTimestamp))
			{
				RemoveOldest();

				itemsRemoved = true;
			}
			// If current oldest stored packet is not too old, exit the loop since we
			// know that packets stored after it are guaranteed to be newer.
			else
			{
				break;
			}
		}

		return itemsRemoved;
	}

	bool RtpRetransmissionBuffer::IsTooOldTimestamp(uint32_t timestamp, uint32_t newestTimestamp) const
	{
		MS_TRACE();

		if (RTC::SeqManager<uint32_t>::IsSeqHigherThan(timestamp, newestTimestamp))
		{
			return false;
		}

		const int64_t diffTs = newestTimestamp - timestamp;

		return static_cast<uint32_t>(diffTs * 1000 / this->clockRate) > this->maxRetransmissionDelayMs;
	}

	void RtpRetransmissionBuffer::Item::Reset()
	{
		MS_TRACE();

		this->packet.reset();
		this->ssrc           = 0u;
		this->sequenceNumber = 0u;
		this->timestamp      = 0u;
		this->resentAtMs     = 0u;
		this->sentTimes      = 0u;
	}
} // namespace RTC
