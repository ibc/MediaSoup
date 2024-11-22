#define MS_CLASS "RTC::RTCP::Feedback"
// #define MS_LOG_DEV_LEVEL 3

#include "RTC/RTCP/Feedback.hpp"
#include "Logger.hpp"
#include "RTC/RTCP/FeedbackPsAfb.hpp"
#include "RTC/RTCP/FeedbackPsFir.hpp"
#include "RTC/RTCP/FeedbackPsLei.hpp"
#include "RTC/RTCP/FeedbackPsPli.hpp"
#include "RTC/RTCP/FeedbackPsRpsi.hpp"
#include "RTC/RTCP/FeedbackPsSli.hpp"
#include "RTC/RTCP/FeedbackPsTst.hpp"
#include "RTC/RTCP/FeedbackPsVbcm.hpp"
#include "RTC/RTCP/FeedbackRtpEcn.hpp"
#include "RTC/RTCP/FeedbackRtpNack.hpp"
#include "RTC/RTCP/FeedbackRtpSrReq.hpp"
#include "RTC/RTCP/FeedbackRtpTllei.hpp"
#include "RTC/RTCP/FeedbackRtpTmmb.hpp"
#include "RTC/RTCP/FeedbackRtpTransport.hpp"
#include <cstring>

namespace RTC
{
	namespace RTCP
	{
		/* Class methods. */

		template<typename T>
		const std::string& FeedbackPacket<T>::MessageType2String(typename T::MessageType type)
		{
			static const std::string Unknown("UNKNOWN");

			auto it = FeedbackPacket<T>::type2String.find(type);

			if (it == FeedbackPacket<T>::type2String.end())
			{
				return Unknown;
			}

			return it->second;
		}

		/* Instance methods. */

		template<typename T>
		FeedbackPacket<T>::FeedbackPacket(CommonHeader* commonHeader)
		  : Packet(commonHeader), messageType(typename T::MessageType(commonHeader->count))
		{
			this->header = reinterpret_cast<Header*>(
			  reinterpret_cast<uint8_t*>(commonHeader) + Packet::CommonHeaderSize);
		}

		template<typename T>
		FeedbackPacket<T>::FeedbackPacket(
		  typename T::MessageType messageType, uint32_t senderSsrc, uint32_t mediaSsrc)
		  : Packet(rtcpType), messageType(messageType)
		{
			this->raw                = new uint8_t[HeaderSize];
			this->header             = reinterpret_cast<Header*>(this->raw);
			this->header->senderSsrc = uint32_t{ htonl(senderSsrc) };
			this->header->mediaSsrc  = uint32_t{ htonl(mediaSsrc) };
		}

		template<typename T>
		FeedbackPacket<T>::~FeedbackPacket()
		{
			delete[] this->raw;
		}

		/* Instance methods. */

		template<typename T>
		size_t FeedbackPacket<T>::Serialize(uint8_t* buffer)
		{
			MS_TRACE();

			size_t offset = Packet::Serialize(buffer);

			// Copy the header.
			std::memcpy(buffer + offset, this->header, HeaderSize);

			return offset + HeaderSize;
		}

		template<typename T>
		void FeedbackPacket<T>::Dump() const
		{
			MS_TRACE();

			MS_DUMP("  sender ssrc: %" PRIu32, GetSenderSsrc());
			MS_DUMP("  media ssrc: %" PRIu32, GetMediaSsrc());
			MS_DUMP("  size: %zu", this->GetSize());
		}

		/* Specialization for Ps class. */

		template<>
		RTCP::Type FeedbackPacket<FeedbackPs>::rtcpType = RTCP::Type::PSFB;

		// clang-format off
		template<>
		absl::flat_hash_map<FeedbackPs::MessageType, std::string> FeedbackPacket<FeedbackPs>::type2String =
		{
			{ FeedbackPs::MessageType::PLI,   "PLI"   },
			{ FeedbackPs::MessageType::SLI,   "SLI"   },
			{ FeedbackPs::MessageType::RPSI,  "RPSI"  },
			{ FeedbackPs::MessageType::FIR,   "FIR"   },
			{ FeedbackPs::MessageType::TSTR,  "TSTR"  },
			{ FeedbackPs::MessageType::TSTN,  "TSTN"  },
			{ FeedbackPs::MessageType::VBCM,  "VBCM"  },
			{ FeedbackPs::MessageType::PSLEI, "PSLEI" },
			{ FeedbackPs::MessageType::ROI,   "ROI"   },
			{ FeedbackPs::MessageType::AFB,   "AFB"   },
			{ FeedbackPs::MessageType::EXT,   "EXT"   }
		};
		// clang-format on

