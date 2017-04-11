/*
 * Memory pool routines.
 *
 * Copyright 1996 by Gray Watson.
 *
 * This file is part of the ks_pool package.
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
 */

#include "ks.h"

 //#define DEBUG 1

#define KS_POOL_MAGIC		 0xDEADBEEF	/* magic for struct */

#define KS_POOL_PREFIX_MAGIC 0xDEADBEEF

#define KS_POOL_FENCE_MAGIC0		 (ks_byte_t)(0xFAU)	/* 1st magic mem byte */
#define KS_POOL_FENCE_MAGIC1		 (ks_byte_t)(0xD3U)	/* 2nd magic mem byte */

#define KS_POOL_FENCE_SIZE			 2		/* fence space */

typedef struct ks_pool_prefix_s ks_pool_prefix_t;

struct ks_pool_prefix_s {
	ks_size_t magic1;
	ks_size_t size;
	ks_size_t magic2;
	ks_size_t refs;
	ks_pool_prefix_t *prev;
	ks_pool_prefix_t *next;
	ks_size_t magic3;
	ks_pool_cleanup_callback_t cleanup_callback;
	void *cleanup_arg;
	ks_size_t magic4;
	ks_size_t reserved[2];
};

#define KS_POOL_PREFIX_SIZE sizeof(ks_pool_prefix_t)

#define SET_POINTER(pnt, val)					\
	do {										\
		if ((pnt) != NULL) {					\
			(*(pnt)) = (val);					\
		}										\
	} while(0)

struct ks_pool_s {
	ks_size_t magic1; /* magic number for struct */
	ks_size_t flags; /* flags for the struct */
	ks_size_t alloc_c; /* number of allocations */
	ks_size_t user_alloc; /* user bytes allocated */
	ks_size_t max_alloc; /* maximum user bytes allocated */
	ks_pool_log_func_t log_func; /* log callback function */
	ks_pool_prefix_t *first; /* first memory allocation we are using */
	ks_pool_prefix_t *last; /* last memory allocation we are using */
	ks_size_t magic2; /* upper magic for overwrite sanity */
	ks_mutex_t *mutex;
	ks_bool_t cleaning_up;
};

static ks_status_t check_pool(const ks_pool_t *pool);
static ks_status_t check_fence(const void *addr);
static void write_fence(void *addr);
static ks_status_t check_prefix(const ks_pool_prefix_t *prefix);

static void perform_pool_cleanup_on_free(ks_pool_t *pool, ks_pool_prefix_t *prefix)
{
	void *addr;

	ks_assert(pool);
	ks_assert(prefix);

	if (pool->cleaning_up) return;

	addr = (void *)((uintptr_t)prefix + KS_POOL_PREFIX_SIZE);

	if (prefix->cleanup_callback) {
		prefix->cleanup_callback(pool, addr, prefix->cleanup_arg, KS_MPCL_ANNOUNCE, KS_MPCL_FREE);
		prefix->cleanup_callback(pool, addr, prefix->cleanup_arg, KS_MPCL_TEARDOWN, KS_MPCL_FREE);
		prefix->cleanup_callback(pool, addr, prefix->cleanup_arg, KS_MPCL_DESTROY, KS_MPCL_FREE);
	}
}

static void perform_pool_cleanup(ks_pool_t *pool)
{
	ks_pool_prefix_t *prefix;

	if (pool->cleaning_up) {
		return;
	}
	pool->cleaning_up = KS_TRUE;

	for (prefix = pool->first; prefix; prefix = prefix->next) {
		if (!prefix->cleanup_callback) continue;
		prefix->cleanup_callback(pool, (void *)((uintptr_t)prefix + KS_POOL_PREFIX_SIZE), prefix->cleanup_arg, KS_MPCL_ANNOUNCE, KS_MPCL_GLOBAL_FREE);
	}

	for (prefix = pool->first; prefix; prefix = prefix->next) {
		if (!prefix->cleanup_callback) continue;
		prefix->cleanup_callback(pool, (void *)((uintptr_t)prefix + KS_POOL_PREFIX_SIZE), prefix->cleanup_arg, KS_MPCL_TEARDOWN, KS_MPCL_GLOBAL_FREE);
	}

	for (prefix = pool->first; prefix; prefix = prefix->next) {
		if (!prefix->cleanup_callback) continue;
		prefix->cleanup_callback(pool, (void *)((uintptr_t)prefix + KS_POOL_PREFIX_SIZE), prefix->cleanup_arg, KS_MPCL_DESTROY, KS_MPCL_GLOBAL_FREE);
	}
}

