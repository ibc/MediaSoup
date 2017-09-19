#define MS_CLASS "RTC::Transport"
// #define MS_LOG_DEV

#include "RTC/Transport.hpp"
#include "DepLibUV.hpp"
#include "Logger.hpp"
#include "Utils.hpp"
#include "RTC/Consumer.hpp"
#include "RTC/Producer.hpp"
#include "RTC/RTCP/FeedbackPsRemb.hpp"

/* Consts. */

namespace RTC
{
	/* Instance methods. */

	Transport::Transport(Listener* listener, Channel::Notifier* notifier, uint32_t transportId)
	  : transportId(transportId), listener(listener), notifier(notifier)
	{
		MS_TRACE();

		// Create the RTCP timer.
		this->rtcpTimer = new Timer(this);
	}

	Transport::~Transport()
	{
		MS_TRACE();

		// Close all the handled Producers.
		for (auto it = this->producers.begin(); it != this->producers.end();)
		{
			auto* producer = *it;

			it = this->producers.erase(it);
			producer->Destroy();
		}

		// Disable all the handled Consumers.
		for (auto* consumer : this->consumers)
		{
			consumer->Disable();

			// Remove us as listener.
			consumer->RemoveListener(this);
		}

		// Destroy the RTCP timer.
		if (this->rtcpTimer != nullptr)
			this->rtcpTimer->Destroy();
	}

	void Transport::Destroy()
	{
		MS_TRACE();

		// Notify.
		this->notifier->Emit(this->transportId, "close");

		// Notify the listener.
		this->listener->OnTransportClosed(this);

		delete this;
	}

	void Transport::HandleProducer(RTC::Producer* producer)
	{
		MS_TRACE();

		// Pass it to the RtpListener.
		// NOTE: This may throw.
		this->rtpListener.AddProducer(producer);

		// Add to the map.
		this->producers.insert(producer);

		// Add us as listener.
		producer->AddListener(this);

		// Take the transport related RTP header extension ids of the Producer
		// and add them to the Transport.

		if (producer->GetTransportHeaderExtensionIds().absSendTime != 0u)
		{
			this->headerExtensionIds.absSendTime = producer->GetTransportHeaderExtensionIds().absSendTime;
		}

		if (producer->GetTransportHeaderExtensionIds().rid != 0u)
		{
			this->headerExtensionIds.rid = producer->GetTransportHeaderExtensionIds().rid;
		}
	}

	void Transport::HandleConsumer(RTC::Consumer* consumer)
	{
		MS_TRACE();

		// Add to the map.
		this->consumers.insert(consumer);

		// Add us as listener.
		consumer->AddListener(this);

		// If we are connected, ask a key request for this enabled Consumer.
		if (IsConnected())
		{
			if (consumer->kind == RTC::Media::Kind::VIDEO)
			{
				MS_DEBUG_2TAGS(
				  rtcp, rtx, "requesting key frame for new Consumer since Transport already connected");
			}

			consumer->RequestKeyFrame(RTC::RtpEncodingParameters::Profile::ALL);
		}
	}

