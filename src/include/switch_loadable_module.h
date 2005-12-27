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
 * switch_loadable_module.h -- Loadable Modules
 *
 */
#ifndef SWITCH_LOADABLE_MODULE_H
#define SWITCH_LOADABLE_MODULE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <switch.h>


struct switch_loadable_module_interface {
	const char *module_name;
	const switch_endpoint_interface *endpoint_interface;
	const switch_timer_interface *timer_interface;
	const switch_dialplan_interface *dialplan_interface;
	const switch_codec_interface *codec_interface;
	const switch_application_interface *application_interface;
	const switch_api_interface *api_interface;
	const switch_file_interface *file_interface;
};

SWITCH_DECLARE(switch_status) switch_loadable_module_init(void);
SWITCH_DECLARE(switch_endpoint_interface *) loadable_module_get_endpoint_interface(char *name);
SWITCH_DECLARE(switch_codec_interface *) loadable_module_get_codec_interface(char *name);
SWITCH_DECLARE(switch_dialplan_interface *) loadable_module_get_dialplan_interface(char *name);
SWITCH_DECLARE(switch_timer_interface *) loadable_module_get_timer_interface(char *name);
SWITCH_DECLARE(switch_application_interface *) loadable_module_get_application_interface(char *name);
SWITCH_DECLARE(switch_api_interface *) loadable_module_get_api_interface(char *name);
SWITCH_DECLARE(int) loadable_module_get_codecs(switch_memory_pool *pool, switch_codec_interface **array, int arraylen);
SWITCH_DECLARE(int) loadable_module_get_codecs_sorted(switch_memory_pool *pool, switch_codec_interface **array, int arraylen, char **prefs, int preflen);
SWITCH_DECLARE(switch_status) switch_api_execute(char *cmd, char *arg, char *retbuf, size_t len);
SWITCH_DECLARE(switch_file_interface *) loadable_module_get_file_interface(char *name);
SWITCH_DECLARE(void) loadable_module_shutdown(void);

#ifdef __cplusplus
}
#endif

#endif
