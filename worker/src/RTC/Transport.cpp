#define MS_CLASS "RTC::Transport"
// #define MS_LOG_DEV

#include "RTC/Transport.hpp"
#include "DepLibUV.hpp"
#include "Logger.hpp"
#include "MediaSoupErrors.hpp"
#include "Utils.hpp"
#include "RTC/Consumer.hpp"
#include "RTC/Producer.hpp"
#include "RTC/RTCP/FeedbackPs.hpp"
#include "RTC/RTCP/FeedbackPsAfb.hpp"
#include "RTC/RTCP/FeedbackPsRemb.hpp"
#include "RTC/RTCP/FeedbackRtp.hpp"
#include "RTC/RTCP/FeedbackRtpNack.hpp"
#include "RTC/RtpDictionaries.hpp"

namespace RTC
{
	/* Instance methods. */

	Transport::Transport(std::string& id, Listener* listener) : id(id), listener(listener)
	{
		MS_TRACE();

		// Create the RTCP timer.
		this->rtcpTimer = new Timer(this);
	}

	Transport::~Transport()
	{
		MS_TRACE();

		// The destructor must delete and clear everything silently.

		// Delete all Producers.
		for (auto& kv : this->mapProducers)
		{
			auto* producer = kv.second;

			delete producer;
		}
		this->mapProducers.clear();

		// Delete all Consumers.
		for (auto& kv : this->mapConsumers)
		{
			auto* consumer = kv.second;

			delete consumer;
		}
		this->mapConsumers.clear();
		this->mapSsrcConsumer.clear();

		// Delete the RTCP timer.
		if (this->rtcpTimer != nullptr)
			delete this->rtcpTimer;
	}

	void Transport::CloseProducersAndConsumers()
	{
		MS_TRACE();

		// This method is called by the Router and must notify him about all Producers
		// and Consumers that we are gonna close.
		//
		// The caller is supposed to delete this Transport instance after calling
		// this method.

		// Close all Producers.
		for (auto& kv : this->mapProducers)
		{
			auto* producer = kv.second;

			// Notify the listener.
			this->listener->OnTransportProducerClosed(this, producer);

			delete producer;
		}
		this->mapProducers.clear();

		// Delete all Consumers.
		for (auto& kv : this->mapConsumers)
		{
			auto* consumer = kv.second;

			// Notify the listener.
			this->listener->OnTransportConsumerClosed(this, consumer);

			delete consumer;
		}
		this->mapConsumers.clear();
		this->mapSsrcConsumer.clear();
	}

