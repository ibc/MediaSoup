#define MS_CLASS "RTC::IceCandidate"
// #define MS_LOG_DEV_LEVEL 3

#include "RTC/IceCandidate.hpp"
#include "Logger.hpp"

namespace RTC
{
	/* Class methods. */

	IceCandidate::CandidateType IceCandidate::CandidateTypeFromFbs(
	  FBS::WebRtcTransport::IceCandidateType type)
	{
		switch (type)
		{
			case FBS::WebRtcTransport::IceCandidateType::HOST:
				return IceCandidate::CandidateType::HOST;
		}
	}

	FBS::WebRtcTransport::IceCandidateType IceCandidate::CandidateTypeToFbs(IceCandidate::CandidateType type)
	{
		switch (type)
		{
			case IceCandidate::CandidateType::HOST:
				return FBS::WebRtcTransport::IceCandidateType::HOST;
		}
	}

	IceCandidate::TcpCandidateType IceCandidate::TcpCandidateTypeFromFbs(
	  FBS::WebRtcTransport::IceCandidateTcpType type)
	{
		switch (type)
		{
			case FBS::WebRtcTransport::IceCandidateTcpType::PASSIVE:
				return IceCandidate::TcpCandidateType::PASSIVE;
		}
	}

	FBS::WebRtcTransport::IceCandidateTcpType IceCandidate::TcpCandidateTypeToFbs(
	  IceCandidate::TcpCandidateType type)
	{
		switch (type)
		{
			case IceCandidate::TcpCandidateType::PASSIVE:
				return FBS::WebRtcTransport::IceCandidateTcpType::PASSIVE;
		}
	}

	/* Instance methods. */

	flatbuffers::Offset<FBS::WebRtcTransport::IceCandidate> IceCandidate::FillBuffer(
	  flatbuffers::FlatBufferBuilder& builder) const
	{
		MS_TRACE();

		auto protocol = TransportTuple::ProtocolToFbs(this->protocol);
		auto type     = CandidateTypeToFbs(this->type);
		flatbuffers::Optional<FBS::WebRtcTransport::IceCandidateTcpType> tcpType;

		if (this->protocol == Protocol::TCP)
		{
			tcpType.emplace(TcpCandidateTypeToFbs(this->tcpType));
		}

		return FBS::WebRtcTransport::CreateIceCandidateDirect(
		  builder,
		  // foundation.
		  this->foundation.c_str(),
		  // priority.
		  this->priority,
		  // address.
		  this->address.c_str(),
		  // protocol.
		  protocol,
		  // port.
		  this->port,
		  // type.
		  type,
		  // tcpType.
		  tcpType);
	}
} // namespace RTC
