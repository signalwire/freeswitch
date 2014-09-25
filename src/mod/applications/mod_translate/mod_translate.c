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
 * Anthony Minessale II <anthm@freeswitch.org>
 * Raymond Chandler <intralanman@freeswitch.org>
 *
 * mod_translate.c -- TRANSLATE
 *
 */

#include <switch.h>

SWITCH_MODULE_LOAD_FUNCTION(mod_translate_load);
SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_translate_shutdown);
SWITCH_MODULE_DEFINITION(mod_translate, mod_translate_load, mod_translate_shutdown, NULL);

static switch_mutex_t *MUTEX = NULL;

static struct {
	switch_memory_pool_t *pool;
	switch_hash_t *translate_profiles;
	switch_thread_rwlock_t *profile_hash_rwlock;
} globals;

struct rule {
	char *regex;
	char *replace;
	struct rule *next;
};
typedef struct rule translate_rule_t;

static switch_event_node_t *NODE = NULL;


static switch_status_t load_config(void)
{
	char *cf = "translate.conf";
	switch_xml_t cfg, xml, rule, profile, profiles;
	switch_status_t status = SWITCH_STATUS_SUCCESS;

	if (!(xml = switch_xml_open_cfg(cf, &cfg, NULL))) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Open of %s failed\n", cf);
		status = SWITCH_STATUS_FALSE;
		goto done;
	}

	if ((profiles = switch_xml_child(cfg, "profiles"))) {
		for (profile = switch_xml_child(profiles, "profile"); profile; profile = profile->next) {
			translate_rule_t *rules_list = NULL;
			char *name = (char *) switch_xml_attr_soft(profile, "name");

			if (!name) {
				continue;
			}

			for (rule = switch_xml_child(profile, "rule"); rule; rule = rule->next) {
				char *regex = (char *) switch_xml_attr_soft(rule, "regex");
				char *replace = (char *) switch_xml_attr_soft(rule, "replace");

				if (regex && replace) {
					translate_rule_t *this_rule = NULL, *rl = NULL;

					this_rule = switch_core_alloc(globals.pool, sizeof(translate_rule_t));
					this_rule->regex = switch_core_strdup(globals.pool, regex);
					this_rule->replace = switch_core_strdup(globals.pool, replace);

					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Replace number matching [%s] with [%s]\n", regex, replace);
					if (rules_list == NULL) {
						switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "starting with an empty list\n");
						rules_list = this_rule;
					} else {
						for (rl = rules_list; rl && rl->next; rl = rl->next);
						rl->next = this_rule;
					}
				} else {
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Invalid Translation!\n");
				}
			}
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Adding rules to profile [%s]\n", name);

			switch_core_hash_insert_wrlock(globals.translate_profiles, name, rules_list, globals.profile_hash_rwlock);
		}
	}

  done:
	if (xml) {
		switch_xml_free(xml);
	}

	return status;
}

static void translate_number(char *number, char *profile, char **translated, switch_core_session_t *session, switch_event_t *event)
{
	translate_rule_t *hi = NULL;
	translate_rule_t *rule = NULL;
	switch_regex_t *re = NULL;
	int proceed = 0, ovector[30];
	char *substituted = NULL, *subbed = NULL;
	uint32_t len = 0;

	if (!profile) {
		profile = "US";
	}

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "translating [%s] against [%s] profile\n", number, profile);

	hi = switch_core_hash_find_rdlock(globals.translate_profiles, (const char *)profile, globals.profile_hash_rwlock);
	if (!hi) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "can't find key for profile matching [%s]\n", profile);
		return;
	}

	for (rule = hi; rule; rule = rule->next) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "%s =~ /%s/\n", number, rule->regex);
		if ((proceed = switch_regex_perform(number, rule->regex, &re, ovector, sizeof(ovector) / sizeof(ovector[0])))) {
			len = (uint32_t) (strlen(number) + strlen(rule->replace) + 10) * proceed;
			if (!(substituted = malloc(len))) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Memory Error!\n");
				switch_regex_safe_free(re);
				goto end;
			}
			memset(substituted, 0, len);
			
			switch_perform_substitution(re, proceed, rule->replace, number, substituted, len, ovector);

			if ((switch_string_var_check_const(substituted) || switch_string_has_escaped_data(substituted))) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "perform variable expansion\n");
				if (session) {
					subbed = switch_channel_expand_variables(switch_core_session_get_channel(session), substituted);
				} else if (event) {
					subbed = switch_event_expand_headers(event, substituted);
				}

				substituted = switch_core_session_strdup(session, subbed);

				if (subbed != substituted) {
					switch_safe_free(subbed);
				}
			}

			break;
		}
	}

 end:
	*translated = substituted ? substituted : NULL;
}


