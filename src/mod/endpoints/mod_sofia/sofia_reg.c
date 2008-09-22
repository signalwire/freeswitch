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
 * Ken Rice, <krice at cometsig.com>  (work sponsored by Comet Signaling LLC, CopperCom, Inc and Asteria Solutions Group, Inc)
 * Paul D. Tinsley <pdt at jackhammer.org>
 * Bret McDanel <trixter AT 0xdecafbad.com>
 * Marcel Barbulescu <marcelbarbulescu@gmail.com>
 * David Knell <>
 *
 *
 * sofia_ref.c -- SOFIA SIP Endpoint (registration code)
 *
 */
#include "mod_sofia.h"

static void sofia_reg_kill_reg(sofia_gateway_t *gateway_ptr, int unreg)
{
	if (gateway_ptr->nh) {
		if (unreg) {
			nua_unregister(gateway_ptr->nh,
						   NUTAG_URL(gateway_ptr->register_url),
						   SIPTAG_FROM_STR(gateway_ptr->register_from),
						   SIPTAG_TO_STR(gateway_ptr->register_from),
						   SIPTAG_CONTACT_STR(gateway_ptr->register_contact),
						   SIPTAG_EXPIRES_STR(gateway_ptr->expires_str),
						   NUTAG_REGISTRAR(gateway_ptr->register_proxy),
						   NUTAG_OUTBOUND("no-options-keepalive"), NUTAG_OUTBOUND("no-validate"), NUTAG_KEEPALIVE(0), TAG_NULL());
		}
		nua_handle_bind(gateway_ptr->nh, NULL);
		nua_handle_destroy(gateway_ptr->nh);
		gateway_ptr->nh = NULL;
	}

}

void sofia_reg_unregister(sofia_profile_t *profile)
{
	sofia_gateway_t *gateway_ptr;
	for (gateway_ptr = profile->gateways; gateway_ptr; gateway_ptr = gateway_ptr->next) {
		if (gateway_ptr->sofia_private) {
			free(gateway_ptr->sofia_private);
			gateway_ptr->sofia_private = NULL;
		}
		sofia_reg_kill_reg(gateway_ptr, 1);
	}
}

