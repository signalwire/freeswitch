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
 *
 *
 * sofia_presence.c -- SOFIA SIP Endpoint (presence code)
 *
 */
#include "mod_sofia.h"

static int sofia_presence_mwi_callback(void *pArg, int argc, char **argv, char **columnNames);
static int sofia_presence_sub_reg_callback(void *pArg, int argc, char **argv, char **columnNames);
static int sofia_presence_resub_callback(void *pArg, int argc, char **argv, char **columnNames);
static int sofia_presence_sub_callback(void *pArg, int argc, char **argv, char **columnNames);

switch_status_t sofia_presence_chat_send(char *proto, char *from, char *to, char *subject, char *body, char *hint)
{
	char buf[256];
	char *user, *host;
	sofia_profile_t *profile = NULL;
	char *ffrom = NULL;
	nua_handle_t *msg_nh;
	char *contact;

	if (!to) {
		return SWITCH_STATUS_SUCCESS;
	}

	user = strdup(to);
	assert(user);

	if ((host = strchr(user, '@'))) {
		*host++ = '\0';
	}

	if (!host || !(profile = sofia_glue_find_profile(host))) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR,
						  "Chat proto [%s]\nfrom [%s]\nto [%s]\n%s\nInvalid Profile %s\n", proto, from, to,
						  body ? body : "[no body]", host ? host : "NULL");
		return SWITCH_STATUS_FALSE;
	}

	if (!sofia_reg_find_reg_url(profile, user, host, buf, sizeof(buf))) {
		return SWITCH_STATUS_FALSE;
	}

	if (!strcmp(proto, SOFIA_CHAT_PROTO)) {
		from = hint;
	} else {
		char *fp, *p, *fu = NULL;
		fp = strdup(from);
		if (!fp) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Memory Error!\n");
			return SWITCH_STATUS_FALSE;
		}

		if ((p = strchr(fp, '@'))) {
			*p = '\0';
			fu = strdup(fp);
			*p = '+';
		}

		ffrom = switch_mprintf("\"%s\" <sip:%s+%s@%s>", fu, proto, fp, profile->name);
		from = ffrom;
		switch_safe_free(fu);
		switch_safe_free(fp);
	}

	contact = sofia_glue_get_url_from_contact(buf, 1);
	msg_nh = nua_handle(profile->nua, NULL, SIPTAG_FROM_STR(from), NUTAG_URL(contact), SIPTAG_TO_STR(buf),	// if this cries, add contact here too, change the 1 to 0 and omit the safe_free
						SIPTAG_CONTACT_STR(profile->url), TAG_END());

	switch_safe_free(contact);

	nua_message(msg_nh, SIPTAG_CONTENT_TYPE_STR("text/html"), SIPTAG_PAYLOAD_STR(body), TAG_END());

	switch_safe_free(ffrom);
	free(user);

	if (profile) {
		switch_thread_rwlock_unlock(profile->rwlock);
	}

	return SWITCH_STATUS_SUCCESS;
}

void sofia_presence_cancel(void)
{
	char *sql;
	sofia_profile_t *profile;
	switch_hash_index_t *hi;
	void *val;

	if ((sql = switch_mprintf("select *,-1,'unavailable','unavailable' from sip_subscriptions where event='presence'"))) {
		switch_mutex_lock(mod_sofia_globals.hash_mutex);
		for (hi = switch_hash_first(NULL, mod_sofia_globals.profile_hash); hi; hi = switch_hash_next(hi)) {
			switch_hash_this(hi, NULL, NULL, &val);
			profile = (sofia_profile_t *) val;
			if (!(profile->pflags & PFLAG_PRESENCE)) {
				continue;
			}

			if (sofia_glue_execute_sql_callback(profile, SWITCH_FALSE, profile->ireg_mutex, sql, sofia_presence_sub_callback, profile) != SWITCH_TRUE) {
				continue;
			}
		}
		switch_safe_free(sql);
		switch_mutex_unlock(mod_sofia_globals.hash_mutex);
	}
}

void sofia_presence_establish_presence(sofia_profile_t *profile)
{

	if (sofia_glue_execute_sql_callback(profile, SWITCH_FALSE, profile->ireg_mutex, 
										"select sip_user,sip_host,'Registered','unknown','' from sip_registrations", 
										sofia_presence_resub_callback, profile) != SWITCH_TRUE) {
		return;
	}

	if (sofia_glue_execute_sql_callback(profile, SWITCH_FALSE, profile->ireg_mutex,
										"select sub_to_user,sub_to_host,'Online','unknown',proto from sip_subscriptions "
										"where proto='ext' or proto='user' or proto='conf'",
										sofia_presence_resub_callback, profile) != SWITCH_TRUE) {
		return;
	}
}

