/* Licensed to the Apache Software Foundation (ASF) under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.
 * The ASF licenses this file to You under the Apache License, Version 2.0
 * (the "License"); you may not use this file except in compliance with
 * the License.  You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */


#ifndef APR_H
#define APR_H

/* GENERATED FILE WARNING!  DO NOT EDIT apr.h
 *
 * You must modify apr.hw instead.
 *
 * And please, make an effort to stub apr.hnw and apr.h.in in the process.
 *
 * This is the Win32 specific version of apr.h.  It is copied from
 * apr.hw by the apr.dsp and libapr.dsp projects. 
 */

/**
 * @file apr.h
 * @brief APR Platform Definitions
 * @remark This is a generated header generated from include/apr.h.in by
 * ./configure, or copied from include/apr.hw or include/apr.hnw 
 * for Win32 or Netware by those build environments, respectively.
 */

#if defined(WIN32) || defined(DOXYGEN)

/* Ignore most warnings (back down to /W3) for poorly constructed headers
 */
#if defined(_MSC_VER) && _MSC_VER >= 1200
#pragma warning(push, 3)
#endif

/* disable or reduce the frequency of...
 *   C4057: indirection to slightly different base types
 *   C4075: slight indirection changes (unsigned short* vs short[])
 *   C4100: unreferenced formal parameter
 *   C4127: conditional expression is constant
 *   C4163: '_rotl64' : not available as an intrinsic function
 *   C4201: nonstandard extension nameless struct/unions
 *   C4244: int to char/short - precision loss
 *   C4514: unreferenced inline function removed
 */
#pragma warning(disable: 4100 4127 4163 4201 4514; once: 4057 4075 4244)

/* Ignore Microsoft's interpretation of secure development
 * and the POSIX string handling API
 */
#if defined(_MSC_VER) && _MSC_VER >= 1400
#ifndef _CRT_SECURE_NO_DEPRECATE
#define _CRT_SECURE_NO_DEPRECATE
#endif
#pragma warning(disable: 4996)
#endif

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

/**
 * @defgroup apr_platform Platform Definitions
 * @ingroup APR 
 * @{
 */

#define APR_INLINE __inline
#define APR_HAS_INLINE          1
#ifndef __attribute__
#define __attribute__(__x)
#endif

#ifndef _WIN32_WCE
#define APR_HAVE_ARPA_INET_H    0
#define APR_HAVE_CONIO_H        1
#define APR_HAVE_CRYPT_H        0
#define APR_HAVE_CTYPE_H        1
#define APR_HAVE_DIRENT_H       0
#define APR_HAVE_ERRNO_H        1
#define APR_HAVE_FCNTL_H        1
#define APR_HAVE_IO_H           1
#define APR_HAVE_LIMITS_H       1
#define APR_HAVE_NETDB_H        0
#define APR_HAVE_NETINET_IN_H   0
#define APR_HAVE_NETINET_SCTP_H 0
#define APR_HAVE_NETINET_SCTP_UIO_H 0
#define APR_HAVE_NETINET_TCP_H  0
#define APR_HAVE_PTHREAD_H      0
#define APR_HAVE_SIGNAL_H       1
#define APR_HAVE_STDARG_H       1
#define APR_HAVE_STDINT_H       0
#define APR_HAVE_STDIO_H        1
#define APR_HAVE_STDLIB_H       1
#define APR_HAVE_STRING_H       1
#define APR_HAVE_STRINGS_H      0
#define APR_HAVE_SYS_SENDFILE_H 0
#define APR_HAVE_SYS_SIGNAL_H   0
#define APR_HAVE_SYS_SOCKET_H   0
#define APR_HAVE_SYS_SOCKIO_H   0
#define APR_HAVE_SYS_SYSLIMITS_H 0
#define APR_HAVE_SYS_TIME_H     0
#define APR_HAVE_SYS_TYPES_H    1
#define APR_HAVE_SYS_UIO_H      0
#define APR_HAVE_SYS_WAIT_H     0
#define APR_HAVE_UNISTD_H       0
#define APR_HAVE_STDDEF_H       1
#define APR_HAVE_PROCESS_H      1
#define APR_HAVE_TIME_H         1
#else
#define APR_HAVE_ARPA_INET_H    0
#define APR_HAVE_CONIO_H        0
#define APR_HAVE_CRYPT_H        0
#define APR_HAVE_CTYPE_H        0
#define APR_HAVE_DIRENT_H       0
#define APR_HAVE_ERRNO_H        0
#define APR_HAVE_FCNTL_H        0
#define APR_HAVE_IO_H           0
#define APR_HAVE_LIMITS_H       0
#define APR_HAVE_NETDB_H        0
#define APR_HAVE_NETINET_IN_H   0
#define APR_HAVE_NETINET_TCP_H  0
#define APR_HAVE_PTHREAD_H      0
#define APR_HAVE_SIGNAL_H       0
#define APR_HAVE_STDARG_H       0
#define APR_HAVE_STDINT_H       0
#define APR_HAVE_STDIO_H        1
#define APR_HAVE_STDLIB_H       1
#define APR_HAVE_STRING_H       1
#define APR_HAVE_STRINGS_H      0
#define APR_HAVE_SYS_SENDFILE_H 0
#define APR_HAVE_SYS_SIGNAL_H   0
#define APR_HAVE_SYS_SOCKET_H   0
#define APR_HAVE_SYS_SYSLIMITS_H 0
#define APR_HAVE_SYS_TIME_H     0
#define APR_HAVE_SYS_TYPES_H    0
#define APR_HAVE_SYS_UIO_H      0
#define APR_HAVE_SYS_WAIT_H     0
#define APR_HAVE_UNISTD_H       0
#define APR_HAVE_STDDEF_H       0
#define APR_HAVE_PROCESS_H      0
#define APR_HAVE_TIME_H         0
#endif

