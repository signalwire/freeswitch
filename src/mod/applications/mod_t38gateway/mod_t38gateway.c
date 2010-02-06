/* 
 * FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
 * Copyright (C) 2009, Steve Underwood <steveu@coppice.org>
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
 * Contributor(s):
 * 
 * Steve Underwood <steveu@coppice.org>
 *
 * mod_t38gateway.c -- A T.38 gateway
 *
 * This module uses the T.38 gateway engine of spandsp to create a T.38 gateway suitable for
 * V.17, V.29, and V.27ter FAX operation.
 * 
 */

#include <switch.h>

#define SPANDSP_EXPOSE_INTERNAL_STRUCTURES
#include <spandsp.h>
#include <spandsp/version.h>

#include "udptl.h"

/*! Syntax of the API call. */
#define T38GATEWAY_SYNTAX "<uuid> <command>"

/*! Number of expected parameters in api call. */
#define T38GATEWAY_PARAMS 2

/*! FreeSWITCH CUSTOM event types. */
#define T38GATEWAY_EVENT_CNG "t38gateway::cng"
#define T38GATEWAY_EVENT_CED "t38gateway::ced"
#define T38GATEWAY_EVENT_PAGE "t38gateway::page"

/* Prototypes */
SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_t38gateway_shutdown);
SWITCH_STANDARD_API(t38gateway_api_main);

SWITCH_MODULE_LOAD_FUNCTION(mod_t38gateway_load);
SWITCH_MODULE_DEFINITION(mod_t38gateway, mod_t38gateway_load, NULL, NULL);
SWITCH_STANDARD_APP(t38gateway_start_function);

/*! Type that holds codec information. */
typedef struct {
	/*! The sampling rate of the audio stream. */
	int rate;
	/*! The number of channels. */
	int channels;
} t38gateway_codec_info_t;

/*! Type that holds session information pertinent to the t38gateway module. */
typedef struct {
	/*! Internal FreeSWITCH session. */
	switch_core_session_t *session;
	/*! Codec information for the session. */
	t38gateway_codec_info_t t38gateway_codec;
	t38_gateway_state_t *t38;
	t38_core_state_t *t38_core;
	udptl_state_t *udptl;
} t38gateway_session_info_t;

static int tx_packet_handler(t38_core_state_t *s, void *user_data, const uint8_t *buf, int len, int count)
{
	t38gateway_session_info_t *t;

	t = (t38gateway_session_info_t *) user_data;
	return 0;
}

/*- End of function --------------------------------------------------------*/

static int rx_packet_handler(void *user_data, const uint8_t *buf, int len, int seq_no)
{
	t38gateway_session_info_t *t;

	t = (t38gateway_session_info_t *) user_data;
	t38_core_rx_ifp_packet(t->t38_core, buf, len, seq_no);
	return 0;
}

/*- End of function --------------------------------------------------------*/

/*! \brief The callback function that is called when new audio data becomes available 
 *
 * @author Steve Underwood
 * @param bug A reference to the media bug.
 * @param user_data The session information for this call.
 * @param type The switch callback type.
 * @return The success or failure of the function.
 */
static switch_bool_t t38gateway_callback(switch_media_bug_t *bug, void *user_data, switch_abc_type_t type)
{
	t38gateway_session_info_t *t38gateway_info;
	switch_codec_t *read_codec;
	switch_frame_t *frame;
	int16_t *amp;

	t38gateway_info = (t38gateway_session_info_t *) user_data;
	if (t38gateway_info == NULL) {
		return SWITCH_FALSE;
	}

	switch (type) {
	case SWITCH_ABC_TYPE_INIT:
		read_codec = switch_core_session_get_read_codec(t38gateway_info->session);
		t38gateway_info->t38gateway_codec.rate = read_codec->implementation->samples_per_second;
		t38gateway_info->t38gateway_codec.channels = read_codec->implementation->number_of_channels;
		break;
	case SWITCH_ABC_TYPE_READ_PING:
	case SWITCH_ABC_TYPE_CLOSE:
		break;
	case SWITCH_ABC_TYPE_READ:
		frame = switch_core_media_bug_get_read_replace_frame(bug);
		amp = (int16_t *) frame->data;
		t38_gateway_rx(t38gateway_info->t38, amp, frame->samples);
		break;
	case SWITCH_ABC_TYPE_WRITE:
	case SWITCH_ABC_TYPE_READ_REPLACE:
	case SWITCH_ABC_TYPE_WRITE_REPLACE:
		break;
	}

	return SWITCH_TRUE;
}

