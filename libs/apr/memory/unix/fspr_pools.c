/* Licensed to the Apache Software Foundation (ASF) under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.
 * The ASF licenses this file to You under the Apache License, Version 2.0
 * (the "License"); you may not use this file except in compliance with
 * the License.  You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "fspr.h"
#include "fspr_private.h"

#include "fspr_atomic.h"
#include "fspr_portable.h" /* for get_os_proc */
#include "fspr_strings.h"
#include "fspr_general.h"
#include "fspr_pools.h"
#include "fspr_allocator.h"
#include "fspr_lib.h"
#include "fspr_thread_mutex.h"
#include "fspr_hash.h"
#include "fspr_time.h"
#define APR_WANT_MEMFUNC
#include "fspr_want.h"
#include "fspr_env.h"

#if APR_HAVE_STDLIB_H
#include <stdlib.h>     /* for malloc, free and abort */
#endif

#if APR_HAVE_UNISTD_H
#include <unistd.h>     /* for getpid */
#endif


/*
 * Magic numbers
 */

#define MIN_ALLOC 8192
#define MAX_INDEX   20

#define BOUNDARY_INDEX 12
#define BOUNDARY_SIZE (1 << BOUNDARY_INDEX)

/* 
 * Timing constants for killing subprocesses
 * There is a total 3-second delay between sending a SIGINT 
 * and sending of the final SIGKILL.
 * TIMEOUT_INTERVAL should be set to TIMEOUT_USECS / 64
 * for the exponetial timeout alogrithm.
 */
#define TIMEOUT_USECS    3000000
#define TIMEOUT_INTERVAL   46875

/*
 * Allocator
 */

struct fspr_allocator_t {
    fspr_uint32_t        max_index;
    fspr_uint32_t        max_free_index;
    fspr_uint32_t        current_free_index;
#if APR_HAS_THREADS
    fspr_thread_mutex_t *mutex;
#endif /* APR_HAS_THREADS */
    fspr_pool_t         *owner;
    fspr_memnode_t      *free[MAX_INDEX];
};

#define SIZEOF_ALLOCATOR_T  APR_ALIGN_DEFAULT(sizeof(fspr_allocator_t))


/*
 * Allocator
 */

APR_DECLARE(fspr_status_t) fspr_allocator_create(fspr_allocator_t **allocator)
{
    fspr_allocator_t *new_allocator;

    *allocator = NULL;

    if ((new_allocator = malloc(SIZEOF_ALLOCATOR_T)) == NULL)
        return APR_ENOMEM;

    memset(new_allocator, 0, SIZEOF_ALLOCATOR_T);
    new_allocator->max_free_index = APR_ALLOCATOR_MAX_FREE_UNLIMITED;

    *allocator = new_allocator;

    return APR_SUCCESS;
}

APR_DECLARE(void) fspr_allocator_destroy(fspr_allocator_t *allocator)
{
    fspr_uint32_t index;
    fspr_memnode_t *node, **ref;

    for (index = 0; index < MAX_INDEX; index++) {
        ref = &allocator->free[index];
        while ((node = *ref) != NULL) {
            *ref = node->next;
            free(node);
        }
    }

    free(allocator);
}

#if APR_HAS_THREADS
APR_DECLARE(void) fspr_allocator_mutex_set(fspr_allocator_t *allocator,
                                          fspr_thread_mutex_t *mutex)
{
    allocator->mutex = mutex;
}

APR_DECLARE(fspr_thread_mutex_t *) fspr_allocator_mutex_get(
                                      fspr_allocator_t *allocator)
{
    return allocator->mutex;
}
#endif /* APR_HAS_THREADS */

APR_DECLARE(void) fspr_allocator_owner_set(fspr_allocator_t *allocator,
                                          fspr_pool_t *pool)
{
    allocator->owner = pool;
}

APR_DECLARE(fspr_pool_t *) fspr_allocator_owner_get(fspr_allocator_t *allocator)
{
    return allocator->owner;
}

APR_DECLARE(void) fspr_allocator_max_free_set(fspr_allocator_t *allocator,
                                             fspr_size_t in_size)
{
    fspr_uint32_t max_free_index;
    fspr_uint32_t size = (APR_UINT32_TRUNC_CAST)in_size;

#if APR_HAS_THREADS
    fspr_thread_mutex_t *mutex;

    mutex = fspr_allocator_mutex_get(allocator);
    if (mutex != NULL)
        fspr_thread_mutex_lock(mutex);
#endif /* APR_HAS_THREADS */

    max_free_index = APR_ALIGN(size, BOUNDARY_SIZE) >> BOUNDARY_INDEX;
    allocator->current_free_index += max_free_index;
    allocator->current_free_index -= allocator->max_free_index;
    allocator->max_free_index = max_free_index;
    if (allocator->current_free_index > max_free_index)
        allocator->current_free_index = max_free_index;

#if APR_HAS_THREADS
    if (mutex != NULL)
        fspr_thread_mutex_unlock(mutex);
#endif
}

static APR_INLINE
fspr_memnode_t *allocator_alloc(fspr_allocator_t *allocator, fspr_size_t size)
{
    fspr_memnode_t *node, **ref;
    fspr_uint32_t max_index;
    fspr_size_t i, index;

    /* Round up the block size to the next boundary, but always
     * allocate at least a certain size (MIN_ALLOC).
     */
    size = APR_ALIGN(size + APR_MEMNODE_T_SIZE, BOUNDARY_SIZE);
    if (size < MIN_ALLOC)
        size = MIN_ALLOC;

    /* Find the index for this node size by
     * dividing its size by the boundary size
     */
    index = (size >> BOUNDARY_INDEX) - 1;
    
    if (index > APR_UINT32_MAX) {
        return NULL;
    }

    /* First see if there are any nodes in the area we know
     * our node will fit into.
     */
    if (index <= allocator->max_index) {
#if APR_HAS_THREADS
        if (allocator->mutex)
            fspr_thread_mutex_lock(allocator->mutex);
#endif /* APR_HAS_THREADS */

        /* Walk the free list to see if there are
         * any nodes on it of the requested size
         *
         * NOTE: an optimization would be to check
         * allocator->free[index] first and if no
         * node is present, directly use
         * allocator->free[max_index].  This seems
         * like overkill though and could cause
         * memory waste.
         */
        max_index = allocator->max_index;
        ref = &allocator->free[index];
        i = index;
        while (*ref == NULL && i < max_index) {
           ref++;
           i++;
        }

        if ((node = *ref) != NULL) {
            /* If we have found a node and it doesn't have any
             * nodes waiting in line behind it _and_ we are on
             * the highest available index, find the new highest
             * available index
             */
            if ((*ref = node->next) == NULL && i >= max_index) {
                do {
                    ref--;
                    max_index--;
                }
                while (*ref == NULL && max_index > 0);

                allocator->max_index = max_index;
            }

            allocator->current_free_index += node->index;
            if (allocator->current_free_index > allocator->max_free_index)
                allocator->current_free_index = allocator->max_free_index;

#if APR_HAS_THREADS
            if (allocator->mutex)
                fspr_thread_mutex_unlock(allocator->mutex);
#endif /* APR_HAS_THREADS */

            node->next = NULL;
            node->first_avail = (char *)node + APR_MEMNODE_T_SIZE;

            return node;
        }

#if APR_HAS_THREADS
        if (allocator->mutex)
            fspr_thread_mutex_unlock(allocator->mutex);
#endif /* APR_HAS_THREADS */
    }

    /* If we found nothing, seek the sink (at index 0), if
     * it is not empty.
     */
    else if (allocator->free[0]) {
#if APR_HAS_THREADS
        if (allocator->mutex)
            fspr_thread_mutex_lock(allocator->mutex);
#endif /* APR_HAS_THREADS */

        /* Walk the free list to see if there are
         * any nodes on it of the requested size
         */
        ref = &allocator->free[0];
        while ((node = *ref) != NULL && index > node->index)
            ref = &node->next;

        if (node) {
            *ref = node->next;

            allocator->current_free_index += node->index;
            if (allocator->current_free_index > allocator->max_free_index)
                allocator->current_free_index = allocator->max_free_index;

#if APR_HAS_THREADS
            if (allocator->mutex)
                fspr_thread_mutex_unlock(allocator->mutex);
#endif /* APR_HAS_THREADS */

            node->next = NULL;
            node->first_avail = (char *)node + APR_MEMNODE_T_SIZE;

            return node;
        }

#if APR_HAS_THREADS
        if (allocator->mutex)
            fspr_thread_mutex_unlock(allocator->mutex);
#endif /* APR_HAS_THREADS */
    }

    /* If we haven't got a suitable node, malloc a new one
     * and initialize it.
     */
    if ((node = malloc(size)) == NULL)
        return NULL;

    node->next = NULL;
    node->index = (APR_UINT32_TRUNC_CAST)index;
    node->first_avail = (char *)node + APR_MEMNODE_T_SIZE;
    node->endp = (char *)node + size;

    return node;
}

