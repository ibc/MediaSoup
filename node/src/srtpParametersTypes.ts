/**
 * SRTP parameters.
 */
export type SrtpParameters = {
	/**
	 * Encryption and authentication transforms to be used.
	 */
	cryptoSuite: SrtpCryptoSuite;

	/**
	 * SRTP keying material (master key and salt) in Base64.
	 */
	keyBase64: string;
};

/**
 * SRTP crypto suite.
 */
export type SrtpCryptoSuite =
	| 'AEAD_AES_256_GCM'
	| 'AEAD_AES_128_GCM'
	| 'AES_CM_128_HMAC_SHA1_80'
	| 'AES_CM_128_HMAC_SHA1_32';
