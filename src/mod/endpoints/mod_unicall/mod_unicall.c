/* 
 * FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
 * Copyright (C) 2005/2012, Anthony Minessale II <anthm@freeswitch.org>
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
 * Steve Underwood 0.0.1 <steveu@coppice.org>
 *
 *
 * mod_unicall.c -- UniCall endpoint module
 *
 */

/* This is a work in progress. Unfinished. Non-functional */

#include <switch.h>
#include <unicall.h>

SWITCH_MODULE_LOAD_FUNCTION(mod_unicall_load);
SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_unicall_shutdown);
//SWITCH_MODULE_RUNTIME_FUNCTION(mod_unicall_runtime);
SWITCH_MODULE_DEFINITION(mod_unicall, mod_unicall_load, mod_unicall_shutdown, NULL);	//mod_unicall_runtime);

#define MAX_SPANS 128

switch_endpoint_interface_t *unicall_endpoint_interface;
static switch_memory_pool_t *module_pool = NULL;
static volatile int running = 1;

typedef struct {
	int span;
	const char *id;
	const char *protocol_class;
	const char *protocol_variant;
	int protocol_end;
	int outgoing_ok;
	char *dialplan;
	char *context;
	int fd;
	uc_t *uc;
} span_data_t;

span_data_t *span_data[MAX_SPANS];

typedef enum {
	TFLAG_IO = (1 << 0),
	TFLAG_INBOUND = (1 << 1),
	TFLAG_OUTBOUND = (1 << 2),
	TFLAG_DTMF = (1 << 3),
	TFLAG_VOICE = (1 << 4),
	TFLAG_HANGUP = (1 << 5),
	TFLAG_LINEAR = (1 << 6),
	TFLAG_CODEC = (1 << 7),
	TFLAG_BREAK = (1 << 8)
} TFLAGS;

typedef enum {
	GFLAG_MY_CODEC_PREFS = (1 << 0)
} GFLAGS;


static struct {
	int debug;
	/*! Requested frame duration, in ms */
	uint32_t frame_duration;
	int dtmf_on;
	int dtmf_off;
	int suppress_dtmf_tone;
	int ignore_dtmf_tone;
	char *dialplan;
	char *codec_string;
	char *codec_order[SWITCH_MAX_CODECS];
	int codec_order_last;
	char *codec_rates_string;
	char *codec_rates[SWITCH_MAX_CODECS];
	int codec_rates_last;
	unsigned int flags;
	int calls;
	int configured_spans;
	switch_hash_t *call_hash;
	switch_mutex_t *mutex;
	switch_mutex_t *hash_mutex;
	switch_mutex_t *channel_mutex;
} globals;

typedef struct {
	unsigned int flags;
	switch_codec_t read_codec;
	switch_codec_t write_codec;
	switch_frame_t read_frame;
	uint8_t databuf[SWITCH_RECOMMENDED_BUFFER_SIZE];
	switch_core_session_t *session;
	switch_caller_profile_t *caller_profile;
	switch_mutex_t *mutex;
	switch_mutex_t *flag_mutex;
	//switch_thread_cond_t *cond;
	uc_t *uc;
} private_t;


SWITCH_DECLARE_GLOBAL_STRING_FUNC(set_global_dialplan, globals.dialplan);



static switch_status_t unicall_on_init(switch_core_session_t *session);
static switch_status_t unicall_on_routing(switch_core_session_t *session);
static switch_status_t unicall_on_execute(switch_core_session_t *session);
static switch_status_t unicall_on_hangup(switch_core_session_t *session);
static switch_status_t unicall_on_exchange_media(switch_core_session_t *session);
static switch_status_t unicall_on_soft_execute(switch_core_session_t *session);

static switch_call_cause_t unicall_outgoing_channel(switch_core_session_t *session,
													switch_event_t *var_event,
													switch_caller_profile_t *outbound_profile,
													switch_core_session_t **new_session,
													switch_memory_pool_t **pool, switch_originate_flag_t flags, switch_call_cause_t *cancel_cause);
static switch_status_t unicall_read_frame(switch_core_session_t *session, switch_frame_t **frame, switch_io_flag_t flags, int stream_id);
static switch_status_t unicall_write_frame(switch_core_session_t *session, switch_frame_t *frame, switch_io_flag_t flags, int stream_id);
static switch_status_t unicall_kill_channel(switch_core_session_t *session, int sig);
static switch_status_t unicall_send_dtmf(switch_core_session_t *session, const switch_dtmf_t *dtmf);
static switch_status_t unicall_receive_message(switch_core_session_t *session, switch_core_session_message_t *msg);
static switch_status_t unicall_receive_event(switch_core_session_t *session, switch_event_t *event);

static void unicall_message(int level, const char *s)
{
	int switch_level;

	switch (level) {
	case UC_LOG_NONE:
		switch_level = SWITCH_LOG_CRIT;
		break;
	case UC_LOG_ERROR:
		switch_level = SWITCH_LOG_ERROR;
		break;
	case UC_LOG_WARNING:
		switch_level = SWITCH_LOG_WARNING;
		break;
	case UC_LOG_PROTOCOL_ERROR:
		switch_level = SWITCH_LOG_ERROR;
		break;
	case UC_LOG_PROTOCOL_WARNING:
		switch_level = SWITCH_LOG_WARNING;
		break;
	case UC_LOG_INFO:
		switch_level = SWITCH_LOG_NOTICE;
		//switch_level = SWITCH_LOG_NOTICE;
		break;
	case UC_LOG_FLOW:
	case UC_LOG_FLOW_2:
	case UC_LOG_FLOW_3:
	case UC_LOG_CAS:
	case UC_LOG_TONE:
	case UC_LOG_DEBUG_1:
	case UC_LOG_DEBUG_2:
	case UC_LOG_DEBUG_3:
		switch_level = SWITCH_LOG_DEBUG;
		break;
	default:
		switch_level = SWITCH_LOG_CRIT;
		break;
	}
	switch_log_printf(SWITCH_CHANNEL_LOG, switch_level, s);
}

static void unicall_report(const char *s)
{
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, s);
}

#if 0
static switch_call_cause_t unicall_incoming_channel(zap_sigmsg_t *sigmsg, switch_core_session_t **sp)
{
	switch_core_session_t *session = NULL;
	private_t *tech_pvt = NULL;
	switch_channel_t *channel = NULL;
	char name[128];

	*sp = NULL;

	if (!(session = switch_core_session_request_uuid(freetdm_endpoint_interface, SWITCH_CALL_DIRECTION_INBOUND, SOF_NONE, NULL, switch_event_get_header(var_event, "origination_uuid")))) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "Initialization Error!\n");
		return ZAP_FAIL;
	}

	switch_core_session_add_stream(session, NULL);

	tech_pvt = (private_t *) switch_core_session_alloc(session, sizeof(private_t));
	assert(tech_pvt != NULL);
	channel = switch_core_session_get_channel(session);
	if (tech_init(tech_pvt, session, sigmsg->channel) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "Initialization Error!\n");
		switch_core_session_destroy(&session);
		return ZAP_FAIL;
	}

	*sigmsg->channel->caller_data.collected = '\0';

	if (sigmsg->channel->caller_data.cid_name[0] == '\0')
		switch_set_string(sigmsg->channel->caller_data.cid_name, sigmsg->channel->chan_name);

	if (sigmsg->channel->caller_data.cid_num.digits[0] == '\0') {
		if (sigmsg->channel->caller_data.ani.digits[0] != '\0')
			switch_set_string(sigmsg->channel->caller_data.cid_num.digits, sigmsg->channel->caller_data.ani.digits);
		else
			switch_set_string(sigmsg->channel->caller_data.cid_num.digits, sigmsg->channel->chan_number);
	}

	tech_pvt->caller_profile = switch_caller_profile_new(switch_core_session_get_pool(session),
														 "UniCall",
														 SPAN_CONFIG[sigmsg->channel->span_id].dialplan,
														 sigmsg->channel->caller_data.cid_name,
														 sigmsg->channel->caller_data.cid_num.digits,
														 NULL,
														 sigmsg->channel->caller_data.ani.digits,
														 sigmsg->channel->caller_data.aniII,
														 sigmsg->channel->caller_data.rdnis.digits,
														 (char *) modname,
														 SPAN_CONFIG[sigmsg->channel->span_id].context, sigmsg->channel->caller_data.dnis.digits);
	assert(tech_pvt->caller_profile != NULL);

	if (sigmsg->channel->caller_data.screen == 1 || sigmsg->channel->caller_data.screen == 3)
		switch_set_flag(tech_pvt->caller_profile, SWITCH_CPF_SCREEN);

	if (sigmsg->channel->caller_data.pres)
		switch_set_flag(tech_pvt->caller_profile, SWITCH_CPF_HIDE_NAME | SWITCH_CPF_HIDE_NUMBER);

	snprintf(name, sizeof(name), "UNICALL/%u:%u/%s", sigmsg->channel->span_id, sigmsg->channel->chan_id, tech_pvt->caller_profile->destination_number);
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Connect inbound channel %s\n", name);
	switch_channel_set_name(channel, name);
	switch_channel_set_caller_profile(channel, tech_pvt->caller_profile);

	switch_channel_set_state(channel, CS_INIT);
	if (switch_core_session_thread_launch(session) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "Error spawning thread\n");
		switch_core_session_destroy(&session);
		return ZAP_FAIL;
	}

	if (zap_channel_add_token(sigmsg->channel, switch_core_session_get_uuid(session), 0) != ZAP_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "Error adding token\n");
		switch_core_session_destroy(&session);
		return ZAP_FAIL;
	}
	*sp = session;

	return ZAP_SUCCESS;
}
#endif

