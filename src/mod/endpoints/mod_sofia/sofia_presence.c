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
 * Contributor(s):
 *
 * Anthony Minessale II <anthm@freeswitch.org>
 * Ken Rice <krice@freeswitch.org>
 * Paul D. Tinsley <pdt at jackhammer.org>
 * Bret McDanel <trixter AT 0xdecafbad.com>
 * Raymond Chandler <intralanman@freeswitch.org>
 * William King <william.king@quentustech.com>
 * Emmanuel Schmidbauer <e.schmidbauer@gmail.com>
 * David Knell <david.knell@telng.com>
 *
 * sofia_presence.c -- SOFIA SIP Endpoint (presence code)
 *
 */
#include "mod_sofia.h"
#include "switch_stun.h"

#define SUB_OVERLAP 300
struct state_helper {
	switch_hash_t *hash;
	sofia_profile_t *profile;
	switch_memory_pool_t *pool;
	int total;
};


static int sofia_presence_mwi_callback(void *pArg, int argc, char **argv, char **columnNames);
static int sofia_presence_mwi_callback2(void *pArg, int argc, char **argv, char **columnNames);
static int sofia_presence_sub_reg_callback(void *pArg, int argc, char **argv, char **columnNames);
static int sofia_presence_resub_callback(void *pArg, int argc, char **argv, char **columnNames);
static int sofia_presence_sub_callback(void *pArg, int argc, char **argv, char **columnNames);
static int broadsoft_sla_gather_state_callback(void *pArg, int argc, char **argv, char **columnNames);
static int broadsoft_sla_notify_callback(void *pArg, int argc, char **argv, char **columnNames);
static int sync_sla(sofia_profile_t *profile, const char *to_user, const char *to_host, switch_bool_t clear, switch_bool_t unseize, const char *call_id);
static int sofia_dialog_probe_callback(void *pArg, int argc, char **argv, char **columnNames);
static int sofia_dialog_probe_notify_callback(void *pArg, int argc, char **argv, char **columnNames);

struct pres_sql_cb {
	sofia_profile_t *profile;
	int ttl;
};

static int sofia_presence_send_sql(void *pArg, int argc, char **argv, char **columnNames);

struct dialog_helper {
	char state[128];
	char status[512];
	char rpid[512];
	char presence_id[1024];
	int hits;
};

struct resub_helper {
	sofia_profile_t *profile;
	switch_event_t *event;
	int rowcount;
	int noreg;
};

struct rfc4235_helper {
	switch_hash_t *hash;
	sofia_profile_t *profile;
	switch_memory_pool_t *pool;
	switch_event_t *event;
	int rowcount;
};

struct presence_helper {
	sofia_profile_t *profile;
	switch_event_t *event;
	switch_stream_handle_t stream;
	char last_uuid[512];
	int hup;
	int calls_up;

};

switch_status_t sofia_presence_chat_send(switch_event_t *message_event)

{
	char *prof = NULL, *user = NULL, *host = NULL;
	sofia_profile_t *profile = NULL;
	char *ffrom = NULL;
	nua_handle_t *msg_nh;
	char *contact = NULL;
	char *dup = NULL;
	switch_status_t status = SWITCH_STATUS_FALSE;
	const char *ct = "text/html";
	sofia_destination_t *dst = NULL;
	char *to_uri = NULL;
	switch_console_callback_match_t *list = NULL;
	switch_console_callback_match_node_t *m;
	char *remote_ip = NULL;
	char *user_via = NULL;
	//char *contact_str = NULL;
	char *dup_dest = NULL;
	char *p = NULL;
	char *remote_host = NULL;
	const char *proto;
	const char *from;
	const char *to;
	//const char *subject;
	const char *body;
	const char *type;
	const char *from_full;
	char header[256] = "";
	char *route_uri = NULL;
	const char *network_ip = NULL, *network_port = NULL, *from_proto;
	char *extra_headers = NULL;
	char uuid_str[SWITCH_UUID_FORMATTED_LENGTH + 1];
	int mstatus = 0, sanity = 0;
	const char *blocking;
	int is_blocking = 0;

	proto = switch_event_get_header(message_event, "proto");
	from_proto = switch_event_get_header(message_event, "from_proto");
	from = switch_event_get_header(message_event, "from");
	to = switch_event_get_header(message_event, "to");
	//subject = switch_event_get_header(message_event, "subject");
	body = switch_event_get_body(message_event);
	type = switch_event_get_header(message_event, "type");
	from_full = switch_event_get_header(message_event, "from_full");
	blocking = switch_event_get_header(message_event, "blocking");
	is_blocking = switch_true(blocking);

	network_ip = switch_event_get_header(message_event, "to_sip_ip");
	network_port = switch_event_get_header(message_event, "to_sip_port");

	extra_headers = sofia_glue_get_extra_headers_from_event(message_event, SOFIA_SIP_HEADER_PREFIX);

	if (!to) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Missing To: header.\n");
		goto end;
	}

	if (!from) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Missing From: header.\n");
		goto end;
	}

	if (!zstr(type)) {
		ct = type;
	}

	dup = strdup(to);
	switch_assert(dup);
	prof = dup;

	/* Do we have a user of the form profile/user[@host]? */
	if ((user = strchr(prof, '/'))) {
		*user++ = '\0';
	} else {
		user = prof;
		prof = NULL;
	}

	if (!prof) {
		prof = switch_event_get_header(message_event, "sip_profile");
	}

	if (!strncasecmp(user, "sip:", 4)) {
		to_uri = user;
	}

	if ((host = strchr(user, '@'))) {
		if (!to_uri) {
			*host++ = '\0';
		} else {
			host++;
		}
		if (!prof) {
			prof = host;
		}
	}

	if (!prof || !(profile = sofia_glue_find_profile(prof))) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR,
		"Chat proto [%s]\nfrom [%s]\nto [%s]\n%s\nInvalid Profile %s\n", proto, from, to,
						  body ? body : "[no body]", prof ? prof : "NULL");
		goto end;
	}

	if (zstr(host)) {
		host = profile->domain_name;
		if (zstr(host)) {
			host = prof;
		}
	}


	if (to_uri) {
		switch_console_push_match(&list, to_uri);
	}  else if (!(list = sofia_reg_find_reg_url_multi(profile, user, host))) {
		sofia_profile_t *test;

		if ((test = sofia_glue_find_profile(host))) {
			sofia_glue_release_profile(test);
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "Not sending to local box for %s@%s\n", user, host);
			/* our box let's not send it */
		} else {
			char *tmp;
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Can't find registered user %s@%s\n", user, host);
			tmp = switch_mprintf("sip:%s@%s", user, host);
			switch_console_push_match(&list, tmp);
			free(tmp);
		}

	}

	if (!strcasecmp(proto, SOFIA_CHAT_PROTO)) {
		from = from_full;
	} else {
		char *fp, *p = NULL;


		fp = strdup(from);
		switch_assert(fp);


		if ((p = strchr(fp, '@'))) {
			*p++ = '\0';
		}

		if (zstr(p)) {
			p = profile->domain_name;
			if (zstr(p)) {
				p = host;
			}
		}

		if (switch_stristr("global", proto)) {
			if (!from_proto || !strcasecmp(from_proto, SOFIA_CHAT_PROTO)) {
				ffrom = switch_mprintf("\"%s\" <sip:%s@%s>", fp, fp, p);
			} else {
				ffrom = switch_mprintf("\"%s\" <sip:%s+%s@%s>", fp, from_proto, fp, p);
			}

		} else {
			ffrom = switch_mprintf("\"%s\" <sip:%s+%s@%s>", fp, from_proto ? from_proto : proto, fp, p);
		}

		from = ffrom;
		switch_safe_free(fp);
	}

	if (!list) {
		switch_event_t *event;

		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR,
		"Chat proto [%s]\nfrom [%s]\nto [%s]\n%s\nNobody to send to: Profile %s\n", proto, from, to,
						  body ? body : "[no body]", prof ? prof : "NULL");
		// emit no recipient event
		if (switch_event_create_subclass(&event, SWITCH_EVENT_CUSTOM, MY_EVENT_ERROR) == SWITCH_STATUS_SUCCESS) {
			switch_event_add_header(event, SWITCH_STACK_BOTTOM, "Error-Type", "chat");
			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "Error-Reason", "no recipient");
			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "Chat-Send-To", to);
			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "Chat-Send-From", from);
			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "Chat-Send-Profile", prof ? prof : "NULL");
			switch_event_add_body(event, "%s", body);
			switch_event_fire(&event);
		}

		goto end;
	}

	for (m = list->head; m; m = m->next) {

		if (!(dst = sofia_glue_get_destination(m->val))) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Memory Error!\n");
			break;
		}

		/* sofia_glue is running sofia_overcome_sip_uri_weakness we do not, not sure if it matters */

		if (dst->route_uri) {
			dup_dest = strdup(dst->route_uri);
		} else  {
			dup_dest = strdup(dst->to);
		}


		remote_host = strdup(dup_dest);
		if (!zstr(remote_host)) {
			switch_split_user_domain(remote_host, NULL, &remote_ip);
		}

		if (!zstr(remote_ip) && sofia_glue_check_nat(profile, remote_ip)) {
			char *ptr = NULL;
			if ((ptr = sofia_glue_find_parameter(dst->contact, "transport="))) {
				sofia_transport_t transport = sofia_glue_str2transport( ptr + 10 );
				user_via = sofia_glue_create_external_via(NULL, profile, transport);
			} else {
				user_via = sofia_glue_create_external_via(NULL, profile, SOFIA_TRANSPORT_UDP);
			}
		}

		status = SWITCH_STATUS_SUCCESS;

		if (dup_dest && (p = strstr(dup_dest, ";fs_"))) {
			*p = '\0';
		}

		/* if this cries, add contact here too, change the 1 to 0 and omit the safe_free */

		//printf("DEBUG To: [%s] From: [%s] Contact: [%s] RURI [%s] ip [%s] port [%s]\n", to, from, contact, dst->route_uri, network_ip, network_port);

		//DUMP_EVENT(message_event);

		if (zstr(dst->route_uri) && !zstr(user) && !zstr(network_ip) && (zstr(host) || strcmp(network_ip, host))) {
			route_uri = switch_mprintf("sip:%s@%s:%s", user, network_ip, network_port);
		}

		msg_nh = nua_handle(profile->nua, NULL,
							TAG_END());

		nua_handle_bind(msg_nh, &mod_sofia_globals.destroy_private);

		switch_snprintf(header, sizeof(header), "X-FS-Sending-Message: %s", switch_core_get_uuid());

		switch_uuid_str(uuid_str, sizeof(uuid_str));

		if (is_blocking) {
			switch_mutex_lock(profile->flag_mutex);
			switch_core_hash_insert(profile->chat_hash, uuid_str, &mstatus);
			switch_mutex_unlock(profile->flag_mutex);
		}

		nua_message(msg_nh,
					TAG_IF(dst->route_uri, NUTAG_PROXY(dst->route_uri)),
					TAG_IF(route_uri, NUTAG_PROXY(route_uri)),
					TAG_IF(dst->route, SIPTAG_ROUTE_STR(dst->route)),
					SIPTAG_FROM_STR(from),
					TAG_IF(contact, NUTAG_URL(contact)),
					SIPTAG_TO_STR(dup_dest),
					SIPTAG_CALL_ID_STR(uuid_str),
					TAG_IF(user_via, SIPTAG_VIA_STR(user_via)),
					SIPTAG_CONTENT_TYPE_STR(ct),
					SIPTAG_PAYLOAD_STR(body),
					SIPTAG_HEADER_STR(header),
					TAG_IF(!zstr(extra_headers), SIPTAG_HEADER_STR(extra_headers)),
					TAG_END());


		if (is_blocking) {
			sanity = 200;

			while(!mstatus && --sanity && !msg_nh->nh_destroyed) {
				switch_yield(100000);
			}

			if (!(mstatus > 199 && mstatus < 300)) {
				status = SWITCH_STATUS_FALSE;
			}

			switch_event_add_header(message_event, SWITCH_STACK_BOTTOM, "Delivery-Result-Code", "%d", mstatus);

			switch_mutex_lock(profile->flag_mutex);
			switch_core_hash_delete(profile->chat_hash, uuid_str);
			switch_mutex_unlock(profile->flag_mutex);
		}

		sofia_glue_free_destination(dst);
		switch_safe_free(dup_dest);
		switch_safe_free(remote_host);
	}

  end:

	if (list) {
		switch_console_free_matches(&list);
	}

	switch_safe_free(contact);
	switch_safe_free(route_uri);
	switch_safe_free(ffrom);
	switch_safe_free(dup);

	if (profile) {
		switch_thread_rwlock_unlock(profile->rwlock);
	}

	return status;
}

void sofia_presence_cancel(void)
{
	char *sql;
	sofia_profile_t *profile;
	struct presence_helper helper = { 0 };
	switch_console_callback_match_t *matches;
	switch_bool_t r;

	if (!mod_sofia_globals.profile_hash) {
		return;
	}

	if (list_profiles_full(NULL, NULL, &matches, SWITCH_FALSE) == SWITCH_STATUS_SUCCESS) {
		switch_console_callback_match_node_t *m;


		for (m = matches->head; m; m = m->next) {
			if ((profile = sofia_glue_find_profile(m->val))) {
				if (profile->pres_type == PRES_TYPE_FULL) {
					helper.profile = profile;
					helper.event = NULL;

					sql = switch_mprintf("select proto,sip_user,sip_host,sub_to_user,sub_to_host,event,contact,call_id,full_from,"
										 "full_via,expires,user_agent,accept,profile_name,network_ip"
										 ",-1,'unavailable','unavailable' from sip_subscriptions where "
										 "event='presence' and hostname='%q' and profile_name='%q'",
										 mod_sofia_globals.hostname, profile->name);

					r = sofia_glue_execute_sql_callback(profile, profile->dbh_mutex, sql, sofia_presence_sub_callback, &helper);
					switch_safe_free(sql);

					if (r != SWITCH_TRUE) {
						sofia_glue_release_profile(profile);
						continue;
					}
				}
				sofia_glue_release_profile(profile);
			}
		}

		switch_console_free_matches(&matches);

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

struct mwi_helper {
	sofia_profile_t *profile;
	int total;
};

static void actual_sofia_presence_mwi_event_handler(switch_event_t *event)
{
	char *account, *dup_account, *yn, *host, *user;
	char *sql;
	sofia_profile_t *profile = NULL;
	switch_stream_handle_t stream = { 0 };
	switch_event_header_t *hp;
	struct mwi_helper h = { 0 };
	const char *pname = NULL;
	const char *call_id;
	const char *sub_call_id;
	int for_everyone = 0;

	switch_assert(event != NULL);

	if (!(account = switch_event_get_header(event, "mwi-message-account"))) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Missing required Header 'MWI-Message-Account'\n");
		return;
	}

	if (!(yn = switch_event_get_header(event, "mwi-messages-waiting"))) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Missing required Header 'MWI-Messages-Waiting'\n");
		return;
	}

	call_id = switch_event_get_header(event, "call-id");
	sub_call_id = switch_event_get_header(event, "sub-call-id");

	if (!call_id && !sub_call_id) {
		for_everyone = 1;
	}


	dup_account = strdup(account);
	switch_assert(dup_account != NULL);
	switch_split_user_domain(dup_account, &user, &host);


	if ((pname = switch_event_get_header(event, "sofia-profile"))) {
		profile = sofia_glue_find_profile(pname);
	}

	if (!profile) {
		if (!host || !(profile = sofia_glue_find_profile(host))) {
			char *sql;
			char buf[512] = "";
			switch_console_callback_match_t *matches;

			sql = switch_mprintf("select profile_name from sip_registrations where hostname='%q' and (sip_host='%s' or mwi_host='%s')",
								 mod_sofia_globals.hostname, host, host);

			if (list_profiles_full(NULL, NULL, &matches, SWITCH_FALSE) == SWITCH_STATUS_SUCCESS) {
				switch_console_callback_match_node_t *m;

				for (m = matches->head; m; m = m->next) {
					if ((profile = sofia_glue_find_profile(m->val))) {

						sofia_glue_execute_sql2str(profile, profile->dbh_mutex, sql, buf, sizeof(buf));
						if (!zstr(buf)) {
							break;
						}
						sofia_glue_release_profile(profile);
					}
				}

				switch_console_free_matches(&matches);
			}

			switch_safe_free(sql);

			if (!(profile = sofia_glue_find_profile(buf))) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Cannot find profile %s\n", switch_str_nil(host));
				switch_safe_free(dup_account);
				return;
			}
		}
	}


	if (profile->domain_name && strcasecmp(profile->domain_name, host)) {
		host = profile->domain_name;
	}

	h.profile = profile;
	h.total = 0;

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

	sql = NULL;

	if (for_everyone) {
		sql = switch_mprintf("select proto,sip_user,sip_host,sub_to_user,sub_to_host,event,contact,call_id,full_from,"
							 "full_via,expires,user_agent,accept,profile_name,network_ip"
							 ",'%q',full_to,network_ip,network_port from sip_subscriptions "
							 "where hostname='%q' and event='message-summary' "
							 "and sub_to_user='%q' and (sub_to_host='%q' or presence_hosts like '%%%q%%')",
							 stream.data, mod_sofia_globals.hostname, user, host, host);
	} else if (sub_call_id) {
		sql = switch_mprintf("select proto,sip_user,sip_host,sub_to_user,sub_to_host,event,contact,call_id,full_from,"
							 "full_via,expires,user_agent,accept,profile_name,network_ip"
							 ",'%q',full_to,network_ip,network_port from sip_subscriptions where "
							 "hostname='%q' and event='message-summary' "
							 "and sub_to_user='%q' and (sub_to_host='%q' or presence_hosts like '%%%q%%') and call_id='%q'",
							 stream.data, mod_sofia_globals.hostname, user, host, host, sub_call_id);
	}


	if (sql) {
		sofia_glue_execute_sql_callback(profile, profile->dbh_mutex, sql, sofia_presence_mwi_callback, &h);
		free(sql);
		sql = NULL;

	}

	if (for_everyone) {
		sql = switch_mprintf("select sip_user,sip_host,contact,profile_name,network_ip,'%q',call_id "
							 "from sip_registrations where hostname='%q' and mwi_user='%q' and mwi_host='%q'",
							 stream.data, mod_sofia_globals.hostname, user, host);
	} else if (call_id) {
		sql = switch_mprintf("select sip_user,sip_host,contact,profile_name,network_ip,'%q',call_id "
							 "from sip_registrations where hostname='%q' and call_id='%q'",
							 stream.data, mod_sofia_globals.hostname, call_id);
	}

	if (sql) {
		switch_assert(sql != NULL);
		sofia_glue_execute_sql_callback(profile, profile->dbh_mutex, sql, sofia_presence_mwi_callback2, &h);
		free(sql);
		sql = NULL;
	}

	switch_safe_free(stream.data);
	switch_safe_free(dup_account);

	if (profile) {
		sofia_glue_release_profile(profile);
	}
}

static int sofia_presence_dialog_callback(void *pArg, int argc, char **argv, char **columnNames)
{
	struct dialog_helper *helper = (struct dialog_helper *) pArg;
	switch_core_session_t *session = NULL;
	switch_channel_t *channel = NULL;
	int done = 0;

	if (argc >= 4) {

		if (argc == 5 && !zstr(argv[4])) {
			if ((session = switch_core_session_locate(argv[4]))) {
				channel = switch_core_session_get_channel(session);

				if (!switch_channel_test_flag(channel, CF_ANSWERED) &&
					switch_true(switch_channel_get_variable_dup(channel, "presence_disable_early", SWITCH_FALSE, -1))) {
					done++;
				}

				switch_core_session_rwunlock(session);
			} else {
				return 0;
			}
		}

		if (done) {
			return 0;
		}

		if (mod_sofia_globals.debug_presence > 0) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "CHECK DIALOG state[%s] status[%s] rpid[%s] pres[%s] uuid[%s]\n",
							  argv[0], argv[1], argv[2], argv[3], argv[4]);
		}

		if (!helper->hits) {
			switch_set_string(helper->state, argv[0]);
			switch_set_string(helper->status, argv[1]);
			switch_set_string(helper->rpid, argv[2]);
			switch_set_string(helper->presence_id, argv[3]);
		}
		helper->hits++;
	}

	return 0;
}


