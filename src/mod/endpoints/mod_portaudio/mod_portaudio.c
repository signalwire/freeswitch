/* 
 * FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
 * Copyright (C) 2005/2006, Anthony Minessale II <anthmct@yahoo.com>
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
 * Anthony Minessale II <anthmct@yahoo.com>
 * Portions created by the Initial Developer are Copyright (C)
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 * 
 * Anthony Minessale II <anthmct@yahoo.com>
 *
 *
 * mod_portaudio.c -- PortAudio Endpoint Module
 *
 */
#include <switch.h>

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "pablio.h"
#include <string.h>

#define MY_EVENT_RINGING "portaudio::ringing"

static const char modname[] = "mod_portaudio";

static switch_memory_pool *module_pool;
//static int running = 1;

#define SAMPLE_TYPE  paInt16
//#define SAMPLE_TYPE  paFloat32
typedef short SAMPLE;

typedef enum {
	TFLAG_IO = (1 << 0),
	TFLAG_INBOUND = (1 << 1),
	TFLAG_OUTBOUND = (1 << 2),
	TFLAG_DTMF = (1 << 3),
	TFLAG_VOICE = (1 << 4),
	TFLAG_HANGUP = (1 << 5),
	TFLAG_LINEAR = (1 << 6),
	TFLAG_ANSWER = (1 << 7)
} TFLAGS;

typedef enum {
	GFLAG_MY_CODEC_PREFS = (1 << 0)
} GFLAGS;

static struct {
	int debug;
	int port;
	char *cid_name;
	char *cid_num;
	char *dialplan;
	unsigned int flags;
	int indev;
	int outdev;
	int call_id;
	switch_hash *call_hash;
	switch_mutex_t *device_lock;
	int sample_rate;
} globals;

struct private_object {
	unsigned int flags;
	switch_codec read_codec;
	switch_codec write_codec;
	struct switch_frame read_frame;
	unsigned char databuf[SWITCH_RECCOMMENDED_BUFFER_SIZE];
	switch_core_session *session;
	switch_caller_profile *caller_profile;
	char call_id[50];
	PaError err;
	PABLIO_Stream *audio_in;
	PABLIO_Stream *audio_out;
	int indev;
	int outdev;
};

SWITCH_DECLARE_GLOBAL_STRING_FUNC(set_global_dialplan, globals.dialplan)
	SWITCH_DECLARE_GLOBAL_STRING_FUNC(set_global_cid_name, globals.cid_name)
	SWITCH_DECLARE_GLOBAL_STRING_FUNC(set_global_cid_num, globals.cid_num)

	 static switch_status channel_on_init(switch_core_session *session);
	 static switch_status channel_on_hangup(switch_core_session *session);
	 static switch_status channel_on_ring(switch_core_session *session);
	 static switch_status channel_on_loopback(switch_core_session *session);
	 static switch_status channel_on_transmit(switch_core_session *session);
	 static switch_status channel_outgoing_channel(switch_core_session *session,
												   switch_caller_profile *outbound_profile,
												   switch_core_session **new_session);
	 static switch_status channel_read_frame(switch_core_session *session, switch_frame **frame, int timeout,
											 switch_io_flag flags, int stream_id);
	 static switch_status channel_write_frame(switch_core_session *session, switch_frame *frame, int timeout,
											  switch_io_flag flags, int stream_id);
	 static switch_status channel_kill_channel(switch_core_session *session, int sig);
	 static switch_status engage_device(struct private_object *tech_pvt);
	 static int dump_info(void);
	 static switch_status load_config(void);
	 static int get_dev_by_name(char *name, int in);
	 static switch_status place_call(char *dest, char *out, size_t outlen);
	 static switch_status hup_call(char *callid, char *out, size_t outlen);
	 static switch_status call_info(char *callid, char *out, size_t outlen);
	 static switch_status send_dtmf(char *callid, char *out, size_t outlen);
	 static switch_status answer_call(char *callid, char *out, size_t outlen);

/* 
   State methods they get called when the state changes to the specific state 
   returning SWITCH_STATUS_SUCCESS tells the core to execute the standard state method next
   so if you fully implement the state you can return SWITCH_STATUS_FALSE to skip it.
*/
	 static switch_status channel_on_init(switch_core_session *session)
{
	switch_channel *channel;
	struct private_object *tech_pvt = NULL;

	tech_pvt = switch_core_session_get_private(session);
	assert(tech_pvt != NULL);

	channel = switch_core_session_get_channel(session);
	assert(channel != NULL);

	tech_pvt->read_frame.data = tech_pvt->databuf;
	tech_pvt->read_frame.buflen = sizeof(tech_pvt->databuf);

	switch_set_flag(tech_pvt, TFLAG_IO);

	/* Move Channel's State Machine to RING */
	switch_channel_set_state(channel, CS_RING);

	return SWITCH_STATUS_SUCCESS;
}

