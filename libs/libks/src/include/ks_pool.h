/*
 * Memory pool defines.
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
 * $Id: ks_mpool.h,v 1.4 2006/05/31 20:26:11 gray Exp $
 */

#ifndef __KS_POOL_H__
#define __KS_POOL_H__

#include "ks.h"

KS_BEGIN_EXTERN_C

/*
 * ks_pool flags to ks_pool_alloc or ks_pool_set_attr
 */

typedef enum {
	KS_POOL_FLAG_DEFAULT = 0,

	KS_POOL_FLAG_BEST_FIT = (1 << 0),
/*
 * Choose a best fit algorithm not first fit.  This takes more CPU
 * time but will result in a tighter heap.
 */

	KS_POOL_FLAG_HEAVY_PACKING = (1 << 1)
/*
 * This enables very heavy packing at the possible expense of CPU.
 * This affects a number of parts of the library.
 *
 * By default the 1st page of memory is reserved for the main ks_pool
 * structure.  This flag will cause the rest of the 1st block to be
 * available for use as user memory.
 *
 * By default the library looks through the memory when freed looking
 * for a magic value.  There is an internal max size that it will look
 * and then it will give up.  This flag forces it to look until it
 * finds it.
 */
} ks_pool_flag_t;

/*
 * Ks_Pool function IDs for the ks_pool_log_func callback function.
 */
#define KS_POOL_FUNC_CLOSE 1	/* ks_pool_close function called */
#define KS_POOL_FUNC_CLEAR 2	/* ks_pool_clear function called */
#define KS_POOL_FUNC_ALLOC 3	/* ks_pool_alloc function called */
#define KS_POOL_FUNC_CALLOC 4	/* ks_pool_calloc function called */
#define KS_POOL_FUNC_FREE  5	/* ks_pool_free function called */
#define KS_POOL_FUNC_RESIZE 6	/* ks_pool_resize function called */

/*
 * void ks_pool_log_func_t
 *
 * DESCRIPTION:
 *
 * Ks_Pool transaction log function.
 *
 * RETURNS:
 *
 * None.
 *
 * ARGUMENT:
 *
 * mp_p -> Associated ks_pool address.
 *
 * func_id -> Integer function ID which identifies which ks_pool
 * function is being called.
 *
 * byte_size -> Optionally specified byte size.
 *
 * ele_n -> Optionally specified element number.  For ks_pool_calloc
 * only.
 *
 * new_addr -> Optionally specified new address.  For ks_pool_alloc,
 * ks_pool_calloc, and ks_pool_resize only.
 *
 * old_addr -> Optionally specified old address.  For ks_pool_resize and
 * ks_pool_free only.
 *
 * old_byte_size -> Optionally specified old byte size.  For
 * ks_pool_resize only.
 */
typedef void (*ks_pool_log_func_t) (const void *mp_p,
									 const int func_id,
									 const unsigned long byte_size,
									 const unsigned long ele_n, const void *old_addr, const void *new_addr, const unsigned long old_byte_size);

/*
 * ks_pool_t *ks_pool_open
 *
 * DESCRIPTION:
 *
 * Open/allocate a new memory pool.
 *
 * RETURNS:
 *
 * Success - KS_STATUS_SUCCESS
 *
 * Failure - ks_status_t error code
 *
 * ARGUMENTS:
 *
 * poolP <- pointer to new pool that will be set on success
 *
 */

KS_DECLARE(ks_status_t) ks_pool_open(ks_pool_t **poolP);

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
 */

KS_DECLARE(ks_status_t) ks_pool_close(ks_pool_t **mp_pP);

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
 * Failure - ks_status_t error code
 *
 * ARGUMENTS:
 *
 * mp_p <-> Pointer to our memory pool.
 */

KS_DECLARE(ks_status_t) ks_pool_clear(ks_pool_t *mp_p);

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
 * mp_p <-> Pointer to the memory pool.  If NULL then it will do a
 * normal malloc.
 *
 * byte_size -> Number of bytes to allocate in the pool.  Must be >0.
 *
 */
