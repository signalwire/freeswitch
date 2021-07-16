/*
 * FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
 * Copyright (C) 2005-2014, Anthony Minessale II <anthm@freeswitch.org>
 *
 * Version: MPL 1.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License `
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
 * Raymond Chandler <intralanman@freeswitch.org>
 * Jérôme Poulin <jeromepoulin@gmail.com>
 *
 *
 * mod_pulseaudio.c -- PulseAudio Endpoint Module
 * based on mod_portaudio.c a0e19e1c7f2fa9256eaf76be346f50302eeed51f
 *
 */

#include "switch.h"
#include <stdlib.h>
#include <string.h>
#include "pablio.h"

#define MY_EVENT_RINGING "pulseaudio::ringing"
#define MY_EVENT_MAKE_CALL "pulseaudio::makecall"
#define MY_EVENT_ERROR_AUDIO_DEV "pulseaudio::audio_dev_error"
#define MOD_PA_CALL_ID_VARIABLE "pa_call_id"

#define MOD_PA_STREAM_VOICE "Voice"
#define MOD_PA_STREAM_RING "Ring"
#define MOD_PA_STREAM_PLAYBACK "Playback"

#define MOD_PA_VAR_SAMPLE_RATE "pa_sample_rate"
#define MOD_PA_VAR_CODEC_MS "pa_codec_ms"
#define MOD_PA_VAR_RING_FILE "pa_ring_file"
#define MOD_PA_VAR_APP_NAME "pa_app_name"

#define MIN_STREAM_SAMPLE_RATE 8000
#define MAX_IO_CHANNELS 2
#define STREAM_SAMPLES_PER_PACKET(stream) ((stream->codec_ms * stream->sample_rate) / 1000)

SWITCH_MODULE_LOAD_FUNCTION(mod_pulseaudio_load);
SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_pulseaudio_shutdown);
SWITCH_MODULE_DEFINITION(mod_pulseaudio, mod_pulseaudio_load, mod_pulseaudio_shutdown, NULL);

static switch_memory_pool_t *module_pool = NULL;
switch_endpoint_interface_t *pulseaudio_endpoint_interface;

typedef switch_status_t (*pa_command_t) (char **argv, int argc, switch_stream_handle_t *endpoint);

typedef enum {
	TFLAG_IO = (1 << 0),
	TFLAG_INBOUND = (1 << 1),
	TFLAG_OUTBOUND = (1 << 2),
	TFLAG_DTMF = (1 << 3),
	TFLAG_VOICE = (1 << 4),
	TFLAG_HANGUP = (1 << 5),
	TFLAG_ANSWER = (1 << 6),
	TFLAG_HUP = (1 << 7),
	TFLAG_AUTO_ANSWER = (1 << 8),
	TFLAG_EAR = (1 << 9),
	TFLAG_MOUTH = (1 << 10),
	TFLAG_RING = (1 << 11),
} TFLAGS;

/* Endpoint that can be called via pulseaudio/endpoint/<endpoint-name> */
typedef struct _audio_endpoint_t {
	/* Friendly name for this endpoint */
	char app_name[255];
	char name[255];
	/* Sampling rate */
	int sample_rate;
	/* Buffer packetization (and therefore timing) */
	int codec_ms;
	/* The io stream helper to buffer audio */
	PABLIO_Stream *pa_stream;
	/* For timed read and writes */
	switch_timer_t read_timer;
	switch_timer_t write_timer;
	/* We need our own read frame */
	switch_frame_t read_frame;
	unsigned char read_buf[SWITCH_RECOMMENDED_BUFFER_SIZE];
	/* Needed codecs for the core to read/write in the proper format */
	switch_codec_t read_codec;
	switch_codec_t write_codec;
} audio_endpoint_t;

typedef struct private_object private_t;

struct private_object {
	unsigned int flags;
	switch_core_session_t *session;
	switch_caller_profile_t *caller_profile;
	char call_id[50];
	int sample_rate;
	int codec_ms;
	switch_mutex_t *flag_mutex;
	audio_endpoint_t *voice_endpoint;
	audio_endpoint_t *ring_endpoint;
	switch_timer_t readfile_timer;
	struct private_object *next;
};

static struct {
	int debug;
	int port;
	char *cid_name;
	char *cid_num;
	char *dialplan;
	char *context;
	char *ring_file;
	char *timer_name;
	char *app_name;
	audio_endpoint_t default_audio;
	switch_hash_t *call_hash;
	switch_mutex_t *call_hash_mutex;
	uint64_t current_call_id;
	int sample_rate;
	int codec_ms;
	switch_frame_t cng_frame;
	unsigned char cngbuf[SWITCH_RECOMMENDED_BUFFER_SIZE];
	/*! Endpoints configured */
	int ring_interval;
	time_t deactivate_timer;
	int no_ring_during_call;
} globals;

SWITCH_DECLARE_GLOBAL_STRING_FUNC(set_global_dialplan, globals.dialplan);
SWITCH_DECLARE_GLOBAL_STRING_FUNC(set_global_context, globals.context);
SWITCH_DECLARE_GLOBAL_STRING_FUNC(set_global_cid_name, globals.cid_name);
SWITCH_DECLARE_GLOBAL_STRING_FUNC(set_global_cid_num, globals.cid_num);
SWITCH_DECLARE_GLOBAL_STRING_FUNC(set_global_ring_file, globals.ring_file);
SWITCH_DECLARE_GLOBAL_STRING_FUNC(set_global_timer_name, globals.timer_name);
SWITCH_DECLARE_GLOBAL_STRING_FUNC(set_global_app_name, globals.app_name);

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

static private_t *create_tech_pvt(switch_core_session_t *session);
static void destroy_tech_pvt(private_t *tech_pvt);
static audio_endpoint_t *create_audio_endpoint(switch_core_session_t *session, const char *endpoint_name);
static switch_status_t destroy_audio_endpoint(audio_endpoint_t *endpoint);

static switch_status_t load_config(void);
SWITCH_STANDARD_API(pa_cmd);

