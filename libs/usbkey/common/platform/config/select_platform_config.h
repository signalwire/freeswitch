#if defined(linux) || defined(__linux) || defined(__linux__)
// linux:
#  define PLATFORM_CONFIG "config/platform/linux.h"

#elif defined(__FreeBSD__) || defined(__NetBSD__) || defined(__OpenBSD__) || defined(__DragonFly__)
// BSD:
#  define PLATFORM_CONFIG "config/platform/bsd.h"

#elif defined(sun) || defined(__sun)
// solaris:
#  define PLATFORM_CONFIG "config/platform/solaris.h"

#elif defined(__sgi)
// SGI Irix:
#  define PLATFORM_CONFIG "config/platform/irix.h"

#elif defined(__hpux)
// hp unix:
#  define PLATFORM_CONFIG "config/platform/hpux.h"

#elif defined(__CYGWIN__)
// cygwin is not win32:
#  define PLATFORM_CONFIG "config/platform/cygwin.h"

#elif defined(_WIN32) || defined(__WIN32__) || defined(WIN32)
// win32:
#  define PLATFORM_CONFIG "config/platform/win32.h"

#elif defined(__BEOS__)
// BeOS
#  define PLATFORM_CONFIG "config/platform/beos.h"

#elif defined(macintosh) || defined(__APPLE__) || defined(__APPLE_CC__)
// MacOS
#  define PLATFORM_CONFIG "config/platform/macos.h"

#elif defined(__IBMCPP__) || defined(_AIX)
// IBM
#  define PLATFORM_CONFIG "config/platform/aix.h"

#endif



