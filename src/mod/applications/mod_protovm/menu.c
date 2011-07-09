/* 
 * FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
 * Copyright (C) 2005-2011, Anthony Minessale II <anthm@freeswitch.org>
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
 * Anthony Minessale II <anthm@freeswitch.org>
 * Portions created by the Initial Developer are Copyright (C)
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 * 
 * Marc Olivier Chouinard <mochouinard@moctel.com>
 *
 *
 * menu.c -- VoiceMail Menu
 *
 */
#include <switch.h>

#include "ivr.h"
#include "menu.h"
#include "util.h"
#include "config.h"

/* List of available menu */
vmivr_menu_function_t menu_list[] = {
	{"std_authenticate", mtvm_menu_authenticate},
	{"std_navigator", mtvm_menu_main},
	{"std_record_name", mtvm_menu_record_name},
	{"std_set_password", mtvm_menu_set_password},
	{"std_select_greeting_slot", mtvm_menu_select_greeting_slot},
	{"std_record_greeting_with_slot", mtvm_menu_record_greeting_with_slot},
	{"std_preference", mtvm_menu_preference},
	{"std_purge", mtvm_menu_purge},
	{"std_forward", mtvm_menu_forward},
	{ NULL, NULL }
};

#define MAX_ATTEMPT 3 /* TODO Make these fields configurable */
#define DEFAULT_IVR_TIMEOUT 3000