	void Transport::HandleRtcpPacket(RTC::RTCP::Packet* packet)
	{
		MS_TRACE();

		switch (packet->GetType())
		{
			case RTCP::Type::RR:
			{
				auto* rr = dynamic_cast<RTCP::ReceiverReportPacket*>(packet);
				auto it  = rr->Begin();

				for (; it != rr->End(); ++it)
				{
					auto& report   = (*it);
					auto* consumer = GetConsumer(report->GetSsrc());

					if (consumer == nullptr)
					{
						MS_WARN_TAG(
						  rtcp,
						  "no Consumer found for received Receiver Report [ssrc:%" PRIu32 "]",
						  report->GetSsrc());

						break;
					}

					consumer->ReceiveRtcpReceiverReport(report);
				}

				break;
			}

			case RTCP::Type::PSFB:
			{
				auto* feedback = dynamic_cast<RTCP::FeedbackPsPacket*>(packet);

				switch (feedback->GetMessageType())
				{
					case RTCP::FeedbackPs::MessageType::PLI:
					case RTCP::FeedbackPs::MessageType::FIR:
					{
						auto* consumer = GetConsumer(feedback->GetMediaSsrc());

						if (consumer == nullptr)
						{
							MS_WARN_TAG(
							  rtcp,
							  "no Consumer found for received %s Feedback packet "
							  "[sender ssrc:%" PRIu32 ", media ssrc:%" PRIu32 "]",
							  RTCP::FeedbackPsPacket::MessageType2String(feedback->GetMessageType()).c_str(),
							  feedback->GetMediaSsrc(),
							  feedback->GetMediaSsrc());

							break;
						}

						MS_DEBUG_2TAGS(
						  rtcp,
						  rtx,
						  "%s received, requesting key frame for Consumer "
						  "[sender ssrc:%" PRIu32 ", media ssrc:%" PRIu32 "]",
						  RTCP::FeedbackPsPacket::MessageType2String(feedback->GetMessageType()).c_str(),
						  feedback->GetMediaSsrc(),
						  feedback->GetMediaSsrc());

						consumer->RequestKeyFrame();

						break;
					}

					case RTCP::FeedbackPs::MessageType::AFB:
					{
						auto* afb = dynamic_cast<RTCP::FeedbackPsAfbPacket*>(feedback);

						// Ignore REMB requests.
						if (afb->GetApplication() == RTCP::FeedbackPsAfbPacket::Application::REMB)
							break;
					}

					// [[fallthrough]]; (C++17)
					case RTCP::FeedbackPs::MessageType::SLI:
					case RTCP::FeedbackPs::MessageType::RPSI:
					{
						auto* consumer = GetConsumer(feedback->GetMediaSsrc());

						if (consumer == nullptr)
						{
							MS_WARN_TAG(
							  rtcp,
							  "no Consumer found for received %s Feedback packet "
							  "[sender ssrc:%" PRIu32 ", media ssrc:%" PRIu32 "]",
							  RTCP::FeedbackPsPacket::MessageType2String(feedback->GetMessageType()).c_str(),
							  feedback->GetMediaSsrc(),
							  feedback->GetMediaSsrc());

							break;
						}

						listener->OnTransportReceiveRtcpFeedback(this, consumer, feedback);

						break;
					}

					default:
					{
						MS_WARN_TAG(
						  rtcp,
						  "ignoring unsupported %s Feedback packet "
						  "[sender ssrc:%" PRIu32 ", media ssrc:%" PRIu32 "]",
						  RTCP::FeedbackPsPacket::MessageType2String(feedback->GetMessageType()).c_str(),
						  feedback->GetMediaSsrc(),
						  feedback->GetMediaSsrc());

						break;
					}
				}

				break;
			}

			case RTCP::Type::RTPFB:
			{
				auto* feedback = dynamic_cast<RTCP::FeedbackRtpPacket*>(packet);
				auto* consumer = GetConsumer(feedback->GetMediaSsrc());

				if (consumer == nullptr)
				{
					MS_WARN_TAG(
					  rtcp,
					  "no Consumer found for received Feedback packet "
					  "[sender ssrc:%" PRIu32 ", media ssrc:%" PRIu32 "]",
					  feedback->GetMediaSsrc(),
					  feedback->GetMediaSsrc());

					break;
				}

				switch (feedback->GetMessageType())
				{
					case RTCP::FeedbackRtp::MessageType::NACK:
					{
						auto* nackPacket = dynamic_cast<RTC::RTCP::FeedbackRtpNackPacket*>(packet);

						consumer->ReceiveNack(nackPacket);

						break;
					}

					default:
					{
						MS_WARN_TAG(
						  rtcp,
						  "ignoring unsupported %s Feedback packet "
						  "[sender ssrc:%" PRIu32 ", media ssrc:%" PRIu32 "]",
						  RTCP::FeedbackRtpPacket::MessageType2String(feedback->GetMessageType()).c_str(),
						  feedback->GetMediaSsrc(),
						  feedback->GetMediaSsrc());

						break;
					}
				}

				break;
			}

			case RTCP::Type::SR:
			{
				auto* sr = dynamic_cast<RTCP::SenderReportPacket*>(packet);
				auto it  = sr->Begin();

				// Even if Sender Report packet can only contain one report..
				for (; it != sr->End(); ++it)
				{
					auto& report = (*it);
					// Get the producer associated to the SSRC indicated in the report.
					auto* producer = this->rtpListener.GetProducer(report->GetSsrc());

					if (producer == nullptr)
					{
						MS_WARN_TAG(
						  rtcp,
						  "no Producer found for received Sender Report [ssrc:%" PRIu32 "]",
						  report->GetSsrc());

						continue;
					}

					producer->ReceiveRtcpSenderReport(report);
				}

				break;
			}

			case RTCP::Type::SDES:
			{
				auto* sdes = dynamic_cast<RTCP::SdesPacket*>(packet);
				auto it    = sdes->Begin();

				for (; it != sdes->End(); ++it)
				{
					auto& chunk = (*it);
					// Get the producer associated to the SSRC indicated in the report.
					auto* producer = this->rtpListener.GetProducer(chunk->GetSsrc());

					if (producer == nullptr)
					{
						MS_WARN_TAG(
						  rtcp, "no Producer for received SDES chunk [ssrc:%" PRIu32 "]", chunk->GetSsrc());

						continue;
					}

					// TODO: Should we do something with the SDES packet?
				}

				break;
			}

			case RTCP::Type::BYE:
			{
				MS_DEBUG_TAG(rtcp, "ignoring received RTCP BYE");

				break;
			}

			default:
			{
				MS_WARN_TAG(
				  rtcp,
				  "unhandled RTCP type received [type:%" PRIu8 "]",
				  static_cast<uint8_t>(packet->GetType()));
			}
		}
	}

