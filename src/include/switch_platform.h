/* 
 * FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
 * Copyright (C) 2005-2012, Anthony Minessale II <anthm@freeswitch.org>
 *
 * Version: MPL 1.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
 *
 * The Initial Developer of the Original Code is
 * Anthony Minessale II <anthm@freeswitch.org>
 * Portions created by the Initial Developer are Copyright (C)
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 * 
 * Anthony Minessale II <anthm@freeswitch.org>
 *
 * switch_platform.h -- Platform Specific Header
 *
 */
/*! \file switch_platform.h
    \brief Platform Specific Header
*/
#ifndef SWITCH_PLATFORM_H
#define SWITCH_PLATFORM_H

SWITCH_BEGIN_EXTERN_C
#define SWITCH_USE_CLOCK_FUNCS

#if defined(WIN32) && defined(_MSC_VER)
#define atoll _atoi64
#endif
#ifdef __ICC
#pragma warning (disable:810 869 981 279 1469 188)
#endif
#include <stdio.h>
#define SWITCH_VA_NONE "%s", ""
#ifdef _MSC_VER
#define __SWITCH_FUNC__ __FUNCTION__
#else
#define __SWITCH_FUNC__ (const char *)__func__
#endif
#ifdef _MSC_VER
/* disable the following warnings 
 * C4100: The formal parameter is not referenced in the body of the function. The unreferenced parameter is ignored. 
 * C4200: Non standard extension C zero sized array
 * C4204: nonstandard extension used : non-constant aggregate initializer 
 * C4706: assignment within conditional expression
 * C4819: The file contains a character that cannot be represented in the current code page
 * C4132: 'object' : const object should be initialized (fires innapropriately for prototyped forward declaration of cost var)
 * C4510: default constructor could not be generated
 * C4512: assignment operator could not be generated
 * C4610: struct  can never be instantiated - user defined constructor required
 */
#pragma warning(disable:4100 4200 4204 4706 4819 4132 4510 4512 4610 4996)
#define SWITCH_HAVE_ODBC 1
#ifdef _MSC_VER
#  pragma comment(lib, "odbc32.lib")
#endif
#pragma include_alias(<libteletone.h>,				<../../libs/libteletone/src/libteletone.h>)
#pragma include_alias(<libteletone_generate.h>,		<../../libs/libteletone/src/libteletone_generate.h>)
#pragma include_alias(<libteletone_detect.h>,		<../../libs/libteletone/src/libteletone_detect.h>)
#if (_MSC_VER >= 1400)			// VC8+
#define switch_assert(expr) assert(expr);__analysis_assume( expr )
#endif
#if (_MSC_VER >= 1400)			// VC8+
#ifndef _CRT_SECURE_NO_DEPRECATE
#define _CRT_SECURE_NO_DEPRECATE
#endif
#ifndef _CRT_NONSTDC_NO_DEPRECATE
#define _CRT_NONSTDC_NO_DEPRECATE
#endif
#endif // VC8+
#if  _MSC_VER < 1300
#ifndef __FUNCTION__
#define __FUNCTION__ ""
#endif
#endif
#ifndef __cplusplus
#undef inline
#define inline __inline
#endif
#if !defined(_STDINT) && !defined(uint32_t)
typedef unsigned __int8 uint8_t;
typedef unsigned __int16 uint16_t;
typedef unsigned __int32 uint32_t;
typedef unsigned __int64 uint64_t;
typedef __int8 int8_t;
typedef __int16 int16_t;
typedef __int32 int32_t;
typedef __int64 int64_t;
typedef unsigned long in_addr_t;
#endif
typedef int pid_t;
typedef int uid_t;
typedef int gid_t;
#define PACKED
#include <io.h>
#define strcasecmp(s1, s2) stricmp(s1, s2)
#define strncasecmp(s1, s2, n) strnicmp(s1, s2, n)
#ifndef snprintf
#define snprintf _snprintf
#endif

