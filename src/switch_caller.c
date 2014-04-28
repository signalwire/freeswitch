/* 
 * FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
 * Copyright (C) 2005-2014, Anthony Minessale II <anthm@freeswitch.org>
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

 * 
 * Anthony Minessale II <anthm@freeswitch.org>
 *
 *
 * switch_caller.c -- Caller Identification
 *
 */

#include <switch.h>
#include <switch_caller.h>

SWITCH_DECLARE(switch_caller_profile_t *) switch_caller_profile_new(switch_memory_pool_t *pool,
																	const char *username,
																	const char *dialplan,
																	const char *caller_id_name,
																	const char *caller_id_number,
																	const char *network_addr,
																	const char *ani,
																	const char *aniii,
																	const char *rdnis,
																	const char *source, const char *context, const char *destination_number)
{
	switch_caller_profile_t *profile = NULL;
	char uuid_str[SWITCH_UUID_FORMATTED_LENGTH + 1];

	profile = switch_core_alloc(pool, sizeof(*profile));
	switch_assert(profile != NULL);
	memset(profile, 0, sizeof(*profile));

	switch_uuid_str(uuid_str, sizeof(uuid_str));
	profile->uuid_str = switch_core_strdup(pool, uuid_str);
	
	if (!context) {
		context = "default";
	}

	if (zstr(caller_id_name)) {
		caller_id_name = SWITCH_DEFAULT_CLID_NAME;
	}

	if (zstr(caller_id_number)) {
		caller_id_number = SWITCH_DEFAULT_CLID_NUMBER;
	}

	/* ANI defaults to Caller ID Number when not specified */
	if (zstr(ani)) {
		ani = caller_id_number;
	}

	profile_dup_clean(username, profile->username, pool);
	profile_dup_clean(dialplan, profile->dialplan, pool);
	profile_dup_clean(caller_id_name, profile->caller_id_name, pool);
	profile_dup_clean(caller_id_number, profile->caller_id_number, pool);
	profile_dup_clean(caller_id_name, profile->orig_caller_id_name, pool);
	profile_dup_clean(caller_id_number, profile->orig_caller_id_number, pool);
	profile->caller_ton = SWITCH_TON_UNDEF;
	profile->caller_numplan = SWITCH_NUMPLAN_UNDEF;
	profile_dup_clean(network_addr, profile->network_addr, pool);
	profile_dup_clean(ani, profile->ani, pool);
	profile->ani_ton = SWITCH_TON_UNDEF;
	profile->ani_numplan = SWITCH_NUMPLAN_UNDEF;
	profile_dup_clean(aniii, profile->aniii, pool);
	profile_dup_clean(rdnis, profile->rdnis, pool);
	profile->rdnis_ton = SWITCH_TON_UNDEF;
	profile->rdnis_numplan = SWITCH_NUMPLAN_UNDEF;
	profile_dup_clean(source, profile->source, pool);
	profile_dup_clean(context, profile->context, pool);
	profile_dup_clean(destination_number, profile->destination_number, pool);
	profile->destination_number_ton = SWITCH_TON_UNDEF;
	profile->destination_number_numplan = SWITCH_NUMPLAN_UNDEF;
	profile->uuid = SWITCH_BLANK_STRING;
	profile->chan_name = SWITCH_BLANK_STRING;
	profile->callee_id_name = SWITCH_BLANK_STRING;
	profile->callee_id_number = SWITCH_BLANK_STRING;
	switch_set_flag(profile, SWITCH_CPF_SCREEN);
	profile->pool = pool;
	return profile;
}

