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
 * sofia.c -- SOFIA SIP Endpoint (sofia code)
 *
 */
#include "mod_sofia.h"
extern su_log_t tport_log[];

static void sofia_handle_sip_i_state(switch_core_session_t *session, int status,
									 char const *phrase,
									 nua_t *nua, sofia_profile_t *profile, nua_handle_t *nh, sofia_private_t *sofia_private, sip_t const *sip, tagi_t tags[]);

void sofia_event_callback(nua_event_t event,
						   int status,
						   char const *phrase,
						   nua_t *nua, sofia_profile_t *profile, nua_handle_t *nh, sofia_private_t *sofia_private, sip_t const *sip, tagi_t tags[])
{
	struct private_object *tech_pvt = NULL;
	auth_res_t auth_res = AUTH_FORBIDDEN;
	switch_core_session_t *session = NULL;
	switch_channel_t *channel = NULL;

	if (sofia_private) {
		if (!switch_strlen_zero(sofia_private->uuid)) {

			if ((session = switch_core_session_locate(sofia_private->uuid))) {
				tech_pvt = switch_core_session_get_private(session);
				channel = switch_core_session_get_channel(tech_pvt->session);
				if (switch_channel_test_flag(channel, CF_NOMEDIA)) {
					switch_set_flag(tech_pvt, TFLAG_NOMEDIA);
				}
				if (!tech_pvt->call_id && sip && sip->sip_call_id && sip->sip_call_id->i_id) {
					tech_pvt->call_id = switch_core_session_strdup(session, (char *) sip->sip_call_id->i_id);
					switch_channel_set_variable(channel, "sip_call_id", tech_pvt->call_id);
				}
			} else {
				/* too late */
				return;
			}
		}
	}

	
	if (status != 100 && status != 200) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "event [%s] status [%d][%s] session: %s\n",
						  nua_event_name(event), status, phrase, session ? switch_channel_get_name(channel) : "n/a");
	}

	if ((profile->pflags & PFLAG_AUTH_ALL) && tech_pvt && tech_pvt->key && sip) {
		sip_authorization_t const *authorization = NULL;
		
		if (sip->sip_authorization) {
			authorization = sip->sip_authorization;
		} else if (sip->sip_proxy_authorization) {
			authorization = sip->sip_proxy_authorization;
		}

		if (authorization) {
			char network_ip[80];
			get_addr(network_ip, sizeof(network_ip), &((struct sockaddr_in *) msg_addrinfo(nua_current_request(nua))->ai_addr)->sin_addr);
			auth_res = sofia_reg_parse_auth(profile, authorization, 
											(char *) sip->sip_request->rq_method_name, tech_pvt->key, strlen(tech_pvt->key), network_ip);
		}

		if (auth_res != AUTH_OK) {
			switch_channel_hangup(channel, SWITCH_CAUSE_DESTINATION_OUT_OF_ORDER);
			nua_respond(nh, SIP_401_UNAUTHORIZED, TAG_END());
			goto done;
		}

		if (channel) {
			switch_channel_set_variable(channel, "sip_authorized", "true");
		}
	}

	if (sip && (status == 401 || status == 407)) {
		sofia_reg_handle_sip_r_challenge(status, phrase, nua, profile, nh, session, sip, tags);
		goto done;
	}

	switch (event) {
	case nua_r_shutdown:
		if (status >= 200) {
			su_root_break(profile->s_root);
		}
		break;
	case nua_r_get_params:
	case nua_r_invite:
	case nua_r_unregister:
	case nua_r_options:
	case nua_i_fork:
	case nua_r_info:
	case nua_r_bye:
	case nua_i_bye:
	case nua_r_unsubscribe:
	case nua_r_publish:
	case nua_r_message:
	case nua_r_notify:
	case nua_i_notify:
	case nua_i_cancel:
	case nua_i_error:
	case nua_i_active:
	case nua_i_ack:
	case nua_i_terminated:
	case nua_r_set_params:
		break;
	case nua_r_register:
		sofia_reg_handle_sip_r_register(status, phrase, nua, profile, nh, sofia_private, sip, tags);
		break;
	case nua_i_options:
		sofia_handle_sip_i_options(status, phrase, nua, profile, nh, sofia_private, sip, tags);
		break;
	case nua_i_invite:
		if (!session) {
			sofia_handle_sip_i_invite(nua, profile, nh, sofia_private, sip, tags);
		}
		break;
	case nua_i_publish:
		sofia_presence_handle_sip_i_publish(nua, profile, nh, sofia_private, sip, tags);
		break;
	case nua_i_register:
		sofia_reg_handle_sip_i_register(nua, profile, nh, sofia_private, sip, tags);
		break;
	case nua_i_prack:
		break;
	case nua_i_state:
		sofia_handle_sip_i_state(session, status, phrase, nua, profile, nh, sofia_private, sip, tags);
		break;
	case nua_i_message:
		sofia_presence_handle_sip_i_message(status, phrase, nua, profile, nh, sofia_private, sip, tags);
		break;
	case nua_i_info:
		sofia_handle_sip_i_info(nua, profile, nh, session, sip, tags);
		break;
	case nua_r_refer:
		break;
	case nua_i_refer:
		if (session) {
			sofia_handle_sip_i_refer(nua, profile, nh, session, sip, tags);
		}
		break;
	case nua_r_subscribe:
		sofia_presence_handle_sip_r_subscribe(status, phrase, nua, profile, nh, sofia_private, sip, tags);
		break;
	case nua_i_subscribe:
		sofia_presence_handle_sip_i_subscribe(status, phrase, nua, profile, nh, sofia_private, sip, tags);
		break;
	default:
		if (status > 100) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "%s: unknown event %d: %03d %s\n", nua_event_name(event), event, status, phrase);
		} else {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "%s: unknown event %d\n", nua_event_name(event), event);
		}
		break;
	}

  done:

	if (session) {
		switch_core_session_rwunlock(session);

	}
}


void event_handler(switch_event_t *event)
{
	char *subclass, *sql;

	if ((subclass = switch_event_get_header(event, "orig-event-subclass")) && !strcasecmp(subclass, MY_EVENT_REGISTER)) {
		char *from_user = switch_event_get_header(event, "orig-from-user");
		char *from_host = switch_event_get_header(event, "orig-from-host");
		char *contact_str = switch_event_get_header(event, "orig-contact");
		char *exp_str = switch_event_get_header(event, "orig-expires");
		char *rpid = switch_event_get_header(event, "orig-rpid");
		long expires = (long) time(NULL) + atol(exp_str);
		char *profile_name = switch_event_get_header(event, "orig-profile-name");
		sofia_profile_t *profile = NULL;
		char buf[512];

		if (!rpid) {
			rpid = "unknown";
		}

		if (!profile_name || !(profile = sofia_glue_find_profile(profile_name))) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Invalid Profile\n");
			return;
		}


		if (!sofia_reg_find_reg_url(profile, from_user, from_host, buf, sizeof(buf))) {
			sql = switch_mprintf("insert into sip_registrations values ('%q','%q','%q','Regestered', '%q', %ld)",
								 from_user, from_host, contact_str, rpid, expires);
		} else {
			sql =
				switch_mprintf
				("update sip_registrations set contact='%q', rpid='%q', expires=%ld where user='%q' and host='%q'",
				 contact_str, rpid, expires, from_user, from_host);

		}

		if (sql) {
			sofia_glue_execute_sql(profile, SWITCH_FALSE, sql, profile->ireg_mutex);
			switch_safe_free(sql);
			sql = NULL;
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Propagating registration for %s@%s->%s\n", from_user, from_host, contact_str);

		}

		if (profile) {
			switch_thread_rwlock_unlock(profile->rwlock);
		}
	}
}



