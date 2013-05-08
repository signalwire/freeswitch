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
 * mod_loopback.c -- Loopback Endpoint Module
 *
 */
#include <switch.h>

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>

#define FRAME_QUEUE_LEN 3

SWITCH_MODULE_LOAD_FUNCTION(mod_loopback_load);
SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_loopback_shutdown);
SWITCH_MODULE_DEFINITION(mod_loopback, mod_loopback_load, mod_loopback_shutdown, NULL);

static switch_status_t find_non_loopback_bridge(switch_core_session_t *session, switch_core_session_t **br_session, const char **br_uuid);

static switch_endpoint_interface_t *loopback_endpoint_interface = NULL;

typedef enum {
	TFLAG_LINKED = (1 << 0),
	TFLAG_OUTBOUND = (1 << 1),
	TFLAG_WRITE = (1 << 2),
	TFLAG_USEME = (1 << 3),
	TFLAG_BRIDGE = (1 << 4),
	TFLAG_BOWOUT = (1 << 5),
	TFLAG_BLEG = (1 << 6),
	TFLAG_APP = (1 << 7),
	TFLAG_RUNNING_APP = (1 << 8),
	TFLAG_BOWOUT_USED = (1 << 9),
	TFLAG_CLEAR = (1 << 10)
} TFLAGS;

struct loopback_private_object {
	unsigned int flags;
	switch_mutex_t *flag_mutex;
	switch_mutex_t *mutex;
	switch_core_session_t *session;
	switch_channel_t *channel;
	switch_core_session_t *other_session;
	struct loopback_private_object *other_tech_pvt;
	switch_channel_t *other_channel;
	switch_codec_t read_codec;
	switch_codec_t write_codec;
	switch_frame_t read_frame;
	unsigned char databuf[SWITCH_RECOMMENDED_BUFFER_SIZE];

	switch_frame_t *x_write_frame;
	switch_frame_t *write_frame;
	unsigned char write_databuf[SWITCH_RECOMMENDED_BUFFER_SIZE];

	switch_frame_t cng_frame;
	unsigned char cng_databuf[SWITCH_RECOMMENDED_BUFFER_SIZE];
	switch_timer_t timer;
	switch_caller_profile_t *caller_profile;
	int32_t bowout_frame_count;
	char *other_uuid;
	switch_queue_t *frame_queue;
	int64_t packet_count;
	int first_cng;
};

typedef struct loopback_private_object loopback_private_t;

static struct {
	int debug;
} globals;

static switch_status_t channel_on_init(switch_core_session_t *session);
static switch_status_t channel_on_hangup(switch_core_session_t *session);
static switch_status_t channel_on_destroy(switch_core_session_t *session);
static switch_status_t channel_on_routing(switch_core_session_t *session);
static switch_status_t channel_on_exchange_media(switch_core_session_t *session);
static switch_status_t channel_on_soft_execute(switch_core_session_t *session);
static switch_call_cause_t channel_outgoing_channel(switch_core_session_t *session, switch_event_t *var_event,
													switch_caller_profile_t *outbound_profile,
													switch_core_session_t **new_session, switch_memory_pool_t **pool, switch_originate_flag_t flags,
													switch_call_cause_t *cancel_cause);
static switch_status_t channel_read_frame(switch_core_session_t *session, switch_frame_t **frame, switch_io_flag_t flags, int stream_id);
static switch_status_t channel_write_frame(switch_core_session_t *session, switch_frame_t *frame, switch_io_flag_t flags, int stream_id);
static switch_status_t channel_kill_channel(switch_core_session_t *session, int sig);


static void clear_queue(loopback_private_t *tech_pvt)
{
	void *pop;

	while (switch_queue_trypop(tech_pvt->frame_queue, &pop) == SWITCH_STATUS_SUCCESS && pop) {
		switch_frame_t *frame = (switch_frame_t *) pop;
		switch_frame_free(&frame);
	}

}

