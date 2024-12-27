#ifndef MS_RTC_RTCP_XR_HPP
#define MS_RTC_RTCP_XR_HPP

#include "common.hpp"
#include "RTC/RTCP/Packet.hpp"
#include <vector>

/* https://tools.ietf.org/html/rfc3611
 * RTP Control Protocol Extended Reports (RTCP XR)

   0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
  |V=2|P|reserved |   PT=XR=207   |             length            |
  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
  |                              SSRC                             |
  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
  :                         report blocks                         :
  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 */

namespace RTC
{
	namespace RTCP
	{
		class ExtendedReportBlock
		{
		public:
			enum class Type : uint8_t
			{
				LRLE = 1,
				DRLE = 2,
				PRT  = 3,
				RRT  = 4,
				DLRR = 5,
				SS   = 6,
				VM   = 7
			};

		public:
			/* Struct for Extended Report Block common header. */
			struct CommonHeader
			{
				uint8_t blockType : 8;
				uint8_t reserved : 8;
				uint16_t length : 16;
			};

		public:
			static const size_t CommonHeaderSize{ 4 };
			static ExtendedReportBlock* Parse(const uint8_t* data, size_t len);

		public:
			ExtendedReportBlock(Type type) : type(type)
			{
				this->header = reinterpret_cast<CommonHeader*>(this->raw);

				this->header->reserved = 0;
			}
			virtual ~ExtendedReportBlock() = default;

		public:
			virtual void Dump() const                 = 0;
			virtual size_t Serialize(uint8_t* buffer) = 0;
			virtual size_t GetSize() const            = 0;
			ExtendedReportBlock::Type GetType() const
			{
				return this->type;
			}

		protected:
			Type type;
			CommonHeader* header{ nullptr };

		private:
			uint8_t raw[CommonHeaderSize] = { 0 };
		};

		class ExtendedReportPacket : public Packet
		{
		public:
			using Iterator = std::vector<ExtendedReportBlock*>::iterator;

		public:
			static ExtendedReportPacket* Parse(const uint8_t* data, size_t len);

		public:
			ExtendedReportPacket() : Packet(Type::XR)
			{
			}
			explicit ExtendedReportPacket(CommonHeader* commonHeader) : Packet(commonHeader)
			{
			}
			~ExtendedReportPacket() override
			{
				for (auto* report : this->reports)
				{
					delete report;
				}
			}

			void AddReport(ExtendedReportBlock* report)
			{
				this->reports.push_back(report);
			}
			void RemoveReport(ExtendedReportBlock* report)
			{
				auto it = std::find(this->reports.begin(), this->reports.end(), report);

				if (it != this->reports.end())
				{
					this->reports.erase(it);
				}
			}
			uint32_t GetSsrc() const
			{
				return this->ssrc;
			}
			void SetSsrc(uint32_t ssrc)
			{
				this->ssrc = ssrc;
			}
			Iterator Begin()
			{
				return this->reports.begin();
			}
			Iterator End()
			{
				return this->reports.end();
			}

			/* Pure virtual methods inherited from Packet. */
		public:
			void Dump() const override;
			size_t Serialize(uint8_t* buffer) override;
			size_t GetCount() const override
			{
				return 0;
			}
			size_t GetSize() const override
			{
				size_t size = Packet::CommonHeaderSize + 4u /*ssrc*/;

				for (auto* report : this->reports)
				{
					size += report->GetSize();
				}

				return size;
			}

		private:
			uint32_t ssrc{ 0u };
			std::vector<ExtendedReportBlock*> reports;
		};
	} // namespace RTCP
} // namespace RTC

#endif
