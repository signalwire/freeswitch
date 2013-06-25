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
 * Moises Silva <moy@sangoma.com>
 *
 *
 * mod_openzap.c -- OPENZAP Endpoint Module
 *
 */
#include <switch.h>
#include "openzap.h"

#ifndef __FUNCTION__
#define __FUNCTION__ __SWITCH_FUNC__
#endif

#define OPENZAP_VAR_PREFIX "openzap_"
#define OPENZAP_VAR_PREFIX_LEN 8

SWITCH_MODULE_LOAD_FUNCTION(mod_openzap_load);
SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_openzap_shutdown);
SWITCH_MODULE_DEFINITION(mod_openzap, mod_openzap_load, mod_openzap_shutdown, NULL);

switch_endpoint_interface_t *openzap_endpoint_interface;

static switch_memory_pool_t *module_pool = NULL;

typedef enum {
	ANALOG_OPTION_NONE = 0,
	ANALOG_OPTION_3WAY = (1 << 0),
	ANALOG_OPTION_CALL_SWAP = (1 << 1)
} analog_option_t;

struct span_config {
	zap_span_t *span;
	char dialplan[80];
	char context[80];
	char dial_regex[256];
	char fail_dial_regex[256];
	char hold_music[256];
	char type[256];	
	analog_option_t analog_options;
};

static struct span_config SPAN_CONFIG[ZAP_MAX_SPANS_INTERFACE] = {{0}};

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
} globals;

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
	zap_channel_t *zchan;
	uint32_t wr_error;
};

typedef struct private_object private_t;


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
zap_status_t zap_channel_from_event(zap_sigmsg_t *sigmsg, switch_core_session_t **sp);
void dump_chan(zap_span_t *span, uint32_t chan_id, switch_stream_handle_t *stream);
void dump_chan_xml(zap_span_t *span, uint32_t chan_id, switch_stream_handle_t *stream);

static void zap_set_npi(const char *npi_string, uint8_t *target)
{
	if (switch_is_number(npi_string)) {
		*target = (uint8_t)atoi(npi_string);
	} else if (!strcasecmp(npi_string, "isdn") || !strcasecmp(npi_string, "e164")) {
		*target = ZAP_NPI_ISDN;
	} else if (!strcasecmp(npi_string, "data")) {
		*target = ZAP_NPI_DATA;
	} else if (!strcasecmp(npi_string, "telex")) {
		*target = ZAP_NPI_TELEX;
	} else if (!strcasecmp(npi_string, "national")) {
		*target = ZAP_NPI_NATIONAL;
	} else if (!strcasecmp(npi_string, "private")) {
		*target = ZAP_NPI_PRIVATE;
	} else if (!strcasecmp(npi_string, "reserved")) {
		*target = ZAP_NPI_RESERVED;
	} else if (!strcasecmp(npi_string, "unknown")) {
		*target = ZAP_NPI_UNKNOWN;
	} else {
		zap_log(ZAP_LOG_WARNING, "Invalid NPI value (%s)\n", npi_string);
		*target = ZAP_NPI_UNKNOWN;
	}
}

static void zap_set_ton(const char *ton_string, uint8_t *target)
{
	if (switch_is_number(ton_string)) {
		*target = (uint8_t)atoi(ton_string);
	} else if (!strcasecmp(ton_string, "national")) {
		*target = ZAP_TON_NATIONAL;
	} else if (!strcasecmp(ton_string, "international")) {
		*target = ZAP_TON_INTERNATIONAL;
	} else if (!strcasecmp(ton_string, "local")) {
		*target = ZAP_TON_SUBSCRIBER_NUMBER;
	} else if (!strcasecmp(ton_string, "unknown")) {
		*target = ZAP_TON_UNKNOWN;
	} else {
		zap_log(ZAP_LOG_WARNING, "Invalid TON value (%s)\n", ton_string);
		*target = ZAP_TON_UNKNOWN;
	}
}

static switch_core_session_t *zap_channel_get_session(zap_channel_t *channel, int32_t id)
{
	switch_core_session_t *session = NULL;

	if (id > ZAP_MAX_TOKENS) {
		return NULL;
	}

	if (!zstr(channel->tokens[id])) {
		if (!(session = switch_core_session_locate(channel->tokens[id]))) {
			zap_channel_clear_token(channel, channel->tokens[id]);
		}
	}

	return session;
}

