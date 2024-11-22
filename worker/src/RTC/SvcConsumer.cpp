#define MS_CLASS "RTC::SvcConsumer"
// #define MS_LOG_DEV_LEVEL 3

#include "RTC/SvcConsumer.hpp"
#include "DepLibUV.hpp"
#include "Logger.hpp"
#include "MediaSoupErrors.hpp"
#include "Utils.hpp"
#include "RTC/Codecs/Tools.hpp"

namespace RTC
{
	/* Static. */

	static constexpr uint64_t BweDowngradeConservativeMs{ 10000u }; // In ms.
	static constexpr uint64_t BweDowngradeMinActiveMs{ 8000u };     // In ms.

	/* Instance methods. */

	SvcConsumer::SvcConsumer(
	  RTC::Shared* shared,
	  const std::string& id,
	  const std::string& producerId,
	  RTC::Consumer::Listener* listener,
	  const FBS::Transport::ConsumeRequest* data)
	  : RTC::Consumer::Consumer(shared, id, producerId, listener, data, RTC::RtpParameters::Type::SVC)
	{
		MS_TRACE();

		// Ensure there is a single encoding.
		if (this->consumableRtpEncodings.size() != 1u)
		{
			MS_THROW_TYPE_ERROR("invalid consumableRtpEncodings with size != 1");
		}

		auto& encoding = this->rtpParameters.encodings[0];

		// Ensure there are multiple spatial or temporal layers.
		if (encoding.spatialLayers < 2u && encoding.temporalLayers < 2u)
		{
			MS_THROW_TYPE_ERROR("invalid number of layers");
		}

		// Set preferredLayers (if given).
		if (flatbuffers::IsFieldPresent(data, FBS::Transport::ConsumeRequest::VT_PREFERREDLAYERS))
		{
			this->preferredSpatialLayer = data->preferredLayers()->spatialLayer();

			if (this->preferredSpatialLayer > encoding.spatialLayers - 1)
			{
				this->preferredSpatialLayer = encoding.spatialLayers - 1;
			}

			if (flatbuffers::IsFieldPresent(
			      data->preferredLayers(), FBS::Consumer::ConsumerLayers::VT_TEMPORALLAYER))
			{
				if (this->preferredTemporalLayer > encoding.temporalLayers - 1)
				{
					this->preferredTemporalLayer = encoding.temporalLayers - 1;
				}
			}
			else
			{
				this->preferredTemporalLayer = encoding.temporalLayers - 1;
			}
		}
		else
		{
			// Initially set preferredSpatialLayer and preferredTemporalLayer to the
			// maximum value.
			this->preferredSpatialLayer  = encoding.spatialLayers - 1;
			this->preferredTemporalLayer = encoding.temporalLayers - 1;
		}

		// Create the encoding context.
		const auto* mediaCodec = this->rtpParameters.GetCodecForEncoding(encoding);

		if (!RTC::Codecs::Tools::IsValidTypeForCodec(this->type, mediaCodec->mimeType))
		{
			MS_THROW_TYPE_ERROR("%s codec not supported for svc", mediaCodec->mimeType.ToString().c_str());
		}

		// Let's chosee an initial output seq number between 1000 and 32768 to avoid
		// libsrtp bug:
		// https://github.com/versatica/mediasoup/issues/1437
		const uint16_t initialOutputSeq =
		  Utils::Crypto::GetRandomUInt(1000u, std::numeric_limits<uint16_t>::max() / 2);

		this->rtpSeqManager.reset(new RTC::SeqManager<uint16_t>(initialOutputSeq));

		RTC::Codecs::EncodingContext::Params params;

		params.spatialLayers  = encoding.spatialLayers;
		params.temporalLayers = encoding.temporalLayers;
		params.ksvc           = encoding.ksvc;

		this->encodingContext.reset(RTC::Codecs::Tools::GetEncodingContext(mediaCodec->mimeType, params));

		MS_ASSERT(this->encodingContext, "no encoding context for this codec");

		// Create RtpStreamSend instance for sending a single stream to the remote.
		CreateRtpStream();

		// NOTE: This may throw.
		this->shared->channelMessageRegistrator->RegisterHandler(
		  this->id,
		  /*channelRequestHandler*/ this,
		  /*channelNotificationHandler*/ nullptr);
	}

