/*
 * This file is part of the Sofia-SIP package
 *
 * Copyright (C) 2005,2006,2007 Nokia Corporation.
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

#ifndef SU_H
/** Defined when <sofia-sip/su.h> has been included. */
#define SU_H
/**@ingroup su_socket
 * @file sofia-sip/su.h Socket and network address interface
 *
 * @author Pekka Pessi <Pekka.Pessi@nokia.com>
 *
 * @date Created: Thu Mar 18 19:40:51 1999 pessi
 */

/* ---------------------------------------------------------------------- */
/* Includes */

#ifndef SU_CONFIG_H
#include <sofia-sip/su_config.h>
#endif
#ifndef SU_TYPES_H
#include <sofia-sip/su_types.h>
#endif
#ifndef SU_ERRNO_H
#include <sofia-sip/su_errno.h>
#endif

#include <stdio.h>
#include <limits.h>

#if SU_HAVE_BSDSOCK		/* Unix-compatible includes */
#include <errno.h>
#include <unistd.h>

#include <fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>

#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#endif

#if SU_HAVE_WINSOCK		/* Windows includes */
#  include <winsock2.h>
#  include <ws2tcpip.h>
#  if SU_HAVE_IN6
#    if defined(IPPROTO_IPV6) || (_WIN32_WINNT >= 0x0501)
/*     case 1: IPv6 defined in winsock2.h/ws2tcpip.h */
#    else
/*     case 2: try to use "IPv6 Tech Preview" */
#      include <tpipv6.h>
#    endif
#  endif
#endif

SOFIA_BEGIN_DECLS

/* ---------------------------------------------------------------------- */
/* Constant definitions */

#if SU_HAVE_BSDSOCK || DOCUMENTATION_ONLY
enum {
  /** Invalid socket descriptor, error from socket() or accept() */
  INVALID_SOCKET = -1,
#define INVALID_SOCKET ((su_socket_t)INVALID_SOCKET)
  /** Error from other socket calls */
  SOCKET_ERROR = -1,
#define SOCKET_ERROR SOCKET_ERROR
  /** Return code for a successful call */
  su_success = 0,
  /** Return code for an unsuccessful call */
  su_failure = -1
};
#if SYMBIAN && !defined(MSG_NOSIGNAL)
#define MSG_NOSIGNAL (0)
#endif
#elif SU_HAVE_WINSOCK
enum {
  su_success = 0,
  su_failure = 0xffffffffUL
};

#define MSG_NOSIGNAL (0)

#endif

/**@HI Maximum size of host name. */
#define SU_MAXHOST (1025)
/**@HI Maximum size of service name. */
#define SU_MAXSERV (25)

/**@HI Maximum size of address in text format. */
#define SU_ADDRSIZE (48)
/**@HI Maximum size of port number in text format. */
#define SU_SERVSIZE (16)

#define SU_SUCCESS su_success
#define SU_FAILURE su_failure

/* ---------------------------------------------------------------------- */
/* Type definitions */

/** Socket descriptor type. */
#if SU_HAVE_BSDSOCK || DOCUMENTATION_ONLY
typedef int su_socket_t;
#elif SU_HAVE_WINSOCK
typedef SOCKET su_socket_t;
#endif

#if !SU_HAVE_SOCKADDR_STORAGE
/*
 * RFC 2553: protocol-independent placeholder for socket addresses
 */
#define _SS_MAXSIZE	128
#define _SS_ALIGNSIZE	(sizeof(int64_t))
#define _SS_PAD1SIZE	(_SS_ALIGNSIZE - sizeof(u_char) * 2)
#define _SS_PAD2SIZE	(_SS_MAXSIZE - sizeof(u_char) * 2 - \
				_SS_PAD1SIZE - _SS_ALIGNSIZE)

struct sockaddr_storage {
#if SU_HAVE_SOCKADDR_SA_LEN
	unsigned char ss_len;		/* address length */
	unsigned char ss_family;	/* address family */
#else
	unsigned short ss_family;	/* address family */
#endif
	char	__ss_pad1[_SS_PAD1SIZE];
	int64_t __ss_align;	/* force desired structure storage alignment */
	char	__ss_pad2[_SS_PAD2SIZE];
};
#endif

/** Common socket address structure. */
union su_sockaddr_u {
#ifdef DOCUMENTATION_ONLY
  uint8_t             su_len;         /**< Length of structure */
  uint8_t             su_family;      /**< Address family. */
  uint16_t            su_port;        /**< Port number. */
#else
  short               su_dummy;	      /**< Dummy member to initialize */
#if SU_HAVE_SOCKADDR_SA_LEN
#define               su_len          su_sa.sa_len
#else
#define               su_len          su_array[0]
#endif
#define               su_family       su_sa.sa_family
#define               su_port         su_sin.sin_port
#endif