static switch_status_t tech_init(loopback_private_t *tech_pvt, switch_core_session_t *session, switch_codec_t *codec)
{
	const char *iananame = "L16";
	uint32_t rate = 8000;
	uint32_t interval = 20;
	switch_status_t status = SWITCH_STATUS_SUCCESS;
	switch_channel_t *channel = switch_core_session_get_channel(session);
	const switch_codec_implementation_t *read_impl;

	if (codec) {
		iananame = codec->implementation->iananame;
		rate = codec->implementation->samples_per_second;
		interval = codec->implementation->microseconds_per_packet / 1000;
	} else {
		const char *var;

		if ((var = switch_channel_get_variable(channel, "loopback_initial_codec"))) {
			char *dup = switch_core_session_strdup(session, var);
			uint32_t bit;
			iananame = switch_parse_codec_buf(dup, &interval, &rate, &bit);
		}
		
	}

	if (switch_core_codec_ready(&tech_pvt->read_codec)) {
		switch_core_codec_destroy(&tech_pvt->read_codec);
	}

	if (switch_core_codec_ready(&tech_pvt->write_codec)) {
		switch_core_codec_destroy(&tech_pvt->write_codec);
	}

	switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "%s setup codec %s/%d/%d\n", switch_channel_get_name(channel), iananame, rate,
					  interval);

	status = switch_core_codec_init(&tech_pvt->read_codec,
									iananame,
									NULL,
									rate, interval, 1, SWITCH_CODEC_FLAG_ENCODE | SWITCH_CODEC_FLAG_DECODE, NULL, switch_core_session_get_pool(session));

	if (status != SWITCH_STATUS_SUCCESS || !tech_pvt->read_codec.implementation || !switch_core_codec_ready(&tech_pvt->read_codec)) {
		goto end;
	}

	status = switch_core_codec_init(&tech_pvt->write_codec,
									iananame,
									NULL,
									rate, interval, 1, SWITCH_CODEC_FLAG_ENCODE | SWITCH_CODEC_FLAG_DECODE, NULL, switch_core_session_get_pool(session));


	if (status != SWITCH_STATUS_SUCCESS) {
		switch_core_codec_destroy(&tech_pvt->read_codec);
		goto end;
	}

	tech_pvt->read_frame.data = tech_pvt->databuf;
	tech_pvt->read_frame.buflen = sizeof(tech_pvt->databuf);
	tech_pvt->read_frame.codec = &tech_pvt->read_codec;


	tech_pvt->cng_frame.data = tech_pvt->cng_databuf;
	tech_pvt->cng_frame.buflen = sizeof(tech_pvt->cng_databuf);

	tech_pvt->cng_frame.datalen = 2;

	tech_pvt->bowout_frame_count = (tech_pvt->read_codec.implementation->actual_samples_per_second /
									tech_pvt->read_codec.implementation->samples_per_packet) * 2;

	switch_core_session_set_read_codec(session, &tech_pvt->read_codec);
	switch_core_session_set_write_codec(session, &tech_pvt->write_codec);

	if (tech_pvt->flag_mutex) {
		switch_core_timer_destroy(&tech_pvt->timer);
	}

	read_impl = tech_pvt->read_codec.implementation;

	switch_core_timer_init(&tech_pvt->timer, "soft",
						   read_impl->microseconds_per_packet / 1000, read_impl->samples_per_packet * 4, switch_core_session_get_pool(session));


	if (!tech_pvt->flag_mutex) {
		switch_mutex_init(&tech_pvt->flag_mutex, SWITCH_MUTEX_NESTED, switch_core_session_get_pool(session));
		switch_mutex_init(&tech_pvt->mutex, SWITCH_MUTEX_NESTED, switch_core_session_get_pool(session));
		switch_core_session_set_private(session, tech_pvt);
		switch_queue_create(&tech_pvt->frame_queue, FRAME_QUEUE_LEN, switch_core_session_get_pool(session));
		tech_pvt->session = session;
		tech_pvt->channel = switch_core_session_get_channel(session);
	}

  end:

	return status;
}

/* 
   State methods they get called when the state changes to the specific state 
   returning SWITCH_STATUS_SUCCESS tells the core to execute the standard state method next
   so if you fully implement the state you can return SWITCH_STATUS_FALSE to skip it.
*/
static switch_status_t channel_on_init(switch_core_session_t *session)
{
	switch_channel_t *channel, *b_channel;
	loopback_private_t *tech_pvt = NULL, *b_tech_pvt = NULL;
	switch_core_session_t *b_session;
	char name[128];
	switch_caller_profile_t *caller_profile;
	switch_event_t *vars = NULL;
	const char *var;

	tech_pvt = switch_core_session_get_private(session);
	switch_assert(tech_pvt != NULL);

	channel = switch_core_session_get_channel(session);
	switch_assert(channel != NULL);



	if (switch_test_flag(tech_pvt, TFLAG_OUTBOUND) && !switch_test_flag(tech_pvt, TFLAG_BLEG)) {

		if (!(b_session = switch_core_session_request(loopback_endpoint_interface, SWITCH_CALL_DIRECTION_INBOUND, SOF_NONE, NULL))) {
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_CRIT, "Failure.\n");
			goto end;
		}

		if (switch_core_session_read_lock(b_session) != SWITCH_STATUS_SUCCESS) {
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_CRIT, "Failure.\n");
			switch_core_session_destroy(&b_session);
			goto end;
		}

		switch_core_session_add_stream(b_session, NULL);
		b_channel = switch_core_session_get_channel(b_session);
		b_tech_pvt = (loopback_private_t *) switch_core_session_alloc(b_session, sizeof(*b_tech_pvt));

		switch_snprintf(name, sizeof(name), "loopback/%s-b", tech_pvt->caller_profile->destination_number);
		switch_channel_set_name(b_channel, name);
		if (tech_init(b_tech_pvt, b_session, switch_core_session_get_read_codec(session)) != SWITCH_STATUS_SUCCESS) {
			switch_channel_hangup(channel, SWITCH_CAUSE_DESTINATION_OUT_OF_ORDER);
			switch_core_session_destroy(&b_session);
			goto end;
		}

		caller_profile = switch_caller_profile_clone(b_session, tech_pvt->caller_profile);
		caller_profile->source = switch_core_strdup(caller_profile->pool, modname);
		switch_channel_set_caller_profile(b_channel, caller_profile);
		b_tech_pvt->caller_profile = caller_profile;
		switch_channel_set_state(b_channel, CS_INIT);

		switch_mutex_lock(tech_pvt->mutex);
		tech_pvt->other_session = b_session;
		tech_pvt->other_tech_pvt = b_tech_pvt;
		tech_pvt->other_channel = b_channel;
		switch_mutex_unlock(tech_pvt->mutex);

		//b_tech_pvt->other_session = session;
		//b_tech_pvt->other_tech_pvt = tech_pvt;
		//b_tech_pvt->other_channel = channel;

		b_tech_pvt->other_uuid = switch_core_session_strdup(b_session, switch_core_session_get_uuid(session));

		switch_set_flag_locked(tech_pvt, TFLAG_LINKED);
		switch_set_flag_locked(b_tech_pvt, TFLAG_LINKED);
		switch_set_flag_locked(b_tech_pvt, TFLAG_BLEG);


		switch_channel_set_flag(channel, CF_ACCEPT_CNG);

		if ((vars = (switch_event_t *) switch_channel_get_private(channel, "__loopback_vars__"))) {
			switch_event_header_t *h;
		
			switch_channel_set_private(channel, "__loopback_vars__", NULL);

			for (h = vars->headers; h; h = h->next) {
				switch_channel_set_variable(tech_pvt->other_channel, h->name, h->value);
			}

			switch_event_destroy(&vars);
		}

		if ((var = switch_channel_get_variable(channel, "loopback_export"))) {
			int argc = 0;
			char *argv[128] = { 0 };
			char *dup = switch_core_session_strdup(session, var);

			if ((argc = switch_split(dup, ',', argv))) {
				int i;
				for (i = 0; i < argc; i++) {
					
					if (!zstr(argv[i])) {
						const char *val = switch_channel_get_variable(channel, argv[i]);

						if(!zstr(val)) {
							switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "Transfer variable [%s]=[%s] %s -> %s\n",
											  argv[i], val, switch_channel_get_name(channel), switch_channel_get_name(tech_pvt->other_channel));
											  
							switch_channel_set_variable(tech_pvt->other_channel, argv[i], val);
						}
					}
				}
			}
		}

		if (switch_test_flag(tech_pvt, TFLAG_APP)) {
			switch_set_flag(b_tech_pvt, TFLAG_APP);
			switch_clear_flag(tech_pvt, TFLAG_APP);
		}

		switch_channel_set_variable(channel, "other_loopback_leg_uuid", switch_channel_get_uuid(b_channel));
		switch_channel_set_variable(b_channel, "other_loopback_leg_uuid", switch_channel_get_uuid(channel));

		if (switch_core_session_thread_launch(b_session) != SWITCH_STATUS_SUCCESS) {
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_CRIT, "Error spawning thread\n");
			switch_channel_hangup(channel, SWITCH_CAUSE_DESTINATION_OUT_OF_ORDER);
			goto end;
		}
	} else { 
		switch_mutex_lock(tech_pvt->mutex);
		if ((tech_pvt->other_session = switch_core_session_locate(tech_pvt->other_uuid))) {
			tech_pvt->other_tech_pvt = switch_core_session_get_private(tech_pvt->other_session);
			tech_pvt->other_channel = switch_core_session_get_channel(tech_pvt->other_session);
		}
		switch_mutex_unlock(tech_pvt->mutex);
	}

	if (!tech_pvt->other_session) {
		switch_clear_flag_locked(tech_pvt, TFLAG_LINKED);
		switch_channel_hangup(channel, SWITCH_CAUSE_DESTINATION_OUT_OF_ORDER);
		goto end;
	}

	switch_channel_set_variable(channel, "loopback_leg", switch_test_flag(tech_pvt, TFLAG_BLEG) ? "B" : "A");
	switch_channel_set_state(channel, CS_ROUTING);

  end:

	return SWITCH_STATUS_SUCCESS;
}

