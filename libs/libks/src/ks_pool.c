/*
 * Memory pool routines.
 *
 * Copyright 1996 by Gray Watson.
 *
 * This file is part of the ks_mpool package.
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
 * $Id: ks_mpool.c,v 1.5 2006/05/31 20:28:31 gray Exp $
 */

/*
 * Memory-pool allocation routines.  I got sick of the GNU mmalloc
 * library which was close to what we needed but did not exactly do
 * what I wanted.
 *
 * The following uses mmap from /dev/zero.  It allows a number of
 * allocations to be made inside of a memory pool then with a clear or
 * close the pool can be reset without any memory fragmentation and
 * growth problems.
 */

#include "ks.h"
#include <sys/mman.h>

#define KS_POOL_MAGIC		 0xABACABA	/* magic for struct */
#define BLOCK_MAGIC			 0xB1B1007	/* magic for blocks */
#define FENCE_MAGIC0		 (unsigned char)(0xFAU)	/* 1st magic mem byte */
#define FENCE_MAGIC1		 (unsigned char)(0xD3U)	/* 2nd magic mem byte */

#define FENCE_SIZE			 2		/* fence space */
#define MIN_ALLOCATION		 (sizeof(ks_pool_free_t))	/* min alloc */
#define MAX_FREE_SEARCH		 10240	/* max size to search */
#define MAX_FREE_LIST_SEARCH 100	/* max looking for free mem */

#define PRE_MAGIC1 0x33U
#define PRE_MAGIC2 0xCCU

typedef struct alloc_prefix_s {
	unsigned char m1;
	unsigned long size;
	unsigned char m2;
} alloc_prefix_t;

#define PREFIX_SIZE sizeof(struct alloc_prefix_s)

/*
 * bitflag tools for Variable and a Flag
 */
#define BIT_FLAG(x)			(1 << (x))
#define BIT_SET(v,f)		(v) |= (f)
#define BIT_CLEAR(v,f)		(v) &= ~(f)
#define BIT_IS_SET(v,f)		((v) & (f))
#define BIT_TOGGLE(v,f)		(v) ^= (f)

#define SET_POINTER(pnt, val)					\
	do {										\
		if ((pnt) != NULL) {					\
			(*(pnt)) = (val);					\
		}										\
	} while(0)

#define BLOCK_FLAG_USED		BIT_FLAG(0)	/* block is used */
#define BLOCK_FLAG_FREE		BIT_FLAG(1)	/* block is free */

#define DEFAULT_PAGE_MULT		16	/* pagesize = this * getpagesize */

/* How many pages SIZE bytes resides in.  We add in the block header. */
#define PAGES_IN_SIZE(mp_p, size)	(((size) + sizeof(ks_pool_block_t) +	\
									  (mp_p)->mp_page_size - 1) /		\
									 (mp_p)->mp_page_size)
#define SIZE_OF_PAGES(mp_p, page_n)	((page_n) * (mp_p)->mp_page_size)
#define MAX_BITS	30			/* we only can allocate 1gb chunks */

#define MAX_BLOCK_USER_MEMORY(mp_p)	((mp_p)->mp_page_size - \
									 sizeof(ks_pool_block_t))
#define FIRST_ADDR_IN_BLOCK(block_p)	(void *)((char *)(block_p) +	\
												 sizeof(ks_pool_block_t))
#define MEMORY_IN_BLOCK(block_p)	((char *)(block_p)->mb_bounds_p -	\
									 ((char *)(block_p) +				\
									  sizeof(ks_pool_block_t)))

typedef struct ks_pool_cleanup_node_s {
	ks_pool_cleanup_fn_t fn;
	void *ptr;
	void *arg;
	int type;
	struct ks_pool_cleanup_node_s *next;
} ks_pool_cleanup_node_t;

struct ks_pool_s {
	unsigned int mp_magic;		/* magic number for struct */
	unsigned int mp_flags;		/* flags for the struct */
	unsigned long mp_alloc_c;	/* number of allocations */
	unsigned long mp_user_alloc;	/* user bytes allocated */
	unsigned long mp_max_alloc;	/* maximum user bytes allocated */
	unsigned int mp_page_c;		/* number of pages allocated */
	unsigned int mp_max_pages;	/* maximum number of pages to use */
	unsigned int mp_page_size;	/* page-size of our system */
	off_t mp_top;				/* top of our allocations in fd */
	ks_pool_log_func_t mp_log_func;	/* log callback function */
	void *mp_addr;				/* current address for mmaping */
	void *mp_min_p;				/* min address in pool for checks */
	void *mp_bounds_p;			/* max address in pool for checks */
	struct ks_pool_block_st *mp_first_p;	/* first memory block we are using */
	struct ks_pool_block_st *mp_last_p;	/* last memory block we are using */
	struct ks_pool_block_st *mp_free[MAX_BITS + 1];	/* free lists based on size */
	unsigned int mp_magic2;		/* upper magic for overwrite sanity */
	ks_pool_cleanup_node_t *clfn_list;
	ks_mutex_t *mutex;
	ks_mutex_t *cleanup_mutex;
	uint8_t cleaning_up;
};

/* for debuggers to be able to interrogate the generic type in the .h file */
typedef ks_pool_t ks_pool_ext_t;

/*
 * Block header structure.  This structure *MUST* be long-word
 * aligned.
 */
typedef struct ks_pool_block_st {
	unsigned int mb_magic;		/* magic number for block header */
	void *mb_bounds_p;			/* block boundary location */
	struct ks_pool_block_st *mb_next_p;	/* linked list next pointer */
	unsigned int mb_magic2;		/* upper magic for overwrite sanity */
} ks_pool_block_t;

/*
 * Free list structure.
 */
typedef struct {
	void *mf_next_p;			/* pointer to the next free address */
	unsigned long mf_size;		/* size of the free block */
} ks_pool_free_t;

#ifndef MAP_ANON
#define MAP_ANON MAP_ANONYMOUS
#endif

/* local variables */
static int enabled_b = 0;		/* lib initialized? */
static unsigned int min_bit_free_next = 0;	/* min size of next pnt */
static unsigned int min_bit_free_size = 0;	/* min size of next + size */
static unsigned long bit_array[MAX_BITS + 1];	/* size -> bit */

#ifdef _MSC_VER
#include <Windows.h>
long getpagesize(void)
{
	static long g_pagesize = 0;
	if (!g_pagesize) {
		SYSTEM_INFO system_info;
		GetSystemInfo(&system_info);
		g_pagesize = system_info.dwPageSize;
	}
	return g_pagesize;
}
#endif


/* We need mutex here probably or this notion of cleanup stuff cannot be threadsafe */

#if 0
static ks_pool_cleanup_node_t *find_cleanup_node(ks_pool_t *mp_p, void *ptr)
{
	ks_pool_cleanup_node_t *np, *cnode = NULL;
	
	ks_assert(mp_p);
	ks_assert(ptr);

	for (np = mp_p->clfn_list; np; np = np->next) {
		if (np->ptr == ptr) {
			cnode = np;
			goto end;
		}
	}

 end:

	/* done, the nodes are all from the pool so they will be destroyed */
	return cnode;
}
#endif

static void perform_pool_cleanup_on_free(ks_pool_t *mp_p, void *ptr)
{
	ks_pool_cleanup_node_t *np, *cnode, *last = NULL;

	np = mp_p->clfn_list;

	ks_mutex_lock(mp_p->mutex);
	if (mp_p->cleaning_up) {
		ks_mutex_unlock(mp_p->mutex);
		return;
	}
	ks_mutex_unlock(mp_p->mutex);

	ks_mutex_lock(mp_p->cleanup_mutex);
	while(np) {
		if (np->ptr == ptr) {
			if (last) {
				last->next = np->next;
			} else {
				mp_p->clfn_list = np->next;
			}

			cnode = np;
			np = np->next;
			cnode->fn(mp_p, cnode->ptr, cnode->arg, cnode->type, KS_MPCL_ANNOUNCE, KS_MPCL_FREE);
			cnode->fn(mp_p, cnode->ptr, cnode->arg, cnode->type, KS_MPCL_TEARDOWN, KS_MPCL_FREE);
			cnode->fn(mp_p, cnode->ptr, cnode->arg, cnode->type, KS_MPCL_DESTROY, KS_MPCL_FREE);

			continue;
		}
		last = np;
		np = np->next;
	}
	ks_mutex_unlock(mp_p->cleanup_mutex);
}

