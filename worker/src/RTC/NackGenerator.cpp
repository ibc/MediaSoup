#define MS_CLASS "RTC::NackGenerator"
// #define MS_LOG_DEV

#include "RTC/NackGenerator.hpp"
#include "DepLibUV.hpp"
#include "Logger.hpp"

namespace RTC
{
	/* Static. */

	constexpr uint32_t MaxPacketAge{ 5000 };
	constexpr size_t MaxNackPackets{ 1000 };
	constexpr uint32_t DefaultRtt{ 100 };
	constexpr uint8_t MaxNackRetries{ 8 };
	constexpr uint64_t TimerInterval{ 50 };

	/* Instance methods. */

	NackGenerator::NackGenerator(Listener* listener) : listener(listener), rtt(DefaultRtt)
	{
		MS_TRACE();

		// Set the timer.
		this->timer = new Timer(this);
	}

	NackGenerator::~NackGenerator()
	{
		MS_TRACE();

		// Close the timer.
		this->timer->Destroy();
	}

	// Returns true if this is a found nacked packet. False otherwise.
	bool NackGenerator::ReceivePacket(RTC::RtpPacket* packet)
	{
		MS_TRACE();

		uint32_t seq32 = packet->GetExtendedSequenceNumber();

		if (!this->started)
		{
			this->lastSeq32 = seq32;
			this->started   = true;

			return false;
		}

		// If a key frame remove all the items in the nack list older than this seq.
		if (packet->IsKeyFrame())
			RemoveFromNackListOlderThan(packet);

		// Obviously never nacked, so ignore.
		if (seq32 == this->lastSeq32)
		{
			return false;
		}
		if (seq32 == this->lastSeq32 + 1)
		{
			this->lastSeq32++;

			return false;
		}

		// May be an out of order packet, or already handled retransmitted packet,
		// or a retransmitted packet.
		if (seq32 < this->lastSeq32)
		{
			auto it = this->nackList.find(seq32);

			// It was a nacked packet.
			if (it != this->nackList.end())
			{
				MS_DEBUG_TAG(
				  rtx,
				  "NACKed packet received [ssrc:%" PRIu32 ", seq:%" PRIu16 "]",
				  packet->GetSsrc(),
				  packet->GetSequenceNumber());

				this->nackList.erase(it);

				return true;
			}
			// Out of order packet or already handled NACKed packet.
			else
			{
				MS_DEBUG_TAG(
				  rtx,
				  "ignoring out of order packet or already handled NACKed packet [ssrc:%" PRIu32
				  ", seq:%" PRIu16 "]",
				  packet->GetSsrc(),
				  packet->GetSequenceNumber());

				return false;
			}
		}

		// Otherwise we may have lost some packets.
		AddPacketsToNackList(this->lastSeq32 + 1, seq32);
		this->lastSeq32 = seq32;

		// Check if there are any nacks that are waiting for this seq number.
		std::vector<uint16_t> nackBatch = GetNackBatch(NackFilter::SEQ);

		if (!nackBatch.empty())
			this->listener->OnNackGeneratorNackRequired(nackBatch);

		MayRunTimer();

		return false;
	}

