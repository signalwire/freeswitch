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

/**@ingroup su_socket
 * @CFILE su.c OS-independent socket functions
 *
 * @author Pekka Pessi <Pekka.Pessi@nokia.com>
 *
 * @date Created: Thu Mar 18 19:40:51 1999 pessi
 */

#include "config.h"

#if HAVE_SYS_SOCKIO_H
#include <sys/sockio.h>
#endif

#include <sofia-sip/su.h>
#include <sofia-sip/su_log.h>
#include <sofia-sip/su_alloc.h>

#include <stdio.h>
#include <string.h>
#if HAVE_SIGNAL
#include <signal.h>
#endif

#if !SU_HAVE_BSDSOCK && !SU_HAVE_WINSOCK
#error Bad configuration
#endif

#ifndef FD_CLOEXEC
#define FD_CLOEXEC (1)
#endif

int su_socket_close_on_exec = 0;
int su_socket_blocking = 0;

#if HAVE_OPEN_C && HAVE_NET_IF_H
#include <net/if.h>
#endif

#if SU_HAVE_BSDSOCK && HAVE_OPEN_C
char su_global_ap_name[IFNAMSIZ];
extern int su_get_local_ip_addr(su_sockaddr_t *su);
#endif

/** Create a socket endpoint for communication.
 *
 * @param af addressing family
 * @param socktype socket type
 * @param proto protocol number specific to the addressing family
 *
 * The newly created socket is nonblocking unless global variable
 * su_socket_blocking is set to true.
 *
 * Also, the newly created socket is closed on exec() if global variable
 * su_socket_close_on_exec is set to true. Note that a multithreaded program
 * can fork() and exec() before the close-on-exec flag is set.
 *
 * @return A valid socket descriptor or INVALID_SOCKET (-1) upon an error.
 */
su_socket_t su_socket(int af, int socktype, int proto)
{
#if HAVE_OPEN_C
  struct ifconf ifc;
  int numifs = 64;
  char *buffer;
  struct ifreq ifr;
  int const su_xtra = 0;
#endif

  su_socket_t s = socket(af, socktype, proto);

  if (s != INVALID_SOCKET) {
#if SU_HAVE_BSDSOCK
    if (su_socket_close_on_exec)
      fcntl(s, F_SETFD, FD_CLOEXEC); /* Close on exec */
#endif
    if (!su_socket_blocking)	/* All sockets are born blocking */
      su_setblocking(s, 0);
  }

#if HAVE_OPEN_C
  /* Use AP we have raised up */
  memset(&ifr, 0, sizeof(struct ifreq));
  strncpy(ifr.ifr_name, (char const *) su_global_ap_name, IFNAMSIZ);

  /* Assign socket to an already active access point (interface) */
  ioctl(s, SIOCSIFNAME, &ifr);
  ioctl(s, SIOCIFSTART, &ifr);
#endif

  return s;
}

#if HAVE_OPEN_C
#include <errno.h>
su_sockaddr_t su_ap[1];
int ifindex;
void *aConnection;
extern void *su_localinfo_ap_set(su_sockaddr_t *su, int *index);
extern int su_localinfo_ap_deinit(void *aconn);
#define NUMIFS 64

int su_localinfo_ap_name_to_index(int ap_index)
{
  struct ifconf ifc;

  struct ifreq *ifr, *ifr_next;
  int error = EFAULT;
  char *buffer, buf[NUMIFS * sizeof(struct ifreq)];
  su_socket_t s;

  s= socket(AF_INET, SOCK_STREAM, 0);
  if (s < 0)
    return -1;

  ifc.ifc_len = NUMIFS * sizeof (struct ifreq);

  memset(buf, 0, ifc.ifc_len);
  ifc.ifc_buf = buf;
  if (ioctl(s, SIOCGIFCONF, (char *)&ifc) < 0) {
    return error;
  }

  buffer = ifc.ifc_buf + ifc.ifc_len;

  for (ifr = ifc.ifc_req;
       (void *)ifr < (void *)buffer;
       ifr = ifr_next) {
    struct ifreq ifreq[1];
    int scope, if_index, flags = 0, gni_flags = 0;
    char *if_name;
    su_sockaddr_t su2[1];

#if SA_LEN
    if (ifr->ifr_addr.sa_len > sizeof(ifr->ifr_addr))
      ifr_next = (struct ifreq *)
	(ifr->ifr_addr.sa_len + (char *)(&ifr->ifr_addr));
    else
#else
      ifr_next = ifr + 1;
#endif

    if_name = ifr->ifr_name;

#if defined(SIOCGIFINDEX)
    ifreq[0] = *ifr;
    if (ioctl(s, SIOCGIFINDEX, ifreq) < 0) {
return -1;
    }
#if HAVE_IFR_INDEX
    if_index = ifreq->ifr_index;
#elif HAVE_IFR_IFINDEX
    if_index = ifreq->ifr_ifindex;
#else
#error Unknown index field in struct ifreq
#endif

    if (ap_index == if_index)
    {
      strncpy(su_global_ap_name, (const char *) if_name, sizeof(su_global_ap_name));
      error = 0;
    };

#else
#error su_localinfo() cannot map interface name to number
#endif

  }

  close(s);
  return error;
}
#endif

