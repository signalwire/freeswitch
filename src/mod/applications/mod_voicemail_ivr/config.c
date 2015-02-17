/* 
 * FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
 * Copyright (C) 2005-2014, Anthony Minessale II <anthm@freeswitch.org>
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
 * config.c -- VoiceMail IVR Config
 *
 */
#include <switch.h>

#include "config.h"

const char *global_cf = "voicemail_ivr.conf";

static void append_event_profile(vmivr_menu_t *menu);
static void populate_dtmfa_from_event(vmivr_menu_t *menu);

void menu_init(vmivr_profile_t *profile, vmivr_menu_t *menu) {
	switch_xml_t cfg, xml, x_profiles, x_profile, x_keys, x_phrases, x_menus, x_menu, x_settings;

	menu->profile = profile;

	if (!(xml = switch_xml_open_cfg(global_cf, &cfg, NULL))) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Open of %s failed\n", global_cf);
		goto end;
	}
	if (!(x_profiles = switch_xml_child(cfg, "profiles"))) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "No profiles group\n");
		goto end;
	}

	if (profile->event_settings) {
		/* TODO Replace this with a switch_event_merge_not_set(...) */
		switch_event_t *menu_default;
		switch_event_create(&menu_default, SWITCH_EVENT_REQUEST_PARAMS);
		if (menu->event_settings) {
			switch_event_merge(menu_default, menu->event_settings);
			switch_event_destroy(&menu->event_settings);
		}
		
		switch_event_create(&menu->event_settings, SWITCH_EVENT_REQUEST_PARAMS);
		switch_event_merge(menu->event_settings, profile->event_settings);
		switch_event_merge(menu->event_settings, menu_default);
		switch_event_destroy(&menu_default);
	}

	{
		const char *s_max_attempts = switch_event_get_header(menu->event_settings, "IVR-Maximum-Attempts");
		const char *s_entry_timeout = switch_event_get_header(menu->event_settings, "IVR-Entry-Timeout");
		menu->ivr_maximum_attempts = atoi(s_max_attempts);
		menu->ivr_entry_timeout = atoi(s_entry_timeout);
	}

	if ((x_profile = switch_xml_find_child(x_profiles, "profile", "name", profile->name))) {
		if ((x_menus = switch_xml_child(x_profile, "menus"))) {
			if ((x_menu = switch_xml_find_child(x_menus, "menu", "name", menu->name))) {

				if ((x_keys = switch_xml_child(x_menu, "keys"))) {
					switch_event_import_xml(switch_xml_child(x_keys, "key"), "dtmf", "action", &menu->event_keys_dtmf);
					switch_event_import_xml(switch_xml_child(x_keys, "key"), "action", "dtmf", &menu->event_keys_action);
					switch_event_import_xml(switch_xml_child(x_keys, "key"), "action", "variable", &menu->event_keys_varname);
				}
				if ((x_phrases = switch_xml_child(x_menu, "phrases"))) {
					switch_event_import_xml(switch_xml_child(x_phrases, "phrase"), "name", "value", &menu->event_phrases);
				}
				if ((x_settings = switch_xml_child(x_menu, "settings"))) {
					switch_event_import_xml(switch_xml_child(x_settings, "param"), "name", "value", &menu->event_settings);
				}

			}
		}
	}

	if (!menu->phrase_params) {
		switch_event_create(&menu->phrase_params, SWITCH_EVENT_REQUEST_PARAMS);
	}

end:
	if (xml)
		switch_xml_free(xml);
	return;

}

void menu_instance_init(vmivr_menu_t *menu) {
	append_event_profile(menu);

	populate_dtmfa_from_event(menu);
}

void menu_instance_free(vmivr_menu_t *menu) {
	if (menu->phrase_params) {
		switch_event_destroy(&menu->phrase_params);
		menu->phrase_params = NULL;
	}
	memset(&menu->ivre_d, 0, sizeof(menu->ivre_d));
}

