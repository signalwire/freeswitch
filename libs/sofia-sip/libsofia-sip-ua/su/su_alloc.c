/*
 * This file is part of the Sofia-SIP package
 *
 * Copyright (C) 2005 Nokia Corporation.
 *
 * Contact: Pekka Pessi <pekka.pessi@nokia.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation; either version 2.1 of
 * the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA
 *
 */

#include "config.h"

/**@defgroup su_alloc Memory Management Tutorial
 *
 * This page gives a short overview of home-based memory management used
 * with Sofia. Such home-based memory management is useful when a lot of
 * memory blocks are allocated for given task. The allocations are done via
 * the @e memory @e home, which keeps a reference to each block. When the
 * memory home is then freed, it will free all blocks to which it has
 * reference.
 *
 * Typically, there is a @e home @e object which contains a su_home_t
 * structure in the beginning of the object (sort of inheritance from
 * su_home_t):
 * @code
 * struct context {
 *   su_home_t ctx_home[1];
 *   other_t  *ctx_stuff;
 *   ...
 * }
 * @endcode
 *
 * A new home memory pool can be created with su_home_new():
 * @code
 * struct context *ctx = su_home_new(sizeof (struct context));
 * @endcode
 *
 * It is also possible to create a secondary memory pool that can be
 * released separately:
 *
 * @code
 * struct context *ctx = su_home_clone(tophome, sizeof (struct context));
 * @endcode
 *
 * Note that the tophome has a reference to @a ctx structure; whenever
 * tophome is freed, the @a ctx is also freed.
 *
 * You can also create an independent home object by passing NULL as @a
 * tophome argument. This is identical to the call to su_home_new().
 *
 * The memory allocations using @a ctx proceed then as follows:
 * @code
 *    zeroblock = su_zalloc(ctx->ctx_home, sizeof (*zeroblock));
 * @endcode
 *
 * The home memory pool - the home object and all the memory blocks
 * allocated using it - are freed when su_home_unref() is called:
 *
 * @code
 *    su_home_unref(ctx->ctx_home).
 * @endcode
 *
 * @note For historical reasons, su_home_unref() is also known as
 * su_home_zap().
 *
 * As you might have guessed, it is also possible to use reference counting
 * with home objects. The function su_home_ref() increases the reference
 * count, su_home_unref() decreases it. A newly allocated or initialized
 * home object has reference count of 1.
 *
 * @note Please note that while it is possible to create new references to
 * secondary home objects which have a parent home, the secondary home
 * objects will always be destroyed when the parent home is destroyed even
 * if there are other references left to them.
 *
 * The memory blocks in a cloned home object are freed when the object with
 * home itself is freed:
 * @code
 *    su_free(tophome, ctx);
 * @endcode
 *
 * @note
 *
 * The su_home_destroy() function is deprecated as it does not free the home
 * object itself. Like su_home_deinit(), it should be called only on home
 * objects with reference count of 1.
 *
 * The function su_home_init() initializes a home object structure. When the
 * initialized home object is destroyed or deinitialized or its reference
 * count reaches zero, the memory allocate thorugh it reclaimed but the home
 * object structure itself is not freed.
 *
 * @section su_home_destructor_usage Destructors
 *
 * It is possible to give a destructor function to a home object. The
 * destructor releases other resources associated with the home object
 * besides memory. The destructor function will be called when the reference
 * count of home reaches zero (upon calling su_home_unref()) or the home
 * object is otherwise deinitialized (calling su_home_deinit() on
 * objects allocated from stack).
 *
 * @section su_home_move_example Combining Allocations
 *
 * In some cases, an operation that makes multiple memory allocations may
 * fail, making those allocations redundant. If the allocations are made
 * through a temporary home, they can be conveniently freed by calling
 * su_home_deinit(), for instance. If, however, the operation is successful,
 * and one wants to keep the allocations, the allocations can be combined
 * into an existing home with su_home_move(). For example,
 * @code
 * int example(su_home_t *home, ...)
 * {
 *   su_home_t temphome[1] = { SU_HOME_INIT(temphome) };
 *
 *   ... do lot of allocations with temphome ...
 *
 *   if (success)
 *     su_home_move(home, temphome);
 *   su_home_deinit(temphome);
 *
 *   return success;
 * }
 * @endcode
 *
 * Note that the @a temphome is deinitialized in every case, but when
 * operation is successful, the allocations are moved from @a temphome to @a
 * home.
 *
 * @section su_alloc_threadsafe Threadsafe Operation
 *
 * If multiple threads need to access same home object, it must be marked as
 * @e threadsafe by calling su_home_threadsafe() with the home pointer as
 * argument. The threadsafeness is not inherited by clones.
 *
 * The threadsafe home objects can be locked and unlocked with
 * su_home_mutex_lock() and su_home_mutex_unlock(). These operations are
 * no-op on home object that is not threadsafe.
 *
 * @section su_alloc_preloading Preloading a Memory Home
 *
 * In some situations there is quite heavy overhead if the global heap
 * allocator is used. The overhead caused by the large number of small
 * allocations can be reduced by using su_home_preload(): it allocates or
 * preloads some a memory to home to be used as a kind of private heap. The
 * preloaded memory area is then used to satisfy small enough allocations.
 * For instance, the SIP parser typically preloads some 2K of memory when it
 * starts to parse the message.
 *
 * @section su_alloc_stack Using Stack
 *
 * In some situation, it is sensible to use memory allocated from stack for
 * some operations. The su_home_auto() function can be used for that
 * purpose. The memory area from stack is used to satisfy the allocations as
 * far as possible; if it is not enough, allocation is made from heap.
 *
 * The word @e auto refers to the automatic scope; however, the home object
 * that was initialized with su_home_auto() must be explicitly deinitialized
 * with su_home_deinit() or su_home_unref() when the program exits the scope
 * where the stack frame used in su_home_auto() was allocated.
 */

/**@ingroup su_alloc
 * @CFILE su_alloc.c  Home-based memory management.
 *
 * @author Pekka Pessi <Pekka.Pessi@nokia.com>.
 *
 * @date Created: Thu Aug 19 01:12:25 1999 ppessi
 */

#include <sofia-sip/su_config.h>
#include "sofia-sip/su_alloc.h"
#include "sofia-sip/su_alloc_stat.h"
#include "sofia-sip/su_errno.h"

#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <memory.h>
#include <limits.h>

#include <assert.h>

int (*_su_home_locker)(void *mutex);
int (*_su_home_unlocker)(void *mutex);

int (*_su_home_mutex_locker)(void *mutex);
int (*_su_home_mutex_trylocker)(void *mutex);
int (*_su_home_mutex_unlocker)(void *mutex);

void (*_su_home_destroy_mutexes)(void *mutex);

