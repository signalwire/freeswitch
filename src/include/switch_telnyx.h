#ifndef SWITCH_TELNYX_H
#define SWITCH_TELNYX_H

#include "switch.h"

SWITCH_BEGIN_EXTERN_C

typedef int (*switch_telnyx_hangup_cause_to_sip_func)(switch_core_session_t *, switch_call_cause_t);
typedef void (*switch_telnyx_sofia_on_hangup_func)(switch_core_session_t *);
typedef switch_bool_t (*switch_telnyx_sofia_on_init_func)(switch_core_session_t *);
typedef void (*switch_telnyx_on_set_variable_func)(switch_channel_t*, const char*, const char*);
typedef int (*switch_telnyx_recover_func)(switch_core_session_t *);
typedef void (*switch_telnyx_on_channel_recover_func)(switch_channel_t*);
typedef void (*switch_telnyx_on_populate_core_heartbeat_func)(switch_event_t *);
typedef void (*switch_telnyx_publish_json_event_func)(const char*);
typedef void (*switch_telnyx_process_audio_stats_func)(switch_core_session_t*,switch_rtp_stats_t*);
typedef void (*switch_telnyx_process_flaws_func)(switch_rtp_t *,int);
typedef void (*switch_telnyx_set_current_trace_message_func)(const char*);
typedef switch_call_cause_t (*switch_telnyx_recompute_cause_code_func)(switch_channel_t*, int , switch_call_cause_t);
typedef switch_bool_t (*switch_telnyx_sip_cause_to_q850_func)(int, switch_call_cause_t*);
typedef switch_bool_t (*switch_telnyx_on_media_timeout_func)(switch_channel_t*, switch_rtp_t*);

typedef struct switch_telnyx_event_dispatch_s {
	switch_telnyx_hangup_cause_to_sip_func switch_telnyx_hangup_cause_to_sip;
	switch_telnyx_sofia_on_init_func switch_telnyx_sofia_on_init;
	switch_telnyx_sofia_on_hangup_func switch_telnyx_sofia_on_hangup;
	switch_telnyx_on_set_variable_func switch_telnyx_on_set_variable;
	switch_telnyx_recover_func switch_telnyx_call_recover;
	switch_telnyx_on_channel_recover_func switch_telnyx_on_channel_recover;
	switch_telnyx_publish_json_event_func switch_telnyx_publish_json_event;
	switch_telnyx_process_audio_stats_func switch_telnyx_process_audio_stats;
	switch_telnyx_process_flaws_func switch_telnyx_process_flaws;
	switch_telnyx_set_current_trace_message_func switch_telnyx_set_current_trace_message;
	switch_telnyx_recompute_cause_code_func switch_telnyx_recompute_cause_code;
	switch_telnyx_sip_cause_to_q850_func switch_telnyx_sip_cause_to_q850;
	switch_telnyx_on_media_timeout_func switch_telnyx_on_media_timeout;
} switch_telnyx_event_dispatch_t;

SWITCH_DECLARE(void) switch_telnyx_init(switch_memory_pool_t *pool);
SWITCH_DECLARE(void) switch_telnyx_deinit();
SWITCH_DECLARE(int) switch_telnyx_hangup_cause_to_sip(switch_core_session_t *session, switch_call_cause_t cause);
SWITCH_DECLARE(switch_bool_t) switch_telnyx_sofia_on_init(switch_core_session_t *session);
SWITCH_DECLARE(void) switch_telnyx_sofia_on_hangup(switch_core_session_t *session);
SWITCH_DECLARE(switch_telnyx_event_dispatch_t*) switch_telnyx_event_dispatch();
SWITCH_DECLARE(void) switch_telnyx_on_set_variable(switch_channel_t* channel, const char* name, const char* value);
SWITCH_DECLARE(int) switch_telnyx_call_recover(switch_core_session_t* session);
SWITCH_DECLARE(void) switch_telnyx_on_channel_recover(switch_channel_t* channel);
SWITCH_DECLARE(void) switch_telnyx_on_populate_core_heartbeat(switch_event_t* event);
SWITCH_DECLARE(void) switch_telnyx_add_core_heartbeat_callback(switch_telnyx_on_populate_core_heartbeat_func cb);
SWITCH_DECLARE(void) switch_telnyx_publish_json_event(const char* json);
SWITCH_DECLARE(void) switch_telnyx_process_audio_stats(switch_core_session_t* session, switch_rtp_stats_t* stats);
SWITCH_DECLARE(void) switch_telnyx_process_flaws(switch_rtp_t* rtp_session, int penalty);
SWITCH_DECLARE(void) switch_telnyx_set_current_trace_message(const char* msg);

SWITCH_DECLARE(switch_call_cause_t) switch_telnyx_recompute_cause_code(switch_channel_t* channel, int status, switch_call_cause_t cause);
SWITCH_DECLARE(switch_bool_t) switch_telnyx_sip_cause_to_q850(int status, switch_call_cause_t* cause);
SWITCH_DECLARE(switch_bool_t) switch_telnyx_sip_on_media_timeout(switch_channel_t* channel, switch_rtp_t* rtp_session);

SWITCH_END_EXTERN_C

#endif /* SWITCH_TELNYX_H */