void sofia_reg_check_gateway(sofia_profile_t *profile, time_t now)
{
	sofia_gateway_t *gateway_ptr, *last = NULL;

	for (gateway_ptr = profile->gateways; gateway_ptr; gateway_ptr = gateway_ptr->next) {
		if (gateway_ptr->deleted && gateway_ptr->state == REG_STATE_NOREG) {
			if (last) {
				last->next = gateway_ptr->next;
			} else {
				profile->gateways = gateway_ptr->next;
			}

			switch_core_hash_delete(mod_sofia_globals.gateway_hash, gateway_ptr->name);
			switch_core_hash_delete(mod_sofia_globals.gateway_hash, gateway_ptr->register_from);
			switch_core_hash_delete(mod_sofia_globals.gateway_hash, gateway_ptr->register_contact);
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "deleted gateway %s\n", gateway_ptr->name);
		} else {
			last = gateway_ptr;
		}
	}

	for (gateway_ptr = profile->gateways; gateway_ptr; gateway_ptr = gateway_ptr->next) {
		int ss_state = nua_callstate_authenticating;
		reg_state_t ostate = gateway_ptr->state;

		if (!now) {
			gateway_ptr->state = ostate = REG_STATE_UNREGED;
			gateway_ptr->expires_str = "0";
		}

		if (gateway_ptr->ping && !gateway_ptr->pinging && (now >= gateway_ptr->ping && (ostate == REG_STATE_NOREG || ostate == REG_STATE_REGED))) {
			nua_handle_t *nh = nua_handle(profile->nua, NULL, NUTAG_URL(gateway_ptr->register_url), SIPTAG_CONTACT_STR(profile->url), TAG_END());
			sofia_private_t *pvt;

			pvt = malloc(sizeof(*pvt));
			switch_assert(pvt);
			memset(pvt, 0, sizeof(*pvt));
			pvt->destroy_nh = 1;
			switch_copy_string(pvt->gateway_name, gateway_ptr->name, sizeof(pvt->gateway_name));
			nua_handle_bind(nh, pvt);

			gateway_ptr->pinging = 1;
			nua_options(nh, TAG_END());
		}

		switch (ostate) {
		case REG_STATE_NOREG:
			gateway_ptr->status = SOFIA_GATEWAY_UP;
			break;
		case REG_STATE_REGISTER:
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "registered %s\n", gateway_ptr->name);
			gateway_ptr->expires = now + gateway_ptr->freq;
			gateway_ptr->state = REG_STATE_REGED;
			gateway_ptr->status = SOFIA_GATEWAY_UP;
			break;

		case REG_STATE_UNREGISTER:
			sofia_reg_kill_reg(gateway_ptr, 1);
			gateway_ptr->state = REG_STATE_NOREG;
			break;
		case REG_STATE_UNREGED:
			gateway_ptr->status = SOFIA_GATEWAY_DOWN;
			sofia_reg_kill_reg(gateway_ptr, 0);

			if ((gateway_ptr->nh = nua_handle(gateway_ptr->profile->nua, NULL,
											  NUTAG_URL(gateway_ptr->register_proxy),
											  SIPTAG_TO_STR(gateway_ptr->register_to),
											  NUTAG_CALLSTATE_REF(ss_state), SIPTAG_FROM_STR(gateway_ptr->register_from), TAG_END()))) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "registering %s\n", gateway_ptr->name);

				if (!gateway_ptr->sofia_private) {
					gateway_ptr->sofia_private = malloc(sizeof(*gateway_ptr->sofia_private));
					switch_assert(gateway_ptr->sofia_private);
				}
				memset(gateway_ptr->sofia_private, 0, sizeof(*gateway_ptr->sofia_private));

				gateway_ptr->sofia_private->gateway = gateway_ptr;
				nua_handle_bind(gateway_ptr->nh, gateway_ptr->sofia_private);

				if (now) {
					nua_register(gateway_ptr->nh,
								 NUTAG_URL(gateway_ptr->register_url),
								 TAG_IF(gateway_ptr->register_sticky_proxy, NUTAG_PROXY(gateway_ptr->register_sticky_proxy)),
								 SIPTAG_TO_STR(gateway_ptr->register_from),
								 SIPTAG_FROM_STR(gateway_ptr->register_from),
								 SIPTAG_CONTACT_STR(gateway_ptr->register_contact),
								 SIPTAG_EXPIRES_STR(gateway_ptr->expires_str),
								 NUTAG_REGISTRAR(gateway_ptr->register_proxy),
								 NUTAG_OUTBOUND("no-options-keepalive"), NUTAG_OUTBOUND("no-validate"), NUTAG_KEEPALIVE(0), TAG_NULL());
					gateway_ptr->retry = now + gateway_ptr->retry_seconds;
				} else {
					nua_unregister(gateway_ptr->nh,
								   NUTAG_URL(gateway_ptr->register_url),
								   SIPTAG_FROM_STR(gateway_ptr->register_from),
								   SIPTAG_TO_STR(gateway_ptr->register_from),
								   SIPTAG_CONTACT_STR(gateway_ptr->register_contact),
								   SIPTAG_EXPIRES_STR(gateway_ptr->expires_str),
								   NUTAG_REGISTRAR(gateway_ptr->register_proxy),
								   NUTAG_OUTBOUND("no-options-keepalive"), NUTAG_OUTBOUND("no-validate"), NUTAG_KEEPALIVE(0), TAG_NULL());
				}

				gateway_ptr->state = REG_STATE_TRYING;

			} else {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Error registering %s\n", gateway_ptr->name);
				gateway_ptr->state = REG_STATE_FAILED;
			}
			break;

		case REG_STATE_FAILED:
			sofia_reg_kill_reg(gateway_ptr, 0);
			gateway_ptr->status = SOFIA_GATEWAY_DOWN;
		case REG_STATE_TRYING:
			if (gateway_ptr->retry && now >= gateway_ptr->retry) {
				gateway_ptr->state = REG_STATE_UNREGED;
				gateway_ptr->retry = 0;
			}
			break;
		default:
			if (now >= gateway_ptr->expires) {
				gateway_ptr->state = REG_STATE_UNREGED;
			}
			break;
		}
	}
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
	char *contact = NULL;
	char to[128] = "";

	switch_snprintf(to, sizeof(to), "sip:%s@%s", argv[1], argv[2]);
	contact = sofia_glue_get_url_from_contact(argv[3], 1);

	nh = nua_handle(profile->nua, NULL, SIPTAG_FROM_STR(profile->url), SIPTAG_TO_STR(to), NUTAG_URL(contact), SIPTAG_CONTACT_STR(profile->url), TAG_END());
	nua_handle_bind(nh, &mod_sofia_globals.destroy_private);
	nua_options(nh, TAG_END());

	switch_safe_free(contact);

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

void sofia_reg_send_reboot(sofia_profile_t *profile, const char *user, const char *host, const char *contact, const char *user_agent)
{
	const char *event = "check-sync";
	nua_handle_t *nh;
	char *contact_url = NULL;
	char *id = NULL;

	if (switch_stristr("snom", user_agent)) {
		event = "check-sync;reboot=true";
	} else if (switch_stristr("linksys", user_agent)) {
		event = "reboot_now";
	}

	if ((contact_url = sofia_glue_get_url_from_contact((char *)contact, 1))) {
		char *p;
		id = switch_mprintf("sip:%s@%s", user, host);

		if ((p = strstr(contact_url, ";fs_"))) {
			*p = '\0';
		}
		
		nh = nua_handle(profile->nua, NULL, 
						NUTAG_URL(contact_url), 
						SIPTAG_FROM_STR(id), 
						SIPTAG_TO_STR(id), 
						SIPTAG_CONTACT_STR(profile->url), 
						TAG_END());

		nua_notify(nh,
				   NUTAG_NEWSUB(1),
				   SIPTAG_EVENT_STR(event), 
				   SIPTAG_CONTENT_TYPE_STR("application/simple-message-summary"),
				   SIPTAG_PAYLOAD_STR(""),
				   TAG_END());

		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "Sending reboot command to %s\n", contact_url);
		free(contact_url);
	}

	switch_safe_free(id);
}