void menu_free(vmivr_menu_t *menu) {
	if (menu->event_keys_dtmf) {
		switch_event_destroy(&menu->event_keys_dtmf);
	}
	if (menu->event_keys_action) {
		switch_event_destroy(&menu->event_keys_action);
	}
	if (menu->event_keys_varname) {
		switch_event_destroy(&menu->event_keys_varname);
	}

	if (menu->event_phrases) {
		switch_event_destroy(&menu->event_phrases);
	}
	if (menu->event_settings) {
		switch_event_destroy(&menu->event_settings);
	}

}

static void append_event_profile(vmivr_menu_t *menu) {

	if (!menu->phrase_params) {
		switch_event_create(&menu->phrase_params, SWITCH_EVENT_REQUEST_PARAMS);
	}

	/* Used for some appending function */
	if (menu->profile && menu->profile->name && menu->profile->id && menu->profile->domain) {
		switch_event_add_header(menu->phrase_params, SWITCH_STACK_BOTTOM, "VM-Profile", "%s", menu->profile->name);
		switch_event_add_header(menu->phrase_params, SWITCH_STACK_BOTTOM, "VM-Account-ID", "%s", menu->profile->id);
		switch_event_add_header(menu->phrase_params, SWITCH_STACK_BOTTOM, "VM-Account-Domain", "%s", menu->profile->domain);
	}
}

static void populate_dtmfa_from_event(vmivr_menu_t *menu) {
	int i = 0;
	if (menu->event_keys_dtmf) {
		switch_event_header_t *hp;

		for (hp = menu->event_keys_dtmf->headers; hp; hp = hp->next) {
			if (strlen(hp->name) < 3 && hp->value) { /* TODO This is a hack to discard default FS Events ! */
				const char *varphrasename = switch_event_get_header(menu->event_keys_varname, hp->value);
				menu->dtmfa[i++] = hp->name;

				if (varphrasename && !zstr(varphrasename)) {
					switch_event_add_header(menu->phrase_params, SWITCH_STACK_BOTTOM, varphrasename, "%s", hp->name);
				}
			}
		}
	}
	menu->dtmfa[i++] = NULL;
}

