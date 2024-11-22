#define MS_CLASS "RTC::Consumer"
// #define MS_LOG_DEV_LEVEL 3

#include "RTC/Consumer.hpp"
#include "DepLibUV.hpp"
#include "Logger.hpp"
#include "MediaSoupErrors.hpp"

namespace RTC
{
	/* Instance methods. */

	Consumer::Consumer(
	  RTC::Shared* shared,
	  const std::string& id,
	  const std::string& producerId,
	  Listener* listener,
	  const FBS::Transport::ConsumeRequest* data,
	  RTC::RtpParameters::Type type)
	  : id(id), producerId(producerId), shared(shared), listener(listener),
	    kind(RTC::Media::Kind(data->kind())), type(type)
	{
		MS_TRACE();

		// This may throw.
		this->rtpParameters = RTC::RtpParameters(data->rtpParameters());

		if (this->rtpParameters.encodings.empty())
		{
			MS_THROW_TYPE_ERROR("empty rtpParameters.encodings");
		}

		// All encodings must have SSRCs.
		for (auto& encoding : this->rtpParameters.encodings)
		{
			if (encoding.ssrc == 0)
			{
				MS_THROW_TYPE_ERROR("invalid encoding in rtpParameters (missing ssrc)");
			}
			else if (encoding.hasRtx && encoding.rtx.ssrc == 0)
			{
				MS_THROW_TYPE_ERROR("invalid encoding in rtpParameters (missing rtx.ssrc)");
			}
		}

		if (data->consumableRtpEncodings()->size() == 0)
		{
			MS_THROW_TYPE_ERROR("empty consumableRtpEncodings");
		}

		this->consumableRtpEncodings.reserve(data->consumableRtpEncodings()->size());

		for (size_t i{ 0 }; i < data->consumableRtpEncodings()->size(); ++i)
		{
			const auto* entry = data->consumableRtpEncodings()->Get(i);

			// This may throw due the constructor of RTC::RtpEncodingParameters.
			this->consumableRtpEncodings.emplace_back(entry);

			// Verify that it has ssrc field.
			auto& encoding = this->consumableRtpEncodings[i];

			if (encoding.ssrc == 0u)
			{
				MS_THROW_TYPE_ERROR("wrong encoding in consumableRtpEncodings (missing ssrc)");
			}
		}

		// Fill RTP header extension ids and their mapped values.
		// This may throw.
		for (auto& exten : this->rtpParameters.headerExtensions)
		{
			if (exten.id == 0u)
			{
				MS_THROW_TYPE_ERROR("RTP extension id cannot be 0");
			}

			if (this->rtpHeaderExtensionIds.ssrcAudioLevel == 0u && exten.type == RTC::RtpHeaderExtensionUri::Type::SSRC_AUDIO_LEVEL)
			{
				this->rtpHeaderExtensionIds.ssrcAudioLevel = exten.id;
			}

			if (this->rtpHeaderExtensionIds.videoOrientation == 0u && exten.type == RTC::RtpHeaderExtensionUri::Type::VIDEO_ORIENTATION)
			{
				this->rtpHeaderExtensionIds.videoOrientation = exten.id;
			}

			if (this->rtpHeaderExtensionIds.playoutDelay == 0u && exten.type == RTC::RtpHeaderExtensionUri::Type::PLAYOUT_DELAY)
			{
				this->rtpHeaderExtensionIds.playoutDelay = exten.id;
			}

			if (this->rtpHeaderExtensionIds.absSendTime == 0u && exten.type == RTC::RtpHeaderExtensionUri::Type::ABS_SEND_TIME)
			{
				this->rtpHeaderExtensionIds.absSendTime = exten.id;
			}

			if (this->rtpHeaderExtensionIds.transportWideCc01 == 0u && exten.type == RTC::RtpHeaderExtensionUri::Type::TRANSPORT_WIDE_CC_01)
			{
				this->rtpHeaderExtensionIds.transportWideCc01 = exten.id;
			}

			if (this->rtpHeaderExtensionIds.mid == 0u && exten.type == RTC::RtpHeaderExtensionUri::Type::MID)
			{
				this->rtpHeaderExtensionIds.mid = exten.id;
			}

			if (this->rtpHeaderExtensionIds.rid == 0u && exten.type == RTC::RtpHeaderExtensionUri::Type::RTP_STREAM_ID)
			{
				this->rtpHeaderExtensionIds.rid = exten.id;
			}

			if (this->rtpHeaderExtensionIds.rrid == 0u && exten.type == RTC::RtpHeaderExtensionUri::Type::REPAIRED_RTP_STREAM_ID)
			{
				this->rtpHeaderExtensionIds.rrid = exten.id;
			}
		}

		// paused is set to false by default.
		this->paused = data->paused();

		// Fill supported codec payload types.
		for (auto& codec : this->rtpParameters.codecs)
		{
			if (codec.mimeType.IsMediaCodec())
			{
				this->supportedCodecPayloadTypes[codec.payloadType] = true;
			}
		}

		// Fill media SSRCs vector.
		for (auto& encoding : this->rtpParameters.encodings)
		{
			this->mediaSsrcs.push_back(encoding.ssrc);
		}

		// Fill RTX SSRCs vector.
		for (auto& encoding : this->rtpParameters.encodings)
		{
			if (encoding.hasRtx)
			{
				this->rtxSsrcs.push_back(encoding.rtx.ssrc);
			}
		}

		// Set the RTCP report generation interval.
		if (this->kind == RTC::Media::Kind::AUDIO)
		{
			this->maxRtcpInterval = RTC::RTCP::MaxAudioIntervalMs;
		}
		else
		{
			this->maxRtcpInterval = RTC::RTCP::MaxVideoIntervalMs;
		}
	}