#define APR_USE_FLOCK_SERIALIZE           0 
#define APR_USE_SYSVSEM_SERIALIZE         0
#define APR_USE_FCNTL_SERIALIZE           0
#define APR_USE_PROC_PTHREAD_SERIALIZE    0 
#define APR_USE_PTHREAD_SERIALIZE         0 

#define APR_HAS_FLOCK_SERIALIZE           0
#define APR_HAS_SYSVSEM_SERIALIZE         0
#define APR_HAS_FCNTL_SERIALIZE           0
#define APR_HAS_PROC_PTHREAD_SERIALIZE    0
#define APR_HAS_RWLOCK_SERIALIZE          0

#define APR_HAS_LOCK_CREATE_NP            0

#define APR_PROCESS_LOCK_IS_GLOBAL        0

#define APR_USES_ANONYMOUS_SHM            0
#define APR_USES_FILEBASED_SHM            0
#define APR_USES_KEYBASED_SHM             0

#define APR_FILE_BASED_SHM      0
#define APR_MEM_BASED_SHM       0

#define APR_HAVE_CORKABLE_TCP   0
#define APR_HAVE_GETRLIMIT      0
#define APR_HAVE_ICONV          0
#define APR_HAVE_IN_ADDR        1
#define APR_HAVE_INET_ADDR      1
#define APR_HAVE_INET_NETWORK   0
#define APR_HAVE_IPV6           0
#define APR_HAVE_MEMMOVE        1
#define APR_HAVE_SETRLIMIT      0
#define APR_HAVE_SIGACTION      0
#define APR_HAVE_SIGSUSPEND     0
#define APR_HAVE_SIGWAIT        0
#define APR_HAVE_STRCASECMP     0
#define APR_HAVE_STRDUP         1
#define APR_HAVE_STRNCASECMP    0
#define APR_HAVE_STRSTR         1
#define APR_HAVE_MEMCHR         1
#define APR_HAVE_STRUCT_RLIMIT  0
#define APR_HAVE_UNION_SEMUN    0
#define APR_HAVE_SCTP           0

#ifndef _WIN32_WCE
#define APR_HAVE_STRICMP        1
#define APR_HAVE_STRNICMP       1
#define APR_PROCATTR_USER_SET_REQUIRES_PASSWORD 1
#else
#define APR_HAVE_STRICMP        0
#define APR_HAVE_STRNICMP       0
#define APR_PROCATTR_USER_SET_REQUIRES_PASSWORD 0
#endif

/** @} */

/* We don't include our conditional headers within the doxyblocks 
 * or the extern "C" namespace 
 */