char *sofia_presence_translate_rpid(char *in, char *ext)
{
	char *r = in;

	if (in && (switch_stristr("null", in))) {
		in = NULL;
	}
	
	if (!in) {
		in = ext;
	}

	if (!in) {
		return NULL;
	}

	if (!strcasecmp(in, "dnd") || !strcasecmp(in, "idle")) {
		r = "busy";
	}

	return r;
}

void sofia_presence_mwi_event_handler(switch_event_t *event)
{
	char *account, *dup_account, *yn, *host, *user;
	char *sql;
	sofia_profile_t *profile = NULL;
	switch_stream_handle_t stream = { 0 };
	switch_event_header_t *hp;

	assert(event != NULL);

	if (!(account = switch_event_get_header(event, "mwi-message-account"))) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Missing required Header 'MWI-Message-Account'\n");
		return;
	}

	if (!(yn = switch_event_get_header(event, "mwi-messages-waiting"))) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Missing required Header 'MWI-Messages-Waiting'\n");
		return;
	}

	dup_account = strdup(account);
	assert(dup_account != NULL);
	sofia_glue_get_user_host(dup_account, &user, &host);

	if (!host || !(profile = sofia_glue_find_profile(host))) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Cannot find profile for host %s\n", switch_str_nil(host));
		return;
	}
	
	SWITCH_STANDARD_STREAM(stream);

	for (hp = event->headers; hp; hp = hp->next) {
		if (!strncasecmp(hp->name, "mwi-", 4)) {
			char *tmp = NULL;
			char *value = hp->value;
			if (!strcasecmp(hp->name, "mwi-message-account") && strncasecmp(hp->value, "sip:", 4)) {
				tmp = switch_mprintf("sip:%s", hp->value);
				value = tmp;
			} 
			stream.write_function(&stream, "%s: %s\r\n", hp->name + 4, value);
			switch_safe_free(tmp);
		}
	}

	stream.write_function(&stream, "\r\n");
	
	sql = switch_mprintf("select *,'%q' from sip_subscriptions where event='message-summary' and sub_to_user='%q' and sub_to_host='%q'", 
						 stream.data, user, host);
	
	switch_safe_free(stream.data);

	assert (sql != NULL);
	sofia_glue_execute_sql_callback(profile,
									SWITCH_FALSE,
									profile->ireg_mutex,
									sql,
									sofia_presence_mwi_callback,
									profile);

	switch_safe_free(sql);
	switch_safe_free(dup_account);
	if (profile) {
		sofia_glue_release_profile(profile);
	}
}

