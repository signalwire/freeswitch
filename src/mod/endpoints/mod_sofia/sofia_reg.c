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
 * Ken Rice, Asteria Solutions Group, Inc <ken@asteriasgi.com>
 * Paul D. Tinsley <pdt at jackhammer.org>
 * Bret McDanel <trixter AT 0xdecafbad.com>
 * Marcel Barbulescu <marcelbarbulescu@gmail.com>
 *
 *
 * sofia_ref.c -- SOFIA SIP Endpoint (registration code)
 *
 */
#include "mod_sofia.h"



void sofia_reg_unregister(sofia_profile_t *profile)
{
	sofia_gateway_t *gateway_ptr;
	for (gateway_ptr = profile->gateways; gateway_ptr; gateway_ptr = gateway_ptr->next) {
		if (gateway_ptr->sofia_private) {
			free(gateway_ptr->sofia_private);
			nua_handle_bind(gateway_ptr->nh, NULL);
			gateway_ptr->sofia_private = NULL;
		}
		nua_handle_destroy(gateway_ptr->nh);
	}
}

void sofia_reg_check_gateway(sofia_profile_t *profile, time_t now)
{
	sofia_gateway_t *gateway_ptr;
	for (gateway_ptr = profile->gateways; gateway_ptr; gateway_ptr = gateway_ptr->next) {
		int ss_state = nua_callstate_authenticating;
		reg_state_t ostate = gateway_ptr->state;

		if (!now) {
			gateway_ptr->state = ostate = REG_STATE_UNREGED;
			gateway_ptr->expires_str = "0";
		}

		switch (ostate) {
		case REG_STATE_NOREG:
			break;
		case REG_STATE_REGISTER:
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "registered %s\n", gateway_ptr->name);
			gateway_ptr->expires = now + gateway_ptr->freq;
			gateway_ptr->state = REG_STATE_REGED;
			break;
		case REG_STATE_UNREGED:
			if ((gateway_ptr->nh = nua_handle(gateway_ptr->profile->nua, NULL,
											  NUTAG_URL(gateway_ptr->register_proxy),
											  SIPTAG_TO_STR(gateway_ptr->register_to),
											  NUTAG_CALLSTATE_REF(ss_state), SIPTAG_FROM_STR(gateway_ptr->register_from), TAG_END()))) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "registering %s\n", gateway_ptr->name);

				if (!(gateway_ptr->sofia_private = malloc(sizeof(*gateway_ptr->sofia_private)))) {
					abort();
				}
				memset(gateway_ptr->sofia_private, 0, sizeof(*gateway_ptr->sofia_private));

				gateway_ptr->sofia_private->gateway = gateway_ptr;
				nua_handle_bind(gateway_ptr->nh, gateway_ptr->sofia_private);

				if (now) {
					nua_register(gateway_ptr->nh,
								 SIPTAG_FROM_STR(gateway_ptr->register_from),
								 SIPTAG_CONTACT_STR(gateway_ptr->register_contact),
								 SIPTAG_EXPIRES_STR(gateway_ptr->expires_str),
								 NUTAG_REGISTRAR(gateway_ptr->register_proxy),
								 NUTAG_OUTBOUND("no-options-keepalive"), NUTAG_OUTBOUND("no-validate"), NUTAG_KEEPALIVE(0), TAG_NULL());
					gateway_ptr->retry = now + 10;
				} else {
					nua_unregister(gateway_ptr->nh,
								   SIPTAG_FROM_STR(gateway_ptr->register_from),
								   SIPTAG_CONTACT_STR(gateway_ptr->register_contact),
								   SIPTAG_EXPIRES_STR(gateway_ptr->expires_str),
								   NUTAG_REGISTRAR(gateway_ptr->register_proxy),
								   NUTAG_OUTBOUND("no-options-keepalive"), NUTAG_OUTBOUND("no-validate"), NUTAG_KEEPALIVE(0), TAG_NULL());
				}

				gateway_ptr->retry = now + time(NULL);
				gateway_ptr->state = REG_STATE_TRYING;

			} else {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Error registering %s\n", gateway_ptr->name);
				gateway_ptr->state = REG_STATE_FAILED;
			}
			break;

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

