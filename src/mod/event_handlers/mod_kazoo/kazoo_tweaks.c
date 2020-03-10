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
 * Luis Azedo <luis@2600hz.com>
 *
 * mod_hacks.c -- hacks with state handlers
 *
 */
#include "mod_kazoo.h"

static char *TWEAK_NAMES[] = {
	"interaction-id",
	"export-vars",
	"switch-uri",
	"replaces-call-id",
	"loopback-vars",
	"caller-id",
	"transfers",
	"bridge",
	"bridge-replaces-aleg",
	"bridge-replaces-call-id",
	"bridge-variables",
	"restore-caller-id-on-blind-xfer"
};

static const char *bridge_variables[] = {
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

static switch_status_t kz_tweaks_signal_bridge_on_hangup(switch_core_session_t *session)
{
	switch_channel_t *channel = switch_core_session_get_channel(session);
	switch_event_t *my_event;

	const char *peer_uuid = switch_channel_get_variable(channel, "Bridge-B-Unique-ID");

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "tweak signal bridge on hangup: %s , %s\n", switch_core_session_get_uuid(session), peer_uuid);

	if (switch_event_create(&my_event, SWITCH_EVENT_CHANNEL_UNBRIDGE) == SWITCH_STATUS_SUCCESS) {
		switch_event_add_header_string(my_event, SWITCH_STACK_BOTTOM, "Bridge-A-Unique-ID", switch_core_session_get_uuid(session));
		switch_event_add_header_string(my_event, SWITCH_STACK_BOTTOM, "Bridge-B-Unique-ID", peer_uuid);
		switch_channel_event_set_data(channel, my_event);
		switch_event_fire(&my_event);
	}

	return SWITCH_STATUS_SUCCESS;
}

static const switch_state_handler_table_t kz_tweaks_signal_bridge_state_handlers = {
	/*.on_init */ NULL,
	/*.on_routing */ NULL,
	/*.on_execute */ NULL,
	/*.on_hangup */ kz_tweaks_signal_bridge_on_hangup,
	/*.on_exchange_media */ NULL,
	/*.on_soft_execute */ NULL,
	/*.on_consume_media */ NULL,
	/*.on_hibernate */ NULL,
	/*.on_reset */ NULL,
	/*.on_park */ NULL,
	/*.on_reporting */ NULL,
	/*.on_destroy */ NULL
};

static void kz_tweaks_handle_bridge_variables(switch_event_t *event)
{
	switch_core_session_t *a_session = NULL, *b_session = NULL;
	const char *a_leg = switch_event_get_header(event, "Bridge-A-Unique-ID");
	const char *b_leg = switch_event_get_header(event, "Bridge-B-Unique-ID");
	int i;

	if (!kz_test_tweak(KZ_TWEAK_BRIDGE_VARIABLES)) return;

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "tweak bridge event handler: variables : %s , %s\n", a_leg, b_leg);

	if (a_leg && (a_session = switch_core_session_locate(a_leg)) != NULL) {
		switch_channel_t *a_channel = switch_core_session_get_channel(a_session);
		if(switch_channel_get_variable_dup(a_channel, bridge_variables[0], SWITCH_FALSE, -1) == NULL) {
			if(b_leg && (b_session = switch_core_session_locate(b_leg)) != NULL) {
				switch_channel_t *b_channel = switch_core_session_get_channel(b_session);
				for(i = 0; bridge_variables[i] != NULL; i++) {
					const char *val = switch_channel_get_variable_dup(b_channel, bridge_variables[i], SWITCH_FALSE, -1);
					switch_channel_set_variable(a_channel, bridge_variables[i], val);
				}
				switch_core_session_rwunlock(b_session);
			}
		} else {
			if(b_leg && (b_session = switch_core_session_locate(b_leg)) != NULL) {
				switch_channel_t *b_channel = switch_core_session_get_channel(b_session);
				if(switch_channel_get_variable_dup(b_channel, bridge_variables[0], SWITCH_FALSE, -1) == NULL) {
					for(i = 0; bridge_variables[i] != NULL; i++) {
						const char *val = switch_channel_get_variable_dup(a_channel, bridge_variables[i], SWITCH_FALSE, -1);
						switch_channel_set_variable(b_channel, bridge_variables[i], val);
					}
				}
				switch_core_session_rwunlock(b_session);
			}
		}
		switch_core_session_rwunlock(a_session);
	} else {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG1, "NOT FOUND : %s\n", a_leg);
	}

}