static switch_status channel_on_ring(switch_core_session *session)
{
	switch_channel *channel = NULL;
	struct private_object *tech_pvt = NULL;

	channel = switch_core_session_get_channel(session);
	assert(channel != NULL);

	tech_pvt = switch_core_session_get_private(session);
	assert(tech_pvt != NULL);

	switch_console_printf(SWITCH_CHANNEL_CONSOLE, "%s CHANNEL RING\n", switch_channel_get_name(channel));

	return SWITCH_STATUS_SUCCESS;
}

static switch_status channel_on_execute(switch_core_session *session)
{

	switch_channel *channel = NULL;
	struct private_object *tech_pvt = NULL;

	channel = switch_core_session_get_channel(session);
	assert(channel != NULL);

	tech_pvt = switch_core_session_get_private(session);
	assert(tech_pvt != NULL);

	switch_console_printf(SWITCH_CHANNEL_CONSOLE, "%s CHANNEL EXECUTE\n", switch_channel_get_name(channel));


	return SWITCH_STATUS_SUCCESS;
}

static void deactivate_audio_device(struct private_object *tech_pvt)
{
	switch_mutex_lock(globals.device_lock);
	if (tech_pvt->audio_in) {
		CloseAudioStream(tech_pvt->audio_in);
		tech_pvt->audio_in = NULL;
	}
	if (tech_pvt->audio_out) {
		CloseAudioStream(tech_pvt->audio_out);
		tech_pvt->audio_out = NULL;
	}
	switch_mutex_unlock(globals.device_lock);
}

static switch_status channel_on_hangup(switch_core_session *session)
{
	switch_channel *channel = NULL;
	struct private_object *tech_pvt = NULL;

	channel = switch_core_session_get_channel(session);
	assert(channel != NULL);

	tech_pvt = switch_core_session_get_private(session);
	assert(tech_pvt != NULL);

	switch_clear_flag(tech_pvt, TFLAG_IO);
	switch_core_hash_delete(globals.call_hash, tech_pvt->call_id);

	switch_core_codec_destroy(&tech_pvt->read_codec);
	switch_core_codec_destroy(&tech_pvt->write_codec);

	deactivate_audio_device(tech_pvt);

	switch_console_printf(SWITCH_CHANNEL_CONSOLE, "%s CHANNEL HANGUP\n", switch_channel_get_name(channel));

	return SWITCH_STATUS_SUCCESS;
}

static switch_status channel_kill_channel(switch_core_session *session, int sig)
{
	switch_channel *channel = NULL;
	struct private_object *tech_pvt = NULL;

	channel = switch_core_session_get_channel(session);
	assert(channel != NULL);

	tech_pvt = switch_core_session_get_private(session);
	assert(tech_pvt != NULL);

	switch_clear_flag(tech_pvt, TFLAG_IO);
	deactivate_audio_device(tech_pvt);
	switch_channel_hangup(channel);

	switch_console_printf(SWITCH_CHANNEL_CONSOLE, "%s CHANNEL KILL\n", switch_channel_get_name(channel));


	return SWITCH_STATUS_SUCCESS;
}

static switch_status channel_on_loopback(switch_core_session *session)
{
	switch_console_printf(SWITCH_CHANNEL_CONSOLE, "CHANNEL LOOPBACK\n");
	return SWITCH_STATUS_SUCCESS;
}

static switch_status channel_on_transmit(switch_core_session *session)
{
	switch_channel *channel = NULL;
	struct private_object *tech_pvt = NULL;
	switch_time_t last;
	int waitsec = 5 * 1000000;

	channel = switch_core_session_get_channel(session);
	assert(channel != NULL);

	tech_pvt = switch_core_session_get_private(session);
	assert(tech_pvt != NULL);

	last = switch_time_now() - waitsec;

	/* Turn on the device */
	engage_device(tech_pvt);

	while (switch_channel_get_state(channel) == CS_TRANSMIT && !switch_test_flag(tech_pvt, TFLAG_ANSWER)) {
		if (switch_time_now() - last >= waitsec) {
			char buf[512];
			switch_event *event;

			snprintf(buf, sizeof(buf), "BRRRRING! BRRRRING! call %s\n", tech_pvt->call_id);

			if (switch_event_create_subclass(&event, SWITCH_EVENT_CUSTOM, MY_EVENT_RINGING) == SWITCH_STATUS_SUCCESS) {
				switch_event_add_header(event, SWITCH_STACK_BOTTOM, "event_info", buf);
				switch_event_fire(&event);
			}

			switch_console_printf(SWITCH_CHANNEL_CONSOLE, "%s\n", buf);
			last = switch_time_now();
		}
		switch_yield(50000);
	}



	switch_console_printf(SWITCH_CHANNEL_CONSOLE, "CHANNEL TRANSMIT\n");

	return SWITCH_STATUS_SUCCESS;
}


