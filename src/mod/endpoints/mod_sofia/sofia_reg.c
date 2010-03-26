/* 
 * FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
 * Copyright (C) 2005-2010, Anthony Minessale II <anthm@freeswitch.org>
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
 * Anthony Minessale II <anthm@freeswitch.org>
 * Ken Rice, <krice at cometsig.com>  (work sponsored by Comet Signaling LLC, CopperCom, Inc and Asteria Solutions Group, Inc)
 * Paul D. Tinsley <pdt at jackhammer.org>
 * Bret McDanel <trixter AT 0xdecafbad.com>
 * Marcel Barbulescu <marcelbarbulescu@gmail.com>
 * David Knell <>
 * Eliot Gable <egable AT.AT broadvox.com>
 *
 *
 * sofia_ref.c -- SOFIA SIP Endpoint (registration code)
 *
 */
#include "mod_sofia.h"

static void sofia_reg_new_handle(sofia_gateway_t *gateway_ptr, int attach)
{
	int ss_state = nua_callstate_authenticating;

	if (gateway_ptr->nh) {
		nua_handle_bind(gateway_ptr->nh, NULL);
		nua_handle_destroy(gateway_ptr->nh);
		gateway_ptr->nh = NULL;
		sofia_private_free(gateway_ptr->sofia_private);
	}

	gateway_ptr->nh = nua_handle(gateway_ptr->profile->nua, NULL,
								 SIPTAG_CALL_ID_STR(gateway_ptr->uuid_str),
								 SIPTAG_TO_STR(gateway_ptr->register_to),
								 NUTAG_CALLSTATE_REF(ss_state), SIPTAG_FROM_STR(gateway_ptr->register_from), TAG_END());
	if (attach) {
		if (!gateway_ptr->sofia_private) {
			gateway_ptr->sofia_private = malloc(sizeof(*gateway_ptr->sofia_private));
			switch_assert(gateway_ptr->sofia_private);
		}
		memset(gateway_ptr->sofia_private, 0, sizeof(*gateway_ptr->sofia_private));

		gateway_ptr->sofia_private->gateway = gateway_ptr;
		nua_handle_bind(gateway_ptr->nh, gateway_ptr->sofia_private);
	}
}

static void sofia_reg_kill_reg(sofia_gateway_t *gateway_ptr)
{

	if (gateway_ptr->state != REG_STATE_REGED) {
		if (gateway_ptr->nh) {
			nua_handle_destroy(gateway_ptr->nh);
			gateway_ptr->nh = NULL;
		}
		return;
	}

	/*
	   if (!gateway_ptr->nh) {
	   sofia_reg_new_handle(gateway_ptr, SWITCH_FALSE);
	   }
	 */

	if (gateway_ptr->nh) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "UN-Registering %s\n", gateway_ptr->name);
		nua_unregister(gateway_ptr->nh, NUTAG_URL(gateway_ptr->register_url), NUTAG_REGISTRAR(gateway_ptr->register_proxy), TAG_END());
	}



}

static void sofia_reg_fire_custom_gateway_state_event(sofia_gateway_t *gateway, int status, const char *phrase)
{
	switch_event_t *s_event;
	if (switch_event_create_subclass(&s_event, SWITCH_EVENT_CUSTOM, MY_EVENT_GATEWAY_STATE) == SWITCH_STATUS_SUCCESS) {
		switch_event_add_header_string(s_event, SWITCH_STACK_BOTTOM, "Gateway", gateway->name);
		switch_event_add_header_string(s_event, SWITCH_STACK_BOTTOM, "State", sofia_state_string(gateway->state));
		if (!zstr(phrase)) {
			switch_event_add_header_string(s_event, SWITCH_STACK_BOTTOM, "Phrase", phrase);
		}
		if (status) {
			switch_event_add_header(s_event, SWITCH_STACK_BOTTOM, "Status", "%d", status);
		}
		switch_event_fire(&s_event);
	}
}

void sofia_reg_unregister(sofia_profile_t *profile)
{
	sofia_gateway_t *gateway_ptr;
	switch_mutex_lock(mod_sofia_globals.hash_mutex);
	for (gateway_ptr = profile->gateways; gateway_ptr; gateway_ptr = gateway_ptr->next) {

		if (gateway_ptr->sofia_private) {
			sofia_private_free(gateway_ptr->sofia_private);
		}

		if (gateway_ptr->state == REG_STATE_REGED) {
			sofia_reg_kill_reg(gateway_ptr);
		}

	}
	switch_mutex_unlock(mod_sofia_globals.hash_mutex);
}

void sofia_sub_check_gateway(sofia_profile_t *profile, time_t now)
{
	/* NOTE: A lot of the mechanism in place here for refreshing subscriptions is
	 * pretty much redundant, as the sofia stack takes it upon itself to
	 * refresh subscriptions on its own, based on the value of the Expires
	 * header (which we control in the outgoing subscription request)
	 */
	sofia_gateway_t *gateway_ptr;

	switch_mutex_lock(mod_sofia_globals.hash_mutex);
	for (gateway_ptr = profile->gateways; gateway_ptr; gateway_ptr = gateway_ptr->next) {
		sofia_gateway_subscription_t *gw_sub_ptr;

		for (gw_sub_ptr = gateway_ptr->subscriptions; gw_sub_ptr; gw_sub_ptr = gw_sub_ptr->next) {
			int ss_state = nua_callstate_authenticating;
			sub_state_t ostate = gw_sub_ptr->state;
			char *user_via = NULL;

			if (!now) {
				gw_sub_ptr->state = ostate = SUB_STATE_UNSUBED;
				gw_sub_ptr->expires_str = "0";
			}

			if (sofia_glue_check_nat(gateway_ptr->profile, gateway_ptr->register_proxy)) {
				user_via = sofia_glue_create_external_via(NULL, gateway_ptr->profile, gateway_ptr->register_transport);
			}

			switch (ostate) {
			case SUB_STATE_NOSUB:
				break;
			case SUB_STATE_SUBSCRIBE:
				gw_sub_ptr->expires = now + gw_sub_ptr->freq;
				gw_sub_ptr->state = SUB_STATE_SUBED;
				break;
			case SUB_STATE_UNSUBSCRIBE:
				gw_sub_ptr->state = SUB_STATE_NOSUB;

				/* not tested .. */
				nua_unsubscribe(gateway_ptr->nh,
								NUTAG_URL(gateway_ptr->register_url),
								TAG_IF(user_via, SIPTAG_VIA_STR(user_via)),
								SIPTAG_EVENT_STR(gw_sub_ptr->event),
								SIPTAG_ACCEPT_STR(gw_sub_ptr->content_type),
								SIPTAG_TO_STR(gateway_ptr->register_from),
								SIPTAG_FROM_STR(gateway_ptr->register_from), SIPTAG_CONTACT_STR(gateway_ptr->register_contact), TAG_NULL());

				break;
			case SUB_STATE_UNSUBED:
				gateway_ptr->sub_nh = nua_handle(gateway_ptr->profile->nua, NULL,
												 NUTAG_URL(gateway_ptr->register_proxy),
												 TAG_IF(user_via, SIPTAG_VIA_STR(user_via)),
												 SIPTAG_TO_STR(gateway_ptr->register_to),
												 NUTAG_CALLSTATE_REF(ss_state), SIPTAG_FROM_STR(gateway_ptr->register_from), TAG_END());
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "subscribing to [%s] on gateway [%s]\n", gw_sub_ptr->event, gateway_ptr->name);

				gateway_ptr->sofia_private = malloc(sizeof(*gateway_ptr->sofia_private));
				switch_assert(gateway_ptr->sofia_private);

				memset(gateway_ptr->sofia_private, 0, sizeof(*gateway_ptr->sofia_private));

				gateway_ptr->sofia_private->gateway = gateway_ptr;
				nua_handle_bind(gateway_ptr->nh, gateway_ptr->sofia_private);

				if (now) {
					nua_subscribe(gateway_ptr->sub_nh, NUTAG_URL(gateway_ptr->register_url), TAG_IF(user_via, SIPTAG_VIA_STR(user_via)), SIPTAG_EVENT_STR(gw_sub_ptr->event), SIPTAG_ACCEPT_STR(gw_sub_ptr->content_type), SIPTAG_TO_STR(gateway_ptr->register_from), SIPTAG_FROM_STR(gateway_ptr->register_from), SIPTAG_CONTACT_STR(gateway_ptr->register_contact), SIPTAG_EXPIRES_STR(gw_sub_ptr->expires_str),	// sofia stack bases its auto-refresh stuff on this
								  TAG_NULL());
					gw_sub_ptr->retry = now + gw_sub_ptr->retry_seconds;
				} else {
					nua_unsubscribe(gateway_ptr->sub_nh,
									NUTAG_URL(gateway_ptr->register_url),
									TAG_IF(user_via, SIPTAG_VIA_STR(user_via)),
									SIPTAG_EVENT_STR(gw_sub_ptr->event),
									SIPTAG_ACCEPT_STR(gw_sub_ptr->content_type),
									SIPTAG_FROM_STR(gateway_ptr->register_from),
									SIPTAG_TO_STR(gateway_ptr->register_from),
									SIPTAG_CONTACT_STR(gateway_ptr->register_contact), SIPTAG_EXPIRES_STR(gw_sub_ptr->expires_str), TAG_NULL());
				}
				gw_sub_ptr->state = SUB_STATE_TRYING;
				break;

			case SUB_STATE_FAILED:
			case SUB_STATE_TRYING:
				if (gw_sub_ptr->retry && now >= gw_sub_ptr->retry) {
					gw_sub_ptr->state = SUB_STATE_UNSUBED;
					gw_sub_ptr->retry = 0;
				}
				break;
			default:
				if (now >= gw_sub_ptr->expires) {
					gw_sub_ptr->state = SUB_STATE_UNSUBED;
				}
				break;
			}
			switch_safe_free(user_via);
		}
	}
	switch_mutex_unlock(mod_sofia_globals.hash_mutex);
}

