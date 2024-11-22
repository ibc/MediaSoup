#define MS_CLASS "RTC::Codecs::Opus"
// #define MS_LOG_DEV_LEVEL 3

#include "RTC/Codecs/Opus.hpp"
#include "Logger.hpp"

namespace RTC
{
	namespace Codecs
	{
		/* Class methods. */

		Opus::PayloadDescriptor* Opus::Parse(const uint8_t* data, size_t len)
		{
			MS_TRACE();

			if (len < 1)
			{
				MS_WARN_DEV("ignoring empty payload");

				return nullptr;
			}

			std::unique_ptr<PayloadDescriptor> payloadDescriptor(new PayloadDescriptor());

			uint8_t byte = data[0];

			payloadDescriptor->stereo = (byte >> 2) & 0x01;
			payloadDescriptor->code   = byte & 0x03;

			switch (payloadDescriptor->code)
			{
				case 0:
				case 1:
				{
					// In code 0 and 1 packets, DTX is determined by total length = 1 (TOC
					// byte only).
					if (len == 1)
					{
						payloadDescriptor->isDtx = true;
					}

					break;
				}

				case 2:
				{
					// As per spec, a 1-byte code 2 packet is always invalid.
					if (len == 1)
					{
						MS_WARN_DEV("ignoring invalid payload (1)");

						return nullptr;
					}

					// In code 2 packets, DTX is determined by total length = 2 (TOC byte
					// only). Per spec, the only valid 2-byte code 2 packet is one where
					// the length of both frames is zero.
					if (len == 2)
					{
						payloadDescriptor->isDtx = true;
					}

					break;
				}

				case 3:
				{
					// As per spec, a 1-byte code 3 packet is always invalid.
					if (len == 1)
					{
						MS_WARN_DEV("ignoring invalid payload (2)");

						return nullptr;
					}

					// A code 3 packet can never be DTX.

					break;
				}

				default:;
			}

			return payloadDescriptor.release();
		}

		void Opus::ProcessRtpPacket(RTC::RtpPacket* packet)
		{
			MS_TRACE();

			auto* data = packet->GetPayload();
			auto len   = packet->GetPayloadLength();

			PayloadDescriptor* payloadDescriptor = Opus::Parse(data, len);

			if (!payloadDescriptor)
			{
				return;
			}

			auto* payloadDescriptorHandler = new PayloadDescriptorHandler(payloadDescriptor);

			packet->SetPayloadDescriptorHandler(payloadDescriptorHandler);
		}

		/* Instance methods. */

		void Opus::PayloadDescriptor::Dump() const
		{
			MS_TRACE();

			MS_DUMP("<Opus::PayloadDescriptor>");
			MS_DUMP("  stereo: %" PRIu8, this->stereo);
			MS_DUMP("  code: %" PRIu8, this->code);
			MS_DUMP("  isDtx: %s", this->isDtx ? "true" : "false");
			MS_DUMP("</Opus::PayloadDescriptor>");
		}

		Opus::PayloadDescriptorHandler::PayloadDescriptorHandler(Opus::PayloadDescriptor* payloadDescriptor)
		{
			MS_TRACE();

			this->payloadDescriptor.reset(payloadDescriptor);
		}

		bool Opus::PayloadDescriptorHandler::Process(
		  RTC::Codecs::EncodingContext* encodingContext, uint8_t* data, bool& /*marker*/)
		{
			MS_TRACE();

			auto* context = static_cast<RTC::Codecs::Opus::EncodingContext*>(encodingContext);

			if (this->payloadDescriptor->isDtx && context->GetIgnoreDtx())
			{
				return false;
			}
			else
			{
				return true;
			}
		};
	} // namespace Codecs
} // namespace RTC