void mtvm_menu_purge(switch_core_session_t *session, vmivr_profile_t *profile) {
	if (profile->id && profile->authorized) {
		if (1==1 /* TODO make Purge email on exit optional ??? */) {
			const char *cmd = switch_core_session_sprintf(session, "%s %s %s", profile->api_profile, profile->domain, profile->id);
			mt_api_execute(session, profile->api_msg_purge, cmd);
		}
	}
}
void mtvm_menu_main(switch_core_session_t *session, vmivr_profile_t *profile) {
	switch_channel_t *channel = switch_core_session_get_channel(session);
	switch_event_t *msg_list_params = NULL;
	size_t msg_count = 0;
	size_t current_msg = 1;
	size_t next_msg = current_msg;
	size_t previous_msg = current_msg;
	char *cmd = NULL;
	int retry;

	/* Different switch to control playback of phrases */
	switch_bool_t initial_count_played = SWITCH_FALSE;
	switch_bool_t skip_header = SWITCH_FALSE;
	switch_bool_t msg_deleted = SWITCH_FALSE;
	switch_bool_t msg_undeleted = SWITCH_FALSE;
	switch_bool_t msg_saved = SWITCH_FALSE;

	vmivr_menu_profile_t menu = { "std_navigator" };

	/* Initialize Menu Configs */
	populate_profile_menu_event(profile, &menu);

	if (!menu.event_keys_dtmf || !menu.event_phrases) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Missing Menu Phrases or Keys\n");
		return;
	}

	/* Get VoiceMail List And update msg count */
	cmd = switch_core_session_sprintf(session, "json %s %s %s", profile->api_profile, profile->domain, profile->id);
	msg_list_params = jsonapi2event(session, NULL, profile->api_msg_list, cmd);
	msg_count = atol(switch_event_get_header(msg_list_params,"VM-List-Count"));

	/* TODO Add Detection of new message and notify the user */

	for (retry = MAX_ATTEMPT; switch_channel_ready(channel) && retry > 0; retry--) {
		switch_core_session_message_t msg = { 0 };
		char cid_buf[1024] = "";
		dtmf_ss_t loc;
		char *dtmfa[16] = { 0 };
		switch_event_t *phrase_params = NULL;

		switch_event_create(&phrase_params, SWITCH_EVENT_REQUEST_PARAMS);

		append_event_profile(phrase_params, profile, menu);

		populate_dtmfa_from_event(phrase_params, profile, menu, dtmfa);

		previous_msg = current_msg;

		/* Simple Protection to not go out of msg list scope */
		/* TODO: Add Prompt to notify they reached the begining or the end */
		if (next_msg == 0) {
			next_msg = 1;
		} else if (next_msg > msg_count) {
			next_msg = msg_count;
		} 

		current_msg = next_msg;

		captureMenuInitialize(&loc, dtmfa);

		/* Prompt related to previous Message here */
		append_event_message(session, profile, phrase_params, msg_list_params, previous_msg);
		if (msg_deleted) {
			msg_deleted = SWITCH_FALSE;
			captureMenu(session, &loc, switch_event_get_header(menu.event_phrases, "ack"), "deleted", phrase_params, NULL, 0);
		}
		if (msg_undeleted) {
			msg_undeleted = SWITCH_FALSE;
			captureMenu(session, &loc, switch_event_get_header(menu.event_phrases, "ack"), "undeleted", phrase_params, NULL, 0);
		} 
		if (msg_saved) {
			msg_saved = SWITCH_FALSE;
			captureMenu(session, &loc, switch_event_get_header(menu.event_phrases, "ack"), "saved", phrase_params, NULL, 0);
		}

		/* Prompt related the current message */
		append_event_message(session, profile, phrase_params, msg_list_params, current_msg);

		/* Save in profile the current msg info for other menu processing AND restoration of our current position */
		switch_snprintf(cid_buf, sizeof(cid_buf), "%s|%s", switch_str_nil(switch_event_get_header(phrase_params, "VM-Message-Caller-Number")), switch_str_nil(switch_event_get_header(phrase_params, "VM-Message-Caller-Name")));

		/* Display MSG CID/Name to caller */
		msg.from = __FILE__;
		msg.string_arg = cid_buf;
		msg.message_id = SWITCH_MESSAGE_INDICATE_DISPLAY;
		switch_core_session_receive_message(session, &msg);

		profile->current_msg = current_msg;
		profile->current_msg_uuid = switch_core_session_strdup(session, switch_event_get_header(phrase_params, "VM-Message-UUID"));
		
		/* TODO check if msg is gone (purged by another session, notify user and auto jump to next message or something) */
		if (!skip_header) {
			if (!initial_count_played) {
				cmd = switch_core_session_sprintf(session, "json %s %s %s", profile->api_profile, profile->domain, profile->id);
				jsonapi2event(session, phrase_params, profile->api_msg_count, cmd);
				initial_count_played = SWITCH_TRUE;
				captureMenu(session, &loc, switch_event_get_header(menu.event_phrases, "msg_count"), NULL, phrase_params, NULL, 0);
			}
			if (msg_count > 0) {
				captureMenu(session, &loc, switch_event_get_header(menu.event_phrases, "say_msg_number"), NULL, phrase_params, NULL, 0);
				captureMenu(session, &loc, switch_event_get_header(menu.event_phrases, "say_date"), NULL, phrase_params, NULL, 0);
			}
		}
		if (msg_count > 0) {
			/* TODO Update the Read date of a message (When msg start, or when it listen compleatly ??? To be determined */
			captureMenu(session, &loc, switch_event_get_header(menu.event_phrases, "play_message"), NULL, phrase_params, NULL, 0);
		}
		skip_header = SWITCH_FALSE;

		captureMenu(session, &loc, switch_event_get_header(menu.event_phrases, "menu_options"), NULL, phrase_params, NULL, DEFAULT_IVR_TIMEOUT);

		if (loc.result == RES_TIMEOUT) {
			/* TODO Ask for the prompt Again IF retry != 0 */
		} else if (loc.result == RES_INVALID) {
			/* TODO Say invalid option, and ask for the prompt again IF retry != 0 */
		} else if (loc.result == RES_FOUND) {  /* Matching DTMF Key Pressed */
			const char *action = switch_event_get_header(menu.event_keys_dtmf, loc.dtmf_stored);

			/* Reset the try count */
			retry = MAX_ATTEMPT;

			if (action) {
				if (!strcasecmp(action, "skip_intro")) { /* Skip Header / Play the recording again */
					skip_header = SWITCH_TRUE;
				} else if (!strcasecmp(action, "next_msg")) { /* Next Message */
					next_msg++;
				} else if (!strcasecmp(action, "prev_msg")) { /* Previous Message */
					next_msg--;
				} else if (!strcasecmp(action, "delete_msg")) { /* Delete / Undelete Message */
					const char *msg_flags = switch_event_get_header(phrase_params, "VM-Message-Flags");
					if (!msg_flags || strncasecmp(msg_flags, "delete", 6)) {
						cmd = switch_core_session_sprintf(session, "%s %s %s %s", profile->api_profile, profile->domain, profile->id, switch_event_get_header(phrase_params, "VM-Message-UUID"));
						mt_api_execute(session, profile->api_msg_delete, cmd);

						msg_deleted = SWITCH_TRUE;
						/* TODO Option for auto going to next message or just return to the menu (So user used to do 76 to delete and next message wont be confused) */
						next_msg++;
					} else { 
						cmd = switch_core_session_sprintf(session, "%s %s %s %s", profile->api_profile, profile->domain, profile->id, switch_event_get_header(phrase_params, "VM-Message-UUID"));
						mt_api_execute(session, profile->api_msg_undelete, cmd);

						msg_undeleted = SWITCH_TRUE;
					}
				} else if (!strcasecmp(action, "save_msg")) { /* Save Message */
					cmd = switch_core_session_sprintf(session, "%s %s %s %s", profile->api_profile, profile->domain, profile->id, switch_event_get_header(phrase_params, "VM-Message-UUID"));
					mt_api_execute(session, profile->api_msg_save, cmd);

					msg_saved = SWITCH_TRUE;
				} else if (!strcasecmp(action, "callback")) { /* CallBack caller */
					const char *cid_num = switch_event_get_header(phrase_params, "VM-Message-Caller-Number");
					if (cid_num) {
						/* TODO add detection for private number */
						switch_core_session_execute_exten(session, cid_num, "XML", profile->domain);
					} else {
						/* TODO Some error msg that the msg doesn't contain a caller number */
					}
				} else if (!strncasecmp(action, "menu:", 5)) { /* Sub Menu */
					void (*fPtr)(switch_core_session_t *session, vmivr_profile_t *profile) = mtvm_get_menu_function(action+5);
					if (fPtr) {
						fPtr(session, profile);
					}
				} else if (!strcasecmp(action, "return")) { /* Return */
					retry = -1;
				}
			}
		}

		/* IF the API to get the message returned us a COPY of the file locally (temp file create from a DB or from a web server), delete it */
		if (switch_true(switch_event_get_header(phrase_params, "VM-Message-Private-Local-Copy"))) {
			const char *file_path = switch_event_get_header(phrase_params, "VM-Message-File-Path");
			if (file_path && unlink(file_path) != 0) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "Failed to delete temp file [%s]\n", file_path);
			}
		}
		switch_event_destroy(&phrase_params);
	}

	switch_event_destroy(&msg_list_params);

	free_profile_menu_event(&menu);

	return;
}