static void kz_tweaks_handle_bridge_replaces_aleg(switch_event_t *event)
{
	switch_event_t *my_event;

	const char *replaced_call_id =	switch_event_get_header(event, "variable_sip_replaces_call_id");
	const char *a_leg_call_id =	switch_event_get_header(event, "variable_sip_replaces_a-leg");
	const char *peer_uuid = switch_event_get_header(event, "Unique-ID");
	int processed = 0;

	if (!kz_test_tweak(KZ_TWEAK_BRIDGE_REPLACES_ALEG)) return;


	if(a_leg_call_id && replaced_call_id) {
		const char *call_id = switch_event_get_header(event, "Bridge-B-Unique-ID");
		switch_core_session_t *session = NULL;
		if ((session = switch_core_session_locate(peer_uuid)) != NULL) {
			switch_channel_t *channel = switch_core_session_get_channel(session);
			processed = switch_true(switch_channel_get_variable_dup(channel, "Bridge-Event-Processed", SWITCH_FALSE, -1));
			switch_channel_set_variable(channel, "Bridge-Event-Processed", "true");
			switch_core_session_rwunlock(session);
		}

		if(processed) {
			if(call_id) {
				if((session = switch_core_session_locate(call_id)) != NULL) {
					switch_channel_t *channel = switch_core_session_get_channel(session);
					switch_channel_set_variable(channel, "Bridge-Event-Processed", "true");
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "creating channel_bridge event A - %s , B - %s\n", switch_core_session_get_uuid(session), peer_uuid);
					if (switch_event_create(&my_event, SWITCH_EVENT_CHANNEL_BRIDGE) == SWITCH_STATUS_SUCCESS) {
						switch_event_add_header_string(my_event, SWITCH_STACK_BOTTOM, "Bridge-A-Unique-ID", switch_core_session_get_uuid(session));
						switch_event_add_header_string(my_event, SWITCH_STACK_BOTTOM, "Bridge-B-Unique-ID", peer_uuid);
						switch_channel_event_set_data(channel, my_event);
						switch_event_fire(&my_event);
					}
					switch_channel_set_variable(channel, "Bridge-B-Unique-ID", peer_uuid);
					switch_channel_add_state_handler(channel, &kz_tweaks_signal_bridge_state_handlers);
					switch_core_session_rwunlock(session);
				} else {
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "invalid session : %s\n", call_id);
					DUMP_EVENT(event);
				}
			}
		}

	}

}

static void kz_tweaks_handle_bridge_replaces_call_id(switch_event_t *event)
{
	switch_event_t *my_event;

	const char *replaced_call_id =	switch_event_get_header(event, "variable_sip_replaces_call_id");
	const char *a_leg_call_id =	switch_event_get_header(event, "variable_sip_replaces_a-leg");
	const char *peer_uuid = switch_event_get_header(event, "Unique-ID");

	if (!kz_test_tweak(KZ_TWEAK_BRIDGE_REPLACES_CALL_ID)) return;

	if(a_leg_call_id && replaced_call_id) {
		switch_core_session_t *call_session = NULL;
		const char *call_id = switch_event_get_header(event, "Bridge-B-Unique-ID");
		if (call_id && (call_session = switch_core_session_locate(call_id)) != NULL) {
			switch_channel_t *call_channel = switch_core_session_get_channel(call_session);
			if (switch_event_create(&my_event, SWITCH_EVENT_CHANNEL_BRIDGE) == SWITCH_STATUS_SUCCESS) {
				switch_event_add_header_string(my_event, SWITCH_STACK_BOTTOM, "Bridge-A-Unique-ID", switch_core_session_get_uuid(call_session));
				switch_event_add_header_string(my_event, SWITCH_STACK_BOTTOM, "Bridge-B-Unique-ID", peer_uuid);
				switch_channel_event_set_data(call_channel, my_event);
				switch_event_fire(&my_event);
			}
			switch_channel_set_variable(call_channel, "Bridge-B-Unique-ID", peer_uuid);
			switch_channel_add_state_handler(call_channel, &kz_tweaks_signal_bridge_state_handlers);
			switch_core_session_rwunlock(call_session);
		} else {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG1, "NOT FOUND : %s\n", call_id);
		}
	}

}