static void do_normal_probe(switch_event_t *event)
{
	char *sql;
	struct resub_helper h = { 0 };
	char *to = switch_event_get_header(event, "to");
	char *proto = switch_event_get_header(event, "proto");
	char *profile_name = switch_event_get_header(event, "sip_profile");
	char *probe_user = NULL, *probe_euser, *probe_host, *p;
	struct dialog_helper dh = { { 0 } };
	char *sub_call_id = switch_event_get_header(event, "sub-call-id");
	sofia_profile_t *profile;

	//DUMP_EVENT(event);

	if (!proto || strcasecmp(proto, SOFIA_CHAT_PROTO) != 0) {
		return;
	}

	if (!to || !(probe_user = strdup(to))) {
		return;
	}

	if ((probe_host = strchr(probe_user, '@'))) {
		*probe_host++ = '\0';
	}
	probe_euser = probe_user;
	if ((p = strchr(probe_euser, '+')) && p != probe_euser) {
		probe_euser = (p + 1);
	}

	if (probe_euser && probe_host && 
		((profile = sofia_glue_find_profile(probe_host)) || (profile_name && (profile = sofia_glue_find_profile(profile_name))))) {
		sql = switch_mprintf("select state,status,rpid,presence_id,uuid from sip_dialogs "
							 "where hostname='%q' and profile_name='%q' and call_info_state != 'seized' and "
							 "((sip_from_user='%q' and sip_from_host='%q') or presence_id='%q@%q') order by rcd desc",
							 mod_sofia_globals.hostname, profile->name, probe_euser, probe_host, probe_euser, probe_host);


		sofia_glue_execute_sql_callback(profile, profile->dbh_mutex, sql, sofia_presence_dialog_callback, &dh);

		h.profile = profile;

		switch_safe_free(sql);

		sql = switch_mprintf("select sip_registrations.sip_user, "
								 "sip_registrations.sub_host, "
								 "sip_registrations.status, "
								 "sip_registrations.rpid, "
								 "'', "
								 "sip_dialogs.uuid, "
								 "sip_dialogs.state, "
								 "sip_dialogs.direction, "
								 "sip_dialogs.sip_to_user, "
								 "sip_dialogs.sip_to_host, "

								 "sip_presence.status,"
								 "sip_presence.rpid,"
								 "sip_dialogs.presence_id, "
								 "sip_presence.open_closed,"
								 "'%q','%q','%q' "
								 "from sip_registrations "

								 "left join sip_dialogs on "
								 "sip_dialogs.hostname = sip_registrations.hostname and sip_dialogs.profile_name = sip_registrations.profile_name and ("
								 "sip_dialogs.presence_id = sip_registrations.sip_user %q '@' %q sip_registrations.sub_host "
								 "or (sip_dialogs.sip_from_user = sip_registrations.sip_user "
								 "and sip_dialogs.sip_from_host = sip_registrations.sip_host)) "

								 "left join sip_presence on "
								 "sip_presence.hostname=sip_registrations.hostname and "
								 "(sip_registrations.sip_user=sip_presence.sip_user and sip_registrations.orig_server_host=sip_presence.sip_host and "
								 "sip_registrations.profile_name=sip_presence.profile_name) "
								 "where sip_registrations.hostname='%q' and sip_registrations.profile_name='%q' and sip_dialogs.call_info_state != 'seized' "
								 "and sip_dialogs.presence_id='%q@%q' or (sip_registrations.sip_user='%q' and "
								 "(sip_registrations.orig_server_host='%q' or sip_registrations.sub_host='%q' "
								 "))",
								 dh.status, dh.rpid, switch_str_nil(sub_call_id),
								 switch_sql_concat(), switch_sql_concat(),
								 mod_sofia_globals.hostname, profile->name, probe_euser, probe_host,  probe_euser, probe_host, probe_host);



		switch_assert(sql);

		if (mod_sofia_globals.debug_presence > 0) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "%s START_PRESENCE_PROBE_SQL\n", profile->name);
		}

		if (mod_sofia_globals.debug_presence > 1) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "%s DUMP PRESENCE_PROBE_SQL:\n%s\n", profile->name, sql);
		}

		sofia_glue_execute_sql_callback(profile, profile->dbh_mutex, sql, sofia_presence_resub_callback, &h);
		switch_safe_free(sql);

		if (!h.rowcount) {
			h.noreg++;

			/* find ones with presence_id defined that are not registred */
			sql = switch_mprintf("select sip_from_user, sip_from_host, 'Registered', '', '', "
								 "uuid, state, direction, "
								 "sip_to_user, sip_to_host,"
								 "'%q','%q',presence_id, '','','' "

								 "from sip_dialogs "

								 "where call_info_state != 'seized' and hostname='%q' and profile_name='%q' and (presence_id='%q@%q' or "
								 "(sip_from_user='%q' and (sip_from_host='%q' or sip_to_host='%q')))",
								 mod_sofia_globals.hostname, profile->name,
								 dh.status, dh.rpid, probe_euser, probe_host,  probe_euser, probe_host, probe_host);

			if (mod_sofia_globals.debug_presence > 0) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "%s START_PRESENCE_PROBE_SQL\n", profile->name);
			}

			if (mod_sofia_globals.debug_presence > 1) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "%s DUMP PRESENCE_PROBE_SQL:\n%s\n", profile->name, sql);
			}

			sofia_glue_execute_sql_callback(profile, profile->dbh_mutex, sql, sofia_presence_resub_callback, &h);
			switch_safe_free(sql);

			if (mod_sofia_globals.debug_presence > 0) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "%s END_PRESENCE_PROBE_SQL\n\n", profile->name);
			}
		}

		if (!h.rowcount) {
			switch_event_t *sevent;
			if (switch_event_create(&sevent, SWITCH_EVENT_PRESENCE_IN) == SWITCH_STATUS_SUCCESS) {
				switch_event_add_header_string(sevent, SWITCH_STACK_BOTTOM, "proto", SOFIA_CHAT_PROTO);
				switch_event_add_header_string(sevent, SWITCH_STACK_BOTTOM, "login", profile->name);
				switch_event_add_header_string(sevent, SWITCH_STACK_BOTTOM, "sip_profile", profile->name);
				switch_event_add_header(sevent, SWITCH_STACK_BOTTOM, "from", "%s@%s", probe_euser, probe_host);
				switch_event_add_header_string(sevent, SWITCH_STACK_BOTTOM, "status", "Unregistered");
				switch_event_fire(&sevent);
			}
		}


		sofia_glue_release_profile(profile);
	}


	switch_safe_free(probe_user);
}

static void do_dialog_probe(switch_event_t *event)
{
	// Received SUBSCRIBE for "dialog" events.
	// Return a complete list of dialogs for the monitored entity.
	char *sql;
	char *to = switch_event_get_header(event, "to");
	char *probe_user = NULL, *probe_euser, *probe_host, *p;

	if (!to || !(probe_user = strdup(to))) {
		return;
	}

	if ((probe_host = strchr(probe_user, '@'))) {
		*probe_host++ = '\0';
	}
	probe_euser = probe_user;
	if ((p = strchr(probe_euser, '+')) && p != probe_euser) {
		probe_euser = (p + 1);
	}

	if (probe_euser && probe_host) {
		char *sub_call_id = switch_event_get_header(event, "sub-call-id");
		char *profile_name = switch_event_get_header(event, "sip_profile");
		sofia_profile_t *profile = sofia_glue_find_profile(probe_host);
		struct rfc4235_helper *h4235 = {0};
		switch_memory_pool_t *pool;

		if (!profile && profile_name) {
			profile = sofia_glue_find_profile(profile_name);
		}
		
		if (!profile) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "Cannot find profile for domain %s\n", probe_host);
			goto end;
		}

		// We need all dialogs with presence_id matching the subscription entity,
		// or from a registered set matching the subscription entity.
		// We need the "proto" of the subscription in case it is for the special "conf" or "park".
		sql = switch_mprintf(
							 "select sip_subscriptions.proto, '%q','%q',"
							 "sip_dialogs.uuid, sip_dialogs.call_id, sip_dialogs.state, sip_dialogs.direction, "
							 "sip_dialogs.sip_to_user, sip_dialogs.sip_to_host, "
							 "sip_dialogs.sip_from_user, sip_dialogs.sip_from_host, "
							 "sip_dialogs.contact, sip_dialogs.contact_user, sip_dialogs.contact_host, "
							 "sip_dialogs.sip_to_tag, sip_dialogs.sip_from_tag, sip_subscriptions.orig_proto "
							 "from sip_dialogs "
							 "left join sip_subscriptions on sip_subscriptions.hostname=sip_dialogs.hostname and "
							 "sip_subscriptions.profile_name=sip_dialogs.profile_name and "
							 "sip_subscriptions.call_id='%q' "
							 "left join sip_registrations on sip_registrations.hostname=sip_dialogs.hostname and "
							 "sip_registrations.profile_name=sip_dialogs.profile_name and "
							 "(sip_dialogs.sip_from_user = sip_registrations.sip_user and sip_dialogs.sip_from_host = '%q' and "
							 "(sip_dialogs.sip_from_host = sip_registrations.orig_server_host or "
							 "sip_dialogs.sip_from_host = sip_registrations.sip_host) ) "
							 "where sip_dialogs.hostname='%q' and sip_dialogs.profile_name='%q' and "
							 "sip_dialogs.call_info_state != 'seized' and sip_dialogs.presence_id='%q@%q' or (sip_registrations.sip_user='%q' and "
							 "(sip_registrations.orig_server_host='%q' or sip_registrations.sub_host='%q' "
							 "or sip_registrations.presence_hosts like '%%%q%%'))",
							 probe_euser, probe_host,
							 sub_call_id, probe_host,
							 mod_sofia_globals.hostname, profile->name,
							 probe_euser, probe_host,
							 probe_euser, probe_host, probe_host, probe_host);
		switch_assert(sql);

		if (mod_sofia_globals.debug_presence > 0) {
			switch_log_printf(SWITCH_CHANNEL_LOG,SWITCH_LOG_INFO, "%s START DIALOG_PROBE_SQL %s@%s\n", profile->name,probe_euser, probe_host);
		}

		if (mod_sofia_globals.debug_presence > 1) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "%s DUMP DIALOG_PROBE_SQL:\n%s\n", profile->name, sql);
		}

		switch_core_new_memory_pool(&pool);
		h4235 = switch_core_alloc(pool, sizeof(*h4235));
		h4235->pool = pool;
		h4235->profile = profile;
		switch_core_hash_init(&h4235->hash);
		sofia_glue_execute_sql_callback(profile, profile->dbh_mutex, sql, sofia_dialog_probe_callback, h4235);
		switch_safe_free(sql);
		if (mod_sofia_globals.debug_presence > 0) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "%s END DIALOG_PROBE_SQL\n\n", profile->name);
		}


		sql = switch_mprintf("update sip_subscriptions set version=version+1 where call_id='%q'", sub_call_id);

		if (mod_sofia_globals.debug_presence > 1) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "%s DUMP DIALOG_PROBE set version sql:\n%s\n", profile->name, sql);
		}
		sofia_glue_execute_sql_now(profile, &sql, SWITCH_TRUE);
		switch_safe_free(sql);


		// The dialog_probe_callback has built up the dialogs to be included in the NOTIFY.
		// Now send the "full" dialog event to the triggering subscription.
		sql = switch_mprintf("select call_id,expires,sub_to_user,sub_to_host,event,version, "
							 "'full',full_to,full_from,contact,network_ip,network_port "
							 "from sip_subscriptions "
							 "where hostname='%q' and profile_name='%q' and sub_to_user='%q' and sub_to_host='%q' and call_id='%q'",
							 mod_sofia_globals.hostname, profile->name, probe_euser, probe_host, sub_call_id);

		if (mod_sofia_globals.debug_presence > 1) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "%s DUMP DIALOG_PROBE subscription sql:\n%s\n", profile->name, sql);
		}

		sofia_glue_execute_sql_callback(profile, profile->dbh_mutex, sql, sofia_dialog_probe_notify_callback, h4235);
		switch_safe_free(sql);

		sofia_glue_release_profile(profile);
		switch_core_hash_destroy(&h4235->hash);
		h4235 = NULL;
		switch_core_destroy_memory_pool(&pool);
	}

 end:

	switch_safe_free(probe_user);
}

static void send_conference_data(sofia_profile_t *profile, switch_event_t *event)
{
	char *sql;
	struct pres_sql_cb cb = {profile, 0};
	const char *call_id = switch_event_get_header(event, "call_id");
	const char *from_user = switch_event_get_header(event, "conference-name");
	const char *from_host = switch_event_get_header(event, "conference-domain");
	const char *event_str = switch_event_get_header(event, "conference-event");
	const char *notfound = switch_event_get_header(event, "notfound");
	const char *body = switch_event_get_body(event);
	const char *type = "application/conference-info+xml";
	const char *final = switch_event_get_header(event, "final");

	if (!event_str) {
		event_str = "conference";
	}

	if (!strcasecmp(event_str, "refer")) {
		type = "message/sipfrag";
	}


	if (!(from_user && from_host)) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Event information not given\n");
		return;
	}

	if (switch_true(notfound)) {
		sql = switch_mprintf("update sip_subscriptions set expires=%ld where "
							 "hostname='%q' and profile_name='%q' and sub_to_user='%q' and sub_to_host='%q' and event='%q'",
							 (long)switch_epoch_time_now(NULL),
							 mod_sofia_globals.hostname, profile->name,
							 from_user, from_host, event_str);

		sofia_glue_execute_sql_now(profile, &sql, SWITCH_TRUE);
	}

	if (call_id) {
	   if (switch_true(final)) {
		  sql = switch_mprintf("update sip_subscriptions set expires=%ld where "
							  "hostname='%q' and profile_name='%q' and sub_to_user='%q' and sub_to_host='%q' and event='%q' "
							   "and call_id = '%q' ",
							   (long)0,
							   mod_sofia_globals.hostname, profile->name,
							   from_user, from_host, event_str, call_id);

		  sofia_glue_execute_sql_now(profile, &sql, SWITCH_TRUE);
	   }

		sql = switch_mprintf("select full_to, full_from, contact %q ';_;isfocus', expires, call_id, event, network_ip, network_port, "
							 "'%q' as ct,'%q' as pt "
							 " from sip_subscriptions where "
							 "hostname='%q' and profile_name='%q' and sub_to_user='%q' and sub_to_host='%q' and event='%q' "
							 "and call_id = '%q' ",
							 switch_sql_concat(),
							 type,
							 switch_str_nil(body),
							 mod_sofia_globals.hostname, profile->name,
							 from_user, from_host, event_str, call_id);
	} else {
	  if (switch_true(final)) {
		 sql = switch_mprintf("update sip_subscriptions set expires=%ld where "
							  "hostname='%q' and profile_name='%q' and sub_to_user='%q' and sub_to_host='%q' and event='%q'",
							  (long)0,
							  mod_sofia_globals.hostname, profile->name,
							  from_user, from_host, event_str);

		 sofia_glue_execute_sql_now(profile, &sql, SWITCH_TRUE);
	  }

		sql = switch_mprintf("select full_to, full_from, contact %q ';_;isfocus', expires, call_id, event, network_ip, network_port, "
							 "'%q' as ct,'%q' as pt "
							 " from sip_subscriptions where "
							 "hostname='%q' and profile_name='%q' and sub_to_user='%q' and sub_to_host='%q' and event='%q'",
							 switch_sql_concat(),
							 type,
							 switch_str_nil(body),
							 mod_sofia_globals.hostname, profile->name,
							 from_user, from_host, event_str);
	}

	sofia_glue_execute_sql_callback(profile, profile->dbh_mutex, sql, sofia_presence_send_sql, &cb);
	switch_safe_free(sql);

	if (switch_true(final)) {
		if (call_id) {
			sql = switch_mprintf("delete from sip_subscriptions where "
								 "hostname='%q' and profile_name='%q' and sub_to_user='%q' and sub_to_host='%q' and event='%q' "
								 "and call_id = '%q' ",
								 mod_sofia_globals.hostname, profile->name,
								 from_user, from_host, event_str, call_id);

		} else {
			sql = switch_mprintf("delete from sip_subscriptions where "
								 "hostname='%q' and profile_name='%q' and sub_to_user='%q' and sub_to_host='%q' and event='%q'",
								 mod_sofia_globals.hostname, profile->name,
								 from_user, from_host, event_str);
		}

		sofia_glue_execute_sql_now(profile, &sql, SWITCH_TRUE);
	}


}

static void conference_data_event_handler(switch_event_t *event)
{
	const char *pname;
	//const char *from_user = switch_event_get_header(event, "conference-name");
	//const char *from_host = switch_event_get_header(event, "conference-domain");
	const char *host = switch_event_get_header(event, "conference-domain");
	char *dup_domain = NULL;
	sofia_profile_t *profile = NULL;

	if (zstr(host)) {
		dup_domain = switch_core_get_domain(SWITCH_TRUE);
		host = dup_domain;
	}

	if ((pname = switch_event_get_header(event, "sofia-profile"))) {
		profile = sofia_glue_find_profile(pname);
	}

	if (host && !profile) {
		profile = sofia_glue_find_profile(host);
	}

	if (profile) {
		send_conference_data(profile, event);
		sofia_glue_release_profile(profile);
	} else {
		switch_console_callback_match_t *matches;

		if (list_profiles_full(NULL, NULL, &matches, SWITCH_FALSE) == SWITCH_STATUS_SUCCESS) {
			switch_console_callback_match_node_t *m;

			for (m = matches->head; m; m = m->next) {
				if ((profile = sofia_glue_find_profile(m->val))) {
					send_conference_data(profile, event);
					sofia_glue_release_profile(profile);
				}
			}

			switch_console_free_matches(&matches);
		}
	}

	switch_safe_free(dup_domain);
}

