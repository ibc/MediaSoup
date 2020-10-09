#define MS_CLASS "RTC::ShmConsumer"
// #define MS_LOG_DEV

#include "RTC/ShmConsumer.hpp"
#include "DepLibUV.hpp"
#include "Logger.hpp"
#include "MediaSoupErrors.hpp"
#include "Channel/Notifier.hpp"
#include "RTC/Codecs/Tools.hpp"
#include "RTC/RTCP/FeedbackRtpNack.hpp"
#include "RTC/RtpStreamRecv.hpp"


namespace RTC
{
	void RtpLostPktRateCounter::Update(RTC::RtpPacket* packet)
	{
		// Each packet if in order will increate number of total received packets
		// If there is gap in seqIds also increase number of lost packets
		// If packet's seqId is older then do not update any counters
		size_t lost    = 0;
		size_t total   = 0;
		uint64_t nowMs = DepLibUV::GetTimeMs();
		uint64_t seq   = static_cast<uint64_t>(packet->GetSequenceNumber());

		// The very first packet, no loss yet, increment total
		if (0u == this->firstSeqId)
		{
			total = 1;
			this->firstSeqId = this->lastSeqId = seq;			
		}
		else if (this->lastSeqId >= seq)
		{
			return; // probably retransmission of earlier lost pkt, ignore
		}
		else
		{
			// Packet in order, check for gaps in seq ids and update the counters
			lost = seq - this->lastSeqId - 1;
			total = seq - this->lastSeqId;
			this->lastSeqId = seq;
		}
		
		this->totalPackets.Update(total, nowMs);
		this->lostPackets.Update(lost, nowMs);
	}


	ShmConsumer::ShmConsumer(const std::string& id, const std::string& producerId, RTC::Consumer::Listener* listener, json& data, DepLibSfuShm::ShmCtx *shmCtx)
	  : RTC::Consumer::Consumer(id, producerId, listener, data, RTC::RtpParameters::Type::SHM)
	{
		MS_TRACE();

		// Ensure there is a single encoding.
		if (this->consumableRtpEncodings.size() != 1)
			MS_THROW_TYPE_ERROR("invalid consumableRtpEncodings with size != 1");

		auto& encoding   = this->rtpParameters.encodings[0];
		auto* mediaCodec = this->rtpParameters.GetCodecForEncoding(encoding);
		if (!RTC::Codecs::Tools::IsValidTypeForCodec(this->type, mediaCodec->mimeType))
		{
			MS_THROW_TYPE_ERROR("%s codec not supported for shm", mediaCodec->mimeType.ToString().c_str());
		}

		MS_DEBUG_TAG(rtp, "ShmConsumer ctor() data [%s] media codec [%s]", data.dump().c_str(), mediaCodec->mimeType.ToString().c_str());

		this->keyFrameSupported = RTC::Codecs::Tools::CanBeKeyFrame(mediaCodec->mimeType);

		this->shmCtx = shmCtx;
		this->shmCtx->SetListener(this);


		// Uncomment for NACK test simulation
		uint64_t nowTs = DepLibUV::GetTimeMs();
		this->lastNACKTestTs = nowTs;


		CreateRtpStream();
	}

	ShmConsumer::~ShmConsumer()
	{
		MS_TRACE();

		delete this->rtpStream;
	}

	void ShmConsumer::FillJson(json& jsonObject) const
	{
		MS_TRACE();

		// Call the parent method.
		RTC::Consumer::FillJson(jsonObject);

		// Add rtpStream.
	  this->rtpStream->FillJson(jsonObject["rtpStream"]);
	}

	void ShmConsumer::FillJsonStats(json& jsonArray) const
	{
		MS_TRACE();

		// Add stats of our send stream.
	  jsonArray.emplace_back(json::value_t::object);
		this->rtpStream->FillJsonStats(jsonArray[0]);

		// Add stats of our recv stream.
		if (this->producerRtpStream)
		{
			jsonArray.emplace_back(json::value_t::object);
			this->producerRtpStream->FillJsonStats(jsonArray[1]);
		}

		// Shm writing stats
	  jsonArray.emplace_back(json::value_t::object);
		this->FillShmWriterStats(jsonArray[2]);
	}

