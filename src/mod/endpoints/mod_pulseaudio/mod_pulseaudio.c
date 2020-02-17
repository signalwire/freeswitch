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
 * Moises Silva <moises.silva@gmail.com> (Multiple endpoints work sponsored by Comrex Corporation)
 * Raymond Chandler <intralanman@freeswitch.org>
 *
 *
 * mod_pulseaudio.c -- PulseAudio Endpoint Module
 * based on mod_portaudio.c a0e19e1c7f2fa9256eaf76be346f50302eeed51f
 *
 */

#include "switch.h"

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include "pablio.h"

#define MY_EVENT_RINGING "pulseaudio::ringing"
#define MY_EVENT_MAKE_CALL "pulseaudio::makecall"
#define MY_EVENT_CALL_HELD "pulseaudio::callheld"
#define MY_EVENT_CALL_RESUMED "pulseaudio::callresumed"
#define MY_EVENT_ERROR_AUDIO_DEV "pulseaudio::audio_dev_error"
#define SWITCH_PA_CALL_ID_VARIABLE "pa_call_id"

#define MIN_STREAM_SAMPLE_RATE 8000
#define MAX_IO_CHANNELS 2
#define STREAM_SAMPLES_PER_PACKET(stream) ((stream->codec_ms * stream->sample_rate) / 1000)

SWITCH_MODULE_LOAD_FUNCTION(mod_pulseaudio_load);
SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_pulseaudio_shutdown);
//SWITCH_MODULE_RUNTIME_FUNCTION(mod_pulseaudio_runtime);
SWITCH_MODULE_DEFINITION(mod_pulseaudio, mod_pulseaudio_load, mod_pulseaudio_shutdown, NULL);

static switch_memory_pool_t *module_pool = NULL;
switch_endpoint_interface_t *pulseaudio_endpoint_interface;

typedef switch_status_t (*pa_command_t) (char **argv, int argc, switch_stream_handle_t *stream);

typedef enum {
	GFLAG_NONE = 0,
	GFLAG_EAR = (1 << 0),
	GFLAG_MOUTH = (1 << 1),
	GFLAG_RING = (1 << 2)
} GFLAGS;

typedef enum {
	TFLAG_IO = (1 << 0),
	TFLAG_INBOUND = (1 << 1),
	TFLAG_OUTBOUND = (1 << 2),
	TFLAG_DTMF = (1 << 3),
	TFLAG_VOICE = (1 << 4),
	TFLAG_HANGUP = (1 << 5),
	TFLAG_LINEAR = (1 << 6),
	TFLAG_ANSWER = (1 << 7),
	TFLAG_HUP = (1 << 8),
	TFLAG_MASTER = (1 << 9),
	TFLAG_AUTO_ANSWER = (1 << 10)
} TFLAGS;

typedef enum {
	STREAM_VOICE = 0,
	STREAM_RING = 1,
	STREAM_PLAYBACK = 2,
} STREAMS;

struct audio_stream {
	PABLIO_Stream *stream;
	switch_timer_t write_timer;
	struct audio_stream *next;
};
typedef struct audio_stream audio_stream_t;

/* Audio stream that can be shared across endpoints */
typedef struct _shared_audio_stream_t {
	/*! Friendly name for this stream */
	char name[255];
	/*! Sampling rate */
	int sample_rate;
	/*! Buffer packetization (and therefore timing) */
	int codec_ms;
	/*! Input channels being used */
	uint8_t inchan_used[MAX_IO_CHANNELS];
	/*! Output channels being used */
	uint8_t outchan_used[MAX_IO_CHANNELS];
	/*! How many channels to create (for input and output) */
	int channels;
	/*! The io stream helper to buffer audio */
	PABLIO_Stream *stream;
	/* It can be shared after all :-)  */
	switch_mutex_t *mutex;
} shared_audio_stream_t;

typedef struct private_object private_t;
/* Endpoint that can be called via pulseaudio/endpoint/<endpoint-name> */
typedef struct _audio_endpoint {
	/*! Friendly name for this endpoint */
	char name[255];

	/*! Input stream for this endpoint */
	shared_audio_stream_t *in_stream;

	/*! Output stream for this endpoint */
	shared_audio_stream_t *out_stream;

	/*! Channel index within the input stream where we get the audio for this endpoint */
	int inchan;

	/*! Channel index within the output stream where we get the audio for this endpoint */
	int outchan;

	/*! Associated private information if involved in a call */
	private_t *master;

	/*! For timed read and writes */
	switch_timer_t read_timer;
	switch_timer_t write_timer;

	/* We need our own read frame */
	switch_frame_t read_frame;
	unsigned char read_buf[SWITCH_RECOMMENDED_BUFFER_SIZE];

	/* Needed codecs for the core to read/write in the proper format */
	switch_codec_t read_codec;
	switch_codec_t write_codec;

	/*! Let's be safe */
	switch_mutex_t *mutex;
} audio_endpoint_t;

struct private_object {
	unsigned int flags;
	switch_core_session_t *session;
	switch_caller_profile_t *caller_profile;
	char call_id[50];
	int sample_rate;
	int codec_ms;
	switch_mutex_t *flag_mutex;
	char *hold_file;
	switch_file_handle_t fh;
	switch_file_handle_t *hfh;
	switch_frame_t hold_frame;
	unsigned char holdbuf[SWITCH_RECOMMENDED_BUFFER_SIZE];
	audio_endpoint_t *audio_endpoint;
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
	char *hold_file;
	char *timer_name;
	int call_id;
	int unload_device_fail;
	switch_hash_t *call_hash;
	switch_mutex_t *device_lock;
	switch_mutex_t *pvt_lock;
	switch_mutex_t *streams_lock;
	switch_mutex_t *flag_mutex;
	switch_mutex_t *pa_mutex;
	int sample_rate;
	int codec_ms;
	audio_stream_t *main_stream;
	audio_stream_t *ring_stream;
	audio_stream_t *play_stream;
	switch_codec_t read_codec;
	switch_codec_t write_codec;
	switch_frame_t read_frame;
	switch_frame_t cng_frame;
	unsigned char databuf[SWITCH_RECOMMENDED_BUFFER_SIZE];
	unsigned char cngbuf[SWITCH_RECOMMENDED_BUFFER_SIZE];
	private_t *call_list;
	/*! Streams that can be used by multiple endpoints at the same time */
	switch_hash_t *sh_streams;
	/*! Endpoints configured */
	switch_hash_t *endpoints;
	int ring_interval;
	GFLAGS flags;
	switch_timer_t read_timer;
	switch_timer_t readfile_timer;
	switch_timer_t hold_timer;
	time_t deactivate_timer;
	int live_stream_switch;
	int no_auto_resume_call;
	int no_ring_during_call;
	int codecs_inited;
	int destroying_streams;
} globals;


#define PA_MASTER 1
#define PA_SLAVE 0


SWITCH_DECLARE_GLOBAL_STRING_FUNC(set_global_dialplan, globals.dialplan);
SWITCH_DECLARE_GLOBAL_STRING_FUNC(set_global_context, globals.context);
SWITCH_DECLARE_GLOBAL_STRING_FUNC(set_global_cid_name, globals.cid_name);
SWITCH_DECLARE_GLOBAL_STRING_FUNC(set_global_cid_num, globals.cid_num);
SWITCH_DECLARE_GLOBAL_STRING_FUNC(set_global_ring_file, globals.ring_file);
SWITCH_DECLARE_GLOBAL_STRING_FUNC(set_global_hold_file, globals.hold_file);
SWITCH_DECLARE_GLOBAL_STRING_FUNC(set_global_timer_name, globals.timer_name);
#define is_master(t) switch_test_flag(t, TFLAG_MASTER)

static void add_pvt(private_t *tech_pvt, int master);
static void remove_pvt(private_t *tech_pvt);
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

static switch_status_t create_codecs(int restart);
static void create_hold_event(private_t *tech_pvt, int unhold);
static audio_stream_t * get_audio_stream(STREAMS stream_number);
static audio_stream_t * create_audio_stream(STREAMS stream_number);
pa_error open_audio_stream(PABLIO_Stream **stream, const char * channelName, const pa_sample_spec * inputParameters, const pa_sample_spec * outputParameters);
static switch_status_t destroy_actual_stream(audio_stream_t *stream);
static void destroy_audio_streams();
static switch_status_t validate_main_audio_stream();
static switch_status_t validate_ring_audio_stream();
static switch_status_t validate_playback_audio_stream();

static switch_status_t load_config(void);
SWITCH_STANDARD_API(pa_cmd);

