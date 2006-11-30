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


static const char modname[] = "mod_dialplan_xml";



typedef enum {
	BREAK_ON_TRUE,
	BREAK_ON_FALSE,
	BREAK_ALWAYS,
	BREAK_NEVER
} break_t;

static int parse_exten(switch_core_session_t *session, switch_xml_t xexten, switch_caller_extension_t **extension)
{
	switch_xml_t xcond, xaction;
	switch_caller_profile_t *caller_profile;
	switch_channel_t *channel;
	char *exten_name = (char *) switch_xml_attr_soft(xexten, "name");
	int proceed = 0;

	channel = switch_core_session_get_channel(session);
	caller_profile = switch_channel_get_caller_profile(channel);

	for (xcond = switch_xml_child(xexten, "condition"); xcond; xcond = xcond->next) {
		char *field = NULL;
		char *do_break_a = NULL;
		char *expression = NULL;
		char *field_data = NULL;
		char retbuf[1024] = "";
		pcre *re = NULL;
		int ovector[30];
		break_t do_break_i = BREAK_ON_FALSE;
		
		field = (char *) switch_xml_attr(xcond, "field");

		expression = (char *) switch_xml_attr_soft(xcond, "expression");

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
			if (*field == '$') {
				field_data = switch_channel_get_variable(channel, field + 1);
			} else if (*field == '%') {
				switch_stream_handle_t stream = {0};
				char *cmd = switch_core_session_strdup(session, field + 1);
				char *arg;
				
				SWITCH_STANDARD_STREAM(stream);
				
				if (cmd) {
					if ((arg = strchr(cmd, ' '))) {
						*arg++ = '\0';
					}
					if (switch_api_execute(cmd, arg, session, &stream) == SWITCH_STATUS_SUCCESS) {
						field_data = retbuf;
					}
					free(stream.data);
				}
			} else {
				field_data = switch_caller_get_field_by_name(caller_profile, field);
			}
			if (!field_data) {
				field_data = "";
			}
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "test conditions %s(%s) =~ /%s/\n", field, field_data, expression);
			if (!(proceed = switch_perform_regex(field_data, expression, &re, ovector, sizeof(ovector) / sizeof(ovector[0])))) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Regex mismatch\n");

				for (xaction = switch_xml_child(xcond, "anti-action"); xaction; xaction = xaction->next) {
					char *application = (char*) switch_xml_attr_soft(xaction, "application");
					char *data = (char *) switch_xml_attr_soft(xaction, "data");

					if (!*extension) {
						if ((*extension =
							 switch_caller_extension_new(session, exten_name, caller_profile->destination_number)) == 0) {
							switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "memory error!\n");
							return 0;
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
			char *application = (char*) switch_xml_attr_soft(xaction, "application");
			char *data = (char *) switch_xml_attr_soft(xaction, "data");
			char substituted[1024] = "";
			char *app_data = NULL;

			if (field && strchr(expression, '(')) {
				switch_perform_substitution(re, proceed, data, field_data, substituted, sizeof(substituted), ovector);
				app_data = substituted;
			} else {
				app_data = data;
			}

			if (!*extension) {
				if ((*extension =
					 switch_caller_extension_new(session, exten_name, caller_profile->destination_number)) == 0) {
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "memory error!\n");
					return 0;
				}
			}

			switch_caller_extension_add_application(session, *extension, application, app_data);
		}

		switch_clean_re(re);

		if (do_break_i == BREAK_ON_TRUE || do_break_i == BREAK_ALWAYS) {
			break;
		}
	}
	return proceed;
}

static switch_caller_extension_t *dialplan_hunt(switch_core_session_t *session)
{
	switch_caller_profile_t *caller_profile;
	switch_caller_extension_t *extension = NULL;
	switch_channel_t *channel;
	switch_xml_t cfg, xml, xcontext, xexten;
	char *context = NULL;
	char params[1024];

	channel = switch_core_session_get_channel(session);
	if ((caller_profile = switch_channel_get_caller_profile(channel))) {
		context = caller_profile->context ? caller_profile->context : "default";
	} else {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Error Obtaining Profile!\n");
		return NULL;
	}

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "Processing %s->%s!\n", caller_profile->caller_id_name,
					  caller_profile->destination_number);
	
	snprintf(params, sizeof(params), "context=%s&dest=%s&cid_name=%s&cid_num=%s&netaddr=%s&ani=%s&aniii=%s&rdnis=%s&source=%s&chan_name=%s&uuid=%s", 
			caller_profile->context, caller_profile->destination_number,
			caller_profile->caller_id_name, caller_profile->caller_id_number,
			caller_profile->network_addr?caller_profile->network_addr:"", 
			caller_profile->ani?caller_profile->ani:"", 
			caller_profile->aniii?caller_profile->aniii:"",
			caller_profile->rdnis?caller_profile->rdnis:"", 
			caller_profile->source, caller_profile->chan_name, caller_profile->uuid);

	if (switch_xml_locate("dialplan", NULL, NULL, NULL, &xml, &cfg, params) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "open of dialplan failed\n");
		switch_channel_hangup(channel, SWITCH_CAUSE_DESTINATION_OUT_OF_ORDER);
		return NULL;
	}
	
	if (!(xcontext = switch_xml_find_child(cfg, "context", "name", context))) {
		if (!(xcontext = switch_xml_find_child(cfg, "context", "name", "global"))) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "context %s not found\n", context);
			switch_channel_hangup(channel, SWITCH_CAUSE_NO_ROUTE_DESTINATION);
			switch_xml_free(xml);
			return NULL;
		}
	}
	
	if (!(xexten = switch_xml_find_child(xcontext, "extension", "name", caller_profile->destination_number))) {
		xexten = switch_xml_child(xcontext, "extension");
	}
	
	while(xexten) {
		int proceed = 0;
		char *cont = (char *) switch_xml_attr_soft(xexten, "continue");

		proceed = parse_exten(session, xexten, &extension);

		if (proceed && !switch_true(cont)) {
			break;
		}

		xexten = xexten->next;
	}


	switch_xml_free(xml);

	if (extension) {
		switch_channel_set_state(channel, CS_EXECUTE);
	} else {
		switch_channel_hangup(channel, SWITCH_CAUSE_NO_ROUTE_DESTINATION);
	}

	return extension;
}


static const switch_dialplan_interface_t dialplan_interface = {
	/*.interface_name = */ "XML",
	/*.hunt_function = */ dialplan_hunt
	/*.next = NULL */
};

static const switch_loadable_module_interface_t dialplan_module_interface = {
	/*.module_name = */ modname,
	/*.endpoint_interface = */ NULL,
	/*.timer_interface = */ NULL,
	/*.dialplan_interface = */ &dialplan_interface,
	/*.codec_interface = */ NULL,
	/*.application_interface = */ NULL
};

SWITCH_MOD_DECLARE(switch_status_t) switch_module_load(const switch_loadable_module_interface_t **module_interface, char *filename)
{

	/* connect my internal structure to the blank pointer passed to me */
	*module_interface = &dialplan_module_interface;

	/* indicate that the module should continue to be loaded */
	return SWITCH_STATUS_SUCCESS;
}

/* For Emacs:
 * Local Variables:
 * mode:c
 * indent-tabs-mode:nil
 * tab-width:4
 * c-basic-offset:4
 * End:
 * For VIM:
 * vim:set softtabstop=4 shiftwidth=4 tabstop=4 expandtab:
 */
