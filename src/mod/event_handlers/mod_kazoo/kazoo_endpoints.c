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
 * Karl Anderson <karl@2600hz.com>
 * Darren Schreiber <darren@2600hz.com>
 *
 *
 * kazoo_dptools.c -- clones of mod_dptools commands slightly modified for kazoo
 *
 */
#include "mod_kazoo.h"

/* kazoo endpoint */
switch_endpoint_interface_t *kz_endpoint_interface;
static switch_call_cause_t kz_endpoint_outgoing_channel(switch_core_session_t *session,
												 switch_event_t *var_event,
												 switch_caller_profile_t *outbound_profile,
												 switch_core_session_t **new_session, switch_memory_pool_t **pool, switch_originate_flag_t flags,
												 switch_call_cause_t *cancel_cause);
switch_io_routines_t kz_endpoint_io_routines = {
	/*.outgoing_channel */ kz_endpoint_outgoing_channel
};

static switch_call_cause_t kz_endpoint_outgoing_channel(switch_core_session_t *session,
												 switch_event_t *var_event,
												 switch_caller_profile_t *outbound_profile,
												 switch_core_session_t **new_session, switch_memory_pool_t **pool, switch_originate_flag_t flags,
												 switch_call_cause_t *cancel_cause)
{
	switch_xml_t x_user = NULL, x_param, x_params;
	char *user = NULL, *domain = NULL, *dup_domain = NULL, *dialed_user = NULL;
	const char *dest = NULL;
	switch_call_cause_t cause = SWITCH_CAUSE_NONE;
	unsigned int timelimit = SWITCH_DEFAULT_TIMEOUT;
	switch_channel_t *new_channel = NULL;
	switch_event_t *params = NULL, *var_event_orig = var_event;
	char stupid[128] = "";
	const char *skip = NULL, *var = NULL;

	if (zstr(outbound_profile->destination_number)) {
		goto done;
	}

	user = strdup(outbound_profile->destination_number);

	if (!user)
		goto done;

	if ((domain = strchr(user, '@'))) {
		*domain++ = '\0';
	} else {
		domain = switch_core_get_domain(SWITCH_TRUE);
		dup_domain = domain;
	}

	if (!domain) {
		goto done;
	}


	switch_event_create(&params, SWITCH_EVENT_REQUEST_PARAMS);
	switch_assert(params);
	switch_event_add_header_string(params, SWITCH_STACK_BOTTOM, "as_channel", "true");
	switch_event_add_header_string(params, SWITCH_STACK_BOTTOM, "action", "user_call");

	if (var_event) {
		switch_event_merge(params, var_event);
	}

	if (var_event && (skip = switch_event_get_header(var_event, "user_recurse_variables")) && switch_false(skip)) {
		if ((var = switch_event_get_header(var_event, SWITCH_CALL_TIMEOUT_VARIABLE)) || (var = switch_event_get_header(var_event, "leg_timeout"))) {
			timelimit = atoi(var);
		}
		var_event = NULL;
	}

	if (switch_xml_locate_user_merged("id", user, domain, NULL, &x_user, params) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_WARNING, "Can't find user [%s@%s]\n", user, domain);
		cause = SWITCH_CAUSE_SUBSCRIBER_ABSENT;
		goto done;
	}

	if ((x_params = switch_xml_child(x_user, "params"))) {
		for (x_param = switch_xml_child(x_params, "param"); x_param; x_param = x_param->next) {
			const char *pvar = switch_xml_attr_soft(x_param, "name");
			const char *val = switch_xml_attr(x_param, "value");

			if (!strcasecmp(pvar, "dial-string")) {
				dest = val;
			} else if (!strncasecmp(pvar, "dial-var-", 9)) {
				if (!var_event) {
					switch_event_create(&var_event, SWITCH_EVENT_GENERAL);
				} else {
					switch_event_del_header(var_event, pvar + 9);
				}
				switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG1, "adding variable to var_event => %s = %s\n", pvar + 9, val);
				switch_event_add_header_string(var_event, SWITCH_STACK_BOTTOM, pvar + 9, val);
			}
		}
	}

	dialed_user = (char *)switch_xml_attr(x_user, "id");

	if (var_event) {
		switch_event_add_header_string(var_event, SWITCH_STACK_BOTTOM, "dialed_user", dialed_user);
		switch_event_add_header_string(var_event, SWITCH_STACK_BOTTOM, "dialed_domain", domain);
		if (!zstr(dest) && !strstr(dest, "presence_id=")) {
			switch_event_add_header(var_event, SWITCH_STACK_BOTTOM, "presence_id", "%s@%s", dialed_user, domain);
		}
	}

	if (!dest) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "No dial-string available, please check your user directory.\n");
		cause = SWITCH_CAUSE_MANDATORY_IE_MISSING;
	} else {
		const char *varval;
		char *d_dest = NULL;
		switch_channel_t *channel;
		switch_originate_flag_t myflags = SOF_NONE;
		char *cid_name_override = NULL;
		char *cid_num_override = NULL;

		if (var_event) {
			cid_name_override = switch_event_get_header(var_event, "origination_caller_id_name");
			cid_num_override = switch_event_get_header(var_event, "origination_caller_id_number");
		}

		if (session) {
			switch_event_t *event = NULL;
			switch_event_create(&event, SWITCH_EVENT_GENERAL);
			channel = switch_core_session_get_channel(session);
			if ((varval = switch_channel_get_variable(channel, SWITCH_CALL_TIMEOUT_VARIABLE))
				|| (var_event && (varval = switch_event_get_header(var_event, "leg_timeout")))) {
				timelimit = atoi(varval);
			}
			switch_channel_event_set_data(channel, event);
			if(var_event) {
				switch_event_merge(event, var_event);
			}

			switch_channel_set_variable(channel, "dialed_user", dialed_user);
			switch_channel_set_variable(channel, "dialed_domain", domain);

			d_dest = switch_event_expand_headers(event, dest);

			switch_event_destroy(&event);

		} else {
			switch_event_t *event = NULL;

			if (var_event) {
				switch_event_dup(&event, var_event);
				switch_event_del_header(event, "dialed_user");
				switch_event_del_header(event, "dialed_domain");
				if ((varval = switch_event_get_header(var_event, SWITCH_CALL_TIMEOUT_VARIABLE)) ||
					(varval = switch_event_get_header(var_event, "leg_timeout"))) {
					timelimit = atoi(varval);
				}
			} else {
				switch_event_create(&event, SWITCH_EVENT_REQUEST_PARAMS);
				switch_assert(event);
			}

			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "dialed_user", dialed_user);
			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "dialed_domain", domain);
			d_dest = switch_event_expand_headers(event, dest);
			switch_event_destroy(&event);
		}

		if ((flags & SOF_NO_LIMITS)) {
			myflags |= SOF_NO_LIMITS;
		}

		if ((flags & SOF_FORKED_DIAL)) {
			myflags |= SOF_NOBLOCK;
		}

		switch_snprintf(stupid, sizeof(stupid), "kz/%s", dialed_user);
		if (switch_stristr(stupid, d_dest)) {
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_WARNING, "Waddya Daft? You almost called '%s' in an infinate loop!\n",
							  stupid);
			cause = SWITCH_CAUSE_INVALID_IE_CONTENTS;
		} else if (switch_ivr_originate(session, new_session, &cause, d_dest, timelimit, NULL,
										cid_name_override, cid_num_override, outbound_profile, var_event, myflags,
										cancel_cause, NULL) == SWITCH_STATUS_SUCCESS) {
			const char *context;
			switch_caller_profile_t *cp;

			if (var_event) {
				switch_event_del_header(var_event, "origination_uuid");
			}

			new_channel = switch_core_session_get_channel(*new_session);

			if ((context = switch_channel_get_variable(new_channel, "user_context"))) {
				if ((cp = switch_channel_get_caller_profile(new_channel))) {
					cp->context = switch_core_strdup(cp->pool, context);
				}
			}

			if ((x_params = switch_xml_child(x_user, "variables"))) {
				for (x_param = switch_xml_child(x_params, "variable"); x_param; x_param = x_param->next) {
					const char *pvar = switch_xml_attr(x_param, "name");
					const char *val = switch_xml_attr(x_param, "value");
					switch_channel_set_variable(new_channel, pvar, val);
				}
			}

			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(*new_session), SWITCH_LOG_DEBUG1, "CHECKING CALLER-ID\n");
			if ((x_params = switch_xml_child(x_user, "profile-variables"))) {
				switch_caller_profile_t *cp = NULL;
				const char* val = NULL;
				for (x_param = switch_xml_child(x_params, "variable"); x_param; x_param = x_param->next) {
					const char *pvar = switch_xml_attr(x_param, "name");
					const char *val = switch_xml_attr(x_param, "value");
					switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(*new_session), SWITCH_LOG_DEBUG1, "setting profile var %s = %s\n", pvar, val);
					switch_channel_set_profile_var(new_channel, pvar, val);
				}
				cp = switch_channel_get_caller_profile(new_channel);
				if((val=switch_caller_get_field_by_name(cp, "Endpoint-Caller-ID-Name"))) {
						cp->callee_id_name = val;
						cp->orig_caller_id_name = val;
				}
				if((val=switch_caller_get_field_by_name(cp, "Endpoint-Caller-ID-Number"))) {
					cp->callee_id_number = val;
					cp->orig_caller_id_number = val;
				}
			}
			switch_core_session_rwunlock(*new_session);
		}

		if (d_dest != dest) {
			switch_safe_free(d_dest);
		}
	}

  done:

	if (x_user) {
		switch_xml_free(x_user);
	}

	if (params) {
		switch_event_destroy(&params);
	}

	if (var_event && var_event_orig != var_event) {
		switch_event_destroy(&var_event);
	}

	switch_safe_free(user);
	switch_safe_free(dup_domain);

	return cause;
}


void add_kz_endpoints(switch_loadable_module_interface_t **module_interface) {
	kz_endpoint_interface = (switch_endpoint_interface_t *) switch_loadable_module_create_interface(*module_interface, SWITCH_ENDPOINT_INTERFACE);
	kz_endpoint_interface->interface_name = "kz";
	kz_endpoint_interface->io_routines = &kz_endpoint_io_routines;
}
