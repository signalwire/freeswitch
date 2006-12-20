/*
 * libEtPan! -- a mail stuff library
 *
 * Copyright (C) 2001, 2005 - DINH Viet Hoa
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the libEtPan! project nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHORS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHORS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * $Id: mailsem.c,v 1.11 2006/05/22 13:39:40 hoa Exp $
 */

#ifdef HAVE_CONFIG_H
#	include <config.h>
#endif

#include "mailsem.h"
#include <stdlib.h>
#include <stdio.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>

#ifdef LIBETPAN_REENTRANT
#include <pthread.h>
#include <semaphore.h>
#endif

struct mailsem_internal {
  /* Current count of the semaphore. */
  unsigned int count;
  
  /* Number of threads that have called <sema_wait>. */
  unsigned long waiters_count;
  
#ifdef LIBETPAN_REENTRANT
  /* Serialize access to <count> and <waiters_count>. */
  pthread_mutex_t lock;
  
   /* Condition variable that blocks the <count> 0. */
  pthread_cond_t count_nonzero;
#endif
};

static int mailsem_internal_init(struct mailsem_internal * s,
    unsigned int initial_count)
{
#ifdef LIBETPAN_REENTRANT
  int r;
  
  r = pthread_mutex_init(&s->lock, NULL);
  if (r != 0)
    goto err;
  
  r = pthread_cond_init(&s->count_nonzero, NULL);
  if (r != 0)
    goto destroy_mutex;
  
  s->count = initial_count;
  s->waiters_count = 0;
  
  return 0;
  
 destroy_mutex:
  pthread_mutex_destroy(&s->lock);
 err:
  return -1;
#else
  return -1;
#endif
}

static void mailsem_internal_destroy(struct mailsem_internal * s)
{
#ifdef LIBETPAN_REENTRANT
  pthread_cond_destroy(&s->count_nonzero);
  pthread_mutex_destroy(&s->lock);
#endif
}

int mailsem_internal_wait(struct mailsem_internal * s)
{
#ifdef LIBETPAN_REENTRANT
  int r;
  
  /* Acquire mutex to enter critical section. */
  r = pthread_mutex_lock(&s->lock);
  if (r != 0)
    goto err;
  
  /* Keep track of the number of waiters so that <sema_post> works correctly. */
  s->waiters_count ++;
  
  /* Wait until the semaphore count is > 0, then atomically release */
  /* <lock> and wait for <count_nonzero> to be signaled. */
  while (s->count == 0) {
    r = pthread_cond_wait(&s->count_nonzero, &s->lock);
    if (r != 0)
      goto unlock;
  }
  
  /* <s->lock> is now held. */
  
  /* Decrement the waiters count. */
  s->waiters_count --;
  
  /* Decrement the semaphore's count. */
  s->count --;
  
  /* Release mutex to leave critical section. */
  pthread_mutex_unlock(&s->lock);
  
  return 0;
  
 unlock:
  pthread_mutex_unlock(&s->lock);
 err:
  return -1;
#else
  return -1;
#endif
}

static int mailsem_internal_post(struct mailsem_internal * s)
{
#ifdef LIBETPAN_REENTRANT
  int r;
  
  r = pthread_mutex_lock(&s->lock);
  if (r != 0)
    goto err;
  
  /* Always allow one thread to continue if it is waiting. */
  if (s->waiters_count > 0) {
    r = pthread_cond_signal(&s->count_nonzero);
    if (r != 0)
      goto unlock;
  }
  
  /* Increment the semaphore's count. */
  s->count ++;
  
  pthread_mutex_unlock(&s->lock);
  
  return 0;
  
 unlock:
  pthread_mutex_unlock(&s->lock);
 err:
  return -1;
#else
  return -1;
#endif
}

enum {
  SEMKIND_SEMOPEN,
  SEMKIND_SEMINIT,
  SEMKIND_INTERNAL
};

#if 0
#define SEMNAME_LEN 64

struct mailsem * mailsem_new(void)
{
#ifdef LIBETPAN_REENTRANT
  struct mailsem * sem;
  int r;
  
  sem = malloc(sizeof(* sem));
  if (sem == NULL)
    goto err;
  
  sem->sem_sem = malloc(sizeof(sem_t));
  if (sem->sem_sem == NULL)
    goto free_sem;
  
  r = sem_init(sem->sem_sem, 0, 0);
  if (r < 0) {
    char name[SEMNAME_LEN];
    pid_t pid;
    
    free(sem->sem_sem);
    
    pid = getpid();
    snprintf(name, sizeof(name), "sem-%p-%i", sem, pid);
    
#ifndef __CYGWIN__
    sem->sem_sem = sem_open(name, O_CREAT | O_EXCL, 0700, 0);
    if (sem->sem_sem == (sem_t *) SEM_FAILED)
      goto free_sem;
    
    sem->sem_kind = SEMKIND_SEMOPEN;
#else
    goto free_sem;
#endif
  }
  else {
    sem->sem_kind = SEMKIND_SEMINIT;
  }
  
  return sem;
  
 free_sem:
    free(sem);
 err:
  return NULL;
#else
  return NULL;
#endif
}

void mailsem_free(struct mailsem * sem)
{
#ifdef LIBETPAN_REENTRANT
  if (sem->sem_kind == SEMKIND_SEMOPEN) {
    char name[SEMNAME_LEN];
    pid_t pid;
    
    pid = getpid();
    
#ifndef __CYGWIN__
    sem_close((sem_t *) sem->sem_sem);
    snprintf(name, sizeof(name), "sem-%p-%i", sem, pid);
    sem_unlink(name);
#endif
  }
  else {
    sem_destroy((sem_t *) sem->sem_sem);
    free(sem->sem_sem);
  }
  free(sem);
#endif
}

int mailsem_up(struct mailsem * sem)
{
#ifdef LIBETPAN_REENTRANT
  return sem_post((sem_t *) sem->sem_sem);
#else
  return -1;
#endif
}

int mailsem_down(struct mailsem * sem)
{
#ifdef LIBETPAN_REENTRANT
  return sem_wait((sem_t *) sem->sem_sem);
#else
  return -1;
#endif
}
#endif

struct mailsem * mailsem_new(void)
{
  struct mailsem * sem;
  int r;
  
  sem = malloc(sizeof(* sem));
  if (sem == NULL)
    goto err;
  
  sem->sem_sem = malloc(sizeof(struct mailsem_internal));
  if (sem->sem_sem == NULL)
    goto free;
  
  r = mailsem_internal_init(sem->sem_sem, 0);
  if (r < 0)
    goto free_sem;
  
  return sem;
  
 free_sem:
  free(sem->sem_sem);
 free:
  free(sem);
 err:
  return NULL;
}

void mailsem_free(struct mailsem * sem)
{
  mailsem_internal_destroy(sem->sem_sem);
  free(sem->sem_sem);
  free(sem);
}

int mailsem_up(struct mailsem * sem)
{
  return mailsem_internal_post(sem->sem_sem);
}

int mailsem_down(struct mailsem * sem)
{
  return mailsem_internal_wait(sem->sem_sem);
}
