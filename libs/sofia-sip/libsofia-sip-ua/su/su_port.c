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
 * @CFILE su_port.c
 *
 * OS-Independent Socket Syncronization Interface.
 *
 * This looks like nth reincarnation of "reactor". It implements the
 * poll/select/WaitForMultipleObjects and message passing functionality.
 * This is virtual implementation:
 *
 * @author Pekka Pessi <Pekka.Pessi@nokia.com>
 * @author Kai Vehmanen <kai.vehmanen@nokia.com>
 *
 * @date Created: Tue Sep 14 15:51:04 1999 ppessi
 */

#include "config.h"

#define SU_CLONE_T      su_msg_t

#define su_port_s su_virtual_port_s

#include "su_port.h"
#include <sofia-sip/su_string.h>

#include <stdlib.h>

/** Create the default su_port_t implementation. */
su_port_t *su_default_port_create(void)
{
#if HAVE_EPOLL
  return su_epoll_port_create();
#elif HAVE_KQUEUE
  return su_kqueue_port_create();
#elif HAVE_SYS_DEVPOLL_H
  return su_devpoll_port_create();
#elif HAVE_POLL_PORT
  return su_poll_port_create();
#elif HAVE_WIN32
  return su_wsaevent_port_create();
#elif HAVE_SELECT
  return su_select_port_create();
#else
  return NULL;
#endif
}

int su_default_clone_start(su_root_t *parent,
			   su_clone_r return_clone,
			   su_root_magic_t *magic,
			   su_root_init_f init,
			   su_root_deinit_f deinit)
{
#if HAVE_EPOLL
  return su_epoll_clone_start(parent, return_clone, magic, init, deinit);
#elif HAVE_KQUEUE
  return su_kqueue_clone_start(parent, return_clone, magic, init, deinit);
#elif HAVE_SYS_DEVPOLL_H
  return su_devpoll_clone_start(parent, return_clone, magic, init, deinit);
#elif HAVE_POLL_PORT
  return su_poll_clone_start(parent, return_clone, magic, init, deinit);
#elif HAVE_WIN32
  return su_wsaevent_clone_start(parent, return_clone, magic, init, deinit);
#elif HAVE_SELECT
  return su_select_clone_start(parent, return_clone, magic, init, deinit);
#else
  errno = ENOSYS;
  return -1;
#endif
}

static su_port_create_f *preferred_su_port_create;
static su_clone_start_f *preferred_su_clone_start;

/** Explicitly set the preferred su_port_t implementation.
 *
 * @sa su_epoll_port_create(), su_poll_port_create(), su_select_port_create()
 */
void su_port_prefer(su_port_create_f *create,
		    su_clone_start_f *start)
{
  if (create) preferred_su_port_create = create;
  if (start) preferred_su_clone_start = start;
}

static
void su_port_set_system_preferences(char const *name)
{
  su_port_create_f *create = preferred_su_port_create;
  su_clone_start_f *start = preferred_su_clone_start;

  if (name == NULL)
      ;
#if HAVE_EPOLL
  else if (su_casematch(name, "epoll")) {
    create = su_epoll_port_create;
    start = su_epoll_clone_start;
  }
#endif
#if HAVE_KQUEUE
  else if (su_casematch(name, "kqueue")) {
    create = su_kqueue_port_create;
    start = su_kqueue_clone_start;
  }
#endif
#if HAVE_SYS_DEVPOLL_H
  else if (su_casematch(name, "devpoll")) {
    create = su_devpoll_port_create;
    start = su_devpoll_clone_start;
  }
#endif
#if HAVE_POLL_PORT
  else if (su_casematch(name, "poll")) {
    create = su_poll_port_create;
    start = su_poll_clone_start;
  }
#endif
#if HAVE_WIN32
  else if (su_casematch(name, "wsaevent")) {
    create = su_wsaevent_port_create;
    start = su_wsaevent_clone_start;
  }
#elif HAVE_SELECT
  else if (su_casematch(name, "select")) {
    create = su_select_port_create;
    start = su_select_clone_start;
  }
#endif

  if (create == NULL)
    create = su_default_port_create;

  if (!preferred_su_port_create ||
      preferred_su_port_create == su_default_port_create)
    preferred_su_port_create = create;

  if (start == NULL)
    start = su_default_clone_start;

  if (!preferred_su_clone_start ||
      preferred_su_clone_start == su_default_clone_start)
    preferred_su_clone_start = start;
}

