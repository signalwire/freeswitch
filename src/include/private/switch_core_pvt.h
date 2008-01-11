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
 *
 * switch_core.h -- Core Library Private Data (not to be installed into the system)
 * If the last line didn't make sense, stop reading this file, go away!,
 * this file does not exist!!!!
 *
 */
#ifndef WIN32
#include <switch_private.h>
#endif

/* for apr_pool_create and apr_pool_destroy */
/* functions only used in this file so not exposed */
#include <apr_pools.h>

/* for apr_hash_make, apr_hash_pool_get, apr_hash_set */
/* functions only used in this file so not exposed */
#include <apr_hash.h>

/* for apr_pvsprintf */
/* function only used in this file so not exposed */
#include <apr_strings.h>

/* for apr_initialize and apr_terminate */
/* function only used in this file so not exposed */
#include <apr_general.h>

#include <apr_portable.h>
typedef apr_os_thread_t switch_thread_id_t;
#define switch_thread_self apr_os_thread_current


#ifdef HAVE_MLOCKALL
#include <sys/mman.h>
#endif

#ifndef WIN32
/* setuid, setgid */
#include <unistd.h>

/* getgrnam, getpwnam */
#include <pwd.h>
#include <grp.h>

#ifdef HAVE_SYS_RESOURCE_H
#include <sys/resource.h>
#endif
#endif

/* #define DEBUG_ALLOC */
#define DO_EVENTS

#define SWITCH_EVENT_QUEUE_LEN 256
#define SWITCH_MESSAGE_QUEUE_LEN 256
#define SWITCH_SQL_QUEUE_LEN 2000

#define SWITCH_BUFFER_BLOCK_FRAMES 25
#define SWITCH_BUFFER_START_FRAMES 50

typedef enum {
	SSF_NONE = 0,
	SSF_DESTROYED = (1 << 0)
} switch_session_flag_t;


struct switch_core_session {
	switch_size_t id;
	char name[80];
	switch_session_flag_t flags;
	int thread_running;
	switch_memory_pool_t *pool;
	switch_channel_t *channel;
	switch_thread_t *thread;
	const switch_endpoint_interface_t *endpoint_interface;
	switch_io_event_hooks_t event_hooks;
	switch_codec_t *read_codec;
	switch_codec_t *write_codec;

	switch_buffer_t *raw_write_buffer;
	switch_frame_t raw_write_frame;
	switch_frame_t enc_write_frame;
	uint8_t raw_write_buf[SWITCH_RECOMMENDED_BUFFER_SIZE];
	uint8_t enc_write_buf[SWITCH_RECOMMENDED_BUFFER_SIZE];

	switch_buffer_t *raw_read_buffer;
	switch_frame_t raw_read_frame;
	switch_frame_t enc_read_frame;
	uint8_t raw_read_buf[SWITCH_RECOMMENDED_BUFFER_SIZE];
	uint8_t enc_read_buf[SWITCH_RECOMMENDED_BUFFER_SIZE];


	switch_audio_resampler_t *read_resampler;
	switch_audio_resampler_t *write_resampler;

	switch_mutex_t *mutex;
	switch_mutex_t *resample_mutex;
	switch_thread_cond_t *cond;

	switch_thread_rwlock_t *rwlock;

	void *streams[SWITCH_MAX_STREAMS];
	int stream_count;

	char uuid_str[SWITCH_UUID_FORMATTED_LENGTH + 1];
	void *private_info;
	switch_queue_t *event_queue;
	switch_queue_t *message_queue;
	switch_queue_t *private_event_queue;
	switch_thread_rwlock_t *bug_rwlock;
	switch_media_bug_t *bugs;
	switch_app_log_t *app_log;
	uint32_t stack_count;
};

struct switch_media_bug {
	switch_buffer_t *raw_write_buffer;
	switch_buffer_t *raw_read_buffer;
	switch_frame_t *read_replace_frame_in;
	switch_frame_t *read_replace_frame_out;
	switch_frame_t *write_replace_frame_in;
	switch_frame_t *write_replace_frame_out;
	switch_media_bug_callback_t callback;
	switch_mutex_t *read_mutex;
	switch_mutex_t *write_mutex;
	switch_core_session_t *session;
	void *user_data;
	uint32_t flags;
	uint8_t ready;
	time_t stop_time;
	struct switch_media_bug *next;
};

struct switch_runtime {
	switch_time_t initiated;
	switch_time_t reference;
	int64_t offset;
	switch_hash_t *global_vars;
	switch_hash_t *mime_types;
	switch_memory_pool_t *memory_pool;
	const switch_state_handler_table_t *state_handlers[SWITCH_MAX_STATE_HANDLERS];
	int state_handler_index;
	FILE *console;
	uint8_t running;
	char uuid_str[SWITCH_UUID_FORMATTED_LENGTH + 1];
	uint32_t flags;
	switch_time_t timestamp;
	switch_mutex_t *throttle_mutex;
	switch_mutex_t *global_mutex;
	uint32_t sps_total;
	int32_t sps;
	int32_t sps_last;
	switch_log_level_t hard_log_level;
	char *mailer_app;
	char *mailer_app_args;
};

extern struct switch_runtime runtime;

void switch_core_sqldb_start(switch_memory_pool_t *pool);
void switch_core_sqldb_stop(void);
void switch_core_session_init(switch_memory_pool_t *pool);
void switch_core_session_uninit(void);
void switch_core_state_machine_init(switch_memory_pool_t *pool);
switch_memory_pool_t *switch_core_memory_init(void);
void switch_core_memory_stop(void);
