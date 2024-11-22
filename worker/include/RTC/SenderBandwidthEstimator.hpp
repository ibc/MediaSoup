#ifndef MS_RTC_SENDER_BANDWIDTH_ESTIMATOR_HPP
#define MS_RTC_SENDER_BANDWIDTH_ESTIMATOR_HPP

#include "common.hpp"
#include "RTC/RTCP/FeedbackRtpTransport.hpp"
#include "RTC/RateCalculator.hpp"
#include "RTC/SeqManager.hpp"
#include "RTC/TrendCalculator.hpp"
#include <map>

namespace RTC
{
	class SenderBandwidthEstimator
	{
	public:
		class Listener
		{
		public:
			virtual ~Listener() = default;

		public:
			virtual void OnSenderBandwidthEstimatorAvailableBitrate(
			  RTC::SenderBandwidthEstimator* senderBwe,
			  uint32_t availableBitrate,
			  uint32_t previousAvailableBitrate) = 0;
		};

	public:
		struct SentInfo
		{
			uint16_t wideSeq{ 0u };
			size_t size{ 0u };
			bool isProbation{ false };
			uint64_t sendingAtMs{ 0u };
			uint64_t sentAtMs{ 0u };
		};

	private:
		class CummulativeResult
		{
		public:
			CummulativeResult() = default;

		public:
			uint64_t GetStartedAtMs() const
			{
				return this->firstPacketSentAtMs;
			}
			size_t GetNumPackets() const
			{
				return this->numPackets;
			}
			size_t GetTotalSize() const
			{
				return this->totalSize;
			}
			uint32_t GetSendBitrate() const
			{
				auto sendIntervalMs =
				  std::max<uint64_t>(this->lastPacketSentAtMs - this->firstPacketSentAtMs, 1u);

				return static_cast<uint32_t>(this->totalSize / sendIntervalMs) * 8 * 1000;
			}
			uint32_t GetReceiveBitrate() const
			{
				auto recvIntervalMs =
				  std::max<uint64_t>(this->lastPacketReceivedAtMs - this->firstPacketReceivedAtMs, 1u);

				return static_cast<uint32_t>(this->totalSize / recvIntervalMs) * 8 * 1000;
			}
			void AddPacket(size_t size, int64_t sentAtMs, int64_t receivedAtMs);
			void Reset();

		private:
			size_t numPackets{ 0u };
			size_t totalSize{ 0u };
			int64_t firstPacketSentAtMs{ 0u };
			int64_t lastPacketSentAtMs{ 0u };
			int64_t firstPacketReceivedAtMs{ 0u };
			int64_t lastPacketReceivedAtMs{ 0u };
		};

	public:
		SenderBandwidthEstimator(
		  RTC::SenderBandwidthEstimator::Listener* listener, uint32_t initialAvailableBitrate);
		virtual ~SenderBandwidthEstimator();

	public:
		void TransportConnected();
		void TransportDisconnected();
		void RtpPacketSent(const SentInfo& sentInfo);
		void ReceiveRtcpTransportFeedback(const RTC::RTCP::FeedbackRtpTransportPacket* feedback);
		void EstimateAvailableBitrate(CummulativeResult& cummulativeResult);
		void UpdateRtt(float rtt);
		uint32_t GetAvailableBitrate() const;
		void RescheduleNextAvailableBitrateEvent();

	private:
		// Passed by argument.
		Listener* listener{ nullptr };
		// Others.
		uint32_t initialAvailableBitrate{ 0u };
		uint32_t availableBitrate{ 0u };
		uint64_t lastAvailableBitrateEventAtMs{ 0u };
		std::map<uint16_t, SentInfo, RTC::SeqManager<uint16_t>::SeqLowerThan> sentInfos;
		float rtt{ 0 }; // Round trip time in ms.
		CummulativeResult cummulativeResult;
		CummulativeResult probationCummulativeResult;
		RTC::RateCalculator sendTransmission;
		RTC::TrendCalculator sendTransmissionTrend;
	};
} // namespace RTC

#endif
