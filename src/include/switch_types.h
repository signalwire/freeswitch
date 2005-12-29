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
#include <switch_platform.h>

#define SWITCH_GLOBAL_VERSION "1"
#define SWITCH_MAX_CODECS 30

typedef enum {
	SWITCH_STACK_BOTTOM,
	SWITCH_STACK_TOP
} switch_stack_t;

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
	SWITCH_SIG_KILL
} switch_signal;

typedef enum {
	SWITCH_CODEC_FLAG_ENCODE =			(1 <<  0),
	SWITCH_CODEC_FLAG_DECODE =			(1 <<  1),
	SWITCH_CODEC_FLAG_SILENCE_START =	(1 <<  2),
	SWITCH_CODEC_FLAG_SILENCE_STOP =	(1 <<  3),
	SWITCH_CODEC_FLAG_SILENCE =		(1 <<  4),
	SWITCH_CODEC_FLAG_FREE_POOL =		(1 <<  5),

} switch_codec_flag;

typedef enum {
		SWITCH_TIMER_FLAG_FREE_POOL =		(1 <<  0),
} switch_timer_flag;

typedef enum {
	SWITCH_FILE_FLAG_READ =			(1 <<  0),
	SWITCH_FILE_FLAG_WRITE =		(1 <<  1),
	SWITCH_FILE_FLAG_FREE_POOL =	(1 <<  2),
	SWITCH_FILE_DATA_SHORT =		(1 <<  3),
	SWITCH_FILE_DATA_INT =			(1 <<  4),
	SWITCH_FILE_DATA_FLOAT =		(1 <<  5),
	SWITCH_FILE_DATA_DOUBLE =		(1 <<  6),
	SWITCH_FILE_DATA_RAW =			(1 <<  7),
} switch_file_flag;

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
	SWITCH_EVENT_SHUTDOWN,
	SWITCH_EVENT_ALL
} switch_event_t;



typedef struct switch_event_header switch_event_header;
typedef struct switch_event switch_event;
typedef struct switch_event_subclass switch_event_subclass;
typedef struct switch_event_node switch_event_node;
typedef void (*switch_event_callback_t)(switch_event *);
typedef struct switch_loadable_module switch_loadable_module;
typedef struct switch_frame switch_frame;
typedef struct switch_channel switch_channel;
typedef struct switch_endpoint_interface switch_endpoint_interface;
typedef struct switch_timer_interface switch_timer_interface;
typedef struct switch_dialplan_interface switch_dialplan_interface;
typedef struct switch_codec_interface switch_codec_interface;
typedef struct switch_application_interface switch_application_interface;
typedef struct switch_api_interface switch_api_interface;
typedef struct switch_file_interface switch_file_interface;
typedef struct switch_file_handle switch_file_handle;
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

/* things we don't deserve to know about */
struct switch_channel;
struct switch_core_session;


#ifdef __cplusplus
}
#endif

#endif