void mtvm_menu_forward(switch_core_session_t *session, vmivr_profile_t *profile) {

	vmivr_menu_profile_t menu = { "std_forward_ask_prepend" };
	switch_channel_t *channel = switch_core_session_get_channel(session);
	const char *prepend_filepath = NULL;
	int retry;
	switch_bool_t forward_msg = SWITCH_FALSE;

	/* Initialize Menu Configs */
	populate_profile_menu_event(profile, &menu);

	if (!menu.event_keys_dtmf || !menu.event_phrases) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Missing Menu Phrases and Keys\n");
		return;
	}

	for (retry = MAX_ATTEMPT; switch_channel_ready(channel) && retry > 0; retry--) {
		dtmf_ss_t loc;
		char *dtmfa[16] = { 0 };
		switch_event_t *phrase_params = NULL;

		switch_event_create(&phrase_params, SWITCH_EVENT_REQUEST_PARAMS);
		append_event_profile(phrase_params, profile, menu);

		populate_dtmfa_from_event(phrase_params, profile, menu, dtmfa);

		captureMenuInitialize(&loc, dtmfa);

		captureMenu(session, &loc, switch_event_get_header(menu.event_phrases, "menu_options"), NULL, phrase_params, NULL, DEFAULT_IVR_TIMEOUT);

		if (loc.result == RES_TIMEOUT) {
			/* TODO Ask for the prompt Again IF retry != 0 */
		} else if (loc.result == RES_INVALID) {
			/* TODO Say invalid option, and ask for the prompt again IF retry != 0 */
		} else if (loc.result == RES_FOUND) {  /* Matching DTMF Key Pressed */
			const char *action = switch_event_get_header(menu.event_keys_dtmf, loc.dtmf_stored);

			/* Reset the try count */
			retry = MAX_ATTEMPT;

			if (action) {
				if (!strcasecmp(action, "return")) { /* Return to the previous menu */
					retry = -1;
					forward_msg = SWITCH_FALSE;
				} else if (!strcasecmp(action, "prepend")) { /* Prepend record msg */
					vmivr_menu_profile_t sub_menu = { "std_record_message" };
					char *tmp_filepath = generate_random_file_name(session, "protovm", "wav" /* TODO make it configurable */);
					switch_status_t status;

					/* Initialize Menu Configs */
					populate_profile_menu_event(profile, &sub_menu);

					status =  mtvm_menu_record(session, profile, sub_menu, tmp_filepath);

					if (status == SWITCH_STATUS_SUCCESS) {
						//char *cmd = switch_core_session_sprintf(session, "%s %s %s %d %s", profile->api_profile, profile->domain, profile->id, gnum, tmp_filepath);
						//char *str_num = switch_core_session_sprintf(session, "%d", gnum);
						//mt_api_execute(session, profile->api_pref_greeting_set, cmd);
						//playbackBufferDTMF(session, switch_event_get_header(menu.event_phrases, "selected_slot"), str_num, NULL, NULL, 0);
						prepend_filepath = tmp_filepath;
						retry = -1;
						forward_msg = SWITCH_TRUE;
					} else {
						/* TODO Error Recording msg */
					}
					free_profile_menu_event(&sub_menu);

				} else if (!strcasecmp(action, "forward")) { /* Forward without prepend msg */
					retry = -1;
					forward_msg = SWITCH_TRUE;
				} else if (!strncasecmp(action, "menu:", 5)) { /* Sub Menu */
					void (*fPtr)(switch_core_session_t *session, vmivr_profile_t *profile) = mtvm_get_menu_function(action+5);
					if (fPtr) {
						fPtr(session, profile);
					}
				}
			}
		}
		switch_event_destroy(&phrase_params);


	}
	/* Ask Extension to Forward */
	if (forward_msg) {
		for (retry = MAX_ATTEMPT; switch_channel_ready(channel) && retry > 0; retry--) {
			const char *id = NULL;
			vmivr_menu_profile_t sub_menu = { "std_forward_ask_extension" };

			/* Initialize Menu Configs */
			populate_profile_menu_event(profile, &sub_menu);

			id = mtvm_menu_get_input_set(session, profile, sub_menu, "X.", "#" /* TODO Conf terminate input key */);
			if (id) {
				const char *cmd = switch_core_session_sprintf(session, "%s %s %s %s %s %s %s%s%s", profile->api_profile, profile->domain, profile->id, profile->current_msg_uuid, profile->domain, id, prepend_filepath?" ":"", prepend_filepath?prepend_filepath:"" );
				if (mt_api_execute(session, profile->api_msg_forward, cmd) == SWITCH_STATUS_SUCCESS) {
					playbackBufferDTMF(session, switch_event_get_header(sub_menu.event_phrases, "ack"), "saved", NULL, NULL, 0);
					retry = -1;
				} else {
					playbackBufferDTMF(session, switch_event_get_header(sub_menu.event_phrases, "invalid_extension"), NULL, NULL, NULL, 0);
				}
			} else {
				/* TODO Prompt about input not valid */
			}
			free_profile_menu_event(&sub_menu);
			/* TODO add Confirmation of the transfered number */
		}
		/* TODO Ask if we want to transfer the msg to more person */

	}

	free_profile_menu_event(&menu);
}