static APR_INLINE
void allocator_free(fspr_allocator_t *allocator, fspr_memnode_t *node)
{
    fspr_memnode_t *next, *freelist = NULL;
    fspr_uint32_t index, max_index;
    fspr_uint32_t max_free_index, current_free_index;

#if APR_HAS_THREADS
    if (allocator->mutex)
        fspr_thread_mutex_lock(allocator->mutex);
#endif /* APR_HAS_THREADS */

    max_index = allocator->max_index;
    max_free_index = allocator->max_free_index;
    current_free_index = allocator->current_free_index;

    /* Walk the list of submitted nodes and free them one by one,
     * shoving them in the right 'size' buckets as we go.
     */
    do {
        next = node->next;
        index = node->index;

        if (max_free_index != APR_ALLOCATOR_MAX_FREE_UNLIMITED
            && index > current_free_index) {
            node->next = freelist;
            freelist = node;
        }
        else if (index < MAX_INDEX) {
            /* Add the node to the appropiate 'size' bucket.  Adjust
             * the max_index when appropiate.
             */
            if ((node->next = allocator->free[index]) == NULL
                && index > max_index) {
                max_index = index;
            }
            allocator->free[index] = node;
            current_free_index -= index;
        }
        else {
            /* This node is too large to keep in a specific size bucket,
             * just add it to the sink (at index 0).
             */
            node->next = allocator->free[0];
            allocator->free[0] = node;
            current_free_index -= index;
        }
    } while ((node = next) != NULL);

    allocator->max_index = max_index;
    allocator->current_free_index = current_free_index;

#if APR_HAS_THREADS
    if (allocator->mutex)
        fspr_thread_mutex_unlock(allocator->mutex);
#endif /* APR_HAS_THREADS */

    while (freelist != NULL) {
        node = freelist;
        freelist = node->next;
        free(node);
    }
}

APR_DECLARE(fspr_memnode_t *) fspr_allocator_alloc(fspr_allocator_t *allocator,
                                                 fspr_size_t size)
{
    return allocator_alloc(allocator, size);
}

APR_DECLARE(void) fspr_allocator_free(fspr_allocator_t *allocator,
                                     fspr_memnode_t *node)
{
    allocator_free(allocator, node);
}



/*
 * Debug level
 */

#define APR_POOL_DEBUG_GENERAL  0x01
#define APR_POOL_DEBUG_VERBOSE  0x02
#define APR_POOL_DEBUG_LIFETIME 0x04
#define APR_POOL_DEBUG_OWNER    0x08
#define APR_POOL_DEBUG_VERBOSE_ALLOC 0x10

#define APR_POOL_DEBUG_VERBOSE_ALL (APR_POOL_DEBUG_VERBOSE \
                                    | APR_POOL_DEBUG_VERBOSE_ALLOC)


/*
 * Structures
 */

typedef struct cleanup_t cleanup_t;

/** A list of processes */
struct process_chain {
    /** The process ID */
    fspr_proc_t *proc;
    fspr_kill_conditions_e kill_how;
    /** The next process in the list */
    struct process_chain *next;
};


#if APR_POOL_DEBUG

typedef struct debug_node_t debug_node_t;

struct debug_node_t {
    debug_node_t *next;
    fspr_uint32_t  index;
    void         *beginp[64];
    void         *endp[64];
};

#define SIZEOF_DEBUG_NODE_T APR_ALIGN_DEFAULT(sizeof(debug_node_t))

#endif /* APR_POOL_DEBUG */

/* The ref field in the fspr_pool_t struct holds a
 * pointer to the pointer referencing this pool.
 * It is used for parent, child, sibling management.
 * Look at fspr_pool_create_ex() and fspr_pool_destroy()
 * to see how it is used.
 */
struct fspr_pool_t {
    fspr_pool_t           *parent;
    fspr_pool_t           *child;
    fspr_pool_t           *sibling;
    fspr_pool_t          **ref;
    cleanup_t            *cleanups;
    cleanup_t            *free_cleanups;
    fspr_allocator_t      *allocator;
    struct process_chain *subprocesses;
    fspr_abortfunc_t       abort_fn;
    fspr_hash_t           *user_data;
    const char           *tag;
#if APR_HAS_THREADS
	fspr_thread_mutex_t   *user_mutex;
#endif
#if !APR_POOL_DEBUG
    fspr_memnode_t        *active;
    fspr_memnode_t        *self; /* The node containing the pool itself */
    char                 *self_first_avail;

#else /* APR_POOL_DEBUG */
    fspr_pool_t           *joined; /* the caller has guaranteed that this pool
                                   * will survive as long as ->joined */
    debug_node_t         *nodes;
    const char           *file_line;
    fspr_uint32_t          creation_flags;
    unsigned int          stat_alloc;
    unsigned int          stat_total_alloc;
    unsigned int          stat_clear;
#if APR_HAS_THREADS
    fspr_os_thread_t       owner;
    fspr_thread_mutex_t   *mutex;
#endif /* APR_HAS_THREADS */
#endif /* APR_POOL_DEBUG */
#ifdef NETWARE
    fspr_os_proc_t         owner_proc;

#endif /* defined(NETWARE) */
};

#define SIZEOF_POOL_T       APR_ALIGN_DEFAULT(sizeof(fspr_pool_t))


/*
 * Variables
 */

static fspr_byte_t   fspr_pools_initialized = 0;
static fspr_pool_t  *global_pool = NULL;

#if !APR_POOL_DEBUG
static fspr_allocator_t *global_allocator = NULL;
#endif /* !APR_POOL_DEBUG */

#if (APR_POOL_DEBUG & APR_POOL_DEBUG_VERBOSE_ALL)
static fspr_file_t *file_stderr = NULL;
#endif /* (APR_POOL_DEBUG & APR_POOL_DEBUG_VERBOSE_ALL) */

/*
 * Local functions
 */

static void run_cleanups(cleanup_t **c);
static void run_child_cleanups(cleanup_t **c);
static void free_proc_chain(struct process_chain *procs);

#if APR_POOL_DEBUG
static void pool_destroy_debug(fspr_pool_t *pool, const char *file_line);
#endif


#if APR_HAS_THREADS
APR_DECLARE(void) fspr_pool_mutex_set(fspr_pool_t *pool,
                                     fspr_thread_mutex_t *mutex)
{
    pool->user_mutex = mutex;
}
#endif


#if !APR_POOL_DEBUG
/*
 * Initialization
 */

APR_DECLARE(fspr_status_t) fspr_pool_initialize(void)
{
    fspr_status_t rv;

    if (fspr_pools_initialized++)
        return APR_SUCCESS;

    if ((rv = fspr_allocator_create(&global_allocator)) != APR_SUCCESS) {
        fspr_pools_initialized = 0;
        return rv;
    }

    if ((rv = fspr_pool_create_ex(&global_pool, NULL, NULL,
                                 global_allocator)) != APR_SUCCESS) {
        fspr_allocator_destroy(global_allocator);
        global_allocator = NULL;
        fspr_pools_initialized = 0;
        return rv;
    }

    fspr_pool_tag(global_pool, "fspr_global_pool");

    /* This has to happen here because mutexes might be backed by
     * atomics.  It used to be snug and safe in fspr_initialize().
     */
    if ((rv = fspr_atomic_init(global_pool)) != APR_SUCCESS) {
        return rv;
    }

#if APR_HAS_THREADS
    {
        fspr_thread_mutex_t *mutex;

        if ((rv = fspr_thread_mutex_create(&mutex,
                                          APR_THREAD_MUTEX_DEFAULT,
                                          global_pool)) != APR_SUCCESS) {
            return rv;
        }

        fspr_allocator_mutex_set(global_allocator, mutex);
    }
#endif /* APR_HAS_THREADS */

    fspr_allocator_owner_set(global_allocator, global_pool);

    return APR_SUCCESS;
}

APR_DECLARE(void) fspr_pool_terminate(void)
{
    if (!fspr_pools_initialized)
        return;

    if (--fspr_pools_initialized)
        return;

    fspr_pool_destroy(global_pool); /* This will also destroy the mutex */
    global_pool = NULL;

    global_allocator = NULL;
}


/* Node list management helper macros; list_insert() inserts 'node'
 * before 'point'. */
#define list_insert(node, point) do {           \
    node->ref = point->ref;                     \
    *node->ref = node;                          \
    node->next = point;                         \
    point->ref = &node->next;                   \
} while (0)

/* list_remove() removes 'node' from its list. */
#define list_remove(node) do {                  \
    *node->ref = node->next;                    \
    node->next->ref = node->ref;                \
} while (0)

