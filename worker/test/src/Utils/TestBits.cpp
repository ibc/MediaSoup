#include "common.hpp"
#include "Utils.hpp"
#include <catch2/catch_test_macros.hpp>

SCENARIO("Utils::Bits::CountSetBits()")
{
	uint16_t mask;

	mask = 0b0000000000000000;
	REQUIRE(Utils::Bits::CountSetBits(mask) == 0);

	mask = 0b0000000000000001;
	REQUIRE(Utils::Bits::CountSetBits(mask) == 1);

	mask = 0b1000000000000001;
	REQUIRE(Utils::Bits::CountSetBits(mask) == 2);

	mask = 0b1111111111111111;
	REQUIRE(Utils::Bits::CountSetBits(mask) == 16);
}
