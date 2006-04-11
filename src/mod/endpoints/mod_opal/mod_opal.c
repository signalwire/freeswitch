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
 * mod_opalchan.c -- OPAL2 Endpoint Module
 *
 */
#include <switch.h>


static const char modname[] = "mod_woomera";

static switch_memory_pool *module_pool = NULL;


typedef enum {
	TFLAG_IO = (1 << 0),
	TFLAG_INBOUND = (1 << 1),
	TFLAG_OUTBOUND = (1 << 2),
	TFLAG_DTMF = (1 << 3),
} TFLAGS;

static struct {
	int debug;
	int port;
} globals;

struct private_object {
	unsigned int flags;
	struct switch_frame frame;
	unsigned char databuf[SWITCH_RECCOMMENDED_BUFFER_SIZE];
	switch_core_session *session;
	switch_caller_profile *caller_profile;
};


static const switch_endpoint_interface channel_endpoint_interface;

static switch_status channel_on_init(switch_core_session *session);
static switch_status channel_on_hangup(switch_core_session *session);
static switch_status channel_on_ring(switch_core_session *session);
static switch_status channel_on_loopback(switch_core_session *session);
static switch_status channel_on_transmit(switch_core_session *session);
static switch_status channel_outgoing_channel(switch_core_session *session, switch_caller_profile *outbound_profile,
											  switch_core_session **new_session);
static switch_status channel_read_frame(switch_core_session *session, switch_frame **frame, int timeout,
										switch_io_flag flags, int stream_id);
static switch_status channel_write_frame(switch_core_session *session, switch_frame *frame, int timeout,
										 switch_io_flag flags, int stream_id);
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

	tech_pvt->frame.data = tech_pvt->databuf;

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

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "%s CHANNEL RING\n", switch_channel_get_name(channel));

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

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "%s CHANNEL EXECUTE\n", switch_channel_get_name(channel));


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


	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "%s CHANNEL HANGUP\n", switch_channel_get_name(channel));

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
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "%s CHANNEL KILL\n", switch_channel_get_name(channel));


	return SWITCH_STATUS_SUCCESS;
}

static switch_status channel_on_loopback(switch_core_session *session)
{
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "CHANNEL LOOPBACK\n");
	return SWITCH_STATUS_SUCCESS;
}

static switch_status channel_on_transmit(switch_core_session *session)
{
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "CHANNEL TRANSMIT\n");
	return SWITCH_STATUS_SUCCESS;
}