void *SWITCH_THREAD_FUNC sofia_profile_thread_run(switch_thread_t *thread, void *obj)
{
	sofia_profile_t *profile = (sofia_profile_t *) obj;
	switch_memory_pool_t *pool;
	sip_alias_node_t *node;
	uint32_t ireg_loops = 0;
	uint32_t gateway_loops = 0;
	switch_event_t *s_event;

	switch_mutex_lock(mod_sofia_globals.mutex);
	mod_sofia_globals.threads++;
	switch_mutex_unlock(mod_sofia_globals.mutex);

	profile->s_root = su_root_create(NULL);
	profile->home = su_home_new(sizeof(*profile->home));

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Creating agent for %s\n", profile->name);

	profile->nua = nua_create(profile->s_root,	/* Event loop */
							  sofia_event_callback,	/* Callback for processing events */
							  profile,	/* Additional data to pass to callback */
							  NUTAG_URL(profile->bindurl), NTATAG_UDP_MTU(65536), TAG_END());	/* Last tag should always finish the sequence */

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Created agent for %s\n", profile->name);

	nua_set_params(profile->nua,
				   //NUTAG_EARLY_MEDIA(1),                 
				   NUTAG_AUTOANSWER(0),
				   NUTAG_AUTOALERT(0),
				   NUTAG_ALLOW("REGISTER"),
				   NUTAG_ALLOW("REFER"),
				   NUTAG_ALLOW("INFO"),
				   TAG_IF((profile->pflags & PFLAG_PRESENCE), NUTAG_ALLOW("PUBLISH")),
				   TAG_IF((profile->pflags & PFLAG_PRESENCE), NUTAG_ALLOW("NOTIFY")),
				   TAG_IF((profile->pflags & PFLAG_PRESENCE), NUTAG_ALLOW("SUBSCRIBE")),
				   TAG_IF((profile->pflags & PFLAG_PRESENCE), NUTAG_ENABLEMESSAGE(1)),
				   TAG_IF((profile->pflags & PFLAG_PRESENCE), NUTAG_ALLOW_EVENTS("presence")),
				   TAG_IF((profile->pflags & PFLAG_PRESENCE), NUTAG_ALLOW_EVENTS("presence.winfo")),
				   TAG_IF((profile->pflags & PFLAG_PRESENCE), NUTAG_ALLOW_EVENTS("message-summary")),
				   SIPTAG_SUPPORTED_STR("100rel, precondition"), SIPTAG_USER_AGENT_STR(SOFIA_USER_AGENT), TAG_END());

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Set params for %s\n", profile->name);

	for (node = profile->aliases; node; node = node->next) {
		node->nua = nua_create(profile->s_root,	/* Event loop */
							   sofia_event_callback,	/* Callback for processing events */
							   profile,	/* Additional data to pass to callback */
							   NUTAG_URL(node->url), TAG_END());	/* Last tag should always finish the sequence */

		nua_set_params(node->nua,
					   NUTAG_EARLY_MEDIA(1),
					   NUTAG_AUTOANSWER(0),
					   NUTAG_AUTOALERT(0),
					   NUTAG_ALLOW("REGISTER"),
					   NUTAG_ALLOW("REFER"),
					   NUTAG_ALLOW("INFO"),
					   TAG_IF((profile->pflags & PFLAG_PRESENCE), NUTAG_ALLOW("PUBLISH")),
					   TAG_IF((profile->pflags & PFLAG_PRESENCE), NUTAG_ENABLEMESSAGE(1)),
					   SIPTAG_SUPPORTED_STR("100rel, precondition"), SIPTAG_USER_AGENT_STR(SOFIA_USER_AGENT), TAG_END());

	}


	if (!sofia_glue_init_sql(profile)) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Cannot Open SQL Database!\n");
		goto end;
	}

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "activated db for %s\n", profile->name);

	switch_mutex_init(&profile->ireg_mutex, SWITCH_MUTEX_NESTED, profile->pool);
	switch_mutex_init(&profile->gateway_mutex, SWITCH_MUTEX_NESTED, profile->pool);

	ireg_loops = IREG_SECONDS;
	gateway_loops = GATEWAY_SECONDS;

	if (switch_event_create(&s_event, SWITCH_EVENT_PUBLISH) == SWITCH_STATUS_SUCCESS) {
		switch_event_add_header(s_event, SWITCH_STACK_BOTTOM, "service", "_sip._udp");
		switch_event_add_header(s_event, SWITCH_STACK_BOTTOM, "port", "%d", profile->sip_port);
		switch_event_fire(&s_event);
	}

	if (switch_event_create(&s_event, SWITCH_EVENT_PUBLISH) == SWITCH_STATUS_SUCCESS) {
		switch_event_add_header(s_event, SWITCH_STACK_BOTTOM, "service", "_sip._tcp");
		switch_event_add_header(s_event, SWITCH_STACK_BOTTOM, "port", "%d", profile->sip_port);
		switch_event_fire(&s_event);
	}

	if (switch_event_create(&s_event, SWITCH_EVENT_PUBLISH) == SWITCH_STATUS_SUCCESS) {
		switch_event_add_header(s_event, SWITCH_STACK_BOTTOM, "service", "_sip._sctp");
		switch_event_add_header(s_event, SWITCH_STACK_BOTTOM, "port", "%d", profile->sip_port);
		switch_event_fire(&s_event);
	}

	sofia_glue_add_profile(profile->name, profile);

	if (profile->pflags & PFLAG_PRESENCE) {
		sofia_presence_establish_presence(profile);
	}

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Starting thread for %s\n", profile->name);

	
	sofia_set_pflag_locked(profile, PFLAG_RUNNING);
	while (mod_sofia_globals.running == 1 && sofia_test_pflag(profile, PFLAG_RUNNING)) {
		if (++ireg_loops >= IREG_SECONDS) {
			sofia_reg_check_expire(profile, time(NULL));
			ireg_loops = 0;
		}

		if (++gateway_loops >= GATEWAY_SECONDS) {
			sofia_reg_check_gateway(profile, time(NULL));
			gateway_loops = 0;
		}

		su_root_step(profile->s_root, 1000);
	}

	//sofia_reg_check_expire(profile, 0);
	//sofia_reg_check_gateway(profile, 0);	
	switch_thread_rwlock_wrlock(profile->rwlock);
	sofia_reg_unregister(profile);
	nua_shutdown(profile->nua);


	su_root_run(profile->s_root);
	nua_destroy(profile->nua);
	while(profile->inuse) {
		switch_yield(100000);
	}

	if (switch_event_create(&s_event, SWITCH_EVENT_UNPUBLISH) == SWITCH_STATUS_SUCCESS) {
		switch_event_add_header(s_event, SWITCH_STACK_BOTTOM, "service", "_sip._udp");
		switch_event_add_header(s_event, SWITCH_STACK_BOTTOM, "port", "%d", profile->sip_port);
		switch_event_fire(&s_event);
	}

	sofia_glue_sql_close(profile);
	su_home_unref(profile->home);
	su_root_destroy(profile->s_root);
	pool = profile->pool;

	switch_mutex_lock(mod_sofia_globals.hash_mutex);
	switch_core_hash_delete(mod_sofia_globals.profile_hash, profile->name);
	switch_mutex_unlock(mod_sofia_globals.hash_mutex);

	switch_thread_rwlock_unlock(profile->rwlock);
	
	switch_core_destroy_memory_pool(&pool);


 end:
	
	switch_mutex_lock(mod_sofia_globals.mutex);
	mod_sofia_globals.threads--;
	switch_mutex_unlock(mod_sofia_globals.mutex);

	return NULL;
}

void launch_sofia_profile_thread(sofia_profile_t *profile)
{
	switch_thread_t *thread;
	switch_threadattr_t *thd_attr = NULL;

	switch_threadattr_create(&thd_attr, profile->pool);
	switch_threadattr_detach_set(thd_attr, 1);
	switch_threadattr_stacksize_set(thd_attr, SWITCH_THREAD_STACKSIZE);
	switch_thread_create(&thread, thd_attr, sofia_profile_thread_run, profile, profile->pool);
}

static void logger(void *logarg, char const *fmt, va_list ap)
{
	char *data = NULL;

	if (fmt) {
#ifdef HAVE_VASPRINTF
		int ret;
		ret = vasprintf(&data, fmt, ap);
		if ((ret == -1) || !data) {
			return;
		}
#else
		data = (char *) malloc(2048);
		if (data) {
			vsnprintf(data, 2048, fmt, ap);
		} else {
			return;
		}
#endif
	}
	if (data) {
		switch_log_printf(SWITCH_CHANNEL_LOG_CLEAN, SWITCH_LOG_CONSOLE, (char *) "%s", data);
		free(data);
	}
}


