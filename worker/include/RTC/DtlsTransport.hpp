#ifndef MS_RTC_DTLS_TRANSPORT_HPP
#define MS_RTC_DTLS_TRANSPORT_HPP

#include "common.hpp"
#include "FBS/webRtcTransport.h"
#include "RTC/SrtpSession.hpp"
#include "handles/TimerHandle.hpp"
#include <openssl/bio.h>
#include <openssl/ssl.h>
#include <openssl/x509.h>
#include <absl/container/flat_hash_map.h>
#include <string>
#include <vector>

namespace RTC
{
	class DtlsTransport : public TimerHandle::Listener
	{
	public:
		enum class DtlsState
		{
			NEW = 1,
			CONNECTING,
			CONNECTED,
			FAILED,
			CLOSED
		};

	public:
		enum class Role
		{
			AUTO = 1,
			CLIENT,
			SERVER
		};

	public:
		enum class FingerprintAlgorithm
		{
			SHA1 = 1,
			SHA224,
			SHA256,
			SHA384,
			SHA512
		};

	public:
		struct Fingerprint
		{
			FingerprintAlgorithm algorithm;
			std::string value;
		};

	private:
		struct SrtpCryptoSuiteMapEntry
		{
			RTC::SrtpSession::CryptoSuite cryptoSuite;
			const char* name;
		};

	public:
		class Listener
		{
		public:
			virtual ~Listener() = default;

		public:
			// DTLS is in the process of negotiating a secure connection. Incoming
			// media can flow through.
			// NOTE: The caller MUST NOT call any method during this callback.
			virtual void OnDtlsTransportConnecting(const RTC::DtlsTransport* dtlsTransport) = 0;
			// DTLS has completed negotiation of a secure connection (including DTLS-SRTP
			// and remote fingerprint verification). Outgoing media can now flow through.
			// NOTE: The caller MUST NOT call any method during this callback.
			virtual void OnDtlsTransportConnected(
			  const RTC::DtlsTransport* dtlsTransport,
			  RTC::SrtpSession::CryptoSuite srtpCryptoSuite,
			  uint8_t* srtpLocalKey,
			  size_t srtpLocalKeyLen,
			  uint8_t* srtpRemoteKey,
			  size_t srtpRemoteKeyLen,
			  std::string& remoteCert) = 0;
			// The DTLS connection has been closed as the result of an error (such as a
			// DTLS alert or a failure to validate the remote fingerprint).
			virtual void OnDtlsTransportFailed(const RTC::DtlsTransport* dtlsTransport) = 0;
			// The DTLS connection has been closed due to receipt of a close_notify alert.
			virtual void OnDtlsTransportClosed(const RTC::DtlsTransport* dtlsTransport) = 0;
			// Need to send DTLS data to the peer.
			virtual void OnDtlsTransportSendData(
			  const RTC::DtlsTransport* dtlsTransport, const uint8_t* data, size_t len) = 0;
			// DTLS application data received.
			virtual void OnDtlsTransportApplicationDataReceived(
			  const RTC::DtlsTransport* dtlsTransport, const uint8_t* data, size_t len) = 0;
		};

	public:
		static void ClassInit();
		static void ClassDestroy();
		static Role RoleFromFbs(FBS::WebRtcTransport::DtlsRole role);
		static FBS::WebRtcTransport::DtlsRole RoleToFbs(Role role);
		static FBS::WebRtcTransport::DtlsState StateToFbs(DtlsState state);
		static FingerprintAlgorithm AlgorithmFromFbs(FBS::WebRtcTransport::FingerprintAlgorithm algorithm);
		static FBS::WebRtcTransport::FingerprintAlgorithm AlgorithmToFbs(FingerprintAlgorithm algorithm);
		static bool IsDtls(const uint8_t* data, size_t len)
		{
			// clang-format off
			return (
				// Minimum DTLS record length is 13 bytes.
				(len >= 13) &&
				// DOC: https://tools.ietf.org/html/draft-ietf-avtcore-rfc5764-mux-fixes
				(data[0] > 19 && data[0] < 64)
			);
			// clang-format on
		}
		static std::vector<Fingerprint>& GetLocalFingerprints()
		{
			return DtlsTransport::localFingerprints;
		}

	private:
		static void GenerateCertificateAndPrivateKey();
		static void ReadCertificateAndPrivateKeyFromFiles();
		static void CreateSslCtx();
		static void GenerateFingerprints();

	private:
		thread_local static X509* certificate;
		thread_local static EVP_PKEY* privateKey;
		thread_local static SSL_CTX* sslCtx;
		thread_local static uint8_t sslReadBuffer[];
		static absl::flat_hash_map<std::string, Role> string2Role;
		static absl::flat_hash_map<std::string, FingerprintAlgorithm> string2FingerprintAlgorithm;
		static absl::flat_hash_map<FingerprintAlgorithm, std::string> fingerprintAlgorithm2String;
		thread_local static std::vector<Fingerprint> localFingerprints;
		static std::vector<SrtpCryptoSuiteMapEntry> srtpCryptoSuites;

	public:
		explicit DtlsTransport(Listener* listener);
		~DtlsTransport() override;

	public:
		void Dump() const;
		void Run(Role localRole);
		bool SetRemoteFingerprint(const Fingerprint& fingerprint);
		void ProcessDtlsData(const uint8_t* data, size_t len);
		DtlsState GetState() const
		{
			return this->state;
		}
		std::optional<Role> GetLocalRole() const
		{
			return this->localRole;
		}
		void SendApplicationData(const uint8_t* data, size_t len);
		// This method must be public since it's called within an OpenSSL callback.
		void SendDtlsData(const uint8_t* data, size_t len);

	private:
		bool IsRunning() const
		{
			switch (this->state)
			{
				case DtlsState::NEW:
					return false;
				case DtlsState::CONNECTING:
				case DtlsState::CONNECTED:
					return true;
				case DtlsState::FAILED:
				case DtlsState::CLOSED:
					return false;
			}

			// Make GCC 4.9 happy.
			return false;
		}
		void Reset();
		bool CheckStatus(int returnCode);
		bool SetTimeout();
		bool ProcessHandshake();
		bool CheckRemoteFingerprint();
		void ExtractSrtpKeys(RTC::SrtpSession::CryptoSuite srtpCryptoSuite);
		std::optional<RTC::SrtpSession::CryptoSuite> GetNegotiatedSrtpCryptoSuite();

		/* Callbacks fired by OpenSSL events. */
	public:
		void OnSslInfo(int where, int ret);

		/* Pure virtual methods inherited from TimerHandle::Listener. */
	public:
		void OnTimer(TimerHandle* timer) override;

	private:
		// Passed by argument.
		Listener* listener{ nullptr };
		// Allocated by this.
		SSL* ssl{ nullptr };
		BIO* sslBioFromNetwork{ nullptr }; // The BIO from which ssl reads.
		BIO* sslBioToNetwork{ nullptr };   // The BIO in which ssl writes.
		TimerHandle* timer{ nullptr };
		// Others.
		DtlsState state{ DtlsState::NEW };
		std::optional<Role> localRole;
		std::optional<Fingerprint> remoteFingerprint;
		bool handshakeDone{ false };
		bool handshakeDoneNow{ false };
		std::string remoteCert;
	};
} // namespace RTC

#endif