static void do_reset(loopback_private_t *tech_pvt)
{
	switch_clear_flag_locked(tech_pvt, TFLAG_WRITE);

	switch_mutex_lock(tech_pvt->mutex);
	if (tech_pvt->other_tech_pvt) {
		switch_clear_flag_locked(tech_pvt->other_tech_pvt, TFLAG_WRITE);
	}
	switch_mutex_unlock(tech_pvt->mutex);
}

static switch_status_t channel_on_routing(switch_core_session_t *session)
{
	switch_channel_t *channel = NULL;
	loopback_private_t *tech_pvt = NULL;
	const char *app, *arg;

	channel = switch_core_session_get_channel(session);
	assert(channel != NULL);

	tech_pvt = switch_core_session_get_private(session);
	assert(tech_pvt != NULL);

	do_reset(tech_pvt);

	switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "%s CHANNEL ROUTING\n", switch_channel_get_name(channel));
	
	if (switch_test_flag(tech_pvt, TFLAG_RUNNING_APP)) {
		switch_clear_flag(tech_pvt, TFLAG_RUNNING_APP);
	}

	if (switch_test_flag(tech_pvt, TFLAG_APP) && !switch_test_flag(tech_pvt, TFLAG_OUTBOUND) && 
		(app = switch_channel_get_variable(channel, "loopback_app"))) {
		switch_caller_extension_t *extension = NULL;

		switch_clear_flag(tech_pvt, TFLAG_APP);
		switch_set_flag(tech_pvt, TFLAG_RUNNING_APP);

		arg = switch_channel_get_variable(channel, "loopback_app_arg");
		extension = switch_caller_extension_new(session, app, app);
		switch_caller_extension_add_application(session, extension, "pre_answer", NULL);
		switch_caller_extension_add_application(session, extension, app, arg);

		switch_channel_set_caller_extension(channel, extension);
		switch_channel_set_state(channel, CS_EXECUTE);
		return SWITCH_STATUS_FALSE;
	}



	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t channel_on_execute(switch_core_session_t *session)
{
	switch_channel_t *channel = NULL;
	loopback_private_t *tech_pvt = NULL;
	switch_caller_extension_t *exten;
	int bow = 0;

	channel = switch_core_session_get_channel(session);
	assert(channel != NULL);

	tech_pvt = switch_core_session_get_private(session);
	assert(tech_pvt != NULL);

	switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "%s CHANNEL EXECUTE\n", switch_channel_get_name(channel));


	if ((exten = switch_channel_get_caller_extension(channel))) {
		switch_caller_application_t *app_p;

		for (app_p = exten->applications; app_p; app_p = app_p->next) {
			int32_t flags;

			switch_core_session_get_app_flags(app_p->application_name, &flags);

			if ((flags & SAF_NO_LOOPBACK)) {
				bow = 1;
				break;
			}
		}
	}

	if (bow) {
		switch_core_session_t *other_session;
		const char *other_uuid;

		if ((find_non_loopback_bridge(tech_pvt->other_session, &other_session, &other_uuid) == SWITCH_STATUS_SUCCESS)) {
			switch_caller_extension_t *extension;
			switch_channel_t *other_channel = switch_core_session_get_channel(other_session);
			switch_caller_extension_clone(&extension, exten, switch_core_session_get_pool(other_session));
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(tech_pvt->session), SWITCH_LOG_INFO, "BOWOUT Transfering current extension to non-loopback leg.\n");
			switch_channel_transfer_to_extension(other_channel, extension);
			switch_core_session_rwunlock(other_session);
		}

	}

	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t channel_on_destroy(switch_core_session_t *session)
{
	switch_channel_t *channel = NULL;
	loopback_private_t *tech_pvt = NULL;
	void *pop;
	switch_event_t *vars;

	channel = switch_core_session_get_channel(session);
	switch_assert(channel != NULL);

	tech_pvt = switch_core_session_get_private(session);

	if ((vars = (switch_event_t *) switch_channel_get_private(channel, "__loopback_vars__"))) {
		switch_channel_set_private(channel, "__loopback_vars__", NULL);
		switch_event_destroy(&vars);
	}
	
	if (tech_pvt) {
		switch_core_timer_destroy(&tech_pvt->timer);

		if (switch_core_codec_ready(&tech_pvt->read_codec)) {
			switch_core_codec_destroy(&tech_pvt->read_codec);
		}

		if (switch_core_codec_ready(&tech_pvt->write_codec)) {
			switch_core_codec_destroy(&tech_pvt->write_codec);
		}

		if (tech_pvt->write_frame) {
			switch_frame_free(&tech_pvt->write_frame);
		}

		while (switch_queue_trypop(tech_pvt->frame_queue, &pop) == SWITCH_STATUS_SUCCESS && pop) {
			switch_frame_t *frame = (switch_frame_t *) pop;
			switch_frame_free(&frame);
		}
	}


	return SWITCH_STATUS_SUCCESS;
}