#else
/* packed attribute */
#if (defined __SUNPRO_CC) || defined(__SUNPRO_C)
#define PACKED
#endif
#ifndef PACKED
#define PACKED __attribute__ ((__packed__))
#endif
#include <inttypes.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#endif // _MSC_VER
#ifndef __LITTLE_ENDIAN
#define __LITTLE_ENDIAN 1234
#endif
#ifndef __BIG_ENDIAN
#define __BIG_ENDIAN 4321
#endif
#ifndef __BYTE_ORDER
#ifdef SWITCH_BYTE_ORDER
#define __BYTE_ORDER SWITCH_BYTE_ORDER
#else
#define __BYTE_ORDER __LITTLE_ENDIAN
#endif
#endif
#ifdef WIN32
#if defined(SWITCH_CORE_DECLARE_STATIC)
#define SWITCH_DECLARE(type)			type __stdcall
#define SWITCH_DECLARE_NONSTD(type)		type __cdecl
#define SWITCH_DECLARE_DATA
#elif defined(FREESWITCHCORE_EXPORTS)
#define SWITCH_DECLARE(type)			__declspec(dllexport) type __stdcall
#define SWITCH_DECLARE_NONSTD(type)		__declspec(dllexport) type __cdecl
#define SWITCH_DECLARE_DATA				__declspec(dllexport)
#else
#define SWITCH_DECLARE(type)			__declspec(dllimport) type __stdcall
#define SWITCH_DECLARE_NONSTD(type)		__declspec(dllimport) type __cdecl
#define SWITCH_DECLARE_DATA				__declspec(dllimport)
#endif
#if defined(SWITCH_MOD_DECLARE_STATIC)
#define SWITCH_MOD_DECLARE(type)		type __stdcall
#define SWITCH_MOD_DECLARE_NONSTD(type)	type __cdecl
#define SWITCH_MOD_DECLARE_DATA
#elif defined(MOD_EXPORTS)
#define SWITCH_MOD_DECLARE(type)		__declspec(dllexport) type __stdcall
#define SWITCH_MOD_DECLARE_NONSTD(type)	__declspec(dllexport) type __cdecl
#define SWITCH_MOD_DECLARE_DATA			__declspec(dllexport)
#else
#define SWITCH_MOD_DECLARE(type)		__declspec(dllimport) type __stdcall
#define SWITCH_MOD_DECLARE_NONSTD(type)	__declspec(dllimport) type __cdecl
#define SWITCH_MOD_DECLARE_DATA			__declspec(dllimport)
#endif
#define SIGHUP SIGTERM
#ifndef S_IRUSR
#define S_IRUSR _S_IREAD
#endif
#ifndef S_IWUSR
#define S_IWUSR _S_IWRITE
#endif
#define SWITCH_THREAD_FUNC  __stdcall
#define SWITCH_DECLARE_CLASS
#else //not win32
#define O_BINARY 0
#if (defined(__GNUC__) || defined(__SUNPRO_CC) || defined (__SUNPRO_C)) && defined(SWITCH_API_VISIBILITY)
#define SWITCH_DECLARE(type)		__attribute__((visibility("default"))) type
#define SWITCH_DECLARE_NONSTD(type)	__attribute__((visibility("default"))) type
#define SWITCH_DECLARE_DATA		__attribute__((visibility("default")))
#define SWITCH_MOD_DECLARE(type)	__attribute__((visibility("default"))) type
#define SWITCH_MOD_DECLARE_NONSTD(type)	__attribute__((visibility("default"))) type
#define SWITCH_MOD_DECLARE_DATA		__attribute__((visibility("default")))
#define SWITCH_DECLARE_CLASS		__attribute__((visibility("default")))
#else
#define SWITCH_DECLARE(type)		type
#define SWITCH_DECLARE_NONSTD(type)	type
#define SWITCH_DECLARE_DATA
#define SWITCH_MOD_DECLARE(type)	type
#define SWITCH_MOD_DECLARE_NONSTD(type)	type
#define SWITCH_MOD_DECLARE_DATA
#define SWITCH_DECLARE_CLASS
#endif
#define SWITCH_THREAD_FUNC
#endif
#define SWITCH_DECLARE_CONSTRUCTOR SWITCH_DECLARE_DATA
#ifdef DOXYGEN
#define DoxyDefine(x) x
#else
#define DoxyDefine(x)
#endif
#if __GNUC__ >= 3
#define PRINTF_FUNCTION(fmtstr,vars) __attribute__((format(printf,fmtstr,vars)))
#else
#define PRINTF_FUNCTION(fmtstr,vars)
#endif
#ifdef SWITCH_INT32
typedef SWITCH_INT32 switch_int32_t;
#else
typedef int32_t switch_int32_t;
#endif

#ifdef SWITCH_SIZE_T
typedef SWITCH_SIZE_T switch_size_t;
#else
typedef uintptr_t switch_size_t;
#endif

#ifdef SWITCH_SSIZE_T
typedef SWITCH_SSIZE_T switch_ssize_t;
#else
typedef intptr_t switch_ssize_t;
#endif