void sofia_presence_event_handler(switch_event_t *event)
{
	sofia_profile_t *profile = NULL;
	switch_hash_index_t *hi;
	void *val;
	char *from = switch_event_get_header(event, "from");
	char *proto = switch_event_get_header(event, "proto");
	char *rpid = switch_event_get_header(event, "rpid");
	char *status = switch_event_get_header(event, "status");
	char *event_type = switch_event_get_header(event, "event_type");
	char *sql = NULL;
	char *euser = NULL, *user = NULL, *host = NULL;

	if (rpid && !strcasecmp(rpid, "n/a")) {
		rpid = NULL;
	}

	if (status && !strcasecmp(status, "n/a")) {
		status = NULL;
	}

	if (rpid) {
		rpid = sofia_presence_translate_rpid(rpid, status);
	}

	if (!status) {
		status = "Available";

		if (rpid) {
			if (!strcasecmp(rpid, "busy")) {
				status = "Busy";
			} else if (!strcasecmp(rpid, "unavailable")) {
				status = "Idle";
			} else if (!strcasecmp(rpid, "away")) {
				status = "Idle";
			}
		}
	}

	if (!rpid) {
		rpid = "unknown";
	}

	if (event->event_id == SWITCH_EVENT_ROSTER) {

		if (from) {
			sql = switch_mprintf("select *,1,'%q','%q' from sip_subscriptions where event='presence' and full_from like '%%%q%%'", status, rpid, from);
		} else {
			sql = switch_mprintf("select *,1,'%q','%q' from sip_subscriptions where event='presence'", status, rpid);
		}

		assert(sql != NULL);
		switch_mutex_lock(mod_sofia_globals.hash_mutex);
		for (hi = switch_hash_first(NULL, mod_sofia_globals.profile_hash); hi; hi = switch_hash_next(hi)) {
			switch_hash_this(hi, NULL, NULL, &val);
			profile = (sofia_profile_t *) val;
			if (!(profile->pflags & PFLAG_PRESENCE)) {
				continue;
			}

			sofia_glue_execute_sql_callback(profile,
											SWITCH_FALSE,
											profile->ireg_mutex,
											sql,
											sofia_presence_sub_callback,
											profile);
		}
		switch_mutex_unlock(mod_sofia_globals.hash_mutex);
		free(sql);
		return;
	}

	if (switch_strlen_zero(event_type)) {
		event_type = "presence";
	}

	if ((user = strdup(from))) {
		if ((host = strchr(user, '@'))) {
			char *p;
			*host++ = '\0';
			if ((p = strchr(host, '/'))) {
				*p = '\0';
			}
		} else {
			switch_safe_free(user);
			return;
		}
		if ((euser = strchr(user, '+'))) {
			euser++;
		} else {
			euser = user;
		}
	} else {
		return;
	}

	switch (event->event_id) {
	case SWITCH_EVENT_PRESENCE_PROBE:
		if (proto) {
			char *to = switch_event_get_header(event, "to");
			char *user, *euser, *host, *p;

			if (!to || !(user = strdup(to))) {
				return;
			}

			if ((host = strchr(user, '@'))) {
				*host++ = '\0';
			}
			euser = user;
			if ((p = strchr(euser, '+'))) {
				euser = (p + 1);
			}

			if (euser && host &&
				(sql = switch_mprintf("select sip_user,sip_host,status,rpid,'' from sip_registrations where sip_user='%q' and sip_host='%q'",
									  euser, host)) && (profile = sofia_glue_find_profile(host))) {
				
				sofia_glue_execute_sql_callback(profile,
												SWITCH_FALSE,
												profile->ireg_mutex,
												sql,
												sofia_presence_resub_callback,
												profile);

				sofia_glue_release_profile(profile);
				switch_safe_free(sql);
			}
			switch_safe_free(user);
		}
		return;
	case SWITCH_EVENT_PRESENCE_IN:
		sql =
			switch_mprintf
			("select *,1,'%q','%q' from sip_subscriptions where proto='%q' and event='%q' and sub_to_user='%q' and sub_to_host='%q'",
			 status, rpid, proto, event_type, euser, host);
		break;
	case SWITCH_EVENT_PRESENCE_OUT:
		sql =
			switch_mprintf
			("select *,0,'%q','%q' from sip_subscriptions where proto='%q' and event='%q' and sub_to_user='%q' and sub_to_host='%q'",
			 status, rpid, proto, event_type, euser, host);
		break;
	default:
		break;
	}

	switch_mutex_lock(mod_sofia_globals.hash_mutex);
	for (hi = switch_hash_first(NULL, mod_sofia_globals.profile_hash); hi; hi = switch_hash_next(hi)) {
		switch_hash_this(hi, NULL, NULL, &val);
		profile = (sofia_profile_t *) val;
		if (!(profile->pflags & PFLAG_PRESENCE)) {
			continue;
		}

		if (sql) {
			sofia_glue_execute_sql_callback(profile,
											SWITCH_FALSE,
											profile->ireg_mutex,
											sql,
											sofia_presence_sub_callback,
											profile);
		}
	}
	switch_mutex_unlock(mod_sofia_globals.hash_mutex);

	switch_safe_free(sql);
	switch_safe_free(user);
}

static int sofia_presence_sub_reg_callback(void *pArg, int argc, char **argv, char **columnNames)
{
	sofia_profile_t *profile = (sofia_profile_t *) pArg;
	char *user = argv[1];
	char *host = argv[2];
	switch_event_t *event;
	char *status = NULL;
	char *event_name = argv[5];

	if (!strcasecmp(event_name, "message-summary")) {
		if (switch_event_create(&event, SWITCH_EVENT_MESSAGE_QUERY) == SWITCH_STATUS_SUCCESS) {
			switch_event_add_header(event, SWITCH_STACK_BOTTOM, "Message-Account", "sip:%s@%s", user, host);
			switch_event_fire(&event);
		}
		return 0;
	}

	if (switch_strlen_zero(status)) {
		status = "Available";
	}
	if (switch_event_create(&event, SWITCH_EVENT_PRESENCE_IN) == SWITCH_STATUS_SUCCESS) {
		switch_event_add_header(event, SWITCH_STACK_BOTTOM, "proto", SOFIA_CHAT_PROTO);
		switch_event_add_header(event, SWITCH_STACK_BOTTOM, "login", "%s", profile->url);
		switch_event_add_header(event, SWITCH_STACK_BOTTOM, "from", "%s@%s", user, host);
		switch_event_add_header(event, SWITCH_STACK_BOTTOM, "status", "%s", status);
		switch_event_add_header(event, SWITCH_STACK_BOTTOM, "event_type", "presence");
		switch_event_add_header(event, SWITCH_STACK_BOTTOM, "event_subtype", "probe");
		switch_event_fire(&event);
	}

	return 0;
}

