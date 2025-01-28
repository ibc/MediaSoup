#include "common.hpp"
#include "RTC/Codecs/VP9.hpp"
#include <catch2/catch_test_macros.hpp>
#include <cstring> // std::memcmp()

using namespace RTC;

constexpr uint16_t MaxPictureId = (1 << 15) - 1;

Codecs::VP9::PayloadDescriptor* CreateVP9Packet(
  uint8_t* buffer, size_t bufferLen, uint16_t pictureId, uint8_t tlIndex)
{
	buffer[0]             = 0xAD; // I, L, B, E bits
	uint16_t netPictureId = htons(pictureId);
	std::memcpy(buffer + 1, &netPictureId, 2);
	buffer[1] |= 0x80;
	buffer[3] = (tlIndex << 5) | (1 << 4); // tlIndex, switchingUpPoint

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

	SECTION("test PayloadDescriptorHandler")
	{
		RTC::Codecs::EncodingContext::Params params;
		params.spatialLayers  = 1;
		params.temporalLayers = 3;

		Codecs::VP9::EncodingContext context(params);

		uint16_t start = MaxPictureId - 2000;

		context.SetCurrentTemporalLayer(0, start + 0);
		context.SetCurrentTemporalLayer(1, start + 1);
		context.SetCurrentTemporalLayer(2, start + 5);
		context.SetCurrentTemporalLayer(0, start + 6);

		REQUIRE(context.GetCurrentTemporalLayer(start + 0) == 0);
		REQUIRE(context.GetCurrentTemporalLayer(start + 1) == 1);
		REQUIRE(context.GetCurrentTemporalLayer(start + 2) == 1);
		REQUIRE(context.GetCurrentTemporalLayer(start + 5) == 2);
		REQUIRE(context.GetCurrentTemporalLayer(start + 6) == 0);

		context.SetCurrentTemporalLayer(1, start + 1000);
		context.SetCurrentTemporalLayer(2, start + 1001); // This will drop the first item.

		REQUIRE(context.GetCurrentTemporalLayer(start + 1000) == 1);
		REQUIRE(context.GetCurrentTemporalLayer(start + 0) == 1); // It will get the item at start+1.

		context.SetCurrentTemporalLayer(0, 0); // This will drop items from start to start+999.

		REQUIRE(context.GetCurrentTemporalLayer(0) == 0);
		REQUIRE(context.GetCurrentTemporalLayer(start + 0) == 1); // It will get the item at start+1000.
	}

	SECTION("drop packets that belong to other temporal layers with unordered pictureID")
	{
		RTC::Codecs::EncodingContext::Params params;
		params.spatialLayers  = 1;
		params.temporalLayers = 3;

		Codecs::VP9::EncodingContext context(params);
		context.SyncRequired();
		context.SetCurrentSpatialLayer(0, 0);
		context.SetTargetSpatialLayer(0);

		uint16_t start                                                     = MaxPictureId - 20;
		std::vector<std::tuple<uint16_t, uint16_t, int16_t, bool>> packets = {
			// targetTemporalLayer=0
			{ start, 0, 0, true },
			{ start, 1, -1, false },
			{ start + 1, 0, -1, true },
			{ start + 1, 1, -1, false },
			{ start + 2, 0, -1, true },
			{ start + 2, 1, -1, false },
			// targetTemporalLayer=1
			{ start + 10, 0, 1, true },
			{ start + 10, 1, -1, true },
			{ start + 11, 0, -1, true },
			{ start + 11, 1, -1, true },

			{ start + 3, 0, -1, true }, // old packet
			{ start + 3, 1, -1, false },
			{ start + 12, 0, -1, true },
			{ start + 12, 1, -1, true },
			// targetTemporalLayer=0
			{ start + 14, 0, 0, true },
			{ start + 14, 1, -1, false },
			{ start + 13, 0, -1, true }, // old packet
			{ start + 13, 1, -1, true },
			// targetTemporalLayer=1
			{ start + 15, 0, 1, true },
			{ start + 15, 1, -1, true },
			// targetTemporalLayer=0
			{ 0, 0, 0, true },
			{ 0, 1, -1, false },
			{ 1, 0, -1, true },
			{ 1, 1, -1, false },
			{ start + 16, 0, -1, true }, // old packet
			{ start + 16, 1, -1, true },
		};

		for (const auto& packet : packets)
		{
			auto pictureId           = std::get<0>(packet);
			auto tlIndex             = std::get<1>(packet);
			auto targetTemporalLayer = std::get<2>(packet);
			auto shouldForward       = std::get<3>(packet);

			if (targetTemporalLayer >= 0)
			{
				context.SetTargetTemporalLayer(targetTemporalLayer);
			}

			auto forwarded = ProcessVP9Packet(context, pictureId, tlIndex);

			if (shouldForward)
			{
				REQUIRE(forwarded);
				REQUIRE(forwarded->pictureId == pictureId);
			}
			else
			{
				REQUIRE_FALSE(forwarded);
			}
		}
	}
}
