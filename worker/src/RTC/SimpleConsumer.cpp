#include "FBS/consumer.h"
#define MS_CLASS "RTC::SimpleConsumer"
// #define MS_LOG_DEV_LEVEL 3

#include "DepLibUV.hpp"
#include "Logger.hpp"
#include "MediaSoupErrors.hpp"
#include "Utils.hpp"
#include "RTC/Codecs/Tools.hpp"
#include "RTC/SimpleConsumer.hpp"

namespace RTC
{
	/* Instance methods. */

	SimpleConsumer::SimpleConsumer(
	  RTC::Shared* shared,
	  const std::string& id,
	  const std::string& producerId,
	  RTC::Consumer::Listener* listener,
	  const FBS::Transport::ConsumeRequest* data)
	  : RTC::Consumer::Consumer(shared, id, producerId, listener, data, RTC::RtpParameters::Type::SIMPLE)
	{
		MS_TRACE();

		// Ensure there is a single encoding.
		if (this->consumableRtpEncodings.size() != 1u)
		{
			MS_THROW_TYPE_ERROR("invalid consumableRtpEncodings with size != 1");
		}

		auto& encoding         = this->rtpParameters.encodings[0];
		const auto* mediaCodec = this->rtpParameters.GetCodecForEncoding(encoding);

		this->keyFrameSupported = RTC::Codecs::Tools::CanBeKeyFrame(mediaCodec->mimeType);

		// Create RtpStreamSend instance for sending a single stream to the remote.
		CreateRtpStream();

		// Let's chosee an initial output seq number between 1000 and 32768 to avoid
		// libsrtp bug:
		// https://github.com/versatica/mediasoup/issues/1437
		const uint16_t initialOutputSeq =
		  Utils::Crypto::GetRandomUInt(1000u, std::numeric_limits<uint16_t>::max() / 2);

		this->rtpSeqManager.reset(new RTC::SeqManager<uint16_t>(initialOutputSeq));

		// Create the encoding context for Opus.
		if (
		  mediaCodec->mimeType.type == RTC::RtpCodecMimeType::Type::AUDIO &&
		  (mediaCodec->mimeType.subtype == RTC::RtpCodecMimeType::Subtype::OPUS ||
		   mediaCodec->mimeType.subtype == RTC::RtpCodecMimeType::Subtype::MULTIOPUS))
		{
			RTC::Codecs::EncodingContext::Params params;

			this->encodingContext.reset(
			  RTC::Codecs::Tools::GetEncodingContext(mediaCodec->mimeType, params));

			// ignoreDtx is set to false by default.
			this->encodingContext->SetIgnoreDtx(data->ignoreDtx());
		}

		// NOTE: This may throw.
		this->shared->channelMessageRegistrator->RegisterHandler(
		  this->id,
		  /*channelRequestHandler*/ this,
		  /*channelNotificationHandler*/ nullptr);
	}

	SimpleConsumer::~SimpleConsumer()
	{
		MS_TRACE();

		this->shared->channelMessageRegistrator->UnregisterHandler(this->id);

		delete this->rtpStream;
	}

	flatbuffers::Offset<FBS::Consumer::DumpResponse> SimpleConsumer::FillBuffer(
	  flatbuffers::FlatBufferBuilder& builder) const
	{
		MS_TRACE();

		// Call the parent method.
		auto base = RTC::Consumer::FillBuffer(builder);
		// Add rtpStream.
		std::vector<flatbuffers::Offset<FBS::RtpStream::Dump>> rtpStreams;
		rtpStreams.emplace_back(this->rtpStream->FillBuffer(builder));

		auto dump = FBS::Consumer::CreateConsumerDumpDirect(builder, base, &rtpStreams);

		return FBS::Consumer::CreateDumpResponse(builder, dump);
	}

	flatbuffers::Offset<FBS::Consumer::GetStatsResponse> SimpleConsumer::FillBufferStats(
	  flatbuffers::FlatBufferBuilder& builder)
	{
		MS_TRACE();

		std::vector<flatbuffers::Offset<FBS::RtpStream::Stats>> rtpStreams;

		// Add stats of our send stream.
		rtpStreams.emplace_back(this->rtpStream->FillBufferStats(builder));

		// Add stats of our recv stream.
		if (this->producerRtpStream)
		{
			rtpStreams.emplace_back(this->producerRtpStream->FillBufferStats(builder));
		}

		return FBS::Consumer::CreateGetStatsResponseDirect(builder, &rtpStreams);
	}

