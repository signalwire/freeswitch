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
 * mod_dialplan_xml.c -- XML/Regex Dialplan Module
 *
 */
#include <switch.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

SWITCH_MODULE_LOAD_FUNCTION(mod_dialplan_xml_load);
SWITCH_MODULE_DEFINITION(mod_dialplan_xml, mod_dialplan_xml_load, NULL, NULL);

typedef enum {
	BREAK_ON_TRUE,
	BREAK_ON_FALSE,
	BREAK_ALWAYS,
	BREAK_NEVER
} break_t;

static int parse_exten(switch_core_session_t *session, switch_caller_profile_t *caller_profile, switch_xml_t xexten, switch_caller_extension_t **extension)
{
	switch_xml_t xcond, xaction;
	switch_channel_t *channel;
	char *exten_name = (char *) switch_xml_attr_soft(xexten, "name");
	int proceed = 0;
	char *expression_expanded = NULL, *field_expanded = NULL;


	channel = switch_core_session_get_channel(session);

	for (xcond = switch_xml_child(xexten, "condition"); xcond; xcond = xcond->next) {
		char *field = NULL;
		char *do_break_a = NULL;
		char *expression = NULL;
		const char *field_data = NULL;
		switch_regex_t *re = NULL;
		int ovector[30];
		break_t do_break_i = BREAK_ON_FALSE;

		switch_safe_free(field_expanded);
		switch_safe_free(expression_expanded);

		field = (char *) switch_xml_attr(xcond, "field");

		expression = (char *) switch_xml_attr_soft(xcond, "expression");
		
		if ((expression_expanded = switch_channel_expand_variables(channel, expression)) == expression) {
			expression_expanded = NULL;
		} else {
			expression = expression_expanded;
		}

		if ((do_break_a = (char *) switch_xml_attr(xcond, "break"))) {
			if (!strcasecmp(do_break_a, "on-true")) {
				do_break_i = BREAK_ON_TRUE;
			} else if (!strcasecmp(do_break_a, "on-false")) {
				do_break_i = BREAK_ON_FALSE;
			} else if (!strcasecmp(do_break_a, "always")) {
				do_break_i = BREAK_ALWAYS;
			} else if (!strcasecmp(do_break_a, "never")) {
				do_break_i = BREAK_NEVER;
			}
		}

		if (field) {
			if (strchr(field, '$')) {
				if ((field_expanded = switch_channel_expand_variables(channel, field)) == field) {
					field_expanded = NULL;
					field_data = field;
				} else {
					field_data = field_expanded;
				}
			} else {
				field_data = switch_caller_get_field_by_name(caller_profile, field);
			}
			if (!field_data) {
				field_data = "";
			}
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "test conditions %s(%s) =~ /%s/\n", field, field_data, expression);
			if (!(proceed = switch_regex_perform(field_data, expression, &re, ovector, sizeof(ovector) / sizeof(ovector[0])))) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Regex mismatch\n");

				for (xaction = switch_xml_child(xcond, "anti-action"); xaction; xaction = xaction->next) {
					char *application = (char *) switch_xml_attr_soft(xaction, "application");
					char *data = (char *) switch_xml_attr_soft(xaction, "data");

					if (!*extension) {
						if ((*extension = switch_caller_extension_new(session, exten_name, caller_profile->destination_number)) == 0) {
							switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "memory error!\n");
							proceed = 0;
							goto done;
						}
					}

					switch_caller_extension_add_application(session, *extension, application, data);
				}

				if (do_break_i == BREAK_ON_FALSE || do_break_i == BREAK_ALWAYS) {
					break;
				} else {
					continue;
				}
			}
			assert(re != NULL);
		}


		for (xaction = switch_xml_child(xcond, "action"); xaction; xaction = xaction->next) {
			char *application = (char *) switch_xml_attr_soft(xaction, "application");
			char *data = NULL;
			char *substituted = NULL;
			uint32_t len = 0;
			char *app_data = NULL;

			if (xaction->txt) {
				data = xaction->txt;
			} else {
				data = (char *) switch_xml_attr_soft(xaction, "data");
			}

			if (field && strchr(expression, '(')) {
				len = (uint32_t) (strlen(data) + strlen(field_data) + 10);
				if (!(substituted = malloc(len))) {
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "memory error!\n");
					proceed = 0;
					goto done;
				}
				memset(substituted, 0, len);
				switch_perform_substitution(re, proceed, data, field_data, substituted, len, ovector);
				app_data = substituted;
			} else {
				app_data = data;
			}

			if (!*extension) {
				if ((*extension = switch_caller_extension_new(session, exten_name, caller_profile->destination_number)) == 0) {
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "memory error!\n");
					proceed = 0;
					goto done;
				}
			}

			switch_caller_extension_add_application(session, *extension, application, app_data);
			switch_safe_free(substituted);
		}

		switch_regex_safe_free(re);

		if (do_break_i == BREAK_ON_TRUE || do_break_i == BREAK_ALWAYS) {
			break;
		}
	}

  done:
	switch_safe_free(field_expanded);
	switch_safe_free(expression_expanded);
	return proceed;
}

