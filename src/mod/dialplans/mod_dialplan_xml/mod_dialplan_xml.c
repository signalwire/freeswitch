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
#include <pcre.h>

static const char modname[] = "mod_dialplan_xml";

#define cleanre(re)	if (re) {\
				pcre_free(re);\
				re = NULL;\
			}


static int perform_regex(switch_channel_t *channel, char *field, char *expression, pcre **new_re, int *ovector, uint32_t olen)
{
	const char *error = NULL;
	int erroffset = 0;
	pcre *re = NULL;
	int match_count = 0;
	
	if (!(field && expression)) {
		return 0;
	}

	re = pcre_compile(expression, /* the pattern */
					  0,		  /* default options */
					  &error,	  /* for error message */
					  &erroffset, /* for error offset */
					  NULL);	  /* use default character tables */
	if (error) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "COMPILE ERROR: %d [%s]\n", erroffset, error);
		cleanre(re);
		return 0;
	}

	match_count = pcre_exec(re,	/* result of pcre_compile() */
							NULL,	/* we didn't study the pattern */
							field,	/* the subject string */
							(int) strlen(field), /* the length of the subject string */
							0,	/* start at offset 0 in the subject */
							0,	/* default options */
							ovector,	/* vector of integers for substring information */
							olen); /* number of elements (NOT size in bytes) */

	if (match_count <= 0) {
		cleanre(re);
		match_count = 0;
	}

	*new_re = re;

	return match_count;
}


static void perform_substitution(pcre *re, int match_count, char *data, char *field_data, char *substituted, uint32_t len, int *ovector)
{
	char index[10] = "";
	char replace[1024] = "";
	uint32_t x, y = 0, z = 0, num = 0;

	for (x = 0; x < (len-1) && x < strlen(data);) {
		if (data[x] == '$') {
			x++;

			while (data[x] > 47 && data[x] < 58) {
				index[z++] = data[x];
				x++;
			}
			index[z++] = '\0';
			z = 0;
			num = atoi(index);
			
			if (pcre_copy_substring(field_data,
									ovector,
									match_count,
									num,
									replace,
									sizeof(replace)) > 0) {
				unsigned int r;
				for (r = 0; r < strlen(replace); r++) {
					substituted[y++] = replace[r];
				}
			}
		} else {
			substituted[y++] = data[x];
			x++;
		}
	}
	substituted[y++] = '\0';
}

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
		char *expression = NULL;
		char *field_data = NULL;
		pcre *re = NULL;
		int ovector[30];

		field = (char *) switch_xml_attr(xcond, "field");
		expression = (char *) switch_xml_attr_soft(xcond, "expression");
		
		if (field) {
			if (*field == '$') {
				field_data = switch_channel_get_variable(channel, field + 1);
			} else {
				field_data = switch_caller_get_field_by_name(caller_profile, field);
			}
			if (!field_data) {
				field_data = "";
			}
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "test conditions %s(%s) =~ /%s/\n", field, field_data, expression);
			if (!(proceed = perform_regex(channel, field_data, expression, &re, ovector, sizeof(ovector) / sizeof(ovector[0])))) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Regex mismatch\n");
				break;
			}
			assert(re != NULL);
		}


		for (xaction = switch_xml_child(xcond, "action"); xaction; xaction = xaction->next) {
			char *application = (char*) switch_xml_attr_soft(xaction, "application");
			char *data = (char *) switch_xml_attr_soft(xaction, "data");
			char substituted[1024] = "";
			char *app_data = NULL;

			if (field && strchr(expression, '(')) {
				perform_substitution(re, proceed, data, field_data, substituted, sizeof(substituted), ovector);
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

		cleanre(re);
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
	
	snprintf(params, sizeof(params), "context=%s&dest=%s", caller_profile->context, caller_profile->destination_number);

	if (switch_xml_locate("dialplan", NULL, NULL, NULL, &xml, &cfg, params) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "open of dialplan failed\n");
		switch_channel_hangup(channel, SWITCH_CAUSE_DESTINATION_OUT_OF_ORDER);
		return NULL;
	}
	
	if (!(xcontext = switch_xml_find_child(cfg, "context", "name", context))) {
		if (!(xcontext = switch_xml_find_child(cfg, "context", "name", "global"))) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "context %s not found\n", context);
			switch_channel_hangup(channel, SWITCH_CAUSE_MESSAGE_TYPE_NONEXIST);
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

		//printf("ASS %s %d\n", (char *) switch_xml_attr_soft(xexten, "name"), proceed);

		if (proceed && !switch_true(cont)) {
			break;
		}

		xexten = xexten->next;
	}


	switch_xml_free(xml);

	if (extension) {
		switch_channel_set_state(channel, CS_EXECUTE);
	} else {
		switch_channel_hangup(channel, SWITCH_CAUSE_MESSAGE_TYPE_NONEXIST);
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