	flatbuffers::Offset<FBS::Consumer::ConsumerScore> SimpleConsumer::FillBufferScore(
	  flatbuffers::FlatBufferBuilder& builder) const
	{
		MS_TRACE();

		MS_ASSERT(this->producerRtpStreamScores, "producerRtpStreamScores not set");

		uint8_t producerScore{ 0 };

		if (this->producerRtpStream)
		{
			producerScore = this->producerRtpStream->GetScore();
		}

		return FBS::Consumer::CreateConsumerScoreDirect(
		  builder, this->rtpStream->GetScore(), producerScore, this->producerRtpStreamScores);
	}

	void SimpleConsumer::HandleRequest(Channel::ChannelRequest* request)
	{
		MS_TRACE();

		switch (request->method)
		{
			case Channel::ChannelRequest::Method::CONSUMER_DUMP:
			{
				auto dumpOffset = FillBuffer(request->GetBufferBuilder());

				request->Accept(FBS::Response::Body::Consumer_DumpResponse, dumpOffset);

				break;
			}

			case Channel::ChannelRequest::Method::CONSUMER_REQUEST_KEY_FRAME:
			{
				if (IsActive())
				{
					RequestKeyFrame();
				}

				request->Accept();

				break;
			}

			case Channel::ChannelRequest::Method::CONSUMER_SET_PREFERRED_LAYERS:
			{
				// Accept with empty preferred layers object.

				auto responseOffset =
				  FBS::Consumer::CreateSetPreferredLayersResponse(request->GetBufferBuilder());

				request->Accept(FBS::Response::Body::Consumer_SetPreferredLayersResponse, responseOffset);

				break;
			}

			default:
			{
				// Pass it to the parent class.
				RTC::Consumer::HandleRequest(request);
			}
		}
	}

	void SimpleConsumer::ProducerRtpStream(RTC::RtpStreamRecv* rtpStream, uint32_t /*mappedSsrc*/)
	{
		MS_TRACE();

		this->producerRtpStream = rtpStream;
	}

	void SimpleConsumer::ProducerNewRtpStream(RTC::RtpStreamRecv* rtpStream, uint32_t /*mappedSsrc*/)
	{
		MS_TRACE();

		this->producerRtpStream = rtpStream;

		// Emit the score event.
		EmitScore();
	}

	void SimpleConsumer::ProducerRtpStreamScore(
	  RTC::RtpStreamRecv* /*rtpStream*/, uint8_t /*score*/, uint8_t /*previousScore*/)
	{
		MS_TRACE();

		// Emit the score event.
		EmitScore();
	}

	void SimpleConsumer::ProducerRtcpSenderReport(RTC::RtpStreamRecv* /*rtpStream*/, bool /*first*/)
	{
		MS_TRACE();

		// Do nothing.
	}

	uint8_t SimpleConsumer::GetBitratePriority() const
	{
		MS_TRACE();

		MS_ASSERT(this->externallyManagedBitrate, "bitrate is not externally managed");

		// Audio SimpleConsumer does not play the BWE game.
		if (this->kind != RTC::Media::Kind::VIDEO)
		{
			return 0u;
		}

		if (!IsActive())
		{
			return 0u;
		}

		return this->priority;
	}

	uint32_t SimpleConsumer::IncreaseLayer(uint32_t bitrate, bool /*considerLoss*/)
	{
		MS_TRACE();

		MS_ASSERT(this->externallyManagedBitrate, "bitrate is not externally managed");
		MS_ASSERT(this->kind == RTC::Media::Kind::VIDEO, "should be video");
		MS_ASSERT(IsActive(), "should be active");

		// If this is not the first time this method is called within the same iteration,
		// return 0 since a video SimpleConsumer does not keep state about this.
		if (this->managingBitrate)
		{
			return 0u;
		}

		this->managingBitrate = true;

		// Video SimpleConsumer does not really play the BWE game when. However, let's
		// be honest and try to be nice.
		auto nowMs          = DepLibUV::GetTimeMs();
		auto desiredBitrate = this->producerRtpStream->GetBitrate(nowMs);

		if (desiredBitrate < bitrate)
		{
			return desiredBitrate;
		}
		else
		{
			return bitrate;
		}
	}

	void SimpleConsumer::ApplyLayers()
	{
		MS_TRACE();

		MS_ASSERT(this->externallyManagedBitrate, "bitrate is not externally managed");
		MS_ASSERT(this->kind == RTC::Media::Kind::VIDEO, "should be video");
		MS_ASSERT(IsActive(), "should be active");

		this->managingBitrate = false;

		// SimpleConsumer does not play the BWE game (even if video kind).
	}