	void Router::HandleRequest(Channel::Request* request)
	{
		MS_TRACE();

		switch (request->methodId)
		{
			case Channel::Request::MethodId::TRANSPORT_SET_MAX_INCOMING_BITRATE:
			{
				static constexpr uint32_t MinBitrate{ 10000 };

				auto jsonBitrateIt = request->data.find("bitrate");

				if (jsonBitrateIt == request->data.end() || !jsonBitrateIt->is_number_unsigned())
					MS_THROW_TYPE_ERROR("missing bitrate");

				uint32_t bitrate = jsonBitrateIt->get<uint32_t>();

				if (bitrate < MinBitrate)
					bitrate = MinBitrate;

				this->maxIncomingBitrate = bitrate;

				MS_DEBUG_TAG(rbe, "Transport maximum incoming bitrate set to %" PRIu32 "bps", bitrate);

				request->Accept();

				break;
			}

			case Channel::Request::MethodId::TRANSPORT_PRODUCE:
			{
				std::string producerId;

				// This may throw.
				SetNewProducerIdFromRequest(request, producerId);

				auto jsonKindIt = request->data.find("kind");

				if (jsonKindIt == request->data.end() || !jsonKindIt->is_string())
					MS_THROW_TYPE_ERROR("missing kind");

				// This may throw.
				RTC::Media::Kind kind = RTC::Media::GetKind(jsonKindIt->get<std::string>());

				if (kind == RTC::Media::Kind::ALL)
					MS_THROW_TYPE_ERROR("invalid empty kind");

				auto jsonRtpParametersIt = request->data.find("rtpParameters");

				if (jsonRtpParametersIt == request->data.end() || !jsonRtpParametersIt->is_object())
					MS_THROW_TYPE_ERROR("missing rtpParameters");

				// This may throw.
				RTC::RtpParameters rtpParameters = RTC::RtpParameters(*jsonRtpParametersIt);

				RTC::Producer::RtpMapping rtpMapping;

				auto jsonMappingIt = request->data.find("mapping");

				if (jsonMappingIt == request->data.end() || !jsonMappingIt->is_object())
					MS_THROW_TYPE_ERROR("missing mapping");

				auto jsonCodecsIt = jsonMappingIt->find("codecs");

				if (jsonCodecsIt == jsonMappingIt.end() || !jsonCodecsIt->is_array())
					MS_THROW_TYPE_ERROR("missing mapping.codecs");

				for (auto& codec : *jsonCodecsIt)
				{
					if (!codec.is_object())
						MS_THROW_TYPE_ERROR("wrong entry in mapping.codecs");

					auto jsonPayloadTypeIt = codec.find("payloadType");

					if (jsonPayloadTypeIt == codec.end() || !jsonPayloadTypeIt->is_number_unsigned())
						MS_THROW_TYPE_ERROR("missing payloadType in entry in mapping.codecs");

					auto jsonMappedPayloadTypeIt = codec.find("mappedPayloadType");

					if (jsonMappedPayloadTypeIt == codec.end() || !jsonMappedPayloadTypeIt->is_number_unsigned())
						MS_THROW_TYPE_ERROR("missing mappedPayloadType in entry in mapping.codecs");

					rtpMapping.codecs[jsonPayloadTypeIt->get<uint8_t>()] =
					  jsonMappedPayloadTypeIt->get<uint8_t>();
				}

				auto jsonHeaderExtensionsIt = jsonMappingIt->find("headerExtensions");

				if (jsonHeaderExtensionsIt == jsonMappingIt.end() || !jsonHeaderExtensionsIt->is_array())
					MS_THROW_TYPE_ERROR("missing mapping.headerExtensions");

				for (auto& extension : *jsonHeaderExtensionsIt)
				{
					if (!extension.is_object())
						MS_THROW_TYPE_ERROR("wrong entry in mapping.headerExtensions");

					auto jsonIdIt = extension.find("id");

					if (jsonIdIt == extension.end() || !jsonIdIt->is_number_unsigned())
						MS_THROW_TYPE_ERROR("missing id in entry in mapping.headerExtensions");

					auto jsonMappedIdIt = extension.find("mappedId");

					if (jsonMappedIdIt == extension.end() || !jsonMappedIdIt->is_number_unsigned())
						MS_THROW_TYPE_ERROR("missing mappedId in entry in mapping.headerExtensions");

					rtpMapping.headerExtensions[jsonIdIt->get<uint8_t>()] = jsonMappedIdIt->get<uint8_t>();
				}

				auto jsonEncodingsIt = jsonMappingIt->find("encodings");

				if (jsonEncodingsIt == jsonMappingIt.end() || !jsonEncodingsIt->is_array())
					MS_THROW_TYPE_ERROR("missing mapping.encodings");

				for (auto& encoding : *jsonEncodingsIt)
				{
					if (!encoding.is_object())
						MS_THROW_TYPE_ERROR("wrong entry in mapping.encodings");

					RTC::Producer::RtpEncodingMapping encodingMapping;

					// ssrc is optional.
					auto jsonSsrcIt = encoding.find("ssrc");

					if (jsonSsrcIt != encoding.end() && jsonSsrcIt->is_number_unsigned())
						encodingMapping.ssrc = jsonSsrcIt->get<uint32_t>();

					// rid is optional.
					auto jsonRidIt = encoding.find("rid");

					if (jsonRidIt != encoding.end() && jsonRidIt->is_string())
						encodingMapping.rid = jsonRidIt->get<std::string>();

					// However ssrc or rid must be present.
					if (jsonSsrcIt == encoding.end() && jsonRidIt == encoding.end())
						MS_THROW_TYPE_ERROR("missing ssrc or rid in entry in mapping.encodings");

					// mappedSsrc is mandatory.
					auto jsonMappedSsrcIt = encoding.find("mappedSsrc");

					if (jsonMappedSsrcIt == encoding.end() || !jsonMappedSsrcIt->is_number_unsigned())
						MS_THROW_TYPE_ERROR("missing mappedSsrc in entry in mapping.encodings");

					encodingMapping.mappedSsrc = jsonMappedSsrcIt->get<uint32_t>();

					rtpMapping.encodings.push_back(encodingMapping);
				}

				// The number of encodings in rtpParameters must match the number of encodings
				// in rtpMapping.
				if (rtpParameters.encodings.size() != rtpMapping.encodings.size())
				{
					MS_THROW_TYPE_ERROR("rtpParameters.encodings size does not match rtpMapping.encodings size");
				}

				// This may throw.
				RTC::Producer* producer =
				  new RTC::Producer(producerId, this, kind, rtpParameters, rtpMapping);

				// Insert the Producer into the RtpListener.
				// This may throw. If so, delete the Producer and throw it out.
				try
				{
					this->rtpListener.AddProducer(producer);
				}
				catch (const MediaSoupError& error)
				{
					delete producer;

					throw;
				}

				// Take the transport related RTP header extensions of the Producer and
				// add them to the Transport.
				// NOTE: Producer::GetRtpHeaderExtensionIds() returns the original
				// header extension ids of the Producer (and not their mapped values).

				auto& producerRtpHeaderExtensionIds = producer->GetRtpHeaderExtensionIds();

				if (producerRtpHeaderExtensionIds.absSendTime != 0u)
					this->rtpHeaderExtensionIds.absSendTime = producerRtpHeaderExtensionIds.absSendTime;

				if (producerRtpHeaderExtensionIds.mid != 0u)
					this->rtpHeaderExtensionIds.mid = producerRtpHeaderExtensionIds.mid;

				if (producerRtpHeaderExtensionIds.rid != 0u)
					this->rtpHeaderExtensionIds.rid = producerRtpHeaderExtensionIds.rid;

				// Insert into the map.
				this->mapProducers[producerId] = producer;

				// Notify the listener.
				this->listener->OnTransportNewProducer(this, producer);

				MS_DEBUG_DEV("Producer created [producerId:%s]", producerId.c_str());

				request->Accept();

				break;
			}

			case Channel::Request::MethodId::TRANSPORT_CONSUME:
			{
				std::string consumerId;

				// This may throw.
				SetNewConsumerIdFromRequest(request, consumerId);

				auto jsonKindIt = request->data.find("kind");

				if (jsonKindIt == request->data.end() || !jsonKindIt->is_string())
					MS_THROW_TYPE_ERROR("missing kind");

				// This may throw.
				RTC::Media::Kind kind = RTC::Media::GetKind(jsonKindIt->get<std::string>());

				if (kind == RTC::Media::Kind::ALL)
					MS_THROW_TYPE_ERROR("invalid empty kind");

				auto jsonRtpParametersIt = request->data.find("rtpParameters");

				if (jsonRtpParametersIt == request->data.end() || !jsonRtpParametersIt->is_object())
					MS_THROW_TYPE_ERROR("missing rtpParameters");

				// This may throw.
				RTC::RtpParameters rtpParameters = RTC::RtpParameters(*jsonRtpParametersIt);

				// Get the associated Producer.
				auto jsonProducerIdIt = request->internal.find("producerId");

				if (jsonProducerIdIt == request->internal.end() || !jsonProducerIdIt->is_string())
					MS_THROW_ERROR("request has no internal.producerId");

				std::string& producerId = jsonProducerIdIt->get<std::string>();
				RTC::Producer* producer = this->listener->OnTransportGetProducer(this, producerId);

				if (producer == nullptr)
					MS_THROW_ERROR("no associated Producer found");

				// TODO: Read Producer type and create the corresponding Consumer subclass.
				// Also read producerPaused and return it in the JSON response.

				// Insert into the maps.
				this->mapConsumers[consumerId] = consumer;

				for (auto ssrc : consumer->GetMediaSsrcs())
				{
					this->mapSsrcConsumer[ssrc] = consumer;
				}

				// Notify the listener.
				this->listener->OnTransportNewConsumer(this, consumer, producer);

				MS_DEBUG_DEV(
				  "Consumer created [consumerId:%s, producerId:%s]", consumerId.c_str(), producerId.c_str());

				request->Accept();

				break;
			}

			case Channel::Request::MethodId::PRODUCER_CLOSE:
			{
				// This may throw.
				RTC::Producer* producer = GetProducerFromRequest(request);

				// Remove it from the RtpListener.
				this->rtpListener.RemoveProducer(producer);

				// Remove it from the map.
				this->mapProducers.erase(producer->id);

				// Notify the listener.
				this->listener->OnTransportProducerClosed(this, producer);

				// Delete it.
				delete producer;

				MS_DEBUG_DEV("Producer closed [id:%s]", producer->id.c_str());

				request->Accept();

				break;
			}

			case Channel::Request::MethodId::CONSUMER_CLOSE:
			{
				// This may throw.
				RTC::Consumer* consumer = GetConsumerFromRequest(request);

				// Remove it from the maps.
				this->mapConsumers.erase(consumer->id);

				for (auto ssrc : consumer->GetMediaSsrcs())
				{
					this->mapSsrcConsumer.erase(ssrc);
				}

				// Notify the listener.
				this->listener->OnTransportConsumerClosed(this, consumer);

				// Delete it.
				delete consumer;

				MS_DEBUG_DEV("Consumer closed [id:%s]", consumer->id.c_str());

				request->Accept();

				break;
			}

			case Channel::Request::MethodId::PRODUCER_DUMP:
			case Channel::Request::MethodId::PRODUCER_GET_STATS:
			case Channel::Request::MethodId::PRODUCER_PAUSE:
			case Channel::Request::MethodId::PRODUCER_RESUME:
			{
				// This may throw.
				RTC::Producer* producer = GetProducerFromRequest(request);

				producer->HandleRequest(request);

				break;
			}

			case Channel::Request::MethodId::CONSUMER_DUMP:
			case Channel::Request::MethodId::CONSUMER_GET_STATS:
			case Channel::Request::MethodId::CONSUMER_START:
			case Channel::Request::MethodId::CONSUMER_PAUSE:
			case Channel::Request::MethodId::CONSUMER_RESUME:
			case Channel::Request::MethodId::CONSUMER_SET_PREFERRED_LAYERS:
			case Channel::Request::MethodId::CONSUMER_REQUEST_KEYFRAME:
			{
				// This may throw.
				RTC::Consumer* consumer = GetConsumerFromRequest(request);

				consumer->HandleRequest(request);

				break;
			}

			default:
			{
				MS_THROW_ERROR("unknown method '%s'", request->method.c_str());
			}
		}
	}

