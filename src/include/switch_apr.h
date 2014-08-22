/* 
 * FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
 * Copyright (C) 2005-2014, Anthony Minessale II <anthm@freeswitch.org>
 *
 * Version: MPL 1.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
 *
 * The Initial Developer of the Original Code is
 * Anthony Minessale II <anthm@freeswitch.org>
 * Portions created by the Initial Developer are Copyright (C)
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 * 
 * Anthony Minessale II <anthm@freeswitch.org>
 * Eliot Gable <egable@gmail.com>
 *
 * switch_apr.h -- APR includes header
 *
 */
/*! \file switch_apr.h
    \brief APR includes header
	
	The things powered by APR are renamed into the switch_ namespace to provide a cleaner
	look to things and helps me to document what parts of APR I am using I'd like to take this
	opportunity to thank APR for all the awesome stuff it does and for making my life much easier.

*/
#ifndef SWITCH_APR_H
#define SWITCH_APR_H

SWITCH_BEGIN_EXTERN_C

SWITCH_DECLARE(int) switch_status_is_timeup(int status);

#ifdef WIN32
typedef DWORD switch_thread_id_t;
#else
#include <pthread.h>
typedef pthread_t switch_thread_id_t;
#endif

SWITCH_DECLARE(switch_thread_id_t) switch_thread_self(void);

/*! \brief Compare two thread ids 
 *  \param tid1 1st Thread ID to compare
 *  \param tid2 2nd Thread ID to compare
*/
SWITCH_DECLARE(int) switch_thread_equal(switch_thread_id_t tid1, switch_thread_id_t tid2);


/*
   The pieces of apr we allow ppl to pass around between modules we typedef into our namespace and wrap all the functions
   any other apr code should be as hidden as possible.
*/
/**
 * @defgroup switch_apr Brought To You By APR
 * @ingroup FREESWITCH
 * @{
 */
/**
 * @defgroup switch_memory_pool Memory Pool Functions
 * @ingroup switch_apr 
 * @{
 */
/** The fundamental pool type */
/* see switch types.h 	typedef struct apr_pool_t switch_memory_pool_t;*/
/**
 * Clear all memory in the pool and run all the cleanups. This also destroys all
 * subpools.
 * @param pool The pool to clear
 * @remark This does not actually free the memory, it just allows the pool
 *         to re-use this memory for the next allocation.
 * @see apr_pool_destroy()
 */
SWITCH_DECLARE(void) switch_pool_clear(switch_memory_pool_t *pool);

/** @} */

/**
 * @defgroup switch_string String Handling funcions
 * @ingroup switch_apr
 * @{
 */

SWITCH_DECLARE(int) switch_snprintf(_Out_z_cap_(len)
									char *buf, _In_ switch_size_t len, _In_z_ _Printf_format_string_ const char *format, ...);

SWITCH_DECLARE(int) switch_vasprintf(_Out_opt_ char **buf, _In_z_ _Printf_format_string_ const char *format, _In_ va_list ap);

SWITCH_DECLARE(int) switch_vsnprintf(char *buf, switch_size_t len, const char *format, va_list ap);

SWITCH_DECLARE(char *) switch_copy_string(_Out_z_cap_(dst_size)
										  char *dst, _In_z_ const char *src, _In_ switch_size_t dst_size);

/** @} */

#if 0
/**
 * @defgroup apr_hash Hash Tables
 * @ingroup switch_apr
 * @{
 */

/** Abstract type for hash tables. */
	 typedef struct apr_hash_t switch_hash_t;

/** Abstract type for scanning hash tables. */
	 typedef struct apr_hash_index_t switch_hash_index_t;

/**
 * When passing a key to switch_hashfunc_default, this value can be
 * passed to indicate a string-valued key, and have the length compute automatically.
 *
 */
#define SWITCH_HASH_KEY_STRING     (-1)

/**
 * Start iterating over the entries in a hash table.
 * @param p The pool to allocate the switch_hash_index_t iterator. If this
 *          pool is NULL, then an internal, non-thread-safe iterator is used.
 * @param ht The hash table
 * @remark  There is no restriction on adding or deleting hash entries during
 * an iteration (although the results may be unpredictable unless all you do
 * is delete the current entry) and multiple iterations can be in
 * progress at the same time.

 */
SWITCH_DECLARE(switch_hash_index_t *) switch_core_hash_first(switch_memory_pool_t *pool, switch_hash_t *ht);

/**
 * Continue iterating over the entries in a hash table.
 * @param ht The iteration state
 * @return a pointer to the updated iteration state.  NULL if there are no more  
 *         entries.
 */
SWITCH_DECLARE(switch_hash_index_t *) switch_core_hash_next(switch_hash_index_t *ht);

/**
 * Get the current entry's details from the iteration state.
 * @param hi The iteration state
 * @param key Return pointer for the pointer to the key.
 * @param klen Return pointer for the key length.
 * @param val Return pointer for the associated value.
 * @remark The return pointers should point to a variable that will be set to the
 *         corresponding data, or they may be NULL if the data isn't interesting.
 */
SWITCH_DECLARE(void) switch_core_hash_this(switch_hash_index_t *hi, const void **key, switch_ssize_t *klen, void **val);



SWITCH_DECLARE(switch_memory_pool_t *) switch_hash_pool_get(switch_hash_t *ht);

/** @} */


#endif

/**
 * The default hash function.
 * @param key pointer to the key.
 * @param klen the key length.
 * 
 */
SWITCH_DECLARE(unsigned int) switch_hashfunc_default(const char *key, switch_ssize_t *klen);

SWITCH_DECLARE(unsigned int) switch_ci_hashfunc_default(const char *char_key, switch_ssize_t *klen);


 /**
 * @defgroup switch_time Time Routines
 * @ingroup switch_apr 
 * @{
 */

 /** number of microseconds since 00:00:00 january 1, 1970 UTC */
	 typedef int64_t switch_time_t;

 /** number of microseconds in the interval */
	 typedef int64_t switch_interval_time_t;

/**
 * a structure similar to ANSI struct tm with the following differences:
 *  - tm_usec isn't an ANSI field
 *  - tm_gmtoff isn't an ANSI field (it's a bsdism)
 */
	 typedef struct switch_time_exp_t {
	/** microseconds past tm_sec */
		 int32_t tm_usec;
	/** (0-61) seconds past tm_min */
		 int32_t tm_sec;
	/** (0-59) minutes past tm_hour */
		 int32_t tm_min;
	/** (0-23) hours past midnight */
		 int32_t tm_hour;
	/** (1-31) day of the month */
		 int32_t tm_mday;
	/** (0-11) month of the year */
		 int32_t tm_mon;
	/** year since 1900 */
		 int32_t tm_year;
	/** (0-6) days since sunday */
		 int32_t tm_wday;
	/** (0-365) days since jan 1 */
		 int32_t tm_yday;
	/** daylight saving time */
		 int32_t tm_isdst;
	/** seconds east of UTC */
		 int32_t tm_gmtoff;
	 } switch_time_exp_t;

SWITCH_DECLARE(switch_time_t) switch_time_make(switch_time_t sec, int32_t usec);

/**
 * @return the current time
 */
SWITCH_DECLARE(switch_time_t) switch_time_now(void);

/**
 * Convert time value from human readable format to a numeric apr_time_t that
 * always represents GMT
 * @param result the resulting imploded time
 * @param input the input exploded time
 */
SWITCH_DECLARE(switch_status_t) switch_time_exp_gmt_get(switch_time_t *result, switch_time_exp_t *input);