/*
   State methods they get called when the state changes to the specific state
   returning SWITCH_STATUS_SUCCESS tells the core to execute the standard state method next
   so if you fully implement the state you can return SWITCH_STATUS_FALSE to skip it.
*/
static switch_status_t channel_on_init(switch_core_session_t *session)
{
	switch_channel_t *channel;

	if (session) {
		if ((channel = switch_core_session_get_channel(session))) {
			switch_channel_set_flag(channel, CF_AUDIO);
		}
	}

	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t prepare_ringer(switch_core_session_t *session, switch_file_handle_t *ring_file_handle)
{
	switch_channel_t *channel = switch_core_session_get_channel(session);
	private_t *tech_pvt = switch_core_session_get_private(session);
	switch_status_t ret;
	const char *ring_file = globals.ring_file;
	const char *val;

	switch_set_flag_locked(tech_pvt, TFLAG_RING);

	if (!tech_pvt->ring_endpoint)
		tech_pvt->ring_endpoint = create_audio_endpoint(session, MOD_PA_STREAM_RING);

	if (!tech_pvt->ring_endpoint) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "Could not open ring endpoint, disabling ringer.\n");
		return SWITCH_STATUS_FALSE;
	}

	if (!tech_pvt->readfile_timer.interval) {
		ret = switch_core_timer_init(&(tech_pvt->readfile_timer),
							   globals.timer_name, tech_pvt->ring_endpoint->codec_ms,
							   STREAM_SAMPLES_PER_PACKET(tech_pvt->ring_endpoint), module_pool);
		if (ret != SWITCH_STATUS_SUCCESS) {
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "Failed to setup readfile timer.\n");
			return SWITCH_STATUS_FALSE;
		}
	}

	if ((val = switch_channel_get_variable(channel, MOD_PA_VAR_RING_FILE)))
		ring_file = val;

	if (ring_file) {
		ret = switch_core_file_open(ring_file_handle,
								    ring_file,
									tech_pvt->ring_endpoint->read_codec.implementation->number_of_channels,
									tech_pvt->ring_endpoint->read_codec.implementation->actual_samples_per_second,
									SWITCH_FILE_FLAG_READ | SWITCH_FILE_DATA_SHORT, NULL);
		if (ret != SWITCH_STATUS_SUCCESS) {
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "Cannot open %s, disabling ringer.\n", ring_file);
			return SWITCH_STATUS_FALSE;
		}
	}

	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t channel_on_routing(switch_core_session_t *session)
{
	switch_channel_t *channel = switch_core_session_get_channel(session);
	private_t *tech_pvt = switch_core_session_get_private(session);
	switch_file_handle_t ring_file_handle = { 0 };
	switch_time_t last;
	switch_status_t ringer_ready = SWITCH_STATUS_FALSE;
	int waitsec = globals.ring_interval * 1000000;
	int16_t abuf[2048];

	switch_assert(tech_pvt != NULL);

	last = switch_micro_time_now() - waitsec;

	if (switch_test_flag(tech_pvt, TFLAG_OUTBOUND)) {
		if (!tech_pvt->voice_endpoint) {
			tech_pvt->voice_endpoint = create_audio_endpoint(session, MOD_PA_STREAM_VOICE);
			if (!tech_pvt->voice_endpoint) {
				switch_channel_hangup(channel, SWITCH_CAUSE_DESTINATION_OUT_OF_ORDER);
				return SWITCH_STATUS_FALSE;
			}
			switch_core_session_set_read_codec(session, &tech_pvt->voice_endpoint->read_codec);
			switch_core_session_set_write_codec(session, &tech_pvt->voice_endpoint->write_codec);
		}

		if (!switch_test_flag(tech_pvt, TFLAG_RING) &&
			!switch_test_flag(tech_pvt, TFLAG_AUTO_ANSWER))
				ringer_ready = prepare_ringer(session, &ring_file_handle);

		if (tech_pvt->voice_endpoint || switch_test_flag(tech_pvt, TFLAG_AUTO_ANSWER)) {
			switch_mutex_lock(tech_pvt->flag_mutex);
			if (switch_test_flag(tech_pvt, TFLAG_AUTO_ANSWER)) {
				switch_channel_mark_answered(channel);
				switch_set_flag(tech_pvt, TFLAG_ANSWER);
			}
			switch_mutex_unlock(tech_pvt->flag_mutex);
		} else {
			switch_channel_mark_ring_ready(channel);
		}

		while (switch_channel_get_state(channel) == CS_ROUTING &&
		       !switch_channel_test_flag(channel, CF_ANSWERED) &&
		       !switch_test_flag(tech_pvt, TFLAG_ANSWER)) {
			switch_size_t olen = tech_pvt->readfile_timer.samples;

			if (switch_micro_time_now() - last >= waitsec) {
				char buf[512];
				switch_event_t *event;

				switch_snprintf(buf, sizeof(buf), "BRRRRING! BRRRRING! call %s", tech_pvt->call_id);

				if (switch_event_create_subclass(&event, SWITCH_EVENT_CUSTOM, MY_EVENT_RINGING) == SWITCH_STATUS_SUCCESS) {
					switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "event_info", buf);
					switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "call_id", tech_pvt->call_id); /* left behind for backwards compatability */
					switch_channel_set_variable(channel, MOD_PA_CALL_ID_VARIABLE, tech_pvt->call_id);
					switch_channel_event_set_data(channel, event);
					switch_event_fire(&event);
				}
				switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "%s\n", buf);
				last = switch_micro_time_now();
			}

			if (ringer_ready == SWITCH_STATUS_SUCCESS) {
				if (switch_core_timer_next(&(tech_pvt->readfile_timer)) != SWITCH_STATUS_SUCCESS) {
					break;
				}
				switch_core_file_read(&ring_file_handle, abuf, &olen);
				if (olen == 0) {
					unsigned int pos = 0;
					switch_core_file_seek(&ring_file_handle, &pos, 0, SEEK_SET);
				}

				if ((!globals.no_ring_during_call)) {
						WriteAudioStream(tech_pvt->ring_endpoint->pa_stream,
										 abuf, olen*2, &(tech_pvt->ring_endpoint->write_timer));
				}
			}
			switch_yield(10000);
		}
	}

	switch_clear_flag_locked(tech_pvt, TFLAG_RING);
	if (ringer_ready == SWITCH_STATUS_SUCCESS) {
		switch_core_file_close(&ring_file_handle);
		destroy_audio_endpoint(tech_pvt->ring_endpoint);
		tech_pvt->ring_endpoint = NULL;
	}

	if (switch_test_flag(tech_pvt, TFLAG_OUTBOUND)) {
		if (!switch_test_flag(tech_pvt, TFLAG_ANSWER) &&
		    !switch_channel_test_flag(channel, CF_ANSWERED)) {
			switch_channel_hangup(channel, SWITCH_CAUSE_NO_ANSWER);
			return SWITCH_STATUS_SUCCESS;
		}
		switch_set_flag(tech_pvt, TFLAG_ANSWER);
	}

	switch_set_flag_locked(tech_pvt, TFLAG_IO);

	switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "%s CHANNEL ROUTING\n",
					  switch_channel_get_name(switch_core_session_get_channel(session)));
	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t channel_on_execute(switch_core_session_t *session)
{
	switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "%s CHANNEL EXECUTE\n",
					  switch_channel_get_name(switch_core_session_get_channel(session)));
	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t destroy_audio_endpoint(audio_endpoint_t *endpoint)
{
	if (!endpoint) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Audio endpoint already freed.");
		return SWITCH_STATUS_SUCCESS;
	}

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Closing audio endpoint '%s'\n", endpoint->name);
	CloseAudioStream(endpoint->pa_stream);

	if (endpoint->read_timer.interval)
		switch_core_timer_destroy(&endpoint->read_timer);

	if (endpoint->write_timer.interval)
		switch_core_timer_destroy(&endpoint->write_timer);

	if (endpoint->read_codec.codec_interface)
			switch_core_codec_destroy(&endpoint->read_codec);

	if (endpoint->write_codec.codec_interface)
			switch_core_codec_destroy(&endpoint->write_codec);

	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t channel_on_destroy(switch_core_session_t *session)
{
	private_t *tech_pvt = switch_core_session_get_private(session);
	switch_assert(tech_pvt != NULL);

	destroy_tech_pvt(tech_pvt);

	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t channel_on_hangup(switch_core_session_t *session)
{
	private_t *tech_pvt = switch_core_session_get_private(session);
	switch_assert(tech_pvt != NULL);

	destroy_audio_endpoint(tech_pvt->voice_endpoint);
	destroy_audio_endpoint(tech_pvt->ring_endpoint);
	switch_clear_flag_locked(tech_pvt, TFLAG_IO);
	switch_set_flag_locked(tech_pvt, TFLAG_HUP);
	switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "%s CHANNEL HANGUP\n",
					  switch_channel_get_name(switch_core_session_get_channel(session)));

	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t channel_kill_channel(switch_core_session_t *session, int sig)
{
	switch_channel_t *channel = switch_core_session_get_channel(session);
	private_t *tech_pvt = switch_core_session_get_private(session);
	switch_assert(tech_pvt != NULL);

	switch (sig) {
	case SWITCH_SIG_KILL:
		switch_set_flag_locked(tech_pvt, TFLAG_HUP);
		switch_channel_hangup(channel, SWITCH_CAUSE_NORMAL_CLEARING);
		break;
	default:
		break;
	}
	switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session),
					  SWITCH_LOG_DEBUG, "%s CHANNEL KILL SIG %d\n",
					  switch_channel_get_name(channel), sig);

	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t channel_on_soft_execute(switch_core_session_t *session)
{
	switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "CHANNEL TRANSMIT\n");
	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t channel_on_exchange_media(switch_core_session_t *session)
{
	switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "CHANNEL LOOPBACK\n");
	return SWITCH_STATUS_SUCCESS;
}


