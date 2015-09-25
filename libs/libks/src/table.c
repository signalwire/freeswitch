/*
 * Generic hash table handler...
 *
 * Copyright 2000 by Gray Watson.
 *
 * This file is part of the table package.
 *
 * Permission to use, copy, modify, and distribute this software for
 * any purpose and without fee is hereby granted, provided that the
 * above copyright notice and this permission notice appear in all
 * copies, and that the name of Gray Watson not be used in advertising
 * or publicity pertaining to distribution of the document or software
 * without specific, written prior permission.
 *
 * Gray Watson makes no representations about the suitability of the
 * software described herein for any purpose.  It is provided "as is"
 * without express or implied warranty.
 *
 * The author may be reached via http://256.com/gray/
 *
 * $Id: table.c,v 1.19 2000/03/09 03:30:41 gray Exp $
 */

/*
 * Handles basic hash-table manipulations.  This is an implementation
 * of open hashing with an array of buckets holding linked lists of
 * elements.  Each element has a key and a data.  The user indexes on
 * the key to find the data.  See the typedefs in table_loc.h for more
 * information.
 */

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(unix) || defined(__APPLE__)

#include <unistd.h>

#else

#include <io.h>
#include <malloc.h>
#define NO_MMAP
#ifndef open
#define open _open
#endif
#ifndef fdopen
#define fdopen _fdopen
#endif
#endif

#ifndef NO_MMAP

#include <sys/mman.h>
#include <sys/stat.h>

#ifndef MAP_FAILED
#define MAP_FAILED	(caddr_t)0L
#endif

#endif

#define TABLE_MAIN

#include "table.h"
#include "table_loc.h"

#ifdef DMALLOC
#include "dmalloc.h"
#endif

static	char	*rcs_id =
	"$Id: table.c,v 1.19 2000/03/09 03:30:41 gray Exp $";

/*
 * Version id for the library.  You also need to add an entry to the
 * NEWS and ChangeLog files.
 */
static char *version_id = "$TableVersion: 4.3.0 March 8, 2000 $";

/****************************** local functions ******************************/

/*
 * static table_entry_t *first_entry
 *
 * DESCRIPTION:
 *
 * Return the first entry in the table.  It will set the linear
 * structure counter to the position of the first entry.
 *
 * RETURNS:
 *
 * Success: A pointer to the first entry in the table.
 *
 * Failure: NULL if there is no first entry.
 *
 * ARGUMENTS:
 *
 * table_p -> Table whose next entry we are finding.
 *
 * linear_p <-> Pointer to a linear structure which we will advance
 * and then find the corresponding entry.
 */
static	table_entry_t	*first_entry(const table_t *table_p,
									 table_linear_t *linear_p)
{
	table_entry_t	*entry_p;
	unsigned int	bucket_c = 0;
  
	/* look for the first non-empty bucket */
	for (bucket_c = 0; bucket_c < table_p->ta_bucket_n; bucket_c++) {
		entry_p = table_p->ta_buckets[bucket_c];
		if (entry_p != NULL) {
			if (linear_p != NULL) {
				linear_p->tl_bucket_c = bucket_c;
				linear_p->tl_entry_c = 0;
			}
			return TABLE_POINTER(table_p, table_entry_t *, entry_p);
		}
	}
  
	return NULL;
}

/*
 * static table_entry_t *next_entry
 *
 * DESCRIPTION:
 *
 * Return the next entry in the table which is past the position in
 * our linear pointer.  It will advance the linear structure counters.
 *
 * RETURNS:
 *
 * Success: A pointer to the next entry in the table.
 *
 * Failure: NULL.
 *
 * ARGUMENTS:
 *
 * table_p -> Table whose next entry we are finding.
 *
 * linear_p <-> Pointer to a linear structure which we will advance
 * and then find the corresponding entry.
 *
 * error_p <- Pointer to an integer which when the routine returns
 * will contain a table error code.
 */
static	table_entry_t	*next_entry(const table_t *table_p,
									table_linear_t *linear_p, int *error_p)
{
	table_entry_t	*entry_p;
	int		entry_c;
  
	/* can't next if we haven't first-ed */
	if (linear_p == NULL) {
		SET_POINTER(error_p, TABLE_ERROR_LINEAR);
		return NULL;
	}
  
	if (linear_p->tl_bucket_c >= table_p->ta_bucket_n) {
		/*
		 * NOTE: this might happen if we delete an item which shortens the
		 * table bucket numbers.
		 */
		SET_POINTER(error_p, TABLE_ERROR_NOT_FOUND);
		return NULL;
	}
  
	linear_p->tl_entry_c++;
  
	/* find the entry which is the nth in the list */
	entry_p = table_p->ta_buckets[linear_p->tl_bucket_c];
	/* NOTE: we swap the order here to be more efficient */
	for (entry_c = linear_p->tl_entry_c; entry_c > 0; entry_c--) {
		/* did we reach the end of the list? */
		if (entry_p == NULL) {
			break;
		}
		entry_p = TABLE_POINTER(table_p, table_entry_t *, entry_p)->te_next_p;
	}
  
	/* did we find an entry in the current bucket? */
	if (entry_p != NULL) {
		SET_POINTER(error_p, TABLE_ERROR_NONE);
		return TABLE_POINTER(table_p, table_entry_t *, entry_p);
	}
  
	/* find the first entry in the next non-empty bucket */
  
	linear_p->tl_entry_c = 0;
	for (linear_p->tl_bucket_c++; linear_p->tl_bucket_c < table_p->ta_bucket_n;
		 linear_p->tl_bucket_c++) {
		entry_p = table_p->ta_buckets[linear_p->tl_bucket_c];
		if (entry_p != NULL) {
			SET_POINTER(error_p, TABLE_ERROR_NONE);
			return TABLE_POINTER(table_p, table_entry_t *, entry_p);
		}
	}
  
	SET_POINTER(error_p, TABLE_ERROR_NOT_FOUND);
	return NULL;
}

/*
 * static table_entry_t *this_entry
 *
 * DESCRIPTION:
 *
 * Return the entry pointer in the table which is currently being
 * indicated by our linear pointer.
 *
 * RETURNS:
 *
 * Success: A pointer to the next entry in the table.
 *
 * Failure: NULL.
 *
 * ARGUMENTS:
 *
 * table_p -> Table whose next entry we are finding.
 *
 * linear_p -> Pointer to a linear structure which we will find the
 * corresponding entry.
 *
 * error_p <- Pointer to an integer which when the routine returns
 * will contain a table error code.
 */
static	table_entry_t	*this_entry(const table_t *table_p,
									const table_linear_t *linear_p,
									int *error_p)
{
	table_entry_t	*entry_p;
	int		entry_c;
  
	/* can't next if we haven't first-ed */
	if (linear_p == NULL) {
		SET_POINTER(error_p, TABLE_ERROR_LINEAR);
		return NULL;
	}
  
	if (linear_p->tl_bucket_c >= table_p->ta_bucket_n) {
		/*
		 * NOTE: this might happen if we delete an item which shortens the
		 * table bucket numbers.
		 */
		SET_POINTER(error_p, TABLE_ERROR_NOT_FOUND);
		return NULL;
	}
  
	/* find the entry which is the nth in the list */
	entry_p = table_p->ta_buckets[linear_p->tl_bucket_c];
  
	/* NOTE: we swap the order here to be more efficient */
	for (entry_c = linear_p->tl_entry_c; entry_c > 0; entry_c--) {
		/* did we reach the end of the list? */
		if (entry_p == NULL) {
			break;
		}
		entry_p = TABLE_POINTER(table_p, table_entry_t *, entry_p)->te_next_p;
	}
  
	/* did we find an entry in the current bucket? */
	if (entry_p == NULL) {
		SET_POINTER(error_p, TABLE_ERROR_NOT_FOUND);
		return NULL;
	}
	else {
		SET_POINTER(error_p, TABLE_ERROR_NONE);
		return TABLE_POINTER(table_p, table_entry_t *, entry_p);
	}
}

/*
 * static unsigned int hash
 *
 * DESCRIPTION:
 *
 * Hash a variable-length key into a 32-bit value.  Every bit of the
 * key affects every bit of the return value.  Every 1-bit and 2-bit
 * delta achieves avalanche.  About (6 * len + 35) instructions.  The
 * best hash table sizes are powers of 2.  There is no need to use mod
 * (sooo slow!).  If you need less than 32 bits, use a bitmask.  For
 * example, if you need only 10 bits, do h = (h & hashmask(10)); In
 * which case, the hash table should have hashsize(10) elements.
 *
 * By Bob Jenkins, 1996.  bob_jenkins@compuserve.com.  You may use
 * this code any way you wish, private, educational, or commercial.
 * It's free.  See
 * http://ourworld.compuserve.com/homepages/bob_jenkins/evahash.htm
 * Use for hash table lookup, or anything where one collision in 2^^32
 * is acceptable.  Do NOT use for cryptographic purposes.
 *
 * RETURNS:
 *
 * Returns a 32-bit hash value.
 *
 * ARGUMENTS:
 *
 * key - Key (the unaligned variable-length array of bytes) that we
 * are hashing.
 *
 * length - Length of the key in bytes.
 *
 * init_val - Initialization value of the hash if you need to hash a
 * number of strings together.  For instance, if you are hashing N
 * strings (unsigned char **)keys, do it like this:
 *
 * for (i=0, h=0; i<N; ++i) h = hash( keys[i], len[i], h);
 */
static	unsigned int	hash(const unsigned char *key,
							 const unsigned int length,
							 const unsigned int init_val)
{
	const unsigned char	*key_p = key;
	unsigned int		a, b, c, len;
  
	/* set up the internal state */
	a = 0x9e3779b9;	/* the golden ratio; an arbitrary value */
	b = 0x9e3779b9;
	c = init_val;		/* the previous hash value */
  
	/* handle most of the key */
	for (len = length; len >= 12; len -= 12) {
		a += (key_p[0]
			  + ((unsigned int)key_p[1] << 8)
			  + ((unsigned int)key_p[2] << 16)
			  + ((unsigned int)key_p[3] << 24));
		b += (key_p[4]
			  + ((unsigned int)key_p[5] << 8)
			  + ((unsigned int)key_p[6] << 16)
			  + ((unsigned int)key_p[7] << 24));
		c += (key_p[8]
			  + ((unsigned int)key_p[9] << 8)
			  + ((unsigned int)key_p[10] << 16)
			  + ((unsigned int)key_p[11] << 24));
		HASH_MIX(a,b,c);
		key_p += 12;
	}
  
	c += length;
  
	/* all the case statements fall through to the next */
	switch(len) {
	case 11:
		c += ((unsigned int)key_p[10] << 24);
	case 10:
		c += ((unsigned int)key_p[9] << 16);
	case 9:
		c += ((unsigned int)key_p[8] << 8);
		/* the first byte of c is reserved for the length */
	case 8:
		b += ((unsigned int)key_p[7] << 24);
	case 7:
		b += ((unsigned int)key_p[6] << 16);
	case 6:
		b += ((unsigned int)key_p[5] << 8);
	case 5:
		b += key_p[4];
	case 4:
		a += ((unsigned int)key_p[3] << 24);
	case 3:
		a += ((unsigned int)key_p[2] << 16);
	case 2:
		a += ((unsigned int)key_p[1] << 8);
	case 1:
		a += key_p[0];
		/* case 0: nothing left to add */
	}
	HASH_MIX(a, b, c);
  
	return c;
}

/*
 * static int entry_size
 *
 * DESCRIPTION:
 *
 * Calculates the appropriate size of an entry to include the key and
 * data sizes as well as any associated alignment to the data.
 *
 * RETURNS:
 *
 * The associated size of the entry.
 *
 * ARGUMENTS:
 *
 * table_p - Table associated with the entries whose size we are
 * determining.
 *
 * key_size - Size of the entry key.
 *
 * data - Size of the entry data.
 */
static	int	entry_size(const table_t *table_p, const unsigned int key_size,
					   const unsigned int data_size)
{
	int	size, left;
  
	/* initial size -- key is already aligned if right after struct */
	size = sizeof(struct table_shell_st) + key_size;
  
	/* if there is no alignment then it is easy */
	if (table_p->ta_data_align == 0) {
		return size + data_size;
	}
  
	/* add in our alignement */
	left = size & (table_p->ta_data_align - 1);
	if (left > 0) {
		size += table_p->ta_data_align - left;
	}
  
	/* we add the data size here after the alignment */
	size += data_size;
  
	return size;
}

/*
 * static unsigned char *entry_data_buf
 *
 * DESCRIPTION:
 *
 * Companion to the ENTRY_DATA_BUF macro but this handles any
 * associated alignment to the data in the entry.
 *
 * NOTE: we assume here that the data-alignment is > 0.
 *
 * RETURNS:
 *
 * Pointer to the data segment of the entry.
 *
 * ARGUMENTS:
 *
 * table_p - Table associated with the entry.
 *
 * entry_p - Entry whose data pointer we are determining.
 */
static	unsigned char	*entry_data_buf(const table_t *table_p,
										const table_entry_t *entry_p)
{
	const unsigned char	*buf_p;
	unsigned int		size, pad;
  
	buf_p = entry_p->te_key_buf + entry_p->te_key_size;
  
	/* we need the size of the space before the data */
	size = sizeof(struct table_shell_st) + entry_p->te_key_size;
  
	/* add in our alignment */
	pad = size & (table_p->ta_data_align - 1);
	if (pad > 0) {
		pad = table_p->ta_data_align - pad;
	}
  
	return (unsigned char *)buf_p + pad;
}

/******************************* sort routines *******************************/

/*
 * static int local_compare
 *
 * DESCRIPTION:
 *
 * Compare two entries by calling user's compare program or by using
 * memcmp.
 *
 * RETURNS:
 *
 * < 0, == 0, or > 0 depending on whether p1 is > p2, == p2, < p2.
 *
 * ARGUMENTS:
 *
 * p1 - First entry pointer to compare.
 *
 * p2 - Second entry pointer to compare.
 *
 * compare - User comparison function.  Ignored.
 *
 * table_p - Associated table being ordered.  Ignored.
 *
 * err_bp - Pointer to an integer which will be set with 1 if an error
 * has occurred.  It cannot be NULL.
 */
static int	local_compare(const void *p1, const void *p2,
						  table_compare_t compare, const table_t *table_p,
						  int *err_bp)
{
	const table_entry_t	* const *ent1_p = p1, * const *ent2_p = p2;
	int			cmp;
	unsigned int		size;
  
	/* compare as many bytes as we can */
	size = (*ent1_p)->te_key_size;
	if ((*ent2_p)->te_key_size < size) {
		size = (*ent2_p)->te_key_size;
	}
	cmp = memcmp(ENTRY_KEY_BUF(*ent1_p), ENTRY_KEY_BUF(*ent2_p), size);
	/* if common-size equal, then if next more bytes, it is larger */
	if (cmp == 0) {
		cmp = (*ent1_p)->te_key_size - (*ent2_p)->te_key_size;
	}
  
	*err_bp = 0;
	return cmp;
}

/*
 * static int local_compare_pos
 *
 * DESCRIPTION:
 *
 * Compare two entries by calling user's compare program or by using
 * memcmp.
 *
 * RETURNS:
 *
 * < 0, == 0, or > 0 depending on whether p1 is > p2, == p2, < p2.
 *
 * ARGUMENTS:
 *
 * p1 - First entry pointer to compare.
 *
 * p2 - Second entry pointer to compare.
 *
 * compare - User comparison function.  Ignored.
 *
 * table_p - Associated table being ordered.
 *
 * err_bp - Pointer to an integer which will be set with 1 if an error
 * has occurred.  It cannot be NULL.
 */