static switch_event_t *actual_sofia_presence_event_handler(switch_event_t *event)
{
	sofia_profile_t *profile = NULL;
	char *from = switch_event_get_header(event, "from");
	char *proto = switch_event_get_header(event, "proto");
	char *rpid = switch_event_get_header(event, "rpid");
	char *status = switch_event_get_header(event, "status");
	char *event_type = switch_event_get_header(event, "event_type");
	char *alt_event_type = switch_event_get_header(event, "alt_event_type");
	//char *event_subtype = switch_event_get_header(event, "event_subtype");
	char *sql = NULL;
	char *euser = NULL, *user = NULL, *host = NULL;
	char *call_info = switch_event_get_header(event, "presence-call-info");
	char *call_id = switch_event_get_header(event, "call-id");
	char *presence_source = switch_event_get_header(event, "presence-source");
	char *call_info_state = switch_event_get_header(event, "presence-call-info-state");
	const char *uuid = switch_event_get_header(event, "unique-id");
	switch_console_callback_match_t *matches = NULL;
	struct presence_helper helper = { 0 };
	int hup = 0;
	switch_event_t *s_event = NULL;

	if (!mod_sofia_globals.running) {
		goto done;
	}

	if (zstr(proto) || !strcasecmp(proto, "any")) {
		proto = SOFIA_CHAT_PROTO;
	}

	//DUMP_EVENT(event);

	if (rpid && !strcasecmp(rpid, "n/a")) {
		rpid = NULL;
	}

	if (status && !strcasecmp(status, "n/a")) {
		status = NULL;
	}

	if (!zstr(uuid) && !switch_ivr_uuid_exists(uuid)) {
		status = "CS_HANGUP";
	}


	if ((status && switch_stristr("CS_HANGUP", status)) || (!zstr(uuid) && !switch_ivr_uuid_exists(uuid))) {
		status = "Available";
		hup = 1;
	}

	if (rpid) {
		rpid = sofia_presence_translate_rpid(rpid, status);
	}

	if (event->event_id == SWITCH_EVENT_ROSTER) {
		if (list_profiles_full(NULL, NULL, &matches, SWITCH_FALSE) == SWITCH_STATUS_SUCCESS) {
			switch_console_callback_match_node_t *m;

			for (m = matches->head; m; m = m->next) {
				if ((profile = sofia_glue_find_profile(m->val))) {
					if (profile->pres_type != PRES_TYPE_FULL) {


						if (!mod_sofia_globals.profile_hash) {
							switch_console_free_matches(&matches);
							goto done;
						}

						if (from) {

							sql = switch_mprintf("update sip_subscriptions set version=version+1 where hostname='%q' and profile_name='%q' and "
												 "sip_subscriptions.event='presence' and sip_subscriptions.full_from like '%%%q%%'",
												 mod_sofia_globals.hostname, profile->name, from);

							if (mod_sofia_globals.debug_presence > 1) {
								switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "PRES SQL %s\n", sql);
							}

							sofia_glue_execute_sql_now(profile, &sql, SWITCH_TRUE);


							sql = switch_mprintf("select sip_subscriptions.proto,sip_subscriptions.sip_user,sip_subscriptions.sip_host,"
												 "sip_subscriptions.sub_to_user,sip_subscriptions.sub_to_host,sip_subscriptions.event,"
												 "sip_subscriptions.contact,sip_subscriptions.call_id,sip_subscriptions.full_from,"
												 "sip_subscriptions.full_via,sip_subscriptions.expires,sip_subscriptions.user_agent,"
												 "sip_subscriptions.accept,sip_subscriptions.profile_name,sip_subscriptions.network_ip"
												 ",1,'%q','%q',sip_presence.status,sip_presence.rpid,sip_presence.open_closed,'','','','','sip',"
												 " sip_subscriptions.full_to,sip_subscriptions.network_ip,sip_subscriptions.network_port "
												 "from sip_subscriptions left join sip_presence on "
												 "(sip_subscriptions.sub_to_user=sip_presence.sip_user and "
												 "sip_subscriptions.sub_to_host=sip_presence.sip_host and "
												 "sip_subscriptions.profile_name=sip_presence.profile_name and "
												 "sip_presence.profile_name=sip_subscriptions.profile_name) "
												 "where sip_subscriptions.hostname='%q' and sip_subscriptions.profile_name='%q' and "
												 "sip_subscriptions.event='presence' and sip_subscriptions.full_from like '%%%q%%'",
												 switch_str_nil(status), switch_str_nil(rpid), mod_sofia_globals.hostname, profile->name, from);
						} else {

							sql = switch_mprintf("update sip_subscriptions set version=version+1 where hostname='%q' and profile_name='%q' and "
												 "sip_subscriptions.event='presence'", mod_sofia_globals.hostname, profile->name);

							if (mod_sofia_globals.debug_presence > 1) {
								switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "PRES SQL %s\n", sql);
							}

							sofia_glue_execute_sql_now(profile, &sql, SWITCH_TRUE);

							sql = switch_mprintf("select sip_subscriptions.proto,sip_subscriptions.sip_user,sip_subscriptions.sip_host,"
												 "sip_subscriptions.sub_to_user,sip_subscriptions.sub_to_host,sip_subscriptions.event,"
												 "sip_subscriptions.contact,sip_subscriptions.call_id,sip_subscriptions.full_from,"
												 "sip_subscriptions.full_via,sip_subscriptions.expires,sip_subscriptions.user_agent,"
												 "sip_subscriptions.accept,sip_subscriptions.profile_name,sip_subscriptions.network_ip"
												 ",1,'%q','%q',sip_presence.status,sip_presence.rpid,sip_presence.open_closed,'','','','','sip',"
												 "sip_subscriptions.full_to,sip_subscriptions.network_ip,sip_subscriptions.network_port "
												 "from sip_subscriptions left join sip_presence on "
												 "(sip_subscriptions.sub_to_user=sip_presence.sip_user and "
												 "sip_subscriptions.sub_to_host=sip_presence.sip_host and "
												 "sip_subscriptions.profile_name=sip_presence.profile_name and "
												 "sip_subscriptions.hostname = sip_presence.hostname) "
												 "where sip_subscriptions.hostname='%q' and sip_subscriptions.profile_name='%q' and "
												 "sip_subscriptions.event='presence'", switch_str_nil(status),
												 switch_str_nil(rpid), mod_sofia_globals.hostname, profile->name);
						}

						switch_assert(sql != NULL);


						if (mod_sofia_globals.debug_presence > 0) {
							switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "%s is passive, skipping\n", (char *) profile->name);
						}
						sofia_glue_release_profile(profile);
						continue;
					}
					memset(&helper, 0, sizeof(helper));
					helper.profile = profile;
					helper.event = NULL;
					sofia_glue_execute_sql_callback(profile, profile->dbh_mutex, sql, sofia_presence_sub_callback, &helper);
					switch_safe_free(sql);
					sofia_glue_release_profile(profile);
				}
			}
			switch_console_free_matches(&matches);
		}

		switch_safe_free(sql);
		goto done;
	}

	if (zstr(event_type)) {
		event_type = "presence";
	}

	if (zstr(alt_event_type)) {
		if (!strcasecmp(event_type, "presence")) {
			alt_event_type = "dialog";
		} else {
			alt_event_type = "presence";
		}
	}

	if (from && (user = strdup(from))) {
		if ((host = strchr(user, '@'))) {
			char *p;
			*host++ = '\0';
			if ((p = strchr(host, '/'))) {
				*p = '\0';
			}
		} else {
			switch_safe_free(user);
			goto done;
		}
		if ((euser = strchr(user, '+')) && euser != user) {
			euser++;
		} else {
			euser = user;
		}
	} else {
		goto done;
	}

	switch (event->event_id) {
	case SWITCH_EVENT_PRESENCE_PROBE:
		{
			char *probe_type = switch_event_get_header(event, "probe-type");

			if (!probe_type || strcasecmp(probe_type, "dialog")) {
				/* NORMAL PROBE */
				do_normal_probe(event);
			} else {
				/* DIALOG PROBE */
				do_dialog_probe(event);
			}
		}
		goto done;

	default:
		break;
	}

	if (!mod_sofia_globals.profile_hash) {
		goto done;
	}

	if (list_profiles_full(NULL, NULL, &matches, SWITCH_FALSE) == SWITCH_STATUS_SUCCESS) {
		switch_console_callback_match_node_t *m;

		for (m = matches->head; m; m = m->next) {
			struct dialog_helper dh = { { 0 } };

			if ((profile = sofia_glue_find_profile(m->val))) {
				if (profile->pres_type != PRES_TYPE_FULL) {
					if (mod_sofia_globals.debug_presence > 0) {
						switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "%s is passive, skipping\n", (char *) profile->name);
					}
					sofia_glue_release_profile(profile);
					continue;
				}


				if (mod_sofia_globals.debug_sla > 1) {
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "SLA EVENT:\n");
					DUMP_EVENT(event);

					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "CHECK CALL_INFO [%s]\n", switch_str_nil(call_info));
				}

				if (call_info) {

					if (uuid) {
						sql = switch_mprintf("update sip_dialogs set call_info='%q',call_info_state='%q' where "
											 "hostname='%q' and profile_name='%q' and uuid='%q'",
											 call_info, call_info_state, mod_sofia_globals.hostname, profile->name, uuid);
					} else {
						sql = switch_mprintf("update sip_dialogs set call_info='%q', call_info_state='%q' where hostname='%q' and profile_name='%q' and "
											 "((sip_dialogs.sip_from_user='%q' and sip_dialogs.sip_from_host='%q') or presence_id='%q@%q') and call_info='%q'",

											 call_info, call_info_state, mod_sofia_globals.hostname, profile->name, euser, host, euser, host, call_info);

					}

					if (mod_sofia_globals.debug_sla > 1) {
						switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "STATE SQL %s\n", sql);
					}
					sofia_glue_execute_sql_now(profile, &sql, SWITCH_TRUE);



					if (mod_sofia_globals.debug_sla > 1) {
						switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "PROCESS PRESENCE EVENT\n");
					}

					sync_sla(profile, euser, host, SWITCH_TRUE, SWITCH_TRUE, call_id);
				}

				if (!strcmp(proto, "dp")) {
					sql = switch_mprintf("update sip_presence set rpid='%q',status='%q' where hostname='%q' and profile_name='%q' and "
										 "sip_user='%q' and sip_host='%q'",
										 rpid, status, mod_sofia_globals.hostname, profile->name, euser, host);
					sofia_glue_execute_sql_now(profile, &sql, SWITCH_TRUE);
					proto = SOFIA_CHAT_PROTO;
				}

				if (zstr(uuid)) {

					sql = switch_mprintf("select state,status,rpid,presence_id,uuid from sip_dialogs "
										 "where call_info_state != 'seized' and hostname='%q' and profile_name='%q' and "
										 "((sip_from_user='%q' and sip_from_host='%q') or presence_id='%q@%q') order by rcd desc",
										 mod_sofia_globals.hostname, profile->name, euser, host, euser, host);
				} else {
					sql = switch_mprintf("select state,status,rpid,presence_id,uuid from sip_dialogs "
										 "where uuid != '%q' and call_info_state != 'seized' and hostname='%q' and profile_name='%q' and "
										 "((sip_from_user='%q' and sip_from_host='%q') or presence_id='%q@%q') order by rcd desc",
										 uuid, mod_sofia_globals.hostname, profile->name, euser, host, euser, host);
				}

				sofia_glue_execute_sql_callback(profile, profile->dbh_mutex, sql, sofia_presence_dialog_callback, &dh);

				if (mod_sofia_globals.debug_presence > 0) {
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "CHECK SQL: %s@%s [%s]\nhits: %d\n", euser, host, sql, dh.hits);
				}

				switch_safe_free(sql);

				if (hup && dh.hits > 0) {
					/* sigh, mangle this packet to simulate a call that is up instead of hungup */
					hup = 0;
					event->flags |= EF_UNIQ_HEADERS;

					if (!strcasecmp(dh.state, "early")) {
						status = "CS_ROUTING";
						if (rpid) {
							rpid = sofia_presence_translate_rpid(rpid, status);
						}

						switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "Answer-State", "early");
						switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "status", status);
						switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "Channel-State", status);
						switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "Channel-Call-State", "EARLY");
						switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "astate", "early");
						switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "answer-state", "early");
					} else {
						status = "CS_EXECUTE";
						if (rpid) {
							rpid = sofia_presence_translate_rpid(rpid, status);
						}

						switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "Answer-State", "answered");
						switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "status", status);
						switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "Channel-State", status);
						switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "Channel-Call-State", "ACTIVE");
						switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "astate", "confirmed");
						switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "answer-state", "confirmed");
					}
				}



				if (zstr(call_id) && (dh.hits && presence_source && (!strcasecmp(presence_source, "register") || switch_stristr("register", status)))) {
					goto done;
				}

				if (zstr(call_id)) {

					sql = switch_mprintf("update sip_subscriptions set version=version+1 where hostname='%q' and profile_name='%q' and "
										 "sip_subscriptions.event != 'line-seize' "
										 "and sip_subscriptions.proto='%q' and (event='%q' or event='%q') and sub_to_user='%q' and "
										 "(sub_to_host='%q' or sub_to_host='%q' or sub_to_host='%q' or "
										 "presence_hosts like '%%%q%%') and "
										 "(sip_subscriptions.profile_name = '%q' or presence_hosts like '%%%q%%')",
										 mod_sofia_globals.hostname, profile->name,
										 proto, event_type, alt_event_type, euser, host, profile->sipip,
										 profile->extsipip ? profile->extsipip : "N/A", host, profile->name, host);


					if (mod_sofia_globals.debug_presence > 1) {
						switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "PRES SQL %s\n", sql);
					}

					sofia_glue_execute_sql_now(profile, &sql, SWITCH_TRUE);



					sql = switch_mprintf("select distinct sip_subscriptions.proto,sip_subscriptions.sip_user,sip_subscriptions.sip_host,"
										 "sip_subscriptions.sub_to_user,sip_subscriptions.sub_to_host,sip_subscriptions.event,"
										 "sip_subscriptions.contact,sip_subscriptions.call_id,sip_subscriptions.full_from,"
										 "sip_subscriptions.full_via,sip_subscriptions.expires,sip_subscriptions.user_agent,"
										 "sip_subscriptions.accept,sip_subscriptions.profile_name"
										 ",'%q','%q','%q',sip_presence.status,sip_presence.rpid,sip_presence.open_closed,'%q','%q',"
										 "sip_subscriptions.version, '%q',sip_subscriptions.orig_proto,sip_subscriptions.full_to,"
										 "sip_subscriptions.network_ip, sip_subscriptions.network_port "
										 "from sip_subscriptions "
										 "left join sip_presence on "
										 "(sip_subscriptions.sub_to_user=sip_presence.sip_user and sip_subscriptions.sub_to_host=sip_presence.sip_host and "
										 "sip_subscriptions.profile_name=sip_presence.profile_name and sip_subscriptions.hostname=sip_presence.hostname) "

										 "where sip_subscriptions.hostname='%q' and sip_subscriptions.profile_name='%q' and "
										 "sip_subscriptions.event != 'line-seize' and "
										 "sip_subscriptions.proto='%q' and "
										 "(event='%q' or event='%q') and sub_to_user='%q' "
										 "and (sub_to_host='%q' or sub_to_host='%q' or sub_to_host='%q' or presence_hosts like '%%%q%%') ",


										 switch_str_nil(status), switch_str_nil(rpid), host,
										 dh.status,dh.rpid,dh.presence_id, mod_sofia_globals.hostname, profile->name, proto,
										 event_type, alt_event_type, euser, host, profile->sipip,
										 profile->extsipip ? profile->extsipip : "N/A", host);
				} else {

					sql = switch_mprintf("update sip_subscriptions set version=version+1 where sip_subscriptions.event != 'line-seize' and "
										 "sip_subscriptions.call_id='%q'", call_id);



					if (mod_sofia_globals.debug_presence > 1) {
						switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "PRES SQL %s\n", sql);
					}

					sofia_glue_execute_sql_now(profile, &sql, SWITCH_TRUE);


					sql = switch_mprintf("select distinct sip_subscriptions.proto,sip_subscriptions.sip_user,sip_subscriptions.sip_host,"
										 "sip_subscriptions.sub_to_user,sip_subscriptions.sub_to_host,sip_subscriptions.event,"
										 "sip_subscriptions.contact,sip_subscriptions.call_id,sip_subscriptions.full_from,"
										 "sip_subscriptions.full_via,sip_subscriptions.expires,sip_subscriptions.user_agent,"
										 "sip_subscriptions.accept,sip_subscriptions.profile_name"
										 ",'%q','%q','%q',sip_presence.status,sip_presence.rpid,sip_presence.open_closed,'%q','%q',"
										 "sip_subscriptions.version, '%q',sip_subscriptions.orig_proto,sip_subscriptions.full_to,"
										 "sip_subscriptions.network_ip, sip_subscriptions.network_port "
										 "from sip_subscriptions "
										 "left join sip_presence on "
										 "(sip_subscriptions.sub_to_user=sip_presence.sip_user and sip_subscriptions.sub_to_host=sip_presence.sip_host and "
										 "sip_subscriptions.profile_name=sip_presence.profile_name and sip_subscriptions.hostname=sip_presence.hostname) "

										 "where sip_subscriptions.hostname='%q' and sip_subscriptions.profile_name='%q' and "
										 "sip_subscriptions.event != 'line-seize' and "
										 "sip_subscriptions.call_id='%q'",

										 switch_str_nil(status), switch_str_nil(rpid), host,
										 dh.status,dh.rpid,dh.presence_id, mod_sofia_globals.hostname, profile->name, call_id);

				}

				helper.hup = hup;
				helper.calls_up = dh.hits;
				helper.profile = profile;
				helper.event = event;
				SWITCH_STANDARD_STREAM(helper.stream);
				switch_assert(helper.stream.data);

				if (mod_sofia_globals.debug_presence > 0) {
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "%s START_PRESENCE_SQL (%s)\n",
									  event->event_id == SWITCH_EVENT_PRESENCE_IN ? "IN" : "OUT", profile->name);
				}

				if (mod_sofia_globals.debug_presence) {
					char *buf;
					switch_event_serialize(event, &buf, SWITCH_FALSE);
					switch_assert(buf);
					if (mod_sofia_globals.debug_presence > 1) {
						switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "DUMP PRESENCE SQL:\n%s\nEVENT DUMP:\n%s\n", sql, buf);
					} else {
						switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "EVENT DUMP:\n%s\n", buf);
					}
					free(buf);
				}

				sofia_glue_execute_sql_callback(profile, profile->dbh_mutex, sql, sofia_presence_sub_callback, &helper);
				switch_safe_free(sql);

				if (mod_sofia_globals.debug_presence > 0) {
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "%s END_PRESENCE_SQL (%s)\n",
									  event->event_id == SWITCH_EVENT_PRESENCE_IN ? "IN" : "OUT", profile->name);
				}

#if 0
				if (hup && dh.hits < 1) {
					/* so many phones get confused when whe hangup we have to reprobe to get them all to reset to absolute states so the lights stay correct */
					if (switch_event_create(&s_event, SWITCH_EVENT_PRESENCE_PROBE) == SWITCH_STATUS_SUCCESS) {
						switch_event_add_header_string(s_event, SWITCH_STACK_BOTTOM, "proto", SOFIA_CHAT_PROTO);
						switch_event_add_header_string(s_event, SWITCH_STACK_BOTTOM, "login", profile->name);
						switch_event_add_header_string(s_event, SWITCH_STACK_BOTTOM, "sip_profile", profile->name);
						switch_event_add_header(s_event, SWITCH_STACK_BOTTOM, "from", "%s@%s", euser, host);
						switch_event_add_header(s_event, SWITCH_STACK_BOTTOM, "to", "%s@%s", euser, host);
						switch_event_add_header_string(s_event, SWITCH_STACK_BOTTOM, "event_type", "presence");
						switch_event_add_header_string(s_event, SWITCH_STACK_BOTTOM, "alt_event_type", "dialog");
						sofia_event_fire(profile, &s_event);
					}
				}
#endif

				if (!zstr((char *) helper.stream.data)) {
					char *this_sql = (char *) helper.stream.data;
					char *next = NULL;
					char *last = NULL;

					do {
						if ((next = strchr(this_sql, ';'))) {
							*next++ = '\0';
							while (*next == '\n' || *next == ' ' || *next == '\r') {
								*next++ = '\0';
							}
						}

						if (!zstr(this_sql) && (!last || strcmp(last, this_sql))) {
							sofia_glue_execute_sql(profile, &this_sql, SWITCH_FALSE);
							last = this_sql;
						}
						this_sql = next;
					} while (this_sql);
				}
				switch_safe_free(helper.stream.data);
				helper.stream.data = NULL;

				sofia_glue_release_profile(profile);
			}
		}
		switch_console_free_matches(&matches);
	}

 done:

	switch_safe_free(sql);
	switch_safe_free(user);

	return s_event;
}

static int EVENT_THREAD_RUNNING = 0;
static int EVENT_THREAD_STARTED = 0;

static void do_flush(void)
{
	void *pop = NULL;

	while (mod_sofia_globals.presence_queue && switch_queue_trypop(mod_sofia_globals.presence_queue, &pop) == SWITCH_STATUS_SUCCESS && pop) {
		switch_event_t *event = (switch_event_t *) pop;
		switch_event_destroy(&event);
	}

}

void *SWITCH_THREAD_FUNC sofia_presence_event_thread_run(switch_thread_t *thread, void *obj)
{
	void *pop;
	int done = 0;

	switch_mutex_lock(mod_sofia_globals.mutex);
	if (!EVENT_THREAD_RUNNING) {
		EVENT_THREAD_RUNNING++;
		mod_sofia_globals.threads++;
	} else {
		done = 1;
	}
	switch_mutex_unlock(mod_sofia_globals.mutex);

	if (done) {
		return NULL;
	}

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CONSOLE, "Event Thread Started\n");

	while (mod_sofia_globals.running == 1) {
		int count = 0;

		if (switch_queue_pop(mod_sofia_globals.presence_queue, &pop) == SWITCH_STATUS_SUCCESS) {
			switch_event_t *event = (switch_event_t *) pop;

			if (!pop) {
				break;
			}

			if (mod_sofia_globals.presence_flush) {
				switch_mutex_lock(mod_sofia_globals.mutex);
				if (mod_sofia_globals.presence_flush) {
					do_flush();
					mod_sofia_globals.presence_flush = 0;
				}
				switch_mutex_unlock(mod_sofia_globals.mutex);
			}

			switch(event->event_id) {
			case SWITCH_EVENT_MESSAGE_WAITING:
				actual_sofia_presence_mwi_event_handler(event);
				break;
			case SWITCH_EVENT_CONFERENCE_DATA:
				conference_data_event_handler(event);
				break;
			default:
				do {
					switch_event_t *ievent = event;
					event = actual_sofia_presence_event_handler(ievent);
					switch_event_destroy(&ievent);
				} while (event);
				break;
			}

			switch_event_destroy(&event);
			count++;
		}
	}

	do_flush();

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CONSOLE, "Event Thread Ended\n");

	switch_mutex_lock(mod_sofia_globals.mutex);
	mod_sofia_globals.threads--;
	EVENT_THREAD_RUNNING = EVENT_THREAD_STARTED = 0;
	switch_mutex_unlock(mod_sofia_globals.mutex);

	return NULL;
}

void sofia_presence_event_thread_start(void)
{
	//switch_thread_t *thread;
	switch_threadattr_t *thd_attr = NULL;
	int done = 0;

	switch_mutex_lock(mod_sofia_globals.mutex);
	if (!EVENT_THREAD_STARTED) {
		EVENT_THREAD_STARTED++;
	} else {
		done = 1;
	}
	switch_mutex_unlock(mod_sofia_globals.mutex);

	if (done) {
		return;
	}

	switch_threadattr_create(&thd_attr, mod_sofia_globals.pool);
	//switch_threadattr_detach_set(thd_attr, 1);
	switch_threadattr_stacksize_set(thd_attr, SWITCH_THREAD_STACKSIZE);
	switch_threadattr_priority_set(thd_attr, SWITCH_PRI_IMPORTANT);
	switch_thread_create(&mod_sofia_globals.presence_thread, thd_attr, sofia_presence_event_thread_run, NULL, mod_sofia_globals.pool);
}


void sofia_presence_event_handler(switch_event_t *event)
{
	switch_event_t *cloned_event;

	if (!EVENT_THREAD_STARTED) {
		sofia_presence_event_thread_start();
		switch_yield(500000);
	}

	switch_event_dup(&cloned_event, event);
	switch_assert(cloned_event);

	if (switch_queue_trypush(mod_sofia_globals.presence_queue, cloned_event) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Presence queue overloaded.... Flushing queue\n");
		switch_mutex_lock(mod_sofia_globals.mutex);
		mod_sofia_globals.presence_flush = 1;
		switch_mutex_unlock(mod_sofia_globals.mutex);
		switch_event_destroy(&cloned_event);
	}


}


static int sofia_presence_sub_reg_callback(void *pArg, int argc, char **argv, char **columnNames)
{
	sofia_profile_t *profile = (sofia_profile_t *) pArg;
	char *user = argv[3];
	char *host = argv[2];
	switch_event_t *event;
	char *event_name = argv[5];
	char *expires = argv[10];



	if (!strcasecmp(event_name, "message-summary")) {

		if (switch_event_create(&event, SWITCH_EVENT_MESSAGE_QUERY) == SWITCH_STATUS_SUCCESS) {
			switch_event_add_header(event, SWITCH_STACK_BOTTOM, "Message-Account", "sip:%s@%s", user, host);
			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "VM-Sofia-Profile", profile->name);
			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "VM-sub-call-id", argv[7]);

			if (mod_sofia_globals.debug_presence > 0) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Create MESSAGE QUERY EVENT...\n");
				DUMP_EVENT(event);
			}


			sofia_event_fire(profile, &event);
		}
		return 0;
	}

	if (switch_event_create(&event, SWITCH_EVENT_PRESENCE_PROBE) == SWITCH_STATUS_SUCCESS) {
		switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "proto", SOFIA_CHAT_PROTO);
		switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "login", profile->url);
		switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "sip_profile", profile->name);
		switch_event_add_header(event, SWITCH_STACK_BOTTOM, "from", "%s@%s", user, host);
		switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "event_type", "presence");
		switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "event_subtype", "probe");
		switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "proto-specific-event-name", event_name);
		switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "expires", expires);
		sofia_event_fire(profile, &event);
	}

	return 0;
}

static int sofia_presence_resub_callback(void *pArg, int argc, char **argv, char **columnNames)
{
	struct resub_helper *h = (struct resub_helper *) pArg;
	sofia_profile_t *profile = h->profile;
	char *user = argv[0];
	char *host = argv[1];
	char *status = argv[2];
	char *rpid = argv[3];
	char *proto = argv[4];
	char *call_id = NULL;
	char *presence_id = NULL;
	char *to_user = NULL;
	char *uuid = NULL;
	char *state = NULL;
	char *direction = NULL;
	switch_event_t *event;
	char to_buf[128] = "";
	switch_event_header_t *hp;
	char *free_me = NULL;
	int do_event = 1, i;


	if (mod_sofia_globals.debug_presence > 1) {
		for (i = 0; i < argc; i++) {
			switch_log_printf(SWITCH_CHANNEL_LOG,SWITCH_LOG_WARNING,  "sofia_presence_resub_callback: %d [%s]=[%s]\n", i, columnNames[i], argv[i]);
		}
	}

	if (argc > 5) {
		uuid = argv[5];
		state = switch_str_nil(argv[6]);
		direction = switch_str_nil(argv[7]);
		if (argc > 8) {
			switch_set_string(to_buf, argv[8]);
			switch_url_decode(to_buf);
			to_user = to_buf;
		}
		if (argc > 10 && !zstr(argv[10]) && !zstr(argv[11])) {
			status = argv[10];
			rpid = argv[11];
		}

		if (argc > 12 && !zstr(argv[12]) && strchr(argv[12], '@')) {
			char *p;

			presence_id = argv[12];
			free_me = strdup(presence_id);
			if ((p = strchr(free_me, '@'))) *p = '\0';
			user = free_me;
		}

		if (argc > 16) {
			call_id = argv[16];
		}

	}

	if (!zstr(uuid) && !switch_ivr_uuid_exists(uuid)) {
		if (mod_sofia_globals.debug_presence > 0) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "%s SKIPPING NOT FOUND UUID %s\n", profile->name, uuid);
		}
		do_event = 0;
	}

	if (zstr(proto)) {
		proto = NULL;
	}

	if (mod_sofia_globals.debug_presence > 0) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "%s PRESENCE_PROBE %s@%s\n", profile->name, user, host);
	}

	if (do_event && switch_event_create(&event, SWITCH_EVENT_PRESENCE_IN) == SWITCH_STATUS_SUCCESS) {
		switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "proto", proto ? proto : SOFIA_CHAT_PROTO);
		switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "login", profile->url);
		switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "sip_profile", profile->name);

		switch_event_add_header(event, SWITCH_STACK_BOTTOM, "from", "%s@%s", user, host);

		if (h->noreg) {
			switch_event_add_header(event, SWITCH_STACK_BOTTOM, "Force-Direction", "inbound");
		}

		if (!zstr(call_id)) {
			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "call-id", call_id);
		}

		//switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "resub", "true");
		switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "status", status);
		switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "rpid", rpid);
		switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "event_type", "presence");
		switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "alt_event_type", "dialog");
		switch_event_add_header(event, SWITCH_STACK_BOTTOM, "event_count", "%d", 0);

		if (!zstr(to_user)) {
			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "to-user", to_user);
		}

		if (zstr(state)) {
			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "channel-state", "CS_HANGUP");
			//switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "answer-state", "resubscribe");
		} else {
			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "channel-state", "CS_ROUTING");
			if (uuid) {
				switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "unique-id", uuid);
			}
			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "answer-state", state);
			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "astate", state);
			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "presence-call-direction", direction);
		}

		if (h->event) {
			for (hp = h->event->headers; hp; hp = hp->next) {
				if (!strncasecmp(hp->name, "fwd-", 4)) {
					switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, hp->name + 4, hp->value);
				}
			}
		}

		sofia_event_fire(profile, &event);
	}

	switch_safe_free(free_me);


	h->rowcount++;


	return 0;
}

