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
	KS_POOL_FLAG_DEFAULT = 0
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
#define KS_POOL_FUNC_INCREF 7	/* reference count incremented */
#define KS_POOL_FUNC_DECREF 8	/* reference count decremented */

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
 * pool -> Associated ks_pool address.
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
typedef void (*ks_pool_log_func_t) (const void *pool,
									 const int func_id,
									 const ks_size_t byte_size,
									 const ks_size_t ele_n, const void *old_addr, const void *new_addr, const ks_size_t old_byte_size);

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
 * poolP <-> Pointer to pointer of our memory pool.
 */

KS_DECLARE(ks_status_t) ks_pool_close(ks_pool_t **poolP);

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
 * pool <-> Pointer to our memory pool.
 */

KS_DECLARE(ks_status_t) ks_pool_clear(ks_pool_t *pool);

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
 * size -> Number of bytes to allocate in the pool.  Must be >0.
 *
 */
KS_DECLARE(void *) ks_pool_alloc(ks_pool_t *pool, const ks_size_t size);

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
KS_DECLARE(void *) ks_pool_alloc_ex(ks_pool_t *pool, const ks_size_t size, ks_status_t *error_p);

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
 * pool -> Pointer to the memory pool.
 *
 * ele_n -> Number of elements to allocate.
 *
 * ele_size -> Number of bytes per element being allocated.
 *
 */
KS_DECLARE(void *) ks_pool_calloc(ks_pool_t *pool, const ks_size_t ele_n, const ks_size_t ele_size);

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
 * pool -> Pointer to the memory pool.
 *
 * ele_n -> Number of elements to allocate.
 *
 * ele_size -> Number of bytes per element being allocated.
 *
 * error_p <- Pointer to integer which, if not NULL, will be set with
 * a ks_pool error code.
 */
KS_DECLARE(void *) ks_pool_calloc_ex(ks_pool_t *pool, const ks_size_t ele_n, const ks_size_t ele_size, ks_status_t *error_p);

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
 * pool -> Pointer to the memory pool.  If NULL then it will do a
 * normal free.
 *
 * addr <-> Address to free.
 *
 */

KS_DECLARE(ks_status_t) ks_pool_free_ex(ks_pool_t *pool, void **addrP);


/*
 * void *ks_pool_ref_ex
 *
 * DESCRIPTION:
 *
 * Ref count increment an address in a memory pool.
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

KS_DECLARE(void *) ks_pool_ref_ex(ks_pool_t *pool, void *addr, ks_status_t *error_p);

#define ks_pool_ref(_p, _x) ks_pool_ref_ex(_p, _x, NULL)

/*
 * void *ks_pool_resize
 *
 * DESCRIPTION:
 *
 * Reallocate an address in a memory pool to a new size.  
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
 * normal realloc.
 *
 * old_addr -> Previously allocated address.
 *
 * new_size -> New size of the allocation.
 *
 */
KS_DECLARE(void *) ks_pool_resize(ks_pool_t *pool, void *old_addr, const ks_size_t new_size);

/*
 * void *ks_pool_resize_ex
 *
 * DESCRIPTION:
 *
 * Reallocate an address in a memory pool to a new size.  
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
 * old_addr -> Previously allocated address.
 *
 * new_size -> New size of the allocation.
 *
 * error_p <- Pointer to integer which, if not NULL, will be set with
 * a ks_pool error code.
 */
KS_DECLARE(void *) ks_pool_resize_ex(ks_pool_t *pool, void *old_addr, const ks_size_t new_size, ks_status_t *error_p);

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
KS_DECLARE(ks_status_t) ks_pool_stats(const ks_pool_t *pool, ks_size_t *num_alloced_p, ks_size_t *user_alloced_p, ks_size_t *max_alloced_p, ks_size_t *tot_alloced_p);

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
KS_DECLARE(ks_status_t) ks_pool_set_log_func(ks_pool_t *pool, ks_pool_log_func_t log_func);

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

KS_DECLARE(ks_status_t) ks_pool_set_cleanup(ks_pool_t *pool, void *ptr, void *arg, ks_pool_cleanup_callback_t callback);

#define ks_pool_free(_p, _x) ks_pool_free_ex(_p, (void **)_x)

/*<<<<<<<<<<   This is end of the auto-generated output from fillproto. */

KS_DECLARE(char *) ks_pstrdup(ks_pool_t *pool, const char *str);
KS_DECLARE(char *) ks_pstrndup(ks_pool_t *pool, const char *str, ks_size_t len);
KS_DECLARE(char *) ks_pstrmemdup(ks_pool_t *pool, const char *str, ks_size_t len);
KS_DECLARE(void *) ks_pmemdup(ks_pool_t *pool, const void *buf, ks_size_t len);
KS_DECLARE(char *) ks_pstrcat(ks_pool_t *pool, ...);
KS_DECLARE(char *) ks_psprintf(ks_pool_t *pool, const char *fmt, ...);

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