/*
 * Memory allocation
 */

APR_DECLARE(void *) fspr_palloc(fspr_pool_t *pool, fspr_size_t size)
{
    fspr_memnode_t *active, *node;
    void *mem = NULL;
    fspr_size_t free_index;
#if APR_HAS_THREADS
	if (pool->user_mutex) fspr_thread_mutex_lock(pool->user_mutex);
#endif
    size = APR_ALIGN_DEFAULT(size);
    active = pool->active;

    /* If the active node has enough bytes left, use it. */
    if (size < (fspr_size_t)(active->endp - active->first_avail)) {
        mem = active->first_avail;
        active->first_avail += size;

		goto end;
    }

    node = active->next;
    if (size < (fspr_size_t)(node->endp - node->first_avail)) {
        list_remove(node);
    }
    else {
        if ((node = allocator_alloc(pool->allocator, size)) == NULL) {
            if (pool->abort_fn)
                pool->abort_fn(APR_ENOMEM);

			mem = NULL;
			goto end;
        }
    }

    node->free_index = 0;

    mem = node->first_avail;
    node->first_avail += size;

    list_insert(node, active);

    pool->active = node;

    free_index = (APR_ALIGN(active->endp - active->first_avail + 1,
                            BOUNDARY_SIZE) - BOUNDARY_SIZE) >> BOUNDARY_INDEX;

    active->free_index = (APR_UINT32_TRUNC_CAST)free_index;
    node = active->next;
    if (free_index >= node->free_index)
		goto end;

    do {
        node = node->next;
    }
    while (free_index < node->free_index);

    list_remove(active);
    list_insert(active, node);

 end:
#if APR_HAS_THREADS
	if (pool->user_mutex) fspr_thread_mutex_unlock(pool->user_mutex);
#endif
    return mem;
}

/* Provide an implementation of fspr_pcalloc for backward compatibility
 * with code built before fspr_pcalloc was a macro
 */

#ifdef fspr_pcalloc
#undef fspr_pcalloc
#endif

APR_DECLARE(void *) fspr_pcalloc(fspr_pool_t *pool, fspr_size_t size);
APR_DECLARE(void *) fspr_pcalloc(fspr_pool_t *pool, fspr_size_t size)
{
    void *mem;

    size = APR_ALIGN_DEFAULT(size);
    if ((mem = fspr_palloc(pool, size)) != NULL) {
        memset(mem, 0, size);
    }

    return mem;
}


/*
 * Pool creation/destruction
 */

APR_DECLARE(void) fspr_pool_clear(fspr_pool_t *pool)
{
    fspr_memnode_t *active;
#if APR_HAS_THREADS
	if (pool->user_mutex) fspr_thread_mutex_lock(pool->user_mutex);
#endif
    /* Destroy the subpools.  The subpools will detach themselves from
     * this pool thus this loop is safe and easy.
     */
    while (pool->child)
        fspr_pool_destroy(pool->child);

    /* Run cleanups */
    run_cleanups(&pool->cleanups);
    pool->cleanups = NULL;
    pool->free_cleanups = NULL;

    /* Free subprocesses */
    free_proc_chain(pool->subprocesses);
    pool->subprocesses = NULL;

    /* Clear the user data. */
    pool->user_data = NULL;

    /* Find the node attached to the pool structure, reset it, make
     * it the active node and free the rest of the nodes.
     */
    active = pool->active = pool->self;
    active->first_avail = pool->self_first_avail;

    if (active->next == active)
        goto end;

    *active->ref = NULL;
    allocator_free(pool->allocator, active->next);
    active->next = active;
    active->ref = &active->next;

 end:
#if APR_HAS_THREADS
	if (pool->user_mutex) fspr_thread_mutex_unlock(pool->user_mutex);
#endif
}

APR_DECLARE(void) fspr_pool_destroy(fspr_pool_t *pool)
{
    fspr_memnode_t *active;
    fspr_allocator_t *allocator;

    /* Destroy the subpools.  The subpools will detach themselve from
     * this pool thus this loop is safe and easy.
     */
    while (pool->child)
        fspr_pool_destroy(pool->child);

    /* Run cleanups */
    run_cleanups(&pool->cleanups);

    /* Free subprocesses */
    free_proc_chain(pool->subprocesses);

    /* Remove the pool from the parents child list */
    if (pool->parent) {
#if APR_HAS_THREADS
        fspr_thread_mutex_t *mutex;

        if ((mutex = fspr_allocator_mutex_get(pool->parent->allocator)) != NULL)
            fspr_thread_mutex_lock(mutex);
#endif /* APR_HAS_THREADS */

        if ((*pool->ref = pool->sibling) != NULL)
            pool->sibling->ref = pool->ref;

#if APR_HAS_THREADS
        if (mutex)
            fspr_thread_mutex_unlock(mutex);
#endif /* APR_HAS_THREADS */
    }

    /* Find the block attached to the pool structure.  Save a copy of the
     * allocator pointer, because the pool struct soon will be no more.
     */
    allocator = pool->allocator;
    active = pool->self;
    *active->ref = NULL;

#if APR_HAS_THREADS
    if (fspr_allocator_owner_get(allocator) == pool) {
        /* Make sure to remove the lock, since it is highly likely to
         * be invalid now.
         */
        fspr_allocator_mutex_set(allocator, NULL);
    }
#endif /* APR_HAS_THREADS */

    /* Free all the nodes in the pool (including the node holding the
     * pool struct), by giving them back to the allocator.
     */
    allocator_free(allocator, active);

    /* If this pool happens to be the owner of the allocator, free
     * everything in the allocator (that includes the pool struct
     * and the allocator).  Don't worry about destroying the optional mutex
     * in the allocator, it will have been destroyed by the cleanup function.
     */
    if (fspr_allocator_owner_get(allocator) == pool) {
        fspr_allocator_destroy(allocator);
    }
}

APR_DECLARE(fspr_status_t) fspr_pool_create_ex(fspr_pool_t **newpool,
                                             fspr_pool_t *parent,
                                             fspr_abortfunc_t abort_fn,
                                             fspr_allocator_t *allocator)
{
    fspr_pool_t *pool;
    fspr_memnode_t *node;

    *newpool = NULL;

    if (!parent)
        parent = global_pool;

    if (!abort_fn && parent)
        abort_fn = parent->abort_fn;

    if (allocator == NULL) {
        if (!parent) {
            /* There is no way to continue without an allocator when no parent */
            if (abort_fn)
                abort_fn(APR_EINVAL);

            return APR_EINVAL;
        }

        allocator = parent->allocator;
    }

    if ((node = allocator_alloc(allocator,
                                MIN_ALLOC - APR_MEMNODE_T_SIZE)) == NULL) {
        if (abort_fn)
            abort_fn(APR_ENOMEM);

        return APR_ENOMEM;
    }

    node->next = node;
    node->ref = &node->next;

    pool = (fspr_pool_t *)node->first_avail;
    node->first_avail = pool->self_first_avail = (char *)pool + SIZEOF_POOL_T;

    pool->allocator = allocator;
    pool->active = pool->self = node;
    pool->abort_fn = abort_fn;
    pool->child = NULL;
    pool->cleanups = NULL;
    pool->free_cleanups = NULL;
    pool->subprocesses = NULL;
    pool->user_data = NULL;
    pool->tag = NULL;
#if APR_HAS_THREADS
	pool->user_mutex = NULL;
#endif
#ifdef NETWARE
    pool->owner_proc = (fspr_os_proc_t)getnlmhandle();
#endif /* defined(NETWARE) */

    if ((pool->parent = parent) != NULL) {
#if APR_HAS_THREADS
        fspr_thread_mutex_t *mutex;

        if ((mutex = fspr_allocator_mutex_get(parent->allocator)) != NULL)
            fspr_thread_mutex_lock(mutex);
#endif /* APR_HAS_THREADS */

        if ((pool->sibling = parent->child) != NULL)
            pool->sibling->ref = &pool->sibling;

        parent->child = pool;
        pool->ref = &parent->child;

#if APR_HAS_THREADS
        if (mutex)
            fspr_thread_mutex_unlock(mutex);
#endif /* APR_HAS_THREADS */
    }
    else {
        pool->sibling = NULL;
        pool->ref = NULL;
    }

    *newpool = pool;

    return APR_SUCCESS;
}


/*
 * "Print" functions
 */

/*
 * fspr_psprintf is implemented by writing directly into the current
 * block of the pool, starting right at first_avail.  If there's
 * insufficient room, then a new block is allocated and the earlier
 * output is copied over.  The new block isn't linked into the pool
 * until all the output is done.
 *
 * Note that this is completely safe because nothing else can
 * allocate in this fspr_pool_t while fspr_psprintf is running.  alarms are
 * blocked, and the only thing outside of fspr_pools.c that's invoked
 * is fspr_vformatter -- which was purposefully written to be
 * self-contained with no callouts.
 */