/**
 * formats the exploded time according to the format specified
 * @param s string to write to
 * @param retsize The length of the returned string
 * @param max The maximum length of the string
 * @param format The format for the time string
 * @param tm The time to convert
 */
SWITCH_DECLARE(switch_status_t) switch_strftime(char *s, switch_size_t *retsize, switch_size_t max, const char *format, switch_time_exp_t *tm);

/**
 * formats the exploded time according to the format specified (does not validate format string)
 * @param s string to write to
 * @param retsize The length of the returned string
 * @param max The maximum length of the string
 * @param format The format for the time string
 * @param tm The time to convert
 */
SWITCH_DECLARE(switch_status_t) switch_strftime_nocheck(char *s, switch_size_t *retsize, switch_size_t max, const char *format, switch_time_exp_t *tm);

/**
 * switch_rfc822_date formats dates in the RFC822
 * format in an efficient manner.  It is a fixed length
 * format which requires the indicated amount of storage,
 * including the trailing NUL terminator.
 * @param date_str String to write to.
 * @param t the time to convert 
 */
SWITCH_DECLARE(switch_status_t) switch_rfc822_date(char *date_str, switch_time_t t);

/**
 * convert a time to its human readable components in GMT timezone
 * @param result the exploded time
 * @param input the time to explode
 */
SWITCH_DECLARE(switch_status_t) switch_time_exp_gmt(switch_time_exp_t *result, switch_time_t input);

/**
 * Convert time value from human readable format to a numeric apr_time_t 
 * e.g. elapsed usec since epoch
 * @param result the resulting imploded time
 * @param input the input exploded time
 */
SWITCH_DECLARE(switch_status_t) switch_time_exp_get(switch_time_t *result, switch_time_exp_t *input);

/**
 * convert a time to its human readable components in local timezone
 * @param result the exploded time
 * @param input the time to explode
 */
SWITCH_DECLARE(switch_status_t) switch_time_exp_lt(switch_time_exp_t *result, switch_time_t input);

/**
 * convert a time to its human readable components in a specific timezone with offset
 * @param result the exploded time
 * @param input the time to explode
 */
SWITCH_DECLARE(switch_status_t) switch_time_exp_tz(switch_time_exp_t *result, switch_time_t input, switch_int32_t offs);

/**
 * Sleep for the specified number of micro-seconds.
 * @param t desired amount of time to sleep.
 * @warning May sleep for longer than the specified time. 
 */
SWITCH_DECLARE(void) switch_sleep(switch_interval_time_t t);
SWITCH_DECLARE(void) switch_micro_sleep(switch_interval_time_t t);

/** @} */

/**
 * @defgroup switch_thread_mutex Thread Mutex Routines
 * @ingroup switch_apr
 * @{
 */

/** Opaque thread-local mutex structure */
	 typedef struct apr_thread_mutex_t switch_mutex_t;

/** Lock Flags */
#define SWITCH_MUTEX_DEFAULT	0x0	/**< platform-optimal lock behavior */
#define SWITCH_MUTEX_NESTED		0x1	/**< enable nested (recursive) locks */
#define	SWITCH_MUTEX_UNNESTED	0x2	/**< disable nested locks */

/**
 * Create and initialize a mutex that can be used to synchronize threads.
 * @param lock the memory address where the newly created mutex will be
 *        stored.
 * @param flags Or'ed value of:
 * <PRE>
 *           SWITCH_THREAD_MUTEX_DEFAULT   platform-optimal lock behavior.
 *           SWITCH_THREAD_MUTEX_NESTED    enable nested (recursive) locks.
 *           SWITCH_THREAD_MUTEX_UNNESTED  disable nested locks (non-recursive).
 * </PRE>
 * @param pool the pool from which to allocate the mutex.
 * @warning Be cautious in using SWITCH_THREAD_MUTEX_DEFAULT.  While this is the
 * most optimial mutex based on a given platform's performance charateristics,
 * it will behave as either a nested or an unnested lock.
 *
*/
SWITCH_DECLARE(switch_status_t) switch_mutex_init(switch_mutex_t ** lock, unsigned int flags, switch_memory_pool_t *pool);


/**
 * Destroy the mutex and free the memory associated with the lock.
 * @param lock the mutex to destroy.
 */
SWITCH_DECLARE(switch_status_t) switch_mutex_destroy(switch_mutex_t *lock);

/**
 * Acquire the lock for the given mutex. If the mutex is already locked,
 * the current thread will be put to sleep until the lock becomes available.
 * @param lock the mutex on which to acquire the lock.
 */
SWITCH_DECLARE(switch_status_t) switch_mutex_lock(switch_mutex_t *lock);

/**
 * Release the lock for the given mutex.
 * @param lock the mutex from which to release the lock.
 */
SWITCH_DECLARE(switch_status_t) switch_mutex_unlock(switch_mutex_t *lock);

/**
 * Attempt to acquire the lock for the given mutex. If the mutex has already
 * been acquired, the call returns immediately with APR_EBUSY. Note: it
 * is important that the APR_STATUS_IS_EBUSY(s) macro be used to determine
 * if the return value was APR_EBUSY, for portability reasons.
 * @param lock the mutex on which to attempt the lock acquiring.
 */
SWITCH_DECLARE(switch_status_t) switch_mutex_trylock(switch_mutex_t *lock);

/** @} */

/**
 * @defgroup switch_atomic Multi-Threaded Adtomic Operations Routines
 * @ingroup switch_apr
 * @{
 */

/** Opaque type used for the atomic operations */
#ifdef apr_atomic_t
    typedef apr_atomic_t switch_atomic_t;
#else
    typedef uint32_t switch_atomic_t;
#endif

/**
 * Some architectures require atomic operations internal structures to be
 * initialized before use.
 * @param pool The memory pool to use when initializing the structures.
 */
SWITCH_DECLARE(switch_status_t) switch_atomic_init(switch_memory_pool_t *pool);

/**
 * Uses an atomic operation to read the uint32 value at the location specified
 * by mem.
 * @param mem The location of memory which stores the value to read.
 */
SWITCH_DECLARE(uint32_t) switch_atomic_read(volatile switch_atomic_t *mem);

/**
 * Uses an atomic operation to set a uint32 value at a specified location of
 * memory.
 * @param mem The location of memory to set.
 * @param val The uint32 value to set at the memory location.
 */
SWITCH_DECLARE(void) switch_atomic_set(volatile switch_atomic_t *mem, uint32_t val);

/**
 * Uses an atomic operation to add the uint32 value to the value at the
 * specified location of memory.
 * @param mem The location of the value to add to.
 * @param val The uint32 value to add to the value at the memory location.
 */
SWITCH_DECLARE(void) switch_atomic_add(volatile switch_atomic_t *mem, uint32_t val);

/**
 * Uses an atomic operation to increment the value at the specified memroy
 * location.
 * @param mem The location of the value to increment.
 */
SWITCH_DECLARE(void) switch_atomic_inc(volatile switch_atomic_t *mem);

/**
 * Uses an atomic operation to decrement the value at the specified memroy
 * location.
 * @param mem The location of the value to decrement.
 */
SWITCH_DECLARE(int)  switch_atomic_dec(volatile switch_atomic_t *mem);

/** @} */

/**
 * @defgroup switch_thread_rwlock Thread Read/Write lock Routines
 * @ingroup switch_apr
 * @{
 */

/** Opaque structure used for the rwlock */
	 typedef struct apr_thread_rwlock_t switch_thread_rwlock_t;