int sofia_reg_del_callback(void *pArg, int argc, char **argv, char **columnNames)
{
	switch_event_t *s_event;

	if (argc >= 3) {
		if (switch_event_create_subclass(&s_event, SWITCH_EVENT_CUSTOM, MY_EVENT_EXPIRE) == SWITCH_STATUS_SUCCESS) {
			switch_event_add_header(s_event, SWITCH_STACK_BOTTOM, "profile-name", "%s", argv[0]);
			switch_event_add_header(s_event, SWITCH_STACK_BOTTOM, "user", "%s", argv[1]);
			switch_event_add_header(s_event, SWITCH_STACK_BOTTOM, "host", "%s", argv[2]);
			switch_event_add_header(s_event, SWITCH_STACK_BOTTOM, "contact", "%s", argv[3]);
			switch_event_add_header(s_event, SWITCH_STACK_BOTTOM, "expires", "%s", argv[4]);
			switch_event_fire(&s_event);
		}
	}
	return 0;
}

void sofia_reg_check_expire(sofia_profile_t *profile, time_t now)
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

	if (now) {
		snprintf(sql, sizeof(sql), "select '%s',* from sip_registrations where expires > 0 and expires <= %ld", profile->name, (long) now);
	} else {
		snprintf(sql, sizeof(sql), "select '%s',* from sip_registrations where expires > 0", profile->name);
	}

	switch_mutex_lock(profile->ireg_mutex);
	sofia_glue_execute_sql_callback(profile,
									SWITCH_TRUE,
									NULL,
									sql,
									sofia_reg_del_callback,
									NULL);
	if (now) {
		snprintf(sql, sizeof(sql), "delete from sip_registrations where expires > 0 and expires <= %ld", (long) now);
	} else {
		snprintf(sql, sizeof(sql), "delete from sip_registrations where expires > 0");
	}
	sofia_glue_execute_sql(profile, SWITCH_TRUE, sql, NULL);
	if (now) {
		snprintf(sql, sizeof(sql), "delete from sip_authentication where expires > 0 and expires <= %ld", (long) now);
	} else {
		snprintf(sql, sizeof(sql), "delete from sip_authentication where expires > 0");
	}
	sofia_glue_execute_sql(profile, SWITCH_TRUE, sql, NULL);
	if (now) {
		snprintf(sql, sizeof(sql), "delete from sip_subscriptions where expires > 0 and expires <= %ld", (long) now);
	} else {
		snprintf(sql, sizeof(sql), "delete from sip_subscriptions where expires > 0");
	}
	sofia_glue_execute_sql(profile, SWITCH_TRUE, sql, NULL);

	switch_mutex_unlock(profile->ireg_mutex);

}

char *sofia_reg_find_reg_url(sofia_profile_t *profile, const char *user, const char *host, char *val, switch_size_t len)
{
	struct callback_t cbt = { 0 };

	if (!user) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Called with null user!\n");
		return NULL;
	}

	cbt.val = val;
	cbt.len = len;

	if (host) {
		snprintf(val, len, "select contact from sip_registrations where user='%s' and host='%s'", user, host);
	} else {
		snprintf(val, len, "select contact from sip_registrations where user='%s'", user);
	}


	sofia_glue_execute_sql_callback(profile,
									SWITCH_FALSE,
									profile->ireg_mutex,
									val,
									sofia_reg_find_callback,
									&cbt);


	if (cbt.matches) {
		return val;
	} else {
		return NULL;
	}
}