/* 
   State methods they get called when the state changes to the specific state 
   returning SWITCH_STATUS_SUCCESS tells the core to execute the standard state method next
   so if you fully implement the state you can return SWITCH_STATUS_FALSE to skip it.
*/
static switch_status_t channel_on_init(switch_core_session_t *session)
{
	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t channel_on_routing(switch_core_session_t *session)
{

	switch_channel_t *channel = switch_core_session_get_channel(session);
	private_t *tech_pvt = switch_core_session_get_private(session);
	switch_time_t last;
	int waitsec = globals.ring_interval * 1000000;
	switch_file_handle_t fh = { 0 };
	const char *val, *ring_file = NULL, *hold_file = NULL;
	int16_t abuf[2048];

	switch_assert(tech_pvt != NULL);

	last = switch_micro_time_now() - waitsec;

	if ((val = switch_channel_get_variable(channel, "pa_hold_file"))) {
		hold_file = val;
	} else {
		hold_file = globals.hold_file;
	}

	if (hold_file) {
		tech_pvt->hold_file = switch_core_session_strdup(session, hold_file);
	}
	if (switch_test_flag(tech_pvt, TFLAG_OUTBOUND)) {
		if (!tech_pvt->audio_endpoint && validate_main_audio_stream() != SWITCH_STATUS_SUCCESS) {
			switch_channel_hangup(channel, SWITCH_CAUSE_DESTINATION_OUT_OF_ORDER);
			return SWITCH_STATUS_FALSE;
		}

		if (!tech_pvt->audio_endpoint &&
		    switch_test_flag(tech_pvt, TFLAG_OUTBOUND) && 
		    !switch_test_flag(tech_pvt, TFLAG_AUTO_ANSWER)) {

			add_pvt(tech_pvt, PA_SLAVE);

			ring_file = globals.ring_file;
			if ((val = switch_channel_get_variable(channel, "pa_ring_file"))) {
				ring_file = val;
			}

			if (switch_test_flag((&globals), GFLAG_RING)) {
				ring_file = NULL;
			}
			switch_set_flag_locked((&globals), GFLAG_RING);
			if (ring_file) {
				if (switch_core_file_open(&fh,
										  ring_file,
										  globals.read_codec.implementation->number_of_channels,
										  globals.read_codec.implementation->actual_samples_per_second,
										  SWITCH_FILE_FLAG_READ | SWITCH_FILE_DATA_SHORT, NULL) == SWITCH_STATUS_SUCCESS) {

					if (validate_ring_audio_stream() != SWITCH_STATUS_SUCCESS) {
						switch_channel_hangup(channel, SWITCH_CAUSE_DESTINATION_OUT_OF_ORDER);
						switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "Ring Error!\n");
						switch_core_file_close(&fh);
						return SWITCH_STATUS_GENERR;
					}
				} else {
					switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "Cannot open %s, disabling ring file!\n", ring_file);
					ring_file = NULL;
				}
			}
		}

		if (tech_pvt->audio_endpoint || switch_test_flag(tech_pvt, TFLAG_AUTO_ANSWER)) {
			switch_mutex_lock(globals.pvt_lock);
			add_pvt(tech_pvt, PA_MASTER);
			if (switch_test_flag(tech_pvt, TFLAG_AUTO_ANSWER)) {
				switch_channel_mark_answered(channel);
				switch_set_flag(tech_pvt, TFLAG_ANSWER);
			}
			switch_mutex_unlock(globals.pvt_lock);
			// This will add one second latency on PulseAudio
			//switch_yield(1000000);
		} else {
			switch_channel_mark_ring_ready(channel);
		}

		while (switch_channel_get_state(channel) == CS_ROUTING && 
		       !switch_channel_test_flag(channel, CF_ANSWERED) &&
		       !switch_test_flag(tech_pvt, TFLAG_ANSWER)) {
			switch_size_t olen = globals.readfile_timer.samples;

			if (switch_micro_time_now() - last >= waitsec) {
				char buf[512];
				switch_event_t *event;

				switch_snprintf(buf, sizeof(buf), "BRRRRING! BRRRRING! call %s\n", tech_pvt->call_id);

				if (switch_event_create_subclass(&event, SWITCH_EVENT_CUSTOM, MY_EVENT_RINGING) == SWITCH_STATUS_SUCCESS) {
					switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "event_info", buf);
					switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "call_id", tech_pvt->call_id); /* left behind for backwards compatability */
					switch_channel_set_variable(channel, SWITCH_PA_CALL_ID_VARIABLE, tech_pvt->call_id);
					switch_channel_event_set_data(channel, event);
					switch_event_fire(&event);
				}
				switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "%s\n", buf);
				last = switch_micro_time_now();
			}

			if (ring_file) {
				if (switch_core_timer_next(&globals.readfile_timer) != SWITCH_STATUS_SUCCESS) {
					switch_core_file_close(&fh);
					break;
				}
				switch_core_file_read(&fh, abuf, &olen);
				if (olen == 0) {
					unsigned int pos = 0;
					switch_core_file_seek(&fh, &pos, 0, SEEK_SET);
				}

				if (globals.ring_stream && (! switch_test_flag(globals.call_list, TFLAG_MASTER) || 
							( !globals.no_ring_during_call )) ) { //if there is a ring stream and not an active call or if there is an active call and we are allowed to ring during it AND the ring stream is not the main stream						
						WriteAudioStream(globals.ring_stream->stream, abuf, olen*2, 0, &globals.ring_stream->write_timer);
				}
			} else {
				switch_yield(10000);
			}
		}
		switch_clear_flag_locked((&globals), GFLAG_RING);
	}

	if (ring_file) {
		switch_core_file_close(&fh);
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

static void destroy_audio_streams()
{
	globals.destroying_streams = 1;
	destroy_actual_stream(globals.ring_stream);
	destroy_actual_stream(globals.main_stream);
	destroy_actual_stream(globals.play_stream);
	globals.destroying_streams = 0;
}

static switch_status_t validate_main_audio_stream()
{
	if (globals.read_timer.timer_interface) {
		switch_core_timer_sync(&globals.read_timer);
	}

	if (globals.main_stream) {
		if (globals.main_stream->write_timer.timer_interface) {
			switch_core_timer_sync(&(globals.main_stream->write_timer));
		}

		return SWITCH_STATUS_SUCCESS;
	}

	globals.main_stream = get_audio_stream(STREAM_VOICE);
	
	if (globals.main_stream) {
		return SWITCH_STATUS_SUCCESS;
	}

	return SWITCH_STATUS_FALSE;
}

static switch_status_t validate_ring_audio_stream()
{
	if (globals.ring_stream) {
		if (globals.ring_stream->write_timer.timer_interface) {
			switch_core_timer_sync(&(globals.ring_stream->write_timer));
		}
		return SWITCH_STATUS_SUCCESS;
	}
	globals.ring_stream = get_audio_stream(STREAM_RING);
	if (globals.ring_stream) {
		return SWITCH_STATUS_SUCCESS;
	}

	return SWITCH_STATUS_FALSE;
}

static switch_status_t validate_playback_audio_stream()
{
	if (globals.play_stream) {
		if (globals.play_stream->write_timer.timer_interface) {
			switch_core_timer_sync(&(globals.play_stream->write_timer));
		}
		return SWITCH_STATUS_SUCCESS;
	}
	globals.play_stream = get_audio_stream(STREAM_PLAYBACK);
	if (globals.play_stream) {
		return SWITCH_STATUS_SUCCESS;
	}

	return SWITCH_STATUS_FALSE;
}

static switch_status_t destroy_actual_stream(audio_stream_t *stream)
{
	if (stream == NULL) {
		return SWITCH_STATUS_FALSE;
	}

	if (globals.main_stream == stream) {
		globals.main_stream = NULL;
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Removing voice stream\n");
	}

	if (globals.ring_stream == stream) {
		globals.ring_stream = NULL;
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Removing ring stream\n");
	}

	if (globals.play_stream == stream) {
		globals.play_stream = NULL;
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Removing playback stream\n");
	}

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Closing audio stream\n");
	CloseAudioStream(stream->stream);
	stream->stream = NULL;
	
	if (stream->write_timer.timer_interface) {
		switch_core_timer_destroy(&stream->write_timer);
	}

	switch_safe_free(stream);
	return SWITCH_STATUS_SUCCESS;
}

static void destroy_codecs(void)
{

	if (switch_core_codec_ready(&globals.read_codec)) {
		switch_core_codec_destroy(&globals.read_codec);
	}

	if (switch_core_codec_ready(&globals.write_codec)) {
		switch_core_codec_destroy(&globals.write_codec);
	}

	if (globals.read_timer.timer_interface) {
		switch_core_timer_destroy(&globals.read_timer);
	}

	if (globals.readfile_timer.timer_interface) {
		switch_core_timer_destroy(&globals.readfile_timer);
	}

	if (globals.hold_timer.timer_interface) {
		switch_core_timer_destroy(&globals.hold_timer);
	}

	globals.codecs_inited = 0;
}

static void create_hold_event(private_t *tech_pvt, int unhold)
{
	switch_event_t *event;
	char * event_id;

	if (unhold) {
		event_id = MY_EVENT_CALL_RESUMED;
	} else {
		event_id = MY_EVENT_CALL_HELD;
	}

	if (switch_event_create_subclass(&event, SWITCH_EVENT_CUSTOM, event_id) == SWITCH_STATUS_SUCCESS) {
		switch_channel_event_set_data(switch_core_session_get_channel(tech_pvt->session), event);
		switch_event_fire(&event);
	}
}

static void add_pvt(private_t *tech_pvt, int master)
{
	private_t *tp;
	uint8_t in_list = 0;

	switch_mutex_lock(globals.pvt_lock);

	if (*tech_pvt->call_id == '\0') {
		switch_mutex_lock(globals.pa_mutex);
		switch_snprintf(tech_pvt->call_id, sizeof(tech_pvt->call_id), "%d", ++globals.call_id);
		switch_channel_set_variable(switch_core_session_get_channel(tech_pvt->session), SWITCH_PA_CALL_ID_VARIABLE, tech_pvt->call_id);
		switch_core_hash_insert(globals.call_hash, tech_pvt->call_id, tech_pvt);
		if (!tech_pvt->audio_endpoint) {
			switch_core_session_set_read_codec(tech_pvt->session, &globals.read_codec);
			switch_core_session_set_write_codec(tech_pvt->session, &globals.write_codec);
		}
		switch_mutex_unlock(globals.pa_mutex);
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(tech_pvt->session), SWITCH_LOG_DEBUG, "Added call %s\n", tech_pvt->call_id);
	}

	for (tp = globals.call_list; tp; tp = tp->next) {
		if (tp == tech_pvt) {
			in_list = 1;
		}
		if (master && switch_test_flag(tp, TFLAG_MASTER) ) {
			switch_clear_flag_locked(tp, TFLAG_MASTER);
			create_hold_event(tp, 0);
		}
	}


	if (master) {
		if (!in_list) {
			tech_pvt->next = globals.call_list;
			globals.call_list = tech_pvt;
		}
		switch_set_flag_locked(tech_pvt, TFLAG_MASTER);

	} else if (!in_list) {
		for (tp = globals.call_list; tp && tp->next; tp = tp->next);
		if (tp) {
			tp->next = tech_pvt;
		} else {
			globals.call_list = tech_pvt;
		}
	}

	switch_mutex_unlock(globals.pvt_lock);
}

