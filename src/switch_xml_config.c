/* 
 * FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
 * Copyright (C) 2005-2009, Anthony Minessale II <anthm@freeswitch.org>
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

#include <stdio.h>
#include <switch.h>

switch_status_t switch_xml_config_parse(switch_xml_t xml, int reload, switch_xml_config_item_t *options)
{
	switch_xml_config_item_t *item;
	switch_xml_t node;
	switch_event_t *event;
	switch_event_create(&event, SWITCH_EVENT_REQUEST_PARAMS);
	switch_assert(event);
	int file_count = 0, 

	for (node = xml; node; node = node->next) {
		switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, switch_xml_attr_soft(node, "name"); switch_xml_attr_soft(node, "value"));
	}
	
	for (item = options; item->key; item++) {
		const char *value = switch_event_get_header(event, item->key);
		
		if (reload && !item->reloadable) {
			continue;
		}
		
		switch(item->type) {
			case SWITCH_CONFIG_INT:
				{
					int *dest = (int*)item->ptr;
					if (value) {
						if (switch_is_number(value)) {
							*dest = atoi(value);
						} else {
							switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Invalid value [%s] for parameter [%s]\n", 
								value, item->key);
							*dest = (int)item->defaultvalue;
						}
					} else {
						*dest = (int)item->defaultvalue;
					}
				}
				break;
			case SWITCH_CONFIG_STRING:
				{
					switch_xml_config_string_options_t *options = (switch_xml_config_string_options_t*)item->data;
					if (options->length > 0) {
						/* We have a preallocated buffer */
						char *dest = (char*)item->ptr;
						strncpy(dest, value, options->length);
					} else {
						char **dest = (char**)item->ptr;
						if (options->pool) {
							*dest = switch_core_strdup(options->pool, value);
						} else {
							switch_safe_free(*dest); /* Free the destination if its not NULL */
							*dest = strdup(value);
						}
					}
				}
				break;
			case SWITCH_CONFIG_YESNO:
				{
					switch_bool_t *dest = (switch_bool_t*)item->ptr;
					if (value) {
						*dest = !!switch_true(value);
					} else {
						*dest = (switch_bool_t)item->defaultvalue;
					}
				}
				break;
			case SWITCH_CONFIG_CUSTOM: 
				{
					switch_xml_config_callback_t callback = (switch_xml_config_callback_t)item->data;
					callback(item);
				}
				break;
			case SWITCH_CONFIG_ENUM:
				{
					switch_xml_config_enum_item_t *options = (switch_xml_config_enum_item_t*)item->data;
					int *dest = (int*)item->ptr;
					
					if (value) {
						for (;options->key; options++) {
							if (!strcasecmp(value, options->key)) {
								*dest = options->value;
								break;
							}
						}
					
						if (!options->key) { /* if (!found) */
							switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Invalid value [%s] for parameter [%s]\n", 
								value, item->key);
						}
					} else {
						*dest = (int)item->defaultvalue;
					}
				}
				break;
			case SWITCH_CONFIG_FLAG:
				{
					int32_t *dest = (int32_t*)item->ptr;
					int index = (int)item->data;
					if (value) {
						if (switch_true(value)) {
							*dest |= (1 << index);
						} else {
							*dest &= ~(1 << index);
						}
					} else {
						if ((switch_bool_t)item->defaultvalue) {
							*dest |= (1 << index);
						} else {
							*dest &= ~(1 << index);
						}
					}
				}
				break;
			case SWITCH_CONFIG_FLAGARRAY:
				{
					int8_t *dest = (int8_t*)item->ptr;
					int index = (int)item->data;
					if (value) {
						dest[index] = !!switch_true(value);						
					} else {
						dest[index] = (int8_t)((int32_t)item->defaultvalue);
					}
				}
				break;
			case SWITCH_CONFIG_LAST:
				break;
		}
	}
	
	switch_event_destroy(&event);
	
	return SWITCH_STATUS_SUCCESS;
}


#if 0
typedef enum {
	MYENUM_TEST1 = 1,
	MYENUM_TEST2 = 2,
	MYENUM_TEST3 = 3
} myenum_t;

static struct {
	char *stringalloc;
	char string[50];
	myenum_t enumm;
	int yesno;
	
} globals;


void switch_xml_config_test()
{
	char *cf = "test.conf";
	switch_xml_t cfg, xml, settings;
	switch_xml_config_string_options_t config_opt_stringalloc = { NULL, 0 }; /* No pool, use strdup */
	switch_xml_config_string_options_t config_opt_buffer = { NULL, 50 }; 	/* No pool, use current var as buffer */
	switch_xml_config_enum_item_t enumm_options[] = { 
		{ "test1", MYENUM_TEST1 },
		{ "test2", MYENUM_TEST2 },
		{ "test3", MYENUM_TEST3 },
		{ NULL, 0 } 
	};
	
	switch_xml_config_item_t instructions[] = {
			{ "db_host", SWITCH_CONFIG_STRING, SWITCH_TRUE, &globals.stringalloc, "blah", &config_opt_stringalloc },
			{ "db_user", SWITCH_CONFIG_STRING, SWITCH_TRUE, globals.string, 	  "dflt", &config_opt_buffer },
			{ "test",	 SWITCH_CONFIG_ENUM, SWITCH_FALSE, &globals.enumm,  (void*)MYENUM_TEST1, enumm_options },
			SWITCH_CONFIG_END
	};

	if (!(xml = switch_xml_open_cfg("blaster.conf", &cfg, NULL))) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Could not open %s\n", cf);
		return;
	}

	if ((settings = switch_xml_child(cfg, "settings"))) {
		if (switch_xml_config_parse(switch_xml_child(settings, "param"), 0, instructions) == SWITCH_STATUS_SUCCESS) {
			printf("YAY!\n");
		}
	}
	
	if (cfg) {
		switch_xml_free(cfg);
	}
}
#endif