SWITCH_DECLARE(switch_status_t) switch_thread_rwlock_create(switch_thread_rwlock_t ** rwlock, switch_memory_pool_t *pool);
SWITCH_DECLARE(switch_status_t) switch_thread_rwlock_destroy(switch_thread_rwlock_t *rwlock);
SWITCH_DECLARE(switch_memory_pool_t *) switch_thread_rwlock_pool_get(switch_thread_rwlock_t *rwlock);
SWITCH_DECLARE(switch_status_t) switch_thread_rwlock_rdlock(switch_thread_rwlock_t *rwlock);
SWITCH_DECLARE(switch_status_t) switch_thread_rwlock_tryrdlock(switch_thread_rwlock_t *rwlock);
SWITCH_DECLARE(switch_status_t) switch_thread_rwlock_wrlock(switch_thread_rwlock_t *rwlock);
SWITCH_DECLARE(switch_status_t) switch_thread_rwlock_trywrlock(switch_thread_rwlock_t *rwlock);
SWITCH_DECLARE(switch_status_t) switch_thread_rwlock_trywrlock_timeout(switch_thread_rwlock_t *rwlock, int timeout);
SWITCH_DECLARE(switch_status_t) switch_thread_rwlock_unlock(switch_thread_rwlock_t *rwlock);

/** @} */

/**
 * @defgroup switch_thread_cond Condition Variable Routines
 * @ingroup switch_apr 
 * @{
 */

/**
 * Note: destroying a condition variable (or likewise, destroying or
 * clearing the pool from which a condition variable was allocated) if
 * any threads are blocked waiting on it gives undefined results.
 */

/** Opaque structure for thread condition variables */
	 typedef struct apr_thread_cond_t switch_thread_cond_t;

/**
 * Create and initialize a condition variable that can be used to signal
 * and schedule threads in a single process.
 * @param cond the memory address where the newly created condition variable
 *        will be stored.
 * @param pool the pool from which to allocate the mutex.
 */
SWITCH_DECLARE(switch_status_t) switch_thread_cond_create(switch_thread_cond_t ** cond, switch_memory_pool_t *pool);

/**
 * Put the active calling thread to sleep until signaled to wake up. Each
 * condition variable must be associated with a mutex, and that mutex must
 * be locked before  calling this function, or the behavior will be
 * undefined. As the calling thread is put to sleep, the given mutex
 * will be simultaneously released; and as this thread wakes up the lock
 * is again simultaneously acquired.
 * @param cond the condition variable on which to block.
 * @param mutex the mutex that must be locked upon entering this function,
 *        is released while the thread is asleep, and is again acquired before
 *        returning from this function.
 */
SWITCH_DECLARE(switch_status_t) switch_thread_cond_wait(switch_thread_cond_t *cond, switch_mutex_t *mutex);

/**
 * Put the active calling thread to sleep until signaled to wake up or
 * the timeout is reached. Each condition variable must be associated
 * with a mutex, and that mutex must be locked before calling this
 * function, or the behavior will be undefined. As the calling thread
 * is put to sleep, the given mutex will be simultaneously released;
 * and as this thread wakes up the lock is again simultaneously acquired.
 * @param cond the condition variable on which to block.
 * @param mutex the mutex that must be locked upon entering this function,
 *        is released while the thread is asleep, and is again acquired before
 *        returning from this function.
 * @param timeout The amount of time in microseconds to wait. This is 
 *        a maximum, not a minimum. If the condition is signaled, we 
 *        will wake up before this time, otherwise the error APR_TIMEUP
 *        is returned.
 */
SWITCH_DECLARE(switch_status_t) switch_thread_cond_timedwait(switch_thread_cond_t *cond, switch_mutex_t *mutex, switch_interval_time_t timeout);

/**
 * Signals a single thread, if one exists, that is blocking on the given
 * condition variable. That thread is then scheduled to wake up and acquire
 * the associated mutex. Although it is not required, if predictable scheduling
 * is desired, that mutex must be locked while calling this function.
 * @param cond the condition variable on which to produce the signal.
 */
SWITCH_DECLARE(switch_status_t) switch_thread_cond_signal(switch_thread_cond_t *cond);

/**
 * Signals all threads blocking on the given condition variable.
 * Each thread that was signaled is then scheduled to wake up and acquire
 * the associated mutex. This will happen in a serialized manner.
 * @param cond the condition variable on which to produce the broadcast.
 */
SWITCH_DECLARE(switch_status_t) switch_thread_cond_broadcast(switch_thread_cond_t *cond);

/**
 * Destroy the condition variable and free the associated memory.
 * @param cond the condition variable to destroy.
 */
SWITCH_DECLARE(switch_status_t) switch_thread_cond_destroy(switch_thread_cond_t *cond);

/** @} */

/**
 * @defgroup switch_UUID UUID Handling
 * @ingroup switch_apr
 * @{
 */

/** we represent a UUID as a block of 16 bytes. */

	 typedef struct {
		 unsigned char data[16];
							/**< the actual UUID */
	 } switch_uuid_t;

/** UUIDs are formatted as: 00112233-4455-6677-8899-AABBCCDDEEFF */
#define SWITCH_UUID_FORMATTED_LENGTH 256

#define SWITCH_MD5_DIGESTSIZE 16
#define SWITCH_MD5_DIGEST_STRING_SIZE 33

/**
 * Format a UUID into a string, following the standard format
 * @param buffer The buffer to place the formatted UUID string into. It must
 *               be at least APR_UUID_FORMATTED_LENGTH + 1 bytes long to hold
 *               the formatted UUID and a null terminator
 * @param uuid The UUID to format
 */
SWITCH_DECLARE(void) switch_uuid_format(char *buffer, const switch_uuid_t *uuid);

/**
 * Generate and return a (new) UUID
 * @param uuid The resulting UUID
 */
SWITCH_DECLARE(void) switch_uuid_get(switch_uuid_t *uuid);

/**
 * Parse a standard-format string into a UUID
 * @param uuid The resulting UUID
 * @param uuid_str The formatted UUID
 */
SWITCH_DECLARE(switch_status_t) switch_uuid_parse(switch_uuid_t *uuid, const char *uuid_str);

/**
 * MD5 in one step
 * @param digest The final MD5 digest
 * @param input The message block to use
 * @param inputLen The length of the message block
 */
SWITCH_DECLARE(switch_status_t) switch_md5(unsigned char digest[SWITCH_MD5_DIGESTSIZE], const void *input, switch_size_t inputLen);
SWITCH_DECLARE(switch_status_t) switch_md5_string(char digest_str[SWITCH_MD5_DIGEST_STRING_SIZE], const void *input, switch_size_t inputLen);

/** @} */

/**
 * @defgroup switch_FIFO Thread Safe FIFO bounded queue
 * @ingroup switch_apr
 * @{
 */

/** Opaque structure used for queue API */
	 typedef struct apr_queue_t switch_queue_t;

/** 
 * create a FIFO queue
 * @param queue The new queue
 * @param queue_capacity maximum size of the queue
 * @param pool a pool to allocate queue from
 */
SWITCH_DECLARE(switch_status_t) switch_queue_create(switch_queue_t ** queue, unsigned int queue_capacity, switch_memory_pool_t *pool);

/**
 * pop/get an object from the queue, blocking if the queue is already empty
 *
 * @param queue the queue
 * @param data the data
 * @returns APR_EINTR the blocking was interrupted (try again)
 * @returns APR_EOF if the queue has been terminated
 * @returns APR_SUCCESS on a successfull pop
 */
SWITCH_DECLARE(switch_status_t) switch_queue_pop(switch_queue_t *queue, void **data);