void sofia_reg_check_gateway(sofia_profile_t *profile, time_t now)
{
	sofia_gateway_t *gateway_ptr, *last = NULL;
	switch_event_t *event;
	char *pkey;

	switch_mutex_lock(mod_sofia_globals.hash_mutex);
	for (gateway_ptr = profile->gateways; gateway_ptr; gateway_ptr = gateway_ptr->next) {
		if (gateway_ptr->deleted && gateway_ptr->state == REG_STATE_NOREG) {
			if (last) {
				last->next = gateway_ptr->next;
			} else {
				profile->gateways = gateway_ptr->next;
			}
			
			pkey = switch_mprintf("%s::%s", profile->name, gateway_ptr->name);

			switch_core_hash_delete(mod_sofia_globals.gateway_hash, pkey);
			switch_core_hash_delete(mod_sofia_globals.gateway_hash, gateway_ptr->name);

			free(pkey);

			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "Deleted gateway %s\n", gateway_ptr->name);
			if (switch_event_create_subclass(&event, SWITCH_EVENT_CUSTOM, MY_EVENT_GATEWAY_DEL) == SWITCH_STATUS_SUCCESS) {
				switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "profile-name", gateway_ptr->profile->name);
				switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "Gateway", gateway_ptr->name);
				switch_event_fire(&event);
			}
			if (gateway_ptr->ob_vars) {
				switch_event_destroy(&gateway_ptr->ob_vars);
			}
			if (gateway_ptr->ib_vars) {
				switch_event_destroy(&gateway_ptr->ib_vars);
			}
		} else {
			last = gateway_ptr;
		}
	}

	for (gateway_ptr = profile->gateways; gateway_ptr; gateway_ptr = gateway_ptr->next) {
		reg_state_t ostate = gateway_ptr->state;
		char *user_via = NULL;

		if (!now) {
			gateway_ptr->state = ostate = REG_STATE_UNREGED;
			gateway_ptr->expires_str = "0";
		}

		if (gateway_ptr->ping && !gateway_ptr->pinging && (now >= gateway_ptr->ping && (ostate == REG_STATE_NOREG || ostate == REG_STATE_REGED)) &&
			!gateway_ptr->deleted) {
			nua_handle_t *nh = nua_handle(profile->nua, NULL, NUTAG_URL(gateway_ptr->register_url), TAG_END());
			sofia_private_t *pvt;

			if (sofia_glue_check_nat(gateway_ptr->profile, gateway_ptr->register_proxy)) {
				user_via = sofia_glue_create_external_via(NULL, gateway_ptr->profile, gateway_ptr->register_transport);
			}

			pvt = malloc(sizeof(*pvt));
			switch_assert(pvt);
			memset(pvt, 0, sizeof(*pvt));
			pvt->destroy_nh = 1;
			pvt->destroy_me = 1;
			switch_copy_string(pvt->gateway_name, gateway_ptr->name, sizeof(pvt->gateway_name));
			nua_handle_bind(nh, pvt);

			gateway_ptr->pinging = 1;
			nua_options(nh,
						TAG_IF(gateway_ptr->register_sticky_proxy, NUTAG_PROXY(gateway_ptr->register_sticky_proxy)),
						TAG_IF(user_via, SIPTAG_VIA_STR(user_via)),
						SIPTAG_TO_STR(gateway_ptr->register_from),
						SIPTAG_CONTACT_STR(gateway_ptr->register_contact), SIPTAG_FROM_STR(gateway_ptr->register_from), TAG_END());

			switch_safe_free(user_via);
			user_via = NULL;
		}

		switch (ostate) {
		case REG_STATE_NOREG:
			if (!gateway_ptr->ping && !gateway_ptr->pinging) {
				gateway_ptr->status = SOFIA_GATEWAY_UP;
			}
			break;
		case REG_STATE_REGISTER:
			if (profile->debug) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Registered %s\n", gateway_ptr->name);
			}

			gateway_ptr->failures = 0;

			if (gateway_ptr->freq > 60) {
				gateway_ptr->expires = now + (gateway_ptr->freq - 15);
			} else {
				gateway_ptr->expires = now + (gateway_ptr->freq - 2);
			}

			gateway_ptr->state = REG_STATE_REGED;
			gateway_ptr->status = SOFIA_GATEWAY_UP;
			break;

		case REG_STATE_UNREGISTER:
			sofia_reg_kill_reg(gateway_ptr);
			gateway_ptr->state = REG_STATE_NOREG;
			break;
		case REG_STATE_UNREGED:
			gateway_ptr->status = SOFIA_GATEWAY_DOWN;
			gateway_ptr->retry = 0;

			if (!gateway_ptr->nh) {
				sofia_reg_new_handle(gateway_ptr, now ? 1 : 0);
			}

			if (sofia_glue_check_nat(gateway_ptr->profile, gateway_ptr->register_proxy)) {
				user_via = sofia_glue_create_external_via(NULL, gateway_ptr->profile, gateway_ptr->register_transport);
			}

			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "Registering %s\n", gateway_ptr->name);

			if (now) {
				nua_register(gateway_ptr->nh,
							 NUTAG_URL(gateway_ptr->register_url),
							 TAG_IF(gateway_ptr->register_sticky_proxy, NUTAG_PROXY(gateway_ptr->register_sticky_proxy)),
							 TAG_IF(user_via, SIPTAG_VIA_STR(user_via)),
							 SIPTAG_TO_STR(gateway_ptr->distinct_to ? gateway_ptr->register_to : gateway_ptr->register_from),
							 SIPTAG_CONTACT_STR(gateway_ptr->register_contact),
							 SIPTAG_FROM_STR(gateway_ptr->register_from),
							 SIPTAG_EXPIRES_STR(gateway_ptr->expires_str),
							 NUTAG_REGISTRAR(gateway_ptr->register_proxy),
							 NUTAG_OUTBOUND("no-options-keepalive"), NUTAG_OUTBOUND("no-validate"), NUTAG_KEEPALIVE(0), TAG_NULL());
				gateway_ptr->retry = now + gateway_ptr->retry_seconds;
			} else {
				nua_unregister(gateway_ptr->nh,
							   NUTAG_URL(gateway_ptr->register_url),
							   TAG_IF(user_via, SIPTAG_VIA_STR(user_via)),
							   SIPTAG_FROM_STR(gateway_ptr->register_from),
							   SIPTAG_TO_STR(gateway_ptr->distinct_to ? gateway_ptr->register_to : gateway_ptr->register_from),
							   SIPTAG_EXPIRES_STR(gateway_ptr->expires_str),
							   NUTAG_REGISTRAR(gateway_ptr->register_proxy),
							   NUTAG_OUTBOUND("no-options-keepalive"), NUTAG_OUTBOUND("no-validate"), NUTAG_KEEPALIVE(0), TAG_NULL());
			}
			gateway_ptr->retry = now + gateway_ptr->retry_seconds;
			gateway_ptr->state = REG_STATE_TRYING;
			switch_safe_free(user_via);
			user_via = NULL;
			break;

		case REG_STATE_FAILED:
			{
				int sec;

				if (gateway_ptr->failure_status == 503) {
					sec = gateway_ptr->retry_seconds;
				} else {
					sec = gateway_ptr->retry_seconds * (gateway_ptr->failures + 1);
				}

				gateway_ptr->retry = switch_epoch_time_now(NULL) + sec;
				gateway_ptr->status = SOFIA_GATEWAY_DOWN;
				gateway_ptr->state = REG_STATE_FAIL_WAIT;
				gateway_ptr->failure_status = 0;

				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "%s Failed Registration, setting retry to %d seconds.\n",
								  gateway_ptr->name, sec);

			}
			break;
		case REG_STATE_FAIL_WAIT:
			if (!gateway_ptr->retry || now >= gateway_ptr->retry) {
				gateway_ptr->state = REG_STATE_UNREGED;
			}
			break;
		case REG_STATE_TRYING:
			if (!gateway_ptr->retry || now >= gateway_ptr->retry) {
				gateway_ptr->state = REG_STATE_FAILED;
			}
			break;
		default:
			if (now >= gateway_ptr->expires) {
				gateway_ptr->state = REG_STATE_UNREGED;
			}
			break;
		}
		if (ostate != gateway_ptr->state) {
			sofia_reg_fire_custom_gateway_state_event(gateway_ptr, 0, NULL);
		}
	}
	switch_mutex_unlock(mod_sofia_globals.hash_mutex);
}


int sofia_reg_find_callback(void *pArg, int argc, char **argv, char **columnNames)
{
	struct callback_t *cbt = (struct callback_t *) pArg;

	switch_copy_string(cbt->val, argv[0], cbt->len);
	cbt->matches++;
	return 0;
}

int sofia_reg_nat_callback(void *pArg, int argc, char **argv, char **columnNames)
{
	sofia_profile_t *profile = (sofia_profile_t *) pArg;
	nua_handle_t *nh;
	char to[128] = "";
	sofia_destination_t *dst = NULL;

	switch_snprintf(to, sizeof(to), "sip:%s@%s", argv[1], argv[2]);
	dst = sofia_glue_get_destination(argv[3]);
	switch_assert(dst);

	nh = nua_handle(profile->nua, NULL, SIPTAG_FROM_STR(profile->url), SIPTAG_TO_STR(to), NUTAG_URL(dst->contact), SIPTAG_CONTACT_STR(profile->url),
					TAG_END());
	nua_handle_bind(nh, &mod_sofia_globals.destroy_private);
	nua_options(nh, TAG_IF(dst->route_uri, NUTAG_PROXY(dst->route_uri)), TAG_IF(dst->route, SIPTAG_ROUTE_STR(dst->route)), TAG_END());

	sofia_glue_free_destination(dst);

	return 0;
}


int sofia_sub_del_callback(void *pArg, int argc, char **argv, char **columnNames)
{
	sofia_profile_t *profile = (sofia_profile_t *) pArg;
	nua_handle_t *nh;

	if (argv[0]) {
		if ((nh = nua_handle_by_call_id(profile->nua, argv[0]))) {
			nua_handle_destroy(nh);
		}
	}
	return 0;
}

void sofia_reg_send_reboot(sofia_profile_t *profile, const char *user, const char *host, const char *contact, const char *user_agent,
						   const char *network_ip)
{
	const char *event = "check-sync";
	const char *contenttype = "application/simple-message-summary";
	const char *body = "";

	if (switch_stristr("snom", user_agent)) {
		event = "check-sync;reboot=true";
	} else if (switch_stristr("linksys", user_agent)) {
		event = "reboot_now";
	} else if (switch_stristr("spa", user_agent)) {
		event = "reboot";
	}

	sofia_glue_send_notify(profile, user, host, event, contenttype, body, contact, network_ip);
}

int sofia_sla_dialog_del_callback(void *pArg, int argc, char **argv, char **columnNames)
{
	sofia_profile_t *profile = (sofia_profile_t *) pArg;
	nua_handle_t *nh = NULL;

	if ((nh = nua_handle_by_call_id(profile->nua, argv[0]))) {
		nua_handle_destroy(nh);
	}

	return 0;
}

int sofia_reg_del_callback(void *pArg, int argc, char **argv, char **columnNames)
{
	switch_event_t *s_event;
	sofia_profile_t *profile = (sofia_profile_t *) pArg;

	if (argc > 12 && atoi(argv[12]) == 1) {
		sofia_reg_send_reboot(profile, argv[1], argv[2], argv[3], argv[7], argv[11]);
	}

	if (argc >= 3) {
		if (switch_event_create_subclass(&s_event, SWITCH_EVENT_CUSTOM, MY_EVENT_EXPIRE) == SWITCH_STATUS_SUCCESS) {
			switch_event_add_header_string(s_event, SWITCH_STACK_BOTTOM, "profile-name", argv[10]);
			switch_event_add_header_string(s_event, SWITCH_STACK_BOTTOM, "call-id", argv[0]);
			switch_event_add_header_string(s_event, SWITCH_STACK_BOTTOM, "user", argv[1]);
			switch_event_add_header_string(s_event, SWITCH_STACK_BOTTOM, "host", argv[2]);
			switch_event_add_header_string(s_event, SWITCH_STACK_BOTTOM, "contact", argv[3]);
			switch_event_add_header_string(s_event, SWITCH_STACK_BOTTOM, "expires", argv[6]);
			switch_event_add_header_string(s_event, SWITCH_STACK_BOTTOM, "user-agent", argv[7]);
			switch_event_fire(&s_event);
		}

		if (switch_event_create(&s_event, SWITCH_EVENT_PRESENCE_OUT) == SWITCH_STATUS_SUCCESS) {
			switch_event_add_header_string(s_event, SWITCH_STACK_BOTTOM, "proto", SOFIA_CHAT_PROTO);
			switch_event_add_header_string(s_event, SWITCH_STACK_BOTTOM, "rpid", "away");
			switch_event_add_header_string(s_event, SWITCH_STACK_BOTTOM, "login", profile->url);

			if (argv[4]) {
				switch_event_add_header_string(s_event, SWITCH_STACK_BOTTOM, "user-agent", argv[4]);
			}

			if (argv[1] && argv[2]) {
				switch_event_add_header(s_event, SWITCH_STACK_BOTTOM, "from", "%s@%s", argv[1], argv[2]);
			}

			switch_event_add_header_string(s_event, SWITCH_STACK_BOTTOM, "status", "Unregistered");
			switch_event_add_header_string(s_event, SWITCH_STACK_BOTTOM, "event_type", "presence");
			switch_event_fire(&s_event);
		}

	}
	return 0;
}

void sofia_reg_expire_call_id(sofia_profile_t *profile, const char *call_id, int reboot)
{
	char *sql = NULL;
	char *sqlextra = NULL;
	char *dup = strdup(call_id);
	char *host = NULL, *user = NULL;

	switch_assert(dup);

	if ((host = strchr(dup, '@'))) {
		*host++ = '\0';
		user = dup;
	} else {
		host = dup;
	}

	if (!host) {
		host = "none";
	}

	if (zstr(user)) {
		sqlextra = switch_mprintf(" or (sip_host='%q')", host);
	} else {
		sqlextra = switch_mprintf(" or (sip_user='%q' and sip_host='%q')", user, host);
	}

	sql = switch_mprintf("select call_id,sip_user,sip_host,contact,status,rpid,expires"
						 ",user_agent,server_user,server_host,profile_name,network_ip"
						 ",%d from sip_registrations where call_id='%q' %s", reboot, call_id, sqlextra);
	switch_safe_free(sqlextra);

	switch_mutex_lock(profile->ireg_mutex);
	sofia_glue_execute_sql_callback(profile, NULL, sql, sofia_reg_del_callback, profile);
	switch_mutex_unlock(profile->ireg_mutex);
	switch_safe_free(sql);

	sql = switch_mprintf("delete from sip_registrations where call_id='%q' or (sip_user='%q' and sip_host='%q')", call_id, user, host);
	sofia_glue_execute_sql(profile, &sql, SWITCH_FALSE);

	switch_safe_free(sql);
	switch_safe_free(dup);

}