KS_DECLARE(ks_status_t) ks_pool_set_cleanup(ks_pool_t *pool, void *ptr, void *arg, ks_pool_cleanup_callback_t callback)
{
	ks_status_t ret = KS_STATUS_SUCCESS;
	ks_pool_prefix_t *prefix = NULL;

	ks_assert(pool);
	ks_assert(ptr);
	ks_assert(callback);

	prefix = (ks_pool_prefix_t *)((uintptr_t)ptr - KS_POOL_PREFIX_SIZE);

	ret = check_prefix(prefix);

	if (ret == KS_STATUS_SUCCESS) {
		prefix->cleanup_arg = arg;
		prefix->cleanup_callback = callback;
	}

	return ret;
}



/****************************** local utilities ******************************/

/*
* static ks_status_t check_pool
*
* DESCRIPTION:
*
* Check the validity of pool checksums.
*
* RETURNS:
*
* Success - KS_STATUS_SUCCESS
*
* Failure - Ks_Pool error code
*
* ARGUMENTS:
*
* pool -> A pointer to a pool.
*/
static ks_status_t check_pool(const ks_pool_t *pool)
{
	ks_assert(pool);

	if (pool->magic1 != KS_POOL_MAGIC) return KS_STATUS_PNT;
	if (pool->magic2 != KS_POOL_MAGIC) return KS_STATUS_POOL_OVER;

	return KS_STATUS_SUCCESS;
}

/*
 * static ks_status_t check_fence
 *
 * DESCRIPTION:
 *
 * Check the validity of the fence checksums.
 *
 * RETURNS:
 *
 * Success - KS_STATUS_SUCCESS
 *
 * Failure - Ks_Pool error code
 *
 * ARGUMENTS:
 *
 * addr -> A pointer directly to the fence.
 */
static ks_status_t check_fence(const void *addr)
{
	const ks_byte_t *mem_p;

	mem_p = (ks_byte_t *)addr;

	if (*mem_p == KS_POOL_FENCE_MAGIC0 && *(mem_p + 1) == KS_POOL_FENCE_MAGIC1)	return KS_STATUS_SUCCESS;
	return KS_STATUS_PNT_OVER;
}

