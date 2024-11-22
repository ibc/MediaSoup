#define MS_CLASS "RTC::TransportCongestionControlServer"
// #define MS_LOG_DEV_LEVEL 3

#include "RTC/TransportCongestionControlServer.hpp"
#include "DepLibUV.hpp"
#include "Logger.hpp"
#include "RTC/RTCP/FeedbackPsRemb.hpp"
#include <iterator> // std::ostream_iterator
#include <sstream>  // std::ostringstream

namespace RTC
{
	/* Static. */

	static constexpr uint64_t TransportCcFeedbackSendInterval{ 100u }; // In ms.
	static constexpr uint64_t LimitationRembInterval{ 1500u };         // In ms.
	static constexpr uint64_t PacketArrivalTimestampWindow{ 500u };    // In ms.
	static constexpr uint8_t UnlimitedRembNumPackets{ 4u };
	static constexpr size_t PacketLossHistogramLength{ 24 };

	/* Instance methods. */

	TransportCongestionControlServer::TransportCongestionControlServer(
	  RTC::TransportCongestionControlServer::Listener* listener,
	  RTC::BweType bweType,
	  size_t maxRtcpPacketLen)
	  : listener(listener), bweType(bweType), maxRtcpPacketLen(maxRtcpPacketLen)
	{
		MS_TRACE();

		switch (this->bweType)
		{
			case RTC::BweType::TRANSPORT_CC:
			{
				// Create a feedback packet.
				ResetTransportCcFeedback(0u);

				// Create the feedback send periodic timer.
				this->transportCcFeedbackSendPeriodicTimer = new TimerHandle(this);

				break;
			}

			case RTC::BweType::REMB:
			{
				this->rembServer = new webrtc::RemoteBitrateEstimatorAbsSendTime(this);

				break;
			}
		}
	}

	TransportCongestionControlServer::~TransportCongestionControlServer()
	{
		MS_TRACE();

		delete this->transportCcFeedbackSendPeriodicTimer;
		this->transportCcFeedbackSendPeriodicTimer = nullptr;

		// Delete REMB server.
		delete this->rembServer;
		this->rembServer = nullptr;
	}

	void TransportCongestionControlServer::TransportConnected()
	{
		MS_TRACE();

		switch (this->bweType)
		{
			case RTC::BweType::TRANSPORT_CC:
			{
				this->transportCcFeedbackSendPeriodicTimer->Start(
				  TransportCcFeedbackSendInterval, TransportCcFeedbackSendInterval);

				break;
			}

			default:;
		}
	}

	void TransportCongestionControlServer::TransportDisconnected()
	{
		MS_TRACE();

		switch (this->bweType)
		{
			case RTC::BweType::TRANSPORT_CC:
			{
				this->transportCcFeedbackSendPeriodicTimer->Stop();

				// Create a new feedback packet.
				ResetTransportCcFeedback(this->transportCcFeedbackPacketCount);

				break;
			}

			default:;
		}
	}

	double TransportCongestionControlServer::GetPacketLoss() const
	{
		MS_TRACE();

		return this->packetLoss;
	}

	void TransportCongestionControlServer::IncomingPacket(uint64_t nowMs, const RTC::RtpPacket* packet)
	{
		MS_TRACE();

		switch (this->bweType)
		{
			case RTC::BweType::TRANSPORT_CC:
			{
				uint16_t wideSeqNumber;

				if (!packet->ReadTransportWideCc01(wideSeqNumber))
				{
					break;
				}

				// Only insert the packet when receiving it for the first time.
				if (!this->mapPacketArrivalTimes.try_emplace(wideSeqNumber, nowMs).second)
				{
					break;
				}

				// We may receive packets with sequence number lower than the one in
				// previous tcc feedback, these packets may have been reported as lost
				// previously, therefore we need to reset the start sequence num for the
				// next tcc feedback.
				if (
				  !this->transportWideSeqNumberReceived ||
				  RTC::SeqManager<uint16_t>::IsSeqLowerThan(
				    wideSeqNumber, this->transportCcFeedbackWideSeqNumStart))
				{
					this->transportCcFeedbackWideSeqNumStart = wideSeqNumber;
				}

				this->transportWideSeqNumberReceived = true;

				MayDropOldPacketArrivalTimes(wideSeqNumber, nowMs);

				// Update the RTCP media SSRC of the ongoing Transport-CC Feedback packet.
				this->transportCcFeedbackSenderSsrc = 0u;
				this->transportCcFeedbackMediaSsrc  = packet->GetSsrc();

				MaySendLimitationRembFeedback(nowMs);

				break;
			}

			case RTC::BweType::REMB:
			{
				uint32_t absSendTime;

				if (!packet->ReadAbsSendTime(absSendTime))
				{
					break;
				}

				// NOTE: nowMs is uint64_t but we need to "convert" it to int64_t before
				// we give it to libwebrtc lib (althought this is implicit in the
				// conversion so it would be converted within the method call).
				auto nowMsInt64 = static_cast<int64_t>(nowMs);

				this->rembServer->IncomingPacket(nowMsInt64, packet->GetPayloadLength(), *packet, absSendTime);

				break;
			}
		}
	}