static void on_devicefail(uc_t *uc, switch_core_session_t *session, uc_event_t *e)
{
	//switch_channel_t *channel;
	//private_t *tech_pvt;

	//tech_pvt = switch_core_session_get_private(session);
	//assert(tech_pvt != NULL);
	//channel = switch_core_session_get_channel(session);
	//assert(channel != NULL);

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "on_devicefail\n");
}

static void on_protocolfail(uc_t *uc, switch_core_session_t *session, uc_event_t *e)
{
	//switch_channel_t *channel;
	//private_t *tech_pvt;

	//tech_pvt = switch_core_session_get_private(session);
	//assert(tech_pvt != NULL);
	//channel = switch_core_session_get_channel(session);
	//assert(channel != NULL);

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "on_protocolfail\n");
}

static void on_sigchanstatus(uc_t *uc, switch_core_session_t *session, uc_event_t *e)
{
	//switch_channel_t *channel;
	//private_t *tech_pvt;

	//tech_pvt = switch_core_session_get_private(session);
	//assert(tech_pvt != NULL);
	//channel = switch_core_session_get_channel(session);
	//assert(channel != NULL);

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "on_sigchanstatus\n");
}

static void on_detected(uc_t *uc, switch_core_session_t *session, uc_event_t *e)
{
	//switch_channel_t *channel;
	//private_t *tech_pvt;
	//struct channel_map *chanmap;
	//char name[128];

	//tech_pvt = switch_core_session_get_private(session);
	//assert(tech_pvt != NULL);
	//channel = switch_core_session_get_channel(session);
	//assert(channel != NULL);

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "on_detected\n");
	switch_mutex_lock(globals.channel_mutex);

	//chanmap = spri->private_info;

#if 0
	if ((session = switch_core_session_locate(chanmap->map[e->offered.channel]))) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "--Duplicate detected on channel s%dc%d (ignored)\n", spri->span, e->offered.channel);
		switch_core_session_rwunlock(session);
		switch_mutex_unlock(globals.channel_mutex);
		return;
	}

	switch_log_printf(SWITCH_CHANNEL_LOG,
					  SWITCH_LOG_NOTICE,
					  "-- Detected on channel s%dc%d (from %s to %s)\n",
					  spri->span, e->offered.channel, e->offered.parms.originating_number, e->offered.parms.destination_number);

	switch_mutex_unlock(chanmap->mutex);

	//pri_proceeding(spri->pri, e->offered.call, e->offered.channel, 0);
	//pri_acknowledge(spri->pri, e->offered.call, e->offered.channel, 0);

	switch_mutex_unlock(chanmap->mutex);

	if ((session = unicall_incoming_channel(sigmsg, &session))) {
	} else {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Cannot create new inbound channel!\n");
	}
#endif
	switch_mutex_unlock(globals.channel_mutex);
}

static void on_offered(uc_t *uc, switch_core_session_t *session, uc_event_t *e)
{
	//switch_channel_t *channel;
	//private_t *tech_pvt;
	//struct channel_map *chanmap;
	//char name[128];

	//tech_pvt = switch_core_session_get_private(session);
	//assert(tech_pvt != NULL);
	//channel = switch_core_session_get_channel(session);
	//assert(channel != NULL);

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "on_offered\n");
}

static void on_requestmoreinfo(uc_t *uc, switch_core_session_t *session, uc_event_t *e)
{
	switch_channel_t *channel;
	private_t *tech_pvt;

	tech_pvt = switch_core_session_get_private(session);
	assert(tech_pvt != NULL);
	channel = switch_core_session_get_channel(session);
	assert(channel != NULL);

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "on_requestmoreinfo\n");
}

static void on_accepted(uc_t *uc, switch_core_session_t *session, uc_event_t *e)
{
	switch_channel_t *channel;
	private_t *tech_pvt;

	tech_pvt = switch_core_session_get_private(session);
	assert(tech_pvt != NULL);
	channel = switch_core_session_get_channel(session);
	assert(channel != NULL);

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "on_accepted\n");
}

static void on_callinfo(uc_t *uc, switch_core_session_t *session, uc_event_t *e)
{
	switch_channel_t *channel;
	private_t *tech_pvt;

	tech_pvt = switch_core_session_get_private(session);
	assert(tech_pvt != NULL);
	channel = switch_core_session_get_channel(session);
	assert(channel != NULL);

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "on_callinfo\n");
}

static void on_facility(uc_t *uc, switch_core_session_t *session, uc_event_t *e)
{
	switch_channel_t *channel;
	private_t *tech_pvt;

	tech_pvt = switch_core_session_get_private(session);
	assert(tech_pvt != NULL);
	channel = switch_core_session_get_channel(session);
	assert(channel != NULL);

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "on_facility\n");
}

static void on_dialednumber(uc_t *uc, switch_core_session_t *session, uc_event_t *e)
{
	switch_channel_t *channel;
	private_t *tech_pvt;

	tech_pvt = switch_core_session_get_private(session);
	assert(tech_pvt != NULL);
	channel = switch_core_session_get_channel(session);
	assert(channel != NULL);

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "on_dialednumber\n");
}

static void on_dialing(uc_t *uc, switch_core_session_t *session, uc_event_t *e)
{
	switch_channel_t *channel;
	private_t *tech_pvt;

	tech_pvt = switch_core_session_get_private(session);
	assert(tech_pvt != NULL);
	channel = switch_core_session_get_channel(session);
	assert(channel != NULL);

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "on_dialing\n");
}

static void on_sendmoreinfo(uc_t *uc, switch_core_session_t *session, uc_event_t *e)
{
	switch_channel_t *channel;
	private_t *tech_pvt;

	tech_pvt = switch_core_session_get_private(session);
	assert(tech_pvt != NULL);
	channel = switch_core_session_get_channel(session);
	assert(channel != NULL);

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "on_sendmoreinfo\n");
}

static void on_proceeding(uc_t *uc, switch_core_session_t *session, uc_event_t *e)
{
	switch_channel_t *channel;
	private_t *tech_pvt;
	//struct channel_map *chanmap;

	tech_pvt = switch_core_session_get_private(session);
	assert(tech_pvt != NULL);
	channel = switch_core_session_get_channel(session);
	assert(channel != NULL);

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "on_proceeding\n");

