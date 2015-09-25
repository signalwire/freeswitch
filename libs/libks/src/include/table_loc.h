/*
 * local defines for the table module
 *
 * Copyright 2000 by Gray Watson.
 *
 * This file is part of the table package.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose and without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies,
 * and that the name of Gray Watson not be used in advertising or
 * publicity pertaining to distribution of the document or software
 * without specific, written prior permission.
 *
 * Gray Watson makes no representations about the suitability of the
 * software described herein for any purpose.  It is provided "as is"
 * without express or implied warranty.
 *
 * The author may be reached via http://256.com/gray/
 *
 * $Id: table_loc.h,v 1.11 2000/03/09 03:30:42 gray Exp $
 */

#ifndef __TABLE_LOC_H__
#define __TABLE_LOC_H__

#ifndef unix
#define NO_MMAP
#endif

#ifndef	BITSPERBYTE
#define BITSPERBYTE	8
#endif
#ifndef	BITS
#define BITS(type)	(BITSPERBYTE * (int)sizeof(type))
#endif

#define TABLE_MAGIC	0xBADF00D	/* very magic magicness */
#define LINEAR_MAGIC	0xAD00D00	/* magic value for linear struct */
#define DEFAULT_SIZE	1024		/* default table size */
#define MAX_ALIGNMENT	128		/* max alignment value */

/*
 * Maximum number of splits.  This should mean that these routines can
 * handle at least 2^128 different values (that's _quite_ a few).  And
 * then you can always increase the value.
 */
#define MAX_QSORT_SPLITS	128

/*
 * Maximum number of entries that must be in list for it to be
 * partitioned.  If there are fewer elements then just do our
 * insertion sort.
 */
#define MAX_QSORT_MANY		8

/*
 * Macros.
 */

/* returns 1 when we should grow or shrink the table */
#define SHOULD_TABLE_GROW(tab)	((tab)->ta_entry_n > (tab)->ta_bucket_n * 2)
#define SHOULD_TABLE_SHRINK(tab) ((tab)->ta_entry_n < (tab)->ta_bucket_n / 2)

/*
 * void HASH_MIX
 *
 * DESCRIPTION:
 *
 * Mix 3 32-bit values reversibly.  For every delta with one or two
 * bits set, and the deltas of all three high bits or all three low
 * bits, whether the original value of a,b,c is almost all zero or is
 * uniformly distributed.
 *
 * If HASH_MIX() is run forward or backward, at least 32 bits in a,b,c
 * have at least 1/4 probability of changing.  If mix() is run
 * forward, every bit of c will change between 1/3 and 2/3 of the
 * time.  (Well, 22/100 and 78/100 for some 2-bit deltas.)
 *
 * HASH_MIX() takes 36 machine instructions, but only 18 cycles on a
 * superscalar machine (like a Pentium or a Sparc).  No faster mixer
 * seems to work, that's the result of my brute-force search.  There
 * were about 2^68 hashes to choose from.  I only tested about a
 * billion of those.
 */
#define HASH_MIX(a, b, c)						\
	do {										\
		a -= b; a -= c; a ^= (c >> 13);			\
		b -= c; b -= a; b ^= (a << 8);			\
		c -= a; c -= b; c ^= (b >> 13);			\
		a -= b; a -= c; a ^= (c >> 12);			\
		b -= c; b -= a; b ^= (a << 16);			\
		c -= a; c -= b; c ^= (b >> 5);			\
		a -= b; a -= c; a ^= (c >> 3);			\
		b -= c; b -= a; b ^= (a << 10);			\
		c -= a; c -= b; c ^= (b >> 15);			\
	} while(0)

#define SET_POINTER(pnt, val)					\
	do {										\
		if ((pnt) != NULL) {					\
			(*(pnt)) = (val);					\
		}										\
	} while(0)

/*
 * The following macros take care of the mmap case.  When we are
 * mmaping a table from a disk file, all of the pointers in the table
 * structures are replaced with offsets into the file.  The following
 * macro, for each pointer, adds the starting address of the mmaped
 * section onto each pointer/offset turning it back into a legitimate
 * pointer.
 */
#ifdef NO_MMAP

#define TABLE_POINTER(table, type, pnt)		(pnt)

#else

#define TABLE_POINTER(tab_p, type, pnt)						\
	((tab_p)->ta_mmap == NULL || (pnt) == NULL ? (pnt) :	\
	 (type)((char *)((tab_p)->ta_mmap) + (long)(pnt)))

#endif

/*
 * Macros to get at the key and the data pointers
 */
#define ENTRY_KEY_BUF(entry_p)		((entry_p)->te_key_buf)
#define ENTRY_DATA_BUF(tab_p, entry_p)					\
	(ENTRY_KEY_BUF(entry_p) + (entry_p)->te_key_size)