void sofia_reg_check_expire(sofia_profile_t *profile, time_t now, int reboot)
{
	char sql[1024];

	switch_mutex_lock(profile->ireg_mutex);

	if (now) {
		switch_snprintf(sql, sizeof(sql), "select call_id,sip_user,sip_host,contact,status,rpid,expires"
						",user_agent,server_user,server_host,profile_name,network_ip"
						",%d from sip_registrations where expires > 0 and expires <= %ld", reboot, (long) now);
	} else {
		switch_snprintf(sql, sizeof(sql), "select call_id,sip_user,sip_host,contact,status,rpid,expires"
						",user_agent,server_user,server_host,profile_name,network_ip" ",%d from sip_registrations where expires > 0", reboot);
	}

	sofia_glue_execute_sql_callback(profile, NULL, sql, sofia_reg_del_callback, profile);
	if (now) {
		switch_snprintf(sql, sizeof(sql), "delete from sip_registrations where expires > 0 and expires <= %ld and hostname='%s'",
						(long) now, mod_sofia_globals.hostname);
	} else {
		switch_snprintf(sql, sizeof(sql), "delete from sip_registrations where expires > 0 and hostname='%s'", mod_sofia_globals.hostname);
	}

	sofia_glue_actually_execute_sql(profile, sql, NULL);



	if (now) {
		switch_snprintf(sql, sizeof(sql), "select call_id from sip_shared_appearance_dialogs where hostname='%s' "
						"and profile_name='%s' and expires <= %ld", mod_sofia_globals.hostname, profile->name, (long) now);

		sofia_glue_execute_sql_callback(profile, NULL, sql, sofia_sla_dialog_del_callback, profile);
		switch_snprintf(sql, sizeof(sql), "delete from sip_registrations where expires > 0 and hostname='%s' and expires <= %ld",
						mod_sofia_globals.hostname, (long) now);


		sofia_glue_actually_execute_sql(profile, sql, NULL);
	}


	if (now) {
		switch_snprintf(sql, sizeof(sql), "delete from sip_presence where expires > 0 and expires <= %ld and hostname='%s'",
						(long) now, mod_sofia_globals.hostname);
	} else {
		switch_snprintf(sql, sizeof(sql), "delete from sip_presence where expires > 0 and hostname='%s'", mod_sofia_globals.hostname);
	}

	sofia_glue_actually_execute_sql(profile, sql, NULL);

	if (now) {
		switch_snprintf(sql, sizeof(sql), "delete from sip_authentication where expires > 0 and expires <= %ld and hostname='%s'",
						(long) now, mod_sofia_globals.hostname);
	} else {
		switch_snprintf(sql, sizeof(sql), "delete from sip_authentication where expires > 0 and hostname='%s'", mod_sofia_globals.hostname);
	}

	sofia_glue_actually_execute_sql(profile, sql, NULL);



	if (now) {
		switch_snprintf(sql, sizeof(sql),
						"select call_id from sip_subscriptions where (expires = -1 or (expires > 0 and expires <= %ld)) and hostname='%s'", (long) now,
						mod_sofia_globals.hostname);
	} else {
		switch_snprintf(sql, sizeof(sql), "select call_id from sip_subscriptions where expires >= -1 and hostname='%s'", mod_sofia_globals.hostname);
	}

	sofia_glue_execute_sql_callback(profile, NULL, sql, sofia_sub_del_callback, profile);

	if (now) {
		switch_snprintf(sql, sizeof(sql), "delete from sip_subscriptions where (expires = -1 or (expires > 0 and expires <= %ld)) and hostname='%s'",
						(long) now, mod_sofia_globals.hostname);
	} else {
		switch_snprintf(sql, sizeof(sql), "delete from sip_subscriptions where expires >= -1 and hostname='%s'", mod_sofia_globals.hostname);
	}

	sofia_glue_actually_execute_sql(profile, sql, NULL);


	if (now) {
		switch_snprintf(sql, sizeof(sql), "delete from sip_dialogs where (expires = -1 or (expires > 0 and expires <= %ld)) and hostname='%s'",
						(long) now, mod_sofia_globals.hostname);
	} else {
		switch_snprintf(sql, sizeof(sql), "delete from sip_dialogs where expires >= -1 and hostname='%s'", mod_sofia_globals.hostname);
	}

	sofia_glue_actually_execute_sql(profile, sql, NULL);


	if (now && sofia_test_pflag(profile, PFLAG_NAT_OPTIONS_PING)) {
		switch_snprintf(sql, sizeof(sql), "select call_id,sip_user,sip_host,contact,status,rpid,"
						"expires,user_agent,server_user,server_host,profile_name"
						" from sip_registrations where (status like '%%AUTO-NAT%%' "
						"or status like '%%UDP-NAT%%') and hostname='%s'", mod_sofia_globals.hostname);

		sofia_glue_execute_sql_callback(profile, NULL, sql, sofia_reg_nat_callback, profile);
	}

	switch_mutex_unlock(profile->ireg_mutex);

}

char *sofia_reg_find_reg_url(sofia_profile_t *profile, const char *user, const char *host, char *val, switch_size_t len)
{
	struct callback_t cbt = { 0 };
	char sql[512] = "";

	if (!user) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Called with null user!\n");
		return NULL;
	}

	cbt.val = val;
	cbt.len = len;

	if (host) {
		switch_snprintf(sql, sizeof(sql), "select contact from sip_registrations where sip_user='%s' and (sip_host='%s' or presence_hosts like '%%%s%%')",
						user, host, host);
	} else {
		switch_snprintf(sql, sizeof(sql), "select contact from sip_registrations where sip_user='%s'", user);
	}


	sofia_glue_execute_sql_callback(profile, profile->ireg_mutex, sql, sofia_reg_find_callback, &cbt);


	if (cbt.matches) {
		return val;
	} else {
		return NULL;
	}
}


void sofia_reg_auth_challenge(nua_t *nua, sofia_profile_t *profile, nua_handle_t *nh, sofia_regtype_t regtype, const char *realm, int stale)
{
	switch_uuid_t uuid;
	char uuid_str[SWITCH_UUID_FORMATTED_LENGTH + 1];
	char *sql, *auth_str;

	switch_uuid_get(&uuid);
	switch_uuid_format(uuid_str, &uuid);

	sql = switch_mprintf("insert into sip_authentication (nonce,expires,profile_name,hostname, last_nc) "
						 "values('%q', %ld, '%q', '%q', 0)", uuid_str,
						 switch_epoch_time_now(NULL) + (profile->nonce_ttl ? profile->nonce_ttl : DEFAULT_NONCE_TTL),
						 profile->name, mod_sofia_globals.hostname);
	switch_assert(sql != NULL);
	sofia_glue_actually_execute_sql(profile, sql, profile->ireg_mutex);
	switch_safe_free(sql);

	auth_str = switch_mprintf("Digest realm=\"%q\", nonce=\"%q\",%s algorithm=MD5, qop=\"auth\"", realm, uuid_str, stale ? " stale=true," : "");

	if (regtype == REG_REGISTER) {
		nua_respond(nh, SIP_401_UNAUTHORIZED, TAG_IF(nua, NUTAG_WITH_THIS(nua)), SIPTAG_WWW_AUTHENTICATE_STR(auth_str), TAG_END());
	} else if (regtype == REG_INVITE) {
		nua_respond(nh, SIP_407_PROXY_AUTH_REQUIRED, TAG_IF(nua, NUTAG_WITH_THIS(nua)), SIPTAG_PROXY_AUTHENTICATE_STR(auth_str), TAG_END());
	}

	switch_safe_free(auth_str);
}

