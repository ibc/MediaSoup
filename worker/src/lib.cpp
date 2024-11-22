#define MS_CLASS "mediasoup-worker"
// #define MS_LOG_DEV_LEVEL 3

#include "common.hpp"
#include "DepLibSRTP.hpp"
#ifdef MS_LIBURING_SUPPORTED
#include "DepLibUring.hpp"
#endif
#include "DepLibUV.hpp"
#include "DepLibWebRTC.hpp"
#include "DepOpenSSL.hpp"
#include "DepUsrSCTP.hpp"
#include "Logger.hpp"
#include "MediaSoupErrors.hpp"
#include "Settings.hpp"
#include "Utils.hpp"
#include "Worker.hpp"
#include "Channel/ChannelSocket.hpp"
#include "RTC/DtlsTransport.hpp"
#include "RTC/SrtpSession.hpp"
#include <uv.h>
#include <absl/container/flat_hash_map.h>
#include <csignal> // sigaction()
#include <string>

void IgnoreSignals();

// NOLINTNEXTLINE
extern "C" int mediasoup_worker_run(
  int argc,
  char* argv[],
  const char* version,
  int consumerChannelFd,
  int producerChannelFd,
  ChannelReadFn channelReadFn,
  ChannelReadCtx channelReadCtx,
  ChannelWriteFn channelWriteFn,
  ChannelWriteCtx channelWriteCtx)
{
	// Initialize libuv stuff (we need it for the Channel).
	DepLibUV::ClassInit();

	// Channel socket. If Worker instance runs properly, this socket is closed by
	// it in its destructor. Otherwise it's closed here by also letting libuv
	// deallocate its UV handles.
	std::unique_ptr<Channel::ChannelSocket> channel{ nullptr };

	try
	{
		if (channelReadFn)
		{
			channel.reset(
			  new Channel::ChannelSocket(channelReadFn, channelReadCtx, channelWriteFn, channelWriteCtx));
		}
		else
		{
			channel.reset(new Channel::ChannelSocket(consumerChannelFd, producerChannelFd));
		}
	}
	catch (const MediaSoupError& error)
	{
		MS_ERROR_STD("error creating the Channel: %s", error.what());

		DepLibUV::RunLoop();
		DepLibUV::ClassDestroy();

		// 40 is a custom exit code to notify "unknown error" to the Node library.
		return 40;
	}

	// Initialize the Logger.
	Logger::ClassInit(channel.get());

	try
	{
		Settings::SetConfiguration(argc, argv);
	}
	catch (const MediaSoupTypeError& error)
	{
		MS_ERROR_STD("settings error: %s", error.what());

		channel->Close();
		DepLibUV::RunLoop();
		DepLibUV::ClassDestroy();

		// 42 is a custom exit code to notify "settings error" to the Node library.
		return 42;
	}
	catch (const MediaSoupError& error)
	{
		MS_ERROR_STD("unexpected settings error: %s", error.what());

		channel->Close();
		DepLibUV::RunLoop();
		DepLibUV::ClassDestroy();

		// 40 is a custom exit code to notify "unknown error" to the Node library.
		return 40;
	}

	MS_DEBUG_TAG(info, "starting mediasoup-worker process [version:%s]", version);

#if defined(MS_LITTLE_ENDIAN)
	MS_DEBUG_TAG(info, "little-endian CPU detected");
#elif defined(MS_BIG_ENDIAN)
	MS_DEBUG_TAG(info, "big-endian CPU detected");
#else
	MS_WARN_TAG(info, "cannot determine whether little-endian or big-endian");
#endif

#if defined(INTPTR_MAX) && defined(INT32_MAX) && (INTPTR_MAX == INT32_MAX)
	MS_DEBUG_TAG(info, "32 bits architecture detected");
#elif defined(INTPTR_MAX) && defined(INT64_MAX) && (INTPTR_MAX == INT64_MAX)
	MS_DEBUG_TAG(info, "64 bits architecture detected");
#else
	MS_WARN_TAG(info, "cannot determine 32 or 64 bits architecture");
#endif

	Settings::PrintConfiguration();
	DepLibUV::PrintVersion();

	try
	{
		// Initialize static stuff.
		DepOpenSSL::ClassInit();
		DepLibSRTP::ClassInit();
		DepUsrSCTP::ClassInit();
#ifdef MS_LIBURING_SUPPORTED
		DepLibUring::ClassInit();
#endif
		DepLibWebRTC::ClassInit();
		Utils::Crypto::ClassInit();
		RTC::DtlsTransport::ClassInit();
		RTC::SrtpSession::ClassInit();

#ifdef MS_EXECUTABLE
		// Ignore some signals.
		IgnoreSignals();
#endif

		// Run the Worker.
		const Worker worker(channel.get());

		// Free static stuff.
		DepLibSRTP::ClassDestroy();
		Utils::Crypto::ClassDestroy();
		DepLibWebRTC::ClassDestroy();
#ifdef MS_LIBURING_SUPPORTED
		DepLibUring::ClassDestroy();
#endif
		RTC::DtlsTransport::ClassDestroy();
		DepUsrSCTP::ClassDestroy();
		DepLibUV::ClassDestroy();

#ifdef MS_EXECUTABLE
		// Wait a bit so pending messages to stdout/Channel arrive to the Node
		// process.
		uv_sleep(200);
#endif

		return 0;
	}
	catch (const MediaSoupError& error)
	{
		MS_ERROR_STD("failure exit: %s", error.what());

		// 40 is a custom exit code to notify "unknown error" to the Node library.
		return 40;
	}
}

void IgnoreSignals()
{
#ifndef _WIN32
	MS_TRACE();

	int err;
	struct sigaction act
	{
	}; // NOLINT(cppcoreguidelines-pro-type-member-init)

	// clang-format off
	absl::flat_hash_map<std::string, int> const ignoredSignals =
	{
		{ "PIPE", SIGPIPE },
		{ "HUP",  SIGHUP  },
		{ "ALRM", SIGALRM },
		{ "USR1", SIGUSR1 },
		{ "USR2", SIGUSR2 }
	};
	// clang-format on

	act.sa_handler = SIG_IGN; // NOLINT(cppcoreguidelines-pro-type-cstyle-cast)
	act.sa_flags   = 0;
	err            = sigfillset(&act.sa_mask);

	if (err != 0)
	{
		MS_THROW_ERROR("sigfillset() failed: %s", std::strerror(errno));
	}

	for (const auto& kv : ignoredSignals)
	{
		const auto& sigName = kv.first;
		const int sigId     = kv.second;

		err = sigaction(sigId, &act, nullptr);

		if (err != 0)
		{
			MS_THROW_ERROR("sigaction() failed for signal %s: %s", sigName.c_str(), std::strerror(errno));
		}
	}
#endif
}
