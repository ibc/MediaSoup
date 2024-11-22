#include "common.hpp"
#include "DepLibUV.hpp"
#include "RTC/TransportCongestionControlServer.hpp"
#include <catch2/catch_test_macros.hpp>

using namespace RTC;

struct TestTransportCongestionControlServerInput
{
	uint16_t wideSeqNumber;
	uint64_t nowMs;
};

struct TestTransportCongestionControlServerResult
{
	uint16_t wideSeqNumber;
	bool received;
	uint64_t timestamp;
};

using TestResults = std::deque<std::vector<TestTransportCongestionControlServerResult>>;

class TestTransportCongestionControlServerListener : public TransportCongestionControlServer::Listener
{
public:
	virtual void OnTransportCongestionControlServerSendRtcpPacket(
	  RTC::TransportCongestionControlServer* tccServer, RTC::RTCP::Packet* packet) override
	{
		auto* tccPacket = dynamic_cast<RTCP::FeedbackRtpTransportPacket*>(packet);

		if (!tccPacket)
		{
			return;
		}

		auto packetResults = tccPacket->GetPacketResults();

		REQUIRE(!this->results.empty());

		auto testResults = this->results.front();
		this->results.pop_front();

		REQUIRE(testResults.size() == packetResults.size());

		auto packetResultIt = packetResults.begin();
		auto testResultIt   = testResults.begin();

		for (; packetResultIt != packetResults.end() && testResultIt != testResults.end();
		     ++packetResultIt, ++testResultIt)
		{
			REQUIRE(packetResultIt->sequenceNumber == testResultIt->wideSeqNumber);
			REQUIRE(packetResultIt->received == testResultIt->received);

			if (packetResultIt->received)
			{
				REQUIRE(packetResultIt->receivedAtMs == testResultIt->timestamp);
			}
		}
	}

public:
	void SetResults(TestResults& results)
	{
		this->results = results;
	}

	void Check()
	{
		REQUIRE(this->results.empty());
	}

private:
	TestResults results;
};

// clang-format off
uint8_t buffer[] =
{
	0x90, 0x01, 0x00, 0x01,
	0x00, 0x00, 0x00, 0x04,
	0x00, 0x00, 0x00, 0x05,
	0xbe, 0xde, 0x00, 0x01,	// Header Extensions
	0x51, 0x60, 0xee, 0x00  // TCC Feedback
};
// clang-format on

void validate(std::vector<TestTransportCongestionControlServerInput>& inputs, TestResults& results)
{
	TestTransportCongestionControlServerListener listener;
	auto tccServer =
	  TransportCongestionControlServer(&listener, RTC::BweType::TRANSPORT_CC, RTC::MtuSize);

	tccServer.SetMaxIncomingBitrate(150000);
	tccServer.TransportConnected();

	std::unique_ptr<RtpPacket> packet{ RtpPacket::Parse(buffer, sizeof(buffer)) };

	packet->SetTransportWideCc01ExtensionId(5);
	packet->SetSequenceNumber(1);

	// Save results.
	listener.SetResults(results);

	uint64_t startTs = inputs[0].nowMs;
	uint64_t TransportCcFeedbackSendInterval{ 100u }; // In ms.

	for (auto input : inputs)
	{
		// Periodic sending TCC packets.
		uint64_t diffTs = input.nowMs - startTs;

		if (diffTs >= TransportCcFeedbackSendInterval)
		{
			tccServer.FillAndSendTransportCcFeedback();
			startTs = input.nowMs;
		}

		packet->UpdateTransportWideCc01(input.wideSeqNumber);
		tccServer.IncomingPacket(input.nowMs, packet.get());
	}

	tccServer.FillAndSendTransportCcFeedback();
	listener.Check();
};

SCENARIO("TransportCongestionControlServer", "[rtp]")
{
	SECTION("normal time and sequence")
	{
		// clang-format off
		std::vector<TestTransportCongestionControlServerInput> inputs
		{
			{ 1u, 1000u },
			{ 2u, 1050u },
			{ 3u, 1100u },
			{ 4u, 1150u },
			{ 5u, 1200u },
		};

		TestResults results
		{
			{
				{ 1u, true, 1000u },
				{ 2u, true, 1050u },
			},
			{
				{ 3u, true, 1100u },
				{ 4u, true, 1150u },
			},
			{
				{ 5u, true, 1200u },
			},
		};
		// clang-format on

		validate(inputs, results);
	}

	SECTION("lost packets")
	{
		// clang-format off
		std::vector<TestTransportCongestionControlServerInput> inputs
		{
			{  1u, 1000u },
			{  3u, 1050u },
			{  5u, 1100u },
			{  6u, 1150u },
		};

		TestResults results
		{
			{
				{ 1u,  true, 1000u },
				{ 2u, false,    0u },
				{ 3u,  true, 1050u },
			},
			{
				{ 4u, false,    0u },
				{ 5u,  true, 1100u },
				{ 6u,  true, 1150u },
			},
		};
		// clang-format on

		validate(inputs, results);
	}

	SECTION("duplicate packets")
	{
		// clang-format off
		std::vector<TestTransportCongestionControlServerInput> inputs
		{
			{  1u, 1000u },
			{  1u, 1050u },
			{  2u, 1100u },
			{  3u, 1150u },
			{  3u, 1200u },
			{  4u, 1250u },
		};

		TestResults results
		{
			{
				{ 1u,  true, 1000u },
			},
			{
				{ 2u,  true, 1100u },
				{ 3u,  true, 1150u },
			},
			{
				{ 4u,  true, 1250u },
			},
		};
		// clang-format on

		validate(inputs, results);
	}

	SECTION("packets arrive out of order")
	{
		// clang-format off
		std::vector<TestTransportCongestionControlServerInput> inputs
		{
			{ 1u, 1000u },
			{ 2u, 1050u },
			{ 4u, 1100u },
			{ 5u, 1150u },
			{ 3u, 1200u }, // Out of order
			{ 6u, 1250u },
		};

		TestResults results
		{
			{
				{ 1u, true, 1000u },
				{ 2u, true, 1050u },
			},
			{
				{ 3u, false,    0u },
				{ 4u,  true, 1100u },
				{ 5u,  true, 1150u },
			},
			{
				{ 3u, true, 1200u },
				{ 4u, true, 1100u },
				{ 5u, true, 1150u },
				{ 6u, true, 1250u },
			},
		};
		// clang-format on

		validate(inputs, results);
	}
}
