#define MS_CLASS "RTC::DtlsTransport"
// #define MS_LOG_DEV_LEVEL 3

#include "RTC/DtlsTransport.hpp"
#include "Logger.hpp"
#include "MediaSoupErrors.hpp"
#include "Settings.hpp"
#include "Utils.hpp"
#include <openssl/err.h>
#include <openssl/evp.h>
#include <uv.h>
#include <cstdio>  // std::snprintf(), std::fopen()
#include <cstring> // std::memcpy(), std::strcmp()

// clang-format off
#define LOG_OPENSSL_ERROR(desc) \
	do \
	{ \
		if (ERR_peek_error() == 0) \
		{ \
			MS_ERROR("OpenSSL error [desc:'%s']", desc); \
		} \
		else \
		{ \
			int64_t err; \
			while ((err = ERR_get_error()) != 0) \
			{ \
				MS_ERROR("OpenSSL error [desc:'%s', error:'%s']", desc, ERR_error_string(err, nullptr)); \
			} \
			ERR_clear_error(); \
		} \
	} while (false)
// clang-format on

/* Static methods for OpenSSL callbacks. */

inline static int onSslCertificateVerify(int /*preverifyOk*/, X509_STORE_CTX* /*ctx*/)
{
	MS_TRACE();

	// Always valid since DTLS certificates are self-signed.
	return 1;
}

inline static void onSslInfo(const SSL* ssl, int where, int ret)
{
	static_cast<RTC::DtlsTransport*>(SSL_get_ex_data(ssl, 0))->OnSslInfo(where, ret);
}

/**
 * This callback is called by OpenSSL when it wants to send DTLS data to the
 * endpoint. Such a data could be a full DTLS message, various DTLS messages,
 * a DTLS message fragment, various DTLS message fragments or a combination of
 * these. It's guaranteed (by observation) that |len| argument corresponds to
 * the entire content of our BIO mem buffer |this->sslBioToNetwork| and it
 * never exceeds our |DtlsMtu| limit.
 */
inline static long onSslBioOut(
  BIO* bio,
  int operationType,
  const char* argp,
  size_t len,
  int /*argi*/,
  long /*argl*/,
  int ret,
  size_t* /*processed*/)
{
	long resultOfcallback = (operationType == BIO_CB_RETURN) ? static_cast<long>(ret) : 1;

	// This callback is called twice for write operations:
	// - First one with operationType = BIO_CB_WRITE.
	// - Second one with operationType = BIO_CB_RETURN | BIO_CB_WRITE.
	// We only care about the former.
	if ((operationType == BIO_CB_WRITE) && argp && len > 0)
	{
		auto* dtlsTransport = reinterpret_cast<RTC::DtlsTransport*>(BIO_get_callback_arg(bio));

		dtlsTransport->SendDtlsData(reinterpret_cast<const uint8_t*>(argp), len);
	}

	return resultOfcallback;
}

inline static unsigned int onSslDtlsTimer(SSL* /*ssl*/, unsigned int timerUs)
{
	if (timerUs == 0u)
	{
		return 100000u;
	}
	else if (timerUs >= 4000000u)
	{
		return 4000000u;
	}
	else
	{
		return 2 * timerUs;
	}
}

namespace RTC
{
	/* Static. */

	// clang-format off
	static constexpr int DtlsMtu{ 1350 };
	static constexpr int SslReadBufferSize{ 65536 };
	// AES-HMAC: http://tools.ietf.org/html/rfc3711
	static constexpr size_t SrtpMasterKeyLength{ 16u };
	static constexpr size_t SrtpMasterSaltLength{ 14u };
	static constexpr size_t SrtpMasterLength{ SrtpMasterKeyLength + SrtpMasterSaltLength };
	// AES-GCM: http://tools.ietf.org/html/rfc7714
	static constexpr size_t SrtpAesGcm256MasterKeyLength{ 32u };
	static constexpr size_t SrtpAesGcm256MasterSaltLength{ 12u };
	static constexpr size_t SrtpAesGcm256MasterLength{ SrtpAesGcm256MasterKeyLength + SrtpAesGcm256MasterSaltLength };
	static constexpr size_t SrtpAesGcm128MasterKeyLength{ 16u };
	static constexpr size_t SrtpAesGcm128MasterSaltLength{ 12u };
	static constexpr size_t SrtpAesGcm128MasterLength{ SrtpAesGcm128MasterKeyLength + SrtpAesGcm128MasterSaltLength };
	// clang-format on

	/* Class variables. */

	thread_local X509* DtlsTransport::certificate{ nullptr };
	thread_local EVP_PKEY* DtlsTransport::privateKey{ nullptr };
	thread_local SSL_CTX* DtlsTransport::sslCtx{ nullptr };
	thread_local uint8_t DtlsTransport::sslReadBuffer[SslReadBufferSize];
	// clang-format off
	absl::flat_hash_map<std::string, DtlsTransport::FingerprintAlgorithm> DtlsTransport::string2FingerprintAlgorithm =
	{
		{ "sha-1",   DtlsTransport::FingerprintAlgorithm::SHA1   },
		{ "sha-224", DtlsTransport::FingerprintAlgorithm::SHA224 },
		{ "sha-256", DtlsTransport::FingerprintAlgorithm::SHA256 },
		{ "sha-384", DtlsTransport::FingerprintAlgorithm::SHA384 },
		{ "sha-512", DtlsTransport::FingerprintAlgorithm::SHA512 }
	};
	absl::flat_hash_map<DtlsTransport::FingerprintAlgorithm, std::string> DtlsTransport::fingerprintAlgorithm2String =
	{
		{ DtlsTransport::FingerprintAlgorithm::SHA1,   "sha-1"   },
		{ DtlsTransport::FingerprintAlgorithm::SHA224, "sha-224" },
		{ DtlsTransport::FingerprintAlgorithm::SHA256, "sha-256" },
		{ DtlsTransport::FingerprintAlgorithm::SHA384, "sha-384" },
		{ DtlsTransport::FingerprintAlgorithm::SHA512, "sha-512" }
	};
	absl::flat_hash_map<std::string, DtlsTransport::Role> DtlsTransport::string2Role =
	{
		{ "auto",   DtlsTransport::Role::AUTO   },
		{ "client", DtlsTransport::Role::CLIENT },
		{ "server", DtlsTransport::Role::SERVER }
	};
	thread_local std::vector<DtlsTransport::Fingerprint> DtlsTransport::localFingerprints;
	std::vector<DtlsTransport::SrtpCryptoSuiteMapEntry> DtlsTransport::srtpCryptoSuites =
	{
		{ RTC::SrtpSession::CryptoSuite::AEAD_AES_256_GCM,        "SRTP_AEAD_AES_256_GCM"  },
		{ RTC::SrtpSession::CryptoSuite::AEAD_AES_128_GCM,        "SRTP_AEAD_AES_128_GCM"  },
		{ RTC::SrtpSession::CryptoSuite::AES_CM_128_HMAC_SHA1_80, "SRTP_AES128_CM_SHA1_80" },
		{ RTC::SrtpSession::CryptoSuite::AES_CM_128_HMAC_SHA1_32, "SRTP_AES128_CM_SHA1_32" }
	};
	// clang-format on