	Consumer::~Consumer()
	{
		MS_TRACE();
	}

	flatbuffers::Offset<FBS::Consumer::BaseConsumerDump> Consumer::FillBuffer(
	  flatbuffers::FlatBufferBuilder& builder) const
	{
		MS_TRACE();

		// Add rtpParameters.
		auto rtpParameters = this->rtpParameters.FillBuffer(builder);

		// Add consumableRtpEncodings.
		std::vector<flatbuffers::Offset<FBS::RtpParameters::RtpEncodingParameters>> consumableRtpEncodings;
		consumableRtpEncodings.reserve(this->consumableRtpEncodings.size());

		for (const auto& encoding : this->consumableRtpEncodings)
		{
			consumableRtpEncodings.emplace_back(encoding.FillBuffer(builder));
		}

		// Add supportedCodecPayloadTypes.
		std::vector<uint8_t> supportedCodecPayloadTypes;

		for (auto i = 0; i < 128; ++i)
		{
			if (this->supportedCodecPayloadTypes[i])
			{
				supportedCodecPayloadTypes.push_back(i);
			}
		}

		// Add traceEventTypes.
		std::vector<FBS::Consumer::TraceEventType> traceEventTypes;

		if (this->traceEventTypes.rtp)
		{
			traceEventTypes.emplace_back(FBS::Consumer::TraceEventType::RTP);
		}
		if (this->traceEventTypes.keyframe)
		{
			traceEventTypes.emplace_back(FBS::Consumer::TraceEventType::KEYFRAME);
		}
		if (this->traceEventTypes.nack)
		{
			traceEventTypes.emplace_back(FBS::Consumer::TraceEventType::NACK);
		}
		if (this->traceEventTypes.pli)
		{
			traceEventTypes.emplace_back(FBS::Consumer::TraceEventType::PLI);
		}
		if (this->traceEventTypes.fir)
		{
			traceEventTypes.emplace_back(FBS::Consumer::TraceEventType::FIR);
		}

		return FBS::Consumer::CreateBaseConsumerDumpDirect(
		  builder,
		  this->id.c_str(),
		  RTC::RtpParameters::TypeToFbs(this->type),
		  this->producerId.c_str(),
		  this->kind == RTC::Media::Kind::AUDIO ? FBS::RtpParameters::MediaKind::AUDIO
		                                        : FBS::RtpParameters::MediaKind::VIDEO,
		  rtpParameters,
		  &consumableRtpEncodings,
		  &supportedCodecPayloadTypes,
		  &traceEventTypes,
		  this->paused,
		  this->producerPaused,
		  this->priority);
	}

