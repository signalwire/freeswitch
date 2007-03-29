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

static void audio_bridge_function(switch_core_session_t *session, char *data)
{
	switch_channel_t *caller_channel;
	switch_core_session_t *peer_session;
	unsigned int timelimit = 60;
	char *var;
	uint8_t no_media_bridge = 0;
	switch_call_cause_t cause = SWITCH_CAUSE_NORMAL_CLEARING;
	uint8_t do_continue = 0;

	if (switch_strlen_zero(data)) {
		return;
	}

	caller_channel = switch_core_session_get_channel(session);
	assert(caller_channel != NULL);

	if ((var = switch_channel_get_variable(caller_channel, "call_timeout"))) {
		timelimit = atoi(var);
	}

	if ((var = switch_channel_get_variable(caller_channel, "continue_on_fail"))) {
		do_continue = switch_true(var);
	}

	if (switch_channel_test_flag(caller_channel, CF_NOMEDIA)
		|| ((var = switch_channel_get_variable(caller_channel, "no_media")) && switch_true(var))) {
		if (!switch_channel_test_flag(caller_channel, CF_ANSWERED)
			&& !switch_channel_test_flag(caller_channel, CF_EARLY_MEDIA)) {
			switch_channel_set_flag(caller_channel, CF_NOMEDIA);
		} else {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG,
							  "Channel is already up, delaying point-to-point mode 'till both legs are up.\n");
			no_media_bridge = 1;
		}
	}

	if (switch_ivr_originate(session, &peer_session, &cause, data, timelimit, NULL, NULL, NULL, NULL) !=
		SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "Originate Failed.  Cause: %s\n",
						  switch_channel_cause2str(cause));
		if (!do_continue && cause != SWITCH_CAUSE_NO_ANSWER) {
			/* All Causes besides NO_ANSWER terminate the originating session unless continue_on_fail is set.
			   We will pass the fail cause on when we hangup. */
			switch_channel_hangup(caller_channel, cause);
		}
		/* Otherwise.. nobody answered.  Go back to the dialplan instructions in case there was more to do. */
		return;
	} else {
		if (no_media_bridge) {
			switch_channel_t *peer_channel = switch_core_session_get_channel(peer_session);
			switch_frame_t *read_frame;
			/* SIP won't let us redir media until the call has been answered #$^#%& so we will proxy any early media until they do */
			while (switch_channel_ready(caller_channel) && switch_channel_ready(peer_channel)
				   && !switch_channel_test_flag(peer_channel, CF_ANSWERED)) {
				switch_status_t status = switch_core_session_read_frame(peer_session, &read_frame, -1, 0);
				uint8_t bad = 1;

				if (SWITCH_READ_ACCEPTABLE(status)
					&& switch_core_session_write_frame(session, read_frame, -1, 0) == SWITCH_STATUS_SUCCESS) {
					bad = 0;
				}
				if (bad) {
					switch_channel_hangup(caller_channel, SWITCH_CAUSE_DESTINATION_OUT_OF_ORDER);
					switch_channel_hangup(peer_channel, SWITCH_CAUSE_DESTINATION_OUT_OF_ORDER);
					return;
				}
			}

			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Redirecting media to point-to-point mode.\n");
			switch_ivr_nomedia(switch_core_session_get_uuid(session), SMF_FORCE);
			switch_ivr_nomedia(switch_core_session_get_uuid(peer_session), SMF_FORCE);
			switch_ivr_signal_bridge(session, peer_session);
		} else {
			if (switch_channel_test_flag(caller_channel, CF_NOMEDIA)) {
				switch_ivr_signal_bridge(session, peer_session);
			} else {
				switch_ivr_multi_threaded_bridge(session, peer_session, NULL, NULL, NULL);
			}
		}
	}
}

static const switch_application_interface_t bridge_application_interface = {
	/*.interface_name */ "bridge",
	/*.application_function */ audio_bridge_function,
	/* long_desc */ "Bridge the audio between two sessions",
	/* short_desc */ "Bridge Audio",
	/* syntax */ "<channel_url>",
	/* flags */ SAF_SUPPORT_NOMEDIA
};

static const switch_loadable_module_interface_t mod_bridgecall_module_interface = {
	/*.module_name = */ modname,
	/*.endpoint_interface = */ NULL,
	/*.timer_interface = */ NULL,
	/*.dialplan_interface = */ NULL,
	/*.codec_interface = */ NULL,
	/*.application_interface */ &bridge_application_interface
};

SWITCH_MOD_DECLARE(switch_status_t) switch_module_load(const switch_loadable_module_interface_t **module_interface,
													   char *filename)
{

	/* connect my internal structure to the blank pointer passed to me */
	*module_interface = &mod_bridgecall_module_interface;

	/* indicate that the module should continue to be loaded */
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
 * vim:set softtabstop=4 shiftwidth=4 tabstop=4 expandtab:
 */