struct psprintf_data {
    fspr_vformatter_buff_t vbuff;
    fspr_memnode_t   *node;
    fspr_pool_t      *pool;
    fspr_byte_t       got_a_new_node;
    fspr_memnode_t   *free;
};

#define APR_PSPRINTF_MIN_STRINGSIZE 32

static int psprintf_flush(fspr_vformatter_buff_t *vbuff)
{
    struct psprintf_data *ps = (struct psprintf_data *)vbuff;
    fspr_memnode_t *node, *active;
    fspr_size_t cur_len, size;
    char *strp;
    fspr_pool_t *pool;
    fspr_size_t free_index;

    pool = ps->pool;
    active = ps->node;
    strp = ps->vbuff.curpos;
    cur_len = strp - active->first_avail;
    size = cur_len << 1;

    /* Make sure that we don't try to use a block that has less
     * than APR_PSPRINTF_MIN_STRINGSIZE bytes left in it.  This
     * also catches the case where size == 0, which would result
     * in reusing a block that can't even hold the NUL byte.
     */
    if (size < APR_PSPRINTF_MIN_STRINGSIZE)
        size = APR_PSPRINTF_MIN_STRINGSIZE;

    node = active->next;
    if (!ps->got_a_new_node
        && size < (fspr_size_t)(node->endp - node->first_avail)) {

        list_remove(node);
        list_insert(node, active);

        node->free_index = 0;

        pool->active = node;

        free_index = (APR_ALIGN(active->endp - active->first_avail + 1,
                                BOUNDARY_SIZE) - BOUNDARY_SIZE) >> BOUNDARY_INDEX;

        active->free_index = (APR_UINT32_TRUNC_CAST)free_index;
        node = active->next;
        if (free_index < node->free_index) {
            do {
                node = node->next;
            }
            while (free_index < node->free_index);

            list_remove(active);
            list_insert(active, node);
        }

        node = pool->active;
    }
    else {
        if ((node = allocator_alloc(pool->allocator, size)) == NULL)
            return -1;

        if (ps->got_a_new_node) {
            active->next = ps->free;
            ps->free = active;
        }

        ps->got_a_new_node = 1;
    }

    memcpy(node->first_avail, active->first_avail, cur_len);

    ps->node = node;
    ps->vbuff.curpos = node->first_avail + cur_len;
    ps->vbuff.endpos = node->endp - 1; /* Save a byte for NUL terminator */

    return 0;
}

APR_DECLARE(char *) fspr_pvsprintf(fspr_pool_t *pool, const char *fmt, va_list ap)
{
    struct psprintf_data ps;
    char *strp;
    fspr_size_t size;
    fspr_memnode_t *active, *node;
    fspr_size_t free_index;

#if APR_HAS_THREADS
	if (pool->user_mutex) fspr_thread_mutex_lock(pool->user_mutex);
#endif

    ps.node = pool->active;
    ps.pool = pool;
    ps.vbuff.curpos  = ps.node->first_avail;

    /* Save a byte for the NUL terminator */
    ps.vbuff.endpos = ps.node->endp - 1;
    ps.got_a_new_node = 0;
    ps.free = NULL;

    /* Make sure that the first node passed to fspr_vformatter has at least
     * room to hold the NUL terminator.
     */
    if (ps.node->first_avail == ps.node->endp) {
        if (psprintf_flush(&ps.vbuff) == -1) {
            if (pool->abort_fn) {
                pool->abort_fn(APR_ENOMEM);
            }

            strp = NULL;
			goto end;
        }
    }

    if (fspr_vformatter(psprintf_flush, &ps.vbuff, fmt, ap) == -1) {
        if (pool->abort_fn)
            pool->abort_fn(APR_ENOMEM);

        strp = NULL;
		goto end;
    }

    strp = ps.vbuff.curpos;
    *strp++ = '\0';

    size = strp - ps.node->first_avail;
    size = APR_ALIGN_DEFAULT(size);
    strp = ps.node->first_avail;
    ps.node->first_avail += size;

    if (ps.free)
        allocator_free(pool->allocator, ps.free);

    /*
     * Link the node in if it's a new one
     */
    if (!ps.got_a_new_node) 
        goto end;

    active = pool->active;
    node = ps.node;

    node->free_index = 0;

    list_insert(node, active);

    pool->active = node;

    free_index = (APR_ALIGN(active->endp - active->first_avail + 1,
                            BOUNDARY_SIZE) - BOUNDARY_SIZE) >> BOUNDARY_INDEX;

    active->free_index = (APR_UINT32_TRUNC_CAST)free_index;
    node = active->next;

    if (free_index >= node->free_index)
        goto end;

    do {
        node = node->next;
    }
    while (free_index < node->free_index);

    list_remove(active);
    list_insert(active, node);

 end:

#if APR_HAS_THREADS
	if (pool->user_mutex) fspr_thread_mutex_unlock(pool->user_mutex);
#endif

    return strp;
}


#else /* APR_POOL_DEBUG */
/*
 * Debug helper functions
 */


/*
 * Walk the pool tree rooted at pool, depth first.  When fn returns
 * anything other than 0, abort the traversal and return the value
 * returned by fn.
 */
static int fspr_pool_walk_tree(fspr_pool_t *pool,
                              int (*fn)(fspr_pool_t *pool, void *data),
                              void *data)
{
    int rv;
    fspr_pool_t *child;

    rv = fn(pool, data);
    if (rv)
        return rv;

#if APR_HAS_THREADS
    if (pool->mutex) {
        fspr_thread_mutex_lock(pool->mutex);
                        }
#endif /* APR_HAS_THREADS */

    child = pool->child;
    while (child) {
        rv = fspr_pool_walk_tree(child, fn, data);
        if (rv)
            break;

        child = child->sibling;
    }

#if APR_HAS_THREADS
    if (pool->mutex) {
        fspr_thread_mutex_unlock(pool->mutex);
    }
#endif /* APR_HAS_THREADS */

    return rv;
}

APR_DECLARE(int) fspr_pool_walk_tree_debug(fspr_pool_t *pool,
	int(*fn)(fspr_pool_t *pool, void *data),
	void *data)
{
	return fspr_pool_walk_tree(pool, fn, data);
}

APR_DECLARE(void) fspr_pool_get_stats(fspr_pool_t *pool, unsigned int *alloc, unsigned int *total_alloc, unsigned int *clear)
{
	if (pool) {
		*alloc = pool->stat_alloc;
		*total_alloc = pool->stat_total_alloc;
		*clear = pool->stat_clear;
	}
}

#if (APR_POOL_DEBUG & APR_POOL_DEBUG_VERBOSE_ALL)
static void fspr_pool_log_event(fspr_pool_t *pool, const char *event,
                               const char *file_line, int deref)
{
    if (file_stderr) {
        if (deref) {
            fspr_file_printf(file_stderr,
                "POOL DEBUG: "
                "[%lu"
#if APR_HAS_THREADS
                "/%lu"
#endif /* APR_HAS_THREADS */
                "] "
                "%7s "
                "(%10lu/%10lu/%10lu) "
                "0x%08X \"%s\" "
                "<%s> "
                "(%u/%u/%u) "
                "\n",
                (unsigned long)getpid(),
#if APR_HAS_THREADS
                (unsigned long)fspr_os_thread_current(),
#endif /* APR_HAS_THREADS */
                event,
                (unsigned long)fspr_pool_num_bytes(pool, 0),
                (unsigned long)fspr_pool_num_bytes(pool, 1),
                (unsigned long)fspr_pool_num_bytes(global_pool, 1),
                (unsigned int)pool, pool->tag,
                file_line,
                pool->stat_alloc, pool->stat_total_alloc, pool->stat_clear);
        }
        else {
            fspr_file_printf(file_stderr,
                "POOL DEBUG: "
                "[%lu"
#if APR_HAS_THREADS
                "/%lu"
#endif /* APR_HAS_THREADS */
                "] "
                "%7s "
                "                                   "
                "0x%08X "
                "<%s> "
                "\n",
                (unsigned long)getpid(),
#if APR_HAS_THREADS
                (unsigned long)fspr_os_thread_current(),
#endif /* APR_HAS_THREADS */
                event,
                (unsigned int)pool,
                file_line);
        }
    }
}
#endif /* (APR_POOL_DEBUG & APR_POOL_DEBUG_VERBOSE_ALL) */

#if (APR_POOL_DEBUG & APR_POOL_DEBUG_LIFETIME)
static int pool_is_child_of(fspr_pool_t *parent, void *data)
{
    fspr_pool_t *pool = (fspr_pool_t *)data;

    return (pool == parent);
}

