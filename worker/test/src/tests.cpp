#include "DepLibSRTP.hpp"
#include "DepLibUV.hpp"
#include "DepLibWebRTC.hpp"
#include "DepOpenSSL.hpp"
#include "DepUsrSCTP.hpp"
#include "LogLevel.hpp"
#include "Settings.hpp"
#include "Utils.hpp"
#include <catch2/catch_session.hpp>
#include <cstdlib> // std::getenv()

int main(int argc, char* argv[])
{
	LogLevel logLevel{ LogLevel::LOG_NONE };

	// Get logLevel from ENV variable.
	if (std::getenv("MS_TEST_LOG_LEVEL"))
	{
		if (std::string(std::getenv("MS_TEST_LOG_LEVEL")) == "debug")
		{
			logLevel = LogLevel::LOG_DEBUG;
		}
		else if (std::string(std::getenv("MS_TEST_LOG_LEVEL")) == "warn")
		{
			logLevel = LogLevel::LOG_WARN;
		}
		else if (std::string(std::getenv("MS_TEST_LOG_LEVEL")) == "error")
		{
			logLevel = LogLevel::LOG_ERROR;
		}
	}

	Settings::configuration.logLevel = logLevel;

	// Fill logTags based by reading ENVs.
	// TODO: Add more on demand.
	if (std::getenv("MS_TEST_LOG_TAG_RTP"))
	{
		Settings::configuration.logTags.rtp = true;
	}
	if (std::getenv("MS_TEST_LOG_TAG_RTCP"))
	{
		Settings::configuration.logTags.rtcp = true;
	}

	// Initialize static stuff.
	DepLibUV::ClassInit();
	DepOpenSSL::ClassInit();
	DepLibSRTP::ClassInit();
	DepUsrSCTP::ClassInit();
	DepLibWebRTC::ClassInit();
	Utils::Crypto::ClassInit();

	Catch::Session session;

	int status = session.run(argc, argv);

	// Free static stuff.
	DepLibSRTP::ClassDestroy();
	Utils::Crypto::ClassDestroy();
	DepLibWebRTC::ClassDestroy();
	DepUsrSCTP::ClassDestroy();
	DepLibUV::ClassDestroy();

	return status;
}