	/* Class methods. */

	void DtlsTransport::ClassInit()
	{
		MS_TRACE();

		// Generate a X509 certificate and private key (unless PEM files are provided).
		if (
		  Settings::configuration.dtlsCertificateFile.empty() ||
		  Settings::configuration.dtlsPrivateKeyFile.empty())
		{
			GenerateCertificateAndPrivateKey();
		}
		else
		{
			ReadCertificateAndPrivateKeyFromFiles();
		}

		// Create a global SSL_CTX.
		CreateSslCtx();

		// Generate certificate fingerprints.
		GenerateFingerprints();
	}

	void DtlsTransport::ClassDestroy()
	{
		MS_TRACE();

		if (DtlsTransport::privateKey)
		{
			EVP_PKEY_free(DtlsTransport::privateKey);
		}

		if (DtlsTransport::certificate)
		{
			X509_free(DtlsTransport::certificate);
		}

		if (DtlsTransport::sslCtx)
		{
			SSL_CTX_free(DtlsTransport::sslCtx);
		}
	}

	DtlsTransport::Role DtlsTransport::RoleFromFbs(FBS::WebRtcTransport::DtlsRole role)
	{
		switch (role)
		{
			case FBS::WebRtcTransport::DtlsRole::AUTO:
			{
				return DtlsTransport::Role::AUTO;
			}

			case FBS::WebRtcTransport::DtlsRole::CLIENT:
			{
				return DtlsTransport::Role::CLIENT;
			}

			case FBS::WebRtcTransport::DtlsRole::SERVER:
			{
				return DtlsTransport::Role::SERVER;
			}
		}
	}

	FBS::WebRtcTransport::DtlsRole DtlsTransport::RoleToFbs(DtlsTransport::Role role)
	{
		switch (role)
		{
			case DtlsTransport::Role::AUTO:
			{
				return FBS::WebRtcTransport::DtlsRole::AUTO;
			}

			case DtlsTransport::Role::CLIENT:
			{
				return FBS::WebRtcTransport::DtlsRole::CLIENT;
			}

			case DtlsTransport::Role::SERVER:
			{
				return FBS::WebRtcTransport::DtlsRole::SERVER;
			}
		}
	}

	FBS::WebRtcTransport::DtlsState DtlsTransport::StateToFbs(DtlsTransport::DtlsState state)
	{
		switch (state)
		{
			case DtlsTransport::DtlsState::NEW:
			{
				return FBS::WebRtcTransport::DtlsState::NEW;
			}

			case DtlsTransport::DtlsState::CONNECTING:
			{
				return FBS::WebRtcTransport::DtlsState::CONNECTING;
			}

			case DtlsTransport::DtlsState::CONNECTED:
			{
				return FBS::WebRtcTransport::DtlsState::CONNECTED;
			}

			case DtlsTransport::DtlsState::FAILED:
			{
				return FBS::WebRtcTransport::DtlsState::FAILED;
			}

			case DtlsTransport::DtlsState::CLOSED:
			{
				return FBS::WebRtcTransport::DtlsState::CLOSED;
			}
		}
	}

	DtlsTransport::FingerprintAlgorithm DtlsTransport::AlgorithmFromFbs(
	  FBS::WebRtcTransport::FingerprintAlgorithm algorithm)
	{
		switch (algorithm)
		{
			case FBS::WebRtcTransport::FingerprintAlgorithm::SHA1:
			{
				return DtlsTransport::FingerprintAlgorithm::SHA1;
			}

			case FBS::WebRtcTransport::FingerprintAlgorithm::SHA224:
			{
				return DtlsTransport::FingerprintAlgorithm::SHA224;
			}

			case FBS::WebRtcTransport::FingerprintAlgorithm::SHA256:
			{
				return DtlsTransport::FingerprintAlgorithm::SHA256;
			}

			case FBS::WebRtcTransport::FingerprintAlgorithm::SHA384:
			{
				return DtlsTransport::FingerprintAlgorithm::SHA384;
			}

			case FBS::WebRtcTransport::FingerprintAlgorithm::SHA512:
			{
				return DtlsTransport::FingerprintAlgorithm::SHA512;
			}
		}
	}

	FBS::WebRtcTransport::FingerprintAlgorithm DtlsTransport::AlgorithmToFbs(
	  DtlsTransport::FingerprintAlgorithm algorithm)
	{
		switch (algorithm)
		{
			case DtlsTransport::FingerprintAlgorithm::SHA1:
			{
				return FBS::WebRtcTransport::FingerprintAlgorithm::SHA1;
			}

			case DtlsTransport::FingerprintAlgorithm::SHA224:
			{
				return FBS::WebRtcTransport::FingerprintAlgorithm::SHA224;
			}

			case DtlsTransport::FingerprintAlgorithm::SHA256:
			{
				return FBS::WebRtcTransport::FingerprintAlgorithm::SHA256;
			}

			case DtlsTransport::FingerprintAlgorithm::SHA384:
			{
				return FBS::WebRtcTransport::FingerprintAlgorithm::SHA384;
			}

			case DtlsTransport::FingerprintAlgorithm::SHA512:
			{
				return FBS::WebRtcTransport::FingerprintAlgorithm::SHA512;
			}
		}
	}

