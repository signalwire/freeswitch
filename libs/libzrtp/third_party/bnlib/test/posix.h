/*
 * Copyright (c) 1995  Colin Plumb.  All rights reserved.
 * For licensing and other legal details, see the file legal.c.
 *
 * This file includes <unistd.h>, if it's available, and
 * declares a bunch of functions with "traditional" values if not.
 * The GNU Libc Manual (node "Version Supported") says this is impossible;
 * I wonder what they think of this.
 */

#include <limits.h>

/*
 * See if this is a POSIX <limits.h>.  A POSIX system *may* define
 * a macro for ARG_MAX, but it may instead defined _SC_ARG_MAX
 * in <unistd.h> and require you yo use sysconf() to get the value.
 * However, a POSIX system is supposed to defined _POSIX_ARG_MAX
 * in <limits.h> with the value of 4096, the POSIX-mandated lower
 * bound on ARG_MAX or sysconf(_SC_ARG_MAX).
 * A POSIX system is supposed to define most of these, so checking for
 * them *all* is overkill, but it's easy enough...
 */
#ifndef HAVE_UNISTD_H
#ifdef __POSIX__	/* Defined by GCC on POSIX systems */
#define HAVE_UNISTD_H 1
#elif defined(_POSIX_ARG_MAX) || defined(_POSIX_CHILD_MAX)
#define HAVE_UNISTD_H 1
#elif defined(_POSIX_LINK_MAX) || defined(_POSIX_MAX_CANON)
#define HAVE_UNISTD_H 1
#elif defined(_POSIX_MAX_INPUT) || defined(_POSIX_NAME_MAX)
#define HAVE_UNISTD_H 1
#elif defined(_POSIX_NGROUPS_MAX) || defined(_POSIX_OPEN_MAX)
#define HAVE_UNISTD_H 1
#elif defined(_POSIX_PATH_MAX) || defined(_POSIX_PIPE_BUF)
#define HAVE_UNISTD_H 1
#elif defined(_POSIX_RE_DUP_MAX) || defined(_POSIX_SSIZE_MAX)
#define HAVE_UNISTD_H 1
#elif defined(_POSIX_STREAM_MAX) || defined (_POSIX_TZNAME_MAX)
#define HAVE_UNISTD_H 1
#endif
#endif

#if HAVE_UNISTD_H
#include <unistd.h>
#elif defined(MSDOS)
#include <io.h>	/* Where MSDOS keeps such things */
#else
/* Not POSIX - declare the portions of <unistd.h> we need manually. */
int ioctl(int fd, int request, void *arg);
int isatty(int fd);
int read(int fd, void *buf, int nbytes);
unsigned sleep(unsigned seconds);
#endif
