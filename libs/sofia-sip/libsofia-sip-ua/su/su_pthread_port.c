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

/**@ingroup su_wait
 * @CFILE su_pthread_port.c
 *
 * OS-Independent Syncronization Interface with pthreads
 *
 * This implements #su_msg_t message passing functionality using pthreads.
 *
 * @author Pekka Pessi <Pekka.Pessi@nokia.com>
 * @author Kai Vehmanen <kai.vehmanen@nokia.com>
 *
 * @date Created: Tue Sep 14 15:51:04 1999 ppessi
 */

#include "config.h"

#include <stdlib.h>
#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <limits.h>
#include <errno.h>

#define su_pthread_port_s su_port_s
#define SU_CLONE_T su_msg_t

#include "sofia-sip/su.h"
#include "su_port.h"
#include "sofia-sip/su_alloc.h"

#if 1
#define PORT_LOCK_DEBUG(x)  ((void)0)
#else
#define PORT_LOCK_DEBUG(x)  printf x
#endif

#define SU_TASK_COPY(d, s, by) (void)((d)[0]=(s)[0], \
  (s)->sut_port?(void)su_port_incref(s->sut_port, #by):(void)0)

/**@internal
 *
 * Initializes a message port. It creates a mailbox used to wake up the
 * thread waiting on the port if needed. Currently, the mailbox is a
 * socketpair or an UDP socket connected to itself.
 */
int su_pthread_port_init(su_port_t *self, su_port_vtable_t const *vtable)
{
  SU_DEBUG_9(("su_pthread_port_init(%p, %p) called\n",
	      (void *)self, (void *)vtable));

  pthread_mutex_init(self->sup_obtained, NULL);

  return su_base_port_init(self, vtable);
}


/** @internal Deinit a base implementation of port. */
void su_pthread_port_deinit(su_port_t *self)
{
  assert(self);

  su_base_port_deinit(self);

#if 0
  pthread_mutex_destroy(self->sup_runlock);
  pthread_cond_destroy(self->sup_resume);
#endif
  pthread_mutex_destroy(self->sup_obtained);
}

void su_pthread_port_lock(su_port_t *self, char const *who)
{
  PORT_LOCK_DEBUG(("%p at %s locking(%p)...",
		   (void *)pthread_self(), who, self));

  su_home_lock(self->sup_base->sup_home);

  PORT_LOCK_DEBUG((" ...%p at %s locked(%p)...",
		   (void *)pthread_self(), who, self));
}

void su_pthread_port_unlock(su_port_t *self, char const *who)
{
  su_home_unlock(self->sup_base->sup_home);

  PORT_LOCK_DEBUG((" ...%p at %s unlocked(%p)\n",
		   (void *)pthread_self(), who, self));
}

/** @internal
 *
 * Change or query ownership of the port object.
 *
 * @param self pointer to a port object
 * @param op operation
 *
 * @ERRORS
 * @ERROR EALREADY port already has an owner (or has no owner)
 */
int su_pthread_port_thread(su_port_t *self, enum su_port_thread_op op)
{
  pthread_t me = pthread_self();

  switch (op) {

  case su_port_thread_op_is_obtained:
    if (self->sup_thread == 0)
      return 0;			/* No thread has obtained the port */
    else if (pthread_equal(self->sup_tid, me))
      return 2;			/* Current thread has obtained the port */
    else
      return 1;			/* A thread has obtained the port */

  case su_port_thread_op_release:
    if (!self->sup_thread || !pthread_equal(self->sup_tid, me))
      return errno = EALREADY, -1;
    self->sup_thread = 0;
    pthread_mutex_unlock(self->sup_obtained);
    return 0;

  case su_port_thread_op_obtain:
    su_home_threadsafe(su_port_home(self));
    pthread_mutex_lock(self->sup_obtained);
    self->sup_tid = me;
    self->sup_thread = 1;
    return 0;

  default:
    return errno = ENOSYS, -1;
  }
}

/* -- Clones ------------------------------------------------------------ */

struct clone_args
{
  su_port_create_f*create;
  su_root_t       *parent;
  su_root_magic_t *magic;
  su_root_init_f   init;
  su_root_deinit_f deinit;
  pthread_mutex_t  mutex[1];
  pthread_cond_t   cv[1];
  int              retval;
  su_msg_r         clone;
};

static void *su_pthread_port_clone_main(void *varg);
static void su_pthread_port_return_to_parent(struct clone_args *arg,
					     int retval);
static su_msg_function su_pthread_port_clone_break;

/* Structure used to synchronize parent and clone in su_clone_wait() */
struct su_pthread_port_waiting_parent {
  pthread_mutex_t deinit[1];
  pthread_mutex_t mutex[1];
  pthread_cond_t cv[1];
  int waiting;
};

/** Start a clone task running under a pthread.
 *
 * @internal
 *
 * Allocates and initializes a sub-task with its own pthread. The sub-task is
 * represented by clone handle to the rest of the application. The function
 * su_clone_start() returns the clone handle in @a return_clone. The clone
 * handle is used to communicate with the newly created clone task using
 * messages.
 *
 * A new #su_root_t object is created for the sub-task with the @a magic as
 * the root context pointer. Because the sub-task may or may not have its
 * own thread, all its activity must be scheduled via this root object. In
 * other words, the sub-task can be schedule
 * -# I/O events with su_root_register()
 * -# timers with su_timer_set(), su_timer_set_at() or su_timer_run()
 * -# messages with su_msg_send().
 *
 * Messages can also be used to pass information between tasks or threads.
 *
 * After the new thread has been launched, the initialization routine is
 * executed by the newly created thread. The calling thread blocks until
 * the initialization routine completes. If the initialization routine
 * returns #su_success (0), the sub-task is considered to be created
 * successfully. After the successful initialization, the sub-task continues
 * to execeute the function su_root_run().
 *
 * If the initalization function @a init fails, the sub-task (either the
 * newly created thread or the current thread executing the su_clone_start()
 * function) calls the deinitialization function, and su_clone_start()
 * returns NULL.
 *
 * @param parent   root to be cloned (may be NULL if multi-threaded)
 * @param return_clone reference to a clone [OUT]
 * @param magic    pointer to user data
 * @param init     initialization function
 * @param deinit   deinitialization function
 *
 * @return 0 if successfull, -1 upon an error.
 *
 * @sa su_root_threading(), su_clone_task(), su_clone_stop(), su_clone_wait(),
 * su_clone_forget().
 *
 */
int su_pthreaded_port_start(su_port_create_f *create,
			    su_root_t *parent,
			    su_clone_r return_clone,
			    su_root_magic_t *magic,
			    su_root_init_f init,
			    su_root_deinit_f deinit)
{
  struct clone_args arg = {
    /* create: */ NULL,
    /* parent: */ NULL,
    /* magic: */  NULL,
    /* init: */   NULL,
    /* deinit: */ NULL,
    /* mutex: */  { PTHREAD_MUTEX_INITIALIZER },
#if HAVE_OPEN_C
/* cv: */     { _ENeedsNormalInit, NULL },
#else
    /* cv: */     { PTHREAD_COND_INITIALIZER },
#endif
    /* retval: */ -1,
    /* clone: */  SU_MSG_R_INIT,
  };

  int thread_created = 0;
  pthread_t tid;

  arg.create = create;
  arg.parent = parent;
  arg.magic = magic;
  arg.init = init;
  arg.deinit = deinit;

  pthread_mutex_lock(arg.mutex);
  if (pthread_create(&tid, NULL, su_pthread_port_clone_main, &arg) == 0) {
    pthread_cond_wait(arg.cv, arg.mutex);
    thread_created = 1;
  }
  pthread_mutex_unlock(arg.mutex);

  pthread_mutex_destroy(arg.mutex);
  pthread_cond_destroy(arg.cv);

  if (arg.retval != 0) {
    if (thread_created)
      pthread_join(tid, NULL);
    return -1;
  }

  *return_clone = *arg.clone;

  return 0;
}

/** Main function for clone thread.
 *
 * @internal
 */
static void *su_pthread_port_clone_main(void *varg)
{
  struct clone_args *arg = (struct clone_args *)varg;
  su_task_r task;
  int zap = 1;

#if SU_HAVE_WINSOCK
  su_init();
#endif

  task->sut_port = arg->create();

  if (task->sut_port) {
    task->sut_root = su_salloc(su_port_home(task->sut_port),
			       sizeof *task->sut_root);
    if (task->sut_root) {

      task->sut_root->sur_threading = 1;	/* By default */

      SU_TASK_COPY(task->sut_root->sur_parent, su_root_task(arg->parent),
		   su_pthread_port_clone_main);
      SU_TASK_COPY(task->sut_root->sur_task, task,
		   su_pthread_port_clone_main);

      if (su_msg_create(arg->clone,
			task,
			su_root_task(arg->parent),
			su_pthread_port_clone_break,
			0) == 0) {
	task->sut_root->sur_magic = arg->magic;
	task->sut_root->sur_deinit = arg->deinit;

	su_root_set_max_defer(task->sut_root, 
			      su_root_get_max_defer(arg->parent));

	if (arg->init(task->sut_root, arg->magic) == 0) {
	  su_pthread_port_return_to_parent(arg, 0), arg = NULL;

	  su_root_run(task->sut_root); /* Do the work */

	  /* Cleanup */
	  if (task->sut_port->sup_waiting_parent) {
	    struct su_pthread_port_waiting_parent *mom;

	    mom = task->sut_port->sup_waiting_parent;
	    pthread_mutex_lock(mom->mutex);
	    mom->waiting = 0;
	    pthread_cond_signal(mom->cv);
	    pthread_mutex_unlock(mom->mutex);

	    pthread_mutex_lock(mom->deinit);
	    su_port_getmsgs(task->sut_port);
	    pthread_mutex_unlock(mom->deinit);
	  }
	  else
	    zap = 0;
	}
	else
	  su_msg_destroy(arg->clone);

	su_root_destroy(task->sut_root);
      }
    }

    task->sut_port->sup_base->sup_vtable->
      su_port_decref(task->sut_port, zap,
		     "su_pthread_port_clone_main");
  }

#if SU_HAVE_WINSOCK
  su_deinit();
#endif

  if (arg)
    su_pthread_port_return_to_parent(arg, -1);

  return NULL;			/* Exit from thread */
}

/* Signal that parent can resume execution */
static void su_pthread_port_return_to_parent(struct clone_args *arg,
					     int retval)
{
  arg->retval = retval;

  pthread_mutex_lock(arg->mutex);
  pthread_cond_signal(arg->cv);
  pthread_mutex_unlock(arg->mutex);
}

/** "Stop" message function for pthread clone.
 *
 * @sa su_clone_wait()
 * @internal
 */
static void su_pthread_port_clone_break(su_root_magic_t *m,
					su_msg_r msg,
					su_msg_arg_t *a)
{
  su_root_t *root = su_msg_to(msg)->sut_root;

  root->sur_deiniting = 1;

  su_root_break(root);
}

/** Wait for the pthread clone to exit.
 * @internal
 *
 * Called by su_port_wait() and su_clone_wait().
 */
void su_pthread_port_wait(su_clone_r rclone)
{
  su_port_t *clone, *parent;
  struct su_pthread_port_waiting_parent mom[1];
  pthread_t tid;

  assert(*rclone);

  clone = su_msg_to(rclone)->sut_port;
  parent = su_msg_from(rclone)->sut_port;

  if (clone == parent) {
    su_base_port_wait(rclone);
    return;
  }

  assert(parent); assert(clone);
  assert(rclone[0]->sum_func == su_pthread_port_clone_break);
#if 0
  assert(!clone->sup_paused);
#endif

  tid = clone->sup_tid;

  if (!clone->sup_thread) {	/* Already died */
    su_msg_destroy(rclone);
    pthread_join(tid, NULL);
    return;
  }

  pthread_mutex_init(mom->deinit, NULL);
  pthread_mutex_lock(mom->deinit);

  pthread_cond_init(mom->cv, NULL);
  pthread_mutex_init(mom->mutex, NULL);
  pthread_mutex_lock(mom->mutex);

  mom->waiting = 1;

  clone->sup_waiting_parent = mom;

  su_msg_send(rclone);

  while (mom->waiting)
    pthread_cond_wait(mom->cv, mom->mutex);

  /* Run all messages from clone */
  while (su_port_getmsgs_from(parent, clone))
    ;

  /* Allow clone thread to exit */
  pthread_mutex_unlock(mom->deinit);
  pthread_join(tid, NULL);

  pthread_mutex_destroy(mom->deinit);

  pthread_mutex_unlock(mom->mutex);
  pthread_mutex_destroy(mom->mutex);
  pthread_cond_destroy(mom->cv);
}

struct su_pthread_port_execute
{
  pthread_mutex_t mutex[1];
  pthread_cond_t cond[1];
  int (*function)(void *);
  void *arg;
  int value;
};

static su_msg_function _su_pthread_port_execute;

/** Execute the @a function by a pthread @a task.
 *
 * @retval 0 if successful
 * @retval -1 upon an error
 *
 * @sa su_task_execute()
 *
 * @internal
 */
int su_pthread_port_execute(su_task_r const task,
			    int (*function)(void *), void *arg,
			    int *return_value)
{
  int success;
  su_msg_r m = SU_MSG_R_INIT;
#if HAVE_OPEN_C
  struct su_pthread_port_execute frame = {
    { PTHREAD_MUTEX_INITIALIZER },
    { _ENeedsNormalInit, NULL },
    NULL, NULL, 0
  };
  frame.function = function;
  frame.arg = arg;
#else
  struct su_pthread_port_execute frame = {
    { PTHREAD_MUTEX_INITIALIZER },
    { PTHREAD_COND_INITIALIZER },
    function, arg, 0
  };
#endif

  if (su_msg_create(m, task, su_task_null,
		    _su_pthread_port_execute, (sizeof &frame)) < 0)
    return -1;

  *(struct su_pthread_port_execute **)su_msg_data(m) = &frame;

  pthread_mutex_lock(frame.mutex);

  success = su_msg_send(m);

  if (success == 0)
    while (frame.function)
      pthread_cond_wait(frame.cond, frame.mutex);
  else
    su_msg_destroy(m);

  pthread_mutex_unlock(frame.mutex);
  pthread_mutex_destroy(frame.mutex);
  pthread_cond_destroy(frame.cond);

  if (return_value)
    *return_value = frame.value;

  return success;
}

static void _su_pthread_port_execute(su_root_magic_t *m,
				     su_msg_r msg,
				     su_msg_arg_t *a)
{
  struct su_pthread_port_execute *frame;
  frame = *(struct su_pthread_port_execute **)a;
  pthread_mutex_lock(frame->mutex);
  frame->value = frame->function(frame->arg);
  frame->function = NULL;	/* Mark as completed */
  pthread_cond_signal(frame->cond);
  pthread_mutex_unlock(frame->mutex);
}

#if 0				/* pausing and resuming are not used */

/** Pause the pthread port.
 *
 * This is a message function invoked by su_pthread_port_pause() and called
 * from the message dispatcher. It releases the lock sup_runlock and waits
 * until the condition variable sup_resume is signaled and sup_paused is
 * cleared by su_pthread_port_resume().
 */
static
void su_pthread_port_paused(su_root_magic_t *magic,
			    su_msg_r msg,
			    su_msg_arg_t *arg)
{
  su_port_t *self = su_msg_to(msg)->sut_port;
  self->sup_paused = 1;
  while (self->sup_paused)
    pthread_cond_wait(self->sup_resume, self->sup_runlock);
}

/** Pause a port.
 *
 * Obtain an exclusive lock on port's private data.
 *
 * @retval 0 if successful (and clone is paused)
 * @retval -1 upon an error
 */
int su_pthread_port_pause(su_port_t *self)
{
  su_msg_r m = SU_MSG_R_INIT;
  _su_task_t task[1] = {{ self, NULL }};

  if (su_msg_create(m, task, su_task_null, su_pthread_port_paused, 0) < 0)
    return -1;

  if (su_msg_send(m) < 0)
    return -1;

  if (pthread_mutex_lock(self->sup_runlock) < 0)
    return -1;

  return 0;
}

/** Resume a port.
 *
 * Give up an exclusive lock on port's private data.
 *
 * @retval 0 if successful (and clone is resumed)
 * @retval -1 upon an error
 */
int su_pthread_port_resume(su_port_t *self)
{
  assert(self && self->sup_paused);

  self->sup_paused = 0;

  if (pthread_cond_signal(self->sup_resume) < 0 ||
      pthread_mutex_unlock(self->sup_runlock) < 0)
    return -1;

  return 0;
}

#endif
