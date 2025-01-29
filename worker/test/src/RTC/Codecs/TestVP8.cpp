#include "common.hpp"
#include "RTC/Codecs/VP8.hpp"
#include <catch2/catch_test_macros.hpp>
#include <cstring> // std::memcmp(), std::memcpy()

using namespace RTC;

constexpr uint16_t MaxPictureId = (1 << 15) - 1;

SCENARIO("parse VP8 payload descriptor", "[codecs][vp8]")
{
	SECTION("parse payload descriptor")
	{
		/** VP8 Payload Descriptor
		 *
		 * 1 = X bit: Extended control bits present (I L T K)
		 * 1 = R bit: Reserved for future use (Error should be zero)
		 * 0 = N bit: Reference frame
		 * 1 = S bit: Start of VP8 partition
		 * Part Id: 0
		 * 1 = I bit: Picture ID byte present
		 * 0 = L bit: TL0PICIDX byte not present
		 * 0 = T bit: TID (temporal layer index) byte not present
		 * 0 = K bit: TID/KEYIDX byte not present
		 * 0000 = Reserved A: 0
		 * 0001 0001 = Picture Id: 17
		 */

		// clang-format off
		uint8_t originalBuffer[] =
		{
			0xd0, 0x80, 0x11, 0x00
		};
		// clang-format on

		// Keep a copy of the original buffer for comparing.
		uint8_t buffer[4] = { 0 };

		std::memcpy(buffer, originalBuffer, sizeof(buffer));

		std::unique_ptr<Codecs::VP8::PayloadDescriptor> payloadDescriptor{ Codecs::VP8::Parse(
			buffer, sizeof(buffer)) };

		REQUIRE(payloadDescriptor);

		REQUIRE(payloadDescriptor->extended == 1);
		REQUIRE(payloadDescriptor->nonReference == 0);
		REQUIRE(payloadDescriptor->start == 1);
		REQUIRE(payloadDescriptor->partitionIndex == 0);

		// optional field flags.
		REQUIRE(payloadDescriptor->i == 1);
		REQUIRE(payloadDescriptor->l == 0);
		REQUIRE(payloadDescriptor->t == 0);
		REQUIRE(payloadDescriptor->k == 0);

		// optional fields.
		REQUIRE(payloadDescriptor->pictureId == 17);
		REQUIRE(payloadDescriptor->tl0PictureIndex == 0);
		REQUIRE(payloadDescriptor->tlIndex == 0);
		REQUIRE(payloadDescriptor->y == 0);
		REQUIRE(payloadDescriptor->keyIndex == 0);

		REQUIRE(payloadDescriptor->isKeyFrame == true);
		REQUIRE(payloadDescriptor->hasPictureId == true);
		REQUIRE(payloadDescriptor->hasOneBytePictureId == true);
		REQUIRE(payloadDescriptor->hasTwoBytesPictureId == false);
		REQUIRE(payloadDescriptor->hasTl0PictureIndex == false);
		REQUIRE(payloadDescriptor->hasTlIndex == false);

		SECTION("encode payload descriptor")
		{
			payloadDescriptor->Encode(
			  buffer, payloadDescriptor->pictureId, payloadDescriptor->tl0PictureIndex);

			SECTION("compare encoded payloadDescriptor with original buffer")
			{
				REQUIRE(std::memcmp(buffer, originalBuffer, sizeof(buffer)) == 0);
			}
		}
	}

	SECTION("parse payload descriptor 2")
	{
		/** VP8 Payload Descriptor
		 *
		 * 1 = X bit: Extended control bits present (I L T K)
		 * 0 = R bit: Reserved for future use
		 * 0 = N bit: Reference frame
		 * 0 = S bit: Continuation of VP8 partition
		 * 000 = Part Id: 0
		 * 0 = I bit: No Picture byte ID
		 * 0 = L bit: TL0PICIDX byte not present
		 * 1 = T bit: TID (temporal layer index) byte present
		 * 1 = K bit: TID/KEYIDX byte present
		 * 1110 = Reserved A: 14
		 * 11 = Temporal layer Index (TID): 3
		 * 1 = 1 Lay Sync Bit (Y): True
		 * ...0 0100 = Temporal Key Frame Index (KEYIDX): 4
		 */

		// clang-format off
		uint8_t originalBuffer[] =
		{
		  0x88, 0x3e, 0xe4, 0x00
		};
		// clang-format on

		// Keep a copy of the original buffer for comparing.
		uint8_t buffer[4] = { 0 };

		std::memcpy(buffer, originalBuffer, sizeof(buffer));

		// Parse the buffer.
		std::unique_ptr<Codecs::VP8::PayloadDescriptor> payloadDescriptor{ Codecs::VP8::Parse(
			buffer, sizeof(buffer)) };

		REQUIRE(payloadDescriptor);

		REQUIRE(payloadDescriptor->extended == 1);
		REQUIRE(payloadDescriptor->nonReference == 0);
		REQUIRE(payloadDescriptor->start == 0);
		REQUIRE(payloadDescriptor->partitionIndex == 0);

		// optional field flags.
		REQUIRE(payloadDescriptor->i == 0);
		REQUIRE(payloadDescriptor->l == 0);
		REQUIRE(payloadDescriptor->t == 1);
		REQUIRE(payloadDescriptor->k == 1);

		// optional fields.
		REQUIRE(payloadDescriptor->pictureId == 0);
		REQUIRE(payloadDescriptor->tl0PictureIndex == 0);
		REQUIRE(payloadDescriptor->tlIndex == 3);
		REQUIRE(payloadDescriptor->y == 1);
		REQUIRE(payloadDescriptor->keyIndex == 4);

		REQUIRE(payloadDescriptor->isKeyFrame == false);
		REQUIRE(payloadDescriptor->hasPictureId == false);
		REQUIRE(payloadDescriptor->hasOneBytePictureId == false);
		REQUIRE(payloadDescriptor->hasTwoBytesPictureId == false);
		REQUIRE(payloadDescriptor->hasTl0PictureIndex == false);
		REQUIRE(payloadDescriptor->hasTlIndex == true);

		SECTION("encode payload descriptor")
		{
			payloadDescriptor->Encode(
			  buffer, payloadDescriptor->pictureId, payloadDescriptor->tl0PictureIndex);

			SECTION("compare encoded payloadDescriptor with original buffer")
			{
				REQUIRE(std::memcmp(buffer, originalBuffer, sizeof(buffer)) == 0);
			}
		}
	};

	SECTION("parse payload descriptor. I flag set but no space for pictureId")
	{
		/** VP8 Payload Descriptor
		 * 1 = X bit: Extended control bits present (I L T K)
		 * 1 = R bit: Reserved for future use (Error should be zero)
		 * 0 = N bit: Reference frame
		 * 1 = S bit: Start of VP8 partition
		 * Part Id: 0
		 * 1 = I bit: Picture ID byte present
		 * 0 = L bit: TL0PICIDX byte not present
		 * 0 = T bit: TID (temporal layer index) byte not present
		 * 0 = K bit: TID/KEYIDX byte not present
		 * 0000 = Reserved A: 0
		 */

		// clang-format off
		uint8_t buffer[] =
		{
			0xd0, 0x80
		};
		// clang-format on

		auto payloadDescriptor = Codecs::VP8::Parse(buffer, sizeof(buffer));

		REQUIRE_FALSE(payloadDescriptor);
	}

	SECTION("parse payload descriptor. X flag is not set")
	{
		/** VP8 Payload Descriptor
		 *
		 * 0 = X bit: Extended control bits present (I L T K)
		 * 1 = R bit: Reserved for future use (Error should be zero)
		 * 0 = N bit: Reference frame
		 * 1 = S bit: Start of VP8 partition
		 * Part Id: 0
		 * 1 = I bit: Picture ID byte present
		 * 0 = L bit: TL0PICIDX byte not present
		 * 0 = T bit: TID (temporal layer index) byte not present
		 * 0 = K bit: TID/KEYIDX byte not present
		 * 0000 = Reserved A: 0
		 * 0001 0001 = Picture Id: 17
		 */

		// clang-format off
		uint8_t buffer[] =
		{
			0x50, 0x80, 0x11
		};
		// clang-format on

		auto payloadDescriptor = Codecs::VP8::Parse(buffer, sizeof(buffer));

		REQUIRE_FALSE(payloadDescriptor);
	}
}