static int	local_compare_pos(const void *p1, const void *p2,
							  table_compare_t compare,
							  const table_t *table_p, int *err_bp)
{
	const table_linear_t	*lin1_p = p1, *lin2_p = p2;
	const table_entry_t	*ent1_p, *ent2_p;
	int			cmp, ret;
	unsigned int		size;
  
	/* get entry pointers */
	ent1_p = this_entry(table_p, lin1_p, &ret);
	ent2_p = this_entry(table_p, lin2_p, &ret);
	if (ent1_p == NULL || ent2_p == NULL) {
		*err_bp = 1;
		return 0;
	}
  
	/* compare as many bytes as we can */
	size = ent1_p->te_key_size;
	if (ent2_p->te_key_size < size) {
		size = ent2_p->te_key_size;
	}
	cmp = memcmp(ENTRY_KEY_BUF(ent1_p), ENTRY_KEY_BUF(ent2_p), size);
	/* if common-size equal, then if next more bytes, it is larger */
	if (cmp == 0) {
		cmp = ent1_p->te_key_size - ent2_p->te_key_size;
	}
  
	*err_bp = 0;
	return cmp;
}

/*
 * static int external_compare
 *
 * DESCRIPTION:
 *
 * Compare two entries by calling user's compare program or by using
 * memcmp.
 *
 * RETURNS:
 *
 * < 0, == 0, or > 0 depending on whether p1 is > p2, == p2, < p2.
 *
 * ARGUMENTS:
 *
 * p1 - First entry pointer to compare.
 *
 * p2 - Second entry pointer to compare.
 *
 * user_compare - User comparison function.
 *
 * table_p - Associated table being ordered.
 *
 * err_bp - Pointer to an integer which will be set with 1 if an error
 * has occurred.  It cannot be NULL.
 */
static int	external_compare(const void *p1, const void *p2,
							 table_compare_t user_compare,
							 const table_t *table_p, int *err_bp)
{
	const table_entry_t	* const *ent1_p = p1, * const *ent2_p = p2;
	/* since we know we are not aligned we can use the EXTRY_DATA_BUF macro */
	*err_bp = 0;
	return user_compare(ENTRY_KEY_BUF(*ent1_p), (*ent1_p)->te_key_size,
						ENTRY_DATA_BUF(table_p, *ent1_p),
						(*ent1_p)->te_data_size,
						ENTRY_KEY_BUF(*ent2_p), (*ent2_p)->te_key_size,
						ENTRY_DATA_BUF(table_p, *ent2_p),
						(*ent2_p)->te_data_size);
}

/*
 * static int external_compare_pos
 *
 * DESCRIPTION:
 *
 * Compare two entries by calling user's compare program or by using
 * memcmp.
 *
 * RETURNS:
 *
 * < 0, == 0, or > 0 depending on whether p1 is > p2, == p2, < p2.
 *
 * ARGUMENTS:
 *
 * p1 - First entry pointer to compare.
 *
 * p2 - Second entry pointer to compare.
 *
 * user_compare - User comparison function.
 *
 * table_p - Associated table being ordered.
 *
 * err_bp - Pointer to an integer which will be set with 1 if an error
 * has occurred.  It cannot be NULL.
 */
static int	external_compare_pos(const void *p1, const void *p2,
								 table_compare_t user_compare,
								 const table_t *table_p, int *err_bp)
{
	const table_linear_t	*lin1_p = p1, *lin2_p = p2;
	const table_entry_t	*ent1_p, *ent2_p;
	int			ret;
  
	/* get entry pointers */
	ent1_p = this_entry(table_p, lin1_p, &ret);
	ent2_p = this_entry(table_p, lin2_p, &ret);
	if (ent1_p == NULL || ent2_p == NULL) {
		*err_bp = 1;
		return 0;
	}
  
	/* since we know we are not aligned we can use the EXTRY_DATA_BUF macro */
	*err_bp = 0;
	return user_compare(ENTRY_KEY_BUF(ent1_p), (ent1_p)->te_key_size,
						ENTRY_DATA_BUF(table_p, ent1_p), ent1_p->te_data_size,
						ENTRY_KEY_BUF(ent2_p), ent2_p->te_key_size,
						ENTRY_DATA_BUF(table_p, ent2_p), ent2_p->te_data_size);
}

/*
 * static int external_compare_align
 *
 * DESCRIPTION:
 *
 * Compare two entries by calling user's compare program or by using
 * memcmp.  Alignment information is necessary.
 *
 * RETURNS:
 *
 * < 0, == 0, or > 0 depending on whether p1 is > p2, == p2, < p2.
 *
 * ARGUMENTS:
 *
 * p1 - First entry pointer to compare.
 *
 * p2 - Second entry pointer to compare.
 *
 * user_compare - User comparison function.
 *
 * table_p - Associated table being ordered.
 *
 * err_bp - Pointer to an integer which will be set with 1 if an error
 * has occurred.  It cannot be NULL.
 */
static int	external_compare_align(const void *p1, const void *p2,
								   table_compare_t user_compare,
								   const table_t *table_p, int *err_bp)
{
	const table_entry_t	* const *ent1_p = p1, * const *ent2_p = p2;
	/* since we are aligned we have to use the entry_data_buf function */
	*err_bp = 0;
	return user_compare(ENTRY_KEY_BUF(*ent1_p), (*ent1_p)->te_key_size,
						entry_data_buf(table_p, *ent1_p),
						(*ent1_p)->te_data_size,
						ENTRY_KEY_BUF(*ent2_p), (*ent2_p)->te_key_size,
						entry_data_buf(table_p, *ent2_p),
						(*ent2_p)->te_data_size);
}

/*
 * static int external_compare_align_pos
 *
 * DESCRIPTION:
 *
 * Compare two entries by calling user's compare program or by using
 * memcmp.  Alignment information is necessary.
 *
 * RETURNS:
 *
 * < 0, == 0, or > 0 depending on whether p1 is > p2, == p2, < p2.
 *
 * ARGUMENTS:
 *
 * p1 - First entry pointer to compare.
 *
 * p2 - Second entry pointer to compare.
 *
 * user_compare - User comparison function.
 *
 * table_p - Associated table being ordered.
 *
 * err_bp - Pointer to an integer which will be set with 1 if an error
 * has occurred.  It cannot be NULL.
 */
static int	external_compare_align_pos(const void *p1, const void *p2,
									   table_compare_t user_compare,
									   const table_t *table_p, int *err_bp)
{
	const table_linear_t	*lin1_p = p1, *lin2_p = p2;
	const table_entry_t	*ent1_p, *ent2_p;
	int			ret;
  
	/* get entry pointers */
	ent1_p = this_entry(table_p, lin1_p, &ret);
	ent2_p = this_entry(table_p, lin2_p, &ret);
	if (ent1_p == NULL || ent2_p == NULL) {
		*err_bp = 1;
		return 0;
	}
  
	/* since we are aligned we have to use the entry_data_buf function */
	*err_bp = 0;
	return user_compare(ENTRY_KEY_BUF(ent1_p), ent1_p->te_key_size,
						entry_data_buf(table_p, ent1_p), ent1_p->te_data_size,
						ENTRY_KEY_BUF(ent2_p), ent2_p->te_key_size,
						entry_data_buf(table_p, ent2_p), ent2_p->te_data_size);
}

/*
 * static void swap_bytes
 *
 * DESCRIPTION:
 *
 * Swap the values between two items of a specified size.
 *
 * RETURNS:
 *
 * None.
 *
 * ARGUMENTS:
 *
 * item1_p -> Pointer to the first item.
 *
 * item2_p -> Pointer to the first item.
 *
 * ele_size -> Size of the two items.
 */
static	void	swap_bytes(unsigned char *item1_p, unsigned char *item2_p,
						   int ele_size)
{
	unsigned char	char_temp;
  
	for (; ele_size > 0; ele_size--) {
		char_temp = *item1_p;
		*item1_p = *item2_p;
		*item2_p = char_temp;
		item1_p++;
		item2_p++;
	}
}

/*
 * static void insert_sort
 *
 * DESCRIPTION:
 *
 * Do an insertion sort which is faster for small numbers of items and
 * better if the items are already sorted.
 *
 * RETURNS:
 *
 * Success - TABLE_ERROR_NONE
 *
 * Failure - Table error code.
 *
 * ARGUMENTS:
 *
 * first_p <-> Start of the list that we are splitting.
 *
 * last_p <-> Last entry in the list that we are splitting.
 *
 * holder_p <-> Location of hold area we can store an entry.
 *
 * ele_size -> Size of the each element in the list.
 *
 * compare -> Our comparison function.
 *
 * user_compare -> User comparison function.  Could be NULL if we are
 * just using a local comparison function.
 *
 * table_p -> Associated table being sorted.
 */
static	int	insert_sort(unsigned char *first_p, unsigned char *last_p,
						unsigned char *holder_p,
						const unsigned int ele_size, compare_t compare,
						table_compare_t user_compare, table_t *table_p)
{
	unsigned char	*inner_p, *outer_p;
	int		ret, err_b;
  
	for (outer_p = first_p + ele_size; outer_p <= last_p; ) {
    
		/* look for the place to insert the entry */
		for (inner_p = outer_p - ele_size;
			 inner_p >= first_p;
			 inner_p -= ele_size) {
			ret = compare(outer_p, inner_p, user_compare, table_p, &err_b);
			if (err_b) {
				return TABLE_ERROR_COMPARE;
			}
			if (ret >= 0) {
				break;
			}
		}
		inner_p += ele_size;
    
		/* do we need to insert the entry in? */
		if (outer_p != inner_p) {
			/*
			 * Now we shift the entry down into its place in the already
			 * sorted list.
			 */
			memcpy(holder_p, outer_p, ele_size);
			memmove(inner_p + ele_size, inner_p, outer_p - inner_p);
			memcpy(inner_p, holder_p, ele_size);
		}
    
		outer_p += ele_size;
	}
  
	return TABLE_ERROR_NONE;
}

/*
 * static int split
 *
 * DESCRIPTION:
 *
 * This sorts an array of longs via the quick sort algorithm (it's
 * pretty quick)
 *
 * RETURNS:
 *
 * None.
 *
 * ARGUMENTS:
 *
 * first_p -> Start of the list that we are splitting.
 *
 * last_p -> Last entry in the list that we are splitting.
 *
 * ele_size -> Size of the each element in the list.
 *
 * compare -> Our comparison function.
 *
 * user_compare -> User comparison function.  Could be NULL if we are
 * just using a local comparison function.
 *
 * table_p -> Associated table being sorted.
 */
static	int	split(unsigned char *first_p, unsigned char *last_p,
				  const unsigned int ele_size, compare_t compare,
				  table_compare_t user_compare, table_t *table_p)
{
	unsigned char	*left_p, *right_p, *pivot_p, *left_last_p, *right_first_p;
	unsigned char	*firsts[MAX_QSORT_SPLITS], *lasts[MAX_QSORT_SPLITS], *pivot;
	unsigned int	width, split_c = 0;
	int		size1, size2, min_qsort_size;
	int		ret, err_b;
  
	/*
	 * Allocate some space for our pivot value.  We also use this as
	 * holder space for our insert sort.
	 */
	pivot = alloca(ele_size);
	if (pivot == NULL) {
		/* what else can we do? */
		abort();
	}
  
	min_qsort_size = MAX_QSORT_MANY * ele_size;
  
	while (1) {
    
		/* find the left, right, and mid point */
		left_p = first_p;
		right_p = last_p;
		/* is there a faster way to find this? */
		width = (last_p - first_p) / ele_size;
		pivot_p = first_p + ele_size * (width >> 1);
    
		/*
		 * Find which of the left, middle, and right elements is the
		 * median (Knuth vol3 p123).
		 */
		ret = compare(first_p, pivot_p, user_compare, table_p, &err_b);
		if (err_b) {
			return TABLE_ERROR_COMPARE;
		}
		if (ret > 0) {
			swap_bytes(first_p, pivot_p, ele_size);
		}
		ret = compare(pivot_p, last_p, user_compare, table_p, &err_b);
		if (err_b) {
			return TABLE_ERROR_COMPARE;
		}
		if (ret > 0) {
			swap_bytes(pivot_p, last_p, ele_size);
			ret = compare(first_p, pivot_p, user_compare, table_p, &err_b);
			if (err_b) {
				return TABLE_ERROR_COMPARE;
			}
			if (ret > 0) {
				swap_bytes(first_p, pivot_p, ele_size);
			}
		}
    
		/*
		 * save our pivot so we don't have to worry about hitting and
		 * swapping it elsewhere while we iterate across the list below.
		 */
		memcpy(pivot, pivot_p, ele_size);
    
		do {
      
			/* shift the left side up until we reach the pivot value */
			while (1) {
				ret = compare(left_p, pivot, user_compare, table_p, &err_b);
				if (err_b) {
					return TABLE_ERROR_COMPARE;
				}
				if (ret >= 0) {
					break;
				}
				left_p += ele_size;
			}
			/* shift the right side down until we reach the pivot value */
			while (1) {
				ret = compare(pivot, right_p, user_compare, table_p, &err_b);
				if (err_b) {
					return TABLE_ERROR_COMPARE;
				}
				if (ret >= 0) {
					break;
				}
				right_p -= ele_size;
			}
      
			/* if we met in the middle then we are done */
			if (left_p == right_p) {
				left_p += ele_size;
				right_p -= ele_size;
				break;
			}
			else if (left_p < right_p) {
				/*
				 * swap the left and right since they both were on the wrong
				 * size of the pivot and continue
				 */
				swap_bytes(left_p, right_p, ele_size);
				left_p += ele_size;
				right_p -= ele_size;
			}
		} while (left_p <= right_p);
    
		/* Rename variables to make more sense.  This will get optimized out. */
		right_first_p = left_p;
		left_last_p = right_p;
    
		/* determine the size of the left and right hand parts */
		size1 = left_last_p - first_p;
		size2 = last_p - right_first_p;
    
		/* is the 1st half small enough to just insert-sort? */
		if (size1 < min_qsort_size) {
      
			/* use the pivot as our temporary space */
			ret = insert_sort(first_p, left_last_p, pivot, ele_size, compare,
							  user_compare, table_p);
			if (ret != TABLE_ERROR_NONE) {
				return ret;
			}
      
			/* is the 2nd part small as well? */
			if (size2 < min_qsort_size) {
	
				/* use the pivot as our temporary space */
				ret = insert_sort(right_first_p, last_p, pivot, ele_size, compare,
								  user_compare, table_p);
				if (ret != TABLE_ERROR_NONE) {
					return ret;
				}
	
				/* pop a partition off our stack */
				if (split_c == 0) {
					/* we are done */
					return TABLE_ERROR_NONE;
				}
				split_c--;
				first_p = firsts[split_c];
				last_p = lasts[split_c];
			}
			else {
				/* we can just handle the right side immediately */
				first_p = right_first_p;
				/* last_p = last_p */
			}
		}
		else if (size2 < min_qsort_size) {
      
			/* use the pivot as our temporary space */
			ret = insert_sort(right_first_p, last_p, pivot, ele_size, compare,
							  user_compare, table_p);
			if (ret != TABLE_ERROR_NONE) {
				return ret;
			}
      
			/* we can just handle the left side immediately */
			/* first_p = first_p */
			last_p = left_last_p;
		}
		else {
			/*
			 * neither partition is small, we'll have to push the larger one
			 * of them on the stack
			 */
			if (split_c >= MAX_QSORT_SPLITS) {
				/* sanity check here -- we should never get here */
				abort();
			}
			if (size1 > size2) {
				/* push the left partition on the stack */
				firsts[split_c] = first_p;
				lasts[split_c] = left_last_p;
				split_c++;
				/* continue handling the right side */
				first_p = right_first_p;
				/* last_p = last_p */
			}
			else {
				/* push the right partition on the stack */
				firsts[split_c] = right_first_p;
				lasts[split_c] = last_p;
				split_c++;
				/* continue handling the left side */
				/* first_p = first_p */
				last_p = left_last_p;
			}
		}
	}
  
	return TABLE_ERROR_NONE;
}

