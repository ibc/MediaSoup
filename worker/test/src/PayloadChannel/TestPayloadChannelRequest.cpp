#include "common.hpp"
#include "PayloadChannel/PayloadChannelRequest.hpp"
#include <catch2/catch_test_macros.hpp>

SCENARIO("PayloadChannelRequest", "[channel][request]")
{
	SECTION("notification")
	{
		char foo[]{ "n:abcd" };

		REQUIRE(PayloadChannel::PayloadChannelRequest::IsRequest(foo, sizeof(foo)) == false);
	}

	SECTION("request")
	{
		char foo[]{ "r:abcd" };

		REQUIRE(PayloadChannel::PayloadChannelRequest::IsRequest(foo, sizeof(foo)) == true);
	}

	SECTION("non request")
	{
		char foo[]{ "abcd" };

		REQUIRE(PayloadChannel::PayloadChannelRequest::IsRequest(foo, sizeof(foo)) == false);
	}

	SECTION("empty string")
	{
		char foo[]{};

		REQUIRE(PayloadChannel::PayloadChannelRequest::IsRequest(foo, sizeof(foo)) == false);
	}
}
