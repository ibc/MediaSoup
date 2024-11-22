#include "common.hpp"
#include "RTC/Codecs/VP9.hpp"
#include <catch2/catch_test_macros.hpp>
#include <cstring> // std::memcmp()

using namespace RTC;

constexpr uint16_t MaxPictureId = (1 << 15) - 1;

Codecs::VP9::PayloadDescriptor* CreateVP9Packet(
  uint8_t* buffer, size_t bufferLen, uint16_t pictureId, uint8_t tlIndex)
{
	buffer[0]             = 0xAD; // I and L bits
	uint16_t netPictureId = htons(pictureId);
	std::memcpy(buffer + 1, &netPictureId, 2);
	buffer[1] |= 0x80;
	buffer[3] = tlIndex << 6;

	auto* payloadDescriptor = Codecs::VP9::Parse(buffer, bufferLen);

	REQUIRE(payloadDescriptor);

	return payloadDescriptor;
}

std::unique_ptr<Codecs::VP9::PayloadDescriptor> ProcessVP9Packet(
  Codecs::VP9::EncodingContext& context, uint16_t pictureId, uint8_t tlIndex)
{
	// clang-format off
	uint8_t buffer[] =
	{
		0xAD, 0x80, 0x00, 0x00, 0x00, 0x00
	};
	// clang-format on
	bool marker;
	auto* payloadDescriptor = CreateVP9Packet(buffer, sizeof(buffer), pictureId, tlIndex);
	std::unique_ptr<Codecs::VP9::PayloadDescriptorHandler> payloadDescriptorHandler(
	  new Codecs::VP9::PayloadDescriptorHandler(payloadDescriptor));

	if (payloadDescriptorHandler->Process(&context, buffer, marker))
	{
		return std::unique_ptr<Codecs::VP9::PayloadDescriptor>(Codecs::VP9::Parse(buffer, sizeof(buffer)));
	}

	return nullptr;
}

SCENARIO("process VP9 payload descriptor", "[codecs][vp9]")
{
	SECTION("drop packets that belong to other temporal layers after rolling over pictureID")
	{
		RTC::Codecs::EncodingContext::Params params;
		params.spatialLayers  = 1;
		params.temporalLayers = 3;

		Codecs::VP9::EncodingContext context(params);
		context.SyncRequired();
		context.SetCurrentTemporalLayer(0);
		context.SetTargetTemporalLayer(0);

		context.SetCurrentSpatialLayer(0);
		context.SetTargetSpatialLayer(0);

		// Frame 1.
		auto forwarded = ProcessVP9Packet(context, MaxPictureId, 0);
		REQUIRE(forwarded);
		REQUIRE(forwarded->pictureId == MaxPictureId);

		// Frame 2.
		forwarded = ProcessVP9Packet(context, 0, 0);
		REQUIRE(forwarded);
		REQUIRE(forwarded->pictureId == 0);

		// Frame 3.
		forwarded = ProcessVP9Packet(context, 1, 1);
		REQUIRE_FALSE(forwarded);
	}
}