#if 0
	chanmap = spri->private_info;

	if ((session = switch_core_session_locate(chanmap->map[e->proceeding.channel]))) {
		switch_core_session_message_t *msg;

		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "-- Proceeding on channel s%dc%d\n", spri->span, e->proceeding.channel);

		channel = switch_core_session_get_channel(session);
		assert(channel != NULL);

		switch_core_session_pass_indication(session, SWITCH_MESSAGE_INDICATE_PROGRESS);
		switch_channel_mark_pre_answered(channel);

		switch_core_session_rwunlock(session);
	} else {
		switch_log_printf(SWITCH_CHANNEL_LOG,
						  SWITCH_LOG_NOTICE, "-- Proceeding on channel s%dc%d but it's not in use?\n", spri->span, e->proceeding.channel);
	}
#endif
}

static void on_alerting(uc_t *uc, switch_core_session_t *session, uc_event_t *e)
{
	switch_channel_t *channel;
	private_t *tech_pvt;
	//struct channel_map *chanmap;

	tech_pvt = switch_core_session_get_private(session);
	assert(tech_pvt != NULL);
	channel = switch_core_session_get_channel(session);
	assert(channel != NULL);

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "on_alerting\n");

#if 0
	chanmap = spri->private_info;

	if ((session = switch_core_session_locate(chanmap->map[e->alerting.channel]))) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "-- Ringing on channel s%dc%d\n", spri->span, e->alerting.channel);
		channel = switch_core_session_get_channel(session);
		assert(channel != NULL);

		switch_core_session_pass_indication(session, SWITCH_MESSAGE_INDICATE_RINGING);
		switch_channel_mark_ring_ready(channel);

		switch_core_session_rwunlock(session);
	} else {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "-- Ringing on channel s%dc%d %s but it's not in use?\n", spri->span, e->alerting.channel,
						  chanmap->map[e->alerting.channel]);
	}
#endif
}

static void on_connected(uc_t *uc, switch_core_session_t *session, uc_event_t *e)
{
	switch_channel_t *channel;
	private_t *tech_pvt;

	tech_pvt = switch_core_session_get_private(session);
	assert(tech_pvt != NULL);
	channel = switch_core_session_get_channel(session);
	assert(channel != NULL);

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "on_connected\n");
}

static void on_answered(uc_t *uc, switch_core_session_t *session, uc_event_t *e)
{
	switch_channel_t *channel;
	private_t *tech_pvt;

	tech_pvt = switch_core_session_get_private(session);
	assert(tech_pvt != NULL);
	channel = switch_core_session_get_channel(session);
	assert(channel != NULL);

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "on_answered\n");
}

static void on_fardisconnected(uc_t *uc, switch_core_session_t *session, uc_event_t *e)
{
	switch_channel_t *channel;
	private_t *tech_pvt;

	tech_pvt = switch_core_session_get_private(session);
	assert(tech_pvt != NULL);
	channel = switch_core_session_get_channel(session);
	assert(channel != NULL);

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "on_fardisconnected\n");
}

static void on_dropcall(uc_t *uc, switch_core_session_t *session, uc_event_t *e)
{
	switch_channel_t *channel;
	private_t *tech_pvt;

	tech_pvt = switch_core_session_get_private(session);
	assert(tech_pvt != NULL);
	channel = switch_core_session_get_channel(session);
	assert(channel != NULL);

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "on_dropcall\n");
}

static void on_releasecall(uc_t *uc, switch_core_session_t *session, uc_event_t *e)
{
	switch_channel_t *channel;
	private_t *tech_pvt;

	tech_pvt = switch_core_session_get_private(session);
	assert(tech_pvt != NULL);
	channel = switch_core_session_get_channel(session);
	assert(channel != NULL);

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "on_releasecall\n");
}

static void on_farblocked(uc_t *uc, switch_core_session_t *session, uc_event_t *e)
{
	//switch_channel_t *channel;
	//private_t *tech_pvt;

	//tech_pvt = switch_core_session_get_private(session);
	//assert(tech_pvt != NULL);
	//channel = switch_core_session_get_channel(session);
	//assert(channel != NULL);

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "on_farblocked\n");
}

static void on_farunblocked(uc_t *uc, switch_core_session_t *session, uc_event_t *e)
{
	//switch_channel_t *channel;
	//private_t *tech_pvt;

	//tech_pvt = switch_core_session_get_private(session);
	//assert(tech_pvt != NULL);
	//channel = switch_core_session_get_channel(session);
	//assert(channel != NULL);

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "on_farunblocked\n");
}

static void on_localblocked(uc_t *uc, switch_core_session_t *session, uc_event_t *e)
{
	//switch_channel_t *channel;
	//private_t *tech_pvt;

	//tech_pvt = switch_core_session_get_private(session);
	//assert(tech_pvt != NULL);
	//channel = switch_core_session_get_channel(session);
	//assert(channel != NULL);

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "on_localblocked\n");
}

static void on_localunblocked(uc_t *uc, switch_core_session_t *session, uc_event_t *e)
{
	//switch_channel_t *channel;
	//private_t *tech_pvt;

	//tech_pvt = switch_core_session_get_private(session);
	//assert(tech_pvt != NULL);
	//channel = switch_core_session_get_channel(session);
	//assert(channel != NULL);

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "on_localunblocked\n");
}

static void on_alarm(uc_t *uc, switch_core_session_t *session, uc_event_t *e)
{
	switch_channel_t *channel;
	private_t *tech_pvt;

	tech_pvt = switch_core_session_get_private(session);
	assert(tech_pvt != NULL);
	channel = switch_core_session_get_channel(session);
	assert(channel != NULL);

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "on_alarm\n");
}

static void on_resetlinedev(uc_t *uc, switch_core_session_t *session, uc_event_t *e)
{
	switch_channel_t *channel;
	private_t *tech_pvt;

	tech_pvt = switch_core_session_get_private(session);
	assert(tech_pvt != NULL);
	channel = switch_core_session_get_channel(session);
	assert(channel != NULL);

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "on_resetlinedev\n");
}

static void on_l2frame(uc_t *uc, switch_core_session_t *session, uc_event_t *e)
{
	switch_channel_t *channel;
	private_t *tech_pvt;

	tech_pvt = switch_core_session_get_private(session);
	assert(tech_pvt != NULL);
	channel = switch_core_session_get_channel(session);
	assert(channel != NULL);

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "on_l2frame\n");
}

static void on_l2bufferfull(uc_t *uc, switch_core_session_t *session, uc_event_t *e)
{
	switch_channel_t *channel;
	private_t *tech_pvt;

	tech_pvt = switch_core_session_get_private(session);
	assert(tech_pvt != NULL);
	channel = switch_core_session_get_channel(session);
	assert(channel != NULL);

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "on_l2bufferfull\n");
}

static void on_l2nobuffer(uc_t *uc, switch_core_session_t *session, uc_event_t *e)
{
	switch_channel_t *channel;
	private_t *tech_pvt;

	tech_pvt = switch_core_session_get_private(session);
	assert(tech_pvt != NULL);
	channel = switch_core_session_get_channel(session);
	assert(channel != NULL);

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "on_l2nobuffer\n");
}

static void on_usrinfo(uc_t *uc, switch_core_session_t *session, uc_event_t *e)
{
	switch_channel_t *channel;
	private_t *tech_pvt;

	tech_pvt = switch_core_session_get_private(session);
	assert(tech_pvt != NULL);
	channel = switch_core_session_get_channel(session);
	assert(channel != NULL);

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "on_usrinfo\n");
}

