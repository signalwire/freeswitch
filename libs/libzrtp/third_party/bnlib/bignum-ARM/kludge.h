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
#ifndef NO_STDLIB_H
#define NO_STDLIB_H 0
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