switch_status_t config_sofia(int reload, char *profile_name)
{
	char *cf = "sofia.conf";
	switch_xml_t cfg, xml = NULL, xprofile, param, settings, profiles, gateway_tag, gateways_tag;
	switch_status_t status = SWITCH_STATUS_SUCCESS;
	sofia_profile_t *profile = NULL;
	char url[512] = "";
	sofia_gateway_t *gp;
	int profile_found = 0;

	if (!reload) {
		su_init();
		su_log_redirect(NULL, logger, NULL);
		su_log_redirect(tport_log, logger, NULL);
	}


	if (!switch_strlen_zero(profile_name) && (profile = sofia_glue_find_profile(profile_name))) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Profile [%s] Already exists.\n", switch_str_nil(profile_name));
		status = SWITCH_STATUS_FALSE;
		switch_thread_rwlock_unlock(profile->rwlock);
		return status;
	}

	snprintf(url, sizeof(url), "profile=%s", switch_str_nil(profile_name));

	if (!(xml = switch_xml_open_cfg(cf, &cfg, url))) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "open of %s failed\n", cf);
		status = SWITCH_STATUS_FALSE;
		goto done;
	}

	if ((settings = switch_xml_child(cfg, "global_settings"))) {
		for (param = switch_xml_child(settings, "param"); param; param = param->next) {
			char *var = (char *) switch_xml_attr_soft(param, "name");
			char *val = (char *) switch_xml_attr_soft(param, "value");
			if (!strcasecmp(var, "log-level")) {
				su_log_set_level(NULL, atoi(val));
			} else if (!strcasecmp(var, "log-level-trace")) {
				su_log_set_level(tport_log, atoi(val));
			}
		}
	}

	if ((profiles = switch_xml_child(cfg, "profiles"))) {
		for (xprofile = switch_xml_child(profiles, "profile"); xprofile; xprofile = xprofile->next) {
			if (!(settings = switch_xml_child(xprofile, "settings"))) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "No Settings, check the new config!\n");
			} else {
				char *xprofilename = (char *) switch_xml_attr_soft(xprofile, "name");
				switch_memory_pool_t *pool = NULL;

				if (!xprofilename) {
					xprofilename = "unnamed";
				}
				
				if (profile_name) {
					if (strcasecmp(profile_name, xprofilename)) {
						continue;
					} else {
						profile_found = 1;
					}
				}
				
				/* Setup the pool */
				if ((status = switch_core_new_memory_pool(&pool)) != SWITCH_STATUS_SUCCESS) {
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Memory Error!\n");
					goto done;
				}

				if (!(profile = (sofia_profile_t *) switch_core_alloc(pool, sizeof(*profile)))) {
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Memory Error!\n");
					goto done;
				}

				

				profile->pool = pool;

				profile->name = switch_core_strdup(profile->pool, xprofilename);
				snprintf(url, sizeof(url), "sofia_reg_%s", xprofilename);
				
				profile->dbname = switch_core_strdup(profile->pool, url);
				switch_core_hash_init(&profile->chat_hash, profile->pool);
				switch_thread_rwlock_create(&profile->rwlock, profile->pool);
				switch_mutex_init(&profile->flag_mutex, SWITCH_MUTEX_NESTED, profile->pool);
				profile->dtmf_duration = 100;
				profile->codec_ms = 20;

				for (param = switch_xml_child(settings, "param"); param; param = param->next) {
					char *var = (char *) switch_xml_attr_soft(param, "name");
					char *val = (char *) switch_xml_attr_soft(param, "value");

					if (!strcasecmp(var, "debug")) {
						profile->debug = atoi(val);
					} else if (!strcasecmp(var, "use-rtp-timer") && switch_true(val)) {
						switch_set_flag(profile, TFLAG_TIMER);

					} else if (!strcasecmp(var, "odbc-dsn")) {
#ifdef SWITCH_HAVE_ODBC
						profile->odbc_dsn = switch_core_strdup(profile->pool, val);
						if ((profile->odbc_user = strchr(profile->odbc_dsn, ':'))) {
							*profile->odbc_user++ = '\0';
						}
						if ((profile->odbc_pass = strchr(profile->odbc_user, ':'))) {
							*profile->odbc_pass++ = '\0';
						}
				
#else
						switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "ODBC IS NOT AVAILABLE!\n");
#endif
						
					} else if (!strcasecmp(var, "inbound-no-media") && switch_true(val)) {
						switch_set_flag(profile, TFLAG_INB_NOMEDIA);
					} else if (!strcasecmp(var, "inbound-late-negotiation") && switch_true(val)) {
						switch_set_flag(profile, TFLAG_LATE_NEGOTIATION);
					} else if (!strcasecmp(var, "rfc2833-pt")) {
						profile->te = (switch_payload_t) atoi(val);
					} else if (!strcasecmp(var, "cng-pt")) {
						profile->cng_pt = (switch_payload_t) atoi(val);
					} else if (!strcasecmp(var, "sip-port")) {
						profile->sip_port = atoi(val);
					} else if (!strcasecmp(var, "vad")) {
						if (!strcasecmp(val, "in")) {
							switch_set_flag(profile, TFLAG_VAD_IN);
						} else if (!strcasecmp(val, "out")) {
							switch_set_flag(profile, TFLAG_VAD_OUT);
						} else if (!strcasecmp(val, "both")) {
							switch_set_flag(profile, TFLAG_VAD_IN);
							switch_set_flag(profile, TFLAG_VAD_OUT);
						} else {
							switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Invald option %s for VAD\n", val);
						}
					} else if (!strcasecmp(var, "ext-rtp-ip")) {
						profile->extrtpip = switch_core_strdup(profile->pool, strcasecmp(val, "auto") ? val : mod_sofia_globals.guess_ip);
					} else if (!strcasecmp(var, "rtp-ip")) {
						profile->rtpip = switch_core_strdup(profile->pool, strcasecmp(val, "auto") ? val : mod_sofia_globals.guess_ip);
					} else if (!strcasecmp(var, "sip-ip")) {
						profile->sipip = switch_core_strdup(profile->pool, strcasecmp(val, "auto") ? val : mod_sofia_globals.guess_ip);
					} else if (!strcasecmp(var, "ext-sip-ip")) {
						if (!strcasecmp(val, "auto")) {
							profile->extsipip = switch_core_strdup(profile->pool, mod_sofia_globals.guess_ip);
						} else {
							char *ip = NULL;
							switch_port_t port = 0;
							if (sofia_glue_ext_address_lookup(&ip, &port, val, profile->pool) == SWITCH_STATUS_SUCCESS) {

								if (ip) {
									profile->extsipip = switch_core_strdup(profile->pool, ip);
								}
							}
						}

					} else if (!strcasecmp(var, "sip-domain")) {
						profile->sipdomain = switch_core_strdup(profile->pool, val);
					} else if (!strcasecmp(var, "rtp-timer-name")) {
						profile->timer_name = switch_core_strdup(profile->pool, val);
					} else if (!strcasecmp(var, "hold-music")) {
						profile->hold_music = switch_core_strdup(profile->pool, val);
					} else if (!strcasecmp(var, "manage-presence")) {
						if (switch_true(val)) {
							profile->pflags |= PFLAG_PRESENCE;
						}
					} else if (!strcasecmp(var, "pass-rfc2833")) {
						if (switch_true(val)) {
							profile->pflags |= PFLAG_PASS_RFC2833;
						}
					} else if (!strcasecmp(var, "disable-transcoding")) {
						if (switch_true(val)) {
							profile->pflags |= PFLAG_DISABLE_TRANSCODING;
						}
					} else if (!strcasecmp(var, "rtp-rewrite-timestamps")) {
						if (switch_true(val)) {
							profile->pflags |= PFLAG_REWRITE_TIMESTAMPS;
						}
					} else if (!strcasecmp(var, "auth-calls")) {
						if (switch_true(val)) {
							profile->pflags |= PFLAG_AUTH_CALLS;
						}
					} else if (!strcasecmp(var, "nonce-ttl")) {
						profile->nonce_ttl = atoi(val);
					} else if (!strcasecmp(var, "accept-blind-reg")) {
						if (switch_true(val)) {
							profile->pflags |= PFLAG_BLIND_REG;
						}
					} else if (!strcasecmp(var, "auth-all-packets")) {
						if (switch_true(val)) {
							profile->pflags |= PFLAG_AUTH_ALL;
						}
					} else if (!strcasecmp(var, "full-id-in-dialplan")) {
						if (switch_true(val)) {
							profile->pflags |= PFLAG_FULL_ID;
						}
					} else if (!strcasecmp(var, "bitpacking")) {
						if (!strcasecmp(val, "aal2")) {
							profile->codec_flags = SWITCH_CODEC_FLAG_AAL2;
						}
					} else if (!strcasecmp(var, "username")) {
						profile->username = switch_core_strdup(profile->pool, val);
					} else if (!strcasecmp(var, "context")) {
						profile->context = switch_core_strdup(profile->pool, val);
					} else if (!strcasecmp(var, "alias")) {
						sip_alias_node_t *node;
						if ((node = switch_core_alloc(profile->pool, sizeof(*node)))) {
							if ((node->url = switch_core_strdup(profile->pool, val))) {
								node->next = profile->aliases;
								profile->aliases = node;
							}
						}
					} else if (!strcasecmp(var, "dialplan")) {
						profile->dialplan = switch_core_strdup(profile->pool, val);
					} else if (!strcasecmp(var, "max-calls")) {
						profile->max_calls = atoi(val);
					} else if (!strcasecmp(var, "codec-prefs")) {
						profile->codec_string = switch_core_strdup(profile->pool, val);
					} else if (!strcasecmp(var, "codec-ms")) {
						profile->codec_ms = atoi(val);
					} else if (!strcasecmp(var, "dtmf-duration")) {
						int dur = atoi(val);
						if (dur > 10 && dur < 8000) {
							profile->dtmf_duration = dur;
						} else {
							switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Duration out of bounds!\n");
						}
					}
				}

				if (!profile->cng_pt) {
					profile->cng_pt = SWITCH_RTP_CNG_PAYLOAD;
				}

				if (!profile->sipip) {
					profile->sipip = switch_core_strdup(profile->pool, mod_sofia_globals.guess_ip);
				}

				if (!profile->rtpip) {
					profile->rtpip = switch_core_strdup(profile->pool, mod_sofia_globals.guess_ip);
				}

				if (profile->nonce_ttl < 60) {
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "Setting nonce TTL to 60 seconds\n");
					profile->nonce_ttl = 60;
				}

				if (switch_test_flag(profile, TFLAG_TIMER) && !profile->timer_name) {
					profile->timer_name = switch_core_strdup(profile->pool, "soft");
				}

				if (!profile->username) {
					profile->username = switch_core_strdup(profile->pool, "FreeSWITCH");
				}

				if (!profile->rtpip) {
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "Setting ip to '127.0.0.1'\n");
					profile->rtpip = switch_core_strdup(profile->pool, "127.0.0.1");
				}

				if (!profile->sip_port) {
					profile->sip_port = atoi(SOFIA_DEFAULT_PORT);
				}

				if (!profile->dialplan) {
					profile->dialplan = switch_core_strdup(profile->pool, "XML");
				}

				if (!profile->sipdomain) {
					profile->sipdomain = switch_core_strdup(profile->pool, profile->sipip);
				}
				if (profile->extsipip) {
					profile->url = switch_core_sprintf(profile->pool, "sip:mod_sofia@%s:%d", profile->extsipip, profile->sip_port);
					profile->bindurl = switch_core_sprintf(profile->pool, "%s;maddr=%s", profile->url, profile->sipip);
				} else {
					profile->url = switch_core_sprintf(profile->pool, "sip:mod_sofia@%s:%d", profile->sipip, profile->sip_port);
					profile->bindurl = profile->url;
				}
			}
			if (profile) {
				if ((gateways_tag = switch_xml_child(xprofile, "registrations"))) {
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT,
									  "The <registrations> syntax has been discontinued, please see the new syntax in the default configuration examples\n");
				}

				if ((gateways_tag = switch_xml_child(xprofile, "gateways"))) {
					for (gateway_tag = switch_xml_child(gateways_tag, "gateway"); gateway_tag; gateway_tag = gateway_tag->next) {
						char *name = (char *) switch_xml_attr_soft(gateway_tag, "name");
						sofia_gateway_t *gateway;

						if (switch_strlen_zero(name)) {
							name = "anonymous";
						}

						if ((gateway = switch_core_alloc(profile->pool, sizeof(*gateway)))) {
							char *register_str = "true", *scheme = "Digest",
								*realm = NULL,
								*username = NULL,
								*password = NULL,
								*caller_id_in_from = "false",
								*extension = NULL,
								*proxy = NULL,
								*context = "default",
								*expire_seconds = "3600";

							gateway->pool = profile->pool;
							gateway->profile = profile;
							gateway->name = switch_core_strdup(gateway->pool, name);
							gateway->freq = 0;


							for (param = switch_xml_child(gateway_tag, "param"); param; param = param->next) {
								char *var = (char *) switch_xml_attr_soft(param, "name");
								char *val = (char *) switch_xml_attr_soft(param, "value");

								if (!strcmp(var, "register")) {
									register_str = val;
								} else if (!strcmp(var, "scheme")) {
									scheme = val;
								} else if (!strcmp(var, "realm")) {
									realm = val;
								} else if (!strcmp(var, "username")) {
									username = val;
								} else if (!strcmp(var, "password")) {
									password = val;
								} else if (!strcmp(var, "caller-id-in-from")) {
									caller_id_in_from = val;
								} else if (!strcmp(var, "extension")) {
									extension = val;
								} else if (!strcmp(var, "proxy")) {
									proxy = val;
								} else if (!strcmp(var, "context")) {
									context = val;
								} else if (!strcmp(var, "expire-seconds")) {
									expire_seconds = val;
								}
							}

							if (switch_strlen_zero(realm)) {
								realm = name;
							}

							if (switch_strlen_zero(username)) {
								switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "ERROR: username param is REQUIRED!\n");
								switch_xml_free(xml);
								goto skip;
							}

							if (switch_strlen_zero(password)) {
								switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "ERROR: password param is REQUIRED!\n");
								switch_xml_free(xml);
								goto skip;
							}

							if (switch_strlen_zero(extension)) {
								extension = username;
							}

							if (switch_strlen_zero(proxy)) {
								proxy = realm;
							}

							if (!switch_true(register_str)) {
								gateway->state = REG_STATE_NOREG;
							}
							gateway->register_scheme = switch_core_strdup(gateway->pool, scheme);
							gateway->register_context = switch_core_strdup(gateway->pool, context);
							gateway->register_realm = switch_core_strdup(gateway->pool, realm);
							gateway->register_username = switch_core_strdup(gateway->pool, username);
							gateway->register_password = switch_core_strdup(gateway->pool, password);
							if (switch_true(caller_id_in_from)) {
								switch_set_flag(gateway, REG_FLAG_CALLERID);
							}
							gateway->register_from = switch_core_sprintf(gateway->pool, "sip:%s@%s", username, realm);
							gateway->register_contact = switch_core_sprintf(gateway->pool, "sip:%s@%s:%d", extension, profile->sipip, profile->sip_port);

							if (!strncasecmp(proxy, "sip:", 4)) {
								gateway->register_proxy = switch_core_strdup(gateway->pool, proxy);
								gateway->register_to = switch_core_sprintf(gateway->pool, "sip:%s@%s", username, proxy + 4);
							} else {
								gateway->register_proxy = switch_core_sprintf(gateway->pool, "sip:%s", proxy);
								gateway->register_to = switch_core_sprintf(gateway->pool, "sip:%s@%s", username, proxy);
							}

							gateway->expires_str = switch_core_strdup(gateway->pool, expire_seconds);

							if ((gateway->freq = atoi(gateway->expires_str)) < 5) {
								switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING,
												  "Invalid Freq: %d.  Setting Register-Frequency to 3600\n", gateway->freq);
								gateway->freq = 3600;
							}
							gateway->freq -= 2;

							gateway->next = profile->gateways;
							profile->gateways = gateway;

							if ((gp = sofia_reg_find_gateway(gateway->name))) {
								switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "Ignoring duplicate gateway '%s'\n", gateway->name);
								sofia_reg_release_gateway(gp);
							} else if ((gp=sofia_reg_find_gateway(gateway->register_from))) {
								switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "Ignoring duplicate uri '%s'\n", gateway->register_from);
								sofia_reg_release_gateway(gp);
							} else if ((gp=sofia_reg_find_gateway(gateway->register_contact))) {
								switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "Ignoring duplicate contact '%s'\n", gateway->register_from);
								sofia_reg_release_gateway(gp);
							} else {
								sofia_reg_add_gateway(gateway->name, gateway);
								sofia_reg_add_gateway(gateway->register_from, gateway);
								sofia_reg_add_gateway(gateway->register_contact, gateway);
							}
						}

					  skip:
						assert(gateway_tag);
					}
				}

				if (profile->sipip) {
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "Started Profile %s [%s]\n", profile->name, url);
					launch_sofia_profile_thread(profile);
				} else {
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "Unable to start Profile %s due to no configured sip-ip\n", profile->name);
				}
				profile = NULL;
			}
			if (profile_found) {
				break;
			}
		}
	}
  done:
	if (xml) {
		switch_xml_free(xml);
	}

	if (profile_name && !profile_found) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "No Such Profile '%s'\n", profile_name);
		status = SWITCH_STATUS_FALSE;
	}

	return status;

}

