/*
 * Copyright (c) 1995  Colin Plumb.  All rights reserved.
 * For licensing and other legal details, see the file legal.c.
 */
#ifndef KLUDGE_H
#define KLUDGE_H

/*
 * Kludges for not-quite-ANSI systems.
 * This should always be the last file included, because it may
 * mess up some system header files.
 */

#if NO_MEMMOVE	/* memove() not in libraries */
#define memmove(dest,src,len) bcopy(src,dest,len)
#endif

#if NO_STRTOUL	/* strtoul() not in libraries */
#define strtoul strtol	/* Close enough */
#endif

#if NO_RAISE	/* raise() not in libraries */
#include <sys/types.h>	/* For getpid() - kill() is in <signal.h> */
#define raise(sig) kill(getpid(),sig)
#endif

/*
 * Make Microsoft Visual C shut the hell up about a few things...
 * Warning 4116 complains about the alignof() macro, saying:
 * warning C4116: unnamed type definition in parentheses
 * I do not know of a reasonable way to recode to eliminate this warning.
 * Warning 4761 complains about passing an expression (which has
 * type int) to a function expecting something narrower - like
 * a ringmask, if ringmask is set to 8 bits.  The error is:
 * warning C4761: integral size mismatch in argument : conversion supplied
 * I do not know of a reasonable way to recode to eliminate this warning.
 */
#ifdef _MSC_VER
#pragma warning(disable: 4116 4761)
#endif

/*
 * Borland C seems to think that it's a bad idea to decleare a
 * structure tag and not declare the contents.  I happen to think
 * it's a *good* idea to use such "opaque" structures wherever
 * possible.  So shut up.
 */
#ifdef __BORLANDC__
#pragma warn -stu
#endif

/* Cope with people forgetting to define the OS, if possible... */

#if !defined(MSDOS) && defined(__MSDOS__)
#define MSDOS 1
#endif

#if !defined(UNIX) && (defined(unix) || defined (__unix__))
#define UNIX 1
#endif


#endif /* KLUDGE_H */
