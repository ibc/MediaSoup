#ifndef MS_SETTINGS_HPP
#define MS_SETTINGS_HPP

#include "common.hpp"
#include "LogLevel.hpp"
#include "Channel/ChannelRequest.hpp"
#include <absl/container/flat_hash_map.h>
#include <string>
#include <vector>

class Settings
{
public:
	struct LogTags
	{
		bool info{ false };
		bool ice{ false };
		bool dtls{ false };
		bool rtp{ false };
		bool srtp{ false };
		bool rtcp{ false };
		bool rtx{ false };
		bool bwe{ false };
		bool score{ false };
		bool simulcast{ false };
		bool svc{ false };
		bool sctp{ false };
		bool message{ false };
	};

public:
	// Struct holding the configuration.
	struct Configuration
	{
		LogLevel logLevel{ LogLevel::LOG_ERROR };
		struct LogTags logTags;
		uint16_t rtcMinPort{ 10000u };
		uint16_t rtcMaxPort{ 59999u };
		std::string dtlsCertificateFile;
		std::string dtlsPrivateKeyFile;
		std::string libwebrtcFieldTrials{ "WebRTC-Bwe-AlrLimitedBackoff/Enabled/" };
		bool liburingDisabled{ false };
	};

public:
	static void SetConfiguration(int argc, char* argv[]);
	static void PrintConfiguration();
	static void HandleRequest(Channel::ChannelRequest* request);

private:
	static void SetLogLevel(std::string& level);
	static void SetLogTags(const std::vector<std::string>& tags);
	static void SetDtlsCertificateAndPrivateKeyFiles();

public:
	thread_local static struct Configuration configuration;

private:
	static absl::flat_hash_map<std::string, LogLevel> String2LogLevel; // NOLINT(readability-identifier-naming)
	static absl::flat_hash_map<LogLevel, std::string> LogLevel2String; // NOLINT(readability-identifier-naming)
};

#endif
