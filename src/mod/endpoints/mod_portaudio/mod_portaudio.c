/* 
 * FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
 * Copyright (C) 2005-2010, Anthony Minessale II <anthm@freeswitch.org>
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
 *
 *
 * mod_portaudio.c -- PortAudio Endpoint Module
 *
 */

#include "switch.h"

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "pablio.h"
#include <string.h>

#define MY_EVENT_RINGING "portaudio::ringing"
#define MY_EVENT_MAKE_CALL "portaudio::makecall"
#define MY_EVENT_ERROR_AUDIO_DEV "portaudio::audio_dev_error"
#define SWITCH_PA_CALL_ID_VARIABLE "pa_call_id"

SWITCH_MODULE_LOAD_FUNCTION(mod_portaudio_load);
SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_portaudio_shutdown);
//SWITCH_MODULE_RUNTIME_FUNCTION(mod_portaudio_runtime);
SWITCH_MODULE_DEFINITION(mod_portaudio, mod_portaudio_load, mod_portaudio_shutdown, NULL);

static switch_memory_pool_t *module_pool = NULL;
switch_endpoint_interface_t *portaudio_endpoint_interface;

#define SAMPLE_TYPE  paInt16
typedef int16_t SAMPLE;

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
	struct private_object *next;
};

typedef struct private_object private_t;

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
	int ringdev;
	int indev;
	int outdev;
	int call_id;
	switch_hash_t *call_hash;
	switch_mutex_t *device_lock;
	switch_mutex_t *pvt_lock;
	switch_mutex_t *flag_mutex;
	switch_mutex_t *pa_mutex;
	int sample_rate;
	int codec_ms;
	PABLIO_Stream *audio_stream;
	PABLIO_Stream *ring_stream;
	switch_codec_t read_codec;
	switch_codec_t write_codec;
	switch_frame_t read_frame;
	switch_frame_t cng_frame;
	unsigned char databuf[SWITCH_RECOMMENDED_BUFFER_SIZE];
	unsigned char cngbuf[SWITCH_RECOMMENDED_BUFFER_SIZE];
	private_t *call_list;
	int ring_interval;
	GFLAGS flags;
	switch_timer_t read_timer;
	switch_timer_t write_timer;
	switch_timer_t hold_timer;
	int dual_streams;
	time_t deactivate_timer;
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
static switch_status_t engage_device(int restart);
static switch_status_t engage_ring_device(void);
static void deactivate_ring_device(void);
static int dump_info(int verbose);
static switch_status_t load_config(void);
static int get_dev_by_name(char *name, int in);
static int get_dev_by_number(char *numstr, int in);
SWITCH_STANDARD_API(pa_cmd);

