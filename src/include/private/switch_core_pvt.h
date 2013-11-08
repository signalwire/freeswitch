/* 
 * FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
 * Copyright (C) 2005-2012, Anthony Minessale II <anthm@freeswitch.org>
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
 *
 *
 * switch_core.h -- Core Library Private Data (not to be installed into the system)
 * If the last line didn't make sense, stop reading this file, go away!,
 * this file does not exist!!!!
 *
 */
#define SPANDSP_NO_TIFF 1
#include "spandsp.h"
#include "switch_profile.h"

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

#ifdef HAVE_MLOCKALL
#include <sys/mman.h>
#endif

#ifdef HAVE_NETDB_H
#include <netdb.h>
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

#define SWITCH_BUFFER_BLOCK_FRAMES 25
#define SWITCH_BUFFER_START_FRAMES 50

typedef enum {
	SSF_NONE = 0,
	SSF_DESTROYED = (1 << 0),
	SSF_WARN_TRANSCODE = (1 << 1),
	SSF_HANGUP = (1 << 2),
	SSF_THREAD_STARTED = (1 << 3),
	SSF_THREAD_RUNNING = (1 << 4),
	SSF_READ_TRANSCODE = (1 << 5),
	SSF_WRITE_TRANSCODE = (1 << 6),
	SSF_READ_CODEC_RESET = (1 << 7),
	SSF_WRITE_CODEC_RESET = (1 << 8),
	SSF_DESTROYABLE = (1 << 9),
	SSF_MEDIA_BUG_TAP_ONLY = (1 << 10)
} switch_session_flag_t;


struct switch_core_session {
	switch_memory_pool_t *pool;
	switch_thread_t *thread;
	switch_thread_id_t thread_id;
	switch_endpoint_interface_t *endpoint_interface;
	switch_size_t id;
	switch_session_flag_t flags;
	switch_channel_t *channel;

	switch_io_event_hooks_t event_hooks;
	switch_codec_t *read_codec;
	switch_codec_t *real_read_codec;
	switch_codec_t *write_codec;
	switch_codec_t *real_write_codec;
	switch_codec_t *video_read_codec;
	switch_codec_t *video_write_codec;

	switch_codec_implementation_t read_impl;
	switch_codec_implementation_t real_read_impl;
	switch_codec_implementation_t write_impl;
	switch_codec_implementation_t video_read_impl;
	switch_codec_implementation_t video_write_impl;

	switch_audio_resampler_t *read_resampler;
	switch_audio_resampler_t *write_resampler;

	switch_mutex_t *mutex;
	switch_mutex_t *resample_mutex;
	switch_mutex_t *codec_read_mutex;
	switch_mutex_t *codec_write_mutex;
	switch_thread_cond_t *cond;
	switch_mutex_t *frame_read_mutex;

	switch_thread_rwlock_t *rwlock;
	switch_thread_rwlock_t *io_rwlock;

	void *streams[SWITCH_MAX_STREAMS];
	int stream_count;

	char uuid_str[SWITCH_UUID_FORMATTED_LENGTH + 1];
	void *private_info[SWITCH_CORE_SESSION_MAX_PRIVATES];
	switch_queue_t *event_queue;
	switch_queue_t *message_queue;
	switch_queue_t *signal_data_queue;
	switch_queue_t *private_event_queue;
	switch_queue_t *private_event_queue_pri;
	switch_thread_rwlock_t *bug_rwlock;
	switch_media_bug_t *bugs;
	switch_app_log_t *app_log;
	uint32_t stack_count;

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
	switch_codec_t bug_codec;
	uint32_t read_frame_count;
	uint32_t track_duration;
	uint32_t track_id;
	switch_log_level_t loglevel;
	uint32_t soft_lock;
	switch_ivr_dmachine_t *dmachine[2];
	plc_state_t *plc;
	uint8_t recur_buffer[SWITCH_RECOMMENDED_BUFFER_SIZE];
	switch_size_t recur_buffer_len;

	switch_media_handle_t *media_handle;
	uint32_t decoder_errors;
};