#ifdef WIN32

#ifdef _WIN64
#define SWITCH_SSIZE_T_FMT          "lld"
#define SWITCH_SIZE_T_FMT           "lld"
#define FS_64BIT 1
#else
#define SWITCH_SSIZE_T_FMT          "d"
#define SWITCH_SIZE_T_FMT           "d"
#endif

#define SWITCH_INT64_T_FMT          "lld"
#define SWITCH_UINT64_T_FMT         "llu"

#ifndef TIME_T_FMT
#define TIME_T_FMT SWITCH_INT64_T_FMT
#endif

#else
#ifndef SWITCH_SSIZE_T_FMT
#define SWITCH_SSIZE_T_FMT          (sizeof (switch_ssize_t) == sizeof (long) ? "ld" : sizeof (switch_ssize_t) == sizeof (int) ? "d" : "lld")
#endif

#ifndef SWITCH_SIZE_T_FMT
#define SWITCH_SIZE_T_FMT           (sizeof (switch_size_t) == sizeof (long) ? "lu" : sizeof (switch_size_t) == sizeof (int) ? "u" : "llu")
#endif

#ifndef SWITCH_INT64_T_FMT
#define SWITCH_INT64_T_FMT          (sizeof (long) == 8 ? "ld" : "lld")
#endif

#ifndef SWITCH_UINT64_T_FMT
#define SWITCH_UINT64_T_FMT         (sizeof (long) == 8 ? "lu" : "llu")
#endif

#ifndef TIME_T_FMT
#if defined(__FreeBSD__) && SIZEOF_VOIDP == 4
#define TIME_T_FMT "d"
#else
#define TIME_T_FMT "ld"
#endif
#endif


#if UINTPTR_MAX == 0xffffffffffffffff
#define FS_64BIT 1
#endif

#endif

#define SWITCH_TIME_T_FMT SWITCH_INT64_T_FMT

SWITCH_END_EXTERN_C
/* these includes must be outside the extern "C" block on windows or it will break compatibility with c++ modules*/
#ifdef WIN32
/* Has windows.h already been included?  If so, our preferences don't matter,
 * but we will still need the winsock things no matter what was included.
 * If not, include a restricted set of windows headers to our tastes.
 */
#ifndef _WINDOWS_
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef _WIN32_WINNT
/* Restrict the server to a subset of Windows NT 4.0 header files by default
 */
#define _WIN32_WINNT 0x0400
#endif
#ifndef NOUSER
#define NOUSER
#endif
#ifndef NOMCX
#define NOMCX
#endif
#ifndef NOIME
#define NOIME
#endif
#include <windows.h>
/* 
 * Add a _very_few_ declarations missing from the restricted set of headers
 * (If this list becomes extensive, re-enable the required headers above!)
 * winsock headers were excluded by WIN32_LEAN_AND_MEAN, so include them now
 */
#define SW_HIDE             0
#ifndef _WIN32_WCE
#include <winsock2.h>
#include <mswsock.h>
#include <ws2tcpip.h>
#else
#include <winsock.h>
#endif
#endif /* !_WINDOWS_ */
#include <process.h>
#endif
#endif
#ifndef FALSE
#define FALSE 0
#endif
#ifndef TRUE
#define TRUE (!FALSE)
#endif
#ifndef switch_assert
#define switch_assert(expr) assert(expr)
#endif
#ifndef __ATTR_SAL
	/* used for msvc code analysis */
	/* http://msdn2.microsoft.com/en-us/library/ms235402.aspx */
#define _In_
#define _In_z_
#define _In_opt_z_
#define _In_opt_
#define _Printf_format_string_
#define _Ret_opt_z_
#define _Ret_z_
#define _Out_opt_
#define _Out_
#define _Check_return_
#define _Inout_
#define _Inout_opt_
#define _In_bytecount_(x)
#define _Out_opt_bytecapcount_(x)
#define _Out_bytecapcount_(x)
#define _Ret_
#define _Post_z_
#define _Out_cap_(x)
#define _Out_z_cap_(x)
#define _Out_ptrdiff_cap_(x)
#define _Out_opt_ptrdiff_cap_(x)
#define _Post_count_(x)
#endif
/* For Emacs:
 * Local Variables:
 * mode:c
 * indent-tabs-mode:t
 * tab-width:4
 * c-basic-offset:4
 * End:
 * For VIM:
 * vim:set softtabstop=4 shiftwidth=4 tabstop=4 noet:
 */