char *get_display_name_from_contact(const char *in, char* dst)
{
	// name-addr      =  [ display-name ] LAQUOT addr-spec RAQUOT
	// display-name   =  *(token LWS)/ quoted-string
	// return whatever comes before the left angle bracket, stripped of whitespace and quotes
	char *p;
	char *buf;

	strcpy(dst, "");
	if (strchr(in, '<') && strchr(in, '>')) {
		buf = strdup(in);
		p = strchr(buf, '<');
		*p = '\0';
		if (!zstr(buf)) {
			p = switch_strip_whitespace(buf);
			if (p) {
				if (*p == '"') {
					if (end_of(p+1) == '"') {
						char *q = strdup(p + 1);
						end_of(q) = '\0';
						strcpy(dst, q);
						switch_safe_free(q);
					}
				} else {
					strcpy(dst, p);
				}
				switch_safe_free(p);
			}
		}
		switch_safe_free(buf);
	}
	return dst;
}

static int sofia_dialog_probe_callback(void *pArg, int argc, char **argv, char **columnNames)
{
	struct rfc4235_helper *h = (struct rfc4235_helper *) pArg;

	char *proto = argv[0];
	char *user = argv[1];
	char *host = argv[2];
	char *uuid = argv[3];
	char *call_id = argv[4];
	char *state = argv[5];
	char *direction = argv[6];
	char *to_user = argv[7];
	char *to_host = argv[8];
	char *from_user = argv[9];
	//    char *from_host = argv[10];
	char *contact = switch_str_nil(argv[11]);
	char *contact_user = switch_str_nil(argv[12]);
	char *contact_host = switch_str_nil(argv[13]);
	char *to_tag = switch_str_nil(argv[14]);
	char *from_tag = switch_str_nil(argv[15]);
	char *orig_proto = switch_str_nil(argv[16]);

	const char *event_status = "";
	char *data = NULL, *tmp;
	char key[256] = "";
	char *local_user;
	char *local_host;
	char *remote_user;
	char *remote_host;
	char *remote_uri;
	char *local_user_param = "";
	char remote_display_buf[512];
	char *buf_to_free = NULL;
	int bInternal = 0;
	int i;
	int skip_proto = 0;

	if (mod_sofia_globals.debug_presence > 1) {
		for (i = 0; i < argc; i++) {
			switch_log_printf(SWITCH_CHANNEL_LOG,SWITCH_LOG_WARNING,  "sofia_dialog_probe_callback: %d [%s]=[%s]\n", i, columnNames[i], argv[i]);
		}
	}

	if (zstr(to_user) || zstr(contact_user)) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "sofia_dialog_probe_callback: not enough info to generate a dialog entry\n");
		return 0;
	}

	// Usually we report the dialogs FROM the probed user.  The exception is when the monitored endpoint is internal,
	// and its presence_id is set in the dialplan.  Reverse the direction if this is not a registered entity.
	if (!strcmp(direction, "inbound") && strcmp(user, from_user) ) {
		// If inbound and the entity is not the caller (i.e. internal to FS), then the direction is reversed
		// because it is not going through the B2BUA
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "sofia_dialog_probe_callback: endpt is internal\n");
		direction = !strcasecmp(direction, "outbound") ? "inbound" : "outbound";
		bInternal = 1;
	}

	if (!strcasecmp(direction, "outbound")) {
		direction = "recipient";
	}
	else {
		direction = "initiator";
	}

	if (!zstr(orig_proto) && !strcmp(orig_proto, SOFIA_CHAT_PROTO)) {
		skip_proto = 1;
	}

	local_host = to_host;
	if (proto && !strcasecmp(proto, "queue")) {
		local_user = to_user;
		local_user_param = switch_mprintf(";proto=%s", proto);
		event_status = "hold";
		if (skip_proto) {
			buf_to_free = switch_mprintf("sip:%s", to_user);
		} else {
			buf_to_free = switch_mprintf("sip:queue+%s", to_user);
		}
		remote_uri = buf_to_free;
		strcpy(remote_display_buf, "queue");
		remote_user = to_user;
		remote_host = local_host;
	}
	else if (proto && !strcasecmp(proto, "park")) {
		local_user = to_user;
		local_user_param = switch_mprintf(";proto=%s", proto);
		event_status = "hold";
		if (skip_proto) {
			buf_to_free = switch_mprintf("sip:%s", to_user);
		} else {
			buf_to_free = switch_mprintf("sip:park+%s", to_user);
		}
		remote_uri = buf_to_free;
		strcpy(remote_display_buf, "park");
		remote_user = to_user;
		remote_host = local_host;
	}
	else if (proto && !strcasecmp(proto, "pickup")) {
		local_user = to_user;
		local_user_param = switch_mprintf(";proto=%s", proto);
		event_status = "hold";
		if (skip_proto) {
			buf_to_free = switch_mprintf("sip:%s", to_user);
		} else {
			buf_to_free = switch_mprintf("sip:pickup+%s", to_user);
		}
		remote_uri = buf_to_free;
		strcpy(remote_display_buf, "pickup");
		remote_user = to_user;
		remote_host = local_host;
	}
	else if (proto && !strcasecmp(proto, "conf")) {
		local_user = to_user;
		local_user_param = switch_mprintf(";proto=%s", proto);
		if (skip_proto) {
			buf_to_free = switch_mprintf("sip:%s@%s", to_user, host);
		} else {
			buf_to_free = switch_mprintf("sip:conf+%s@%s", to_user, host);
		}
		remote_uri = buf_to_free;
		strcpy(remote_display_buf, "conference");
		remote_user = to_user;
		remote_host = local_host;
	}
	else if (bInternal) {
		local_user = to_user;
		get_display_name_from_contact(contact, remote_display_buf);
		buf_to_free = sofia_glue_strip_uri(contact);
		remote_uri = buf_to_free;
		remote_user = contact_user;
		remote_host = contact_host;
	} else {
		local_user = from_user;
		buf_to_free = switch_mprintf("**%s@%s", from_user, local_host);
		remote_uri = buf_to_free;
		strcpy(remote_display_buf, to_user);
		remote_user = to_user;
		remote_host = local_host;
	}

	switch_snprintf(key, sizeof(key), "%s%s", user, host);
	data = switch_core_hash_find(h->hash, key);
	if (!data) {
		data = "";
	}
	tmp = switch_core_sprintf(h->pool, "%s"
							  "<dialog id=\"%s\" call-id=\"%s\" local-tag=\"%s\" remote-tag=\"%s\" direction=\"%s\">\n"
							  " <state>%s</state>\n"
							  " <local>\n"
							  "  <identity display=\"%s\">sip:%s@%s%s</identity>\n"
							  "  <target uri=\"sip:%s@%s\">\n"
							  "   <param pname=\"+sip.rendering\" pvalue=\"%s\"/>\n"
							  "  </target>\n"
							  " </local>\n"
							  " <remote>\n"
							  "  <identity display=\"%s\">sip:%s@%s</identity>\n"
							  "  <target uri=\"%s\"/>\n"
							  " </remote>\n"
							  "</dialog>\n",
							  data,
							  uuid, call_id, to_tag, from_tag, direction,
							  state,
							  local_user, local_user, local_host, local_user_param,
							  local_user, local_host,
							  !strcasecmp(event_status, "hold") ? "no" : "yes",
							  remote_display_buf, remote_user, remote_host,
							  remote_uri
							  );
	switch_core_hash_insert(h->hash, key, tmp);
	switch_safe_free(buf_to_free);

	h->rowcount++;

	return 0;
}

uint32_t sofia_presence_get_cseq(sofia_profile_t *profile)
{
	uint32_t callsequence;
	uint32_t now = (uint32_t) switch_epoch_time_now(NULL);

	switch_mutex_lock(profile->ireg_mutex);

	callsequence = (now - mod_sofia_globals.presence_epoch) * 100;

	if (profile->last_cseq && callsequence <= profile->last_cseq) {
		callsequence = ++profile->last_cseq;
	}

	profile->last_cseq = callsequence;

	switch_mutex_unlock(profile->ireg_mutex);

	return callsequence;

}


#define send_presence_notify(_a,_b,_c,_d,_e,_f,_g,_h,_i,_j,_k,_l) \
_send_presence_notify(_a,_b,_c,_d,_e,_f,_g,_h,_i,_j,_k,_l,__FILE__, __SWITCH_FUNC__, __LINE__)

static void _send_presence_notify(sofia_profile_t *profile,
								  const char *full_to,
								  const char *full_from,
								  const char *o_contact,
								  const char *expires,
								  const char *call_id,
								  const char *event,
								  const char *remote_ip,
								  const char *remote_port,
								  const char *ct,
								  const char *pl,
								  const char *call_info,
								  const char *file, const char *func, int line
								 )
{
	char sstr[128] = "";
	nua_handle_t *nh;
	int exptime = 0;
	char expires_str[10] = "";
	sip_cseq_t *cseq = NULL;
	uint32_t callsequence;
	uint32_t now = (uint32_t) switch_epoch_time_now(NULL);
	char *our_contact = profile->url, *our_contact_dup = NULL;

	sofia_destination_t *dst = NULL;
	char *contact_str, *contact, *user_via = NULL, *send_contact = NULL;
	char *route_uri = NULL, *o_contact_dup = NULL, *tmp, *to_uri, *dcs = NULL;
	const char *tp;
	char *cparams = NULL;
	char *path = NULL;

	if (zstr(full_to) || zstr(full_from) || zstr(o_contact)) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "MISSING DATA TO SEND NOTIFY.\n");
		return;
	}

	if ((cparams = strstr(o_contact, ";_;"))) {
		cparams += 3;
	}

	if (!switch_stristr("fs_nat=yes", o_contact)) {
		path = sofia_glue_get_path_from_contact((char *) o_contact);
	}

	tmp = (char *)o_contact;
	o_contact_dup = sofia_glue_get_url_from_contact(tmp, 1);


	if ((tp = switch_stristr("transport=", o_contact_dup))) {
		tp += 10;
	}

	if (zstr(tp)) {
		tp = "udp";
	}

	if (!switch_stristr("transport=", our_contact)) {
		our_contact_dup = switch_mprintf("<%s;transport=%s>", our_contact, tp);
		our_contact = our_contact_dup;
	}


   	if (!zstr(remote_ip) && sofia_glue_check_nat(profile, remote_ip)) {
		sofia_transport_t transport = sofia_glue_str2transport(tp);
		
		switch (transport) {
		case SOFIA_TRANSPORT_TCP:
			contact_str = profile->tcp_public_contact;
			break;
		case SOFIA_TRANSPORT_TCP_TLS:
			contact_str = profile->tls_public_contact;
			break;
		default:
			contact_str = profile->public_url;
			break;
		}
		user_via = sofia_glue_create_external_via(NULL, profile, transport);
	} else {
		sofia_transport_t transport = sofia_glue_str2transport(tp);
		switch (transport) {
		case SOFIA_TRANSPORT_TCP:
			contact_str = profile->tcp_contact;
			break;
		case SOFIA_TRANSPORT_TCP_TLS:
			contact_str = profile->tls_contact;
			break;
		default:
			contact_str = profile->url;
			break;
		}
	}


	if ((to_uri = sofia_glue_get_url_from_contact((char *)full_to, 1))) {
		char *p;

		if ((p = strstr(to_uri, "sip:"))) {
			char *q;

			p += 4;
			if ((q = strchr(p, '@'))) {
				*q++ = '\0';

				if ((dcs = switch_string_replace(contact_str, "mod_sofia", p))) {
					contact_str = dcs;
				}

			}
		}

		free(to_uri);
	}

	dst = sofia_glue_get_destination((char *) o_contact);
	switch_assert(dst);

	if (!zstr(dst->contact)) {
		contact = sofia_glue_get_url_from_contact(dst->contact, 1);
	} else {
		contact = strdup(o_contact);
	}

	if (dst->route_uri) {
		route_uri = sofia_glue_strip_uri(dst->route_uri);
	}

	if (expires) {
		long ltmp = atol(expires);

		if (ltmp > 0) {
			exptime = (ltmp - now);
		} else {
			exptime = 0;
		}
	}

	if (exptime <= 0) {
		switch_snprintf(sstr, sizeof(sstr), "terminated;reason=noresource");
	} else {
		switch_snprintf(sstr, sizeof(sstr), "active;expires=%u", (unsigned) exptime);
	}

	if (mod_sofia_globals.debug_presence > 1 || mod_sofia_globals.debug_sla > 1) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "SEND PRES NOTIFY:\n"
						  "file[%s]\nfunc[%s]\nline[%d]\n"
						  "profile[%s]\nvia[%s]\nip[%s]\nport[%s]\nroute[%s]\ncontact[%s]\nto[%s]\nfrom[%s]\nurl[%s]\ncall_id[%s]\nexpires_str[%s]\n"
						  "event[%s]\nct[%s]\npl[%s]\ncall_info[%s]\nexptime[%ld]\n",
						  file, func, line,
						  profile->name,
						  switch_str_nil(user_via),
						  remote_ip,
						  remote_port,
						  route_uri,
						  o_contact,
						  full_to,
						  full_from,
						  contact,
						  call_id,
						  expires_str,
						  event,
						  switch_str_nil(ct),
						  switch_str_nil(pl),
						  switch_str_nil(call_info),
						  (long)exptime
						  );
	}


	callsequence = sofia_presence_get_cseq(profile);

	if (cparams) {
		send_contact = switch_mprintf("%s;%s", contact_str, cparams);
		contact_str = send_contact;
	}

	nh = nua_handle(profile->nua, NULL, NUTAG_URL(contact), SIPTAG_CONTACT_STR(contact_str), TAG_END());
	cseq = sip_cseq_create(nh->nh_home, callsequence, SIP_METHOD_NOTIFY);
	nua_handle_bind(nh, &mod_sofia_globals.destroy_private);


	nua_notify(nh,
			   NUTAG_NEWSUB(1),
			   TAG_IF(route_uri, NUTAG_PROXY(route_uri)),
			   TAG_IF(dst->route, SIPTAG_ROUTE_STR(dst->route)),
			   TAG_IF(user_via, SIPTAG_VIA_STR(user_via)),
			   TAG_IF(path, SIPTAG_RECORD_ROUTE_STR(path)),

			   SIPTAG_FROM_STR(full_to),
			   SIPTAG_TO_STR(full_from),

			   SIPTAG_CALL_ID_STR(call_id),
			   TAG_IF(*expires_str, SIPTAG_EXPIRES_STR(expires_str)),
			   SIPTAG_SUBSCRIPTION_STATE_STR(sstr),
			   SIPTAG_EVENT_STR(event),
			   TAG_IF(!zstr(ct), SIPTAG_CONTENT_TYPE_STR(ct)),
			   TAG_IF(!zstr(pl), SIPTAG_PAYLOAD_STR(pl)),
			   TAG_IF(!zstr(call_info), SIPTAG_CALL_INFO_STR(call_info)),
			   TAG_IF(!exptime, SIPTAG_EXPIRES_STR("0")),
			   SIPTAG_CSEQ(cseq),
			   TAG_END());


	switch_safe_free(route_uri);
	switch_safe_free(dcs);
	switch_safe_free(contact);

	sofia_glue_free_destination(dst);
	switch_safe_free(user_via);
	switch_safe_free(o_contact_dup);
	switch_safe_free(send_contact);
	switch_safe_free(our_contact_dup);
	switch_safe_free(path);
}


static int sofia_dialog_probe_notify_callback(void *pArg, int argc, char **argv, char **columnNames)
{
	struct rfc4235_helper *sh = (struct rfc4235_helper *) pArg;
	char key[256] = "";
	char *data = NULL;
	char *call_id = argv[0];
	char *expires = argv[1];
	char *user = argv[2];
	char *host = argv[3];
	char *event = argv[4];
	char *version = argv[5];
	char *notify_state = argv[6];
	char *full_to = argv[7];
	char *full_from = argv[8];
	char *contact = argv[9];
	char *remote_ip = argv[10];
	char *remote_port = argv[11];

	switch_stream_handle_t stream = { 0 };
	char *to;
	const char *pl = NULL;
	const char *ct = "application/dialog-info+xml";

	if (mod_sofia_globals.debug_presence > 0) {
		int i;
		for(i = 0; i < argc; i++) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "arg %d[%s] = [%s]\n", i, columnNames[i], argv[i]);
		}
	}


	if (mod_sofia_globals.debug_presence > 0) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE,
						  "SEND DIALOG\nTo:      \t%s@%s\nFrom:    \t%s@%s\nCall-ID:  \t%s\n",
						  user, host, user, host, call_id);
	}

	to = switch_mprintf("sip:%s@%s", user, host);

	SWITCH_STANDARD_STREAM(stream);

	if (zstr(version)) {
		version = "0";
	}

	stream.write_function(&stream,
						  "<?xml version=\"1.0\"?>\n"
						  "<dialog-info xmlns=\"urn:ietf:params:xml:ns:dialog-info\" "
						  "version=\"%s\" state=\"%s\" entity=\"%s\">\n",
						  version,
						  notify_state, to);

	switch_snprintf(key, sizeof(key), "%s%s", user, host);

	data = switch_core_hash_find(sh->hash, key);

	if (data) {
		stream.write_function(&stream, "%s\n", data);
	}

	stream.write_function(&stream, "</dialog-info>\n");
	pl = stream.data;
	ct = "application/dialog-info+xml";

	if (mod_sofia_globals.debug_presence > 0 && pl) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "send payload:\n%s\n", pl);
	}


	send_presence_notify(sh->profile,
						 full_to,
						 full_from,
						 contact,
						 expires,
						 call_id,
						 event,
						 remote_ip,
						 remote_port,
						 ct,
						 pl,
						 NULL
						 );


	switch_safe_free(to);
	switch_safe_free(stream.data);

	return 0;
}

