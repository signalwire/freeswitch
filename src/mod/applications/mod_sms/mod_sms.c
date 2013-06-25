/* 
 * FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
 * Copyright (C) 2005-2012, Anthony Minessale II <anthm@freeswitch.org>
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
 * mod_sms.c -- Abstract SMS 
 *
 */
#include <switch.h>
#define SMS_CHAT_PROTO "GLOBAL_SMS"
#define MY_EVENT_SEND_MESSAGE "SMS::SEND_MESSAGE"

/* Prototypes */
SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_sms_shutdown);
SWITCH_MODULE_RUNTIME_FUNCTION(mod_sms_runtime);
SWITCH_MODULE_LOAD_FUNCTION(mod_sms_load);
SWITCH_MODULE_DEFINITION(mod_sms, mod_sms_load, mod_sms_shutdown, NULL);


static void event_handler(switch_event_t *event) 
{
	const char *dest_proto = switch_event_get_header(event, "dest_proto");
	const char *check_failure = switch_event_get_header(event, "Delivery-Failure");
	const char *check_nonblocking = switch_event_get_header(event, "Nonblocking-Delivery");

	switch_event_add_header(event, SWITCH_STACK_BOTTOM, "skip_global_process", "true");

	if (switch_true(check_failure)) {

		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Delivery Failure\n");
		DUMP_EVENT(event);

		return;
	} else if ( check_failure && switch_false(check_failure) ) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "SMS Delivery Success\n");
		return;
	} else if ( check_nonblocking && switch_true(check_nonblocking) ) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "SMS Delivery assumed successful due to being sent in non-blocking manner\n");
		return;
	}

	switch_core_chat_send(dest_proto, event);
}

typedef enum {
	BREAK_ON_TRUE,
	BREAK_ON_FALSE,
	BREAK_ALWAYS,
	BREAK_NEVER
} break_t;


#define check_tz()														\
	do {																\
		tzoff = switch_event_get_header(event, "tod_tz_offset");		\
		tzname = switch_event_get_header(event, "timezone");			\
		if (!zstr(tzoff) && switch_is_number(tzoff)) {					\
			offset = atoi(tzoff);										\
			break;														\
		} else {														\
			tzoff = NULL;												\
		}																\
	} while(tzoff)														