	void Transport::Connected()
	{
		MS_TRACE();

		// Start the RTCP timer.
		this->rtcpTimer->Start(static_cast<uint64_t>(RTC::RTCP::MaxVideoIntervalMs / 2));

		// Iterate all Consumers and tell them that the Transport is connected, so they
		// will request key frames.
		for (auto& kv : this->mapConsumers)
		{
			auto* consumer = kv.second;

			consumer->TransportConnected();
		}
	}

	void Transport::Disconnected()
	{
		MS_TRACE();

		// Stop the RTCP timer.
		this->rtcpTimer->Stop();
	}

	void Transport::ReceiveRtcpPacket(RTC::RTCP::Packet* packet)
	{
		MS_TRACE();

		switch (packet->GetType())
		{
			case RTC::RTCP::Type::RR:
			{
				auto* rr = dynamic_cast<RTC::RTCP::ReceiverReportPacket*>(packet);
				auto it  = rr->Begin();

				for (; it != rr->End(); ++it)
				{
					auto& report   = (*it);
					auto* consumer = GetStartedConsumer(report->GetSsrc());

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

			case RTC::RTCP::Type::PSFB:
			{
				auto* feedback = dynamic_cast<RTC::RTCP::FeedbackPsPacket*>(packet);

				switch (feedback->GetMessageType())
				{
					case RTC::RTCP::FeedbackPs::MessageType::PLI:
					case RTC::RTCP::FeedbackPs::MessageType::FIR:
					{
						auto* consumer = GetStartedConsumer(feedback->GetMediaSsrc());

						if (consumer == nullptr)
						{
							MS_WARN_TAG(
							  rtcp,
							  "no Consumer found for received %s Feedback packet "
							  "[sender ssrc:%" PRIu32 ", media ssrc:%" PRIu32 "]",
							  RTC::RTCP::FeedbackPsPacket::MessageType2String(feedback->GetMessageType()).c_str(),
							  feedback->GetMediaSsrc(),
							  feedback->GetMediaSsrc());

							break;
						}

						MS_DEBUG_2TAGS(
						  rtcp,
						  rtx,
						  "%s received, requesting key frame for Consumer "
						  "[sender ssrc:%" PRIu32 ", media ssrc:%" PRIu32 "]",
						  RTC::RTCP::FeedbackPsPacket::MessageType2String(feedback->GetMessageType()).c_str(),
						  feedback->GetMediaSsrc(),
						  feedback->GetMediaSsrc());

						consumer->ReceiveKeyFrameRequest(feedback->GetMessageType());

						break;
					}

					case RTC::RTCP::FeedbackPs::MessageType::AFB:
					{
						auto* afb = dynamic_cast<RTC::RTCP::FeedbackPsAfbPacket*>(feedback);

						// Store REMB info.
						if (afb->GetApplication() == RTC::RTCP::FeedbackPsAfbPacket::Application::REMB)
						{
							auto* remb = dynamic_cast<RTC::RTCP::FeedbackPsRembPacket*>(afb);

							this->availableOutgoingBitrate = remb->GetBitrate();

							break;
						}
						else
						{
							MS_WARN_TAG(
							  rtcp,
							  "ignoring unsupported %s Feedback PS AFB packet "
							  "[sender ssrc:%" PRIu32 ", media ssrc:%" PRIu32 "]",
							  RTC::RTCP::FeedbackPsPacket::MessageType2String(feedback->GetMessageType()).c_str(),
							  feedback->GetMediaSsrc(),
							  feedback->GetMediaSsrc());

							break;
						}
					}

					default:
					{
						MS_WARN_TAG(
						  rtcp,
						  "ignoring unsupported %s Feedback packet "
						  "[sender ssrc:%" PRIu32 ", media ssrc:%" PRIu32 "]",
						  RTC::RTCP::FeedbackPsPacket::MessageType2String(feedback->GetMessageType()).c_str(),
						  feedback->GetMediaSsrc(),
						  feedback->GetMediaSsrc());
					}
				}

				break;
			}

			case RTC::RTCP::Type::RTPFB:
			{
				auto* feedback = dynamic_cast<RTC::RTCP::FeedbackRtpPacket*>(packet);
				auto* consumer = GetStartedConsumer(feedback->GetMediaSsrc());

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
					case RTC::RTCP::FeedbackRtp::MessageType::NACK:
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
						  RTC::RTCP::FeedbackRtpPacket::MessageType2String(feedback->GetMessageType()).c_str(),
						  feedback->GetMediaSsrc(),
						  feedback->GetMediaSsrc());
					}
				}

				break;
			}

			case RTC::RTCP::Type::SR:
			{
				auto* sr = dynamic_cast<RTC::RTCP::SenderReportPacket*>(packet);
				auto it  = sr->Begin();

				// Even if Sender Report packet can only contains one report...
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

			case RTC::RTCP::Type::SDES:
			{
				auto* sdes = dynamic_cast<RTC::RTCP::SdesPacket*>(packet);
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
				}

				break;
			}

			case RTC::RTCP::Type::BYE:
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

	void Transport::SetNewProducerIdFromRequest(Channel::Request* request, std::string& producerId) const
	{
		MS_TRACE();

		auto jsonProducerIdIt = request->internal.find("producerId");

		if (jsonProducerIdIt == request->internal.end() || !jsonProducerIdIt->is_string())
			MS_THROW_ERROR("request has no internal.producerId");

		producerId.assign(jsonProducerIdIt->get<std::string>());

		if (this->mapProducers.find(producerId) != this->mapProducers.end())
			MS_THROW_ERROR("a Producer with same producerId already exists");
	}

	RTC::Producer* Transport::GetProducerFromRequest(Channel::Request* request) const
	{
		MS_TRACE();

		auto jsonProducerIdIt = request->internal.find("producerId");

		if (jsonProducerIdIt == request->internal.end() || !jsonProducerIdIt->is_string())
			MS_THROW_ERROR("request has no internal.producerId");

		auto it = this->mapProducers.find(jsonProducerIdIt->get<std::string>());

		if (it == this->mapProducers.end())
			MS_THROW_ERROR("Producer not found");

		RTC::Producer* producer = it->second;

		return producer;
	}

	void Transport::SetNewConsumerIdFromRequest(Channel::Request* request, std::string& consumerId) const
	{
		MS_TRACE();

		auto jsonConsumerIdIt = request->internal.find("consumerId");

		if (jsonConsumerIdIt == request->internal.end() || !jsonConsumerIdIt->is_string())
			MS_THROW_ERROR("request has no internal.consumerId");

		consumerId.assign(jsonConsumerIdIt->get<std::string>());

		if (this->mapConsumers.find(consumerId) != this->mapConsumers.end())
			MS_THROW_ERROR("a Consumer with same consumerId already exists");
	}

	RTC::Consumer* Transport::GetConsumerFromRequest(Channel::Request* request) const
	{
		MS_TRACE();

		auto jsonConsumerIdIt = request->internal.find("consumerId");

		if (jsonConsumerIdIt == request->internal.end() || !jsonConsumerIdIt->is_string())
			MS_THROW_ERROR("request has no internal.consumerId");

		auto it = this->mapConsumers.find(jsonConsumerIdIt->get<std::string>());

		if (it == this->mapConsumers.end())
			MS_THROW_ERROR("Consumer not found");

		RTC::Consumer* consumer = it->second;

		return consumer;
	}

	inline RTC::Consumer* Transport::GetStartedConsumer(uint32_t ssrc) const
	{
		MS_TRACE();

		auto mapSsrcConsumerIt = this->mapSsrcConsumer.find(ssrc);

		if (mapSsrcConsumerIt == this->mapSsrcConsumer.end())
			return nullptr;

		auto* consumer = mapSsrcConsumerIt->second;

		if (consumer->IsStarted())
			return consumer;
		else
			return nullptr;
	}

	void Transport::SendRtcp(uint64_t now)
	{
		MS_TRACE();

		// - Create a CompoundPacket.
		// - Request every Consumer and Producer their RTCP data.
		// - Send the CompoundPacket.

		std::unique_ptr<RTC::RTCP::CompoundPacket> packet(new RTC::RTCP::CompoundPacket());

		for (auto& kv : this->mapConsumers)
		{
			auto* consumer = kv.second;

			consumer->GetRtcp(packet.get(), now);

			// Send the RTCP compound packet if there is a sender report.
			if (packet->HasSenderReport())
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

		for (auto& kv : this->mapProducers)
		{
			auto* producer = kv.second;

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

	inline void Transport::OnProducerPaused(RTC::Producer* producer)
	{
		this->listener->OnTransportProducerPaused(this, producer);
	}

	inline void Transport::OnProducerResumed(RTC::Producer* producer)
	{
		this->listener->OnTransportProducerResumed(this, producer);
	}

	inline void Transport::OnProducerRtpStreamHealthy(
	  RTC::Producer* producer, const RTC::RtpStream* rtpStream, uint32_t mappedSsrc)
	{
		this->listener->OnTransportProducerRtpStreamHealthy(this, producer, rtpStream, mappedSsrc);
	}

	inline void Transport::OnProducerRtpStreamUnhealthy(
	  RTC::Producer* producer, const RTC::RtpStream* rtpStream, uint32_t mappedSsrc)
	{
		this->listener->OnTransportProducerRtpStreamUnhealthy(this, producer, rtpStream, mappedSsrc);
	}

	inline void Transport::OnProducerRtpPacketReceived(RTC::Producer* producer, RTC::RtpPacket* packet)
	{
		this->listener->OnTransportProducerRtpPacketReceived(this, producer, packet);
	}

	inline void Transport::OnProducerSendRtcpPacket(RTC::Producer* /*producer*/, RTC::RTCP::Packet* packet)
	{
		SendRtcpPacket(packet);
	}

	inline void Transport::OnConsumerSendRtpPacket(RTC::Producer* /*consumer*/, RTC::Packet* packet)
	{
		SendRtpPacket(packet);
	}

	inline void Transport::OnConsumerKeyFrameRequired(RTC::Consumer* consumer, uint32_t mappedSsrc)
	{
		this->listener->OnTransportConsumerKeyFrameRequested(this, consumer, mappedSsrc);
	}

	inline void Transport::onConsumerProducerClosed(RTC::Consumer* consumer)
	{
		// Remove it from the maps.
		this->mapConsumers.erase(consumer->id);

		for (auto ssrc : consumer->GetMediaSsrcs())
		{
			this->mapSsrcConsumer.erase(ssrc);
		}

		// Notify the listener.
		this->listener->OnTransportConsumerClosed(this, consumer);

		// Delete it.
		delete consumer;
	}

	void Transport::OnTimer(Timer* timer)
	{
		if (timer == this->rtcpTimer)
		{
			uint64_t interval = static_cast<uint64_t>(RTC::RTCP::MaxVideoIntervalMs);
			uint64_t now      = DepLibUV::GetTime();

			SendRtcp(now);

			// Recalculate next RTCP interval.
			if (!this->mapConsumers.empty())
			{
				// Transmission rate in kbps.
				uint32_t rate = 0;

				// Get the RTP sending rate.
				for (auto& kv : this->mapConsumers)
				{
					auto* consumer = kv.second;

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