static void remove_pvt(private_t *tech_pvt)
{
	private_t *tp, *last = NULL;
	int was_master = 0;

	switch_mutex_lock(globals.pvt_lock);
	for (tp = globals.call_list; tp; tp = tp->next) {
		
		if (tp == tech_pvt) {
			if (switch_test_flag(tp, TFLAG_MASTER)) {
				switch_clear_flag_locked(tp, TFLAG_MASTER);
				was_master = 1;
			}
			if (last) {
				last->next = tp->next;
			} else {
				globals.call_list = tp->next;
			}
		}
		last = tp;
	}

	if (globals.call_list) {
		if (was_master && ! globals.no_auto_resume_call) {
			switch_set_flag_locked(globals.call_list, TFLAG_MASTER);
			create_hold_event(globals.call_list, 1);
		}
	} else {
		globals.deactivate_timer = switch_epoch_time_now(NULL) + 2;
		destroy_audio_streams();
	}

	switch_mutex_unlock(globals.pvt_lock);
}

static void tech_close_file(private_t *tech_pvt)
{
	if (tech_pvt->hfh) {
		tech_pvt->hfh = NULL;
		switch_core_file_close(&tech_pvt->fh);
	}
}

static switch_status_t channel_on_destroy(switch_core_session_t *session)
{
	//private_t *tech_pvt = switch_core_session_get_private(session);
	//switch_assert(tech_pvt != NULL);
	return SWITCH_STATUS_SUCCESS;
}