	uint32_t SimpleConsumer::GetDesiredBitrate() const
	{
		MS_TRACE();

		MS_ASSERT(this->externallyManagedBitrate, "bitrate is not externally managed");

		// Audio SimpleConsumer does not play the BWE game.
		if (this->kind != RTC::Media::Kind::VIDEO)
		{
			return 0u;
		}

		if (!IsActive())
		{
			return 0u;
		}

		auto nowMs          = DepLibUV::GetTimeMs();
		auto desiredBitrate = this->producerRtpStream->GetBitrate(nowMs);

		// If consumer.rtpParameters.encodings[0].maxBitrate was given and it's
		// greater than computed one, then use it.
		auto maxBitrate = this->rtpParameters.encodings[0].maxBitrate;

		if (maxBitrate > desiredBitrate)
		{
			desiredBitrate = maxBitrate;
		}

		return desiredBitrate;
	}

	void SimpleConsumer::SendRtpPacket(RTC::RtpPacket* packet, std::shared_ptr<RTC::RtpPacket>& sharedPacket)
	{
		MS_TRACE();

#ifdef MS_RTC_LOGGER_RTP
		packet->logger.consumerId = this->id;
#endif

		if (!IsActive())
		{
#ifdef MS_RTC_LOGGER_RTP
			packet->logger.Dropped(RtcLogger::RtpPacket::DropReason::CONSUMER_INACTIVE);
#endif

			return;
		}

		auto payloadType = packet->GetPayloadType();

		// NOTE: This may happen if this Consumer supports just some codecs of those
		// in the corresponding Producer.
		if (!this->supportedCodecPayloadTypes[payloadType])
		{
			MS_DEBUG_DEV("payload type not supported [payloadType:%" PRIu8 "]", payloadType);

#ifdef MS_RTC_LOGGER_RTP
			packet->logger.Dropped(RtcLogger::RtpPacket::DropReason::UNSUPPORTED_PAYLOAD_TYPE);
#endif

			return;
		}

		bool marker;

		// Process the payload if needed. Drop packet if necessary.
		if (this->encodingContext && !packet->ProcessPayload(this->encodingContext.get(), marker))
		{
			MS_DEBUG_DEV(
			  "discarding packet [ssrc:%" PRIu32 ", seq:%" PRIu16 ", ts:%" PRIu32 "]",
			  packet->GetSsrc(),
			  packet->GetSequenceNumber(),
			  packet->GetTimestamp());

			this->rtpSeqManager->Drop(packet->GetSequenceNumber());

#ifdef MS_RTC_LOGGER_RTP
			packet->logger.Dropped(RtcLogger::RtpPacket::DropReason::DROPPED_BY_CODEC);
#endif

			return;
		}

		// If we need to sync, support key frames and this is not a key frame, ignore
		// the packet.
		if (this->syncRequired && this->keyFrameSupported && !packet->IsKeyFrame())
		{
#ifdef MS_RTC_LOGGER_RTP
			packet->logger.Dropped(RtcLogger::RtpPacket::DropReason::NOT_A_KEYFRAME);
#endif

			return;
		}

		// Packets with only padding are not forwarded.
		if (packet->GetPayloadLength() == 0)
		{
			this->rtpSeqManager->Drop(packet->GetSequenceNumber());

#ifdef MS_RTC_LOGGER_RTP
			packet->logger.Dropped(RtcLogger::RtpPacket::DropReason::EMPTY_PAYLOAD);
#endif

			return;
		}

		// Whether this is the first packet after re-sync.
		const bool isSyncPacket = this->syncRequired;

		// Sync sequence number and timestamp if required.
		if (isSyncPacket)
		{
			if (packet->IsKeyFrame())
			{
				MS_DEBUG_TAG(rtp, "sync key frame received");
			}

			this->rtpSeqManager->Sync(packet->GetSequenceNumber() - 1);

			this->syncRequired = false;
		}

		// Update RTP seq number and timestamp.
		uint16_t seq;

		this->rtpSeqManager->Input(packet->GetSequenceNumber(), seq);

		// Save original packet fields.
		auto origSsrc = packet->GetSsrc();
		auto origSeq  = packet->GetSequenceNumber();

		// Rewrite packet.
		packet->SetSsrc(this->rtpParameters.encodings[0].ssrc);
		packet->SetSequenceNumber(seq);

#ifdef MS_RTC_LOGGER_RTP
		packet->logger.sendRtpTimestamp = packet->GetTimestamp();
		packet->logger.sendSeqNumber    = seq;
#endif

		if (isSyncPacket)
		{
			MS_DEBUG_TAG(
			  rtp,
			  "sending sync packet [ssrc:%" PRIu32 ", seq:%" PRIu16 ", ts:%" PRIu32
			  "] from original [seq:%" PRIu16 "]",
			  packet->GetSsrc(),
			  packet->GetSequenceNumber(),
			  packet->GetTimestamp(),
			  origSeq);
		}

		// Process the packet.
		if (this->rtpStream->ReceivePacket(packet, sharedPacket))
		{
			// Send the packet.
			this->listener->OnConsumerSendRtpPacket(this, packet);

			// May emit 'trace' event.
			EmitTraceEventRtpAndKeyFrameTypes(packet);
		}
		else
		{
			MS_WARN_TAG(
			  rtp,
			  "failed to send packet [ssrc:%" PRIu32 ", seq:%" PRIu16 ", ts:%" PRIu32
			  "] from original [seq:%" PRIu16 "]",
			  packet->GetSsrc(),
			  packet->GetSequenceNumber(),
			  packet->GetTimestamp(),
			  origSeq);
		}

		// Restore packet fields.
		packet->SetSsrc(origSsrc);
		packet->SetSequenceNumber(origSeq);
	}