/*************************** exported routines *******************************/

/*
 * table_t *table_alloc
 *
 * DESCRIPTION:
 *
 * Allocate a new table structure.
 *
 * RETURNS:
 *
 * A pointer to the new table structure which must be passed to
 * table_free to be deallocated.  On error a NULL is returned.
 *
 * ARGUMENTS:
 *
 * bucket_n - Number of buckets for the hash table.  Our current hash
 * value works best with base two numbers.  Set to 0 to take the
 * library default of 1024.
 *
 * error_p - Pointer to an integer which, if not NULL, will contain a
 * table error code.
 */
table_t		*table_alloc(const unsigned int bucket_n, int *error_p)
{
	table_t	*table_p = NULL;
	unsigned int	buck_n;
  
	/* allocate a table structure */
	table_p = malloc(sizeof(table_t));
	if (table_p == NULL) {
		SET_POINTER(error_p, TABLE_ERROR_ALLOC);
		return NULL;
	}
  
	if (bucket_n > 0) {
		buck_n = bucket_n;
	}
	else {
		buck_n = DEFAULT_SIZE;
	}
  
	/* allocate the buckets which are NULLed */
	table_p->ta_buckets = (table_entry_t **)calloc(buck_n,
												   sizeof(table_entry_t *));
	if (table_p->ta_buckets == NULL) {
		SET_POINTER(error_p, TABLE_ERROR_ALLOC);
		free(table_p);
		return NULL;
	}
  
	/* initialize structure */
	table_p->ta_magic = TABLE_MAGIC;
	table_p->ta_flags = 0;
	table_p->ta_bucket_n = buck_n;
	table_p->ta_entry_n = 0;
	table_p->ta_data_align = 0;
	table_p->ta_linear.tl_magic = 0;
	table_p->ta_linear.tl_bucket_c = 0;
	table_p->ta_linear.tl_entry_c = 0;
	table_p->ta_mmap = NULL;
	table_p->ta_file_size = 0;
	table_p->ta_mem_pool = NULL;
	table_p->ta_alloc_func = NULL;
	table_p->ta_resize_func = NULL;
	table_p->ta_free_func = NULL;
  
	SET_POINTER(error_p, TABLE_ERROR_NONE);
	return table_p;
}

/*
 * table_t *table_alloc_in_pool
 *
 * DESCRIPTION:
 *
 * Allocate a new table structure in a memory pool or using
 * alternative allocation and free functions.
 *
 * RETURNS:
 *
 * A pointer to the new table structure which must be passed to
 * table_free to be deallocated.  On error a NULL is returned.
 *
 * ARGUMENTS:
 *
 * bucket_n - Number of buckets for the hash table.  Our current hash
 * value works best with base two numbers.  Set to 0 to take the
 * library default of 1024.
 *
 * mem_pool <-> Memory pool to associate with the table.  Can be NULL.
 *
 * alloc_func -> Allocate function we are overriding malloc() with.
 *
 * resize_func -> Resize function we are overriding the standard
 * memory resize/realloc with.  This can be NULL in which cause the
 * library will allocate, copy, and free itself.
 *
 * free_func -> Free function we are overriding free() with.
 *
 * error_p - Pointer to an integer which, if not NULL, will contain a
 * table error code.
 */
table_t		*table_alloc_in_pool(const unsigned int bucket_n,
								 void *mem_pool,
								 table_mem_alloc_t alloc_func,
								 table_mem_resize_t resize_func,
								 table_mem_free_t free_func, int *error_p)
{
	table_t	*table_p = NULL;
	unsigned int	buck_n, size;
  
	/* make sure we have real functions, mem_pool and resize_func can be NULL */
	if (alloc_func == NULL || free_func == NULL) {
		SET_POINTER(error_p, TABLE_ERROR_ARG_NULL);
		return NULL;
	}
  
	/* allocate a table structure */
	table_p = alloc_func(mem_pool, sizeof(table_t));
	if (table_p == NULL) {
		SET_POINTER(error_p, TABLE_ERROR_ALLOC);
		return NULL;
	}
  
	if (bucket_n > 0) {
		buck_n = bucket_n;
	}
	else {
		buck_n = DEFAULT_SIZE;
	}
  
	/* allocate the buckets which are NULLed */
	size = buck_n * sizeof(table_entry_t *);
	table_p->ta_buckets = (table_entry_t **)alloc_func(mem_pool, size);
	if (table_p->ta_buckets == NULL) {
		SET_POINTER(error_p, TABLE_ERROR_ALLOC);
		(void)free_func(mem_pool, table_p, sizeof(table_t));
		return NULL;
	}
	/*
	 * We zero it ourselves to save the necessity of having a
	 * table_mem_calloc_t memory override function.
	 */
	memset(table_p->ta_buckets, 0, size);
  
	/* initialize structure */
	table_p->ta_magic = TABLE_MAGIC;
	table_p->ta_flags = 0;
	table_p->ta_bucket_n = buck_n;
	table_p->ta_entry_n = 0;
	table_p->ta_data_align = 0;
	table_p->ta_linear.tl_magic = 0;
	table_p->ta_linear.tl_bucket_c = 0;
	table_p->ta_linear.tl_entry_c = 0;
	table_p->ta_mmap = NULL;
	table_p->ta_file_size = 0;
	table_p->ta_mem_pool = mem_pool;
	table_p->ta_alloc_func = alloc_func;
	table_p->ta_resize_func = resize_func;
	table_p->ta_free_func = free_func;
  
	SET_POINTER(error_p, TABLE_ERROR_NONE);
	return table_p;
}

/*
 * int table_attr
 *
 * DESCRIPTION:
 *
 * Set the attributes for the table.  The available attributes are
 * specified at the top of table.h.
 *
 * RETURNS:
 *
 * Success - TABLE_ERROR_NONE
 *
 * Failure - Table error code.
 *
 * ARGUMENTS:
 *
 * table_p - Pointer to a table structure which we will be altering.
 *
 * attr - Attribute(s) that we will be applying to the table.
 */
int	table_attr(table_t *table_p, const int attr)
{
	if (table_p == NULL) {
		return TABLE_ERROR_ARG_NULL;
	}
	if (table_p->ta_magic != TABLE_MAGIC) {
		return TABLE_ERROR_PNT;
	}
  
	table_p->ta_flags = attr;
  
	return TABLE_ERROR_NONE;
}

/*
 * int table_set_data_alignment
 *
 * DESCRIPTION:
 *
 * Set the alignment for the data in the table.  This is used when you
 * want to store binary data types and refer to them directly out of
 * the table storage.  For instance if you are storing integers as
 * data in the table and want to be able to retrieve the location of
 * the interger and then increment it as (*loc_p)++.  Otherwise you
 * would have to memcpy it out to an integer, increment it, and memcpy
 * it back.  If you are storing character data, no alignment is
 * necessary.
 *
 * For most data elements, sizeof(long) is recommended unless you use
 * smaller data types exclusively.
 *
 * WARNING: If necessary, you must set the data alignment before any
 * data gets put into the table.  Otherwise a TABLE_ERROR_NOT_EMPTY
 * error will be returned.
 *
 * NOTE: there is no way to set the key data alignment although it
 * should automatically be long aligned.
 *
 * RETURNS:
 *
 * Success - TABLE_ERROR_NONE
 *
 * Failure - Table error code.
 *
 * ARGUMENTS:
 *
 * table_p - Pointer to a table structure which we will be altering.
 *
 * alignment - Alignment requested for the data.  Must be a power of
 * 2.  Set to 0 for none.
 */
int	table_set_data_alignment(table_t *table_p, const int alignment)
{
	int		val;
  
	if (table_p == NULL) {
		return TABLE_ERROR_ARG_NULL;
	}
	if (table_p->ta_magic != TABLE_MAGIC) {
		return TABLE_ERROR_PNT;
	}
	if (table_p->ta_entry_n > 0) {
		return TABLE_ERROR_NOT_EMPTY;
	}
  
	/* defaults */
	if (alignment < 2) {
		table_p->ta_data_align = 0;
	}
	else {
		/* verify we have a base 2 number */
		for (val = 2; val < MAX_ALIGNMENT; val *= 2) {
			if (val == alignment) {
				break;
			}
		}
		if (val >= MAX_ALIGNMENT) {
			return TABLE_ERROR_ALIGNMENT;
		}
		table_p->ta_data_align = alignment;
	}
  
	return TABLE_ERROR_NONE;
}

/*
 * int table_clear
 *
 * DESCRIPTION:
 *
 * Clear out and free all elements in a table structure.
 *
 * RETURNS:
 *
 * Success - TABLE_ERROR_NONE
 *
 * Failure - Table error code.
 *
 * ARGUMENTS:
 *
 * table_p - Table structure pointer that we will be clearing.
 */
int	table_clear(table_t *table_p)
{
	int		final = TABLE_ERROR_NONE;
	table_entry_t	*entry_p, *next_p;
	table_entry_t	**bucket_p, **bounds_p;
  
	if (table_p == NULL) {
		return TABLE_ERROR_ARG_NULL;
	}
	if (table_p->ta_magic != TABLE_MAGIC) {
		return TABLE_ERROR_PNT;
	}
  
#ifndef NO_MMAP
	/* no mmap support so immediate error */
	if (table_p->ta_mmap != NULL) {
		return TABLE_ERROR_MMAP_OP;
	}
#endif
  
	/* free the table allocation and table structure */
	bounds_p = table_p->ta_buckets + table_p->ta_bucket_n;
	for (bucket_p = table_p->ta_buckets; bucket_p < bounds_p; bucket_p++) {
		for (entry_p = *bucket_p; entry_p != NULL; entry_p = next_p) {
			/* record the next pointer before we free */
			next_p = entry_p->te_next_p;
			if (table_p->ta_free_func == NULL) {
				free(entry_p);
			}
			else if (! table_p->ta_free_func(table_p->ta_mem_pool, entry_p,
											 entry_size(table_p,
														entry_p->te_key_size,
														entry_p->te_data_size))) {
				final = TABLE_ERROR_FREE;
			}
		}
    
		/* clear the bucket entry after we free its entries */
		*bucket_p = NULL;
	}
  
	/* reset table state info */
	table_p->ta_entry_n = 0;
	table_p->ta_linear.tl_magic = 0;
	table_p->ta_linear.tl_bucket_c = 0;
	table_p->ta_linear.tl_entry_c = 0;
  
	return final;
}

/*
 * int table_free
 *
 * DESCRIPTION:
 *
 * Deallocates a table structure.
 *
 * RETURNS:
 *
 * Success - TABLE_ERROR_NONE
 *
 * Failure - Table error code.
 *
 * ARGUMENTS:
 *
 * table_p - Table structure pointer that we will be freeing.
 */
int	table_free(table_t *table_p)
{
	int	ret;
  
	if (table_p == NULL) {
		return TABLE_ERROR_ARG_NULL;
	}
	if (table_p->ta_magic != TABLE_MAGIC) {
		return TABLE_ERROR_PNT;
	}
  
#ifndef NO_MMAP
	/* no mmap support so immediate error */
	if (table_p->ta_mmap != NULL) {
		return TABLE_ERROR_MMAP_OP;
	}
#endif
  
	ret = table_clear(table_p);
  
	if (table_p->ta_buckets != NULL) {
		if (table_p->ta_free_func == NULL) {
			free(table_p->ta_buckets);
		}
		else if (! table_p->ta_free_func(table_p->ta_mem_pool,
										 table_p->ta_buckets,
										 table_p->ta_bucket_n *
										 sizeof(table_entry_t *))) {
			return TABLE_ERROR_FREE;
		}
	}
	table_p->ta_magic = 0;
	if (table_p->ta_free_func == NULL) {
		free(table_p);
	}
	else if (! table_p->ta_free_func(table_p->ta_mem_pool, table_p,
									 sizeof(table_t))) {
		if (ret == TABLE_ERROR_NONE) {
			ret = TABLE_ERROR_FREE;
		}
	}
  
	return ret;
}

/*
 * int table_insert_kd
 *
 * DESCRIPTION:
 *
 * Like table_insert except it passes back a pointer to the key and
 * the data buffers after they have been inserted into the table
 * structure.
 *
 * This routine adds a key/data pair both of which are made up of a
 * buffer of bytes and an associated size.  Both the key and the data
 * will be copied into buffers allocated inside the table.  If the key
 * exists already, the associated data will be replaced if the
 * overwrite flag is set, otherwise an error is returned.
 *
 * NOTE: be very careful changing the values since the table library
 * provides the pointers to its memory.  The key can _never_ be
 * changed otherwise you will not find it again.  The data can be
 * changed but its length can never be altered unless you delete and
 * re-insert it into the table.
 *
 * WARNING: The pointers to the key and data are not in any specific
 * alignment.  Accessing the key and/or data as an short, integer, or
 * long pointer directly can cause problems.
 *
 * WARNING: Replacing a data cell (not inserting) will cause the table
 * linked list to be temporarily invalid.  Care must be taken with
 * multiple threaded programs which are relying on the first/next
 * linked list to be always valid.
 *
 * RETURNS:
 *
 * Success - TABLE_ERROR_NONE
 *
 * Failure - Table error code.
 *
 * ARGUMENTS:
 *
 * table_p - Table structure pointer into which we will be inserting a
 * new key/data pair.
 *
 * key_buf - Buffer of bytes of the key that we are inserting.  If you
 * are storing an (int) as the key (for example) then key_buf should
 * be a (int *).
 *
 * key_size - Size of the key_buf buffer.  If set to < 0 then the
 * library will do a strlen of key_buf and add 1 for the '\0'.  If you
 * are storing an (int) as the key (for example) then key_size should
 * be sizeof(int).
 *
 * data_buf - Buffer of bytes of the data that we are inserting.  If
 * it is NULL then the library will allocate space for the data in the
 * table without copying in any information.  If data_buf is NULL and
 * data_size is 0 then the library will associate a NULL data pointer
 * with the key.  If you are storing a (long) as the data (for
 * example) then data_buf should be a (long *).
 *
 * data_size - Size of the data_buf buffer.  If set to < 0 then the
 * library will do a strlen of data_buf and add 1 for the '\0'.  If
 * you are storing an (long) as the key (for example) then key_size
 * should be sizeof(long).
 *
 * key_buf_p - Pointer which, if not NULL, will be set to the address
 * of the key storage that was allocated in the table.  If you are
 * storing an (int) as the key (for example) then key_buf_p should be
 * (int **) i.e. the address of a (int *).
 *
 * data_buf_p - Pointer which, if not NULL, will be set to the address
 * of the data storage that was allocated in the table.  If you are
 * storing an (long) as the data (for example) then data_buf_p should
 * be (long **) i.e. the address of a (long *).
 *
 * overwrite - Flag which, if set to 1, will allow the overwriting of
 * the data in the table with the new data if the key already exists
 * in the table.
 */
