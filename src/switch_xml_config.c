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
 * Mathieu Rene <mathieu.rene@gmail.com>
 * Portions created by the Initial Developer are Copyright (C)
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 * 
 * Mathieu Rene <mathieu.rene@gmail.com>
 *
 *
 * switch_xml_config.c - Generic configuration parser
 *
 */

#include <switch.h>

SWITCH_DECLARE_DATA switch_xml_config_string_options_t switch_config_string_strdup = { NULL, 0, NULL };

static switch_xml_config_enum_item_t switch_config_types_enum[] = {
	{"int", SWITCH_CONFIG_INT},
	{"string", SWITCH_CONFIG_STRING},
	{"bool", SWITCH_CONFIG_BOOL},
	{"custom", SWITCH_CONFIG_CUSTOM},
	{"enum", SWITCH_CONFIG_ENUM},
	{"flag", SWITCH_CONFIG_FLAG},
	{"flagarray", SWITCH_CONFIG_FLAGARRAY},
	{NULL, 0}
};

SWITCH_DECLARE(switch_size_t) switch_event_import_xml(switch_xml_t xml, const char *keyname, const char *valuename, switch_event_t **event)
{
	switch_xml_t node;
	switch_size_t count = 0;

	if (!*event) {
		/* SWITCH_EVENT_CLONE will not insert any generic event headers */
		switch_event_create(event, SWITCH_EVENT_CLONE);
		switch_assert(*event);
	}

	for (node = xml; node; node = node->next) {
		const char *key = switch_xml_attr_soft(node, keyname);
		const char *value = switch_xml_attr_soft(node, valuename);
		if (key && value) {
			switch_event_add_header_string(*event, SWITCH_STACK_BOTTOM, key, value);
			count++;
		}
	}

	return count;
}

SWITCH_DECLARE(switch_status_t) switch_xml_config_parse_module_settings(const char *file, switch_bool_t reload, switch_xml_config_item_t *instructions)
{
	switch_xml_t cfg, xml, settings;
	switch_status_t status = SWITCH_STATUS_SUCCESS;

	if (!(xml = switch_xml_open_cfg(file, &cfg, NULL))) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Could not open %s\n", file);
		return SWITCH_STATUS_FALSE;
	}

	if ((settings = switch_xml_child(cfg, "settings"))) {
		status = switch_xml_config_parse(switch_xml_child(settings, "param"), reload, instructions);
	}

	switch_xml_free(xml);

	return status;
}

SWITCH_DECLARE(void) switch_xml_config_item_print_doc(int level, switch_xml_config_item_t *item)
{
	if (item->syntax && item->helptext) {
		switch_log_printf(SWITCH_CHANNEL_LOG_CLEAN, level, "Item name: [%s]\nType: %s (%s)\nSyntax: %s\nHelp: %s\n\n",
						  item->key, switch_xml_config_enum_int2str(switch_config_types_enum, item->type),
						  switch_test_flag(item, CONFIG_REQUIRED) ? "required" : "optional", item->syntax, item->helptext);
	}
}

SWITCH_DECLARE(switch_status_t) switch_xml_config_parse(switch_xml_t xml, switch_bool_t reload, switch_xml_config_item_t *instructions)
{
	switch_event_t *event = NULL;
	switch_status_t result;
	int count = switch_event_import_xml(xml, "name", "value", &event);

	result = switch_xml_config_parse_event(event, count, reload, instructions);

	if (event) {
		switch_event_destroy(&event);
	}

	return result;
}

SWITCH_DECLARE(switch_status_t) switch_xml_config_enum_str2int(switch_xml_config_enum_item_t *enum_options, const char *value, int *out)
{
	for (; enum_options->key; enum_options++) {
		if (!strcasecmp(value, enum_options->key)) {
			*out = enum_options->value;
			return SWITCH_STATUS_SUCCESS;
		}
	}

	return SWITCH_STATUS_FALSE;
}

SWITCH_DECLARE(const char *) switch_xml_config_enum_int2str(switch_xml_config_enum_item_t *enum_options, int value)
{
	for (; enum_options->key; enum_options++) {
		if (value == enum_options->value) {
			return enum_options->key;
		}
	}
	return NULL;
}