static void kz_tweaks_channel_bridge_event_handler(switch_event_t *event)
{
	if (!kz_test_tweak(KZ_TWEAK_BRIDGE)) return;

	kz_tweaks_handle_bridge_replaces_call_id(event);
	kz_tweaks_handle_bridge_replaces_aleg(event);
	kz_tweaks_handle_bridge_variables(event);
}

// TRANSFERS

static void kz_tweaks_channel_replaced_event_handler(switch_event_t *event)
{
	const char *uuid = switch_event_get_header(event, "Unique-ID");
	const char *replaced_by = switch_event_get_header(event, "att_xfer_replaced_by");

	if (!kz_test_tweak(KZ_TWEAK_TRANSFERS)) return;

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "REPLACED : %s , %s\n", uuid, replaced_by);
}

static void kz_tweaks_channel_intercepted_event_handler(switch_event_t *event)
{
	const char *uuid = switch_event_get_header(event, "Unique-ID");
	const char *peer_uuid = switch_event_get_header(event, "intercepted_by");

	if (!kz_test_tweak(KZ_TWEAK_TRANSFERS)) return;

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "INTERCEPTED : %s => %s\n", uuid, peer_uuid);
}

static void kz_tweaks_channel_transferor_event_handler(switch_event_t *event)
{
	switch_core_session_t *uuid_session = NULL;
	switch_event_t *evt = NULL;
	const char *uuid = switch_event_get_header(event, "Unique-ID");

	const char *orig_call_id = switch_event_get_header(event, "att_xfer_original_call_id");
	const char *dest_peer_uuid = switch_event_get_header(event, "att_xfer_destination_peer_uuid");
	const char *dest_call_id = switch_event_get_header(event, "att_xfer_destination_call_id");

	const char *file = switch_event_get_header(event, "Event-Calling-File");
	const char *func = switch_event_get_header(event, "Event-Calling-Function");
	const char *line = switch_event_get_header(event, "Event-Calling-Line-Number");

	if (!kz_test_tweak(KZ_TWEAK_TRANSFERS)) return;

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "TRANSFEROR : %s , %s , %s, %s, %s , %s , %s \n", uuid, orig_call_id, dest_peer_uuid, dest_call_id, file, func, line);
	if ((uuid_session = switch_core_session_locate(uuid)) != NULL) {
		switch_channel_t *uuid_channel = switch_core_session_get_channel(uuid_session);
		const char* interaction_id = switch_channel_get_variable_dup(uuid_channel, INTERACTION_VARIABLE, SWITCH_TRUE, -1);
		// set to uuid & peer_uuid
		if(interaction_id != NULL) {
			switch_core_session_t *session = NULL;
			if(dest_call_id && (session = switch_core_session_locate(dest_call_id)) != NULL) {
				switch_channel_t *channel = switch_core_session_get_channel(session);
				const char* prv_interaction_id = switch_channel_get_variable_dup(channel, INTERACTION_VARIABLE, SWITCH_TRUE, -1);
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "LOCATING UUID PRV : %s : %s\n", prv_interaction_id, interaction_id);
				switch_channel_set_variable(channel, INTERACTION_VARIABLE, interaction_id);
				if (switch_event_create(&evt, SWITCH_EVENT_CHANNEL_DATA) == SWITCH_STATUS_SUCCESS) {
					switch_channel_event_set_data(channel, evt);
					switch_event_fire(&evt);
				}
				switch_core_session_rwunlock(session);
				switch_safe_strdup(prv_interaction_id);
			} else {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "TRANSFEROR NO UUID SESSION: %s , %s , %s \n", uuid, dest_call_id, dest_peer_uuid);
			}
			if(dest_peer_uuid && (session = switch_core_session_locate(dest_peer_uuid)) != NULL) {
				switch_channel_t *channel = switch_core_session_get_channel(session);
				const char* prv_interaction_id = switch_channel_get_variable_dup(channel, INTERACTION_VARIABLE, SWITCH_TRUE, -1);
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "LOCATING PEER UUID PRV : %s : %s\n", prv_interaction_id, interaction_id);
				switch_channel_set_variable(channel, INTERACTION_VARIABLE, interaction_id);
				if (switch_event_create(&evt, SWITCH_EVENT_CHANNEL_DATA) == SWITCH_STATUS_SUCCESS) {
					switch_channel_event_set_data(channel, evt);
					switch_event_fire(&evt);
				}
				switch_core_session_rwunlock(session);
				switch_safe_strdup(prv_interaction_id);
			} else {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "TRANSFEROR NO PEER SESSION: %s , %s , %s \n", uuid, dest_call_id, dest_peer_uuid);
			}
			if(orig_call_id && (session = switch_core_session_locate(orig_call_id)) != NULL) {
				switch_channel_t *channel = switch_core_session_get_channel(session);
				const char* prv_interaction_id = switch_channel_get_variable_dup(channel, INTERACTION_VARIABLE, SWITCH_TRUE, -1);
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "LOCATING PEER UUID PRV : %s : %s\n", prv_interaction_id, interaction_id);
				switch_channel_set_variable(channel, INTERACTION_VARIABLE, interaction_id);
				if (switch_event_create(&evt, SWITCH_EVENT_CHANNEL_DATA) == SWITCH_STATUS_SUCCESS) {
					switch_channel_event_set_data(channel, evt);
					switch_event_fire(&evt);
				}
				switch_core_session_rwunlock(session);
				switch_safe_strdup(prv_interaction_id);
			} else {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "TRANSFEROR NO PEER SESSION: %s , %s , %s \n", uuid, dest_call_id, dest_peer_uuid);
			}
			switch_safe_strdup(interaction_id);
		} else {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "TRANSFEROR ID = NULL : %s , %s , %s \n", uuid, dest_call_id, dest_peer_uuid);
		}
		switch_core_session_rwunlock(uuid_session);
	} else {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "SESSION NOT FOUND : %s\n", uuid);
	}
}