	bool SimpleConsumer::GetRtcp(RTC::RTCP::CompoundPacket* packet, uint64_t nowMs)
	{
		MS_TRACE();

		if (static_cast<float>((nowMs - this->lastRtcpSentTime) * 1.15) < this->maxRtcpInterval)
		{
			return true;
		}

		auto* senderReport = this->rtpStream->GetRtcpSenderReport(nowMs);

		if (!senderReport)
		{
			return true;
		}

		// Build SDES chunk for this sender.
		auto* sdesChunk = this->rtpStream->GetRtcpSdesChunk();

		auto* delaySinceLastRrSsrcInfo = this->rtpStream->GetRtcpXrDelaySinceLastRrSsrcInfo(nowMs);

		// RTCP Compound packet buffer cannot hold the data.
		if (!packet->Add(senderReport, sdesChunk, delaySinceLastRrSsrcInfo))
		{
			return false;
		}

		this->lastRtcpSentTime = nowMs;

		return true;
	}

	void SimpleConsumer::NeedWorstRemoteFractionLost(
	  uint32_t /*mappedSsrc*/, uint8_t& worstRemoteFractionLost)
	{
		MS_TRACE();

		if (!IsActive())
		{
			return;
		}

		auto fractionLost = this->rtpStream->GetFractionLost();

		// If our fraction lost is worse than the given one, update it.
		if (fractionLost > worstRemoteFractionLost)
		{
			worstRemoteFractionLost = fractionLost;
		}
	}

	void SimpleConsumer::ReceiveNack(RTC::RTCP::FeedbackRtpNackPacket* nackPacket)
	{
		MS_TRACE();

		if (!IsActive())
		{
			return;
		}

		// May emit 'trace' event.
		EmitTraceEventNackType();

		this->rtpStream->ReceiveNack(nackPacket);
	}

	void SimpleConsumer::ReceiveKeyFrameRequest(
	  RTC::RTCP::FeedbackPs::MessageType messageType, uint32_t ssrc)
	{
		MS_TRACE();

		switch (messageType)
		{
			case RTC::RTCP::FeedbackPs::MessageType::PLI:
			{
				EmitTraceEventPliType(ssrc);

				break;
			}

			case RTC::RTCP::FeedbackPs::MessageType::FIR:
			{
				EmitTraceEventFirType(ssrc);

				break;
			}

			default:;
		}

		this->rtpStream->ReceiveKeyFrameRequest(messageType);

		if (IsActive())
		{
			RequestKeyFrame();
		}
	}

	void SimpleConsumer::ReceiveRtcpReceiverReport(RTC::RTCP::ReceiverReport* report)
	{
		MS_TRACE();

		this->rtpStream->ReceiveRtcpReceiverReport(report);
	}

	void SimpleConsumer::ReceiveRtcpXrReceiverReferenceTime(RTC::RTCP::ReceiverReferenceTime* report)
	{
		MS_TRACE();

		this->rtpStream->ReceiveRtcpXrReceiverReferenceTime(report);
	}

	uint32_t SimpleConsumer::GetTransmissionRate(uint64_t nowMs)
	{
		MS_TRACE();

		if (!IsActive())
		{
			return 0u;
		}

		return this->rtpStream->GetBitrate(nowMs);
	}

