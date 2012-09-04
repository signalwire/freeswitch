/* 
 * mod_html5 for FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
 * Copyright (C) 2011-2012, Barracuda Networks Inc.
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
 * The Original Code is mod_html5 for FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
 *
 * The Initial Developer of the Original Code is Barracuda Networks Inc.
 * Portions created by the Initial Developer are Copyright (C)
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 * 
 * Anthony Minessale II <anthm@freeswitch.org>
 * Michael Jerris <mike@jerris.com>
 *
 * mod_html5.c -- HTML5 Endpoint Module
 *
 */

#include <switch.h>

#define HTML5_EVENT_CUSTOM "html5::custom"

struct html5_profile {
	char *name;			/* < Profile name */
	switch_memory_pool_t *pool;	/* < Memory pool */
	switch_thread_rwlock_t *rwlock;	/* < Rwlock for reference counting */
	uint32_t flags;			/* < PFLAGS */
	switch_mutex_t *mutex;		/* < Mutex for call count */
	int calls;			/* < Active calls count */
	int clients;			/* < Number of connected clients */
	switch_hash_t *session_hash;	/* < Active rtmp sessions */
	switch_thread_rwlock_t *session_rwlock; /* < rwlock for session hashtable */
	const char *context;		/* < Default dialplan name */
	const char *dialplan;		/* < Default dialplan context */
	const char *bind_address;	/* < Bind address */
	const char *io_name;		/* < Name of I/O module (from config) */
	int chunksize;				/* < Override default chunksize (from config) */
	int buffer_len;				/* < Receive buffer length the flash clients should use */ 
	
	switch_hash_t *reg_hash;	/* < Registration hashtable */
	switch_thread_rwlock_t *reg_rwlock; /* < Registration hash rwlock */
	
	switch_bool_t auth_calls;	/* < Require authentiation */
};

typedef struct html5_profile html5_profile_t;

struct html5_private {
 	unsigned int flags;
	switch_codec_t read_codec;
	switch_codec_t write_codec;
	
	switch_frame_t read_frame;
	unsigned char databuf[SWITCH_RECOMMENDED_BUFFER_SIZE];	/* < Buffer for read_frame */
	
	switch_caller_profile_t *caller_profile;
	
	switch_mutex_t *mutex;
	switch_mutex_t *flag_mutex;
	
	switch_core_session_t *session;
	switch_channel_t *channel;
};

typedef struct html5_private html5_private_t;

switch_status_t html5_on_execute(switch_core_session_t *session);
switch_status_t html5_send_dtmf(switch_core_session_t *session, const switch_dtmf_t *dtmf);
switch_status_t html5_receive_message(switch_core_session_t *session, switch_core_session_message_t *msg);
switch_status_t html5_receive_event(switch_core_session_t *session, switch_event_t *event);
switch_status_t html5_on_init(switch_core_session_t *session);
switch_status_t html5_on_hangup(switch_core_session_t *session);
switch_status_t html5_on_destroy(switch_core_session_t *session);
switch_status_t html5_on_routing(switch_core_session_t *session);
switch_status_t html5_on_exchange_media(switch_core_session_t *session);
switch_status_t html5_on_soft_execute(switch_core_session_t *session);
switch_call_cause_t html5_outgoing_channel(switch_core_session_t *session, switch_event_t *var_event,
 											switch_caller_profile_t *outbound_profile,
 											switch_core_session_t **new_session, switch_memory_pool_t **pool, switch_originate_flag_t flags,
 											switch_call_cause_t *cancel_cause);
switch_status_t html5_read_frame(switch_core_session_t *session, switch_frame_t **frame, switch_io_flag_t flags, int stream_id);
switch_status_t html5_write_frame(switch_core_session_t *session, switch_frame_t *frame, switch_io_flag_t flags, int stream_id);
switch_status_t html5_kill_channel(switch_core_session_t *session, int sig);

SWITCH_MODULE_LOAD_FUNCTION(mod_html5_load);
SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_html5_shutdown);
SWITCH_MODULE_DEFINITION(mod_html5, mod_html5_load, mod_html5_shutdown, NULL);

switch_state_handler_table_t html5_state_handlers = {
	/*.on_init */ html5_on_init,
	/*.on_routing */ html5_on_routing,
	/*.on_execute */ html5_on_execute,
	/*.on_hangup */ html5_on_hangup,
	/*.on_exchange_media */ html5_on_exchange_media,
	/*.on_soft_execute */ html5_on_soft_execute,
	/*.on_consume_media */ NULL,
	/*.on_hibernate */ NULL,
	/*.on_reset */ NULL,
	/*.on_park */ NULL,
	/*.on_reporting */ NULL,
	/*.on_destroy */ html5_on_destroy
};

