#define MS_CLASS "RTC::Producer"
// #define MS_LOG_DEV_LEVEL 3

#include "RTC/Producer.hpp"
#include "DepLibUV.hpp"
#include "Logger.hpp"
#include "MediaSoupErrors.hpp"
#include "Utils.hpp"
#include "RTC/Codecs/Tools.hpp"
#include "RTC/RTCP/Feedback.hpp"
#include "RTC/RTCP/XrReceiverReferenceTime.hpp"
#include <absl/container/inlined_vector.h>
#include <cstring> // std::memcpy()

namespace RTC
{
	/* Static variables. */

	thread_local uint8_t* Producer::buffer{ nullptr };

	/* Static. */

	static constexpr unsigned int SendNackDelay{ 10u }; // In ms.

	/* Instance methods. */

	Producer::Producer(
	  RTC::Shared* shared,
	  const std::string& id,
	  RTC::Producer::Listener* listener,
	  const FBS::Transport::ProduceRequest* data)
	  : id(id), shared(shared), listener(listener), kind(RTC::Media::Kind(data->kind()))
	{
		MS_TRACE();

		// This may throw.
		this->rtpParameters = RTC::RtpParameters(data->rtpParameters());

		// Evaluate type.
		auto type = RTC::RtpParameters::GetType(this->rtpParameters);

		if (!type.has_value())
		{
			MS_THROW_TYPE_ERROR("invalid RTP parameters");
		}

		this->type = type.value();

		// Reserve a slot in rtpStreamByEncodingIdx and rtpStreamsScores vectors
		// for each RTP stream.
		this->rtpStreamByEncodingIdx.resize(this->rtpParameters.encodings.size(), nullptr);
		this->rtpStreamScores.resize(this->rtpParameters.encodings.size(), 0u);

		auto& encoding         = this->rtpParameters.encodings[0];
		const auto* mediaCodec = this->rtpParameters.GetCodecForEncoding(encoding);

		if (!RTC::Codecs::Tools::IsValidTypeForCodec(this->type, mediaCodec->mimeType))
		{
			MS_THROW_TYPE_ERROR(
			  "%s codec not supported for %s",
			  mediaCodec->mimeType.ToString().c_str(),
			  RTC::RtpParameters::GetTypeString(this->type).c_str());
		}

		for (const auto& codec : *data->rtpMapping()->codecs())
		{
			this->rtpMapping.codecs[codec->payloadType()] = codec->mappedPayloadType();
		}

		const auto* encodings = data->rtpMapping()->encodings();

		this->rtpMapping.encodings.reserve(encodings->size());

		for (const auto& encoding : *encodings)
		{
			this->rtpMapping.encodings.emplace_back();

			auto& encodingMapping = this->rtpMapping.encodings.back();

			// ssrc is optional.
			if (encoding->ssrc().has_value())
			{
				encodingMapping.ssrc = encoding->ssrc().value();
			}

			// rid is optional.
			// However ssrc or rid must be present (if more than 1 encoding).
			// clang-format off
			if (
				encodings->size() > 1 &&
				!encoding->ssrc().has_value() &&
				!flatbuffers::IsFieldPresent(encoding, FBS::RtpParameters::EncodingMapping::VT_RID)
			)
			// clang-format on
			{
				MS_THROW_TYPE_ERROR("wrong entry in rtpMapping.encodings (missing ssrc or rid)");
			}

			// If there is no mid and a single encoding, ssrc or rid must be present.
			// clang-format off
			if (
				this->rtpParameters.mid.empty() &&
				encodings->size() == 1 &&
				!encoding->ssrc().has_value() &&
				!flatbuffers::IsFieldPresent(encoding, FBS::RtpParameters::EncodingMapping::VT_RID)
			)
			// clang-format on
			{
				MS_THROW_TYPE_ERROR(
				  "wrong entry in rtpMapping.encodings (missing ssrc or rid, or rtpParameters.mid)");
			}

			// mappedSsrc is mandatory.
			if (!encoding->mappedSsrc())
			{
				MS_THROW_TYPE_ERROR("wrong entry in rtpMapping.encodings (missing mappedSsrc)");
			}

			encodingMapping.mappedSsrc = encoding->mappedSsrc();
		}

		this->paused = data->paused();

		// The number of encodings in rtpParameters must match the number of encodings
		// in rtpMapping.
		if (this->rtpParameters.encodings.size() != this->rtpMapping.encodings.size())
		{
			MS_THROW_TYPE_ERROR("rtpParameters.encodings size does not match rtpMapping.encodings size");
		}

		// Fill RTP header extension ids.
		// This may throw.
		for (auto& exten : this->rtpParameters.headerExtensions)
		{
			if (exten.id == 0u)
			{
				MS_THROW_TYPE_ERROR("RTP extension id cannot be 0");
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

			if (this->rtpHeaderExtensionIds.absSendTime == 0u && exten.type == RTC::RtpHeaderExtensionUri::Type::ABS_SEND_TIME)
			{
				this->rtpHeaderExtensionIds.absSendTime = exten.id;
			}

			if (this->rtpHeaderExtensionIds.transportWideCc01 == 0u && exten.type == RTC::RtpHeaderExtensionUri::Type::TRANSPORT_WIDE_CC_01)
			{
				this->rtpHeaderExtensionIds.transportWideCc01 = exten.id;
			}

			// NOTE: Remove this once framemarking draft becomes RFC.
			if (this->rtpHeaderExtensionIds.frameMarking07 == 0u && exten.type == RTC::RtpHeaderExtensionUri::Type::FRAME_MARKING_07)
			{
				this->rtpHeaderExtensionIds.frameMarking07 = exten.id;
			}

			if (this->rtpHeaderExtensionIds.frameMarking == 0u && exten.type == RTC::RtpHeaderExtensionUri::Type::FRAME_MARKING)
			{
				this->rtpHeaderExtensionIds.frameMarking = exten.id;
			}

			if (this->rtpHeaderExtensionIds.ssrcAudioLevel == 0u && exten.type == RTC::RtpHeaderExtensionUri::Type::SSRC_AUDIO_LEVEL)
			{
				this->rtpHeaderExtensionIds.ssrcAudioLevel = exten.id;
			}

			if (this->rtpHeaderExtensionIds.videoOrientation == 0u && exten.type == RTC::RtpHeaderExtensionUri::Type::VIDEO_ORIENTATION)
			{
				this->rtpHeaderExtensionIds.videoOrientation = exten.id;
			}

			if (this->rtpHeaderExtensionIds.toffset == 0u && exten.type == RTC::RtpHeaderExtensionUri::Type::TOFFSET)
			{
				this->rtpHeaderExtensionIds.toffset = exten.id;
			}

			if (this->rtpHeaderExtensionIds.absCaptureTime == 0u && exten.type == RTC::RtpHeaderExtensionUri::Type::ABS_CAPTURE_TIME)
			{
				this->rtpHeaderExtensionIds.absCaptureTime = exten.id;
			}

			if (this->rtpHeaderExtensionIds.playoutDelay == 0u && exten.type == RTC::RtpHeaderExtensionUri::Type::PLAYOUT_DELAY)
			{
				this->rtpHeaderExtensionIds.playoutDelay = exten.id;
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

		// Create a KeyFrameRequestManager.
		if (this->kind == RTC::Media::Kind::VIDEO)
		{
			auto keyFrameRequestDelay = data->keyFrameRequestDelay();

			this->keyFrameRequestManager = new RTC::KeyFrameRequestManager(this, keyFrameRequestDelay);
		}

		// NOTE: This may throw.
		this->shared->channelMessageRegistrator->RegisterHandler(
		  this->id,
		  /*channelRequestHandler*/ this,
		  /*channelNotificationHandler*/ this);
	}

	Producer::~Producer()
	{
		MS_TRACE();

		this->shared->channelMessageRegistrator->UnregisterHandler(this->id);

		// Delete all streams.
		for (auto& kv : this->mapSsrcRtpStream)
		{
			auto* rtpStream = kv.second;

			delete rtpStream;
		}

		this->mapSsrcRtpStream.clear();
		this->rtpStreamByEncodingIdx.clear();
		this->rtpStreamScores.clear();
		this->mapRtxSsrcRtpStream.clear();
		this->mapRtpStreamMappedSsrc.clear();
		this->mapMappedSsrcSsrc.clear();

		// Delete the KeyFrameRequestManager.
		delete this->keyFrameRequestManager;
	}

	flatbuffers::Offset<FBS::Producer::DumpResponse> Producer::FillBuffer(
	  flatbuffers::FlatBufferBuilder& builder) const
	{
		MS_TRACE();

		// Add rtpParameters.
		auto rtpParameters = this->rtpParameters.FillBuffer(builder);

		// Add rtpMapping.codecs.
		std::vector<flatbuffers::Offset<FBS::RtpParameters::CodecMapping>> codecs;

		for (const auto& kv : this->rtpMapping.codecs)
		{
			codecs.emplace_back(FBS::RtpParameters::CreateCodecMapping(builder, kv.first, kv.second));
		}

		// Add rtpMapping.encodings.
		std::vector<flatbuffers::Offset<FBS::RtpParameters::EncodingMapping>> encodings;
		encodings.reserve(this->rtpMapping.encodings.size());

		for (const auto& encodingMapping : this->rtpMapping.encodings)
		{
			encodings.emplace_back(FBS::RtpParameters::CreateEncodingMappingDirect(
			  builder,
			  encodingMapping.rid.c_str(),
			  encodingMapping.ssrc != 0u ? flatbuffers::Optional<uint32_t>(encodingMapping.ssrc)
			                             : flatbuffers::nullopt,
			  nullptr, /* capability mode. NOTE: Present in NODE*/
			  encodingMapping.mappedSsrc));
		}

		// Build rtpMapping.
		auto rtpMapping = FBS::RtpParameters::CreateRtpMappingDirect(builder, &codecs, &encodings);

		// Add rtpStreams.
		std::vector<flatbuffers::Offset<FBS::RtpStream::Dump>> rtpStreams;

		for (const auto* rtpStream : this->rtpStreamByEncodingIdx)
		{
			if (!rtpStream)
			{
				continue;
			}

			rtpStreams.emplace_back(rtpStream->FillBuffer(builder));
		}

		// Add traceEventTypes.
		std::vector<FBS::Producer::TraceEventType> traceEventTypes;

		if (this->traceEventTypes.rtp)
		{
			traceEventTypes.emplace_back(FBS::Producer::TraceEventType::RTP);
		}
		if (this->traceEventTypes.keyframe)
		{
			traceEventTypes.emplace_back(FBS::Producer::TraceEventType::KEYFRAME);
		}
		if (this->traceEventTypes.nack)
		{
			traceEventTypes.emplace_back(FBS::Producer::TraceEventType::NACK);
		}
		if (this->traceEventTypes.pli)
		{
			traceEventTypes.emplace_back(FBS::Producer::TraceEventType::PLI);
		}
		if (this->traceEventTypes.fir)
		{
			traceEventTypes.emplace_back(FBS::Producer::TraceEventType::FIR);
		}

		return FBS::Producer::CreateDumpResponseDirect(
		  builder,
		  this->id.c_str(),
		  this->kind == RTC::Media::Kind::AUDIO ? FBS::RtpParameters::MediaKind::AUDIO
		                                        : FBS::RtpParameters::MediaKind::VIDEO,
		  RTC::RtpParameters::TypeToFbs(this->type),
		  rtpParameters,
		  rtpMapping,
		  &rtpStreams,
		  &traceEventTypes,
		  this->paused);
	}

	flatbuffers::Offset<FBS::Producer::GetStatsResponse> Producer::FillBufferStats(
	  flatbuffers::FlatBufferBuilder& builder)
	{
		MS_TRACE();

		std::vector<flatbuffers::Offset<FBS::RtpStream::Stats>> rtpStreams;

		for (auto* rtpStream : this->rtpStreamByEncodingIdx)
		{
			if (!rtpStream)
			{
				continue;
			}

			rtpStreams.emplace_back(rtpStream->FillBufferStats(builder));
		}

		return FBS::Producer::CreateGetStatsResponseDirect(builder, &rtpStreams);
	}

	void Producer::HandleRequest(Channel::ChannelRequest* request)
	{
		MS_TRACE();

		switch (request->method)
		{
			case Channel::ChannelRequest::Method::PRODUCER_DUMP:
			{
				auto dumpOffset = FillBuffer(request->GetBufferBuilder());

				request->Accept(FBS::Response::Body::Producer_DumpResponse, dumpOffset);

				break;
			}

			case Channel::ChannelRequest::Method::PRODUCER_GET_STATS:
			{
				auto responseOffset = FillBufferStats(request->GetBufferBuilder());

				request->Accept(FBS::Response::Body::Producer_GetStatsResponse, responseOffset);

				break;
			}

			case Channel::ChannelRequest::Method::PRODUCER_PAUSE:
			{
				if (this->paused)
				{
					request->Accept();

					break;
				}

				// Pause all streams.
				for (auto& kv : this->mapSsrcRtpStream)
				{
					auto* rtpStream = kv.second;

					rtpStream->Pause();
				}

				this->paused = true;

				MS_DEBUG_DEV("Producer paused [producerId:%s]", this->id.c_str());

				this->listener->OnProducerPaused(this);

				request->Accept();

				break;
			}

			case Channel::ChannelRequest::Method::PRODUCER_RESUME:
			{
				if (!this->paused)
				{
					request->Accept();

					break;
				}

				// Resume all streams.
				for (auto& kv : this->mapSsrcRtpStream)
				{
					auto* rtpStream = kv.second;

					rtpStream->Resume();
				}

				this->paused = false;

				MS_DEBUG_DEV("Producer resumed [producerId:%s]", this->id.c_str());

				this->listener->OnProducerResumed(this);

				if (this->keyFrameRequestManager)
				{
					MS_DEBUG_2TAGS(rtcp, rtx, "requesting forced key frame(s) after resumed");

					// Request a key frame for all streams.
					for (auto& kv : this->mapSsrcRtpStream)
					{
						auto ssrc = kv.first;

						this->keyFrameRequestManager->ForceKeyFrameNeeded(ssrc);
					}
				}

				request->Accept();

				break;
			}

			case Channel::ChannelRequest::Method::PRODUCER_ENABLE_TRACE_EVENT:
			{
				const auto* body = request->data->body_as<FBS::Producer::EnableTraceEventRequest>();

				// Reset traceEventTypes.
				struct TraceEventTypes newTraceEventTypes;

				for (const auto& type : *body->events())
				{
					switch (type)
					{
						case FBS::Producer::TraceEventType::KEYFRAME:
						{
							newTraceEventTypes.keyframe = true;

							break;
						}
						case FBS::Producer::TraceEventType::FIR:
						{
							newTraceEventTypes.fir = true;

							break;
						}
						case FBS::Producer::TraceEventType::NACK:
						{
							newTraceEventTypes.nack = true;

							break;
						}
						case FBS::Producer::TraceEventType::PLI:
						{
							newTraceEventTypes.pli = true;

							break;
						}
						case FBS::Producer::TraceEventType::RTP:
						{
							newTraceEventTypes.rtp = true;

							break;
						}
						case FBS::Producer::TraceEventType::SR:
						{
							newTraceEventTypes.sr = true;

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

	void Producer::HandleNotification(Channel::ChannelNotification* notification)
	{
		MS_TRACE();

		switch (notification->event)
		{
			case Channel::ChannelNotification::Event::PRODUCER_SEND:
			{
				const auto* body = notification->data->body_as<FBS::Producer::SendNotification>();
				auto len         = body->data()->size();

				// Increase receive transmission.
				this->listener->OnProducerReceiveData(this, len);

				if (len > RTC::MtuSize + 100)
				{
					MS_WARN_TAG(rtp, "given RTP packet exceeds maximum size [len:%i]", len);

					break;
				}

				// If this is the first time to receive a RTP packet then allocate the
				// receiving buffer now.
				if (!Producer::buffer)
				{
					Producer::buffer = new uint8_t[RTC::MtuSize + 100];
				}

				// Copy the received packet into this buffer so it can be expanded later.
				std::memcpy(Producer::buffer, body->data()->data(), static_cast<size_t>(len));

				RTC::RtpPacket* packet = RTC::RtpPacket::Parse(Producer::buffer, len);

				if (!packet)
				{
					MS_WARN_TAG(rtp, "received data is not a valid RTP packet");

					break;
				}

				// Pass the packet to the parent transport.
				this->listener->OnProducerReceiveRtpPacket(this, packet);

				break;
			}

			default:
			{
				MS_ERROR("unknown event '%s'", notification->eventCStr);
			}
		}
	}

	Producer::ReceiveRtpPacketResult Producer::ReceiveRtpPacket(RTC::RtpPacket* packet)
	{
		MS_TRACE();

#ifdef MS_RTC_LOGGER_RTP
		packet->logger.producerId = this->id;
#endif

		// Reset current packet.
		this->currentRtpPacket = nullptr;

		// Count number of RTP streams.
		auto numRtpStreamsBefore = this->mapSsrcRtpStream.size();

		auto* rtpStream = GetRtpStream(packet);

		if (!rtpStream)
		{
			MS_WARN_TAG(rtp, "no stream found for received packet [ssrc:%" PRIu32 "]", packet->GetSsrc());

#ifdef MS_RTC_LOGGER_RTP
			packet->logger.Dropped(RtcLogger::RtpPacket::DropReason::RECV_RTP_STREAM_NOT_FOUND);
#endif

			return ReceiveRtpPacketResult::DISCARDED;
		}

		// Pre-process the packet.
		PreProcessRtpPacket(packet);

		ReceiveRtpPacketResult result;
		bool isRtx{ false };

		// Media packet.
		if (packet->GetSsrc() == rtpStream->GetSsrc())
		{
			result = ReceiveRtpPacketResult::MEDIA;

			// Process the packet.
			if (!rtpStream->ReceivePacket(packet))
			{
				// May have to announce a new RTP stream to the listener.
				if (this->mapSsrcRtpStream.size() > numRtpStreamsBefore)
				{
					NotifyNewRtpStream(rtpStream);
				}

#ifdef MS_RTC_LOGGER_RTP
				packet->logger.Dropped(RtcLogger::RtpPacket::DropReason::RECV_RTP_STREAM_DISCARDED);
#endif

				return result;
			}
		}
		// RTX packet.
		else if (packet->GetSsrc() == rtpStream->GetRtxSsrc())
		{
			result = ReceiveRtpPacketResult::RETRANSMISSION;
			isRtx  = true;

			// Process the packet.
			if (!rtpStream->ReceiveRtxPacket(packet))
			{
#ifdef MS_RTC_LOGGER_RTP
				packet->logger.Dropped(RtcLogger::RtpPacket::DropReason::RECV_RTP_STREAM_NOT_FOUND);
#endif

				return result;
			}
		}
		// Should not happen.
		else
		{
			MS_ABORT("found stream does not match received packet");
		}

		if (packet->IsKeyFrame())
		{
			MS_DEBUG_TAG(
			  rtp,
			  "key frame received [ssrc:%" PRIu32 ", seq:%" PRIu16 "]",
			  packet->GetSsrc(),
			  packet->GetSequenceNumber());

			// Tell the keyFrameRequestManager.
			if (this->keyFrameRequestManager)
			{
				this->keyFrameRequestManager->KeyFrameReceived(packet->GetSsrc());
			}
		}

		// May have to announce a new RTP stream to the listener.
		if (this->mapSsrcRtpStream.size() > numRtpStreamsBefore)
		{
			// Request a key frame for this stream since we may have lost the first packets
			// (do not do it if this is a key frame).
			if (this->keyFrameRequestManager && !this->paused && !packet->IsKeyFrame())
			{
				this->keyFrameRequestManager->ForceKeyFrameNeeded(packet->GetSsrc());
			}

			// Update current packet.
			this->currentRtpPacket = packet;

			NotifyNewRtpStream(rtpStream);

			// Reset current packet.
			this->currentRtpPacket = nullptr;
		}

		// If paused stop here.
		if (this->paused)
		{
			return result;
		}

		// May emit 'trace' event.
		EmitTraceEventRtpAndKeyFrameTypes(packet, isRtx);

		// Mangle the packet before providing the listener with it.
		if (!MangleRtpPacket(packet, rtpStream))
		{
			return ReceiveRtpPacketResult::DISCARDED;
		}

		// Post-process the packet.
		PostProcessRtpPacket(packet);

		this->listener->OnProducerRtpPacketReceived(this, packet);

		return result;
	}

	void Producer::ReceiveRtcpSenderReport(RTC::RTCP::SenderReport* report)
	{
		MS_TRACE();

		auto it = this->mapSsrcRtpStream.find(report->GetSsrc());

		if (it != this->mapSsrcRtpStream.end())
		{
			auto* rtpStream  = it->second;
			const bool first = rtpStream->GetSenderReportNtpMs() == 0;

			rtpStream->ReceiveRtcpSenderReport(report);

			this->listener->OnProducerRtcpSenderReport(this, rtpStream, first);

			EmitTraceEventSrType(report);

			return;
		}

		// If not found, check with RTX.
		auto it2 = this->mapRtxSsrcRtpStream.find(report->GetSsrc());

		if (it2 != this->mapRtxSsrcRtpStream.end())
		{
			auto* rtpStream = it2->second;

			rtpStream->ReceiveRtxRtcpSenderReport(report);

			return;
		}

		MS_DEBUG_TAG(rtcp, "RtpStream not found [ssrc:%" PRIu32 "]", report->GetSsrc());
	}

	void Producer::ReceiveRtcpXrDelaySinceLastRr(RTC::RTCP::DelaySinceLastRr::SsrcInfo* ssrcInfo)
	{
		MS_TRACE();

		auto it = this->mapSsrcRtpStream.find(ssrcInfo->GetSsrc());

		if (it == this->mapSsrcRtpStream.end())
		{
			MS_WARN_TAG(rtcp, "RtpStream not found [ssrc:%" PRIu32 "]", ssrcInfo->GetSsrc());

			return;
		}

		auto* rtpStream = it->second;

		rtpStream->ReceiveRtcpXrDelaySinceLastRr(ssrcInfo);
	}

	bool Producer::GetRtcp(RTC::RTCP::CompoundPacket* packet, uint64_t nowMs)
	{
		MS_TRACE();

		if (static_cast<float>((nowMs - this->lastRtcpSentTime) * 1.15) < this->maxRtcpInterval)
		{
			return true;
		}

		std::vector<RTCP::ReceiverReport*> receiverReports;
		RTCP::ReceiverReferenceTime* receiverReferenceTimeReport{ nullptr };

		for (auto& kv : this->mapSsrcRtpStream)
		{
			auto* rtpStream = kv.second;
			auto* report    = rtpStream->GetRtcpReceiverReport();

			receiverReports.push_back(report);

			auto* rtxReport = rtpStream->GetRtxRtcpReceiverReport();

			if (rtxReport)
			{
				receiverReports.push_back(rtxReport);
			}
		}

		// Add a receiver reference time report if no present in the packet.
		if (!packet->HasReceiverReferenceTime())
		{
			auto ntp                    = Utils::Time::TimeMs2Ntp(nowMs);
			receiverReferenceTimeReport = new RTC::RTCP::ReceiverReferenceTime();

			receiverReferenceTimeReport->SetNtpSec(ntp.seconds);
			receiverReferenceTimeReport->SetNtpFrac(ntp.fractions);
		}

		// RTCP Compound packet buffer cannot hold the data.
		if (!packet->Add(receiverReports, receiverReferenceTimeReport))
		{
			return false;
		}

		this->lastRtcpSentTime = nowMs;

		return true;
	}

	void Producer::RequestKeyFrame(uint32_t mappedSsrc)
	{
		MS_TRACE();

		if (!this->keyFrameRequestManager || this->paused)
		{
			return;
		}

		auto it = this->mapMappedSsrcSsrc.find(mappedSsrc);

		if (it == this->mapMappedSsrcSsrc.end())
		{
			MS_WARN_2TAGS(rtcp, rtx, "given mappedSsrc not found, ignoring");

			return;
		}

		const uint32_t ssrc = it->second;

		// If the current RTP packet is a key frame for the given mapped SSRC do
		// nothing since we are gonna provide Consumers with the requested key frame
		// right now.
		//
		// NOTE: We know that this may only happen before calling MangleRtpPacket()
		// so the SSRC of the packet is still the original one and not the mapped one.
		//
		// clang-format off
		if (
			this->currentRtpPacket &&
			this->currentRtpPacket->GetSsrc() == ssrc &&
			this->currentRtpPacket->IsKeyFrame()
		)
		// clang-format on
		{
			return;
		}

		this->keyFrameRequestManager->KeyFrameNeeded(ssrc);
	}

	RTC::RtpStreamRecv* Producer::GetRtpStream(RTC::RtpPacket* packet)
	{
		MS_TRACE();

		const uint32_t ssrc       = packet->GetSsrc();
		const uint8_t payloadType = packet->GetPayloadType();

		// If stream found in media ssrcs map, return it.
		{
			auto it = this->mapSsrcRtpStream.find(ssrc);

			if (it != this->mapSsrcRtpStream.end())
			{
				auto* rtpStream = it->second;

				return rtpStream;
			}
		}

		// If stream found in RTX ssrcs map, return it.
		{
			auto it = this->mapRtxSsrcRtpStream.find(ssrc);

			if (it != this->mapRtxSsrcRtpStream.end())
			{
				auto* rtpStream = it->second;

				return rtpStream;
			}
		}

		// Otherwise check our encodings and, if appropriate, create a new stream.

		// First, look for an encoding with matching media or RTX ssrc value.
		for (size_t i{ 0 }; i < this->rtpParameters.encodings.size(); ++i)
		{
			auto& encoding           = this->rtpParameters.encodings[i];
			const auto* mediaCodec   = this->rtpParameters.GetCodecForEncoding(encoding);
			const auto* rtxCodec     = this->rtpParameters.GetRtxCodecForEncoding(encoding);
			const bool isMediaPacket = (mediaCodec->payloadType == payloadType);
			const bool isRtxPacket   = (rtxCodec && rtxCodec->payloadType == payloadType);

			if (isMediaPacket && encoding.ssrc == ssrc)
			{
				auto* rtpStream = CreateRtpStream(packet, *mediaCodec, i);

				return rtpStream;
			}
			else if (isRtxPacket && encoding.hasRtx && encoding.rtx.ssrc == ssrc)
			{
				auto it = this->mapSsrcRtpStream.find(encoding.ssrc);

				// Ignore if no stream has been created yet for the corresponding encoding.
				if (it == this->mapSsrcRtpStream.end())
				{
					MS_DEBUG_2TAGS(rtp, rtx, "ignoring RTX packet for not yet created RtpStream (ssrc lookup)");

					return nullptr;
				}

				auto* rtpStream = it->second;

				// Ensure no RTX ssrc was previously detected.
				if (rtpStream->HasRtx())
				{
					MS_DEBUG_2TAGS(rtp, rtx, "ignoring RTX packet with new ssrc (ssrc lookup)");

					return nullptr;
				}

				// Update the stream RTX data.
				rtpStream->SetRtx(payloadType, ssrc);

				// Insert the new RTX ssrc into the map.
				this->mapRtxSsrcRtpStream[ssrc] = rtpStream;

				return rtpStream;
			}
		}

		// If not found, look for an encoding matching the packet RID value.
		std::string rid;

		if (packet->ReadRid(rid))
		{
			for (size_t i{ 0 }; i < this->rtpParameters.encodings.size(); ++i)
			{
				auto& encoding = this->rtpParameters.encodings[i];

				if (encoding.rid != rid)
				{
					continue;
				}

				const auto* mediaCodec   = this->rtpParameters.GetCodecForEncoding(encoding);
				const auto* rtxCodec     = this->rtpParameters.GetRtxCodecForEncoding(encoding);
				const bool isMediaPacket = (mediaCodec->payloadType == payloadType);
				const bool isRtxPacket   = (rtxCodec && rtxCodec->payloadType == payloadType);

				if (isMediaPacket)
				{
					// Ensure no other stream already exists with same RID.
					for (auto& kv : this->mapSsrcRtpStream)
					{
						auto* rtpStream = kv.second;

						if (rtpStream->GetRid() == rid)
						{
							MS_WARN_TAG(
							  rtp, "ignoring packet with unknown ssrc but already handled RID (RID lookup)");

							return nullptr;
						}
					}

					auto* rtpStream = CreateRtpStream(packet, *mediaCodec, i);

					return rtpStream;
				}
				else if (isRtxPacket)
				{
					// Ensure a stream already exists with same RID.
					for (auto& kv : this->mapSsrcRtpStream)
					{
						auto* rtpStream = kv.second;

						if (rtpStream->GetRid() == rid)
						{
							// Ensure no RTX ssrc was previously detected.
							if (rtpStream->HasRtx())
							{
								MS_DEBUG_2TAGS(rtp, rtx, "ignoring RTX packet with new SSRC (RID lookup)");

								return nullptr;
							}

							// Update the stream RTX data.
							rtpStream->SetRtx(payloadType, ssrc);

							// Insert the new RTX ssrc into the map.
							this->mapRtxSsrcRtpStream[ssrc] = rtpStream;

							return rtpStream;
						}
					}

					MS_DEBUG_2TAGS(rtp, rtx, "ignoring RTX packet for not yet created RtpStream (RID lookup)");

					return nullptr;
				}
			}

			MS_WARN_TAG(rtp, "ignoring packet with unknown RID (RID lookup)");

			return nullptr;
		}

		// If not found, and there is a single encoding without ssrc and RID, this
		// may be the media or RTX stream.
		//
		// clang-format off
		if (
			this->rtpParameters.encodings.size() == 1 &&
			!this->rtpParameters.encodings[0].ssrc &&
			this->rtpParameters.encodings[0].rid.empty()
		)
		// clang-format on
		{
			auto& encoding           = this->rtpParameters.encodings[0];
			const auto* mediaCodec   = this->rtpParameters.GetCodecForEncoding(encoding);
			const auto* rtxCodec     = this->rtpParameters.GetRtxCodecForEncoding(encoding);
			const bool isMediaPacket = (mediaCodec->payloadType == payloadType);
			const bool isRtxPacket   = (rtxCodec && rtxCodec->payloadType == payloadType);

			if (isMediaPacket)
			{
				// Ensure there is no other RTP stream already.
				if (!this->mapSsrcRtpStream.empty())
				{
					MS_WARN_TAG(
					  rtp,
					  "ignoring packet with unknown ssrc not matching the already existing stream (single RtpStream lookup)");

					return nullptr;
				}

				auto* rtpStream = CreateRtpStream(packet, *mediaCodec, 0);

				return rtpStream;
			}
			else if (isRtxPacket)
			{
				// There must be already a media RTP stream.
				auto it = this->mapSsrcRtpStream.begin();

				if (it == this->mapSsrcRtpStream.end())
				{
					MS_DEBUG_2TAGS(
					  rtp, rtx, "ignoring RTX packet for not yet created RtpStream (single stream lookup)");

					return nullptr;
				}

				auto* rtpStream = it->second;

				// Ensure no RTX SSRC was previously detected.
				if (rtpStream->HasRtx())
				{
					MS_DEBUG_2TAGS(rtp, rtx, "ignoring RTX packet with new SSRC (single stream lookup)");

					return nullptr;
				}

				// Update the stream RTX data.
				rtpStream->SetRtx(payloadType, ssrc);

				// Insert the new RTX SSRC into the map.
				this->mapRtxSsrcRtpStream[ssrc] = rtpStream;

				return rtpStream;
			}
		}

		return nullptr;
	}

	RTC::RtpStreamRecv* Producer::CreateRtpStream(
	  RTC::RtpPacket* packet, const RTC::RtpCodecParameters& mediaCodec, size_t encodingIdx)
	{
		MS_TRACE();

		const uint32_t ssrc = packet->GetSsrc();

		MS_ASSERT(
		  this->mapSsrcRtpStream.find(ssrc) == this->mapSsrcRtpStream.end(),
		  "RtpStream with given SSRC already exists");
		MS_ASSERT(
		  !this->rtpStreamByEncodingIdx[encodingIdx],
		  "RtpStream for given encoding index already exists");

		auto& encoding        = this->rtpParameters.encodings[encodingIdx];
		auto& encodingMapping = this->rtpMapping.encodings[encodingIdx];

		MS_DEBUG_TAG(
		  rtp,
		  "[encodingIdx:%zu, ssrc:%" PRIu32 ", rid:%s, payloadType:%" PRIu8 "]",
		  encodingIdx,
		  ssrc,
		  encoding.rid.c_str(),
		  mediaCodec.payloadType);

		// Set stream params.
		RTC::RtpStream::Params params;

		params.encodingIdx    = encodingIdx;
		params.ssrc           = ssrc;
		params.payloadType    = mediaCodec.payloadType;
		params.mimeType       = mediaCodec.mimeType;
		params.clockRate      = mediaCodec.clockRate;
		params.rid            = encoding.rid;
		params.cname          = this->rtpParameters.rtcp.cname;
		params.spatialLayers  = encoding.spatialLayers;
		params.temporalLayers = encoding.temporalLayers;

		// Check in band FEC in codec parameters.
		if (mediaCodec.parameters.HasInteger("useinbandfec") && mediaCodec.parameters.GetInteger("useinbandfec") == 1)
		{
			MS_DEBUG_TAG(rtcp, "in band FEC enabled");

			params.useInBandFec = true;
		}

		// Check DTX in codec parameters.
		if (mediaCodec.parameters.HasInteger("usedtx") && mediaCodec.parameters.GetInteger("usedtx") == 1)
		{
			MS_DEBUG_TAG(rtp, "DTX enabled");

			params.useDtx = true;
		}

		// Check DTX in the encoding.
		if (encoding.dtx)
		{
			MS_DEBUG_TAG(rtp, "DTX enabled");

			params.useDtx = true;
		}

		for (const auto& fb : mediaCodec.rtcpFeedback)
		{
			if (!params.useNack && fb.type == "nack" && fb.parameter.empty())
			{
				MS_DEBUG_2TAGS(rtp, rtcp, "NACK supported");

				params.useNack = true;
			}
			else if (!params.usePli && fb.type == "nack" && fb.parameter == "pli")
			{
				MS_DEBUG_2TAGS(rtp, rtcp, "PLI supported");

				params.usePli = true;
			}
			else if (!params.useFir && fb.type == "ccm" && fb.parameter == "fir")
			{
				MS_DEBUG_2TAGS(rtp, rtcp, "FIR supported");

				params.useFir = true;
			}
		}

		// Only perform RTP inactivity check on simulcast and only if there are
		// more than 1 stream.
		auto useRtpInactivityCheck =
		  this->type == RtpParameters::Type::SIMULCAST && this->rtpMapping.encodings.size() > 1;

		// Create a RtpStreamRecv for receiving a media stream.
		auto* rtpStream = new RTC::RtpStreamRecv(this, params, SendNackDelay, useRtpInactivityCheck);

		// Insert into the maps.
		this->mapSsrcRtpStream[ssrc]              = rtpStream;
		this->rtpStreamByEncodingIdx[encodingIdx] = rtpStream;
		this->rtpStreamScores[encodingIdx]        = rtpStream->GetScore();

		// Set the mapped SSRC.
		this->mapRtpStreamMappedSsrc[rtpStream]             = encodingMapping.mappedSsrc;
		this->mapMappedSsrcSsrc[encodingMapping.mappedSsrc] = ssrc;

		// If the Producer is paused tell it to the new RtpStreamRecv.
		if (this->paused)
		{
			rtpStream->Pause();
		}

		// Emit the first score event right now.
		EmitScore();

		return rtpStream;
	}

	void Producer::NotifyNewRtpStream(RTC::RtpStreamRecv* rtpStream)
	{
		MS_TRACE();

		auto mappedSsrc = this->mapRtpStreamMappedSsrc.at(rtpStream);

		// Notify the listener.
		this->listener->OnProducerNewRtpStream(this, rtpStream, mappedSsrc);
	}

	inline void Producer::PreProcessRtpPacket(RTC::RtpPacket* packet)
	{
		MS_TRACE();

		if (this->kind == RTC::Media::Kind::VIDEO)
		{
			// NOTE: Remove this once framemarking draft becomes RFC.
			packet->SetFrameMarking07ExtensionId(this->rtpHeaderExtensionIds.frameMarking07);
			packet->SetFrameMarkingExtensionId(this->rtpHeaderExtensionIds.frameMarking);
		}
	}

	inline bool Producer::MangleRtpPacket(RTC::RtpPacket* packet, RTC::RtpStreamRecv* rtpStream) const
	{
		MS_TRACE();

		// Mangle the payload type.
		{
			const uint8_t payloadType = packet->GetPayloadType();
			auto it                   = this->rtpMapping.codecs.find(payloadType);

			if (it == this->rtpMapping.codecs.end())
			{
				MS_WARN_TAG(rtp, "unknown payload type [payloadType:%" PRIu8 "]", payloadType);

				return false;
			}

			const uint8_t mappedPayloadType = it->second;

			packet->SetPayloadType(mappedPayloadType);
		}

		// Mangle the SSRC.
		{
			const uint32_t mappedSsrc = this->mapRtpStreamMappedSsrc.at(rtpStream);

			packet->SetSsrc(mappedSsrc);
		}

		// Mangle RTP header extensions.
		{
			thread_local static uint8_t buffer[4096];
			thread_local static std::vector<RTC::RtpPacket::GenericExtension> extensions;

			// This happens just once.
			if (extensions.capacity() != 24)
			{
				extensions.reserve(24);
			}

			extensions.clear();

			uint8_t* extenValue;
			uint8_t extenLen;
			uint8_t* bufferPtr{ buffer };

			// Add urn:ietf:params:rtp-hdrext:sdes:mid.
			{
				extenLen = RTC::MidMaxLength;

				extensions.emplace_back(
				  static_cast<uint8_t>(RTC::RtpHeaderExtensionUri::Type::MID), extenLen, bufferPtr);

				bufferPtr += extenLen;
			}

			// Proxy http://www.webrtc.org/experiments/rtp-hdrext/abs-capture-time.
			extenValue = packet->GetExtension(this->rtpHeaderExtensionIds.absCaptureTime, extenLen);

			if (extenValue)
			{
				std::memcpy(bufferPtr, extenValue, extenLen);

				extensions.emplace_back(
				  static_cast<uint8_t>(RTC::RtpHeaderExtensionUri::Type::ABS_CAPTURE_TIME),
				  extenLen,
				  bufferPtr);

				bufferPtr += extenLen;
			}

			// Proxy http://www.webrtc.org/experiments/rtp-hdrext/playout-delay
			extenValue = packet->GetExtension(this->rtpHeaderExtensionIds.playoutDelay, extenLen);

			if (extenValue)
			{
				std::memcpy(bufferPtr, extenValue, extenLen);

				extensions.emplace_back(
				  static_cast<uint8_t>(RTC::RtpHeaderExtensionUri::Type::PLAYOUT_DELAY), extenLen, bufferPtr);

				bufferPtr += extenLen;
			}

			if (this->kind == RTC::Media::Kind::AUDIO)
			{
				// Proxy urn:ietf:params:rtp-hdrext:ssrc-audio-level.
				extenValue = packet->GetExtension(this->rtpHeaderExtensionIds.ssrcAudioLevel, extenLen);

				if (extenValue)
				{
					std::memcpy(bufferPtr, extenValue, extenLen);

					extensions.emplace_back(
					  static_cast<uint8_t>(RTC::RtpHeaderExtensionUri::Type::SSRC_AUDIO_LEVEL),
					  extenLen,
					  bufferPtr);

					// Not needed since this is the latest added extension.
					// bufferPtr += extenLen;
				}
			}
			else if (this->kind == RTC::Media::Kind::VIDEO)
			{
				// Add http://www.webrtc.org/experiments/rtp-hdrext/abs-send-time.
				// NOTE: This is for REMB.
				{
					extenLen = 3u;

					// NOTE: Add value 0. The sending Transport will update it.
					const uint32_t absSendTime{ 0u };

					Utils::Byte::Set3Bytes(bufferPtr, 0, absSendTime);

					extensions.emplace_back(
					  static_cast<uint8_t>(RTC::RtpHeaderExtensionUri::Type::ABS_SEND_TIME), extenLen, bufferPtr);

					bufferPtr += extenLen;
				}

				// Add http://www.ietf.org/id/draft-holmer-rmcat-transport-wide-cc-extensions-01.
				// NOTE: We don't include it in outbound audio packets for now.
				{
					extenLen = 2u;

					// NOTE: Add value 0. The sending Transport will update it.
					const uint16_t wideSeqNumber{ 0u };

					Utils::Byte::Set2Bytes(bufferPtr, 0, wideSeqNumber);

					extensions.emplace_back(
					  static_cast<uint8_t>(RTC::RtpHeaderExtensionUri::Type::TRANSPORT_WIDE_CC_01),
					  extenLen,
					  bufferPtr);

					bufferPtr += extenLen;
				}

				// NOTE: Remove this once framemarking draft becomes RFC.
				// Proxy http://tools.ietf.org/html/draft-ietf-avtext-framemarking-07.
				extenValue = packet->GetExtension(this->rtpHeaderExtensionIds.frameMarking07, extenLen);

				if (extenValue)
				{
					std::memcpy(bufferPtr, extenValue, extenLen);

					extensions.emplace_back(
					  static_cast<uint8_t>(RTC::RtpHeaderExtensionUri::Type::FRAME_MARKING_07),
					  extenLen,
					  bufferPtr);

					bufferPtr += extenLen;
				}

				// Proxy urn:ietf:params:rtp-hdrext:framemarking.
				extenValue = packet->GetExtension(this->rtpHeaderExtensionIds.frameMarking, extenLen);

				if (extenValue)
				{
					std::memcpy(bufferPtr, extenValue, extenLen);

					extensions.emplace_back(
					  static_cast<uint8_t>(RTC::RtpHeaderExtensionUri::Type::FRAME_MARKING), extenLen, bufferPtr);

					bufferPtr += extenLen;
				}

				// Proxy urn:3gpp:video-orientation.
				extenValue = packet->GetExtension(this->rtpHeaderExtensionIds.videoOrientation, extenLen);

				if (extenValue)
				{
					std::memcpy(bufferPtr, extenValue, extenLen);

					extensions.emplace_back(
					  static_cast<uint8_t>(RTC::RtpHeaderExtensionUri::Type::VIDEO_ORIENTATION),
					  extenLen,
					  bufferPtr);

					bufferPtr += extenLen;
				}

				// Proxy urn:ietf:params:rtp-hdrext:toffset.
				extenValue = packet->GetExtension(this->rtpHeaderExtensionIds.toffset, extenLen);

				if (extenValue)
				{
					std::memcpy(bufferPtr, extenValue, extenLen);

					extensions.emplace_back(
					  static_cast<uint8_t>(RTC::RtpHeaderExtensionUri::Type::TOFFSET), extenLen, bufferPtr);

					// Not needed since this is the latest added extension.
					// bufferPtr += extenLen;
				}
			}

			// Set the new extensions into the packet using One-Byte format.
			packet->SetExtensions(1, extensions);

			// Assign mediasoup RTP header extension ids (just those that mediasoup may
			// be interested in after passing it to the Router).
			packet->SetMidExtensionId(static_cast<uint8_t>(RTC::RtpHeaderExtensionUri::Type::MID));
			packet->SetAbsSendTimeExtensionId(
			  static_cast<uint8_t>(RTC::RtpHeaderExtensionUri::Type::ABS_SEND_TIME));
			packet->SetTransportWideCc01ExtensionId(
			  static_cast<uint8_t>(RTC::RtpHeaderExtensionUri::Type::TRANSPORT_WIDE_CC_01));
			// NOTE: Remove this once framemarking draft becomes RFC.
			packet->SetFrameMarking07ExtensionId(
			  static_cast<uint8_t>(RTC::RtpHeaderExtensionUri::Type::FRAME_MARKING_07));
			packet->SetFrameMarkingExtensionId(
			  static_cast<uint8_t>(RTC::RtpHeaderExtensionUri::Type::FRAME_MARKING));
			packet->SetSsrcAudioLevelExtensionId(
			  static_cast<uint8_t>(RTC::RtpHeaderExtensionUri::Type::SSRC_AUDIO_LEVEL));
			packet->SetVideoOrientationExtensionId(
			  static_cast<uint8_t>(RTC::RtpHeaderExtensionUri::Type::VIDEO_ORIENTATION));
			packet->SetPlayoutDelayExtensionId(
			  static_cast<uint8_t>(RTC::RtpHeaderExtensionUri::Type::PLAYOUT_DELAY));
		}

		return true;
	}

	inline void Producer::PostProcessRtpPacket(RTC::RtpPacket* packet)
	{
		MS_TRACE();

		if (this->kind == RTC::Media::Kind::VIDEO)
		{
			bool camera;
			bool flip;
			uint16_t rotation;

			if (packet->ReadVideoOrientation(camera, flip, rotation))
			{
				// If video orientation was not yet detected or any value has changed,
				// emit event.
				// clang-format off
				if (
					!this->videoOrientationDetected ||
					camera != this->videoOrientation.camera ||
					flip != this->videoOrientation.flip ||
					rotation != this->videoOrientation.rotation
				)
				// clang-format on
				{
					this->videoOrientationDetected  = true;
					this->videoOrientation.camera   = camera;
					this->videoOrientation.flip     = flip;
					this->videoOrientation.rotation = rotation;

					auto notification = FBS::Producer::CreateVideoOrientationChangeNotification(
					  this->shared->channelNotifier->GetBufferBuilder(),
					  this->videoOrientation.camera,
					  this->videoOrientation.flip,
					  this->videoOrientation.rotation);

					this->shared->channelNotifier->Emit(
					  this->id,
					  FBS::Notification::Event::PRODUCER_VIDEO_ORIENTATION_CHANGE,
					  FBS::Notification::Body::Producer_VideoOrientationChangeNotification,
					  notification);
				}
			}
		}
	}

	inline void Producer::EmitScore() const
	{
		MS_TRACE();

		std::vector<flatbuffers::Offset<FBS::Producer::Score>> scores;

		for (const auto* rtpStream : this->rtpStreamByEncodingIdx)
		{
			if (!rtpStream)
			{
				continue;
			}

			scores.emplace_back(FBS::Producer::CreateScoreDirect(
			  this->shared->channelNotifier->GetBufferBuilder(),
			  rtpStream->GetEncodingIdx(),
			  rtpStream->GetSsrc(),
			  !rtpStream->GetRid().empty() ? rtpStream->GetRid().c_str() : nullptr,
			  rtpStream->GetScore()));
		}

		auto notification = FBS::Producer::CreateScoreNotificationDirect(
		  this->shared->channelNotifier->GetBufferBuilder(), &scores);

		this->shared->channelNotifier->Emit(
		  this->id,
		  FBS::Notification::Event::PRODUCER_SCORE,
		  FBS::Notification::Body::Producer_ScoreNotification,
		  notification);
	}

	inline void Producer::EmitTraceEventRtpAndKeyFrameTypes(RTC::RtpPacket* packet, bool isRtx) const
	{
		MS_TRACE();

		if (this->traceEventTypes.keyframe && packet->IsKeyFrame())
		{
			auto rtpPacketDump = packet->FillBuffer(this->shared->channelNotifier->GetBufferBuilder());
			auto traceInfo     = FBS::Producer::CreateKeyFrameTraceInfo(
        this->shared->channelNotifier->GetBufferBuilder(), rtpPacketDump, isRtx);

			auto notification = FBS::Producer::CreateTraceNotification(
			  this->shared->channelNotifier->GetBufferBuilder(),
			  FBS::Producer::TraceEventType::KEYFRAME,
			  DepLibUV::GetTimeMs(),
			  FBS::Common::TraceDirection::DIRECTION_IN,
			  FBS::Producer::TraceInfo::KeyFrameTraceInfo,
			  traceInfo.Union());

			EmitTraceEvent(notification);
		}
		else if (this->traceEventTypes.rtp)
		{
			auto rtpPacketDump = packet->FillBuffer(this->shared->channelNotifier->GetBufferBuilder());
			auto traceInfo     = FBS::Producer::CreateRtpTraceInfo(
        this->shared->channelNotifier->GetBufferBuilder(), rtpPacketDump, isRtx);

			auto notification = FBS::Producer::CreateTraceNotification(
			  this->shared->channelNotifier->GetBufferBuilder(),
			  FBS::Producer::TraceEventType::RTP,
			  DepLibUV::GetTimeMs(),
			  FBS::Common::TraceDirection::DIRECTION_IN,
			  FBS::Producer::TraceInfo::RtpTraceInfo,
			  traceInfo.Union());

			EmitTraceEvent(notification);
		}
	}

	inline void Producer::EmitTraceEventPliType(uint32_t ssrc) const
	{
		MS_TRACE();

		if (!this->traceEventTypes.pli)
		{
			return;
		}

		auto traceInfo =
		  FBS::Producer::CreatePliTraceInfo(this->shared->channelNotifier->GetBufferBuilder(), ssrc);

		auto notification = FBS::Producer::CreateTraceNotification(
		  this->shared->channelNotifier->GetBufferBuilder(),
		  FBS::Producer::TraceEventType::PLI,
		  DepLibUV::GetTimeMs(),
		  FBS::Common::TraceDirection::DIRECTION_OUT,
		  FBS::Producer::TraceInfo::PliTraceInfo,
		  traceInfo.Union());

		EmitTraceEvent(notification);
	}

	inline void Producer::EmitTraceEventFirType(uint32_t ssrc) const
	{
		MS_TRACE();

		if (!this->traceEventTypes.fir)
		{
			return;
		}

		auto traceInfo =
		  FBS::Producer::CreateFirTraceInfo(this->shared->channelNotifier->GetBufferBuilder(), ssrc);

		auto notification = FBS::Producer::CreateTraceNotification(
		  this->shared->channelNotifier->GetBufferBuilder(),
		  FBS::Producer::TraceEventType::FIR,
		  DepLibUV::GetTimeMs(),
		  FBS::Common::TraceDirection::DIRECTION_OUT,
		  FBS::Producer::TraceInfo::FirTraceInfo,
		  traceInfo.Union());

		EmitTraceEvent(notification);
	}

	inline void Producer::EmitTraceEventNackType() const
	{
		MS_TRACE();

		if (!this->traceEventTypes.nack)
		{
			return;
		}

		auto notification = FBS::Producer::CreateTraceNotification(
		  this->shared->channelNotifier->GetBufferBuilder(),
		  FBS::Producer::TraceEventType::NACK,
		  DepLibUV::GetTimeMs(),
		  FBS::Common::TraceDirection::DIRECTION_OUT);

		EmitTraceEvent(notification);
	}

	inline void Producer::EmitTraceEventSrType(RTC::RTCP::SenderReport* report) const
	{
		MS_TRACE();

		if (!this->traceEventTypes.sr)
		{
			return;
		}

		auto traceInfo = FBS::Producer::CreateSrTraceInfo(
		  this->shared->channelNotifier->GetBufferBuilder(),
		  report->GetSsrc(),
		  report->GetNtpSec(),
		  report->GetNtpFrac(),
		  report->GetRtpTs(),
		  report->GetPacketCount(),
		  report->GetOctetCount());

		auto notification = FBS::Producer::CreateTraceNotification(
		  this->shared->channelNotifier->GetBufferBuilder(),
		  FBS::Producer::TraceEventType::SR,
		  DepLibUV::GetTimeMs(),
		  FBS::Common::TraceDirection::DIRECTION_IN,
		  FBS::Producer::TraceInfo::SrTraceInfo,
		  traceInfo.Union());

		EmitTraceEvent(notification);
	}

	inline void Producer::EmitTraceEvent(
	  flatbuffers::Offset<FBS::Producer::TraceNotification>& notification) const
	{
		MS_TRACE();

		this->shared->channelNotifier->Emit(
		  this->id,
		  FBS::Notification::Event::PRODUCER_TRACE,
		  FBS::Notification::Body::Producer_TraceNotification,
		  notification);
	}

	inline void Producer::OnRtpStreamScore(RTC::RtpStream* rtpStream, uint8_t score, uint8_t previousScore)
	{
		MS_TRACE();

		// Update the vector of scores.
		this->rtpStreamScores[rtpStream->GetEncodingIdx()] = score;

		// Notify the listener.
		this->listener->OnProducerRtpStreamScore(
		  this, static_cast<RTC::RtpStreamRecv*>(rtpStream), score, previousScore);

		// Emit the score event.
		EmitScore();
	}

	inline void Producer::OnRtpStreamSendRtcpPacket(
	  RTC::RtpStreamRecv* /*rtpStream*/, RTC::RTCP::Packet* packet)
	{
		switch (packet->GetType())
		{
			case RTC::RTCP::Type::PSFB:
			{
				auto* feedback = static_cast<RTC::RTCP::FeedbackPsPacket*>(packet);

				switch (feedback->GetMessageType())
				{
					case RTC::RTCP::FeedbackPs::MessageType::PLI:
					{
						// May emit 'trace' event.
						EmitTraceEventPliType(feedback->GetMediaSsrc());

						break;
					}

					case RTC::RTCP::FeedbackPs::MessageType::FIR:
					{
						// May emit 'trace' event.
						EmitTraceEventFirType(feedback->GetMediaSsrc());

						break;
					}

					default:;
				}
			}

			case RTC::RTCP::Type::RTPFB:
			{
				auto* feedback = static_cast<RTC::RTCP::FeedbackRtpPacket*>(packet);

				switch (feedback->GetMessageType())
				{
					case RTC::RTCP::FeedbackRtp::MessageType::NACK:
					{
						// May emit 'trace' event.
						EmitTraceEventNackType();

						break;
					}

					default:;
				}
			}

			default:;
		}

		// Notify the listener.
		this->listener->OnProducerSendRtcpPacket(this, packet);
	}

	inline void Producer::OnRtpStreamNeedWorstRemoteFractionLost(
	  RTC::RtpStreamRecv* rtpStream, uint8_t& worstRemoteFractionLost)
	{
		auto mappedSsrc = this->mapRtpStreamMappedSsrc.at(rtpStream);

		// Notify the listener.
		this->listener->OnProducerNeedWorstRemoteFractionLost(this, mappedSsrc, worstRemoteFractionLost);
	}

	inline void Producer::OnKeyFrameNeeded(
	  RTC::KeyFrameRequestManager* /*keyFrameRequestManager*/, uint32_t ssrc)
	{
		MS_TRACE();

		auto it = this->mapSsrcRtpStream.find(ssrc);

		if (it == this->mapSsrcRtpStream.end())
		{
			MS_WARN_2TAGS(rtcp, rtx, "no associated RtpStream found [ssrc:%" PRIu32 "]", ssrc);

			return;
		}

		auto* rtpStream = it->second;

		rtpStream->RequestKeyFrame();
	}
} // namespace RTC