#if SU_HAVE_BSDSOCK || DOCUMENTATION_ONLY
/** Initialize socket implementation.
 *
 * Before using any sofia-sip-ua functions, the application should call
 * su_init() in order to initialize run-time environment including sockets.
 * This function may prepare plugins if there are any.
 *
 * @par POSIX Implementation
 * The su_init() initializes debugging logs and ignores the SIGPIPE signal.
 *
 * @par Windows Implementation
 * The function su_init() initializes Winsock2 library on Windows.
 *
 * @par Symbian Implementation
 * The function su_init() prompts user to select an access point (interface
 * towards Internet) and uses the activated access point for the socket
 * operations.
 */
int su_init(void)
{
#if HAVE_OPEN_C
  char apname[60];
  su_socket_t s;
#endif

  su_home_threadsafe(NULL);

#if HAVE_SIGPIPE
  signal(SIGPIPE, SIG_IGN);	/* we want to get EPIPE instead */
#endif

#if HAVE_OPEN_C
  /* This code takes care of enabling an access point (interface) */
  aConnection = su_localinfo_ap_set(su_ap, &ifindex);
  su_localinfo_ap_name_to_index(ifindex);
#endif

  su_log_init(su_log_default);
  su_log_init(su_log_global);

  return 0;
}

/** Deinitialize socket implementation. */
void su_deinit(void)
{
#if HAVE_OPEN_C
	su_localinfo_ap_deinit(aConnection);
#endif
}

/** Close an socket descriptor. */
int su_close(su_socket_t s)
{
  return close(s);
}

int su_setblocking(su_socket_t s, int blocking)
{
  int mode = fcntl(s, F_GETFL, 0);

  if (mode < 0)
     return -1;

  if (blocking)
    mode &= ~(O_NDELAY | O_NONBLOCK);
  else
    mode |= O_NDELAY | O_NONBLOCK;

  return fcntl(s, F_SETFL, mode);
}
#endif

#if SU_HAVE_WINSOCK
int su_init(void)
{
  WORD	wVersionRequested;
  WSADATA	wsaData;

  wVersionRequested = MAKEWORD(2, 0);

  if (WSAStartup(wVersionRequested, &wsaData) !=0) {
    return -1;
  }

  su_log_init(su_log_default);

  su_log_init(su_log_global);

  return 0;
}

void su_deinit(void)
{
  WSACleanup();
}

/** Close a socket descriptor. */
int su_close(su_socket_t s)
{
  return closesocket(s);
}

/** Control socket. */
int su_ioctl(su_socket_t s, int request, ...)
{
  int retval;
  void *argp;
  va_list va;
  va_start(va, request);
  argp = va_arg(va, void *);
  retval = ioctlsocket(s, request, argp);
  va_end(va);
  return retval;
}

int su_is_blocking(int errcode)
{
  return errcode == EAGAIN || errcode == EWOULDBLOCK || errcode == EINPROGRESS;
}

int su_setblocking(su_socket_t s, int blocking)
{
  unsigned long nonBlock = !blocking;

  return ioctlsocket(s, FIONBIO, &nonBlock);
}


#endif /* SU_HAVE_WINSOCK */

int su_soerror(su_socket_t s)
{
  int error = 0;
  socklen_t errorlen = sizeof(error);

  getsockopt(s, SOL_SOCKET, SO_ERROR, (void *)&error, &errorlen);

  return error;
}

int su_getsocktype(su_socket_t s)
{
  int socktype = 0;
  socklen_t intlen = sizeof(socktype);

  if (getsockopt(s, SOL_SOCKET, SO_TYPE, (void *)&socktype, &intlen) < 0)
    return -1;

  return socktype;
}

