#ifndef LIBETPAN_CONFIG_H
#define LIBETPAN_CONFIG_H

#ifdef WIN32
#	define PATH_MAX 512

// Windows API security level
#	define SECURITY_WIN32

#	ifdef __cplusplus
#		define PropVariantInit __inline PropVariantInit
#		pragma warning( push )
#		pragma warning( disable :  4005 4141 )
#	endif

#	include <tchar.h>
#	include <stdio.h>
#	include <string.h>
#	include <io.h>
#	include <Winsock2.h>

#	ifdef __cplusplus
#		pragma warning( pop )
#		undef  PropVariantInit
#	endif

#	if !defined(snprintf)
#		define snprintf _snprintf
#	endif
#	if !defined(strncasecmp)
#		define strncasecmp _strnicmp
#	endif
#	if !defined(strcasecmp)
#		define strcasecmp _stricmp
#	endif

	/* use Windows Types */
#	if !defined(ssize_t)
		typedef SSIZE_T ssize_t;
#	endif
#	if !defined(uint16_t)
		typedef UINT16 uint16_t;
#	endif
#	if !defined(uint32_t)
		typedef UINT32 uint32_t;
#	endif
#	if !defined(int32_t)
		typedef INT32 int32_t;
#	endif
#	if !defined(pid_t)
		typedef DWORD pid_t;
#	endif

#	if !defined(caddr_t)
		typedef void * caddr_t;
#	endif

	/* avoid config.h*/
#	define CONFIG_H
#endif // WIN32

#include <limits.h>
#ifdef _MSC_VER
#	define MMAP_UNAVAILABLE
#	define inline __inline
#else
#	include <sys/param.h>
#endif
#define MAIL_DIR_SEPARATOR '/'
#define MAIL_DIR_SEPARATOR_S "/"
#ifdef _MSC_VER
#	ifdef LIBETPAN_DLL
#		define LIBETPAN_EXPORT __declspec(dllexport)
#	else
#		define LIBETPAN_EXPORT __declspec(dllimport)
#   endif
#else
#	define LIBETPAN_EXPORT
#endif
#endif
