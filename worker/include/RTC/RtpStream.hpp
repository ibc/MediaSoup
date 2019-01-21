#ifndef MS_RTC_RTP_STREAM_HPP
#define MS_RTC_RTP_STREAM_HPP

#include "common.hpp"
#include "json.hpp"
#include "RTC/RtpDataCounter.hpp"
#include "RTC/RtpDictionaries.hpp"
#include "RTC/RtpPacket.hpp"
#include "handles/Timer.hpp"
#include <string>

using json = nlohmann::json;

namespace RTC
{
	class RtpStream : public Timer::Listener
	{
	public:
		struct Params
		{
			void FillJson(json& jsonObject) const;

			uint32_t ssrc{ 0 };
			uint8_t payloadType{ 0 };
			RTC::RtpCodecMimeType mimeType;
			uint32_t clockRate{ 0 };
			std::string rid;
			uint32_t rtxSsrc{ 0 };
			uint8_t rtxPayloadType{ 0 };
			bool useNack{ false };
			bool usePli{ false };
			bool useFir{ false };
		};

	public:
		explicit RtpStream(RTC::RtpStream::Params& params);
		virtual ~RtpStream();

		void FillJson(json& jsonObject) const;
		virtual void FillJsonStats(json& jsonObject);
		uint32_t GetSsrc() const;
		uint8_t GetPayloadType() const;
		const RTC::RtpCodecMimeType& GetMimeType() const;
		uint32_t GetClockRate() const;
		const std::string& GetRid() const;
		bool HasRtx() const;
		virtual void SetRtx(uint8_t payloadType, uint32_t ssrc);
		uint32_t GetRtxSsrc() const;
		uint8_t GetRtxPayloadType() const;
		virtual bool ReceivePacket(RTC::RtpPacket* packet);
		void RestartStatusCheckTimer();
		void StopStatusCheckTimer();
		uint32_t GetRate(uint64_t now);
		float GetLossPercentage() const;
		uint64_t GetMaxPacketMs() const;
		size_t GetExpectedPackets() const;

	protected:
		bool UpdateSeq(RTC::RtpPacket* packet);

	private:
		void InitSeq(uint16_t seq);

		/* Pure virtual methods that must be implemented by the subclass. */
	protected:
		virtual void CheckStatus() = 0;

		/* Pure virtual methods inherited from Timer::Listener. */
	public:
		void OnTimer(Timer* timer) override;

	public:
		uint32_t packetsLost{ 0 };
		uint8_t fractionLost{ 0 };
		size_t packetsDiscarded{ 0 };
		size_t packetsRepaired{ 0 };
		size_t nackCount{ 0 };
		size_t nackRtpPacketCount{ 0 };
		size_t pliCount{ 0 };
		size_t firCount{ 0 };
		RTC::RtpDataCounter transmissionCounter;
		RTC::RtpDataCounter retransmissionCounter;

	protected:
		// Given as argument.
		Params params;
		// Others.
		// Whether at least a RTP packet has been received.
		//   https://tools.ietf.org/html/rfc3550#appendix-A.1 stuff.
		bool started{ false };
		uint16_t maxSeq{ 0 };      // Highest seq. number seen.
		uint32_t cycles{ 0 };      // Shifted count of seq. number cycles.
		uint32_t baseSeq{ 0 };     // Base seq number.
		uint32_t badSeq{ 0 };      // Last 'bad' seq number + 1.
		uint32_t maxPacketTs{ 0 }; // Highest timestamp seen.
		uint64_t maxPacketMs{ 0 }; // When the packet with highest timestammp was seen.
		bool healthy{ true };
		Timer* statusCheckTimer{ nullptr };
	};

	/* Inline instance methods. */

	inline uint32_t RtpStream::GetSsrc() const
	{
		return this->params.ssrc;
	}

	inline uint8_t RtpStream::GetPayloadType() const
	{
		return this->params.payloadType;
	}

	inline const RTC::RtpCodecMimeType& RtpStream::GetMimeType() const
	{
		return this->params.mimeType;
	}

	inline uint32_t RtpStream::GetClockRate() const
	{
		return this->params.clockRate;
	}

	inline const std::string& RtpStream::GetRid() const
	{
		return this->params.rid;
	}

	inline bool RtpStream::HasRtx() const
	{
		return this->params.rtxSsrc != 0;
	}

	inline void RtpStream::SetRtx(uint8_t payloadType, uint32_t ssrc)
	{
		this->params.rtxPayloadType = payloadType;
		this->params.rtxSsrc        = ssrc;
	}

	inline uint32_t RtpStream::GetRtxSsrc() const
	{
		return this->params.rtxSsrc;
	}

	inline uint8_t RtpStream::GetRtxPayloadType() const
	{
		return this->params.rtxPayloadType;
	}

	inline uint32_t RtpStream::GetRate(uint64_t now)
	{
		return this->transmissionCounter.GetRate(now);
	}

	inline float RtpStream::GetLossPercentage() const
	{
		return static_cast<float>(this->fractionLost) * 100 / 256;
	}

	inline uint64_t RtpStream::GetMaxPacketMs() const
	{
		return this->maxPacketMs;
	}

	inline size_t RtpStream::GetExpectedPackets() const
	{
		return (this->cycles + this->maxSeq) - this->baseSeq + 1;
	}
} // namespace RTC

#endif