	void TransportCongestionControlServer::FillAndSendTransportCcFeedback()
	{
		MS_TRACE();

		if (!this->transportWideSeqNumberReceived)
		{
			return;
		}

		auto it = this->mapPacketArrivalTimes.lower_bound(this->transportCcFeedbackWideSeqNumStart);

		if (it == this->mapPacketArrivalTimes.end())
		{
			return;
		}

		for (; it != this->mapPacketArrivalTimes.end(); ++it)
		{
			auto sequenceNumber = it->first;
			auto timestamp      = it->second;

			// If the base is not set in this packet let's set it.
			// NOTE: This maybe needed many times during this loop since the current
			// feedback packet maybe a fresh new one if the previous one was full (so
			// already sent) or failed to be built.
			if (!this->transportCcFeedbackPacket->IsBaseSet())
			{
				// Set base sequence num and reference time.
				this->transportCcFeedbackPacket->SetBase(this->transportCcFeedbackWideSeqNumStart, timestamp);
			}

			auto result = this->transportCcFeedbackPacket->AddPacket(
			  sequenceNumber, timestamp, this->maxRtcpPacketLen);

			switch (result)
			{
				case RTC::RTCP::FeedbackRtpTransportPacket::AddPacketResult::SUCCESS:
				{
					// If the feedback packet is full, send it now.
					if (this->transportCcFeedbackPacket->IsFull())
					{
						MS_DEBUG_DEV("transport-cc feedback packet is full, sending feedback now");

						auto sent = SendTransportCcFeedback();

						if (sent)
						{
							++this->transportCcFeedbackPacketCount;
						}

						// Create a new feedback packet.
						ResetTransportCcFeedback(this->transportCcFeedbackPacketCount);
					}

					break;
				}

				case RTC::RTCP::FeedbackRtpTransportPacket::AddPacketResult::MAX_SIZE_EXCEEDED:
				{
					// This should not happen.
					MS_WARN_TAG(rtcp, "transport-cc feedback packet is exceeded");

					// Create a new feedback packet.
					// NOTE: Do not increment packet count it since the previous ongoing
					// feedback packet was not sent.
					ResetTransportCcFeedback(this->transportCcFeedbackPacketCount);

					break;
				}

				case RTC::RTCP::FeedbackRtpTransportPacket::AddPacketResult::FATAL:
				{
					// Create a new feedback packet.
					// NOTE: Do not increment packet count it since the previous ongoing
					// feedback packet was not sent.
					ResetTransportCcFeedback(this->transportCcFeedbackPacketCount);

					break;
				}
			}
		}

		// It may happen that the packet is empty (no deltas) but in that case
		// SendTransportCcFeedback() won't send it so we are safe.
		auto sent = SendTransportCcFeedback();

		if (sent)
		{
			++this->transportCcFeedbackPacketCount;
		}

		// Create a new feedback packet.
		ResetTransportCcFeedback(this->transportCcFeedbackPacketCount);
	}

	void TransportCongestionControlServer::SetMaxIncomingBitrate(uint32_t bitrate)
	{
		MS_TRACE();

		auto previousMaxIncomingBitrate = this->maxIncomingBitrate;

		this->maxIncomingBitrate = bitrate;

		if (previousMaxIncomingBitrate != 0u && this->maxIncomingBitrate == 0u)
		{
			// This is to ensure that we send N REMB packets with bitrate 0 (unlimited).
			this->unlimitedRembCounter = UnlimitedRembNumPackets;

			auto nowMs = DepLibUV::GetTimeMs();

			MaySendLimitationRembFeedback(nowMs);
		}
	}

	bool TransportCongestionControlServer::SendTransportCcFeedback()
	{
		MS_TRACE();

		this->transportCcFeedbackPacket->Finish();

		if (!this->transportCcFeedbackPacket->IsSerializable())
		{
			MS_WARN_TAG(rtcp, "couldn't send feedback-cc packet because it is not serializable");

			return false;
		}

		auto latestWideSeqNumber = this->transportCcFeedbackPacket->GetLatestSequenceNumber();

		// Notify the listener.
		this->listener->OnTransportCongestionControlServerSendRtcpPacket(
		  this, this->transportCcFeedbackPacket.get());

		// Update packet loss history.
		const size_t expectedPackets = this->transportCcFeedbackPacket->GetPacketStatusCount();
		size_t lostPackets           = 0;

		for (const auto& result : this->transportCcFeedbackPacket->GetPacketResults())
		{
			if (!result.received)
			{
				lostPackets += 1;
			}
		}

		if (expectedPackets > 0)
		{
			this->UpdatePacketLoss(static_cast<double>(lostPackets) / expectedPackets);
		}

		this->transportCcFeedbackWideSeqNumStart = latestWideSeqNumber + 1;

		return true;
	}

