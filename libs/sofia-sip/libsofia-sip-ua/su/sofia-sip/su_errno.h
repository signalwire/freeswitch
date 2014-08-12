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

#ifndef SU_ERRNO_H
/** Defined when <sofia-sip/su_errno.h> has been included. */
#define SU_ERRNO_H

/**@file sofia-sip/su_errno.h Errno handling
 *
 * Source-code compatibility with Windows (having separate errno for
 * socket library and C libraries).
 *
 * @author Pekka Pessi <Pekka.Pessi@nokia.com>
 *
 * @date Created: Thu Dec 22 18:16:06 EET 2005 pessi
 */

#ifndef SU_CONFIG_H
#include "sofia-sip/su_config.h"
#endif

#include <errno.h>

SOFIA_BEGIN_DECLS

/** Return string describing su error code. */
SOFIAPUBFUN char const *su_strerror(int e);

/** The latest su error. */
SOFIAPUBFUN int su_errno(void);

/** Set the su error. */
SOFIAPUBFUN int su_seterrno(int);

#if defined(__APPLE_CC__) || defined(__OpenBSD__) || defined(__NetBSD__) || defined(__FreeBSD__)
#ifndef EBADMSG
#define EBADMSG EFAULT
#endif
#ifndef EPROTO
#define EPROTO EPROTOTYPE
#endif
#ifndef EBADMSG
#define EBADMSG EFAULT
#endif
#endif

#if defined(_WIN32)
/* VS 2010 defines these for POSIX but we cant have that */
#if (_MSC_VER > 1500)
#undef EWOULDBLOCK
#undef EINPROGRESS
#undef EALREADY
#undef ENOTSOCK
#undef EDESTADDRREQ
#undef EMSGSIZE
#undef EPROTOTYPE
#undef ENOPROTOOPT
#undef EPROTONOSUPPORT
#undef ESOCKTNOSUPPORT
#undef EOPNOTSUPP
#undef EPFNOSUPPORT
#undef EAFNOSUPPORT
#undef EADDRINUSE
#undef EADDRNOTAVAIL
#undef ENETDOWN
#undef ENETUNREACH
#undef ENETRESET
#undef ECONNABORTED
#undef ECONNRESET
#undef ENOBUFS
#undef EISCONN
#undef ENOTCONN
#undef ESHUTDOWN
#undef ETOOMANYREFS
#undef ETIMEDOUT
#undef ECONNREFUSED
#undef ELOOP
#undef EHOSTDOWN
#undef EHOSTUNREACH
#undef EPROCLIM
#undef EUSERS
#undef EDQUOT
#undef ESTALE
#undef EREMOTE
#undef EBADMSG
#undef EPROTO
#endif

#ifndef EWOULDBLOCK
#define EWOULDBLOCK  (10035) /* WSAEWOULDBLOCK */
#endif

#ifndef EINPROGRESS
#define EINPROGRESS  (10036) /* WSAEINPROGRESS */
#endif

#ifndef EALREADY
#define EALREADY (10037) /* WSAEALREADY */
#endif

#ifndef ENOTSOCK
#define ENOTSOCK (10038) /* WSAENOTSOCK */
#endif

#ifndef EDESTADDRREQ
#define EDESTADDRREQ (10039) /* WSAEDESTADDRREQ */
#endif

#ifndef EMSGSIZE
#define EMSGSIZE (10040) /* WSAEMSGSIZE */
#endif

#ifndef EPROTOTYPE
#define EPROTOTYPE (10041) /* WSAEPROTOTYPE */
#endif

#ifndef ENOPROTOOPT
#define ENOPROTOOPT (10042) /* WSAENOPROTOOPT */
#endif

#ifndef EPROTONOSUPPORT
#define EPROTONOSUPPORT (10043) /* WSAEPROTONOSUPPORT */
#endif

#ifndef ESOCKTNOSUPPORT
#define ESOCKTNOSUPPORT (10044) /* WSAESOCKTNOSUPPORT */
#endif

#ifndef EOPNOTSUPP
#define EOPNOTSUPP (10045) /* WSAEOPNOTSUPP */
#endif

#ifndef EPFNOSUPPORT
#define EPFNOSUPPORT (10046) /* WSAEPFNOSUPPORT */
#endif

#ifndef EAFNOSUPPORT
#define EAFNOSUPPORT (10047) /* WSAEAFNOSUPPORT */
#endif

#ifndef EADDRINUSE
#define EADDRINUSE (10048) /* WSAEADDRINUSE */
#endif

#ifndef EADDRNOTAVAIL
#define EADDRNOTAVAIL (10049) /* WSAEADDRNOTAVAIL */
#endif

#ifndef ENETDOWN
#define ENETDOWN (10050) /* WSAENETDOWN */
#endif

#ifndef ENETUNREACH
#define ENETUNREACH (10051) /* WSAENETUNREACH */
#endif

#ifndef ENETRESET
#define ENETRESET (10052) /* WSAENETRESET */
#endif

#ifndef ECONNABORTED
#define ECONNABORTED (10053) /* WSAECONNABORTED */
#endif

#ifndef ECONNRESET
#define ECONNRESET (10054) /* WSAECONNRESET */
#endif

#ifndef ENOBUFS
#define ENOBUFS (10055) /* WSAENOBUFS */
#endif

#ifndef EISCONN
#define EISCONN (10056) /* WSAEISCONN */
#endif

#ifndef ENOTCONN
#define ENOTCONN (10057) /* WSAENOTCONN */
#endif

#ifndef ESHUTDOWN
#define ESHUTDOWN (10058) /* WSAESHUTDOWN */
#endif

#ifndef ETOOMANYREFS
#define ETOOMANYREFS (10059) /* WSAETOOMANYREFS */
#endif

#ifndef ETIMEDOUT
#define ETIMEDOUT (10060) /* WSAETIMEDOUT */
#endif

#ifndef ECONNREFUSED
#define ECONNREFUSED (10061) /* WSAECONNREFUSED */
#endif

#ifndef ELOOP
#define ELOOP (10062) /* WSAELOOP */
#endif

#ifndef ENAMETOOLONG
#define ENAMETOOLONG (10063) /* WSAENAMETOOLONG */
#endif

#ifndef EHOSTDOWN
#define EHOSTDOWN (10064) /* WSAEHOSTDOWN */
#endif

#ifndef EHOSTUNREACH
#define EHOSTUNREACH (10065) /* WSAEHOSTUNREACH */
#endif

#ifndef ENOTEMPTY
#define ENOTEMPTY (10066) /* WSAENOTEMPTY */
#endif

#ifndef EPROCLIM
#define EPROCLIM (10067) /* WSAEPROCLIM */
#endif

#ifndef EUSERS
#define EUSERS (10068) /* WSAEUSERS */
#endif

#ifndef EDQUOT
#define EDQUOT (10069) /* WSAEDQUOT */
#endif

#ifndef ESTALE
#define ESTALE (10070) /* WSAESTALE */
#endif

#ifndef EREMOTE
#define EREMOTE (10071) /* WSAEREMOTE */
#endif

#ifndef EBADMSG
#  if defined(WSABADMSG)
#    define EBADMSG (WSAEBADMSG)
#  else
#    define EBADMSG (20005)
#  endif
#endif

#ifndef EPROTO
#  if defined(WSAEPROTO)
#    define EPROTO WSAEPROTO
#  else
#    define EPROTO (20006)
#  endif
#endif

#endif

SOFIA_END_DECLS

#endif
