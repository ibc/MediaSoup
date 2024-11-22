#ifndef MS_FUZZER_RTC_DTLS_TRANSPORT_HPP
#define MS_FUZZER_RTC_DTLS_TRANSPORT_HPP

#include "common.hpp"
#include "RTC/DtlsTransport.hpp"

namespace Fuzzer
{
	namespace RTC
	{
		namespace DtlsTransport
		{
			class DtlsTransportListener : public ::RTC::DtlsTransport::Listener
			{
				/* Pure virtual methods inherited from RTC::DtlsTransport::Listener. */
			public:
				void OnDtlsTransportConnecting(const ::RTC::DtlsTransport* dtlsTransport) override;
				void OnDtlsTransportConnected(
				  const ::RTC::DtlsTransport* dtlsTransport,
				  ::RTC::SrtpSession::CryptoSuite srtpCryptoSuite,
				  uint8_t* srtpLocalKey,
				  size_t srtpLocalKeyLen,
				  uint8_t* srtpRemoteKey,
				  size_t srtpRemoteKeyLen,
				  std::string& remoteCert) override;
				void OnDtlsTransportFailed(const ::RTC::DtlsTransport* dtlsTransport) override;
				void OnDtlsTransportClosed(const ::RTC::DtlsTransport* dtlsTransport) override;
				void OnDtlsTransportSendData(
				  const ::RTC::DtlsTransport* dtlsTransport, const uint8_t* data, size_t len) override;
				void OnDtlsTransportApplicationDataReceived(
				  const ::RTC::DtlsTransport* dtlsTransport, const uint8_t* data, size_t len) override;
			};

			void Fuzz(const uint8_t* data, size_t len);
		} // namespace DtlsTransport
	}   // namespace RTC
} // namespace Fuzzer

#endif