	void ShmConsumer::FillShmWriterStats(json& jsonObject) const
	{
		MS_TRACE();
		
		uint64_t nowMs = DepLibUV::GetTimeMs();
		RTC::RtpLostPktRateCounter* loss = const_cast<RTC::RtpLostPktRateCounter*>(&this->lostPktRateCounter);
		uint64_t totalRate = loss->GetTotalRate(nowMs);
		uint64_t lossRate = loss->GetLossRate(nowMs);

		RTC::RtpDataCounter* ptr       = const_cast<RTC::RtpDataCounter*>(&this->shmWriterCounter);
		RTC::RtpStreamRecv* recvStream = dynamic_cast<RTC::RtpStreamRecv*>(this->producerRtpStream);
		jsonObject["type"]             = "shm-writer-stats";
		jsonObject["packetCount"]      = ptr->GetPacketCount();
		jsonObject["byteCount"]        = ptr->GetBytes();
		jsonObject["bitrate"]          = ptr->GetBitrate(nowMs);
		jsonObject["packetLossRate"]   = totalRate != 0 ? (float)lossRate / (float)totalRate : 0.0f;
		jsonObject["recvJitter"]       = recvStream != nullptr ? recvStream->GetJitter() : 0;
	}

	void ShmConsumer::FillJsonScore(json& jsonObject) const
	{
		MS_TRACE();

		// NOTE: Hardcoded values
		jsonObject["score"]         = 10;
		jsonObject["producerScore"] = 10;
	}

	void ShmConsumer::HandleRequest(Channel::Request* request)
	{
		MS_TRACE();

		switch (request->methodId)
		{
			case Channel::Request::MethodId::CONSUMER_REQUEST_KEY_FRAME:
			{
				if (IsActive())
					RequestKeyFrame();

				request->Accept();

				break;
			}

			default:
			{
				// Pass it to the parent class.
				RTC::Consumer::HandleRequest(request);
			}
		}
	}

	void ShmConsumer::ProducerRtpStream(RTC::RtpStream* rtpStream, uint32_t /*mappedSsrc*/)
	{
		MS_TRACE();

		this->producerRtpStream = rtpStream;
		MS_DEBUG_TAG(rtp, "ShmConsumer's producerRtpStream is set up");
	}

	void ShmConsumer::ProducerNewRtpStream(RTC::RtpStream* rtpStream, uint32_t /*mappedSsrc*/)
	{
		MS_TRACE();

		this->producerRtpStream = rtpStream;
	}

	void ShmConsumer::ProducerRtpStreamScore(
	  RTC::RtpStream* /*rtpStream*/, uint8_t /*score*/, uint8_t /*previousScore*/)
	{
		MS_TRACE();

		// Emit the score event.
	}


	void ShmConsumer::ProducerRtcpSenderReport(RTC::RtpStream* /*rtpStream*/, bool /*first*/)
	{
		MS_TRACE();

		if (!this->producerRtpStream || !this->producerRtpStream->GetSenderReportNtpMs() || !this->producerRtpStream->GetSenderReportTs())
		{
			MS_DEBUG_2TAGS(rtcp, xcode, "Producer stream failed to read SR RTCP msg");
			return;
		}

		uint64_t lastSenderReportNtpMs = this->producerRtpStream->GetSenderReportNtpMs(); // NTP timestamp in last Sender Report (in ms)
		uint32_t lastSenderReporTs     = this->producerRtpStream->GetSenderReportTs();    // RTP timestamp in last Sender Report.

		shmCtx->WriteRtcpSenderReportTs(lastSenderReportNtpMs, lastSenderReporTs, (this->GetKind() == RTC::Media::Kind::AUDIO) ? DepLibSfuShm::Media::AUDIO : DepLibSfuShm::Media::VIDEO);
	}