static int release_stream_channel(shared_audio_stream_t *stream, int index, int input);
static switch_status_t channel_on_hangup(switch_core_session_t *session)
{
	private_t *tech_pvt = switch_core_session_get_private(session);
	switch_assert(tech_pvt != NULL);

	if (tech_pvt->audio_endpoint) {
		audio_endpoint_t *endpoint = tech_pvt->audio_endpoint;

		tech_pvt->audio_endpoint = NULL;

		switch_mutex_lock(endpoint->mutex);

		release_stream_channel(endpoint->in_stream, endpoint->inchan, 1);
		release_stream_channel(endpoint->out_stream, endpoint->outchan, 0);
		switch_core_timer_destroy(&endpoint->read_timer);
		switch_core_timer_destroy(&endpoint->write_timer);
		switch_core_codec_destroy(&endpoint->read_codec);
		switch_core_codec_destroy(&endpoint->write_codec);
		endpoint->master = NULL;

		switch_mutex_unlock(endpoint->mutex);
	}

	switch_mutex_lock(globals.pa_mutex);
	switch_core_hash_delete(globals.call_hash, tech_pvt->call_id);
	switch_mutex_unlock(globals.pa_mutex);

	switch_clear_flag_locked(tech_pvt, TFLAG_IO);
	switch_set_flag_locked(tech_pvt, TFLAG_HUP);

	remove_pvt(tech_pvt);

	tech_close_file(tech_pvt);

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

static switch_status_t channel_endpoint_read(audio_endpoint_t *endpoint, switch_frame_t **frame)
{
	int datalen = 0;

	if (!endpoint->in_stream) {
		switch_core_timer_next(&endpoint->read_timer);
		*frame = &globals.cng_frame;
		return SWITCH_STATUS_SUCCESS;
	}

	datalen = ReadAudioStream(endpoint->in_stream->stream, 
			endpoint->read_frame.data, STREAM_SAMPLES_PER_PACKET(endpoint->in_stream)*sizeof(SAMPLE),
			endpoint->inchan, &endpoint->read_timer);

	if (!datalen) {
		switch_core_timer_next(&endpoint->read_timer);
		*frame = &globals.cng_frame;
		return SWITCH_STATUS_SUCCESS;
	}

	endpoint->read_frame.datalen = datalen;
	endpoint->read_frame.samples = (datalen / sizeof(SAMPLE));
	endpoint->read_frame.codec = &endpoint->read_codec;
	*frame = &endpoint->read_frame;
	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t channel_read_frame(switch_core_session_t *session, switch_frame_t **frame, switch_io_flag_t flags, int stream_id)
{
	private_t *tech_pvt = switch_core_session_get_private(session);
	int datalen = 0;
	switch_status_t status = SWITCH_STATUS_FALSE;
	switch_assert(tech_pvt != NULL);

	if (tech_pvt->audio_endpoint) {
		return channel_endpoint_read(tech_pvt->audio_endpoint, frame);
	}
	
	if (!globals.main_stream) {
		goto normal_return;
	}

	if (switch_test_flag(tech_pvt, TFLAG_HUP)) {
		goto normal_return;
	}

	if (!switch_test_flag(tech_pvt, TFLAG_IO)) {
		goto cng_wait;
	}

	if (!is_master(tech_pvt)) {
		if (tech_pvt->hold_file) {
			switch_size_t olen = globals.read_codec.implementation->samples_per_packet;

			if (!tech_pvt->hfh) {
				int sample_rate = globals.sample_rate;
				if (switch_core_file_open(&tech_pvt->fh,
										  tech_pvt->hold_file,
										  globals.read_codec.implementation->number_of_channels,
										  globals.read_codec.implementation->actual_samples_per_second,
										  SWITCH_FILE_FLAG_READ | SWITCH_FILE_DATA_SHORT, NULL) != SWITCH_STATUS_SUCCESS) {
					tech_pvt->hold_file = NULL;
					goto cng_wait;
				}

				tech_pvt->hfh = &tech_pvt->fh;
				tech_pvt->hold_frame.data = tech_pvt->holdbuf;
				tech_pvt->hold_frame.buflen = sizeof(tech_pvt->holdbuf);
				tech_pvt->hold_frame.rate = sample_rate;
				tech_pvt->hold_frame.codec = &globals.write_codec;
			}

			if (switch_core_timer_next(&globals.hold_timer) != SWITCH_STATUS_SUCCESS) {
				switch_core_file_close(&tech_pvt->fh);
				goto cng_nowait;
			}
			switch_core_file_read(tech_pvt->hfh, tech_pvt->hold_frame.data, &olen);

			if (olen == 0) {
				unsigned int pos = 0;
				switch_core_file_seek(tech_pvt->hfh, &pos, 0, SEEK_SET);
				goto cng_nowait;
			}

			tech_pvt->hold_frame.datalen = (uint32_t) (olen * sizeof(int16_t));
			tech_pvt->hold_frame.samples = (uint32_t) olen;
			*frame = &tech_pvt->hold_frame;

			status = SWITCH_STATUS_SUCCESS;
			goto normal_return;
		}

		goto cng_wait;
	}

	if (tech_pvt->hfh) {
		tech_close_file(tech_pvt);
	}

	switch_mutex_lock(globals.device_lock);
	datalen = ReadAudioStream(globals.main_stream->stream, globals.read_frame.data, globals.read_codec.implementation->decoded_bytes_per_packet, 0, &globals.read_timer);
	switch_mutex_unlock(globals.device_lock);

	if (datalen) {
		globals.read_frame.datalen = datalen;
		globals.read_frame.samples = datalen * 2;

		*frame = &globals.read_frame;

		if (!switch_test_flag((&globals), GFLAG_MOUTH)) {
			memset(globals.read_frame.data, 255, globals.read_frame.datalen);
		}
		status = SWITCH_STATUS_SUCCESS;
	} else {
		goto cng_nowait;
	}

normal_return:
	return status;

  cng_nowait:
	*frame = &globals.cng_frame;
	return SWITCH_STATUS_SUCCESS;

  cng_wait:
	switch_core_timer_next(&globals.hold_timer);
	*frame = &globals.cng_frame;
	return SWITCH_STATUS_SUCCESS;

}

static switch_status_t channel_endpoint_write(audio_endpoint_t *endpoint, switch_frame_t *frame)
{
	if (!endpoint->out_stream) {
		switch_core_timer_next(&endpoint->write_timer);
		return SWITCH_STATUS_SUCCESS;
	}
	if (!endpoint->master) {
		return SWITCH_STATUS_SUCCESS;
	}
	if (switch_test_flag(endpoint->master, TFLAG_HUP)) {
		return SWITCH_STATUS_FALSE;
	}
	if (!switch_test_flag(endpoint->master, TFLAG_IO)) {
		return SWITCH_STATUS_SUCCESS;
	}
	WriteAudioStream(endpoint->out_stream->stream, (short *)frame->data, 
			frame->datalen, 
			endpoint->outchan, &(endpoint->write_timer));
	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t channel_write_frame(switch_core_session_t *session, switch_frame_t *frame, switch_io_flag_t flags, int stream_id)
{
	switch_status_t status = SWITCH_STATUS_FALSE;
	private_t *tech_pvt = switch_core_session_get_private(session);
	switch_assert(tech_pvt != NULL);

	if (tech_pvt->audio_endpoint) {
		return channel_endpoint_write(tech_pvt->audio_endpoint, frame);
	}

	if (!globals.main_stream) {
		return SWITCH_STATUS_FALSE;
	}

	if (switch_test_flag(tech_pvt, TFLAG_HUP)) {
		return SWITCH_STATUS_FALSE;
	}

	if (!is_master(tech_pvt) || !switch_test_flag(tech_pvt, TFLAG_IO)) {
		return SWITCH_STATUS_SUCCESS;
	}

	if (globals.main_stream) {
		if (switch_test_flag((&globals), GFLAG_EAR)) {
			WriteAudioStream(globals.main_stream->stream, (short *) frame->data, frame->datalen, 0, &(globals.main_stream->write_timer));
		}
		status = SWITCH_STATUS_SUCCESS;
	}

	return status;
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
	case SWITCH_MESSAGE_INDICATE_ANSWER:
		channel_answer_channel(session);
		break;
	case SWITCH_MESSAGE_INDICATE_PROGRESS:
		{
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "Engage Early Media\n");
			switch_set_flag_locked(tech_pvt, TFLAG_IO);
		}
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

static int create_shared_audio_stream(shared_audio_stream_t *stream, const char * channelName);
static int destroy_shared_audio_stream(shared_audio_stream_t *stream);
static int take_stream_channel(shared_audio_stream_t *stream, int index, int input)
{
	int rc = 0;
	if (!stream) {
		return rc;
	}

	switch_mutex_lock(stream->mutex);

	if (!stream->stream && create_shared_audio_stream(stream, "Shared")) {
		rc = -1;
		goto done;
	}

	if (input) {
	       	if (stream->inchan_used[index]) {
			rc = -1;
			goto done;
		}
		stream->inchan_used[index] = 1;
	} else {
		if (!input && stream->outchan_used[index]) {
			rc = -1;
			goto done;
		}
		stream->outchan_used[index] = 1;
	}

done:
	switch_mutex_unlock(stream->mutex);
	return rc;
}

static int release_stream_channel(shared_audio_stream_t *stream, int index, int input)
{
	int i = 0;
	int destroy_stream = 1;
	int rc = 0;

	if (!stream) {
		return rc;
	}
	
	switch_mutex_lock(stream->mutex);

	if (input) {
		stream->inchan_used[index] = 0;
	} else {
		stream->outchan_used[index] = 0;
	}

	for (i = 0; i < stream->channels; i++) {
		if (stream->inchan_used[i] || stream->outchan_used[i]) {
			destroy_stream = 0;
		}
	}
	if (destroy_stream) {
		destroy_shared_audio_stream(stream);
	}

	switch_mutex_unlock(stream->mutex);
	return rc;
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
	int codec_ms = -1;
	int samples_per_packet = -1;
	int sample_rate = 0;
	audio_endpoint_t *endpoint = NULL;
	char *endpoint_name = NULL;
	const char *endpoint_answer = NULL;

	if (!outbound_profile) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Missing caller profile\n");
		return retcause;
	}

	if (!(*new_session = switch_core_session_request_uuid(pulseaudio_endpoint_interface, SWITCH_CALL_DIRECTION_OUTBOUND, flags, pool, switch_event_get_header(var_event, "origination_uuid")))) {
		return retcause;
	}

	switch_core_session_add_stream(*new_session, NULL);
	if ((tech_pvt = (private_t *) switch_core_session_alloc(*new_session, sizeof(private_t))) != 0) {
		memset(tech_pvt, 0, sizeof(*tech_pvt));
		switch_mutex_init(&tech_pvt->flag_mutex, SWITCH_MUTEX_NESTED, switch_core_session_get_pool(*new_session));
		channel = switch_core_session_get_channel(*new_session);
		switch_core_session_set_private(*new_session, tech_pvt);
		tech_pvt->session = *new_session;
	} else {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(*new_session), SWITCH_LOG_CRIT, "Hey where is my memory pool?\n");
		switch_core_session_destroy(new_session);
		return retcause;
	}

	if (outbound_profile->destination_number && !strncasecmp(outbound_profile->destination_number, "endpoint", sizeof("endpoint")-1)) {
		codec_ms = -1;
		samples_per_packet = -1;
		endpoint = NULL;
		endpoint_name = switch_core_strdup(outbound_profile->pool, outbound_profile->destination_number);
		endpoint_name = strchr(endpoint_name, '/');
		if (!endpoint_name) {
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(*new_session), SWITCH_LOG_CRIT, "No pulseaudio endpoint specified\n");
			goto error;
		}
		endpoint_name++;
		endpoint = switch_core_hash_find(globals.endpoints, endpoint_name);
		if (!endpoint) {
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(*new_session), SWITCH_LOG_CRIT, "Invalid pulseaudio endpoint %s\n", endpoint_name);
			goto error;
		}
		
		switch_mutex_lock(endpoint->mutex);

		if (endpoint->master) {
			/* someone already has this endpoint */
			retcause = SWITCH_CAUSE_USER_BUSY;
			goto error;
		}

		codec_ms = endpoint->in_stream ? endpoint->in_stream->codec_ms : endpoint->out_stream->codec_ms;
		samples_per_packet = endpoint->in_stream ? 
			STREAM_SAMPLES_PER_PACKET(endpoint->in_stream) : STREAM_SAMPLES_PER_PACKET(endpoint->out_stream);
		sample_rate = endpoint->in_stream ? endpoint->in_stream->sample_rate : endpoint->out_stream->sample_rate;

		if (switch_core_timer_init(&endpoint->read_timer,
				   globals.timer_name, codec_ms, 
				   samples_per_packet, module_pool) != SWITCH_STATUS_SUCCESS) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "failed to setup read timer for endpoint '%s'!\n", endpoint->name);
			goto error;
		}

		/* The write timer must be setup regardless */
		if (switch_core_timer_init(&endpoint->write_timer,
				   globals.timer_name, codec_ms, 
				   samples_per_packet, module_pool) != SWITCH_STATUS_SUCCESS) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "failed to setup read timer for endpoint '%s'!\n", endpoint->name);
			goto error;
		}

		if (switch_core_codec_init(&endpoint->read_codec,
					"L16", NULL, sample_rate, codec_ms, 1, 
					SWITCH_CODEC_FLAG_ENCODE | SWITCH_CODEC_FLAG_DECODE, NULL, NULL) != SWITCH_STATUS_SUCCESS) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Can't load codec?\n");
			goto error;
		}

		if (switch_core_codec_init(&endpoint->write_codec,
					 "L16", NULL, sample_rate, codec_ms, 1, 
					 SWITCH_CODEC_FLAG_ENCODE | SWITCH_CODEC_FLAG_DECODE, NULL, NULL) != SWITCH_STATUS_SUCCESS) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Can't load codec?\n");
			goto error;
		}
		switch_core_session_set_read_codec(tech_pvt->session, &endpoint->read_codec);
		switch_core_session_set_write_codec(tech_pvt->session, &endpoint->write_codec);

		/* try to acquire the stream */	
		if (take_stream_channel(endpoint->in_stream, endpoint->inchan, 1)) {
			retcause = SWITCH_CAUSE_USER_BUSY;
			goto error;
		}
		if (take_stream_channel(endpoint->out_stream, endpoint->outchan, 0)) {
			release_stream_channel(endpoint->in_stream, endpoint->inchan, 1);
			retcause = SWITCH_CAUSE_USER_BUSY;
			goto error;
		}
		switch_snprintf(name, sizeof(name), "pulseaudio/endpoint-%s", endpoint_name);
		if (var_event && (endpoint_answer = (switch_event_get_header(var_event, "endpoint_answer")))) {
			if (switch_true(endpoint_answer)) {
				switch_set_flag(tech_pvt, TFLAG_AUTO_ANSWER);
			}
		} else {
			switch_set_flag(tech_pvt, TFLAG_AUTO_ANSWER);
		}
		endpoint->master = tech_pvt;
		tech_pvt->audio_endpoint = endpoint;
		switch_mutex_unlock(endpoint->mutex);
	} else {
		id = !zstr(outbound_profile->caller_id_number) ? outbound_profile->caller_id_number : "na";
		switch_snprintf(name, sizeof(name), "pulseaudio/%s", id);
		if (outbound_profile->destination_number && !strcasecmp(outbound_profile->destination_number, "auto_answer")) {
			switch_set_flag(tech_pvt, TFLAG_AUTO_ANSWER);
		}
	}
	switch_channel_set_name(channel, name);
	caller_profile = switch_caller_profile_clone(*new_session, outbound_profile);
	switch_channel_set_caller_profile(channel, caller_profile);
	tech_pvt->caller_profile = caller_profile;

	switch_set_flag_locked(tech_pvt, TFLAG_OUTBOUND);
	switch_channel_set_state(channel, CS_INIT);
	return SWITCH_CAUSE_SUCCESS;

error:
	if (endpoint) {
		if (!endpoint->master) {
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
		switch_mutex_unlock(endpoint->mutex);
	}
	if (new_session && *new_session) {
		switch_core_session_destroy(new_session);
	}
	return retcause;
}


