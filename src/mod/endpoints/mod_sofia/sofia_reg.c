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
 * Ken Rice, krice@suspicious.org  (work sponsored by CopperCom, Inc and Asteria Solutions Group, Inc)
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
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "registered %s\n", gateway_ptr->name);
			gateway_ptr->expires = now + gateway_ptr->freq;
			gateway_ptr->state = REG_STATE_REGED;
			break;

		case REG_STATE_UNREGISTER:
            sofia_reg_kill_reg(gateway_ptr, 1);
			gateway_ptr->state = REG_STATE_NOREG;
			break;
		case REG_STATE_UNREGED:

            sofia_reg_kill_reg(gateway_ptr, 1);

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

	switch_snprintf(to, sizeof(to), "%s@%s", argv[1], argv[2]);
	contact = sofia_glue_get_url_from_contact(argv[3], 1);

	nh = nua_handle(profile->nua, NULL, SIPTAG_FROM_STR(profile->url), SIPTAG_TO_STR(to), NUTAG_URL(contact), SIPTAG_CONTACT_STR(profile->url), TAG_END());

	nua_options(nh, TAG_END());

	switch_safe_free(contact);
	
	return 0;
}

int sofia_reg_del_callback(void *pArg, int argc, char **argv, char **columnNames)
{
	switch_event_t *s_event;

	if (argc >= 3) {
		if (switch_event_create_subclass(&s_event, SWITCH_EVENT_CUSTOM, MY_EVENT_EXPIRE) == SWITCH_STATUS_SUCCESS) {
			switch_event_add_header(s_event, SWITCH_STACK_BOTTOM, "profile-name", "%s", argv[6]);
			switch_event_add_header(s_event, SWITCH_STACK_BOTTOM, "call-id", "%s", argv[0]);
			switch_event_add_header(s_event, SWITCH_STACK_BOTTOM, "user", "%s", argv[1]);
			switch_event_add_header(s_event, SWITCH_STACK_BOTTOM, "host", "%s", argv[2]);
			switch_event_add_header(s_event, SWITCH_STACK_BOTTOM, "contact", "%s", argv[3]);
			switch_event_add_header(s_event, SWITCH_STACK_BOTTOM, "expires", "%s", argv[4]);
			switch_event_add_header(s_event, SWITCH_STACK_BOTTOM, "user-agent", "%s", argv[5]);
			switch_event_fire(&s_event);
		}
	}
	return 0;
}