/** Create the preferred su_port_t implementation. */
su_port_t *su_port_create(void)
{
  if (preferred_su_port_create == NULL)
    su_port_set_system_preferences(getenv("SU_PORT"));

  return preferred_su_port_create();
}

/** Return name of the su_port_t instance. */
char const *su_port_name(su_port_t const *port)
{
  return port->sup_vtable->su_port_name(port);
}

/* ========================================================================
 * su_clone_t
 */

/**@ingroup su_wait
 *
 * @page su_clone_t Clone Objects
 *
 * The process may be divided into many tasks via cloning. Several tasks may
 * run in context of one thread, or each task may be run by its own thread.
 * However, only a single thread can execute code within a task. There can
 * be a 1-to-N mapping from thread to tasks. Thus, software using tasks can
 * be executed by multiple threads in a multithreaded environment and by a
 * single thread in a singlethreaded environment.
 *
 * The clones are useful for handling tasks that can be executed by a
 * separate threads, but which do not block excessively. When threads are
 * not available or they are not needed, clones can also be run in a
 * single-threaded mode. Running in single-threaded mode is especially
 * useful while debugging.
 *
 * A clone task is created with function su_clone_start(). Each clone has
 * its own root object (su_root_t), which holds a context pointer
 * (su_root_magic_t *). The context object can be different from that of
 * parent task.
 *
 * When a clone is started, the clone initialization function is called. The
 * initialization function should do whatever initialization there is to be
 * performed, register I/O events and timers, and then return. If the
 * initialization is successful, the clone task reverts to run the event
 * loop and invoking the event callbacks until its parent stops it by
 * calling su_clone_wait() which invokes the deinit function. The clone task
 * is destroyed when the deinit function returns.
 *
 * The public API consists of following functions:
 *    - su_clone_start()
 *    - su_clone_task()
 *    - su_clone_wait()
 *    - su_clone_forget()
 *
 * @note
 * There is only one event loop for each thread which can be shared by
 * multiple clone tasks. Therefore, the clone tasks can not explicitly run
 * or step the event loop, but they are limited to event callbacks. A clone
 * task may not call su_root_break(), su_root_run() or su_root_step().
 */

static int su_root_init_nothing(su_root_t *root, su_root_magic_t *magic)
{
  return 0;
}

static void su_root_deinit_nothing(su_root_t *root, su_root_magic_t *magic)
{
}

/** Start a clone task.
 *
 * Allocate and initialize a sub-task. Depending on the su_root_threading()
 * settings, a separate thread may be created to execute the sub-task. The
 * sub-task is represented by clone handle to the rest of the application.
 * The function su_clone_start() returns the clone handle in @a
 * return_clone. The clone handle is used to communicate with the newly
 * created clone task using messages.
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
 * In multi-threaded implementation, su_clone_start() launches a new thread,
 * and the initialization routine is executed by this newly created thread.
 * The calling thread blocks until the initialization routine completes. If
 * the initialization routine returns #su_success (0), the sub-task is
 * considered to be created successfully. After the successful
 * initialization, the sub-task continues to execeute the function
 * su_root_run().
 *
 * In single-threaded implementations, just a new root object is created.
 * The initialization routine is called directly from su_clone_start().
 *
 * If the initalization function @a init fails, the sub-task (either the
 * newly created thread or the current thread executing the su_clone_start()
 * function) calls the deinitialization function, and su_clone_start()
 * returns NULL.
 *
 * @param parent   root to be cloned
 * @param return_clone reference to a clone [OUT]
 * @param magic    pointer to user data
 * @param init     initialization function
 * @param deinit   deinitialization function
 *
 * @return 0 if successfull, -1 upon an error.
 *
 * @note Earlier documentation mentioned that @a parent could be NULL. That
 * feature has never been implemented, however.
 *
 * @sa su_root_threading(), su_clone_task(), su_clone_stop(), su_clone_wait(),
 * su_clone_forget().
 */
int su_clone_start(su_root_t *parent,
		   su_clone_r return_clone,
		   su_root_magic_t *magic,
		   su_root_init_f init,
		   su_root_deinit_f deinit)
{
  su_port_vtable_t const *svp;

  if (init == NULL)
    init = su_root_init_nothing;
  if (deinit == NULL)
    deinit = su_root_deinit_nothing;


  if (parent == NULL || parent->sur_threading) {
    if (preferred_su_clone_start == NULL)
      su_port_set_system_preferences(getenv("SU_PORT"));
    return preferred_su_clone_start(parent, return_clone, magic, init, deinit);
  }

  svp = parent->sur_task->sut_port->sup_vtable;

  if (svp->su_port_start_shared == NULL)
    return su_seterrno(EINVAL);

  /* Return a task sharing the same port. */
  return svp->su_port_start_shared(parent, return_clone, magic, init, deinit);
}