	void DtlsTransport::GenerateCertificateAndPrivateKey()
	{
		MS_TRACE();

		int ret{ 0 };
		X509_NAME* certName{ nullptr };
		const std::string subject =
		  std::string("mediasoup") + std::to_string(Utils::Crypto::GetRandomUInt(100000, 999999));

		// Create key with curve.
		DtlsTransport::privateKey = EVP_EC_gen(SN_X9_62_prime256v1);

		if (!DtlsTransport::privateKey)
		{
			LOG_OPENSSL_ERROR("EVP_EC_gen() failed");

			goto error;
		}

		// Create the X509 certificate.
		DtlsTransport::certificate = X509_new();

		if (!DtlsTransport::certificate)
		{
			LOG_OPENSSL_ERROR("X509_new() failed");

			goto error;
		}

		// Set version 3 (note that 0 means version 1).
		X509_set_version(DtlsTransport::certificate, 2);

		// Set serial number (avoid default 0).
		ASN1_INTEGER_set(
		  X509_get_serialNumber(DtlsTransport::certificate),
		  static_cast<uint64_t>(Utils::Crypto::GetRandomUInt(1000000, 9999999)));

		// Set valid period.
		X509_gmtime_adj(X509_get_notBefore(DtlsTransport::certificate), -315360000); // -10 years.
		X509_gmtime_adj(X509_get_notAfter(DtlsTransport::certificate), 315360000);   // 10 years.

		// Set the public key for the certificate using the key.
		ret = X509_set_pubkey(DtlsTransport::certificate, DtlsTransport::privateKey);

		if (ret == 0)
		{
			LOG_OPENSSL_ERROR("X509_set_pubkey() failed");

			goto error;
		}

		// Set certificate fields.
		certName = X509_get_subject_name(DtlsTransport::certificate);

		if (!certName)
		{
			LOG_OPENSSL_ERROR("X509_get_subject_name() failed");

			goto error;
		}

		X509_NAME_add_entry_by_txt(
		  certName, "O", MBSTRING_ASC, reinterpret_cast<const uint8_t*>(subject.c_str()), -1, -1, 0);
		X509_NAME_add_entry_by_txt(
		  certName, "CN", MBSTRING_ASC, reinterpret_cast<const uint8_t*>(subject.c_str()), -1, -1, 0);

		// It is self-signed so set the issuer name to be the same as the subject.
		ret = X509_set_issuer_name(DtlsTransport::certificate, certName);

		if (ret == 0)
		{
			LOG_OPENSSL_ERROR("X509_set_issuer_name() failed");

			goto error;
		}

		// Sign the certificate with its own private key.
		ret = X509_sign(DtlsTransport::certificate, DtlsTransport::privateKey, EVP_sha256());

		if (ret == 0)
		{
			LOG_OPENSSL_ERROR("X509_sign() failed");

			goto error;
		}

		return;

	error:

		if (DtlsTransport::privateKey)
		{
			EVP_PKEY_free(DtlsTransport::privateKey);
		}

		if (DtlsTransport::certificate)
		{
			X509_free(DtlsTransport::certificate);
		}

		MS_THROW_ERROR("DTLS certificate and private key generation failed");
	}

	void DtlsTransport::ReadCertificateAndPrivateKeyFromFiles()
	{
		MS_TRACE();

		FILE* file{ nullptr };

		file = fopen(Settings::configuration.dtlsCertificateFile.c_str(), "r");

		if (!file)
		{
			MS_ERROR("error reading DTLS certificate file: %s", std::strerror(errno));

			goto error;
		}

		DtlsTransport::certificate = PEM_read_X509(file, nullptr, nullptr, nullptr);

		if (!DtlsTransport::certificate)
		{
			LOG_OPENSSL_ERROR("PEM_read_X509() failed");

			goto error;
		}

		fclose(file);

		file = fopen(Settings::configuration.dtlsPrivateKeyFile.c_str(), "r");

		if (!file)
		{
			MS_ERROR("error reading DTLS private key file: %s", std::strerror(errno));

			goto error;
		}

		DtlsTransport::privateKey = PEM_read_PrivateKey(file, nullptr, nullptr, nullptr);

		if (!DtlsTransport::privateKey)
		{
			LOG_OPENSSL_ERROR("PEM_read_PrivateKey() failed");

			goto error;
		}

		fclose(file);

		return;

	error:

		MS_THROW_ERROR("error reading DTLS certificate and private key PEM files");
	}

	void DtlsTransport::CreateSslCtx()
	{
		MS_TRACE();

		std::string dtlsSrtpCryptoSuites;
		int ret;

		/* Set the global DTLS context. */

		// Both DTLS 1.0 and 1.2 (requires OpenSSL >= 1.1.0).
		DtlsTransport::sslCtx = SSL_CTX_new(DTLS_method());

		if (!DtlsTransport::sslCtx)
		{
			LOG_OPENSSL_ERROR("SSL_CTX_new() failed");

			goto error;
		}

		ret = SSL_CTX_use_certificate(DtlsTransport::sslCtx, DtlsTransport::certificate);

		if (ret == 0)
		{
			LOG_OPENSSL_ERROR("SSL_CTX_use_certificate() failed");

			goto error;
		}

		ret = SSL_CTX_use_PrivateKey(DtlsTransport::sslCtx, DtlsTransport::privateKey);

		if (ret == 0)
		{
			LOG_OPENSSL_ERROR("SSL_CTX_use_PrivateKey() failed");

			goto error;
		}

		ret = SSL_CTX_check_private_key(DtlsTransport::sslCtx);

		if (ret == 0)
		{
			LOG_OPENSSL_ERROR("SSL_CTX_check_private_key() failed");

			goto error;
		}

		// Set options.
		SSL_CTX_set_options(
		  DtlsTransport::sslCtx,
		  SSL_OP_CIPHER_SERVER_PREFERENCE | SSL_OP_NO_TICKET | SSL_OP_SINGLE_ECDH_USE |
		    SSL_OP_NO_QUERY_MTU);

		// Don't use sessions cache.
		SSL_CTX_set_session_cache_mode(DtlsTransport::sslCtx, SSL_SESS_CACHE_OFF);

		SSL_CTX_set_verify_depth(DtlsTransport::sslCtx, 4);

		// Require certificate from peer.
		SSL_CTX_set_verify(
		  DtlsTransport::sslCtx, SSL_VERIFY_PEER | SSL_VERIFY_FAIL_IF_NO_PEER_CERT, onSslCertificateVerify);

		// Set SSL info callback.
		SSL_CTX_set_info_callback(DtlsTransport::sslCtx, onSslInfo);

		// Set ciphers.
		ret = SSL_CTX_set_cipher_list(
		  DtlsTransport::sslCtx, "DEFAULT:!NULL:!aNULL:!SHA256:!SHA384:!aECDH:!AESGCM+AES256:!aPSK");

		if (ret == 0)
		{
			LOG_OPENSSL_ERROR("SSL_CTX_set_cipher_list() failed");

			goto error;
		}

		// Enable ECDH ciphers.
		// DOC: http://en.wikibooks.org/wiki/OpenSSL/Diffie-Hellman_parameters
		// NOTE: https://code.google.com/p/chromium/issues/detail?id=406458
		// NOTE: https://bugs.ruby-lang.org/issues/12324

		// For OpenSSL >= 1.0.2.
		SSL_CTX_set_ecdh_auto(DtlsTransport::sslCtx, 1);

		// Set the "use_srtp" DTLS extension.
		for (auto it = DtlsTransport::srtpCryptoSuites.begin();
		     it != DtlsTransport::srtpCryptoSuites.end();
		     ++it)
		{
			if (it != DtlsTransport::srtpCryptoSuites.begin())
			{
				dtlsSrtpCryptoSuites += ":";
			}

			SrtpCryptoSuiteMapEntry* cryptoSuiteEntry = std::addressof(*it);
			dtlsSrtpCryptoSuites += cryptoSuiteEntry->name;
		}

		MS_DEBUG_2TAGS(dtls, srtp, "setting SRTP cryptoSuites for DTLS: %s", dtlsSrtpCryptoSuites.c_str());

		// NOTE: This function returns 0 on success.
		ret = SSL_CTX_set_tlsext_use_srtp(DtlsTransport::sslCtx, dtlsSrtpCryptoSuites.c_str());

		if (ret != 0)
		{
			MS_ERROR(
			  "SSL_CTX_set_tlsext_use_srtp() failed when entering '%s'", dtlsSrtpCryptoSuites.c_str());
			LOG_OPENSSL_ERROR("SSL_CTX_set_tlsext_use_srtp() failed");

			goto error;
		}

		return;

	error:

		if (DtlsTransport::sslCtx)
		{
			SSL_CTX_free(DtlsTransport::sslCtx);
			DtlsTransport::sslCtx = nullptr;
		}

		MS_THROW_ERROR("SSL context creation failed");
	}

