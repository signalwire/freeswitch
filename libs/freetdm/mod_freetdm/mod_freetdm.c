/* 
 * FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
 * Copyright (C) 2005-2011, Anthony Minessale II <anthm@freeswitch.org>
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
 * Moises Silva <moy@sangoma.com>
 * David Yat Sin <dyatsin@sangoma.com>
 *
 *
 * mod_freetdm.c -- FreeTDM Endpoint Module
 *
 */
#include <switch.h>
#include "freetdm.h"

#ifndef __FUNCTION__
#define __FUNCTION__ __SWITCH_FUNC__
#endif

#define FREETDM_LIMIT_REALM "__freetdm"
#define FREETDM_VAR_PREFIX "freetdm_"
#define FREETDM_VAR_PREFIX_LEN (sizeof(FREETDM_VAR_PREFIX)-1) 

SWITCH_MODULE_LOAD_FUNCTION(mod_freetdm_load);
SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_freetdm_shutdown);
SWITCH_MODULE_DEFINITION(mod_freetdm, mod_freetdm_load, mod_freetdm_shutdown, NULL);

switch_endpoint_interface_t *freetdm_endpoint_interface;

static switch_memory_pool_t *module_pool = NULL;

typedef enum {
	ANALOG_OPTION_NONE = 0,
	ANALOG_OPTION_3WAY = (1 << 0),
	ANALOG_OPTION_CALL_SWAP = (1 << 1)
} analog_option_t;

typedef enum {
	FTDM_LIMIT_RESET_ON_TIMEOUT = 0,
	FTDM_LIMIT_RESET_ON_ANSWER = 1
} limit_reset_event_t;

typedef enum {
	TFLAG_IO = (1 << 0),
	TFLAG_DTMF = (1 << 1),
	TFLAG_CODEC = (1 << 2),
	TFLAG_BREAK = (1 << 3),
	TFLAG_HOLD = (1 << 4),
	TFLAG_DEAD = (1 << 5)
} TFLAGS;

static struct {
	int debug;
	char *dialplan;
	char *codec_string;
	char *codec_order[SWITCH_MAX_CODECS];
	int codec_order_last;
	char *codec_rates_string;
	char *codec_rates[SWITCH_MAX_CODECS];
	int codec_rates_last;
	unsigned int flags;
	int fd;
	int calls;
	char hold_music[256];
	switch_mutex_t *mutex;
	analog_option_t analog_options;
	switch_hash_t *ss7_configs;
	int sip_headers;
	uint8_t crash_on_assert;
} globals;

/* private data attached to each fs session */
struct private_object {
	unsigned int flags;
	switch_codec_t read_codec;
	switch_codec_t write_codec;
	switch_frame_t read_frame;
	unsigned char databuf[SWITCH_RECOMMENDED_BUFFER_SIZE];
	switch_frame_t cng_frame;
	unsigned char cng_databuf[SWITCH_RECOMMENDED_BUFFER_SIZE];
	switch_core_session_t *session;
	switch_caller_profile_t *caller_profile;
	unsigned int codec;
	unsigned int codecs;
	unsigned short samprate;
	switch_mutex_t *mutex;
	switch_mutex_t *flag_mutex;
	ftdm_channel_t *ftdmchan;
	uint32_t wr_error;
};

/* private data attached to FTDM channels (only FXS for now) */
typedef struct chan_pvt {
	unsigned int flags;
} chan_pvt_t;

typedef struct private_object private_t;

struct span_config {
	ftdm_span_t *span;
	char dialplan[80];
	char context[80];
	char dial_regex[256];
	char fail_dial_regex[256];
	char hold_music[256];
	char type[256];	
	analog_option_t analog_options;
	const char *limit_backend;
	int limit_calls;
	int limit_seconds;
	limit_reset_event_t limit_reset_event;
	chan_pvt_t pvts[FTDM_MAX_CHANNELS_SPAN];
};

static struct span_config SPAN_CONFIG[FTDM_MAX_SPANS_INTERFACE] = {{0}};

static switch_status_t channel_on_init(switch_core_session_t *session);
static switch_status_t channel_on_hangup(switch_core_session_t *session);
static switch_status_t channel_on_destroy(switch_core_session_t *session);
static switch_status_t channel_on_routing(switch_core_session_t *session);
static switch_status_t channel_on_exchange_media(switch_core_session_t *session);
static switch_status_t channel_on_soft_execute(switch_core_session_t *session);
static switch_call_cause_t channel_outgoing_channel(switch_core_session_t *session, switch_event_t *var_event,
													switch_caller_profile_t *outbound_profile,
													switch_core_session_t **new_session, 
													switch_memory_pool_t **pool,
													switch_originate_flag_t flags, switch_call_cause_t *cancel_cause);
static switch_status_t channel_read_frame(switch_core_session_t *session, switch_frame_t **frame, switch_io_flag_t flags, int stream_id);
static switch_status_t channel_write_frame(switch_core_session_t *session, switch_frame_t *frame, switch_io_flag_t flags, int stream_id);
static switch_status_t channel_kill_channel(switch_core_session_t *session, int sig);
static const char* channel_get_variable(switch_core_session_t *session, switch_event_t *var_event, const char *variable_name);
ftdm_status_t ftdm_channel_from_event(ftdm_sigmsg_t *sigmsg, switch_core_session_t **sp);
void dump_chan(ftdm_span_t *span, uint32_t chan_id, switch_stream_handle_t *stream);
void dump_chan_xml(ftdm_span_t *span, uint32_t chan_id, switch_stream_handle_t *stream);

static switch_core_session_t *ftdm_channel_get_session(ftdm_channel_t *channel, int32_t id)
{
	switch_core_session_t *session = NULL;
	const char *token = ftdm_channel_get_token(channel, id);

	if (!zstr(token)) {
		if (!(session = switch_core_session_locate(token))) {
			ftdm_channel_clear_token(channel, token);
		}
	}

	return session;
}

static const char *ftdm_channel_get_uuid(ftdm_channel_t *channel, int32_t id)
{
	return ftdm_channel_get_token(channel, id);
}

static void stop_hold(switch_core_session_t *session_a, const char *uuid)
{
	switch_core_session_t *session;
	switch_channel_t *channel, *channel_a;
	;

	if (!uuid) {
		return;
	}

	if ((session = switch_core_session_locate(uuid))) {
		channel = switch_core_session_get_channel(session);

		if (switch_channel_test_flag(channel, CF_HOLD)) {
			channel_a = switch_core_session_get_channel(session_a);
			switch_ivr_unhold(session);
			switch_channel_clear_flag(channel_a, CF_SUSPEND);
			switch_channel_clear_flag(channel_a, CF_HOLD);
		} else {
			switch_channel_stop_broadcast(channel);
			switch_channel_wait_for_flag(channel, CF_BROADCAST, SWITCH_FALSE, 2000, NULL);
		}

		switch_core_session_rwunlock(session);
	}
}

static void start_hold(ftdm_channel_t *ftdmchan, switch_core_session_t *session_a, const char *uuid, const char *stream)
{
	switch_core_session_t *session;
	switch_channel_t *channel, *channel_a;
	int32_t spanid = 0;

	if (!uuid) {
		return;
	}
	
	spanid = ftdm_channel_get_span_id(ftdmchan);	
	if ((session = switch_core_session_locate(uuid))) {
		channel = switch_core_session_get_channel(session);
		if (zstr(stream)) {
			if (!strcasecmp(globals.hold_music, "indicate_hold")) {
				stream = "indicate_hold";
			}
			if (!strcasecmp(SPAN_CONFIG[spanid].hold_music, "indicate_hold")) {
				stream = "indicate_hold";
			}
		}

		if (zstr(stream)) {
			stream = switch_channel_get_variable(channel, SWITCH_HOLD_MUSIC_VARIABLE);
		}

		if (zstr(stream)) {
			stream = SPAN_CONFIG[spanid].hold_music;
		}

		if (zstr(stream)) {
			stream = globals.hold_music;
		}
		
		
		if (zstr(stream) && !(stream = switch_channel_get_variable(channel, SWITCH_HOLD_MUSIC_VARIABLE))) {
			stream = globals.hold_music;
		}

		if (!zstr(stream)) {
			if (!strcasecmp(stream, "indicate_hold")) {
				channel_a = switch_core_session_get_channel(session_a);
				switch_ivr_hold_uuid(uuid, NULL, 0);
				switch_channel_set_flag(channel_a, CF_SUSPEND);
				switch_channel_set_flag(channel_a, CF_HOLD);
			} else {
				switch_ivr_broadcast(switch_core_session_get_uuid(session), stream, SMF_ECHO_ALEG | SMF_LOOP);
			}
		}

		switch_core_session_rwunlock(session);
	}
}


static void cycle_foreground(ftdm_channel_t *ftdmchan, int flash, const char *bcast) {
	uint32_t i = 0;
	switch_core_session_t *session;
	switch_channel_t *channel;
	private_t *tech_pvt;
	uint32_t tokencnt = ftdm_channel_get_token_count(ftdmchan);
	

	for (i = 0; i < tokencnt; i++) {
		if ((session = ftdm_channel_get_session(ftdmchan, i))) {
			const char *buuid;
			tech_pvt = switch_core_session_get_private(session);
			channel = switch_core_session_get_channel(session);
			buuid = switch_channel_get_variable(channel, SWITCH_SIGNAL_BOND_VARIABLE);

			
			if (tokencnt  == 1 && flash) {
				if (switch_test_flag(tech_pvt, TFLAG_HOLD)) {
					stop_hold(session, buuid);
					switch_clear_flag_locked(tech_pvt, TFLAG_HOLD);
				} else {
					start_hold(ftdmchan, session, buuid, bcast);
					switch_set_flag_locked(tech_pvt, TFLAG_HOLD);
				}
			} else if (i) {
				start_hold(ftdmchan, session, buuid, bcast);
				switch_set_flag_locked(tech_pvt, TFLAG_HOLD);
			} else {
				stop_hold(session, buuid);
				switch_clear_flag_locked(tech_pvt, TFLAG_HOLD);
				if (!switch_channel_test_flag(channel, CF_ANSWERED)) {
					switch_channel_mark_answered(channel);
				}
			}
			switch_core_session_rwunlock(session);
		}
	}
}




static switch_status_t tech_init(private_t *tech_pvt, switch_core_session_t *session, ftdm_channel_t *ftdmchan)
{
	const char *dname = NULL;
	uint32_t interval = 0, srate = 8000;
	ftdm_codec_t codec;

	tech_pvt->ftdmchan = ftdmchan;
	tech_pvt->read_frame.data = tech_pvt->databuf;
	tech_pvt->read_frame.buflen = sizeof(tech_pvt->databuf);
	tech_pvt->cng_frame.data = tech_pvt->cng_databuf;
	tech_pvt->cng_frame.buflen = sizeof(tech_pvt->cng_databuf);
	tech_pvt->cng_frame.flags = SFF_CNG;
	tech_pvt->cng_frame.codec = &tech_pvt->read_codec;
	memset(tech_pvt->cng_frame.data, 255, tech_pvt->cng_frame.buflen);
	switch_mutex_init(&tech_pvt->mutex, SWITCH_MUTEX_NESTED, switch_core_session_get_pool(session));
	switch_mutex_init(&tech_pvt->flag_mutex, SWITCH_MUTEX_NESTED, switch_core_session_get_pool(session));
	switch_core_session_set_private(session, tech_pvt);
	tech_pvt->session = session;

	if (FTDM_SUCCESS != ftdm_channel_command(ftdmchan, FTDM_COMMAND_GET_INTERVAL, &interval)) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Failed to retrieve channel interval.\n");
		return SWITCH_STATUS_GENERR;
	}

	if (FTDM_SUCCESS != ftdm_channel_command(ftdmchan, FTDM_COMMAND_GET_CODEC, &codec)) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Failed to retrieve channel codec.\n");
		return SWITCH_STATUS_GENERR;
	}

	switch(codec) {
	case FTDM_CODEC_ULAW:
		{
			dname = "PCMU";
		}
		break;
	case FTDM_CODEC_ALAW:
		{
			dname = "PCMA";
		}
		break;
	case FTDM_CODEC_SLIN:
		{
			dname = "L16";
		}
		break;
	default:
		{
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Invalid codec value retrieved from channel, codec value: %d\n", codec);
			return SWITCH_STATUS_GENERR;
		}
	}


	if (switch_core_codec_init(&tech_pvt->read_codec,
							   dname,
							   NULL,
							   srate,
							   interval,
							   1,
							   SWITCH_CODEC_FLAG_ENCODE | SWITCH_CODEC_FLAG_DECODE,
							   NULL, switch_core_session_get_pool(tech_pvt->session)) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Can't load codec?\n");
		return SWITCH_STATUS_GENERR;
	} else {
		if (switch_core_codec_init(&tech_pvt->write_codec,
								   dname,
								   NULL,
								   srate,
								   interval,
								   1,
								   SWITCH_CODEC_FLAG_ENCODE | SWITCH_CODEC_FLAG_DECODE,
								   NULL, switch_core_session_get_pool(tech_pvt->session)) != SWITCH_STATUS_SUCCESS) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Can't load codec?\n");
			switch_core_codec_destroy(&tech_pvt->read_codec);
			return SWITCH_STATUS_GENERR;
		}
	}

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Set codec %s %dms\n", dname, interval);
	switch_core_session_set_read_codec(tech_pvt->session, &tech_pvt->read_codec);
	switch_core_session_set_write_codec(tech_pvt->session, &tech_pvt->write_codec);
	switch_set_flag_locked(tech_pvt, TFLAG_CODEC);
	tech_pvt->read_frame.codec = &tech_pvt->read_codec;
	switch_set_flag_locked(tech_pvt, TFLAG_IO);

	return SWITCH_STATUS_SUCCESS;
	
}

