#define MS_CLASS "RTC::Producer"
// #define MS_LOG_DEV

#include "RTC/Producer.hpp"
#include "Logger.hpp"
#include "MediaSoupErrors.hpp"
#include "RTC/RTCP/FeedbackPsFir.hpp"
#include "RTC/RTCP/FeedbackPsPli.hpp"
#include "RTC/RTCP/FeedbackRtp.hpp"
#include "RTC/RTCP/FeedbackRtpNack.hpp"

namespace RTC
{
	/* Static. */

	static uint8_t ClonedPacketBuffer[RTC::RtpBufferSize];

	/* Instance methods. */

	Producer::Producer(
	  const std::string& id,
	  Listener* listener,
	  RTC::Media::Kind kind,
	  RTC::RtpParameters& rtpParameters,
	  struct RtpMapping& rtpMapping)
	  : id(id), listener(listener), kind(kind), rtpParameters(rtpParameters), rtpMapping(rtpMapping)
	{
		MS_TRACE();

		// Fill RTP header extension ids and their mapped values.
		// This may throw.
		for (auto& exten : this->rtpParameters.headerExtensions)
		{
			auto it = this->rtpMapping.headerExtensions.find(exten.id);

			// This should not happen since rtpMapping has been made reading the rtpParameters.
			if (it == this->rtpMapping.headerExtensions.end())
				MS_THROW_TYPE_ERROR("RTP extension id not present in the RTP mapping");

			auto mappedId = it->second;

			if (this->rtpHeaderExtensionIds.ssrcAudioLevel == 0u && exten.type == RTC::RtpHeaderExtensionUri::Type::SSRC_AUDIO_LEVEL)
			{
				this->rtpHeaderExtensionIds.ssrcAudioLevel       = exten.id;
				this->mappedRtpHeaderExtensionIds.ssrcAudioLevel = mappedId;
			}

			if (this->rtpHeaderExtensionIds.absSendTime == 0u && exten.type == RTC::RtpHeaderExtensionUri::Type::ABS_SEND_TIME)
			{
				this->rtpHeaderExtensionIds.absSendTime       = exten.id;
				this->mappedRtpHeaderExtensionIds.absSendTime = mappedId;
			}

			if (this->rtpHeaderExtensionIds.mid == 0u && exten.type == RTC::RtpHeaderExtensionUri::Type::MID)
			{
				this->rtpHeaderExtensionIds.mid       = exten.id;
				this->mappedRtpHeaderExtensionIds.mid = mappedId;
			}

			if (this->rtpHeaderExtensionIds.rid == 0u && exten.type == RTC::RtpHeaderExtensionUri::Type::RTP_STREAM_ID)
			{
				this->rtpHeaderExtensionIds.rid       = exten.id;
				this->mappedRtpHeaderExtensionIds.rid = mappedId;
			}
		}

		// Set the RTCP report generation interval.
		if (this->kind == RTC::Media::Kind::AUDIO)
			this->maxRtcpInterval = RTC::RTCP::MaxAudioIntervalMs;
		else
			this->maxRtcpInterval = RTC::RTCP::MaxVideoIntervalMs;

		// TODO: Uncmment when done.
		// this->keyFrameRequestManager = new RTC::KeyFrameRequestManager(this);
	}

	Producer::~Producer()
	{
		MS_TRACE();

		// Delete all streams.
		for (auto& kv : this->mapSsrcRtpStream)
		{
			auto* rtpStream = kv.second;

			delete rtpStream;
		}

		this->mapSsrcRtpStream.clear();
		this->mapRtxSsrcRtpStream.clear();
		this->mapRtpStreamMappedSsrc.clear();
		this->mapMappedSsrcSsrc.clear();
		this->healthyRtpStreams.clear();

		// TODO: Uncmment when done.
		// Delete the KeyFrameRequestManager().
		// delete this->keyFrameRequestManager;
	}