static switch_status channel_waitfor_read(switch_core_session *session, int ms, int stream_id)
{
	struct private_object *tech_pvt = NULL;

	tech_pvt = switch_core_session_get_private(session);
	assert(tech_pvt != NULL);

	return SWITCH_STATUS_SUCCESS;
}

static switch_status channel_waitfor_write(switch_core_session *session, int ms, int stream_id)
{
	struct private_object *tech_pvt = NULL;

	tech_pvt = switch_core_session_get_private(session);
	assert(tech_pvt != NULL);

	return SWITCH_STATUS_SUCCESS;

}

static switch_status channel_send_dtmf(switch_core_session *session, char *dtmf)
{
	struct private_object *tech_pvt = NULL;

	tech_pvt = switch_core_session_get_private(session);
	assert(tech_pvt != NULL);

	switch_console_printf(SWITCH_CHANNEL_CONSOLE, "DTMF ON CALL %s [%s]\n", tech_pvt->call_id, dtmf);

	return SWITCH_STATUS_SUCCESS;
}

static switch_status channel_read_frame(switch_core_session *session, switch_frame **frame, int timeout,
										switch_io_flag flags, int stream_id)
{
	switch_channel *channel = NULL;
	struct private_object *tech_pvt = NULL;
	int samples;
	switch_status status = SWITCH_STATUS_FALSE;

	channel = switch_core_session_get_channel(session);
	assert(channel != NULL);

	tech_pvt = switch_core_session_get_private(session);
	assert(tech_pvt != NULL);

	if (!switch_test_flag(tech_pvt, TFLAG_IO)) {
		return SWITCH_STATUS_FALSE;
	}


	switch_mutex_lock(globals.device_lock);
	if (tech_pvt->audio_in &&
		(samples =
		 ReadAudioStream(tech_pvt->audio_in, tech_pvt->read_frame.data,
						 tech_pvt->read_codec.implementation->samples_per_frame)) != 0) {
		tech_pvt->read_frame.datalen = samples * 2;
		tech_pvt->read_frame.samples = samples;
		*frame = &tech_pvt->read_frame;
		status = SWITCH_STATUS_SUCCESS;
	}
	switch_mutex_unlock(globals.device_lock);

	return status;
}

static switch_status channel_write_frame(switch_core_session *session, switch_frame *frame, int timeout,
										 switch_io_flag flags, int stream_id)
{
	switch_channel *channel = NULL;
	struct private_object *tech_pvt = NULL;
	switch_status status = SWITCH_STATUS_FALSE;
	channel = switch_core_session_get_channel(session);
	assert(channel != NULL);

	tech_pvt = switch_core_session_get_private(session);
	assert(tech_pvt != NULL);

	if (!switch_test_flag(tech_pvt, TFLAG_IO)) {
		return SWITCH_STATUS_FALSE;
	}
	//switch_mutex_lock(globals.device_lock);
	if (tech_pvt->audio_out) {
		WriteAudioStream(tech_pvt->audio_out, (short *) frame->data, (int) (frame->datalen / sizeof(SAMPLE)));
		status = SWITCH_STATUS_SUCCESS;
	}
	//switch_mutex_unlock(globals.device_lock);

	return status;

}

static switch_status channel_answer_channel(switch_core_session *session)
{
	struct private_object *tech_pvt;
	switch_channel *channel = NULL;

	channel = switch_core_session_get_channel(session);
	assert(channel != NULL);

	tech_pvt = switch_core_session_get_private(session);
	assert(tech_pvt != NULL);

	return SWITCH_STATUS_SUCCESS;
}


static struct switch_api_interface send_dtmf_interface = {
	/*.interface_name */ "padtmf",
	/*.desc */ "PortAudio Dial DTMF",
	/*.function */ send_dtmf,
	/*.next */ NULL
};