static const char *zap_channel_get_uuid(zap_channel_t *channel, int32_t id)
{
	if (id > ZAP_MAX_TOKENS) {
		return NULL;
	}

	if (!zstr(channel->tokens[id])) {
		return channel->tokens[id];
	}
	return NULL;
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

static void start_hold(zap_channel_t *zchan, switch_core_session_t *session_a, const char *uuid, const char *stream)
{
	switch_core_session_t *session;
	switch_channel_t *channel, *channel_a;

	if (!uuid) {
		return;
	}
	
	
	if ((session = switch_core_session_locate(uuid))) {
		channel = switch_core_session_get_channel(session);
		if (zstr(stream)) {
			if (!strcasecmp(globals.hold_music, "indicate_hold")) {
				stream = "indicate_hold";
			}
			if (!strcasecmp(SPAN_CONFIG[zchan->span->span_id].hold_music, "indicate_hold")) {
				stream = "indicate_hold";
			}
		}

		if (zstr(stream)) {
			stream = switch_channel_get_variable(channel, SWITCH_HOLD_MUSIC_VARIABLE);
		}

		if (zstr(stream)) {
			stream = SPAN_CONFIG[zchan->span->span_id].hold_music;
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


static void cycle_foreground(zap_channel_t *zchan, int flash, const char *bcast) {
	uint32_t i = 0;
	switch_core_session_t *session;
	switch_channel_t *channel;
	private_t *tech_pvt;
	

	for (i = 0; i < zchan->token_count; i++) {
		if ((session = zap_channel_get_session(zchan, i))) {
			const char *buuid;
			tech_pvt = switch_core_session_get_private(session);
			channel = switch_core_session_get_channel(session);
			buuid = switch_channel_get_partner_uuid(channel);

			
			if (zchan->token_count == 1 && flash) {
				if (switch_test_flag(tech_pvt, TFLAG_HOLD)) {
					stop_hold(session, buuid);
					switch_clear_flag_locked(tech_pvt, TFLAG_HOLD);
				} else {
					start_hold(zchan, session, buuid, bcast);
					switch_set_flag_locked(tech_pvt, TFLAG_HOLD);
				}
			} else if (i) {
				start_hold(zchan, session, buuid, bcast);
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




static switch_status_t tech_init(private_t *tech_pvt, switch_core_session_t *session, zap_channel_t *zchan)
{
	const char *dname = NULL;
	uint32_t interval = 0, srate = 8000;
	zap_codec_t codec;

	tech_pvt->zchan = zchan;
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

	if (ZAP_SUCCESS != zap_channel_command(zchan, ZAP_COMMAND_GET_INTERVAL, &interval)) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Failed to retrieve channel interval.\n");
		return SWITCH_STATUS_GENERR;
	}

	if (ZAP_SUCCESS != zap_channel_command(zchan, ZAP_COMMAND_GET_CODEC, &codec)) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Failed to retrieve channel codec.\n");
		return SWITCH_STATUS_GENERR;
	}

	switch(codec) {
	case ZAP_CODEC_ULAW:
		{
			dname = "PCMU";
		}
		break;
	case ZAP_CODEC_ALAW:
		{
			dname = "PCMA";
		}
		break;
	case ZAP_CODEC_SLIN:
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

	zap_channel_init(tech_pvt->zchan);

	//switch_channel_set_flag(channel, CF_ACCEPT_CNG);

	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t channel_on_routing(switch_core_session_t *session)
{
	switch_channel_t *channel = NULL;
	private_t *tech_pvt = NULL;

	channel = switch_core_session_get_channel(session);
	assert(channel != NULL);

	tech_pvt = switch_core_session_get_private(session);
	assert(tech_pvt != NULL);

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "%s CHANNEL ROUTING\n", switch_channel_get_name(channel));

	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t channel_on_execute(switch_core_session_t *session)
{

	switch_channel_t *channel = NULL;
	private_t *tech_pvt = NULL;

	channel = switch_core_session_get_channel(session);
	assert(channel != NULL);

	tech_pvt = switch_core_session_get_private(session);
	assert(tech_pvt != NULL);

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

	channel = switch_core_session_get_channel(session);
	assert(channel != NULL);

	tech_pvt = switch_core_session_get_private(session);
	assert(tech_pvt != NULL);

	if (!tech_pvt->zchan) {
		goto end;
	} 

	zap_channel_clear_token(tech_pvt->zchan, switch_core_session_get_uuid(session));
	
	switch (tech_pvt->zchan->type) {
	case ZAP_CHAN_TYPE_FXO:
	case ZAP_CHAN_TYPE_EM:
	case ZAP_CHAN_TYPE_CAS:
		{
			zap_set_state_locked(tech_pvt->zchan, ZAP_CHANNEL_STATE_HANGUP);
		}
		break;
	case ZAP_CHAN_TYPE_FXS:
		{
			if (tech_pvt->zchan->state != ZAP_CHANNEL_STATE_BUSY && tech_pvt->zchan->state != ZAP_CHANNEL_STATE_DOWN) {
				if (tech_pvt->zchan->token_count) {
					cycle_foreground(tech_pvt->zchan, 0, NULL);
				} else {
					zap_set_state_locked(tech_pvt->zchan, ZAP_CHANNEL_STATE_HANGUP);
				}
			}
		}
		break;
	case ZAP_CHAN_TYPE_B:
		{
			if (tech_pvt->zchan->state != ZAP_CHANNEL_STATE_DOWN) {
				if (tech_pvt->zchan->state != ZAP_CHANNEL_STATE_TERMINATING) {
					tech_pvt->zchan->caller_data.hangup_cause = switch_channel_get_cause_q850(channel);
					if (tech_pvt->zchan->caller_data.hangup_cause < 1 || tech_pvt->zchan->caller_data.hangup_cause > 127) {
						tech_pvt->zchan->caller_data.hangup_cause = ZAP_CAUSE_DESTINATION_OUT_OF_ORDER;
					}
				}
				zap_set_state_locked(tech_pvt->zchan, ZAP_CHANNEL_STATE_HANGUP);
			}
		}
		break;
	default: 
		{
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Unhandled channel type %d for channel %s\n", tech_pvt->zchan->type,
                    switch_channel_get_name(channel));
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
	zap_channel_command(tech_pvt->zchan, ZAP_COMMAND_SEND_DTMF, tmp);
		
	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t channel_read_frame(switch_core_session_t *session, switch_frame_t **frame, switch_io_flag_t flags, int stream_id)
{
	switch_channel_t *channel = NULL;
	private_t *tech_pvt = NULL;
	zap_size_t len;
	zap_wait_flag_t wflags = ZAP_READ;
	char dtmf[128] = "";
	zap_status_t status;
	int total_to;
	int chunk, do_break = 0;


	channel = switch_core_session_get_channel(session);
	assert(channel != NULL);
	
	
	tech_pvt = switch_core_session_get_private(session);
	assert(tech_pvt != NULL);

	if (switch_test_flag(tech_pvt, TFLAG_DEAD)) {
		return SWITCH_STATUS_FALSE;
	} 

	/* Digium Cards sometimes timeout several times in a row here. 
	   Yes, we support digium cards, ain't we nice.......
	   6 double length intervals should compensate */
	chunk = tech_pvt->zchan->effective_interval * 2;
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
		switch_yield(tech_pvt->zchan->effective_interval * 1000);
		tech_pvt->cng_frame.datalen = tech_pvt->zchan->packet_len;
		tech_pvt->cng_frame.samples = tech_pvt->cng_frame.datalen;
		tech_pvt->cng_frame.flags = SFF_CNG;
		*frame = &tech_pvt->cng_frame;
		if (tech_pvt->zchan->effective_codec == ZAP_CODEC_SLIN) {
			tech_pvt->cng_frame.samples /= 2;
		}
		return SWITCH_STATUS_SUCCESS;
	}
	
	if (!switch_test_flag(tech_pvt, TFLAG_IO)) {
		goto fail;
	}

	wflags = ZAP_READ;	
	status = zap_channel_wait(tech_pvt->zchan, &wflags, chunk);
	
	if (status == ZAP_FAIL) {
		goto fail;
	}
	
	if (status == ZAP_TIMEOUT) {
		if (!switch_test_flag(tech_pvt, TFLAG_HOLD)) {
			total_to -= chunk;
			if (total_to <= 0) {
				goto fail;
			}
		}

		goto top;
	}

	if (!(wflags & ZAP_READ)) {
		goto fail;
	}

	len = tech_pvt->read_frame.buflen;
	if (zap_channel_read(tech_pvt->zchan, tech_pvt->read_frame.data, &len) != ZAP_SUCCESS) {
		goto fail;
	}

	*frame = &tech_pvt->read_frame;
	tech_pvt->read_frame.datalen = (uint32_t)len;
	tech_pvt->read_frame.samples = tech_pvt->read_frame.datalen;

	if (tech_pvt->zchan->effective_codec == ZAP_CODEC_SLIN) {
		tech_pvt->read_frame.samples /= 2;
	}

	while (zap_channel_dequeue_dtmf(tech_pvt->zchan, dtmf, sizeof(dtmf))) {
		switch_dtmf_t _dtmf = { 0, SWITCH_DEFAULT_DTMF_DURATION };
		char *p;
		for (p = dtmf; p && *p; p++) {
			if (is_dtmf(*p)) {
				_dtmf.digit = *p;
				zap_log(ZAP_LOG_DEBUG, "queue DTMF [%c]\n", *p);
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
	zap_size_t len;
	unsigned char data[SWITCH_RECOMMENDED_BUFFER_SIZE] = {0};
	zap_wait_flag_t wflags = ZAP_WRITE;
	zap_status_t status;

	channel = switch_core_session_get_channel(session);
	assert(channel != NULL);

	tech_pvt = switch_core_session_get_private(session);
	assert(tech_pvt != NULL);

	if (!tech_pvt->zchan) {
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


	wflags = ZAP_WRITE;	
	status = zap_channel_wait(tech_pvt->zchan, &wflags, tech_pvt->zchan->effective_interval * 10);
	
	if (!(wflags & ZAP_WRITE)) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Dropping frame! (write not ready)\n");
		return SWITCH_STATUS_SUCCESS;
	}

	len = frame->datalen;
	if (zap_channel_write(tech_pvt->zchan, frame->data, frame->buflen, &len) != ZAP_SUCCESS) {
		if (++tech_pvt->wr_error > 10) {
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

	channel = switch_core_session_get_channel(session);
	assert(channel != NULL);
			
	tech_pvt = (private_t *) switch_core_session_get_private(session);
	assert(tech_pvt != NULL);
	
	if (switch_test_flag(tech_pvt, TFLAG_DEAD)) {
        switch_channel_hangup(channel, SWITCH_CAUSE_LOSE_RACE);
        return SWITCH_STATUS_FALSE;
    }
	
	zap_log(ZAP_LOG_DEBUG, "Got Freeswitch message in R2 channel %d [%d]\n", tech_pvt->zchan->physical_chan_id, 
            msg->message_id);

	switch (msg->message_id) {
	case SWITCH_MESSAGE_INDICATE_RINGING:
		{
			if (switch_channel_direction(channel) == SWITCH_CALL_DIRECTION_OUTBOUND) {
				zap_set_flag_locked(tech_pvt->zchan, ZAP_CHANNEL_PROGRESS);
			} else {
				zap_set_state_locked_wait(tech_pvt->zchan, ZAP_CHANNEL_STATE_PROGRESS);
			}
		}
		break;
	case SWITCH_MESSAGE_INDICATE_PROGRESS:
		{
			if (switch_channel_direction(channel) == SWITCH_CALL_DIRECTION_OUTBOUND) {
				zap_set_flag_locked(tech_pvt->zchan, ZAP_CHANNEL_PROGRESS);
				zap_set_flag_locked(tech_pvt->zchan, ZAP_CHANNEL_MEDIA);
			} else {
				zap_set_state_locked_wait(tech_pvt->zchan, ZAP_CHANNEL_STATE_PROGRESS);
				zap_set_state_locked_wait(tech_pvt->zchan, ZAP_CHANNEL_STATE_PROGRESS_MEDIA);
			}
		}
		break;
	case SWITCH_MESSAGE_INDICATE_ANSWER:
		{
			if (switch_channel_direction(channel) == SWITCH_CALL_DIRECTION_OUTBOUND) {
				zap_set_flag_locked(tech_pvt->zchan, ZAP_CHANNEL_ANSWERED);
			} else {
				/* lets make the ozmod_r2 module life easier by moving thru each
                 * state waiting for completion, clumsy, but does the job
				 */
				if (tech_pvt->zchan->state < ZAP_CHANNEL_STATE_PROGRESS) {
					zap_set_state_locked_wait(tech_pvt->zchan, ZAP_CHANNEL_STATE_PROGRESS);
				}
				if (tech_pvt->zchan->state < ZAP_CHANNEL_STATE_PROGRESS_MEDIA) {
					zap_set_state_locked_wait(tech_pvt->zchan, ZAP_CHANNEL_STATE_PROGRESS_MEDIA);
				}
				zap_set_state_locked_wait(tech_pvt->zchan, ZAP_CHANNEL_STATE_UP);
			}
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

	if (tech_pvt->zchan->state == ZAP_CHANNEL_STATE_TERMINATING) {
		zap_mutex_unlock(tech_pvt->zchan->mutex);	
		return SWITCH_STATUS_SUCCESS;
	}

	switch (msg->message_id) {
	case SWITCH_MESSAGE_INDICATE_RINGING:
		{
			if (switch_channel_direction(channel) == SWITCH_CALL_DIRECTION_OUTBOUND) {
				zap_set_flag(tech_pvt->zchan, ZAP_CHANNEL_PROGRESS);
			} else {
				zap_set_state_wait(tech_pvt->zchan, ZAP_CHANNEL_STATE_PROGRESS);
			}
		}
		break;
	case SWITCH_MESSAGE_INDICATE_PROGRESS:
		{
			if (switch_channel_direction(channel) == SWITCH_CALL_DIRECTION_OUTBOUND) {
				zap_set_flag(tech_pvt->zchan, ZAP_CHANNEL_PROGRESS);
				zap_set_flag(tech_pvt->zchan, ZAP_CHANNEL_MEDIA);
			} else {
				/* Don't skip messages in the ISDN call setup
				 * TODO: make the isdn stack smart enough to handle that itself
				 *       until then, this is here for safety...
				 */
				if (tech_pvt->zchan->state < ZAP_CHANNEL_STATE_PROGRESS) {
					zap_set_state_wait(tech_pvt->zchan, ZAP_CHANNEL_STATE_PROGRESS);
				}
				zap_set_state_wait(tech_pvt->zchan, ZAP_CHANNEL_STATE_PROGRESS_MEDIA);
			}
		}
		break;
	case SWITCH_MESSAGE_INDICATE_ANSWER:
		{
			if (switch_channel_direction(channel) == SWITCH_CALL_DIRECTION_OUTBOUND) {
				zap_set_flag(tech_pvt->zchan, ZAP_CHANNEL_ANSWERED);
			} else {
				/* Don't skip messages in the ISDN call setup
				 * TODO: make the isdn stack smart enough to handle that itself
				 *       until then, this is here for safety...
				 */
				if (tech_pvt->zchan->state < ZAP_CHANNEL_STATE_PROGRESS) {
					zap_set_state_wait(tech_pvt->zchan, ZAP_CHANNEL_STATE_PROGRESS);
				}
				if (tech_pvt->zchan->state < ZAP_CHANNEL_STATE_PROGRESS_MEDIA) {
					zap_set_state_wait(tech_pvt->zchan, ZAP_CHANNEL_STATE_PROGRESS_MEDIA);
				}
				zap_set_state_wait(tech_pvt->zchan, ZAP_CHANNEL_STATE_UP);
			}
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
	
	switch (msg->message_id) {
	case SWITCH_MESSAGE_INDICATE_PROGRESS:
	case SWITCH_MESSAGE_INDICATE_ANSWER:
		if (switch_channel_direction(channel) == SWITCH_CALL_DIRECTION_OUTBOUND) {
			zap_set_flag_locked(tech_pvt->zchan, ZAP_CHANNEL_ANSWERED);
			zap_set_flag_locked(tech_pvt->zchan, ZAP_CHANNEL_PROGRESS);
			zap_set_flag_locked(tech_pvt->zchan, ZAP_CHANNEL_MEDIA);
		} else {
			zap_set_state_locked(tech_pvt->zchan, ZAP_CHANNEL_STATE_UP);
		}
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
	
	switch (msg->message_id) {
	case SWITCH_MESSAGE_INDICATE_PROGRESS:
	case SWITCH_MESSAGE_INDICATE_ANSWER:
		if (switch_channel_direction(channel) == SWITCH_CALL_DIRECTION_INBOUND) {
			zap_set_flag_locked(tech_pvt->zchan, ZAP_CHANNEL_ANSWERED);
			zap_set_flag_locked(tech_pvt->zchan, ZAP_CHANNEL_PROGRESS);
			zap_set_flag_locked(tech_pvt->zchan, ZAP_CHANNEL_MEDIA);
			zap_set_state_locked(tech_pvt->zchan, ZAP_CHANNEL_STATE_UP);
			switch_channel_mark_answered(channel);
		}
		break;
	case SWITCH_MESSAGE_INDICATE_RINGING:
		if (switch_channel_direction(channel) == SWITCH_CALL_DIRECTION_INBOUND) {
			
			if (!switch_channel_test_flag(channel, CF_ANSWERED) && 
				!switch_channel_test_flag(channel, CF_EARLY_MEDIA) &&
				!switch_channel_test_flag(channel, CF_RING_READY)
				) {
				zap_set_state_locked(tech_pvt->zchan, ZAP_CHANNEL_STATE_RING);
				switch_channel_mark_ring_ready(channel);
			}
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
	zap_channel_t *zchan = NULL;

	tech_pvt = (private_t *) switch_core_session_get_private(session);
	assert(tech_pvt != NULL);

	channel = switch_core_session_get_channel(session);

	if (switch_test_flag(tech_pvt, TFLAG_DEAD)) {
        switch_channel_hangup(channel, SWITCH_CAUSE_LOSE_RACE);
        return SWITCH_STATUS_FALSE;
	}

	if (!(zchan = tech_pvt->zchan)) {
        switch_channel_hangup(channel, SWITCH_CAUSE_LOSE_RACE);
        return SWITCH_STATUS_FALSE;
    }

	zap_mutex_lock(zchan->mutex);	

	if (!tech_pvt->zchan) {
		switch_channel_hangup(channel, SWITCH_CAUSE_LOSE_RACE);
		status = SWITCH_STATUS_FALSE;
		goto end;
	}

	switch (msg->message_id) {
	case SWITCH_MESSAGE_INDICATE_PROGRESS:
	case SWITCH_MESSAGE_INDICATE_ANSWER:
		if (switch_channel_direction(channel) == SWITCH_CALL_DIRECTION_INBOUND) {
			if ((var = switch_channel_get_variable(channel, "openzap_pre_buffer_size"))) {
				int tmp = atoi(var);
				if (tmp > -1) {
					zap_channel_command(tech_pvt->zchan, ZAP_COMMAND_SET_PRE_BUFFER_SIZE, &tmp);
				}
			}
		}
		break;
	case SWITCH_MESSAGE_INDICATE_UUID_CHANGE:
		{
			zap_channel_replace_token(tech_pvt->zchan, msg->string_array_arg[0], msg->string_array_arg[1]);
		}
		break;
	default:
		break;
	}

	switch (tech_pvt->zchan->type) {
	case ZAP_CHAN_TYPE_FXS:
	case ZAP_CHAN_TYPE_EM:
		status = channel_receive_message_fxs(session, msg);
		break;
	case ZAP_CHAN_TYPE_FXO:
		status = channel_receive_message_fxo(session, msg);
		break;
	case ZAP_CHAN_TYPE_B:
		status = channel_receive_message_b(session, msg);
        break;
	case ZAP_CHAN_TYPE_CAS:
		status = channel_receive_message_cas(session, msg);
        break;
	default:
		status = SWITCH_STATUS_FALSE;
		break;
	}

 end:

	zap_mutex_unlock(zchan->mutex);	

	return status;

}

switch_state_handler_table_t openzap_state_handlers = {
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

switch_io_routines_t openzap_io_routines = {
	/*.outgoing_channel */ channel_outgoing_channel,
	/*.read_frame */ channel_read_frame,
	/*.write_frame */ channel_write_frame,
	/*.kill_channel */ channel_kill_channel,
	/*.send_dtmf */ channel_send_dtmf,
	/*.receive_message*/ channel_receive_message
};

/* Make sure when you have 2 sessions in the same scope that you pass the appropriate one to the routines
that allocate memory or you will have 1 channel with memory allocated from another channel's pool!
*/
static switch_call_cause_t channel_outgoing_channel(switch_core_session_t *session, switch_event_t *var_event,
													switch_caller_profile_t *outbound_profile,
													switch_core_session_t **new_session, switch_memory_pool_t **pool,
													switch_originate_flag_t flags, switch_call_cause_t *cancel_cause)
{

	const char *dest = NULL;
	char *data = NULL;
	int span_id = -1, chan_id = 0;
	zap_channel_t *zchan = NULL;
	switch_call_cause_t cause = SWITCH_CAUSE_DESTINATION_OUT_OF_ORDER;
	char name[128];
	zap_status_t status;
	int direction = ZAP_TOP_DOWN;
	zap_caller_data_t caller_data = {{ 0 }};
	char *span_name = NULL;
	switch_event_header_t *h;
	char *argv[3];
	int argc = 0;
	const char *var;
	const char *dest_num = NULL, *callerid_num = NULL;

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
		direction = ZAP_BOTTOM_UP;
	} else if (*argv[1] == 'a') {
		direction =  ZAP_TOP_DOWN;
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
		zap_span_t *span;
		zap_status_t zstatus = zap_span_find_by_name(span_name, &span);
		if (zstatus == ZAP_SUCCESS && span) {
			span_id = span->span_id;
		}
	}

	if (span_id == -1) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Missing span\n");
		return SWITCH_CAUSE_DESTINATION_OUT_OF_ORDER;
	}

	if (chan_id < 0) {
		direction = ZAP_BOTTOM_UP;
		chan_id = 0;
	}
	
	if (switch_test_flag(outbound_profile, SWITCH_CPF_SCREEN)) {
		caller_data.screen = 1;
	}

	if (switch_test_flag(outbound_profile, SWITCH_CPF_HIDE_NUMBER)) {
		caller_data.pres = 1;
	}

	if (!zstr(dest)) {
		zap_set_string(caller_data.ani.digits, dest);
	}
	
	if ((var = switch_event_get_header(var_event, "openzap_outbound_ton"))) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "Setting TON to: %s\n", var);
		zap_set_ton(var, &caller_data.ani.type);
	} else {
		caller_data.ani.type = outbound_profile->destination_number_ton;
	}

	if ((var = switch_event_get_header(var_event, "openzap_custom_call_data"))) {
		zap_set_string((char *)caller_data.raw_data, var);
		caller_data.raw_data_len = strlen(var);
	}
	
	if ((var = switch_event_get_header(var_event, "openzap_outbound_npi"))) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "Setting NPI to: %s\n", var);
		zap_set_npi(var, &caller_data.ani.plan);
	} else {
		caller_data.ani.plan = outbound_profile->destination_number_numplan;
	}
	
	/* blindly copy data from outbound_profile. They will be overwritten 
	 * by calling zap_caller_data if needed after */
	caller_data.cid_num.type = outbound_profile->caller_ton;
	caller_data.cid_num.plan = outbound_profile->caller_numplan;

	caller_data.rdnis.type = outbound_profile->rdnis_ton;
	caller_data.rdnis.plan = outbound_profile->rdnis_numplan;
	
#if 0
	if (!zstr(outbound_profile->rdnis)) {
		zap_set_string(caller_data.rdnis.digits, outbound_profile->rdnis);
	}
#endif

	zap_set_string(caller_data.cid_name, outbound_profile->caller_id_name);
	zap_set_string(caller_data.cid_num.digits, switch_str_nil(callerid_num));
	
	if (chan_id) {
		status = zap_channel_open(span_id, chan_id, &zchan);
	} else {
		status = zap_channel_open_any(span_id, direction, &caller_data, &zchan);
	}
	
	if (status != ZAP_SUCCESS) {
		if (caller_data.hangup_cause == SWITCH_CAUSE_NONE) {
			caller_data.hangup_cause = SWITCH_CAUSE_NORMAL_CIRCUIT_CONGESTION;
		}
		return caller_data.hangup_cause;
	}

	if ((var = switch_event_get_header(var_event, "openzap_pre_buffer_size"))) {
		int tmp = atoi(var);
		if (tmp > -1) {
			zap_channel_command(zchan, ZAP_COMMAND_SET_PRE_BUFFER_SIZE, &tmp);
		}
	}

	zap_channel_clear_vars(zchan);
	for (h = var_event->headers; h; h = h->next) {
		if (!strncasecmp(h->name, OPENZAP_VAR_PREFIX, OPENZAP_VAR_PREFIX_LEN)) {
			char *v = h->name + OPENZAP_VAR_PREFIX_LEN;
			if (!zstr(v)) {
				zap_channel_add_var(zchan, v, h->value);
			}
		}
	}
	
	if ((*new_session = switch_core_session_request(openzap_endpoint_interface, SWITCH_CALL_DIRECTION_OUTBOUND, flags, pool)) != 0) {
		private_t *tech_pvt;
		switch_caller_profile_t *caller_profile;
		switch_channel_t *channel = switch_core_session_get_channel(*new_session);
		
		switch_core_session_add_stream(*new_session, NULL);
		if ((tech_pvt = (private_t *) switch_core_session_alloc(*new_session, sizeof(private_t))) != 0) {
			tech_init(tech_pvt, *new_session, zchan);
		} else {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Hey where is my memory pool?\n");
			switch_core_session_destroy(new_session);
			cause = SWITCH_CAUSE_DESTINATION_OUT_OF_ORDER;
			goto fail;
		}

		snprintf(name, sizeof(name), "OpenZAP/%u:%u/%s", zchan->span_id, zchan->chan_id, dest);
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Connect outbound channel %s\n", name);
		switch_channel_set_name(channel, name);
		switch_channel_set_variable(channel, "openzap_span_name", zchan->span->name);
		switch_channel_set_variable_printf(channel, "openzap_span_number", "%d", zchan->span_id);	
		switch_channel_set_variable_printf(channel, "openzap_chan_number", "%d", zchan->chan_id);
		zap_channel_set_caller_data(zchan, &caller_data);
		caller_profile = switch_caller_profile_clone(*new_session, outbound_profile);
		caller_profile->destination_number = switch_core_strdup(caller_profile->pool, switch_str_nil(dest_num));
		caller_profile->caller_id_number = switch_core_strdup(caller_profile->pool, switch_str_nil(callerid_num));
		switch_channel_set_caller_profile(channel, caller_profile);
		tech_pvt->caller_profile = caller_profile;
		
		
		switch_channel_set_state(channel, CS_INIT);
		if (zap_channel_add_token(zchan, switch_core_session_get_uuid(*new_session), zchan->token_count) != ZAP_SUCCESS) {
			switch_core_session_destroy(new_session);
			cause = SWITCH_CAUSE_DESTINATION_OUT_OF_ORDER;
            goto fail;
		}


		if (zap_channel_outgoing_call(zchan) != ZAP_SUCCESS) {
			if (tech_pvt->read_codec.implementation) {
				switch_core_codec_destroy(&tech_pvt->read_codec);
			}
			
			if (tech_pvt->write_codec.implementation) {
				switch_core_codec_destroy(&tech_pvt->write_codec);
			}
			switch_core_session_destroy(new_session);
            cause = SWITCH_CAUSE_DESTINATION_OUT_OF_ORDER;
            goto fail;
		}

		zap_channel_init(zchan);
		
		return SWITCH_CAUSE_SUCCESS;
	}

 fail:

	if (zchan) {
		zap_channel_done(zchan);
	}

	return cause;

}

zap_status_t zap_channel_from_event(zap_sigmsg_t *sigmsg, switch_core_session_t **sp)
{
	switch_core_session_t *session = NULL;
	private_t *tech_pvt = NULL;
	switch_channel_t *channel = NULL;
	char name[128];
	
	*sp = NULL;
	
	if (!(session = switch_core_session_request(openzap_endpoint_interface, SWITCH_CALL_DIRECTION_INBOUND, SOF_NONE, NULL))) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Initilization Error!\n");
		return ZAP_FAIL;
	}
	
	switch_core_session_add_stream(session, NULL);
	
	tech_pvt = (private_t *) switch_core_session_alloc(session, sizeof(private_t));
	assert(tech_pvt != NULL);
	channel = switch_core_session_get_channel(session);
	if (tech_init(tech_pvt, session, sigmsg->channel) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Initilization Error!\n");
		switch_core_session_destroy(&session);
		return ZAP_FAIL;
	}
	
	*sigmsg->channel->caller_data.collected = '\0';
	
	if (zstr(sigmsg->channel->caller_data.cid_name)) {
		switch_set_string(sigmsg->channel->caller_data.cid_name, sigmsg->channel->chan_name);
	}

	if (zstr(sigmsg->channel->caller_data.cid_num.digits)) {
		if (!zstr(sigmsg->channel->caller_data.ani.digits)) {
			switch_set_string(sigmsg->channel->caller_data.cid_num.digits, sigmsg->channel->caller_data.ani.digits);
		} else {
			switch_set_string(sigmsg->channel->caller_data.cid_num.digits, sigmsg->channel->chan_number);
		}
	}

	tech_pvt->caller_profile = switch_caller_profile_new(switch_core_session_get_pool(session),
														 "OpenZAP",
														 SPAN_CONFIG[sigmsg->channel->span_id].dialplan,
														 sigmsg->channel->caller_data.cid_name,
														 sigmsg->channel->caller_data.cid_num.digits,
														 NULL,
														 sigmsg->channel->caller_data.ani.digits,
														 sigmsg->channel->caller_data.aniII,
														 sigmsg->channel->caller_data.rdnis.digits,
														 (char *) modname,
														 SPAN_CONFIG[sigmsg->channel->span_id].context,
														 sigmsg->channel->caller_data.dnis.digits);

	assert(tech_pvt->caller_profile != NULL);

	if (sigmsg->channel->caller_data.screen == 1 || sigmsg->channel->caller_data.screen == 3) {
		switch_set_flag(tech_pvt->caller_profile, SWITCH_CPF_SCREEN);
	}

	tech_pvt->caller_profile->caller_ton = sigmsg->channel->caller_data.cid_num.type;
	tech_pvt->caller_profile->caller_numplan = sigmsg->channel->caller_data.cid_num.plan;
	tech_pvt->caller_profile->ani_ton = sigmsg->channel->caller_data.ani.type;
	tech_pvt->caller_profile->ani_numplan = sigmsg->channel->caller_data.ani.plan;
	tech_pvt->caller_profile->destination_number_ton = sigmsg->channel->caller_data.dnis.type;
	tech_pvt->caller_profile->destination_number_numplan = sigmsg->channel->caller_data.dnis.plan;
	tech_pvt->caller_profile->rdnis_ton = sigmsg->channel->caller_data.rdnis.type;
	tech_pvt->caller_profile->rdnis_numplan = sigmsg->channel->caller_data.rdnis.plan;

	if (sigmsg->channel->caller_data.pres) {
		switch_set_flag(tech_pvt->caller_profile, SWITCH_CPF_HIDE_NAME | SWITCH_CPF_HIDE_NUMBER);
	}
	
	snprintf(name, sizeof(name), "OpenZAP/%u:%u/%s", sigmsg->channel->span_id, sigmsg->channel->chan_id, tech_pvt->caller_profile->destination_number);
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Connect inbound channel %s\n", name);
	switch_channel_set_name(channel, name);
	switch_channel_set_caller_profile(channel, tech_pvt->caller_profile);

	switch_channel_set_variable(channel, "openzap_span_name", sigmsg->channel->span->name);
	switch_channel_set_variable_printf(channel, "openzap_span_number", "%d", sigmsg->channel->span_id);	
	switch_channel_set_variable_printf(channel, "openzap_chan_number", "%d", sigmsg->channel->chan_id);
	if (sigmsg->channel->caller_data.raw_data_len) {
		switch_channel_set_variable_printf(channel, "openzap_custom_call_data", "%s", sigmsg->channel->caller_data.raw_data);
	}
		
	switch_channel_set_state(channel, CS_INIT);
	if (switch_core_session_thread_launch(session) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Error spawning thread\n");
		switch_core_session_destroy(&session);
		return ZAP_FAIL;
	}

	if (zap_channel_add_token(sigmsg->channel, switch_core_session_get_uuid(session), 0) != ZAP_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Error adding token\n");
		switch_core_session_destroy(&session);
		return ZAP_FAIL;
	}
	*sp = session;

    return ZAP_SUCCESS;
}

static ZIO_SIGNAL_CB_FUNCTION(on_common_signal)
{
	switch_event_t *event = NULL;

	switch (sigmsg->event_id) {

	case ZAP_SIGEVENT_ALARM_CLEAR:
	case ZAP_SIGEVENT_ALARM_TRAP:
		{
			if (zap_channel_get_alarms(sigmsg->channel) != ZAP_SUCCESS) {
				zap_log(ZAP_LOG_ERROR, "failed to retrieve alarms\n");
				return ZAP_FAIL;
			}
			if (switch_event_create(&event, SWITCH_EVENT_TRAP) != SWITCH_STATUS_SUCCESS) {
				zap_log(ZAP_LOG_ERROR, "failed to create alarms events\n");
				return ZAP_FAIL;
			}
			if (sigmsg->event_id == ZAP_SIGEVENT_ALARM_CLEAR) {
				switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "condition", "zap-alarm-clear");
			} else {
				switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "condition", "zap-alarm-trap");
			}
		}
		break;
	default:
		return ZAP_SUCCESS;
		break;
	}

	switch_event_add_header(event, SWITCH_STACK_BOTTOM, "span-name", "%s", sigmsg->channel->span->name);
	switch_event_add_header(event, SWITCH_STACK_BOTTOM, "span-number", "%d", sigmsg->channel->span_id);
	switch_event_add_header(event, SWITCH_STACK_BOTTOM, "chan-number", "%d", sigmsg->channel->chan_id);

	if (zap_test_alarm_flag(sigmsg->channel, ZAP_ALARM_RECOVER)) {
		switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "alarm", "recover");
	}
	if (zap_test_alarm_flag(sigmsg->channel, ZAP_ALARM_LOOPBACK)) {
		switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "alarm", "loopback");
	}
	if (zap_test_alarm_flag(sigmsg->channel, ZAP_ALARM_YELLOW)) {
		switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "alarm", "yellow");
	}
	if (zap_test_alarm_flag(sigmsg->channel, ZAP_ALARM_RED)) {
		switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "alarm", "red");
	}
	if (zap_test_alarm_flag(sigmsg->channel, ZAP_ALARM_BLUE)) {
		switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "alarm", "blue");
	}
	if (zap_test_alarm_flag(sigmsg->channel, ZAP_ALARM_NOTOPEN)) {
		switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "alarm", "notopen");
	}
	if (zap_test_alarm_flag(sigmsg->channel, ZAP_ALARM_AIS)) {
		switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "alarm", "ais");
	}
	if (zap_test_alarm_flag(sigmsg->channel, ZAP_ALARM_RAI)) {
		switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "alarm", "rai");
	}
	if (zap_test_alarm_flag(sigmsg->channel, ZAP_ALARM_GENERAL)) {
		switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "alarm", "general");
	}
	switch_event_fire(&event);

	return ZAP_BREAK;
}

static void zap_enable_channel_dtmf(zap_channel_t *zchan, switch_channel_t *channel)
{
	if (channel) {
		const char *var;
		if ((var = switch_channel_get_variable(channel, "openzap_disable_dtmf"))) {
			if (switch_true(var)) {
				zap_channel_command(zchan, ZAP_COMMAND_DISABLE_DTMF_DETECT, NULL);
				zap_log(ZAP_LOG_INFO, "DTMF detection disabled in channel %d:%d\n", zchan->span_id, zchan->chan_id);
				return;
			}
		}
		/* the variable is not present or has negative value then proceed to enable DTMF */
	}	
	if (zap_channel_command(zchan, ZAP_COMMAND_ENABLE_DTMF_DETECT, NULL) != ZAP_SUCCESS) {
		zap_log(ZAP_LOG_ERROR, "Failed to enable DTMF detection in channel %d:%d\n", zchan->span_id, zchan->chan_id);
	}
}

static ZIO_SIGNAL_CB_FUNCTION(on_fxo_signal)
{
	switch_core_session_t *session = NULL;
	switch_channel_t *channel = NULL;
	zap_status_t status;

	zap_log(ZAP_LOG_DEBUG, "got FXO sig %d:%d [%s]\n", sigmsg->channel->span_id, sigmsg->channel->chan_id, zap_signal_event2str(sigmsg->event_id));

    switch(sigmsg->event_id) {

    case ZAP_SIGEVENT_PROGRESS_MEDIA:
		{
			if ((session = zap_channel_get_session(sigmsg->channel, 0))) {
				channel = switch_core_session_get_channel(session);
				switch_channel_mark_pre_answered(channel);
				switch_core_session_rwunlock(session);
			}
		}
		break;
    case ZAP_SIGEVENT_STOP:
		{
			private_t *tech_pvt = NULL;
			while((session = zap_channel_get_session(sigmsg->channel, 0))) {
				tech_pvt = switch_core_session_get_private(session);
				switch_set_flag_locked(tech_pvt, TFLAG_DEAD);
				zap_channel_clear_token(sigmsg->channel, 0);
				channel = switch_core_session_get_channel(session);
				switch_channel_hangup(channel, sigmsg->channel->caller_data.hangup_cause);
				zap_channel_clear_token(sigmsg->channel, switch_core_session_get_uuid(session));
				switch_core_session_rwunlock(session);
			}
		}
		break;
    case ZAP_SIGEVENT_UP:
		{
			if ((session = zap_channel_get_session(sigmsg->channel, 0))) {
				channel = switch_core_session_get_channel(session);
				switch_channel_mark_answered(channel);
				zap_enable_channel_dtmf(sigmsg->channel, channel);
				switch_core_session_rwunlock(session);
			}
		}
		break;
    case ZAP_SIGEVENT_START:
		{
			status = zap_channel_from_event(sigmsg, &session);
			if (status != ZAP_SUCCESS) {
				zap_set_state_locked(sigmsg->channel, ZAP_CHANNEL_STATE_DOWN);
			}
		}
		break;

	default:
		{
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "Unhandled msg type %d for channel %d:%d\n",
							  sigmsg->event_id, sigmsg->channel->span_id, sigmsg->channel->chan_id);
		}
		break;

	}

	return ZAP_SUCCESS;
}

static ZIO_SIGNAL_CB_FUNCTION(on_fxs_signal)
{
	switch_core_session_t *session = NULL;
	switch_channel_t *channel = NULL;
	zap_status_t status = ZAP_SUCCESS;

    zap_log(ZAP_LOG_DEBUG, "got FXS sig [%s]\n", zap_signal_event2str(sigmsg->event_id));

    switch(sigmsg->event_id) {
    case ZAP_SIGEVENT_UP:
		{
			if ((session = zap_channel_get_session(sigmsg->channel, 0))) {
				channel = switch_core_session_get_channel(session);
				switch_channel_mark_answered(channel);
				zap_enable_channel_dtmf(sigmsg->channel, channel);
				switch_core_session_rwunlock(session);
			}
		}
		break;
    case ZAP_SIGEVENT_PROGRESS:
		{
			if ((session = zap_channel_get_session(sigmsg->channel, 0))) {
				channel = switch_core_session_get_channel(session);
				switch_channel_mark_ring_ready(channel);
				switch_core_session_rwunlock(session);
			}
		}
		break;
    case ZAP_SIGEVENT_START:
		{
			zap_clear_flag_locked(sigmsg->channel, ZAP_CHANNEL_HOLD);
			status = zap_channel_from_event(sigmsg, &session);
			if (status != ZAP_SUCCESS) {
				zap_set_state_locked(sigmsg->channel, ZAP_CHANNEL_STATE_BUSY);
			}
		}
		break;
    case ZAP_SIGEVENT_STOP:
		{
			private_t *tech_pvt = NULL;
			switch_call_cause_t cause = SWITCH_CAUSE_NORMAL_CLEARING;
			if (sigmsg->channel->token_count) {
				switch_core_session_t *session_a, *session_b, *session_t = NULL;
				switch_channel_t *channel_a = NULL, *channel_b = NULL;
				int digits = !zstr(sigmsg->channel->caller_data.collected);
				const char *br_a_uuid = NULL, *br_b_uuid = NULL;
				private_t *tech_pvt = NULL;


				if ((session_a = switch_core_session_locate(sigmsg->channel->tokens[0]))) {
					channel_a = switch_core_session_get_channel(session_a);
					br_a_uuid = switch_channel_get_partner_uuid(channel_a);

					tech_pvt = switch_core_session_get_private(session_a);
					stop_hold(session_a, switch_channel_get_partner_uuid(channel_a));
					switch_clear_flag_locked(tech_pvt, TFLAG_HOLD);
				}

				if ((session_b = switch_core_session_locate(sigmsg->channel->tokens[1]))) {
					channel_b = switch_core_session_get_channel(session_b);
					br_b_uuid = switch_channel_get_partner_uuid(channel_b);

					tech_pvt = switch_core_session_get_private(session_b);
					stop_hold(session_a, switch_channel_get_partner_uuid(channel_b));
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
					switch_ivr_session_transfer(session_t, sigmsg->channel->caller_data.collected, NULL, NULL);
					switch_core_session_rwunlock(session_t);
				}

				if (session_a) {
					switch_core_session_rwunlock(session_a);
				}

				if (session_b) {
					switch_core_session_rwunlock(session_b);
				}

				
			}

			while((session = zap_channel_get_session(sigmsg->channel, 0))) {
				tech_pvt = switch_core_session_get_private(session);
				switch_set_flag_locked(tech_pvt, TFLAG_DEAD);
				channel = switch_core_session_get_channel(session);
				switch_channel_hangup(channel, cause);
				zap_channel_clear_token(sigmsg->channel, switch_core_session_get_uuid(session));
				switch_core_session_rwunlock(session);
			}
			zap_channel_clear_token(sigmsg->channel, NULL);
			
		}
		break;

    case ZAP_SIGEVENT_ADD_CALL:
		{
			cycle_foreground(sigmsg->channel, 1, NULL);
		}
		break;
    case ZAP_SIGEVENT_FLASH:
		{

			if (zap_test_flag(sigmsg->channel, ZAP_CHANNEL_HOLD) && sigmsg->channel->token_count == 1) {
				switch_core_session_t *session;
				if ((session = zap_channel_get_session(sigmsg->channel, 0))) {
					const char *buuid;
					switch_channel_t *channel;
					private_t *tech_pvt;
					
					tech_pvt = switch_core_session_get_private(session);
					channel = switch_core_session_get_channel(session);
					buuid = switch_channel_get_partner_uuid(channel);
					zap_set_state_locked(sigmsg->channel,  ZAP_CHANNEL_STATE_UP);
					stop_hold(session, buuid);
					switch_clear_flag_locked(tech_pvt, TFLAG_HOLD);
					switch_core_session_rwunlock(session);
				}
			} else if (sigmsg->channel->token_count == 2 && (SPAN_CONFIG[sigmsg->span_id].analog_options & ANALOG_OPTION_3WAY)) {
				if (zap_test_flag(sigmsg->channel, ZAP_CHANNEL_3WAY)) {
					zap_clear_flag(sigmsg->channel, ZAP_CHANNEL_3WAY);
					if ((session = zap_channel_get_session(sigmsg->channel, 1))) {
						channel = switch_core_session_get_channel(session);
						switch_channel_hangup(channel, SWITCH_CAUSE_NORMAL_CLEARING);
						zap_channel_clear_token(sigmsg->channel, switch_core_session_get_uuid(session));
						switch_core_session_rwunlock(session);
					}
					cycle_foreground(sigmsg->channel, 1, NULL);
				} else {
					char *cmd;
					cmd = switch_mprintf("three_way::%s", sigmsg->channel->tokens[0]);
					zap_set_flag(sigmsg->channel, ZAP_CHANNEL_3WAY);
					cycle_foreground(sigmsg->channel, 1, cmd);
					free(cmd);
				}
			} else if ((SPAN_CONFIG[sigmsg->span_id].analog_options & ANALOG_OPTION_CALL_SWAP)
					   || (SPAN_CONFIG[sigmsg->span_id].analog_options & ANALOG_OPTION_3WAY)
					   ) { 
				cycle_foreground(sigmsg->channel, 1, NULL);
				if (sigmsg->channel->token_count == 1) {
					zap_set_flag_locked(sigmsg->channel, ZAP_CHANNEL_HOLD);
					zap_set_state_locked(sigmsg->channel, ZAP_CHANNEL_STATE_DIALTONE);
				}
			}
			
		}
		break;

    case ZAP_SIGEVENT_COLLECTED_DIGIT:
		{
			char *dtmf = sigmsg->raw_data;
			char *regex = SPAN_CONFIG[sigmsg->channel->span->span_id].dial_regex;
			char *fail_regex = SPAN_CONFIG[sigmsg->channel->span->span_id].fail_dial_regex;
			
			if (zstr(regex)) {
				regex = NULL;
			}

			if (zstr(fail_regex)) {
				fail_regex = NULL;
			}

			zap_log(ZAP_LOG_DEBUG, "got DTMF sig [%s]\n", dtmf);
			switch_set_string(sigmsg->channel->caller_data.collected, dtmf);
			
			if ((regex || fail_regex) && !zstr(dtmf)) {
				switch_regex_t *re = NULL;
				int ovector[30];
				int match = 0;

				if (fail_regex) {
					match = switch_regex_perform(dtmf, fail_regex, &re, ovector, sizeof(ovector) / sizeof(ovector[0]));
					status = match ? ZAP_SUCCESS : ZAP_BREAK;
					switch_regex_safe_free(re);
				}

				if (status == ZAP_SUCCESS && regex) {
					match = switch_regex_perform(dtmf, regex, &re, ovector, sizeof(ovector) / sizeof(ovector[0]));
					status = match ? ZAP_BREAK : ZAP_SUCCESS;
				}
				
				switch_regex_safe_free(re);
			}

		}
		break;

	default:
		{
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "Unhandled msg type %d for channel %d:%d\n",
							  sigmsg->event_id, sigmsg->channel->span_id, sigmsg->channel->chan_id);
		}
		break;

	}

	return status;
}

static ZIO_SIGNAL_CB_FUNCTION(on_r2_signal)
{
	switch_core_session_t *session = NULL;
	switch_channel_t *channel = NULL;
	zap_status_t status = ZAP_SUCCESS;

	zap_log(ZAP_LOG_DEBUG, "Got R2 channel sig [%s] in channel %d\n", zap_signal_event2str(sigmsg->event_id), sigmsg->channel->physical_chan_id);

	if (on_common_signal(sigmsg) == ZAP_BREAK) {
		return ZAP_SUCCESS;
	}

	switch(sigmsg->event_id) {
		/* on_call_disconnect from the R2 side */
		case ZAP_SIGEVENT_STOP: 
		{	
			private_t *tech_pvt = NULL;
			while((session = zap_channel_get_session(sigmsg->channel, 0))) {
				tech_pvt = switch_core_session_get_private(session);
				switch_set_flag_locked(tech_pvt, TFLAG_DEAD);
				channel = switch_core_session_get_channel(session);
				switch_channel_hangup(channel, sigmsg->channel->caller_data.hangup_cause);
				zap_channel_clear_token(sigmsg->channel, switch_core_session_get_uuid(session));
				switch_core_session_rwunlock(session);
			}
		}
		break;

		/* on_call_offered from the R2 side */
		case ZAP_SIGEVENT_START: 
		{
			status = zap_channel_from_event(sigmsg, &session);
		}
		break;

		/* on DNIS received from the R2 forward side, return status == ZAP_BREAK to stop requesting DNIS */
		case ZAP_SIGEVENT_COLLECTED_DIGIT: 
		{
			char *regex = SPAN_CONFIG[sigmsg->channel->span->span_id].dial_regex;
			char *fail_regex = SPAN_CONFIG[sigmsg->channel->span->span_id].fail_dial_regex;

			if (zstr(regex)) {
				regex = NULL;
			}

			if (zstr(fail_regex)) {
				fail_regex = NULL;
			}

			zap_log(ZAP_LOG_DEBUG, "R2 DNIS so far [%s]\n", sigmsg->channel->caller_data.dnis.digits);

			if ((regex || fail_regex) && !zstr(sigmsg->channel->caller_data.dnis.digits)) {
				switch_regex_t *re = NULL;
				int ovector[30];
				int match = 0;

				if (fail_regex) {
					match = switch_regex_perform(sigmsg->channel->caller_data.dnis.digits, fail_regex, &re, ovector, sizeof(ovector) / sizeof(ovector[0]));
					status = match ? ZAP_SUCCESS : ZAP_BREAK;
					switch_regex_safe_free(re);
				}

				if (status == ZAP_SUCCESS && regex) {
					match = switch_regex_perform(sigmsg->channel->caller_data.dnis.digits, regex, &re, ovector, sizeof(ovector) / sizeof(ovector[0]));
					status = match ? ZAP_BREAK : ZAP_SUCCESS;
				}

				switch_regex_safe_free(re);
			}
		}
		break;

		case ZAP_SIGEVENT_PROGRESS:
		{
			if ((session = zap_channel_get_session(sigmsg->channel, 0))) {
				channel = switch_core_session_get_channel(session);
				switch_channel_mark_ring_ready(channel);
				switch_core_session_rwunlock(session);
			}
		}
		break;

		case ZAP_SIGEVENT_UP:
		{
			if ((session = zap_channel_get_session(sigmsg->channel, 0))) {
				channel = switch_core_session_get_channel(session);
				switch_channel_mark_answered(channel);
				zap_enable_channel_dtmf(sigmsg->channel, channel);
				switch_core_session_rwunlock(session);
			}
		}
		break;

		default:
		{
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "Unhandled event %d from R2 for channel %d:%d\n",
			sigmsg->event_id, sigmsg->channel->span_id, sigmsg->channel->chan_id);
		}
		break;
	}

	return status;
}

static ZIO_SIGNAL_CB_FUNCTION(on_clear_channel_signal)
{
	switch_core_session_t *session = NULL;
	switch_channel_t *channel = NULL;

	zap_log(ZAP_LOG_DEBUG, "got clear channel sig [%s]\n", zap_signal_event2str(sigmsg->event_id));

	if (on_common_signal(sigmsg) == ZAP_BREAK) {
		return ZAP_SUCCESS;
	}

    switch(sigmsg->event_id) {
    case ZAP_SIGEVENT_START:
		{
			zap_enable_channel_dtmf(sigmsg->channel, channel);
			return zap_channel_from_event(sigmsg, &session);
		}
		break;
    case ZAP_SIGEVENT_STOP:
    case ZAP_SIGEVENT_RESTART:
		{
			private_t *tech_pvt = NULL;
			while((session = zap_channel_get_session(sigmsg->channel, 0))) {
				tech_pvt = switch_core_session_get_private(session);
				switch_set_flag_locked(tech_pvt, TFLAG_DEAD);
				channel = switch_core_session_get_channel(session);
				switch_channel_hangup(channel, sigmsg->channel->caller_data.hangup_cause);
				zap_channel_clear_token(sigmsg->channel, switch_core_session_get_uuid(session));
				switch_core_session_rwunlock(session);
			}
		}
		break;
    case ZAP_SIGEVENT_UP:
		{
			if ((session = zap_channel_get_session(sigmsg->channel, 0))) {
				channel = switch_core_session_get_channel(session);
				switch_channel_mark_answered(channel);
				zap_enable_channel_dtmf(sigmsg->channel, channel);
				switch_core_session_rwunlock(session);
			} else {
				const char *uuid = zap_channel_get_uuid(sigmsg->channel, 0);

				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Session for channel %d:%d not found [UUID: %s]\n",
					sigmsg->channel->span_id, sigmsg->channel->chan_id, (uuid) ? uuid : "N/A");
			}
		}
    case ZAP_SIGEVENT_PROGRESS_MEDIA:
		{
			if ((session = zap_channel_get_session(sigmsg->channel, 0))) {
				channel = switch_core_session_get_channel(session);
				switch_channel_mark_pre_answered(channel);
				switch_core_session_rwunlock(session);
			} else {
				const char *uuid = zap_channel_get_uuid(sigmsg->channel, 0);

				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Session for channel %d:%d not found [UUID: %s]\n",
					sigmsg->channel->span_id, sigmsg->channel->chan_id, (uuid) ? uuid : "N/A");
			}
		}
		break;
    case ZAP_SIGEVENT_PROGRESS:
		{
			if ((session = zap_channel_get_session(sigmsg->channel, 0))) {
				channel = switch_core_session_get_channel(session);
				switch_channel_mark_ring_ready(channel);
				switch_core_session_rwunlock(session);
			} else {
				const char *uuid = zap_channel_get_uuid(sigmsg->channel, 0);

				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Session for channel %d:%d not found [UUID: %s]\n",
					sigmsg->channel->span_id, sigmsg->channel->chan_id, (uuid) ? uuid : "N/A");
			}
		}
		break;

	default:
		{
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "Unhandled msg type %d for channel %d:%d\n",
							  sigmsg->event_id, sigmsg->channel->span_id, sigmsg->channel->chan_id);
		}
		break;
	}

	return ZAP_SUCCESS;
}


