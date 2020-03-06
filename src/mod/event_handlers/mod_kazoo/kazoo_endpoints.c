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

#define INTERACTION_VARIABLE "Call-Interaction-ID"

static const char *x_bridge_variables[] = {
		"Call-Control-Queue",
		"Call-Control-PID",
		"Call-Control-Node",
		INTERACTION_VARIABLE,
		"ecallmgr_Ecallmgr-Node",
		"sip_h_k-cid",
		"Switch-URI",
		"Switch-URL",
		NULL
};

static void kz_tweaks_variables_to_event(switch_core_session_t *session, switch_event_t *event)
{
	int i;
	switch_channel_t *channel = switch_core_session_get_channel(session);
	for(i = 0; x_bridge_variables[i] != NULL; i++) {
		const char *val = switch_channel_get_variable_dup(channel, x_bridge_variables[i], SWITCH_FALSE, -1);
		switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, x_bridge_variables[i], val);
	}
}

static switch_call_cause_t kz_endpoint_outgoing_channel(switch_core_session_t *session,
							switch_event_t *var_event,
							switch_caller_profile_t *outbound_profile_in,
							switch_core_session_t **new_session, switch_memory_pool_t **pool, switch_originate_flag_t flags,
							switch_call_cause_t *cancel_cause)
{
	switch_xml_t x_user = NULL, x_param, x_params, x_callfwd;
	char *user = NULL, *domain = NULL, *dup_domain = NULL, *dialed_user = NULL;
	char *dest = NULL;
	switch_call_cause_t cause = SWITCH_CAUSE_NONE;
	unsigned int timelimit = SWITCH_DEFAULT_TIMEOUT;
	switch_channel_t *new_channel = NULL;
	switch_event_t *params = NULL, *var_event_orig = var_event;
	char stupid[128] = "";
	const char *skip = NULL, *var = NULL;
	switch_core_session_t *a_session = NULL, *e_session = NULL;
	cJSON * ctx = NULL;
	const char *endpoint_dial = NULL;
	const char *callforward_dial = NULL;
	const char *failover_dial = NULL;
	char *b_failover_dial = NULL;
	const char *endpoint_separator = NULL;
	const char *varval = NULL;
	char *d_dest = NULL;
	switch_channel_t *channel = NULL;
	switch_originate_flag_t myflags = SOF_NONE;
	char *cid_name_override = NULL;
	char *cid_num_override = NULL;
	switch_event_t *event = NULL;
	switch_status_t status = SWITCH_STATUS_SUCCESS;
	switch_caller_profile_t *outbound_profile = NULL;


	if (zstr(outbound_profile_in->destination_number)) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_WARNING, "NO DESTINATION NUMBER\n");
		goto done;
	}

	user = strdup(outbound_profile_in->destination_number);

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
	if (session) {
		switch_event_add_header_string(params, SWITCH_STACK_BOTTOM, "Unique-ID", switch_core_session_get_uuid(session));
	}

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

	if (var_event) {
		const char * str_ctx = switch_event_get_header(var_event, "kz-endpoint-runtime-context");
		if ( str_ctx  ) {
			ctx = cJSON_Parse(str_ctx);
			if (ctx) {
				switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "call context parsed => %s\n", str_ctx);
			}
		}
	}

	if ((x_params = switch_xml_child(x_user, "variables"))) {
		for (x_param = switch_xml_child(x_params, "variable"); x_param; x_param = x_param->next) {
			const char *pvar = switch_xml_attr_soft(x_param, "name");
			const char *val = switch_xml_attr(x_param, "value");
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "adding variable to var_event => %s = %s\n", pvar, val);
			if (!var_event) {
				switch_event_create(&var_event, SWITCH_EVENT_GENERAL);
			} else {
				switch_event_del_header(var_event, pvar);
			}
			switch_event_add_header_string(var_event, SWITCH_STACK_BOTTOM, pvar, val);
		}
	}

	if ((x_params = switch_xml_child(x_user, "params"))) {
		for (x_param = switch_xml_child(x_params, "param"); x_param; x_param = x_param->next) {
			const char *pvar = switch_xml_attr_soft(x_param, "name");
			const char *val = switch_xml_attr(x_param, "value");

			if (!strcasecmp(pvar, "endpoint-dial-string")) {
				endpoint_dial = val;
			} else if (!strcasecmp(pvar, "callforward-dial-string")) {
				callforward_dial = val;
			} else if (!strcasecmp(pvar, "endpoint-separator")) {
				endpoint_separator = val;
			} else if (!strncasecmp(pvar, "dial-var-", 9)) {
				if (!var_event) {
					switch_event_create(&var_event, SWITCH_EVENT_GENERAL);
				} else {
					switch_event_del_header(var_event, pvar + 9);
				}
				switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "adding dialog var to var_event => %s = %s\n", pvar + 9, val);
				switch_event_add_header_string(var_event, SWITCH_STACK_BOTTOM, pvar + 9, val);
			}
		}
	}

	x_callfwd = switch_xml_child(x_user, "call-forward");
	if (x_callfwd) {
		switch_bool_t call_fwd_is_substitute = SWITCH_FALSE,
				call_fwd_is_failover = SWITCH_FALSE,
				call_fwd_direct_calls_only = SWITCH_FALSE,
				call_fwd_is_valid = SWITCH_TRUE;
		for (x_param = switch_xml_child(x_callfwd, "variable"); x_param; x_param = x_param->next) {
			const char *var = switch_xml_attr_soft(x_param, "name");
			const char *val = switch_xml_attr(x_param, "value");
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "cfw %s => %s\n", var, val);
			if (!strcasecmp(var, "Is-Substitute")) {
				call_fwd_is_substitute = switch_true(val);
			} else if (!strcasecmp(var, "Is-Failover")) {
				call_fwd_is_failover = switch_true(val);
			} else if (!strcasecmp(var, "Direct-Calls-Only")) {
				call_fwd_direct_calls_only = switch_true(val);
			}
		}

		if (call_fwd_direct_calls_only) {
			call_fwd_is_valid = SWITCH_FALSE;
			if (ctx ) {
				cJSON *json_flags = cJSON_GetObjectItem(ctx, "Flags");
				if (json_flags && json_flags->type == cJSON_Array) {
					cJSON *item;
					cJSON_ArrayForEach(item, json_flags) {
						if (!strcmp(item->valuestring, "direct_call")) {
							call_fwd_is_valid = SWITCH_TRUE;
							break;
						}
					}
					if (!call_fwd_is_valid) {
						switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "call fwd requires direct_call and it was not found\n");
					}
				}
			}
		}

		if (!call_fwd_is_valid) {
			dest = strdup(endpoint_dial);
		} else if (call_fwd_is_failover) {
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "setting failover => %s\n", callforward_dial);
			dest = strdup(endpoint_dial);
			failover_dial = callforward_dial;
		} else if (call_fwd_is_substitute) {
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "setting call fwd substitute => %s\n", callforward_dial);
			dest = strdup(callforward_dial);
		} else {
			dest = switch_mprintf("%s%s%s", endpoint_dial ? endpoint_dial : "", (endpoint_dial ? endpoint_separator ? endpoint_separator : "," : ""), callforward_dial);
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "setting call fwd append => %s => %s\n", callforward_dial, dest);
		}
	} else {
		dest = strdup(endpoint_dial);
	}

	dialed_user = (char *)switch_xml_attr(x_user, "id");

	if (var_event) {
		switch_event_add_header_string(var_event, SWITCH_STACK_BOTTOM, "dialed_user", dialed_user);
		switch_event_add_header_string(var_event, SWITCH_STACK_BOTTOM, "dialed_domain", domain);
	}

	if (!dest) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "No dial-string available, please check your user directory.\n");
		cause = SWITCH_CAUSE_MANDATORY_IE_MISSING;
		goto done;
	}

	if (var_event) {
		cid_name_override = switch_event_get_header(var_event, "origination_caller_id_name");
		cid_num_override = switch_event_get_header(var_event, "origination_caller_id_number");
	}

	if(session) {
		a_session = session;
	} else if(var_event) {
		const char* uuid_e_session = switch_event_get_header(var_event, "ent_originate_aleg_uuid");
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "CHECKING ORIGINATE-UUID : %s\n", uuid_e_session);
		if (uuid_e_session && (e_session = switch_core_session_locate(uuid_e_session)) != NULL) {
			a_session = e_session;
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "FOUND ORIGINATE-UUID : %s\n", uuid_e_session);
		}
	}

	if (a_session) {
		switch_event_create(&event, SWITCH_EVENT_GENERAL);
		channel = switch_core_session_get_channel(a_session);
		if ((varval = switch_channel_get_variable(channel, SWITCH_CALL_TIMEOUT_VARIABLE))
			|| (var_event && (varval = switch_event_get_header(var_event, "leg_timeout")))) {
			timelimit = atoi(varval);
		}
		switch_channel_event_set_data(channel, event);

		switch_channel_set_variable(channel, "dialed_user", dialed_user);
		switch_channel_set_variable(channel, "dialed_domain", domain);

	} else {
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
	}

	if ((x_params = switch_xml_child(x_user, "profile-variables"))) {
		for (x_param = switch_xml_child(x_params, "variable"); x_param; x_param = x_param->next) {
			const char *pvar = switch_xml_attr_soft(x_param, "name");
			const char *val = switch_xml_attr(x_param, "value");
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "adding profile variable to event => %s = %s\n", pvar, val);
			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, pvar, val);
		}
	}

	if ((x_params = switch_xml_child(x_user, "variables"))) {
		for (x_param = switch_xml_child(x_params, "variable"); x_param; x_param = x_param->next) {
			const char *pvar = switch_xml_attr_soft(x_param, "name");
			const char *val = switch_xml_attr(x_param, "value");
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "adding variable to event => %s = %s\n", pvar, val);
			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, pvar, val);
		}
	}

	if ((x_params = switch_xml_child(x_user, "params"))) {
		for (x_param = switch_xml_child(x_params, "param"); x_param; x_param = x_param->next) {
			const char *pvar = switch_xml_attr_soft(x_param, "name");
			const char *val = switch_xml_attr(x_param, "value");

			if (!strncasecmp(pvar, "dial-var-", 9)) {
				switch_event_del_header(event, pvar + 9);
				switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "adding dialog var to event => %s = %s\n", pvar + 9, val);
				switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, pvar + 9, val);
			}
		}
	}

	// add runtime vars to event for expand
	if (ctx) {
		kz_expand_json_to_event(ctx, event, "kz_ctx");
	}

	d_dest = kz_event_expand_headers(event, dest);
	switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "dialing %s => %s\n", dest, d_dest);

	if(failover_dial) {
		b_failover_dial = kz_event_expand_headers(event, failover_dial);
	}

	if (var_event) {
		kz_expand_headers(event, var_event);
	}

	switch_event_destroy(&event);


	if ((flags & SOF_NO_LIMITS)) {
		myflags |= SOF_NO_LIMITS;
	}

	if ((flags & SOF_FORKED_DIAL)) {
		myflags |= SOF_NOBLOCK;
	}

	if ( a_session ) {
		if(var_event) {
			kz_tweaks_variables_to_event(a_session, var_event);
		}
	}

	if(e_session) {
		switch_core_session_rwunlock(e_session);
	}

	switch_snprintf(stupid, sizeof(stupid), "kz/%s", dialed_user);
	if (switch_stristr(stupid, d_dest)) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_WARNING, "Waddya Daft? You almost called '%s' in an infinate loop!\n", stupid);
		cause = SWITCH_CAUSE_INVALID_IE_CONTENTS;
		goto done;
	}

	//
	outbound_profile = outbound_profile_in;
	/*
	outbound_profile = switch_caller_profile_dup(outbound_profile_in->pool, outbound_profile_in);
	switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(*new_session), SWITCH_LOG_DEBUG1, "CHECKING CALLER-ID\n");
	if ((x_params = switch_xml_child(x_user, "profile-variables"))) {
		const char* val = NULL;
		outbound_profile->soft = NULL;
		for (x_param = switch_xml_child(x_params, "variable"); x_param; x_param = x_param->next) {
			const char *pvar = switch_xml_attr(x_param, "name");
			const char *val = switch_xml_attr(x_param, "value");
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(*new_session), SWITCH_LOG_DEBUG1, "setting profile var %s = %s\n", pvar, val);
			switch_caller_profile_set_var(outbound_profile, pvar, val);
		}
		// * TODO * verify onnet/offnet
		if((val=switch_caller_get_field_by_name(outbound_profile, "Endpoint-Caller-ID-Name"))) {
			outbound_profile->callee_id_name = val;
			outbound_profile->orig_caller_id_name = val;
		}
		if((val=switch_caller_get_field_by_name(outbound_profile, "Endpoint-Caller-ID-Number"))) {
			outbound_profile->callee_id_number = val;
			outbound_profile->orig_caller_id_number = val;
		}
	}
	*/

	status = switch_ivr_originate(session, new_session, &cause, d_dest, timelimit, NULL,
	 	                         cid_name_override, cid_num_override, outbound_profile, var_event, myflags,
	 	                         cancel_cause, NULL);

	if (status != SWITCH_STATUS_SUCCESS && b_failover_dial) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_INFO, "trying failover => %s\n", failover_dial);
		status = switch_ivr_originate(session, new_session, &cause, b_failover_dial, timelimit, NULL,
		 	                         cid_name_override, cid_num_override, outbound_profile, var_event, myflags,
		 	                         cancel_cause, NULL);
	}

	if (status == SWITCH_STATUS_SUCCESS) {
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


		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(*new_session), SWITCH_LOG_DEBUG1, "CHECKING CALLER-ID\n");
		if ((x_params = switch_xml_child(x_user, "profile-variables"))) {
			switch_caller_profile_t *cp = switch_channel_get_caller_profile(new_channel);
			const char* val = NULL;
			cp->soft = NULL;
			for (x_param = switch_xml_child(x_params, "variable"); x_param; x_param = x_param->next) {
				const char *pvar = switch_xml_attr(x_param, "name");
				const char *val = switch_xml_attr(x_param, "value");
				switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(*new_session), SWITCH_LOG_DEBUG1, "setting profile var %s = %s\n", pvar, val);
				switch_channel_set_profile_var(new_channel, pvar, val);
				//switch_caller_profile_set_var(cp, pvar, val);
			}
			// * TODO * verify onnet/offnet
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

  done:

	if (d_dest && d_dest != dest) {
		switch_safe_free(d_dest);
	}

	if(b_failover_dial && b_failover_dial != failover_dial) {
		switch_safe_free(b_failover_dial);
	}

	switch_safe_free(dest);

  	if (ctx) {
  		cJSON_Delete(ctx);
  	}

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


/* kazoo endpoint */


switch_io_routines_t kz_endpoint_io_routines = {
	/*.outgoing_channel */ kz_endpoint_outgoing_channel
};

void add_kz_endpoints(switch_loadable_module_interface_t **module_interface) {
	switch_endpoint_interface_t *kz_endpoint_interface;
	kz_endpoint_interface = (switch_endpoint_interface_t *) switch_loadable_module_create_interface(*module_interface, SWITCH_ENDPOINT_INTERFACE);
	kz_endpoint_interface->interface_name = "kz";
	kz_endpoint_interface->io_routines = &kz_endpoint_io_routines;
}