static char *translate_rpid(char *in)
{
	char *r = in;

	if (in && (strstr(in, "null") || strstr(in, "NULL"))) {
		in = NULL;
	}

	if (zstr(in)) {
		return NULL;
	}

	if (!strcasecmp(in, "unknown")) {
		r = NULL;
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


static char *gen_pidf(char *user_agent, char *id, char *url, char *open, char *rpid, char *prpid, char *status, const char **ct)
{
	char *ret = NULL;

	if (switch_stristr("polycom", user_agent)) {
		*ct = "application/xpidf+xml";

		/* If unknown/none prpid is provided, just show the user as online. */
		if (!prpid || !strcasecmp(prpid, "unknown")) {
			prpid = "online";
		}

		/* of course!, lets make a big deal over dashes. Now the stupidity is complete. */
		if (!strcmp(prpid, "on-the-phone")) {
			prpid = "onthephone";
		}

		if (zstr(open)) {
			open = "open";
		}

		ret = switch_mprintf("<?xml version=\"1.0\"?>\n"
							 "<!DOCTYPE presence PUBLIC \"-//IETF//DTD RFCxxxx XPIDF 1.0//EN\" \"xpidf.dtd\">\n"
							 "<presence>\n"
							 " <status>\n"
							 "  <note>%s</note>\n"
							 " </status>\n"
							 " <presentity uri=\"%s;method=SUBSCRIBE\" />\n"
							 " <atom id=\"%s\">\n"
							 "  <address uri=\"%s;user=ip\" priority=\"0.800000\">\n"
							 "   <status status=\"%s\" />\n"
							 "   <msnsubstatus substatus=\"%s\" />\n"
							 "  </address>\n"
							 " </atom>\n"
							 "</presence>\n", status, id, id, url, open, prpid);
	} else {
		char *xml_rpid = NULL;

		*ct = "application/pidf+xml";

		if (!strcasecmp(open, "closed")) {
			status = "Unregistered";
			prpid = NULL;
		}

		if (!strncasecmp(status, "Registered", 10)) {
			status = "Available";
		}

		if (!strcasecmp(status, "Available")) {
			prpid = NULL;
		}


		if (!strcasecmp(status, "Unregistered")) {
			prpid = NULL;
			open = "closed";
		}

		if (zstr(rpid)) {
			prpid = NULL;
		}


		if (zstr(status) && !zstr(prpid)) {
			status = "Available";
			prpid = NULL;
		}

		if (prpid) {
			xml_rpid = switch_mprintf("  <rpid:activities>\r\n"
									  "   <rpid:%s/>\n"
									  "  </rpid:activities>\n", prpid);
		}

		ret = switch_mprintf("<?xml version=\"1.0\" encoding=\"ISO-8859-1\"?> \n"
							 "<presence xmlns='urn:ietf:params:xml:ns:pidf' \n"
							 "xmlns:dm='urn:ietf:params:xml:ns:pidf:data-model' \n"
							 "xmlns:rpid='urn:ietf:params:xml:ns:pidf:rpid' \n"
							 "xmlns:c='urn:ietf:params:xml:ns:pidf:cipid' entity='%s'>\n"
							 " <tuple id='t6a5ed77e'>\n"
							 "  <status>\r\n"
							 "   <basic>%s</basic>\n"
							 "  </status>\n"
							 " </tuple>\n"
							 " <dm:person id='p06360c4a'>\n"
							 "%s"
							 "  <dm:note>%s</dm:note>\n"
							 " </dm:person>\n"
							 "</presence>", id, open, switch_str_nil(xml_rpid), status);


		switch_safe_free(xml_rpid);
	}


	return ret;
}

static int sofia_presence_sub_callback(void *pArg, int argc, char **argv, char **columnNames)
{
	struct presence_helper *helper = (struct presence_helper *) pArg;
	char *pl = NULL;
	char *clean_id = NULL, *id = NULL;
	char *proto = argv[0];
	char *user = argv[1];
	char *host = argv[2];
	char *sub_to_user = argv[3];
	char *event = argv[5];
	char *contact = argv[6];
	char *call_id = argv[7];
	char *full_from = argv[8];
	//char *full_via = argv[9];
	char *expires = argv[10];
	char *user_agent = argv[11];
	char *profile_name = argv[13];
	uint32_t in = 0;
	char *status = argv[14];
	char *rpid = argv[15];
	char *sub_to_host = argv[16];
	char *open_closed = NULL;
	char *dialog_status = NULL;
	char *dialog_rpid = NULL;
	//char *default_dialog = "partial";
	char *default_dialog = "full";
	const char *ct = "no/idea";

	char *to = NULL;
	char *open;
	char *prpid;

	int is_dialog = 0;
	sofia_profile_t *ext_profile = NULL, *profile = helper->profile;

	char status_line[256] = "";
	char *version = "0";
	char *presence_id = NULL;
	char *free_me = NULL;
	int holding = 0;
	char *orig_proto = NULL;
	int skip_proto = 0;
	char *full_to = NULL;
	char *ip = NULL;
	char *port = 0;
	const char *call_state = NULL;
	const char *astate = NULL;
	const char *event_status = NULL;
	const char *force_event_status = NULL;
	char *contact_str, *contact_stripped;

	if (mod_sofia_globals.debug_presence > 0) {
		int i;
		for(i = 0; i < argc; i++) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "arg %d[%s] = [%s]\n", i, columnNames[i], argv[i]);
		}
		DUMP_EVENT(helper->event);
	}

	if (argc > 18) {
		if (!zstr(argv[17])) {
			status = argv[17];
		}
		if (!zstr(argv[18])) {
			rpid = argv[18];
		}
		open_closed = argv[19];
	}

	if (argc > 20) {
		dialog_status = argv[20];
		dialog_rpid = argv[21];
		version = argv[22];
		presence_id = argv[23];
		orig_proto = argv[24];
		full_to = argv[25];
		ip = argv[26];
		port = argv[27];
	}

	if (!zstr(ip) && sofia_glue_check_nat(profile, ip)) {
		char *ptr;
		if ((ptr = sofia_glue_find_parameter(contact, "transport="))) {
			sofia_transport_t transport = sofia_glue_str2transport( ptr + 10 );

			switch (transport) {
			case SOFIA_TRANSPORT_TCP:
				contact_str = profile->tcp_public_contact;
				break;
			case SOFIA_TRANSPORT_TCP_TLS:
				contact_str = profile->tls_public_contact;
				break;
			default:
				contact_str = profile->public_url;
				break;
			}
		} else {
			contact_str = profile->public_url;		
		}
	} else {
		char *ptr;
		if ((ptr = sofia_glue_find_parameter(contact, "transport="))) {
			sofia_transport_t transport = sofia_glue_str2transport( ptr + 10 );

			switch (transport) {
			case SOFIA_TRANSPORT_TCP:
				contact_str = profile->tcp_contact;
				break;
			case SOFIA_TRANSPORT_TCP_TLS:
				contact_str = profile->tls_contact;
				break;
			default:
				contact_str = profile->url;
				break;
			}
		} else {
			contact_str = profile->url;
		}
	}


	if (!zstr(presence_id) && strchr(presence_id, '@')) {
		char *p;

		free_me = strdup(presence_id);

		if ((p = strchr(free_me, '@'))) {
			*p = '\0';
		}

		user = free_me;
	}


	if (!zstr(orig_proto) && !strcmp(orig_proto, SOFIA_CHAT_PROTO)) {
		skip_proto = 1;
	}

	in = helper->event && helper->event->event_id == SWITCH_EVENT_PRESENCE_IN;

	if (zstr(rpid)) {
		rpid = "unknown";
	}

	if (zstr(status)) {
		if (!strcasecmp(rpid, "busy")) {
			status = "Busy";
		} else if (!strcasecmp(rpid, "unavailable")) {
			status = "Idle";
		} else if (!strcasecmp(rpid, "away")) {
			status = "Idle";
		} else {
			status = "Available";
		}
	}

	if (status && !strncasecmp(status, "hold", 4)) {
		holding = 1;
	}

	if (profile_name && strcasecmp(profile_name, helper->profile->name)) {
		if ((ext_profile = sofia_glue_find_profile(profile_name))) {
			profile = ext_profile;
		}
	}


	if (!strcasecmp(proto, SOFIA_CHAT_PROTO) || skip_proto) {
		clean_id = switch_mprintf("sip:%s@%s", sub_to_user, sub_to_host);
	} else {
		clean_id = switch_mprintf("sip:%s+%s@%s", proto, sub_to_user, sub_to_host);
	}



	if (!rpid) {
		rpid = "unknown";
	}

	//	if (!strcasecmp(proto, SOFIA_CHAT_PROTO) || skip_proto) {
	//		clean_id = switch_mprintf("sip:%s@%s", sub_to_user, sub_to_host);
	//} else {
	//		clean_id = switch_mprintf("sip:%s+%s@%s", proto, sub_to_user, sub_to_host);
	//}

	if (mod_sofia_globals.debug_presence > 0) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE,
						  "SEND PRESENCE\nTo:      \t%s@%s\nFrom:    \t%s@%s\nCall-ID:  \t%s\nProfile:\t%s [%s]\n\n",
						  user, host, sub_to_user, sub_to_host, call_id, profile_name, helper->profile->name);
	}

	if (!strcasecmp(sub_to_host, host) && !skip_proto) {
		/* same host */
		id = switch_mprintf("sip:%s+%s@%s", proto, sub_to_user, sub_to_host);
	} else if (strcasecmp(proto, SOFIA_CHAT_PROTO) && !skip_proto) {
		/*encapsulate */
		id = switch_mprintf("sip:%s+%s+%s@%s", proto, sub_to_user, sub_to_host, host);
	} else {
		id = switch_mprintf("sip:%s@%s", sub_to_user, sub_to_host);
	}

	to = switch_mprintf("sip:%s@%s", user, host);

	is_dialog = !strcmp(event, "dialog");

	if (helper->hup && helper->calls_up > 0) {
		call_state = "CS_EXECUTE";
		astate = "active";
		event_status = "Active";
		force_event_status = NULL;
	} else {
		if (helper->event) {
			call_state = switch_event_get_header(helper->event, "channel-state");
			astate = switch_str_nil(switch_event_get_header(helper->event, "astate"));
			event_status = switch_str_nil(switch_event_get_header(helper->event, "status"));
			force_event_status = switch_str_nil(switch_event_get_header(helper->event, "force-status"));
		}
	}

	if (helper->event) {
		switch_stream_handle_t stream = { 0 };
		const char *direction = switch_str_nil(switch_event_get_header(helper->event, "presence-call-direction"));
		//const char *force_direction = switch_str_nil(switch_event_get_header(helper->event, "force-direction"));
		const char *uuid = switch_str_nil(switch_event_get_header(helper->event, "unique-id"));
		const char *resub = switch_str_nil(switch_event_get_header(helper->event, "resub"));
		const char *answer_state = switch_str_nil(switch_event_get_header(helper->event, "answer-state"));
		const char *dft_state;
		const char *from_id = NULL, *from_name = NULL;
		const char *to_user = switch_str_nil(switch_event_get_header(helper->event, "variable_sip_to_user"));
		const char *from_user = switch_str_nil(switch_event_get_header(helper->event, "variable_sip_from_user"));
		const char *disable_early = switch_str_nil(switch_event_get_header(helper->event, "variable_presence_disable_early"));
		const char *answer_epoch = switch_str_nil(switch_event_get_header(helper->event, "variable_answer_epoch"));
		int answered = 0;
		char *clean_to_user = NULL;
		char *clean_from_user = NULL;
		int force_status = 0;
		int term = 0;

		if (answer_epoch) {
			answered = atoi(answer_epoch);
		}


		//if (user_agent && switch_stristr("snom", user_agent) && uuid) {
		//	default_dialog = "full" ;
		//}

		if (call_state && !strcasecmp(call_state, "cs_hangup")) {
			astate = "hangup";
			holding = 0;
			term = 1;
		} else {

			if (event_status && !strncasecmp(event_status, "hold", 4)) {
				holding = 1;
			}

			if (force_event_status && !event_status) {
				event_status = force_event_status;
			}

			if (event_status && !strncasecmp(event_status, "hold", 4)) {
				holding = 1;
			}
		}

		if (!strcasecmp(direction, "inbound")) {
			from_id = switch_str_nil(switch_event_get_header(helper->event, "Caller-Destination-Number"));

		} else {
			from_id = switch_str_nil(switch_event_get_header(helper->event, "Caller-Caller-ID-Number"));
			from_name = switch_event_get_header(helper->event, "Caller-Caller-ID-Name");

			if (zstr(from_id)) {
				from_id = switch_str_nil(switch_event_get_header(helper->event, "Other-Leg-Caller-ID-Number"));
			}

			if (zstr(from_name)) {
				from_name = switch_event_get_header(helper->event, "Other-Leg-Caller-ID-Name");
			}

		}

#if 0
		char *buf;
		switch_event_serialize(helper->event, &buf, SWITCH_FALSE);
		switch_assert(buf);
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "CHANNEL_DATA:\n%s\n", buf);
		free(buf);
#endif

		if (is_dialog) {
			SWITCH_STANDARD_STREAM(stream);
		}

		if (is_dialog) {
			// Usually we report the dialogs FROM the probed user.  The exception is when the monitored endpoint is internal,
			// and its presence_id is set in the dialplan.  Reverse the direction if this is not a registered entity.
			const char *caller = switch_str_nil(switch_event_get_header(helper->event, "caller-username"));
			if (!strcmp(direction, "inbound") && strcmp(sub_to_user,  caller)) {
				// If inbound and the entity is not the caller (i.e. internal to FS), then the direction is reversed
				// because it is not going through the B2BUA
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "sofia_presence_sub_callback: endpt is internal\n");
				direction = !strcasecmp(direction, "outbound") ? "inbound" : "outbound";
			}

		}

		if (!strcasecmp(direction, "outbound")) {
			direction = "recipient";
			dft_state = "early";
		} else {
			direction = "initiator";
			dft_state = "confirmed";
		}

		if (is_dialog) {
			if (zstr(version)) {
				version = "0";
			}

			stream.write_function(&stream,
								  "<?xml version=\"1.0\"?>\n"
								  "<dialog-info xmlns=\"urn:ietf:params:xml:ns:dialog-info\" "
								  "version=\"%s\" state=\"%s\" entity=\"%s\">\n", version, default_dialog, clean_id);

		}

		if (!zstr(uuid)) {
			if (!zstr(answer_state)) {
				astate = answer_state;
			}

			if (zstr(astate)) {
				if (is_dialog) {
					astate = dft_state;
				} else {
					astate = "terminated";
				}
			}

			if (!strcasecmp(astate, "answered")) {
				astate = "confirmed";
			}


			if (is_dialog) {

				if (!strcasecmp(astate, "ringing")) {
					if (!strcasecmp(direction, "recipient")) {
						astate = "early";
					} else {
						astate = "confirmed";
					}
				}

				if (holding) {
					if (profile->pres_held_type == PRES_HELD_CONFIRMED) {
						astate = "confirmed";
					} else if (profile->pres_held_type == PRES_HELD_TERMINATED) {
						astate = "terminated";
					} else {
						astate = "early";
					}
				}


				if (!strcasecmp(astate, "hangup")) {
					astate = "terminated";
				}

				stream.write_function(&stream, "<dialog id=\"%s\" direction=\"%s\">\n", uuid, direction);
				stream.write_function(&stream, "<state>%s</state>\n", astate);
			} else {
				if (!strcasecmp(astate, "ringing")) {
					astate = "early";
				}
			}


			if ((sofia_test_pflag(profile, PFLAG_PRESENCE_DISABLE_EARLY) || switch_true(disable_early)) &&
				((!zstr(astate) && (!strcasecmp(astate, "early") || !strcasecmp(astate, "ringing") || (!strcasecmp(astate, "terminated") && !answered))))) {
				switch_safe_free(stream.data);
				goto end;
			}

			if (!strcasecmp(astate, "early") || !strcasecmp(astate, "confirmed")) {

				clean_to_user = switch_mprintf("%s", sub_to_user ? sub_to_user : to_user);
				clean_from_user = switch_mprintf("%s", from_id ? from_id : from_user);

				if (is_dialog) {
					if (!zstr(clean_to_user) && !zstr(clean_from_user)) {
						stream.write_function(&stream, "<local>\n<identity display=\"%s\">sip:%s@%s</identity>\n", clean_to_user, clean_to_user, host);
						stream.write_function(&stream, "<target uri=\"sip:%s@%s\">\n", clean_to_user, host);
						stream.write_function(&stream, "<param pname=\"+sip.rendering\" pvalue=\"%s\"/>\n", holding ? "no" : "yes");

						stream.write_function(&stream, "</target>\n</local>\n");
						if (switch_true(switch_event_get_header(helper->event, "Presence-Privacy"))) {
							stream.write_function(&stream, "<remote>\n<identity display=\"Anonymous\">sip:anonymous@anonymous.invalid</identity>\n");
						} else {
							stream.write_function(&stream, "<remote>\n<identity display=\"%s\">sip:%s@%s</identity>\n",
												  from_name ? from_name : clean_from_user, clean_from_user,
												  host);
						}
						stream.write_function(&stream, "<target uri=\"sip:**%s@%s\"/>\n", clean_to_user, host);
						stream.write_function(&stream, "</remote>\n");

					} else if (!strcasecmp(proto, "queue")) {
						stream.write_function(&stream, "<local>\n<identity display=\"queue\">sip:%s@%s;proto=queue</identity>\n",
											  !zstr(clean_to_user) ? clean_to_user : "unknown", host);
						stream.write_function(&stream, "<target uri=\"sip:%s@%s;proto=fifo\">\n", !zstr(clean_to_user) ? clean_to_user : "unknown", host);
						stream.write_function(&stream, "<param pname=\"+sip.rendering\" pvalue=\"no\"/>\n</target>\n</local>\n");
						stream.write_function(&stream, "<remote>\n<identity display=\"queue\">sip:%s</identity>\n", uuid);
						if (skip_proto) {
							stream.write_function(&stream, "<target uri=\"sip:%s\"/>\n", uuid);
						} else {
							stream.write_function(&stream, "<target uri=\"sip:queue+%s\"/>\n", uuid);
						}

						stream.write_function(&stream, "</remote>\n");
					} else if (!strcasecmp(proto, "park")) {
						stream.write_function(&stream, "<local>\n<identity display=\"park\">sip:%s@%s;proto=park</identity>\n",
											  !zstr(clean_to_user) ? clean_to_user : "unknown", host);
						stream.write_function(&stream, "<target uri=\"sip:%s@%s;proto=park\">\n", !zstr(clean_to_user) ? clean_to_user : "unknown", host);
						stream.write_function(&stream, "<param pname=\"+sip.rendering\" pvalue=\"no\"/>\n</target>\n</local>\n");
						stream.write_function(&stream, "<remote>\n<identity display=\"park\">sip:%s</identity>\n", uuid);
						if (skip_proto) {
							stream.write_function(&stream, "<target uri=\"sip:%s\"/>\n", uuid);
						} else {
							stream.write_function(&stream, "<target uri=\"sip:park+%s\"/>\n", uuid);
						}
						stream.write_function(&stream, "</remote>\n");
					} else if (!strcasecmp(proto, "pickup")) {
						stream.write_function(&stream, "<local>\n<identity display=\"pickup\">sip:%s@%s;proto=pickup</identity>\n",
											  !zstr(clean_to_user) ? clean_to_user : "unknown", host);
						stream.write_function(&stream, "<target uri=\"sip:%s@%s;proto=pickup\">\n", !zstr(clean_to_user) ? clean_to_user : "unknown", host);
						stream.write_function(&stream, "<param pname=\"+sip.rendering\" pvalue=\"no\"/>\n</target>\n</local>\n");
						stream.write_function(&stream, "<remote>\n<identity display=\"pickup\">sip:%s</identity>\n", uuid);
						if (skip_proto) {
							stream.write_function(&stream, "<target uri=\"sip:%s\"/>\n", uuid);
						} else {
							stream.write_function(&stream, "<target uri=\"sip:pickup+%s\"/>\n", uuid);
						}
						stream.write_function(&stream, "</remote>\n");
					} else if (!strcasecmp(proto, "conf")) {
						stream.write_function(&stream, "<local>\n<identity display=\"conference\">sip:%s@%s;proto=conference</identity>\n",
											  !zstr(clean_to_user) ? clean_to_user : "unknown", host);
						stream.write_function(&stream, "<target uri=\"sip:%s@%s;proto=conference\">\n",
											  !zstr(clean_to_user) ? clean_to_user : "unknown", host);
						stream.write_function(&stream, "<param pname=\"+sip.rendering\" pvalue=\"yes\"/>\n</target>\n</local>\n");
						stream.write_function(&stream, "<remote>\n<identity display=\"conference\">sip:%s@%s</identity>\n", uuid, host);
						if (skip_proto) {
							stream.write_function(&stream, "<target uri=\"sip:%s@%s\"/>\n", uuid, host);
						} else {
							stream.write_function(&stream, "<target uri=\"sip:conf+%s@%s\"/>\n", uuid, host);
						}
						stream.write_function(&stream, "</remote>\n");
					}
				}

				switch_safe_free(clean_to_user);
				switch_safe_free(clean_from_user);
			}
			if (is_dialog) {
				stream.write_function(&stream, "</dialog>\n");
			}
		}

		if (is_dialog) {
			stream.write_function(&stream, "</dialog-info>\n");
			pl = stream.data;
			ct = "application/dialog-info+xml";
		}

		if (!zstr(astate) && !zstr(uuid) &&
			helper && helper->stream.data && strcmp(helper->last_uuid, uuid) && strcasecmp(astate, "terminated") && strchr(uuid, '-')) {
			helper->stream.write_function(&helper->stream, "update sip_dialogs set state='%s' where hostname='%q' and profile_name='%q' and uuid='%s';",
										  astate, mod_sofia_globals.hostname, profile->name, uuid);
			switch_copy_string(helper->last_uuid, uuid, sizeof(helper->last_uuid));
		}

		if (zstr(astate)) astate = "";

		if (!is_dialog) {
			switch_set_string(status_line, status);

			if (in) {
				open = "open";

				if (switch_false(resub)) {
					const char *direction = switch_event_get_header(helper->event, "Caller-Direction");
					const char *op, *what = "Ring";

					if (direction && !strcasecmp(direction, "outbound")) {
						op = switch_event_get_header(helper->event, "Other-Leg-Caller-ID-Number");
					} else {
						op = switch_event_get_header(helper->event, "Caller-Callee-ID-Number");
					}

					if (zstr(op)) {
						op = switch_event_get_header(helper->event, "Caller-Destination-Number");
					}

					if (direction) {
						what = strcasecmp(direction, "outbound") ? "Call" : "Ring";
					}

					if (!strcmp(astate, "early")) {
						if (!zstr(op)) {
							//switch_snprintf(status_line, sizeof(status_line), "%sing", what);
							//} else {
							if (sofia_test_pflag(profile, PFLAG_PRESENCE_PRIVACY)) {
								switch_snprintf(status_line, sizeof(status_line), "%s", what);
							} else {
								switch_snprintf(status_line, sizeof(status_line), "%s %s", what, op);
							}
						}

						rpid = "on-the-phone";
						force_status = 1;

					} else if (!strcmp(astate, "confirmed")) {
						if (!zstr(op)) {
							if (sofia_test_pflag(profile, PFLAG_PRESENCE_PRIVACY)) {
								switch_snprintf(status_line, sizeof(status_line), "On The Phone");
							} else {
								switch_snprintf(status_line, sizeof(status_line), "Talk %s", op);
							}
						} else {
							switch_snprintf(status_line, sizeof(status_line), "On The Phone");
						}

						rpid = "on-the-phone";
						force_status = 1;
					} else if (!strcmp(astate, "terminated") || !strcmp(astate, "hangup")) {
						//rpid = "online";
						//dialog_rpid = "";
						//force_event_status = "Available";
						term = 1;
					}

					if (!term && !strcmp(status, "hold")) {
						rpid = "on-the-phone";
						if (!zstr(op)) {
							if (sofia_test_pflag(profile, PFLAG_PRESENCE_PRIVACY)) {
								switch_snprintf(status_line, sizeof(status_line), "Hold");
							} else {
								switch_snprintf(status_line, sizeof(status_line), "Hold %s", op);
							}
							force_status = 1;
						}
					}
				}
			} else {
				open = "closed";
			}

			if (!zstr(open_closed)) {
				open = open_closed;
			}

			prpid = translate_rpid(rpid);

			if (!zstr(dialog_status) && !force_status) {
				status = dialog_status;
				switch_set_string(status_line, status);
			}

			if (!zstr(force_event_status)) {
				switch_set_string(status_line, force_event_status);
			}

			if (!zstr(dialog_rpid)) {
				prpid = rpid = dialog_rpid;
			}

			contact_stripped = sofia_glue_strip_uri(contact_str);
			pl = gen_pidf(user_agent, clean_id, contact_stripped, open, rpid, prpid, status_line, &ct);
			free(contact_stripped);
		}

	} else {
		if (in) {
			open = "open";
		} else {
			open = "closed";
		}

		if (!zstr(open_closed)) {
			open = open_closed;
		}

		prpid = translate_rpid(rpid);

		if (!zstr(dialog_status)) {
			status = dialog_status;
		}

		if (!zstr(dialog_rpid)) {
			prpid = rpid = dialog_rpid;
		}

		contact_stripped = sofia_glue_strip_uri(contact_str); 
		pl = gen_pidf(user_agent, clean_id, contact_stripped, open, rpid, prpid, status, &ct);
		free(contact_stripped);
	}


	if (!is_dialog && helper->event && !switch_stristr("registered", status_line)){
		const char *uuid = switch_event_get_header_nil(helper->event, "unique-id");
		const char *register_source = switch_event_get_header_nil(helper->event, "register-source");

		if (!zstr(uuid) && strchr(uuid, '-') && !zstr(status_line) && !zstr(rpid) && (zstr(register_source) || strcasecmp(register_source, "register"))) {
			char *sql = switch_mprintf("update sip_dialogs set rpid='%q',status='%q' where hostname='%q' and profile_name='%q' and uuid='%q'",
									   rpid, status_line,
									   mod_sofia_globals.hostname, profile->name, uuid);
			sofia_glue_execute_sql(profile, &sql, SWITCH_TRUE);
		}
	}

	send_presence_notify(profile, full_to, full_from, contact, expires, call_id, event, ip, port, ct, pl, NULL);


 end:

	switch_safe_free(free_me);

	if (ext_profile) {
		sofia_glue_release_profile(ext_profile);
	}

	switch_safe_free(id);
	switch_safe_free(clean_id);
	switch_safe_free(pl);
	switch_safe_free(to);

	return 0;
}

static int sofia_presence_mwi_callback(void *pArg, int argc, char **argv, char **columnNames)
{
	//char *sub_to_user = argv[3];
	//char *sub_to_host = argv[4];
	char *event = argv[5];
	char *contact = argv[6];
	char *call_id = argv[7];
	char *full_from = argv[8];
	char *expires = argv[10];
	char *profile_name = argv[13];
	char *body = argv[15];
	char *full_to = argv[16];
	char *remote_ip = argv[17];
	char *remote_port = argv[18];

	struct mwi_helper *h = (struct mwi_helper *) pArg;
	sofia_profile_t *ext_profile = NULL, *profile = h->profile;


	if (mod_sofia_globals.debug_presence > 0) {
		int i;
		for(i = 0; i < argc; i++) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "arg %d[%s] = [%s]\n", i, columnNames[i], argv[i]);
		}
	}

	if (profile_name && strcasecmp(profile_name, h->profile->name)) {
		if ((ext_profile = sofia_glue_find_profile(profile_name))) {
			profile = ext_profile;
		}
	}

	send_presence_notify(profile,
						 full_to,
						 full_from,
						 contact,
						 expires,
						 call_id,
						 event,
						 remote_ip,
						 remote_port,
						 "application/simple-message-summary",
						 body,
						 NULL
						 );


	h->total++;

	if (ext_profile) {
		sofia_glue_release_profile(ext_profile);
	}

	return 0;
}