void mtvm_menu_record_name(switch_core_session_t *session, vmivr_profile_t *profile) {
	switch_status_t status;
	vmivr_menu_profile_t menu = { "std_record_name" };

	char *tmp_filepath = generate_random_file_name(session, "protovm", "wav" /* TODO make it configurable */);

	/* Initialize Menu Configs */
	populate_profile_menu_event(profile, &menu);

	status = mtvm_menu_record(session, profile, menu, tmp_filepath);

	if (status == SWITCH_STATUS_SUCCESS) {
		char *cmd = switch_core_session_sprintf(session, "%s %s %s %s", profile->api_profile, profile->domain, profile->id, tmp_filepath);
		mt_api_execute(session, profile->api_pref_recname_set, cmd);
	}
}

void mtvm_menu_set_password(switch_core_session_t *session, vmivr_profile_t *profile) {
	char *password;
	vmivr_menu_profile_t menu = { "std_set_password" };

	password = mtvm_menu_get_input_set(session, profile, menu, "XXX." /* TODO Conf Min 3 Digit */, "#" /* TODO Conf terminate input key */);

	/* TODO Add Prompts to tell if password was set and if it was not */
	if (password) {
		char *cmd = switch_core_session_sprintf(session, "%s %s %s %s", profile->api_profile, profile->domain, profile->id, password);
		mt_api_execute(session, profile->api_pref_password_set, cmd);
	}

	free_profile_menu_event(&menu);
}

