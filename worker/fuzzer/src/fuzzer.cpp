#define MS_CLASS "fuzzer"

#include "DepLibSRTP.hpp"
#include "DepLibUV.hpp"
#include "DepLibWebRTC.hpp"
#include "DepOpenSSL.hpp"
#include "DepUsrSCTP.hpp"
#include "FuzzerUtils.hpp"
#include "LogLevel.hpp"
#include "Settings.hpp"
#include "Utils.hpp"
#include "RTC/Codecs/FuzzerH264.hpp"
#include "RTC/Codecs/FuzzerH264_SVC.hpp"
#include "RTC/Codecs/FuzzerOpus.hpp"
#include "RTC/Codecs/FuzzerVP8.hpp"
#include "RTC/Codecs/FuzzerVP9.hpp"
#include "RTC/DtlsTransport.hpp"
#include "RTC/FuzzerDtlsTransport.hpp"
#include "RTC/FuzzerRateCalculator.hpp"
#include "RTC/FuzzerRtpPacket.hpp"
#include "RTC/FuzzerRtpRetransmissionBuffer.hpp"
#include "RTC/FuzzerRtpStreamSend.hpp"
#include "RTC/FuzzerSeqManager.hpp"
#include "RTC/FuzzerStunPacket.hpp"
#include "RTC/FuzzerTrendCalculator.hpp"
#include "RTC/RTCP/FuzzerPacket.hpp"
#include <cstdlib> // std::getenv()
#include <iostream>
#include <stddef.h>
#include <stdint.h>

bool fuzzStun   = false;
bool fuzzDtls   = false;
bool fuzzRtp    = false;
bool fuzzRtcp   = false;
bool fuzzCodecs = false;
bool fuzzUtils  = false;

// This Init() function must be declared static, otherwise linking will fail if
// another source file defines same non static Init() function.
static int Init();

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t len)
{
	// Trick to initialize our stuff just once.
	static int unused = Init();

	// Avoid [-Wunused-variable].
	unused++;

	if (fuzzStun)
	{
		Fuzzer::RTC::StunPacket::Fuzz(data, len);
	}

	if (fuzzDtls)
	{
		Fuzzer::RTC::DtlsTransport::Fuzz(data, len);
	}

	if (fuzzRtp)
	{
		Fuzzer::RTC::RtpPacket::Fuzz(data, len);
		Fuzzer::RTC::RtpStreamSend::Fuzz(data, len);
		Fuzzer::RTC::RtpRetransmissionBuffer::Fuzz(data, len);
		Fuzzer::RTC::SeqManager::Fuzz(data, len);
		Fuzzer::RTC::RateCalculator::Fuzz(data, len);
	}

	if (fuzzRtcp)
	{
		Fuzzer::RTC::RTCP::Packet::Fuzz(data, len);
	}

	if (fuzzCodecs)
	{
		Fuzzer::RTC::Codecs::Opus::Fuzz(data, len);
		Fuzzer::RTC::Codecs::VP8::Fuzz(data, len);
		Fuzzer::RTC::Codecs::VP9::Fuzz(data, len);
		Fuzzer::RTC::Codecs::H264::Fuzz(data, len);
		Fuzzer::RTC::Codecs::H264_SVC::Fuzz(data, len);
	}

	if (fuzzUtils)
	{
		Fuzzer::Utils::Fuzz(data, len);
		Fuzzer::RTC::TrendCalculator::Fuzz(data, len);
	}

	return 0;
}

int Init()
{
	LogLevel logLevel{ LogLevel::LOG_NONE };

	// Get logLevel from ENV variable.
	if (std::getenv("MS_FUZZ_LOG_LEVEL"))
	{
		if (std::string(std::getenv("MS_FUZZ_LOG_LEVEL")) == "debug")
		{
			logLevel = LogLevel::LOG_DEBUG;
		}
		else if (std::string(std::getenv("MS_FUZZ_LOG_LEVEL")) == "warn")
		{
			logLevel = LogLevel::LOG_WARN;
		}
		else if (std::string(std::getenv("MS_FUZZ_LOG_LEVEL")) == "error")
		{
			logLevel = LogLevel::LOG_ERROR;
		}
	}

	// Select what to fuzz.

	if (std::getenv("MS_FUZZ_STUN") && std::string(std::getenv("MS_FUZZ_STUN")) == "1")
	{
		std::cout << "[fuzzer] STUN fuzzer enabled" << std::endl;

		fuzzStun = true;
	}
	if (std::getenv("MS_FUZZ_DTLS") && std::string(std::getenv("MS_FUZZ_DTLS")) == "1")
	{
		std::cout << "[fuzzer] DTLS fuzzer enabled" << std::endl;

		fuzzDtls = true;
	}
	if (std::getenv("MS_FUZZ_RTP") && std::string(std::getenv("MS_FUZZ_RTP")) == "1")
	{
		std::cout << "[fuzzer] RTP fuzzer enabled" << std::endl;

		fuzzRtp = true;
	}
	if (std::getenv("MS_FUZZ_RTCP") && std::string(std::getenv("MS_FUZZ_RTCP")) == "1")
	{
		std::cout << "[fuzzer] RTCP fuzzer enabled" << std::endl;

		fuzzRtcp = true;
	}
	if (std::getenv("MS_FUZZ_CODECS") && std::string(std::getenv("MS_FUZZ_CODECS")) == "1")
	{
		std::cout << "[fuzzer] codecs fuzzer enabled" << std::endl;

		fuzzCodecs = true;
	}
	if (std::getenv("MS_FUZZ_UTILS") && std::string(std::getenv("MS_FUZZ_UTILS")) == "1")
	{
		std::cout << "[fuzzer] Utils fuzzer enabled" << std::endl;

		fuzzUtils = true;
	}
	if (!fuzzStun && !fuzzDtls && !fuzzRtp && !fuzzRtcp && !fuzzCodecs && !fuzzUtils)
	{
		std::cout << "[fuzzer] all fuzzers enabled" << std::endl;

		fuzzStun   = true;
		fuzzDtls   = true;
		fuzzRtp    = true;
		fuzzRtcp   = true;
		fuzzCodecs = true;
		fuzzUtils  = true;
	}

	Settings::configuration.logLevel = logLevel;

	// Initialize static stuff.
	DepLibUV::ClassInit();
	DepOpenSSL::ClassInit();
	DepLibSRTP::ClassInit();
	DepUsrSCTP::ClassInit();
	DepLibWebRTC::ClassInit();
	Utils::Crypto::ClassInit();
	::RTC::DtlsTransport::ClassInit();

	return 0;
}
