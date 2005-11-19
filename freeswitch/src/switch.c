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
 * switch.c -- Main
 *
 */
#include <switch.h>

int main(int argc, char *argv[]) {
	char *err = NULL;

	if (switch_core_init() != SWITCH_STATUS_SUCCESS) {
		err = "Cannot Initilize\n";
	}

	if (!err) {
		switch_console_printf(SWITCH_CHANNEL_CONSOLE, "Bringing up environment.\n");
		switch_console_printf(SWITCH_CHANNEL_CONSOLE, "Loading Modules.\n");
		if (switch_loadable_module_init() != SWITCH_STATUS_SUCCESS) {
			err = "Cannot load modules";
		}
	}

	if(err) {
		switch_console_printf(SWITCH_CHANNEL_CONSOLE, "Error: %s", err);
		exit(-1);
	}

	switch_console_printf(SWITCH_CHANNEL_CONSOLE, "freeswitch Version %s Started\n\n", SWITCH_GLOBAL_VERSION);

	/* wait for console input */
	switch_console_loop();

	switch_console_printf(SWITCH_CHANNEL_CONSOLE, "Clean up modules.\n");
	loadable_module_shutdown();
	switch_console_printf(SWITCH_CHANNEL_CONSOLE, "Tearing down environment.\n");
	switch_core_destroy();
	switch_console_printf(SWITCH_CHANNEL_CONSOLE, "Exiting Now.\n");
	return 0;

}