#if HAVE_FREE_NULL
#define safefree(x) free((x))
#else
su_inline void safefree(void *b) { b ? free(b) : (void)0; }
#endif

static inline su_block_t* MEMLOCK(const su_home_t *h) {
  if (h && h->suh_lock) _su_home_locker(h->suh_lock);
  return h->suh_blocks;
}
static inline void* UNLOCK(const su_home_t *h) {
  if (h && h->suh_lock) _su_home_unlocker(h->suh_lock);
  return NULL;
}

#ifdef NDEBUG
#define MEMCHECK 0
#define MEMCHECK_EXTRA 0
#elif !defined(MEMCHECK)
/* Default settings for valgrinding */
#define MEMCHECK 1
#define MEMCHECK_EXTRA 0
#elif !defined(MEMCHECK_EXTRA)
#define MEMCHECK_EXTRA sizeof (size_t)
#endif

enum {
  SUB_N = 31,			/**< Initial size */
  SUB_N_AUTO = 7,		/**< Initial size for autohome */
  SUB_P = 29			/**< Secondary probe.
				 * Secondary probe must be relative prime
				 * with all sub_n values */
};

#define ALIGNMENT (8)
#define __ALIGN(n) (size_t)(((n) + (ALIGNMENT - 1)) & (size_t)~(ALIGNMENT - 1))
#define SIZEBITS (sizeof (unsigned) * 8 - 1)

typedef struct {
  unsigned sua_size:SIZEBITS;	/**< Size of the block */
  unsigned sua_home:1;		/**< Is this another home? */
  unsigned :0;
  void    *sua_data;		/**< Data pointer */
} su_alloc_t;

struct su_block_s {
  su_home_t  *sub_parent;	/**< Parent home */
  char       *sub_preload;	/**< Preload area */
  su_home_stat_t *sub_stats;	/**< Statistics.. */
  void      (*sub_destructor)(void *); /**< Destructor function */
  size_t      sub_ref;		/**< Reference count */
#define REF_MAX SIZE_MAX
  size_t      sub_used;		/**< Number of blocks allocated */
  size_t      sub_n;		/**< Size of hash table  */

  unsigned    sub_prsize:16;	/**< Preload size */
  unsigned    sub_prused:16;	/**< Used from preload */
  unsigned    sub_hauto:1;      /**< "Home" is not from malloc */
  unsigned    sub_auto:1;	/**< struct su_block_s is not from malloc */
  unsigned    sub_preauto:1;	/**< Preload is not from malloc */
  unsigned    sub_auto_all:1;	/**< Everything is from stack! */
  unsigned :0;

  su_alloc_t  sub_nodes[SUB_N];	/**< Pointers to data/lower blocks */
};

static void su_home_check_blocks(su_block_t const *b);

static void su_home_stats_alloc(su_block_t *, void *p, void *preload,
				size_t size, int zero);
static void su_home_stats_free(su_block_t *sub, void *p, void *preload,
			       unsigned size);

static void _su_home_deinit(su_home_t *home);

#define SU_ALLOC_STATS 1

#if SU_ALLOC_STATS
size_t count_su_block_find, count_su_block_find_loop;
size_t size_su_block_find, used_su_block_find;
size_t max_size_su_block_find, max_used_su_block_find;
size_t su_block_find_collision, su_block_find_collision_used,
  su_block_find_collision_size;
#endif

su_inline su_alloc_t *su_block_find(su_block_t const *b, void const *p)
{
  size_t h, h0, probe;

#if SU_ALLOC_STATS
  size_t collision = 0;

  count_su_block_find++;
  size_su_block_find += b->sub_n;
  used_su_block_find += b->sub_used;
  if (b->sub_n > max_size_su_block_find)
    max_size_su_block_find = b->sub_n;
  if (b->sub_used > max_used_su_block_find)
    max_used_su_block_find = b->sub_used;
#endif

  assert(p != NULL);

  h = h0 = (size_t)((uintptr_t)p % b->sub_n);

  probe = (b->sub_n > SUB_P) ? SUB_P : 1;

  do {
    if (b->sub_nodes[h].sua_data == p) {
      su_alloc_t const *retval = &b->sub_nodes[h];
      return (su_alloc_t *)retval; /* discard const */
    }
    h += probe;
    if (h >= b->sub_n)
      h -= b->sub_n;
#if SU_ALLOC_STATS
    if (++collision > su_block_find_collision)
      su_block_find_collision = collision,
	su_block_find_collision_used = b->sub_used,
	su_block_find_collision_size = b->sub_n;
    count_su_block_find_loop++;
#endif
  } while (h != h0);

  return NULL;
}

su_inline su_alloc_t *su_block_add(su_block_t *b, void *p)
{
  size_t h, probe;

  assert(p != NULL);

  h = (size_t)((uintptr_t)p % b->sub_n);

  probe = (b->sub_n > SUB_P) ? SUB_P : 1;

  while (b->sub_nodes[h].sua_data) {
    h += probe;
    if (h >= b->sub_n)
      h -= b->sub_n;
  }

  b->sub_used++;
  b->sub_nodes[h].sua_data = p;

  return &b->sub_nodes[h];
}

su_inline int su_is_preloaded(su_block_t const *sub, char *data)
{
  return
    sub->sub_preload &&
    sub->sub_preload <= data &&
    sub->sub_preload + sub->sub_prsize > data;
}

su_inline int su_alloc_check(su_block_t const *sub, su_alloc_t const *sua)
{
#if MEMCHECK_EXTRA
  size_t size, term;
  assert(sua);
  if (sua) {
    size = (size_t)sua->sua_size;
    memcpy(&term, (char *)sua->sua_data + size, sizeof (term));
    assert(size - term == 0);
    return size - term == 0;
  }
  else
    return 0;
#endif
  return sua != NULL;
}

/** Allocate the block hash table.
 *
 * @internal
 *
 * Allocate a block hash table of @a n elements.
 *
 * @param home  pointer to home object
 * @param n     number of buckets in hash table
 *
 * @return
 *   This function returns a pointer to the allocated hash table or
 *   NULL if an error occurred.
 */
su_inline su_block_t *su_hash_alloc(size_t n)
{
  su_block_t *b = calloc(1, offsetof(su_block_t, sub_nodes[n]));

  if (b) {
    /* Implicit su_home_init(); */
    b->sub_ref = 1;
    b->sub_hauto = 1;
    b->sub_n = n;
  }

  return b;
}

enum sub_zero { do_malloc, do_calloc, do_clone };

/** Allocate a memory block.
 *
 * @internal
 *
 * Precondition: locked home
 *
 * @param home home to allocate
 * @param sub  block structure used to allocate
 * @param size
 * @param zero if true, zero allocated block;
 *             if > 1, allocate a subhome
 *
 */
static
void *sub_alloc(su_home_t *home,
		su_block_t *sub,
		size_t size,
		enum sub_zero zero)
{
  void *data, *preload = NULL;