uint8_t sofia_reg_handle_register(nua_t *nua, sofia_profile_t *profile, nua_handle_t *nh, sip_t const *sip, sofia_regtype_t regtype, char *key,
								  uint32_t keylen, switch_event_t **v_event, const char *is_nat)
{
	sip_to_t const *to = NULL;
	sip_from_t const *from = NULL;
	sip_expires_t const *expires = NULL;
	sip_authorization_t const *authorization = NULL;
	sip_contact_t const *contact = NULL;
	char *sql;
	switch_event_t *s_event;
	const char *to_user = NULL;
	const char *to_host = NULL;
	char *mwi_account = NULL;
	char *dup_mwi_account = NULL;
	char *mwi_user = NULL;
	char *mwi_host = NULL;
	char *var = NULL;
	const char *from_user = NULL;
	const char *from_host = NULL;
	const char *reg_host = profile->reg_db_domain;
	char contact_str[1024] = "";
	int nat_hack = 0;
	uint8_t multi_reg = 0, multi_reg_contact = 0, avoid_multi_reg = 0;
	uint8_t stale = 0, forbidden = 0;
	auth_res_t auth_res;
	long exptime = 300;
	switch_event_t *event;
	const char *rpid = "unknown";
	const char *display = "\"user\"";
	char network_ip[80];
	char network_port_c[6];
	char url_ip[80];
	char *register_gateway = NULL;
	int network_port;
	const char *reg_desc = "Registered";
	const char *call_id = NULL;
	char *force_user;
	char received_data[128] = "";
	char *path_val = NULL;
	switch_event_t *auth_params = NULL;
	int r = 0;
	long reg_count = 0;

	/* all callers must confirm that sip, sip->sip_request and sip->sip_contact are not NULL */
	switch_assert(sip != NULL && sip->sip_contact != NULL && sip->sip_request != NULL);

	sofia_glue_get_addr(nua_current_request(nua), network_ip, sizeof(network_ip), &network_port);

	snprintf(network_port_c, sizeof(network_port_c), "%d", network_port);

	snprintf(url_ip, sizeof(url_ip), (msg_addrinfo(nua_current_request(nua)))->ai_addr->sa_family == AF_INET6 ? "[%s]" : "%s", network_ip);

	expires = sip->sip_expires;
	authorization = sip->sip_authorization;
	contact = sip->sip_contact;
	to = sip->sip_to;
	from = sip->sip_from;

	if (from) {
		from_user = from->a_url->url_user;
		from_host = from->a_url->url_host;
	}

	if (to) {
		to_user = to->a_url->url_user;
		to_host = to->a_url->url_host;
	}

	if (!to_user)
		to_user = from_user;
	if (!to_host)
		to_host = from_host;

	if (!to_user || !to_host) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Can not do authorization without a complete header\n");
		nua_respond(nh, SIP_401_UNAUTHORIZED, NUTAG_WITH_THIS(nua), TAG_END());
		switch_goto_int(r, 1, end);
	}

	if (!reg_host) {
		reg_host = to_host;
	}

	if (contact->m_url) {
		const char *port = contact->m_url->url_port;
		char new_port[25] = "";
		const char *contact_host = contact->m_url->url_host;
		char *path_encoded = NULL;
		int path_encoded_len = 0;
		const char *proto = "sip";
		int is_tls = 0, is_tcp = 0;


		if (switch_stristr("transport=tls", sip->sip_contact->m_url->url_params)) {
			is_tls += 1;
		}

		if (sip->sip_contact->m_url->url_type == url_sips) {
			proto = "sips";
			is_tls += 2;
		}

		if (switch_stristr("transport=tcp", sip->sip_contact->m_url->url_params)) {
			is_tcp = 1;
		}

		display = contact->m_display;

		if (is_nat) {
			if (is_tls) {
				reg_desc = "Registered(TLS-NAT)";
			} else if (is_tcp) {
				reg_desc = "Registered(TCP-NAT)";
			} else {
				reg_desc = "Registered(UDP-NAT)";
			}
			//contact_host = url_ip;
			//switch_snprintf(new_port, sizeof(new_port), ":%d", network_port);
			//port = NULL;
		} else {
			if (is_tls) {
				reg_desc = "Registered(TLS)";
			} else if (is_tcp) {
				reg_desc = "Registered(TCP)";
			} else {
				reg_desc = "Registered(UDP)";
			}
		}

		if (zstr(display)) {
			if (to) {
				display = to->a_display;
				if (zstr(display)) {
					display = "\"user\"";
				}
			}
		}

		if (sip->sip_path) {
			path_val = sip_header_as_string(nua_handle_home(nh), (void *) sip->sip_path);
			path_encoded_len = (strlen(path_val) * 3) + 1;
			switch_zmalloc(path_encoded, path_encoded_len);
			switch_copy_string(path_encoded, ";fs_path=", 10);
			switch_url_encode(path_val, path_encoded + 9, path_encoded_len - 9);
		} else if (is_nat) {
			char my_contact_str[1024];
			if (sip->sip_contact->m_url->url_params) {
				switch_snprintf(my_contact_str, sizeof(my_contact_str), "sip:%s@%s:%d;%s",
								contact->m_url->url_user, url_ip, network_port, sip->sip_contact->m_url->url_params);
			} else {
				switch_snprintf(my_contact_str, sizeof(my_contact_str), "sip:%s@%s:%d", contact->m_url->url_user, url_ip, network_port);
			}

			path_encoded_len = (strlen(my_contact_str) * 3) + 1;

			switch_zmalloc(path_encoded, path_encoded_len);
			switch_copy_string(path_encoded, ";fs_path=", 10);
			switch_url_encode(my_contact_str, path_encoded + 9, path_encoded_len - 9);
			exptime = 30;
		}

		if (port) {
			switch_snprintf(new_port, sizeof(new_port), ":%s", port);
		}

		if (is_nat && sofia_test_pflag(profile, PFLAG_RECIEVED_IN_NAT_REG_CONTACT)) {
			switch_snprintf(received_data, sizeof(received_data), ";received=%s:%d", url_ip, network_port);
		}

		if (contact->m_url->url_params) {
			switch_snprintf(contact_str, sizeof(contact_str), "%s <%s:%s@%s%s;%s%s%s%s>",
							display, proto, contact->m_url->url_user, contact_host, new_port,
							contact->m_url->url_params, received_data, is_nat ? ";fs_nat=yes" : "", path_encoded ? path_encoded : "");
		} else {
			switch_snprintf(contact_str, sizeof(contact_str), "%s <%s:%s@%s%s%s%s%s>", display, proto, contact->m_url->url_user, contact_host, new_port,
							received_data, is_nat ? ";fs_nat=yes" : "", path_encoded ? path_encoded : "");
		}

		switch_safe_free(path_encoded);
	}

	if (expires) {
		exptime = expires->ex_delta;
	} else if (contact->m_expires) {
		exptime = atol(contact->m_expires);
	}

	if (regtype == REG_REGISTER) {
		authorization = sip->sip_authorization;
	} else if (regtype == REG_INVITE) {
		authorization = sip->sip_proxy_authorization;
	}

	if (regtype == REG_AUTO_REGISTER || (regtype == REG_REGISTER && sofia_test_pflag(profile, PFLAG_BLIND_REG))) {
		regtype = REG_REGISTER;
		goto reg;
	}

	if (authorization) {
		char *v_contact_str;
		if ((auth_res = sofia_reg_parse_auth(profile, authorization, sip, sip->sip_request->rq_method_name,
											 key, keylen, network_ip, v_event, exptime, regtype, to_user, &auth_params, &reg_count)) == AUTH_STALE) {
			stale = 1;
		}

		if (exptime && v_event && *v_event) {
			char *exp_var;
			char *allow_multireg = NULL;

			allow_multireg = switch_event_get_header(*v_event, "sip-allow-multiple-registrations");
			if (allow_multireg && switch_false(allow_multireg)) {
				avoid_multi_reg = 1;
			}

			register_gateway = switch_event_get_header(*v_event, "sip-register-gateway");

			/* Allow us to force the SIP user to be something specific - needed if 
			 * we - for example - want to be able to ensure that the username a UA can
			 * be contacted at is the same one that they used for authentication.
			 */
			if ((force_user = switch_event_get_header(*v_event, "sip-force-user"))) {
				to_user = force_user;
			}

			if ((v_contact_str = switch_event_get_header(*v_event, "sip-force-contact"))) {
				if (!strcasecmp(v_contact_str, "NDLB-connectile-dysfunction-2.0")) {
					char *path_encoded;
					size_t path_encoded_len;
					char my_contact_str[1024];

					switch_snprintf(my_contact_str, sizeof(my_contact_str), "sip:%s@%s:%d", contact->m_url->url_user, url_ip, network_port);
					path_encoded_len = (strlen(my_contact_str) * 3) + 1;

					switch_zmalloc(path_encoded, path_encoded_len);
					switch_copy_string(path_encoded, ";fs_nat=yes;fs_path=", 21);
					switch_url_encode(my_contact_str, path_encoded + 20, path_encoded_len - 20);
					reg_desc = "Registered(AUTO-NAT-2.0)";
					exptime = 30;
					switch_snprintf(contact_str + strlen(contact_str), sizeof(contact_str) - strlen(contact_str), "%s", path_encoded);
					free(path_encoded);
				} else {
					if (*received_data && sofia_test_pflag(profile, PFLAG_RECIEVED_IN_NAT_REG_CONTACT)) {
						switch_snprintf(received_data, sizeof(received_data), ";received=%s:%d", url_ip, network_port);
					}

					if (!strcasecmp(v_contact_str, "nat-connectile-dysfunction") ||
						!strcasecmp(v_contact_str, "NDLB-connectile-dysfunction") || !strcasecmp(v_contact_str, "NDLB-tls-connectile-dysfunction")) {
						if (contact->m_url->url_params) {
							switch_snprintf(contact_str, sizeof(contact_str), "%s <sip:%s@%s:%d;%s%s;fs_nat=yes>",
											display, contact->m_url->url_user, url_ip, network_port, contact->m_url->url_params, received_data);
						} else {
							switch_snprintf(contact_str, sizeof(contact_str), "%s <sip:%s@%s:%d%s;fs_nat=yes>", display, contact->m_url->url_user, url_ip,
											network_port, received_data);
						}
						if (switch_stristr(v_contact_str, "transport=tls")) {
							reg_desc = "Registered(TLSHACK)";
						} else {
							reg_desc = "Registered(AUTO-NAT)";
							exptime = 30;
						}
						nat_hack = 1;
					} else {
						char *p;
						switch_copy_string(contact_str, v_contact_str, sizeof(contact_str));
						for (p = contact_str; p && *p; p++) {
							if (*p == '\'' || *p == '[' || *p == ']') {
								*p = '"';
							}
						}
					}
				}
			}

			if ((exp_var = switch_event_get_header(*v_event, "sip-force-expires"))) {
				int tmp = atoi(exp_var);
				if (tmp > 0) {
					exptime = tmp;
				}
			}
		}

		if (auth_res != AUTH_OK && !stale) {
			if (profile->debug) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Send %s for [%s@%s]\n", forbidden ? "forbidden" : "challenge", to_user, to_host);
			}
			if (auth_res == AUTH_FORBIDDEN) {
				nua_respond(nh, SIP_403_FORBIDDEN, NUTAG_WITH_THIS(nua), TAG_END());

				/* Log line added to support Fail2Ban */
				if (sofia_test_pflag(profile, PFLAG_LOG_AUTH_FAIL)) {
					if (regtype == REG_REGISTER) {
						switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "SIP auth failure (REGISTER) on sofia profile '%s' "
										  "for [%s@%s] from ip %s\n", profile->name, to_user, to_host, network_ip);
					} else if (regtype == REG_INVITE) {
						switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "SIP auth failure (INVITE) on sofia profile '%s' "
										  "for [%s@%s] from ip %s\n", profile->name, to_user, to_host, network_ip);
					}
				}
			} else {
				nua_respond(nh, SIP_401_UNAUTHORIZED, NUTAG_WITH_THIS(nua), TAG_END());
			}
			switch_goto_int(r, 1, end);
		}
	}

	if (!authorization || stale) {
		const char *realm = profile->challenge_realm;

		if (zstr(realm) || !strcasecmp(realm, "auto_to")) {
			realm = to_host;
		} else if (!strcasecmp(realm, "auto_from")) {
			realm = from_host;
		}

		if (regtype == REG_REGISTER) {
			sofia_reg_auth_challenge(nua, profile, nh, regtype, realm, stale);
			if (profile->debug) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Requesting Registration from: [%s@%s]\n", to_user, to_host);
			}
		} else {
			sofia_reg_auth_challenge(nua, profile, nh, regtype, realm, stale);
		}
		switch_goto_int(r, 1, end);
	}
  reg:


	if (v_event && *v_event && (var = switch_event_get_header(*v_event, "sip-force-extension"))) {
		to_user = var;
	}

	if (v_event && *v_event && (mwi_account = switch_event_get_header(*v_event, "mwi-account"))) {
		dup_mwi_account = strdup(mwi_account);
		switch_assert(dup_mwi_account != NULL);
		sofia_glue_get_user_host(dup_mwi_account, &mwi_user, &mwi_host);
	}

	if (!mwi_user) {
		mwi_user = (char *) to_user;
	}
	if (!mwi_host) {
		mwi_host = (char *) reg_host;
	}

	if (regtype != REG_REGISTER) {
		switch_goto_int(r, 0, end);
	}

	call_id = sip->sip_call_id->i_id;
	switch_assert(call_id);

	/* Does this profile supports multiple registrations ? */
	multi_reg = (sofia_test_pflag(profile, PFLAG_MULTIREG)) ? 1 : 0;
	multi_reg_contact = (sofia_test_pflag(profile, PFLAG_MULTIREG_CONTACT)) ? 1 : 0;

	if (multi_reg && avoid_multi_reg) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG,
						  "Disabling multiple registrations on a per-user basis for %s@%s\n", switch_str_nil(to_user), switch_str_nil(to_host));
		multi_reg = 0;
	}

	if (exptime) {
		const char *agent = "dunno";
		char guess_ip4[256];
		const char *username = "unknown";
		const char *realm = reg_host;

		if (auth_params) {
			username = switch_event_get_header(auth_params, "sip_auth_username");
			realm = switch_event_get_header(auth_params, "sip_auth_realm");
		}

		if (sip->sip_user_agent) {
			agent = sip->sip_user_agent->g_string;
		}

		if (multi_reg) {
			if (multi_reg_contact) {
				sql =
					switch_mprintf("delete from sip_registrations where sip_user='%q' and sip_host='%q' and contact='%q'", to_user, reg_host, contact_str);
			} else {
				sql = switch_mprintf("delete from sip_registrations where call_id='%q'", call_id);
			}
		} else {
			sql = switch_mprintf("delete from sip_registrations where sip_user='%q' and sip_host='%q'", to_user, reg_host);
		}
		switch_mutex_lock(profile->ireg_mutex);
		sofia_glue_execute_sql(profile, &sql, SWITCH_TRUE);

		switch_find_local_ip(guess_ip4, sizeof(guess_ip4), NULL, AF_INET);
		sql = switch_mprintf("insert into sip_registrations "
							 "(call_id,sip_user,sip_host,presence_hosts,contact,status,rpid,expires,"
							 "user_agent,server_user,server_host,profile_name,hostname,network_ip,network_port,sip_username,sip_realm,"
							 "mwi_user,mwi_host, orig_server_host, orig_hostname) "
							 "values ('%q','%q', '%q','%q','%q','%q', '%q', %ld, '%q', '%q', '%q', '%q', '%q', '%q', '%q','%q','%q','%q','%q','%q','%q')", 
							 call_id, to_user, reg_host, profile->presence_hosts ? profile->presence_hosts : reg_host, 
							 contact_str, reg_desc, rpid, (long) switch_epoch_time_now(NULL) + (long) exptime * 2, 
							 agent, from_user, guess_ip4, profile->name, mod_sofia_globals.hostname, network_ip, network_port_c, username, realm, 
							 mwi_user, mwi_host, guess_ip4, mod_sofia_globals.hostname);
							 
		if (sql) {
			sofia_glue_execute_sql(profile, &sql, SWITCH_TRUE);
		}

		switch_mutex_unlock(profile->ireg_mutex);

		if (switch_event_create_subclass(&s_event, SWITCH_EVENT_CUSTOM, MY_EVENT_REGISTER) == SWITCH_STATUS_SUCCESS) {
			switch_event_add_header_string(s_event, SWITCH_STACK_BOTTOM, "profile-name", profile->name);
			switch_event_add_header_string(s_event, SWITCH_STACK_BOTTOM, "from-user", to_user);
			switch_event_add_header_string(s_event, SWITCH_STACK_BOTTOM, "from-host", reg_host);
			switch_event_add_header_string(s_event, SWITCH_STACK_BOTTOM, "presence-hosts", profile->presence_hosts ? profile->presence_hosts : reg_host);
			switch_event_add_header_string(s_event, SWITCH_STACK_BOTTOM, "contact", contact_str);
			switch_event_add_header_string(s_event, SWITCH_STACK_BOTTOM, "call-id", call_id);
			switch_event_add_header_string(s_event, SWITCH_STACK_BOTTOM, "rpid", rpid);
			switch_event_add_header_string(s_event, SWITCH_STACK_BOTTOM, "status", reg_desc);
			switch_event_add_header(s_event, SWITCH_STACK_BOTTOM, "expires", "%ld", (long) exptime);
			switch_event_add_header_string(s_event, SWITCH_STACK_BOTTOM, "to-user", from_user);
			switch_event_add_header_string(s_event, SWITCH_STACK_BOTTOM, "to-host", from_host);
			switch_event_add_header_string(s_event, SWITCH_STACK_BOTTOM, "network-ip", network_ip);
			switch_event_add_header_string(s_event, SWITCH_STACK_BOTTOM, "network-port", network_port_c);
			switch_event_add_header_string(s_event, SWITCH_STACK_BOTTOM, "username", username);
			switch_event_add_header_string(s_event, SWITCH_STACK_BOTTOM, "realm", realm);
			switch_event_add_header_string(s_event, SWITCH_STACK_BOTTOM, "user-agent", agent);
			switch_event_fire(&s_event);
		}



		if (profile->debug) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG,
							  "Register:\nFrom:    [%s@%s]\nContact: [%s]\nExpires: [%ld]\n", to_user, reg_host, contact_str, (long) exptime);
		}