static void kz_tweaks_channel_transferee_event_handler(switch_event_t *event)
{
	const char *uuid = switch_event_get_header(event, "Unique-ID");
	const char *replaced_by_uuid = switch_event_get_header(event, "att_xfer_replaced_call_id");

	if (!kz_test_tweak(KZ_TWEAK_TRANSFERS)) return;

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "TRANSFEREE : %s replaced by %s\n", uuid, replaced_by_uuid);
}

// END TRANSFERS


static switch_status_t kz_tweaks_handle_loopback(switch_core_session_t *session)
{
	switch_channel_t *channel = switch_core_session_get_channel(session);
	switch_channel_t *a_channel = NULL;
	const char * loopback_leg = NULL;
	const char * loopback_aleg = NULL;
	switch_core_session_t *a_session = NULL;
	switch_event_t *event = NULL;
	switch_event_header_t *header = NULL;
	switch_event_t *to_add = NULL;
	switch_event_t *to_remove = NULL;
	switch_caller_profile_t *caller;
	int n = 0;

	if (!kz_test_tweak(KZ_TWEAK_LOOPBACK_VARS)) {
		return SWITCH_STATUS_SUCCESS;
	}

	caller = switch_channel_get_caller_profile(channel);
	if(strncmp(caller->source, "mod_loopback", 12))
		return SWITCH_STATUS_SUCCESS;

	if((loopback_leg = switch_channel_get_variable(channel, "loopback_leg")) == NULL)
		return SWITCH_STATUS_SUCCESS;

	if(strncmp(loopback_leg, "B", 1))
		return SWITCH_STATUS_SUCCESS;

	switch_channel_get_variables(channel, &event);
	switch_event_create_plain(&to_add, SWITCH_EVENT_CHANNEL_DATA);
	switch_event_create_plain(&to_remove, SWITCH_EVENT_CHANNEL_DATA);

	for(header = event->headers; header; header = header->next) {
		if(!strncmp(header->name, "Export-Loopback-", 16)) {
			kz_switch_event_add_variable_name_printf(to_add, SWITCH_STACK_BOTTOM, header->value, "%s", header->name+16);
			switch_channel_set_variable(channel, header->name, NULL);
			n++;
		} else if(!strncmp(header->name, "sip_loopback_", 13)) {
			kz_switch_event_add_variable_name_printf(to_add, SWITCH_STACK_BOTTOM, header->value, "sip_%s", header->name+13);
		} else if(!strncmp(header->name, "ecallmgr_", 9)) {
			switch_event_add_header_string(to_remove, SWITCH_STACK_BOTTOM, header->name, header->value);
		}
	}
	if(n) {
		for(header = to_remove->headers; header; header = header->next) {
			switch_channel_set_variable(channel, header->name, NULL);
		}
	}

	for(header = to_add->headers; header; header = header->next) {
		switch_channel_set_variable(channel, header->name, header->value);
	}

	// cleanup leg A
	loopback_aleg = switch_channel_get_variable(channel, "other_loopback_leg_uuid");
	if(loopback_aleg != NULL) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "found loopback a-leg uuid - %s\n", loopback_aleg);
		if ((a_session = switch_core_session_locate(loopback_aleg))) {
			a_channel = switch_core_session_get_channel(a_session);
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "found loopback session a - %s\n", loopback_aleg);
			switch_channel_del_variable_prefix(a_channel, "Export-Loopback-");
		} else {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Couldn't locate loopback session a - %s\n", loopback_aleg);
		}
	} else {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Couldn't find loopback a-leg uuid!\n");
	}

	/*
	 *  set Interaction-ID
	 *  if we're not crossing account boundaries
	 */
	if (kz_test_tweak(KZ_TWEAK_INTERACTION_ID)) {
		if (a_channel) {
			const char *interaction_id = switch_channel_get_variable_dup(a_channel, INTERACTION_VARIABLE, SWITCH_FALSE, -1);
			const char *a_account_id  = switch_channel_get_variable_dup(a_channel, "ecallmgr_Account-ID", SWITCH_FALSE, -1);
			const char *b_account_id  = switch_channel_get_variable_dup(channel, "ecallmgr_Account-ID", SWITCH_FALSE, -1);
			if ((!a_account_id) || (!b_account_id) || (!strcmp(a_account_id, b_account_id))) {
				switch_channel_set_variable(channel, INTERACTION_VARIABLE, interaction_id);
			}
		}
	}

	if (a_session){
		switch_core_session_rwunlock(a_session);
	}
	switch_event_destroy(&event);
	switch_event_destroy(&to_add);
	switch_event_destroy(&to_remove);

	return SWITCH_STATUS_SUCCESS;

}