static switch_status_t channel_on_init(switch_core_session_t *session)
{
	switch_channel_t *channel;
	private_t *tech_pvt = NULL;

	tech_pvt = switch_core_session_get_private(session);
	assert(tech_pvt != NULL);

	channel = switch_core_session_get_channel(session);
	assert(channel != NULL);

	if (switch_test_flag(tech_pvt, TFLAG_DEAD)) {
		switch_channel_hangup(channel, SWITCH_CAUSE_LOSE_RACE);
		return SWITCH_STATUS_SUCCESS;
	} 
	
	/* Move channel's state machine to ROUTING */
	switch_channel_set_state(channel, CS_ROUTING);
	switch_mutex_lock(globals.mutex);
	globals.calls++;
	switch_mutex_unlock(globals.mutex);

	//switch_channel_set_flag(channel, CF_ACCEPT_CNG);

	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t channel_on_routing(switch_core_session_t *session)
{
	switch_channel_t *channel = NULL;
	private_t *tech_pvt = NULL;

	channel = switch_core_session_get_channel(session);
	switch_assert(channel != NULL);

	tech_pvt = switch_core_session_get_private(session);
	switch_assert(tech_pvt != NULL);

	switch_assert(tech_pvt->ftdmchan != NULL);

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "%s CHANNEL ROUTING\n", switch_channel_get_name(channel));

	if (switch_channel_direction(channel) == SWITCH_CALL_DIRECTION_INBOUND) {
		ftdm_channel_call_indicate(tech_pvt->ftdmchan, FTDM_CHANNEL_INDICATE_PROCEED);
	}
	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t channel_on_execute(switch_core_session_t *session)
{

	switch_channel_t *channel = NULL;
	private_t *tech_pvt = NULL;

	channel = switch_core_session_get_channel(session);
	switch_assert(channel != NULL);

	tech_pvt = switch_core_session_get_private(session);
	switch_assert(tech_pvt != NULL);

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "%s CHANNEL EXECUTE\n", switch_channel_get_name(channel));


	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t channel_on_destroy(switch_core_session_t *session)
{
	private_t *tech_pvt = NULL;
	
	if ((tech_pvt = switch_core_session_get_private(session))) {

		if (tech_pvt->read_codec.implementation) {
			switch_core_codec_destroy(&tech_pvt->read_codec);
		}
		
		if (tech_pvt->write_codec.implementation) {
			switch_core_codec_destroy(&tech_pvt->write_codec);
		}
	}

	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t channel_on_hangup(switch_core_session_t *session)
{
	switch_channel_t *channel = NULL;
	private_t *tech_pvt = NULL;
	ftdm_chan_type_t chantype;
	uint32_t tokencnt;

	channel = switch_core_session_get_channel(session);
	assert(channel != NULL);

	tech_pvt = switch_core_session_get_private(session);
	assert(tech_pvt != NULL);

	if (!tech_pvt->ftdmchan) {
		goto end;
	} 

	ftdm_channel_clear_token(tech_pvt->ftdmchan, switch_core_session_get_uuid(session));

	chantype = ftdm_channel_get_type(tech_pvt->ftdmchan);	
	switch (chantype) {
	case FTDM_CHAN_TYPE_FXO:
	case FTDM_CHAN_TYPE_EM:
		{
			ftdm_channel_call_hangup(tech_pvt->ftdmchan);
		}
		break;
	case FTDM_CHAN_TYPE_FXS:
		{
			if (!ftdm_channel_call_check_busy(tech_pvt->ftdmchan) && !ftdm_channel_call_check_done(tech_pvt->ftdmchan)) {
				tokencnt = ftdm_channel_get_token_count(tech_pvt->ftdmchan);
				if (tokencnt) {
					cycle_foreground(tech_pvt->ftdmchan, 0, NULL);
				} else {
					ftdm_channel_call_hangup(tech_pvt->ftdmchan);
				}
			}
		}
		break;
	case FTDM_CHAN_TYPE_CAS:
	case FTDM_CHAN_TYPE_B:
		{
			ftdm_call_cause_t hcause = switch_channel_get_cause_q850(channel);
			if (hcause  < 1 || hcause > 127) {
				hcause = FTDM_CAUSE_DESTINATION_OUT_OF_ORDER;
			}
			ftdm_channel_call_hangup_with_cause(tech_pvt->ftdmchan, hcause);
		}
		break;
	default: 
		{
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Unhandled channel type %d for channel %s\n", chantype, switch_channel_get_name(channel));
		}
		break;
	}

 end:

	switch_clear_flag_locked(tech_pvt, TFLAG_IO);
	
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "%s CHANNEL HANGUP\n", switch_channel_get_name(channel));
	switch_mutex_lock(globals.mutex);
	globals.calls--;
	if (globals.calls < 0) {
		globals.calls = 0;
	}
	switch_mutex_unlock(globals.mutex);

	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t channel_kill_channel(switch_core_session_t *session, int sig)
{
	switch_channel_t *channel = NULL;
	private_t *tech_pvt = NULL;

	channel = switch_core_session_get_channel(session);
	assert(channel != NULL);

	tech_pvt = switch_core_session_get_private(session);
	assert(tech_pvt != NULL);

	switch (sig) {
	case SWITCH_SIG_KILL:
		switch_clear_flag_locked(tech_pvt, TFLAG_IO);
		switch_set_flag_locked(tech_pvt, TFLAG_DEAD);
		break;
	case SWITCH_SIG_BREAK:
		switch_set_flag_locked(tech_pvt, TFLAG_BREAK);
		break;
	default:
		break;
	}

	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t channel_on_exchange_media(switch_core_session_t *session)
{
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "CHANNEL EXCHANGE_MEDIA\n");
	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t channel_on_soft_execute(switch_core_session_t *session)
{
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "CHANNEL SOFT_EXECUTE\n");
	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t channel_send_dtmf(switch_core_session_t *session, const switch_dtmf_t *dtmf)
{
	private_t *tech_pvt = NULL;
	char tmp[2] = "";

	tech_pvt = switch_core_session_get_private(session);
	assert(tech_pvt != NULL);

	if (switch_test_flag(tech_pvt, TFLAG_DEAD)) {
		switch_channel_hangup(switch_core_session_get_channel(session), SWITCH_CAUSE_LOSE_RACE);
		return SWITCH_STATUS_FALSE;
	} 

	tmp[0] = dtmf->digit;
	ftdm_channel_command(tech_pvt->ftdmchan, FTDM_COMMAND_SEND_DTMF, tmp);
		
	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t channel_read_frame(switch_core_session_t *session, switch_frame_t **frame, switch_io_flag_t flags, int stream_id)
{
	switch_channel_t *channel = NULL;
	private_t *tech_pvt = NULL;
	ftdm_size_t len;
	ftdm_wait_flag_t wflags = FTDM_READ;
	char dtmf[128] = "";
	ftdm_status_t status;
	int total_to;
	int chunk, do_break = 0;


	channel = switch_core_session_get_channel(session);
	assert(channel != NULL);
	
	
	tech_pvt = switch_core_session_get_private(session);
	assert(tech_pvt != NULL);

	if (switch_test_flag(tech_pvt, TFLAG_DEAD)) {
		ftdm_log(FTDM_LOG_DEBUG, "TFLAG_DEAD is set\n");
		return SWITCH_STATUS_FALSE;
	} 

	/* Digium Cards sometimes timeout several times in a row here. 
	   Yes, we support digium cards, ain't we nice.......
	   6 double length intervals should compensate */
	chunk = ftdm_channel_get_io_interval(tech_pvt->ftdmchan) * 2;
	total_to = chunk * 6;

 top:

	if (switch_channel_test_flag(channel, CF_SUSPEND)) {
		do_break = 1;
	}

	if (switch_test_flag(tech_pvt, TFLAG_BREAK)) {
		switch_clear_flag_locked(tech_pvt, TFLAG_BREAK);
		do_break = 1;
	}

	if (switch_test_flag(tech_pvt, TFLAG_HOLD) || do_break) {
		switch_yield(ftdm_channel_get_io_interval(tech_pvt->ftdmchan) * 1000);
		tech_pvt->cng_frame.datalen = ftdm_channel_get_io_packet_len(tech_pvt->ftdmchan);
		tech_pvt->cng_frame.samples = tech_pvt->cng_frame.datalen;
		tech_pvt->cng_frame.flags = SFF_CNG;
		*frame = &tech_pvt->cng_frame;
		if (ftdm_channel_get_codec(tech_pvt->ftdmchan) == FTDM_CODEC_SLIN) {
			tech_pvt->cng_frame.samples /= 2;
		}
		return SWITCH_STATUS_SUCCESS;
	}
	
	if (!switch_test_flag(tech_pvt, TFLAG_IO)) {
		ftdm_log(FTDM_LOG_DEBUG, "TFLAG_IO is not set\n");
		goto fail;
	}

	wflags = FTDM_READ;	
	status = ftdm_channel_wait(tech_pvt->ftdmchan, &wflags, chunk);
	
	if (status == FTDM_FAIL) {
		ftdm_log(FTDM_LOG_ERROR, "Failed to wait for I/O\n");
		goto fail;
	}
	
	if (status == FTDM_TIMEOUT) {
		if (!switch_test_flag(tech_pvt, TFLAG_HOLD)) {
			total_to -= chunk;
			if (total_to <= 0) {
				ftdm_log(FTDM_LOG_WARNING, "Too many timeouts while waiting for I/O\n");
				goto fail;
			}
		}
		goto top;
	}

	if (!(wflags & FTDM_READ)) {
		goto top;
	}

	len = tech_pvt->read_frame.buflen;
	if (ftdm_channel_read(tech_pvt->ftdmchan, tech_pvt->read_frame.data, &len) != FTDM_SUCCESS) {
		ftdm_log(FTDM_LOG_WARNING, "failed to read from device\n");
		goto fail;
	}

	*frame = &tech_pvt->read_frame;
	tech_pvt->read_frame.datalen = (uint32_t)len;
	tech_pvt->read_frame.samples = tech_pvt->read_frame.datalen;

	if (ftdm_channel_get_codec(tech_pvt->ftdmchan) == FTDM_CODEC_SLIN) {
		tech_pvt->read_frame.samples /= 2;
	}

	while (ftdm_channel_dequeue_dtmf(tech_pvt->ftdmchan, dtmf, sizeof(dtmf))) {
		switch_dtmf_t _dtmf = { 0, SWITCH_DEFAULT_DTMF_DURATION };
		char *p;
		for (p = dtmf; p && *p; p++) {
			if (is_dtmf(*p)) {
				_dtmf.digit = *p;
				ftdm_log(FTDM_LOG_DEBUG, "Queuing DTMF [%c] in channel %s\n", *p, switch_channel_get_name(channel));
				switch_channel_queue_dtmf(channel, &_dtmf);
			}
		}
	}
	return SWITCH_STATUS_SUCCESS;

 fail:
	switch_clear_flag_locked(tech_pvt, TFLAG_IO);
	return SWITCH_STATUS_GENERR;
	

}

static switch_status_t channel_write_frame(switch_core_session_t *session, switch_frame_t *frame, switch_io_flag_t flags, int stream_id)
{
	switch_channel_t *channel = NULL;
	private_t *tech_pvt = NULL;
	ftdm_size_t len;
	unsigned char data[SWITCH_RECOMMENDED_BUFFER_SIZE] = {0};
	ftdm_wait_flag_t wflags = FTDM_WRITE;
	ftdm_status_t status;

	channel = switch_core_session_get_channel(session);
	assert(channel != NULL);

	tech_pvt = switch_core_session_get_private(session);
	assert(tech_pvt != NULL);

	if (!tech_pvt->ftdmchan) {
		return SWITCH_STATUS_FALSE;
	} 

	if (switch_test_flag(tech_pvt, TFLAG_DEAD)) {
		return SWITCH_STATUS_FALSE;
	}

	if (switch_test_flag(tech_pvt, TFLAG_HOLD)) {
		return SWITCH_STATUS_SUCCESS;
	}

	if (!switch_test_flag(tech_pvt, TFLAG_IO)) {
		goto fail;
	}
	
	if (switch_test_flag(frame, SFF_CNG)) {
		frame->data = data;
		frame->buflen = sizeof(data);
		if ((frame->datalen = tech_pvt->write_codec.implementation->encoded_bytes_per_packet) > frame->buflen) {
			goto fail;
		}
		memset(data, 255, frame->datalen);
	}


	wflags = FTDM_WRITE;	
	status = ftdm_channel_wait(tech_pvt->ftdmchan, &wflags, ftdm_channel_get_io_interval(tech_pvt->ftdmchan) * 10);
	
	if (!(wflags & FTDM_WRITE)) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Dropping frame! (write not ready)\n");
		return SWITCH_STATUS_SUCCESS;
	}

	len = frame->datalen;
	if (ftdm_channel_write(tech_pvt->ftdmchan, frame->data, frame->buflen, &len) != FTDM_SUCCESS) {
		if (++tech_pvt->wr_error > 10) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "too many I/O write errors!\n");
			goto fail;
		}
	} else {
		tech_pvt->wr_error = 0;
	}

	return SWITCH_STATUS_SUCCESS;

 fail:
	
	switch_clear_flag_locked(tech_pvt, TFLAG_IO);
	return SWITCH_STATUS_GENERR;

}

static switch_status_t channel_receive_message_cas(switch_core_session_t *session, switch_core_session_message_t *msg)
{
	switch_channel_t *channel;
	private_t *tech_pvt;
	uint32_t phy_id;

	channel = switch_core_session_get_channel(session);
	assert(channel != NULL);
			
	tech_pvt = (private_t *) switch_core_session_get_private(session);
	assert(tech_pvt != NULL);
	
	if (switch_test_flag(tech_pvt, TFLAG_DEAD)) {
        switch_channel_hangup(channel, SWITCH_CAUSE_LOSE_RACE);
        return SWITCH_STATUS_FALSE;
    }

	phy_id = ftdm_channel_get_ph_id(tech_pvt->ftdmchan);	
	ftdm_log(FTDM_LOG_DEBUG, "Got Freeswitch message in R2 channel %d [%d]\n", phy_id, msg->message_id);

	if (switch_channel_direction(channel) == SWITCH_CALL_DIRECTION_OUTBOUND) {
		return SWITCH_STATUS_SUCCESS;
	}

	switch (msg->message_id) {
	case SWITCH_MESSAGE_INDICATE_RINGING:
		{
			ftdm_channel_call_indicate(tech_pvt->ftdmchan, FTDM_CHANNEL_INDICATE_PROGRESS);
		}
		break;
	case SWITCH_MESSAGE_INDICATE_PROGRESS:
		{
			ftdm_channel_call_indicate(tech_pvt->ftdmchan, FTDM_CHANNEL_INDICATE_PROGRESS_MEDIA);
		}
		break;
	case SWITCH_MESSAGE_INDICATE_ANSWER:
		{
			ftdm_channel_call_answer(tech_pvt->ftdmchan);
		}
		break;
	default:
		break;
	}

	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t channel_receive_message_b(switch_core_session_t *session, switch_core_session_message_t *msg)
{
	switch_channel_t *channel;
	private_t *tech_pvt;

	channel = switch_core_session_get_channel(session);
	assert(channel != NULL);
			
	tech_pvt = (private_t *) switch_core_session_get_private(session);
	assert(tech_pvt != NULL);

	if (switch_test_flag(tech_pvt, TFLAG_DEAD)) {
		switch_channel_hangup(channel, SWITCH_CAUSE_LOSE_RACE);
		return SWITCH_STATUS_FALSE;
    	}

	if (ftdm_channel_call_check_hangup(tech_pvt->ftdmchan)) {
		return SWITCH_STATUS_SUCCESS;
	}

	if (switch_channel_direction(channel) == SWITCH_CALL_DIRECTION_OUTBOUND) {
		return SWITCH_STATUS_SUCCESS;
	}

	switch (msg->message_id) {
	case SWITCH_MESSAGE_INDICATE_RINGING:
		{
			ftdm_channel_call_indicate(tech_pvt->ftdmchan, FTDM_CHANNEL_INDICATE_RINGING);
		}
		break;
	case SWITCH_MESSAGE_INDICATE_PROGRESS:
		{
			ftdm_channel_call_indicate(tech_pvt->ftdmchan, FTDM_CHANNEL_INDICATE_PROGRESS_MEDIA);
		}
		break;
	case SWITCH_MESSAGE_INDICATE_ANSWER:
		{
			ftdm_channel_call_answer(tech_pvt->ftdmchan);
		}
		break;
	default:
		break;
	}
	
	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t channel_receive_message_fxo(switch_core_session_t *session, switch_core_session_message_t *msg)
{
	switch_channel_t *channel;
	private_t *tech_pvt;

	channel = switch_core_session_get_channel(session);
	assert(channel != NULL);
			
	tech_pvt = (private_t *) switch_core_session_get_private(session);
	assert(tech_pvt != NULL);

	if (switch_test_flag(tech_pvt, TFLAG_DEAD)) {
		switch_channel_hangup(channel, SWITCH_CAUSE_LOSE_RACE);
		return SWITCH_STATUS_FALSE;
    	}
	
	if (switch_channel_direction(channel) == SWITCH_CALL_DIRECTION_OUTBOUND) {
		return SWITCH_STATUS_SUCCESS;
	}

	switch (msg->message_id) {
	case SWITCH_MESSAGE_INDICATE_PROGRESS:
	case SWITCH_MESSAGE_INDICATE_ANSWER:
		ftdm_channel_call_answer(tech_pvt->ftdmchan);
		break;
	default:
		break;
	}

	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t channel_receive_message_fxs(switch_core_session_t *session, switch_core_session_message_t *msg)
{
	switch_channel_t *channel;
	private_t *tech_pvt;

	channel = switch_core_session_get_channel(session);
	assert(channel != NULL);
			
	tech_pvt = (private_t *) switch_core_session_get_private(session);
	assert(tech_pvt != NULL);

	if (switch_test_flag(tech_pvt, TFLAG_DEAD)) {
		switch_channel_hangup(channel, SWITCH_CAUSE_LOSE_RACE);
		return SWITCH_STATUS_FALSE;
	}

	if (switch_channel_direction(channel) == SWITCH_CALL_DIRECTION_OUTBOUND) {
		return SWITCH_STATUS_SUCCESS;
	}
	
	switch (msg->message_id) {
	case SWITCH_MESSAGE_INDICATE_PROGRESS:
	case SWITCH_MESSAGE_INDICATE_ANSWER:
		ftdm_channel_call_answer(tech_pvt->ftdmchan);
		switch_channel_mark_answered(channel);
		break;
	case SWITCH_MESSAGE_INDICATE_RINGING:
		if (!switch_channel_test_flag(channel, CF_ANSWERED) && 
			!switch_channel_test_flag(channel, CF_EARLY_MEDIA) &&
			!switch_channel_test_flag(channel, CF_RING_READY)
			) {
				ftdm_channel_call_indicate(tech_pvt->ftdmchan, FTDM_CHANNEL_INDICATE_RINGING);
				switch_channel_mark_ring_ready(channel);
		}
		break;
	default:
		break;
	}

	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t channel_receive_message(switch_core_session_t *session, switch_core_session_message_t *msg)
{
	private_t *tech_pvt;
	switch_status_t status;
	switch_channel_t *channel;
	const char *var;
	ftdm_channel_t *ftdmchan = NULL;

	tech_pvt = (private_t *) switch_core_session_get_private(session);
	assert(tech_pvt != NULL);

	channel = switch_core_session_get_channel(session);

	if (switch_test_flag(tech_pvt, TFLAG_DEAD)) {
        switch_channel_hangup(channel, SWITCH_CAUSE_LOSE_RACE);
        return SWITCH_STATUS_FALSE;
	}

	if (!(ftdmchan = tech_pvt->ftdmchan)) {
        switch_channel_hangup(channel, SWITCH_CAUSE_LOSE_RACE);
        return SWITCH_STATUS_FALSE;
    }

	if (!tech_pvt->ftdmchan) {
		switch_channel_hangup(channel, SWITCH_CAUSE_LOSE_RACE);
		return SWITCH_STATUS_FALSE;
	}

	switch (msg->message_id) {
	case SWITCH_MESSAGE_INDICATE_PROGRESS:
	case SWITCH_MESSAGE_INDICATE_ANSWER:
		if (switch_channel_direction(channel) == SWITCH_CALL_DIRECTION_INBOUND) {
			if ((var = switch_channel_get_variable(channel, "freetdm_pre_buffer_size"))) {
				int tmp = atoi(var);
				if (tmp > -1) {
					ftdm_channel_command(tech_pvt->ftdmchan, FTDM_COMMAND_SET_PRE_BUFFER_SIZE, &tmp);
				}
			}
			if ((var = switch_channel_get_variable(channel, "freetdm_disable_dtmf"))) {
				ftdm_channel_command(tech_pvt->ftdmchan, FTDM_COMMAND_DISABLE_DTMF_DETECT, NULL);
			}
		}
		break;
	case SWITCH_MESSAGE_INDICATE_UUID_CHANGE:
		{
			ftdm_channel_replace_token(tech_pvt->ftdmchan, msg->string_array_arg[0], msg->string_array_arg[1]);
		}
		break;
	default:
		break;
	}

	switch (ftdm_channel_get_type(tech_pvt->ftdmchan)) {
	case FTDM_CHAN_TYPE_FXS:
	case FTDM_CHAN_TYPE_EM:
		status = channel_receive_message_fxs(session, msg);
		break;
	case FTDM_CHAN_TYPE_FXO:
		status = channel_receive_message_fxo(session, msg);
		break;
	case FTDM_CHAN_TYPE_B:
		status = channel_receive_message_b(session, msg);
        break;
	case FTDM_CHAN_TYPE_CAS:
		status = channel_receive_message_cas(session, msg);
        break;
	default:
		status = SWITCH_STATUS_FALSE;
		break;
	}

	return status;

}

switch_state_handler_table_t freetdm_state_handlers = {
	/*.on_init */ channel_on_init,
	/*.on_routing */ channel_on_routing,
	/*.on_execute */ channel_on_execute,
	/*.on_hangup */ channel_on_hangup,
	/*.on_exchange_media */ channel_on_exchange_media,
	/*.on_soft_execute */ channel_on_soft_execute,
	/*.on_consume_media */ NULL,
    /*.on_hibernate */ NULL,
    /*.on_reset */ NULL,
    /*.on_park*/ NULL,
    /*.on_reporting*/ NULL,
    /*.on_destroy*/ channel_on_destroy

};

switch_io_routines_t freetdm_io_routines = {
	/*.outgoing_channel */ channel_outgoing_channel,
	/*.read_frame */ channel_read_frame,
	/*.write_frame */ channel_write_frame,
	/*.kill_channel */ channel_kill_channel,
	/*.send_dtmf */ channel_send_dtmf,
	/*.receive_message*/ channel_receive_message
};

static const char* channel_get_variable(switch_core_session_t *session, switch_event_t *var_event, const char *variable_name)
{
       const char *variable = NULL;
       if (var_event) {
               if ((variable = switch_event_get_header(var_event, variable_name))) {
                       return variable;
               }
       }

       if (session) {
               switch_channel_t *channel = switch_core_session_get_channel(session);
               if ((variable = switch_channel_get_variable(channel, variable_name))) {
                       return variable;
               }
       }
       return NULL;
}

typedef struct {
	switch_event_t *var_event;
	switch_core_session_t *new_session;
	private_t *tech_pvt;
	switch_caller_profile_t *caller_profile;
} hunt_data_t;

static ftdm_status_t on_channel_found(ftdm_channel_t *fchan, ftdm_caller_data_t *caller_data)
{
	uint32_t span_id, chan_id;
	const char *var;
	char *sess_uuid;
	char name[128];
	ftdm_status_t status;
	hunt_data_t *hdata = caller_data->priv;
	switch_channel_t *channel = switch_core_session_get_channel(hdata->new_session);

	if ((var = switch_event_get_header(hdata->var_event, "freetdm_pre_buffer_size"))) {
		int tmp = atoi(var);
		if (tmp > -1) {
			ftdm_channel_command(fchan, FTDM_COMMAND_SET_PRE_BUFFER_SIZE, &tmp);
		}
	}

	span_id = ftdm_channel_get_span_id(fchan);
	chan_id = ftdm_channel_get_id(fchan);

	tech_init(hdata->tech_pvt, hdata->new_session, fchan);

	snprintf(name, sizeof(name), "FreeTDM/%u:%u/%s", span_id, chan_id, caller_data->dnis.digits);
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Connect outbound channel %s\n", name);
	switch_channel_set_name(channel, name);
	switch_channel_set_variable(channel, "freetdm_span_name", ftdm_channel_get_span_name(fchan));
	switch_channel_set_variable_printf(channel, "freetdm_span_number", "%d", span_id);
	switch_channel_set_variable_printf(channel, "freetdm_chan_number", "%d", chan_id);

	switch_channel_set_caller_profile(channel, hdata->caller_profile);
	hdata->tech_pvt->caller_profile = hdata->caller_profile;

	switch_channel_set_state(channel, CS_INIT);
	sess_uuid = switch_core_session_get_uuid(hdata->new_session);
	status = ftdm_channel_add_token(fchan, sess_uuid, ftdm_channel_get_token_count(fchan));
	switch_assert(status == FTDM_SUCCESS);

	if (SPAN_CONFIG[span_id].limit_calls) {
		char spanresource[512];
		snprintf(spanresource, sizeof(spanresource), "span_%s_%s", ftdm_channel_get_span_name(fchan), 
				caller_data->dnis.digits);
		ftdm_log(FTDM_LOG_DEBUG, "Adding rate limit resource on channel %d:%d (%s/%s/%d/%d)\n", 
				span_id, chan_id, FREETDM_LIMIT_REALM, 
				spanresource, SPAN_CONFIG[span_id].limit_calls, SPAN_CONFIG[span_id].limit_seconds);
		if (switch_limit_incr("hash", hdata->new_session, FREETDM_LIMIT_REALM, spanresource, 
					SPAN_CONFIG[span_id].limit_calls, SPAN_CONFIG[span_id].limit_seconds) != SWITCH_STATUS_SUCCESS) {
			return FTDM_BREAK;
		}
	}
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Attached session %s to channel %d:%d\n", sess_uuid, span_id, chan_id);
	return FTDM_SUCCESS;
}

/* Make sure when you have 2 sessions in the same scope that you pass the appropriate one to the routines
that allocate memory or you will have 1 channel with memory allocated from another channel's pool!
*/
static switch_call_cause_t channel_outgoing_channel(switch_core_session_t *session, switch_event_t *var_event,
													switch_caller_profile_t *outbound_profile,
													switch_core_session_t **new_session, switch_memory_pool_t **pool,
													switch_originate_flag_t flags, switch_call_cause_t *cancel_cause)
{
	hunt_data_t hunt_data;
	const char *dest = NULL;
	char *data = NULL;
	int span_id = -1, group_id = -1, chan_id = 0;
	switch_call_cause_t cause = SWITCH_CAUSE_DESTINATION_OUT_OF_ORDER;
	ftdm_status_t status;
	int direction = FTDM_TOP_DOWN;
	ftdm_caller_data_t caller_data = {{ 0 }};
	char *span_name = NULL;
	switch_event_header_t *h;
	char *argv[3];
	int argc = 0;
	const char *var;
	const char *dest_num = NULL, *callerid_num = NULL;
	ftdm_hunting_scheme_t hunting;
	ftdm_usrmsg_t usrmsg;

	memset(&usrmsg, 0, sizeof(ftdm_usrmsg_t));

	if (!outbound_profile) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Missing caller profile\n");
		return SWITCH_CAUSE_DESTINATION_OUT_OF_ORDER;
	}

	if (zstr(outbound_profile->destination_number)) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Invalid dial string\n");
		return SWITCH_CAUSE_DESTINATION_OUT_OF_ORDER;
	}


	data = switch_core_strdup(outbound_profile->pool, outbound_profile->destination_number);

	if (!zstr(outbound_profile->destination_number)) {
		dest_num = switch_sanitize_number(switch_core_strdup(outbound_profile->pool, outbound_profile->destination_number));
	}

	if (!zstr(outbound_profile->caller_id_number)) {
		callerid_num = switch_sanitize_number(switch_core_strdup(outbound_profile->pool, outbound_profile->caller_id_number));
	}

	if (!zstr(callerid_num) && !strcmp(callerid_num, "0000000000")) {
		callerid_num = NULL;
	}
	
	if ((argc = switch_separate_string(data, '/', argv, (sizeof(argv) / sizeof(argv[0])))) < 2) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Invalid dial string\n");
        	return SWITCH_CAUSE_DESTINATION_OUT_OF_ORDER;
	}
	
	if (switch_is_number(argv[0])) {
		span_id = atoi(argv[0]);
	} else {
		span_name = argv[0];
	}	

	if (*argv[1] == 'A') {
		direction = FTDM_BOTTOM_UP;
	} else if (*argv[1] == 'a') {
		direction =  FTDM_TOP_DOWN;
	} else if (*argv[1] == 'r') {
		direction =  FTDM_RR_DOWN;
	} else if (*argv[1] == 'R') {
		direction =  FTDM_RR_UP;
	} else {
		chan_id = atoi(argv[1]);
	}

	if (!(dest = argv[2])) {
		dest = "";
	}

	if (span_id == 0 && chan_id != 0) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Span 0 is used to pick the first available span, selecting a channel is not supported (and doesn't make sense)\n");
		return SWITCH_CAUSE_DESTINATION_OUT_OF_ORDER;
	}

	if (span_id == -1 && !zstr(span_name)) {
		ftdm_span_t *span;
		ftdm_status_t zstatus = ftdm_span_find_by_name(span_name, &span);
		if (zstatus == FTDM_SUCCESS && span) {
			span_id = ftdm_span_get_id(span);
		}
	}

	if (span_id == -1) {
		//Look for a group
		ftdm_group_t *group;
		ftdm_status_t zstatus = ftdm_group_find_by_name(span_name, &group);
		if (zstatus == FTDM_SUCCESS && group) {
			group_id = ftdm_group_get_id(group);
		} else {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Missing ftdm span or group: %s\n", span_name);
			return SWITCH_CAUSE_DESTINATION_OUT_OF_ORDER;
		}
	}

	if (group_id < 0 && chan_id < 0) {
		direction = FTDM_BOTTOM_UP;
		chan_id = 0;
	}

	if (session && globals.sip_headers) {
		switch_channel_t *channel = switch_core_session_get_channel(session);
		const char *sipvar;
		sipvar = switch_channel_get_variable(channel, "sip_h_X-FreeTDM-CallerName");
		if (sipvar) {
			ftdm_set_string(caller_data.cid_name, sipvar);
		}
		
		sipvar = switch_channel_get_variable(channel, "sip_h_X-FreeTDM-CallerNumber");
		if (sipvar) {
			ftdm_set_string(caller_data.cid_num.digits, sipvar);
		}

		sipvar = switch_channel_get_variable(channel, "sip_h_X-FreeTDM-ANI");
		if (sipvar) {
			ftdm_set_string(caller_data.ani.digits, sipvar);
		}

		sipvar = switch_channel_get_variable(channel, "sip_h_X-FreeTDM-ANI-TON");
		if (sipvar) {
			caller_data.ani.type = (uint8_t)atoi(sipvar);
		}

		sipvar = switch_channel_get_variable(channel, "sip_h_X-FreeTDM-ANI-Plan");
		if (sipvar) {
			caller_data.ani.plan = (uint8_t)atoi(sipvar);
		}

		sipvar = switch_channel_get_variable(channel, "sip_h_X-FreeTDM-ANI2");
		if (sipvar) {
			ftdm_set_string(caller_data.aniII, sipvar);
		}
		
		sipvar = switch_channel_get_variable(channel, "sip_h_X-FreeTDM-DNIS");
		if (sipvar) {
			ftdm_set_string(caller_data.dnis.digits, sipvar);
		}

		sipvar = switch_channel_get_variable(channel, "sip_h_X-FreeTDM-DNIS-TON");
		if (sipvar) {
			caller_data.dnis.type = (uint8_t)atoi(sipvar);
		}

		sipvar = switch_channel_get_variable(channel, "sip_h_X-FreeTDM-DNIS-Plan");
		if (sipvar) {
			caller_data.dnis.plan = (uint8_t)atoi(sipvar);
		}

		sipvar = switch_channel_get_variable(channel, "sip_h_X-FreeTDM-RDNIS");
		if (sipvar) {
			ftdm_set_string(caller_data.rdnis.digits, sipvar);
		}

		sipvar = switch_channel_get_variable(channel, "sip_h_X-FreeTDM-RDNIS-TON");
		if (sipvar) {
			caller_data.rdnis.type = (uint8_t)atoi(sipvar);
		}

		sipvar = switch_channel_get_variable(channel, "sip_h_X-FreeTDM-RDNIS-Plan");
		if (sipvar) {
			caller_data.rdnis.plan = (uint8_t)atoi(sipvar);
		}

		sipvar = switch_channel_get_variable(channel, "sip_h_X-FreeTDM-Screen");
		if (sipvar) {
			caller_data.screen = (uint8_t)atoi(sipvar);
		}

		sipvar = switch_channel_get_variable(channel, "sip_h_X-FreeTDM-Presentation");
		if (sipvar) {
			caller_data.pres = (uint8_t)atoi(sipvar);
		}
	}

	if (switch_test_flag(outbound_profile, SWITCH_CPF_SCREEN)) {
		caller_data.screen = FTDM_SCREENING_VERIFIED_PASSED;
	}

	if (switch_test_flag(outbound_profile, SWITCH_CPF_HIDE_NUMBER)) {
		caller_data.pres = FTDM_PRES_RESTRICTED;
	}

	if ((var = channel_get_variable(session, var_event, "freetdm_bearer_capability"))) {
		caller_data.bearer_capability = (uint8_t)atoi(var);
	}
	
	if ((var = channel_get_variable(session, var_event, "freetdm_bearer_layer1"))) {
			caller_data.bearer_layer1 = (uint8_t)atoi(var);
	}

	if ((var = channel_get_variable(session, var_event, "freetdm_screening_ind"))) {
			ftdm_set_screening_ind(var, &caller_data.screen);
	}

	if ((var = channel_get_variable(session, var_event, "freetdm_presentation_ind"))) {
			ftdm_set_presentation_ind(var, &caller_data.pres);
	}

	if ((var = channel_get_variable(session, var_event, "freetdm_outbound_ton"))) {
			ftdm_set_ton(var, &caller_data.dnis.type);
	} else {
		caller_data.dnis.type = outbound_profile->destination_number_ton;
	}

	if ((var = channel_get_variable(session, var_event, "freetdm_calling_party_category"))) {
		ftdm_set_calling_party_category(var, (uint8_t *)&caller_data.cpc);
	}
	
	if (!zstr(dest)) {
		ftdm_set_string(caller_data.dnis.digits, dest);
	}

	caller_data.dnis.plan = outbound_profile->destination_number_numplan;

	/* blindly copy data from outbound_profile. They will be overwritten
	 * by calling ftdm_caller_data if needed after */
	caller_data.cid_num.type = outbound_profile->caller_ton;
	caller_data.cid_num.plan = outbound_profile->caller_numplan;
	caller_data.rdnis.type = outbound_profile->rdnis_ton;
	caller_data.rdnis.plan = outbound_profile->rdnis_numplan;

	ftdm_set_string(caller_data.cid_name, outbound_profile->caller_id_name);
	ftdm_set_string(caller_data.cid_num.digits, switch_str_nil(outbound_profile->caller_id_number));

	memset(&hunting, 0, sizeof(hunting));

	if (group_id >= 0) {
		hunting.mode = FTDM_HUNT_GROUP;
		hunting.mode_data.group.group_id = group_id;
		hunting.mode_data.group.direction = direction;
	} else if (chan_id) {
		hunting.mode = FTDM_HUNT_CHAN;
		hunting.mode_data.chan.span_id = span_id;
		hunting.mode_data.chan.chan_id = chan_id;
	} else {
		hunting.mode = FTDM_HUNT_SPAN;
		hunting.mode_data.span.span_id = span_id;
		hunting.mode_data.span.direction = direction;
	}

	for (h = var_event->headers; h; h = h->next) {
		if (!strncasecmp(h->name, FREETDM_VAR_PREFIX, FREETDM_VAR_PREFIX_LEN)) {
			char *v = h->name + FREETDM_VAR_PREFIX_LEN;
			if (!zstr(v)) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Adding outbound freetdm variable %s=%s to channel %d:%d\n", v, h->value, span_id, chan_id);
				ftdm_usrmsg_add_var(&usrmsg, v, h->value);
			}
		}
	}

	if ((*new_session = switch_core_session_request(freetdm_endpoint_interface, SWITCH_CALL_DIRECTION_OUTBOUND, flags, pool)) != 0) {
		private_t *tech_pvt;
		switch_caller_profile_t *caller_profile;
		
		switch_core_session_add_stream(*new_session, NULL);
		if (!(tech_pvt = (private_t *) switch_core_session_alloc(*new_session, sizeof(private_t)))) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Hey where is my memory pool?\n");
			switch_core_session_destroy(new_session);
			cause = SWITCH_CAUSE_DESTINATION_OUT_OF_ORDER;
			goto fail;
		}

		caller_profile = switch_caller_profile_clone(*new_session, outbound_profile);
		caller_profile->destination_number = switch_core_strdup(caller_profile->pool, switch_str_nil(dest_num));
		caller_profile->caller_id_number = switch_core_strdup(caller_profile->pool, switch_str_nil(callerid_num));

		hunting.result_cb = on_channel_found;
		hunt_data.var_event = var_event;
		hunt_data.new_session = *new_session;
		hunt_data.caller_profile = caller_profile;
		hunt_data.tech_pvt = tech_pvt;
		caller_data.priv = &hunt_data;

		if ((status = ftdm_call_place_ex(&caller_data, &hunting, &usrmsg)) != FTDM_SUCCESS) {
			if (tech_pvt->read_codec.implementation) {
				switch_core_codec_destroy(&tech_pvt->read_codec);
			}
			
			if (tech_pvt->write_codec.implementation) {
				switch_core_codec_destroy(&tech_pvt->write_codec);
			}
			switch_core_session_destroy(new_session);
			if (status == FTDM_BREAK || status == FTDM_EBUSY) { 
				cause = SWITCH_CAUSE_NORMAL_CIRCUIT_CONGESTION;
			} else {
				cause = SWITCH_CAUSE_DESTINATION_OUT_OF_ORDER;
			}
            		goto fail;
		}

		return SWITCH_CAUSE_SUCCESS;
	}

fail:

	return cause;
}