void mtvm_menu_authenticate(switch_core_session_t *session, vmivr_profile_t *profile) {
	switch_channel_t *channel = switch_core_session_get_channel(session);
        vmivr_menu_profile_t menu = { "std_authenticate" };
	int retry;
	const char *auth_var = NULL;
	/* Initialize Menu Configs */
	populate_profile_menu_event(profile, &menu);

	if (profile->id && (auth_var = switch_channel_get_variable(channel, "voicemail_authorized")) && switch_true(auth_var)) {
		profile->authorized = SWITCH_TRUE;
	}

	for (retry = MAX_ATTEMPT; switch_channel_ready(channel) && retry > 0 && profile->authorized == SWITCH_FALSE; retry--) {
		const char *id = profile->id, *password = NULL;
		char *cmd = NULL;

		if (!id) {
			vmivr_menu_profile_t sub_menu = { "std_authenticate_ask_user" };
			/* Initialize Menu Configs */
			populate_profile_menu_event(profile, &sub_menu);

			id = mtvm_menu_get_input_set(session, profile, sub_menu, "X." /* TODO Conf Min 3 Digit */, "#" /* TODO Conf terminate input key */);
			free_profile_menu_event(&sub_menu);
		}
		if (!password) {
			vmivr_menu_profile_t sub_menu = { "std_authenticate_ask_password" };
			/* Initialize Menu Configs */
			populate_profile_menu_event(profile, &sub_menu);

			password = mtvm_menu_get_input_set(session, profile, sub_menu, "X." /* TODO Conf Min 3 Digit */, "#" /* TODO Conf terminate input key */);
			free_profile_menu_event(&sub_menu);
		}
		cmd = switch_core_session_sprintf(session, "%s %s %s %s", profile->api_profile, profile->domain, id, password);

		if (mt_api_execute(session, profile->api_auth_login, cmd) == SWITCH_STATUS_SUCCESS) {
			profile->id = id;
			profile->authorized = SWITCH_TRUE;
		} else {
			playbackBufferDTMF(session, switch_event_get_header(menu.event_phrases, "fail_auth"), NULL, NULL, NULL, 0);
		}
	}
	free_profile_menu_event(&menu);
}