	void DtlsTransport::GenerateFingerprints()
	{
		MS_TRACE();

		for (auto& kv : DtlsTransport::string2FingerprintAlgorithm)
		{
			const std::string& algorithmString   = kv.first;
			const FingerprintAlgorithm algorithm = kv.second;
			uint8_t binaryFingerprint[EVP_MAX_MD_SIZE];
			unsigned int size{ 0 };
			char hexFingerprint[(EVP_MAX_MD_SIZE * 3) + 1];
			const EVP_MD* hashFunction;
			int ret;

			switch (algorithm)
			{
				case FingerprintAlgorithm::SHA1:
				{
					hashFunction = EVP_sha1();
					break;
				}

				case FingerprintAlgorithm::SHA224:
				{
					hashFunction = EVP_sha224();
					break;
				}

				case FingerprintAlgorithm::SHA256:
				{
					hashFunction = EVP_sha256();
					break;
				}

				case FingerprintAlgorithm::SHA384:
				{
					hashFunction = EVP_sha384();
					break;
				}

				case FingerprintAlgorithm::SHA512:
				{
					hashFunction = EVP_sha512();
					break;
				}

				default:
				{
					MS_THROW_ERROR("unknown algorithm");
				}
			}

			ret = X509_digest(DtlsTransport::certificate, hashFunction, binaryFingerprint, &size);

			if (ret == 0)
			{
				MS_ERROR("X509_digest() failed");
				MS_THROW_ERROR("Fingerprints generation failed");
			}

			// Convert to hexadecimal format in uppercase with colons.
			for (unsigned int i{ 0 }; i < size; ++i)
			{
				std::snprintf(hexFingerprint + (i * 3), 4, "%.2X:", binaryFingerprint[i]);
			}
			hexFingerprint[(size * 3) - 1] = '\0';

			MS_DEBUG_TAG(dtls, "%-7s fingerprint: %s", algorithmString.c_str(), hexFingerprint);

			// Store it in the vector.
			DtlsTransport::Fingerprint fingerprint;

			fingerprint.algorithm = algorithm;
			fingerprint.value     = hexFingerprint;

			DtlsTransport::localFingerprints.push_back(fingerprint);
		}
	}

	/* Instance methods. */

	DtlsTransport::DtlsTransport(Listener* listener)
	  : listener(listener), ssl(SSL_new(DtlsTransport::sslCtx))
	{
		MS_TRACE();

		if (!this->ssl)
		{
			LOG_OPENSSL_ERROR("SSL_new() failed");

			goto error;
		}

		// Set this as custom data.
		SSL_set_ex_data(this->ssl, 0, static_cast<void*>(this));

		this->sslBioFromNetwork = BIO_new(BIO_s_mem());

		if (!this->sslBioFromNetwork)
		{
			LOG_OPENSSL_ERROR("BIO_new() failed");

			SSL_free(this->ssl);

			goto error;
		}

		this->sslBioToNetwork = BIO_new(BIO_s_mem());

		if (!this->sslBioToNetwork)
		{
			LOG_OPENSSL_ERROR("BIO_new() failed");

			BIO_free(this->sslBioFromNetwork);
			SSL_free(this->ssl);

			goto error;
		}

		// Set the MTU so that we don't send packets that are too large with no
		// fragmentation.
		SSL_set_mtu(this->ssl, DtlsMtu);
		DTLS_set_link_mtu(this->ssl, DtlsMtu);

		// We want to monitor OpenSSL write operations into our |sslBioToNetwork|
		// buffer so we can immediately send those DTLS bytes (containing full DTLS
		// messages, or valid DTLS fragment messages, or combination of them) to
		// the endpoint, and hence we honor the configured DTLS MTU.
		BIO_set_callback_ex(this->sslBioToNetwork, onSslBioOut);
		BIO_set_callback_arg(this->sslBioToNetwork, reinterpret_cast<char*>(this));
		SSL_set_bio(this->ssl, this->sslBioFromNetwork, this->sslBioToNetwork);

		// Set callback handler for setting DTLS timer interval.
		DTLS_set_timer_cb(this->ssl, onSslDtlsTimer);

		// Set the DTLS timer.
		this->timer = new TimerHandle(this);

		return;

	error:

		// NOTE: At this point SSL_set_bio() was not called so we must free BIOs as
		// well.
		if (this->sslBioFromNetwork)
		{
			BIO_free(this->sslBioFromNetwork);
		}

		if (this->sslBioToNetwork)
		{
			BIO_free(this->sslBioToNetwork);
		}

		if (this->ssl)
		{
			SSL_free(this->ssl);
		}

		// NOTE: If this is not catched by the caller the program will abort, but
		// this should never happen.
		MS_THROW_ERROR("DtlsTransport instance creation failed");
	}

