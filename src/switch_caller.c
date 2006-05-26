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
 * switch_caller.c -- Caller Identification
 *
 */
#include <switch.h>
#include <switch_caller.h>

SWITCH_DECLARE(switch_caller_profile_t *) switch_caller_profile_new(switch_memory_pool_t *pool,
																	char *username,
																	char *dialplan,
																	char *caller_id_name,
																	char *caller_id_number,
																	char *network_addr,
																	char *ani,
																	char *ani2, 
																	char *rdnis,
																	char *source,
																	char *context,
																	char *destination_number)
{


	switch_caller_profile_t *profile = NULL;

	if ((profile = switch_core_alloc(pool, sizeof(switch_caller_profile_t))) != 0) {
		if (!context) {
			context = "default";
		}
		profile->username = switch_core_strdup(pool, username);
		profile->dialplan = switch_core_strdup(pool, dialplan);
		profile->caller_id_name = switch_core_strdup(pool, caller_id_name);
		profile->caller_id_number = switch_core_strdup(pool, caller_id_number);
		profile->network_addr = switch_core_strdup(pool, network_addr);
		profile->ani = switch_core_strdup(pool, ani);
		profile->ani2 = switch_core_strdup(pool, ani2);
		profile->rdnis = switch_core_strdup(pool, rdnis);
		profile->source = switch_core_strdup(pool, source);
		profile->context = switch_core_strdup(pool, context);
		profile->destination_number = switch_core_strdup(pool, destination_number);
	}

	return profile;
}


SWITCH_DECLARE(switch_caller_profile_t *) switch_caller_profile_clone(switch_core_session_t *session, switch_caller_profile_t *tocopy)
																	
{
	switch_caller_profile_t *profile = NULL;
	if ((profile = switch_core_session_alloc(session, sizeof(switch_caller_profile_t))) != 0) {
		profile->username = switch_core_session_strdup(session, tocopy->username);
		profile->dialplan = switch_core_session_strdup(session, tocopy->dialplan);
		profile->caller_id_name = switch_core_session_strdup(session, tocopy->caller_id_name);
		profile->ani = switch_core_session_strdup(session, tocopy->ani);
		profile->ani2 = switch_core_session_strdup(session, tocopy->ani2);
		profile->caller_id_number = switch_core_session_strdup(session, tocopy->caller_id_number);
		profile->network_addr = switch_core_session_strdup(session, tocopy->network_addr);
		profile->rdnis = switch_core_session_strdup(session, tocopy->rdnis);
		profile->destination_number = switch_core_session_strdup(session, tocopy->destination_number);
		profile->uuid = switch_core_session_strdup(session, tocopy->uuid);
		profile->source = switch_core_session_strdup(session, tocopy->source);
		profile->context = switch_core_session_strdup(session, tocopy->context);
		profile->chan_name = switch_core_session_strdup(session, tocopy->chan_name);
	}

	return profile;
}

SWITCH_DECLARE(char *) switch_caller_get_field_by_name(switch_caller_profile_t *caller_profile, char *name)
{
	if (!strcasecmp(name, "dialplan")) {
		return caller_profile->dialplan;
	}
	if (!strcasecmp(name, "username")) {
		return caller_profile->username;
	}
	if (!strcasecmp(name, "caller_id_name")) {
		return caller_profile->caller_id_name;
	}
	if (!strcasecmp(name, "ani")) {
		return caller_profile->ani;
	}
	if (!strcasecmp(name, "ani2")) {
		return caller_profile->ani2;
	}
	if (!strcasecmp(name, "caller_id_number")) {
		return caller_profile->caller_id_number;
	}
	if (!strcasecmp(name, "network_addr")) {
		return caller_profile->network_addr;
	}
	if (!strcasecmp(name, "rdnis")) {
		return caller_profile->rdnis;
	}
	if (!strcasecmp(name, "destination_number")) {
		return caller_profile->destination_number;
	}
	if (!strcasecmp(name, "uuid")) {
		return caller_profile->uuid;
	}
	if (!strcasecmp(name, "source")) {
		return caller_profile->source;
	}
	if (!strcasecmp(name, "context")) {
		return caller_profile->context;
	}
	if (!strcasecmp(name, "chan_name")) {
		return caller_profile->chan_name;
	}
	return NULL;
}

