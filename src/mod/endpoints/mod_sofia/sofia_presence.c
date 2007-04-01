#include "mod_sofia.h"


switch_status_t sofia_presence_chat_send(char *proto, char *from, char *to, char *subject, char *body, char *hint)
{
	char buf[256];
	char *user, *host;
	sofia_profile_t *profile;
	char *ffrom = NULL;
	nua_handle_t *msg_nh;
	char *contact;

	if (to && (user = strdup(to))) {
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

			if (!(fp = strdup(from))) {
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
	}

	return SWITCH_STATUS_SUCCESS;
}

void sofia_presence_cancel(void)
{
	char *sql, *errmsg = NULL;
	switch_core_db_t *db;
	sofia_profile_t *profile;
	switch_hash_index_t *hi;
	void *val;

	if ((sql = switch_mprintf("select 0,'unavailable','unavailable',* from sip_subscriptions where event='presence'"))) {
		switch_mutex_lock(mod_sofia_globals.hash_mutex);
		for (hi = switch_hash_first(switch_hash_pool_get(mod_sofia_globals.profile_hash), mod_sofia_globals.profile_hash); hi; hi = switch_hash_next(hi)) {
			switch_hash_this(hi, NULL, NULL, &val);
			profile = (sofia_profile_t *) val;
			if (!(profile->pflags & PFLAG_PRESENCE)) {
				continue;
			}

			if (!(db = switch_core_db_open_file(profile->dbname))) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Error Opening DB %s\n", profile->dbname);
				continue;
			}
			switch_mutex_lock(profile->ireg_mutex);
			switch_core_db_exec(db, sql, sofia_presence_sub_callback, profile, &errmsg);
			switch_mutex_unlock(profile->ireg_mutex);
			switch_core_db_close(db);
		}
		switch_safe_free(sql);
	}
	switch_mutex_unlock(mod_sofia_globals.hash_mutex);
}

void sofia_presence_establish_presence(sofia_profile_t * profile)
{
	char *sql, *errmsg = NULL;
	switch_core_db_t *db;

	if (!(db = switch_core_db_open_file(profile->dbname))) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Error Opening DB %s\n", profile->dbname);
		return;
	}

	if ((sql = switch_mprintf("select user,host,'Registered','unknown','' from sofia_handle_sip_registrations"))) {
		switch_mutex_lock(profile->ireg_mutex);
		switch_core_db_exec(db, sql, sofia_presence_resub_callback, profile, &errmsg);
		switch_mutex_unlock(profile->ireg_mutex);
		switch_safe_free(sql);
	}

	if ((sql = switch_mprintf("select sub_to_user,sub_to_host,'Online','unknown',proto from sip_subscriptions "
							  "where proto='ext' or proto='user' or proto='conf'"))) {
		switch_mutex_lock(profile->ireg_mutex);
		switch_core_db_exec(db, sql, sofia_presence_resub_callback, profile, &errmsg);
		switch_mutex_unlock(profile->ireg_mutex);
		switch_safe_free(sql);
	}

	switch_core_db_close(db);

}



char *sofia_presence_translate_rpid(char *in, char *ext)
{
	char *r = NULL;

	if (in && (strstr(in, "null") || strstr(in, "NULL"))) {
		in = NULL;
	}

	if (!in) {
		in = ext;
	}

	if (!in) {
		return NULL;
	}

	if (!strcasecmp(in, "dnd")) {
		r = "busy";
	}

	if (ext && !strcasecmp(ext, "away")) {
		r = "idle";
	}

	return r;
}