uint8_t sofia_reg_handle_register(nua_t * nua, sofia_profile_t *profile, nua_handle_t * nh, sip_t const *sip, sofia_regtype_t regtype, char *key,
								  uint32_t keylen, switch_event_t **v_event)
{
	sip_from_t const *from = NULL;
	sip_expires_t const *expires = NULL;
	sip_authorization_t const *authorization = NULL;
	sip_contact_t const *contact = NULL;
	char *sql;
	switch_event_t *s_event;
	const char *from_user = NULL;
	const char *from_host = NULL;
	char contact_str[1024] = "";
	char buf[512];
	uint8_t stale = 0, forbidden = 0;
	auth_res_t auth_res;
	long exptime = 60;
	switch_event_t *event;
	const char *rpid = "unknown";
	const char *display = "\"user\"";
	char network_ip[80];

	/* all callers must confirm that sip, sip->sip_request and sip->sip_contact are not NULL */
	assert(sip != NULL && sip->sip_contact != NULL && sip->sip_request != NULL);

	get_addr(network_ip, sizeof(network_ip), &((struct sockaddr_in *) msg_addrinfo(nua_current_request(nua))->ai_addr)->sin_addr);

	expires = sip->sip_expires;
	authorization = sip->sip_authorization;
	contact = sip->sip_contact;
	from = sip->sip_from;

	if (from) {
		from_user = from->a_url->url_user;
		from_host = from->a_url->url_host;
	}

	if (!from_user || !from_host) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Can not do authorization without a complete from header\n");
		nua_respond(nh, SIP_500_INTERNAL_SERVER_ERROR, TAG_END());
		return 1;
	}

	if (contact->m_url) {
		const char *port = contact->m_url->url_port;
		display = contact->m_display;

		if (switch_strlen_zero(display)) {
			if (from) {
				display = from->a_display;
				if (switch_strlen_zero(display)) {
					display = "\"user\"";
				}
			}
		}

		if (!port) {
			port = SOFIA_DEFAULT_PORT;
		}

		if (contact->m_url->url_params) {
			snprintf(contact_str, sizeof(contact_str), "%s <sip:%s@%s:%s;%s>",
					 display, contact->m_url->url_user, contact->m_url->url_host, port, contact->m_url->url_params);
		} else {
			snprintf(contact_str, sizeof(contact_str), "%s <sip:%s@%s:%s>", display, contact->m_url->url_user, contact->m_url->url_host, port);
		}
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

	if ((profile->pflags & PFLAG_BLIND_REG)) {
		goto reg;
	}

	if (authorization) {
		if ((auth_res = sofia_reg_parse_auth(profile, authorization, sip->sip_request->rq_method_name, key, keylen, network_ip, v_event)) == AUTH_STALE) {
			stale = 1;
		}
		
		if (auth_res != AUTH_OK && !stale) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "send %s for [%s@%s]\n", forbidden ? "forbidden" : "challange", from_user, from_host);
			if (auth_res == AUTH_FORBIDDEN) {
				nua_respond(nh, SIP_403_FORBIDDEN, NUTAG_WITH_THIS(nua), TAG_END());
			} else {
				nua_respond(nh, SIP_401_UNAUTHORIZED, NUTAG_WITH_THIS(nua), TAG_END());
			}
			return 1;
		}
	}

	if (!authorization || stale) {
		switch_uuid_t uuid;
		char uuid_str[SWITCH_UUID_FORMATTED_LENGTH + 1];
		char *sql, *auth_str;

		switch_uuid_get(&uuid);
		switch_uuid_format(uuid_str, &uuid);

		switch_mutex_lock(profile->ireg_mutex);
		sql = switch_mprintf("insert into sip_authentication (nonce, expires) values('%q', %ld)",
							uuid_str, time(NULL) + profile->nonce_ttl);
		assert(sql != NULL);
		sofia_glue_execute_sql(profile, SWITCH_FALSE, sql, NULL);
		switch_safe_free(sql);
		switch_mutex_unlock(profile->ireg_mutex);

		auth_str =
			switch_mprintf("Digest realm=\"%q\", nonce=\"%q\",%s algorithm=MD5, qop=\"auth\"", from_host, uuid_str, stale ? " stale=\"true\"," : "");

		if (regtype == REG_REGISTER) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Requesting Registration from: [%s@%s]\n", from_user, from_host);
			nua_respond(nh, SIP_401_UNAUTHORIZED, NUTAG_WITH_THIS(nua), SIPTAG_WWW_AUTHENTICATE_STR(auth_str), TAG_END());
		} else if (regtype == REG_INVITE) {
			nua_respond(nh, SIP_407_PROXY_AUTH_REQUIRED, NUTAG_WITH_THIS(nua), SIPTAG_PROXY_AUTHENTICATE_STR(auth_str), TAG_END());
		}

		switch_safe_free(auth_str);
		return 1;
	}
  reg:

	if (exptime) {
		if (!sofia_reg_find_reg_url(profile, from_user, from_host, buf, sizeof(buf))) {
			sql = switch_mprintf("insert into sip_registrations values ('%q','%q','%q','Registered', '%q', %ld)",
								 from_user, from_host, contact_str, rpid, (long) time(NULL) + (long) exptime * 2);

		} else {
			sql =
				switch_mprintf
				("update sip_registrations set contact='%q', expires=%ld, rpid='%q' where user='%q' and host='%q'",
				 contact_str, (long) time(NULL) + (long) exptime * 2, rpid, from_user, from_host);

		}

		if (switch_event_create_subclass(&s_event, SWITCH_EVENT_CUSTOM, MY_EVENT_REGISTER) == SWITCH_STATUS_SUCCESS) {
			switch_event_add_header(s_event, SWITCH_STACK_BOTTOM, "profile-name", "%s", profile->name);
			switch_event_add_header(s_event, SWITCH_STACK_BOTTOM, "from-user", "%s", from_user);
			switch_event_add_header(s_event, SWITCH_STACK_BOTTOM, "from-host", "%s", from_host);
			switch_event_add_header(s_event, SWITCH_STACK_BOTTOM, "contact", "%s", contact_str);
			switch_event_add_header(s_event, SWITCH_STACK_BOTTOM, "rpid", "%s", rpid);
			switch_event_add_header(s_event, SWITCH_STACK_BOTTOM, "expires", "%ld", (long) exptime);
			switch_event_fire(&s_event);
		}

		if (sql) {
			sofia_glue_execute_sql(profile, SWITCH_FALSE, sql, profile->ireg_mutex);
			switch_safe_free(sql);
			sql = NULL;
		}

		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG,
						  "Register:\nFrom:    [%s@%s]\nContact: [%s]\nExpires: [%ld]\n", from_user, from_host, contact_str, (long) exptime);


		if (switch_event_create(&event, SWITCH_EVENT_PRESENCE_IN) == SWITCH_STATUS_SUCCESS) {
			switch_event_add_header(event, SWITCH_STACK_BOTTOM, "proto", "sip");
			switch_event_add_header(event, SWITCH_STACK_BOTTOM, "login", "%s", profile->url);
			switch_event_add_header(event, SWITCH_STACK_BOTTOM, "rpid", "%s", rpid);
			switch_event_add_header(event, SWITCH_STACK_BOTTOM, "from", "%s@%s", from_user, from_host);

			switch_event_add_header(event, SWITCH_STACK_BOTTOM, "status", "Registered");
			switch_event_add_header(event, SWITCH_STACK_BOTTOM, "event_type", "presence");
			switch_event_fire(&event);
		}
	} else {
		if ((sql = switch_mprintf("delete from sip_subscriptions where user='%q' and host='%q'", from_user, from_host))) {
			sofia_glue_execute_sql(profile, SWITCH_FALSE, sql, profile->ireg_mutex);
			switch_safe_free(sql);
			sql = NULL;
		}
		if ((sql = switch_mprintf("delete from sip_registrations where user='%q' and host='%q'", from_user, from_host))) {
			sofia_glue_execute_sql(profile, SWITCH_FALSE, sql, profile->ireg_mutex);
			switch_safe_free(sql);
			sql = NULL;
		}
		if (switch_event_create(&event, SWITCH_EVENT_PRESENCE_OUT) == SWITCH_STATUS_SUCCESS) {
			switch_event_add_header(event, SWITCH_STACK_BOTTOM, "proto", "sip");
			switch_event_add_header(event, SWITCH_STACK_BOTTOM, "login", "%s", profile->url);
			switch_event_add_header(event, SWITCH_STACK_BOTTOM, "from", "%s+%s@%s", SOFIA_CHAT_PROTO, from_user, from_host);

			switch_event_add_header(event, SWITCH_STACK_BOTTOM, "status", "unavailable");
			switch_event_add_header(event, SWITCH_STACK_BOTTOM, "rpid", "%s", rpid);
			switch_event_add_header(event, SWITCH_STACK_BOTTOM, "event_type", "presence");
			switch_event_fire(&event);
		}
	}


	if (switch_event_create(&event, SWITCH_EVENT_ROSTER) == SWITCH_STATUS_SUCCESS) {
		switch_event_add_header(event, SWITCH_STACK_BOTTOM, "proto", "sip");
		switch_event_add_header(event, SWITCH_STACK_BOTTOM, "from", "%s@%s", from_user, from_host);
		switch_event_fire(&event);
	}

	if (regtype == REG_REGISTER) {
		nua_respond(nh, SIP_200_OK, SIPTAG_CONTACT(contact), NUTAG_WITH_THIS(nua), TAG_END());
		return 1;
	}

	return 0;
}