static ZIO_SIGNAL_CB_FUNCTION(on_analog_signal)
{
	switch_status_t status = SWITCH_STATUS_FALSE;

	if (on_common_signal(sigmsg) == ZAP_BREAK) {
		return ZAP_SUCCESS;
	}

	switch (sigmsg->channel->type) {
	case ZAP_CHAN_TYPE_FXO:
	case ZAP_CHAN_TYPE_EM:
		{
			status = on_fxo_signal(sigmsg);
		}
		break;
	case ZAP_CHAN_TYPE_FXS:
		{
			status = on_fxs_signal(sigmsg);
		}
		break;
	default: 
		{
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "Unhandled analog channel type %d for channel %d:%d\n",
							  sigmsg->channel->type, sigmsg->channel->span_id, sigmsg->channel->chan_id);
		}
		break;
	}

	return status;
}

static void zap_logger(const char *file, const char *func, int line, int level, const char *fmt, ...)
{
    char *data = NULL;
    va_list ap;
	
    va_start(ap, fmt);

	if (switch_vasprintf(&data, fmt, ap) != -1) {
		switch_log_printf(SWITCH_CHANNEL_ID_LOG, file, (char *)func, line, NULL, level, "%s", data);
		free(data);
	}
	
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

static switch_status_t load_config(void)
{
	const char *cf = "openzap.conf";
	switch_xml_t cfg, xml, settings, param, spans, myspan;

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
			} else if (!strcasecmp(var, "enable-analog-option")) {
				globals.analog_options = enable_analog_option(val, globals.analog_options);
			}
		}
	}

	if ((spans = switch_xml_child(cfg, "analog_spans"))) {
		for (myspan = switch_xml_child(spans, "span"); myspan; myspan = myspan->next) {
			char *id = (char *) switch_xml_attr(myspan, "id");
			char *name = (char *) switch_xml_attr(myspan, "name");
			zap_status_t zstatus = ZAP_FAIL;
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
			int callwaiting = 1;

			uint32_t span_id = 0, to = 0, max = 0;
			zap_span_t *span = NULL;
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
				} else if (!strcasecmp(var, "enable-callerid")) {
					enable_callerid = val;
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
				zstatus = zap_span_find_by_name(name, &span);
			} else {
				if (switch_is_number(id)) {
					span_id = atoi(id);
					zstatus = zap_span_find(span_id, &span);
				}

				if (zstatus != ZAP_SUCCESS) {
					zstatus = zap_span_find_by_name(id, &span);
				}
			}

			if (zstatus != ZAP_SUCCESS) {
				zap_log(ZAP_LOG_ERROR, "Error finding OpenZAP span id:%s name:%s\n", switch_str_nil(id), switch_str_nil(name));
				continue;
			}
			
			if (!span_id) {
				span_id = span->span_id;
			}

			if (zap_configure_span("analog", span, on_analog_signal, 
								   "tonemap", tonegroup, 
								   "digit_timeout", &to,
								   "max_dialstr", &max,
								   "hotline", hotline ? hotline : "",
								   "enable_callerid", enable_callerid,
								   "callwaiting", &callwaiting,
								   TAG_END) != ZAP_SUCCESS) {
				zap_log(ZAP_LOG_ERROR, "Error starting OpenZAP span %d\n", span_id);
				continue;
			}

			SPAN_CONFIG[span->span_id].span = span;
			switch_set_string(SPAN_CONFIG[span->span_id].context, context);
			switch_set_string(SPAN_CONFIG[span->span_id].dialplan, dialplan);
			SPAN_CONFIG[span->span_id].analog_options = analog_options | globals.analog_options;
			
			if (dial_regex) {
				switch_set_string(SPAN_CONFIG[span->span_id].dial_regex, dial_regex);
			}

			if (fail_dial_regex) {
				switch_set_string(SPAN_CONFIG[span->span_id].fail_dial_regex, fail_dial_regex);
			}

			if (hold_music) {
				switch_set_string(SPAN_CONFIG[span->span_id].hold_music, hold_music);
			}
			switch_copy_string(SPAN_CONFIG[span->span_id].type, "analog", sizeof(SPAN_CONFIG[span->span_id].type));
			zap_span_start(span);
		}
	}

	if ((spans = switch_xml_child(cfg, "analog_em_spans"))) {
		for (myspan = switch_xml_child(spans, "span"); myspan; myspan = myspan->next) {
			char *id = (char *) switch_xml_attr(myspan, "id");
			char *name = (char *) switch_xml_attr(myspan, "name");
			zap_status_t zstatus = ZAP_FAIL;
			const char *context = "default";
			const char *dialplan = "XML";
			const char *tonegroup = NULL;
			char *digit_timeout = NULL;
			char *max_digits = NULL;
			char *dial_regex = NULL;
			char *hold_music = NULL;
			char *fail_dial_regex = NULL;
			uint32_t span_id = 0, to = 0, max = 0;
			zap_span_t *span = NULL;
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
				zstatus = zap_span_find_by_name(name, &span);
			} else {
				if (switch_is_number(id)) {
					span_id = atoi(id);
					zstatus = zap_span_find(span_id, &span);
				}

				if (zstatus != ZAP_SUCCESS) {
					zstatus = zap_span_find_by_name(id, &span);
				}
			}

			if (zstatus != ZAP_SUCCESS) {
				zap_log(ZAP_LOG_ERROR, "Error finding OpenZAP span id:%s name:%s\n", switch_str_nil(id), switch_str_nil(name));
				continue;
			}
			
			if (!span_id) {
				span_id = span->span_id;
			}


			if (zap_configure_span("analog_em", span, on_analog_signal, 
								   "tonemap", tonegroup, 
								   "digit_timeout", &to,
								   "max_dialstr", &max,
								   TAG_END) != ZAP_SUCCESS) {
				zap_log(ZAP_LOG_ERROR, "Error starting OpenZAP span %d\n", span_id);
				continue;
			}

			SPAN_CONFIG[span->span_id].span = span;
			switch_set_string(SPAN_CONFIG[span->span_id].context, context);
			switch_set_string(SPAN_CONFIG[span->span_id].dialplan, dialplan);
			SPAN_CONFIG[span->span_id].analog_options = analog_options | globals.analog_options;
			
			if (dial_regex) {
				switch_set_string(SPAN_CONFIG[span->span_id].dial_regex, dial_regex);
			}

			if (fail_dial_regex) {
				switch_set_string(SPAN_CONFIG[span->span_id].fail_dial_regex, fail_dial_regex);
			}

			if (hold_music) {
				switch_set_string(SPAN_CONFIG[span->span_id].hold_music, hold_music);
			}
			switch_copy_string(SPAN_CONFIG[span->span_id].type, "analog_em", sizeof(SPAN_CONFIG[span->span_id].type));
			zap_span_start(span);
		}
	}

	if ((spans = switch_xml_child(cfg, "pri_spans"))) {
		for (myspan = switch_xml_child(spans, "span"); myspan; myspan = myspan->next) {
			char *id = (char *) switch_xml_attr(myspan, "id");
			char *name = (char *) switch_xml_attr(myspan, "name");
			zap_status_t zstatus = ZAP_FAIL;
			const char *context = "default";
			const char *dialplan = "XML";
			//Q921NetUser_t mode = Q931_TE;
			//Q931Dialect_t dialect = Q931_Dialect_National;
			char *mode = NULL;
			char *dialect = NULL;
			uint32_t span_id = 0;
			zap_span_t *span = NULL;
			const char *tonegroup = NULL;
			char *digit_timeout = NULL;
			const char *opts = "none";
			uint32_t to = 0;
			int q921loglevel = -1;
			int q931loglevel = -1;
			
			// quick debug
			//switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "ID: '%s', Name:'%s'\n",id,name);

			for (param = switch_xml_child(myspan, "param"); param; param = param->next) {
				char *var = (char *) switch_xml_attr_soft(param, "name");
				char *val = (char *) switch_xml_attr_soft(param, "value");

				if (!strcasecmp(var, "tonegroup")) {
					tonegroup = val;
				} else if (!strcasecmp(var, "mode")) {
					mode = val;
				} else if (!strcasecmp(var, "dialect")) {
					dialect = val;
				} else if (!strcasecmp(var, "q921loglevel")) {
                    if ((q921loglevel = switch_log_str2level(val)) == SWITCH_LOG_INVALID) {
                        q921loglevel = -1;
                    }
				} else if (!strcasecmp(var, "q931loglevel")) {
                    if ((q931loglevel = switch_log_str2level(val)) == SWITCH_LOG_INVALID) {
                        q931loglevel = -1;
                    }
				} else if (!strcasecmp(var, "context")) {
					context = val;
				} else if (!strcasecmp(var, "opts")) {
					opts = val;
				} else if (!strcasecmp(var, "dialplan")) {
					dialplan = val;
				} else if (!strcasecmp(var, "digit_timeout") || !strcasecmp(var, "digit-timeout")) {
					digit_timeout = val;
				}
			}
	
			if (!id && !name) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "span missing required param 'id'\n");
				continue;
			}
			
			if (name) {
				zstatus = zap_span_find_by_name(name, &span);
			} else {
				if (switch_is_number(id)) {
					span_id = atoi(id);
					zstatus = zap_span_find(span_id, &span);
				}

				if (zstatus != ZAP_SUCCESS) {
					zstatus = zap_span_find_by_name(id, &span);
				}
			}

			if (digit_timeout) {
				to = atoi(digit_timeout);
			}
			
			if (zstatus != ZAP_SUCCESS) {
				zap_log(ZAP_LOG_ERROR, "Error finding OpenZAP span id:%s name:%s\n", switch_str_nil(id), switch_str_nil(name));
				continue;
			}

                        if (!span_id) {
                                span_id = span->span_id;
                        }

			if (!tonegroup) {
				tonegroup = "us";
			}
			
			if (zap_configure_span("isdn", span, on_clear_channel_signal, 
								   "mode", mode,
								   "dialect", dialect,
								   "digit_timeout", &to,
								   "opts", opts,
								   "tonemap", tonegroup,
								   "q921loglevel", q921loglevel,
								   "q931loglevel", q931loglevel,
								   TAG_END) != ZAP_SUCCESS) {
				zap_log(ZAP_LOG_ERROR, "Error starting OpenZAP span %d mode: %s dialect: %s error: %s\n", span_id, mode, dialect, span->last_error);
				continue;
			}

			SPAN_CONFIG[span->span_id].span = span;
			switch_copy_string(SPAN_CONFIG[span->span_id].context, context, sizeof(SPAN_CONFIG[span->span_id].context));
			switch_copy_string(SPAN_CONFIG[span->span_id].dialplan, dialplan, sizeof(SPAN_CONFIG[span->span_id].dialplan));
			switch_copy_string(SPAN_CONFIG[span->span_id].type, "isdn", sizeof(SPAN_CONFIG[span->span_id].type));

			zap_span_start(span);
		}
	}



	if ((spans = switch_xml_child(cfg, "libpri_spans"))) {
		for (myspan = switch_xml_child(spans, "span"); myspan; myspan = myspan->next) {
			char *id = (char *) switch_xml_attr(myspan, "id");
			char *name = (char *) switch_xml_attr(myspan, "name");
			zap_status_t zstatus = ZAP_FAIL;
			const char *context = "default";
			const char *dialplan = "XML";
			
			const char *o_node = "cpe";
			const char *o_switch = "dms100";
			const char *o_dp = "unknown";
			const char *o_l1 = "ulaw";
			const char *o_debug = "none";
			const char* opts = "none";	
					
			uint32_t span_id = 0;
			zap_span_t *span = NULL;

			
			for (param = switch_xml_child(myspan, "param"); param; param = param->next) {
				char *var = (char *) switch_xml_attr_soft(param, "name");
				char *val = (char *) switch_xml_attr_soft(param, "value");

				if (!strcasecmp(var, "node")) {
					o_node = val;
				} else if (!strcasecmp(var, "switch")) {
					o_switch = val;
				} else if (!strcasecmp(var, "dp")) {
					o_dp = val;
				} else if (!strcasecmp(var, "l1")) {
					o_l1 = val;
				} else if (!strcasecmp(var, "debug")) {
					o_debug = val;
				} else if (!strcasecmp(var, "context")) {
					context = val;
				} else if (!strcasecmp(var, "opts")) {
					opts = val;
				} else if (!strcasecmp(var, "dialplan")) {
					dialplan = val;
				}
			}
	
			if (!id && !name) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "span missing required param 'id'\n");
				continue;
			}
			
			if (name) {
				zstatus = zap_span_find_by_name(name, &span);
			} else {
				if (switch_is_number(id)) {
					span_id = atoi(id);
					zstatus = zap_span_find(span_id, &span);
				}

				if (zstatus != ZAP_SUCCESS) {
					zstatus = zap_span_find_by_name(id, &span);
				}
			}

			if (zstatus != ZAP_SUCCESS) {
				zap_log(ZAP_LOG_ERROR, "Error finding OpenZAP span id:%s name:%s\n", switch_str_nil(id), switch_str_nil(name));
				continue;
			}

			if (!span_id) {
				span_id = span->span_id;
			}
			
			
			if (zap_configure_span("libpri", span, on_clear_channel_signal, 
								   "node", o_node,
								   "switch", o_switch,
								   "dp", o_dp,
								   "l1", o_l1,
								   "debug", o_debug,
								   "opts", opts,
								   TAG_END) != ZAP_SUCCESS) {
				zap_log(ZAP_LOG_ERROR, "Error starting OpenZAP span %d node: %s switch: %s dp: %s l1: %s debug: %s error: %s\n", 
						span_id, switch_str_nil(o_node), switch_str_nil(o_switch), switch_str_nil(o_dp), switch_str_nil(o_l1), switch_str_nil(o_debug),
						span->last_error);
				continue;
			}

			SPAN_CONFIG[span->span_id].span = span;
			switch_copy_string(SPAN_CONFIG[span->span_id].context, context, sizeof(SPAN_CONFIG[span->span_id].context));
			switch_copy_string(SPAN_CONFIG[span->span_id].dialplan, dialplan, sizeof(SPAN_CONFIG[span->span_id].dialplan));
			switch_copy_string(SPAN_CONFIG[span->span_id].type, "isdn", sizeof(SPAN_CONFIG[span->span_id].type));

			zap_span_start(span);
		}
	}

	if ((spans = switch_xml_child(cfg, "boost_spans"))) {
		for (myspan = switch_xml_child(spans, "span"); myspan; myspan = myspan->next) {
			char *id = (char *) switch_xml_attr(myspan, "id");
			char *name = (char *) switch_xml_attr(myspan, "name");
			zap_status_t zstatus = ZAP_FAIL;
			const char *context = "default";
			const char *dialplan = "XML";
			const char *outbound_called_ton = "national";
			const char *outbound_called_npi = "isdn";
			const char *outbound_calling_ton = "national";
			const char *outbound_calling_npi = "isdn";
			const char *outbound_rdnis_ton = "national";
			const char *outbound_rdnis_npi = "isdn";
			uint32_t span_id = 0;
			zap_span_t *span = NULL;
			const char *tonegroup = NULL;
			char *local_ip = NULL;
			int local_port = 0;
			char *remote_ip = NULL;
			int remote_port = 0;

			for (param = switch_xml_child(myspan, "param"); param; param = param->next) {
				char *var = (char *) switch_xml_attr_soft(param, "name");
				char *val = (char *) switch_xml_attr_soft(param, "value");

				if (!strcasecmp(var, "tonegroup")) {
					tonegroup = val;
				} else if (!strcasecmp(var, "local-ip")) {
					local_ip = val;
				} else if (!strcasecmp(var, "local-port")) {
					local_port = atoi(val);
				} else if (!strcasecmp(var, "remote-ip")) {
					remote_ip = val;
				} else if (!strcasecmp(var, "remote-port")) {
					remote_port = atoi(val);
				} else if (!strcasecmp(var, "context")) {
					context = val;
				} else if (!strcasecmp(var, "outbound-called-ton")) {
					outbound_called_ton = val;
				} else if (!strcasecmp(var, "outbound-called-npi")) {
					outbound_called_npi = val;
				} else if (!strcasecmp(var, "outbound-calling-ton")) {
					outbound_calling_ton = val;
				} else if (!strcasecmp(var, "outbound-calling-npi")) {
					outbound_calling_npi = val;
				} else if (!strcasecmp(var, "outbound-rdnis-ton")) {
					outbound_rdnis_ton = val;
				} else if (!strcasecmp(var, "outbound-rdnis-npi")) {
					outbound_rdnis_npi = val;
				} 
			}
				
			if (!id && !name) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "span missing required param\n");
				continue;
			}

			if (!tonegroup) {
				tonegroup = "us";
			}
			
			if (name) {
				zstatus = zap_span_find_by_name(name, &span);
			} else {
				if (switch_is_number(id)) {
					span_id = atoi(id);
					zstatus = zap_span_find(span_id, &span);
				}

				if (zstatus != ZAP_SUCCESS) {
					zstatus = zap_span_find_by_name(id, &span);
				}
			}

			if (zstatus != ZAP_SUCCESS) {
				zap_log(ZAP_LOG_ERROR, "Error finding OpenZAP span id:%s name:%s\n", switch_str_nil(id), switch_str_nil(name));
				continue;
			}
			
			if (!span_id) {
				span_id = span->span_id;
			}

			zap_set_npi(outbound_called_npi, &span->default_caller_data.ani.plan);
			zap_set_npi(outbound_calling_npi, &span->default_caller_data.cid_num.plan);
			zap_set_npi(outbound_rdnis_npi, &span->default_caller_data.rdnis.plan);

			zap_set_ton(outbound_called_ton, &span->default_caller_data.ani.type);
			zap_set_ton(outbound_calling_ton, &span->default_caller_data.cid_num.type);
			zap_set_ton(outbound_rdnis_ton, &span->default_caller_data.rdnis.type);

			if (zap_configure_span("sangoma_boost", span, on_clear_channel_signal, 
								   "local_ip", local_ip,
								   "local_port", &local_port,
								   "remote_ip", remote_ip,
								   "remote_port", &remote_port,
								   TAG_END) != ZAP_SUCCESS) {
				zap_log(ZAP_LOG_ERROR, "Error starting OpenZAP span %d error: %s\n", span_id, span->last_error);
				continue;
			}

			SPAN_CONFIG[span->span_id].span = span;
			switch_copy_string(SPAN_CONFIG[span->span_id].context, context, sizeof(SPAN_CONFIG[span->span_id].context));
			switch_copy_string(SPAN_CONFIG[span->span_id].dialplan, dialplan, sizeof(SPAN_CONFIG[span->span_id].dialplan));

			zap_span_start(span);
			switch_copy_string(SPAN_CONFIG[span->span_id].type, "Sangoma (boost)", sizeof(SPAN_CONFIG[span->span_id].type));
		}
	}

	if ((spans = switch_xml_child(cfg, "r2_spans"))) {
		for (myspan = switch_xml_child(spans, "span"); myspan; myspan = myspan->next) {
			char *id = (char *) switch_xml_attr(myspan, "id");
			char *name = (char *) switch_xml_attr(myspan, "name");
			zap_status_t zstatus = ZAP_FAIL;

			/* strings */
			const char *variant = "itu";
			const char *category = "national_subscriber";
			const char *logdir = "/usr/local/freeswitch/log/"; /* FIXME: get PREFIX variable */
			const char *logging = "notice,warning,error";
			const char *advanced_protocol_file = "";

			/* booleans */
			int call_files = 0;
			int get_ani_first = -1;
			int immediate_accept = -1;
			int double_answer = -1;
			int skip_category = -1;
			int forced_release = -1;
			int charge_calls = -1;

			/* integers */
			int mfback_timeout = -1;
			int metering_pulse_timeout = -1;
			int allow_collect_calls = -1;
			int max_ani = 10;
			int max_dnis = 4;

			/* common non r2 stuff */
			const char *context = "default";
			const char *dialplan = "XML";
			char *dial_regex = NULL;
			char *fail_dial_regex = NULL;
			uint32_t span_id = 0;
			zap_span_t *span = NULL;


			for (param = switch_xml_child(myspan, "param"); param; param = param->next) {
				char *var = (char *) switch_xml_attr_soft(param, "name");
				char *val = (char *) switch_xml_attr_soft(param, "value");

				/* string parameters */
				if (!strcasecmp(var, "variant")) {
					variant = val;
				} else if (!strcasecmp(var, "category")) {
					category = val;
				} else if (!strcasecmp(var, "logdir")) {
					logdir = val;
				} else if (!strcasecmp(var, "logging")) {
					logging = val;
				} else if (!strcasecmp(var, "advanced_protocol_file")) {
					advanced_protocol_file = val;

				/* booleans */
				} else if (!strcasecmp(var, "allow_collect_calls")) {
					allow_collect_calls = switch_true(val);
				} else if (!strcasecmp(var, "immediate_accept")) {
					immediate_accept = switch_true(val);
				} else if (!strcasecmp(var, "double_answer")) {
					double_answer = switch_true(val);
				} else if (!strcasecmp(var, "skip_category")) {
					skip_category = switch_true(var);
				} else if (!strcasecmp(var, "forced_release")) {
					forced_release = switch_true(val);
				} else if (!strcasecmp(var, "charge_calls")) {
					charge_calls = switch_true(val);
				} else if (!strcasecmp(var, "get_ani_first")) {
					get_ani_first = switch_true(val);
				} else if (!strcasecmp(var, "call_files")) {
					call_files = switch_true(val);

				/* integers */
				} else if (!strcasecmp(var, "mfback_timeout")) {
					mfback_timeout = atoi(val);
				} else if (!strcasecmp(var, "metering_pulse_timeout")) {
					metering_pulse_timeout = atoi(val);
				} else if (!strcasecmp(var, "max_ani")) {
					max_ani = atoi(val);
				} else if (!strcasecmp(var, "max_dnis")) {
					max_dnis = atoi(val);

				/* common non r2 stuff */
				} else if (!strcasecmp(var, "context")) {
					context = val;
				} else if (!strcasecmp(var, "dialplan")) {
					dialplan = val;
				} else if (!strcasecmp(var, "dial-regex")) {
					dial_regex = val;
				} else if (!strcasecmp(var, "fail-dial-regex")) {
					fail_dial_regex = val;
				}
			}

			if (!id && !name) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "span missing required param 'id'\n");
				continue;
			}

			if (name) {
				zstatus = zap_span_find_by_name(name, &span);
			} else {
				if (switch_is_number(id)) {
					span_id = atoi(id);
					zstatus = zap_span_find(span_id, &span);
				}

				if (zstatus != ZAP_SUCCESS) {
					zstatus = zap_span_find_by_name(id, &span);
				}
			}

			if (zstatus != ZAP_SUCCESS) {
				zap_log(ZAP_LOG_ERROR, "Error finding OpenZAP span id:%s name:%s\n", switch_str_nil(id), switch_str_nil(name));
				continue;
			}

			if (!span_id) {
				span_id = span->span_id;
			}

			if (zap_configure_span("r2", span, on_r2_signal, 
				"variant", variant, 
				"max_ani", max_ani,
				"max_dnis", max_dnis,
				"category", category,
				"logdir", logdir,
				"logging", logging,
				"advanced_protocol_file", advanced_protocol_file,
				"allow_collect_calls", allow_collect_calls,
				"immediate_accept", immediate_accept,
				"double_answer", double_answer,
				"skip_category", skip_category,
				"forced_release", forced_release,
				"charge_calls", charge_calls,
				"get_ani_first", get_ani_first,
				"call_files", call_files,
				"mfback_timeout", mfback_timeout,
				"metering_pulse_timeout", metering_pulse_timeout, 
				TAG_END) != ZAP_SUCCESS) {
				zap_log(ZAP_LOG_ERROR, "Error configuring R2 OpenZAP span %d, error: %s\n", 
				span_id, span->last_error);
				continue;
			}

			if (dial_regex) {
				switch_set_string(SPAN_CONFIG[span->span_id].dial_regex, dial_regex);
			}

			if (fail_dial_regex) {
				switch_set_string(SPAN_CONFIG[span->span_id].fail_dial_regex, fail_dial_regex);
			}

			SPAN_CONFIG[span->span_id].span = span;
			switch_copy_string(SPAN_CONFIG[span->span_id].context, context, sizeof(SPAN_CONFIG[span->span_id].context));
			switch_copy_string(SPAN_CONFIG[span->span_id].dialplan, dialplan, sizeof(SPAN_CONFIG[span->span_id].dialplan));
			switch_copy_string(SPAN_CONFIG[span->span_id].type, "r2", sizeof(SPAN_CONFIG[span->span_id].type));

			if (zap_span_start(span) == ZAP_FAIL) {
				zap_log(ZAP_LOG_ERROR, "Error starting R2 OpenZAP span %d, error: %s\n", span_id, span->last_error);
				continue;
			}
		}
	}

	switch_xml_free(xml);

	return SWITCH_STATUS_SUCCESS;
}


