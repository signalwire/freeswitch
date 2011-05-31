/* Copy paste from FS mod_voicemail */
#include <switch.h>

#include "config.h"

const char *global_cf = "protovm.conf";

void populate_profile_menu_event(vmivr_profile_t *profile, vmivr_menu_profile_t *menu) {
	switch_xml_t cfg, xml, x_profiles, x_profile, x_keys, x_phrases, x_menus, x_menu;

	free_profile_menu_event(menu);

	if (!(xml = switch_xml_open_cfg(global_cf, &cfg, NULL))) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Open of %s failed\n", global_cf);
		goto end;
	}
	if (!(x_profiles = switch_xml_child(cfg, "profiles"))) {
		goto end;
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
			}
		}
	}
end:
	if (xml)
		switch_xml_free(xml);
	return;

}

void free_profile_menu_event(vmivr_menu_profile_t *menu) {
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
}

vmivr_profile_t *get_profile(switch_core_session_t *session, const char *profile_name)
{
	vmivr_profile_t *profile = NULL;
	switch_xml_t cfg, xml, x_profiles, x_profile, x_apis, param;

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

		/* TODO Make the following configurable */
		profile->api_profile = profile->name;
		profile->menu_check_auth = "std_authenticate";
		profile->menu_check_main = "std_navigator";
		profile->menu_check_terminate = "std_purge";

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
					else if (!strcasecmp(var, "pref_greeting_set") && !profile->api_pref_greeting_set)
						profile->api_pref_greeting_set = switch_core_session_strdup(session, val);
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
			if (total_options - total_invalid_options != 11) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Missing api definition for profile '%s'\n", profile_name);
				profile = NULL;
			}
		}

	}

end:
	switch_xml_free(xml);
	return profile;
}