  assert (size < (((size_t)1) << SIZEBITS));

  if (size >= ((size_t)1) << SIZEBITS)
    return (void)(errno = ENOMEM), NULL;

  if (!size) return NULL;

  if (sub == NULL || 3 * sub->sub_used > 2 * sub->sub_n) {
    /* Resize the hash table */
    size_t i, n, n2;
    su_block_t *b2;

    if (sub)
      n = home->suh_blocks->sub_n, n2 = 4 * n + 3; //, used = sub->sub_used;
    else
      n = 0, n2 = SUB_N; //, used = 0;

#if 0
    printf("su_alloc(home = %p): realloc block hash of size %d\n", home, n2);
#endif

    if (!(b2 = su_hash_alloc(n2)))
      return NULL;

    for (i = 0; i < n; i++) {
      if (sub->sub_nodes[i].sua_data)
	su_block_add(b2, sub->sub_nodes[i].sua_data)[0] = sub->sub_nodes[i];
    }

    if (sub) {
      b2->sub_parent = sub->sub_parent;
      b2->sub_ref = sub->sub_ref;
      b2->sub_preload = sub->sub_preload;
      b2->sub_prsize = sub->sub_prsize;
      b2->sub_prused = sub->sub_prused;
      b2->sub_hauto = sub->sub_hauto;
      b2->sub_preauto = sub->sub_preauto;
      b2->sub_destructor = sub->sub_destructor;
      /* auto_all is not copied! */
      b2->sub_stats = sub->sub_stats;
    }

    home->suh_blocks = b2;

    if (sub && !sub->sub_auto)
      free(sub);
    sub = b2;
  }

  if (sub && zero < do_clone &&
      sub->sub_preload && size <= sub->sub_prsize) {
    /* Use preloaded memory */
    size_t prused = sub->sub_prused + size + MEMCHECK_EXTRA;
    prused = __ALIGN(prused);
    if (prused <= sub->sub_prsize) {
      preload = (char *)sub->sub_preload + sub->sub_prused;
      sub->sub_prused = (unsigned)prused;
    }
  }

  if (preload && zero)
    data = memset(preload, 0, size);
  else if (preload)
    data = preload;
  else if (zero)
    data = calloc(1, size + MEMCHECK_EXTRA);
  else
    data = malloc(size + MEMCHECK_EXTRA);

  if (data) {
    su_alloc_t *sua;

#if MEMCHECK_EXTRA
    size_t term = 0 - size;
    memcpy((char *)data + size, &term, sizeof (term));
#endif

    if (!preload)
      sub->sub_auto_all = 0;

    if (zero >= do_clone) {
      /* Prepare cloned home */
      su_home_t *subhome = data;

      assert(preload == 0);

      subhome->suh_blocks = su_hash_alloc(SUB_N);
      if (!subhome->suh_blocks)
	return (void)safefree(data), NULL;

      subhome->suh_size = (unsigned)size;
      subhome->suh_blocks->sub_parent = home;
      subhome->suh_blocks->sub_hauto = 0;
    }

    /* OK, add the block to the hash table. */

    sua = su_block_add(sub, data); assert(sua);
    sua->sua_size = (unsigned)size;
    sua->sua_home = zero > 1;

    if (sub->sub_stats)
      su_home_stats_alloc(sub, data, preload, size, zero);
  }

  return data;
}

/**Create a new su_home_t object.
 *
 * Create a home object used to collect multiple memory allocations under
 * one handle. The memory allocations made using this home object is freed
 * either when this home is destroyed.
 *
 * The maximum @a size of a home object is INT_MAX (2 gigabytes).
 *
 * @param size    size of home object
 *
 * The memory home object allocated with su_home_new() can be reclaimed with
 * su_home_unref().
 *
 * @return
 * This function returns a pointer to an su_home_t object, or NULL upon
 * an error.
 */
void *su_home_new(isize_t size)
{
  su_home_t *home;

  assert(size >= sizeof (*home));

  if (size < sizeof (*home))
    return (void)(errno = EINVAL), NULL;
  else if (size > INT_MAX)
    return (void)(errno = ENOMEM), NULL;

  home = calloc(1, size);
  if (home) {
    home->suh_size = (int)size;
    home->suh_blocks = su_hash_alloc(SUB_N);
    if (home->suh_blocks)
      home->suh_blocks->sub_hauto = 0;
    else
      safefree(home), home = NULL;
  }

  return home;
}

/** Set destructor function.
 *
 * The destructor function is called after the reference count of a
 * #su_home_t object reaches zero or a home object is deinitialized, but
 * before any of the memory areas within the home object are freed.
 *
 * @since New in @VERSION_1_12_4.
 * Earlier versions had su_home_desctructor() (spelling).
 */
int su_home_destructor(su_home_t *home, void (*destructor)(void *))
{
  int retval = -1;

  if (home) {
    su_block_t *sub = MEMLOCK(home);
    if (sub && sub->sub_destructor == NULL) {
      sub->sub_destructor = destructor;
      retval = 0;
    }
    UNLOCK(home);
  }
  else
    su_seterrno(EFAULT);

  return retval;
}

#undef su_home_desctructor

/** Set destructor function.
 *
 * @deprecated The su_home_destructor() was added in @VERSION_1_12_4. The
 * su_home_desctructor() is now defined as a macro expanding as
 * su_home_destructor(). If you want to compile an application as binary
 * compatible with earlier versions, you have to define su_home_desctructor
 * as itself, e.g.,
 * @code
 * #define su_home_desctructor su_home_desctructor
 * #include <sofia-sip/su_alloc.h>
 * @endcode
 */
int su_home_desctructor(su_home_t *home, void (*destructor)(void *))
{
  return su_home_destructor(home, destructor);
}


#if (defined(HAVE_MEMLEAK_LOG) && (HAVE_MEMLEAK_LOG != 1))
#include "sofia-sip/su_debug.h"


static void *real_su_home_ref(su_home_t const *home)
{
  if (home) {
    su_block_t *sub = MEMLOCK(home);

    if (sub == NULL || sub->sub_ref == 0) {
      assert(sub && sub->sub_ref != 0);
      UNLOCK(home);
      return NULL;
    }

    if (sub->sub_ref != REF_MAX)
      sub->sub_ref++;
    UNLOCK(home);
  }
  else
    su_seterrno(EFAULT);

  return (void *)home;
}


