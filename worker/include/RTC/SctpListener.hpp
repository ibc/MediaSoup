#ifndef MS_RTC_SCTP_LISTENER_HPP
#define MS_RTC_SCTP_LISTENER_HPP

#include "common.hpp"
#include "RTC/DataProducer.hpp"
#include <unordered_map>

namespace RTC
{
	class SctpListener
	{
	public:
		flatbuffers::Offset<FBS::Transport::SctpListener> FillBuffer(
		  flatbuffers::FlatBufferBuilder& builder) const;
		void AddDataProducer(RTC::DataProducer* dataProducer);
		void RemoveDataProducer(RTC::DataProducer* dataProducer);
		RTC::DataProducer* GetDataProducer(uint16_t streamId);

	public:
		// Table of streamId / DataProducer pairs.
		std::unordered_map<uint16_t, RTC::DataProducer*> streamIdTable;
	};
} // namespace RTC

#endif