static switch_status_t channel_on_hangup(switch_core_session_t *session)
{
	switch_channel_t *channel = NULL;
	loopback_private_t *tech_pvt = NULL;

	channel = switch_core_session_get_channel(session);
	switch_assert(channel != NULL);

	tech_pvt = switch_core_session_get_private(session);
	switch_assert(tech_pvt != NULL);
	switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "%s CHANNEL HANGUP\n", switch_channel_get_name(channel));

	switch_clear_flag_locked(tech_pvt, TFLAG_LINKED);

	switch_mutex_lock(tech_pvt->mutex);

	if (tech_pvt->other_tech_pvt) {
		switch_clear_flag_locked(tech_pvt->other_tech_pvt, TFLAG_LINKED);
		if (tech_pvt->other_tech_pvt->session && tech_pvt->other_tech_pvt->session != tech_pvt->other_session) {
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_CRIT, "OTHER SESSION MISMATCH????\n");
			tech_pvt->other_session = tech_pvt->other_tech_pvt->session;
		}
		tech_pvt->other_tech_pvt = NULL;
	}

	if (tech_pvt->other_session) {
		switch_channel_hangup(tech_pvt->other_channel, switch_channel_get_cause(channel));
		switch_core_session_rwunlock(tech_pvt->other_session);
		tech_pvt->other_channel = NULL;
		tech_pvt->other_session = NULL;
	}
	switch_mutex_unlock(tech_pvt->mutex);

	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t channel_kill_channel(switch_core_session_t *session, int sig)
{
	switch_channel_t *channel = NULL;
	loopback_private_t *tech_pvt = NULL;

	channel = switch_core_session_get_channel(session);
	switch_assert(channel != NULL);

	tech_pvt = switch_core_session_get_private(session);
	switch_assert(tech_pvt != NULL);

	switch (sig) {
	case SWITCH_SIG_BREAK:
		break;
	case SWITCH_SIG_KILL:
		switch_channel_hangup(channel, SWITCH_CAUSE_NORMAL_CLEARING);
		switch_clear_flag_locked(tech_pvt, TFLAG_LINKED);
		switch_mutex_lock(tech_pvt->mutex);
		if (tech_pvt->other_tech_pvt) {
			switch_clear_flag_locked(tech_pvt->other_tech_pvt, TFLAG_LINKED);
		}
		switch_mutex_unlock(tech_pvt->mutex);
		break;
	default:
		break;
	}

	switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "%s CHANNEL KILL\n", switch_channel_get_name(channel));

	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t channel_on_soft_execute(switch_core_session_t *session)
{
	switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "CHANNEL TRANSMIT\n");
	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t channel_on_exchange_media(switch_core_session_t *session)
{
	switch_channel_t *channel = NULL;
	loopback_private_t *tech_pvt = NULL;

	channel = switch_core_session_get_channel(session);
	assert(channel != NULL);

	tech_pvt = switch_core_session_get_private(session);
	assert(tech_pvt != NULL);

	switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "CHANNEL LOOPBACK\n");

	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t channel_on_reset(switch_core_session_t *session)
{
	loopback_private_t *tech_pvt = (loopback_private_t *) switch_core_session_get_private(session);
	switch_assert(tech_pvt != NULL);

	do_reset(tech_pvt);
	switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "%s RESET\n",
					  switch_channel_get_name(switch_core_session_get_channel(session)));

	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t channel_on_hibernate(switch_core_session_t *session)
{
	switch_assert(switch_core_session_get_private(session));

	switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "%s HIBERNATE\n",
					  switch_channel_get_name(switch_core_session_get_channel(session)));

	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t channel_on_consume_media(switch_core_session_t *session)
{
	switch_channel_t *channel = NULL;
	loopback_private_t *tech_pvt = NULL;

	channel = switch_core_session_get_channel(session);
	assert(channel != NULL);

	tech_pvt = switch_core_session_get_private(session);
	assert(tech_pvt != NULL);

	switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "CHANNEL CONSUME_MEDIA\n");

	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t channel_send_dtmf(switch_core_session_t *session, const switch_dtmf_t *dtmf)
{
	loopback_private_t *tech_pvt = NULL;

	tech_pvt = switch_core_session_get_private(session);
	switch_assert(tech_pvt != NULL);

	if (tech_pvt->other_channel) {
		switch_channel_queue_dtmf(tech_pvt->other_channel, dtmf);
	}

	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t channel_read_frame(switch_core_session_t *session, switch_frame_t **frame, switch_io_flag_t flags, int stream_id)
{
	switch_channel_t *channel = NULL;
	loopback_private_t *tech_pvt = NULL;
	switch_status_t status = SWITCH_STATUS_FALSE;
	switch_mutex_t *mutex = NULL;
	void *pop = NULL;

	channel = switch_core_session_get_channel(session);
	switch_assert(channel != NULL);

	tech_pvt = switch_core_session_get_private(session);
	switch_assert(tech_pvt != NULL);

	if (!switch_test_flag(tech_pvt, TFLAG_LINKED)) {
		goto end;
	}

	*frame = NULL;

	if (!switch_channel_ready(channel)) {
		goto end;
	}

	switch_core_timer_next(&tech_pvt->timer);

	mutex = tech_pvt->mutex;
	switch_mutex_lock(mutex);


	if (switch_test_flag(tech_pvt, TFLAG_CLEAR)) {
		clear_queue(tech_pvt);
		switch_clear_flag(tech_pvt, TFLAG_CLEAR);
	}

	if (switch_queue_trypop(tech_pvt->frame_queue, &pop) == SWITCH_STATUS_SUCCESS && pop) {
		if (tech_pvt->write_frame) {
			switch_frame_free(&tech_pvt->write_frame);
		}
		
		tech_pvt->write_frame = (switch_frame_t *) pop;
		tech_pvt->write_frame->codec = &tech_pvt->read_codec;
		*frame = tech_pvt->write_frame;
		tech_pvt->packet_count++;
		switch_clear_flag(tech_pvt->write_frame, SFF_CNG);
		tech_pvt->first_cng = 0;
	} else {
		*frame = &tech_pvt->cng_frame;
		tech_pvt->cng_frame.codec = &tech_pvt->read_codec;
		tech_pvt->cng_frame.datalen = tech_pvt->read_codec.implementation->decoded_bytes_per_packet;
		switch_set_flag((&tech_pvt->cng_frame), SFF_CNG);
		if (!tech_pvt->first_cng) {
			switch_yield(tech_pvt->read_codec.implementation->samples_per_packet);
			tech_pvt->first_cng = 1;
		}
	}


	if (*frame) {
		status = SWITCH_STATUS_SUCCESS;
	} else {
		status = SWITCH_STATUS_FALSE;
	}

  end:

	if (mutex) {
		switch_mutex_unlock(mutex);
	}

	return status;
}

static void switch_channel_wait_for_state_or_greater(switch_channel_t *channel, switch_channel_t *other_channel, switch_channel_state_t want_state)
{

	switch_assert(channel);
	
	for (;;) {
		if ((switch_channel_get_state(channel) < CS_HANGUP && 
			 switch_channel_get_state(channel) == switch_channel_get_running_state(channel) && switch_channel_get_running_state(channel) >= want_state) ||
			(other_channel && switch_channel_down_nosig(other_channel)) || switch_channel_down(channel)) {
			break;
		}
		switch_cond_next();
	}
}


static switch_status_t find_non_loopback_bridge(switch_core_session_t *session, switch_core_session_t **br_session, const char **br_uuid)
{
	switch_channel_t *channel = switch_core_session_get_channel(session);
	const char *a_uuid = NULL;
	switch_core_session_t *sp;

	*br_session = NULL;
	*br_uuid = NULL;

	a_uuid = switch_channel_get_partner_uuid(channel);

	while (a_uuid && (sp = switch_core_session_locate(a_uuid))) {
		if (switch_core_session_check_interface(sp, loopback_endpoint_interface)) {
			loopback_private_t *tech_pvt;
			switch_channel_t *spchan = switch_core_session_get_channel(sp);

			switch_channel_wait_for_state_or_greater(spchan, channel, CS_ROUTING);
			
			tech_pvt = switch_core_session_get_private(sp);

			if (tech_pvt->other_channel) {
				a_uuid = switch_channel_get_partner_uuid(tech_pvt->other_channel);
			}

			switch_core_session_rwunlock(sp);
			sp = NULL;
		} else {
			break;
		}
	}

	if (sp) {
		*br_session = sp;
		*br_uuid = a_uuid;
		return SWITCH_STATUS_SUCCESS;
	}

	return SWITCH_STATUS_FALSE;

}

static switch_status_t channel_write_frame(switch_core_session_t *session, switch_frame_t *frame, switch_io_flag_t flags, int stream_id)
{
	switch_channel_t *channel = NULL;
	loopback_private_t *tech_pvt = NULL;
	switch_status_t status = SWITCH_STATUS_FALSE;

	channel = switch_core_session_get_channel(session);
	switch_assert(channel != NULL);

	tech_pvt = switch_core_session_get_private(session);
	switch_assert(tech_pvt != NULL);

	if (switch_test_flag(frame, SFF_CNG) || 
		(switch_test_flag(tech_pvt, TFLAG_BOWOUT) && switch_test_flag(tech_pvt, TFLAG_BOWOUT_USED))) {
		switch_core_timer_sync(&tech_pvt->timer);
		switch_core_timer_sync(&tech_pvt->other_tech_pvt->timer);
		return SWITCH_STATUS_SUCCESS;
	}

	switch_mutex_lock(tech_pvt->mutex);
	if (!switch_test_flag(tech_pvt, TFLAG_BOWOUT) &&
		tech_pvt->other_tech_pvt &&
		switch_test_flag(tech_pvt, TFLAG_BRIDGE) &&
		!switch_test_flag(tech_pvt, TFLAG_BLEG) &&
		switch_test_flag(tech_pvt->other_tech_pvt, TFLAG_BRIDGE) &&
		switch_channel_test_flag(tech_pvt->channel, CF_BRIDGED) &&
		switch_channel_test_flag(tech_pvt->other_channel, CF_BRIDGED) &&
		switch_channel_test_flag(tech_pvt->channel, CF_ANSWERED) &&
		switch_channel_test_flag(tech_pvt->other_channel, CF_ANSWERED) && --tech_pvt->bowout_frame_count <= 0) {
		const char *a_uuid = NULL;
		const char *b_uuid = NULL;
		const char *vetoa, *vetob;


		vetoa = switch_channel_get_variable(tech_pvt->channel, "loopback_bowout");
		vetob = switch_channel_get_variable(tech_pvt->other_tech_pvt->channel, "loopback_bowout");

		if ((!vetoa || switch_true(vetoa)) && (!vetob || switch_true(vetob))) {
			switch_core_session_t *br_a, *br_b;
			switch_channel_t *ch_a = NULL, *ch_b = NULL;
			int good_to_go = 0;

			switch_mutex_unlock(tech_pvt->mutex);
			find_non_loopback_bridge(session, &br_a, &a_uuid);
			find_non_loopback_bridge(tech_pvt->other_session, &br_b, &b_uuid);
			switch_mutex_lock(tech_pvt->mutex);

			
			if (br_a) {
				ch_a = switch_core_session_get_channel(br_a);
				switch_core_media_bug_transfer_recordings(session, br_a);
			}

			if (br_b) {
				ch_b = switch_core_session_get_channel(br_b);
				switch_core_media_bug_transfer_recordings(tech_pvt->other_session, br_b);
			}
			
			if (ch_a && ch_b && switch_channel_test_flag(ch_a, CF_BRIDGED) && switch_channel_test_flag(ch_b, CF_BRIDGED)) {

				switch_set_flag_locked(tech_pvt, TFLAG_BOWOUT);
				switch_set_flag_locked(tech_pvt->other_tech_pvt, TFLAG_BOWOUT);

				switch_clear_flag_locked(tech_pvt, TFLAG_WRITE);
				switch_clear_flag_locked(tech_pvt->other_tech_pvt, TFLAG_WRITE);

				switch_set_flag_locked(tech_pvt, TFLAG_BOWOUT_USED);
				switch_set_flag_locked(tech_pvt->other_tech_pvt, TFLAG_BOWOUT_USED);

				if (a_uuid && b_uuid) {
					switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG,
									  "%s detected bridge on both ends, attempting direct connection.\n", switch_channel_get_name(channel));
					
					/* channel_masquerade eat your heart out....... */
					switch_ivr_uuid_bridge(a_uuid, b_uuid);
					good_to_go = 1;
					switch_mutex_unlock(tech_pvt->mutex);
				}
			}

			if (br_a) switch_core_session_rwunlock(br_a);
			if (br_b) switch_core_session_rwunlock(br_b);
			
			if (good_to_go) {
				return SWITCH_STATUS_SUCCESS;
			}

		}
	}

	if (switch_test_flag(tech_pvt, TFLAG_LINKED) && tech_pvt->other_tech_pvt) {
		switch_frame_t *clone;
		
		if (frame->codec->implementation != tech_pvt->write_codec.implementation) {
			/* change codecs to match */
			tech_init(tech_pvt, session, frame->codec);
			tech_init(tech_pvt->other_tech_pvt, tech_pvt->other_session, frame->codec);
		}


		if (switch_frame_dup(frame, &clone) != SWITCH_STATUS_SUCCESS) {
			abort();
		}
		
		if ((status = switch_queue_trypush(tech_pvt->other_tech_pvt->frame_queue, clone)) != SWITCH_STATUS_SUCCESS) { 
			clear_queue(tech_pvt->other_tech_pvt);
			status = switch_queue_trypush(tech_pvt->other_tech_pvt->frame_queue, clone);
		}

		if (status == SWITCH_STATUS_SUCCESS) {
			switch_set_flag_locked(tech_pvt->other_tech_pvt, TFLAG_WRITE);
		} else {
			switch_frame_free(&clone);
		}

		status = SWITCH_STATUS_SUCCESS;
	}

	switch_mutex_unlock(tech_pvt->mutex);

	return status;
}

