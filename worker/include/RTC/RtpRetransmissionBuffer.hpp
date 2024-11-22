#ifndef MS_RTC_RTP_RETRANSMISSION_BUFFER_HPP
#define MS_RTC_RTP_RETRANSMISSION_BUFFER_HPP

#include "common.hpp"
#include "RTC/RtpPacket.hpp"
#include <deque>

namespace RTC
{
	// Special container that stores `Item`* elements addressable by their `uint16_t`
	// sequence number, while only taking as little memory as necessary to store
	// the range covering a maximum of `MaxRetransmissionDelayForVideoMs` or
	//  `MaxRetransmissionDelayForAudioMs` ms.
	class RtpRetransmissionBuffer
	{
	public:
		struct Item
		{
			void Reset();

			// Original packet.
			std::shared_ptr<RTC::RtpPacket> packet{ nullptr };
			// Correct SSRC since original packet may not have the same.
			uint32_t ssrc{ 0u };
			// Correct sequence number since original packet may not have the same.
			uint16_t sequenceNumber{ 0u };
			// Correct timestamp since original packet may not have the same.
			uint32_t timestamp{ 0u };
			// Last time this packet was resent.
			uint64_t resentAtMs{ 0u };
			// Number of times this packet was resent.
			uint8_t sentTimes{ 0u };
		};

	private:
		static Item* FillItem(
		  Item* item, RTC::RtpPacket* packet, std::shared_ptr<RTC::RtpPacket>& sharedPacket);

	public:
		RtpRetransmissionBuffer(uint16_t maxItems, uint32_t maxRetransmissionDelayMs, uint32_t clockRate);
		~RtpRetransmissionBuffer();

		Item* Get(uint16_t seq) const;
		void Insert(RTC::RtpPacket* packet, std::shared_ptr<RTC::RtpPacket>& sharedPacket);
		void Clear();
		void Dump() const;

	private:
		Item* GetOldest() const;
		Item* GetNewest() const;
		void RemoveOldest();
		void RemoveOldest(uint16_t numItems);
		bool ClearTooOldByTimestamp(uint32_t newestTimestamp);
		bool IsTooOldTimestamp(uint32_t timestamp, uint32_t newestTimestamp) const;

	protected:
		// Make buffer protected for testing purposes.
		std::deque<Item*> buffer;

	private:
		// Given as argument.
		uint16_t maxItems;
		uint32_t maxRetransmissionDelayMs;
		uint32_t clockRate;
	};
} // namespace RTC

#endif
