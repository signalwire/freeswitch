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
 * mod_bridgecall.c -- Channel Bridge Application Module
 *
 */
#include <switch.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>


static const char modname[] = "mod_bridgecall";

struct audio_bridge_data {
	switch_core_session *session_a;
	switch_core_session *session_b;
	int running;
};

static void *audio_bridge_thread(switch_thread *thread, void *obj)
{
	struct switch_core_thread_session *data = obj;

	switch_channel *chan_a, *chan_b;
	switch_frame *read_frame;
	switch_core_session *session_a, *session_b;
	
	session_a = data->objs[0];
	session_b = data->objs[1];

	chan_a = switch_core_session_get_channel(session_a);
	chan_b = switch_core_session_get_channel(session_b);

	while(data->running > 0) {
		switch_channel_state b_state = switch_channel_get_state(chan_b);

		switch (b_state) {
		case CS_HANGUP:
			data->running = -1;
			continue;
			break;
		default:
			break;
		}

		if (switch_channel_has_dtmf(chan_a)) {
			char dtmf[128];
			switch_channel_dequeue_dtmf(chan_a, dtmf, sizeof(dtmf));
			switch_core_session_send_dtmf(session_b, dtmf);
		}
		if (switch_core_session_read_frame(session_a, &read_frame, -1) == SWITCH_STATUS_SUCCESS && read_frame->datalen) {
			if (switch_core_session_write_frame(session_b, read_frame, -1) != SWITCH_STATUS_SUCCESS) {
				switch_console_printf(SWITCH_CHANNEL_CONSOLE, "write: Bad Frame.... %d Bubye!\n", read_frame->datalen);
				data->running = -1;
			}
		} else {			
			switch_console_printf(SWITCH_CHANNEL_CONSOLE, "read: Bad Frame.... %d Bubye!\n", read_frame->datalen);
			data->running = -1;
		}  
	}

	switch_channel_hangup(chan_b);
	data->running = 0;

	return NULL;
}


static switch_status audio_bridge_on_hangup(switch_core_session *session)
{
	switch_core_session *other_session;
	switch_channel *channel = NULL, *other_channel = NULL;

	channel = switch_core_session_get_channel(session);
	assert(channel != NULL);

	other_session = switch_channel_get_private(channel);
	assert(other_session != NULL);

	other_channel = switch_core_session_get_channel(other_session);
	assert(other_channel != NULL);

	switch_console_printf(SWITCH_CHANNEL_CONSOLE, "CUSTOM HANGUP %s kill %s\n", switch_channel_get_name(channel), switch_channel_get_name(other_channel));

	switch_core_session_kill_channel(other_session, SWITCH_SIG_KILL);
	switch_core_session_kill_channel(session, SWITCH_SIG_KILL);
	
	return SWITCH_STATUS_SUCCESS;
}

static switch_status audio_bridge_on_ring(switch_core_session *session)
{
	switch_channel *channel = NULL;

	channel = switch_core_session_get_channel(session);
	assert(channel != NULL);

	switch_console_printf(SWITCH_CHANNEL_CONSOLE, "CUSTOM RING\n");

	/* put the channel in a passive state so we can loop audio to it */
	if (switch_channel_test_flag(channel, CF_OUTBOUND)) {
		switch_channel_set_state(channel, CS_TRANSMIT);
		return SWITCH_STATUS_FALSE;
	}


	return SWITCH_STATUS_SUCCESS;
}

static const switch_event_handler_table audio_bridge_peer_event_handlers = {
	/*.on_init*/		NULL,
	/*.on_ring*/		audio_bridge_on_ring,
	/*.on_execute*/		NULL,
    /*.on_hangup*/		audio_bridge_on_hangup,
	/*.on_loopback*/	NULL,
	/*.on_transmit*/	NULL
};

static const switch_event_handler_table audio_bridge_caller_event_handlers = {
	/*.on_init*/		NULL,
	/*.on_ring*/		NULL,
	/*.on_execute*/		NULL,
    /*.on_hangup*/		audio_bridge_on_hangup,
	/*.on_loopback*/	NULL,
	/*.on_transmit*/	NULL
};