static switch_status_t channel_receive_message(switch_core_session_t *session, switch_core_session_message_t *msg)
{
	switch_channel_t *channel;
	loopback_private_t *tech_pvt;
	int done = 1, pass = 0;
	
	channel = switch_core_session_get_channel(session);
	switch_assert(channel != NULL);

	tech_pvt = switch_core_session_get_private(session);
	switch_assert(tech_pvt != NULL);

	switch (msg->message_id) {
	case SWITCH_MESSAGE_INDICATE_ANSWER:
		if (tech_pvt->other_channel && !switch_test_flag(tech_pvt, TFLAG_OUTBOUND)) {
			switch_channel_mark_answered(tech_pvt->other_channel);
		}
		break;
	case SWITCH_MESSAGE_INDICATE_PROGRESS:
		if (tech_pvt->other_channel && !switch_test_flag(tech_pvt, TFLAG_OUTBOUND)) {
			switch_channel_mark_pre_answered(tech_pvt->other_channel);
		}
		break;
	case SWITCH_MESSAGE_INDICATE_RINGING:
		if (tech_pvt->other_channel && !switch_test_flag(tech_pvt, TFLAG_OUTBOUND)) {
			switch_channel_mark_ring_ready(tech_pvt->other_channel);
		}
		break;
	case SWITCH_MESSAGE_INDICATE_BRIDGE:
		{
			switch_set_flag_locked(tech_pvt, TFLAG_BRIDGE);
		}
		break;
	case SWITCH_MESSAGE_INDICATE_UNBRIDGE:
		{
			switch_clear_flag_locked(tech_pvt, TFLAG_BRIDGE);
		}
		break;
	default:
		done = 0;
		break;
	}

	switch (msg->message_id) {
	case SWITCH_MESSAGE_INDICATE_BRIDGE:
	case SWITCH_MESSAGE_INDICATE_UNBRIDGE:
	case SWITCH_MESSAGE_INDICATE_AUDIO_SYNC:
		{

			done = 1;
			switch_set_flag(tech_pvt, TFLAG_CLEAR);
			switch_set_flag(tech_pvt->other_tech_pvt, TFLAG_CLEAR);

			switch_core_timer_sync(&tech_pvt->timer);
			switch_core_timer_sync(&tech_pvt->other_tech_pvt->timer);
		}
		break;
	default:
		break;
	}


	switch (msg->message_id) {
	case SWITCH_MESSAGE_INDICATE_DISPLAY:
		if (tech_pvt->other_channel) {

			if (switch_test_flag(tech_pvt, TFLAG_BLEG)) {
				if (!zstr(msg->string_array_arg[0])) {
					switch_channel_set_profile_var(tech_pvt->other_channel, "caller_id_name", msg->string_array_arg[0]);
				}
				
				if (!zstr(msg->string_array_arg[1])) {
					switch_channel_set_profile_var(tech_pvt->other_channel, "caller_id_number", msg->string_array_arg[1]);
				}
			} else {
				if (!zstr(msg->string_array_arg[0])) {
					switch_channel_set_profile_var(tech_pvt->other_channel, "callee_id_name", msg->string_array_arg[0]);
				}
				
				if (!zstr(msg->string_array_arg[1])) {
					switch_channel_set_profile_var(tech_pvt->other_channel, "callee_id_number", msg->string_array_arg[1]);
				}
			}
			
			pass = 1;
		}
		break;
	default:
		break;
	}


	if (!done && tech_pvt->other_session && (pass || switch_test_flag(tech_pvt, TFLAG_RUNNING_APP))) {
		switch_status_t r = SWITCH_STATUS_FALSE;
		switch_core_session_t *other_session;
		
		if (switch_core_session_get_partner(tech_pvt->other_session, &other_session) == SWITCH_STATUS_SUCCESS) {
			r = switch_core_session_receive_message(other_session, msg);
			switch_core_session_rwunlock(other_session);
		}
		
		return r;
	}
	
	return SWITCH_STATUS_SUCCESS;
}