/**
 * pop/get an object from the queue, blocking if the queue is already empty
 *
 * @param queue the queue
 * @param data the data
 * @param timeout The amount of time in microseconds to wait. This is
 *        a maximum, not a minimum. If the condition is signaled, we
 *        will wake up before this time, otherwise the error APR_TIMEUP
 *        is returned.
 * @returns APR_TIMEUP the request timed out
 * @returns APR_EINTR the blocking was interrupted (try again)
 * @returns APR_EOF if the queue has been terminated
 * @returns APR_SUCCESS on a successfull pop
 */
SWITCH_DECLARE(switch_status_t) switch_queue_pop_timeout(switch_queue_t *queue, void **data, switch_interval_time_t timeout);

/**
 * push/add a object to the queue, blocking if the queue is already full
 *
 * @param queue the queue
 * @param data the data
 * @returns APR_EINTR the blocking was interrupted (try again)
 * @returns APR_EOF the queue has been terminated
 * @returns APR_SUCCESS on a successfull push
 */
SWITCH_DECLARE(switch_status_t) switch_queue_push(switch_queue_t *queue, void *data);

/**
 * returns the size of the queue.
 *
 * @warning this is not threadsafe, and is intended for reporting/monitoring
 * of the queue.
 * @param queue the queue
 * @returns the size of the queue
 */
SWITCH_DECLARE(unsigned int) switch_queue_size(switch_queue_t *queue);

/**
 * pop/get an object to the queue, returning immediatly if the queue is empty
 *
 * @param queue the queue
 * @param data the data
 * @returns APR_EINTR the blocking operation was interrupted (try again)
 * @returns APR_EAGAIN the queue is empty
 * @returns APR_EOF the queue has been terminated
 * @returns APR_SUCCESS on a successfull push
 */
SWITCH_DECLARE(switch_status_t) switch_queue_trypop(switch_queue_t *queue, void **data);

SWITCH_DECLARE(switch_status_t) switch_queue_interrupt_all(switch_queue_t *queue);

/**
 * push/add a object to the queue, returning immediatly if the queue is full
 *
 * @param queue the queue
 * @param data the data
 * @returns APR_EINTR the blocking operation was interrupted (try again)
 * @returns APR_EAGAIN the queue is full
 * @returns APR_EOF the queue has been terminated
 * @returns APR_SUCCESS on a successfull push
 */
SWITCH_DECLARE(switch_status_t) switch_queue_trypush(switch_queue_t *queue, void *data);

/** @} */

/**
 * @defgroup switch_file_io File I/O Handling Functions
 * @ingroup switch_apr 
 * @{
 */

/** Structure for referencing files. */
	 typedef struct apr_file_t switch_file_t;

	 typedef int32_t switch_fileperms_t;
	 typedef int switch_seek_where_t;

	 /**
 * @defgroup apr_file_seek_flags File Seek Flags
 * @{
 */

/* flags for apr_file_seek */
/** Set the file position */
#define SWITCH_SEEK_SET SEEK_SET
/** Current */
#define SWITCH_SEEK_CUR SEEK_CUR
/** Go to end of file */
#define SWITCH_SEEK_END SEEK_END
/** @} */


/**
 * @defgroup switch_file_permissions File Permissions flags 
 * @ingroup switch_file_io
 * @{
 */

#define SWITCH_FPROT_USETID 0x8000			/**< Set user id */
#define SWITCH_FPROT_UREAD 0x0400			/**< Read by user */
#define SWITCH_FPROT_UWRITE 0x0200			/**< Write by user */
#define SWITCH_FPROT_UEXECUTE 0x0100		/**< Execute by user */

#define SWITCH_FPROT_GSETID 0x4000			/**< Set group id */
#define SWITCH_FPROT_GREAD 0x0040			/**< Read by group */
#define SWITCH_FPROT_GWRITE 0x0020			/**< Write by group */
#define SWITCH_FPROT_GEXECUTE 0x0010		/**< Execute by group */

#define SWITCH_FPROT_WSTICKY 0x2000
#define SWITCH_FPROT_WREAD 0x0004			/**< Read by others */
#define SWITCH_FPROT_WWRITE 0x0002			/**< Write by others */
#define SWITCH_FPROT_WEXECUTE 0x0001		/**< Execute by others */

#define SWITCH_FPROT_OS_DEFAULT 0x0FFF		/**< use OS's default permissions */

/* additional permission flags for apr_file_copy  and apr_file_append */
#define SWITCH_FPROT_FILE_SOURCE_PERMS 0x1000	/**< Copy source file's permissions */
/** @} */

/* File lock types/flags */
/**
 * @defgroup switch_file_lock_types File Lock Types
 * @{
 */

#define SWITCH_FLOCK_SHARED        1	   /**< Shared lock. More than one process
                                           or thread can hold a shared lock
                                           at any given time. Essentially,
                                           this is a "read lock", preventing
                                           writers from establishing an
                                           exclusive lock. */
#define SWITCH_FLOCK_EXCLUSIVE     2	   /**< Exclusive lock. Only one process
                                           may hold an exclusive lock at any
                                           given time. This is analogous to
                                           a "write lock". */

#define SWITCH_FLOCK_TYPEMASK      0x000F  /**< mask to extract lock type */
#define SWITCH_FLOCK_NONBLOCK      0x0010  /**< do not block while acquiring the
                                           file lock */

 /** @} */

/**
 * @defgroup switch_file_open_flags File Open Flags/Routines
 * @ingroup switch_file_io
 * @{
 */
#define SWITCH_FOPEN_READ				0x00001		/**< Open the file for reading */
#define SWITCH_FOPEN_WRITE				0x00002		/**< Open the file for writing */
#define SWITCH_FOPEN_CREATE				0x00004		/**< Create the file if not there */
#define SWITCH_FOPEN_APPEND				0x00008		/**< Append to the end of the file */
#define SWITCH_FOPEN_TRUNCATE			0x00010		/**< Open the file and truncate to 0 length */
#define SWITCH_FOPEN_BINARY				0x00020		/**< Open the file in binary mode */
#define SWITCH_FOPEN_EXCL				0x00040		/**< Open should fail if APR_CREATE and file exists. */
#define SWITCH_FOPEN_BUFFERED			0x00080		/**< Open the file for buffered I/O */
#define SWITCH_FOPEN_DELONCLOSE			0x00100		/**< Delete the file after close */
#define SWITCH_FOPEN_XTHREAD			0x00200		/**< Platform dependent tag to open the file for use across multiple threads */
#define SWITCH_FOPEN_SHARELOCK			0x00400		/**< Platform dependent support for higher level locked read/write access to support writes across process/machines */
#define SWITCH_FOPEN_NOCLEANUP			0x00800		/**< Do not register a cleanup when the file is opened */
#define SWITCH_FOPEN_SENDFILE_ENABLED	0x01000		/**< Advisory flag that this file should support apr_socket_sendfile operation */
#define SWITCH_FOPEN_LARGEFILE			0x04000		/**< Platform dependent flag to enable large file support */
/** @} */