static switch_status_t channel_send_dtmf(switch_core_session_t *session, const switch_dtmf_t *dtmf)
{
	private_t *tech_pvt = switch_core_session_get_private(session);
	switch_assert(tech_pvt != NULL);

	switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "DTMF ON CALL %s [%c]\n", tech_pvt->call_id, dtmf->digit);

	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t channel_read_frame(switch_core_session_t *session, switch_frame_t **frame, switch_io_flag_t flags, int stream_id)
{
	private_t *tech_pvt = switch_core_session_get_private(session);
	audio_endpoint_t *endpoint;
	int datalen = 0;

	switch_assert(tech_pvt != NULL);
	endpoint = tech_pvt->voice_endpoint;

	if (switch_test_flag(tech_pvt, TFLAG_HUP)) {
		return SWITCH_STATUS_SUCCESS;
	}

	if (tech_pvt->voice_endpoint) {
		if (switch_test_flag(tech_pvt, TFLAG_MOUTH))
			datalen = ReadAudioStream(endpoint->pa_stream,
									  endpoint->read_frame.data,
									  STREAM_SAMPLES_PER_PACKET(endpoint)*sizeof(SAMPLE),
									  &(endpoint->read_timer));
		else
			FlushAudioStream(endpoint->pa_stream);

		if (!datalen) {
			switch_core_timer_next(&endpoint->read_timer);
			globals.cng_frame.rate = endpoint->read_frame.rate;
			globals.cng_frame.codec = endpoint->read_frame.codec;
			*frame = &globals.cng_frame;
			return SWITCH_STATUS_SUCCESS;
		}

		endpoint->read_frame.datalen = datalen;
		endpoint->read_frame.samples = (datalen / sizeof(SAMPLE));
		endpoint->read_frame.codec = &endpoint->read_codec;
		*frame = &endpoint->read_frame;
	}

	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t channel_write_frame(switch_core_session_t *session, switch_frame_t *frame, switch_io_flag_t flags, int stream_id)
{
	private_t *tech_pvt = switch_core_session_get_private(session);
	audio_endpoint_t *endpoint;

	switch_assert(tech_pvt != NULL);
	endpoint = tech_pvt->voice_endpoint;

	if (tech_pvt->voice_endpoint && switch_test_flag(tech_pvt, TFLAG_EAR)) {
		WriteAudioStream(endpoint->pa_stream, (short *)frame->data,
						 frame->datalen, &(endpoint->write_timer));
	}

	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t channel_answer_channel(switch_core_session_t *session)
{
	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t channel_receive_message(switch_core_session_t *session, switch_core_session_message_t *msg)
{
	private_t *tech_pvt = switch_core_session_get_private(session);
	switch_assert(tech_pvt != NULL);

	switch (msg->message_id) {
	case SWITCH_MESSAGE_INDICATE_AUDIO_SYNC:
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "Flushing voice stream\n");
		FlushAudioStream(tech_pvt->voice_endpoint->pa_stream);
		break;
	case SWITCH_MESSAGE_INDICATE_ANSWER:
		channel_answer_channel(session);
		break;
	case SWITCH_MESSAGE_INDICATE_PROGRESS:
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "Engage Early Media\n");
		switch_set_flag_locked(tech_pvt, TFLAG_IO);
		break;
	default:
		break;
	}
	return SWITCH_STATUS_SUCCESS;
}

switch_state_handler_table_t pulseaudio_event_handlers = {
	/*.on_init */ channel_on_init,
	/*.on_routing */ channel_on_routing,
	/*.on_execute */ channel_on_execute,
	/*.on_hangup */ channel_on_hangup,
	/*.on_exchange_media */ channel_on_exchange_media,
	/*.on_soft_execute */ channel_on_soft_execute,
	/*.on_consume_media */ NULL,
	/*.on_hibernate */ NULL,
	/*.on_reset */ NULL,
	/*.on_park */ NULL,
	/*.on_reporting */ NULL,
	/*.on_destroy */ channel_on_destroy
};

switch_io_routines_t pulseaudio_io_routines = {
	/*.outgoing_channel */ channel_outgoing_channel,
	/*.read_frame */ channel_read_frame,
	/*.write_frame */ channel_write_frame,
	/*.kill_channel */ channel_kill_channel,
	/*.send_dtmf */ channel_send_dtmf,
	/*.receive_message */ channel_receive_message
};

static private_t *create_tech_pvt(switch_core_session_t *session)
{
	private_t *tech_pvt;
	uint64_t call_id = globals.current_call_id++;

	tech_pvt = (private_t *) switch_core_session_alloc(session, sizeof(private_t));
	if (!tech_pvt) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_CRIT, "Hey where is my memory pool?\n");
		return NULL;
	}

	memset(tech_pvt, 0, sizeof(*tech_pvt));
	switch_snprintf(tech_pvt->call_id, sizeof(tech_pvt->call_id), "%d", call_id);
	switch_mutex_init(&tech_pvt->flag_mutex, SWITCH_MUTEX_NESTED, switch_core_session_get_pool(session));
	tech_pvt->session = session;
	tech_pvt->flags = TFLAG_EAR | TFLAG_MOUTH;
	switch_channel_set_variable(switch_core_session_get_channel(session),
								MOD_PA_CALL_ID_VARIABLE, tech_pvt->call_id);
	switch_mutex_lock(globals.call_hash_mutex);
	switch_core_hash_insert_locked(globals.call_hash,
								   tech_pvt->call_id,
								   tech_pvt, globals.call_hash_mutex);
	switch_mutex_unlock(globals.call_hash_mutex);

	return tech_pvt;
}

static void destroy_tech_pvt(private_t *tech_pvt)
{
	if (tech_pvt->readfile_timer.interval)
		switch_core_timer_destroy(&(tech_pvt->readfile_timer));
	switch_mutex_lock(globals.call_hash_mutex);
	switch_core_hash_delete(globals.call_hash, tech_pvt->call_id);
	switch_mutex_unlock(globals.call_hash_mutex);
	switch_mutex_unlock(tech_pvt->flag_mutex);
	switch_mutex_destroy(tech_pvt->flag_mutex);
}