void dump_chan(zap_span_t *span, uint32_t chan_id, switch_stream_handle_t *stream)
{
	if (chan_id > span->chan_count) {
		return;
	}

	stream->write_function(stream,
						   "span_id: %u\n"
						   "chan_id: %u\n"
						   "physical_span_id: %u\n"
						   "physical_chan_id: %u\n"
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
						   "cause: %s\n\n",
						   span->channels[chan_id]->span_id,
						   span->channels[chan_id]->chan_id,
						   span->channels[chan_id]->physical_span_id,
						   span->channels[chan_id]->physical_chan_id,
						   zap_chan_type2str(span->channels[chan_id]->type),
						   zap_channel_state2str(span->channels[chan_id]->state),
						   zap_channel_state2str(span->channels[chan_id]->last_state),
						   span->channels[chan_id]->txgain,
						   span->channels[chan_id]->rxgain,
						   span->channels[chan_id]->caller_data.cid_date,
						   span->channels[chan_id]->caller_data.cid_name,
						   span->channels[chan_id]->caller_data.cid_num.digits,
						   span->channels[chan_id]->caller_data.ani.digits,
						   span->channels[chan_id]->caller_data.aniII,
						   span->channels[chan_id]->caller_data.dnis.digits,
						   span->channels[chan_id]->caller_data.rdnis.digits,
						   switch_channel_cause2str(span->channels[chan_id]->caller_data.hangup_cause)
						   );
}

