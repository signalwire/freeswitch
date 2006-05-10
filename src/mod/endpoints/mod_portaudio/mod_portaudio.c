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

static switch_memory_pool_t *module_pool = NULL;
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
	switch_hash_t *call_hash;
	switch_mutex_t *device_lock;
	int sample_rate;
} globals;

struct private_object {
	unsigned int flags;
	switch_codec_t read_codec;
	switch_codec_t write_codec;
	switch_frame_t read_frame;
	unsigned char databuf[SWITCH_RECCOMMENDED_BUFFER_SIZE];
	switch_core_session_t *session;
	switch_caller_profile_t *caller_profile;
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

	 static switch_status_t channel_on_init(switch_core_session_t *session);
	 static switch_status_t channel_on_hangup(switch_core_session_t *session);
	 static switch_status_t channel_on_ring(switch_core_session_t *session);
	 static switch_status_t channel_on_loopback(switch_core_session_t *session);
	 static switch_status_t channel_on_transmit(switch_core_session_t *session);
	 static switch_status_t channel_outgoing_channel(switch_core_session_t *session,
												   switch_caller_profile_t *outbound_profile,
												   switch_core_session_t **new_session, switch_memory_pool_t *pool);
	 static switch_status_t channel_read_frame(switch_core_session_t *session, switch_frame_t **frame, int timeout,
											 switch_io_flag_t flags, int stream_id);
	 static switch_status_t channel_write_frame(switch_core_session_t *session, switch_frame_t *frame, int timeout,
											  switch_io_flag_t flags, int stream_id);
	 static switch_status_t channel_kill_channel(switch_core_session_t *session, int sig);
	 static switch_status_t engage_device(struct private_object *tech_pvt);
	 static int dump_info(void);
	 static switch_status_t load_config(void);
	 static int get_dev_by_name(char *name, int in);
	 static switch_status_t place_call(char *dest, char *out, size_t outlen);
	 static switch_status_t hup_call(char *callid, char *out, size_t outlen);
	 static switch_status_t call_info(char *callid, char *out, size_t outlen);
	 static switch_status_t send_dtmf(char *callid, char *out, size_t outlen);
	 static switch_status_t answer_call(char *callid, char *out, size_t outlen);

/* 
   State methods they get called when the state changes to the specific state 
   returning SWITCH_STATUS_SUCCESS tells the core to execute the standard state method next
   so if you fully implement the state you can return SWITCH_STATUS_FALSE to skip it.
*/

static switch_status_t channel_on_init(switch_core_session_t *session)
{
	switch_channel_t *channel;
	struct private_object *tech_pvt = NULL;
	switch_time_t last;
	int waitsec = 5 * 1000000;

	tech_pvt = switch_core_session_get_private(session);
	assert(tech_pvt != NULL);

	channel = switch_core_session_get_channel(session);
	assert(channel != NULL);

	tech_pvt->read_frame.data = tech_pvt->databuf;
	tech_pvt->read_frame.buflen = sizeof(tech_pvt->databuf);

	last = switch_time_now() - waitsec;

	if (switch_test_flag(tech_pvt, TFLAG_OUTBOUND)) {
		/* Turn on the device */
		engage_device(tech_pvt);


		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "%s CHANNEL INIT %d %d\n", switch_channel_get_name(channel),
			switch_channel_get_state(channel), switch_test_flag(tech_pvt, TFLAG_ANSWER));

		while (switch_channel_get_state(channel) == CS_INIT && !switch_test_flag(tech_pvt, TFLAG_ANSWER)) {
			if (switch_time_now() - last >= waitsec) {
				char buf[512];
				switch_event_t *event;
				 
				snprintf(buf, sizeof(buf), "BRRRRING! BRRRRING! call %s\n", tech_pvt->call_id);

				if (switch_event_create_subclass(&event, SWITCH_EVENT_CUSTOM, MY_EVENT_RINGING) == SWITCH_STATUS_SUCCESS) {
					switch_event_add_header(event, SWITCH_STACK_BOTTOM, "event_info", buf);
					switch_channel_event_set_data(channel, event);
					switch_event_fire(&event);
				}

				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "%s\n", buf);
				last = switch_time_now();
			}
			switch_yield(50000);
		}
	}


	if (switch_test_flag(tech_pvt, TFLAG_OUTBOUND) && !switch_test_flag(tech_pvt, TFLAG_ANSWER)) {
		switch_channel_hangup(channel, SWITCH_CAUSE_NO_ANSWER);
	} else {
		switch_set_flag(tech_pvt, TFLAG_IO);

		/* Move Channel's State Machine to RING */
		switch_channel_set_state(channel, CS_RING);
	}

	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t channel_on_ring(switch_core_session_t *session)
{
	switch_channel_t *channel = NULL;
	struct private_object *tech_pvt = NULL;

	channel = switch_core_session_get_channel(session);
	assert(channel != NULL);

	tech_pvt = switch_core_session_get_private(session);
	assert(tech_pvt != NULL);

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "%s CHANNEL RING\n", switch_channel_get_name(channel));

	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t channel_on_execute(switch_core_session_t *session)
{

	switch_channel_t *channel = NULL;
	struct private_object *tech_pvt = NULL;

	channel = switch_core_session_get_channel(session);
	assert(channel != NULL);

	tech_pvt = switch_core_session_get_private(session);
	assert(tech_pvt != NULL);

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "%s CHANNEL EXECUTE\n", switch_channel_get_name(channel));


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

