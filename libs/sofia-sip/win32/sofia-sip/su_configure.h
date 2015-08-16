/*
 * This file is part of the Sofia-SIP package
 *
 * Copyright (C) 2005 Nokia Corporation.
 *
 * Contact: Pekka Pessi <pekka.pessi@nokia.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation; either version 2.1 of
 * the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA
 *
 */

/**@file su_configure_win32.h
 *
 * @b su library configuration for WIN32 (VC6/VC98)
 *
 * The file <su_configure_win32.h> contains configuration information needed
 * by WIN32 programs using @b su library.
 *
 * @author Pekka Pessi <Pekka.Pessi@nokia.com>
 *
 * @date Created: Thu Jan 18 15:30:55 2001 ppessi
 */

#define WIN32_LEAN_AND_MEAN
/* Remove this when building DLL */
/* #define LIBSOFIA_SIP_UA_STATIC */

#define SU_HAVE_WIN32		1
#define SU_HAVE_WINSOCK         1
#define SU_HAVE_WINSOCK2        1
#define SU_HAVE_POLL            0
#define SU_HAVE_BSDSOCK         0
#define SU_HAVE_STDINT          (0)
#define SU_HAVE_NT              0

/* note: on Windows 2000 and older (WINVER<=500), IPv6-tech-preview
 * is needed for IPv4 support as well, so SU_HAVE_IN6 must be set */
#define SU_HAVE_IN6             1

#define SU_HAVE_PTHREADS        (1)

/** Define as 1 if you have sa_len field in struct sockaddr */
#undef SU_HAVE_SOCKADDR_SA_LEN

/** Define as 1 if you have struct sockaddr_storage */
#define SU_HAVE_SOCKADDR_STORAGE 1

/* Define this as 1 if you have if_nameindex() */
#undef SU_HAVE_IF_NAMEINDEX

/* Define as 1 if you have struct getaddrinfo. */
#define SU_HAVE_ADDRINFO     1

#define SU_INLINE                  __inline
#define su_inline                  static __inline
#define SU_HAVE_INLINE             (1)

/** Define this as 1 if we can use tags directly from stack. */
#define SU_HAVE_TAGSTACK (1)

#define SU_S64_T __int64
#define SU_U64_T unsigned __int64
#define SU_S32_T __int32
#define SU_U32_T unsigned __int32
#define SU_S16_T __int16
#define SU_U16_T unsigned __int16
#define SU_S8_T  __int8
#define SU_U8_T  unsigned __int8

#define SU_LEAST64_T __int64
#define SU_LEAST32_T __int32
#define SU_LEAST16_T __int16
#define SU_LEAST8_T  __int8

#define SU_S64_C(i) (SU_S64_T)(i ## L)
#define SU_U64_C(i) (SU_U64_T)(i ## UL)
#define SU_S32_C(i) (SU_S32_T)(i ## L)
#define SU_U32_C(i) (SU_U32_T)(i ## UL)
#define SU_S16_C(i) (SU_S16_T)(i)
#define SU_U16_C(i) (SU_U16_T)(i ## U)
#define SU_S8_C(i)  (SU_S8_T)(i)
#define SU_U8_C(i)  (SU_U8_T)(i ## U)

#ifndef strcasecmp
#define strcasecmp  _stricmp
#endif
#ifndef strncasecmp
#define strncasecmp _strnicmp
#endif
#if _MSC_VER < 1900
#define snprintf    _snprintf
#endif
#ifndef vsnprintf
#ifndef _MSC_VER
#define vsnprintf _vsnprintf
#endif
#if _MSC_VER < 1500
#define vsnprintf _vsnprintf
#endif
#endif

#define srandom(x)    srand((x))
#define random()      rand()

#include <basetsd.h>

#define SOFIA_ISIZE_T size_t
#define SOFIA_USIZE_T size_t

#ifdef _WIN64
#define SOFIA_SSIZE_T __int64
#define SOFIA_ISSIZE_T __int64
#define SU_INTPTR_T __int64
#elif _MSC_VER >= 1400
#define SOFIA_SSIZE_T __int32 __w64
#define SOFIA_ISSIZE_T __int32 __w64
#define SU_INTPTR_T __int32 __w64
#else
#define SOFIA_SSIZE_T __int32
#define SOFIA_ISSIZE_T __int32
#define SU_INTPTR_T __int32
#endif

#ifndef SIZE_MAX
#define SIZE_MAX MAXINT_PTR
#endif
#ifndef SSIZE_MAX
#define SSIZE_MAX MAXUINT_PTR
#endif

#define ISIZE_MAX SIZE_MAX
#define ISSIZE_MAX SSIZE_MAX
#define USIZE_MAX SIZE_MAX