SWITCH_DECLARE(switch_caller_profile_t *) switch_caller_profile_dup(switch_memory_pool_t *pool, switch_caller_profile_t *tocopy)
{
	switch_caller_profile_t *profile = NULL;
	char uuid_str[SWITCH_UUID_FORMATTED_LENGTH + 1];

	profile = switch_core_alloc(pool, sizeof(*profile));
	switch_assert(profile != NULL);

	switch_uuid_str(uuid_str, sizeof(uuid_str));
	profile->uuid_str = switch_core_strdup(pool, uuid_str);
	profile->clone_of = switch_core_strdup(pool, tocopy->uuid_str);

	profile_dup(tocopy->username, profile->username, pool);
	profile_dup(tocopy->dialplan, profile->dialplan, pool);
	profile_dup(tocopy->caller_id_name, profile->caller_id_name, pool);
	profile_dup(tocopy->caller_id_number, profile->caller_id_number, pool);
	profile_dup(tocopy->callee_id_name, profile->callee_id_name, pool);
	profile_dup(tocopy->callee_id_number, profile->callee_id_number, pool);
	profile_dup(tocopy->orig_caller_id_name, profile->orig_caller_id_name, pool);
	profile_dup(tocopy->orig_caller_id_number, profile->orig_caller_id_number, pool);
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
	profile->direction = tocopy->direction;

	if (tocopy->times) {
		profile->old_times = (switch_channel_timetable_t *) switch_core_alloc(profile->pool, sizeof(switch_channel_timetable_t));
		*profile->old_times = *tocopy->times;
	} else {
		tocopy->times = (switch_channel_timetable_t *) switch_core_alloc(tocopy->pool, sizeof(*tocopy->times));
	}

	if (tocopy->soft) {
		profile_node_t *pn;

		for (pn = tocopy->soft; pn; pn = pn->next) {
			profile_node_t *pp, *n = switch_core_alloc(profile->pool, sizeof(*n));

			n->var = switch_core_strdup(profile->pool, pn->var);
			n->val = switch_core_strdup(profile->pool, pn->val);

			if (!profile->soft) {
				profile->soft = n;
			} else {
				for(pp = profile->soft; pp && pp->next; pp = pp->next);
			
				if (pp) {
					pp->next = n;
				}	
			}
		}

	}

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
	if (!strcasecmp(name, "caller_id_number")) {
		return caller_profile->caller_id_number;
	}
	if (!strcasecmp(name, "orig_caller_id_name")) {
		return caller_profile->orig_caller_id_name;
	}
	if (!strcasecmp(name, "orig_caller_id_number")) {
		return caller_profile->orig_caller_id_number;
	}
	if (!strcasecmp(name, "callee_id_name")) {
		return caller_profile->callee_id_name;
	}
	if (!strcasecmp(name, "callee_id_number")) {
		return caller_profile->callee_id_number;
	}
	if (!strcasecmp(name, "ani")) {
		return caller_profile->ani;
	}
	if (!strcasecmp(name, "aniii")) {
		return caller_profile->aniii;
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
	if (!strcasecmp(name, "transfer_source")) {
		return caller_profile->transfer_source;
	}
	if (!strcasecmp(name, "context")) {
		return caller_profile->context;
	}

	if (!strcasecmp(name, "chan_name")) {
		return caller_profile->chan_name;
	}

	if (!strcasecmp(name, "profile_index")) {
		return caller_profile->profile_index;
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
	if (!strcasecmp(name, "screen_bit")) {
		return switch_test_flag(caller_profile, SWITCH_CPF_SCREEN) ? "true" : "false";
	}
	if (!strcasecmp(name, "privacy_hide_name")) {
		return switch_test_flag(caller_profile, SWITCH_CPF_HIDE_NAME) ? "true" : "false";
	}
	if (!strcasecmp(name, "privacy_hide_number")) {
		return switch_test_flag(caller_profile, SWITCH_CPF_HIDE_NUMBER) ? "true" : "false";
	}
	if (!strcasecmp(name, "profile_created_time")) {
		return switch_core_sprintf(caller_profile->pool, "%" SWITCH_TIME_T_FMT, caller_profile->times->profile_created);
	}
	if (!strcasecmp(name, "created_time")) {
		return switch_core_sprintf(caller_profile->pool, "%" SWITCH_TIME_T_FMT, caller_profile->times->created);
	}
	if (!strcasecmp(name, "answered_time")) {
		return switch_core_sprintf(caller_profile->pool, "%" SWITCH_TIME_T_FMT, caller_profile->times->answered);
	}
	if (!strcasecmp(name, "progress_time")) {
		return switch_core_sprintf(caller_profile->pool, "%" SWITCH_TIME_T_FMT, caller_profile->times->progress);
	}
	if (!strcasecmp(name, "progress_media_time")) {
		return switch_core_sprintf(caller_profile->pool, "%" SWITCH_TIME_T_FMT, caller_profile->times->progress_media);
	}
	if (!strcasecmp(name, "hungup_time")) {
		return switch_core_sprintf(caller_profile->pool, "%" SWITCH_TIME_T_FMT, caller_profile->times->hungup);
	}
	if (!strcasecmp(name, "transferred_time")) {
		return switch_core_sprintf(caller_profile->pool, "%" SWITCH_TIME_T_FMT, caller_profile->times->transferred);
	}


	return NULL;
}

SWITCH_DECLARE(void) switch_caller_profile_event_set_data(switch_caller_profile_t *caller_profile, const char *prefix, switch_event_t *event)
{
	char header_name[1024];
	switch_channel_timetable_t *times = NULL;

	switch_snprintf(header_name, sizeof(header_name), "%s-Direction", prefix);
	switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, header_name, caller_profile->direction == SWITCH_CALL_DIRECTION_INBOUND ? 
								   "inbound" : "outbound");

	switch_snprintf(header_name, sizeof(header_name), "%s-Logical-Direction", prefix);
	switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, header_name, caller_profile->logical_direction == SWITCH_CALL_DIRECTION_INBOUND ? 
								   "inbound" : "outbound");
	
	if (!zstr(caller_profile->username)) {
		switch_snprintf(header_name, sizeof(header_name), "%s-Username", prefix);
		switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, header_name, caller_profile->username);
	}
	if (!zstr(caller_profile->dialplan)) {
		switch_snprintf(header_name, sizeof(header_name), "%s-Dialplan", prefix);
		switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, header_name, caller_profile->dialplan);
	}
	if (!zstr(caller_profile->caller_id_name)) {
		switch_snprintf(header_name, sizeof(header_name), "%s-Caller-ID-Name", prefix);
		switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, header_name, caller_profile->caller_id_name);
	}
	if (!zstr(caller_profile->caller_id_number)) {
		switch_snprintf(header_name, sizeof(header_name), "%s-Caller-ID-Number", prefix);
		switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, header_name, caller_profile->caller_id_number);
	}
	if (!zstr(caller_profile->caller_id_name)) {
		switch_snprintf(header_name, sizeof(header_name), "%s-Orig-Caller-ID-Name", prefix);
		switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, header_name, caller_profile->orig_caller_id_name);
	}
	if (!zstr(caller_profile->caller_id_number)) {
		switch_snprintf(header_name, sizeof(header_name), "%s-Orig-Caller-ID-Number", prefix);
		switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, header_name, caller_profile->orig_caller_id_number);
	}
	if (!zstr(caller_profile->callee_id_name)) {
		switch_snprintf(header_name, sizeof(header_name), "%s-Callee-ID-Name", prefix);
		switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, header_name, caller_profile->callee_id_name);
	}
	if (!zstr(caller_profile->callee_id_number)) {
		switch_snprintf(header_name, sizeof(header_name), "%s-Callee-ID-Number", prefix);
		switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, header_name, caller_profile->callee_id_number);
	}
	if (!zstr(caller_profile->network_addr)) {
		switch_snprintf(header_name, sizeof(header_name), "%s-Network-Addr", prefix);
		switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, header_name, caller_profile->network_addr);
	}
	if (!zstr(caller_profile->ani)) {
		switch_snprintf(header_name, sizeof(header_name), "%s-ANI", prefix);
		switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, header_name, caller_profile->ani);
	}
	if (!zstr(caller_profile->aniii)) {
		switch_snprintf(header_name, sizeof(header_name), "%s-ANI-II", prefix);
		switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, header_name, caller_profile->aniii);
	}
	if (!zstr(caller_profile->destination_number)) {
		switch_snprintf(header_name, sizeof(header_name), "%s-Destination-Number", prefix);
		switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, header_name, caller_profile->destination_number);
	}
	if (!zstr(caller_profile->uuid)) {
		switch_snprintf(header_name, sizeof(header_name), "%s-Unique-ID", prefix);
		switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, header_name, caller_profile->uuid);
	}
	if (!zstr(caller_profile->source)) {
		switch_snprintf(header_name, sizeof(header_name), "%s-Source", prefix);
		switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, header_name, caller_profile->source);
	}
	if (!zstr(caller_profile->transfer_source)) {
		switch_snprintf(header_name, sizeof(header_name), "%s-Transfer-Source", prefix);
		switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, header_name, caller_profile->transfer_source);
	}
	if (!zstr(caller_profile->context)) {
		switch_snprintf(header_name, sizeof(header_name), "%s-Context", prefix);
		switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, header_name, caller_profile->context);
	}
	if (!zstr(caller_profile->rdnis)) {
		switch_snprintf(header_name, sizeof(header_name), "%s-RDNIS", prefix);
		switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, header_name, caller_profile->rdnis);
	}
	if (!zstr(caller_profile->chan_name)) {
		switch_snprintf(header_name, sizeof(header_name), "%s-Channel-Name", prefix);
		switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, header_name, caller_profile->chan_name);
	}
	if (!zstr(caller_profile->profile_index)) {
		switch_snprintf(header_name, sizeof(header_name), "%s-Profile-Index", prefix);
		switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, header_name, caller_profile->profile_index);
	}

	if (caller_profile->soft) {
		profile_node_t *pn;

		for (pn = caller_profile->soft; pn; pn = pn->next) {
			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, pn->var, pn->val);
		}

	}
	
	if (!(times = caller_profile->times)) {
		times = caller_profile->old_times;
	}


	if (times) {
		switch_snprintf(header_name, sizeof(header_name), "%s-Profile-Created-Time", prefix);
		switch_event_add_header(event, SWITCH_STACK_BOTTOM, header_name, "%" SWITCH_TIME_T_FMT, times->profile_created);
		switch_snprintf(header_name, sizeof(header_name), "%s-Channel-Created-Time", prefix);
		switch_event_add_header(event, SWITCH_STACK_BOTTOM, header_name, "%" SWITCH_TIME_T_FMT, times->created);
		switch_snprintf(header_name, sizeof(header_name), "%s-Channel-Answered-Time", prefix);
		switch_event_add_header(event, SWITCH_STACK_BOTTOM, header_name, "%" SWITCH_TIME_T_FMT, times->answered);
		switch_snprintf(header_name, sizeof(header_name), "%s-Channel-Progress-Time", prefix);
		switch_event_add_header(event, SWITCH_STACK_BOTTOM, header_name, "%" SWITCH_TIME_T_FMT, times->progress);
		switch_snprintf(header_name, sizeof(header_name), "%s-Channel-Progress-Media-Time", prefix);
		switch_event_add_header(event, SWITCH_STACK_BOTTOM, header_name, "%" SWITCH_TIME_T_FMT, times->progress_media);
		switch_snprintf(header_name, sizeof(header_name), "%s-Channel-Hangup-Time", prefix);
		switch_event_add_header(event, SWITCH_STACK_BOTTOM, header_name, "%" SWITCH_TIME_T_FMT, times->hungup);
		switch_snprintf(header_name, sizeof(header_name), "%s-Channel-Transfer-Time", prefix);
		switch_event_add_header(event, SWITCH_STACK_BOTTOM, header_name, "%" SWITCH_TIME_T_FMT, times->transferred);
		switch_snprintf(header_name, sizeof(header_name), "%s-Channel-Resurrect-Time", prefix);
		switch_event_add_header(event, SWITCH_STACK_BOTTOM, header_name, "%" SWITCH_TIME_T_FMT, times->resurrected);
		switch_snprintf(header_name, sizeof(header_name), "%s-Channel-Bridged-Time", prefix);
		switch_event_add_header(event, SWITCH_STACK_BOTTOM, header_name, "%" SWITCH_TIME_T_FMT, times->bridged);
		switch_snprintf(header_name, sizeof(header_name), "%s-Channel-Last-Hold", prefix);
		switch_event_add_header(event, SWITCH_STACK_BOTTOM, header_name, "%" SWITCH_TIME_T_FMT, times->last_hold);
		switch_snprintf(header_name, sizeof(header_name), "%s-Channel-Hold-Accum", prefix);
		switch_event_add_header(event, SWITCH_STACK_BOTTOM, header_name, "%" SWITCH_TIME_T_FMT, times->hold_accum);
	}

	switch_snprintf(header_name, sizeof(header_name), "%s-Screen-Bit", prefix);
	switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, header_name, switch_test_flag(caller_profile, SWITCH_CPF_SCREEN) ? "true" : "false");

	switch_snprintf(header_name, sizeof(header_name), "%s-Privacy-Hide-Name", prefix);
	switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, header_name, switch_test_flag(caller_profile, SWITCH_CPF_HIDE_NAME) ? "true" : "false");

	switch_snprintf(header_name, sizeof(header_name), "%s-Privacy-Hide-Number", prefix);
	switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, header_name, switch_test_flag(caller_profile, SWITCH_CPF_HIDE_NUMBER) ? "true" : "false");
}

