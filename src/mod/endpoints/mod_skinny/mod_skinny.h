/* 
 * FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
 * Copyright (C) 2010, Mathieu Parent <math.parent@gmail.com>
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
 * Mathieu Parent <math.parent@gmail.com>
 * Portions created by the Initial Developer are Copyright (C)
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 * 
 * Mathieu Parent <math.parent@gmail.com>
 *
 *
 * mod_skinny.h -- Skinny Call Control Protocol (SCCP) Endpoint Module
 *
 */

#ifndef _MOD_SKINNY_H
#define _MOD_SKINNY_H

#include <switch.h>

/*****************************************************************************/
/* UTILITY MACROS */
/*****************************************************************************/
#define empty_null(a) ((a)?(a):NULL)
#define empty_null2(a,b) ((a)?(a):empty_null(b))

/*****************************************************************************/
/* LOGGING FUNCTIONS */
/*****************************************************************************/
#define skinny_undef_str(x) (zstr(x) ? "_undef_" : x)

#define skinny_log_l(listener, level, _fmt, ...) switch_log_printf(SWITCH_CHANNEL_LOG, level, \
    "[%s:%d @ %s:%d] " _fmt, skinny_undef_str(listener->device_name), listener->device_instance, skinny_undef_str(listener->remote_ip), \
    listener->remote_port, __VA_ARGS__)

#define skinny_log_l_msg(listener, level, _fmt) switch_log_printf(SWITCH_CHANNEL_LOG, level, \
    "[%s:%d @ %s:%d] " _fmt, skinny_undef_str(listener->device_name), listener->device_instance, skinny_undef_str(listener->remote_ip), \
    listener->remote_port)

#define skinny_log_l_ffl(listener, file, func, line, level, _fmt, ...) switch_log_printf( \
	SWITCH_CHANNEL_ID_LOG, file, func, line, NULL, level, \
    "[%s:%d @ %s:%d] " _fmt, skinny_undef_str(listener->device_name), listener->device_instance, skinny_undef_str(listener->remote_ip), \
    listener->remote_port, __VA_ARGS__)

#define skinny_log_ls(listener, session, level, _fmt, ...) switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), level, \
    "[%s:%d @ %s:%d] " _fmt, skinny_undef_str(listener->device_name), listener->device_instance, skinny_undef_str(listener->remote_ip), \
    listener->remote_port, __VA_ARGS__)

#define skinny_log_ls_msg(listener, session, level, _fmt) switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), level, \
    "[%s:%d @ %s:%d] " _fmt, skinny_undef_str(listener->device_name), listener->device_instance, skinny_undef_str(listener->remote_ip), \
    listener->remote_port)

#define skinny_log_s(session, level, _fmt, ...) switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), level, \
    _fmt, __VA_ARGS__)


/*****************************************************************************/
/* MODULE TYPES */
/*****************************************************************************/
#define SKINNY_EVENT_REGISTER "skinny::register"
#define SKINNY_EVENT_UNREGISTER "skinny::unregister"
#define SKINNY_EVENT_EXPIRE "skinny::expire"
#define SKINNY_EVENT_ALARM "skinny::alarm"
#define SKINNY_EVENT_XML_ALARM "skinny::xml_alarm"
#define SKINNY_EVENT_CALL_STATE "skinny::call_state"
#define SKINNY_EVENT_USER_TO_DEVICE "skinny::user_to_device"
#define SKINNY_EVENT_DEVICE_TO_USER "skinny::device_to_user"

struct skinny_globals {
	int running;
	switch_memory_pool_t *pool;
	switch_mutex_t *mutex;
	switch_hash_t *profile_hash;
	switch_event_node_t *user_to_device_node;
	switch_event_node_t *call_state_node;
	switch_event_node_t *message_waiting_node;
	switch_event_node_t *trap_node;
	int auto_restart;
};
typedef struct skinny_globals skinny_globals_t;

extern skinny_globals_t globals;

typedef enum {
	PFLAG_LISTENER_READY = (1 << 0),
	PFLAG_SHOULD_RESPAWN = (1 << 1),
	PFLAG_RESPAWN = (1 << 2),
} profile_flag_t;