/* 
   State methods they get called when the state changes to the specific state 
   returning SWITCH_STATUS_SUCCESS tells the core to execute the standard state method next
   so if you fully implement the state you can return SWITCH_STATUS_FALSE to skip it.
*/
static switch_status_t channel_on_init(switch_core_session_t *session)
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

		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "%s CHANNEL INIT %d %d\n",
						  switch_channel_get_name(channel), switch_channel_get_state(channel), switch_test_flag(tech_pvt, TFLAG_ANSWER));

		if (engage_device(0) != SWITCH_STATUS_SUCCESS) {
			switch_channel_hangup(channel, SWITCH_CAUSE_DESTINATION_OUT_OF_ORDER);
			return SWITCH_STATUS_FALSE;
		}

		if (switch_test_flag(tech_pvt, TFLAG_OUTBOUND) && !switch_test_flag(tech_pvt, TFLAG_AUTO_ANSWER)) {
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

					if (engage_ring_device() != SWITCH_STATUS_SUCCESS) {
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

		if (switch_test_flag(tech_pvt, TFLAG_AUTO_ANSWER)) {
			switch_mutex_lock(globals.pvt_lock);
			add_pvt(tech_pvt, PA_MASTER);
			switch_channel_mark_answered(channel);
			switch_set_flag(tech_pvt, TFLAG_ANSWER);
			switch_mutex_unlock(globals.pvt_lock);
			switch_yield(1000000);
		} else {
			switch_channel_mark_ring_ready(channel);
		}

		while (switch_channel_get_state(channel) == CS_INIT && !switch_test_flag(tech_pvt, TFLAG_ANSWER)) {
			switch_size_t olen = globals.read_timer.samples;

			if (switch_micro_time_now() - last >= waitsec) {
				char buf[512];
				switch_event_t *event;

				switch_snprintf(buf, sizeof(buf), "BRRRRING! BRRRRING! call %s\n", tech_pvt->call_id);

				if (switch_event_create_subclass(&event, SWITCH_EVENT_CUSTOM, MY_EVENT_RINGING) == SWITCH_STATUS_SUCCESS) {
					switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "event_info", buf);
					switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "call_id", tech_pvt->call_id);
					switch_channel_event_set_data(channel, event);
					switch_event_fire(&event);
				}
				switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "%s\n", buf);
				last = switch_micro_time_now();
			}

			if (ring_file) {
				if (switch_core_timer_next(&globals.read_timer) != SWITCH_STATUS_SUCCESS) {
					switch_core_file_close(&fh);
					break;
				}
				switch_core_file_read(&fh, abuf, &olen);
				if (olen == 0) {
					unsigned int pos = 0;
					switch_core_file_seek(&fh, &pos, 0, SEEK_SET);
				}

				if (globals.ring_stream) {
					WriteAudioStream(globals.ring_stream, abuf, (long) olen, &globals.write_timer);
				}
			} else {
				switch_yield(10000);
			}
		}
		switch_clear_flag_locked((&globals), GFLAG_RING);
	}

	if (ring_file) {
		deactivate_ring_device();
		switch_core_file_close(&fh);
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

static void deactivate_audio_device(void)
{
	if (!globals.audio_stream) {
		return;
	}

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Stop audio device.\n");

	switch_mutex_lock(globals.device_lock);
	/* LOCKED ************************************************************************************************** */

	if (globals.audio_stream) {
		if (globals.ring_stream == globals.audio_stream) {
			globals.ring_stream = NULL;
		}
		CloseAudioStream(globals.audio_stream);
		globals.audio_stream = NULL;
	}

	/* UNLOCKED ************************************************************************************************* */
	switch_mutex_unlock(globals.device_lock);
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

	if (globals.write_timer.timer_interface) {
		switch_core_timer_destroy(&globals.write_timer);
	}

	if (globals.hold_timer.timer_interface) {
		switch_core_timer_destroy(&globals.hold_timer);
	}



}

static void deactivate_ring_device(void)
{
	if (!globals.ring_stream) {
		return;
	}

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Stop ring device.\n");
	switch_mutex_lock(globals.device_lock);
	if (globals.ring_stream && globals.ring_stream != globals.audio_stream) {
		CloseAudioStream(globals.ring_stream);
	}
	globals.ring_stream = NULL;
	switch_mutex_unlock(globals.device_lock);
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
		switch_core_session_set_read_codec(tech_pvt->session, &globals.read_codec);
		switch_core_session_set_write_codec(tech_pvt->session, &globals.write_codec);
		switch_mutex_unlock(globals.pa_mutex);
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
		globals.deactivate_timer = switch_epoch_time_now(NULL) + 2;
		deactivate_audio_device();
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

static switch_status_t channel_on_hangup(switch_core_session_t *session)
{
	private_t *tech_pvt = switch_core_session_get_private(session);
	switch_assert(tech_pvt != NULL);

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

static switch_status_t channel_read_frame(switch_core_session_t *session, switch_frame_t **frame, switch_io_flag_t flags, int stream_id)
{
	private_t *tech_pvt = switch_core_session_get_private(session);
	int samples = 0;
	switch_status_t status = SWITCH_STATUS_FALSE;
	switch_assert(tech_pvt != NULL);

	if (!globals.audio_stream) {
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
	samples = ReadAudioStream(globals.audio_stream, globals.read_frame.data, globals.read_codec.implementation->samples_per_packet, &globals.read_timer);
	switch_mutex_unlock(globals.device_lock);

	if (samples) {
		globals.read_frame.datalen = samples * 2;
		globals.read_frame.samples = samples;

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

static switch_status_t channel_write_frame(switch_core_session_t *session, switch_frame_t *frame, switch_io_flag_t flags, int stream_id)
{
	switch_status_t status = SWITCH_STATUS_FALSE;
	private_t *tech_pvt = switch_core_session_get_private(session);
	switch_assert(tech_pvt != NULL);

	if (!globals.audio_stream) {
		return SWITCH_STATUS_FALSE;
	}

	if (switch_test_flag(tech_pvt, TFLAG_HUP)) {
		return SWITCH_STATUS_FALSE;
	}

	if (!is_master(tech_pvt) || !switch_test_flag(tech_pvt, TFLAG_IO)) {
		return SWITCH_STATUS_SUCCESS;
	}

	if (globals.audio_stream) {
		if (switch_test_flag((&globals), GFLAG_EAR)) {
			WriteAudioStream(globals.audio_stream, (short *) frame->data, (int) (frame->datalen / sizeof(SAMPLE)), &globals.write_timer);
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

switch_state_handler_table_t portaudio_event_handlers = {
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

switch_io_routines_t portaudio_io_routines = {
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

	if ((*new_session = switch_core_session_request(portaudio_endpoint_interface, SWITCH_CALL_DIRECTION_OUTBOUND, pool)) != 0) {
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
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(*new_session), SWITCH_LOG_CRIT, "Hey where is my memory pool?\n");
			switch_core_session_destroy(new_session);
			return SWITCH_CAUSE_DESTINATION_OUT_OF_ORDER;
		}

		if (outbound_profile) {
			char name[128];
			const char *id = !zstr(outbound_profile->caller_id_number) ? outbound_profile->caller_id_number : "na";
			switch_snprintf(name, sizeof(name), "portaudio/%s", id);

			switch_channel_set_name(channel, name);

			caller_profile = switch_caller_profile_clone(*new_session, outbound_profile);
			switch_channel_set_caller_profile(channel, caller_profile);
			tech_pvt->caller_profile = caller_profile;
			if (outbound_profile->destination_number && !strcasecmp(outbound_profile->destination_number, "auto_answer")) {
				switch_set_flag(tech_pvt, TFLAG_AUTO_ANSWER);
			}

		} else {
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(*new_session), SWITCH_LOG_ERROR, "Doh! no caller profile\n");
			switch_core_session_destroy(new_session);
			return SWITCH_CAUSE_DESTINATION_OUT_OF_ORDER;
		}

		switch_set_flag_locked(tech_pvt, TFLAG_OUTBOUND);
		switch_channel_set_state(channel, CS_INIT);
		return SWITCH_CAUSE_SUCCESS;
	}

	return SWITCH_CAUSE_DESTINATION_OUT_OF_ORDER;
}


SWITCH_MODULE_LOAD_FUNCTION(mod_portaudio_load)
{
	switch_status_t status;
	switch_api_interface_t *api_interface;

	module_pool = pool;

	Pa_Initialize();

	memset(&globals, 0, sizeof(globals));
	switch_core_hash_init(&globals.call_hash, module_pool);
	switch_mutex_init(&globals.device_lock, SWITCH_MUTEX_NESTED, module_pool);
	switch_mutex_init(&globals.pvt_lock, SWITCH_MUTEX_NESTED, module_pool);
	switch_mutex_init(&globals.flag_mutex, SWITCH_MUTEX_NESTED, module_pool);
	switch_mutex_init(&globals.pa_mutex, SWITCH_MUTEX_NESTED, module_pool);

	globals.read_frame.data = globals.databuf;
	globals.read_frame.buflen = sizeof(globals.databuf);
	globals.cng_frame.data = globals.cngbuf;
	globals.cng_frame.buflen = sizeof(globals.cngbuf);
	globals.cng_frame.datalen = switch_samples_per_packet(globals.sample_rate, globals.codec_ms) * 2;
	switch_set_flag((&globals.cng_frame), SFF_CNG);

	/* dual streams makes portaudio on solaris choke */
#if defined(sun) || defined(__sun)
	globals.dual_streams = 0;
#endif

	if ((status = load_config()) != SWITCH_STATUS_SUCCESS) {
		return status;
	}

	if (dump_info(0)) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Couldn't find any audio devices!\n");
		return SWITCH_STATUS_TERM;
	}

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG,
					  "Input Device: %d, Output Device: %d, Ring Device: %d Sample Rate: %d MS: %d\n", globals.indev,
					  globals.outdev, globals.ringdev, globals.sample_rate, globals.codec_ms);


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
	portaudio_endpoint_interface = switch_loadable_module_create_interface(*module_interface, SWITCH_ENDPOINT_INTERFACE);
	portaudio_endpoint_interface->interface_name = "portaudio";
	portaudio_endpoint_interface->io_routines = &portaudio_io_routines;
	portaudio_endpoint_interface->state_handler = &portaudio_event_handlers;

	SWITCH_ADD_API(api_interface, "pa", "PortAudio", pa_cmd, "<command> [<args>]");
	switch_console_set_complete("add pa help");
	switch_console_set_complete("add pa dump");
	switch_console_set_complete("add pa call");
	switch_console_set_complete("add pa answer");
	switch_console_set_complete("add pa hangup");
	switch_console_set_complete("add pa list");
	switch_console_set_complete("add pa switch");
	switch_console_set_complete("add pa dtmf");
	switch_console_set_complete("add pa flags");
	switch_console_set_complete("add pa devlist");
	switch_console_set_complete("add pa indev");
	switch_console_set_complete("add pa outdev");
	switch_console_set_complete("add pa ringdev");
	switch_console_set_complete("add pa ringfile");
	switch_console_set_complete("add pa play");
	switch_console_set_complete("add pa looptest");

	/* indicate that the module should continue to be loaded */
	return SWITCH_STATUS_SUCCESS;
}


static switch_status_t load_config(void)
{
	char *cf = "portaudio.conf";
	switch_xml_t cfg, xml, settings, param;
	switch_status_t status = SWITCH_STATUS_SUCCESS;

	if (!(xml = switch_xml_open_cfg(cf, &cfg, NULL))) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Open of %s failed\n", cf);
		return SWITCH_STATUS_TERM;
	}

	globals.dual_streams = 0;

	globals.indev = globals.outdev = globals.ringdev = -1;
	globals.sample_rate = 8000;

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
			} else if (!strcmp(var, "dual-streams")) {
				if (switch_true(val)) {
					globals.dual_streams = 1;
				} else {
					globals.dual_streams = 0;
				}
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
			} else if (!strcmp(var, "indev")) {
				if (*val == '#') {
					globals.indev = get_dev_by_number(val + 1, 1);
				} else {
					globals.indev = get_dev_by_name(val, 1);
				}
			} else if (!strcmp(var, "outdev")) {
				if (*val == '#') {
					globals.outdev = get_dev_by_number(val + 1, 0);
				} else {
					globals.outdev = get_dev_by_name(val, 0);
				}
			} else if (!strcmp(var, "ringdev")) {
				if (*val == '#') {
					globals.ringdev = get_dev_by_number(val + 1, 0);
				} else {
					globals.ringdev = get_dev_by_name(val, 0);
				}
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

	if (!globals.ring_interval) {
		globals.ring_interval = 5;
	}

	if (!globals.timer_name) {
		set_global_timer_name("soft");
	}

	if (globals.indev < 0) {
		globals.indev = get_dev_by_name(NULL, 1);
		if (globals.indev > -1) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "Switching to default input device\n");
		} else {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Cannot find an input device\n");
			status = SWITCH_STATUS_GENERR;
		}
	}

	if (globals.outdev < 0) {
		globals.outdev = get_dev_by_name(NULL, 0);
		if (globals.outdev > -1) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "Switching to default output device\n");
		} else {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Cannot find an input device\n");
			status = SWITCH_STATUS_GENERR;
		}
	}

	if (globals.ringdev < 0) {
		if (globals.outdev > -1) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "Invalid ring device configured using output device\n");
			globals.ringdev = globals.outdev;
		}
	}

	switch_xml_free(xml);

	return status;
}

SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_portaudio_shutdown)
{

	deactivate_audio_device();
	deactivate_ring_device();
	destroy_codecs();

	Pa_Terminate();
	switch_core_hash_destroy(&globals.call_hash);

	switch_event_free_subclass(MY_EVENT_RINGING);
	switch_event_free_subclass(MY_EVENT_MAKE_CALL);
	switch_event_free_subclass(MY_EVENT_ERROR_AUDIO_DEV);

	switch_safe_free(globals.dialplan);
	switch_safe_free(globals.context);
	switch_safe_free(globals.cid_name);
	switch_safe_free(globals.cid_num);
	switch_safe_free(globals.ring_file);
	switch_safe_free(globals.hold_file);
	switch_safe_free(globals.timer_name);

	return SWITCH_STATUS_SUCCESS;
}

static int get_dev_by_number(char *numstr, int in)
{
	int numDevices = Pa_GetDeviceCount();
	const PaDeviceInfo *pdi;
	char *end_ptr;
	int number;

	number = (int) strtol(numstr, &end_ptr, 10);

	if (end_ptr == numstr || number < 0) {
		return -1;
	}

	if (number > -1 && number < numDevices && (pdi = Pa_GetDeviceInfo(number))) {
		if (in && pdi->maxInputChannels) {
			return number;
		} else if (!in && pdi->maxOutputChannels) {
			return number;
		}
	}

	return -1;
}