static void handle_uc_event(uc_t *uc, void *user_data, uc_event_t *e)
{
	switch_core_session_t *session;

	session = (switch_core_session_t *) user_data;
	assert(session != NULL);

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "event %s\n", uc_event_to_str(e->e));
	switch (e->e) {
	case UC_EVENT_DEVICEFAIL:
		on_devicefail(uc, session, e);
		break;
	case UC_EVENT_PROTOCOLFAIL:
		on_protocolfail(uc, session, e);
		break;
	case UC_EVENT_SIGCHANSTATUS:
		on_sigchanstatus(uc, session, e);
		break;
	case UC_EVENT_DETECTED:
		on_detected(uc, session, e);
		break;
	case UC_EVENT_OFFERED:
		on_offered(uc, session, e);
		break;
	case UC_EVENT_REQUESTMOREINFO:
		on_requestmoreinfo(uc, session, e);
		break;
	case UC_EVENT_ACCEPTED:
		on_accepted(uc, session, e);
		break;
	case UC_EVENT_CALLINFO:
		on_callinfo(uc, session, e);
		break;
	case UC_EVENT_FACILITY:
		on_facility(uc, session, e);
		break;
	case UC_EVENT_DIALEDNUMBER:
		on_dialednumber(uc, session, e);
		break;
	case UC_EVENT_DIALING:
		on_dialing(uc, session, e);
		break;
	case UC_EVENT_SENDMOREINFO:
		on_sendmoreinfo(uc, session, e);
		break;
	case UC_EVENT_PROCEEDING:
		on_proceeding(uc, session, e);
		break;
	case UC_EVENT_ALERTING:
		on_alerting(uc, session, e);
		break;
	case UC_EVENT_CONNECTED:
		on_connected(uc, session, e);
		break;
	case UC_EVENT_ANSWERED:
		on_answered(uc, session, e);
		break;
	case UC_EVENT_FARDISCONNECTED:
		on_fardisconnected(uc, session, e);
		break;
	case UC_EVENT_DROPCALL:
		on_dropcall(uc, session, e);
		break;
	case UC_EVENT_RELEASECALL:
		on_releasecall(uc, session, e);
		break;
	case UC_EVENT_FARBLOCKED:
		on_farblocked(uc, session, e);
		break;
	case UC_EVENT_FARUNBLOCKED:
		on_farunblocked(uc, session, e);
		break;
	case UC_EVENT_LOCALBLOCKED:
		on_localblocked(uc, session, e);
		break;
	case UC_EVENT_LOCALUNBLOCKED:
		on_localunblocked(uc, session, e);
		break;
	case UC_EVENT_ALARM:
		on_alarm(uc, session, e);
		break;
	case UC_EVENT_RESETLINEDEV:
		on_resetlinedev(uc, session, e);
		break;
	case UC_EVENT_L2FRAME:
		on_l2frame(uc, session, e);
		break;
	case UC_EVENT_L2BUFFERFULL:
		on_l2bufferfull(uc, session, e);
		break;
	case UC_EVENT_L2NOBUFFER:
		on_l2nobuffer(uc, session, e);
		break;
	case UC_EVENT_USRINFO:
		on_usrinfo(uc, session, e);
		break;
	default:
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "unknown unicall event %d\n", e->e);
		break;
	}
}

static void tech_init(private_t *tech_pvt, switch_core_session_t *session)
{
	tech_pvt->read_frame.data = tech_pvt->databuf;
	tech_pvt->read_frame.buflen = sizeof(tech_pvt->databuf);
	switch_mutex_init(&tech_pvt->mutex, SWITCH_MUTEX_NESTED, switch_core_session_get_pool(session));
	switch_mutex_init(&tech_pvt->flag_mutex, SWITCH_MUTEX_NESTED, switch_core_session_get_pool(session));
	switch_core_session_set_private(session, tech_pvt);
	tech_pvt->session = session;
}

/* 
   State methods. They get called when the state changes to the specified state.
   Returning SWITCH_STATUS_SUCCESS tells the core to execute the standard state method next,
   so if you fully implement the state you can return SWITCH_STATUS_FALSE to skip it.
*/
static switch_status_t unicall_on_init(switch_core_session_t *session)
{
	switch_channel_t *channel;
	private_t *tech_pvt;

	tech_pvt = switch_core_session_get_private(session);
	assert(tech_pvt != NULL);
	channel = switch_core_session_get_channel(session);
	assert(channel != NULL);

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "unicall_on_init(%p)\n", (void *) session);

	switch_set_flag_locked(tech_pvt, TFLAG_IO);

	switch_mutex_lock(globals.mutex);
	globals.calls++;
	switch_mutex_unlock(globals.mutex);

	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t unicall_on_routing(switch_core_session_t *session)
{
	switch_channel_t *channel;
	private_t *tech_pvt;

	channel = switch_core_session_get_channel(session);
	assert(channel != NULL);
	tech_pvt = switch_core_session_get_private(session);
	assert(tech_pvt != NULL);

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "unicall_on_routing(%p)\n", (void *) session);

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "%s channel routing\n", switch_channel_get_name(channel));

	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t unicall_on_execute(switch_core_session_t *session)
{
	switch_channel_t *channel;
	private_t *tech_pvt;

	channel = switch_core_session_get_channel(session);
	assert(channel != NULL);
	tech_pvt = switch_core_session_get_private(session);
	assert(tech_pvt != NULL);

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "unicall_on_execute(%p)\n", (void *) session);

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "%s channel execute\n", switch_channel_get_name(channel));


	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t unicall_on_destroy(switch_core_session_t *session)
{
	//switch_channel_t *channel;
	private_t *tech_pvt;

	//channel = switch_core_session_get_channel(session);
	//assert(channel != NULL);

	tech_pvt = switch_core_session_get_private(session);

	if (tech_pvt) {
		if (switch_core_codec_ready(&tech_pvt->read_codec))
			switch_core_codec_destroy(&tech_pvt->read_codec);
		if (switch_core_codec_ready(&tech_pvt->write_codec))
			switch_core_codec_destroy(&tech_pvt->write_codec);
	}

	return SWITCH_STATUS_SUCCESS;

}

static switch_status_t unicall_on_hangup(switch_core_session_t *session)
{
	switch_channel_t *channel;
	private_t *tech_pvt;

	channel = switch_core_session_get_channel(session);
	assert(channel != NULL);
	tech_pvt = switch_core_session_get_private(session);
	assert(tech_pvt != NULL);

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "unicall_on_hangup(%p)\n", (void *) session);

	switch_clear_flag_locked(tech_pvt, TFLAG_IO);
	switch_clear_flag_locked(tech_pvt, TFLAG_VOICE);
	//switch_thread_cond_signal(tech_pvt->cond);


	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "%s channel hangup\n", switch_channel_get_name(channel));

#if 0
	if ((ret = uc_call_control(uc, UC_OP_DROPCALL, crn, (void *) (intptr_t) switch_channel_get_cause(channel))) < 0) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "Drop call failed - %s\n", uc_ret_to_str(ret));
		return SWITCH_STATUS_FAILED;
	}
	/*endif */