static struct switch_api_interface answer_call_interface = {
	/*.interface_name */ "paoffhook",
	/*.desc */ "PortAudio Answer Call",
	/*.function */ answer_call,
	/*.next */ &send_dtmf_interface
};

static struct switch_api_interface channel_info_interface = {
	/*.interface_name */ "painfo",
	/*.desc */ "PortAudio Call Info",
	/*.function */ call_info,
	/*.next */ &answer_call_interface
};

static struct switch_api_interface channel_hup_interface = {
	/*.interface_name */ "pahup",
	/*.desc */ "PortAudio Hangup Call",
	/*.function */ hup_call,
	/*.next */ &channel_info_interface
};

static struct switch_api_interface channel_api_interface = {
	/*.interface_name */ "pacall",
	/*.desc */ "PortAudio Call",
	/*.function */ place_call,
	/*.next */ &channel_hup_interface
};

static const switch_state_handler_table channel_event_handlers = {
	/*.on_init */ channel_on_init,
	/*.on_ring */ channel_on_ring,
	/*.on_execute */ channel_on_execute,
	/*.on_hangup */ channel_on_hangup,
	/*.on_loopback */ channel_on_loopback,
	/*.on_transmit */ channel_on_transmit
};

static const switch_io_routines channel_io_routines = {
	/*.outgoing_channel */ channel_outgoing_channel,
	/*.answer_channel */ channel_answer_channel,
	/*.read_frame */ channel_read_frame,
	/*.write_frame */ channel_write_frame,
	/*.kill_channel */ channel_kill_channel,
	/*.waitfor_read */ channel_waitfor_read,
	/*.waitfor_write */ channel_waitfor_write,
	/*.send_dtmf */ channel_send_dtmf
};

static const switch_endpoint_interface channel_endpoint_interface = {
	/*.interface_name */ "portaudio",
	/*.io_routines */ &channel_io_routines,
	/*.event_handlers */ &channel_event_handlers,
	/*.private */ NULL,
	/*.next */ NULL
};

static const switch_loadable_module_interface channel_module_interface = {
	/*.module_name */ modname,
	/*.endpoint_interface */ &channel_endpoint_interface,
	/*.timer_interface */ NULL,
	/*.dialplan_interface */ NULL,
	/*.codec_interface */ NULL,
	/*.application_interface */ NULL,
	/*.api_interface */ &channel_api_interface
};

/* Make sure when you have 2 sessions in the same scope that you pass the appropriate one to the routines
   that allocate memory or you will have 1 channel with memory allocated from another channel's pool!
*/
static switch_status channel_outgoing_channel(switch_core_session *session, switch_caller_profile *outbound_profile,
											  switch_core_session **new_session)
{
	if ((*new_session = switch_core_session_request(&channel_endpoint_interface, NULL)) != 0) {
		struct private_object *tech_pvt;
		switch_channel *channel;
		switch_caller_profile *caller_profile;

		switch_core_session_add_stream(*new_session, NULL);
		if ((tech_pvt =
			 (struct private_object *) switch_core_session_alloc(*new_session, sizeof(struct private_object))) != 0) {
			memset(tech_pvt, 0, sizeof(*tech_pvt));
			channel = switch_core_session_get_channel(*new_session);
			switch_core_session_set_private(*new_session, tech_pvt);
			tech_pvt->session = *new_session;
		} else {
			switch_console_printf(SWITCH_CHANNEL_CONSOLE, "Hey where is my memory pool?\n");
			switch_core_session_destroy(new_session);
			return SWITCH_STATUS_GENERR;
		}

		if (outbound_profile) {
			char name[128];
			snprintf(name, sizeof(name), "PortAudio/%s-%04x",
					 outbound_profile->destination_number ? outbound_profile->destination_number : modname,
					 rand() & 0xffff);
			switch_channel_set_name(channel, name);

			caller_profile = switch_caller_profile_clone(*new_session, outbound_profile);
			switch_channel_set_caller_profile(channel, caller_profile);
			tech_pvt->caller_profile = caller_profile;
		} else {
			switch_console_printf(SWITCH_CHANNEL_CONSOLE, "Doh! no caller profile\n");
			switch_core_session_destroy(new_session);
			return SWITCH_STATUS_GENERR;
		}

		switch_channel_set_flag(channel, CF_OUTBOUND);
		switch_set_flag(tech_pvt, TFLAG_OUTBOUND);
		switch_channel_set_state(channel, CS_INIT);
		return SWITCH_STATUS_SUCCESS;
	}

	return SWITCH_STATUS_GENERR;

}