static int fspr_pool_is_child_of(fspr_pool_t *pool, fspr_pool_t *parent)
{
    if (parent == NULL)
        return 0;

    return fspr_pool_walk_tree(parent, pool_is_child_of, pool);
}
#endif /* (APR_POOL_DEBUG & APR_POOL_DEBUG_LIFETIME) */

static void fspr_pool_check_integrity(fspr_pool_t *pool)
{
    /* Rule of thumb: use of the global pool is always
     * ok, since the only user is fspr_pools.c.  Unless
     * people have searched for the top level parent and
     * started to use that...
     */
    if (pool == global_pool || global_pool == NULL)
        return;

    /* Lifetime
     * This basically checks to see if the pool being used is still
     * a relative to the global pool.  If not it was previously
     * destroyed, in which case we abort().
     */
#if (APR_POOL_DEBUG & APR_POOL_DEBUG_LIFETIME)
    if (!fspr_pool_is_child_of(pool, global_pool)) {
#if (APR_POOL_DEBUG & APR_POOL_DEBUG_VERBOSE_ALL)
        fspr_pool_log_event(pool, "LIFE",
                           __FILE__ ":fspr_pool_integrity check", 0);
#endif /* (APR_POOL_DEBUG & APR_POOL_DEBUG_VERBOSE_ALL) */
        abort();
    }
#endif /* (APR_POOL_DEBUG & APR_POOL_DEBUG_LIFETIME) */

#if (APR_POOL_DEBUG & APR_POOL_DEBUG_OWNER)
#if APR_HAS_THREADS
    if (!fspr_os_thread_equal(pool->owner, fspr_os_thread_current())) {
#if (APR_POOL_DEBUG & APR_POOL_DEBUG_VERBOSE_ALL)
        fspr_pool_log_event(pool, "THREAD",
                           __FILE__ ":fspr_pool_integrity check", 0);
#endif /* (APR_POOL_DEBUG & APR_POOL_DEBUG_VERBOSE_ALL) */
        abort();
    }
#endif /* APR_HAS_THREADS */
#endif /* (APR_POOL_DEBUG & APR_POOL_DEBUG_OWNER) */
}


/*
 * Initialization (debug)
 */

APR_DECLARE(fspr_status_t) fspr_pool_initialize(void)
{
    fspr_status_t rv;
#if (APR_POOL_DEBUG & APR_POOL_DEBUG_VERBOSE_ALL)
    char *logpath;
#endif

    if (fspr_pools_initialized++)
        return APR_SUCCESS;

    /* Since the debug code works a bit differently then the
     * regular pools code, we ask for a lock here.  The regular
     * pools code has got this lock embedded in the global
     * allocator, a concept unknown to debug mode.
     */
    if ((rv = fspr_pool_create_ex(&global_pool, NULL, NULL,
                                 NULL)) != APR_SUCCESS) {
        return rv;
    }

    fspr_pool_tag(global_pool, "APR global pool");

    fspr_pools_initialized = 1;

    /* This has to happen here because mutexes might be backed by
     * atomics.  It used to be snug and safe in fspr_initialize().
     */
    if ((rv = fspr_atomic_init(global_pool)) != APR_SUCCESS) {
        return rv;
    }

#if (APR_POOL_DEBUG & APR_POOL_DEBUG_VERBOSE_ALL)
    rv = fspr_env_get(&logpath, "APR_POOL_DEBUG_LOG", global_pool);

    if (rv == APR_SUCCESS) {
        fspr_file_open(&file_stderr, logpath, APR_APPEND|APR_WRITE|APR_CREATE,
                      APR_OS_DEFAULT, global_pool);
    }
    else {
        fspr_file_open_stderr(&file_stderr, global_pool);
    }

    if (file_stderr) {
        fspr_file_printf(file_stderr,
            "POOL DEBUG: [PID"
#if APR_HAS_THREADS
            "/TID"
#endif /* APR_HAS_THREADS */
            "] ACTION  (SIZE      /POOL SIZE /TOTAL SIZE) "
            "POOL       \"TAG\" <__FILE__:__LINE__> (ALLOCS/TOTAL ALLOCS/CLEARS)\n");

        fspr_pool_log_event(global_pool, "GLOBAL", __FILE__ ":fspr_pool_initialize", 0);
    }
#endif /* (APR_POOL_DEBUG & APR_POOL_DEBUG_VERBOSE_ALL) */

    return APR_SUCCESS;
}

APR_DECLARE(void) fspr_pool_terminate(void)
{
    if (!fspr_pools_initialized)
        return;

    fspr_pools_initialized = 0;

    fspr_pool_destroy(global_pool); /* This will also destroy the mutex */
    global_pool = NULL;

#if (APR_POOL_DEBUG & APR_POOL_DEBUG_VERBOSE_ALL)
    file_stderr = NULL;
#endif /* (APR_POOL_DEBUG & APR_POOL_DEBUG_VERBOSE_ALL) */
}


/*
 * Memory allocation (debug)
 */

static void *pool_alloc(fspr_pool_t *pool, fspr_size_t size)
{
    debug_node_t *node;
    void *mem;

    if ((mem = malloc(size)) == NULL) {
        if (pool->abort_fn)
            pool->abort_fn(APR_ENOMEM);

        return NULL;
    }

    node = pool->nodes;
    if (node == NULL || node->index == 64) {
        if ((node = malloc(SIZEOF_DEBUG_NODE_T)) == NULL) {
            if (pool->abort_fn)
                pool->abort_fn(APR_ENOMEM);

            return NULL;
        }

        memset(node, 0, SIZEOF_DEBUG_NODE_T);

        node->next = pool->nodes;
        pool->nodes = node;
        node->index = 0;
    }

    node->beginp[node->index] = mem;
    node->endp[node->index] = (char *)mem + size;
    node->index++;

    pool->stat_alloc++;
    pool->stat_total_alloc++;

    return mem;
}

APR_DECLARE(void *) fspr_palloc_debug(fspr_pool_t *pool, fspr_size_t size,
                                     const char *file_line)
{
    void *mem;

    fspr_pool_check_integrity(pool);

    mem = pool_alloc(pool, size);

#if (APR_POOL_DEBUG & APR_POOL_DEBUG_VERBOSE_ALLOC)
    fspr_pool_log_event(pool, "PALLOC", file_line, 1);
#endif /* (APR_POOL_DEBUG & APR_POOL_DEBUG_VERBOSE_ALLOC) */

    return mem;
}

APR_DECLARE(void *) fspr_pcalloc_debug(fspr_pool_t *pool, fspr_size_t size,
                                      const char *file_line)
{
    void *mem;

    fspr_pool_check_integrity(pool);

    mem = pool_alloc(pool, size);
    memset(mem, 0, size);

#if (APR_POOL_DEBUG & APR_POOL_DEBUG_VERBOSE_ALLOC)
    fspr_pool_log_event(pool, "PCALLOC", file_line, 1);
#endif /* (APR_POOL_DEBUG & APR_POOL_DEBUG_VERBOSE_ALLOC) */

    return mem;
}


/*
 * Pool creation/destruction (debug)
 */

#define POOL_POISON_BYTE 'A'

static void pool_clear_debug(fspr_pool_t *pool, const char *file_line)
{
    debug_node_t *node;
    fspr_uint32_t index;

    /* Destroy the subpools.  The subpools will detach themselves from
     * this pool thus this loop is safe and easy.
     */
    while (pool->child)
        pool_destroy_debug(pool->child, file_line);

    /* Run cleanups */
    run_cleanups(&pool->cleanups);
    pool->free_cleanups = NULL;
    pool->cleanups = NULL;

    /* If new child pools showed up, this is a reason to raise a flag */
    if (pool->child)
        abort();

    /* Free subprocesses */
    free_proc_chain(pool->subprocesses);
    pool->subprocesses = NULL;

    /* Clear the user data. */
    pool->user_data = NULL;

    /* Free the blocks, scribbling over them first to help highlight
     * use-after-free issues. */
    while ((node = pool->nodes) != NULL) {
        pool->nodes = node->next;

        for (index = 0; index < node->index; index++) {
            memset(node->beginp[index], POOL_POISON_BYTE,
                   (char *)node->endp[index] - (char *)node->beginp[index]);
            free(node->beginp[index]);
        }

        memset(node, POOL_POISON_BYTE, SIZEOF_DEBUG_NODE_T);
        free(node);
    }

    pool->stat_alloc = 0;
    pool->stat_clear++;
}