static void ftdm_enable_channel_dtmf(ftdm_channel_t *fchan, switch_channel_t *channel)
{
	if (channel) {
		const char *var;
		if ((var = switch_channel_get_variable(channel, "freetdm_disable_dtmf"))) {
			if (switch_true(var)) {
				ftdm_channel_command(fchan, FTDM_COMMAND_DISABLE_DTMF_DETECT, NULL);
				ftdm_log(FTDM_LOG_INFO, "DTMF detection disabled in channel %d:%d\n", ftdm_channel_get_span_id(fchan), ftdm_channel_get_id(fchan));
				return;
			}
		}
		/* the variable is not present or has a negative value then proceed to enable DTMF ... */
	}
	if (ftdm_channel_command(fchan, FTDM_COMMAND_ENABLE_DTMF_DETECT, NULL) != FTDM_SUCCESS) {
		ftdm_log(FTDM_LOG_ERROR, "Failed to enable DTMF detection in channel %d:%d\n", ftdm_channel_get_span_id(fchan), ftdm_channel_get_id(fchan));
	}
}

ftdm_status_t ftdm_channel_from_event(ftdm_sigmsg_t *sigmsg, switch_core_session_t **sp)
{
	switch_core_session_t *session = NULL;
	private_t *tech_pvt = NULL;
	switch_channel_t *channel = NULL;
	ftdm_iterator_t *iter = NULL;
	ftdm_iterator_t *curr = NULL;
	const char *var_name = NULL;
	const char *var_value = NULL;
	uint32_t spanid, chanid;
	char name[128];
	ftdm_caller_data_t *channel_caller_data = ftdm_channel_get_caller_data(sigmsg->channel);
	
	*sp = NULL;

	spanid = ftdm_channel_get_span_id(sigmsg->channel);
	chanid = ftdm_channel_get_id(sigmsg->channel);
	
	if (!(session = switch_core_session_request(freetdm_endpoint_interface, SWITCH_CALL_DIRECTION_INBOUND, SOF_NONE, NULL))) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Initilization Error!\n");
		return FTDM_FAIL;
	}
	
	/* I guess we always want DTMF detection */
	ftdm_enable_channel_dtmf(sigmsg->channel, NULL);

	switch_core_session_add_stream(session, NULL);
	
	tech_pvt = (private_t *) switch_core_session_alloc(session, sizeof(private_t));
	assert(tech_pvt != NULL);
	channel = switch_core_session_get_channel(session);
	if (tech_init(tech_pvt, session, sigmsg->channel) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Initilization Error!\n");
		switch_core_session_destroy(&session);
		return FTDM_FAIL;
	}

	channel_caller_data->collected[0] = '\0';
	
	if (zstr(channel_caller_data->cid_name)) {
		switch_set_string(channel_caller_data->cid_name, ftdm_channel_get_name(sigmsg->channel));
	}

	if (zstr(channel_caller_data->cid_num.digits)) {
		if (!zstr(channel_caller_data->ani.digits)) {
			switch_set_string(channel_caller_data->cid_num.digits, channel_caller_data->ani.digits);
		} else {
			switch_set_string(channel_caller_data->cid_num.digits, ftdm_channel_get_number(sigmsg->channel));
		}
	}

	tech_pvt->caller_profile = switch_caller_profile_new(switch_core_session_get_pool(session),
														 "FreeTDM",
														 SPAN_CONFIG[spanid].dialplan,
														 channel_caller_data->cid_name,
														 channel_caller_data->cid_num.digits,
														 NULL,
														 channel_caller_data->ani.digits,
														 channel_caller_data->aniII,
														 channel_caller_data->rdnis.digits,
														 (char *)modname,
														 SPAN_CONFIG[spanid].context,
														 channel_caller_data->dnis.digits);

	assert(tech_pvt->caller_profile != NULL);

	if (channel_caller_data->screen == 1 || channel_caller_data->screen == 3) {
		switch_set_flag(tech_pvt->caller_profile, SWITCH_CPF_SCREEN);
	}

	tech_pvt->caller_profile->caller_ton = channel_caller_data->cid_num.type;
	tech_pvt->caller_profile->caller_numplan = channel_caller_data->cid_num.plan;
	tech_pvt->caller_profile->ani_ton = channel_caller_data->ani.type;
	tech_pvt->caller_profile->ani_numplan = channel_caller_data->ani.plan;
	tech_pvt->caller_profile->destination_number_ton = channel_caller_data->dnis.type;
	tech_pvt->caller_profile->destination_number_numplan = channel_caller_data->dnis.plan;
	tech_pvt->caller_profile->rdnis_ton = channel_caller_data->rdnis.type;
	tech_pvt->caller_profile->rdnis_numplan = channel_caller_data->rdnis.plan;

	if (channel_caller_data->pres) {
		switch_set_flag(tech_pvt->caller_profile, SWITCH_CPF_HIDE_NAME | SWITCH_CPF_HIDE_NUMBER);
	}
	
	snprintf(name, sizeof(name), "FreeTDM/%u:%u/%s", spanid, chanid, tech_pvt->caller_profile->destination_number);
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Connect inbound channel %s\n", name);
	switch_channel_set_name(channel, name);
	switch_channel_set_caller_profile(channel, tech_pvt->caller_profile);

	switch_channel_set_variable(channel, "freetdm_span_name", ftdm_channel_get_span_name(sigmsg->channel));
	switch_channel_set_variable_printf(channel, "freetdm_span_number", "%d", spanid);	
	switch_channel_set_variable_printf(channel, "freetdm_chan_number", "%d", chanid);
	switch_channel_set_variable_printf(channel, "freetdm_bearer_capability", "%d", channel_caller_data->bearer_capability);	
	switch_channel_set_variable_printf(channel, "freetdm_bearer_layer1", "%d", channel_caller_data->bearer_layer1);
	switch_channel_set_variable_printf(channel, "screening_ind", ftdm_screening2str(channel_caller_data->screen));
	switch_channel_set_variable_printf(channel, "presentation_ind", ftdm_presentation2str(channel_caller_data->pres));
	
	if (globals.sip_headers) {
		switch_channel_set_variable(channel, "sip_h_X-FreeTDM-SpanName", ftdm_channel_get_span_name(sigmsg->channel));
		switch_channel_set_variable_printf(channel, "sip_h_X-FreeTDM-SpanNumber", "%d", spanid);	
		switch_channel_set_variable_printf(channel, "sip_h_X-FreeTDM-ChanNumber", "%d", chanid);

		switch_channel_set_variable_printf(channel, "sip_h_X-FreeTDM-CallerName", "%s", channel_caller_data->cid_name);
		switch_channel_set_variable_printf(channel, "sip_h_X-FreeTDM-CallerNumber", "%s", channel_caller_data->cid_num.digits);

		switch_channel_set_variable_printf(channel, "sip_h_X-FreeTDM-ANI", "%s", channel_caller_data->ani.digits);
		switch_channel_set_variable_printf(channel, "sip_h_X-FreeTDM-ANI-TON", "%d", channel_caller_data->ani.type);
		switch_channel_set_variable_printf(channel, "sip_h_X-FreeTDM-ANI-Plan", "%d", channel_caller_data->ani.plan);
		switch_channel_set_variable_printf(channel, "sip_h_X-FreeTDM-ANI2", "%s", channel_caller_data->aniII);
		
		switch_channel_set_variable_printf(channel, "sip_h_X-FreeTDM-DNIS", "%s", channel_caller_data->dnis.digits);
		switch_channel_set_variable_printf(channel, "sip_h_X-FreeTDM-DNIS-TON", "%d", channel_caller_data->dnis.type);
		switch_channel_set_variable_printf(channel, "sip_h_X-FreeTDM-DNIS-Plan", "%d", channel_caller_data->dnis.plan);

		switch_channel_set_variable_printf(channel, "sip_h_X-FreeTDM-RDNIS", "%s", channel_caller_data->rdnis.digits);
		switch_channel_set_variable_printf(channel, "sip_h_X-FreeTDM-RDNIS-TON", "%d", channel_caller_data->rdnis.type);
		switch_channel_set_variable_printf(channel, "sip_h_X-FreeTDM-RDNIS-Plan", "%d", channel_caller_data->rdnis.plan);

		switch_channel_set_variable_printf(channel, "sip_h_X-FreeTDM-Screen", "%d", channel_caller_data->screen);
		switch_channel_set_variable_printf(channel, "sip_h_X-FreeTDM-Presentation", "%d", channel_caller_data->pres);
	}

	/* Add any call variable to the dial plan */
	iter = ftdm_sigmsg_get_var_iterator(sigmsg, iter);
	for (curr = iter ; curr; curr = ftdm_iterator_next(curr)) {
		ftdm_get_current_var(curr, &var_name, &var_value);
		snprintf(name, sizeof(name), FREETDM_VAR_PREFIX "%s", var_name);
		switch_channel_set_variable_printf(channel, name, "%s", var_value);
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Call Variable: %s = %s\n", name, var_value);
	}
	ftdm_iterator_free(iter);
	
	switch_channel_set_state(channel, CS_INIT);
	if (switch_core_session_thread_launch(session) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Error spawning thread\n");
		switch_core_session_destroy(&session);
		return FTDM_FAIL;
	}

	if (ftdm_channel_add_token(sigmsg->channel, switch_core_session_get_uuid(session), 0) != FTDM_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Error adding token\n");
		switch_core_session_destroy(&session);
		return FTDM_FAIL;
	}
	*sp = session;

    return FTDM_SUCCESS;
}

static FIO_SIGNAL_CB_FUNCTION(on_common_signal)
{
	switch_event_t *event = NULL;
	ftdm_alarm_flag_t alarmbits = FTDM_ALARM_NONE;
	uint32_t chanid, spanid;
	chanid = ftdm_channel_get_id(sigmsg->channel);
	spanid = ftdm_channel_get_span_id(sigmsg->channel);
	switch (sigmsg->event_id) {

	case FTDM_SIGEVENT_ALARM_CLEAR:
	case FTDM_SIGEVENT_ALARM_TRAP:
		{
			if (ftdm_channel_get_alarms(sigmsg->channel, &alarmbits) != FTDM_SUCCESS) {
				ftdm_log(FTDM_LOG_ERROR, "failed to retrieve alarms\n");
				return FTDM_FAIL;
			}
			if (switch_event_create(&event, SWITCH_EVENT_TRAP) != SWITCH_STATUS_SUCCESS) {
				ftdm_log(FTDM_LOG_ERROR, "failed to create alarms events\n");
				return FTDM_FAIL;
			}
			if (sigmsg->event_id == FTDM_SIGEVENT_ALARM_CLEAR) {
				ftdm_log(FTDM_LOG_NOTICE, "Alarm cleared on channel %d:%d\n", spanid, chanid);
				switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "condition", "ftdm-alarm-clear");
			} else {
				ftdm_log(FTDM_LOG_NOTICE, "Alarm raised on channel %d:%d\n", spanid, chanid);
				switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "condition", "ftdm-alarm-trap");
			}
		}
		break;
	case FTDM_SIGEVENT_UP:
		{
			/* clear any rate limit resource for this span */
			char spanresource[512];
			if (SPAN_CONFIG[spanid].limit_reset_event == FTDM_LIMIT_RESET_ON_ANSWER && SPAN_CONFIG[spanid].limit_calls) {
				ftdm_caller_data_t *caller_data = ftdm_channel_get_caller_data(sigmsg->channel);
				snprintf(spanresource, sizeof(spanresource), "span_%s_%s", ftdm_channel_get_span_name(sigmsg->channel), caller_data->dnis.digits);
				ftdm_log(FTDM_LOG_DEBUG, "Clearing rate limit resource on channel %d:%d (%s/%s)\n", spanid, chanid, FREETDM_LIMIT_REALM, spanresource);
				switch_limit_interval_reset("hash", FREETDM_LIMIT_REALM, spanresource);
			}
			return FTDM_SUCCESS;
		}

	case FTDM_SIGEVENT_RELEASED: 
	case FTDM_SIGEVENT_INDICATION_COMPLETED:
	case FTDM_SIGEVENT_DIALING:
		{ 
			/* Swallow these events */
			return FTDM_BREAK;
		} 
		break;
	default:
		return FTDM_SUCCESS;
		break;
	}

	switch_event_add_header(event, SWITCH_STACK_BOTTOM, "span-name", "%s", ftdm_channel_get_span_name(sigmsg->channel));
	switch_event_add_header(event, SWITCH_STACK_BOTTOM, "span-number", "%d", ftdm_channel_get_span_id(sigmsg->channel));
	switch_event_add_header(event, SWITCH_STACK_BOTTOM, "chan-number", "%d", ftdm_channel_get_id(sigmsg->channel));

	if (alarmbits & FTDM_ALARM_RED) {
		switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "alarm", "red");
	}
	if (alarmbits & FTDM_ALARM_YELLOW) {
		switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "alarm", "yellow");
	}
	if (alarmbits & FTDM_ALARM_RAI) {
		switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "alarm", "rai");
	}
	if (alarmbits & FTDM_ALARM_BLUE) {
		switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "alarm", "blue");
	}
	if (alarmbits & FTDM_ALARM_AIS) {
		switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "alarm", "ais");
	}
	if (alarmbits & FTDM_ALARM_GENERAL) {
		switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "alarm", "general");
	}
	switch_event_fire(&event);

	return FTDM_BREAK;
}