int su_setreuseaddr(su_socket_t s, int reuse)
{
#ifdef SO_REUSEPORT
	if (setsockopt(s, SOL_SOCKET, SO_REUSEPORT,
				   (void *)&reuse, (socklen_t)sizeof(reuse)) < 0)
		return -1;
#endif
	if (setsockopt(s, SOL_SOCKET, SO_REUSEADDR,
				   (void *)&reuse, (socklen_t)sizeof(reuse)) < 0)
		return -1;
	return 0;
}


#if HAVE_SYS_FILIO_H
#include <sys/filio.h>
#endif

#if SU_HAVE_WINSOCK
issize_t su_getmsgsize(su_socket_t s)
{
  unsigned long n = (unsigned long)-1;
  if (ioctlsocket(s, FIONREAD, &n) == -1)
    return -1;
  return (issize_t)n;
}
#elif HAVE_OPEN_C
issize_t su_getmsgsize(su_socket_t s)
{
  int n = -1;
  if (su_ioctl(s, E32IONREAD, &n) == -1)
    return -1;
  return (issize_t)n;
}
#else
issize_t su_getmsgsize(su_socket_t s)
{
  int n = -1;
  if (su_ioctl(s, FIONREAD, &n) == -1)
    return -1;
  return (issize_t)n;
}
#endif

#if SU_HAVE_WINSOCK && SU_HAVE_IN6
/** Return a pointer to the in6addr_any. */
struct in_addr6 const *su_in6addr_any(void)
{
  static const struct in_addr6 a = SU_IN6ADDR_ANY_INIT;
  return &a;
}

/** Return a pointer to IPv6 loopback address */
struct in_addr6 const *su_in6addr_loopback(void)
{
  static const struct in_addr6 a = SU_IN6ADDR_LOOPBACK_INIT;
  return &a;
}
#endif

#if SU_HAVE_WINSOCK || DOCUMENTATION_ONLY

/** Call send() with POSIX-compatible signature */
ssize_t su_send(su_socket_t s, void *buffer, size_t length, int flags)
{
  if (length > INT_MAX)
    length = INT_MAX;
  return (ssize_t)send(s, buffer, (int)length, flags);
}

/** Call sendto() with POSIX-compatible signature */
ssize_t su_sendto(su_socket_t s, void *buffer, size_t length, int flags,
		   su_sockaddr_t const *to, socklen_t tolen)
{
  if (length > INT_MAX)
    length = INT_MAX;
  return (ssize_t)sendto(s, buffer, (int)length, flags,
			 &to->su_sa, (int) tolen);
}

/** Call recv() with POSIX-compatible signature */
ssize_t su_recv(su_socket_t s, void *buffer, size_t length, int flags)
{
  if (length > INT_MAX)
    length = INT_MAX;

  return (ssize_t)recv(s, buffer, (int)length, flags);
}

/** Call recvfrom() with POSIX-compatible signature */
ssize_t su_recvfrom(su_socket_t s, void *buffer, size_t length, int flags,
		    su_sockaddr_t *from, socklen_t *fromlen)
{
  int retval, ilen = 0;

  if (fromlen)
    ilen = *fromlen;

  if (length > INT_MAX)
    length = INT_MAX;

  retval = recvfrom(s, buffer, (int)length, flags,
		    &from->su_sa, fromlen ? &ilen : NULL);

  if (fromlen)
    *fromlen = ilen;

  return (ssize_t)retval;
}

/** Scatter/gather send */
issize_t su_vsend(su_socket_t s,
		  su_iovec_t const iov[], isize_t iovlen, int flags,
		  su_sockaddr_t const *su, socklen_t sulen)
{
  int ret;
  DWORD bytes_sent = (DWORD)su_failure;

  ret = WSASendTo(s,
		  (LPWSABUF)iov,
		  (DWORD)iovlen,
		  &bytes_sent,
		  flags,
		  &su->su_sa,
		  sulen,
		  NULL,
		  NULL);
  if (ret < 0)
    return (issize_t)ret;
  else
    return (issize_t)bytes_sent;
}


/** Scatter/gather recv */
issize_t su_vrecv(su_socket_t s, su_iovec_t iov[], isize_t iovlen, int flags,
		  su_sockaddr_t *su, socklen_t *sulen)
{
  int ret;
  DWORD bytes_recv = (DWORD)su_failure;
  DWORD dflags = flags;
  int fromlen = sulen ? *sulen : 0;

  ret =  WSARecvFrom(s,
		     (LPWSABUF)iov,
		     (DWORD)iovlen,
		     &bytes_recv,
		     &dflags,
		     &su->su_sa,
		     sulen ? &fromlen : NULL,
		     NULL,
		     NULL);

  if (sulen) *sulen = fromlen;

  if (ret < 0)
    return (issize_t)ret;
  else
    return (issize_t)bytes_recv;
}