void sofia_presence_event_handler(switch_event_t *event)
{
	sofia_profile_t *profile;
	switch_hash_index_t *hi;
	void *val;
	char *from = switch_event_get_header(event, "from");
	char *proto = switch_event_get_header(event, "proto");
	char *rpid = switch_event_get_header(event, "rpid");
	char *status = switch_event_get_header(event, "status");
	char *event_type = switch_event_get_header(event, "event_type");
	//char *event_subtype = switch_event_get_header(event, "event_subtype");
	char *sql = NULL;
	char *euser = NULL, *user = NULL, *host = NULL;
	char *errmsg;
	switch_core_db_t *db;


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
			sql = switch_mprintf("select 1,'%q','%q',* from sip_subscriptions where event='presence' and full_from like '%%%q%%'", status, rpid, from);
		} else {
			sql = switch_mprintf("select 1,'%q','%q',* from sip_subscriptions where event='presence'", status, rpid);
		}

		switch_mutex_lock(mod_sofia_globals.hash_mutex);
		for (hi = switch_hash_first(switch_hash_pool_get(mod_sofia_globals.profile_hash), mod_sofia_globals.profile_hash); hi; hi = switch_hash_next(hi)) {
			switch_hash_this(hi, NULL, NULL, &val);
			profile = (sofia_profile_t *) val;
			if (!(profile->pflags & PFLAG_PRESENCE)) {
				continue;
			}

			if (sql) {
				if (!(db = switch_core_db_open_file(profile->dbname))) {
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Error Opening DB %s\n", profile->dbname);
					continue;
				}
				switch_mutex_lock(profile->ireg_mutex);
				switch_core_db_exec(db, sql, sofia_presence_sub_callback, profile, &errmsg);
				switch_mutex_unlock(profile->ireg_mutex);
				switch_core_db_close(db);
			}

		}
		switch_mutex_unlock(mod_sofia_globals.hash_mutex);

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
			switch_core_db_t *db = NULL;
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
				(sql =
				 switch_mprintf("select user,host,status,rpid,'' from sofia_handle_sip_registrations where user='%q' and host='%q'",
								euser, host)) && (profile = sofia_glue_find_profile(host))) {
				if (!(db = switch_core_db_open_file(profile->dbname))) {
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Error Opening DB %s\n", profile->dbname);
					switch_safe_free(user);
					switch_safe_free(sql);
					return;
				}

				switch_mutex_lock(profile->ireg_mutex);
				switch_core_db_exec(db, sql, sofia_presence_resub_callback, profile, &errmsg);
				switch_mutex_unlock(profile->ireg_mutex);
				switch_safe_free(sql);
			}
			switch_safe_free(user);
			switch_core_db_close(db);
		}
		return;
	case SWITCH_EVENT_PRESENCE_IN:
		sql =
			switch_mprintf
			("select 1,'%q','%q',* from sip_subscriptions where proto='%q' and event='%q' and sub_to_user='%q' and sub_to_host='%q'",
			 status, rpid, proto, event_type, euser, host);
		break;
	case SWITCH_EVENT_PRESENCE_OUT:
		sql =
			switch_mprintf
			("select 0,'%q','%q',* from sip_subscriptions where proto='%q' and event='%q' and sub_to_user='%q' and sub_to_host='%q'",
			 status, rpid, proto, event_type, euser, host);
		break;
	default:
		break;
	}

	switch_mutex_lock(mod_sofia_globals.hash_mutex);
	for (hi = switch_hash_first(switch_hash_pool_get(mod_sofia_globals.profile_hash), mod_sofia_globals.profile_hash); hi; hi = switch_hash_next(hi)) {
		switch_hash_this(hi, NULL, NULL, &val);
		profile = (sofia_profile_t *) val;
		if (!(profile->pflags & PFLAG_PRESENCE)) {
			continue;
		}

		if (sql) {
			if (!(db = switch_core_db_open_file(profile->dbname))) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Error Opening DB %s\n", profile->dbname);
				continue;
			}
			switch_mutex_lock(profile->ireg_mutex);
			switch_core_db_exec(db, sql, sofia_presence_sub_callback, profile, &errmsg);
			switch_mutex_unlock(profile->ireg_mutex);

			switch_core_db_close(db);
		}
	}
	switch_mutex_unlock(mod_sofia_globals.hash_mutex);

	switch_safe_free(sql);
	switch_safe_free(user);
}