static switch_status_t loopback_bowout_on_execute_state_handler(switch_core_session_t *session)
{
	switch_channel_t *channel = switch_core_session_get_channel(session);
	switch_channel_state_t state = switch_channel_get_state(channel);
	loopback_private_t *tech_pvt = NULL;


	if (state == CS_EXECUTE) {
		const char *uuid;
		switch_core_session_t *other_session = NULL;
		switch_channel_t *b_channel = NULL;

		tech_pvt = switch_core_session_get_private(session);

		if (switch_core_session_read_lock(tech_pvt->other_session) == SWITCH_STATUS_SUCCESS) {
			b_channel = switch_core_session_get_channel(tech_pvt->other_session);

			/* Wait for b_channel to be fully bridged */
			switch_channel_wait_for_flag(b_channel, CF_BRIDGED, SWITCH_TRUE, 5000, NULL);

			uuid = switch_channel_get_partner_uuid(b_channel);

			if (uuid && (other_session = switch_core_session_locate(uuid))) {
				switch_channel_t *other_channel = switch_core_session_get_channel(other_session);
				switch_caller_profile_t *cp, *clone;

				switch_channel_wait_for_state(other_channel, NULL, CS_EXCHANGE_MEDIA);

				switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(tech_pvt->other_session), SWITCH_LOG_INFO, "Replacing loopback channel: %s with real channel: %s\n",
								  switch_channel_get_name(b_channel), switch_channel_get_name(other_channel));

				if ((cp = switch_channel_get_caller_profile(channel))) {
					clone = switch_caller_profile_clone(other_session, cp);
					clone->originator_caller_profile = NULL;
					clone->originatee_caller_profile = NULL;
					switch_channel_set_caller_profile(other_channel, clone);
				}

				switch_channel_caller_extension_masquerade(channel, other_channel, 0);
				switch_channel_set_state(other_channel, CS_RESET);
				switch_channel_wait_for_state(other_channel, NULL, CS_RESET);
				switch_channel_set_variable(channel, "process_cdr", "false");
				switch_channel_set_variable(b_channel, "process_cdr", "false");
				switch_channel_hangup(channel, SWITCH_CAUSE_NORMAL_CLEARING);
				switch_channel_set_state(other_channel, CS_EXECUTE);
				switch_core_session_rwunlock(other_session);
			}

			switch_core_session_rwunlock(tech_pvt->other_session);
		}
		
		switch_core_event_hook_remove_state_change(session, loopback_bowout_on_execute_state_handler);
	}

	return SWITCH_STATUS_SUCCESS;
}

