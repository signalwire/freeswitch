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
 * mod_alsa.c -- Alsa Endpoint Module
 *
 */
#include <switch.h>

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include <alsa/asoundlib.h>

#define MY_EVENT_RINGING "alsa::ringing"

SWITCH_MODULE_LOAD_FUNCTION(mod_alsa_load);
SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_alsa_shutdown);
SWITCH_MODULE_DEFINITION(mod_alsa, mod_alsa_load, mod_alsa_shutdown, NULL);

static switch_memory_pool_t *module_pool = NULL;
switch_endpoint_interface_t *alsa_endpoint_interface;

//static int running = 1;

#define SAMPLE_TYPE  paInt16
//#define SAMPLE_TYPE  paFloat32
typedef short SAMPLE;

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
	TFLAG_MASTER = (1 << 9)
} TFLAGS;

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
	switch_codec_t write_codec;
	switch_timer_t timer;
	struct private_object *next;
};

typedef struct private_object private_t;

static struct {
	int debug;
	int port;
	char *cid_name;
	char *cid_num;
	char *dialplan;
	char *ring_file;
	char *hold_file;
	char *timer_name;
	char *device_name;
	int call_id;
	switch_hash_t *call_hash;
	switch_mutex_t *device_lock;
	switch_mutex_t *pvt_lock;
	switch_mutex_t *flag_mutex;
	int sample_rate;
	int codec_ms;
	snd_pcm_t *audio_stream_in;
	snd_pcm_t *audio_stream_out;


	switch_codec_t read_codec;
	switch_codec_t write_codec;
	switch_frame_t read_frame;
	switch_frame_t cng_frame;
	unsigned char databuf[SWITCH_RECOMMENDED_BUFFER_SIZE];
	unsigned char cngbuf[SWITCH_RECOMMENDED_BUFFER_SIZE];
	private_t *call_list;
	int ring_interval;
	GFLAGS flags;
	switch_timer_t timer;
} globals;


#define PA_MASTER 1
#define PA_SLAVE 0


SWITCH_DECLARE_GLOBAL_STRING_FUNC(set_global_dialplan, globals.dialplan);
SWITCH_DECLARE_GLOBAL_STRING_FUNC(set_global_cid_name, globals.cid_name);
SWITCH_DECLARE_GLOBAL_STRING_FUNC(set_global_cid_num, globals.cid_num);
SWITCH_DECLARE_GLOBAL_STRING_FUNC(set_global_ring_file, globals.ring_file);
SWITCH_DECLARE_GLOBAL_STRING_FUNC(set_global_hold_file, globals.hold_file);
SWITCH_DECLARE_GLOBAL_STRING_FUNC(set_global_timer_name, globals.timer_name);
SWITCH_DECLARE_GLOBAL_STRING_FUNC(set_global_device_name, globals.device_name);

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
static switch_status_t engage_device(unsigned int samplerate, int codec_ms);
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
	private_t *tech_pvt = NULL;
	switch_time_t last;
	int waitsec = globals.ring_interval * 1000000;
	switch_file_handle_t fh = { 0 };
	const char *val, *ring_file = NULL, *hold_file = NULL;
	int16_t abuf[2048];

	tech_pvt = switch_core_session_get_private(session);
	assert(tech_pvt != NULL);

	channel = switch_core_session_get_channel(session);
	assert(channel != NULL);


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


		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "%s CHANNEL INIT %d %d\n",
						  switch_channel_get_name(channel), switch_channel_get_state(channel), switch_test_flag(tech_pvt, TFLAG_ANSWER));



		if (engage_device(tech_pvt->sample_rate, tech_pvt->codec_ms) != SWITCH_STATUS_SUCCESS) {
			switch_channel_hangup(channel, SWITCH_CAUSE_DESTINATION_OUT_OF_ORDER);
			return SWITCH_STATUS_FALSE;
		}

		if (switch_test_flag(tech_pvt, TFLAG_OUTBOUND)) {
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

					if (engage_device(fh.samplerate, fh.channels) != SWITCH_STATUS_SUCCESS) {
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

		switch_channel_mark_ring_ready(channel);

		while (switch_channel_get_state(channel) == CS_INIT && !switch_test_flag(tech_pvt, TFLAG_ANSWER)) {
			if (switch_micro_time_now() - last >= waitsec) {
				char buf[512];
				switch_event_t *event;

				snprintf(buf, sizeof(buf), "BRRRRING! BRRRRING! call %s\n", tech_pvt->call_id);

				if (switch_event_create_subclass(&event, SWITCH_EVENT_CUSTOM, MY_EVENT_RINGING) == SWITCH_STATUS_SUCCESS) {
					switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "event_info", buf);
					switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "call_id", tech_pvt->call_id);
					switch_channel_event_set_data(channel, event);
					switch_event_fire(&event);
				}

				switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "%s\n", buf);
				last = switch_micro_time_now();
				if (ring_file) {
					unsigned int pos = 0;
					switch_core_file_seek(&fh, &pos, 0, SEEK_SET);
					for (;;) {
						switch_size_t olen = 1024;
						switch_core_file_read(&fh, abuf, &olen);
						if (olen == 0) {
							break;
						}
						snd_pcm_writei(globals.audio_stream_out, abuf, (int) olen);
					}
				}
			}

			switch_yield(globals.read_codec.implementation->microseconds_per_packet);

		}
		switch_clear_flag_locked((&globals), GFLAG_RING);
	}

	if (ring_file) {
		switch_core_file_close(&fh);
		switch_core_codec_destroy(&tech_pvt->write_codec);
		switch_core_timer_destroy(&tech_pvt->timer);
	}

	if (switch_test_flag(tech_pvt, TFLAG_OUTBOUND)) {
		if (!switch_test_flag(tech_pvt, TFLAG_ANSWER)) {
			switch_channel_hangup(channel, SWITCH_CAUSE_NO_ANSWER);
			return SWITCH_STATUS_SUCCESS;
		}
	}

	switch_set_flag_locked(tech_pvt, TFLAG_IO);

	/* Move channel's state machine to ROUTING */
	switch_channel_set_state(channel, CS_ROUTING);

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

	switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "%s CHANNEL ROUTING\n", switch_channel_get_name(channel));

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

	switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "%s CHANNEL EXECUTE\n", switch_channel_get_name(channel));


	return SWITCH_STATUS_SUCCESS;
}