SWITCH_MODULE_LOAD_FUNCTION(mod_pulseaudio_load)
{
	switch_status_t status;
	switch_api_interface_t *api_interface;

	module_pool = pool;

	memset(&globals, 0, sizeof(globals));

	switch_core_hash_init(&globals.call_hash);
	switch_core_hash_init(&globals.sh_streams);
	switch_core_hash_init(&globals.endpoints);
	switch_mutex_init(&globals.device_lock, SWITCH_MUTEX_NESTED, module_pool);
	switch_mutex_init(&globals.pvt_lock, SWITCH_MUTEX_NESTED, module_pool);
	switch_mutex_init(&globals.streams_lock, SWITCH_MUTEX_NESTED, module_pool);
	switch_mutex_init(&globals.flag_mutex, SWITCH_MUTEX_NESTED, module_pool);
	switch_mutex_init(&globals.pa_mutex, SWITCH_MUTEX_NESTED, module_pool);
	globals.codecs_inited = 0;
	globals.read_frame.data = globals.databuf;
	globals.read_frame.buflen = sizeof(globals.databuf);
	globals.cng_frame.data = globals.cngbuf;
	globals.cng_frame.buflen = sizeof(globals.cngbuf);
	switch_set_flag((&globals.cng_frame), SFF_CNG);
	globals.flags = GFLAG_EAR | GFLAG_MOUTH;

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
	if (switch_event_reserve_subclass(MY_EVENT_CALL_HELD) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Couldn't register subclass!\n");
		return SWITCH_STATUS_GENERR;
	}
	if (switch_event_reserve_subclass(MY_EVENT_CALL_RESUMED) != SWITCH_STATUS_SUCCESS) {
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
	switch_console_set_complete("add pulse preparestream");
	switch_console_set_complete("add pulse closestreams");
	switch_console_set_complete("add pulse ringfile");
	switch_console_set_complete("add pulse play");
	switch_console_set_complete("add pulse looptest");
	switch_console_set_complete("add pulse shstreams");
	switch_console_set_complete("add pulse endpoints");

	/* indicate that the module should continue to be loaded */
	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t load_streams(switch_xml_t streams)
{
	switch_status_t status = SWITCH_STATUS_SUCCESS;
	switch_xml_t param, mystream;
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Loading streams ...\n");
	for (mystream = switch_xml_child(streams, "stream"); mystream; mystream = mystream->next) {
		shared_audio_stream_t *stream = NULL;
		char *stream_name = (char *) switch_xml_attr_soft(mystream, "name");

		if (!stream_name) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Missing stream name attribute, skipping ...\n");
			continue;
		}

		/* check if that stream name is not already used */
		stream = switch_core_hash_find(globals.sh_streams, stream_name);
		if (stream) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "A stream with name '%s' already exists\n", stream_name);
			continue;
		}

		stream = switch_core_alloc(module_pool, sizeof(*stream));
		if (!stream) {
			continue;
		}
		switch_mutex_init(&stream->mutex, SWITCH_MUTEX_NESTED, module_pool);
		stream->sample_rate = globals.sample_rate;
		stream->codec_ms = globals.codec_ms;
		stream->channels = 1;
		switch_snprintf(stream->name, sizeof(stream->name), "%s", stream_name);
		for (param = switch_xml_child(mystream, "param"); param; param = param->next) {
			char *var = (char *) switch_xml_attr_soft(param, "name");
			char *val = (char *) switch_xml_attr_soft(param, "value");
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Parsing stream '%s' parameter %s = %s\n", stream_name, var, val);
			if (!strcmp(var, "sample-rate")) {
				stream->sample_rate = atoi(val);
				if (stream->sample_rate < MIN_STREAM_SAMPLE_RATE) {
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, 
							"Invalid sample rate specified for stream '%s', forcing to 8000\n", stream_name);
					stream->sample_rate = MIN_STREAM_SAMPLE_RATE;
				}
			} else if (!strcmp(var, "codec-ms")) {
				int tmp = atoi(val);
				if (switch_check_interval(stream->sample_rate, tmp)) {
					stream->codec_ms = tmp;
				} else {
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING,
									  "codec-ms must be multiple of 10 and less than %d, Using default of 20\n", SWITCH_MAX_INTERVAL);
				}
			} else if (!strcmp(var, "channels")) {
				stream->channels = atoi(val);
				if (stream->channels < 1 || stream->channels > MAX_IO_CHANNELS) {
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, 
							"Invalid number of channels specified for stream '%s', forcing to 1\n", stream_name);
					stream->channels = 1;
				}
			}
		}
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, 
				"Created stream '%s', sample-rate = %d, codec-ms = %d\n", stream->name, stream->sample_rate, stream->codec_ms);
		switch_core_hash_insert(globals.sh_streams, stream->name, stream);
	}
	return status;
}

static int check_stream_compat(shared_audio_stream_t *in_stream, shared_audio_stream_t *out_stream)
{
	if (!in_stream || !out_stream) {
		/* nothing to be compatible with */
		return 0;
	}
	if (in_stream->sample_rate != out_stream->sample_rate) {
		return -1;
	}
	if (in_stream->codec_ms != out_stream->codec_ms) {
		return -1;
	}
	return 0;
}

static shared_audio_stream_t *check_stream(char *streamstr, int check_input, int *chanindex)
{
	shared_audio_stream_t *stream = NULL;
	int cnum = 0;
	char stream_name[255];
	char *chan = NULL;

	*chanindex = -1;

	switch_snprintf(stream_name, sizeof(stream_name), "%s", streamstr);

	chan = strchr(stream_name, ':');
	if (!chan) {
		return NULL;
	}
	*chan = 0;
	chan++;
	cnum = atoi(chan);

	stream = switch_core_hash_find(globals.sh_streams, stream_name);
	if (!stream) {
		return NULL;
	}

	if (cnum < 0 || cnum > stream->channels) {
		return NULL;
	}

	*chanindex = cnum;

	return stream;
}

static switch_status_t load_endpoints(switch_xml_t endpoints)
{
	switch_status_t status = SWITCH_STATUS_SUCCESS;
	switch_xml_t param, myendpoint;
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Loading endpoints ...\n");
	for (myendpoint = switch_xml_child(endpoints, "endpoint"); myendpoint; myendpoint = myendpoint->next) {
		audio_endpoint_t *endpoint = NULL;
		shared_audio_stream_t *stream = NULL;
		char *endpoint_name = (char *) switch_xml_attr_soft(myendpoint, "name");

		if (!endpoint_name) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Missing endpoint name attribute, skipping ...\n");
			continue;
		}

		/* check if that endpoint name is not already used */
		endpoint = switch_core_hash_find(globals.endpoints, endpoint_name);
		if (endpoint) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "An endpoint with name '%s' already exists\n", endpoint_name);
			continue;
		}

		endpoint = switch_core_alloc(module_pool, sizeof(*endpoint));
		if (!endpoint) {
			continue;
		}
		switch_mutex_init(&endpoint->mutex, SWITCH_MUTEX_NESTED, module_pool);
		endpoint->inchan = -1;
		endpoint->outchan = -1;
		switch_snprintf(endpoint->name, sizeof(endpoint->name), "%s", endpoint_name);
		for (param = switch_xml_child(myendpoint, "param"); param; param = param->next) {
			char *var = (char *) switch_xml_attr_soft(param, "name");
			char *val = (char *) switch_xml_attr_soft(param, "value");
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Parsing endpoint '%s' parameter %s = %s\n", endpoint_name, var, val);
			if (!strcmp(var, "instream")) {
				stream = check_stream(val, 1, &endpoint->inchan) ;
				if (!stream) {
					endpoint->in_stream = NULL;
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, 
							"Invalid instream specified for endpoint '%s'\n", endpoint_name);
					continue;
				}
				endpoint->in_stream = stream;
			} else if (!strcmp(var, "outstream")) {
				stream = check_stream(val, 0, &endpoint->outchan);
				if (!stream) {
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, 
							"Invalid outstream specified for endpoint '%s'\n", endpoint_name);
					endpoint->out_stream = NULL;
					continue;
				}
				endpoint->out_stream = stream;
			}
		}
		if (!endpoint->in_stream && !endpoint->out_stream) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, 
					"You need at least one stream for endpoint '%s'\n", endpoint_name);
			continue;
		}
		if (check_stream_compat(endpoint->in_stream, endpoint->out_stream)) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, 
					"Incompatible input and output streams for endpoint '%s'\n", endpoint_name);
			continue;
		}
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, 
				"Created endpoint '%s', instream = %s, outstream = %s\n", endpoint->name, 
				endpoint->in_stream ? endpoint->in_stream->name : "(none)", 
				endpoint->out_stream ? endpoint->out_stream->name : "(none)");
		switch_core_hash_insert(globals.endpoints, endpoint->name, endpoint);
	}
	return status;
}