	DtlsTransport::~DtlsTransport()
	{
		MS_TRACE();

		if (IsRunning())
		{
			// Send close alert to the peer.
			SSL_shutdown(this->ssl);
		}

		if (this->ssl)
		{
			SSL_free(this->ssl);

			this->ssl               = nullptr;
			this->sslBioFromNetwork = nullptr;
			this->sslBioToNetwork   = nullptr;
		}

		// Close the DTLS timer.
		delete this->timer;
	}

	void DtlsTransport::Dump() const
	{
		MS_TRACE();

		std::string state{ "new" };
		std::string role{ "none " };

		switch (this->state)
		{
			case DtlsState::CONNECTING:
			{
				state = "connecting";
				break;
			}

			case DtlsState::CONNECTED:
			{
				state = "connected";
				break;
			}

			case DtlsState::FAILED:
			{
				state = "failed";
				break;
			}

			case DtlsState::CLOSED:
			{
				state = "closed";
				break;
			}

			default:;
		}

		if (this->localRole.has_value())
		{
			switch (this->localRole.value())
			{
				case Role::AUTO:
				{
					role = "auto";
					break;
				}

				case Role::SERVER:
				{
					role = "server";
					break;
				}

				case Role::CLIENT:
				{
					role = "client";
					break;
				}
			}
		}

		MS_DUMP("<DtlsTransport>");
		MS_DUMP("  state: %s", state.c_str());
		MS_DUMP("  role: %s", role.c_str());
		MS_DUMP("  handshake done: %s", this->handshakeDone ? "yes" : "no");
		MS_DUMP("</DtlsTransport>");
	}

	void DtlsTransport::Run(Role localRole)
	{
		MS_TRACE();

		MS_ASSERT(
		  localRole == Role::CLIENT || localRole == Role::SERVER,
		  "local DTLS role must be 'client' or 'server'");

		if (this->localRole.has_value() && localRole == this->localRole.value())
		{
			MS_ERROR("same local DTLS role provided, doing nothing");

			return;
		}

		// If the previous local DTLS role was 'client' or 'server' do reset.
		if (
		  this->localRole.has_value() &&
		  (this->localRole.value() == Role::CLIENT || this->localRole.value() == Role::SERVER))
		{
			MS_DEBUG_TAG(dtls, "resetting DTLS due to local role change");

			Reset();
		}

		// Update local role.
		this->localRole = localRole;

		// Set state and notify the listener.
		this->state = DtlsState::CONNECTING;
		this->listener->OnDtlsTransportConnecting(this);

		switch (this->localRole.value())
		{
			case Role::CLIENT:
			{
				MS_DEBUG_TAG(dtls, "running [role:client]");

				SSL_set_connect_state(this->ssl);
				SSL_do_handshake(this->ssl);
				SetTimeout();

				break;
			}

			case Role::SERVER:
			{
				MS_DEBUG_TAG(dtls, "running [role:server]");

				SSL_set_accept_state(this->ssl);
				SSL_do_handshake(this->ssl);

				break;
			}

			default:
			{
				MS_ABORT("invalid local DTLS role");
			}
		}
	}

	bool DtlsTransport::SetRemoteFingerprint(const Fingerprint& fingerprint)
	{
		MS_TRACE();

		this->remoteFingerprint = fingerprint;

		// The remote fingerpring may have been set after DTLS handshake was done,
		// so we may need to process it now.
		if (this->handshakeDone && this->state != DtlsState::CONNECTED)
		{
			MS_DEBUG_TAG(dtls, "handshake already done, processing it right now");

			return ProcessHandshake();
		}

		return true;
	}

	void DtlsTransport::ProcessDtlsData(const uint8_t* data, size_t len)
	{
		MS_TRACE();

		int written;
		int read;

		if (!IsRunning())
		{
			MS_ERROR("cannot process data while not running");

			return;
		}

		// Write the received DTLS data into the sslBioFromNetwork.
		written =
		  BIO_write(this->sslBioFromNetwork, static_cast<const void*>(data), static_cast<int>(len));

		if (written != static_cast<int>(len))
		{
			MS_WARN_TAG(
			  dtls,
			  "OpenSSL BIO_write() wrote less (%zu bytes) than given data (%zu bytes)",
			  static_cast<size_t>(written),
			  len);
		}

		// Must call SSL_read() to process received DTLS data.
		read = SSL_read(this->ssl, static_cast<void*>(DtlsTransport::sslReadBuffer), SslReadBufferSize);

		// Check SSL status and return if it is bad/closed.
		if (!CheckStatus(read))
		{
			return;
		}

		// Set/update the DTLS timeout.
		if (!SetTimeout())
		{
			return;
		}

		// Application data received. Notify to the listener.
		if (read > 0)
		{
			// It is allowed to receive DTLS data even before validating remote
			// fingerprint.
			if (!this->handshakeDone)
			{
				MS_WARN_TAG(dtls, "ignoring application data received while DTLS handshake not done");

				return;
			}

			// Notify the listener.
			this->listener->OnDtlsTransportApplicationDataReceived(
			  this, (uint8_t*)DtlsTransport::sslReadBuffer, static_cast<size_t>(read));
		}
	}

	void DtlsTransport::SendApplicationData(const uint8_t* data, size_t len)
	{
		MS_TRACE();

		// We cannot send data to the peer if its remote fingerprint is not
		// validated.
		if (this->state != DtlsState::CONNECTED)
		{
			MS_WARN_TAG(dtls, "cannot send application data while DTLS is not fully connected");

			return;
		}

		if (len == 0)
		{
			MS_WARN_TAG(dtls, "ignoring 0 length data");

			return;
		}

		int written;

		written = SSL_write(this->ssl, static_cast<const void*>(data), static_cast<int>(len));

		if (written < 0)
		{
			LOG_OPENSSL_ERROR("SSL_write() failed");

			if (!CheckStatus(written))
			{
				return;
			}
		}
		else if (written != static_cast<int>(len))
		{
			MS_WARN_TAG(
			  dtls, "OpenSSL SSL_write() wrote less (%d bytes) than given data (%zu bytes)", written, len);
		}
	}

	/**
	 * This method is called within our |onSslBioOut| callback above. As told
	 * there, it's guaranteed that OpenSSL invokes that callback with all the
	 * bytes currently written in our BIO mem buffer |this->sslBioToNetwork| so
	 * we can safely reset/clear that buffer once we have sent the data to the
	 * endpoint.
	 */
	void DtlsTransport::SendDtlsData(const uint8_t* data, size_t len)
	{
		MS_TRACE();

		MS_DEBUG_DEV("%zu bytes of DTLS data ready to be sent", len);

		// Notify the listener.
		this->listener->OnDtlsTransportSendData(this, data, len);

		// Clear the BIO buffer.
		auto ret = BIO_reset(this->sslBioToNetwork);

		if (ret != 1)
		{
			MS_ERROR("BIO_reset() failed [ret:%d]", ret);
		}
	}