	void ShmConsumer::SendRtpPacket(RTC::RtpPacket* packet)
	{
		MS_TRACE();

		if (!IsActive()) {
			return;
		}

		auto payloadType = packet->GetPayloadType();

		// NOTE: This may happen if this Consumer supports just some codecs of those
		// in the corresponding Producer.
		if (this->supportedCodecPayloadTypes.find(payloadType) == this->supportedCodecPayloadTypes.end())
		{
			MS_DEBUG_TAG(rtp, "payload type not supported [payloadType:%" PRIu8 "]", payloadType);

			return;
		}

		// If we need to sync, support key frames and this is not a key frame,
		// do not write this packet into shm
		// but use it to check video orientation and config shm ssrc
		bool ignorePkt = false;

		if (this->syncRequired && this->keyFrameSupported && !packet->IsKeyFrame())
		{
			MS_DEBUG_TAG(rtp, "need to sync but this is not keyframe, ignore packet");
			ignorePkt = true;
		}

		// Whether this is the first packet after re-sync.
		bool isSyncPacket = this->syncRequired;

		// Sync sequence number and timestamp if required.
		if (isSyncPacket)
		{
			if (packet->IsKeyFrame())
				MS_DEBUG_TAG(rtp, "sync key frame received");

			this->rtpSeqManager.Sync(packet->GetSequenceNumber() - 1);

			this->syncRequired = false;
		}

		// Update RTP seq number and timestamp.
		uint16_t seq;

		this->rtpSeqManager.Input(packet->GetSequenceNumber(), seq);

		// Save original packet fields.
		auto origSsrc = packet->GetSsrc();
		auto origSeq  = packet->GetSequenceNumber();

		// Rewrite packet.
		packet->SetSsrc(this->rtpParameters.encodings[0].ssrc);
		packet->SetSequenceNumber(seq);

		// Update received and missed packet counters
		lostPktRateCounter.Update(packet);

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

		// Check for video orientation changes
		if (VideoOrientationChanged(packet))
		{
				MS_DEBUG_2TAGS(rtp, xcode, "Video orientation changed to %d in packet[ssrc:%" PRIu32 ", seq:%" PRIu16 ", ts:%" PRIu32 "]",
				this->rotation,
				packet->GetSsrc(),
			  packet->GetSequenceNumber(),
			  packet->GetTimestamp());
			
			shmCtx->WriteVideoOrientation(this->rotation);
		}

		// Need both audio and video SSRCs to set up shm writer
		if (DepLibSfuShm::SHM_WRT_READY != this->shmCtx->Status())
		{
			shmCtx->ConfigureMediaSsrc(
				packet->GetSsrc(),
				(this->GetKind() == RTC::Media::Kind::AUDIO) ? DepLibSfuShm::Media::AUDIO : DepLibSfuShm::Media::VIDEO);
		}

		// Process the packet. In case of shm writer this is still needed for NACKs
		if (this->rtpStream->ReceivePacket(packet))
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


// Uncomment for NACK test simulation
/*		if (this->TestNACK(packet))
		{
			MS_DEBUG_TAG(rtp, "Pretend NACK for packet ssrc:%" PRIu32 ", seq:%" PRIu16 " ts: %" PRIu32 " and wait for retransmission",
			packet->GetSsrc(), packet->GetSequenceNumber(), packet->GetTimestamp());
			return;
		}*/

// End of NACK test simulation

		if (shmCtx->CanWrite() && !ignorePkt)
		{
			this->WritePacketToShm(packet);

			// Increase transmission counter.
			this->shmWriterCounter.Update(packet);
		}
		else {
			MS_DEBUG_2TAGS(rtp, xcode, "Skip pkt [ssrc:%" PRIu32 ", seq:%" PRIu16 ", ts:%" PRIu32 "] shm-writer is %s, ignorePkt is %s",
				packet->GetSsrc(),
			  packet->GetSequenceNumber(),
			  packet->GetTimestamp(),
				shmCtx->CanWrite() ? "ready" : "not ready", ignorePkt ? "true": "false");
		}

		// Restore packet fields.
		packet->SetSsrc(origSsrc);
		packet->SetSequenceNumber(origSeq);
	}


	//Uncomment for NACK test simulation
	bool ShmConsumer::TestNACK(RTC::RtpPacket* packet)
	{
		if (this->GetKind() != RTC::Media::Kind::VIDEO)
			return false; // not video

		uint64_t nowTs = DepLibUV::GetTimeMs();
		if (nowTs - this->lastNACKTestTs < 3000)
			return false; // too soon

		this->lastNACKTestTs = nowTs;

		RtpStream::Params params;
		params.ssrc      = packet->GetSsrc();
		params.clockRate = 90000;
		params.useNack   = true;

		// Create a NACK item that request for all the packets.
		RTCP::FeedbackRtpNackPacket nackPacket(0, params.ssrc);
		auto* nackItem = new RTCP::FeedbackRtpNackItem(packet->GetSequenceNumber(), 0b0000000000000000);
		nackPacket.AddItem(nackItem);

		auto it = nackPacket.Begin();
		if (it != nackPacket.End())
		{
			RTC::RTCP::FeedbackRtpNackItem* item = *it;	
			MS_DEBUG_TAG(rtp,"TestNACK, nackPacket is NOT EMPTY");
			item->Dump();		
		}
		else {
			MS_DEBUG_TAG(rtp,"TestNACK, nackPacket is EMPTY");
		}

		this->ReceiveNack(&nackPacket);

		return true;
	}
// End of NACK test simulation