int	table_insert_kd(table_t *table_p,
					const void *key_buf, const int key_size,
					const void *data_buf, const int data_size,
					void **key_buf_p, void **data_buf_p,
					const char overwrite_b)
{
	int		bucket;
	unsigned int	ksize, dsize, new_size, old_size, copy_size;
	table_entry_t	*entry_p, *last_p, *new_entry_p;
	void		*key_copy_p, *data_copy_p;
  
	/* check the arguments */
	if (table_p == NULL) {
		return TABLE_ERROR_ARG_NULL;
	}
	if (table_p->ta_magic != TABLE_MAGIC) {
		return TABLE_ERROR_PNT;
	}
	if (key_buf == NULL) {
		return TABLE_ERROR_ARG_NULL;
	}
	/* data_buf can be null but size must be >= 0, if it isn't null size != 0 */
	if ((data_buf == NULL && data_size < 0)
		|| (data_buf != NULL && data_size == 0)) {
		return TABLE_ERROR_SIZE;
	}
  
#ifndef NO_MMAP
	/* no mmap support so immediate error */
	if (table_p->ta_mmap != NULL) {
		return TABLE_ERROR_MMAP_OP;
	}
#endif
  
	/* determine sizes of key and data */
	if (key_size < 0) {
		ksize = strlen((char *)key_buf) + sizeof(char);
	}
	else {
		ksize = key_size;
	}
	if (data_size < 0) {
		dsize = strlen((char *)data_buf) + sizeof(char);
	}
	else {
		dsize = data_size;
	}
  
	/* get the bucket number via a hash function */
	bucket = hash(key_buf, ksize, 0) % table_p->ta_bucket_n;
  
	/* look for the entry in this bucket, only check keys of the same size */
	last_p = NULL;
	for (entry_p = table_p->ta_buckets[bucket];
		 entry_p != NULL;
		 last_p = entry_p, entry_p = entry_p->te_next_p) {
		if (entry_p->te_key_size == ksize
			&& memcmp(ENTRY_KEY_BUF(entry_p), key_buf, ksize) == 0) {
			break;
		}
	}
  
	/* did we find it?  then we are in replace mode. */
	if (entry_p != NULL) {
    
		/* can we not overwrite existing data? */
		if (! overwrite_b) {
			SET_POINTER(key_buf_p, ENTRY_KEY_BUF(entry_p));
			if (data_buf_p != NULL) {
				if (entry_p->te_data_size == 0) {
					*data_buf_p = NULL;
				}
				else {
					if (table_p->ta_data_align == 0) {
						*data_buf_p = ENTRY_DATA_BUF(table_p, entry_p);
					}
					else {
						*data_buf_p = entry_data_buf(table_p, entry_p);
					}
				}
			}
			return TABLE_ERROR_OVERWRITE;
		}
    
		/* re-alloc entry's data if the new size != the old */
		if (dsize != entry_p->te_data_size) {
      
			/*
			 * First we delete it from the list to keep the list whole.
			 * This properly preserves the linked list in case we have a
			 * thread marching through the linked list while we are
			 * inserting.  Maybe this is an unnecessary protection but it
			 * should not harm that much.
			 */
			if (last_p == NULL) {
				table_p->ta_buckets[bucket] = entry_p->te_next_p;
			}
			else {
				last_p->te_next_p = entry_p->te_next_p;
			}
      
			/*
			 * Realloc the structure which may change its pointer. NOTE:
			 * this may change any previous data_key_p and data_copy_p
			 * pointers.
			 */
			new_size = entry_size(table_p, entry_p->te_key_size, dsize);
			if (table_p->ta_resize_func == NULL) {
				/* if the alloc function has not been overriden do realloc */
				if (table_p->ta_alloc_func == NULL) {
					entry_p = (table_entry_t *)realloc(entry_p, new_size);
					if (entry_p == NULL) {
						return TABLE_ERROR_ALLOC;
					}
				}
				else {
					old_size = new_size - dsize + entry_p->te_data_size;
					/*
					 * if the user did override alloc but not resize, assume
					 * that the user's allocation functions can't grok realloc
					 * and do it ourselves the hard way.
					 */
					new_entry_p =
						(table_entry_t *)table_p->ta_alloc_func(table_p->ta_mem_pool,
																new_size);
					if (new_entry_p == NULL) {
						return TABLE_ERROR_ALLOC;
					}
					if (new_size > old_size) {
						copy_size = old_size;
					}
					else {
						copy_size = new_size;
					}
					memcpy(new_entry_p, entry_p, copy_size);
					if (! table_p->ta_free_func(table_p->ta_mem_pool, entry_p,
												old_size)) {
						return TABLE_ERROR_FREE;
					}
					entry_p = new_entry_p;
				}
			}
			else {
				old_size = new_size - dsize + entry_p->te_data_size;
				entry_p = (table_entry_t *)
					table_p->ta_resize_func(table_p->ta_mem_pool, entry_p,
											old_size, new_size);
				if (entry_p == NULL) {
					return TABLE_ERROR_ALLOC;
				}
			}
      
			/* add it back to the front of the list */
			entry_p->te_data_size = dsize;
			entry_p->te_next_p = table_p->ta_buckets[bucket];
			table_p->ta_buckets[bucket] = entry_p;
		}
    
		/* copy or replace data in storage */
		if (dsize > 0) {
			if (table_p->ta_data_align == 0) {
				data_copy_p = ENTRY_DATA_BUF(table_p, entry_p);
			}
			else {
				data_copy_p = entry_data_buf(table_p, entry_p);
			}
			if (data_buf != NULL) {
				memcpy(data_copy_p, data_buf, dsize);
			}
		}
		else {
			data_copy_p = NULL;
		}
    
		SET_POINTER(key_buf_p, ENTRY_KEY_BUF(entry_p));
		SET_POINTER(data_buf_p, data_copy_p);
    
		/* returning from the section where we were overwriting table data */
		return TABLE_ERROR_NONE;
	}
  
	/*
	 * It is a new entry.
	 */
  
	/* allocate a new entry */
	new_size = entry_size(table_p, ksize, dsize);
	if (table_p->ta_alloc_func == NULL) {
		entry_p = (table_entry_t *)malloc(new_size);
	}
	else {
		entry_p =
			(table_entry_t *)table_p->ta_alloc_func(table_p->ta_mem_pool, new_size);
	}
	if (entry_p == NULL) {
		return TABLE_ERROR_ALLOC;
	}
  
	/* copy key into storage */
	entry_p->te_key_size = ksize;
	key_copy_p = ENTRY_KEY_BUF(entry_p);
	memcpy(key_copy_p, key_buf, ksize);
  
	/* copy data in */
	entry_p->te_data_size = dsize;
	if (dsize > 0) {
		if (table_p->ta_data_align == 0) {
			data_copy_p = ENTRY_DATA_BUF(table_p, entry_p);
		}
		else {
			data_copy_p = entry_data_buf(table_p, entry_p);
		}
		if (data_buf != NULL) {
			memcpy(data_copy_p, data_buf, dsize);
		}
	}
	else {
		data_copy_p = NULL;
	}
  
	SET_POINTER(key_buf_p, key_copy_p);
	SET_POINTER(data_buf_p, data_copy_p);
  
	/* insert into list, no need to append */
	entry_p->te_next_p = table_p->ta_buckets[bucket];
	table_p->ta_buckets[bucket] = entry_p;
  
	table_p->ta_entry_n++;
  
	/* do we need auto-adjust? */
	if ((table_p->ta_flags & TABLE_FLAG_AUTO_ADJUST)
		&& SHOULD_TABLE_GROW(table_p)) {
		return table_adjust(table_p, table_p->ta_entry_n);
	}
  
	return TABLE_ERROR_NONE;
}

/*
 * int table_insert
 *
 * DESCRIPTION:
 *
 * Exactly the same as table_insert_kd except it does not pass back a
 * pointer to the key after they have been inserted into the table
 * structure.  This is still here for backwards compatibility.
 *
 * See table_insert_kd for more information.
 *
 * RETURNS:
 *
 * Success - TABLE_ERROR_NONE
 *
 * Failure - Table error code.
 *
 * ARGUMENTS:
 *
 * table_p - Table structure pointer into which we will be inserting a
 * new key/data pair.
 *
 * key_buf - Buffer of bytes of the key that we are inserting.  If you
 * are storing an (int) as the key (for example) then key_buf should
 * be a (int *).
 *
 * key_size - Size of the key_buf buffer.  If set to < 0 then the
 * library will do a strlen of key_buf and add 1 for the '\0'.  If you
 * are storing an (int) as the key (for example) then key_size should
 * be sizeof(int).
 *
 * data_buf - Buffer of bytes of the data that we are inserting.  If
 * it is NULL then the library will allocate space for the data in the
 * table without copying in any information.  If data_buf is NULL and
 * data_size is 0 then the library will associate a NULL data pointer
 * with the key.  If you are storing a (long) as the data (for
 * example) then data_buf should be a (long *).
 *
 * data_size - Size of the data_buf buffer.  If set to < 0 then the
 * library will do a strlen of data_buf and add 1 for the '\0'.  If
 * you are storing an (long) as the key (for example) then key_size
 * should be sizeof(long).
 *
 * data_buf_p - Pointer which, if not NULL, will be set to the address
 * of the data storage that was allocated in the table.  If you are
 * storing an (long) as the data (for example) then data_buf_p should
 * be (long **) i.e. the address of a (long *).
 *
 * overwrite - Flag which, if set to 1, will allow the overwriting of
 * the data in the table with the new data if the key already exists
 * in the table.
 */
int	table_insert(table_t *table_p,
				 const void *key_buf, const int key_size,
				 const void *data_buf, const int data_size,
				 void **data_buf_p, const char overwrite_b)
{
	return table_insert_kd(table_p, key_buf, key_size, data_buf, data_size,
						   NULL, data_buf_p, overwrite_b);
}

/*
 * int table_retrieve
 *
 * DESCRIPTION:
 *
 * This routine looks up a key made up of a buffer of bytes and an
 * associated size in the table.  If found then it returns the
 * associated data information.
 *
 * RETURNS:
 *
 * Success - TABLE_ERROR_NONE
 *
 * Failure - Table error code.
 *
 * ARGUMENTS:
 *
 * table_p - Table structure pointer into which we will be searching
 * for the key.
 *
 * key_buf - Buffer of bytes of the key that we are searching for.  If
 * you are looking for an (int) as the key (for example) then key_buf
 * should be a (int *).
 *
 * key_size - Size of the key_buf buffer.  If set to < 0 then the
 * library will do a strlen of key_buf and add 1 for the '\0'.  If you
 * are looking for an (int) as the key (for example) then key_size
 * should be sizeof(int).
 *
 * data_buf_p - Pointer which, if not NULL, will be set to the address
 * of the data storage that was allocated in the table and that is
 * associated with the key.  If a (long) was stored as the data (for
 * example) then data_buf_p should be (long **) i.e. the address of a
 * (long *).
 *
 * data_size_p - Pointer to an integer which, if not NULL, will be set
 * to the size of the data stored in the table that is associated with
 * the key.
 */
int	table_retrieve(table_t *table_p,
				   const void *key_buf, const int key_size,
				   void **data_buf_p, int *data_size_p)
{
	int		bucket;
	unsigned int	ksize;
	table_entry_t	*entry_p, **buckets;
  
	if (table_p == NULL) {
		return TABLE_ERROR_ARG_NULL;
	}
	if (table_p->ta_magic != TABLE_MAGIC) {
		return TABLE_ERROR_PNT;
	}
	if (key_buf == NULL) {
		return TABLE_ERROR_ARG_NULL;
	}
  
	/* find key size */
	if (key_size < 0) {
		ksize = strlen((char *)key_buf) + sizeof(char);
	}
	else {
		ksize = key_size;
	}
  
	/* get the bucket number via a has function */
	bucket = hash(key_buf, ksize, 0) % table_p->ta_bucket_n;
  
	/* look for the entry in this bucket, only check keys of the same size */
	buckets = table_p->ta_buckets;
	for (entry_p = buckets[bucket];
		 entry_p != NULL;
		 entry_p = entry_p->te_next_p) {
		entry_p = TABLE_POINTER(table_p, table_entry_t *, entry_p);
		if (entry_p->te_key_size == ksize
			&& memcmp(ENTRY_KEY_BUF(entry_p), key_buf, ksize) == 0) {
			break;
		}
	}
  
	/* not found? */
	if (entry_p == NULL) {
		return TABLE_ERROR_NOT_FOUND;
	}
  
	if (data_buf_p != NULL) {
		if (entry_p->te_data_size == 0) {
			*data_buf_p = NULL;
		}
		else {
			if (table_p->ta_data_align == 0) {
				*data_buf_p = ENTRY_DATA_BUF(table_p, entry_p);
			}
			else {
				*data_buf_p = entry_data_buf(table_p, entry_p);
			}
		}
	}
	SET_POINTER(data_size_p, entry_p->te_data_size);
  
	return TABLE_ERROR_NONE;
}

/*
 * int table_delete
 *
 * DESCRIPTION:
 *
 * This routine looks up a key made up of a buffer of bytes and an
 * associated size in the table.  If found then it will be removed
 * from the table.  The associated data can be passed back to the user
 * if requested.
 *
 * RETURNS:
 *
 * Success - TABLE_ERROR_NONE
 *
 * Failure - Table error code.
 *
 * NOTE: this could be an allocation error if the library is to return
 * the data to the user.
 *
 * ARGUMENTS:
 *
 * table_p - Table structure pointer from which we will be deleteing
 * the key.
 *
 * key_buf - Buffer of bytes of the key that we are searching for to
 * delete.  If you are deleting an (int) key (for example) then
 * key_buf should be a (int *).
 *
 * key_size - Size of the key_buf buffer.  If set to < 0 then the
 * library will do a strlen of key_buf and add 1 for the '\0'.  If you
 * are deleting an (int) key (for example) then key_size should be
 * sizeof(int).
 *
 * data_buf_p - Pointer which, if not NULL, will be set to the address
 * of the data storage that was allocated in the table and that was
 * associated with the key.  If a (long) was stored as the data (for
 * example) then data_buf_p should be (long **) i.e. the address of a
 * (long *).  If a pointer is passed in, the caller is responsible for
 * freeing it after use.  If data_buf_p is NULL then the library will
 * free up the data allocation itself.
 *
 * data_size_p - Pointer to an integer which, if not NULL, will be set
 * to the size of the data that was stored in the table and that was
 * associated with the key.
 */
