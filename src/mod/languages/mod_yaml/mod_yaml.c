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
 * mod_yaml.c -- YAML Module
 *
 */
#include <switch.h>
#include <yaml.h>

SWITCH_MODULE_LOAD_FUNCTION(mod_yaml_load);
SWITCH_MODULE_DEFINITION(mod_yaml, mod_yaml_load, NULL, NULL);

static void print_error(yaml_parser_t *parser)
{
	switch (parser->error) {
	case YAML_MEMORY_ERROR:
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Memory error: Not enough memory for parsing\n");
		break;

	case YAML_READER_ERROR:
		if (parser->problem_value != -1) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Reader error: %s: #%X at %d\n", parser->problem,
							  parser->problem_value, (int) parser->problem_offset);
		} else {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Reader error: %s at %d\n", parser->problem, (int) parser->problem_offset);
		}
		break;

	case YAML_SCANNER_ERROR:
		if (parser->context) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Scanner error: %s at line %d, column %d\n"
							  "%s at line %d, column %d\n", parser->context,
							  (int) parser->context_mark.line + 1, (int) parser->context_mark.column + 1,
							  parser->problem, (int) parser->problem_mark.line + 1, (int) parser->problem_mark.column + 1);
		} else {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Scanner error: %s at line %d, column %d\n",
							  parser->problem, (int) parser->problem_mark.line + 1, (int) parser->problem_mark.column + 1);
		}
		break;

	case YAML_PARSER_ERROR:
		if (parser->context) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Parser error: %s at line %d, column %d\n"
							  "%s at line %d, column %d\n", parser->context,
							  (int) parser->context_mark.line + 1, (int) parser->context_mark.column + 1,
							  parser->problem, (int) parser->problem_mark.line + 1, (int) parser->problem_mark.column + 1);
		} else {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Parser error: %s at line %d, column %d\n",
							  parser->problem, (int) parser->problem_mark.line + 1, (int) parser->problem_mark.column + 1);
		}
		break;

	default:
		/* Couldn't happen. */
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Internal error\n");
		break;
	}
}


static switch_xml_t parse_file(FILE * input, const char *file_name)
{
	yaml_parser_t parser;
	yaml_event_t event = { 0 };
	char *scalar_data;
	int done = 0;
	int depth = 0;
	char name[128] = "";
	char value[128] = "";
	char category[128] = "";
	int nv = 0, p_off = 0;
	switch_xml_t xml, param, top, current = NULL;

	yaml_parser_initialize(&parser);
	yaml_parser_set_input_file(&parser, input);


	if (!(xml = switch_xml_new("document"))) {
		return NULL;
	}

	switch_xml_set_attr_d(xml, "type", "freeswitch/xml");
	current = switch_xml_add_child_d(xml, "section", 0);
	switch_xml_set_attr_d(current, "name", "configuration");

	top = switch_xml_add_child_d(current, "configuration", 0);
	switch_xml_set_attr_d(top, "name", file_name);

	while (!done) {
		if (!yaml_parser_parse(&parser, &event)) {
			print_error(&parser);
			break;
		} else {
			switch (event.type) {
			case YAML_MAPPING_START_EVENT:
				depth++;
				break;
			case YAML_MAPPING_END_EVENT:
				depth--;
				break;
			case YAML_STREAM_END_EVENT:
				done = 1;
				break;
			case YAML_SCALAR_EVENT:
				scalar_data = (char *) event.data.scalar.value;
				switch (depth) {
				case 1:
					if (!(current = switch_xml_add_child_d(top, scalar_data, depth - 1))) {
						done = 1;
					}
					switch_set_string(category, scalar_data);
					nv = 0;
					p_off = 0;
					break;
				case 2:
					if (current) {
						if (nv == 0) {
							switch_set_string(name, scalar_data);
							nv++;
						} else {
							switch_set_string(value, scalar_data);
							param = switch_xml_add_child_d(current, "param", p_off++);
							switch_xml_set_attr_d_buf(param, "name", name);
							switch_xml_set_attr_d(param, "value", scalar_data);
							nv = 0;
						}
					}
					break;
				}

				break;
			default:
				break;
			}
		}

		yaml_event_delete(&event);
	}

	yaml_parser_delete(&parser);

	if (input) {
		fclose(input);
	}
#ifdef DEBUG_XML
	if (xml) {
		char *foo = switch_xml_toxml(xml, SWITCH_FALSE);
		printf("%s\n", foo);
		free(foo);
	}
#endif

	return xml;

}

