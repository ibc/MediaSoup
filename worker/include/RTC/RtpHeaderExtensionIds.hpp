#ifndef MS_RTC_RTP_HEADER_EXTENSION_IDS_HPP
#define MS_RTC_RTP_HEADER_EXTENSION_IDS_HPP

namespace RTC
{
	// RTP header extension ids. Some of these are shared by all Producers using
	// the same Transport. Others are different for each Producer.
	struct RtpHeaderExtensionIds
	{
		// 0 means no id.
		uint8_t mid{ 0u };
		uint8_t rid{ 0u };
		uint8_t rrid{ 0u };
		uint8_t absSendTime{ 0u };
		uint8_t transportWideCc01{ 0u };
		uint8_t frameMarking07{ 0u }; // NOTE: Remove once RFC.
		uint8_t frameMarking{ 0u };
		uint8_t ssrcAudioLevel{ 0u };
		uint8_t videoOrientation{ 0u };
		uint8_t toffset{ 0u };
		uint8_t absCaptureTime{ 0u };
		uint8_t playoutDelay{ 0u };
	};
} // namespace RTC

#endif