  char                su_array[32];   /**< Presented as chars */
  uint16_t            su_array16[16]; /**< Presented as 16-bit ints */
  uint32_t            su_array32[8];  /**< Presented as 32-bit ints */
  struct sockaddr     su_sa;          /**< Address in struct sockaddr format */
  struct sockaddr_in  su_sin;         /**< Address in IPv4 format */
#if SU_HAVE_IN6
  struct sockaddr_in6 su_sin6;        /**< Address in IPv6 format */
#endif
#ifdef DOCUMENTATION_ONLY
  uint32_t            su_scope_id;    /**< Scope ID. */
#else
#define               su_scope_id     su_array32[6]
#endif
};

typedef union su_sockaddr_u su_sockaddr_t;

#if SU_HAVE_BSDSOCK || DOCUMENTATION_ONLY
/**Type of @a siv_len field in #su_iovec_t.
 *
 * The @a siv_len field in #su_iovec_t has different types in with POSIX
 * (size_t) and WINSOCK2 (u_long). Truncate the iovec element size to
 * #SU_IOVECLEN_MAX, if needed, and cast using #su_ioveclen_t.
 *
 * @sa #su_iovec_t, #SU_IOVECLEN_MAX
 *
 * @since New in @VERSION_1_12_2.
 */
typedef size_t su_ioveclen_t;

/** I/O vector for scatter-gather I/O.
 *
 * This is the I/O vector element used with su_vsend() and su_vrecv(). It is
 * defined like struct iovec with POSIX sockets:
 * @code
 * struct iovec {
 *    void *iov_base;	// Pointer to data.
 *    size_t iov_len;	// Length of data.
 * };
 * @endcode
 *
 * When using WINSOCK sockets it is defined as
 * <a href="http://msdn.microsoft.com/library/en-us/winsock/winsock/wsabuf_2.asp">
 * WSABUF</a>:
 * @code
 * typedef struct __WSABUF {
 *   u_long len;
 *   char FAR* buf;
 * } WSABUF, *LPWSABUF;
 * @endcode
 *
 * @note Ordering of the fields is reversed on Windows. Do not initialize
 * this structure with static initializer, but assign both fields
 * separately. Note that the type of the siv_len is #su_ioveclen_t which is
 * defined as u_long on Windows and size_t on POSIX.
 *
 * For historical reasons, the structure is known as #msg_iovec_t in @msg
 * module.
 *
 * @sa #su_ioveclen_t, SU_IOVECLEN_MAX, su_vsend(), su_vrecv(),
 * #msg_iovec_t, msg_iovec(), msg_recv_iovec(),
 * @c struct @c iovec defined in <sys/uio.h>, writev(2), readv(2),
 * sendmsg(), recvmsg(),
 * <a href="http://msdn.microsoft.com/library/en-us/winsock/winsock/wsabuf_2.asp">
 * WSABUF of WinSock2</a>
 */
typedef struct su_iovec_s {
  void  *siv_base;		/**< Pointer to buffer. */
  su_ioveclen_t siv_len;		/**< Size of buffer.  */
} su_iovec_t;

/** Maximum size of buffer in a single su_iovec_t element.
 * @sa #su_ioveclen_t, #su_iovec_t
 *
 * @since New in @VERSION_1_12_2.
 * @HIDE
 */
#define SU_IOVECLEN_MAX SIZE_MAX
#endif

#if SU_HAVE_WINSOCK
typedef u_long su_ioveclen_t;

/* This is same as WSABUF */
typedef struct su_iovec_s {
  su_ioveclen_t  siv_len;
  void   *siv_base;
} su_iovec_t;

#define SU_IOVECLEN_MAX ULONG_MAX
#endif

/* ---------------------------------------------------------------------- */
/* Socket compatibility functions */

SOFIAPUBFUN int su_init(void);
SOFIAPUBFUN void su_deinit(void);

/** Create an endpoint for communication. */
SOFIAPUBFUN su_socket_t su_socket(int af, int sock, int proto);
/** Close an socket descriptor. */
SOFIAPUBFUN int su_close(su_socket_t s);
/** Control socket. */
SOFIAPUBFUN int su_ioctl(su_socket_t s, int request, ...);

/**Check for in-progress error codes.
 *
 * Checks if the @a errcode indicates that the socket call failed because
 * it would have blocked.
 *
 * Defined as macro with POSIX sockets.
 *
 * @since New in @VERSION_1_12_2.
 */