void sofia_reg_handle_sip_i_register(nua_t * nua, sofia_profile_t *profile, nua_handle_t * nh, sofia_private_t * sofia_private, sip_t const *sip, tagi_t tags[])
{
	char key[128] = "";

	if (!sip || !sip->sip_request || !sip->sip_request->rq_method_name) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Received an invalid packet!\n");
		nua_respond(nh, SIP_500_INTERNAL_SERVER_ERROR, TAG_END());
		return;
	}

	if (!(sip->sip_contact && sip->sip_contact->m_url)) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "NO CONTACT!\n");
		nua_respond(nh, 400, "Missing Contact Header", TAG_END());
		return;
	}

	sofia_reg_handle_register(nua, profile, nh, sip, REG_REGISTER, key, sizeof(key), NULL);
}


void sofia_reg_handle_sip_r_register(int status,
						   char const *phrase,
						   nua_t * nua, sofia_profile_t *profile, nua_handle_t * nh, sofia_private_t * sofia_private, sip_t const *sip, tagi_t tags[])
{
	if (sofia_private && sofia_private->gateway) {
		switch (status) {
		case 200:
			if (sip && sip->sip_contact && sip->sip_contact->m_expires) {
				char *new_expires = (char *) sip->sip_contact->m_expires;
				uint32_t expi = (uint32_t) atoi(new_expires);

				if (expi != sofia_private->gateway->freq) {
					sofia_private->gateway->freq = expi;
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE,
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
							nua_t * nua, sofia_profile_t *profile, nua_handle_t * nh, switch_core_session_t *session, sip_t const *sip, tagi_t tags[])
{
	sofia_gateway_t *gateway = NULL;
	sip_www_authenticate_t const *authenticate = NULL;
	char const *realm = NULL;
	char *p = NULL, *duprealm = NULL, *qrealm = NULL;
	char const *scheme = NULL;
	int indexnum;
	char *cur;
	char authentication[256] = "";
	int ss_state;

	if (session) {
		private_object_t *tech_pvt;
		if ((tech_pvt = switch_core_session_get_private(session)) && switch_test_flag(tech_pvt, TFLAG_REFER)) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "received reply from refer\n");
			return;
		}
	}


	if (sip->sip_www_authenticate) {
		authenticate = sip->sip_www_authenticate;
	} else if (sip->sip_proxy_authenticate) {
		authenticate = sip->sip_proxy_authenticate;
	} else {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Missing Authenticate Header!\n");
		return;
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

	if (!(scheme && realm)) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "No scheme and realm!\n");
		return;
	}

	if (profile) {
		sofia_gateway_t *gateway_ptr;

		if ((duprealm = strdup(realm))) {
			qrealm = duprealm;

			while (*qrealm && *qrealm == '"') {
				qrealm++;
			}

			if ((p = strchr(qrealm, '"'))) {
				*p = '\0';
			}

			if (sip->sip_from) {
				char *from_key = switch_mprintf("sip:%s@%s",
												(char *) sip->sip_from->a_url->url_user,
												(char *) sip->sip_from->a_url->url_host);

				if (!(gateway = sofia_reg_find_gateway(from_key))) {
					gateway = sofia_reg_find_gateway(qrealm);
				}

				switch_safe_free(from_key);
			}

			if (!gateway) {
				switch_mutex_lock(mod_sofia_globals.hash_mutex);
				for (gateway_ptr = profile->gateways; gateway_ptr; gateway_ptr = gateway_ptr->next) {
					if (scheme && qrealm && !strcasecmp(gateway_ptr->register_scheme, scheme)
						&& !strcasecmp(gateway_ptr->register_realm, qrealm)) {
						gateway = gateway_ptr;

						if (!switch_test_flag(gateway->profile, PFLAG_RUNNING)) {
							gateway = NULL;
						} else if (switch_thread_rwlock_tryrdlock(gateway->profile->rwlock) != SWITCH_STATUS_SUCCESS) {
							gateway = NULL;
						}						
						break;
					}
				}
				switch_mutex_unlock(mod_sofia_globals.hash_mutex);
			}

			if (!gateway) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "No Match for Scheme [%s] Realm [%s]\n", scheme, qrealm);
				goto cancel;
			}
			switch_safe_free(duprealm);
		} else {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Memory Error!\n");
			goto cancel;
		}
	}

	snprintf(authentication, sizeof(authentication), "%s:%s:%s:%s", scheme, realm, gateway->register_username, gateway->register_password);

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Authenticating '%s' with '%s'.\n", profile->username, authentication);


	ss_state = nua_callstate_authenticating;

	tl_gets(tags, NUTAG_CALLSTATE_REF(ss_state), SIPTAG_WWW_AUTHENTICATE_REF(authenticate), TAG_END());

	nua_authenticate(nh, SIPTAG_EXPIRES_STR(gateway->expires_str), NUTAG_AUTH(authentication), TAG_END());
	sofia_reg_release_gateway(gateway);
	return;

 cancel:
	if (session) {
		switch_channel_t *channel = switch_core_session_get_channel(session);
		switch_channel_hangup(channel, SWITCH_CAUSE_MANDATORY_IE_MISSING);
	} else {
		nua_cancel(nh, TAG_END());
	}
	
}

