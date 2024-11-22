#include "RTC/FuzzerRtpPacket.hpp"
#include "RTC/RtpPacket.hpp"
#include <cstring> // std::memory()
#include <string>
#include <vector>

void Fuzzer::RTC::RtpPacket::Fuzz(const uint8_t* data, size_t len)
{
	if (!::RTC::RtpPacket::IsRtp(data, len))
	{
		return;
	}

	// We need to clone the given data into a separate buffer because setters
	// below will try to write into packet memory.
	//
	// NOTE: Let's make the buffer bigger to test API that increases packet size.
	uint8_t data2[len + 64];
	uint8_t extenLen;
	bool voice;
	uint8_t volume;
	bool camera;
	bool flip;
	uint16_t rotation;
	uint32_t absSendTime;
	uint16_t playoutDelayMinDelay;
	uint16_t playoutDelayMaxDelay;
	uint16_t wideSeqNumber;
	std::string mid;
	std::string rid;
	std::vector<::RTC::RtpPacket::GenericExtension> extensions;

	std::memcpy(data2, data, len);

	::RTC::RtpPacket* packet = ::RTC::RtpPacket::Parse(data2, len);

	if (!packet)
	{
		return;
	}

	// packet->Dump();
	packet->GetData();
	packet->GetSize();
	packet->GetPayloadType();
	packet->SetPayloadType(100);
	packet->HasMarker();
	packet->SetMarker(true);
	packet->SetPayloadPaddingFlag(true);
	packet->GetSequenceNumber();
	packet->SetSequenceNumber(12345);
	packet->GetTimestamp();
	packet->SetTimestamp(8888);
	packet->GetSsrc();
	packet->SetSsrc(666);
	packet->HasHeaderExtension();
	packet->GetHeaderExtensionId();
	packet->GetHeaderExtensionLength();
	packet->GetHeaderExtensionValue();
	packet->HasOneByteExtensions();
	packet->HasTwoBytesExtensions();

	packet->SetMidExtensionId(5);
	packet->HasExtension(5);
	packet->GetExtension(5, extenLen);
	packet->ReadMid(mid);
	packet->UpdateMid(mid);

	packet->SetRidExtensionId(6);
	packet->HasExtension(6);
	packet->GetExtension(6, extenLen);
	packet->ReadRid(rid);

	packet->SetAbsSendTimeExtensionId(3);
	packet->HasExtension(3);
	packet->GetExtension(3, extenLen);
	packet->ReadAbsSendTime(absSendTime);
	packet->UpdateAbsSendTime(12345678u);

	packet->SetTransportWideCc01ExtensionId(4);
	packet->HasExtension(4);
	packet->GetExtension(4, extenLen);
	packet->ReadTransportWideCc01(wideSeqNumber);
	packet->UpdateTransportWideCc01(12345u);

	packet->SetSsrcAudioLevelExtensionId(1);
	packet->HasExtension(1);
	packet->GetExtension(1, extenLen);
	packet->ReadSsrcAudioLevel(volume, voice);

	packet->SetVideoOrientationExtensionId(2);
	packet->HasExtension(2);
	packet->GetExtension(2, extenLen);
	packet->ReadVideoOrientation(camera, flip, rotation);

	packet->SetPlayoutDelayExtensionId(8);
	packet->HasExtension(8);
	packet->GetExtension(8, extenLen);
	packet->ReadPlayoutDelay(playoutDelayMinDelay, playoutDelayMaxDelay);

	packet->HasExtension(6);
	packet->HasExtension(7);
	packet->HasExtension(8);
	packet->HasExtension(9);
	packet->HasExtension(10);
	packet->HasExtension(11);
	packet->HasExtension(12);
	packet->HasExtension(13);
	packet->HasExtension(14);
	packet->HasExtension(15);

	uint8_t value1[] = { 0x01, 0x02, 0x03, 0x04 };

	extensions.emplace_back(
	  1,     // id
	  4,     // len
	  value1 // value
	);

	uint8_t value2[] = { 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x10, 0x11 };

	extensions.emplace_back(
	  2,     // id
	  11,    // len
	  value2 // value
	);

	packet->SetExtensions(1, extensions);
	packet->SetExtensions(2, extensions);

	extensions.clear();

	packet->SetExtensions(2, extensions);
	packet->SetExtensions(1, extensions);

	uint8_t value3[] = { 0x01, 0x02, 0x03 };

	extensions.emplace_back(
	  14,    // id
	  3,     // len
	  value3 // value
	);

	extensions.emplace_back(
	  15,    // id
	  3,     // len
	  value3 // value
	);

	extensions.emplace_back(
	  22,    // id
	  3,     // len
	  value3 // value
	);

	extensions.emplace_back(
	  0,     // id
	  3,     // len
	  value3 // value
	);

	packet->SetExtensions(2, extensions);
	packet->SetExtensions(1, extensions);

	packet->SetAbsSendTimeExtensionId(13);
	packet->HasExtension(13);
	packet->GetExtension(13, extenLen);
	packet->ReadAbsSendTime(absSendTime);
	packet->UpdateAbsSendTime(12345678u);

	packet->SetTransportWideCc01ExtensionId(14);
	packet->HasExtension(14);
	packet->GetExtension(14, extenLen);
	packet->ReadTransportWideCc01(wideSeqNumber);
	packet->UpdateTransportWideCc01(12345u);
	packet->SetExtensionLength(14, 2);

	packet->SetSsrcAudioLevelExtensionId(11);
	packet->HasExtension(11);
	packet->GetExtension(11, extenLen);
	packet->ReadSsrcAudioLevel(volume, voice);

	packet->SetVideoOrientationExtensionId(12);
	packet->HasExtension(12);
	packet->GetExtension(12, extenLen);
	packet->ReadVideoOrientation(camera, flip, rotation);

	packet->SetPlayoutDelayExtensionId(15);
	packet->HasExtension(15);
	packet->GetExtension(15, extenLen);
	packet->ReadPlayoutDelay(playoutDelayMinDelay, playoutDelayMaxDelay);

	packet->GetPayload();
	packet->GetPayloadLength();
	packet->GetPayloadPadding();
	packet->IsKeyFrame();

	auto* clonedPacket = packet->Clone();

	delete clonedPacket;

	// TODO: packet->RtxEncode(); // This cannot be tested this way.
	// TODO: packet->RtxDecode(); // This cannot be tested this way.
	// TODO: packet->SetPayloadDescriptorHandler();
	// TODO: packet->ProcessPayload();
	// TODO: packet->ProcessPayload();
	// TODO: packet->ShiftPayload();

	delete packet;
}
