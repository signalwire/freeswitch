#pragma once

/* From xmlrpc_amconfig.h */
  
/* Define to `unsigned' if <sys/types.h> doesn't define.  */
/* #undef size_t */

/* Define if you have the setgroups function.  */
/* #undef HAVE_SETGROUPS */

/* #undef HAVE_ASPRINTF */

/* Define if you have the wcsncmp function.  */
#define HAVE_WCSNCMP 1

/* Define if you have the <stdarg.h> header file.  */
#define HAVE_STDARG_H 1

/* Define if you have the <sys/filio.h> header file.  */
/* #undef HAVE_SYS_FILIO_H */

/* Define if you have the <sys/ioctl.h> header file.  */
/* #undef HAVE_SYS_IOCTL_H 1 */

/* Define if you have the <wchar.h> header file.  */
#define HAVE_WCHAR_H 1

/* Define if you have the socket library (-lsocket).  */
/* #undef HAVE_LIBSOCKET */

/* Name of package */
#define PACKAGE "xmlrpc-c"


/* Win32 version of xmlrpc_config.h

   Logical macros are 0 or 1 instead of the more traditional defined and
   undefined.  That's so we can distinguish when compiling code between
   "false" and some problem with the code.
*/

/* Define if va_list is actually an array. */
#define VA_LIST_IS_ARRAY 0
  
/* Define if we're using a copy of libwww with built-in SSL support. */
#define HAVE_LIBWWW_SSL 0

/* Used to mark unused variables under GCC... */
#define ATTR_UNUSED

/* Define this if your C library provides reasonably complete and correct Unicode wchar_t support. */
#define HAVE_UNICODE_WCHAR 1
  
#define DIRECTORY_SEPARATOR "\\"

  
/* Windows-specific includes. */
 
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#if !defined (vsnprintf)
  #define vsnprintf _vsnprintf
#endif
#if !defined (snprintf)
  #define snprintf _snprintf
#endif
#if !defined (popen) 
  #define popen _popen
#endif


#include <time.h>
#include <WINSOCK.h>
#include <direct.h>  /* for _chdir() */

/* We are linking against the multithreaded versions
   of the Microsoft runtimes - this makes gmtime 
   equiv to gmtime_r in that Windows gmtime is threadsafe
*/
#if !defined (gmtime_r)
static struct tm* gmtime_r(const time_t *timep, struct tm* result)
{
	struct tm *local;

	local = gmtime(timep);
	memcpy(result,local,sizeof(struct tm));
	return result;
}

#endif

#ifndef socklen_t
typedef unsigned int socklen_t;
#endif

/* inttypes.h */
#ifndef int8_t
typedef signed char       int8_t;
#endif
#ifndef uint8_t
typedef unsigned char     uint8_t;
#endif
#ifndef int16_t
typedef signed short      int16_t;
#endif
#ifndef uint16_t
typedef unsigned short    uint16_t;
#endif
#ifndef int32_t
typedef signed int        int32_t;
#endif
#ifndef uint32_t
typedef unsigned int      uint32_t;
#endif
#ifndef int64_t
typedef __int64           int64_t;
#endif
#ifndef uint64_t
typedef unsigned __int64  uint64_t;
#endif

#define __inline__ __inline

#define HAVE_SETENV 1
__inline BOOL setenv(const char* name, const char* value, int i) 
{
	return (SetEnvironmentVariable(name, value) != 0) ? TRUE : FALSE;
}