/* Make sure when you have 2 sessions in the same scope that you pass the appropriate one to the routines
   that allocate memory or you will have 1 channel with memory allocated from another channel's pool!
*/
static switch_status channel_outgoing_channel(switch_core_session *session, switch_caller_profile *outbound_profile,
											  switch_core_session **new_session)
{
	if ((*new_session = switch_core_session_request(&channel_endpoint_interface, NULL))) {
		struct private_object *tech_pvt;
		switch_channel *channel, *orig_channel;
		switch_caller_profile *caller_profile, *originator_caller_profile = NULL;

		switch_core_session_add_stream(*new_session, NULL);
		if ((tech_pvt =
			 (struct private_object *) switch_core_session_alloc(*new_session, sizeof(struct private_object)))) {
			memset(tech_pvt, 0, sizeof(*tech_pvt));
			channel = switch_core_session_get_channel(*new_session);
			switch_core_session_set_private(*new_session, tech_pvt);
			tech_pvt->session = *new_session;
		} else {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Hey where is my memory pool?\n");
			switch_core_session_destroy(new_session);
			return SWITCH_STATUS_GENERR;
		}

		if (outbound_profile) {
			char name[128];
			caller_profile = switch_caller_profile_clone(*new_session, outbound_profile);
			switch_channel_set_caller_profile(channel, caller_profile);
			tech_pvt->caller_profile = caller_profile;
			snprintf(name, sizeof(name), "Opal/%s-%04x", caller_profile->destination_number, rand() & 0xffff);
			switch_channel_set_name(channel, name);
		} else {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Doh! no caller profile\n");
			switch_core_session_destroy(new_session);
			return SWITCH_STATUS_GENERR;
		}

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

static switch_status channel_read_frame(switch_core_session *session, switch_frame **frame, int timeout,
										switch_io_flag flags, int stream_id)
{
	switch_channel *channel = NULL;
	struct private_object *tech_pvt = NULL;
	//switch_frame *pframe;
	//switch_status status;

	channel = switch_core_session_get_channel(session);
	assert(channel != NULL);

	tech_pvt = switch_core_session_get_private(session);
	assert(tech_pvt != NULL);

	return SWITCH_STATUS_SUCCESS;
}

static switch_status channel_write_frame(switch_core_session *session, switch_frame *frame, int timeout,
										 switch_io_flag flags, int stream_id)
{
	switch_channel *channel = NULL;
	struct private_object *tech_pvt = NULL;
	//switch_frame *pframe;

	channel = switch_core_session_get_channel(session);
	assert(channel != NULL);

	tech_pvt = switch_core_session_get_private(session);
	assert(tech_pvt != NULL);

	return SWITCH_STATUS_SUCCESS;

}

static const switch_event_handler_table channel_event_handlers = {
	/*.on_init */ channel_on_init,
	/*.on_ring */ channel_on_ring,
	/*.on_execute */ channel_on_execute,
	/*.on_hangup */ channel_on_hangup,
	/*.on_loopback */ channel_on_loopback,
	/*.on_transmit */ channel_on_transmit
};

static const switch_io_routines channel_io_routines = {
	/*.outgoing_channel */ channel_outgoing_channel,
	/*.answer_channel */ NULL,
	/*.read_frame */ channel_read_frame,
	/*.write_frame */ channel_write_frame,
	/*.kill_channel */ channel_kill_channel,
	/*.waitfor_read */ channel_waitfor_read,
	/*.waitfor_write */ channel_waitfor_write
};

static const switch_endpoint_interface channel_endpoint_interface = {
	/*.interface_name */ "opal",
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
	/*.application_interface */ NULL
};


/*



SWITCH_MOD_DECLARE(switch_status) switch_module_shutdown(void)
{
	int x = 0;
	opal_profile_thread_running(&default_profile, 1, 0);
	while (!opal_profile_thread_running(&default_profile, 0, 0)) {
		opal_socket_close(&default_profile.opal_socket);
		if (x++ > 10) {
			break;
		}
		switch_yield(1);
	}
	return SWITCH_STATUS_SUCCESS;
}
*/

SWITCH_MOD_DECLARE(switch_status) switch_module_load(const switch_loadable_module_interface **interface, char *filename)
{

	switch_config cfg;
	char *var, *val;
	char *cf = "opal.conf";

	memset(&globals, 0, sizeof(globals));

	if (!switch_config_open_file(&cfg, cf)) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "open of %s failed\n", cf);
		return SWITCH_STATUS_TERM;
	}


	while (switch_config_next_pair(&cfg, &var, &val)) {
		if (!strcasecmp(cfg.category, "settings")) {
			if (!strcmp(var, "debug")) {
				globals.debug = atoi(val);
			} else if (!strcmp(var, "port")) {
				globals.port = atoi(val);
			}
		}
	}

	switch_config_close_file(&cfg);


	if (switch_core_new_memory_pool(&module_pool) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "OH OH no pool\n");
		return SWITCH_STATUS_TERM;
	}

	/* connect my internal structure to the blank pointer passed to me */
	*interface = &channel_module_interface;

	/* indicate that the module should continue to be loaded */
	return SWITCH_STATUS_SUCCESS;
}



SWITCH_MOD_DECLARE(switch_status) switch_module_runtime(void)
{

	return SWITCH_STATUS_TERM;
}
