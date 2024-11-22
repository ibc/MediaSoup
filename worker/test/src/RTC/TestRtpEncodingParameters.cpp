#include "common.hpp"
#include <catch2/catch_test_macros.hpp>
#include <regex>

static const std::regex ScalabilityModeRegex(
  "^[LS]([1-9]\\d{0,1})T([1-9]\\d{0,1})(_KEY)?.*", std::regex_constants::ECMAScript);

struct scalabilityMode
{
	uint8_t spatialLayers  = 1;
	uint8_t temporalLayers = 1;
	bool ksvc              = false;
};

static struct scalabilityMode parseScalabilityMode(const std::string& scalabilityMode)
{
	struct scalabilityMode result;
	std::smatch match;

	std::regex_match(scalabilityMode, match, ScalabilityModeRegex);

	if (!match.empty())
	{
		try
		{
			result.spatialLayers  = std::stoul(match[1].str());
			result.temporalLayers = std::stoul(match[2].str());
			result.ksvc           = match.size() >= 4 && match[3].str() == "_KEY";
		}
		catch (std::exception& e)
		{
		}
	}

	return result;
}

SCENARIO("parseScalabilityMode", "[rtc]")
{
	SECTION("parse L1T3")
	{
		const auto scalabilityMode = parseScalabilityMode("L1T3");

		REQUIRE(scalabilityMode.spatialLayers == 1);
		REQUIRE(scalabilityMode.temporalLayers == 3);
		REQUIRE(scalabilityMode.ksvc == false);
	}

	SECTION("parse S1T3")
	{
		const auto scalabilityMode = parseScalabilityMode("S1T3");

		REQUIRE(scalabilityMode.spatialLayers == 1);
		REQUIRE(scalabilityMode.temporalLayers == 3);
		REQUIRE(scalabilityMode.ksvc == false);
	}

	SECTION("parse L3T2_KEY")
	{
		const auto scalabilityMode = parseScalabilityMode("L3T2_KEY");

		REQUIRE(scalabilityMode.spatialLayers == 3);
		REQUIRE(scalabilityMode.temporalLayers == 2);
		REQUIRE(scalabilityMode.ksvc == true);
	}

	SECTION("parse S2T3")
	{
		const auto scalabilityMode = parseScalabilityMode("S2T3");

		REQUIRE(scalabilityMode.spatialLayers == 2);
		REQUIRE(scalabilityMode.temporalLayers == 3);
		REQUIRE(scalabilityMode.ksvc == false);
	}

	SECTION("parse foo")
	{
		const auto scalabilityMode = parseScalabilityMode("foo");

		REQUIRE(scalabilityMode.spatialLayers == 1);
		REQUIRE(scalabilityMode.temporalLayers == 1);
		REQUIRE(scalabilityMode.ksvc == false);
	}

	SECTION("parse ''")
	{
		const auto scalabilityMode = parseScalabilityMode("");

		REQUIRE(scalabilityMode.spatialLayers == 1);
		REQUIRE(scalabilityMode.temporalLayers == 1);
		REQUIRE(scalabilityMode.ksvc == false);
	}

	SECTION("parse S0T3")
	{
		const auto scalabilityMode = parseScalabilityMode("S0T3");

		REQUIRE(scalabilityMode.spatialLayers == 1);
		REQUIRE(scalabilityMode.temporalLayers == 1);
		REQUIRE(scalabilityMode.ksvc == false);
	}

	SECTION("parse S1T0")
	{
		const auto scalabilityMode = parseScalabilityMode("S1T0");

		REQUIRE(scalabilityMode.spatialLayers == 1);
		REQUIRE(scalabilityMode.temporalLayers == 1);
		REQUIRE(scalabilityMode.ksvc == false);
	}

	SECTION("parse S20T3")
	{
		const auto scalabilityMode = parseScalabilityMode("S20T3");

		REQUIRE(scalabilityMode.spatialLayers == 20);
		REQUIRE(scalabilityMode.temporalLayers == 3);
		REQUIRE(scalabilityMode.ksvc == false);
	}

	SECTION("parse S200T3")
	{
		const auto scalabilityMode = parseScalabilityMode("S200T3");

		REQUIRE(scalabilityMode.spatialLayers == 1);
		REQUIRE(scalabilityMode.temporalLayers == 1);
		REQUIRE(scalabilityMode.ksvc == false);
	}
}