static void sofia_handle_sip_i_state(switch_core_session_t *session, int status,
									 char const *phrase,
									 nua_t *nua, sofia_profile_t *profile, nua_handle_t *nh, sofia_private_t *sofia_private, sip_t const *sip, tagi_t tags[])
{
	const char *l_sdp = NULL, *r_sdp = NULL;
	int offer_recv = 0, answer_recv = 0, offer_sent = 0, answer_sent = 0;
	int ss_state = nua_callstate_init;
	switch_channel_t *channel = NULL;
	private_object_t *tech_pvt = NULL;
	const char *replaces_str = NULL;
	char *uuid;
	switch_core_session_t *other_session = NULL;
	switch_channel_t *other_channel = NULL;
	char st[80] = "";


	tl_gets(tags,
			NUTAG_CALLSTATE_REF(ss_state),
			NUTAG_OFFER_RECV_REF(offer_recv),
			NUTAG_ANSWER_RECV_REF(answer_recv),
			NUTAG_OFFER_SENT_REF(offer_sent),
			NUTAG_ANSWER_SENT_REF(answer_sent),
			SIPTAG_REPLACES_STR_REF(replaces_str), SOATAG_LOCAL_SDP_STR_REF(l_sdp), SOATAG_REMOTE_SDP_STR_REF(r_sdp), TAG_END());

	if (session) {
		channel = switch_core_session_get_channel(session);
		assert(channel != NULL);

		tech_pvt = switch_core_session_get_private(session);
		assert(tech_pvt != NULL);
		assert(tech_pvt->nh != NULL);
		


		if (switch_channel_test_flag(channel, CF_NOMEDIA)) {
			switch_set_flag(tech_pvt, TFLAG_NOMEDIA);
		}

		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Channel %s entering state [%s]\n",
						  switch_channel_get_name(channel), nua_callstate_name(ss_state));

		if (r_sdp) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Remote SDP:\n%s\n", r_sdp);
			tech_pvt->remote_sdp_str = switch_core_session_strdup(session, r_sdp);
			switch_channel_set_variable(channel, SWITCH_R_SDP_VARIABLE, r_sdp);
			sofia_glue_pass_sdp(tech_pvt, (char *) r_sdp);

		}
	}

	if (status == 988) {
		goto done;
	}

	switch ((enum nua_callstate) ss_state) {
	case nua_callstate_init:
		break;
	case nua_callstate_authenticating:
		break;
	case nua_callstate_calling:
		break;
	case nua_callstate_proceeding:
		if (channel) {
			if (status == 180) {
				switch_channel_mark_ring_ready(channel);
				if (!switch_channel_test_flag(channel, CF_GEN_RINGBACK)) {
					if (switch_test_flag(tech_pvt, TFLAG_NOMEDIA)) {
						if ((uuid = switch_channel_get_variable(channel, SWITCH_SIGNAL_BOND_VARIABLE))
							&& (other_session = switch_core_session_locate(uuid))) {
							switch_core_session_message_t msg;
							msg.message_id = SWITCH_MESSAGE_INDICATE_RINGING;
							msg.from = __FILE__;
							switch_core_session_receive_message(other_session, &msg);
							switch_core_session_rwunlock(other_session);
						}

					} else {
						switch_core_session_queue_indication(session, SWITCH_MESSAGE_INDICATE_RINGING);
					}
				}
			}

			if (r_sdp) {
				if (switch_test_flag(tech_pvt, TFLAG_NOMEDIA)) {
					switch_set_flag_locked(tech_pvt, TFLAG_EARLY_MEDIA);
					switch_channel_mark_pre_answered(channel);
					if (!switch_channel_test_flag(channel, CF_GEN_RINGBACK) && (uuid = switch_channel_get_variable(channel, SWITCH_SIGNAL_BOND_VARIABLE))
						&& (other_session = switch_core_session_locate(uuid))) {
						other_channel = switch_core_session_get_channel(other_session);
						if (!switch_channel_get_variable(other_channel, SWITCH_B_SDP_VARIABLE)) {
							switch_channel_set_variable(other_channel, SWITCH_B_SDP_VARIABLE, r_sdp);
						}

						switch_channel_pre_answer(other_channel);
						switch_core_session_rwunlock(other_session);
					}
					goto done;
				} else {
					if (switch_test_flag(tech_pvt, TFLAG_LATE_NEGOTIATION)) {
						switch_channel_set_variable(channel, SWITCH_ENDPOINT_DISPOSITION_VARIABLE, "DELAYED NEGOTIATION");
					} else {
						if (sofia_glue_tech_media(tech_pvt, (char *) r_sdp) != SWITCH_STATUS_SUCCESS) {
							switch_channel_set_variable(channel, SWITCH_ENDPOINT_DISPOSITION_VARIABLE, "CODEC NEGOTIATION ERROR");
							nua_respond(nh, SIP_488_NOT_ACCEPTABLE, TAG_END());
							switch_channel_hangup(channel, SWITCH_CAUSE_INCOMPATIBLE_DESTINATION);
						}
					}
					goto done;
				}
			}
		}
		break;
	case nua_callstate_completing:
		nua_ack(nh, TAG_END());
		break;
	case nua_callstate_received:
		if (tech_pvt && !switch_test_flag(tech_pvt, TFLAG_SDP)) {
			if (r_sdp && !switch_test_flag(tech_pvt, TFLAG_SDP)) {
				if (switch_test_flag(tech_pvt, TFLAG_NOMEDIA)) {
					switch_channel_set_variable(channel, SWITCH_ENDPOINT_DISPOSITION_VARIABLE, "RECEIVED_NOMEDIA");
					switch_set_flag_locked(tech_pvt, TFLAG_READY);
					switch_channel_set_state(channel, CS_INIT);
					//switch_core_session_thread_launch(session);
					goto done;
				} else {
					sdp_parser_t *parser;
					sdp_session_t *sdp;
					uint8_t match = 0;

					if (tech_pvt->num_codecs) {
						if ((parser = sdp_parse(NULL, r_sdp, (int) strlen(r_sdp), 0))) {
							if ((sdp = sdp_session(parser))) {
								match = sofia_glue_negotiate_sdp(session, sdp);
							}
							sdp_parser_free(parser);
						}
					}
					
					if (match) {
						nua_handle_t *bnh;
						sip_replaces_t *replaces;
						switch_channel_set_variable(channel, SWITCH_ENDPOINT_DISPOSITION_VARIABLE, "RECEIVED");
						switch_set_flag_locked(tech_pvt, TFLAG_READY);
						switch_channel_set_state(channel, CS_INIT);
						//switch_core_session_thread_launch(session);

						if (replaces_str && (replaces = sip_replaces_make(tech_pvt->sofia_private->home, replaces_str))
							&& (bnh = nua_handle_by_replaces(nua, replaces))) {
							sofia_private_t *b_private;

							switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Processing Replaces Attended Transfer\n");
							while (switch_channel_get_state(channel) < CS_EXECUTE) {
								switch_yield(10000);
							}

							if ((b_private = nua_handle_magic(bnh))) {
								char *br_b = switch_channel_get_variable(channel, SWITCH_SIGNAL_BOND_VARIABLE);
								char *br_a = b_private->uuid;

								if (br_b) {
									switch_ivr_uuid_bridge(br_a, br_b);
									switch_channel_set_variable(channel, SWITCH_ENDPOINT_DISPOSITION_VARIABLE, "ATTENDED_TRANSFER");
									switch_channel_hangup(channel, SWITCH_CAUSE_ATTENDED_TRANSFER);
								} else {
									switch_channel_set_variable(channel, SWITCH_ENDPOINT_DISPOSITION_VARIABLE, "ATTENDED_TRANSFER_ERROR");
									switch_channel_hangup(channel, SWITCH_CAUSE_DESTINATION_OUT_OF_ORDER);
								}
							} else {
								switch_channel_set_variable(channel, SWITCH_ENDPOINT_DISPOSITION_VARIABLE, "ATTENDED_TRANSFER_ERROR");
								switch_channel_hangup(channel, SWITCH_CAUSE_DESTINATION_OUT_OF_ORDER);
							}
							nua_handle_unref(bnh);
						}
						goto done;
					}

					switch_channel_set_variable(channel, SWITCH_ENDPOINT_DISPOSITION_VARIABLE, "NO CODECS");
					nua_respond(nh, SIP_488_NOT_ACCEPTABLE, TAG_END());
					switch_channel_hangup(channel, SWITCH_CAUSE_INCOMPATIBLE_DESTINATION);
				}
			} else {
				if (switch_test_flag(tech_pvt, TFLAG_NOMEDIA)) {
					goto done;
				} else {
					switch_channel_set_variable(channel, SWITCH_ENDPOINT_DISPOSITION_VARIABLE, "RECEIVED_NOSDP");
					switch_set_flag(tech_pvt, TFLAG_LATE_NEGOTIATION);
					sofia_glue_set_local_sdp(tech_pvt, NULL, 0, NULL, 0);
					
					nua_respond(tech_pvt->nh, SIP_200_OK,
								SIPTAG_CONTACT_STR(tech_pvt->profile->url),
								SOATAG_USER_SDP_STR(tech_pvt->local_sdp_str), SOATAG_AUDIO_AUX("cn telephone-event"), NUTAG_INCLUDE_EXTRA_SDP(1), TAG_END());
					goto done;
				}
			}
		}

		break;
	case nua_callstate_early:
		break;
	case nua_callstate_completed:
		if (tech_pvt && r_sdp) {
			if (r_sdp) { // && !switch_test_flag(tech_pvt, TFLAG_SDP)) {
				if (switch_test_flag(tech_pvt, TFLAG_NOMEDIA)) {
					goto done;
				} else {
					sdp_parser_t *parser; 
					sdp_session_t *sdp;
					uint8_t match = 0;

					if (tech_pvt->num_codecs) {
						if ((parser = sdp_parse(NULL, r_sdp, (int) strlen(r_sdp), 0))) {
							if ((sdp = sdp_session(parser))) {
								match = sofia_glue_negotiate_sdp(session, sdp);
							}
							sdp_parser_free(parser);
						}
					}
					if (match) {
						if (sofia_glue_tech_choose_port(tech_pvt) != SWITCH_STATUS_SUCCESS) {
							goto done;
						}
						sofia_glue_set_local_sdp(tech_pvt, NULL, 0, NULL, 0);
						switch_set_flag_locked(tech_pvt, TFLAG_REINVITE);
						if (sofia_glue_activate_rtp(tech_pvt) != SWITCH_STATUS_SUCCESS) {
							switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Reinvite RTP Error!\n");
							switch_channel_hangup(channel, SWITCH_CAUSE_DESTINATION_OUT_OF_ORDER);
						}
						switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Processing Reinvite\n");
					} else {
						switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Reinvite Codec Error!\n");
						switch_channel_hangup(channel, SWITCH_CAUSE_DESTINATION_OUT_OF_ORDER);
					}
				}
			}
		}
		break;
	case nua_callstate_ready:
		if (tech_pvt && nh == tech_pvt->nh2) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Cheater Reinvite!\n");
			switch_set_flag_locked(tech_pvt, TFLAG_REINVITE);
			tech_pvt->nh = tech_pvt->nh2;
			tech_pvt->nh2 = NULL;
			if (sofia_glue_tech_choose_port(tech_pvt) == SWITCH_STATUS_SUCCESS) {
				if (sofia_glue_activate_rtp(tech_pvt) != SWITCH_STATUS_SUCCESS) {
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Cheater Reinvite RTP Error!\n");
					switch_channel_hangup(channel, SWITCH_CAUSE_DESTINATION_OUT_OF_ORDER);
				}
			}
			goto done;
		}

		if (channel) {
			if (switch_test_flag(tech_pvt, TFLAG_EARLY_MEDIA)) {
				switch_set_flag_locked(tech_pvt, TFLAG_ANS);
				switch_channel_mark_answered(channel);
				if ((uuid = switch_channel_get_variable(channel, SWITCH_SIGNAL_BOND_VARIABLE))
					&& (other_session = switch_core_session_locate(uuid))) {
					other_channel = switch_core_session_get_channel(other_session);
					switch_channel_answer(other_channel);
					switch_core_session_rwunlock(other_session);
				}
				goto done;
			}

			if (!r_sdp && !switch_test_flag(tech_pvt, TFLAG_SDP)) {
				r_sdp = (const char *) switch_channel_get_variable(channel, SWITCH_R_SDP_VARIABLE);
			}
			if (r_sdp && !switch_test_flag(tech_pvt, TFLAG_SDP)) {
				if (switch_test_flag(tech_pvt, TFLAG_NOMEDIA)) {
					switch_set_flag_locked(tech_pvt, TFLAG_ANS);
					switch_channel_mark_answered(channel);
					if ((uuid = switch_channel_get_variable(channel, SWITCH_SIGNAL_BOND_VARIABLE))
						&& (other_session = switch_core_session_locate(uuid))) {
						other_channel = switch_core_session_get_channel(other_session);
						if (!switch_channel_get_variable(other_channel, SWITCH_B_SDP_VARIABLE)) {
							switch_channel_set_variable(other_channel, SWITCH_B_SDP_VARIABLE, r_sdp);
						}
						switch_channel_answer(other_channel);
						switch_core_session_rwunlock(other_session);
					}
					goto done;
				} else {
					sdp_parser_t *parser;
					sdp_session_t *sdp;
					uint8_t match = 0;

					if (tech_pvt->num_codecs) {
						if ((parser = sdp_parse(NULL, r_sdp, (int) strlen(r_sdp), 0))) {
							if ((sdp = sdp_session(parser))) {
								match = sofia_glue_negotiate_sdp(session, sdp);
							}
							sdp_parser_free(parser);
						}
					}

					if (match) {
						switch_set_flag_locked(tech_pvt, TFLAG_ANS);
						if (sofia_glue_tech_choose_port(tech_pvt) == SWITCH_STATUS_SUCCESS) {
							if (sofia_glue_activate_rtp(tech_pvt) == SWITCH_STATUS_SUCCESS) {
								switch_channel_mark_answered(channel);
							} else {
								switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "RTP Error!\n");
								switch_channel_hangup(channel, SWITCH_CAUSE_DESTINATION_OUT_OF_ORDER);
							}
							goto done;
						}
					}

					switch_channel_set_variable(channel, SWITCH_ENDPOINT_DISPOSITION_VARIABLE, "NO CODECS");
					nua_respond(nh, SIP_488_NOT_ACCEPTABLE, TAG_END());
					switch_channel_hangup(channel, SWITCH_CAUSE_INCOMPATIBLE_DESTINATION);
				}
			}

		}

		break;
	case nua_callstate_terminating:
		break;
	case nua_callstate_terminated:
		if (session) {
			if (!switch_test_flag(tech_pvt, TFLAG_BYE)) {
				switch_set_flag_locked(tech_pvt, TFLAG_BYE);
				if (switch_test_flag(tech_pvt, TFLAG_NOHUP)) {
					switch_clear_flag_locked(tech_pvt, TFLAG_NOHUP);
				} else {
					snprintf(st, sizeof(st), "%d", status);
					switch_channel_set_variable(channel, "sip_term_status", st);
					switch_channel_hangup(channel, sofia_glue_sip_cause_to_freeswitch(status));
				}
			}
			
			if (tech_pvt->sofia_private) {
				if (tech_pvt->sofia_private->home) {
					su_home_unref(tech_pvt->sofia_private->home);
				}
				free(tech_pvt->sofia_private);
				tech_pvt->sofia_private = NULL;
			}

			tech_pvt->nh = NULL;
			
			switch_mutex_lock(profile->flag_mutex);
			profile->inuse--;
			switch_mutex_unlock(profile->flag_mutex);
			
		} else if (sofia_private) {
			if (sofia_private->home) {
				su_home_unref(sofia_private->home);
			}
			free(sofia_private);
		}

		if (nh) {
			nua_handle_bind(nh, NULL);
			nua_handle_destroy(nh);
		}
		break;
	}

  done:

	return;
}