static int parse_exten(switch_event_t *event, switch_xml_t xexten, switch_event_t **extension)
{
	switch_xml_t xcond, xaction, xexpression;
	char *exten_name = (char *) switch_xml_attr(xexten, "name");
	int proceed = 0;
	char *expression_expanded = NULL, *field_expanded = NULL;
	switch_regex_t *re = NULL;
	const char *to = switch_event_get_header(event, "to");
	const char *tzoff = NULL, *tzname = NULL;
	int offset = 0;

	check_tz();

	if (!to) {
		to = "nobody";
	}

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
		int time_match;

		check_tz();
		time_match = switch_xml_std_datetime_check(xcond, tzoff ? &offset : NULL, tzname);

		switch_safe_free(field_expanded);
		switch_safe_free(expression_expanded);

		if (switch_xml_child(xcond, "condition")) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Nested conditions are not allowed!\n");
			proceed = 1;
			goto done;
		}

		field = (char *) switch_xml_attr(xcond, "field");

		if ((xexpression = switch_xml_child(xcond, "expression"))) {
			expression = switch_str_nil(xexpression->txt);
		} else {
			expression = (char *) switch_xml_attr_soft(xcond, "expression");
		}

		if ((expression_expanded = switch_event_expand_headers(event, expression)) == expression) {
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
			switch_log_printf(SWITCH_CHANNEL_LOG_CLEAN, SWITCH_LOG_DEBUG,
							  "Chatplan: %s Date/Time Match (PASS) [%s] break=%s\n",
							  to, exten_name, do_break_a ? do_break_a : "on-false");
			anti_action = SWITCH_FALSE;
		} else if (time_match == 0) {
			switch_log_printf(SWITCH_CHANNEL_LOG_CLEAN, SWITCH_LOG_DEBUG,
							  "Chatplan: %s Date/Time Match (FAIL) [%s] break=%s\n",
							  to, exten_name, do_break_a ? do_break_a : "on-false");
		}

		if (field) {
			if (strchr(field, '$')) {
				if ((field_expanded = switch_event_expand_headers(event, field)) == field) {
					field_expanded = NULL;
					field_data = field;
				} else {
					field_data = field_expanded;
				}
			} else {
				field_data = switch_event_get_header(event, field);
			}
			if (!field_data) {
				field_data = "";
			}

			if ((proceed = switch_regex_perform(field_data, expression, &re, ovector, sizeof(ovector) / sizeof(ovector[0])))) {
				switch_log_printf(SWITCH_CHANNEL_LOG_CLEAN, SWITCH_LOG_DEBUG,
								  "Chatplan: %s Regex (PASS) [%s] %s(%s) =~ /%s/ break=%s\n",
								  to, exten_name, field, field_data, expression, do_break_a ? do_break_a : "on-false");
				anti_action = SWITCH_FALSE;
			} else {
				switch_log_printf(SWITCH_CHANNEL_LOG_CLEAN, SWITCH_LOG_DEBUG,
								  "Chatplan: %s Regex (FAIL) [%s] %s(%s) =~ /%s/ break=%s\n",
								  to, exten_name, field, field_data, expression, do_break_a ? do_break_a : "on-false");
			}
		} else if (time_match == -1) {
			switch_log_printf(SWITCH_CHANNEL_LOG_CLEAN, SWITCH_LOG_DEBUG,
							  "Chatplan: %s Absolute Condition [%s]\n", to, exten_name);
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
					if ((switch_event_create(extension, SWITCH_EVENT_CLONE)) != SWITCH_STATUS_SUCCESS) {
						switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Memory Error!\n");
						abort();
					}
				}

				if (loop) {
					loop_count = atoi(loop);
				}

				for (;loop_count > 0; loop_count--) {
					switch_log_printf(SWITCH_CHANNEL_LOG_CLEAN, SWITCH_LOG_DEBUG,
							"Chatplan: %s ANTI-Action %s(%s) %s\n", to, application, data, xinline ? "INLINE" : "");

					if (xinline) {
						switch_core_execute_chat_app(event, application, data);
					} else {
						switch_event_add_header_string(*extension, SWITCH_STACK_BOTTOM, application, zstr(data) ? "__undef" : data);
					}
				}
				proceed = 1;
			}
		} else {
			if (field && strchr(expression, '(')) {
				switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "DP_MATCH", NULL);
				switch_capture_regex(re, proceed, field_data, ovector, "DP_MATCH", switch_regex_set_event_header_callback, event);
			}

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
					if (!(substituted = (char *) malloc(len))) {
						abort();
					}
					memset(substituted, 0, len);
					switch_perform_substitution(re, proceed, data, field_data, substituted, len, ovector);
					app_data = substituted;
				} else {
					app_data = data;
				}

				if (!*extension) {
					if ((switch_event_create(extension, SWITCH_EVENT_CLONE)) != SWITCH_STATUS_SUCCESS) {
						switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Memory Error!\n");
						abort();
					}
				}

				if (loop) {
					loop_count = atoi(loop);
				}
				for (;loop_count > 0; loop_count--) {
					switch_log_printf(SWITCH_CHANNEL_LOG_CLEAN, SWITCH_LOG_DEBUG,
							"Chatplan: %s Action %s(%s) %s\n", to, application, app_data, xinline ? "INLINE" : "");

					if (xinline) {
						switch_core_execute_chat_app(event, application, app_data);
					} else {
						switch_event_add_header_string(*extension, SWITCH_STACK_BOTTOM, application, zstr(app_data) ? "__undef" : app_data);
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


static switch_event_t *chatplan_hunt(switch_event_t *event)
{
	switch_event_t *extension = NULL;
	switch_xml_t alt_root = NULL, cfg, xml = NULL, xcontext, xexten = NULL;
	const char *alt_path;
	const char *context;
	const char *from;
	const char *to;

	if (!(context = switch_event_get_header(event, "context"))) {
		context = "default";
	}

	if (!(from = switch_event_get_header(event, "from_user"))) {
		from = switch_event_get_header(event, "from");
	}

	if (!(to = switch_event_get_header(event, "to_user"))) {
		to = switch_event_get_header(event, "to");
	}

	alt_path = switch_event_get_header(event, "alt_path");

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "Processing text message %s->%s in context %s\n", from, to, context);

	/* get our handle to the "chatplan" section of the config */

	if (!zstr(alt_path)) {
		switch_xml_t conf = NULL, tag = NULL;
		if (!(alt_root = switch_xml_parse_file_simple(alt_path))) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Open of [%s] failed\n", alt_path);
			goto done;
		}

		if ((conf = switch_xml_find_child(alt_root, "section", "name", "chatplan")) && (tag = switch_xml_find_child(conf, "chatplan", NULL, NULL))) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Getting chatplan from alternate path: %s\n", alt_path);
			xml = alt_root;
			cfg = tag;
		} else {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Open of chatplan failed\n");
			goto done;
		}
	} else {
		if (switch_xml_locate("chatplan", NULL, NULL, NULL, &xml, &cfg, event, SWITCH_FALSE) != SWITCH_STATUS_SUCCESS) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Open of chatplan failed\n");
			goto done;
		}
	}

	/* get a handle to the context tag */
	if (!(xcontext = switch_xml_find_child(cfg, "context", "name", context))) {
		if (!(xcontext = switch_xml_find_child(cfg, "context", "name", "global"))) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "Context %s not found\n", context);
			goto done;
		}
	}

	xexten = switch_xml_child(xcontext, "extension");
	
	while (xexten) {
		int proceed = 0;
		const char *cont = switch_xml_attr(xexten, "continue");
		const char *exten_name = switch_xml_attr(xexten, "name");

		if (!exten_name) {
			exten_name = "UNKNOWN";
		}

		switch_log_printf(SWITCH_CHANNEL_LOG_CLEAN, SWITCH_LOG_DEBUG,
						  "Chatplan: %s parsing [%s->%s] continue=%s\n",
						  to, context, exten_name, cont ? cont : "false");

		proceed = parse_exten(event, xexten, &extension);
		
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


static switch_status_t chat_send(switch_event_t *message_event)
								 {
	switch_status_t status = SWITCH_STATUS_BREAK;
	switch_event_t *exten;
	int forwards = 0;
	const char *var;

	var = switch_event_get_header(message_event, "max_forwards");

	if (!var) {
		forwards = 70;
	} else {
		forwards = atoi(var);
		
		if (forwards) {
			forwards--;
		}

		if (!forwards) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Max forwards reached\n");
			DUMP_EVENT(message_event);
			return SWITCH_STATUS_FALSE;
		}
	}

	if (forwards) {
		switch_event_add_header(message_event, SWITCH_STACK_BOTTOM, "max_forwards", "%d", forwards);
	}

	if ((exten = chatplan_hunt(message_event))) {
		switch_event_header_t *hp;
		
		for (hp = exten->headers; hp; hp = hp->next) {
			status = switch_core_execute_chat_app(message_event, hp->name, hp->value);
			if (!SWITCH_READ_ACCEPTABLE(status)) {
				status = SWITCH_STATUS_SUCCESS;	
				break;
			}
		}

		switch_event_destroy(&exten);
	} else {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "SMS chatplan no actions found\n");
	}

	return status;

}