SWITCH_MOD_DECLARE(switch_status) switch_module_load(const switch_loadable_module_interface **interface, char *filename)
{

	if (switch_core_new_memory_pool(&module_pool) != SWITCH_STATUS_SUCCESS) {
		switch_console_printf(SWITCH_CHANNEL_CONSOLE, "OH OH no pool\n");
		return SWITCH_STATUS_TERM;
	}


	Pa_Initialize();
	load_config();
	switch_core_hash_init(&globals.call_hash, module_pool);
	switch_mutex_init(&globals.device_lock, SWITCH_MUTEX_NESTED, module_pool);

	dump_info();

	if (switch_event_reserve_subclass(MY_EVENT_RINGING) != SWITCH_STATUS_SUCCESS) {
		switch_console_printf(SWITCH_CHANNEL_CONSOLE, "Couldn't register subclass!");
		return SWITCH_STATUS_GENERR;
	}

	/* connect my internal structure to the blank pointer passed to me */
	*interface = &channel_module_interface;

	/* indicate that the module should continue to be loaded */
	return SWITCH_STATUS_SUCCESS;
}


static switch_status load_config(void)
{
	switch_config cfg;
	char *var, *val;
	char *cf = "portaudio.conf";

	memset(&globals, 0, sizeof(globals));

	if (!switch_config_open_file(&cfg, cf)) {
		switch_console_printf(SWITCH_CHANNEL_CONSOLE, "open of %s failed\n", cf);
		return SWITCH_STATUS_TERM;
	}

	while (switch_config_next_pair(&cfg, &var, &val)) {
		if (!strcasecmp(cfg.category, "settings")) {
			if (!strcmp(var, "debug")) {
				globals.debug = atoi(val);
			} else if (!strcmp(var, "sample_rate")) {
				globals.sample_rate = atoi(val);
			} else if (!strcmp(var, "dialplan")) {
				set_global_dialplan(val);
			} else if (!strcmp(var, "cid_name")) {
				set_global_cid_name(val);
			} else if (!strcmp(var, "cid_num")) {
				set_global_cid_num(val);
			} else if (!strcmp(var, "indev")) {
				if (*val == '#') {
					globals.indev = atoi(val + 1);
				} else {
					globals.indev = get_dev_by_name(val, 1);
				}
			} else if (!strcmp(var, "outdev")) {
				if (*val == '#') {
					globals.outdev = atoi(val + 1);
				} else {
					globals.outdev = get_dev_by_name(val, 0);
				}
			}
		}
	}

	if (!globals.dialplan) {
		set_global_dialplan("default");
	}

	if (!globals.sample_rate) {
		globals.sample_rate = 8000;
	}

	switch_config_close_file(&cfg);

	return SWITCH_STATUS_SUCCESS;
}

/*
SWITCH_MOD_DECLARE(switch_status) switch_module_runtime(void)
{

	switch_yield(50000);
	make_call("8888");
	return SWITCH_STATUS_TERM;
}
*/

SWITCH_MOD_DECLARE(switch_status) switch_module_shutdown(void)
{
	Pa_Terminate();

	return SWITCH_STATUS_SUCCESS;
}


static int get_dev_by_name(char *name, int in)
{
	int i;
	int numDevices;
	const PaDeviceInfo *pdi;
	numDevices = Pa_CountDevices();

	if (numDevices < 0) {
		switch_console_printf(SWITCH_CHANNEL_CONSOLE_CLEAN, "ERROR: Pa_CountDevices returned 0x%x\n", numDevices);
		return -2;
	}

	for (i = 0; i < numDevices; i++) {
		pdi = Pa_GetDeviceInfo(i);
		if (strstr(pdi->name, name)) {
			if (in && pdi->maxInputChannels) {
				return i;
			} else if (!in && pdi->maxOutputChannels) {
				return i;
			}
		}
	}
	return -1;
}