#if 0
		if (switch_event_create(&event, SWITCH_EVENT_PRESENCE_IN) == SWITCH_STATUS_SUCCESS) {
			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "proto", SOFIA_CHAT_PROTO);
			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "rpid", rpid);
			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "login", profile->url);
			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "user-agent",
										   (sip && sip->sip_user_agent) ? sip->sip_user_agent->g_string : "unknown");
			switch_event_add_header(event, SWITCH_STACK_BOTTOM, "from", "%s@%s", to_user, reg_host);
			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "status", "Registered");
			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "event_type", "presence");
			switch_event_fire(&event);
		}
#else

		if (sofia_test_pflag(profile, PFLAG_MESSAGE_QUERY_ON_REGISTER) ||
			(reg_count == 1 && sofia_test_pflag(profile, PFLAG_MESSAGE_QUERY_ON_FIRST_REGISTER))) {

			if (switch_event_create(&event, SWITCH_EVENT_PRESENCE_PROBE) == SWITCH_STATUS_SUCCESS) {
				switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "proto", "sip");
				switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "login", profile->url);
				switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "rpid", rpid);
				switch_event_add_header(event, SWITCH_STACK_BOTTOM, "from", "%s@%s", to_user, reg_host);
				switch_event_add_header(event, SWITCH_STACK_BOTTOM, "to", "%s@%s", to_user, reg_host);
				switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "status", "Registered");
				switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "event_subtype", "probe");
				switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "event_type", "presence");
				switch_event_fire(&event);
			}
		}
#endif


	} else {

		if (switch_event_create(&event, SWITCH_EVENT_PRESENCE_OUT) == SWITCH_STATUS_SUCCESS) {
			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "proto", SOFIA_CHAT_PROTO);
			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "rpid", rpid);
			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "login", profile->url);
			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "user-agent",
										   (sip && sip->sip_user_agent) ? sip->sip_user_agent->g_string : "unknown");
			switch_event_add_header(event, SWITCH_STACK_BOTTOM, "from", "%s@%s", to_user, reg_host);
			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "status", "Unregistered");
			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "event_type", "presence");
			switch_event_fire(&event);
		}
#if 0
		if (switch_event_create(&event, SWITCH_EVENT_PRESENCE_OUT) == SWITCH_STATUS_SUCCESS) {
			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "proto", "sip");
			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "login", profile->url);
			switch_event_add_header(event, SWITCH_STACK_BOTTOM, "from", "%s+%s@%s", SOFIA_CHAT_PROTO, to_user, reg_host);

			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "status", "unavailable");
			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "rpid", rpid);
			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "event_type", "presence");
			switch_event_fire(&event);
		}
#endif

		if (multi_reg) {
			char *icontact, *p;
			icontact = sofia_glue_get_url_from_contact(contact_str, 1);
			if ((p = strchr(icontact, ';'))) {
				*p = '\0';
			}
			if ((p = strchr(icontact + 4, ':'))) {
				*p = '\0';
			}

			if (multi_reg_contact) {
				sql =
					switch_mprintf("delete from sip_subscriptions where sip_user='%q' and sip_host='%q' and contact='%q'", to_user, reg_host, contact_str);
			} else {
				sql = switch_mprintf("delete from sip_subscriptions where call_id='%q'", call_id);
			}

			sofia_glue_execute_sql(profile, &sql, SWITCH_TRUE);

			if (multi_reg_contact) {
				sql =
					switch_mprintf("delete from sip_registrations where sip_user='%q' and sip_host='%q' and contact='%q'", to_user, reg_host, contact_str);
			} else {
				sql = switch_mprintf("delete from sip_registrations where call_id='%q'", call_id);
			}

			sofia_glue_execute_sql(profile, &sql, SWITCH_TRUE);

			switch_safe_free(icontact);
		} else {
			if ((sql = switch_mprintf("delete from sip_subscriptions where sip_user='%q' and sip_host='%q'", to_user, reg_host))) {
				sofia_glue_execute_sql(profile, &sql, SWITCH_TRUE);
			}

			if ((sql = switch_mprintf("delete from sip_registrations where sip_user='%q' and sip_host='%q'", to_user, reg_host))) {
				sofia_glue_execute_sql(profile, &sql, SWITCH_TRUE);
			}
		}
	}


	if (regtype == REG_REGISTER) {
		char exp_param[128] = "";
		char date[80] = "";

		s_event = NULL;

		if (exptime) {
			switch_snprintf(exp_param, sizeof(exp_param), "expires=%ld", exptime);
			sip_contact_add_param(nua_handle_home(nh), sip->sip_contact, exp_param);

			if (sofia_test_pflag(profile, PFLAG_MESSAGE_QUERY_ON_REGISTER) ||
				(reg_count == 1 && sofia_test_pflag(profile, PFLAG_MESSAGE_QUERY_ON_FIRST_REGISTER))) {
				if (switch_event_create(&s_event, SWITCH_EVENT_MESSAGE_QUERY) == SWITCH_STATUS_SUCCESS) {
					switch_event_add_header(s_event, SWITCH_STACK_BOTTOM, "Message-Account", "sip:%s@%s", mwi_user, mwi_host);
					switch_event_add_header_string(s_event, SWITCH_STACK_BOTTOM, "VM-Sofia-Profile", profile->name);
					switch_event_add_header_string(s_event, SWITCH_STACK_BOTTOM, "VM-Call-ID", call_id);
				}
			}
		} else {
			if (switch_event_create_subclass(&s_event, SWITCH_EVENT_CUSTOM, MY_EVENT_UNREGISTER) == SWITCH_STATUS_SUCCESS) {
				switch_event_add_header_string(s_event, SWITCH_STACK_BOTTOM, "profile-name", profile->name);
				switch_event_add_header_string(s_event, SWITCH_STACK_BOTTOM, "from-user", to_user);
				switch_event_add_header_string(s_event, SWITCH_STACK_BOTTOM, "from-host", reg_host);
				switch_event_add_header_string(s_event, SWITCH_STACK_BOTTOM, "contact", contact_str);
				switch_event_add_header_string(s_event, SWITCH_STACK_BOTTOM, "call-id", call_id);
				switch_event_add_header_string(s_event, SWITCH_STACK_BOTTOM, "rpid", rpid);
				switch_event_add_header(s_event, SWITCH_STACK_BOTTOM, "expires", "%ld", (long) exptime);
			}
		}

		switch_rfc822_date(date, switch_micro_time_now());
		nua_respond(nh, SIP_200_OK, SIPTAG_CONTACT(sip->sip_contact),
					TAG_IF(path_val, SIPTAG_PATH_STR(path_val)), NUTAG_WITH_THIS(nua), SIPTAG_DATE_STR(date), TAG_END());

		if (s_event) {
			switch_event_fire(&s_event);
		}

		if (*contact_str && sofia_test_pflag(profile, PFLAG_MANAGE_SHARED_APPEARANCE_SYLANTRO)) {
			sofia_sla_handle_register(nua, profile, sip, exptime, contact_str);
		}

		switch_goto_int(r, 1, end);
	}


  end:
	switch_safe_free(dup_mwi_account);

	if (auth_params) {
		switch_event_destroy(&auth_params);
	}

	return (uint8_t) r;
}



void sofia_reg_handle_sip_i_register(nua_t *nua, sofia_profile_t *profile, nua_handle_t *nh, sofia_private_t *sofia_private, sip_t const *sip,
									 tagi_t tags[])
{
	char key[128] = "";
	switch_event_t *v_event = NULL;
	char network_ip[80];
	sofia_regtype_t type = REG_REGISTER;
	int network_port = 0;
	char *is_nat = NULL;

	sofia_glue_get_addr(nua_current_request(nua), network_ip, sizeof(network_ip), &network_port);

	if (!(sip->sip_contact && sip->sip_contact->m_url)) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "NO CONTACT!\n");
		nua_respond(nh, 400, "Missing Contact Header", TAG_END());
		goto end;
	}

	if (!(profile->mflags & MFLAG_REGISTER)) {
		nua_respond(nh, SIP_403_FORBIDDEN, NUTAG_WITH_THIS(nua), TAG_END());
		goto end;
	}

	if (sofia_test_pflag(profile, PFLAG_AGGRESSIVE_NAT_DETECTION)) {
		if (sip && sip->sip_via) {
			const char *port = sip->sip_via->v_port;
			const char *host = sip->sip_via->v_host;

			if (host && sip->sip_via->v_received) {
				is_nat = "via received";
			} else if (host && strcmp(network_ip, host)) {
				is_nat = "via host";
			} else if (port && atoi(port) != network_port) {
				is_nat = "via port";
			}
		}
	}

	if (!is_nat && profile->nat_acl_count) {
		uint32_t x = 0;
		int ok = 1;
		char *last_acl = NULL;
		const char *contact_host = NULL;

		if (sip && sip->sip_contact && sip->sip_contact->m_url) {
			contact_host = sip->sip_contact->m_url->url_host;
		}

		if (!zstr(contact_host)) {
			for (x = 0; x < profile->nat_acl_count; x++) {
				last_acl = profile->nat_acl[x];
				if (!(ok = switch_check_network_list_ip(contact_host, last_acl))) {
					break;
				}
			}

			if (ok) {
				is_nat = last_acl;
			}
		}
	}

	if (profile->reg_acl_count) {
		uint32_t x = 0;
		int ok = 1;
		char *last_acl = NULL;

		for (x = 0; x < profile->reg_acl_count; x++) {
			last_acl = profile->reg_acl[x];
			if (!(ok = switch_check_network_list_ip(network_ip, last_acl))) {
				break;
			}
		}

		if (ok && !sofia_test_pflag(profile, PFLAG_BLIND_REG)) {
			type = REG_AUTO_REGISTER;
		} else if (!ok) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "IP %s Rejected by register acl \"%s\"\n", network_ip, profile->reg_acl[x]);
			nua_respond(nh, SIP_403_FORBIDDEN, NUTAG_WITH_THIS(nua), TAG_END());
			goto end;
		}
	}

	if (!sip || !sip->sip_request || !sip->sip_request->rq_method_name) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Received an invalid packet!\n");
		nua_respond(nh, SIP_500_INTERNAL_SERVER_ERROR, TAG_END());
		goto end;
	}

	if (is_nat && profile->local_network && switch_check_network_list_ip(network_ip, profile->local_network)) {
		if (profile->debug) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "IP %s is on local network, not seting NAT mode.\n", network_ip);
		}
		is_nat = NULL;
	}

	sofia_reg_handle_register(nua, profile, nh, sip, type, key, sizeof(key), &v_event, is_nat);

	if (v_event) {
		switch_event_destroy(&v_event);
	}

  end:

	nua_handle_destroy(nh);

}


