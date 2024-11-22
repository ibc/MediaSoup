#define MS_CLASS "RTC::Codecs::H264_SVC"

#include "RTC/Codecs/H264_SVC.hpp"
#include "Logger.hpp"
#include "Utils.hpp"

namespace RTC
{
	namespace Codecs
	{
		/* Class methods. */

		H264_SVC::PayloadDescriptor* H264_SVC::Parse(
		  const uint8_t* data, size_t len, RTC::RtpPacket::FrameMarking* frameMarking, uint8_t frameMarkingLen)
		{
			MS_TRACE();

			if (len < 2)
			{
				MS_WARN_DEV("ignoring payload with length < 2");

				return nullptr;
			}

			std::unique_ptr<PayloadDescriptor> payloadDescriptor(new PayloadDescriptor());

			// Use frame-marking.
			if (frameMarking)
			{
				// Read fields.
				payloadDescriptor->s       = frameMarking->start;
				payloadDescriptor->e       = frameMarking->end;
				payloadDescriptor->i       = frameMarking->independent;
				payloadDescriptor->d       = frameMarking->discardable;
				payloadDescriptor->b       = frameMarking->base;
				payloadDescriptor->tlIndex = frameMarking->tid;

				payloadDescriptor->hasTlIndex = true;

				if (frameMarkingLen >= 2)
				{
					payloadDescriptor->hasSlIndex = true;
					payloadDescriptor->slIndex    = frameMarking->lid >> 4 & 0x07;
				}

				if (frameMarkingLen == 3)
				{
					payloadDescriptor->hasTl0picidx = true;
					payloadDescriptor->tl0picidx    = frameMarking->tl0picidx;
				}

				// Detect key frame.
				if (frameMarking->start && frameMarking->independent)
				{
					payloadDescriptor->isKeyFrame = true;
				}
			}

			// NOTE: Unfortunately libwebrtc produces wrong Frame-Marking (without i=1
			// in keyframes) when it uses H264 hardware encoder (at least in Mac):
			//   https://bugs.chromium.org/p/webrtc/issues/detail?id=10746
			//
			// As a temporal workaround, always do payload parsing to detect keyframes
			// if there is no frame-marking or if there is but keyframe was not
			// detected above.
			if (!frameMarking || !payloadDescriptor->isKeyFrame)
			{
				const uint8_t nal = *data & 0x1F;

				switch (nal)
				{
					// Single NAL unit packet.
					// IDR (instantaneous decoding picture).
					case 1:
					case 5:
					case 7:
					case 14:
					case 20:
					{
						payloadDescriptor =
						  H264_SVC::ParseSingleNalu(data, len, std::move(payloadDescriptor), true);

						if (payloadDescriptor == nullptr)
						{
							MS_WARN_DEV("ignoring invalid payload (1)");

							return nullptr;
						}

						break;
					}

					// Aggreation packet.
					// STAP-A.
					case 24:
					{
						size_t offset{ 1 };

						len -= 1;

						// Iterate NAL units.
						while (len >= 3)
						{
							auto naluSize = Utils::Byte::Get2Bytes(data, offset);

							payloadDescriptor = H264_SVC::ParseSingleNalu(
							  (data + offset + sizeof(naluSize)),
							  (len - sizeof(naluSize)),
							  std::move(payloadDescriptor),
							  true);

							if (payloadDescriptor == nullptr)
							{
								MS_WARN_DEV("ignoring invalid payload (2)");

								return nullptr;
							}

							if (payloadDescriptor->isKeyFrame)
							{
								break;
							}

							// Check if there is room for the indicated NAL unit size.
							if (len < (naluSize + sizeof(naluSize)))
							{
								break;
							}

							offset += naluSize + sizeof(naluSize);
							len -= naluSize + sizeof(naluSize);
						}

						break;
					}

					// Aggreation packet.
					// FU-A, FU-B.
					case 28:
					case 29:
					{
						const uint8_t startBit = *(data + 1) & 0x80;

						if (startBit == 128)
						{
							payloadDescriptor = H264_SVC::ParseSingleNalu(
							  (data + 1), (len - 1), std::move(payloadDescriptor), (startBit == 128 ? true : false));
						}

						if (payloadDescriptor == nullptr)
						{
							MS_WARN_DEV("ignoring invalid payload (3)");

							return nullptr;
						}

						break;
					}
				}
			}

			return payloadDescriptor.release();
		}

