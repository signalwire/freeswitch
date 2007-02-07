/* Copyright (C) 1994,1996,1997,1998,1999,2001,2002
   Free Software Foundation, Inc.
   This file is part of the GNU C Library.

   The GNU C Library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 2.1 of the License, or (at your option) any later version.

   The GNU C Library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with the GNU C Library; if not, write to the Free
   Software Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
   02111-1307 USA.  */

#include "config.h"

#if !HAVE_POLL && HAVE_SELECT && HAVE_POLL_PORT

#include "sofia-sip/su.h"


//extern __typeof(poll) __libc_poll;

#if 1

#include <alloca.h>
#include <string.h>

#include "sofia-sip/su_wait.h"

#ifndef __NFDBITS
#define	NBBY		8				/* bits in a byte */
#define __NFDBITS	(sizeof(__int32_t) * NBBY)		/* bits per mask */
#endif


#undef howmany

#ifndef howmany
#define        howmany(x, y)   (((x)+((y)-1))/(y)) * sizeof(uint32_t)
#endif

//libc_hidden_proto(memcpy)
//libc_hidden_proto(memset)
//libc_hidden_proto(getdtablesize)
//libc_hidden_proto(select)

/* uClinux 2.0 doesn't have poll, emulate it using select */

/* Poll the file descriptors described by the NFDS structures starting at
   FDS.  If TIMEOUT is nonzero and not -1, allow TIMEOUT milliseconds for
   an event to occur; if TIMEOUT is -1, block until an event occurs.
   Returns the number of file descriptors with events, zero if timed out,
   or -1 for errors.  */

int poll(struct pollfd *fds, nfds_t nfds, int timeout)
{
    struct timeval tv;
#if HAVE_ALLOCA_H
    static int max_fd_size;
    int bytes;
    fd_set *rset, *wset, *xset;
#else
    fd_set rset[1], wset[1], xset[1];
#endif
    struct pollfd *f;
    int ready;
    int maxfd = 0;

#if HAVE_ALLOCA_H
    if (!max_fd_size)
	max_fd_size = getdtablesize ();

    bytes = howmany (max_fd_size, __NFDBITS);

    rset = alloca (bytes);
    wset = alloca (bytes);
    xset = alloca (bytes);

    /* We can't call FD_ZERO, since FD_ZERO only works with sets
       of exactly __FD_SETSIZE size.  */
    memset (rset, 0, bytes);
    memset (wset, 0, bytes);
    memset (xset, 0, bytes);
#else
    FD_ZERO(rset);
    FD_ZERO(wset);
    FD_ZERO(xset);
#endif


    for (f = fds; f < &fds[nfds]; ++f)
    {
	f->revents = 0;
	if (f->fd >= 0)
	{
#if HAVE_ALLOCA_H
	    if (f->fd >= max_fd_size)
	    {
		/* The user provides a file descriptor number which is higher
		   than the maximum we got from the `getdtablesize' call.
		   Maybe this is ok so enlarge the arrays.  */
		fd_set *nrset, *nwset, *nxset;
		int nbytes;

		max_fd_size = roundup (f->fd, __NFDBITS);
		nbytes = howmany (max_fd_size, __NFDBITS);

		nrset = alloca (nbytes);
		nwset = alloca (nbytes);
		nxset = alloca (nbytes);

		memset ((char *) nrset + bytes, 0, nbytes - bytes);
		memset ((char *) nwset + bytes, 0, nbytes - bytes);
		memset ((char *) nxset + bytes, 0, nbytes - bytes);

		rset = memcpy (nrset, rset, bytes);
		wset = memcpy (nwset, wset, bytes);
		xset = memcpy (nxset, xset, bytes);

		bytes = nbytes;
	    }
#else
	    if (f->fd >= FD_SETSIZE) {
	      errno = EBADF;
	      return -1;
	    }
#endif /* HAVE_ALLOCA_H */

	    if (f->events & POLLIN)
		FD_SET (f->fd, rset);
	    if (f->events & POLLOUT)
		FD_SET (f->fd, wset);
	    if (f->events & POLLPRI)
		FD_SET (f->fd, xset);
	    if (f->fd > maxfd && (f->events & (POLLIN|POLLOUT|POLLPRI)))
		maxfd = f->fd;
	}
    }

    tv.tv_sec = timeout / 1000;
    tv.tv_usec = (timeout % 1000) * 1000;

    while (1)
    {
	ready = select (maxfd + 1, rset, wset, xset,
		timeout == -1 ? NULL : &tv);

	/* It might be that one or more of the file descriptors is invalid.
	   We now try to find and mark them and then try again.  */
	if (ready == -1 && errno == EBADF)
	{
	    struct timeval sngl_tv;
#if HAVE_ALLOCA_H
	    fd_set *sngl_rset = alloca (bytes);
	    fd_set *sngl_wset = alloca (bytes);
	    fd_set *sngl_xset = alloca (bytes);

	    /* Clear the original set.  */
	    memset (rset, 0, bytes);
	    memset (wset, 0, bytes);
	    memset (xset, 0, bytes);
#else
	    fd_set sngl_rset[1];
	    fd_set sngl_wset[1];
	    fd_set sngl_xset[1];

	    FD_ZERO(rset);
	    FD_ZERO(wset);
	    FD_ZERO(xset);
#endif

	    /* This means we don't wait for input.  */
	    sngl_tv.tv_sec = 0;
	    sngl_tv.tv_usec = 0;

	    maxfd = -1;

	    /* Reset the return value.  */
	    ready = 0;

	    for (f = fds; f < &fds[nfds]; ++f)
		if (f->fd != -1 && (f->events & (POLLIN|POLLOUT|POLLPRI))
			&& (f->revents & POLLNVAL) == 0)
		{
		    int n;

#if HAVE_ALLOCA_H
		    memset (sngl_rset, 0, bytes);
		    memset (sngl_wset, 0, bytes);
		    memset (sngl_xset, 0, bytes);
#else
		    FD_ZERO(rset);
		    FD_ZERO(wset);
		    FD_ZERO(xset);
#endif

		    if (f->events & POLLIN)
			FD_SET (f->fd, sngl_rset);
		    if (f->events & POLLOUT)
			FD_SET (f->fd, sngl_wset);
		    if (f->events & POLLPRI)
			FD_SET (f->fd, sngl_xset);

		    n = select (f->fd + 1, sngl_rset, sngl_wset, sngl_xset,
			    &sngl_tv);
		    if (n != -1)
		    {
			/* This descriptor is ok.  */
			if (f->events & POLLIN)
			    FD_SET (f->fd, rset);
			if (f->events & POLLOUT)
			    FD_SET (f->fd, wset);
			if (f->events & POLLPRI)
			    FD_SET (f->fd, xset);
			if (f->fd > maxfd)
			    maxfd = f->fd;
			if (n > 0)
			    /* Count it as being available.  */
			    ++ready;
		    }
		    else if (errno == EBADF)
			f->revents |= POLLNVAL;
		}
	    /* Try again.  */
	    continue;
	}

	break;
    }

    if (ready > 0) 
	for (f = fds; f < &fds[nfds]; ++f)
	{
	    if (f->fd >= 0)
	    {
		if (FD_ISSET (f->fd, rset))
		    f->revents |= POLLIN;
		if (FD_ISSET (f->fd, wset))
		    f->revents |= POLLOUT;
		if (FD_ISSET (f->fd, xset))
		    f->revents |= POLLPRI;
	    }
	}

    return ready;
}

#endif
#endif
