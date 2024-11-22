#include "common.hpp"
#include "Logger.hpp"
#include "RTC/RTCP/FeedbackRtpTransport.hpp"
#include <catch2/catch_test_macros.hpp>
#include <cstring> // std::memcmp()

using namespace RTC::RTCP;

struct TestFeedbackRtpTransportInput
{
	TestFeedbackRtpTransportInput(uint16_t sequenceNumber, uint64_t timestamp, size_t maxPacketSize)
	  : sequenceNumber(sequenceNumber), timestamp(timestamp), maxPacketSize(maxPacketSize)
	{
	}

	uint16_t sequenceNumber{ 0u };
	uint64_t timestamp{ 0u };
	size_t maxPacketSize{ 0u };
};

void validate(
  const std::vector<struct TestFeedbackRtpTransportInput>& inputs,
  std::vector<struct FeedbackRtpTransportPacket::PacketResult> packetResults)
{
	auto inputsIterator        = inputs.begin();
	auto packetResultsIterator = packetResults.begin();
	auto lastInput             = *inputsIterator;

	for (++inputsIterator; inputsIterator != inputs.end(); ++inputsIterator, ++packetResultsIterator)
	{
		auto& input             = *inputsIterator;
		auto& packetResult      = *packetResultsIterator;
		uint16_t missingPackets = input.sequenceNumber - lastInput.sequenceNumber - 1;

		if (missingPackets > 0)
		{
			// All missing packets must be represented in packetResults.
			for (uint16_t i{ 0u }; i < missingPackets; ++i)
			{
				packetResult = *packetResultsIterator;

				REQUIRE(packetResult.sequenceNumber == lastInput.sequenceNumber + i + 1);
				REQUIRE(packetResult.received == false);

				packetResultsIterator++;
			}
		}
		else
		{
			REQUIRE(packetResult.sequenceNumber == lastInput.sequenceNumber + 1);
			REQUIRE(packetResult.sequenceNumber == input.sequenceNumber);
			REQUIRE(packetResult.received == true);
			REQUIRE(
			  static_cast<int32_t>(packetResult.receivedAtMs & 0x1FFFFFC0) / 64 ==
			  static_cast<int32_t>(input.timestamp & 0x1FFFFFC0) / 64);
		}

		lastInput = input;
	}
}