static void perform_pool_cleanup(ks_pool_t *mp_p)
{
	ks_pool_cleanup_node_t *np;

	ks_mutex_lock(mp_p->mutex);
	if (mp_p->cleaning_up) {
		ks_mutex_unlock(mp_p->mutex);
		return;
	}
	mp_p->cleaning_up = 1;
	ks_mutex_unlock(mp_p->mutex);

	ks_mutex_lock(mp_p->cleanup_mutex);
	for (np = mp_p->clfn_list; np; np = np->next) {
		np->fn(mp_p, np->ptr, np->arg, np->type, KS_MPCL_ANNOUNCE, KS_MPCL_GLOBAL_FREE);
	}

	for (np = mp_p->clfn_list; np; np = np->next) {
		np->fn(mp_p, np->ptr, np->arg, np->type, KS_MPCL_TEARDOWN, KS_MPCL_GLOBAL_FREE);
	}

	for (np = mp_p->clfn_list; np; np = np->next) {
		np->fn(mp_p, np->ptr, np->arg, np->type, KS_MPCL_DESTROY, KS_MPCL_GLOBAL_FREE);
	}
	ks_mutex_unlock(mp_p->cleanup_mutex);

	mp_p->clfn_list = NULL;
}

KS_DECLARE(ks_status_t) ks_pool_set_cleanup(ks_pool_t *mp_p, void *ptr, void *arg, int type, ks_pool_cleanup_fn_t fn)
{
	ks_pool_cleanup_node_t *cnode;

	ks_assert(mp_p);
	ks_assert(ptr);
	ks_assert(fn);

	/* don't set cleanup on this cnode obj or it will be an endless loop */
	cnode = (ks_pool_cleanup_node_t *) ks_pool_alloc(mp_p, sizeof(*cnode));
	
	if (!cnode) {
		return KS_STATUS_FAIL;
	}

	cnode->ptr = ptr;
	cnode->arg = arg;
	cnode->fn = fn;
	cnode->type = type;

	ks_mutex_lock(mp_p->cleanup_mutex);
	cnode->next = mp_p->clfn_list;
	mp_p->clfn_list = cnode;
	ks_mutex_unlock(mp_p->cleanup_mutex);

	return KS_STATUS_SUCCESS;
}



/****************************** local utilities ******************************/

/*
 * static void startup
 *
 * DESCRIPTION:
 *
 * Perform any library level initialization.
 *
 * RETURNS:
 *
 * None.
 *
 * ARGUMENTS:
 *
 * None.
 */
static void startup(void)
{
	int bit_c;
	unsigned long size = 1;

	if (enabled_b) {
		return;
	}

	/* allocate our free bit array list */
	for (bit_c = 0; bit_c <= MAX_BITS; bit_c++) {
		bit_array[bit_c] = size;

		/*
		 * Note our minimum number of bits that can store a pointer.  This
		 * is smallest address that we can have a linked list for.
		 */
		if (min_bit_free_next == 0 && size >= sizeof(void *)) {
			min_bit_free_next = bit_c;
		}
		/*
		 * Note our minimum number of bits that can store a pointer and
		 * the size of the block.
		 */
		if (min_bit_free_size == 0 && size >= sizeof(ks_pool_free_t)) {
			min_bit_free_size = bit_c;
		}

		size *= 2;
	}

	enabled_b = 1;
}

/*
 * static int size_to_bits
 *
 * DESCRIPTION:
 *
 * Calculate the number of bits in a size.
 *
 * RETURNS:
 *
 * Number of bits.
 *
 * ARGUMENTS:
 *
 * size -> Size of memory of which to calculate the number of bits.
 */
static int size_to_bits(const unsigned long size)
{
	int bit_c = 0;

	for (bit_c = 0; bit_c <= MAX_BITS; bit_c++) {
		if (size <= bit_array[bit_c]) {
			break;
		}
	}

	return bit_c;
}

/*
 * static int size_to_free_bits
 *
 * DESCRIPTION:
 *
 * Calculate the number of bits in a size going on the free list.
 *
 * RETURNS:
 *
 * Number of bits.
 *
 * ARGUMENTS:
 *
 * size -> Size of memory of which to calculate the number of bits.
 */
static int size_to_free_bits(const unsigned long size)
{
	int bit_c = 0;

	if (size == 0) {
		return 0;
	}

	for (bit_c = 0; bit_c <= MAX_BITS; bit_c++) {
		if (size < bit_array[bit_c]) {
			break;
		}
	}

	return bit_c - 1;
}

/*
 * static int bits_to_size
 *
 * DESCRIPTION:
 *
 * Calculate the size represented by a number of bits.
 *
 * RETURNS:
 *
 * Number of bits.
 *
 * ARGUMENTS:
 *
 * bit_n -> Number of bits
 */
static unsigned long bits_to_size(const int bit_n)
{
	if (bit_n > MAX_BITS) {
		return bit_array[MAX_BITS];
	} else {
		return bit_array[bit_n];
	}
}

/*
 * static void *alloc_pages
 *
 * DESCRIPTION:
 *
 * Allocate space for a number of memory pages in the memory pool.
 *
 * RETURNS:
 *
 * Success - New pages of memory
 *
 * Failure - NULL
 *
 * ARGUMENTS:
 *
 * mp_p <-> Pointer to our memory pool.
 *
 * page_n -> Number of pages to alloc.
 *
 * error_p <- Pointer to ks_status_t which, if not NULL, will be set with
 * a ks_pool error code.
 */
static void *alloc_pages(ks_pool_t *mp_p, const unsigned int page_n, ks_status_t *error_p)
{
	void *mem;
	unsigned long size;
	int state;

	/* are we over our max-pages? */
	if (mp_p->mp_max_pages > 0 && mp_p->mp_page_c >= mp_p->mp_max_pages) {
		SET_POINTER(error_p, KS_STATUS_NO_PAGES);
		return NULL;
	}

	size = SIZE_OF_PAGES(mp_p, page_n);

#ifdef DEBUG
	(void) printf("allocating %u pages or %lu bytes\n", page_n, size);
#endif


	state = MAP_PRIVATE | MAP_ANON;

#if defined(MAP_FILE)
	state |= MAP_FILE;
#endif

#if defined(MAP_VARIABLE)
	state |= MAP_VARIABLE;
#endif

	/* mmap from /dev/zero */
	mem = mmap(mp_p->mp_addr, size, PROT_READ | PROT_WRITE, state, -1, mp_p->mp_top);

	if (mem == (void *) MAP_FAILED) {
		if (errno == ENOMEM) {
			SET_POINTER(error_p, KS_STATUS_NO_MEM);
		} else {
			SET_POINTER(error_p, KS_STATUS_MMAP);
		}
		return NULL;
	}

	mp_p->mp_top += size;

	if (mp_p->mp_addr != NULL) {
		mp_p->mp_addr = (char *) mp_p->mp_addr + size;
	}

	mp_p->mp_page_c += page_n;

	SET_POINTER(error_p, KS_STATUS_SUCCESS);
	return mem;
}

/*
 * static int free_pages
 *
 * DESCRIPTION:
 *
 * Free previously allocated pages of memory.
 *
 * RETURNS:
 *
 * Success - KS_STATUS_SUCCESS
 *
 * Failure - Ks_Pool error code
 *
 * ARGUMENTS:
 *
 * pages <-> Pointer to memory pages that we are freeing.
 *
 * size -> Size of the block that we are freeing.
 *
 * sbrk_b -> Set to one if the pages were allocated with sbrk else mmap.
 */
static int free_pages(void *pages, const unsigned long size)
{
	(void) munmap(pages, size);
	return KS_STATUS_SUCCESS;
}

/*
 * static int check_magic
 *
 * DESCRIPTION:
 *
 * Check for the existance of the magic ID in a memory pointer.
 *
 * RETURNS:
 *
 * Success - KS_STATUS_SUCCESS
 *
 * Failure - Ks_Pool error code
 *
 * ARGUMENTS:
 *
 * addr -> Address inside of the block that we are tryign to locate.
 *
 * size -> Size of the block.
 */