static switch_status_t dialplan_xml_locate(switch_core_session_t *session, switch_caller_profile_t *caller_profile, switch_xml_t * root,
										   switch_xml_t * node)
{
	switch_channel_t *channel;
	switch_status_t status = SWITCH_STATUS_GENERR;
	switch_event_t *params = NULL;

	channel = switch_core_session_get_channel(session);
	switch_event_create(&params, SWITCH_EVENT_MESSAGE);
    switch_assert(params);

	switch_channel_event_set_data(channel, params);

	status = switch_xml_locate("dialplan", NULL, NULL, NULL, root, node, params);
	switch_event_destroy(&params);
	return status;
}

SWITCH_STANDARD_DIALPLAN(dialplan_hunt)
{
	switch_caller_extension_t *extension = NULL;
	switch_channel_t *channel;
	switch_xml_t alt_root = NULL, cfg, xml = NULL, xcontext, xexten;
	char *alt_path = (char *) arg;
	
	channel = switch_core_session_get_channel(session);

	if (!caller_profile) {
		if (!(caller_profile = switch_channel_get_caller_profile(channel))) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Error Obtaining Profile!\n");
			goto done;
		}
	}
	
	if (!caller_profile->context) {
		caller_profile->context = "default";
	}
	

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "Processing %s->%s!\n", caller_profile->caller_id_name, caller_profile->destination_number);

	/* get our handle to the "dialplan" section of the config */

	if (!switch_strlen_zero(alt_path)) {
		switch_xml_t conf = NULL, tag = NULL;
		if (!(alt_root = switch_xml_parse_file_simple(alt_path))) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "open of [%s] failed\n", alt_path);
			goto done;
		}

		if ((conf = switch_xml_find_child(alt_root, "section", "name", "dialplan")) && (tag = switch_xml_find_child(conf, "dialplan", NULL, NULL))) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Getting dialplan from alternate path: %s\n", alt_path);
			xml = alt_root;
			cfg = tag;
		} else {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "open of dialplan failed\n");
			goto done;
		}
	} else {
		if (dialplan_xml_locate(session, caller_profile, &xml, &cfg) != SWITCH_STATUS_SUCCESS) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "open of dialplan failed\n");
			goto done;
		}
	}

	/* get a handle to the context tag */
	if (!(xcontext = switch_xml_find_child(cfg, "context", "name", caller_profile->context))) {
		if (!(xcontext = switch_xml_find_child(cfg, "context", "name", "global"))) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "context %s not found\n", caller_profile->context);
			goto done;
		}
	}

	if (!(xexten = switch_xml_find_child(xcontext, "extension", "name", caller_profile->destination_number))) {
		xexten = switch_xml_child(xcontext, "extension");
	}

	while (xexten) {
		int proceed = 0;
		char *cont = (char *) switch_xml_attr_soft(xexten, "continue");

		proceed = parse_exten(session, caller_profile, xexten, &extension);

		if (proceed && !switch_true(cont)) {
			break;
		}

		xexten = xexten->next;
	}


	switch_xml_free(xml);
	xml = NULL;

	if (extension) {
		switch_channel_set_state(channel, CS_EXECUTE);
	}

  done:
	switch_xml_free(xml);
	return extension;
}

SWITCH_MODULE_LOAD_FUNCTION(mod_dialplan_xml_load)
{
	switch_dialplan_interface_t *dp_interface;

	/* connect my internal structure to the blank pointer passed to me */
	*module_interface = switch_loadable_module_create_module_interface(pool, modname);
	SWITCH_ADD_DIALPLAN(dp_interface, "XML", dialplan_hunt);

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