#else

issize_t su_vsend(su_socket_t s,
		  su_iovec_t const iov[], isize_t iovlen, int flags,
		  su_sockaddr_t const *su, socklen_t sulen)
{
  struct msghdr hdr[1] = {{0}};
  int rv;

  hdr->msg_name = (void *)su;
  hdr->msg_namelen = sulen;
  hdr->msg_iov = (struct iovec *)iov;
  hdr->msg_iovlen = iovlen;

  do {
	  if ((rv = sendmsg(s, hdr, flags)) == -1) {
		  if (errno == EAGAIN) usleep(1000);
	  }
  } while (rv == -1 && (errno == EAGAIN || errno == EINTR));

  return rv;
}

issize_t su_vrecv(su_socket_t s, su_iovec_t iov[], isize_t iovlen, int flags,
		  su_sockaddr_t *su, socklen_t *sulen)
{
  struct msghdr hdr[1] = {{0}};
  issize_t retval;

  hdr->msg_name = (void *)su;
  if (su && sulen)
    hdr->msg_namelen = *sulen;
  hdr->msg_iov = (struct iovec *)iov;
  hdr->msg_iovlen = iovlen;

  do {
	  retval = recvmsg(s, hdr, flags);
  } while (retval == -1 && errno == EINTR);

  if (su && sulen)
    *sulen = hdr->msg_namelen;

  return retval;
}

#endif

/** Compare two socket addresses */
int su_cmp_sockaddr(su_sockaddr_t const *a, su_sockaddr_t const *b)
{
  int rv;

  /* Check that a and b are non-NULL */
  if ((rv = (a != NULL) - (b != NULL)) || a == NULL /* && b == NULL */)
    return rv;

  if ((rv = a->su_family - b->su_family))
    return rv;

  if (a->su_family == AF_INET)
    rv = memcmp(&a->su_sin.sin_addr, &b->su_sin.sin_addr,
		sizeof(struct in_addr));
#if SU_HAVE_IN6
  else if (a->su_family == AF_INET6)
    rv = memcmp(&a->su_sin6.sin6_addr, &b->su_sin6.sin6_addr,
		sizeof(struct in6_addr));
#endif
  else
    rv = memcmp(a, b, sizeof(struct sockaddr));

  if (rv)
    return rv;

  return a->su_port - b->su_port;
}

/** Check if socket address b match with a.
 *
 * The function su_match_sockaddr() returns true if the socket address @a b
 * matches with the socket address @a a. This happens if either all the
 * interesting fields are identical: address family, port number, address,
 * and scope ID (in case of IPv6) or that the @a a contains a wildcard
 * (zero) in their place.
 */
int su_match_sockaddr(su_sockaddr_t const *a, su_sockaddr_t const *b)
{
  /* Check that a and b are non-NULL */
  if (a == NULL)
    return 1;
  if (b == NULL)
    return 0;

  if (a->su_family != 0 && a->su_family != b->su_family)
    return 0;

  if (a->su_family == 0 || SU_SOCKADDR_INADDR_ANY(a))
    ;
  else if (a->su_family == AF_INET) {
    if (memcmp(&a->su_sin.sin_addr, &b->su_sin.sin_addr,
	       sizeof(struct in_addr)))
      return 0;
  }
#if SU_HAVE_IN6
  else if (a->su_family == AF_INET6) {
    if (a->su_scope_id != 0 && a->su_scope_id != b->su_scope_id)
      return 0;
    if (memcmp(&a->su_sin6.sin6_addr, &b->su_sin6.sin6_addr,
	       sizeof(struct in6_addr)))
      return 0;
  }
#endif
  else if (memcmp(a, b, sizeof(struct sockaddr)))
    return 0;

  if (a->su_port == 0)
    return 1;

  return a->su_port == b->su_port;
}

/** Convert mapped/compat address to IPv4 address */
void su_canonize_sockaddr(su_sockaddr_t *su)
{
#if SU_HAVE_IN6
  if (su->su_family != AF_INET6)
    return;

  if (!IN6_IS_ADDR_V4MAPPED(&su->su_sin6.sin6_addr) &&
      !IN6_IS_ADDR_V4COMPAT(&su->su_sin6.sin6_addr))
    return;

  su->su_family = AF_INET;
  su->su_array32[1] = su->su_array32[5];
  su->su_array32[2] = 0;
  su->su_array32[3] = 0;
#endif
}