	SvcConsumer::~SvcConsumer()
	{
		MS_TRACE();

		this->shared->channelMessageRegistrator->UnregisterHandler(this->id);

		delete this->rtpStream;
	}

	flatbuffers::Offset<FBS::Consumer::DumpResponse> SvcConsumer::FillBuffer(
	  flatbuffers::FlatBufferBuilder& builder) const
	{
		MS_TRACE();

		// Call the parent method.
		auto base = RTC::Consumer::FillBuffer(builder);
		// Add rtpStream.
		std::vector<flatbuffers::Offset<FBS::RtpStream::Dump>> rtpStreams;
		rtpStreams.emplace_back(this->rtpStream->FillBuffer(builder));

		auto dump = FBS::Consumer::CreateConsumerDumpDirect(
		  builder,
		  base,
		  &rtpStreams,
		  this->preferredSpatialLayer,
		  this->encodingContext->GetTargetSpatialLayer(),
		  this->encodingContext->GetCurrentSpatialLayer(),
		  this->preferredTemporalLayer,
		  this->encodingContext->GetTargetTemporalLayer(),
		  this->encodingContext->GetCurrentTemporalLayer());

		return FBS::Consumer::CreateDumpResponse(builder, dump);
	}

	flatbuffers::Offset<FBS::Consumer::GetStatsResponse> SvcConsumer::FillBufferStats(
	  flatbuffers::FlatBufferBuilder& builder)
	{
		MS_TRACE();

		std::vector<flatbuffers::Offset<FBS::RtpStream::Stats>> rtpStreams;

		// Add stats of our send stream.
		rtpStreams.emplace_back(this->rtpStream->FillBufferStats(builder));

		// Add stats of our recv stream.
		if (this->producerRtpStream)
		{
			rtpStreams.emplace_back(producerRtpStream->FillBufferStats(builder));
		}

		return FBS::Consumer::CreateGetStatsResponseDirect(builder, &rtpStreams);
	}

	flatbuffers::Offset<FBS::Consumer::ConsumerScore> SvcConsumer::FillBufferScore(
	  flatbuffers::FlatBufferBuilder& builder) const
	{
		MS_TRACE();

		MS_ASSERT(this->producerRtpStreamScores, "producerRtpStreamScores not set");

		uint8_t producerScore{ 0 };

		if (this->producerRtpStream)
		{
			producerScore = this->producerRtpStream->GetScore();
		}
		else
		{
			producerScore = 0;
		}

		return FBS::Consumer::CreateConsumerScoreDirect(
		  builder, this->rtpStream->GetScore(), producerScore, this->producerRtpStreamScores);
	}

	void SvcConsumer::HandleRequest(Channel::ChannelRequest* request)
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
				auto previousPreferredSpatialLayer  = this->preferredSpatialLayer;
				auto previousPreferredTemporalLayer = this->preferredTemporalLayer;

				const auto* body = request->data->body_as<FBS::Consumer::SetPreferredLayersRequest>();
				const auto* preferredLayers = body->preferredLayers();

				// Spatial layer.
				this->preferredSpatialLayer = preferredLayers->spatialLayer();

				if (this->preferredSpatialLayer > this->rtpStream->GetSpatialLayers() - 1)
				{
					this->preferredSpatialLayer = this->rtpStream->GetSpatialLayers() - 1;
				}

				// preferredTemporaLayer is optional.
				if (preferredLayers->temporalLayer().has_value())
				{
					this->preferredTemporalLayer = preferredLayers->temporalLayer().value();

					if (this->preferredTemporalLayer > this->rtpStream->GetTemporalLayers() - 1)
					{
						this->preferredTemporalLayer = this->rtpStream->GetTemporalLayers() - 1;
					}
				}
				else
				{
					this->preferredTemporalLayer = this->rtpStream->GetTemporalLayers() - 1;
				}

				MS_DEBUG_DEV(
				  "preferred layers changed [spatial:%" PRIi16 ", temporal:%" PRIi16 ", consumerId:%s]",
				  this->preferredSpatialLayer,
				  this->preferredTemporalLayer,
				  this->id.c_str());