static void audio_bridge_function(switch_core_session *session, char *data)
{
	switch_channel *caller_channel, *peer_channel;
	switch_core_session *peer_session;
	switch_caller_profile *caller_profile, *caller_caller_profile;
	char chan_type[128]= {'\0'}, *chan_data;
	int timelimit = 60; /* probably a useful option to pass in when there's time */
	caller_channel = switch_core_session_get_channel(session);
	assert(caller_channel != NULL);


	strncpy(chan_type, data, sizeof(chan_type));

	if ((chan_data = strchr(chan_type, '/'))) {
		*chan_data = '\0';
		chan_data++;
	}

	caller_caller_profile = switch_channel_get_caller_profile(caller_channel);
	caller_profile = switch_caller_profile_new(session,
											   caller_caller_profile->dialplan,
											   caller_caller_profile->caller_id_name,
											   caller_caller_profile->caller_id_number,
											   caller_caller_profile->network_addr,
											   NULL,
											   NULL,
											   chan_data);
	

	
	if (switch_core_session_outgoing_channel(session, chan_type, caller_profile, &peer_session) != SWITCH_STATUS_SUCCESS) {
		switch_console_printf(SWITCH_CHANNEL_CONSOLE, "DOH!\n");
		switch_channel_hangup(caller_channel);
		return;
	} else {
		struct switch_core_thread_session this_audio_thread, other_audio_thread;
		time_t start;
		
		peer_channel = switch_core_session_get_channel(peer_session);
		memset(&other_audio_thread, 0, sizeof(other_audio_thread));
		memset(&this_audio_thread, 0, sizeof(this_audio_thread));
		other_audio_thread.objs[0] = session;
		other_audio_thread.objs[1] = peer_session;
		other_audio_thread.running = 5;
		
		this_audio_thread.objs[0] = peer_session;
		this_audio_thread.objs[1] = session;
		this_audio_thread.running = 2;
		

		switch_channel_set_private(caller_channel, peer_session);
		switch_channel_set_private(peer_channel, session);
		switch_channel_set_event_handlers(caller_channel, &audio_bridge_caller_event_handlers);		
		switch_channel_set_event_handlers(peer_channel, &audio_bridge_peer_event_handlers);
		switch_core_session_thread_launch(peer_session);

		for(;;) {
			int state = switch_channel_get_state(peer_channel);
			if (state > CS_RING) {
				break;
			}
			switch_yield(1000);
		}

		time(&start);
		while(switch_channel_get_state(caller_channel) == CS_EXECUTE && 
			  switch_channel_get_state(peer_channel) == CS_TRANSMIT && 
			  !switch_channel_test_flag(peer_channel, CF_ANSWERED) && 
			  ((time(NULL) - start) < timelimit)) {
			switch_yield(20000);
		}

		if (switch_channel_test_flag(peer_channel, CF_ANSWERED)) {
			switch_channel_answer(caller_channel);

			switch_core_session_launch_thread(session, audio_bridge_thread, (void *) &other_audio_thread);
			audio_bridge_thread(NULL, (void *) &this_audio_thread);
			switch_channel_hangup(peer_channel);
			if (other_audio_thread.running > 0) {
				other_audio_thread.running = -1;
				/* wait for the other audio thread */
				while (other_audio_thread.running) {
					switch_yield(1000);
				}
			}


		}
	}

	switch_channel_hangup(caller_channel);
	switch_channel_hangup(peer_channel);
}


static const switch_application_interface bridge_application_interface = {
	/*.interface_name*/			"bridge",
	/*.application_function*/	audio_bridge_function
};


static const switch_loadable_module_interface mod_bridgecall_module_interface = {
	/*.module_name = */			modname,
	/*.endpoint_interface = */	NULL,
	/*.timer_interface = */		NULL,
	/*.dialplan_interface = */	NULL,
	/*.codec_interface = */		NULL,
	/*.application_interface*/	&bridge_application_interface
};

SWITCH_MOD_DECLARE(switch_status) switch_module_load(const switch_loadable_module_interface **interface, char *filename) {
	
	/* connect my internal structure to the blank pointer passed to me */
	*interface = &mod_bridgecall_module_interface;

	/* indicate that the module should continue to be loaded */
	return SWITCH_STATUS_SUCCESS;
}