/*---------------------------------------*/
void sofia_handle_sip_i_refer(nua_t *nua, sofia_profile_t *profile, nua_handle_t *nh, switch_core_session_t *session, sip_t const *sip, tagi_t tags[])
{
	/* Incoming refer */
	sip_from_t const *from;
	sip_to_t const *to;
	sip_refer_to_t const *refer_to;
	private_object_t *tech_pvt = NULL;
	char *etmp = NULL, *exten = NULL;
	switch_channel_t *channel_a = NULL, *channel_b = NULL;

	tech_pvt = switch_core_session_get_private(session);
	channel_a = switch_core_session_get_channel(session);

	if (!sip->sip_cseq || !(etmp = switch_mprintf("refer;id=%u", sip->sip_cseq->cs_seq))) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Memory Error!\n");
		goto done;
	}


	if (switch_channel_test_flag(channel_a, CF_NOMEDIA)) {
		nua_notify(tech_pvt->nh, SIPTAG_CONTENT_TYPE_STR("message/sipfrag"),
				   NUTAG_SUBSTATE(nua_substate_terminated), SIPTAG_PAYLOAD_STR("SIP/2.0 403 Forbidden"), SIPTAG_EVENT_STR(etmp), TAG_END());
		goto done;
	}

	from = sip->sip_from;
	to = sip->sip_to;

	if ((refer_to = sip->sip_refer_to)) {
		if (profile->pflags & PFLAG_FULL_ID) {
			exten = switch_mprintf("%s@%s", (char *) refer_to->r_url->url_user, (char *) refer_to->r_url->url_host);
		} else {
			exten = (char *) refer_to->r_url->url_user;
		}

		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Process REFER to [%s@%s]\n", exten, (char *) refer_to->r_url->url_host);

		if (refer_to->r_url->url_headers) {
			sip_replaces_t *replaces;
			nua_handle_t *bnh;
			char *rep;

			if ((rep = strchr(refer_to->r_url->url_headers, '='))) {
				char *br_a = NULL, *br_b = NULL;
				char *buf;
				rep++;



				if ((buf = switch_core_session_alloc(session, strlen(rep) + 1))) {
					rep = url_unescape(buf, (const char *) rep);
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Replaces: [%s]\n", rep);
				} else {
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Memory Error!\n");
					goto done;
				}
				if ((replaces = sip_replaces_make(tech_pvt->sofia_private->home, rep))
					&& (bnh = nua_handle_by_replaces(nua, replaces))) {
					sofia_private_t *b_private = NULL;
					private_object_t *b_tech_pvt = NULL;
					switch_core_session_t *b_session = NULL;

					switch_channel_set_variable(channel_a, SOFIA_REPLACES_HEADER, rep);
					if ((b_private = nua_handle_magic(bnh))) {
						if (!(b_session = switch_core_session_locate(b_private->uuid))) {
							goto done;
						}
						b_tech_pvt = (private_object_t *) switch_core_session_get_private(b_session);
						channel_b = switch_core_session_get_channel(b_session);

						br_a = switch_channel_get_variable(channel_a, SWITCH_SIGNAL_BOND_VARIABLE);
						br_b = switch_channel_get_variable(channel_b, SWITCH_SIGNAL_BOND_VARIABLE);

						switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "Attended Transfer [%s][%s]\n", br_a, br_b);

						if (br_a && br_b) {
							switch_ivr_uuid_bridge(br_a, br_b);
							switch_channel_set_variable(channel_b, SWITCH_ENDPOINT_DISPOSITION_VARIABLE, "ATTENDED_TRANSFER");
							switch_set_flag_locked(tech_pvt, TFLAG_BYE);
							switch_set_flag_locked(b_tech_pvt, TFLAG_BYE);
							nua_notify(tech_pvt->nh, SIPTAG_CONTENT_TYPE_STR("message/sipfrag"),
									   NUTAG_SUBSTATE(nua_substate_terminated), SIPTAG_PAYLOAD_STR("SIP/2.0 200 OK"), SIPTAG_EVENT_STR(etmp), TAG_END());

						} else {
							if (!br_a && !br_b) {
								switch_set_flag_locked(tech_pvt, TFLAG_NOHUP);
								switch_set_flag_locked(b_tech_pvt, TFLAG_XFER);
								b_tech_pvt->xferto = switch_core_session_strdup(b_session, switch_core_session_get_uuid(session));
							} else if (!br_a && br_b) {
								switch_core_session_t *br_b_session;

								if ((br_b_session = switch_core_session_locate(br_b))) {
									private_object_t *br_b_tech_pvt = switch_core_session_get_private(br_b_session);
									switch_channel_t *br_b_channel = switch_core_session_get_channel(br_b_session);

									switch_set_flag_locked(tech_pvt, TFLAG_NOHUP);

									switch_channel_clear_state_handler(br_b_channel, NULL);
									switch_channel_set_state_flag(br_b_channel, CF_TRANSFER);
									switch_channel_set_state(br_b_channel, CS_TRANSMIT);

									switch_set_flag_locked(tech_pvt, TFLAG_REINVITE);
									tech_pvt->local_sdp_audio_ip = switch_core_session_strdup(session, b_tech_pvt->local_sdp_audio_ip);
									tech_pvt->local_sdp_audio_port = b_tech_pvt->local_sdp_audio_port;

									tech_pvt->remote_sdp_audio_ip = switch_core_session_strdup(session, br_b_tech_pvt->remote_sdp_audio_ip);
									tech_pvt->remote_sdp_audio_port = br_b_tech_pvt->remote_sdp_audio_port;
									if (sofia_glue_activate_rtp(tech_pvt) == SWITCH_STATUS_SUCCESS) {
										br_b_tech_pvt->kick = switch_core_session_strdup(br_b_session, switch_core_session_get_uuid(session));
									} else {
										switch_channel_hangup(channel_a, SWITCH_CAUSE_DESTINATION_OUT_OF_ORDER);
									}

									switch_core_session_rwunlock(br_b_session);
								}

								switch_channel_hangup(channel_b, SWITCH_CAUSE_ATTENDED_TRANSFER);
							}
							switch_set_flag_locked(tech_pvt, TFLAG_BYE);
							nua_notify(tech_pvt->nh, SIPTAG_CONTENT_TYPE_STR("message/sipfrag"),
									   NUTAG_SUBSTATE(nua_substate_terminated), SIPTAG_PAYLOAD_STR("SIP/2.0 200 OK"), SIPTAG_EVENT_STR(etmp), TAG_END());

						}
						if (b_session) {
							switch_core_session_rwunlock(b_session);
						}
					}
					nua_handle_unref(bnh);
				} else {		/* the other channel is on a different box, we have to go find them */
					if (exten && (br_a = switch_channel_get_variable(channel_a, SWITCH_SIGNAL_BOND_VARIABLE))) {
						switch_core_session_t *a_session;
						switch_channel_t *channel = switch_core_session_get_channel(session);

						if ((a_session = switch_core_session_locate(br_a))) {
							switch_core_session_t *tsession;
							switch_call_cause_t cause = SWITCH_CAUSE_NORMAL_CLEARING;
							uint32_t timeout = 60;
							char *tuuid_str;

							channel = switch_core_session_get_channel(a_session);

							exten = switch_mprintf("sofia/%s/%s@%s:%s",
												   profile->name,
												   (char *) refer_to->r_url->url_user, (char *) refer_to->r_url->url_host, refer_to->r_url->url_port);

							switch_channel_set_variable(channel, SOFIA_REPLACES_HEADER, rep);

							if (switch_ivr_originate(a_session,
													 &tsession, &cause, exten, timeout, &noop_state_handler, NULL, NULL, NULL) != SWITCH_STATUS_SUCCESS) {
								switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Cannot Create Outgoing Channel! [%s]\n", exten);
								nua_notify(tech_pvt->nh, SIPTAG_CONTENT_TYPE_STR("message/sipfrag"),
										   NUTAG_SUBSTATE(nua_substate_terminated),
										   SIPTAG_PAYLOAD_STR("SIP/2.0 403 Forbidden"), SIPTAG_EVENT_STR(etmp), TAG_END());
								goto done;
							}

							switch_core_session_rwunlock(a_session);
							tuuid_str = switch_core_session_get_uuid(tsession);
							switch_ivr_uuid_bridge(br_a, tuuid_str);
							switch_channel_set_variable(channel_a, SWITCH_ENDPOINT_DISPOSITION_VARIABLE, "ATTENDED_TRANSFER");
							switch_set_flag_locked(tech_pvt, TFLAG_BYE);
							nua_notify(tech_pvt->nh, SIPTAG_CONTENT_TYPE_STR("message/sipfrag"),
									   NUTAG_SUBSTATE(nua_substate_terminated), SIPTAG_PAYLOAD_STR("SIP/2.0 200 OK"), SIPTAG_EVENT_STR(etmp), TAG_END());
							switch_core_session_rwunlock(tsession);
						} else {
							goto error;
						}

					} else {
					  error:
						switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Invalid Transfer! [%s]\n", br_a);
						switch_channel_set_variable(channel_a, SWITCH_ENDPOINT_DISPOSITION_VARIABLE, "ATTENDED_TRANSFER_ERROR");
						nua_notify(tech_pvt->nh, SIPTAG_CONTENT_TYPE_STR("message/sipfrag"),
								   NUTAG_SUBSTATE(nua_substate_terminated), SIPTAG_PAYLOAD_STR("SIP/2.0 403 Forbidden"), SIPTAG_EVENT_STR(etmp),
								   TAG_END());
					}
				}
			} else {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Cannot parse Replaces!\n");
			}
			goto done;
		}

	} else {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Missing Refer-To\n");
		goto done;
	}

	if (exten) {
		switch_channel_t *channel = switch_core_session_get_channel(session);
		char *br;

		if ((br = switch_channel_get_variable(channel, SWITCH_SIGNAL_BOND_VARIABLE))) {
			switch_core_session_t *b_session;

			if ((b_session = switch_core_session_locate(br))) {
				switch_channel_set_variable(channel, "TRANSFER_FALLBACK", from->a_user);
				switch_ivr_session_transfer(b_session, exten, profile->dialplan, profile->context);
				switch_core_session_rwunlock(b_session);
			}

			switch_channel_set_variable(channel, SWITCH_ENDPOINT_DISPOSITION_VARIABLE, "BLIND_TRANSFER");

			/*
			   nua_notify(tech_pvt->nh, SIPTAG_CONTENT_TYPE_STR("message/sipfrag"),
			   NUTAG_SUBSTATE(nua_substate_terminated),
			   SIPTAG_PAYLOAD_STR("SIP/2.0 200 OK"),
			   SIPTAG_EVENT_STR(etmp),
			   TAG_END());
			 */
		} else {
			exten = switch_mprintf("sip:%s@%s:%s", (char *) refer_to->r_url->url_user, (char *) refer_to->r_url->url_host, refer_to->r_url->url_port);
			tech_pvt->dest = switch_core_session_strdup(session, exten);


			switch_set_flag_locked(tech_pvt, TFLAG_NOHUP);

			/*
			   nua_notify(tech_pvt->nh, SIPTAG_CONTENT_TYPE_STR("message/sipfrag"),
			   NUTAG_SUBSTATE(nua_substate_terminated),
			   SIPTAG_PAYLOAD_STR("SIP/2.0 200 OK"),
			   SIPTAG_EVENT_STR(etmp),
			   TAG_END());
			 */
			sofia_glue_do_xfer_invite(session);

		}
	}

  done:
	if (exten && strchr(exten, '@')) {
		switch_safe_free(exten);
	}
	if (etmp) {
		switch_safe_free(etmp);
	}


}