vmivr_profile_t *get_profile(switch_core_session_t *session, const char *profile_name)
{
	vmivr_profile_t *profile = NULL;
	switch_xml_t cfg, xml, x_profiles, x_profile, x_apis, x_settings, param;

	if (!(xml = switch_xml_open_cfg(global_cf, &cfg, NULL))) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Open of %s failed\n", global_cf);
		return profile;
	}
	if (!(x_profiles = switch_xml_child(cfg, "profiles"))) {
		goto end;
	}

	if ((x_profile = switch_xml_find_child(x_profiles, "profile", "name", profile_name))) {
		if (!(profile = switch_core_session_alloc(session, sizeof(vmivr_profile_t)))) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Alloc Failure\n");
			goto end;	
		}

		profile->name = profile_name;

		profile->current_msg = 0;
		profile->current_msg_uuid = NULL;

		profile->folder_name = VM_FOLDER_ROOT;
		profile->folder_filter = VM_MSG_NOT_READ;

		/* TODO Make the following configurable */
		profile->api_profile = profile->name;
		profile->menu_check_auth = "std_authenticate";
		profile->menu_check_main = "std_main_menu";
		profile->menu_check_terminate = "std_purge";

		/* Populate default general settings */
		switch_event_create(&profile->event_settings, SWITCH_EVENT_REQUEST_PARAMS);
		switch_event_add_header(profile->event_settings, SWITCH_STACK_BOTTOM, "IVR-Maximum-Attempts", "%d", 3);
		switch_event_add_header(profile->event_settings, SWITCH_STACK_BOTTOM, "IVR-Entry-Timeout", "%d", 3000);
		switch_event_add_header(profile->event_settings, SWITCH_STACK_BOTTOM, "Exit-Purge", "%s", "true");
		switch_event_add_header(profile->event_settings, SWITCH_STACK_BOTTOM, "Password-Mask", "%s", "XXX.");
		switch_event_add_header(profile->event_settings, SWITCH_STACK_BOTTOM, "User-Mask", "%s", "X.");
		switch_event_add_header(profile->event_settings, SWITCH_STACK_BOTTOM, "Record-Format", "%s", "wav");
		switch_event_add_header(profile->event_settings, SWITCH_STACK_BOTTOM, "Record-Silence-Hits", "%d", 4);
		switch_event_add_header(profile->event_settings, SWITCH_STACK_BOTTOM, "Record-Silence-Threshold", "%d", 200);
		switch_event_add_header(profile->event_settings, SWITCH_STACK_BOTTOM, "Record-Maximum-Length", "%d", 30);

		if ((x_settings = switch_xml_child(x_profile, "settings"))) {
			switch_event_import_xml(switch_xml_child(x_settings, "param"), "name", "value", &profile->event_settings);
		}

		if ((x_apis = switch_xml_child(x_profile, "apis"))) {
			int total_options = 0;
			int total_invalid_options = 0;
			for (param = switch_xml_child(x_apis, "api"); param; param = param->next) {
				char *var, *val;
				if ((var = (char *) switch_xml_attr_soft(param, "name")) && (val = (char *) switch_xml_attr_soft(param, "value"))) {
					if (!strcasecmp(var, "msg_undelete") && !profile->api_msg_undelete)
						profile->api_msg_undelete = switch_core_session_strdup(session, val);
					else if (!strcasecmp(var, "msg_delete") && !profile->api_msg_delete)
						profile->api_msg_delete = switch_core_session_strdup(session, val);
					else if (!strcasecmp(var, "msg_list") && !profile->api_msg_list)
						profile->api_msg_list = switch_core_session_strdup(session, val);
					else if (!strcasecmp(var, "msg_count") && !profile->api_msg_count)
						profile->api_msg_count = switch_core_session_strdup(session, val);
					else if (!strcasecmp(var, "msg_save") && !profile->api_msg_save)
						profile->api_msg_save = switch_core_session_strdup(session, val);
					else if (!strcasecmp(var, "msg_purge") && !profile->api_msg_purge)
						profile->api_msg_purge = switch_core_session_strdup(session, val);
					else if (!strcasecmp(var, "msg_get") && !profile->api_msg_get)
						profile->api_msg_get = switch_core_session_strdup(session, val);
					else if (!strcasecmp(var, "msg_forward") && !profile->api_msg_forward)
						profile->api_msg_forward = switch_core_session_strdup(session, val);
					else if (!strcasecmp(var, "pref_greeting_set") && !profile->api_pref_greeting_set)
						profile->api_pref_greeting_set = switch_core_session_strdup(session, val);
					else if (!strcasecmp(var, "pref_greeting_get") && !profile->api_pref_greeting_get)
						profile->api_pref_greeting_get = switch_core_session_strdup(session, val);
					else if (!strcasecmp(var, "pref_recname_set") && !profile->api_pref_recname_set)
						profile->api_pref_recname_set = switch_core_session_strdup(session, val);
					else if (!strcasecmp(var, "pref_password_set") && !profile->api_pref_password_set)
						profile->api_pref_password_set = switch_core_session_strdup(session, val);
					else if (!strcasecmp(var, "auth_login") && !profile->api_auth_login)
						profile->api_auth_login = switch_core_session_strdup(session, val);
					else
						total_invalid_options++;
					total_options++;
				}
			}
			if (total_options - total_invalid_options != 13) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Missing api definition for profile '%s'\n", profile_name);
				profile = NULL;
			}
		}

	}

end:
	switch_xml_free(xml);
	return profile;
}

void free_profile(vmivr_profile_t *profile) {
	if (profile->event_settings) {
		switch_event_destroy(&profile->event_settings);
	}
}
