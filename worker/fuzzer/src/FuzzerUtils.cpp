#include "FuzzerUtils.hpp"
#include "MediaSoupErrors.hpp"
#include "Utils.hpp"
#include <cstring> // std::memory()
#include <string>

void Fuzzer::Utils::Fuzz(const uint8_t* data, size_t len)
{
	uint8_t data2[len + INET6_ADDRSTRLEN * 3]; // For some fuzzers below.
	std::memcpy(data2, data, len);

	/* IP class. */

	std::string ip;

	ip = std::string(reinterpret_cast<const char*>(data2), INET6_ADDRSTRLEN / 2);
	::Utils::IP::GetFamily(ip);

	ip = std::string(reinterpret_cast<const char*>(data2), INET6_ADDRSTRLEN);
	::Utils::IP::GetFamily(ip);

	ip = std::string(reinterpret_cast<const char*>(data2), INET6_ADDRSTRLEN * 2);
	::Utils::IP::GetFamily(ip);

	// Protect with try/catch since throws are legit.
	try
	{
		auto ip = std::string(reinterpret_cast<const char*>(data2), len);

		::Utils::IP::NormalizeIp(ip);
	}
	catch (const MediaSoupError& error)
	{
	}

	/* Byte class. */

	::Utils::Byte::Get1Byte(data2, len);
	::Utils::Byte::Get2Bytes(data2, len);
	::Utils::Byte::Get3Bytes(data2, len);
	::Utils::Byte::Get4Bytes(data2, len);
	::Utils::Byte::Get8Bytes(data2, len);
	::Utils::Byte::Set1Byte(data2, len, uint8_t{ 6u });
	::Utils::Byte::Set2Bytes(data2, len, uint16_t{ 66u });
	::Utils::Byte::Set3Bytes(data2, len, uint32_t{ 666u });
	::Utils::Byte::Set4Bytes(data2, len, uint32_t{ 666u });
	::Utils::Byte::Set8Bytes(data2, len, uint64_t{ 6666u });
	::Utils::Byte::PadTo4Bytes(static_cast<uint16_t>(len));
	::Utils::Byte::PadTo4Bytes(static_cast<uint32_t>(len));

	/* Bits class. */

	::Utils::Bits::CountSetBits(static_cast<uint16_t>(len));

	/* Crypto class. */

	::Utils::Crypto::GetRandomUInt(static_cast<uint32_t>(len), static_cast<uint32_t>(len + 1000000));
	::Utils::Crypto::GetRandomString(len);
	::Utils::Crypto::GetCRC32(data2, len);

	/* String class. */

	// Protect with try/catch since throws are legit.
	try
	{
		size_t outLen;

		::Utils::String::Base64Encode(data2, len);
		::Utils::String::Base64Decode(data2, len, outLen);
	}
	catch (const MediaSoupError& error)
	{
	}

	/* Time class. */

	auto ntp = ::Utils::Time::TimeMs2Ntp(static_cast<uint64_t>(len));

	::Utils::Time::Ntp2TimeMs(ntp);
	::Utils::Time::IsNewerTimestamp(static_cast<uint32_t>(len), static_cast<uint32_t>(len * len));
	::Utils::Time::IsNewerTimestamp(static_cast<uint32_t>(len * len), static_cast<uint32_t>(len));
	::Utils::Time::LatestTimestamp(static_cast<uint32_t>(len), static_cast<uint32_t>(len * len));
	::Utils::Time::LatestTimestamp(static_cast<uint32_t>(len * len), static_cast<uint32_t>(len));
	::Utils::Time::TimeMsToAbsSendTime(static_cast<uint64_t>(len));
}