	void Producer::FillJson(json& jsonObject) const
	{
		MS_TRACE();

		// Add id.
		jsonObject["id"] = this->id;

		// Add kind.
		jsonObject["kind"] = RTC::Media::GetString(this->kind);

		// Add rtpParameters.
		this->rtpParameters.FillJson(jsonObject["rtpParameters"]);

		// Add rtpMapping.
		jsonObject["rtpMapping"] = json::object();
		auto jsonRtpMappingIt    = jsonObject.find("rtpMapping");

		// Add rtpMapping.codecs.
		{
			(*jsonRtpMappingIt)["codecs"] = json::array();
			auto jsonCodecsIt             = jsonRtpMappingIt->find("codecs");
			size_t idx                    = 0;

			for (auto& kv : this->rtpMapping.codecs)
			{
				jsonCodecsIt->emplace_back(json::value_t::object);

				auto& jsonEntry        = (*jsonCodecsIt)[idx];
				auto payloadType       = kv.first;
				auto mappedPayloadType = kv.second;

				jsonEntry[payloadType] = mappedPayloadType;

				++idx;
			}
		}

		// Add rtpMapping.headerExtensions.
		{
			(*jsonRtpMappingIt)["headerExtensions"] = json::array();
			auto jsonHeaderExtensionsIt             = jsonRtpMappingIt->find("headerExtensions");
			size_t idx                              = 0;

			for (auto& kv : this->rtpMapping.headerExtensions)
			{
				jsonHeaderExtensionsIt->emplace_back(json::value_t::object);

				auto& jsonEntry = (*jsonHeaderExtensionsIt)[idx];
				auto id         = kv.first;
				auto mappedId   = kv.second;

				jsonEntry[id] = mappedId;

				++idx;
			}
		}

		// Add rtpMapping.encodings.
		{
			(*jsonRtpMappingIt)["encodings"] = json::array();
			auto jsonEncodingsIt             = jsonRtpMappingIt->find("encodings");

			for (size_t i = 0; i < this->rtpMapping.encodings.size(); ++i)
			{
				jsonEncodingsIt->emplace_back(json::value_t::object);

				auto& jsonEntry       = (*jsonEncodingsIt)[i];
				auto& encodingMapping = this->rtpMapping.encodings[i];

				if (!encodingMapping.rid.empty())
					jsonEntry["rid"] = encodingMapping.rid;
				else
					jsonEntry["rid"] = nullptr;

				if (encodingMapping.ssrc != 0u)
					jsonEntry["ssrc"] = encodingMapping.ssrc;
				else
					jsonEntry["ssrc"] = nullptr;

				if (encodingMapping.rtxSsrc != 0u)
					jsonEntry["rtxSsrc"] = encodingMapping.rtxSsrc;
				else
					jsonEntry["rtxSsrc"] = nullptr;

				jsonEntry["mappedSsrc"] = encodingMapping.mappedSsrc;
			}
		}

		// Add rtpStreams.
		jsonObject["rtpStreams"] = json::array();
		auto jsonRtpStreamsIt    = jsonObject.find("rtpStreams");
		size_t rtpStreamIdx      = 0;
		float lossPercentage     = 0;

		for (auto& kv : this->mapSsrcRtpStream)
		{
			jsonRtpStreamsIt->emplace_back(json::value_t::object);

			auto& jsonEntry = (*jsonRtpStreamsIt)[rtpStreamIdx];
			auto* rtpStream = kv.second;

			rtpStream->FillJson(jsonEntry);

			lossPercentage += rtpStream->GetLossPercentage();
			++rtpStreamIdx;
		}

		if (!this->mapSsrcRtpStream.empty())
			lossPercentage = lossPercentage / this->mapSsrcRtpStream.size();

		// Add lossPercentage.
		jsonObject["lossPercentage"] = lossPercentage;

		// Add paused.
		jsonObject["paused"] = this->paused;
	}

	void Producer::FillJsonStats(json& jsonArray) const
	{
		MS_TRACE();

		size_t rtpStreamIdx = 0;

		for (auto& kv : this->mapSsrcRtpStream)
		{
			jsonArray.emplace_back(json::value_t::object);

			auto& jsonEntry = jsonArray[rtpStreamIdx];
			auto* rtpStream = kv.second;

			rtpStream->FillJsonStats(jsonEntry);

			++rtpStreamIdx;
		}
	}