/* Make sure when you have 2 sessions in the same scope that you pass the appropriate one to the routines
   that allocate memory or you will have 1 channel with memory allocated from another channel's pool!
*/
static switch_call_cause_t channel_outgoing_channel(switch_core_session_t *session, switch_event_t *var_event,
													switch_caller_profile_t *outbound_profile,
													switch_core_session_t **new_session, switch_memory_pool_t **pool, switch_originate_flag_t flags,
													switch_call_cause_t *cancel_cause)
{
	char name[128];
	const char *id = NULL;
	private_t *tech_pvt = NULL;
	switch_channel_t *channel = NULL;
	switch_caller_profile_t *caller_profile = NULL;
	switch_call_cause_t retcause = SWITCH_CAUSE_DESTINATION_OUT_OF_ORDER;

	if (!outbound_profile) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Missing caller profile\n");
		return retcause;
	}

	*new_session = switch_core_session_request_uuid(pulseaudio_endpoint_interface,
													SWITCH_CALL_DIRECTION_OUTBOUND,
													flags, pool, switch_event_get_header(var_event, "origination_uuid"));
	if (!*new_session)
		return retcause;

	switch_core_session_add_stream(*new_session, NULL);
	tech_pvt = create_tech_pvt(*new_session);
	if (tech_pvt == NULL) {
		switch_core_session_destroy(new_session);
		return retcause;
	}
	channel = switch_core_session_get_channel(*new_session);
	switch_core_session_set_private(*new_session, tech_pvt);

	id = !zstr(outbound_profile->caller_id_number) ? outbound_profile->caller_id_number : "na";
	switch_snprintf(name, sizeof(name), "pulseaudio/%s", id);
	if (outbound_profile->destination_number && !strcasecmp(outbound_profile->destination_number, "auto_answer")) {
		switch_set_flag(tech_pvt, TFLAG_AUTO_ANSWER);
	}

	switch_channel_set_name(channel, name);
	caller_profile = switch_caller_profile_clone(*new_session, outbound_profile);
	switch_channel_set_caller_profile(channel, caller_profile);
	tech_pvt->caller_profile = caller_profile;
	switch_set_flag_locked(tech_pvt, TFLAG_OUTBOUND);
	switch_channel_set_state(channel, CS_INIT);
	switch_channel_set_flag(channel, CF_AUDIO);
	return SWITCH_CAUSE_SUCCESS;
}


SWITCH_MODULE_LOAD_FUNCTION(mod_pulseaudio_load)
{
	switch_status_t status;
	switch_api_interface_t *api_interface;

	module_pool = pool;

	memset(&globals, 0, sizeof(globals));

	switch_core_hash_init(&globals.call_hash);
	switch_mutex_init(&globals.call_hash_mutex, SWITCH_MUTEX_NESTED, module_pool);
	globals.cng_frame.data = globals.cngbuf;
	globals.cng_frame.buflen = sizeof(globals.cngbuf);
	switch_set_flag((&globals.cng_frame), SFF_CNG);

	if ((status = load_config()) != SWITCH_STATUS_SUCCESS) {
		return status;
	}

	if (switch_event_reserve_subclass(MY_EVENT_RINGING) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Couldn't register subclass!\n");
		return SWITCH_STATUS_GENERR;
	}

	if (switch_event_reserve_subclass(MY_EVENT_MAKE_CALL) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Couldn't register subclass!\n");
		return SWITCH_STATUS_GENERR;
	}
	if (switch_event_reserve_subclass(MY_EVENT_ERROR_AUDIO_DEV) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Couldn't register subclass!\n");
		return SWITCH_STATUS_GENERR;
	}


	/* connect my internal structure to the blank pointer passed to me */
	*module_interface = switch_loadable_module_create_module_interface(pool, modname);
	pulseaudio_endpoint_interface = switch_loadable_module_create_interface(*module_interface, SWITCH_ENDPOINT_INTERFACE);
	pulseaudio_endpoint_interface->interface_name = "pulseaudio";
	pulseaudio_endpoint_interface->io_routines = &pulseaudio_io_routines;
	pulseaudio_endpoint_interface->state_handler = &pulseaudio_event_handlers;

	SWITCH_ADD_API(api_interface, "pulse", "PulseAudio", pa_cmd, "<command> [<args>]");
	switch_console_set_complete("add pulse help");
	switch_console_set_complete("add pulse call");
	switch_console_set_complete("add pulse answer");
	switch_console_set_complete("add pulse hangup");
	switch_console_set_complete("add pulse list");
	switch_console_set_complete("add pulse switch");
	switch_console_set_complete("add pulse dtmf");
	switch_console_set_complete("add pulse flags");
	switch_console_set_complete("add pulse ringfile");
	switch_console_set_complete("add pulse play");
	switch_console_set_complete("add pulse looptest");

	/* indicate that the module should continue to be loaded */
	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t load_config(void)
{
	char *cf = "pulseaudio.conf";
	switch_xml_t cfg, xml, settings, param;
	switch_status_t status = SWITCH_STATUS_SUCCESS;

	if (!(xml = switch_xml_open_cfg(cf, &cfg, NULL))) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Open of %s failed\n", cf);
		return SWITCH_STATUS_TERM;
	}
	globals.no_ring_during_call = 0;
	globals.sample_rate = 8000;

	if ((settings = switch_xml_child(cfg, "settings"))) {
		for (param = switch_xml_child(settings, "param"); param; param = param->next) {
			char *var = (char *) switch_xml_attr_soft(param, "name");
			char *val = (char *) switch_xml_attr_soft(param, "value");

			if (!strcmp(var, "debug")) {
				globals.debug = atoi(val);
			} else if (!strcmp(var, "ring-interval")) {
				globals.ring_interval = atoi(val);
			} else if (!strcmp(var, "no-ring-during-call")) {
				if (switch_true(val)) {
					globals.no_ring_during_call = 1;
				} else {
					globals.no_ring_during_call = 0;
				}
			} else if (!strcmp(var, "ring-file")) {
				set_global_ring_file(val);
			} else if (!strcmp(var, "timer-name")) {
				set_global_timer_name(val);
			} else if (!strcmp(var, "sample-rate")) {
				globals.sample_rate = atoi(val);
			} else if (!strcmp(var, "codec-ms")) {
				int tmp = atoi(val);
				if (switch_check_interval(globals.sample_rate, tmp)) {
					globals.codec_ms = tmp;
				} else {
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING,
									  "codec-ms must be multiple of 10 and less than %d, Using default of 20\n", SWITCH_MAX_INTERVAL);
				}
			} else if (!strcmp(var, "dialplan")) {
				set_global_dialplan(val);
			} else if (!strcmp(var, "context")) {
				set_global_context(val);
			} else if (!strcmp(var, "cid-name")) {
				set_global_cid_name(val);
			} else if (!strcmp(var, "cid-num")) {
				set_global_cid_num(val);
			} else if (!strcmp(var, "application-name")) {
				set_global_app_name(val);
			}
		}
	}

	if (!globals.dialplan) {
		set_global_dialplan("XML");
	}

	if (!globals.context) {
		set_global_context("default");
	}

	if (!globals.sample_rate) {
		globals.sample_rate = 8000;
	}

	if (!globals.codec_ms) {
		globals.codec_ms = 20;
	}

	globals.cng_frame.datalen = switch_samples_per_packet(globals.sample_rate, globals.codec_ms) * 2;

	if (!globals.ring_interval) {
		globals.ring_interval = 5;
	}

	if (!globals.timer_name) {
		set_global_timer_name("soft");
	}

	switch_xml_free(xml);

	return status;
}

SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_pulseaudio_shutdown)
{
	switch_core_hash_destroy(&globals.call_hash);

	switch_event_free_subclass(MY_EVENT_RINGING);
	switch_event_free_subclass(MY_EVENT_MAKE_CALL);
	switch_event_free_subclass(MY_EVENT_ERROR_AUDIO_DEV);

	switch_safe_free(globals.dialplan);
	switch_safe_free(globals.context);
	switch_safe_free(globals.cid_name);
	switch_safe_free(globals.cid_num);
	switch_safe_free(globals.ring_file);
	switch_safe_free(globals.timer_name);

	return SWITCH_STATUS_SUCCESS;
}