void sofia_reg_check_expire(sofia_profile_t *profile, time_t now)
{
	char sql[1024];
	char *psql = sql;

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
		switch_snprintf(sql, sizeof(sql), "select *,'%s' from sip_registrations where expires > 0 and expires <= %ld", profile->name, (long) now);
	} else {
		switch_snprintf(sql, sizeof(sql), "select *,'%s' from sip_registrations where expires > 0", profile->name);
	}

	switch_mutex_lock(profile->ireg_mutex);
	sofia_glue_execute_sql_callback(profile,
									SWITCH_TRUE,
									NULL,
									sql,
									sofia_reg_del_callback,
									NULL);
	if (now) {
		switch_snprintf(sql, sizeof(sql), "delete from sip_registrations where expires > 0 and expires <= %ld", (long) now);
	} else {
		switch_snprintf(sql, sizeof(sql), "delete from sip_registrations where expires > 0");
	}
	sofia_glue_execute_sql(profile, &psql, SWITCH_FALSE);
	if (now) {
		switch_snprintf(sql, sizeof(sql), "delete from sip_authentication where expires > 0 and expires <= %ld", (long) now);
	} else {
		switch_snprintf(sql, sizeof(sql), "delete from sip_authentication where expires > 0");
	}
	sofia_glue_execute_sql(profile, &psql, SWITCH_FALSE);
	if (now) {
		switch_snprintf(sql, sizeof(sql), "delete from sip_subscriptions where expires > 0 and expires <= %ld", (long) now);
	} else {
		switch_snprintf(sql, sizeof(sql), "delete from sip_subscriptions where expires > 0");
	}
	sofia_glue_execute_sql(profile, &psql, SWITCH_FALSE);

	if (now) {
		switch_snprintf(sql, sizeof(sql), "select * from sip_registrations where status like '%%NATHACK%%'");
		sofia_glue_execute_sql_callback(profile,
										SWITCH_TRUE,
										NULL,
										sql,
										sofia_reg_nat_callback,
										profile);
	}

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
		switch_snprintf(val, len, "select contact from sip_registrations where sip_user='%s' and sip_host='%s'", user, host);
	} else {
		switch_snprintf(val, len, "select contact from sip_registrations where sip_user='%s'", user);
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


void sofia_reg_auth_challange(nua_t *nua, sofia_profile_t *profile, nua_handle_t *nh, sofia_regtype_t regtype, const char *realm, int stale)
{
	switch_uuid_t uuid;
	char uuid_str[SWITCH_UUID_FORMATTED_LENGTH + 1];
	char *sql, *auth_str;

	switch_uuid_get(&uuid);
	switch_uuid_format(uuid_str, &uuid);

	sql = switch_mprintf("insert into sip_authentication (nonce, expires) values('%q', %ld)",
						 uuid_str, switch_timestamp(NULL) + profile->nonce_ttl);
	switch_assert(sql != NULL);
	sofia_glue_execute_sql(profile, &sql, SWITCH_TRUE);

	auth_str =
		switch_mprintf("Digest realm=\"%q\", nonce=\"%q\",%s algorithm=MD5, qop=\"auth\"", realm, uuid_str, stale ? " stale=\"true\"," : "");

	if (regtype == REG_REGISTER) {
		nua_respond(nh, SIP_401_UNAUTHORIZED, TAG_IF(nua, NUTAG_WITH_THIS(nua)), SIPTAG_WWW_AUTHENTICATE_STR(auth_str), TAG_END());
	} else if (regtype == REG_INVITE) {
		nua_respond(nh, SIP_407_PROXY_AUTH_REQUIRED, TAG_IF(nua, NUTAG_WITH_THIS(nua)), SIPTAG_PROXY_AUTHENTICATE_STR(auth_str), TAG_END());
	}

	switch_safe_free(auth_str);
}

uint8_t sofia_reg_handle_register(nua_t * nua, sofia_profile_t *profile, nua_handle_t * nh, sip_t const *sip, sofia_regtype_t regtype, char *key,
								  uint32_t keylen, switch_event_t **v_event)
{
	sip_to_t const *to = NULL;
	sip_expires_t const *expires = NULL;
	sip_authorization_t const *authorization = NULL;
	sip_contact_t const *contact = NULL;
	char *sql;
	switch_event_t *s_event;
	const char *to_user = NULL;
	const char *to_host = NULL;
	char contact_str[1024] = "";
	//char buf[512];
	uint8_t stale = 0, forbidden = 0;
	auth_res_t auth_res;
	long exptime = 60;
	switch_event_t *event;
	const char *rpid = "unknown";
	const char *display = "\"user\"";
	char network_ip[80];
	char *register_gateway = NULL;
	int network_port;
	const char *reg_desc = "Registered";
	const char *call_id = NULL;
	char *force_user;

	/* all callers must confirm that sip, sip->sip_request and sip->sip_contact are not NULL */
	switch_assert(sip != NULL && sip->sip_contact != NULL && sip->sip_request != NULL);

	get_addr(network_ip, sizeof(network_ip), &((struct sockaddr_in *) msg_addrinfo(nua_current_request(nua))->ai_addr)->sin_addr);
	network_port = ntohs(((struct sockaddr_in *) msg_addrinfo(nua_current_request(nua))->ai_addr)->sin_port);
	
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

	if (contact->m_url) {
		const char *port = contact->m_url->url_port;
		display = contact->m_display;

		if (switch_strlen_zero(display)) {
			if (to) {
				display = to->a_display;
				if (switch_strlen_zero(display)) {
					display = "\"user\"";
				}
			}
		}

		if (!port) {
			port = SOFIA_DEFAULT_PORT;
		}

		if (contact->m_url->url_params) {
			switch_snprintf(contact_str, sizeof(contact_str), "%s <sip:%s@%s:%s;%s>",
					 display, contact->m_url->url_user, contact->m_url->url_host, port, contact->m_url->url_params);
		} else {
			switch_snprintf(contact_str, sizeof(contact_str), "%s <sip:%s@%s:%s>", display, contact->m_url->url_user, contact->m_url->url_host, port);
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

	if (regtype == REG_REGISTER && (profile->pflags & PFLAG_BLIND_REG)) {
		goto reg;
	}

	if (authorization) {
		char *v_contact_str;
		if ((auth_res = sofia_reg_parse_auth(profile, authorization, sip, sip->sip_request->rq_method_name, 
											 key, keylen, network_ip, v_event, exptime, regtype, to_user)) == AUTH_STALE) {
			stale = 1;
		}
		
		if (v_event && *v_event) {
			char *exp_var;

			register_gateway = switch_event_get_header(*v_event, "sip-register-gateway");
	
			/* Allow us to force the SIP user to be something specific - needed if 
			 * we - for example - want to be able to ensure that the username a UA can
			 * be contacted at is the same one that they used for authentication.
			 */ 
			if ((force_user = switch_event_get_header(*v_event, "sip-force-user"))) {
				to_user = force_user;
			}
			
			if ((v_contact_str = switch_event_get_header(*v_event, "sip-force-contact"))) {
				if (!strcasecmp(v_contact_str, "nat-connectile-dysfunction") || 
					!strcasecmp(v_contact_str, "NDLB-connectile-dysfunction") || !strcasecmp(v_contact_str, "NDLB-tls-connectile-dysfunction")) {
					if (contact->m_url->url_params) {
						switch_snprintf(contact_str, sizeof(contact_str), "%s <sip:%s@%s:%d;%s>",
								 display, contact->m_url->url_user, network_ip, network_port, contact->m_url->url_params);
					} else {
						switch_snprintf(contact_str, sizeof(contact_str), "%s <sip:%s@%s:%d>", display, contact->m_url->url_user, network_ip, network_port);
					}
					if (strstr(v_contact_str, "tls")) {
						reg_desc = "Registered(TLSHACK)";
					} else {
						reg_desc = "Registered(NATHACK)";
						exptime = 20;
					}
				} else {
					char *p;
					switch_copy_string(contact_str, v_contact_str, sizeof(contact_str));
					for(p = contact_str; p && *p; p++) {
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
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "send %s for [%s@%s]\n", forbidden ? "forbidden" : "challange", to_user, to_host);
			if (auth_res == AUTH_FORBIDDEN) {
				nua_respond(nh, SIP_403_FORBIDDEN, NUTAG_WITH_THIS(nua), TAG_END());
			} else {
				nua_respond(nh, SIP_401_UNAUTHORIZED, NUTAG_WITH_THIS(nua), TAG_END());
			}
			return 1;
		}
	}

	if (!authorization || stale) {
		sofia_reg_auth_challange(nua, profile, nh, regtype, to_host, stale);
		if (regtype == REG_REGISTER && profile->debug) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Requesting Registration from: [%s@%s]\n", to_user, to_host);
		}
		return 1;
	}
  reg:

	if (regtype != REG_REGISTER) {
		return 0;
	}

	call_id = sip->sip_call_id->i_id; //sip_header_as_string(profile->home, (void *) sip->sip_call_id);
	switch_assert(call_id);

	
	if (exptime) {
		const char *agent = "dunno";

		if (sip->sip_user_agent) {
			agent = sip->sip_user_agent->g_string;
		}

		if (sofia_test_pflag(profile, PFLAG_MULTIREG)) {
			sql = switch_mprintf("delete from sip_registrations where call_id='%q'", call_id);
		} else {
			sql = switch_mprintf("delete from sip_registrations where sip_user='%q' and sip_host='%q'", to_user, to_host);
		}
		switch_mutex_lock(profile->ireg_mutex);
		sofia_glue_execute_sql(profile, &sql, SWITCH_TRUE);
		
		sql = switch_mprintf("insert into sip_registrations values ('%q', '%q','%q','%q','%q', '%q', %ld, '%q')", call_id,
							 to_user, to_host, contact_str, reg_desc,
							 rpid, (long) switch_timestamp(NULL) + (long) exptime * 2, agent);

		
		if (sql) {
			sofia_glue_execute_sql(profile, &sql, SWITCH_TRUE);
		}
		switch_mutex_unlock(profile->ireg_mutex);

		if (switch_event_create_subclass(&s_event, SWITCH_EVENT_CUSTOM, MY_EVENT_REGISTER) == SWITCH_STATUS_SUCCESS) {
			switch_event_add_header(s_event, SWITCH_STACK_BOTTOM, "profile-name", "%s", profile->name);
			switch_event_add_header(s_event, SWITCH_STACK_BOTTOM, "from-user", "%s", to_user);
			switch_event_add_header(s_event, SWITCH_STACK_BOTTOM, "from-host", "%s", to_host);
			switch_event_add_header(s_event, SWITCH_STACK_BOTTOM, "contact", "%s", contact_str);
			switch_event_add_header(s_event, SWITCH_STACK_BOTTOM, "call-id", "%s", call_id);
			switch_event_add_header(s_event, SWITCH_STACK_BOTTOM, "rpid", "%s", rpid);
			switch_event_add_header(s_event, SWITCH_STACK_BOTTOM, "expires", "%ld", (long) exptime);
			switch_event_fire(&s_event);
		}

		

		if (profile->debug) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG,
							  "Register:\nFrom:    [%s@%s]\nContact: [%s]\nExpires: [%ld]\n", to_user, to_host, contact_str, (long) exptime);
		}

		if (switch_event_create(&event, SWITCH_EVENT_PRESENCE_IN) == SWITCH_STATUS_SUCCESS) {
			switch_event_add_header(event, SWITCH_STACK_BOTTOM, "proto", "sip");
			switch_event_add_header(event, SWITCH_STACK_BOTTOM, "login", "%s", profile->url);
			switch_event_add_header(event, SWITCH_STACK_BOTTOM, "rpid", "%s", rpid);
			switch_event_add_header(event, SWITCH_STACK_BOTTOM, "from", "%s@%s", to_user, to_host);
			switch_event_add_header(event, SWITCH_STACK_BOTTOM, "status", "Registered");
			switch_event_add_header(event, SWITCH_STACK_BOTTOM, "event_type", "presence");
			switch_event_fire(&event);
		}
	} else {
		if (sofia_test_pflag(profile, PFLAG_MULTIREG)) {
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
			if ((sql = switch_mprintf("delete from sip_subscriptions where sip_user='%q' and sip_host='%q'", to_user, to_host))) {
				sofia_glue_execute_sql(profile, &sql, SWITCH_TRUE);
			}
			
			if ((sql = switch_mprintf("delete from sip_registrations where sip_user='%q' and sip_host='%q'", to_user, to_host))) {
				sofia_glue_execute_sql(profile, &sql, SWITCH_TRUE);
			}
		}
		if (switch_event_create(&event, SWITCH_EVENT_PRESENCE_OUT) == SWITCH_STATUS_SUCCESS) {
			switch_event_add_header(event, SWITCH_STACK_BOTTOM, "proto", "sip");
			switch_event_add_header(event, SWITCH_STACK_BOTTOM, "login", "%s", profile->url);
			switch_event_add_header(event, SWITCH_STACK_BOTTOM, "from", "%s+%s@%s", SOFIA_CHAT_PROTO, to_user, to_host);

			switch_event_add_header(event, SWITCH_STACK_BOTTOM, "status", "unavailable");
			switch_event_add_header(event, SWITCH_STACK_BOTTOM, "rpid", "%s", rpid);
			switch_event_add_header(event, SWITCH_STACK_BOTTOM, "event_type", "presence");
			switch_event_fire(&event);
		}
	}


	if (switch_event_create(&event, SWITCH_EVENT_ROSTER) == SWITCH_STATUS_SUCCESS) {
		switch_event_add_header(event, SWITCH_STACK_BOTTOM, "proto", "sip");
		switch_event_add_header(event, SWITCH_STACK_BOTTOM, "from", "%s@%s", to_user, to_host);
		switch_event_fire(&event);
	}

	
	/*
	if (call_id) {
		su_free(profile->home, call_id);
	}
	*/

	if (regtype == REG_REGISTER) {
		char *new_contact = NULL;
		if (exptime) {
			new_contact = switch_mprintf("%s;expires=%ld", contact_str, (long)exptime);
			nua_respond(nh, SIP_200_OK, SIPTAG_CONTACT_STR(new_contact), NUTAG_WITH_THIS(nua), TAG_END());
			switch_safe_free(new_contact);
			if (switch_event_create(&event, SWITCH_EVENT_MESSAGE_QUERY) == SWITCH_STATUS_SUCCESS) {
				switch_event_add_header(event, SWITCH_STACK_BOTTOM, "Message-Account", "sip:%s@%s", to_user, to_host);
				switch_event_fire(&event);
			}
		} else {
			nua_respond(nh, SIP_200_OK, SIPTAG_CONTACT(contact), NUTAG_WITH_THIS(nua), TAG_END());
		}

		return 1;
	}

	return 0;
}



void sofia_reg_handle_sip_i_register(nua_t * nua, sofia_profile_t *profile, nua_handle_t * nh, sofia_private_t * sofia_private, sip_t const *sip, tagi_t tags[])
{
	char key[128] = "";
	switch_event_t *v_event = NULL;

	if (profile->mflags & MFLAG_REGISTER) {
		nua_respond(nh, SIP_403_FORBIDDEN, NUTAG_WITH_THIS(nua), TAG_END());
		goto end;
	}
	
	if (!sip || !sip->sip_request || !sip->sip_request->rq_method_name) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Received an invalid packet!\n");
		nua_respond(nh, SIP_500_INTERNAL_SERVER_ERROR, TAG_END());
		goto end;
	}

	if (!(sip->sip_contact && sip->sip_contact->m_url)) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "NO CONTACT!\n");
		nua_respond(nh, 400, "Missing Contact Header", TAG_END());
		goto end;
	}

	sofia_reg_handle_register(nua, profile, nh, sip, REG_REGISTER, key, sizeof(key), &v_event);

	if (v_event) {
		switch_event_fire(&v_event);
	}

 end:	

	nua_handle_destroy(nh);

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
									  nua_t * nua, sofia_profile_t *profile, nua_handle_t * nh, 
									  switch_core_session_t *session, sofia_gateway_t *gateway, sip_t const *sip, tagi_t tags[])
{
	sip_www_authenticate_t const *authenticate = NULL;
	char const *realm = NULL;
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

	if (!gateway) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "No Matching gateway found\n");
		goto cancel;
	}

	switch_snprintf(authentication, sizeof(authentication), "%s:%s:%s:%s", scheme, realm, gateway->register_username, gateway->register_password);

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Authenticating '%s' with '%s'.\n", profile->username, authentication);


	ss_state = nua_callstate_authenticating;

	tl_gets(tags, NUTAG_CALLSTATE_REF(ss_state), SIPTAG_WWW_AUTHENTICATE_REF(authenticate), TAG_END());

	nua_authenticate(nh, SIPTAG_EXPIRES_STR(gateway->expires_str), NUTAG_AUTH(authentication), TAG_END());

	return;

 cancel:

	if (session) {
		switch_channel_hangup(switch_core_session_get_channel(session), SWITCH_CAUSE_MANDATORY_IE_MISSING);
	} else {
		nua_cancel(nh, TAG_END());
	}
	
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
	switch_event_add_header_string(params, SWITCH_STACK_BOTTOM, "sip_user_agent", (sip && sip->sip_user_agent) ? sip->sip_user_agent->g_string : "unknown");
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
	
	if (switch_xml_locate_user("id", username, domain_name, ip, &xml, &domain, &user, params) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "can't find user [%s@%s]\n", username, domain_name);
		ret = AUTH_FORBIDDEN;
		goto end;
	}

	if (!(mailbox = (char *)switch_xml_attr(user, "mailbox"))) {
		mailbox = username;
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
			
			if (!strcasecmp(var, "password")) {
				passwd = val;
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
			
			if (!strcasecmp(var, "password")) {
				passwd = val;
			}
			
			if (!strcasecmp(var, "a1-hash")) {
				a1_hash = val;
			}
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

			switch_event_add_header(*v_event, SWITCH_STACK_BOTTOM, "sip_mailbox", "%s", mailbox);
			switch_event_add_header(*v_event, SWITCH_STACK_BOTTOM, "sip_auth_username", "%s", username);
			switch_event_add_header(*v_event, SWITCH_STACK_BOTTOM, "sip_auth_realm", "%s", realm);
			switch_event_add_header(*v_event, SWITCH_STACK_BOTTOM, "mailbox", "%s", mailbox);
			switch_event_add_header(*v_event, SWITCH_STACK_BOTTOM, "user_name", "%s", username);
			switch_event_add_header(*v_event, SWITCH_STACK_BOTTOM, "domain_name", "%s", realm);
			
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
							switch_event_add_header(*v_event, SWITCH_STACK_BOTTOM, var, "%s", val);
						
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
										if ((gateway_ptr = sofia_reg_find_gateway((char *)argv[x]))) {
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


sofia_gateway_t *sofia_reg_find_gateway__(const char *file, const char *func, int line, char *key)
{
	sofia_gateway_t *gateway = NULL;

	switch_mutex_lock(mod_sofia_globals.hash_mutex);
	if ((gateway = (sofia_gateway_t *) switch_core_hash_find(mod_sofia_globals.gateway_hash, key))) {
		if (!(gateway->profile->pflags & PFLAG_RUNNING)) {
			return NULL;
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
 * vim:set softtabstop=4 shiftwidth=4 tabstop=4 expandtab:
 */