static int check_magic(const void *addr, const unsigned long size)
{
	const unsigned char *mem_p;

	/* set our starting point */
	mem_p = (unsigned char *) addr + size;

	if (*mem_p == FENCE_MAGIC0 && *(mem_p + 1) == FENCE_MAGIC1) {
		return KS_STATUS_SUCCESS;
	} else {
		return KS_STATUS_PNT_OVER;
	}
}

/*
 * static void write_magic
 *
 * DESCRIPTION:
 *
 * Write the magic ID to the address.
 *
 * RETURNS:
 *
 * None.
 *
 * ARGUMENTS:
 *
 * addr -> Address where to write the magic.
 */
static void write_magic(const void *addr)
{
	*(unsigned char *) addr = FENCE_MAGIC0;
	*((unsigned char *) addr + 1) = FENCE_MAGIC1;
}

/*
 * static void free_pointer
 *
 * DESCRIPTION:
 *
 * Moved a pointer into our free lists.
 *
 * RETURNS:
 *
 * Success - KS_STATUS_SUCCESS
 *
 * Failure - Ks_Pool error code
 *
 * ARGUMENTS:
 *
 * mp_p <-> Pointer to the memory pool.
 *
 * addr <-> Address where to write the magic.  We may write a next
 * pointer to it.
 *
 * size -> Size of the address space.
 */
static int free_pointer(ks_pool_t *mp_p, void *addr, const unsigned long size)
{
	unsigned int bit_n;
	unsigned long real_size;
	ks_pool_free_t free_pnt;

#ifdef DEBUG
	(void) printf("freeing a block at %lx of %lu bytes\n", (long) addr, size);
#endif

	if (size == 0) {
		return KS_STATUS_SUCCESS;
	}

	/*
	 * if the user size is larger then can fit in an entire block then
	 * we change the size
	 */
	if (size > MAX_BLOCK_USER_MEMORY(mp_p)) {
		real_size = SIZE_OF_PAGES(mp_p, PAGES_IN_SIZE(mp_p, size)) - sizeof(ks_pool_block_t);
	} else {
		real_size = size;
	}

	/*
	 * We use a specific free bits calculation here because if we are
	 * freeing 10 bytes then we will be putting it into the 8-byte free
	 * list and not the 16 byte list.  size_to_bits(10) will return 4
	 * instead of 3.
	 */
	bit_n = size_to_free_bits(real_size);

	/*
	 * Minimal error checking.  We could go all the way through the
	 * list however this might be prohibitive.
	 */
	if (mp_p->mp_free[bit_n] == addr) {
		return KS_STATUS_IS_FREE;
	}

	/* add the freed pointer to the free list */
	if (bit_n < min_bit_free_next) {
		/*
		 * Yes we know this will lose 99% of the allocations but what else
		 * can we do?  No space for a next pointer.
		 */
		if (mp_p->mp_free[bit_n] == NULL) {
			mp_p->mp_free[bit_n] = addr;
		}
	} else if (bit_n < min_bit_free_size) {
		/* we copy, not assign, to maintain the free list */
		memcpy(addr, mp_p->mp_free + bit_n, sizeof(void *));
		mp_p->mp_free[bit_n] = addr;
	} else {

		/* setup our free list structure */
		free_pnt.mf_next_p = mp_p->mp_free[bit_n];
		free_pnt.mf_size = real_size;

		/* we copy the structure in since we don't know about alignment */
		memcpy(addr, &free_pnt, sizeof(free_pnt));
		mp_p->mp_free[bit_n] = addr;
	}

	return KS_STATUS_SUCCESS;
}

/*
 * static int split_block
 *
 * DESCRIPTION:
 *
 * When freeing space in a multi-block chunk we have to create new
 * blocks out of the upper areas being freed.
 *
 * RETURNS:
 *
 * Success - KS_STATUS_SUCCESS
 *
 * Failure - Ks_Pool error code
 *
 * ARGUMENTS:
 *
 * mp_p <-> Pointer to the memory pool.
 *
 * free_addr -> Address that we are freeing.
 *
 * size -> Size of the space that we are taking from address.
 */
static int split_block(ks_pool_t *mp_p, void *free_addr, const unsigned long size)
{
	ks_pool_block_t *block_p, *new_block_p;
	int ret, page_n;
	void *end_p;

	/*
	 * 1st we find the block pointer from our free addr.  At this point
	 * the pointer must be the 1st one in the block if it is spans
	 * multiple blocks.
	 */
	block_p = (ks_pool_block_t *) ((char *) free_addr - sizeof(ks_pool_block_t));
	if (block_p->mb_magic != BLOCK_MAGIC || block_p->mb_magic2 != BLOCK_MAGIC) {
		return KS_STATUS_POOL_OVER;
	}

	page_n = PAGES_IN_SIZE(mp_p, size);

	/* we are creating a new block structure for the 2nd ... */
	new_block_p = (ks_pool_block_t *) ((char *) block_p + SIZE_OF_PAGES(mp_p, page_n));
	new_block_p->mb_magic = BLOCK_MAGIC;
	/* New bounds is 1st block bounds.  The 1st block's is reset below. */
	new_block_p->mb_bounds_p = block_p->mb_bounds_p;
	/* Continue the linked list.  The 1st block will point to us below. */
	new_block_p->mb_next_p = block_p->mb_next_p;
	new_block_p->mb_magic2 = BLOCK_MAGIC;

	/* bounds for the 1st block are reset to the 1st page only */
	block_p->mb_bounds_p = (char *) new_block_p;
	/* the next block pointer for the 1st block is now the new one */
	block_p->mb_next_p = new_block_p;

	/* only free the space in the 1st block if it is only 1 block in size */
	if (page_n == 1) {
		/* now free the rest of the 1st block block */
		end_p = (char *) free_addr + size;
		ret = free_pointer(mp_p, end_p, (unsigned long)((char *) block_p->mb_bounds_p - (char *) end_p));
		if (ret != KS_STATUS_SUCCESS) {
			return ret;
		}
	}

	/* now free the rest of the block */
	ret = free_pointer(mp_p, FIRST_ADDR_IN_BLOCK(new_block_p), (unsigned long)MEMORY_IN_BLOCK(new_block_p));
	if (ret != KS_STATUS_SUCCESS) {
		return ret;
	}

	return KS_STATUS_SUCCESS;
}

/*
 * static void *get_space
 *
 * DESCRIPTION:
 *
 * Moved a pointer into our free lists.
 *
 * RETURNS:
 *
 * Success - New address that we can use.
 *
 * Failure - NULL
 *
 * ARGUMENTS:
 *
 * mp_p <-> Pointer to the memory pool.
 *
 * byte_size -> Size of the address space that we need.
 *
 * error_p <- Pointer to ks_status_t which, if not NULL, will be set with
 * a ks_pool error code.
 */