static FIO_SIGNAL_CB_FUNCTION(on_fxo_signal)
{
	switch_core_session_t *session = NULL;
	switch_channel_t *channel = NULL;
	ftdm_status_t status;
	uint32_t spanid;
	uint32_t chanid;
	ftdm_caller_data_t *caller_data;

	spanid = ftdm_channel_get_span_id(sigmsg->channel);
	chanid = ftdm_channel_get_id(sigmsg->channel);
	caller_data = ftdm_channel_get_caller_data(sigmsg->channel);

	ftdm_log(FTDM_LOG_DEBUG, "got FXO sig %d:%d [%s]\n", spanid, chanid, ftdm_signal_event2str(sigmsg->event_id));

    switch(sigmsg->event_id) {

    case FTDM_SIGEVENT_PROGRESS_MEDIA:
		{
			if ((session = ftdm_channel_get_session(sigmsg->channel, 0))) {
				channel = switch_core_session_get_channel(session);
				switch_channel_mark_pre_answered(channel);
				switch_core_session_rwunlock(session);
			}
		}
		break;
    case FTDM_SIGEVENT_STOP:
		{
			private_t *tech_pvt = NULL;
			while((session = ftdm_channel_get_session(sigmsg->channel, 0))) {
				tech_pvt = switch_core_session_get_private(session);
				switch_set_flag_locked(tech_pvt, TFLAG_DEAD);
				ftdm_channel_clear_token(sigmsg->channel, 0);
				channel = switch_core_session_get_channel(session);
				switch_channel_hangup(channel, caller_data->hangup_cause);
				ftdm_channel_clear_token(sigmsg->channel, switch_core_session_get_uuid(session));
				switch_core_session_rwunlock(session);
			}
		}
		break;
    case FTDM_SIGEVENT_UP:
		{
			if ((session = ftdm_channel_get_session(sigmsg->channel, 0))) {
				channel = switch_core_session_get_channel(session);
				switch_channel_mark_answered(channel);
				ftdm_enable_channel_dtmf(sigmsg->channel, channel);
				switch_core_session_rwunlock(session);
			}
		}
		break;
    case FTDM_SIGEVENT_START:
		{
			status = ftdm_channel_from_event(sigmsg, &session);
			if (status != FTDM_SUCCESS) {
				ftdm_channel_call_hangup(sigmsg->channel);
			}
		}
		break;
	case FTDM_SIGEVENT_SIGSTATUS_CHANGED: { /* twiddle */ } break;
	
	default:
		{
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "Unhandled msg type %d for channel %d:%d\n",
							  sigmsg->event_id, spanid, chanid);
		}
		break;

	}

	return FTDM_SUCCESS;
}

static FIO_SIGNAL_CB_FUNCTION(on_fxs_signal)
{
	switch_core_session_t *session = NULL;
	switch_channel_t *channel = NULL;
	ftdm_status_t status = FTDM_SUCCESS;
	uint32_t chanid, spanid, tokencount;

	chanid = ftdm_channel_get_id(sigmsg->channel);
	spanid = ftdm_channel_get_span_id(sigmsg->channel);
	tokencount = ftdm_channel_get_token_count(sigmsg->channel);

	ftdm_log(FTDM_LOG_DEBUG, "got FXS sig [%s]\n", ftdm_signal_event2str(sigmsg->event_id));

    switch(sigmsg->event_id) {
    case FTDM_SIGEVENT_UP:
		{
			if ((session = ftdm_channel_get_session(sigmsg->channel, 0))) {
				channel = switch_core_session_get_channel(session);
				switch_channel_mark_answered(channel);
				ftdm_enable_channel_dtmf(sigmsg->channel, channel);
				switch_core_session_rwunlock(session);
			}
		}
		break;
    case FTDM_SIGEVENT_PROGRESS:
		{
			if ((session = ftdm_channel_get_session(sigmsg->channel, 0))) {
				channel = switch_core_session_get_channel(session);
				switch_channel_mark_ring_ready(channel);
				switch_core_session_rwunlock(session);
			}
		}
		break;
    case FTDM_SIGEVENT_START:
		{
			status = ftdm_channel_from_event(sigmsg, &session);
			if (status != FTDM_SUCCESS) {
				ftdm_channel_call_indicate(sigmsg->channel, FTDM_CHANNEL_INDICATE_BUSY);
			}
		}
		break;
	
    case FTDM_SIGEVENT_STOP:
		{
			private_t *tech_pvt = NULL;
			switch_call_cause_t cause = SWITCH_CAUSE_NORMAL_CLEARING;
			if (tokencount) {
				ftdm_caller_data_t *caller_data = ftdm_channel_get_caller_data(sigmsg->channel);
				switch_core_session_t *session_a, *session_b, *session_t = NULL;
				switch_channel_t *channel_a = NULL, *channel_b = NULL;
				int digits = !zstr(caller_data->collected);
				const char *br_a_uuid = NULL, *br_b_uuid = NULL;
				private_t *tech_pvt = NULL;


				if ((session_a = switch_core_session_locate(ftdm_channel_get_token(sigmsg->channel, 0)))) {
					channel_a = switch_core_session_get_channel(session_a);
					br_a_uuid = switch_channel_get_variable(channel_a, SWITCH_SIGNAL_BOND_VARIABLE);

					tech_pvt = switch_core_session_get_private(session_a);
					stop_hold(session_a, switch_channel_get_variable(channel_a, SWITCH_SIGNAL_BOND_VARIABLE));
					switch_clear_flag_locked(tech_pvt, TFLAG_HOLD);
				}

				if ((session_b = switch_core_session_locate(ftdm_channel_get_token(sigmsg->channel, 1)))) {
					channel_b = switch_core_session_get_channel(session_b);
					br_b_uuid = switch_channel_get_variable(channel_b, SWITCH_SIGNAL_BOND_VARIABLE);

					tech_pvt = switch_core_session_get_private(session_b);
					stop_hold(session_a, switch_channel_get_variable(channel_b, SWITCH_SIGNAL_BOND_VARIABLE));
					switch_clear_flag_locked(tech_pvt, TFLAG_HOLD);
				}

				if (channel_a && channel_b &&  switch_channel_direction(channel_a) == SWITCH_CALL_DIRECTION_INBOUND && 
					switch_channel_direction(channel_b) == SWITCH_CALL_DIRECTION_INBOUND) {

					cause = SWITCH_CAUSE_ATTENDED_TRANSFER;
					if (br_a_uuid && br_b_uuid) {
						switch_ivr_uuid_bridge(br_a_uuid, br_b_uuid);
					} else if (br_a_uuid && digits) {
						session_t = switch_core_session_locate(br_a_uuid);
					} else if (br_b_uuid && digits) {
						session_t = switch_core_session_locate(br_b_uuid);
					}
				}
				
				if (session_t) {
					switch_ivr_session_transfer(session_t, caller_data->collected, NULL, NULL);
					switch_core_session_rwunlock(session_t);
				}

				if (session_a) {
					switch_core_session_rwunlock(session_a);
				}

				if (session_b) {
					switch_core_session_rwunlock(session_b);
				}

				
			}

			while((session = ftdm_channel_get_session(sigmsg->channel, 0))) {
				tech_pvt = switch_core_session_get_private(session);
				switch_set_flag_locked(tech_pvt, TFLAG_DEAD);
				channel = switch_core_session_get_channel(session);
				switch_channel_hangup(channel, cause);
				ftdm_channel_clear_token(sigmsg->channel, switch_core_session_get_uuid(session));
				switch_core_session_rwunlock(session);
			}
			ftdm_channel_clear_token(sigmsg->channel, NULL);
			
		}
		break;

    case FTDM_SIGEVENT_ADD_CALL:
		{
			cycle_foreground(sigmsg->channel, 1, NULL);
		}
		break;
    case FTDM_SIGEVENT_FLASH:
		{
			chan_pvt_t *chanpvt = ftdm_channel_get_private(sigmsg->channel);
			if (!chanpvt) {
				ftdm_log(FTDM_LOG_ERROR, "%d:%d has no private data, can't handle FXS features! (this is a bug)\n",
						chanid, spanid);
				break;
			}
			if (ftdm_channel_call_check_hold(sigmsg->channel) && tokencount == 1) {
				switch_core_session_t *session;
				if ((session = ftdm_channel_get_session(sigmsg->channel, 0))) {
					const char *buuid;
					switch_channel_t *channel;
					private_t *tech_pvt;
					
					tech_pvt = switch_core_session_get_private(session);
					channel = switch_core_session_get_channel(session);
					buuid = switch_channel_get_variable(channel, SWITCH_SIGNAL_BOND_VARIABLE);
					ftdm_channel_call_unhold(sigmsg->channel);
					stop_hold(session, buuid);
					switch_clear_flag_locked(tech_pvt, TFLAG_HOLD);
					switch_core_session_rwunlock(session);
				}
			} else if (tokencount == 2 && (SPAN_CONFIG[sigmsg->span_id].analog_options & ANALOG_OPTION_3WAY)) {
				if (switch_test_flag(chanpvt, ANALOG_OPTION_3WAY)) {
					switch_clear_flag(chanpvt, ANALOG_OPTION_3WAY);
					if ((session = ftdm_channel_get_session(sigmsg->channel, 1))) {
						channel = switch_core_session_get_channel(session);
						switch_channel_hangup(channel, SWITCH_CAUSE_NORMAL_CLEARING);
						ftdm_channel_clear_token(sigmsg->channel, switch_core_session_get_uuid(session));
						switch_core_session_rwunlock(session);
					}
					cycle_foreground(sigmsg->channel, 1, NULL);
				} else {
					char *cmd;
					cmd = switch_mprintf("three_way::%s", ftdm_channel_get_token(sigmsg->channel, 0));
					switch_set_flag(chanpvt, ANALOG_OPTION_3WAY);
					cycle_foreground(sigmsg->channel, 1, cmd);
					free(cmd);
				}
			} else if ((SPAN_CONFIG[sigmsg->span_id].analog_options & ANALOG_OPTION_CALL_SWAP)
					   || (SPAN_CONFIG[sigmsg->span_id].analog_options & ANALOG_OPTION_3WAY)
					   ) { 
				cycle_foreground(sigmsg->channel, 1, NULL);
				if (tokencount == 1) {
					ftdm_channel_call_hold(sigmsg->channel);
				}
			}
			
		}
		break;

    case FTDM_SIGEVENT_COLLECTED_DIGIT:
		{
			int span_id = ftdm_channel_get_span_id(sigmsg->channel);
			char *dtmf = sigmsg->ev_data.collected.digits;
			char *regex = SPAN_CONFIG[span_id].dial_regex;
			char *fail_regex = SPAN_CONFIG[span_id].fail_dial_regex;
			ftdm_caller_data_t *caller_data = ftdm_channel_get_caller_data(sigmsg->channel);
			
			if (zstr(regex)) {
				regex = NULL;
			}

			if (zstr(fail_regex)) {
				fail_regex = NULL;
			}

			ftdm_log(FTDM_LOG_DEBUG, "got DTMF sig [%s]\n", dtmf);
			switch_set_string(caller_data->collected, dtmf);
			
			if ((regex || fail_regex) && !zstr(dtmf)) {
				switch_regex_t *re = NULL;
				int ovector[30];
				int match = 0;

				if (fail_regex) {
					match = switch_regex_perform(dtmf, fail_regex, &re, ovector, sizeof(ovector) / sizeof(ovector[0]));
					status = match ? FTDM_SUCCESS : FTDM_BREAK;
					switch_regex_safe_free(re);
					ftdm_log(FTDM_LOG_DEBUG, "DTMF [%s] vs fail regex %s %s\n", dtmf, fail_regex, match ? "matched" : "did not match");
				}

				if (status == FTDM_SUCCESS && regex) {
					match = switch_regex_perform(dtmf, regex, &re, ovector, sizeof(ovector) / sizeof(ovector[0]));
					status = match ? FTDM_BREAK : FTDM_SUCCESS;
					switch_regex_safe_free(re);
					ftdm_log(FTDM_LOG_DEBUG, "DTMF [%s] vs dial regex %s %s\n", dtmf, regex, match ? "matched" : "did not match");
				}
				ftdm_log(FTDM_LOG_DEBUG, "returning %s to COLLECT event with DTMF %s\n", status == FTDM_SUCCESS ? "success" : "break", dtmf);
			}
		}
		break;

	default:
		{
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "Unhandled msg type %d for channel %d:%d\n",
							  sigmsg->event_id, spanid, chanid);
		}
		break;

	}

	return status;
}

static FIO_SIGNAL_CB_FUNCTION(on_r2_signal)
{
	uint32_t phyid, chanid, spanid;
	switch_core_session_t *session = NULL;
	switch_channel_t *channel = NULL;
	ftdm_status_t status = FTDM_SUCCESS;
	ftdm_caller_data_t *caller_data = ftdm_channel_get_caller_data(sigmsg->channel);

	phyid = ftdm_channel_get_ph_id(sigmsg->channel);
	chanid = ftdm_channel_get_id(sigmsg->channel);
	spanid = ftdm_channel_get_span_id(sigmsg->channel);

	ftdm_log(FTDM_LOG_DEBUG, "Got R2 channel sig [%s] in channel %d\n", ftdm_signal_event2str(sigmsg->event_id), phyid);

	if (on_common_signal(sigmsg) == FTDM_BREAK) {
		return FTDM_SUCCESS;
	}

	switch(sigmsg->event_id) {
		/* on_call_disconnect from the R2 side */
		case FTDM_SIGEVENT_STOP: 
		{	
			private_t *tech_pvt = NULL;
			while((session = ftdm_channel_get_session(sigmsg->channel, 0))) {
				tech_pvt = switch_core_session_get_private(session);
				switch_set_flag_locked(tech_pvt, TFLAG_DEAD);
				channel = switch_core_session_get_channel(session);
				switch_channel_hangup(channel, caller_data->hangup_cause);
				ftdm_channel_clear_token(sigmsg->channel, switch_core_session_get_uuid(session));
				switch_core_session_rwunlock(session);
			}
		}
		break;

		/* on_call_offered from the R2 side */
		case FTDM_SIGEVENT_START: 
		{
			status = ftdm_channel_from_event(sigmsg, &session);
		}
		break;
		
		/* on DNIS received from the R2 forward side, return status == FTDM_BREAK to stop requesting DNIS */
		case FTDM_SIGEVENT_COLLECTED_DIGIT: 
		{
			ftdm_caller_data_t *caller_data = ftdm_channel_get_caller_data(sigmsg->channel);
			int span_id = ftdm_channel_get_span_id(sigmsg->channel);
			char *regex = SPAN_CONFIG[span_id].dial_regex;
			char *fail_regex = SPAN_CONFIG[span_id].fail_dial_regex;

			if (zstr(regex)) {
				regex = NULL;
			}

			if (zstr(fail_regex)) {
				fail_regex = NULL;
			}

			ftdm_log(FTDM_LOG_DEBUG, "R2 DNIS so far [%s]\n", caller_data->dnis.digits);

			if ((regex || fail_regex) && !zstr(caller_data->dnis.digits)) {
				switch_regex_t *re = NULL;
				int ovector[30];
				int match = 0;

				if (fail_regex) {
					match = switch_regex_perform(caller_data->dnis.digits, fail_regex, &re, ovector, sizeof(ovector) / sizeof(ovector[0]));
					status = match ? FTDM_SUCCESS : FTDM_BREAK;
					switch_regex_safe_free(re);
				}

				if (status == FTDM_SUCCESS && regex) {
					match = switch_regex_perform(caller_data->dnis.digits, regex, &re, ovector, sizeof(ovector) / sizeof(ovector[0]));
					status = match ? FTDM_BREAK : FTDM_SUCCESS;
				}

				switch_regex_safe_free(re);
			}
		}
		break;

		case FTDM_SIGEVENT_PROGRESS:
		{
			if ((session = ftdm_channel_get_session(sigmsg->channel, 0))) {
				channel = switch_core_session_get_channel(session);
				switch_channel_mark_ring_ready(channel);
				switch_core_session_rwunlock(session);
			}
		}
		break;

		case FTDM_SIGEVENT_PROGRESS_MEDIA:
		{
			if ((session = ftdm_channel_get_session(sigmsg->channel, 0))) {
				channel = switch_core_session_get_channel(session);
				switch_channel_mark_pre_answered(channel);
				switch_core_session_rwunlock(session);
			}
		}
		break;

		case FTDM_SIGEVENT_UP:
		{
			if ((session = ftdm_channel_get_session(sigmsg->channel, 0))) {
				channel = switch_core_session_get_channel(session);
				switch_channel_mark_answered(channel);
				ftdm_enable_channel_dtmf(sigmsg->channel, channel);
				switch_core_session_rwunlock(session);
			}
		}
		break;

		case FTDM_SIGEVENT_SIGSTATUS_CHANGED:
		{
			ftdm_signaling_status_t sigstatus = sigmsg->ev_data.sigstatus.status;
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "%d:%d signalling changed to: %s\n",
					spanid, chanid, ftdm_signaling_status2str(sigstatus));
		}
		break;

		case FTDM_SIGEVENT_PROCEED:{} break;
		case FTDM_SIGEVENT_INDICATION_COMPLETED:{} break;

		default:
		{
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "Unhandled event %d from R2 for channel %d:%d\n",
			sigmsg->event_id, spanid, chanid);
		}
		break;
	}

	return status;
}

static FIO_SIGNAL_CB_FUNCTION(on_clear_channel_signal)
{
	switch_core_session_t *session = NULL;
	switch_channel_t *channel = NULL;
	ftdm_caller_data_t *caller_data;
	uint32_t spanid, chanid;

	ftdm_log(FTDM_LOG_DEBUG, "got clear channel sig [%s]\n", ftdm_signal_event2str(sigmsg->event_id));

	caller_data = ftdm_channel_get_caller_data(sigmsg->channel);
	chanid = ftdm_channel_get_id(sigmsg->channel);
	spanid = ftdm_channel_get_span_id(sigmsg->channel);

	if (on_common_signal(sigmsg) == FTDM_BREAK) {
		return FTDM_SUCCESS;
	}

    switch(sigmsg->event_id) {
    case FTDM_SIGEVENT_START:
		{
			return ftdm_channel_from_event(sigmsg, &session);
		}
		break;

    case FTDM_SIGEVENT_STOP:
    case FTDM_SIGEVENT_RESTART:
		{
			private_t *tech_pvt = NULL;
			while((session = ftdm_channel_get_session(sigmsg->channel, 0))) {
				tech_pvt = switch_core_session_get_private(session);
				switch_set_flag_locked(tech_pvt, TFLAG_DEAD);
				channel = switch_core_session_get_channel(session);
				switch_channel_hangup(channel, caller_data->hangup_cause);
				ftdm_channel_clear_token(sigmsg->channel, switch_core_session_get_uuid(session));
				switch_core_session_rwunlock(session);
			}
		}
		break;
    case FTDM_SIGEVENT_UP:
		{
			if ((session = ftdm_channel_get_session(sigmsg->channel, 0))) {
				channel = switch_core_session_get_channel(session);
				switch_channel_mark_answered(channel);
				ftdm_enable_channel_dtmf(sigmsg->channel, channel);
				switch_core_session_rwunlock(session);
			} else {
				const char *uuid = ftdm_channel_get_uuid(sigmsg->channel, 0);

				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Session for channel %d:%d not found [UUID: %s]\n",
					spanid, chanid, (uuid) ? uuid : "N/A");
			}
		}
    case FTDM_SIGEVENT_PROGRESS_MEDIA:
		{
			if ((session = ftdm_channel_get_session(sigmsg->channel, 0))) {
				channel = switch_core_session_get_channel(session);
				switch_channel_mark_pre_answered(channel);
				switch_core_session_rwunlock(session);
			} else {
				const char *uuid = ftdm_channel_get_uuid(sigmsg->channel, 0);

				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Session for channel %d:%d not found [UUID: %s]\n",
					spanid, chanid, (uuid) ? uuid : "N/A");
			}
		}
		break;
	case FTDM_SIGEVENT_PROGRESS:
	case FTDM_SIGEVENT_RINGING:
		{
			if ((session = ftdm_channel_get_session(sigmsg->channel, 0))) {
				channel = switch_core_session_get_channel(session);
				switch_channel_mark_ring_ready(channel);
				switch_core_session_rwunlock(session);
			} else {
				const char *uuid = ftdm_channel_get_uuid(sigmsg->channel, 0);

				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Session for channel %d:%d not found [UUID: %s]\n",
					spanid, chanid, (uuid) ? uuid : "N/A");
			}
		}
		break;	
	case FTDM_SIGEVENT_SIGSTATUS_CHANGED:
		{	
			ftdm_signaling_status_t sigstatus = sigmsg->ev_data.sigstatus.status;
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "%d:%d signalling changed to :%s\n",
					spanid, chanid, ftdm_signaling_status2str(sigstatus));
		}
		break;
	case FTDM_SIGEVENT_PROCEED:
	case FTDM_SIGEVENT_FACILITY:
		/* FS does not have handlers for these messages, so ignore them for now */
		break;
	default:
		{
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "Unhandled msg type %d for channel %d:%d\n",
							  sigmsg->event_id, spanid, chanid);
		}
		break;
	}

	return FTDM_SUCCESS;
}

