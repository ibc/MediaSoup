#define MS_CLASS "RTC::SenderBandwidthEstimator"
// #define MS_LOG_DEV_LEVEL 3

#include "RTC/SenderBandwidthEstimator.hpp"
#include "DepLibUV.hpp"
#include "Logger.hpp"

namespace RTC
{
	/* Static. */

	// static constexpr uint64_t AvailableBitrateEventInterval{ 2000u }; // In ms.
	static constexpr uint16_t MaxSentInfoAge{ 2000u }; // TODO: Let's see.
	static constexpr float DefaultRtt{ 100 };

	/* Instance methods. */

	SenderBandwidthEstimator::SenderBandwidthEstimator(
	  RTC::SenderBandwidthEstimator::Listener* listener, uint32_t initialAvailableBitrate)
	  : listener(listener), initialAvailableBitrate(initialAvailableBitrate), rtt(DefaultRtt),
	    sendTransmission(1000u), sendTransmissionTrend(0.15f)
	{
		MS_TRACE();
	}

	SenderBandwidthEstimator::~SenderBandwidthEstimator()
	{
		MS_TRACE();
	}

	void SenderBandwidthEstimator::TransportConnected()
	{
		MS_TRACE();

		this->availableBitrate              = this->initialAvailableBitrate;
		this->lastAvailableBitrateEventAtMs = DepLibUV::GetTimeMs();
	}

	void SenderBandwidthEstimator::TransportDisconnected()
	{
		MS_TRACE();

		this->availableBitrate = 0u;

		this->sentInfos.clear();
		this->cummulativeResult.Reset();
	}

	void SenderBandwidthEstimator::RtpPacketSent(const SentInfo& sentInfo)
	{
		MS_TRACE();

		auto nowMs = sentInfo.sentAtMs;

		// Remove old sent infos.
		auto it = this->sentInfos.lower_bound(sentInfo.wideSeq - MaxSentInfoAge + 1);

		this->sentInfos.erase(this->sentInfos.begin(), it);

		// Insert the sent info into the map.
		this->sentInfos[sentInfo.wideSeq] = sentInfo;

		// Fill the send transmission counter.
		this->sendTransmission.Update(sentInfo.size, nowMs);

		// Update the send transmission trend.
		auto sendBitrate = this->sendTransmission.GetRate(nowMs);

		this->sendTransmissionTrend.Update(sendBitrate, nowMs);

		// TODO: Remove.
		// MS_DEBUG_DEV(
		// 	"[wideSeq:%" PRIu16 ", size:%zu, isProbation:%s, bitrate:%" PRIu32 "]",
		// 	sentInfo.wideSeq,
		// 	sentInfo.size,
		// 	sentInfo.isProbation ? "true" : "false",
		// 	sendBitrate);
	}

	void SenderBandwidthEstimator::ReceiveRtcpTransportFeedback(
	  const RTC::RTCP::FeedbackRtpTransportPacket* feedback)
	{
		MS_TRACE();

		auto nowMs               = DepLibUV::GetTimeMs();
		const uint64_t elapsedMs = nowMs - this->cummulativeResult.GetStartedAtMs();

		// Drop ongoing cummulative result if too old.
		if (elapsedMs > 1000u)
		{
			this->cummulativeResult.Reset();
		}

		for (auto& result : feedback->GetPacketResults())
		{
			if (!result.received)
			{
				continue;
			}

			const uint16_t wideSeq = result.sequenceNumber;
			auto it                = this->sentInfos.find(wideSeq);

			if (it == this->sentInfos.end())
			{
				MS_WARN_DEV("received packet not present in sent infos [wideSeq:%" PRIu16 "]", wideSeq);

				continue;
			}

			auto& sentInfo = it->second;

			if (!sentInfo.isProbation)
			{
				this->cummulativeResult.AddPacket(
				  sentInfo.size, static_cast<int64_t>(sentInfo.sentAtMs), result.receivedAtMs);
			}
			else
			{
				this->probationCummulativeResult.AddPacket(
				  sentInfo.size, static_cast<int64_t>(sentInfo.sentAtMs), result.receivedAtMs);
			}
		}

		// Handle probation packets separately.
		if (this->probationCummulativeResult.GetNumPackets() >= 2u)
		{
			// MS_DEBUG_DEV(
			//   "probation [packets:%zu, size:%zu] "
			//   "[send bps:%" PRIu32 ", recv bps:%" PRIu32 "] ",
			//   this->probationCummulativeResult.GetNumPackets(),
			//   this->probationCummulativeResult.GetTotalSize(),
			//   this->probationCummulativeResult.GetSendBitrate(),
			//   this->probationCummulativeResult.GetReceiveBitrate());

			EstimateAvailableBitrate(this->probationCummulativeResult);

			// Reset probation cummulative result.
			this->probationCummulativeResult.Reset();
		}
		else
		{
			// Reset probation cummulative result.
			this->probationCummulativeResult.Reset();
		}

		// Handle real and probation packets all together.
		if (elapsedMs >= 100u && this->cummulativeResult.GetNumPackets() >= 20u)
		{
			// auto sendBitrate      = this->sendTransmission.GetRate(nowMs);
			// auto sendBitrateTrend = this->sendTransmissionTrend.GetValue();

			// MS_DEBUG_DEV(
			//   "real+prob [packets:%zu, size:%zu] "
			//   "[send bps:%" PRIu32 ", recv bps:%" PRIu32
			//   "] "
			//   "[real bps:%" PRIu32 ", trend bps:%" PRIu32 "]",
			//   this->cummulativeResult.GetNumPackets(),
			//   this->cummulativeResult.GetTotalSize(),
			//   this->cummulativeResult.GetSendBitrate(),
			//   this->cummulativeResult.GetReceiveBitrate(),
			//   sendBitrate,
			//   sendBitrateTrend);

			EstimateAvailableBitrate(this->cummulativeResult);

			// Reset cummulative result.
			this->cummulativeResult.Reset();
		}
	}

