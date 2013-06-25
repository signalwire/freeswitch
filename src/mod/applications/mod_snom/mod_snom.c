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
 * mod_snom.c -- SNOM Specific Features
 *
 */
#include <switch.h>

SWITCH_MODULE_LOAD_FUNCTION(mod_snom_load);
SWITCH_MODULE_DEFINITION(mod_snom, mod_snom_load, NULL, NULL);

static switch_bool_t snom_bind_key(const char *key,
								   const char *light,
								   const char *label, const char *user, const char *host, const char *profile, const char *action_name, const char *action)
{
	switch_event_t *event;


	if (user && host && profile) {
		if (switch_event_create(&event, SWITCH_EVENT_SEND_MESSAGE) == SWITCH_STATUS_SUCCESS) {
			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "user", user);
			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "host", host);
			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "profile", profile);
			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "content-type", "application/x-buttons");
			if (action && action_name) {
				switch_event_add_body(event, "k=%s\nc=%s\nl=%s\nn=%s\na=%s\n", key, light, label, action, action_name);
			} else {
				switch_event_add_body(event, "k=%s\nc=%s\nl=%s\n\n", key, light, label);
			}

			switch_event_fire(&event);
		}
		return SWITCH_TRUE;
	}

	return SWITCH_FALSE;
}


#define URL_SYNTAX ""
SWITCH_STANDARD_API(snom_url_api_function)
{
#if 0
	char *tmp;
	switch_event_serialize(stream->param_event, &tmp, SWITCH_TRUE);
	printf("W00t\n%s\n", tmp);
	free(tmp);
#endif

	return SWITCH_STATUS_SUCCESS;

}

#define KEY_BIND_SYNTAX "<key> <light> <label> <user> <host> <profile> <action_name> <action>"
SWITCH_STANDARD_API(snom_bind_key_api_function)
{
	int argc;
	char *mydata = NULL, *argv[8];

	mydata = strdup(cmd);
	argc = switch_separate_string(mydata, ' ', argv, (sizeof(argv) / sizeof(argv[0])));

	if (argc < 6) {
		goto err;
	}

	if (snom_bind_key(argv[0], argv[1], argv[2], argv[3], argv[4], argv[5], argv[6], argv[7])) {
		stream->write_function(stream, "+OK %s\n", cmd);
		goto end;
	}

  err:

	stream->write_function(stream, "-Error %s\n", KEY_BIND_SYNTAX);

  end:

	free(mydata);

	return SWITCH_STATUS_SUCCESS;
}


SWITCH_MODULE_LOAD_FUNCTION(mod_snom_load)
{

	switch_api_interface_t *commands_api_interface;

	/* connect my internal structure to the blank pointer passed to me */
	*module_interface = switch_loadable_module_create_module_interface(pool, modname);


	SWITCH_ADD_API(commands_api_interface, "snom_bind_key", "Bind a key", snom_bind_key_api_function, KEY_BIND_SYNTAX);
	SWITCH_ADD_API(commands_api_interface, "snom_url", "url", snom_url_api_function, URL_SYNTAX);


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
 * vim:set softtabstop=4 shiftwidth=4 tabstop=4 noet:
 */