static FIO_SIGNAL_CB_FUNCTION(on_analog_signal)
{
	uint32_t spanid, chanid;
	ftdm_chan_type_t type;
	switch_status_t status = SWITCH_STATUS_FALSE;

	spanid = ftdm_channel_get_span_id(sigmsg->channel);
	chanid = ftdm_channel_get_span_id(sigmsg->channel);
	type = ftdm_channel_get_type(sigmsg->channel);

	if (on_common_signal(sigmsg) == FTDM_BREAK) {
		return FTDM_SUCCESS;
	}

	switch (type) {
	case FTDM_CHAN_TYPE_FXO:
	case FTDM_CHAN_TYPE_EM:
		{
			status = on_fxo_signal(sigmsg);
		}
		break;
	case FTDM_CHAN_TYPE_FXS:
		{
			status = on_fxs_signal(sigmsg);
		}
		break;
	default: 
		{
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "Unhandled analog channel type %d for channel %d:%d\n",
							  type, spanid, chanid);
		}
		break;
	}

	return status;
}

static void ftdm_logger(const char *file, const char *func, int line, int level, const char *fmt, ...)
{
    char *data = NULL;
    va_list ap;
	
    va_start(ap, fmt);

	if (switch_vasprintf(&data, fmt, ap) != -1) {
		switch_log_printf(SWITCH_CHANNEL_ID_LOG, file, (char *)func, line, NULL, level, "%s", data);
	}
	if (data) free(data);
    va_end(ap);

}

static uint32_t enable_analog_option(const char *str, uint32_t current_options)
{
	if (!strcasecmp(str, "3-way")) {
		current_options |= ANALOG_OPTION_3WAY;
		current_options &= ~ANALOG_OPTION_CALL_SWAP;
	} else if (!strcasecmp(str, "call-swap")) {
		current_options |= ANALOG_OPTION_CALL_SWAP;
		current_options &= ~ANALOG_OPTION_3WAY;
	}
	
	return current_options;
	
}

/* create ftdm_conf_node_t tree based on a fixed pattern XML configuration list 
 * last 2 args are for limited aka dumb recursivity
 * */
static int add_config_list_nodes(switch_xml_t swnode, ftdm_conf_node_t *rootnode, 
		const char *list_name, const char *list_element_name, 
		const char *sub_list_name, const char *sub_list_element_name)
{
	char *var, *val;
	switch_xml_t list;
	switch_xml_t element;
	switch_xml_t param;

	ftdm_conf_node_t *n_list;
	ftdm_conf_node_t *n_element;

	list = switch_xml_child(swnode, list_name);
	if (!list) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "no list %s found\n", list_name);
		return -1;
	}

	if ((FTDM_SUCCESS != ftdm_conf_node_create(list_name, &n_list, rootnode))) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "failed to create %s node\n", list_name);
		return -1;
	}

	for (element = switch_xml_child(list, list_element_name); element; element = element->next) {
		char *element_name = (char *) switch_xml_attr(element, "name");

		if (!element_name) {
			continue;
		}

		if ((FTDM_SUCCESS != ftdm_conf_node_create(list_element_name, &n_element, n_list))) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "failed to create %s node for %s\n", list_element_name, element_name);
			return -1;
		}
		ftdm_conf_node_add_param(n_element, "name", element_name);

		for (param = switch_xml_child(element, "param"); param; param = param->next) {
			var = (char *) switch_xml_attr_soft(param, "name");
			val = (char *) switch_xml_attr_soft(param, "value");
			ftdm_conf_node_add_param(n_element, var, val);
		}

		if (sub_list_name && sub_list_element_name) {
			if (add_config_list_nodes(element, n_element, sub_list_name, sub_list_element_name, NULL, NULL)) {
				return -1;
			}
		}
	}

	return 0;
}

static ftdm_conf_node_t *get_ss7_config_node(switch_xml_t cfg, const char *confname)
{
	switch_xml_t signode, ss7configs, isup, gen, param;
	ftdm_conf_node_t *rootnode, *list;
	char *var, *val;

	/* try to find the conf in the hash first */
	rootnode = switch_core_hash_find(globals.ss7_configs, confname);
	if (rootnode) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "ss7 config %s was found in the hash already\n", confname);
		return rootnode;
	}
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "not found %s config in hash, searching in xml ...\n", confname);

	signode = switch_xml_child(cfg, "signaling_configs");
	if (!signode) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "not found 'signaling_configs' XML config section\n");
		return NULL;
	}

	ss7configs = switch_xml_child(signode, "sngss7_configs");
	if (!ss7configs) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "not found 'sngss7_configs' XML config section\n");
		return NULL;
	}

	/* search the isup config */
	for (isup = switch_xml_child(ss7configs, "sng_isup"); isup; isup = isup->next) {
		char *name = (char *) switch_xml_attr(isup, "name");
		if (!name) {
			continue;
		}
		if (!strcasecmp(name, confname)) {
			break;
		}
	}

	if (!isup) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "not found '%s' sng_isup XML config section\n", confname);
		return NULL;
	}

	/* found our XML chunk, create the root node */
	if ((FTDM_SUCCESS != ftdm_conf_node_create("sng_isup", &rootnode, NULL))) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "failed to create root node for sng_isup config %s\n", confname);
		return NULL;
	}

	/* add sng_gen */
	gen = switch_xml_child(isup, "sng_gen");
	if (gen == NULL) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "failed to process sng_gen for sng_isup config %s\n", confname);
		ftdm_conf_node_destroy(rootnode);
		return NULL;
	}

	if ((FTDM_SUCCESS != ftdm_conf_node_create("sng_gen", &list, rootnode))) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "failed to create %s node for %s\n", "sng_gen", confname);
		ftdm_conf_node_destroy(rootnode);
		return NULL;
	}

	for (param = switch_xml_child(gen, "param"); param; param = param->next) {
		var = (char *) switch_xml_attr_soft(param, "name");
		val = (char *) switch_xml_attr_soft(param, "value");
		ftdm_conf_node_add_param(list, var, val);
	}

	/* add relay channels */
	if (add_config_list_nodes(isup, rootnode, "sng_relay", "relay_channel", NULL, NULL)) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "failed to process sng_relay for sng_isup config %s\n", confname);
		ftdm_conf_node_destroy(rootnode);
		return NULL;
	}

	/* add mtp1 links */
	if (add_config_list_nodes(isup, rootnode, "mtp1_links", "mtp1_link", NULL, NULL)) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "failed to process mtp1_links for sng_isup config %s\n", confname);
		ftdm_conf_node_destroy(rootnode);
		return NULL;
	}

	/* add mtp2 links */
	if (add_config_list_nodes(isup, rootnode, "mtp2_links", "mtp2_link", NULL, NULL)) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "failed to process mtp2_links for sng_isup config %s\n", confname);
		ftdm_conf_node_destroy(rootnode);
		return NULL;
	}

	/* add mtp3 links */
	if (add_config_list_nodes(isup, rootnode, "mtp3_links", "mtp3_link", NULL, NULL)) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "failed to process mtp3_links for sng_isup config %s\n", confname);
		ftdm_conf_node_destroy(rootnode);
		return NULL;
	}

	/* add mtp linksets */
	if (add_config_list_nodes(isup, rootnode, "mtp_linksets", "mtp_linkset", NULL, NULL)) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "failed to process mtp_linksets for sng_isup config %s\n", confname);
		ftdm_conf_node_destroy(rootnode);
		return NULL;
	}

	/* add mtp routes */
	if (add_config_list_nodes(isup, rootnode, "mtp_routes", "mtp_route", "linksets", "linkset")) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "failed to process mtp_routes for sng_isup config %s\n", confname);
		ftdm_conf_node_destroy(rootnode);
		return NULL;
	}

	/* add isup interfaces */
	if (add_config_list_nodes(isup, rootnode, "isup_interfaces", "isup_interface", NULL, NULL)) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "failed to process isup_interfaces for sng_isup config %s\n", confname);
		ftdm_conf_node_destroy(rootnode);
		return NULL;
	}

	/* add cc spans */
	if (add_config_list_nodes(isup, rootnode, "cc_spans", "cc_span", NULL, NULL)) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "failed to process cc_spans for sng_isup config %s\n", confname);
		ftdm_conf_node_destroy(rootnode);
		return NULL;
	}

	switch_core_hash_insert(globals.ss7_configs, confname, rootnode);

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "Added SS7 node configuration %s\n", confname);
	return rootnode;
}

static int add_profile_parameters(switch_xml_t cfg, const char *profname, ftdm_conf_parameter_t *parameters, int len)
{
	switch_xml_t profnode, profile, param;
	int paramindex = 0;

	profnode = switch_xml_child(cfg, "config_profiles");
	if (!profnode) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "cannot find profile '%s', there is no 'config_profiles' XML section\n", profname);
		return 0;
	}

	/* search the profile */
	for (profile = switch_xml_child(profnode, "profile"); profile; profile = profile->next) {
		char *name = (char *) switch_xml_attr(profile, "name");
		if (!name) {
			continue;
		}
		if (!strcasecmp(name, profname)) {
			break;
		}
	}

	if (!profile) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "failed to find profile '%s'\n", profname);
		return 0;
	}

	for (param = switch_xml_child(profile, "param"); param; param = param->next) {
		char *var = (char *) switch_xml_attr_soft(param, "name");
		char *val = (char *) switch_xml_attr_soft(param, "value");
		if (!var || !val) {
			continue;
		}
		parameters[paramindex].var = var;
		parameters[paramindex].val = val;
		paramindex++;
	}

	return paramindex;
}

static void parse_bri_pri_spans(switch_xml_t cfg, switch_xml_t spans)
{
	switch_xml_t myspan, param;

	for (myspan = switch_xml_child(spans, "span"); myspan; myspan = myspan->next) {
		ftdm_status_t zstatus = FTDM_FAIL;
		const char *context = "default";
		const char *dialplan = "XML";
		ftdm_conf_parameter_t spanparameters[30];
		char *id = (char *) switch_xml_attr(myspan, "id");
		char *name = (char *) switch_xml_attr(myspan, "name");
		char *configname = (char *) switch_xml_attr(myspan, "cfgprofile");
		ftdm_span_t *span = NULL;
		uint32_t span_id = 0;
		unsigned paramindex = 0;

		if (!name && !id) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "sangoma isdn span missing required attribute 'id' or 'name', skipping ...\n");
			continue;
		}

		if (name) {
			zstatus = ftdm_span_find_by_name(name, &span);
		} else {
			if (switch_is_number(id)) {
				span_id = atoi(id);
				zstatus = ftdm_span_find(span_id, &span);
			}

			if (zstatus != FTDM_SUCCESS) {
				zstatus = ftdm_span_find_by_name(id, &span);
			}
		}

		if (zstatus != FTDM_SUCCESS) {
			ftdm_log(FTDM_LOG_ERROR, "Error finding FreeTDM span id:%s name:%s\n", switch_str_nil(id), switch_str_nil(name));
			continue;
		}
		
		if (!span_id) {
			span_id = ftdm_span_get_id(span);
		}

		memset(spanparameters, 0, sizeof(spanparameters));
		paramindex = 0;

		if (configname) {
			paramindex = add_profile_parameters(cfg, configname, spanparameters, ftdm_array_len(spanparameters));
			if (paramindex) {
				ftdm_log(FTDM_LOG_DEBUG, "Added %d parameters from profile %s for span %d\n", paramindex, configname, span_id);
			}
		}

		/* some defaults first */
		SPAN_CONFIG[span_id].limit_backend = "hash";
		SPAN_CONFIG[span_id].limit_reset_event = FTDM_LIMIT_RESET_ON_TIMEOUT;

		for (param = switch_xml_child(myspan, "param"); param; param = param->next) {
			char *var = (char *) switch_xml_attr_soft(param, "name");
			char *val = (char *) switch_xml_attr_soft(param, "value");

			if (ftdm_array_len(spanparameters) - 1 == paramindex) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Too many parameters for ss7 span, ignoring any parameter after %s\n", var);
				break;
			}

			if (!strcasecmp(var, "context")) {
				context = val;
			} else if (!strcasecmp(var, "dialplan")) {
				dialplan = val;
			} else if (!strcasecmp(var, "call_limit_backend")) {
				SPAN_CONFIG[span_id].limit_backend = val;
				ftdm_log(FTDM_LOG_DEBUG, "Using limit backend %s for span %d\n", SPAN_CONFIG[span_id].limit_backend, span_id);
			} else if (!strcasecmp(var, "call_limit_rate")) {
				int calls;
				int seconds;
				if (sscanf(val, "%d/%d", &calls, &seconds) != 2) {
					ftdm_log(FTDM_LOG_ERROR, "Invalid %s parameter, format example: 3/1 for 3 calls per second\n", var);
				} else {
					if (calls < 1 || seconds < 1) {
						ftdm_log(FTDM_LOG_ERROR, "Invalid %s parameter value, minimum call limit must be 1 per second\n", var);
					} else {
						SPAN_CONFIG[span_id].limit_calls = calls;
						SPAN_CONFIG[span_id].limit_seconds = seconds;
					}
				}
			} else if (!strcasecmp(var, "call_limit_reset_event")) {
				if (!strcasecmp(val, "answer")) {
					SPAN_CONFIG[span_id].limit_reset_event = FTDM_LIMIT_RESET_ON_ANSWER;
				} else {
					ftdm_log(FTDM_LOG_ERROR, "Invalid %s parameter value, only accepted event is 'answer'\n", var);
				}
			} else {
				spanparameters[paramindex].var = var;
				spanparameters[paramindex].val = val;
				paramindex++;
			}
		}

		if (ftdm_configure_span_signaling(span, 
						  "sangoma_isdn", 
						  on_clear_channel_signal,
						  spanparameters) != FTDM_SUCCESS) {
			ftdm_log(FTDM_LOG_ERROR, "Error configuring Sangoma ISDN FreeTDM span %d\n", span_id);
			continue;
		}
		SPAN_CONFIG[span_id].span = span;
		switch_copy_string(SPAN_CONFIG[span_id].context, context, sizeof(SPAN_CONFIG[span_id].context));
		switch_copy_string(SPAN_CONFIG[span_id].dialplan, dialplan, sizeof(SPAN_CONFIG[span_id].dialplan));
		switch_copy_string(SPAN_CONFIG[span_id].type, "Sangoma (ISDN)", sizeof(SPAN_CONFIG[span_id].type));
		ftdm_log(FTDM_LOG_DEBUG, "Configured Sangoma ISDN FreeTDM span %d\n", span_id);
		ftdm_span_start(span);
	}
}