APR_DECLARE(void) fspr_pool_clear_debug(fspr_pool_t *pool,
                                       const char *file_line)
{
#if APR_HAS_THREADS
    fspr_thread_mutex_t *mutex = NULL;
#endif

    fspr_pool_check_integrity(pool);

#if (APR_POOL_DEBUG & APR_POOL_DEBUG_VERBOSE)
    fspr_pool_log_event(pool, "CLEAR", file_line, 1);
#endif /* (APR_POOL_DEBUG & APR_POOL_DEBUG_VERBOSE) */

#if APR_HAS_THREADS
    if (pool->parent != NULL)
        mutex = pool->parent->mutex;

    /* Lock the parent mutex before clearing so that if we have our
     * own mutex it won't be accessed by fspr_pool_walk_tree after
     * it has been destroyed.
     */
    if (mutex != NULL && mutex != pool->mutex) {
        fspr_thread_mutex_lock(mutex);
    }
#endif

    pool_clear_debug(pool, file_line);

#if APR_HAS_THREADS
    /* If we had our own mutex, it will have been destroyed by
     * the registered cleanups.  Recreate the mutex.  Unlock
     * the mutex we obtained above.
     */
    if (mutex != pool->mutex) {
        (void)fspr_thread_mutex_create(&pool->mutex,
                                      APR_THREAD_MUTEX_NESTED, pool);

        if (mutex != NULL)
            (void)fspr_thread_mutex_unlock(mutex);
    }
#endif /* APR_HAS_THREADS */
}

static void pool_destroy_debug(fspr_pool_t *pool, const char *file_line)
{
    fspr_pool_check_integrity(pool);

#if (APR_POOL_DEBUG & APR_POOL_DEBUG_VERBOSE)
    fspr_pool_log_event(pool, "DESTROY", file_line, 1);
#endif /* (APR_POOL_DEBUG & APR_POOL_DEBUG_VERBOSE) */

    pool_clear_debug(pool, file_line);

    /* Remove the pool from the parents child list */
    if (pool->parent) {
#if APR_HAS_THREADS
        fspr_thread_mutex_t *mutex;

        if ((mutex = pool->parent->mutex) != NULL)
            fspr_thread_mutex_lock(mutex);
#endif /* APR_HAS_THREADS */

        if ((*pool->ref = pool->sibling) != NULL)
            pool->sibling->ref = pool->ref;

#if APR_HAS_THREADS
        if (mutex)
            fspr_thread_mutex_unlock(mutex);
#endif /* APR_HAS_THREADS */
    }

    if (pool->allocator != NULL
        && fspr_allocator_owner_get(pool->allocator) == pool) {
        fspr_allocator_destroy(pool->allocator);
    }

    /* Free the pool itself */
    free(pool);
}

APR_DECLARE(void) fspr_pool_destroy_debug(fspr_pool_t *pool,
                                         const char *file_line)
{
    if (pool->joined) {
        /* Joined pools must not be explicitly destroyed; the caller
         * has broken the guarantee. */
#if (APR_POOL_DEBUG & APR_POOL_DEBUG_VERBOSE_ALL)
        fspr_pool_log_event(pool, "LIFE",
                           __FILE__ ":fspr_pool_destroy abort on joined", 0);
#endif /* (APR_POOL_DEBUG & APR_POOL_DEBUG_VERBOSE_ALL) */

        abort();
    }
    pool_destroy_debug(pool, file_line);
}

APR_DECLARE(fspr_status_t) fspr_pool_create_ex_debug(fspr_pool_t **newpool,
                                                   fspr_pool_t *parent,
                                                   fspr_abortfunc_t abort_fn,
                                                   fspr_allocator_t *allocator,
                                                   const char *file_line)
{
    fspr_pool_t *pool;

    *newpool = NULL;

    if (!parent) {
        parent = global_pool;
    }
    else {
       fspr_pool_check_integrity(parent);

       if (!allocator)
           allocator = parent->allocator;
    }

    if (!abort_fn && parent)
        abort_fn = parent->abort_fn;

    if ((pool = malloc(SIZEOF_POOL_T)) == NULL) {
        if (abort_fn)
            abort_fn(APR_ENOMEM);

         return APR_ENOMEM;
    }

    memset(pool, 0, SIZEOF_POOL_T);

    pool->allocator = allocator;
    pool->abort_fn = abort_fn;
    pool->tag = file_line;
    pool->file_line = file_line;

    if ((pool->parent = parent) != NULL) {
#if APR_HAS_THREADS
        if (parent->mutex)
            fspr_thread_mutex_lock(parent->mutex);
#endif /* APR_HAS_THREADS */
        if ((pool->sibling = parent->child) != NULL)
            pool->sibling->ref = &pool->sibling;

        parent->child = pool;
        pool->ref = &parent->child;

#if APR_HAS_THREADS
        if (parent->mutex)
            fspr_thread_mutex_unlock(parent->mutex);
#endif /* APR_HAS_THREADS */
    }
    else {
        pool->sibling = NULL;
        pool->ref = NULL;
    }

#if APR_HAS_THREADS
    pool->owner = fspr_os_thread_current();
#endif /* APR_HAS_THREADS */
#ifdef NETWARE
    pool->owner_proc = (fspr_os_proc_t)getnlmhandle();
#endif /* defined(NETWARE) */


    if (parent == NULL || parent->allocator != allocator) {
#if APR_HAS_THREADS
        fspr_status_t rv;

        /* No matter what the creation flags say, always create
         * a lock.  Without it integrity_check and fspr_pool_num_bytes
         * blow up (because they traverse pools child lists that
         * possibly belong to another thread, in combination with
         * the pool having no lock).  However, this might actually
         * hide problems like creating a child pool of a pool
         * belonging to another thread.
         */
        if ((rv = fspr_thread_mutex_create(&pool->mutex,
                APR_THREAD_MUTEX_NESTED, pool)) != APR_SUCCESS) {
            free(pool);
            return rv;
        }
#endif /* APR_HAS_THREADS */
    }
    else {
#if APR_HAS_THREADS
        if (parent)
            pool->mutex = parent->mutex;
#endif /* APR_HAS_THREADS */
    }

    *newpool = pool;

#if (APR_POOL_DEBUG & APR_POOL_DEBUG_VERBOSE)
    fspr_pool_log_event(pool, "CREATE", file_line, 1);
#endif /* (APR_POOL_DEBUG & APR_POOL_DEBUG_VERBOSE) */

    return APR_SUCCESS;
}


/*
 * "Print" functions (debug)
 */

struct psprintf_data {
    fspr_vformatter_buff_t vbuff;
    char      *mem;
    fspr_size_t size;
};

static int psprintf_flush(fspr_vformatter_buff_t *vbuff)
{
    struct psprintf_data *ps = (struct psprintf_data *)vbuff;
    fspr_size_t size;

    size = ps->vbuff.curpos - ps->mem;

    ps->size <<= 1;
    if ((ps->mem = realloc(ps->mem, ps->size)) == NULL)
        return -1;

    ps->vbuff.curpos = ps->mem + size;
    ps->vbuff.endpos = ps->mem + ps->size - 1;

    return 0;
}

APR_DECLARE(char *) fspr_pvsprintf(fspr_pool_t *pool, const char *fmt, va_list ap)
{
    struct psprintf_data ps;
    debug_node_t *node;

    fspr_pool_check_integrity(pool);

    ps.size = 64;
    ps.mem = malloc(ps.size);
    ps.vbuff.curpos  = ps.mem;

    /* Save a byte for the NUL terminator */
    ps.vbuff.endpos = ps.mem + ps.size - 1;

    if (fspr_vformatter(psprintf_flush, &ps.vbuff, fmt, ap) == -1) {
        if (pool->abort_fn)
            pool->abort_fn(APR_ENOMEM);

        return NULL;
    }

    *ps.vbuff.curpos++ = '\0';

    /*
     * Link the node in
     */
    node = pool->nodes;
    if (node == NULL || node->index == 64) {
        if ((node = malloc(SIZEOF_DEBUG_NODE_T)) == NULL) {
            if (pool->abort_fn)
                pool->abort_fn(APR_ENOMEM);

            return NULL;
        }

        node->next = pool->nodes;
        pool->nodes = node;
        node->index = 0;
    }

    node->beginp[node->index] = ps.mem;
    node->endp[node->index] = ps.mem + ps.size;
    node->index++;

    return ps.mem;
}


/*
 * Debug functions
 */

APR_DECLARE(void) fspr_pool_join(fspr_pool_t *p, fspr_pool_t *sub)
{
#if APR_POOL_DEBUG
    if (sub->parent != p) {
        abort();
    }
    sub->joined = p;
#endif
}

static int pool_find(fspr_pool_t *pool, void *data)
{
    void **pmem = (void **)data;
    debug_node_t *node;
    fspr_uint32_t index;

    node = pool->nodes;

    while (node) {
        for (index = 0; index < node->index; index++) {
             if (node->beginp[index] <= *pmem
                 && node->endp[index] > *pmem) {
                 *pmem = pool;
                 return 1;
             }
        }

        node = node->next;
    }

    return 0;
}

