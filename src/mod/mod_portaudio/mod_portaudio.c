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

static const char modname[] = "mod_portaudio";

static switch_memory_pool *module_pool;
static int running = 1;


typedef enum {
	TFLAG_IO = (1 << 0),
	TFLAG_INBOUND = (1 << 1),
	TFLAG_OUTBOUND = (1 << 2),
	TFLAG_DTMF = (1 << 3),
	TFLAG_VOICE = (1 << 4),
	TFLAG_HANGUP = (1 << 5),
	TFLAG_LINEAR = (1 << 6)
} TFLAGS;

typedef enum {
	GFLAG_MY_CODEC_PREFS = (1 << 0)
} GFLAGS;

static struct {
	int debug;
	int port;
	char *dialplan;
	char *soundcard;
	unsigned int flags;
} globals;

struct private_object {
	unsigned int flags;
	switch_codec read_codec;
    switch_codec write_codec;
	struct switch_frame read_frame;
	unsigned char databuf[1024];
	switch_core_session *session;
	switch_caller_profile *caller_profile;	
	unsigned int codec;
	unsigned int codecs;
	switch_mutex_t *mutex;
	switch_thread_cond_t *cond;
};

static void set_global_dialplan(char *dialplan)
{
	if (globals.dialplan) {
		free(globals.dialplan);
		globals.dialplan = NULL;
	}
	
	globals.dialplan = strdup(dialplan);
}

static void set_global_soundcard(char *soundcard)
{
	if (globals.soundcard) {
		free(globals.soundcard);
		globals.soundcard = NULL;
	}
	
	globals.soundcard = strdup(soundcard);
}
static const switch_endpoint_interface channel_endpoint_interface;

static switch_status channel_on_init(switch_core_session *session);
static switch_status channel_on_hangup(switch_core_session *session);
static switch_status channel_on_ring(switch_core_session *session);
static switch_status channel_on_loopback(switch_core_session *session);
static switch_status channel_on_transmit(switch_core_session *session);
static switch_status channel_outgoing_channel(switch_core_session *session, switch_caller_profile *outbound_profile, switch_core_session **new_session);
static switch_status channel_read_frame(switch_core_session *session, switch_frame **frame, int timeout, switch_io_flag flags);
static switch_status channel_write_frame(switch_core_session *session, switch_frame *frame, int timeout, switch_io_flag flags);
static switch_status channel_kill_channel(switch_core_session *session, int sig);


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

	switch_set_flag(tech_pvt, TFLAG_IO);
	
	switch_mutex_init(&tech_pvt->mutex, SWITCH_MUTEX_NESTED, switch_core_session_get_pool(session));
	switch_thread_cond_create(&tech_pvt->cond, switch_core_session_get_pool(session));	
	switch_mutex_lock(tech_pvt->mutex);

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

static switch_status channel_on_hangup(switch_core_session *session)
{
	switch_channel *channel = NULL;
	struct private_object *tech_pvt = NULL;
	
	channel = switch_core_session_get_channel(session);
	assert(channel != NULL);

	tech_pvt = switch_core_session_get_private(session);
	assert(tech_pvt != NULL);

	switch_clear_flag(tech_pvt, TFLAG_IO);
	switch_thread_cond_signal(tech_pvt->cond);

	switch_core_codec_destroy(&tech_pvt->read_codec);
	switch_core_codec_destroy(&tech_pvt->write_codec);
	
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
	switch_console_printf(SWITCH_CHANNEL_CONSOLE, "CHANNEL TRANSMIT\n");
	return SWITCH_STATUS_SUCCESS;
}