static void kz_tweaks_handle_caller_id(switch_core_session_t *session)
{
	if (kz_test_tweak(KZ_TWEAK_CALLER_ID)) {
		switch_channel_t *channel = switch_core_session_get_channel(session);
		switch_caller_profile_t* caller = switch_channel_get_caller_profile(channel);
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG1, "CHECKING CALLER-ID\n");
		if (caller && switch_channel_direction(channel) == SWITCH_CALL_DIRECTION_INBOUND) {
			const char* val = NULL;
			if((val=switch_caller_get_field_by_name(caller, "Endpoint-Caller-ID-Name"))) {
				caller->caller_id_name = val;
				caller->orig_caller_id_name = val;
			}
			if((val=switch_caller_get_field_by_name(caller, "Endpoint-Caller-ID-Number"))) {
				caller->caller_id_number = val;
				caller->orig_caller_id_number = val;
			}
		}
	}
}

static switch_status_t kz_tweaks_handle_nightmare_xfer_interaction_id(switch_core_session_t *session)
{
	if (kz_test_tweak(KZ_TWEAK_INTERACTION_ID)) {
		switch_core_session_t *replace_session = NULL;
		switch_channel_t *channel = switch_core_session_get_channel(session);
		const char *replaced_call_id = switch_channel_get_variable(channel, "sip_replaces_call_id");
		const char *core_uuid = switch_channel_get_variable(channel, "sip_h_X-FS-From-Core-UUID");
		const char *partner_uuid = switch_channel_get_variable(channel, "sip_h_X-FS-Refer-Partner-UUID");
		const char *interaction_id = switch_channel_get_variable(channel, "sip_h_X-FS-Call-Interaction-ID");
		if(core_uuid && partner_uuid && replaced_call_id && interaction_id) {
			switch_channel_set_variable(channel, INTERACTION_VARIABLE, interaction_id);
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "checking nightmare xfer tweak for %s\n", switch_channel_get_uuid(channel));
			if ((replace_session = switch_core_session_locate(replaced_call_id))) {
				switch_channel_t *replaced_call_channel = switch_core_session_get_channel(replace_session);
				switch_channel_set_variable(replaced_call_channel, INTERACTION_VARIABLE, interaction_id);
				switch_core_session_rwunlock(replace_session);
			}
			if ((replace_session = switch_core_session_locate(partner_uuid))) {
				switch_channel_t *replaced_call_channel = switch_core_session_get_channel(replace_session);
				switch_channel_set_variable(replaced_call_channel, INTERACTION_VARIABLE, interaction_id);
				switch_core_session_rwunlock(replace_session);
			}
		}
	}

	return SWITCH_STATUS_SUCCESS;

}