int	table_delete(table_t *table_p,
				 const void *key_buf, const int key_size,
				 void **data_buf_p, int *data_size_p)
{
	int		bucket;
	unsigned int	ksize;
	unsigned char	*data_copy_p;
	table_entry_t	*entry_p, *last_p;
  
	if (table_p == NULL) {
		return TABLE_ERROR_ARG_NULL;
	}
	if (table_p->ta_magic != TABLE_MAGIC) {
		return TABLE_ERROR_PNT;
	}
	if (key_buf == NULL) {
		return TABLE_ERROR_ARG_NULL;
	}
  
#ifndef NO_MMAP
	/* no mmap support so immediate error */
	if (table_p->ta_mmap != NULL) {
		return TABLE_ERROR_MMAP_OP;
	}
#endif
  
	/* get the key size */
	if (key_size < 0) {
		ksize = strlen((char *)key_buf) + sizeof(char);
	}
	else {
		ksize = key_size;
	}
  
	/* find our bucket */
	bucket = hash(key_buf, ksize, 0) % table_p->ta_bucket_n;
  
	/* look for the entry in this bucket, only check keys of the same size */
	for (last_p = NULL, entry_p = table_p->ta_buckets[bucket];
		 entry_p != NULL;
		 last_p = entry_p, entry_p = entry_p->te_next_p) {
		if (entry_p->te_key_size == ksize
			&& memcmp(ENTRY_KEY_BUF(entry_p), key_buf, ksize) == 0) {
			break;
		}
	}
  
	/* did we find it? */
	if (entry_p == NULL) {
		return TABLE_ERROR_NOT_FOUND;
	}
  
	/*
	 * NOTE: we may want to adjust the linear counters here if the entry
	 * we are deleting is the one we are pointing on or is ahead of the
	 * one in the bucket list
	 */
  
	/* remove entry from the linked list */
	if (last_p == NULL) {
		table_p->ta_buckets[bucket] = entry_p->te_next_p;
	}
	else {
		last_p->te_next_p = entry_p->te_next_p;
	}
  
	/* free entry */
	if (data_buf_p != NULL) {
		if (entry_p->te_data_size == 0) {
			*data_buf_p = NULL;
		}
		else {
			/*
			 * if we were storing it compacted, we now need to malloc some
			 * space if the user wants the value after the delete.
			 */
			if (table_p->ta_alloc_func == NULL) {
				*data_buf_p = malloc(entry_p->te_data_size);
			}
			else {
				*data_buf_p = table_p->ta_alloc_func(table_p->ta_mem_pool,
													 entry_p->te_data_size);
			}
			if (*data_buf_p == NULL) {
				return TABLE_ERROR_ALLOC;
			}
			if (table_p->ta_data_align == 0) {
				data_copy_p = ENTRY_DATA_BUF(table_p, entry_p);
			}
			else {
				data_copy_p = entry_data_buf(table_p, entry_p);
			}
			memcpy(*data_buf_p, data_copy_p, entry_p->te_data_size);
		}
	}
	SET_POINTER(data_size_p, entry_p->te_data_size);
	if (table_p->ta_free_func == NULL) {
		free(entry_p);
	}
	else if (! table_p->ta_free_func(table_p->ta_mem_pool, entry_p,
									 entry_size(table_p,
												entry_p->te_key_size,
												entry_p->te_data_size))) {
		return TABLE_ERROR_FREE;
	}
  
	table_p->ta_entry_n--;
  
	/* do we need auto-adjust down? */
	if ((table_p->ta_flags & TABLE_FLAG_AUTO_ADJUST)
		&& (table_p->ta_flags & TABLE_FLAG_ADJUST_DOWN)
		&& SHOULD_TABLE_SHRINK(table_p)) {
		return table_adjust(table_p, table_p->ta_entry_n);
	}
  
	return TABLE_ERROR_NONE;
}

/*
 * int table_delete_first
 *
 * DESCRIPTION:
 *
 * This is like the table_delete routines except it deletes the first
 * key/data pair in the table instead of an entry corresponding to a
 * particular key.  The associated key and data information can be
 * passed back to the user if requested.  This routines is handy to
 * clear out a table.
 *
 * RETURNS:
 *
 * Success - TABLE_ERROR_NONE
 *
 * Failure - Table error code.
 *
 * NOTE: this could be an allocation error if the library is to return
 * the data to the user.
 *
 * ARGUMENTS:
 *
 * table_p - Table structure pointer from which we will be deleteing
 * the first key.
 *
 * key_buf_p - Pointer which, if not NULL, will be set to the address
 * of the storage of the first key that was allocated in the table.
 * If an (int) was stored as the first key (for example) then
 * key_buf_p should be (int **) i.e. the address of a (int *).  If a
 * pointer is passed in, the caller is responsible for freeing it
 * after use.  If key_buf_p is NULL then the library will free up the
 * key allocation itself.
 *
 * key_size_p - Pointer to an integer which, if not NULL, will be set
 * to the size of the key that was stored in the table and that was
 * associated with the key.
 *
 * data_buf_p - Pointer which, if not NULL, will be set to the address
 * of the data storage that was allocated in the table and that was
 * associated with the key.  If a (long) was stored as the data (for
 * example) then data_buf_p should be (long **) i.e. the address of a
 * (long *).  If a pointer is passed in, the caller is responsible for
 * freeing it after use.  If data_buf_p is NULL then the library will
 * free up the data allocation itself.
 *
 * data_size_p - Pointer to an integer which, if not NULL, will be set
 * to the size of the data that was stored in the table and that was
 * associated with the key.
 */
int	table_delete_first(table_t *table_p,
					   void **key_buf_p, int *key_size_p,
					   void **data_buf_p, int *data_size_p)
{
	unsigned char		*data_copy_p;
	table_entry_t		*entry_p;
	table_linear_t	linear;
  
	if (table_p == NULL) {
		return TABLE_ERROR_ARG_NULL;
	}
	if (table_p->ta_magic != TABLE_MAGIC) {
		return TABLE_ERROR_PNT;
	}
  
#ifndef NO_MMAP
	/* no mmap support so immediate error */
	if (table_p->ta_mmap != NULL) {
		return TABLE_ERROR_MMAP_OP;
	}
#endif
  
	/* take the first entry */
	entry_p = first_entry(table_p, &linear);
	if (entry_p == NULL) {
		return TABLE_ERROR_NOT_FOUND;
	}
  
	/*
	 * NOTE: we may want to adjust the linear counters here if the entry
	 * we are deleting is the one we are pointing on or is ahead of the
	 * one in the bucket list
	 */
  
	/* remove entry from the linked list */
	table_p->ta_buckets[linear.tl_bucket_c] = entry_p->te_next_p;
  
	/* free entry */
	if (key_buf_p != NULL) {
		if (entry_p->te_key_size == 0) {
			*key_buf_p = NULL;
		}
		else {
			/*
			 * if we were storing it compacted, we now need to malloc some
			 * space if the user wants the value after the delete.
			 */
			if (table_p->ta_alloc_func == NULL) {
				*key_buf_p = malloc(entry_p->te_key_size);
			}
			else {
				*key_buf_p = table_p->ta_alloc_func(table_p->ta_mem_pool,
													entry_p->te_key_size);
			}
			if (*key_buf_p == NULL) {
				return TABLE_ERROR_ALLOC;
			}
			memcpy(*key_buf_p, ENTRY_KEY_BUF(entry_p), entry_p->te_key_size);
		}
	}
	SET_POINTER(key_size_p, entry_p->te_key_size);
  
	if (data_buf_p != NULL) {
		if (entry_p->te_data_size == 0) {
			*data_buf_p = NULL;
		}
		else {
			/*
			 * if we were storing it compacted, we now need to malloc some
			 * space if the user wants the value after the delete.
			 */
			if (table_p->ta_alloc_func == NULL) {
				*data_buf_p = malloc(entry_p->te_data_size);
			}
			else {
				*data_buf_p = table_p->ta_alloc_func(table_p->ta_mem_pool,
													 entry_p->te_data_size);
			}
			if (*data_buf_p == NULL) {
				return TABLE_ERROR_ALLOC;
			}
			if (table_p->ta_data_align == 0) {
				data_copy_p = ENTRY_DATA_BUF(table_p, entry_p);
			}
			else {
				data_copy_p = entry_data_buf(table_p, entry_p);
			}
			memcpy(*data_buf_p, data_copy_p, entry_p->te_data_size);
		}
	}
	SET_POINTER(data_size_p, entry_p->te_data_size);
	if (table_p->ta_free_func == NULL) {
		free(entry_p);
	}
	else if (! table_p->ta_free_func(table_p->ta_mem_pool, entry_p,
									 entry_size(table_p,
												entry_p->te_key_size,
												entry_p->te_data_size))) {
		return TABLE_ERROR_FREE;
	}
  
	table_p->ta_entry_n--;
  
	/* do we need auto-adjust down? */
	if ((table_p->ta_flags & TABLE_FLAG_AUTO_ADJUST)
		&& (table_p->ta_flags & TABLE_FLAG_ADJUST_DOWN)
		&& SHOULD_TABLE_SHRINK(table_p)) {
		return table_adjust(table_p, table_p->ta_entry_n);
	}
  
	return TABLE_ERROR_NONE;
}

/*
 * int table_info
 *
 * DESCRIPTION:
 *
 * Get some information about a table_p structure.
 *
 * RETURNS:
 *
 * Success - TABLE_ERROR_NONE
 *
 * Failure - Table error code.
 *
 * ARGUMENTS:
 *
 * table_p - Table structure pointer from which we are getting
 * information.
 *
 * num_buckets_p - Pointer to an integer which, if not NULL, will
 * contain the number of buckets in the table.
 *
 * num_entries_p - Pointer to an integer which, if not NULL, will
 * contain the number of entries stored in the table.
 */
int	table_info(table_t *table_p, int *num_buckets_p, int *num_entries_p)
{
	if (table_p == NULL) {
		return TABLE_ERROR_ARG_NULL;
	}
	if (table_p->ta_magic != TABLE_MAGIC) {
		return TABLE_ERROR_PNT;
	}
  
	SET_POINTER(num_buckets_p, table_p->ta_bucket_n);
	SET_POINTER(num_entries_p, table_p->ta_entry_n);
  
	return TABLE_ERROR_NONE;
}

/*
 * int table_adjust
 *
 * DESCRIPTION:
 *
 * Set the number of buckets in a table to a certain value.
 *
 * RETURNS:
 *
 * Success - TABLE_ERROR_NONE
 *
 * Failure - Table error code.
 *
 * ARGUMENTS:
 *
 * table_p - Table structure pointer of which we are adjusting.
 *
 * bucket_n - Number buckets to adjust the table to.  Set to 0 to
 * adjust the table to its number of entries.
 */
int	table_adjust(table_t *table_p, const int bucket_n)
{
	table_entry_t	*entry_p, *next_p;
	table_entry_t	**buckets, **bucket_p, **bounds_p;
	int		bucket;
	unsigned int	buck_n, bucket_size;
  
	if (table_p == NULL) {
		return TABLE_ERROR_ARG_NULL;
	}
	if (table_p->ta_magic != TABLE_MAGIC) {
		return TABLE_ERROR_PNT;
	}
  
#ifndef NO_MMAP
	/* no mmap support so immediate error */
	if (table_p->ta_mmap != NULL) {
		return TABLE_ERROR_MMAP_OP;
	}
#endif
  
	/*
	 * NOTE: we walk through the entries and rehash them.  If we stored
	 * the hash value as a full int in the table-entry, all we would
	 * have to do is remod it.
	 */
  
	/* normalize to the number of entries */
	if (bucket_n == 0) {
		buck_n = table_p->ta_entry_n;
	}
	else {
		buck_n = bucket_n;
	}
  
	/* we must have at least 1 bucket */
	if (buck_n == 0) {
		buck_n = 1;
	}
  
	(void)printf("growing table to %d\n", buck_n);
  
	/* make sure we have something to do */
	if (buck_n == table_p->ta_bucket_n) {
		return TABLE_ERROR_NONE;
	}
  
	/* allocate a new bucket list */
	bucket_size = buck_n * sizeof(table_entry_t *);
	if (table_p->ta_alloc_func == NULL) {
		buckets = (table_entry_t **)malloc(bucket_size);
	}
	else {
		buckets =
			(table_entry_t **)table_p->ta_alloc_func(table_p->ta_mem_pool,
													 bucket_size);
	}
	if (buckets == NULL) {
		return TABLE_ERROR_ALLOC;
	}
	/*
	 * We zero it ourselves to save the necessity of having a
	 * table_mem_calloc_t memory override function.
	 */
	memset(buckets, 0, bucket_size);
  
	/*
	 * run through each of the items in the current table and rehash
	 * them into the newest bucket sizes
	 */
	bounds_p = table_p->ta_buckets + table_p->ta_bucket_n;
	for (bucket_p = table_p->ta_buckets; bucket_p < bounds_p; bucket_p++) {
		for (entry_p = *bucket_p; entry_p != NULL; entry_p = next_p) {
      
			/* hash the old data into the new table size */
			bucket = hash(ENTRY_KEY_BUF(entry_p), entry_p->te_key_size, 0) % buck_n;
      
			/* record the next one now since we overwrite next below */
			next_p = entry_p->te_next_p;
      
			/* insert into new list, no need to append */
			entry_p->te_next_p = buckets[bucket];
			buckets[bucket] = entry_p;
      
			/*
			 * NOTE: we may want to adjust the bucket_c linear entry here to
			 * keep it current
			 */
		}
		/* remove the old table pointers as we go by */
		*bucket_p = NULL;
	}
  
	/* replace the table buckets with the new ones */
	if (table_p->ta_free_func == NULL) {
		free(table_p->ta_buckets);
	}
	else if (! table_p->ta_free_func(table_p->ta_mem_pool,
									 table_p->ta_buckets,
									 table_p->ta_bucket_n *
									 sizeof(table_entry_t *))) {
		return TABLE_ERROR_FREE;
	}
	table_p->ta_buckets = buckets;
	table_p->ta_bucket_n = buck_n;
  
	return TABLE_ERROR_NONE;
}

/*
 * int table_type_size
 *
 * DESCRIPTION:
 *
 * Return the size of the internal table type.
 *
 * RETURNS:
 *
 * The size of the table_t type.
 *
 * ARGUMENTS:
 *
 * None.
 */
int	table_type_size(void)
{
	return sizeof(table_t);
}

/************************* linear access routines ****************************/

/*
 * int table_first
 *
 * DESCRIPTION:
 *
 * Find first element in a table and pass back information about the
 * key/data pair.  If any of the key/data pointers are NULL then they
 * are ignored.
 *
 * NOTE: This function is not reentrant.  More than one thread cannot
 * be doing a first and next on the same table at the same time.  Use
 * the table_first_r version below for this.
 *
 * RETURNS:
 *
 * Success - TABLE_ERROR_NONE
 *
 * Failure - Table error code.
 *
 * ARGUMENTS:
 *
 * table_p - Table structure pointer from which we are getting the
 * first element.
 *
 * key_buf_p - Pointer which, if not NULL, will be set to the address
 * of the storage of the first key that is allocated in the table.  If
 * an (int) is stored as the first key (for example) then key_buf_p
 * should be (int **) i.e. the address of a (int *).
 *
 * key_size_p - Pointer to an integer which, if not NULL, will be set
 * to the size of the key that is stored in the table and that is
 * associated with the first key.
 *
 * data_buf_p - Pointer which, if not NULL, will be set to the address
 * of the data storage that is allocated in the table and that is
 * associated with the first key.  If a (long) is stored as the data
 * (for example) then data_buf_p should be (long **) i.e. the address
 * of a (long *).
 *
 * data_size_p - Pointer to an integer which, if not NULL, will be set
 * to the size of the data that is stored in the table and that is
 * associated with the first key.
 */
int	table_first(table_t *table_p,
				void **key_buf_p, int *key_size_p,
				void **data_buf_p, int *data_size_p)
{
	table_entry_t	*entry_p;
  
	if (table_p == NULL) {
		return TABLE_ERROR_ARG_NULL;
	}
	if (table_p->ta_magic != TABLE_MAGIC) {
		return TABLE_ERROR_PNT;
	}
  
	/* initialize our linear magic number */
	table_p->ta_linear.tl_magic = LINEAR_MAGIC;
  
	entry_p = first_entry(table_p, &table_p->ta_linear);
	if (entry_p == NULL) {
		return TABLE_ERROR_NOT_FOUND;
	}
  
	SET_POINTER(key_buf_p, ENTRY_KEY_BUF(entry_p));
	SET_POINTER(key_size_p, entry_p->te_key_size);
	if (data_buf_p != NULL) {
		if (entry_p->te_data_size == 0) {
			*data_buf_p = NULL;
		}
		else {
			if (table_p->ta_data_align == 0) {
				*data_buf_p = ENTRY_DATA_BUF(table_p, entry_p);
			}
			else {
				*data_buf_p = entry_data_buf(table_p, entry_p);
			}
		}
	}
	SET_POINTER(data_size_p, entry_p->te_data_size);
  
	return TABLE_ERROR_NONE;
}