/*
 * static void write_fence
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
static void write_fence(void *addr)
{
	*((ks_byte_t *)addr) = KS_POOL_FENCE_MAGIC0;
	*((ks_byte_t *)addr + 1) = KS_POOL_FENCE_MAGIC1;
}

/*
* static ks_status_t check_prefix
*
* DESCRIPTION:
*
* Check the validity of prefix checksums.
*
* RETURNS:
*
* Success - KS_STATUS_SUCCESS
*
* Failure - Ks_Pool error code
*
* ARGUMENTS:
*
* prefix -> A pointer to a prefix.
*/
static ks_status_t check_prefix(const ks_pool_prefix_t *prefix)
{
	if (!(prefix->magic1 == KS_POOL_PREFIX_MAGIC && prefix->magic2 == KS_POOL_PREFIX_MAGIC && prefix->magic3 == KS_POOL_PREFIX_MAGIC && prefix->magic4 == KS_POOL_PREFIX_MAGIC)) return KS_STATUS_INVALID_POINTER;
	return KS_STATUS_SUCCESS;
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
 * pool -> Pointer to the memory pool.
 *
 * byte_size -> Number of bytes to allocate in the pool.  Must be >0.
 *
 * error_p <- Pointer to ks_status_t which, if not NULL, will be set with
 * a ks_pool error code.
 */
static void *alloc_mem(ks_pool_t *pool, const ks_size_t size, ks_status_t *error_p)
{
	ks_size_t required;
	void *start = NULL;
	void *addr = NULL;
	void *fence = NULL;
	ks_pool_prefix_t *prefix = NULL;

	ks_assert(pool);
	ks_assert(size);

	required = KS_POOL_PREFIX_SIZE + size + KS_POOL_FENCE_SIZE;
	start = malloc(required);
	ks_assert(start);
	memset(start, 0, required);

	prefix = (ks_pool_prefix_t *)start;
	addr = (void *)((ks_byte_t *)start + KS_POOL_PREFIX_SIZE);
	fence = (void *)((ks_byte_t *)addr + size);

	prefix->magic1 = KS_POOL_PREFIX_MAGIC;
	prefix->size = size;
	prefix->magic2 = KS_POOL_PREFIX_MAGIC;
	prefix->refs = 1;
	prefix->next = pool->first;
	if (pool->first) pool->first->prev = prefix;
	pool->first = prefix;
	if (!pool->last) pool->last = prefix;
	prefix->magic3 = KS_POOL_PREFIX_MAGIC;
	prefix->magic4 = KS_POOL_PREFIX_MAGIC;

	write_fence(fence);
	
	if (pool->log_func != NULL) {
		pool->log_func(pool, KS_POOL_FUNC_INCREF, prefix->size, prefix->refs, NULL, addr, 0);
	}

	pool->alloc_c++;
	pool->user_alloc += prefix->size;
	if (pool->user_alloc > pool->max_alloc) {
		pool->max_alloc = pool->user_alloc;
	}

	SET_POINTER(error_p, KS_STATUS_SUCCESS);
	return addr;
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
 * pool -> Pointer to the memory pool.
 *
 * addr -> Address to free.
 *
 */
static ks_status_t free_mem(ks_pool_t *pool, void *addr)
{
	ks_status_t ret = KS_STATUS_SUCCESS;
	void *start = NULL;
	void *fence = NULL;
	ks_pool_prefix_t *prefix = NULL;

	ks_assert(pool);
	ks_assert(addr);

	start = (void *)((uintptr_t)addr - KS_POOL_PREFIX_SIZE);
	prefix = (ks_pool_prefix_t *)start;

	if ((ret = check_prefix(prefix)) != KS_STATUS_SUCCESS) return ret;

	if (prefix->refs > 0) {
		prefix->refs--;

		if (pool->log_func != NULL) {
			pool->log_func(pool, KS_POOL_FUNC_DECREF, prefix->size, prefix->refs, addr, NULL, 0);
		}
	}

	if (prefix->refs > 0) {
		return KS_STATUS_REFS_EXIST;
	}

	fence = (void *)((uintptr_t)addr + prefix->size);
	ret = check_fence(fence);

	perform_pool_cleanup_on_free(pool, prefix);

	if (!prefix->prev && !prefix->next) pool->first = pool->last = NULL;
	else if (!prefix->prev) {
		pool->first = prefix->next;
		pool->first->prev = NULL;
	}
	else if (!prefix->next) {
		pool->last = prefix->prev;
		pool->last->next = NULL;
	} else {
		prefix->prev->next = prefix->next;
		prefix->next->prev = prefix->prev;
	}

	pool->alloc_c--;
	pool->user_alloc -= prefix->size;

	free(start);

	return ret;
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
 * error_p <- Pointer to ks_status_t which, if not NULL, will be set with
 * a ks_pool error code.
 */
static ks_pool_t *ks_pool_raw_open(const ks_size_t flags, ks_status_t *error_p)
{
	ks_pool_t *pool;

	pool = malloc(sizeof(ks_pool_t));
	ks_assert(pool);
	memset(pool, 0, sizeof(ks_pool_t));

	pool->magic1 = KS_POOL_MAGIC;
	pool->flags = flags;
	pool->magic2 = KS_POOL_MAGIC;

	SET_POINTER(error_p, KS_STATUS_SUCCESS);
	return pool;
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
	ks_status_t ret = KS_STATUS_SUCCESS;
	ks_pool_t *pool = NULL;
	
	ks_assert(poolP);

	pool = ks_pool_raw_open(KS_POOL_FLAG_DEFAULT, &ret);

	*poolP = pool;

	if (pool) ks_mutex_create(&pool->mutex, KS_MUTEX_FLAG_DEFAULT, NULL);

	return ret;
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
 * pool -> Pointer to our memory pool.
 */
static ks_status_t ks_pool_raw_close(ks_pool_t *pool)
{
	ks_status_t ret = KS_STATUS_SUCCESS;

	if ((ret = ks_pool_clear(pool)) != KS_STATUS_SUCCESS) goto done;

	if (pool->log_func != NULL) {
		pool->log_func(pool, KS_POOL_FUNC_CLOSE, 0, 0, NULL, NULL, 0);
	}

	ks_mutex_destroy(&pool->mutex);

	free(pool);

done:
	ks_assert(ret == KS_STATUS_SUCCESS);
	return ret;
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
 * poolP <-> Pointer to pointer of our memory pool.
 */

KS_DECLARE(ks_status_t) ks_pool_close(ks_pool_t **poolP)
{
	ks_status_t ret = KS_STATUS_SUCCESS;

	ks_assert(poolP);
	ks_assert(*poolP);

	if ((ret = ks_pool_raw_close(*poolP)) == KS_STATUS_SUCCESS) *poolP = NULL;

	return ret;
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
 * pool -> Pointer to our memory pool.
 */
KS_DECLARE(ks_status_t) ks_pool_clear(ks_pool_t *pool)
{
	ks_status_t ret = KS_STATUS_SUCCESS;
	ks_pool_prefix_t *prefix, *nprefix;

	ks_assert(pool);

	if ((ret = check_pool(pool)) != KS_STATUS_SUCCESS) goto done;

	if (pool->log_func != NULL) {
		pool->log_func(pool, KS_POOL_FUNC_CLEAR, 0, 0, NULL, NULL, 0);
	}

	ks_mutex_lock(pool->mutex);

	perform_pool_cleanup(pool);

	for (prefix = pool->first; prefix; prefix = nprefix) {
		nprefix = prefix->next;
		// @todo check_prefix()? still want to clear out properly if some has been cleared though, not leak memory if there has been corruption
		free(prefix);
	}

	ks_mutex_unlock(pool->mutex);

done:
	ks_assert(ret == KS_STATUS_SUCCESS);
	return ret;
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
 * pool -> Pointer to the memory pool.
 *
 * size -> Number of bytes to allocate in the pool.  Must be >0.
 *
 * error_p <- Pointer to integer which, if not NULL, will be set with
 * a ks_pool error code.
 */
KS_DECLARE(void *) ks_pool_alloc_ex(ks_pool_t *pool, const ks_size_t size, ks_status_t *error_p)
{
	ks_status_t ret = KS_STATUS_SUCCESS;
	void *addr = NULL;

	ks_assert(pool);
	ks_assert(size);

	if ((ret = check_pool(pool)) != KS_STATUS_SUCCESS) goto done;

	ks_mutex_lock(pool->mutex);
	addr = alloc_mem(pool, size, &ret);
	ks_mutex_unlock(pool->mutex);

	if (pool->log_func != NULL) {
		pool->log_func(pool, KS_POOL_FUNC_ALLOC, size, 0, addr, NULL, 0);
	}

	ks_assert(addr);

done:
	ks_assert(ret == KS_STATUS_SUCCESS);
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
 * pool -> Pointer to the memory pool.
 *
 *
 * size -> Number of bytes to allocate in the pool.  Must be >0.
 *
 */
KS_DECLARE(void *) ks_pool_alloc(ks_pool_t *pool, const ks_size_t size)
{
	return ks_pool_alloc_ex(pool, size, NULL);
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
 * pool -> Pointer to the memory pool.  If NULL then it will do a
 * normal calloc.
 *
 * ele_n -> Number of elements to allocate.
 *
 * ele_size -> Number of bytes per element being allocated.
 *
 * error_p <- Pointer to integer which, if not NULL, will be set with
 * a ks_pool error code.
 */
KS_DECLARE(void *) ks_pool_calloc_ex(ks_pool_t *pool, const ks_size_t ele_n, const ks_size_t ele_size, ks_status_t *error_p)
{
	ks_status_t ret = KS_STATUS_SUCCESS;
	void *addr = NULL;
	ks_size_t size;

	ks_assert(pool);
	ks_assert(ele_n);
	ks_assert(ele_size);

	if ((ret = check_pool(pool)) != KS_STATUS_SUCCESS) goto done;

	size = ele_n * ele_size;

	ks_mutex_lock(pool->mutex);
	addr = alloc_mem(pool, size, &ret);
	ks_mutex_unlock(pool->mutex);

	if (pool->log_func != NULL) {
		pool->log_func(pool, KS_POOL_FUNC_CALLOC, ele_size, ele_n, addr, NULL, 0);
	}

	ks_assert(addr);

done:
	ks_assert(ret == KS_STATUS_SUCCESS);

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
 * pool -> Pointer to the memory pool.  If NULL then it will do a
 * normal calloc.
 *
 * ele_n -> Number of elements to allocate.
 *
 * ele_size -> Number of bytes per element being allocated.
 *
 */
KS_DECLARE(void *) ks_pool_calloc(ks_pool_t *pool, const ks_size_t ele_n, const ks_size_t ele_size)
{
	return ks_pool_calloc_ex(pool, ele_n, ele_size, NULL);
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
 * pool -> Pointer to the memory pool.
 *
 * addr <-> Pointer to pointer of Address to free.
 *
 */
KS_DECLARE(ks_status_t) ks_pool_free_ex(ks_pool_t *pool, void **addrP)
{
	ks_status_t ret = KS_STATUS_SUCCESS;
	void *addr;

	ks_assert(pool);
	ks_assert(addrP);
	ks_assert(*addrP);

	addr = *addrP;

	if ((ret = check_pool(pool)) != KS_STATUS_SUCCESS) goto done;

	ks_mutex_lock(pool->mutex);

	if (pool->log_func != NULL) {
		ks_pool_prefix_t *prefix = (ks_pool_prefix_t *)((uintptr_t)addr - KS_POOL_PREFIX_SIZE);
		// @todo check_prefix()?
		pool->log_func(pool, prefix->refs == 1 ? KS_POOL_FUNC_FREE : KS_POOL_FUNC_DECREF, prefix->size, prefix->refs - 1, addr, NULL, 0);
	}

	ret = free_mem(pool, addr);
	ks_mutex_unlock(pool->mutex);

done:
	if (ret != KS_STATUS_REFS_EXIST) {
		ks_assert(ret == KS_STATUS_SUCCESS);
		*addrP = NULL;
	}

	return ret;
}

/*
 * void *ks_pool_ref_ex
 *
 * DESCRIPTION:
 *
 * Ref count increment an address in a memoory pool.
 *
 * RETURNS:
 *
 * Success - The same pointer
 *
 * Failure - NULL
 *
 * ARGUMENTS:
 *
 * pool -> Pointer to the memory pool.
 *
 * addr -> The addr to ref
 *
 * error_p <- Pointer to integer which, if not NULL, will be set with
 * a ks_pool error code.
 */
KS_DECLARE(void *) ks_pool_ref_ex(ks_pool_t *pool, void *addr, ks_status_t *error_p)
{
	ks_status_t ret = KS_STATUS_SUCCESS;
	ks_pool_prefix_t *prefix;
	ks_size_t refs;

	ks_assert(pool);
	ks_assert(addr);

	if ((ret = check_pool(pool)) != KS_STATUS_SUCCESS) goto done;

	prefix = (ks_pool_prefix_t *)((uintptr_t)addr - KS_POOL_PREFIX_SIZE);
	if ((ret = check_prefix(prefix)) != KS_STATUS_SUCCESS) goto done;

	ks_mutex_lock(pool->mutex);
	refs = ++prefix->refs;
	ks_mutex_unlock(pool->mutex);

	if (pool->log_func != NULL) {
		pool->log_func(pool, KS_POOL_FUNC_INCREF, prefix->size, refs, addr, NULL, 0);
	}

done:
	ks_assert(ret == KS_STATUS_SUCCESS);

	return addr;
}

/*
 * void *ks_pool_resize_ex
 *
 * DESCRIPTION:
 *
 * Reallocate an address in a memory pool to a new size.  This is
 *
 * RETURNS:
 *
 * Success - Pointer to the address to use.
 *
 * Failure - NULL
 *
 * ARGUMENTS:
 *
 * pool -> Pointer to the memory pool.
 *
 *
 * old_addr -> Previously allocated address.
 *
 * new_size -> New size of the allocation.
 *
 * error_p <- Pointer to integer which, if not NULL, will be set with
 * a ks_pool error code.
 */
KS_DECLARE(void *) ks_pool_resize_ex(ks_pool_t *pool, void *old_addr, const ks_size_t new_size, ks_status_t *error_p)
{
	ks_status_t ret = KS_STATUS_SUCCESS;
	ks_size_t old_size;
	ks_pool_prefix_t *prefix;
	void *new_addr = NULL;
	ks_size_t required;

	ks_assert(pool);
	ks_assert(new_size);

	if ((ret = check_pool(pool)) != KS_STATUS_SUCCESS) {
		SET_POINTER(error_p, ret);
		return NULL;
	}

	if (!old_addr) {
		return ks_pool_alloc_ex(pool, new_size, error_p);
	}

	prefix = (ks_pool_prefix_t *)((uintptr_t)old_addr - KS_POOL_PREFIX_SIZE);
	if ((ret = check_prefix(prefix)) != KS_STATUS_SUCCESS) {
		SET_POINTER(error_p, ret);
		return NULL;
	}

	ks_mutex_lock(pool->mutex);

	if (prefix->refs > 1) {
		ret = KS_STATUS_NOT_ALLOWED;
		goto done;
	}
	if (new_size == prefix->size) {
		new_addr = old_addr;
		goto done;
	}

	old_size = prefix->size;

	required = KS_POOL_PREFIX_SIZE + new_size + KS_POOL_FENCE_SIZE;
	new_addr = realloc((void *)prefix, required);
	ks_assert(new_addr);

	prefix = (ks_pool_prefix_t *)new_addr;

	prefix->size = new_size;

	new_addr = (void *)((uintptr_t)new_addr + KS_POOL_PREFIX_SIZE);
	write_fence((void *)((uintptr_t)new_addr + new_size));

	if (prefix->prev) prefix->prev->next = prefix;
	else pool->first = prefix;
	if (prefix->next) prefix->next->prev = prefix;
	else pool->last = prefix;

	if (pool->log_func != NULL) {
		pool->log_func(pool, KS_POOL_FUNC_RESIZE, new_size, 0, old_addr, new_addr, old_size);
	}

done:
	ks_mutex_unlock(pool->mutex);

	ks_assert(ret == KS_STATUS_SUCCESS);

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
 * pool -> Pointer to the memory pool.
 *
 *
 * old_addr -> Previously allocated address.
 *
 * new_size -> New size of the allocation.
 *
 */
KS_DECLARE(void *) ks_pool_resize(ks_pool_t *pool, void *old_addr, const ks_size_t new_size)
{
	return ks_pool_resize_ex(pool, old_addr, new_size, NULL);
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
 * pool -> Pointer to the memory pool.
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
KS_DECLARE(ks_status_t) ks_pool_stats(const ks_pool_t *pool, ks_size_t *num_alloced_p, ks_size_t *user_alloced_p, ks_size_t *max_alloced_p, ks_size_t *tot_alloced_p)
{
	ks_status_t ret = KS_STATUS_SUCCESS;

	ks_assert(pool);

	if ((ret = check_pool(pool)) != KS_STATUS_SUCCESS) goto done;

	SET_POINTER(num_alloced_p, pool->alloc_c);
	SET_POINTER(user_alloced_p, pool->user_alloc);
	SET_POINTER(max_alloced_p, pool->max_alloc);
	SET_POINTER(tot_alloced_p, pool->user_alloc + (pool->alloc_c * (KS_POOL_PREFIX_SIZE + KS_POOL_FENCE_SIZE)));

done:
	ks_assert(ret == KS_STATUS_SUCCESS);
	return ret;
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
 * pool -> Pointer to the memory pool.
 *
 * log_func -> Log function (defined in ks_pool.h) which will be called
 * with each ks_pool transaction.
 */
KS_DECLARE(ks_status_t) ks_pool_set_log_func(ks_pool_t *pool, ks_pool_log_func_t log_func)
{
	ks_status_t ret = KS_STATUS_SUCCESS;

	ks_assert(pool);
	ks_assert(log_func);

	if ((ret = check_pool(pool)) != KS_STATUS_SUCCESS) goto done;

	pool->log_func = log_func;

done:
	ks_assert(ret == KS_STATUS_SUCCESS);
	return ret;
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
    ks_size_t len;

    if (!str) {
        return NULL;
    }

    len = (ks_size_t)strlen(str) + 1;
    result = ks_pool_alloc(pool, len);
    memcpy(result, str, len);

    return result;
}

KS_DECLARE(char *) ks_pstrndup(ks_pool_t *pool, const char *str, ks_size_t len)
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

    result = ks_pool_alloc(pool, len + 1);
    memcpy(result, str, len);
    result[len] = '\0';

    return result;
}

KS_DECLARE(char *) ks_pstrmemdup(ks_pool_t *pool, const char *str, ks_size_t len)
{
    char *result;

    if (!str) {
        return NULL;
    }

    result = ks_pool_alloc(pool, len + 1);
    memcpy(result, str, len);
    result[len] = '\0';

    return result;
}

KS_DECLARE(void *) ks_pmemdup(ks_pool_t *pool, const void *buf, ks_size_t len)
{
    void *result;

    if (!buf) {
		return NULL;
	}

    result = ks_pool_alloc(pool, len);
    memcpy(result, buf, len);

    return result;
}

KS_DECLARE(char *) ks_pstrcat(ks_pool_t *pool, ...)
{
    char *endp, *argp;
	char *result;
    ks_size_t lengths[10];
    int i = 0;
    ks_size_t len = 0;
    va_list ap;

    va_start(ap, pool);

	/* get lengths so we know what to allocate, cache some so we don't have to double strlen those */

    while ((argp = va_arg(ap, char *))) {
		ks_size_t arglen = strlen(argp);
        if (i < 10) lengths[i++] = arglen;
        len += arglen;
    }

    va_end(ap);

    result = (char *) ks_pool_alloc(pool, len + 1);
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

KS_DECLARE(char *) ks_psprintf(ks_pool_t *pool, const char *fmt, ...)
{
	va_list ap;
	char *result;
	va_start(ap, fmt);
	result = ks_vpprintf(pool, fmt, ap);
	va_end(ap);

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