struct skinny_profile {
	/* prefs */
	char *name;
	char *domain;
	char *ip;
	unsigned int port;
	char *dialplan;
	char *context;
	char *patterns_dialplan;
	char *patterns_context;
	uint32_t keep_alive;
	char date_format[6];
	int debug;
	int auto_restart;
	switch_hash_t *soft_key_set_sets_hash;
	switch_hash_t *device_type_params_hash;
	/* extensions */
	char *ext_voicemail;
	char *ext_redial;
	char *ext_meetme;
	char *ext_pickup;
	char *ext_cfwdall;
	/* db */
	char *dbname;
	char *odbc_dsn;
	switch_odbc_handle_t *master_odbc;
	switch_mutex_t *sql_mutex;	
	/* stats */
	uint32_t ib_calls;
	uint32_t ob_calls;
	uint32_t ib_failed_calls;
	uint32_t ob_failed_calls;	
	/* listener */
	int listener_threads;
	switch_mutex_t *listener_mutex;	
	switch_socket_t *sock;
	switch_mutex_t *sock_mutex;
	struct listener *listeners;
	int flags;
	switch_mutex_t *flag_mutex;
	/* call id */
	uint32_t next_call_id;
	/* others */
	switch_memory_pool_t *pool;
};
typedef struct skinny_profile skinny_profile_t;

struct skinny_device_type_params {
	char firmware_version[16];
};
typedef struct skinny_device_type_params skinny_device_type_params_t;

typedef enum {
	SKINNY_ACTION_PROCESS,
	SKINNY_ACTION_DROP,
	SKINNY_ACTION_WAIT
} skinny_action_t;

/*****************************************************************************/
/* LISTENERS TYPES */
/*****************************************************************************/

typedef enum {
	LFLAG_RUNNING = (1 << 0),
} listener_flag_t;

#define SKINNY_MAX_LINES 42
struct listener {
	skinny_profile_t *profile;
	char device_name[16];
	uint32_t device_instance;
	uint32_t device_type;

	char firmware_version[16];
	char *soft_key_set_set;

	switch_socket_t *sock;
	switch_memory_pool_t *pool;
	switch_thread_rwlock_t *rwlock;
	char remote_ip[50];
	switch_port_t remote_port;
	char local_ip[50];
	switch_port_t local_port;
	switch_mutex_t *flag_mutex;
	uint32_t flags;
	time_t expire_time;
	struct listener *next;
	char *ext_voicemail;
	char *ext_redial;
	char *ext_meetme;
	char *ext_pickup;
	char *ext_cfwdall;
};

typedef struct listener listener_t;

typedef switch_status_t (*skinny_listener_callback_func_t) (listener_t *listener, void *pvt);

/*****************************************************************************/
/* CHANNEL TYPES */
/*****************************************************************************/
typedef enum {
	TFLAG_FORCE_ROUTE = (1 << 0),
	TFLAG_EARLY_MEDIA = (1 << 1),
	TFLAG_IO = (1 << 2),
	TFLAG_READING = (1 << 3),
	TFLAG_WRITING = (1 << 4)
} TFLAGS;

typedef enum {
	GFLAG_MY_CODEC_PREFS = (1 << 0)
} GFLAGS;

struct private_object {
	unsigned int flags;
	switch_mutex_t *flag_mutex;
	switch_frame_t read_frame;
	unsigned char databuf[SWITCH_RECOMMENDED_BUFFER_SIZE];
	switch_core_session_t *session;
	switch_caller_profile_t *caller_profile;
	switch_mutex_t *mutex;

	/* identification */
	skinny_profile_t *profile;
	uint32_t call_id;
	uint32_t party_id;

	/* related calls */
	uint32_t transfer_to_call_id;
	uint32_t transfer_from_call_id;

	/* codec */
	char *iananame;	
	switch_codec_t read_codec;
	switch_codec_t write_codec;
	switch_codec_implementation_t read_impl;
	switch_codec_implementation_t write_impl;
	unsigned long rm_rate;
	uint32_t codec_ms;
	char *rm_encoding;
	char *rm_fmtp;
	switch_payload_t agreed_pt;
	/* RTP */
	switch_rtp_t *rtp_session;
	char *local_sdp_audio_ip;
	switch_port_t local_sdp_audio_port;
	char *remote_sdp_audio_ip;
	switch_port_t remote_sdp_audio_port;
};

typedef struct private_object private_t;