static switch_status_t channel_on_hangup(switch_core_session_t *session)
{
	switch_channel_t *channel = NULL;
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

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "%s CHANNEL HANGUP\n", switch_channel_get_name(channel));

	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t channel_kill_channel(switch_core_session_t *session, int sig)
{
	switch_channel_t *channel = NULL;
	struct private_object *tech_pvt = NULL;

	channel = switch_core_session_get_channel(session);
	assert(channel != NULL);

	tech_pvt = switch_core_session_get_private(session);
	assert(tech_pvt != NULL);

	switch_clear_flag(tech_pvt, TFLAG_IO);
	deactivate_audio_device(tech_pvt);
	switch_channel_hangup(channel, SWITCH_CAUSE_NORMAL_CLEARING);

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "%s CHANNEL KILL\n", switch_channel_get_name(channel));


	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t channel_on_transmit(switch_core_session_t *session)
{
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "CHANNEL TRANSMIT\n");
	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t channel_on_loopback(switch_core_session_t *session)
{
	switch_channel_t *channel = NULL;
	struct private_object *tech_pvt = NULL;

	channel = switch_core_session_get_channel(session);
	assert(channel != NULL);

	tech_pvt = switch_core_session_get_private(session);
	assert(tech_pvt != NULL);

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "CHANNEL LOOPBACK\n");

	return SWITCH_STATUS_SUCCESS;
}