	bool ShmConsumer::VideoOrientationChanged(RTC::RtpPacket* packet)
	{
		if (this->GetKind() != RTC::Media::Kind::VIDEO)
			return false;
		
		bool c;
		bool f;
		uint16_t r;

		// If it's the first read or value changed then true
		if (packet->ReadVideoOrientation(c, f, r))
		{
			if (!this->rotationDetected)
			{
				this->rotationDetected = true;
				this->rotation = r;
				return true;
			}
			
			if (r != this->rotation)
			{
				this->rotation = r;
				return true;
			}
		}
		return false;
	}


	void ShmConsumer::WritePacketToShm(RTC::RtpPacket* packet)
	{
		MS_TRACE();

		uint8_t const* pktdata = packet->GetData();
		uint8_t const* cdata   = packet->GetPayload();
		uint8_t* data          = const_cast<uint8_t*>(cdata);
		size_t len             = packet->GetPayloadLength();
		uint64_t ts            = static_cast<uint64_t>(packet->GetTimestamp());
		uint64_t seq           = static_cast<uint64_t>(packet->GetSequenceNumber());
		uint32_t ssrc          = packet->GetSsrc();
		bool isKeyFrame        = packet->IsKeyFrame();

		std::memset(&chunk, 0, sizeof(chunk));

		if (this->GetKind() == Media::Kind::VIDEO)
		{
			ts = shmCtx->AdjustVideoPktTs(ts);
			seq = shmCtx->AdjustVideoPktSeq(seq);
		}
		else // audio
		{
			ts = shmCtx->AdjustAudioPktTs(ts);
			seq = shmCtx->AdjustAudioPktSeq(seq);
		}

		switch (this->GetKind())
		{
			/* Audio NALUs are always simple */
			case RTC::Media::Kind::AUDIO:
			{
				this->chunk.data = data;
				this->chunk.len = len;
				this->chunk.rtp_time = ts;
				this->chunk.first_rtp_seq = this->chunk.last_rtp_seq = seq;
				this->chunk.ssrc = packet->GetSsrc();
				this->chunk.begin = this->chunk.end = 1;
				shmCtx->WriteRtpDataToShm(DepLibSfuShm::Media::AUDIO, &chunk);
				break;
			} // audio

			case RTC::Media::Kind::VIDEO:
			{
				uint8_t nal = *(data) & 0x1F;
				uint8_t marker = *(pktdata + 1) & 0x80; // Marker bit indicates the last or the only NALU in this packet is the end of the picture data
				bool begin_picture = (shmCtx->IsLastVideoSeqNotSet() || (ts > shmCtx->LastVideoTs())); // assume that first video pkt starts the picture frame
				// Single NAL unit packet
				if (nal >= 1 && nal <= 23)
			  {
					if (len < 1) {
						MS_WARN_TAG(xcode, "NALU data len < 1: %lu", len);
						break;
					}

					//Add Annex B 0x00000001 to the beginning of this packet: overwrite 3 or 4 bytes from the PTP pkt header
					uint16_t chunksize = len;
					if (begin_picture)
					{
						data[-1] = 0x01;
						data[-2] = 0x00;
						data[-3] = 0x00;
						data[-4] = 0x00;
						data -= 4;
						chunksize += 4;
					}
					else {
						data[-1] = 0x01;
						data[-2] = 0x00;
						data[-3] = 0x00;
						data -= 3;
						chunksize += 3;
					}
				
					this->chunk.data          = data;
					this->chunk.len           = chunksize;
					this->chunk.rtp_time      = ts;
					this->chunk.first_rtp_seq = this->chunk.last_rtp_seq = seq;
					this->chunk.ssrc          = ssrc;
					this->chunk.begin         = begin_picture;
					this->chunk.end           = (marker != 0);
					 
					if (isKeyFrame)
					{
						MS_DEBUG_TAG(xcode, "video single NALU=%d LEN=%zu ts %" PRIu64 " seq %" PRIu64 " isKeyFrame=%d begin_picture(chunk.begin)=%d marker(chunk.end)=%d lastTs=%" PRIu64,
  						nal, this->chunk.len, this->chunk.rtp_time, this->chunk.first_rtp_seq, isKeyFrame, begin_picture, marker, shmCtx->LastVideoTs());
          }

					shmCtx->WriteRtpDataToShm(DepLibSfuShm::Media::VIDEO, &chunk, !(this->chunk.begin && this->chunk.end));
				}
				else {
					switch (nal)
					{
						// Aggregation packet. Contains several NAL units in a single RTP packet
						// STAP-A.
						/*
							0                   1                   2                   3
							0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
								+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
							|                          RTP Header                           |
							+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
							|STAP-A NAL HDR |         NALU 1 Size           | NALU 1 HDR    |
							+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
							|                         NALU 1 Data                           |
							:                                                               :
							+               +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
							|               | NALU 2 Size                   | NALU 2 HDR    |
							+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
							|                         NALU 2 Data                           |
							:                                                               :
							|                               +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
							|                               :...OPTIONAL RTP padding        |
							+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
						*/
						case 24:
						{
							size_t offset{ 1 }; // Skip over STAP-A NAL header

							// Iterate NAL units.
							while (offset < len - 3) // need to be able to read 2 bytes of NALU size
							{
								uint16_t naluSize = Utils::Byte::Get2Bytes(data, offset); 
								if ( offset + naluSize > len) {
									MS_WARN_TAG(xcode, "payload left to read from STAP-A is too short: %zu > %zu", offset + naluSize, len);
									break;
								}

								offset += 2; // skip over NALU size
								uint8_t subnal = *(data + offset) & 0x1F; // actual NAL type
								
								//Add Annex B
								uint16_t chunksize = naluSize;
								this->chunk.end = (marker != 0) && (offset + chunksize >= len - 3);
								if (begin_picture) {
									this->chunk.begin = 1;
									data[offset - 1] = 0x01;
									data[offset - 2] = 0x00;
									data[offset - 3] = 0x00;
									data[offset - 4] = 0x00;
									begin_picture = 0;
									offset -= 4;
									chunksize += 4;
								}
								else {
									this->chunk.begin = 0;
									data[offset - 1] = 0x01;
									data[offset - 2] = 0x00;
									data[offset - 3] = 0x00;
									offset -= 3;
									chunksize += 3;
								}

								this->chunk.data          = data + offset;
								this->chunk.len           = chunksize;
								this->chunk.rtp_time      = ts;
								this->chunk.first_rtp_seq = this->chunk.last_rtp_seq = seq;
								this->chunk.ssrc          = ssrc;

								MS_DEBUG_TAG(xcode, "video STAP-A: NAL=%d payloadlen=%" PRIu32 " nalulen=%" PRIu16 " chunklen=%" PRIu32 " ts=%" PRIu64 " seq=%" PRIu64 " lastTs=%" PRIu64 " isKeyFrame=%d chunk.begin=%d chunk.end=%d",
									subnal, len, naluSize, this->chunk.len, this->chunk.rtp_time, this->chunk.first_rtp_seq,
									shmCtx->LastVideoTs(), isKeyFrame, this->chunk.begin, this->chunk.end);

								shmCtx->WriteRtpDataToShm(DepLibSfuShm::Media::VIDEO, &chunk, !(this->chunk.begin && this->chunk.end));
								offset += chunksize;
							}
							break;
						}

						/*
						Fragmentation unit. Single NAL unit is spreaded accross several RTP packets.
						FU-A
									0                   1                   2                   3
									0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
									+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
									| FU indicator  |   FU header   |                               |
									+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+                               |
									|                                                               |
									|                         FU payload                            |
									|                                                               |
									|                               +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
									|                               :...OPTIONAL RTP padding        |
									+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+ */
						case 28:
						{
							if (len < 3)
							{
								MS_DEBUG_TAG(xcode, "FU-A payload too short");
								break;
							}
							// Parse FU header octet
							uint8_t startBit = *(data + 1) & 0x80; // 1st bit indicates the starting fragment
							uint8_t endBit   = *(data + 1) & 0x40; // 2nd bit indicates the ending fragment
							uint8_t fuInd    = *(data) & 0xE0;     // 3 first bytes of FU indicator, use to compose NAL header for the beginning fragment 
							uint8_t subnal   = *(data + 1) & 0x1F; // Last 5 bits in FU header subtype of FU unit, use to compose NAL header for the beginning fragment, also if (videoNALU == 7 (SPS) && startBit == 128) then we have a key frame
							size_t chunksize = len - 1;            // skip over FU indicator

							if (startBit == 128)
							{
								// Write AnnexB marker and put NAL header in place of FU header
								data[1] = fuInd + subnal;
								if (begin_picture)
								{
									data[0]  = 0x01;
									data[-1] = 0x00;
									data[-2] = 0x00;
									data[-3] = 0x00;
									chunksize += 4;
									data -= 3;
									this->chunk.begin = 1;
									begin_picture = 0;
								}
								else
								{
									data[0]  = 0x01;
									data[-1] = 0x00;
									data[-2] = 0x00;
									chunksize += 3;
									data -= 2;
									this->chunk.begin  = 0;
								}
							}
							else {
								// if not the beginning fragment, discard FU indicator and FU header
								chunksize -= 1;
								data += 2;
								this->chunk.begin = 0;
							}

							this->chunk.data          = data;
							this->chunk.len           = chunksize;
							this->chunk.rtp_time      = ts;
							this->chunk.first_rtp_seq = this->chunk.last_rtp_seq = seq;
							this->chunk.ssrc          = ssrc;
							this->chunk.end           = (endBit && marker) ? 1 : 0;
							if (isKeyFrame) // remove if not afraid of too much output
							{
								MS_DEBUG_TAG(xcode, "video FU-A NAL=%" PRIu8 " len=%" PRIu32 " ts=%" PRIu64 " prev_ts=%" PRIu64 " seq=%" PRIu64 " isKeyFrame=%d startBit=%" PRIu8 " endBit=%" PRIu8 " marker=%" PRIu8 " chunk.begin=%d chunk.end=%d",
									subnal, this->chunk.len, this->chunk.rtp_time, shmCtx->LastVideoTs(), this->chunk.first_rtp_seq,
									isKeyFrame, startBit, endBit, marker, this->chunk.begin, this->chunk.end);
							}

							shmCtx->WriteRtpDataToShm(DepLibSfuShm::Media::VIDEO, &(this->chunk), true);
							break;
						}
						case 25: // STAB-B
						case 26: // MTAP-16
						case 27: // MTAP-24
						case 29: // FU-B
						{
							MS_WARN_TAG(xcode, "Unsupported NAL unit type %u in video packet", nal);
							break;
						}
						default: // ignore the rest
						{
							MS_DEBUG_TAG(xcode, "Unknown NAL unit type %u in video packet", nal);
							break;
						}
					}
				} // Aggregate or fragmented NALUs
				break;
			} // case video

			default:
				break;
		} // kind

		// Update last timestamp and seqId, even if we failed to write pkt
		switch (this->GetKind())
		{
			case RTC::Media::Kind::AUDIO:
				shmCtx->UpdatePktStat(seq, ts, DepLibSfuShm::Media::AUDIO);
				break;
			case RTC::Media::Kind::VIDEO:
				shmCtx->UpdatePktStat(seq, ts, DepLibSfuShm::Media::VIDEO);
				break;
			default:
				break;
		}
	}