static int real_su_home_unref(su_home_t *home)
{
  su_block_t *sub;

  if (home == NULL)
    return 0;

  sub = MEMLOCK(home);

  if (sub == NULL) {
    /* Xyzzy */
    return 0;
  }
  else if (sub->sub_ref == REF_MAX) {
    UNLOCK(home);
    return 0;
  }
  else if (--sub->sub_ref > 0) {
    UNLOCK(home);
    return 0;
  }
  else if (sub->sub_parent) {
    su_home_t *parent = sub->sub_parent;
    UNLOCK(home);
    su_free(parent, home);
    return 1;
  }
  else {
    int hauto = sub->sub_hauto;
    _su_home_deinit(home);
    if (!hauto)
      safefree(home);
    /* UNLOCK(home); */
    return 1;
  }
}

su_home_t *
_su_home_ref_by(su_home_t *home,
		   char const *file, unsigned line,
		   char const *function)
{
  if (home)
	  SU_DEBUG_0(("%ld %p - su_home_ref() => "MOD_ZU" by %s:%u: %s()\n", pthread_self(),
		home, su_home_refcount(home) + 1, file, line, function));
  return (su_home_t *)real_su_home_ref(home);
}

int
_su_home_unref_by(su_home_t *home,
		    char const *file, unsigned line,
		    char const *function)
{
  if (home) {
    size_t refcount = su_home_refcount(home) - 1;
    int freed =  real_su_home_unref(home);

    if (freed) refcount = 0;
    SU_DEBUG_0(("%ld %p - su_home_unref() => "MOD_ZU" by %s:%u: %s()\n", pthread_self(),
		home, refcount, file, line, function));
    return freed;
  }

  return 0;
}
#else

/** Create a new reference to a home object. */
void *su_home_ref(su_home_t const *home)
{
  if (home) {
    su_block_t *sub = MEMLOCK(home);

    if (sub == NULL || sub->sub_ref == 0) {
      assert(sub && sub->sub_ref != 0);
      UNLOCK(home);
      return NULL;
    }

    if (sub->sub_ref != REF_MAX)
      sub->sub_ref++;
    UNLOCK(home);
  }
  else
    su_seterrno(EFAULT);

  return (void *)home;
}


/**Unreference a su_home_t object.
 *
 * Decrements the reference count on home object and destroys and frees it
 * and the memory allocations using it if the reference count reaches 0.
 *
 * @param home memory pool object to be unreferenced
 *
 * @retval 1 if object was freed
 * @retval 0 if object is still alive
 */
int su_home_unref(su_home_t *home)
{
  su_block_t *sub;

  if (home == NULL)
    return 0;

  sub = MEMLOCK(home);

  if (sub == NULL) {
    /* Xyzzy */
    return 0;
  }
  else if (sub->sub_ref == REF_MAX) {
    UNLOCK(home);
    return 0;
  }
  else if (--sub->sub_ref > 0) {
    UNLOCK(home);
    return 0;
  }
  else if (sub->sub_parent) {
    su_home_t *parent = sub->sub_parent;
    UNLOCK(home);
    su_free(parent, home);
    return 1;
  }
  else {
    int hauto = sub->sub_hauto;
    _su_home_deinit(home);
    if (!hauto)
      safefree(home);
    /* UNLOCK(home); */
    return 1;
  }
}
#endif

/** Return reference count of home. */
size_t su_home_refcount(su_home_t *home)
{
  size_t count = 0;

  if (home) {
    su_block_t *sub = MEMLOCK(home);

    if (sub)
      count = sub->sub_ref;

    UNLOCK(home);
  }

  return count;
}

/**Clone a su_home_t object.
 *
 * Clone a secondary home object used to collect multiple memoryf
 * allocations under one handle. The memory is freed either when the cloned
 * home is destroyed or when the parent home is destroyed.
 *
 * An independent
 * home object is created if NULL is passed as @a parent argument.
 *
 * @param parent  a parent object (may be NULL)
 * @param size    size of home object
 *
 * The memory home object allocated with su_home_clone() can be freed with
 * su_home_unref().
 *
 * @return
 * This function returns a pointer to an su_home_t object, or NULL upon
 * an error.
 */
void *su_home_clone(su_home_t *parent, isize_t size)
{
  su_home_t *home;

  assert(size >= sizeof (*home));

  if (size < sizeof (*home))
    return (void)(errno = EINVAL), NULL;
  else if (size > INT_MAX)
    return (void)(errno = ENOMEM), NULL;

  if (parent) {
    su_block_t *sub = MEMLOCK(parent);
    home = sub_alloc(parent, sub, size, (enum sub_zero)2);
    UNLOCK(parent);
  }
  else {
    home = su_home_new(size);
  }

  return home;
}

/** Return true if home is a clone. */
int su_home_has_parent(su_home_t const *home)
{
  return su_home_parent(home) != NULL;
}

/** Return home's parent home. */
su_home_t *su_home_parent(su_home_t const *home)
{
  su_home_t *parent = NULL;

  if (home && home->suh_blocks) {
    su_block_t *sub = MEMLOCK(home);
    parent = sub->sub_parent;
    UNLOCK(home);
  }

  return parent;
}

/** Allocate a memory block.
 *
 * Allocates a memory block of a given @a size.
 *
 * If @a home is NULL, this function behaves exactly like malloc().
 *
 * @param home  pointer to home object
 * @param size  size of the memory block to be allocated
 *
 * @return
 * This function returns a pointer to the allocated memory block or
 * NULL if an error occurred.
 */
void *su_alloc(su_home_t *home, isize_t size)
{
  void *data;

  if (home) {
    data = sub_alloc(home, MEMLOCK(home), size, (enum sub_zero)0);
    UNLOCK(home);
  }
  else
    data = malloc(size);

  return data;
}

/**Free a memory block.
 *
 * Frees a single memory block. The @a home must be the owner of the memory
 * block (usually the memory home used to allocate the memory block, or NULL
 * if no home was used).
 *
 * @param home  pointer to home object
 * @param data  pointer to the memory block to be freed
 */
void su_free(su_home_t *home, void *data)
{
  if (!data)
    return;

  if (home) {
    su_alloc_t *allocation;
    su_block_t *sub = MEMLOCK(home);

    assert(sub);
    allocation = su_block_find(sub, data);
    assert(allocation);

    if (su_alloc_check(sub, allocation)) {
      void *preloaded = NULL;

      /* Is this preloaded data? */
      if (su_is_preloaded(sub, data))
	preloaded = data;

      if (sub->sub_stats)
	su_home_stats_free(sub, data, preloaded, allocation->sua_size);

      if (allocation->sua_home) {
	su_home_t *subhome = data;
	su_block_t *sub = MEMLOCK(subhome);

	assert(sub->sub_ref != REF_MAX);
	/* assert(sub->sub_ref > 0); */

	sub->sub_ref = 0;	/* Zap all references */

	_su_home_deinit(subhome);
      }

#if MEMCHECK != 0
      memset(data, 0xaa, (size_t)allocation->sua_size);
#endif

      memset(allocation, 0, sizeof (*allocation));
      sub->sub_used--;

      if (preloaded)
	data = NULL;
    }

    UNLOCK(home);
  }

  safefree(data);
}