/* Make sure when you have 2 sessions in the same scope that you pass the appropriate one to the routines
   that allocate memory or you will have 1 channel with memory allocated from another channel's pool!
*/
static switch_status channel_outgoing_channel(switch_core_session *session, switch_caller_profile *outbound_profile, switch_core_session **new_session) 
{
	if ((*new_session = switch_core_session_request(&channel_endpoint_interface, NULL))) {
		struct private_object *tech_pvt;
		switch_channel *channel, *orig_channel;
		switch_caller_profile *caller_profile, *originator_caller_profile = NULL;
		unsigned int req = 0, cap = 0;

		if ((tech_pvt = (struct private_object *) switch_core_session_alloc(*new_session, sizeof(struct private_object)))) {
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
			caller_profile = switch_caller_profile_clone(*new_session, outbound_profile);
			switch_channel_set_caller_profile(channel, caller_profile);
			tech_pvt->caller_profile = caller_profile;
			snprintf(name, sizeof(name), "PortAudio/%s-%04x", caller_profile->destination_number, rand() & 0xffff);
			switch_channel_set_name(channel, name);
		} else {
			switch_console_printf(SWITCH_CHANNEL_CONSOLE, "Doh! no caller profile\n");
			switch_core_session_destroy(new_session);
			return SWITCH_STATUS_GENERR;
		}

		//XXX outbound shit 

		/* (session == NULL) means it was originated from the core not from another channel */
		if (session && (orig_channel = switch_core_session_get_channel(session))) {
			switch_caller_profile *cloned_profile;

			if ((originator_caller_profile = switch_channel_get_caller_profile(orig_channel))) {
				cloned_profile = switch_caller_profile_clone(*new_session, originator_caller_profile);
				switch_channel_set_originator_caller_profile(channel, cloned_profile);
			}
		}
		
		switch_channel_set_flag(channel, CF_OUTBOUND);
		switch_set_flag(tech_pvt, TFLAG_OUTBOUND);
		switch_channel_set_state(channel, CS_INIT);
		return SWITCH_STATUS_SUCCESS;
	}

	return SWITCH_STATUS_GENERR;

}

static switch_status channel_waitfor_read(switch_core_session *session, int ms)
{
	struct private_object *tech_pvt = NULL;

	tech_pvt = switch_core_session_get_private(session);
	assert(tech_pvt != NULL);

	return SWITCH_STATUS_SUCCESS;
}

static switch_status channel_waitfor_write(switch_core_session *session, int ms)
{
	struct private_object *tech_pvt = NULL;

	tech_pvt = switch_core_session_get_private(session);
	assert(tech_pvt != NULL);

	return SWITCH_STATUS_SUCCESS;

}

static switch_status channel_send_dtmf(switch_core_session *session, char *dtmf)
{
	struct private_object *tech_pvt = NULL;
	char *digit;

	tech_pvt = switch_core_session_get_private(session);
	assert(tech_pvt != NULL);
		for(digit = dtmf; *digit; digit++) {
			//XXX Do something...
		}

    return SWITCH_STATUS_SUCCESS;
}

static switch_status channel_read_frame(switch_core_session *session, switch_frame **frame, int timeout, switch_io_flag flags) 
{
	switch_channel *channel = NULL;
	struct private_object *tech_pvt = NULL;

	channel = switch_core_session_get_channel(session);
	assert(channel != NULL);

	tech_pvt = switch_core_session_get_private(session);
	assert(tech_pvt != NULL);

	for(;;) {
		if (switch_test_flag(tech_pvt, TFLAG_IO)) {
			switch_thread_cond_wait(tech_pvt->cond, tech_pvt->mutex);
			if (!switch_test_flag(tech_pvt, TFLAG_IO)) {
				return SWITCH_STATUS_FALSE;			
			}
			while (switch_test_flag(tech_pvt, TFLAG_IO) && !switch_test_flag(tech_pvt, TFLAG_VOICE)) {
				switch_yield(1000);
			}
	
			if (switch_test_flag(tech_pvt, TFLAG_IO)) {
				switch_clear_flag(tech_pvt, TFLAG_VOICE);
				if(!tech_pvt->read_frame.datalen) {
					continue;
				}
						
				*frame = &tech_pvt->read_frame;
				return SWITCH_STATUS_SUCCESS;
			}
		}
		break;
	}
	return SWITCH_STATUS_FALSE;
}