static int sofia_presence_resub_callback(void *pArg, int argc, char **argv, char **columnNames)
{
	sofia_profile_t *profile = (sofia_profile_t *) pArg;
	char *user = argv[0];
	char *host = argv[1];
	char *status = argv[2];
	char *rpid = argv[3];
	char *proto = argv[4];
	switch_event_t *event;

	if (switch_strlen_zero(proto)) {
		proto = NULL;
	}

	if (switch_event_create(&event, SWITCH_EVENT_PRESENCE_IN) == SWITCH_STATUS_SUCCESS) {
		switch_event_add_header(event, SWITCH_STACK_BOTTOM, "proto", "%s", proto ? proto : SOFIA_CHAT_PROTO);
		switch_event_add_header(event, SWITCH_STACK_BOTTOM, "login", "%s", profile->url);
		switch_event_add_header(event, SWITCH_STACK_BOTTOM, "from", "%s@%s", user, host);
		switch_event_add_header(event, SWITCH_STACK_BOTTOM, "status", "%s", status);
		switch_event_add_header(event, SWITCH_STACK_BOTTOM, "rpid", "%s", rpid);
		switch_event_add_header(event, SWITCH_STACK_BOTTOM, "event_type", "presence");
		switch_event_fire(&event);
	}

	return 0;
}

static char *translate_rpid(char *in)
{
	char *r = in;
	
	if (in && (strstr(in, "null") || strstr(in, "NULL"))) {
		in = NULL;
	}

	if (!in || !strcasecmp(in, "unknown")) {
		r = "online";
		goto end;
	}

	if (!strcasecmp(in, "busy")) {
		r = in;
		goto end;
	}

	if (!strcasecmp(in, "unavailable")) {
		r = "away";
		goto end;
	}

	if (!strcasecmp(in, "idle")) {
		r = "busy";
	}

 end:
	return r;
}


static int sofia_presence_sub_callback(void *pArg, int argc, char **argv, char **columnNames)
{
	sofia_profile_t *profile = (sofia_profile_t *) pArg;
	char *pl;
	char *id, *note;
	uint32_t in = atoi(argv[11]);
	char *status = argv[12];
	char *rpid = argv[13];

	char *proto = argv[0];
	char *user = argv[1];
	char *host = argv[2];
	char *sub_to_user = argv[3];
	char *sub_to_host = argv[4];
	char *event = argv[5];
	char *call_id = argv[7];
	nua_handle_t *nh;
	char *to;
	char *open;
	char *prpid;
	int done = 0;

	if (!(nh = (nua_handle_t *) switch_core_hash_find(profile->sub_hash, call_id))) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Cannot find handle for %s\n", call_id);
		return 0;
	}

	if (!rpid) {
		rpid = "unknown";
	}

	prpid = translate_rpid(rpid);

	if (in < 0) {
		done = 1;
		in = 0;
	} 

	if (in) {
		note = switch_mprintf("<dm:note>%s</dm:note>", status);
		open = "open";
	} else {
		note = NULL;
		open = "closed";
	}

	if (!strcasecmp(sub_to_host, host)) {
		/* same host */
		id = switch_mprintf("sip:%s+%s@%s", proto, sub_to_user, sub_to_host);
	} else if (strcasecmp(proto, SOFIA_CHAT_PROTO)) {
		/*encapsulate */
		id = switch_mprintf("sip:%s+%s+%s@%s", proto, sub_to_user, sub_to_host, host);
	} else {
		id = switch_mprintf("sip:%s@%s", sub_to_user, sub_to_host);
	}

	to = switch_mprintf("sip:%s@%s", user, host);
	pl = switch_mprintf("<?xml version='1.0' encoding='UTF-8'?>\r\n"
						"<presence xmlns='urn:ietf:params:xml:ns:pidf'\r\n"
						"xmlns:dm='urn:ietf:params:xml:ns:pidf:data-model'\r\n"
						"xmlns:rpid='urn:ietf:params:xml:ns:pidf:rpid'\r\n"
						"xmlns:c='urn:ietf:params:xml:ns:pidf:cipid'\r\n"
						"entity='pres:%s'>\r\n"
						"<presentity uri=\"%s;method=SUBSCRIBE\"/>\r\n"
						"<atom id=\"1002\">\r\n"
						"<address uri=\"%s\" priority=\"0.800000\">\r\n"
						"<status status=\"%s\">\r\n"
						"<note>%s</note>\r\n"
						"</status>\r\n"
						"<msnsubstatus substatus=\"%s\"/>\r\n"
						"</address>\r\n"
						"</atom>\r\n"
						"<tuple id='t6a5ed77e'>\r\n"
						"<status>\r\n"
						"<basic>%s</basic>\r\n"
						"</status>\r\n"
						"</tuple>\r\n"
						"<dm:person id='p06360c4a'>\r\n"
						"<rpid:activities>\r\n" "<rpid:%s/>\r\n" 
						"</rpid:activities>%s</dm:person>\r\n" 
						"</presence>", id, 
						id, profile->url, open, status, prpid,
						open, rpid, note);

	nua_notify(nh,
			   SIPTAG_SUBSCRIPTION_STATE_STR("active;expires=3600"),
			   SIPTAG_EVENT_STR(event), SIPTAG_CONTENT_TYPE_STR("application/pidf+xml"), SIPTAG_PAYLOAD_STR(pl), TAG_END());

	if (done) {
		switch_core_hash_delete(profile->sub_hash, call_id);
	}

	switch_safe_free(id);
	switch_safe_free(note);
	switch_safe_free(pl);
	switch_safe_free(to);

	return 0;
}

