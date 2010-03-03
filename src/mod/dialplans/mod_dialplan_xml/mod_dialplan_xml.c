/* 
 * FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
 * Copyright (C) 2005-2010, Anthony Minessale II <anthm@freeswitch.org>
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


static switch_status_t exec_app(switch_core_session_t *session, const char *app, const char *arg)
{
	switch_application_interface_t *application_interface;
	switch_status_t status = SWITCH_STATUS_SUCCESS;
	switch_channel_t *channel = switch_core_session_get_channel(session);

	switch_assert(channel);

	if ((application_interface = switch_loadable_module_get_application_interface(app)) == 0) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "Invalid Application %s\n", app);
		switch_channel_hangup(channel, SWITCH_CAUSE_DESTINATION_OUT_OF_ORDER);
		return SWITCH_STATUS_FALSE;
	}

	if (!application_interface->application_function) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "No Function for %s\n", app);
		switch_channel_hangup(channel, SWITCH_CAUSE_DESTINATION_OUT_OF_ORDER);
		switch_goto_status(SWITCH_STATUS_FALSE, done);
	}

	if (!switch_test_flag(application_interface, SAF_ROUTING_EXEC)) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "This application cannot be executed inline\n");
		switch_goto_status(SWITCH_STATUS_FALSE, done);
	}

	switch_core_session_exec(session, application_interface, arg);

  done:

	UNPROTECT_INTERFACE(application_interface);

	return status;
}

static int parse_exten(switch_core_session_t *session, switch_caller_profile_t *caller_profile, switch_xml_t xexten, switch_caller_extension_t **extension)
{
	switch_xml_t xcond, xaction, xexpression;
	switch_channel_t *channel = switch_core_session_get_channel(session);
	char *exten_name = (char *) switch_xml_attr(xexten, "name");
	int proceed = 0;
	char *expression_expanded = NULL, *field_expanded = NULL;
	switch_regex_t *re = NULL;

	if (!exten_name) {
		exten_name = "_anon_";
	}

	for (xcond = switch_xml_child(xexten, "condition"); xcond; xcond = xcond->next) {
		char *field = NULL;
		char *do_break_a = NULL;
		char *expression = NULL;
		const char *field_data = NULL;
		int ovector[30];
		switch_bool_t anti_action = SWITCH_TRUE;
		break_t do_break_i = BREAK_ON_FALSE;

		const char *xyear = switch_xml_attr(xcond, "year");
		const char *xyday = switch_xml_attr(xcond, "yday");
		const char *xmon = switch_xml_attr(xcond, "mon");
		const char *xmday = switch_xml_attr(xcond, "mday");
		const char *xweek = switch_xml_attr(xcond, "week");
		const char *xmweek = switch_xml_attr(xcond, "mweek");
		const char *xwday = switch_xml_attr(xcond, "wday");
		const char *xhour = switch_xml_attr(xcond, "hour");
		const char *xminute = switch_xml_attr(xcond, "minute");
		const char *xminday = switch_xml_attr(xcond, "minute-of-day");
		switch_time_t ts = switch_micro_time_now();
		int time_match = -1;
		switch_time_exp_t tm;

		switch_safe_free(field_expanded);
		switch_safe_free(expression_expanded);

		if (switch_xml_child(xcond, "condition")) {
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "Nested conditions are not allowed!\n");
			proceed = 1;
			goto done;
		}

		switch_time_exp_lt(&tm, ts);

		if (time_match && xyear) {
			int test = tm.tm_year + 1900;
			time_match = switch_number_cmp(xyear, test);
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG_CLEAN(session), SWITCH_LOG_DEBUG,
							  "Dialplan: year[%d] =~ %s (%s)\n", test, xyear, time_match ? "PASS" : "FAIL");
		}

		if (time_match && xyday) {
			int test = tm.tm_yday + 1;
			time_match = switch_number_cmp(xyday, test);
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG_CLEAN(session), SWITCH_LOG_DEBUG,
							  "Dialplan: day of year[%d] =~ %s (%s)\n", test, xyday, time_match ? "PASS" : "FAIL");
		}

		if (time_match && xmon) {
			int test = tm.tm_mon + 1;
			time_match = switch_number_cmp(xmon, test);
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG_CLEAN(session), SWITCH_LOG_DEBUG,
							  "Dialplan: month[%d] =~ %s (%s)\n", test, xmon, time_match ? "PASS" : "FAIL");
		}

		if (time_match && xmday) {
			int test = tm.tm_mday;
			time_match = switch_number_cmp(xmday, test);
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG_CLEAN(session), SWITCH_LOG_DEBUG,
							  "Dialplan: day of month[%d] =~ %s (%s)\n", test, xmday, time_match ? "PASS" : "FAIL");
		}

		if (time_match && xweek) {
			int test = (int) (tm.tm_yday / 7 + 1);
			time_match = switch_number_cmp(xweek, test);
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG_CLEAN(session), SWITCH_LOG_DEBUG,
							  "Dialplan: week of year[%d] =~ %s (%s)\n", test, xweek, time_match ? "PASS" : "FAIL");
		}

		if (time_match && xmweek) {
			/* calculate the day of the week of the first of the month (0-6) */
			int firstdow = (int) (7 - (tm.tm_mday - (tm.tm_wday + 1)) % 7) % 7;
			/* calculate the week of the month (1-6) */
			int test = (int) ceil((tm.tm_mday + firstdow) / 7.0);
			time_match = switch_number_cmp(xmweek, test);
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG_CLEAN(session), SWITCH_LOG_DEBUG,
							  "Dialplan: week of month[%d] =~ %s (%s)\n", test, xmweek, time_match ? "PASS" : "FAIL");
		}

		if (time_match && xwday) {
			int test = tm.tm_wday + 1;
			time_match = switch_number_cmp(xwday, test);
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG_CLEAN(session), SWITCH_LOG_DEBUG,
							  "Dialplan: day of week[%d] =~ %s (%s)\n", test, xwday, time_match ? "PASS" : "FAIL");
		}

		if (time_match && xhour) {
			int test = tm.tm_hour;
			time_match = switch_number_cmp(xhour, test);
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG_CLEAN(session), SWITCH_LOG_DEBUG,
							  "Dialplan: hour[%d] =~ %s (%s)\n", test, xhour, time_match ? "PASS" : "FAIL");
		}

		if (time_match && xminute) {
			int test = tm.tm_min;
			time_match = switch_number_cmp(xminute, test);
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG_CLEAN(session), SWITCH_LOG_DEBUG,
							  "Dialplan: minute[%d] =~ %s (%s)\n", test, xminute, time_match ? "PASS" : "FAIL");
		}

		if (time_match && xminday) {
			int test = (tm.tm_hour * 60) + (tm.tm_min + 1);
			time_match = switch_number_cmp(xminday, test);
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG_CLEAN(session), SWITCH_LOG_DEBUG,
							  "Dialplan: minute of day[%d] =~ %s (%s)\n", test, xminday, time_match ? "PASS" : "FAIL");
		}

		field = (char *) switch_xml_attr(xcond, "field");

		if ((xexpression = switch_xml_child(xcond, "expression"))) {
			expression = switch_str_nil(xexpression->txt);
		} else {
			expression = (char *) switch_xml_attr_soft(xcond, "expression");
		}

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
			} else {
				do_break_a = NULL;
			}
		}

		if (time_match == 1) {
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG_CLEAN(session), SWITCH_LOG_DEBUG,
							  "Dialplan: %s Date/Time Match (PASS) [%s] break=%s\n",
							  switch_channel_get_name(channel), exten_name, do_break_a ? do_break_a : "on-false");
			anti_action = SWITCH_FALSE;
		} else if (time_match == 0) {
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG_CLEAN(session), SWITCH_LOG_DEBUG,
							  "Dialplan: %s Date/Time Match (FAIL) [%s] break=%s\n",
							  switch_channel_get_name(channel), exten_name, do_break_a ? do_break_a : "on-false");
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

			if ((proceed = switch_regex_perform(field_data, expression, &re, ovector, sizeof(ovector) / sizeof(ovector[0])))) {
				switch_log_printf(SWITCH_CHANNEL_SESSION_LOG_CLEAN(session), SWITCH_LOG_DEBUG,
								  "Dialplan: %s Regex (PASS) [%s] %s(%s) =~ /%s/ break=%s\n",
								  switch_channel_get_name(channel), exten_name, field, field_data, expression, do_break_a ? do_break_a : "on-false");
				anti_action = SWITCH_FALSE;
			} else {
				switch_log_printf(SWITCH_CHANNEL_SESSION_LOG_CLEAN(session), SWITCH_LOG_DEBUG,
								  "Dialplan: %s Regex (FAIL) [%s] %s(%s) =~ /%s/ break=%s\n",
								  switch_channel_get_name(channel), exten_name, field, field_data, expression, do_break_a ? do_break_a : "on-false");
			}
		} else if (time_match == -1) {
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG_CLEAN(session), SWITCH_LOG_DEBUG,
							  "Dialplan: %s Absolute Condition [%s]\n", switch_channel_get_name(channel), exten_name);
			anti_action = SWITCH_FALSE;
		}

		if (anti_action) {
			for (xaction = switch_xml_child(xcond, "anti-action"); xaction; xaction = xaction->next) {
				const char *application = switch_xml_attr_soft(xaction, "application");
				const char *loop = switch_xml_attr(xaction, "loop");
				const char *data;
				const char *inline_ = switch_xml_attr_soft(xaction, "inline");
				int xinline = switch_true(inline_);
				int loop_count = 1;

				if (!zstr(xaction->txt)) {
					data = xaction->txt;
				} else {
					data = (char *) switch_xml_attr_soft(xaction, "data");
				}

				if (!*extension) {
					if ((*extension = switch_caller_extension_new(session, exten_name, caller_profile->destination_number)) == 0) {
						switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_CRIT, "Memory Error!\n");
						proceed = 0;
						goto done;
					}
				}

				if (loop) {
					loop_count = atoi(loop);
				}

				for (;loop_count > 0; loop_count--) {
					switch_log_printf(SWITCH_CHANNEL_SESSION_LOG_CLEAN(session), SWITCH_LOG_DEBUG,
							"Dialplan: %s ANTI-Action %s(%s) %s\n", switch_channel_get_name(channel), application, data, xinline ? "INLINE" : "");

					if (xinline) {
						exec_app(session, application, data);
					} else {
						switch_caller_extension_add_application(session, *extension, application, data);
					}
				}
				proceed = 1;
			}
		} else {
			for (xaction = switch_xml_child(xcond, "action"); xaction; xaction = xaction->next) {
				char *application = (char *) switch_xml_attr_soft(xaction, "application");
				const char *loop = switch_xml_attr(xaction, "loop");
				char *data = NULL;
				char *substituted = NULL;
				uint32_t len = 0;
				char *app_data = NULL;
				const char *inline_ = switch_xml_attr_soft(xaction, "inline");
				int xinline = switch_true(inline_);
				int loop_count = 1;

				if (!zstr(xaction->txt)) {
					data = xaction->txt;
				} else {
					data = (char *) switch_xml_attr_soft(xaction, "data");
				}

				if (field && strchr(expression, '(')) {
					len = (uint32_t) (strlen(data) + strlen(field_data) + 10) * proceed;
					if (!(substituted = malloc(len))) {
						switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_CRIT, "Memory Error!\n");
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
						switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_CRIT, "Memory Error!\n");
						proceed = 0;
						goto done;
					}
				}

				if (loop) {
					loop_count = atoi(loop);
				}
				for (;loop_count > 0; loop_count--) {
					switch_log_printf(SWITCH_CHANNEL_SESSION_LOG_CLEAN(session), SWITCH_LOG_DEBUG,
							"Dialplan: %s Action %s(%s) %s\n", switch_channel_get_name(channel), application, app_data, xinline ? "INLINE" : "");

					if (xinline) {
						exec_app(session, application, app_data);
					} else {
						switch_caller_extension_add_application(session, *extension, application, app_data);
					}
				}
				switch_safe_free(substituted);
			}
		}
		switch_regex_safe_free(re);

		if (((anti_action == SWITCH_FALSE && do_break_i == BREAK_ON_TRUE) ||
			 (anti_action == SWITCH_TRUE && do_break_i == BREAK_ON_FALSE)) || do_break_i == BREAK_ALWAYS) {
			break;
		}
	}

  done:
	switch_regex_safe_free(re);
	switch_safe_free(field_expanded);
	switch_safe_free(expression_expanded);
	return proceed;
}