static int sofia_presence_mwi_callback2(void *pArg, int argc, char **argv, char **columnNames)
{
	const char *user = argv[0];
	const char *host = argv[1];
	const char *event = "message-summary";
	const char *contenttype = "application/simple-message-summary";
	const char *body = argv[5];
	const char *o_contact = argv[2];
	const char *network_ip = argv[4];
	const char *call_id = argv[6];

	char *profile_name = argv[3];
	struct mwi_helper *h = (struct mwi_helper *) pArg;
	sofia_profile_t *ext_profile = NULL, *profile = h->profile;

	if (profile_name && strcasecmp(profile_name, h->profile->name)) {
		if ((ext_profile = sofia_glue_find_profile(profile_name))) {
			profile = ext_profile;
		}
	}

	if (!sofia_test_pflag(profile, PFLAG_MWI_USE_REG_CALLID)) {
		call_id = NULL;
	}

	sofia_glue_send_notify(profile, user, host, event, contenttype, body, o_contact, network_ip, call_id);

	if (ext_profile) {
		sofia_glue_release_profile(ext_profile);
	}

	return 0;
}

static int broadsoft_sla_notify_callback(void *pArg, int argc, char **argv, char **columnNames)
{
	struct state_helper *sh = (struct state_helper *) pArg;
	char key[256] = "";
	char *data = NULL, *tmp;
	char *call_id = argv[0];
	//char *expires = argv[1];
	char *user = argv[2];
	char *host = argv[3];
	char *event = argv[4];
	int i;


	if (mod_sofia_globals.debug_sla > 1) {
		for (i = 0; i < argc; i++) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "SLA3: %d [%s]=[%s]\n", i, columnNames[i], argv[i]);
		}
	}

	switch_snprintf(key, sizeof(key), "%s%s", user, host);
	data = switch_core_hash_find(sh->hash, key);

	if (data) {
		tmp = switch_core_sprintf(sh->pool, "%s,<sip:%s>;appearance-index=*;appearance-state=idle", data, host);
	} else {
		tmp = switch_core_sprintf(sh->pool, "<sip:%s>;appearance-index=*;appearance-state=idle", host);
	}


	if (!strcasecmp(event, "line-seize")) {
		char *hack;

		if ((hack = (char *) switch_stristr("=seized", tmp))) {
			switch_snprintf(hack, 7, "=idle  ");
		}
	}

	if (mod_sofia_globals.debug_sla > 1) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "DB PRES NOTIFY: [%s]\n[%s]\n[%s]\n[%s]\n[%s]\n[%s]\n[%s]\n[%s]\n[%s]\n",
						  argv[5], argv[6], argv[7], argv[8], call_id, event, argv[9], argv[10], tmp);

	}

	send_presence_notify(sh->profile, argv[5], argv[6], argv[7], argv[8], call_id, event, argv[9], argv[10], NULL, NULL, tmp);

	sh->total++;

	return 0;
}

static int broadsoft_sla_gather_state_callback(void *pArg, int argc, char **argv, char **columnNames)
{
	struct state_helper *sh = (struct state_helper *) pArg;
	char key[256] = "";
	switch_core_session_t *session;
	const char *callee_name = NULL, *callee_number = NULL;
	char *data = NULL, *tmp;
	char *user = argv[0];
	char *host = argv[1];
	char *info = argv[2];
	char *state = argv[3];
	char *uuid = argv[4];
	int i;

	if (mod_sofia_globals.debug_sla > 1) {
		for (i = 0; i < argc; i++) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "SLA2: %d [%s]=[%s]\n", i, columnNames[i], argv[i]);
		}
	}

	if (zstr(info)) {
		return 0;
	}

	if (zstr(state)) {
		state = "idle";
	}

	switch_snprintf(key, sizeof(key), "%s%s", user, host);

	data = switch_core_hash_find(sh->hash, key);

	if (strcasecmp(state, "idle") && uuid && (session = switch_core_session_locate(uuid))) {
		switch_channel_t *channel = switch_core_session_get_channel(session);

		if (switch_channel_test_flag(channel, CF_ORIGINATOR) || switch_channel_test_flag(channel, CF_BRIDGE_ORIGINATOR) ||
			switch_channel_inbound_display(channel) || switch_channel_test_flag(channel, CF_SLA_BARGING)) {
			callee_name = switch_channel_get_variable(channel, "callee_id_name");
			callee_number = switch_channel_get_variable(channel, "callee_id_number");

			if (zstr(callee_number)) {
				callee_number = switch_channel_get_variable(channel, "destination_number");
			}

		} else {
			callee_name = switch_channel_get_variable(channel, "caller_id_name");
			callee_number = switch_channel_get_variable(channel, "caller_id_number");
		}

		if (zstr(callee_name) && !zstr(callee_number)) {
			callee_name = callee_number;
		}

		if (!zstr(callee_number)) {
			callee_number = switch_sanitize_number(switch_core_session_strdup(session, callee_number));
		}

		if (!zstr(callee_name)) {
			char *tmp = switch_core_session_strdup(session, callee_name);
			switch_url_decode(tmp);
			callee_name = switch_sanitize_number(tmp);
		}


		//if (switch_channel_get_state(channel) != CS_EXECUTE) {
			//callee_number = NULL;
		//}

		switch_core_session_rwunlock(session);
	}

	if (data && strstr(data, info)) {
		return 0;
	}


	if (!zstr(callee_number)) {
		if (zstr(callee_name)) {
			callee_name = "unknown";
		}

		if (data) {
			tmp = switch_core_sprintf(sh->pool,
									  "%s,<sip:%s>;%s;appearance-state=%s;appearance-uri=\"\\\"%s\\\" <sip:%s@%s>\"",
									  data, host, info, state, callee_name, callee_number, host);
		} else {
			tmp = switch_core_sprintf(sh->pool,
									  "<sip:%s>;%s;appearance-state=%s;appearance-uri=\"\\\"%s\\\" <sip:%s@%s>\"",
									  host, info, state, callee_name, callee_number, host);
		}
	} else {
		if (data) {
			tmp = switch_core_sprintf(sh->pool, "%s,<sip:%s>;%s;appearance-state=%s", data, host, info, state);
		} else {
			tmp = switch_core_sprintf(sh->pool, "<sip:%s>;%s;appearance-state=%s", host, info, state);
		}
	}

	switch_core_hash_insert(sh->hash, key, tmp);

	return 0;
}

static int sync_sla(sofia_profile_t *profile, const char *to_user, const char *to_host, switch_bool_t clear, switch_bool_t unseize, const char *call_id)
{
	struct state_helper *sh;
	switch_memory_pool_t *pool;
	char *sql;
	int total = 0;


	if (clear) {
		struct pres_sql_cb cb = {profile, 0};


		if (call_id) {

			sql = switch_mprintf("update sip_subscriptions set version=version+1,expires=%ld where "
								 "call_id='%q' "
								 "and event='line-seize'", (long) switch_epoch_time_now(NULL),
								 call_id);

			sofia_glue_execute_sql_now(profile, &sql, SWITCH_TRUE);

			if (mod_sofia_globals.debug_sla > 1) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "CLEAR SQL %s\n", sql);
			}
			switch_safe_free(sql);

			sql = switch_mprintf("select full_to, full_from, contact, -1, call_id, event, network_ip, network_port, "
								 "NULL as ct, NULL as pt "
								 " from sip_subscriptions where call_id='%q' "

								 "and event='line-seize'", call_id);

			sofia_glue_execute_sql_callback(profile, profile->dbh_mutex, sql, sofia_presence_send_sql, &cb);
			if (mod_sofia_globals.debug_sla > 1) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "CLEAR SQL %s\n", sql);
			}
			switch_safe_free(sql);
		} else {

			sql = switch_mprintf("update sip_subscriptions set version=version+1,expires=%ld where "
								 "hostname='%q' and profile_name='%q' "
								 "and sub_to_user='%q' and sub_to_host='%q' "

								 "and event='line-seize'", (long) switch_epoch_time_now(NULL),
								 mod_sofia_globals.hostname, profile->name, to_user, to_host
								 );

			if (mod_sofia_globals.debug_sla > 1) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "CLEAR SQL %s\n", sql);
			}

			sofia_glue_execute_sql_now(profile, &sql, SWITCH_TRUE);


			sql = switch_mprintf("select full_to, full_from, contact, -1, call_id, event, network_ip, network_port, "
								 "NULL as ct, NULL as pt "
								 " from sip_subscriptions where "
								 "hostname='%q' and profile_name='%q' "
								 "and sub_to_user='%q' and sub_to_host='%q' "
								 "and event='line-seized'",
								 mod_sofia_globals.hostname, profile->name, to_user, to_host
								 );

			sofia_glue_execute_sql_callback(profile, profile->dbh_mutex, sql, sofia_presence_send_sql, &cb);

			if (mod_sofia_globals.debug_sla > 1) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "CLEAR SQL %s\n", sql);
			}

			switch_safe_free(sql);
		}


		sql = switch_mprintf("delete from sip_dialogs where hostname='%q' and profile_name='%q' and "
							 "((sip_from_user='%q' and sip_from_host='%q') or presence_id='%q@%q') "
							 "and call_info_state='seized'", mod_sofia_globals.hostname, profile->name, to_user, to_host, to_user, to_host);


		if (mod_sofia_globals.debug_sla > 1) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "CLEAR SQL %s\n", sql);
		}
		sofia_glue_execute_sql_now(profile, &sql, SWITCH_TRUE);
		switch_safe_free(sql);
	}


	switch_core_new_memory_pool(&pool);
	sh = switch_core_alloc(pool, sizeof(*sh));
	sh->pool = pool;
	switch_core_hash_init(&sh->hash);

	sql = switch_mprintf("select sip_from_user,sip_from_host,call_info,call_info_state,uuid from sip_dialogs "
						 "where call_info_state is not null and call_info_state != '' and call_info_state != 'idle' and hostname='%q' and profile_name='%q' "
						 "and ((sip_from_user='%q' and sip_from_host='%q') or presence_id='%q@%q') "
						 "and profile_name='%q'",
						 mod_sofia_globals.hostname, profile->name, to_user, to_host, to_user, to_host, profile->name);


	if (mod_sofia_globals.debug_sla > 1) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "PRES SQL %s\n", sql);
	}
	sofia_glue_execute_sql_callback(profile, profile->dbh_mutex, sql, broadsoft_sla_gather_state_callback, sh);
	switch_safe_free(sql);


	if (!zstr(call_id)) {

		if (unseize) {
			sql = switch_mprintf("select call_id,expires,sub_to_user,sub_to_host,event,full_to,full_from,contact,expires,network_ip,network_port "
								 "from sip_subscriptions where call_id='%q' and hostname='%q' and profile_name='%q' "
								 "and (event='call-info' or event='line-seize')",
								 call_id, mod_sofia_globals.hostname, profile->name);

		} else {
			sql = switch_mprintf("select call_id,expires,sub_to_user,sub_to_host,event,full_to,full_from,contact,expires,network_ip,network_port "
								 "from sip_subscriptions where call_id='%q' and hostname='%q' and profile_name='%q' and event='call-info'",
								 call_id, mod_sofia_globals.hostname, profile->name);
		}

	} else {

		if (unseize) {
			sql = switch_mprintf("select call_id,expires,sub_to_user,sub_to_host,event,full_to,full_from,contact,expires,network_ip,network_port "
								 "from sip_subscriptions "
								 "where hostname='%q' and profile_name='%q' "
								 "and sub_to_user='%q' and sub_to_host='%q' "
								 "and (event='call-info' or event='line-seize') and (profile_name='%q' or presence_hosts like '%%%q%%')",
								 mod_sofia_globals.hostname, profile->name, to_user, to_host, profile->name, to_host);
		} else {
			sql = switch_mprintf("select call_id,expires,sub_to_user,sub_to_host,event,full_to,full_from,contact,expires,network_ip,network_port "
								 "from sip_subscriptions "
								 "where hostname='%q' and profile_name='%q' "
								 "and sub_to_user='%q' and sub_to_host='%q' " "and (event='call-info') and "
								 "(profile_name='%q' or presence_hosts like '%%%q%%')",
								 mod_sofia_globals.hostname, profile->name, to_user, to_host, profile->name, to_host);
		}
	}

	if (mod_sofia_globals.debug_sla > 1) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "PRES SQL %s\n", sql);
	}

	sh->profile = profile;
	sofia_glue_execute_sql_callback(profile, profile->dbh_mutex, sql, broadsoft_sla_notify_callback, sh);
	switch_safe_free(sql);
	total = sh->total;
	sh = NULL;
	switch_core_destroy_memory_pool(&pool);





	return total;

}