static switch_status_t channel_waitfor_read(switch_core_session_t *session, int ms, int stream_id)
{
	struct private_object *tech_pvt = NULL;

	tech_pvt = switch_core_session_get_private(session);
	assert(tech_pvt != NULL);

	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t channel_waitfor_write(switch_core_session_t *session, int ms, int stream_id)
{
	struct private_object *tech_pvt = NULL;

	tech_pvt = switch_core_session_get_private(session);
	assert(tech_pvt != NULL);

	return SWITCH_STATUS_SUCCESS;

}

static switch_status_t channel_send_dtmf(switch_core_session_t *session, char *dtmf)
{
	struct private_object *tech_pvt = NULL;

	tech_pvt = switch_core_session_get_private(session);
	assert(tech_pvt != NULL);

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "DTMF ON CALL %s [%s]\n", tech_pvt->call_id, dtmf);

	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t channel_read_frame(switch_core_session_t *session, switch_frame_t **frame, int timeout,
										switch_io_flag_t flags, int stream_id)
{
	switch_channel_t *channel = NULL;
	struct private_object *tech_pvt = NULL;
	int samples;
	switch_status_t status = SWITCH_STATUS_FALSE;

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

static switch_status_t channel_write_frame(switch_core_session_t *session, switch_frame_t *frame, int timeout,
										 switch_io_flag_t flags, int stream_id)
{
	switch_channel_t *channel = NULL;
	struct private_object *tech_pvt = NULL;
	switch_status_t status = SWITCH_STATUS_FALSE;
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

static switch_status_t channel_answer_channel(switch_core_session_t *session)
{
	struct private_object *tech_pvt;
	switch_channel_t *channel = NULL;

	channel = switch_core_session_get_channel(session);
	assert(channel != NULL);

	tech_pvt = switch_core_session_get_private(session);
	assert(tech_pvt != NULL);

	return SWITCH_STATUS_SUCCESS;
}


static switch_api_interface_t send_dtmf_interface = {
	/*.interface_name */ "padtmf",
	/*.desc */ "PortAudio Dial DTMF",
	/*.function */ send_dtmf,
	/*.next */ NULL
};

static switch_api_interface_t answer_call_interface = {
	/*.interface_name */ "paoffhook",
	/*.desc */ "PortAudio Answer Call",
	/*.function */ answer_call,
	/*.next */ &send_dtmf_interface
};

static switch_api_interface_t channel_info_interface = {
	/*.interface_name */ "painfo",
	/*.desc */ "PortAudio Call Info",
	/*.function */ call_info,
	/*.next */ &answer_call_interface
};

static switch_api_interface_t channel_hup_interface = {
	/*.interface_name */ "pahup",
	/*.desc */ "PortAudio Hangup Call",
	/*.function */ hup_call,
	/*.next */ &channel_info_interface
};

static switch_api_interface_t channel_api_interface = {
	/*.interface_name */ "pacall",
	/*.desc */ "PortAudio Call",
	/*.function */ place_call,
	/*.next */ &channel_hup_interface
};

static const switch_state_handler_table_t channel_event_handlers = {
	/*.on_init */ channel_on_init,
	/*.on_ring */ channel_on_ring,
	/*.on_execute */ channel_on_execute,
	/*.on_hangup */ channel_on_hangup,
	/*.on_loopback */ channel_on_loopback,
	/*.on_transmit */ channel_on_transmit
};

static const switch_io_routines_t channel_io_routines = {
	/*.outgoing_channel */ channel_outgoing_channel,
	/*.answer_channel */ channel_answer_channel,
	/*.read_frame */ channel_read_frame,
	/*.write_frame */ channel_write_frame,
	/*.kill_channel */ channel_kill_channel,
	/*.waitfor_read */ channel_waitfor_read,
	/*.waitfor_write */ channel_waitfor_write,
	/*.send_dtmf */ channel_send_dtmf
};

static const switch_endpoint_interface_t channel_endpoint_interface = {
	/*.interface_name */ "portaudio",
	/*.io_routines */ &channel_io_routines,
	/*.event_handlers */ &channel_event_handlers,
	/*.private */ NULL,
	/*.next */ NULL
};

static const switch_loadable_module_interface_t channel_module_interface = {
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
static switch_status_t channel_outgoing_channel(switch_core_session_t *session, switch_caller_profile_t *outbound_profile,
											  switch_core_session_t **new_session, switch_memory_pool_t *pool)
{

	if ((*new_session = switch_core_session_request(&channel_endpoint_interface, pool)) != 0) {
		struct private_object *tech_pvt;
		switch_channel_t *channel;
		switch_caller_profile_t *caller_profile;

		switch_core_session_add_stream(*new_session, NULL);
		if ((tech_pvt =
			 (struct private_object *) switch_core_session_alloc(*new_session, sizeof(struct private_object))) != 0) {
			memset(tech_pvt, 0, sizeof(*tech_pvt));
			channel = switch_core_session_get_channel(*new_session);
			switch_core_session_set_private(*new_session, tech_pvt);
			tech_pvt->session = *new_session;
		} else {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Hey where is my memory pool?\n");
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
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Doh! no caller profile\n");
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


SWITCH_MOD_DECLARE(switch_status_t) switch_module_load(const switch_loadable_module_interface_t **module_interface, char *filename)
{

	if (switch_core_new_memory_pool(&module_pool) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "OH OH no pool\n");
		return SWITCH_STATUS_TERM;
	}


	Pa_Initialize();
	load_config();
	switch_core_hash_init(&globals.call_hash, module_pool);
	switch_mutex_init(&globals.device_lock, SWITCH_MUTEX_NESTED, module_pool);

	dump_info();

	if (switch_event_reserve_subclass(MY_EVENT_RINGING) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Couldn't register subclass!");
		return SWITCH_STATUS_GENERR;
	}

	/* connect my internal structure to the blank pointer passed to me */
	*module_interface = &channel_module_interface;

	/* indicate that the module should continue to be loaded */
	return SWITCH_STATUS_SUCCESS;
}


static switch_status_t load_config(void)
{
	char *cf = "portaudio.conf";
	switch_xml_t cfg, xml, settings, param;

	memset(&globals, 0, sizeof(globals));

	if (!(xml = switch_xml_open_cfg(cf, &cfg))) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "open of %s failed\n", cf);
		return SWITCH_STATUS_TERM;
	}

	if ((settings = switch_xml_child(cfg, "settings"))) {
		for (param = switch_xml_child(settings, "param"); param; param = param->next) {
			char *var = (char *) switch_xml_attr(param, "name");
			char *val = (char *) switch_xml_attr(param, "value");

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

	switch_xml_free(xml);

	return SWITCH_STATUS_SUCCESS;
}

/*
SWITCH_MOD_DECLARE(switch_status_t) switch_module_runtime(void)
{

	switch_yield(50000);
	make_call("8888");
	return SWITCH_STATUS_TERM;
}
*/

SWITCH_MOD_DECLARE(switch_status_t) switch_module_shutdown(void)
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
		switch_log_printf(SWITCH_CHANNEL_LOG_CLEAN, SWITCH_LOG_ERROR, "ERROR: Pa_CountDevices returned 0x%x\n", numDevices);
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
		switch_log_printf(SWITCH_CHANNEL_LOG_CLEAN, SWITCH_LOG_ERROR, "ERROR: Pa_CountDevices returned 0x%x\n", numDevices);
		err = numDevices;
		goto error;
	}
	switch_log_printf(SWITCH_CHANNEL_LOG_CLEAN, SWITCH_LOG_INFO, "Number of devices = %d\n", numDevices);
	for (i = 0; i < numDevices; i++) {
		pdi = Pa_GetDeviceInfo(i);
		switch_log_printf(SWITCH_CHANNEL_LOG_CLEAN, SWITCH_LOG_INFO, "---------------------------------------------- #%d", i);
		if (i == Pa_GetDefaultInputDeviceID())
			switch_log_printf(SWITCH_CHANNEL_LOG_CLEAN, SWITCH_LOG_INFO, " DefaultInput");
		if (i == Pa_GetDefaultOutputDeviceID())
			switch_log_printf(SWITCH_CHANNEL_LOG_CLEAN, SWITCH_LOG_INFO, " DefaultOutput");
		switch_log_printf(SWITCH_CHANNEL_LOG_CLEAN, SWITCH_LOG_INFO, "\nName         = %s\n", pdi->name);
		switch_log_printf(SWITCH_CHANNEL_LOG_CLEAN, SWITCH_LOG_INFO, "Max Inputs   = %d", pdi->maxInputChannels);
		switch_log_printf(SWITCH_CHANNEL_LOG_CLEAN, SWITCH_LOG_INFO, ", Max Outputs = %d\n", pdi->maxOutputChannels);
		if (pdi->numSampleRates == -1) {
			switch_log_printf(SWITCH_CHANNEL_LOG_CLEAN, SWITCH_LOG_INFO, "Sample Rate Range = %f to %f\n", pdi->sampleRates[0],
								  pdi->sampleRates[1]);
		} else {
			switch_log_printf(SWITCH_CHANNEL_LOG_CLEAN, SWITCH_LOG_INFO, "Sample Rates =");
			for (j = 0; j < pdi->numSampleRates; j++) {
				switch_log_printf(SWITCH_CHANNEL_LOG_CLEAN, SWITCH_LOG_INFO, " %8.2f,", pdi->sampleRates[j]);
			}
			switch_log_printf(SWITCH_CHANNEL_LOG_CLEAN, SWITCH_LOG_INFO, "\n");
		}
		switch_log_printf(SWITCH_CHANNEL_LOG_CLEAN, SWITCH_LOG_INFO, "Native Sample Formats = ");
		if (pdi->nativeSampleFormats & paInt8)
			switch_log_printf(SWITCH_CHANNEL_LOG_CLEAN, SWITCH_LOG_INFO, "paInt8, ");
		if (pdi->nativeSampleFormats & paUInt8)
			switch_log_printf(SWITCH_CHANNEL_LOG_CLEAN, SWITCH_LOG_INFO, "paUInt8, ");
		if (pdi->nativeSampleFormats & paInt16)
			switch_log_printf(SWITCH_CHANNEL_LOG_CLEAN, SWITCH_LOG_INFO, "paInt16, ");
		if (pdi->nativeSampleFormats & paInt32)
			switch_log_printf(SWITCH_CHANNEL_LOG_CLEAN, SWITCH_LOG_INFO, "paInt32, ");
		if (pdi->nativeSampleFormats & paFloat32)
			switch_log_printf(SWITCH_CHANNEL_LOG_CLEAN, SWITCH_LOG_INFO, "paFloat32, ");
		if (pdi->nativeSampleFormats & paInt24)
			switch_log_printf(SWITCH_CHANNEL_LOG_CLEAN, SWITCH_LOG_INFO, "paInt24, ");
		if (pdi->nativeSampleFormats & paPackedInt24)
			switch_log_printf(SWITCH_CHANNEL_LOG_CLEAN, SWITCH_LOG_INFO, "paPackedInt24, ");
		switch_log_printf(SWITCH_CHANNEL_LOG_CLEAN, SWITCH_LOG_INFO, "\n");
	}

	switch_log_printf(SWITCH_CHANNEL_LOG_CLEAN, SWITCH_LOG_INFO, "----------------------------------------------\n");
	return 0;
  error:
	switch_log_printf(SWITCH_CHANNEL_LOG_CLEAN, SWITCH_LOG_ERROR, "An error occured while using the portaudio stream\n");
	switch_log_printf(SWITCH_CHANNEL_LOG_CLEAN, SWITCH_LOG_ERROR, "Error number: %d\n", err);
	switch_log_printf(SWITCH_CHANNEL_LOG_CLEAN, SWITCH_LOG_ERROR, "Error message: %s\n", Pa_GetErrorText(err));
	return err;
}

static switch_status_t engage_device(struct private_object *tech_pvt)
{
	int sample_rate = globals.sample_rate;
	int codec_ms = 20;

	switch_channel_t *channel;
	channel = switch_core_session_get_channel(tech_pvt->session);
	assert(channel != NULL);

	if (switch_core_codec_init(&tech_pvt->read_codec,
							   "L16",
							   sample_rate,
							   codec_ms,
							   1,
							   SWITCH_CODEC_FLAG_ENCODE | SWITCH_CODEC_FLAG_DECODE,
							   NULL, switch_core_session_get_pool(tech_pvt->session)) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Can't load codec?\n");
		return SWITCH_STATUS_FALSE;
	} else {
		if (switch_core_codec_init(&tech_pvt->write_codec,
								   "L16",
								   sample_rate,
								   codec_ms,
								   1,
								   SWITCH_CODEC_FLAG_ENCODE | SWITCH_CODEC_FLAG_DECODE,
								   NULL, switch_core_session_get_pool(tech_pvt->session)) != SWITCH_STATUS_SUCCESS) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Can't load codec?\n");
			switch_core_codec_destroy(&tech_pvt->read_codec);
			return SWITCH_STATUS_FALSE;
		}
	}
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "Loaded codec L16 %dhz %dms on %s\n", sample_rate, codec_ms,
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
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Can't open audio out [%d]!\n", tech_pvt->outdev);
			CloseAudioStream(tech_pvt->audio_in);
			tech_pvt->audio_in = NULL;
		}
	} else {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Can't open audio in [%d]!\n", tech_pvt->indev);
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

static switch_status_t place_call(char *dest, char *out, size_t outlen)
{
	switch_core_session_t *session;
	switch_status_t status = SWITCH_STATUS_FALSE;

	if (!dest) {
		strncpy(out, "Usage: pacall <exten>", outlen - 1);
		return SWITCH_STATUS_FALSE;
	}

	strncpy(out, "FAIL", outlen - 1);

	if ((session = switch_core_session_request(&channel_endpoint_interface, NULL)) != 0) {
		struct private_object *tech_pvt;
		switch_channel_t *channel;

		switch_core_session_add_stream(session, NULL);
		if ((tech_pvt = (struct private_object *) switch_core_session_alloc(session, sizeof(struct private_object))) != 0) {
			memset(tech_pvt, 0, sizeof(*tech_pvt));
			channel = switch_core_session_get_channel(session);
			switch_core_session_set_private(session, tech_pvt);
			tech_pvt->session = session;
		} else {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Hey where is my memory pool?\n");
			switch_core_session_destroy(&session);
			return SWITCH_STATUS_FALSE;
		}

		if ((tech_pvt->caller_profile = switch_caller_profile_new(switch_core_session_get_pool(session),
																  globals.dialplan,
																  globals.cid_name,
																  globals.cid_num, NULL, NULL, NULL, NULL, (char *)modname, NULL, dest)) != 0) {
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
			snprintf(out, outlen, "SUCCESS:%s:%s", tech_pvt->call_id, switch_core_session_get_uuid(tech_pvt->session));
		}
	}
	return status;
}


static switch_status_t hup_call(char *callid, char *out, size_t outlen)
{
	struct private_object *tech_pvt;
	switch_channel_t *channel = NULL;
	char tmp[50];

	if (callid && !strcasecmp(callid, "last")) {
		snprintf(tmp, sizeof(tmp), "%d", globals.call_id - 1);
		callid = tmp;
	}
	if (switch_strlen_zero(callid) || !strcasecmp(callid, "all")) {
		switch_hash_index_t *hi;
		void *val;
		int i = 0;

		for (hi = apr_hash_first(module_pool, globals.call_hash); hi; hi = switch_hash_next(hi)) {
			switch_hash_this(hi, NULL, NULL, &val);
			tech_pvt = val;
			channel = switch_core_session_get_channel(tech_pvt->session);
			assert(channel != NULL);
			switch_channel_hangup(channel, SWITCH_CAUSE_NORMAL_CLEARING);
			i++;
		}

		snprintf(out, outlen, "HUNGUP: %d", i);

		return SWITCH_STATUS_SUCCESS;
	}

	if ((tech_pvt = switch_core_hash_find(globals.call_hash, callid)) != 0) {

		channel = switch_core_session_get_channel(tech_pvt->session);
		assert(channel != NULL);

		switch_channel_hangup(channel, SWITCH_CAUSE_NORMAL_CLEARING);
		strncpy(out, "OK", outlen - 1);
	} else {
		strncpy(out, "NO SUCH CALL", outlen - 1);
	}

	return SWITCH_STATUS_SUCCESS;
}


static switch_status_t send_dtmf(char *callid, char *out, size_t outlen)
{
	struct private_object *tech_pvt = NULL;
	switch_channel_t *channel = NULL;
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

static switch_status_t answer_call(char *callid, char *out, size_t outlen)
{
	struct private_object *tech_pvt = NULL;
	switch_channel_t *channel = NULL;

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
	switch_channel_t *channel = NULL;
	channel = switch_core_session_get_channel(tech_pvt->session);
	assert(channel != NULL);

	snprintf(out, outlen, "CALL %s\t%s\t%s\t%s\t%s\n",
			 tech_pvt->call_id,
			 tech_pvt->caller_profile->caller_id_name ? tech_pvt->caller_profile->caller_id_name : "n/a",
			 tech_pvt->caller_profile->caller_id_number ? tech_pvt->caller_profile->caller_id_number : "n/a",
			 tech_pvt->caller_profile->destination_number ? tech_pvt->caller_profile->destination_number : "n/a",
			 switch_channel_get_name(channel));

}

static switch_status_t call_info(char *callid, char *out, size_t outlen)
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
	} else if (callid && (tech_pvt = switch_core_hash_find(globals.call_hash, callid)) != 0) {
		print_info(tech_pvt, out, outlen);
	} else {
		strncpy(out, "NO SUCH CALL", outlen - 1);
	}

	return SWITCH_STATUS_SUCCESS;
}