/*! \brief FreeSWITCH module loading function 
 *
 * @author Steve Underwood
 * @return Load success or failure.
 */
SWITCH_MODULE_LOAD_FUNCTION(mod_t38gateway_load)
{
	switch_application_interface_t *app_interface;
	switch_api_interface_t *api_interface;
	/* connect my internal structure to the blank pointer passed to me */
	*module_interface = switch_loadable_module_create_module_interface(pool, modname);

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "T.38 gateway enabled\n");

	SWITCH_ADD_APP(app_interface, "t38gateway", "T.38 gateway", "T.38 gateway", t38gateway_start_function, "[start] [stop]", SAF_NONE);

	SWITCH_ADD_API(api_interface, "t38gateway", "T.38 gateway", t38gateway_api_main, T38GATEWAY_SYNTAX);

	/* indicate that the module should continue to be loaded */
	return SWITCH_STATUS_SUCCESS;
}

/*! \brief FreeSWITCH application handler function.
 *  This handles calls made from applications such as LUA and the dialplan
 *
 * @author Steve Underwood
 * @return Success or failure of the function.
 */
SWITCH_STANDARD_APP(t38gateway_start_function)
{
	switch_media_bug_t *bug;
	switch_status_t status;
	switch_channel_t *channel;
	t38gateway_session_info_t *t38gateway_info;

	if (session == NULL)
		return;

	channel = switch_core_session_get_channel(session);

	/* Is this channel already set? */
	bug = (switch_media_bug_t *) switch_channel_get_private(channel, "_t38gateway_");
	/* If yes */
	if (bug != NULL) {
		/* If we have a stop remove audio bug */
		if (strcasecmp(data, "stop") == 0) {
			switch_channel_set_private(channel, "_t38gateway_", NULL);
			switch_core_media_bug_remove(session, &bug);
			return;
		}

		/* We have already started */
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_WARNING, "Cannot run 2 at once on the same channel!\n");

		return;
	}

	t38gateway_info = (t38gateway_session_info_t *) switch_core_session_alloc(session, sizeof(t38gateway_session_info_t));

	t38gateway_info->session = session;
	t38gateway_info->t38 = t38_gateway_init(NULL, tx_packet_handler, (void *) t38gateway_info);
	t38gateway_info->t38_core = t38_gateway_get_t38_core_state(t38gateway_info->t38);
	t38gateway_info->udptl = udptl_init(NULL, UDPTL_ERROR_CORRECTION_REDUNDANCY, 3, 3, rx_packet_handler, (void *) t38gateway_info);

	status = switch_core_media_bug_add(session, t38gateway_callback, t38gateway_info, 0, SMBF_READ_STREAM, &bug);

	if (status != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "Failure hooking to stream\n");
		return;
	}

	switch_channel_set_private(channel, "_t38gateway_", bug);
}

/*! \brief Called when the module shuts down
 *
 * @author Steve Underwood
 * @return The success or failure of the function.
 */
SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_t38gateway_shutdown)
{
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "T.38 gateway disabled\n");

	return SWITCH_STATUS_SUCCESS;
}

/*! \brief FreeSWITCH API handler function.
 *  This function handles API calls such as the ones from mod_event_socket and in some cases
 *  scripts such as LUA scripts.
 *
 *  @author Steve Underwood
 *  @return The success or failure of the function.
 */