		std::unique_ptr<H264_SVC::PayloadDescriptor> H264_SVC::ParseSingleNalu(
		  const uint8_t* data,
		  size_t len,
		  std::unique_ptr<H264_SVC::PayloadDescriptor> payloadDescriptor,
		  bool isStartBit)
		{
			const uint8_t nal = *data & 0x1F;

			switch (nal)
			{
				// Single NAL unit packet.
				// IDR (instantaneous decoding picture).
				case 5:
				{
					payloadDescriptor->isKeyFrame = true;
				}

				case 1:
				{
					payloadDescriptor->slIndex = 0;
					payloadDescriptor->tlIndex = 0;

					payloadDescriptor->hasSlIndex = false;
					payloadDescriptor->hasTlIndex = false;

					break;
				}

				case 14:
				case 20:
				{
					if (len <= 1)
					{
						return nullptr;
					}

					size_t offset{ 1 };
					uint8_t byte = data[offset];

					payloadDescriptor->idr        = byte >> 6 & 0x01;
					payloadDescriptor->priorityId = byte & 0x06;
					payloadDescriptor->isKeyFrame = (isStartBit && payloadDescriptor->idr) ? true : false;

					if (len < ++offset + 1)
					{
						return nullptr;
					}

					byte                                  = data[offset];
					payloadDescriptor->noIntLayerPredFlag = byte >> 7 & 0x01;
					payloadDescriptor->slIndex            = byte >> 4 & 0x03;

					if (len < ++offset + 1)
					{
						return nullptr;
					}

					byte = data[offset];

					payloadDescriptor->tlIndex = byte >> 5 & 0x03;

					payloadDescriptor->hasSlIndex = payloadDescriptor->slIndex ? true : false;
					payloadDescriptor->hasTlIndex = payloadDescriptor->tlIndex ? true : false;

					break;
				}

				case 7:
				{
					payloadDescriptor->isKeyFrame = isStartBit ? true : false;

					break;
				}
			}

			return payloadDescriptor;
		}

		void H264_SVC::ProcessRtpPacket(RTC::RtpPacket* packet)
		{
			MS_TRACE();

			auto* data = packet->GetPayload();
			auto len   = packet->GetPayloadLength();
			RtpPacket::FrameMarking* frameMarking{ nullptr };
			uint8_t frameMarkingLen{ 0 };

			// Read frame-marking.
			packet->ReadFrameMarking(&frameMarking, frameMarkingLen);

			PayloadDescriptor* payloadDescriptor =
			  H264_SVC::Parse(data, len, frameMarking, frameMarkingLen);

			if (!payloadDescriptor)
			{
				return;
			}

			auto* payloadDescriptorHandler = new PayloadDescriptorHandler(payloadDescriptor);

			packet->SetPayloadDescriptorHandler(payloadDescriptorHandler);
		}

		/* Instance methods. */

		void H264_SVC::PayloadDescriptor::Dump() const
		{
			MS_TRACE();

			MS_DUMP("<H264_SVC::PayloadDescriptor>");
			MS_DUMP(
			  "  s:%" PRIu8 "|e:%" PRIu8 "|i:%" PRIu8 "|d:%" PRIu8 "|b:%" PRIu8,
			  this->s,
			  this->e,
			  this->i,
			  this->d,
			  this->b);
			MS_DUMP("  hasSlIndex: %s", this->hasSlIndex ? "true" : "false");
			MS_DUMP("  hasTlIndex: %s", this->hasTlIndex ? "true" : "false");
			MS_DUMP("  tl0picidx: %" PRIu8, this->tl0picidx);
			MS_DUMP("  noIntLayerPredFlag: %" PRIu8, this->noIntLayerPredFlag);
			MS_DUMP("  isKeyFrame: %s", this->isKeyFrame ? "true" : "false");
			MS_DUMP("</H264_SVC::PayloadDescriptor>");
		}

		H264_SVC::PayloadDescriptorHandler::PayloadDescriptorHandler(
		  H264_SVC::PayloadDescriptor* payloadDescriptor)
		{
			MS_TRACE();

			this->payloadDescriptor.reset(payloadDescriptor);
		}