static void deactivate_audio_device(void)
{

	switch_mutex_lock(globals.device_lock);
	if (globals.audio_stream_in) {
		snd_pcm_drain(globals.audio_stream_in);
		snd_pcm_close(globals.audio_stream_in);
		globals.audio_stream_in = NULL;
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Close IN stream\n");
	}
	if (globals.audio_stream_out) {
		snd_pcm_drain(globals.audio_stream_out);
		snd_pcm_close(globals.audio_stream_out);
		globals.audio_stream_out = NULL;
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Close OUT stream\n");
	}
	switch_mutex_unlock(globals.device_lock);
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "DONE\n");
}



static void add_pvt(private_t *tech_pvt, int master)
{
	private_t *tp;
	uint8_t in_list = 0;

	switch_mutex_lock(globals.pvt_lock);

	if (zstr(tech_pvt->call_id)) {
		snprintf(tech_pvt->call_id, sizeof(tech_pvt->call_id), "%d", ++globals.call_id);
		switch_core_hash_insert(globals.call_hash, tech_pvt->call_id, tech_pvt);
		switch_core_session_set_read_codec(tech_pvt->session, &globals.read_codec);
		switch_core_session_set_write_codec(tech_pvt->session, &globals.write_codec);
	}

	for (tp = globals.call_list; tp; tp = tp->next) {
		if (tp == tech_pvt) {
			in_list = 1;
		}
		if (master) {
			switch_clear_flag_locked(tp, TFLAG_MASTER);
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

	switch_mutex_lock(globals.pvt_lock);
	for (tp = globals.call_list; tp; tp = tp->next) {
		switch_clear_flag_locked(tp, TFLAG_MASTER);
		if (tp == tech_pvt) {
			if (last) {
				last->next = tp->next;
			} else {
				globals.call_list = tp->next;
			}
		}
		last = tp;
	}

	if (globals.call_list) {
		switch_set_flag_locked(globals.call_list, TFLAG_MASTER);
	} else {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "No more channels\n");
	}

	switch_mutex_unlock(globals.pvt_lock);
}

static switch_status_t channel_on_destroy(switch_core_session_t *session)
{
	//switch_channel_t *channel = NULL;
	//private_t *tech_pvt = NULL;

	//channel = switch_core_session_get_channel(session);
	//tech_pvt = switch_core_session_get_private(session);


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

	remove_pvt(tech_pvt);

	switch_clear_flag_locked(tech_pvt, TFLAG_IO);
	switch_set_flag_locked(tech_pvt, TFLAG_HUP);

	switch_core_hash_delete(globals.call_hash, tech_pvt->call_id);

	if (tech_pvt->hfh) {
		tech_pvt->hfh = NULL;
		switch_core_file_close(&tech_pvt->fh);
	}

	switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "%s CHANNEL HANGUP\n", switch_channel_get_name(channel));

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
	switch_channel_t *channel = NULL;
	private_t *tech_pvt = NULL;

	channel = switch_core_session_get_channel(session);
	assert(channel != NULL);

	tech_pvt = switch_core_session_get_private(session);
	assert(tech_pvt != NULL);

	switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "CHANNEL LOOPBACK\n");

	return SWITCH_STATUS_SUCCESS;
}