void sofia_handle_sip_i_info(nua_t *nua, sofia_profile_t *profile, nua_handle_t *nh, switch_core_session_t *session, sip_t const *sip, tagi_t tags[])
{

	//placeholder for string searching
	char *signal_ptr;

	//Try and find signal information in the payload
	signal_ptr = strstr(sip->sip_payload->pl_data, "Signal=");

	//See if we found a match
	if (signal_ptr) {
		struct private_object *tech_pvt = NULL;
		switch_channel_t *channel = NULL;
		char dtmf_digit[2] = { 0, 0 };

		//Get the channel
		channel = switch_core_session_get_channel(session);

		//Barf if we didn't get it
		assert(channel != NULL);

		//make sure we have our privates
		tech_pvt = switch_core_session_get_private(session);

		//Barf if we didn't get it
		assert(tech_pvt != NULL);

		//move signal_ptr where we need it (right past Signal=)
		signal_ptr = signal_ptr + 7;

		//put the digit somewhere we can muck with
		strncpy(dtmf_digit, signal_ptr, 1);

		//queue it up
		switch_channel_queue_dtmf(channel, dtmf_digit);

		//print debug info
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "INFO DTMF(%s)\n", dtmf_digit);

	} else {					//unknown info type
		sip_from_t const *from;

		from = sip->sip_from;

		//print in the logs if something comes through we don't understand
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Unknown INFO Recieved: %s%s" URL_PRINT_FORMAT "[%s]\n",
						  from->a_display ? from->a_display : "", from->a_display ? " " : "", URL_PRINT_ARGS(from->a_url), sip->sip_payload->pl_data);
	}

	return;
}

