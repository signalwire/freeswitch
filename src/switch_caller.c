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

 * 
 * Anthony Minessale II <anthmct@yahoo.com>
 *
 *
 * switch_caller.c -- Caller Identification
 *
 */
#include <switch.h>
#include <switch_caller.h>


#define profile_dup(a,b,p) if (!switch_strlen_zero(a)) { b = switch_core_strdup(p, a); } else { b = SWITCH_BLANK_STRING; }
#define profile_dup_clean(a,b,p) if (!switch_strlen_zero(a)) { b = switch_clean_string(switch_core_strdup(p, a)); } else { b = SWITCH_BLANK_STRING; }

SWITCH_DECLARE(switch_caller_profile_t *) switch_caller_profile_new(switch_memory_pool_t *pool,
																	const char *username,
																	const char *dialplan,
																	const char *caller_id_name,
																	const char *caller_id_number,
																	const char *network_addr,
																	const char *ani,
																	const char *aniii,
																	const char *rdnis, 
																	const char *source, 
																	const char *context,
																	const char *destination_number)
{


	switch_caller_profile_t *profile = NULL;

	profile = switch_core_alloc(pool, sizeof(*profile));
	switch_assert(profile != NULL);
		
	if (!context) {
		context = "default";
	}

	profile_dup_clean(username, profile->username, pool);
	profile_dup_clean(dialplan, profile->dialplan, pool);
	profile_dup_clean(caller_id_name, profile->caller_id_name, pool);
	profile_dup_clean(caller_id_number, profile->caller_id_number, pool);
	profile_dup_clean(network_addr, profile->network_addr, pool);
	profile_dup_clean(ani, profile->ani, pool);
	profile_dup_clean(aniii, profile->aniii, pool);
	profile_dup_clean(rdnis, profile->rdnis, pool);
	profile_dup_clean(source, profile->source, pool);
	profile_dup_clean(context, profile->context, pool);
	profile_dup_clean(destination_number, profile->destination_number, pool);
	profile->uuid = SWITCH_BLANK_STRING;
	profile->chan_name = SWITCH_BLANK_STRING;

	switch_set_flag(profile, SWITCH_CPF_SCREEN);
	profile->pool = pool;
	return profile;
}


SWITCH_DECLARE(switch_caller_profile_t *) switch_caller_profile_dup(switch_memory_pool_t *pool, switch_caller_profile_t *tocopy)
{
	switch_caller_profile_t *profile = NULL;

	profile = switch_core_alloc(pool, sizeof(*profile));
	switch_assert(profile != NULL);

	profile_dup(tocopy->username, profile->username, pool);
	profile_dup(tocopy->dialplan, profile->dialplan, pool);
	profile_dup(tocopy->caller_id_name, profile->caller_id_name, pool);
	profile_dup(tocopy->caller_id_number, profile->caller_id_number, pool);
	profile_dup(tocopy->network_addr, profile->network_addr, pool);
	profile_dup(tocopy->ani, profile->ani, pool);
	profile_dup(tocopy->aniii, profile->aniii, pool);
	profile_dup(tocopy->rdnis, profile->rdnis, pool);
	profile_dup(tocopy->source, profile->source, pool);
	profile_dup(tocopy->context, profile->context, pool);
	profile_dup(tocopy->destination_number, profile->destination_number, pool);
	profile_dup(tocopy->uuid, profile->uuid, pool);
	profile_dup(tocopy->chan_name, profile->chan_name, pool);

	profile->caller_ton = tocopy->caller_ton;
	profile->caller_numplan = tocopy->caller_numplan;
	profile->ani_ton = tocopy->ani_ton;
	profile->ani_numplan = tocopy->ani_numplan;
	profile->rdnis_ton = tocopy->rdnis_ton;
	profile->rdnis_numplan = tocopy->rdnis_numplan;
	profile->destination_number_ton = tocopy->destination_number_ton;
	profile->destination_number_numplan = tocopy->destination_number_numplan;
	profile->flags = tocopy->flags;
	profile->pool = pool;

	return profile;
}


