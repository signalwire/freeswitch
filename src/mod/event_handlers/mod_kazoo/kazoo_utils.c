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
 * Andrew Thompson <andrew@hijacked.us>
 * Rob Charlton <rob.charlton@savageminds.com>
 * Karl Anderson <karl@2600hz.com>
 *
 * Original from mod_erlang_event.
 * ei_helpers.c -- helper functions for ei
 *
 */
#include "mod_kazoo.h"

char *kazoo_expand_header(switch_memory_pool_t *pool, switch_event_t *event, char *val)
{
	char *expanded;
	char *dup = NULL;

	expanded = switch_event_expand_headers(event, val);
	dup = switch_core_strdup(pool, expanded);

	if (expanded != val) {
		free(expanded);
	}

	return dup;
}

char* switch_event_get_first_of(switch_event_t *event, const char *list[])
{
	switch_event_header_t *header = NULL;
	int i = 0;
	while(list[i] != NULL) {
		if((header = switch_event_get_header_ptr(event, list[i])) != NULL)
			break;
		i++;
	}
	if(header != NULL) {
		return header->value;
	} else {
		return "nodomain";
	}
}

SWITCH_DECLARE(switch_status_t) switch_event_add_variable_name_printf(switch_event_t *event, switch_stack_t stack, const char *val, const char *fmt, ...)
{
	int ret = 0;
	char *varname;
	va_list ap;
	switch_status_t status = SWITCH_STATUS_SUCCESS;

	switch_assert(event != NULL);


	va_start(ap, fmt);
	ret = switch_vasprintf(&varname, fmt, ap);
	va_end(ap);

	if (ret == -1) {
		return SWITCH_STATUS_MEMERR;
	}

	status = switch_event_add_header_string(event, stack, varname, val);

	free(varname);

	return status;
}

SWITCH_DECLARE(switch_xml_t) kz_xml_child(switch_xml_t xml, const char *name)
{
	xml = (xml) ? xml->child : NULL;
	while (xml && strcasecmp(name, xml->name))
		xml = xml->sibling;
	return xml;
}

void kz_xml_process(switch_xml_t cfg)
{
	switch_xml_t xml_process;
	for (xml_process = kz_xml_child(cfg, "X-PRE-PROCESS"); xml_process; xml_process = xml_process->next) {
		const char *cmd = switch_xml_attr(xml_process, "cmd");
		const char *data = switch_xml_attr(xml_process, "data");
		if(cmd != NULL && !strcasecmp(cmd, "set") && data) {
			char *name = (char *) data;
			char *val = strchr(name, '=');

			if (val) {
				char *ve = val++;
				while (*val && *val == ' ') {
					val++;
				}
				*ve-- = '\0';
				while (*ve && *ve == ' ') {
					*ve-- = '\0';
				}
			}

			if (name && val) {
				switch_core_set_variable(name, val);
			}
		}
	}

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