#define url_set_chanvars(session, url, varprefix) _url_set_chanvars(session, url, #varprefix "_user", #varprefix "_host", #varprefix "_port", #varprefix "_uri")
const char *_url_set_chanvars(switch_core_session_t *session, url_t *url, const char *user_var,
									 const char *host_var, const char *port_var, const char *uri_var)
{
	const char *user = NULL, *host = NULL, *port = NULL;
	char *uri = NULL;
	switch_channel_t *channel = switch_core_session_get_channel(session);

	if (url) {
		user = url->url_user;
		host = url->url_host;
		port = url->url_port;
	}

	if (user) {
		switch_channel_set_variable(channel, user_var, user);
	}

	if (!port) {
		port = SOFIA_DEFAULT_PORT;
	}

	switch_channel_set_variable(channel, port_var, port);
	if (host) {
		if (user) {
			uri = switch_core_session_sprintf(session, "%s@%s:%s", user, host, port);
		} else {
			uri = switch_core_session_sprintf(session, "%s:%s", host, port);
		}
		switch_channel_set_variable_nodup(channel, uri_var, uri);
		switch_channel_set_variable(channel, host_var, host);
	}

	return uri;
}

void process_rpid(sip_unknown_t *un, private_object_t *tech_pvt)
{
	int argc, x, screen = 1;
	char *mydata, *argv[10] = { 0 };
	if (!switch_strlen_zero(un->un_value)) {
		if ((mydata = strdup(un->un_value))) {
			argc = switch_separate_string(mydata, ';', argv, (sizeof(argv) / sizeof(argv[0])));

			// Do We really need this at this time 
			// clid_uri = argv[0];

			for (x = 1; x < argc && argv[x]; x++) {
				// we dont need to do anything with party yet we should only be seeing party=calling here anyway
				// maybe thats a dangerous assumption bit oh well yell at me later
				// if (!strncasecmp(argv[x], "party", 5)) {
				//  party = argv[x];
				// } else 
				if (!strncasecmp(argv[x], "privacy=", 8)) {
					char *arg = argv[x] + 9;

					if (!strcasecmp(arg, "yes")) {
						switch_set_flag(tech_pvt->caller_profile, SWITCH_CPF_HIDE_NAME | SWITCH_CPF_HIDE_NUMBER);
					} else if (!strcasecmp(arg, "full")) {
						switch_set_flag(tech_pvt->caller_profile, SWITCH_CPF_HIDE_NAME | SWITCH_CPF_HIDE_NUMBER);
					} else if (!strcasecmp(arg, "name")) {
						switch_set_flag(tech_pvt->caller_profile, SWITCH_CPF_HIDE_NAME);
					} else if (!strcasecmp(arg, "number")) {
						switch_set_flag(tech_pvt->caller_profile, SWITCH_CPF_HIDE_NUMBER);
					} else {
						switch_clear_flag(tech_pvt->caller_profile, SWITCH_CPF_HIDE_NAME);
						switch_clear_flag(tech_pvt->caller_profile, SWITCH_CPF_HIDE_NUMBER);
					}

				} else if (!strncasecmp(argv[x], "screen=", 7) && screen > 0) {
					char *arg = argv[x] + 8;
					if (!strcasecmp(arg, "no")) {
						screen = 0;
						switch_clear_flag(tech_pvt->caller_profile, SWITCH_CPF_SCREEN);
					}
				}
			}
			free(mydata);
		}
	}
}

