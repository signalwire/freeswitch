/*
 * Copyright (c) 1995  Colin Plumb.  All rights reserved.
 * For licensing and other legal details, see the file legal.c.
 */
#ifndef CPUTIME_H
#define CPUTIME_H

/*
 * Figure out what clock to use.  Each possibility can be specifically
 * enabled or disabled by predefining USE_XXX to 1 or 0.  For some,
 * the code attempts to detect availability automatically.  If the
 * Symbols HAVE_XXX are defined, they are used.  If not, they are
 * set to reasonable default assumptions while further conditions
 * are checked.  The choices, and the ways they are auto-detected are:
 * - gethrvtime(), if HAVE_GETHRVTIME is set to 1.
 * - clock_gettime(CLOCK_VIRTUAL,...), if CLOCK_VIRTUAL is defined in <time.h>
 * - getrusage(RUSAGE_SELF,...), if RUSAGE_SELF is defined in <sys/resource.h>
 * - clock(), if CLOCKS_PER_SEC or CLK_TCK are defined in <time.h>
 * - time(), unless specifically disabled.
 *
 * The symbol CLOCK_AVAIL is given a value of 1 if a clock is found.
 * The following are then available:
 * timetype (typedef): the type needed to hold a clock value.
 * gettime(t) (macro): A function that gets passed a timetype *.
 * subtime(d,s) (macro): Sets d -= s, essentially.
 * msec(t) (macro): Given a timetype, return the number of milliseconds
 *	in it, as an unsigned integer between 0 and 999.
 * sec(t) (macro): Given a timetype, return the number of seconds in it,
 *	as an unsigned long integer.
 *
 * This is written to accomocate a number of crufy old preprocessors that:
 * - Emit annoying warnings if you use "#if NOT_DEFINED".
 *   (Workaround: #ifndef FOO / #define FOO 0 / #endif)
 * - Emit annoying warnings if you #undef something not defined.
 *   (Workaround: #ifdef FOO / #undef FOO / #endif)
 * - Don't like spaces in "# define" and the like.
 *   (Workaround: harder-to-read code with no indentation.)
 */

/* We expect that our caller has already #included "bnconfig.h" if possible. */

#ifndef unix
#define unix 0
#endif
#ifndef __unix
#define __unix 0
#endif
#ifndef __unix__
#define __unix__ 0
#endif

#ifdef UNIX
/* Nothing */
#elif unix
#define UNIX 1
#elif __unix
#define UNIX 1
#elif __unix__
#define UNIX 1
#endif

#ifndef UNIX
#define UNIX 0
#endif

#ifndef TIME_WITH_SYS_TIME
#define TIME_WITH_SYS_TIME 1	/* Assume true if not told */
#endif
#ifndef HAVE_SYS_TIME_H
#define HAVE_SYS_TIME_H 1	/* Assume true if not told */
#endif

/*
 * Include <time.h> unless that would prevent us from later including
 * <sys/time.h>, in which case include *that* immediately.
 */
#if TIME_WITH_SYS_TIME
#include <time.h>
#elif HAVE_SYS_TIME_H
#include <sys/time.h>
#else
#include <time.h>
#endif

/* Do we want to use gethrvtime() (a Solaris special?) */
#ifndef USE_GETHRVTIME
#ifdef HAVE_GETHRVTIME
#define USE_GETHRVTIME HAVE_GETHRVTIME
#else
#define USE_GETHRVTIME 0
#endif
#endif

/* If we do want to use gethrvtime(), define the functions */
#if USE_GETHRVTIME
#define CLOCK_AVAIL 1
typedef hrtime_t timetype;
#define gettime(t) *(t) = gethrvtime()
#define subtime(d,s) d -= s
#define msec(t) (unsigned)((t/1000000)%1000)
#define sec(t) (unsigned long)(t/1000000000)

#else /* !USE_GETHRVTIME, extends to end of file */

/* Do we want to use clock_gettime()? */
#ifndef USE_CLOCK_GETTIME
#ifndef HAVE_CLOCK_GETTIME
#define HAVE_CLOCK_GETTIME 1	/* Assume the CLOCK_VIRTUAL test will catch */
#endif
/*
 * It turns out to be non-ANSI to use the apparently simpler construct
 * "#define USE_CLOCK_GETTIME defined(CLOCK_VIRTUAL)", since
 * "If the token defined is generated as a result of this replacement
 *  process or use of the defined unary operator does not match one
 *  of the two specified forms prior ro macro replacement, the behaviour
 *  is undefined."  (ANSI/ISO 9899-1990 section 6.8.1)
 * In practice, it breaks the DEC Alpha compiler.
 */
#if HAVE_CLOCK_GETTIME
#ifdef CLOCK_VIRTUAL
#define USE_CLOCK_GETTIME 1
#endif
#endif
#endif

