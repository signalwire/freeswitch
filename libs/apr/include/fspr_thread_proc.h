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

#ifndef APR_THREAD_PROC_H
#define APR_THREAD_PROC_H

/**
 * @file fspr_thread_proc.h
 * @brief APR Thread and Process Library
 */

#include "fspr.h"
#include "fspr_file_io.h"
#include "fspr_pools.h"
#include "fspr_errno.h"

#if APR_HAVE_STRUCT_RLIMIT
#include <sys/time.h>
#include <sys/resource.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/**
 * @defgroup fspr_thread_proc Threads and Process Functions
 * @ingroup APR 
 * @{
 */

typedef enum {
    APR_SHELLCMD,           /**< use the shell to invoke the program */
    APR_PROGRAM,            /**< invoke the program directly, no copied env */
    APR_PROGRAM_ENV,        /**< invoke the program, replicating our environment */
    APR_PROGRAM_PATH,       /**< find program on PATH, use our environment */
    APR_SHELLCMD_ENV        /**< use the shell to invoke the program,
                             *   replicating our environment
                             */
} fspr_cmdtype_e;

typedef enum {
    APR_WAIT,           /**< wait for the specified process to finish */
    APR_NOWAIT          /**< do not wait -- just see if it has finished */
} fspr_wait_how_e;

/* I am specifically calling out the values so that the macros below make
 * more sense.  Yes, I know I don't need to, but I am hoping this makes what
 * I am doing more clear.  If you want to add more reasons to exit, continue
 * to use bitmasks.
 */
typedef enum {
    APR_PROC_EXIT = 1,          /**< process exited normally */
    APR_PROC_SIGNAL = 2,        /**< process exited due to a signal */
    APR_PROC_SIGNAL_CORE = 4    /**< process exited and dumped a core file */
} fspr_exit_why_e;

/** did we exit the process */
#define APR_PROC_CHECK_EXIT(x)        (x & APR_PROC_EXIT)
/** did we get a signal */
#define APR_PROC_CHECK_SIGNALED(x)    (x & APR_PROC_SIGNAL)
/** did we get core */
#define APR_PROC_CHECK_CORE_DUMP(x)   (x & APR_PROC_SIGNAL_CORE)

/** @see fspr_procattr_io_set */
#define APR_NO_PIPE          0

/** @see fspr_procattr_io_set */
#define APR_FULL_BLOCK       1
/** @see fspr_procattr_io_set */
#define APR_FULL_NONBLOCK    2
/** @see fspr_procattr_io_set */
#define APR_PARENT_BLOCK     3
/** @see fspr_procattr_io_set */
#define APR_CHILD_BLOCK      4

/** @see fspr_procattr_limit_set */
#define APR_LIMIT_CPU        0
/** @see fspr_procattr_limit_set */
#define APR_LIMIT_MEM        1
/** @see fspr_procattr_limit_set */
#define APR_LIMIT_NPROC      2
/** @see fspr_procattr_limit_set */
#define APR_LIMIT_NOFILE     3

/**
 * @defgroup APR_OC Other Child Flags
 * @{
 */
#define APR_OC_REASON_DEATH         0     /**< child has died, caller must call
                                           * unregister still */
#define APR_OC_REASON_UNWRITABLE    1     /**< write_fd is unwritable */
#define APR_OC_REASON_RESTART       2     /**< a restart is occuring, perform
                                           * any necessary cleanup (including
                                           * sending a special signal to child)
                                           */
#define APR_OC_REASON_UNREGISTER    3     /**< unregister has been called, do
                                           * whatever is necessary (including
                                           * kill the child) */
#define APR_OC_REASON_LOST          4     /**< somehow the child exited without
                                           * us knowing ... buggy os? */
#define APR_OC_REASON_RUNNING       5     /**< a health check is occuring, 
                                           * for most maintainence functions
                                           * this is a no-op.
                                           */
/** @} */

/** The APR process type */
typedef struct fspr_proc_t {
    /** The process ID */
    pid_t pid;
    /** Parent's side of pipe to child's stdin */
    fspr_file_t *in;
    /** Parent's side of pipe to child's stdout */
    fspr_file_t *out;
    /** Parent's side of pipe to child's stdouterr */
    fspr_file_t *err;
#if APR_HAS_PROC_INVOKED || defined(DOXYGEN)
    /** Diagnositics/debugging string of the command invoked for 
     *  this process [only present if APR_HAS_PROC_INVOKED is true]
     * @remark Only enabled on Win32 by default.
     * @bug This should either always or never be present in release
     * builds - since it breaks binary compatibility.  We may enable
     * it always in APR 1.0 yet leave it undefined in most cases.
     */
    char *invoked;
#endif
#if defined(WIN32) || defined(DOXYGEN)
    /** (Win32 only) Creator's handle granting access to the process
     * @remark This handle is closed and reset to NULL in every case
     * corresponding to a waitpid() on Unix which returns the exit status.
     * Therefore Win32 correspond's to Unix's zombie reaping characteristics
     * and avoids potential handle leaks.
     */
    HANDLE hproc;
#endif
} fspr_proc_t;

/**
 * The prototype for APR child errfn functions.  (See the description
 * of fspr_procattr_child_errfn_set() for more information.)
 * It is passed the following parameters:
 * @param pool Pool associated with the fspr_proc_t.  If your child
 *             error function needs user data, associate it with this
 *             pool.
 * @param err APR error code describing the error
 * @param description Text description of type of processing which failed
 */
typedef void (fspr_child_errfn_t)(fspr_pool_t *proc, fspr_status_t err,
                                 const char *description);

/** Opaque Thread structure. */
typedef struct fspr_thread_t           fspr_thread_t;

/** Opaque Thread attributes structure. */
typedef struct fspr_threadattr_t       fspr_threadattr_t;

/** Opaque Process attributes structure. */
typedef struct fspr_procattr_t         fspr_procattr_t;

/** Opaque control variable for one-time atomic variables.  */
typedef struct fspr_thread_once_t      fspr_thread_once_t;

/** Opaque thread private address space. */
typedef struct fspr_threadkey_t        fspr_threadkey_t;

/** Opaque record of child process. */
typedef struct fspr_other_child_rec_t  fspr_other_child_rec_t;

/**
 * The prototype for any APR thread worker functions.
 */
typedef void *(APR_THREAD_FUNC *fspr_thread_start_t)(fspr_thread_t*, void*);

typedef enum {
    APR_KILL_NEVER,             /**< process is never sent any signals */
    APR_KILL_ALWAYS,            /**< process is sent SIGKILL on fspr_pool_t cleanup */
    APR_KILL_AFTER_TIMEOUT,     /**< SIGTERM, wait 3 seconds, SIGKILL */
    APR_JUST_WAIT,              /**< wait forever for the process to complete */
    APR_KILL_ONLY_ONCE          /**< send SIGTERM and then wait */
} fspr_kill_conditions_e;

/* Thread Function definitions */

#if APR_HAS_THREADS

/**
 * Create and initialize a new threadattr variable
 * @param new_attr The newly created threadattr.
 * @param cont The pool to use
 */
APR_DECLARE(fspr_status_t) fspr_threadattr_create(fspr_threadattr_t **new_attr, 
                                                fspr_pool_t *cont);

/**
 * Set if newly created threads should be created in detached state.
 * @param attr The threadattr to affect 
 * @param on Non-zero if detached threads should be created.
 */
APR_DECLARE(fspr_status_t) fspr_threadattr_detach_set(fspr_threadattr_t *attr, 
                                                    fspr_int32_t on);

/**
 * Get the detach state for this threadattr.
 * @param attr The threadattr to reference
 * @return APR_DETACH if threads are to be detached, or APR_NOTDETACH
 * if threads are to be joinable. 
 */
APR_DECLARE(fspr_status_t) fspr_threadattr_detach_get(fspr_threadattr_t *attr);

/**
 * Set the stack size of newly created threads.
 * @param attr The threadattr to affect 
 * @param stacksize The stack size in bytes
 */
APR_DECLARE(fspr_status_t) fspr_threadattr_stacksize_set(fspr_threadattr_t *attr,
                                                       fspr_size_t stacksize);

/**
 * Set the stack guard area size of newly created threads.
 * @param attr The threadattr to affect 
 * @param guardsize The stack guard area size in bytes
 * @note Thread library implementations commonly use a "guard area"
 * after each thread's stack which is not readable or writable such that
 * stack overflows cause a segfault; this consumes e.g. 4K of memory
 * and increases memory management overhead.  Setting the guard area
 * size to zero hence trades off reliable behaviour on stack overflow
 * for performance. */
APR_DECLARE(fspr_status_t) fspr_threadattr_guardsize_set(fspr_threadattr_t *attr,
                                                       fspr_size_t guardsize);

/**
 * Create a new thread of execution
 * @param new_thread The newly created thread handle.
 * @param attr The threadattr to use to determine how to create the thread
 * @param func The function to start the new thread in
 * @param data Any data to be passed to the starting function
 * @param cont The pool to use
 */
APR_DECLARE(fspr_status_t) fspr_thread_create(fspr_thread_t **new_thread, 
                                            fspr_threadattr_t *attr, 
                                            fspr_thread_start_t func, 
                                            void *data, fspr_pool_t *cont);

/**
 * stop the current thread
 * @param thd The thread to stop
 * @param retval The return value to pass back to any thread that cares
 */
APR_DECLARE(fspr_status_t) fspr_thread_exit(fspr_thread_t *thd, 
                                          fspr_status_t retval);

/**
 * block until the desired thread stops executing.
 * @param retval The return value from the dead thread.
 * @param thd The thread to join
 */
APR_DECLARE(fspr_status_t) fspr_thread_join(fspr_status_t *retval, 
                                          fspr_thread_t *thd); 

/**
 * force the current thread to yield the processor
 */
APR_DECLARE(void) fspr_thread_yield(void);

/**
 * Initialize the control variable for fspr_thread_once.  If this isn't
 * called, fspr_initialize won't work.
 * @param control The control variable to initialize
 * @param p The pool to allocate data from.
 */
APR_DECLARE(fspr_status_t) fspr_thread_once_init(fspr_thread_once_t **control,
                                               fspr_pool_t *p);

/**
 * Run the specified function one time, regardless of how many threads
 * call it.
 * @param control The control variable.  The same variable should
 *                be passed in each time the function is tried to be
 *                called.  This is how the underlying functions determine
 *                if the function has ever been called before.
 * @param func The function to call.
 */
APR_DECLARE(fspr_status_t) fspr_thread_once(fspr_thread_once_t *control,
                                          void (*func)(void));

/**
 * detach a thread
 * @param thd The thread to detach 
 */
APR_DECLARE(fspr_status_t) fspr_thread_detach(fspr_thread_t *thd);

/**
 * Return the pool associated with the current thread.
 * @param data The user data associated with the thread.
 * @param key The key to associate with the data
 * @param thread The currently open thread.
 */
APR_DECLARE(fspr_status_t) fspr_thread_data_get(void **data, const char *key,
                                             fspr_thread_t *thread);

/**
 * Return the pool associated with the current thread.
 * @param data The user data to associate with the thread.
 * @param key The key to use for associating the data with the thread
 * @param cleanup The cleanup routine to use when the thread is destroyed.
 * @param thread The currently open thread.
 */
APR_DECLARE(fspr_status_t) fspr_thread_data_set(void *data, const char *key,
                                             fspr_status_t (*cleanup) (void *),
                                             fspr_thread_t *thread);

/**
 * Create and initialize a new thread private address space
 * @param key The thread private handle.
 * @param dest The destructor to use when freeing the private memory.
 * @param cont The pool to use
 */
APR_DECLARE(fspr_status_t) fspr_threadkey_private_create(fspr_threadkey_t **key, 
                                                    void (*dest)(void *),
                                                    fspr_pool_t *cont);

/**
 * Get a pointer to the thread private memory
 * @param new_mem The data stored in private memory 
 * @param key The handle for the desired thread private memory 
 */
APR_DECLARE(fspr_status_t) fspr_threadkey_private_get(void **new_mem, 
                                                 fspr_threadkey_t *key);

/**
 * Set the data to be stored in thread private memory
 * @param priv The data to be stored in private memory 
 * @param key The handle for the desired thread private memory 
 */
APR_DECLARE(fspr_status_t) fspr_threadkey_private_set(void *priv, 
                                                 fspr_threadkey_t *key);

/**
 * Free the thread private memory
 * @param key The handle for the desired thread private memory 
 */
APR_DECLARE(fspr_status_t) fspr_threadkey_private_delete(fspr_threadkey_t *key);

/**
 * Return the pool associated with the current threadkey.
 * @param data The user data associated with the threadkey.
 * @param key The key associated with the data
 * @param threadkey The currently open threadkey.
 */
APR_DECLARE(fspr_status_t) fspr_threadkey_data_get(void **data, const char *key,
                                                fspr_threadkey_t *threadkey);

/**
 * Return the pool associated with the current threadkey.
 * @param data The data to set.
 * @param key The key to associate with the data.
 * @param cleanup The cleanup routine to use when the file is destroyed.
 * @param threadkey The currently open threadkey.
 */
APR_DECLARE(fspr_status_t) fspr_threadkey_data_set(void *data, const char *key,
                                                fspr_status_t (*cleanup) (void *),
                                                fspr_threadkey_t *threadkey);

#endif

/**
 * Create and initialize a new procattr variable
 * @param new_attr The newly created procattr. 
 * @param cont The pool to use
 */
APR_DECLARE(fspr_status_t) fspr_procattr_create(fspr_procattr_t **new_attr,
                                                  fspr_pool_t *cont);

/**
 * Determine if any of stdin, stdout, or stderr should be linked to pipes 
 * when starting a child process.
 * @param attr The procattr we care about. 
 * @param in Should stdin be a pipe back to the parent?
 * @param out Should stdout be a pipe back to the parent?
 * @param err Should stderr be a pipe back to the parent?
 */
APR_DECLARE(fspr_status_t) fspr_procattr_io_set(fspr_procattr_t *attr, 
                                             fspr_int32_t in, fspr_int32_t out,
                                             fspr_int32_t err);

/**
 * Set the child_in and/or parent_in values to existing fspr_file_t values.
 * @param attr The procattr we care about. 
 * @param child_in fspr_file_t value to use as child_in. Must be a valid file.
 * @param parent_in fspr_file_t value to use as parent_in. Must be a valid file.
 * @remark  This is NOT a required initializer function. This is
 *          useful if you have already opened a pipe (or multiple files)
 *          that you wish to use, perhaps persistently across multiple
 *          process invocations - such as a log file. You can save some 
 *          extra function calls by not creating your own pipe since this
 *          creates one in the process space for you.
 */
APR_DECLARE(fspr_status_t) fspr_procattr_child_in_set(struct fspr_procattr_t *attr,
                                                  fspr_file_t *child_in,
                                                  fspr_file_t *parent_in);

/**
 * Set the child_out and parent_out values to existing fspr_file_t values.
 * @param attr The procattr we care about. 
 * @param child_out fspr_file_t value to use as child_out. Must be a valid file.
 * @param parent_out fspr_file_t value to use as parent_out. Must be a valid file.
 * @remark This is NOT a required initializer function. This is
 *         useful if you have already opened a pipe (or multiple files)
 *         that you wish to use, perhaps persistently across multiple
 *         process invocations - such as a log file. 
 */
APR_DECLARE(fspr_status_t) fspr_procattr_child_out_set(struct fspr_procattr_t *attr,
                                                   fspr_file_t *child_out,
                                                   fspr_file_t *parent_out);

/**
 * Set the child_err and parent_err values to existing fspr_file_t values.
 * @param attr The procattr we care about. 
 * @param child_err fspr_file_t value to use as child_err. Must be a valid file.
 * @param parent_err fspr_file_t value to use as parent_err. Must be a valid file.
 * @remark This is NOT a required initializer function. This is
 *         useful if you have already opened a pipe (or multiple files)
 *         that you wish to use, perhaps persistently across multiple
 *         process invocations - such as a log file. 
 */
APR_DECLARE(fspr_status_t) fspr_procattr_child_err_set(struct fspr_procattr_t *attr,
                                                   fspr_file_t *child_err,
                                                   fspr_file_t *parent_err);

/**
 * Set which directory the child process should start executing in.
 * @param attr The procattr we care about. 
 * @param dir Which dir to start in.  By default, this is the same dir as
 *            the parent currently resides in, when the createprocess call
 *            is made. 
 */
APR_DECLARE(fspr_status_t) fspr_procattr_dir_set(fspr_procattr_t *attr, 
                                              const char *dir);

/**
 * Set what type of command the child process will call.
 * @param attr The procattr we care about. 
 * @param cmd The type of command.  One of:
 * <PRE>
 *            APR_SHELLCMD     --  Anything that the shell can handle
 *            APR_PROGRAM      --  Executable program   (default) 
 *            APR_PROGRAM_ENV  --  Executable program, copy environment
 *            APR_PROGRAM_PATH --  Executable program on PATH, copy env
 * </PRE>
 */
APR_DECLARE(fspr_status_t) fspr_procattr_cmdtype_set(fspr_procattr_t *attr,
                                                  fspr_cmdtype_e cmd);

/**
 * Determine if the child should start in detached state.
 * @param attr The procattr we care about. 
 * @param detach Should the child start in detached state?  Default is no. 
 */
APR_DECLARE(fspr_status_t) fspr_procattr_detach_set(fspr_procattr_t *attr, 
                                                 fspr_int32_t detach);

#if APR_HAVE_STRUCT_RLIMIT
/**
 * Set the Resource Utilization limits when starting a new process.
 * @param attr The procattr we care about. 
 * @param what Which limit to set, one of:
 * <PRE>
 *                 APR_LIMIT_CPU
 *                 APR_LIMIT_MEM
 *                 APR_LIMIT_NPROC
 *                 APR_LIMIT_NOFILE
 * </PRE>
 * @param limit Value to set the limit to.
 */
APR_DECLARE(fspr_status_t) fspr_procattr_limit_set(fspr_procattr_t *attr, 
                                                fspr_int32_t what,
                                                struct rlimit *limit);
#endif

/**
 * Specify an error function to be called in the child process if APR
 * encounters an error in the child prior to running the specified program.
 * @param attr The procattr describing the child process to be created.
 * @param errfn The function to call in the child process.
 * @remark At the present time, it will only be called from fspr_proc_create()
 *         on platforms where fork() is used.  It will never be called on other
 *         platforms, on those platforms fspr_proc_create() will return the error
 *         in the parent process rather than invoke the callback in the now-forked
 *         child process.
 */
APR_DECLARE(fspr_status_t) fspr_procattr_child_errfn_set(fspr_procattr_t *attr,
                                                       fspr_child_errfn_t *errfn);

/**
 * Specify that fspr_proc_create() should do whatever it can to report
 * failures to the caller of fspr_proc_create(), rather than find out in
 * the child.
 * @param attr The procattr describing the child process to be created.
 * @param chk Flag to indicate whether or not extra work should be done
 *            to try to report failures to the caller.
 * @remark This flag only affects fspr_proc_create() on platforms where
 *         fork() is used.  This leads to extra overhead in the calling
 *         process, but that may help the application handle such
 *         errors more gracefully.
 */
APR_DECLARE(fspr_status_t) fspr_procattr_error_check_set(fspr_procattr_t *attr,
                                                       fspr_int32_t chk);

/**
 * Determine if the child should start in its own address space or using the 
 * current one from its parent
 * @param attr The procattr we care about. 
 * @param addrspace Should the child start in its own address space?  Default
 *                  is no on NetWare and yes on other platforms.
 */
APR_DECLARE(fspr_status_t) fspr_procattr_addrspace_set(fspr_procattr_t *attr,
                                                       fspr_int32_t addrspace);

/**
 * Set the username used for running process
 * @param attr The procattr we care about. 
 * @param username The username used
 * @param password User password if needed. Password is needed on WIN32
 *                 or any other platform having
 *                 APR_PROCATTR_USER_SET_REQUIRES_PASSWORD set.
 */
APR_DECLARE(fspr_status_t) fspr_procattr_user_set(fspr_procattr_t *attr,
                                                const char *username,
                                                const char *password);

/**
 * Set the group used for running process
 * @param attr The procattr we care about. 
 * @param groupname The group name  used
 */
APR_DECLARE(fspr_status_t) fspr_procattr_group_set(fspr_procattr_t *attr,
                                                 const char *groupname);


#if APR_HAS_FORK
/**
 * This is currently the only non-portable call in APR.  This executes 
 * a standard unix fork.
 * @param proc The resulting process handle. 
 * @param cont The pool to use. 
 * @remark returns APR_INCHILD for the child, and APR_INPARENT for the parent
 * or an error.
 */
APR_DECLARE(fspr_status_t) fspr_proc_fork(fspr_proc_t *proc, fspr_pool_t *cont);
#endif

/**
 * Create a new process and execute a new program within that process.
 * @param new_proc The resulting process handle.
 * @param progname The program to run 
 * @param args the arguments to pass to the new program.  The first 
 *             one should be the program name.
 * @param env The new environment table for the new process.  This 
 *            should be a list of NULL-terminated strings. This argument
 *            is ignored for APR_PROGRAM_ENV, APR_PROGRAM_PATH, and
 *            APR_SHELLCMD_ENV types of commands.
 * @param attr the procattr we should use to determine how to create the new
 *         process
 * @param pool The pool to use.
 * @note This function returns without waiting for the new process to terminate;
 * use fspr_proc_wait for that.
 */
APR_DECLARE(fspr_status_t) fspr_proc_create(fspr_proc_t *new_proc,
                                          const char *progname,
                                          const char * const *args,
                                          const char * const *env, 
                                          fspr_procattr_t *attr, 
                                          fspr_pool_t *pool);

/**
 * Wait for a child process to die
 * @param proc The process handle that corresponds to the desired child process 
 * @param exitcode The returned exit status of the child, if a child process 
 *                 dies, or the signal that caused the child to die.
 *                 On platforms that don't support obtaining this information, 
 *                 the status parameter will be returned as APR_ENOTIMPL.
 * @param exitwhy Why the child died, the bitwise or of:
 * <PRE>
 *            APR_PROC_EXIT         -- process terminated normally
 *            APR_PROC_SIGNAL       -- process was killed by a signal
 *            APR_PROC_SIGNAL_CORE  -- process was killed by a signal, and
 *                                     generated a core dump.
 * </PRE>
 * @param waithow How should we wait.  One of:
 * <PRE>
 *            APR_WAIT   -- block until the child process dies.
 *            APR_NOWAIT -- return immediately regardless of if the 
 *                          child is dead or not.
 * </PRE>
 * @remark The childs status is in the return code to this process.  It is one of:
 * <PRE>
 *            APR_CHILD_DONE     -- child is no longer running.
 *            APR_CHILD_NOTDONE  -- child is still running.
 * </PRE>
 */
APR_DECLARE(fspr_status_t) fspr_proc_wait(fspr_proc_t *proc,
                                        int *exitcode, fspr_exit_why_e *exitwhy,
                                        fspr_wait_how_e waithow);

/**
 * Wait for any current child process to die and return information 
 * about that child.
 * @param proc Pointer to NULL on entry, will be filled out with child's 
 *             information 
 * @param exitcode The returned exit status of the child, if a child process 
 *                 dies, or the signal that caused the child to die.
 *                 On platforms that don't support obtaining this information, 
 *                 the status parameter will be returned as APR_ENOTIMPL.
 * @param exitwhy Why the child died, the bitwise or of:
 * <PRE>
 *            APR_PROC_EXIT         -- process terminated normally
 *            APR_PROC_SIGNAL       -- process was killed by a signal
 *            APR_PROC_SIGNAL_CORE  -- process was killed by a signal, and
 *                                     generated a core dump.
 * </PRE>
 * @param waithow How should we wait.  One of:
 * <PRE>
 *            APR_WAIT   -- block until the child process dies.
 *            APR_NOWAIT -- return immediately regardless of if the 
 *                          child is dead or not.
 * </PRE>
 * @param p Pool to allocate child information out of.
 * @bug Passing proc as a *proc rather than **proc was an odd choice
 * for some platforms... this should be revisited in 1.0
 */
APR_DECLARE(fspr_status_t) fspr_proc_wait_all_procs(fspr_proc_t *proc,
                                                  int *exitcode,
                                                  fspr_exit_why_e *exitwhy,
                                                  fspr_wait_how_e waithow,
                                                  fspr_pool_t *p);

#define APR_PROC_DETACH_FOREGROUND 0    /**< Do not detach */
#define APR_PROC_DETACH_DAEMONIZE 1     /**< Detach */

/**
 * Detach the process from the controlling terminal.
 * @param daemonize set to non-zero if the process should daemonize
 *                  and become a background process, else it will
 *                  stay in the foreground.
 */
APR_DECLARE(fspr_status_t) fspr_proc_detach(int daemonize);

/**
 * Register an other_child -- a child associated to its registered 
 * maintence callback.  This callback is invoked when the process
 * dies, is disconnected or disappears.
 * @param proc The child process to register.
 * @param maintenance maintenance is a function that is invoked with a 
 *                    reason and the data pointer passed here.
 * @param data Opaque context data passed to the maintenance function.
 * @param write_fd An fd that is probed for writing.  If it is ever unwritable
 *                 then the maintenance is invoked with reason 
 *                 OC_REASON_UNWRITABLE.
 * @param p The pool to use for allocating memory.
 * @bug write_fd duplicates the proc->out stream, it's really redundant
 * and should be replaced in the APR 1.0 API with a bitflag of which
 * proc->in/out/err handles should be health checked.
 * @bug no platform currently tests the pipes health.
 */
APR_DECLARE(void) fspr_proc_other_child_register(fspr_proc_t *proc, 
                                           void (*maintenance) (int reason, 
                                                                void *, 
                                                                int status),
                                           void *data, fspr_file_t *write_fd,
                                           fspr_pool_t *p);

/**
 * Stop watching the specified other child.  
 * @param data The data to pass to the maintenance function.  This is
 *             used to find the process to unregister.
 * @warning Since this can be called by a maintenance function while we're
 *          scanning the other_children list, all scanners should protect 
 *          themself by loading ocr->next before calling any maintenance 
 *          function.
 */
APR_DECLARE(void) fspr_proc_other_child_unregister(void *data);

/**
 * Notify the maintenance callback of a registered other child process
 * that application has detected an event, such as death.
 * @param proc The process to check
 * @param reason The reason code to pass to the maintenance function
 * @param status The status to pass to the maintenance function
 * @remark An example of code using this behavior;
 * <pre>
 * rv = fspr_proc_wait_all_procs(&proc, &exitcode, &status, APR_WAIT, p);
 * if (APR_STATUS_IS_CHILD_DONE(rv)) {
 * #if APR_HAS_OTHER_CHILD
 *     if (fspr_proc_other_child_alert(&proc, APR_OC_REASON_DEATH, status)
 *             == APR_SUCCESS) {
 *         ;  (already handled)
 *     }
 *     else
 * #endif
 *         [... handling non-otherchild processes death ...]
 * </pre>
 */
APR_DECLARE(fspr_status_t) fspr_proc_other_child_alert(fspr_proc_t *proc, 
                                                     int reason,
                                                     int status);

/**
 * Test one specific other child processes and invoke the maintenance callback 
 * with the appropriate reason code, if still running, or the appropriate reason 
 * code if the process is no longer healthy.
 * @param ocr The registered other child
 * @param reason The reason code (e.g. APR_OC_REASON_RESTART) if still running
 */
APR_DECLARE(void) fspr_proc_other_child_refresh(fspr_other_child_rec_t *ocr,
                                               int reason);

/**
 * Test all registered other child processes and invoke the maintenance callback 
 * with the appropriate reason code, if still running, or the appropriate reason 
 * code if the process is no longer healthy.
 * @param reason The reason code (e.g. APR_OC_REASON_RESTART) to running processes
 */
APR_DECLARE(void) fspr_proc_other_child_refresh_all(int reason);

/** 
 * Terminate a process.
 * @param proc The process to terminate.
 * @param sig How to kill the process.
 */
APR_DECLARE(fspr_status_t) fspr_proc_kill(fspr_proc_t *proc, int sig);

/**
 * Register a process to be killed when a pool dies.
 * @param a The pool to use to define the processes lifetime 
 * @param proc The process to register
 * @param how How to kill the process, one of:
 * <PRE>
 *         APR_KILL_NEVER         -- process is never sent any signals
 *         APR_KILL_ALWAYS        -- process is sent SIGKILL on fspr_pool_t cleanup
 *         APR_KILL_AFTER_TIMEOUT -- SIGTERM, wait 3 seconds, SIGKILL
 *         APR_JUST_WAIT          -- wait forever for the process to complete
 *         APR_KILL_ONLY_ONCE     -- send SIGTERM and then wait
 * </PRE>
 */
APR_DECLARE(void) fspr_pool_note_subprocess(fspr_pool_t *a, fspr_proc_t *proc,
                                           fspr_kill_conditions_e how);

#if APR_HAS_THREADS 

#if (APR_HAVE_SIGWAIT || APR_HAVE_SIGSUSPEND) && !defined(OS2)

/**
 * Setup the process for a single thread to be used for all signal handling.
 * @warning This must be called before any threads are created
 */
APR_DECLARE(fspr_status_t) fspr_setup_signal_thread(void);

/**
 * Make the current thread listen for signals.  This thread will loop
 * forever, calling a provided function whenever it receives a signal.  That
 * functions should return 1 if the signal has been handled, 0 otherwise.
 * @param signal_handler The function to call when a signal is received
 * fspr_status_t fspr_signal_thread((int)(*signal_handler)(int signum))
 */
APR_DECLARE(fspr_status_t) fspr_signal_thread(int(*signal_handler)(int signum));

#endif /* (APR_HAVE_SIGWAIT || APR_HAVE_SIGSUSPEND) && !defined(OS2) */

/**
 * Get the child-pool used by the thread from the thread info.
 * @return fspr_pool_t the pool
 */
APR_POOL_DECLARE_ACCESSOR(thread);

#endif /* APR_HAS_THREADS */

/** @} */

#ifdef __cplusplus
}
#endif

#endif  /* ! APR_THREAD_PROC_H */