void sofia_reg_handle_sip_r_register(int status,
									 char const *phrase,
									 nua_t *nua, sofia_profile_t *profile, nua_handle_t *nh, sofia_private_t *sofia_private, sip_t const *sip,
									 tagi_t tags[])
{
	if (status >= 500) {
		if (sofia_private && sofia_private->gateway) {
			nua_handle_destroy(sofia_private->gateway->nh);
			sofia_private->gateway->nh = NULL;
		} else {
			nua_handle_destroy(nh);
		}
	}

	if (sofia_private && sofia_private->gateway) {
		reg_state_t ostate = sofia_private->gateway->state;
		switch (status) {
		case 200:
			if (sip && sip->sip_contact) {
				sip_contact_t *contact = sip->sip_contact;
				const char *new_expires;
				uint32_t expi;
				if (contact->m_next) {
					const char *sipip = profile->extsipip ? profile->extsipip : profile->sipip;
					for (; contact && strcasecmp(contact->m_url->url_host, sipip); contact = contact->m_next);
				}

				if (!contact) {
					contact = sip->sip_contact;
				}

				if (contact->m_expires) {
					new_expires = contact->m_expires;
					expi = (uint32_t) atoi(new_expires);

					if (expi > 0 && expi != sofia_private->gateway->freq) {
						sofia_private->gateway->freq = expi;
						sofia_private->gateway->expires_str = switch_core_sprintf(sofia_private->gateway->pool, "%d", expi);

						if (expi > 60) {
							sofia_private->gateway->expires = switch_epoch_time_now(NULL) + (expi - 15);
						} else {
							sofia_private->gateway->expires = switch_epoch_time_now(NULL) + (expi - 2);
						}


						switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG,
										  "Changing expire time to %d by request of proxy %s\n", expi, sofia_private->gateway->register_proxy);
					}
				}
			}
			sofia_private->gateway->state = REG_STATE_REGISTER;
			break;
		case 100:
			break;
		default:
			sofia_private->gateway->state = REG_STATE_FAILED;
			sofia_private->gateway->failure_status = status;
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "%s Registration Failed with status %s [%d]. failure #%d\n",
							  sofia_private->gateway->name, switch_str_nil(phrase), status, ++sofia_private->gateway->failures);
			break;
		}
		if (ostate != sofia_private->gateway->state) {
			sofia_reg_fire_custom_gateway_state_event(sofia_private->gateway, status, phrase);
		}
	}
}

void sofia_reg_handle_sip_r_challenge(int status,
									  char const *phrase,
									  nua_t *nua, sofia_profile_t *profile, nua_handle_t *nh, sofia_private_t *sofia_private,
									  switch_core_session_t *session, sofia_gateway_t *gateway, sip_t const *sip, tagi_t tags[])
{
	sip_www_authenticate_t const *authenticate = NULL;
	char const *realm = NULL;
	char const *scheme = NULL;
	int indexnum;
	char *cur;
	char authentication[256] = "";
	int ss_state;
	sofia_gateway_t *var_gateway = NULL;
	const char *gw_name = NULL;
	switch_channel_t *channel = NULL;
	const char *sip_auth_username = NULL;
	const char *sip_auth_password = NULL;

	if (session && (channel = switch_core_session_get_channel(session))) {
		sip_auth_username = switch_channel_get_variable(channel, "sip_auth_username");
		sip_auth_password = switch_channel_get_variable(channel, "sip_auth_password");
	}

	if (sofia_private && *sofia_private->auth_gateway_name) {
		gw_name = sofia_private->auth_gateway_name;
	}

	if (session) {
		private_object_t *tech_pvt;

		if ((tech_pvt = switch_core_session_get_private(session)) && sofia_test_flag(tech_pvt, TFLAG_REFER)) {
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "Received reply from REFER\n");
			goto end;
		}

		gw_name = switch_channel_get_variable(switch_core_session_get_channel(session), "sip_use_gateway");
	}


	if (sip->sip_www_authenticate) {
		authenticate = sip->sip_www_authenticate;
	} else if (sip->sip_proxy_authenticate) {
		authenticate = sip->sip_proxy_authenticate;
	} else {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "Missing Authenticate Header!\n");
		goto end;
	}
	scheme = (char const *) authenticate->au_scheme;
	if (authenticate->au_params) {
		for (indexnum = 0; (cur = (char *) authenticate->au_params[indexnum]); indexnum++) {
			if ((realm = strstr(cur, "realm="))) {
				realm += 6;
				break;
			}
		}
	}

	if (!gateway) {
		if (gw_name) {
			var_gateway = sofia_reg_find_gateway((char *) gw_name);
		}


		if (!var_gateway && realm) {
			char rb[512] = "";
			char *p = (char *) realm;
			while ((*p == '"')) {
				p++;
			}
			switch_set_string(rb, p);
			if ((p = strchr(rb, '"'))) {
				*p = '\0';
			}
			if (!(var_gateway = sofia_reg_find_gateway(rb))) {
				var_gateway = sofia_reg_find_gateway_by_realm(rb);
			}
		}

		if (!var_gateway && sip && sip->sip_to) {
			var_gateway = sofia_reg_find_gateway(sip->sip_to->a_url->url_host);
		}

		if (var_gateway) {
			gateway = var_gateway;
		}
	}




	if (!(scheme && realm)) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "No scheme and realm!\n");
		goto end;
	}

	if (sip_auth_username && sip_auth_password) {
		switch_snprintf(authentication, sizeof(authentication), "%s:%s:%s:%s", scheme, realm, sip_auth_username, sip_auth_password);
	} else if (gateway) {
		switch_snprintf(authentication, sizeof(authentication), "%s:%s:%s:%s", scheme, realm, gateway->auth_username, gateway->register_password);
	} else {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "No Matching gateway found\n");
		goto cancel;
	}

	if (profile->debug) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "Authenticating '%s' with '%s'.\n",
						  (sip_auth_username && sip_auth_password) ? sip_auth_username : gateway->auth_username, authentication);
	}

	ss_state = nua_callstate_authenticating;

	tl_gets(tags, NUTAG_CALLSTATE_REF(ss_state), SIPTAG_WWW_AUTHENTICATE_REF(authenticate), TAG_END());

	nua_authenticate(nh, SIPTAG_EXPIRES_STR(gateway ? gateway->expires_str : "3600"), NUTAG_AUTH(authentication), TAG_END());

	goto end;

  cancel:

	if (session) {
		switch_channel_hangup(switch_core_session_get_channel(session), SWITCH_CAUSE_MANDATORY_IE_MISSING);
	} else {
		nua_cancel(nh, TAG_END());
	}

  end:

	if (var_gateway) {
		sofia_reg_release_gateway(var_gateway);
	}

	return;



}

typedef struct {
	char *nonce;
	switch_size_t nplen;
	int last_nc;
} nonce_cb_t;

static int sofia_reg_nonce_callback(void *pArg, int argc, char **argv, char **columnNames)
{
	nonce_cb_t *cb = (nonce_cb_t *) pArg;
	switch_copy_string(cb->nonce, argv[0], cb->nplen);
	if (argc == 2) {
		cb->last_nc = zstr(argv[1]) ? 0 : atoi(argv[1]);
	} else {
		cb->last_nc = 0;
	}
	return 0;
}

static int sofia_reg_regcount_callback(void *pArg, int argc, char **argv, char **columnNames)
{
	int *ret = (int *) pArg;
	if (argc == 1) {
		*ret = atoi(argv[0]);
	}
	return 0;
}

