#ifndef MS_RTC_RTCP_XR_RECEIVER_REFERENCE_TIME_HPP
#define MS_RTC_RTCP_XR_RECEIVER_REFERENCE_TIME_HPP

#include "common.hpp"
#include "RTC/RTCP/XR.hpp"

/* https://tools.ietf.org/html/rfc3611
 * Receiver Reference Time Report Block

   0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
  |     BT=4      |   reserved    |       block length = 2        |
  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
  |              NTP timestamp, most significant word             |
  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
  |             NTP timestamp, least significant word             |
  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 */

namespace RTC
{
	namespace RTCP
	{
		class ReceiverReferenceTime : public ExtendedReportBlock
		{
		public:
			struct Body
			{
				uint32_t ntpSec;
				uint32_t ntpFrac;
			};

		public:
			static const size_t BodySize{ 8 };
			static ReceiverReferenceTime* Parse(const uint8_t* data, size_t len);

		public:
			// Locally generated Report. Holds the data internally.
			ReceiverReferenceTime() : ExtendedReportBlock(RTCP::ExtendedReportBlock::Type::RRT)
			{
				this->body = reinterpret_cast<Body*>(this->raw);
			}
			// Parsed Report. Points to an external data.
			explicit ReceiverReferenceTime(CommonHeader* header)
			  : ExtendedReportBlock(ExtendedReportBlock::Type::RRT)
			{
				this->header = header;
				this->body   = reinterpret_cast<Body*>((header) + 1);
			}

		public:
			uint32_t GetNtpSec() const
			{
				return uint32_t{ ntohl(this->body->ntpSec) };
			}
			void SetNtpSec(uint32_t ntpSec)
			{
				this->body->ntpSec = uint32_t{ htonl(ntpSec) };
			}
			uint32_t GetNtpFrac() const
			{
				return uint32_t{ ntohl(this->body->ntpFrac) };
			}
			void SetNtpFrac(uint32_t ntpFrac)
			{
				this->body->ntpFrac = uint32_t{ htonl(ntpFrac) };
			}

			/* Pure virtual methods inherited from ExtendedReportBlock. */
		public:
			void Dump() const override;
			size_t Serialize(uint8_t* buffer) override;
			size_t GetSize() const override
			{
				size_t size{ 4 }; // Common header.

				size += BodySize;

				return size;
			}

		private:
			Body* body{ nullptr };
			uint8_t raw[BodySize] = { 0 };
		};
	} // namespace RTCP
} // namespace RTC

#endif
