#ifndef MS_RTC_TRANSPORT_CONGESTION_CONTROL_CLIENT_HPP
#define MS_RTC_TRANSPORT_CONGESTION_CONTROL_CLIENT_HPP

#include "common.hpp"
#include "RTC/BweType.hpp"
#include "RTC/RTCP/FeedbackRtpTransport.hpp"
#include "RTC/RTCP/ReceiverReport.hpp"
#include "RTC/RtpPacket.hpp"
#include "RTC/RtpProbationGenerator.hpp"
#include "RTC/TrendCalculator.hpp"
#include "handles/TimerHandle.hpp"
#include <libwebrtc/api/transport/goog_cc_factory.h>
#include <libwebrtc/api/transport/network_types.h>
#include <libwebrtc/call/rtp_transport_controller_send.h>
#include <libwebrtc/modules/pacing/packet_router.h>
#include <deque>

namespace RTC
{
	constexpr uint32_t TransportCongestionControlMinOutgoingBitrate{ 30000u };

	class TransportCongestionControlClient : public webrtc::PacketRouter,
	                                         public webrtc::TargetTransferRateObserver,
	                                         public TimerHandle::Listener
	{
	public:
		struct Bitrates
		{
			uint32_t desiredBitrate{ 0u };
			uint32_t effectiveDesiredBitrate{ 0u };
			uint32_t minBitrate{ 0u };
			uint32_t maxBitrate{ 0u };
			uint32_t startBitrate{ 0u };
			uint32_t maxPaddingBitrate{ 0u };
			uint32_t availableBitrate{ 0u };
		};

	public:
		class Listener
		{
		public:
			virtual ~Listener() = default;

		public:
			virtual void OnTransportCongestionControlClientBitrates(
			  RTC::TransportCongestionControlClient* tccClient,
			  RTC::TransportCongestionControlClient::Bitrates& bitrates) = 0;
			virtual void OnTransportCongestionControlClientSendRtpPacket(
			  RTC::TransportCongestionControlClient* tccClient,
			  RTC::RtpPacket* packet,
			  const webrtc::PacedPacketInfo& pacingInfo) = 0;
		};

	public:
		TransportCongestionControlClient(
		  RTC::TransportCongestionControlClient::Listener* listener,
		  RTC::BweType bweType,
		  uint32_t initialAvailableBitrate,
		  uint32_t maxOutgoingBitrate,
		  uint32_t minOutgoingBitrate);
		~TransportCongestionControlClient() override;

	public:
		RTC::BweType GetBweType() const
		{
			return this->bweType;
		}
		void TransportConnected();
		void TransportDisconnected();
		void InsertPacket(webrtc::RtpPacketSendInfo& packetInfo);
		webrtc::PacedPacketInfo GetPacingInfo();
		void PacketSent(const webrtc::RtpPacketSendInfo& packetInfo, int64_t nowMs);
		void ReceiveEstimatedBitrate(uint32_t bitrate);
		void ReceiveRtcpReceiverReport(RTC::RTCP::ReceiverReportPacket* packet, float rtt, int64_t nowMs);
		void ReceiveRtcpTransportFeedback(const RTC::RTCP::FeedbackRtpTransportPacket* feedback);
		void SetDesiredBitrate(uint32_t desiredBitrate, bool force);
		void SetMaxOutgoingBitrate(uint32_t maxBitrate);
		void SetMinOutgoingBitrate(uint32_t minBitrate);
		const Bitrates& GetBitrates() const
		{
			return this->bitrates;
		}
		uint32_t GetAvailableBitrate() const;
		double GetPacketLoss() const;
		void RescheduleNextAvailableBitrateEvent();

	private:
		void MayEmitAvailableBitrateEvent(uint32_t previousAvailableBitrate);
		void UpdatePacketLoss(double packetLoss);
		void ApplyBitrateUpdates();

		void InitializeController();
		void DestroyController();

		// jmillan: missing.
		// void OnRemoteNetworkEstimate(NetworkStateEstimate estimate) override;

		/* Pure virtual methods inherited from webrtc::TargetTransferRateObserver. */
	public:
		void OnTargetTransferRate(webrtc::TargetTransferRate targetTransferRate) override;

		/* Pure virtual methods inherited from webrtc::PacketRouter. */
	public:
		void SendPacket(RTC::RtpPacket* packet, const webrtc::PacedPacketInfo& pacingInfo) override;
		RTC::RtpPacket* GeneratePadding(size_t size) override;

		/* Pure virtual methods inherited from RTC::TimerHandle. */
	public:
		void OnTimer(TimerHandle* timer) override;

	private:
		// Passed by argument.
		Listener* listener{ nullptr };
		// Allocated by this.
		webrtc::NetworkControllerFactoryInterface* controllerFactory{ nullptr };
		webrtc::RtpTransportControllerSend* rtpTransportControllerSend{ nullptr };
		RTC::RtpProbationGenerator* probationGenerator{ nullptr };
		TimerHandle* processTimer{ nullptr };
		// Others.
		RTC::BweType bweType;
		uint32_t initialAvailableBitrate{ 0u };
		uint32_t maxOutgoingBitrate{ 0u };
		uint32_t minOutgoingBitrate{ 0u };
		Bitrates bitrates;
		bool availableBitrateEventCalled{ false };
		uint64_t lastAvailableBitrateEventAtMs{ 0u };
		RTC::TrendCalculator desiredBitrateTrend;
		std::deque<double> packetLossHistory;
		double packetLoss{ 0 };
	};
} // namespace RTC

#endif