APR_DECLARE(fspr_pool_t *) fspr_pool_find(const void *mem)
{
    void *pool = (void *)mem;

    if (fspr_pool_walk_tree(global_pool, pool_find, &pool))
        return pool;

    return NULL;
}

static int pool_num_bytes(fspr_pool_t *pool, void *data)
{
    fspr_size_t *psize = (fspr_size_t *)data;
    debug_node_t *node;
    fspr_uint32_t index;

    node = pool->nodes;

    while (node) {
        for (index = 0; index < node->index; index++) {
            *psize += (char *)node->endp[index] - (char *)node->beginp[index];
        }

        node = node->next;
    }

    return 0;
}

APR_DECLARE(fspr_size_t) fspr_pool_num_bytes(fspr_pool_t *pool, int recurse)
{
    fspr_size_t size = 0;

    if (!recurse) {
        pool_num_bytes(pool, &size);

        return size;
    }

    fspr_pool_walk_tree(pool, pool_num_bytes, &size);

    return size;
}

APR_DECLARE(void) fspr_pool_lock(fspr_pool_t *pool, int flag)
{
}

#endif /* !APR_POOL_DEBUG */

#ifdef NETWARE
void netware_pool_proc_cleanup ()
{
    fspr_pool_t *pool = global_pool->child;
    fspr_os_proc_t owner_proc = (fspr_os_proc_t)getnlmhandle();

    while (pool) {
        if (pool->owner_proc == owner_proc) {
            fspr_pool_destroy (pool);
            pool = global_pool->child;
        }
        else {
            pool = pool->sibling;
        }
    }
    return;
}
#endif /* defined(NETWARE) */


/*
 * "Print" functions (common)
 */

APR_DECLARE_NONSTD(char *) fspr_psprintf(fspr_pool_t *p, const char *fmt, ...)
{
    va_list ap;
    char *res;

    va_start(ap, fmt);
    res = fspr_pvsprintf(p, fmt, ap);
    va_end(ap);
    return res;
}

/*
 * Pool Properties
 */

APR_DECLARE(void) fspr_pool_abort_set(fspr_abortfunc_t abort_fn,
                                     fspr_pool_t *pool)
{
    pool->abort_fn = abort_fn;
}

APR_DECLARE(fspr_abortfunc_t) fspr_pool_abort_get(fspr_pool_t *pool)
{
    return pool->abort_fn;
}

APR_DECLARE(fspr_pool_t *) fspr_pool_parent_get(fspr_pool_t *pool)
{
#ifdef NETWARE
    /* On NetWare, don't return the global_pool, return the application pool 
       as the top most pool */
    if (pool->parent == global_pool)
        return NULL;
    else
#endif
    return pool->parent;
}

APR_DECLARE(fspr_allocator_t *) fspr_pool_allocator_get(fspr_pool_t *pool)
{
    return pool->allocator;
}

/* return TRUE if a is an ancestor of b
 * NULL is considered an ancestor of all pools
 */
APR_DECLARE(int) fspr_pool_is_ancestor(fspr_pool_t *a, fspr_pool_t *b)
{
    if (a == NULL)
        return 1;

#if APR_POOL_DEBUG
    /* Find the pool with the longest lifetime guaranteed by the
     * caller: */
    while (a->joined) {
        a = a->joined;
    }
#endif

    while (b) {
        if (a == b)
            return 1;

        b = b->parent;
    }

    return 0;
}

APR_DECLARE(const char *) fspr_pool_tag(fspr_pool_t *pool, const char *tag)
{
	if (tag) {
		pool->tag = tag;
	}

	return pool->tag;
}


/*
 * User data management
 */

APR_DECLARE(fspr_status_t) fspr_pool_userdata_set(const void *data, const char *key,
                                                fspr_status_t (*cleanup) (void *),
                                                fspr_pool_t *pool)
{
#if APR_POOL_DEBUG
    fspr_pool_check_integrity(pool);
#endif /* APR_POOL_DEBUG */

    if (pool->user_data == NULL)
        pool->user_data = fspr_hash_make(pool);

    if (fspr_hash_get(pool->user_data, key, APR_HASH_KEY_STRING) == NULL) {
        char *new_key = fspr_pstrdup(pool, key);
        fspr_hash_set(pool->user_data, new_key, APR_HASH_KEY_STRING, data);
    }
    else {
        fspr_hash_set(pool->user_data, key, APR_HASH_KEY_STRING, data);
    }

    if (cleanup)
        fspr_pool_cleanup_register(pool, data, cleanup, cleanup);

    return APR_SUCCESS;
}

APR_DECLARE(fspr_status_t) fspr_pool_userdata_setn(const void *data,
                              const char *key,
                              fspr_status_t (*cleanup)(void *),
                              fspr_pool_t *pool)
{
#if APR_POOL_DEBUG
    fspr_pool_check_integrity(pool);
#endif /* APR_POOL_DEBUG */

    if (pool->user_data == NULL)
        pool->user_data = fspr_hash_make(pool);

    fspr_hash_set(pool->user_data, key, APR_HASH_KEY_STRING, data);

    if (cleanup)
        fspr_pool_cleanup_register(pool, data, cleanup, cleanup);

    return APR_SUCCESS;
}

APR_DECLARE(fspr_status_t) fspr_pool_userdata_get(void **data, const char *key,
                                                fspr_pool_t *pool)
{
#if APR_POOL_DEBUG
    fspr_pool_check_integrity(pool);
#endif /* APR_POOL_DEBUG */

    if (pool->user_data == NULL) {
        *data = NULL;
    }
    else {
        *data = fspr_hash_get(pool->user_data, key, APR_HASH_KEY_STRING);
    }

    return APR_SUCCESS;
}


/*
 * Cleanup
 */

struct cleanup_t {
    struct cleanup_t *next;
    const void *data;
    fspr_status_t (*plain_cleanup_fn)(void *data);
    fspr_status_t (*child_cleanup_fn)(void *data);
};

APR_DECLARE(void) fspr_pool_cleanup_register(fspr_pool_t *p, const void *data,
                      fspr_status_t (*plain_cleanup_fn)(void *data),
                      fspr_status_t (*child_cleanup_fn)(void *data))
{
    cleanup_t *c;

#if APR_POOL_DEBUG
    fspr_pool_check_integrity(p);
#endif /* APR_POOL_DEBUG */

    if (p != NULL) {
        if (p->free_cleanups) {
            /* reuse a cleanup structure */
            c = p->free_cleanups;
            p->free_cleanups = c->next;
        } else {
            c = fspr_palloc(p, sizeof(cleanup_t));
        }
        c->data = data;
        c->plain_cleanup_fn = plain_cleanup_fn;
        c->child_cleanup_fn = child_cleanup_fn;
        c->next = p->cleanups;
        p->cleanups = c;
    }
}

APR_DECLARE(void) fspr_pool_cleanup_kill(fspr_pool_t *p, const void *data,
                      fspr_status_t (*cleanup_fn)(void *))
{
    cleanup_t *c, **lastp;

#if APR_POOL_DEBUG
    fspr_pool_check_integrity(p);
#endif /* APR_POOL_DEBUG */

    if (p == NULL)
        return;

    c = p->cleanups;
    lastp = &p->cleanups;
    while (c) {
        if (c->data == data && c->plain_cleanup_fn == cleanup_fn) {
            *lastp = c->next;
            /* move to freelist */
            c->next = p->free_cleanups;
            p->free_cleanups = c;
            break;
        }

        lastp = &c->next;

		if (c == c->next) {
			c = NULL;
		} else {
			c = c->next;
		}
    }
}

APR_DECLARE(void) fspr_pool_child_cleanup_set(fspr_pool_t *p, const void *data,
                      fspr_status_t (*plain_cleanup_fn)(void *),
                      fspr_status_t (*child_cleanup_fn)(void *))
{
    cleanup_t *c;

#if APR_POOL_DEBUG
    fspr_pool_check_integrity(p);
#endif /* APR_POOL_DEBUG */

    if (p == NULL)
        return;

    c = p->cleanups;
    while (c) {
        if (c->data == data && c->plain_cleanup_fn == plain_cleanup_fn) {
            c->child_cleanup_fn = child_cleanup_fn;
            break;
        }

        c = c->next;
    }
}

APR_DECLARE(fspr_status_t) fspr_pool_cleanup_run(fspr_pool_t *p, void *data,
                              fspr_status_t (*cleanup_fn)(void *))
{
    fspr_pool_cleanup_kill(p, data, cleanup_fn);
    return (*cleanup_fn)(data);
}

static void run_cleanups(cleanup_t **cref)
{
    cleanup_t *c = *cref;

    while (c) {
        *cref = c->next;
        (*c->plain_cleanup_fn)((void *)c->data);
        c = *cref;
    }
}