	void Producer::HandleRequest(Channel::Request* request)
	{
		MS_TRACE();

		switch (request->methodId)
		{
			case Channel::Request::MethodId::PRODUCER_DUMP:
			{
				json data{ json::object() };

				FillJson(data);

				request->Accept(data);

				break;
			}

			case Channel::Request::MethodId::PRODUCER_GET_STATS:
			{
				json data{ json::array() };

				FillJsonStats(data);

				request->Accept(data);

				break;
			}

			case Channel::Request::MethodId::PRODUCER_PAUSE:
			{
				if (!this->paused)
				{
					this->paused = true;

					MS_DEBUG_DEV("Producer paused [producerId:%s]", this->producerId.c_str());

					this->listener->OnProducerPaused(this);
				}

				request->Accept();

				break;
			}

			case Channel::Request::MethodId::PRODUCER_RESUME:
			{
				if (this->paused)
				{
					this->paused = false;

					MS_DEBUG_DEV("Producer resumed [producerId:%s]", this->producerId.c_str());

					this->listener->OnProducerResumed(this);

					if (this->kind == RTC::Media::Kind::VIDEO)
					{
						MS_DEBUG_2TAGS(rtcp, rtx, "requesting forced key frame after resumed");

						// Request a key frame for all streams.
						for (auto& kv : this->mapSsrcRtpStream)
						{
							auto ssrc = kv.first;

							// TODO: Uncomment when done.
							// this->keyFrameRequestManager->ForceKeyFrameNeeded(ssrc);
						}
					}
				}

				request->Accept();

				break;
			}

				// TODO: More.

			default:
			{
				MS_THROW_ERROR("unknown method '%s'", request->method.c_str());
			}
		}
	}

	void Producer::ReceiveRtpPacket(RTC::RtpPacket* packet)
	{
		MS_TRACE();

		auto* rtpStream = GetRtpStream(packet);

		if (!rtpStream)
		{
			MS_WARN_TAG(rtp, "no stream found for received packet [ssrc:%" PRIu32 "]", packet->GetSsrc());

			return;
		}

		// Let's clone the RTP packet so we can mangle the payload (if needed) and other
		// stuff that would change its size.
		std::unique_ptr<RTC::RtpPacket> clonedPacket;

		clonedPacket.reset(packet->Clone(ClonedPacketBuffer));
		packet = clonedPacket.get();

		// Media packet.
		if (packet->GetSsrc() == rtpStream->GetSsrc())
		{
			// Process the packet.
			if (!rtpStream->ReceivePacket(packet))
				return;
		}
		// RTX packet.
		else if (packet->GetSsrc() == rtpStream->GetRtxSsrc())
		{
			// Process the packet.
			if (!rtpStream->ReceiveRtxPacket(packet))
				return;

			// Packet repaired after applying RTX.
			rtpStream->packetsRepaired++;
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

			// TODO: Uncomment when done.
			// Tell the keyFrameRequestManager.
			// this->keyFrameRequestManager->KeyFrameReceived(packet->GetSsrc());
		}

		// If paused stop here.
		if (this->paused)
			return;

		// Mangle the packet before providing the listener with it.
		MangleRtpPacket(packet, rtpStream);

		this->listener->OnProducerRtpPacketReceived(this, packet);
	}

	void Producer::ReceiveRtcpSenderReport(RTC::RTCP::SenderReport* report)
	{
		auto it = this->mapSsrcRtpStream.find(report->GetSsrc());

		if (it == this->mapSsrcRtpStream.end())
		{
			MS_WARN_TAG(rtcp, "stream not found");

			return;
		}

		auto* rtpStream = it->second;

		rtpStream->ReceiveRtcpSenderReport(report);
	}

	void Producer::GetRtcp(RTC::RTCP::CompoundPacket* packet, uint64_t now)
	{
		if (static_cast<float>((now - this->lastRtcpSentTime) * 1.15) < this->maxRtcpInterval)
			return;

		for (auto& kv : this->mapSsrcRtpStream)
		{
			auto* rtpStream = kv.second;
			auto* report    = rtpStream->GetRtcpReceiverReport();

			report->SetSsrc(rtpStream->GetSsrc());
			packet->AddReceiverReport(report);
		}

		this->lastRtcpSentTime = now;
	}

	void Producer::RequestKeyFrame(uint32_t mappedSsrc)
	{
		MS_TRACE();

		if (this->kind != RTC::Media::Kind::VIDEO || this->paused)
			return;

		auto it = this->mapMappedSsrcSsrc.find(mappedSsrc);

		if (it == this->mapMappedSsrcSsrc.end())
		{
			MS_WARN_2TAGS(rtcp, rtx, "given mappedSsrc not found, ignoring");

			return;
		}

		uint32_t ssrc = it->second;

		// TODO: Uncomment when done.
		// this->keyFrameRequestManager->KeyFrameNeeded(ssrc);
	}