	void ShmConsumer::GetRtcp(
	  RTC::RTCP::CompoundPacket* packet, RTC::RtpStreamSend* rtpStream, uint64_t nowMs)
	{
		MS_TRACE();

		MS_ASSERT(rtpStream == this->rtpStream, "RTP stream does not match");

		if (static_cast<float>((nowMs - this->lastRtcpSentTime) * 1.15) < this->maxRtcpInterval)
			return;

		auto* report = this->rtpStream->GetRtcpSenderReport(nowMs);

		if (!report)
			return;

		packet->AddSenderReport(report);

		// Build SDES chunk for this sender.
		auto* sdesChunk = this->rtpStream->GetRtcpSdesChunk();

		packet->AddSdesChunk(sdesChunk);

		this->lastRtcpSentTime = nowMs;
	}

	void ShmConsumer::NeedWorstRemoteFractionLost(
	  uint32_t /*mappedSsrc*/, uint8_t& worstRemoteFractionLost)
	{
		MS_TRACE();

		if (!IsActive())
			return;

		auto fractionLost = this->rtpStream->GetFractionLost();

		// If our fraction lost is worse than the given one, update it.
		if (fractionLost > worstRemoteFractionLost)
			worstRemoteFractionLost = fractionLost;
	}


	void ShmConsumer::ReceiveNack(RTC::RTCP::FeedbackRtpNackPacket* nackPacket)
	{
		MS_TRACE();

		if (!IsActive())
			return;

		// May emit 'trace' event.
		EmitTraceEventNackType();

		this->rtpStream->ReceiveNack(nackPacket);
	}