#endif

	switch_mutex_lock(globals.mutex);
	if (--globals.calls < 0)
		globals.calls = 0;
	switch_mutex_unlock(globals.mutex);

	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t unicall_kill_channel(switch_core_session_t *session, int sig)
{
	switch_channel_t *channel = NULL;
	private_t *tech_pvt = NULL;

	channel = switch_core_session_get_channel(session);
	assert(channel != NULL);
	tech_pvt = switch_core_session_get_private(session);
	assert(tech_pvt != NULL);

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "unicall_kill_channel(%p, %d)\n", (void *) session, sig);

	switch (sig) {
	case SWITCH_SIG_KILL:
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "unicall_kill_channel(%p, %d) SIG_KILL\n", (void *) session, sig);
		switch_clear_flag_locked(tech_pvt, TFLAG_IO);
		switch_clear_flag_locked(tech_pvt, TFLAG_VOICE);
		switch_channel_hangup(channel, SWITCH_CAUSE_NORMAL_CLEARING);
		//switch_thread_cond_signal(tech_pvt->cond);
		break;
	case SWITCH_SIG_BREAK:
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "unicall_kill_channel(%p, %d) SIG_BREAK\n", (void *) session, sig);
		switch_set_flag_locked(tech_pvt, TFLAG_BREAK);
		break;
	default:
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "unicall_kill_channel(%p, %d) DEFAULT\n", (void *) session, sig);
		break;
	}

	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t unicall_on_exchange_media(switch_core_session_t *session)
{
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "unicall_on_exchange_media(%p)\n", (void *) session);

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "CHANNEL LOOPBACK\n");
	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t unicall_on_soft_execute(switch_core_session_t *session)
{
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "unicall_on_soft_execute(%p)\n", (void *) session);

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "CHANNEL TRANSMIT\n");
	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t unicall_send_dtmf(switch_core_session_t *session, const switch_dtmf_t *dtmf)
{
	private_t *tech_pvt = switch_core_session_get_private(session);
	switch_assert(tech_pvt != NULL);

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "unicall_send_dtmf(%p)\n", (void *) session);

	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t unicall_read_frame(switch_core_session_t *session, switch_frame_t **frame, switch_io_flag_t flags, int stream_id)
{
	switch_channel_t *channel = NULL;
	private_t *tech_pvt = NULL;
	//switch_time_t started = switch_time_now();
	//unsigned int elapsed;
	switch_byte_t *data;

	channel = switch_core_session_get_channel(session);
	assert(channel != NULL);
	tech_pvt = switch_core_session_get_private(session);
	assert(tech_pvt != NULL);

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "unicall_read_frame(%p)\n", (void *) session);

	tech_pvt->read_frame.flags = SFF_NONE;
	*frame = NULL;

	while (switch_test_flag(tech_pvt, TFLAG_IO)) {
		if (switch_test_flag(tech_pvt, TFLAG_BREAK)) {
			switch_clear_flag(tech_pvt, TFLAG_BREAK);
			data = (switch_byte_t *) tech_pvt->read_frame.data;
			data[0] = 65;
			data[1] = 0;
			tech_pvt->read_frame.datalen = 2;
			tech_pvt->read_frame.flags = SFF_CNG;
			*frame = &tech_pvt->read_frame;
			return SWITCH_STATUS_SUCCESS;
		}

		if (!switch_test_flag(tech_pvt, TFLAG_IO))
			return SWITCH_STATUS_FALSE;

		if (switch_test_flag(tech_pvt, TFLAG_IO) && switch_test_flag(tech_pvt, TFLAG_VOICE)) {
			switch_clear_flag_locked(tech_pvt, TFLAG_VOICE);
			if (!tech_pvt->read_frame.datalen)
				continue;
			*frame = &tech_pvt->read_frame;
#if SWITCH_BYTE_ORDER == __BIG_ENDIAN
			if (switch_test_flag(tech_pvt, TFLAG_LINEAR))
				switch_swap_linear((*frame)->data, (int) (*frame)->datalen / 2);
#endif
			return SWITCH_STATUS_SUCCESS;
		}

		switch_cond_next();
	}

	return SWITCH_STATUS_FALSE;
}

static switch_status_t unicall_write_frame(switch_core_session_t *session, switch_frame_t *frame, switch_io_flag_t flags, int stream_id)
{
	switch_channel_t *channel = NULL;
	private_t *tech_pvt = NULL;
	//switch_frame_t *pframe;

	channel = switch_core_session_get_channel(session);
	assert(channel != NULL);
	tech_pvt = switch_core_session_get_private(session);
	assert(tech_pvt != NULL);

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "unicall_write_frame(%p)\n", (void *) session);

	if (!switch_test_flag(tech_pvt, TFLAG_IO))
		return SWITCH_STATUS_FALSE;
#if SWITCH_BYTE_ORDER == __BIG_ENDIAN
	if (switch_test_flag(tech_pvt, TFLAG_LINEAR))
		switch_swap_linear(frame->data, (int) frame->datalen / sizeof(int16_t));
#endif

	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t redirect_audio(switch_core_session_t *session, switch_core_session_message_t *msg)
{
	switch_channel_t *channel;
	private_t *tech_pvt;
	uc_t *uc;

	channel = switch_core_session_get_channel(session);
	assert(channel != NULL);
	tech_pvt = switch_core_session_get_private(session);
	assert(tech_pvt != NULL);
	uc = tech_pvt->uc;
	assert(uc != NULL);

	switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_INFO, "redirect_audio(%p)\n", (void *) session);

	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t transmit_text(switch_core_session_t *session, switch_core_session_message_t *msg)
{
	switch_channel_t *channel;
	private_t *tech_pvt;
	uc_t *uc;

	channel = switch_core_session_get_channel(session);
	assert(channel != NULL);
	tech_pvt = switch_core_session_get_private(session);
	assert(tech_pvt != NULL);
	uc = tech_pvt->uc;
	assert(uc != NULL);

	switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_INFO, "transmit_text(%p)\n", (void *) session);

	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t answer(switch_core_session_t *session, switch_core_session_message_t *msg)
{
	switch_channel_t *channel;
	private_t *tech_pvt;
	uc_t *uc;

	channel = switch_core_session_get_channel(session);
	assert(channel != NULL);
	tech_pvt = switch_core_session_get_private(session);
	assert(tech_pvt != NULL);
	uc = tech_pvt->uc;
	assert(uc != NULL);

	switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_INFO, "answer(%p)\n", (void *) session);

#if 0
	if ((ret = uc_call_control(uc, UC_OP_ANSWERCALL, crn, NULL)) < 0) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_INFO, "Answer call failed - %s\n", uc_ret_to_str(ret));
		return SWITCH_STATUS_FAILED;
	}
	/*endif */
#endif

	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t progress(switch_core_session_t *session, switch_core_session_message_t *msg)
{
	switch_channel_t *channel;
	private_t *tech_pvt;
	uc_t *uc;

	channel = switch_core_session_get_channel(session);
	assert(channel != NULL);
	tech_pvt = switch_core_session_get_private(session);
	assert(tech_pvt != NULL);
	uc = tech_pvt->uc;
	assert(uc != NULL);

	switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_INFO, "progress(%p)\n", (void *) session);

#if 0
	if ((ret = uc_call_control(uc, UC_OP_ACCEPTCALL, crn, NULL)) < 0) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_INFO, "Accept call failed - %s\n", uc_ret_to_str(ret));
		return SWITCH_STATUS_FAILED;
	}
	/*endif */