static switch_status_t dialplan_xml_locate(switch_core_session_t *session, switch_caller_profile_t *caller_profile, switch_xml_t *root,
										   switch_xml_t *node)
{
	switch_channel_t *channel = switch_core_session_get_channel(session);
	switch_status_t status = SWITCH_STATUS_GENERR;
	switch_event_t *params = NULL;

	switch_event_create(&params, SWITCH_EVENT_REQUEST_PARAMS);
	switch_assert(params);

	switch_channel_event_set_data(channel, params);
	switch_caller_profile_event_set_data(caller_profile, "Hunt", params);
	status = switch_xml_locate("dialplan", NULL, NULL, NULL, root, node, params, SWITCH_FALSE);
	switch_event_destroy(&params);
	return status;
}

SWITCH_STANDARD_DIALPLAN(dialplan_hunt)
{
	switch_caller_extension_t *extension = NULL;
	switch_channel_t *channel = switch_core_session_get_channel(session);
	switch_xml_t alt_root = NULL, cfg, xml = NULL, xcontext, xexten = NULL;
	char *alt_path = (char *) arg;
	const char *hunt = NULL;

	if (!caller_profile) {
		if (!(caller_profile = switch_channel_get_caller_profile(channel))) {
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "Error Obtaining Profile!\n");
			goto done;
		}
	}

	if (!caller_profile->context) {
		caller_profile->context = "default";
	}

	switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_INFO, "Processing %s->%s in context %s\n",
					  caller_profile->caller_id_name, caller_profile->destination_number, caller_profile->context);

	/* get our handle to the "dialplan" section of the config */

	if (!zstr(alt_path)) {
		switch_xml_t conf = NULL, tag = NULL;
		if (!(alt_root = switch_xml_parse_file_simple(alt_path))) {
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "Open of [%s] failed\n", alt_path);
			goto done;
		}

		if ((conf = switch_xml_find_child(alt_root, "section", "name", "dialplan")) && (tag = switch_xml_find_child(conf, "dialplan", NULL, NULL))) {
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "Getting dialplan from alternate path: %s\n", alt_path);
			xml = alt_root;
			cfg = tag;
		} else {
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "Open of dialplan failed\n");
			goto done;
		}
	} else {
		if (dialplan_xml_locate(session, caller_profile, &xml, &cfg) != SWITCH_STATUS_SUCCESS) {
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "Open of dialplan failed\n");
			goto done;
		}
	}

	/* get a handle to the context tag */
	if (!(xcontext = switch_xml_find_child(cfg, "context", "name", caller_profile->context))) {
		if (!(xcontext = switch_xml_find_child(cfg, "context", "name", "global"))) {
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_WARNING, "Context %s not found\n", caller_profile->context);
			goto done;
		}
	}

	if ((hunt = switch_channel_get_variable(channel, "auto_hunt")) && switch_true(hunt)) {
		xexten = switch_xml_find_child(xcontext, "extension", "name", caller_profile->destination_number);
	}

	if (!xexten) {
		xexten = switch_xml_child(xcontext, "extension");
	}

	while (xexten) {
		int proceed = 0;
		const char *cont = switch_xml_attr(xexten, "continue");
		const char *exten_name = switch_xml_attr(xexten, "name");

		if (!exten_name) {
			exten_name = "UNKNOWN";
		}

		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG_CLEAN(session), SWITCH_LOG_DEBUG,
						  "Dialplan: %s parsing [%s->%s] continue=%s\n",
						  switch_channel_get_name(channel), caller_profile->context, exten_name, cont ? cont : "false");

		proceed = parse_exten(session, caller_profile, xexten, &extension);

		if (proceed && !switch_true(cont)) {
			break;
		}

		xexten = xexten->next;
	}

	switch_xml_free(xml);
	xml = NULL;

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
 * vim:set softtabstop=4 shiftwidth=4 tabstop=4:
 */
