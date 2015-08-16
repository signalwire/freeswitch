/* Win32 version of xmlrpc_config.h.

   For other platforms, this is generated automatically, but for Windows,
   someone generates it manually.  Nonetheless, we keep it looking as much
   as possible like the automatically generated one to make it easier to
   maintain (e.g. you can compare the two and see why something builds
   differently for Windows than for some other platform).

   The purpose of this file is to define stuff particular to the build
   environment being used to build Xmlrpc-c.  Xmlrpc-c source files can
   #include this file and have build-environment-independent source code.

   A major goal of this file is to reduce conditional compilation in
   the other source files as much as possible.  Even more, we want to avoid
   having to generate source code particular to a build environment
   except in this file.   

   This file is NOT meant to be used by any code outside of the
   Xmlrpc-c source tree.  There is a similar file that gets installed
   as <xmlrpc-c/config.h> that performs the same function for Xmlrpc-c
   interface header files that get compiled as part of a user's program.

   Logical macros are 0 or 1 instead of the more traditional defined and
   undefined.  That's so we can distinguish when compiling code between
   "false" and some problem with the code.
*/

#ifndef XMLRPC_CONFIG_H_INCLUDED
#define XMLRPC_CONFIG_H_INCLUDED

/* From xmlrpc_amconfig.h */

#define HAVE__STRICMP 1
#define HAVE__STRTOUI64 1

/* Name of package */
#define PACKAGE "xmlrpc-c"
/*----------------------------------*/

#ifndef HAVE_SETGROUPS
#define HAVE_SETGROUPS 0
#endif
#ifndef HAVE_ASPRINTF
#define HAVE_ASPRINTF 0
#endif
#ifndef HAVE_SETENV
#define HAVE_SETENV 0
#endif
#ifndef HAVE_PSELECT
#define HAVE_PSELECT 0
#endif
#ifndef HAVE_WCSNCMP
#define HAVE_WCSNCMP 1
#endif
#ifndef HAVE_GETTIMEOFDAY
#define HAVE_GETTIMEOFDAY 0
#endif
#ifndef HAVE_LOCALTIME_R
#define HAVE_LOCALTIME_R 0
#endif
#ifndef HAVE_GMTIME_R
#define HAVE_GMTIME_R 0
#endif
#ifndef HAVE_STRCASECMP
#define HAVE_STRCASECMP 0
#endif
#ifndef HAVE_STRICMP
#define HAVE_STRICMP 0
#endif
#ifndef HAVE__STRICMP
#define HAVE__STRICMP 0
#endif

#define HAVE_WCHAR_H 1
#define HAVE_SYS_FILIO_H 0
#define HAVE_SYS_IOCTL_H 0
#define HAVE_SYS_SELECT_H 0

#define VA_LIST_IS_ARRAY 0

#define HAVE_LIBWWW_SSL 0

/* Used to mark an unused function parameter */
#define ATTR_UNUSED

#define DIRECTORY_SEPARATOR "\\"

#define HAVE_UNICODE_WCHAR 1

/*  Xmlrpc-c code uses __inline__ to declare functions that should
    be compiled as inline code.  GNU C recognizes the __inline__ keyword.
    Others recognize 'inline' or '__inline' or nothing at all to say
    a function should be inlined.

    We could make 'configure' simply do a trial compile to figure out
    which one, but for now, this approximation is easier:
*/
#if (!defined(__GNUC__))
  #if (!defined(__inline__))
    #if (defined(__sgi) || defined(_AIX) || defined(_MSC_VER))
      #define __inline__ __inline
    #else   
      #define __inline__
    #endif
  #endif
#endif

/* MSVCRT means we're using the Microsoft Visual C++ runtime library */

#ifdef _MSC_VER
/* The compiler is Microsoft Visual C++. */
  #define MSVCRT _MSC_VER
#else
  #define MSVCRT 0
#endif

#if MSVCRT
  /* The MSVC runtime library _does_ have a 'struct timeval', but it is
     part of the Winsock interface (along with select(), which is probably
     its intended use), so isn't intended for use for general timekeeping.
  */
  #define HAVE_TIMEVAL 0
  #define HAVE_TIMESPEC 0
#else
  #define HAVE_TIMEVAL 1
  /* timespec is Posix.1b.  If we need to work on a non-Posix.1b non-Windows
     system, we'll have to figure out how to make Configure determine this.
  */
  #define HAVE_TIMESPEC 1
#endif

#if MSVCRT
  #define HAVE_WINDOWS_THREAD 1
#else
  #define HAVE_WINDOWS_THREAD 0
#endif

/* Some people have and use pthreads on Windows.  See
   http://sourceware.org/pthreads-win32 .  For that case, we can set
   HAVE_PTHREAD to 1.  The builder prefers to use pthreads if it has
   a choice.
*/
#define HAVE_PTHREAD 0

/* Note that the return value of XMLRPC_VSNPRINTF is int on Windows,
   ssize_t on POSIX.
*/
#if MSVCRT
  #define XMLRPC_VSNPRINTF _vsnprintf
#else
  #define XMLRPC_VSNPRINTF vsnprintf
#endif

#if MSVCRT
  #define HAVE_REGEX 0
#else
  #define HAVE_REGEX 1
#endif

#if MSVCRT
  #define XMLRPC_SOCKETPAIR xmlrpc_win32_socketpair
  #define XMLRPC_CLOSESOCKET closesocket
#else
  #define XMLRPC_SOCKETPAIR socketpair
  #define XMLRPC_CLOSESOCKET close
#endif

#if defined(_MSC_VER) && (_MSC_VER >= 1400)
/* Starting with MSVC 8, the runtime library defines various POSIX functions
   such as strdup() whose names violate the ISO C standard (the standard
   says the strXXX names are reserved for the standard), but warns you of
   the standards violation.  That warning is 4996, along with other warnings
   that tell you you're using a function that Microsoft thinks you
   shouldn't.

   Well, POSIX is more important than that element of ISO C, so we disable
   that warning.

   FYI, msvcrt also defines _strdup(), etc, which doesn't violate the
   naming standard.  But since other environments don't define _strdup(),
   we can't use it in portable code.
*/
#pragma warning(disable:4996)
#endif
/* Warning C4090 is "different 'const' qualifiers".

   We disable this warning because MSVC erroneously issues it when there is
   in fact no difference in const qualifiers:

     const char ** p;
     void * q;
     q = p;

   Note that both p and q are pointers to non-const.

   We have seen this in MSVC 7.1, 8, and 9 (but not 6).
*/
#pragma warning(disable:4090)

#if HAVE_STRTOLL
  # define XMLRPC_STRTOLL strtoll
#elif HAVE_STRTOQ
  # define XMLRPC_STRTOLL strtoq /* Interix */
#elif HAVE___STRTOLL
  # define XMLRPC_STRTOLL __strtoll /* HP-UX <= 11.11 */
#elif HAVE__STRTOUI64
  #define XMLRPC_STRTOLL _strtoui64  /* Windows MSVC */
#endif

#if HAVE_STRTOULL
  # define XMLRPC_STRTOULL strtoull
#elif HAVE_STRTOUQ
  # define XMLRPC_STRTOULL strtouq /* Interix */
#elif HAVE___STRTOULL
  # define XMLRPC_STRTOULL __strtoull /* HP-UX <= 11.11 */
#elif HAVE__STRTOUI64
  #define XMLRPC_STRTOULL _strtoui64  /* Windows MSVC */
#endif

#if _MSC_VER < 1900
#define snprintf _snprintf
#endif
#define popen _popen

#endif
