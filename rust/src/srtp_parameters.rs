//! Collection of SRTP-related data structures that are used to specify SRTP encryption/decryption
//! parameters.

use mediasoup_sys::fbs::srtp_parameters;
use serde::{Deserialize, Serialize};

/// SRTP parameters.
#[derive(Debug, Clone, Ord, PartialOrd, Eq, PartialEq, Hash, Deserialize, Serialize)]
#[serde(rename_all = "camelCase")]
pub struct SrtpParameters {
    /// Encryption and authentication transforms to be used.
    pub crypto_suite: SrtpCryptoSuite,
    /// SRTP keying material (master key and salt) in Base64.
    pub key_base64: String,
}

impl SrtpParameters {
    pub(crate) fn from_fbs(tuple: &srtp_parameters::SrtpParameters) -> Self {
        Self {
            crypto_suite: SrtpCryptoSuite::from_fbs(tuple.crypto_suite),
            key_base64: String::from(tuple.key_base64.as_str()),
        }
    }

    pub(crate) fn to_fbs(&self) -> srtp_parameters::SrtpParameters {
        srtp_parameters::SrtpParameters {
            crypto_suite: SrtpCryptoSuite::to_fbs(self.crypto_suite),
            key_base64: String::from(self.key_base64.as_str()),
        }
    }
}

/// SRTP crypto suite.
#[derive(Debug, Copy, Clone, Ord, PartialOrd, Eq, PartialEq, Hash, Deserialize, Serialize)]
pub enum SrtpCryptoSuite {
    /// AEAD_AES_256_GCM
    #[serde(rename = "AEAD_AES_256_GCM")]
    AeadAes256Gcm,
    /// AEAD_AES_128_GCM
    #[serde(rename = "AEAD_AES_128_GCM")]
    AeadAes128Gcm,
    /// AES_CM_128_HMAC_SHA1_80
    #[serde(rename = "AES_CM_128_HMAC_SHA1_80")]
    AesCm128HmacSha180,
    /// AES_CM_128_HMAC_SHA1_32
    #[serde(rename = "AES_CM_128_HMAC_SHA1_32")]
    AesCm128HmacSha132,
}

impl SrtpCryptoSuite {
    pub(crate) fn from_fbs(crypto_suite: srtp_parameters::SrtpCryptoSuite) -> Self {
        match crypto_suite {
            srtp_parameters::SrtpCryptoSuite::AeadAes256Gcm => Self::AeadAes256Gcm,
            srtp_parameters::SrtpCryptoSuite::AeadAes128Gcm => Self::AeadAes128Gcm,
            srtp_parameters::SrtpCryptoSuite::AesCm128HmacSha180 => Self::AesCm128HmacSha180,
            srtp_parameters::SrtpCryptoSuite::AesCm128HmacSha132 => Self::AesCm128HmacSha132,
        }
    }

    pub(crate) fn to_fbs(self) -> srtp_parameters::SrtpCryptoSuite {
        match self {
            Self::AeadAes256Gcm => srtp_parameters::SrtpCryptoSuite::AeadAes256Gcm,
            Self::AeadAes128Gcm => srtp_parameters::SrtpCryptoSuite::AeadAes128Gcm,
            Self::AesCm128HmacSha180 => srtp_parameters::SrtpCryptoSuite::AesCm128HmacSha180,
            Self::AesCm128HmacSha132 => srtp_parameters::SrtpCryptoSuite::AesCm128HmacSha132,
        }
    }
}

impl Default for SrtpCryptoSuite {
    fn default() -> Self {
        Self::AesCm128HmacSha180
    }
}
