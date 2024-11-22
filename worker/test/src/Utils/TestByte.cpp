#include "common.hpp"
#include "Utils.hpp"
#include <catch2/catch_test_macros.hpp>

SCENARIO("Utils::Byte")
{
	// NOTE: The setup and teardown are implicit in how Catch2 works, meaning that
	// this buffer is initialized before each SECTION below.
	// Docs: https://github.com/catchorg/Catch2/blob/devel/docs/tutorial.md#test-cases-and-sections

	// clang-format off
	uint8_t buffer[] =
	{
		0b00000000, 0b00000001, 0b00000010, 0b00000011,
		0b10000000, 0b01000000, 0b00100000, 0b00010000,
		0b01111111, 0b11111111, 0b11111111, 0b00000000,
		0b11111111, 0b11111111, 0b11111111, 0b00000000,
		0b10000000, 0b00000000, 0b00000000, 0b00000000
	};
	// clang-format on

	SECTION("Utils::Byte::Get3Bytes()")
	{
		// Bytes 4,5 and 6 in the array are number 8405024.
		REQUIRE(Utils::Byte::Get3Bytes(buffer, 4) == 8405024);
	}

	SECTION("Utils::Byte::Set3Bytes()")
	{
		Utils::Byte::Set3Bytes(buffer, 4, 5666777);
		REQUIRE(Utils::Byte::Get3Bytes(buffer, 4) == 5666777);
	}

	SECTION("Utils::Byte::Get3BytesSigned()")
	{
		// Bytes 8, 9 and 10 in the array are number 8388607 since first bit is 0 and
		// all other bits are 1, so it must be maximum positive 24 bits signed integer,
		// which is Math.pow(2, 23) - 1 = 8388607.
		REQUIRE(Utils::Byte::Get3BytesSigned(buffer, 8) == 8388607);

		// Bytes 12, 13 and 14 in the array are number -1.
		REQUIRE(Utils::Byte::Get3BytesSigned(buffer, 12) == -1);

		// Bytes 16, 17 and 18 in the array are number -8388608 since first bit is 1
		// and all other bits are 0, so it must be minimum negative 24 bits signed
		// integer, which is -1 * Math.pow(2, 23) = -8388608.
		REQUIRE(Utils::Byte::Get3BytesSigned(buffer, 16) == -8388608);
	}

	SECTION("Utils::Byte::Set3BytesSigned()")
	{
		Utils::Byte::Set3BytesSigned(buffer, 0, 8388607);
		REQUIRE(Utils::Byte::Get3BytesSigned(buffer, 0) == 8388607);

		Utils::Byte::Set3BytesSigned(buffer, 0, -1);
		REQUIRE(Utils::Byte::Get3BytesSigned(buffer, 0) == -1);

		Utils::Byte::Set3BytesSigned(buffer, 0, -8388608);
		REQUIRE(Utils::Byte::Get3BytesSigned(buffer, 0) == -8388608);
	}
}
