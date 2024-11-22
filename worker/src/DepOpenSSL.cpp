#define MS_CLASS "DepOpenSSL"
// #define MS_LOG_DEV_LEVEL 3

#include "DepOpenSSL.hpp"
#include "Logger.hpp"
#include <openssl/crypto.h>
#include <openssl/rand.h>
#include <mutex>

/* Static. */

static std::once_flag GlobalInitOnce;

/* Static methods. */

void DepOpenSSL::ClassInit()
{
	MS_TRACE();

	std::call_once(
	  GlobalInitOnce,
	  []
	  {
		  MS_DEBUG_TAG(info, "openssl version: \"%s\"", OpenSSL_version(OPENSSL_VERSION));
		  MS_DEBUG_TAG(info, "openssl CPU info: \"%s\"", OpenSSL_version(OPENSSL_CPU_INFO));

		  // Initialize some crypto stuff.
		  RAND_poll();
	  });
}
