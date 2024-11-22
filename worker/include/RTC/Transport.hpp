#ifndef MS_RTC_TRANSPORT_HPP
#define MS_RTC_TRANSPORT_HPP
// #define ENABLE_RTC_SENDER_BANDWIDTH_ESTIMATOR

#include "common.hpp"
#include "DepLibUV.hpp"
#include "Channel/ChannelNotification.hpp"
#include "Channel/ChannelRequest.hpp"
#include "Channel/ChannelSocket.hpp"
#include "FBS/transport.h"
#include "RTC/Consumer.hpp"
#include "RTC/DataConsumer.hpp"
#include "RTC/DataProducer.hpp"
#include "RTC/Producer.hpp"
#include "RTC/RTCP/CompoundPacket.hpp"
#include "RTC/RTCP/Packet.hpp"
#include "RTC/RTCP/ReceiverReport.hpp"
#include "RTC/RateCalculator.hpp"
#include "RTC/RtpHeaderExtensionIds.hpp"
#include "RTC/RtpListener.hpp"
#include "RTC/RtpPacket.hpp"
#include "RTC/SctpAssociation.hpp"
#include "RTC/SctpListener.hpp"
#include "RTC/Shared.hpp"
#ifdef ENABLE_RTC_SENDER_BANDWIDTH_ESTIMATOR
#include "RTC/SenderBandwidthEstimator.hpp"
#endif
#include "RTC/TransportCongestionControlClient.hpp"
#include "RTC/TransportCongestionControlServer.hpp"
#include "handles/TimerHandle.hpp"
#include <absl/container/flat_hash_map.h>
#include <string>
#include <vector>