auth_res_t sofia_reg_parse_auth(sofia_profile_t *profile, sip_authorization_t const *authorization, 
								const char *regstr, char *np, size_t nplen, char *ip, switch_event_t **v_event)
{
	int indexnum;
	const char *cur;
	su_md5_t ctx;
	char uridigest[2 * SU_MD5_DIGEST_SIZE + 1];
	char bigdigest[2 * SU_MD5_DIGEST_SIZE + 1];
	char *username, *realm, *nonce, *uri, *qop, *cnonce, *nc, *response, *input = NULL, *input2 = NULL;
	auth_res_t ret = AUTH_FORBIDDEN;
	char *npassword = NULL;
	int cnt = 0;

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
						cnt++;
					} else if (!strcasecmp(var, "realm")) {
						realm = strdup(val);
						cnt++;
					} else if (!strcasecmp(var, "nonce")) {
						nonce = strdup(val);
						cnt++;
					} else if (!strcasecmp(var, "uri")) {
						uri = strdup(val);
						cnt++;
					} else if (!strcasecmp(var, "qop")) {
						qop = strdup(val);
						cnt++;
					} else if (!strcasecmp(var, "cnonce")) {
						cnonce = strdup(val);
						cnt++;
					} else if (!strcasecmp(var, "response")) {
						response = strdup(val);
						cnt++;
					} else if (!strcasecmp(var, "nc")) {
						nc = strdup(val);
						cnt++;
					}
				}

				free(work);
			}
		}
	}

	if (cnt != 8) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Invalid Authorization header!\n");
		goto end;
	}

	if (switch_strlen_zero(np)) {
		switch_xml_t domain, xml, user, param, xparams;
		const char *passwd = NULL;
		const char *a1_hash = NULL;
		char *sql;

		sql = switch_mprintf("select nonce from sip_authentication where nonce='%q'", nonce);
		assert(sql != NULL);
		if (!sofia_glue_execute_sql2str(profile, profile->ireg_mutex, sql, np, nplen)) {
			free(sql);
			ret = AUTH_STALE;
			goto end;
		}
		free(sql);

		if (switch_xml_locate_user(username, realm, ip, &xml, &domain, &user) != SWITCH_STATUS_SUCCESS) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "can't find user [%s@%s]\n", username, realm);
			ret = AUTH_FORBIDDEN;
			goto end;
		}
		
		if (!(xparams = switch_xml_child(user, "params"))) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "can't find params for user [%s@%s]\n", username, realm);
			ret = AUTH_FORBIDDEN;
			goto end;
		}
		
		for (param = switch_xml_child(xparams, "param"); param; param = param->next) {
			const char *var = switch_xml_attr_soft(param, "name");
			const char *val = switch_xml_attr_soft(param, "value");

			if (!strcasecmp(var, "password")) {
				passwd = val;
			}

			if (!strcasecmp(var, "a1-hash")) {
				a1_hash = val;
			}
		}

		if (v_event && (xparams = switch_xml_child(user, "variables"))) {
			if (switch_event_create(v_event, SWITCH_EVENT_MESSAGE) == SWITCH_STATUS_SUCCESS) {
				for (param = switch_xml_child(xparams, "variable"); param; param = param->next) {
					const char *var = switch_xml_attr_soft(param, "name");
					const char *val = switch_xml_attr_soft(param, "value");

					if (!switch_strlen_zero(var) && !switch_strlen_zero(val)) {
						switch_event_add_header(*v_event, SWITCH_STACK_BOTTOM, var, "%s", val);
					}
				}
			}
		}


		if (switch_strlen_zero(passwd) && switch_strlen_zero(a1_hash)) {
			ret = AUTH_OK;
			goto end;
		}

		if (!a1_hash) {
			su_md5_t ctx;
			char hexdigest[2 * SU_MD5_DIGEST_SIZE + 1];
			char *input;

			input = switch_mprintf("%s:%s:%s", username, realm, passwd);
			su_md5_init(&ctx);
			su_md5_strupdate(&ctx, input);
			su_md5_hexdigest(&ctx, hexdigest);
			su_md5_deinit(&ctx);
			switch_safe_free(input);
			a1_hash = hexdigest;
			
		}

		np = strdup(a1_hash);

		switch_xml_free(xml);
	}

	npassword = np;

	if ((input = switch_mprintf("%s:%q", regstr, uri))) {
		su_md5_init(&ctx);
		su_md5_strupdate(&ctx, input);
		su_md5_hexdigest(&ctx, uridigest);
		su_md5_deinit(&ctx);
	}

	if ((input2 = switch_mprintf("%q:%q:%q:%q:%q:%q", npassword, nonce, nc, cnonce, qop, uridigest))) {
		memset(&ctx, 0, sizeof(ctx));
		su_md5_init(&ctx);
		su_md5_strupdate(&ctx, input2);
		su_md5_hexdigest(&ctx, bigdigest);
		su_md5_deinit(&ctx);

		if (!strcasecmp(bigdigest, response)) {
			ret = AUTH_OK;
		} else {
			ret = AUTH_FORBIDDEN;
		}
	}

  end:
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