SWITCH_DECLARE(switch_status_t) switch_xml_config_parse_event(switch_event_t *event, int count, switch_bool_t reload,
															  switch_xml_config_item_t *instructions)
{
	switch_xml_config_item_t *item;
	int matched_count = 0;

	for (item = instructions; item->key; item++) {
		const char *value = switch_event_get_header(event, item->key);
		switch_bool_t changed = SWITCH_FALSE;
		switch_xml_config_callback_t callback = (switch_xml_config_callback_t) item->function;
		void *ptr = item->ptr;

		//switch_assert(ptr);

		if (value) {
			matched_count++;
		}

		if (reload && !switch_test_flag(item, CONFIG_RELOADABLE)) {
			continue;
		}

		if (!value && switch_test_flag(item, CONFIG_REQUIRED)) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Required parameter [%s] is missing\n", item->key);
			return SWITCH_STATUS_FALSE;
		}

		switch (item->type) {
		case SWITCH_CONFIG_INT:
			{
				switch_xml_config_int_options_t *int_options = (switch_xml_config_int_options_t *) item->data;
				int *dest = (int *) ptr;
				int intval;
				if (value) {
					if (switch_is_number(value)) {
						intval = atoi(value);
					} else {
						intval = (int) (intptr_t) item->defaultvalue;
						switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Invalid value [%s] for parameter [%s], setting default [%d]\n",
										  value, item->key, intval);
					}

					if (int_options) {
						/* Enforce validation options */
						if ((int_options->enforce_min && !(intval > int_options->min)) || (int_options->enforce_max && !(intval < int_options->max))) {
							/* Validation failed, set default */
							intval = (int) (intptr_t) item->defaultvalue;
							/* Then complain */
							if (int_options->enforce_min && int_options->enforce_max) {
								switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR,
												  "Invalid value [%s] for parameter [%s], should be between [%d] and [%d], setting default [%d]\n", value,
												  item->key, int_options->min, int_options->max, intval);
							} else {
								switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR,
												  "Invalid value [%s] for parameter [%s], should be %s [%d], setting default [%d]\n", value, item->key,
												  int_options->enforce_min ? "at least" : "at max",
												  int_options->enforce_min ? int_options->min : int_options->max, intval);
							}
						}
					}
				} else {
					intval = (int) (intptr_t) item->defaultvalue;
				}

				if (*dest != intval) {
					*dest = intval;
					changed = SWITCH_TRUE;
				}
			}
			break;
		case SWITCH_CONFIG_STRING:
			{
				switch_xml_config_string_options_t string_options_default = { 0 };
				switch_xml_config_string_options_t *string_options =
					item->data ? (switch_xml_config_string_options_t *) item->data : &string_options_default;
				const char *newstring = NULL;

				/* Perform validation */
				if (value) {
					if (!zstr(string_options->validation_regex)) {
						if (switch_regex_match(value, string_options->validation_regex) == SWITCH_STATUS_SUCCESS) {
							newstring = value;	/* Regex match, accept value */
						} else {
							newstring = (char *) item->defaultvalue;	/* Regex failed */
							if (newstring) {
								switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Invalid value [%s] for parameter [%s], setting default [%s]\n",
												  value, item->key, newstring);
							} else {
								switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Invalid value [%s] for parameter [%s]\n", value, item->key);
							}
							switch_xml_config_item_print_doc(SWITCH_LOG_ERROR, item);
						}
					} else {
						newstring = value;	/* No validation */
					}
				} else {
					newstring = (char *) item->defaultvalue;
				}

				if (string_options->length > 0) {
					/* We have a preallocated buffer */
					char *dest = (char *) ptr;

					if (newstring) {
						if (strncasecmp(dest, newstring, string_options->length)) {
							switch_copy_string(dest, newstring, string_options->length);
							changed = SWITCH_TRUE;
						}
					} else {
						if (*dest != '\0') {
							*dest = '\0';
							changed = SWITCH_TRUE;
						}
					}
				} else if (string_options->pool) {
					/* Pool-allocated buffer */
					char **dest = (char **) ptr;

					if (newstring) {
						if (!*dest || strcmp(*dest, newstring)) {
							*dest = switch_core_strdup(string_options->pool, newstring);
						}
					} else {
						if (*dest) {
							changed = SWITCH_TRUE;
							*dest = NULL;
						}
					}
				} else {
					/* Dynamically allocated buffer */
					char **dest = (char **) ptr;

					if (newstring) {
						if (!*dest || strcmp(*dest, newstring)) {
							switch_safe_free(*dest);
							*dest = strdup(newstring);
							changed = SWITCH_TRUE;
						}
					} else {
						if (*dest) {
							switch_safe_free(*dest);
							changed = SWITCH_TRUE;
						}
					}
				}
			}
			break;
		case SWITCH_CONFIG_BOOL:
			{
				switch_bool_t *dest = (switch_bool_t *) ptr;
				switch_bool_t newval = SWITCH_FALSE;

				if (value && switch_true(value)) {
					newval = SWITCH_TRUE;
				} else if (value && switch_false(value)) {
					newval = SWITCH_FALSE;
				} else if (value) {
					/* Value isnt true or false */
					newval = (switch_bool_t) (intptr_t) item->defaultvalue;
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Invalid value [%s] for parameter [%s], setting default [%s]\n",
									  value, item->key, newval ? "true" : "false");
					switch_xml_config_item_print_doc(SWITCH_LOG_ERROR, item);
				} else {
					newval = (switch_bool_t) (intptr_t) item->defaultvalue;
				}

				if (*dest != newval) {
					*dest = newval;
					changed = SWITCH_TRUE;
				}
			}
			break;
		case SWITCH_CONFIG_CUSTOM:
			break;
		case SWITCH_CONFIG_ENUM:
			{
				switch_xml_config_enum_item_t *enum_options = (switch_xml_config_enum_item_t *) item->data;
				int *dest = (int *) ptr;
				int newval = 0;
				switch_status_t lookup_result = SWITCH_STATUS_SUCCESS;

				if (value) {
					lookup_result = switch_xml_config_enum_str2int(enum_options, value, &newval);
				} else {
					newval = (int) (intptr_t) item->defaultvalue;
				}

				if (lookup_result != SWITCH_STATUS_SUCCESS) {
					newval = (int) (intptr_t) item->defaultvalue;
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Invalid value [%s] for parameter [%s]\n", value, item->key);
					switch_xml_config_item_print_doc(SWITCH_LOG_ERROR, item);
				}

				if (*dest != newval) {
					changed = SWITCH_TRUE;
					*dest = newval;
				}
			}
			break;
		case SWITCH_CONFIG_FLAG:
			{
				int32_t *dest = (int32_t *) ptr;
				int index = (int) (intptr_t) item->data;
				int8_t currentval = (int8_t) ! !(*dest & index);
				int8_t newval = 0;

				if (value) {
					newval = switch_true(value);
				} else {
					newval = (switch_bool_t) (intptr_t) item->defaultvalue;
				}

				if (newval != currentval) {
					changed = SWITCH_TRUE;
					if (newval) {
						*dest |= (1 << index);
					} else {
						*dest &= ~(1 << index);
					}
				}
			}
			break;
		case SWITCH_CONFIG_FLAGARRAY:
			{
				int8_t *dest = (int8_t *) ptr;
				unsigned int index = (unsigned int) (intptr_t) item->data;
				int8_t newval = value ? !!switch_true(value) : (int8_t) ((intptr_t) item->defaultvalue);
				if (dest[index] != newval) {
					changed = SWITCH_TRUE;
					dest[index] = newval;
				}
			}
			break;
		case SWITCH_CONFIG_LAST:
			break;
		default:
			break;
		}

		if (callback) {
			callback(item, value, (reload ? CONFIG_RELOAD : CONFIG_LOAD), changed);
		}
	}

	if (count != matched_count) {
		/* User made a mistake, find it */
		switch_event_header_t *header;
		for (header = event->headers; header; header = header->next) {
			switch_bool_t found = SWITCH_FALSE;
			for (item = instructions; item->key; item++) {
				if (!strcasecmp(header->name, item->key)) {
					found = SWITCH_TRUE;
					break;
				}
			}

			if (!found) {
				/* Tell the user */
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING,
								  "Configuration parameter [%s] is unfortunately not valid, you might want to double-check that.\n", header->name);
			}
		}
	}

	return SWITCH_STATUS_SUCCESS;
}