static switch_xml_t yaml_fetch(const char *section,
							   const char *tag_name, const char *key_name, const char *key_value, switch_event_t *params, void *user_data)
{
	char *path;
	FILE *input;
	switch_xml_t xml = NULL;

	path = switch_mprintf("%s/yaml/%s.yaml", SWITCH_GLOBAL_dirs.conf_dir, key_value);
	if ((input = fopen(path, "r"))) {
		xml = parse_file(input, key_value);
	} else {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "cannot open %s\n", path);
	}

	switch_safe_free(path);
	return xml;
}


static switch_caller_extension_t *parse_dp(FILE * input, switch_core_session_t *session, switch_caller_profile_t *caller_profile)
{
	yaml_parser_t parser;
	yaml_event_t event = { 0 };
	char *scalar_data;
	int done = 0;
	int depth = 0;
	char name[128] = "";
	char value[128] = "";
	char category[128] = "";
	char *last_field = NULL;
	int nv = 0;
	switch_caller_extension_t *extension = NULL;
	switch_channel_t *channel = switch_core_session_get_channel(session);
	int context_hit = 0;
	int proceed = 0;
	switch_regex_t *re = NULL;
	int ovector[30];
	int parens = 0;

	if (!caller_profile) {
		if (!(caller_profile = switch_channel_get_caller_profile(channel))) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Error Obtaining Profile!\n");
			return NULL;
		}
	}

	if (!caller_profile->context) {
		caller_profile->context = "default";
	}

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "Processing %s->%s@%s\n",
					  caller_profile->caller_id_name, caller_profile->destination_number, caller_profile->context);

	yaml_parser_initialize(&parser);
	yaml_parser_set_input_file(&parser, input);

	while (!done) {
		if (!yaml_parser_parse(&parser, &event)) {
			print_error(&parser);
			break;
		} else {
			switch (event.type) {
			case YAML_MAPPING_START_EVENT:
				depth++;
				break;
			case YAML_MAPPING_END_EVENT:
				depth--;
				break;
			case YAML_STREAM_END_EVENT:
				done = 1;
				break;
			case YAML_SCALAR_EVENT:
				scalar_data = (char *) event.data.scalar.value;
				switch (depth) {
				case 1:
					switch_set_string(category, scalar_data);
					context_hit = (!strcasecmp(category, caller_profile->context));
					nv = 0;
					break;
				case 2:
					if (context_hit) {
						char *field = switch_core_session_strdup(session, scalar_data);
						char *p, *e, *expression = NULL, *field_expanded = NULL, *expression_expanded = NULL;
						const char *field_data = NULL;

						parens = 0;
						proceed = 0;
						switch_regex_safe_free(re);

						if ((p = strstr(field, "=~"))) {
							*p = '\0';
							e = p - 1;
							while (*e == ' ') {
								*e-- = '\0';
							}
							e = p + 2;
							while (*e == ' ') {
								*e++ = '\0';
							}
							expression = e;
						}

						if (field && expression) {
							if ((expression_expanded = switch_channel_expand_variables(channel, expression)) == expression) {
								expression_expanded = NULL;
							} else {
								expression = expression_expanded;
							}

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
							switch_safe_free(last_field);
							last_field = strdup(field_data);

							switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "test conditions %s(%s) =~ /%s/\n", field, field_data, expression);
							if (!(proceed = switch_regex_perform(field_data, expression, &re, ovector, sizeof(ovector) / sizeof(ovector[0])))) {
								switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Regex mismatch\n");
							}

							if (strchr(expression, '(')) {
								parens++;
							}

							switch_safe_free(field_expanded);
							switch_safe_free(expression_expanded);
						}
					}
					break;
				case 3:
					if (nv == 0) {
						if (!strcasecmp(scalar_data, "exit")) {
							yaml_event_delete(&event);
							goto end;
						}
						switch_set_string(name, scalar_data);
						nv++;
					} else {
						switch_set_string(value, scalar_data);
						nv = 0;
						if (proceed) {
							uint32_t len = 0;
							char *substituted = NULL;
							char *app_data;


							if (!extension) {
								extension = switch_caller_extension_new(session, "YAML", caller_profile->destination_number);
								switch_assert(extension);
							}

							if (parens) {
								len = (uint32_t) (strlen(value) + strlen(last_field) + 10) * proceed;
								switch_zmalloc(substituted, len);
								switch_perform_substitution(re, proceed, value, last_field, substituted, len, ovector);
								app_data = substituted;
							} else {
								app_data = value;
							}

							switch_caller_extension_add_application(session, extension, name, app_data);
							switch_safe_free(substituted);
						}
					}
					break;
				}

				break;
			default:
				break;
			}
		}

		yaml_event_delete(&event);
	}

  end:

	switch_safe_free(last_field);
	switch_regex_safe_free(re);
	yaml_parser_delete(&parser);

	if (input) {
		fclose(input);
	}