static void *get_space(ks_pool_t *mp_p, const unsigned long byte_size, ks_status_t *error_p)
{
	ks_pool_block_t *block_p;
	ks_pool_free_t free_pnt;
	int ret;
	unsigned long size;
	unsigned int bit_c, page_n, left;
	void *free_addr = NULL, *free_end;

	size = byte_size;
	while ((size & (sizeof(void *) - 1)) > 0) {
		size++;
	}

	/*
	 * First we check the free lists looking for something with enough
	 * pages.  Maybe we should only look X bits higher in the list.
	 *
	 * XXX: this is where we'd do the best fit.  We'd look for the
	 * closest match.  We then could put the rest of the allocation that
	 * we did not use in a lower free list.  Have a define which states
	 * how deep in the free list to go to find the closest match.
	 */
	for (bit_c = size_to_bits(size); bit_c <= MAX_BITS; bit_c++) {
		if (mp_p->mp_free[bit_c] != NULL) {
			free_addr = mp_p->mp_free[bit_c];
			break;
		}
	}

	/*
	 * If we haven't allocated any blocks or if the last block doesn't
	 * have enough memory then we need a new block.
	 */
	if (bit_c > MAX_BITS) {

		/* we need to allocate more space */

		page_n = PAGES_IN_SIZE(mp_p, size);

		/* now we try and get the pages we need/want */
		block_p = alloc_pages(mp_p, page_n, error_p);
		if (block_p == NULL) {
			/* error_p set in alloc_pages */
			return NULL;
		}

		/* init the block header */
		block_p->mb_magic = BLOCK_MAGIC;
		block_p->mb_bounds_p = (char *) block_p + SIZE_OF_PAGES(mp_p, page_n);
		block_p->mb_next_p = mp_p->mp_first_p;
		block_p->mb_magic2 = BLOCK_MAGIC;

		/*
		 * We insert it into the front of the queue.  We could add it to
		 * the end but there is not much use.
		 */
		mp_p->mp_first_p = block_p;
		if (mp_p->mp_last_p == NULL) {
			mp_p->mp_last_p = block_p;
		}

		free_addr = FIRST_ADDR_IN_BLOCK(block_p);

#ifdef DEBUG
		(void) printf("had to allocate space for %lx of %lu bytes\n", (long) free_addr, size);
#endif

		free_end = (char *) free_addr + size;
		left = (unsigned) ((char *) block_p->mb_bounds_p - (char *) free_end);
	} else {

		if (bit_c < min_bit_free_next) {
			mp_p->mp_free[bit_c] = NULL;
			/* calculate the number of left over bytes */
			left = bits_to_size(bit_c) - size;
		} else if (bit_c < min_bit_free_next) {
			/* grab the next pointer from the freed address into our list */
			memcpy(mp_p->mp_free + bit_c, free_addr, sizeof(void *));
			/* calculate the number of left over bytes */
			left = bits_to_size(bit_c) - size;
		} else {
			/* grab the free structure from the address */
			memcpy(&free_pnt, free_addr, sizeof(free_pnt));
			mp_p->mp_free[bit_c] = free_pnt.mf_next_p;

			/* are we are splitting up a multiblock chunk into fewer blocks? */
			if (PAGES_IN_SIZE(mp_p, free_pnt.mf_size) > PAGES_IN_SIZE(mp_p, size)) {
				ret = split_block(mp_p, free_addr, size);
				if (ret != KS_STATUS_SUCCESS) {
					SET_POINTER(error_p, ret);
					return NULL;
				}
				/* left over memory was taken care of in split_block */
				left = 0;
			} else {
				/* calculate the number of left over bytes */
				left = free_pnt.mf_size - size;
			}
		}

#ifdef DEBUG
		(void) printf("found a free block at %lx of %lu bytes\n", (long) free_addr, left + size);
#endif

		free_end = (char *) free_addr + size;
	}

	/*
	 * If we have memory left over then we free it so someone else can
	 * use it.  We do not free the space if we just allocated a
	 * multi-block chunk because we need to have every allocation easily
	 * find the start of the block.  Every user address % page-size
	 * should take us to the start of the block.
	 */
	if (left > 0 && size <= MAX_BLOCK_USER_MEMORY(mp_p)) {
		/* free the rest of the block */
		ret = free_pointer(mp_p, free_end, left);
		if (ret != KS_STATUS_SUCCESS) {
			SET_POINTER(error_p, ret);
			return NULL;
		}
	}

	/* update our bounds */
	if (free_addr > mp_p->mp_bounds_p) {
		mp_p->mp_bounds_p = free_addr;
	} else if (free_addr < mp_p->mp_min_p) {
		mp_p->mp_min_p = free_addr;
	}

	return free_addr;
}

/*
 * static void *alloc_mem
 *
 * DESCRIPTION:
 *
 * Allocate space for bytes inside of an already open memory pool.
 *
 * RETURNS:
 *
 * Success - Pointer to the address to use.
 *
 * Failure - NULL
 *
 * ARGUMENTS:
 *
 * mp_p <-> Pointer to the memory pool.  If NULL then it will do a
 * normal malloc.
 *
 * byte_size -> Number of bytes to allocate in the pool.  Must be >0.
 *
 * error_p <- Pointer to ks_status_t which, if not NULL, will be set with
 * a ks_pool error code.
 */
static void *alloc_mem(ks_pool_t *mp_p, const unsigned long byte_size, ks_status_t *error_p)
{
	unsigned long size, fence;
	void *addr;
	alloc_prefix_t *prefix;

	/* make sure we have enough bytes */
	if (byte_size < MIN_ALLOCATION) {
		size = MIN_ALLOCATION;
	} else {
		size = byte_size;
	}

	fence = FENCE_SIZE;

	/* get our free space + the space for the fence post */
	addr = get_space(mp_p, size + fence + PREFIX_SIZE, error_p);
	if (addr == NULL) {
		/* error_p set in get_space */
		return NULL;
	}

	write_magic((char *) addr + size + PREFIX_SIZE);
	prefix = (alloc_prefix_t *) addr;
	prefix->m1 = PRE_MAGIC1;
	prefix->m2 = PRE_MAGIC2;
	prefix->size = size;

	/* maintain our stats */
	mp_p->mp_alloc_c++;
	mp_p->mp_user_alloc += size;
	if (mp_p->mp_user_alloc > mp_p->mp_max_alloc) {
		mp_p->mp_max_alloc = mp_p->mp_user_alloc;
	}

	SET_POINTER(error_p, KS_STATUS_SUCCESS);
	return (uint8_t *)addr + PREFIX_SIZE;
}

/*
 * static int free_mem
 *
 * DESCRIPTION:
 *
 * Free an address from a memory pool.
 *
 * RETURNS:
 *
 * Success - KS_STATUS_SUCCESS
 *
 * Failure - Ks_Pool error code
 *
 * ARGUMENTS:
 *
 * mp_p <-> Pointer to the memory pool.  If NULL then it will do a
 * normal free.
 *
 * addr <-> Address to free.
 *
 */
static int free_mem(ks_pool_t *mp_p, void *addr)
{
	unsigned long size, old_size, fence;
	int ret;
	ks_pool_block_t *block_p;
	alloc_prefix_t *prefix;


	prefix = (alloc_prefix_t *) ((char *) addr - PREFIX_SIZE);
	if (!(prefix->m1 == PRE_MAGIC1 && prefix->m2 == PRE_MAGIC2)) {
		return KS_STATUS_INVALID_POINTER;
	}

	size = prefix->size;

	/*
	 * If the size is larger than a block then the allocation must be at
	 * the front of the block.
	 */
	if (size > MAX_BLOCK_USER_MEMORY(mp_p)) {
		block_p = (ks_pool_block_t *) ((char *) addr - PREFIX_SIZE - sizeof(ks_pool_block_t));
		if (block_p->mb_magic != BLOCK_MAGIC || block_p->mb_magic2 != BLOCK_MAGIC) {
			return KS_STATUS_POOL_OVER;
		}
	}

	/* make sure we have enough bytes */
	if (size < MIN_ALLOCATION) {
		old_size = MIN_ALLOCATION;
	} else {
		old_size = size;
	}

	/* find the user's magic numbers */
	ret = check_magic(addr, old_size);

	perform_pool_cleanup_on_free(mp_p, addr);

	/* move pointer to actual beginning */
	addr = prefix;

	if (ret != KS_STATUS_SUCCESS) {
		return ret;
	}

	fence = FENCE_SIZE;

	/* now we free the pointer */
	ret = free_pointer(mp_p, addr, old_size + fence);
	if (ret != KS_STATUS_SUCCESS) {
		return ret;
	}
	mp_p->mp_user_alloc -= old_size;

	/* adjust our stats */
	mp_p->mp_alloc_c--;

	return KS_STATUS_SUCCESS;
}

/***************************** exported routines *****************************/

/*
 * ks_pool_t *ks_pool_open
 *
 * DESCRIPTION:
 *
 * Open/allocate a new memory pool.
 *
 * RETURNS:
 *
 * Success - Pool pointer which must be passed to ks_pool_close to
 * deallocate.
 *
 * Failure - NULL
 *
 * ARGUMENTS:
 *
 * flags -> Flags to set attributes of the memory pool.  See the top
 * of ks_pool.h.
 *
 * page_size -> Set the internal memory page-size.  This must be a
 * multiple of the getpagesize() value.  Set to 0 for the default.
 *
 * start_addr -> Starting address to try and allocate memory pools.
 *
 * error_p <- Pointer to ks_status_t which, if not NULL, will be set with
 * a ks_pool error code.
 */
