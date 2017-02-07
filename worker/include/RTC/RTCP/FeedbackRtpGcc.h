#ifndef MS_RTC_RTCP_FEEDBACK_GCC_H
#define MS_RTC_RTCP_FEEDBACK_GCC_H

#include "common.h"
#include "RTC/RTCP/Feedback.h"

namespace RTC { namespace RTCP
{
	class FeedbackRtpGccPacket
		: public FeedbackRtpPacket
	{
	public:
		static FeedbackRtpGccPacket* Parse(const uint8_t* data, size_t len);

	public:
		// Parsed Report. Points to an external data.
		explicit FeedbackRtpGccPacket(CommonHeader* commonHeader);
		FeedbackRtpGccPacket(uint32_t sender_ssrc, uint32_t media_ssrc);
		virtual ~FeedbackRtpGccPacket() {};

		uint8_t* GetData();

	/* Pure virtual methods inherited from Packet. */
	public:
		virtual void Dump() override;
		virtual size_t Serialize(uint8_t* buffer) override;
		virtual size_t GetSize() override;

	private:
		uint8_t* data = nullptr;
		size_t size = 0;
	};

	/* Inline instance methods. */

	inline
	FeedbackRtpGccPacket::FeedbackRtpGccPacket(CommonHeader* commonHeader):
		FeedbackRtpPacket(commonHeader)
	{
		this->size = ((ntohs(commonHeader->length) + 1) * 4) - (sizeof(CommonHeader) + sizeof(FeedbackPacket::Header));
		this->data = (uint8_t*)commonHeader + sizeof(CommonHeader) + sizeof(FeedbackPacket::Header);
	}

	inline
	FeedbackRtpGccPacket::FeedbackRtpGccPacket(uint32_t sender_ssrc, uint32_t media_ssrc):
		FeedbackRtpPacket(FeedbackRtp::GCC, sender_ssrc, media_ssrc)
	{}

	inline
	uint8_t* FeedbackRtpGccPacket::GetData()
	{
		return this->data;
	}

	inline
	size_t FeedbackRtpGccPacket::GetSize()
	{
		return FeedbackRtpPacket::GetSize() + this->size;
	}
}}

#endif