static switch_status channel_write_frame(switch_core_session *session, switch_frame *frame, int timeout, switch_io_flag flags)
{
	switch_channel *channel = NULL;
	struct private_object *tech_pvt = NULL;
	//switch_frame *pframe;

	channel = switch_core_session_get_channel(session);
	assert(channel != NULL);

	tech_pvt = switch_core_session_get_private(session);
	assert(tech_pvt != NULL);

	if(!switch_test_flag(tech_pvt, TFLAG_IO)) {
		return SWITCH_STATUS_FALSE;
	}
/*
	if (switch_test_flag(tech_pvt, TFLAG_LINEAR)) {
		switch_swap_linear(frame->data, (int)frame->datalen / 2);
	}
*/

	//XXX send voice

	return SWITCH_STATUS_SUCCESS;
	
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

static const switch_event_handler_table channel_event_handlers = {
	/*.on_init*/			channel_on_init,
	/*.on_ring*/			channel_on_ring,
	/*.on_execute*/			channel_on_execute,
	/*.on_hangup*/			channel_on_hangup,
	/*.on_loopback*/		channel_on_loopback,
	/*.on_transmit*/		channel_on_transmit
};

static const switch_io_routines channel_io_routines = {
	/*.outgoing_channel*/	channel_outgoing_channel,
	/*.answer_channel*/		channel_answer_channel,
	/*.read_frame*/			channel_read_frame,
	/*.write_frame*/		channel_write_frame,
	/*.kill_channel*/		channel_kill_channel,
	/*.waitfor_read*/		channel_waitfor_read,
	/*.waitfor_write*/		channel_waitfor_write,
	/*.send_dtmf*/			channel_send_dtmf
};

static const switch_endpoint_interface channel_endpoint_interface = {
	/*.interface_name*/		"portaudio",
	/*.io_routines*/		&channel_io_routines,
	/*.event_handlers*/		&channel_event_handlers,
	/*.private*/			NULL,
	/*.next*/				NULL
};

static const switch_loadable_module_interface channel_module_interface = {
	/*.module_name*/			modname,
	/*.endpoint_interface*/		&channel_endpoint_interface,
	/*.timer_interface*/		NULL,
	/*.dialplan_interface*/		NULL,
	/*.codec_interface*/		NULL,
	/*.application_interface*/	NULL
};


SWITCH_MOD_DECLARE(switch_status) switch_module_load(const switch_loadable_module_interface **interface, char *filename) {

	if (switch_core_new_memory_pool(&module_pool) != SWITCH_STATUS_SUCCESS) {
		switch_console_printf(SWITCH_CHANNEL_CONSOLE, "OH OH no pool\n");
		return SWITCH_STATUS_TERM;
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
			} else if (!strcmp(var, "dialplan")) {
				set_global_dialplan(val);
			} else if (!strcmp(var, "soundcard")) {
				set_global_soundcard(val);
			}
		}
	}

	if (!globals.dialplan) {
		set_global_dialplan("default");
	}

	switch_config_close_file(&cfg);

	return SWITCH_STATUS_SUCCESS;
}

/*
SWITCH_MOD_DECLARE(switch_status) switch_module_runtime(void)
{
	int res;
	int netfd;
	int refresh;
	struct private_object *tech_pvt = NULL;
	
	load_config();

	running = 0;

	return SWITCH_STATUS_TERM;
}


SWITCH_MOD_DECLARE(switch_status) switch_module_shutdown(void)
{
	int x = 0;

	running = -1;

	while (running) {
		if (x++ > 1000) {
			break;
		}
		switch_yield(20000);
	}
	return SWITCH_STATUS_SUCCESS;
}

*/