switch_io_routines_t html5_io_routines = {
	/*.outgoing_channel */ html5_outgoing_channel,
	/*.read_frame */ html5_read_frame,
	/*.write_frame */ html5_write_frame,
	/*.kill_channel */ html5_kill_channel,
	/*.send_dtmf */ html5_send_dtmf,
	/*.receive_message */ html5_receive_message,
	/*.receive_event */ html5_receive_event
};

/* 
   State methods they get called when the state changes to the specific state 
   returning SWITCH_STATUS_SUCCESS tells the core to execute the standard state method next
   so if you fully implement the state you can return SWITCH_STATUS_FALSE to skip it.
*/
switch_status_t html5_on_init(switch_core_session_t *session)
{
	switch_channel_t *channel;
	html5_private_t *tech_pvt = NULL;

	tech_pvt = switch_core_session_get_private(session);
	assert(tech_pvt != NULL);

	channel = switch_core_session_get_channel(session);
	assert(channel != NULL);

	return SWITCH_STATUS_SUCCESS;
}

switch_status_t html5_on_routing(switch_core_session_t *session)
{
	switch_channel_t *channel = NULL;
	html5_private_t *tech_pvt = NULL;

	channel = switch_core_session_get_channel(session);
	assert(channel != NULL);

	tech_pvt = switch_core_session_get_private(session);
	assert(tech_pvt != NULL);

	switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "%s CHANNEL ROUTING\n", switch_channel_get_name(channel));

	return SWITCH_STATUS_SUCCESS;
}

switch_status_t html5_on_execute(switch_core_session_t *session)
{
	switch_channel_t *channel = NULL;
	html5_private_t *tech_pvt = NULL;

	channel = switch_core_session_get_channel(session);
	assert(channel != NULL);

	tech_pvt = switch_core_session_get_private(session);
	assert(tech_pvt != NULL);

	switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "%s CHANNEL EXECUTE\n", switch_channel_get_name(channel));

	return SWITCH_STATUS_SUCCESS;
}

switch_status_t html5_on_destroy(switch_core_session_t *session)
{
	switch_channel_t *channel = NULL;
	html5_private_t *tech_pvt = NULL;

	channel = switch_core_session_get_channel(session);
	assert(channel != NULL);

	tech_pvt = switch_core_session_get_private(session);

	if (tech_pvt == NULL) {
		return SWITCH_STATUS_SUCCESS;
	}

	return SWITCH_STATUS_SUCCESS;
}

switch_status_t html5_on_hangup(switch_core_session_t *session)
{
	switch_channel_t *channel = NULL;
	html5_private_t *tech_pvt = NULL;

	channel = switch_core_session_get_channel(session);
	assert(channel != NULL);

	tech_pvt = switch_core_session_get_private(session);
	assert(tech_pvt != NULL);

	return SWITCH_STATUS_SUCCESS;
}

switch_status_t html5_kill_channel(switch_core_session_t *session, int sig)
{
	switch_channel_t *channel = NULL;
	html5_private_t *tech_pvt = NULL;

	channel = switch_core_session_get_channel(session);
	assert(channel != NULL);

	tech_pvt = switch_core_session_get_private(session);
	assert(tech_pvt != NULL);

	switch (sig) {
	case SWITCH_SIG_KILL:
		break;
	case SWITCH_SIG_BREAK:
		break;
	default:
		break;
	}

	return SWITCH_STATUS_SUCCESS;
}

switch_status_t html5_on_exchange_media(switch_core_session_t *session)
{
	switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "CHANNEL LOOPBACK\n");
	return SWITCH_STATUS_SUCCESS;
}

switch_status_t html5_on_soft_execute(switch_core_session_t *session)
{
	switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "CHANNEL TRANSMIT\n");
	return SWITCH_STATUS_SUCCESS;
}

switch_status_t html5_send_dtmf(switch_core_session_t *session, const switch_dtmf_t *dtmf)
{
	html5_private_t *tech_pvt = switch_core_session_get_private(session);
	switch_assert(tech_pvt != NULL);

	return SWITCH_STATUS_SUCCESS;
}

switch_status_t html5_read_frame(switch_core_session_t *session, switch_frame_t **frame, switch_io_flag_t flags, int stream_id)
{
	switch_channel_t *channel = NULL;
	html5_private_t *tech_pvt = NULL;

	channel = switch_core_session_get_channel(session);
	assert(channel != NULL);

	tech_pvt = switch_core_session_get_private(session);
	assert(tech_pvt != NULL);
	
	//	*frame = &tech_pvt->read_frame;
	
	return SWITCH_STATUS_SUCCESS;
}

