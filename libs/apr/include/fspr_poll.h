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

#ifndef APR_POLL_H
#define APR_POLL_H
/**
 * @file fspr_poll.h
 * @brief APR Poll interface
 */
#include "fspr.h"
#include "fspr_pools.h"
#include "fspr_errno.h"
#include "fspr_inherit.h" 
#include "fspr_file_io.h" 
#include "fspr_network_io.h" 

#if APR_HAVE_NETINET_IN_H
#include <netinet/in.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/**
 * @defgroup fspr_poll Poll Routines
 * @ingroup APR 
 * @{
 */

/**
 * Poll options
 */
#define APR_POLLIN    0x001     /**< Can read without blocking */
#define APR_POLLPRI   0x002     /**< Priority data available */
#define APR_POLLOUT   0x004     /**< Can write without blocking */
#define APR_POLLERR   0x010     /**< Pending error */
#define APR_POLLHUP   0x020     /**< Hangup occurred */
#define APR_POLLNVAL  0x040     /**< Descriptior invalid */

/**
 * Pollset Flags
 */
#define APR_POLLSET_THREADSAFE 0x001 /**< Adding or Removing a Descriptor is thread safe */

/** Used in fspr_pollfd_t to determine what the fspr_descriptor is */
typedef enum { 
    APR_NO_DESC,                /**< nothing here */
    APR_POLL_SOCKET,            /**< descriptor refers to a socket */
    APR_POLL_FILE,              /**< descriptor refers to a file */
    APR_POLL_LASTDESC           /**< descriptor is the last one in the list */
} fspr_datatype_e ;

/** Union of either an APR file or socket. */
typedef union {
    fspr_file_t *f;              /**< file */
    fspr_socket_t *s;            /**< socket */
} fspr_descriptor;

/** @see fspr_pollfd_t */
typedef struct fspr_pollfd_t fspr_pollfd_t;

/** Poll descriptor set. */
struct fspr_pollfd_t {
    fspr_pool_t *p;              /**< associated pool */
    fspr_datatype_e desc_type;   /**< descriptor type */
    fspr_int16_t reqevents;      /**< requested events */
    fspr_int16_t rtnevents;      /**< returned events */
    fspr_descriptor desc;        /**< @see fspr_descriptor */
    void *client_data;          /**< allows app to associate context */
};


/* General-purpose poll API for arbitrarily large numbers of
 * file descriptors
 */

/** Opaque structure used for pollset API */
typedef struct fspr_pollset_t fspr_pollset_t;

/**
 * Setup a pollset object
 * @param pollset  The pointer in which to return the newly created object 
 * @param size The maximum number of descriptors that this pollset can hold
 * @param p The pool from which to allocate the pollset
 * @param flags Optional flags to modify the operation of the pollset.
 *
 * @remark If flags equals APR_POLLSET_THREADSAFE, then a pollset is
 * created on which it is safe to make concurrent calls to
 * fspr_pollset_add(), fspr_pollset_remove() and fspr_pollset_poll() from
 * separate threads.  This feature is only supported on some
 * platforms; the fspr_pollset_create() call will fail with
 * APR_ENOTIMPL on platforms where it is not supported.
 */
APR_DECLARE(fspr_status_t) fspr_pollset_create(fspr_pollset_t **pollset,
                                             fspr_uint32_t size,
                                             fspr_pool_t *p,
                                             fspr_uint32_t flags);

/**
 * Destroy a pollset object
 * @param pollset The pollset to destroy
 */
APR_DECLARE(fspr_status_t) fspr_pollset_destroy(fspr_pollset_t *pollset);

/**
 * Add a socket or file descriptor to a pollset
 * @param pollset The pollset to which to add the descriptor
 * @param descriptor The descriptor to add
 * @remark If you set client_data in the descriptor, that value
 *         will be returned in the client_data field whenever this
 *         descriptor is signalled in fspr_pollset_poll().
 * @remark If the pollset has been created with APR_POLLSET_THREADSAFE
 *         and thread T1 is blocked in a call to fspr_pollset_poll() for
 *         this same pollset that is being modified via fspr_pollset_add()
 *         in thread T2, the currently executing fspr_pollset_poll() call in
 *         T1 will either: (1) automatically include the newly added descriptor
 *         in the set of descriptors it is watching or (2) return immediately
 *         with APR_EINTR.  Option (1) is recommended, but option (2) is
 *         allowed for implementations where option (1) is impossible
 *         or impractical.
 */
APR_DECLARE(fspr_status_t) fspr_pollset_add(fspr_pollset_t *pollset,
                                          const fspr_pollfd_t *descriptor);

/**
 * Remove a descriptor from a pollset
 * @param pollset The pollset from which to remove the descriptor
 * @param descriptor The descriptor to remove
 * @remark If the pollset has been created with APR_POLLSET_THREADSAFE
 *         and thread T1 is blocked in a call to fspr_pollset_poll() for
 *         this same pollset that is being modified via fspr_pollset_remove()
 *         in thread T2, the currently executing fspr_pollset_poll() call in
 *         T1 will either: (1) automatically exclude the newly added descriptor
 *         in the set of descriptors it is watching or (2) return immediately
 *         with APR_EINTR.  Option (1) is recommended, but option (2) is
 *         allowed for implementations where option (1) is impossible
 *         or impractical.
 */
APR_DECLARE(fspr_status_t) fspr_pollset_remove(fspr_pollset_t *pollset,
                                             const fspr_pollfd_t *descriptor);

/**
 * Block for activity on the descriptor(s) in a pollset
 * @param pollset The pollset to use
 * @param timeout Timeout in microseconds
 * @param num Number of signalled descriptors (output parameter)
 * @param descriptors Array of signalled descriptors (output parameter)
 */
APR_DECLARE(fspr_status_t) fspr_pollset_poll(fspr_pollset_t *pollset,
                                           fspr_interval_time_t timeout,
                                           fspr_int32_t *num,
                                           const fspr_pollfd_t **descriptors);


/**
 * Poll the descriptors in the poll structure
 * @param aprset The poll structure we will be using. 
 * @param numsock The number of descriptors we are polling
 * @param nsds The number of descriptors signalled.
 * @param timeout The amount of time in microseconds to wait.  This is 
 *                a maximum, not a minimum.  If a descriptor is signalled, we 
 *                will wake up before this time.  A negative number means 
 *                wait until a descriptor is signalled.
 * @remark The number of descriptors signalled is returned in the third argument. 
 *         This is a blocking call, and it will not return until either a 
 *         descriptor has been signalled, or the timeout has expired. 
 * @remark The rtnevents field in the fspr_pollfd_t array will only be filled-
 *         in if the return value is APR_SUCCESS.
 */
APR_DECLARE(fspr_status_t) fspr_poll(fspr_pollfd_t *aprset, fspr_int32_t numsock,
                                   fspr_int32_t *nsds, 
                                   fspr_interval_time_t timeout);

/** @} */


#ifdef __cplusplus
}
#endif

#endif  /* ! APR_POLL_H */

