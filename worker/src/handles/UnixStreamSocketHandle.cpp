/**
 * NOTE: This code cannot log to the Channel since this is the base code of the
 * Channel.
 */

#define MS_CLASS "UnixStreamSocketHandle"
// #define MS_LOG_DEV_LEVEL 3

#include "handles/UnixStreamSocketHandle.hpp"

UnixStreamSocketHandle::Writer UnixStreamSocketHandle::writer_;
