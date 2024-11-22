#ifndef MS_RTC_SCTP_DICTIONARIES_HPP
#define MS_RTC_SCTP_DICTIONARIES_HPP

#include "common.hpp"
#include <FBS/sctpParameters.h>

namespace RTC
{
	class SctpStreamParameters
	{
	public:
		SctpStreamParameters() = default;
		explicit SctpStreamParameters(const FBS::SctpParameters::SctpStreamParameters* data);

		flatbuffers::Offset<FBS::SctpParameters::SctpStreamParameters> FillBuffer(
		  flatbuffers::FlatBufferBuilder& builder) const;

	public:
		uint16_t streamId{ 0u };
		bool ordered{ true };
		uint16_t maxPacketLifeTime{ 0u };
		uint16_t maxRetransmits{ 0u };
	};
} // namespace RTC

#endif