static ks_pool_t *ks_pool_raw_open(const unsigned int flags, const unsigned int page_size, void *start_addr, ks_status_t *error_p)
{
	ks_pool_block_t *block_p;
	int page_n, ret;
	ks_pool_t mp, *mp_p;
	void *free_addr;

	if (!enabled_b) {
		startup();
	}

	/* zero our temp struct */
	memset(&mp, 0, sizeof(mp));

	mp.mp_magic = KS_POOL_MAGIC;
	mp.mp_flags = flags;
	mp.mp_alloc_c = 0;
	mp.mp_user_alloc = 0;
	mp.mp_max_alloc = 0;
	mp.mp_page_c = 0;
	/* mp.mp_page_size set below */
	/* mp.mp_blocks_bit_n set below */
	/* mp.mp_top set below */
	/* mp.mp_addr set below */
	mp.mp_log_func = NULL;
	mp.mp_min_p = NULL;
	mp.mp_bounds_p = NULL;
	mp.mp_first_p = NULL;
	mp.mp_last_p = NULL;
	mp.mp_magic2 = KS_POOL_MAGIC;

	/* get and sanity check our page size */
	if (page_size > 0) {
		mp.mp_page_size = page_size;
		if (mp.mp_page_size % getpagesize() != 0) {
			SET_POINTER(error_p, KS_STATUS_ARG_INVALID);
			return NULL;
		}
	} else {
		mp.mp_page_size = getpagesize() * DEFAULT_PAGE_MULT;
		if (mp.mp_page_size % 1024 != 0) {
			SET_POINTER(error_p, KS_STATUS_PAGE_SIZE);
			return NULL;
		}
	}

	mp.mp_addr = start_addr;
	/* we start at the front of the file */
	mp.mp_top = 0;


	/*
	 * Find out how many pages we need for our ks_pool structure.
	 *
	 * NOTE: this adds possibly unneeded space for ks_pool_block_t which
	 * may not be in this block.
	 */
	page_n = PAGES_IN_SIZE(&mp, sizeof(ks_pool_t));

	/* now allocate us space for the actual struct */
	mp_p = alloc_pages(&mp, page_n, error_p);
	if (mp_p == NULL) {
		return NULL;
	}

	/*
	 * NOTE: we do not normally free the rest of the block here because
	 * we want to lesson the chance of an allocation overwriting the
	 * main structure.
	 */
	if (BIT_IS_SET(flags, KS_POOL_FLAG_HEAVY_PACKING)) {

		/* we add a block header to the front of the block */
		block_p = (ks_pool_block_t *) mp_p;

		/* init the block header */
		block_p->mb_magic = BLOCK_MAGIC;
		block_p->mb_bounds_p = (char *) block_p + SIZE_OF_PAGES(&mp, page_n);
		block_p->mb_next_p = NULL;
		block_p->mb_magic2 = BLOCK_MAGIC;

		/* the ks_pool pointer is then the 2nd thing in the block */
		mp_p = FIRST_ADDR_IN_BLOCK(block_p);
		free_addr = (char *) mp_p + sizeof(ks_pool_t);

		/* free the rest of the block */
		ret = free_pointer(&mp, free_addr, (unsigned long)((char *) block_p->mb_bounds_p - (char *) free_addr));
		if (ret != KS_STATUS_SUCCESS) {
			/* NOTE: after this line mp_p will be invalid */
			(void) free_pages(block_p, SIZE_OF_PAGES(&mp, page_n));

			SET_POINTER(error_p, ret);
			return NULL;
		}

		/*
		 * NOTE: if we are HEAVY_PACKING then the 1st block with the ks_pool
		 * header is not on the block linked list.
		 */

		/* now copy our tmp structure into our new memory area */
		memcpy(mp_p, &mp, sizeof(ks_pool_t));

		/* we setup min/max to our current address which is as good as any */
		mp_p->mp_min_p = block_p;
		mp_p->mp_bounds_p = block_p->mb_bounds_p;
	} else {
		/* now copy our tmp structure into our new memory area */
		memcpy(mp_p, &mp, sizeof(ks_pool_t));

		/* we setup min/max to our current address which is as good as any */
		mp_p->mp_min_p = mp_p;
		mp_p->mp_bounds_p = (char *) mp_p + SIZE_OF_PAGES(mp_p, page_n);
	}

	SET_POINTER(error_p, KS_STATUS_SUCCESS);
	return mp_p;
}

/*
 * ks_pool_t *ks_pool_open
 *
 * DESCRIPTION:
 *
 * Open/allocate a new memory pool.
 *
 * RETURNS:
 *
 * Success - KS_SUCCESS
 *
 * Failure - KS_FAIL
 *
 * ARGUMENTS:
 *
 * poolP <- pointer to new pool that will be set on success
 *
 */

KS_DECLARE(ks_status_t) ks_pool_open(ks_pool_t **poolP)
{
	ks_status_t err;
	ks_pool_t *pool = ks_pool_raw_open(KS_POOL_FLAG_DEFAULT, 0, NULL, &err);

	if (pool && (err == KS_STATUS_SUCCESS)) {
		ks_mutex_create(&pool->mutex, KS_MUTEX_FLAG_DEFAULT, NULL);
		ks_mutex_create(&pool->cleanup_mutex, KS_MUTEX_FLAG_DEFAULT, NULL);
		*poolP = pool;
		return KS_STATUS_SUCCESS;
	} else {
		*poolP = NULL;
		return err;
	}
}

/*
 * int ks_pool_raw_close
 *
 * DESCRIPTION:
 *
 * Close/free a memory allocation pool previously opened with
 * ks_pool_open.
 *
 * RETURNS:
 *
 * Success - KS_STATUS_SUCCESS
 *
 * Failure - Ks_Pool error code
 *
 * ARGUMENTS:
 *
 * mp_p <-> Pointer to our memory pool.
 */
static ks_status_t ks_pool_raw_close(ks_pool_t *mp_p)
{
	ks_pool_block_t *block_p, *next_p;
	void *addr;
	unsigned long size;
	int ret, final = KS_STATUS_SUCCESS;

	/* special case, just return no-error */
	if (mp_p == NULL) {
		return KS_STATUS_ARG_NULL;
	}
	if (mp_p->mp_magic != KS_POOL_MAGIC) {
		return KS_STATUS_PNT;
	}
	if (mp_p->mp_magic2 != KS_POOL_MAGIC) {
		return KS_STATUS_POOL_OVER;
	}

	if (mp_p->mp_log_func != NULL) {
		mp_p->mp_log_func(mp_p, KS_POOL_FUNC_CLOSE, 0, 0, NULL, NULL, 0);
	}

	perform_pool_cleanup(mp_p);

	ks_mutex_t *mutex = mp_p->mutex, *cleanup_mutex = mp_p->cleanup_mutex;
	ks_mutex_lock(mutex);
	/*
	 * NOTE: if we are HEAVY_PACKING then the 1st block with the ks_pool
	 * header is not on the linked list.
	 */

	/* free/invalidate the blocks */
	for (block_p = mp_p->mp_first_p; block_p != NULL; block_p = next_p) {
		if (block_p->mb_magic != BLOCK_MAGIC || block_p->mb_magic2 != BLOCK_MAGIC) {
			final = KS_STATUS_POOL_OVER;
			break;
		}
		block_p->mb_magic = 0;
		block_p->mb_magic2 = 0;
		/* record the next pointer because it might be invalidated below */
		next_p = block_p->mb_next_p;
		ret = free_pages(block_p, (unsigned long)((char *) block_p->mb_bounds_p - (char *) block_p));

		if (ret != KS_STATUS_SUCCESS) {
			final = ret;
		}
	}

	/* invalidate the ks_pool before we ditch it */
	mp_p->mp_magic = 0;
	mp_p->mp_magic2 = 0;

	/* if we are heavy packing then we need to free the 1st block later */
	if (BIT_IS_SET(mp_p->mp_flags, KS_POOL_FLAG_HEAVY_PACKING)) {
		addr = (char *) mp_p - sizeof(ks_pool_block_t);
	} else {
		addr = mp_p;
	}
	size = SIZE_OF_PAGES(mp_p, PAGES_IN_SIZE(mp_p, sizeof(ks_pool_t)));
	
	(void) munmap(addr, size);

	ks_mutex_unlock(mutex);
	ks_mutex_destroy(&mutex);
	ks_mutex_destroy(&cleanup_mutex);

	return final;
}


