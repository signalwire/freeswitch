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
 * Brian Fertig <brian.fertig@convergencetek.com>
 *
 *
 * mod_ruby.c -- ruby Module
 *
 */

#ifndef _REENTRANT
#define _REENTRANT
#endif

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <switch.h>

#include <ruby.h>

const char modname[] = "mod_ruby";

static void ruby_function(switch_core_session_t *session, char *data)
{
	char *uuid = switch_core_session_get_uuid(session);
	uint32_t ulen = strlen(uuid);
	uint32_t len = strlen((char *) data) + ulen + 2;
	char *mydata = switch_core_session_alloc(session, len);
	int argc, state;
	char *argv[5];
	char ruby_code[1024]; 

	snprintf(mydata, len, "%s %s", uuid, data);

	argc = switch_separate_string(mydata, ' ', argv, (sizeof(argv) / sizeof(argv[0])));
	
	sprintf(ruby_code, "$uuid=\"%s\"; include(\"%s\");\n", argv[0], argv[1]);

	ruby_init();
			
	ruby_init_loadpath();

	ruby_script("embedded");	
	rb_load_file(data);
	rb_p(rb_eval_string_protect(argv[1], &state));
	if (state) {
		VALUE error = rb_inspect(rb_gv_get("$!"));
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "Your code is broken.  \nHere is the error I found: %s\n",error);		
	}
	state = ruby_exec();
	state = ruby_cleanup(state);
	ruby_finalize();	
}

static const switch_application_interface_t ruby_application_interface = {
	/*.interface_name */ "ruby",
	/*.application_function */ ruby_function
};

static switch_loadable_module_interface_t ruby_module_interface = {
	/*.module_name */ modname,
	/*.endpoint_interface */ NULL,
	/*.timer_interface */ NULL,
	/*.dialplan_interface */ NULL,
	/*.codec_interface */ NULL,
	/*.application_interface */ &ruby_application_interface,
	/*.api_interface */ NULL,
	/*.file_interface */ NULL,
	/*.speech_interface */ NULL,
	/*.directory_interface */ NULL
};

SWITCH_MOD_DECLARE(switch_status_t) switch_module_load(const switch_loadable_module_interface_t **module_interface, char *filename)
{
	/* connect my internal structure to the blank pointer passed to me */
	*module_interface = &ruby_module_interface;

	/* indicate that the module should continue to be loaded */
	return SWITCH_STATUS_SUCCESS;
}