/**
 * Open the specified file.
 * @param newf The opened file descriptor.
 * @param fname The full path to the file (using / on all systems)
 * @param flag Or'ed value of:
 * <PRE>
 *         SWITCH_FOPEN_READ				open for reading
 *         SWITCH_FOPEN_WRITE				open for writing
 *         SWITCH_FOPEN_CREATE				create the file if not there
 *         SWITCH_FOPEN_APPEND				file ptr is set to end prior to all writes
 *         SWITCH_FOPEN_TRUNCATE			set length to zero if file exists
 *         SWITCH_FOPEN_BINARY				not a text file (This flag is ignored on 
 *											UNIX because it has no meaning)
 *         SWITCH_FOPEN_BUFFERED			buffer the data.  Default is non-buffered
 *         SWITCH_FOPEN_EXCL				return error if APR_CREATE and file exists
 *         SWITCH_FOPEN_DELONCLOSE			delete the file after closing.
 *         SWITCH_FOPEN_XTHREAD				Platform dependent tag to open the file
 *											for use across multiple threads
 *         SWITCH_FOPEN_SHARELOCK			Platform dependent support for higher
 *											level locked read/write access to support
 *											writes across process/machines
 *         SWITCH_FOPEN_NOCLEANUP			Do not register a cleanup with the pool 
 *											passed in on the <EM>pool</EM> argument (see below).
 *											The apr_os_file_t handle in apr_file_t will not
 *											be closed when the pool is destroyed.
 *         SWITCH_FOPEN_SENDFILE_ENABLED	Open with appropriate platform semantics
 *											for sendfile operations.  Advisory only,
 *											apr_socket_sendfile does not check this flag.
 * </PRE>
 * @param perm Access permissions for file.
 * @param pool The pool to use.
 * @remark If perm is SWITCH_FPROT_OS_DEFAULT and the file is being created,
 * appropriate default permissions will be used.
 */
SWITCH_DECLARE(switch_status_t) switch_file_open(switch_file_t ** newf, const char *fname, int32_t flag, switch_fileperms_t perm,
												 switch_memory_pool_t *pool);


SWITCH_DECLARE(switch_status_t) switch_file_seek(switch_file_t *thefile, switch_seek_where_t where, int64_t *offset);


SWITCH_DECLARE(switch_status_t) switch_file_copy(const char *from_path, const char *to_path, switch_fileperms_t perms, switch_memory_pool_t *pool);

/**
 * Close the specified file.
 * @param thefile The file descriptor to close.
 */
SWITCH_DECLARE(switch_status_t) switch_file_close(switch_file_t *thefile);

SWITCH_DECLARE(switch_status_t) switch_file_trunc(switch_file_t *thefile, int64_t offset);

SWITCH_DECLARE(switch_status_t) switch_file_lock(switch_file_t *thefile, int type);

/**
 * Delete the specified file.
 * @param path The full path to the file (using / on all systems)
 * @param pool The pool to use.
 * @remark If the file is open, it won't be removed until all
 * instances are closed.
 */
SWITCH_DECLARE(switch_status_t) switch_file_remove(const char *path, switch_memory_pool_t *pool);

SWITCH_DECLARE(switch_status_t) switch_file_rename(const char *from_path, const char *to_path, switch_memory_pool_t *pool);

/**
 * Read data from the specified file.
 * @param thefile The file descriptor to read from.
 * @param buf The buffer to store the data to.
 * @param nbytes On entry, the number of bytes to read; on exit, the number
 * of bytes read.
 *
 * @remark apr_file_read will read up to the specified number of
 * bytes, but never more.  If there isn't enough data to fill that
 * number of bytes, all of the available data is read.  The third
 * argument is modified to reflect the number of bytes read.  If a
 * char was put back into the stream via ungetc, it will be the first
 * character returned.
 *
 * @remark It is not possible for both bytes to be read and an APR_EOF
 * or other error to be returned.  APR_EINTR is never returned.
 */
SWITCH_DECLARE(switch_status_t) switch_file_read(switch_file_t *thefile, void *buf, switch_size_t *nbytes);

/**
 * Write data to the specified file.
 * @param thefile The file descriptor to write to.
 * @param buf The buffer which contains the data.
 * @param nbytes On entry, the number of bytes to write; on exit, the number 
 *               of bytes written.
 *
 * @remark apr_file_write will write up to the specified number of
 * bytes, but never more.  If the OS cannot write that many bytes, it
 * will write as many as it can.  The third argument is modified to
 * reflect the * number of bytes written.
 *
 * @remark It is possible for both bytes to be written and an error to
 * be returned.  APR_EINTR is never returned.
 */
SWITCH_DECLARE(switch_status_t) switch_file_write(switch_file_t *thefile, const void *buf, switch_size_t *nbytes);
SWITCH_DECLARE(int) switch_file_printf(switch_file_t *thefile, const char *format, ...);

SWITCH_DECLARE(switch_status_t) switch_file_mktemp(switch_file_t ** thefile, char *templ, int32_t flags, switch_memory_pool_t *pool);

SWITCH_DECLARE(switch_size_t) switch_file_get_size(switch_file_t *thefile);

SWITCH_DECLARE(switch_status_t) switch_file_exists(const char *filename, switch_memory_pool_t *pool);

SWITCH_DECLARE(switch_status_t) switch_directory_exists(const char *dirname, switch_memory_pool_t *pool);

/**
* Create a new directory on the file system.
* @param path the path for the directory to be created. (use / on all systems)
* @param perm Permissions for the new direcoty.
* @param pool the pool to use.
*/
SWITCH_DECLARE(switch_status_t) switch_dir_make(const char *path, switch_fileperms_t perm, switch_memory_pool_t *pool);

/** Creates a new directory on the file system, but behaves like
* 'mkdir -p'. Creates intermediate directories as required. No error
* will be reported if PATH already exists.
* @param path the path for the directory to be created. (use / on all systems)
* @param perm Permissions for the new direcoty.
* @param pool the pool to use.
*/
SWITCH_DECLARE(switch_status_t) switch_dir_make_recursive(const char *path, switch_fileperms_t perm, switch_memory_pool_t *pool);

	 typedef struct switch_dir switch_dir_t;

	 struct switch_array_header_t {
	/** The pool the array is allocated out of */
		 switch_memory_pool_t *pool;
	/** The amount of memory allocated for each element of the array */
		 int elt_size;
	/** The number of active elements in the array */
		 int nelts;
	/** The number of elements allocated in the array */
		 int nalloc;
	/** The elements in the array */
		 char *elts;
	 };
	 typedef struct switch_array_header_t switch_array_header_t;

SWITCH_DECLARE(switch_status_t) switch_dir_open(switch_dir_t ** new_dir, const char *dirname, switch_memory_pool_t *pool);
SWITCH_DECLARE(switch_status_t) switch_dir_close(switch_dir_t *thedir);
SWITCH_DECLARE(const char *) switch_dir_next_file(switch_dir_t *thedir, char *buf, switch_size_t len);

/** @} */

/**
 * @defgroup switch_thread_proc Threads and Process Functions
 * @ingroup switch_apr 
 * @{
 */

/** Opaque Thread structure. */
	 typedef struct apr_thread_t switch_thread_t;

/** Opaque Thread attributes structure. */
	 typedef struct apr_threadattr_t switch_threadattr_t;

/**
 * The prototype for any APR thread worker functions.
 * typedef void *(SWITCH_THREAD_FUNC *switch_thread_start_t)(switch_thread_t*, void*);
 */
	 typedef void *(SWITCH_THREAD_FUNC * switch_thread_start_t) (switch_thread_t *, void *);

//APR_DECLARE(apr_status_t) apr_threadattr_stacksize_set(apr_threadattr_t *attr, switch_size_t stacksize)
SWITCH_DECLARE(switch_status_t) switch_threadattr_stacksize_set(switch_threadattr_t *attr, switch_size_t stacksize);

SWITCH_DECLARE(switch_status_t) switch_threadattr_priority_set(switch_threadattr_t *attr, switch_thread_priority_t priority);


/**
 * Create and initialize a new threadattr variable
 * @param new_attr The newly created threadattr.
 * @param pool The pool to use
 */
SWITCH_DECLARE(switch_status_t) switch_threadattr_create(switch_threadattr_t ** new_attr, switch_memory_pool_t *pool);