static int sofia_presence_mwi_callback(void *pArg, int argc, char **argv, char **columnNames)
{
	//char *proto = argv[0];
	//char *user = argv[1];
	//char *host = argv[2];
	char *sub_to_user = argv[3];
	char *sub_to_host = argv[4];
	char *event = argv[5];
	//char *contact = argv[6];
	char *call_id = argv[7];
	//char *full_from = argv[8];
	//char *full_via = argv[9];
	char *expires = argv[10];
	char *body = argv[11];
	char *exp;
	sofia_profile_t *profile = NULL;
	char *id = NULL;
	nua_handle_t *nh;
	int expire_sec = atoi(expires);

	if (!(profile = sofia_glue_find_profile(sub_to_host))) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Cannot find profile for host %s\n", sub_to_host);
		return 0;
	}
	
	if (!(nh = (nua_handle_t *) switch_core_hash_find(profile->sub_hash, call_id))) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Cannot find handle for %s\n", call_id);
		return 0;
	}

	id = switch_mprintf("sip:%s@%s", sub_to_user, sub_to_host);
	expire_sec = (int)(expire_sec - time(NULL));
	if (expire_sec < 0) {
		expire_sec = 3600;
	}
	exp = switch_mprintf("active;expires=%ld", expire_sec);

	nua_notify(nh,
			   SIPTAG_SUBSCRIPTION_STATE_STR(exp),
			   SIPTAG_EVENT_STR(event), SIPTAG_CONTENT_TYPE_STR("application/simple-message-summary"), SIPTAG_PAYLOAD_STR(body), TAG_END());

	switch_safe_free(id);
	switch_safe_free(exp);

	sofia_glue_release_profile(profile);

	return 0;
}