	void TransportCongestionControlServer::MayDropOldPacketArrivalTimes(uint16_t seqNum, uint64_t nowMs)
	{
		MS_TRACE();

		// Ignore nowMs value if it's smaller than PacketArrivalTimestampWindow in
		// order to avoid negative values (should never happen) and return early if
		// the condition is met.
		if (nowMs >= PacketArrivalTimestampWindow)
		{
			uint64_t expiryTimestamp = nowMs - PacketArrivalTimestampWindow;
			auto it                  = this->mapPacketArrivalTimes.begin();

			while (it != this->mapPacketArrivalTimes.end() &&
			       it->first != this->transportCcFeedbackWideSeqNumStart &&
			       RTC::SeqManager<uint16_t>::IsSeqLowerThan(it->first, seqNum) &&
			       it->second <= expiryTimestamp)
			{
				it = this->mapPacketArrivalTimes.erase(it);
			}
		}
	}

	void TransportCongestionControlServer::MaySendLimitationRembFeedback(uint64_t nowMs)
	{
		MS_TRACE();

		// May fix unlimitedRembCounter.
		if (this->unlimitedRembCounter > 0u && this->maxIncomingBitrate != 0u)
		{
			this->unlimitedRembCounter = 0u;
		}

		// In case this is the first unlimited REMB packet, send it fast.
		// clang-format off
		if (
			(
				(this->bweType != RTC::BweType::REMB && this->maxIncomingBitrate != 0u) ||
				this->unlimitedRembCounter > 0u
			) &&
			(
				nowMs - this->limitationRembSentAtMs > LimitationRembInterval ||
				this->unlimitedRembCounter == UnlimitedRembNumPackets
			)
		)
		// clang-format on
		{
			MS_DEBUG_DEV(
			  "sending limitation RTCP REMB packet [bitrate:%" PRIu32 "]", this->maxIncomingBitrate);

			RTC::RTCP::FeedbackPsRembPacket packet(0u, 0u);

			packet.SetBitrate(this->maxIncomingBitrate);
			packet.Serialize(RTC::RTCP::Buffer);

			// Notify the listener.
			this->listener->OnTransportCongestionControlServerSendRtcpPacket(this, &packet);

			this->limitationRembSentAtMs = nowMs;

			if (this->unlimitedRembCounter > 0u)
			{
				this->unlimitedRembCounter--;
			}
		}
	}

	void TransportCongestionControlServer::UpdatePacketLoss(double packetLoss)
	{
		// Add the lost into the histogram.
		if (this->packetLossHistory.size() == PacketLossHistogramLength)
		{
			this->packetLossHistory.pop_front();
		}

		this->packetLossHistory.push_back(packetLoss);

		// Calculate a weighted average
		size_t weight{ 0 };
		size_t samples{ 0 };
		double totalPacketLoss{ 0 };

		for (auto packetLossEntry : this->packetLossHistory)
		{
			weight++;
			samples += weight;
			totalPacketLoss += weight * packetLossEntry;
		}

		// clang-tidy "thinks" that this can lead to division by zero but we are
		// smarter.
		// NOLINTNEXTLINE(clang-analyzer-core.DivideZero)
		this->packetLoss = totalPacketLoss / samples;
	}

	void TransportCongestionControlServer::ResetTransportCcFeedback(uint8_t feedbackPacketCount)
	{
		MS_TRACE();

		this->transportCcFeedbackPacket.reset(new RTC::RTCP::FeedbackRtpTransportPacket(
		  this->transportCcFeedbackSenderSsrc, this->transportCcFeedbackMediaSsrc));

		this->transportCcFeedbackPacket->SetFeedbackPacketCount(feedbackPacketCount);
	}

	void TransportCongestionControlServer::OnRembServerAvailableBitrate(
	  const webrtc::RemoteBitrateEstimator* /*rembServer*/,
	  const std::vector<uint32_t>& ssrcs,
	  uint32_t availableBitrate)
	{
		MS_TRACE();

		// Limit announced bitrate if requested via API.
		if (this->maxIncomingBitrate != 0u)
		{
			availableBitrate = std::min(availableBitrate, this->maxIncomingBitrate);
		}

#if MS_LOG_DEV_LEVEL == 3
		std::ostringstream ssrcsStream;

		if (!ssrcs.empty())
		{
			std::copy(ssrcs.begin(), ssrcs.end() - 1, std::ostream_iterator<uint32_t>(ssrcsStream, ","));
			ssrcsStream << ssrcs.back();
		}

		MS_DEBUG_DEV(
		  "sending RTCP REMB packet [bitrate:%" PRIu32 ", ssrcs:%s]",
		  availableBitrate,
		  ssrcsStream.str().c_str());
#endif

		RTC::RTCP::FeedbackPsRembPacket packet(0u, 0u);

		packet.SetBitrate(availableBitrate);
		packet.SetSsrcs(ssrcs);
		packet.Serialize(RTC::RTCP::Buffer);

		// Notify the listener.
		this->listener->OnTransportCongestionControlServerSendRtcpPacket(this, &packet);
	}

	void TransportCongestionControlServer::OnTimer(TimerHandle* timer)
	{
		MS_TRACE();

		if (timer == this->transportCcFeedbackSendPeriodicTimer)
		{
			FillAndSendTransportCcFeedback();
		}
	}
} // namespace RTC