SWITCH_DECLARE(switch_caller_profile_t *) switch_caller_profile_clone(switch_core_session_t *session, switch_caller_profile_t *tocopy)
{
	switch_memory_pool_t *pool;

	pool = switch_core_session_get_pool(session);

	return switch_caller_profile_dup(pool, tocopy);
}


SWITCH_DECLARE(const char *) switch_caller_get_field_by_name(switch_caller_profile_t *caller_profile, const char *name)
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
	if (!strcasecmp(name, "aniii")) {
		return caller_profile->aniii;
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
	if (!strcasecmp(name, "caller_ton")) {
		return switch_core_sprintf(caller_profile->pool, "%u", caller_profile->caller_ton);
	}
	if (!strcasecmp(name, "caller_numplan")) {
		return switch_core_sprintf(caller_profile->pool, "%u", caller_profile->caller_numplan);
	}
	if (!strcasecmp(name, "destination_number_ton")) {
		return switch_core_sprintf(caller_profile->pool, "%u", caller_profile->destination_number_ton);
	}
	if (!strcasecmp(name, "destination_number_numplan")) {
		return switch_core_sprintf(caller_profile->pool, "%u", caller_profile->destination_number_numplan);
	}
	if (!strcasecmp(name, "ani_ton")) {
		return switch_core_sprintf(caller_profile->pool, "%u", caller_profile->ani_ton);
	}
	if (!strcasecmp(name, "ani_numplan")) {
		return switch_core_sprintf(caller_profile->pool, "%u", caller_profile->ani_numplan);
	}
	if (!strcasecmp(name, "rdnis_ton")) {
		return switch_core_sprintf(caller_profile->pool, "%u", caller_profile->rdnis_ton);
	}
	if (!strcasecmp(name, "rdnis_numplan")) {
		return switch_core_sprintf(caller_profile->pool, "%u", caller_profile->rdnis_numplan);
	}
	return NULL;
}