void sofia_presence_handle_sip_i_subscribe(int status,
							char const *phrase,
							nua_t *nua, sofia_profile_t *profile, nua_handle_t *nh, sofia_private_t *sofia_private, sip_t const *sip, tagi_t tags[])
{
	if (sip) {
		long exp, exp_raw;
		sip_to_t const *to = sip->sip_to;
		sip_from_t const *from = sip->sip_from;
		sip_contact_t const *contact = sip->sip_contact;
		const char *from_user = NULL, *from_host = NULL;
		const char *to_user = NULL, *to_host = NULL;
		char *my_to_user = NULL;
		char *sql, *event = NULL;
		char *proto = "sip";
		char *d_user = NULL;
		char *contact_str = "";
		const char *call_id = NULL;
		char *to_str = NULL;
		char *full_from = NULL;
		char *full_via = NULL;
		char *sstr;
		const char *display = "\"user\"";
		switch_event_t *sevent;
		int sub_state;

		tl_gets(tags,
				NUTAG_SUBSTATE_REF(sub_state), TAG_END());

		if (contact) {
			char *port = (char *) contact->m_url->url_port;

			display = contact->m_display;

			if (switch_strlen_zero(display)) {
				if (from) {
					display = from->a_display;
					if (switch_strlen_zero(display)) {
						display = "\"user\"";
					}
				}
			} else {
				display = "\"user\"";
			}

			if (!port) {
				port = SOFIA_DEFAULT_PORT;
			}

			if (contact->m_url->url_params) {
				contact_str = switch_mprintf("%s <sip:%s@%s:%s;%s>",
											 display, contact->m_url->url_user, contact->m_url->url_host, port, contact->m_url->url_params);
			} else {
				contact_str = switch_mprintf("%s <sip:%s@%s:%s>", display, contact->m_url->url_user, contact->m_url->url_host, port);
			}
		}

		if (to) {
			to_str = switch_mprintf("sip:%s@%s", to->a_url->url_user, to->a_url->url_host);	//, to->a_url->url_port);
		}

		if (to) {
			to_user = to->a_url->url_user;
			to_host = to->a_url->url_host;
		}

		if (sip && sip->sip_from) {
			from_user = sip->sip_from->a_url->url_user;
			from_host = sip->sip_from->a_url->url_host;
		} else {
			from_user = "n/a";
			from_host = "n/a";
		}

		if (strstr(to_user, "ext+") || strstr(to_user, "user+")) {
			char proto[80];
			char *p;

			switch_copy_string(proto, to_user, sizeof(proto));
			if ((p = strchr(proto, '+'))) {
				*p = '\0';
			}
			
			if (switch_event_create(&sevent, SWITCH_EVENT_PRESENCE_IN) == SWITCH_STATUS_SUCCESS) {
				switch_event_add_header(sevent, SWITCH_STACK_BOTTOM, "proto", proto);
				switch_event_add_header(sevent, SWITCH_STACK_BOTTOM, "login", "%s", profile->name);
				switch_event_add_header(sevent, SWITCH_STACK_BOTTOM, "from", "%s@%s", to_user, to_host);
				switch_event_add_header(sevent, SWITCH_STACK_BOTTOM, "rpid", "active");
				switch_event_add_header(sevent, SWITCH_STACK_BOTTOM, "status", "Click To Call");
				switch_event_fire(&sevent);
			}
		
		} else {
			if (switch_event_create(&sevent, SWITCH_EVENT_PRESENCE_PROBE) == SWITCH_STATUS_SUCCESS) {
				switch_event_add_header(sevent, SWITCH_STACK_BOTTOM, "proto", SOFIA_CHAT_PROTO);
				switch_event_add_header(sevent, SWITCH_STACK_BOTTOM, "login", "%s", profile->name);
				switch_event_add_header(sevent, SWITCH_STACK_BOTTOM, "from", "%s@%s", from_user, from_host);
				switch_event_add_header(sevent, SWITCH_STACK_BOTTOM, "to", "%s@%s", to_user, to_host);
				
				switch_event_fire(&sevent);
			}
		}

		if (strchr(to_user, '+')) {
			char *h;
			if ((proto = (d_user = strdup(to_user)))) {
				if ((my_to_user = strchr(d_user, '+'))) {
					*my_to_user++ = '\0';
					to_user = my_to_user;
					if ((h = strchr(to_user, '+')) || (h = strchr(to_user, '@'))) {
						*h++ = '\0';
						to_host = h;
					}
				}
			}

			if (!(proto && to_user && to_host)) {
				nua_respond(nh, SIP_404_NOT_FOUND, NUTAG_WITH_THIS(nua), TAG_END());
				goto end;
			}
		}

		call_id = sip->sip_call_id->i_id;
		event = sip_header_as_string(profile->home, (void *) sip->sip_event);
		full_from = sip_header_as_string(profile->home, (void *) sip->sip_from);
		full_via = sip_header_as_string(profile->home, (void *) sip->sip_via);

		exp_raw = (sip->sip_expires ? sip->sip_expires->ex_delta : 3600);
		exp = (long) time(NULL) + exp_raw;

		switch_mutex_lock(profile->ireg_mutex);

		sql = switch_mprintf("delete from sip_subscriptions where "
							 "proto='%q' and user='%q' and host='%q' and sub_to_user='%q' and sub_to_host='%q' and event='%q'",
							 proto,
							 from_user,
							 from_host,
							 to_user,
							 to_host, event
							 );

		assert(sql != NULL);
		sofia_glue_execute_sql(profile, SWITCH_FALSE, sql, NULL);
		free(sql);

		if (sub_state == nua_substate_terminated) {
			sstr = switch_mprintf("terminated");
			switch_core_hash_delete(profile->sub_hash, call_id);
		} else {
			sql = switch_mprintf("insert into sip_subscriptions values ('%q','%q','%q','%q','%q','%q','%q','%q','%q','%q',%ld)",
								 proto, from_user, from_host, to_user, to_host, event, contact_str, call_id, full_from, full_via, exp);
			
			assert(sql != NULL);
			sofia_glue_execute_sql(profile, SWITCH_FALSE, sql, NULL);
			free(sql);

			switch_mutex_unlock(profile->ireg_mutex);
			sstr = switch_mprintf("active;expires=%ld", exp_raw);
			switch_core_hash_insert(profile->sub_hash, call_id, nh);
		}

		nua_respond(nh, SIP_202_ACCEPTED,
					NUTAG_WITH_THIS(nua),
					SIPTAG_SUBSCRIPTION_STATE_STR(sstr), 
					SIPTAG_FROM(sip->sip_to),
					SIPTAG_TO(sip->sip_from),
					SIPTAG_CONTACT_STR(contact_str),
					TAG_END());

		nua_notify(nh, SIPTAG_EVENT_STR(event), TAG_END());

		switch_safe_free(sstr);

		if ((sql = switch_mprintf("select * from sip_subscriptions where sip_user='%q' and sip_host='%q'", to_user, to_host))) {
			sofia_glue_execute_sql_callback(profile,
											SWITCH_FALSE,
											profile->ireg_mutex,
											sql,
											sofia_presence_sub_reg_callback,
											profile);

			switch_safe_free(sql);
		}
	end:

		if (event) {
			su_free(profile->home, event);
		}

		if (full_from) {
			su_free(profile->home, full_from);
		}
		if (full_via) {
			su_free(profile->home, full_via);
		}

		switch_safe_free(d_user);
		switch_safe_free(to_str);
		switch_safe_free(contact_str);
	}
}

