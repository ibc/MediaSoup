#define MS_CLASS "RTC::Codecs::VP9"
// #define MS_LOG_DEV_LEVEL 3

#include "RTC/Codecs/VP9.hpp"
#include "Logger.hpp"

namespace RTC
{
	namespace Codecs
	{
		/* Class methods. */

		VP9::PayloadDescriptor* VP9::Parse(
		  const uint8_t* data,
		  size_t len,
		  RTC::RtpPacket::FrameMarking* /*frameMarking*/,
		  uint8_t /*frameMarkingLen*/)
		{
			MS_TRACE();

			if (len < 1)
			{
				MS_WARN_DEV("ignoring empty payload");

				return nullptr;
			}

			std::unique_ptr<PayloadDescriptor> payloadDescriptor(new PayloadDescriptor());

			size_t offset{ 0 };
			uint8_t byte = data[offset];

			payloadDescriptor->i = (byte >> 7) & 0x01;
			payloadDescriptor->p = (byte >> 6) & 0x01;
			payloadDescriptor->l = (byte >> 5) & 0x01;
			payloadDescriptor->f = (byte >> 4) & 0x01;
			payloadDescriptor->b = (byte >> 3) & 0x01;
			payloadDescriptor->e = (byte >> 2) & 0x01;
			payloadDescriptor->v = (byte >> 1) & 0x01;

			if (payloadDescriptor->i)
			{
				if (len < ++offset + 1)
				{
					MS_WARN_DEV("ignoring invalid payload (1)");

					return nullptr;
				}

				byte = data[offset];

				if (byte >> 7 & 0x01)
				{
					if (len < ++offset + 1)
					{
						MS_WARN_DEV("ignoring invalid payload (2)");

						return nullptr;
					}

					payloadDescriptor->pictureId = (byte & 0x7F) << 8;
					payloadDescriptor->pictureId += data[offset];
					payloadDescriptor->hasTwoBytesPictureId = true;
				}
				else
				{
					payloadDescriptor->pictureId           = byte & 0x7F;
					payloadDescriptor->hasOneBytePictureId = true;
				}

				payloadDescriptor->hasPictureId = true;
			}

			if (payloadDescriptor->l)
			{
				if (len < ++offset + 1)
				{
					MS_WARN_DEV("ignoring invalid payload (3)");

					return nullptr;
				}

				byte = data[offset];

				payloadDescriptor->interLayerDependency = byte & 0x01;
				payloadDescriptor->switchingUpPoint     = byte >> 4 & 0x01;
				payloadDescriptor->slIndex              = byte >> 1 & 0x07;
				payloadDescriptor->tlIndex              = byte >> 5 & 0x07;
				payloadDescriptor->hasSlIndex           = true;
				payloadDescriptor->hasTlIndex           = true;

				if (len < ++offset + 1)
				{
					MS_WARN_DEV("ignoring invalid payload (4)");

					return nullptr;
				}

				// Read TL0PICIDX if flexible mode is unset.
				if (!payloadDescriptor->f)
				{
					payloadDescriptor->tl0PictureIndex    = data[offset];
					payloadDescriptor->hasTl0PictureIndex = true;
				}
			}

			// clang-format off
			if (
				!payloadDescriptor->p &&
				payloadDescriptor->b &&
				payloadDescriptor->slIndex == 0
			)
			// clang-format on
			{
				payloadDescriptor->isKeyFrame = true;
			}

			return payloadDescriptor.release();
		}

		void VP9::ProcessRtpPacket(RTC::RtpPacket* packet)
		{
			MS_TRACE();

			auto* data = packet->GetPayload();
			auto len   = packet->GetPayloadLength();
			RtpPacket::FrameMarking* frameMarking{ nullptr };
			uint8_t frameMarkingLen{ 0 };

			// Read frame-marking.
			packet->ReadFrameMarking(&frameMarking, frameMarkingLen);

			PayloadDescriptor* payloadDescriptor = VP9::Parse(data, len, frameMarking, frameMarkingLen);

			if (!payloadDescriptor)
			{
				return;
			}

			if (payloadDescriptor->isKeyFrame)
			{
				MS_DEBUG_DEV(
				  "key frame [spatialLayer:%" PRIu8 ", temporalLayer:%" PRIu8 "]",
				  packet->GetSpatialLayer(),
				  packet->GetTemporalLayer());
			}

			auto* payloadDescriptorHandler = new PayloadDescriptorHandler(payloadDescriptor);

			packet->SetPayloadDescriptorHandler(payloadDescriptorHandler);
		}