static switch_status_t load_config(void)
{
	char *cf = "pulseaudio.conf";
	switch_xml_t cfg, xml, settings, streams, endpoints, param;
	switch_status_t status = SWITCH_STATUS_SUCCESS;

	if (!(xml = switch_xml_open_cfg(cf, &cfg, NULL))) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Open of %s failed\n", cf);
		return SWITCH_STATUS_TERM;
	}
	destroy_audio_streams();
	destroy_codecs();
	globals.live_stream_switch = 0;
	globals.no_auto_resume_call = 0;
	globals.no_ring_during_call = 0;
	globals.sample_rate = 8000;
	globals.unload_device_fail = 0;

	if ((settings = switch_xml_child(cfg, "settings"))) {
		for (param = switch_xml_child(settings, "param"); param; param = param->next) {
			char *var = (char *) switch_xml_attr_soft(param, "name");
			char *val = (char *) switch_xml_attr_soft(param, "value");

			if (!strcmp(var, "debug")) {
				globals.debug = atoi(val);
			} else if (!strcmp(var, "ring-interval")) {
				globals.ring_interval = atoi(val);
			} else if (!strcmp(var, "no-auto-resume-call")) {
				if (switch_true(val)) {
					globals.no_auto_resume_call = 1;
				} else {
					globals.no_auto_resume_call = 0;
				}
			} else if (!strcmp(var, "no-ring-during-call")) {
				if (switch_true(val)) {
					globals.no_ring_during_call = 1;
				} else {
					globals.no_ring_during_call = 0;
				}
			} else if (!strcmp(var, "live-stream-switch")) {
				if (switch_true(val)) {
					globals.live_stream_switch = 1;
				} else {
					globals.live_stream_switch = 0;
				}
			} else if (!strcmp(var, "ring-file")) {
				set_global_ring_file(val);
			} else if (!strcmp(var, "hold-file")) {
				set_global_hold_file(val);
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
			} else if (!strcasecmp(var, "unload-on-device-fail")) {
				globals.unload_device_fail = switch_true(val);
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

	/* streams and endpoints must be last, some initialization depend on globals defaults */
	if ((streams = switch_xml_child(cfg, "streams"))) {
		load_streams(streams);
	}

	if ((endpoints = switch_xml_child(cfg, "endpoints"))) {
		load_endpoints(endpoints);
	}


	switch_xml_free(xml);

	return status;
}

SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_pulseaudio_shutdown)
{

	destroy_audio_streams();
	destroy_codecs();

	switch_core_hash_destroy(&globals.call_hash);
	switch_core_hash_destroy(&globals.sh_streams);
	switch_core_hash_destroy(&globals.endpoints);

	switch_event_free_subclass(MY_EVENT_RINGING);
	switch_event_free_subclass(MY_EVENT_MAKE_CALL);
	switch_event_free_subclass(MY_EVENT_ERROR_AUDIO_DEV);
	switch_event_free_subclass(MY_EVENT_CALL_HELD);
	switch_event_free_subclass(MY_EVENT_CALL_RESUMED);
	

	switch_safe_free(globals.dialplan);
	switch_safe_free(globals.context);
	switch_safe_free(globals.cid_name);
	switch_safe_free(globals.cid_num);
	switch_safe_free(globals.ring_file);
	switch_safe_free(globals.hold_file);
	switch_safe_free(globals.timer_name);

	return SWITCH_STATUS_SUCCESS;
}

/*******************************************************************/
static switch_status_t play_dev(switch_stream_handle_t *stream, char * file, const char * max_seconds, const char * no_close)
{
	switch_file_handle_t fh = { 0 };
	int samples = 0;
	int seconds = 5;
	audio_stream_t * audio_stream;
	int created_stream = 0;
	int wrote = 0;
	switch_size_t olen;
	int16_t abuf[2048];


	if (!strcasecmp(file, "ringtest")) {
		file = globals.ring_file;
	}
	audio_stream = get_audio_stream(STREAM_PLAYBACK);

	fh.pre_buffer_datalen = SWITCH_DEFAULT_FILE_BUFFER_LEN;

	if (switch_core_file_open(&fh,	file,
		globals.read_codec.implementation->number_of_channels,
		globals.read_codec.implementation->actual_samples_per_second,
		SWITCH_FILE_FLAG_READ | SWITCH_FILE_DATA_SHORT, NULL) != SWITCH_STATUS_SUCCESS) {
			stream->write_function(stream, "Cannot play requested file %s\n", file);
			return SWITCH_STATUS_FALSE;
	}

	olen = globals.read_codec.implementation->samples_per_packet;

	if (max_seconds) {
		int i = atoi(max_seconds);
		if (i >= 0) {
			seconds = i;
		}
	}

	if (! audio_stream) {
		stream->write_function(stream, "Failed to engage audio device\n");
		return SWITCH_STATUS_FALSE;
	}

	samples = globals.read_codec.implementation->actual_samples_per_second * seconds;
	while (switch_core_file_read(&fh, abuf, &olen) == SWITCH_STATUS_SUCCESS) {
		if (globals.destroying_streams ||  ! audio_stream->stream) {
			break;
		}
		
		WriteAudioStream(audio_stream->stream, abuf, olen*2, 0, &(audio_stream->write_timer));
		wrote += (int) olen;
		if (samples) {
			samples -= (int) olen;
			if (samples <= 0) {
				break;
			}
		}
		olen = globals.read_codec.implementation->samples_per_packet;
	}

	switch_core_file_close(&fh);
	if (! globals.call_list && ( ! no_close || strcasecmp(no_close,  "no_close"))) {
		destroy_audio_streams();
	}

	seconds = wrote / globals.read_codec.implementation->actual_samples_per_second;
	stream->write_function(stream, "playback test [%s] %d second(s) %d samples @%dkhz",
		file, seconds, wrote, globals.read_codec.implementation->actual_samples_per_second);
	if (created_stream) { /*still need this as not added to the global pool */
		destroy_actual_stream(audio_stream);
	}
	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t create_codecs(int restart)
{
	int sample_rate = globals.sample_rate;
	int codec_ms = globals.codec_ms;

	if (restart) {
		destroy_codecs();
	}
	if (globals.codecs_inited) {
		return SWITCH_STATUS_SUCCESS;
	}

	if (!switch_core_codec_ready(&globals.read_codec)) {
		if (switch_core_codec_init(&globals.read_codec,
								   "L16",
								   NULL, sample_rate, codec_ms, 1, SWITCH_CODEC_FLAG_ENCODE | SWITCH_CODEC_FLAG_DECODE, NULL,
								   NULL) != SWITCH_STATUS_SUCCESS) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Can't load codec?\n");
			return SWITCH_STATUS_FALSE;
		}
	}

	switch_assert(globals.read_codec.implementation);

	if (!switch_core_codec_ready(&globals.write_codec)) {
		if (switch_core_codec_init(&globals.write_codec,
								   "L16",
								   NULL,
								   sample_rate, codec_ms, 1, SWITCH_CODEC_FLAG_ENCODE | SWITCH_CODEC_FLAG_DECODE, NULL, NULL) != SWITCH_STATUS_SUCCESS) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Can't load codec?\n");
			switch_core_codec_destroy(&globals.read_codec);
			return SWITCH_STATUS_FALSE;
		}
	}

	if (!globals.read_timer.timer_interface) {
		if (switch_core_timer_init(&globals.read_timer,
								   globals.timer_name, codec_ms, globals.read_codec.implementation->samples_per_packet,
								   module_pool) != SWITCH_STATUS_SUCCESS) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "setup timer failed!\n");
			switch_core_codec_destroy(&globals.read_codec);
			switch_core_codec_destroy(&globals.write_codec);
			return SWITCH_STATUS_FALSE;
		}
	}
	if (!globals.readfile_timer.timer_interface) {
		if (switch_core_timer_init(&globals.readfile_timer,
								   globals.timer_name, codec_ms, globals.read_codec.implementation->samples_per_packet,
								   module_pool) != SWITCH_STATUS_SUCCESS) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "setup timer failed!\n");
			switch_core_codec_destroy(&globals.read_codec);
			switch_core_codec_destroy(&globals.write_codec);
			return SWITCH_STATUS_FALSE;
		}
	}


	if (!globals.hold_timer.timer_interface) {
		if (switch_core_timer_init(&globals.hold_timer,
								   globals.timer_name, codec_ms, globals.read_codec.implementation->samples_per_packet,
								   module_pool) != SWITCH_STATUS_SUCCESS) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "setup hold timer failed!\n");
			switch_core_codec_destroy(&globals.read_codec);
			switch_core_codec_destroy(&globals.write_codec);
			switch_core_timer_destroy(&globals.read_timer);
			switch_core_timer_destroy(&globals.readfile_timer);
			
			return SWITCH_STATUS_FALSE;
		}
	}

	globals.cng_frame.rate = globals.read_frame.rate = sample_rate;
	globals.cng_frame.codec = globals.read_frame.codec = &globals.read_codec;

	globals.codecs_inited=1;
	return SWITCH_STATUS_SUCCESS;
}

pa_error open_audio_stream(PABLIO_Stream **stream, const char * channelName, const pa_sample_spec * inputParameters, const pa_sample_spec * outputParameters)
{
	return OpenAudioStream(stream, channelName, inputParameters, outputParameters, globals.sample_rate, globals.read_codec.implementation->samples_per_packet);
}

pa_error open_shared_audio_stream(shared_audio_stream_t *shstream, const char * channelName, const pa_sample_spec * inputParameters, const pa_sample_spec * outputParameters)
{
	pa_error err;
	err = OpenAudioStream(&shstream->stream, channelName, inputParameters, outputParameters,
			shstream->sample_rate, STREAM_SAMPLES_PER_PACKET(shstream));
	if (err) {
		shstream->stream = NULL;
	}
	return err;
}

static int create_shared_audio_stream(shared_audio_stream_t *shstream, const char * channelName)
{
	pa_sample_spec inputParameters, outputParameters;
	pa_error err;
	switch_event_t *event;

	inputParameters.channels = 1;
	inputParameters.format = SAMPLE_TYPE;
	inputParameters.rate = globals.sample_rate;

	outputParameters.channels = 1;
	outputParameters.format = SAMPLE_TYPE;
	outputParameters.rate = globals.sample_rate;
	
	err = open_shared_audio_stream(shstream, channelName, &inputParameters, &outputParameters);
	if (err) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Can't open PulseAudio server: %s\n", pa_strerror(err));
		if (switch_event_create_subclass(&event, SWITCH_EVENT_CUSTOM, MY_EVENT_ERROR_AUDIO_DEV) == SWITCH_STATUS_SUCCESS) {
			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "Reason", pa_strerror(err));
			switch_event_fire(&event);
		}
		return -1;
	}
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Created shared audio stream %s: %d channels %d\n", 
			shstream->name, shstream->sample_rate, shstream->channels);
	return 0;
}

static int destroy_shared_audio_stream(shared_audio_stream_t *shstream)
{
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Destroying shared audio stream %s\n", shstream->name);
	CloseAudioStream(shstream->stream);
	shstream->stream = NULL;
	return 0;
}