SWITCH_DECLARE(switch_status_t) switch_caller_extension_clone(switch_caller_extension_t **new_ext, switch_caller_extension_t *orig,
															  switch_memory_pool_t *pool)
{
	switch_caller_extension_t *caller_extension = NULL;
	switch_caller_application_t *caller_application = NULL, *ap = NULL;

	*new_ext = NULL;

	if ((caller_extension = switch_core_alloc(pool, sizeof(switch_caller_extension_t))) != 0) {
		int match = 0;

		caller_extension->extension_name = switch_core_strdup(pool, orig->extension_name);
		caller_extension->extension_number = switch_core_strdup(pool, orig->extension_number);

		for (ap = orig->applications; ap; ap = ap->next) {

			if (!match) {
				if (ap == orig->current_application) {
					match++;
				} else {
					continue;
				}
			}
			caller_application = switch_core_alloc(pool, sizeof(switch_caller_application_t));

			caller_application->application_name = switch_core_strdup(pool, ap->application_name);
			caller_application->application_data = switch_core_strdup(pool, ap->application_data);

			if (!caller_extension->applications) {
				caller_extension->applications = caller_application;
			} else if (caller_extension->last_application) {
				caller_extension->last_application->next = caller_application;
			}

			caller_extension->last_application = caller_application;

			if (ap == orig->current_application) {
				caller_extension->current_application = caller_application;
			}
		}

		*new_ext = caller_extension;

		return SWITCH_STATUS_SUCCESS;
	}


	return SWITCH_STATUS_MEMERR;
}

