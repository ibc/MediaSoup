#include "common.hpp"
#include "RTC/TrendCalculator.hpp"
#include <catch2/catch_test_macros.hpp>

using namespace RTC;

SCENARIO("TrendCalculator", "[rtc]")
{
	SECTION("trend values after same elapsed time match")
	{
		TrendCalculator trendA;
		TrendCalculator trendB;

		trendA.Update(1000u, 0u);
		trendB.Update(1000u, 0u);

		REQUIRE(trendA.GetValue() == 1000u);
		REQUIRE(trendB.GetValue() == 1000u);

		trendA.Update(200u, 500u);
		trendA.Update(200u, 1000u);
		trendB.Update(200u, 1000u);

		REQUIRE(trendA.GetValue() == trendB.GetValue());

		trendA.Update(200u, 2000u);
		trendA.Update(200u, 4000u);
		trendB.Update(200u, 4000u);

		REQUIRE(trendA.GetValue() == trendB.GetValue());

		trendA.Update(2000u, 5000u);
		trendB.Update(2000u, 5000u);

		REQUIRE(trendA.GetValue() == 2000u);
		REQUIRE(trendB.GetValue() == 2000u);

		trendA.ForceUpdate(0u, 5500u);
		trendB.ForceUpdate(100u, 5000u);

		REQUIRE(trendA.GetValue() == 0u);
		REQUIRE(trendB.GetValue() == 100u);
	}
}
