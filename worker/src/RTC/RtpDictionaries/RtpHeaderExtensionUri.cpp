#define MS_CLASS "RTC::RtpHeaderExtensionUri"
// #define MS_LOG_DEV_LEVEL 3

#include "Logger.hpp"
#include "RTC/RtpDictionaries.hpp"

namespace RTC
{
	/* Class methods. */

	RtpHeaderExtensionUri::Type RtpHeaderExtensionUri::TypeFromFbs(
	  FBS::RtpParameters::RtpHeaderExtensionUri uri)
	{
		switch (uri)
		{
			case FBS::RtpParameters::RtpHeaderExtensionUri::Mid:
			{
				return RtpHeaderExtensionUri::Type::MID;
			}

			case FBS::RtpParameters::RtpHeaderExtensionUri::RtpStreamId:
			{
				return RtpHeaderExtensionUri::Type::RTP_STREAM_ID;
			}

			case FBS::RtpParameters::RtpHeaderExtensionUri::RepairRtpStreamId:
			{
				return RtpHeaderExtensionUri::Type::REPAIRED_RTP_STREAM_ID;
			}

			case FBS::RtpParameters::RtpHeaderExtensionUri::FrameMarkingDraft07:
			{
				return RtpHeaderExtensionUri::Type::FRAME_MARKING_07;
			}

			case FBS::RtpParameters::RtpHeaderExtensionUri::FrameMarking:
			{
				return RtpHeaderExtensionUri::Type::FRAME_MARKING;
			}

			case FBS::RtpParameters::RtpHeaderExtensionUri::AudioLevel:
			{
				return RtpHeaderExtensionUri::Type::SSRC_AUDIO_LEVEL;
			}

			case FBS::RtpParameters::RtpHeaderExtensionUri::VideoOrientation:
			{
				return RtpHeaderExtensionUri::Type::VIDEO_ORIENTATION;
			}

			case FBS::RtpParameters::RtpHeaderExtensionUri::PlayoutDelay:
			{
				return RtpHeaderExtensionUri::Type::PLAYOUT_DELAY;
			}

			case FBS::RtpParameters::RtpHeaderExtensionUri::TimeOffset:
			{
				return RtpHeaderExtensionUri::Type::TOFFSET;
			}

			case FBS::RtpParameters::RtpHeaderExtensionUri::TransportWideCcDraft01:
			{
				return RtpHeaderExtensionUri::Type::TRANSPORT_WIDE_CC_01;
			}

			case FBS::RtpParameters::RtpHeaderExtensionUri::AbsSendTime:
			{
				return RtpHeaderExtensionUri::Type::ABS_SEND_TIME;
			}

			case FBS::RtpParameters::RtpHeaderExtensionUri::AbsCaptureTime:
			{
				return RtpHeaderExtensionUri::Type::ABS_CAPTURE_TIME;
			}
		}
	}

	FBS::RtpParameters::RtpHeaderExtensionUri RtpHeaderExtensionUri::TypeToFbs(
	  RtpHeaderExtensionUri::Type uri)
	{
		switch (uri)
		{
			case RtpHeaderExtensionUri::Type::MID:
			{
				return FBS::RtpParameters::RtpHeaderExtensionUri::Mid;
			}

			case RtpHeaderExtensionUri::Type::RTP_STREAM_ID:
			{
				return FBS::RtpParameters::RtpHeaderExtensionUri::RtpStreamId;
			}

			case RtpHeaderExtensionUri::Type::REPAIRED_RTP_STREAM_ID:
			{
				return FBS::RtpParameters::RtpHeaderExtensionUri::RepairRtpStreamId;
			}

			case RtpHeaderExtensionUri::Type::ABS_SEND_TIME:
			{
				return FBS::RtpParameters::RtpHeaderExtensionUri::AbsSendTime;
			}

			case RtpHeaderExtensionUri::Type::TRANSPORT_WIDE_CC_01:
			{
				return FBS::RtpParameters::RtpHeaderExtensionUri::TransportWideCcDraft01;
			}

			case RtpHeaderExtensionUri::Type::FRAME_MARKING_07:
			{
				return FBS::RtpParameters::RtpHeaderExtensionUri::FrameMarkingDraft07;
			}

			case RtpHeaderExtensionUri::Type::FRAME_MARKING:
			{
				return FBS::RtpParameters::RtpHeaderExtensionUri::FrameMarking;
			}

			case RtpHeaderExtensionUri::Type::SSRC_AUDIO_LEVEL:
			{
				return FBS::RtpParameters::RtpHeaderExtensionUri::AudioLevel;
			}

			case RtpHeaderExtensionUri::Type::VIDEO_ORIENTATION:
			{
				return FBS::RtpParameters::RtpHeaderExtensionUri::VideoOrientation;
			}

			case RtpHeaderExtensionUri::Type::PLAYOUT_DELAY:
			{
				return FBS::RtpParameters::RtpHeaderExtensionUri::PlayoutDelay;
			}

			case RtpHeaderExtensionUri::Type::TOFFSET:
			{
				return FBS::RtpParameters::RtpHeaderExtensionUri::TimeOffset;
			}

			case RtpHeaderExtensionUri::Type::ABS_CAPTURE_TIME:
			{
				return FBS::RtpParameters::RtpHeaderExtensionUri::AbsCaptureTime;
			}
		}
	}

} // namespace RTC
