#ifndef MS_RTC_RTCP_FEEDBACK_H
#define MS_RTC_RTCP_FEEDBACK_H

#include "common.h"
#include "RTC/RTCP/Packet.h"
#include "RTC/RTCP/FeedbackItem.h"

namespace RTC { namespace RTCP
{
	template <typename T> class FeedbackPacket
		: public Packet
	{
	public:
		/* Struct for RTP Feedback message. */
		struct Header
		{
			uint32_t s_ssrc;
			uint32_t m_ssrc;
		};

	public:
		static RTCP::Type RtcpType;
		static FeedbackPacket<T>* Parse(const uint8_t* data, size_t len);
		static const std::string& MessageType2String(typename T::MessageType type);

	private:
		static std::map<typename T::MessageType, std::string> type2String;

	public:
		typename T::MessageType GetMessageType();
		uint32_t GetSenderSsrc();
		void SetSenderSsrc(uint32_t ssrc);
		uint32_t GetMediaSsrc();
		void SetMediaSsrc(uint32_t ssrc);

	/* Pure virtual methods inherited from Packet. */
	public:
		virtual void Dump() override;
		virtual size_t Serialize(uint8_t* buffer) override;
		virtual size_t GetCount() override;
		virtual size_t GetSize() override;

	protected:
		explicit FeedbackPacket(CommonHeader* commonHeader);
		FeedbackPacket(typename T::MessageType type, uint32_t sender_ssrc, uint32_t media_ssrc);
		virtual ~FeedbackPacket();

	private:
		Header* header = nullptr;
		uint8_t* raw = nullptr;
		typename T::MessageType messageType;
	};

	class FeedbackPs
	{
	public:
		typedef enum MessageType : uint8_t
		{
			PLI  = 1,
			SLI  = 2,
			RPSI = 3,
			FIR  = 4,
			TSTR = 5,
			TSTN = 6,
			VBCM = 7,
			PSLEI = 8,
			ROI  = 9,
			AFB  = 15,
			EXT  = 31
		} MessageType;
	};

	class FeedbackRtp
	{
	public:
		typedef enum MessageType : uint8_t
		{
			NACK   = 1,
			TMMBR  = 3,
			TMMBN  = 4,
			SR_REQ = 5,
			RAMS   = 6,
			TLLEI  = 7,
			ECN    = 8,
			PS     = 9,
			GCC    = 15,
			EXT    = 31
		} MessageType;
	};

	typedef FeedbackPacket<FeedbackPs> FeedbackPsPacket;
	typedef FeedbackPacket<FeedbackRtp> FeedbackRtpPacket;

	/* Inline instance methods. */

	template <typename T>
	typename T::MessageType FeedbackPacket<T>::GetMessageType()
	{
		return this->messageType;
	}

	template <typename T>
	size_t FeedbackPacket<T>::GetCount()
	{
		return (size_t)this->GetMessageType();
	}

	template <typename T>
	size_t FeedbackPacket<T>::GetSize()
	{
		return sizeof(CommonHeader) + sizeof(Header);
	}

	template <typename T>
	uint32_t FeedbackPacket<T>::GetSenderSsrc()
	{
		return (uint32_t)ntohl(this->header->s_ssrc);
	}

	template <typename T>
	void FeedbackPacket<T>::SetSenderSsrc(uint32_t ssrc)
	{
		this->header->s_ssrc = (uint32_t)htonl(ssrc);
	}

	template <typename T>
	uint32_t FeedbackPacket<T>::GetMediaSsrc()
	{
		return (uint32_t)ntohl(this->header->m_ssrc);
	}

	template <typename T>
	void FeedbackPacket<T>::SetMediaSsrc(uint32_t ssrc)
	{
		this->header->m_ssrc = (uint32_t)htonl(ssrc);
	}
}}

#endif
