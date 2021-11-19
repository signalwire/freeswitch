#ifndef COMPAT_H
#define COMPAT_H

#include "config.h"

#ifdef OS_UNIX
#include "compat.unix.h"
#endif


#ifdef OS_MSWIN
#include "compat.win32.h"
#endif

#endif