void dump_chan_xml(zap_span_t *span, uint32_t chan_id, switch_stream_handle_t *stream)
{
	if (chan_id > span->chan_count) {
		return;
	}

	stream->write_function(stream,
						   " <channel>\n"
						   "  <span-id>%u</span-id>\n"
						   "  <chan-id>%u</chan-id>>\n"
						   "  <physical-span-id>%u</physical-span-id>\n"
						   "  <physical-chan-id>%u</physical-chan-id>\n"
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
						   span->channels[chan_id]->span_id,
						   span->channels[chan_id]->chan_id,
						   span->channels[chan_id]->physical_span_id,
						   span->channels[chan_id]->physical_chan_id,
						   zap_chan_type2str(span->channels[chan_id]->type),
						   zap_channel_state2str(span->channels[chan_id]->state),
						   zap_channel_state2str(span->channels[chan_id]->last_state),
						   span->channels[chan_id]->txgain,
						   span->channels[chan_id]->rxgain,
						   span->channels[chan_id]->caller_data.cid_date,
						   span->channels[chan_id]->caller_data.cid_name,
						   span->channels[chan_id]->caller_data.cid_num.digits,
						   span->channels[chan_id]->caller_data.ani.digits,
						   span->channels[chan_id]->caller_data.aniII,
						   span->channels[chan_id]->caller_data.dnis.digits,
						   span->channels[chan_id]->caller_data.rdnis.digits,
						   switch_channel_cause2str(span->channels[chan_id]->caller_data.hangup_cause)
						   );
}