void sofia_handle_sip_i_invite(nua_t *nua, sofia_profile_t *profile, nua_handle_t *nh, sofia_private_t *sofia_private, sip_t const *sip, tagi_t tags[])
{
	switch_core_session_t *session = NULL;
	char key[128] = "";
	sip_unknown_t *un;
	private_object_t *tech_pvt = NULL;
	switch_channel_t *channel = NULL;
	const char *channel_name = NULL;
	const char *displayname = NULL;
	const char *destination_number = NULL;
	const char *from_user = NULL, *from_host = NULL;
	const char *context = NULL;
	char network_ip[80];

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

	if ((profile->pflags & PFLAG_AUTH_CALLS)) {
		if (sofia_reg_handle_register(nua, profile, nh, sip, REG_INVITE, key, sizeof(key))) {
			return;
		}
	}

	if (!(session = switch_core_session_request(&sofia_endpoint_interface, NULL))) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Session Alloc Failed!\n");
		nua_respond(nh, SIP_503_SERVICE_UNAVAILABLE, TAG_END());
		return;
	}

	if (!(tech_pvt = (private_object_t *) switch_core_session_alloc(session, sizeof(private_object_t)))) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Hey where is my memory pool?\n");
		nua_respond(nh, SIP_503_SERVICE_UNAVAILABLE, TAG_END());
		switch_core_session_destroy(&session);
		return;
	}
	switch_mutex_init(&tech_pvt->flag_mutex, SWITCH_MUTEX_NESTED, switch_core_session_get_pool(session));

	if (!switch_strlen_zero(key)) {
		tech_pvt->key = switch_core_session_strdup(session, key);
	}

	get_addr(network_ip, sizeof(network_ip), &((struct sockaddr_in *) msg_addrinfo(nua_current_request(nua))->ai_addr)->sin_addr);

	channel = switch_core_session_get_channel(session);

	if (sip->sip_from && sip->sip_from->a_url) {
		from_user = sip->sip_from->a_url->url_user;
		from_host = sip->sip_from->a_url->url_host;
		channel_name = url_set_chanvars(session, sip->sip_from->a_url, sip_from);

		if (!switch_strlen_zero(from_user)) {
			if (*from_user == '+') {
				switch_channel_set_variable(channel, "sip_from_user_stripped", (const char *) (from_user + 1));
			} else {
				switch_channel_set_variable(channel, "sip_from_user_stripped", from_user);
			}
		}

		if (!switch_strlen_zero(sip->sip_from->a_display)) {
			char *tmp;
			tmp = switch_core_session_strdup(session, sip->sip_from->a_display);
			if (*tmp == '"') {
				char *p;

				tmp++;
				if ((p = strchr(tmp, '"'))) {
					*p = '\0';
				}
			}
			displayname = tmp;
		} else {
			displayname = switch_strlen_zero(from_user) ? "unkonwn" : from_user;
		}
	}

	if (sip->sip_request->rq_url) {
		const char *req_uri = url_set_chanvars(session, sip->sip_request->rq_url, sip_req);
		if (profile->pflags & PFLAG_FULL_ID) {
			destination_number = req_uri;
		} else {
			destination_number = sip->sip_request->rq_url->url_user;
		}
	}

	if (sip->sip_to && sip->sip_to->a_url) {
		url_set_chanvars(session, sip->sip_to->a_url, sip_to);
	}

	if (sip->sip_contact && sip->sip_contact->m_url) {
		const char *contact_uri = url_set_chanvars(session, sip->sip_contact->m_url, sip_contact);
		if (!channel_name) {
			channel_name = contact_uri;
		}
	}

	sofia_glue_attach_private(session, profile, tech_pvt, channel_name);
	sofia_glue_tech_prepare_codecs(tech_pvt);

	switch_channel_set_variable(channel, SWITCH_ENDPOINT_DISPOSITION_VARIABLE, "INBOUND CALL");
	

	if (switch_test_flag(tech_pvt, TFLAG_INB_NOMEDIA)) {
		switch_set_flag_locked(tech_pvt, TFLAG_NOMEDIA);
		switch_channel_set_flag(channel, CF_NOMEDIA);
	}

	if (!tech_pvt->call_id && sip->sip_call_id && sip->sip_call_id->i_id) {
		tech_pvt->call_id = switch_core_session_strdup(session, sip->sip_call_id->i_id);
		switch_channel_set_variable(channel, "sip_call_id", tech_pvt->call_id);
	}

	if (sip->sip_via) {
		if (sip->sip_via->v_host) {
			switch_channel_set_variable(channel, "sip_via_host", sip->sip_via->v_host);
		}
		if (sip->sip_via->v_port) {
			switch_channel_set_variable(channel, "sip_via_port", sip->sip_via->v_port);
		}
		if (sip->sip_via->v_rport) {
			switch_channel_set_variable(channel, "sip_via_rport", sip->sip_via->v_rport);
		}
	}

	if (sip->sip_max_forwards) {
		char max_forwards[32];
		snprintf(max_forwards, sizeof(max_forwards), "%lu", sip->sip_max_forwards->mf_count);
		switch_channel_set_variable(channel, SWITCH_MAX_FORWARDS_VARIABLE, max_forwards);
	}


	if (sip->sip_request->rq_url) {
		sofia_gateway_t *gateway;
		char *from_key = switch_core_session_sprintf(session, "sip:%s@%s",
													 (char *) sip->sip_request->rq_url->url_user,
													 (char *) sip->sip_request->rq_url->url_host);

		if ((gateway = sofia_reg_find_gateway(from_key))) {
			context = gateway->register_context;
			switch_channel_set_variable(channel, "sip_gateway", gateway->name);
			sofia_reg_release_gateway(gateway);
		}
	}


	if (!context) {
		if (profile->context && !strcasecmp(profile->context, "_domain_")) {
			context = from_host;
		} else {
			context = profile->context;
		}
	}

	tech_pvt->caller_profile = switch_caller_profile_new(switch_core_session_get_pool(session),
														 from_user,
														 profile->dialplan,
														 displayname, from_user, network_ip, NULL, NULL, NULL, modname, context, destination_number);

	if (tech_pvt->caller_profile) {

		/* Loop thru unknown Headers Here so we can do something with them */
		for (un = sip->sip_unknown; un; un = un->un_next) {
			if (!strncasecmp(un->un_name, "Alert-Info", 10)) {
				if (!switch_strlen_zero(un->un_value)) {
					switch_channel_set_variable(channel, "alert_info", un->un_value);
				}
			} else if (!strncasecmp(un->un_name, "Remote-Party-ID", 15)) {
				process_rpid(un, tech_pvt);
			} else if (!strncasecmp(un->un_name, "X-", 2)) {
				if (!switch_strlen_zero(un->un_value)) {
					char *new_name;
					if ((new_name = switch_mprintf("%s%s", SOFIA_SIP_HEADER_PREFIX, un->un_name))) {
						switch_channel_set_variable(channel, new_name, un->un_value);
						free(new_name);
					}
				}
			}
		}

		switch_channel_set_caller_profile(channel, tech_pvt->caller_profile);
	}

	if (!(tech_pvt->sofia_private = malloc(sizeof(*tech_pvt->sofia_private)))) {
		abort();
	}
	memset(tech_pvt->sofia_private, 0, sizeof(*tech_pvt->sofia_private));
	tech_pvt->sofia_private->home = su_home_new(sizeof(*tech_pvt->sofia_private->home));
	sofia_presence_set_chat_hash(tech_pvt, sip);
	switch_copy_string(tech_pvt->sofia_private->uuid, switch_core_session_get_uuid(session), sizeof(tech_pvt->sofia_private->uuid));
	nua_handle_bind(nh, tech_pvt->sofia_private);
	tech_pvt->nh = nh;
	switch_core_session_thread_launch(session);
}

void sofia_handle_sip_i_options(int status,
						  char const *phrase,
						  nua_t *nua, sofia_profile_t *profile, nua_handle_t *nh, sofia_private_t *sofia_private, sip_t const *sip, tagi_t tags[])
{
	nua_respond(nh, SIP_200_OK,
				//SOATAG_USER_SDP_STR(tech_pvt->local_sdp_str),
				//SOATAG_AUDIO_AUX("cn telephone-event"),
				//NUTAG_INCLUDE_EXTRA_SDP(1),
				TAG_END());
}