SCENARIO("RTCP Feeback RTP transport", "[parser][rtcp][feedback-rtp][transport]")
{
	static constexpr size_t RtcpMtu{ 1200u };

	uint32_t senderSsrc{ 1111u };
	uint32_t mediaSsrc{ 2222u };

	SECTION(
	  "create FeedbackRtpTransportPacket, small delta run length chunk and single large delta status packet")
	{
		auto packet = std::make_unique<FeedbackRtpTransportPacket>(senderSsrc, mediaSsrc);

		REQUIRE(packet);

		/* clang-format off */
		std::vector<struct TestFeedbackRtpTransportInput> inputs =
		{
			{ 999, 1000000000, RtcpMtu },  // Pre base.
			{ 1000, 1000000000, RtcpMtu }, // Base.
			{ 1001, 1000000001, RtcpMtu },
			{ 1002, 1000000012, RtcpMtu },
			{ 1003, 1000000015, RtcpMtu },
			{ 1004, 1000000017, RtcpMtu },
			{ 1005, 1000000018, RtcpMtu },
			{ 1006, 1000000018, RtcpMtu },
			{ 1007, 1000000018, RtcpMtu },
			{ 1008, 1000000018, RtcpMtu },
			{ 1009, 1000000019, RtcpMtu },
			{ 1010, 1000000010, RtcpMtu },
			{ 1011, 1000000011, RtcpMtu },
			{ 1012, 1000000011, RtcpMtu },
			{ 1013, 1000000013, RtcpMtu }
		};
		/* clang-format on */

		packet->SetFeedbackPacketCount(1);

		for (auto& input : inputs)
		{
			if (std::addressof(input) == std::addressof(inputs.front()))
			{
				packet->SetBase(input.sequenceNumber + 1, input.timestamp);
			}
			else
			{
				packet->AddPacket(input.sequenceNumber, input.timestamp, input.maxPacketSize);
			}
		}

		REQUIRE(packet->GetLatestSequenceNumber() == 1013);
		REQUIRE(packet->GetLatestTimestamp() == 1000000013);

		// Add a packet with greater seq number but older timestamp.
		packet->AddPacket(1014, 1000000013 - 128, RtcpMtu);
		inputs.emplace_back(1014, 1000000013 - 128, RtcpMtu);

		REQUIRE(packet->GetLatestSequenceNumber() == 1014);
		REQUIRE(packet->GetLatestTimestamp() == 1000000013 - 128);

		packet->AddPacket(1015, 1000000015, RtcpMtu);
		inputs.emplace_back(1015, 1000000015, RtcpMtu);

		REQUIRE(packet->GetLatestSequenceNumber() == 1015);
		REQUIRE(packet->GetLatestTimestamp() == 1000000015);

		packet->Finish();
		validate(inputs, packet->GetPacketResults());

		REQUIRE(packet->GetBaseSequenceNumber() == 1000);
		REQUIRE(packet->GetPacketStatusCount() == 16);
		REQUIRE(packet->GetFeedbackPacketCount() == 1);
		REQUIRE(packet->GetPacketFractionLost() == 0);

		SECTION("serialize packet instance")
		{
			uint8_t buffer[1024];
			auto len = packet->Serialize(buffer);

			REQUIRE(packet->GetSize() == len);

			SECTION("parse serialized buffer")
			{
				std::unique_ptr<FeedbackRtpTransportPacket> packet2{ FeedbackRtpTransportPacket::Parse(
					buffer, len) };

				REQUIRE(packet2);
				REQUIRE(packet2->GetBaseSequenceNumber() == 1000);
				REQUIRE(packet2->GetPacketStatusCount() == 16);
				REQUIRE(packet2->GetFeedbackPacketCount() == 1);
				REQUIRE(packet2->GetPacketFractionLost() == 0);

				uint8_t buffer2[1024];
				auto len2 = packet2->Serialize(buffer2);

				REQUIRE(len == len2);
				REQUIRE(std::memcmp(buffer, buffer2, len) == 0);
				REQUIRE(packet2->GetSize() == len2);
			}
		}
	}

	SECTION("create FeedbackRtpTransportPacket, run length chunk (2)")
	{
		auto packet = std::make_unique<FeedbackRtpTransportPacket>(senderSsrc, mediaSsrc);

		/* clang-format off */
		std::vector<TestFeedbackRtpTransportInput> inputs =
		{
			{ 999, 1000000000, RtcpMtu }, // Pre base.
			{ 1000, 1000000000, RtcpMtu }, // Base.
			{ 1050, 1000000216, RtcpMtu }
		};
		/* clang-format on */

		packet->SetFeedbackPacketCount(10);

		for (auto& input : inputs)
		{
			if (std::addressof(input) == std::addressof(inputs.front()))
			{
				packet->SetBase(input.sequenceNumber + 1, input.timestamp);
			}
			else
			{
				packet->AddPacket(input.sequenceNumber, input.timestamp, input.maxPacketSize);
			}
		}

		packet->Finish();
		validate(inputs, packet->GetPacketResults());

		REQUIRE(packet->GetBaseSequenceNumber() == 1000);
		REQUIRE(packet->GetPacketStatusCount() == 51);
		REQUIRE(packet->GetFeedbackPacketCount() == 10);
		REQUIRE(packet->GetPacketFractionLost() > 0);
		REQUIRE(packet->GetLatestSequenceNumber() == 1050);
		REQUIRE(packet->GetLatestTimestamp() == 1000000216);

		SECTION("serialize packet instance")
		{
			uint8_t buffer[1024];
			auto len = packet->Serialize(buffer);

			REQUIRE(packet->GetSize() == len);

			SECTION("parse serialized buffer")
			{
				std::unique_ptr<FeedbackRtpTransportPacket> packet2{ FeedbackRtpTransportPacket::Parse(
					buffer, len) };

				REQUIRE(packet2);
				REQUIRE(packet2->GetBaseSequenceNumber() == 1000);
				REQUIRE(packet2->GetPacketStatusCount() == 51);
				REQUIRE(packet2->GetFeedbackPacketCount() == 10);
				REQUIRE(packet2->GetPacketFractionLost() > 0);

				uint8_t buffer2[1024];
				auto len2 = packet2->Serialize(buffer2);

				REQUIRE(len == len2);
				REQUIRE(std::memcmp(buffer, buffer2, len) == 0);
				REQUIRE(packet2->GetSize() == len2);
			}
		}
	}

	SECTION("create FeedbackRtpTransportPacket, mixed chunks")
	{
		/* clang-format off */
		std::vector<TestFeedbackRtpTransportInput> inputs =
		{
			{ 999, 1000000000, RtcpMtu },  // Pre base.
			{ 1000, 1000000000, RtcpMtu }, // Base.
			{ 1001, 1000000100, RtcpMtu },
			{ 1002, 1000000200, RtcpMtu },
			{ 1015, 1000000300, RtcpMtu },
			{ 1016, 1000000400, RtcpMtu },
			{ 1017, 1000000500, RtcpMtu }
		};
		/* clang-format on */

		auto packet = std::make_unique<FeedbackRtpTransportPacket>(senderSsrc, mediaSsrc);

		packet->SetFeedbackPacketCount(1);

		for (auto& input : inputs)
		{
			if (std::addressof(input) == std::addressof(inputs.front()))
			{
				packet->SetBase(input.sequenceNumber + 1, input.timestamp);
			}
			else
			{
				packet->AddPacket(input.sequenceNumber, input.timestamp, input.maxPacketSize);
			}
		}

		packet->Finish();
		validate(inputs, packet->GetPacketResults());

		REQUIRE(packet->GetBaseSequenceNumber() == 1000);
		REQUIRE(packet->GetPacketStatusCount() == 18);
		REQUIRE(packet->GetFeedbackPacketCount() == 1);
		REQUIRE(packet->GetPacketFractionLost() > 0);
		REQUIRE(packet->GetLatestSequenceNumber() == 1017);
		REQUIRE(packet->GetLatestTimestamp() == 1000000500);

		SECTION("serialize packet instance")
		{
			uint8_t buffer[1024];
			auto len = packet->Serialize(buffer);

			REQUIRE(packet->GetSize() == len);

			SECTION("parse serialized buffer")
			{
				std::unique_ptr<FeedbackRtpTransportPacket> packet2{ FeedbackRtpTransportPacket::Parse(
					buffer, len) };

				REQUIRE(packet2);
				REQUIRE(packet2->GetBaseSequenceNumber() == 1000);
				REQUIRE(packet2->GetPacketStatusCount() == 18);
				REQUIRE(packet2->GetFeedbackPacketCount() == 1);
				REQUIRE(packet2->GetPacketFractionLost() > 0);

				uint8_t buffer2[1024];
				auto len2 = packet2->Serialize(buffer2);

				REQUIRE(len == len2);
				REQUIRE(std::memcmp(buffer, buffer2, len) == 0);
				REQUIRE(packet2->GetSize() == len2);
			}
		}
	}

	SECTION("create FeedbackRtpTransportPacket, incomplete two bit vector chunk")
	{
		std::vector<TestFeedbackRtpTransportInput> inputs = {
			{ 999, 1000000000, RtcpMtu },  // Pre base.
			{ 1000, 1000000100, RtcpMtu }, // Base.
			{ 1001, 1000000700, RtcpMtu },
		};

		auto packet = std::make_unique<FeedbackRtpTransportPacket>(senderSsrc, mediaSsrc);

		packet->SetFeedbackPacketCount(1);

		for (auto& input : inputs)
		{
			if (std::addressof(input) == std::addressof(inputs.front()))
			{
				packet->SetBase(input.sequenceNumber + 1, input.timestamp);
			}
			else
			{
				packet->AddPacket(input.sequenceNumber, input.timestamp, input.maxPacketSize);
			}
		}

		packet->Finish();
		validate(inputs, packet->GetPacketResults());

		REQUIRE(packet->GetBaseSequenceNumber() == 1000);
		REQUIRE(packet->GetPacketStatusCount() == 2);
		REQUIRE(packet->GetFeedbackPacketCount() == 1);
		REQUIRE(packet->GetPacketFractionLost() == 0);
		REQUIRE(packet->GetLatestSequenceNumber() == 1001);
		REQUIRE(packet->GetLatestTimestamp() == 1000000700);

		SECTION("serialize packet instance")
		{
			uint8_t buffer[1024];
			auto len = packet->Serialize(buffer);

			REQUIRE(packet->GetSize() == len);

			SECTION("parse serialized buffer")
			{
				std::unique_ptr<FeedbackRtpTransportPacket> packet2{ FeedbackRtpTransportPacket::Parse(
					buffer, len) };

				REQUIRE(packet2);
				REQUIRE(packet2->GetBaseSequenceNumber() == 1000);
				REQUIRE(packet2->GetPacketStatusCount() == 2);
				REQUIRE(packet2->GetFeedbackPacketCount() == 1);
				REQUIRE(packet2->GetPacketFractionLost() == 0);

				uint8_t buffer2[1024];
				auto len2 = packet2->Serialize(buffer2);

				REQUIRE(len == len2);
				REQUIRE(std::memcmp(buffer, buffer2, len) == 0);
				REQUIRE(packet2->GetSize() == len2);
			}
		}
	}

	SECTION("create two sequential FeedbackRtpTransportPackets")
	{
		/* clang-format off */
		std::vector<TestFeedbackRtpTransportInput> inputs =
		{
			{ 999, 1000000000, RtcpMtu },  // Pre base.
			{ 1000, 1000000000, RtcpMtu }, // Base.
			{ 1001, 1000000003, RtcpMtu },
			{ 1002, 1000000003, RtcpMtu },
			{ 1003, 1000000003, RtcpMtu },
			{ 1004, 1000000004, RtcpMtu },
			{ 1005, 1000000005, RtcpMtu },
			{ 1006, 1000000005, RtcpMtu },
			{ 1007, 1000000007, RtcpMtu }
		};
		/* clang-format on */

		auto packet = std::make_unique<FeedbackRtpTransportPacket>(senderSsrc, mediaSsrc);

		packet->SetFeedbackPacketCount(1);

		for (auto& input : inputs)
		{
			if (std::addressof(input) == std::addressof(inputs.front()))
			{
				packet->SetBase(input.sequenceNumber + 1, input.timestamp);
			}
			else
			{
				packet->AddPacket(input.sequenceNumber, input.timestamp, input.maxPacketSize);
			}
		}

		packet->Finish();
		validate(inputs, packet->GetPacketResults());

		REQUIRE(packet->GetBaseSequenceNumber() == 1000);
		REQUIRE(packet->GetPacketStatusCount() == 8);
		REQUIRE(packet->GetFeedbackPacketCount() == 1);
		REQUIRE(packet->GetPacketFractionLost() == 0);
		REQUIRE(packet->GetLatestSequenceNumber() == 1007);
		REQUIRE(packet->GetLatestTimestamp() == 1000000007);

		uint8_t buffer[1024];
		auto len = packet->Serialize(buffer);

		REQUIRE(packet->GetSize() == len);

		SECTION("parse serialized buffer")
		{
			std::unique_ptr<FeedbackRtpTransportPacket> packet2{ FeedbackRtpTransportPacket::Parse(
				buffer, len) };

			REQUIRE(packet2);
			REQUIRE(packet2->GetBaseSequenceNumber() == 1000);
			REQUIRE(packet2->GetPacketStatusCount() == 8);
			REQUIRE(packet2->GetFeedbackPacketCount() == 1);
			REQUIRE(packet2->GetPacketFractionLost() == 0);

			uint8_t buffer2[1024];
			auto len2 = packet2->Serialize(buffer2);

			REQUIRE(len == len2);
			REQUIRE(std::memcmp(buffer, buffer2, len) == 0);
			REQUIRE(packet2->GetSize() == len2);
		}

		auto latestWideSeqNumber = packet->GetLatestSequenceNumber();
		auto latestTimestamp     = packet->GetLatestTimestamp();

		/* clang-format off */
		std::vector<TestFeedbackRtpTransportInput> inputs2 =
		{
			{ latestWideSeqNumber, latestTimestamp, RtcpMtu },
			{ 1008, 1000000008, RtcpMtu },
			{ 1009, 1000000009, RtcpMtu },
			{ 1010, 1000000010, RtcpMtu },
			{ 1011, 1000000010, RtcpMtu },
			{ 1012, 1000000010, RtcpMtu },
			{ 1013, 1000000014, RtcpMtu },
			{ 1014, 1000000014, RtcpMtu }
		};
		/* clang-format on */

		auto packet2 = std::make_unique<FeedbackRtpTransportPacket>(senderSsrc, mediaSsrc);

		packet2->SetFeedbackPacketCount(2);

		for (auto& input : inputs2)
		{
			if (std::addressof(input) == std::addressof(inputs2.front()))
			{
				packet2->SetBase(input.sequenceNumber + 1, input.timestamp);
			}
			else
			{
				packet2->AddPacket(input.sequenceNumber, input.timestamp, input.maxPacketSize);
			}
		}

		packet2->Finish();
		validate(inputs2, packet2->GetPacketResults());

		REQUIRE(packet2->GetBaseSequenceNumber() == 1008);
		REQUIRE(packet2->GetPacketStatusCount() == 7);
		REQUIRE(packet2->GetFeedbackPacketCount() == 2);
		REQUIRE(packet2->GetPacketFractionLost() == 0);
		REQUIRE(packet2->GetLatestSequenceNumber() == 1014);
		REQUIRE(packet2->GetLatestTimestamp() == 1000000014);

		len = packet2->Serialize(buffer);

		REQUIRE(packet2->GetSize() == len);

		SECTION("parse serialized buffer")
		{
			std::unique_ptr<FeedbackRtpTransportPacket> packet3{ FeedbackRtpTransportPacket::Parse(
				buffer, len) };

			REQUIRE(packet3);
			REQUIRE(packet3->GetBaseSequenceNumber() == 1008);
			REQUIRE(packet3->GetPacketStatusCount() == 7);
			REQUIRE(packet3->GetFeedbackPacketCount() == 2);
			REQUIRE(packet3->GetPacketFractionLost() == 0);

			uint8_t buffer2[1024];
			auto len2 = packet3->Serialize(buffer2);

			REQUIRE(len == len2);
			REQUIRE(std::memcmp(buffer, buffer2, len) == 0);
			REQUIRE(packet3->GetSize() == len2);
		}
	}

	SECTION("parse FeedbackRtpTransportPacket, one bit vector chunk")
	{
		// clang-format off
		uint8_t data[] =
		{
			0x8F, 0xCD, 0x00, 0x07,
			0xFA, 0x17, 0xFA, 0x17,
			0x09, 0xFA, 0xFF, 0x67,
			0x00, 0x27, 0x00, 0x0D,
			0x5F, 0xC2, 0xF1, 0x03,
			0xBF, 0x8E, 0x10, 0x00,
			0x00, 0x00, 0x00, 0x00,
			0x00, 0x1C, 0x04, 0x00
		};
		// clang-format on

		std::unique_ptr<FeedbackRtpTransportPacket> packet{ FeedbackRtpTransportPacket::Parse(
			data, sizeof(data)) };

		REQUIRE(packet);
		REQUIRE(packet->GetSize() == sizeof(data));
		REQUIRE(packet->GetBaseSequenceNumber() == 39);
		REQUIRE(packet->GetPacketStatusCount() == 13);
		REQUIRE(packet->GetReferenceTime() == 6275825); // 0x5FC2F1 (signed 24 bits)
		REQUIRE(
		  packet->GetReferenceTimestamp() ==
		  FeedbackRtpTransportPacket::TimeWrapPeriod +
		    static_cast<int64_t>(6275825) * FeedbackRtpTransportPacket::BaseTimeTick);
		REQUIRE(packet->GetFeedbackPacketCount() == 3);

		SECTION("serialize packet")
		{
			uint8_t buffer[1024];
			auto len = packet->Serialize(buffer);

			REQUIRE(len == sizeof(data));
			REQUIRE(std::memcmp(data, buffer, len) == 0);
		}
	}

	SECTION("parse FeedbackRtpTransportPacket with negative reference time")
	{
		// clang-format off
		uint8_t data[] =
		{
			0x8F, 0xCD, 0x00, 0x04,
			0xFA, 0x17, 0xFA, 0x17,
			0x09, 0xFA, 0xFF, 0x67,
			0x00, 0x27, 0x00, 0x00,
			0xFF, 0xFF, 0xFE, 0x01
		};
		// clang-format on

		std::unique_ptr<FeedbackRtpTransportPacket> packet{ FeedbackRtpTransportPacket::Parse(
			data, sizeof(data)) };

		REQUIRE(packet);
		REQUIRE(packet->GetSize() == sizeof(data));
		REQUIRE(packet->GetBaseSequenceNumber() == 39);
		REQUIRE(packet->GetPacketStatusCount() == 0);
		REQUIRE(packet->GetReferenceTime() == -2); // 0xFFFFFE = -2 (signed 24 bits)
		REQUIRE(
		  packet->GetReferenceTimestamp() ==
		  FeedbackRtpTransportPacket::TimeWrapPeriod +
		    static_cast<int64_t>(-2) * FeedbackRtpTransportPacket::BaseTimeTick);
		REQUIRE(packet->GetFeedbackPacketCount() == 1);

		SECTION("serialize packet")
		{
			uint8_t buffer[1024];
			auto len = packet->Serialize(buffer);

			REQUIRE(len == sizeof(data));
			REQUIRE(std::memcmp(data, buffer, len) == 0);
		}
	}

	SECTION("parse FeedbackRtpTransportPacket generated by Chrome")
	{
		// clang-format off
		uint8_t data[] =
		{
			0x8F, 0xCD, 0x00, 0x05,
			0xFA, 0x17, 0xFA, 0x17,
			0x39, 0xE9, 0x42, 0x38,
			0x00, 0x01, 0x00, 0x02,
			0xBD, 0x57, 0xAA, 0x00,
			0x20, 0x02, 0x8C, 0x44
		};
		// clang-format on

		std::unique_ptr<FeedbackRtpTransportPacket> packet{ FeedbackRtpTransportPacket::Parse(
			data, sizeof(data)) };

		REQUIRE(packet);
		REQUIRE(packet->GetSize() == sizeof(data));
		REQUIRE(packet->GetBaseSequenceNumber() == 1);
		REQUIRE(packet->GetPacketStatusCount() == 2);
		REQUIRE(packet->GetReferenceTime() == -4368470);
		REQUIRE(
		  packet->GetReferenceTimestamp() ==
		  FeedbackRtpTransportPacket::TimeWrapPeriod +
		    static_cast<int64_t>(-4368470) * FeedbackRtpTransportPacket::BaseTimeTick);

		REQUIRE(packet->GetFeedbackPacketCount() == 0);

		SECTION("serialize packet")
		{
			uint8_t buffer[1024];
			auto len = packet->Serialize(buffer);

			REQUIRE(len == sizeof(data));
			REQUIRE(std::memcmp(data, buffer, len) == 0);
		}
	}

	SECTION("parse FeedbackRtpTransportPacket generated by Chrome with libwebrtc as a reference")
	{
		using FeedbackPacketsMeta = struct
		{
			int32_t baseTimeRaw;
			int64_t baseTimeMs;
			uint16_t baseSequence;
			size_t packetStatusCount;
			std::vector<int16_t> deltas;
			std::vector<uint8_t> buffer;
		};

		// Metadata collected by parsing buffers with libwebrtc, buffers itself.
		// were generated by chrome in direction of mediasoup.
		std::vector<FeedbackPacketsMeta> feedbackPacketsMeta = {
			{ .baseTimeRaw       = 35504,
			  .baseTimeMs        = 1076014080,
			  .baseSequence      = 13,
			  .packetStatusCount = 1,
			  .deltas            = std::vector<int16_t>{ 57 },
			  .buffer            = std::vector<uint8_t>{ 0xaf, 0xcd, 0x00, 0x05, 0xfa, 0x17, 0xfa, 0x17,
			                                             0x00, 0x00, 0x04, 0xd2, 0x00, 0x0d, 0x00, 0x01,
			                                             0x00, 0x8A, 0xB0, 0x00, 0x20, 0x01, 0xE4, 0x01 } },
			{ .baseTimeRaw       = 35504,
			  .baseTimeMs        = 1076014080,
			  .baseSequence      = 14,
			  .packetStatusCount = 4,
			  .deltas            = std::vector<int16_t>{ 58, 2, 3, 55 },
			  .buffer = std::vector<uint8_t>{ 0xaf, 0xcd, 0x00, 0x06, 0xFA, 0x17, 0xFA, 0x17, 0x1C, 0xB7,
			                                  0xDA, 0xF3, 0x00, 0x0E, 0x00, 0x04, 0x00, 0x8A, 0xB0, 0x01,
			                                  0x20, 0x04, 0xE8, 0x08, 0x0C, 0xDC, 0x00, 0x02 } },
			{ .baseTimeRaw       = 35505,
			  .baseTimeMs        = 1076014144,
			  .baseSequence      = 18,
			  .packetStatusCount = 5,
			  .deltas            = std::vector<int16_t>{ 60, 6, 5, 9, 22 },
			  .buffer = std::vector<uint8_t>{ 0xAF, 0xCD, 0x00, 0x06, 0xFA, 0x17, 0xFA, 0x17, 0x1C, 0xB7,
			                                  0xDA, 0xF3, 0x00, 0x12, 0x00, 0x05, 0x00, 0x8A, 0xB1, 0x02,
			                                  0x20, 0x05, 0xF0, 0x18, 0x14, 0x24, 0x58, 0x01 } },

			{ .baseTimeRaw       = 617873,
			  .baseTimeMs        = 1113285696,
			  .baseSequence      = 2924,
			  .packetStatusCount = 22,
			  .deltas =
			    std::vector<int16_t>{ 3, 5, 5, 0, 10, 0, 0, 4, 0, 1, 0, 2, 0, 2, 0, 2, 0, 2, 0, 1, 0, 4 },
			  .buffer = std::vector<uint8_t>{ 0x8F, 0xCD, 0x00, 0x0A, 0xFA, 0x17, 0xFA, 0x17, 0x06,
			                                  0xF5, 0x11, 0x4C, 0x0B, 0x6C, 0x00, 0x16, 0x09, 0x6D,
			                                  0x91, 0xEE, 0x20, 0x16, 0x0C, 0x14, 0x14, 0x00, 0x28,
			                                  0x00, 0x00, 0x10, 0x00, 0x04, 0x00, 0x08, 0x00, 0x08,
			                                  0x00, 0x08, 0x00, 0x08, 0x00, 0x04, 0x00, 0x10 } },

			{ .baseTimeRaw       = -4368470,
			  .baseTimeMs        = 794159744,
			  .baseSequence      = 1,
			  .packetStatusCount = 2,
			  .deltas            = std::vector<int16_t>{ 35, 17 },
			  .buffer            = std::vector<uint8_t>{ 0x8F, 0xCD, 0x00, 0x05, 0xFA, 0x17, 0xFA, 0x17,
			                                             0x39, 0xE9, 0x42, 0x38, 0x00, 0x01, 0x00, 0x02,
			                                             0xBD, 0x57, 0xAA, 0x00, 0x20, 0x02, 0x8C, 0x44 } },

			{ .baseTimeRaw       = 818995,
			  .baseTimeMs        = 1126157504,
			  .baseSequence      = 930,
			  .packetStatusCount = 5,
			  .deltas            = std::vector<int16_t>{ 62, 18, 5, 6, 19 },
			  .buffer = std::vector<uint8_t>{ 0xAF, 0xCD, 0x00, 0x06, 0xFA, 0x17, 0xFA, 0x17, 0x26, 0x9E,
			                                  0x8E, 0x50, 0x03, 0xA2, 0x00, 0x05, 0x0C, 0x7F, 0x33, 0x9F,
			                                  0x20, 0x05, 0xF8, 0x48, 0x14, 0x18, 0x4C, 0x01 } },
			{ .baseTimeRaw       = 818996,
			  .baseTimeMs        = 1126157568,
			  .baseSequence      = 921,
			  .packetStatusCount = 7,
			  .deltas            = std::vector<int16_t>{ 14, 5, 6, 6, 7, 14, 5 },
			  .buffer =
			    std::vector<uint8_t>{ 0xAF, 0xCD, 0x00, 0x07, 0xFA, 0x17, 0xFA, 0x17, 0x33, 0xB0, 0x4A,
			                          0xE8, 0x03, 0x99, 0x00, 0x07, 0x0C, 0x7F, 0x34, 0x9F, 0x20, 0x07,
			                          0x38, 0x14, 0x18, 0x18, 0x1C, 0x38, 0x14, 0x00, 0x00, 0x03 } },
			{ .baseTimeRaw       = 818996,
			  .baseTimeMs        = 1126157568,
			  .baseSequence      = 935,
			  .packetStatusCount = 7,
			  .deltas            = std::vector<int16_t>{ 57, 0, 6, 5, 5, 24, 0 },
			  .buffer =
			    std::vector<uint8_t>{ 0xAF, 0xCD, 0x00, 0x07, 0xFA, 0x17, 0xFA, 0x17, 0x26, 0x9E, 0x8E,
			                          0x50, 0x03, 0xA7, 0x00, 0x07, 0x0C, 0x7F, 0x34, 0xA0, 0x20, 0x07,
			                          0xE4, 0x00, 0x18, 0x14, 0x14, 0x60, 0x00, 0x00, 0x00, 0x03 } },
			{ .baseTimeRaw       = 818996,
			  .baseTimeMs        = 1126157568,
			  .baseSequence      = 928,
			  .packetStatusCount = 5,
			  .deltas            = std::vector<int16_t>{ 63, 11, 21, 6, 0 },
			  .buffer = std::vector<uint8_t>{ 0xAF, 0xCD, 0x00, 0x06, 0xFA, 0x17, 0xFA, 0x17, 0x33, 0xB0,
			                                  0x4A, 0xE8, 0x03, 0xA0, 0x00, 0x05, 0x0C, 0x7F, 0x34, 0xA0,
			                                  0x20, 0x05, 0xFC, 0x2C, 0x54, 0x18, 0x00, 0x01 } },
			{ .baseTimeRaw       = 818997,
			  .baseTimeMs        = 1126157632,
			  .baseSequence      = 942,
			  .packetStatusCount = 6,
			  .deltas            = std::vector<int16_t>{ 39, 13, 9, 5, 4, 13 },
			  .buffer = std::vector<uint8_t>{ 0x8F, 0xCD, 0x00, 0x06, 0xFA, 0x17, 0xFA, 0x17, 0x26, 0x9E,
			                                  0x8E, 0x50, 0x03, 0xAE, 0x00, 0x06, 0x0C, 0x7F, 0x35, 0xA1,
			                                  0x20, 0x06, 0x9C, 0x34, 0x24, 0x14, 0x10, 0x34 } },
			{ .baseTimeRaw       = 821523,
			  .baseTimeMs        = 1126319296,
			  .baseSequence      = 10,
			  .packetStatusCount = 7,
			  .deltas            = std::vector<int16_t>{ 25, 2, 2, 3, 1, 1, 3 },
			  .buffer =
			    std::vector<uint8_t>{ 0xAF, 0xCD, 0x00, 0x07, 0xFA, 0x17, 0xFA, 0x17, 0x00, 0x00, 0x04,
			                          0xD2, 0x00, 0x0A, 0x00, 0x07, 0x0C, 0x89, 0x13, 0x00, 0x20, 0x07,
			                          0x64, 0x08, 0x08, 0x0C, 0x04, 0x04, 0x0C, 0x00, 0x00, 0x03 } },
			{ .baseTimeRaw       = 821524,
			  .baseTimeMs        = 1126319360,
			  .baseSequence      = 17,
			  .packetStatusCount = 2,
			  .deltas            = std::vector<int16_t>{ 44, 18 },
			  .buffer            = std::vector<uint8_t>{ 0x8F, 0xCD, 0x00, 0x05, 0xFA, 0x17, 0xFA, 0x17,
			                                             0x08, 0xEB, 0x06, 0xD7, 0x00, 0x11, 0x00, 0x02,
			                                             0x0C, 0x89, 0x14, 0x01, 0x20, 0x02, 0xB0, 0x48 } },
			{ .baseTimeRaw       = 821524,
			  .baseTimeMs        = 1126319360,
			  .baseSequence      = 17,
			  .packetStatusCount = 1,
			  .deltas            = std::vector<int16_t>{ 62 },
			  .buffer            = std::vector<uint8_t>{ 0xAF, 0xCD, 0x00, 0x05, 0xFA, 0x17, 0xFA, 0x17,
			                                             0x20, 0x92, 0x5E, 0xB7, 0x00, 0x11, 0x00, 0x01,
			                                             0x0C, 0x89, 0x14, 0x00, 0x20, 0x01, 0xF8, 0x01 } },
			{ .baseTimeRaw       = 821526,
			  .baseTimeMs        = 1126319488,
			  .baseSequence      = 19,
			  .packetStatusCount = 4,
			  .deltas            = std::vector<int16_t>{ 4, 0, 4, 0 },
			  .buffer = std::vector<uint8_t>{ 0xAF, 0xCD, 0x00, 0x06, 0xFA, 0x17, 0xFA, 0x17, 0x08, 0xEB,
			                                  0x06, 0xD7, 0x00, 0x13, 0x00, 0x04, 0x0C, 0x89, 0x16, 0x02,
			                                  0x20, 0x04, 0x10, 0x00, 0x10, 0x00, 0x00, 0x02 } }
		};

		for (const auto& packetMeta : feedbackPacketsMeta)
		{
			auto buffer = packetMeta.buffer;

			std::unique_ptr<FeedbackRtpTransportPacket> feedback{ FeedbackRtpTransportPacket::Parse(
				buffer.data(), buffer.size()) };

			REQUIRE(feedback->GetReferenceTime() == packetMeta.baseTimeRaw);
			REQUIRE(feedback->GetReferenceTimestamp() == packetMeta.baseTimeMs);
			REQUIRE(feedback->GetBaseSequenceNumber() == packetMeta.baseSequence);
			REQUIRE(feedback->GetPacketStatusCount() == packetMeta.packetStatusCount);

			auto packetsResults = feedback->GetPacketResults();

			int deltasIt = 0;
			for (const auto& delta : packetMeta.deltas)
			{
				auto resultDelta = packetsResults[deltasIt].delta;
				REQUIRE(static_cast<int16_t>(resultDelta / 4) == delta);
				deltasIt++;
			}
		}
	}

	SECTION("Check GetBaseDelta Wraparound")
	{
		auto MaxBaseTime =
		  FeedbackRtpTransportPacket::TimeWrapPeriod - FeedbackRtpTransportPacket::BaseTimeTick;
		auto packet1 = std::make_unique<FeedbackRtpTransportPacket>(senderSsrc, mediaSsrc);
		auto packet2 = std::make_unique<FeedbackRtpTransportPacket>(senderSsrc, mediaSsrc);
		auto packet3 = std::make_unique<FeedbackRtpTransportPacket>(senderSsrc, mediaSsrc);

		packet1->SetReferenceTime(MaxBaseTime);
		packet2->SetReferenceTime(MaxBaseTime + FeedbackRtpTransportPacket::BaseTimeTick);
		packet3->SetReferenceTime(
		  MaxBaseTime + FeedbackRtpTransportPacket::BaseTimeTick +
		  FeedbackRtpTransportPacket::BaseTimeTick);

		REQUIRE(packet1->GetReferenceTime() == 16777215);
		REQUIRE(packet2->GetReferenceTime() == 0);
		REQUIRE(packet3->GetReferenceTime() == 1);

		REQUIRE(packet1->GetReferenceTimestamp() == 2147483584);
		REQUIRE(packet2->GetReferenceTimestamp() == 1073741824);
		REQUIRE(packet3->GetReferenceTimestamp() == 1073741888);

		REQUIRE(packet1->GetBaseDelta(packet1->GetReferenceTimestamp()) == 0);
		REQUIRE(packet2->GetBaseDelta(packet1->GetReferenceTimestamp()) == 64);
		REQUIRE(packet3->GetBaseDelta(packet2->GetReferenceTimestamp()) == 64);
		REQUIRE(packet3->GetBaseDelta(packet1->GetReferenceTimestamp()) == 128);
	}
}