/*******************************************************************/
static switch_status_t play_dev(switch_stream_handle_t *stream, char *file, const char *max_seconds, const char *no_close)
{
	switch_status_t errstatus = SWITCH_STATUS_FALSE;
	switch_file_handle_t fh = { 0 };
	int samples = 0;
	int seconds = 5;
	audio_endpoint_t *playback_stream;
	int wrote = 0;
	switch_size_t olen;
	int16_t abuf[2048];


	if (!strcasecmp(file, "ringtest")) {
		file = globals.ring_file;
	}
	playback_stream = create_audio_endpoint(NULL, MOD_PA_STREAM_PLAYBACK);
	if (!playback_stream) {
		stream->write_function(stream, "Failed to engage audio device\n");
		goto finally;
	}

	fh.pre_buffer_datalen = SWITCH_DEFAULT_FILE_BUFFER_LEN;

	if (switch_core_file_open(&fh,	file,
		playback_stream->read_codec.implementation->number_of_channels,
		playback_stream->read_codec.implementation->actual_samples_per_second,
		SWITCH_FILE_FLAG_READ | SWITCH_FILE_DATA_SHORT, NULL) != SWITCH_STATUS_SUCCESS) {
			stream->write_function(stream, "Cannot play requested file %s\n", file);
			goto finally;
	}

	olen = playback_stream->read_codec.implementation->samples_per_packet;

	if (max_seconds) {
		int i = atoi(max_seconds);
		if (i >= 0) {
			seconds = i;
		}
	}

	samples = playback_stream->read_codec.implementation->actual_samples_per_second * seconds;
	while (switch_core_file_read(&fh, abuf, &olen) == SWITCH_STATUS_SUCCESS) {
		WriteAudioStream(playback_stream->pa_stream, abuf, olen*2, &(playback_stream->write_timer));
		wrote += (int) olen;
		if (samples) {
			samples -= (int) olen;
			if (samples <= 0) {
				break;
			}
		}
		olen = playback_stream->read_codec.implementation->samples_per_packet;
	}

	seconds = wrote / playback_stream->read_codec.implementation->actual_samples_per_second;
	stream->write_function(stream, "playback test [%s] %d second(s) %d samples @%dkhz",
		file, seconds, wrote, playback_stream->read_codec.implementation->actual_samples_per_second);

	errstatus = SWITCH_STATUS_SUCCESS;

finally:
	if (playback_stream)
		destroy_audio_endpoint(playback_stream);
	switch_core_file_close(&fh);
	return errstatus;
}

static PABLIO_Stream *create_audio_stream(switch_core_session_t *session, audio_endpoint_t *endpoint)
{
	pa_sample_spec input_parameters, output_parameters;
	pa_error err;
	PABLIO_Stream *stream;
	switch_event_t *event;

	if (session)
		stream = switch_core_session_alloc(session, sizeof(*stream));
	else
		stream = switch_core_alloc(module_pool, sizeof(*stream));

	if (stream == NULL) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Unable to alloc memory for stream\n");
		return NULL;
	}
	memset(stream, 0, sizeof(*stream));

	input_parameters.channels = 1;
	input_parameters.format = PA_SAMPLE_S16LE;
	input_parameters.rate = endpoint->sample_rate;

	output_parameters.channels = 1;
	output_parameters.format = PA_SAMPLE_S16LE;
	output_parameters.rate = endpoint->sample_rate;

	err = OpenAudioStream(&stream, endpoint->app_name, endpoint->name, &input_parameters, &output_parameters);
	if (err) {
		switch_safe_free(stream);
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Can't open PulseAudio server: %s\n", pa_strerror(err));
		if (switch_event_create_subclass(&event, SWITCH_EVENT_CUSTOM, MY_EVENT_ERROR_AUDIO_DEV) == SWITCH_STATUS_SUCCESS) {
			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "Reason", pa_strerror(err));
			switch_event_fire(&event);
		}
		return NULL;
	}
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG,
					  "Created %s audio stream: %d Hz, %d channels\n",
					  endpoint->name, output_parameters.rate, output_parameters.channels);

	return stream;
}

static audio_endpoint_t *create_audio_endpoint(switch_core_session_t *session, const char *endpoint_name)
{
	switch_channel_t *channel = NULL;
	audio_endpoint_t *endpoint;
	const char *channel_var;
	int sample_rate = 0;
	int codec_ms = 0;
	int samples_per_packet;

	if (session) {
		endpoint = switch_core_session_alloc(session, sizeof(*endpoint));
		channel = switch_core_session_get_channel(session);
	} else {
		endpoint = switch_core_alloc(module_pool, sizeof(*endpoint));
	}

	if (endpoint == NULL) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Unable to alloc memory\n");
		return NULL;
	}

	switch_snprintf(endpoint->name, sizeof(endpoint->name), "%s", endpoint_name);
	switch_snprintf(endpoint->app_name, sizeof(endpoint->app_name), "%s", globals.app_name);
	endpoint->read_frame.data = endpoint->read_buf;
	endpoint->read_frame.buflen = sizeof(endpoint->read_buf);

	if (channel) {
		channel_var = switch_channel_get_variable(channel, MOD_PA_VAR_SAMPLE_RATE);
		if (channel_var)
			sample_rate = atoi(channel_var);

		channel_var = switch_channel_get_variable(channel, MOD_PA_VAR_CODEC_MS);
		if (channel_var)
			codec_ms = atoi(channel_var);

		channel_var = switch_channel_get_variable(channel, MOD_PA_VAR_APP_NAME);
		if (channel_var)
			switch_snprintf(endpoint->app_name, sizeof(endpoint->app_name), "%s", channel_var);

		channel_var = switch_channel_get_variable(channel, "uuid");
		if (channel_var)
			switch_snprintf(endpoint->name, sizeof(endpoint->name), "%s %s", endpoint_name, channel_var);
	}

	if (sample_rate >= MIN_STREAM_SAMPLE_RATE) {
		endpoint->sample_rate = sample_rate;
	} else if (globals.sample_rate >= MIN_STREAM_SAMPLE_RATE) {
		endpoint->sample_rate = globals.sample_rate;
	} else {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING,
			"Invalid sample rate specified for endpoint '%s', forcing to 8000\n", endpoint_name);
		endpoint->sample_rate = MIN_STREAM_SAMPLE_RATE;
	}
	sample_rate = endpoint->sample_rate;

	if (codec_ms && switch_check_interval(endpoint->sample_rate, codec_ms)) {
		endpoint->codec_ms = codec_ms;
	} else if (globals.codec_ms && switch_check_interval(endpoint->sample_rate, globals.codec_ms)) {
		endpoint->codec_ms = globals.codec_ms;
	} else {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING,
						  "codec-ms must be multiple of 10 and less than %d, Using default of 20\n", SWITCH_MAX_INTERVAL);
		endpoint->codec_ms = 20;
	}
	codec_ms = endpoint->codec_ms;

	samples_per_packet = STREAM_SAMPLES_PER_PACKET(endpoint);

	if (switch_core_timer_init(&endpoint->read_timer,
							   globals.timer_name, codec_ms,
							   samples_per_packet, module_pool) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "failed to setup read timer for endpoint '%s'!\n", endpoint->name);
		goto error;
	}

	if (switch_core_timer_init(&endpoint->write_timer,
					   globals.timer_name, codec_ms,
					   samples_per_packet, module_pool) != SWITCH_STATUS_SUCCESS) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "failed to setup write timer for endpoint '%s'!\n", endpoint->name);
			goto error;
	}

	if (switch_core_codec_init(&endpoint->read_codec,
							"L16", NULL, NULL, sample_rate, codec_ms, 1,
							SWITCH_CODEC_FLAG_ENCODE | SWITCH_CODEC_FLAG_DECODE, NULL, NULL) != SWITCH_STATUS_SUCCESS) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Can't load L16 read codec?\n");
			goto error;
	}

	if (switch_core_codec_init(&endpoint->write_codec,
							 "L16", NULL, NULL, sample_rate, codec_ms, 1,
							 SWITCH_CODEC_FLAG_ENCODE | SWITCH_CODEC_FLAG_DECODE, NULL, NULL) != SWITCH_STATUS_SUCCESS) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Can't load L16 write codec?\n");
			goto error;
	}

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE,
					  "Created endpoint '%s', sample-rate = %d, codec-ms = %d\n",
					  endpoint->name, endpoint->sample_rate, endpoint->codec_ms);

	endpoint->pa_stream = create_audio_stream(session, endpoint);

	if (endpoint->pa_stream == NULL) {
		goto error;
	}

	return endpoint;

