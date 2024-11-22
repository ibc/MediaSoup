#include "common.hpp"
#include "RTC/Codecs/H264_SVC.hpp"
#include <catch2/catch_test_macros.hpp>
#include <cstring> // std::memcmp()

using namespace RTC;

SCENARIO("parse H264_SVC payload descriptor", "[codecs][h264_svc]")
{
	SECTION("parse payload descriptor for NALU 7")
	{
		// clang-format off
		uint8_t originalBuffer[] =
		{
			0x67, 0x42, 0xc0, 0x33
		};
		// clang-format on

		// Keep a copy of the original buffer for comparing.
		uint8_t buffer[4] = { 0 };

		std::memcpy(buffer, originalBuffer, sizeof(buffer));

		std::unique_ptr<RTC::Codecs::H264_SVC::PayloadDescriptor> payloadDescriptor{
			Codecs::H264_SVC::Parse(buffer, sizeof(buffer))
		};

		REQUIRE(payloadDescriptor);

		REQUIRE(payloadDescriptor->tlIndex == 0);
		REQUIRE(payloadDescriptor->slIndex == 0);

		REQUIRE(payloadDescriptor->isKeyFrame == true);
		REQUIRE(payloadDescriptor->hasTlIndex == false);
		REQUIRE(payloadDescriptor->hasSlIndex == false);
	}

	SECTION("parse payload descriptor for NALU 8")
	{
		// clang-format off
		uint8_t originalBuffer[] =
		{
			0x68, 0xce, 0x01, 0xa8
		};
		// clang-format on

		// Keep a copy of the original buffer for comparing.
		uint8_t buffer[4] = { 0 };

		std::memcpy(buffer, originalBuffer, sizeof(buffer));

		std::unique_ptr<RTC::Codecs::H264_SVC::PayloadDescriptor> payloadDescriptor{
			Codecs::H264_SVC::Parse(buffer, sizeof(buffer))
		};

		REQUIRE(payloadDescriptor);

		REQUIRE(payloadDescriptor->tlIndex == 0);
		REQUIRE(payloadDescriptor->slIndex == 0);

		REQUIRE(payloadDescriptor->isKeyFrame == false);
		REQUIRE(payloadDescriptor->hasTlIndex == false);
		REQUIRE(payloadDescriptor->hasSlIndex == false);
	}

	SECTION("parse payload descriptor for NALU 1")
	{
		// clang-format off
		uint8_t originalBuffer[] =
		{
			0x81, 0xe0, 0x00, 0x4e
		};
		// clang-format on

		// Keep a copy of the original buffer for comparing.
		uint8_t buffer[4] = { 0 };

		std::memcpy(buffer, originalBuffer, sizeof(buffer));

		std::unique_ptr<RTC::Codecs::H264_SVC::PayloadDescriptor> payloadDescriptor{
			Codecs::H264_SVC::Parse(buffer, sizeof(buffer))
		};

		REQUIRE(payloadDescriptor);

		REQUIRE(payloadDescriptor->tlIndex == 0);
		REQUIRE(payloadDescriptor->slIndex == 0);

		REQUIRE(payloadDescriptor->isKeyFrame == false);
		REQUIRE(payloadDescriptor->hasTlIndex == false);
		REQUIRE(payloadDescriptor->hasSlIndex == false);
	}

	SECTION("parse payload descriptor for NALU 5")
	{
		// clang-format off
		uint8_t originalBuffer[] =
		{
			0x85, 0xb8, 0x00, 0x04
		};
		// clang-format on

		// Keep a copy of the original buffer for comparing.
		uint8_t buffer[4] = { 0 };

		std::memcpy(buffer, originalBuffer, sizeof(buffer));

		std::unique_ptr<RTC::Codecs::H264_SVC::PayloadDescriptor> payloadDescriptor{
			Codecs::H264_SVC::Parse(buffer, sizeof(buffer))
		};

		REQUIRE(payloadDescriptor);

		REQUIRE(payloadDescriptor->tlIndex == 0);
		REQUIRE(payloadDescriptor->slIndex == 0);

		REQUIRE(payloadDescriptor->isKeyFrame == true);
		REQUIRE(payloadDescriptor->hasTlIndex == false);
		REQUIRE(payloadDescriptor->hasSlIndex == false);
	}

	SECTION("parse payload descriptor for NALU 14")
	{
		// clang-format off
		uint8_t originalBuffer[] =
		{
			0x6e, 0x80, 0x90, 0x20
		};
		// clang-format on

		// Keep a copy of the original buffer for comparing.
		uint8_t buffer[4] = { 0 };

		std::memcpy(buffer, originalBuffer, sizeof(buffer));

		std::unique_ptr<RTC::Codecs::H264_SVC::PayloadDescriptor> payloadDescriptor{
			Codecs::H264_SVC::Parse(buffer, sizeof(buffer))
		};

		REQUIRE(payloadDescriptor);

		REQUIRE(payloadDescriptor->idr == 0);

		REQUIRE(payloadDescriptor->tlIndex == 1);
		REQUIRE(payloadDescriptor->slIndex == 1);

		REQUIRE(payloadDescriptor->isKeyFrame == false);
		REQUIRE(payloadDescriptor->hasTlIndex == true);
		REQUIRE(payloadDescriptor->hasSlIndex == true);
	}

	SECTION("parse payload descriptor for NALU 20")
	{
		// clang-format off
		uint8_t originalBuffer[] =
		{
			0x74, 0x80, 0x90, 0x20
		};
		// clang-format on

		// Keep a copy of the original buffer for comparing.
		uint8_t buffer[4] = { 0 };

		std::memcpy(buffer, originalBuffer, sizeof(buffer));

		std::unique_ptr<RTC::Codecs::H264_SVC::PayloadDescriptor> payloadDescriptor{
			Codecs::H264_SVC::Parse(buffer, sizeof(buffer))
		};

		REQUIRE(payloadDescriptor);

		REQUIRE(payloadDescriptor->idr == 0);

		REQUIRE(payloadDescriptor->tlIndex == 1);
		REQUIRE(payloadDescriptor->slIndex == 1);

		REQUIRE(payloadDescriptor->isKeyFrame == false);
		REQUIRE(payloadDescriptor->hasTlIndex == true);
		REQUIRE(payloadDescriptor->hasSlIndex == true);
	}
}