namespace RTC
{
	class Transport : public RTC::Producer::Listener,
	                  public RTC::Consumer::Listener,
	                  public RTC::DataProducer::Listener,
	                  public RTC::DataConsumer::Listener,
	                  public RTC::SctpAssociation::Listener,
	                  public RTC::TransportCongestionControlClient::Listener,
	                  public RTC::TransportCongestionControlServer::Listener,
	                  public Channel::ChannelSocket::RequestHandler,
	                  public Channel::ChannelSocket::NotificationHandler,
#ifdef ENABLE_RTC_SENDER_BANDWIDTH_ESTIMATOR
	                  public RTC::SenderBandwidthEstimator::Listener,
#endif
	                  public TimerHandle::Listener
	{
	protected:
		using onSendCallback   = const std::function<void(bool sent)>;
		using onQueuedCallback = const std::function<void(bool queued, bool sctpSendBufferFull)>;

	public:
		class Listener
		{
		public:
			virtual ~Listener() = default;

		public:
			virtual void OnTransportNewProducer(RTC::Transport* transport, RTC::Producer* producer) = 0;
			virtual void OnTransportProducerClosed(RTC::Transport* transport, RTC::Producer* producer) = 0;
			virtual void OnTransportProducerPaused(RTC::Transport* transport, RTC::Producer* producer) = 0;
			virtual void OnTransportProducerResumed(RTC::Transport* transport, RTC::Producer* producer) = 0;
			virtual void OnTransportProducerNewRtpStream(
			  RTC::Transport* transport,
			  RTC::Producer* producer,
			  RTC::RtpStreamRecv* rtpStream,
			  uint32_t mappedSsrc) = 0;
			virtual void OnTransportProducerRtpStreamScore(
			  RTC::Transport* transport,
			  RTC::Producer* producer,
			  RTC::RtpStreamRecv* rtpStream,
			  uint8_t score,
			  uint8_t previousScore) = 0;
			virtual void OnTransportProducerRtcpSenderReport(
			  RTC::Transport* transport,
			  RTC::Producer* producer,
			  RTC::RtpStreamRecv* rtpStream,
			  bool first) = 0;
			virtual void OnTransportProducerRtpPacketReceived(
			  RTC::Transport* transport, RTC::Producer* producer, RTC::RtpPacket* packet) = 0;
			virtual void OnTransportNeedWorstRemoteFractionLost(
			  RTC::Transport* transport,
			  RTC::Producer* producer,
			  uint32_t mappedSsrc,
			  uint8_t& worstRemoteFractionLost) = 0;
			virtual void OnTransportNewConsumer(
			  RTC::Transport* transport, RTC::Consumer* consumer, const std::string& producerId) = 0;
			virtual void OnTransportConsumerClosed(RTC::Transport* transport, RTC::Consumer* consumer) = 0;
			virtual void OnTransportConsumerProducerClosed(
			  RTC::Transport* transport, RTC::Consumer* consumer) = 0;
			virtual void OnTransportDataProducerPaused(
			  RTC::Transport* transport, RTC::DataProducer* dataProducer) = 0;
			virtual void OnTransportDataProducerResumed(
			  RTC::Transport* transport, RTC::DataProducer* dataProducer) = 0;
			virtual void OnTransportConsumerKeyFrameRequested(
			  RTC::Transport* transport, RTC::Consumer* consumer, uint32_t mappedSsrc) = 0;
			virtual void OnTransportNewDataProducer(
			  RTC::Transport* transport, RTC::DataProducer* dataProducer) = 0;
			virtual void OnTransportDataProducerClosed(
			  RTC::Transport* transport, RTC::DataProducer* dataProducer) = 0;
			virtual void OnTransportDataProducerMessageReceived(
			  RTC::Transport* transport,
			  RTC::DataProducer* dataProducer,
			  const uint8_t* msg,
			  size_t len,
			  uint32_t ppid,
			  std::vector<uint16_t>& subchannels,
			  std::optional<uint16_t> requiredSubchannel) = 0;
			virtual void OnTransportNewDataConsumer(
			  RTC::Transport* transport, RTC::DataConsumer* dataConsumer, std::string& dataProducerId) = 0;
			virtual void OnTransportDataConsumerClosed(
			  RTC::Transport* transport, RTC::DataConsumer* dataConsumer) = 0;
			virtual void OnTransportDataConsumerDataProducerClosed(
			  RTC::Transport* transport, RTC::DataConsumer* dataConsumer)         = 0;
			virtual void OnTransportListenServerClosed(RTC::Transport* transport) = 0;
		};

	public:
		struct SocketFlags
		{
			bool ipv6Only{ false };
			bool udpReusePort{ false };
		};

		struct PortRange
		{
			uint16_t min{ 0u };
			uint16_t max{ 0u };
		};

		struct ListenInfo
		{
			std::string ip;
			std::string announcedAddress;
			uint16_t port{ 0u };
			PortRange portRange;
			SocketFlags flags;
			uint32_t sendBufferSize{ 0u };
			uint32_t recvBufferSize{ 0u };
		};

	private:
		struct TraceEventTypes
		{
			bool probation{ false };
			bool bwe{ false };
		};

	public:
		Transport(
		  RTC::Shared* shared,
		  const std::string& id,
		  RTC::Transport::Listener* listener,
		  const FBS::Transport::Options* options);
		~Transport() override;

	public:
		void CloseProducersAndConsumers();
		void ListenServerClosed();
		// Subclasses must also invoke the parent Close().
		flatbuffers::Offset<FBS::Transport::Stats> FillBufferStats(flatbuffers::FlatBufferBuilder& builder);
		flatbuffers::Offset<FBS::Transport::Dump> FillBuffer(flatbuffers::FlatBufferBuilder& builder) const;

		/* Methods inherited from Channel::ChannelSocket::RequestHandler. */
	public:
		void HandleRequest(Channel::ChannelRequest* request) override;

		/* Methods inherited from Channel::ChannelSocket::NotificationHandler. */
	public:
		void HandleNotification(Channel::ChannelNotification* notification) override;

	protected:
		// Must be called from the subclass.
		void Destroying();
		void Connected();
		void Disconnected();
		void DataReceived(size_t len)
		{
			this->recvTransmission.Update(len, DepLibUV::GetTimeMs());
		}
		void DataSent(size_t len)
		{
			this->sendTransmission.Update(len, DepLibUV::GetTimeMs());
		}
		void ReceiveRtpPacket(RTC::RtpPacket* packet);
		void ReceiveRtcpPacket(RTC::RTCP::Packet* packet);
		void ReceiveSctpData(const uint8_t* data, size_t len);
		RTC::Producer* GetProducerById(const std::string& producerId) const;
		RTC::Consumer* GetConsumerById(const std::string& consumerId) const;
		RTC::Consumer* GetConsumerByMediaSsrc(uint32_t ssrc) const;
		RTC::Consumer* GetConsumerByRtxSsrc(uint32_t ssrc) const;
		RTC::DataProducer* GetDataProducerById(const std::string& dataProducerId) const;
		RTC::DataConsumer* GetDataConsumerById(const std::string& dataConsumerId) const;

	private:
		virtual bool IsConnected() const = 0;
		virtual void SendRtpPacket(
		  RTC::Consumer* consumer, RTC::RtpPacket* packet, onSendCallback* cb = nullptr) = 0;
		void HandleRtcpPacket(RTC::RTCP::Packet* packet);
		void SendRtcp(uint64_t nowMs);
		virtual void SendRtcpPacket(RTC::RTCP::Packet* packet)                 = 0;
		virtual void SendRtcpCompoundPacket(RTC::RTCP::CompoundPacket* packet) = 0;
		virtual void SendMessage(
		  RTC::DataConsumer* dataConsumer,
		  const uint8_t* msg,
		  size_t len,
		  uint32_t ppid,
		  onQueuedCallback* = nullptr)                             = 0;
		virtual void SendSctpData(const uint8_t* data, size_t len) = 0;
		virtual void RecvStreamClosed(uint32_t ssrc)               = 0;
		virtual void SendStreamClosed(uint32_t ssrc)               = 0;
		void DistributeAvailableOutgoingBitrate();
		void ComputeOutgoingDesiredBitrate(bool forceBitrate = false);
		void EmitTraceEventProbationType(RTC::RtpPacket* packet) const;
		void EmitTraceEventBweType(RTC::TransportCongestionControlClient::Bitrates& bitrates) const;
		void CheckNoProducer(const std::string& producerId) const;
		void CheckNoDataProducer(const std::string& dataProducerId) const;
		void CheckNoDataConsumer(const std::string& dataConsumerId) const;

		/* Pure virtual methods inherited from RTC::Producer::Listener. */
	public:
		void OnProducerReceiveData(RTC::Producer* /*producer*/, size_t len) override
		{
			this->DataReceived(len);
		}
		void OnProducerReceiveRtpPacket(RTC::Producer* /*producer*/, RTC::RtpPacket* packet) override
		{
			this->ReceiveRtpPacket(packet);
		}
		void OnProducerPaused(RTC::Producer* producer) override;
		void OnProducerResumed(RTC::Producer* producer) override;
		void OnProducerNewRtpStream(
		  RTC::Producer* producer, RTC::RtpStreamRecv* rtpStream, uint32_t mappedSsrc) override;
		void OnProducerRtpStreamScore(
		  RTC::Producer* producer,
		  RTC::RtpStreamRecv* rtpStream,
		  uint8_t score,
		  uint8_t previousScore) override;
		void OnProducerRtcpSenderReport(
		  RTC::Producer* producer, RTC::RtpStreamRecv* rtpStream, bool first) override;
		void OnProducerRtpPacketReceived(RTC::Producer* producer, RTC::RtpPacket* packet) override;
		void OnProducerSendRtcpPacket(RTC::Producer* producer, RTC::RTCP::Packet* packet) override;
		void OnProducerNeedWorstRemoteFractionLost(
		  RTC::Producer* producer, uint32_t mappedSsrc, uint8_t& worstRemoteFractionLost) override;

		/* Pure virtual methods inherited from RTC::Consumer::Listener. */
	public:
		void OnConsumerSendRtpPacket(RTC::Consumer* consumer, RTC::RtpPacket* packet) override;
		void OnConsumerRetransmitRtpPacket(RTC::Consumer* consumer, RTC::RtpPacket* packet) override;
		void OnConsumerKeyFrameRequested(RTC::Consumer* consumer, uint32_t mappedSsrc) override;
		void OnConsumerNeedBitrateChange(RTC::Consumer* consumer) override;
		void OnConsumerNeedZeroBitrate(RTC::Consumer* consumer) override;
		void OnConsumerProducerClosed(RTC::Consumer* consumer) override;

		/* Pure virtual methods inherited from RTC::DataProducer::Listener. */
	public:
		void OnDataProducerReceiveData(RTC::DataProducer* /*dataProducer*/, size_t len) override
		{
			this->DataReceived(len);
		}
		void OnDataProducerMessageReceived(
		  RTC::DataProducer* dataProducer,
		  const uint8_t* msg,
		  size_t len,
		  uint32_t ppid,
		  std::vector<uint16_t>& subchannels,
		  std::optional<uint16_t> requiredSubchannel) override;
		void OnDataProducerPaused(RTC::DataProducer* dataProducer) override;
		void OnDataProducerResumed(RTC::DataProducer* dataProducer) override;

		/* Pure virtual methods inherited from RTC::DataConsumer::Listener. */
	public:
		void OnDataConsumerSendMessage(
		  RTC::DataConsumer* dataConsumer,
		  const uint8_t* msg,
		  size_t len,
		  uint32_t ppid,
		  onQueuedCallback* cb = nullptr) override;
		void OnDataConsumerDataProducerClosed(RTC::DataConsumer* dataConsumer) override;

		/* Pure virtual methods inherited from RTC::SctpAssociation::Listener. */
	public:
		void OnSctpAssociationConnecting(RTC::SctpAssociation* sctpAssociation) override;
		void OnSctpAssociationConnected(RTC::SctpAssociation* sctpAssociation) override;
		void OnSctpAssociationFailed(RTC::SctpAssociation* sctpAssociation) override;
		void OnSctpAssociationClosed(RTC::SctpAssociation* sctpAssociation) override;
		void OnSctpAssociationSendData(
		  RTC::SctpAssociation* sctpAssociation, const uint8_t* data, size_t len) override;
		void OnSctpAssociationMessageReceived(
		  RTC::SctpAssociation* sctpAssociation,
		  uint16_t streamId,
		  const uint8_t* msg,
		  size_t len,
		  uint32_t ppid) override;
		void OnSctpAssociationBufferedAmount(
		  RTC::SctpAssociation* sctpAssociation, uint32_t bufferedAmount) override;

		/* Pure virtual methods inherited from RTC::TransportCongestionControlClient::Listener. */
	public:
		void OnTransportCongestionControlClientBitrates(
		  RTC::TransportCongestionControlClient* tccClient,
		  RTC::TransportCongestionControlClient::Bitrates& bitrates) override;
		void OnTransportCongestionControlClientSendRtpPacket(
		  RTC::TransportCongestionControlClient* tccClient,
		  RTC::RtpPacket* packet,
		  const webrtc::PacedPacketInfo& pacingInfo) override;

		/* Pure virtual methods inherited from RTC::TransportCongestionControlServer::Listener. */
	public:
		void OnTransportCongestionControlServerSendRtcpPacket(
		  RTC::TransportCongestionControlServer* tccServer, RTC::RTCP::Packet* packet) override;

#ifdef ENABLE_RTC_SENDER_BANDWIDTH_ESTIMATOR
		/* Pure virtual methods inherited from RTC::SenderBandwidthEstimator::Listener. */
	public:
		void OnSenderBandwidthEstimatorAvailableBitrate(
		  RTC::SenderBandwidthEstimator* senderBwe,
		  uint32_t availableBitrate,
		  uint32_t previousAvailableBitrate) override;
#endif

		/* Pure virtual methods inherited from TimerHandle::Listener. */
	public:
		void OnTimer(TimerHandle* timer) override;

	public:
		// Passed by argument.
		const std::string id;

	protected:
		RTC::Shared* shared{ nullptr };
		size_t maxMessageSize{ 262144u };
		// Allocated by this.
		RTC::SctpAssociation* sctpAssociation{ nullptr };

	private:
		// Passed by argument.
		Listener* listener{ nullptr };
		// Allocated by this.
		absl::flat_hash_map<std::string, RTC::Producer*> mapProducers;
		absl::flat_hash_map<std::string, RTC::Consumer*> mapConsumers;
		absl::flat_hash_map<std::string, RTC::DataProducer*> mapDataProducers;
		absl::flat_hash_map<std::string, RTC::DataConsumer*> mapDataConsumers;
		absl::flat_hash_map<uint32_t, RTC::Consumer*> mapSsrcConsumer;
		absl::flat_hash_map<uint32_t, RTC::Consumer*> mapRtxSsrcConsumer;
		TimerHandle* rtcpTimer{ nullptr };
		std::shared_ptr<RTC::TransportCongestionControlClient> tccClient{ nullptr };
		std::shared_ptr<RTC::TransportCongestionControlServer> tccServer{ nullptr };
#ifdef ENABLE_RTC_SENDER_BANDWIDTH_ESTIMATOR
		std::shared_ptr<RTC::SenderBandwidthEstimator> senderBwe{ nullptr };
#endif
		// Others.
		bool direct{ false }; // Whether this Transport allows direct communication.
		bool destroying{ false };
		struct RTC::RtpHeaderExtensionIds recvRtpHeaderExtensionIds;
		RTC::RtpListener rtpListener;
		RTC::SctpListener sctpListener;
		RTC::RateCalculator recvTransmission;
		RTC::RateCalculator sendTransmission;
		RTC::RtpDataCounter recvRtpTransmission;
		RTC::RtpDataCounter sendRtpTransmission;
		RTC::RtpDataCounter recvRtxTransmission;
		RTC::RtpDataCounter sendRtxTransmission;
		RTC::RtpDataCounter sendProbationTransmission;
		uint16_t transportWideCcSeq{ 0u };
		uint32_t initialAvailableOutgoingBitrate{ 600000u };
		uint32_t maxIncomingBitrate{ 0u };
		uint32_t maxOutgoingBitrate{ 0u };
		uint32_t minOutgoingBitrate{ 0u };
		struct TraceEventTypes traceEventTypes;
	};
} // namespace RTC

#endif