KS_DECLARE(void *) ks_pool_alloc(ks_pool_t *mp_p, const unsigned long byte_size);

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
 * mp_p <-> Pointer to the memory pool.  If NULL then it will do a
 * normal malloc.
 *
 * byte_size -> Number of bytes to allocate in the pool.  Must be >0.
 *
 * error_p <- Pointer to integer which, if not NULL, will be set with
 * a ks_pool error code.
 */
KS_DECLARE(void *) ks_pool_alloc_ex(ks_pool_t *mp_p, const unsigned long byte_size, ks_status_t *error_p);

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
KS_DECLARE(void *) ks_pool_calloc(ks_pool_t *mp_p, const unsigned long ele_n, const unsigned long ele_size);

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
KS_DECLARE(void *) ks_pool_calloc_ex(ks_pool_t *mp_p, const unsigned long ele_n, const unsigned long ele_size, ks_status_t *error_p);

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
 * mp_p <-> Pointer to the memory pool.  If NULL then it will do a
 * normal free.
 *
 * addr <-> Address to free.
 *
 */

KS_DECLARE(ks_status_t) ks_pool_free(ks_pool_t *mp_p, void *addr);

/*
 * void *ks_pool_resize
 *
 * DESCRIPTION:
 *
 * Reallocate an address in a mmeory pool to a new size.  
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
 * normal realloc.
 *
 * old_addr -> Previously allocated address.
 *
 * new_byte_size -> New size of the allocation.
 *
 */
KS_DECLARE(void *) ks_pool_resize(ks_pool_t *mp_p, void *old_addr, const unsigned long new_byte_size);

/*
 * void *ks_pool_resize_ex
 *
 * DESCRIPTION:
 *
 * Reallocate an address in a mmeory pool to a new size.  
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
 * normal realloc.
 *
 * old_addr -> Previously allocated address.
 *
 * new_byte_size -> New size of the allocation.
 *
 * error_p <- Pointer to integer which, if not NULL, will be set with
 * a ks_pool error code.
 */
KS_DECLARE(void *) ks_pool_resize_ex(ks_pool_t *mp_p, void *old_addr, const unsigned long new_byte_size, ks_status_t *error_p);

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
							   unsigned long *num_alloced_p, unsigned long *user_alloced_p, unsigned long *max_alloced_p, unsigned long *tot_alloced_p);

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
KS_DECLARE(ks_status_t) ks_pool_set_log_func(ks_pool_t *mp_p, ks_pool_log_func_t log_func);

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
KS_DECLARE(ks_status_t) ks_pool_set_max_pages(ks_pool_t *mp_p, const unsigned int max_pages);

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
 * error -> Error number that we are converting.
 */
KS_DECLARE(const char *) ks_pool_strerror(const ks_status_t error);

KS_DECLARE(ks_status_t) ks_pool_set_cleanup(ks_pool_t *mp_p, void *ptr, void *arg, int type, ks_pool_cleanup_fn_t fn);

#define ks_pool_safe_free(_p, _a) ks_pool_free(_p, _a); (_a) = NULL

/*<<<<<<<<<<   This is end of the auto-generated output from fillproto. */

KS_DECLARE(char *) ks_pstrdup(ks_pool_t *pool, const char *str);
KS_DECLARE(char *) ks_pstrndup(ks_pool_t *pool, const char *str, size_t len);
KS_DECLARE(char *) ks_pstrmemdup(ks_pool_t *pool, const char *str, size_t len);
KS_DECLARE(void *) ks_pmemdup(ks_pool_t *pool, const void *buf, size_t len);
KS_DECLARE(char *) ks_pstrcat(ks_pool_t *pool, ...);

KS_END_EXTERN_C

#endif /* ! __KS_POOL_H__ */

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