static void do_unload(void) {
	switch_hash_index_t *hi = NULL;

	switch_mutex_lock(MUTEX);

	while ((hi = switch_core_hash_first_iter( globals.translate_profiles, hi))) {
		void *val = NULL;
		const void *key;
		switch_ssize_t keylen;
		translate_rule_t *rl = NULL, *nrl;

		switch_core_hash_this(hi, &key, &keylen, &val);
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "deleting translate profile [%s]\n", (char *) key);

		for (nrl = val; rl;) {
			rl = nrl;
			nrl = nrl->next;
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "deleting rule for [%s]\n", rl->regex);
			switch_safe_free(rl->regex);
			switch_safe_free(rl->replace);
			switch_safe_free(rl);
		}

		switch_core_hash_delete_wrlock(globals.translate_profiles, key, globals.profile_hash_rwlock);
	}

	switch_thread_rwlock_destroy(globals.profile_hash_rwlock);
	switch_core_hash_destroy(&globals.translate_profiles);

	switch_mutex_unlock(MUTEX);
}

static void do_load(void)
{
	switch_mutex_lock(MUTEX);

	switch_core_hash_init(&globals.translate_profiles);
	switch_thread_rwlock_create(&globals.profile_hash_rwlock, globals.pool);
	load_config();

	switch_mutex_unlock(MUTEX);
}

static void event_handler(switch_event_t *event)
{
	switch_mutex_lock(MUTEX);
	do_unload();
	do_load();
	switch_mutex_unlock(MUTEX);
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "Number Translations Reloaded\n");
}

SWITCH_STANDARD_APP(translate_app_function)
{
	int argc = 0;
	char *argv[32] = { 0 };
	char *mydata = NULL;
	char *translated = NULL;
	switch_channel_t *channel = switch_core_session_get_channel(session);
	switch_memory_pool_t *pool = NULL;
	switch_event_t *event = NULL;

	switch_assert(session);

	if (!(mydata = switch_core_session_strdup(session, data))) {
		goto end;
	}

	if ((argc = switch_separate_string(mydata, ' ', argv, (sizeof(argv) / sizeof(argv[0]))))) {
		char *areacode = switch_core_get_variable("default_areacode");

		if (session) {
			pool = switch_core_session_get_pool(session);
		} else {
			switch_core_new_memory_pool(&pool);
			switch_event_create(&event, SWITCH_EVENT_MESSAGE);

			if (zstr(areacode)) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "no default_areacode set, using default of 777\n");
				switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "areacode", "777");
			} else {
				switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "areacode", areacode);
			}
		}

		translate_number(argv[0], argv[1], &translated, session, event);

		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "Translated: %s\n", translated);

		switch_channel_set_variable_var_check(channel, "translated", translated, SWITCH_FALSE);
	}

end:
	if (pool) {
		switch_core_destroy_memory_pool(&pool);
	}

	return;
}