void sofia_presence_handle_sip_i_subscribe(int status,
										   char const *phrase,
										   nua_t *nua, sofia_profile_t *profile, nua_handle_t *nh, sofia_private_t *sofia_private, sip_t const *sip,
										   sofia_dispatch_event_t *de,
										   tagi_t tags[])
{

	long exp_delta = 0;
	char exp_delta_str[30] = "";
	uint32_t sub_max_deviation_var = 0;
	sip_to_t const *to;
	const char *from_user = NULL, *from_host = NULL;
	const char *to_user = NULL, *to_host = NULL;
	char *my_to_user = NULL;
	char *sql, *event = NULL;
	char *proto = "sip";
	char *orig_proto = "";
	char *alt_proto = NULL;
	char *d_user = NULL;
	char *contact_str = "";
	const char *call_id = NULL;
	char *to_str = NULL;
	char *full_from = NULL;
	char *full_to = NULL;
	char *full_via = NULL;
	char *full_agent = NULL;
	char *sstr;
	switch_event_t *sevent;
	int sub_state = nua_substate_pending;
	int sent_reply = 0;
	sip_contact_t const *contact;
	const char *ipv6;
	const char *contact_user = NULL;
	const char *contact_host = NULL;
	const char *contact_port = NULL;
	sofia_nat_parse_t np = { { 0 } };
	int found_proto = 0;
	const char *use_to_tag;
	char to_tag[13] = "";
	char buf[80] = "";
	char *orig_to_user = NULL;
	char *p;

	if (!sip) {
		return;
	}

	to = sip->sip_to;
	contact = sip->sip_contact;

	np.fs_path = 1;
	if (!(contact_str = sofia_glue_gen_contact_str(profile, sip, nh, de, &np))) {
		nua_respond(nh, 481, "INVALID SUBSCRIPTION", TAG_END());
		return;
	}

	if (sip->sip_to && sip->sip_to->a_tag) {
		use_to_tag = sip->sip_to->a_tag;
	} else {
		switch_stun_random_string(to_tag, 12, NULL);
		use_to_tag = to_tag;
	}

	if ( sip->sip_contact && sip->sip_contact->m_url ) {
		contact_host = sip->sip_contact->m_url->url_host;
		contact_port = sip->sip_contact->m_url->url_port;
		contact_user = sip->sip_contact->m_url->url_user;
	}

	full_agent = sip_header_as_string(nh->nh_home, (void *) sip->sip_user_agent);

	//tl_gets(tags, NUTAG_SUBSTATE_REF(sub_state), TAG_END());

	//sip->sip_subscription_state->ss_substate

	if (sip->sip_subscription_state && sip->sip_subscription_state->ss_substate) {
		if (switch_stristr("terminated", sip->sip_subscription_state->ss_substate)) {
			sub_state = nua_substate_terminated;
		} else if (switch_stristr("active", sip->sip_subscription_state->ss_substate)) {
			sub_state = nua_substate_active;
		}
	}

	event = sip_header_as_string(nh->nh_home, (void *) sip->sip_event);

	if (to) {
		to_str = switch_mprintf("sip:%s@%s", to->a_url->url_user, to->a_url->url_host);
	}

	if (to) {
		to_user = to->a_url->url_user;
		to_host = to->a_url->url_host;
	}

	if (profile->sub_domain) {
		to_host = profile->sub_domain;
	}

	if (sip->sip_from) {
		from_user = sip->sip_from->a_url->url_user;
		from_host = sip->sip_from->a_url->url_host;
	} else {
		from_user = "n/a";
		from_host = "n/a";
	}

	if ((exp_delta = sip->sip_expires ? sip->sip_expires->ex_delta : 3600)) {
		if ((profile->force_subscription_expires > 0) && (profile->force_subscription_expires < (uint32_t)exp_delta)) {
			exp_delta = profile->force_subscription_expires;
		}
	}

	if ((sub_max_deviation_var = profile->sip_subscription_max_deviation)) {
		if (sub_max_deviation_var > 0) {
			int sub_deviation;
			srand( (unsigned) ( (unsigned)(intptr_t)switch_thread_self() + switch_micro_time_now() ) );
			/* random negative number between 0 and negative sub_max_deviation_var: */
			sub_deviation = ( rand() % sub_max_deviation_var ) - sub_max_deviation_var;
			if ( (exp_delta + sub_deviation) > 45 ) {
				exp_delta += sub_deviation;
			}
		}
	}

	if (mod_sofia_globals.debug_presence > 0 || mod_sofia_globals.debug_sla > 0) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "DELTA %ld\n", exp_delta);
	}

	if (!exp_delta) {
		sub_state = nua_substate_terminated;
	}

	switch_snprintf(exp_delta_str, sizeof(exp_delta_str), "%ld", exp_delta);

	if (!strcmp("as-feature-event", event)) {
		sip_authorization_t const *authorization = NULL;
		auth_res_t auth_res = AUTH_FORBIDDEN;
		char key[128] = "";
		switch_event_t *v_event = NULL;


		if (sip->sip_authorization) {
			authorization = sip->sip_authorization;
		} else if (sip->sip_proxy_authorization) {
			authorization = sip->sip_proxy_authorization;
		}

		if (authorization) {
			char network_ip[80];
			int network_port;
			sofia_glue_get_addr(de->data->e_msg, network_ip, sizeof(network_ip), &network_port);
			auth_res = sofia_reg_parse_auth(profile, authorization, sip, de,
											(char *) sip->sip_request->rq_method_name, key, sizeof(key), network_ip, network_port, &v_event, 0,
											REG_REGISTER, to_user, NULL, NULL, NULL);
			if (v_event) switch_event_destroy(&v_event);
		} else if (sofia_reg_handle_register(nua, profile, nh, sip, de, REG_REGISTER, key, sizeof(key), &v_event, NULL, NULL, NULL)) {
			if (v_event) switch_event_destroy(&v_event);
			goto end;
		}

		if ((auth_res != AUTH_OK && auth_res != AUTH_RENEWED)) {
			nua_respond(nh, SIP_401_UNAUTHORIZED, NUTAG_WITH_THIS_MSG(de->data->e_msg), TAG_END());
			goto end;
		}
	} else if (sofia_test_pflag(profile, PFLAG_AUTH_SUBSCRIPTIONS)) {
		sip_authorization_t const *authorization = NULL;
		auth_res_t auth_res = AUTH_FORBIDDEN;
		char keybuf[128] = "";
		char *key;
		size_t keylen;
		switch_event_t *v_event = NULL;

		key = keybuf;
		keylen = sizeof(keybuf);

		if (sip->sip_authorization) {
			authorization = sip->sip_authorization;
		} else if (sip->sip_proxy_authorization) {
			authorization = sip->sip_proxy_authorization;
		}

		if (authorization) {
			char network_ip[80];
			int network_port;
			sofia_glue_get_addr(de->data->e_msg, network_ip, sizeof(network_ip), &network_port);
			auth_res = sofia_reg_parse_auth(profile, authorization, sip, de,
											(char *) sip->sip_request->rq_method_name, key, keylen, network_ip, network_port, NULL, 0,
											REG_INVITE, NULL, NULL, NULL, NULL);
		} else if ( sofia_reg_handle_register(nua, profile, nh, sip, de, REG_INVITE, key, (uint32_t)keylen, &v_event, NULL, NULL, NULL)) {
			if (v_event) {
				switch_event_destroy(&v_event);
			}

			goto end;
		}

		if ((auth_res != AUTH_OK && auth_res != AUTH_RENEWED)) {
			nua_respond(nh, SIP_401_UNAUTHORIZED, NUTAG_WITH_THIS_MSG(de->data->e_msg), TAG_END());
			goto end;
		}
	}

	orig_to_user = su_strdup(nua_handle_home(nh), to_user);

	if (to_user && (p = strchr(to_user, '+')) && p != to_user) {
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
			nua_respond(nh, SIP_404_NOT_FOUND, NUTAG_WITH_THIS_MSG(de->data->e_msg), TAG_END());
			goto end;
		}

		found_proto++;
	}

	call_id = sip->sip_call_id->i_id;
	full_from = sip_header_as_string(nh->nh_home, (void *) sip->sip_from);
	full_to = sip_header_as_string(nh->nh_home, (void *) sip->sip_to);
	full_via = sip_header_as_string(nh->nh_home, (void *) sip->sip_via);


	if (sip->sip_expires && sip->sip_expires->ex_delta > 31536000) {
		sip->sip_expires->ex_delta = 31536000;
	}

	if (sofia_test_pflag(profile, PFLAG_PRESENCE_MAP) && !found_proto && (alt_proto = switch_ivr_check_presence_mapping(to_user, to_host))) {
		orig_proto = proto;
		proto = alt_proto;
	}

	if ((sub_state != nua_substate_terminated)) {
		sql = switch_mprintf("select call_id from sip_subscriptions where call_id='%q' and profile_name='%q' and hostname='%q'",
							 call_id, profile->name, mod_sofia_globals.hostname);
		sofia_glue_execute_sql2str(profile, profile->dbh_mutex, sql, buf, sizeof(buf));


		if (mod_sofia_globals.debug_presence > 0 || mod_sofia_globals.debug_sla > 0) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR,
							  "check subs sql: %s [%s]\n", sql, buf);
		}

		switch_safe_free(sql);

		if (!zstr(buf)) {
			sub_state = nua_substate_active;
		}
	}

	if (sub_state == nua_substate_active) {

		sstr = switch_mprintf("active;expires=%ld", exp_delta);
		
		sql = switch_mprintf("update sip_subscriptions "
							 "set expires=%ld, "
							 "network_ip='%q',network_port='%d',sip_user='%q',sip_host='%q',full_via='%q',full_to='%q',full_from='%q',contact='%q' "
							 "where call_id='%q' and profile_name='%q' and hostname='%q'",
							 (long) switch_epoch_time_now(NULL) + exp_delta, 
							 np.network_ip, np.network_port, from_user, from_host, full_via, full_to, full_from, contact_str,
							 
							 call_id, profile->name, mod_sofia_globals.hostname);

		if (mod_sofia_globals.debug_presence > 0 || mod_sofia_globals.debug_sla > 0) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR,
							  "re-subscribe event %s, sql: %s\n", event, sql);
		}

		sofia_glue_execute_sql_now(profile, &sql, SWITCH_TRUE);
	} else {

		if (sub_state == nua_substate_terminated) {
			sql = switch_mprintf("delete from sip_subscriptions where call_id='%q' and profile_name='%q' and hostname='%q'",
								 call_id, profile->name, mod_sofia_globals.hostname);

			if (mod_sofia_globals.debug_presence > 0 || mod_sofia_globals.debug_sla > 0) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR,
								  "sub del sql: %s\n", sql);
			}

			switch_assert(sql != NULL);
			sofia_glue_execute_sql_now(profile, &sql, SWITCH_TRUE);
			sstr = switch_mprintf("terminated;reason=noresource");

		} else {
			sip_accept_t *ap = sip->sip_accept;
			char accept_header[256] = "";

			sub_state = nua_substate_active;

			while (ap) {
				switch_snprintf(accept_header + strlen(accept_header), sizeof(accept_header) - strlen(accept_header),
								"%s%s ", ap->ac_type, ap->ac_next ? "," : "");
				ap = ap->ac_next;
			}

			sql = switch_mprintf("insert into sip_subscriptions "
								 "(proto,sip_user,sip_host,sub_to_user,sub_to_host,presence_hosts,event,contact,call_id,full_from,"
								 "full_via,expires,user_agent,accept,profile_name,hostname,network_port,network_ip,version,orig_proto, full_to) "
								 "values ('%q','%q','%q','%q','%q','%q','%q','%q','%q','%q','%q',%ld,'%q','%q','%q','%q','%d','%q',-1,'%q','%q;tag=%q')",
								 proto, from_user, from_host, to_user, to_host, profile->presence_hosts ? profile->presence_hosts : "",
								 event, contact_str, call_id, full_from, full_via,
								 (long) switch_epoch_time_now(NULL) + exp_delta,
								 full_agent, accept, profile->name, mod_sofia_globals.hostname,
								 np.network_port, np.network_ip, orig_proto, full_to, use_to_tag);

			switch_assert(sql != NULL);


			if (mod_sofia_globals.debug_presence > 0 || mod_sofia_globals.debug_sla > 0) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "%s SUBSCRIBE %s@%s %s@%s\n%s\n",
								  profile->name, from_user, from_host, to_user, to_host, sql);
			}


			sofia_glue_execute_sql_now(profile, &sql, SWITCH_TRUE);
			sstr = switch_mprintf("active;expires=%ld", exp_delta);
		}

	}

	if ( sip->sip_event && sip->sip_event->o_type && !strcasecmp(sip->sip_event->o_type, "ua-profile") && contact_host ) {
		char *uri = NULL;
		char *ct = "application/url";
		char *extra_headers = NULL;

		if ( contact_port ) {
			uri = switch_mprintf("sip:%s:%s", contact_host, contact_port);
		} else {
			uri = switch_mprintf("sip:%s", contact_host);
		}

		if ( uri ) {
			switch_event_t *params = NULL;
			/* Grandstream REALLY uses a header called Message Body */
			extra_headers = switch_mprintf("MessageBody: %s\r\n", profile->pnp_prov_url);
			if (sofia_test_pflag(profile, PFLAG_SUBSCRIBE_RESPOND_200_OK)) {
				nua_respond(nh, SIP_200_OK, NUTAG_WITH_THIS_MSG(de->data->e_msg), TAG_END());
			} else {
				nua_respond(nh, SIP_202_ACCEPTED, NUTAG_WITH_THIS_MSG(de->data->e_msg), TAG_END());
			}

			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "sending pnp NOTIFY for %s to provision to %s\n", uri, profile->pnp_prov_url);

			switch_event_create(&params, SWITCH_EVENT_NOTIFY);
			switch_event_add_header_string(params, SWITCH_STACK_BOTTOM, "profile", profile->name);
			switch_event_add_header_string(params, SWITCH_STACK_BOTTOM, "event-string", sip->sip_event->o_type);
			switch_event_add_header_string(params, SWITCH_STACK_BOTTOM, "to-uri", uri);
			switch_event_add_header_string(params, SWITCH_STACK_BOTTOM, "from-uri", uri);
			switch_event_add_header_string(params, SWITCH_STACK_BOTTOM, "extra-headers", extra_headers);
			switch_event_add_header_string(params, SWITCH_STACK_BOTTOM, "content-type", ct);
			switch_event_add_body(params, "%s", profile->pnp_prov_url);
			switch_event_fire(&params);

			switch_safe_free(uri);
			switch_safe_free(extra_headers);

			goto end;
		}
	}

	if (status < 200) {
		char *sticky = NULL;
		char *contactstr = profile->url, *cs = NULL;
		char *p = NULL, *new_contactstr = NULL;


		if (np.is_nat) {
			char params[128] = "";
			if (contact->m_url->url_params) {
				switch_snprintf(params, sizeof(params), ";%s", contact->m_url->url_params);
			}
			ipv6 = strchr(np.network_ip, ':');
			sticky = switch_mprintf("sip:%s@%s%s%s:%d%s", contact_user, ipv6 ? "[" : "", np.network_ip, ipv6 ? "]" : "", np.network_port, params);
		}

		if (np.is_auto_nat) {
			contactstr = profile->public_url;
		} else {
			contactstr = profile->url;
		}


		if (switch_stristr("port=tcp", contact->m_url->url_params)) {
			if (np.is_auto_nat) {
				cs = profile->tcp_public_contact;
			} else {
				cs = profile->tcp_contact;
			}
		} else if (switch_stristr("port=tls", contact->m_url->url_params)) {
			if (np.is_auto_nat) {
				cs = profile->tls_public_contact;
			} else {
				cs = profile->tls_contact;
			}
		}

		if (cs) {
			contactstr = cs;
		}


		if (nh && nh->nh_ds && nh->nh_ds->ds_usage) {
			/* nua_dialog_usage_set_refresh_range(nh->nh_ds->ds_usage, exp_delta + SUB_OVERLAP, exp_delta + SUB_OVERLAP); */
			nua_dialog_usage_set_refresh_range(nh->nh_ds->ds_usage, exp_delta, exp_delta);
		}

		if (contactstr && (p = strchr(contactstr, '@'))) {
			if (strrchr(p, '>')) {
				new_contactstr = switch_mprintf("<sip:%s%s", orig_to_user, p);
			} else {
				new_contactstr = switch_mprintf("<sip:%s%s>", orig_to_user, p);
			}
		}

		sip_to_tag(nh->nh_home, sip->sip_to, use_to_tag);

		if (mod_sofia_globals.debug_presence > 0) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Responding to SUBSCRIBE with 202 Accepted\n");
		}
		if (sofia_test_pflag(profile, PFLAG_SUBSCRIBE_RESPOND_200_OK)) {
			nua_respond(nh, SIP_200_OK,
						SIPTAG_TO(sip->sip_to),
						TAG_IF(new_contactstr, SIPTAG_CONTACT_STR(new_contactstr)),
						NUTAG_WITH_THIS_MSG(de->data->e_msg),
						SIPTAG_SUBSCRIPTION_STATE_STR(sstr), SIPTAG_EXPIRES_STR(exp_delta_str), TAG_IF(sticky, NUTAG_PROXY(sticky)), TAG_END());
		} else {
			nua_respond(nh, SIP_202_ACCEPTED,
						SIPTAG_TO(sip->sip_to),
						TAG_IF(new_contactstr, SIPTAG_CONTACT_STR(new_contactstr)),
						NUTAG_WITH_THIS_MSG(de->data->e_msg),
						SIPTAG_SUBSCRIPTION_STATE_STR(sstr), SIPTAG_EXPIRES_STR(exp_delta_str), TAG_IF(sticky, NUTAG_PROXY(sticky)), TAG_END());
		}

		switch_safe_free(new_contactstr);
		switch_safe_free(sticky);

		if (sub_state == nua_substate_terminated) {
			if (mod_sofia_globals.debug_presence > 0) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Sending NOTIFY with Expires [0] and State [%s]\n", sstr);
			}

			if (zstr(full_agent) || (*full_agent != 'z' && *full_agent != 'Z')) {
				/* supress endless loop bug with zoiper */
				nua_notify(nh,
						   SIPTAG_EXPIRES_STR("0"),
						   SIPTAG_SUBSCRIPTION_STATE_STR(sstr),
						   TAG_END());
			}


		}
	}

	if (sub_state == nua_substate_terminated) {
		char *full_call_info = NULL;
		char *p = NULL;

		if (sip->sip_call_info) {
			full_call_info = sip_header_as_string(nh->nh_home, (void *) sip->sip_call_info);
			if ((p = strchr(full_call_info, ';'))) {
				p++;
			}

#if 0
			nua_notify(nh,
					   SIPTAG_EXPIRES_STR("0"),
					   SIPTAG_SUBSCRIPTION_STATE_STR(sstr), TAG_IF(full_call_info, SIPTAG_CALL_INFO_STR(full_call_info)), TAG_END());
#endif

			if (!strcasecmp(event, "line-seize")) {
				if (mod_sofia_globals.debug_sla > 1) {
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "CANCEL LINE SEIZE\n");
				}

				sql = switch_mprintf("delete from sip_dialogs where hostname='%q' and profile_name='%q' and "
									 "((sip_from_user='%q' and sip_from_host='%q') or presence_id='%q@%q') "
									 "and call_info_state='seized'",
									 mod_sofia_globals.hostname, profile->name, to_user, to_host, to_user, to_host);


				if (mod_sofia_globals.debug_sla > 1) {
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "CLEAR SQL %s\n", sql);
				}
				sofia_glue_execute_sql_now(profile, &sql, SWITCH_TRUE);

				sync_sla(profile, to_user, to_host, SWITCH_FALSE, SWITCH_FALSE, NULL);
			}

			su_free(nh->nh_home, full_call_info);

		}

	} else {
		if (!strcasecmp(event, "line-seize")) {
			char *full_call_info = NULL;
			char *p;
			switch_time_t now;

			if (sip->sip_call_info) {
				full_call_info = sip_header_as_string(nh->nh_home, (void *) sip->sip_call_info);
				if ((p = strchr(full_call_info, ';'))) {
					p++;
				}

				nua_notify(nh,
						   SIPTAG_FROM(sip->sip_to),
						   SIPTAG_TO(sip->sip_from),
						   SIPTAG_EXPIRES_STR(exp_delta_str),
						   SIPTAG_SUBSCRIPTION_STATE_STR(sstr),
						   SIPTAG_EVENT_STR("line-seize"), TAG_IF(full_call_info, SIPTAG_CALL_INFO_STR(full_call_info)), TAG_END());




				sql = switch_mprintf("delete from sip_dialogs where hostname='%q' and profile_name='%q' and "
									 "((sip_from_user='%q' and sip_from_host='%q') or presence_id='%q@%q') "
									 "and call_info_state='seized' and profile_name='%q'",
									 mod_sofia_globals.hostname, profile->name, to_user, to_host, to_user, to_host, profile->name);


				if (mod_sofia_globals.debug_sla > 1) {
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "CLEAR SQL %s\n", sql);
				}
				sofia_glue_execute_sql_now(profile, &sql, SWITCH_TRUE);

				now = switch_epoch_time_now(NULL);
				sql = switch_mprintf("insert into sip_dialogs (sip_from_user,sip_from_host,call_info,call_info_state,hostname,expires,rcd,profile_name) "
									 "values ('%q','%q','%q','seized','%q',%"TIME_T_FMT",%ld,'%q')",
									 to_user, to_host, switch_str_nil(p), mod_sofia_globals.hostname,
									 switch_epoch_time_now(NULL) + exp_delta, (long)now, profile->name);

				if (mod_sofia_globals.debug_sla > 1) {
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "SEIZE SQL %s\n", sql);
				}
				sofia_glue_execute_sql_now(profile, &sql, SWITCH_TRUE);
				sync_sla(profile, to_user, to_host, SWITCH_FALSE, SWITCH_FALSE, NULL);

				su_free(nh->nh_home, full_call_info);
			}
		} else if (!strcasecmp(event, "call-info")) {
			sync_sla(profile, to_user, to_host, SWITCH_FALSE, SWITCH_FALSE, call_id);
		}
	}

	sent_reply++;

	switch_safe_free(sstr);

	if (!strcasecmp(event, "as-feature-event")) {
		switch_event_t *event;
		char sip_cseq[40] = "";

		switch_snprintf(sip_cseq, sizeof(sip_cseq), "%d", sip->sip_cseq->cs_seq);
		switch_event_create(&event, SWITCH_EVENT_PHONE_FEATURE_SUBSCRIBE);
		switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "user", from_user);
		switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "host", from_host);
		switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "contact", contact_str);
		switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "call-id", call_id);
		switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "expires", exp_delta_str);
		switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "cseq", sip_cseq);
		switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "profile_name", profile->name);
		switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "hostname", mod_sofia_globals.hostname);

		if (sip->sip_payload) {
			switch_xml_t xml = NULL;
			char *pd_dup = NULL;

			pd_dup = strdup(sip->sip_payload->pl_data);

			if ((xml = switch_xml_parse_str(pd_dup, strlen(pd_dup)))) {
				switch_xml_t device = NULL;

				if ((device = switch_xml_child(xml, "device"))) {
					switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "device", device->txt);
				}

				if (!strcmp(xml->name, "SetDoNotDisturb")) {
					switch_xml_t action = NULL;

					switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "Feature-Action", "SetDoNotDisturb");
					if ((action = switch_xml_child(xml, "doNotDisturbOn"))) {
						switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "Feature-Enabled", action->txt);
						switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "Action-Name", action->name);
					}
				}

				if (!strcmp(xml->name, "SetForwarding")) {
					switch_xml_t cfwd_type, cfwd_enable, cfwd_target;

					switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "Feature-Action", "SetCallForward");
					if ((cfwd_type = switch_xml_child(xml, "forwardingType"))
						&& (cfwd_enable = switch_xml_child(xml, "activateForward"))
						&& (cfwd_target = switch_xml_child(xml, "forwardDN"))) {

						if (!strcmp(cfwd_type->txt, "forwardImmediate")) {
							switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "Feature-Enabled", cfwd_enable->txt);
							switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "Action-Name", "forward_immediate");
							switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "Action-Value", cfwd_target->txt);
						} else if (!strcmp(cfwd_type->txt, "forwardBusy")) {
							switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "Feature-Enabled", cfwd_enable->txt);
							switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "Action-Name", "forward_busy");
							switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "Action-Value", cfwd_target->txt);
						} else if (!strcmp(cfwd_type->txt, "forwardNoAns")) {
							switch_xml_t rc;

							switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "Feature-Enabled", cfwd_enable->txt);
							switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "Action-Name", "forward_no_answer");
							switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "Action-Value", cfwd_target->txt);
							if ((rc = switch_xml_child(xml, "ringCount"))) {
								switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "ringCount", rc->txt);
							}
						}
					}
				}
			}
		}
		switch_event_fire(&event);
	} else if (!strcasecmp(event, "message-summary")) {
		if ((sql = switch_mprintf("select proto,sip_user,'%q',sub_to_user,sub_to_host,event,contact,call_id,full_from,"
								  "full_via,expires,user_agent,accept,profile_name,network_ip"
								  " from sip_subscriptions where hostname='%q' and profile_name='%q' and "
								  "event='message-summary' and sub_to_user='%q' "
								  "and (sip_host='%q' or presence_hosts like '%%%q%%')",
								  to_host, mod_sofia_globals.hostname, profile->name,
								  to_user, to_host, to_host))) {

			if (mod_sofia_globals.debug_presence > 0) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR,
								  "SUBSCRIBE MWI SQL: %s\n", sql);
			}


			sofia_glue_execute_sql_callback(profile, profile->dbh_mutex, sql, sofia_presence_sub_reg_callback, profile);

			switch_safe_free(sql);
		}
	} else 	if (!strcasecmp(event, "conference")) {
		switch_event_t *event;
		switch_event_create(&event, SWITCH_EVENT_CONFERENCE_DATA_QUERY);
		switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "Conference-Name", to_user);
		switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "Conference-Domain", to_host);
		switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "Query-From", from_user);
		switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "Query-From-Domain", from_host);
		switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "Call-Id", call_id);
		switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "Sofia-Profile", profile->name);
		switch_event_fire(&event);
	}

 end:

	if (strcasecmp(event, "call-info") && strcasecmp(event, "line-seize")) {

		if (to_user && (strstr(to_user, "ext+") || strstr(to_user, "user+"))) {
			char protocol[80];
			char *p;

			switch_copy_string(protocol, to_user, sizeof(protocol));
			if ((p = strchr(protocol, '+'))) {
				*p = '\0';
			}

			if (switch_event_create(&sevent, SWITCH_EVENT_PRESENCE_IN) == SWITCH_STATUS_SUCCESS) {
				switch_event_add_header_string(sevent, SWITCH_STACK_BOTTOM, "proto", protocol);
				if (!zstr(orig_proto)) {
					switch_event_add_header_string(sevent, SWITCH_STACK_BOTTOM, "orig_proto", orig_proto);
				}
				switch_event_add_header_string(sevent, SWITCH_STACK_BOTTOM, "login", profile->name);
				switch_event_add_header_string(sevent, SWITCH_STACK_BOTTOM, "sip_profile", profile->name);
				switch_event_add_header(sevent, SWITCH_STACK_BOTTOM, "from", "%s@%s", to_user, to_host);
				switch_event_add_header_string(sevent, SWITCH_STACK_BOTTOM, "rpid", "active");
				switch_event_add_header_string(sevent, SWITCH_STACK_BOTTOM, "status", "Click To Call");
				switch_event_fire(&sevent);
			}

		} else if (to_user && (strcasecmp(proto, SOFIA_CHAT_PROTO) != 0)) {
			if (switch_event_create(&sevent, SWITCH_EVENT_PRESENCE_PROBE) == SWITCH_STATUS_SUCCESS) {
				switch_event_add_header_string(sevent, SWITCH_STACK_BOTTOM, "proto", proto);
				if (!zstr(orig_proto)) {
					switch_event_add_header_string(sevent, SWITCH_STACK_BOTTOM, "orig_proto", orig_proto);
				}
				switch_event_add_header_string(sevent, SWITCH_STACK_BOTTOM, "login", profile->name);
				switch_event_add_header_string(sevent, SWITCH_STACK_BOTTOM, "sip_profile", profile->name);
				switch_event_add_header(sevent, SWITCH_STACK_BOTTOM, "from", "%s@%s", from_user, from_host);
				switch_event_add_header(sevent, SWITCH_STACK_BOTTOM, "to", "%s%s%s@%s", proto, "+", to_user, to_host);
				switch_event_add_header_string(sevent, SWITCH_STACK_BOTTOM, "proto-specific-event-name", event);
				switch_event_add_header_string(sevent, SWITCH_STACK_BOTTOM, "expires", exp_delta_str);
				switch_event_add_header_string(sevent, SWITCH_STACK_BOTTOM, "event_type", "presence");
				switch_event_add_header_string(sevent, SWITCH_STACK_BOTTOM, "alt_event_type", "dialog");
				switch_event_add_header_string(sevent, SWITCH_STACK_BOTTOM, "expires", exp_delta_str);
				switch_event_fire(&sevent);

			}
		} else {

			if (!strcasecmp(event, "dialog")) {
				if (switch_event_create(&sevent, SWITCH_EVENT_PRESENCE_PROBE) == SWITCH_STATUS_SUCCESS) {
					switch_event_add_header_string(sevent, SWITCH_STACK_BOTTOM, "probe-type", "dialog");
					switch_event_add_header_string(sevent, SWITCH_STACK_BOTTOM, "proto", SOFIA_CHAT_PROTO);
					switch_event_add_header_string(sevent, SWITCH_STACK_BOTTOM, "login", profile->name);
					switch_event_add_header_string(sevent, SWITCH_STACK_BOTTOM, "sip_profile", profile->name);
					switch_event_add_header(sevent, SWITCH_STACK_BOTTOM, "from", "%s@%s", from_user, from_host);
					switch_event_add_header(sevent, SWITCH_STACK_BOTTOM, "to", "%s@%s", to_user, to_host);
					switch_event_add_header_string(sevent, SWITCH_STACK_BOTTOM, "proto-specific-event-name", event);
					switch_event_add_header_string(sevent, SWITCH_STACK_BOTTOM, "expires", exp_delta_str);
					switch_event_add_header_string(sevent, SWITCH_STACK_BOTTOM, "event_type", "presence");
					switch_event_add_header_string(sevent, SWITCH_STACK_BOTTOM, "alt_event_type", "dialog");
					switch_event_add_header_string(sevent, SWITCH_STACK_BOTTOM, "expires", exp_delta_str);
					switch_event_add_header_string(sevent, SWITCH_STACK_BOTTOM, "sub-call-id", call_id);
					switch_event_fire(&sevent);
				}
			} else if (!strcasecmp(event, "presence")) {
				if (switch_event_create(&sevent, SWITCH_EVENT_PRESENCE_PROBE) == SWITCH_STATUS_SUCCESS) {
					switch_event_add_header_string(sevent, SWITCH_STACK_BOTTOM, "proto", SOFIA_CHAT_PROTO);
					switch_event_add_header_string(sevent, SWITCH_STACK_BOTTOM, "login", profile->name);
					switch_event_add_header_string(sevent, SWITCH_STACK_BOTTOM, "sip_profile", profile->name);
					switch_event_add_header_string(sevent, SWITCH_STACK_BOTTOM, "presence-source", "subscribe");
					switch_event_add_header(sevent, SWITCH_STACK_BOTTOM, "from", "%s@%s", from_user, from_host);
					switch_event_add_header(sevent, SWITCH_STACK_BOTTOM, "to", "%s@%s", to_user, to_host);
					switch_event_add_header_string(sevent, SWITCH_STACK_BOTTOM, "expires", exp_delta_str);
					switch_event_add_header_string(sevent, SWITCH_STACK_BOTTOM, "alt_event_type", "dialog");
					switch_event_add_header_string(sevent, SWITCH_STACK_BOTTOM, "rpid", "unknown");
					switch_event_add_header_string(sevent, SWITCH_STACK_BOTTOM, "status", "Registered");
					switch_event_add_header_string(sevent, SWITCH_STACK_BOTTOM, "sub-call-id", call_id);
					switch_event_fire(&sevent);
				}
			}
		}
	}

	if (event) {
		su_free(nh->nh_home, event);
	}

	if (full_from) {
		su_free(nh->nh_home, full_from);
	}
	if (full_to) {
		su_free(nh->nh_home, full_to);
	}

	if (full_via) {
		su_free(nh->nh_home, full_via);
	}
	if (full_agent) {
		su_free(nh->nh_home, full_agent);
	}

	switch_safe_free(d_user);
	switch_safe_free(to_str);
	switch_safe_free(contact_str);
	switch_safe_free(alt_proto);

	if (!sent_reply) {
		nua_respond(nh, 481, "INVALID SUBSCRIPTION", TAG_END());
	}

	if (!sofia_private || !sofia_private->is_call) {
		nua_handle_destroy(nh);
	}

}


sofia_gateway_subscription_t *sofia_find_gateway_subscription(sofia_gateway_t *gateway_ptr, const char *event)
{
	sofia_gateway_subscription_t *gw_sub_ptr;
	for (gw_sub_ptr = gateway_ptr->subscriptions; gw_sub_ptr; gw_sub_ptr = gw_sub_ptr->next) {
		if (!strcasecmp(gw_sub_ptr->event, event)) {
			/* this is the gateway subscription we are interested in */
			return gw_sub_ptr;
		}
	}
	return NULL;
}