static int get_dev_by_name(char *name, int in)
{
	int i;
	int numDevices;
	const PaDeviceInfo *pdi;
	numDevices = Pa_GetDeviceCount();

	if (numDevices < 0) {
		switch_log_printf(SWITCH_CHANNEL_LOG_CLEAN, SWITCH_LOG_ERROR, "ERROR: Pa_CountDevices returned 0x%x\n", numDevices);
		return -2;
	}

	for (i = 0; i < numDevices; i++) {
		int match = 0;
		pdi = Pa_GetDeviceInfo(i);

		if (zstr(name)) {
			match = 1;
		} else if (pdi && pdi->name && strstr(pdi->name, name)) {
			match = 1;
		}

		if (match) {
			if (in && pdi->maxInputChannels) {
				return i;
			} else if (!in && pdi->maxOutputChannels) {
				return i;
			}
		}
	}

	return -1;
}


/*******************************************************************/
static void PrintSupportedStandardSampleRates(const PaStreamParameters * inputParameters, const PaStreamParameters * outputParameters)
{
	int i, printCount, cr = 7;
	PaError err;
	static double standardSampleRates[] = { 8000.0, 9600.0, 11025.0, 12000.0, 16000.0, 22050.0, 24000.0, 32000.0,
		44100.0, 48000.0, 88200.0, 96000.0, 192000.0, -1
	};

	printCount = cr;
	for (i = 0; standardSampleRates[i] > 0; i++) {
		err = Pa_IsFormatSupported(inputParameters, outputParameters, standardSampleRates[i]);
		if (err == paFormatIsSupported) {
			if (printCount == cr) {
				printCount = 0;
				switch_log_printf(SWITCH_CHANNEL_LOG_CLEAN, SWITCH_LOG_INFO, "\n\t%0.2f", standardSampleRates[i]);
			} else {
				switch_log_printf(SWITCH_CHANNEL_LOG_CLEAN, SWITCH_LOG_INFO, ", %0.2f", standardSampleRates[i]);
			}
			printCount++;
		}
	}
	if (!printCount) {
		switch_log_printf(SWITCH_CHANNEL_LOG_CLEAN, SWITCH_LOG_INFO, " None\n");
	} else {
		switch_log_printf(SWITCH_CHANNEL_LOG_CLEAN, SWITCH_LOG_INFO, "\n");
	}
}

/*******************************************************************/