	void DtlsTransport::Reset()
	{
		MS_TRACE();

		int ret;

		if (!IsRunning())
		{
			return;
		}

		MS_WARN_TAG(dtls, "resetting DTLS transport");

		// Stop the DTLS timer.
		this->timer->Stop();

		// NOTE: We need to reset the SSL instance so we need to "shutdown" it, but
		// we don't want to send a DTLS Close Alert to the peer. However this is
		// gonna happen since SSL_shutdown() will trigger a DTLS Close Alert and
		// we'll have our onSslBioOut() callback called to deliver it.
		SSL_shutdown(this->ssl);

		this->localRole.reset();
		this->state            = DtlsState::NEW;
		this->handshakeDone    = false;
		this->handshakeDoneNow = false;

		// Reset SSL status.
		// NOTE: For this to properly work, SSL_shutdown() must be called before.
		// NOTE: This may fail if not enough DTLS handshake data has been received,
		// but we don't care so just clear the error queue.
		ret = SSL_clear(this->ssl);

		if (ret == 0)
		{
			ERR_clear_error();
		}
	}

	bool DtlsTransport::CheckStatus(int returnCode)
	{
		MS_TRACE();

		const bool wasHandshakeDone = this->handshakeDone;

		int err = SSL_get_error(this->ssl, returnCode);

		switch (err)
		{
			case SSL_ERROR_NONE:
			{
				break;
			}

			case SSL_ERROR_SSL:
			{
				LOG_OPENSSL_ERROR("SSL status: SSL_ERROR_SSL");
				break;
			}

			case SSL_ERROR_WANT_READ:
			{
				break;
			}

			case SSL_ERROR_WANT_WRITE:
			{
				MS_WARN_TAG(dtls, "SSL status: SSL_ERROR_WANT_WRITE");
				break;
			}

			case SSL_ERROR_WANT_X509_LOOKUP:
			{
				MS_DEBUG_TAG(dtls, "SSL status: SSL_ERROR_WANT_X509_LOOKUP");
				break;
			}

			case SSL_ERROR_SYSCALL:
			{
				LOG_OPENSSL_ERROR("SSL status: SSL_ERROR_SYSCALL");
				break;
			}

			case SSL_ERROR_ZERO_RETURN:
			{
				break;
			}

			case SSL_ERROR_WANT_CONNECT:
			{
				MS_WARN_TAG(dtls, "SSL status: SSL_ERROR_WANT_CONNECT");
				break;
			}

			case SSL_ERROR_WANT_ACCEPT:
			{
				MS_WARN_TAG(dtls, "SSL status: SSL_ERROR_WANT_ACCEPT");
				break;
			}

			default:
			{
				MS_WARN_TAG(dtls, "SSL status: unknown error");
			}
		}

		// Check if the handshake (or re-handshake) has been done right now.
		if (this->handshakeDoneNow)
		{
			this->handshakeDoneNow = false;
			this->handshakeDone    = true;

			// Stop the timer.
			this->timer->Stop();

			// Process the handshake just once (ignore if DTLS renegotiation).
			if (!wasHandshakeDone && this->remoteFingerprint.has_value())
			{
				return ProcessHandshake();
			}

			return true;
		}
		// Check if the peer sent close alert or a fatal error happened.
		else if (((SSL_get_shutdown(this->ssl) & SSL_RECEIVED_SHUTDOWN) != 0) || err == SSL_ERROR_SSL || err == SSL_ERROR_SYSCALL)
		{
			if (this->state == DtlsState::CONNECTED)
			{
				MS_DEBUG_TAG(dtls, "disconnected");

				Reset();

				// Set state and notify the listener.
				this->state = DtlsState::CLOSED;
				this->listener->OnDtlsTransportClosed(this);
			}
			else
			{
				MS_WARN_TAG(dtls, "connection failed");

				Reset();

				// Set state and notify the listener.
				this->state = DtlsState::FAILED;
				this->listener->OnDtlsTransportFailed(this);
			}

			return false;
		}
		else
		{
			return true;
		}
	}

	bool DtlsTransport::SetTimeout()
	{
		MS_TRACE();

		MS_ASSERT(this->ssl, "this->ssl is not set");
		MS_ASSERT(
		  this->state == DtlsState::CONNECTING || this->state == DtlsState::CONNECTED,
		  "invalid DTLS state");

		uv_timeval_t dtlsTimeout{ 0, 0 };
		uint64_t timeoutMs;

		// DTLSv1_get_timeout queries the next DTLS handshake timeout. If there is
		// a timeout in progress, it sets *out to the time remaining and returns
		// one. Otherwise, it returns zero.
		DTLSv1_get_timeout(this->ssl, static_cast<void*>(&dtlsTimeout)); // NOLINT

		timeoutMs = (dtlsTimeout.tv_sec * static_cast<uint64_t>(1000)) + (dtlsTimeout.tv_usec / 1000);

		if (timeoutMs == 0)
		{
			MS_DEBUG_DEV("timeout is 0, calling OnTimer() callback directly");

			OnTimer(this->timer);

			return true;
		}
		else if (timeoutMs < 30000)
		{
			MS_DEBUG_DEV("DTLS timer set in %" PRIu64 "ms", timeoutMs);

			this->timer->Start(timeoutMs);

			return true;
		}
		// NOTE: Don't start the timer again if the timeout is greater than 30
		// seconds.
		else
		{
			MS_WARN_TAG(dtls, "DTLS timeout too high (%" PRIu64 "ms), resetting DLTS", timeoutMs);

			Reset();

			// Set state and notify the listener.
			this->state = DtlsState::FAILED;
			this->listener->OnDtlsTransportFailed(this);

			return false;
		}
	}

