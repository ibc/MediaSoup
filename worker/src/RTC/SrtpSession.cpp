#define MS_CLASS "RTC::SrtpSession"
// #define MS_LOG_DEV_LEVEL 3

#include "RTC/SrtpSession.hpp"
#include "DepLibSRTP.hpp"
#ifdef MS_LIBURING_SUPPORTED
#include "DepLibUring.hpp"
#endif
#include "Logger.hpp"
#include "MediaSoupErrors.hpp"
#include <cstring> // std::memset(), std::memcpy()

namespace RTC
{
	/* Static. */

	static constexpr size_t EncryptBufferSize{ 65536 };
	thread_local static uint8_t EncryptBuffer[EncryptBufferSize];

	/* Class methods. */

	void SrtpSession::ClassInit()
	{
		// Set libsrtp event handler.
		const srtp_err_status_t err =
		  srtp_install_event_handler(static_cast<srtp_event_handler_func_t*>(OnSrtpEvent));

		if (DepLibSRTP::IsError(err))
		{
			MS_THROW_ERROR("srtp_install_event_handler() failed: %s", DepLibSRTP::GetErrorString(err));
		}
	}

	FBS::SrtpParameters::SrtpCryptoSuite SrtpSession::CryptoSuiteToFbs(CryptoSuite cryptoSuite)
	{
		switch (cryptoSuite)
		{
			case SrtpSession::CryptoSuite::AEAD_AES_256_GCM:
				return FBS::SrtpParameters::SrtpCryptoSuite::AEAD_AES_256_GCM;

			case SrtpSession::CryptoSuite::AEAD_AES_128_GCM:
				return FBS::SrtpParameters::SrtpCryptoSuite::AEAD_AES_128_GCM;

			case SrtpSession::CryptoSuite::AES_CM_128_HMAC_SHA1_80:
				return FBS::SrtpParameters::SrtpCryptoSuite::AES_CM_128_HMAC_SHA1_80;

			case SrtpSession::CryptoSuite::AES_CM_128_HMAC_SHA1_32:
				return FBS::SrtpParameters::SrtpCryptoSuite::AES_CM_128_HMAC_SHA1_32;
		}
	}

	SrtpSession::CryptoSuite SrtpSession::CryptoSuiteFromFbs(FBS::SrtpParameters::SrtpCryptoSuite cryptoSuite)
	{
		switch (cryptoSuite)
		{
			case FBS::SrtpParameters::SrtpCryptoSuite::AEAD_AES_256_GCM:
				return SrtpSession::CryptoSuite::AEAD_AES_256_GCM;

			case FBS::SrtpParameters::SrtpCryptoSuite::AEAD_AES_128_GCM:
				return SrtpSession::CryptoSuite::AEAD_AES_128_GCM;

			case FBS::SrtpParameters::SrtpCryptoSuite::AES_CM_128_HMAC_SHA1_80:
				return SrtpSession::CryptoSuite::AES_CM_128_HMAC_SHA1_80;

			case FBS::SrtpParameters::SrtpCryptoSuite::AES_CM_128_HMAC_SHA1_32:
				return SrtpSession::CryptoSuite::AES_CM_128_HMAC_SHA1_32;
		}
	}

	void SrtpSession::OnSrtpEvent(srtp_event_data_t* data)
	{
		MS_TRACE();

		switch (data->event)
		{
			case event_ssrc_collision:
				MS_WARN_TAG(srtp, "SSRC collision occurred");
				break;

			case event_key_soft_limit:
				MS_WARN_TAG(srtp, "stream reached the soft key usage limit and will expire soon");
				break;

			case event_key_hard_limit:
				MS_WARN_TAG(srtp, "stream reached the hard key usage limit and has expired");
				break;

			case event_packet_index_limit:
				MS_WARN_TAG(srtp, "stream reached the hard packet limit (2^48 packets)");
				break;
		}
	}

	/* Instance methods. */