SWITCH_DECLARE(switch_caller_extension_t *) switch_caller_extension_new(switch_core_session_t *session, const char *extension_name,
																		const char *extension_number)
{
	switch_caller_extension_t *caller_extension = NULL;

	if ((caller_extension = switch_core_session_alloc(session, sizeof(switch_caller_extension_t))) != 0) {
		caller_extension->extension_name = switch_core_session_strdup(session, extension_name);
		caller_extension->extension_number = switch_core_session_strdup(session, extension_number);
		caller_extension->current_application = caller_extension->last_application = caller_extension->applications;
	}

	return caller_extension;
}


SWITCH_DECLARE(void) switch_caller_extension_add_application_printf(switch_core_session_t *session,
																	switch_caller_extension_t *caller_extension, const char *application_name,
																	const char *fmt, ...)
{
	va_list ap;
	char *data = NULL;

	va_start(ap, fmt);
	if ( switch_vasprintf(&data, fmt, ap) != -1 ) {
		if (strstr(data, "\\'")) {
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_WARNING, "App not added, Invalid character sequence in data string [%s]\n",
							  data);
		} else {
			switch_caller_extension_add_application(session, caller_extension, application_name, data);
		}
	}
	va_end(ap);

	switch_safe_free(data);
}


SWITCH_DECLARE(void) switch_caller_extension_add_application(switch_core_session_t *session,
															 switch_caller_extension_t *caller_extension, const char *application_name,
															 const char *application_data)
{
	switch_caller_application_t *caller_application = NULL;

	switch_assert(session != NULL);

	if ((caller_application = switch_core_session_alloc(session, sizeof(switch_caller_application_t))) != 0) {
		caller_application->application_name = switch_core_session_strdup(session, application_name);
		caller_application->application_data = switch_core_session_strdup(session, application_data);



		if (caller_application->application_data && strstr(caller_application->application_data, "\\'")) {
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_WARNING, "App not added, Invalid character sequence in data string [%s]\n", 
							  caller_application->application_data);
			return;
		}
		
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
 * vim:set softtabstop=4 shiftwidth=4 tabstop=4 noet:
 */