				const flatbuffers::Optional<int16_t> preferredTemporalLayer{ this->preferredTemporalLayer };
				auto preferredLayersOffset = FBS::Consumer::CreateConsumerLayers(
				  request->GetBufferBuilder(), this->preferredSpatialLayer, preferredTemporalLayer);
				auto responseOffset = FBS::Consumer::CreateSetPreferredLayersResponse(
				  request->GetBufferBuilder(), preferredLayersOffset);

				request->Accept(FBS::Response::Body::Consumer_SetPreferredLayersResponse, responseOffset);

				// clang-format off
				if (
					IsActive() &&
					(
						this->preferredSpatialLayer != previousPreferredSpatialLayer ||
						this->preferredTemporalLayer != previousPreferredTemporalLayer
					)
				)
				// clang-format on
				{
					MayChangeLayers(/*force*/ true);
				}

				break;
			}

			default:
			{
				// Pass it to the parent class.
				RTC::Consumer::HandleRequest(request);
			}
		}
	}

	void SvcConsumer::ProducerRtpStream(RTC::RtpStreamRecv* rtpStream, uint32_t /*mappedSsrc*/)
	{
		MS_TRACE();

		this->producerRtpStream = rtpStream;
	}

	void SvcConsumer::ProducerNewRtpStream(RTC::RtpStreamRecv* rtpStream, uint32_t /*mappedSsrc*/)
	{
		MS_TRACE();

		this->producerRtpStream = rtpStream;

		// Emit the score event.
		EmitScore();

		if (IsActive())
		{
			MayChangeLayers();
		}
	}

	void SvcConsumer::ProducerRtpStreamScore(
	  RTC::RtpStreamRecv* /*rtpStream*/, uint8_t score, uint8_t previousScore)
	{
		MS_TRACE();

		// Emit score event.
		EmitScore();

		if (RTC::Consumer::IsActive())
		{
			// Just check target layers if the stream has died or reborned.
			// clang-format off
			if (
				!this->externallyManagedBitrate ||
				(score == 0u || previousScore == 0u)
			)
			// clang-format on
			{
				MayChangeLayers();
			}
		}
	}

	void SvcConsumer::ProducerRtcpSenderReport(RTC::RtpStreamRecv* /*rtpStream*/, bool /*first*/)
	{
		MS_TRACE();

		// Do nothing.
	}

	uint8_t SvcConsumer::GetBitratePriority() const
	{
		MS_TRACE();

		MS_ASSERT(this->externallyManagedBitrate, "bitrate is not externally managed");

		if (!IsActive())
		{
			return 0u;
		}

		return this->priority;
	}

	uint32_t SvcConsumer::IncreaseLayer(uint32_t bitrate, bool considerLoss)
	{
		MS_TRACE();

		MS_ASSERT(this->externallyManagedBitrate, "bitrate is not externally managed");
		MS_ASSERT(IsActive(), "should be active");

		if (this->producerRtpStream->GetScore() == 0u)
		{
			return 0u;
		}

		// If already in the preferred layers, do nothing.
		// clang-format off
		if (
			this->provisionalTargetSpatialLayer == this->preferredSpatialLayer &&
			this->provisionalTargetTemporalLayer == this->preferredTemporalLayer
		)
		// clang-format on
		{
			return 0u;
		}

		uint32_t virtualBitrate;

		if (considerLoss)
		{
			// Calculate virtual available bitrate based on given bitrate and our
			// packet lost.
			auto lossPercentage = this->rtpStream->GetLossPercentage();

			if (lossPercentage < 2)
			{
				virtualBitrate = 1.08 * bitrate;
			}
			else if (lossPercentage > 10)
			{
				virtualBitrate = (1 - 0.5 * (lossPercentage / 100)) * bitrate;
			}
			else
			{
				virtualBitrate = bitrate;
			}
		}
		else
		{
			virtualBitrate = bitrate;
		}

		uint32_t requiredBitrate{ 0u };
		int16_t spatialLayer{ 0 };
		int16_t temporalLayer{ 0 };
		auto nowMs = DepLibUV::GetTimeMs();

		for (; spatialLayer < this->producerRtpStream->GetSpatialLayers(); ++spatialLayer)
		{
			// If this is higher than current spatial layer and we moved to to current spatial
			// layer due to BWE limitations, check how much it has elapsed since then.
			if (nowMs - this->lastBweDowngradeAtMs < BweDowngradeConservativeMs)
			{
				if (this->provisionalTargetSpatialLayer > -1 && spatialLayer > this->encodingContext->GetCurrentSpatialLayer())
				{
					MS_DEBUG_DEV(
					  "avoid upgrading to spatial layer %" PRIi16 " due to recent BWE downgrade", spatialLayer);

					goto done;
				}
			}

			// Ignore spatial layers lower than the one we already have.
			if (spatialLayer < this->provisionalTargetSpatialLayer)
			{
				continue;
			}

			temporalLayer = 0;

			// Check bitrate of every temporal layer.
			for (; temporalLayer < this->producerRtpStream->GetTemporalLayers(); ++temporalLayer)
			{
				// Ignore temporal layers lower than the one we already have (taking into account
				// the spatial layer too).
				// clang-format off
				if (
					spatialLayer == this->provisionalTargetSpatialLayer &&
					temporalLayer <= this->provisionalTargetTemporalLayer
				)
				// clang-format on
				{
					continue;
				}

				requiredBitrate =
				  this->producerRtpStream->GetLayerBitrate(nowMs, spatialLayer, temporalLayer);

				// When using K-SVC we must subtract the bitrate of the current used layer
				// if the new layer is the temporal layer 0 of an higher spatial layer.
				//
				// clang-format off
				if (
					this->encodingContext->IsKSvc() &&
					requiredBitrate &&
					temporalLayer == 0 &&
					this->provisionalTargetSpatialLayer > -1 &&
					spatialLayer > this->provisionalTargetSpatialLayer
				)
				// clang-format on
				{
					auto provisionalRequiredBitrate = this->producerRtpStream->GetBitrate(
					  nowMs, this->provisionalTargetSpatialLayer, this->provisionalTargetTemporalLayer);

					if (requiredBitrate > provisionalRequiredBitrate)
					{
						requiredBitrate -= provisionalRequiredBitrate;
					}
					else
					{
						requiredBitrate = 1u; // Don't set 0 since it would be ignored.
					}
				}

				MS_DEBUG_DEV(
				  "testing layers %" PRIi16 ":%" PRIi16 " [virtual bitrate:%" PRIu32
				  ", required bitrate:%" PRIu32 "]",
				  spatialLayer,
				  temporalLayer,
				  virtualBitrate,
				  requiredBitrate);

				// If active layer, end iterations here. Otherwise move to next spatial layer.
				if (requiredBitrate)
				{
					goto done;
				}
				else
				{
					break;
				}
			}

			// If this is the preferred or higher spatial layer, take it and exit.
			if (spatialLayer >= this->preferredSpatialLayer)
			{
				break;
			}
		}

	done:

		// No higher active layers found.
		if (!requiredBitrate)
		{
			return 0u;
		}

		// No luck.
		if (requiredBitrate > virtualBitrate)
		{
			return 0u;
		}

		// Set provisional layers.
		this->provisionalTargetSpatialLayer  = spatialLayer;
		this->provisionalTargetTemporalLayer = temporalLayer;

		MS_DEBUG_DEV(
		  "upgrading to layers %" PRIi16 ":%" PRIi16 " [virtual bitrate:%" PRIu32
		  ", required bitrate:%" PRIu32 "]",
		  this->provisionalTargetSpatialLayer,
		  this->provisionalTargetTemporalLayer,
		  virtualBitrate,
		  requiredBitrate);

		if (requiredBitrate <= bitrate)
		{
			return requiredBitrate;
		}
		else if (requiredBitrate <= virtualBitrate)
		{
			return bitrate;
		}
		else
		{
			return requiredBitrate; // NOTE: This cannot happen.
		}
	}

	void SvcConsumer::ApplyLayers()
	{
		MS_TRACE();

		MS_ASSERT(this->externallyManagedBitrate, "bitrate is not externally managed");
		MS_ASSERT(IsActive(), "should be active");

		auto provisionalTargetSpatialLayer  = this->provisionalTargetSpatialLayer;
		auto provisionalTargetTemporalLayer = this->provisionalTargetTemporalLayer;

		// Reset provisional target layers.
		this->provisionalTargetSpatialLayer  = -1;
		this->provisionalTargetTemporalLayer = -1;

		if (!IsActive())
		{
			return;
		}

		// clang-format off
		if (
			provisionalTargetSpatialLayer != this->encodingContext->GetTargetSpatialLayer() ||
			provisionalTargetTemporalLayer != this->encodingContext->GetTargetTemporalLayer()
		)
		// clang-format on
		{
			UpdateTargetLayers(provisionalTargetSpatialLayer, provisionalTargetTemporalLayer);

			// If this looks like a spatial layer downgrade due to BWE limitations, set member.
			// clang-format off
			if (
				this->rtpStream->GetActiveMs() > BweDowngradeMinActiveMs &&
				this->encodingContext->GetTargetSpatialLayer() < this->encodingContext->GetCurrentSpatialLayer() &&
				this->encodingContext->GetCurrentSpatialLayer() <= this->preferredSpatialLayer
			)
			// clang-format on
			{
				MS_DEBUG_DEV(
				  "possible target spatial layer downgrade (from %" PRIi16 " to %" PRIi16
				  ") due to BWE limitation",
				  this->encodingContext->GetCurrentSpatialLayer(),
				  this->encodingContext->GetTargetSpatialLayer());

				this->lastBweDowngradeAtMs = DepLibUV::GetTimeMs();
			}
		}
	}

	uint32_t SvcConsumer::GetDesiredBitrate() const
	{
		MS_TRACE();

		MS_ASSERT(this->externallyManagedBitrate, "bitrate is not externally managed");

		if (!IsActive())
		{
			return 0u;
		}

		auto nowMs = DepLibUV::GetTimeMs();
		uint32_t desiredBitrate{ 0u };

		// When using K-SVC each spatial layer is independent of the others.
		if (this->encodingContext->IsKSvc())
		{
			// Let's iterate all spatial layers of the Producer (from highest to lowest) and
			// obtain their bitrate. Choose the highest one.
			// NOTE: When the Producer enables a higher spatial layer, initially the bitrate
			// oft could be less than the bitrate of a lower one. That's why we iterate all
			// spatial layers here anyway.
			for (auto spatialLayer{ this->producerRtpStream->GetSpatialLayers() - 1 }; spatialLayer >= 0;
			     --spatialLayer)
			{
				auto spatialLayerBitrate =
				  this->producerRtpStream->GetSpatialLayerBitrate(nowMs, spatialLayer);

				if (spatialLayerBitrate > desiredBitrate)
				{
					desiredBitrate = spatialLayerBitrate;
				}
			}
		}
		else
		{
			desiredBitrate = this->producerRtpStream->GetBitrate(nowMs);
		}

		// If consumer.rtpParameters.encodings[0].maxBitrate was given and it's
		// greater than computed one, then use it.
		auto maxBitrate = this->rtpParameters.encodings[0].maxBitrate;

		if (maxBitrate > desiredBitrate)
		{
			desiredBitrate = maxBitrate;
		}

		return desiredBitrate;
	}

	void SvcConsumer::SendRtpPacket(RTC::RtpPacket* packet, std::shared_ptr<RTC::RtpPacket>& sharedPacket)
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

		// clang-format off
		if (
			this->encodingContext->GetTargetSpatialLayer() == -1 ||
			this->encodingContext->GetTargetTemporalLayer() == -1
		)
		// clang-format on
		{
#ifdef MS_RTC_LOGGER_RTP
			packet->logger.Dropped(RtcLogger::RtpPacket::DropReason::INVALID_TARGET_LAYER);
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

		// If we need to sync and this is not a key frame, ignore the packet.
		if (this->syncRequired && !packet->IsKeyFrame())
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
			this->encodingContext->SyncRequired();

			this->syncRequired = false;
		}

		auto previousSpatialLayer  = this->encodingContext->GetCurrentSpatialLayer();
		auto previousTemporalLayer = this->encodingContext->GetCurrentTemporalLayer();

		bool marker{ false };
		const bool origMarker = packet->HasMarker();

		if (!packet->ProcessPayload(this->encodingContext.get(), marker))
		{
			this->rtpSeqManager->Drop(packet->GetSequenceNumber());

#ifdef MS_RTC_LOGGER_RTP
			packet->logger.Dropped(RtcLogger::RtpPacket::DropReason::DROPPED_BY_CODEC);
#endif

			return;
		}

		// clang-format off
		if (
			previousSpatialLayer != this->encodingContext->GetCurrentSpatialLayer() ||
			previousTemporalLayer != this->encodingContext->GetCurrentTemporalLayer()
		)
		// clang-format on
		{
			// Emit the layersChange event.
			EmitLayersChange();
		}

		// Update RTP seq number and timestamp based on NTP offset.
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

		if (marker)
		{
			packet->SetMarker(true);
		}

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
			  "] from original [ssrc:%" PRIu32 ", seq:%" PRIu16 "]",
			  packet->GetSsrc(),
			  packet->GetSequenceNumber(),
			  packet->GetTimestamp(),
			  origSsrc,
			  origSeq);
		}

		// Restore packet fields.
		packet->SetSsrc(origSsrc);
		packet->SetSequenceNumber(origSeq);
		packet->SetMarker(origMarker);

		// Restore the original payload if needed.
		packet->RestorePayload();
	}

	bool SvcConsumer::GetRtcp(RTC::RTCP::CompoundPacket* packet, uint64_t nowMs)
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

	void SvcConsumer::NeedWorstRemoteFractionLost(uint32_t /*mappedSsrc*/, uint8_t& worstRemoteFractionLost)
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

	void SvcConsumer::ReceiveNack(RTC::RTCP::FeedbackRtpNackPacket* nackPacket)
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

	void SvcConsumer::ReceiveKeyFrameRequest(RTC::RTCP::FeedbackPs::MessageType messageType, uint32_t ssrc)
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

	void SvcConsumer::ReceiveRtcpReceiverReport(RTC::RTCP::ReceiverReport* report)
	{
		MS_TRACE();

		this->rtpStream->ReceiveRtcpReceiverReport(report);
	}

	void SvcConsumer::ReceiveRtcpXrReceiverReferenceTime(RTC::RTCP::ReceiverReferenceTime* report)
	{
		MS_TRACE();

		this->rtpStream->ReceiveRtcpXrReceiverReferenceTime(report);
	}

	uint32_t SvcConsumer::GetTransmissionRate(uint64_t nowMs)
	{
		MS_TRACE();

		if (!IsActive())
		{
			return 0u;
		}

		return this->rtpStream->GetBitrate(nowMs);
	}

	float SvcConsumer::GetRtt() const
	{
		MS_TRACE();

		return this->rtpStream->GetRtt();
	}

	void SvcConsumer::UserOnTransportConnected()
	{
		MS_TRACE();

		this->syncRequired = true;

		if (IsActive())
		{
			MayChangeLayers();
		}
	}

	void SvcConsumer::UserOnTransportDisconnected()
	{
		MS_TRACE();

		this->lastBweDowngradeAtMs = 0u;

		this->rtpStream->Pause();

		UpdateTargetLayers(-1, -1);
	}

	void SvcConsumer::UserOnPaused()
	{
		MS_TRACE();

		this->lastBweDowngradeAtMs = 0u;

		this->rtpStream->Pause();

		UpdateTargetLayers(-1, -1);

		if (this->externallyManagedBitrate)
		{
			this->listener->OnConsumerNeedZeroBitrate(this);
		}
	}

	void SvcConsumer::UserOnResumed()
	{
		MS_TRACE();

		this->syncRequired = true;

		if (IsActive())
		{
			MayChangeLayers();
		}
	}

	void SvcConsumer::CreateRtpStream()
	{
		MS_TRACE();

		auto& encoding         = this->rtpParameters.encodings[0];
		const auto* mediaCodec = this->rtpParameters.GetCodecForEncoding(encoding);

		MS_DEBUG_TAG(
		  rtp, "[ssrc:%" PRIu32 ", payloadType:%" PRIu8 "]", encoding.ssrc, mediaCodec->payloadType);

		// Set stream params.
		RTC::RtpStream::Params params;

		params.ssrc           = encoding.ssrc;
		params.payloadType    = mediaCodec->payloadType;
		params.mimeType       = mediaCodec->mimeType;
		params.clockRate      = mediaCodec->clockRate;
		params.cname          = this->rtpParameters.rtcp.cname;
		params.spatialLayers  = encoding.spatialLayers;
		params.temporalLayers = encoding.temporalLayers;

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

	void SvcConsumer::RequestKeyFrame()
	{
		MS_TRACE();

		if (this->kind != RTC::Media::Kind::VIDEO)
		{
			return;
		}

		auto mappedSsrc = this->consumableRtpEncodings[0].ssrc;

		this->listener->OnConsumerKeyFrameRequested(this, mappedSsrc);
	}

	void SvcConsumer::MayChangeLayers(bool force)
	{
		MS_TRACE();

		int16_t newTargetSpatialLayer;
		int16_t newTargetTemporalLayer;

		if (RecalculateTargetLayers(newTargetSpatialLayer, newTargetTemporalLayer))
		{
			// If bitrate externally managed, don't bother the transport unless
			// the newTargetSpatialLayer has changed (or force is true).
			// This is because, if bitrate is externally managed, the target temporal
			// layer is managed by the available given bitrate so the transport
			// will let us change it when it considers.
			if (this->externallyManagedBitrate)
			{
				if (newTargetSpatialLayer != this->encodingContext->GetTargetSpatialLayer() || force)
				{
					this->listener->OnConsumerNeedBitrateChange(this);
				}
			}
			else
			{
				UpdateTargetLayers(newTargetSpatialLayer, newTargetTemporalLayer);
			}
		}
	}

	bool SvcConsumer::RecalculateTargetLayers(
	  int16_t& newTargetSpatialLayer, int16_t& newTargetTemporalLayer) const
	{
		MS_TRACE();

		// Start with no layers.
		newTargetSpatialLayer  = -1;
		newTargetTemporalLayer = -1;

		auto nowMs = DepLibUV::GetTimeMs();
		int16_t spatialLayer{ 0 };

		if (!this->producerRtpStream)
		{
			goto done;
		}

		if (this->producerRtpStream->GetScore() == 0u)
		{
			goto done;
		}

		for (; spatialLayer < this->producerRtpStream->GetSpatialLayers(); ++spatialLayer)
		{
			// If this is higher than current spatial layer and we moved to to current spatial
			// layer due to BWE limitations, check how much it has elapsed since then.
			if (nowMs - this->lastBweDowngradeAtMs < BweDowngradeConservativeMs)
			{
				if (newTargetSpatialLayer > -1 && spatialLayer > this->encodingContext->GetCurrentSpatialLayer())
				{
					continue;
				}
			}

			if (!this->producerRtpStream->GetSpatialLayerBitrate(nowMs, spatialLayer))
			{
				continue;
			}

			newTargetSpatialLayer = spatialLayer;

			// If this is the preferred or higher spatial layer and has bitrate,
			// take it and exit.
			if (spatialLayer >= this->preferredSpatialLayer)
			{
				break;
			}
		}

		if (newTargetSpatialLayer != -1)
		{
			if (newTargetSpatialLayer == this->preferredSpatialLayer)
			{
				newTargetTemporalLayer = this->preferredTemporalLayer;
			}
			else if (newTargetSpatialLayer < this->preferredSpatialLayer)
			{
				newTargetTemporalLayer = this->rtpStream->GetTemporalLayers() - 1;
			}
			else
			{
				newTargetTemporalLayer = 0;
			}
		}

	done:

		// Return true if any target layer changed.
		// clang-format off
		return (
			newTargetSpatialLayer != this->encodingContext->GetTargetSpatialLayer() ||
			newTargetTemporalLayer != this->encodingContext->GetTargetTemporalLayer()
		);
		// clang-format on
	}

	void SvcConsumer::UpdateTargetLayers(int16_t newTargetSpatialLayer, int16_t newTargetTemporalLayer)
	{
		MS_TRACE();

		if (newTargetSpatialLayer == -1)
		{
			// Unset current and target layers.
			this->encodingContext->SetTargetSpatialLayer(-1);
			this->encodingContext->SetCurrentSpatialLayer(-1);
			this->encodingContext->SetTargetTemporalLayer(-1);
			this->encodingContext->SetCurrentTemporalLayer(-1);

			MS_DEBUG_TAG(
			  svc, "target layers changed [spatial:-1, temporal:-1, consumerId:%s]", this->id.c_str());

			EmitLayersChange();

			return;
		}

		this->encodingContext->SetTargetSpatialLayer(newTargetSpatialLayer);
		this->encodingContext->SetTargetTemporalLayer(newTargetTemporalLayer);

		MS_DEBUG_TAG(
		  svc,
		  "target layers changed [spatial:%" PRIi16 ", temporal:%" PRIi16 ", consumerId:%s]",
		  newTargetSpatialLayer,
		  newTargetTemporalLayer,
		  this->id.c_str());

		// Target spatial layer has changed.
		if (newTargetSpatialLayer != this->encodingContext->GetCurrentSpatialLayer())
		{
			// In K-SVC always ask for a keyframe when changing target spatial layer.
			if (this->encodingContext->IsKSvc())
			{
				MS_DEBUG_DEV("K-SVC: requesting keyframe to target spatial change");

				RequestKeyFrame();
			}
			// In full SVC just ask for a keyframe when upgrading target spatial layer.
			// NOTE: This is because nobody implements RTCP LRR yet.
			else if (newTargetSpatialLayer > this->encodingContext->GetCurrentSpatialLayer())
			{
				MS_DEBUG_DEV("full SVC: requesting keyframe to target spatial upgrade");

				RequestKeyFrame();
			}
		}
	}

	inline void SvcConsumer::EmitScore() const
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

	inline void SvcConsumer::EmitLayersChange() const
	{
		MS_TRACE();

		MS_DEBUG_DEV(
		  "current layers changed to [spatial:%" PRIi16 ", temporal:%" PRIi16 ", consumerId:%s]",
		  this->encodingContext->GetCurrentSpatialLayer(),
		  this->encodingContext->GetCurrentTemporalLayer(),
		  this->id.c_str());

		flatbuffers::Offset<FBS::Consumer::ConsumerLayers> layersOffset;

		if (this->encodingContext->GetCurrentSpatialLayer() >= 0)
		{
			layersOffset = FBS::Consumer::CreateConsumerLayers(
			  this->shared->channelNotifier->GetBufferBuilder(),
			  this->encodingContext->GetCurrentSpatialLayer(),
			  this->encodingContext->GetCurrentTemporalLayer());
		}

		auto notificationOffset = FBS::Consumer::CreateLayersChangeNotification(
		  this->shared->channelNotifier->GetBufferBuilder(), layersOffset);

		this->shared->channelNotifier->Emit(
		  this->id,
		  FBS::Notification::Event::CONSUMER_LAYERS_CHANGE,
		  FBS::Notification::Body::Consumer_LayersChangeNotification,
		  notificationOffset);
	}

	inline void SvcConsumer::OnRtpStreamScore(
	  RTC::RtpStream* /*rtpStream*/, uint8_t /*score*/, uint8_t /*previousScore*/)
	{
		MS_TRACE();

		// Emit the score event.
		EmitScore();

		if (IsActive())
		{
			// Just check target layers if our bitrate is not externally managed.
			// NOTE: For now this is a bit useless since, when locally managed, we do
			// not check the Consumer score at all.
			if (!this->externallyManagedBitrate)
			{
				MayChangeLayers();
			}
		}
	}

	inline void SvcConsumer::OnRtpStreamRetransmitRtpPacket(
	  RTC::RtpStreamSend* /*rtpStream*/, RTC::RtpPacket* packet)
	{
		MS_TRACE();

		this->listener->OnConsumerRetransmitRtpPacket(this, packet);

		// May emit 'trace' event.
		EmitTraceEventRtpAndKeyFrameTypes(packet, this->rtpStream->HasRtx());
	}
} // namespace RTC
