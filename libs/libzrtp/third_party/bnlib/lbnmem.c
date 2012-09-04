/*
 * Copyright (c) 1995  Colin Plumb.  All rights reserved.
 * For licensing and other legal details, see the file legal.c.
 *
 * lbnmem.c - low-level bignum memory handling.
 *
 * Note that in all cases, the pointers passed around
 * are pointers to the *least* significant end of the word.
 * On big-endian machines, these are pointers to the *end*
 * of the allocated range.
 *
 * BNSECURE is a simple level of security; for more security
 * change these function to use locked unswappable memory.
 */
#ifndef HAVE_CONFIG_H
#define HAVE_CONFIG_H 0
#endif
#if HAVE_CONFIG_H
#include "bnconfig.h"
#endif

/*
 * Some compilers complain about #if FOO if FOO isn't defined,
 * so do the ANSI-mandated thing explicitly...
 */
#ifndef NO_STDLIB_H
#define NO_STDLIB_H 0
#endif
#ifndef NO_STRING_H
#define NO_STRING_H 0
#endif
#ifndef HAVE_STRINGS_H
#define HAVE_STRINGS_H 0
#endif

#if !NO_STDLIB_H
#include <stdlib.h>	/* For malloc() & co. */
#else
void *malloc();
void *realloc();
void free();
#endif

#if !NO_STRING_H
#include <string.h>	/* For memset */
#elif HAVE_STRINGS_H
#include <strings.h>
#endif

#ifndef DBMALLOC
#define DBMALLOC 0
#endif
#if DBMALLOC
/* Development debugging */
#include "../dbmalloc/malloc.h"
#endif

#include "lbn.h"
#include "lbnmem.h"

#include "kludge.h"

#include "zrtp.h"

#ifndef lbnMemWipe
void
lbnMemWipe(void *ptr, unsigned bytes)
{
	zrtp_memset(ptr, 0, bytes);
}
#define lbnMemWipe(ptr, bytes) memset(ptr, 0, bytes)
#endif

#ifndef lbnMemAlloc
void *
lbnMemAlloc(unsigned bytes)
{
	return zrtp_sys_alloc(bytes);
}
#endif

#ifndef lbnMemFree
void
lbnMemFree(void *ptr, unsigned bytes)
{
	lbnMemWipe(ptr, bytes);
	zrtp_sys_free(ptr);
}
#endif

#ifndef lbnRealloc
#if defined(lbnMemRealloc) || !BNSECURE
void *
lbnRealloc(void *ptr, unsigned oldbytes, unsigned newbytes)
{
	if (ptr) {
		BIG(ptr = (char *)ptr - oldbytes;)
		if (newbytes < oldbytes)
			memmove(ptr, (char *)ptr + oldbytes-newbytes, oldbytes);
	}
#ifdef lbnMemRealloc
	ptr = lbnMemRealloc(ptr, oldbytes, newbytes);
#else
	ptr = realloc(ptr, newbytes);
#endif
	if (ptr) {
		if (newbytes > oldbytes)
			memmove((char *)ptr + newbytes-oldbytes, ptr, oldbytes);
		BIG(ptr = (char *)ptr + newbytes;)
	}

	return ptr;
}

#else /* BNSECURE */

void *
lbnRealloc(void *oldptr, unsigned oldbytes, unsigned newbytes)
{
	void *newptr = lbnMemAlloc(newbytes);

	if (!newptr)
		return newptr;
	if (!oldptr)
		return BIGLITTLE((char *)newptr+newbytes, newptr);

	/*
	 * The following copies are a bit non-obvious in the big-endian case
	 * because one of the pointers points to the *end* of allocated memory.
	 */
	if (newbytes > oldbytes) {	/* Copy all of old into part of new */
		BIG(newptr = (char *)newptr + newbytes;)
		BIG(oldptr = (char *)oldptr - oldbytes;)
		memcpy(BIGLITTLE((char *)newptr-oldbytes, newptr), oldptr,
		       oldbytes);
	} else {	/* Copy part of old into all of new */
		memcpy(newptr, BIGLITTLE((char *)oldptr-newbytes, oldptr),
		       newbytes);
		BIG(newptr = (char *)newptr + newbytes;)
		BIG(oldptr = (char *)oldptr - oldbytes;)
	}

	lbnMemFree(oldptr, oldbytes);

	return newptr;
}
#endif /* BNSECURE */
#endif /* !lbnRealloc */