static switch_call_cause_t channel_outgoing_channel(switch_core_session_t *session, switch_event_t *var_event,
													switch_caller_profile_t *outbound_profile,
													switch_core_session_t **new_session, switch_memory_pool_t **pool, switch_originate_flag_t flags,
													switch_call_cause_t *cancel_cause)
{
	char name[128];
	switch_channel_t *ochannel = NULL;

	if (session) {
		ochannel = switch_core_session_get_channel(session);
		switch_channel_clear_flag(ochannel, CF_PROXY_MEDIA);
		switch_channel_clear_flag(ochannel, CF_PROXY_MODE);
		switch_channel_pre_answer(ochannel);
	}

	if ((*new_session = switch_core_session_request(loopback_endpoint_interface, SWITCH_CALL_DIRECTION_OUTBOUND, flags, pool)) != 0) {
		loopback_private_t *tech_pvt;
		switch_channel_t *channel;
		switch_caller_profile_t *caller_profile;
		switch_event_t *clone = NULL;

		switch_core_session_add_stream(*new_session, NULL);

		if ((tech_pvt = (loopback_private_t *) switch_core_session_alloc(*new_session, sizeof(loopback_private_t))) != 0) {
			channel = switch_core_session_get_channel(*new_session);
			switch_snprintf(name, sizeof(name), "loopback/%s-a", outbound_profile->destination_number);
			switch_channel_set_name(channel, name);
			if (tech_init(tech_pvt, *new_session, session ? switch_core_session_get_read_codec(session) : NULL) != SWITCH_STATUS_SUCCESS) {
				switch_core_session_destroy(new_session);
				return SWITCH_CAUSE_DESTINATION_OUT_OF_ORDER;
			}
		} else {
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(*new_session), SWITCH_LOG_CRIT, "Hey where is my memory pool?\n");
			switch_core_session_destroy(new_session);
			return SWITCH_CAUSE_DESTINATION_OUT_OF_ORDER;
		}

		if (switch_event_dup(&clone, var_event) == SWITCH_STATUS_SUCCESS) {
			switch_channel_set_private(channel, "__loopback_vars__", clone);
		}

		if (outbound_profile) {
			char *dialplan = NULL, *context = NULL;

			caller_profile = switch_caller_profile_clone(*new_session, outbound_profile);
			caller_profile->source = switch_core_strdup(caller_profile->pool, modname);
			if (!strncasecmp(caller_profile->destination_number, "app=", 4)) {
				char *dest = switch_core_session_strdup(*new_session, caller_profile->destination_number);
				char *app = dest + 4;
				char *arg = NULL;

				if ((arg = strchr(app, ':'))) {
					*arg++ = '\0';
				}

				switch_channel_set_variable(channel, "loopback_app", app);

				if (clone) {
					switch_event_add_header_string(clone, SWITCH_STACK_BOTTOM, "loopback_app", app);
				}
				
				if (arg) {
					switch_channel_set_variable(channel, "loopback_app_arg", arg);
					if (clone) {
						switch_event_add_header_string(clone, SWITCH_STACK_BOTTOM, "loopback_app_arg", arg);
					}
				}

				switch_set_flag(tech_pvt, TFLAG_APP);

				caller_profile->destination_number = switch_core_strdup(caller_profile->pool, app);
			}

			if ((context = strchr(caller_profile->destination_number, '/'))) {
				*context++ = '\0';

				if ((dialplan = strchr(context, '/'))) {
					*dialplan++ = '\0';
				}

				if (!zstr(context)) {
					caller_profile->context = switch_core_strdup(caller_profile->pool, context);
				}

				if (!zstr(dialplan)) {
					caller_profile->dialplan = switch_core_strdup(caller_profile->pool, dialplan);
				}
			}

			if (zstr(caller_profile->context)) {
				caller_profile->context = switch_core_strdup(caller_profile->pool, "default");
			}

			if (zstr(caller_profile->dialplan)) {
				caller_profile->dialplan = switch_core_strdup(caller_profile->pool, "xml");
			}

			switch_snprintf(name, sizeof(name), "loopback/%s-a", caller_profile->destination_number);
			switch_channel_set_name(channel, name);
			switch_set_flag_locked(tech_pvt, TFLAG_OUTBOUND);
			switch_channel_set_caller_profile(channel, caller_profile);
			tech_pvt->caller_profile = caller_profile;
		} else {
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(*new_session), SWITCH_LOG_ERROR, "Doh! no caller profile\n");
			switch_core_session_destroy(new_session);
			return SWITCH_CAUSE_DESTINATION_OUT_OF_ORDER;
		}

		if (switch_true(switch_event_get_header(var_event, "loopback_bowout_on_execute"))) {
			switch_core_event_hook_add_state_change(*new_session, loopback_bowout_on_execute_state_handler);
		}

		switch_channel_set_state(channel, CS_INIT);

		return SWITCH_CAUSE_SUCCESS;
	}

	return SWITCH_CAUSE_DESTINATION_OUT_OF_ORDER;
}