static int dump_info(void)
{
	int i, j;
	int numDevices;
	const PaDeviceInfo *pdi;
	PaError err;
	numDevices = Pa_CountDevices();
	if (numDevices < 0) {
		switch_console_printf(SWITCH_CHANNEL_CONSOLE_CLEAN, "ERROR: Pa_CountDevices returned 0x%x\n", numDevices);
		err = numDevices;
		goto error;
	}
	switch_console_printf(SWITCH_CHANNEL_CONSOLE_CLEAN, "Number of devices = %d\n", numDevices);
	for (i = 0; i < numDevices; i++) {
		pdi = Pa_GetDeviceInfo(i);
		switch_console_printf(SWITCH_CHANNEL_CONSOLE_CLEAN, "---------------------------------------------- #%d", i);
		if (i == Pa_GetDefaultInputDeviceID())
			switch_console_printf(SWITCH_CHANNEL_CONSOLE_CLEAN, " DefaultInput");
		if (i == Pa_GetDefaultOutputDeviceID())
			switch_console_printf(SWITCH_CHANNEL_CONSOLE_CLEAN, " DefaultOutput");
		switch_console_printf(SWITCH_CHANNEL_CONSOLE_CLEAN, "\nName         = %s\n", pdi->name);
		switch_console_printf(SWITCH_CHANNEL_CONSOLE_CLEAN, "Max Inputs   = %d", pdi->maxInputChannels);
		switch_console_printf(SWITCH_CHANNEL_CONSOLE_CLEAN, ", Max Outputs = %d\n", pdi->maxOutputChannels);
		if (pdi->numSampleRates == -1) {
			switch_console_printf(SWITCH_CHANNEL_CONSOLE_CLEAN, "Sample Rate Range = %f to %f\n", pdi->sampleRates[0],
								  pdi->sampleRates[1]);
		} else {
			switch_console_printf(SWITCH_CHANNEL_CONSOLE_CLEAN, "Sample Rates =");
			for (j = 0; j < pdi->numSampleRates; j++) {
				switch_console_printf(SWITCH_CHANNEL_CONSOLE_CLEAN, " %8.2f,", pdi->sampleRates[j]);
			}
			switch_console_printf(SWITCH_CHANNEL_CONSOLE_CLEAN, "\n");
		}
		switch_console_printf(SWITCH_CHANNEL_CONSOLE_CLEAN, "Native Sample Formats = ");
		if (pdi->nativeSampleFormats & paInt8)
			switch_console_printf(SWITCH_CHANNEL_CONSOLE_CLEAN, "paInt8, ");
		if (pdi->nativeSampleFormats & paUInt8)
			switch_console_printf(SWITCH_CHANNEL_CONSOLE_CLEAN, "paUInt8, ");
		if (pdi->nativeSampleFormats & paInt16)
			switch_console_printf(SWITCH_CHANNEL_CONSOLE_CLEAN, "paInt16, ");
		if (pdi->nativeSampleFormats & paInt32)
			switch_console_printf(SWITCH_CHANNEL_CONSOLE_CLEAN, "paInt32, ");
		if (pdi->nativeSampleFormats & paFloat32)
			switch_console_printf(SWITCH_CHANNEL_CONSOLE_CLEAN, "paFloat32, ");
		if (pdi->nativeSampleFormats & paInt24)
			switch_console_printf(SWITCH_CHANNEL_CONSOLE_CLEAN, "paInt24, ");
		if (pdi->nativeSampleFormats & paPackedInt24)
			switch_console_printf(SWITCH_CHANNEL_CONSOLE_CLEAN, "paPackedInt24, ");
		switch_console_printf(SWITCH_CHANNEL_CONSOLE_CLEAN, "\n");
	}

	switch_console_printf(SWITCH_CHANNEL_CONSOLE_CLEAN, "----------------------------------------------\n");
	return 0;
  error:
	switch_console_printf(SWITCH_CHANNEL_CONSOLE_CLEAN, "An error occured while using the portaudio stream\n");
	switch_console_printf(SWITCH_CHANNEL_CONSOLE_CLEAN, "Error number: %d\n", err);
	switch_console_printf(SWITCH_CHANNEL_CONSOLE_CLEAN, "Error message: %s\n", Pa_GetErrorText(err));
	return err;
}

