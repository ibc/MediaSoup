#define MS_CLASS "RTC::RTCP::CompoundPacket"
// #define MS_LOG_DEV

#include "RTC/RTCP/CompoundPacket.h"
#include "Logger.h"

namespace RTC { namespace RTCP
{
	/* Instance methods. */

	CompoundPacket::~CompoundPacket()
	{
		if (this->raw)
			delete this->raw;
	}

	void CompoundPacket::Serialize()
	{
		MS_TRACE();

		// Calculate the total required size for the entire message.
		if (this->senderReportPacket.GetCount())
		{
			this->size = this->senderReportPacket.GetSize();

			if (this->receiverReportPacket.GetCount())
				this->size += sizeof(ReceiverReport::Header) * this->receiverReportPacket.GetCount();
		}
		// If no sender nor receiver reports are present send an empty Receiver Report
		// packet as the head of the compound packet.
		else
		{
			this->size = this->receiverReportPacket.GetSize();
		}

		if (this->sdesPacket.GetCount())
			this->size += this->sdesPacket.GetSize();

		// Allocate it.
		this->raw = new uint8_t[this->size];

		// Fill it.
		size_t offset = 0;

		if (this->senderReportPacket.GetCount())
		{
			this->senderReportPacket.Serialize(this->raw);
			offset = this->senderReportPacket.GetSize();

			if (this->receiverReportPacket.GetCount())
			{
				// Fix header length.
				Packet::CommonHeader* header = (Packet::CommonHeader*) this->raw;
				header->length += (sizeof(ReceiverReport::Header) * this->receiverReportPacket.GetCount()) / 4;

				ReceiverReportPacket::Iterator it = this->receiverReportPacket.Begin();
				for (; it != this->receiverReportPacket.End(); it++)
				{
					ReceiverReport* report = (*it);

					report->Serialize(this->raw + offset);
					offset += sizeof(ReceiverReport::Header);
				}
			}
		}
		else
		{
			this->receiverReportPacket.Serialize(this->raw);
			offset = this->receiverReportPacket.GetSize();
		}

		if (this->sdesPacket.GetCount())
		{
			this->sdesPacket.Serialize(this->raw + offset);
			offset += this->sdesPacket.GetSize();
		}
	}

	void CompoundPacket::Dump()
	{
		#ifdef MS_LOG_DEV

		MS_TRACE();

		if (this->senderReportPacket.GetCount())
		{
			this->senderReportPacket.Dump();

			if (this->receiverReportPacket.GetCount())
			{
				ReceiverReportPacket::Iterator it = this->receiverReportPacket.Begin();

				for (; it != this->receiverReportPacket.End(); it++)
				{
					(*it)->Dump();
				}
			}
		}
		else
		{
			this->receiverReportPacket.Dump();
		}

		if (this->sdesPacket.GetCount())
			this->sdesPacket.Dump();

		#endif
	}

	void CompoundPacket::AddSenderReport(SenderReport* report)
	{
		MS_ASSERT(this->senderReportPacket.GetCount() == 0, "a sender report is already present");

		this->senderReportPacket.AddReport(report);
	}
}}
