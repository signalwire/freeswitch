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
 * switch_caller.h -- Caller Identification
 *
 */
#ifndef SWITCH_CALLER_H
#define SWITCH_CALLER_H

#ifdef __cplusplus
extern "C" {
#endif

#include <switch.h>

struct switch_caller_step {
	char *step_name;
	struct switch_caller_step *next_step;
};

struct switch_caller_profile {
	char *dialplan;
	char *caller_id_name;
	char *caller_id_number;
	char *ani;
	char *ani2;
	char *destination_number;
	struct switch_caller_step *steps;
};

struct switch_caller_application {
	char *application_name;
	char *application_data;
	switch_application_function application_function;
	struct switch_caller_application *next;
};

struct switch_caller_extension {
	char *extension_name;
	char *extension_number;
	struct switch_caller_application *current_application;
	struct switch_caller_application *last_application;
	struct switch_caller_application *applications;
};

SWITCH_DECLARE(switch_caller_extension *) switch_caller_extension_new(switch_core_session *session,
											   char *extension_name,
											   char *extension_number
											   );

SWITCH_DECLARE(void) switch_caller_extension_add_application(switch_core_session *session,
										  switch_caller_extension *caller_extension,
										  char *application_name,
										  char *extra_data);


SWITCH_DECLARE(switch_caller_profile *) switch_caller_profile_new(switch_core_session *session,
					   char *dialplan,
					   char *caller_id_name,
					   char *caller_id_number,
					   char *ani,
					   char *ani2,
					   char *destination_number);

SWITCH_DECLARE(switch_caller_profile *) switch_caller_profile_clone(switch_core_session *session,
					     switch_caller_profile *tocopy);



#ifdef __cplusplus
}
#endif


#endif