static switch_status_t load_config(void)
{
	const char *cf = "freetdm.conf";
	switch_xml_t cfg, xml, settings, param, spans, myspan;
	ftdm_conf_node_t *ss7confnode = NULL;
	ftdm_span_t *boost_spans[FTDM_MAX_PHYSICAL_SPANS_PER_LOGICAL_SPAN];
	ftdm_span_t *boost_span = NULL;
	unsigned boosti = 0;
	unsigned int i = 0;
	ftdm_channel_t *fchan = NULL;
	ftdm_iterator_t *chaniter = NULL;
	ftdm_iterator_t *curr = NULL;

	memset(boost_spans, 0, sizeof(boost_spans));
	memset(&globals, 0, sizeof(globals));
	switch_mutex_init(&globals.mutex, SWITCH_MUTEX_NESTED, module_pool);
	if (!(xml = switch_xml_open_cfg(cf, &cfg, NULL))) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "open of %s failed\n", cf);
		return SWITCH_STATUS_TERM;
	}
	
	if ((settings = switch_xml_child(cfg, "settings"))) {
		for (param = switch_xml_child(settings, "param"); param; param = param->next) {
			char *var = (char *) switch_xml_attr_soft(param, "name");
			char *val = (char *) switch_xml_attr_soft(param, "value");

			if (!strcasecmp(var, "debug")) {
				globals.debug = atoi(val);
			} else if (!strcasecmp(var, "hold-music")) {
				switch_set_string(globals.hold_music, val);
			} else if (!strcasecmp(var, "crash-on-assert")) {
				globals.crash_on_assert = switch_true(val);
			} else if (!strcasecmp(var, "sip-headers")) {
				globals.sip_headers = switch_true(val);
			} else if (!strcasecmp(var, "enable-analog-option")) {
				globals.analog_options = enable_analog_option(val, globals.analog_options);
			}
		}
	}

	if ((spans = switch_xml_child(cfg, "sangoma_pri_spans"))) { 
		parse_bri_pri_spans(cfg, spans);
	}

	if ((spans = switch_xml_child(cfg, "sangoma_bri_spans"))) {
		parse_bri_pri_spans(cfg, spans);
	}

	switch_core_hash_init(&globals.ss7_configs, module_pool);
	if ((spans = switch_xml_child(cfg, "sangoma_ss7_spans"))) {
		for (myspan = switch_xml_child(spans, "span"); myspan; myspan = myspan->next) {
			ftdm_status_t zstatus = FTDM_FAIL;
			const char *context = "default";
			const char *dialplan = "XML";
			ftdm_conf_parameter_t spanparameters[30];
			char *id = (char *) switch_xml_attr(myspan, "id");
			char *name = (char *) switch_xml_attr(myspan, "name");
			char *configname = (char *) switch_xml_attr(myspan, "cfgprofile");
			ftdm_span_t *span = NULL;
			uint32_t span_id = 0;
			unsigned paramindex = 0;
			if (!name && !id) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "ss7 span missing required attribute 'id' or 'name', skipping ...\n");
				continue;
			}
			if (!configname) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "ss7 span missing required attribute, skipping ...\n");
				continue;
			}
			if (name) {
				zstatus = ftdm_span_find_by_name(name, &span);
			} else {
				if (switch_is_number(id)) {
					span_id = atoi(id);
					zstatus = ftdm_span_find(span_id, &span);
				}

				if (zstatus != FTDM_SUCCESS) {
					zstatus = ftdm_span_find_by_name(id, &span);
				}
			}

			if (zstatus != FTDM_SUCCESS) {
				ftdm_log(FTDM_LOG_ERROR, "Error finding FreeTDM span id:%s name:%s\n", switch_str_nil(id), switch_str_nil(name));
				continue;
			}
			
			if (!span_id) {
				span_id = ftdm_span_get_id(span);
			}

			ss7confnode = get_ss7_config_node(cfg, configname);
			if (!ss7confnode) {
				ftdm_log(FTDM_LOG_ERROR, "Error finding ss7config '%s' for FreeTDM span id: %s\n", configname, switch_str_nil(id));
				continue;
			}

			memset(spanparameters, 0, sizeof(spanparameters));
			paramindex = 0;
			spanparameters[paramindex].var = "confnode";
			spanparameters[paramindex].ptr = ss7confnode;
			paramindex++;
			for (param = switch_xml_child(myspan, "param"); param; param = param->next) {
				char *var = (char *) switch_xml_attr_soft(param, "name");
				char *val = (char *) switch_xml_attr_soft(param, "value");

				if (ftdm_array_len(spanparameters) - 1 == paramindex) {
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Too many parameters for ss7 span, ignoring any parameter after %s\n", var);
					break;
				}

				if (!strcasecmp(var, "context")) {
					context = val;
				} else if (!strcasecmp(var, "dialplan")) {
					dialplan = val;
				} else {
					spanparameters[paramindex].var = var;
					spanparameters[paramindex].val = val;
					paramindex++;
				}
			}

			if (ftdm_configure_span_signaling(span, 
						          "sangoma_ss7", 
						          on_clear_channel_signal,
							  spanparameters) != FTDM_SUCCESS) {
				ftdm_log(FTDM_LOG_ERROR, "Error configuring ss7 FreeTDM span %d\n", span_id);
				continue;
			}
			SPAN_CONFIG[span_id].span = span;
			switch_copy_string(SPAN_CONFIG[span_id].context, context, sizeof(SPAN_CONFIG[span_id].context));
			switch_copy_string(SPAN_CONFIG[span_id].dialplan, dialplan, sizeof(SPAN_CONFIG[span_id].dialplan));
			switch_copy_string(SPAN_CONFIG[span_id].type, "Sangoma (SS7)", sizeof(SPAN_CONFIG[span_id].type));
			ftdm_log(FTDM_LOG_DEBUG, "Configured ss7 FreeTDM span %d with config node %s\n", span_id, configname);
			ftdm_span_start(span);
		}
	}

	if ((spans = switch_xml_child(cfg, "analog_spans"))) {
		for (myspan = switch_xml_child(spans, "span"); myspan; myspan = myspan->next) {
			char *id = (char *) switch_xml_attr(myspan, "id");
			char *name = (char *) switch_xml_attr(myspan, "name");
			ftdm_status_t zstatus = FTDM_FAIL;
			const char *context = "default";
			const char *dialplan = "XML";
			const char *tonegroup = NULL;
			char *digit_timeout = NULL;
			char *max_digits = NULL;
			char *hotline = NULL;
			char *dial_regex = NULL;
			char *hold_music = NULL;
			char *fail_dial_regex = NULL;
			const char *enable_callerid = "true";
			const char *answer_polarity = "false";
			const char *hangup_polarity = "false";
			int polarity_delay = 600;
			int callwaiting = 1;
			int dialtone_timeout = 5000;

			uint32_t span_id = 0, to = 0, max = 0;
			ftdm_span_t *span = NULL;
			analog_option_t analog_options = ANALOG_OPTION_NONE;

			if (name) {
				zstatus = ftdm_span_find_by_name(name, &span);
			} else {
				if (switch_is_number(id)) {
					span_id = atoi(id);
					zstatus = ftdm_span_find(span_id, &span);
				}

				if (zstatus != FTDM_SUCCESS) {
					zstatus = ftdm_span_find_by_name(id, &span);
				}
			}

			if (zstatus != FTDM_SUCCESS) {
				ftdm_log(FTDM_LOG_ERROR, "Error finding FreeTDM span id:%s name:%s\n", switch_str_nil(id), switch_str_nil(name));
				continue;
			}
			
			if (!span_id) {
				span_id = ftdm_span_get_id(span);
			}

			/* some defaults first */
			SPAN_CONFIG[span_id].limit_backend = "hash";
			SPAN_CONFIG[span_id].limit_reset_event = FTDM_LIMIT_RESET_ON_TIMEOUT;
			
			for (param = switch_xml_child(myspan, "param"); param; param = param->next) {
				char *var = (char *) switch_xml_attr_soft(param, "name");
				char *val = (char *) switch_xml_attr_soft(param, "value");

				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "analog_spans var = %s\n", var);
				if (!strcasecmp(var, "tonegroup")) {
					tonegroup = val;
				} else if (!strcasecmp(var, "digit_timeout") || !strcasecmp(var, "digit-timeout")) {
					digit_timeout = val;
				} else if (!strcasecmp(var, "wait-dialtone-timeout")) {
					dialtone_timeout = atoi(val);
				} else if (!strcasecmp(var, "context")) {
					context = val;
				} else if (!strcasecmp(var, "dialplan")) {
					dialplan = val;
				} else if (!strcasecmp(var, "call_limit_backend")) {
					SPAN_CONFIG[span_id].limit_backend = val;
					ftdm_log(FTDM_LOG_DEBUG, "Using limit backend %s for span %d\n", SPAN_CONFIG[span_id].limit_backend, span_id);
				} else if (!strcasecmp(var, "call_limit_rate")) {
					int calls;
					int seconds;
					if (sscanf(val, "%d/%d", &calls, &seconds) != 2) {
						ftdm_log(FTDM_LOG_ERROR, "Invalid %s parameter, format example: 3/1 for 3 calls per second\n", var);
					} else {
						if (calls < 1 || seconds < 1) {
							ftdm_log(FTDM_LOG_ERROR, "Invalid %s parameter value, minimum call limit must be 1 per second\n", var);
						} else {
							SPAN_CONFIG[span_id].limit_calls = calls;
							SPAN_CONFIG[span_id].limit_seconds = seconds;
						}
					}
				} else if (!strcasecmp(var, "call_limit_reset_event")) {
					if (!strcasecmp(val, "answer")) {
						SPAN_CONFIG[span_id].limit_reset_event = FTDM_LIMIT_RESET_ON_ANSWER;
					} else {
						ftdm_log(FTDM_LOG_ERROR, "Invalid %s parameter value, only accepted event is 'answer'\n", var);
					}
				} else if (!strcasecmp(var, "dial-regex")) {
					dial_regex = val;
				} else if (!strcasecmp(var, "enable-callerid")) {
					enable_callerid = val;
				} else if (!strcasecmp(var, "answer-polarity-reverse")) {
					answer_polarity = val;
				} else if (!strcasecmp(var, "hangup-polarity-reverse")) {
					hangup_polarity = val;
				} else if (!strcasecmp(var, "polarity-delay")) {
					polarity_delay = atoi(val);
				} else if (!strcasecmp(var, "fail-dial-regex")) {
					fail_dial_regex = val;
				} else if (!strcasecmp(var, "hold-music")) {
					hold_music = val;
				} else if (!strcasecmp(var, "max_digits") || !strcasecmp(var, "max-digits")) {
					max_digits = val;
				} else if (!strcasecmp(var, "hotline")) {
					hotline = val;
				} else if (!strcasecmp(var, "callwaiting")) {
					callwaiting = switch_true(val) ? 1 : 0;
				} else if (!strcasecmp(var, "enable-analog-option")) {
					analog_options = enable_analog_option(val, analog_options);
				}
			}
				
			if (!id && !name) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "span missing required param 'id'\n");
				continue;
			}
			
			if (!tonegroup) {
				tonegroup = "us";
			}
			
			if (digit_timeout) {
				to = atoi(digit_timeout);
			}

			if (max_digits) {
				max = atoi(max_digits);
			}

			if (name) {
				zstatus = ftdm_span_find_by_name(name, &span);
			} else {
				if (switch_is_number(id)) {
					span_id = atoi(id);
					zstatus = ftdm_span_find(span_id, &span);
				}

				if (zstatus != FTDM_SUCCESS) {
					zstatus = ftdm_span_find_by_name(id, &span);
				}
			}

			if (zstatus != FTDM_SUCCESS) {
				ftdm_log(FTDM_LOG_ERROR, "Error finding FreeTDM span id:%s name:%s\n", switch_str_nil(id), switch_str_nil(name));
				continue;
			}
			
			if (!span_id) {
				span_id = ftdm_span_get_id(span);
			}

			if (ftdm_configure_span(span, "analog", on_analog_signal, 
								   "tonemap", tonegroup, 
								   "digit_timeout", &to,
								   "max_dialstr", &max,
								   "hotline", hotline ? hotline : "",
								   "enable_callerid", enable_callerid,
								   "answer_polarity_reverse", answer_polarity,
								   "hangup_polarity_reverse", hangup_polarity,
								   "polarity_delay", &polarity_delay,
								   "callwaiting", &callwaiting,
								   "wait_dialtone_timeout", &dialtone_timeout,
								   FTDM_TAG_END) != FTDM_SUCCESS) {
				ftdm_log(FTDM_LOG_ERROR, "Error configuring FreeTDM analog span %s\n", ftdm_span_get_name(span));
				continue;
			}

			SPAN_CONFIG[span_id].span = span;
			switch_set_string(SPAN_CONFIG[span_id].context, context);
			switch_set_string(SPAN_CONFIG[span_id].dialplan, dialplan);
			SPAN_CONFIG[span_id].analog_options = analog_options | globals.analog_options;
			
			chaniter = ftdm_span_get_chan_iterator(span, NULL);
			curr = chaniter;
			for (curr = chaniter; curr; curr = ftdm_iterator_next(curr)) {
				fchan = ftdm_iterator_current(curr);
				ftdm_channel_set_private(fchan, &SPAN_CONFIG[span_id].pvts[i]);
			}
			ftdm_iterator_free(chaniter);
			
			if (dial_regex) {
				switch_set_string(SPAN_CONFIG[span_id].dial_regex, dial_regex);
			}

			if (fail_dial_regex) {
				switch_set_string(SPAN_CONFIG[span_id].fail_dial_regex, fail_dial_regex);
			}

			if (hold_music) {
				switch_set_string(SPAN_CONFIG[span_id].hold_music, hold_music);
			}
			switch_copy_string(SPAN_CONFIG[span_id].type, "analog", sizeof(SPAN_CONFIG[span_id].type));
			ftdm_span_start(span);
		}
	}

	if ((spans = switch_xml_child(cfg, "analog_em_spans"))) {
		for (myspan = switch_xml_child(spans, "span"); myspan; myspan = myspan->next) {
			char *id = (char *) switch_xml_attr(myspan, "id");
			char *name = (char *) switch_xml_attr(myspan, "name");
			ftdm_status_t zstatus = FTDM_FAIL;
			const char *context = "default";
			const char *dialplan = "XML";
			const char *tonegroup = NULL;
			char *digit_timeout = NULL;
			char *max_digits = NULL;
			char *dial_regex = NULL;
			char *hold_music = NULL;
			char *fail_dial_regex = NULL;
			uint32_t span_id = 0, to = 0, max = 0;
			ftdm_span_t *span = NULL;
			analog_option_t analog_options = ANALOG_OPTION_NONE;

			for (param = switch_xml_child(myspan, "param"); param; param = param->next) {
				char *var = (char *) switch_xml_attr_soft(param, "name");
				char *val = (char *) switch_xml_attr_soft(param, "value");

				if (!strcasecmp(var, "tonegroup")) {
					tonegroup = val;
				} else if (!strcasecmp(var, "digit_timeout") || !strcasecmp(var, "digit-timeout")) {
					digit_timeout = val;
				} else if (!strcasecmp(var, "context")) {
					context = val;
				} else if (!strcasecmp(var, "dialplan")) {
					dialplan = val;
				} else if (!strcasecmp(var, "dial-regex")) {
					dial_regex = val;
				} else if (!strcasecmp(var, "fail-dial-regex")) {
					fail_dial_regex = val;
				} else if (!strcasecmp(var, "hold-music")) {
					hold_music = val;
				} else if (!strcasecmp(var, "max_digits") || !strcasecmp(var, "max-digits")) {
					max_digits = val;
				} else if (!strcasecmp(var, "enable-analog-option")) {
					analog_options = enable_analog_option(val, analog_options);
				}
			}
				
			if (!id && !name) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "span missing required param 'id'\n");
				continue;
			}

			
			if (!tonegroup) {
				tonegroup = "us";
			}
			
			if (digit_timeout) {
				to = atoi(digit_timeout);
			}

			if (max_digits) {
				max = atoi(max_digits);
			}


			if (name) {
				zstatus = ftdm_span_find_by_name(name, &span);
			} else {
				if (switch_is_number(id)) {
					span_id = atoi(id);
					zstatus = ftdm_span_find(span_id, &span);
				}

				if (zstatus != FTDM_SUCCESS) {
					zstatus = ftdm_span_find_by_name(id, &span);
				}
			}

			if (zstatus != FTDM_SUCCESS) {
				ftdm_log(FTDM_LOG_ERROR, "Error finding FreeTDM span id:%s name:%s\n", switch_str_nil(id), switch_str_nil(name));
				continue;
			}
			
			if (!span_id) {
				span_id = ftdm_span_get_id(span);
			}


			if (ftdm_configure_span(span, "analog_em", on_analog_signal, 
								   "tonemap", tonegroup, 
								   "digit_timeout", &to,
								   "max_dialstr", &max,
								   FTDM_TAG_END) != FTDM_SUCCESS) {
				ftdm_log(FTDM_LOG_ERROR, "Error starting FreeTDM span %d\n", span_id);
				continue;
			}

			SPAN_CONFIG[span_id].span = span;
			switch_set_string(SPAN_CONFIG[span_id].context, context);
			switch_set_string(SPAN_CONFIG[span_id].dialplan, dialplan);
			SPAN_CONFIG[span_id].analog_options = analog_options | globals.analog_options;
			
			if (dial_regex) {
				switch_set_string(SPAN_CONFIG[span_id].dial_regex, dial_regex);
			}

			if (fail_dial_regex) {
				switch_set_string(SPAN_CONFIG[span_id].fail_dial_regex, fail_dial_regex);
			}

			if (hold_music) {
				switch_set_string(SPAN_CONFIG[span_id].hold_music, hold_music);
			}
			switch_copy_string(SPAN_CONFIG[span_id].type, "analog_em", sizeof(SPAN_CONFIG[span_id].type));
			ftdm_span_start(span);
		}
	}

	if ((spans = switch_xml_child(cfg, "pri_spans"))) {
		for (myspan = switch_xml_child(spans, "span"); myspan; myspan = myspan->next) {
			char *name = (char *) switch_xml_attr(myspan, "name");
			ftdm_conf_parameter_t spanparameters[10];
			ftdm_status_t zstatus = FTDM_FAIL;
			const char *context = "default";
			const char *dialplan = "XML";
			unsigned paramindex = 0;
			ftdm_span_t *span = NULL;
			uint32_t span_id = 0;

			if (!name) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "span missing required attribute 'name'\n");
				continue;
			}

			memset(spanparameters, 0, sizeof(spanparameters));

			for (param = switch_xml_child(myspan, "param"); param; param = param->next) {
				char *var = (char *) switch_xml_attr_soft(param, "name");
				char *val = (char *) switch_xml_attr_soft(param, "value");

				if (ftdm_array_len(spanparameters) - 1 == paramindex) {
					ftdm_log(FTDM_LOG_ERROR, "Too many parameters for pri span '%s', ignoring everything after '%s'\n", name, var);
					break;
				}

				if (ftdm_strlen_zero(var) || ftdm_strlen_zero(val)) {
					ftdm_log(FTDM_LOG_WARNING, "Skipping parameter with empty name or value\n");
					continue;
				}

				if (!strcasecmp(var, "context")) {
					context = val;
				} else if (!strcasecmp(var, "dialplan")) {
					dialplan = val;
				} else {
					spanparameters[paramindex].var = var;
					spanparameters[paramindex].val = val;
					paramindex++;
				}
			}

			zstatus = ftdm_span_find_by_name(name, &span);
			if (zstatus != FTDM_SUCCESS) {
				ftdm_log(FTDM_LOG_ERROR, "Error finding FreeTDM span %s\n", name);
				continue;
			}

			span_id = ftdm_span_get_id(span);
			if (ftdm_configure_span_signaling(span, "isdn", on_clear_channel_signal, spanparameters) != FTDM_SUCCESS) {
				ftdm_log(FTDM_LOG_ERROR, "Error configuring FreeTDM span %s\n", name);
				continue;
			}

			SPAN_CONFIG[span_id].span = span;
			switch_copy_string(SPAN_CONFIG[span_id].context, context, sizeof(SPAN_CONFIG[span_id].context));
			switch_copy_string(SPAN_CONFIG[span_id].dialplan, dialplan, sizeof(SPAN_CONFIG[span_id].dialplan));
			switch_copy_string(SPAN_CONFIG[span_id].type, "isdn", sizeof(SPAN_CONFIG[span_id].type));

			ftdm_span_start(span);
		}
	}

	if ((spans = switch_xml_child(cfg, "pritap_spans"))) {
		for (myspan = switch_xml_child(spans, "span"); myspan; myspan = myspan->next) {

			char *name = (char *) switch_xml_attr(myspan, "name");

			ftdm_status_t zstatus = FTDM_FAIL;
			unsigned paramindex = 0;
			ftdm_conf_parameter_t spanparameters[10];
			const char *context = "default";
			const char *dialplan = "XML";
			ftdm_span_t *span = NULL;
			int span_id = 0;

			if (!name) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "span missing required attribute 'name'\n");
				continue;
			}

			memset(spanparameters, 0, sizeof(spanparameters));

			for (param = switch_xml_child(myspan, "param"); param; param = param->next) {
				char *var = (char *) switch_xml_attr_soft(param, "name");
				char *val = (char *) switch_xml_attr_soft(param, "value");

				if (ftdm_array_len(spanparameters) - 1 == paramindex) {
					ftdm_log(FTDM_LOG_ERROR, "Too many parameters for pritap span '%s', ignoring everything after '%s'\n", name, var);
					break;
				}

				if (!strcasecmp(var, "context")) {
					context = val;
				} else if (!strcasecmp(var, "dialplan")) {
					dialplan = val;
				} else {
					spanparameters[paramindex].var = var;
					spanparameters[paramindex].val = val;
					paramindex++;
				}
			}
	
			zstatus = ftdm_span_find_by_name(name, &span);
			if (zstatus != FTDM_SUCCESS) {
				ftdm_log(FTDM_LOG_ERROR, "Error finding FreeTDM span %s\n", name);
				continue;
			}

			span_id = ftdm_span_get_id(span);
			if (ftdm_configure_span_signaling(span, "pritap", on_clear_channel_signal, spanparameters) != FTDM_SUCCESS) {
				ftdm_log(FTDM_LOG_ERROR, "Error configuring FreeTDM span %s\n", name);
				continue;
			}

			SPAN_CONFIG[span_id].span = span;
			switch_copy_string(SPAN_CONFIG[span_id].context, context, sizeof(SPAN_CONFIG[span_id].context));
			switch_copy_string(SPAN_CONFIG[span_id].dialplan, dialplan, sizeof(SPAN_CONFIG[span_id].dialplan));
			switch_copy_string(SPAN_CONFIG[span_id].type, "isdn", sizeof(SPAN_CONFIG[span_id].type));

			ftdm_span_start(span);
		}
	}


	if ((spans = switch_xml_child(cfg, "libpri_spans"))) {
		for (myspan = switch_xml_child(spans, "span"); myspan; myspan = myspan->next) {
			char *name = (char *) switch_xml_attr(myspan, "name");
			ftdm_conf_parameter_t spanparameters[10];
			ftdm_status_t zstatus = FTDM_FAIL;
			const char *context  = "default";
			const char *dialplan = "XML";
			unsigned paramindex = 0;
			ftdm_span_t *span = NULL;
			uint32_t span_id = 0;

			if (!name) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "span missing required attribute 'name'\n");
				continue;
			}

			memset(spanparameters, 0, sizeof(spanparameters));

			for (param = switch_xml_child(myspan, "param"); param; param = param->next) {
				char *var = (char *) switch_xml_attr_soft(param, "name");
				char *val = (char *) switch_xml_attr_soft(param, "value");

				if (ftdm_array_len(spanparameters) - 1 == paramindex) {
					ftdm_log(FTDM_LOG_ERROR, "Too many parameters for libpri span, ignoring everything after '%s'\n", var);
					break;
				}

				if (ftdm_strlen_zero(var) || ftdm_strlen_zero(val)) {
					ftdm_log(FTDM_LOG_WARNING, "Skipping parameter with empty name or value\n");
					continue;
				}

				if (!strcasecmp(var, "context")) {
					context = val;
				} else if (!strcasecmp(var, "dialplan")) {
					dialplan = val;
				} else {
					spanparameters[paramindex].var = var;
					spanparameters[paramindex].val = val;
					paramindex++;
				}
			}

			zstatus = ftdm_span_find_by_name(name, &span);
			if (zstatus != FTDM_SUCCESS) {
				ftdm_log(FTDM_LOG_ERROR, "Error finding FreeTDM span %s\n", name);
				continue;
			}

			span_id = ftdm_span_get_id(span);
			if (ftdm_configure_span_signaling(span, "libpri", on_clear_channel_signal, spanparameters) != FTDM_SUCCESS) {
				ftdm_log(FTDM_LOG_ERROR, "Error configuring FreeTDM span %s\n", name);
				continue;
			}

			SPAN_CONFIG[span_id].span = span;
			switch_copy_string(SPAN_CONFIG[span_id].context, context, sizeof(SPAN_CONFIG[span_id].context));
			switch_copy_string(SPAN_CONFIG[span_id].dialplan, dialplan, sizeof(SPAN_CONFIG[span_id].dialplan));
			switch_copy_string(SPAN_CONFIG[span_id].type, "isdn", sizeof(SPAN_CONFIG[span_id].type));

			ftdm_span_start(span);
		}
	}

	if ((spans = switch_xml_child(cfg, "boost_spans"))) {
		for (myspan = switch_xml_child(spans, "span"); myspan; myspan = myspan->next) {
			char *id = (char *) switch_xml_attr(myspan, "id");
			char *name = (char *) switch_xml_attr(myspan, "name");
			char *sigmod = (char *) switch_xml_attr(myspan, "sigmod");
			ftdm_status_t zstatus = FTDM_FAIL;
			const char *context = "default";
			const char *dialplan = "XML";
			uint32_t span_id = 0;
			ftdm_span_t *span = NULL;
			ftdm_conf_parameter_t spanparameters[30];
			unsigned paramindex = 0;
			
			if (!id && !name) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "boost span requires an id or name as attribute: <span id=ftid|name=ftname>\n");
				continue;
			}
			memset(spanparameters, 0, sizeof(spanparameters));
			if (sigmod) {
				spanparameters[paramindex].var = "sigmod";
				spanparameters[paramindex].val = sigmod;
				paramindex++;
			}

			for (param = switch_xml_child(myspan, "param"); param; param = param->next) {
				char *var = (char *) switch_xml_attr_soft(param, "name");
				char *val = (char *) switch_xml_attr_soft(param, "value");

				if (ftdm_array_len(spanparameters) - 1 == paramindex) {
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Too many parameters for boost span, ignoring any parameter after %s\n", var);
					break;
				}

				if (!strcasecmp(var, "context")) {
					context = val;
				} else if (!strcasecmp(var, "dialplan")) {
					dialplan = val;
				} else {
					spanparameters[paramindex].var = var;
					spanparameters[paramindex].val = val;
					paramindex++;
				}
			}

			if (name) {
				zstatus = ftdm_span_find_by_name(name, &span);
			} else {
				if (switch_is_number(id)) {
					span_id = atoi(id);
					zstatus = ftdm_span_find(span_id, &span);
				}

				if (zstatus != FTDM_SUCCESS) {
					zstatus = ftdm_span_find_by_name(id, &span);
				}
			}

			if (zstatus != FTDM_SUCCESS) {
				ftdm_log(FTDM_LOG_ERROR, "Error finding FreeTDM span id:%s name:%s\n", switch_str_nil(id), switch_str_nil(name));
				continue;
			}
			
			if (!span_id) {
				span_id = ftdm_span_get_id(span);
			}

			if (ftdm_configure_span_signaling(span, "sangoma_boost", on_clear_channel_signal, spanparameters) != FTDM_SUCCESS) {
				ftdm_log(FTDM_LOG_ERROR, "Error starting FreeTDM span %d error: %s\n", span_id, ftdm_span_get_last_error(span));
				continue;
			}

			SPAN_CONFIG[span_id].span = span;
			switch_copy_string(SPAN_CONFIG[span_id].context, context, sizeof(SPAN_CONFIG[span_id].context));
			switch_copy_string(SPAN_CONFIG[span_id].dialplan, dialplan, sizeof(SPAN_CONFIG[span_id].dialplan));

			switch_copy_string(SPAN_CONFIG[span_id].type, "Sangoma (boost)", sizeof(SPAN_CONFIG[span_id].type));
			boost_spans[boosti++] = span;
		}
	}

	if ((spans = switch_xml_child(cfg, "r2_spans"))) {
		for (myspan = switch_xml_child(spans, "span"); myspan; myspan = myspan->next) {
			char *name = (char *) switch_xml_attr(myspan, "name");
			char *configname = (char *) switch_xml_attr(myspan, "cfgprofile");
			ftdm_status_t zstatus = FTDM_FAIL;

			/* common non r2 stuff */
			const char *context = "default";
			const char *dialplan = "XML";
			char *dial_regex = NULL;
			char *fail_dial_regex = NULL;
			uint32_t span_id = 0;
			ftdm_span_t *span = NULL;

			ftdm_conf_parameter_t spanparameters[30];
			unsigned paramindex = 0;

			if (!name) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "'name' attribute required for R2 spans!\n");
				continue;
			}

			memset(spanparameters, 0, sizeof(spanparameters));

			if (configname) {
				paramindex = add_profile_parameters(cfg, configname, spanparameters, ftdm_array_len(spanparameters));
				if (paramindex) {
					ftdm_log(FTDM_LOG_DEBUG, "Added %d parameters from profile %s for span %d\n", paramindex, configname, span_id);
				}
			}

			for (param = switch_xml_child(myspan, "param"); param; param = param->next) {
				char *var = (char *) switch_xml_attr_soft(param, "name");
				char *val = (char *) switch_xml_attr_soft(param, "value");

				/* string parameters */
				if (!strcasecmp(var, "context")) {
					context = val;
				} else if (!strcasecmp(var, "dialplan")) {
					dialplan = val;
				} else if (!strcasecmp(var, "dial-regex")) {
					dial_regex = val;
				} else if (!strcasecmp(var, "fail-dial-regex")) {
					fail_dial_regex = val;
				} else {
					spanparameters[paramindex].var = var;
					spanparameters[paramindex].val = val;
					paramindex++;
				}
			}

			zstatus = ftdm_span_find_by_name(name, &span);
			if (zstatus != FTDM_SUCCESS) {
				ftdm_log(FTDM_LOG_ERROR, "Error finding FreeTDM R2 Span '%s'\n", name);
				continue;
			}
			span_id = ftdm_span_get_id(span);

			if (ftdm_configure_span_signaling(span, "r2", on_r2_signal, spanparameters) != FTDM_SUCCESS) {
				ftdm_log(FTDM_LOG_ERROR, "Error configuring FreeTDM R2 span %s, error: %s\n", 
				name, ftdm_span_get_last_error(span));
				continue;
			}

			if (dial_regex) {
				switch_set_string(SPAN_CONFIG[span_id].dial_regex, dial_regex);
			}

			if (fail_dial_regex) {
				switch_set_string(SPAN_CONFIG[span_id].fail_dial_regex, fail_dial_regex);
			}

			SPAN_CONFIG[span_id].span = span;
			switch_copy_string(SPAN_CONFIG[span_id].context, context, sizeof(SPAN_CONFIG[span_id].context));
			switch_copy_string(SPAN_CONFIG[span_id].dialplan, dialplan, sizeof(SPAN_CONFIG[span_id].dialplan));
			switch_copy_string(SPAN_CONFIG[span_id].type, "R2", sizeof(SPAN_CONFIG[span_id].type));

			if (ftdm_span_start(span) == FTDM_FAIL) {
				ftdm_log(FTDM_LOG_ERROR, "Error starting FreeTDM R2 span %s, error: %s\n", name, ftdm_span_get_last_error(span));
				continue;
			}
		}
	}

	/* start all boost spans now that we're done configuring. Unfortunately at this point boost modules have the limitation
	 * of needing all spans to be configured before starting them */
	for (i=0 ; i < boosti; i++) {
		boost_span = boost_spans[i];
		ftdm_log(FTDM_LOG_DEBUG, "Starting boost span %d\n", ftdm_span_get_id(boost_span));
		if (ftdm_span_start(boost_span) == FTDM_FAIL) {
			ftdm_log(FTDM_LOG_ERROR, "Error starting boost FreeTDM span %d, error: %s\n",
					ftdm_span_get_id(boost_span), ftdm_span_get_last_error(boost_span));
			continue;
		}
	}

	if (globals.crash_on_assert) {
		ftdm_log(FTDM_LOG_WARNING, "Crash on assert enabled\n");
		ftdm_global_set_crash_policy(FTDM_CRASH_ON_ASSERT);
	}

	switch_xml_free(xml);

	return SWITCH_STATUS_SUCCESS;
}

