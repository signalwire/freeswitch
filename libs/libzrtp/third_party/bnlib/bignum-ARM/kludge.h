#ifndef KLUDGE_H
#define KLUDGE_H

/*
 * Kludges for not-quite-ANSI systems.
 * This should always be the last file included, because it may
 * mess up some system header files.
 */

/*
 * Some compilers complain about #if FOO if FOO isn't defined,
 * so do the ANSI-mandated thing explicitly...
 */
#ifndef ASSERT_NEEDS_STDIO
#define ASSERT_NEEDS_STDIO 0
#endif
#ifndef ASSERT_NEEDS_STDLIB
#define ASSERT_NEEDS_STDLIB 0
#endif
#ifndef NO_STDLIB_H
#define NO_STDLIB_H 0
#endif

/* SunOS 4.1.x <assert.h> needs "stderr" defined, and "exit" declared... */
#ifdef assert
#if ASSERT_NEEDS_STDIO
#include <stdio.h>
#endif
#if ASSERT_NEEDS_STDLIB
#if !NO_STDLIB_H
#include <stdlib.h>
#endif
#endif
#endif

#ifndef NO_MEMMOVE
#define NO_MEMMOVE 0
#endif
#if NO_MEMMOVE	/* memove() not in libraries */
#define memmove(dest,src,len) bcopy(src,dest,len)
#endif

#ifndef NO_MEMCPY
#define NO_MEMCPY 0
#endif
#if NO_MEMCPY	/* memcpy() not in libraries */
#define memcpy(dest,src,len) bcopy(src,dest,len)
#endif

#ifndef MEM_PROTOS_BROKEN
#define MEM_PROTOS_BROKEN 0
#endif
#if MEM_PROTOS_BROKEN
#define memcpy(d,s,l) memcpy((void *)(d), (void const *)(s), l)
#define memmove(d,s,l) memmove((void *)(d), (void const *)(s), l)
#define memcmp(d,s,l) memcmp((void const *)(d), (void const *)(s), l)
#define memset(d,v,l) memset((void *)(d), v, l)
#endif

/*
 * If there are no prototypes for the stdio functions, use these to
 * reduce compiler warnings.  Uses EOF as a giveaway to indicate
 * that <stdio.h> was #included.
 */
#ifndef NO_STDIO_PROTOS
#define NO_STDIO_PROTOS 0
#endif
#if NO_STDIO_PROTOS	/* Missing prototypes for "simple" functions */
#ifdef EOF
#ifdef __cplusplus
extern "C" {
#endif
int (puts)(char const *);
int (fputs)(char const *, FILE *);
int (fflush)(FILE *);
int (printf)(char const *, ...);
int (fprintf)(FILE *, char const *, ...);
/* If we have a sufficiently old-fashioned stdio, it probably uses these... */
int (_flsbuf)(int, FILE *);
int (_filbuf)(FILE *);
#ifdef __cplusplus
}
#endif
#endif /* EOF */
#endif /* NO_STDIO_PROTOS */

/*
 * Borland C seems to think that it's a bad idea to decleare a
 * structure tag and not declare the contents.  I happen to think
 * it's a *good* idea to use such "opaque" structures wherever
 * possible.  So shut up.
 */
#ifdef __BORLANDC__
#pragma warn -stu
#ifndef MSDOS
#define MSDOS 1
#endif
#endif

/* Turn off warning about negation of unsigned values */
#ifdef _MSC_VER
#pragma warning(disable:4146)
#endif

/* Cope with people forgetting to define the OS, if possible... */
#ifndef MSDOS
#ifdef __MSDOS
#define MSDOS 1
#endif
#endif
#ifndef MSDOS
#ifdef __MSDOS__
#define MSDOS 1
#endif
#endif

/* By MS-DOS, we mean 16-bit brain-dead MS-DOS.  Not GCC & GO32 */
#ifdef __GO32
#undef MSDOS
#endif
#ifdef __GO32__
#undef MSDOS
#endif

#endif /* KLUDGE_H */