SWITCH_STANDARD_CHAT_APP(system_function)
{
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "Executing command: %s\n", data);
	if (switch_system(data, SWITCH_TRUE) < 0) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "Failed to execute command: %s\n", data);
		return SWITCH_STATUS_FALSE;
	}
	return SWITCH_STATUS_SUCCESS;
}

SWITCH_STANDARD_CHAT_APP(stop_function)
{
	switch_set_flag(message, EF_NO_CHAT_EXEC);
	return SWITCH_STATUS_FALSE;
}

SWITCH_STANDARD_CHAT_APP(send_function)
{
	const char *dest_proto = data;

	if (zstr(dest_proto)) {
		dest_proto = switch_event_get_header(message, "dest_proto");
	}

	switch_event_add_header(message, SWITCH_STACK_BOTTOM, "skip_global_process", "true");

	switch_core_chat_send(dest_proto, message);

	return SWITCH_STATUS_SUCCESS;
}

SWITCH_STANDARD_CHAT_APP(set_function)
{
	char *var, *val;

	if (data) {
		var = strdup(data);
		if ((val = strchr(var, '='))) {
			*val++ = '\0';
		}
 
		if (zstr(val)) {
			switch_event_del_header(message, var);
		} else {
			switch_event_add_header_string(message, SWITCH_STACK_BOTTOM, var, val);
		}
	}

	return SWITCH_STATUS_SUCCESS;
}