Codecs::VP8::PayloadDescriptor* CreatePacket(
  uint8_t* buffer,
  size_t bufferLen,
  uint16_t pictureId,
  uint8_t tl0PictureIndex,
  uint8_t tlIndex,
  bool layerSync = true)
{
	uint16_t netPictureId = htons(pictureId);
	std::memcpy(buffer + 2, &netPictureId, 2);
	buffer[2] |= 0x80;
	buffer[4] = tl0PictureIndex;
	buffer[5] = tlIndex << 6;

	if (layerSync)
	{
		buffer[5] |= 0x20; // y bit
	}

	auto* payloadDescriptor = Codecs::VP8::Parse(buffer, bufferLen);

	REQUIRE(payloadDescriptor);

	return payloadDescriptor;
}

std::unique_ptr<Codecs::VP8::PayloadDescriptor> ProcessPacket(
  Codecs::VP8::EncodingContext& context,
  uint16_t pictureId,
  uint8_t tl0PictureIndex,
  uint8_t tlIndex,
  bool layerSync = true)
{
	// clang-format off
	uint8_t buffer[] =
	{
		0x90, 0xe0, 0x80, 0x00, 0x00, 0x00
	};
	// clang-format on
	bool marker;
	auto* payloadDescriptor =
	  CreatePacket(buffer, sizeof(buffer), pictureId, tl0PictureIndex, tlIndex, layerSync);
	std::unique_ptr<Codecs::VP8::PayloadDescriptorHandler> payloadDescriptorHandler(
	  new Codecs::VP8::PayloadDescriptorHandler(payloadDescriptor));

	if (payloadDescriptorHandler->Process(&context, buffer, marker))
	{
		return std::unique_ptr<Codecs::VP8::PayloadDescriptor>(Codecs::VP8::Parse(buffer, sizeof(buffer)));
	}

	return nullptr;
}

