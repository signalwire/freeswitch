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

//#define SWITCH_XML_CONFIG_TEST

#include <switch.h>

SWITCH_DECLARE(switch_status_t) switch_xml_config_parse(switch_xml_t xml, int reload, switch_xml_config_item_t *instructions)
{
	switch_xml_config_item_t *item;
	switch_xml_t node;
	switch_event_t *event;
	int file_count = 0, matched_count = 0;
	
	switch_event_create(&event, SWITCH_EVENT_REQUEST_PARAMS);
	switch_assert(event);


	for (node = xml; node; node = node->next) {
		switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, switch_xml_attr_soft(node, "name"), switch_xml_attr_soft(node, "value"));
		file_count++;
	}
	
	for (item = instructions; item->key; item++) {
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
							*dest = (int)(intptr_t)item->defaultvalue;
						}
					} else {
						*dest = (int)(intptr_t)item->defaultvalue;
					}
				}
				break;
			case SWITCH_CONFIG_STRING:
				{
					switch_xml_config_string_options_t *string_options = (switch_xml_config_string_options_t*)item->data;
					if (string_options->length > 0) {
						/* We have a preallocated buffer */
						char *dest = (char*)item->ptr;
						if (value) {
							switch_copy_string(dest, value, string_options->length);
						} else if (item->defaultvalue){
							switch_copy_string(dest, item->defaultvalue, string_options->length);
						}
					} else {
						char **dest = (char**)item->ptr;
						if (string_options->pool) {
							if (value) {
								*dest = switch_core_strdup(string_options->pool, value);
							} else if (item->defaultvalue) {
								*dest = switch_core_strdup(string_options->pool, (char*)item->defaultvalue);
							}
						} else {
							switch_safe_free(*dest); /* Free the destination if its not NULL */
							if (value) {
								*dest = strdup(value);
							} else if(item->defaultvalue) {
								*dest = strdup((char*)item->defaultvalue);
							}
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
						*dest = (switch_bool_t)(intptr_t)item->defaultvalue;
					}
				}
				break;
			case SWITCH_CONFIG_CUSTOM: 
				{	
					switch_xml_config_callback_t callback = (switch_xml_config_callback_t)item->function;
					callback(item);
				}
				break;
			case SWITCH_CONFIG_ENUM:
				{
					switch_xml_config_enum_item_t *enum_options = (switch_xml_config_enum_item_t*)item->data;
					int *dest = (int*)item->ptr;
					
					if (value) {
						for (;enum_options->key; enum_options++) {
							if (!strcasecmp(value, enum_options->key)) {
								*dest = enum_options->value;
								break;
							}
						}
					
						if (!enum_options->key) { /* if (!found) */
							switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Invalid value [%s] for parameter [%s]\n", 
								value, item->key);
						}
					} else {
						*dest = (int)(intptr_t)item->defaultvalue;
					}
				}
				break;
			case SWITCH_CONFIG_FLAG:
				{
					int32_t *dest = (int32_t*)item->ptr;
					int index = (int)(intptr_t)item->data;
					if (value) {
						if (switch_true(value)) {
							*dest |= (1 << index);
						} else {
							*dest &= ~(1 << index);
						}
					} else {
						if ((switch_bool_t)(intptr_t)item->defaultvalue) {
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
					int index = (int)(intptr_t)item->data;
					if (value) {
						dest[index] = !!switch_true(value);						
					} else {
						dest[index] = (int8_t)((intptr_t)item->defaultvalue);
					}
				}
				break;
			case SWITCH_CONFIG_LAST:
				break;
		}
	}
	
	if (file_count > matched_count) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "Config file had %d params but only %d were valid\n", file_count, matched_count);
	}
	
	switch_event_destroy(&event);
	
	return SWITCH_STATUS_SUCCESS;
}


#if SWITCH_XML_CONFIG_TEST
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


SWITCH_DECLARE(void) switch_xml_config_test()
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
			SWITCH_CONFIG_ITEM("db_host", SWITCH_CONFIG_STRING, SWITCH_TRUE, &globals.stringalloc, "blah", &config_opt_stringalloc),
			SWITCH_CONFIG_ITEM("db_user", SWITCH_CONFIG_STRING, SWITCH_TRUE, globals.string, "dflt", &config_opt_buffer ),
			SWITCH_CONFIG_ITEM("test", SWITCH_CONFIG_ENUM, SWITCH_FALSE, &globals.enumm,  (void*)MYENUM_TEST1, enumm_options ),
			SWITCH_CONFIG_ITEM_END()
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
