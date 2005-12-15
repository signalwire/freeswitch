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
 * mod_event_test.c -- Framework Demo Module
 *
 */
#include <switch.h>

static const char modname[] = "mod_event_test";

static void event_handler (switch_event *event)
{
	switch_console_printf(SWITCH_CHANNEL_CONSOLE,"\n*** EVENT ***\nEventName: %s\nSubclassOwner: %s\nSubclassName: %s\nEventData: %s\n\n", 
						  switch_event_name(event->event),
						  event->subclass->owner,
						  event->subclass->name,
						  event->data);
}

static switch_loadable_module_interface event_test_module_interface = {
	/*.module_name*/			modname,
	/*.endpoint_interface*/		NULL,
	/*.timer_interface*/		NULL,
	/*.dialplan_interface*/		NULL,
	/*.codec_interface*/		NULL,
	/*.application_interface*/	NULL
};

#define MY_EVENT_COOL "test::cool"

SWITCH_MOD_DECLARE(switch_status) switch_module_load(switch_loadable_module_interface **interface, char *filename) {
	/* connect my internal structure to the blank pointer passed to me */
	*interface = &event_test_module_interface;

	if (switch_event_reserve_subclass(MY_EVENT_COOL) != SWITCH_STATUS_SUCCESS) {
		switch_console_printf(SWITCH_CHANNEL_CONSOLE, "Couldn't register subclass!");
		return SWITCH_STATUS_GENERR;
	}

	if (switch_event_bind((char *)modname, SWITCH_EVENT_ALL, SWITCH_EVENT_SUBCLASS_ANY, event_handler, NULL) != SWITCH_STATUS_SUCCESS) {
		switch_console_printf(SWITCH_CHANNEL_CONSOLE, "Couldn't bind!\n");
		return SWITCH_STATUS_GENERR;
	}

	/* indicate that the module should continue to be loaded */
	return SWITCH_STATUS_SUCCESS;
}

//#define TORTURE_ME

#ifdef TORTURE_ME
SWITCH_MOD_DECLARE(switch_status) switch_module_runtime(void)
{
	for(;;) {
		int x;
		for(x = 0; x < 100; x++) {
			switch_event_fire_subclass(SWITCH_EVENT_CUSTOM, MY_EVENT_COOL, "hello world");
		}
		switch_yield(100000);
	}
}
#endif