static switch_status_t kz_tweaks_handle_replaces_call_id(switch_core_session_t *session)
{
	if (kz_test_tweak(KZ_TWEAK_REPLACES_CALL_ID)) {
		switch_core_session_t *replace_call_session = NULL;
		switch_event_t *event;
		switch_channel_t *channel = switch_core_session_get_channel(session);
		const char *replaced_call_id = switch_channel_get_variable(channel, "sip_replaces_call_id");
		const char *core_uuid = switch_channel_get_variable(channel, "sip_h_X-FS-From-Core-UUID");
		if((!core_uuid) && replaced_call_id) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "checking replaces header tweak for %s\n", replaced_call_id);
			if ((replace_call_session = switch_core_session_locate(replaced_call_id))) {
				switch_channel_t *replaced_call_channel = switch_core_session_get_channel(replace_call_session);
				int i;
				switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "setting bridge variables from %s to %s\n", replaced_call_id, switch_channel_get_uuid(channel));
				for(i = 0; bridge_variables[i] != NULL; i++) {
					const char *val = switch_channel_get_variable_dup(replaced_call_channel, bridge_variables[i], SWITCH_TRUE, -1);
					switch_channel_set_variable(channel, bridge_variables[i], val);
					switch_safe_strdup(val);
				}
				if (switch_event_create(&event, SWITCH_EVENT_CHANNEL_DATA) == SWITCH_STATUS_SUCCESS) {
					switch_channel_event_set_data(channel, event);
					switch_event_fire(&event);
				}
				switch_core_session_rwunlock(replace_call_session);
			}
		}
	}

	return SWITCH_STATUS_SUCCESS;

}


static switch_status_t kz_tweaks_handle_switch_uri(switch_core_session_t *session)
{
	switch_channel_t *channel = switch_core_session_get_channel(session);
	if (kz_test_tweak(KZ_TWEAK_SWITCH_URI)) {
		const char *profile_url = switch_channel_get_variable(channel, "sofia_profile_url");
		if(profile_url) {
			int n = strcspn(profile_url, "@");
			switch_channel_set_variable(channel, "Switch-URL", profile_url);
			switch_channel_set_variable_printf(channel, "Switch-URI", "sip:%s", n > 0 ? profile_url + n + 1 : profile_url);
		}
	}

	return SWITCH_STATUS_SUCCESS;

}

static char * kz_tweaks_new_interaction_id()
{
	long int first = (switch_micro_time_now() / 1000000) + UNIX_EPOCH_IN_GREGORIAN;
	char uuid_str[SWITCH_UUID_FORMATTED_LENGTH + 1];
	switch_uuid_t uuid;
	switch_uuid_get(&uuid);
	switch_uuid_format(uuid_str, &uuid);
	uuid_str[8] = '\0';
	return switch_mprintf("%ld-%s", first, uuid_str);
}