static audio_stream_t *create_audio_stream(STREAMS stream_number)
{
	pa_sample_spec inputParameters, outputParameters;
	pa_sample_spec *inputParametersPointer = &inputParameters;
	pa_error err;
	switch_event_t *event;
	audio_stream_t *stream;
	char *channel_name;

	switch (stream_number) {
		case STREAM_VOICE:
			channel_name = "Voice";
			break;
		case STREAM_RING:
			channel_name = "Ring";
			break;
		case STREAM_PLAYBACK:
			channel_name = "Playback";
			break;
	}

	stream = malloc(sizeof(*stream));
	if (stream == NULL) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Unable to alloc memory\n");
		return NULL;
	}
	memset(stream, 0, sizeof(*stream));
	stream->next = NULL;
	stream->stream = NULL;
	if (!stream->write_timer.timer_interface) {
		if (switch_core_timer_init(&(stream->write_timer),
								   globals.timer_name, globals.codec_ms, globals.read_codec.implementation->samples_per_packet,
								   module_pool) != SWITCH_STATUS_SUCCESS) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "setup timer failed!\n");
			switch_safe_free(stream);
			return NULL;
		}
	}

	if (stream_number == STREAM_VOICE) {
		inputParameters.channels = 1;
		inputParameters.format = SAMPLE_TYPE;
		inputParameters.rate = globals.sample_rate;
	} else {
		inputParametersPointer = NULL;
	}

	outputParameters.channels = 1;
	outputParameters.format = SAMPLE_TYPE;
	outputParameters.rate = globals.sample_rate;
	
	err = open_audio_stream(&(stream->stream), channel_name, inputParametersPointer, &outputParameters);
	if (err) {
		switch_safe_free(stream);
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Can't open PulseAudio server: %s\n", pa_strerror(err));
		if (switch_event_create_subclass(&event, SWITCH_EVENT_CUSTOM, MY_EVENT_ERROR_AUDIO_DEV) == SWITCH_STATUS_SUCCESS) {
			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "Reason", pa_strerror(err));
			switch_event_fire(&event);
		}
		return NULL;
	}
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Created %s audio stream: %d Hz, %d channels\n", channel_name, outputParameters.rate, outputParameters.channels);
	return stream;
}

audio_stream_t *get_audio_stream(STREAMS stream_number)
{
	if (create_codecs(0) != SWITCH_STATUS_SUCCESS) {
		return NULL;
	}

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Searching for audio stream %d\n", stream_number);
	switch (stream_number) {
		case STREAM_VOICE:
			if (!globals.main_stream) {
				globals.main_stream = create_audio_stream(stream_number);
			}
			return globals.main_stream;
		case STREAM_RING:
			if (!globals.ring_stream) {
				globals.ring_stream = create_audio_stream(stream_number);
			}
			return globals.ring_stream;
		case STREAM_PLAYBACK:
			if (!globals.play_stream) {
				globals.play_stream = create_audio_stream(stream_number);
			}
			return globals.play_stream;
	}
	return NULL;
}