SOFIAPUBFUN int su_is_blocking(int errcode);

/** Set/reset blocking option. */
SOFIAPUBFUN int su_setblocking(su_socket_t s, int blocking);
/** Set/reset address reusing option. */
SOFIAPUBFUN int su_setreuseaddr(su_socket_t s, int reuse);
/** Get the error code associated with the socket. */
SOFIAPUBFUN int su_soerror(su_socket_t s);
/** Get the socket type. */
SOFIAPUBFUN int su_getsocktype(su_socket_t s);

/** Get size of message available in socket. */
SOFIAPUBFUN issize_t su_getmsgsize(su_socket_t s);

/** Scatter-gather send. */
SOFIAPUBFUN
issize_t su_vsend(su_socket_t, su_iovec_t const iov[], isize_t len, int flags,
		  su_sockaddr_t const *su, socklen_t sulen);
/** Scatter-gather receive. */
SOFIAPUBFUN
issize_t su_vrecv(su_socket_t, su_iovec_t iov[], isize_t len, int flags,
		  su_sockaddr_t *su, socklen_t *sulen);
/** Return local IP address */
SOFIAPUBFUN int su_getlocalip(su_sockaddr_t *sin);

#if SU_HAVE_BSDSOCK
#define su_ioctl  ioctl
/*
 * Note: before 1.12.2, there was su_isblocking() which did not take argument
 * and which was missing from WINSOCK
 */
#define su_is_blocking(e) \
((e) == EINPROGRESS || (e) == EAGAIN || (e) == EWOULDBLOCK || (e) == EINTR)
#endif

#if SU_HAVE_WINSOCK
SOFIAPUBFUN int su_inet_pton(int af, char const *src, void *dst);
SOFIAPUBFUN const char *su_inet_ntop(int af, void const *src,
				  char *dst, size_t size);
SOFIAPUBFUN ssize_t
  su_send(su_socket_t s, void *buffer, size_t length, int flags),
  su_sendto(su_socket_t s, void *buffer, size_t length, int flags,
	    su_sockaddr_t const *to, socklen_t tolen),
  su_recv(su_socket_t s, void *buffer, size_t length, int flags),
  su_recvfrom(su_socket_t s, void *buffer, size_t length, int flags,
	      su_sockaddr_t *from, socklen_t *fromlen);

static __inline
uint16_t su_ntohs(uint16_t s)
{
  return (uint16_t)(((s & 255) << 8) | ((s & 0xff00) >> 8));
}

static __inline
uint32_t su_ntohl(uint32_t l)
{
  return ((l & 0xff) << 24) | ((l & 0xff00) << 8)
       | ((l & 0xff0000) >> 8) | ((l & 0xff000000U) >> 24);
}

#define ntohs su_ntohs
#define htons su_ntohs
#define ntohl su_ntohl
#define htonl su_ntohl

#else
#define su_inet_pton inet_pton
#define su_inet_ntop inet_ntop
#define su_send(s,b,l,f) send((s),(b),(l),(f))
#define su_sendto(s,b,l,f,a,L) sendto((s),(b),(l),(f),(void const*)(a),(L))
#define su_recv(s,b,l,f) recv((s),(b),(l),(f))
#define su_recvfrom(s,b,l,f,a,L) recvfrom((s),(b),(l),(f),(void *)(a),(L))
#endif

/* ---------------------------------------------------------------------- */
/* Other compatibility stuff */

#if SU_HAVE_WINSOCK
#define getuid() (0x505)
#endif

#ifndef IPPROTO_SCTP
#define IPPROTO_SCTP (132)
#endif

/* ---------------------------------------------------------------------- */
/* Address manipulation macros */

/**@HI Get pointer to address field.
 *
 * The macro SU_ADDR() returns pointer to the address field (sin_data,
 * sin_addr or sin_addr6, depending on the address family).
 */
#if SU_HAVE_IN6
#define SU_ADDR(su) \
  ((su)->su_family == AF_INET ? (void *)&(su)->su_sin.sin_addr : \
  ((su)->su_family == AF_INET6 ? (void *)&(su)->su_sin6.sin6_addr : \
  (void *)&(su)->su_sa.sa_data))
#else
#define SU_ADDR(su) \
  ((su)->su_family == AF_INET ? (void *)&(su)->su_sin.sin_addr : \
  (void *)&(su)->su_sa.sa_data)
#endif

/**@HI Get length of address field.
 *
 * The macro SU_ADDRLEN() returns length of the address field (sin_data,
 * sin_addr or sin_addr6, depending on the address family).
 */