static switch_status_t devlist(char **argv, int argc, switch_stream_handle_t *stream)
{
	int i, numDevices, prev;
	const PaDeviceInfo *deviceInfo;
	const PaHostApiInfo *hostApiInfo;

	numDevices = Pa_GetDeviceCount();

	if (numDevices < 0) {
		return SWITCH_STATUS_SUCCESS;
	}

	if (argv[0] && !strcasecmp(argv[0], "xml")) {
		stream->write_function(stream, "<xml>\n\t<devices>\n");

		for (i = 0; i < numDevices; i++) {
			deviceInfo = Pa_GetDeviceInfo(i);
			hostApiInfo = Pa_GetHostApiInfo(deviceInfo->hostApi);
			stream->write_function(stream, "\t\t<device id=\"%d\" name=\"%s\" hostapi=\"%s\" inputs=\"%d\" outputs=\"%d\" />\n", i, deviceInfo->name,
								   hostApiInfo->name, deviceInfo->maxInputChannels, deviceInfo->maxOutputChannels);
		}

		stream->write_function(stream, "\t</devices>\n\t<bindings>\n"
							   "\t\t<ring device=\"%d\" />\n"
							   "\t\t<input device=\"%d\" />\n"
							   "\t\t<output device=\"%d\" />\n" "\t</bindings>\n</xml>\n", globals.ringdev, globals.indev, globals.outdev);
	} else {

		for (i = 0; i < numDevices; i++) {
			deviceInfo = Pa_GetDeviceInfo(i);
			hostApiInfo = Pa_GetHostApiInfo(deviceInfo->hostApi);

			stream->write_function(stream, "%d;%s(%s);%d;%d;", i, deviceInfo->name, hostApiInfo->name, deviceInfo->maxInputChannels,
								   deviceInfo->maxOutputChannels);

			prev = 0;
			if (globals.ringdev == i) {
				stream->write_function(stream, "r");
				prev = 1;
			}

			if (globals.indev == i) {
				if (prev) {
					stream->write_function(stream, ",");
				}
				stream->write_function(stream, "i");
				prev = 1;
			}

			if (globals.outdev == i) {
				if (prev) {
					stream->write_function(stream, ",");
				}
				stream->write_function(stream, "o");
				prev = 1;
			}

			stream->write_function(stream, "\n");

		}
	}

	return SWITCH_STATUS_SUCCESS;
}

static int dump_info(int verbose)
{
	int i, numDevices, defaultDisplayed;
	const PaDeviceInfo *deviceInfo;
	PaStreamParameters inputParameters, outputParameters;
	PaError err;
	const char *line = "--------------------------------------------------------------------------------\n";

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO,
					  "PortAudio version number = %d\nPortAudio version text = '%s'\n", Pa_GetVersion(), Pa_GetVersionText());
	if (globals.call_list) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "ERROR: Cannot use this command this while a call is in progress\n");
		return 0;
	}

	if (verbose < 0) {
		deactivate_audio_device();
		deactivate_ring_device();
		destroy_codecs();
		Pa_Terminate();
		Pa_Initialize();
		load_config();
		verbose = 0;
	}

	numDevices = Pa_GetDeviceCount();
	if (numDevices < 0) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "ERROR: Pa_CountDevices returned 0x%x\n", numDevices);
		err = numDevices;
		goto error;
	}

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "Number of devices = %d\n", numDevices);

	if (!verbose) {
		return 0;
	}

	for (i = 0; i < numDevices; i++) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "%s", line);
		deviceInfo = Pa_GetDeviceInfo(i);
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "Device #%d ", i);

		/* Mark global and API specific default devices */
		defaultDisplayed = 0;
		if (i == Pa_GetDefaultInputDevice()) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "**Default Input");
			defaultDisplayed = 1;

		} else if (i == Pa_GetHostApiInfo(deviceInfo->hostApi)->defaultInputDevice) {

			const PaHostApiInfo *hostInfo = Pa_GetHostApiInfo(deviceInfo->hostApi);
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "**Default %s Input", hostInfo->name);
			defaultDisplayed = 1;
		}

		if (i == Pa_GetDefaultOutputDevice()) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "**Default Output");
			defaultDisplayed = 1;

		} else if (i == Pa_GetHostApiInfo(deviceInfo->hostApi)->defaultOutputDevice) {

			const PaHostApiInfo *hostInfo = Pa_GetHostApiInfo(deviceInfo->hostApi);
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "**Default %s Output", hostInfo->name);
			defaultDisplayed = 1;
		}

		if (defaultDisplayed) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "** | ");
		}
		/* print device info fields */
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "Name: %s\n", deviceInfo->name);
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "Host: %s | ", Pa_GetHostApiInfo(deviceInfo->hostApi)->name);
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "inputs: %d | ", deviceInfo->maxInputChannels);
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "outputs: %d | ", deviceInfo->maxOutputChannels);

		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "Default rate: %8.2f\n", deviceInfo->defaultSampleRate);

		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "Default input latency: %.3f | ", deviceInfo->defaultLowInputLatency);
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "Default output latency: %.3f\n", deviceInfo->defaultLowOutputLatency);

		/* poll for standard sample rates */
		inputParameters.device = i;
		inputParameters.channelCount = deviceInfo->maxInputChannels;
		inputParameters.sampleFormat = paInt16;
		inputParameters.suggestedLatency = deviceInfo->defaultLowInputLatency;	/* ignored by Pa_IsFormatSupported() */
		inputParameters.hostApiSpecificStreamInfo = NULL;

		outputParameters.device = i;
		outputParameters.channelCount = deviceInfo->maxOutputChannels;
		outputParameters.sampleFormat = paInt16;
		outputParameters.suggestedLatency = deviceInfo->defaultLowOutputLatency;	/* ignored by Pa_IsFormatSupported() */
		outputParameters.hostApiSpecificStreamInfo = NULL;

		if (inputParameters.channelCount > 0) {

			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "half-duplex 16 bit %d channel input rates:", inputParameters.channelCount);
			PrintSupportedStandardSampleRates(&inputParameters, NULL);
		}

		if (outputParameters.channelCount > 0) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "half-duplex 16 bit %d channel output rates:", outputParameters.channelCount);
			PrintSupportedStandardSampleRates(NULL, &outputParameters);
		}

		if (inputParameters.channelCount > 0 && outputParameters.channelCount > 0) {

			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO,
							  "full-duplex 16 bit %d channel input, %d channel output rates:", inputParameters.channelCount,
							  outputParameters.channelCount);
			PrintSupportedStandardSampleRates(&inputParameters, &outputParameters);
		}
	}

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "%s", line);

	return 0;

  error:
	switch_log_printf(SWITCH_CHANNEL_LOG_CLEAN, SWITCH_LOG_ERROR, "An error occurred while using the portaudio stream\n");
	switch_log_printf(SWITCH_CHANNEL_LOG_CLEAN, SWITCH_LOG_ERROR, "Error number: %d\n", err);
	switch_log_printf(SWITCH_CHANNEL_LOG_CLEAN, SWITCH_LOG_ERROR, "Error message: %s\n", Pa_GetErrorText(err));
	return err;
}