	void ShmConsumer::ReceiveKeyFrameRequest(
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
			RequestKeyFrame();
	}


	void ShmConsumer::ReceiveRtcpReceiverReport(RTC::RTCP::ReceiverReport* report)
	{
		MS_TRACE();

		this->rtpStream->ReceiveRtcpReceiverReport(report);
	}


	uint32_t ShmConsumer::GetTransmissionRate(uint64_t nowMs)
	{
		MS_TRACE();

		//return 0u;
		if (!IsActive())
			return 0u;

		return this->rtpStream->GetBitrate(nowMs);
	}


	void ShmConsumer::UserOnTransportConnected()
	{
		MS_TRACE();

		this->syncRequired = true;

		if (IsActive())
			RequestKeyFrame();
	}


	void ShmConsumer::UserOnTransportDisconnected()
	{
		MS_TRACE();

		this->rtpStream->Pause();
	}

	void ShmConsumer::UserOnPaused()
	{
		MS_TRACE();

		this->rtpStream->Pause();
	}

	void ShmConsumer::UserOnResumed()
	{
		MS_TRACE();

		this->syncRequired = true;

		if (IsActive())
			RequestKeyFrame();
	}

	void ShmConsumer::CreateRtpStream()
	{
		MS_TRACE();

		auto& encoding   = this->rtpParameters.encodings[0];
		auto* mediaCodec = this->rtpParameters.GetCodecForEncoding(encoding);

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

		// Always enable these for ShmConsumer
		params.useNack = true;
		params.usePli = true;
		params.useFir = true;

		// Create a RtpStreamSend for sending a single media stream.
		size_t bufferSize = params.useNack ? 600u : 0u;

		this->rtpStream = new RTC::RtpStreamSend(this, params, bufferSize);
		this->rtpStreams.push_back(this->rtpStream);

		// If the Consumer is paused, tell the RtpStreamSend.
		if (IsPaused() || IsProducerPaused())
			rtpStream->Pause();

		auto* rtxCodec = this->rtpParameters.GetRtxCodecForEncoding(encoding);

		if (rtxCodec && encoding.hasRtx)
			this->rtpStream->SetRtx(rtxCodec->payloadType, encoding.rtx.ssrc);
	}