#if APR_HAVE_STDLIB_H
#include <stdlib.h>
#endif
#if APR_HAVE_STDIO_H
#include <stdio.h>
#endif
#if APR_HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#if APR_HAVE_STDDEF_H
#include <stddef.h>
#endif
#if APR_HAVE_TIME_H
#include <time.h>
#endif
#if APR_HAVE_PROCESS_H
#include <process.h>
#endif
#if APR_HAVE_IPV6
#include <ws2tcpip.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @addtogroup apr_platform
 * @ingroup APR 
 * @{
 */

/*  APR Feature Macros */
#define APR_HAS_SHARED_MEMORY     1
#define APR_HAS_THREADS           1
#define APR_HAS_MMAP              1
#define APR_HAS_FORK              0
#define APR_HAS_RANDOM            1
#define APR_HAS_OTHER_CHILD       1
#define APR_HAS_DSO               1
#define APR_HAS_SO_ACCEPTFILTER   0
#define APR_HAS_UNICODE_FS        1
#define APR_HAS_PROC_INVOKED      1
#ifndef _WIN32_WCE
#define APR_HAS_SENDFILE          1
#define APR_HAS_USER              1
#define APR_HAS_LARGE_FILES       1
#define APR_HAS_XTHREAD_FILES     1
#else
#define APR_HAS_SENDFILE          0
#define APR_HAS_USER              0
#define APR_HAS_LARGE_FILES       0
#define APR_HAS_XTHREAD_FILES     0
#endif
#define APR_HAS_OS_UUID           1

/* Win32 cannot poll [just yet] on files/pipes.
 */
#define APR_FILES_AS_SOCKETS      0

/* This macro indicates whether or not EBCDIC is the native character set.
 */
#define APR_CHARSET_EBCDIC        0

/* Is the TCP_NODELAY socket option inherited from listening sockets?
 */
#define APR_TCP_NODELAY_INHERITED 1

/* Is the O_NONBLOCK flag inherited from listening sockets?
 */
#define APR_O_NONBLOCK_INHERITED  1

/* Typedefs that APR needs. */

typedef  unsigned char     apr_byte_t;

typedef  short             apr_int16_t;
typedef  unsigned short    apr_uint16_t;
                                               
typedef  int               apr_int32_t;
typedef  unsigned int      apr_uint32_t;
                                               
typedef  __int64           apr_int64_t;
typedef  unsigned __int64  apr_uint64_t;

typedef  size_t      apr_size_t;
#if APR_HAVE_STDDEF_H
typedef  ptrdiff_t   apr_ssize_t;
#else
typedef  int         apr_ssize_t;
#endif
#if APR_HAS_LARGE_FILES
typedef  __int64     apr_off_t;
#else
typedef  int         apr_off_t;
#endif
typedef  int         apr_socklen_t;

/* Are we big endian? */
/* XXX: Fatal assumption on Alpha platforms */
#define APR_IS_BIGENDIAN	0

#ifdef WIN64
#define APR_SIZEOF_VOIDP   8
#else
#define APR_SIZEOF_VOIDP   4
#endif

/* XXX These simply don't belong here, perhaps in apr_portable.h
 * based on some APR_HAVE_PID/GID/UID?
 */
typedef  int         pid_t;
typedef  int         uid_t;
typedef  int         gid_t;

/* Mechanisms to properly type numeric literals */

#define APR_INT64_C(val) (val##i64)
#define APR_UINT64_C(val) (val##Ui64)


#if APR_HAVE_IPV6

/* Appears in later flavors, not the originals. */
#ifndef in_addr6
#define  in6_addr    in_addr6
#endif

#ifndef WS2TCPIP_INLINE
#define IN6_IS_ADDR_V4MAPPED(a) \
    (   (*(const apr_uint64_t *)(const void *)(&(a)->s6_addr[0]) == 0) \
     && (*(const apr_uint32_t *)(const void *)(&(a)->s6_addr[8]) == ntohl(0x0000ffff)))
#endif

#endif /* APR_HAS_IPV6 */

/* Definitions that APR programs need to work properly. */

/** 
 * Thread callbacks from APR functions must be declared with APR_THREAD_FUNC, 
 * so that they follow the platform's calling convention.
 * @example
 */
/** void* APR_THREAD_FUNC my_thread_entry_fn(apr_thread_t *thd, void *data);
 */
#define APR_THREAD_FUNC  __stdcall


#if defined(DOXYGEN) || !defined(WIN32)

/**
 * The public APR functions are declared with APR_DECLARE(), so they may
 * use the most appropriate calling convention.  Public APR functions with 
 * variable arguments must use APR_DECLARE_NONSTD().
 *
 * @remark Both the declaration and implementations must use the same macro.
 * @example
 */
/** APR_DECLARE(rettype) apr_func(args)
 * @see APR_DECLARE_NONSTD @see APR_DECLARE_DATA
 * @remark Note that when APR compiles the library itself, it passes the 
 * symbol -DAPR_DECLARE_EXPORT to the compiler on some platforms (e.g. Win32) 
 * to export public symbols from the dynamic library build.\n
 * The user must define the APR_DECLARE_STATIC when compiling to target
 * the static APR library on some platforms (e.g. Win32.)  The public symbols 
 * are neither exported nor imported when APR_DECLARE_STATIC is defined.\n
 * By default, compiling an application and including the APR public
 * headers, without defining APR_DECLARE_STATIC, will prepare the code to be
 * linked to the dynamic library.
 */
#define APR_DECLARE(type)            type 

/**
 * The public APR functions using variable arguments are declared with 
 * APR_DECLARE_NONSTD(), as they must follow the C language calling convention.
 * @see APR_DECLARE @see APR_DECLARE_DATA
 * @remark Both the declaration and implementations must use the same macro.
 * @example
 */
/** APR_DECLARE_NONSTD(rettype) apr_func(args, ...);
 */
#define APR_DECLARE_NONSTD(type)     type

/**
 * The public APR variables are declared with AP_MODULE_DECLARE_DATA.
 * This assures the appropriate indirection is invoked at compile time.
 * @see APR_DECLARE @see APR_DECLARE_NONSTD
 * @remark Note that the declaration and implementations use different forms,
 * but both must include the macro.
 * @example
 */
/** extern APR_DECLARE_DATA type apr_variable;\n
 * APR_DECLARE_DATA type apr_variable = value;
 */
#define APR_DECLARE_DATA

#elif defined(APR_DECLARE_STATIC)
#define APR_DECLARE(type)            type __stdcall
#define APR_DECLARE_NONSTD(type)     type __cdecl
#define APR_DECLARE_DATA
#elif defined(APR_DECLARE_EXPORT)
#define APR_DECLARE(type)            __declspec(dllexport) type __stdcall
#define APR_DECLARE_NONSTD(type)     __declspec(dllexport) type __cdecl
#define APR_DECLARE_DATA             __declspec(dllexport)
#else
#define APR_DECLARE(type)            __declspec(dllimport) type __stdcall
#define APR_DECLARE_NONSTD(type)     __declspec(dllimport) type __cdecl
#define APR_DECLARE_DATA             __declspec(dllimport)
#endif

#ifdef WIN64
#define APR_SSIZE_T_FMT          "I64d"
#define APR_SIZE_T_FMT           "I64d"
#else
#define APR_SSIZE_T_FMT          "d"
#define APR_SIZE_T_FMT           "d"
#endif

#if APR_HAS_LARGE_FILES
#define APR_OFF_T_FMT            "I64d"
#else
#define APR_OFF_T_FMT            "d"
#endif

#define APR_PID_T_FMT            "d"

#define APR_INT64_T_FMT          "I64d"
#define APR_UINT64_T_FMT         "I64u"
#define APR_UINT64_T_HEX_FMT     "I64x"

/* Local machine definition for console and log output. */
#define APR_EOL_STR              "\r\n"

/* No difference between PROC and GLOBAL mutex */
#define APR_PROC_MUTEX_IS_GLOBAL      1

typedef int apr_wait_t;

/* struct iovec is needed to emulate Unix writev */
struct iovec {
    char*      iov_base;
    apr_size_t iov_len;
};

/* Nasty Win32 .h ommissions we really need */
#define STDIN_FILENO  0
#define STDOUT_FILENO 1
#define STDERR_FILENO 2

#if APR_HAS_UNICODE_FS
/* An arbitrary size that is digestable. True max is a bit less than 32000 */
#define APR_PATH_MAX 8192
#else /* !APR_HAS_UNICODE_FS */
#define APR_PATH_MAX MAX_PATH
#endif

/** @} */

#ifdef __cplusplus
}
#endif

/* Done with badly written headers
 */
#if defined(_MSC_VER) && _MSC_VER >= 1200
#pragma warning(pop)
#pragma warning(disable: 4996)
#endif

#endif /* WIN32 */

#endif /* APR_H */