auth_res_t sofia_reg_parse_auth(sofia_profile_t *profile,
								sip_authorization_t const *authorization,
								sip_t const *sip,
								const char *regstr,
								char *np,
								size_t nplen,
								char *ip,
								switch_event_t **v_event,
								long exptime, sofia_regtype_t regtype, const char *to_user, switch_event_t **auth_params, long *reg_count)
{
	int indexnum;
	const char *cur;
	su_md5_t ctx;
	char uridigest[2 * SU_MD5_DIGEST_SIZE + 1];
	char bigdigest[2 * SU_MD5_DIGEST_SIZE + 1];
	char *username, *realm, *nonce, *uri, *qop, *cnonce, *nc, *response, *input = NULL, *input2 = NULL;
	auth_res_t ret = AUTH_FORBIDDEN;
	int first = 0;
	const char *passwd = NULL;
	const char *a1_hash = NULL;
	const char *mwi_account = NULL;
	const char *call_id = NULL;
	char *sql;
	char *number_alias = NULL;
	switch_xml_t domain, xml = NULL, user, param, uparams, dparams, group = NULL, gparams = NULL;
	char hexdigest[2 * SU_MD5_DIGEST_SIZE + 1] = "";
	char *domain_name = NULL;
	switch_event_t *params = NULL;
	const char *auth_acl = NULL;
	long ncl = 0;
	sip_unknown_t *un;
	const char *user_agent = NULL;
	const char *user_agent_filter = profile->user_agent_filter;
	uint32_t max_registrations_perext = profile->max_registrations_perext;

	username = realm = nonce = uri = qop = cnonce = nc = response = NULL;

	if (authorization->au_params) {
		for (indexnum = 0; (cur = authorization->au_params[indexnum]); indexnum++) {
			char *var, *val, *p, *work;
			var = val = work = NULL;
			if ((work = strdup(cur))) {
				var = work;
				if ((val = strchr(var, '='))) {
					*val++ = '\0';
					while (*val == '"') {
						*val++ = '\0';
					}
					if ((p = strchr(val, '"'))) {
						*p = '\0';
					}

					if (!strcasecmp(var, "username")) {
						username = strdup(val);
					} else if (!strcasecmp(var, "realm")) {
						realm = strdup(val);
					} else if (!strcasecmp(var, "nonce")) {
						nonce = strdup(val);
					} else if (!strcasecmp(var, "uri")) {
						uri = strdup(val);
					} else if (!strcasecmp(var, "qop")) {
						qop = strdup(val);
					} else if (!strcasecmp(var, "cnonce")) {
						cnonce = strdup(val);
					} else if (!strcasecmp(var, "response")) {
						response = strdup(val);
					} else if (!strcasecmp(var, "nc")) {
						nc = strdup(val);
					}
				}

				free(work);
			}
		}
	}

	if (!(username && realm && nonce && uri && response)) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Invalid Authorization header!\n");
		ret = AUTH_STALE;
		goto end;
	}

	/* Optional check that auth name == SIP username */
	if ((regtype == REG_REGISTER) && sofia_test_pflag(profile, PFLAG_CHECKUSER)) {
		if (zstr(username) || zstr(to_user) || strcasecmp(to_user, username)) {
			/* Names don't match, so fail */
			if (profile->debug) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "SIP username %s does not match auth username\n", switch_str_nil(to_user));
			}
			goto end;
		}
	}

	user_agent = (sip && sip->sip_user_agent) ? sip->sip_user_agent->g_string : "unknown";

	if (zstr(np)) {
		nonce_cb_t cb = { 0 };
		long nc_long = 0;
		first = 1;

		if (nc) {
			nc_long = strtoul(nc, 0, 16);
			sql = switch_mprintf("select nonce,last_nc from sip_authentication where nonce='%q' and last_nc < %lu", nonce, nc_long);
		} else {
			sql = switch_mprintf("select nonce from sip_authentication where nonce='%q'", nonce);
		}

		cb.nonce = np;
		cb.nplen = nplen;

		switch_assert(sql != NULL);
		sofia_glue_execute_sql_callback(profile, NULL, sql, sofia_reg_nonce_callback, &cb);
		free(sql);

		//if (!sofia_glue_execute_sql2str(profile, profile->ireg_mutex, sql, np, nplen)) {
		if (zstr(np)) {
			sql = switch_mprintf("delete from sip_authentication where nonce='%q'", nonce);
			sofia_glue_execute_sql(profile, &sql, SWITCH_TRUE);
			ret = AUTH_STALE;
			goto end;
		}

		if (reg_count) {
			*reg_count = cb.last_nc + 1;
		}
	}

	switch_event_create(&params, SWITCH_EVENT_REQUEST_PARAMS);
	switch_assert(params);
	switch_event_add_header_string(params, SWITCH_STACK_BOTTOM, "action", "sip_auth");
	switch_event_add_header_string(params, SWITCH_STACK_BOTTOM, "sip_profile", profile->name);
	switch_event_add_header_string(params, SWITCH_STACK_BOTTOM, "sip_user_agent", user_agent);
	switch_event_add_header_string(params, SWITCH_STACK_BOTTOM, "sip_auth_username", username);
	switch_event_add_header_string(params, SWITCH_STACK_BOTTOM, "sip_auth_realm", realm);
	switch_event_add_header_string(params, SWITCH_STACK_BOTTOM, "sip_auth_nonce", nonce);
	switch_event_add_header_string(params, SWITCH_STACK_BOTTOM, "sip_auth_uri", uri);

	if (sip->sip_contact) {
		switch_event_add_header_string(params, SWITCH_STACK_BOTTOM, "sip_contact_user", sip->sip_contact->m_url->url_user);
		switch_event_add_header_string(params, SWITCH_STACK_BOTTOM, "sip_contact_host", sip->sip_contact->m_url->url_host);
	}

	if (sip->sip_to) {
		switch_event_add_header_string(params, SWITCH_STACK_BOTTOM, "sip_to_user", sip->sip_to->a_url->url_user);
		switch_event_add_header_string(params, SWITCH_STACK_BOTTOM, "sip_to_host", sip->sip_to->a_url->url_host);
		if (sip->sip_to->a_url->url_port) {
			switch_event_add_header_string(params, SWITCH_STACK_BOTTOM, "sip_to_port", sip->sip_to->a_url->url_port);
		}
	}

	if (sip->sip_from) {
		switch_event_add_header_string(params, SWITCH_STACK_BOTTOM, "sip_from_user", sip->sip_from->a_url->url_user);
		switch_event_add_header_string(params, SWITCH_STACK_BOTTOM, "sip_from_host", sip->sip_from->a_url->url_host);
		if (sip->sip_from->a_url->url_port) {
			switch_event_add_header_string(params, SWITCH_STACK_BOTTOM, "sip_from_port", sip->sip_from->a_url->url_port);
		}
	}

	if (sip->sip_request) {
		switch_event_add_header_string(params, SWITCH_STACK_BOTTOM, "sip_request_user", sip->sip_request->rq_url->url_user);
		switch_event_add_header_string(params, SWITCH_STACK_BOTTOM, "sip_request_host", sip->sip_request->rq_url->url_host);
		if (sip->sip_request->rq_url->url_port) {
			switch_event_add_header_string(params, SWITCH_STACK_BOTTOM, "sip_request_port", sip->sip_request->rq_url->url_port);
		}
	}

	for (un = sip->sip_unknown; un; un = un->un_next) {
		if (!strncasecmp(un->un_name, "X-", 2)) {
			if (!zstr(un->un_value)) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG10, "adding %s => %s to xml_curl request\n", un->un_name, un->un_value);
				switch_event_add_header_string(params, SWITCH_STACK_BOTTOM, un->un_name, un->un_value);
			}
		} else {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG10, "skipping %s => %s from xml_curl request\n", un->un_name, un->un_value);
		}
	}

	if (qop) {
		switch_event_add_header_string(params, SWITCH_STACK_BOTTOM, "sip_auth_qop", qop);
	}
	if (cnonce) {
		switch_event_add_header_string(params, SWITCH_STACK_BOTTOM, "sip_auth_cnonce", cnonce);
	}
	if (nc) {
		switch_event_add_header_string(params, SWITCH_STACK_BOTTOM, "sip_auth_nc", nc);
	}
	switch_event_add_header_string(params, SWITCH_STACK_BOTTOM, "sip_auth_response", response);

	switch_event_add_header_string(params, SWITCH_STACK_BOTTOM, "sip_auth_method", (sip && sip->sip_request) ? sip->sip_request->rq_method_name : NULL);

	if (auth_params) {
		switch_event_dup(auth_params, params);
	}


	if (!zstr(profile->reg_domain)) {
		domain_name = profile->reg_domain;
	} else {
		domain_name = realm;
	}

	if (switch_xml_locate_user("id", zstr(username) ? "nobody" : username, domain_name, ip, &xml, &domain, &user, &group, params) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "Can't find user [%s@%s]\n"
						  "You must define a domain called '%s' in your directory and add a user with the id=\"%s\" attribute\n"
						  "and you must configure your device to use the proper domain in it's authentication credentials.\n", username, domain_name,
						  domain_name, username);

		ret = AUTH_FORBIDDEN;
		goto end;
	} else {
		const char *type = switch_xml_attr(user, "type");
		if (type && !strcasecmp(type, "pointer")) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "Cant register a pointer.\n");
			ret = AUTH_FORBIDDEN;
			goto end;
		}
	}

	if (!(number_alias = (char *) switch_xml_attr(user, "number-alias"))) {
		number_alias = zstr(username) ? "nobody" : username;
	}

	dparams = switch_xml_child(domain, "params");
	uparams = switch_xml_child(user, "params");
	if (group) {
		gparams = switch_xml_child(group, "params");
	}

	if (!(dparams || uparams)) {
		ret = AUTH_OK;
		goto skip_auth;
	}

	if (dparams) {
		for (param = switch_xml_child(dparams, "param"); param; param = param->next) {
			const char *var = switch_xml_attr_soft(param, "name");
			const char *val = switch_xml_attr_soft(param, "value");

			if (!strcasecmp(var, "sip-forbid-register") && switch_true(val)) {
				ret = AUTH_FORBIDDEN;
				goto end;
			}

			if (!strcasecmp(var, "password")) {
				passwd = val;
			}

			if (!strcasecmp(var, "auth-acl")) {
				auth_acl = val;
			}

			if (!strcasecmp(var, "a1-hash")) {
				a1_hash = val;
			}
			if (!strcasecmp(var, "mwi-account")) {
				mwi_account = val;
			}

			if (!strcasecmp(var, "user-agent-filter")) {
				user_agent_filter = val;
			}
			if (!strcasecmp(var, "max-registrations-per-extension")) {
				max_registrations_perext = atoi(val);
			}
		}
	}

	if (gparams) {
		for (param = switch_xml_child(gparams, "param"); param; param = param->next) {
			const char *var = switch_xml_attr_soft(param, "name");
			const char *val = switch_xml_attr_soft(param, "value");

			if (!strcasecmp(var, "sip-forbid-register") && switch_true(val)) {
				ret = AUTH_FORBIDDEN;
				goto end;
			}

			if (!strcasecmp(var, "password")) {
				passwd = val;
			}

			if (!strcasecmp(var, "auth-acl")) {
				auth_acl = val;
			}

			if (!strcasecmp(var, "a1-hash")) {
				a1_hash = val;
			}
			if (!strcasecmp(var, "mwi-account")) {
				mwi_account = val;
			}

			if (!strcasecmp(var, "user-agent-filter")) {
				user_agent_filter = val;
			}
			if (!strcasecmp(var, "max-registrations-per-extension")) {
				max_registrations_perext = atoi(val);
			}
		}
	}

	if (uparams) {
		for (param = switch_xml_child(uparams, "param"); param; param = param->next) {
			const char *var = switch_xml_attr_soft(param, "name");
			const char *val = switch_xml_attr_soft(param, "value");

			if (!strcasecmp(var, "sip-forbid-register") && switch_true(val)) {
				ret = AUTH_FORBIDDEN;
				goto end;
			}

			if (!strcasecmp(var, "password")) {
				passwd = val;
			}

			if (!strcasecmp(var, "auth-acl")) {
				auth_acl = val;
			}

			if (!strcasecmp(var, "a1-hash")) {
				a1_hash = val;
			}
			if (!strcasecmp(var, "mwi-account")) {
				mwi_account = val;
			}
			if (!strcasecmp(var, "user-agent-filter")) {
				user_agent_filter = val;
			}
			if (!strcasecmp(var, "max-registrations-per-extension")) {
				max_registrations_perext = atoi(val);
			}
		}
	}

	if (auth_acl) {
		if (!switch_check_network_list_ip(ip, auth_acl)) {
			int network_ip_is_proxy = 0, x = 0;
			char *last_acl = NULL;
			if (profile->proxy_acl_count == 0) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "IP %s Rejected by user acl [%s] and no proxy acl present\n", ip, auth_acl);
				ret = AUTH_FORBIDDEN;
				goto end;
			} else {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "IP %s Rejected by user acl [%s] checking proxy ACLs now\n", ip, auth_acl);
			}
			/* Check if network_ip is a proxy allowed to send us calls */
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "%d acls to check for proxy\n", profile->proxy_acl_count);

			for (x = 0; x < profile->proxy_acl_count; x++) {
				last_acl = profile->proxy_acl[x];
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "checking %s against acl %s\n", ip, last_acl);
				if (switch_check_network_list_ip(ip, last_acl)) {
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "%s is a proxy according to the %s acl\n", ip, last_acl);
					network_ip_is_proxy = 1;
					break;
				}
			}
			/*
			 * if network_ip is a proxy allowed to send traffic, check for auth
			 * ip header and see if it matches against the auth acl
			 */
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "network ip is a proxy [%d]\n", network_ip_is_proxy);
			if (network_ip_is_proxy) {
				int x_auth_ip = 0;
				for (un = sip->sip_unknown; un; un = un->un_next) {
					if (!strcasecmp(un->un_name, "X-AUTH-IP")) {
						switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "found auth ip [%s] header of [%s]\n", un->un_name, un->un_value);
						if (!zstr(un->un_value)) {
							if (!switch_check_network_list_ip(un->un_value, auth_acl)) {
								switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "IP %s Rejected by user acl %s\n", un->un_value, auth_acl);
								ret = AUTH_FORBIDDEN;
								goto end;
							} else {
								switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG,
												  "IP %s allowed by acl %s, checking credentials\n", un->un_value, auth_acl);
								x_auth_ip = 1;
								break;
							}
						}
					}
				}
				if (!x_auth_ip) {
					ret = AUTH_FORBIDDEN;
					goto end;
				}
			}
		} else {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "IP [%s] passed ACL check [%s]\n", ip, auth_acl);
		}
	}

	if (zstr(passwd) && zstr(a1_hash)) {
		ret = AUTH_OK;
		goto skip_auth;
	}

	if (!a1_hash) {
		input = switch_mprintf("%s:%s:%s", username, realm, passwd);
		su_md5_init(&ctx);
		su_md5_strupdate(&ctx, input);
		su_md5_hexdigest(&ctx, hexdigest);
		su_md5_deinit(&ctx);
		switch_safe_free(input);
		a1_hash = hexdigest;

	}

	if (user_agent_filter) {
		if (switch_regex_match(user_agent, user_agent_filter) == SWITCH_STATUS_SUCCESS) {
			if (sofia_test_pflag(profile, PFLAG_LOG_AUTH_FAIL)) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG,
								  "SIP auth OK (REGISTER) due to user-agent-filter.  Filter \"%s\" User-Agent \"%s\"\n", user_agent_filter, user_agent);
			}
		} else {
			ret = AUTH_FORBIDDEN;
			if (sofia_test_pflag(profile, PFLAG_LOG_AUTH_FAIL)) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING,
								  "SIP auth failure (REGISTER) due to user-agent-filter.  Filter \"%s\" User-Agent \"%s\"\n", user_agent_filter,
								  user_agent);
			}
			goto end;
		}
	}

	if (max_registrations_perext > 0 && (sip && sip->sip_contact && (sip->sip_contact->m_expires == NULL || atol(sip->sip_contact->m_expires) > 0))) {
		/* if expires is null still process */
		/* expires == 0 means the phone is going to unregiser, so don't count against max */
		int count = 0;

		call_id = sip->sip_call_id->i_id;
		switch_assert(call_id);

		sql = switch_mprintf("select count(sip_user) from sip_registrations where sip_user='%q' AND call_id <> '%q'", username, call_id);
		switch_assert(sql != NULL);
		sofia_glue_execute_sql_callback(profile, NULL, sql, sofia_reg_regcount_callback, &count);
		free(sql);

		if (count + 1 > max_registrations_perext) {
			ret = AUTH_FORBIDDEN;
			if (sofia_test_pflag(profile, PFLAG_LOG_AUTH_FAIL)) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING,
								  "SIP auth failure (REGISTER) due to reaching max allowed registrations.  Count: %d\n", count);
			}
			goto end;
		}
	}

  for_the_sake_of_interop:

	if ((input = switch_mprintf("%s:%q", regstr, uri))) {
		su_md5_init(&ctx);
		su_md5_strupdate(&ctx, input);
		su_md5_hexdigest(&ctx, uridigest);
		su_md5_deinit(&ctx);
	}

	if (nc && cnonce && qop) {
		input2 = switch_mprintf("%s:%s:%s:%s:%s:%s", a1_hash, nonce, nc, cnonce, qop, uridigest);
	} else {
		input2 = switch_mprintf("%s:%s:%s", a1_hash, nonce, uridigest);
	}

	if (input2) {
		memset(&ctx, 0, sizeof(ctx));
		su_md5_init(&ctx);
		su_md5_strupdate(&ctx, input2);
		su_md5_hexdigest(&ctx, bigdigest);
		su_md5_deinit(&ctx);
	}

	if (input2 && !strcasecmp(bigdigest, response)) {
		ret = AUTH_OK;
	} else {
		if ((profile->ndlb & PFLAG_NDLB_BROKEN_AUTH_HASH) && strcasecmp(regstr, "REGISTER") && strcasecmp(regstr, "INVITE")) {
			/* some clients send an ACK with the method 'INVITE' in the hash which will break auth so we will
			   try again with INVITE so we don't get people complaining to us when someone else's client has a bug......
			 */
			switch_safe_free(input);
			switch_safe_free(input2);
			regstr = "INVITE";
			goto for_the_sake_of_interop;
		}

		ret = AUTH_FORBIDDEN;
	}

	switch_safe_free(input2);

  skip_auth:

	if (first && ret == AUTH_OK) {
		if (v_event) {
			switch_event_create_plain(v_event, SWITCH_EVENT_REQUEST_PARAMS);
		}


		if (v_event && *v_event) {
			short int xparams_type[6];
			switch_xml_t xparams[6];
			int i = 0;

			switch_event_add_header_string(*v_event, SWITCH_STACK_BOTTOM, "sip_number_alias", number_alias);
			switch_event_add_header_string(*v_event, SWITCH_STACK_BOTTOM, "sip_auth_username", username);
			switch_event_add_header_string(*v_event, SWITCH_STACK_BOTTOM, "sip_auth_realm", realm);
			switch_event_add_header_string(*v_event, SWITCH_STACK_BOTTOM, "number_alias", number_alias);
			switch_event_add_header_string(*v_event, SWITCH_STACK_BOTTOM, "user_name", username);
			switch_event_add_header_string(*v_event, SWITCH_STACK_BOTTOM, "domain_name", domain_name);

			if (mwi_account) {
				switch_event_add_header_string(*v_event, SWITCH_STACK_BOTTOM, "mwi-account", mwi_account);
			}

			if ((uparams = switch_xml_child(user, "params"))) {
				xparams_type[i] = 0;
				xparams[i++] = uparams;
			}

			if (group && (gparams = switch_xml_child(group, "params"))) {
				xparams_type[i] = 0;
				xparams[i++] = gparams;
			}

			if ((dparams = switch_xml_child(domain, "params"))) {
				xparams_type[i] = 0;
				xparams[i++] = dparams;
			}

			if ((uparams = switch_xml_child(user, "variables"))) {
				xparams_type[i] = 1;
				xparams[i++] = uparams;
			}

			if (group && (gparams = switch_xml_child(group, "variables"))) {
				xparams_type[i] = 1;
				xparams[i++] = gparams;
			}

			if ((dparams = switch_xml_child(domain, "variables"))) {
				xparams_type[i] = 1;
				xparams[i++] = dparams;
			}

			if (i <= 6) {
				int j = 0;

				for (j = 0; j < i; j++) {
					for (param = switch_xml_child(xparams[j], (xparams_type[j] ? "variable" : "param")); param; param = param->next) {
						const char *var = switch_xml_attr_soft(param, "name");
						const char *val = switch_xml_attr_soft(param, "value");
						sofia_gateway_t *gateway_ptr = NULL;

						if (!zstr(var) && !zstr(val) && (xparams_type[j] == 1 || !strncasecmp(var, "sip-", 4) || !strcasecmp(var, "register-gateway"))) {
							if (!switch_event_get_header(*v_event, var)) {
								if (profile->debug) {
									switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "event_add_header -> '%s' = '%s'\n", var, val);
								}
								switch_event_add_header_string(*v_event, SWITCH_STACK_BOTTOM, var, val);
							} else {
								continue;
							}

							if (!strcasecmp(var, "register-gateway")) {
								if (!strcasecmp(val, "all")) {
									switch_xml_t gateways_tag, gateway_tag;
									if ((gateways_tag = switch_xml_child(user, "gateways"))) {
										for (gateway_tag = switch_xml_child(gateways_tag, "gateway"); gateway_tag; gateway_tag = gateway_tag->next) {
											char *name = (char *) switch_xml_attr_soft(gateway_tag, "name");
											if (zstr(name)) {
												name = "anonymous";
											}

											if ((gateway_ptr = sofia_reg_find_gateway(name))) {
												reg_state_t ostate = gateway_ptr->state;
												gateway_ptr->retry = 0;
												if (exptime) {
													gateway_ptr->state = REG_STATE_UNREGED;
												} else {
													gateway_ptr->state = REG_STATE_UNREGISTER;
												}
												if (ostate != gateway_ptr->state) {
													sofia_reg_fire_custom_gateway_state_event(gateway_ptr, 0, NULL);
												}
												sofia_reg_release_gateway(gateway_ptr);
											}

										}
									}
								} else {
									int x, argc;
									char *mydata, *argv[50];

									mydata = strdup(val);
									switch_assert(mydata != NULL);

									argc = switch_separate_string(mydata, ',', argv, (sizeof(argv) / sizeof(argv[0])));

									for (x = 0; x < argc; x++) {
										if ((gateway_ptr = sofia_reg_find_gateway((char *) argv[x]))) {
											reg_state_t ostate = gateway_ptr->state;
											gateway_ptr->retry = 0;
											if (exptime) {
												gateway_ptr->state = REG_STATE_UNREGED;
											} else {
												gateway_ptr->state = REG_STATE_UNREGISTER;
											}
											if (ostate != gateway_ptr->state) {
												sofia_reg_fire_custom_gateway_state_event(gateway_ptr, 0, NULL);
											}
											sofia_reg_release_gateway(gateway_ptr);
										} else {
											switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "Gateway '%s' not found.\n", argv[x]);
										}
									}

									free(mydata);
								}
							}
						}
					}
				}
			}
		}
	}
  end:


	if (nc && cnonce && qop) {
		ncl = strtoul(nc, 0, 16);

#if defined(_WIN32) && !defined(_WIN64)
#define	LL_FMT "ll"
#else
#define	LL_FMT "l"
#endif
		sql = switch_mprintf("update sip_authentication set expires='%" LL_FMT "u',last_nc=%lu where nonce='%s'",
							 switch_epoch_time_now(NULL) + (profile->nonce_ttl ? profile->nonce_ttl : exptime + 10), ncl, nonce);

		switch_assert(sql != NULL);
		sofia_glue_actually_execute_sql(profile, sql, profile->ireg_mutex);
		switch_safe_free(sql);
	}

	switch_event_destroy(&params);

	if (xml) {
		switch_xml_free(xml);
	}

	switch_safe_free(input);
	switch_safe_free(username);
	switch_safe_free(realm);
	switch_safe_free(nonce);
	switch_safe_free(uri);
	switch_safe_free(qop);
	switch_safe_free(cnonce);
	switch_safe_free(nc);
	switch_safe_free(response);

	if (reg_count && !*reg_count) {
		if (ret == AUTH_OK) {
			if (ncl) {
				*reg_count = ncl;
			} else {
				*reg_count = 1;
			}
		} else {
			*reg_count = 0;
		}
	}

	return ret;

}


