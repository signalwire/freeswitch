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

/**@ingroup su_alloc
 * @CFILE su_alloc_lock.c
 * @brief Thread-locking for su_alloc module.
 *
 * @author Pekka Pessi <Pekka.Pessi@nokia.com>
 *
 * @date Created: Fri Feb 23 17:38:11 2001 ppessi
 */

#include "config.h"

#include <sofia-sip/su_alloc.h>
#include <sofia-sip/su.h>

#if SU_HAVE_PTHREADS
#include <pthread.h>
#include <assert.h>
#include <stdlib.h>

extern int (*_su_home_locker)(void *mutex);
extern int (*_su_home_unlocker)(void *mutex);

extern int (*_su_home_mutex_locker)(void *mutex);
extern int (*_su_home_mutex_trylocker)(void *mutex);
extern int (*_su_home_mutex_unlocker)(void *mutex);

extern void (*_su_home_destroy_mutexes)(void *mutex);

/** Mutex */
static int mutex_locker(void *_mutex)
{
  pthread_mutex_t *mutex = _mutex;
  return pthread_mutex_lock(mutex + 1);
}

static int mutex_trylocker(void *_mutex)
{
  pthread_mutex_t *mutex = _mutex;
  return pthread_mutex_trylock(mutex + 1);
}

static int mutex_unlocker(void *_mutex)
{
  pthread_mutex_t *mutex = _mutex;
  return pthread_mutex_unlock(mutex + 1);
}

static void mutex_destroy(void *_mutex)
{
  pthread_mutex_t *mutex = _mutex;
  pthread_mutex_destroy(mutex + 0);
  pthread_mutex_destroy(mutex + 1);
  free(_mutex);
}
#endif


/** Convert su_home_t object to a thread-safe one.
 *
 * Convert a memory home object as thread-safe by allocating mutexes and
 * modifying function pointers in su_alloc.c module.
 *
 * @param home memory home object to be converted thread-safe.
 *
 * @retval 0 when successful,
 * @retval -1 upon an error.
 */
int su_home_threadsafe(su_home_t *home)
{
  pthread_mutex_t *mutex;

  if (home == NULL)
    return su_seterrno(EFAULT);

  if (home->suh_lock)		/* Already? */
    return 0;

#if 0				/* Allow threadsafe subhomes */
  assert(!su_home_has_parent(home));
  if (su_home_has_parent(home))
    return su_seterrno(EINVAL);
#endif

#if SU_HAVE_PTHREADS
  if (!_su_home_unlocker) {
    /* Avoid linking pthread library just for memory management */
    _su_home_mutex_locker = mutex_locker;
    _su_home_mutex_trylocker = mutex_trylocker;
    _su_home_mutex_unlocker = mutex_unlocker;
    _su_home_locker = (int (*)(void *))pthread_mutex_lock;
    _su_home_unlocker = (int (*)(void *))pthread_mutex_unlock;
    _su_home_destroy_mutexes = mutex_destroy;
  }

  mutex = calloc(1, 2 * (sizeof *mutex));
  assert(mutex);
  if (mutex) {
    /* Mutex for memory operations */
    pthread_mutex_init(mutex, NULL);
    /* Mutex used for explicit locking */
    pthread_mutex_init(mutex + 1, NULL);
    home->suh_lock = (void *)mutex;
    return 0;
  }
#else
  su_seterrno(ENOSYS);
#endif

  return -1;
}