/*
 * int table_next
 *
 * DESCRIPTION:
 *
 * Find the next element in a table and pass back information about
 * the key/data pair.  If any of the key/data pointers are NULL then
 * they are ignored.
 *
 * NOTE: This function is not reentrant.  More than one thread cannot
 * be doing a first and next on the same table at the same time.  Use
 * the table_next_r version below for this.
 *
 * RETURNS:
 *
 * Success - TABLE_ERROR_NONE
 *
 * Failure - Table error code.
 *
 * ARGUMENTS:
 *
 * table_p - Table structure pointer from which we are getting the
 * next element.
 *
 * key_buf_p - Pointer which, if not NULL, will be set to the address
 * of the storage of the next key that is allocated in the table.  If
 * an (int) is stored as the next key (for example) then key_buf_p
 * should be (int **) i.e. the address of a (int *).
 *
 * key_size_p - Pointer to an integer which, if not NULL, will be set
 * to the size of the key that is stored in the table and that is
 * associated with the next key.
 *
 * data_buf_p - Pointer which, if not NULL, will be set to the address
 * of the data storage that is allocated in the table and that is
 * associated with the next key.  If a (long) is stored as the data
 * (for example) then data_buf_p should be (long **) i.e. the address
 * of a (long *).
 *
 * data_size_p - Pointer to an integer which, if not NULL, will be set
 * to the size of the data that is stored in the table and that is
 * associated with the next key.
 */
int	table_next(table_t *table_p,
			   void **key_buf_p, int *key_size_p,
			   void **data_buf_p, int *data_size_p)
{
	table_entry_t	*entry_p;
	int		error;
  
	if (table_p == NULL) {
		return TABLE_ERROR_ARG_NULL;
	}
	if (table_p->ta_magic != TABLE_MAGIC) {
		return TABLE_ERROR_PNT;
	}
	if (table_p->ta_linear.tl_magic != LINEAR_MAGIC) {
		return TABLE_ERROR_LINEAR;
	}
  
	/* move to the next entry */
	entry_p = next_entry(table_p, &table_p->ta_linear, &error);
	if (entry_p == NULL) {
		return error;
	}
  
	SET_POINTER(key_buf_p, ENTRY_KEY_BUF(entry_p));
	SET_POINTER(key_size_p, entry_p->te_key_size);
	if (data_buf_p != NULL) {
		if (entry_p->te_data_size == 0) {
			*data_buf_p = NULL;
		}
		else {
			if (table_p->ta_data_align == 0) {
				*data_buf_p = ENTRY_DATA_BUF(table_p, entry_p);
			}
			else {
				*data_buf_p = entry_data_buf(table_p, entry_p);
			}
		}
	}
	SET_POINTER(data_size_p, entry_p->te_data_size);
  
	return TABLE_ERROR_NONE;
}

/*
 * int table_this
 *
 * DESCRIPTION:
 *
 * Find the current element in a table and pass back information about
 * the key/data pair.  If any of the key/data pointers are NULL then
 * they are ignored.
 *
 * NOTE: This function is not reentrant.  Use the table_current_r
 * version below.
 *
 * RETURNS:
 *
 * Success - TABLE_ERROR_NONE
 *
 * Failure - Table error code.
 *
 * ARGUMENTS:
 *
 * table_p - Table structure pointer from which we are getting the
 * current element.
 *
 * key_buf_p - Pointer which, if not NULL, will be set to the address
 * of the storage of the current key that is allocated in the table.
 * If an (int) is stored as the current key (for example) then
 * key_buf_p should be (int **) i.e. the address of a (int *).
 *
 * key_size_p - Pointer to an integer which, if not NULL, will be set
 * to the size of the key that is stored in the table and that is
 * associated with the current key.
 *
 * data_buf_p - Pointer which, if not NULL, will be set to the address
 * of the data storage that is allocated in the table and that is
 * associated with the current key.  If a (long) is stored as the data
 * (for example) then data_buf_p should be (long **) i.e. the address
 * of a (long *).
 *
 * data_size_p - Pointer to an integer which, if not NULL, will be set
 * to the size of the data that is stored in the table and that is
 * associated with the current key.
 */
int	table_this(table_t *table_p,
			   void **key_buf_p, int *key_size_p,
			   void **data_buf_p, int *data_size_p)
{
	table_entry_t	*entry_p = NULL;
	int		entry_c;
  
	if (table_p == NULL) {
		return TABLE_ERROR_ARG_NULL;
	}
	if (table_p->ta_magic != TABLE_MAGIC) {
		return TABLE_ERROR_PNT;
	}
	if (table_p->ta_linear.tl_magic != LINEAR_MAGIC) {
		return TABLE_ERROR_LINEAR;
	}
  
	/* if we removed an item that shorted the bucket list, we may get this */
	if (table_p->ta_linear.tl_bucket_c >= table_p->ta_bucket_n) {
		/*
		 * NOTE: this might happen if we delete an item which shortens the
		 * table bucket numbers.
		 */
		return TABLE_ERROR_NOT_FOUND;
	}
  
	/* find the entry which is the nth in the list */
	entry_p = table_p->ta_buckets[table_p->ta_linear.tl_bucket_c];
	/* NOTE: we swap the order here to be more efficient */
	for (entry_c = table_p->ta_linear.tl_entry_c; entry_c > 0; entry_c--) {
		/* did we reach the end of the list? */
		if (entry_p == NULL) {
			break;
		}
		entry_p = TABLE_POINTER(table_p, table_entry_t *, entry_p)->te_next_p;
	}

	/* is this a NOT_FOUND or a LINEAR error */
	if (entry_p == NULL) {
		return TABLE_ERROR_NOT_FOUND;
	}
  
	SET_POINTER(key_buf_p, ENTRY_KEY_BUF(entry_p));
	SET_POINTER(key_size_p, entry_p->te_key_size);
	if (data_buf_p != NULL) {
		if (entry_p->te_data_size == 0) {
			*data_buf_p = NULL;
		}
		else {
			if (table_p->ta_data_align == 0) {
				*data_buf_p = ENTRY_DATA_BUF(table_p, entry_p);
			}
			else {
				*data_buf_p = entry_data_buf(table_p, entry_p);
			}
		}
	}
	SET_POINTER(data_size_p, entry_p->te_data_size);
  
	return TABLE_ERROR_NONE;
}

/*
 * int table_first_r
 *
 * DESCRIPTION:
 *
 * Reetrant version of the table_first routine above.  Find first
 * element in a table and pass back information about the key/data
 * pair.  If any of the key/data pointers are NULL then they are
 * ignored.
 *
 * RETURNS:
 *
 * Success - TABLE_ERROR_NONE
 *
 * Failure - Table error code.
 *
 * ARGUMENTS:
 *
 * table_p - Table structure pointer from which we are getting the
 * first element.
 *
 * linear_p - Pointer to a table linear structure which is initialized
 * here.  The same pointer should then be passed to table_next_r
 * below.
 *
 * key_buf_p - Pointer which, if not NULL, will be set to the address
 * of the storage of the first key that is allocated in the table.  If
 * an (int) is stored as the first key (for example) then key_buf_p
 * should be (int **) i.e. the address of a (int *).
 *
 * key_size_p - Pointer to an integer which, if not NULL, will be set
 * to the size of the key that is stored in the table and that is
 * associated with the first key.
 *
 * data_buf_p - Pointer which, if not NULL, will be set to the address
 * of the data storage that is allocated in the table and that is
 * associated with the first key.  If a (long) is stored as the data
 * (for example) then data_buf_p should be (long **) i.e. the address
 * of a (long *).
 *
 * data_size_p - Pointer to an integer which, if not NULL, will be set
 * to the size of the data that is stored in the table and that is
 * associated with the first key.
 */
int	table_first_r(table_t *table_p, table_linear_t *linear_p,
				  void **key_buf_p, int *key_size_p,
				  void **data_buf_p, int *data_size_p)
{
	table_entry_t	*entry_p;
  
	if (table_p == NULL) {
		return TABLE_ERROR_ARG_NULL;
	}
	if (table_p->ta_magic != TABLE_MAGIC) {
		return TABLE_ERROR_PNT;
	}
	if (linear_p == NULL) {
		return TABLE_ERROR_ARG_NULL;
	}
  
	/* initialize our linear magic number */
	linear_p->tl_magic = LINEAR_MAGIC;
  
	entry_p = first_entry(table_p, linear_p);
	if (entry_p == NULL) {
		return TABLE_ERROR_NOT_FOUND;
	}
  
	SET_POINTER(key_buf_p, ENTRY_KEY_BUF(entry_p));
	SET_POINTER(key_size_p, entry_p->te_key_size);
	if (data_buf_p != NULL) {
		if (entry_p->te_data_size == 0) {
			*data_buf_p = NULL;
		}
		else {
			if (table_p->ta_data_align == 0) {
				*data_buf_p = ENTRY_DATA_BUF(table_p, entry_p);
			}
			else {
				*data_buf_p = entry_data_buf(table_p, entry_p);
			}
		}
	}
	SET_POINTER(data_size_p, entry_p->te_data_size);
  
	return TABLE_ERROR_NONE;
}

/*
 * int table_next_r
 *
 * DESCRIPTION:
 *
 * Reetrant version of the table_next routine above.  Find next
 * element in a table and pass back information about the key/data
 * pair.  If any of the key/data pointers are NULL then they are
 * ignored.
 *
 * RETURNS:
 *
 * Success - TABLE_ERROR_NONE
 *
 * Failure - Table error code.
 *
 * ARGUMENTS:
 *
 * table_p - Table structure pointer from which we are getting the
 * next element.
 *
 * linear_p - Pointer to a table linear structure which is incremented
 * here.  The same pointer must have been passed to table_first_r
 * first so that it can be initialized.
 *
 * key_buf_p - Pointer which, if not NULL, will be set to the address
 * of the storage of the next key that is allocated in the table.  If
 * an (int) is stored as the next key (for example) then key_buf_p
 * should be (int **) i.e. the address of a (int *).
 *
 * key_size_p - Pointer to an integer which, if not NULL will be set
 * to the size of the key that is stored in the table and that is
 * associated with the next key.
 *
 * data_buf_p - Pointer which, if not NULL, will be set to the address
 * of the data storage that is allocated in the table and that is
 * associated with the next key.  If a (long) is stored as the data
 * (for example) then data_buf_p should be (long **) i.e. the address
 * of a (long *).
 *
 * data_size_p - Pointer to an integer which, if not NULL, will be set
 * to the size of the data that is stored in the table and that is
 * associated with the next key.
 */
int	table_next_r(table_t *table_p, table_linear_t *linear_p,
				 void **key_buf_p, int *key_size_p,
				 void **data_buf_p, int *data_size_p)
{
	table_entry_t	*entry_p;
	int		error;
  
	if (table_p == NULL) {
		return TABLE_ERROR_ARG_NULL;
	}
	if (table_p->ta_magic != TABLE_MAGIC) {
		return TABLE_ERROR_PNT;
	}
	if (linear_p == NULL) {
		return TABLE_ERROR_ARG_NULL;
	}
	if (linear_p->tl_magic != LINEAR_MAGIC) {
		return TABLE_ERROR_LINEAR;
	}
  
	/* move to the next entry */
	entry_p = next_entry(table_p, linear_p, &error);
	if (entry_p == NULL) {
		return error;
	}
  
	SET_POINTER(key_buf_p, ENTRY_KEY_BUF(entry_p));
	SET_POINTER(key_size_p, entry_p->te_key_size);
	if (data_buf_p != NULL) {
		if (entry_p->te_data_size == 0) {
			*data_buf_p = NULL;
		}
		else {
			if (table_p->ta_data_align == 0) {
				*data_buf_p = ENTRY_DATA_BUF(table_p, entry_p);
			}
			else {
				*data_buf_p = entry_data_buf(table_p, entry_p);
			}
		}
	}
	SET_POINTER(data_size_p, entry_p->te_data_size);
  
	return TABLE_ERROR_NONE;
}

/*
 * int table_this_r
 *
 * DESCRIPTION:
 *
 * Reetrant version of the table_this routine above.  Find current
 * element in a table and pass back information about the key/data
 * pair.  If any of the key/data pointers are NULL then they are
 * ignored.
 *
 * RETURNS:
 *
 * Success - TABLE_ERROR_NONE
 *
 * Failure - Table error code.
 *
 * ARGUMENTS:
 *
 * table_p - Table structure pointer from which we are getting the
 * current element.
 *
 * linear_p - Pointer to a table linear structure which is accessed
 * here.  The same pointer must have been passed to table_first_r
 * first so that it can be initialized.
 *
 * key_buf_p - Pointer which, if not NULL, will be set to the address
 * of the storage of the current key that is allocated in the table.
 * If an (int) is stored as the current key (for example) then
 * key_buf_p should be (int **) i.e. the address of a (int *).
 *
 * key_size_p - Pointer to an integer which, if not NULL, will be set
 * to the size of the key that is stored in the table and that is
 * associated with the current key.
 *
 * data_buf_p - Pointer which, if not NULL, will be set to the address
 * of the data storage that is allocated in the table and that is
 * associated with the current key.  If a (long) is stored as the data
 * (for example) then data_buf_p should be (long **) i.e. the address
 * of a (long *).
 *
 * data_size_p - Pointer to an integer which, if not NULL, will be set
 * to the size of the data that is stored in the table and that is
 * associated with the current key.
 */
int	table_this_r(table_t *table_p, table_linear_t *linear_p,
				 void **key_buf_p, int *key_size_p,
				 void **data_buf_p, int *data_size_p)
{
	table_entry_t	*entry_p;
	int		entry_c;
  
	if (table_p == NULL) {
		return TABLE_ERROR_ARG_NULL;
	}
	if (table_p->ta_magic != TABLE_MAGIC) {
		return TABLE_ERROR_PNT;
	}
	if (linear_p->tl_magic != LINEAR_MAGIC) {
		return TABLE_ERROR_LINEAR;
	}
  
	/* if we removed an item that shorted the bucket list, we may get this */
	if (linear_p->tl_bucket_c >= table_p->ta_bucket_n) {
		/*
		 * NOTE: this might happen if we delete an item which shortens the
		 * table bucket numbers.
		 */
		return TABLE_ERROR_NOT_FOUND;
	}
  
	/* find the entry which is the nth in the list */
	for (entry_c = linear_p->tl_entry_c,
			 entry_p = table_p->ta_buckets[linear_p->tl_bucket_c];
		 entry_p != NULL && entry_c > 0;
		 entry_c--, entry_p = TABLE_POINTER(table_p, table_entry_t *,
											entry_p)->te_next_p) {
	}
  
	if (entry_p == NULL) {
		return TABLE_ERROR_NOT_FOUND;
	}
  
	SET_POINTER(key_buf_p, ENTRY_KEY_BUF(entry_p));
	SET_POINTER(key_size_p, entry_p->te_key_size);
	if (data_buf_p != NULL) {
		if (entry_p->te_data_size == 0) {
			*data_buf_p = NULL;
		}
		else {
			if (table_p->ta_data_align == 0) {
				*data_buf_p = ENTRY_DATA_BUF(table_p, entry_p);
			}
			else {
				*data_buf_p = entry_data_buf(table_p, entry_p);
			}
		}
	}
	SET_POINTER(data_size_p, entry_p->te_data_size);
  
	return TABLE_ERROR_NONE;
}

/******************************* mmap routines *******************************/

/*
 * table_t *table_mmap
 *
 * DESCRIPTION:
 *
 * Mmap a table from a file that had been written to disk earlier via
 * table_write.
 *
 * RETURNS:
 *
 * A pointer to the new table structure which must be passed to
 * table_munmap to be deallocated.  On error a NULL is returned.
 *
 * ARGUMENTS:
 *
 * path - Table file to mmap in.
 *
 * error_p - Pointer to an integer which, if not NULL, will contain a
 * table error code.
 */