void dump_chan(ftdm_span_t *span, uint32_t chan_id, switch_stream_handle_t *stream)
{
	uint32_t span_id;
	uint32_t phspan_id, phchan_id;
	const char *chan_type;
	const char *state;
	const char *last_state;
	const char *uuid = NULL;
	char sessionid[255];
	float txgain, rxgain;
	switch_core_session_t *session = NULL;
	ftdm_alarm_flag_t alarmflag;
	ftdm_caller_data_t *caller_data;
	ftdm_channel_t *ftdmchan;
	ftdm_signaling_status_t sigstatus = FTDM_SIG_STATE_DOWN;

	if (chan_id > ftdm_span_get_chan_count(span)) {
		return;
	}

	strcpy(sessionid, "(none)");
	ftdmchan = ftdm_span_get_channel(span, chan_id);
	span_id = ftdm_span_get_id(span);

	phspan_id = ftdm_channel_get_ph_span_id(ftdmchan);
	phchan_id = ftdm_channel_get_ph_id(ftdmchan);
	chan_type = ftdm_chan_type2str(ftdm_channel_get_type(ftdmchan));
	state = ftdm_channel_get_state_str(ftdmchan);
	last_state = ftdm_channel_get_last_state_str(ftdmchan);
	ftdm_channel_command(ftdmchan, FTDM_COMMAND_GET_RX_GAIN, &rxgain);
	ftdm_channel_command(ftdmchan, FTDM_COMMAND_GET_TX_GAIN, &txgain);
	caller_data = ftdm_channel_get_caller_data(ftdmchan);
	ftdm_channel_get_sig_status(ftdmchan, &sigstatus);
	ftdm_channel_get_alarms(ftdmchan, &alarmflag);

	uuid = ftdm_channel_get_uuid(ftdmchan, 0);
	if (!zstr(uuid)) {
		if (!(session = switch_core_session_locate(uuid))) {
			snprintf(sessionid, sizeof(sessionid), "%s (dead)", uuid);
		} else {
			snprintf(sessionid, sizeof(sessionid), "%s", uuid);
			switch_core_session_rwunlock(session);
		}
	}

	stream->write_function(stream,
						   "span_id: %u\n"
						   "chan_id: %u\n"
						   "physical_span_id: %u\n"
						   "physical_chan_id: %u\n"
						   "physical_status: %s\n"
						   "signaling_status: %s\n"
						   "type: %s\n"
						   "state: %s\n"
						   "last_state: %s\n"
						   "txgain: %3.2f\n"
						   "rxgain: %3.2f\n"
						   "cid_date: %s\n"
						   "cid_name: %s\n"
						   "cid_num: %s\n"
						   "ani: %s\n"
						   "aniII: %s\n"
						   "dnis: %s\n"
						   "rdnis: %s\n"
						   "cause: %s\n"
						   "session: %s\n\n",
						   span_id,
						   chan_id,
						   phspan_id,
						   phchan_id,
					  	   alarmflag ? "alarmed" : "ok",
					           ftdm_signaling_status2str(sigstatus),
						   chan_type,
						   state,
						   last_state,
						   txgain,
						   rxgain,
						   caller_data->cid_date,
						   caller_data->cid_name,
						   caller_data->cid_num.digits,
						   caller_data->ani.digits,
						   caller_data->aniII,
						   caller_data->dnis.digits,
						   caller_data->rdnis.digits,
						   switch_channel_cause2str(caller_data->hangup_cause),
						   sessionid);
}

void dump_chan_xml(ftdm_span_t *span, uint32_t chan_id, switch_stream_handle_t *stream)
{
	uint32_t span_id;
	uint32_t phspan_id, phchan_id;
	const char *chan_type;
	const char *state;
	const char *last_state;
	float txgain, rxgain;
	ftdm_caller_data_t *caller_data;
	ftdm_channel_t *ftdmchan;
	ftdm_alarm_flag_t alarmflag;
	ftdm_signaling_status_t sigstatus = FTDM_SIG_STATE_DOWN;

	if (chan_id > ftdm_span_get_chan_count(span)) {
		return;
	}

	ftdmchan = ftdm_span_get_channel(span, chan_id);
	span_id = ftdm_span_get_id(span);

	phspan_id = ftdm_channel_get_ph_span_id(ftdmchan);
	phchan_id = ftdm_channel_get_ph_id(ftdmchan);
	chan_type = ftdm_chan_type2str(ftdm_channel_get_type(ftdmchan));
	state = ftdm_channel_get_state_str(ftdmchan);
	last_state = ftdm_channel_get_last_state_str(ftdmchan);
	ftdm_channel_command(ftdmchan, FTDM_COMMAND_GET_RX_GAIN, &rxgain);
	ftdm_channel_command(ftdmchan, FTDM_COMMAND_GET_TX_GAIN, &txgain);
	caller_data = ftdm_channel_get_caller_data(ftdmchan);
	ftdm_channel_get_sig_status(ftdmchan, &sigstatus);
	ftdm_channel_get_alarms(ftdmchan, &alarmflag);


	stream->write_function(stream,
						   " <channel>\n"
						   "  <span-id>%u</span-id>\n"
						   "  <chan-id>%u</chan-id>>\n"
						   "  <physical-span-id>%u</physical-span-id>\n"
						   "  <physical-chan-id>%u</physical-chan-id>\n"
						   "  <physical-status>%s</physical-status>\n"
						   "  <signaling-status>%s</signaling-status>\n"
						   "  <type>%s</type>\n"
						   "  <state>%s</state>\n"
						   "  <last-state>%s</last-state>\n"
						   "  <txgain>%3.2f</txgain>\n"
						   "  <rxgain>%3.2f</rxgain>\n"
						   "  <cid-date>%s</cid-date>\n"
						   "  <cid-name>%s</cid-name>\n"
						   "  <cid-num>%s</cid-num>\n"
						   "  <ani>%s</ani>\n"
						   "  <aniII>%s</aniII>\n"
						   "  <dnis>%s</dnis>\n"
						   "  <rdnis>%s</rdnis>\n"
						   "  <cause>%s</cause>\n"
						   " </channel>\n",
						   span_id,
						   chan_id,
						   phspan_id,
						   phchan_id,
						   alarmflag ? "alarmed" : "ok",
					     	   ftdm_signaling_status2str(sigstatus),
						   chan_type,
						   state,
						   last_state,
						   txgain,
						   rxgain,
						   caller_data->cid_date,
						   caller_data->cid_name,
						   caller_data->cid_num.digits,
						   caller_data->ani.digits,
						   caller_data->aniII,
						   caller_data->dnis.digits,
						   caller_data->rdnis.digits,
						   switch_channel_cause2str(caller_data->hangup_cause));
}

