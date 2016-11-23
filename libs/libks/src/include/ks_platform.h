/*
 * Copyright (c) 2007-2015, Anthony Minessale II
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 
 * * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 * 
 * * Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the distribution.
 * 
 * * Neither the name of the original author; nor the names of any contributors
 * may be used to endorse or promote products derived from this software
 * without specific prior written permission.
 * 
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER
 * OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef _KS_PLATFORM_H_
#define _KS_PLATFORM_H_

KS_BEGIN_EXTERN_C

#if !defined(_XOPEN_SOURCE) && !defined(__FreeBSD__) && !defined(__NetBSD__) && !defined(__OpenBSD__) && !defined(__APPLE__)
#define _XOPEN_SOURCE 600
#endif

#if defined(__linux__) && !defined(_DEFAULT_SOURCE)
#define _DEFAULT_SOURCE 1
#endif

#if !defined(__WINDOWS__) && (defined(WIN32) || defined(WIN64) || defined(_MSC_VER) || defined(_WIN32))
#define __WINDOWS__
#endif

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#if UINTPTR_MAX == 0xffffffffffffffff
#define KS_64BIT 1
#endif

#include <stdarg.h>
#include <time.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <ctype.h>
#include <float.h>
#include <limits.h>
#include <sys/types.h>

#ifdef __WINDOWS__
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#else
#include <sys/time.h>
#include <sys/select.h>
#include <netinet/tcp.h>
#include <sys/signal.h>
#include <unistd.h>
#include <strings.h>
#include <stdint.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>				
#endif
	
#ifdef _MSC_VER
#pragma comment(lib, "Ws2_32.lib")

#include <io.h>
/*#include <winsock2.h>
#include <ws2tcpip.h>
#include <Synchapi.h>
*/
#ifndef open
#define open _open
#endif

#ifndef close
#define close _close
#endif

#ifndef read
#define read _read
#endif

#ifndef write
#define write _write
#endif

#ifndef __inline__
#define __inline__ __inline
#endif

#ifndef strdup
#define strdup _strdup
#endif

#if (_MSC_VER >= 1400)			/* VC8+ */
#ifndef _CRT_SECURE_NO_DEPRECATE
#define _CRT_SECURE_NO_DEPRECATE
#endif
#ifndef _CRT_NONSTDC_NO_DEPRECATE
#define _CRT_NONSTDC_NO_DEPRECATE
#endif
#endif

#ifndef strcasecmp
#define strcasecmp(s1, s2) _stricmp(s1, s2)
#endif

#ifndef strncasecmp
#define strncasecmp(s1, s2, n) _strnicmp(s1, s2, n)
#endif

#if (_MSC_VER < 1900)			/* VC 2015 */
#ifndef snprintf
#define snprintf _snprintf
#endif
#endif

#ifndef S_IRUSR
#define S_IRUSR _S_IREAD
#endif

#ifndef S_IWUSR
#define S_IWUSR _S_IWRITE
#endif

#endif  /* _MSC_VER */

#if (_MSC_VER >= 1400)			// VC8+
#define ks_assert(expr) assert(expr);__analysis_assume( expr )
#endif

#ifndef ks_assert
#define ks_assert(_x) assert(_x)
#endif

#ifdef __WINDOWS__
	typedef SOCKET ks_socket_t;
	typedef unsigned __int64 uint64_t;
	typedef unsigned __int32 uint32_t;
	typedef unsigned __int16 uint16_t;
	typedef unsigned __int8 uint8_t;
	typedef __int64 int64_t;
	typedef __int32 int32_t;
	typedef __int16 int16_t;
	typedef __int8 int8_t;
	typedef intptr_t ks_ssize_t;
	typedef int ks_filehandle_t;

#define KS_SOCK_INVALID INVALID_SOCKET
#define strerror_r(num, buf, size) strerror_s(buf, size, num)
#else
#define KS_SOCK_INVALID -1
	typedef int ks_socket_t;
	typedef ssize_t ks_ssize_t;
	typedef int ks_filehandle_t;
#endif

#ifdef __WINDOWS__
#if defined(KS_DECLARE_STATIC)
#define KS_DECLARE(type)			type __stdcall
#define KS_DECLARE_NONSTD(type)		type __cdecl
#define KS_DECLARE_DATA
#elif defined(KS_EXPORTS)
#define KS_DECLARE(type)			__declspec(dllexport) type __stdcall
#define KS_DECLARE_NONSTD(type)		__declspec(dllexport) type __cdecl
#define KS_DECLARE_DATA				__declspec(dllexport)
#else
#define KS_DECLARE(type)			__declspec(dllimport) type __stdcall
#define KS_DECLARE_NONSTD(type)		__declspec(dllimport) type __cdecl
#define KS_DECLARE_DATA				__declspec(dllimport)
#endif
#else							// !WIN32
#if (defined(__GNUC__) || defined(__SUNPRO_CC) || defined (__SUNPRO_C)) && defined(KS_API_VISIBILITY)
#define KS_DECLARE(type)		__attribute__((visibility("default"))) type
#define KS_DECLARE_NONSTD(type)	__attribute__((visibility("default"))) type
#define KS_DECLARE_DATA		__attribute__((visibility("default")))
#else
#define KS_DECLARE(type) type
#define KS_DECLARE_NONSTD(type) type
#define KS_DECLARE_DATA
#endif
#endif

/* malloc or DIE macros */
#ifdef NDEBUG
#define ks_malloc(ptr, len) (void)( (!!(ptr = malloc(len))) || (fprintf(stderr,"ABORT! Malloc failure at: %s:%d", __FILE__, __LINE__),abort(), 0), ptr )
#define ks_zmalloc(ptr, len) (void)( (!!(ptr = calloc(1, (len)))) || (fprintf(stderr,"ABORT! Malloc failure at: %s:%d", __FILE__, __LINE__),abort(), 0), ptr)
#if (_MSC_VER >= 1500)			// VC9+
#define ks_strdup(ptr, s) (void)( (!!(ptr = _strdup(s))) || (fprintf(stderr,"ABORT! Malloc failure at: %s:%d", __FILE__, __LINE__),abort(), 0), ptr)
#else
#define ks_strdup(ptr, s) (void)( (!!(ptr = strdup(s))) || (fprintf(stderr,"ABORT! Malloc failure at: %s:%d", __FILE__, __LINE__),abort(), 0), ptr)
#endif
#else
#if (_MSC_VER >= 1500)			// VC9+
#define ks_malloc(ptr, len) (void)(assert(((ptr) = malloc((len)))),ptr);__analysis_assume( ptr )
#define ks_zmalloc(ptr, len) (void)(assert((ptr = calloc(1, (len)))),ptr);__analysis_assume( ptr )
#define ks_strdup(ptr, s) (void)(assert(((ptr) = _strdup(s))),ptr);__analysis_assume( ptr )
#else
#define ks_malloc(ptr, len) (void)(assert(((ptr) = malloc((len)))),ptr)
#define ks_zmalloc(ptr, len) (void)(assert((ptr = calloc(1, (len)))),ptr)
#define ks_strdup(ptr, s) (void)(assert(((ptr) = strdup((s)))),ptr)
#endif
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

KS_END_EXTERN_C
#endif							/* defined(_KS_PLATFORM_H_) */
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