/** Check if pointer has been allocated through home.
 *
 * @param home   pointer to a memory home
 * @param data   pointer to a memory area possibly allocated though home
 *
 * @NEW_1_12_9
 */
int su_home_check_alloc(su_home_t const *home, void const *data)
{
  int retval = 0;

  if (home && data) {
    su_block_t const *sub = MEMLOCK(home);
    su_alloc_t *allocation = su_block_find(sub, data);

    retval = allocation != NULL;

    UNLOCK(home);
  }

  return retval;
}

/** Check home consistency.
 *
 * Ensures that the home structure and all memory blocks allocated through
 * it are consistent. It can be used to catch memory allocation and usage
 * errors.
 *
 * @param home Pointer to a memory home.
 */
void su_home_check(su_home_t const *home)
{
#if MEMCHECK != 0
  su_block_t const *b = MEMLOCK(home);

  su_home_check_blocks(b);

  UNLOCK(home);
#endif
}

/** Check home blocks. */
static
void su_home_check_blocks(su_block_t const *b)
{
#if MEMCHECK != 0
  if (b) {
    size_t i, used;
    assert(b->sub_used <= b->sub_n);

    for (i = 0, used = 0; i < b->sub_n; i++)
      if (b->sub_nodes[i].sua_data) {
	su_alloc_check(b, &b->sub_nodes[i]), used++;
	if (b->sub_nodes[i].sua_home)
	  su_home_check((su_home_t *)b->sub_nodes[i].sua_data);
      }

    assert(used == b->sub_used);
  }
#endif
}

/**
 * Create an su_home_t object.
 *
 * Creates a home object. A home object is used to collect multiple memory
 * allocations, so that they all can be freed by calling su_home_unref().
 *
 * @return This function returns a pointer to an #su_home_t object, or
 * NULL upon an error.
 */
su_home_t *su_home_create(void)
{
  return su_home_new(sizeof(su_home_t));
}

/** Destroy a home object
 *
 * Frees all memory blocks associated with a home object. Note that the home
 * object structure is not freed.
 *
 * @param home pointer to a home object
 *
 * @deprecated
 * su_home_destroy() is included for backwards compatibility only. Use
 * su_home_unref() instead of su_home_destroy().
 */
void su_home_destroy(su_home_t *home)
{
  if (MEMLOCK(home)) {
    assert(home->suh_blocks);
    assert(home->suh_blocks->sub_ref == 1);
    if (!home->suh_blocks->sub_hauto)
      /* should warn user */;
    home->suh_blocks->sub_hauto = 1;
    _su_home_deinit(home);
    /* UNLOCK(home); */
  }
}

/** Initialize an su_home_t struct.
 *
 * Initializes an su_home_t structure. It can be used when the home
 * structure is allocated from stack or when the home structure is part of
 * an another object.
 *
 * @param home pointer to home object
 *
 * @retval 0 when successful
 * @retval -1 upon an error.
 *
 * @sa SU_HOME_INIT(), su_home_deinit(), su_home_new(), su_home_clone()
 *
 * @bug
 * Prior to @VERSION_1_12_8 the su_home_t structure should have been
 * initialized with SU_HOME_INIT() or otherwise zeroed before calling
 * su_home_init().
 */
int su_home_init(su_home_t *home)
{
  su_block_t *sub;

  if (home == NULL)
    return -1;

  home->suh_blocks = sub = su_hash_alloc(SUB_N);
  home->suh_lock = NULL;

  if (!sub)
    return -1;

  return 0;
}

/** Internal deinitialization */
static
void _su_home_deinit(su_home_t *home)
{
  if (home->suh_blocks) {
    size_t i;
    su_block_t *b;
    void *suh_lock = home->suh_lock;

    home->suh_lock = NULL;

     if (home->suh_blocks->sub_destructor) {
      void (*destructor)(void *) = home->suh_blocks->sub_destructor;
      home->suh_blocks->sub_destructor = NULL;
      destructor(home);
    }

    b = home->suh_blocks;

    su_home_check_blocks(b);

    for (i = 0; i < b->sub_n; i++) {
      if (b->sub_nodes[i].sua_data) {
	if (b->sub_nodes[i].sua_home) {
	  su_home_t *subhome = b->sub_nodes[i].sua_data;
	  su_block_t *subb = MEMLOCK(subhome);

	  assert(subb); assert(subb->sub_ref >= 1);
#if 0
	  if (subb->sub_ref > 0)
	    SU_DEBUG_7(("su_home_unref: subhome %p with destructor %p has still %u refs\n",
			subhome, subb->sub_destructor, subb->sub_ref));
#endif
	  subb->sub_ref = 0;	/* zap them all */
	  _su_home_deinit(subhome);
	}
	else if (su_is_preloaded(b, b->sub_nodes[i].sua_data))
	  continue;
	safefree(b->sub_nodes[i].sua_data);
      }
    }

    if (b->sub_preload && !b->sub_preauto)
      free(b->sub_preload);
    if (b->sub_stats)
      free(b->sub_stats);
    if (!b->sub_auto)
      free(b);

    home->suh_blocks = NULL;

    if (suh_lock) {
      /* Unlock, or risk assert() or leak handles on Windows */
      _su_home_unlocker(suh_lock);
      _su_home_destroy_mutexes(suh_lock);
    }
  }
}

/** Free memory blocks allocated through home.
 *
 * Frees the memory blocks associated with the home object allocated. It
 * does not free the home object itself. Use su_home_unref() to free the
 * home object.
 *
 * @param home pointer to home object
 *
 * @sa su_home_init()
 */
void su_home_deinit(su_home_t *home)
{
  if (MEMLOCK(home)) {
    assert(home->suh_blocks);
    assert(home->suh_blocks->sub_ref == 1);
    assert(home->suh_blocks->sub_hauto);
    _su_home_deinit(home);
    /* UNLOCK(home); */
  }
}

/**Move allocations from a su_home_t object to another.
 *
 * Moves allocations made through the @a src home object under the @a dst
 * home object. It is handy, for example, if an operation allocates some
 * number of blocks that should be freed upon an error. It uses a temporary
 * home and moves the blocks from temporary to a proper home when
 * successful, but frees the temporary home upon an error.
 *
 * If @a src has destructor, it is called before starting to move.
 *
 * @param dst destination home
 * @param src source home
 *
 * @retval 0 if succesful
 * @retval -1 upon an error
 */