struct switch_media_bug {
	switch_buffer_t *raw_write_buffer;
	switch_buffer_t *raw_read_buffer;
	switch_frame_t *read_replace_frame_in;
	switch_frame_t *read_replace_frame_out;
	switch_frame_t *write_replace_frame_in;
	switch_frame_t *write_replace_frame_out;
	switch_frame_t *native_read_frame;
	switch_frame_t *native_write_frame;
	switch_media_bug_callback_t callback;
	switch_mutex_t *read_mutex;
	switch_mutex_t *write_mutex;
	switch_core_session_t *session;
	void *user_data;
	uint32_t flags;
	uint8_t ready;
	uint8_t data[SWITCH_RECOMMENDED_BUFFER_SIZE];
	int16_t tmp[SWITCH_RECOMMENDED_BUFFER_SIZE];
	time_t stop_time;
	switch_thread_id_t thread_id;
	char *function;
	char *target;
	switch_codec_implementation_t read_impl;
	switch_codec_implementation_t write_impl;
	uint32_t record_frame_size;
	uint32_t record_pre_buffer_count;
	uint32_t record_pre_buffer_max;
	switch_frame_t *ping_frame;
	switch_frame_t *read_demux_frame;
	struct switch_media_bug *next;
};

typedef enum {
	DBTYPE_DEFAULT = 0,
	DBTYPE_MSSQL = 1,
} switch_dbtype_t;

struct switch_runtime {
	switch_time_t initiated;
	switch_time_t reference;
	int64_t offset;
	switch_event_t *global_vars;
	switch_hash_t *mime_types;
	switch_hash_t *ptimes;
	switch_memory_pool_t *memory_pool;
	const switch_state_handler_table_t *state_handlers[SWITCH_MAX_STATE_HANDLERS];
	int state_handler_index;
	FILE *console;
	uint8_t running;
	char uuid_str[SWITCH_UUID_FORMATTED_LENGTH + 1];
	uint32_t flags;
	switch_time_t timestamp;
	switch_mutex_t *uuid_mutex;
	switch_mutex_t *throttle_mutex;
	switch_mutex_t *session_hash_mutex;
	switch_mutex_t *global_mutex;
	switch_thread_rwlock_t *global_var_rwlock;
	uint32_t sps_total;
	int32_t sps;
	int32_t sps_last;
	int32_t sps_peak;
	int32_t sps_peak_fivemin;
	int32_t sessions_peak;
	int32_t sessions_peak_fivemin;
	switch_log_level_t hard_log_level;
	char *mailer_app;
	char *mailer_app_args;
	uint32_t max_dtmf_duration;
	uint32_t min_dtmf_duration;
	uint32_t default_dtmf_duration;
	switch_frame_t dummy_cng_frame;
	char dummy_data[5];
	switch_bool_t colorize_console;
	char *odbc_dsn;
	char *dbname;
	uint32_t debug_level;
	uint32_t runlevel;
	uint32_t tipping_point;
	uint32_t microseconds_per_tick;
	int32_t timer_affinity;
	switch_profile_timer_t *profile_timer;
	double profile_time;
	double min_idle_time;
	switch_dbtype_t odbc_dbtype;
	char hostname[256];
	char *switchname;
	int multiple_registrations;
	uint32_t max_db_handles;
	uint32_t db_handle_timeout;
	int cpu_count;
	uint32_t time_sync;
	char *core_db_pre_trans_execute;
	char *core_db_post_trans_execute;
	char *core_db_inner_pre_trans_execute;
	char *core_db_inner_post_trans_execute;
	int events_use_dispatch;
};

extern struct switch_runtime runtime;


struct switch_session_manager {
	switch_memory_pool_t *memory_pool;
	switch_hash_t *session_table;
	uint32_t session_count;
	uint32_t session_limit;
	switch_size_t session_id;
	switch_queue_t *thread_queue;
	switch_thread_t *manager_thread;
	switch_mutex_t *mutex;
	switch_thread_cond_t *cond;
	switch_mutex_t *cond_mutex;
	switch_mutex_t *cond2_mutex;
	int ready;
	int running;
	int busy;
	int popping;
	int starting;
};

extern struct switch_session_manager session_manager;



switch_status_t switch_core_sqldb_start(switch_memory_pool_t *pool, switch_bool_t manage);
void switch_core_sqldb_stop(void);
void switch_core_session_init(switch_memory_pool_t *pool);
void switch_core_session_uninit(void);
void switch_core_state_machine_init(switch_memory_pool_t *pool);
switch_memory_pool_t *switch_core_memory_init(void);
void switch_core_memory_stop(void);