static switch_status engage_device(struct private_object *tech_pvt)
{
	int sample_rate = globals.sample_rate;
	int codec_ms = 20;

	switch_channel *channel;
	channel = switch_core_session_get_channel(tech_pvt->session);
	assert(channel != NULL);

	if (switch_core_codec_init(&tech_pvt->read_codec,
							   "L16",
							   sample_rate,
							   codec_ms,
							   1,
							   SWITCH_CODEC_FLAG_ENCODE | SWITCH_CODEC_FLAG_DECODE,
							   NULL, switch_core_session_get_pool(tech_pvt->session)) != SWITCH_STATUS_SUCCESS) {
		switch_console_printf(SWITCH_CHANNEL_CONSOLE, "Can't load codec?\n");
		return SWITCH_STATUS_FALSE;
	} else {
		if (switch_core_codec_init(&tech_pvt->write_codec,
								   "L16",
								   sample_rate,
								   codec_ms,
								   1,
								   SWITCH_CODEC_FLAG_ENCODE | SWITCH_CODEC_FLAG_DECODE,
								   NULL, switch_core_session_get_pool(tech_pvt->session)) != SWITCH_STATUS_SUCCESS) {
			switch_console_printf(SWITCH_CHANNEL_CONSOLE, "Can't load codec?\n");
			switch_core_codec_destroy(&tech_pvt->read_codec);
			return SWITCH_STATUS_FALSE;
		}
	}
	switch_console_printf(SWITCH_CHANNEL_CONSOLE, "Loaded codec L16 %dhz %dms on %s\n", sample_rate, codec_ms,
						  switch_channel_get_name(channel));
	tech_pvt->read_frame.rate = sample_rate;
	tech_pvt->read_frame.codec = &tech_pvt->read_codec;
	switch_core_session_set_read_codec(tech_pvt->session, &tech_pvt->read_codec);
	switch_core_session_set_write_codec(tech_pvt->session, &tech_pvt->write_codec);

	tech_pvt->indev = globals.indev;
	tech_pvt->outdev = globals.outdev;

	switch_mutex_lock(globals.device_lock);
	if ((tech_pvt->err =
		 OpenAudioStream(&tech_pvt->audio_in, sample_rate, SAMPLE_TYPE, PABLIO_READ | PABLIO_MONO, tech_pvt->indev,
						 -1)) == paNoError) {
		if ((tech_pvt->err =
			 OpenAudioStream(&tech_pvt->audio_out, sample_rate, SAMPLE_TYPE, PABLIO_WRITE | PABLIO_MONO, -1,
							 tech_pvt->outdev)) != paNoError) {
			switch_console_printf(SWITCH_CHANNEL_CONSOLE, "Can't open audio out [%d]!\n", tech_pvt->outdev);
			CloseAudioStream(tech_pvt->audio_in);
			tech_pvt->audio_in = NULL;
		}
	} else {
		switch_console_printf(SWITCH_CHANNEL_CONSOLE, "Can't open audio in [%d]!\n", tech_pvt->indev);
	}
	switch_mutex_unlock(globals.device_lock);

	if (tech_pvt->err == paNoError) {
		snprintf(tech_pvt->call_id, sizeof(tech_pvt->call_id), "%d", globals.call_id++);
		switch_core_hash_insert(globals.call_hash, tech_pvt->call_id, tech_pvt);
		return SWITCH_STATUS_SUCCESS;
	} else {
		switch_core_codec_destroy(&tech_pvt->read_codec);
		switch_core_codec_destroy(&tech_pvt->write_codec);
		switch_core_session_destroy(&tech_pvt->session);
	}

	return SWITCH_STATUS_FALSE;
}

static switch_status place_call(char *dest, char *out, size_t outlen)
{
	switch_core_session *session;
	switch_status status = SWITCH_STATUS_FALSE;

	if (!dest) {
		strncpy(out, "Usage: pacall <exten>", outlen - 1);
		return SWITCH_STATUS_FALSE;
	}

	strncpy(out, "FAIL", outlen - 1);

	if ((session = switch_core_session_request(&channel_endpoint_interface, NULL)) != 0) {
		struct private_object *tech_pvt;
		switch_channel *channel;

		switch_core_session_add_stream(session, NULL);
		if ((tech_pvt = (struct private_object *) switch_core_session_alloc(session, sizeof(struct private_object))) != 0) {
			memset(tech_pvt, 0, sizeof(*tech_pvt));
			channel = switch_core_session_get_channel(session);
			switch_core_session_set_private(session, tech_pvt);
			tech_pvt->session = session;
		} else {
			switch_console_printf(SWITCH_CHANNEL_CONSOLE, "Hey where is my memory pool?\n");
			switch_core_session_destroy(&session);
			return SWITCH_STATUS_FALSE;
		}

		if ((tech_pvt->caller_profile = switch_caller_profile_new(session,
																  globals.dialplan,
																  globals.cid_name,
																  globals.cid_num, NULL, NULL, NULL, dest)) != 0) {
			char name[128];
			snprintf(name, sizeof(name), "PortAudio/%s-%04x",
					 tech_pvt->caller_profile->destination_number ? tech_pvt->caller_profile->
					 destination_number : modname, rand() & 0xffff);
			switch_channel_set_name(channel, name);

			switch_channel_set_caller_profile(channel, tech_pvt->caller_profile);
		}
		tech_pvt->session = session;
		if ((status = engage_device(tech_pvt)) == SWITCH_STATUS_SUCCESS) {
			switch_channel_set_state(channel, CS_INIT);
			switch_core_session_thread_launch(tech_pvt->session);
			snprintf(out, outlen, "SUCCESS: %s", tech_pvt->call_id);
		}
	}
	return status;
}