		template<>
		FeedbackPacket<FeedbackPs>* FeedbackPacket<FeedbackPs>::Parse(const uint8_t* data, size_t len)
		{
			MS_TRACE();

			if (len < Packet::CommonHeaderSize + FeedbackPacket::HeaderSize)
			{
				MS_WARN_TAG(rtcp, "not enough space for Feedback packet, discarded");

				return nullptr;
			}

			auto* commonHeader = const_cast<CommonHeader*>(reinterpret_cast<const CommonHeader*>(data));
			FeedbackPsPacket* packet{ nullptr };

			switch (FeedbackPs::MessageType(commonHeader->count))
			{
				case FeedbackPs::MessageType::PLI:
					packet = FeedbackPsPliPacket::Parse(data, len);
					break;

				case FeedbackPs::MessageType::SLI:
					packet = FeedbackPsSliPacket::Parse(data, len);
					break;

				case FeedbackPs::MessageType::RPSI:
					packet = FeedbackPsRpsiPacket::Parse(data, len);
					break;

				case FeedbackPs::MessageType::FIR:
					packet = FeedbackPsFirPacket::Parse(data, len);
					break;

				case FeedbackPs::MessageType::TSTR:
					packet = FeedbackPsTstrPacket::Parse(data, len);
					break;

				case FeedbackPs::MessageType::TSTN:
					packet = FeedbackPsTstnPacket::Parse(data, len);
					break;

				case FeedbackPs::MessageType::VBCM:
					packet = FeedbackPsVbcmPacket::Parse(data, len);
					break;

				case FeedbackPs::MessageType::PSLEI:
					packet = FeedbackPsLeiPacket::Parse(data, len);
					break;

				case FeedbackPs::MessageType::ROI:
					break;

				case FeedbackPs::MessageType::AFB:
					packet = FeedbackPsAfbPacket::Parse(data, len);
					break;

				case FeedbackPs::MessageType::EXT:
					break;

				default:
					MS_WARN_TAG(
					  rtcp, "unknown RTCP PS Feedback message type [packetType:%" PRIu8 "]", commonHeader->count);
			}

			return packet;
		}

		/* Specialization for Rtcp class. */

		template<>
		Type FeedbackPacket<FeedbackRtp>::rtcpType = RTCP::Type::RTPFB;

		// clang-format off
		template<>
		absl::flat_hash_map<FeedbackRtp::MessageType, std::string> FeedbackPacket<FeedbackRtp>::type2String =
		{
			{ FeedbackRtp::MessageType::NACK,   "NACK"   },
			{ FeedbackRtp::MessageType::TMMBR,  "TMMBR"  },
			{ FeedbackRtp::MessageType::TMMBN,  "TMMBN"  },
			{ FeedbackRtp::MessageType::SR_REQ, "SR_REQ" },
			{ FeedbackRtp::MessageType::RAMS,   "RAMS"   },
			{ FeedbackRtp::MessageType::TLLEI,  "TLLEI"  },
			{ FeedbackRtp::MessageType::ECN,    "ECN"    },
			{ FeedbackRtp::MessageType::PS,     "PS"     },
			{ FeedbackRtp::MessageType::EXT,    "EXT"    },
			{ FeedbackRtp::MessageType::TCC,    "TCC"    }
		};
		// clang-format on

		/* Class methods. */

		template<>
		FeedbackPacket<FeedbackRtp>* FeedbackPacket<FeedbackRtp>::Parse(const uint8_t* data, size_t len)
		{
			MS_TRACE();

			if (len < Packet::CommonHeaderSize + FeedbackPacket::HeaderSize)
			{
				MS_WARN_TAG(rtcp, "not enough space for Feedback packet, discarded");

				return nullptr;
			}

			auto* commonHeader = reinterpret_cast<CommonHeader*>(const_cast<uint8_t*>(data));
			FeedbackRtpPacket* packet{ nullptr };

			switch (FeedbackRtp::MessageType(commonHeader->count))
			{
				case FeedbackRtp::MessageType::NACK:
					packet = FeedbackRtpNackPacket::Parse(data, len);
					break;

				case FeedbackRtp::MessageType::TMMBR:
					packet = FeedbackRtpTmmbrPacket::Parse(data, len);
					break;

				case FeedbackRtp::MessageType::TMMBN:
					packet = FeedbackRtpTmmbnPacket::Parse(data, len);
					break;

				case FeedbackRtp::MessageType::SR_REQ:
					packet = FeedbackRtpSrReqPacket::Parse(data, len);
					break;

				case FeedbackRtp::MessageType::RAMS:
					break;

				case FeedbackRtp::MessageType::TLLEI:
					packet = FeedbackRtpTlleiPacket::Parse(data, len);
					break;

				case FeedbackRtp::MessageType::ECN:
					packet = FeedbackRtpEcnPacket::Parse(data, len);
					break;

				case FeedbackRtp::MessageType::PS:
					break;

				case FeedbackRtp::MessageType::EXT:
					break;

				case FeedbackRtp::MessageType::TCC:
					packet = FeedbackRtpTransportPacket::Parse(data, len);
					break;

				default:
					MS_WARN_TAG(
					  rtcp,
					  "unknown RTCP RTP Feedback message type [packetType:%" PRIu8 "]",
					  commonHeader->count);
			}

			return packet;
		}

		// Explicit instantiation to have all FeedbackPacket definitions in this file.
		template class FeedbackPacket<FeedbackPs>;
		template class FeedbackPacket<FeedbackRtp>;
	} // namespace RTCP
} // namespace RTC