	float SimpleConsumer::GetRtt() const
	{
		MS_TRACE();

		return this->rtpStream->GetRtt();
	}

	void SimpleConsumer::UserOnTransportConnected()
	{
		MS_TRACE();

		this->syncRequired = true;

		if (IsActive())
		{
			RequestKeyFrame();
		}
	}

	void SimpleConsumer::UserOnTransportDisconnected()
	{
		MS_TRACE();

		this->rtpStream->Pause();
	}

	void SimpleConsumer::UserOnPaused()
	{
		MS_TRACE();

		this->rtpStream->Pause();

		if (this->externallyManagedBitrate && this->kind == RTC::Media::Kind::VIDEO)
		{
			this->listener->OnConsumerNeedZeroBitrate(this);
		}
	}

	void SimpleConsumer::UserOnResumed()
	{
		MS_TRACE();

		this->syncRequired = true;

		if (IsActive())
		{
			RequestKeyFrame();
		}
	}

	void SimpleConsumer::CreateRtpStream()
	{
		MS_TRACE();

		auto& encoding         = this->rtpParameters.encodings[0];
		const auto* mediaCodec = this->rtpParameters.GetCodecForEncoding(encoding);

		MS_DEBUG_TAG(
		  rtp, "[ssrc:%" PRIu32 ", payloadType:%" PRIu8 "]", encoding.ssrc, mediaCodec->payloadType);

		// Set stream params.
		RTC::RtpStream::Params params;

		params.ssrc        = encoding.ssrc;
		params.payloadType = mediaCodec->payloadType;
		params.mimeType    = mediaCodec->mimeType;
		params.clockRate   = mediaCodec->clockRate;
		params.cname       = this->rtpParameters.rtcp.cname;

		// Check in band FEC in codec parameters.
		if (mediaCodec->parameters.HasInteger("useinbandfec") && mediaCodec->parameters.GetInteger("useinbandfec") == 1)
		{
			MS_DEBUG_TAG(rtp, "in band FEC enabled");

			params.useInBandFec = true;
		}

		// Check DTX in codec parameters.
		if (mediaCodec->parameters.HasInteger("usedtx") && mediaCodec->parameters.GetInteger("usedtx") == 1)
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

		for (const auto& fb : mediaCodec->rtcpFeedback)
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

		this->rtpStream = new RTC::RtpStreamSend(this, params, this->rtpParameters.mid);
		this->rtpStreams.push_back(this->rtpStream);

		// If the Consumer is paused, tell the RtpStreamSend.
		if (IsPaused() || IsProducerPaused())
		{
			this->rtpStream->Pause();
		}

		const auto* rtxCodec = this->rtpParameters.GetRtxCodecForEncoding(encoding);

		if (rtxCodec && encoding.hasRtx)
		{
			this->rtpStream->SetRtx(rtxCodec->payloadType, encoding.rtx.ssrc);
		}
	}

	void SimpleConsumer::RequestKeyFrame()
	{
		MS_TRACE();

		if (this->kind != RTC::Media::Kind::VIDEO)
		{
			return;
		}

		auto mappedSsrc = this->consumableRtpEncodings[0].ssrc;

		this->listener->OnConsumerKeyFrameRequested(this, mappedSsrc);
	}

	inline void SimpleConsumer::EmitScore() const
	{
		MS_TRACE();

		auto scoreOffset = FillBufferScore(this->shared->channelNotifier->GetBufferBuilder());

		auto notificationOffset = FBS::Consumer::CreateScoreNotification(
		  this->shared->channelNotifier->GetBufferBuilder(), scoreOffset);

		this->shared->channelNotifier->Emit(
		  this->id,
		  FBS::Notification::Event::CONSUMER_SCORE,
		  FBS::Notification::Body::Consumer_ScoreNotification,
		  notificationOffset);
	}

	inline void SimpleConsumer::OnRtpStreamScore(
	  RTC::RtpStream* /*rtpStream*/, uint8_t /*score*/, uint8_t /*previousScore*/)
	{
		MS_TRACE();

		// Emit the score event.
		EmitScore();
	}

	inline void SimpleConsumer::OnRtpStreamRetransmitRtpPacket(
	  RTC::RtpStreamSend* /*rtpStream*/, RTC::RtpPacket* packet)
	{
		MS_TRACE();

		this->listener->OnConsumerRetransmitRtpPacket(this, packet);

		// May emit 'trace' event.
		EmitTraceEventRtpAndKeyFrameTypes(packet, this->rtpStream->HasRtx());
	}
} // namespace RTC