SWITCH_STANDARD_DIALPLAN(translate_dialplan_hunt)
{
	switch_channel_t *channel = switch_core_session_get_channel(session);
	char *translated_dest = NULL;
	char *translated_cid_num = NULL;
	char *translate_profile = NULL;
	char *areacode = NULL;
	switch_event_t *event = NULL;

	if (!caller_profile) {
		if (!(caller_profile = switch_channel_get_caller_profile(channel))) {
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "Error Obtaining Profile!\n");
			goto done;
		}
	}

	switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_INFO, "Processing %s <%s>->%s in translate\n",
					  caller_profile->caller_id_name, caller_profile->caller_id_number, caller_profile->destination_number);

	if ((translate_profile = (char *) switch_channel_get_variable(channel, "translate_profile"))) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "using translate_profile variable [%s] for translate profile\n", translate_profile);
	} else 	if ((translate_profile = (char *) switch_channel_get_variable(channel, "country"))) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "using country variable [%s] for translate profile\n", translate_profile);
	} else if ((translate_profile = (char *) switch_channel_get_variable(channel, "default_country"))) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "using default_country variable [%s] for translate profile\n", translate_profile);
	} else {
		translate_profile = "US";
	}

	areacode = (char *) switch_channel_get_variable(channel, "areacode");
	if (zstr(areacode)) {
		areacode = (char *) switch_channel_get_variable(channel, "default_areacode");
		if (!zstr(areacode)) {
			switch_channel_set_variable_safe(channel, "areacode", areacode);
		}
	}

	translate_number((char *) caller_profile->destination_number, translate_profile, &translated_dest, session, event);
	translate_number((char *) caller_profile->caller_id_number, translate_profile, &translated_cid_num, session, event);
	/* maybe we should translate ani/aniii here too? */
	switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_INFO,
					  "Profile: [%s] Translated Destination: [%s] Translated CID: [%s]\n", translate_profile, translated_dest, translated_cid_num);

	if (!zstr(translated_cid_num)) {
		caller_profile->caller_id_number = translated_cid_num;
	}

	if (!zstr(translated_dest)) {
		caller_profile->destination_number = translated_dest;
	}

 done:
	return NULL;
}

#define TRANSLATE_SYNTAX "translate <number> [<profile>]"
SWITCH_STANDARD_API(translate_function)
{
	char *mydata = NULL;
	switch_memory_pool_t *pool = NULL;
	char *translated = NULL;
	switch_event_t *event = NULL;
	char *argv[32] = { 0 };
	int argc = 0;

	if (zstr(cmd)) {
		goto usage;
	}
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "%s\n", cmd);

	mydata = switch_core_strdup(globals.pool, cmd);

	if ((argc = switch_separate_string(mydata, ' ', argv, (sizeof(argv) / sizeof(argv[0]))))) {
		if (!session) {
			char *areacode = switch_core_get_variable("default_areacode");

			switch_event_create(&event, SWITCH_EVENT_REQUEST_PARAMS);

			if (zstr(areacode)) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "no default_areacode set, using default of 777\n");
				switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "areacode", "777");
			} else {
				switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "areacode", areacode);
			}
		}
		translate_number(argv[0], argv[1], &translated, session, event);

		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "Translated: %s\n", translated);

		stream->write_function(stream, "%s", translated);
	}

end:
	if (!session) {
		if (pool) {
			switch_core_destroy_memory_pool(&pool);
		}
	}
	return SWITCH_STATUS_SUCCESS;

usage:
	stream->write_function(stream, "USAGE: %s\n", TRANSLATE_SYNTAX);
	goto end;
}

SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_translate_shutdown)
{
	switch_event_unbind(&NODE);

	do_unload();

	return SWITCH_STATUS_UNLOAD;
}

SWITCH_MODULE_LOAD_FUNCTION(mod_translate_load)
{
	switch_api_interface_t *api_interface;
	switch_application_interface_t *app_interface;
	switch_dialplan_interface_t *dp_interface;

	memset(&globals, 0, sizeof(globals));
	globals.pool = pool;


	switch_mutex_init(&MUTEX, SWITCH_MUTEX_NESTED, pool);

	if ((switch_event_bind_removable(modname, SWITCH_EVENT_RELOADXML, NULL, event_handler, NULL, &NODE) != SWITCH_STATUS_SUCCESS)) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Couldn't bind!\n");
		return SWITCH_STATUS_TERM;
	}

	do_load();

	*module_interface = switch_loadable_module_create_module_interface(pool, modname);
	SWITCH_ADD_API(api_interface, "translate", "TRANSLATE", translate_function, "");
	SWITCH_ADD_APP(app_interface, "translate", "Perform an TRANSLATE lookup", "Translate a number based on predefined rules", translate_app_function, "<number> <profile>]",
				   SAF_SUPPORT_NOMEDIA | SAF_ROUTING_EXEC);
	SWITCH_ADD_DIALPLAN(dp_interface, "translate", translate_dialplan_hunt);

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
 * vim:set softtabstop=4 shiftwidth=4 tabstop=4 noet:
 */