/*
 * ks_status_t ks_pool_close
 *
 * DESCRIPTION:
 *
 * Close/free a memory allocation pool previously opened with
 * ks_pool_open.
 *
 * RETURNS:
 *
 * Success - KS_STATUS_SUCCESS
 *
 * Failure - ks_status_t error code
 *
 * ARGUMENTS:
 *
 * mp_pp <-> Pointer to pointer of our memory pool.
 * error_p <- Pointer to error
 */

KS_DECLARE(ks_status_t) ks_pool_close(ks_pool_t **mp_pP)
{
    ks_status_t err;

	ks_assert(mp_pP);

	err = ks_pool_raw_close(*mp_pP);

	if (err == KS_STATUS_SUCCESS) {
		*mp_pP = NULL;
	}

	return err;
}

/*
 * int ks_pool_clear
 *
 * DESCRIPTION:
 *
 * Wipe an opened memory pool clean so we can start again.
 *
 * RETURNS:
 *
 * Success - KS_STATUS_SUCCESS
 *
 * Failure - Ks_Pool error code
 *
 * ARGUMENTS:
 *
 * mp_p <-> Pointer to our memory pool.
 */
KS_DECLARE(ks_status_t) ks_pool_clear(ks_pool_t *mp_p)
{
	ks_pool_block_t *block_p;
	int final = KS_STATUS_SUCCESS, bit_n, ret;
	void *first_p;

	/* special case, just return no-error */
	if (mp_p == NULL) {
		return KS_STATUS_ARG_NULL;
	}
	if (mp_p->mp_magic != KS_POOL_MAGIC) {
		return KS_STATUS_PNT;
	}
	if (mp_p->mp_magic2 != KS_POOL_MAGIC) {
		return KS_STATUS_POOL_OVER;
	}

	ks_mutex_lock(mp_p->mutex);
	if (mp_p->mp_log_func != NULL) {
		mp_p->mp_log_func(mp_p, KS_POOL_FUNC_CLEAR, 0, 0, NULL, NULL, 0);
	}

	perform_pool_cleanup(mp_p);

	/* reset all of our free lists */
	for (bit_n = 0; bit_n <= MAX_BITS; bit_n++) {
		mp_p->mp_free[bit_n] = NULL;
	}

	/* free the blocks */
	for (block_p = mp_p->mp_first_p; block_p != NULL; block_p = block_p->mb_next_p) {
		if (block_p->mb_magic != BLOCK_MAGIC || block_p->mb_magic2 != BLOCK_MAGIC) {
			final = KS_STATUS_POOL_OVER;
			break;
		}

		first_p = FIRST_ADDR_IN_BLOCK(block_p);

		/* free the memory */
		ret = free_pointer(mp_p, first_p, (unsigned long)MEMORY_IN_BLOCK(block_p));
		if (ret != KS_STATUS_SUCCESS) {
			final = ret;
		}
	}
	ks_mutex_unlock(mp_p->mutex);

	return final;
}

/*
 * void *ks_pool_alloc_ex
 *
 * DESCRIPTION:
 *
 * Allocate space for bytes inside of an already open memory pool.
 *
 * RETURNS:
 *
 * Success - Pointer to the address to use.
 *
 * Failure - NULL
 *
 * ARGUMENTS:
 *
 * mp_p <-> Pointer to the memory pool.
 *
 *
 * byte_size -> Number of bytes to allocate in the pool.  Must be >0.
 *
 * error_p <- Pointer to integer which, if not NULL, will be set with
 * a ks_pool error code.
 */
KS_DECLARE(void *) ks_pool_alloc_ex(ks_pool_t *mp_p, const unsigned long byte_size, ks_status_t *error_p)
{
	void *addr;

	ks_assert(mp_p);

	if (mp_p->mp_magic != KS_POOL_MAGIC) {
		SET_POINTER(error_p, KS_STATUS_PNT);
		return NULL;
	}
	if (mp_p->mp_magic2 != KS_POOL_MAGIC) {
		SET_POINTER(error_p, KS_STATUS_POOL_OVER);
		return NULL;
	}

	if (byte_size == 0) {
		SET_POINTER(error_p, KS_STATUS_ARG_INVALID);
		return NULL;
	}

	ks_mutex_lock(mp_p->mutex);
	addr = alloc_mem(mp_p, byte_size, error_p);
	ks_mutex_unlock(mp_p->mutex);

	if (mp_p->mp_log_func != NULL) {
		mp_p->mp_log_func(mp_p, KS_POOL_FUNC_ALLOC, byte_size, 0, addr, NULL, 0);
	}

	return addr;
}

/*
 * void *ks_pool_alloc
 *
 * DESCRIPTION:
 *
 * Allocate space for bytes inside of an already open memory pool.
 *
 * RETURNS:
 *
 * Success - Pointer to the address to use.
 *
 * Failure - NULL
 *
 * ARGUMENTS:
 *
 * mp_p <-> Pointer to the memory pool.
 *
 *
 * byte_size -> Number of bytes to allocate in the pool.  Must be >0.
 *
 */
KS_DECLARE(void *) ks_pool_alloc(ks_pool_t *mp_p, const unsigned long byte_size)
{
	return ks_pool_alloc_ex(mp_p, byte_size, NULL);
}


/*
 * void *ks_pool_calloc_ex
 *
 * DESCRIPTION:
 *
 * Allocate space for elements of bytes in the memory pool and zero
 * the space afterwards.
 *
 * RETURNS:
 *
 * Success - Pointer to the address to use.
 *
 * Failure - NULL
 *
 * ARGUMENTS:
 *
 * mp_p <-> Pointer to the memory pool.  If NULL then it will do a
 * normal calloc.
 *
 * ele_n -> Number of elements to allocate.
 *
 * ele_size -> Number of bytes per element being allocated.
 *
 * error_p <- Pointer to integer which, if not NULL, will be set with
 * a ks_pool error code.
 */
KS_DECLARE(void *) ks_pool_calloc_ex(ks_pool_t *mp_p, const unsigned long ele_n, const unsigned long ele_size, ks_status_t *error_p)
{
	void *addr;
	unsigned long byte_size;

	ks_assert(mp_p);

	if (mp_p->mp_magic != KS_POOL_MAGIC) {
		SET_POINTER(error_p, KS_STATUS_PNT);
		return NULL;
	}
	if (mp_p->mp_magic2 != KS_POOL_MAGIC) {
		SET_POINTER(error_p, KS_STATUS_POOL_OVER);
		return NULL;
	}

	if (ele_n == 0 || ele_size == 0) {
		SET_POINTER(error_p, KS_STATUS_ARG_INVALID);
		return NULL;
	}

	ks_mutex_lock(mp_p->mutex);
	byte_size = ele_n * ele_size;
	addr = alloc_mem(mp_p, byte_size, error_p);
	if (addr != NULL) {
		memset(addr, 0, byte_size);
	}
	ks_mutex_unlock(mp_p->mutex);

	if (mp_p->mp_log_func != NULL) {
		mp_p->mp_log_func(mp_p, KS_POOL_FUNC_CALLOC, ele_size, ele_n, addr, NULL, 0);
	}

	return addr;
}

/*
 * void *ks_pool_calloc
 *
 * DESCRIPTION:
 *
 * Allocate space for elements of bytes in the memory pool and zero
 * the space afterwards.
 *
 * RETURNS:
 *
 * Success - Pointer to the address to use.
 *
 * Failure - NULL
 *
 * ARGUMENTS:
 *
 * mp_p <-> Pointer to the memory pool.  If NULL then it will do a
 * normal calloc.
 *
 * ele_n -> Number of elements to allocate.
 *
 * ele_size -> Number of bytes per element being allocated.
 *
 */