	void Consumer::HandleRequest(Channel::ChannelRequest* request)
	{
		MS_TRACE();

		switch (request->method)
		{
			case Channel::ChannelRequest::Method::CONSUMER_GET_STATS:
			{
				auto responseOffset = FillBufferStats(request->GetBufferBuilder());

				request->Accept(FBS::Response::Body::Consumer_GetStatsResponse, responseOffset);

				break;
			}

			case Channel::ChannelRequest::Method::CONSUMER_PAUSE:
			{
				if (this->paused)
				{
					request->Accept();

					break;
				}

				const bool wasActive = IsActive();

				this->paused = true;

				MS_DEBUG_DEV("Consumer paused [consumerId:%s]", this->id.c_str());

				if (wasActive)
				{
					UserOnPaused();
				}

				request->Accept();

				break;
			}

			case Channel::ChannelRequest::Method::CONSUMER_RESUME:
			{
				if (!this->paused)
				{
					request->Accept();

					break;
				}

				this->paused = false;

				MS_DEBUG_DEV("Consumer resumed [consumerId:%s]", this->id.c_str());

				if (IsActive())
				{
					UserOnResumed();
				}

				request->Accept();

				break;
			}

			case Channel::ChannelRequest::Method::CONSUMER_SET_PRIORITY:
			{
				const auto* body = request->data->body_as<FBS::Consumer::SetPriorityRequest>();

				if (body->priority() < 1u)
				{
					MS_THROW_TYPE_ERROR("wrong priority (must be higher than 0)");
				}

				this->priority = body->priority();

				auto responseOffset =
				  FBS::Consumer::CreateSetPriorityResponse(request->GetBufferBuilder(), this->priority);

				request->Accept(FBS::Response::Body::Consumer_SetPriorityResponse, responseOffset);

				break;
			}

			case Channel::ChannelRequest::Method::CONSUMER_ENABLE_TRACE_EVENT:
			{
				const auto* body = request->data->body_as<FBS::Consumer::EnableTraceEventRequest>();

				// Reset traceEventTypes.
				struct TraceEventTypes newTraceEventTypes;

				for (const auto& type : *body->events())
				{
					switch (type)
					{
						case FBS::Consumer::TraceEventType::KEYFRAME:
						{
							newTraceEventTypes.keyframe = true;

							break;
						}
						case FBS::Consumer::TraceEventType::FIR:
						{
							newTraceEventTypes.fir = true;

							break;
						}
						case FBS::Consumer::TraceEventType::NACK:
						{
							newTraceEventTypes.nack = true;

							break;
						}
						case FBS::Consumer::TraceEventType::PLI:
						{
							newTraceEventTypes.pli = true;

							break;
						}
						case FBS::Consumer::TraceEventType::RTP:
						{
							newTraceEventTypes.rtp = true;

							break;
						}
					}
				}

				this->traceEventTypes = newTraceEventTypes;

				request->Accept();

				break;
			}

			default:
			{
				MS_THROW_ERROR("unknown method '%s'", request->methodCStr);
			}
		}
	}

	void Consumer::TransportConnected()
	{
		MS_TRACE();

		if (this->transportConnected)
		{
			return;
		}

		this->transportConnected = true;

		MS_DEBUG_DEV("Transport connected [consumerId:%s]", this->id.c_str());

		UserOnTransportConnected();
	}

	void Consumer::TransportDisconnected()
	{
		MS_TRACE();

		if (!this->transportConnected)
		{
			return;
		}

		this->transportConnected = false;

		MS_DEBUG_DEV("Transport disconnected [consumerId:%s]", this->id.c_str());

		UserOnTransportDisconnected();
	}

	void Consumer::ProducerPaused()
	{
		MS_TRACE();

		if (this->producerPaused)
		{
			return;
		}

		const bool wasActive = IsActive();

		this->producerPaused = true;

		MS_DEBUG_DEV("Producer paused [consumerId:%s]", this->id.c_str());

		if (wasActive)
		{
			UserOnPaused();
		}

		this->shared->channelNotifier->Emit(this->id, FBS::Notification::Event::CONSUMER_PRODUCER_PAUSE);
	}

	void Consumer::ProducerResumed()
	{
		MS_TRACE();

		if (!this->producerPaused)
		{
			return;
		}

		this->producerPaused = false;

		MS_DEBUG_DEV("Producer resumed [consumerId:%s]", this->id.c_str());

		if (IsActive())
		{
			UserOnResumed();
		}

		this->shared->channelNotifier->Emit(this->id, FBS::Notification::Event::CONSUMER_PRODUCER_RESUME);
	}

	void Consumer::ProducerRtpStreamScores(const std::vector<uint8_t>* scores)
	{
		MS_TRACE();

		// This is gonna be a constant pointer.
		this->producerRtpStreamScores = scores;
	}

	// The caller (Router) is supposed to proceed with the deletion of this Consumer
	// right after calling this method. Otherwise ugly things may happen.
	void Consumer::ProducerClosed()
	{
		MS_TRACE();

		this->producerClosed = true;

		MS_DEBUG_DEV("Producer closed [consumerId:%s]", this->id.c_str());

		this->shared->channelNotifier->Emit(this->id, FBS::Notification::Event::CONSUMER_PRODUCER_CLOSE);

		this->listener->OnConsumerProducerClosed(this);
	}