/*
 * Table structures...
 */

/*
 * HACK: this should be equiv as the table_entry_t without the key_buf
 * char.  We use this with the ENTRY_SIZE() macro above which solves
 * the problem with the lack of the [0] GNU hack.  We use the
 * table_entry_t structure to better map the memory and make things
 * faster.
 */
typedef struct table_shell_st {
	unsigned int		te_key_size;	/* size of data */
	unsigned int		te_data_size;	/* size of data */
	struct table_shell_st	*te_next_p;	/* pointer to next in the list */
	/* NOTE: this does not have the te_key_buf field here */
} table_shell_t;

/*
 * Elements in the bucket linked-lists.  The key[1] is the start of
 * the key with the rest of the key and all of the data information
 * packed in memory directly after the end of this structure.
 *
 * NOTE: if this structure is changed, the table_shell_t must be
 * changed to match.
 */
typedef struct table_entry_st {
	unsigned int		te_key_size;	/* size of data */
	unsigned int		te_data_size;	/* size of data */
	struct table_entry_st	*te_next_p;	/* pointer to next in the list */
	unsigned char		te_key_buf[1];	/* 1st byte of key buf */
} table_entry_t;

/* external structure for debuggers be able to see void */
typedef table_entry_t	table_entry_ext_t;

/* main table structure */
typedef struct table_st {
	unsigned int		ta_magic;	/* magic number */
	unsigned int		ta_flags;	/* table's flags defined in table.h */
	unsigned int		ta_bucket_n;	/* num of buckets, should be 2^X */
	unsigned int		ta_entry_n;	/* num of entries in all buckets */
	unsigned int		ta_data_align;	/* data alignment value */
	table_entry_t		**ta_buckets;	/* array of linked lists */
	table_linear_t	ta_linear;	/* linear tracking */
	struct table_st	*ta_mmap;	/* mmaped table */
	unsigned long		ta_file_size;	/* size of on-disk space */
  
	void			*ta_mem_pool;	/* pointer to some memory pool */
	table_mem_alloc_t	ta_alloc_func;	/* memory allocation function */
	table_mem_resize_t	ta_resize_func;	/* memory resize function */
	table_mem_free_t	ta_free_func;	/* memory free function */
} table_t;

/* external table structure for debuggers */
typedef table_t	table_ext_t;

/* local comparison functions */
typedef int	(*compare_t)(const void *element1_p, const void *element2_p,
						 table_compare_t user_compare,
						 const table_t *table_p, int *err_bp);

/*
 * to map error to string
 */
typedef struct {
	int		es_error;		/* error number */
	char		*es_string;		/* assocaited string */
} error_str_t;

static	error_str_t	errors[] = {
	{ TABLE_ERROR_NONE,		"no error" },
	{ TABLE_ERROR_PNT,		"invalid table pointer" },
	{ TABLE_ERROR_ARG_NULL,	"buffer argument is null" },
	{ TABLE_ERROR_SIZE,		"incorrect size argument" },
	{ TABLE_ERROR_OVERWRITE,	"key exists and no overwrite" },
	{ TABLE_ERROR_NOT_FOUND,	"key does not exist" },
	{ TABLE_ERROR_ALLOC,		"error allocating memory" },
	{ TABLE_ERROR_LINEAR,		"linear access not in progress" },
	{ TABLE_ERROR_OPEN,		"could not open file" },
	{ TABLE_ERROR_SEEK,		"could not seek to position in file" },
	{ TABLE_ERROR_READ,		"could not read from file" },
	{ TABLE_ERROR_WRITE,		"could not write to file" },
	{ TABLE_ERROR_MMAP_NONE,	"no mmap support compiled in library" },
	{ TABLE_ERROR_MMAP,		"could not mmap the file" },
	{ TABLE_ERROR_MMAP_OP,	"operation not valid on mmap files" },
	{ TABLE_ERROR_EMPTY,		"table is empty" },
	{ TABLE_ERROR_NOT_EMPTY,	"table contains data" },
	{ TABLE_ERROR_ALIGNMENT,	"invalid alignment value" },
	{ TABLE_ERROR_COMPARE,	"problems with internal comparison" },
	{ TABLE_ERROR_FREE,		"memory free error" },
	{ 0 }
};

#define INVALID_ERROR	"invalid error code"

#endif /* ! __TABLE_LOC_H__ */

/* For Emacs:
 * Local Variables:
 * mode:c
 * indent-tabs-mode:t
 * tab-width:4
 * c-basic-offset:4
 * End:
 * For VIM:
 * vim:set softtabstop=4 shiftwidth=4 tabstop=4 noet:
 */

