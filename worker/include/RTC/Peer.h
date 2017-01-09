#ifndef MS_RTC_PEER_H
#define MS_RTC_PEER_H

#include "common.h"
#include "RTC/Transport.h"
#include "RTC/RtpReceiver.h"
#include "RTC/RtpSender.h"
#include "RTC/RtpDictionaries.h"
#include "RTC/RtpPacket.h"
#include "RTC/RTCP/SenderReport.h"
#include "RTC/RTCP/ReceiverReport.h"
#include "RTC/RTCP/Feedback.h"
#include "Channel/Request.h"
#include "Channel/Notifier.h"
#include <string>
#include <unordered_map>
#include <vector>
#include <json/json.h>

namespace RTC
{
	class Peer :
		public RTC::Transport::Listener,
		public RTC::RtpReceiver::Listener,
		public RTC::RtpSender::Listener
	{
	public:
		class Listener
		{
		public:
			virtual void onPeerClosed(RTC::Peer* peer) = 0;
			virtual void onPeerCapabilities(RTC::Peer* peer, RTC::RtpCapabilities* capabilities) = 0;
			virtual void onPeerRtpReceiverParameters(RTC::Peer* peer, RTC::RtpReceiver* rtpReceiver) = 0;
			virtual void onPeerRtpReceiverClosed(RTC::Peer* peer, RTC::RtpReceiver* rtpReceiver) = 0;
			virtual void onPeerRtpSenderClosed(RTC::Peer* peer, RTC::RtpSender* rtpSender) = 0;
			virtual void onPeerRtpPacket(RTC::Peer* peer, RTC::RtpReceiver* rtpReceiver, RTC::RtpPacket* packet) = 0;
			virtual void onPeerRtcpReceiverReport(RTC::Peer* peer, RTC::RtpSender* rtpSender, RTC::RTCP::ReceiverReport* report) = 0;
			virtual void onPeerRtcpFeedback(RTC::Peer* peer, RTC::RtpSender* rtpSender, RTC::RTCP::FeedbackPsPacket* packet) = 0;
			virtual void onPeerRtcpFeedback(RTC::Peer* peer, RTC::RtpSender* rtpSender, RTC::RTCP::FeedbackRtpPacket* packet) = 0;
			virtual void onPeerRtcpSenderReport(RTC::Peer* peer, RTC::RtpReceiver* rtpReceiver, RTC::RTCP::SenderReport* report) = 0;
			virtual void onPeerRtcpSdesChunk(RTC::Peer* peer, RTC::RtpReceiver* rtpReceiver, RTC::RTCP::SdesChunk* chunk) = 0;
			virtual void onPeerRtcpCompleted(RTC::Peer* peer) = 0;
		};

	private:
		static uint8_t rtcpBuffer[];

	public:
		Peer(Listener* listener, Channel::Notifier* notifier, uint32_t peerId, std::string& peerName);
		virtual ~Peer();

		void Close();
		Json::Value toJson();
		void HandleRequest(Channel::Request* request);
		bool HasCapabilities();
		std::vector<RTC::RtpReceiver*> GetRtpReceivers();
		std::vector<RTC::RtpSender*> GetRtpSenders();
		std::unordered_map<uint32_t, RTC::Transport*>& GetTransports();
		/**
		 * Add a new RtpSender to the Peer.
		 * @param rtpSender     Instance of RtpSender.
		 * @param peerName      Name of the receiver Peer.
		 */
		void AddRtpSender(RTC::RtpSender* rtpSender, std::string& peerName, RTC::RtpParameters* rtpParameters);
		RTC::RtpSender* GetRtpSender(uint32_t ssrc);
		void SendRtcp();

	private:
		RTC::Transport* GetTransportFromRequest(Channel::Request* request, uint32_t* transportId = nullptr);
		RTC::RtpReceiver* GetRtpReceiverFromRequest(Channel::Request* request, uint32_t* rtpReceiverId = nullptr);
		RTC::RtpSender* GetRtpSenderFromRequest(Channel::Request* request, uint32_t* rtpSenderId = nullptr);

	/* Pure virtual methods inherited from RTC::Transport::Listener. */
	public:
		virtual void onTransportClosed(RTC::Transport* transport) override;
		// TODO: TMP
		virtual void onTransportRtcpPacket(RTC::Transport* transport, RTC::RTCP::Packet* packet) override;

	/* Pure virtual methods inherited from RTC::RtpReceiver::Listener. */
	public:
		virtual void onRtpReceiverParameters(RTC::RtpReceiver* rtpReceiver) override;
		virtual void onRtpReceiverParametersDone(RTC::RtpReceiver* rtpReceiver) override;
		virtual void onRtpPacket(RTC::RtpReceiver* rtpReceiver, RTC::RtpPacket* packet) override;
		virtual void onRtpReceiverClosed(RTC::RtpReceiver* rtpReceiver) override;

	/* Pure virtual methods inherited from RTC::RtpSender::Listener. */
	public:
		virtual void onRtpSenderClosed(RTC::RtpSender* rtpSender) override;

	public:
		// Passed by argument.
		uint32_t peerId;
		std::string peerName;

	private:
		// Passed by argument.
		Listener* listener = nullptr;
		Channel::Notifier* notifier = nullptr;
		// Others.
		bool hasCapabilities = false;
		RTC::RtpCapabilities capabilities;
		std::unordered_map<uint32_t, RTC::Transport*> transports;
		std::unordered_map<uint32_t, RTC::RtpReceiver*> rtpReceivers;
		std::unordered_map<uint32_t, RTC::RtpSender*> rtpSenders;
	};

	/* Inline methods. */

	inline
	bool Peer::HasCapabilities()
	{
		return this->hasCapabilities;
	}

	inline
	std::vector<RTC::RtpReceiver*> Peer::GetRtpReceivers()
	{
		std::vector<RTC::RtpReceiver*> rtpReceivers;

		for (auto it = this->rtpReceivers.begin(); it != this->rtpReceivers.end(); ++it)
		{
			rtpReceivers.push_back(it->second);
		}

		return rtpReceivers;
	}

	inline
	std::vector<RTC::RtpSender*> Peer::GetRtpSenders()
	{
		std::vector<RTC::RtpSender*> rtpSenders;

		for (auto it = this->rtpSenders.begin(); it != this->rtpSenders.end(); ++it)
		{
			rtpSenders.push_back(it->second);
		}

		return rtpSenders;
	}

	inline
	std::unordered_map<uint32_t, RTC::Transport*>& Peer::GetTransports()
	{
		return this->transports;
	}
}

#endif
