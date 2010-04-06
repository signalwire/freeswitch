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
/* MODULE TYPES */
/*****************************************************************************/
#define SKINNY_EVENT_REGISTER "skinny::register"
#define SKINNY_EVENT_UNREGISTER "skinny::unregister"
#define SKINNY_EVENT_EXPIRE "skinny::expire"
#define SKINNY_EVENT_ALARM "skinny::alarm"
#define SKINNY_EVENT_CALL_STATE "skinny::call_state"

struct skinny_globals {
    int running;
    switch_memory_pool_t *pool;
    switch_mutex_t *mutex;
    switch_hash_t *profile_hash;
    switch_event_node_t *heartbeat_node;
    switch_event_node_t *call_state_node;
};
typedef struct skinny_globals skinny_globals_t;

skinny_globals_t globals;

struct skinny_profile {
    /* prefs */
    char *name;
    char *domain;
    char *ip;
    unsigned int port;
    char *dialplan;
    char *context;
    uint32_t keep_alive;
    char date_format[6];
    int debug;
    /* db */
    char *dbname;
    char *odbc_dsn;
    char *odbc_user;
    char *odbc_pass;
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
    uint8_t listener_ready;
    /* call id */
    uint32_t next_call_id;
    /* others */
    switch_memory_pool_t *pool;
};
typedef struct skinny_profile skinny_profile_t;


/*****************************************************************************/
/* LISTENERS TYPES */
/*****************************************************************************/

typedef enum {
    LFLAG_RUNNING = (1 << 0),
} event_flag_t;

#define SKINNY_MAX_LINES 42
struct listener {
    skinny_profile_t *profile;
    char device_name[16];
    uint32_t device_instance;

    switch_socket_t *sock;
    switch_memory_pool_t *pool;
    switch_thread_rwlock_t *rwlock;
    switch_sockaddr_t *sa;
    char remote_ip[50];
    switch_mutex_t *flag_mutex;
    uint32_t flags;
    switch_port_t remote_port;
    uint32_t id;
    time_t expire_time;
    struct listener *next;
};

typedef struct listener listener_t;

typedef switch_status_t (*skinny_listener_callback_func_t) (listener_t *listener, void *pvt);

/*****************************************************************************/
/* CHANNEL TYPES */
/*****************************************************************************/
typedef enum {
    TFLAG_IO = (1 << 0),
    TFLAG_INBOUND = (1 << 1),
    TFLAG_OUTBOUND = (1 << 2),
    TFLAG_DTMF = (1 << 3),
    TFLAG_VOICE = (1 << 4),
    TFLAG_HANGUP = (1 << 5),
    TFLAG_LINEAR = (1 << 6),
    TFLAG_CODEC = (1 << 7),

    TFLAG_READING = (1 << 9),
    TFLAG_WRITING = (1 << 10)
} TFLAGS;

typedef enum {
    GFLAG_MY_CODEC_PREFS = (1 << 0)
} GFLAGS;

struct private_object {
    unsigned int flags;
    switch_frame_t read_frame;
    unsigned char databuf[SWITCH_RECOMMENDED_BUFFER_SIZE];
    switch_core_session_t *session;
    switch_caller_profile_t *caller_profile;
    switch_mutex_t *mutex;
    switch_mutex_t *flag_mutex;
    /* identification */
    uint32_t call_id;
    uint32_t party_id;

    skinny_profile_t *profile;

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
switch_core_session_t * skinny_profile_find_session(skinny_profile_t *profile, listener_t *listener, uint32_t *line_instance_p, uint32_t call_id);
switch_status_t dump_device(skinny_profile_t *profile, const char *device_name, switch_stream_handle_t *stream);

/*****************************************************************************/
/* SQL FUNCTIONS */
/*****************************************************************************/
void skinny_execute_sql(skinny_profile_t *profile, char *sql, switch_mutex_t *mutex);
switch_bool_t skinny_execute_sql_callback(skinny_profile_t *profile,
										      switch_mutex_t *mutex, char *sql, switch_core_db_callback_func_t callback, void *pdata);

/*****************************************************************************/
/* LISTENER FUNCTIONS */
/*****************************************************************************/
uint8_t listener_is_ready(listener_t *listener);
switch_status_t keepalive_listener(listener_t *listener, void *pvt);

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

#endif /* _MOD_SKINNY_H */

/* For Emacs:
 * Local Variables:
 * mode:c
 * indent-tabs-mode:t
 * tab-width:4
 * c-basic-offset:4
 * End:
 * For VIM:
 * vim:set softtabstop=4 shiftwidth=4 tabstop=4:
 */