static void run_child_cleanups(cleanup_t **cref)
{
    cleanup_t *c = *cref;

    while (c) {
        *cref = c->next;
        (*c->child_cleanup_fn)((void *)c->data);
        c = *cref;
    }
}

static void cleanup_pool_for_exec(fspr_pool_t *p)
{
    run_child_cleanups(&p->cleanups);

    for (p = p->child; p; p = p->sibling)
        cleanup_pool_for_exec(p);
}

APR_DECLARE(void) fspr_pool_cleanup_for_exec(void)
{
#if !defined(WIN32) && !defined(OS2)
    /*
     * Don't need to do anything on NT or OS/2, because I
     * am actually going to spawn the new process - not
     * exec it. All handles that are not inheritable, will
     * be automajically closed. The only problem is with
     * file handles that are open, but there isn't much
     * I can do about that (except if the child decides
     * to go out and close them
     */
    cleanup_pool_for_exec(global_pool);
#endif /* !defined(WIN32) && !defined(OS2) */
}

APR_DECLARE_NONSTD(fspr_status_t) fspr_pool_cleanup_null(void *data)
{
    /* do nothing cleanup routine */
    return APR_SUCCESS;
}

/* Subprocesses don't use the generic cleanup interface because
 * we don't want multiple subprocesses to result in multiple
 * three-second pauses; the subprocesses have to be "freed" all
 * at once.  If other resources are introduced with the same property,
 * we might want to fold support for that into the generic interface.
 * For now, it's a special case.
 */
APR_DECLARE(void) fspr_pool_note_subprocess(fspr_pool_t *pool, fspr_proc_t *proc,
                                           fspr_kill_conditions_e how)
{
    struct process_chain *pc = fspr_palloc(pool, sizeof(struct process_chain));

    pc->proc = proc;
    pc->kill_how = how;
    pc->next = pool->subprocesses;
    pool->subprocesses = pc;
}

static void free_proc_chain(struct process_chain *procs)
{
    /* Dispose of the subprocesses we've spawned off in the course of
     * whatever it was we're cleaning up now.  This may involve killing
     * some of them off...
     */
    struct process_chain *pc;
    int need_timeout = 0;
    fspr_time_t timeout_interval;

    if (!procs)
        return; /* No work.  Whew! */

    /* First, check to see if we need to do the SIGTERM, sleep, SIGKILL
     * dance with any of the processes we're cleaning up.  If we've got
     * any kill-on-sight subprocesses, ditch them now as well, so they
     * don't waste any more cycles doing whatever it is that they shouldn't
     * be doing anymore.
     */

#ifndef NEED_WAITPID
    /* Pick up all defunct processes */
    for (pc = procs; pc; pc = pc->next) {
        if (fspr_proc_wait(pc->proc, NULL, NULL, APR_NOWAIT) != APR_CHILD_NOTDONE)
            pc->kill_how = APR_KILL_NEVER;
    }
#endif /* !defined(NEED_WAITPID) */

    for (pc = procs; pc; pc = pc->next) {
#ifndef WIN32
        if ((pc->kill_how == APR_KILL_AFTER_TIMEOUT)
            || (pc->kill_how == APR_KILL_ONLY_ONCE)) {
            /*
             * Subprocess may be dead already.  Only need the timeout if not.
             * Note: fspr_proc_kill on Windows is TerminateProcess(), which is
             * similar to a SIGKILL, so always give the process a timeout
             * under Windows before killing it.
             */
            if (fspr_proc_kill(pc->proc, SIGTERM) == APR_SUCCESS)
                need_timeout = 1;
        }
        else if (pc->kill_how == APR_KILL_ALWAYS) {
#else /* WIN32 knows only one fast, clean method of killing processes today */
        if (pc->kill_how != APR_KILL_NEVER) {
            need_timeout = 1;
            pc->kill_how = APR_KILL_ALWAYS;
#endif
            fspr_proc_kill(pc->proc, SIGKILL);
        }
    }

    /* Sleep only if we have to. The sleep algorithm grows
     * by a factor of two on each iteration. TIMEOUT_INTERVAL
     * is equal to TIMEOUT_USECS / 64.
     */
    if (need_timeout) {
        timeout_interval = TIMEOUT_INTERVAL;
        fspr_sleep(timeout_interval);

        do {
            /* check the status of the subprocesses */
            need_timeout = 0;
            for (pc = procs; pc; pc = pc->next) {
                if (pc->kill_how == APR_KILL_AFTER_TIMEOUT) {
                    if (fspr_proc_wait(pc->proc, NULL, NULL, APR_NOWAIT)
                            == APR_CHILD_NOTDONE)
                        need_timeout = 1;		/* subprocess is still active */
                    else
                        pc->kill_how = APR_KILL_NEVER;	/* subprocess has exited */
                }
            }
            if (need_timeout) {
                if (timeout_interval >= TIMEOUT_USECS) {
                    break;
                }
                fspr_sleep(timeout_interval);
                timeout_interval *= 2;
            }
        } while (need_timeout);
    }

    /* OK, the scripts we just timed out for have had a chance to clean up
     * --- now, just get rid of them, and also clean up the system accounting
     * goop...
     */
    for (pc = procs; pc; pc = pc->next) {
        if (pc->kill_how == APR_KILL_AFTER_TIMEOUT)
            fspr_proc_kill(pc->proc, SIGKILL);
    }

    /* Now wait for all the signaled processes to die */
    for (pc = procs; pc; pc = pc->next) {
        if (pc->kill_how != APR_KILL_NEVER)
            (void)fspr_proc_wait(pc->proc, NULL, NULL, APR_WAIT);
    }
}


/*
 * Pool creation/destruction stubs, for people who are running
 * mixed release/debug enviroments.
 */

#if !APR_POOL_DEBUG
APR_DECLARE(void *) fspr_palloc_debug(fspr_pool_t *pool, fspr_size_t size,
                                     const char *file_line)
{
    return fspr_palloc(pool, size);
}

APR_DECLARE(void *) fspr_pcalloc_debug(fspr_pool_t *pool, fspr_size_t size,
                                      const char *file_line)
{
    return fspr_pcalloc(pool, size);
}

APR_DECLARE(void) fspr_pool_clear_debug(fspr_pool_t *pool,
                                       const char *file_line)
{
    fspr_pool_clear(pool);
}

APR_DECLARE(void) fspr_pool_destroy_debug(fspr_pool_t *pool,
                                         const char *file_line)
{
    fspr_pool_destroy(pool);
}

APR_DECLARE(fspr_status_t) fspr_pool_create_ex_debug(fspr_pool_t **newpool,
                                                   fspr_pool_t *parent,
                                                   fspr_abortfunc_t abort_fn,
                                                   fspr_allocator_t *allocator,
                                                   const char *file_line)
{
    return fspr_pool_create_ex(newpool, parent, abort_fn, allocator);
}

#else /* APR_POOL_DEBUG */

#undef fspr_palloc
APR_DECLARE(void *) fspr_palloc(fspr_pool_t *pool, fspr_size_t size);

APR_DECLARE(void *) fspr_palloc(fspr_pool_t *pool, fspr_size_t size)
{
    return fspr_palloc_debug(pool, size, "undefined");
}

#undef fspr_pcalloc
APR_DECLARE(void *) fspr_pcalloc(fspr_pool_t *pool, fspr_size_t size);

APR_DECLARE(void *) fspr_pcalloc(fspr_pool_t *pool, fspr_size_t size)
{
    return fspr_pcalloc_debug(pool, size, "undefined");
}

#undef fspr_pool_clear
APR_DECLARE(void) fspr_pool_clear(fspr_pool_t *pool);

APR_DECLARE(void) fspr_pool_clear(fspr_pool_t *pool)
{
    fspr_pool_clear_debug(pool, "undefined");
}

#undef fspr_pool_destroy
APR_DECLARE(void) fspr_pool_destroy(fspr_pool_t *pool);

APR_DECLARE(void) fspr_pool_destroy(fspr_pool_t *pool)
{
    fspr_pool_destroy_debug(pool, "undefined");
}

#undef fspr_pool_create_ex
APR_DECLARE(fspr_status_t) fspr_pool_create_ex(fspr_pool_t **newpool,
                                             fspr_pool_t *parent,
                                             fspr_abortfunc_t abort_fn,
                                             fspr_allocator_t *allocator);

APR_DECLARE(fspr_status_t) fspr_pool_create_ex(fspr_pool_t **newpool,
                                             fspr_pool_t *parent,
                                             fspr_abortfunc_t abort_fn,
                                             fspr_allocator_t *allocator)
{
    return fspr_pool_create_ex_debug(newpool, parent,
                                    abort_fn, allocator,
                                    "undefined");
}

#endif /* APR_POOL_DEBUG */
