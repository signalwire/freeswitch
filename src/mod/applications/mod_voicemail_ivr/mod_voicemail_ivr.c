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
 * Marc Olivier Chouinard <mochouinard@moctel.com>
 *
 *
 * mod_voicemail_ivr.c -- VoiceMail IVR System
 *
 */
#include <switch.h>

#include "config.h"
#include "menu.h"

/* Prototypes */
SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_voicemail_ivr_shutdown);
SWITCH_MODULE_RUNTIME_FUNCTION(mod_voicemail_ivr_runtime);
SWITCH_MODULE_LOAD_FUNCTION(mod_voicemail_ivr_load);

/* SWITCH_MODULE_DEFINITION(name, load, shutdown, runtime) 
 * Defines a switch_loadable_module_function_table_t and a static const char[] modname
 */
SWITCH_MODULE_DEFINITION(mod_voicemail_ivr, mod_voicemail_ivr_load, mod_voicemail_ivr_shutdown, NULL);


#define VMIVR_DESC "voicemail_ivr"
#define VMIVR_USAGE "<check> profile domain [id]"

SWITCH_STANDARD_APP(voicemail_ivr_function)
{
	const char *id = NULL;
	const char *domain = NULL;
	const char *profile_name = NULL;
	vmivr_profile_t *profile = NULL;
	char *argv[6] = { 0 }; 
	char *mydata = NULL;

	if (!zstr(data)) {
		mydata = switch_core_session_strdup(session, data);
		switch_separate_string(mydata, ' ', argv, (sizeof(argv) / sizeof(argv[0])));
	}

	if (argv[1])
		profile_name = argv[1];

	if (argv[2])
		domain = argv[2];

	if (!strcasecmp(argv[0], "check")) {
		if (argv[3])
			id = argv[3];

		if (domain && profile_name) {
			profile = get_profile(session, profile_name);

			if (profile) {
				void (*fPtrAuth)(switch_core_session_t *session, vmivr_profile_t *profile) = vmivr_get_menu_function(profile->menu_check_auth);
				void (*fPtrMain)(switch_core_session_t *session, vmivr_profile_t *profile) = vmivr_get_menu_function(profile->menu_check_main);
				void (*fPtrTerminate)(switch_core_session_t *session, vmivr_profile_t *profile) = vmivr_get_menu_function(profile->menu_check_terminate);

				profile->domain = domain;
				profile->id = id;

				if (fPtrAuth && !profile->authorized) {
					fPtrAuth(session, profile);
				}

				if (fPtrMain && profile->authorized) {
					fPtrMain(session, profile);
				}
				if (fPtrTerminate) {
					fPtrTerminate(session, profile);
				}
				free_profile(profile);

			} else {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Profile '%s' not found\n", profile_name);
			}
		}
	}
	return;
}

/* Macro expands to: switch_status_t mod_voicemail_ivr_load(switch_loadable_module_interface_t **module_interface, switch_memory_pool_t *pool) */
SWITCH_MODULE_LOAD_FUNCTION(mod_voicemail_ivr_load)
{
	switch_application_interface_t *app_interface;
	switch_status_t status = SWITCH_STATUS_SUCCESS;

	/* connect my internal structure to the blank pointer passed to me */
	*module_interface = switch_loadable_module_create_module_interface(pool, modname);

	SWITCH_ADD_APP(app_interface, "voicemail_ivr", "voicemail_ivr", VMIVR_DESC, voicemail_ivr_function, VMIVR_USAGE, SAF_NONE);

	/* indicate that the module should continue to be loaded */
	return status;
}

/*
   Called when the system shuts down
   Macro expands to: switch_status_t mod_voicemail_ivr_shutdown() */
SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_voicemail_ivr_shutdown)
{

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
 * vim:set softtabstop=4 shiftwidth=4 tabstop=4 noet
 */
