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
 * switch_types.h -- Data Types
 *
 */
#ifndef SWITCH_TYPES_H
#define SWITCH_TYPES_H

#ifdef __cplusplus
extern "C" {
#endif

#include <switch.h>

#define SWITCH_GLOBAL_VERSION "1"
#define SWITCH_MAX_CODECS 30

typedef enum {
	SWITCH_STATUS_SUCCESS,
	SWITCH_STATUS_FALSE,
	SWITCH_STATUS_TIMEOUT,
	SWITCH_STATUS_RESTART,
	SWITCH_STATUS_TERM,
	SWITCH_STATUS_NOTIMPL,
	SWITCH_STATUS_MEMERR,
	SWITCH_STATUS_NOOP,
	SWITCH_STATUS_GENERR,
	SWITCH_STATUS_INUSE
} switch_status;

typedef enum {
	SWITCH_CHANNEL_ID_CONSOLE,
	SWITCH_CHANNEL_ID_CONSOLE_CLEAN,
	SWITCH_CHANNEL_ID_EVENT
} switch_text_channel;

#if defined(_MSC_VER) && _MSC_VER < 1300
#ifndef __FUNCTION__
#define __FUNCTION__ ""
#endif
#endif

#define SWITCH_UUID_FORMATTED_LENGTH APR_UUID_FORMATTED_LENGTH 	
#define SWITCH_CHANNEL_CONSOLE SWITCH_CHANNEL_ID_CONSOLE, __FILE__, __FUNCTION__, __LINE__
#define SWITCH_CHANNEL_CONSOLE_CLEAN SWITCH_CHANNEL_ID_CONSOLE_CLEAN, __FILE__, __FUNCTION__, __LINE__
#define SWITCH_CHANNEL_EVENT SWITCH_CHANNEL_ID_EVENT, __FILE__, __FUNCTION__, __LINE__

typedef enum {
	CS_NEW,
	CS_INIT,
	CS_RING,
	CS_TRANSMIT,
	CS_EXECUTE,
	CS_LOOPBACK,
	CS_HANGUP,
	CS_DONE
} switch_channel_state;

typedef enum {
	CF_SEND_AUDIO = (1 <<  0),
	CF_RECV_AUDIO = (1 <<  1),
	CF_ANSWERED   = (1 <<  2),
	CF_OUTBOUND   = (1 <<  3),
} switch_channel_flag;

typedef enum {
	SWITCH_MUTEX_DEFAULT = APR_THREAD_MUTEX_DEFAULT,
	SWITCH_MUTEX_NESTED = APR_THREAD_MUTEX_NESTED,
	SWITCH_MUTEX_UNNESTED = APR_THREAD_MUTEX_UNNESTED
} switch_lock_flag;

typedef enum {
	SWITCH_SIG_KILL
} switch_signal;

typedef enum {
	SWITCH_CODEC_FLAG_ENCODE =			(1 <<  0),
	SWITCH_CODEC_FLAG_DECODE =			(1 <<  1),
	SWITCH_CODEC_FLAG_SILENCE_START =	(1 <<  2),
	SWITCH_CODEC_FLAG_SILENCE_STOP =	(1 <<  3),
	SWITCH_CODEC_FLAG_SILENCE =		(1 <<  4),

} switch_codec_flag;

typedef enum {
	SWITCH_CODEC_TYPE_AUDIO,
	SWITCH_CODEC_TYPE_VIDEO,
	SWITCH_CODEC_TYPE_T38,
	SWITCH_CODEC_TYPE_APP
} switch_codec_type;

typedef enum {
	SWITCH_IO_FLAG_NOOP = 0,
} switch_io_flag;

/* make sure this is synced with the EVENT_NAMES array in switch_event.c
   also never put any new ones before EVENT_ALL
*/
typedef enum {
	SWITCH_EVENT_CUSTOM,
	SWITCH_EVENT_CHANNEL_STATE,
	SWITCH_EVENT_CHANNEL_ANSWER,
	SWITCH_EVENT_API,
	SWITCH_EVENT_LOG,
	SWITCH_EVENT_INBOUND_CHAN,
	SWITCH_EVENT_OUTBOUND_CHAN,
	SWITCH_EVENT_ANSWER_CHAN,
	SWITCH_EVENT_HANGUP_CHAN,
	SWITCH_EVENT_STARTUP,
	SWITCH_EVENT_EVENT_SHUTDOWN,
	SWITCH_EVENT_SHUTDOWN,
	SWITCH_EVENT_ALL
} switch_event_t;



typedef struct switch_event_header switch_event_header;
typedef struct switch_event switch_event;
typedef struct switch_event_subclass switch_event_subclass;
typedef struct switch_event_node switch_event_node;
typedef void (*switch_event_callback_t)(switch_event *);
typedef apr_threadattr_t switch_threadattr_t;
typedef struct switch_loadable_module switch_loadable_module;
typedef struct switch_frame switch_frame;
typedef struct switch_channel switch_channel;
typedef struct switch_endpoint_interface switch_endpoint_interface;
typedef struct switch_timer_interface switch_timer_interface;
typedef struct switch_dialplan_interface switch_dialplan_interface;
typedef struct switch_codec_interface switch_codec_interface;
typedef struct switch_application_interface switch_application_interface;
typedef struct switch_api_interface switch_api_interface;
typedef struct switch_core_session switch_core_session;
typedef struct switch_loadable_module_interface switch_loadable_module_interface;
typedef struct switch_caller_profile switch_caller_profile;
typedef struct switch_caller_step switch_caller_step;
typedef struct switch_caller_extension switch_caller_extension;
typedef struct switch_caller_application switch_caller_application;
typedef struct switch_event_handler_table switch_event_handler_table;
typedef struct switch_timer switch_timer;
typedef struct switch_codec switch_codec;
typedef struct switch_core_thread_session switch_core_thread_session;
typedef struct switch_codec_implementation switch_codec_implementation;
typedef struct switch_io_event_hook_outgoing_channel switch_io_event_hook_outgoing_channel;
typedef struct switch_io_event_hook_answer_channel switch_io_event_hook_answer_channel;
typedef struct switch_io_event_hook_read_frame switch_io_event_hook_read_frame;
typedef struct switch_io_event_hook_write_frame switch_io_event_hook_write_frame;
typedef struct switch_io_event_hook_kill_channel switch_io_event_hook_kill_channel;
typedef struct switch_io_event_hook_waitfor_read switch_io_event_hook_waitfor_read;
typedef struct switch_io_event_hook_waitfor_write switch_io_event_hook_waitfor_write;
typedef struct switch_io_event_hook_send_dtmf switch_io_event_hook_send_dtmf;
typedef struct switch_io_routines switch_io_routines;
typedef struct switch_io_event_hooks switch_io_event_hooks;
typedef struct switch_buffer switch_buffer;
typedef struct switch_codec_settings switch_codec_settings;
typedef void (*switch_application_function)(switch_core_session *, char *);
typedef switch_caller_extension *(*switch_dialplan_hunt_function)(switch_core_session *);
typedef switch_status (*switch_event_handler)(switch_core_session *);
typedef switch_status (*switch_outgoing_channel_hook)(switch_core_session *, switch_caller_profile *, switch_core_session *);
typedef switch_status (*switch_answer_channel_hook)(switch_core_session *);
typedef switch_status (*switch_read_frame_hook)(switch_core_session *, switch_frame **, int, switch_io_flag);
typedef switch_status (*switch_write_frame_hook)(switch_core_session *, switch_frame *, int, switch_io_flag);
typedef switch_status (*switch_kill_channel_hook)(switch_core_session *, int);
typedef switch_status (*switch_waitfor_read_hook)(switch_core_session *, int);
typedef switch_status (*switch_waitfor_write_hook)(switch_core_session *, int);
typedef switch_status (*switch_send_dtmf_hook)(switch_core_session *, char *);
typedef switch_status (*switch_api_function)(char *in, char *out, size_t outlen);

/*
   The pieces of apr we allow ppl to pass around between modules we typedef into our namespace and wrap all the functions
   any other apr code should be as hidden as possible.
*/
typedef apr_strmatch_pattern switch_strmatch_pattern;
typedef apr_uuid_t switch_uuid_t;
typedef apr_queue_t switch_queue_t;
typedef apr_hash_t switch_hash;
typedef apr_pool_t switch_memory_pool;
typedef apr_thread_t switch_thread;
typedef apr_thread_mutex_t switch_mutex_t;
typedef apr_time_t switch_time_t;
typedef apr_time_exp_t switch_time_exp_t;
typedef apr_thread_start_t switch_thread_start_t;
typedef apr_sockaddr_t switch_sockaddr_t;
typedef apr_socket_t switch_socket_t;
typedef apr_pollfd_t switch_pollfd_t;
typedef apr_pollset_t switch_pollset_t;
typedef apr_file_t switch_file_t;
typedef apr_thread_cond_t switch_thread_cond_t;
typedef apr_hash_index_t switch_hash_index_t;

#define SWITCH_UNSPEC APR_UNSPEC 
#define SWITCH_POLLIN APR_POLLIN
#define SWITCH_POLLPRI APR_POLLPRI
#define SWITCH_POLLOUT APR_POLLOUT
#define SWITCH_POLLERR APR_POLLERR
#define SWITCH_POLLHUP APR_POLLHUP
#define SWITCH_POLLNVAL APR_POLLNVAL
#define SWITCH_READ APR_READ 
#define SWITCH_FPROT_UREAD APR_FPROT_UREAD
#define SWITCH_FPROT_GREAD APR_FPROT_GREAD

#define switch_thread_cond_create apr_thread_cond_create
#define switch_thread_cond_wait apr_thread_cond_wait
#define switch_thread_cond_timedwait apr_thread_cond_timedwait
#define switch_thread_cond_signal apr_thread_cond_signal
#define switch_thread_cond_broadcast apr_thread_cond_broadcast
#define switch_thread_cond_destroy apr_thread_cond_destroy

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
#define SWITCH_POLL_SOCKET APR_POLL_SOCKET
#define SWITCH_THREAD_FUNC APR_THREAD_FUNC
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
#define switch_threadattr_create apr_threadattr_create
#define switch_threadattr_detach_set apr_threadattr_detach_set

#define SWITCH_FOPEN_READ APR_FOPEN_READ
#define SWITCH_FOPEN_WRITE APR_FOPEN_WRITE
#define SWITCH_FOPEN_CREATE APR_FOPEN_CREATE
#define SWITCH_FOPEN_APPEND APR_FOPEN_APPEND
#define SWITCH_FOPEN_TRUNCATE APR_FOPEN_TRUNCATE
#define SWITCH_FOPEN_BINARY APR_FOPEN_BINARY
#define SWITCH_FOPEN_EXCL APR_FOPEN_EXCL
#define SWITCH_FOPEN_BUFFERED APR_FOPEN_BUFFERED
#define SWITCH_FOPEN_DELONCLOSE APR_FOPEN_DELONCLOSE
#define SWITCH_FOPEN_XTHREAD APR_FOPEN_XTHREAD
#define SWITCH_FOPEN_SHARELOCK APR_FOPEN_SHARELOCK
#define SWITCH_FOPEN_NOCLEANUP APR_FOPEN_NOCLEANUP
#define SWITCH_FOPEN_SENDFILE_ENABLED APR_FOPEN_SENDFILE_ENABLED
#define SWITCH_FOPEN_LARGEFILE APR_FOPEN_LARGEFILE



#define SWITCH_FPROT_USETID APR_FPROT_USETID
#define SWITCH_FPROT_UREAD APR_FPROT_UREAD
#define SWITCH_FPROT_UWRITE APR_FPROT_UWRITE
#define SWITCH_FPROT_UEXECUTE APR_FPROT_UEXECUTE

#define SWITCH_FPROT_GSETID APR_FPROT_GSETID
#define SWITCH_FPROT_GREAD APR_FPROT_GREAD
#define SWITCH_FPROT_GWRITE APR_FPROT_GWRITE
#define SWITCH_FPROT_GEXECUTE APR_FPROT_GEXECUTE

#define SWITCH_FPROT_WSETID APR_FPROT_U WSETID
#define SWITCH_FPROT_WREAD APR_FPROT_WREAD
#define SWITCH_FPROT_WWRITE APR_FPROT_WWRITE
#define SWITCH_FPROT_WEXECUTE APR_FPROT_WEXECUTE

#define SWITCH_FPROT_OS_DEFAULT APR_FPROT_OS_DEFAULT
#define SWITCH_FPROT_FILE_SOURCE_PERMS APR_FPROT_FILE_SOURCE_PERMS

/* SQLITE */
typedef sqlite3 switch_core_db;
#define switch_core_db_aggregate_context sqlite3_aggregate_context
#define switch_core_db_aggregate_count sqlite3_aggregate_count
#define switch_core_db_bind_blob sqlite3_bind_blob
#define switch_core_db_bind_double sqlite3_bind_double
#define switch_core_db_bind_int sqlite3_bind_int
#define switch_core_db_bind_int64 sqlite3_bind_int64
#define switch_core_db_bind_null sqlite3_bind_null
#define switch_core_db_bind_parameter_count sqlite3_bind_parameter_count
#define switch_core_db_bind_parameter_index sqlite3_bind_parameter_index
#define switch_core_db_bind_parameter_name sqlite3_bind_parameter_name
#define switch_core_db_bind_text sqlite3_bind_text
#define switch_core_db_bind_text16 sqlite3_bind_text16
#define switch_core_db_btree_trace sqlite3_btree_trace
#define switch_core_db_busy_handler sqlite3_busy_handler
#define switch_core_db_busy_timeout sqlite3_busy_timeout
#define switch_core_db_changes sqlite3_changes
#define switch_core_db_close sqlite3_close
#define switch_core_db_collation_needed sqlite3_collation_needed
#define switch_core_db_collation_needed16 sqlite3_collation_needed16
#define switch_core_db_column_blob sqlite3_column_blob
#define switch_core_db_column_bytes sqlite3_column_bytes
#define switch_core_db_column_bytes16 sqlite3_column_bytes16
#define switch_core_db_column_count sqlite3_column_count
#define switch_core_db_column_decltype sqlite3_column_decltype
#define switch_core_db_column_decltype16 sqlite3_column_decltype16
#define switch_core_db_column_double sqlite3_column_double
#define switch_core_db_column_int sqlite3_column_int
#define switch_core_db_column_int64 sqlite3_column_int64
#define switch_core_db_column_name sqlite3_column_name
#define switch_core_db_column_name16 sqlite3_column_name16
#define switch_core_db_column_text sqlite3_column_text
#define switch_core_db_column_text16 sqlite3_column_text16
#define switch_core_db_column_type sqlite3_column_type
#define switch_core_db_commit_hook sqlite3_commit_hook
#define switch_core_db_complete sqlite3_complete
#define switch_core_db_complete16 sqlite3_complete16
#define switch_core_db_create_collation sqlite3_create_collation
#define switch_core_db_create_collation16 sqlite3_create_collation16
#define switch_core_db_create_function sqlite3_create_function
#define switch_core_db_create_function16 sqlite3_create_function16
#define switch_core_db_data_count sqlite3_data_count
#define switch_core_db_db_handle sqlite3_db_handle
#define switch_core_db_errcode sqlite3_errcode
#define switch_core_db_errmsg sqlite3_errmsg
#define switch_core_db_errmsg16 sqlite3_errmsg16
#define switch_core_db_exec sqlite3_exec
#define switch_core_db_expired sqlite3_expired
#define switch_core_db_finalize sqlite3_finalize
#define switch_core_db_free sqlite3_free
#define switch_core_db_free_table sqlite3_free_table
#define switch_core_db_get_autocommit sqlite3_get_autocommit
#define switch_core_db_get_auxdata sqlite3_get_auxdata
#define switch_core_db_get_table sqlite3_get_table
#define switch_core_db_get_table_cb sqlite3_get_table_cb
#define switch_core_db_global_recover sqlite3_global_recover
#define switch_core_db_interrupt sqlite3_interrupt
#define switch_core_db_interrupt_count sqlite3_interrupt_count
#define switch_core_db_last_insert_rowid sqlite3_last_insert_rowid
#define switch_core_db_libversion sqlite3_libversion
#define switch_core_db_libversion_number sqlite3_libversion_number
#define switch_core_db_malloc_failed sqlite3_malloc_failed
#define switch_core_db_mprintf sqlite3_mprintf
#define switch_core_db_open sqlite3_open
#define switch_core_db_open16 sqlite3_open16
#define switch_core_db_opentemp_count sqlite3_opentemp_count
#define switch_core_db_os_trace sqlite3_os_trace
#define switch_core_db_prepare sqlite3_prepare
#define switch_core_db_prepare16 sqlite3_prepare16
#define switch_core_db_profile sqlite3_profile
#define switch_core_db_progress_handler sqlite3_progress_handler
#define switch_core_db_reset sqlite3_reset
#define switch_core_db_result_blob sqlite3_result_blob
#define switch_core_db_result_double sqlite3_result_double
#define switch_core_db_result_error sqlite3_result_error
#define switch_core_db_result_error16 sqlite3_result_error16
#define switch_core_db_result_int sqlite3_result_int
#define switch_core_db_result_int64 sqlite3_result_int64
#define switch_core_db_result_null sqlite3_result_null
#define switch_core_db_result_text sqlite3_result_text
#define switch_core_db_result_text16 sqlite3_result_text16
#define switch_core_db_result_text16be sqlite3_result_text16be
#define switch_core_db_result_text16le sqlite3_result_text16le
#define switch_core_db_result_value sqlite3_result_value
#define switch_core_db_search_count sqlite3_search_count
#define switch_core_db_set_authorizer sqlite3_set_authorizer
#define switch_core_db_set_auxdata sqlite3_set_auxdata
#define switch_core_db_snprintf sqlite3_snprintf
#define switch_core_db_sort_count sqlite3_sort_count
#define switch_core_db_step sqlite3_step
#define switch_core_db_temp_directory sqlite3_temp_directory
#define switch_core_db_total_changes sqlite3_total_changes
#define switch_core_db_trace sqlite3_trace
#define switch_core_db_transfer_bindings sqlite3_transfer_bindings
#define switch_core_db_user_data sqlite3_user_data
#define switch_core_db_value_blob sqlite3_value_blob
#define switch_core_db_value_bytes sqlite3_value_bytes
#define switch_core_db_value_bytes16 sqlite3_value_bytes16
#define switch_core_db_value_double sqlite3_value_double
#define switch_core_db_value_int sqlite3_value_int
#define switch_core_db_value_int64 sqlite3_value_int64
#define switch_core_db_value_text sqlite3_value_text
#define switch_core_db_value_text16 sqlite3_value_text16
#define switch_core_db_value_text16be sqlite3_value_text16be
#define switch_core_db_value_text16le sqlite3_value_text16le
#define switch_core_db_value_type sqlite3_value_type
#define switch_core_db_version sqlite3_version
#define switch_core_db_vmprintf sqlite3_vmprintf


/* things we don't deserve to know about */
struct switch_channel;
struct switch_core_session;

#ifndef uint32_t
#ifdef WIN32
typedef unsigned __int8		uint8_t;
typedef unsigned __int16	uint16_t;
typedef unsigned __int32	uint32_t;
typedef unsigned __int64    uint64_t;
typedef __int8		int8_t;
typedef __int16		int16_t;
typedef __int32		int32_t;
typedef __int64		int64_t;
typedef unsigned long	in_addr_t;
#else
#include <sys/types.h>
#endif
#endif

#ifdef __cplusplus
}
#endif

#endif
