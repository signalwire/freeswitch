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

static void audio_bridge_function(switch_core_session *session, char *data)
{
	switch_channel *caller_channel;
	switch_core_session *peer_session;
	switch_caller_profile *caller_profile, *caller_caller_profile;
	char chan_type[128] = { '\0' }, *chan_data;
	unsigned int timelimit = 60;			/* probably a useful option to pass in when there's time */
	caller_channel = switch_core_session_get_channel(session);
	assert(caller_channel != NULL);


	strncpy(chan_type, data, sizeof(chan_type));

	if ((chan_data = strchr(chan_type, '/')) != 0) {
		*chan_data = '\0';
		chan_data++;
	}

	caller_caller_profile = switch_channel_get_caller_profile(caller_channel);
	caller_profile = switch_caller_profile_new(switch_core_session_get_pool(session),
											   caller_caller_profile->dialplan,
											   caller_caller_profile->caller_id_name,
											   caller_caller_profile->caller_id_number,
											   caller_caller_profile->network_addr, 
											   NULL, 
											   NULL, 
											   caller_caller_profile->rdnis,
											   caller_caller_profile->source,
											   caller_caller_profile->context,
											   chan_data);



	if (switch_core_session_outgoing_channel(session, chan_type, caller_profile, &peer_session, NULL) !=
		SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Cannot Create Outgoing Channel!\n");
		switch_channel_hangup(caller_channel, SWITCH_CAUSE_REQUESTED_CHAN_UNAVAIL);
		return;
	} else {
		switch_ivr_multi_threaded_bridge(session, peer_session, timelimit, NULL, NULL, NULL);
	}
}


static const switch_application_interface bridge_application_interface = {
	/*.interface_name */ "bridge",
	/*.application_function */ audio_bridge_function
};


static const switch_loadable_module_interface mod_bridgecall_module_interface = {
	/*.module_name = */ modname,
	/*.endpoint_interface = */ NULL,
	/*.timer_interface = */ NULL,
	/*.dialplan_interface = */ NULL,
	/*.codec_interface = */ NULL,
	/*.application_interface */ &bridge_application_interface
};

SWITCH_MOD_DECLARE(switch_status) switch_module_load(const switch_loadable_module_interface **interface, char *filename)
{

	/* connect my internal structure to the blank pointer passed to me */
	*interface = &mod_bridgecall_module_interface;

	/* indicate that the module should continue to be loaded */
	return SWITCH_STATUS_SUCCESS;
}