SWITCH_STANDARD_CHAT_APP(fire_function)
{
	switch_event_t *fireme;

	switch_event_dup(&fireme, message);
	switch_event_fire(&fireme);

	return SWITCH_STATUS_SUCCESS;
}

SWITCH_STANDARD_CHAT_APP(reply_function)
{
	switch_event_t *reply;
	const char *proto = switch_event_get_header(message, "proto");

	if (proto) {
		switch_ivr_create_message_reply(&reply, message, SMS_CHAT_PROTO);

		if (!zstr(data)) {
			switch_event_set_body(reply, data);
		}

		switch_core_chat_deliver(proto, &reply);

		return SWITCH_STATUS_SUCCESS;
	}

	return SWITCH_STATUS_SUCCESS;
}

/* Macro expands to: switch_status_t mod_sms_load(switch_loadable_module_interface_t **module_interface, switch_memory_pool_t *pool) */
SWITCH_MODULE_LOAD_FUNCTION(mod_sms_load)
{
	switch_chat_interface_t *chat_interface;
	switch_chat_application_interface_t *chat_app_interface;


	if (switch_event_bind(modname, SWITCH_EVENT_CUSTOM, MY_EVENT_SEND_MESSAGE, event_handler, NULL) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Couldn't bind!\n");
		return SWITCH_STATUS_GENERR;
	}
	
	/* connect my internal structure to the blank pointer passed to me */
	*module_interface = switch_loadable_module_create_module_interface(pool, modname);

	SWITCH_ADD_CHAT(chat_interface, SMS_CHAT_PROTO, chat_send);

	SWITCH_ADD_CHAT_APP(chat_app_interface, "reply", "reply to a message", "reply to a message", reply_function, "", SCAF_NONE);
	SWITCH_ADD_CHAT_APP(chat_app_interface, "stop", "stop execution", "stop execution", stop_function, "", SCAF_NONE);
	SWITCH_ADD_CHAT_APP(chat_app_interface, "set", "set a variable", "set a variable", set_function, "", SCAF_NONE);
	SWITCH_ADD_CHAT_APP(chat_app_interface, "send", "send the message as-is", "send the message as-is", send_function, "", SCAF_NONE);
	SWITCH_ADD_CHAT_APP(chat_app_interface, "fire", "fire the message", "fire the message", fire_function, "", SCAF_NONE);
	SWITCH_ADD_CHAT_APP(chat_app_interface, "system", "execute a system command", "execute a sytem command", system_function, "", SCAF_NONE);
						
	/* indicate that the module should continue to be loaded */
	return SWITCH_STATUS_SUCCESS;
}

/*
  Called when the system shuts down
  Macro expands to: switch_status_t mod_sms_shutdown() */
SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_sms_shutdown)
{
	switch_event_unbind_callback(event_handler);

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
 * vim:set softtabstop=4 shiftwidth=4 tabstop=4 noet
 */