/*****************************************************************************/
/* PROFILES FUNCTIONS */
/*****************************************************************************/
skinny_profile_t *skinny_find_profile(const char *profile_name);
switch_status_t skinny_profile_dump(const skinny_profile_t *profile, switch_stream_handle_t *stream);
switch_status_t skinny_profile_find_listener_by_device_name(skinny_profile_t *profile, const char *device_name, listener_t **listener);
switch_status_t skinny_profile_find_listener_by_device_name_and_instance(skinny_profile_t *profile, const char *device_name, uint32_t device_instance, listener_t **listener);
char * skinny_profile_find_session_uuid(skinny_profile_t *profile, listener_t *listener, uint32_t *line_instance_p, uint32_t call_id);
#ifdef SWITCH_DEBUG_RWLOCKS
switch_core_session_t * skinny_profile_perform_find_session(skinny_profile_t *profile, listener_t *listener, uint32_t *line_instance_p, uint32_t call_id, const char *file, const char *func, int line);
#define skinny_profile_find_session(profile, listener, line_instance_p, call_id) skinny_profile_perform_find_session(profile, listener, line_instance_p, call_id, __FILE__, __SWITCH_FUNC__, __LINE__)
#else
switch_core_session_t * skinny_profile_find_session(skinny_profile_t *profile, listener_t *listener, uint32_t *line_instance_p, uint32_t call_id);
#endif
switch_status_t dump_device(skinny_profile_t *profile, const char *device_name, switch_stream_handle_t *stream);
switch_status_t skinny_profile_respawn(skinny_profile_t *profile, int force);
switch_status_t skinny_profile_set(skinny_profile_t *profile, const char *var, const char *val);
void profile_walk_listeners(skinny_profile_t *profile, skinny_listener_callback_func_t callback, void *pvt);

/*****************************************************************************/
/* SQL FUNCTIONS */
/*****************************************************************************/
switch_cache_db_handle_t *skinny_get_db_handle(skinny_profile_t *profile);
switch_status_t skinny_execute_sql(skinny_profile_t *profile, char *sql, switch_mutex_t *mutex);
switch_bool_t skinny_execute_sql_callback(skinny_profile_t *profile,
		switch_mutex_t *mutex, char *sql, switch_core_db_callback_func_t callback, void *pdata);

/*****************************************************************************/
/* LISTENER FUNCTIONS */
/*****************************************************************************/
uint8_t listener_is_ready(listener_t *listener);
switch_status_t kill_listener(listener_t *listener, void *pvt);
switch_status_t keepalive_listener(listener_t *listener, void *pvt);
void skinny_clean_listener_from_db(listener_t *listener);
void skinny_clean_device_from_db(listener_t *listener, char *device_name);

/*****************************************************************************/
/* CHANNEL FUNCTIONS */
/*****************************************************************************/
void skinny_line_perform_set_state(const char *file, const char *func, int line, listener_t *listener, uint32_t line_instance, uint32_t call_id, uint32_t call_state);
#define  skinny_line_set_state(listener, line_instance, call_id, call_state)  skinny_line_perform_set_state(__FILE__, __SWITCH_FUNC__, __LINE__, listener, line_instance, call_id, call_state)

uint32_t skinny_line_get_state(listener_t *listener, uint32_t line_instance, uint32_t call_id);

switch_status_t skinny_tech_set_codec(private_t *tech_pvt, int force);
void tech_init(private_t *tech_pvt, skinny_profile_t *profile, switch_core_session_t *session);
switch_status_t channel_on_init(switch_core_session_t *session);
switch_status_t channel_on_hangup(switch_core_session_t *session);
switch_status_t channel_on_destroy(switch_core_session_t *session);
switch_status_t channel_on_routing(switch_core_session_t *session);
switch_status_t channel_on_exchange_media(switch_core_session_t *session);
switch_status_t channel_on_soft_execute(switch_core_session_t *session);
switch_call_cause_t channel_outgoing_channel(switch_core_session_t *session, switch_event_t *var_event,
		switch_caller_profile_t *outbound_profile,
		switch_core_session_t **new_session, switch_memory_pool_t **pool, switch_originate_flag_t flags, switch_call_cause_t *cancel_cause);
switch_status_t channel_read_frame(switch_core_session_t *session, switch_frame_t **frame, switch_io_flag_t flags, int stream_id);
switch_status_t channel_write_frame(switch_core_session_t *session, switch_frame_t *frame, switch_io_flag_t flags, int stream_id);
switch_status_t channel_kill_channel(switch_core_session_t *session, int sig);

/*****************************************************************************/
/* MODULE FUNCTIONS */
/*****************************************************************************/
switch_endpoint_interface_t *skinny_get_endpoint_interface();

/*****************************************************************************/
/* TEXT FUNCTIONS */
/*****************************************************************************/
#define skinny_textid2raw(label) (label > 0 ? switch_mprintf("\200%c", label) : switch_mprintf(""))
char *skinny_expand_textid(const char *str);

#endif /* _MOD_SKINNY_H */

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