void sofia_presence_handle_sip_r_subscribe(int status,
							char const *phrase,
							nua_t *nua, sofia_profile_t *profile, nua_handle_t *nh, sofia_private_t *sofia_private, sip_t const *sip, tagi_t tags[])
{

}

void sofia_presence_handle_sip_i_publish(nua_t *nua, sofia_profile_t *profile, nua_handle_t *nh, sofia_private_t *sofia_private, sip_t const *sip, tagi_t tags[])
{
	if (sip) {
		sip_from_t const *from = sip->sip_from;
		char *from_user = NULL;
		char *from_host = NULL;
		char *rpid = "unknown";
		sip_payload_t *payload = sip->sip_payload;
		char *event_type;

		if (from) {
			from_user = (char *) from->a_url->url_user;
			from_host = (char *) from->a_url->url_host;
		}

		if (payload) {
			switch_xml_t xml, note, person, tuple, status, basic, act;
			switch_event_t *event;
			uint8_t in = 0;
			char *sql;

			if ((xml = switch_xml_parse_str(payload->pl_data, strlen(payload->pl_data)))) {
				char *status_txt = "", *note_txt = "";

				if ((tuple = switch_xml_child(xml, "tuple")) && (status = switch_xml_child(tuple, "status"))
					&& (basic = switch_xml_child(status, "basic"))) {
					status_txt = basic->txt;
				}

				if ((person = switch_xml_child(xml, "dm:person")) && (note = switch_xml_child(person, "dm:note"))) {
					note_txt = note->txt;
				}

				if (person && (act = switch_xml_child(person, "rpid:activities"))) {
					if ((rpid = strchr(act->child->name, ':'))) {
						rpid++;
					} else {
						rpid = act->child->name;
					}
				}

				if (!strcasecmp(status_txt, "open")) {
					if (switch_strlen_zero(note_txt)) {
						note_txt = "Available";
					}
					in = 1;
				} else if (!strcasecmp(status_txt, "closed")) {
					if (switch_strlen_zero(note_txt)) {
						note_txt = "Unavailable";
					}
				}

				if ((sql =
					 switch_mprintf("update sip_registrations set status='%q',rpid='%q' where sip_user='%q' and sip_host='%q'",
									note_txt, rpid, from_user, from_host))) {
					sofia_glue_execute_sql(profile, SWITCH_FALSE, sql, profile->ireg_mutex);
					switch_safe_free(sql);
				}

				event_type = sip_header_as_string(profile->home, (void *) sip->sip_event);

				if (in) {
					if (switch_event_create(&event, SWITCH_EVENT_PRESENCE_IN) == SWITCH_STATUS_SUCCESS) {
						switch_event_add_header(event, SWITCH_STACK_BOTTOM, "proto", SOFIA_CHAT_PROTO);
						switch_event_add_header(event, SWITCH_STACK_BOTTOM, "rpid", "%s", rpid);
						switch_event_add_header(event, SWITCH_STACK_BOTTOM, "login", "%s", profile->url);
						switch_event_add_header(event, SWITCH_STACK_BOTTOM, "from", "%s@%s", from_user, from_host);

						switch_event_add_header(event, SWITCH_STACK_BOTTOM, "status", "%s", note_txt);
						switch_event_add_header(event, SWITCH_STACK_BOTTOM, "event_type", "%s", event_type);
						switch_event_fire(&event);
					}
				} else {
					if (switch_event_create(&event, SWITCH_EVENT_PRESENCE_OUT) == SWITCH_STATUS_SUCCESS) {
						switch_event_add_header(event, SWITCH_STACK_BOTTOM, "proto", SOFIA_CHAT_PROTO);
						switch_event_add_header(event, SWITCH_STACK_BOTTOM, "rpid", "%s", rpid);
						switch_event_add_header(event, SWITCH_STACK_BOTTOM, "login", "%s", profile->url);
						switch_event_add_header(event, SWITCH_STACK_BOTTOM, "from", "%s@%s", from_user, from_host);

						switch_event_add_header(event, SWITCH_STACK_BOTTOM, "event_type", "%s", event_type);
						switch_event_fire(&event);
					}
				}

				if (event_type) {
					su_free(profile->home, event_type);
				}
				switch_xml_free(xml);
			}
		}
	}
	nua_respond(nh, SIP_200_OK, NUTAG_WITH_THIS(nua), TAG_END());
}

void sofia_presence_set_hash_key(char *hash_key, int32_t len, sip_t const *sip)
{
	url_t *to = sip->sip_to->a_url;
	url_t *from = sip->sip_from->a_url;
	snprintf(hash_key, len, "%s%s%s", from->url_user, from->url_host, to->url_user);
}