int su_home_move(su_home_t *dst, su_home_t *src)
{
  size_t i, n, n2, used;
  su_block_t *s, *d, *d2;

  if (src == NULL || dst == src)
    return 0;

  if (dst) {
    s = MEMLOCK(src); d = MEMLOCK(dst);

    if (s && s->sub_n) {

      if (s->sub_destructor) {
	void (*destructor)(void *) = s->sub_destructor;
	s->sub_destructor = NULL;
	destructor(src);
      }

      if (d)
	used = s->sub_used + d->sub_used;
      else
	used = s->sub_used;

      if (used && (d == NULL || 3 * used > 2 * d->sub_n)) {
	if (d)
	  for (n = n2 = d->sub_n; 3 * used > 2 * n2; n2 = 4 * n2 + 3)
	    ;
	else
	  n = 0, n2 = s->sub_n;

	if (!(d2 = su_hash_alloc(n2))) {
	  UNLOCK(dst); UNLOCK(src);
	  return -1;
	}

	dst->suh_blocks = d2;

      	for (i = 0; i < n; i++)
	  if (d->sub_nodes[i].sua_data)
	    su_block_add(d2, d->sub_nodes[i].sua_data)[0] = d->sub_nodes[i];

	if (d) {
	  d2->sub_parent = d->sub_parent;
	  d2->sub_ref = d->sub_ref;
	  d2->sub_preload = d->sub_preload;
	  d2->sub_prsize = d->sub_prsize;
	  d2->sub_prused = d->sub_prused;
	  d2->sub_preauto = d->sub_preauto;
	  d2->sub_stats = d->sub_stats;
	}

	if (d && !d->sub_auto)
	  free(d);

	d = d2;
      }

      if (s->sub_used) {
	n = s->sub_n;

	for (i = 0; i < n; i++)
	  if (s->sub_nodes[i].sua_data) {
	    su_block_add(d, s->sub_nodes[i].sua_data)[0] = s->sub_nodes[i];
	    if (s->sub_nodes[i].sua_home) {
	      su_home_t *subhome = s->sub_nodes[i].sua_data;
	      su_block_t *subsub = MEMLOCK(subhome);
	      subsub->sub_parent = dst;
	      UNLOCK(subhome);
	    }
	  }

	s->sub_used = 0;

	memset(s->sub_nodes, 0, n * sizeof (s->sub_nodes[0]));
      }

      if (s->sub_stats) {
				/* XXX */
      }
    }

    UNLOCK(dst); UNLOCK(src);
  }
  else {
    s = MEMLOCK(src);

    if (s && s->sub_used) {
      n = s->sub_n;

      for (i = 0; i < n; i++) {
	if (s->sub_nodes[i].sua_data && s->sub_nodes[i].sua_home) {
	  su_home_t *subhome = s->sub_nodes[i].sua_data;
	  su_block_t *subsub = MEMLOCK(subhome);
	  subsub->sub_parent = dst;
	  UNLOCK(subhome);
	}
      }

      s->sub_used = 0;
      memset(s->sub_nodes, 0, s->sub_n * sizeof (s->sub_nodes[0]));

      s->sub_used = 0;
    }

    UNLOCK(src);
  }

  return 0;
}

/** Preload a memory home.
 *
 * The function su_home_preload() preloads a memory home.
 */
void su_home_preload(su_home_t *home, isize_t n, isize_t isize)
{
  su_block_t *sub;

  if (home == NULL)
    return;

  if (home->suh_blocks == NULL)
    su_home_init(home);

  sub = MEMLOCK(home);
  if (!sub->sub_preload) {
    size_t size;
    void *preload;

    size = n * __ALIGN(isize);
    if (size > 65535)		/* We have 16 bits... */
      size = 65535 & (ALIGNMENT - 1);

    preload = malloc(size);

    home->suh_blocks->sub_preload = preload;
    home->suh_blocks->sub_prsize = (unsigned)size;
  }
  UNLOCK(home);
}

/** Preload a memory home from stack.
 *
 * Initializes a memory home using an area allocated from stack. Poor man's
 * alloca().
 */
su_home_t *su_home_auto(void *area, isize_t size)
{
  su_home_t *home;
  su_block_t *sub;
  size_t homesize = __ALIGN(sizeof *home);
  size_t subsize = __ALIGN(offsetof(su_block_t, sub_nodes[SUB_N_AUTO]));
  size_t prepsize;

  char *p = area;

  prepsize = homesize + subsize + (__ALIGN((intptr_t)p) - (intptr_t)p);

  if (area == NULL || size < prepsize)
    return NULL;

  if (size > INT_MAX)
    size = INT_MAX;

  home = memset(p, 0, homesize);
  home->suh_size = (int)size;

  sub = memset(p + homesize, 0, subsize);
  home->suh_blocks = sub;

  if (size > prepsize + 65535)
    size = prepsize + 65535;

  sub->sub_n = SUB_N_AUTO;
  sub->sub_ref = 1;
  sub->sub_preload = p + prepsize;
  sub->sub_prsize = (unsigned)(size - prepsize);
  sub->sub_hauto = 1;
  sub->sub_auto = 1;
  sub->sub_preauto = 1;
  sub->sub_auto_all = 1;

  return home;
}


/** Reallocate a memory block.
 *
 *   Allocates a memory block of @a size bytes.
 *   It copies the old block contents to the new block and frees the old
 *   block.
 *
 *   If @a home is NULL, this function behaves exactly like realloc().
 *
 *   @param home  pointer to memory pool object
 *   @param data  pointer to old memory block
 *   @param size  size of the memory block to be allocated
 *
 * @return
 *   A pointer to the allocated memory block or
 *   NULL if an error occurred.
 */