#define OZ_SYNTAX "list || dump <span_id> [<chan_id>] || q931_pcap <span_id> on|off [pcapfilename without suffix] || gains <txgain> <rxgain> <span_id> [<chan_id>] || dtmf on|off <span_id> [<chan_id>]" 
SWITCH_STANDARD_API(oz_function)
{
	char *mycmd = NULL, *argv[10] = { 0 };
	int argc = 0;

	if (!zstr(cmd) && (mycmd = strdup(cmd))) {
		argc = switch_separate_string(mycmd, ' ', argv, (sizeof(argv) / sizeof(argv[0])));
	}

	if (!argc) {
		stream->write_function(stream, "%s", OZ_SYNTAX);
		goto end;
	}

	if (!strcasecmp(argv[0], "dump")) {
		if (argc < 2) {
			stream->write_function(stream, "-ERR Usage: oz dump <span_id> [<chan_id>]\n");
			goto end;
		} else {
			uint32_t chan_id = 0;
			zap_span_t *span;
			char *as = NULL;
			
			zap_span_find_by_name(argv[1], &span);
			
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
						if(chan_id > span->chan_count) {
							stream->write_function(stream, "<error>invalid channel</error>\n");
						} else {
							dump_chan_xml(span, chan_id, stream);
						}
					} else {
						uint32_t j;
						for (j = 1; j <= span->chan_count; j++) {
							dump_chan_xml(span, j, stream);
						}
						
					}
				}
				stream->write_function(stream, "</channels>\n");
			} else {
				if (!span) {
					stream->write_function(stream, "-ERR invalid span\n");
				} else {
					if (chan_id) {
						if(chan_id > span->chan_count) {
							stream->write_function(stream, "-ERR invalid channel\n");
						} else {
							dump_chan(span, chan_id, stream);
						}
					} else {
						uint32_t j;
						
						stream->write_function(stream, "+OK\n");
						for (j = 1; j <= span->chan_count; j++) {
							dump_chan(span, j, stream);
						}
						
					}
				}
			}
		}
	} else if (!strcasecmp(argv[0], "list")) {
		int j;
		for (j = 0 ; j < ZAP_MAX_SPANS_INTERFACE; j++) {
			if (SPAN_CONFIG[j].span) {
				const char *flags = "none";

				if (SPAN_CONFIG[j].analog_options & ANALOG_OPTION_3WAY) {
					flags = "3way";
				} else if (SPAN_CONFIG[j].analog_options & ANALOG_OPTION_CALL_SWAP) {
					flags = "call swap";
				}

				stream->write_function(stream,
									   "+OK\n"
									   "span: %u (%s)\n"
									   "type: %s\n"
									   "chan_count: %u\n"
									   "dialplan: %s\n"
									   "context: %s\n"
									   "dial_regex: %s\n"
									   "fail_dial_regex: %s\n"
									   "hold_music: %s\n"
									   "analog_options %s\n",
									   j,
									   SPAN_CONFIG[j].span->name,
									   SPAN_CONFIG[j].type,
									   SPAN_CONFIG[j].span->chan_count,
									   SPAN_CONFIG[j].dialplan,
									   SPAN_CONFIG[j].context,
									   SPAN_CONFIG[j].dial_regex,
									   SPAN_CONFIG[j].fail_dial_regex,
									   SPAN_CONFIG[j].hold_music,
									   flags
									   );
			}
		}
	} else if (!strcasecmp(argv[0], "stop") || !strcasecmp(argv[0], "start")) {
		char *span_name = argv[1];
		zap_span_t *span = NULL;
		zap_status_t status;

		if (span_name) {
			zap_span_find_by_name(span_name, &span);
		}

		if (!span) {
			stream->write_function(stream, "-ERR no span\n");
			goto end;
		}
		
		if (!strcasecmp(argv[0], "stop")) {
			status = zap_span_stop(span);
		} else {
			status = zap_span_start(span);
		}
		
		stream->write_function(stream, status == ZAP_SUCCESS ? "+OK\n" : "-ERR failure\n");
		
		goto end;

		/*Q931ToPcap enhancement*/
	} else if (!strcasecmp(argv[0], "q931_pcap")) {
		int32_t span_id = 0;
                zap_span_t *span;
		const char *pcapfn = NULL;
		char *tmp_path = NULL;

                if (argc < 3) {
                        stream->write_function(stream, "-ERR Usage: oz q931_pcap <span_id> on|off [pcapfilename without suffix]\n");
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
			if (zap_configure_span("isdn", span, on_clear_channel_signal, "q931topcap", 1, "pcapfilename", tmp_path, TAG_END) != ZAP_SUCCESS) {
                                zap_log(ZAP_LOG_WARNING, "Error couldn't (re-)enable Q931-To-Pcap!\n");
				goto end;
                        } else {
				stream->write_function(stream, "+OK\n");
			}
		} else if(!strcasecmp(argv[2], "off")) {
			if (zap_configure_span("isdn", span, on_clear_channel_signal, "q931topcap", 0, TAG_END) != ZAP_SUCCESS) {
                                zap_log(ZAP_LOG_ERROR, "Error couldn't enable Q931-To-Pcap!\n");
                                goto end;
			} else {
                                stream->write_function(stream, "+OK\n");
                        }
                } else {
			stream->write_function(stream, "-ERR Usage: oz q931_pcap <span_id> on|off [pcapfilename without suffix]\n");
                        goto end;
		}

	} else if (!strcasecmp(argv[0], "gains")) {
		int i = 0;
		float txgain = 0.0;
		float rxgain = 0.0;
		uint32_t chan_id = 0;
		zap_span_t *span = NULL;
		if (argc < 4) {
			stream->write_function(stream, "-ERR Usage: oz gains <txgain> <rxgain> <span_id> [<chan_id>]\n");
			goto end;
		} 
		zap_span_find_by_name(argv[3], &span);
		if (!span) {
			stream->write_function(stream, "-ERR invalid span\n");
			goto end;
		}
		if (argc > 4) {
			chan_id = atoi(argv[4]);
			if (chan_id > span->chan_count) {
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
			zap_channel_command(span->channels[chan_id], ZAP_COMMAND_SET_RX_GAIN, &rxgain);
			zap_channel_command(span->channels[chan_id], ZAP_COMMAND_SET_TX_GAIN, &txgain);
		} else {
			for (i = 1; i <= (int)span->chan_count; i++) {
				zap_channel_command(span->channels[i], ZAP_COMMAND_SET_RX_GAIN, &rxgain);
				zap_channel_command(span->channels[i], ZAP_COMMAND_SET_TX_GAIN, &txgain);
			}
		}
		stream->write_function(stream, "+OK gains set to Rx %f and Tx %f\n", rxgain, txgain);
	} else if (!strcasecmp(argv[0], "dtmf")) {
		int i = 0;
		uint32_t chan_id = 0;
		zap_span_t *span = NULL;
		zap_command_t zapcmd = ZAP_COMMAND_ENABLE_DTMF_DETECT;
		if (argc < 3) {
			stream->write_function(stream, "-ERR Usage: dtmf on|off <span_id> [<chan_id>]\n");
			goto end;
		}

		if (switch_true(argv[1])) {
			zapcmd = ZAP_COMMAND_ENABLE_DTMF_DETECT;
		} else {
			zapcmd = ZAP_COMMAND_DISABLE_DTMF_DETECT;
		}

		zap_span_find_by_name(argv[2], &span);
		if (!span) {
			stream->write_function(stream, "-ERR invalid span\n");
			goto end;
		}

		if (argc > 3) {
			chan_id = atoi(argv[3]);
			if (chan_id > span->chan_count) {
				stream->write_function(stream, "-ERR invalid chan\n");
				goto end;
			}
		}

		if (chan_id) {
			zap_channel_command(span->channels[chan_id], zapcmd, NULL);
		} else {
			for (i = 1; i <= (int)span->chan_count; i++) {
				zap_channel_command(span->channels[i], zapcmd, NULL);
			}
		}

		stream->write_function(stream, "+OK DTMF detection was %s\n", zapcmd == ZAP_COMMAND_ENABLE_DTMF_DETECT ? "enabled" : "disabled");
	} else if (!strcasecmp(argv[0], "trace")) {
		char tracepath[255];
		int i = 0;
		uint32_t chan_id = 0;
		zap_span_t *span = NULL;
		if (argc < 3) {
			stream->write_function(stream, "-ERR Usage: oz trace <path> <span_id> [<chan_id>]\n");
			goto end;
		} 
		zap_span_find_by_name(argv[2], &span);
		if (!span) {
			stream->write_function(stream, "-ERR invalid span\n");
			goto end;
		}
		if (argc > 3) {
			chan_id = atoi(argv[3]);
			if (chan_id > span->chan_count) {
				stream->write_function(stream, "-ERR invalid chan\n");
				goto end;
			}
		}
		if (chan_id) {
			snprintf(tracepath, sizeof(tracepath), "%s-in-c%d", argv[1], chan_id);
			zap_channel_command(span->channels[chan_id], ZAP_COMMAND_TRACE_INPUT, tracepath);
			snprintf(tracepath, sizeof(tracepath), "%s-out-c%d", argv[1], chan_id);
			zap_channel_command(span->channels[chan_id], ZAP_COMMAND_TRACE_OUTPUT, tracepath);
		} else {
			for (i = 1; i <= (int)span->chan_count; i++) {
				snprintf(tracepath, sizeof(tracepath), "%s-in-c%d", argv[1], i);
				zap_channel_command(span->channels[i], ZAP_COMMAND_TRACE_INPUT, tracepath);
				snprintf(tracepath, sizeof(tracepath), "%s-out-c%d", argv[1], i);
				zap_channel_command(span->channels[i], ZAP_COMMAND_TRACE_OUTPUT, tracepath);
			}
		}
		stream->write_function(stream, "+OK trace enabled with prefix path %s\n", argv[1]);
	} else if (!strcasecmp(argv[0], "notrace")) {
		uint32_t i = 0;
		uint32_t chan_id = 0;
		zap_span_t *span = NULL;
		if (argc < 2) {
			stream->write_function(stream, "-ERR Usage: oz notrace <span_id> [<chan_id>]\n");
			goto end;
		} 
		zap_span_find_by_name(argv[1], &span);
		if (!span) {
			stream->write_function(stream, "-ERR invalid span\n");
			goto end;
		}
		if (argc > 2) {
			chan_id = atoi(argv[2]);
			if (chan_id > span->chan_count) {
				stream->write_function(stream, "-ERR invalid chan\n");
				goto end;
			}
		}
		if (chan_id) {
			zap_channel_command(span->channels[chan_id], ZAP_COMMAND_TRACE_END_ALL, NULL);
		} else {
			for (i = 1; i <= (int)span->chan_count; i++) {
				zap_channel_command(span->channels[i], ZAP_COMMAND_TRACE_END_ALL, NULL);
			}
		}
		stream->write_function(stream, "+OK trace disabled\n");
	} else {
		char *rply = zap_api_execute(cmd, NULL);
		
		if (rply) {
			stream->write_function(stream, "%s", rply);
			free(rply);
		} else {
			stream->write_function(stream, "-ERR Usage: %s\n", OZ_SYNTAX);
		}
	}
	/*Q931ToPcap enhancement done*/

 end:

	switch_safe_free(mycmd);

	return SWITCH_STATUS_SUCCESS;
}


