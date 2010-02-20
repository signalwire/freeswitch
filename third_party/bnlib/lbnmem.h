/*
 * Operations on the usual buffers of bytes
 */
#ifndef BNSECURE
#define BNSECURE 1
#endif

/*
 * These operations act on buffers of memory, just like malloc & free.
 * One exception: it is not legal to pass a NULL pointer to lbnMemFree.
 */

#ifndef lbnMemAlloc
void *lbnMemAlloc(unsigned bytes);
#endif

#ifndef lbnMemFree
void lbnMemFree(void *ptr, unsigned bytes);
#endif

/* This wipes out a buffer of bytes if necessary needed. */

#ifndef lbnMemWipe
#if BNSECURE
void lbnMemWipe(void *ptr, unsigned bytes);
#else
#define lbnMemWipe(ptr, bytes) (void)(ptr,bytes)
#endif
#endif /* !lbnMemWipe */

/*
 * lbnRealloc is NOT like realloc(); it's endian-sensitive!
 * If lbnMemRealloc is #defined, lbnRealloc will be defined in terms of it.
 * It is legal to pass a NULL pointer to lbnRealloc, although oldbytes
 * will always be sero.
 */
#ifndef lbnRealloc
void *lbnRealloc(void *ptr, unsigned oldbytes, unsigned newbytes);
#endif


/*
 * These macros are the ones actually used most often in the math library.
 * They take and return pointers to the *end* of the given buffer, and
 * take sizes in terms of words, not bytes.
 *
 * Note that LBNALLOC takes the pointer as an argument instead of returning
 * the value.
 *
 * Note also that these macros are only useable if you have included
 * lbn.h (for the BIG and BIGLITTLE macros), which this file does NOT include.
 */

#define LBNALLOC(p,type,words) BIGLITTLE( \
	if ( ((p) = (type *)lbnMemAlloc((words)*sizeof*(p))) != 0) \
		(p) += (words), \
	(p) = (type *)lbnMemAlloc((words) * sizeof*(p)) \
	)
#define LBNFREE(p,words) lbnMemFree((p) BIG(-(words)), (words) * sizeof*(p))
#define LBNREALLOC(p,old,new) \
	lbnRealloc(p, (old) * sizeof*(p), (new) * sizeof*(p))
#define LBNWIPE(p,words) lbnMemWipe((p) BIG(-(words)), (words) * sizeof*(p))