void *su_realloc(su_home_t *home, void *data, isize_t size)
{
  void *ndata;
  su_alloc_t *sua;
  su_block_t *sub;
  size_t p;
  size_t term = 0 - size;

  if (!home)
    return realloc(data, size);

  if (size == 0) {
    if (data)
      su_free(home, data);
    return NULL;
  }

  sub = MEMLOCK(home);
  if (!data) {
    data = sub_alloc(home, sub, size, (enum sub_zero)0);
    UNLOCK(home);
    return data;
  }

  sua = su_block_find(sub, data);

  if (!su_alloc_check(sub, sua))
    return UNLOCK(home);

  assert(!sua->sua_home);
  if (sua->sua_home)
    return UNLOCK(home);

  if (!su_is_preloaded(sub, data)) {
    ndata = realloc(data, size + MEMCHECK_EXTRA);
    if (ndata) {
      if (sub->sub_stats) {
	su_home_stats_free(sub, data, 0, sua->sua_size);
	su_home_stats_alloc(sub, data, 0, size, 1);
      }

#if MEMCHECK_EXTRA
      memcpy((char *)ndata + size, &term, sizeof (term));
#else
      (void)term;
#endif
      memset(sua, 0, sizeof *sua);
      sub->sub_used--;
      su_block_add(sub, ndata)->sua_size = (unsigned)size;
    }
    UNLOCK(home);

    return ndata;
  }

  p = (char *)data - home->suh_blocks->sub_preload;
  p += sua->sua_size + MEMCHECK_EXTRA;
  p = __ALIGN(p);

  if (p == sub->sub_prused) {
    size_t p2 = (char *)data - sub->sub_preload + size + MEMCHECK_EXTRA;
    p2 = __ALIGN(p2);
    if (p2 <= sub->sub_prsize) {
      /* Extend/reduce existing preload */
      if (sub->sub_stats) {
	su_home_stats_free(sub, data, data, sua->sua_size);
	su_home_stats_alloc(sub, data, data, size, 0);
      }

      sub->sub_prused = (unsigned)p2;
      sua->sua_size = (unsigned)size;

#if MEMCHECK_EXTRA
      memcpy((char *)data + size, &term, sizeof (term));
#endif
      UNLOCK(home);
      return data;
    }
  }
  else if (size < (size_t)sua->sua_size) {
    /* Reduce existing preload */
    if (sub->sub_stats) {
      su_home_stats_free(sub, data, data, sua->sua_size);
      su_home_stats_alloc(sub, data, data, size, 0);
    }
#if MEMCHECK_EXTRA
    memcpy((char *)data + size, &term, sizeof (term));
#endif
    sua->sua_size = (unsigned)size;
    UNLOCK(home);
    return data;
  }

  ndata = malloc(size + MEMCHECK_EXTRA);

  if (ndata) {
    if (p == sub->sub_prused) {
      /* Free preload */
      sub->sub_prused = (char *)data - home->suh_blocks->sub_preload;
      if (sub->sub_stats)
	su_home_stats_free(sub, data, data, sua->sua_size);
    }

    memcpy(ndata, data,
	   (size_t)sua->sua_size < size
	   ? (size_t)sua->sua_size
	   : size);
#if MEMCHECK_EXTRA
    memcpy((char *)ndata + size, &term, sizeof (term));
#endif

    if (sub->sub_stats)
      su_home_stats_alloc(sub, data, 0, size, 1);

    memset(sua, 0, sizeof *sua); sub->sub_used--;

    su_block_add(sub, ndata)->sua_size = (unsigned)size;
  }

  UNLOCK(home);

  return ndata;
}


/**Check if a memory block has been allocated from the @a home.
 *
 * Check if the given memory block has been allocated from the home.
 *
 * @param home pointer to memory pool object
 * @param memory ponter to memory block
 *
 * @retval 1 if @a memory has been allocated from @a home.
 * @retval 0 otherwise
 *
 * @since New in @VERSION_1_12_4.
 */
int su_in_home(su_home_t *home, void const *memory)
{
  su_alloc_t *sua;
  su_block_t *sub;
  int retval = 0;

  if (!home || !memory)
    return 0;

  sub = MEMLOCK(home);

  if (sub) {
    sua = su_block_find(sub, memory);

    retval = su_alloc_check(sub, sua);

    UNLOCK(home);
  }

  return retval;
}


/**Allocate and zero a memory block.
 *
 * Allocates a memory block with a given size from
 * given memory home @a home and zeroes the allocated block.
 *
 *  @param home  pointer to memory pool object
 *  @param size  size of the memory block
 *
 * @note The memory home pointer @a home may be @c NULL. In that case, the
 * allocated memory block is not associated with any memory home, and it
 * must be freed by calling su_free() or free().
 *
 * @return
 * The function su_zalloc() returns a pointer to the allocated memory block,
 * or NULL upon an error.
 */
void *su_zalloc(su_home_t *home, isize_t size)
{
  void *data;

  assert (size >= 0);

  if (home) {
    data = sub_alloc(home, MEMLOCK(home), size, (enum sub_zero)1);
    UNLOCK(home);
  }
  else
    data = calloc(1, size);

  return data;
}

/** Allocate a structure
 *
 * Allocates a structure with a given size, zeros
 * it, and initializes the size field to the given size.  The size field
 * is an int at the beginning of the structure. Note that it has type of int.
 *
 * @param home  pointer to memory pool object
 * @param size  size of the structure
 *
 * @par Example
 * The structure is defined and allocated as follows:
 * @code
 *   struct test {
 *     int   tst_size;
 *     char *tst_name;
 *     void *tst_ptr[3];
 *   };
 *
 *   struct test *t;
 *   ...
 *   t = su_salloc(home, sizeof (*t));
 *   assert(t && t->t_size == sizeof (*t));
 *
 * @endcode
 * After calling su_salloc() we get a pointer t to a struct test,
 * initialized to zero except the tst_size field, which is initialized to
 * sizeof (*t).
 *
 * @return A pointer to the allocated structure, or NULL upon an error.
 */
void *su_salloc(su_home_t *home, isize_t size)
{
  struct { int size; } *retval;

  if (size < sizeof (*retval))
    size = sizeof (*retval);

  if (size > INT_MAX)
    return (void)(errno = ENOMEM), NULL;

  if (home) {
    retval = sub_alloc(home, MEMLOCK(home), size, (enum sub_zero)1);
    UNLOCK(home);
  }
  else
    retval = calloc(1, size);

  if (retval)
    retval->size = (int)size;

  return retval;
}

/** Check if a memory home is threadsafe */
int su_home_is_threadsafe(su_home_t const *home)
{
  return home && home->suh_lock;
}

/** Increase refcount and obtain exclusive lock on home.
 *
 * @note The #su_home_t structure must be created with su_home_new() or
 * su_home_clone(), or initialized with su_home_init() before using this
 * function.
 *
 * In order to enable actual locking, use su_home_threadsafe(), too.
 * Otherwise the su_home_mutex_lock() will just increase the reference
 * count.
 */

#if (defined(HAVE_MEMLEAK_LOG) && (HAVE_MEMLEAK_LOG != 1))
int _su_home_mutex_lock(su_home_t *home, const char *file, unsigned int line, const char *function)
#else
int su_home_mutex_lock(su_home_t *home)
#endif

{
  int error;

  if (home == NULL)
    return su_seterrno(EFAULT);

#if (defined(HAVE_MEMLEAK_LOG) && (HAVE_MEMLEAK_LOG != 1))
  if (home->suh_blocks == NULL || !_su_home_ref_by(home, file, line, function))
#else
  if (home->suh_blocks == NULL || !su_home_ref(home))
#endif
    return su_seterrno(EINVAL);  /* Uninitialized home */

  if (!home->suh_lock)
    return 0;			/* No-op */

  error = _su_home_mutex_locker(home->suh_lock);
  if (error)
    return su_seterrno(error);

  return 0;
}

/** Release exclusive lock on home and decrease refcount (if home is threadsafe).
 *
 * @sa su_home_unlock().
 */