	void Transport::SendRtcp(uint64_t now)
	{
		MS_TRACE();

		// - Create a CompoundPacket.
		// - Request every Consumer and Producer their RTCP data.
		// - Send the CompoundPacket.

		std::unique_ptr<RTC::RTCP::CompoundPacket> packet(new RTC::RTCP::CompoundPacket());

		for (auto& consumer : this->consumers)
		{
			consumer->GetRtcp(packet.get(), now);

			// Send the RTCP compound packet if there is a sender report.
			if (packet->GetSenderReportCount() != 0u)
			{
				// Ensure that the RTCP packet fits into the RTCP buffer.
				if (packet->GetSize() > RTC::RTCP::BufferSize)
				{
					MS_WARN_TAG(rtcp, "cannot send RTCP packet, size too big (%zu bytes)", packet->GetSize());

					return;
				}

				packet->Serialize(RTC::RTCP::Buffer);
				SendRtcpCompoundPacket(packet.get());

				// Reset the Compound packet.
				packet.reset(new RTC::RTCP::CompoundPacket());
			}
		}

		for (auto& producer : this->producers)
		{
			producer->GetRtcp(packet.get(), now);
		}

		// Send the RTCP compound with all receiver reports.
		if (packet->GetReceiverReportCount() != 0u)
		{
			// Ensure that the RTCP packet fits into the RTCP buffer.
			if (packet->GetSize() > RTC::RTCP::BufferSize)
			{
				MS_WARN_TAG(rtcp, "cannot send RTCP packet, size too big (%zu bytes)", packet->GetSize());

				return;
			}

			packet->Serialize(RTC::RTCP::Buffer);
			SendRtcpCompoundPacket(packet.get());
		}
	}

	inline RTC::Consumer* Transport::GetConsumer(uint32_t ssrc) const
	{
		MS_TRACE();

		for (auto* consumer : this->consumers)
		{
			// Ignore if not enabled.
			if (!consumer->IsEnabled())
				continue;

			// NOTE: Use & since, otherwise, a full copy will be retrieved.
			auto& rtpParameters = consumer->GetParameters();

			for (auto& encoding : rtpParameters.encodings)
			{
				if (encoding.ssrc == ssrc)
					return consumer;
				if (encoding.hasFec && encoding.fec.ssrc == ssrc)
					return consumer;
				if (encoding.hasRtx && encoding.rtx.ssrc == ssrc)
					return consumer;
			}
		}

		return nullptr;
	}

	void Transport::OnProducerClosed(RTC::Producer* producer)
	{
		MS_TRACE();

		// Remove it from the map.
		this->producers.erase(producer);

		// Remove it from the RtpListener.
		this->rtpListener.RemoveProducer(producer);
	}

	void Transport::OnProducerRtpParametersUpdated(RTC::Producer* producer)
	{
		MS_TRACE();

		// Update our RtpListener.
		// NOTE: This may throw.
		this->rtpListener.AddProducer(producer);
	}

	void Transport::OnProducerPaused(RTC::Producer* /*producer*/)
	{
		// Do nothing.
	}

	void Transport::OnProducerResumed(RTC::Producer* /*producer*/)
	{
		// Do nothing.
	}

	void Transport::OnProducerRtpPacket(
	  RTC::Producer* /*producer*/,
	  RTC::RtpPacket* /*packet*/,
	  RTC::RtpEncodingParameters::Profile /*profile*/)
	{
		// Do nothing.
	}

	void Transport::OnProducerProfileEnabled(
	  RTC::Producer* /*producer*/, RTC::RtpEncodingParameters::Profile /*profile*/)
	{
		// Do nothing.
	}

	void Transport::OnProducerProfileDisabled(
	  RTC::Producer* /*producer*/, RTC::RtpEncodingParameters::Profile /*profile*/)
	{
		// Do nothing.
	}

	void Transport::OnConsumerClosed(RTC::Consumer* consumer)
	{
		MS_TRACE();

		// Remove from the map.
		this->consumers.erase(consumer);
	}

	void Transport::OnConsumerKeyFrameRequired(
	  RTC::Consumer* /*consumer*/, RTC::RtpEncodingParameters::Profile /*profile*/)
	{
		// Do nothing.
	}

	void Transport::OnTimer(Timer* timer)
	{
		if (timer == this->rtcpTimer)
		{
			uint64_t interval = RTC::RTCP::MaxVideoIntervalMs;
			uint32_t now      = DepLibUV::GetTime();

			SendRtcp(now);

			// Recalculate next RTCP interval.
			if (!this->consumers.empty())
			{
				// Transmission rate in kbps.
				uint32_t rate = 0;

				// Get the RTP sending rate.
				for (auto& consumer : this->consumers)
				{
					rate += consumer->GetTransmissionRate(now) / 1000;
				}

				// Calculate bandwidth: 360 / transmission bandwidth in kbit/s
				if (rate != 0u)
					interval = 360000 / rate;

				if (interval > RTC::RTCP::MaxVideoIntervalMs)
					interval = RTC::RTCP::MaxVideoIntervalMs;
			}

			/*
			 * The interval between RTCP packets is varied randomly over the range
			 * [0.5,1.5] times the calculated interval to avoid unintended synchronization
			 * of all participants.
			 */
			interval *= static_cast<float>(Utils::Crypto::GetRandomUInt(5, 15)) / 10;
			this->rtcpTimer->Start(interval);
		}
	}
} // namespace RTC