	bool DtlsTransport::ProcessHandshake()
	{
		MS_TRACE();

		MS_ASSERT(this->handshakeDone, "handshake not done yet");

		// Validate the remote fingerprint.
		if (!CheckRemoteFingerprint())
		{
			Reset();

			// Set state and notify the listener.
			this->state = DtlsState::FAILED;
			this->listener->OnDtlsTransportFailed(this);

			return false;
		}

		// Get the negotiated SRTP crypto suite.
		auto srtpCryptoSuite = GetNegotiatedSrtpCryptoSuite();

		if (srtpCryptoSuite)
		{
			// Extract the SRTP keys (will notify the listener with them).
			ExtractSrtpKeys(srtpCryptoSuite.value());

			return true;
		}

		// NOTE: We assume that "use_srtp" DTLS extension is required even if
		// there is no audio/video.
		MS_WARN_2TAGS(dtls, srtp, "SRTP crypto suite not negotiated");

		Reset();

		// Set state and notify the listener.
		this->state = DtlsState::FAILED;
		this->listener->OnDtlsTransportFailed(this);

		return false;
	}

	bool DtlsTransport::CheckRemoteFingerprint()
	{
		MS_TRACE();

		MS_ASSERT(this->remoteFingerprint.has_value(), "remote fingerprint not set");

		X509* certificate;
		uint8_t binaryFingerprint[EVP_MAX_MD_SIZE];
		unsigned int size{ 0 };
		char hexFingerprint[(EVP_MAX_MD_SIZE * 3) + 1];
		const EVP_MD* hashFunction;
		int ret;

		certificate = SSL_get_peer_certificate(this->ssl);

		if (!certificate)
		{
			MS_WARN_TAG(dtls, "no certificate was provided by the peer");

			return false;
		}

		switch (this->remoteFingerprint->algorithm)
		{
			case FingerprintAlgorithm::SHA1:
			{
				hashFunction = EVP_sha1();
				break;
			}

			case FingerprintAlgorithm::SHA224:
			{
				hashFunction = EVP_sha224();
				break;
			}

			case FingerprintAlgorithm::SHA256:
			{
				hashFunction = EVP_sha256();
				break;
			}

			case FingerprintAlgorithm::SHA384:
			{
				hashFunction = EVP_sha384();
				break;
			}

			case FingerprintAlgorithm::SHA512:
			{
				hashFunction = EVP_sha512();
				break;
			}
		}

		// Compare the remote fingerprint with the value given via signaling.
		ret = X509_digest(certificate, hashFunction, binaryFingerprint, &size);

		if (ret == 0)
		{
			MS_ERROR("X509_digest() failed");

			X509_free(certificate);

			return false;
		}

		// Convert to hexadecimal format in uppercase with colons.
		for (unsigned int i{ 0 }; i < size; ++i)
		{
			std::snprintf(hexFingerprint + (i * 3), 4, "%.2X:", binaryFingerprint[i]);
		}
		hexFingerprint[(size * 3) - 1] = '\0';

		if (this->remoteFingerprint->value != hexFingerprint)
		{
			MS_WARN_TAG(
			  dtls,
			  "fingerprint in the remote certificate (%s) does not match the announced one (%s)",
			  hexFingerprint,
			  this->remoteFingerprint->value.c_str());

			X509_free(certificate);

			return false;
		}

		MS_DEBUG_TAG(dtls, "valid remote fingerprint");

		// Get the remote certificate in PEM format.

		BIO* bio = BIO_new(BIO_s_mem());

		// Ensure the underlying BUF_MEM structure is also freed.
		// NOTE: Avoid stupid "warning: value computed is not used [-Wunused-value]"
		// since BIO_set_close() always returns 1.
		(void)BIO_set_close(bio, BIO_CLOSE);

		ret = PEM_write_bio_X509(bio, certificate);

		if (ret != 1)
		{
			LOG_OPENSSL_ERROR("PEM_write_bio_X509() failed");

			X509_free(certificate);
			BIO_free(bio);

			return false;
		}

		BUF_MEM* mem;

		BIO_get_mem_ptr(bio, &mem); // NOLINT[cppcoreguidelines-pro-type-cstyle-cast]

		if (!mem || !mem->data || mem->length == 0u)
		{
			LOG_OPENSSL_ERROR("BIO_get_mem_ptr() failed");

			X509_free(certificate);
			BIO_free(bio);

			return false;
		}

		this->remoteCert = std::string(mem->data, mem->length);

		X509_free(certificate);
		BIO_free(bio);

		return true;
	}

	void DtlsTransport::ExtractSrtpKeys(RTC::SrtpSession::CryptoSuite srtpCryptoSuite)
	{
		MS_TRACE();

		size_t srtpKeyLength{ 0 };
		size_t srtpSaltLength{ 0 };
		size_t srtpMasterLength{ 0 };

		switch (srtpCryptoSuite)
		{
			case RTC::SrtpSession::CryptoSuite::AEAD_AES_256_GCM:
			{
				srtpKeyLength    = SrtpAesGcm256MasterKeyLength;
				srtpSaltLength   = SrtpAesGcm256MasterSaltLength;
				srtpMasterLength = SrtpAesGcm256MasterLength;

				break;
			}

			case RTC::SrtpSession::CryptoSuite::AEAD_AES_128_GCM:
			{
				srtpKeyLength    = SrtpAesGcm128MasterKeyLength;
				srtpSaltLength   = SrtpAesGcm128MasterSaltLength;
				srtpMasterLength = SrtpAesGcm128MasterLength;

				break;
			}

			case RTC::SrtpSession::CryptoSuite::AES_CM_128_HMAC_SHA1_80:
			case RTC::SrtpSession::CryptoSuite::AES_CM_128_HMAC_SHA1_32:
			{
				srtpKeyLength    = SrtpMasterKeyLength;
				srtpSaltLength   = SrtpMasterSaltLength;
				srtpMasterLength = SrtpMasterLength;

				break;
			}
		}

		auto* srtpMaterial = new uint8_t[srtpMasterLength * 2];
		uint8_t* srtpLocalKey{ nullptr };
		uint8_t* srtpLocalSalt{ nullptr };
		uint8_t* srtpRemoteKey{ nullptr };
		uint8_t* srtpRemoteSalt{ nullptr };
		auto* srtpLocalMasterKey  = new uint8_t[srtpMasterLength];
		auto* srtpRemoteMasterKey = new uint8_t[srtpMasterLength];
		int ret;

		ret = SSL_export_keying_material(
		  this->ssl, srtpMaterial, srtpMasterLength * 2, "EXTRACTOR-dtls_srtp", 19, nullptr, 0, 0);

		MS_ASSERT(ret != 0, "SSL_export_keying_material() failed");
		MS_ASSERT(this->localRole.has_value(), "no DTLS role set");

		switch (this->localRole.value())
		{
			case Role::SERVER:
			{
				srtpRemoteKey  = srtpMaterial;
				srtpLocalKey   = srtpRemoteKey + srtpKeyLength;
				srtpRemoteSalt = srtpLocalKey + srtpKeyLength;
				srtpLocalSalt  = srtpRemoteSalt + srtpSaltLength;

				break;
			}

			case Role::CLIENT:
			{
				srtpLocalKey   = srtpMaterial;
				srtpRemoteKey  = srtpLocalKey + srtpKeyLength;
				srtpLocalSalt  = srtpRemoteKey + srtpKeyLength;
				srtpRemoteSalt = srtpLocalSalt + srtpSaltLength;

				break;
			}

			default:
			{
				MS_ABORT("no DTLS role set");
			}
		}

		// Create the SRTP local master key.
		std::memcpy(srtpLocalMasterKey, srtpLocalKey, srtpKeyLength);
		std::memcpy(srtpLocalMasterKey + srtpKeyLength, srtpLocalSalt, srtpSaltLength);
		// Create the SRTP remote master key.
		std::memcpy(srtpRemoteMasterKey, srtpRemoteKey, srtpKeyLength);
		std::memcpy(srtpRemoteMasterKey + srtpKeyLength, srtpRemoteSalt, srtpSaltLength);

		// Set state and notify the listener.
		this->state = DtlsState::CONNECTED;
		this->listener->OnDtlsTransportConnected(
		  this,
		  srtpCryptoSuite,
		  srtpLocalMasterKey,
		  srtpMasterLength,
		  srtpRemoteMasterKey,
		  srtpMasterLength,
		  this->remoteCert);

		delete[] srtpMaterial;
		delete[] srtpLocalMasterKey;
		delete[] srtpRemoteMasterKey;
	}