table_t		*table_mmap(const char *path, int *error_p)
{
#ifdef NO_MMAP
  
	/* no mmap support so immediate error */
	SET_POINTER(error_p, TABLE_ERROR_MMAP_NONE);
	return NULL;
  
#else
  
	table_t	*table_p;
	struct stat	sbuf;
	int		fd, state;
  
	table_p = (table_t *)malloc(sizeof(table_t));
	if (table_p == NULL) {
		SET_POINTER(error_p, TABLE_ERROR_ALLOC);
		return NULL;
	}
  
	/* open the mmap file */
	fd = open(path, O_RDONLY, 0);
	if (fd < 0) {
		free(table_p);
		SET_POINTER(error_p, TABLE_ERROR_OPEN);
		return NULL;
	}
  
	/* get the file size */
	if (fstat(fd, &sbuf) != 0) {
		free(table_p);
		SET_POINTER(error_p, TABLE_ERROR_OPEN);
		return NULL;
	}
  
	/* mmap the space and close the file */
#ifdef __alpha
	state = (MAP_SHARED | MAP_FILE | MAP_VARIABLE);
#else
	state = MAP_SHARED;
#endif
  
	table_p->ta_mmap = (table_t *)mmap((caddr_t)0, sbuf.st_size, PROT_READ,
									   state, fd, 0);
	(void)close(fd);
  
	if (table_p->ta_mmap == (table_t *)MAP_FAILED) {
		SET_POINTER(error_p, TABLE_ERROR_MMAP);
		return NULL;
	}  
  
	/* is the mmap file contain bad info or maybe another system type? */
	if (table_p->ta_mmap->ta_magic != TABLE_MAGIC) {
		SET_POINTER(error_p, TABLE_ERROR_PNT);
		return NULL;
	}
  
	/* sanity check on the file size */
	if (table_p->ta_mmap->ta_file_size != sbuf.st_size) {
		SET_POINTER(error_p, TABLE_ERROR_SIZE);
		return NULL;
	}
  
	/* copy the fields out of the mmap file into our memory version */
	table_p->ta_magic = TABLE_MAGIC;
	table_p->ta_flags = table_p->ta_mmap->ta_flags;
	table_p->ta_bucket_n = table_p->ta_mmap->ta_bucket_n;
	table_p->ta_entry_n = table_p->ta_mmap->ta_entry_n;
	table_p->ta_data_align = table_p->ta_mmap->ta_data_align;
	table_p->ta_buckets = TABLE_POINTER(table_p, table_entry_t **,
										table_p->ta_mmap->ta_buckets);
	table_p->ta_linear.tl_magic = 0;
	table_p->ta_linear.tl_bucket_c = 0;
	table_p->ta_linear.tl_entry_c = 0;
	/* mmap is already set */
	table_p->ta_file_size = table_p->ta_mmap->ta_file_size;
  
	SET_POINTER(error_p, TABLE_ERROR_NONE);
	return table_p;
  
#endif
}

/*
 * int table_munmap
 *
 * DESCRIPTION:
 *
 * Unmmap a table that was previously mmapped using table_mmap.
 *
 * RETURNS:
 *
 * Returns table error codes.
 *
 * ARGUMENTS:
 *
 * table_p - Mmaped table pointer to unmap.
 */
int	table_munmap(table_t *table_p)
{
#ifdef NO_MMAP
  
	/* no mmap support so immediate error */
	return TABLE_ERROR_MMAP_NONE;
  
#else
  
	if (table_p == NULL) {
		return TABLE_ERROR_ARG_NULL;
	}
	if (table_p->ta_magic != TABLE_MAGIC) {
		return TABLE_ERROR_PNT;
	}
	if (table_p->ta_mmap == NULL) {
		return TABLE_ERROR_PNT;
	}
  
	(void)munmap((caddr_t)table_p->ta_mmap, table_p->ta_file_size);
	table_p->ta_magic = 0;
	free(table_p);
	return TABLE_ERROR_NONE;
  
#endif
}

/******************************* file routines *******************************/

/*
 * int table_read
 *
 * DESCRIPTION:
 *
 * Read in a table from a file that had been written to disk earlier
 * via table_write.
 *
 * RETURNS:
 *
 * Success - Pointer to the new table structure which must be passed
 * to table_free to be deallocated.
 *
 * Failure - NULL
 *
 * ARGUMENTS:
 *
 * path - Table file to read in.
 *
 * error_p - Pointer to an integer which, if not NULL, will contain a
 * table error code.
 */
table_t	*table_read(const char *path, int *error_p)
{
	unsigned int	size;
	int		fd, ent_size;
	FILE		*infile;
	table_entry_t	entry, **bucket_p, *entry_p = NULL, *last_p;
	unsigned long	pos;
	table_t	*table_p;
  
	/* open the file */
	fd = open(path, O_RDONLY, 0);
	if (fd < 0) {
		SET_POINTER(error_p, TABLE_ERROR_OPEN);
		return NULL;
	}
  
	/* allocate a table structure */
	table_p = malloc(sizeof(table_t));
	if (table_p == NULL) {
		SET_POINTER(error_p, TABLE_ERROR_ALLOC);
		return NULL;
	}
  
	/* now open the fd to get buffered i/o */
	infile = fdopen(fd, "r");
	if (infile == NULL) {
		SET_POINTER(error_p, TABLE_ERROR_OPEN);
		return NULL;
	}
  
	/* read the main table struct */
	if (fread(table_p, sizeof(table_t), 1, infile) != 1) {
		SET_POINTER(error_p, TABLE_ERROR_READ);
		free(table_p);
		return NULL;
	}
	table_p->ta_file_size = 0;
  
	/* is the mmap file contain bad info or maybe another system type? */
	if (table_p->ta_magic != TABLE_MAGIC) {
		SET_POINTER(error_p, TABLE_ERROR_PNT);
		return NULL;
	}
  
	/* allocate the buckets */
	table_p->ta_buckets = (table_entry_t **)calloc(table_p->ta_bucket_n,
												   sizeof(table_entry_t *));
	if (table_p->ta_buckets == NULL) {
		SET_POINTER(error_p, TABLE_ERROR_ALLOC);
		free(table_p);
		return NULL;
	}
  
	if (fread(table_p->ta_buckets, sizeof(table_entry_t *), table_p->ta_bucket_n,
			  infile) != (size_t)table_p->ta_bucket_n) {
		SET_POINTER(error_p, TABLE_ERROR_READ);
		free(table_p->ta_buckets);
		free(table_p);
		return NULL;
	}
  
	/* read in the entries */
	for (bucket_p = table_p->ta_buckets;
		 bucket_p < table_p->ta_buckets + table_p->ta_bucket_n;
		 bucket_p++) {
    
		/* skip null buckets */
		if (*bucket_p == NULL) {
			continue;
		}
    
		/* run through the entry list */
		last_p = NULL;
		for (pos = *(unsigned long *)bucket_p;;
			 pos = (unsigned long)entry_p->te_next_p) {
      
			/* read in the entry */
			if (fseek(infile, pos, SEEK_SET) != 0) {
				SET_POINTER(error_p, TABLE_ERROR_SEEK);
				free(table_p->ta_buckets);
				free(table_p);
				if (entry_p != NULL) {
					free(entry_p);
				}
				/* the other table elements will not be freed */
				return NULL;
			}
			if (fread(&entry, sizeof(struct table_shell_st), 1, infile) != 1) {
				SET_POINTER(error_p, TABLE_ERROR_READ);
				free(table_p->ta_buckets);
				free(table_p);
				if (entry_p != NULL) {
					free(entry_p);
				}
				/* the other table elements will not be freed */
				return NULL;
			}
      
			/* make a new entry */
			ent_size = entry_size(table_p, entry.te_key_size, entry.te_data_size);
			entry_p = (table_entry_t *)malloc(ent_size);
			if (entry_p == NULL) {
				SET_POINTER(error_p, TABLE_ERROR_ALLOC);
				free(table_p->ta_buckets);
				free(table_p);
				/* the other table elements will not be freed */
				return NULL;
			}
			entry_p->te_key_size = entry.te_key_size;
			entry_p->te_data_size = entry.te_data_size;
			entry_p->te_next_p = entry.te_next_p;
      
			if (last_p == NULL) {
				*bucket_p = entry_p;
			}
			else {
				last_p->te_next_p = entry_p;
			}
      
			/* determine how much more we have to read */
			size = ent_size - sizeof(struct table_shell_st);
			if (fread(ENTRY_KEY_BUF(entry_p), sizeof(char), size, infile) != size) {
				SET_POINTER(error_p, TABLE_ERROR_READ);
				free(table_p->ta_buckets);
				free(table_p);
				free(entry_p);
				/* the other table elements will not be freed */
				return NULL;
			}
      
			/* we are done if the next pointer is null */
			if (entry_p->te_next_p == (unsigned long)0) {
				break;
			}
			last_p = entry_p;
		}
	}
  
	(void)fclose(infile);
  
	SET_POINTER(error_p, TABLE_ERROR_NONE);
	return table_p;
}

/*
 * int table_write
 *
 * DESCRIPTION:
 *
 * Write a table from memory to file.
 *
 * RETURNS:
 *
 * Success - TABLE_ERROR_NONE
 *
 * Failure - Table error code.
 *
 * ARGUMENTS:
 *
 * table_p - Pointer to the table that we are writing to the file.
 *
 * path - Table file to write out to.
 *
 * mode - Mode of the file.  This argument is passed on to open when
 * the file is created.
 */
int	table_write(const table_t *table_p, const char *path, const int mode)
{
	int		fd, rem, ent_size;
	unsigned int	bucket_c, bucket_size;
	unsigned long	size;
	table_entry_t	*entry_p, **buckets, **bucket_p, *next_p;
	table_t	main_tab;
	FILE		*outfile;
  
	if (table_p == NULL) {
		return TABLE_ERROR_ARG_NULL;
	}
	if (table_p->ta_magic != TABLE_MAGIC) {
		return TABLE_ERROR_PNT;
	}
  
	fd = open(path, O_WRONLY | O_CREAT, mode);
	if (fd < 0) {
		return TABLE_ERROR_OPEN;
	}
  
	outfile = fdopen(fd, "w");
	if (outfile == NULL) {
		return TABLE_ERROR_OPEN;
	}
  
	/* allocate a block of sizes for each bucket */
	bucket_size = sizeof(table_entry_t *) * table_p->ta_bucket_n;
	if (table_p->ta_alloc_func == NULL) {
		buckets = (table_entry_t **)malloc(bucket_size);
	}
	else {
		buckets =
			(table_entry_t **)table_p->ta_alloc_func(table_p->ta_mem_pool,
													 bucket_size);
	}
	if (buckets == NULL) {
		return TABLE_ERROR_ALLOC;
	}
  
	/* make a copy of the main struct */
	main_tab = *table_p;
  
	/* start counting the bytes */
	size = 0;
	size += sizeof(table_t);
  
	/* buckets go right after main struct */
	main_tab.ta_buckets = (table_entry_t **)size;
	size += sizeof(table_entry_t *) * table_p->ta_bucket_n;
  
	/* run through and count the buckets */
	for (bucket_c = 0; bucket_c < table_p->ta_bucket_n; bucket_c++) {
		bucket_p = table_p->ta_buckets + bucket_c;
		if (*bucket_p == NULL) {
			buckets[bucket_c] = NULL;
			continue;
		}
		buckets[bucket_c] = (table_entry_t *)size;
		for (entry_p = *bucket_p; entry_p != NULL; entry_p = entry_p->te_next_p) {
			size += entry_size(table_p, entry_p->te_key_size, entry_p->te_data_size);
			/*
			 * We now have to round the file to the nearest long so the
			 * mmaping of the longs in the entry structs will work.
			 */
			rem = size & (sizeof(long) - 1);
			if (rem > 0) {
				size += sizeof(long) - rem;
			}
		}
	}
	/* add a \0 at the end to fill the last section */
	size++;
  
	/* set the main fields */
	main_tab.ta_linear.tl_magic = 0;
	main_tab.ta_linear.tl_bucket_c = 0;
	main_tab.ta_linear.tl_entry_c = 0;
	main_tab.ta_mmap = NULL;
	main_tab.ta_file_size = size;
  
	/*
	 * Now we can start the writing because we got the bucket offsets.
	 */
  
	/* write the main table struct */
	size = 0;
	if (fwrite(&main_tab, sizeof(table_t), 1, outfile) != 1) {
		if (table_p->ta_free_func == NULL) {
			free(buckets);
		}
		else {
			(void)table_p->ta_free_func(table_p->ta_mem_pool, buckets, bucket_size);
		}
		return TABLE_ERROR_WRITE;
	}
	size += sizeof(table_t);
	if (fwrite(buckets, sizeof(table_entry_t *), table_p->ta_bucket_n,
			   outfile) != (size_t)table_p->ta_bucket_n) {
		if (table_p->ta_free_func == NULL) {
			free(buckets);
		}
		else {
			(void)table_p->ta_free_func(table_p->ta_mem_pool, buckets, bucket_size);
		}
		return TABLE_ERROR_WRITE;
	}
	size += sizeof(table_entry_t *) * table_p->ta_bucket_n;
  
	/* write out the entries */
	for (bucket_p = table_p->ta_buckets;
		 bucket_p < table_p->ta_buckets + table_p->ta_bucket_n;
		 bucket_p++) {
		for (entry_p = *bucket_p; entry_p != NULL; entry_p = entry_p->te_next_p) {
      
			ent_size = entry_size(table_p, entry_p->te_key_size,
								  entry_p->te_data_size);
			size += ent_size;
			/* round to nearest long here so we can write copy */
			rem = size & (sizeof(long) - 1);
			if (rem > 0) {
				size += sizeof(long) - rem;
			}
			next_p = entry_p->te_next_p;
			if (next_p != NULL) {
				entry_p->te_next_p = (table_entry_t *)size;
			}
      
			/* now write to disk */
			if (fwrite(entry_p, ent_size, 1, outfile) != 1) {
				if (table_p->ta_free_func == NULL) {
					free(buckets);
				}
				else {
					(void)table_p->ta_free_func(table_p->ta_mem_pool, buckets,
												bucket_size);
				}
				return TABLE_ERROR_WRITE;
			}
      
			/* restore the next pointer */
			if (next_p != NULL) {
				entry_p->te_next_p = next_p;
			}
      
			/* now write the padding information */
			if (rem > 0) {
				rem = sizeof(long) - rem;
				/*
				 * NOTE: this won't leave fseek'd space at the end but we
				 * don't care there because there is no accessed memory
				 * afterwards.  We write 1 \0 at the end to make sure.
				 */
				if (fseek(outfile, rem, SEEK_CUR) != 0) {
					if (table_p->ta_free_func == NULL) {
						free(buckets);
					}
					else {
						(void)table_p->ta_free_func(table_p->ta_mem_pool, buckets,
													bucket_size);
					}
					return TABLE_ERROR_SEEK;
				}
			}
		}
	}
	/*
	 * Write a \0 at the end of the file to make sure that the last
	 * fseek filled with nulls.
	 */
	(void)fputc('\0', outfile);
  
	(void)fclose(outfile);
	if (table_p->ta_free_func == NULL) {
		free(buckets);
	}
	else if (! table_p->ta_free_func(table_p->ta_mem_pool, buckets,
									 bucket_size)) {
		return TABLE_ERROR_FREE;
	}
  
	return TABLE_ERROR_NONE;
}

/******************************** table order ********************************/

