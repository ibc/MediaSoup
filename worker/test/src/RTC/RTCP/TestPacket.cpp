#include "common.hpp"
#include "RTC/RTCP/Packet.hpp"
#include <catch2/catch_test_macros.hpp>

using namespace RTC::RTCP;

SCENARIO("RTCP parsing", "[parser][rtcp][packet]")
{
	// RTCP common header
	// Version:2, Padding:false, Count:0, Type:200(SR), Lengh:0

	// clang-format off
	uint8_t buffer[] =
	{
		0x80, 0xc8, 0x00, 0x00
	};
	// clang-format on

	SECTION("a RTCP packet may only contain the RTCP common header")
	{
		std::unique_ptr<Packet> packet{ Packet::Parse(buffer, sizeof(buffer)) };

		REQUIRE(packet);
	}

	SECTION("a too small RTCP packet should fail")
	{
		// Provide a wrong packet length.
		size_t length = sizeof(buffer) - 1;

		std::unique_ptr<Packet> packet{ Packet::Parse(buffer, length) };

		REQUIRE_FALSE(packet);
	}

	SECTION("a RTCP packet with incorrect version should fail")
	{
		// Set an incorrect version value (0).
		buffer[0] &= 0b00111111;

		std::unique_ptr<Packet> packet{ Packet::Parse(buffer, sizeof(buffer)) };

		REQUIRE_FALSE(packet);
	}

	SECTION("a RTCP packet with incorrect length should fail")
	{
		// Set the packet length to zero.
		buffer[3] = 1;

		std::unique_ptr<Packet> packet{ Packet::Parse(buffer, sizeof(buffer)) };

		REQUIRE_FALSE(packet);
	}

	SECTION("a RTCP packet with unknown type should fail")
	{
		// Set and unknown packet type (0).
		buffer[1] = 0;

		std::unique_ptr<Packet> packet{ Packet::Parse(buffer, sizeof(buffer)) };

		REQUIRE_FALSE(packet);
	}
}