error:
	if (endpoint) {
			if (endpoint->read_timer.interval) {
					switch_core_timer_destroy(&endpoint->read_timer);
			}
			if (endpoint->write_timer.interval) {
					switch_core_timer_destroy(&endpoint->write_timer);
			}
			if (endpoint->read_codec.codec_interface) {
					switch_core_codec_destroy(&endpoint->read_codec);
			}
			if (endpoint->write_codec.codec_interface) {
					switch_core_codec_destroy(&endpoint->write_codec);
			}
	}
	return NULL;
}

static switch_status_t dtmf_call(char **argv, int argc, switch_stream_handle_t *stream)
{
	char *call_id = argv[0];
	char *dtmf_str = argv[1];
	private_t *tech_pvt;
	switch_dtmf_t dtmf = { 0, switch_core_default_dtmf_duration(0) };

	if (zstr(call_id)) {
		stream->write_function(stream, "No call ID supplied.\n");
	}
	else if (zstr(dtmf_str)) {
		stream->write_function(stream, "No DTMF supplied.\n");
	} else {
		switch_mutex_lock(globals.call_hash_mutex);
		tech_pvt = switch_core_hash_find(globals.call_hash, call_id);
		switch_mutex_unlock(globals.call_hash_mutex);
		if (tech_pvt) {
			switch_channel_t *channel = switch_core_session_get_channel(tech_pvt->session);
			char *p = dtmf_str;
			switch_mutex_lock(tech_pvt->flag_mutex);
			while (p && *p) {
				dtmf.digit = *p;
				switch_channel_queue_dtmf(channel, &dtmf);
				p++;
			}
			switch_mutex_unlock(tech_pvt->flag_mutex);
		}
	}

	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t looptest(char **argv, int argc, switch_stream_handle_t *console_stream)
{
	switch_status_t errstatus = SWITCH_STATUS_FALSE;
	audio_endpoint_t *voice_endpoint;
	int datalen = 0;
	int success = 0;
	int i;

	if (switch_core_hash_first(globals.call_hash)) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "ERROR: Cannot use this command this while a call is in progress\n");
		return SWITCH_STATUS_FALSE;
	}

	voice_endpoint = create_audio_endpoint(NULL, MOD_PA_STREAM_VOICE);

	if (voice_endpoint == NULL)
		return SWITCH_STATUS_MEMERR;

	for (i = 0; i < 400; i++) {
		datalen = ReadAudioStream(voice_endpoint->pa_stream, voice_endpoint->read_frame.data,
								  voice_endpoint->read_codec.implementation->decoded_bytes_per_packet,
								  &(voice_endpoint->read_timer));
		if (datalen && voice_endpoint->read_frame.data) {
			WriteAudioStream(voice_endpoint->pa_stream, voice_endpoint->read_frame.data, datalen, &(voice_endpoint->write_timer));
			success = 1;
		}
		switch_yield(10000);
	}

	if (!success) {
		console_stream->write_function(console_stream, "Failed to read any bytes from PulseAudio\n");
		goto finally;
	}
	console_stream->write_function(console_stream, "looptest complete\n");
	errstatus = SWITCH_STATUS_SUCCESS;

finally:
	if (voice_endpoint)
		destroy_audio_endpoint(voice_endpoint);
	return errstatus;
}