SCENARIO("process VP8 payload descriptor", "[codecs][vp8]")
{
	SECTION("do not drop TL0PICIDX from temporal layers higher than 0")
	{
		RTC::Codecs::EncodingContext::Params params;
		params.spatialLayers  = 0;
		params.temporalLayers = 2;
		Codecs::VP8::EncodingContext context(params);

		context.SetCurrentTemporalLayer(0);
		context.SetTargetTemporalLayer(0);

		// Frame 1.
		auto forwarded = ProcessPacket(context, 0, 0, 0);
		REQUIRE(forwarded);
		REQUIRE(forwarded->pictureId == 0);
		REQUIRE(forwarded->tl0PictureIndex == 0);

		// Frame 2 gets lost.

		// Frame 3.
		forwarded = ProcessPacket(context, 2, 1, 1);
		REQUIRE_FALSE(forwarded);

		// Frame 2 retransmitted.
		forwarded = ProcessPacket(context, 1, 1, 0);
		REQUIRE(forwarded);
		REQUIRE(forwarded->pictureId == 1);
		REQUIRE(forwarded->tl0PictureIndex == 1);
	}

	SECTION("drop packets that belong to other temporal layers after rolling over pictureID")
	{
		RTC::Codecs::EncodingContext::Params params;
		params.spatialLayers  = 0;
		params.temporalLayers = 2;
		Codecs::VP8::EncodingContext context(params);
		context.SyncRequired();

		context.SetCurrentTemporalLayer(0);
		context.SetTargetTemporalLayer(0);

		// Frame 1.
		auto forwarded = ProcessPacket(context, MaxPictureId, 0, 0);
		REQUIRE(forwarded);
		REQUIRE(forwarded->pictureId == 1);
		REQUIRE(forwarded->tl0PictureIndex == 1);

		// Frame 2.
		forwarded = ProcessPacket(context, 0, 0, 0);
		REQUIRE(forwarded);
		REQUIRE(forwarded->pictureId == 2);
		REQUIRE(forwarded->tl0PictureIndex == 1);

		// Frame 3.
		forwarded = ProcessPacket(context, 1, 0, 1);
		REQUIRE_FALSE(forwarded);
	}

	SECTION("old packets with higher temporal layer than current are dropped")
	{
		RTC::Codecs::EncodingContext::Params params;
		params.spatialLayers  = 0;
		params.temporalLayers = 2;
		Codecs::VP8::EncodingContext context(params);
		context.SyncRequired();

		context.SetCurrentTemporalLayer(0);
		context.SetTargetTemporalLayer(0);

		// Frame 1.
		auto forwarded = ProcessPacket(context, 1, 0, 0);
		REQUIRE(forwarded);
		REQUIRE(forwarded->pictureId == 1);
		REQUIRE(forwarded->tlIndex == 0);
		REQUIRE(forwarded->tl0PictureIndex == 1);

		// Frame 2.
		forwarded = ProcessPacket(context, 2, 0, 0);
		REQUIRE(forwarded);
		REQUIRE(forwarded->pictureId == 2);
		REQUIRE(forwarded->tlIndex == 0);
		REQUIRE(forwarded->tl0PictureIndex == 1);

		// Frame 3. Old packet with higher temporal layer than current.
		forwarded = ProcessPacket(context, 0, 0, 1);
		REQUIRE_FALSE(forwarded);
		REQUIRE(context.GetCurrentTemporalLayer() == 0);
	}

	SECTION("packets with higher temporal layer than current are dropped")
	{
		RTC::Codecs::EncodingContext::Params params;
		params.spatialLayers  = 0;
		params.temporalLayers = 2;
		Codecs::VP8::EncodingContext context(params);
		context.SyncRequired();

		context.SetCurrentTemporalLayer(0);
		context.SetTargetTemporalLayer(0);

		// Frame 1.
		auto forwarded = ProcessPacket(context, 1, 0, 0);
		REQUIRE(forwarded);
		REQUIRE(forwarded->pictureId == 1);
		REQUIRE(forwarded->tlIndex == 0);
		REQUIRE(forwarded->tl0PictureIndex == 1);

		// Frame 2.
		forwarded = ProcessPacket(context, 2, 0, 0);
		REQUIRE(forwarded);
		REQUIRE(forwarded->pictureId == 2);
		REQUIRE(forwarded->tlIndex == 0);
		REQUIRE(forwarded->tl0PictureIndex == 1);

		context.SetTargetTemporalLayer(2);

		// Frame 3. Old packet with higher temporal layer than current.
		forwarded = ProcessPacket(context, 3, 0, 1);
		REQUIRE_FALSE(forwarded);
		REQUIRE(context.GetCurrentTemporalLayer() == 0);
	}
}