switch_status_t html5_write_frame(switch_core_session_t *session, switch_frame_t *frame, switch_io_flag_t flags, int stream_id)
{
	switch_channel_t *channel = NULL;
	html5_private_t *tech_pvt = NULL;
	
	channel = switch_core_session_get_channel(session);
	assert(channel != NULL);

	tech_pvt = switch_core_session_get_private(session);
	assert(tech_pvt != NULL);

	return SWITCH_STATUS_SUCCESS;
}

switch_status_t html5_receive_message(switch_core_session_t *session, switch_core_session_message_t *msg)
{
	switch_channel_t *channel;
	html5_private_t *tech_pvt;

	channel = switch_core_session_get_channel(session);
	assert(channel != NULL);

	tech_pvt = (html5_private_t *) switch_core_session_get_private(session);
	assert(tech_pvt != NULL);

	switch (msg->message_id) {
	case SWITCH_MESSAGE_INDICATE_ANSWER:
		switch_channel_mark_answered(channel);
		break;
	case SWITCH_MESSAGE_INDICATE_RINGING:
		switch_channel_mark_ring_ready(channel);
		break;
	case SWITCH_MESSAGE_INDICATE_PROGRESS:
		switch_channel_mark_pre_answered(channel);
		break;
	case SWITCH_MESSAGE_INDICATE_HOLD:
	case SWITCH_MESSAGE_INDICATE_UNHOLD:
		break;
	case SWITCH_MESSAGE_INDICATE_DISPLAY:
		break;
	default:
		break;
	}

	return SWITCH_STATUS_SUCCESS;
}

/* Make sure when you have 2 sessions in the same scope that you pass the appropriate one to the routines
   that allocate memory or you will have 1 channel with memory allocated from another channel's pool!
*/
switch_call_cause_t html5_outgoing_channel(switch_core_session_t *session, switch_event_t *var_event,
													switch_caller_profile_t *outbound_profile,
													switch_core_session_t **newsession, switch_memory_pool_t **inpool, switch_originate_flag_t flags,
													switch_call_cause_t *cancel_cause)
{
	//	html5_private_t *tech_pvt;
	//	switch_caller_profile_t *caller_profile;
	//	switch_channel_t *channel;
	switch_call_cause_t cause = SWITCH_CAUSE_DESTINATION_OUT_OF_ORDER;
	
	return cause;
}

switch_status_t html5_receive_event(switch_core_session_t *session, switch_event_t *event)
{
	html5_private_t *tech_pvt = switch_core_session_get_private(session);
	switch_assert(tech_pvt != NULL);

	return SWITCH_STATUS_SUCCESS;
}

#if 0

static switch_xml_config_item_t *get_instructions(html5_profile_t *profile) {
	switch_xml_config_item_t *dup;
	switch_xml_config_item_t instructions[] = {
		/* parameter name        type                 reloadable   pointer                         default value     options structure */
		SWITCH_CONFIG_ITEM("context", SWITCH_CONFIG_STRING, CONFIG_RELOADABLE, &profile->context, "public", &switch_config_string_strdup,
						   "", "The dialplan context to use for inbound calls"),
		SWITCH_CONFIG_ITEM("dialplan", SWITCH_CONFIG_STRING, CONFIG_RELOADABLE, &profile->dialplan, "XML", &switch_config_string_strdup,
						   "", "The dialplan to use for inbound calls"),
		SWITCH_CONFIG_ITEM("bind-address", SWITCH_CONFIG_STRING, 0, &profile->bind_address, "0.0.0.0:1935", &switch_config_string_strdup,
						   "ip:port", "IP and port to bind"),
		SWITCH_CONFIG_ITEM("auth-calls", SWITCH_CONFIG_BOOL, CONFIG_RELOADABLE, &profile->auth_calls, SWITCH_FALSE, NULL, "true|false", "Set to true in order to reject unauthenticated calls"),
		SWITCH_CONFIG_ITEM_END()
	};
	
	dup = malloc(sizeof(instructions));
	memcpy(dup, instructions, sizeof(instructions));
	return dup;
}

static switch_status_t config_profile(html5_profile_t *profile, switch_bool_t reload)
{
	switch_xml_t cfg, xml, x_profiles, x_profile, x_settings;
	switch_status_t status = SWITCH_STATUS_FALSE;
	switch_xml_config_item_t *instructions = (profile ? get_instructions(profile) : NULL);
	switch_event_t *event = NULL;
	int count;
	const char *file = "html5.conf";
	
	if (!(xml = switch_xml_open_cfg(file, &cfg, NULL))) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Could not open %s\n", file);
		goto done;
	}
	
	if (!(x_profiles = switch_xml_child(cfg, "profiles"))) {
		goto done;
	}
	
	for (x_profile = switch_xml_child(x_profiles, "profile"); x_profile; x_profile = x_profile->next) {
		const char *name = switch_xml_attr_soft(x_profile, "name");
		if (strcmp(name, profile->name)) {
			continue;
		}
		
		if (!(x_settings = switch_xml_child(x_profile, "settings"))) {
			goto done;
		}
		
		
		count = switch_event_import_xml(switch_xml_child(x_settings, "param"), "name", "value", &event);
		status = switch_xml_config_parse_event(event, count, reload, instructions);
	}
	
	