sofia_gateway_t *sofia_reg_find_gateway(char *key)
{
	sofia_gateway_t *gateway = NULL;

	switch_mutex_lock(mod_sofia_globals.hash_mutex);
	if ((gateway = (sofia_gateway_t *) switch_core_hash_find(mod_sofia_globals.gateway_hash, key))) {
		if (!sofia_test_pflag(gateway->profile, PFLAG_RUNNING)) {
			gateway = NULL;
		} else if (switch_thread_rwlock_tryrdlock(gateway->profile->rwlock) != SWITCH_STATUS_SUCCESS) {
			gateway = NULL;
		}
	}

	switch_mutex_unlock(mod_sofia_globals.hash_mutex);
	return gateway;
}

void sofia_reg_release_gateway(sofia_gateway_t *gateway)
{
	switch_thread_rwlock_unlock(gateway->profile->rwlock);
}

switch_status_t sofia_reg_add_gateway(char *key, sofia_gateway_t *gateway)
{
	switch_status_t status = SWITCH_STATUS_FALSE;

	switch_mutex_lock(mod_sofia_globals.hash_mutex);
	if (!switch_core_hash_find(mod_sofia_globals.gateway_hash, key)) {
		status = switch_core_hash_insert(mod_sofia_globals.gateway_hash, key, gateway);
	}
	switch_mutex_unlock(mod_sofia_globals.hash_mutex);

	return status;
}