int sofia_reg_del_callback(void *pArg, int argc, char **argv, char **columnNames)
{
	switch_event_t *s_event;
	sofia_profile_t *profile = (sofia_profile_t *) pArg;
	
	if (argc > 11 && atoi(argv[11]) == 1) {
		sofia_reg_send_reboot(profile, argv[1], argv[2], argv[3], argv[7]);
	}

	if (argc >= 3) {
		if (switch_event_create_subclass(&s_event, SWITCH_EVENT_CUSTOM, MY_EVENT_EXPIRE) == SWITCH_STATUS_SUCCESS) {
			switch_event_add_header_string(s_event, SWITCH_STACK_BOTTOM, "profile-name", argv[8]);
			switch_event_add_header_string(s_event, SWITCH_STACK_BOTTOM, "call-id", argv[0]);
			switch_event_add_header_string(s_event, SWITCH_STACK_BOTTOM, "user", argv[1]);
			switch_event_add_header_string(s_event, SWITCH_STACK_BOTTOM, "host", argv[2]);
			switch_event_add_header_string(s_event, SWITCH_STACK_BOTTOM, "contact", argv[3]);
			switch_event_add_header_string(s_event, SWITCH_STACK_BOTTOM, "expires", argv[6]);
			switch_event_add_header_string(s_event, SWITCH_STACK_BOTTOM, "user-agent", argv[7]);
			switch_event_fire(&s_event);
		}
	}
	return 0;
}

void sofia_reg_expire_call_id(sofia_profile_t *profile, const char *call_id, int reboot)
{
	char sql[1024];
	char *psql = sql;
	char *user = strdup(call_id);
	char *host = NULL;

	switch_assert(user);

	if ((host = strchr(user, '@'))) {
		*host++ = '\0';
	}
	
	if (!host) {
		host = "none";
	}

	switch_snprintf(sql, sizeof(sql), "select call_id,sip_user,sip_host,contact,status,rpid,expires,user_agent,server_user,server_host,profile_name"
					",%d from sip_registrations where call_id='%s' or (sip_user='%s' and sip_host='%s') and hostname='%q'", 
					reboot, call_id, user, host, mod_sofia_globals.hostname);
	
	switch_mutex_lock(profile->ireg_mutex);
	sofia_glue_execute_sql_callback(profile, SWITCH_TRUE, NULL, sql, sofia_reg_del_callback, profile);
	switch_mutex_unlock(profile->ireg_mutex);

	switch_snprintf(sql, sizeof(sql), "delete from sip_registrations where call_id='%s' or (sip_user='%s' and sip_host='%s') and hostname='%q'", 
					call_id, user, host, mod_sofia_globals.hostname);
	sofia_glue_execute_sql(profile, &psql, SWITCH_FALSE);

	switch_safe_free(user);

}

void sofia_reg_check_expire(sofia_profile_t *profile, time_t now, int reboot)
{
	char sql[1024];

#ifdef SWITCH_HAVE_ODBC
	if (profile->odbc_dsn) {
		if (!profile->master_odbc) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Error Opening DB %s\n", profile->dbname);
			return;
		}
	} else {
#endif
		if (!profile->master_db) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Error Opening DB %s\n", profile->dbname);
			return;
		}
#ifdef SWITCH_HAVE_ODBC
	}