void sofia_presence_handle_sip_i_message(int status,
						  char const *phrase,
						  nua_t *nua, sofia_profile_t *profile, nua_handle_t *nh, sofia_private_t *sofia_private, sip_t const *sip, tagi_t tags[])
{
	if (sip) {
		sip_from_t const *from = sip->sip_from;
		const char *from_user = NULL;
		const char *from_host = NULL;
		sip_to_t const *to = sip->sip_to;
		const char *to_user = NULL;
		const char *to_host = NULL;
		sip_subject_t const *sip_subject = sip->sip_subject;
		sip_payload_t *payload = sip->sip_payload;
		const char *subject = "n/a";
		char *msg = NULL;

		if (sip->sip_content_type) {
			if (strstr(sip->sip_content_type->c_subtype, "composing")) {
				return;
			}
		}

		if (from) {
			from_user = from->a_url->url_user;
			from_host = from->a_url->url_host;
		}

		if (to) {
			to_user = to->a_url->url_user;
			to_host = to->a_url->url_host;
		}

		if (!to_user) {
			return;
		}

		if (payload) {
			msg = payload->pl_data;
		}

		if (sip_subject) {
			subject = sip_subject->g_value;
		}

		if (nh) {
			char hash_key[512];
			private_object_t *tech_pvt;
			switch_channel_t *channel;
			switch_event_t *event;
			char *to_addr;
			char *from_addr;
			char *p;
			char *full_from;
			char proto[512] = SOFIA_CHAT_PROTO;

			full_from = sip_header_as_string(profile->home, (void *) sip->sip_from);

			if ((p = strchr(to_user, '+'))) {
				switch_copy_string(proto, to_user, sizeof(proto));
				p = strchr(proto, '+');
				*p++ = '\0';

				if ((to_addr = strdup(p))) {
					if ((p = strchr(to_addr, '+'))) {
						*p = '@';
					}
				}
			} else {
				to_addr = switch_mprintf("%s@%s", to_user, to_host);
			}

			from_addr = switch_mprintf("%s@%s", from_user, from_host);

			sofia_presence_set_hash_key(hash_key, sizeof(hash_key), sip);
			if ((tech_pvt = (private_object_t *) switch_core_hash_find(profile->chat_hash, hash_key))) {
				channel = switch_core_session_get_channel(tech_pvt->session);
				if (switch_event_create(&event, SWITCH_EVENT_MESSAGE) == SWITCH_STATUS_SUCCESS) {
					switch_event_add_header(event, SWITCH_STACK_BOTTOM, "proto", SOFIA_CHAT_PROTO);
					switch_event_add_header(event, SWITCH_STACK_BOTTOM, "login", "%s", profile->url);
					switch_event_add_header(event, SWITCH_STACK_BOTTOM, "from", "%s", tech_pvt->hash_key);
					switch_event_add_header(event, SWITCH_STACK_BOTTOM, "to", "%s", to_addr);
					switch_event_add_header(event, SWITCH_STACK_BOTTOM, "subject", "SIMPLE MESSAGE");
					if (msg) {
						switch_event_add_body(event, "%s", msg);
					}
					if (switch_core_session_queue_event(tech_pvt->session, &event) != SWITCH_STATUS_SUCCESS) {
						switch_event_add_header(event, SWITCH_STACK_BOTTOM, "delivery-failure", "true");
						switch_event_fire(&event);
					}
				}
			} else {
				switch_chat_interface_t *ci;

				if ((ci = switch_loadable_module_get_chat_interface(proto))) {
					ci->chat_send(SOFIA_CHAT_PROTO, from_addr, to_addr, "", msg, full_from);
				} else {
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Invalid Chat Interface [%s]!\n", proto);
				}
			}
			switch_safe_free(to_addr);
			switch_safe_free(from_addr);
			if (full_from) {
				su_free(profile->home, full_from);
			}
		}
	}
}

void sofia_presence_set_chat_hash(private_object_t *tech_pvt, sip_t const *sip)
{
	char hash_key[256] = "";
	char buf[512];
	su_home_t *home = NULL;

	if (tech_pvt->hash_key || !sip || !sip->sip_from || !sip->sip_from->a_url || !sip->sip_from->a_url->url_user || !sip->sip_from->a_url->url_host) {
		return;
	}

	if (sofia_reg_find_reg_url(tech_pvt->profile, sip->sip_from->a_url->url_user, sip->sip_from->a_url->url_host, buf, sizeof(buf))) {
		home = su_home_new(sizeof(*home));
		assert(home != NULL);		
		tech_pvt->chat_from = sip_header_as_string(home, (const sip_header_t *) sip->sip_to);
		tech_pvt->chat_to = switch_core_session_strdup(tech_pvt->session, buf);
		sofia_presence_set_hash_key(hash_key, sizeof(hash_key), sip);
		su_home_unref(home);
		home = NULL;
	} else {
		return;
	}

	tech_pvt->hash_key = switch_core_session_strdup(tech_pvt->session, hash_key);
	switch_core_hash_insert(tech_pvt->profile->chat_hash, tech_pvt->hash_key, tech_pvt);

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