	void SenderBandwidthEstimator::EstimateAvailableBitrate(CummulativeResult& cummulativeResult)
	{
		MS_TRACE();

		auto previousAvailableBitrate = this->availableBitrate;

		const double ratio = static_cast<double>(cummulativeResult.GetReceiveBitrate()) /
		                     static_cast<double>(cummulativeResult.GetSendBitrate());
		auto bitrate =
		  std::min<uint32_t>(cummulativeResult.GetReceiveBitrate(), cummulativeResult.GetSendBitrate());

		if (0.75f <= ratio && ratio <= 1.25f)
		{
			if (bitrate > this->availableBitrate)
			{
				this->availableBitrate = bitrate;

				MS_DEBUG_DEV("BWE UP [ratio:%f, availableBitrate:%" PRIu32 "]", ratio, this->availableBitrate);
			}
		}
		else
		{
			if (bitrate < this->availableBitrate)
			{
				this->availableBitrate = bitrate;

				MS_DEBUG_DEV(
				  "BWE DOWN [ratio:%f, availableBitrate:%" PRIu32 "]", ratio, this->availableBitrate);
			}
		}

		// TODO: No, should wait for AvailableBitrateEventInterval and so on.
		this->listener->OnSenderBandwidthEstimatorAvailableBitrate(
		  this, this->availableBitrate, previousAvailableBitrate);
	}

	void SenderBandwidthEstimator::UpdateRtt(float rtt)
	{
		MS_TRACE();

		this->rtt = rtt;
	}

	uint32_t SenderBandwidthEstimator::GetAvailableBitrate() const
	{
		MS_TRACE();

		return this->availableBitrate;
	}

	void SenderBandwidthEstimator::RescheduleNextAvailableBitrateEvent()
	{
		MS_TRACE();

		this->lastAvailableBitrateEventAtMs = DepLibUV::GetTimeMs();
	}

	void SenderBandwidthEstimator::CummulativeResult::AddPacket(
	  size_t size, int64_t sentAtMs, int64_t receivedAtMs)
	{
		MS_TRACE();

		if (this->numPackets == 0u)
		{
			this->firstPacketSentAtMs     = sentAtMs;
			this->firstPacketReceivedAtMs = receivedAtMs;
			this->lastPacketSentAtMs      = sentAtMs;
			this->lastPacketReceivedAtMs  = receivedAtMs;
		}
		else
		{
			if (sentAtMs < this->firstPacketSentAtMs)
			{
				this->firstPacketSentAtMs = sentAtMs;
			}

			if (receivedAtMs < this->firstPacketReceivedAtMs)
			{
				this->firstPacketReceivedAtMs = receivedAtMs;
			}

			if (sentAtMs > this->lastPacketSentAtMs)
			{
				this->lastPacketSentAtMs = sentAtMs;
			}

			if (receivedAtMs > this->lastPacketReceivedAtMs)
			{
				this->lastPacketReceivedAtMs = receivedAtMs;
			}
		}

		this->numPackets++;
		this->totalSize += size;
	}

	void SenderBandwidthEstimator::CummulativeResult::Reset()
	{
		MS_TRACE();

		this->numPackets              = 0u;
		this->totalSize               = 0u;
		this->firstPacketSentAtMs     = 0u;
		this->lastPacketSentAtMs      = 0u;
		this->firstPacketReceivedAtMs = 0u;
		this->lastPacketReceivedAtMs  = 0u;
	}
} // namespace RTC
