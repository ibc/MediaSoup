#include "RTC/FuzzerStunPacket.hpp"
#include "RTC/StunPacket.hpp"

static constexpr size_t StunSerializeBufferSize{ 65536 };
thread_local static uint8_t StunSerializeBuffer[StunSerializeBufferSize];

void Fuzzer::RTC::StunPacket::Fuzz(const uint8_t* data, size_t len)
{
	if (!::RTC::StunPacket::IsStun(data, len))
	{
		return;
	}

	::RTC::StunPacket* packet = ::RTC::StunPacket::Parse(data, len);

	if (!packet)
	{
		return;
	}

	// packet->Dump();
	packet->GetClass();
	packet->GetMethod();
	packet->GetData();
	packet->GetSize();
	packet->SetUsername("foo", 3);
	packet->SetPassword("lalala");
	packet->SetPriority(123);
	packet->SetIceControlling(123);
	packet->SetIceControlled(123);
	packet->SetUseCandidate();
	// TODO: packet->SetXorMappedAddress();
	packet->SetErrorCode(666);
	// TODO: packet->SetMessageIntegrity();
	packet->SetFingerprint();
	packet->GetUsername();
	packet->GetPriority();
	packet->GetIceControlling();
	packet->GetIceControlled();
	packet->HasUseCandidate();
	packet->GetErrorCode();
	packet->HasMessageIntegrity();
	packet->HasFingerprint();
	packet->CheckAuthentication("foo", "xxx");

	if (packet->GetClass() == ::RTC::StunPacket::Class::REQUEST)
	{
		auto* successResponse = packet->CreateSuccessResponse();
		auto* errorResponse   = packet->CreateErrorResponse(444);

		delete successResponse;
		delete errorResponse;
	}

	if (len < StunSerializeBufferSize - 1000)
	{
		packet->Serialize(StunSerializeBuffer);
	}

	delete packet;
}