SWITCH_DECLARE(void) switch_xml_config_cleanup(switch_xml_config_item_t *instructions)
{
	switch_xml_config_item_t *item;

	for (item = instructions; item->key; item++) {
		switch_xml_config_callback_t callback = (switch_xml_config_callback_t) item->function;

		switch (item->type) {
		case SWITCH_CONFIG_STRING:
			{
				char **ptr = (char **) item->ptr;
				switch_xml_config_string_options_t *string_options = (switch_xml_config_string_options_t *) item->data;
				/* if (using_strdup) */
				if (string_options && !string_options->pool && !string_options->length) {
					switch_safe_free(*ptr);
				}
			}
			break;
		default:
			break;
		}

		if (callback) {
			callback(item, NULL, CONFIG_SHUTDOWN, SWITCH_FALSE);
		}

	}
}


SWITCH_DECLARE(void) switch_config_perform_set_item(switch_xml_config_item_t *item, const char *key, switch_xml_config_type_t type, int flags, void *ptr,
													const void *defaultvalue, void *data, switch_xml_config_callback_t function, const char *syntax,
													const char *helptext)
{
	item->key = key;
	item->type = type;
	item->flags = flags;
	item->ptr = ptr;
	item->defaultvalue = defaultvalue;
	item->data = data;
	item->function = function;
	item->syntax = syntax;
	item->helptext = helptext;
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