		/* Instance methods. */

		void VP9::PayloadDescriptor::Dump() const
		{
			MS_TRACE();

			MS_DUMP("<VP9::PayloadDescriptor>");
			MS_DUMP(
			  "  i:%" PRIu8 "|p:%" PRIu8 "|l:%" PRIu8 "|f:%" PRIu8 "|b:%" PRIu8 "|e:%" PRIu8 "|v:%" PRIu8,
			  this->i,
			  this->p,
			  this->l,
			  this->f,
			  this->b,
			  this->e,
			  this->v);
			MS_DUMP("  pictureId: %" PRIu16, this->pictureId);
			MS_DUMP("  slIndex: %" PRIu8, this->slIndex);
			MS_DUMP("  tlIndex: %" PRIu8, this->tlIndex);
			MS_DUMP("  tl0PictureIndex: %" PRIu8, this->tl0PictureIndex);
			MS_DUMP("  interLayerDependency: %" PRIu8, this->interLayerDependency);
			MS_DUMP("  switchingUpPoint: %" PRIu8, this->switchingUpPoint);
			MS_DUMP("  isKeyFrame: %s", this->isKeyFrame ? "true" : "false");
			MS_DUMP("  hasPictureId: %s", this->hasPictureId ? "true" : "false");
			MS_DUMP("  hasOneBytePictureId: %s", this->hasOneBytePictureId ? "true" : "false");
			MS_DUMP("  hasTwoBytesPictureId: %s", this->hasTwoBytesPictureId ? "true" : "false");
			MS_DUMP("  hasTl0PictureIndex: %s", this->hasTl0PictureIndex ? "true" : "false");
			MS_DUMP("  hasSlIndex: %s", this->hasSlIndex ? "true" : "false");
			MS_DUMP("  hasTlIndex: %s", this->hasTlIndex ? "true" : "false");
			MS_DUMP("</VP9::PayloadDescriptor>");
		}

		VP9::PayloadDescriptorHandler::PayloadDescriptorHandler(VP9::PayloadDescriptor* payloadDescriptor)
		{
			MS_TRACE();

			this->payloadDescriptor.reset(payloadDescriptor);
		}

		bool VP9::PayloadDescriptorHandler::Process(
		  RTC::Codecs::EncodingContext* encodingContext, uint8_t* /*data*/, bool& marker)
		{
			MS_TRACE();

			auto* context = static_cast<RTC::Codecs::VP9::EncodingContext*>(encodingContext);

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

			// Check whether pictureId sync is required.
			// clang-format off
			if (
				context->syncRequired &&
				this->payloadDescriptor->hasPictureId
			)
			// clang-format on
			{
				context->pictureIdManager.Sync(this->payloadDescriptor->pictureId - 1);

				context->syncRequired = false;
			}

			// clang-format off
			const bool isOldPacket = (
				this->payloadDescriptor->hasPictureId &&
				RTC::SeqManager<uint16_t, 15>::IsSeqLowerThan(
					this->payloadDescriptor->pictureId,
					context->pictureIdManager.GetMaxInput())
			);
			// clang-format on

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

			// Unless old packet filter spatial layers that are either
			// * higher than current one
			// * different than the current one when KSVC is enabled and this is not a keyframe
			// (interframe p bit = 1)
			// clang-format off
			if (
			  !isOldPacket &&
			  (
			  	packetSpatialLayer > tmpSpatialLayer ||
			  	(context->IsKSvc() && this->payloadDescriptor->p && packetSpatialLayer != tmpSpatialLayer)
			  )
			)
			// clang-format on
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
						(
							context->GetCurrentTemporalLayer() == -1 ||
							this->payloadDescriptor->switchingUpPoint
						) &&
						this->payloadDescriptor->b
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

			// Update the pictureId manager.
			if (this->payloadDescriptor->hasPictureId)
			{
				uint16_t pictureId;

				context->pictureIdManager.Input(this->payloadDescriptor->pictureId, pictureId);
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

		void VP9::PayloadDescriptorHandler::Restore(uint8_t* /*data*/)
		{
			MS_TRACE();
		}
	} // namespace Codecs
} // namespace RTC