static switch_status hup_call(char *callid, char *out, size_t outlen)
{
	struct private_object *tech_pvt;
	switch_channel *channel = NULL;
	char tmp[50];

	if (callid && !strcasecmp(callid, "last")) {
		snprintf(tmp, sizeof(tmp), "%d", globals.call_id - 1);
		callid = tmp;
	}
	if (!callid || !strcasecmp(callid, "all")) {
		switch_hash_index_t *hi;
		void *val;
		int i = 0;

		for (hi = apr_hash_first(module_pool, globals.call_hash); hi; hi = switch_hash_next(hi)) {
			switch_hash_this(hi, NULL, NULL, &val);
			tech_pvt = val;
			channel = switch_core_session_get_channel(tech_pvt->session);
			assert(channel != NULL);
			switch_channel_hangup(channel);
			i++;
		}

		snprintf(out, outlen, "HUNGUP: %d", i);

		return SWITCH_STATUS_SUCCESS;
	}

	if ((tech_pvt = switch_core_hash_find(globals.call_hash, callid)) != 0) {

		channel = switch_core_session_get_channel(tech_pvt->session);
		assert(channel != NULL);

		switch_channel_hangup(channel);
		strncpy(out, "OK", outlen - 1);
	} else {
		strncpy(out, "NO SUCH CALL", outlen - 1);
	}

	return SWITCH_STATUS_SUCCESS;
}


static switch_status send_dtmf(char *callid, char *out, size_t outlen)
{
	struct private_object *tech_pvt = NULL;
	switch_channel *channel = NULL;
	char *dtmf;

	if ((dtmf = strchr(callid, ' ')) != 0) {
		*dtmf++ = '\0';
	} else {
		dtmf = "";
	}

	if ((tech_pvt = switch_core_hash_find(globals.call_hash, callid)) != 0) {
		channel = switch_core_session_get_channel(tech_pvt->session);
		assert(channel != NULL);
		switch_channel_queue_dtmf(channel, dtmf);
		strncpy(out, "OK", outlen - 1);
	} else {
		strncpy(out, "NO SUCH CALL", outlen - 1);
	}

	return SWITCH_STATUS_SUCCESS;
}

static switch_status answer_call(char *callid, char *out, size_t outlen)
{
	struct private_object *tech_pvt = NULL;
	switch_channel *channel = NULL;

	if ((tech_pvt = switch_core_hash_find(globals.call_hash, callid)) != 0) {
		channel = switch_core_session_get_channel(tech_pvt->session);
		assert(channel != NULL);
		switch_set_flag(tech_pvt, TFLAG_ANSWER);
		switch_channel_answer(channel);
	} else {
		strncpy(out, "NO SUCH CALL", outlen - 1);
	}
	return SWITCH_STATUS_SUCCESS;
}


static void print_info(struct private_object *tech_pvt, char *out, size_t outlen)
{
	switch_channel *channel = NULL;
	channel = switch_core_session_get_channel(tech_pvt->session);
	assert(channel != NULL);

	snprintf(out, outlen, "CALL %s\t%s\t%s\t%s\t%s\n",
			 tech_pvt->call_id,
			 tech_pvt->caller_profile->caller_id_name ? tech_pvt->caller_profile->caller_id_name : "n/a",
			 tech_pvt->caller_profile->caller_id_number ? tech_pvt->caller_profile->caller_id_number : "n/a",
			 tech_pvt->caller_profile->destination_number ? tech_pvt->caller_profile->destination_number : "n/a",
			 switch_channel_get_name(channel));

}

static switch_status call_info(char *callid, char *out, size_t outlen)
{
	struct private_object *tech_pvt;
	switch_hash_index_t *hi;
	void *val;
	if (!callid || !strcasecmp(callid, "all")) {
		for (hi = apr_hash_first(module_pool, globals.call_hash); hi; hi = switch_hash_next(hi)) {
			switch_hash_this(hi, NULL, NULL, &val);
			tech_pvt = val;
			print_info(tech_pvt, out + strlen(out), outlen - strlen(out));
		}
	} else if ((tech_pvt = switch_core_hash_find(globals.call_hash, callid)) != 0) {
		print_info(tech_pvt, out, outlen);
	} else {
		strncpy(out, "NO SUCH CALL", outlen - 1);
	}

	return SWITCH_STATUS_SUCCESS;
}