static switch_status_t channel_send_dtmf(switch_core_session_t *session, const switch_dtmf_t *dtmf)
{
	private_t *tech_pvt = NULL;

	tech_pvt = switch_core_session_get_private(session);
	assert(tech_pvt != NULL);

	switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "DTMF ON CALL %s [%c]\n", tech_pvt->call_id, dtmf->digit);

	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t channel_read_frame(switch_core_session_t *session, switch_frame_t **frame, switch_io_flag_t flags, int stream_id)
{
	switch_channel_t *channel = NULL;
	private_t *tech_pvt = NULL;
	int samples = 0;
	switch_status_t status = SWITCH_STATUS_FALSE;

	channel = switch_core_session_get_channel(session);
	assert(channel != NULL);

	tech_pvt = switch_core_session_get_private(session);
	assert(tech_pvt != NULL);

	if (!globals.audio_stream_in) {
		return SWITCH_STATUS_FALSE;
	}

	if (switch_test_flag(tech_pvt, TFLAG_HUP)) {
		return SWITCH_STATUS_FALSE;
	}

	if (!switch_test_flag(tech_pvt, TFLAG_IO)) {
		goto cng;
	}

	if (!is_master(tech_pvt)) {
		if (tech_pvt->hold_file) {
			if (!tech_pvt->hfh) {
				int codec_ms = tech_pvt->codec_ms ? tech_pvt->codec_ms : globals.codec_ms;
				int sample_rate = tech_pvt->sample_rate ? tech_pvt->sample_rate : globals.sample_rate;

				if (switch_core_codec_init(&tech_pvt->write_codec,
										   "L16",
										   NULL,
										   sample_rate,
										   codec_ms,
										   1,
										   SWITCH_CODEC_FLAG_ENCODE | SWITCH_CODEC_FLAG_DECODE,
										   NULL, switch_core_session_get_pool(tech_pvt->session)) != SWITCH_STATUS_SUCCESS) {
					switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "Can't load codec?\n");
					switch_core_codec_destroy(&globals.read_codec);
					tech_pvt->hold_file = NULL;
					goto cng;
				}

				if (switch_core_file_open(&tech_pvt->fh,
										  tech_pvt->hold_file,
										  globals.read_codec.implementation->number_of_channels,
										  globals.read_codec.implementation->actual_samples_per_second,
										  SWITCH_FILE_FLAG_READ | SWITCH_FILE_DATA_SHORT, NULL) != SWITCH_STATUS_SUCCESS) {
					switch_core_codec_destroy(&tech_pvt->write_codec);
					tech_pvt->hold_file = NULL;
					goto cng;
				}

				tech_pvt->hfh = &tech_pvt->fh;
				tech_pvt->hold_frame.data = tech_pvt->holdbuf;
				tech_pvt->hold_frame.buflen = sizeof(tech_pvt->holdbuf);
				tech_pvt->hold_frame.rate = sample_rate;
				tech_pvt->hold_frame.codec = &tech_pvt->write_codec;
			}

			goto hold;
		}
	  cng:
		switch_yield(globals.read_codec.implementation->microseconds_per_packet);
		*frame = &globals.cng_frame;
		return SWITCH_STATUS_SUCCESS;

	  hold:

		{
			switch_size_t olen = globals.read_codec.implementation->samples_per_packet;
			if (switch_core_timer_next(&tech_pvt->timer) != SWITCH_STATUS_SUCCESS) {
				switch_core_file_close(&tech_pvt->fh);
				switch_core_codec_destroy(&tech_pvt->write_codec);
				goto cng;
			}
			switch_core_file_read(tech_pvt->hfh, tech_pvt->hold_frame.data, &olen);

			if (olen == 0) {
				unsigned int pos = 0;
				switch_core_file_seek(tech_pvt->hfh, &pos, 0, SEEK_SET);
				goto cng;
			}


			tech_pvt->hold_frame.datalen = (uint32_t) (olen * sizeof(int16_t));
			tech_pvt->hold_frame.samples = (uint32_t) olen;
			tech_pvt->hold_frame.timestamp = tech_pvt->timer.samplecount;
			*frame = &tech_pvt->hold_frame;

		}

		return SWITCH_STATUS_SUCCESS;
	}

	switch_mutex_lock(globals.device_lock);
	if ((samples = snd_pcm_readi(globals.audio_stream_in, globals.read_frame.data, globals.read_codec.implementation->samples_per_packet)) > 0) {
		globals.read_frame.datalen = samples * 2;
		globals.read_frame.samples = samples;

		switch_core_timer_check(&globals.timer, SWITCH_TRUE);
		globals.read_frame.timestamp = globals.timer.samplecount;
		*frame = &globals.read_frame;

		if (!switch_test_flag((&globals), GFLAG_MOUTH)) {
			memset(globals.read_frame.data, 255, globals.read_frame.datalen);
		}

		status = SWITCH_STATUS_SUCCESS;
	}
	switch_mutex_unlock(globals.device_lock);

	return status;
}