SWITCH_STANDARD_APP(disable_ec_function)
{
	private_t *tech_pvt;
	int x = 0;

	if (!switch_core_session_check_interface(session, openzap_endpoint_interface)) {
		zap_log(ZAP_LOG_ERROR, "This application is only for OpenZAP channels.\n");
		return;
	}
	
	tech_pvt = switch_core_session_get_private(session);

	if (switch_test_flag(tech_pvt, TFLAG_DEAD)) {
        switch_channel_hangup(switch_core_session_get_channel(session), SWITCH_CAUSE_LOSE_RACE);
        return;
    }
	
	zap_channel_command(tech_pvt->zchan, ZAP_COMMAND_DISABLE_ECHOCANCEL, &x);
	zap_channel_command(tech_pvt->zchan, ZAP_COMMAND_DISABLE_ECHOTRAIN, &x);
	zap_log(ZAP_LOG_INFO, "Echo Canceller Disabled\n");
}

SWITCH_STANDARD_APP(disable_dtmf_function)
{
	private_t *tech_pvt;
	if (!switch_core_session_check_interface(session, openzap_endpoint_interface)) {
		zap_log(ZAP_LOG_ERROR, "This application is only for OpenZAP channels.\n");
		return;
	}
	
	tech_pvt = switch_core_session_get_private(session);

	if (switch_test_flag(tech_pvt, TFLAG_DEAD)) {
        	switch_channel_hangup(switch_core_session_get_channel(session), SWITCH_CAUSE_LOSE_RACE);
		return;
	}
	
	zap_channel_command(tech_pvt->zchan, ZAP_COMMAND_DISABLE_DTMF_DETECT, NULL);
	zap_log(ZAP_LOG_INFO, "DTMF detection Disabled in channel %d:%d\n", tech_pvt->zchan->span_id, tech_pvt->zchan->chan_id);
}

