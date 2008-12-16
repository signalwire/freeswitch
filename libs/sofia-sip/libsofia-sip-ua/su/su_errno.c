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

/**@CFILE su_errno.c errno compatibility
 *
 * @author Pekka Pessi <Pekka.Pessi@nokia.com>
 *
 * @date Original: Thu Mar 18 19:40:51 1999 pessi
 * @date Split to su_errno.c: Thu Dec 22 18:37:02 EET 2005 pessi
 */

#include "config.h"

#include <sofia-sip/su_errno.h>
#include <sofia-sip/su.h>

#include <string.h>

#if SU_HAVE_WINSOCK

#include <stdio.h>

/** Get the latest socket error. */
int su_errno(void)
{
  return WSAGetLastError();
}

/** Set the socket error. */
int su_seterrno(int errcode)
{
  WSASetLastError(errcode);

  return -1;
}

const char *su_strerror(int errcode)
{
  struct errmsg { int no; const char *msg; };
  static struct errmsg *msgp;
  static struct errmsg msgs[] = {
    { 0, "Success" },
    { WSAEINTR, "Interrupted system call" },
    { WSAEBADF, "Bad file descriptor" },
    { WSAEACCES, "Permission denied" },
    { WSAEFAULT, "Bad address" },
    { WSAEINVAL, "Invalid argument" },
    { WSAEMFILE, "Too many open files" },
    { WSAEWOULDBLOCK, "Another winsock call while a "
      "blocking function was in progress" },
    { WSAEINPROGRESS, "Operation now in progress" },
    { WSAEALREADY, "Operation already in progress" },
    { WSAENOTSOCK, "Socket operation on non-socket" },
    { WSAEDESTADDRREQ, "Destination address required" },
    { WSAEMSGSIZE, "Message too long" },
    { WSAEPROTOTYPE, "Protocol wrong type for socket" },
    { WSAENOPROTOOPT, "Protocol not available" },
    { WSAEPROTONOSUPPORT, "Protocol not supported" },
    { WSAESOCKTNOSUPPORT, "Socket type not supported" },
    { WSAEOPNOTSUPP, "Operation not supported" },
    { WSAEPFNOSUPPORT, "Protocol family not supported" },
    { WSAEAFNOSUPPORT, "Address family not supported" },
    { WSAEADDRINUSE, "Address already in use" },
    { WSAEADDRNOTAVAIL, "Can't assign requested address" },
    { WSAENETDOWN, "Network is down" },
    { WSAENETUNREACH, "Network is unreachable" },
    { WSAENETRESET, "Network dropped connection on reset" },
    { WSAECONNABORTED, "Software caused connection abort" },
    { WSAECONNRESET, "Connection reset by peer" },
    { WSAENOBUFS, "No buffer space available" },
    { WSAEISCONN, "Socket is already connected" },
    { WSAENOTCONN, "Socket is not connected" },
    { WSAESHUTDOWN, "Can't send after socket shutdown" },
    { WSAETOOMANYREFS, "Too many references: "
      "can't splice" },
    { WSAETIMEDOUT, "Operation timed out" },
    { WSAECONNREFUSED, "Connection refused" },
    { WSAELOOP, "Too many levels of symbolic links" },
    { WSAENAMETOOLONG, "File name too long" },
    { WSAEHOSTDOWN, "Host is down" },
    { WSAEHOSTUNREACH, "No route to host" },
    { WSAENOTEMPTY, "Directory not empty" },
    { WSAEPROCLIM, "Too many processes" },
    { WSAEUSERS, "Too many users" },
    { WSAEDQUOT, "Disc quota exceeded" },
    { WSAESTALE, "Stale NFS file handle" },
    { WSAEREMOTE, "Too many levels of remote in path" },
    { WSASYSNOTREADY, "Network subsystem is unvailable" },
    { WSAVERNOTSUPPORTED, "WinSock version is not "
      "supported" },
    { WSANOTINITIALISED, "Successful WSAStartup() not yet "
      "performed" },
    { WSAEDISCON, "Graceful shutdown in progress" },
    /* Resolver errors */
    { WSAHOST_NOT_FOUND, "No such host is known" },
    { WSATRY_AGAIN, "Host not found, or server failed" },
    { WSANO_RECOVERY, "Unexpected server error "
      "encountered" },
    { WSANO_DATA, "Valid name without requested data" },
    { WSANO_ADDRESS, "No address, look for MX record" },
    { 0, NULL }
  };
  static struct errmsg sofia_msgs[] = {
    { EBADMSG, "Bad message" },
    { EPROTO, "Protocol error" },
    { 0, NULL }
  };
  static char buf[64];

  if (errcode < WSABASEERR)
    return strerror(errcode);

  if (errcode < 20000)
    for (msgp = msgs; msgp->msg; msgp++) {
      if (errcode == msgp->no) {
	return msgp->msg;
      }
    }
  else
    for (msgp = sofia_msgs; msgp->msg; msgp++) {
      if (errcode == msgp->no) {
	return msgp->msg;
      }
    }

  /* This can not overflow, but in other hand, it is not thread-safe */
  sprintf(buf, "winsock error %d", errcode);

  return buf;
}

#else

const char *su_strerror(int errcode)
{
  return strerror(errcode);
}

#endif /* SU_HAVE_WINSOCK */
