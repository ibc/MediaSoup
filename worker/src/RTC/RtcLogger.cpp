#define MS_CLASS "RTC::RtcLogger"
// #define MS_LOG_DEV_LEVEL 3

#include "RTC/RtcLogger.hpp"
#include "Logger.hpp"

namespace RTC
{
	namespace RtcLogger
	{
		// clang-format off
		absl::flat_hash_map<RtpPacket::DropReason, std::string> RtpPacket::dropReason2String = {
			{ RtpPacket::DropReason::NONE,                                    "None"                               },
			{ RtpPacket::DropReason::PRODUCER_NOT_FOUND,                      "ProducerNotFound"                   },
			{ RtpPacket::DropReason::RECV_RTP_STREAM_NOT_FOUND,               "RecvRtpStreamNotFound"              },
			{ RtpPacket::DropReason::RECV_RTP_STREAM_DISCARDED,               "RecvRtpStreamDiscarded"             },
			{ RtpPacket::DropReason::CONSUMER_INACTIVE,                       "ConsumerInactive"                   },
			{ RtpPacket::DropReason::INVALID_TARGET_LAYER,                    "InvalidTargetLayer"                 },
			{ RtpPacket::DropReason::UNSUPPORTED_PAYLOAD_TYPE,                "UnsupportedPayloadType"             },
			{ RtpPacket::DropReason::NOT_A_KEYFRAME,                          "NotAKeyframe"                       },
			{ RtpPacket::DropReason::EMPTY_PAYLOAD,                           "EmptyPayload"                       },
			{ RtpPacket::DropReason::SPATIAL_LAYER_MISMATCH,                  "SpatialLayerMismatch"               },
			{ RtpPacket::DropReason::TOO_HIGH_TIMESTAMP_EXTRA_NEEDED,         "TooHighTimestampExtraNeeded"        },
			{ RtpPacket::DropReason::PACKET_PREVIOUS_TO_SPATIAL_LAYER_SWITCH, "PacketPreviousToSpatialLayerSwitch" },
			{ RtpPacket::DropReason::DROPPED_BY_CODEC,                        "DroppedByCodec"                     },
			{ RtpPacket::DropReason::SEND_RTP_STREAM_DISCARDED,               "SendRtpStreamDiscarded"             },
		};
		// clang-format on

		void RtpPacket::Sent()
		{
			MS_TRACE();

			this->dropped = false;

			Log();
			Clear();
		}

		void RtpPacket::Dropped(DropReason dropReason)
		{
			MS_TRACE();

			this->dropped    = true;
			this->dropReason = dropReason;

			Log();
			Clear();
		}

		void RtpPacket::Log() const
		{
			MS_TRACE();

			std::cout << "{";
			std::cout << "\"timestamp\": " << this->timestamp;

			if (!this->recvTransportId.empty())
			{
				std::cout << R"(, "recvTransportId": ")" << this->recvTransportId << "\"";
			}
			if (!this->sendTransportId.empty())
			{
				std::cout << R"(, "sendTransportId": ")" << this->sendTransportId << "\"";
			}
			if (!this->routerId.empty())
			{
				std::cout << R"(, "routerId": ")" << this->routerId << "\"";
			}
			if (!this->producerId.empty())
			{
				std::cout << R"(, "producerId": ")" << this->producerId << "\"";
			}
			if (!this->consumerId.empty())
			{
				std::cout << R"(, "consumerId": ")" << this->consumerId << "\"";
			}

			std::cout << ", \"recvRtpTimestamp\": " << this->recvRtpTimestamp;
			std::cout << ", \"sendRtpTimestamp\": " << this->sendRtpTimestamp;
			std::cout << ", \"recvSeqNumber\": " << this->recvSeqNumber;
			std::cout << ", \"sendSeqNumber\": " << this->sendSeqNumber;
			std::cout << ", \"dropped\": " << (this->dropped ? "true" : "false");
			std::cout << ", \"dropReason\": '" << dropReason2String[this->dropReason] << "'";
			std::cout << "}" << std::endl;
		}

		void RtpPacket::Clear()
		{
			MS_TRACE();

			this->sendTransportId = {};
			this->routerId        = {};
			this->producerId      = {};
			this->sendSeqNumber   = { 0 };
			this->dropped         = { false };
			this->dropReason      = { DropReason::NONE };
		}
	} // namespace RtcLogger
} // namespace RTC