static switch_status_t set_ringfile(char **argv, int argc, switch_stream_handle_t *stream)
{
	audio_endpoint_t *ring_endpoint;
	switch_status_t status;
	switch_file_handle_t fh = { 0 };

	if (! argv[0]) {
		stream->write_function(stream, "%s", globals.ring_file);
		return SWITCH_STATUS_SUCCESS;
	}

	ring_endpoint = create_audio_endpoint(NULL, MOD_PA_STREAM_RING);
	if (!ring_endpoint) {
		stream->write_function(stream, "could not initiate audio stream\n");
		return SWITCH_STATUS_MEMERR;
	}

	status = switch_core_file_open(&fh,
								   argv[0],
								   ring_endpoint->read_codec.implementation->number_of_channels,
								   ring_endpoint->read_codec.implementation->actual_samples_per_second,
								   SWITCH_FILE_FLAG_READ | SWITCH_FILE_DATA_SHORT, NULL);
	destroy_audio_endpoint(ring_endpoint);

	if (status == SWITCH_STATUS_SUCCESS) {
		switch_core_file_close(&fh);
		set_global_ring_file(argv[0]);
	} else {
		stream->write_function(stream, "ringfile Unable to open ring file %s\n", argv[0]);
		return SWITCH_STATUS_FALSE;
	}

	stream->write_function(stream, "ringfile set to %s", globals.ring_file);
	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t switch_call(char **argv, int argc, switch_stream_handle_t *stream)
{
	private_t *tech_pvt = NULL;
	switch_status_t errstatus = SWITCH_STATUS_FALSE;
	char *call_id = argv[0];

	switch_mutex_lock(globals.call_hash_mutex);
	if (zstr(call_id)) {
		stream->write_function(stream, "No call ID supplied.\n");
		goto done;
	} else if (!strcasecmp(call_id, "none")) {
		stream->write_function(stream, "OK\n");
		/*TODO: errstatus = SWITCH_STATUS_SUCCESS; */
		goto done;
	} else {
		tech_pvt = switch_core_hash_find(globals.call_hash, call_id);
	}

	if (tech_pvt) {
		stream->write_function(stream, "OK\n");
		/*TODO: errstatus = SWITCH_STATUS_SUCCESS; */
	} else {
		stream->write_function(stream, "NO SUCH CALL\n");
	}

done:
	switch_mutex_unlock(globals.call_hash_mutex);

	return errstatus;
}

static switch_status_t hangup_call(char **argv, int argc, switch_stream_handle_t *stream)
{
	private_t *tech_pvt;
	char *call_id = argv[0];

	if (zstr(call_id)) {
		stream->write_function(stream, "No call ID supplied.\n");
		return SWITCH_STATUS_FALSE;
	} else {
		switch_mutex_lock(globals.call_hash_mutex);
		tech_pvt = switch_core_hash_find(globals.call_hash, call_id);
		switch_mutex_unlock(globals.call_hash_mutex);
	}

	if (tech_pvt) {
		switch_channel_hangup(switch_core_session_get_channel(tech_pvt->session), SWITCH_CAUSE_NORMAL_CLEARING);
		stream->write_function(stream, "OK\n");
	} else {
		stream->write_function(stream, "NO SUCH CALL\n");
	}

	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t answer_call(char **argv, int argc, switch_stream_handle_t *stream)
{
	switch_status_t errstatus = SWITCH_STATUS_SUCCESS;
	char *callid = argv[0];

	switch_mutex_lock(globals.call_hash_mutex);

	if (!zstr(callid)) {
		private_t *tech_pvt;

		if ((tech_pvt = switch_core_hash_find(globals.call_hash, callid))) {
			if (switch_test_flag(tech_pvt, TFLAG_ANSWER)) {
				stream->write_function(stream, "CALL ALREADY ANSWERED\n");
				goto finally;
			} else {
				switch_channel_t *channel = switch_core_session_get_channel(tech_pvt->session);
				switch_set_flag_locked(tech_pvt, TFLAG_ANSWER);
				switch_channel_mark_answered(channel);
			}
		} else {
			stream->write_function(stream, "NO SUCH CALL\n");
			errstatus = SWITCH_STATUS_FALSE;
			goto finally;
		}
	} else {
		switch_hash_index_t *ci;
		void *val;

		for (ci = switch_core_hash_first(globals.call_hash); ci; ci = switch_core_hash_next(&ci)) {
			private_t *tech_pvt;

			switch_core_hash_this(ci, NULL, NULL, &val);
			tech_pvt = (private_t *)val;

			if (!switch_test_flag(tech_pvt, TFLAG_ANSWER)) {
				switch_channel_t *channel = switch_core_session_get_channel(tech_pvt->session);

				switch_set_flag_locked(tech_pvt, TFLAG_ANSWER);
				switch_channel_mark_answered(channel);
				break;
			}
		}
	}

	stream->write_function(stream, "Call Answered.\n");

finally:
	switch_mutex_unlock(globals.call_hash_mutex);
	return errstatus;
}

static switch_status_t do_flags(char **argv, int argc, switch_stream_handle_t *stream)
{
	char *call_id = argv[0];
	char *flag_str = argv[1];
	private_t *tech_pvt;

	if (argc < 2) {
		goto usage;
	}

	if (zstr(call_id)) {
		stream->write_function(stream, "No call ID supplied.\n");
		goto usage;
	} else {
		switch_mutex_lock(globals.call_hash_mutex);
		tech_pvt = switch_core_hash_find(globals.call_hash, call_id);
		switch_mutex_unlock(globals.call_hash_mutex);
	}

	if (!tech_pvt) {
		stream->write_function(stream, "NO SUCH CALL\n");
		return SWITCH_STATUS_FALSE;
	}

	if (!strcasecmp(flag_str, "-ear")) {
		switch_clear_flag_locked(tech_pvt, TFLAG_EAR);
	} else if (!strcasecmp(flag_str, "+ear")) {
		switch_set_flag_locked(tech_pvt, TFLAG_EAR);
	} else if (!strcasecmp(flag_str, "-mouth")) {
		switch_clear_flag_locked(tech_pvt, TFLAG_MOUTH);
	} else if (!strcasecmp(flag_str, "+mouth")) {
		switch_set_flag_locked(tech_pvt, TFLAG_MOUTH);
	} else {
		goto usage;
	}

	stream->write_function(stream, "OK\n");
	return SWITCH_STATUS_SUCCESS;

usage:
	stream->write_function(stream, "Usage: flags <call_id> <[+|-][mouth|ear]>\n");
	return SWITCH_STATUS_FALSE;
}

static switch_status_t list_calls(char **argv, int argc, switch_stream_handle_t *stream)
{
	switch_hash_index_t *ci;
	void *val;
	int x = 0;
	const char *cid_name = "n/a";
	const char *cid_num = "n/a";

	switch_mutex_lock(globals.call_hash_mutex);
	for (ci = switch_core_hash_first(globals.call_hash); ci; ci = switch_core_hash_next(&ci)) {
		private_t *tech_pvt;
		switch_channel_t *channel;
		switch_caller_profile_t *profile;

		switch_core_hash_this(ci, NULL, NULL, &val);
		tech_pvt = (private_t *)val;
		x++;
		channel = switch_core_session_get_channel(tech_pvt->session);

		if ((profile = switch_channel_get_caller_profile(channel))) {
			if (profile->originatee_caller_profile) {
				cid_name = "Outbound Call";
				cid_num = profile->originatee_caller_profile->destination_number;
			} else {
				cid_name = profile->caller_id_name;
				cid_num = profile->caller_id_number;
			}
		}

		stream->write_function(stream, "%s %s [%s (%s)]\n", tech_pvt->call_id, switch_channel_get_name(channel),
							   cid_name, cid_num);
	}
	switch_mutex_unlock(globals.call_hash_mutex);

	stream->write_function(stream, "\n%d call%s\n", x, x == 1 ? "" : "s");

	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t place_call(char **argv, int argc, switch_stream_handle_t *stream)
{
	switch_core_session_t *session;
	char *dest = NULL;

	if (zstr(argv[0])) {
		stream->write_function(stream, "FAIL:Usage: call <dest>\n");
		return SWITCH_STATUS_SUCCESS;
	}
	dest = argv[0];

	if ((session = switch_core_session_request(pulseaudio_endpoint_interface, SWITCH_CALL_DIRECTION_INBOUND, SOF_NONE, NULL)) != 0) {
		private_t *tech_pvt;
		switch_channel_t *channel;
		char *dialplan = globals.dialplan;
		char *context = globals.context;
		char *cid_name = globals.cid_name;
		char *cid_num = globals.cid_num;
		char ip[25] = "0.0.0.0";

		switch_core_session_add_stream(session, NULL);
		tech_pvt = create_tech_pvt(session);

		if (tech_pvt == NULL) {
			switch_core_session_destroy(&session);
			return SWITCH_STATUS_MEMERR;
		}
		channel = switch_core_session_get_channel(session);
		switch_core_session_set_private(session, tech_pvt);

		if (!zstr(argv[1]))
			dialplan = argv[1];

		if (!zstr(argv[2]))
			context = argv[2];

		if (!zstr(argv[3]))
			cid_num = argv[3];

		if (!zstr(argv[4]))
			cid_name = argv[4];

		if (!zstr(argv[5]))
			tech_pvt->sample_rate = atoi(argv[5]);

		if (!zstr(argv[6]))
			tech_pvt->codec_ms = atoi(argv[6]);

		switch_find_local_ip(ip, sizeof(ip), NULL, AF_INET);

		if ((tech_pvt->caller_profile = switch_caller_profile_new(switch_core_session_get_pool(session),
																  NULL, dialplan, cid_name, cid_num, ip, NULL, NULL, NULL, modname, context, dest)) != 0) {
			char name[128];
			switch_snprintf(name, sizeof(name), "pulseaudio/%s",
							tech_pvt->caller_profile->destination_number ? tech_pvt->caller_profile->destination_number : modname);
			switch_channel_set_name(channel, name);
			switch_channel_set_caller_profile(channel, tech_pvt->caller_profile);
		}
		tech_pvt->voice_endpoint = create_audio_endpoint(session, MOD_PA_STREAM_VOICE);
		if (tech_pvt->voice_endpoint) {
			switch_core_session_set_read_codec(session, &tech_pvt->voice_endpoint->read_codec);
			switch_core_session_set_write_codec(session, &tech_pvt->voice_endpoint->write_codec);
			switch_set_flag_locked(tech_pvt, TFLAG_ANSWER);
			switch_channel_mark_answered(channel);
			switch_channel_set_state(channel, CS_INIT);
			if (switch_core_session_thread_launch(tech_pvt->session) != SWITCH_STATUS_SUCCESS) {
				switch_event_t *event;
				switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_CRIT, "Error spawning thread\n");
				switch_core_session_destroy(&session);
				stream->write_function(stream, "FAIL:Thread Error!\n");
				if (switch_event_create_subclass(&event, SWITCH_EVENT_CUSTOM, MY_EVENT_MAKE_CALL) == SWITCH_STATUS_SUCCESS) {
					char buf[512];
					switch_channel_event_set_data(channel, event);
					switch_snprintf(buf, sizeof(buf), "Thread error!.\n");
					switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "error", buf);
					switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "fail", "true");
					switch_event_fire(&event);
				}
			} else {
				switch_event_t *event;
				stream->write_function(stream, "SUCCESS:%s:%s\n", tech_pvt->call_id, switch_core_session_get_uuid(tech_pvt->session));
				if (switch_event_create_subclass(&event, SWITCH_EVENT_CUSTOM, MY_EVENT_MAKE_CALL) == SWITCH_STATUS_SUCCESS) {
					switch_channel_event_set_data(channel, event);
					switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "fail", "false");
					switch_event_fire(&event);
				}
			}
		} else {
			switch_event_t *event;
			switch_core_session_destroy(&session);
			stream->write_function(stream, "FAIL:Device Error!\n");
			if (switch_event_create_subclass(&event, SWITCH_EVENT_CUSTOM, MY_EVENT_MAKE_CALL) == SWITCH_STATUS_SUCCESS) {
				char buf[512];
				switch_channel_event_set_data(channel, event);
				switch_snprintf(buf, sizeof(buf), "Device fail.\n");
				switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "error", buf);
				switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "fail", "true");
				switch_event_fire(&event);
			}
			return SWITCH_STATUS_FALSE;
		}
	}

	return SWITCH_STATUS_SUCCESS;
}