#endif

	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t bridge(switch_core_session_t *session, switch_core_session_message_t *msg)
{
	switch_channel_t *channel;
	private_t *tech_pvt;
	uc_t *uc;

	channel = switch_core_session_get_channel(session);
	assert(channel != NULL);
	tech_pvt = switch_core_session_get_private(session);
	assert(tech_pvt != NULL);
	uc = tech_pvt->uc;
	assert(uc != NULL);

	switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_INFO, "bridge(%p)\n", (void *) session);

	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t unbridge(switch_core_session_t *session, switch_core_session_message_t *msg)
{
	switch_channel_t *channel;
	private_t *tech_pvt;
	uc_t *uc;

	channel = switch_core_session_get_channel(session);
	assert(channel != NULL);
	tech_pvt = switch_core_session_get_private(session);
	assert(tech_pvt != NULL);
	uc = tech_pvt->uc;
	assert(uc != NULL);

	switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_INFO, "unbridge(%p)\n", (void *) session);

	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t transfer(switch_core_session_t *session, switch_core_session_message_t *msg)
{
	switch_channel_t *channel;
	private_t *tech_pvt;
	uc_t *uc;

	channel = switch_core_session_get_channel(session);
	assert(channel != NULL);
	tech_pvt = switch_core_session_get_private(session);
	assert(tech_pvt != NULL);
	uc = tech_pvt->uc;
	assert(uc != NULL);

	switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_INFO, "transfer(%p)\n", (void *) session);

	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t ringing(switch_core_session_t *session, switch_core_session_message_t *msg)
{
	switch_channel_t *channel;
	private_t *tech_pvt;
	uc_t *uc;

	channel = switch_core_session_get_channel(session);
	assert(channel != NULL);
	tech_pvt = switch_core_session_get_private(session);
	assert(tech_pvt != NULL);
	uc = tech_pvt->uc;
	assert(uc != NULL);

	switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_INFO, "ringing(%p)\n", (void *) session);

	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t media(switch_core_session_t *session, switch_core_session_message_t *msg)
{
	switch_channel_t *channel;
	private_t *tech_pvt;
	uc_t *uc;

	channel = switch_core_session_get_channel(session);
	assert(channel != NULL);
	tech_pvt = switch_core_session_get_private(session);
	assert(tech_pvt != NULL);
	uc = tech_pvt->uc;
	assert(uc != NULL);

	switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_INFO, "media(%p)\n", (void *) session);

	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t nomedia(switch_core_session_t *session, switch_core_session_message_t *msg)
{
	switch_channel_t *channel;
	private_t *tech_pvt;
	uc_t *uc;

	channel = switch_core_session_get_channel(session);
	assert(channel != NULL);
	tech_pvt = switch_core_session_get_private(session);
	assert(tech_pvt != NULL);
	uc = tech_pvt->uc;
	assert(uc != NULL);

	switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_INFO, "nomedia(%p)\n", (void *) session);

	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t hold(switch_core_session_t *session, switch_core_session_message_t *msg)
{
	switch_channel_t *channel;
	private_t *tech_pvt;
	uc_t *uc;

	channel = switch_core_session_get_channel(session);
	assert(channel != NULL);
	tech_pvt = switch_core_session_get_private(session);
	assert(tech_pvt != NULL);
	uc = tech_pvt->uc;
	assert(uc != NULL);

	switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_INFO, "hold(%p)\n", (void *) session);

	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t unhold(switch_core_session_t *session, switch_core_session_message_t *msg)
{
	switch_channel_t *channel;
	private_t *tech_pvt;
	uc_t *uc;

	channel = switch_core_session_get_channel(session);
	assert(channel != NULL);
	tech_pvt = switch_core_session_get_private(session);
	assert(tech_pvt != NULL);
	uc = tech_pvt->uc;
	assert(uc != NULL);

	switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_INFO, "unhold(%p)\n", (void *) session);

	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t redirect(switch_core_session_t *session, switch_core_session_message_t *msg)
{
	switch_channel_t *channel;
	private_t *tech_pvt;
	uc_t *uc;

	channel = switch_core_session_get_channel(session);
	assert(channel != NULL);
	tech_pvt = switch_core_session_get_private(session);
	assert(tech_pvt != NULL);
	uc = tech_pvt->uc;
	assert(uc != NULL);

	switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_INFO, "redirect(%p)\n", (void *) session);

	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t respond(switch_core_session_t *session, switch_core_session_message_t *msg)
{
	switch_channel_t *channel;
	private_t *tech_pvt;
	uc_t *uc;

	channel = switch_core_session_get_channel(session);
	assert(channel != NULL);
	tech_pvt = switch_core_session_get_private(session);
	assert(tech_pvt != NULL);
	uc = tech_pvt->uc;
	assert(uc != NULL);

	switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_INFO, "respond(%p)\n", (void *) session);

	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t broadcast(switch_core_session_t *session, switch_core_session_message_t *msg)
{
	switch_channel_t *channel;
	private_t *tech_pvt;
	uc_t *uc;

	channel = switch_core_session_get_channel(session);
	assert(channel != NULL);
	tech_pvt = switch_core_session_get_private(session);
	assert(tech_pvt != NULL);
	uc = tech_pvt->uc;
	assert(uc != NULL);

	switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_INFO, "broadcast(%p)\n", (void *) session);

	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t media_redirect(switch_core_session_t *session, switch_core_session_message_t *msg)
{
	switch_channel_t *channel;
	private_t *tech_pvt;
	uc_t *uc;

	channel = switch_core_session_get_channel(session);
	assert(channel != NULL);
	tech_pvt = switch_core_session_get_private(session);
	assert(tech_pvt != NULL);
	uc = tech_pvt->uc;
	assert(uc != NULL);

	switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_INFO, "media_redirect(%p)\n", (void *) session);

	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t deflect(switch_core_session_t *session, switch_core_session_message_t *msg)
{
	switch_channel_t *channel;
	private_t *tech_pvt;
	uc_t *uc;

	channel = switch_core_session_get_channel(session);
	assert(channel != NULL);
	tech_pvt = switch_core_session_get_private(session);
	assert(tech_pvt != NULL);
	uc = tech_pvt->uc;
	assert(uc != NULL);

	switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_INFO, "deflect(%p)\n", (void *) session);

	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t video_refresh_req(switch_core_session_t *session, switch_core_session_message_t *msg)
{
	switch_channel_t *channel;
	private_t *tech_pvt;
	uc_t *uc;

	channel = switch_core_session_get_channel(session);
	assert(channel != NULL);
	tech_pvt = switch_core_session_get_private(session);
	assert(tech_pvt != NULL);
	uc = tech_pvt->uc;
	assert(uc != NULL);

	switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_INFO, "video_refresh_req(%p)\n", (void *) session);

	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t display(switch_core_session_t *session, switch_core_session_message_t *msg)
{
	switch_channel_t *channel;
	private_t *tech_pvt;
	uc_t *uc;

	channel = switch_core_session_get_channel(session);
	assert(channel != NULL);
	tech_pvt = switch_core_session_get_private(session);
	assert(tech_pvt != NULL);
	uc = tech_pvt->uc;
	assert(uc != NULL);

	switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_INFO, "display(%p)\n", (void *) session);

	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t unicall_receive_message(switch_core_session_t *session, switch_core_session_message_t *msg)
{
	switch (msg->message_id) {
	case SWITCH_MESSAGE_REDIRECT_AUDIO:
		return redirect_audio(session, msg);
	case SWITCH_MESSAGE_TRANSMIT_TEXT:
		return transmit_text(session, msg);
	case SWITCH_MESSAGE_INDICATE_ANSWER:
		return answer(session, msg);
	case SWITCH_MESSAGE_INDICATE_PROGRESS:
		return progress(session, msg);
	case SWITCH_MESSAGE_INDICATE_BRIDGE:
		return bridge(session, msg);
	case SWITCH_MESSAGE_INDICATE_UNBRIDGE:
		return unbridge(session, msg);
	case SWITCH_MESSAGE_INDICATE_TRANSFER:
		return transfer(session, msg);
	case SWITCH_MESSAGE_INDICATE_RINGING:
		return ringing(session, msg);
	case SWITCH_MESSAGE_INDICATE_MEDIA:
		return media(session, msg);
	case SWITCH_MESSAGE_INDICATE_NOMEDIA:
		return nomedia(session, msg);
	case SWITCH_MESSAGE_INDICATE_HOLD:
		return hold(session, msg);
	case SWITCH_MESSAGE_INDICATE_UNHOLD:
		return unhold(session, msg);
	case SWITCH_MESSAGE_INDICATE_REDIRECT:
		return redirect(session, msg);
	case SWITCH_MESSAGE_INDICATE_RESPOND:
		return respond(session, msg);
	case SWITCH_MESSAGE_INDICATE_BROADCAST:
		return broadcast(session, msg);
	case SWITCH_MESSAGE_INDICATE_MEDIA_REDIRECT:
		return media_redirect(session, msg);
	case SWITCH_MESSAGE_INDICATE_DEFLECT:
		return deflect(session, msg);
	case SWITCH_MESSAGE_INDICATE_VIDEO_REFRESH_REQ:
		return video_refresh_req(session, msg);
	case SWITCH_MESSAGE_INDICATE_DISPLAY:
		return display(session, msg);
	default:
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_INFO, "unicall_receive_message(%p) %d\n", (void *) session, msg->message_id);
		break;
	}

	return SWITCH_STATUS_SUCCESS;
}