static switch_state_handler_table_t channel_event_handlers = {
	/*.on_init */ channel_on_init,
	/*.on_routing */ channel_on_routing,
	/*.on_execute */ channel_on_execute,
	/*.on_hangup */ channel_on_hangup,
	/*.on_exchange_media */ channel_on_exchange_media,
	/*.on_soft_execute */ channel_on_soft_execute,
	/*.on_consume_media */ channel_on_consume_media,
	/*.on_hibernate */ channel_on_hibernate,
	/*.on_reset */ channel_on_reset,
	/*.on_park */ NULL,
	/*.on_reporting */ NULL,
	/*.on_destroy */ channel_on_destroy
};

static switch_io_routines_t channel_io_routines = {
	/*.outgoing_channel */ channel_outgoing_channel,
	/*.read_frame */ channel_read_frame,
	/*.write_frame */ channel_write_frame,
	/*.kill_channel */ channel_kill_channel,
	/*.send_dtmf */ channel_send_dtmf,
	/*.receive_message */ channel_receive_message
};

SWITCH_STANDARD_APP(unloop_function) { /* NOOP */}

SWITCH_MODULE_LOAD_FUNCTION(mod_loopback_load)
{
	switch_application_interface_t *app_interface;

	memset(&globals, 0, sizeof(globals));

	/* connect my internal structure to the blank pointer passed to me */
	*module_interface = switch_loadable_module_create_module_interface(pool, modname);
	loopback_endpoint_interface = switch_loadable_module_create_interface(*module_interface, SWITCH_ENDPOINT_INTERFACE);
	loopback_endpoint_interface->interface_name = "loopback";
	loopback_endpoint_interface->io_routines = &channel_io_routines;
	loopback_endpoint_interface->state_handler = &channel_event_handlers;

	SWITCH_ADD_APP(app_interface, "unloop", "Tell loopback to unfold", "Tell loopback to unfold", unloop_function, "", SAF_NO_LOOPBACK);

	/* indicate that the module should continue to be loaded */
	return SWITCH_STATUS_SUCCESS;
}

SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_loopback_shutdown)
{
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
