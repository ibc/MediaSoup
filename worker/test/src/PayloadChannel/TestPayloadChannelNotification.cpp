#include "common.hpp"
#include "PayloadChannel/PayloadChannelNotification.hpp"
#include <catch2/catch_test_macros.hpp>

SCENARIO("PayloadChannelNotification", "[channel][notification]")
{
	SECTION("notification")
	{
		char foo[]{ "n:abcd" };

		REQUIRE(PayloadChannel::PayloadChannelNotification::IsNotification(foo, sizeof(foo)) == true);
	}

	SECTION("request")
	{
		char foo[]{ "r:abcd" };

		REQUIRE(PayloadChannel::PayloadChannelNotification::IsNotification(foo, sizeof(foo)) == false);
	}

	SECTION("non notification")
	{
		char foo[]{ "abcd" };

		REQUIRE(PayloadChannel::PayloadChannelNotification::IsNotification(foo, sizeof(foo)) == false);
	}

	SECTION("empty string")
	{
		char foo[]{};

		REQUIRE(PayloadChannel::PayloadChannelNotification::IsNotification(foo, sizeof(foo)) == false);
	}
}
