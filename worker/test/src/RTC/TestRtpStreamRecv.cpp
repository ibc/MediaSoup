#include "common.hpp"
#include "DepLibUV.hpp"
#include "RTC/RtpPacket.hpp"
#include "RTC/RtpStream.hpp"
#include "RTC/RtpStreamRecv.hpp"
#include <catch2/catch_test_macros.hpp>
#include <vector>

using namespace RTC;

// 17: 16 bit mask + the initial sequence number.
static constexpr size_t MaxRequestedPackets{ 17 };
static constexpr unsigned int SendNackDelay{ 0u }; // In ms.
static const bool UseRtpInactivityCheck{ false };

SCENARIO("receive RTP packets and trigger NACK", "[rtp][rtpstream]")
{
	class RtpStreamRecvListener : public RtpStreamRecv::Listener
	{
	public:
		void OnRtpStreamScore(RtpStream* /*rtpStream*/, uint8_t /*score*/, uint8_t /*previousScore*/) override
		{
		}

		void OnRtpStreamSendRtcpPacket(RtpStreamRecv* rtpStream, RTCP::Packet* packet) override
		{
			switch (packet->GetType())
			{
				case RTCP::Type::PSFB:
				{
					switch (dynamic_cast<RTCP::FeedbackPsPacket*>(packet)->GetMessageType())
					{
						case RTCP::FeedbackPs::MessageType::PLI:
						{
							INFO("PLI required");

							REQUIRE(this->shouldTriggerPLI == true);

							this->shouldTriggerPLI = false;
							this->nackedSeqNumbers.clear();

							break;
						}

						case RTCP::FeedbackPs::MessageType::FIR:
						{
							INFO("FIR required");

							REQUIRE(this->shouldTriggerFIR == true);

							this->shouldTriggerFIR = false;
							this->nackedSeqNumbers.clear();

							break;
						}

						default:;
					}

					break;
				}

				case RTCP::Type::RTPFB:
				{
					switch (dynamic_cast<RTCP::FeedbackRtpPacket*>(packet)->GetMessageType())
					{
						case RTCP::FeedbackRtp::MessageType::NACK:
						{
							INFO("NACK required");

							REQUIRE(this->shouldTriggerNack == true);

							this->shouldTriggerNack = false;

							auto* nackPacket = dynamic_cast<RTCP::FeedbackRtpNackPacket*>(packet);

							for (auto it = nackPacket->Begin(); it != nackPacket->End(); ++it)
							{
								RTCP::FeedbackRtpNackItem* item = *it;

								uint16_t firstSeq = item->GetPacketId();
								uint16_t bitmask  = item->GetLostPacketBitmask();

								this->nackedSeqNumbers.push_back(firstSeq);

								for (size_t i{ 1 }; i < MaxRequestedPackets; ++i)
								{
									if ((bitmask & 1) != 0)
									{
										this->nackedSeqNumbers.push_back(firstSeq + i);
									}

									bitmask >>= 1;
								}
							}

							break;
						}

						default:;
					}

					break;
				}

				default:;
			}
		}

		void OnRtpStreamNeedWorstRemoteFractionLost(
		  RTC::RtpStreamRecv* /*rtpStream*/, uint8_t& /*worstRemoteFractionLost*/) override
		{
		}

	public:
		bool shouldTriggerNack = false;
		bool shouldTriggerPLI  = false;
		bool shouldTriggerFIR  = false;
		std::vector<uint16_t> nackedSeqNumbers;
	};

	// clang-format off
	uint8_t buffer[] =
	{
		0x80, 0x01, 0x00, 0x01,
		0x00, 0x00, 0x00, 0x04,
		0x00, 0x00, 0x00, 0x05
	};
	// clang-format on

	std::unique_ptr<RtpPacket> packet{ RtpPacket::Parse(buffer, sizeof(buffer)) };

	if (!packet)
	{
		FAIL("not a RTP packet");
	}

	RtpStream::Params params;

	params.ssrc      = packet->GetSsrc();
	params.clockRate = 90000;
	params.useNack   = true;
	params.usePli    = true;
	params.useFir    = false;

	SECTION("NACK one packet")
	{
		RtpStreamRecvListener listener;
		RtpStreamRecv rtpStream(&listener, params, SendNackDelay, UseRtpInactivityCheck);

		packet->SetSequenceNumber(1);
		rtpStream.ReceivePacket(packet.get());

		packet->SetSequenceNumber(3);
		listener.shouldTriggerNack = true;
		listener.shouldTriggerPLI  = false;
		listener.shouldTriggerFIR  = false;
		rtpStream.ReceivePacket(packet.get());

		REQUIRE(listener.nackedSeqNumbers.size() == 1);
		REQUIRE(listener.nackedSeqNumbers[0] == 2);
		listener.nackedSeqNumbers.clear();

		packet->SetSequenceNumber(2);
		rtpStream.ReceivePacket(packet.get());

		REQUIRE(listener.nackedSeqNumbers.size() == 0);

		packet->SetSequenceNumber(4);
		rtpStream.ReceivePacket(packet.get());

		REQUIRE(listener.nackedSeqNumbers.size() == 0);
	}

	SECTION("wrapping sequence numbers")
	{
		RtpStreamRecvListener listener;
		RtpStreamRecv rtpStream(&listener, params, SendNackDelay, UseRtpInactivityCheck);

		packet->SetSequenceNumber(0xfffe);
		rtpStream.ReceivePacket(packet.get());

		packet->SetSequenceNumber(1);
		listener.shouldTriggerNack = true;
		listener.shouldTriggerPLI  = false;
		listener.shouldTriggerFIR  = false;
		rtpStream.ReceivePacket(packet.get());

		REQUIRE(listener.nackedSeqNumbers.size() == 2);
		REQUIRE(listener.nackedSeqNumbers[0] == 0xffff);
		REQUIRE(listener.nackedSeqNumbers[1] == 0);
		listener.nackedSeqNumbers.clear();
	}

	SECTION("require key frame")
	{
		RtpStreamRecvListener listener;
		RtpStreamRecv rtpStream(&listener, params, SendNackDelay, UseRtpInactivityCheck);

		packet->SetSequenceNumber(1);
		rtpStream.ReceivePacket(packet.get());

		// Seq different is bigger than MaxNackPackets in NackGenerator, so it
		// triggers a key frame.
		packet->SetSequenceNumber(1003);
		listener.shouldTriggerPLI = true;
		listener.shouldTriggerFIR = false;
		rtpStream.ReceivePacket(packet.get());
	}

	// Must run the loop to wait for UV timers and close them.
	DepLibUV::RunLoop();
}