sofia_gateway_t *sofia_reg_find_gateway__(const char *file, const char *func, int line, const char *key)
{
	sofia_gateway_t *gateway = NULL;

	switch_mutex_lock(mod_sofia_globals.hash_mutex);
	if ((gateway = (sofia_gateway_t *) switch_core_hash_find(mod_sofia_globals.gateway_hash, key))) {
		if (!sofia_test_pflag(gateway->profile, PFLAG_RUNNING) || gateway->deleted) {
			gateway = NULL;
			goto done;
		}
		if (sofia_reg_gateway_rdlock__(file, func, line, gateway) != SWITCH_STATUS_SUCCESS) {
			gateway = NULL;
		}
	}

  done:
	switch_mutex_unlock(mod_sofia_globals.hash_mutex);
	return gateway;
}


sofia_gateway_t *sofia_reg_find_gateway_by_realm__(const char *file, const char *func, int line, const char *key)
{
	sofia_gateway_t *gateway = NULL;
	switch_hash_index_t *hi;
	const void *var;
	void *val;

	switch_mutex_lock(mod_sofia_globals.hash_mutex);
	for (hi = switch_hash_first(NULL, mod_sofia_globals.gateway_hash); hi; hi = switch_hash_next(hi)) {
		switch_hash_this(hi, &var, NULL, &val);
		if ((gateway = (sofia_gateway_t *) val)) {
			if (!strcasecmp(gateway->register_realm, key)) {
				break;
			}
		} else {
			gateway = NULL;
		}
	}

	if (gateway) {
		if (!sofia_test_pflag(gateway->profile, PFLAG_RUNNING) || gateway->deleted) {
			gateway = NULL;
			goto done;
		}
		if (switch_thread_rwlock_tryrdlock(gateway->profile->rwlock) != SWITCH_STATUS_SUCCESS) {
			switch_log_printf(SWITCH_CHANNEL_ID_LOG, file, func, line, NULL, SWITCH_LOG_ERROR, "Profile %s is locked\n", gateway->profile->name);
			gateway = NULL;
		}
	}
	if (gateway) {
#ifdef SOFIA_DEBUG_RWLOCKS
		switch_log_printf(SWITCH_CHANNEL_ID_LOG, file, func, line, SWITCH_LOG_ERROR, "XXXXXXXXXXXXXX GW LOCK %s\n", gateway->profile->name);
#endif
	}

  done:
	switch_mutex_unlock(mod_sofia_globals.hash_mutex);
	return gateway;
}

switch_status_t sofia_reg_gateway_rdlock__(const char *file, const char *func, int line, sofia_gateway_t *gateway)
{
	switch_status_t status = sofia_glue_profile_rdlock__(file, func, line, gateway->profile);

#ifdef SOFIA_DEBUG_RWLOCKS
	if (status == SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_ID_LOG, file, func, line, SWITCH_LOG_ERROR, "XXXXXXXXXXXXXX GW LOCK %s\n", gateway->profile->name);		
	}
#endif

	return status;
}


void sofia_reg_release_gateway__(const char *file, const char *func, int line, sofia_gateway_t *gateway)
{
	switch_thread_rwlock_unlock(gateway->profile->rwlock);
#ifdef SOFIA_DEBUG_RWLOCKS
	switch_log_printf(SWITCH_CHANNEL_ID_LOG, file, func, line, SWITCH_LOG_ERROR, "XXXXXXXXXXXXXX GW UNLOCK %s\n", gateway->profile->name);
#endif
}

switch_status_t sofia_reg_add_gateway(sofia_profile_t *profile, const char *key, sofia_gateway_t *gateway)
{
	switch_status_t status = SWITCH_STATUS_FALSE;
	char *pkey = switch_mprintf("%s::%s", profile->name, key);

	switch_mutex_lock(mod_sofia_globals.hash_mutex);

	gateway->next = profile->gateways;
	profile->gateways = gateway;
	
	if (!switch_core_hash_find(mod_sofia_globals.gateway_hash, key)) {
		status = switch_core_hash_insert(mod_sofia_globals.gateway_hash, key, gateway);
	}

	if (!switch_core_hash_find(mod_sofia_globals.gateway_hash, pkey)) {
		status = switch_core_hash_insert(mod_sofia_globals.gateway_hash, pkey, gateway);
	}
	switch_mutex_unlock(mod_sofia_globals.hash_mutex);

	free(pkey);

	if (status == SWITCH_STATUS_SUCCESS) {
		switch_event_t *s_event;
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "Added gateway '%s' to profile '%s'\n", gateway->name, gateway->profile->name);
		if (switch_event_create_subclass(&s_event, SWITCH_EVENT_CUSTOM, MY_EVENT_GATEWAY_ADD) == SWITCH_STATUS_SUCCESS) {
			switch_event_add_header_string(s_event, SWITCH_STACK_BOTTOM, "Gateway", gateway->name);
			switch_event_add_header_string(s_event, SWITCH_STACK_BOTTOM, "profile-name", gateway->profile->name);
			switch_event_fire(&s_event);
		}
	}

	return status;
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