#if SU_HAVE_IN6
#define SU_ADDRLEN(su)					\
  ((su)->su_family == AF_INET				\
   ? (socklen_t)sizeof((su)->su_sin.sin_addr) :		\
   ((su)->su_family == AF_INET6				\
    ? (socklen_t)sizeof((su)->su_sin6.sin6_addr)	\
    : (socklen_t)sizeof((su)->su_sa.sa_data)))
#else
#define SU_ADDRLEN(su)					\
  ((su)->su_family == AF_INET				\
   ? (socklen_t)sizeof((su)->su_sin.sin_addr)		\
   : (socklen_t)sizeof((su)->su_sa.sa_data))
#endif

/**@HI Test if su_sockaddr_t is INADDR_ANY or IN6ADDR_ANY. */
#if SU_HAVE_IN6
#define SU_HAS_INADDR_ANY(su) \
  ((su)->su_family == AF_INET \
   ? ((su)->su_sin.sin_addr.s_addr == INADDR_ANY) \
   : ((su)->su_family == AF_INET6 \
      ? (memcmp(&(su)->su_sin6.sin6_addr, su_in6addr_any(), \
		sizeof(*su_in6addr_any())) == 0) : 0))
#else
#define SU_HAS_INADDR_ANY(su) \
  ((su)->su_family == AF_INET \
  ? ((su)->su_sin.sin_addr.s_addr == INADDR_ANY) : 0)
#endif

#define SU_SOCKADDR_INADDR_ANY(su) SU_HAS_INADDR_ANY(su)

/**@HI Calculate correct size of su_sockaddr_t structure. */
#if SU_HAVE_IN6
#define SU_SOCKADDR_SIZE(su) \
  ((socklen_t)((su)->su_family == AF_INET ? sizeof((su)->su_sin)	  \
	       : ((su)->su_family == AF_INET6 ? sizeof((su)->su_sin6)	\
		  : sizeof(*su))))
#else
#define SU_SOCKADDR_SIZE(su) \
  ((socklen_t)((su)->su_family == AF_INET ? sizeof((su)->su_sin)	\
	       : sizeof(*su)))
#endif
#define su_sockaddr_size SU_SOCKADDR_SIZE

#if SU_HAVE_IN6
#if SU_HAVE_BSDSOCK
#define su_in6addr_any()         (&in6addr_any)
#define su_in6addr_loopback()    (&in6addr_loopback)
#define SU_IN6ADDR_ANY_INIT      IN6ADDR_ANY_INIT
#define SU_IN6ADDR_LOOPBACK_INIT IN6ADDR_LOOPBACK_INIT
#endif
#if SU_HAVE_WINSOCK || DOCUMENTATION_ONLY
SOFIAPUBVAR const struct in_addr6 *su_in6addr_any(void);
SOFIAPUBVAR const struct in_addr6 *su_in6addr_loopback(void);
#define SU_IN6ADDR_ANY_INIT      { 0 }
#define SU_IN6ADDR_LOOPBACK_INIT { 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,1 }
#endif
#endif /* SU_HAVE_IN6 */

#define SU_IN6_IS_ADDR_V4MAPPED(a) \
  (((uint32_t const *) (a))[0] == 0 &&			      \
   ((uint32_t const *) (a))[1] == 0 &&			      \
   ((uint32_t const *) (a))[2] == htonl(0xffff))

#define SU_IN6_IS_ADDR_V4COMPAT(a)			      \
  (((uint32_t const *)(a))[0] == 0 &&			      \
   ((uint32_t const *)(a))[1] == 0 &&			      \
   ((uint32_t const *)(a))[2] == 0 &&			      \
   ((uint32_t const *)(a))[3] != htonl(1) &&		      \
   ((uint32_t const *)(a))[3] != htonl(0))

SOFIAPUBFUN int su_cmp_sockaddr(su_sockaddr_t const *a,
				su_sockaddr_t const *b);
SOFIAPUBFUN int su_match_sockaddr(su_sockaddr_t const *a,
				  su_sockaddr_t const *b);
SOFIAPUBFUN void su_canonize_sockaddr(su_sockaddr_t *su);

#if SU_HAVE_IN6
#define SU_CANONIZE_SOCKADDR(su) \
  ((su)->su_family == AF_INET6 ? su_canonize_sockaddr(su) : (void)0)
#else
#define SU_CANONIZE_SOCKADDR(su) \
  ((void)0)
#endif

SOFIA_END_DECLS

#ifndef SU_ADDRINFO_H
#include <sofia-sip/su_addrinfo.h>
#endif

#endif /* !defined(SU_H) */