#endif


	switch_mutex_lock(profile->ireg_mutex);

	if (now) {
		switch_snprintf(sql, sizeof(sql), "select call_id,sip_user,sip_host,contact,status,rpid,expires,user_agent,server_user,server_host,profile_name"
						",%d from sip_registrations where expires > 0 and expires <= %ld", reboot, (long) now);
	} else {
		switch_snprintf(sql, sizeof(sql), "select call_id,sip_user,sip_host,contact,status,rpid,expires,user_agent,server_user,server_host,profile_name"
						",%d from sip_registrations where expires > 0", reboot);
	}

	sofia_glue_execute_sql_callback(profile, SWITCH_TRUE, NULL, sql, sofia_reg_del_callback, profile);
	if (now) {
		switch_snprintf(sql, sizeof(sql), "delete from sip_registrations where expires > 0 and expires <= %ld and hostname='%q'", 
						(long) now, mod_sofia_globals.hostname);
	} else {
		switch_snprintf(sql, sizeof(sql), "delete from sip_registrations where expires > 0 and hostname='%q'", mod_sofia_globals.hostname);
	}

	sofia_glue_actually_execute_sql(profile, SWITCH_FALSE, sql, NULL);

	if (now) {
		switch_snprintf(sql, sizeof(sql), "delete from sip_authentication where expires > 0 and expires <= %ld and hostname='%q'", 
						(long) now, mod_sofia_globals.hostname);
	} else {
		switch_snprintf(sql, sizeof(sql), "delete from sip_authentication where expires > 0 and hostname='%q'", mod_sofia_globals.hostname);
	}

	sofia_glue_actually_execute_sql(profile, SWITCH_FALSE, sql, NULL);



	if (now) {
		switch_snprintf(sql, sizeof(sql), "select call_id from sip_subscriptions where expires > 0 and expires <= %ld and hostname='%q'", 
						(long) now, mod_sofia_globals.hostname);
	} else {
		switch_snprintf(sql, sizeof(sql), "select call_id from sip_subscriptions where expires > 0 and hostname='%q'", mod_sofia_globals.hostname);
	}

	sofia_glue_execute_sql_callback(profile, SWITCH_TRUE, NULL, sql, sofia_sub_del_callback, profile);

	if (now) {
		switch_snprintf(sql, sizeof(sql), "delete from sip_subscriptions where expires > 0 and expires <= %ld and hostname='%q'", 
						(long) now, mod_sofia_globals.hostname);
	} else {
		switch_snprintf(sql, sizeof(sql), "delete from sip_subscriptions where expires > 0 and hostname='%q'", mod_sofia_globals.hostname);
	}

	sofia_glue_actually_execute_sql(profile, SWITCH_FALSE, sql, NULL);


	if (now) {
		switch_snprintf(sql, sizeof(sql), "select call_id,sip_user,sip_host,contact,status,rpid,expires,user_agent,server_user,server_host,profile_name"
						" from sip_registrations where status like '%%AUTO-NAT%%' or status like '%%UDP-NAT%%'");
		sofia_glue_execute_sql_callback(profile, SWITCH_TRUE, NULL, sql, sofia_reg_nat_callback, profile);
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
		switch_snprintf(sql, sizeof(sql), "select contact from sip_registrations where sip_user='%s' and (sip_host='%s' or presence_hosts like '%%%q%%')"
						, user, host, host);
	} else {
		switch_snprintf(sql, sizeof(sql), "select contact from sip_registrations where sip_user='%s'", user);
	}


	sofia_glue_execute_sql_callback(profile, SWITCH_FALSE, profile->ireg_mutex, sql, sofia_reg_find_callback, &cbt);


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

	sql = switch_mprintf("insert into sip_authentication (nonce,expires,profile_name,hostname) "
						 "values('%q', %ld, '%q', '%q')", uuid_str, switch_timestamp(NULL) + profile->nonce_ttl, profile->name, mod_sofia_globals.hostname);
	switch_assert(sql != NULL);
	sofia_glue_actually_execute_sql(profile, SWITCH_FALSE, sql, profile->ireg_mutex);
	switch_safe_free(sql);
	//sofia_glue_execute_sql(profile, &sql, SWITCH_TRUE);

	auth_str = switch_mprintf("Digest realm=\"%q\", nonce=\"%q\",%s algorithm=MD5, qop=\"auth\"", realm, uuid_str, stale ? " stale=\"true\"," : "");

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
	const char *from_user = NULL;
	const char *from_host = NULL;
	const char *reg_host = profile->reg_db_domain;
	char contact_str[1024] = "";
	int nat_hack = 0;
	uint8_t multi_reg = 0, avoid_multi_reg = 0;
	//char buf[512];
	uint8_t stale = 0, forbidden = 0;
	auth_res_t auth_res;
	long exptime = 60;
	switch_event_t *event;
	const char *rpid = "unknown";
	const char *display = "\"user\"";
	char network_ip[80];
	char url_ip[80];
	char *register_gateway = NULL;
	int network_port;
	const char *reg_desc = "Registered";
	const char *call_id = NULL;
	char *force_user;
	char received_data[128] = "";
	char *path_val = NULL;
	su_addrinfo_t *my_addrinfo = msg_addrinfo(nua_current_request(nua));

	/* all callers must confirm that sip, sip->sip_request and sip->sip_contact are not NULL */
	switch_assert(sip != NULL && sip->sip_contact != NULL && sip->sip_request != NULL);

	get_addr(network_ip, sizeof(network_ip), my_addrinfo->ai_addr,my_addrinfo->ai_addrlen);
	network_port = get_port(my_addrinfo->ai_addr);

	snprintf(url_ip, sizeof(url_ip), my_addrinfo->ai_addr->sa_family == AF_INET6 ? "[%s]" : "%s", network_ip);

	expires = sip->sip_expires;
	authorization = sip->sip_authorization;
	contact = sip->sip_contact;
	to = sip->sip_to;

	if (to) {
		to_user = to->a_url->url_user;
		to_host = to->a_url->url_host;
	}

	if (!to_user || !to_host) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Can not do authorization without a complete from header\n");
		nua_respond(nh, SIP_401_UNAUTHORIZED, NUTAG_WITH_THIS(nua), TAG_END());
		return 1;
	}

	if (!reg_host) {
		reg_host = to_host;
	}

	from = sip->sip_from;

	if (from) {
		from_user = from->a_url->url_user;
		from_host = from->a_url->url_host;
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
			contact_host = url_ip;
			switch_snprintf(new_port, sizeof(new_port), ":%d", network_port);
			port = NULL;
		} else {
			if (is_tls) {
				reg_desc = "Registered(TLS)";
			} else if (is_tcp) {
				reg_desc = "Registered(TCP)";
			} else {
				reg_desc = "Registered(UDP)";
			}
		}

		if (switch_strlen_zero(display)) {
			if (to) {
				display = to->a_display;
				if (switch_strlen_zero(display)) {
					display = "\"user\"";
				}
			}
		}

		if (sip->sip_path) {
			path_val = sip_header_as_string(nh->nh_home, (void *) sip->sip_path);
			path_encoded_len = (strlen(path_val) * 3) + 1;
			switch_zmalloc(path_encoded, path_encoded_len);
			switch_copy_string(path_encoded, ";fs_path=", 10);
			switch_url_encode(path_val, path_encoded + 9, path_encoded_len - 9);
		}

		if (port) {
			switch_snprintf(new_port, sizeof(new_port), ":%s", port);
		}

		if (is_nat && (profile->pflags & PFLAG_RECIEVED_IN_NAT_REG_CONTACT)) {
			switch_snprintf(received_data, sizeof(received_data), ";received=\"%s:%d\"", url_ip, network_port);
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

	if (regtype == REG_AUTO_REGISTER || (regtype == REG_REGISTER && (profile->pflags & PFLAG_BLIND_REG))) {
		regtype = REG_REGISTER;
		goto reg;
	}

	if (authorization) {
		char *v_contact_str;
		if ((auth_res = sofia_reg_parse_auth(profile, authorization, sip, sip->sip_request->rq_method_name,
											 key, keylen, network_ip, v_event, exptime, regtype, to_user)) == AUTH_STALE) {
			stale = 1;
		}

		if (exptime && v_event && *v_event) {
			char *exp_var;
			char *allow_multireg = NULL;

			allow_multireg = switch_event_get_header(*v_event, "sip-allow-multiple-registrations");
			if ( allow_multireg && switch_false(allow_multireg) ) {
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

				if (*received_data && (profile->pflags & PFLAG_RECIEVED_IN_NAT_REG_CONTACT)) {
					switch_snprintf(received_data, sizeof(received_data), ";received=\"%s:%d\"", url_ip, network_port);
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
					if (strstr(v_contact_str, "tls")) {
						reg_desc = "Registered(TLSHACK)";
					} else {
						reg_desc = "Registered(AUTO-NAT)";
						exptime = 20;
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

			if ((exp_var = switch_event_get_header(*v_event, "sip-force-expires"))) {
				int tmp = atoi(exp_var);
				if (tmp > 0) {
					exptime = tmp;
				}
			}
		}

		if (auth_res != AUTH_OK && !stale) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "send %s for [%s@%s]\n", forbidden ? "forbidden" : "challenge", to_user, to_host);
			if (auth_res == AUTH_FORBIDDEN) {
				nua_respond(nh, SIP_403_FORBIDDEN, NUTAG_WITH_THIS(nua), TAG_END());
			} else {
				nua_respond(nh, SIP_401_UNAUTHORIZED, NUTAG_WITH_THIS(nua), TAG_END());
			}
			return 1;
		}
	}

	if (!authorization || stale) {
		if (regtype == REG_REGISTER) {
			sofia_reg_auth_challenge(nua, profile, nh, regtype, to_host, stale);
			if (profile->debug) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Requesting Registration from: [%s@%s]\n", to_user, to_host);
			}
		} else {
			sofia_reg_auth_challenge(nua, profile, nh, regtype, from_host, stale);
		}
		return 1;
	}
  reg:

	if (regtype != REG_REGISTER) {
		return 0;
	}

	call_id = sip->sip_call_id->i_id;	//sip_header_as_string(profile->home, (void *) sip->sip_call_id);
	switch_assert(call_id);

	/* Does this profile supports multiple registrations ? */
	multi_reg = ( sofia_test_pflag(profile, PFLAG_MULTIREG) ) ? 1 : 0;

	if ( multi_reg && avoid_multi_reg ) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, 
					"Disabling multiple registrations on a per-user basis for %s@%s\n", 
					switch_str_nil(to_user), switch_str_nil(to_host) );
		multi_reg = 0;
	}

	if (exptime) {
		const char *agent = "dunno";
		char guess_ip4[256];

		if (sip->sip_user_agent) {
			agent = sip->sip_user_agent->g_string;
		}

		if (multi_reg) {
			sql = switch_mprintf("delete from sip_registrations where call_id='%q'", call_id);
		} else {
			sql = switch_mprintf("delete from sip_registrations where sip_user='%q' and sip_host='%q' and hostname='%q'", 
								 to_user, reg_host, mod_sofia_globals.hostname);
		}
		switch_mutex_lock(profile->ireg_mutex);
		sofia_glue_execute_sql(profile, &sql, SWITCH_TRUE);
		
		switch_find_local_ip(guess_ip4, sizeof(guess_ip4), AF_INET);
		sql = switch_mprintf("insert into sip_registrations "
							 "(call_id,sip_user,sip_host,presence_hosts,contact,status,rpid,expires,user_agent,server_user,server_host,profile_name,hostname) "
							 "values ('%q','%q', '%q','%q','%q','%q', '%q', %ld, '%q', '%q', '%q', '%q', '%q')", 
							 call_id, to_user, reg_host, profile->presence_hosts ? profile->presence_hosts : reg_host, 
							 contact_str, reg_desc, rpid, (long) switch_timestamp(NULL) + (long) exptime * 2, 
							 agent, from_user, guess_ip4, profile->name, mod_sofia_globals.hostname);
							 
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
			switch_event_add_header(s_event, SWITCH_STACK_BOTTOM, "expires", "%ld", (long) exptime);
			switch_event_add_header_string(s_event, SWITCH_STACK_BOTTOM, "to-user", from_user);
			switch_event_add_header_string(s_event, SWITCH_STACK_BOTTOM, "to-host", from_host);
			switch_event_fire(&s_event);
		}



		if (profile->debug) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG,
							  "Register:\nFrom:    [%s@%s]\nContact: [%s]\nExpires: [%ld]\n", to_user, reg_host, contact_str, (long) exptime);
		}

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
	} else {
		if (multi_reg) {
			char *icontact, *p;
			icontact = sofia_glue_get_url_from_contact(contact_str, 1);
			if ((p = strchr(icontact, ';'))) {
				*p = '\0';
			}
			if ((p = strchr(icontact + 4, ':'))) {
				*p = '\0';
			}
			if ((sql = switch_mprintf("delete from sip_subscriptions where call_id='%q'", call_id))) {
				sofia_glue_execute_sql(profile, &sql, SWITCH_TRUE);
			}

			if ((sql = switch_mprintf("delete from sip_registrations where call_id='%q'", call_id))) {
				sofia_glue_execute_sql(profile, &sql, SWITCH_TRUE);
			}
			switch_safe_free(icontact);
		} else {
			if ((sql = switch_mprintf("delete from sip_subscriptions where sip_user='%q' and sip_host='%q' and hostname='%q'", to_user, reg_host, 
									  mod_sofia_globals.hostname))) {
				sofia_glue_execute_sql(profile, &sql, SWITCH_TRUE);
			}

			if ((sql = switch_mprintf("delete from sip_registrations where sip_user='%q' and sip_host='%q' and hostname='%q'", to_user, reg_host,
									  mod_sofia_globals.hostname))) {
				sofia_glue_execute_sql(profile, &sql, SWITCH_TRUE);
			}
		}
		if (switch_event_create(&event, SWITCH_EVENT_PRESENCE_OUT) == SWITCH_STATUS_SUCCESS) {
			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "proto", "sip");
			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "login", profile->url);
			switch_event_add_header(event, SWITCH_STACK_BOTTOM, "from", "%s+%s@%s", SOFIA_CHAT_PROTO, to_user, reg_host);

			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "status", "unavailable");
			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "rpid", rpid);
			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "event_type", "presence");
			switch_event_fire(&event);
		}
	}


	if (regtype == REG_REGISTER) {
		char exp_param[128] = "";
		s_event = NULL;

		if (exptime) {
			switch_snprintf(exp_param, sizeof(exp_param), "expires=%ld", exptime);
			sip_contact_add_param(nh->nh_home, sip->sip_contact, exp_param);

			if (switch_event_create(&s_event, SWITCH_EVENT_MESSAGE_QUERY) == SWITCH_STATUS_SUCCESS) {
				switch_event_add_header(s_event, SWITCH_STACK_BOTTOM, "Message-Account", "sip:%s@%s", to_user, reg_host);
				switch_event_add_header_string(s_event, SWITCH_STACK_BOTTOM, "VM-Sofia-Profile", profile->name);
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
		
		nua_respond(nh, SIP_200_OK, SIPTAG_CONTACT(sip->sip_contact), TAG_IF(path_val, SIPTAG_PATH_STR(path_val)), NUTAG_WITH_THIS(nua), TAG_END());

		if (s_event) {
			switch_event_fire(&s_event);
		}

		return 1;
	}

	return 0;
}



void sofia_reg_handle_sip_i_register(nua_t *nua, sofia_profile_t *profile, nua_handle_t *nh, sofia_private_t *sofia_private, sip_t const *sip,
									 tagi_t tags[])
{
	char key[128] = "";
	switch_event_t *v_event = NULL;
	char network_ip[80];
	su_addrinfo_t *my_addrinfo = msg_addrinfo(nua_current_request(nua));
	sofia_regtype_t type = REG_REGISTER;
	int network_port = 0;
	char *is_nat = NULL;


	get_addr(network_ip, sizeof(network_ip), my_addrinfo->ai_addr, my_addrinfo->ai_addrlen);
	network_port = get_port(msg_addrinfo(nua_current_request(nua))->ai_addr);


	if (!(sip->sip_contact && sip->sip_contact->m_url)) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "NO CONTACT!\n");
		nua_respond(nh, 400, "Missing Contact Header", TAG_END());
		goto end;
	}

	if (!(profile->mflags & MFLAG_REGISTER)) {
		nua_respond(nh, SIP_403_FORBIDDEN, NUTAG_WITH_THIS(nua), TAG_END());
		goto end;
	}

	if ((profile->pflags & PFLAG_AGGRESSIVE_NAT_DETECTION)) {
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

		if (!switch_strlen_zero(contact_host)) {
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

		if (ok && !(profile->pflags & PFLAG_BLIND_REG)) {
			type = REG_AUTO_REGISTER;
		} else if (!ok) {
            switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "IP %s Rejected by acl %s\n", network_ip, profile->reg_acl[x]);
            nua_respond(nh, SIP_403_FORBIDDEN, NUTAG_WITH_THIS(nua), TAG_END());
			goto end;
		}
	}

	if (!sip || !sip->sip_request || !sip->sip_request->rq_method_name) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Received an invalid packet!\n");
		nua_respond(nh, SIP_500_INTERNAL_SERVER_ERROR, TAG_END());
		goto end;
	}

	sofia_reg_handle_register(nua, profile, nh, sip, type, key, sizeof(key), &v_event, is_nat);
	
	if (v_event) {
		switch_event_fire(&v_event);
	}

  end:

	nua_handle_destroy(nh);

}