#define FT_SYNTAX "USAGE:\n" \
"--------------------------------------------------------------------------------\n" \
"ftdm list\n" \
"ftdm start|stop <span_name|span_id>\n" \
"ftdm restart <span_id|span_name> [<chan_id>]\n" \
"ftdm dump <span_id|span_name> [<chan_id>]\n" \
"ftdm sigstatus get|set [<span_id|span_name>] [<channel>] [<sigstatus>]\n" \
"ftdm trace <path> <span_id|span_name> [<chan_id>]\n" \
"ftdm notrace <span_id|span_name> [<chan_id>]\n" \
"ftdm q931_pcap <span_id> on|off [pcapfilename without suffix]\n" \
"ftdm gains <rxgain> <txgain> <span_id> [<chan_id>]\n" \
"ftdm dtmf on|off <span_id> [<chan_id>]\n" \
"ftdm queuesize <rxsize> <txsize> <span_id> [<chan_id>]\n" \
"--------------------------------------------------------------------------------\n"
SWITCH_STANDARD_API(ft_function)
{
	char *mycmd = NULL, *argv[10] = { 0 };
	int argc = 0;
	ftdm_iterator_t *chaniter = NULL;
	ftdm_iterator_t *curr = NULL;

	if (!zstr(cmd) && (mycmd = strdup(cmd))) {
		argc = switch_separate_string(mycmd, ' ', argv, (sizeof(argv) / sizeof(argv[0])));
	}

	if (!argc) {
		stream->write_function(stream, "%s", FT_SYNTAX);
		goto end;
	}

	if (!strcasecmp(argv[0], "sigstatus")) {
		ftdm_span_t *span = NULL;
		ftdm_signaling_status_t sigstatus;

		if (argc < 3) {
			stream->write_function(stream, "-ERR Usage: ftdm sigstatus get|set [<span_id>] [<channel>] [<sigstatus>]\n");
			goto end;
		}
		if (!strcasecmp(argv[1], "get") && argc < 3) {
			stream->write_function(stream, "-ERR sigstatus get usage: get <span_id>\n");
			goto end;
		}
		if (!strcasecmp(argv[1], "set") && argc != 5) {
			stream->write_function(stream, "-ERR sigstatus set usage: set <span_id> <channel>|all <sigstatus>\n");
			goto end;
		}

		ftdm_span_find_by_name(argv[2], &span);
		if (!span) {
			stream->write_function(stream, "-ERR invalid span\n");
			goto end;
		}

		if (!strcasecmp(argv[1], "get")) {
			if (argc == 4) {
				uint32_t chan_id = atol(argv[3]);
				ftdm_channel_t *fchan = ftdm_span_get_channel(span, chan_id);
				if (!fchan) {
					stream->write_function(stream, "-ERR failed to get channel id '%d'\n", chan_id);
					goto end;
				}

				if ((FTDM_SUCCESS == ftdm_channel_get_sig_status(fchan, &sigstatus))) {
					stream->write_function(stream, "channel %d signaling status: %s\n", chan_id, ftdm_signaling_status2str(sigstatus));
				} else {
					stream->write_function(stream, "-ERR failed to get channel sigstatus\n");
				}
				goto end;
			} else {
				if ((FTDM_SUCCESS == ftdm_span_get_sig_status(span, &sigstatus))) {
					stream->write_function(stream, "signaling_status: %s\n", ftdm_signaling_status2str(sigstatus));
				} else {
					stream->write_function(stream, "-ERR failed to read span status: %s\n", ftdm_span_get_last_error(span));
				}
			}
			goto end;
		}
		if (!strcasecmp(argv[1], "set")) {
			sigstatus = ftdm_str2ftdm_signaling_status(argv[4]);

			if (!strcasecmp(argv[3], "all")) {
				if ((FTDM_SUCCESS == ftdm_span_set_sig_status(span, sigstatus))) {
					stream->write_function(stream, "Signaling status of all channels from span %s set to %s\n",
							ftdm_span_get_name(span), ftdm_signaling_status2str(sigstatus));
				} else {
					stream->write_function(stream, "-ERR failed to set span sigstatus to '%s'\n", ftdm_signaling_status2str(sigstatus));
				}
				goto end;
			} else {
				uint32_t chan_id = atol(argv[3]);
				ftdm_channel_t *fchan = ftdm_span_get_channel(span, chan_id);
				if (!fchan) {
					stream->write_function(stream, "-ERR failed to get channel id '%d'\n", chan_id);
					goto end;
				}

				if ((FTDM_SUCCESS == ftdm_channel_set_sig_status(fchan, sigstatus))) {
					stream->write_function(stream, "Signaling status of channel %d set to %s\n", chan_id,
							ftdm_signaling_status2str(sigstatus));
				} else {
					stream->write_function(stream, "-ERR failed to set span sigstatus to '%s'\n", ftdm_signaling_status2str(sigstatus));
				}
				goto end;
			}
		}

	} else if (!strcasecmp(argv[0], "dump")) {
		if (argc < 2) {
			stream->write_function(stream, "-ERR Usage: ftdm dump <span_id> [<chan_id>]\n");
			goto end;
		} else {
			uint32_t chan_id = 0;
			ftdm_span_t *span;
			char *as = NULL;
			
			ftdm_span_find_by_name(argv[1], &span);
			
			if (argc > 2) {
				if (argv[3] && !strcasecmp(argv[2], "as")) {
					as = argv[3];
				} else {
					chan_id = atoi(argv[2]);
				}
			}

			if (argv[4] && !strcasecmp(argv[3], "as")) {
				as = argv[4];
			}

			if (!zstr(as) && !strcasecmp(as, "xml")) {
				stream->write_function(stream, "<channels>\n");
				if (!span) {
					stream->write_function(stream, "<error>invalid span</error>\n");
				} else {
					if (chan_id) {
						if(chan_id > ftdm_span_get_chan_count(span)) {
							stream->write_function(stream, "<error>invalid channel</error>\n");
						} else {
							dump_chan_xml(span, chan_id, stream);
						}
					} else {
						chaniter = ftdm_span_get_chan_iterator(span, NULL);
						for (curr = chaniter; curr; curr = ftdm_iterator_next(curr)) {
							dump_chan_xml(span, ftdm_channel_get_id(ftdm_iterator_current(curr)), stream);
						}
						ftdm_iterator_free(chaniter);
						
					}
				}
				stream->write_function(stream, "</channels>\n");
			} else {
				if (!span) {
					stream->write_function(stream, "-ERR invalid span\n");
				} else {
					if (chan_id) {
						if(chan_id > ftdm_span_get_chan_count(span)) {
							stream->write_function(stream, "-ERR invalid channel\n");
						} else {
							char *dbgstr = NULL;
							ftdm_channel_t *fchan = ftdm_span_get_channel(span, chan_id);
							dump_chan(span, chan_id, stream);
							dbgstr = ftdm_channel_get_history_str(fchan);
							stream->write_function(stream, "%s\n", dbgstr);
							ftdm_free(dbgstr);
						}
					} else {
						stream->write_function(stream, "+OK\n");
						chaniter = ftdm_span_get_chan_iterator(span, NULL);
						for (curr = chaniter; curr; curr = ftdm_iterator_next(curr)) {
							dump_chan(span, ftdm_channel_get_id(ftdm_iterator_current(curr)), stream);
						}
						ftdm_iterator_free(chaniter);
						
					}
				}
			}
		}
	} else if (!strcasecmp(argv[0], "list")) {
		int j;
		for (j = 0 ; j < FTDM_MAX_SPANS_INTERFACE; j++) {
			if (SPAN_CONFIG[j].span) {
				ftdm_channel_t *fchan;
				ftdm_alarm_flag_t alarmbits = FTDM_ALARM_NONE;
				const char *flags = "none";
				ftdm_signaling_status_t sigstatus;

				if (SPAN_CONFIG[j].analog_options & ANALOG_OPTION_3WAY) {
					flags = "3way";
				} else if (SPAN_CONFIG[j].analog_options & ANALOG_OPTION_CALL_SWAP) {
					flags = "call swap";
				}
				fchan = ftdm_span_get_channel(SPAN_CONFIG[j].span, 1);
				ftdm_channel_get_alarms(fchan, &alarmbits);
				
				if ((FTDM_SUCCESS == ftdm_span_get_sig_status(SPAN_CONFIG[j].span, &sigstatus))) {
					stream->write_function(stream,
										   "+OK\n"
										   "span: %u (%s)\n"
										   "type: %s\n"		
										   "physical_status: %s\n"
										   "signaling_status: %s\n"
										   "chan_count: %u\n"
										   "dialplan: %s\n"
										   "context: %s\n"
										   "dial_regex: %s\n"
										   "fail_dial_regex: %s\n"
										   "hold_music: %s\n"
										   "analog_options: %s\n",
										   j,
										   ftdm_span_get_name(SPAN_CONFIG[j].span),
										   SPAN_CONFIG[j].type,
										   alarmbits ? "alarmed" : "ok",
										   ftdm_signaling_status2str(sigstatus),
										   ftdm_span_get_chan_count(SPAN_CONFIG[j].span),
										   SPAN_CONFIG[j].dialplan,
										   SPAN_CONFIG[j].context,
										   SPAN_CONFIG[j].dial_regex,
										   SPAN_CONFIG[j].fail_dial_regex,
										   SPAN_CONFIG[j].hold_music,
										   flags
										   );
				} else {
					stream->write_function(stream,
										   "+OK\n"
										   "span: %u (%s)\n"
										   "type: %s\n"
										   "physical_status: %s\n"
										   "chan_count: %u\n"
										   "dialplan: %s\n"
										   "context: %s\n"
										   "dial_regex: %s\n"
										   "fail_dial_regex: %s\n"
										   "hold_music: %s\n"
										   "analog_options: %s\n",
										   j,
										   ftdm_span_get_name(SPAN_CONFIG[j].span),
										   SPAN_CONFIG[j].type,
										   alarmbits ? "alarmed" : "ok",
										   ftdm_span_get_chan_count(SPAN_CONFIG[j].span),
										   SPAN_CONFIG[j].dialplan,
										   SPAN_CONFIG[j].context,
										   SPAN_CONFIG[j].dial_regex,
										   SPAN_CONFIG[j].fail_dial_regex,
										   SPAN_CONFIG[j].hold_music,
										   flags
										   );
				}
			}
		}
	} else if (!strcasecmp(argv[0], "stop") || !strcasecmp(argv[0], "start")) {
		char *span_name = argv[1];
		ftdm_span_t *span = NULL;
		ftdm_status_t status;

		if (span_name) {
			ftdm_span_find_by_name(span_name, &span);
		}

		if (!span) {
			stream->write_function(stream, "-ERR no span\n");
			goto end;
		}
		
		if (!strcasecmp(argv[0], "stop")) {
			status = ftdm_span_stop(span);
		} else {
			status = ftdm_span_start(span);
		}
		
		stream->write_function(stream, status == FTDM_SUCCESS ? "+OK\n" : "-ERR failure\n");
		
		goto end;

		/*Q931ToPcap enhancement*/
	} else if (!strcasecmp(argv[0], "q931_pcap")) {
		int32_t span_id = 0;
                ftdm_span_t *span;
		const char *pcapfn = NULL;
		char *tmp_path = NULL;

                if (argc < 3) {
                        stream->write_function(stream, "-ERR Usage: ftdm q931_pcap <span_id> on|off [pcapfilename without suffix]\n");
                        goto end;
                }
		span_id = atoi(argv[1]);
		if (!(span_id && (span = SPAN_CONFIG[span_id].span))) {
                                stream->write_function(stream, "-ERR invalid span\n");
				goto end;
                } 

		/*Look for a given file name or use default file name*/
		if (argc > 3) {
			if(argv[3]){
				pcapfn=argv[3];
			}
		}
		else {
			pcapfn="q931";
		}

		/*Add log directory path to file name*/
		tmp_path=switch_mprintf("%s%s%s.pcap", SWITCH_GLOBAL_dirs.log_dir, SWITCH_PATH_SEPARATOR, pcapfn);
		
		if(!strcasecmp(argv[2], "on")) {
			if (ftdm_configure_span(span, "isdn", on_clear_channel_signal, "q931topcap", 1, "pcapfilename", tmp_path, FTDM_TAG_END) != FTDM_SUCCESS) {
                                ftdm_log(FTDM_LOG_WARNING, "Error couldn't (re-)enable Q931-To-Pcap!\n");
				goto end;
                        } else {
				stream->write_function(stream, "+OK\n");
			}
		} else if(!strcasecmp(argv[2], "off")) {
			if (ftdm_configure_span(span, "isdn", on_clear_channel_signal, "q931topcap", 0, FTDM_TAG_END) != FTDM_SUCCESS) {
                                ftdm_log(FTDM_LOG_ERROR, "Error couldn't enable Q931-To-Pcap!\n");
                                goto end;
			} else {
                                stream->write_function(stream, "+OK\n");
                        }
                } else {
			stream->write_function(stream, "-ERR Usage: ft q931_pcap <span_id> on|off [pcapfilename without suffix]\n");
                        goto end;
		}

	} else if (!strcasecmp(argv[0], "dtmf")) {
		unsigned i = 0;
		uint32_t chan_id = 0;
		unsigned schan_count = 0;
		ftdm_span_t *span = NULL;
		ftdm_command_t fcmd = FTDM_COMMAND_ENABLE_DTMF_DETECT;
		ftdm_channel_t *fchan;
		if (argc < 3) {
			stream->write_function(stream, "-ERR Usage: dtmf on|off <span_id> [<chan_id>]\n");
			goto end;
		}

		if (switch_true(argv[1])) {
			fcmd = FTDM_COMMAND_ENABLE_DTMF_DETECT;
		} else {
			fcmd = FTDM_COMMAND_DISABLE_DTMF_DETECT;
		}

		ftdm_span_find_by_name(argv[2], &span);
		if (!span) {
			stream->write_function(stream, "-ERR invalid span\n");
			goto end;
		}
		schan_count = ftdm_span_get_chan_count(span);
		if (argc > 3) {
			chan_id = atoi(argv[3]);
			if (chan_id > schan_count) {
				stream->write_function(stream, "-ERR invalid chan\n");
				goto end;
			}
		}

		if (chan_id) {
			fchan = ftdm_span_get_channel(span, chan_id);
			ftdm_channel_command(fchan, fcmd, NULL);
		} else {
			for (i = 1; i <= schan_count; i++) {
				fchan = ftdm_span_get_channel(span, i);
				ftdm_channel_command(fchan, fcmd, NULL);
			}
		}

		stream->write_function(stream, "+OK DTMF detection was %s\n", fcmd == FTDM_COMMAND_ENABLE_DTMF_DETECT ? "enabled" : "disabled");
	} else if (!strcasecmp(argv[0], "trace")) {
		char tracepath[255];
		unsigned i = 0;
		uint32_t chan_id = 0;
		uint32_t span_id = 0;
		uint32_t chan_count = 0;
		ftdm_span_t *span = NULL;
		ftdm_channel_t *chan = NULL;
		if (argc < 3) {
			stream->write_function(stream, "-ERR Usage: ftdm trace <path> <span_id> [<chan_id>]\n");
			goto end;
		} 
		ftdm_span_find_by_name(argv[2], &span);
		if (!span) {
			stream->write_function(stream, "-ERR invalid span\n");
			goto end;
		}
		chan_count = ftdm_span_get_chan_count(span);
		if (argc > 3) {
			chan_id = atoi(argv[3]);
			if (chan_id > chan_count) {
				stream->write_function(stream, "-ERR invalid chan\n");
				goto end;
			}
		}
		span_id = ftdm_span_get_id(span);
		if (chan_id) {
			chan = ftdm_span_get_channel(span, chan_id);
			snprintf(tracepath, sizeof(tracepath), "%s-in-s%dc%d", argv[1], span_id, chan_id);
			ftdm_channel_command(chan, FTDM_COMMAND_TRACE_INPUT, tracepath);
			snprintf(tracepath, sizeof(tracepath), "%s-out-s%dc%d", argv[1], span_id, chan_id);
			ftdm_channel_command(chan, FTDM_COMMAND_TRACE_OUTPUT, tracepath);
		} else {
			for (i = 1; i <= chan_count; i++) {
				chan = ftdm_span_get_channel(span, i);
				snprintf(tracepath, sizeof(tracepath), "%s-in-s%dc%d", argv[1], span_id, i);
				ftdm_channel_command(chan, FTDM_COMMAND_TRACE_INPUT, tracepath);
				snprintf(tracepath, sizeof(tracepath), "%s-out-s%dc%d", argv[1], span_id, i);
				ftdm_channel_command(chan, FTDM_COMMAND_TRACE_OUTPUT, tracepath);
			}
		}
		stream->write_function(stream, "+OK trace enabled with prefix path %s\n", argv[1]);
	} else if (!strcasecmp(argv[0], "notrace")) {
		uint32_t i = 0;
		uint32_t chan_id = 0;
		uint32_t chan_count = 0;
		ftdm_channel_t *fchan = NULL;
		ftdm_span_t *span = NULL;
		if (argc < 2) {
			stream->write_function(stream, "-ERR Usage: ftdm notrace <span_id> [<chan_id>]\n");
			goto end;
		} 
		ftdm_span_find_by_name(argv[1], &span);
		if (!span) {
			stream->write_function(stream, "-ERR invalid span\n");
			goto end;
		}
		chan_count = ftdm_span_get_chan_count(span);
		if (argc > 2) {
			chan_id = atoi(argv[2]);
			if (chan_id > chan_count) {
				stream->write_function(stream, "-ERR invalid chan\n");
				goto end;
			}
		}
		if (chan_id) {
			fchan = ftdm_span_get_channel(span, chan_id);
			ftdm_channel_command(fchan, FTDM_COMMAND_TRACE_END_ALL, NULL);
		} else {
			for (i = 1; i <= chan_count; i++) {
				fchan = ftdm_span_get_channel(span, i);
				ftdm_channel_command(fchan, FTDM_COMMAND_TRACE_END_ALL, NULL);
			}
		}
		stream->write_function(stream, "+OK trace disabled\n");
	} else if (!strcasecmp(argv[0], "gains")) {
		unsigned int i = 0;
		float txgain = 0.0;
		float rxgain = 0.0;
		uint32_t chan_id = 0;
		uint32_t ccount = 0;
		ftdm_channel_t *chan;
		ftdm_span_t *span = NULL;
		if (argc < 4) {
			stream->write_function(stream, "-ERR Usage: ftdm gains <rxgain> <txgain> <span_id> [<chan_id>]\n");
			goto end;
		} 
		ftdm_span_find_by_name(argv[3], &span);
		if (!span) {
			stream->write_function(stream, "-ERR invalid span\n");
			goto end;
		}
		if (argc > 4) {
			chan_id = atoi(argv[4]);
			if (chan_id > ftdm_span_get_chan_count(span)) {
				stream->write_function(stream, "-ERR invalid chan\n");
				goto end;
			}
		}
		i = sscanf(argv[1], "%f", &rxgain);
		i += sscanf(argv[2], "%f", &txgain);
		if (i != 2) {
			stream->write_function(stream, "-ERR invalid gains\n");
			goto end;
		}

		if (chan_id) {
			chan = ftdm_span_get_channel(span, chan_id);
			ftdm_channel_command(chan, FTDM_COMMAND_SET_RX_GAIN, &rxgain);
			ftdm_channel_command(chan, FTDM_COMMAND_SET_TX_GAIN, &txgain);
		} else {
			ccount = ftdm_span_get_chan_count(span);
			for (i = 1; i < ccount; i++) {
				chan = ftdm_span_get_channel(span, i);
				ftdm_channel_command(chan, FTDM_COMMAND_SET_RX_GAIN, &rxgain);
				ftdm_channel_command(chan, FTDM_COMMAND_SET_TX_GAIN, &txgain);
			}
		}
		stream->write_function(stream, "+OK gains set to Rx %f and Tx %f\n", rxgain, txgain);
	} else if (!strcasecmp(argv[0], "queuesize")) {
		unsigned int i = 0;
		uint32_t rxsize = 10;
		uint32_t txsize = 10;
		uint32_t chan_id = 0;
		uint32_t ccount = 0;
		ftdm_channel_t *chan;
		ftdm_span_t *span = NULL;
		if (argc < 4) {
			stream->write_function(stream, "-ERR Usage: ftdm queuesize <rxsize> <txsize> <span_id> [<chan_id>]\n");
			goto end;
		} 
		ftdm_span_find_by_name(argv[3], &span);
		if (!span) {
			stream->write_function(stream, "-ERR invalid span\n");
			goto end;
		}
		if (argc > 4) {
			chan_id = atoi(argv[4]);
			if (chan_id > ftdm_span_get_chan_count(span)) {
				stream->write_function(stream, "-ERR invalid chan\n");
				goto end;
			}
		}
		i = sscanf(argv[1], "%u", &rxsize);
		i += sscanf(argv[2], "%u", &txsize);
		if (i != 2) {
			stream->write_function(stream, "-ERR invalid queue sizes provided\n");
			goto end;
		}

		if (chan_id) {
			chan = ftdm_span_get_channel(span, chan_id);
			ftdm_channel_command(chan, FTDM_COMMAND_SET_RX_QUEUE_SIZE, &rxsize);
			ftdm_channel_command(chan, FTDM_COMMAND_SET_TX_QUEUE_SIZE, &txsize);
		} else {
			ccount = ftdm_span_get_chan_count(span);
			for (i = 1; i < ccount; i++) {
				chan = ftdm_span_get_channel(span, i);
				ftdm_channel_command(chan, FTDM_COMMAND_SET_RX_QUEUE_SIZE, &rxsize);
				ftdm_channel_command(chan, FTDM_COMMAND_SET_TX_QUEUE_SIZE, &txsize);
			}
		}
		stream->write_function(stream, "+OK queue sizes set to Rx %d and Tx %d\n", rxsize, txsize);
	} else if (!strcasecmp(argv[0], "restart")) {
		uint32_t chan_id = 0;
		uint32_t ccount = 0;
		ftdm_channel_t *chan;
		ftdm_span_t *span = NULL;
		if (argc < 2) {
			stream->write_function(stream, "-ERR Usage: ftdm restart <span_id> [<chan_id>]\n");
			goto end;
		}
		ftdm_span_find_by_name(argv[1], &span);
		if (!span) {
			stream->write_function(stream, "-ERR invalid span\n");
			goto end;
		}

		if (argc > 2) {
			chan_id = atoi(argv[2]);
			if (chan_id > ftdm_span_get_chan_count(span)) {
				stream->write_function(stream, "-ERR invalid chan\n");
				goto end;
			}
		}
		if (chan_id) {
			chan = ftdm_span_get_channel(span, chan_id);
			if (!chan) {
				stream->write_function(stream, "-ERR Could not find chan\n");
				goto end;
			}
			stream->write_function(stream, "Resetting channel %s:%s\n", argv[1], argv[2]);
			ftdm_channel_reset(chan);
		} else {
			uint32_t i = 0;
			ccount = ftdm_span_get_chan_count(span);
			for (i = 1; i < ccount; i++) {
				chan = ftdm_span_get_channel(span, i);
				stream->write_function(stream, "Resetting channel %s:%d\n", argv[1], i);
				ftdm_channel_reset(chan);
			}
		}

	} else {

		char *rply = ftdm_api_execute(cmd);
		
		if (rply) {
			stream->write_function(stream, "%s", rply);
			ftdm_free(rply);
		} else {
			stream->write_function(stream, "-ERR Usage: %s\n", FT_SYNTAX);
		}
	}
	/*Q931ToPcap enhancement done*/

 end:

	switch_safe_free(mycmd);

	return SWITCH_STATUS_SUCCESS;
}

SWITCH_STANDARD_APP(enable_dtmf_function)
{
	private_t *tech_pvt;
	if (!switch_core_session_check_interface(session, freetdm_endpoint_interface)) {
		ftdm_log(FTDM_LOG_ERROR, "This application is only for FreeTDM channels.\n");
		return;
	}
	
	tech_pvt = switch_core_session_get_private(session);

	if (switch_test_flag(tech_pvt, TFLAG_DEAD)) {
        	switch_channel_hangup(switch_core_session_get_channel(session), SWITCH_CAUSE_LOSE_RACE);
		return;
	}
	
	ftdm_channel_command(tech_pvt->ftdmchan, FTDM_COMMAND_ENABLE_DTMF_DETECT, NULL);
	ftdm_log(FTDM_LOG_INFO, "DTMF detection enabled in channel %d:%d\n", ftdm_channel_get_id(tech_pvt->ftdmchan), ftdm_channel_get_span_id(tech_pvt->ftdmchan));
}

SWITCH_STANDARD_APP(disable_dtmf_function)
{
	private_t *tech_pvt;
	if (!switch_core_session_check_interface(session, freetdm_endpoint_interface)) {
		ftdm_log(FTDM_LOG_ERROR, "This application is only for FreeTDM channels.\n");
		return;
	}
	
	tech_pvt = switch_core_session_get_private(session);

	if (switch_test_flag(tech_pvt, TFLAG_DEAD)) {
        	switch_channel_hangup(switch_core_session_get_channel(session), SWITCH_CAUSE_LOSE_RACE);
		return;
	}
	
	ftdm_channel_command(tech_pvt->ftdmchan, FTDM_COMMAND_DISABLE_DTMF_DETECT, NULL);
	ftdm_log(FTDM_LOG_INFO, "DTMF detection Disabled in channel %d:%d\n", ftdm_channel_get_id(tech_pvt->ftdmchan), ftdm_channel_get_span_id(tech_pvt->ftdmchan));
}

SWITCH_STANDARD_APP(disable_ec_function)
{
	private_t *tech_pvt;
	int x = 0;

	if (!switch_core_session_check_interface(session, freetdm_endpoint_interface)) {
		ftdm_log(FTDM_LOG_ERROR, "This application is only for FreeTDM channels.\n");
		return;
	}
	
	tech_pvt = switch_core_session_get_private(session);

	if (switch_test_flag(tech_pvt, TFLAG_DEAD)) {
        switch_channel_hangup(switch_core_session_get_channel(session), SWITCH_CAUSE_LOSE_RACE);
        return;
    }
	
	ftdm_channel_command(tech_pvt->ftdmchan, FTDM_COMMAND_DISABLE_ECHOCANCEL, &x);
	ftdm_channel_command(tech_pvt->ftdmchan, FTDM_COMMAND_DISABLE_ECHOTRAIN, &x);
	ftdm_log(FTDM_LOG_INFO, "Echo Canceller Disabled\n");
}


SWITCH_MODULE_LOAD_FUNCTION(mod_freetdm_load)
{

	switch_api_interface_t *commands_api_interface;
	switch_application_interface_t *app_interface;

	module_pool = pool;

	ftdm_global_set_logger(ftdm_logger);

	ftdm_global_set_mod_directory(SWITCH_GLOBAL_dirs.mod_dir);

	ftdm_global_set_config_directory(SWITCH_GLOBAL_dirs.conf_dir);

	if (ftdm_global_init() != FTDM_SUCCESS) {
		ftdm_log(FTDM_LOG_ERROR, "Error loading FreeTDM\n");
		return SWITCH_STATUS_TERM;
	}

	if (ftdm_global_configuration() != FTDM_SUCCESS) {
		ftdm_log(FTDM_LOG_ERROR, "Error configuring FreeTDM\n");
		return SWITCH_STATUS_TERM;
	}
	
	if (load_config() != SWITCH_STATUS_SUCCESS) {
		ftdm_global_destroy();
		return SWITCH_STATUS_TERM;
	}

	*module_interface = switch_loadable_module_create_module_interface(pool, modname);
	freetdm_endpoint_interface = switch_loadable_module_create_interface(*module_interface, SWITCH_ENDPOINT_INTERFACE);
	freetdm_endpoint_interface->interface_name = "freetdm";
	freetdm_endpoint_interface->io_routines = &freetdm_io_routines;
	freetdm_endpoint_interface->state_handler = &freetdm_state_handlers;
	
	SWITCH_ADD_API(commands_api_interface, "ftdm", "FreeTDM commands", ft_function, FT_SYNTAX);
	switch_console_set_complete("add ftdm start");
	switch_console_set_complete("add ftdm stop");
	switch_console_set_complete("add ftdm restart");
	switch_console_set_complete("add ftdm dump");
	switch_console_set_complete("add ftdm sigstatus get");
	switch_console_set_complete("add ftdm sigstatus set");
	switch_console_set_complete("add ftdm trace");
	switch_console_set_complete("add ftdm notrace");
	switch_console_set_complete("add ftdm q931_pcap");
	switch_console_set_complete("add ftdm gains");
	switch_console_set_complete("add ftdm queuesize");
	switch_console_set_complete("add ftdm dtmf on");
	switch_console_set_complete("add ftdm dtmf off");
	switch_console_set_complete("add ftdm core state");
	switch_console_set_complete("add ftdm core flag");
	switch_console_set_complete("add ftdm core calls");

	SWITCH_ADD_APP(app_interface, "disable_ec", "Disable Echo Canceller", "Disable Echo Canceller", disable_ec_function, "", SAF_NONE);
	SWITCH_ADD_APP(app_interface, "disable_dtmf", "Disable DTMF Detection", "Disable DTMF Detection", disable_dtmf_function, "", SAF_NONE);
	SWITCH_ADD_APP(app_interface, "enable_dtmf", "Enable DTMF Detection", "Enable DTMF Detection", enable_dtmf_function, "", SAF_NONE);

	/* indicate that the module should continue to be loaded */
	return SWITCH_STATUS_SUCCESS;
}

SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_freetdm_shutdown)
{
	switch_hash_index_t *hi;		

	const void *var;
	void *val;

	/* destroy ss7 configs */
	for (hi = switch_hash_first(NULL, globals.ss7_configs); hi; hi = switch_hash_next(hi)) {
		switch_hash_this(hi, &var, NULL, &val);	
		ftdm_conf_node_destroy(val);
	}

	ftdm_global_destroy();

	// this breaks pika but they are MIA so *shrug*
	//return SWITCH_STATUS_NOUNLOAD;
	return SWITCH_STATUS_SUCCESS;
}

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
