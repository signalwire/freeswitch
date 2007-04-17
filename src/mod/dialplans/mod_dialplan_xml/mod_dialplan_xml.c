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
	switch_stream_handle_t stream = { 0 };

	channel = switch_core_session_get_channel(session);
	caller_profile = switch_channel_get_caller_profile(channel);

	for (xcond = switch_xml_child(xexten, "condition"); xcond; xcond = xcond->next) {
		char *field = NULL;
		char *do_break_a = NULL;
		char *expression = NULL;
		char *field_data = NULL;
		switch_regex_t *re = NULL;
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
				char *cmd = switch_core_session_strdup(session, field + 1);
				char *e, *arg;
				field = cmd;
				field_data = "";

				if (*field == '{') {
					field++;
					if ((e = strchr(field, '}'))) {
						*e = '\0';
						field_data = switch_channel_get_variable(channel, field);
					}
				} else {
					switch_safe_free(stream.data);
					memset(&stream, 0, sizeof(stream));
					SWITCH_STANDARD_STREAM(stream);

					if ((arg = strchr(cmd, '('))) {
						*arg++ = '\0';
						if ((e = strchr(arg, ')'))) {
							*e = '\0';
							if (switch_api_execute(cmd, arg, session, &stream) == SWITCH_STATUS_SUCCESS) {
								field_data = stream.data;
							}
						}
					}
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
			char *data = (char *) switch_xml_attr_soft(xaction, "data");
			char *substituted = NULL;
			uint32_t len = 0;
			char *app_data = NULL;

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
	switch_safe_free(stream.data);
	return proceed;
}

static switch_status_t dialplan_xml_locate(switch_core_session_t *session, switch_caller_profile_t *caller_profile, switch_xml_t * root,
										   switch_xml_t * node)
{
	switch_status_t status = SWITCH_STATUS_GENERR;
	switch_channel_t *channel;
	switch_stream_handle_t stream = { 0 };
	switch_size_t encode_len = 1024, new_len = 0;
	char *encode_buf = NULL;
	char *prof[12] = { 0 }, *prof_names[12] = {
	0}, *e = NULL;
	switch_hash_index_t *hi;
	uint32_t x = 0;

	channel = switch_core_session_get_channel(session);

	SWITCH_STANDARD_STREAM(stream);

	if (!(encode_buf = malloc(encode_len))) {
		goto done;
	}

	prof[0] = caller_profile->context;
	prof[1] = caller_profile->destination_number;
	prof[2] = caller_profile->caller_id_name;
	prof[3] = caller_profile->caller_id_number;
	prof[4] = caller_profile->network_addr;
	prof[5] = caller_profile->ani;
	prof[6] = caller_profile->aniii;
	prof[7] = caller_profile->rdnis;
	prof[8] = caller_profile->source;
	prof[9] = caller_profile->chan_name;
	prof[10] = caller_profile->uuid;

	prof_names[0] = "context";
	prof_names[1] = "destination_number";
	prof_names[2] = "caller_id_name";
	prof_names[3] = "caller_id_number";
	prof_names[4] = "network_addr";
	prof_names[5] = "ani";
	prof_names[6] = "aniii";
	prof_names[7] = "rdnis";
	prof_names[8] = "source";
	prof_names[9] = "chan_name";
	prof_names[10] = "uuid";

	for (x = 0; prof[x]; x++) {
		if (switch_strlen_zero(prof[x])) {
			continue;
		}
		new_len = (strlen(prof[x]) * 3) + 1;
		if (encode_len < new_len) {
			char *tmp;

			encode_len = new_len;

			if (!(tmp = realloc(encode_buf, encode_len))) {
				goto done;
			}

			encode_buf = tmp;
		}
		switch_url_encode(prof[x], encode_buf, encode_len - 1);
		stream.write_function(&stream, "%s=%s&", prof_names[x], encode_buf);
	}


	for (hi = switch_channel_variable_first(channel, switch_core_session_get_pool(session)); hi; hi = switch_hash_next(hi)) {
		void *val;
		const void *var;
		switch_hash_this(hi, &var, NULL, &val);

		new_len = (strlen((char *) var) * 3) + 1;
		if (encode_len < new_len) {
			char *tmp;

			encode_len = new_len;

			if (!(tmp = realloc(encode_buf, encode_len))) {
				goto done;
			}

			encode_buf = tmp;
		}

		switch_url_encode((char *) val, encode_buf, encode_len - 1);
		stream.write_function(&stream, "%s=%s&", (char *) var, encode_buf);

	}

	e = (char *) stream.data + (strlen((char *) stream.data) - 1);

	if (e && *e == '&') {
		*e = '\0';
	}

	status = switch_xml_locate("dialplan", NULL, NULL, NULL, root, node, stream.data);

  done:
	switch_safe_free(stream.data);
	switch_safe_free(encode_buf);
	return status;
}

static switch_caller_extension_t *dialplan_hunt(switch_core_session_t *session, void *arg)
{
	switch_caller_profile_t *caller_profile;
	switch_caller_extension_t *extension = NULL;
	switch_channel_t *channel;
	switch_xml_t alt_root = NULL, cfg, xml = NULL, xcontext, xexten;
	char *alt_path = (char *) arg;

	channel = switch_core_session_get_channel(session);

	if ((caller_profile = switch_channel_get_caller_profile(channel))) {
		if (!caller_profile->context) {
			caller_profile->context = "default";
		}
	} else {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Error Obtaining Profile!\n");
		goto done;
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

		proceed = parse_exten(session, xexten, &extension);

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
 * indent-tabs-mode:t
 * tab-width:4
 * c-basic-offset:4
 * End:
 * For VIM:
 * vim:set softtabstop=4 shiftwidth=4 tabstop=4 expandtab:
 */