/**
 * Set if newly created threads should be created in detached state.
 * @param attr The threadattr to affect 
 * @param on Non-zero if detached threads should be created.
 */
SWITCH_DECLARE(switch_status_t) switch_threadattr_detach_set(switch_threadattr_t *attr, int32_t on);

/**
 * Create a new thread of execution
 * @param new_thread The newly created thread handle.
 * @param attr The threadattr to use to determine how to create the thread
 * @param func The function to start the new thread in
 * @param data Any data to be passed to the starting function
 * @param cont The pool to use
 */
SWITCH_DECLARE(switch_status_t) switch_thread_create(switch_thread_t ** new_thread, switch_threadattr_t *attr,
													 switch_thread_start_t func, void *data, switch_memory_pool_t *cont);

/** @} */

/**
 * @defgroup switch_network_io Network Routines
 * @ingroup switch_apr 
 * @{
 */

#define SWITCH_SO_LINGER 1
#define SWITCH_SO_KEEPALIVE 2
#define SWITCH_SO_DEBUG 4
#define SWITCH_SO_NONBLOCK 8
#define SWITCH_SO_REUSEADDR 16
#define SWITCH_SO_SNDBUF 64
#define SWITCH_SO_RCVBUF 128
#define SWITCH_SO_DISCONNECTED 256
#define SWITCH_SO_TCP_NODELAY 512
#define SWITCH_SO_TCP_KEEPIDLE 520
#define SWITCH_SO_TCP_KEEPINTVL 530


 /**
 * @def SWITCH_INET
 * Not all platforms have these defined, so we'll define them here
 * The default values come from FreeBSD 4.1.1
 */
#define SWITCH_INET     AF_INET
#ifdef AF_INET6
#define SWITCH_INET6    AF_INET6
#else
#define SWITCH_INET6 0
#endif

/** @def SWITCH_UNSPEC
 * Let the system decide which address family to use
 */
#ifdef AF_UNSPEC
#define SWITCH_UNSPEC   AF_UNSPEC
#else
#define SWITCH_UNSPEC   0
#endif

/** A structure to represent sockets */
	 typedef struct apr_socket_t switch_socket_t;

/** Freeswitch's socket address type, used to ensure protocol independence */
	 typedef struct apr_sockaddr_t switch_sockaddr_t;

	 typedef enum {
		 SWITCH_SHUTDOWN_READ,	   /**< no longer allow read request */
		 SWITCH_SHUTDOWN_WRITE,	   /**< no longer allow write requests */
		 SWITCH_SHUTDOWN_READWRITE /**< no longer allow read or write requests */
	 } switch_shutdown_how_e;

/**
 * @defgroup IP_Proto IP Protocol Definitions for use when creating sockets
 * @{
 */
#define SWITCH_PROTO_TCP       6   /**< TCP  */
#define SWITCH_PROTO_UDP      17   /**< UDP  */
#define SWITCH_PROTO_SCTP    132   /**< SCTP */
/** @} */

/* function definitions */

/**
 * Create a socket.
 * @param new_sock The new socket that has been set up.
 * @param family The address family of the socket (e.g., SWITCH_INET).
 * @param type The type of the socket (e.g., SOCK_STREAM).
 * @param protocol The protocol of the socket (e.g., SWITCH_PROTO_TCP).
 * @param pool The pool to use
 */
SWITCH_DECLARE(switch_status_t) switch_socket_create(switch_socket_t ** new_sock, int family, int type, int protocol, switch_memory_pool_t *pool);

/**
 * Shutdown either reading, writing, or both sides of a socket.
 * @param sock The socket to close 
 * @param how How to shutdown the socket.  One of:
 * <PRE>
 *            SWITCH_SHUTDOWN_READ         no longer allow read requests
 *            SWITCH_SHUTDOWN_WRITE        no longer allow write requests
 *            SWITCH_SHUTDOWN_READWRITE    no longer allow read or write requests 
 * </PRE>
 * @see switch_shutdown_how_e
 * @remark This does not actually close the socket descriptor, it just
 *      controls which calls are still valid on the socket.
 */
SWITCH_DECLARE(switch_status_t) switch_socket_shutdown(switch_socket_t *sock, switch_shutdown_how_e how);

/**
 * Close a socket.
 * @param sock The socket to close 
 */
SWITCH_DECLARE(switch_status_t) switch_socket_close(switch_socket_t *sock);

/**
 * Bind the socket to its associated port
 * @param sock The socket to bind 
 * @param sa The socket address to bind to
 * @remark This may be where we will find out if there is any other process
 *      using the selected port.
 */
SWITCH_DECLARE(switch_status_t) switch_socket_bind(switch_socket_t *sock, switch_sockaddr_t *sa);

/**
 * Listen to a bound socket for connections.
 * @param sock The socket to listen on 
 * @param backlog The number of outstanding connections allowed in the sockets
 *                listen queue.  If this value is less than zero, the listen
 *                queue size is set to zero.  
 */
SWITCH_DECLARE(switch_status_t) switch_socket_listen(switch_socket_t *sock, int32_t backlog);

/**
 * Accept a new connection request
 * @param new_sock A copy of the socket that is connected to the socket that
 *                 made the connection request.  This is the socket which should
 *                 be used for all future communication.
 * @param sock The socket we are listening on.
 * @param pool The pool for the new socket.
 */
SWITCH_DECLARE(switch_status_t) switch_socket_accept(switch_socket_t ** new_sock, switch_socket_t *sock, switch_memory_pool_t *pool);

/**
 * Issue a connection request to a socket either on the same machine 
 * or a different one.
 * @param sock The socket we wish to use for our side of the connection 
 * @param sa The address of the machine we wish to connect to.
 */
SWITCH_DECLARE(switch_status_t) switch_socket_connect(switch_socket_t *sock, switch_sockaddr_t *sa);

SWITCH_DECLARE(uint16_t) switch_sockaddr_get_port(switch_sockaddr_t *sa);
SWITCH_DECLARE(const char *) switch_get_addr(char *buf, switch_size_t len, switch_sockaddr_t *in);
SWITCH_DECLARE(int32_t) switch_sockaddr_get_family(switch_sockaddr_t *sa);
SWITCH_DECLARE(switch_status_t) switch_sockaddr_ip_get(char **addr, switch_sockaddr_t *sa);
SWITCH_DECLARE(int) switch_sockaddr_equal(const switch_sockaddr_t *sa1, const switch_sockaddr_t *sa2);


/**
 * Create apr_sockaddr_t from hostname, address family, and port.
 * @param sa The new apr_sockaddr_t.
 * @param hostname The hostname or numeric address string to resolve/parse, or
 *               NULL to build an address that corresponds to 0.0.0.0 or ::
 * @param family The address family to use, or SWITCH_UNSPEC if the system should 
 *               decide.
 * @param port The port number.
 * @param flags Special processing flags:
 * <PRE>
 *       APR_IPV4_ADDR_OK          first query for IPv4 addresses; only look
 *                                 for IPv6 addresses if the first query failed;
 *                                 only valid if family is APR_UNSPEC and hostname
 *                                 isn't NULL; mutually exclusive with
 *                                 APR_IPV6_ADDR_OK
 *       APR_IPV6_ADDR_OK          first query for IPv6 addresses; only look
 *                                 for IPv4 addresses if the first query failed;
 *                                 only valid if family is APR_UNSPEC and hostname
 *                                 isn't NULL and APR_HAVE_IPV6; mutually exclusive
 *                                 with APR_IPV4_ADDR_OK
 * </PRE>
 * @param pool The pool for the apr_sockaddr_t and associated storage.
 */
