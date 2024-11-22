#define MS_CLASS "Utils::File"
// #define MS_LOG_DEV_LEVEL 3

#include "Logger.hpp"
#include "MediaSoupErrors.hpp"
#include "Utils.hpp"
#include <cerrno>
#include <sys/stat.h> // stat()
#ifdef _WIN32
#include <io.h>
#define __S_ISTYPE(mode, mask) (((mode)&_S_IFMT) == (mask))
#define S_ISREG(mode) __S_ISTYPE((mode), _S_IFREG)
#else
#include <unistd.h> // access(), R_OK
#endif

namespace Utils
{
	void Utils::File::CheckFile(const char* file)
	{
		MS_TRACE();

		struct stat fileStat
		{
		}; // NOLINT(cppcoreguidelines-pro-type-member-init)
		int err;

		// Ensure the given file exists.
		err = stat(file, &fileStat);

		if (err != 0)
		{
			MS_THROW_ERROR("cannot read file '%s': %s", file, std::strerror(errno));
		}

		// Ensure it is a regular file.
		if (!S_ISREG(fileStat.st_mode))
		{
			MS_THROW_ERROR("'%s' is not a regular file", file);
		}

		// Ensure it is readable.
		err = access(file, R_OK);

		if (err != 0)
		{
			MS_THROW_ERROR("cannot read file '%s': %s", file, std::strerror(errno));
		}
	}
} // namespace Utils