void mtvm_menu_select_greeting_slot(switch_core_session_t *session, vmivr_profile_t *profile) {
	vmivr_menu_profile_t menu = { "std_select_greeting_slot" };

	const char *result;
	int gnum = -1;

	/* Initialize Menu Configs */
	populate_profile_menu_event(profile, &menu);

	result = mtvm_menu_get_input_set(session, profile, menu, "X", NULL);

	if (result)
		gnum = atoi(result);
	if (gnum != -1) {
		char * cmd = switch_core_session_sprintf(session, "%s %s %s %d", profile->api_profile, profile->domain, profile->id, gnum);
		if (mt_api_execute(session, profile->api_pref_greeting_set, cmd) == SWITCH_STATUS_SUCCESS) {
			char *str_num = switch_core_session_sprintf(session, "%d", gnum);
			playbackBufferDTMF(session, switch_event_get_header(menu.event_phrases, "selected_slot"), str_num, NULL, NULL, 0);
		} else {
			playbackBufferDTMF(session, switch_event_get_header(menu.event_phrases, "invalid_slot"), NULL, NULL, NULL, 0);
		}
	}
	free_profile_menu_event(&menu);
}

void mtvm_menu_record_greeting_with_slot(switch_core_session_t *session, vmivr_profile_t *profile) {

	vmivr_menu_profile_t menu = { "std_record_greeting_with_slot" };

	const char *result;
	int gnum = -1;

	/* Initialize Menu Configs */
	populate_profile_menu_event(profile, &menu);

	result = mtvm_menu_get_input_set(session, profile, menu, "X", NULL);

	if (result)
		gnum = atoi(result);

	/* If user entered 0, we don't accept it */
	if (gnum > 0) {
		vmivr_menu_profile_t sub_menu = { "std_record_greeting" };
		char *tmp_filepath = generate_random_file_name(session, "protovm", "wav" /* TODO make it configurable */);
		switch_status_t status;

		/* Initialize Menu Configs */
		populate_profile_menu_event(profile, &sub_menu);

		status =  mtvm_menu_record(session, profile, sub_menu, tmp_filepath);

		if (status == SWITCH_STATUS_SUCCESS) {
			char *cmd = switch_core_session_sprintf(session, "%s %s %s %d %s", profile->api_profile, profile->domain, profile->id, gnum, tmp_filepath);
			char *str_num = switch_core_session_sprintf(session, "%d", gnum);
			mt_api_execute(session, profile->api_pref_greeting_set, cmd);
			playbackBufferDTMF(session, switch_event_get_header(menu.event_phrases, "selected_slot"), str_num, NULL, NULL, 0);
		}
		free_profile_menu_event(&sub_menu);

	}

	free_profile_menu_event(&menu);

}

void mtvm_menu_preference(switch_core_session_t *session, vmivr_profile_t *profile) {
	switch_channel_t *channel = switch_core_session_get_channel(session);

	int retry;

	vmivr_menu_profile_t menu = { "std_preference" };

	/* Initialize Menu Configs */
	populate_profile_menu_event(profile, &menu);

	if (!menu.event_keys_dtmf || !menu.event_phrases) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Missing Menu Phrases and Keys\n");
		return;
	}

	for (retry = MAX_ATTEMPT; switch_channel_ready(channel) && retry > 0; retry--) {
		dtmf_ss_t loc;
		char *dtmfa[16] = { 0 };
		switch_event_t *phrase_params = NULL;

		switch_event_create(&phrase_params, SWITCH_EVENT_REQUEST_PARAMS);
		append_event_profile(phrase_params, profile, menu);

		populate_dtmfa_from_event(phrase_params, profile, menu, dtmfa);

		captureMenuInitialize(&loc, dtmfa);

		captureMenu(session, &loc, switch_event_get_header(menu.event_phrases, "menu_options"), NULL, phrase_params, NULL, DEFAULT_IVR_TIMEOUT);

		if (loc.result == RES_TIMEOUT) {
			/* TODO Ask for the prompt Again IF retry != 0 */
		} else if (loc.result == RES_INVALID) {
			/* TODO Say invalid option, and ask for the prompt again IF retry != 0 */
		} else if (loc.result == RES_FOUND) {  /* Matching DTMF Key Pressed */
			const char *action = switch_event_get_header(menu.event_keys_dtmf, loc.dtmf_stored);

			/* Reset the try count */
			retry = MAX_ATTEMPT;

			if (action) {
				if (!strcasecmp(action, "return")) { /* Return to the previous menu */
					retry = -1;
				} else if (!strncasecmp(action, "menu:", 5)) { /* Sub Menu */
					void (*fPtr)(switch_core_session_t *session, vmivr_profile_t *profile) = mtvm_get_menu_function(action+5);
					if (fPtr) {
						fPtr(session, profile);
					}
				}
			}
		}
		switch_event_destroy(&phrase_params);
	}

	free_profile_menu_event(&menu);
}

