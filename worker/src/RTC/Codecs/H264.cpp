#define MS_CLASS "RTC::Codecs::H264"
// #define MS_LOG_DEV_LEVEL 3

#include "RTC/Codecs/H264.hpp"
#include "Logger.hpp"
#include "Utils.hpp"

namespace RTC
{
	namespace Codecs
	{
		/* Class methods. */

		H264::PayloadDescriptor* H264::Parse(
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
				payloadDescriptor->s   = frameMarking->start;
				payloadDescriptor->e   = frameMarking->end;
				payloadDescriptor->i   = frameMarking->independent;
				payloadDescriptor->d   = frameMarking->discardable;
				payloadDescriptor->b   = frameMarking->base;
				payloadDescriptor->tid = frameMarking->tid;

				payloadDescriptor->hasTid = true;

				if (frameMarkingLen >= 2)
				{
					payloadDescriptor->hasLid = true;
					payloadDescriptor->lid    = frameMarking->lid;
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

			// NOTE: Unfortunately libwebrtc produces wrong Frame-Marking (without i=1 in
			// keyframes) when it uses H264 hardware encoder (at least in Mac):
			//   https://bugs.chromium.org/p/webrtc/issues/detail?id=10746
			//
			// As a temporal workaround, always do payload parsing to detect keyframes if
			// there is no frame-marking or if there is but keyframe was not detected above.
			if (!frameMarking || !payloadDescriptor->isKeyFrame)
			{
				const uint8_t nal = *data & 0x1F;

				switch (nal)
				{
					// Single NAL unit packet.
					// IDR (instantaneous decoding picture).
					case 7:
					{
						payloadDescriptor->isKeyFrame = true;

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
							auto naluSize        = Utils::Byte::Get2Bytes(data, offset);
							const uint8_t subnal = *(data + offset + sizeof(naluSize)) & 0x1F;

							if (subnal == 7)
							{
								payloadDescriptor->isKeyFrame = true;

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
						const uint8_t subnal   = *(data + 1) & 0x1F;
						const uint8_t startBit = *(data + 1) & 0x80;

						if (subnal == 7 && startBit == 128)
						{
							payloadDescriptor->isKeyFrame = true;
						}

						break;
					}
				}
			}

			return payloadDescriptor.release();
		}

		void H264::ProcessRtpPacket(RTC::RtpPacket* packet)
		{
			MS_TRACE();

			auto* data = packet->GetPayload();
			auto len   = packet->GetPayloadLength();
			RtpPacket::FrameMarking* frameMarking{ nullptr };
			uint8_t frameMarkingLen{ 0 };

			// Read frame-marking.
			packet->ReadFrameMarking(&frameMarking, frameMarkingLen);

			PayloadDescriptor* payloadDescriptor = H264::Parse(data, len, frameMarking, frameMarkingLen);

			if (!payloadDescriptor)
			{
				return;
			}

			auto* payloadDescriptorHandler = new PayloadDescriptorHandler(payloadDescriptor);

			packet->SetPayloadDescriptorHandler(payloadDescriptorHandler);
		}

		/* Instance methods. */

		void H264::PayloadDescriptor::Dump() const
		{
			MS_TRACE();

			MS_DUMP("<H264::PayloadDescriptor>");
			MS_DUMP(
			  "  s:%" PRIu8 "|e:%" PRIu8 "|i:%" PRIu8 "|d:%" PRIu8 "|b:%" PRIu8,
			  this->s,
			  this->e,
			  this->i,
			  this->d,
			  this->b);
			if (this->hasTid)
			{
				MS_DUMP("  tid: %" PRIu8, this->tid);
			}
			if (this->hasLid)
			{
				MS_DUMP("  lid: %" PRIu8, this->lid);
			}
			if (this->hasTl0picidx)
			{
				MS_DUMP("  tl0picidx: %" PRIu8, this->tl0picidx);
			}
			MS_DUMP("  isKeyFrame: %s", this->isKeyFrame ? "true" : "false");
			MS_DUMP("</H264::PayloadDescriptor>");
		}

		H264::PayloadDescriptorHandler::PayloadDescriptorHandler(H264::PayloadDescriptor* payloadDescriptor)
		{
			MS_TRACE();

			this->payloadDescriptor.reset(payloadDescriptor);
		}

		bool H264::PayloadDescriptorHandler::Process(
		  RTC::Codecs::EncodingContext* encodingContext, uint8_t* /*data*/, bool& /*marker*/)
		{
			MS_TRACE();

			auto* context = static_cast<RTC::Codecs::H264::EncodingContext*>(encodingContext);

			MS_ASSERT(context->GetTargetTemporalLayer() >= 0, "target temporal layer cannot be -1");

			// Check if the payload should contain temporal layer info.
			if (context->GetTemporalLayers() > 1 && !this->payloadDescriptor->hasTid)
			{
				MS_WARN_DEV("stream is supposed to have >1 temporal layers but does not have tid field");
			}

			// clang-format off
			if (
				this->payloadDescriptor->hasTid &&
				this->payloadDescriptor->tid > context->GetTargetTemporalLayer()
			)
			// clang-format on
			{
				return false;
			}
			// Upgrade required. Drop current packet if base flag is not set.
			// NOTE: This is possible once this bug in libwebrtc has been fixed:
			//   https://github.com/versatica/mediasoup/issues/306
			//
			// clang-format off
			else if (
				this->payloadDescriptor->hasTid &&
				this->payloadDescriptor->tid > context->GetCurrentTemporalLayer() &&
				!this->payloadDescriptor->b
			)
			// clang-format on
			{
				return false;
			}

			// Update/fix current temporal layer.
			// clang-format off
			if (
				this->payloadDescriptor->hasTid &&
				this->payloadDescriptor->tid > context->GetCurrentTemporalLayer()
			)
			// clang-format on
			{
				context->SetCurrentTemporalLayer(this->payloadDescriptor->tid);
			}
			else if (!this->payloadDescriptor->hasTid)
			{
				context->SetCurrentTemporalLayer(0);
			}

			if (context->GetCurrentTemporalLayer() > context->GetTargetTemporalLayer())
			{
				context->SetCurrentTemporalLayer(context->GetTargetTemporalLayer());
			}

			return true;
		}

		void H264::PayloadDescriptorHandler::Restore(uint8_t* /*data*/)
		{
			MS_TRACE();
		}
	} // namespace Codecs
} // namespace RTC