	void ShmConsumer::RequestKeyFrame()
	{
		MS_TRACE();

		if (this->kind != RTC::Media::Kind::VIDEO)
			return;

		auto mappedSsrc = this->consumableRtpEncodings[0].ssrc;

		this->listener->OnConsumerKeyFrameRequested(this, mappedSsrc);
	}

	void ShmConsumer::OnShmWriterReady()
	{
		MS_TRACE();

		MS_DEBUG_2TAGS(rtp, xcode, "shm-writer ready, consumer %s, get key frame", IsActive() ? "ready" : "not ready");

		this->syncRequired = true;

		if (IsActive())
			RequestKeyFrame();
	}

	void ShmConsumer::OnRtpStreamRetransmitRtpPacket(RTC::RtpStreamSend* rtpStream, RTC::RtpPacket* packet)
	{
		MS_TRACE();

		this->listener->OnConsumerRetransmitRtpPacket(this, packet);

		// May emit 'trace' event.
		EmitTraceEventRtpAndKeyFrameTypes(packet, this->rtpStream->HasRtx());
	}

	inline void ShmConsumer::OnRtpStreamScore(
	  RTC::RtpStream* /*rtpStream*/, uint8_t /*score*/, uint8_t /*previousScore*/)
	{
		MS_TRACE();
	}

	uint8_t ShmConsumer::GetBitratePriority() const
	{
		MS_TRACE();

		return 0u;
	}

	uint32_t ShmConsumer::IncreaseLayer(uint32_t /*bitrate*/, bool /*considerLoss*/)
	{
		MS_TRACE();

		return 0u;
	}

	void ShmConsumer::ApplyLayers()
	{
		MS_TRACE();
	}

	uint32_t ShmConsumer::GetDesiredBitrate() const
	{
		MS_TRACE();

		return 0u;
	}


	float ShmConsumer::GetRtt() const
	{
		MS_TRACE();

		return this->rtpStream->GetRtt();
	}
} // namespace RTC