void sofia_presence_handle_sip_r_subscribe(int status,
										   char const *phrase,
										   nua_t *nua, sofia_profile_t *profile, nua_handle_t *nh, sofia_private_t *sofia_private, sip_t const *sip,
										   sofia_dispatch_event_t *de,
										   tagi_t tags[])
{
	sip_event_t const *o = NULL;
	sofia_gateway_subscription_t *gw_sub_ptr;
	sofia_gateway_t *gateway = NULL;

	if (!sip) {
		return;
	}

	tl_gets(tags, SIPTAG_EVENT_REF(o), TAG_END());
	/* o->o_type: message-summary (for example) */
	if (!o) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Event information not given\n");
		return;
	}

	if (!sofia_private || zstr(sofia_private->gateway_name)) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Gateway information missing\n");
		return;
	}


	if (!(gateway = sofia_reg_find_gateway(sofia_private->gateway_name))) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Gateway information missing\n");
		return;
	}


	/* Find the subscription if one exists */
	if (!(gw_sub_ptr = sofia_find_gateway_subscription(gateway, o->o_type))) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "Could not find gateway subscription.  Gateway: %s.  Subscription Event: %s\n",
						  gateway->name, o->o_type);
		goto end;
	}

	/* Update the subscription status for the subscription */
	switch (status) {
	case 200:
	case 202:
		/* TODO: in the spec it is possible for the other side to change the original expiry time,
		 * this needs to be researched (eg, what sip header this information will be in) and implemented.
		 * Although, since it seems the sofia stack is pretty much handling the subscription expiration
		 * anyway, then maybe its not even worth bothering.
		 */
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "got 200 OK response, updated state to SUB_STATE_SUBSCRIBE.\n");
		gw_sub_ptr->state = SUB_STATE_SUBSCRIBE;
		break;
	case 100:
		break;
	default:
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "status (%d) != 200, updated state to SUB_STATE_FAILED.\n", status);
		gw_sub_ptr->state = SUB_STATE_FAILED;

		if (!sofia_private) {
			nua_handle_destroy(nh);
		}
		
		break;
	}

 end:

	if (gateway) {
		sofia_reg_release_gateway(gateway);
	}

}


static int sofia_presence_send_sql(void *pArg, int argc, char **argv, char **columnNames)
{
	struct pres_sql_cb *cb = (struct pres_sql_cb *) pArg;


	if (mod_sofia_globals.debug_presence > 0) {
		int i;
		for(i = 0; i < argc; i++) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "arg %d[%s] = [%s]\n", i, columnNames[i], argv[i]);
		}
	}

	send_presence_notify(cb->profile, argv[0], argv[1], argv[2], argv[3], argv[4], argv[5], argv[6], argv[7], argv[8], argv[9], NULL);
	cb->ttl++;

	return 0;
}


uint32_t sofia_presence_contact_count(sofia_profile_t *profile, const char *contact_str)
{
	char buf[32] = "";
	char *sql;

	sql = switch_mprintf("select count(*) from sip_subscriptions where hostname='%q' and profile_name='%q' and contact='%q'",
						 mod_sofia_globals.hostname, profile->name, contact_str);

	sofia_glue_execute_sql2str(profile, profile->dbh_mutex, sql, buf, sizeof(buf));
	switch_safe_free(sql);
	return atoi(buf);
}

void sofia_presence_handle_sip_i_publish(nua_t *nua, sofia_profile_t *profile, nua_handle_t *nh, sofia_private_t *sofia_private, sip_t const *sip,
										 sofia_dispatch_event_t *de,
										 tagi_t tags[])
{

	sip_from_t const *from;
	char *from_user = NULL;
	char *from_host = NULL;
	char *rpid = "";
	sip_payload_t *payload;
	char *event_type = NULL;
	char etag[9] = "";
	char expstr[30] = "";
	long exp = 0, exp_delta = 3600;
	char *pd_dup = NULL;
	int count = 1, sub_count = 1;
	char *contact_str;
	sofia_nat_parse_t np = { { 0 } };

	if (!sip) {
		return;
	}

	from = sip->sip_from;
	payload = sip->sip_payload;

	np.fs_path = 1;
	contact_str = sofia_glue_gen_contact_str(profile, sip, nh, de, &np);

	if (from) {
		from_user = (char *) from->a_url->url_user;
		from_host = (char *) from->a_url->url_host;
	}

	exp_delta = (sip->sip_expires ? sip->sip_expires->ex_delta : 3600);
	if ((profile->force_publish_expires > 0) && (profile->force_publish_expires < (uint32_t)exp_delta)) {
		exp_delta = profile->force_publish_expires;
	}

	if (exp_delta < 0) {
		exp = exp_delta;
	} else {
		exp = (long) switch_epoch_time_now(NULL) + exp_delta;
	}

	if (payload) {
		switch_xml_t xml, note, person, tuple, status, basic, act;
		switch_event_t *event;
		char *sql;
		char *full_agent = NULL;
		char network_ip[80];
		int network_port = 0;

		sofia_glue_get_addr(de->data->e_msg, network_ip, sizeof(network_ip), &network_port);

		pd_dup = strdup(payload->pl_data);

		if ((xml = switch_xml_parse_str(pd_dup, strlen(pd_dup)))) {
			char *open_closed = "", *note_txt = "";

			if (sip->sip_user_agent) {
				full_agent = sip_header_as_string(nh->nh_home, (void *) sip->sip_user_agent);
			}

			if ((tuple = switch_xml_child(xml, "tuple")) && (status = switch_xml_child(tuple, "status"))
				&& (basic = switch_xml_child(status, "basic"))) {
				open_closed = basic->txt;

				if ((note = switch_xml_child(tuple, "note"))) {
					rpid = note_txt = note->txt;
				} else if ((note = switch_xml_child(tuple, "dm:note"))) {
					rpid = note_txt = note->txt;
				}
			}

			if ((person = switch_xml_child(xml, "dm:person"))) {
				if ((note = switch_xml_child(person, "dm:note"))) {
					note_txt = note->txt;
				} else if ((note = switch_xml_child(person, "rpid:note"))) {
					note_txt = note->txt;
				}
				if ((act = switch_xml_child(person, "rpid:activities")) && act->child && act->child->name) {
					if ((rpid = strchr(act->child->name, ':'))) {
						rpid++;
					} else {
						rpid = act->child->name;
					}
				}
				if (zstr(note_txt)) note_txt = "Available";
			}

			if (!strcasecmp(open_closed, "closed")) {
				rpid = note_txt = "Unregistered";
				if (sofia_test_pflag(profile, PFLAG_MULTIREG)) {
					count = sofia_reg_reg_count(profile, from_user, from_host);

					if (count != 1) {
						/* Don't broadcast offline when there is more than one client or one signing off makes them all appear to sign off on some clients */
						count = 0;
					} else {
						sub_count = sofia_presence_contact_count(profile, contact_str);
					}
				}
			}

			event_type = sip_header_as_string(nh->nh_home, (void *) sip->sip_event);

			if (count) {
				if ((sql = switch_mprintf("delete from sip_presence where sip_user='%q' and sip_host='%q' "
										  " and profile_name='%q' and hostname='%q'",
										  from_user, from_host, profile->name, mod_sofia_globals.hostname))) {
					sofia_glue_execute_sql_now(profile, &sql, SWITCH_TRUE);
				}

				if (sub_count > 0 && (sql = switch_mprintf("insert into sip_presence (sip_user, sip_host, status, rpid, expires, user_agent,"
														   " profile_name, hostname, open_closed, network_ip, network_port) "
														   "values ('%q','%q','%q','%q',%ld,'%q','%q','%q','%q','%q','%d')",
														   from_user, from_host, note_txt, rpid, exp, full_agent, profile->name,
														   mod_sofia_globals.hostname, open_closed, network_ip, network_port))) {

					sofia_glue_execute_sql_now(profile, &sql, SWITCH_TRUE);
				}

			} else if (contact_str) {
				struct pres_sql_cb cb = {profile, 0};

				sql = switch_mprintf("select full_to, full_from, contact, expires, call_id, event, network_ip, network_port, "
									 "'application/pidf+xml' as ct,'%q' as pt "
									 " from sip_subscriptions where "
									 "hostname='%q' and profile_name='%q' and sub_to_user='%q' and sub_to_host='%q' and event='%q'"
									 "and contact = '%q' ",

									 switch_str_nil(payload->pl_data),
									 mod_sofia_globals.hostname, profile->name,
									 from_user, from_host, event_type, contact_str);

				sofia_glue_execute_sql_callback(profile, profile->dbh_mutex, sql, sofia_presence_send_sql, &cb);
				switch_safe_free(sql);
			}

			if (sub_count > 0) {
				if (switch_event_create(&event, SWITCH_EVENT_PRESENCE_IN) == SWITCH_STATUS_SUCCESS) {
					switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "proto", SOFIA_CHAT_PROTO);
					switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "rpid", rpid);
					switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "login", profile->url);
					switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "sip_profile", profile->name);
					switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "user-agent", full_agent);
					switch_event_add_header(event, SWITCH_STACK_BOTTOM, "from", "%s@%s", from_user, from_host);
					switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "status", note_txt);
					switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "event_type", event_type);
					switch_event_fire(&event);
				}
			}

			if (event_type) {
				su_free(nh->nh_home, event_type);
			}

			if (full_agent) {
				su_free(nh->nh_home, full_agent);
			}

			switch_xml_free(xml);
		}
	} else {
		char *sql = switch_mprintf("update sip_presence set expires=%ld where sip_user='%q' and sip_host='%q' and profile_name='%q' and hostname='%q'",
								   exp, from_user, from_host, profile->name, mod_sofia_globals.hostname);
		sofia_glue_execute_sql_now(profile, &sql, SWITCH_TRUE);
	}

	switch_safe_free(pd_dup);

	switch_snprintf(expstr, sizeof(expstr), "%d", exp_delta);
	switch_stun_random_string(etag, 8, NULL);

	if (sub_count > 0) {
		nua_respond(nh, SIP_200_OK, NUTAG_WITH_THIS_MSG(de->data->e_msg), SIPTAG_ETAG_STR(etag), SIPTAG_EXPIRES_STR(expstr), TAG_END());
	} else {
		nua_respond(nh, SIP_404_NOT_FOUND, NUTAG_WITH_THIS_MSG(de->data->e_msg), TAG_END());
	}

	switch_safe_free(contact_str);
}

void sofia_presence_set_hash_key(char *hash_key, int32_t len, sip_t const *sip)
{
	url_t *to = sip->sip_to->a_url;
	url_t *from = sip->sip_from->a_url;
	switch_snprintf(hash_key, len, "%s%s%s", from->url_user, from->url_host, to->url_user);
}

void sofia_presence_handle_sip_i_message(int status,
										 char const *phrase,
										 nua_t *nua, sofia_profile_t *profile, nua_handle_t *nh,
										 switch_core_session_t *session,
										 sofia_private_t *sofia_private, sip_t const *sip,
										 sofia_dispatch_event_t *de,
										 tagi_t tags[])
{

	if (sip) {
		sip_from_t const *from = sip->sip_from;
		const char *from_user = NULL;
		const char *from_host = NULL;
		sip_to_t const *to = sip->sip_to;
		const char *to_user = NULL;
		const char *to_host = NULL;
		sip_payload_t *payload = sip->sip_payload;
		char *msg = NULL;
		const char *us;
		char network_ip[80];
		int network_port = 0;
		switch_channel_t *channel = NULL;


		if (!sofia_test_pflag(profile, PFLAG_ENABLE_CHAT)) {
			goto end;
		}


		if (session) {
			channel = switch_core_session_get_channel(session);
		}

		if (sofia_test_pflag(profile, PFLAG_AUTH_MESSAGES) && sip){
			sip_authorization_t const *authorization = NULL;
			auth_res_t auth_res = AUTH_FORBIDDEN;
			char keybuf[128] = "";
			char *key;
			size_t keylen;
			switch_event_t *v_event = NULL;

			key = keybuf;
			keylen = sizeof(keybuf);

			if (sip->sip_authorization) {
				authorization = sip->sip_authorization;
			} else if (sip->sip_proxy_authorization) {
				authorization = sip->sip_proxy_authorization;
			}

			if (authorization) {
				char network_ip[80];
				int network_port;
				sofia_glue_get_addr(de->data->e_msg, network_ip, sizeof(network_ip), &network_port);
				auth_res = sofia_reg_parse_auth(profile, authorization, sip, de,
												(char *) sip->sip_request->rq_method_name, key, keylen, network_ip, network_port, NULL, 0,
												REG_INVITE, NULL, NULL, NULL, NULL);
			} else if ( sofia_reg_handle_register(nua, profile, nh, sip, de, REG_INVITE, key, (uint32_t)keylen, &v_event, NULL, NULL, NULL)) {
				if (v_event) {
					switch_event_destroy(&v_event);
				}

				goto end;
			}

			if ((auth_res != AUTH_OK && auth_res != AUTH_RENEWED)) {
				nua_respond(nh, SIP_401_UNAUTHORIZED, NUTAG_WITH_THIS_MSG(de->data->e_msg), TAG_END());
				goto end;
			}

			if (channel) {
				switch_channel_set_variable(channel, "sip_authorized", "true");
			}
		}

		if ((us = sofia_glue_get_unknown_header(sip, "X-FS-Sending-Message")) && !strcmp(us, switch_core_get_uuid())) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "Not sending message to ourselves!\n");
			nua_respond(nh, SIP_503_SERVICE_UNAVAILABLE, NUTAG_WITH_THIS_MSG(de->data->e_msg), TAG_END());
			return;
		}

		if (sip->sip_content_type && sip->sip_content_type->c_subtype) {
			if (strstr(sip->sip_content_type->c_subtype, "composing")) {
				goto end;
			}
		}


		sofia_glue_get_addr(de->data->e_msg, network_ip, sizeof(network_ip), &network_port);


		if (from) {
			from_user = from->a_url->url_user;
			from_host = from->a_url->url_host;
		}

		if (to) {
			to_user = to->a_url->url_user;
			to_host = to->a_url->url_host;
		}

		if (!to_user) {
			goto end;
		}

		if (payload) {
			msg = payload->pl_data;
		}

		if (nh) {
			char hash_key[512];
			private_object_t *tech_pvt;
			switch_event_t *event, *event_dup;
			char *to_addr;
			char *from_addr;
			char *p;
			char *full_from;
			char proto[512] = SOFIA_CHAT_PROTO;
			sip_unknown_t *un;
			int first_history_info = 1;

			full_from = sip_header_as_string(nh->nh_home, (void *) sip->sip_from);

			if ((p = strchr(to_user, '+')) && p != to_user) {
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

			if (sofia_test_pflag(profile, PFLAG_IN_DIALOG_CHAT)) {
				sofia_presence_set_hash_key(hash_key, sizeof(hash_key), sip);
			}

			if (switch_event_create(&event, SWITCH_EVENT_MESSAGE) == SWITCH_STATUS_SUCCESS) {
				switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "login", profile->url);
				switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "proto", SOFIA_CHAT_PROTO);

				switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "to_proto", proto);

				switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "from", from_addr);
				switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "from_user", from_user);
				switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "from_host", from_host);
				switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "to_user", to_user);
				switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "to_host", to_host);
				switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "from_sip_ip", network_ip);
				switch_event_add_header(event, SWITCH_STACK_BOTTOM, "from_sip_port", "%d", network_port);
				switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "to", to_addr);
				switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "subject", "SIMPLE MESSAGE");
				switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "context", profile->context);

				if (sip->sip_content_type && sip->sip_content_type->c_subtype) {
					switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "type", sip->sip_content_type->c_type);
				} else {
					switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "type", "text/plain");
				}
				
				switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "from_full", full_from);
				switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "sip_profile", profile->name);
				

				if (sip->sip_call_info) {
					sip_call_info_t *call_info = sip->sip_call_info;
					char *ci = sip_header_as_string(nua_handle_home(nh), (void *) call_info);
					switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "sip_call_info", ci);
				}
				
				/* Loop thru unknown Headers Here so we can do something with them */
				for (un = sip->sip_unknown; un; un = un->un_next) {
					if (!strncasecmp(un->un_name, "Diversion", 9)) {
						/* Basic Diversion Support for Diversion Indication in SIP */
						/* draft-levy-sip-diversion-08 */
						if (!zstr(un->un_value)) {
							char *tmp_name;
							if ((tmp_name = switch_mprintf("%s%s", SOFIA_SIP_HEADER_PREFIX, un->un_name))) {
								switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, tmp_name, un->un_value);
								free(tmp_name);
							}
						}
					} else if (!strncasecmp(un->un_name, "History-Info", 12)) {
						if (first_history_info) {
							/* If the header exists first time, make sure to remove old info and re-set the variable */
							switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "sip_history_info", un->un_value);
							first_history_info = 0;
						} else {
							/* Append the History-Info into one long string */
							const char *history_var = switch_channel_get_variable(channel, "sip_history_info");
							if (!zstr(history_var)) {
								char *tmp_str;
								if ((tmp_str = switch_mprintf("%s, %s", history_var, un->un_value))) {
									switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "sip_history_info", tmp_str);
									free(tmp_str);
								} else {
									switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "sip_history_info", un->un_value);
								}
							} else {
								switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "sip_history_info", un->un_value);
							}
						}
					} else if (!strcasecmp(un->un_name, "Geolocation")) {
						switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "sip_geolocation", un->un_value);
					} else if (!strcasecmp(un->un_name, "Geolocation-Error")) {
						switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "sip_geolocation_error", un->un_value);
					} else if (!strncasecmp(un->un_name, "X-", 2) || !strncasecmp(un->un_name, "P-", 2) || !strcasecmp(un->un_name, "User-to-User")) {
						if (!zstr(un->un_value)) {
							char new_name[512] = "";
							int reps = 0;
							for (;;) {
								char postfix[25] = "";
								if (reps > 0) {
									switch_snprintf(postfix, sizeof(postfix), "-%d", reps);
								}
								reps++;
								switch_snprintf(new_name, sizeof(new_name), "%s%s%s", SOFIA_SIP_HEADER_PREFIX, un->un_name, postfix);

								if (switch_channel_get_variable(channel, new_name)) {
									continue;
								}

								switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, new_name, un->un_value);
								break;
							}
						}
					}
				}
				
				if (msg) {
					switch_event_add_body(event, "%s", msg);
				}

				if (channel) {
					switch_channel_event_set_data(channel, event);
				}


				if (sofia_test_pflag(profile, PFLAG_FIRE_MESSAGE_EVENTS)) {
					if (switch_event_dup(&event_dup, event) == SWITCH_STATUS_SUCCESS) {
						event_dup->event_id = SWITCH_EVENT_RECV_MESSAGE;
						event_dup->flags |= EF_UNIQ_HEADERS;
						switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "Event-Name", switch_event_name(event->event_id));
						switch_event_fire(&event_dup);
					}
				}

				if (session) {
					if (switch_event_dup(&event_dup, event) == SWITCH_STATUS_SUCCESS) {
						switch_core_session_queue_event(session, &event_dup);
					}
				}


			} else {
				abort();
			}

			if (sofia_test_pflag(profile, PFLAG_IN_DIALOG_CHAT) && (tech_pvt = (private_object_t *) switch_core_hash_find(profile->chat_hash, hash_key))) {
				switch_core_session_queue_event(tech_pvt->session, &event);
			} else {
				switch_core_chat_send(proto, event);
				switch_event_destroy(&event);
			}

			switch_safe_free(to_addr);
			switch_safe_free(from_addr);

			if (full_from) {
				su_free(nh->nh_home, full_from);
			}
		}
	}

 end:

	if (sofia_test_pflag(profile, PFLAG_MESSAGES_RESPOND_200_OK)) {
		nua_respond(nh, SIP_200_OK, NUTAG_WITH_THIS_MSG(de->data->e_msg), TAG_END());
	} else {
		nua_respond(nh, SIP_202_ACCEPTED, NUTAG_WITH_THIS_MSG(de->data->e_msg), TAG_END());
	}

}

void sofia_presence_set_chat_hash(private_object_t *tech_pvt, sip_t const *sip)
{
	char hash_key[256] = "";
	char buf[512];
	su_home_t *home = NULL;

	if (!tech_pvt || tech_pvt->hash_key || !sip || !sip->sip_from || !sip->sip_from->a_url ||
		!sip->sip_from->a_url->url_user || !sip->sip_from->a_url->url_host) {
		return;
	}

	if (sofia_reg_find_reg_url(tech_pvt->profile, sip->sip_from->a_url->url_user, sip->sip_from->a_url->url_host, buf, sizeof(buf))) {
		home = su_home_new(sizeof(*home));
		switch_assert(home != NULL);
		tech_pvt->chat_from = sip_header_as_string(home, (const sip_header_t *) sip->sip_to);
		tech_pvt->chat_to = switch_core_session_strdup(tech_pvt->session, buf);
		sofia_presence_set_hash_key(hash_key, sizeof(hash_key), sip);
		su_home_unref(home);
		home = NULL;
	} else {
		return;
	}

	switch_mutex_lock(tech_pvt->profile->flag_mutex);
	tech_pvt->hash_key = switch_core_session_strdup(tech_pvt->session, hash_key);
	switch_core_hash_insert(tech_pvt->profile->chat_hash, tech_pvt->hash_key, tech_pvt);
	switch_mutex_unlock(tech_pvt->profile->flag_mutex);
}


void sofia_presence_check_subscriptions(sofia_profile_t *profile, time_t now)
{
	char *sql;

	if (now) {
		struct pres_sql_cb cb = {profile, 0};

		if (profile->pres_type != PRES_TYPE_FULL) {
			if (mod_sofia_globals.debug_presence > 0) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "check_subs: %s is passive, skipping\n", (char *) profile->name);
			}
			return;
		}

		sql = switch_mprintf("update sip_subscriptions set version=version+1 where "
							 "((expires > 0 and expires <= %ld)) and profile_name='%q' and hostname='%q'",
							 (long) now, profile->name, mod_sofia_globals.hostname);

		sofia_glue_execute_sql_now(profile, &sql, SWITCH_TRUE);
		switch_safe_free(sql);

		sql = switch_mprintf("select full_to, full_from, contact, -1, call_id, event, network_ip, network_port, "
							 "NULL as ct, NULL as pt "
							 " from sip_subscriptions where ((expires > 0 and expires <= %ld)) and profile_name='%q' and hostname='%q'",
							 (long) now, profile->name, mod_sofia_globals.hostname);

		sofia_glue_execute_sql_callback(profile, profile->dbh_mutex, sql, sofia_presence_send_sql, &cb);
		switch_safe_free(sql);

		if (cb.ttl) {
			sql = switch_mprintf("delete from sip_subscriptions where ((expires > 0 and expires <= %ld)) "
								 "and profile_name='%q' and hostname='%q'",
								 (long) now, profile->name, mod_sofia_globals.hostname);

			if (mod_sofia_globals.debug_presence > 0 || mod_sofia_globals.debug_sla > 0) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR,
								  "sub del sql: %s\n", sql);
			}

			sofia_glue_execute_sql_now(profile, &sql, SWITCH_TRUE);
		}
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