KS_DECLARE(void *) ks_pool_calloc(ks_pool_t *mp_p, const unsigned long ele_n, const unsigned long ele_size)
{
	return ks_pool_calloc_ex(mp_p, ele_n, ele_size, NULL);
}

/*
 * int ks_pool_free
 *
 * DESCRIPTION:
 *
 * Free an address from a memory pool.
 *
 * RETURNS:
 *
 * Success - KS_STATUS_SUCCESS
 *
 * Failure - ks_status_t error code
 *
 * ARGUMENTS:
 *
 * mp_p <-> Pointer to the memory pool.
 *
 *
 * addr <-> Address to free.
 *
 */
KS_DECLARE(ks_status_t) ks_pool_free(ks_pool_t *mp_p, void *addr)
{
	ks_status_t r;

	ks_assert(mp_p);
	ks_assert(addr);

	ks_mutex_lock(mp_p->mutex);

	if (mp_p->mp_magic != KS_POOL_MAGIC) {
		r = KS_STATUS_PNT;
		goto end;
	}

	if (mp_p->mp_magic2 != KS_POOL_MAGIC) {
		r = KS_STATUS_POOL_OVER;
		goto end;
	}

	if (mp_p->mp_log_func != NULL) {
		alloc_prefix_t *prefix = (alloc_prefix_t *) ((char *) addr - PREFIX_SIZE);
		mp_p->mp_log_func(mp_p, KS_POOL_FUNC_FREE, prefix->size, 0, NULL, addr, 0);
	}

	r = free_mem(mp_p, addr);

 end:

	ks_mutex_unlock(mp_p->mutex);

	return r;

}

/*
 * void *ks_pool_resize_ex
 *
 * DESCRIPTION:
 *
 * Reallocate an address in a mmeory pool to a new size.  This is
 * different from realloc in that it needs the old address' size.  
 *
 * RETURNS:
 *
 * Success - Pointer to the address to use.
 *
 * Failure - NULL
 *
 * ARGUMENTS:
 *
 * mp_p <-> Pointer to the memory pool.
 *
 *
 * old_addr -> Previously allocated address.
 *
 * new_byte_size -> New size of the allocation.
 *
 * error_p <- Pointer to integer which, if not NULL, will be set with
 * a ks_pool error code.
 */
KS_DECLARE(void *) ks_pool_resize_ex(ks_pool_t *mp_p, void *old_addr, const unsigned long new_byte_size, ks_status_t *error_p)
{
	unsigned long copy_size, new_size, old_size, old_byte_size;
	void *new_addr;
	ks_pool_block_t *block_p;
	int ret;
	alloc_prefix_t *prefix;

	ks_assert(mp_p);
	//ks_assert(old_addr);

	if (!old_addr) {
		return ks_pool_alloc_ex(mp_p, new_byte_size, error_p);
	}

	if (mp_p->mp_magic != KS_POOL_MAGIC) {
		SET_POINTER(error_p, KS_STATUS_PNT);
		return NULL;
	}
	if (mp_p->mp_magic2 != KS_POOL_MAGIC) {
		SET_POINTER(error_p, KS_STATUS_POOL_OVER);
		return NULL;
	}

	prefix = (alloc_prefix_t *) ((char *) old_addr - PREFIX_SIZE);

	if (!(prefix->m1 == PRE_MAGIC1 && prefix->m2 == PRE_MAGIC2)) {
		SET_POINTER(error_p, KS_STATUS_INVALID_POINTER);
		return NULL;
	}


	ks_mutex_lock(mp_p->mutex);
	old_byte_size = prefix->size;

	/*
	 * If the size is larger than a block then the allocation must be at
	 * the front of the block.
	 */
	if (old_byte_size > MAX_BLOCK_USER_MEMORY(mp_p)) {
		block_p = (ks_pool_block_t *) ((char *) old_addr - PREFIX_SIZE - sizeof(ks_pool_block_t));
		if (block_p->mb_magic != BLOCK_MAGIC || block_p->mb_magic2 != BLOCK_MAGIC) {
			SET_POINTER(error_p, KS_STATUS_POOL_OVER);
			new_addr = NULL;
			goto end;
		}
	}

	/* make sure we have enough bytes */
	if (old_byte_size < MIN_ALLOCATION) {
		old_size = MIN_ALLOCATION;
	} else {
		old_size = old_byte_size;
	}

	/* verify that the size matches exactly */

	if (old_size > 0) {
		ret = check_magic(old_addr, old_size);
		if (ret != KS_STATUS_SUCCESS) {
			SET_POINTER(error_p, ret);
			new_addr = NULL;
			goto end;
		}
	}

	/* move pointer to actual beginning */
	old_addr = prefix;

	/* make sure we have enough bytes */
	if (new_byte_size < MIN_ALLOCATION) {
		new_size = MIN_ALLOCATION;
	} else {
		new_size = new_byte_size;
	}

	/*
	 * NOTE: we could here see if the size is the same or less and then
	 * use the current memory and free the space above.  This is harder
	 * than it sounds if we are changing the block size of the
	 * allocation.
	 */

	/* we need to get another address */
	new_addr = alloc_mem(mp_p, new_size, error_p);
	if (new_addr == NULL) {
		/* error_p set in ks_pool_alloc */
		new_addr = NULL;
		goto end;
	}

	if (new_byte_size > old_byte_size) {
		copy_size = old_byte_size;
	} else {
		copy_size = new_byte_size;
	}
	memcpy(new_addr, old_addr, copy_size);

	/* free the old address */
	ret = free_mem(mp_p, (uint8_t *)old_addr + PREFIX_SIZE);
	if (ret != KS_STATUS_SUCCESS) {
		/* if the old free failed, try and free the new address */
		(void) free_mem(mp_p, new_addr);
		SET_POINTER(error_p, ret);
		new_addr = NULL;
		goto end;
	}

	if (mp_p->mp_log_func != NULL) {
		mp_p->mp_log_func(mp_p, KS_POOL_FUNC_RESIZE, new_byte_size, 0, new_addr, old_addr, old_byte_size);
	}

	SET_POINTER(error_p, KS_STATUS_SUCCESS);

 end:

	ks_mutex_unlock(mp_p->mutex);

	return new_addr;
}

/*
 * void *ks_pool_resize
 *
 * DESCRIPTION:
 *
 * Reallocate an address in a mmeory pool to a new size.  This is
 * different from realloc in that it needs the old address' size.  
 *
 * RETURNS:
 *
 * Success - Pointer to the address to use.
 *
 * Failure - NULL
 *
 * ARGUMENTS:
 *
 * mp_p <-> Pointer to the memory pool.
 *
 *
 * old_addr -> Previously allocated address.
 *
 * new_byte_size -> New size of the allocation.
 *
 */
KS_DECLARE(void *) ks_pool_resize(ks_pool_t *mp_p, void *old_addr, const unsigned long new_byte_size)
{
	return ks_pool_resize_ex(mp_p, old_addr, new_byte_size, NULL);
}

/*
 * int ks_pool_stats
 *
 * DESCRIPTION:
 *
 * Return stats from the memory pool.
 *
 * RETURNS:
 *
 * Success - KS_STATUS_SUCCESS
 *
 * Failure - ks_status_t error code
 *
 * ARGUMENTS:
 *
 * mp_p -> Pointer to the memory pool.
 *
 * page_size_p <- Pointer to an unsigned integer which, if not NULL,
 * will be set to the page-size of the pool.
 *
 * num_alloced_p <- Pointer to an unsigned long which, if not NULL,
 * will be set to the number of pointers currently allocated in pool.
 *
 * user_alloced_p <- Pointer to an unsigned long which, if not NULL,
 * will be set to the number of user bytes allocated in this pool.
 *
 * max_alloced_p <- Pointer to an unsigned long which, if not NULL,
 * will be set to the maximum number of user bytes that have been
 * allocated in this pool.
 *
 * tot_alloced_p <- Pointer to an unsigned long which, if not NULL,
 * will be set to the total amount of space (including administrative
 * overhead) used by the pool.
 */