/* Make sure when you have 2 sessions in the same scope that you pass the appropriate one to the routines
   that allocate memory or you will have 1 channel with memory allocated from another channel's pool!
*/
static switch_call_cause_t unicall_outgoing_channel(switch_core_session_t *session,
													switch_event_t *var_event,
													switch_caller_profile_t *outbound_profile,
													switch_core_session_t **new_session,
													switch_memory_pool_t **pool, switch_originate_flag_t flags, switch_call_cause_t *cancel_cause)
{
	private_t *tech_pvt;
	switch_channel_t *channel;
	switch_caller_profile_t *caller_profile;
	uc_t *uc;
	uc_makecall_t makecall;
	uc_callparms_t *callparms;
	int screen;
	int hide;
	char name[128];

	switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_INFO, "unicall_outgoing_channel(%p)\n", (void *) session);

	if ((*new_session = switch_core_session_request(unicall_endpoint_interface, SWITCH_CALL_DIRECTION_OUTBOUND, flags, pool)) == NULL) {
		return SWITCH_CAUSE_DESTINATION_OUT_OF_ORDER;
	}

	switch_core_session_add_stream(*new_session, NULL);
	if ((tech_pvt = (private_t *) switch_core_session_alloc(*new_session, sizeof(private_t))) == NULL) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_CRIT, "Hey where is my memory pool?\n");
		switch_core_session_destroy(new_session);
		return SWITCH_CAUSE_DESTINATION_OUT_OF_ORDER;
	}
	channel = switch_core_session_get_channel(*new_session);
	tech_init(tech_pvt, *new_session);

	if (outbound_profile == NULL) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "Doh! no caller profile\n");
		switch_core_session_destroy(new_session);
		return SWITCH_CAUSE_DESTINATION_OUT_OF_ORDER;
	}
	snprintf(name, sizeof(name), "UNICALL/%s", outbound_profile->destination_number);
	switch_channel_set_name(channel, name);

	caller_profile = switch_caller_profile_clone(*new_session, outbound_profile);
	switch_channel_set_caller_profile(channel, caller_profile);
	tech_pvt->caller_profile = caller_profile;

	uc = tech_pvt->uc;

	if ((callparms = uc_new_callparms(NULL)) == NULL)
		return SWITCH_CAUSE_DESTINATION_OUT_OF_ORDER;

	//uc_callparm_bear_cap_transfer_cap(callparms, cap);
	//uc_callparm_bear_cap_transfer_mode(callparms, mode);
	//uc_callparm_bear_cap_transfer_rate(callparms, rate);
	//uc_callparm_userinfo_layer1_protocol(callparms, prot);
	//uc_callparm_user_rate(callparms, rate);

	switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(*new_session), SWITCH_LOG_INFO, "destination '%s'\n", outbound_profile->destination_number);
	uc_callparm_set_destination_number(callparms, outbound_profile->destination_number);
	uc_callparm_set_destination_ton(callparms, outbound_profile->destination_number_ton);
	uc_callparm_set_destination_npi(callparms, outbound_profile->destination_number_numplan);
	//uc_callparm_set_destination_sub_addr_number(callparms, num);
	//uc_callparm_set_destination_sub_addr_ton(callparms, ton);
	//uc_callparm_set_destination_sub_addr_npi(callparms, npi);

	//uc_callparm_set_redirecting_cause(callparms, cause);
	//uc_callparm_set_redirecting_presentation(callparms, pres);
	uc_callparm_set_redirecting_number(callparms, outbound_profile->rdnis);
	uc_callparm_set_redirecting_ton(callparms, outbound_profile->rdnis_ton);
	uc_callparm_set_redirecting_npi(callparms, outbound_profile->rdnis_numplan);
	//uc_callparm_set_redirecting_subaddr(callparms, num);
	//uc_callparm_set_redirecting_subaddr_ton(callparms, ton);
	//uc_callparm_set_redirecting_subaddr_npi(callparms, npi);

	//uc_callparm_set_original_called_number(callparms, num);
	//uc_callparm_set_original_called_number_ton(callparms, ton);
	//uc_callparm_set_original_called_number_npi(callparms, npi);

	switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(*new_session), SWITCH_LOG_INFO, "caller id name '%s'\n", outbound_profile->caller_id_name);
	switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(*new_session), SWITCH_LOG_INFO, "caller id number '%s'\n", outbound_profile->caller_id_number);
	uc_callparm_set_originating_name(callparms, outbound_profile->caller_id_name);
	uc_callparm_set_originating_number(callparms, outbound_profile->caller_id_number);
	screen = switch_test_flag(outbound_profile, SWITCH_CPF_SCREEN);
	hide = switch_test_flag(outbound_profile, SWITCH_CPF_HIDE_NUMBER);
	if (!screen && !hide)
		uc_callparm_set_originating_presentation(callparms, UC_PRES_ALLOWED_USER_NUMBER_NOT_SCREENED);
	else if (!screen && hide)
		uc_callparm_set_originating_presentation(callparms, UC_PRES_PROHIB_USER_NUMBER_NOT_SCREENED);
	else if (screen && !hide)
		uc_callparm_set_originating_presentation(callparms, UC_PRES_ALLOWED_USER_NUMBER_PASSED_SCREEN);
	else
		uc_callparm_set_originating_presentation(callparms, UC_PRES_PROHIB_USER_NUMBER_PASSED_SCREEN);
	uc_callparm_set_originating_ton(callparms, outbound_profile->caller_ton);
	uc_callparm_set_originating_npi(callparms, outbound_profile->caller_numplan);
	//uc_callparm_set_originating_sub_addr_number(callparms, num);
	//uc_callparm_set_originating_sub_addr_ton(callparms, ton);
	//uc_callparm_set_originating_sub_addr_npi(callparms, npi);

	uc_callparm_set_calling_party_category(callparms, UC_CALLER_CATEGORY_NATIONAL_SUBSCRIBER_CALL);

	makecall.callparms = callparms;
	makecall.call = NULL;

#if 0
	if ((ret = uc_call_control(uc, UC_OP_MAKECALL, 0, (void *) &makecall)) < 0) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(*new_session), SWITCH_LOG_INFO, "Make call failed - %s\n", uc_ret_to_str(ret));
		return SWITCH_STATUS_FAILED;
	}
	/*endif */
#endif
	free(callparms);


	switch_channel_set_state(channel, CS_INIT);
	switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(*new_session), SWITCH_LOG_INFO, "unicall_outgoing_channel(%p) SUCCESS\n", (void *) session);
	return SWITCH_CAUSE_SUCCESS;
}

static switch_status_t unicall_receive_event(switch_core_session_t *session, switch_event_t *event)
{
	struct private_object *tech_pvt = switch_core_session_get_private(session);
	char *body = switch_event_get_body(event);

	switch_assert(tech_pvt != NULL);

	switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_INFO, "unicall_receive_event(%p)\n", (void *) session);

	if (body == NULL)
		body = "";

	return SWITCH_STATUS_SUCCESS;
}

static void *SWITCH_THREAD_FUNC unicall_thread_run(switch_thread_t *thread, void *obj)
{
	fd_set read;
	fd_set write;
	fd_set oob;
	int fd;
	int ret;
	switch_event_t *s_event;
	uc_t *uc = (uc_t *) obj;

#if 0
	switch_mutex_init(&chanmap.mutex, SWITCH_MUTEX_NESTED, module_pool);
#endif

	if (switch_event_create(&s_event, SWITCH_EVENT_PUBLISH) == SWITCH_STATUS_SUCCESS) {
		switch_event_add_header(s_event, SWITCH_STACK_BOTTOM, "service", "_uc._tcp");
		switch_event_fire(&s_event);
	}

	uc_get_device_handle(uc, 0, &fd);
	if ((ret = uc_call_control(uc, UC_OP_UNBLOCK, 0, (void *) (intptr_t) - 1)) < 0)
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "Unblock failed - %s\n", uc_ret_to_str(ret));
	/*endif */
	FD_ZERO(&read);
	FD_ZERO(&write);
	FD_ZERO(&oob);
	for (;;) {
		FD_SET(fd, &read);
		//FD_SET(fd, &write);
		//FD_SET(fd, &oob);

		if (select(fd + 1, &read, NULL, NULL, NULL)) {
			if (FD_ISSET(fd, &read)) {
				uc_check_event(uc);
				uc_schedule_run(uc);
			}
		}
	}

	uc_delete(uc);
	return NULL;
}

static void unicall_thread_launch(uc_t *uc)
{
	switch_thread_t *thread;
	switch_threadattr_t *thd_attr = NULL;

	switch_threadattr_create(&thd_attr, module_pool);
	switch_threadattr_detach_set(thd_attr, 1);
	switch_threadattr_stacksize_set(thd_attr, SWITCH_THREAD_STACKSIZE);
	switch_thread_create(&thread, thd_attr, unicall_thread_run, uc, module_pool);
}

switch_state_handler_table_t unicall_state_handlers = {
	/*.on_init */ unicall_on_init,
	/*.on_routing */ unicall_on_routing,
	/*.on_execute */ unicall_on_execute,
	/*.on_hangup */ unicall_on_hangup,
	/*.on_exchange_media */ unicall_on_exchange_media,
	/*.on_soft_execute */ unicall_on_soft_execute,
	/*.on_consume_media */ NULL,
	/*.on_hibernate */ NULL,
	/*.on_reset */ NULL,
	/*.on_park */ NULL,
	/*.on_reporting */ NULL,
	/*.on_destroy */ unicall_on_destroy
};