/*
 * table_entry_t *table_order
 *
 * DESCRIPTION:
 *
 * Order a table by building an array of table entry pointers and then
 * sorting this array using the qsort function.  To retrieve the
 * sorted entries, you can then use the table_entry routine to access
 * each entry in order.
 *
 * NOTE: This routine is thread safe and makes use of an internal
 * status qsort function.
 *
 * RETURNS:
 *
 * Success - An allocated list of table-linear structures which must
 * be freed by table_order_free later.
 *
 * Failure - NULL
 *
 * ARGUMENTS:
 *
 * table_p - Pointer to the table that we are ordering.
 *
 * compare - Comparison function defined by the user.  Its definition
 * is at the top of the table.h file.  If this is NULL then it will
 * order the table my memcmp-ing the keys.
 *
 * num_entries_p - Pointer to an integer which, if not NULL, will
 * contain the number of entries in the returned entry pointer array.
 *
 * error_p - Pointer to an integer which, if not NULL, will contain a
 * table error code.
 */
table_entry_t	**table_order(table_t *table_p, table_compare_t compare,
							  int *num_entries_p, int *error_p)
{
	table_entry_t		*entry_p, **entries, **entries_p;
	table_linear_t	linear;
	compare_t		comp_func;
	unsigned int		entries_size;
	int			ret;
  
	if (table_p == NULL) {
		SET_POINTER(error_p, TABLE_ERROR_ARG_NULL);
		return NULL;
	}
	if (table_p->ta_magic != TABLE_MAGIC) {
		SET_POINTER(error_p, TABLE_ERROR_PNT);
		return NULL;
	}
  
	/* there must be at least 1 element in the table for this to work */
	if (table_p->ta_entry_n == 0) {
		SET_POINTER(error_p, TABLE_ERROR_EMPTY);
		return NULL;
	}
  
	entries_size = table_p->ta_entry_n * sizeof(table_entry_t *);
	if (table_p->ta_alloc_func == NULL) {
		entries = (table_entry_t **)malloc(entries_size);
	}
	else {
		entries =
			(table_entry_t **)table_p->ta_alloc_func(table_p->ta_mem_pool,
													 entries_size);
	}
	if (entries == NULL) {
		SET_POINTER(error_p, TABLE_ERROR_ALLOC);
		return NULL;
	}
  
	/* get a pointer to all entries */
	entry_p = first_entry(table_p, &linear);
	if (entry_p == NULL) {
		if (table_p->ta_free_func == NULL) {
			free(entries);
		}
		else {
			(void)table_p->ta_free_func(table_p->ta_mem_pool, entries, entries_size);
		}
		SET_POINTER(error_p, TABLE_ERROR_NOT_FOUND);
		return NULL;
	}
  
	/* add all of the entries to the array */
	for (entries_p = entries;
		 entry_p != NULL;
		 entry_p = next_entry(table_p, &linear, &ret)) {
		*entries_p++ = entry_p;
	}
  
	if (ret != TABLE_ERROR_NOT_FOUND) {
		if (table_p->ta_free_func == NULL) {
			free(entries);
		}
		else {
			(void)table_p->ta_free_func(table_p->ta_mem_pool, entries, entries_size);
		}
		SET_POINTER(error_p, ret);
		return NULL;
	}
  
	if (compare == NULL) {
		/* this is regardless of the alignment */
		comp_func = local_compare;
	}
	else if (table_p->ta_data_align == 0) {
		comp_func = external_compare;
	}
	else {
		comp_func = external_compare_align;
	}
  
	/* now qsort the entire entries array from first to last element */
	ret = split((unsigned char *)entries,
				(unsigned char *)(entries + table_p->ta_entry_n - 1),
				sizeof(table_entry_t *), comp_func, compare, table_p);
	if (ret != TABLE_ERROR_NONE) {
		if (table_p->ta_free_func == NULL) {
			free(entries);
		}
		else {
			(void)table_p->ta_free_func(table_p->ta_mem_pool, entries, entries_size);
		}
		SET_POINTER(error_p, ret);
		return NULL;
	}
  
	SET_POINTER(num_entries_p, table_p->ta_entry_n);
  
	SET_POINTER(error_p, TABLE_ERROR_NONE);
	return entries;
}

/*
 * int table_order_free
 *
 * DESCRIPTION:
 *
 * Free the pointer returned by the table_order or table_order_pos
 * routines.
 *
 * RETURNS:
 *
 * Success - TABLE_ERROR_NONE
 *
 * Failure - Table error code.
 *
 * ARGUMENTS:
 *
 * table_p - Pointer to the table.
 *
 * table_entries - Allocated list of entry pointers returned by
 * table_order.
 *
 * entry_n - Number of entries in the array as passed back by
 * table_order or table_order_pos in num_entries_p.
 */
int	table_order_free(table_t *table_p, table_entry_t **table_entries,
					 const int entry_n)
{
	int	ret, final = TABLE_ERROR_NONE;
  
	if (table_p == NULL) {
		return TABLE_ERROR_ARG_NULL;
	}
	if (table_p->ta_magic != TABLE_MAGIC) {
		return TABLE_ERROR_PNT;
	}
  
	if (table_p->ta_free_func == NULL) {
		free(table_entries);
	}
	else {
		ret = table_p->ta_free_func(table_p->ta_mem_pool, table_entries,
									sizeof(table_entry_t *) * entry_n);
		if (ret != 1) {
			final = TABLE_ERROR_FREE;
		}
	}
  
	return final;
}

/*
 * int table_entry
 *
 * DESCRIPTION:
 *
 * Get information about an element.  The element is one from the
 * array returned by the table_order function.  If any of the key/data
 * pointers are NULL then they are ignored.
 *
 * RETURNS:
 *
 * Success - TABLE_ERROR_NONE
 *
 * Failure - Table error code.
 *
 * ARGUMENTS:
 *
 * table_p - Table structure pointer from which we are getting the
 * element.
 *
 * entry_p - Pointer to a table entry from the array returned by the
 * table_order function.
 *
 * key_buf_p - Pointer which, if not NULL, will be set to the address
 * of the storage of this entry that is allocated in the table.  If an
 * (int) is stored as this entry (for example) then key_buf_p should
 * be (int **) i.e. the address of a (int *).
 *
 * key_size_p - Pointer to an integer which, if not NULL, will be set
 * to the size of the key that is stored in the table.
 *
 * data_buf_p - Pointer which, if not NULL, will be set to the address
 * of the data storage of this entry that is allocated in the table.
 * If a (long) is stored as this entry data (for example) then
 * data_buf_p should be (long **) i.e. the address of a (long *).
 *
 * data_size_p - Pointer to an integer which, if not NULL, will be set
 * to the size of the data that is stored in the table.
 */
int	table_entry(table_t *table_p, table_entry_t *entry_p,
				void **key_buf_p, int *key_size_p,
				void **data_buf_p, int *data_size_p)
{
	if (table_p == NULL) {
		return TABLE_ERROR_ARG_NULL;
	}
	if (table_p->ta_magic != TABLE_MAGIC) {
		return TABLE_ERROR_PNT;
	}
	if (entry_p == NULL) {
		return TABLE_ERROR_ARG_NULL;
	}
  
	SET_POINTER(key_buf_p, ENTRY_KEY_BUF(entry_p));
	SET_POINTER(key_size_p, entry_p->te_key_size);
	if (data_buf_p != NULL) {
		if (entry_p->te_data_size == 0) {
			*data_buf_p = NULL;
		}
		else {
			if (table_p->ta_data_align == 0) {
				*data_buf_p = ENTRY_DATA_BUF(table_p, entry_p);
			}
			else {
				*data_buf_p = entry_data_buf(table_p, entry_p);
			}
		}
	}
	SET_POINTER(data_size_p, entry_p->te_data_size);
  
	return TABLE_ERROR_NONE;
}

/*
 * table_linear_t *table_order_pos
 *
 * DESCRIPTION:
 *
 * Order a table by building an array of table linear structures and
 * then sorting this array using the qsort function.  To retrieve the
 * sorted entries, you can then use the table_entry_pos routine to
 * access each entry in order.
 *
 * NOTE: This routine is thread safe and makes use of an internal
 * status qsort function.
 *
 * RETURNS:
 *
 * Success - An allocated list of table-linear structures which must
 * be freed by table_order_pos_free later.
 *
 * Failure - NULL
 *
 * ARGUMENTS:
 *
 * table_p - Pointer to the table that we are ordering.
 *
 * compare - Comparison function defined by the user.  Its definition
 * is at the top of the table.h file.  If this is NULL then it will
 * order the table my memcmp-ing the keys.
 *
 * num_entries_p - Pointer to an integer which, if not NULL, will
 * contain the number of entries in the returned entry pointer array.
 *
 * error_p - Pointer to an integer which, if not NULL, will contain a
 * table error code.
 */
table_linear_t	*table_order_pos(table_t *table_p, table_compare_t compare,
								 int *num_entries_p, int *error_p)
{
	table_entry_t		*entry_p;
	table_linear_t	linear, *linears, *linears_p;
	compare_t		comp_func;
	int			ret;
  
	if (table_p == NULL) {
		SET_POINTER(error_p, TABLE_ERROR_ARG_NULL);
		return NULL;
	}
	if (table_p->ta_magic != TABLE_MAGIC) {
		SET_POINTER(error_p, TABLE_ERROR_PNT);
		return NULL;
	}
  
	/* there must be at least 1 element in the table for this to work */
	if (table_p->ta_entry_n == 0) {
		SET_POINTER(error_p, TABLE_ERROR_EMPTY);
		return NULL;
	}
  
	if (table_p->ta_alloc_func == NULL) {
		linears = (table_linear_t *)malloc(table_p->ta_entry_n *
										   sizeof(table_linear_t));
	}
	else {
		linears =
			(table_linear_t *)table_p->ta_alloc_func(table_p->ta_mem_pool,
													 table_p->ta_entry_n *
													 sizeof(table_linear_t));
	}
	if (linears == NULL) {
		SET_POINTER(error_p, TABLE_ERROR_ALLOC);
		return NULL;
	}
  
	/* get a pointer to all entries */
	entry_p = first_entry(table_p, &linear);
	if (entry_p == NULL) {
		SET_POINTER(error_p, TABLE_ERROR_NOT_FOUND);
		return NULL;
	}
  
	/* add all of the entries to the array */
	for (linears_p = linears;
		 entry_p != NULL;
		 entry_p = next_entry(table_p, &linear, &ret)) {
		*linears_p++ = linear;
	}
  
	if (ret != TABLE_ERROR_NOT_FOUND) {
		SET_POINTER(error_p, ret);
		return NULL;
	}
  
	if (compare == NULL) {
		/* this is regardless of the alignment */
		comp_func = local_compare_pos;
	}
	else if (table_p->ta_data_align == 0) {
		comp_func = external_compare_pos;
	}
	else {
		comp_func = external_compare_align_pos;
	}
  
	/* now qsort the entire entries array from first to last element */
	split((unsigned char *)linears,
		  (unsigned char *)(linears + table_p->ta_entry_n - 1),
		  sizeof(table_linear_t),	comp_func, compare, table_p);
  
	if (num_entries_p != NULL) {
		*num_entries_p = table_p->ta_entry_n;
	}
  
	SET_POINTER(error_p, TABLE_ERROR_NONE);
	return linears;
}

/*
 * int table_order_pos_free
 *
 * DESCRIPTION:
 *
 * Free the pointer returned by the table_order or table_order_pos
 * routines.
 *
 * RETURNS:
 *
 * Success - TABLE_ERROR_NONE
 *
 * Failure - Table error code.
 *
 * ARGUMENTS:
 *
 * table_p - Pointer to the table.
 *
 * table_entries - Allocated list of entry pointers returned by
 * table_order_pos.
 *
 * entry_n - Number of entries in the array as passed back by
 * table_order or table_order_pos in num_entries_p.
 */
int	table_order_pos_free(table_t *table_p, table_linear_t *table_entries,
						 const int entry_n)
{
	int	ret, final = TABLE_ERROR_NONE;
  
	if (table_p == NULL) {
		return TABLE_ERROR_ARG_NULL;
	}
	if (table_p->ta_magic != TABLE_MAGIC) {
		return TABLE_ERROR_PNT;
	}
  
	if (table_p->ta_free_func == NULL) {
		free(table_entries);
	}
	else {
		ret = table_p->ta_free_func(table_p->ta_mem_pool, table_entries,
									sizeof(table_linear_t) * entry_n);
		if (ret != 1) {
			final = TABLE_ERROR_FREE;
		}
	}
  
	return final;
}

/*
 * int table_entry_pos
 *
 * DESCRIPTION:
 *
 * Get information about an element.  The element is one from the
 * array returned by the table_order function.  If any of the key/data
 * pointers are NULL then they are ignored.
 *
 * RETURNS:
 *
 * Success - TABLE_ERROR_NONE
 *
 * Failure - Table error code.
 *
 * ARGUMENTS:
 *
 * table_p - Table structure pointer from which we are getting the
 * element.
 *
 * linear_p - Pointer to a table linear structure from the array
 * returned by the table_order function.
 *
 * key_buf_p - Pointer which, if not NULL, will be set to the address
 * of the storage of this entry that is allocated in the table.  If an
 * (int) is stored as this entry (for example) then key_buf_p should
 * be (int **) i.e. the address of a (int *).
 *
 * key_size_p - Pointer to an integer which, if not NULL, will be set
 * to the size of the key that is stored in the table.
 *
 * data_buf_p - Pointer which, if not NULL, will be set to the address
 * of the data storage of this entry that is allocated in the table.
 * If a (long) is stored as this entry data (for example) then
 * data_buf_p should be (long **) i.e. the address of a (long *).
 *
 * data_size_p - Pointer to an integer which, if not NULL, will be set
 * to the size of the data that is stored in the table.
 */
int	table_entry_pos(table_t *table_p, table_linear_t *linear_p,
					void **key_buf_p, int *key_size_p,
					void **data_buf_p, int *data_size_p)
{
	table_entry_t		*entry_p;
	int			ret;
  
	if (table_p == NULL) {
		return TABLE_ERROR_ARG_NULL;
	}
	if (table_p->ta_magic != TABLE_MAGIC) {
		return TABLE_ERROR_PNT;
	}
	if (linear_p == NULL) {
		return TABLE_ERROR_ARG_NULL;
	}
  
	/* find the associated entry */
	entry_p = this_entry(table_p, linear_p, &ret);
	if (entry_p == NULL) {
		return ret;
	}
  
	if (key_buf_p != NULL) {
		*key_buf_p = ENTRY_KEY_BUF(entry_p);
	}
	if (key_size_p != NULL) {
		*key_size_p = entry_p->te_key_size;
	}
	if (data_buf_p != NULL) {
		if (entry_p->te_data_size == 0) {
			*data_buf_p = NULL;
		}
		else {
			if (table_p->ta_data_align == 0) {
				*data_buf_p = ENTRY_DATA_BUF(table_p, entry_p);
			}
			else {
				*data_buf_p = entry_data_buf(table_p, entry_p);
			}
		}
	}
	if (data_size_p != NULL) {
		*data_size_p = entry_p->te_data_size;
	}
  
	return TABLE_ERROR_NONE;
}

/*
 * const char *table_strerror
 *
 * DESCRIPTION:
 *
 * Return the corresponding string for the error number.
 *
 * RETURNS:
 *
 * Success - String equivalient of the error.
 *
 * Failure - String "invalid error code"
 *
 * ARGUMENTS:
 *
 * error - Error number that we are converting.
 */
const char	*table_strerror(const int error)
{
	error_str_t	*err_p;
  
	for (err_p = errors; err_p->es_error != 0; err_p++) {
		if (err_p->es_error == error) {
			return err_p->es_string;
		}
	}
  
	return INVALID_ERROR;
}

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
