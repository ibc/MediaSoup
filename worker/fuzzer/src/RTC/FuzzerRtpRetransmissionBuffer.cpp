#include "RTC/FuzzerRtpRetransmissionBuffer.hpp"
#include "Utils.hpp"
#include "RTC/RtpPacket.hpp"
#include "RTC/RtpRetransmissionBuffer.hpp"

void Fuzzer::RTC::RtpRetransmissionBuffer::Fuzz(const uint8_t* data, size_t len)
{
	uint16_t maxItems{ 2500u };
	uint32_t maxRetransmissionDelayMs{ 2000u };
	uint32_t clockRate{ 90000 };

	// Trick to initialize our stuff just once (use static).
	static ::RTC::RtpRetransmissionBuffer retransmissionBuffer(
	  maxItems, maxRetransmissionDelayMs, clockRate);

	// clang-format off
	uint8_t buffer[] =
	{
		0b10000000, 0b01111011, 0b01010010, 0b00001110,
		0b01011011, 0b01101011, 0b11001010, 0b10110101,
		0, 0, 0, 2
	};
	// clang-format on

	// Create base RtpPacket instance.
	auto* packet = ::RTC::RtpPacket::Parse(buffer, 12);
	size_t offset{ 0u };

	while (len >= 4u)
	{
		std::shared_ptr<::RTC::RtpPacket> sharedPacket;

		// Set 'random' sequence number and timestamp.
		packet->SetSequenceNumber(Utils::Byte::Get2Bytes(data, offset));
		packet->SetTimestamp(Utils::Byte::Get4Bytes(data, offset));

		retransmissionBuffer.Insert(packet, sharedPacket);

		len -= 4u;
		offset += 4;
	}

	delete packet;
}