SWITCH_DECLARE(switch_status_t) switch_sockaddr_info_get(switch_sockaddr_t ** sa, const char *hostname,
														 int32_t family, switch_port_t port, int32_t flags, switch_memory_pool_t *pool);

SWITCH_DECLARE(switch_status_t) switch_sockaddr_create(switch_sockaddr_t **sa, switch_memory_pool_t *pool);

/**
 * Send data over a network.
 * @param sock The socket to send the data over.
 * @param buf The buffer which contains the data to be sent. 
 * @param len On entry, the number of bytes to send; on exit, the number
 *            of bytes sent.
 * @remark
 * <PRE>
 * This functions acts like a blocking write by default.  To change 
 * this behavior, use apr_socket_timeout_set() or the APR_SO_NONBLOCK
 * socket option.
 *
 * It is possible for both bytes to be sent and an error to be returned.
 *
 * APR_EINTR is never returned.
 * </PRE>
 */
SWITCH_DECLARE(switch_status_t) switch_socket_send(switch_socket_t *sock, const char *buf, switch_size_t *len);

/**
 * @param sock The socket to send from
 * @param where The apr_sockaddr_t describing where to send the data
 * @param flags The flags to use
 * @param buf  The data to send
 * @param len  The length of the data to send
 */
SWITCH_DECLARE(switch_status_t) switch_socket_sendto(switch_socket_t *sock, switch_sockaddr_t *where, int32_t flags, const char *buf,
													 switch_size_t *len);
													
SWITCH_DECLARE(switch_status_t) switch_socket_send_nonblock(switch_socket_t *sock, const char *buf, switch_size_t *len);

/**
 * @param from The apr_sockaddr_t to fill in the recipient info
 * @param sock The socket to use
 * @param flags The flags to use
 * @param buf  The buffer to use
 * @param len  The length of the available buffer
 *
 */
SWITCH_DECLARE(switch_status_t) switch_socket_recvfrom(switch_sockaddr_t *from, switch_socket_t *sock, int32_t flags, char *buf, size_t *len);

SWITCH_DECLARE(switch_status_t) switch_socket_atmark(switch_socket_t *sock, int *atmark);

/**
 * Read data from a network.
 * @param sock The socket to read the data from.
 * @param buf The buffer to store the data in. 
 * @param len On entry, the number of bytes to receive; on exit, the number
 *            of bytes received.
 * @remark
 * <PRE>
 * This functions acts like a blocking read by default.  To change 
 * this behavior, use apr_socket_timeout_set() or the APR_SO_NONBLOCK
 * socket option.
 * The number of bytes actually received is stored in argument 3.
 *
 * It is possible for both bytes to be received and an APR_EOF or
 * other error to be returned.
 *
 * APR_EINTR is never returned.
 * </PRE>
 */
SWITCH_DECLARE(switch_status_t) switch_socket_recv(switch_socket_t *sock, char *buf, switch_size_t *len);

/**
 * Setup socket options for the specified socket
 * @param sock The socket to set up.
 * @param opt The option we would like to configure.  One of:
 * <PRE>
 *            APR_SO_DEBUG      --  turn on debugging information 
 *            APR_SO_KEEPALIVE  --  keep connections active
 *            APR_SO_LINGER     --  lingers on close if data is present
 *            APR_SO_NONBLOCK   --  Turns blocking on/off for socket
 *                                  When this option is enabled, use
 *                                  the APR_STATUS_IS_EAGAIN() macro to
 *                                  see if a send or receive function
 *                                  could not transfer data without
 *                                  blocking.
 *            APR_SO_REUSEADDR  --  The rules used in validating addresses
 *                                  supplied to bind should allow reuse
 *                                  of local addresses.
 *            APR_SO_SNDBUF     --  Set the SendBufferSize
 *            APR_SO_RCVBUF     --  Set the ReceiveBufferSize
 * </PRE>
 * @param on Value for the option.
 */
SWITCH_DECLARE(switch_status_t) switch_socket_opt_set(switch_socket_t *sock, int32_t opt, int32_t on);

/**
 * Setup socket timeout for the specified socket
 * @param sock The socket to set up.
 * @param t Value for the timeout.
 * <PRE>
 *   t > 0  -- read and write calls return APR_TIMEUP if specified time
 *             elapsess with no data read or written
 *   t == 0 -- read and write calls never block
 *   t < 0  -- read and write calls block
 * </PRE>
 */
SWITCH_DECLARE(switch_status_t) switch_socket_timeout_set(switch_socket_t *sock, switch_interval_time_t t);

/**
 * Join a Multicast Group
 * @param sock The socket to join a multicast group
 * @param join The address of the multicast group to join
 * @param iface Address of the interface to use.  If NULL is passed, the 
 *              default multicast interface will be used. (OS Dependent)
 * @param source Source Address to accept transmissions from (non-NULL 
 *               implies Source-Specific Multicast)
 */
SWITCH_DECLARE(switch_status_t) switch_mcast_join(switch_socket_t *sock, switch_sockaddr_t *join, switch_sockaddr_t *iface, switch_sockaddr_t *source);

/**
 * Set the Multicast Time to Live (ttl) for a multicast transmission.
 * @param sock The socket to set the multicast ttl
 * @param ttl Time to live to Assign. 0-255, default=1
 * @remark If the TTL is 0, packets will only be seen by sockets on the local machine,
 *     and only when multicast loopback is enabled.
 */
SWITCH_DECLARE(switch_status_t) switch_mcast_hops(switch_socket_t *sock, uint8_t ttl);

SWITCH_DECLARE(switch_status_t) switch_mcast_loopback(switch_socket_t *sock, uint8_t opt);
SWITCH_DECLARE(switch_status_t) switch_mcast_interface(switch_socket_t *sock, switch_sockaddr_t *iface);

/** @} */

	 typedef enum {
		 SWITCH_NO_DESC,		   /**< nothing here */
		 SWITCH_POLL_SOCKET,	   /**< descriptor refers to a socket */
		 SWITCH_POLL_FILE,		   /**< descriptor refers to a file */
		 SWITCH_POLL_LASTDESC	   /**< descriptor is the last one in the list */
	 } switch_pollset_type_t;

	 typedef union {
		 switch_file_t *f;		   /**< file */
		 switch_socket_t *s;	   /**< socket */
	 } switch_descriptor_t;

	 struct switch_pollfd {
		 switch_memory_pool_t *p;		  /**< associated pool */
		 switch_pollset_type_t desc_type;
									   /**< descriptor type */
		 int16_t reqevents;	/**< requested events */
		 int16_t rtnevents;	/**< returned events */
		 switch_descriptor_t desc;	 /**< @see apr_descriptor */
		 void *client_data;		/**< allows app to associate context */
	 };



/**
 * @defgroup apr_poll Poll Routines
 * @ingroup switch_apr
 * @{
 */
/** Poll descriptor set. */
	 typedef struct switch_pollfd switch_pollfd_t;

/** Opaque structure used for pollset API */
	 typedef struct apr_pollset_t switch_pollset_t;

/**
 * Poll options
 */
#define SWITCH_POLLIN 0x001			/**< Can read without blocking */
#define SWITCH_POLLPRI 0x002			/**< Priority data available */
#define SWITCH_POLLOUT 0x004			/**< Can write without blocking */
#define SWITCH_POLLERR 0x010			/**< Pending error */
#define SWITCH_POLLHUP 0x020			/**< Hangup occurred */
#define SWITCH_POLLNVAL 0x040		/**< Descriptior invalid */

