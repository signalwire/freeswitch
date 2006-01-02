/* 
 * FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
 * Copyright (C) 2005/2006, Anthony Minessale II <anthmct@yahoo.com>
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
 * Anthony Minessale II <anthmct@yahoo.com>
 * Portions created by the Initial Developer are Copyright (C)
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 * 
 * Anthony Minessale II <anthmct@yahoo.com>
 *
 * switch_apr.h -- APR includes header
 *
 */
/*! \file switch_apr.h
    \brief APR includes header
*/
#ifndef SWITCH_APR_H
#define SWITCH_APR_H

#ifdef __cplusplus
extern "C" {
#endif

#include <apr.h>
#include <apr_network_io.h>
#include <apr_errno.h>
#include <apr_general.h>
#include <apr_thread_proc.h>
#include <apr_thread_mutex.h>
#include <apr_thread_cond.h>
#include <apr_thread_rwlock.h>
#include <apr_file_io.h>
#include <apr_poll.h>
#include <apr_dso.h>
#include <apr_hash.h>
#include <apr_strings.h>
#include <apr_network_io.h>
#include <apr_poll.h>
#include <apr_queue.h>
#include <apr_uuid.h>
#include <apr_strmatch.h>
#define APR_WANT_STDIO
#define APR_WANT_STRFUNC
#include <apr_want.h>

/*
   The pieces of apr we allow ppl to pass around between modules we typedef into our namespace and wrap all the functions
   any other apr code should be as hidden as possible.
*/

/**< descriptor refers to a socket */
#define SWITCH_POLL_SOCKET APR_POLL_SOCKET

/** @def SWITCH_UNSPEC
 * Let the system decide which address family to use
 */
#define SWITCH_UNSPEC APR_UNSPEC 

/**
 * Poll options
 */
#define SWITCH_POLLIN APR_POLLIN			/**< Can read without blocking */
#define SWITCH_POLLPRI APR_POLLPRI			/**< Priority data available */
#define SWITCH_POLLOUT APR_POLLOUT			/**< Can write without blocking */
#define SWITCH_POLLERR APR_POLLERR			/**< Pending error */
#define SWITCH_POLLHUP APR_POLLHUP			/**< Hangup occurred */
#define SWITCH_POLLNVAL APR_POLLNVAL		/**< Descriptior invalid */
	
/** Opaque thread-local mutex structure */
typedef apr_thread_mutex_t switch_mutex_t;

/** Abstract type for hash tables. */
typedef apr_hash_t switch_hash;

/** Abstract type for scanning hash tables. */
typedef apr_hash_index_t switch_hash_index_t;

/** The fundamental pool type */
typedef apr_pool_t switch_memory_pool;

/** number of microseconds since 00:00:00 january 1, 1970 UTC */
typedef apr_time_t switch_time_t;

/**
 * a structure similar to ANSI struct tm with the following differences:
 *  - tm_usec isn't an ANSI field
 *  - tm_gmtoff isn't an ANSI field (it's a bsdism)
 */
typedef apr_time_exp_t switch_time_exp_t;

/** Poll descriptor set. */
typedef apr_pollfd_t switch_pollfd_t;

/** Opaque structure used for pollset API */
typedef apr_pollset_t switch_pollset_t;

/** Precompiled search pattern */
typedef apr_strmatch_pattern switch_strmatch_pattern;

/** we represent a UUID as a block of 16 bytes. */
typedef apr_uuid_t switch_uuid_t;

/** Opaque structure used for queue API */
typedef apr_queue_t switch_queue_t;

/**
 * @defgroup apr_file_io File I/O Handling Functions
 * @ingroup APR 
 * @{
 */

/** Structure for referencing files. */
typedef apr_file_t switch_file_t;

/**
 * @defgroup switch_file_permissions File Permissions flags 
 * @{
 */
    
#define SWITCH_FPROT_USETID APR_FPROT_USETID		/**< Set user id */
#define SWITCH_FPROT_UREAD APR_FPROT_UREAD			/**< Read by user */
#define SWITCH_FPROT_UWRITE APR_FPROT_UWRITE		/**< Write by user */
#define SWITCH_FPROT_UEXECUTE APR_FPROT_UEXECUTE	/**< Execute by user */

#define SWITCH_FPROT_GSETID APR_FPROT_GSETID		/**< Set group id */
#define SWITCH_FPROT_GREAD APR_FPROT_GREAD			/**< Read by group */
#define SWITCH_FPROT_GWRITE APR_FPROT_GWRITE		/**< Write by group */
#define SWITCH_FPROT_GEXECUTE APR_FPROT_GEXECUTE	/**< Execute by group */

#define SWITCH_FPROT_WSETID APR_FPROT_U WSETID
#define SWITCH_FPROT_WREAD APR_FPROT_WREAD			/**< Read by others */
#define SWITCH_FPROT_WWRITE APR_FPROT_WWRITE		/**< Write by others */
#define SWITCH_FPROT_WEXECUTE APR_FPROT_WEXECUTE	/**< Execute by others */

#define SWITCH_FPROT_OS_DEFAULT APR_FPROT_OS_DEFAULT	/**< use OS's default permissions */

/* additional permission flags for apr_file_copy  and apr_file_append */
#define SWITCH_FPROT_FILE_SOURCE_PERMS APR_FPROT_FILE_SOURCE_PERMS	/**< Copy source file's permissions */
/** @} */

/**
 * @defgroup switch_file_open_flags File Open Flags/Routines
 * @{
 */
#define SWITCH_FOPEN_READ APR_FOPEN_READ							/**< Open the file for reading */
#define SWITCH_FOPEN_WRITE APR_FOPEN_WRITE							/**< Open the file for writing */
#define SWITCH_FOPEN_CREATE APR_FOPEN_CREATE						/**< Create the file if not there */
#define SWITCH_FOPEN_APPEND APR_FOPEN_APPEND						/**< Append to the end of the file */
#define SWITCH_FOPEN_TRUNCATE APR_FOPEN_TRUNCATE					/**< Open the file and truncate to 0 length */
#define SWITCH_FOPEN_BINARY APR_FOPEN_BINARY						/**< Open the file in binary mode */
#define SWITCH_FOPEN_EXCL APR_FOPEN_EXCL							/**< Open should fail if APR_CREATE and file exists. */
#define SWITCH_FOPEN_BUFFERED APR_FOPEN_BUFFERED					/**< Open the file for buffered I/O */
#define SWITCH_FOPEN_DELONCLOSE APR_FOPEN_DELONCLOSE				/**< Delete the file after close */
#define SWITCH_FOPEN_XTHREAD APR_FOPEN_XTHREAD						/**< Platform dependent tag to open the file for use across multiple threads */
#define SWITCH_FOPEN_SHARELOCK APR_FOPEN_SHARELOCK					/**< Platform dependent support for higher level locked read/write access to support writes across process/machines */
#define SWITCH_FOPEN_NOCLEANUP APR_FOPEN_NOCLEANUP					/**< Do not register a cleanup when the file is opened */
#define SWITCH_FOPEN_SENDFILE_ENABLED APR_FOPEN_SENDFILE_ENABLED	/**< Advisory flag that this file should support apr_socket_sendfile operation */
#define SWITCH_FOPEN_LARGEFILE APR_FOPEN_LAREFILE					/**< Platform dependent flag to enable large file support */

#define SWITCH_READ APR_READ				/**< @deprecated @see SWITCH_FOPEN_READ */
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
apr_status_t switch_file_open(switch_file_t **newf, const char *fname, apr_int32_t flag, switch_fileperms_t perm, switch_pool_t *pool);
#define switch_file_open apr_file_open

/**
 * Close the specified file.
 * @param file The file descriptor to close.
 */
apr_status_t switch_file_close(switch_file_t *file);
#define switch_file_close apr_file_close

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
apr_status_t switch_file_read(switch_file_t *thefile, void *buf, switch_size_t *nbytes);
#define switch_file_read apr_file_read

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
apr_status_t swich_file_write(switch_file_t *thefile, const void *buf, switch_size_t *nbytes);
#define switch_file_write apr_file_write

/** @} */

/**
 * @defgroup switch_thread_cond Condition Variable Routines
 * @ingroup APR 
 * @{
 */

/**
 * Note: destroying a condition variable (or likewise, destroying or
 * clearing the pool from which a condition variable was allocated) if
 * any threads are blocked waiting on it gives undefined results.
 */

/** Opaque structure for thread condition variables */
typedef apr_thread_cond_t switch_thread_cond_t;

/**
 * Create and initialize a condition variable that can be used to signal
 * and schedule threads in a single process.
 * @param cond the memory address where the newly created condition variable
 *        will be stored.
 * @param pool the pool from which to allocate the mutex.
 */
apr_status_t switch_thread_cond_create(switch_thread_cond_t **cond, switch_pool_t *pool);
#define switch_thread_cond_create apr_thread_cond_create

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
apr_status_t switch_thread_cond_wait(switch_thread_cond_t *cond, switch_thread_mutex_t *mutex);
#define switch_thread_cond_wait apr_thread_cond_wait

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
apr_status_t switch_thread_cond_timedwait(switch_thread_cond_t *cond, switch_thread_mutex_t *mutex, switch_interval_time_t timeout);
#define switch_thread_cond_timedwait apr_thread_cond_timedwait

/**
 * Signals a single thread, if one exists, that is blocking on the given
 * condition variable. That thread is then scheduled to wake up and acquire
 * the associated mutex. Although it is not required, if predictable scheduling
 * is desired, that mutex must be locked while calling this function.
 * @param cond the condition variable on which to produce the signal.
 */
apr_status_t switch_thread_cond_signal(switch_thread_cond_t *cond);
#define switch_thread_cond_signal apr_thread_cond_signal

/**
 * Signals all threads blocking on the given condition variable.
 * Each thread that was signaled is then scheduled to wake up and acquire
 * the associated mutex. This will happen in a serialized manner.
 * @param cond the condition variable on which to produce the broadcast.
 */
apr_status_t switch_thread_cond_broadcast(switch_thread_cond_t *cond);
#define switch_thread_cond_broadcast apr_thread_cond_broadcast

/**
 * Destroy the condition variable and free the associated memory.
 * @param cond the condition variable to destroy.
 */
apr_status_t switch_thread_cond_destroy(switch_thread_cond_t *cond);
#define switch_thread_cond_destroy apr_thread_cond_destroy

/** @} */

/**
 * @defgroup switch_thread_proc Threads and Process Functions
 * @ingroup APR 
 * @{
 */

/** Lock Flags */
typedef enum {
	SWITCH_MUTEX_DEFAULT = APR_THREAD_MUTEX_DEFAULT	/**< platform-optimal lock behavior */,
	SWITCH_MUTEX_NESTED = APR_THREAD_MUTEX_NESTED	/**< enable nested (recursive) locks */,
	SWITCH_MUTEX_UNNESTED = APR_THREAD_MUTEX_UNNESTED	/**< disable nested locks */
} switch_lock_flag;

/** Opaque Thread structure. */
typedef apr_thread_t switch_thread;

/** Opaque Thread attributes structure. */
typedef apr_threadattr_t switch_threadattr_t;

/**
 * The prototype for any APR thread worker functions.
 * typedef void *(SWITCH_THREAD_FUNC *switch_thread_start_t)(switch_thread_t*, void*);
 */
#define SWITCH_THREAD_FUNC APR_THREAD_FUNC
typedef apr_thread_start_t switch_thread_start_t;

/**
 * Create and initialize a new threadattr variable
 * @param new_attr The newly created threadattr.
 * @param cont The pool to use
 */
apr_status_t switch_threadattr_create(switch_threadattr_t **new_attr, switch_pool_t *cont);
#define switch_threadattr_create apr_threadattr_create

/**
 * Set if newly created threads should be created in detached state.
 * @param attr The threadattr to affect 
 * @param on Non-zero if detached threads should be created.
 */
apr_status_t switch_threadattr_detach_set(switch_threadattr_t *attr, switch_int32_t on);
#define switch_threadattr_detach_set apr_threadattr_detach_set

/**
 * Create a new thread of execution
 * @param new_thread The newly created thread handle.
 * @param attr The threadattr to use to determine how to create the thread
 * @param func The function to start the new thread in
 * @param data Any data to be passed to the starting function
 * @param cont The pool to use
 */
apr_status_t switch_thread_create(switch_thread_t **new_thread, switch_threadattr_t *attr, switch_thread_start_t func, void *data, switch_pool_t *cont);
#define switch_thread_create apr_thread_create

/** @} */

/**
 * @defgroup apr_network_io Network Routines
 * @ingroup APR 
 * @{
 */

/** A structure to represent sockets */
typedef apr_socket_t switch_socket_t;

/** Freeswitch's socket address type, used to ensure protocol independence */
typedef apr_sockaddr_t switch_sockaddr_t;

/* function definitions */

/**
 * Create a socket.
 * @param new_sock The new socket that has been set up.
 * @param family The address family of the socket (e.g., APR_INET).
 * @param type The type of the socket (e.g., SOCK_STREAM).
 * @param protocol The protocol of the socket (e.g., APR_PROTO_TCP).
 * @param cont The pool to use
 */
apr_status_t switch_socket_create(switch_socket_t **new_sock, int family, int type, int protocol, switch_pool_t *cont);
#define switch_socket_create apr_socket_create

/**
 * Shutdown either reading, writing, or both sides of a socket.
 * @param thesocket The socket to close 
 * @param how How to shutdown the socket.  One of:
 * <PRE>
 *            APR_SHUTDOWN_READ         no longer allow read requests
 *            APR_SHUTDOWN_WRITE        no longer allow write requests
 *            APR_SHUTDOWN_READWRITE    no longer allow read or write requests 
 * </PRE>
 * @see apr_shutdown_how_e
 * @remark This does not actually close the socket descriptor, it just
 *      controls which calls are still valid on the socket.
 */
apr_status_t switch_socket_shutdown(switch_socket_t *thesocket, switch_shutdown_how_e how);
#define switch_socket_shutdown apr_socket_shutdown

/**
 * Close a socket.
 * @param thesocket The socket to close 
 */
apr_status_t switch_socket_close(switch_socket_t *thesocket);
#define switch_socket_close apr_socket_close

/**
 * Bind the socket to its associated port
 * @param sock The socket to bind 
 * @param sa The socket address to bind to
 * @remark This may be where we will find out if there is any other process
 *      using the selected port.
 */
apr_status_t switch_socket_bind(switch_socket_t *sock, switch_sockaddr_t *sa);
#define switch_socket_bind apr_socket_bind

/**
 * Listen to a bound socket for connections.
 * @param sock The socket to listen on 
 * @param backlog The number of outstanding connections allowed in the sockets
 *                listen queue.  If this value is less than zero, the listen
 *                queue size is set to zero.  
 */
apr_status_t switch_socket_listen(switch_socket_t *sock, switch_int32_t backlog);
#define switch_socket_listen apr_socket_listen

/**
 * Accept a new connection request
 * @param new_sock A copy of the socket that is connected to the socket that
 *                 made the connection request.  This is the socket which should
 *                 be used for all future communication.
 * @param sock The socket we are listening on.
 * @param connection_pool The pool for the new socket.
 */
apr_status_t switch_socket_accept(switch_socket_t **new_sock, switch_socket_t *sock, switch_pool_t *connection_pool);
#define switch_socket_accept apr_socket_accept

/**
 * Issue a connection request to a socket either on the same machine 
 * or a different one.
 * @param sock The socket we wish to use for our side of the connection 
 * @param sa The address of the machine we wish to connect to.
 */
apr_status_t switch_socket_connect(switch_socket_t *sock, switch_sockaddr_t *sa);
#define switch_socket_connect apr_socket_connect

/**
 * Create apr_sockaddr_t from hostname, address family, and port.
 * @param sa The new apr_sockaddr_t.
 * @param hostname The hostname or numeric address string to resolve/parse, or
 *               NULL to build an address that corresponds to 0.0.0.0 or ::
 * @param family The address family to use, or APR_UNSPEC if the system should 
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
 * @param p The pool for the apr_sockaddr_t and associated storage.
 */
apr_status_t switch_sockaddr_info_get(switch_sockaddr_t **sa,
										const char *hostname,
										switch_int32_t family,
										switch_port_t port,
										switch_int32_t flags,
										switch_pool_t *p);
#define switch_sockaddr_info_get apr_sockaddr_info_get

/**
 * Look up the host name from an apr_sockaddr_t.
 * @param hostname The hostname.
 * @param sa The apr_sockaddr_t.
 * @param flags Special processing flags.
 */
apr_status_t switch_getnameinfo(char **hostname,
									switch_sockaddr_t *sa,
									switch_int32_t flags);
#define switch_getnameinfo apr_getnameinfo

/**
 * Parse hostname/IP address with scope id and port.
 *
 * Any of the following strings are accepted:
 *   8080                  (just the port number)
 *   www.apache.org        (just the hostname)
 *   www.apache.org:8080   (hostname and port number)
 *   [fe80::1]:80          (IPv6 numeric address string only)
 *   [fe80::1%eth0]        (IPv6 numeric address string and scope id)
 *
 * Invalid strings:
 *                         (empty string)
 *   [abc]                 (not valid IPv6 numeric address string)
 *   abc:65536             (invalid port number)
 *
 * @param addr The new buffer containing just the hostname.  On output, *addr 
 *             will be NULL if no hostname/IP address was specfied.
 * @param scope_id The new buffer containing just the scope id.  On output, 
 *                 *scope_id will be NULL if no scope id was specified.
 * @param port The port number.  On output, *port will be 0 if no port was 
 *             specified.
 *             ### FIXME: 0 is a legal port (per RFC 1700). this should
 *             ### return something besides zero if the port is missing.
 * @param str The input string to be parsed.
 * @param p The pool from which *addr and *scope_id are allocated.
 * @remark If scope id shouldn't be allowed, check for scope_id != NULL in 
 *         addition to checking the return code.  If addr/hostname should be 
 *         required, check for addr == NULL in addition to checking the 
 *         return code.
 */
apr_status_t switch_parse_addr_port(char **addr,
									char **scope_id,
									switch_port_t *port,
									const char *str,
									switch_pool_t *p);
#define switch_parse_addr_port apr_parse_addr_port

/**
 * Get name of the current machine
 * @param buf A buffer to store the hostname in.
 * @param len The maximum length of the hostname that can be stored in the
 *            buffer provided.  The suggested length is APRMAXHOSTLEN + 1.
 * @param cont The pool to use.
 * @remark If the buffer was not large enough, an error will be returned.
 */
apr_status_t switch_gethostname(char *buf, int len, switch_pool_t *cont);
#define switch_gethostname apr_gethostname

/**
 * Return the data associated with the current socket
 * @param data The user data associated with the socket.
 * @param key The key to associate with the user data.
 * @param sock The currently open socket.
 */
apr_status_t switch_socket_data_get(void **data, const char *key,
										switch_socket_t *sock);
#define switch_socket_data_get apr_socket_data_get

/**
 * Set the data associated with the current socket.
 * @param sock The currently open socket.
 * @param data The user data to associate with the socket.
 * @param key The key to associate with the data.
 * @param cleanup The cleanup to call when the socket is destroyed.
 */
apr_status_t switch_socket_data_set(switch_socket_t *sock, void *data,
											const char *key,
											switch_status_t (*cleanup)(void*));
#define switch_socket_data_set apr_socket_data_set

#define switch_socket_send apr_socket_send
#define switch_socket_sendv apr_socket_sendv
#define switch_socket_sendto apr_socket_sendto
#define switch_socket_recvfrom apr_socket_recvfrom
#define switch_socket_sendfile apr_socket_sendfile
#define switch_socket_recv apr_socket_recv
#define switch_socket_opt_set apr_socket_opt_set
#define switch_socket_timeout_set apr_socket_timeout_set
#define switch_socket_opt_get apr_socket_opt_get
#define switch_socket_timeout_get apr_socket_timeout_get
#define switch_socket_atmark apr_socket_atmark
#define switch_socket_addr_get apr_socket_addr_get
#define switch_sockaddr_ip_get apr_sockaddr_ip_get
#define switch_sockaddr_equal apr_sockaddr_equal
#define switch_socket_type_get apr_socket_type_get
#define switch_getservbyname apr_getservbyname
#define switch_ipsubnet_create apr_ipsubnet_create
#define switch_ipsubnet_test apr_ipsubnet_test
#define switch_socket_protocol_get apr_socket_protocol_get
#define switch_mcast_join apr_mcast_join
#define switch_mcast_leave apr_mcast_leave
#define switch_mcast_hops apr_mcast_hops
#define switch_mcast_loopback apr_mcast_loopback
#define switch_mcast_interface apr_mcast_interface

/** @} */

#define switch_pool_clear apr_pool_clear
#define switch_strmatch_precompile apr_strmatch_precompile
#define switch_strmatch apr_strmatch
#define switch_uuid_format apr_uuid_format
#define switch_uuid_get apr_uuid_get
#define switch_uuid_parse apr_uuid_parse
#define switch_queue_create apr_queue_create
#define switch_queue_interrupt_all apr_queue_interrupt_all
#define switch_queue_pop apr_queue_pop
#define switch_queue_push apr_queue_push
#define switch_queue_size apr_queue_size
#define switch_queue_term apr_queue_term
#define switch_queue_trypop apr_queue_trypop
#define switch_queue_trypush apr_queue_trypush
#define switch_poll_setup apr_poll_setup
#define switch_pollset_create apr_pollset_create
#define switch_pollset_add apr_pollset_add
#define switch_poll apr_poll
#define switch_time_now apr_time_now
#define switch_strftime apr_strftime
#define switch_rfc822_date apr_rfc822_date
#define switch_time_exp_gmt apr_time_exp_gmt
#define switch_time_exp_get apr_time_exp_get
#define switch_time_exp_lt apr_time_exp_lt
#define switch_sleep apr_sleep
#define switch_hash_first apr_hash_first
#define switch_hash_next apr_hash_next
#define switch_hash_this apr_hash_this




#ifdef __cplusplus
}
#endif

#endif