SWITCH_DECLARE(void) switch_caller_profile_event_set_data(switch_caller_profile_t *caller_profile, char *prefix,
														  switch_event_t *event)
{
	char header_name[1024];


	if (caller_profile->username) {
		snprintf(header_name, sizeof(header_name), "%s-Username", prefix);
		switch_event_add_header(event, SWITCH_STACK_BOTTOM, header_name, caller_profile->username);
	}
	if (caller_profile->dialplan) {
		snprintf(header_name, sizeof(header_name), "%s-Dialplan", prefix);
		switch_event_add_header(event, SWITCH_STACK_BOTTOM, header_name, caller_profile->dialplan);
	}
	if (caller_profile->caller_id_name) {
		snprintf(header_name, sizeof(header_name), "%s-Caller-ID-Name", prefix);
		switch_event_add_header(event, SWITCH_STACK_BOTTOM, header_name, caller_profile->caller_id_name);
	}
	if (caller_profile->caller_id_number) {
		snprintf(header_name, sizeof(header_name), "%s-Caller-ID-Number", prefix);
		switch_event_add_header(event, SWITCH_STACK_BOTTOM, header_name, caller_profile->caller_id_number);
	}
	if (caller_profile->network_addr) {
		snprintf(header_name, sizeof(header_name), "%s-Network-Addr", prefix);
		switch_event_add_header(event, SWITCH_STACK_BOTTOM, header_name, caller_profile->network_addr);
	}
	if (caller_profile->ani) {
		snprintf(header_name, sizeof(header_name), "%s-ANI", prefix);
		switch_event_add_header(event, SWITCH_STACK_BOTTOM, header_name, caller_profile->ani);
	}
	if (caller_profile->ani2) {
		snprintf(header_name, sizeof(header_name), "%s-ANI2", prefix);
		switch_event_add_header(event, SWITCH_STACK_BOTTOM, header_name, caller_profile->ani2);
	}
	if (caller_profile->destination_number) {
		snprintf(header_name, sizeof(header_name), "%s-Destination-Number", prefix);
		switch_event_add_header(event, SWITCH_STACK_BOTTOM, header_name, caller_profile->destination_number);
	}
	if (caller_profile->uuid) {
		snprintf(header_name, sizeof(header_name), "%s-Unique-ID", prefix);
		switch_event_add_header(event, SWITCH_STACK_BOTTOM, header_name, caller_profile->uuid);
	}
	if (caller_profile->source) {
		snprintf(header_name, sizeof(header_name), "%s-Source", prefix);
		switch_event_add_header(event, SWITCH_STACK_BOTTOM, header_name, caller_profile->source);
	}
	if (caller_profile->context) {
		snprintf(header_name, sizeof(header_name), "%s-Context", prefix);
		switch_event_add_header(event, SWITCH_STACK_BOTTOM, header_name, caller_profile->context);
	}
	if (caller_profile->rdnis) {
		snprintf(header_name, sizeof(header_name), "%s-RDNIS", prefix);
		switch_event_add_header(event, SWITCH_STACK_BOTTOM, header_name, caller_profile->rdnis);
	}
	if (caller_profile->chan_name) {
		snprintf(header_name, sizeof(header_name), "%s-Channel-Name", prefix);
		switch_event_add_header(event, SWITCH_STACK_BOTTOM, header_name, caller_profile->chan_name);
	}

}

SWITCH_DECLARE(switch_caller_extension_t *) switch_caller_extension_new(switch_core_session_t *session,
																	  char *extension_name, char *extension_number)
{
	switch_caller_extension_t *caller_extension = NULL;

	if ((caller_extension = switch_core_session_alloc(session, sizeof(switch_caller_extension_t))) != 0) {
		caller_extension->extension_name = switch_core_session_strdup(session, extension_name);
		caller_extension->extension_number = switch_core_session_strdup(session, extension_number);
		caller_extension->current_application = caller_extension->last_application = caller_extension->applications;
	}

	return caller_extension;
}


SWITCH_DECLARE(void) switch_caller_extension_add_application(switch_core_session_t *session,
															 switch_caller_extension_t *caller_extension,
															 char *application_name, char *application_data)
{
	switch_caller_application_t *caller_application = NULL;

	assert(session != NULL);

	if ((caller_application = switch_core_session_alloc(session, sizeof(switch_caller_application_t))) != 0) {
		caller_application->application_name = switch_core_session_strdup(session, application_name);
		caller_application->application_data = switch_core_session_strdup(session, application_data);
		if (!caller_extension->applications) {
			caller_extension->applications = caller_application;
		} else if (caller_extension->last_application) {
			caller_extension->last_application->next = caller_application;
		}

		caller_extension->last_application = caller_application;
		caller_extension->current_application = caller_extension->applications;
	}

}