SWITCH_DECLARE(void) switch_caller_profile_event_set_data(switch_caller_profile_t *caller_profile, const char *prefix, switch_event_t *event)
{
	char header_name[1024];


	if (!switch_strlen_zero(caller_profile->username)) {
		snprintf(header_name, sizeof(header_name), "%s-Username", prefix);
		switch_event_add_header(event, SWITCH_STACK_BOTTOM, header_name, "%s", caller_profile->username);
	}
	if (!switch_strlen_zero(caller_profile->dialplan)) {
		snprintf(header_name, sizeof(header_name), "%s-Dialplan", prefix);
		switch_event_add_header(event, SWITCH_STACK_BOTTOM, header_name, "%s", caller_profile->dialplan);
	}
	if (!switch_strlen_zero(caller_profile->caller_id_name)) {
		snprintf(header_name, sizeof(header_name), "%s-Caller-ID-Name", prefix);
		switch_event_add_header(event, SWITCH_STACK_BOTTOM, header_name, "%s", caller_profile->caller_id_name);
	}
	if (!switch_strlen_zero(caller_profile->caller_id_number)) {
		snprintf(header_name, sizeof(header_name), "%s-Caller-ID-Number", prefix);
		switch_event_add_header(event, SWITCH_STACK_BOTTOM, header_name, "%s", caller_profile->caller_id_number);
	}
	if (!switch_strlen_zero(caller_profile->network_addr)) {
		snprintf(header_name, sizeof(header_name), "%s-Network-Addr", prefix);
		switch_event_add_header(event, SWITCH_STACK_BOTTOM, header_name, "%s", caller_profile->network_addr);
	}
	if (!switch_strlen_zero(caller_profile->ani)) {
		snprintf(header_name, sizeof(header_name), "%s-ANI", prefix);
		switch_event_add_header(event, SWITCH_STACK_BOTTOM, header_name, "%s", caller_profile->ani);
	}
	if (!switch_strlen_zero(caller_profile->aniii)) {
		snprintf(header_name, sizeof(header_name), "%s-ANI-II", prefix);
		switch_event_add_header(event, SWITCH_STACK_BOTTOM, header_name, "%s", caller_profile->aniii);
	}
	if (!switch_strlen_zero(caller_profile->destination_number)) {
		snprintf(header_name, sizeof(header_name), "%s-Destination-Number", prefix);
		switch_event_add_header(event, SWITCH_STACK_BOTTOM, header_name, "%s", caller_profile->destination_number);
	}
	if (!switch_strlen_zero(caller_profile->uuid)) {
		snprintf(header_name, sizeof(header_name), "%s-Unique-ID", prefix);
		switch_event_add_header(event, SWITCH_STACK_BOTTOM, header_name, "%s", caller_profile->uuid);
	}
	if (!switch_strlen_zero(caller_profile->source)) {
		snprintf(header_name, sizeof(header_name), "%s-Source", prefix);
		switch_event_add_header(event, SWITCH_STACK_BOTTOM, header_name, "%s", caller_profile->source);
	}
	if (!switch_strlen_zero(caller_profile->context)) {
		snprintf(header_name, sizeof(header_name), "%s-Context", prefix);
		switch_event_add_header(event, SWITCH_STACK_BOTTOM, header_name, "%s", caller_profile->context);
	}
	if (!switch_strlen_zero(caller_profile->rdnis)) {
		snprintf(header_name, sizeof(header_name), "%s-RDNIS", prefix);
		switch_event_add_header(event, SWITCH_STACK_BOTTOM, header_name, "%s", caller_profile->rdnis);
	}
	if (!switch_strlen_zero(caller_profile->chan_name)) {
		snprintf(header_name, sizeof(header_name), "%s-Channel-Name", prefix);
		switch_event_add_header(event, SWITCH_STACK_BOTTOM, header_name, "%s", caller_profile->chan_name);
	}
	if (caller_profile->times) {
		snprintf(header_name, sizeof(header_name), "%s-Channel-Created-Time", prefix);
		switch_event_add_header(event, SWITCH_STACK_BOTTOM, header_name, "%" SWITCH_TIME_T_FMT, caller_profile->times->created);
		snprintf(header_name, sizeof(header_name), "%s-Channel-Answered-Time", prefix);
		switch_event_add_header(event, SWITCH_STACK_BOTTOM, header_name, "%" SWITCH_TIME_T_FMT, caller_profile->times->answered);
		snprintf(header_name, sizeof(header_name), "%s-Channel-Hangup-Time", prefix);
		switch_event_add_header(event, SWITCH_STACK_BOTTOM, header_name, "%" SWITCH_TIME_T_FMT, caller_profile->times->hungup);
		snprintf(header_name, sizeof(header_name), "%s-Channel-Transfer-Time", prefix);
		switch_event_add_header(event, SWITCH_STACK_BOTTOM, header_name, "%" SWITCH_TIME_T_FMT, caller_profile->times->transferred);
	}

	snprintf(header_name, sizeof(header_name), "%s-Screen-Bit", prefix);
	switch_event_add_header(event, SWITCH_STACK_BOTTOM, header_name, switch_test_flag(caller_profile, SWITCH_CPF_SCREEN) ? "yes" : "no");

	snprintf(header_name, sizeof(header_name), "%s-Privacy-Hide-Name", prefix);
	switch_event_add_header(event, SWITCH_STACK_BOTTOM, header_name, switch_test_flag(caller_profile, SWITCH_CPF_HIDE_NAME) ? "yes" : "no");

	snprintf(header_name, sizeof(header_name), "%s-Privacy-Hide-Number", prefix);
	switch_event_add_header(event, SWITCH_STACK_BOTTOM, header_name, switch_test_flag(caller_profile, SWITCH_CPF_HIDE_NUMBER) ? "yes" : "no");

	

}

SWITCH_DECLARE(switch_caller_extension_t *) switch_caller_extension_new(switch_core_session_t *session, const char *extension_name, const char *extension_number)
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
															 switch_caller_extension_t *caller_extension, const char *application_name, const char *application_data)
{
	switch_caller_application_t *caller_application = NULL;

	switch_assert(session != NULL);

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

/* For Emacs:
 * Local Variables:
 * mode:c
 * indent-tabs-mode:t
 * tab-width:4
 * c-basic-offset:4
 * End:
 * For VIM:
 * vim:set softtabstop=4 shiftwidth=4 tabstop=4 expandtab:
 */