/** Get reference to a clone task.
 *
 * @param clone Clone pointer
 *
 * @return A reference to the task structure of the clone.
 */
_su_task_r su_clone_task(su_clone_r clone)
{
  return su_msg_to(clone);
}

/**Forget the clone.
 *
 * Normally, the clone task executes until it is stopped.  If the parent
 * task does not need to stop the task, it can "forget" the clone.  The
 * clone exits independently of the parent task.
 *
 * @param rclone Reference to the clone.
 */
void su_clone_forget(su_clone_r rclone)
{
  su_msg_destroy(rclone);
}

/** Stop the clone.
 *
 * This can used only if clone task has sent no report messages (messages
 * with delivery report sent back to clone).
 *
 * @deprecated. Use su_clone_wait().
 */
void su_clone_stop(su_clone_r rclone)
{
  su_msg_send(rclone);
}

/** Stop a clone and wait until it is has completed.
 *
 * The function su_clone_wait() is used to stop the clone task and wait
 * until it has cleaned up. The clone task is destroyed asynchronously. The
 * parent sends a message to clone, clone deinitializes itself and then
 * replies. After the reply message is received by the parent, it will send
 * a third message back to clone.
 *
 * The parent destroy all messages to or from clone task before calling
 * su_clone_wait(). The parent task may not send any messages to the clone
 * after calling su_clone_wait(). The su_clone_wait() function blocks until
 * the cloned task is destroyed. During that time, the parent task must be
 * prepared to process all the messages sent by clone task. This includes
 * all the messages sent by clone before destroy the message reached the
 * clone.
 */
void su_clone_wait(su_root_t *root, su_clone_r rclone)
{
  if (rclone[0]) {
    assert(root == NULL || root == su_msg_from(rclone)->sut_root);
    su_port_wait(rclone);
  }
}

/** Pause a clone.
 *
 * Obtain an exclusive lock on clone's private data.
 *
 * @retval 0 if successful (and clone is paused)
 * @retval -1 upon an error
 *
 * @deprecated Never implemented.
 */
int su_clone_pause(su_clone_r rclone)
{
#if 0
  su_root_t *cloneroot = su_task_root(su_msg_to(rclone));

  if (!cloneroot)
    return (errno = EFAULT), -1;

  if (SU_ROOT_OWN_THREAD(cloneroot))
    /* We own it already */
    return 0;

  return su_port_pause(cloneroot->sur_port);
#else
  return errno = ENOSYS, -1;
#endif
}

/** Resume a clone.
 *
 * Give up an exclusive lock on clone's private data.
 *
 * @retval 0 if successful (and clone is resumed)
 * @retval -1 upon an error
 *
 * @deprecated Never implemented.
 */
int su_clone_resume(su_clone_r rclone)
{
#if 0
  su_root_t *cloneroot = su_task_root(su_msg_to(rclone));

  if (!cloneroot)
    return (errno = EFAULT), -1;

  if (SU_ROOT_OWN_THREAD(cloneroot))
    /* We cannot give it away */
    return 0;

  return su_port_resume(cloneroot->sur_port);
#else
  return errno = ENOSYS, -1;
#endif
}

/** Wait for clone to exit.
 *
 * @internal
 *
 * Called by su_clone_wait().
 */
void su_port_wait(su_clone_r rclone)
{
  su_port_t *cloneport;

  assert(*rclone);

  cloneport = su_msg_to(rclone)->sut_port;
  cloneport->sup_vtable->su_port_wait(rclone);
}

int su_port_execute(su_task_r const task,
		    int (*function)(void *), void *arg,
		    int *return_value)
{
  if (!task->sut_port->sup_vtable->su_port_execute)
    return errno = ENOSYS, -1;

  return task->sut_port->sup_vtable->
    su_port_execute(task, function, arg, return_value);
}

#if notyet && nomore
int su_port_pause(su_port_t *self)
{
  assert(self->sup_vtable->su_port_pause);
  return self->sup_vtable->su_port_pause(self);
}

int su_port_resume(su_port_t *self)
{
  assert(self->sup_vtable->su_port_resume);
  return self->sup_vtable->su_port_resume(self);
}
#endif
