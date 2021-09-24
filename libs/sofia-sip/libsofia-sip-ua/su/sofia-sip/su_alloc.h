/*
 * This file is part of the Sofia-SIP package
 *
 * Copyright (C) 2006 Nokia Corporation.
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

#ifndef SU_ALLOC_H  /** Defined when <sofia-sip/su_alloc.h> has been included. */
#define SU_ALLOC_H

/**@ingroup su_alloc
 *
 * @file sofia-sip/su_alloc.h Home-based memory management interface
 *
 * @author Pekka Pessi <Pekka.Pessi@nokia.com>
 *
 * @date Created: Thu Aug 19 01:12:25 1999 ppessi
 */

#ifndef SU_TYPES_H
#include <sofia-sip/su_types.h>
#endif

#include <stdarg.h>

SOFIA_BEGIN_DECLS

#ifndef SU_HOME_T
#define SU_HOME_T struct su_home_s
#endif

/** Memory home type. */
typedef SU_HOME_T su_home_t;
typedef struct su_block_s su_block_t;

/** Thread-locking function. @internal */
typedef struct su_alock su_alock_t;

/** Memory home structure */
struct su_home_s {
  int         suh_size;
  su_block_t *suh_blocks;
  su_alock_t *suh_lock;
};

#define SU_HOME_INIT(obj) { 0, NULL, NULL }

SU_DLL void *su_home_new(isize_t size)
     __attribute__((__malloc__));

#if (defined(HAVE_MEMLEAK_LOG) && (HAVE_MEMLEAK_LOG != 1))

int _su_home_mutex_lock(su_home_t *home, const char *file, unsigned int line, const char *function);
int _su_home_mutex_unlock(su_home_t *home, const char *file, unsigned int line, const char *function);

#define su_home_mutex_lock(home) \
  _su_home_mutex_lock((home), __FILE__, __LINE__, __func__)

#define su_home_mutex_unlock(home) \
  _su_home_mutex_unlock((home), __FILE__, __LINE__, __func__)


su_home_t *_su_home_ref_by(
  su_home_t *home, char const *file, unsigned line, char const *by);
int _su_home_unref_by(
  su_home_t *home, char const *file, unsigned line, char const *by);

#define su_home_ref(home) \
  _su_home_ref_by((home), __FILE__, __LINE__, __func__)
#define su_home_unref(home) \
  _su_home_unref_by((home), __FILE__, __LINE__, __func__)

#else
SU_DLL void *su_home_ref(su_home_t const *);
SU_DLL int su_home_unref(su_home_t *);
#endif

SU_DLL size_t su_home_refcount(su_home_t *home);

SU_DLL int su_home_destructor(su_home_t *, void (*)(void *));

SU_DLL int su_home_desctructor(su_home_t *, void (*)(void *));
#ifndef su_home_desctructor
/* This has typo in before 1.12.4 */
#define su_home_desctructor(home, destructor) \
        su_home_destructor((home), (destructor))
#endif

SU_DLL void *su_home_clone(su_home_t *parent, isize_t size)
     __attribute__((__malloc__));

SU_DLL int  su_home_init(su_home_t *h);

SU_DLL void su_home_deinit(su_home_t *h);

SU_DLL void su_home_preload(su_home_t *h, isize_t n, isize_t size);

SU_DLL su_home_t *su_home_auto(void *area, isize_t size);

#define SU_HOME_AUTO_SIZE(n)				\
  (((n) + ((sizeof(su_home_t) + 7) & (size_t)~8) +	\
    ((3 * sizeof (void *) + 4 * sizeof(unsigned) +	\
      7 * (sizeof (long) + sizeof(void *)) + 7) & (size_t)~8)) 	\
    / sizeof(su_home_t))

SU_DLL int su_home_move(su_home_t *dst, su_home_t *src);

SU_DLL int su_home_threadsafe(su_home_t *home);

SU_DLL int su_home_has_parent(su_home_t const *home);

SU_DLL su_home_t *su_home_parent(su_home_t const *home);

SU_DLL void su_home_check(su_home_t const *home);

SU_DLL int su_home_check_alloc(su_home_t const *home, void const *data);

#if (!defined(HAVE_MEMLEAK_LOG) || (HAVE_MEMLEAK_LOG != 1))
SU_DLL int su_home_mutex_lock(su_home_t *home);

SU_DLL int su_home_mutex_unlock(su_home_t *home);
#endif

SU_DLL int su_home_lock(su_home_t *home);
SU_DLL int su_home_trylock(su_home_t *home);
SU_DLL int su_home_unlock(su_home_t *home);

SU_DLL void *su_alloc(su_home_t *h, isize_t size)
     __attribute__((__malloc__));
SU_DLL void *su_zalloc(su_home_t *h, isize_t size)
     __attribute__((__malloc__));
SU_DLL void *su_salloc(su_home_t *h, isize_t size)
     __attribute__((__malloc__));
SU_DLL void *su_realloc(su_home_t *h, void *data, isize_t size)
     __attribute__((__malloc__));
SU_DLL int su_in_home(su_home_t *h, void const *data);

SU_DLL char *su_strdup(su_home_t *home, char const *s)
     __attribute__((__malloc__));
SU_DLL char *su_strcat(su_home_t *home, char const *s1, char const *s2)
     __attribute__((__malloc__));
SU_DLL char *su_strndup(su_home_t *home, char const *s, isize_t n)
     __attribute__((__malloc__));

SU_DLL char *su_strcat_all(su_home_t *home, ...)
     __attribute__((__malloc__));

SU_DLL char *su_sprintf(su_home_t *home, char const *fmt, ...)
     __attribute__ ((__malloc__, __format__ (printf, 2, 3)));

SU_DLL char *su_vsprintf(su_home_t *home, char const *fmt, va_list ap)
     __attribute__((__malloc__));

/* free an independent block */
SU_DLL void su_free(su_home_t *h, void *);

/** Check if a memory home is threadsafe */
SU_DLL int su_home_is_threadsafe(su_home_t const *home);

/* ---------------------------------------------------------------------- */
/* Deprecated */

SU_DLL su_home_t *su_home_create(void)
     __attribute__((__malloc__));
SU_DLL void su_home_destroy(su_home_t *h);

#define su_home_zap(h) su_home_unref((h))

SOFIA_END_DECLS

#endif /* ! defined(SU_ALLOC_H) */