char *mtvm_menu_get_input_set(switch_core_session_t *session, vmivr_profile_t *profile, vmivr_menu_profile_t menu, const char *input_mask, const char *terminate_key) {
	char *result = NULL;
	int retry;

	switch_channel_t *channel = switch_core_session_get_channel(session);

	if (!menu.event_keys_dtmf || !menu.event_phrases) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Missing Menu Phrases and Keys : %s\n", menu.name);
		return result;
	}

	for (retry = MAX_ATTEMPT; switch_channel_ready(channel) && retry > 0; retry--) {
		dtmf_ss_t loc;
		char *dtmfa[16] = { 0 };
		int i;
		switch_event_t *phrase_params = NULL;

		switch_event_create(&phrase_params, SWITCH_EVENT_REQUEST_PARAMS);
		append_event_profile(phrase_params, profile, menu);

		populate_dtmfa_from_event(phrase_params, profile, menu, dtmfa);
		
		/* Find the last entry and append this one to it */
		for (i=0; dtmfa[i] && i < 16; i++){
		}
                dtmfa[i] = (char *) input_mask;

		captureMenuInitialize(&loc, dtmfa);
		if (terminate_key)
			loc.terminate_key = terminate_key[0]; /* TODO Make this load from the configuration */
		captureMenu(session, &loc, switch_event_get_header(menu.event_phrases, "instructions"), NULL, phrase_params, NULL, DEFAULT_IVR_TIMEOUT);

		if (loc.result == RES_TIMEOUT) {
			/* TODO Ask for the prompt Again IF retry != 0 */
		} else if (loc.result == RES_INVALID) {
			/* TODO Say invalid option, and ask for the prompt again IF retry != 0 */
		} else if (loc.result == RES_FOUND) {  /* Matching DTMF Key Pressed */

			/* Reset the try count */
			retry = MAX_ATTEMPT;

			if (!strncasecmp(loc.completeMatch, input_mask, 1)) {
				result = switch_core_session_strdup(session, loc.dtmf_stored);
				retry = -1;

			}
		}
		switch_event_destroy(&phrase_params);
	}

	return result;
}