	SrtpSession::SrtpSession(Type type, CryptoSuite cryptoSuite, uint8_t* key, size_t keyLen)
	{
		MS_TRACE();

		srtp_policy_t policy; // NOLINT(cppcoreguidelines-pro-type-member-init)

		// Set all policy fields to 0.
		std::memset(&policy, 0, sizeof(srtp_policy_t));

		switch (cryptoSuite)
		{
			case CryptoSuite::AEAD_AES_256_GCM:
			{
				srtp_crypto_policy_set_aes_gcm_256_16_auth(&policy.rtp);
				srtp_crypto_policy_set_aes_gcm_256_16_auth(&policy.rtcp);

				break;
			}

			case CryptoSuite::AEAD_AES_128_GCM:
			{
				srtp_crypto_policy_set_aes_gcm_128_16_auth(&policy.rtp);
				srtp_crypto_policy_set_aes_gcm_128_16_auth(&policy.rtcp);

				break;
			}

			case CryptoSuite::AES_CM_128_HMAC_SHA1_80:
			{
				srtp_crypto_policy_set_aes_cm_128_hmac_sha1_80(&policy.rtp);
				srtp_crypto_policy_set_aes_cm_128_hmac_sha1_80(&policy.rtcp);

				break;
			}

			case CryptoSuite::AES_CM_128_HMAC_SHA1_32:
			{
				srtp_crypto_policy_set_aes_cm_128_hmac_sha1_32(&policy.rtp);
				// NOTE: Must be 80 for RTCP.
				srtp_crypto_policy_set_aes_cm_128_hmac_sha1_80(&policy.rtcp);

				break;
			}

			default:
			{
				MS_ABORT("unknown SRTP crypto suite");
			}
		}

		MS_ASSERT(
		  (int)keyLen == policy.rtp.cipher_key_len,
		  "given keyLen does not match policy.rtp.cipher_keyLen");

		switch (type)
		{
			case Type::INBOUND:
				policy.ssrc.type = ssrc_any_inbound;
				break;

			case Type::OUTBOUND:
				policy.ssrc.type = ssrc_any_outbound;
				break;
		}

		policy.ssrc.value = 0;
		policy.key        = key;
		// Required for sending RTP retransmission without RTX.
		policy.allow_repeat_tx = 1;
		policy.window_size     = 1024;
		policy.next            = nullptr;

		// Set the SRTP session.
		const srtp_err_status_t err = srtp_create(&this->session, &policy);

		if (DepLibSRTP::IsError(err))
		{
			MS_THROW_ERROR("srtp_create() failed: %s", DepLibSRTP::GetErrorString(err));
		}
	}

	SrtpSession::~SrtpSession()
	{
		MS_TRACE();

		if (this->session != nullptr)
		{
			const srtp_err_status_t err = srtp_dealloc(this->session);

			if (DepLibSRTP::IsError(err))
			{
				MS_ABORT("srtp_dealloc() failed: %s", DepLibSRTP::GetErrorString(err));
			}
		}
	}

	bool SrtpSession::EncryptRtp(const uint8_t** data, size_t* len)
	{
		MS_TRACE();

		// Ensure that the resulting SRTP packet fits into the encrypt buffer.
		if (*len + SRTP_MAX_TRAILER_LEN > EncryptBufferSize)
		{
			MS_WARN_TAG(srtp, "cannot encrypt RTP packet, size too big (%zu bytes)", *len);

			return false;
		}

		uint8_t* encryptBuffer = EncryptBuffer;

#ifdef MS_LIBURING_SUPPORTED
		if (DepLibUring::IsEnabled())
		{
			if (!DepLibUring::IsActive())
			{
				goto protect;
			}

			// Use a preallocated buffer, if available.
			auto* sendBuffer = DepLibUring::GetSendBuffer();

			if (sendBuffer)
			{
				encryptBuffer = sendBuffer;
			}
		}

	protect:
#endif

		std::memcpy(encryptBuffer, *data, *len);

		const srtp_err_status_t err = srtp_protect(this->session, encryptBuffer, len);

		if (DepLibSRTP::IsError(err))
		{
			MS_WARN_TAG(srtp, "srtp_protect() failed: %s", DepLibSRTP::GetErrorString(err));

			return false;
		}

		// Update the given data pointer.
		*data = const_cast<const uint8_t*>(encryptBuffer);

		return true;
	}

	bool SrtpSession::DecryptSrtp(uint8_t* data, size_t* len)
	{
		MS_TRACE();

		const srtp_err_status_t err = srtp_unprotect(this->session, data, len);

		if (DepLibSRTP::IsError(err))
		{
			MS_DEBUG_TAG(srtp, "srtp_unprotect() failed: %s", DepLibSRTP::GetErrorString(err));

			return false;
		}

		return true;
	}

	bool SrtpSession::EncryptRtcp(const uint8_t** data, size_t* len)
	{
		MS_TRACE();

		// Ensure that the resulting SRTCP packet fits into the encrypt buffer.
		if (*len + SRTP_MAX_TRAILER_LEN > EncryptBufferSize)
		{
			MS_WARN_TAG(srtp, "cannot encrypt RTCP packet, size too big (%zu bytes)", *len);

			return false;
		}

		std::memcpy(EncryptBuffer, *data, *len);

		const srtp_err_status_t err = srtp_protect_rtcp(this->session, EncryptBuffer, len);

		if (DepLibSRTP::IsError(err))
		{
			MS_WARN_TAG(srtp, "srtp_protect_rtcp() failed: %s", DepLibSRTP::GetErrorString(err));

			return false;
		}

		// Update the given data pointer.
		*data = (const uint8_t*)EncryptBuffer;

		return true;
	}

	bool SrtpSession::DecryptSrtcp(uint8_t* data, size_t* len)
	{
		MS_TRACE();

		const srtp_err_status_t err = srtp_unprotect_rtcp(this->session, data, len);

		if (DepLibSRTP::IsError(err))
		{
			MS_DEBUG_TAG(srtp, "srtp_unprotect_rtcp() failed: %s", DepLibSRTP::GetErrorString(err));

			return false;
		}

		return true;
	}
} // namespace RTC