static switch_status_t engage_device(int restart)
{
	PaStreamParameters inputParameters, outputParameters;
	PaError err;
	int sample_rate = globals.sample_rate;
	int codec_ms = globals.codec_ms;
	switch_event_t *event;

	switch_mutex_lock(globals.device_lock);
	while (globals.deactivate_timer > switch_epoch_time_now(NULL)) {
		switch_yield(1000000);
	}
	switch_mutex_unlock(globals.device_lock);


	if (restart) {
		deactivate_audio_device();
		deactivate_ring_device();
		destroy_codecs();
	}

	if (globals.read_timer.timer_interface) {
		switch_core_timer_sync(&globals.read_timer);
	}

	if (globals.write_timer.timer_interface) {
		switch_core_timer_sync(&globals.write_timer);
	}

	if (globals.audio_stream) {
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


	if (!globals.write_timer.timer_interface) {
		if (switch_core_timer_init(&globals.write_timer,
								   globals.timer_name, codec_ms, globals.read_codec.implementation->samples_per_packet,
								   module_pool) != SWITCH_STATUS_SUCCESS) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "setup timer failed!\n");
			switch_core_codec_destroy(&globals.read_codec);
			switch_core_codec_destroy(&globals.write_codec);
			switch_core_timer_destroy(&globals.read_timer);
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
			switch_core_timer_destroy(&globals.write_timer);
			return SWITCH_STATUS_FALSE;
		}
	}

	globals.read_frame.rate = sample_rate;
	globals.read_frame.codec = &globals.read_codec;

	switch_mutex_lock(globals.device_lock);
	/* LOCKED ************************************************************************************************** */
	inputParameters.device = globals.indev;
	inputParameters.channelCount = 1;
	inputParameters.sampleFormat = SAMPLE_TYPE;
	inputParameters.suggestedLatency = Pa_GetDeviceInfo(inputParameters.device)->defaultLowInputLatency;
	inputParameters.hostApiSpecificStreamInfo = NULL;
	outputParameters.device = globals.outdev;
	outputParameters.channelCount = 1;
	outputParameters.sampleFormat = SAMPLE_TYPE;
	outputParameters.suggestedLatency = Pa_GetDeviceInfo(outputParameters.device)->defaultLowOutputLatency;
	outputParameters.hostApiSpecificStreamInfo = NULL;
	//err = OpenAudioStream(&globals.audio_stream, NULL/*&inputParameters*/, &outputParameters, sample_rate, paClipOff,
	//globals.read_codec.implementation->samples_per_packet);
	err = OpenAudioStream(&globals.audio_stream, &inputParameters, &outputParameters, sample_rate, paClipOff,
						  globals.read_codec.implementation->samples_per_packet, globals.dual_streams);
	/* UNLOCKED ************************************************************************************************* */
	if (err != paNoError) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Error opening audio device retrying\n");
		switch_yield(1000000);
		err = OpenAudioStream(&globals.audio_stream, &inputParameters, &outputParameters, sample_rate, paClipOff,
							  globals.read_codec.implementation->samples_per_packet, globals.dual_streams);
	}

	switch_mutex_unlock(globals.device_lock);

	if (err != paNoError) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Can't open audio device\n");
		switch_core_codec_destroy(&globals.read_codec);
		switch_core_codec_destroy(&globals.write_codec);
		switch_core_timer_destroy(&globals.read_timer);
		switch_core_timer_destroy(&globals.write_timer);
		switch_core_timer_destroy(&globals.hold_timer);
		if (switch_event_create_subclass(&event, SWITCH_EVENT_CUSTOM, MY_EVENT_ERROR_AUDIO_DEV) == SWITCH_STATUS_SUCCESS) {
			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "Reason", Pa_GetErrorText(err));
			switch_event_fire(&event);
		}
		return SWITCH_STATUS_FALSE;
	}

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Engage audio device rate: %d channels %d\n", sample_rate, outputParameters.channelCount);

	engage_ring_device();

	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t engage_ring_device(void)
{
	PaStreamParameters outputParameters = { 0 };
	PaError err;
	int sample_rate = globals.sample_rate;
	int channels = 1;

	if (globals.ring_stream) {
		return SWITCH_STATUS_SUCCESS;
	}

	if (globals.ringdev == globals.outdev) {
		if (engage_device(0) == SWITCH_STATUS_SUCCESS) {
			globals.ring_stream = globals.audio_stream;
		} else {
			goto error;
		}
	} else {
		switch_mutex_lock(globals.device_lock);
		/* LOCKED ************************************************************************************************** */
		outputParameters.device = globals.ringdev;
		outputParameters.channelCount = channels;
		outputParameters.sampleFormat = SAMPLE_TYPE;
		outputParameters.suggestedLatency = Pa_GetDeviceInfo(outputParameters.device)->defaultLowOutputLatency;
		outputParameters.hostApiSpecificStreamInfo = NULL;
		err = OpenAudioStream(&globals.ring_stream, NULL,
							  &outputParameters, sample_rate, paClipOff, globals.read_codec.implementation->samples_per_packet, globals.dual_streams);
		/* UNLOCKED ************************************************************************************************* */
		switch_mutex_unlock(globals.device_lock);

		if (err != paNoError) {
			goto error;
		}
	}

	switch_yield(10000);
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Engage ring device rate: %d channels %d\n", sample_rate, channels);
	return SWITCH_STATUS_SUCCESS;


  error:

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Can't open ring device\n");
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
				add_pvt(tp, PA_MASTER);
				deactivate_ring_device();
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
			deactivate_ring_device();
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

	if ((session = switch_core_session_request(portaudio_endpoint_interface, SWITCH_CALL_DIRECTION_INBOUND, NULL)) != 0) {
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
			globals.flags = GFLAG_EAR | GFLAG_MOUTH;
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
			switch_snprintf(name, sizeof(name), "portaudio/%s",
							tech_pvt->caller_profile->destination_number ? tech_pvt->caller_profile->destination_number : modname);
			switch_channel_set_name(channel, name);

			switch_channel_set_caller_profile(channel, tech_pvt->caller_profile);
		}
		tech_pvt->session = session;
		if ((status = engage_device(0)) == SWITCH_STATUS_SUCCESS) {
			switch_set_flag_locked(tech_pvt, TFLAG_ANSWER);
			deactivate_ring_device();
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
	char *mycmd = NULL, *devname = NULL;
	switch_status_t status = SWITCH_STATUS_SUCCESS;
	pa_command_t func = NULL;
	int lead = 1, devval = 0;
	char *wcmd = NULL, *action = NULL;
	char cmd_buf[1024] = "";
	char *http = NULL;

	const char *usage_string = "USAGE:\n"
		"--------------------------------------------------------------------------------\n"
		"pa help\n"
		"pa dump\n"
		"pa rescan\n"
		"pa call <dest> [<dialplan> <cid_name> <cid_num>]\n"
		"pa answer [<call_id>]\n"
		"pa hangup [<call_id>]\n"
		"pa list\n"
		"pa switch [<call_id>|none]\n"
		"pa dtmf <digit string>\n"
		"pa flags [on|off] [ear] [mouth]\n"
		"pa devlist [xml]\n"
		"pa indev #<num>|<partial name>\n"
		"pa outdev #<num>|<partial name>\n"
		"pa ringdev #<num>|<partial name>\n"
		"pa play [ringtest|<filename>]\n"
		"pa ringfile [filename]\n" "pa looptest\n" "--------------------------------------------------------------------------------\n";


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
	} else if (!strcasecmp(argv[0], "devlist")) {
		func = devlist;
	} else if (!strcasecmp(argv[0], "rescan")) {

		if (globals.call_list) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "ERROR: Cannot use this command this while a call is in progress\n");
			goto done;
		}

		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "Looking for new devices.\n");
		dump_info(-1);
		goto done;
	} else if (!strcasecmp(argv[0], "dump")) {
		dump_info(1);
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
	} else if (argv[1] && !strcmp(argv[0], "indev")) {

		if (globals.call_list) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "ERROR: Cannot use this command this while a call is in progress\n");
			goto done;
		}

		if (*argv[1] == '#') {
			devval = get_dev_by_number(argv[1] + 1, 1);
		} else {
			devval = get_dev_by_name(argv[1], 1);
		}
		devname = "indev";
		if (devval > -1) {
			globals.indev = devval;
			//engage_device(1);
		}
	} else if (argv[1] && !strcmp(argv[0], "outdev")) {

		if (globals.call_list) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "ERROR: Cannot use this command this while a call is in progress\n");
			goto done;
		}

		if (*argv[1] == '#') {
			devval = get_dev_by_number(argv[1] + 1, 0);
		} else {
			devval = get_dev_by_name(argv[1], 0);
		}
		devname = "outdev";
		if (devval > -1) {
			globals.outdev = devval;
			//engage_device(1);
		}
	} else if (argv[1] && !strcmp(argv[0], "ringdev")) {

		if (globals.call_list) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "ERROR: Cannot use this command this while a call is in progress\n");
			goto done;
		}

		if (*argv[1] == '#') {
			devval = get_dev_by_number(argv[1] + 1, 0);
		} else {
			devval = get_dev_by_name(argv[1], 0);
		}
		devname = "ringdev";
		if (devval > -1) {
			globals.ringdev = devval;
			//engage_device(1);
		}
	} else if ((argv[1] && !strcasecmp(argv[0], "play"))) {
		switch_file_handle_t fh = { 0 };
		char *playfile = NULL;
		int samples = 0;
		int seconds = 5;
		int wrote = 0;

		if (globals.call_list) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "ERROR: Cannot use this command this while a call is in progress\n");
			goto done;
		}

		if (!strcasecmp(argv[1], "ringtest")) {
			playfile = globals.ring_file;
		} else {
			playfile = argv[1];
		}

		fh.pre_buffer_datalen = SWITCH_DEFAULT_FILE_BUFFER_LEN;
		if (engage_device(0) == SWITCH_STATUS_SUCCESS) {
			if (switch_core_file_open(&fh,
									  playfile,
									  globals.read_codec.implementation->number_of_channels,
									  globals.read_codec.implementation->actual_samples_per_second,
									  SWITCH_FILE_FLAG_READ | SWITCH_FILE_DATA_SHORT, NULL) == SWITCH_STATUS_SUCCESS) {
				switch_size_t olen = globals.read_codec.implementation->samples_per_packet;
				int16_t abuf[2048];

				if (argv[2]) {
					int i = atoi(argv[2]);
					if (i >= 0) {
						seconds = i;
					}
				}

				samples = globals.read_codec.implementation->actual_samples_per_second * seconds;
				while (switch_core_file_read(&fh, abuf, &olen) == SWITCH_STATUS_SUCCESS) {
					WriteAudioStream(globals.audio_stream, abuf, (long) olen, &globals.read_timer);
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
				deactivate_audio_device();

				seconds = wrote / globals.read_codec.implementation->actual_samples_per_second;
				stream->write_function(stream, "playback test [%s] %d second(s) %d samples @%dkhz",
									   playfile, seconds, wrote, globals.read_codec.implementation->actual_samples_per_second);


			} else {
				stream->write_function(stream, "Cannot play requested file %s\n", argv[1]);
			}
		} else {
			stream->write_function(stream, "Failed to engage audio device\n");
		}
		goto done;
	} else if (!strcasecmp(argv[0], "looptest")) {
		if (globals.call_list) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "ERROR: Cannot use this command this while a call is in progress\n");
			goto done;
		}

		if (engage_device(0) == SWITCH_STATUS_SUCCESS) {
			int samples = 0;
			int success = 0;
			int i;
			for (i = 0; i < 400; i++) {
				if ((samples = ReadAudioStream(globals.audio_stream, globals.read_frame.data,
											   globals.read_codec.implementation->samples_per_packet, &globals.read_timer))) {
					WriteAudioStream(globals.audio_stream, globals.read_frame.data, (long) samples, &globals.write_timer);
					success = 1;
				}
				switch_yield(10000);
			}
			if (!success) {
				stream->write_function(stream, "Failed to read any bytes from indev\n");
			}
			deactivate_audio_device();
		} else {
			stream->write_function(stream, "Failed to engage audio device\n");
		}
		goto done;
	} else if (!strcasecmp(argv[0], "ringfile")) {
		if (argv[1]) {
			if (engage_device(0) == SWITCH_STATUS_SUCCESS) {
				switch_file_handle_t fh = { 0 };
				if (switch_core_file_open(&fh,
										  argv[1],
										  globals.read_codec.implementation->number_of_channels,
										  globals.read_codec.implementation->actual_samples_per_second,
										  SWITCH_FILE_FLAG_READ | SWITCH_FILE_DATA_SHORT, NULL) == SWITCH_STATUS_SUCCESS) {
					switch_core_file_close(&fh);
					set_global_ring_file(argv[1]);
				} else {
					stream->write_function(stream, "Unable to open ring file %s\n", argv[1]);
				}
			} else {
				stream->write_function(stream, "Failed to engage audio device\n");
			}
		} else {
			stream->write_function(stream, "%s", globals.ring_file);
		}
		goto done;
	}

	if (func) {
		if (http) {
			stream->write_function(stream, "<pre>");
		}

		switch_mutex_lock(globals.pa_mutex);
		status = func(&argv[lead], argc - lead, stream);
		switch_mutex_unlock(globals.pa_mutex);

		if (http) {
			stream->write_function(stream, "\n\n</pre>");
		}
	} else {
		if (devname) {
			if (devval > -1) {
				stream->write_function(stream, "%s set to %d\n", devname, devval);
			} else {
				stream->write_function(stream, "%s not set (invalid value)\n", devname);
			}
		} else {
			stream->write_function(stream, "Unknown Command [%s]\n", argv[0]);
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
 * vim:set softtabstop=4 shiftwidth=4 tabstop=4:
 */