switch_io_routines_t unicall_io_routines = {
	/*.outgoing_channel */ unicall_outgoing_channel,
	/*.read_frame */ unicall_read_frame,
	/*.write_frame */ unicall_write_frame,
	/*.kill_channel */ unicall_kill_channel,
	/*.send_dtmf */ unicall_send_dtmf,
	/*.receive_message */ unicall_receive_message,
	/*.receive_event */ unicall_receive_event
};

static switch_status_t config_unicall(int reload)
{
	const char *cf = "unicall.conf";
	switch_xml_t cfg;
	switch_xml_t xml;
	switch_xml_t settings;
	switch_xml_t param;
	switch_xml_t spans;
	switch_xml_t span;
	int current_span = 0;
	int min_span = 0;
	int max_span = 0;
	int logging_level;
	int i;
	char *id;
	span_data_t *sp;

	if (!(xml = switch_xml_open_cfg(cf, &cfg, NULL))) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Opening of %s failed\n", cf);
		return SWITCH_STATUS_TERM;
	}

	if ((settings = switch_xml_child(cfg, "settings"))) {
		for (param = switch_xml_child(settings, "param"); param; param = param->next) {
			char *var = (char *) switch_xml_attr_soft(param, "name");
			char *val = (char *) switch_xml_attr_soft(param, "value");

			if (!strcmp(var, "debug")) {
				globals.debug = atoi(val);
			} else if (!strcmp(var, "ms-per-frame")) {
				globals.frame_duration = atoi(val);
			} else if (!strcmp(var, "dtmf-on")) {
				globals.dtmf_on = atoi(val);
			} else if (!strcmp(var, "dtmf-off")) {
				globals.dtmf_off = atoi(val);
			} else if (!strcmp(var, "dialplan")) {
				set_global_dialplan(val);
			} else if (!strcmp(var, "suppress-dtmf-tone")) {
				globals.suppress_dtmf_tone = switch_true(val);
			} else if (!strcmp(var, "ignore-dtmf-tone")) {
				globals.ignore_dtmf_tone = switch_true(val);
			}
		}
	}
	spans = switch_xml_child(cfg, "spans");
	id = NULL;
	for (span = switch_xml_child(spans, "span"); span; span = span->next) {
		id = (char *) switch_xml_attr(span, "id");

		current_span = 0;

		if (id) {
			char *p;

			min_span = atoi(id);
			if ((p = strchr(id, '-'))) {
				p++;
				max_span = atoi(p);
			} else {
				max_span = min_span;
			}
			if (min_span < 1 || max_span < min_span) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "Invalid Span Config! [%s]\n", id);
				continue;
			}
		} else {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "Missing SPAN ID!\n");
			continue;
		}
		for (i = min_span; i <= max_span; i++) {
			current_span = i;

			if (current_span <= 0 || current_span > MAX_SPANS) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "Invalid SPAN %d!\n", current_span);
				current_span = 0;
				continue;
			}
			if (span_data[current_span] == NULL) {
				if ((span_data[current_span] = switch_core_alloc(module_pool, sizeof(*span_data[current_span]))) == NULL) {
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "MEMORY ERROR\n");
					break;
				}
				sp = span_data[current_span];
				memset(sp, 0, sizeof(*sp));
				sp->span = current_span;
				sp->protocol_class = NULL;
				sp->protocol_variant = NULL;
				sp->protocol_end = UC_MODE_CPE;
				sp->outgoing_ok = TRUE;
			}
			sp = span_data[current_span];
			sp->id = strdup(id);
			for (param = switch_xml_child(span, "param"); param; param = param->next) {
				char *var = (char *) switch_xml_attr_soft(param, "name");
				char *val = (char *) switch_xml_attr_soft(param, "value");

				if (strcmp(var, "protocol-class") == 0) {
					sp->protocol_class = strdup(val);
				} else if (strcmp(var, "protocol-variant") == 0) {
					sp->protocol_variant = strdup(val);
				} else if (strcmp(var, "protocol-end") == 0) {
					if (strcasecmp(val, "co") == 0)
						sp->protocol_end = UC_MODE_CO;
					else if (strcasecmp(val, "cpe") == 0)
						sp->protocol_end = UC_MODE_CPE;
					else if (strcasecmp(val, "peer") == 0)
						sp->protocol_end = UC_MODE_PEER;
					else
						switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "UNKNOWN protocol-end TYPE %s\n", val);
				} else if (strcmp(var, "outgoing-allowed") == 0) {
					sp->outgoing_ok = switch_true(var);
				} else if (strcmp(var, "dialplan") == 0) {
					sp->dialplan = strdup(val);
				} else if (strcmp(var, "context") == 0) {
					sp->context = strdup(val);
				} else {
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "UNKNOWN PARAMETER %s\n", var);
				}
			}
		}
	}
	switch_xml_free(xml);

	if (globals.dialplan == NULL)
		set_global_dialplan("XML");

	globals.configured_spans = 0;
	for (current_span = 1; current_span < MAX_SPANS; current_span++) {
		if (span_data[current_span]) {
			sp = span_data[current_span];
			if ((sp->uc = uc_create(sp->id, sp->protocol_class, sp->protocol_variant, sp->protocol_end, sp->outgoing_ok)) == NULL) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "Cannot launch span %d\n", current_span);
				return SWITCH_STATUS_FALSE;
			}
			uc_get_device_handle(sp->uc, 0, &sp->fd);
			uc_set_signaling_callback(sp->uc, handle_uc_event, (void *) (intptr_t) current_span);
			logging_level = UC_LOG_SEVERITY_MASK | UC_LOG_SHOW_TAG | UC_LOG_SHOW_PROTOCOL;
			uc_set_logging(sp->uc, logging_level, 1, sp->id);

			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "Launched span %d\n", current_span);
			unicall_thread_launch(sp->uc);
			switch_mutex_lock(globals.hash_mutex);
			globals.configured_spans++;
			switch_mutex_unlock(globals.hash_mutex);
		}
	}
	return SWITCH_STATUS_SUCCESS;
}

SWITCH_MODULE_LOAD_FUNCTION(mod_unicall_load)
{
	int logging_level;
	switch_status_t status = SWITCH_STATUS_SUCCESS;

	uc_start();
	uc_set_error_handler(unicall_report);
	uc_set_message_handler(unicall_message);
	logging_level = UC_LOG_SEVERITY_MASK;
	uc_set_logging(NULL, logging_level, 1, NULL);

	memset(span_data, 0, sizeof(span_data));

	module_pool = pool;

	memset(&globals, 0, sizeof(globals));
	switch_core_hash_init(&globals.call_hash);
	switch_mutex_init(&globals.mutex, SWITCH_MUTEX_NESTED, module_pool);
	switch_mutex_init(&globals.hash_mutex, SWITCH_MUTEX_NESTED, module_pool);
	switch_mutex_init(&globals.channel_mutex, SWITCH_MUTEX_NESTED, module_pool);

	if ((status = config_unicall(FALSE)) != SWITCH_STATUS_SUCCESS)
		return status;

	*module_interface = switch_loadable_module_create_module_interface(pool, modname);
	unicall_endpoint_interface = switch_loadable_module_create_interface(*module_interface, SWITCH_ENDPOINT_INTERFACE);
	unicall_endpoint_interface->interface_name = "unicall";
	unicall_endpoint_interface->io_routines = &unicall_io_routines;
	unicall_endpoint_interface->state_handler = &unicall_state_handlers;

	/* Indicate that the module should continue to be loaded */
	return SWITCH_STATUS_SUCCESS;
}

/*
SWITCH_MODULE_RUNTIME_FUNCTION(mod_unicall_runtime)
{
    return SWITCH_STATUS_TERM;
}
*/

SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_unicall_shutdown)
{
	int x = 0;

	for (x = 0, running = -1; running && x <= 100; x++)
		switch_yield(20000);
	uc_end();
	return SWITCH_STATUS_SUCCESS;
}