static void kz_tweaks_handle_interaction_id(switch_core_session_t *session)
{
	switch_channel_t *channel = switch_core_session_get_channel(session);
	char * val = NULL;
	switch_core_session_t *peer_session = NULL;
	const char* peer_interaction_id = NULL;

	if (kz_test_tweak(KZ_TWEAK_INTERACTION_ID)) {
		val = kz_tweaks_new_interaction_id();
		if (val) {
			switch_channel_set_variable(channel, "Original-"INTERACTION_VARIABLE, val);
			if(switch_core_session_get_partner(session, &peer_session) == SWITCH_STATUS_SUCCESS) {
				switch_channel_t *peer_channel = switch_core_session_get_channel(peer_session);
				peer_interaction_id = switch_channel_get_variable_dup(peer_channel, INTERACTION_VARIABLE, SWITCH_FALSE, -1);
				switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "PEER_SESSION => %s\n", peer_interaction_id);
				if(peer_interaction_id) {
					switch_channel_set_variable(channel, INTERACTION_VARIABLE, peer_interaction_id);
				}
				switch_core_session_rwunlock(peer_session);
			} else if (!switch_channel_get_variable_dup(channel, INTERACTION_VARIABLE, SWITCH_FALSE, -1)) {
				switch_channel_set_variable(channel, INTERACTION_VARIABLE, val);
			}
		}

		switch_safe_free(val);
	}

}

static switch_status_t kz_outgoing_channel(switch_core_session_t * session, switch_event_t * event, switch_caller_profile_t * cp, switch_core_session_t * peer_session, switch_originate_flag_t flag)
{
	if (kz_test_tweak(KZ_TWEAK_INTERACTION_ID)) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "GOT OUTGOING\n");
		if (peer_session) {
			switch_channel_t *channel = switch_core_session_get_channel(session);
			const char* interaction_id = switch_channel_get_variable_dup(channel, INTERACTION_VARIABLE, SWITCH_FALSE, -1);
			switch_channel_t *peer_channel = switch_core_session_get_channel(peer_session);
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "SESSION && PEER_SESSION => %s\n", interaction_id );
			if (interaction_id) {
				switch_channel_set_variable(peer_channel, INTERACTION_VARIABLE, interaction_id);
			}
		} else {
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "NO SESSION && PEER_SESSION\n");
		}
	}
	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t kz_tweaks_register_handle_blind_xfer(switch_core_session_t *session)
{
	if (kz_test_tweak(KZ_TWEAK_RESTORE_CALLER_ID_ON_BLIND_XFER)) {
		switch_channel_t *channel = switch_core_session_get_channel(session);
		switch_channel_set_variable(channel, "execute_on_blind_transfer", "kz_restore_caller_id");
	}
	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t kz_tweaks_on_init(switch_core_session_t *session)
{
	switch_channel_t *channel = switch_core_session_get_channel(session);
	switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG1, "checking tweaks for %s\n", switch_channel_get_uuid(channel));
	switch_channel_set_flag(channel, CF_VERBOSE_EVENTS);
	switch_core_event_hook_add_outgoing_channel(session, kz_outgoing_channel);
	kz_tweaks_handle_interaction_id(session);
	kz_tweaks_handle_switch_uri(session);
	kz_tweaks_handle_caller_id(session);
	kz_tweaks_handle_nightmare_xfer_interaction_id(session);
	kz_tweaks_handle_replaces_call_id(session);
	kz_tweaks_handle_loopback(session);
	kz_tweaks_register_handle_blind_xfer(session);

	return SWITCH_STATUS_SUCCESS;
}

static switch_state_handler_table_t kz_tweaks_state_handlers = {
	/*.on_init */ kz_tweaks_on_init,
	/*.on_routing */ NULL,
	/*.on_execute */ NULL,
	/*.on_hangup */ NULL,
	/*.on_exchange_media */ NULL,
	/*.on_soft_execute */ NULL,
	/*.on_consume_media */ NULL,
	/*.on_hibernate */ NULL,
	/*.on_reset */ NULL,
	/*.on_park */ NULL,
	/*.on_reporting */ NULL,
	/*.on_destroy */ NULL
};


