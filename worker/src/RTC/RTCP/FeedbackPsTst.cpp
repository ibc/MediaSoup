#define MS_CLASS "RTC::RTCP::FeedbackPsTst"
// #define MS_LOG_DEV_LEVEL 3

#include "RTC/RTCP/FeedbackPsTst.hpp"
#include "Logger.hpp"
#include <cstring> // std::memcpy

namespace RTC
{
	namespace RTCP
	{
		template<typename T>
		FeedbackPsTstItem<T>::FeedbackPsTstItem(uint32_t ssrc, uint8_t sequenceNumber, uint8_t index)
		{
			MS_TRACE();

			this->raw    = new uint8_t[HeaderSize];
			this->header = reinterpret_cast<Header*>(this->raw);

			// Set reserved bits to zero.
			std::memset(this->header, 0, HeaderSize);

			this->header->ssrc           = uint32_t{ htonl(ssrc) };
			this->header->sequenceNumber = sequenceNumber;
			this->header->index          = index;
		}

		template<typename T>
		size_t FeedbackPsTstItem<T>::Serialize(uint8_t* buffer)
		{
			MS_TRACE();

			std::memcpy(buffer, this->header, HeaderSize);

			return HeaderSize;
		}

		template<typename T>
		void FeedbackPsTstItem<T>::Dump() const
		{
			MS_TRACE();

			MS_DUMP("<FeedbackPsTstItem>");
			MS_DUMP("  ssrc: %" PRIu32, this->GetSsrc());
			MS_DUMP("  sequence number: %" PRIu32, this->GetSequenceNumber());
			MS_DUMP("  index: %" PRIu32, this->GetIndex());
			MS_DUMP("</FeedbackPsTstItem>");
		}

		/* Specialization for Tstr class. */

		template<>
		const FeedbackPs::MessageType FeedbackPsTstItem<Tstr>::messageType =
		  FeedbackPs::MessageType::TSTR;

		/* Specialization for Tstn class. */

		template<>
		const FeedbackPs::MessageType FeedbackPsTstItem<Tstn>::messageType =
		  FeedbackPs::MessageType::TSTN;

		// Explicit instantiation to have all definitions in this file.
		template class FeedbackPsTstItem<Tstr>;
		template class FeedbackPsTstItem<Tstn>;
	} // namespace RTCP
} // namespace RTC