	std::optional<RTC::SrtpSession::CryptoSuite> DtlsTransport::GetNegotiatedSrtpCryptoSuite()
	{
		MS_TRACE();

		std::optional<RTC::SrtpSession::CryptoSuite> negotiatedSrtpCryptoSuite;

		// Ensure that the SRTP crypto suite has been negotiated.
		// NOTE: This is a OpenSSL type.
		SRTP_PROTECTION_PROFILE* sslSrtpCryptoSuite = SSL_get_selected_srtp_profile(this->ssl);

		if (!sslSrtpCryptoSuite)
		{
			return negotiatedSrtpCryptoSuite;
		}

		// Get the negotiated SRTP crypto suite.
		for (auto& srtpCryptoSuite : DtlsTransport::srtpCryptoSuites)
		{
			SrtpCryptoSuiteMapEntry* cryptoSuiteEntry = std::addressof(srtpCryptoSuite);

			if (std::strcmp(sslSrtpCryptoSuite->name, cryptoSuiteEntry->name) == 0)
			{
				MS_DEBUG_2TAGS(dtls, srtp, "chosen SRTP crypto suite: %s", cryptoSuiteEntry->name);

				negotiatedSrtpCryptoSuite = cryptoSuiteEntry->cryptoSuite;
			}
		}

		MS_ASSERT(
		  negotiatedSrtpCryptoSuite.has_value(), "chosen SRTP crypto suite is not an available one");

		return negotiatedSrtpCryptoSuite;
	}

	void DtlsTransport::OnSslInfo(int where, int ret)
	{
		MS_TRACE();

		const int w = where & -SSL_ST_MASK;
		const char* role;

		if ((w & SSL_ST_CONNECT) != 0)
		{
			role = "client";
		}
		else if ((w & SSL_ST_ACCEPT) != 0)
		{
			role = "server";
		}
		else
		{
			role = "undefined";
		}

		if ((where & SSL_CB_LOOP) != 0)
		{
			MS_DEBUG_TAG(dtls, "[role:%s, action:'%s']", role, SSL_state_string_long(this->ssl));
		}
		else if ((where & SSL_CB_ALERT) != 0)
		{
			const char* alertType;

			switch (*SSL_alert_type_string(ret))
			{
				case 'W':
				{
					alertType = "warning";
					break;
				}

				case 'F':
				{
					alertType = "fatal";
					break;
				}

				default:
				{
					alertType = "undefined";
				}
			}

			if ((where & SSL_CB_READ) != 0)
			{
				MS_WARN_TAG(dtls, "received DTLS %s alert: %s", alertType, SSL_alert_desc_string_long(ret));
			}
			else if ((where & SSL_CB_WRITE) != 0)
			{
				MS_DEBUG_TAG(dtls, "sending DTLS %s alert: %s", alertType, SSL_alert_desc_string_long(ret));
			}
			else
			{
				MS_DEBUG_TAG(dtls, "DTLS %s alert: %s", alertType, SSL_alert_desc_string_long(ret));
			}
		}
		else if ((where & SSL_CB_EXIT) != 0)
		{
			if (ret == 0)
			{
				MS_DEBUG_TAG(dtls, "[role:%s, failed:'%s']", role, SSL_state_string_long(this->ssl));
			}
			else if (ret < 0)
			{
				MS_DEBUG_TAG(dtls, "role: %s, waiting:'%s']", role, SSL_state_string_long(this->ssl));
			}
		}
		else if ((where & SSL_CB_HANDSHAKE_START) != 0)
		{
			MS_DEBUG_TAG(dtls, "DTLS handshake start");
		}
		else if ((where & SSL_CB_HANDSHAKE_DONE) != 0)
		{
			MS_DEBUG_TAG(dtls, "DTLS handshake done");

			this->handshakeDoneNow = true;
		}

		// NOTE: checking SSL_get_shutdown(this->ssl) & SSL_RECEIVED_SHUTDOWN here
		// upon receipt of a close alert does not work (the flag is set after this
		// callback).
	}

	void DtlsTransport::OnTimer(TimerHandle* /*timer*/)
	{
		MS_TRACE();

		// Workaround for https://github.com/openssl/openssl/issues/7998.
		if (this->handshakeDone)
		{
			MS_DEBUG_DEV("handshake is done so return");

			return;
		}

		// DTLSv1_handle_timeout is called when a DTLS handshake timeout expires.
		// If no timeout had expired, it returns 0. Otherwise, it retransmits the
		// previous flight of handshake messages and returns 1. If too many timeouts
		// had expired without progress or an error occurs, it returns -1.
		auto ret = DTLSv1_handle_timeout(this->ssl);

		if (ret == 1)
		{
			// Set the DTLS timer again.
			SetTimeout();
		}
		else if (ret == -1)
		{
			MS_WARN_TAG(dtls, "DTLSv1_handle_timeout() failed");

			Reset();

			// Set state and notify the listener.
			this->state = DtlsState::FAILED;
			this->listener->OnDtlsTransportFailed(this);
		}
	}
} // namespace RTC