	void NackGenerator::AddPacketsToNackList(uint32_t seq32Start, uint32_t seq32End)
	{
		MS_TRACE();

		if (seq32End > MaxPacketAge)
		{
			uint32_t numItemsBefore = this->nackList.size();

			// Remove old packets.
			auto it = this->nackList.lower_bound(seq32End - MaxPacketAge);

			this->nackList.erase(this->nackList.begin(), it);

			uint32_t numItemsRemoved = numItemsBefore - this->nackList.size();

			if (numItemsRemoved > 0)
			{
				MS_DEBUG_TAG(
				  rtx,
				  "removed %" PRIu32 " NACK items due to too old seq number [seq32End:%" PRIu32 "]",
				  numItemsRemoved,
				  seq32End);
			}
		}

		// If the nack list is too large, clear it and request a key frame.
		uint32_t numNewNacks = seq32End - seq32Start;

		if (this->nackList.size() + numNewNacks > MaxNackPackets)
		{
			MS_DEBUG_TAG(
			  rtx,
			  "NACK list too large, clearing it and requesting a key frame [seq32End:%" PRIu32 "]",
			  seq32End);

			this->nackList.clear();
			this->listener->OnNackGeneratorKeyFrameRequired();

			return;
		}

		for (uint32_t seq32 = seq32Start; seq32 != seq32End; ++seq32)
		{
			// NOTE: Let the packet become out of order for a while without requesting
			// it into a NACK.
			// TODO: To be done.
			uint32_t sendAtSeq32 = seq32 + 0;
			NackInfo nackInfo(seq32, sendAtSeq32);

			MS_ASSERT(
			  this->nackList.find(seq32) == this->nackList.end(), "packet already in the NACK list");

			this->nackList[seq32] = nackInfo;
		}
	}

	// Delete all the entries in the NACK list whose key (seq32) is older than
	// the given one.
	void NackGenerator::RemoveFromNackListOlderThan(RTC::RtpPacket* packet)
	{
		MS_TRACE();

		uint32_t seq32          = packet->GetExtendedSequenceNumber();
		uint32_t numItemsBefore = this->nackList.size();

		auto it = this->nackList.lower_bound(seq32);

		this->nackList.erase(this->nackList.begin(), it);

		uint32_t numItemsRemoved = numItemsBefore - this->nackList.size();

		if (numItemsRemoved > 0)
		{
			MS_DEBUG_TAG(
			  rtx,
			  "removed %" PRIu32 " old NACK items older than received key frame [seq:%" PRIu16 "]",
			  numItemsRemoved,
			  packet->GetSequenceNumber());
		}
	}

	std::vector<uint16_t> NackGenerator::GetNackBatch(NackFilter filter)
	{
		MS_TRACE();

		uint64_t now = DepLibUV::GetTime();
		std::vector<uint16_t> nackBatch;
		auto it = this->nackList.begin();

		while (it != this->nackList.end())
		{
			NackInfo& nackInfo = it->second;
			uint16_t seq       = nackInfo.seq32 % (1 << 16);

			if (filter == NackFilter::SEQ && nackInfo.sentAtTime == 0 && this->lastSeq32 >= nackInfo.sendAtSeq32)
			{
				nackInfo.retries++;
				nackInfo.sentAtTime = now;

				if (nackInfo.retries >= MaxNackRetries)
				{
					MS_WARN_TAG(
					  rtx,
					  "sequence number removed from the NACK list due to max retries [seq:%" PRIu16 "]",
					  seq);

					it = this->nackList.erase(it);
				}
				else
				{
					nackBatch.emplace_back(seq);
					++it;
				}

				continue;
			}

			if (filter == NackFilter::TIME && nackInfo.sentAtTime + this->rtt < now)
			{
				nackInfo.retries++;
				nackInfo.sentAtTime = now;

				if (nackInfo.retries >= MaxNackRetries)
				{
					MS_WARN_TAG(
					  rtx,
					  "sequence number removed from the NACK list due to max retries [seq:%" PRIu16 "]",
					  seq);

					it = this->nackList.erase(it);
				}
				else
				{
					nackBatch.emplace_back(seq);
					++it;
				}

				continue;
			}

			++it;
		}

		return nackBatch;
	}

	inline void NackGenerator::MayRunTimer() const
	{
		if (!this->nackList.empty())
			this->timer->Start(TimerInterval);
	}

	inline void NackGenerator::OnTimer(Timer* /*timer*/)
	{
		MS_TRACE();

		std::vector<uint16_t> nackBatch = GetNackBatch(NackFilter::TIME);

		if (!nackBatch.empty())
			this->listener->OnNackGeneratorNackRequired(nackBatch);

		MayRunTimer();
	}
} // namespace RTC