		bool H264_SVC::PayloadDescriptorHandler::Process(
		  RTC::Codecs::EncodingContext* encodingContext, uint8_t* /*data*/, bool& marker)
		{
			MS_TRACE();

			auto* context = static_cast<RTC::Codecs::H264_SVC::EncodingContext*>(encodingContext);

			MS_ASSERT(context->GetTargetSpatialLayer() >= 0, "target spatial layer cannot be -1");
			MS_ASSERT(context->GetTargetTemporalLayer() >= 0, "target temporal layer cannot be -1");

			auto packetSpatialLayer  = GetSpatialLayer();
			auto packetTemporalLayer = GetTemporalLayer();
			auto tmpSpatialLayer     = context->GetCurrentSpatialLayer();
			auto tmpTemporalLayer    = context->GetCurrentTemporalLayer();

			// If packet spatial or temporal layer is higher than maximum announced
			// one, drop the packet.
			// clang-format off
			if (
				packetSpatialLayer >= context->GetSpatialLayers() ||
				packetTemporalLayer >= context->GetTemporalLayers()
			)
			// clang-format on
			{
				MS_WARN_TAG(
				  rtp, "too high packet layers %" PRIu8 ":%" PRIu8, packetSpatialLayer, packetTemporalLayer);

				return false;
			}

			// clang-format off
			const bool isOldPacket = false;

			// Upgrade current spatial layer if needed.
			if (context->GetTargetSpatialLayer() > context->GetCurrentSpatialLayer())
			{
				if (this->payloadDescriptor->isKeyFrame)
				{
					MS_DEBUG_DEV(
					  "upgrading tmpSpatialLayer from %" PRIu16 " to %" PRIu16 " (packet:%" PRIu8 ":%" PRIu8
					  ")",
					  context->GetCurrentSpatialLayer(),
					  context->GetTargetSpatialLayer(),
					  packetSpatialLayer,
					  packetTemporalLayer);

					tmpSpatialLayer  = context->GetTargetSpatialLayer();
					tmpTemporalLayer = 0; // Just in case.
				}
			}
			// Downgrade current spatial layer if needed.
			else if (context->GetTargetSpatialLayer() < context->GetCurrentSpatialLayer())
			{
				// In K-SVC we must wait for a keyframe.
				if (context->IsKSvc())
				{
					if (this->payloadDescriptor->isKeyFrame)
					// clang-format on
					{
						MS_DEBUG_DEV(
						  "downgrading tmpSpatialLayer from %" PRIu16 " to %" PRIu16 " (packet:%" PRIu8
						  ":%" PRIu8 ") after keyframe (K-SVC)",
						  context->GetCurrentSpatialLayer(),
						  context->GetTargetSpatialLayer(),
						  packetSpatialLayer,
						  packetTemporalLayer);

						tmpSpatialLayer  = context->GetTargetSpatialLayer();
						tmpTemporalLayer = 0; // Just in case.
					}
				}
				// In full SVC we do not need a keyframe.
				else
				{
					// clang-format off
					if (
						packetSpatialLayer == context->GetTargetSpatialLayer() &&
						this->payloadDescriptor->e
					)
					// clang-format on
					{
						MS_DEBUG_DEV(
						  "downgrading tmpSpatialLayer from %" PRIu16 " to %" PRIu16 " (packet:%" PRIu8
						  ":%" PRIu8 ") without keyframe (full SVC)",
						  context->GetCurrentSpatialLayer(),
						  context->GetTargetSpatialLayer(),
						  packetSpatialLayer,
						  packetTemporalLayer);

						tmpSpatialLayer  = context->GetTargetSpatialLayer();
						tmpTemporalLayer = 0; // Just in case.
					}
				}
			}

			// Filter spatial layers higher than current one (unless old packet).
			if (packetSpatialLayer > tmpSpatialLayer && !isOldPacket)
			{
				return false;
			}

			// Check and handle temporal layer (unless old packet).
			if (!isOldPacket)
			{
				// Upgrade current temporal layer if needed.
				if (context->GetTargetTemporalLayer() > context->GetCurrentTemporalLayer())
				{
					// clang-format off
					if (
						packetTemporalLayer >= context->GetCurrentTemporalLayer() + 1 &&
						this->payloadDescriptor->s
					)
					// clang-format on
					{
						MS_DEBUG_DEV(
						  "upgrading tmpTemporalLayer from %" PRIu16 " to %" PRIu8 " (packet:%" PRIu8 ":%" PRIu8
						  ")",
						  context->GetCurrentTemporalLayer(),
						  packetTemporalLayer,
						  packetSpatialLayer,
						  packetTemporalLayer);

						tmpTemporalLayer = packetTemporalLayer;
					}
				}
				// Downgrade current temporal layer if needed.
				else if (context->GetTargetTemporalLayer() < context->GetCurrentTemporalLayer())
				{
					// clang-format off
					if (
						packetTemporalLayer == context->GetTargetTemporalLayer() &&
						this->payloadDescriptor->e
					)
					// clang-format on
					{
						MS_DEBUG_DEV(
						  "downgrading tmpTemporalLayer from %" PRIu16 " to %" PRIu16 " (packet:%" PRIu8
						  ":%" PRIu8 ")",
						  context->GetCurrentTemporalLayer(),
						  context->GetTargetTemporalLayer(),
						  packetSpatialLayer,
						  packetTemporalLayer);

						tmpTemporalLayer = context->GetTargetTemporalLayer();
					}
				}

				// Filter temporal layers higher than current one.
				if (packetTemporalLayer > tmpTemporalLayer)
				{
					return false;
				}
			}

			// Set marker bit if needed.
			if (packetSpatialLayer == tmpSpatialLayer && this->payloadDescriptor->e)
			{
				marker = true;
			}

			// Update current spatial layer if needed.
			if (tmpSpatialLayer != context->GetCurrentSpatialLayer())
			{
				context->SetCurrentSpatialLayer(tmpSpatialLayer);
			}

			// Update current temporal layer if needed.
			if (tmpTemporalLayer != context->GetCurrentTemporalLayer())
			{
				context->SetCurrentTemporalLayer(tmpTemporalLayer);
			}

			return true;
		}

		void H264_SVC::PayloadDescriptorHandler::Restore(uint8_t* /*data*/)
		{
			MS_TRACE();
		}
	} // namespace Codecs
} // namespace RTC