static switch_status_t dtmf_call(char **argv, int argc, switch_stream_handle_t *stream)
{
	char *dtmf_str = argv[0];
	switch_dtmf_t dtmf = { 0, switch_core_default_dtmf_duration(0) };

	if (zstr(dtmf_str)) {
		stream->write_function(stream, "No DTMF Supplied!\n");
	} else {
		switch_mutex_lock(globals.pvt_lock);
		if (globals.call_list) {
			switch_channel_t *channel = switch_core_session_get_channel(globals.call_list->session);
			char *p = dtmf_str;
			while (p && *p) {
				dtmf.digit = *p;
				switch_channel_queue_dtmf(channel, &dtmf);
				p++;
			}
		}
		switch_mutex_unlock(globals.pvt_lock);
	}

	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t list_shared_streams(char **argv, int argc, switch_stream_handle_t *stream)
{
	switch_hash_index_t *hi;
	int cnt = 0;
	for (hi = switch_core_hash_first(globals.sh_streams); hi; hi = switch_core_hash_next(&hi)) {
		const void *var;
		void *val;
		shared_audio_stream_t *s = NULL;
		switch_core_hash_this(hi, &var, NULL, &val);
		s = val;
		stream->write_function(stream, "%s> sample-rate: %d, codec-ms: %d, channels: %d\n",
				s->name, s->sample_rate, s->codec_ms, s->channels);
		cnt++;
	}
	stream->write_function(stream, "Total streams: %d\n", cnt);
	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t list_endpoints(char **argv, int argc, switch_stream_handle_t *stream)
{
	switch_hash_index_t *hi;
	int cnt = 0;
	for (hi = switch_core_hash_first(globals.endpoints); hi; hi = switch_core_hash_next(&hi)) {
		const void *var;
		void *val;
		audio_endpoint_t *e = NULL;
		switch_core_hash_this(hi, &var, NULL, &val);
		e = val;
		stream->write_function(stream, "%s> instream: %s, outstream: %s\n",
				e->name, e->in_stream ? e->in_stream->name : "(none)", 
				e->out_stream ? e->out_stream->name : "(none)");
		cnt++;
	}
	stream->write_function(stream, "Total endpoints: %d\n", cnt);
	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t close_streams(char **argv, int argc, switch_stream_handle_t *stream)
{
	if (globals.call_list) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "ERROR: Cannot use this command this while a call is in progress\n");
		return SWITCH_STATUS_FALSE;
	}
	destroy_audio_streams();
	stream->write_function(stream, "closestreams all open streams closed\n");
	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t prepare_stream(char **argv, int argc, switch_stream_handle_t *stream)
{
	audio_stream_t * audio_stream;

	if (! strcasecmp(argv[0], "voice")) {
		audio_stream = get_audio_stream(STREAM_VOICE);
	} else if (! strcasecmp(argv[0], "ring")) {
		audio_stream = get_audio_stream(STREAM_RING);
	} else if (! strcasecmp(argv[0], "playback")) {
		audio_stream = get_audio_stream(STREAM_PLAYBACK);
	} else {
		stream->write_function(stream, "invalid string specified\n");
		return SWITCH_STATUS_FALSE;
	}

	if (! audio_stream) {
		stream->write_function(stream, "preparestream not prepared for %s, received an invalid stream back\n", argv[0]);
		return SWITCH_STATUS_FALSE;
	}
	stream->write_function(stream, "preparestream prepared %s\n", argv[0]);
	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t looptest(char **argv, int argc, switch_stream_handle_t *stream)
{
	int datalen = 0;
	int success = 0;
	int i;

	if (globals.call_list) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "ERROR: Cannot use this command this while a call is in progress\n");
		return SWITCH_STATUS_FALSE;
	}
	if (validate_main_audio_stream() != SWITCH_STATUS_SUCCESS) {
		stream->write_function(stream, "looptest Failed to engage audio device\n");
		return SWITCH_STATUS_FALSE;
	}
	for (i = 0; i < 400; i++) {
		if (globals.destroying_streams ||  ! globals.main_stream->stream) {
			break;
		}
		if ((datalen = ReadAudioStream(globals.main_stream->stream, globals.read_frame.data, globals.read_codec.implementation->decoded_bytes_per_packet, 0, &globals.read_timer))) {
			WriteAudioStream(globals.main_stream->stream, globals.read_frame.data, datalen, 0, &(globals.main_stream->write_timer));
			success = 1;
		}
		switch_yield(10000);
	}

	if (!success) {
		stream->write_function(stream, "Failed to read any bytes from PulseAudio\n");
		return SWITCH_STATUS_FALSE;
	}
	destroy_audio_streams();
	stream->write_function(stream, "looptest complete\n");
	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t set_ringfile(char **argv, int argc, switch_stream_handle_t *stream)
{
	if (! argv[0]) {
		stream->write_function(stream, "%s", globals.ring_file);
		return SWITCH_STATUS_SUCCESS;
	}
	if (create_codecs(0) == SWITCH_STATUS_SUCCESS) {
		switch_file_handle_t fh = { 0 };
		if (switch_core_file_open(&fh,
								  argv[0],
								  globals.read_codec.implementation->number_of_channels,
								  globals.read_codec.implementation->actual_samples_per_second,
								  SWITCH_FILE_FLAG_READ | SWITCH_FILE_DATA_SHORT, NULL) == SWITCH_STATUS_SUCCESS) {
			switch_core_file_close(&fh);
			set_global_ring_file(argv[0]);
		} else {
			stream->write_function(stream, "ringfile Unable to open ring file %s\n", argv[0]);
			return SWITCH_STATUS_FALSE;
		}
	} else {
		stream->write_function(stream, "ringfile Failed to init codecs device\n");
		return SWITCH_STATUS_FALSE;
	}
	stream->write_function(stream, "ringfile set to %s", globals.ring_file);
	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t switch_call(char **argv, int argc, switch_stream_handle_t *stream)
{
	private_t *tp, *tech_pvt = NULL;
	char *callid = argv[0];
	uint8_t one_call = 0;


	switch_mutex_lock(globals.pvt_lock);
	if (zstr(callid)) {
		if (globals.call_list) {
			if (globals.call_list->next) {
				tech_pvt = globals.call_list->next;
			} else {
				tech_pvt = globals.call_list;
				one_call = 1;
			}
		}
	} else if (!strcasecmp(callid, "none")) {
		for (tp = globals.call_list; tp; tp = tp->next) {
			if (switch_test_flag(tp, TFLAG_MASTER))	{
			switch_clear_flag_locked(tp, TFLAG_MASTER);
				create_hold_event(tp,0);
			}
		}
		stream->write_function(stream, "OK\n");
		goto done;
	} else {
		tech_pvt = switch_core_hash_find(globals.call_hash, callid);
	}

	if (tech_pvt) {
		if (tech_pvt == globals.call_list && !tech_pvt->next) {
			one_call = 1;
		}

		if (!one_call) {
			remove_pvt(tech_pvt);
		}
		add_pvt(tech_pvt, PA_MASTER);
		create_hold_event(tech_pvt, 1);
		stream->write_function(stream, "OK\n");
	} else {
		stream->write_function(stream, "NO SUCH CALL\n");
	}

  done:
	switch_mutex_unlock(globals.pvt_lock);

	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t hangup_call(char **argv, int argc, switch_stream_handle_t *stream)
{
	private_t *tech_pvt;
	char *callid = argv[0];

	switch_mutex_lock(globals.pvt_lock);
	if (zstr(callid)) {
		tech_pvt = globals.call_list;
	} else {
		tech_pvt = switch_core_hash_find(globals.call_hash, callid);
	}

	if (tech_pvt) {
		switch_channel_hangup(switch_core_session_get_channel(tech_pvt->session), SWITCH_CAUSE_NORMAL_CLEARING);
		stream->write_function(stream, "OK\n");
	} else {
		stream->write_function(stream, "NO SUCH CALL\n");
	}
	switch_mutex_unlock(globals.pvt_lock);

	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t answer_call(char **argv, int argc, switch_stream_handle_t *stream)
{
	private_t *tp;
	int x = 0;
	char *callid = argv[0];

	switch_mutex_lock(globals.pvt_lock);

	if (!zstr(callid)) {
		if ((tp = switch_core_hash_find(globals.call_hash, callid))) {
			if (switch_test_flag(tp, TFLAG_ANSWER)) {
				stream->write_function(stream, "CALL ALREADY ANSWERED\n");
			} else {
				switch_channel_t *channel = switch_core_session_get_channel(tp->session);
				switch_set_flag_locked(tp, TFLAG_ANSWER);
				if (tp != globals.call_list) {
					remove_pvt(tp);
				}
				add_pvt(tp, PA_MASTER);
				switch_channel_mark_answered(channel);
			}
		} else {
			stream->write_function(stream, "NO SUCH CALL\n");
		}

		goto done;
	}

	for (tp = globals.call_list; tp; tp = tp->next) {
		if (!switch_test_flag(tp, TFLAG_ANSWER)) {
			switch_channel_t *channel = switch_core_session_get_channel(tp->session);
			switch_set_flag_locked(tp, TFLAG_ANSWER);
			add_pvt(tp, PA_MASTER);
			switch_channel_mark_answered(channel);
			x++;
			break;
		}
	}
  done:
	switch_mutex_unlock(globals.pvt_lock);
	stream->write_function(stream, "Answered %d channels.\n", x);

	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t do_flags(char **argv, int argc, switch_stream_handle_t *stream)
{
	char *action = argv[0];
	char *flag_str = argv[1];
	GFLAGS flags = GFLAG_NONE;
	char *p;
	int x = 0;

	if (argc < 2) {
		goto desc;
	}

	for (x = 1; x < argc; x++) {
		flag_str = argv[x];
		for (p = flag_str; *p; p++) {
			*p = (char) tolower(*p);
		}

		if (strstr(flag_str, "ear")) {
			flags |= GFLAG_EAR;
		}
		if (strstr(flag_str, "mouth")) {
			flags |= GFLAG_MOUTH;
		}
	}

	if (!strcasecmp(action, "on")) {
		if (flags & GFLAG_EAR) {
			switch_set_flag((&globals), GFLAG_EAR);
		}
		if (flags & GFLAG_MOUTH) {
			switch_set_flag((&globals), GFLAG_MOUTH);
		}
	} else if (!strcasecmp(action, "off")) {
		if (flags & GFLAG_EAR) {
			switch_clear_flag((&globals), GFLAG_EAR);
		}
		if (flags & GFLAG_MOUTH) {
			switch_clear_flag((&globals), GFLAG_MOUTH);
		}
	} else {
		goto bad;
	}

  desc:
	x = 0;
	stream->write_function(stream, "FLAGS: ");
	if (switch_test_flag((&globals), GFLAG_EAR)) {
		stream->write_function(stream, "ear");
		x++;
	}
	if (switch_test_flag((&globals), GFLAG_MOUTH)) {
		stream->write_function(stream, "%smouth", x ? "|" : "");
		x++;
	}
	if (!x) {
		stream->write_function(stream, "none");
	}

	goto done;

  bad:
	stream->write_function(stream, "Usage: flags [on|off] <flags>\n");
  done:
	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t list_calls(char **argv, int argc, switch_stream_handle_t *stream)
{
	private_t *tp;
	int x = 0;
	const char *cid_name = "n/a";
	const char *cid_num = "n/a";

	switch_mutex_lock(globals.pvt_lock);
	for (tp = globals.call_list; tp; tp = tp->next) {
		switch_channel_t *channel;
		switch_caller_profile_t *profile;
		x++;
		channel = switch_core_session_get_channel(tp->session);

		if ((profile = switch_channel_get_caller_profile(channel))) {
			if (profile->originatee_caller_profile) {
				cid_name = "Outbound Call";
				cid_num = profile->originatee_caller_profile->destination_number;
			} else {
				cid_name = profile->caller_id_name;
				cid_num = profile->caller_id_number;
			}
		}

		stream->write_function(stream, "%s %s [%s (%s)] %s\n", tp->call_id, switch_channel_get_name(channel),
							   cid_name, cid_num, switch_test_flag(tp, TFLAG_MASTER) ? "active" : "hold");
	}
	switch_mutex_unlock(globals.pvt_lock);

	stream->write_function(stream, "\n%d call%s\n", x, x == 1 ? "" : "s");

	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t place_call(char **argv, int argc, switch_stream_handle_t *stream)
{
	switch_core_session_t *session;
	switch_status_t status = SWITCH_STATUS_SUCCESS;
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
		if ((tech_pvt = (private_t *) switch_core_session_alloc(session, sizeof(private_t))) != 0) {
			memset(tech_pvt, 0, sizeof(*tech_pvt));
			switch_mutex_init(&tech_pvt->flag_mutex, SWITCH_MUTEX_NESTED, switch_core_session_get_pool(session));
			channel = switch_core_session_get_channel(session);
			switch_core_session_set_private(session, tech_pvt);
			tech_pvt->session = session;
		} else {
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_CRIT, "Hey where is my memory pool?\n");
			switch_core_session_destroy(&session);
			return SWITCH_STATUS_MEMERR;
		}

		if (!zstr(argv[1])) {
			dialplan = argv[1];
		}

		if (!zstr(argv[2])) {
			cid_num = argv[2];
		}

		if (!zstr(argv[3])) {
			cid_name = argv[3];
		}

		if (!zstr(argv[4])) {
			tech_pvt->sample_rate = atoi(argv[4]);
		}

		if (!zstr(argv[5])) {
			tech_pvt->codec_ms = atoi(argv[5]);
		}

		switch_find_local_ip(ip, sizeof(ip), NULL, AF_INET);

		if ((tech_pvt->caller_profile = switch_caller_profile_new(switch_core_session_get_pool(session),
																  NULL, dialplan, cid_name, cid_num, ip, NULL, NULL, NULL, modname, context, dest)) != 0) {
			char name[128];
			switch_snprintf(name, sizeof(name), "pulseaudio/%s",
							tech_pvt->caller_profile->destination_number ? tech_pvt->caller_profile->destination_number : modname);
			switch_channel_set_name(channel, name);

			switch_channel_set_caller_profile(channel, tech_pvt->caller_profile);
		}
		tech_pvt->session = session;
		if ((status = validate_main_audio_stream()) == SWITCH_STATUS_SUCCESS) {
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
				add_pvt(tech_pvt, PA_MASTER);
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
		"pulse call <dest> [<dialplan> <cid_name> <cid_num>]\n"
		"pulse answer [<call_id>]\n"
		"pulse hangup [<call_id>]\n"
		"pulse list\n"
		"pulse switch [<call_id>|none]\n"
		"pulse dtmf <digit string>\n"
		"pulse flags [on|off] [ear] [mouth]\n"
		"pulse preparestream <voice|ring|playback>\n"
		"pulse closestreams\n"
		"pulse play [ringtest|<filename>] [seconds] [no_close]\n"
		"pulse playdev #<num> [ringtest|<filename>] [seconds] [no_close]\n"
		"pulse ringfile [filename]\n"
		"pulse looptest\n"
		"pulse shstreams\n"
		"pulse endpoints\n"
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
	} else if (!strcasecmp(argv[0], "closestreams")) {
		func = close_streams;
	} else if (argv[1] && !strcmp(argv[0], "preparestream")) {
		func = prepare_stream;	
	} else if ((argv[1] && !strcmp(argv[0], "play"))) {
		if (validate_playback_audio_stream() == SWITCH_STATUS_SUCCESS) {
			play_dev(stream, argv[1], argv[2], argv[3]);
		}else{
			stream->write_function(stream, "Failed to engage audio device\n");
		}
		goto done;
	} else if (!strcasecmp(argv[0], "looptest")) {
		func = looptest;
	} else if (!strcasecmp(argv[0], "ringfile")) {
		func = set_ringfile;
	} else if (!strcasecmp(argv[0], "shstreams")) {
		func = list_shared_streams;
	} else if (!strcasecmp(argv[0], "endpoints")) {
		func = list_endpoints;
	} else {
		stream->write_function(stream, "Unknown Command or not enough args [%s]\n", argv[0]);
	}


	if (func) {
		if (http) {
			stream->write_function(stream, "<pre>");
		}

		switch_mutex_lock(globals.pa_mutex);
		status = func(&argv[lead], argc - lead, stream);
		status = SWITCH_STATUS_SUCCESS; /*if func was defined we want to always return success as the command was found */
		switch_mutex_unlock(globals.pa_mutex);

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
							   "<input name=action type=submit value=\"preparestream\"> "
							   "<input name=action type=submit value=\"switchstream\"> "
							   "<input name=action type=submit value=\"closestreams\"> "
							   "<input name=action type=submit value=\"play\"> "
							   "<input name=action type=submit value=\"playdev\"> "
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
