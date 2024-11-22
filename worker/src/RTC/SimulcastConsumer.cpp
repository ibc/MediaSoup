#define MS_CLASS "RTC::SimulcastConsumer"
// #define MS_LOG_DEV_LEVEL 3

#include "RTC/SimulcastConsumer.hpp"
#include "DepLibUV.hpp"
#include "Logger.hpp"
#include "MediaSoupErrors.hpp"
#include "Utils.hpp"
#include "RTC/Codecs/Tools.hpp"

namespace RTC
{
	/* Static. */

	static constexpr uint64_t StreamMinActiveMs{ 2000u };           // In ms.
	static constexpr uint64_t BweDowngradeConservativeMs{ 10000u }; // In ms.
	static constexpr uint64_t BweDowngradeMinActiveMs{ 8000u };     // In ms.
	static constexpr uint16_t MaxSequenceNumberGap{ 100u };

	/* Instance methods. */

	SimulcastConsumer::SimulcastConsumer(
	  RTC::Shared* shared,
	  const std::string& id,
	  const std::string& producerId,
	  RTC::Consumer::Listener* listener,
	  const FBS::Transport::ConsumeRequest* data)
	  : RTC::Consumer::Consumer(
	      shared, id, producerId, listener, data, RTC::RtpParameters::Type::SIMULCAST)
	{
		MS_TRACE();

		// We allow a single encoding in simulcast (so we can enable temporal layers
		// with a single simulcast stream).
		// NOTE: No need to check this->consumableRtpEncodings.size() > 0 here since
		// it's already done in Consumer constructor.

		auto& encoding = this->rtpParameters.encodings[0];

		// Ensure there are as many spatial layers as encodings.
		if (encoding.spatialLayers != this->consumableRtpEncodings.size())
		{
			MS_THROW_TYPE_ERROR("encoding.spatialLayers does not match number of consumableRtpEncodings");
		}

		// Fill mapMappedSsrcSpatialLayer.
		for (size_t idx{ 0u }; idx < this->consumableRtpEncodings.size(); ++idx)
		{
			auto& encoding = this->consumableRtpEncodings[idx];

			this->mapMappedSsrcSpatialLayer[encoding.ssrc] = static_cast<int16_t>(idx);
		}

		// Set preferredLayers (if given).
		if (flatbuffers::IsFieldPresent(data, FBS::Transport::ConsumeRequest::VT_PREFERREDLAYERS))
		{
			const auto* preferredLayers = data->preferredLayers();

			this->preferredSpatialLayer = preferredLayers->spatialLayer();

			if (this->preferredSpatialLayer > encoding.spatialLayers - 1)
			{
				this->preferredSpatialLayer = encoding.spatialLayers - 1;
			}

			if (preferredLayers->temporalLayer().has_value())
			{
				this->preferredTemporalLayer = preferredLayers->temporalLayer().value();

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

		// Reserve space for the Producer RTP streams by filling all the possible
		// entries with nullptr.
		this->producerRtpStreams.insert(
		  this->producerRtpStreams.begin(), this->consumableRtpEncodings.size(), nullptr);

		// Create the encoding context.
		const auto* mediaCodec = this->rtpParameters.GetCodecForEncoding(encoding);

		if (!RTC::Codecs::Tools::IsValidTypeForCodec(this->type, mediaCodec->mimeType))
		{
			MS_THROW_TYPE_ERROR(
			  "%s codec not supported for simulcast", mediaCodec->mimeType.ToString().c_str());
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

		this->encodingContext.reset(RTC::Codecs::Tools::GetEncodingContext(mediaCodec->mimeType, params));

		MS_ASSERT(this->encodingContext, "no encoding context for this codec");

		// Create RtpStreamSend instance for sending a single stream to the remote.
		CreateRtpStream();

		// NOTE: This may throw.
		this->shared->channelMessageRegistrator->RegisterHandler(
		  this->id,
		  /*channelRequestHandler*/ this,
		  /*channelRequestHandler*/ nullptr);
	}

	SimulcastConsumer::~SimulcastConsumer()
	{
		MS_TRACE();

		this->shared->channelMessageRegistrator->UnregisterHandler(this->id);

		delete this->rtpStream;
	}

	flatbuffers::Offset<FBS::Consumer::DumpResponse> SimulcastConsumer::FillBuffer(
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
		  this->targetSpatialLayer,
		  this->currentSpatialLayer,
		  this->preferredTemporalLayer,
		  this->targetTemporalLayer,
		  this->encodingContext->GetCurrentTemporalLayer());

		return FBS::Consumer::CreateDumpResponse(builder, dump);
	}

	flatbuffers::Offset<FBS::Consumer::GetStatsResponse> SimulcastConsumer::FillBufferStats(
	  flatbuffers::FlatBufferBuilder& builder)
	{
		MS_TRACE();

		std::vector<flatbuffers::Offset<FBS::RtpStream::Stats>> rtpStreams;

		// Add stats of our send stream.
		rtpStreams.emplace_back(this->rtpStream->FillBufferStats(builder));

		auto* producerCurrentRtpStream = GetProducerCurrentRtpStream();

		// Add stats of our recv stream.
		if (producerCurrentRtpStream)
		{
			rtpStreams.emplace_back(producerCurrentRtpStream->FillBufferStats(builder));
		}

		return FBS::Consumer::CreateGetStatsResponseDirect(builder, &rtpStreams);
	}

	flatbuffers::Offset<FBS::Consumer::ConsumerScore> SimulcastConsumer::FillBufferScore(
	  flatbuffers::FlatBufferBuilder& builder) const
	{
		MS_TRACE();

		MS_ASSERT(this->producerRtpStreamScores, "producerRtpStreamScores not set");

		auto* producerCurrentRtpStream = GetProducerCurrentRtpStream();

		uint8_t producerScore{ 0 };

		if (producerCurrentRtpStream)
		{
			producerScore = producerCurrentRtpStream->GetScore();
		}
		else
		{
			producerScore = 0;
		}

		return FBS::Consumer::CreateConsumerScoreDirect(
		  builder, this->rtpStream->GetScore(), producerScore, this->producerRtpStreamScores);
	}

	void SimulcastConsumer::HandleRequest(Channel::ChannelRequest* request)
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
					RequestKeyFrames();
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

	void SimulcastConsumer::ProducerRtpStream(RTC::RtpStreamRecv* rtpStream, uint32_t mappedSsrc)
	{
		MS_TRACE();

		auto it = this->mapMappedSsrcSpatialLayer.find(mappedSsrc);

		MS_ASSERT(it != this->mapMappedSsrcSpatialLayer.end(), "unknown mappedSsrc");

		const int16_t spatialLayer = it->second;

		this->producerRtpStreams[spatialLayer] = rtpStream;
	}

	void SimulcastConsumer::ProducerNewRtpStream(RTC::RtpStreamRecv* rtpStream, uint32_t mappedSsrc)
	{
		MS_TRACE();

		auto it = this->mapMappedSsrcSpatialLayer.find(mappedSsrc);

		MS_ASSERT(it != this->mapMappedSsrcSpatialLayer.end(), "unknown mappedSsrc");

		const int16_t spatialLayer = it->second;

		this->producerRtpStreams[spatialLayer] = rtpStream;

		// Emit the score event.
		EmitScore();

		if (IsActive())
		{
			MayChangeLayers();
		}
	}

	void SimulcastConsumer::ProducerRtpStreamScore(
	  RTC::RtpStreamRecv* /*rtpStream*/, uint8_t score, uint8_t previousScore)
	{
		MS_TRACE();

		// Emit the score event.
		EmitScore();

		if (RTC::Consumer::IsActive())
		{
			// All Producer streams are dead.
			if (!IsActive())
			{
				UpdateTargetLayers(-1, -1);
			}
			// Just check target layers if the stream has died or reborned.
			// clang-format off
			else if (
				!this->externallyManagedBitrate ||
				(score == 0u || previousScore == 0u)
			)
			// clang-format on
			{
				MayChangeLayers();
			}
		}
	}

	void SimulcastConsumer::ProducerRtcpSenderReport(RTC::RtpStreamRecv* rtpStream, bool first)
	{
		MS_TRACE();

		// Just interested if this is the first Sender Report for a RTP stream.
		if (!first)
		{
			return;
		}

		MS_DEBUG_TAG(simulcast, "first SenderReport [ssrc:%" PRIu32 "]", rtpStream->GetSsrc());

		// If our RTP timestamp reference stream does not yet have SR, do nothing
		// since we know we won't be able to switch.
		auto* producerTsReferenceRtpStream = GetProducerTsReferenceRtpStream();

		if (!producerTsReferenceRtpStream || !producerTsReferenceRtpStream->GetSenderReportNtpMs())
		{
			return;
		}

		if (IsActive())
		{
			MayChangeLayers();
		}
	}

	uint8_t SimulcastConsumer::GetBitratePriority() const
	{
		MS_TRACE();

		MS_ASSERT(this->externallyManagedBitrate, "bitrate is not externally managed");

		if (!IsActive())
		{
			return 0u;
		}

		return this->priority;
	}

	uint32_t SimulcastConsumer::IncreaseLayer(uint32_t bitrate, bool considerLoss)
	{
		MS_TRACE();

		MS_ASSERT(this->externallyManagedBitrate, "bitrate is not externally managed");
		MS_ASSERT(IsActive(), "should be active");

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

		for (size_t sIdx{ 0u }; sIdx < this->producerRtpStreams.size(); ++sIdx)
		{
			spatialLayer = static_cast<int16_t>(sIdx);

			// If this is higher than current spatial layer and we moved to to current spatial
			// layer due to BWE limitations, check how much it has elapsed since then.
			if (nowMs - this->lastBweDowngradeAtMs < BweDowngradeConservativeMs)
			{
				if (this->provisionalTargetSpatialLayer > -1 && spatialLayer > this->currentSpatialLayer)
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

			// This can be null.
			auto* producerRtpStream = this->producerRtpStreams.at(spatialLayer);

			// Producer stream does not exist. Ignore.
			if (!producerRtpStream)
			{
				continue;
			}

			// Ignore spatial layers (streams) with score 0.
			if (producerRtpStream->GetScore() == 0)
			{
				continue;
			}

			// If the stream has not been active time enough and we have an active one
			// already, move to the next spatial layer.
			// clang-format off
			if (
				spatialLayer != this->provisionalTargetSpatialLayer &&
				this->provisionalTargetSpatialLayer != -1 &&
				producerRtpStream->GetActiveMs() < StreamMinActiveMs
			)
			// clang-format on
			{
				const auto* provisionalProducerRtpStream =
				  this->producerRtpStreams.at(this->provisionalTargetSpatialLayer);

				// The stream for the current provisional spatial layer has been active
				// for enough time, move to the next spatial layer.
				if (provisionalProducerRtpStream->GetActiveMs() >= StreamMinActiveMs)
				{
					continue;
				}
			}

			// We may not yet switch to this spatial layer.
			if (!CanSwitchToSpatialLayer(spatialLayer))
			{
				continue;
			}

			temporalLayer = 0;

			// Check bitrate of every temporal layer.
			for (; temporalLayer < producerRtpStream->GetTemporalLayers(); ++temporalLayer)
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

				requiredBitrate = producerRtpStream->GetLayerBitrate(nowMs, 0, temporalLayer);

				// This is simulcast so we must substract the bitrate of the current temporal
				// spatial layer if this is the temporal layer 0 of a higher spatial layer.
				//
				// clang-format off
				if (
					requiredBitrate &&
					temporalLayer == 0 &&
					this->provisionalTargetSpatialLayer > -1 &&
					spatialLayer > this->provisionalTargetSpatialLayer
				)
				// clang-format on
				{
					auto* provisionalProducerRtpStream =
					  this->producerRtpStreams.at(this->provisionalTargetSpatialLayer);
					auto provisionalRequiredBitrate =
					  provisionalProducerRtpStream->GetBitrate(nowMs, 0, this->provisionalTargetTemporalLayer);

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
		  "setting provisional layers to %" PRIi16 ":%" PRIi16 " [virtual bitrate:%" PRIu32
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

	void SimulcastConsumer::ApplyLayers()
	{
		MS_TRACE();

		MS_ASSERT(this->externallyManagedBitrate, "bitrate is not externally managed");
		MS_ASSERT(IsActive(), "should be active");

		auto provisionalTargetSpatialLayer  = this->provisionalTargetSpatialLayer;
		auto provisionalTargetTemporalLayer = this->provisionalTargetTemporalLayer;

		// Reset provisional target layers.
		this->provisionalTargetSpatialLayer  = -1;
		this->provisionalTargetTemporalLayer = -1;

		// clang-format off
		if (
			provisionalTargetSpatialLayer != this->targetSpatialLayer ||
			provisionalTargetTemporalLayer != this->targetTemporalLayer
		)
		// clang-format on
		{
			UpdateTargetLayers(provisionalTargetSpatialLayer, provisionalTargetTemporalLayer);

			// If this looks like a spatial layer downgrade due to BWE limitations, set member.
			// clang-format off
			if (
				this->rtpStream->GetActiveMs() > BweDowngradeMinActiveMs &&
				this->targetSpatialLayer < this->currentSpatialLayer &&
				this->currentSpatialLayer <= this->preferredSpatialLayer
			)
			// clang-format on
			{
				MS_DEBUG_DEV(
				  "possible target spatial layer downgrade (from %" PRIi16 " to %" PRIi16
				  ") due to BWE limitation",
				  this->currentSpatialLayer,
				  this->targetSpatialLayer);

				this->lastBweDowngradeAtMs = DepLibUV::GetTimeMs();
			}
		}
	}

	uint32_t SimulcastConsumer::GetDesiredBitrate() const
	{
		MS_TRACE();

		MS_ASSERT(this->externallyManagedBitrate, "bitrate is not externally managed");

		if (!IsActive())
		{
			return 0u;
		}

		auto nowMs = DepLibUV::GetTimeMs();
		uint32_t desiredBitrate{ 0u };

		// Let's iterate all streams of the Producer (from highest to lowest) and
		// obtain their bitrate. Choose the highest one.
		// NOTE: When the Producer enables a higher stream, initially the bitrate of
		// it could be less than the bitrate of a lower stream. That's why we
		// iterate all streams here anyway.
		for (auto sIdx{ static_cast<int16_t>(this->producerRtpStreams.size() - 1) }; sIdx >= 0; --sIdx)
		{
			auto* producerRtpStream = this->producerRtpStreams.at(sIdx);

			if (!producerRtpStream)
			{
				continue;
			}

			auto streamBitrate = producerRtpStream->GetBitrate(nowMs);

			if (streamBitrate > desiredBitrate)
			{
				desiredBitrate = streamBitrate;
			}
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

	void SimulcastConsumer::SendRtpPacket(
	  RTC::RtpPacket* packet, std::shared_ptr<RTC::RtpPacket>& sharedPacket)
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

		if (this->targetTemporalLayer == -1)
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

		auto spatialLayer = this->mapMappedSsrcSpatialLayer.at(packet->GetSsrc());
		bool shouldSwitchCurrentSpatialLayer{ false };

		// Check whether this is the packet we are waiting for in order to update
		// the current spatial layer.
		if (this->currentSpatialLayer != this->targetSpatialLayer && spatialLayer == this->targetSpatialLayer)
		{
			// Ignore if not a key frame.
			if (!packet->IsKeyFrame())
			{
#ifdef MS_RTC_LOGGER_RTP
				packet->logger.Dropped(RtcLogger::RtpPacket::DropReason::NOT_A_KEYFRAME);
#endif

				return;
			}

			shouldSwitchCurrentSpatialLayer = true;

			// Need to resync the stream.
			this->syncRequired       = true;
			this->spatialLayerToSync = spatialLayer;
		}
		// If the packet belongs to different spatial layer than the one being sent,
		// drop it.
		else if (spatialLayer != this->currentSpatialLayer)
		{
#ifdef MS_RTC_LOGGER_RTP
			packet->logger.Dropped(RtcLogger::RtpPacket::DropReason::SPATIAL_LAYER_MISMATCH);
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

		// If the packet belongs to current spatial layer being sent and packet does
		// not have payload other than padding, then drop it.
		if (spatialLayer == this->currentSpatialLayer && packet->GetPayloadLength() == 0)
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
		if (isSyncPacket && (this->spatialLayerToSync == -1 || this->spatialLayerToSync == spatialLayer))
		{
			if (packet->IsKeyFrame())
			{
				MS_DEBUG_TAG(rtp, "sync key frame received");
			}

			uint32_t tsOffset{ 0u };

			// Sync our RTP stream's RTP timestamp.
			if (spatialLayer == this->tsReferenceSpatialLayer)
			{
				tsOffset = 0u;
			}
			// If this is not the RTP stream we use as TS reference, do NTP based RTP TS synchronization.
			else
			{
				auto* producerTsReferenceRtpStream = GetProducerTsReferenceRtpStream();
				auto* producerTargetRtpStream      = GetProducerTargetRtpStream();

				// NOTE: If we are here is because we have Sender Reports for both the
				// TS reference stream and the target one.
				MS_ASSERT(
				  producerTsReferenceRtpStream->GetSenderReportNtpMs(),
				  "no Sender Report for TS reference RTP stream");
				MS_ASSERT(
				  producerTargetRtpStream->GetSenderReportNtpMs(), "no Sender Report for current RTP stream");

				// Calculate NTP and TS stuff.
				auto ntpMs1 = producerTsReferenceRtpStream->GetSenderReportNtpMs();
				auto ts1    = producerTsReferenceRtpStream->GetSenderReportTs();
				auto ntpMs2 = producerTargetRtpStream->GetSenderReportNtpMs();
				auto ts2    = producerTargetRtpStream->GetSenderReportTs();
				int64_t diffMs;

				if (ntpMs2 >= ntpMs1)
				{
					diffMs = ntpMs2 - ntpMs1;
				}
				else
				{
					diffMs = -1 * (ntpMs1 - ntpMs2);
				}

				const int64_t diffTs  = diffMs * this->rtpStream->GetClockRate() / 1000;
				const uint32_t newTs2 = ts2 - diffTs;

				// Apply offset. This is the difference that later must be removed from the
				// sending RTP packet.
				tsOffset = newTs2 - ts1;
			}

			// When switching to a new stream it may happen that the timestamp of this
			// key frame is lower than the highest timestamp sent to the remote endpoint.
			// If so, apply an extra offset to "fix" it for the whole live of this selected
			// Producer stream.
			//
			// clang-format off
			if (
				shouldSwitchCurrentSpatialLayer &&
				(packet->GetTimestamp() - tsOffset <= this->rtpStream->GetMaxPacketTs())
			)
			// clang-format on
			{
				// Max delay in ms we allow for the stream when switching.
				// https://en.wikipedia.org/wiki/Audio-to-video_synchronization#Recommendations
				static const uint32_t MaxExtraOffsetMs{ 75u };

				// Outgoing packet matches the highest timestamp seen in the previous stream.
				// Apply an expected offset for a new frame in a 30fps stream.
				static const uint8_t MsOffset{ 33u }; // (1 / 30 * 1000).

				const int64_t maxTsExtraOffset = MaxExtraOffsetMs * this->rtpStream->GetClockRate() / 1000;
				uint32_t tsExtraOffset = this->rtpStream->GetMaxPacketTs() - packet->GetTimestamp() +
				                         tsOffset + MsOffset * this->rtpStream->GetClockRate() / 1000;

				// NOTE: Don't ask for a key frame if already done.
				if (this->keyFrameForTsOffsetRequested)
				{
					// Give up and use the theoretical offset.
					if (tsExtraOffset > maxTsExtraOffset)
					{
						MS_WARN_TAG(
						  simulcast,
						  "giving up on proper stream switching after got a requested keyframe for which still too high RTP timestamp extra offset is needed (%" PRIu32
						  ")",
						  tsExtraOffset);

						tsExtraOffset = 1u;
					}
				}
				else if (tsExtraOffset > maxTsExtraOffset)
				{
					MS_WARN_TAG(
					  simulcast,
					  "cannot switch stream due to too high RTP timestamp extra offset needed (%" PRIu32
					  "), requesting keyframe",
					  tsExtraOffset);

					RequestKeyFrameForTargetSpatialLayer();

					this->keyFrameForTsOffsetRequested = true;

					// Reset flags since we are discarding this key frame.
					this->syncRequired       = false;
					this->spatialLayerToSync = -1;

#ifdef MS_RTC_LOGGER_RTP
					packet->logger.Dropped(RtcLogger::RtpPacket::DropReason::TOO_HIGH_TIMESTAMP_EXTRA_NEEDED);
#endif

					return;
				}

				if (tsExtraOffset > 0u)
				{
					MS_DEBUG_TAG(
					  simulcast,
					  "RTP timestamp extra offset generated for stream switching: %" PRIu32,
					  tsExtraOffset);

					// Increase the timestamp offset for the whole life of this Producer stream
					// (until switched to a different one).
					tsOffset -= tsExtraOffset;
				}
			}

			this->tsOffset = tsOffset;

			// Sync our RTP stream's sequence number.
			// If previous frame has not been sent completely when we switch layer,
			// we can tell libwebrtc that previous frame is incomplete by skipping
			// one RTP sequence number.
			// 'packet->GetSequenceNumber() -2' may increase SeqManager::base and
			// increase the output sequence number.
			// https://github.com/versatica/mediasoup/issues/408
			this->rtpSeqManager->Sync(packet->GetSequenceNumber() - (this->lastSentPacketHasMarker ? 1 : 2));

			this->encodingContext->SyncRequired();

			this->syncRequired                 = false;
			this->spatialLayerToSync           = -1;
			this->keyFrameForTsOffsetRequested = false;
		}

		if (!shouldSwitchCurrentSpatialLayer && this->checkingForOldPacketsInSpatialLayer)
		{
			// If this is a packet previous to the spatial layer switch, ignore the packet.
			if (SeqManager<uint16_t>::IsSeqLowerThan(
			      packet->GetSequenceNumber(), this->snReferenceSpatialLayer))
			{
#ifdef MS_RTC_LOGGER_RTP
				packet->logger.Dropped(
				  RtcLogger::RtpPacket::DropReason::PACKET_PREVIOUS_TO_SPATIAL_LAYER_SWITCH);
#endif

				return;
			}
			else if (SeqManager<uint16_t>::IsSeqHigherThan(
			           packet->GetSequenceNumber(), this->snReferenceSpatialLayer + MaxSequenceNumberGap))
			{
				this->checkingForOldPacketsInSpatialLayer = false;
			}
		}

		bool marker{ false };

		if (shouldSwitchCurrentSpatialLayer)
		{
			// Update current spatial layer.
			this->currentSpatialLayer = this->targetSpatialLayer;

			this->snReferenceSpatialLayer             = packet->GetSequenceNumber();
			this->checkingForOldPacketsInSpatialLayer = true;

			// Update target and current temporal layer.
			this->encodingContext->SetTargetTemporalLayer(this->targetTemporalLayer);
			this->encodingContext->SetCurrentTemporalLayer(packet->GetTemporalLayer());

			// Reset the score of our RtpStream to 10.
			this->rtpStream->ResetScore(10u, /*notify*/ false);

			// Emit the layersChange event.
			EmitLayersChange();

			// Emit the score event.
			EmitScore();

			// Rewrite payload if needed.
			packet->ProcessPayload(this->encodingContext.get(), marker);
		}
		else
		{
			auto previousTemporalLayer = this->encodingContext->GetCurrentTemporalLayer();

			// Rewrite payload if needed. Drop packet if necessary.
			if (!packet->ProcessPayload(this->encodingContext.get(), marker))
			{
				this->rtpSeqManager->Drop(packet->GetSequenceNumber());

#ifdef MS_RTC_LOGGER_RTP
				packet->logger.Dropped(RtcLogger::RtpPacket::DropReason::DROPPED_BY_CODEC);
#endif

				return;
			}

			if (previousTemporalLayer != this->encodingContext->GetCurrentTemporalLayer())
			{
				EmitLayersChange();
			}
		}

		// Update RTP seq number and timestamp based on NTP offset.
		uint16_t seq;
		const uint32_t timestamp = packet->GetTimestamp() - this->tsOffset;

		this->rtpSeqManager->Input(packet->GetSequenceNumber(), seq);

		// Save original packet fields.
		auto origSsrc      = packet->GetSsrc();
		auto origSeq       = packet->GetSequenceNumber();
		auto origTimestamp = packet->GetTimestamp();

		// Rewrite packet.
		packet->SetSsrc(this->rtpParameters.encodings[0].ssrc);
		packet->SetSequenceNumber(seq);
		packet->SetTimestamp(timestamp);

#ifdef MS_RTC_LOGGER_RTP
		packet->logger.sendRtpTimestamp = timestamp;
		packet->logger.sendSeqNumber    = seq;
#endif

		if (isSyncPacket)
		{
			MS_DEBUG_TAG(
			  rtp,
			  "sending sync packet [ssrc:%" PRIu32 ", seq:%" PRIu16 ", ts:%" PRIu32
			  "] from original [ssrc:%" PRIu32 ", seq:%" PRIu16 ", ts:%" PRIu32 "]",
			  packet->GetSsrc(),
			  packet->GetSequenceNumber(),
			  packet->GetTimestamp(),
			  origSsrc,
			  origSeq,
			  origTimestamp);
		}

		// Process the packet.
		if (this->rtpStream->ReceivePacket(packet, sharedPacket))
		{
			if (this->rtpSeqManager->GetMaxOutput() == packet->GetSequenceNumber())
			{
				this->lastSentPacketHasMarker = packet->HasMarker();
			}

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
			  "] from original [ssrc:%" PRIu32 ", seq:%" PRIu16 ", ts:%" PRIu32 "]",
			  packet->GetSsrc(),
			  packet->GetSequenceNumber(),
			  packet->GetTimestamp(),
			  origSsrc,
			  origSeq,
			  origTimestamp);

#ifdef MS_RTC_LOGGER_RTP
			packet->logger.Dropped(RtcLogger::RtpPacket::DropReason::SEND_RTP_STREAM_DISCARDED);
#endif
		}

		// Restore packet fields.
		packet->SetSsrc(origSsrc);
		packet->SetSequenceNumber(origSeq);
		packet->SetTimestamp(origTimestamp);

		// Restore the original payload if needed.
		packet->RestorePayload();
	}

	bool SimulcastConsumer::GetRtcp(RTC::RTCP::CompoundPacket* packet, uint64_t nowMs)
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

	void SimulcastConsumer::NeedWorstRemoteFractionLost(
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

	void SimulcastConsumer::ReceiveNack(RTC::RTCP::FeedbackRtpNackPacket* nackPacket)
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

	void SimulcastConsumer::ReceiveKeyFrameRequest(
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
			RequestKeyFrameForCurrentSpatialLayer();
		}
	}

	void SimulcastConsumer::ReceiveRtcpReceiverReport(RTC::RTCP::ReceiverReport* report)
	{
		MS_TRACE();

		this->rtpStream->ReceiveRtcpReceiverReport(report);
	}

	void SimulcastConsumer::ReceiveRtcpXrReceiverReferenceTime(RTC::RTCP::ReceiverReferenceTime* report)
	{
		MS_TRACE();

		this->rtpStream->ReceiveRtcpXrReceiverReferenceTime(report);
	}

	uint32_t SimulcastConsumer::GetTransmissionRate(uint64_t nowMs)
	{
		MS_TRACE();

		if (!IsActive())
		{
			return 0u;
		}

		return this->rtpStream->GetBitrate(nowMs);
	}

	float SimulcastConsumer::GetRtt() const
	{
		MS_TRACE();

		return this->rtpStream->GetRtt();
	}

	void SimulcastConsumer::UserOnTransportConnected()
	{
		MS_TRACE();

		this->syncRequired                 = true;
		this->spatialLayerToSync           = -1;
		this->keyFrameForTsOffsetRequested = false;

		if (IsActive())
		{
			MayChangeLayers();
		}
	}

	void SimulcastConsumer::UserOnTransportDisconnected()
	{
		MS_TRACE();

		this->lastBweDowngradeAtMs = 0u;

		this->rtpStream->Pause();

		UpdateTargetLayers(-1, -1);
	}

	void SimulcastConsumer::UserOnPaused()
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

	void SimulcastConsumer::UserOnResumed()
	{
		MS_TRACE();

		this->syncRequired                        = true;
		this->spatialLayerToSync                  = -1;
		this->keyFrameForTsOffsetRequested        = false;
		this->checkingForOldPacketsInSpatialLayer = false;

		if (IsActive())
		{
			MayChangeLayers();
		}
	}

	void SimulcastConsumer::CreateRtpStream()
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

	void SimulcastConsumer::RequestKeyFrames()
	{
		MS_TRACE();

		if (this->kind != RTC::Media::Kind::VIDEO)
		{
			return;
		}

		auto* producerTargetRtpStream  = GetProducerTargetRtpStream();
		auto* producerCurrentRtpStream = GetProducerCurrentRtpStream();

		if (producerTargetRtpStream)
		{
			auto mappedSsrc = this->consumableRtpEncodings[this->targetSpatialLayer].ssrc;

			this->listener->OnConsumerKeyFrameRequested(this, mappedSsrc);
		}

		if (producerCurrentRtpStream && producerCurrentRtpStream != producerTargetRtpStream)
		{
			auto mappedSsrc = this->consumableRtpEncodings[this->currentSpatialLayer].ssrc;

			this->listener->OnConsumerKeyFrameRequested(this, mappedSsrc);
		}
	}

	void SimulcastConsumer::RequestKeyFrameForTargetSpatialLayer()
	{
		MS_TRACE();

		if (this->kind != RTC::Media::Kind::VIDEO)
		{
			return;
		}

		auto* producerTargetRtpStream = GetProducerTargetRtpStream();

		if (!producerTargetRtpStream)
		{
			return;
		}

		auto mappedSsrc = this->consumableRtpEncodings[this->targetSpatialLayer].ssrc;

		this->listener->OnConsumerKeyFrameRequested(this, mappedSsrc);
	}

	void SimulcastConsumer::RequestKeyFrameForCurrentSpatialLayer()
	{
		MS_TRACE();

		if (this->kind != RTC::Media::Kind::VIDEO)
		{
			return;
		}

		auto* producerCurrentRtpStream = GetProducerCurrentRtpStream();

		if (!producerCurrentRtpStream)
		{
			return;
		}

		auto mappedSsrc = this->consumableRtpEncodings[this->currentSpatialLayer].ssrc;

		this->listener->OnConsumerKeyFrameRequested(this, mappedSsrc);
	}

	void SimulcastConsumer::MayChangeLayers(bool force)
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
				if (newTargetSpatialLayer != this->targetSpatialLayer || force)
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

	bool SimulcastConsumer::RecalculateTargetLayers(
	  int16_t& newTargetSpatialLayer, int16_t& newTargetTemporalLayer) const
	{
		MS_TRACE();

		// Start with no layers.
		newTargetSpatialLayer  = -1;
		newTargetTemporalLayer = -1;

		auto nowMs = DepLibUV::GetTimeMs();

		for (size_t sIdx{ 0u }; sIdx < this->producerRtpStreams.size(); ++sIdx)
		{
			auto spatialLayer       = static_cast<int16_t>(sIdx);
			auto* producerRtpStream = this->producerRtpStreams.at(sIdx);
			auto producerScore      = producerRtpStream ? producerRtpStream->GetScore() : 0u;

			// If this is higher than current spatial layer and we moved to to current spatial
			// layer due to BWE limitations, check how much it has elapsed since then.
			if (nowMs - this->lastBweDowngradeAtMs < BweDowngradeConservativeMs)
			{
				if (newTargetSpatialLayer > -1 && spatialLayer > this->currentSpatialLayer)
				{
					continue;
				}
			}

			// Ignore spatial layers for non existing Producer streams or for those
			// with score 0.
			if (producerScore == 0u)
			{
				continue;
			}

			// If the stream has not been active time enough and we have an active one
			// already, move to the next spatial layer.
			// NOTE: Require bitrate externally managed for this.
			// clang-format off
			if (
				this->externallyManagedBitrate &&
				newTargetSpatialLayer != -1 &&
				producerRtpStream->GetActiveMs() < StreamMinActiveMs
			)
			// clang-format on
			{
				continue;
			}

			// We may not yet switch to this spatial layer.
			if (!CanSwitchToSpatialLayer(spatialLayer))
			{
				continue;
			}

			newTargetSpatialLayer = spatialLayer;

			// If this is the preferred or higher spatial layer take it and exit.
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

		// Return true if any target layer changed.
		// clang-format off
		return (
			newTargetSpatialLayer != this->targetSpatialLayer ||
			newTargetTemporalLayer != this->targetTemporalLayer
		);
		// clang-format on
	}

	void SimulcastConsumer::UpdateTargetLayers(int16_t newTargetSpatialLayer, int16_t newTargetTemporalLayer)
	{
		MS_TRACE();

		// If we don't have yet a RTP timestamp reference, set it now.
		if (
		  newTargetSpatialLayer != -1 && (this->tsReferenceSpatialLayer == -1 ||
		                                  !GetProducerTsReferenceRtpStream()->GetSenderReportNtpMs()))
		{
			MS_DEBUG_TAG(
			  simulcast, "using spatial layer %" PRIi16 " as RTP timestamp reference", newTargetSpatialLayer);

			this->tsReferenceSpatialLayer = newTargetSpatialLayer;
		}

		if (newTargetSpatialLayer == -1)
		{
			// Unset current and target layers.
			this->targetSpatialLayer  = -1;
			this->targetTemporalLayer = -1;
			this->currentSpatialLayer = -1;

			this->encodingContext->SetTargetTemporalLayer(-1);
			this->encodingContext->SetCurrentTemporalLayer(-1);

			MS_DEBUG_TAG(
			  simulcast, "target layers changed [spatial:-1, temporal:-1, consumerId:%s]", this->id.c_str());

			EmitLayersChange();

			return;
		}

		this->targetSpatialLayer  = newTargetSpatialLayer;
		this->targetTemporalLayer = newTargetTemporalLayer;

		// If the new target spatial layer matches the current one, apply the new
		// target temporal layer now.
		if (this->targetSpatialLayer == this->currentSpatialLayer)
		{
			this->encodingContext->SetTargetTemporalLayer(this->targetTemporalLayer);
		}

		MS_DEBUG_TAG(
		  simulcast,
		  "target layers changed [spatial:%" PRIi16 ", temporal:%" PRIi16 ", consumerId:%s]",
		  this->targetSpatialLayer,
		  this->targetTemporalLayer,
		  this->id.c_str());

		// If the target spatial layer is different than the current one, request
		// a key frame.
		if (this->targetSpatialLayer != this->currentSpatialLayer)
		{
			RequestKeyFrameForTargetSpatialLayer();
		}
	}

	inline bool SimulcastConsumer::CanSwitchToSpatialLayer(int16_t spatialLayer) const
	{
		MS_TRACE();

		// This method assumes that the caller has verified that there is a valid
		// Producer RtpStream for the given spatial layer.
		MS_ASSERT(
		  this->producerRtpStreams.at(spatialLayer),
		  "no Producer RtpStream for the given spatialLayer:%" PRIi16,
		  spatialLayer);

		// We can switch to the given spatial layer if:
		// - we don't have any TS reference spatial layer yet, or
		// - the given spatial layer matches the TS reference spatial layer, or
		// - both , the RTP streams of our TS reference spatial layer and the given
		//   spatial layer, have Sender Report.
		//
		// clang-format off
		return (
			this->tsReferenceSpatialLayer == -1 ||
			spatialLayer == this->tsReferenceSpatialLayer ||
			this->producerRtpStreams.at(spatialLayer)->GetSenderReportNtpMs()
		);
		// clang-format on
	}

	inline void SimulcastConsumer::EmitScore() const
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

	inline void SimulcastConsumer::EmitLayersChange() const
	{
		MS_TRACE();

		MS_DEBUG_DEV(
		  "current layers changed to [spatial:%" PRIi16 ", temporal:%" PRIi16 ", consumerId:%s]",
		  this->currentSpatialLayer,
		  this->encodingContext->GetCurrentTemporalLayer(),
		  this->id.c_str());

		flatbuffers::Offset<FBS::Consumer::ConsumerLayers> layersOffset;

		if (this->currentSpatialLayer >= 0)
		{
			layersOffset = FBS::Consumer::CreateConsumerLayers(
			  this->shared->channelNotifier->GetBufferBuilder(),
			  this->currentSpatialLayer,
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

	inline RTC::RtpStreamRecv* SimulcastConsumer::GetProducerCurrentRtpStream() const
	{
		MS_TRACE();

		if (this->currentSpatialLayer == -1)
		{
			return nullptr;
		}

		// This may return nullptr.
		return this->producerRtpStreams.at(this->currentSpatialLayer);
	}

	inline RTC::RtpStreamRecv* SimulcastConsumer::GetProducerTargetRtpStream() const
	{
		MS_TRACE();

		if (this->targetSpatialLayer == -1)
		{
			return nullptr;
		}

		// This may return nullptr.
		return this->producerRtpStreams.at(this->targetSpatialLayer);
	}

	inline RTC::RtpStreamRecv* SimulcastConsumer::GetProducerTsReferenceRtpStream() const
	{
		MS_TRACE();

		if (this->tsReferenceSpatialLayer == -1)
		{
			return nullptr;
		}

		// This may return nullptr.
		return this->producerRtpStreams.at(this->tsReferenceSpatialLayer);
	}

	inline void SimulcastConsumer::OnRtpStreamScore(
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

	inline void SimulcastConsumer::OnRtpStreamRetransmitRtpPacket(
	  RTC::RtpStreamSend* /*rtpStream*/, RTC::RtpPacket* packet)
	{
		MS_TRACE();

		this->listener->OnConsumerRetransmitRtpPacket(this, packet);

		// May emit 'trace' event.
		EmitTraceEventRtpAndKeyFrameTypes(packet, this->rtpStream->HasRtx());
	}
} // namespace RTC
