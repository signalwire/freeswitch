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

typedef enum {
	SWITCH_MUTEX_DEFAULT = APR_THREAD_MUTEX_DEFAULT	/**< platform-optimal lock behavior */,
	SWITCH_MUTEX_NESTED = APR_THREAD_MUTEX_NESTED	/**< enable nested (recursive) locks */,
	SWITCH_MUTEX_UNNESTED = APR_THREAD_MUTEX_UNNESTED	/**< disable nested locks */
} switch_lock_flag;

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

	
/** Opaque Thread structure. */
typedef apr_thread_t switch_thread;

/** Opaque Thread attributes structure. */
typedef apr_threadattr_t switch_threadattr_t;

/** Opaque thread-local mutex structure */
typedef apr_thread_mutex_t switch_mutex_t;

/**
 * /fn typedef void *(SWITCH_THREAD_FUNC *switch_thread_start_t)(switch_thread_t*, void*);
 * The prototype for any APR thread worker functions.
 */
#define SWITCH_THREAD_FUNC APR_THREAD_FUNC
typedef apr_thread_start_t switch_thread_start_t;

/** Opaque structure for thread condition variables */
typedef apr_thread_cond_t switch_thread_cond_t;

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

/** Freeswitch's socket address type, used to ensure protocol independence */
typedef apr_sockaddr_t switch_sockaddr_t;

/** A structure to represent sockets */
typedef apr_socket_t switch_socket_t;

/** Poll descriptor set. */
typedef apr_pollfd_t switch_pollfd_t;

/** Opaque structure used for pollset API */
typedef apr_pollset_t switch_pollset_t;

/** Structure for referencing files. */
typedef apr_file_t switch_file_t;

/** Precompiled search pattern */
typedef apr_strmatch_pattern switch_strmatch_pattern;

/** we represent a UUID as a block of 16 bytes. */
typedef apr_uuid_t switch_uuid_t;

/** Opaque structure used for queue API */
typedef apr_queue_t switch_queue_t;


#define switch_thread_cond_create apr_thread_cond_create
#define switch_thread_cond_wait apr_thread_cond_wait
#define switch_thread_cond_timedwait apr_thread_cond_timedwait
#define switch_thread_cond_signal apr_thread_cond_signal
#define switch_thread_cond_broadcast apr_thread_cond_broadcast
#define switch_thread_cond_destroy apr_thread_cond_destroy
#define switch_threadattr_create apr_threadattr_create
#define switch_threadattr_detach_set apr_threadattr_detach_set

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
#define switch_thread_create apr_thread_create
#define switch_sleep apr_sleep
#define switch_socket_create apr_socket_create
#define switch_socket_shutdown apr_socket_shutdown
#define switch_socket_close apr_socket_close
#define switch_socket_bind apr_socket_bind
#define switch_socket_listen apr_socket_listen
#define switch_socket_accept apr_socket_accept
#define switch_socket_connect apr_socket_connect
#define switch_sockaddr_info_get apr_sockaddr_info_get
#define switch_getnameinfo apr_getnameinfo
#define switch_parse_addr_port apr_parse_addr_port
#define switch_gethostname apr_gethostname
#define switch_socket_data_get apr_socket_data_get
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
#define switch_file_open apr_file_open
#define switch_file_close apr_file_close
#define switch_file_read apr_file_read
#define switch_file_write apr_file_write
#define switch_hash_first apr_hash_first
#define switch_hash_next apr_hash_next
#define switch_hash_this apr_hash_this




#ifdef __cplusplus
}
#endif

#endif