SWITCH_STANDARD_API(pa_cmd)
{
	char *argv[1024] = { 0 };
	int argc = 0;
	char *mycmd = NULL;
	switch_status_t status = SWITCH_STATUS_SUCCESS;
	pa_command_t func = NULL;
	int lead = 1;
	char *wcmd = NULL, *action = NULL;
	char cmd_buf[1024] = "";
	char *http = NULL;

	const char *usage_string = "USAGE:\n"
		"--------------------------------------------------------------------------------\n"
		"pulse help\n"
		"pulse call <dest> [<dialplan> <context> <cid_name> <cid_num> <sample_rate> <codec_ms>]\n"
		"pulse answer <call_id>\n"
		"pulse hangup <call_id>\n"
		"pulse list\n"
		"pulse switch <call_id>|none\n"
		"pulse dtmf <call_id> <digit string>\n"
		"pulse flags <call_id> <[+|-][ear|mouth]>\n"
		"pulse play [ringtest|<filename>] [seconds] [no_close]\n"
		"pulse ringfile [filename]\n"
		"pulse looptest\n"
		"--------------------------------------------------------------------------------\n";


	if (stream->param_event) {
		http = switch_event_get_header(stream->param_event, "http-host");
	}

	if (http) {
		stream->write_function(stream, "Content-type: text/html\n\n");

		wcmd = switch_str_nil(switch_event_get_header(stream->param_event, "wcmd"));
		action = switch_event_get_header(stream->param_event, "action");

		if (action) {
			if (strlen(action) == 1) {
				switch_snprintf(cmd_buf, sizeof(cmd_buf), "dtmf %s", action);
				cmd = cmd_buf;
			} else if (!strcmp(action, "mute")) {
				switch_snprintf(cmd_buf, sizeof(cmd_buf), "flags off mouth");
				cmd = cmd_buf;
			} else if (!strcmp(action, "unmute")) {
				switch_snprintf(cmd_buf, sizeof(cmd_buf), "flags on mouth");
				cmd = cmd_buf;
			} else if (!strcmp(action, "switch")) {
				switch_snprintf(cmd_buf, sizeof(cmd_buf), "switch %s", wcmd);
				cmd = cmd_buf;
			} else if (!strcmp(action, "call")) {
				switch_snprintf(cmd_buf, sizeof(cmd_buf), "call %s", wcmd);
				cmd = cmd_buf;
			} else if (!strcmp(action, "hangup") || !strcmp(action, "list") || !strcmp(action, "answer")) {
				cmd = action;
			}
		}

		if (zstr(cmd)) {
			goto done;
		}

	} else {

		if (zstr(cmd)) {
			stream->write_function(stream, "%s", usage_string);
			goto done;
		}
	}

	if (!(mycmd = strdup(cmd))) {
		status = SWITCH_STATUS_MEMERR;
		goto done;
	}

	if (!(argc = switch_separate_string(mycmd, ' ', argv, (sizeof(argv) / sizeof(argv[0]))))) {
		stream->write_function(stream, "%s", usage_string);
		goto done;
	}

	if (!argv[0]) {
		stream->write_function(stream, "Unknown Command\n");
		goto done;
	}

	if (!strcasecmp(argv[0], "call")) {
		func = place_call;
	} else if (!strcasecmp(argv[0], "help")) {
		stream->write_function(stream, "%s", usage_string);
		goto done;
	} else if (!strcasecmp(argv[0], "list")) {
		func = list_calls;
	} else if (!strcasecmp(argv[0], "flags")) {
		func = do_flags;
	} else if (!strcasecmp(argv[0], "hangup")) {
		func = hangup_call;
	} else if (!strcasecmp(argv[0], "answer")) {
		func = answer_call;
	} else if (!strcasecmp(argv[0], "switch")) {
		func = switch_call;
	} else if (!strcasecmp(argv[0], "dtmf")) {
		func = dtmf_call;
	} else if ((argv[1] && !strcmp(argv[0], "play"))) {
		play_dev(stream, argv[1], argv[2], argv[3]);
	} else if (!strcasecmp(argv[0], "looptest")) {
		func = looptest;
	} else if (!strcasecmp(argv[0], "ringfile")) {
		func = set_ringfile;
	} else {
		stream->write_function(stream, "Unknown Command or not enough args [%s]\n", argv[0]);
	}


	if (func) {
		if (http) {
			stream->write_function(stream, "<pre>");
		}

		status = func(&argv[lead], argc - lead, stream);
		status = SWITCH_STATUS_SUCCESS; /*if func was defined we want to always return success as the command was found */

		if (http) {
			stream->write_function(stream, "\n\n</pre>");
		}
	}

  done:
	if (http) {
		stream->write_function(stream,
							   "<br><br><table align=center><tr><td><center><form method=post>\n"
							   "<input type=text name=wcmd size=40><br><br>\n"
							   "<input name=action type=submit value=\"call\"> "
							   "<input name=action type=submit value=\"hangup\"> "
							   "<input name=action type=submit value=\"list\"> "
							   "<input name=action type=submit value=\"switch\"> "
							   "<input name=action type=submit value=\"mute\"> "
							   "<input name=action type=submit value=\"unmute\"> "
							   "<input name=action type=submit value=\"play\"> "
							   "<input name=action type=submit value=\"answer\"> <br><br>"
							   "<table border=1>\n"
							   "<tr><td><input name=action type=submit value=\"1\"></td>"
							   "<td><input name=action type=submit value=\"2\"></td>"
							   "<td><input name=action type=submit value=\"3\"></td>\n"
							   "<td><input name=action type=submit value=\"A\"></td></tr>\n"
							   "<tr><td><input name=action type=submit value=\"4\"></td>"
							   "<td><input name=action type=submit value=\"5\"></td>"
							   "<td><input name=action type=submit value=\"6\"></td>\n"
							   "<td><input name=action type=submit value=\"B\"></td></tr>\n"
							   "<tr><td><input name=action type=submit value=\"7\"></td>"
							   "<td><input name=action type=submit value=\"8\"></td>"
							   "<td><input name=action type=submit value=\"9\"></td>\n"
							   "<td><input name=action type=submit value=\"C\"></td></tr>\n"
							   "<tr><td><input name=action type=submit value=\"*\"></td>"
							   "<td><input name=action type=submit value=\"0\"></td>"
							   "<td><input name=action type=submit value=\"#\"></td>\n"
							   "<td><input name=action type=submit value=\"D\"></td></tr>\n" "</table>" "</form><br></center></td></tr></table>\n");
	}

	switch_safe_free(mycmd);
	return status;
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