SWITCH_STANDARD_API(t38gateway_api_main)
{
	switch_core_session_t *t38gateway_session;
	switch_media_bug_t *bug;
	t38gateway_session_info_t *t38gateway_info;
	switch_channel_t *channel;
	switch_status_t status;
	int argc;
	char *argv[T38GATEWAY_PARAMS];
	char *ccmd;
	char *uuid;
	char *command;

	/* No command? Display usage */
	if (cmd == NULL) {
		stream->write_function(stream, "-USAGE: %s\n", T38GATEWAY_SYNTAX);
		return SWITCH_STATUS_SUCCESS;
	}

	/* Duplicated contents of original string */
	ccmd = strdup(cmd);
	/* Separate the arguments */
	argc = switch_separate_string(ccmd, ' ', argv, T38GATEWAY_PARAMS);

	/* If we don't have the expected number of parameters 
	 * display usage */
	if (argc != T38GATEWAY_PARAMS) {
		stream->write_function(stream, "-USAGE: %s\n", T38GATEWAY_SYNTAX);
		switch_safe_free(ccmd);
		return SWITCH_STATUS_SUCCESS;
	}

	uuid = argv[0];
	command = argv[1];

	/* using uuid locate a reference to the FreeSWITCH session */
	t38gateway_session = switch_core_session_locate(uuid);

	/* If the session was not found exit */
	if (t38gateway_session == NULL) {
		switch_safe_free(ccmd);
		stream->write_function(stream, "-USAGE: %s\n", T38GATEWAY_SYNTAX);
		return SWITCH_STATUS_FALSE;
	}

	/* Get current channel of the session to tag the session
	 * This indicates that our module is present */
	channel = switch_core_session_get_channel(t38gateway_session);

	/* Is this channel already set? */
	bug = (switch_media_bug_t *) switch_channel_get_private(channel, "_t38gateway_");
	/* If yes */
	if (bug != NULL) {
		/* If we have a stop remove audio bug */
		if (strcasecmp(command, "stop") == 0) {
			switch_channel_set_private(channel, "_t38gateway_", NULL);
			switch_core_media_bug_remove(t38gateway_session, &bug);
			switch_safe_free(ccmd);
			stream->write_function(stream, "+OK\n");
			return SWITCH_STATUS_SUCCESS;
		}

		/* We have already started */
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "Cannot run 2 at once on the same channel!\n");

		switch_safe_free(ccmd);
		return SWITCH_STATUS_FALSE;
	}

	/* If we don't see the expected start exit */
	if (strcasecmp(command, "start") != 0) {
		switch_safe_free(ccmd);
		stream->write_function(stream, "-USAGE: %s\n", T38GATEWAY_SYNTAX);
		return SWITCH_STATUS_FALSE;
	}

	/* Allocate memory attached to this FreeSWITCH session for
	 * use in the callback routine and to store state information */
	t38gateway_info = (t38gateway_session_info_t *) switch_core_session_alloc(t38gateway_session, sizeof(t38gateway_session_info_t));

	/* Set initial values and states */
	t38gateway_info->session = t38gateway_session;
	t38gateway_info->t38 = t38_gateway_init(NULL, tx_packet_handler, (void *) t38gateway_info);
	t38gateway_info->t38_core = t38_gateway_get_t38_core_state(t38gateway_info->t38);
	t38gateway_info->udptl = udptl_init(NULL, UDPTL_ERROR_CORRECTION_REDUNDANCY, 3, 3, rx_packet_handler, (void *) t38gateway_info);

	/* Add a media bug that allows me to intercept the 
	 * reading leg of the audio stream */
	status = switch_core_media_bug_add(t38gateway_session, t38gateway_callback, t38gateway_info, 0, SMBF_READ_STREAM, &bug);

	/* If adding a media bug fails exit */
	if (status != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Failure hooking to stream\n");

		switch_safe_free(ccmd);
		return SWITCH_STATUS_FALSE;
	}

	/* Set the t38gateway tag to detect an existing t38gateway media bug */
	switch_channel_set_private(channel, "_t38gateway_", bug);

	/* Everything went according to plan! Notify the user */
	stream->write_function(stream, "+OK\n");

	switch_safe_free(ccmd);
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