int sofia_presence_sub_reg_callback(void *pArg, int argc, char **argv, char **columnNames)
{
	sofia_profile_t *profile = (sofia_profile_t *) pArg;
	//char *proto = argv[0];
	char *user = argv[1];
	char *host = argv[2];
	switch_event_t *event;
	char *status = NULL;
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

int sofia_presence_resub_callback(void *pArg, int argc, char **argv, char **columnNames)
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

int sofia_presence_sub_callback(void *pArg, int argc, char **argv, char **columnNames)
{
	sofia_profile_t *profile = (sofia_profile_t *) pArg;
	char *pl;
	char *id, *note;
	uint32_t in = atoi(argv[0]);
	char *status = argv[1];
	char *rpid = argv[2];
	char *proto = argv[3];
	char *user = argv[4];
	char *host = argv[5];
	char *sub_to_user = argv[6];
	char *sub_to_host = argv[7];
	char *event = argv[8];
	char *contact = argv[9];
	char *callid = argv[10];
	char *full_from = argv[11];
	char *full_via = argv[12];
	nua_handle_t *nh;
	char *to;
	char *open;
	char *tmp;

	if (!rpid) {
		rpid = "unknown";
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
						"<tuple id='t6a5ed77e'>\r\n"
						"<status>\r\n"
						"<basic>%s</basic>\r\n"
						"</status>\r\n"
						"</tuple>\r\n"
						"<dm:person id='p06360c4a'>\r\n"
						"<rpid:activities>\r\n" "<rpid:%s/>\r\n" "</rpid:activities>%s</dm:person>\r\n" "</presence>", id, open, rpid, note);



	nh = nua_handle(profile->nua, NULL, TAG_END());
	tmp = contact;
	contact = sofia_glue_get_url_from_contact(tmp, 0);

	nua_notify(nh,
			   NUTAG_URL(contact),
			   SIPTAG_TO_STR(full_from),
			   SIPTAG_FROM_STR(id),
			   SIPTAG_CONTACT_STR(profile->url),
			   SIPTAG_CALL_ID_STR(callid),
			   SIPTAG_VIA_STR(full_via),
			   SIPTAG_SUBSCRIPTION_STATE_STR("active;expires=3600"),
			   SIPTAG_EVENT_STR(event), SIPTAG_CONTENT_TYPE_STR("application/pidf+xml"), SIPTAG_PAYLOAD_STR(pl), TAG_END());

	switch_safe_free(id);
	switch_safe_free(note);
	switch_safe_free(pl);
	switch_safe_free(to);

	return 0;
}

void sofia_presence_handle_sip_i_subscribe(int status,
							char const *phrase,
							nua_t * nua, sofia_profile_t * profile, nua_handle_t * nh, sofia_private_t * sofia_private, sip_t const *sip, tagi_t tags[])
{
	if (sip) {
		long exp, exp_raw;
		sip_to_t const *to = sip->sip_to;
		sip_from_t const *from = sip->sip_from;
		sip_contact_t const *contact = sip->sip_contact;
		char *from_user = NULL;
		char *from_host = NULL;
		char *to_user = NULL;
		char *to_host = NULL;
		char *sql, *event = NULL;
		char *proto = "sip";
		char *d_user = NULL;
		char *contact_str = "";
		char *call_id = NULL;
		char *to_str = NULL;
		char *full_from = NULL;
		char *full_via = NULL;
		switch_core_db_t *db;
		char *errmsg;
		char *sstr;
		const char *display = "\"user\"";
		switch_event_t *sevent;

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
			to_user = (char *) to->a_url->url_user;
			to_host = (char *) to->a_url->url_host;
		}


		if (strstr(to_user, "ext+") || strstr(to_user, "user+") || strstr(to_user, "conf+")) {
			char proto[80];
			char *p;

			switch_copy_string(proto, to_user, sizeof(proto));
			if ((p = strchr(proto, '+'))) {
				*p = '\0';
			}

			if (switch_event_create(&sevent, SWITCH_EVENT_PRESENCE_IN) == SWITCH_STATUS_SUCCESS) {
				switch_event_add_header(sevent, SWITCH_STACK_BOTTOM, "proto", SOFIA_CHAT_PROTO);
				switch_event_add_header(sevent, SWITCH_STACK_BOTTOM, "login", "%s", profile->name);
				switch_event_add_header(sevent, SWITCH_STACK_BOTTOM, "from", "%s@%s", to_user, to_host);
				switch_event_add_header(sevent, SWITCH_STACK_BOTTOM, "rpid", "unknown");
				switch_event_add_header(sevent, SWITCH_STACK_BOTTOM, "status", "Click To Call");
				switch_event_fire(&sevent);
			}
		}

		if (strchr(to_user, '+')) {
			char *h;
			if ((proto = (d_user = strdup(to_user)))) {
				if ((to_user = strchr(d_user, '+'))) {
					*to_user++ = '\0';
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

		call_id = sip_header_as_string(profile->home, (void *) sip->sip_call_id);
		event = sip_header_as_string(profile->home, (void *) sip->sip_event);
		full_from = sip_header_as_string(profile->home, (void *) sip->sip_from);
		full_via = sip_header_as_string(profile->home, (void *) sip->sip_via);

		exp_raw = (sip->sip_expires ? sip->sip_expires->ex_delta : 3600);
		exp = (long) time(NULL) + exp_raw;

		if (sip && sip->sip_from) {
			from_user = (char *) sip->sip_from->a_url->url_user;
			from_host = (char *) sip->sip_from->a_url->url_host;
		} else {
			from_user = "n/a";
			from_host = "n/a";
		}

		if ((sql = switch_mprintf("delete from sip_subscriptions where "
								  "proto='%q' and user='%q' and host='%q' and sub_to_user='%q' and sub_to_host='%q' and event='%q';\n"
								  "insert into sip_subscriptions values ('%q','%q','%q','%q','%q','%q','%q','%q','%q','%q',%ld)",
								  proto,
								  from_user,
								  from_host,
								  to_user,
								  to_host, event, proto, from_user, from_host, to_user, to_host, event, contact_str, call_id, full_from, full_via, exp))) {
			sofia_glue_execute_sql(profile->dbname, sql, profile->ireg_mutex);
			switch_safe_free(sql);
		}

		sstr = switch_mprintf("active;expires=%ld", exp_raw);

		nua_respond(nh, SIP_202_ACCEPTED,
					NUTAG_WITH_THIS(nua),
					SIPTAG_SUBSCRIPTION_STATE_STR(sstr), SIPTAG_FROM(sip->sip_to), SIPTAG_TO(sip->sip_from), SIPTAG_CONTACT_STR(to_str), TAG_END());



		switch_safe_free(sstr);

		if (!(db = switch_core_db_open_file(profile->dbname))) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Error Opening DB %s\n", profile->dbname);
			goto end;
		}
		if ((sql = switch_mprintf("select * from sip_subscriptions where user='%q' and host='%q'", to_user, to_host, to_user, to_host))) {
			switch_mutex_lock(profile->ireg_mutex);
			switch_core_db_exec(db, sql, sofia_presence_sub_reg_callback, profile, &errmsg);
			switch_mutex_unlock(profile->ireg_mutex);
			switch_safe_free(sql);
		}
		switch_core_db_close(db);
	  end:

		if (event) {
			su_free(profile->home, event);
		}
		if (call_id) {
			su_free(profile->home, call_id);
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
							nua_t * nua, sofia_profile_t * profile, nua_handle_t * nh, sofia_private_t * sofia_private, sip_t const *sip, tagi_t tags[])
{

}

void sofia_presence_handle_sip_i_publish(nua_t * nua, sofia_profile_t * profile, nua_handle_t * nh, sofia_private_t * sofia_private, sip_t const *sip, tagi_t tags[])
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
					 switch_mprintf("update sofia_handle_sip_registrations set status='%q',rpid='%q' where user='%q' and host='%q'",
									note_txt, rpid, from_user, from_host))) {
					sofia_glue_execute_sql(profile->dbname, sql, profile->ireg_mutex);
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

	snprintf(hash_key, len, "%s%s%s", (char *) sip->sip_from->a_url->url_user, (char *) sip->sip_from->a_url->url_host,
			 (char *) sip->sip_to->a_url->url_user);


#if 0
	/* nicer one we cant use in both directions >=0 */
	snprintf(hash_key, len, "%s%s%s%s%s%s",
			 (char *) sip->sip_to->a_url->url_user,
			 (char *) sip->sip_to->a_url->url_host,
			 (char *) sip->sip_to->a_url->url_params,
			 (char *) sip->sip_from->a_url->url_user, (char *) sip->sip_from->a_url->url_host, (char *) sip->sip_from->a_url->url_params);
#endif

}


void sofia_presence_handle_sip_i_message(int status,
						  char const *phrase,
						  nua_t * nua, sofia_profile_t * profile, nua_handle_t * nh, sofia_private_t * sofia_private, sip_t const *sip, tagi_t tags[])
{
	if (sip) {
		sip_from_t const *from = sip->sip_from;
		char *from_user = NULL;
		char *from_host = NULL;
		sip_to_t const *to = sip->sip_to;
		char *to_user = NULL;
		char *to_host = NULL;
		sip_subject_t const *sip_subject = sip->sip_subject;
		sip_payload_t *payload = sip->sip_payload;
		const char *subject = "n/a";
		char *msg = NULL;

		if (sip->sip_content_type) {
			if (strstr((char *) sip->sip_content_type->c_subtype, "composing")) {
				return;
			}
		}

		if (from) {
			from_user = (char *) from->a_url->url_user;
			from_host = (char *) from->a_url->url_host;
		}

		if (to) {
			to_user = (char *) to->a_url->url_user;
			to_host = (char *) to->a_url->url_host;
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
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Invalid Chat Interface [%s]!\n", proto ? proto : "(none)");
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

void sofia_presence_set_chat_hash(private_object_t * tech_pvt, sip_t const *sip)
{
	char hash_key[256] = "";
	char buf[512];

	if (tech_pvt->hash_key || !sip || !sip->sip_from || !sip->sip_from->a_url || !sip->sip_from->a_url->url_user || !sip->sip_from->a_url->url_host) {
		return;
	}

	if (sofia_reg_find_reg_url(tech_pvt->profile, sip->sip_from->a_url->url_user, sip->sip_from->a_url->url_host, buf, sizeof(buf))) {
		tech_pvt->chat_from = sip_header_as_string(tech_pvt->home, (const sip_header_t *) sip->sip_to);
		tech_pvt->chat_to = switch_core_session_strdup(tech_pvt->session, buf);
		sofia_presence_set_hash_key(hash_key, sizeof(hash_key), sip);
	} else {
		return;
	}

	tech_pvt->hash_key = switch_core_session_strdup(tech_pvt->session, hash_key);
	switch_core_hash_insert(tech_pvt->profile->chat_hash, tech_pvt->hash_key, tech_pvt);

}