	void Consumer::EmitTraceEventRtpAndKeyFrameTypes(RTC::RtpPacket* packet, bool isRtx) const
	{
		MS_TRACE();

		if (this->traceEventTypes.keyframe && packet->IsKeyFrame())
		{
			auto rtpPacketDump = packet->FillBuffer(this->shared->channelNotifier->GetBufferBuilder());
			auto traceInfo     = FBS::Consumer::CreateKeyFrameTraceInfo(
        this->shared->channelNotifier->GetBufferBuilder(), rtpPacketDump, isRtx);

			auto notification = FBS::Consumer::CreateTraceNotification(
			  this->shared->channelNotifier->GetBufferBuilder(),
			  FBS::Consumer::TraceEventType::KEYFRAME,
			  DepLibUV::GetTimeMs(),
			  FBS::Common::TraceDirection::DIRECTION_OUT,
			  FBS::Consumer::TraceInfo::KeyFrameTraceInfo,
			  traceInfo.Union());

			EmitTraceEvent(notification);
		}
		else if (this->traceEventTypes.rtp)
		{
			auto rtpPacketDump = packet->FillBuffer(this->shared->channelNotifier->GetBufferBuilder());
			auto traceInfo     = FBS::Consumer::CreateRtpTraceInfo(
        this->shared->channelNotifier->GetBufferBuilder(), rtpPacketDump, isRtx);

			auto notification = FBS::Consumer::CreateTraceNotification(
			  this->shared->channelNotifier->GetBufferBuilder(),
			  FBS::Consumer::TraceEventType::RTP,
			  DepLibUV::GetTimeMs(),
			  FBS::Common::TraceDirection::DIRECTION_OUT,
			  FBS::Consumer::TraceInfo::RtpTraceInfo,
			  traceInfo.Union());

			EmitTraceEvent(notification);
		}
	}

	void Consumer::EmitTraceEventPliType(uint32_t ssrc) const
	{
		MS_TRACE();

		if (!this->traceEventTypes.pli)
		{
			return;
		}

		auto traceInfo =
		  FBS::Consumer::CreatePliTraceInfo(this->shared->channelNotifier->GetBufferBuilder(), ssrc);

		auto notification = FBS::Consumer::CreateTraceNotification(
		  this->shared->channelNotifier->GetBufferBuilder(),
		  FBS::Consumer::TraceEventType::PLI,
		  DepLibUV::GetTimeMs(),
		  FBS::Common::TraceDirection::DIRECTION_IN,
		  FBS::Consumer::TraceInfo::PliTraceInfo,
		  traceInfo.Union());

		EmitTraceEvent(notification);
	}

	void Consumer::EmitTraceEventFirType(uint32_t ssrc) const
	{
		MS_TRACE();

		if (!this->traceEventTypes.fir)
		{
			return;
		}

		auto traceInfo =
		  FBS::Consumer::CreateFirTraceInfo(this->shared->channelNotifier->GetBufferBuilder(), ssrc);

		auto notification = FBS::Consumer::CreateTraceNotification(
		  this->shared->channelNotifier->GetBufferBuilder(),
		  FBS::Consumer::TraceEventType::FIR,
		  DepLibUV::GetTimeMs(),
		  FBS::Common::TraceDirection::DIRECTION_IN,
		  FBS::Consumer::TraceInfo::FirTraceInfo,
		  traceInfo.Union());

		EmitTraceEvent(notification);
	}

	void Consumer::EmitTraceEventNackType() const
	{
		MS_TRACE();

		if (!this->traceEventTypes.nack)
		{
			return;
		}

		auto notification = FBS::Consumer::CreateTraceNotification(
		  this->shared->channelNotifier->GetBufferBuilder(),
		  FBS::Consumer::TraceEventType::NACK,
		  DepLibUV::GetTimeMs(),
		  FBS::Common::TraceDirection::DIRECTION_IN);

		EmitTraceEvent(notification);
	}

	void Consumer::EmitTraceEvent(flatbuffers::Offset<FBS::Consumer::TraceNotification>& notification) const
	{
		MS_TRACE();

		this->shared->channelNotifier->Emit(
		  this->id,
		  FBS::Notification::Event::CONSUMER_TRACE,
		  FBS::Notification::Body::Consumer_TraceNotification,
		  notification);
	}
} // namespace RTC