static switch_status_t channel_write_frame(switch_core_session_t *session, switch_frame_t *frame, switch_io_flag_t flags, int stream_id)
{
	switch_channel_t *channel = NULL;
	private_t *tech_pvt = NULL;
	switch_status_t status = SWITCH_STATUS_FALSE;
	channel = switch_core_session_get_channel(session);
	assert(channel != NULL);

	tech_pvt = switch_core_session_get_private(session);
	assert(tech_pvt != NULL);

	if (!globals.audio_stream_out) {
		return SWITCH_STATUS_FALSE;
	}

	if (switch_test_flag(tech_pvt, TFLAG_HUP)) {
		return SWITCH_STATUS_FALSE;
	}

	if (!is_master(tech_pvt) || !switch_test_flag(tech_pvt, TFLAG_IO)) {
		return SWITCH_STATUS_SUCCESS;
	}

	if (globals.audio_stream_out) {
		if (switch_test_flag((&globals), GFLAG_EAR)) {
			snd_pcm_writei(globals.audio_stream_out, (short *) frame->data, (int) (frame->datalen / sizeof(SAMPLE)));
		}
		status = SWITCH_STATUS_SUCCESS;
	}

	return status;

}

static switch_status_t channel_answer_channel(switch_core_session_t *session)
{
	private_t *tech_pvt;
	switch_channel_t *channel = NULL;

	channel = switch_core_session_get_channel(session);
	assert(channel != NULL);

	tech_pvt = switch_core_session_get_private(session);
	assert(tech_pvt != NULL);

	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t channel_receive_message(switch_core_session_t *session, switch_core_session_message_t *msg)
{
	switch_channel_t *channel;
	private_t *tech_pvt;

	channel = switch_core_session_get_channel(session);
	assert(channel != NULL);

	tech_pvt = switch_core_session_get_private(session);
	assert(tech_pvt != NULL);


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


static switch_state_handler_table_t channel_event_handlers = {
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

static switch_io_routines_t channel_io_routines = {
	/*.outgoing_channel */ channel_outgoing_channel,
	/*.read_frame */ channel_read_frame,
	/*.write_frame */ channel_write_frame,
	/*.kill_channel */ channel_kill_channel,
	/*.send_dtmf */ channel_send_dtmf,
	/*.receive_message */ channel_receive_message
};


/* Make sure when you have 2 sessions in the same scope that you pass the appropriate one to the routines
   that allocate memory or you will have 1 channel with memory allocated from another channel's pool!
*/
static switch_call_cause_t channel_outgoing_channel(switch_core_session_t *session, switch_event_t *var_event,
													switch_caller_profile_t *outbound_profile,
													switch_core_session_t **new_session, switch_memory_pool_t **pool, switch_originate_flag_t flags,
													switch_call_cause_t *cancel_cause)
{

	if ((*new_session = switch_core_session_request_uuid(alsa_endpoint_interface, SWITCH_CALL_DIRECTION_OUTBOUND, flags, pool, switch_event_get_header(var_event, "origination_uuid"))) != 0) {
		private_t *tech_pvt;
		switch_channel_t *channel;
		switch_caller_profile_t *caller_profile;

		switch_core_session_add_stream(*new_session, NULL);
		if ((tech_pvt = (private_t *) switch_core_session_alloc(*new_session, sizeof(private_t))) != 0) {
			memset(tech_pvt, 0, sizeof(*tech_pvt));
			switch_mutex_init(&tech_pvt->flag_mutex, SWITCH_MUTEX_NESTED, switch_core_session_get_pool(*new_session));
			channel = switch_core_session_get_channel(*new_session);
			switch_core_session_set_private(*new_session, tech_pvt);
			tech_pvt->session = *new_session;
			globals.flags = GFLAG_EAR | GFLAG_MOUTH;
		} else {
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_CRIT, "Hey where is my memory pool?\n");
			switch_core_session_destroy(new_session);
			return SWITCH_CAUSE_DESTINATION_OUT_OF_ORDER;
		}

		if (outbound_profile) {
			char name[128];
			const char *id = !zstr(outbound_profile->caller_id_number) ? outbound_profile->caller_id_number : "na";
			snprintf(name, sizeof(name), "alsa/%s", id);

			switch_channel_set_name(channel, name);

			caller_profile = switch_caller_profile_clone(*new_session, outbound_profile);
			switch_channel_set_caller_profile(channel, caller_profile);
			tech_pvt->caller_profile = caller_profile;
		} else {
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "Doh! no caller profile\n");
			switch_core_session_destroy(new_session);
			return SWITCH_CAUSE_DESTINATION_OUT_OF_ORDER;
		}

		switch_set_flag_locked(tech_pvt, TFLAG_OUTBOUND);
		switch_channel_set_state(channel, CS_INIT);
		return SWITCH_CAUSE_SUCCESS;
	}

	return SWITCH_CAUSE_DESTINATION_OUT_OF_ORDER;

}


SWITCH_MODULE_LOAD_FUNCTION(mod_alsa_load)
{

	switch_status_t status;
	switch_api_interface_t *api_interface;

	*module_interface = switch_loadable_module_create_module_interface(pool, modname);

	alsa_endpoint_interface = switch_loadable_module_create_interface(*module_interface, SWITCH_ENDPOINT_INTERFACE);
	alsa_endpoint_interface->interface_name = "alsa";
	alsa_endpoint_interface->io_routines = &channel_io_routines;
	alsa_endpoint_interface->state_handler = &channel_event_handlers;

	SWITCH_ADD_API(api_interface, "alsa", "Alsa", pa_cmd, "<command> [<args>]");


	if (switch_core_new_memory_pool(&module_pool) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "OH OH no pool\n");
		return SWITCH_STATUS_TERM;
	}

	memset(&globals, 0, sizeof(globals));

	if ((status = load_config()) != SWITCH_STATUS_SUCCESS) {
		return status;
	}



	switch_core_hash_init(&globals.call_hash, module_pool);
	switch_mutex_init(&globals.device_lock, SWITCH_MUTEX_NESTED, module_pool);
	switch_mutex_init(&globals.pvt_lock, SWITCH_MUTEX_NESTED, module_pool);
	switch_mutex_init(&globals.flag_mutex, SWITCH_MUTEX_NESTED, module_pool);



	if (switch_event_reserve_subclass(MY_EVENT_RINGING) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Couldn't register subclass!");
		return SWITCH_STATUS_GENERR;
	}

	globals.read_frame.data = globals.databuf;
	globals.read_frame.buflen = sizeof(globals.databuf);
	globals.cng_frame.data = globals.cngbuf;
	globals.cng_frame.buflen = sizeof(globals.cngbuf);
	globals.cng_frame.datalen = sizeof(globals.cngbuf);
	switch_set_flag((&globals.cng_frame), SFF_CNG);

	/* indicate that the module should continue to be loaded */
	return SWITCH_STATUS_SUCCESS;
}