#if (defined(HAVE_MEMLEAK_LOG) && (HAVE_MEMLEAK_LOG != 1))
int _su_home_mutex_unlock(su_home_t *home, const char *file, unsigned int line, const char *function)
#else
int su_home_mutex_unlock(su_home_t *home)
#endif
{
  if (home == NULL)
    return su_seterrno(EFAULT);

  if (home->suh_lock) {
    int error = _su_home_mutex_unlocker(home->suh_lock);
    if (error)
      return su_seterrno(error);
  }

  if (home->suh_blocks == NULL)
    return su_seterrno(EINVAL), -1; /* Uninitialized home */

#if (defined(HAVE_MEMLEAK_LOG) && (HAVE_MEMLEAK_LOG != 1))
  _su_home_unref_by(home, file, line, function);
#else
  su_home_unref(home);
#endif

  return 0;
}


/** Obtain exclusive lock on home without increasing refcount.
 *
 * Unless su_home_threadsafe() has been used to intialize locking on home
 * object the function just returns -1.
 *
 * @return 0 if successful, -1 if not threadsafe, error code otherwise.
 *
 * @sa su_home_mutex_lock(), su_home_unlock(), su_home_trylock().
 *
 * @NEW_1_12_8
 */
int su_home_lock(su_home_t *home)
{
  if (home == NULL)
    return EFAULT;

  if (home->suh_lock == NULL)
    return -1;			/* No-op */

  return _su_home_mutex_locker(home->suh_lock);
}


/** Try to obtain exclusive lock on home without increasing refcount.
 *
 * @return 0 if successful, -1 if not threadsafe,
 * EBUSY if already locked, error code otherwise.
 *
 * @sa su_home_lock(), su_home_unlock().
 *
 * @NEW_1_12_8
 */
int su_home_trylock(su_home_t *home)
{
  if (home == NULL)
    return EFAULT;

  if (home->suh_lock == NULL)
    return -1;			/* No-op */

  return _su_home_mutex_trylocker(home->suh_lock);
}


/** Release exclusive lock on home.
 *
 * Release lock without decreasing refcount.
 *
 * @return 0 if successful, -1 if not threadsafe, error code otherwise.
 *
 * @sa su_home_lock(), su_home_trylock(), su_home_mutex_unlock().
 *
 * @NEW_1_12_8
 */
int su_home_unlock(su_home_t *home)
{
  if (home == NULL)
    return EFAULT;

  if (home->suh_lock == NULL)
    return -1;			/* No-op */

  return _su_home_mutex_unlocker(home->suh_lock);
}


/** Initialize statistics structure */
void su_home_init_stats(su_home_t *home)
{
  su_block_t *sub;
  size_t size;

  if (home == NULL)
    return;

  sub = home->suh_blocks;

  if (!sub)
    sub = home->suh_blocks = su_hash_alloc(SUB_N);
  if (!sub)
    return;

  if (!sub->sub_stats) {
    size = sizeof (*sub->sub_stats);
    sub->sub_stats = malloc(size);
    if (!sub->sub_stats)
      return;
  }
  else
    size = sub->sub_stats->hs_size;

  memset(sub->sub_stats, 0, size);
  sub->sub_stats->hs_size = (int)size;
  sub->sub_stats->hs_blocksize = sub->sub_n;
}

/** Retrieve statistics from memory home.
 */
void su_home_get_stats(su_home_t *home, int include_clones,
		       su_home_stat_t *hs,
		       isize_t size)
{
  su_block_t *sub;

  if (hs == NULL || size < (sizeof hs->hs_size))
    return;

  memset((void *)hs, 0, size);

  sub = MEMLOCK(home);

  if (sub && sub->sub_stats) {
    int sub_size = sub->sub_stats->hs_size;
    if (sub_size > (int)size)
      sub_size = (int)size;
    sub->sub_stats->hs_preload.hsp_size = sub->sub_prsize;
    sub->sub_stats->hs_preload.hsp_used = sub->sub_prused;
    memcpy(hs, sub->sub_stats, sub_size);
    hs->hs_size = sub_size;
  }

  UNLOCK(home);
}

static
void su_home_stats_alloc(su_block_t *sub, void *p, void *preload,
			 size_t size, int zero)
{
  su_home_stat_t *hs = sub->sub_stats;

  size_t rsize = __ALIGN(size);

  hs->hs_rehash += (sub->sub_n != hs->hs_blocksize);
  hs->hs_blocksize = sub->sub_n;

  hs->hs_clones += zero > 1;

  if (preload) {
    hs->hs_allocs.hsa_preload++;
    return;
  }

  hs->hs_allocs.hsa_number++;
  hs->hs_allocs.hsa_bytes += size;
  hs->hs_allocs.hsa_rbytes += rsize;
  if (hs->hs_allocs.hsa_rbytes > hs->hs_allocs.hsa_maxrbytes)
    hs->hs_allocs.hsa_maxrbytes = hs->hs_allocs.hsa_rbytes;

  hs->hs_blocks.hsb_number++;
  hs->hs_blocks.hsb_bytes += size;
  hs->hs_blocks.hsb_rbytes += rsize;
}

static
void su_home_stats_free(su_block_t *sub, void *p, void *preload,
			unsigned size)
{
  su_home_stat_t *hs = sub->sub_stats;

  size_t rsize = __ALIGN(size);

  if (preload) {
    hs->hs_frees.hsf_preload++;
    return;
  }

  hs->hs_frees.hsf_number++;
  hs->hs_frees.hsf_bytes += size;
  hs->hs_frees.hsf_rbytes += rsize;

  hs->hs_blocks.hsb_number--;
  hs->hs_blocks.hsb_bytes -= size;
  hs->hs_blocks.hsb_rbytes -= rsize;
}

void su_home_stat_add(su_home_stat_t total[1], su_home_stat_t const hs[1])
{
  total->hs_clones               += hs->hs_clones;
  total->hs_rehash               += hs->hs_rehash;

  if (total->hs_blocksize < hs->hs_blocksize)
    total->hs_blocksize = hs->hs_blocksize;

  total->hs_allocs.hsa_number    += hs->hs_allocs.hsa_number;
  total->hs_allocs.hsa_bytes     += hs->hs_allocs.hsa_bytes;
  total->hs_allocs.hsa_rbytes    += hs->hs_allocs.hsa_rbytes;
  total->hs_allocs.hsa_maxrbytes += hs->hs_allocs.hsa_maxrbytes;

  total->hs_frees.hsf_number     += hs->hs_frees.hsf_number;
  total->hs_frees.hsf_bytes      += hs->hs_frees.hsf_bytes;
  total->hs_frees.hsf_rbytes     += hs->hs_frees.hsf_rbytes;

  total->hs_blocks.hsb_number    += hs->hs_blocks.hsb_number;
  total->hs_blocks.hsb_bytes     += hs->hs_blocks.hsb_bytes;
  total->hs_blocks.hsb_rbytes    += hs->hs_blocks.hsb_rbytes;
}