static void kz_tweaks_register_state_handlers()
{
/*
 * we may need two handlers
 * one with SSH_FLAG_PRE_EXEC
 * and another without it
 * mod_loopback tweaks needs post-init (SSH_FLAG_PRE_EXEC off)
 *
 * kz_tweaks_state_handlers.flags = SSH_FLAG_PRE_EXEC;
 *
 */
	switch_core_add_state_handler(&kz_tweaks_state_handlers);
}

static void kz_tweaks_unregister_state_handlers()
{
	switch_core_remove_state_handler(&kz_tweaks_state_handlers);
}

static void kz_tweaks_bind_events()
{
	if (switch_event_bind("kz_tweaks", SWITCH_EVENT_CHANNEL_BRIDGE, SWITCH_EVENT_SUBCLASS_ANY, kz_tweaks_channel_bridge_event_handler, NULL) != SWITCH_STATUS_SUCCESS) {
	        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Couldn't bind to channel_bridge event!\n");
	}
	if (switch_event_bind("kz_tweaks", SWITCH_EVENT_CUSTOM, "sofia::replaced", kz_tweaks_channel_replaced_event_handler, NULL) != SWITCH_STATUS_SUCCESS) {
	        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Couldn't bind to channel_bridge event!\n");
	}
	if (switch_event_bind("kz_tweaks", SWITCH_EVENT_CUSTOM, "sofia::intercepted", kz_tweaks_channel_intercepted_event_handler, NULL) != SWITCH_STATUS_SUCCESS) {
	        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Couldn't bind to channel_bridge event!\n");
	}
	if (switch_event_bind("kz_tweaks", SWITCH_EVENT_CUSTOM, "sofia::transferor", kz_tweaks_channel_transferor_event_handler, NULL) != SWITCH_STATUS_SUCCESS) {
	        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Couldn't bind to channel_bridge event!\n");
	}
	if (switch_event_bind("kz_tweaks", SWITCH_EVENT_CUSTOM, "sofia::transferee", kz_tweaks_channel_transferee_event_handler, NULL) != SWITCH_STATUS_SUCCESS) {
	        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Couldn't bind to channel_bridge event!\n");
	}
}

static void kz_tweaks_unbind_events()
{
	switch_event_unbind_callback(kz_tweaks_channel_bridge_event_handler);
	switch_event_unbind_callback(kz_tweaks_channel_replaced_event_handler);
	switch_event_unbind_callback(kz_tweaks_channel_intercepted_event_handler);
	switch_event_unbind_callback(kz_tweaks_channel_transferor_event_handler);
	switch_event_unbind_callback(kz_tweaks_channel_transferee_event_handler);
}

void kz_tweaks_add_core_variables()
{
	switch_core_set_variable("UNIX_EPOCH_IN_GREGORIAN", UNIX_EPOCH_IN_GREGORIAN_STR);
}

void kz_tweaks_start()
{
	kz_tweaks_add_core_variables();
	kz_tweaks_register_state_handlers();
	kz_tweaks_bind_events();
}

void kz_tweaks_stop()
{
	kz_tweaks_unbind_events();
	kz_tweaks_unregister_state_handlers();
}

SWITCH_DECLARE(const char *) kz_tweak_name(kz_tweak_t tweak)
{
	return TWEAK_NAMES[tweak];
}

SWITCH_DECLARE(switch_status_t) kz_name_tweak(const char *name, kz_tweak_t *type)
{
	kz_tweak_t x;
	for (x = 0; x < KZ_TWEAK_MAX; x++) {
		if (!strcasecmp(name, TWEAK_NAMES[x])) {
			*type = x;
			return SWITCH_STATUS_SUCCESS;
		}
	}

	return SWITCH_STATUS_FALSE;
}

/* For Emacs:
 * Local Variables:
 * mode:c
 * indent-tabs-mode:t
 * tab-width:4
 * c-basic-offset:4
 * End:
 * For VIM:
 * vim:set softtabstop=4 shiftwidth=4 tabstop=4:
 */