void sofia_reg_handle_sip_r_register(int status,
									 char const *phrase,
									 nua_t *nua, sofia_profile_t *profile, nua_handle_t *nh, sofia_private_t *sofia_private, sip_t const *sip,
									 tagi_t tags[])
{
	if (sofia_private && sofia_private->gateway) {
		switch (status) {
		case 200:
			if (sip && sip->sip_contact && sip->sip_contact->m_expires) {
				char *new_expires = (char *) sip->sip_contact->m_expires;
				uint32_t expi = (uint32_t) atoi(new_expires);

				if (expi != sofia_private->gateway->freq) {
					sofia_private->gateway->freq = expi;
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG,
									  "Changing expire time to %d by request of proxy %s\n", expi, sofia_private->gateway->register_proxy);
				}

			}
			sofia_private->gateway->state = REG_STATE_REGISTER;
			break;
		case 100:
			break;
		default:
			sofia_private->gateway->state = REG_STATE_FAILED;
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "Registration Failed with status %d\n", status);
			break;
		}
	}
}

void sofia_reg_handle_sip_r_challenge(int status,
									  char const *phrase,
									  nua_t *nua, sofia_profile_t *profile, nua_handle_t *nh,
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

	if (session) {
		private_object_t *tech_pvt;
		switch_channel_t *channel = switch_core_session_get_channel(session);

		if ((tech_pvt = switch_core_session_get_private(session)) && switch_test_flag(tech_pvt, TFLAG_REFER)) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "received reply from refer\n");
			goto end;
		}

		gw_name = switch_channel_get_variable(channel, "sip_use_gateway");
	}


	if (sip->sip_www_authenticate) {
		authenticate = sip->sip_www_authenticate;
	} else if (sip->sip_proxy_authenticate) {
		authenticate = sip->sip_proxy_authenticate;
	} else {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Missing Authenticate Header!\n");
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
			var_gateway = sofia_reg_find_gateway((char *)gw_name);
		}


		if (!var_gateway && realm) {
			char rb[512] = "";
			char *p = (char *) realm;
			while((*p == '"')) {
				p++;
			}
			switch_set_string(rb, p);
			if ((p = strchr(rb, '"'))) {
				*p = '\0';
			}
			var_gateway = sofia_reg_find_gateway(rb);
		}

		if (!var_gateway && sip && sip->sip_to) {
			var_gateway = sofia_reg_find_gateway(sip->sip_to->a_url->url_host);
		}
		
		if (var_gateway) {
			gateway = var_gateway;
		}
	}



	if (!(scheme && realm)) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "No scheme and realm!\n");
		goto end;
	}

	if (!gateway) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "No Matching gateway found\n");
		goto cancel;
	}

	switch_snprintf(authentication, sizeof(authentication), "%s:%s:%s:%s", scheme, realm, gateway->auth_username, gateway->register_password);

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Authenticating '%s' with '%s'.\n", profile->username, authentication);


	ss_state = nua_callstate_authenticating;

	tl_gets(tags, NUTAG_CALLSTATE_REF(ss_state), SIPTAG_WWW_AUTHENTICATE_REF(authenticate), TAG_END());

	nua_authenticate(nh, SIPTAG_EXPIRES_STR(gateway->expires_str), NUTAG_AUTH(authentication), TAG_END());

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