switch_status_t mtvm_menu_record(switch_core_session_t *session, vmivr_profile_t *profile, vmivr_menu_profile_t menu, const char *file_name) {
	switch_status_t status = SWITCH_STATUS_FALSE;
	switch_channel_t *channel = switch_core_session_get_channel(session);
	int retry;

	switch_bool_t record_prompt = SWITCH_TRUE;
	switch_bool_t listen_recording = SWITCH_FALSE;
	switch_bool_t play_instruction = SWITCH_TRUE;

	if (!menu.event_keys_dtmf || !menu.event_phrases) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Missing Menu Phrases and Keys\n");
		return status;
	}

	for (retry = MAX_ATTEMPT; switch_channel_ready(channel) && retry > 0; retry--) {
		dtmf_ss_t loc;

		char *dtmfa[16] = { 0 };
		switch_event_t *phrase_params = NULL;
		switch_file_handle_t fh = { 0 };

		/* TODO Make the following configurable */
		fh.thresh = 200;
		fh.silence_hits = 4;
		//fh.samplerate = 8000;


		switch_event_create(&phrase_params, SWITCH_EVENT_REQUEST_PARAMS);
		append_event_profile(phrase_params, profile, menu);

		populate_dtmfa_from_event(phrase_params, profile, menu, dtmfa);

		captureMenuInitialize(&loc, dtmfa);
		if (record_prompt) {
			if (play_instruction) {
				captureMenu(session, &loc, switch_event_get_header(menu.event_phrases, "instructions"), NULL, phrase_params, NULL, 0);
			}
			play_instruction = SWITCH_TRUE;

			captureMenuRecord(session, &loc, phrase_params, file_name, &fh, 30 /* TODO Make max recording configurable */);
		} else {
			if (listen_recording) {
				switch_event_add_header(phrase_params, SWITCH_STACK_BOTTOM, "VM-Record-File-Path", "%s", file_name);
				captureMenu(session, &loc, switch_event_get_header(menu.event_phrases, "play_recording"), NULL, phrase_params, NULL, 0);
				listen_recording = SWITCH_FALSE;

			}
			captureMenu(session, &loc, switch_event_get_header(menu.event_phrases, "menu_options"), NULL, phrase_params, NULL, DEFAULT_IVR_TIMEOUT);
		}

		if (loc.recorded_audio) {
			/* Reset the try count */
			retry = MAX_ATTEMPT;

			/* TODO Check if message is too short */

			record_prompt = SWITCH_FALSE;

		} else if (loc.result == RES_TIMEOUT) {
			/* TODO Ask for the prompt Again IF retry != 0 */
		} else if (loc.result == RES_INVALID) {
			/* TODO Say invalid option, and ask for the prompt again IF retry != 0 */
		} else if (loc.result == RES_FOUND) {  /* Matching DTMF Key Pressed */
			const char *action = switch_event_get_header(menu.event_keys_dtmf, loc.dtmf_stored);

			/* Reset the try count */
			retry = MAX_ATTEMPT;

			if (action) {
				if (!strcasecmp(action, "listen")) { /* Listen */
					listen_recording = SWITCH_TRUE;

				} else if (!strcasecmp(action, "save")) {
					retry = -1;
					/* TODO ALLOW SAVE ONLY IF FILE IS RECORDED AND HIGHER THAN MIN SIZE */
					status = SWITCH_STATUS_SUCCESS;

				} else if (!strcasecmp(action, "rerecord")) {
					record_prompt = SWITCH_TRUE;

				} else if (!strcasecmp(action, "skip_instruction")) { /* Skip Recording Greeting */
					play_instruction = SWITCH_FALSE;

				} else if (!strncasecmp(action, "menu:", 5)) { /* Sub Menu */
					void (*fPtr)(switch_core_session_t *session, vmivr_profile_t *profile) = mtvm_get_menu_function(action+5);
					if (fPtr) {
						fPtr(session, profile);
					}
				} else if (!strcasecmp(action, "return")) { /* Return */
					retry = -1;
				}
			}
		}
		switch_event_destroy(&phrase_params);
	}
	return status;
}


void (*mtvm_get_menu_function(const char *menu_name))(switch_core_session_t *session, vmivr_profile_t *profile) {
	int i = 0;

	if (menu_name) {
		for (i=0; menu_list[i].name ; i++) {
			if (!strcasecmp(menu_list[i].name, menu_name)) {
				return menu_list[i].pt2Func;
			}
		}
	}
	return NULL;
}



/* For Emacs:
 * Local Variables:
 * mode:c
 * indent-tabs-mode:t
 * tab-width:4
 * c-basic-offset:4
 * End:
 * For VIM:
 * vim:set softtabstop=4 shiftwidth=4 tabstop=4
 */
