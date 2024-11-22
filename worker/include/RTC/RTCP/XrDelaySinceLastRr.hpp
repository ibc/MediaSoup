#ifndef MS_RTC_RTCP_XR_DELAY_SINCE_LAST_RR_HPP
#define MS_RTC_RTCP_XR_DELAY_SINCE_LAST_RR_HPP

#include "common.hpp"
#include "RTC/RTCP/XR.hpp"

/* https://tools.ietf.org/html/rfc3611
 * Delay Since Last Receiver Report (DLRR) Report Block

  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
  |     BT=5      |   reserved    |         block length          |
  +=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+
  |                 SSRC_1 (SSRC of first receiver)               | sub-
  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+ block
  |                         last RR (LRR)                         |   1
  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
  |                   delay since last RR (DLRR)                  |
  +=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+
  |                 SSRC_2 (SSRC of second receiver)              | sub-
  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+ block
  :                               ...                             :   2
  +=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+
 */

namespace RTC
{
	namespace RTCP
	{
		class DelaySinceLastRr : public ExtendedReportBlock
		{
		public:
			static DelaySinceLastRr* Parse(const uint8_t* data, size_t len);

		public:
			class SsrcInfo
			{
			public:
				static const size_t BodySize{ 12 };
				static SsrcInfo* Parse(const uint8_t* data, size_t len);

			public:
				struct Body
				{
					uint32_t ssrc;
					uint32_t lrr;
					uint32_t dlrr;
				};

			public:
				// Locally generated Report. Holds the data internally.
				SsrcInfo()
				{
					this->body = reinterpret_cast<Body*>(this->raw);
				}
				// Parsed Report. Points to an external data.
				explicit SsrcInfo(Body* body) : body(body)
				{
				}

				void Dump() const;
				size_t Serialize(uint8_t* buffer);
				size_t GetSize() const
				{
					return BodySize;
				}
				uint32_t GetSsrc() const
				{
					return uint32_t{ ntohl(this->body->ssrc) };
				}
				void SetSsrc(uint32_t ssrc)
				{
					this->body->ssrc = uint32_t{ htonl(ssrc) };
				}
				uint32_t GetLastReceiverReport() const
				{
					return uint32_t{ ntohl(this->body->lrr) };
				}
				void SetLastReceiverReport(uint32_t lrr)
				{
					this->body->lrr = uint32_t{ htonl(lrr) };
				}
				uint32_t GetDelaySinceLastReceiverReport() const
				{
					return uint32_t{ ntohl(this->body->dlrr) };
				}
				void SetDelaySinceLastReceiverReport(uint32_t dlrr)
				{
					this->body->dlrr = uint32_t{ htonl(dlrr) };
				}

			private:
				Body* body{ nullptr };
				uint8_t raw[BodySize] = { 0 };
			};

		public:
			using Iterator = std::vector<SsrcInfo*>::iterator;

		public:
			DelaySinceLastRr() : ExtendedReportBlock(ExtendedReportBlock::Type::DLRR)
			{
			}
			explicit DelaySinceLastRr(CommonHeader* header)
			  : ExtendedReportBlock(ExtendedReportBlock::Type::DLRR)
			{
				this->header = header;
			}
			~DelaySinceLastRr()
			{
				for (auto* ssrcInfo : this->ssrcInfos)
				{
					delete ssrcInfo;
				}
			}

		public:
			void AddSsrcInfo(SsrcInfo* ssrcInfo)
			{
				this->ssrcInfos.push_back(ssrcInfo);
			}
			// NOTE: This method not only removes given number of ssrc info sub-blocks
			// but also deletes their SsrcInfo instances.
			void RemoveLastSsrcInfos(size_t number)
			{
				while (!this->ssrcInfos.empty() && number-- > 0)
				{
					auto* ssrcInfo = this->ssrcInfos.back();

					this->ssrcInfos.pop_back();

					delete ssrcInfo;
				}
			}
			Iterator Begin()
			{
				return this->ssrcInfos.begin();
			}
			Iterator End()
			{
				return this->ssrcInfos.end();
			}

			/* Pure virtual methods inherited from ExtendedReportBlock. */
		public:
			void Dump() const override;
			size_t Serialize(uint8_t* buffer) override;
			size_t GetSize() const override
			{
				size_t size{ 4u }; // Common header.

				for (auto* ssrcInfo : this->ssrcInfos)
				{
					size += ssrcInfo->GetSize();
				}

				return size;
			}

		private:
			std::vector<SsrcInfo*> ssrcInfos;
		};
	} // namespace RTCP
} // namespace RTC

#endif