auth_res_t sofia_reg_parse_auth(sofia_profile_t *profile, sip_authorization_t const *authorization, sip_t const *sip, const char *regstr,
								char *np, size_t nplen, char *ip, switch_event_t **v_event, long exptime, sofia_regtype_t regtype, const char *to_user)
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
	char *sql;
	char *mailbox = NULL;
	switch_xml_t domain, xml = NULL, user, param, uparams, dparams;
	char hexdigest[2 * SU_MD5_DIGEST_SIZE + 1] = "";
	char *domain_name = NULL;
	switch_event_t *params = NULL;
	const char *auth_acl = NULL;

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
	if ((regtype == REG_REGISTER) && (profile->pflags & PFLAG_CHECKUSER)) {
		if (switch_strlen_zero(username) || switch_strlen_zero(to_user) || strcasecmp(to_user, username)) {
			/* Names don't match, so fail */
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "SIP username %s does not match auth username\n", switch_str_nil(to_user));
			goto end;
		}
	}

	if (switch_strlen_zero(np)) {
		first = 1;
		sql = switch_mprintf("select nonce from sip_authentication where nonce='%q'", nonce);
		switch_assert(sql != NULL);
		if (!sofia_glue_execute_sql2str(profile, profile->ireg_mutex, sql, np, nplen)) {
			free(sql);
			ret = AUTH_STALE;
			goto end;
		}
		free(sql);
	}

	switch_event_create(&params, SWITCH_EVENT_MESSAGE);
	switch_assert(params);
	switch_event_add_header_string(params, SWITCH_STACK_BOTTOM, "action", "sip_auth");
	switch_event_add_header_string(params, SWITCH_STACK_BOTTOM, "sip_profile", profile->name);
	switch_event_add_header_string(params, SWITCH_STACK_BOTTOM, "sip_user_agent",
								   (sip && sip->sip_user_agent) ? sip->sip_user_agent->g_string : "unknown");
	switch_event_add_header_string(params, SWITCH_STACK_BOTTOM, "sip_auth_username", username);
	switch_event_add_header_string(params, SWITCH_STACK_BOTTOM, "sip_auth_realm", realm);
	switch_event_add_header_string(params, SWITCH_STACK_BOTTOM, "sip_auth_nonce", nonce);
	switch_event_add_header_string(params, SWITCH_STACK_BOTTOM, "sip_auth_uri", uri);
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


	if (!switch_strlen_zero(profile->reg_domain)) {
		domain_name = profile->reg_domain;
	} else {
		domain_name = realm;
	}

	if (switch_xml_locate_user("id", switch_strlen_zero(username) ? "nobody" : username, 
							   domain_name, ip, &xml, &domain, &user, params) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "can't find user [%s@%s]\n"
						  "You must define a domain called '%s' in your directory and add a user with the id=\"%s\" attribute\n"
						  "and you must configure your device to use the proper domain in it's authentication credentials.\n"
						  , username, domain_name, domain_name, username);

		ret = AUTH_FORBIDDEN;
		goto end;
	}
	
	if (!(mailbox = (char *) switch_xml_attr(user, "mailbox"))) {
		mailbox = switch_strlen_zero(username) ? "nobody" : username;
	}

	dparams = switch_xml_child(domain, "params");
	uparams = switch_xml_child(user, "params");

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
		}
	}

	if (auth_acl) {
		if (!switch_check_network_list_ip(ip, auth_acl)) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "IP %s Rejected by user acl %s\n", ip, auth_acl);
			ret = AUTH_FORBIDDEN;
			goto end;
		}
	}

	if (switch_strlen_zero(passwd) && switch_strlen_zero(a1_hash)) {
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

  for_the_sake_of_interop:

	if ((input = switch_mprintf("%s:%q", regstr, uri))) {
		su_md5_init(&ctx);
		su_md5_strupdate(&ctx, input);
		su_md5_hexdigest(&ctx, uridigest);
		su_md5_deinit(&ctx);
	}

	if (nc && cnonce && qop) {
		input2 = switch_mprintf("%q:%q:%q:%q:%q:%q", a1_hash, nonce, nc, cnonce, qop, uridigest);
	} else {
		input2 = switch_mprintf("%q:%q:%q", a1_hash, nonce, uridigest);
	}

	switch_assert(input2);

	memset(&ctx, 0, sizeof(ctx));
	su_md5_init(&ctx);
	su_md5_strupdate(&ctx, input2);
	su_md5_hexdigest(&ctx, bigdigest);
	su_md5_deinit(&ctx);

	if (!strcasecmp(bigdigest, response)) {
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


  skip_auth:

	if (first && ret == AUTH_OK) {
		if (v_event) {
			switch_event_create(v_event, SWITCH_EVENT_MESSAGE);
		}
		if (v_event && *v_event) {
			switch_xml_t xparams[2];
			int i = 0;

			switch_event_add_header_string(*v_event, SWITCH_STACK_BOTTOM, "sip_mailbox", mailbox);
			switch_event_add_header_string(*v_event, SWITCH_STACK_BOTTOM, "sip_auth_username", username);
			switch_event_add_header_string(*v_event, SWITCH_STACK_BOTTOM, "sip_auth_realm", realm);
			switch_event_add_header_string(*v_event, SWITCH_STACK_BOTTOM, "mailbox", mailbox);
			switch_event_add_header_string(*v_event, SWITCH_STACK_BOTTOM, "user_name", username);
			switch_event_add_header_string(*v_event, SWITCH_STACK_BOTTOM, "domain_name", realm);

			if ((dparams = switch_xml_child(domain, "variables"))) {
				xparams[i++] = dparams;
			}

			if ((uparams = switch_xml_child(user, "variables"))) {
				xparams[i++] = uparams;
			}

			if (dparams || uparams) {
				int j = 0;

				for (j = 0; j < i; j++) {
					for (param = switch_xml_child(xparams[j], "variable"); param; param = param->next) {
						const char *var = switch_xml_attr_soft(param, "name");
						const char *val = switch_xml_attr_soft(param, "value");
						sofia_gateway_t *gateway_ptr = NULL;

						if (!switch_strlen_zero(var) && !switch_strlen_zero(val)) {
							switch_event_add_header_string(*v_event, SWITCH_STACK_BOTTOM, var, val);

							if (!strcasecmp(var, "register-gateway")) {
								if (!strcasecmp(val, "all")) {
									switch_xml_t gateways_tag, gateway_tag;
									if ((gateways_tag = switch_xml_child(user, "gateways"))) {
										for (gateway_tag = switch_xml_child(gateways_tag, "gateway"); gateway_tag; gateway_tag = gateway_tag->next) {
											char *name = (char *) switch_xml_attr_soft(gateway_tag, "name");
											if (switch_strlen_zero(name)) {
												name = "anonymous";
											}

											if ((gateway_ptr = sofia_reg_find_gateway(name))) {
												gateway_ptr->retry = 0;
												if (exptime) {
													gateway_ptr->state = REG_STATE_UNREGED;
												} else {
													gateway_ptr->state = REG_STATE_UNREGISTER;
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
											gateway_ptr->retry = 0;
											if (exptime) {
												gateway_ptr->state = REG_STATE_UNREGED;
											} else {
												gateway_ptr->state = REG_STATE_UNREGISTER;
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

	switch_event_destroy(&params);

	if (xml) {
		switch_xml_free(xml);
	}

	switch_safe_free(input);
	switch_safe_free(input2);
	switch_safe_free(username);
	switch_safe_free(realm);
	switch_safe_free(nonce);
	switch_safe_free(uri);
	switch_safe_free(qop);
	switch_safe_free(cnonce);
	switch_safe_free(nc);
	switch_safe_free(response);

	return ret;

}


sofia_gateway_t *sofia_reg_find_gateway__(const char *file, const char *func, int line, const char *key)
{
	sofia_gateway_t *gateway = NULL;

	switch_mutex_lock(mod_sofia_globals.hash_mutex);
	if ((gateway = (sofia_gateway_t *) switch_core_hash_find(mod_sofia_globals.gateway_hash, key))) {
		if (!(gateway->profile->pflags & PFLAG_RUNNING) || gateway->deleted) {
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

void sofia_reg_release_gateway__(const char *file, const char *func, int line, sofia_gateway_t *gateway)
{
	switch_thread_rwlock_unlock(gateway->profile->rwlock);
#ifdef SOFIA_DEBUG_RWLOCKS
	switch_log_printf(SWITCH_CHANNEL_ID_LOG, file, func, line, SWITCH_LOG_ERROR, "XXXXXXXXXXXXXX GW UNLOCK %s\n", gateway->profile->name);
#endif
}

switch_status_t sofia_reg_add_gateway(char *key, sofia_gateway_t *gateway)
{
	switch_status_t status = SWITCH_STATUS_FALSE;

	switch_mutex_lock(mod_sofia_globals.hash_mutex);
	if (!switch_core_hash_find(mod_sofia_globals.gateway_hash, key)) {
		status = switch_core_hash_insert(mod_sofia_globals.gateway_hash, key, gateway);
	}
	switch_mutex_unlock(mod_sofia_globals.hash_mutex);

	if (status == SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "Added gateway '%s' to profile '%s'\n", gateway->name, gateway->profile->name);
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