/**
 * Setup a pollset object
 * @param pollset  The pointer in which to return the newly created object 
 * @param size The maximum number of descriptors that this pollset can hold
 * @param pool The pool from which to allocate the pollset
 * @param flags Optional flags to modify the operation of the pollset.
 *
 * @remark If flags equals APR_POLLSET_THREADSAFE, then a pollset is
 * created on which it is safe to make concurrent calls to
 * apr_pollset_add(), apr_pollset_remove() and apr_pollset_poll() from
 * separate threads.  This feature is only supported on some
 * platforms; the apr_pollset_create() call will fail with
 * APR_ENOTIMPL on platforms where it is not supported.
 */
SWITCH_DECLARE(switch_status_t) switch_pollset_create(switch_pollset_t ** pollset, uint32_t size, switch_memory_pool_t *pool, uint32_t flags);

/**
 * Add a socket or file descriptor to a pollset
 * @param pollset The pollset to which to add the descriptor
 * @param descriptor The descriptor to add
 * @remark If you set client_data in the descriptor, that value
 *         will be returned in the client_data field whenever this
 *         descriptor is signalled in apr_pollset_poll().
 * @remark If the pollset has been created with APR_POLLSET_THREADSAFE
 *         and thread T1 is blocked in a call to apr_pollset_poll() for
 *         this same pollset that is being modified via apr_pollset_add()
 *         in thread T2, the currently executing apr_pollset_poll() call in
 *         T1 will either: (1) automatically include the newly added descriptor
 *         in the set of descriptors it is watching or (2) return immediately
 *         with APR_EINTR.  Option (1) is recommended, but option (2) is
 *         allowed for implementations where option (1) is impossible
 *         or impractical.
 */
SWITCH_DECLARE(switch_status_t) switch_pollset_add(switch_pollset_t *pollset, const switch_pollfd_t *descriptor);

/**
 * Remove a descriptor from a pollset
 * @param pollset The pollset from which to remove the descriptor
 * @param descriptor The descriptor to remove
 * @remark If the pollset has been created with APR_POLLSET_THREADSAFE
 *         and thread T1 is blocked in a call to apr_pollset_poll() for
 *         this same pollset that is being modified via apr_pollset_remove()
 *         in thread T2, the currently executing apr_pollset_poll() call in
 *         T1 will either: (1) automatically exclude the newly added descriptor
 *         in the set of descriptors it is watching or (2) return immediately
 *         with APR_EINTR.  Option (1) is recommended, but option (2) is
 *         allowed for implementations where option (1) is impossible
 *         or impractical.
 */
SWITCH_DECLARE(switch_status_t) switch_pollset_remove(switch_pollset_t *pollset, const switch_pollfd_t *descriptor);

/**
 * Poll the sockets in the poll structure
 * @param aprset The poll structure we will be using. 
 * @param numsock The number of sockets we are polling
 * @param nsds The number of sockets signalled.
 * @param timeout The amount of time in microseconds to wait.  This is 
 *                a maximum, not a minimum.  If a socket is signalled, we 
 *                will wake up before this time.  A negative number means 
 *                wait until a socket is signalled.
 * @remark The number of sockets signalled is returned in the third argument. 
 *         This is a blocking call, and it will not return until either a 
 *         socket has been signalled, or the timeout has expired. 
 */
SWITCH_DECLARE(switch_status_t) switch_poll(switch_pollfd_t *aprset, int32_t numsock, int32_t *nsds, switch_interval_time_t timeout);

/**
 * Block for activity on the descriptor(s) in a pollset
 * @param pollset The pollset to use
 * @param timeout Timeout in microseconds
 * @param num Number of signalled descriptors (output parameter)
 * @param descriptors Array of signalled descriptors (output parameter)
 */
SWITCH_DECLARE(switch_status_t) switch_pollset_poll(switch_pollset_t *pollset, switch_interval_time_t timeout, int32_t *num, const switch_pollfd_t **descriptors);

/*!
  \brief Create a set of file descriptors to poll from a socket
  \param poll the polfd to create
  \param sock the socket to add
  \param flags the flags to modify the behaviour
  \param pool the memory pool to use
  \return SWITCH_STATUS_SUCCESS when successful
*/
SWITCH_DECLARE(switch_status_t) switch_socket_create_pollset(switch_pollfd_t ** poll, switch_socket_t *sock, int16_t flags, switch_memory_pool_t *pool);

SWITCH_DECLARE(switch_interval_time_t) switch_interval_time_from_timeval(struct timeval *tvp);
																

/*!
  \brief Create a pollfd out of a socket
  \param pollfd the pollfd to create
  \param sock the socket to add
  \param flags the flags to modify the behaviour
  \param client_data custom user data
  \param pool the memory pool to use
  \return SWITCH_STATUS_SUCCESS when successful
*/
SWITCH_DECLARE(switch_status_t) switch_socket_create_pollfd(switch_pollfd_t **pollfd, switch_socket_t *sock, int16_t flags, void *client_data, switch_memory_pool_t *pool);
SWITCH_DECLARE(switch_status_t) switch_match_glob(const char *pattern, switch_array_header_t ** result, switch_memory_pool_t *pool);
SWITCH_DECLARE(switch_status_t) switch_os_sock_get(switch_os_socket_t *thesock, switch_socket_t *sock);
SWITCH_DECLARE(switch_status_t) switch_socket_addr_get(switch_sockaddr_t ** sa, switch_bool_t remote, switch_socket_t *sock);
SWITCH_DECLARE(switch_status_t) switch_os_sock_put(switch_socket_t **sock, switch_os_socket_t *thesock, switch_memory_pool_t *pool);
/**
 * Create an anonymous pipe.
 * @param in The file descriptor to use as input to the pipe.
 * @param out The file descriptor to use as output from the pipe.
 * @param pool The pool to operate on.
 */
SWITCH_DECLARE(switch_status_t) switch_file_pipe_create(switch_file_t ** in, switch_file_t ** out, switch_memory_pool_t *pool);

/**
 * Get the timeout value for a pipe or manipulate the blocking state.
 * @param thepipe The pipe we are getting a timeout for.
 * @param timeout The current timeout value in microseconds. 
 */
SWITCH_DECLARE(switch_status_t) switch_file_pipe_timeout_get(switch_file_t *thepipe, switch_interval_time_t *timeout);

/**
 * Set the timeout value for a pipe or manipulate the blocking state.
 * @param thepipe The pipe we are setting a timeout on.
 * @param timeout The timeout value in microseconds.  Values < 0 mean wait 
 *        forever, 0 means do not wait at all.
 */
SWITCH_DECLARE(switch_status_t) switch_file_pipe_timeout_set(switch_file_t *thepipe, switch_interval_time_t timeout);


/**
 * stop the current thread
 * @param thd The thread to stop
 * @param retval The return value to pass back to any thread that cares
 */
SWITCH_DECLARE(switch_status_t) switch_thread_exit(switch_thread_t *thd, switch_status_t retval);

/**
 * block until the desired thread stops executing.
 * @param retval The return value from the dead thread.
 * @param thd The thread to join
 */
SWITCH_DECLARE(switch_status_t) switch_thread_join(switch_status_t *retval, switch_thread_t *thd);

/**
 * Return a human readable string describing the specified error.
 * @param statcode The error code the get a string for.
 * @param buf A buffer to hold the error string.
 * @bufsize Size of the buffer to hold the string.
 */

SWITCH_DECLARE(char *) switch_strerror(switch_status_t statcode, char *buf, switch_size_t bufsize);



/** @} */

SWITCH_END_EXTERN_C
#endif
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