static switch_status_t load_config(void)
{
	char *cf = "alsa.conf";
	switch_xml_t cfg, xml, settings, param;

	if (!(xml = switch_xml_open_cfg(cf, &cfg, NULL))) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Open of %s failed\n", cf);
		return SWITCH_STATUS_TERM;
	}

	memset(&globals, 0, sizeof(globals));


	if ((settings = switch_xml_child(cfg, "settings"))) {
		for (param = switch_xml_child(settings, "param"); param; param = param->next) {
			char *var = (char *) switch_xml_attr_soft(param, "name");
			char *val = (char *) switch_xml_attr_soft(param, "value");

			if (!strcmp(var, "debug")) {
				globals.debug = atoi(val);
			} else if (!strcmp(var, "ring-interval")) {
				globals.ring_interval = atoi(val);
			} else if (!strcmp(var, "ring-file")) {
				set_global_ring_file(val);
			} else if (!strcmp(var, "hold-file")) {
				set_global_hold_file(val);
			} else if (!strcmp(var, "timer-name")) {
				set_global_timer_name(val);
			} else if (!strcmp(var, "device-name")) {
				set_global_device_name(val);
			} else if (!strcmp(var, "sample-rate")) {
				globals.sample_rate = atoi(val);
			} else if (!strcmp(var, "codec-ms")) {
				int tmp = atoi(val);
				if (SWITCH_ACCEPTABLE_INTERVAL(tmp)) {
					globals.codec_ms = tmp;
				} else {
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING,
									  "codec-ms must be multipe of 10 and less than %d, Using default of 20\n", SWITCH_MAX_INTERVAL);
				}
			} else if (!strcmp(var, "dialplan")) {
				set_global_dialplan(val);
			} else if (!strcmp(var, "cid-name")) {
				set_global_cid_name(val);
			} else if (!strcmp(var, "cid-num")) {
				set_global_cid_num(val);
			}
		}
	}

	if (!globals.dialplan) {
		set_global_dialplan("default");
	}

	if (!globals.device_name) {
		set_global_device_name("default");
	}

	if (!globals.sample_rate) {
		globals.sample_rate = 8000;
	}

	if (!globals.codec_ms) {
		globals.codec_ms = 20;
	}

	if (!globals.ring_interval) {
		globals.ring_interval = 5;
	}

	if (!globals.timer_name) {
		set_global_timer_name("soft");
	}

	switch_xml_free(xml);

	return SWITCH_STATUS_SUCCESS;
}

SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_alsa_shutdown)
{
	deactivate_audio_device();

	if (switch_core_codec_ready(&globals.read_codec)) {
		switch_core_codec_destroy(&globals.read_codec);
	}

	if (switch_core_codec_ready(&globals.write_codec)) {
		switch_core_codec_destroy(&globals.write_codec);
	}
	switch_core_hash_destroy(&globals.call_hash);

	switch_event_free_subclass(MY_EVENT_RINGING);
	switch_safe_free(globals.dialplan);
	switch_safe_free(globals.cid_name);
	switch_safe_free(globals.cid_num);
	switch_safe_free(globals.ring_file);
	switch_safe_free(globals.hold_file);
	switch_safe_free(globals.timer_name);
	switch_safe_free(globals.device_name);

	return SWITCH_STATUS_SUCCESS;
}



static switch_status_t engage_device(unsigned int sample_rate, int codec_ms)
{
	int err = 0;
	snd_pcm_hw_params_t *hw_params = NULL;
	char *device = globals.device_name;

	if (globals.audio_stream_in && globals.audio_stream_out) {
		return SWITCH_STATUS_SUCCESS;
	}

	if (globals.audio_stream_in) {
		snd_pcm_close(globals.audio_stream_in);
		globals.audio_stream_in = NULL;
	}

	if (globals.audio_stream_out) {
		snd_pcm_close(globals.audio_stream_out);
		globals.audio_stream_out = NULL;
	}

	if (!sample_rate) {
		sample_rate = globals.sample_rate;
	}

	if (!codec_ms) {
		codec_ms = globals.codec_ms;
	}

	if (switch_core_codec_init(&globals.read_codec,
							   "L16",
							   NULL, sample_rate, codec_ms, 1, SWITCH_CODEC_FLAG_ENCODE | SWITCH_CODEC_FLAG_DECODE, NULL, NULL) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Can't load codec?\n");
		return SWITCH_STATUS_FALSE;
	} else {
		if (switch_core_codec_init(&globals.write_codec,
								   "L16",
								   NULL,
								   sample_rate, codec_ms, 1, SWITCH_CODEC_FLAG_ENCODE | SWITCH_CODEC_FLAG_DECODE, NULL, NULL) != SWITCH_STATUS_SUCCESS) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Can't load codec?\n");
			switch_core_codec_destroy(&globals.read_codec);
			return SWITCH_STATUS_FALSE;
		}
	}

	if (switch_core_timer_init(&globals.timer,
							   globals.timer_name, codec_ms, globals.read_codec.implementation->samples_per_packet,
							   module_pool) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "setup timer failed!\n");
		switch_core_codec_destroy(&globals.read_codec);
		switch_core_codec_destroy(&globals.write_codec);
		return SWITCH_STATUS_FALSE;
	}



	globals.read_frame.rate = sample_rate;
	globals.read_frame.codec = &globals.read_codec;



	switch_mutex_lock(globals.device_lock);
	/* LOCKED ************************************************************************************************** */


	if ((err = snd_pcm_open(&globals.audio_stream_out, device, SND_PCM_STREAM_PLAYBACK, 0)) < 0) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "cannot open audio device %s (%s)\n", device, snd_strerror(err));
		goto fail;
	}

	if ((err = snd_pcm_hw_params_malloc(&hw_params)) < 0) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "cannot allocate hardware parameter structure (%s)\n", snd_strerror(err));
		goto fail;
	}

	if ((err = snd_pcm_hw_params_any(globals.audio_stream_out, hw_params)) < 0) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "cannot initialize hardware parameter structure (%s)\n", snd_strerror(err));
		goto fail;
	}

	if ((err = snd_pcm_hw_params_set_access(globals.audio_stream_out, hw_params, SND_PCM_ACCESS_RW_INTERLEAVED)) < 0) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "cannot set access type (%s)\n", snd_strerror(err));
		goto fail;
	}

	if ((err = snd_pcm_hw_params_set_format(globals.audio_stream_out, hw_params, SND_PCM_FORMAT_S16_LE)) < 0) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "cannot set sample format (%s)\n", snd_strerror(err));
		goto fail;
	}

	if ((err = snd_pcm_hw_params_set_rate_near(globals.audio_stream_out, hw_params, &sample_rate, 0)) < 0) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "cannot set sample rate (%s)\n", snd_strerror(err));
		goto fail;
	}

	if ((err = snd_pcm_hw_params_set_channels(globals.audio_stream_out, hw_params, 1)) < 0) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "cannot set channel count (%s)\n", snd_strerror(err));
		goto fail;
	}

	if ((err = snd_pcm_hw_params(globals.audio_stream_out, hw_params)) < 0) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "cannot set parameters (%s)\n", snd_strerror(err));
		goto fail;
	}


	snd_pcm_hw_params_free(hw_params);
	hw_params = NULL;


	if ((err = snd_pcm_open(&globals.audio_stream_in, device, SND_PCM_STREAM_CAPTURE, 0)) < 0) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "cannot open audio device %s (%s)\n", device, snd_strerror(err));
		goto fail;
	}

	if ((err = snd_pcm_hw_params_malloc(&hw_params)) < 0) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "cannot allocate hardware parameter structure (%s)\n", snd_strerror(err));
		goto fail;
	}

	if ((err = snd_pcm_hw_params_any(globals.audio_stream_in, hw_params)) < 0) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "cannot initialize hardware parameter structure (%s)\n", snd_strerror(err));
		goto fail;
	}

	if ((err = snd_pcm_hw_params_set_access(globals.audio_stream_in, hw_params, SND_PCM_ACCESS_RW_INTERLEAVED)) < 0) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "cannot set access type (%s)\n", snd_strerror(err));
		goto fail;
	}

	if ((err = snd_pcm_hw_params_set_format(globals.audio_stream_in, hw_params, SND_PCM_FORMAT_S16_LE)) < 0) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "cannot set sample format (%s)\n", snd_strerror(err));
		goto fail;
	}

	if ((err = snd_pcm_hw_params_set_rate_near(globals.audio_stream_in, hw_params, &sample_rate, 0)) < 0) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "cannot set sample rate (%s)\n", snd_strerror(err));
		goto fail;
	}

	if ((err = snd_pcm_hw_params_set_channels(globals.audio_stream_in, hw_params, 1)) < 0) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "cannot set channel count (%s)\n", snd_strerror(err));
		goto fail;
	}

	if ((err = snd_pcm_hw_params_set_period_time(globals.audio_stream_in, hw_params, globals.read_codec.implementation->microseconds_per_packet, 0)) < 0) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "cannot set period time (%s)\n", snd_strerror(err));
		goto fail;
	}

	if ((err = snd_pcm_hw_params_set_period_size(globals.audio_stream_in, hw_params, globals.read_codec.implementation->samples_per_packet, 0)) < 0) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "cannot set period size (%s)\n", snd_strerror(err));
		goto fail;
	}

	if ((err = snd_pcm_hw_params(globals.audio_stream_in, hw_params)) < 0) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "cannot set parameters (%s)\n", snd_strerror(err));
		goto fail;
	}

	if (hw_params) {
		snd_pcm_hw_params_free(hw_params);
		hw_params = NULL;
	}

	if ((err = snd_pcm_prepare(globals.audio_stream_out)) < 0) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "cannot prepare audio interface for use (%s)\n", snd_strerror(err));
		goto fail;
	}

	if ((err = snd_pcm_start(globals.audio_stream_out)) < 0) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "cannot start audio interface for use (%s)\n", snd_strerror(err));
		goto fail;
	}

	if ((err = snd_pcm_prepare(globals.audio_stream_in)) < 0) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "cannot prepare audio interface for use (%s)\n", snd_strerror(err));
		goto fail;
	}

	if ((err = snd_pcm_start(globals.audio_stream_in)) < 0) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "cannot start audio interface for use (%s)\n", snd_strerror(err));
		goto fail;
	}

	switch_mutex_unlock(globals.device_lock);
	return SWITCH_STATUS_SUCCESS;

  fail:

	if (hw_params) {
		snd_pcm_hw_params_free(hw_params);
		hw_params = NULL;
	}

	switch_mutex_unlock(globals.device_lock);

	if (globals.audio_stream_in) {
		snd_pcm_close(globals.audio_stream_in);
	}

	if (globals.audio_stream_out) {
		snd_pcm_close(globals.audio_stream_out);
	}

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Can't open audio device!\n");
	switch_core_codec_destroy(&globals.read_codec);
	switch_core_codec_destroy(&globals.write_codec);

	return SWITCH_STATUS_FALSE;



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
			switch_clear_flag_locked(tp, TFLAG_MASTER);
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
		//switch_set_flag_locked(tech_pvt, TFLAG_HUP);
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
				switch_channel_answer(channel);
				add_pvt(tp, PA_MASTER);
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
			switch_channel_answer(channel);
			add_pvt(tp, PA_MASTER);
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

	if (!argv[0]) {
		stream->write_function(stream, "FAIL:Usage: call <dest>\n");
		return SWITCH_STATUS_SUCCESS;
	}
	dest = argv[0];

	if ((session = switch_core_session_request(alsa_endpoint_interface, SWITCH_CALL_DIRECTION_INBOUND, SOF_NONE, NULL)) != 0) {
		private_t *tech_pvt;
		switch_channel_t *channel;
		char *dialplan = globals.dialplan;
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
			globals.flags = GFLAG_EAR | GFLAG_MOUTH;
		} else {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Hey where is my memory pool?\n");
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
																  NULL,
																  dialplan, cid_name, cid_num, ip, NULL, NULL, NULL, (char *) modname, NULL, dest)) != 0) {
			char name[128];
			snprintf(name, sizeof(name), "alsa/%s", tech_pvt->caller_profile->destination_number ? tech_pvt->caller_profile->destination_number : modname);
			switch_channel_set_name(channel, name);

			switch_channel_set_caller_profile(channel, tech_pvt->caller_profile);
		}
		tech_pvt->session = session;
		if ((status = engage_device(tech_pvt->sample_rate, tech_pvt->codec_ms)) == SWITCH_STATUS_SUCCESS) {
			switch_set_flag_locked(tech_pvt, TFLAG_ANSWER);
			switch_channel_mark_answered(channel);
			switch_channel_set_state(channel, CS_INIT);

			if (switch_core_session_thread_launch(tech_pvt->session) != SWITCH_STATUS_SUCCESS) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Error spawning thread\n");
				switch_core_session_destroy(&session);
				stream->write_function(stream, "FAIL:Thread Error!\n");
			} else {
				add_pvt(tech_pvt, PA_MASTER);
				stream->write_function(stream, "SUCCESS:%s:%s\n", tech_pvt->call_id, switch_core_session_get_uuid(tech_pvt->session));
			}
		} else {
			switch_core_session_destroy(&session);
			stream->write_function(stream, "FAIL:Device Error!\n");
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
		"alsa help\n"
		"alsa call <dest> [<dialplan> <cid_name> <cid_num> <rate>]\n"
		"alsa answer [<call_id>]\n"
		"alsa hangup [<call_id>]\n"
		"alsa list\n"
		"alsa switch [<call_id>|none]\n"
		"alsa dtmf <digit string>\n"
		"alsa flags [on|off] [ear] [mouth]\n" "--------------------------------------------------------------------------------\n";

	if (stream->param_event) {
		http = switch_event_get_header(stream->param_event, "http-host");
	}

	if (http) {
#if 0
		switch_event_header_t *hp;
		stream->write_function(stream, "<pre>");
		for (hp = stream->param_event->headers; hp; hp = hp->next) {
			stream->write_function(stream, "[%s]=[%s]\n", hp->name, hp->value);
		}
		stream->write_function(stream, "</pre>");
#endif

		stream->write_function(stream, "Content-type: text/html\n\n");

		wcmd = switch_str_nil(switch_event_get_header(stream->param_event, "wcmd"));
		action = switch_event_get_header(stream->param_event, "action");

		if (action) {
			if (strlen(action) == 1) {
				snprintf(cmd_buf, sizeof(cmd_buf), "dtmf %s", action);
				cmd = cmd_buf;
			} else if (!strcmp(action, "mute")) {
				snprintf(cmd_buf, sizeof(cmd_buf), "flags off mouth");
				cmd = cmd_buf;
			} else if (!strcmp(action, "unmute")) {
				snprintf(cmd_buf, sizeof(cmd_buf), "flags on mouth");
				cmd = cmd_buf;
			} else if (!strcmp(action, "switch")) {
				snprintf(cmd_buf, sizeof(cmd_buf), "switch %s", wcmd);
				cmd = cmd_buf;
			} else if (!strcmp(action, "call")) {
				snprintf(cmd_buf, sizeof(cmd_buf), "call %s", wcmd);
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
	}

	if (func) {
		if (http) {
			stream->write_function(stream, "<pre>");
		}
		status = func(&argv[lead], argc - lead, stream);
		if (http) {
			stream->write_function(stream, "\n\n</pre>");
		}
	} else {
		stream->write_function(stream, "Unknown Command [%s]\n", argv[0]);
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