#ifdef DEBUG_XML
	if (xml) {
		char *foo = switch_xml_toxml(xml, SWITCH_FALSE);
		printf("%s\n", foo);
		free(foo);
	}
#endif

	return extension;

}

SWITCH_STANDARD_DIALPLAN(yaml_dialplan_hunt)
{
	switch_caller_extension_t *extension = NULL;
	char *alt_path = (char *) arg;
	char *path = NULL;
	FILE *input;

	if (!zstr(alt_path)) {
		path = strdup(alt_path);
	} else {
		path = switch_mprintf("%s/yaml/extensions.yaml", SWITCH_GLOBAL_dirs.conf_dir);
	}

	if ((input = fopen(path, "r"))) {
		extension = parse_dp(input, session, caller_profile);
	} else {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Error opening %s\n", path);
	}

	switch_safe_free(path);
	return extension;
}


static switch_status_t do_config(void)
{
	yaml_parser_t parser;
	yaml_event_t event = { 0 };
	char *path;
	const char *cfg = "mod_yaml.yaml";
	FILE *input;
	switch_status_t status = SWITCH_STATUS_FALSE;
	char *scalar_data;
	int done = 0;
	int depth = 0;
	char name[128] = "";
	char value[128] = "";
	char category[128] = "";
	int nv = 0;

	path = switch_mprintf("%s/yaml/%s", SWITCH_GLOBAL_dirs.conf_dir, cfg);

	if (!(input = fopen(path, "r"))) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Error opening %s\n", path);
		goto end;
	}

	yaml_parser_initialize(&parser);
	yaml_parser_set_input_file(&parser, input);

	while (!done) {
		if (!yaml_parser_parse(&parser, &event)) {
			print_error(&parser);
			break;
		} else {
			switch (event.type) {
			case YAML_MAPPING_START_EVENT:
				depth++;
				break;
			case YAML_MAPPING_END_EVENT:
				depth--;
				break;
			case YAML_STREAM_END_EVENT:
				done = 1;
				break;
			case YAML_SCALAR_EVENT:
				scalar_data = (char *) event.data.scalar.value;
				switch (depth) {
				case 1:
					switch_set_string(category, scalar_data);
					nv = 0;
					break;
				case 2:
					if (nv == 0) {
						switch_set_string(name, scalar_data);
						nv++;
					} else {
						switch_set_string(value, scalar_data);
						if (!strcasecmp(category, "settings")) {
							if (!strcasecmp(name, "bind_config") && switch_true_buf(value)) {
								switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "Binding To XML Config\n");
								switch_xml_bind_search_function(yaml_fetch, switch_xml_parse_section_string("config"), NULL);
							}
						}
						nv = 0;
					}
					break;
				}

				break;
			default:
				break;
			}
		}

		yaml_event_delete(&event);
	}

	yaml_parser_delete(&parser);
	status = SWITCH_STATUS_SUCCESS;

  end:

	if (input) {
		fclose(input);
	}

	switch_safe_free(path);

	return status;

}

SWITCH_MODULE_LOAD_FUNCTION(mod_yaml_load)
{
	switch_dialplan_interface_t *dp_interface;

	/* connect my internal structure to the blank pointer passed to me */
	*module_interface = switch_loadable_module_create_module_interface(pool, modname);

	if (do_config() != SWITCH_STATUS_SUCCESS) {
		return SWITCH_STATUS_TERM;
	}

	SWITCH_ADD_DIALPLAN(dp_interface, "YAML", yaml_dialplan_hunt);

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