	RTC::RtpStreamRecv* Producer::GetRtpStream(RTC::RtpPacket* packet)
	{
		MS_TRACE();

		uint32_t ssrc       = packet->GetSsrc();
		uint8_t payloadType = packet->GetPayloadType();

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
		for (size_t i = 0; i < this->rtpParameters.encodings.size(); ++i)
		{
			auto& encoding     = this->rtpParameters.encodings[i];
			auto* mediaCodec   = this->rtpParameters.GetCodecForEncoding(encoding);
			auto* rtxCodec     = this->rtpParameters.GetRtxCodecForEncoding(encoding);
			bool isMediaPacket = (mediaCodec->payloadType == payloadType);
			bool isRtxPacket   = (rtxCodec && rtxCodec->payloadType == payloadType);

			if (isMediaPacket && encoding.ssrc == ssrc)
			{
				auto* rtpStream = CreateRtpStream(ssrc, *mediaCodec, i);

				return rtpStream;
			}
			else if (isRtxPacket && encoding.hasRtx && encoding.rtx.ssrc == ssrc)
			{
				auto it = this->mapSsrcRtpStream.find(encoding.ssrc);

				// Ignore if no stream has been created yet for the corresponding encoding.
				if (it == this->mapSsrcRtpStream.end())
				{
					MS_DEBUG_2TAGS(rtp, rtx, "ignoring RTX packet for not yet created stream (ssrc lookup)");

					return nullptr;
				}

				auto* rtpStream = it->second;

				// Ensure no RTX SSRC was previously detected.
				if (rtpStream->HasRtx())
				{
					MS_DEBUG_2TAGS(rtp, rtx, "ignoring RTX packet with new ssrc (ssrc lookup)");

					return nullptr;
				}

				// Update the stream RTX data.
				rtpStream->SetRtx(payloadType, ssrc);

				// Insert the new RTX SSRC into the map.
				this->mapRtxSsrcRtpStream[ssrc] = rtpStream;

				return rtpStream;
			}
		}

		// If not found, look for an encoding matching the packet RID value.
		const uint8_t* ridPtr;
		size_t ridLen;

		if (packet->ReadRid(&ridPtr, &ridLen))
		{
			auto* charRidPtr = reinterpret_cast<const char*>(ridPtr);
			std::string rid(charRidPtr, ridLen);

			for (size_t i = 0; i < this->rtpParameters.encodings.size(); ++i)
			{
				auto& encoding = this->rtpParameters.encodings[i];

				if (encoding.rid != rid)
					continue;

				auto* mediaCodec   = this->rtpParameters.GetCodecForEncoding(encoding);
				auto* rtxCodec     = this->rtpParameters.GetRtxCodecForEncoding(encoding);
				bool isMediaPacket = (mediaCodec->payloadType == payloadType);
				bool isRtxPacket   = (rtxCodec && rtxCodec->payloadType == payloadType);

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

					auto* rtpStream = CreateRtpStream(ssrc, *mediaCodec, i);

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
							// Ensure no RTX SSRC was previously detected.
							if (rtpStream->HasRtx())
							{
								MS_DEBUG_2TAGS(rtp, rtx, "ignoring RTX packet with new SSRC (RID lookup)");

								return nullptr;
							}

							// Update the stream RTX data.
							rtpStream->SetRtx(payloadType, ssrc);

							// Insert the new RTX SSRC into the map.
							this->mapRtxSsrcRtpStream[ssrc] = rtpStream;

							return rtpStream;
						}
					}

					MS_DEBUG_2TAGS(rtp, rtx, "ignoring RTX packet for not yet created stream (RID lookup)");

					return nullptr;
				}
			}

			MS_WARN_TAG(rtp, "ignoring packet with unknown RID (RID lookup)");

			return nullptr;
		}

		return nullptr;
	}

	RTC::RtpStreamRecv* Producer::CreateRtpStream(
	  uint32_t ssrc, const RTC::RtpCodecParameters& codec, size_t encodingIdx)
	{
		MS_TRACE();

		MS_ASSERT(
		  this->mapSsrcRtpStream.find(ssrc) == this->mapSsrcRtpStream.end(), "stream already exists");

		auto& encoding        = this->rtpParameters.encodings[encodingIdx];
		auto& encodingMapping = this->rtpMapping.encodings[encodingIdx];

		// Set stream params.
		RTC::RtpStream::Params params;

		params.ssrc        = ssrc;
		params.payloadType = codec.payloadType;
		params.mimeType    = codec.mimeType;
		params.clockRate   = codec.clockRate;
		params.rid         = encoding.rid;

		for (auto& fb : codec.rtcpFeedback)
		{
			if (!params.useNack && fb.type == "nack")
			{
				MS_DEBUG_2TAGS(rtcp, rtx, "NACK supported");

				params.useNack = true;
			}
			if (!params.usePli && fb.type == "nack" && fb.parameter == "pli")
			{
				MS_DEBUG_TAG(rtcp, "PLI supported");

				params.usePli = true;
			}
			if (!params.useFir && fb.type == "ccm" && fb.parameter == "fir")
			{
				MS_DEBUG_TAG(rtcp, "FIR supported");

				params.useFir = true;
			}
		}

		// Create a RtpStreamRecv for receiving a media stream.
		auto* rtpStream = new RTC::RtpStreamRecv(this, params);

		// Insert into the map.
		this->mapSsrcRtpStream[ssrc] = rtpStream;

		// Set the mapped SSRC.
		this->mapRtpStreamMappedSsrc[rtpStream]             = encodingMapping.mappedSsrc;
		this->mapMappedSsrcSsrc[encodingMapping.mappedSsrc] = ssrc;

		// Set stream as healthy.
		SetHealthyStream(rtpStream);

		// Request a key frame for this stream since we may have lost the first packets.
		// TODO: Uncomment when done.
		// this->keyFrameRequestManager->ForceKeyFrameNeeded(ssrc);

		return rtpStream;
	}

	void Producer::SetHealthyStream(RTC::RtpStreamRecv* rtpStream)
	{
		MS_TRACE();

		if (this->healthyRtpStreams.find(rtpStream) != this->healthyRtpStreams.end())
			return;

		this->healthyRtpStreams.insert(rtpStream);

		uint32_t mappedSsrc = this->mapRtpStreamMappedSsrc.at(rtpStream);

		// Notify the listener.
		this->listener->OnProducerRtpStreamHealthy(this, rtpStream, mappedSsrc);
	}

	void Producer::SetUnhealthyStream(RTC::RtpStreamRecv* rtpStream)
	{
		MS_TRACE();

		if (this->healthyRtpStreams.find(rtpStream) == this->healthyRtpStreams.end())
			return;

		this->healthyRtpStreams.erase(rtpStream);

		uint32_t mappedSsrc = this->mapRtpStreamMappedSsrc.at(rtpStream);

		// Notify the listener.
		this->listener->OnProducerRtpStreamUnhealthy(this, rtpStream, mappedSsrc);
	}

	// TODO: Must set mapped ssrc!
	void Producer::MangleRtpPacket(RTC::RtpPacket* packet, RTC::RtpStreamRecv* rtpStream) const
	{
		MS_TRACE();

		// Mangle the payload type.
		uint8_t payloadType = packet->GetPayloadType();
		auto it             = this->rtpMapping.codecs.find(payloadType);

		if (it != this->rtpMapping.codecs.end())
		{
			uint8_t mappedPayloadType = it->second;

			packet->SetPayloadType(mappedPayloadType);
		}

		// Mangle RTP header extension ids.
		if (this->mappedRtpHeaderExtensionIds.ssrcAudioLevel != 0u)
			packet->SetAudioLevelExtensionId(this->mappedRtpHeaderExtensionIds.ssrcAudioLevel);

		if (this->mappedRtpHeaderExtensionIds.absSendTime != 0u)
			packet->SetAbsSendTimeExtensionId(this->mappedRtpHeaderExtensionIds.absSendTime);

		if (this->mappedRtpHeaderExtensionIds.mid != 0u)
			packet->SetMidExtensionId(this->mappedRtpHeaderExtensionIds.mid);

		if (this->mappedRtpHeaderExtensionIds.rid != 0u)
			packet->SetRidExtensionId(this->mappedRtpHeaderExtensionIds.rid);

		// Mangle the SSRC.
		uint32_t mappedSsrc = this->mapRtpStreamMappedSsrc.at(rtpStream);

		packet->SetSsrc(mappedSsrc);
	}

	void Producer::OnRtpStreamRecvNackRequired(
	  RTC::RtpStreamRecv* rtpStream, const std::vector<uint16_t>& seqNumbers)
	{
		MS_TRACE();

		RTC::RTCP::FeedbackRtpNackPacket packet(0, rtpStream->GetSsrc());

		auto it        = seqNumbers.begin();
		const auto end = seqNumbers.end();
		size_t numPacketsRequested{ 0 };

		while (it != end)
		{
			uint16_t seq;
			uint16_t bitmask{ 0 };

			seq = *it;
			++it;

			while (it != end)
			{
				uint16_t shift = *it - seq - 1;

				if (shift <= 15)
				{
					bitmask |= (1 << shift);
					++it;
				}
				else
				{
					break;
				}
			}

			auto* nackItem = new RTC::RTCP::FeedbackRtpNackItem(seq, bitmask);

			packet.AddItem(nackItem);

			numPacketsRequested += nackItem->CountRequestedPackets();
		}

		// Ensure that the RTCP packet fits into the RTCP buffer.
		if (packet.GetSize() > RTC::RTCP::BufferSize)
		{
			MS_WARN_TAG(rtx, "cannot send RTCP NACK packet, size too big (%zu bytes)", packet.GetSize());

			return;
		}

		packet.Serialize(RTC::RTCP::Buffer);

		this->listener->OnProducerSendRtcpPacket(this, &packet);

		rtpStream->nackCount++;
		rtpStream->nackRtpPacketCount += numPacketsRequested;
	}

	void Producer::OnRtpStreamRecvPliRequired(RTC::RtpStreamRecv* rtpStream)
	{
		MS_TRACE();

		MS_DEBUG_2TAGS(rtcp, rtx, "sending PLI [ssrc:%" PRIu32 "]", rtpStream->GetSsrc());

		RTC::RTCP::FeedbackPsPliPacket packet(0, rtpStream->GetSsrc());

		packet.Serialize(RTC::RTCP::Buffer);

		this->listener->OnProducerSendRtcpPacket(this, &packet);

		rtpStream->pliCount++;
	}

	void Producer::OnRtpStreamRecvFirRequired(RTC::RtpStreamRecv* rtpStream)
	{
		MS_TRACE();

		MS_DEBUG_2TAGS(rtcp, rtx, "sending FIR [ssrc:%" PRIu32 "]", rtpStream->GetSsrc());

		RTC::RTCP::FeedbackPsFirPacket packet(0, rtpStream->GetSsrc());
		auto* item = new RTC::RTCP::FeedbackPsFirItem(rtpStream->GetSsrc(), rtpStream->GetFirSeqNumber());

		packet.AddItem(item);
		packet.Serialize(RTC::RTCP::Buffer);

		this->listener->OnProducerSendRtcpPacket(this, &packet);

		rtpStream->pliCount++;
	}

	void Producer::OnRtpStreamHealthy(RTC::RtpStream* rtpStream)
	{
		MS_TRACE();

		auto* rtpStreamRecv = dynamic_cast<RtpStreamRecv*>(rtpStream);

		SetHealthyStream(rtpStreamRecv);
	}

	void Producer::OnRtpStreamUnhealthy(RTC::RtpStream* rtpStream)
	{
		MS_TRACE();

		auto* rtpStreamRecv = dynamic_cast<RtpStreamRecv*>(rtpStream);

		SetUnhealthyStream(rtpStreamRecv);
	}

	// void Producer::OnKeyFrameNeeded(KeyFrameRequestManager* /*keyFrameRequestManager*/, uint32_t ssrc)
	// {
	// 	MS_TRACE();

	// 	auto it = this->mapSsrcRtpStream.find(ssrc);

	// 	if (it == this->mapSsrcRtpStream.end())
	// 	{
	// 		MS_WARN_2TAGS(rtcp, rtx, "no associated stream found [ssrc:%" PRIu32 "]", ssrc);

	// 		return;
	// 	}

	// 	auto* rtpStream = it->second;

	// 	rtpStream->RequestKeyFrame();
	// }
} // namespace RTC