/* If we do want to use clock_gettime(), define the necessary functions */
#if USE_CLOCK_GETTIME
#define CLOCK_AVAIL 1
typedef struct timespec timetype;
#define gettime(t) clock_gettime(CLOCK_VIRTUAL, t)
#define subtime(d,s) \
	d.tv_sec -= s.tv_sec + (d.tv_nsec >= s.tv_nsec ? \
	                        (d.tv_nsec -= s.tv_nsec, 0) : \
	                        (d.tv_nsec += 1000000000-s.tv_nsec, 1))
#define msec(t) (unsigned)(t.tv_nsec/1000000)
#define sec(t) (unsigned long)(t.tv_sec)

#else /* !USE_CLOCK_GETTIME, extends to end of file */

#if UNIX
#ifndef HAVE_GETRUSAGE
#define HAVE_GETRUSAGE 1
#endif
#endif /* UNIX */

/* Do we want to use getrusage()? */
#if HAVE_GETRUSAGE
#if TIME_WITH_SYS_TIME
#ifndef HAVE_SYS_TIME_H	/* If it's not defined */
#include <sys/time.h>
#elif HAVE_SYS_TIME_H	/* Or it's defined true */
#include <sys/time.h>
#endif
#endif /* TIME_WITH_SYS_TIME */
#include <sys/resource.h>

#ifdef RUSAGE_SELF
#undef USE_GETRUSAGE
#define USE_GETRUSAGE 1
#endif
#endif /* HAVE_GETRUSAGE */

/* If we do want to use getrusage(), define the necessary functions */
#if USE_GETRUSAGE
#define CLOCK_AVAIL 1
typedef struct rusage timetype;
#define gettime(t) getrusage(RUSAGE_SELF, t);
#define subtime(d, s) \
	d.ru_utime.tv_sec -= s.ru_utime.tv_sec + \
	             (d.ru_utime.tv_usec >= s.ru_utime.tv_usec ? \
	              (d.ru_utime.tv_usec -= s.ru_utime.tv_usec, 0) : \
	              (d.ru_utime.tv_usec += 1000000-s.ru_utime.tv_usec, 1))
#define msec(t) (unsigned)(t.ru_utime.tv_usec/1000)
#define sec(t) (unsigned long)(t.ru_utime.tv_sec)

#else /* !USE_GETRUSAGE, extends to end of file */

#ifndef HAVE_CLOCK
#define HAVE_CLOCK 1
#endif

#if HAVE_CLOCK
#ifndef CLOCKS_PER_SEC
#ifdef CLK_TCK
#define CLOCKS_PER_SEC CLK_TCK
#endif
#endif /* !defined(CLOCKS_PER_SEC) */

#ifndef USE_CLOCK
#ifdef CLOCKS_PER_SEC
#define USE_CLOCK 1
#endif
#endif /* !defined(USE_CLOCK) */
#endif /* HAVE_CLOCK */

/* If we want to use clock(), define the necessary functions */
#if USE_CLOCK
#define CLOCK_AVAIL 1
typedef clock_t timetype;
#define gettime(t) *(t) = clock()
#define subtime(d, s) d -= s
/*
 * I don't like having to do floating point math.  CLOCKS_PER_SEC is
 * almost always an integer, and the most common non-integral case is
 * the MS-DOS wierdness of 18.2.  We have to be a bit careful with the
 * casts, because ANSI C doesn't provide % with non-integral operands,
 * but just to be extra annoying, some implementations define it as an
 * integral-valued float.  (E.g. Borland C++ 4.5 with 1000.0)
 */
#if ((unsigned)CLOCKS_PER_SEC == CLOCKS_PER_SEC)
	/* Integer CLOCKS_PER_SEC */

#define sec(t) (unsigned long)(t/CLOCKS_PER_SEC)
#define msec(t) (unsigned)(t % (unsigned)CLOCKS_PER_SEC * 1000 / \
					(unsigned)CLOCKS_PER_SEC)
#elif (CLOCKS_PER_SEC == 18.2)
	/* MS-DOS-ism */

#define sec(t) (unsigned long)(t*5 / 91)
#define msec(t) (unsigned)(t*5 % 91 * 1000 / 91)

#else /* We are forced to muck with floating point.... */

#include <math.h>	/* For floor() */
#define sec(t) (unsigned long)(t/CLOCKS_PER_SEC)
#define msec(t) (unsigned)((t - sec(t)*CLOCKS_PER_SEC) * 1000 / CLOCKS_PER_SEC)

#endif

#else /* !USE_CLOCK, extends to end of file */

#ifndef HAVE_TIME
#define HAVE_TIME 1
#endif

#if HAVE_TIME
#ifndef USE_TIME
#define USE_TIME 1
#endif
#endif

#if USE_TIME
#define CLOCK_AVAIL 1
typedef time_t timetype;
#define gettime(t) time(t)
#define subtime(d, s) d -= s
#define msec(t) (unsigned)0
#define sec(t) (unsigned long)t

#else /* !USE_TIME, extends to end of file */

#error No clock available.

#endif /* USE_TIME */
#endif /* USE_CLOCK */
#endif /* USE_GETRUSAGE */
#endif /* USE_CLOCK_GETTIME */
#endif /* USE_GETHRVTIME */

#endif /*CPUTIME_H*/