SWITCH_STANDARD_APP(enable_dtmf_function)
{
	private_t *tech_pvt;
	if (!switch_core_session_check_interface(session, openzap_endpoint_interface)) {
		zap_log(ZAP_LOG_ERROR, "This application is only for OpenZAP channels.\n");
		return;
	}
	
	tech_pvt = switch_core_session_get_private(session);

	if (switch_test_flag(tech_pvt, TFLAG_DEAD)) {
        	switch_channel_hangup(switch_core_session_get_channel(session), SWITCH_CAUSE_LOSE_RACE);
		return;
	}
	
	zap_channel_command(tech_pvt->zchan, ZAP_COMMAND_ENABLE_DTMF_DETECT, NULL);
	zap_log(ZAP_LOG_INFO, "DTMF detection Enabled in channel %d:%d\n", tech_pvt->zchan->span_id, tech_pvt->zchan->chan_id);
}

SWITCH_MODULE_LOAD_FUNCTION(mod_openzap_load)
{

	switch_api_interface_t *commands_api_interface;
	switch_application_interface_t *app_interface;

	module_pool = pool;

	zap_global_set_logger(zap_logger);
	zap_cpu_monitor_disable();

	if (zap_global_init() != ZAP_SUCCESS) {
		zap_log(ZAP_LOG_ERROR, "Error loading OpenZAP\n");
		return SWITCH_STATUS_TERM;
	}

	if (load_config() != SWITCH_STATUS_SUCCESS) {
		zap_global_destroy();
		return SWITCH_STATUS_TERM;
	}

	*module_interface = switch_loadable_module_create_module_interface(pool, modname);
	openzap_endpoint_interface = switch_loadable_module_create_interface(*module_interface, SWITCH_ENDPOINT_INTERFACE);
	openzap_endpoint_interface->interface_name = "openzap";
	openzap_endpoint_interface->io_routines = &openzap_io_routines;
	openzap_endpoint_interface->state_handler = &openzap_state_handlers;
	
	SWITCH_ADD_API(commands_api_interface, "oz", "OpenZAP commands", oz_function, OZ_SYNTAX);

	SWITCH_ADD_APP(app_interface, "disable_ec", "Disable Echo Canceller", "Disable Echo Canceller", disable_ec_function, "", SAF_NONE);
	SWITCH_ADD_APP(app_interface, "disable_dtmf", "Disable DTMF Detection", "Disable DTMF Detection", disable_dtmf_function, "", SAF_NONE);
	SWITCH_ADD_APP(app_interface, "enable_dtmf", "Enable DTMF Detection", "Enable DTMF Detection", enable_dtmf_function, "", SAF_NONE);

	/* indicate that the module should continue to be loaded */
	return SWITCH_STATUS_SUCCESS;
}

SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_openzap_shutdown)
{
	zap_global_destroy();

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
 * vim:set softtabstop=4 shiftwidth=4 tabstop=4 noet:
 */