done:
	if (xml) {
		switch_xml_free(xml);	
	}
	switch_safe_free(instructions);
	if (event) {
		switch_event_destroy(&event);
	}
	return status;
}
#endif

static void html5_event_handler(switch_event_t *event)
{
	if (!event) {
		return;
	}
}

#define HTML5_CONTACT_FUNCTION_SYNTAX "profile/user@domain[/[!]nickname]"
SWITCH_STANDARD_API(html5_contact_function)
{
	int argc;
	char *argv[5];
	char *dup = NULL;
	//	char *szprofile = NULL, *user = NULL; 
	//	const char *nickname = NULL;

	if (zstr(cmd)) {
		goto usage;
	}
	
	dup = strdup(cmd);
	argc = switch_split(dup, '/', argv);
	
	if (argc < 2 || zstr(argv[0]) || zstr(argv[1])) {
		goto usage;
	}
	
	//	szprofile = argv[0];

	if (!strchr(argv[1], '@')) {
		goto usage;
	} 
	
	//	user = argv[1];
	//	nickname = argv[2];
	
	goto done;
	
usage:
	stream->write_function(stream, "Usage: html5_contact "HTML5_CONTACT_FUNCTION_SYNTAX"\n");

done:
	switch_safe_free(dup);
	return SWITCH_STATUS_SUCCESS;
}

#define HTML5_FUNCTION_SYNTAX "profile [profilename] [start | stop | rescan | restart]\nstatus profile [profilename]\nstatus profile [profilename] [reg | sessions]\nsession [session_id] [kill | login [user@domain] | logout [user@domain]]"
SWITCH_STANDARD_API(html5_function)
{
	int argc;
	char *argv[10];
	char *dup = NULL;
	
	if (zstr(cmd)) {
		goto usage;
	}
	
	dup = strdup(cmd);
	argc = switch_split(dup, ' ', argv);
	
	if (argc < 1 || zstr(argv[0])) {
		goto usage;
	}
	
	goto done;
	
usage:
	stream->write_function(stream, "-ERR Usage: "HTML5_FUNCTION_SYNTAX"\n");

done:
	switch_safe_free(dup);
	return SWITCH_STATUS_SUCCESS;
}

SWITCH_MODULE_LOAD_FUNCTION(mod_html5_load)
{
	switch_api_interface_t *api_interface;
	switch_endpoint_interface_t *html5_endpoint_interface;

	*module_interface = switch_loadable_module_create_module_interface(pool, modname);

	html5_endpoint_interface = switch_loadable_module_create_interface(*module_interface, SWITCH_ENDPOINT_INTERFACE);
	html5_endpoint_interface->interface_name = "html5";
	html5_endpoint_interface->io_routines = &html5_io_routines;
	html5_endpoint_interface->state_handler = &html5_state_handlers;
	
	SWITCH_ADD_API(api_interface, "html5", "html5 management", html5_function, HTML5_FUNCTION_SYNTAX);
	SWITCH_ADD_API(api_interface, "html5_contact", "html5 contact", html5_contact_function, HTML5_CONTACT_FUNCTION_SYNTAX);

	//	switch_console_set_complete("add html5 status");
	
	switch_event_bind("mod_html5", SWITCH_EVENT_CUSTOM, HTML5_EVENT_CUSTOM, html5_event_handler, NULL);
	
	{
		switch_xml_t cfg, xml, x_profiles, x_profile;
		const char *file = "html5.conf";

		if (!(xml = switch_xml_open_cfg(file, &cfg, NULL))) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Could not open %s\n", file);
			goto done;
		}

		if (!(x_profiles = switch_xml_child(cfg, "profiles"))) {
			goto done;
		}

		for (x_profile = switch_xml_child(x_profiles, "profile"); x_profile; x_profile = x_profile->next) {
			//const char *name = switch_xml_attr_soft(x_profile, "name");
			//html5_profile_start(name);
		}
		done:
		if (xml) {
			switch_xml_free(xml);	
		}
	}
	
	return SWITCH_STATUS_SUCCESS;
}

SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_html5_shutdown)
{
	switch_event_unbind_callback(html5_event_handler);
	
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