KS_DECLARE(ks_status_t) ks_pool_stats(const ks_pool_t *mp_p, unsigned int *page_size_p,
							   unsigned long *num_alloced_p, unsigned long *user_alloced_p, unsigned long *max_alloced_p, unsigned long *tot_alloced_p)
{
	if (mp_p == NULL) {
		return KS_STATUS_ARG_NULL;
	}
	if (mp_p->mp_magic != KS_POOL_MAGIC) {
		return KS_STATUS_PNT;
	}
	if (mp_p->mp_magic2 != KS_POOL_MAGIC) {
		return KS_STATUS_POOL_OVER;
	}

	SET_POINTER(page_size_p, mp_p->mp_page_size);
	SET_POINTER(num_alloced_p, mp_p->mp_alloc_c);
	SET_POINTER(user_alloced_p, mp_p->mp_user_alloc);
	SET_POINTER(max_alloced_p, mp_p->mp_max_alloc);
	SET_POINTER(tot_alloced_p, SIZE_OF_PAGES(mp_p, mp_p->mp_page_c));

	return KS_STATUS_SUCCESS;
}

/*
 * int ks_pool_set_log_func
 *
 * DESCRIPTION:
 *
 * Set a logging callback function to be called whenever there was a
 * memory transaction.  See ks_pool_log_func_t.
 *
 * RETURNS:
 *
 * Success - KS_STATUS_SUCCESS
 *
 * Failure - ks_status_t error code
 *
 * ARGUMENTS:
 *
 * mp_p <-> Pointer to the memory pool.
 *
 * log_func -> Log function (defined in ks_pool.h) which will be called
 * with each ks_pool transaction.
 */
KS_DECLARE(ks_status_t) ks_pool_set_log_func(ks_pool_t *mp_p, ks_pool_log_func_t log_func)
{
	if (mp_p == NULL) {
		return KS_STATUS_ARG_NULL;
	}
	if (mp_p->mp_magic != KS_POOL_MAGIC) {
		return KS_STATUS_PNT;
	}
	if (mp_p->mp_magic2 != KS_POOL_MAGIC) {
		return KS_STATUS_POOL_OVER;
	}

	mp_p->mp_log_func = log_func;

	return KS_STATUS_SUCCESS;
}

/*
 * int ks_pool_set_max_pages
 *
 * DESCRIPTION:
 *
 * Set the maximum number of pages that the library will use.  Once it
 * hits the limit it will return KS_STATUS_NO_PAGES.
 *
 * NOTE: if the KS_POOL_FLAG_HEAVY_PACKING is set then this max-pages
 * value will include the page with the ks_pool header structure in it.
 * If the flag is _not_ set then the max-pages will not include this
 * first page.
 *
 * RETURNS:
 *
 * Success - KS_STATUS_SUCCESS
 *
 * Failure - ks_status_t error code
 *
 * ARGUMENTS:
 *
 * mp_p <-> Pointer to the memory pool.
 *
 * max_pages -> Maximum number of pages used by the library.
 */
KS_DECLARE(ks_status_t) ks_pool_set_max_pages(ks_pool_t *mp_p, const unsigned int max_pages)
{
	if (mp_p == NULL) {
		return KS_STATUS_ARG_NULL;
	}
	if (mp_p->mp_magic != KS_POOL_MAGIC) {
		return KS_STATUS_PNT;
	}
	if (mp_p->mp_magic2 != KS_POOL_MAGIC) {
		return KS_STATUS_POOL_OVER;
	}

	if (BIT_IS_SET(mp_p->mp_flags, KS_POOL_FLAG_HEAVY_PACKING)) {
		mp_p->mp_max_pages = max_pages;
	} else {
		/*
		 * If we are not heavy-packing the pool then we don't count the
		 * 1st page allocated which holds the ks_pool header structure.
		 */
		mp_p->mp_max_pages = max_pages + 1;
	}

	return KS_STATUS_SUCCESS;
}

/*
 * const char *ks_pool_strerror
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
 * error -> ks_status_t that we are converting.
 */
KS_DECLARE(const char *) ks_pool_strerror(const ks_status_t error)
{
	switch (error) {
	case KS_STATUS_SUCCESS:
		return "no error";
		break;
	case KS_STATUS_ARG_NULL:
		return "function argument is null";
		break;
	case KS_STATUS_ARG_INVALID:
		return "function argument is invalid";
		break;
	case KS_STATUS_PNT:
		return "invalid ks_pool pointer";
		break;
	case KS_STATUS_POOL_OVER:
		return "ks_pool structure was overwritten";
		break;
	case KS_STATUS_PAGE_SIZE:
		return "could not get system page-size";
		break;
	case KS_STATUS_OPEN_ZERO:
		return "could not open /dev/zero";
		break;
	case KS_STATUS_NO_MEM:
		return "no memory available";
		break;
	case KS_STATUS_MMAP:
		return "problems with mmap";
		break;
	case KS_STATUS_SIZE:
		return "error processing requested size";
		break;
	case KS_STATUS_TOO_BIG:
		return "allocation exceeds pool max size";
		break;
	case KS_STATUS_MEM:
		return "invalid memory address";
		break;
	case KS_STATUS_MEM_OVER:
		return "memory lower bounds overwritten";
		break;
	case KS_STATUS_NOT_FOUND:
		return "memory block not found in pool";
		break;
	case KS_STATUS_IS_FREE:
		return "memory address has already been freed";
		break;
	case KS_STATUS_BLOCK_STAT:
		return "invalid internal block status";
		break;
	case KS_STATUS_FREE_ADDR:
		return "invalid internal free address";
		break;
	case KS_STATUS_NO_PAGES:
		return "no available pages left in pool";
		break;
	case KS_STATUS_ALLOC:
		return "system alloc function failed";
		break;
	case KS_STATUS_PNT_OVER:
		return "user pointer admin space overwritten";
		break;
	case KS_STATUS_INVALID_POINTER:
		return "pointer is not valid";
		break;
	default:
		break;
	}

	return "invalid error code";
}

KS_DECLARE(char *) ks_pstrdup(ks_pool_t *pool, const char *str)
{
    char *result;
    unsigned long len;

    if (!str) {
        return NULL;
    }

    len = (unsigned long)strlen(str) + 1;
    result = ks_pool_alloc(pool, len);
    memcpy(result, str, len);

    return result;
}

KS_DECLARE(char *) ks_pstrndup(ks_pool_t *pool, const char *str, size_t len)
{
    char *result;
    const char *end;

    if (!str) {
        return NULL;
    }

    end = memchr(str, '\0', len);

    if (!end) {
        len = end - str;
	}

    result = ks_pool_alloc(pool, (unsigned long)(len + 1));
    memcpy(result, str, len);
    result[len] = '\0';

    return result;
}

KS_DECLARE(char *) ks_pstrmemdup(ks_pool_t *pool, const char *str, size_t len)
{
    char *result;

    if (!str) {
        return NULL;
    }

    result = ks_pool_alloc(pool, (unsigned long)(len + 1));
    memcpy(result, str, len);
    result[len] = '\0';

    return result;
}

KS_DECLARE(void *) ks_pmemdup(ks_pool_t *pool, const void *buf, size_t len)
{
    void *result;

    if (!buf) {
		return NULL;
	}

    result = ks_pool_alloc(pool, (unsigned long)len);
    memcpy(result, buf, len);

    return result;
}

KS_DECLARE(char *) ks_pstrcat(ks_pool_t *pool, ...)
{
    char *endp, *argp;
	char *result;
    size_t lengths[10];
    int i = 0;
    size_t len = 0;
    va_list ap;

    va_start(ap, pool);

	/* get lengths so we know what to allocate, cache some so we don't have to double strlen those */

    while ((argp = va_arg(ap, char *))) {
		size_t arglen = strlen(argp);
        if (i < 10) lengths[i++] = arglen;
        len += arglen;
    }

    va_end(ap);

    result = (char *) ks_pool_alloc(pool, (unsigned long)(len + 1));
    endp = result;

    va_start(ap, pool);

    i = 0;

    while ((argp = va_arg(ap, char *))) {
        len = (i < 10) ? lengths[i++] : strlen(argp);
        memcpy(endp, argp, len);
        endp += len;
    }

    va_end(ap);

    *endp = '\0';

    return result;
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
