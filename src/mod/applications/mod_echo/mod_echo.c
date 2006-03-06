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
 * mod_echo.c -- Echo application
 *
 */
#include <switch.h>

static const char modname[] = "mod_echo";

static void echo_function(switch_core_session *session, char *data)
{
	switch_channel *channel;
	switch_frame *frame;
	char *codec_name; 
	switch_codec codec, *read_codec;

	channel = switch_core_session_get_channel(session);
    assert(channel != NULL);	

	read_codec = switch_core_session_get_read_codec(session); 

	switch_channel_answer(channel);

	codec_name = "L16";
	if (switch_core_codec_init(&codec,
							   codec_name,
							   read_codec->implementation->samples_per_second,
							   read_codec->implementation->microseconds_per_frame / 1000,
							   read_codec->implementation->number_of_channels,
							   SWITCH_CODEC_FLAG_ENCODE | SWITCH_CODEC_FLAG_DECODE,
							   NULL, switch_core_session_get_pool(session)) == SWITCH_STATUS_SUCCESS) {
		switch_console_printf(SWITCH_CHANNEL_CONSOLE, "Raw Codec Activated\n");
		switch_core_session_set_read_codec(session, &codec);		
		switch_core_session_set_read_codec(session, &codec);
		while(switch_channel_ready(channel)) { 
			switch_core_session_read_frame(session, &frame, -1, 0);
			switch_core_session_write_frame(session, frame, -1 ,0);
		}
	}
}

static const switch_application_interface echo_application_interface = {
	/*.interface_name */ "echo",
	/*.application_function */ echo_function,
	NULL,NULL,NULL,NULL
};

static switch_loadable_module_interface echo_module_interface = {
	/*.module_name */ modname,
	/*.endpoint_interface */ NULL,
	/*.timer_interface */ NULL,
	/*.dialplan_interface */ NULL,
	/*.codec_interface */ NULL,
	/*.application_interface */ &echo_application_interface,
	/*.api_interface */ NULL,
	/*.file_interface */ NULL,
	/*.speech_interface */ NULL,
	/*.directory_interface */ NULL
};

SWITCH_MOD_DECLARE(switch_status) switch_module_load(const switch_loadable_module_interface **interface, char *filename)
{
	/* connect my internal structure to the blank pointer passed to me */
	*interface = &echo_module_interface;

	/* indicate that the module should continue to be loaded */
	return SWITCH_STATUS_SUCCESS;
}
