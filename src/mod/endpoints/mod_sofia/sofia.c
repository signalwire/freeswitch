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
 * Marcel Barbulescu <marcelbarbulescu@gmail.com>
 * Norman Brandinger
 * Raymond Chandler <intralanman@freeswitch.org>
 * Nathan Patrick <npatrick at corp.sonic.net>
 * Joseph Sullivan <jossulli@amazon.com>
 * Emmanuel Schmidbauer <e.schmidbauer@gmail.com>
 * William King <william.king@quentustech.com>
 * David Knell <david.knell@telng.com>
 * David Villasmil <david.villasmil@gmail.com>
 *
 * sofia.c -- SOFIA SIP Endpoint (sofia code)
 *
 */
#include "mod_sofia.h"


extern su_log_t tport_log[];
extern su_log_t iptsec_log[];
extern su_log_t nea_log[];
extern su_log_t nta_log[];
extern su_log_t nth_client_log[];
extern su_log_t nth_server_log[];
extern su_log_t nua_log[];
extern su_log_t soa_log[];
extern su_log_t sresolv_log[];
#ifdef HAVE_SOFIA_STUN
extern su_log_t stun_log[];
#endif
extern su_log_t su_log_default[];

static void config_sofia_profile_urls(sofia_profile_t * profile);
static void parse_gateways(sofia_profile_t *profile, switch_xml_t gateways_tag, const char *gwname);
static void parse_domain_tag(sofia_profile_t *profile, switch_xml_t x_domain_tag, const char *dname, const char *parse, const char *alias);

void sofia_handle_sip_i_reinvite(switch_core_session_t *session,
								 nua_t *nua, sofia_profile_t *profile, nua_handle_t *nh, sofia_private_t *sofia_private, sip_t const *sip,
								sofia_dispatch_event_t *de,
								 tagi_t tags[]);

static void set_variable_sip_param(switch_channel_t *channel, char *header_type, sip_param_t const *params);



static void sofia_handle_sip_i_state(switch_core_session_t *session, int status,
									 char const *phrase,
									 nua_t *nua, sofia_profile_t *profile, nua_handle_t *nh, sofia_private_t *sofia_private, sip_t const *sip,
								sofia_dispatch_event_t *de,
									 tagi_t tags[]);

static void sofia_handle_sip_r_invite(switch_core_session_t *session, int status,
									  char const *phrase,
									  nua_t *nua, sofia_profile_t *profile, nua_handle_t *nh, sofia_private_t *sofia_private, sip_t const *sip,
								sofia_dispatch_event_t *de,
									  tagi_t tags[]);
static void sofia_handle_sip_r_options(switch_core_session_t *session, int status, char const *phrase, nua_t *nua, sofia_profile_t *profile,
									   nua_handle_t *nh, sofia_private_t *sofia_private, sip_t const *sip,
								sofia_dispatch_event_t *de, tagi_t tags[]);

static void sofia_set_accept_language_channel_variable(switch_channel_t *channel, sip_t const *sip)
{
	if (sip->sip_accept_language) {
		sip_accept_language_t *sip_accept_language = NULL;
		int count = 0;

		for(sip_accept_language = sip->sip_accept_language; sip_accept_language; sip_accept_language = sip_accept_language->aa_next) {
			char var_name[64] = "";

			if (zstr(sip_accept_language->aa_value)) {
				continue;

			}
			if (count == 0) {
				switch_channel_set_variable(channel, "sip_accept_language", sip_accept_language->aa_value);
			}

			switch_snprintf(var_name, sizeof(var_name), "sip_accept_language_%d_value", count);
			switch_channel_set_variable(channel, var_name, sip_accept_language->aa_value);

			if (!zstr(sip_accept_language->aa_q)) {
				switch_snprintf(var_name, sizeof(var_name), "sip_accept_language_%d_q", count);
				switch_channel_set_variable(channel, var_name, sip_accept_language->aa_q);
			}

			count++;
		}
		switch_channel_set_variable_printf(channel, "sip_accept_language_count", "%d", count);
	}
}

void sofia_handle_sip_r_notify(switch_core_session_t *session, int status,
							   char const *phrase,
							   nua_t *nua, sofia_profile_t *profile, nua_handle_t *nh, sofia_private_t *sofia_private, sip_t const *sip,
								sofia_dispatch_event_t *de, tagi_t tags[])
{
	private_object_t *tech_pvt = switch_core_session_get_private(session);
	switch_core_session_t *other_session;

	if (tech_pvt->proxy_refer_uuid && (other_session = switch_core_session_locate(tech_pvt->proxy_refer_uuid))) {
		switch_core_session_message_t *msg;

		msg = switch_core_session_alloc(other_session, sizeof(*msg));
		msg->message_id = SWITCH_MESSAGE_INDICATE_RESPOND;
		msg->from = __FILE__;
		msg->numeric_arg = status;
		msg->string_arg = switch_core_session_strdup(other_session, phrase);
		switch_core_session_queue_message(other_session, msg);
		switch_core_session_rwunlock(other_session);
	} else {
		tech_pvt->proxy_refer_uuid = NULL;
	}


	if (status == 481 && sip && !sip->sip_retry_after && sip->sip_call_id && (!sofia_private || !sofia_private->is_call)) {
		char *sql;

		sql = switch_mprintf("delete from sip_subscriptions where call_id='%q'", sip->sip_call_id->i_id);
		switch_assert(sql != NULL);
		sofia_glue_execute_sql(profile, &sql, SWITCH_TRUE);
		nua_handle_destroy(nh);
	}

}

#define url_set_chanvars(session, url, varprefix) _url_set_chanvars(session, url, #varprefix "_user", #varprefix "_host", #varprefix "_port", #varprefix "_uri", #varprefix "_params")

static const char *_url_set_chanvars(switch_core_session_t *session, url_t *url, const char *user_var,
									 const char *host_var, const char *port_var, const char *uri_var, const char *params_var)
{
	const char *user = NULL, *host = NULL, *port = NULL;
	char *uri = NULL;
	switch_channel_t *channel = switch_core_session_get_channel(session);
	char new_port[25] = "";

	if (url) {
		user = url->url_user;
		host = url->url_host;
		port = url->url_port;
		if (!zstr(url->url_params)) {
			switch_channel_set_variable(channel, params_var, url->url_params);
		}
	}

	if (zstr(user)) {
		user = "nobody";
	}

	if (zstr(host)) {
		host = "nowhere";
	}

	check_decode(user, session);

	if (user) {
		switch_channel_set_variable(channel, user_var, user);
	}


	if (port) {
		switch_snprintf(new_port, sizeof(new_port), ":%s", port);
	}

	switch_channel_set_variable(channel, port_var, port);
	if (host) {
		if (user) {
			uri = switch_core_session_sprintf(session, "%s@%s%s", user, host, new_port);
		} else {
			uri = switch_core_session_sprintf(session, "%s%s", host, new_port);
		}
		switch_channel_set_variable(channel, uri_var, uri);
		switch_channel_set_variable(channel, host_var, host);
	}

	return uri;
}

static char *strip_quotes(const char *in)
{
	char *t = (char *) in;
	char *r = (char *) in;

	if (t && *t == '"') {
		t++;

		if (end_of(t) == '"') {
			r = strdup(t);
			end_of(r) = '\0';
		}
	}

	return r;
}

static void extract_header_vars(sofia_profile_t *profile, sip_t const *sip,
								switch_core_session_t *session, nua_handle_t *nh)
{
	switch_channel_t *channel = switch_core_session_get_channel(session);
	char *full;

	if (sip) {
		if (sip->sip_route) {
			const char *v = switch_channel_get_variable(channel, "sip_full_route");

			if (!v) {
				sip_route_t *rp;
				switch_stream_handle_t stream = { 0 };
				int x = 0;

				SWITCH_STANDARD_STREAM(stream);

				for (rp = sip->sip_route; rp; rp = rp->r_next) {
					char *route = sip_header_as_string(nh->nh_home, (void *) rp);
					stream.write_function(&stream, x == 0 ? "%s" : ",%s", route);
					su_free(nh->nh_home, route);
					x++;
				}
				switch_channel_set_variable(channel, "sip_full_route", stream.data);
				free(stream.data);
			}
		}

		if (switch_channel_direction(channel) == SWITCH_CALL_DIRECTION_OUTBOUND) {
			if (sip->sip_contact) {
				char *c = sip_header_as_string(nh->nh_home, (void *) sip->sip_contact);
				switch_channel_set_variable(channel, "sip_recover_contact", c);
				su_free(nh->nh_home, c);
			}
		}

		if (sip->sip_record_route) {
			sip_record_route_t *rrp;
			switch_stream_handle_t forward_stream = { 0 };
			switch_stream_handle_t reverse_stream = { 0 };
			int x = 0;
			char *tmp[128] = { 0 };
			int y = 0;

			SWITCH_STANDARD_STREAM(forward_stream);
			SWITCH_STANDARD_STREAM(reverse_stream);

			for(rrp = sip->sip_record_route; rrp; rrp = rrp->r_next) {
				char *rr = sip_header_as_string(nh->nh_home, (void *) rrp);
				forward_stream.write_function(&forward_stream, x == 0 ? "%s" : ",%s", rr);
				tmp[y++] = rr;
				if (y == 127) break;
				x++;
			}

			y--;

			x = 0;

			while(y >= 0) {
				reverse_stream.write_function(&reverse_stream, x == 0 ? "%s" : ",%s", tmp[y]);
				su_free(nh->nh_home, tmp[y]);
				y--;
				x++;
			}

			if (switch_channel_direction(channel) == SWITCH_CALL_DIRECTION_OUTBOUND || switch_channel_test_flag(channel, CF_RECOVERED)) {
				switch_channel_set_variable(channel, "sip_invite_route_uri", (char *)reverse_stream.data);
				switch_channel_set_variable(channel, "sip_invite_record_route", (char *)forward_stream.data);
			} else {
				switch_channel_set_variable(channel, "sip_invite_route_uri", (char *)forward_stream.data);
				switch_channel_set_variable(channel, "sip_invite_record_route", (char *)reverse_stream.data);
			}

			free(reverse_stream.data);
			free(forward_stream.data);
		}

		if (sip->sip_via) {
			sip_via_t *vp;
			switch_stream_handle_t stream = { 0 };
			int x = 0;

			SWITCH_STANDARD_STREAM(stream);

			for(vp = sip->sip_via; vp; vp = vp->v_next) {
				char *v = sip_header_as_string(nh->nh_home, (void *) vp);

				stream.write_function(&stream, x == 0 ? "%s" : ",%s", v);
				su_free(nh->nh_home, v);

				x++;
			}

			switch_channel_set_variable(channel, "sip_full_via", (char *)stream.data);

			if (switch_channel_direction(channel) == SWITCH_CALL_DIRECTION_OUTBOUND || switch_stristr("TCP", (char *)stream.data)) {
				switch_channel_set_variable(channel, "sip_recover_via", (char *)stream.data);
			}

			free(stream.data);

		}

		if (sip->sip_from) {
			char *p = strip_quotes(sip->sip_from->a_display);

			if (p) {
				switch_channel_set_variable(channel, "sip_from_display", p);
			}
			if (p != sip->sip_from->a_display) free(p);
			if ((full = sip_header_as_string(nh->nh_home, (void *) sip->sip_from))) {
				switch_channel_set_variable(channel, "sip_full_from", full);
				su_free(nh->nh_home, full);
			}
		}

		if (sip->sip_to) {
			char *p = strip_quotes(sip->sip_to->a_display);

			if (p) {
				switch_channel_set_variable(channel, "sip_to_display", p);
			}

			if (p != sip->sip_to->a_display) free(p);

			if ((full = sip_header_as_string(nh->nh_home, (void *) sip->sip_to))) {
				switch_channel_set_variable(channel, "sip_full_to", full);
				su_free(nh->nh_home, full);
			}
		}

	}
}

static void extract_vars(sofia_profile_t *profile, sip_t const *sip,
						 switch_core_session_t *session)
{
	switch_channel_t *channel = switch_core_session_get_channel(session);

	if (sip) {
		if (sip->sip_from)
			url_set_chanvars(session, sip->sip_from->a_url, sip_from);
		if (sip->sip_request)
			url_set_chanvars(session, sip->sip_request->rq_url, sip_req);
		if (sip->sip_to)
			url_set_chanvars(session, sip->sip_to->a_url, sip_to);
		if (sip->sip_contact)
			url_set_chanvars(session, sip->sip_contact->m_url, sip_contact);
		if (sip->sip_referred_by)
			url_set_chanvars(session, sip->sip_referred_by->b_url, sip_referred_by);
		if (sip->sip_to && sip->sip_to->a_tag) {
			switch_channel_set_variable(channel, "sip_to_tag", sip->sip_to->a_tag);
		}
		if (sip->sip_from && sip->sip_from->a_tag) {
			switch_channel_set_variable(channel, "sip_from_tag", sip->sip_from->a_tag);
		}
		if (sip->sip_cseq && sip->sip_cseq->cs_seq) {
			char sip_cseq[40] = "";
			switch_snprintf(sip_cseq, sizeof(sip_cseq), "%d", sip->sip_cseq->cs_seq);
			switch_channel_set_variable(channel, "sip_cseq", sip_cseq);
		}
		if (sip->sip_call_id && sip->sip_call_id->i_id) {
			switch_channel_set_variable(channel, "sip_call_id", sip->sip_call_id->i_id);
		}
	}
}

/**
 * Add a specific SIP INVITE header to the channel variables, prefixed with "sip_i_"
 */
static void sofia_add_invite_header_to_chanvars(switch_channel_t *channel, nua_handle_t *nh, void *sip_header, const char *var)
{
	switch_assert(channel);
	switch_assert(nh);
	switch_assert(var);

	if (sip_header) {
		char *full;
		if ((full = sip_header_as_string(nh->nh_home, sip_header))) {
			switch_channel_set_variable(channel, var, full);
			su_free(nh->nh_home, full);
		}
	}
}

/**
 * Deep search into the SIP message to recreate the original headers, including multiple Diversions, etc.
 * Finally sets the "sip_invite_headers" to a string containing the 'original' SIP headers, except that the order may have changed.
 * Multiple headers will have the original internal order, though.
 *
 * @param sip A sip_t struct containing the parsed message
 * @param session A call session
 * @param nh A NUA handle for string allocation
 */
static void sofia_parse_all_invite_headers(sip_t const *sip, switch_core_session_t *session, nua_handle_t *nh)
{
	switch_channel_t *channel = switch_core_session_get_channel(session);
	sip_unknown_t *un;
	sip_p_asserted_identity_t *passerted;
	sip_p_preferred_identity_t *ppreferred;
	sip_remote_party_id_t *rpid;
	sip_reply_to_t *reply_to;
	sip_alert_info_t *alert_info;

	if (!sip) return;

	/* Add simple (unique) headers first */
	sofia_add_invite_header_to_chanvars(channel, nh, sip->sip_from, "sip_i_from");
	sofia_add_invite_header_to_chanvars(channel, nh, sip->sip_to, "sip_i_to");
	sofia_add_invite_header_to_chanvars(channel, nh, sip->sip_call_id, "sip_i_call_id");
	sofia_add_invite_header_to_chanvars(channel, nh, sip->sip_cseq, "sip_i_cseq");
	sofia_add_invite_header_to_chanvars(channel, nh, sip->sip_route, "sip_i_route");
	sofia_add_invite_header_to_chanvars(channel, nh, sip->sip_max_forwards, "sip_i_max_forwards");
	sofia_add_invite_header_to_chanvars(channel, nh, sip->sip_proxy_require, "sip_i_proxy_require");
	sofia_add_invite_header_to_chanvars(channel, nh, sip->sip_contact, "sip_i_contact");
	sofia_add_invite_header_to_chanvars(channel, nh, sip->sip_user_agent, "sip_i_user_agent");
	sofia_add_invite_header_to_chanvars(channel, nh, sip->sip_subject, "sip_i_subject");
	sofia_add_invite_header_to_chanvars(channel, nh, sip->sip_priority, "sip_i_priority");
	sofia_add_invite_header_to_chanvars(channel, nh, sip->sip_organization, "sip_i_organization");
	sofia_add_invite_header_to_chanvars(channel, nh, sip->sip_in_reply_to, "sip_i_in_reply_to");
	sofia_add_invite_header_to_chanvars(channel, nh, sip->sip_accept_encoding, "sip_i_accept_encoding");
	sofia_add_invite_header_to_chanvars(channel, nh, sip->sip_accept_language, "sip_i_accept_language");
	sofia_add_invite_header_to_chanvars(channel, nh, sip->sip_allow, "sip_i_allow");
	sofia_add_invite_header_to_chanvars(channel, nh, sip->sip_require, "sip_i_require");
	sofia_add_invite_header_to_chanvars(channel, nh, sip->sip_supported, "sip_i_supported");
	sofia_add_invite_header_to_chanvars(channel, nh, sip->sip_date, "sip_i_date");
	sofia_add_invite_header_to_chanvars(channel, nh, sip->sip_timestamp, "sip_i_timestamp");
	sofia_add_invite_header_to_chanvars(channel, nh, sip->sip_expires, "sip_i_expires");
	sofia_add_invite_header_to_chanvars(channel, nh, sip->sip_min_expires, "sip_i_min_expires");
	sofia_add_invite_header_to_chanvars(channel, nh, sip->sip_session_expires, "sip_i_session_expires");
	sofia_add_invite_header_to_chanvars(channel, nh, sip->sip_min_se, "sip_i_min_se");
	sofia_add_invite_header_to_chanvars(channel, nh, sip->sip_privacy, "sip_i_privacy");
	sofia_add_invite_header_to_chanvars(channel, nh, sip->sip_mime_version, "sip_i_mime_version");
	sofia_add_invite_header_to_chanvars(channel, nh, sip->sip_content_type, "sip_i_content_type");
	sofia_add_invite_header_to_chanvars(channel, nh, sip->sip_content_encoding, "sip_i_content_encoding");
	sofia_add_invite_header_to_chanvars(channel, nh, sip->sip_content_language, "sip_i_content_language");
	sofia_add_invite_header_to_chanvars(channel, nh, sip->sip_content_disposition, "sip_i_content_disposition");
	sofia_add_invite_header_to_chanvars(channel, nh, sip->sip_content_length, "sip_i_content_length");

	/* Add all other headers - which might exist more than once */

	if (sip->sip_via) {
		sip_via_t *vp;
		for (vp = sip->sip_via; vp; vp = vp->v_next) {
			char *v = sip_header_as_string(nh->nh_home, (void *) vp);
			switch_channel_add_variable_var_check(channel, "sip_i_via", v, SWITCH_FALSE, SWITCH_STACK_PUSH);
			su_free(nh->nh_home, v);
		}
	}

	if (sip->sip_record_route) {
		sip_record_route_t *rrp;
		for (rrp = sip->sip_record_route; rrp; rrp = rrp->r_next) {
			char *rr = sip_header_as_string(nh->nh_home, (void *) rrp);
			switch_channel_add_variable_var_check(channel, "sip_i_record_route", rr, SWITCH_FALSE, SWITCH_STACK_PUSH);
			su_free(nh->nh_home, rr);
		}
	}

	if (sip->sip_proxy_authorization) {
		sip_proxy_authorization_t *vp;
		for (vp = sip->sip_proxy_authorization; vp; vp = vp->au_next) {
			char *v = sip_header_as_string(nh->nh_home, (void *) vp);
			switch_channel_add_variable_var_check(channel, "sip_i_proxy_authorization", v, SWITCH_FALSE, SWITCH_STACK_PUSH);
			su_free(nh->nh_home, v);
		}
	}

	if (sip->sip_call_info) {
		sip_call_info_t *vp;
		for (vp = sip->sip_call_info; vp; vp = vp->ci_next) {
			char *v = sip_header_as_string(nh->nh_home, (void *) vp);
			switch_channel_add_variable_var_check(channel, "sip_i_call_info", v, SWITCH_FALSE, SWITCH_STACK_PUSH);
			su_free(nh->nh_home, v);
		}
	}

	if (sip->sip_accept) {
		sip_accept_t *vp;
		for (vp = sip->sip_accept; vp; vp = vp->ac_next) {
			char *v = sip_header_as_string(nh->nh_home, (void *) vp);
			switch_channel_add_variable_var_check(channel, "sip_i_accept", v, SWITCH_FALSE, SWITCH_STACK_PUSH);
			su_free(nh->nh_home, v);
		}
	}

	if (sip->sip_authorization) {
		sip_authorization_t *vp;
		for (vp = sip->sip_authorization; vp; vp = vp->au_next) {
			char *v = sip_header_as_string(nh->nh_home, (void *) vp);
			switch_channel_add_variable_var_check(channel, "sip_i_authorization", v, SWITCH_FALSE, SWITCH_STACK_PUSH);
			su_free(nh->nh_home, v);
		}
	}

	if ((alert_info = sip_alert_info(sip))) {
		sip_alert_info_t *vp;
		for (vp = alert_info; vp; vp = vp->ai_next) {
			char *v = sip_header_as_string(nh->nh_home, (void *) vp);
			switch_channel_add_variable_var_check(channel, "sip_i_alert_info", v, SWITCH_FALSE, SWITCH_STACK_PUSH);
			su_free(nh->nh_home, v);
		}
	}

	if ((passerted = sip_p_asserted_identity(sip))) {
		sip_p_asserted_identity_t *vp;
		for (vp = passerted; vp; vp = vp->paid_next) {
			char *v = sip_header_as_string(nh->nh_home, (void *) vp);
			switch_channel_add_variable_var_check(channel, "sip_i_p_asserted_identity", v, SWITCH_FALSE, SWITCH_STACK_PUSH);
			su_free(nh->nh_home, v);
		}
	}

	if ((ppreferred = sip_p_preferred_identity(sip))) {
		sip_p_preferred_identity_t *vp;
		for (vp = ppreferred; vp; vp = vp->ppid_next) {
			char *v = sip_header_as_string(nh->nh_home, (void *) vp);
			switch_channel_add_variable_var_check(channel, "sip_i_p_preferred_identity", v, SWITCH_FALSE, SWITCH_STACK_PUSH);
			su_free(nh->nh_home, v);
		}
	}

	if ((rpid = sip_remote_party_id(sip))) {
		sip_remote_party_id_t *vp;
		for (vp = rpid; vp; vp = vp->rpid_next) {
			char *v = sip_header_as_string(nh->nh_home, (void *) vp);
			switch_channel_add_variable_var_check(channel, "sip_i_remote_party_id", v, SWITCH_FALSE, SWITCH_STACK_PUSH);
			su_free(nh->nh_home, v);
		}
	}

	if ((reply_to = sip_reply_to(sip))) {
		sip_reply_to_t *vp;
		for (vp = reply_to; vp; vp = vp->rplyto_next) {
			char *v = sip_header_as_string(nh->nh_home, (void *) vp);
			switch_channel_add_variable_var_check(channel, "sip_i_reply_to", v, SWITCH_FALSE, SWITCH_STACK_PUSH);
			su_free(nh->nh_home, v);
		}
	}

	/* Loop through the unknown headers */
	for (un = sip->sip_unknown; un; un = un->un_next) {
		if (!zstr(un->un_name) && !zstr(un->un_value)) {
			char *parsed_name;
			if ((parsed_name = switch_mprintf("sip_i_%s", un->un_name))) {
				char *p, *x = parsed_name;
				switch_tolower_max(x);
				while ((p = strchr(x, '-'))) {
					*p = '_';
					x = ++p;
				}
				switch_channel_add_variable_var_check(channel, parsed_name, un->un_value, SWITCH_FALSE, SWITCH_STACK_PUSH);
				free(parsed_name);
			}
		}
	}
}

static switch_status_t sofia_pass_notify(switch_core_session_t *session, const char *uuid, const char *payload)
{
	switch_core_session_t *other_session;

	if ((other_session = switch_core_session_locate(uuid))) {
		switch_core_session_message_t *msg;

		msg = switch_core_session_alloc(other_session, sizeof(*msg));
		MESSAGE_STAMP_FFL(msg);
		msg->message_id = SWITCH_MESSAGE_INDICATE_BLIND_TRANSFER_RESPONSE;
		msg->string_arg = switch_core_session_strdup(other_session, payload);
		msg->from = __FILE__;
		switch_core_session_queue_message(other_session, msg);
		switch_core_session_rwunlock(other_session);
		return SWITCH_STATUS_SUCCESS;
	}

	return SWITCH_STATUS_FALSE;
}


void sofia_handle_sip_i_notify(switch_core_session_t *session, int status,
							   char const *phrase,
							   nua_t *nua, sofia_profile_t *profile, nua_handle_t *nh, sofia_private_t *sofia_private, sip_t const *sip,
								sofia_dispatch_event_t *de, tagi_t tags[])
{
	switch_channel_t *channel = NULL;
	private_object_t *tech_pvt = NULL;
	switch_event_t *s_event = NULL;
	sofia_gateway_subscription_t *gw_sub_ptr;
	int sub_state;
	sofia_gateway_t *gateway = NULL;

	tl_gets(tags, NUTAG_SUBSTATE_REF(sub_state), TAG_END());

	/* make sure we have a proper event */
	if (!sip || !sip->sip_event) {
		goto error;
	}

	/* Automatically return a 200 OK for Event: keep-alive */
	if (!strcasecmp(sip->sip_event->o_type, "keep-alive")) {
		/* XXX MTK - is this right? in this case isn't sofia is already sending a 200 itself also? */
		nua_respond(nh, SIP_200_OK, NUTAG_WITH_THIS_MSG(de->data->e_msg), TAG_END());
		goto end;
	}

	if (session) {
		channel = switch_core_session_get_channel(session);
		switch_assert(channel != NULL);
		tech_pvt = switch_core_session_get_private(session);
		switch_assert(tech_pvt != NULL);
	}


	if (tech_pvt && tech_pvt->proxy_refer_uuid && sofia_test_pflag(profile, PFLAG_PROXY_REFER) && sip->sip_payload && sip->sip_payload->pl_data &&
		sip->sip_content_type && sip->sip_content_type->c_type && switch_stristr("sipfrag", sip->sip_content_type->c_type)) {

		if (sofia_pass_notify(session, tech_pvt->proxy_refer_uuid, sip->sip_payload->pl_data) == SWITCH_STATUS_SUCCESS) {
			if (tech_pvt->proxy_refer_msg) {
				msg_ref_destroy(tech_pvt->proxy_refer_msg);
				tech_pvt->proxy_refer_msg = NULL;
			}
			tech_pvt->proxy_refer_msg = msg_ref_create(de->data->e_msg);
			//nua_respond(nh, SIP_202_ACCEPTED, NUTAG_WITH_THIS_MSG(de->data->e_msg), TAG_END());
		} else {
			switch_channel_hangup(channel, SWITCH_CAUSE_DESTINATION_OUT_OF_ORDER);
		}

		goto end;
	}

	/* For additional NOTIFY event packages see http://www.iana.org/assignments/sip-events. */
	if (sip->sip_content_type &&
		sip->sip_content_type->c_type && sip->sip_payload && sip->sip_payload->pl_data && !strcasecmp(sip->sip_event->o_type, "refer")) {
		if (switch_event_create_subclass(&s_event, SWITCH_EVENT_CUSTOM, MY_EVENT_NOTIFY_REFER) == SWITCH_STATUS_SUCCESS) {
			switch_event_add_header_string(s_event, SWITCH_STACK_BOTTOM, "content-type", sip->sip_content_type->c_type);
			switch_event_add_body(s_event, "%s", sip->sip_payload->pl_data);
		}
	}

	/* add common headers for the NOTIFY to the switch_event and fire if it exists */
	if (s_event != NULL) {
		switch_event_add_header_string(s_event, SWITCH_STACK_BOTTOM, "event-package", sip->sip_event->o_type);
		switch_event_add_header_string(s_event, SWITCH_STACK_BOTTOM, "event-id", sip->sip_event->o_id);

		if (sip->sip_contact) {
			switch_event_add_header(s_event, SWITCH_STACK_BOTTOM, "contact", "%s@%s",
									sip->sip_contact->m_url->url_user, sip->sip_contact->m_url->url_host);
		}

		switch_event_add_header(s_event, SWITCH_STACK_BOTTOM, "from", "%s@%s", sip->sip_from->a_url->url_user, sip->sip_from->a_url->url_host);
		switch_event_add_header_string(s_event, SWITCH_STACK_BOTTOM, "from-tag", sip->sip_from->a_tag);
		switch_event_add_header(s_event, SWITCH_STACK_BOTTOM, "to", "%s@%s", sip->sip_to->a_url->url_user, sip->sip_to->a_url->url_host);
		switch_event_add_header_string(s_event, SWITCH_STACK_BOTTOM, "to-tag", sip->sip_to->a_tag);

		if (sip->sip_call_id && sip->sip_call_id->i_id) {
			switch_event_add_header_string(s_event, SWITCH_STACK_BOTTOM, "call-id", sip->sip_call_id->i_id);
		}
		if (sip->sip_subscription_state && sip->sip_subscription_state->ss_substate) {
			switch_event_add_header_string(s_event, SWITCH_STACK_BOTTOM, "subscription-substate", sip->sip_subscription_state->ss_substate);
		}
		if (sip->sip_subscription_state && sip->sip_subscription_state->ss_reason) {
			switch_event_add_header_string(s_event, SWITCH_STACK_BOTTOM, "subscription-reason", sip->sip_subscription_state->ss_reason);
		}
		if (sip->sip_subscription_state && sip->sip_subscription_state->ss_retry_after) {
			switch_event_add_header_string(s_event, SWITCH_STACK_BOTTOM, "subscription-retry-after", sip->sip_subscription_state->ss_retry_after);
		}
		if (sip->sip_subscription_state && sip->sip_subscription_state->ss_expires) {
			switch_event_add_header_string(s_event, SWITCH_STACK_BOTTOM, "subscription-expires", sip->sip_subscription_state->ss_expires);
		}
		if (session) {
			switch_event_add_header_string(s_event, SWITCH_STACK_BOTTOM, "UniqueID", switch_core_session_get_uuid(session));
		}
		switch_event_fire(&s_event);
	}

	if (!strcasecmp(sip->sip_event->o_type, "refer")) {
		if (session && channel && tech_pvt) {
			if (sip->sip_payload && sip->sip_payload->pl_data) {
				char *p;
				int status_val = 0;
				if ((p = strchr(sip->sip_payload->pl_data, ' '))) {
					p++;
					if (p) {
						status_val = atoi(p);
					}
				}
				if (!status_val || status_val >= 200) {
					switch_channel_set_variable(channel, "sip_refer_reply", sip->sip_payload->pl_data);
					if (status_val == 200) {
						switch_channel_hangup(channel, SWITCH_CAUSE_BLIND_TRANSFER);
					}
					if ((int)tech_pvt->want_event == 9999) {
						tech_pvt->want_event = 0;
					}
				}
			}
		}
		nua_respond(nh, SIP_200_OK, NUTAG_WITH_THIS_MSG(de->data->e_msg), TAG_END());
	}

	/* if no session, assume it could be an incoming notify from a gateway subscription */
	if (session) {
		if (!zstr(profile->proxy_notify_events) && (!strcasecmp(profile->proxy_notify_events, "all") || strstr(profile->proxy_notify_events, sip->sip_event->o_type))) {
			switch_core_session_t *other_session;
			if (switch_core_session_get_partner(session, &other_session) == SWITCH_STATUS_SUCCESS) {
				private_object_t *other_tech_pvt = switch_core_session_get_private(other_session);
				const char *full_to = NULL;
				char *pl = NULL;
				char *unknown = NULL;

				full_to = switch_str_nil(switch_channel_get_variable(switch_core_session_get_channel(other_session), "sip_full_to"));
				if (sip->sip_payload && sip->sip_payload->pl_data) {
					pl = switch_core_session_strdup(other_session, (char*)sip->sip_payload->pl_data);
				}
				unknown = sofia_glue_get_non_extra_unknown_headers(sip);
				nua_notify(other_tech_pvt->nh, NUTAG_NEWSUB(1), NUTAG_SUBSTATE(nua_substate_active),
							TAG_IF((full_to), SIPTAG_TO_STR(full_to)), SIPTAG_SUBSCRIPTION_STATE_STR("active"),
							SIPTAG_EVENT_STR(sip->sip_event->o_type), TAG_IF(!zstr(unknown), SIPTAG_HEADER_STR(unknown)),
							TAG_IF(!zstr(pl), SIPTAG_PAYLOAD_STR(pl)), TAG_END());
				switch_safe_free(unknown);
				switch_core_session_rwunlock(other_session);
			}
			nua_respond(nh, SIP_200_OK, NUTAG_WITH_THIS_MSG(de->data->e_msg), TAG_END());
			goto end;
		}
		/* make sure we have a proper "talk" event */
		if (strcasecmp(sip->sip_event->o_type, "talk")) {
			goto error;
		}

		if (switch_channel_direction(channel) == SWITCH_CALL_DIRECTION_INBOUND) {
			switch_channel_answer(channel);
			switch_channel_set_variable(channel, "auto_answer_destination", switch_channel_get_variable(channel, "destination_number"));
			switch_ivr_session_transfer(session, "auto_answer", NULL, NULL);
			nua_respond(nh, SIP_200_OK, NUTAG_WITH_THIS_MSG(de->data->e_msg), TAG_END());
			goto end;
		}
	}

	if (!sofia_private || zstr(sofia_private->gateway_name)) {
		if (profile->debug) {
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "Gateway information missing Subscription Event: %s\n",
							  sip->sip_event->o_type);
		}
		goto error;
	}


	if (!(gateway = sofia_reg_find_gateway(sofia_private->gateway_name))) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Gateway information missing\n");
		goto error;
	}

	/* find the corresponding gateway subscription (if any) */
	if (!(gw_sub_ptr = sofia_find_gateway_subscription(gateway, sip->sip_event->o_type))) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_WARNING,
						  "Could not find gateway subscription.  Gateway: %s.  Subscription Event: %s\n",
						  gateway->name, sip->sip_event->o_type);
		goto error;
	}

	if (!(gw_sub_ptr->state == SUB_STATE_SUBED || gw_sub_ptr->state == SUB_STATE_SUBSCRIBE)) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_WARNING, "Ignoring notify due to subscription state: %d\n", gw_sub_ptr->state);
		goto error;
	}

	if (sip->sip_subscription_state && sip->sip_subscription_state->ss_expires) {
		int delta = atoi(sip->sip_subscription_state->ss_expires);

		delta /= 2;

		if (delta < 1) {
			delta = 1;
		}
		gw_sub_ptr->expires = switch_epoch_time_now(NULL) + delta;
	}

	/* dispatch freeswitch event */
	if (switch_event_create(&s_event, SWITCH_EVENT_NOTIFY_IN) == SWITCH_STATUS_SUCCESS) {
		switch_event_add_header_string(s_event, SWITCH_STACK_BOTTOM, "event", sip->sip_event->o_type);
		switch_event_add_header_string(s_event, SWITCH_STACK_BOTTOM, "pl_data", sip->sip_payload ? sip->sip_payload->pl_data : "");
		if ( sip->sip_content_type != NULL )
			switch_event_add_header_string(s_event, SWITCH_STACK_BOTTOM, "sip_content_type", sip->sip_content_type->c_type);
		switch_event_add_header(s_event, SWITCH_STACK_BOTTOM, "port", "%d", gateway->profile->sip_port);
		switch_event_add_header_string(s_event, SWITCH_STACK_BOTTOM, "module_name", "mod_sofia");
		switch_event_add_header_string(s_event, SWITCH_STACK_BOTTOM, "profile_name", gateway->profile->name);
		switch_event_add_header_string(s_event, SWITCH_STACK_BOTTOM, "profile_uri", gateway->profile->url);
		switch_event_add_header_string(s_event, SWITCH_STACK_BOTTOM, "gateway_name", gateway->name);
		if ( sip->sip_call_info != NULL ) {
			sip_call_info_t *call_info = sip->sip_call_info;
			char *nua_hold = sip_header_as_string(nua_handle_home(nh), (void *) call_info);
			size_t cur_len = strlen(nua_hold);
			char *hold = strdup(nua_hold);
			su_free(nua_handle_home(nh), nua_hold);

			while ((call_info = call_info->ci_next) != NULL) {
				char *tmp = sip_header_as_string(nua_handle_home(nh), (void *) call_info);
				size_t tmp_len = strlen(tmp);
				hold = realloc(hold, cur_len + tmp_len + 2);
				switch_assert(hold);
				strncpy(hold + cur_len, ",", 2);
				strncpy(hold + cur_len + 1, tmp, tmp_len +1);
				su_free(nua_handle_home(nh), tmp);
				cur_len = cur_len + tmp_len + 2;
			}
			switch_event_add_header_string(s_event, SWITCH_STACK_BOTTOM | SWITCH_STACK_NODUP, "Call-Info", hold);
		}
		switch_event_fire(&s_event);
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "dispatched freeswitch event for message-summary NOTIFY\n");
	} else {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "Failed to create event\n");
		goto error;
	}

	goto end;

  error:


	if (sip && sip->sip_event && sip->sip_event->o_type && !strcasecmp(sip->sip_event->o_type, "message-summary")) {
		/* unsolicited mwi, just say ok */
		nua_respond(nh, SIP_200_OK, NUTAG_WITH_THIS_MSG(de->data->e_msg), TAG_END());

		if (sofia_test_pflag(profile, PFLAG_FORWARD_MWI_NOTIFY)) {
			const char *mwi_status = NULL;
			char network_ip[80];
			uint32_t x = 0;
			int acl_ok = 1;
			char *last_acl = NULL;

			if (sip->sip_to && sip->sip_to->a_url->url_user && sip->sip_to->a_url->url_host
				&& sip->sip_payload && sip->sip_payload->pl_data ) {

				sofia_glue_get_addr(de->data->e_msg, network_ip, sizeof(network_ip), NULL);
				for (x = 0; x < profile->acl_count; x++) {
					last_acl = profile->acl[x];
					if (!(acl_ok = switch_check_network_list_ip(network_ip, last_acl))) {
						break;
					}
				}

				if ( acl_ok )
				{
					mwi_status = switch_stristr("Messages-Waiting: ", sip->sip_payload->pl_data);

					if ( mwi_status ) {
						char *mwi_stat;
						mwi_status += strlen( "Messages-Waiting: " );
						mwi_stat = switch_strip_whitespace( mwi_status );

						switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG,
										  "Forwarding unsolicited MWI ( %s : %s@%s )\n",
										  mwi_stat, sip->sip_to->a_url->url_user, sip->sip_to->a_url->url_host );
						if (switch_event_create(&s_event, SWITCH_EVENT_MESSAGE_WAITING) == SWITCH_STATUS_SUCCESS) {
							switch_event_add_header_string(s_event, SWITCH_STACK_BOTTOM, "MWI-Messages-Waiting", mwi_stat );
							switch_event_add_header(s_event, SWITCH_STACK_BOTTOM,
													"MWI-Message-Account", "%s@%s", sip->sip_to->a_url->url_user, sip->sip_to->a_url->url_host );
							switch_event_fire(&s_event);
						}
						switch_safe_free(mwi_stat);
					}
				} else {
					switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "Dropping unsolicited MWI ( %s@%s ) because of ACL\n",
									  sip->sip_to->a_url->url_user, sip->sip_to->a_url->url_host );
				};

			}
		}

	} else {
		nua_respond(nh, 481, "Subscription Does Not Exist", NUTAG_WITH_THIS_MSG(de->data->e_msg), TAG_END());
	}

  end:

	if (!gateway && sub_state == nua_substate_terminated && sofia_private && sofia_private != &mod_sofia_globals.destroy_private &&
		sofia_private != &mod_sofia_globals.keep_private) {
		sofia_private->destroy_nh = 1;
		sofia_private->destroy_me = 1;
	}

	if (gateway) {
		sofia_reg_release_gateway(gateway);
	}

}

/*
 * This will fire an event containing X-headers from the BYE response
 */
void sofia_handle_sip_r_bye(switch_core_session_t *session, int status,
							char const *phrase,
							nua_t *nua, sofia_profile_t *profile, nua_handle_t *nh, sofia_private_t *sofia_private, sip_t const *sip,
								sofia_dispatch_event_t *de, tagi_t tags[])
{
	if (profile && sofia_test_pflag(profile, PFLAG_FIRE_BYE_RESPONSE_EVENTS) && sip && sip->sip_call_id && !zstr(sip->sip_call_id->i_id) && sofia_private && !zstr_buf(sofia_private->uuid_str)) {
		switch_event_t *bye_response_event = NULL;
		sip_unknown_t *un;
		if (switch_event_create_subclass(&bye_response_event, SWITCH_EVENT_CUSTOM, MY_EVENT_BYE_RESPONSE) == SWITCH_STATUS_SUCCESS) {
			switch_event_add_header(bye_response_event, SWITCH_STACK_BOTTOM, "call-id", "%s", sip->sip_call_id->i_id);
			switch_event_add_header(bye_response_event, SWITCH_STACK_BOTTOM, "Unique-ID", "%s", sofia_private->uuid_str);
			for (un = sip->sip_unknown; un; un = un->un_next) {
				if (!zstr(un->un_value)) {
					switch_event_add_header(bye_response_event, SWITCH_STACK_BOTTOM, un->un_name, "%s", un->un_value);
				}
			}
			switch_event_fire(&bye_response_event);
		}
	}
}

void sofia_handle_sip_i_bye(switch_core_session_t *session, int status,
							char const *phrase,
							nua_t *nua, sofia_profile_t *profile, nua_handle_t *nh, sofia_private_t *sofia_private, sip_t const *sip,
								sofia_dispatch_event_t *de, tagi_t tags[])
{
	const char *tmp;
	switch_channel_t *channel;
	private_object_t *tech_pvt;
	char *extra_headers;
	const char *call_info = NULL;
	const char *vval = NULL;
#ifdef MANUAL_BYE
	int cause;
	char st[80] = "";
#endif

	if (!session) {
		return;
	}

	channel = switch_core_session_get_channel(session);
	tech_pvt = switch_core_session_get_private(session);

#ifdef MANUAL_BYE
	status = 200;
	phrase = "OK";

	if (switch_channel_test_flag(tech_pvt->channel, CF_SLA_BARGING)) {
		const char *bargee_uuid = switch_channel_get_variable(channel, "sip_barging_uuid");
		switch_core_session_t *bargee_session;
		uint32_t ttl = 0;

		if ((bargee_session = switch_core_session_locate(bargee_uuid))) {
			//switch_channel_t *bargee_channel = switch_core_session_get_channel(bargee_session);
			if ((ttl = switch_core_media_bug_count(bargee_session, "eavesdrop")) == 1) {
				if (switch_core_session_check_interface(bargee_session, sofia_endpoint_interface)) {
					switch_channel_clear_flag(switch_core_session_get_channel(bargee_session), CF_SLA_BARGE);
				}
			}
			switch_core_session_rwunlock(bargee_session);
		}
	}

	if (switch_channel_test_flag(tech_pvt->channel, CF_SLA_BARGE)) {
		switch_core_session_t *new_session, *other_session;
		const char *other_uuid = switch_channel_get_partner_uuid(tech_pvt->channel);
		char *cmd = NULL;

		if (!zstr(other_uuid) && (other_session = switch_core_session_locate(other_uuid))) {
			switch_channel_t *other_channel = switch_core_session_get_channel(other_session);

			switch_mutex_lock(profile->ireg_mutex);
			if (switch_ivr_eavesdrop_pop_eavesdropper(session, &new_session) == SWITCH_STATUS_SUCCESS) {
				switch_channel_t *new_channel = switch_core_session_get_channel(new_session);
				const char *new_uuid = switch_core_session_get_uuid(new_session);
				switch_caller_profile_t *cp = switch_channel_get_caller_profile(new_channel);

				cp->caller_id_name = cp->orig_caller_id_name;
				cp->caller_id_number = cp->orig_caller_id_number;


				switch_channel_set_variable(new_channel, SWITCH_SIGNAL_BOND_VARIABLE, NULL);

				switch_channel_set_flag(other_channel, CF_REDIRECT);

				switch_channel_set_state(new_channel, CS_RESET);

				switch_ivr_uuid_bridge(new_uuid, other_uuid);
				cmd = switch_core_session_sprintf(session, "sleep:500,sofia_sla:%s inline", new_uuid);

				switch_channel_clear_flag(other_channel, CF_REDIRECT);

				switch_core_session_rwunlock(new_session);
			}
			switch_mutex_unlock(profile->ireg_mutex);

			switch_core_session_rwunlock(other_session);
		}

		if (!zstr(cmd)) {
			switch_ivr_eavesdrop_exec_all(session, "transfer", cmd);
		}
	}

	sofia_set_flag_locked(tech_pvt, TFLAG_BYE);
	call_info = switch_channel_get_variable(channel, "presence_call_info_full");

	if (sip->sip_reason) {
		char *reason_header = sip_header_as_string(nh->nh_home, (void *) sip->sip_reason);

		if (!zstr(reason_header)) {
			switch_channel_set_variable(channel, "sip_reason", reason_header);
			switch_channel_set_variable_partner(channel, "sip_reason", reason_header);
		}
	}

	if (sip->sip_reason && sip->sip_reason->re_protocol && (!strcasecmp(sip->sip_reason->re_protocol, "Q.850")
															|| !strcasecmp(sip->sip_reason->re_protocol, "FreeSWITCH")
															|| !strcasecmp(sip->sip_reason->re_protocol, profile->sdp_username)) && sip->sip_reason->re_cause) {
		tech_pvt->q850_cause = atoi(sip->sip_reason->re_cause);
		cause = tech_pvt->q850_cause;
	} else {
		cause = sofia_glue_sip_cause_to_freeswitch(status);
	}

	if (sip->sip_content_type && sip->sip_content_type->c_type) {
		switch_channel_set_variable(channel, "sip_bye_content_type", sip->sip_content_type->c_type);
	}

	if (sip->sip_payload && sip->sip_payload->pl_data) {
		switch_channel_set_variable(channel, "sip_bye_payload", sip->sip_payload->pl_data);
	}

	switch_snprintf(st, sizeof(st), "%d", status);
	switch_channel_set_variable(channel, "sip_term_status", st);
	switch_snprintf(st, sizeof(st), "sip:%d", status);
	switch_channel_set_variable(channel, SWITCH_PROTO_SPECIFIC_HANGUP_CAUSE_VARIABLE, st);

	if (phrase) {
		switch_channel_set_variable_partner(channel, "sip_hangup_phrase", phrase);
	}

	switch_snprintf(st, sizeof(st), "%d", cause);
	switch_channel_set_variable(channel, "sip_term_cause", st);

	extra_headers = sofia_glue_get_extra_headers(channel, SOFIA_SIP_BYE_HEADER_PREFIX);
	sofia_glue_set_extra_headers(session, sip, SOFIA_SIP_BYE_HEADER_PREFIX);

	if (!(vval = switch_channel_get_variable(channel, "sip_copy_custom_headers")) || switch_true(vval)) {
		switch_core_session_t *nsession = NULL;

		switch_core_session_get_partner(session, &nsession);

		if (nsession) {
			const char *vval;
			switch_channel_t *nchannel = switch_core_session_get_channel(nsession);

			if (!(vval = switch_channel_get_variable(nchannel, "sip_copy_custom_headers")) || switch_true(vval)) {
				switch_ivr_transfer_variable(session, nsession, SOFIA_SIP_BYE_HEADER_PREFIX_T);
			}
			switch_core_session_rwunlock(nsession);
		}
	}


	switch_channel_hangup(channel, cause);
	nua_respond(nh, SIP_200_OK, NUTAG_WITH_THIS_MSG(de->data->e_msg),
				TAG_IF(call_info, SIPTAG_CALL_INFO_STR(call_info)), TAG_IF(!zstr(extra_headers), SIPTAG_HEADER_STR(extra_headers)), TAG_END());

	switch_safe_free(extra_headers);

	if (sofia_private) {
		sofia_private->destroy_me = 1;
		sofia_private->destroy_nh = 1;
	}
#endif


	if (sip->sip_user_agent && !zstr(sip->sip_user_agent->g_string)) {
		switch_channel_set_variable(channel, "sip_user_agent", sip->sip_user_agent->g_string);
	} else if (sip->sip_server && !zstr(sip->sip_server->g_string)) {
		switch_channel_set_variable(channel, "sip_user_agent", sip->sip_server->g_string);
	}

	sofia_set_accept_language_channel_variable(channel, sip);

	if ((tmp = sofia_glue_get_unknown_header(sip, "rtp-txstat"))) {
		switch_channel_set_variable(channel, "sip_rtp_txstat", tmp);
	}
	if ((tmp = sofia_glue_get_unknown_header(sip, "rtp-rxstat"))) {
		switch_channel_set_variable(channel, "sip_rtp_rxstat", tmp);
	}
	if ((tmp = sofia_glue_get_unknown_header(sip, "P-RTP-Stat"))) {
		switch_channel_set_variable(channel, "sip_p_rtp_stat", tmp);
	}

	tech_pvt->got_bye = 1;
	switch_channel_set_variable(channel, "sip_hangup_disposition", "recv_bye");

	return;
}

void sofia_handle_sip_r_message(int status, sofia_profile_t *profile, nua_handle_t *nh, sip_t const *sip)
{
	const char *call_id;
	int *mstatus;

	if (!(sip && sip->sip_call_id)) {
		nua_handle_destroy(nh);
		return;
	}

	call_id = sip->sip_call_id->i_id;



	switch_mutex_lock(profile->flag_mutex);
	mstatus = switch_core_hash_find(profile->chat_hash, call_id);
	switch_mutex_unlock(profile->flag_mutex);

	if (mstatus) {
		*mstatus = status;
	}

}

void sofia_wait_for_reply(struct private_object *tech_pvt, nua_event_t event, uint32_t timeout)
{
	time_t exp = switch_epoch_time_now(NULL) + timeout;

	tech_pvt->want_event = event;

	while (switch_channel_ready(tech_pvt->channel) && tech_pvt->want_event && switch_epoch_time_now(NULL) < exp) {
		switch_yield(100000);
	}

}

void sofia_send_callee_id(switch_core_session_t *session, const char *name, const char *number)
{
	const char *uuid;
	switch_core_session_t *session_b;
	switch_channel_t *channel = switch_core_session_get_channel(session);
	switch_caller_profile_t *caller_profile = switch_channel_get_caller_profile(channel);


	if (switch_channel_inbound_display(channel)) {
		if (zstr(name)) {
			name = caller_profile->caller_id_name;
		}

		if (zstr(number)) {
			number = caller_profile->caller_id_number;
		}

		if (zstr(name)) {
			name = number;
		}

		if (zstr(number)) {
			name = number = "UNKNOWN";
		}

		if (!zstr(name) && !strcmp(name,"_undef_")) {
			name = "";
		}
	} else {
		if (zstr(name)) {
			name = caller_profile->callee_id_name;
		}

		if (zstr(number)) {
			number = caller_profile->callee_id_number;
		}

		if (zstr(name)) {
			name = number;
		}

		if (zstr(number)) {
			number = caller_profile->destination_number;
		}

		if (!zstr(name) && !strcmp(name,"_undef_")) {
			name = "";
		}
	}

	if ((uuid = switch_channel_get_partner_uuid(channel)) && (session_b = switch_core_session_locate(uuid))) {
		switch_core_session_message_t *msg;
		//switch_channel_t *channel_b = switch_core_session_get_channel(session_b);

		//switch_channel_set_profile_var(channel_b, "callee_id_name", name);
		//switch_channel_set_profile_var(channel_b, "callee_id_number", number);

		msg = switch_core_session_alloc(session_b, sizeof(*msg));
		MESSAGE_STAMP_FFL(msg);
		msg->message_id = SWITCH_MESSAGE_INDICATE_DISPLAY;
		msg->string_array_arg[0] = switch_core_session_strdup(session_b, name);
		msg->string_array_arg[1] = switch_core_session_strdup(session_b, number);
		msg->from = __FILE__;
		switch_core_session_queue_message(session_b, msg);
		switch_core_session_rwunlock(session_b);
	}
}

void sofia_update_callee_id(switch_core_session_t *session, sofia_profile_t *profile, sip_t const *sip,
							switch_bool_t send)
{
	switch_channel_t *channel = switch_core_session_get_channel(session);
	sip_p_asserted_identity_t *passerted = NULL;
	char *name = NULL;
	const char *number = "unknown", *tmp;
	switch_caller_profile_t *caller_profile;
	char *dup = NULL;
	switch_event_t *event;
	const char *val;
	int fs = 0, lazy = 0, att = 0;
	const char *name_var = "callee_id_name";
	const char *num_var = "callee_id_number";

	if (switch_true(switch_channel_get_variable(channel, SWITCH_IGNORE_DISPLAY_UPDATES_VARIABLE)) || !sofia_test_pflag(profile, PFLAG_SEND_DISPLAY_UPDATE)) {
		return;
	}


	if (switch_channel_inbound_display(channel)) {
		name_var = "caller_id_name";
		num_var = "caller_id_number";
	}


	number = (char *) switch_channel_get_variable(channel, num_var);
	name = (char *) switch_channel_get_variable(channel, name_var);


	if (zstr(number) && sip->sip_to) {
		number = sip->sip_to->a_url->url_user;
	}

	if ((val = sofia_glue_get_unknown_header(sip, "X-FS-Display-Number"))) {
		number = val;
		fs++;
	}

	if ((val = sofia_glue_get_unknown_header(sip, "X-FS-Display-Name"))) {
		name = (char *) val;
		check_decode(name, session);
		fs++;
	}

	if ((val = sofia_glue_get_unknown_header(sip, "X-FS-Lazy-Attended-Transfer"))) {
		lazy = switch_true(val);
		fs++;
	}

	if ((val = sofia_glue_get_unknown_header(sip, "X-FS-Attended-Transfer"))) {
		att = switch_true(val);
		fs++;
	}

	if (!fs) {
		sip_remote_party_id_t *rpid;
		if ((passerted = sip_p_asserted_identity(sip))) {
			if (passerted->paid_url->url_user) {
				number = passerted->paid_url->url_user;
			}
			if (!zstr(passerted->paid_display)) {
				dup = strdup(passerted->paid_display);
				if (*dup == '"') {
					name = dup + 1;
				} else {
					name = dup;
				}
				if (end_of(name) == '"') {
					end_of(name) = '\0';
				}
			}
		} else if ((rpid = sip_remote_party_id(sip))) {
			if (rpid->rpid_url->url_user) {
				number = rpid->rpid_url->url_user;
			}
			if (!zstr(rpid->rpid_display)) {
				dup = strdup(rpid->rpid_display);
				if (*dup == '"') {
					name = dup + 1;
				} else {
					name = dup;
				}
				if (end_of(name) == '"') {
					end_of(name) = '\0';
				}
			}
		}
	}


	if (zstr(number)) {
		if ((tmp = switch_channel_get_variable(channel, num_var)) && !zstr(tmp)) {
			number = (char *) tmp;
		}

		if (zstr(number)) {
			number = "unknown";
		}
	}

	if (zstr(name)) {
		if ((tmp = switch_channel_get_variable(channel, name_var)) && !zstr(tmp)) {
			name = (char *) tmp;
		}
	}

	if (zstr(name)) {
		name = (char *) number;
	}

	if (zstr(name) || zstr(number)) {
		goto end;
	}

	caller_profile = switch_channel_get_caller_profile(channel);

	if (switch_channel_inbound_display(channel)) {

		if (!strcmp(caller_profile->caller_id_name, name) && !strcmp(caller_profile->caller_id_number, number)) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG1, "%s Same Caller ID \"%s\" <%s>\n", switch_channel_get_name(channel), name, number);
			send = 0;
		} else {
			caller_profile->caller_id_name = switch_sanitize_number(switch_core_strdup(caller_profile->pool, name));
			caller_profile->caller_id_number = switch_sanitize_number(switch_core_strdup(caller_profile->pool, number));
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "%s Update Caller ID to \"%s\" <%s>\n", switch_channel_get_name(channel), name, number);
		}

	} else {

		if (!strcmp(caller_profile->callee_id_name, name) && !strcmp(caller_profile->callee_id_number, number)) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG1, "%s Same Callee ID \"%s\" <%s>\n", switch_channel_get_name(channel), name, number);
			send = 0;
		} else {
			caller_profile->callee_id_name = switch_sanitize_number(switch_core_strdup(caller_profile->pool, name));
			caller_profile->callee_id_number = switch_sanitize_number(switch_core_strdup(caller_profile->pool, number));
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "%s Update Callee ID to \"%s\" <%s>\n", switch_channel_get_name(channel), name, number);

			if (lazy || (att && !switch_channel_get_partner_uuid(channel))) {
				switch_channel_flip_cid(channel);
			}
		}
	}

	if (send) {

		if (switch_event_create(&event, SWITCH_EVENT_CALL_UPDATE) == SWITCH_STATUS_SUCCESS) {
			const char *uuid = switch_channel_get_partner_uuid(channel);
			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "Direction", "RECV");
			if (uuid) {
				switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "Bridged-To", uuid);
			}
			switch_channel_event_set_data(channel, event);
			switch_event_fire(&event);
		}

		sofia_send_callee_id(session, NULL, NULL);
	}

  end:
	switch_safe_free(dup);
}

static void tech_send_ack(nua_handle_t *nh, private_object_t *tech_pvt, const char *r_sdp)
{
	const char *invite_full_from = switch_channel_get_variable(tech_pvt->channel, "sip_invite_full_from");
	const char *invite_full_to = switch_channel_get_variable(tech_pvt->channel, "sip_invite_full_to");
	int soa = sofia_use_soa(tech_pvt);

	if (sofia_test_pflag(tech_pvt->profile, PFLAG_TRACK_CALLS)) {
		const char *invite_full_via = switch_channel_get_variable(tech_pvt->channel, "sip_invite_full_via");
		const char *invite_route_uri = switch_channel_get_variable(tech_pvt->channel, "sip_invite_route_uri");

		nua_ack(nh,
				TAG_IF(invite_full_from, SIPTAG_FROM_STR(invite_full_from)),
				TAG_IF(invite_full_to, SIPTAG_TO_STR(invite_full_to)),
				TAG_IF(!zstr(tech_pvt->user_via), SIPTAG_VIA_STR(tech_pvt->user_via)),
				TAG_IF((zstr(tech_pvt->user_via) && !zstr(invite_full_via)), SIPTAG_VIA_STR(invite_full_via)),
				TAG_IF(!zstr(invite_route_uri), SIPTAG_ROUTE_STR(invite_route_uri)),
				TAG_IF(r_sdp && soa, SOATAG_USER_SDP_STR(r_sdp)),
				TAG_IF(r_sdp && soa, SOATAG_REUSE_REJECTED(1)),
				TAG_IF(r_sdp && soa, SOATAG_AUDIO_AUX("cn telephone-event")),
				TAG_IF(r_sdp && !soa, SIPTAG_CONTENT_TYPE_STR("application/sdp")),
				TAG_IF(r_sdp && !soa, SIPTAG_PAYLOAD_STR(r_sdp)),
				TAG_IF(r_sdp && !soa, NUTAG_MEDIA_ENABLE(0)),
				TAG_END());
	} else {
		nua_ack(nh,
				TAG_IF(invite_full_from, SIPTAG_FROM_STR(invite_full_from)),
				TAG_IF(invite_full_to, SIPTAG_TO_STR(invite_full_to)),
				TAG_IF(!zstr(tech_pvt->user_via), SIPTAG_VIA_STR(tech_pvt->user_via)),
				TAG_IF(r_sdp && soa, SOATAG_USER_SDP_STR(r_sdp)),
				TAG_IF(r_sdp && soa, SOATAG_REUSE_REJECTED(1)),
				TAG_IF(r_sdp && soa, SOATAG_AUDIO_AUX("cn telephone-event")),
				TAG_IF(r_sdp && !soa, SIPTAG_CONTENT_TYPE_STR("application/sdp")),
				TAG_IF(r_sdp && !soa, SIPTAG_PAYLOAD_STR(r_sdp)),
				TAG_IF(r_sdp && !soa, NUTAG_MEDIA_ENABLE(0)),
				TAG_END());
	}

}

static void notify_watched_header(switch_core_session_t *session, const char *msgline, const char *hdrname, const char *hdrval)
{
	switch_event_t *event = NULL;
	switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "Found known watched header in message '%s', %s: %s\n", msgline, hdrname, hdrval);
	if (switch_event_create_subclass(&event, SWITCH_EVENT_CUSTOM, MY_EVENT_NOTIFY_WATCHED_HEADER) == SWITCH_STATUS_SUCCESS) {
		switch_channel_t *channel = switch_core_session_get_channel(session);
		switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "SIP-Message", msgline);
		switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "Header-Name", hdrname);
		switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "Header-Value", hdrval);
		switch_channel_event_set_data(channel, event);
		switch_event_fire(&event);
	} else {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_CRIT, "Failed creating event of type %s!\n", MY_EVENT_NOTIFY_WATCHED_HEADER);
	}
}


static void sofia_handle_sip_r_refer(nua_t *nua, sofia_profile_t *profile, nua_handle_t *nh, switch_core_session_t *session, int status, const char *phrase, sip_t const *sip, sofia_dispatch_event_t *de, tagi_t tags[])
{
	private_object_t *tech_pvt = switch_core_session_get_private(session);
	switch_core_session_t *other_session;

	if (status < 200) {
		return;
	}

	if (tech_pvt->proxy_refer_uuid && (other_session = switch_core_session_locate(tech_pvt->proxy_refer_uuid))) {
		switch_core_session_message_t *msg;

		msg = switch_core_session_alloc(other_session, sizeof(*msg));
		msg->message_id = SWITCH_MESSAGE_INDICATE_RESPOND;
		msg->from = __FILE__;
		msg->numeric_arg = status;
		msg->string_arg = switch_core_session_strdup(other_session, phrase);
		switch_core_session_queue_message(other_session, msg);
		switch_core_session_rwunlock(other_session);
	} else {
		tech_pvt->proxy_refer_uuid = NULL;
	}
}




//sofia_dispatch_event_t *de
static void our_sofia_event_callback(nua_event_t event,
						  int status,
						  char const *phrase,
						  nua_t *nua, sofia_profile_t *profile, nua_handle_t *nh, sofia_private_t *sofia_private, sip_t const *sip,
								sofia_dispatch_event_t *de, tagi_t tags[])
{
	struct private_object *tech_pvt = NULL;
	auth_res_t auth_res = AUTH_FORBIDDEN;
	switch_core_session_t *session = NULL;
	switch_channel_t *channel = NULL;
	sofia_gateway_t *gateway = NULL;
	int locked = 0;
	int check_destroy = 1;

	profile->last_sip_event = switch_time_now();

	/* sofia_private will be == &mod_sofia_globals.keep_private whenever a request is done with a new handle that has to be
	  freed whenever the request is done */
	if (nh && sofia_private == &mod_sofia_globals.keep_private) {
		if (status >= 300) {
			nua_handle_bind(nh, NULL);
			nua_handle_destroy(nh);
			return;
		}
	}


	if (sofia_private && sofia_private != &mod_sofia_globals.destroy_private && sofia_private != &mod_sofia_globals.keep_private) {
		if (!zstr(sofia_private->gateway_name)) {
			if (!(gateway = sofia_reg_find_gateway(sofia_private->gateway_name))) {
				return;
			}
		} else if (!zstr(sofia_private->uuid)) {
			if ((session = de->init_session)) {
				de->init_session = NULL;
			} else if ((session = de->session) || (session = switch_core_session_locate(sofia_private->uuid))) {
				tech_pvt = switch_core_session_get_private(session);
				channel = switch_core_session_get_channel(session);
				if (tech_pvt) {
					switch_mutex_lock(tech_pvt->sofia_mutex);
					locked = 1;
				} else {
					if (session != de->session) switch_core_session_rwunlock(session);
					return;
				}

				if (status >= 180 && !*sofia_private->auth_gateway_name) {
					const char *gwname = switch_channel_get_variable(channel, "sip_use_gateway");
					if (!zstr(gwname)) {
						switch_set_string(sofia_private->auth_gateway_name, gwname);
					}
				}
				if (!tech_pvt->call_id && sip && sip->sip_call_id && sip->sip_call_id->i_id) {
					tech_pvt->call_id = switch_core_session_strdup(session, sip->sip_call_id->i_id);
					switch_channel_set_variable(channel, "sip_call_id", tech_pvt->call_id);
				}

				if (tech_pvt->gateway_name) {
					gateway = sofia_reg_find_gateway(tech_pvt->gateway_name);
				}

				if (channel && switch_channel_down(channel)) {
					switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "Channel is already hungup.\n");
					goto done;
				}
			} else {
				/* we can't find the session it must be hanging up or something else, its too late to do anything with it. */
				return;
			}
		}
	}

	if (session && tech_pvt && tech_pvt->watch_headers && sip) {
		char msgline[512];
		int hi;
		msg_header_t *h = NULL;
		if (sip->sip_request) {
			h = (msg_header_t *)sip->sip_request;
			msg_header_field_e(msgline, sizeof(msgline), h, 0);
		} else if (sip->sip_status) {
			h = (msg_header_t *)sip->sip_status;
			msg_header_field_e(msgline, sizeof(msgline), h, 0);
		}
		if (h) {
			sip_unknown_t *un = NULL;
			char buf[512];
			char *c = NULL;

			msgline[sizeof(msgline)-1] = '\0';
			c = strchr(msgline, '\r');
			if (c) {
				*c = '\0';
			}

			/* Faster (ie hash-based) search here would be nice? ie, make watch_headers a hash? */

			/* Search first in the valid headers */
			for (h = h->sh_succ; h; h = h->sh_succ) {
				sip_header_t *sh = (sip_header_t *)h;
				if (!sh->sh_class->hc_name) {
					continue;
				}
				for (hi = 0; tech_pvt->watch_headers[hi]; hi++) {
					if (!strcasecmp(tech_pvt->watch_headers[hi], sh->sh_class->hc_name)) {
						msg_header_field_e(buf, sizeof(buf), h, 0);
						buf[sizeof(buf)-1] = '\0';
						notify_watched_header(session, msgline, sh->sh_class->hc_name, buf);
					}
				}
			}

			/* Search now in the unknown headers */
			for (un = sip->sip_unknown; un; un = un->un_next) {
				for (hi = 0; tech_pvt->watch_headers[hi]; hi++) {
					if (!strcasecmp(tech_pvt->watch_headers[hi], un->un_name)) {
						notify_watched_header(session, msgline, un->un_name, un->un_value);
					}
				}
			}
		}
	}

	if (sofia_test_pflag(profile, PFLAG_AUTH_ALL) && tech_pvt && tech_pvt->key && sip && (event < nua_r_set_params || event > nua_r_authenticate)) {
		sip_authorization_t const *authorization = NULL;

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
											(char *) sip->sip_request->rq_method_name, tech_pvt->key, strlen(tech_pvt->key), network_ip, network_port, NULL, 0,
											REG_INVITE, NULL, NULL, NULL, NULL);
		}

		if ((auth_res != AUTH_OK && auth_res != AUTH_RENEWED)) {
			//switch_channel_hangup(channel, SWITCH_CAUSE_DESTINATION_OUT_OF_ORDER);
			nua_respond(nh, SIP_401_UNAUTHORIZED, TAG_END());
			goto done;
		}

		if (channel) {
			switch_channel_set_variable(channel, "sip_authorized", "true");
		}
	}

	if (sip && (status == 401 || status == 407)) {
		sofia_reg_handle_sip_r_challenge(status, phrase, nua, profile, nh, sofia_private, session, gateway, sip, de, tags);
		goto done;
	}

	if (sip && sip->sip_payload && sip->sip_payload->pl_data) {
		if (sip->sip_payload->pl_len != strlen(sip->sip_payload->pl_data)) {
			sip->sip_payload->pl_data = su_strndup(nh->nh_home, sip->sip_payload->pl_data, sip->sip_payload->pl_len);
		}
	}

	switch (event) {
	case nua_r_get_params:
	case nua_i_fork:
	case nua_r_info:
		break;
	case nua_r_unregister:
	case nua_r_unsubscribe:
	case nua_i_terminated:
	case nua_r_publish:
	case nua_i_error:
	case nua_i_active:
	case nua_r_set_params:
	case nua_i_prack:
	case nua_r_prack:
		break;

	case nua_i_cancel:

		if (sip && channel) {
			switch_channel_set_variable(channel, "sip_hangup_disposition", "recv_cancel");
			switch_channel_set_variable(channel, "sip_invite_failure_status", "487");
			switch_channel_set_variable(channel, "sip_invite_failure_phrase", "CANCEL");

			if (sip->sip_reason) {
				char *reason_header = sip_header_as_string(nh->nh_home, (void *) sip->sip_reason);

				if (!zstr(reason_header)) {
					switch_channel_set_variable(channel, "sip_reason", reason_header);
					switch_channel_set_variable_partner(channel, "sip_reason", reason_header);
				}
			}
		}

		break;
	case nua_r_cancel:
		{
			if (status > 299 && nh) {
				nua_handle_destroy(nh);
			}
		}
		break;
	case nua_i_ack:
		{
			if (channel && sip) {
				const char *r_sdp = NULL;

				if (sip->sip_payload && sip->sip_payload->pl_data) {
					if (sofia_test_flag(tech_pvt, TFLAG_PASS_ACK)) {
						r_sdp = sip->sip_payload->pl_data;

						if (tech_pvt->mparams.last_sdp_str) {
							tech_pvt->mparams.prev_sdp_str = tech_pvt->mparams.last_sdp_str;
						}
						tech_pvt->mparams.last_sdp_str = NULL;


						if (!zstr(tech_pvt->mparams.prev_sdp_str) && strcmp(tech_pvt->mparams.prev_sdp_str, sip->sip_payload->pl_data)) {
							switch_channel_set_variable(channel, "sip_reinvite_sdp", sip->sip_payload->pl_data);
							tech_pvt->mparams.last_sdp_str = switch_core_session_strdup(session, sip->sip_payload->pl_data);
						} else {
							tech_pvt->mparams.last_sdp_str = tech_pvt->mparams.prev_sdp_str;
						}
					} else {
						switch_channel_set_variable(channel, "sip_reinvite_sdp", sip->sip_payload->pl_data);
						tech_pvt->mparams.last_sdp_str = switch_core_session_strdup(session, sip->sip_payload->pl_data);
					}
				}

				if (sip->sip_to && sip->sip_to->a_tag) {
					switch_channel_set_variable(channel, "sip_to_tag", sip->sip_to->a_tag);
				}

				if (sip->sip_from && sip->sip_from->a_tag) {
					switch_channel_set_variable(channel, "sip_from_tag", sip->sip_from->a_tag);
				}

				if (sip->sip_cseq && sip->sip_cseq->cs_seq) {
					char sip_cseq[40] = "";
					switch_snprintf(sip_cseq, sizeof(sip_cseq), "%d", sip->sip_cseq->cs_seq);
					switch_channel_set_variable(channel, "sip_cseq", sip_cseq);
				}

				if (sip->sip_call_id && sip->sip_call_id->i_id) {
					switch_channel_set_variable(channel, "sip_call_id", sip->sip_call_id->i_id);
				}

				extract_header_vars(profile, sip, session, nh);
				switch_core_recovery_track(session);
				sofia_set_flag(tech_pvt, TFLAG_GOT_ACK);

				if (switch_channel_test_flag(tech_pvt->channel, CF_PROXY_MEDIA) && r_sdp) {
					if (sofia_test_pflag(tech_pvt->profile, PFLAG_3PCC_PROXY)) {
						switch_channel_set_variable_partner(tech_pvt->channel, SWITCH_B_SDP_VARIABLE, r_sdp);

						switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(tech_pvt->session), SWITCH_LOG_DEBUG, "3PCC-PROXY, Got my ACK\n");
						sofia_media_activate_rtp(tech_pvt);
						switch_core_media_proxy_remote_addr(tech_pvt->session, r_sdp);
						sofia_set_flag(tech_pvt, TFLAG_3PCC_HAS_ACK);
						sofia_clear_flag(tech_pvt, TFLAG_PASS_ACK);

					}

				}


				if (sofia_test_flag(tech_pvt, TFLAG_PASS_ACK)) {
					switch_core_session_t *other_session;

					sofia_clear_flag(tech_pvt, TFLAG_PASS_ACK);

					if (switch_core_session_get_partner(session, &other_session) == SWITCH_STATUS_SUCCESS) {
						if (switch_core_session_compare(session, other_session)) {
							private_object_t *other_tech_pvt = switch_core_session_get_private(other_session);
							tech_send_ack(other_tech_pvt->nh, other_tech_pvt, r_sdp);
						}
						switch_core_session_rwunlock(other_session);
					}

				}


			}
		}
	case nua_r_ack:
		if (channel)
			switch_channel_set_flag(channel, CF_MEDIA_ACK);
		break;
	case nua_r_shutdown:
		if (status >= 200) {
			sofia_set_pflag(profile, PFLAG_SHUTDOWN);
			su_root_break(profile->s_root);
		}
		break;
	case nua_r_message:
		sofia_handle_sip_r_message(status, profile, nh, sip);
		break;
	case nua_r_invite:
		sofia_handle_sip_r_invite(session, status, phrase, nua, profile, nh, sofia_private, sip, de, tags);
		break;
	case nua_r_options:
		sofia_handle_sip_r_options(session, status, phrase, nua, profile, nh, sofia_private, sip, de, tags);
		break;
	case nua_i_bye:
		sofia_handle_sip_i_bye(session, status, phrase, nua, profile, nh, sofia_private, sip, de, tags);
		break;
	case nua_r_bye:
		sofia_handle_sip_r_bye(session, status, phrase, nua, profile, nh, sofia_private, sip, de, tags);
		break;
	case nua_r_notify:
		if (session) {
			sofia_handle_sip_r_notify(session, status, phrase, nua, profile, nh, sofia_private, sip, de, tags);
		}
		break;
	case nua_i_notify:
		sofia_handle_sip_i_notify(session, status, phrase, nua, profile, nh, sofia_private, sip, de, tags);
		break;
	case nua_r_register:
		sofia_reg_handle_sip_r_register(status, phrase, nua, profile, nh, sofia_private, sip, de, tags);
		break;
	case nua_i_options:
		sofia_handle_sip_i_options(status, phrase, nua, profile, nh, sofia_private, sip, de, tags);
		break;
	case nua_i_invite:
		if (session && sofia_private) {
			if (sofia_private->is_call > 1) {
				sofia_handle_sip_i_reinvite(session, nua, profile, nh, sofia_private, sip, de, tags);
			} else {
				sofia_private->is_call++;
				sofia_handle_sip_i_invite(session, nua, profile, nh, sofia_private, sip, de, tags);
			}
		}
		break;
	case nua_i_publish:
		sofia_presence_handle_sip_i_publish(nua, profile, nh, sofia_private, sip, de, tags);
		break;
	case nua_i_register:
		//nua_respond(nh, SIP_200_OK, SIPTAG_CONTACT(sip->sip_contact), NUTAG_WITH_THIS_MSG(de->data->e_msg), TAG_END());
		//nua_handle_destroy(nh);
		sofia_reg_handle_sip_i_register(nua, profile, nh, &sofia_private, sip, de, tags);
		break;
	case nua_i_state:
		sofia_handle_sip_i_state(session, status, phrase, nua, profile, nh, sofia_private, sip, de, tags);
		break;
	case nua_i_message:
		{
			int handle_message = 1;
			int proxy_message = sofia_test_pflag(profile, PFLAG_PROXY_MESSAGE);

			if (!proxy_message && session) {
				switch_channel_t *channel = switch_core_session_get_channel(session);
				proxy_message = switch_channel_var_true(channel, "sip_proxy_message");
			}

			if (proxy_message) {
				if (sofia_proxy_sip_i_message(nua, profile, nh, session, sip, de, tags) == SWITCH_STATUS_SUCCESS) {
					handle_message = 0;
				}
			}

			if (handle_message) {
				sofia_presence_handle_sip_i_message(status, phrase, nua, profile, nh, session, sofia_private, sip, de, tags);
			}
		}
		break;
	case nua_i_info:
		{
			int handle_info = 1;
			int proxy_info = sofia_test_pflag(profile, PFLAG_PROXY_INFO);

			if (!proxy_info && session) {
				switch_channel_t *channel = switch_core_session_get_channel(session);
				proxy_info = switch_channel_var_true(channel, "sip_proxy_info");
			}

			if (proxy_info) {
				if (sofia_proxy_sip_i_info(nua, profile, nh, session, sip, de, tags) == SWITCH_STATUS_SUCCESS) {
					handle_info = 0;
				}
			}

			if (handle_info) {
				sofia_handle_sip_i_info(nua, profile, nh, session, sip, de, tags);
			}

		}
		break;
	case nua_i_update:
		if (session) {
			sofia_update_callee_id(session, profile, sip, SWITCH_TRUE);
		}
		break;
	case nua_r_update:
		if (session && tech_pvt && locked) {
			sofia_clear_flag_locked(tech_pvt, TFLAG_UPDATING_DISPLAY);
		}
		break;
	case nua_r_refer:
		if (session) {
			sofia_handle_sip_r_refer(nua, profile, nh, session, status, phrase, sip, de, tags);
		}
		break;
	case nua_i_refer:
		if (session) {
			sofia_handle_sip_i_refer(nua, profile, nh, session, sip, de, tags);
		} else {
			const char *req_user = NULL, *req_host = NULL, *action = NULL, *ref_by_user = NULL, *ref_to_user = NULL, *ref_to_host = NULL;
			char *refer_to = NULL, *referred_by = NULL, *method = NULL, *full_url = NULL;
			char *params = NULL, *iparams = NULL;
			switch_event_t *event;
			char *tmp;

			if (sip->sip_refer_to) {
				ref_to_user = sip->sip_refer_to->r_url->url_user;
				ref_to_host = sip->sip_refer_to->r_url->url_host;

				if (sip->sip_refer_to->r_url->url_params && switch_stristr("method=", sip->sip_refer_to->r_url->url_params)) {
					params = su_strdup(nua_handle_home(nh), sip->sip_refer_to->r_url->url_params);
				}


				if ((refer_to = sip_header_as_string(nua_handle_home(nh), (void *) sip->sip_refer_to))) {
					if ((iparams = strchr(refer_to, ';'))) {
						*iparams++ = '\0';

						if (!params || !switch_stristr("method=", params)) {
							params = iparams;
						}
					}

					if ((tmp = sofia_glue_get_url_from_contact(refer_to, 0))) {
						refer_to = tmp;
					}
				}

				if (params) {
					method = switch_find_parameter(params, "method", NULL);
					full_url = switch_find_parameter(params, "full_url", NULL);
				}


			}

			if (!method) {
				method = strdup("INVITE");
			}

			if (!strcasecmp(method, "INVITE")) {
				action = "call";
			} else if (!strcasecmp(method, "BYE")) {
				action = "end";
			} else {
				action = method;
			}

			if (sip->sip_referred_by) {
				referred_by = sofia_glue_get_url_from_contact(sip_header_as_string(nua_handle_home(nh), (void *) sip->sip_referred_by), 0);
				ref_by_user = sip->sip_referred_by->b_url->url_user;
			} else if(sip->sip_to){
				referred_by = sofia_glue_get_url_from_contact(sip_header_as_string(nua_handle_home(nh), (void *) sip->sip_to), 0);
				ref_by_user = sip->sip_to->a_url->url_user;
			}

			if (sip->sip_to) {
				req_user = sip->sip_to->a_url->url_user;
				req_host = sip->sip_to->a_url->url_host;
			}

			if (switch_event_create(&event, SWITCH_EVENT_CALL_SETUP_REQ) == SWITCH_STATUS_SUCCESS) {
				switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "Requesting-Component", "mod_sofia");
				switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "Target-Component", req_user);
				switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "Target-Domain", req_host);
				switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "Request-Action", action);
				switch_event_add_header(event, SWITCH_STACK_BOTTOM, "Request-Target", "sofia/%s/%s", profile->name, refer_to);
				switch_event_add_header(event, SWITCH_STACK_BOTTOM, "Request-Target-URI", "%s", refer_to);
				switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "Request-Target-Extension", ref_to_user);
				switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "Request-Target-Domain", ref_to_host);
				if (switch_true(full_url)) {
					switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "full_url", "true");
				}

				if (sip->sip_call_id && sip->sip_call_id->i_id) {
					switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "Request-Call-ID", sip->sip_call_id->i_id);
				}

				if (!zstr(referred_by)) {
					switch_event_add_header(event, SWITCH_STACK_BOTTOM, "Request-Sender", "sofia/%s/%s", profile->name, referred_by);
				}

				switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "var_origination_caller_id_number", ref_by_user);
				switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "var_origination_caller_id_name", ref_by_user);
				switch_event_fire(&event);
			}



			if (sip) {
				char *sql;
				sofia_nat_parse_t np = { { 0 } };
				char *contact_str;
				char *proto = "sip", *orig_proto = "sip";
				const char *call_id, *full_from, *full_to, *full_via, *from_user = NULL, *from_host = NULL, *to_user, *to_host, *full_agent;
				char to_tag[13] = "";
				char *event_str = "refer";
				sip_accept_t *ap = sip->sip_accept;
				char accept_header[256] = "";

				np.fs_path = 1;
				contact_str = sofia_glue_gen_contact_str(profile, sip, nh, de, &np);

				call_id = sip->sip_call_id->i_id;
				full_from = sip_header_as_string(nh->nh_home, (void *) sip->sip_from);
				full_to = sip_header_as_string(nh->nh_home, (void *) sip->sip_to);
				full_via = sip_header_as_string(nh->nh_home, (void *) sip->sip_via);

				full_agent = sip_header_as_string(nh->nh_home, (void *) sip->sip_user_agent);

				switch_stun_random_string(to_tag, 12, NULL);

				if (sip->sip_from) {
					from_user = sip->sip_from->a_url->url_user;
					from_host = sip->sip_from->a_url->url_host;
				} else {
					from_user = "n/a";
					from_host = "n/a";
				}

				if (sip->sip_to) {
					to_user = sip->sip_to->a_url->url_user;
					to_host = sip->sip_to->a_url->url_host;
				} else {
					to_user = "n/a";
					to_host = "n/a";
				}

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
									 event_str, contact_str, call_id, full_from, full_via,
									 (long) switch_epoch_time_now(NULL) + 60,
									 full_agent, accept_header, profile->name, mod_sofia_globals.hostname,
									 np.network_port, np.network_ip, orig_proto, full_to, to_tag);

				switch_assert(sql != NULL);


				if (mod_sofia_globals.debug_presence > 0 || mod_sofia_globals.debug_sla > 0) {
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "%s REFER SUBSCRIBE %s@%s %s@%s\n%s\n",
									  profile->name, from_user, from_host, to_user, to_host, sql);
				}


				sofia_glue_execute_sql_now(profile, &sql, SWITCH_TRUE);

				sip_to_tag(nh->nh_home, sip->sip_to, to_tag);
			}

			nua_respond(nh, SIP_202_ACCEPTED, SIPTAG_TO(sip->sip_to), NUTAG_WITH_THIS_MSG(de->data->e_msg), TAG_END());
			switch_safe_free(method);
			switch_safe_free(full_url);

		}
		break;
	case nua_r_subscribe:
		sofia_presence_handle_sip_r_subscribe(status, phrase, nua, profile, nh, sofia_private, sip, de, tags);
		break;
	case nua_i_subscribe:
		sofia_presence_handle_sip_i_subscribe(status, phrase, nua, profile, nh, sofia_private, sip, de, tags);
		break;
	case nua_i_media_error:
		{

			if (sofia_private && sofia_private->call_id && sofia_private->network_ip && sofia_private->network_port) {
				char *sql;
				switch_event_t *event = NULL;

				sql = switch_mprintf("delete from sip_registrations where call_id='%q' and network_ip='%q' and network_port='%q'",
										   sofia_private->call_id, sofia_private->network_ip, sofia_private->network_port);
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG1, "SOCKET DISCONNECT: %s %s:%s\n",
								  sofia_private->call_id, sofia_private->network_ip, sofia_private->network_port);
				sofia_glue_execute_sql(profile, &sql, SWITCH_TRUE);

				switch_core_del_registration(sofia_private->user, sofia_private->realm, sofia_private->call_id);



				if (switch_event_create(&event, SWITCH_EVENT_PRESENCE_IN) == SWITCH_STATUS_SUCCESS) {
					switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "proto", SOFIA_CHAT_PROTO);
					switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "rpid", "unknown");
					switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "login", profile->url);
					switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "user-agent",
												   (sip && sip->sip_user_agent) ? sip->sip_user_agent->g_string : "unknown");
					switch_event_add_header(event, SWITCH_STACK_BOTTOM, "from", "%s@%s", sofia_private->user, sofia_private->realm);
					switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "status", "Unregistered");
					switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "presence-source", "register");
					switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "event_type", "presence");
					switch_event_fire(&event);
				}


				if (switch_event_create_subclass(&event, SWITCH_EVENT_CUSTOM, MY_EVENT_UNREGISTER) == SWITCH_STATUS_SUCCESS) {
					switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "profile-name", profile->name);
					switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "from-user", sofia_private->user);
					switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "from-host", sofia_private->realm);
					switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "call-id", sofia_private->call_id);
					switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "rpid", "unknown");
					switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "reason", "socket-disconnection");
					switch_event_fire(&event);
				}


				sofia_reg_check_socket(profile, sofia_private->call_id, sofia_private->network_ip, sofia_private->network_port);
			        nua_handle_destroy(nh);
			}
		}
		break;
	case nua_r_authenticate:

		if (status >= 500) {
			if (sofia_private && !zstr(sofia_private->gateway_name)) {
				sofia_gateway_t *gateway = NULL;

				if ((gateway = sofia_reg_find_gateway(sofia_private->gateway_name))) {
					gateway->state = REG_STATE_FAILED;
					gateway->failure_status = status;
					sofia_reg_release_gateway(gateway);
				}
			} else {
				nua_handle_destroy(nh);
			}
		}

		break;
	default:
		if (status > 100) {
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "%s: unknown event %d: %03d %s\n", nua_event_name(event), event,
							  status, phrase);
		} else {
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "%s: unknown event %d\n", nua_event_name(event), event);
		}
		break;
	}

  done:

	if (tech_pvt && tech_pvt->want_event && event == tech_pvt->want_event) {
		tech_pvt->want_event = 0;
	}

	if (sofia_private && sofia_private->call_id) {
		check_destroy = 0;
	}

	switch (event) {
	case nua_i_subscribe:
	case nua_r_notify:
		check_destroy = 0;
		break;

	case nua_i_notify:

		if (sip && sip->sip_event && !strcmp(sip->sip_event->o_type, "dialog") && sip->sip_event->o_params && !strcmp(sip->sip_event->o_params[0], "sla")) {
			check_destroy = 0;
		}

		break;
	default:
		break;
	}

	if ((sofia_private && sofia_private == &mod_sofia_globals.destroy_private)) {
		nua_handle_bind(nh, NULL);
		nua_handle_destroy(nh);
		nh = NULL;
	}

	if (check_destroy) {
		if (nh && ((sofia_private && sofia_private->destroy_nh) || !nua_handle_magic(nh))) {
			if (sofia_private) {
				nua_handle_bind(nh, NULL);
			}

			if (tech_pvt && (tech_pvt->nh == nh)) {
				tech_pvt->nh = NULL;
			}

			nua_handle_destroy(nh);
			nh = NULL;
		}
	}

	if (sofia_private && sofia_private->destroy_me) {
		if (tech_pvt) {
			tech_pvt->sofia_private = NULL;
		}

		if (nh) {
			nua_handle_bind(nh, NULL);
		}
		sofia_private->destroy_me = 12;
		sofia_private_free(sofia_private);

	}

	if (gateway) {
		sofia_reg_release_gateway(gateway);
	}

	if (locked && tech_pvt) {
		switch_mutex_unlock(tech_pvt->sofia_mutex);
	}

	if (session && session != de->session) {
		switch_core_session_rwunlock(session);
	}
}

static uint32_t DE_THREAD_CNT = 0;

void *SWITCH_THREAD_FUNC sofia_msg_thread_run_once(switch_thread_t *thread, void *obj)
{
	sofia_dispatch_event_t *de = (sofia_dispatch_event_t *) obj;
	switch_memory_pool_t *pool = NULL;

	switch_mutex_lock(mod_sofia_globals.mutex);
	DE_THREAD_CNT++;
	switch_mutex_unlock(mod_sofia_globals.mutex);

	if (de) {
		pool = de->pool;
		de->pool = NULL;
		sofia_process_dispatch_event(&de);
	}

	if (pool) {
		switch_core_destroy_memory_pool(&pool);
	}

	switch_mutex_lock(mod_sofia_globals.mutex);
	DE_THREAD_CNT--;
	switch_mutex_unlock(mod_sofia_globals.mutex);

	return NULL;
}

void sofia_process_dispatch_event_in_thread(sofia_dispatch_event_t **dep)
{
	sofia_dispatch_event_t *de = *dep;
	switch_memory_pool_t *pool;
	//sofia_profile_t *profile = (*dep)->profile;
	switch_thread_data_t *td;

	switch_core_new_memory_pool(&pool);

	*dep = NULL;
	de->pool = pool;

	td = switch_core_alloc(pool, sizeof(*td));
	td->func = sofia_msg_thread_run_once;
	td->obj = de;

	switch_thread_pool_launch_thread(&td);
}

void sofia_process_dispatch_event(sofia_dispatch_event_t **dep)
{
	sofia_dispatch_event_t *de = *dep;
	nua_handle_t *nh = de->nh;
	nua_t *nua = de->nua;
	sofia_profile_t *profile = de->profile;
	sofia_private_t *sofia_private = nua_handle_magic(de->nh);
	*dep = NULL;

	our_sofia_event_callback(de->data->e_event, de->data->e_status, de->data->e_phrase, de->nua, de->profile,
							 de->nh, sofia_private, de->sip, de, (tagi_t *) de->data->e_tags);

	nua_destroy_event(de->event);
	su_free(nh->nh_home, de);

	switch_mutex_lock(profile->flag_mutex);
	profile->queued_events--;
	switch_mutex_unlock(profile->flag_mutex);

	nua_handle_unref(nh);
	nua_stack_unref(nua);
}



static int msg_queue_threads = 0;
//static int count = 0;

void *SWITCH_THREAD_FUNC sofia_msg_thread_run(switch_thread_t *thread, void *obj)
{
	void *pop;
	switch_queue_t *q = (switch_queue_t *) obj;
	int my_id;


	for (my_id = 0; my_id < mod_sofia_globals.msg_queue_len; my_id++) {
		if (mod_sofia_globals.msg_queue_thread[my_id] == thread) {
			break;
		}
	}

	switch_mutex_lock(mod_sofia_globals.mutex);
	msg_queue_threads++;
	switch_mutex_unlock(mod_sofia_globals.mutex);

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "MSG Thread %d Started\n", my_id);


	for(;;) {

		if (switch_queue_pop(q, &pop) != SWITCH_STATUS_SUCCESS) {
			switch_cond_next();
			continue;
		}

		if (pop) {
			sofia_dispatch_event_t *de = (sofia_dispatch_event_t *) pop;
			sofia_process_dispatch_event(&de);
		} else {
			break;
		}
	}

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "MSG Thread Ended\n");

	switch_mutex_lock(mod_sofia_globals.mutex);
	msg_queue_threads--;
	switch_mutex_unlock(mod_sofia_globals.mutex);

	return NULL;
}

void sofia_msg_thread_start(int idx)
{

	if (idx >= mod_sofia_globals.max_msg_queues ||
		idx >= SOFIA_MAX_MSG_QUEUE || (idx < mod_sofia_globals.msg_queue_len && mod_sofia_globals.msg_queue_thread[idx])) {
		return;
	}

	switch_mutex_lock(mod_sofia_globals.mutex);

	if (idx >= mod_sofia_globals.msg_queue_len) {
		int i;
		mod_sofia_globals.msg_queue_len = idx + 1;

		for (i = 0; i < mod_sofia_globals.msg_queue_len; i++) {
			if (!mod_sofia_globals.msg_queue_thread[i]) {
				switch_threadattr_t *thd_attr = NULL;

				switch_threadattr_create(&thd_attr, mod_sofia_globals.pool);
				switch_threadattr_stacksize_set(thd_attr, SWITCH_THREAD_STACKSIZE);
				//switch_threadattr_priority_set(thd_attr, SWITCH_PRI_REALTIME);
				switch_thread_create(&mod_sofia_globals.msg_queue_thread[i],
									 thd_attr,
									 sofia_msg_thread_run,
									 mod_sofia_globals.msg_queue,
									 mod_sofia_globals.pool);
			}
		}
	}

	switch_mutex_unlock(mod_sofia_globals.mutex);
}

//static int foo = 0;
void sofia_queue_message(sofia_dispatch_event_t *de)
{
	int launch = 0;

	if (mod_sofia_globals.running == 0 || !mod_sofia_globals.msg_queue) {
		sofia_process_dispatch_event(&de);
		return;
	}


	if (de->profile && sofia_test_pflag(de->profile, PFLAG_THREAD_PER_REG) &&
		de->data->e_event == nua_i_register && DE_THREAD_CNT < mod_sofia_globals.max_reg_threads) {
		sofia_process_dispatch_event_in_thread(&de);
		return;
	}


	if ((switch_queue_size(mod_sofia_globals.msg_queue) > (SOFIA_MSG_QUEUE_SIZE * (unsigned int)msg_queue_threads))) {
		launch++;
	}


	if (launch) {
		if (mod_sofia_globals.msg_queue_len < mod_sofia_globals.max_msg_queues) {
			sofia_msg_thread_start(mod_sofia_globals.msg_queue_len + 1);
		}
	}

	switch_queue_push(mod_sofia_globals.msg_queue, de);
}

static void set_call_id(private_object_t *tech_pvt, sip_t const *sip)
{
	if (!tech_pvt->call_id && tech_pvt->session && tech_pvt->channel && sip && sip->sip_call_id && sip->sip_call_id->i_id) {
		tech_pvt->call_id = switch_core_session_strdup(tech_pvt->session, sip->sip_call_id->i_id);
		switch_channel_set_variable(tech_pvt->channel, "sip_call_id", tech_pvt->call_id);
	}
}


void sofia_event_callback(nua_event_t event,
						  int status,
						  char const *phrase,
						  nua_t *nua, sofia_profile_t *profile, nua_handle_t *nh, sofia_private_t *sofia_private, sip_t const *sip,
						  tagi_t tags[])
{
	sofia_dispatch_event_t *de;
	int critical = (((SOFIA_MSG_QUEUE_SIZE * mod_sofia_globals.max_msg_queues) * 900) / 1000);
	uint32_t sess_count = switch_core_session_count();
	uint32_t sess_max = switch_core_session_limit(0);

	switch(event) {
	case nua_i_terminated:
		if ((status == 401 || status == 407 || status == 403) && sofia_private) {
			switch_core_session_t *session;

			if ((session = switch_core_session_locate(sofia_private->uuid))) {
				switch_channel_t *channel = switch_core_session_get_channel(session);
				int end = 0;

				if (switch_channel_direction(channel) == SWITCH_CALL_DIRECTION_INBOUND && !switch_channel_test_flag(channel, CF_ANSWERED)) {
					private_object_t *tech_pvt = switch_core_session_get_private(session);

					if (status == 403) {
						switch_channel_set_flag(channel, CF_NO_CDR);
						switch_channel_hangup(channel, SWITCH_CAUSE_CALL_REJECTED);
					} else {
						switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "detaching session %s\n", sofia_private->uuid);

						if (!zstr(tech_pvt->call_id)) {
							tech_pvt->sofia_private = NULL;
							tech_pvt->nh = NULL;
							sofia_set_flag(tech_pvt, TFLAG_BYE);
							switch_mutex_lock(profile->flag_mutex);
							switch_core_hash_insert(profile->chat_hash, tech_pvt->call_id, strdup(switch_core_session_get_uuid(session)));
							switch_mutex_unlock(profile->flag_mutex);
							nua_handle_destroy(nh);
						} else {
							switch_channel_hangup(channel, SWITCH_CAUSE_DESTINATION_OUT_OF_ORDER);
						}
					}
					end++;
				}

				switch_core_session_rwunlock(session);

				if (end) {
					goto end;
				}
			}
		}
		break;
	case nua_i_invite:
	case nua_i_register:
	case nua_i_options:
	case nua_i_notify:
	case nua_i_info:


		if (event == nua_i_invite) {
			if (sip->sip_session_expires && profile->minimum_session_expires) {
				if (sip->sip_session_expires->x_delta < profile->minimum_session_expires) {
					char buf[64] = "";
					switch_snprintf(buf, sizeof(buf), "Min-SE: %d", profile->minimum_session_expires);
					nua_respond(nh, SIP_422_SESSION_TIMER_TOO_SMALL, SIPTAG_HEADER_STR(buf),TAG_END());
					goto end;
				}
			}
		}

		if (!sofia_private) {
			if (sess_count >= sess_max || !sofia_test_pflag(profile, PFLAG_RUNNING) || !switch_core_ready_inbound()) {
				nua_respond(nh, 503, "Maximum Calls In Progress", SIPTAG_RETRY_AFTER_STR("300"), NUTAG_WITH_THIS(nua), TAG_END());
				goto end;
			}


			if (switch_queue_size(mod_sofia_globals.msg_queue) > (unsigned int)critical) {
				nua_respond(nh, 503, "System Busy", SIPTAG_RETRY_AFTER_STR("300"), NUTAG_WITH_THIS(nua), TAG_END());
				goto end;
			}

			if (sofia_test_pflag(profile, PFLAG_STANDBY)) {
				nua_respond(nh, 503, "System Paused", NUTAG_WITH_THIS(nua), TAG_END());
				goto end;
			}
		}

		break;

	default:
		break;

	}

	switch_mutex_lock(profile->flag_mutex);
	profile->queued_events++;
	switch_mutex_unlock(profile->flag_mutex);

	de = su_alloc(nh->nh_home, sizeof(*de));
	memset(de, 0, sizeof(*de));
	nua_save_event(nua, de->event);
	de->nh = nua_handle_ref(nh);
	de->data = nua_event_data(de->event);
	de->sip = sip_object(de->data->e_msg);
	de->profile = profile;
	de->nua = nua_stack_ref(nua);

	if (event == nua_i_invite && !sofia_private) {
		switch_core_session_t *session;
		private_object_t *tech_pvt = NULL;

		if (!(sofia_private = su_alloc(nh->nh_home, sizeof(*sofia_private)))) {
			abort();
		}

		memset(sofia_private, 0, sizeof(*sofia_private));
		sofia_private->is_call++;
		sofia_private->is_static++;
		nua_handle_bind(nh, sofia_private);


		if (sip->sip_call_id && sip->sip_call_id->i_id) {
			char *uuid;

			switch_mutex_lock(profile->flag_mutex);
			if ((uuid = (char *) switch_core_hash_find(profile->chat_hash, sip->sip_call_id->i_id))) {
				switch_core_hash_delete(profile->chat_hash, sip->sip_call_id->i_id);
			}
			switch_mutex_unlock(profile->flag_mutex);

			if (uuid) {
				if ((session = switch_core_session_locate(uuid))) {
					tech_pvt = switch_core_session_get_private(session);
					switch_copy_string(sofia_private->uuid_str, switch_core_session_get_uuid(session), sizeof(sofia_private->uuid_str));
					sofia_private->uuid = sofia_private->uuid_str;
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Re-attaching to session %s\n", sofia_private->uuid);
					de->init_session = session;
					sofia_clear_flag(tech_pvt, TFLAG_BYE);
					tech_pvt->sofia_private = NULL;
					tech_pvt->nh = NULL;
					switch_core_session_queue_signal_data(session, de);
					switch_core_session_rwunlock(session);
					session = NULL;
					free(uuid);
					uuid = NULL;
					goto end;
				} else {
					free(uuid);
					uuid = NULL;
					sip = NULL;
				}
			}
		}

		if (!sip || !sip->sip_call_id || zstr(sip->sip_call_id->i_id)) {
			nua_respond(nh, 503, "INVALID INVITE", TAG_END());
			nua_destroy_event(de->event);
			su_free(nh->nh_home, de);

			switch_mutex_lock(profile->flag_mutex);
			profile->queued_events--;
			switch_mutex_unlock(profile->flag_mutex);

			nua_handle_unref(nh);
			nua_stack_unref(nua);

			goto end;
		}

		if (sofia_test_pflag(profile, PFLAG_CALLID_AS_UUID)) {
			session = switch_core_session_request_uuid(sofia_endpoint_interface, SWITCH_CALL_DIRECTION_INBOUND, SOF_NONE, NULL, sip->sip_call_id->i_id);
		} else {
			session = switch_core_session_request(sofia_endpoint_interface, SWITCH_CALL_DIRECTION_INBOUND, SOF_NONE, NULL);
		}

		if (session) {
			const char *channel_name = NULL;
			tech_pvt = sofia_glue_new_pvt(session);

			if (sip->sip_from) {
				channel_name = url_set_chanvars(session, sip->sip_from->a_url, sip_from);
			}
			if (!channel_name && sip->sip_contact) {
				channel_name = url_set_chanvars(session, sip->sip_contact->m_url, sip_contact);
			}
			if (sip->sip_referred_by) {
				channel_name = url_set_chanvars(session, sip->sip_referred_by->b_url, sip_referred_by);
			}

			sofia_glue_attach_private(session, profile, tech_pvt, channel_name);

			set_call_id(tech_pvt, sip);
		} else {
			nua_respond(nh, 503, "Maximum Calls In Progress", SIPTAG_RETRY_AFTER_STR("300"), TAG_END());
			nua_destroy_event(de->event);
			su_free(nh->nh_home, de);

			switch_mutex_lock(profile->flag_mutex);
			profile->queued_events--;
			switch_mutex_unlock(profile->flag_mutex);

			nua_handle_unref(nh);
			nua_stack_unref(nua);

			goto end;
		}


		if (switch_core_session_thread_launch(session) != SWITCH_STATUS_SUCCESS) {
			char *uuid;

			if (!switch_core_session_running(session) && !switch_core_session_started(session)) {
				nua_handle_bind(nh, NULL);
				sofia_private_free(sofia_private);
				switch_core_session_destroy(&session);
				nua_respond(nh, 503, "Maximum Calls In Progress", SIPTAG_RETRY_AFTER_STR("300"), TAG_END());
			}
			switch_mutex_lock(profile->flag_mutex);
			if ((uuid = switch_core_hash_find(profile->chat_hash, tech_pvt->call_id))) {
				free(uuid);
				uuid = NULL;
				switch_core_hash_delete(profile->chat_hash, tech_pvt->call_id);
			}
			switch_mutex_unlock(profile->flag_mutex);

			goto end;
		}

		switch_copy_string(sofia_private->uuid_str, switch_core_session_get_uuid(session), sizeof(sofia_private->uuid_str));
		sofia_private->uuid = sofia_private->uuid_str;

		de->init_session = session;
		switch_core_session_queue_signal_data(session, de);
		goto end;
	}

	if (sofia_private && sofia_private != &mod_sofia_globals.destroy_private && sofia_private != &mod_sofia_globals.keep_private) {
		switch_core_session_t *session;

		if ((session = switch_core_session_locate(sofia_private->uuid))) {
			switch_core_session_queue_signal_data(session, de);
			switch_core_session_rwunlock(session);
			goto end;
		}
	}

	sofia_queue_message(de);

 end:
	//switch_cond_next();

	return;
}


void event_handler(switch_event_t *event)
{
	char *subclass, *sql;
	char *class;
	switch_event_t *pevent;

	/* Get Original Event Name */
	class = switch_event_get_header_nil(event, "orig-event-name");
	if (!strcasecmp(class, "PRESENCE_IN")) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "\nGot Presence IN event via MultiCast\n");
		if (switch_event_create(&pevent, SWITCH_EVENT_PRESENCE_IN) == SWITCH_STATUS_SUCCESS) {
			switch_event_add_header_string(pevent, SWITCH_STACK_BOTTOM, "alt_event_type", switch_event_get_header_nil(event, "orig-alt_event_type"));
			switch_event_add_header_string(pevent, SWITCH_STACK_BOTTOM, "answer-state", switch_event_get_header_nil(event, "orig-answer-state"));
			switch_event_add_header_string(pevent, SWITCH_STACK_BOTTOM, "astate", switch_event_get_header_nil(event, "orig-astate"));
			switch_event_add_header_string(pevent, SWITCH_STACK_BOTTOM, "call-direction", switch_event_get_header_nil(event, "orig-call-direction"));
			switch_event_add_header_string(pevent, SWITCH_STACK_BOTTOM, "Caller-Callee-ID-Number", switch_event_get_header_nil(event, "Orig-Caller-Callee-ID-Number"));
			switch_event_add_header_string(pevent, SWITCH_STACK_BOTTOM, "Caller-Caller-ID-Name", switch_event_get_header_nil(event, "Orig-Caller-Caller-ID-Name"));
			switch_event_add_header_string(pevent, SWITCH_STACK_BOTTOM, "Caller-Caller-ID-Number", switch_event_get_header_nil(event, "Orig-Caller-Caller-ID-Number"));
			switch_event_add_header_string(pevent, SWITCH_STACK_BOTTOM, "Caller-Destination-Number", switch_event_get_header_nil(event, "Orig-Caller-Destination-Number"));
			switch_event_add_header_string(pevent, SWITCH_STACK_BOTTOM, "Caller-Direction", switch_event_get_header_nil(event, "Orig-Caller-Direction"));
			switch_event_add_header_string(pevent, SWITCH_STACK_BOTTOM, "Caller-Username", switch_event_get_header_nil(event, "Orig-Caller-Username"));
			switch_event_add_header_string(pevent, SWITCH_STACK_BOTTOM, "channel-state", switch_event_get_header_nil(event, "orig-channel-state"));
			switch_event_add_header_string(pevent, SWITCH_STACK_BOTTOM, "force-status", switch_event_get_header_nil(event, "orig-force-status"));
			switch_event_add_header_string(pevent, SWITCH_STACK_BOTTOM, "from", switch_event_get_header_nil(event, "orig-from"));
			switch_event_add_header_string(pevent, SWITCH_STACK_BOTTOM, "login", switch_event_get_header_nil(event, "orig-login"));
			switch_event_add_header_string(pevent, SWITCH_STACK_BOTTOM, "Other-Leg-Caller-ID-Name", switch_event_get_header_nil(event, "Orig-Other-Leg-Caller-ID-Name"));
			switch_event_add_header_string(pevent, SWITCH_STACK_BOTTOM, "Other-Leg-Caller-ID-Number", switch_event_get_header_nil(event, "Orig-Other-Leg-Caller-ID-Number"));
			switch_event_add_header_string(pevent, SWITCH_STACK_BOTTOM, "presence-call-direction", switch_event_get_header_nil(event, "orig-presence-call-direction"));
			switch_event_add_header_string(pevent, SWITCH_STACK_BOTTOM, "presence-call-info-state", switch_event_get_header_nil(event, "orig-presence-call-info-state"));
			switch_event_add_header_string(pevent, SWITCH_STACK_BOTTOM, "Presence-Privacy", switch_event_get_header_nil(event, "Orig-Presence-Privacy"));
			switch_event_add_header_string(pevent, SWITCH_STACK_BOTTOM, "proto", switch_event_get_header_nil(event, "orig-proto"));
			switch_event_add_header_string(pevent, SWITCH_STACK_BOTTOM, "register-source", switch_event_get_header_nil(event, "orig-register-source"));
			switch_event_add_header_string(pevent, SWITCH_STACK_BOTTOM, "resub", switch_event_get_header_nil(event, "orig-resub"));
			switch_event_add_header_string(pevent, SWITCH_STACK_BOTTOM, "rpid", switch_event_get_header_nil(event, "orig-rpid"));
			switch_event_add_header_string(pevent, SWITCH_STACK_BOTTOM, "status", switch_event_get_header_nil(event, "orig-status"));
			switch_event_add_header_string(pevent, SWITCH_STACK_BOTTOM, "Unique-ID", switch_event_get_header_nil(event, "Orig-Unique-ID"));
			switch_event_add_header_string(pevent, SWITCH_STACK_BOTTOM, "variable_sip_from_user", switch_event_get_header_nil(event, "Orig-variable_sip_from_user"));
			switch_event_add_header_string(pevent, SWITCH_STACK_BOTTOM, "variable_sip_to_user", switch_event_get_header_nil(event, "Orig-variable_sip_to_user"));

			/* we cannot use switch_event_fire, or otherwise we'll start an endless loop */
			sofia_presence_event_handler(pevent);
			return;
		} else {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "\nCannot inject PRESENCE_IN event\n");
			return;
		}
	} else if (!strcasecmp(class, "MESSAGE_WAITING")) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "\nGot MWI event via MultiCast\n");
		if (switch_event_create(&pevent, SWITCH_EVENT_MESSAGE_WAITING) == SWITCH_STATUS_SUCCESS) {
			switch_event_add_header_string(pevent, SWITCH_STACK_BOTTOM, "MWI-Messages-Waiting", switch_event_get_header_nil(event, "orig-MWI-Messages-Waiting"));
			switch_event_add_header_string(pevent, SWITCH_STACK_BOTTOM, "MWI-Message-Account", switch_event_get_header_nil(event, "orig-MWI-Message-Account"));
			switch_event_add_header_string(pevent, SWITCH_STACK_BOTTOM, "MWI-Voice-Message", switch_event_get_header_nil(event, "orig-MWI-Voice-Message"));
			/* we cannot use switch_event_fire, or otherwise we'll start an endless loop */
			sofia_presence_event_handler(pevent);
			return;
		} else {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "\nCannot inject MWI event\n");
			return;
		}
	} else if ((subclass = switch_event_get_header_nil(event, "orig-event-subclass")) && !strcasecmp(subclass, MY_EVENT_UNREGISTER)) {
		char *profile_name = switch_event_get_header_nil(event, "orig-profile-name");
		char *from_user = switch_event_get_header_nil(event, "orig-from-user");
		char *from_host = switch_event_get_header_nil(event, "orig-from-host");
		char *call_id = switch_event_get_header_nil(event, "orig-call-id");
		char *contact_str = switch_event_get_header_nil(event, "orig-contact");

		sofia_profile_t *profile = NULL;

		if (!profile_name || !(profile = sofia_glue_find_profile(profile_name))) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Invalid Profile\n");
			return;
		}

		if (sofia_test_pflag(profile, PFLAG_MULTIREG)) {
			sql = switch_mprintf("delete from sip_registrations where call_id='%q'", call_id);
		} else {
			sql = switch_mprintf("delete from sip_registrations where sip_user='%q' and sip_host='%q'", from_user, from_host);
		}

		sofia_glue_execute_sql(profile, &sql, SWITCH_TRUE);
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Expired propagated registration for %s@%s->%s\n", from_user, from_host, contact_str);

		if (profile) {
			sofia_glue_release_profile(profile);
		}
	} else if ((subclass = switch_event_get_header_nil(event, "orig-event-subclass")) && !strcasecmp(subclass, MY_EVENT_REGISTER)) {
		char *from_user = switch_event_get_header_nil(event, "orig-from-user");
		char *from_host = switch_event_get_header_nil(event, "orig-from-host");
		char *to_host = switch_event_get_header_nil(event, "orig-to-host");
		char *contact_str = switch_event_get_header_nil(event, "orig-contact");
		char *exp_str = switch_event_get_header_nil(event, "orig-expires");
		char *rpid = switch_event_get_header_nil(event, "orig-rpid");
		char *call_id = switch_event_get_header_nil(event, "orig-call-id");
		char *user_agent = switch_event_get_header_nil(event, "orig-user-agent");
		long expires = (long) switch_epoch_time_now(NULL);
		char *profile_name = switch_event_get_header_nil(event, "orig-profile-name");
		char *to_user = switch_event_get_header_nil(event, "orig-to-user");
		char *presence_hosts = switch_event_get_header_nil(event, "orig-presence-hosts");
		char *network_ip = switch_event_get_header_nil(event, "orig-network-ip");
		char *network_port = switch_event_get_header_nil(event, "orig-network-port");
		char *username = switch_event_get_header_nil(event, "orig-username");
		char *realm = switch_event_get_header_nil(event, "orig-realm");
		char *orig_server_host = switch_event_get_header_nil(event, "orig-FreeSWITCH-IPv4");
		char *orig_hostname = switch_event_get_header_nil(event, "orig-FreeSWITCH-Hostname");
		char *fixed_contact_str = NULL;

		sofia_profile_t *profile = NULL;
		char guess_ip4[256];

		char *mwi_account = NULL;
		char *dup_mwi_account = NULL;
		char *mwi_user = NULL;
		char *mwi_host = NULL;

		if ((mwi_account = switch_event_get_header_nil(event, "orig-mwi-account"))) {
			dup_mwi_account = strdup(mwi_account);
			switch_assert(dup_mwi_account != NULL);
			switch_split_user_domain(dup_mwi_account, &mwi_user, &mwi_host);
		}

		if (!mwi_user) {
			mwi_user = (char *) from_user;
		}
		if (!mwi_host) {
			mwi_host = (char *) from_host;
		}

		if (exp_str) {
			expires += atol(exp_str);
		}

		if (!rpid) {
			rpid = "unknown";
		}

		if (!profile_name || !(profile = sofia_glue_find_profile(profile_name))) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Invalid Profile\n");
			goto end;
		}
		if (sofia_test_pflag(profile, PFLAG_MULTIREG)) {
			sql = switch_mprintf("delete from sip_registrations where call_id='%q'", call_id);
		} else {
			sql = switch_mprintf("delete from sip_registrations where sip_user='%q' and sip_host='%q'", from_user, from_host);
		}

		if (mod_sofia_globals.rewrite_multicasted_fs_path && contact_str) {
			const char *needle = ";fs_path=";
			char *sptr, *eptr = NULL;
			/* allocate enough room for worst-case scenario */
			size_t len = strlen(contact_str) + strlen(to_host) + 14;
			fixed_contact_str = malloc(len);
			switch_assert(fixed_contact_str);
			switch_copy_string(fixed_contact_str, contact_str, len);

			if ((sptr = strstr(fixed_contact_str, needle))) {
				char *origsptr = strstr(contact_str, needle);
				eptr = strchr(++origsptr, ';');
			} else {
				sptr = strchr(fixed_contact_str, '\0') - 1;
			}

			switch (mod_sofia_globals.rewrite_multicasted_fs_path) {
				case 1:
					switch_snprintf(sptr, len - (sptr - fixed_contact_str), ";fs_path=sip:%s%s", to_host, eptr ? eptr : ">");
				break;
				case 2:
					switch_snprintf(sptr, len - (sptr - fixed_contact_str), ";fs_path=sip:%s%s", orig_server_host, eptr ? eptr : ">");
				break;
				case 3:
					switch_snprintf(sptr, len - (sptr - fixed_contact_str), ";fs_path=sip:%s%s", orig_hostname, eptr ? eptr : ">");
				break;
				default:
					switch_snprintf(sptr, len - (sptr - fixed_contact_str), ";fs_path=sip:%s%s", to_host, eptr ? eptr : ">");
				break;
			}

			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Rewrote contact string from '%s' to '%s'\n", contact_str, fixed_contact_str);
			contact_str = fixed_contact_str;
		}


		sofia_glue_execute_sql(profile, &sql, SWITCH_TRUE);

		switch_find_local_ip(guess_ip4, sizeof(guess_ip4), NULL, AF_INET);
		sql = switch_mprintf("insert into sip_registrations "
							 "(call_id, sip_user, sip_host, presence_hosts, contact, status, rpid, expires,"
							 "user_agent, server_user, server_host, profile_name, hostname, network_ip, network_port, sip_username, sip_realm,"
							 "mwi_user, mwi_host, orig_server_host, orig_hostname, ping_status, ping_count) "
							 "values ('%q','%q','%q','%q','%q','Registered','%q',%ld, '%q','%q','%q','%q','%q','%q','%q','%q','%q','%q','%q','%q','%q', '%q', %d)",
							 call_id, from_user, from_host, presence_hosts, contact_str, rpid, expires, user_agent, to_user, guess_ip4,
							 profile_name, mod_sofia_globals.hostname, network_ip, network_port, username, realm, mwi_user, mwi_host,
							 orig_server_host, orig_hostname, "Reachable", 0);

		if (sql) {
			sofia_glue_execute_sql(profile, &sql, SWITCH_TRUE);
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Propagating registration for %s@%s->%s\n", from_user, from_host, contact_str);
		}


		if (profile) {
			sofia_glue_release_profile(profile);
		}
	  end:
		switch_safe_free(fixed_contact_str);
		switch_safe_free(dup_mwi_account);
	} else if ((subclass = switch_event_get_header_nil(event, "orig-event-subclass")) && !strcasecmp(subclass, MY_EVENT_SIP_USER_STATE)) {
		char *profile_name = switch_event_get_header_nil(event, "orig-profile-name");
		char *from_user = switch_event_get_header_nil(event, "orig-from-user");
		char *from_host = switch_event_get_header_nil(event, "orig-from-host");
		const char *call_id = switch_event_get_header_nil(event, "orig-call-id");
		char *ping_status = switch_event_get_header_nil(event, "orig-Ping-Status");
		sofia_profile_t *profile = NULL;

		if (!profile_name || !(profile = sofia_glue_find_profile(profile_name))) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Invalid Profile\n");
		} else {
			if (!strcmp(ping_status, "REACHABLE")) {
				sql = switch_mprintf("update sip_registrations set ping_status='%q' where sip_user='%q' and sip_host='%q' and call_id='%q'",
								 	"Reachable", from_user, from_host, call_id);
			} else {
				sql = switch_mprintf("update sip_registrations set ping_status='%q' where sip_user='%q' and sip_host='%q' and call_id='%q'",
								 	"Unreachable", from_user, from_host, call_id);
			}
			if (sql) {
				sofia_glue_execute_sql(profile, &sql, SWITCH_TRUE);
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Propagating sip_user_state for %s@%s. Ping-Status: %s\n", from_user, from_host, ping_status);
			}

			if (profile) {
				sofia_glue_release_profile(profile);
			}
        }
	}
}

static void sofia_perform_profile_start_failure(sofia_profile_t *profile, char *profile_name, char *file, int line)
{
	int arg = 0;
	switch_event_t *s_event;

	if (profile) {
		if (!strcasecmp(profile->shutdown_type, "true")) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Profile %s could not load! Shutting down!\n", profile->name);
			switch_core_session_ctl(SCSC_SHUTDOWN, &arg);
		} else if (!strcasecmp(profile->shutdown_type, "elegant")) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Profile %s could not load! Waiting for calls to finish, then shutting down!\n",
							  profile->name);
			switch_core_session_ctl(SCSC_SHUTDOWN_ELEGANT, &arg);
		} else if (!strcasecmp(profile->shutdown_type, "asap")) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Profile %s could not load! Shutting down ASAP!\n", profile->name);
			switch_core_session_ctl(SCSC_SHUTDOWN_ASAP, &arg);
		} else if (!strcasecmp(profile->shutdown_type, "reincarnate-now")) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Profile %s could not load! Asking for reincarnation now!\n", profile->name);
			switch_core_session_ctl(SCSC_REINCARNATE_NOW, &arg);
		}
	}

	if ((switch_event_create(&s_event, SWITCH_EVENT_FAILURE) == SWITCH_STATUS_SUCCESS)) {
		switch_event_add_header_string(s_event, SWITCH_STACK_BOTTOM, "module_name", "mod_sofia");
		switch_event_add_header_string(s_event, SWITCH_STACK_BOTTOM, "profile_name", profile_name);
		if (profile) {
			switch_event_add_header_string(s_event, SWITCH_STACK_BOTTOM, "profile_uri", profile->url);
		}
		switch_event_add_header_string(s_event, SWITCH_STACK_BOTTOM, "failure_message", "Profile failed to start.");
		switch_event_add_header_string(s_event, SWITCH_STACK_BOTTOM, "file", file);
		switch_event_add_header(s_event, SWITCH_STACK_BOTTOM, "line", "%d", line);

		switch_event_fire(&s_event);
	}
}

/* not a static function so that it's still visible on stacktraces */
void watchdog_triggered_abort(void) {
	abort();
}

#define sofia_profile_start_failure(p, xp) sofia_perform_profile_start_failure(p, xp, __FILE__, __LINE__)


#define SQLLEN 1024 * 1024
void *SWITCH_THREAD_FUNC sofia_profile_worker_thread_run(switch_thread_t *thread, void *obj)
{
	sofia_profile_t *profile = (sofia_profile_t *) obj;
	uint32_t ireg_loops = profile->ireg_seconds;					/* Number of loop iterations done when we haven't checked for registrations */
	uint32_t iping_loops = profile->iping_freq;					/* Number of loop iterations done when we haven't checked for ping expires */
	uint32_t gateway_loops = GATEWAY_SECONDS;			/* Number of loop iterations done when we haven't checked for gateways */
	void *pop;
	int tick = 0, x = 0;

	sofia_set_pflag_locked(profile, PFLAG_WORKER_RUNNING);

	while ((mod_sofia_globals.running == 1 && sofia_test_pflag(profile, PFLAG_RUNNING))) {

		if (tick) {
			if (profile->watchdog_enabled) {
				uint32_t event_diff = 0, step_diff = 0, event_fail = 0, step_fail = 0;

				if (profile->step_timeout) {
					step_diff = (uint32_t) ((switch_time_now() - profile->last_root_step) / 1000);

					if (step_diff > profile->step_timeout) {
						step_fail = 1;
					}
				}

				if (profile->event_timeout) {
					event_diff = (uint32_t) ((switch_time_now() - profile->last_sip_event) / 1000);

					if (event_diff > profile->event_timeout) {
						event_fail = 1;
					}
				}

				if (step_fail && profile->event_timeout && !event_fail) {
					step_fail = 0;
				}

				if (event_fail || step_fail) {
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Profile %s: SIP STACK FAILURE DETECTED BY WATCHDOG!\n"
									  "GOODBYE CRUEL WORLD, I'M LEAVING YOU TODAY....GOODBYE, GOODBYE, GOOD BYE\n", profile->name);
					switch_yield(2000000);
					watchdog_triggered_abort();
				}
			}


			if (!sofia_test_pflag(profile, PFLAG_STANDBY)) {
				if (++ireg_loops >= (uint32_t)profile->ireg_seconds) {
					time_t now = switch_epoch_time_now(NULL);
					sofia_reg_check_expire(profile, now, 0);
					ireg_loops = 0;
				}

				if(++iping_loops >= (uint32_t)profile->iping_freq) {
					time_t now = switch_epoch_time_now(NULL);
					sofia_reg_check_ping_expire(profile, now, profile->iping_seconds);
					iping_loops = 0;
				}

				if (++gateway_loops >= GATEWAY_SECONDS) {
					sofia_reg_check_gateway(profile, switch_epoch_time_now(NULL));
					sofia_sub_check_gateway(profile, switch_epoch_time_now(NULL));
					gateway_loops = 0;
				}
			}

			tick = 0;
		}

		if (switch_queue_pop_timeout(mod_sofia_globals.general_event_queue, &pop, 100000) == SWITCH_STATUS_SUCCESS) {

			do {
				switch_event_t *event = (switch_event_t *) pop;
				general_event_handler(event);
				switch_event_destroy(&event);

				pop = NULL;
				switch_queue_trypop(mod_sofia_globals.general_event_queue, &pop);
			} while (pop);

		}

		sofia_glue_fire_events(profile);

		if (++x == 10) {
			tick = 1;
			x = 0;
		}

	}

	sofia_clear_pflag_locked(profile, PFLAG_WORKER_RUNNING);

	return NULL;
}

switch_thread_t *launch_sofia_worker_thread(sofia_profile_t *profile)
{
	switch_thread_t *thread = NULL;
	switch_threadattr_t *thd_attr = NULL;
	int x = 0;
	switch_xml_t cfg = NULL, xml = NULL, xprofile = NULL, xprofiles = NULL, gateways_tag = NULL, domains_tag = NULL, domain_tag = NULL;
	switch_event_t *params = NULL;
	char *cf = "sofia.conf";

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Launching worker thread for %s\n", profile->name);

	/* Parse gateways */
	switch_event_create(&params, SWITCH_EVENT_REQUEST_PARAMS);
	switch_assert(params);
	switch_event_add_header_string(params, SWITCH_STACK_BOTTOM, "profile", profile->name);

	if (!(xml = switch_xml_open_cfg(cf, &cfg, params))) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Open of %s failed\n", cf);
		goto end;
	}

	if ((xprofiles = switch_xml_child(cfg, "profiles"))) {
		if ((xprofile = switch_xml_find_child(xprofiles, "profile", "name", profile->name))) {

			if ((gateways_tag = switch_xml_child(xprofile, "gateways"))) {
				parse_gateways(profile, gateways_tag, NULL);
			}

			if ((domains_tag = switch_xml_child(xprofile, "domains"))) {
				switch_event_t *xml_params;
				switch_event_create(&xml_params, SWITCH_EVENT_REQUEST_PARAMS);
				switch_assert(xml_params);
				switch_event_add_header_string(xml_params, SWITCH_STACK_BOTTOM, "purpose", "gateways");
				switch_event_add_header_string(xml_params, SWITCH_STACK_BOTTOM, "profile", profile->name);

				for (domain_tag = switch_xml_child(domains_tag, "domain"); domain_tag; domain_tag = domain_tag->next) {
					switch_xml_t droot, x_domain_tag;
					const char *dname = switch_xml_attr_soft(domain_tag, "name");
					const char *parse = switch_xml_attr_soft(domain_tag, "parse");
					const char *alias = switch_xml_attr_soft(domain_tag, "alias");

					if (!zstr(dname)) {
						if (!strcasecmp(dname, "all")) {
							switch_xml_t xml_root, x_domains;
							if (switch_xml_locate("directory", NULL, NULL, NULL, &xml_root, &x_domains, xml_params, SWITCH_FALSE) ==
								SWITCH_STATUS_SUCCESS) {
								for (x_domain_tag = switch_xml_child(x_domains, "domain"); x_domain_tag; x_domain_tag = x_domain_tag->next) {
									dname = switch_xml_attr_soft(x_domain_tag, "name");
									parse_domain_tag(profile, x_domain_tag, dname, parse, alias);
								}
								switch_xml_free(xml_root);
							}
						} else if (switch_xml_locate_domain(dname, xml_params, &droot, &x_domain_tag) == SWITCH_STATUS_SUCCESS) {
							parse_domain_tag(profile, x_domain_tag, dname, parse, alias);
							switch_xml_free(droot);
						}
					}
				}

				switch_event_destroy(&xml_params);
			}

		}
	}

	switch_threadattr_create(&thd_attr, profile->pool);
	switch_threadattr_stacksize_set(thd_attr, SWITCH_THREAD_STACKSIZE);
	//switch_threadattr_priority_set(thd_attr, SWITCH_PRI_REALTIME);
	switch_thread_create(&thread, thd_attr, sofia_profile_worker_thread_run, profile, profile->pool);

	while (!sofia_test_pflag(profile, PFLAG_WORKER_RUNNING)) {
		switch_yield(100000);
		if (++x >= 100) {
			break;
		}
	}

 end:
	switch_event_destroy(&params);

	if (xml) {
		switch_xml_free(xml);
	}

	return thread;
}

void *SWITCH_THREAD_FUNC sofia_profile_thread_run(switch_thread_t *thread, void *obj)
{
	sofia_profile_t *profile = (sofia_profile_t *) obj;
	//switch_memory_pool_t *pool;
	sip_alias_node_t *node;
	switch_event_t *s_event;
	int use_100rel = !sofia_test_pflag(profile, PFLAG_DISABLE_100REL);
	int use_timer = !sofia_test_pflag(profile, PFLAG_DISABLE_TIMER);
	int use_rfc_5626 = sofia_test_pflag(profile, PFLAG_ENABLE_RFC5626);
	const char *supported = NULL;
	int sanity, attempts = 0;
	switch_thread_t *worker_thread;
	switch_status_t st;
	char qname [128] = "";

	switch_mutex_lock(mod_sofia_globals.mutex);
	mod_sofia_globals.threads++;
	switch_mutex_unlock(mod_sofia_globals.mutex);

	profile->s_root = su_root_create(NULL);
	//profile->home = su_home_new(sizeof(*profile->home));

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Creating agent for %s\n", profile->name);

	if (!sofia_glue_init_sql(profile)) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Cannot Open SQL Database [%s]!\n", profile->name);
		sofia_profile_start_failure(profile, profile->name);
		sofia_glue_del_profile(profile);
		goto end;
	}

	supported = switch_core_sprintf(profile->pool, "%s%s%spath, replaces", use_100rel ? "precondition, 100rel, " : "", use_timer ? "timer, " : "", use_rfc_5626 ? "outbound, " : "");

	if (sofia_test_pflag(profile, PFLAG_AUTO_NAT) && switch_nat_get_type()) {
		if ( (! sofia_test_pflag(profile, PFLAG_TLS) || ! profile->tls_only) && switch_nat_add_mapping(profile->sip_port, SWITCH_NAT_UDP, NULL, SWITCH_FALSE) == SWITCH_STATUS_SUCCESS) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Created UDP nat mapping for %s port %d\n", profile->name, profile->sip_port);
		}
		if (switch_nat_add_mapping(profile->sip_port, SWITCH_NAT_TCP, NULL, SWITCH_FALSE) == SWITCH_STATUS_SUCCESS) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Created TCP nat mapping for %s port %d\n", profile->name, profile->sip_port);
		}
		if (sofia_test_pflag(profile, PFLAG_TLS)
			&& switch_nat_add_mapping(profile->tls_sip_port, SWITCH_NAT_TCP, NULL, SWITCH_FALSE) == SWITCH_STATUS_SUCCESS) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Created TCP/TLS nat mapping for %s port %d\n", profile->name, profile->tls_sip_port);
		}
	}

	/* We have to init the verify_subjects here as during config stage profile->home isn't setup, it should be freed when profile->home is freed */
	if ( (profile->tls_verify_policy & TPTLS_VERIFY_SUBJECTS_IN)  && profile->tls_verify_in_subjects_str && ! profile->tls_verify_in_subjects) {
		profile->tls_verify_in_subjects = su_strlst_dup_split((su_home_t *)profile->nua, profile->tls_verify_in_subjects_str, "|");
	}

	do {
		profile->nua = nua_create(profile->s_root,	/* Event loop */
								  sofia_event_callback,	/* Callback for processing events */
								  profile,	/* Additional data to pass to callback */
								  TAG_IF( ! sofia_test_pflag(profile, PFLAG_TLS) || ! profile->tls_only, NUTAG_URL(profile->bindurl)),
								  NTATAG_USER_VIA(1),
								  TPTAG_PONG2PING(1),
								  NTATAG_TCP_RPORT(0),
								  NTATAG_TLS_RPORT(0),
								  NUTAG_RETRY_AFTER_ENABLE(0),
								  NUTAG_AUTO_INVITE_100(0),
								  TAG_IF(!strchr(profile->sipip, ':'),
										 SOATAG_AF(SOA_AF_IP4_ONLY)),
								  TAG_IF(strchr(profile->sipip, ':'),
										 SOATAG_AF(SOA_AF_IP6_ONLY)),
								  TAG_IF(sofia_test_pflag(profile, PFLAG_TLS),
										 NUTAG_SIPS_URL(profile->tls_bindurl)),
								  TAG_IF(profile->ws_bindurl,
										 NUTAG_WS_URL(profile->ws_bindurl)),
								  TAG_IF(profile->wss_bindurl,
										 NUTAG_WSS_URL(profile->wss_bindurl)),
								  TAG_IF(profile->tls_cert_dir,
										 NUTAG_CERTIFICATE_DIR(profile->tls_cert_dir)),
								  TAG_IF(sofia_test_pflag(profile, PFLAG_TLS) && profile->tls_passphrase,
										 TPTAG_TLS_PASSPHRASE(profile->tls_passphrase)),
								  TAG_IF(sofia_test_pflag(profile, PFLAG_TLS),
										 TPTAG_TLS_VERIFY_POLICY(profile->tls_verify_policy)),
								  TAG_IF(sofia_test_pflag(profile, PFLAG_TLS),
										 TPTAG_TLS_VERIFY_DEPTH(profile->tls_verify_depth)),
								  TAG_IF(sofia_test_pflag(profile, PFLAG_TLS),
										 TPTAG_TLS_VERIFY_DATE(profile->tls_verify_date)),
								  TAG_IF(sofia_test_pflag(profile, PFLAG_TLS) && profile->tls_verify_in_subjects,
										 TPTAG_TLS_VERIFY_SUBJECTS(profile->tls_verify_in_subjects)),
								  TAG_IF(sofia_test_pflag(profile, PFLAG_TLS),
										 TPTAG_TLS_CIPHERS(profile->tls_ciphers)),
								  TAG_IF(sofia_test_pflag(profile, PFLAG_TLS),
										 TPTAG_TLS_VERSION(profile->tls_version)),
								  TAG_IF(sofia_test_pflag(profile, PFLAG_TLS) && profile->tls_timeout,
										 TPTAG_TLS_TIMEOUT(profile->tls_timeout)),
								  TAG_IF(!strchr(profile->sipip, ':'),
										 NTATAG_UDP_MTU(65535)),
								  TAG_IF(sofia_test_pflag(profile, PFLAG_DISABLE_SRV),
										 NTATAG_USE_SRV(0)),
								  TAG_IF(sofia_test_pflag(profile, PFLAG_DISABLE_NAPTR),
										 NTATAG_USE_NAPTR(0)),
								  TAG_IF(sofia_test_pflag(profile, PFLAG_TCP_PINGPONG),
										 TPTAG_PINGPONG(profile->tcp_pingpong)),
								  TAG_IF(sofia_test_pflag(profile, PFLAG_TCP_PING2PONG),
										 TPTAG_PINGPONG(profile->tcp_ping2pong)),
								  TAG_IF(sofia_test_pflag(profile, PFLAG_DISABLE_SRV503),
										 NTATAG_SRV_503(0)),
								  TAG_IF(sofia_test_pflag(profile, PFLAG_SOCKET_TCP_KEEPALIVE),
										 TPTAG_SOCKET_KEEPALIVE(profile->socket_tcp_keepalive)),
								  TAG_IF(sofia_test_pflag(profile, PFLAG_TCP_KEEPALIVE),
										 TPTAG_KEEPALIVE(profile->tcp_keepalive)),
								  NTATAG_DEFAULT_PROXY(profile->outbound_proxy),
								  NTATAG_SERVER_RPORT(profile->server_rport_level),
								  NTATAG_CLIENT_RPORT(profile->client_rport_level),
								  TPTAG_LOG(sofia_test_flag(profile, TFLAG_TPORT_LOG)),
								  TPTAG_CAPT(sofia_test_flag(profile, TFLAG_CAPTURE) ? mod_sofia_globals.capture_server : NULL),
								  TAG_IF(sofia_test_pflag(profile, PFLAG_SIPCOMPACT),
										 NTATAG_SIPFLAGS(MSG_DO_COMPACT)),
								  TAG_IF(profile->timer_t1, NTATAG_SIP_T1(profile->timer_t1)),
								  TAG_IF(profile->timer_t1x64, NTATAG_SIP_T1X64(profile->timer_t1x64)),
								  TAG_IF(profile->timer_t2, NTATAG_SIP_T2(profile->timer_t2)),
								  TAG_IF(profile->timer_t4, NTATAG_SIP_T4(profile->timer_t4)),
								  SIPTAG_ACCEPT_STR("application/sdp, multipart/mixed"),
								  TAG_IF(sofia_test_pflag(profile, PFLAG_NO_CONNECTION_REUSE),
										 TPTAG_REUSE(0)),
								  TAG_END());	/* Last tag should always finish the sequence */

		if (!profile->nua) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Error Creating SIP UA for profile: %s (%s) ATTEMPT %d (RETRY IN %d SEC)\n",
							  profile->name, profile->bindurl, attempts + 1, profile->bind_attempt_interval);
			if (attempts < profile->bind_attempts) {
				switch_yield(1000000 * profile->bind_attempt_interval);
			}
		}

	} while (!profile->nua && attempts++ < profile->bind_attempts);

	if (!profile->nua) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Error Creating SIP UA for profile: %s (%s)\n"
						  "The likely causes for this are:\n" "1) Another application is already listening on the specified address.\n"
						  "2) The IP the profile is attempting to bind to is not local to this system.\n", profile->name, profile->bindurl);
		sofia_profile_start_failure(profile, profile->name);
		sofia_glue_del_profile(profile);
		goto end;
	}

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Created agent for %s\n", profile->name);

	nua_set_params(profile->nua,
				   SIPTAG_ALLOW_STR("INVITE, ACK, BYE, CANCEL, OPTIONS, MESSAGE, INFO"),
				   SIPTAG_USER_AGENT(SIP_NONE),
				   NUTAG_AUTOANSWER(0),
				   NUTAG_AUTOACK(0),
				   NUTAG_AUTOALERT(0),
				   NUTAG_ENABLEMESSENGER(1),
				   NTATAG_EXTRA_100(0),
				   TAG_IF(sofia_test_pflag(profile, PFLAG_ALLOW_UPDATE), NUTAG_ALLOW("UPDATE")),
				   TAG_IF((profile->mflags & MFLAG_REGISTER), NUTAG_ALLOW("REGISTER")),
				   TAG_IF((profile->mflags & MFLAG_REFER), NUTAG_ALLOW("REFER")),
				   TAG_IF(!sofia_test_pflag(profile, PFLAG_DISABLE_100REL), NUTAG_ALLOW("PRACK")),
				   NUTAG_ALLOW("INFO"),
				   NUTAG_ALLOW("NOTIFY"),
				   NUTAG_ALLOW_EVENTS("talk"),
				   NUTAG_ALLOW_EVENTS("hold"),
				   NUTAG_ALLOW_EVENTS("conference"),
				   NUTAG_APPL_METHOD("OPTIONS"),
				   NUTAG_APPL_METHOD("INVITE"),
				   NUTAG_APPL_METHOD("REFER"),
				   NUTAG_APPL_METHOD("REGISTER"),
				   NUTAG_APPL_METHOD("NOTIFY"), NUTAG_APPL_METHOD("INFO"), NUTAG_APPL_METHOD("ACK"), NUTAG_APPL_METHOD("SUBSCRIBE"),
#ifdef MANUAL_BYE
				   NUTAG_APPL_METHOD("BYE"),
#endif
				   NUTAG_APPL_METHOD("MESSAGE"),

				   TAG_IF(profile->session_timeout && profile->minimum_session_expires, NUTAG_MIN_SE(profile->minimum_session_expires)),
				   NUTAG_SESSION_TIMER(profile->session_timeout),
				   NTATAG_MAX_PROCEEDING(profile->max_proceeding),
				   TAG_IF(profile->pres_type, NUTAG_ALLOW("PUBLISH")),
				   TAG_IF(profile->pres_type, NUTAG_ALLOW("SUBSCRIBE")),
				   TAG_IF(profile->pres_type, NUTAG_ENABLEMESSAGE(1)),
				   TAG_IF(profile->pres_type, NUTAG_ALLOW_EVENTS("presence")),
				   TAG_IF(profile->pres_type, NUTAG_ALLOW_EVENTS("as-feature-event")),
				   TAG_IF((profile->pres_type || sofia_test_pflag(profile, PFLAG_MANAGE_SHARED_APPEARANCE)), NUTAG_ALLOW_EVENTS("dialog")),
				   TAG_IF((profile->pres_type || sofia_test_pflag(profile, PFLAG_MANAGE_SHARED_APPEARANCE)), NUTAG_ALLOW_EVENTS("line-seize")),
				   TAG_IF(profile->pres_type, NUTAG_ALLOW_EVENTS("call-info")),
				   TAG_IF((profile->pres_type || sofia_test_pflag(profile, PFLAG_MANAGE_SHARED_APPEARANCE)), NUTAG_ALLOW_EVENTS("sla")),
				   TAG_IF(profile->pres_type, NUTAG_ALLOW_EVENTS("include-session-description")),
				   TAG_IF(profile->pres_type, NUTAG_ALLOW_EVENTS("presence.winfo")),
				   TAG_IF(profile->pres_type, NUTAG_ALLOW_EVENTS("message-summary")),
				   TAG_IF(profile->pres_type == PRES_TYPE_PNP, NUTAG_ALLOW_EVENTS("ua-profile")),
				   NUTAG_ALLOW_EVENTS("refer"), SIPTAG_SUPPORTED_STR(supported),
				   TAG_IF(strcasecmp(profile->user_agent, "_undef_"), SIPTAG_USER_AGENT_STR(profile->user_agent)),
				   TAG_END());

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Set params for %s\n", profile->name);

	if (sofia_test_pflag(profile, PFLAG_AUTO_ASSIGN_PORT) || sofia_test_pflag(profile, PFLAG_AUTO_ASSIGN_TLS_PORT)) {
		sip_via_t *vias = nta_agent_via(profile->nua->nua_nta);
		sip_via_t *via = NULL;

		for (via = vias; via; via = via->v_next) {
			if (sofia_test_pflag(profile, PFLAG_AUTO_ASSIGN_PORT) && !strcmp(via->v_protocol, "SIP/2.0/UDP")) {
				profile->sip_port = (switch_port_t)atoi(via->v_port);
				if (!profile->extsipport) profile->extsipport = profile->sip_port;
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Found auto sip port %d for %s\n", profile->sip_port, profile->name);
			}

			if (sofia_test_pflag(profile, PFLAG_AUTO_ASSIGN_TLS_PORT) && !strcmp(via->v_protocol, "SIP/2.0/TLS")) {
				profile->tls_sip_port = (switch_port_t)atoi(via->v_port);
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Found auto sip port %d for %s (TLS)\n", profile->tls_sip_port, profile->name);
			}

		}

		config_sofia_profile_urls(profile);
	}

	for (node = profile->aliases; node; node = node->next) {
		node->nua = nua_create(profile->s_root,	/* Event loop */
							   sofia_event_callback,	/* Callback for processing events */
							   profile,	/* Additional data to pass to callback */
							   NTATAG_SERVER_RPORT(profile->server_rport_level), NUTAG_URL(node->url), TAG_END());	/* Last tag should always finish the sequence */

		nua_set_params(node->nua,
					   SIPTAG_USER_AGENT(SIP_NONE),
					   NUTAG_APPL_METHOD("OPTIONS"),
					   NUTAG_APPL_METHOD("REFER"),
					   NUTAG_APPL_METHOD("SUBSCRIBE"),
					   NUTAG_AUTOANSWER(0),
					   NUTAG_AUTOACK(0),
					   NUTAG_AUTOALERT(0),
					   TAG_IF((profile->mflags & MFLAG_REGISTER), NUTAG_ALLOW("REGISTER")),
					   TAG_IF((profile->mflags & MFLAG_REFER), NUTAG_ALLOW("REFER")),
					   NUTAG_ALLOW("INFO"),
					   TAG_IF(profile->pres_type, NUTAG_ALLOW("PUBLISH")),
					   TAG_IF(profile->pres_type, NUTAG_ENABLEMESSAGE(1)),
					   SIPTAG_SUPPORTED_STR(supported),
					   TAG_IF(strcasecmp(profile->user_agent, "_undef_"), SIPTAG_USER_AGENT_STR(profile->user_agent)),
					   TAG_END());
	}

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Activated db for %s\n", profile->name);

	switch_mutex_init(&profile->ireg_mutex, SWITCH_MUTEX_NESTED, profile->pool);
	switch_mutex_init(&profile->dbh_mutex, SWITCH_MUTEX_NESTED, profile->pool);
	switch_mutex_init(&profile->gateway_mutex, SWITCH_MUTEX_NESTED, profile->pool);
	switch_queue_create(&profile->event_queue, SOFIA_QUEUE_SIZE, profile->pool);


	switch_snprintf(qname, sizeof(qname), "sofia:%s", profile->name);
	switch_sql_queue_manager_init_name(qname,
									   &profile->qm,
									   2,
									   profile->odbc_dsn ? profile->odbc_dsn : profile->dbname,
									   SWITCH_MAX_TRANS,
									   profile->pre_trans_execute,
									   profile->post_trans_execute,
									   profile->inner_pre_trans_execute,
									   profile->inner_post_trans_execute);
	switch_sql_queue_manager_start(profile->qm);

	if (switch_event_create(&s_event, SWITCH_EVENT_PUBLISH) == SWITCH_STATUS_SUCCESS) {
		switch_event_add_header(s_event, SWITCH_STACK_BOTTOM, "service", "_sip._udp,_sip._tcp,_sip._sctp%s",
								(sofia_test_pflag(profile, PFLAG_TLS)) ? ",_sips._tcp" : "");

		switch_event_add_header(s_event, SWITCH_STACK_BOTTOM, "port", "%d", profile->sip_port);
		switch_event_add_header_string(s_event, SWITCH_STACK_BOTTOM, "module_name", "mod_sofia");
		switch_event_add_header_string(s_event, SWITCH_STACK_BOTTOM, "profile_name", profile->name);
		switch_event_add_header_string(s_event, SWITCH_STACK_BOTTOM, "profile_uri", profile->url);

		if (sofia_test_pflag(profile, PFLAG_TLS)) {
			switch_event_add_header(s_event, SWITCH_STACK_BOTTOM, "tls_port", "%d", profile->tls_sip_port);
			switch_event_add_header_string(s_event, SWITCH_STACK_BOTTOM, "profile_tls_uri", profile->tls_url);
		}
		switch_event_fire(&s_event);
	}

	sofia_glue_add_profile(profile->name, profile);

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Starting thread for %s\n", profile->name);

	profile->started = switch_epoch_time_now(NULL);

	sofia_set_pflag_locked(profile, PFLAG_RUNNING);
	worker_thread = launch_sofia_worker_thread(profile);

	switch_yield(1000000);


	while (mod_sofia_globals.running == 1 && sofia_test_pflag(profile, PFLAG_RUNNING) && sofia_test_pflag(profile, PFLAG_WORKER_RUNNING)) {
		su_root_step(profile->s_root, 1000);
		profile->last_root_step = switch_time_now();
	}

	sofia_clear_pflag_locked(profile, PFLAG_RUNNING);
	sofia_reg_close_handles(profile);

	switch_core_session_hupall_matching_var("sofia_profile_name", profile->name, SWITCH_CAUSE_MANAGER_REQUEST);
	sanity = 10;
	while (profile->inuse) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Waiting for %d session(s)\n", profile->inuse);
		su_root_step(profile->s_root, 1000);
		if (!--sanity) {
			break;
		} else if (sanity == 5) {
			switch_core_session_hupall_matching_var("sofia_profile_name", profile->name, SWITCH_CAUSE_MANAGER_REQUEST);
		}
	}


	sofia_reg_unregister(profile);
	nua_shutdown(profile->nua);

	sanity = 100;
	while (!sofia_test_pflag(profile, PFLAG_SHUTDOWN) || profile->queued_events > 0) {
		su_root_step(profile->s_root, 1000);
		if (!--sanity) {
			break;
		}
	}

	sofia_clear_pflag_locked(profile, PFLAG_RUNNING);
	sofia_clear_pflag_locked(profile, PFLAG_SHUTDOWN);
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "Waiting for worker thread\n");

	if ( worker_thread ) {
		switch_thread_join(&st, worker_thread);
	} else {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "ERROR: Sofia worker thead failed to start\n");
	}

	sanity = 4;
	while (profile->inuse) {
		switch_core_session_hupall_matching_var("sofia_profile_name", profile->name, SWITCH_CAUSE_MANAGER_REQUEST);
		switch_yield(5000000);
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Waiting for %d session(s)\n", profile->inuse);
		if (!--sanity) {
			break;
		}
	}
	nua_destroy(profile->nua);

	switch_mutex_lock(profile->ireg_mutex);
	switch_mutex_unlock(profile->ireg_mutex);

	switch_mutex_lock(profile->flag_mutex);
	switch_mutex_unlock(profile->flag_mutex);

	switch_sql_queue_manager_destroy(&profile->qm);

	if (switch_event_create(&s_event, SWITCH_EVENT_UNPUBLISH) == SWITCH_STATUS_SUCCESS) {
		switch_event_add_header(s_event, SWITCH_STACK_BOTTOM, "service", "_sip._udp,_sip._tcp,_sip._sctp%s",
								(sofia_test_pflag(profile, PFLAG_TLS)) ? ",_sips._tcp" : "");

		switch_event_add_header(s_event, SWITCH_STACK_BOTTOM, "port", "%d", profile->sip_port);
		switch_event_add_header_string(s_event, SWITCH_STACK_BOTTOM, "module_name", "mod_sofia");
		switch_event_add_header_string(s_event, SWITCH_STACK_BOTTOM, "profile_name", profile->name);
		switch_event_add_header_string(s_event, SWITCH_STACK_BOTTOM, "profile_uri", profile->url);

		if (sofia_test_pflag(profile, PFLAG_TLS)) {
			switch_event_add_header(s_event, SWITCH_STACK_BOTTOM, "tls_port", "%d", profile->tls_sip_port);
			switch_event_add_header_string(s_event, SWITCH_STACK_BOTTOM, "profile_tls_uri", profile->tls_url);
		}
		switch_event_fire(&s_event);
	}

	if (sofia_test_pflag(profile, PFLAG_AUTO_NAT) && switch_nat_get_type()) {
		if (switch_nat_del_mapping(profile->sip_port, SWITCH_NAT_UDP) == SWITCH_STATUS_SUCCESS) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Deleted UDP nat mapping for %s port %d\n", profile->name, profile->sip_port);
		}
		if (switch_nat_del_mapping(profile->sip_port, SWITCH_NAT_TCP) == SWITCH_STATUS_SUCCESS) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Deleted TCP nat mapping for %s port %d\n", profile->name, profile->sip_port);
		}
		if (sofia_test_pflag(profile, PFLAG_TLS) && switch_nat_del_mapping(profile->tls_sip_port, SWITCH_NAT_TCP) == SWITCH_STATUS_SUCCESS) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Deleted TCP/TLS nat mapping for %s port %d\n", profile->name, profile->tls_sip_port);
		}
	}

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Write lock %s\n", profile->name);
	switch_thread_rwlock_wrlock(profile->rwlock);

	//su_home_unref(profile->home);
	su_root_destroy(profile->s_root);
	//pool = profile->pool;

	sofia_glue_del_profile(profile);
	switch_core_hash_destroy(&profile->chat_hash);
	switch_core_hash_destroy(&profile->reg_nh_hash);
	switch_core_hash_destroy(&profile->mwi_debounce_hash);

	switch_thread_rwlock_unlock(profile->rwlock);
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Write unlock %s\n", profile->name);

	if (sofia_test_pflag(profile, PFLAG_RESPAWN)) {
		config_sofia(SOFIA_CONFIG_RESPAWN, profile->name);
	}

	sofia_profile_destroy(profile);

  end:
	switch_mutex_lock(mod_sofia_globals.mutex);
	mod_sofia_globals.threads--;
	switch_mutex_unlock(mod_sofia_globals.mutex);

	return NULL;
}

void sofia_profile_destroy(sofia_profile_t *profile)
{
	if (!profile->inuse) {
		switch_memory_pool_t *pool = profile->pool;
		switch_core_destroy_memory_pool(&pool);
	} else {
		sofia_set_pflag(profile, PFLAG_DESTROY);
	}
}

void launch_sofia_profile_thread(sofia_profile_t *profile)
{
	//switch_thread_t *thread;
	switch_threadattr_t *thd_attr = NULL;

	switch_threadattr_create(&thd_attr, profile->pool);
	switch_threadattr_detach_set(thd_attr, 1);
	switch_threadattr_stacksize_set(thd_attr, SWITCH_THREAD_STACKSIZE);
	switch_threadattr_priority_set(thd_attr, SWITCH_PRI_REALTIME);
	switch_thread_create(&profile->thread, thd_attr, sofia_profile_thread_run, profile, profile->pool);
}

static int is_packet_begin_or_end(char *mybuf)
{
	if (!strncasecmp( mybuf, "recv ", 3) || !strncasecmp( mybuf, "send ", 3) ) {
		// Buffer starts with "recv" or "send", this means it's a new packet
		if (strstr(mybuf, "------------------------------------------------------------------------") != NULL) {
			// Buffer also contains the dahsed line, this is good, the complete "header" so to speak
			return 1;
		}
	} else if (!strcmp(mybuf, "   ------------------------------------------------------------------------\n")) {
		// Buffer only has the dashed line, this means it is the end of a packet
		return 2;
	}
	return 0;
}

static void logger(void *logarg, char const *fmt, va_list ap)
{
	filter_packet_state_t filter_packet_state;
    char buf[1024];
    static switch_stream_handle_t packetstream = { 0 };
    static switch_bool_t print_this_packet = SWITCH_FALSE;
    static int ovector[30];

    va_list temp_ap;
    va_copy(temp_ap,ap);

    if (!fmt) return;

    vsnprintf( buf, 1024, fmt, temp_ap);
	buf[sizeof(buf)-1] = '\0';

    if (mod_sofia_globals.filtering) {
    	if (switch_regex_perform( buf, mod_sofia_globals.filter_expression, &mod_sofia_globals.filter_re, ovector, sizeof(ovector) / sizeof(ovector[0]) ) > 0) {
    		print_this_packet = SWITCH_TRUE;
    	}

    	filter_packet_state = is_packet_begin_or_end(buf);

		if ( filter_packet_state == FILTER_BEGIN ) {
			print_this_packet = SWITCH_FALSE;
			SWITCH_STANDARD_STREAM(packetstream);
			packetstream.write_function(&packetstream, "%s", buf);

    	} else if ( filter_packet_state == FILTER_END ) {
			if ( print_this_packet == SWITCH_TRUE ) {
				switch_log_printf(SWITCH_CHANNEL_LOG_CLEAN, mod_sofia_globals.tracelevel, "\nFILTER REGEX (%s) FOUND IN: \n <<<%s>>>\n", mod_sofia_globals.filter_expression, (char *)packetstream.data );
			}
			switch_safe_free(packetstream.data);

    	} else {
			packetstream.write_function(&packetstream, "%s", buf);
    	}
    } else {
    	switch_log_printf(SWITCH_CHANNEL_LOG_CLEAN, mod_sofia_globals.tracelevel, "%s", buf	);
    }
	buf[0] = '\0';
}


static su_log_t *sofia_get_logger(const char *name)
{
	if (!strcasecmp(name, "tport")) {
		return tport_log;
	} else if (!strcasecmp(name, "iptsec")) {
		return iptsec_log;
	} else if (!strcasecmp(name, "nea")) {
		return nea_log;
	} else if (!strcasecmp(name, "nta")) {
		return nta_log;
	} else if (!strcasecmp(name, "nth_client")) {
		return nth_client_log;
	} else if (!strcasecmp(name, "nth_server")) {
		return nth_server_log;
	} else if (!strcasecmp(name, "nua")) {
		return nua_log;
	} else if (!strcasecmp(name, "soa")) {
		return soa_log;
	} else if (!strcasecmp(name, "sresolv")) {
		return sresolv_log;
#ifdef HAVE_SOFIA_STUN
	} else if (!strcasecmp(name, "stun")) {
		return stun_log;
#endif
	} else if (!strcasecmp(name, "default")) {
		return su_log_default;
	} else {
		return NULL;
	}
}

switch_status_t sofia_set_loglevel(const char *name, int level)
{
	su_log_t *log = NULL;

	if (level < 0 || level > 9) {
		return SWITCH_STATUS_FALSE;
	}

	if (!strcasecmp(name, "all")) {
		su_log_set_level(su_log_default, level);
		su_log_set_level(tport_log, level);
		su_log_set_level(iptsec_log, level);
		su_log_set_level(nea_log, level);
		su_log_set_level(nta_log, level);
		su_log_set_level(nth_client_log, level);
		su_log_set_level(nth_server_log, level);
		su_log_set_level(nua_log, level);
		su_log_set_level(soa_log, level);
		su_log_set_level(sresolv_log, level);
#ifdef HAVE_SOFIA_STUN
		su_log_set_level(stun_log, level);
#endif
		return SWITCH_STATUS_SUCCESS;
	}

	if (!(log = sofia_get_logger(name))) {
		return SWITCH_STATUS_FALSE;
	}

	su_log_set_level(log, level);

	return SWITCH_STATUS_SUCCESS;
}

int sofia_get_loglevel(const char *name)
{
	su_log_t *log = NULL;

	if ((log = sofia_get_logger(name))) {
		return log->log_level;
	} else {
		return -1;
	}
}

static void parse_gateway_subscriptions(sofia_profile_t *profile, sofia_gateway_t *gateway, switch_xml_t gw_subs_tag)
{
	switch_xml_t subscription_tag, param;

	for (subscription_tag = switch_xml_child(gw_subs_tag, "subscription"); subscription_tag; subscription_tag = subscription_tag->next) {
		sofia_gateway_subscription_t *gw_sub;

		if ((gw_sub = switch_core_alloc(profile->pool, sizeof(*gw_sub)))) {
			char *expire_seconds = "3600", *retry_seconds = "30", *content_type = "NO_CONTENT_TYPE";
			uint32_t username_in_request = 0;
			char *event = (char *) switch_xml_attr_soft(subscription_tag, "event");
			gw_sub->event = switch_core_strdup(gateway->pool, event);
			gw_sub->gateway = gateway;
			gw_sub->next = NULL;

			for (param = switch_xml_child(subscription_tag, "param"); param; param = param->next) {
				char *var = (char *) switch_xml_attr_soft(param, "name");
				char *val = (char *) switch_xml_attr_soft(param, "value");
				if (!strcmp(var, "expire-seconds")) {
					expire_seconds = val;
				} else if (!strcmp(var, "retry-seconds")) {
					retry_seconds = val;
				} else if (!strcmp(var, "content-type")) {
					content_type = val;
				} else if (!strcmp(var, "username-in-request")) {
					username_in_request = switch_true(val);
				}
			}

			gw_sub->retry_seconds = atoi(retry_seconds);
			if (gw_sub->retry_seconds < 10) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "INVALID: retry_seconds correcting the value to 30\n");
				gw_sub->retry_seconds = 30;
			}

			gw_sub->expires_str = switch_core_strdup(gateway->pool, expire_seconds);

			if ((gw_sub->freq = atoi(gw_sub->expires_str)) < 5) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "Invalid Freq: %d.  Setting Register-Frequency to 3600\n", gw_sub->freq);
				gw_sub->freq = 3600;
			}

			if(username_in_request) {
				gw_sub->request_uri = gateway->register_to;
			} else {
				gw_sub->request_uri = gateway->register_url;
			}

			gw_sub->freq -= 2;
			gw_sub->content_type = switch_core_strdup(gateway->pool, content_type);
			gw_sub->next = gateway->subscriptions;
		}
		gateway->subscriptions = gw_sub;
	}
}

static void parse_gateways(sofia_profile_t *profile, switch_xml_t gateways_tag, const char *gwname)
{
	switch_xml_t gateway_tag, param = NULL, x_params, gw_subs_tag;
	sofia_gateway_t *gp;

	for (gateway_tag = switch_xml_child(gateways_tag, "gateway"); gateway_tag; gateway_tag = gateway_tag->next) {
		char *name = (char *) switch_xml_attr_soft(gateway_tag, "name");
		sofia_gateway_t *gateway;
		char *pkey = switch_mprintf("%s::%s", profile->name, name);

		if (zstr(name) || switch_regex_match(name, "^[\\w\\.\\-\\_]+$") != SWITCH_STATUS_SUCCESS) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Ignoring invalid name '%s'\n", name ? name : "NULL");
			free(pkey);
			goto skip;
		}

		if (gwname && strcmp(gwname, name)) {
			free(pkey);
			goto skip;
		}

		switch_mutex_lock(mod_sofia_globals.hash_mutex);
		if ((gp = switch_core_hash_find(mod_sofia_globals.gateway_hash, name)) && (gp = switch_core_hash_find(mod_sofia_globals.gateway_hash, pkey)) && !gp->deleted) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "Ignoring duplicate gateway '%s'\n", name);
			switch_mutex_unlock(mod_sofia_globals.hash_mutex);
			free(pkey);
			goto skip;
		}
		free(pkey);
		switch_mutex_unlock(mod_sofia_globals.hash_mutex);

		if ((gateway = switch_core_alloc(profile->pool, sizeof(*gateway)))) {
			const char *sipip, *format;
			switch_uuid_t uuid;
			uint32_t ping_freq = 0, extension_in_contact = 0, ping_monitoring = 0, distinct_to = 0, rfc_5626 = 0;
			int ping_max = 1, ping_min = 1;
			char *register_str = "true", *scheme = "Digest",
				*realm = NULL,
				*username = NULL,
				*auth_username = NULL,
				*password = NULL,
				*caller_id_in_from = "false",
				*extension = NULL,
				*proxy = NULL,
				*options_user_agent = NULL,
				*context = profile->context,
				*expire_seconds = "3600",
				*retry_seconds = "30",
				*fail_908_retry_seconds = NULL,
				*timeout_seconds = "60",
				*from_user = "", *from_domain = NULL, *outbound_proxy = NULL, *register_proxy = NULL, *contact_host = NULL,
				*contact_params = "", *params = NULL, *register_transport = NULL,
				*reg_id = NULL, *str_rfc_5626 = "";

			if (!context) {
				context = "default";
			}

			switch_uuid_get(&uuid);
			switch_uuid_format(gateway->uuid_str, &uuid);

			gateway->register_transport = SOFIA_TRANSPORT_UDP;
			gateway->pool = profile->pool;
			gateway->profile = profile;
			gateway->name = switch_core_strdup(gateway->pool, name);
			gateway->freq = 0;
			gateway->next = NULL;
			gateway->ping = 0;
			gateway->ping_freq = 0;
			gateway->ping_max = 0;
			gateway->ping_min = 0;
			gateway->ping_sent = 0;
			gateway->ping_time = 0;
			gateway->ping_count = 0;
			gateway->ping_monitoring = SWITCH_FALSE;
			gateway->ib_calls = 0;
			gateway->ob_calls = 0;
			gateway->ib_failed_calls = 0;
			gateway->ob_failed_calls = 0;
			gateway->destination_prefix = "";

			if ((x_params = switch_xml_child(gateway_tag, "variables"))) {
				param = switch_xml_child(x_params, "variable");
			} else {
				param = switch_xml_child(gateway_tag, "variable");
			}


			for (; param; param = param->next) {
				const char *var = switch_xml_attr(param, "name");
				const char *val = switch_xml_attr(param, "value");
				const char *direction = switch_xml_attr(param, "direction");
				int in = 0, out = 0;

				if (var && val) {
					if (direction) {
						if (!strcasecmp(direction, "inbound")) {
							in = 1;
						} else if (!strcasecmp(direction, "outbound")) {
							out = 1;
						}
					} else {
						in = out = 1;
					}

					if (in) {
						if (!gateway->ib_vars) {
							switch_event_create_plain(&gateway->ib_vars, SWITCH_EVENT_GENERAL);
						}
						switch_event_add_header_string(gateway->ib_vars, SWITCH_STACK_BOTTOM, var, val);
					}

					if (out) {
						if (!gateway->ob_vars) {
							switch_event_create_plain(&gateway->ob_vars, SWITCH_EVENT_GENERAL);
						}
						switch_event_add_header_string(gateway->ob_vars, SWITCH_STACK_BOTTOM, var, val);
					}
				}
			}

			if ((x_params = switch_xml_child(gateway_tag, "params"))) {
				param = switch_xml_child(x_params, "param");
			} else {
				param = switch_xml_child(gateway_tag, "param");
			}

			for (; param; param = param->next) {
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
				} else if (!strcmp(var, "extension-in-contact")) {
					extension_in_contact = switch_true(val);
				} else if (!strcmp(var, "auth-username")) {
					auth_username = val;
				} else if (!strcmp(var, "password")) {
					password = val;
				} else if (!strcmp(var, "caller-id-in-from")) {
					caller_id_in_from = val;
				} else if (!strcmp(var, "extension")) {
					extension = val;
				} else if (!strcmp(var, "ping")) {
					ping_freq = atoi(val);
				} else if (!strcmp(var, "ping-max")) {
					ping_max = atoi(val);
				} else if (!strcmp(var, "ping-min")) {
					ping_min = atoi(val);
				} else if (!strcmp(var, "ping-user-agent")) {
					options_user_agent = val;
				} else if (!strcmp(var, "ping-monitoring")) { // if true then every gw ping result will fire a gateway status event
					ping_monitoring = switch_true(val);
				} else if (!strcmp(var, "proxy")) {
					proxy = val;
				} else if (!strcmp(var, "context")) {
					context = val;
				} else if (!strcmp(var, "expire-seconds")) {
					expire_seconds = val;
				} else if (!strcmp(var, "908-retry-seconds")) {
					fail_908_retry_seconds = val;
				} else if (!strcmp(var, "retry-seconds")) {
					retry_seconds = val;
				} else if (!strcmp(var, "timeout-seconds")) {
					timeout_seconds = val;
				} else if (!strcmp(var, "retry_seconds")) {	// support typo for back compat
					retry_seconds = val;
				} else if (!strcmp(var, "from-user")) {
					from_user = val;
				} else if (!strcmp(var, "from-domain")) {
					from_domain = val;
				} else if (!strcmp(var, "contact-host")) {
					contact_host = val;
				} else if (!strcmp(var, "register-proxy")) {
					register_proxy = val;
				} else if (!strcmp(var, "outbound-proxy")) {
					outbound_proxy = val;
				} else if (!strcmp(var, "distinct-to")) {
					distinct_to = switch_true(val);
				} else if (!strcmp(var, "destination-prefix")) {
					if (!zstr(val)) {
						gateway->destination_prefix = switch_core_strdup(gateway->pool, val);
					}
				} else if (!strcmp(var, "rfc-5626")) {
					rfc_5626 = switch_true(val);
				} else if (!strcmp(var, "reg-id")) {
					reg_id = val;
				} else if (!strcmp(var, "contact-params")) {
					contact_params = val;
				} else if (!strcmp(var, "register-transport")) {
					sofia_transport_t transport = sofia_glue_str2transport(val);

					if (transport == SOFIA_TRANSPORT_UNKNOWN || (!sofia_test_pflag(profile, PFLAG_TLS) && sofia_glue_transport_has_tls(transport))) {
						switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "ERROR: unsupported transport\n");
						goto skip;
					}

					gateway->register_transport = transport;
				}
			}

			/* RFC 5626 enable in the GW profile and the UA profile */
			if (rfc_5626 && sofia_test_pflag(profile, PFLAG_ENABLE_RFC5626)) {
				char str_guid[su_guid_strlen + 1];
				su_guid_t guid[1];
				su_guid_generate(guid);
				su_guid_sprintf(str_guid, su_guid_strlen + 1, guid);
				str_rfc_5626 = switch_core_sprintf(gateway->pool, ";reg-id=%s;+sip.instance=\"<urn:uuid:%s>\"",reg_id,str_guid);
			}

			if (zstr(realm)) {
				if (zstr(proxy)) {
					realm = name;
				} else {
					realm = proxy;
				}
			}

			if (switch_true(register_str)) {
				if (zstr(username)) {
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "ERROR: username param is REQUIRED!\n");
					goto skip;
				}

				if (zstr(password)) {
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "ERROR: password param is REQUIRED!\n");
					goto skip;
				}
			} else {
				if (zstr(username)) {
					username = "FreeSWITCH";
				}

				if (zstr(password)) {
					password = "";
				}
			}

			if (zstr(from_user)) {
				from_user = username;
			}

			if (zstr(proxy)) {
				proxy = realm;
			}

			if (!switch_true(register_str)) {
				gateway->state = REG_STATE_NOREG;
				gateway->status = SOFIA_GATEWAY_UP;
				gateway->uptime = switch_time_now();
			}

			if (zstr(auth_username)) {
				auth_username = username;
			}

			if (!zstr(register_proxy)) {
				if (strncasecmp(register_proxy, "sip:", 4) && strncasecmp(register_proxy, "sips:", 5)) {
					gateway->register_sticky_proxy = switch_core_sprintf(gateway->pool, "sip:%s", register_proxy);
				} else {
					gateway->register_sticky_proxy = switch_core_strdup(gateway->pool, register_proxy);
				}
			}

			if (!zstr(outbound_proxy)) {
				if (strncasecmp(outbound_proxy, "sip:", 4) && strncasecmp(outbound_proxy, "sips:", 5)) {
					gateway->outbound_sticky_proxy = switch_core_sprintf(gateway->pool, "sip:%s", outbound_proxy);
				} else {
					gateway->outbound_sticky_proxy = switch_core_strdup(gateway->pool, outbound_proxy);
				}
			}

			gateway->retry_seconds = atoi(retry_seconds);

			if (fail_908_retry_seconds) {
				gateway->fail_908_retry_seconds = atoi(fail_908_retry_seconds);
			}

			if (gateway->retry_seconds < 5) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "Invalid retry-seconds of %d on gateway %s, using the value of 30 instead.\n",
								  gateway->retry_seconds, name);
				gateway->retry_seconds = 30;
			}

			gateway->reg_timeout_seconds = atoi(timeout_seconds);

			if (gateway->reg_timeout_seconds < 5) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "Invalid timeout-seconds of %d on gateway %s, using the value of 60 instead.\n",
								  gateway->reg_timeout_seconds, name);
				gateway->reg_timeout_seconds = 60;
			}


			gateway->register_scheme = switch_core_strdup(gateway->pool, scheme);
			gateway->register_context = switch_core_strdup(gateway->pool, context);
			gateway->register_realm = switch_core_strdup(gateway->pool, realm);
			gateway->register_username = switch_core_strdup(gateway->pool, username);
			gateway->auth_username = switch_core_strdup(gateway->pool, auth_username);
			gateway->register_password = switch_core_strdup(gateway->pool, password);
			gateway->distinct_to = distinct_to;
			gateway->options_user_agent = options_user_agent;

			if (switch_true(caller_id_in_from)) {
				sofia_set_flag(gateway, REG_FLAG_CALLERID);
			}

			register_transport = (char *) sofia_glue_transport2str(gateway->register_transport);

			if (! zstr(contact_params)) {
				if (*contact_params == ';') {
					params = switch_core_sprintf(gateway->pool, "%s;transport=%s;gw=%s", contact_params, register_transport, gateway->name);
				} else {
					params = switch_core_sprintf(gateway->pool, ";%s;transport=%s;gw=%s", contact_params, register_transport, gateway->name);
				}
			} else {
				params = switch_core_sprintf(gateway->pool, ";transport=%s;gw=%s", register_transport, gateway->name);
			}

			if (!zstr(from_domain)) {
				gateway->from_domain = switch_core_strdup(gateway->pool, from_domain);
			}

			if (!zstr(register_transport) && !switch_stristr("transport=", proxy)) {
				gateway->register_url = switch_core_sprintf(gateway->pool, "sip:%s;transport=%s", proxy, register_transport);
			} else {
				gateway->register_url = switch_core_sprintf(gateway->pool, "sip:%s", proxy);
			}

			gateway->register_from = switch_core_sprintf(gateway->pool, "<sip:%s@%s>",
				 from_user, !zstr(from_domain) ? from_domain : proxy);

			if (ping_freq) {
				if (ping_freq >= 5) {
					gateway->ping_freq = ping_freq;
					gateway->ping_max = ping_max;
					gateway->ping_min = ping_min;
					gateway->ping_monitoring = ping_monitoring;
					gateway->ping = switch_epoch_time_now(NULL) + ping_freq;
					gateway->options_to_uri = switch_core_sprintf(gateway->pool, "<sip:%s>",
						!zstr(from_domain) ? from_domain : proxy);
					gateway->options_from_uri = gateway->options_to_uri;
				} else {
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "ERROR: invalid ping!\n");
				}
			}

			if (contact_host) {
				if (!strcmp(contact_host, "sip-ip")) {
					sipip = profile->sipip;
				} else {
					sipip = contact_host;
				}
			} else if (profile->extsipip) {
				sipip = profile->extsipip;
			} else {
				sipip = profile->sipip;
			}

			if (zstr(extension)) {
				extension = username;
			} else {
				gateway->real_extension = switch_core_strdup(gateway->pool, extension);
			}

			gateway->extension = switch_core_strdup(gateway->pool, extension);

			if (!strncasecmp(proxy, "sip:", 4)) {
				gateway->register_proxy = switch_core_strdup(gateway->pool, proxy);
				gateway->register_to = switch_core_sprintf(gateway->pool, "sip:%s@%s", username, proxy + 4);
			} else {
				gateway->register_proxy = switch_core_sprintf(gateway->pool, "sip:%s", proxy);
				gateway->register_to = switch_core_sprintf(gateway->pool, "sip:%s@%s", username, proxy);
			}

			/* This checks to make sure we provide the right contact on register for targets behind nat with us. */
			if (sofia_test_pflag(profile, PFLAG_AUTO_NAT)) {
				char *register_host = NULL;

				register_host = sofia_glue_get_register_host(gateway->register_proxy);

				if (register_host && switch_is_lan_addr(register_host)) {
					sipip = profile->sipip;
				}

				switch_safe_free(register_host);
			}

			if (extension_in_contact) {
				if (rfc_5626) {
					format = strchr(sipip, ':') ? "<sip:%s@[%s]:%d>%s" : "<sip:%s@%s:%d%s>%s";
					gateway->register_contact = switch_core_sprintf(gateway->pool, format, extension,
							sipip,
							sofia_glue_transport_has_tls(gateway->register_transport) ?
							profile->tls_sip_port : profile->extsipport, params, str_rfc_5626);

				} else {
					format = strchr(sipip, ':') ? "<sip:%s@[%s]:%d%s>" : "<sip:%s@%s:%d%s>";
					gateway->register_contact = switch_core_sprintf(gateway->pool, format, extension,
							sipip,
							sofia_glue_transport_has_tls(gateway->register_transport) ?
							profile->tls_sip_port : profile->extsipport, params);
				}
			} else {
				if (rfc_5626) {
					format = strchr(sipip, ':') ? "<sip:gw+%s@[%s]:%d%s>%s" : "<sip:gw+%s@%s:%d%s>%s";
					gateway->register_contact = switch_core_sprintf(gateway->pool, format, gateway->name,
							sipip,
							sofia_glue_transport_has_tls(gateway->register_transport) ?
							profile->tls_sip_port : profile->extsipport, params, str_rfc_5626);

				} else {
					format = strchr(sipip, ':') ? "<sip:gw+%s@[%s]:%d%s>" : "<sip:gw+%s@%s:%d%s>";
					gateway->register_contact = switch_core_sprintf(gateway->pool, format, gateway->name,
							sipip,
							sofia_glue_transport_has_tls(gateway->register_transport) ?
							profile->tls_sip_port : profile->extsipport, params);

				}
			}

			gateway->expires_str = switch_core_strdup(gateway->pool, expire_seconds);

			if ((gateway->freq = atoi(gateway->expires_str)) < 5) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING,
								  "Invalid register-frequency of %d on gateway %s, using the value of 3600 instead\n", gateway->freq, name);
				gateway->freq = 3600;
			}

			if ((gw_subs_tag = switch_xml_child(gateway_tag, "subscriptions"))) {
				parse_gateway_subscriptions(profile, gateway, gw_subs_tag);
			}

			sofia_reg_add_gateway(profile, gateway->name, gateway);

		}

	  skip:
		switch_assert(gateway_tag);
	}
}

static void parse_domain_tag(sofia_profile_t *profile, switch_xml_t x_domain_tag, const char *dname, const char *parse, const char *alias)
{
	if (switch_true(alias)) {
		if (sofia_glue_add_profile(switch_core_strdup(profile->pool, dname), profile) == SWITCH_STATUS_SUCCESS) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "Adding Alias [%s] for profile [%s]\n", dname, profile->name);
		} else {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG1, "Alias [%s] for profile [%s] (already exists)\n", dname, profile->name);
		}
	}

	if (switch_true(parse)) {
		switch_xml_t gts, gt, uts, ut, gateways_tag;
		/* Backwards Compatibility */
		for (ut = switch_xml_child(x_domain_tag, "user"); ut; ut = ut->next) {
			if (((gateways_tag = switch_xml_child(ut, "gateways")))) {
				parse_gateways(profile, gateways_tag, NULL);
			}
		}
		/* New Method with <groups> tags and users are now inside a <users> tag */
		for (gts = switch_xml_child(x_domain_tag, "groups"); gts; gts = gts->next) {
			for (gt = switch_xml_child(gts, "group"); gt; gt = gt->next) {
				for (uts = switch_xml_child(gt, "users"); uts; uts = uts->next) {
					for (ut = switch_xml_child(uts, "user"); ut; ut = ut->next) {
						if (((gateways_tag = switch_xml_child(ut, "gateways")))) {
							parse_gateways(profile, gateways_tag, NULL);
						}
					}
				}
			}
		}
	}
}

static void config_sofia_profile_urls(sofia_profile_t * profile)
{

	if (profile->extsipip) {
		char *ipv6 = strchr(profile->extsipip, ':');
		profile->public_url = switch_core_sprintf(profile->pool,
												  "sip:%s@%s%s%s:%d",
												  profile->contact_user,
												  ipv6 ? "[" : "", profile->extsipip, ipv6 ? "]" : "", profile->extsipport);
	}

	if (profile->extsipip && !sofia_test_pflag(profile, PFLAG_AUTO_NAT)) {
		char *ipv6 = strchr(profile->extsipip, ':');
		profile->url = switch_core_sprintf(profile->pool,
										   "sip:%s@%s%s%s:%d",
										   profile->contact_user, ipv6 ? "[" : "", profile->extsipip, ipv6 ? "]" : "", profile->extsipport);
		profile->bindurl = switch_core_sprintf(profile->pool, "%s;maddr=%s", profile->url, profile->sipip);
	} else {
		char *ipv6 = strchr(profile->sipip, ':');
		profile->url = switch_core_sprintf(profile->pool,
										   "sip:%s@%s%s%s:%d",
										   profile->contact_user, ipv6 ? "[" : "", profile->sipip, ipv6 ? "]" : "", profile->sip_port);
		profile->bindurl = profile->url;
	}

	profile->tcp_contact = switch_core_sprintf(profile->pool, "<%s;transport=tcp>", profile->url);

	if (profile->public_url) {
		profile->tcp_public_contact = switch_core_sprintf(profile->pool, "<%s;transport=tcp>", profile->public_url);
	}

	if (profile->bind_params) {
		char *bindurl;
		if (!switch_stristr("transport=", profile->bind_params)) {
			profile->bind_params = switch_core_sprintf(profile->pool, "%s;transport=udp,tcp", profile->bind_params);
		}
		bindurl = switch_core_sprintf(profile->pool, "%s;%s", profile->bindurl, profile->bind_params);
		profile->bindurl = bindurl;
	} else {
		char *bindurl = switch_core_sprintf(profile->pool, "%s;transport=udp,tcp", profile->bindurl);
		profile->bindurl = bindurl;
	}


	if ( profile->ws_port ) {
		char *ip = !zstr(profile->ws_ip) ? profile->ws_ip : profile->sipip;
		switch_port_t port = profile->ws_port;
		char *ipv6 = strchr(ip, ':');
		profile->ws_bindurl =
			switch_core_sprintf(profile->pool,
								"sip:%s@%s%s%s:%d;transport=ws",
								profile->contact_user, ipv6 ? "[" : "", ip, ipv6 ? "]" : "", port);
	}

	if ( profile->wss_port ) {
		char *ip = !zstr(profile->wss_ip) ? profile->wss_ip : profile->sipip;
		switch_port_t port = profile->wss_port;
		char *ipv6 = strchr(ip, ':');
		profile->wss_bindurl =
			switch_core_sprintf(profile->pool,
								"sips:%s@%s%s%s:%d;transport=wss",
								profile->contact_user, ipv6 ? "[" : "", ip, ipv6 ? "]" : "", port);
	}

	/*
	 * handle TLS params #2
	 */
	if (sofia_test_pflag(profile, PFLAG_TLS)) {
		if (!profile->tls_sip_port && !sofia_test_pflag(profile, PFLAG_AUTO_ASSIGN_TLS_PORT)) {
			profile->tls_sip_port = (switch_port_t) atoi(SOFIA_DEFAULT_TLS_PORT);
		}

		if (profile->extsipip) {
			char *ipv6 = strchr(profile->extsipip, ':');
			profile->tls_public_url = switch_core_sprintf(profile->pool,
														  "sip:%s@%s%s%s:%d",
														  profile->contact_user,
														  ipv6 ? "[" : "", profile->extsipip, ipv6 ? "]" : "", profile->tls_sip_port);
		}

		if (profile->extsipip && !sofia_test_pflag(profile, PFLAG_AUTO_NAT)) {
			char *ipv6 = strchr(profile->extsipip, ':');
			profile->tls_url =
				switch_core_sprintf(profile->pool,
									"sip:%s@%s%s%s:%d",
									profile->contact_user, ipv6 ? "[" : "", profile->extsipip, ipv6 ? "]" : "", profile->tls_sip_port);
			profile->tls_bindurl =
				switch_core_sprintf(profile->pool,
									"sips:%s@%s%s%s:%d;maddr=%s",
									profile->contact_user,
									ipv6 ? "[" : "", profile->extsipip, ipv6 ? "]" : "", profile->tls_sip_port, profile->sipip);
		} else {
			char *ipv6 = strchr(profile->sipip, ':');
			profile->tls_url =
				switch_core_sprintf(profile->pool,
									"sip:%s@%s%s%s:%d",
									profile->contact_user, ipv6 ? "[" : "", profile->sipip, ipv6 ? "]" : "", profile->tls_sip_port);
			profile->tls_bindurl =
				switch_core_sprintf(profile->pool,
									"sips:%s@%s%s%s:%d",
									profile->contact_user, ipv6 ? "[" : "", profile->sipip, ipv6 ? "]" : "", profile->tls_sip_port);
		}

		if (profile->tls_bind_params) {
			char *tls_bindurl = profile->tls_bindurl;
			profile->tls_bindurl = switch_core_sprintf(profile->pool, "%s;%s", tls_bindurl, profile->tls_bind_params);
		}

		profile->tls_contact = switch_core_sprintf(profile->pool, "<%s;transport=tls>", profile->tls_url);
		if (profile->tls_public_url) {
			profile->tls_public_contact = switch_core_sprintf(profile->pool, "<%s;transport=tls>", profile->tls_public_url);
		}


	}
}

#ifdef SOFIA_CUSTOM_TIME
/* appears to not be granular enough */
static void sofia_time(su_time_t *tv)
{
	switch_time_t now;

	if (tv) {
		now = switch_micro_time_now();
		tv->tv_sec = ((uint32_t) (now / 1000000)) + 2208988800UL;
		tv->tv_usec = (uint32_t) (now % 1000000);
	}

}
#endif

switch_status_t sofia_init(void)
{
	su_init();
	if (sip_update_default_mclass(sip_extend_mclass(NULL)) < 0) {
		su_deinit();
		return SWITCH_STATUS_GENERR;
	}

#ifdef SOFIA_TIME
	su_set_time_func(sofia_time);
#endif

	/* Redirect loggers in sofia */
	su_log_redirect(su_log_default, logger, NULL);
	su_log_redirect(tport_log, logger, NULL);
	su_log_redirect(iptsec_log, logger, NULL);
	su_log_redirect(nea_log, logger, NULL);
	su_log_redirect(nta_log, logger, NULL);
	su_log_redirect(nth_client_log, logger, NULL);
	su_log_redirect(nth_server_log, logger, NULL);
	su_log_redirect(nua_log, logger, NULL);
	su_log_redirect(soa_log, logger, NULL);
	su_log_redirect(sresolv_log, logger, NULL);
#ifdef HAVE_SOFIA_STUN
	su_log_redirect(stun_log, logger, NULL);
#endif

	return SWITCH_STATUS_SUCCESS;
}

switch_status_t config_gateway(const char *profile_name, const char *gateway_name)
{
	switch_xml_t cfg, xml = NULL, xprofiles, xprofile, gateways_tag;
	switch_status_t status = SWITCH_STATUS_SUCCESS;
	sofia_profile_t *profile = NULL;
	switch_event_t *params = NULL;
	const char *cf = "sofia.conf";

	if (zstr(profile_name) || !(profile = sofia_glue_find_profile(profile_name))) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "Profile [%s] does not exist.\n", profile_name);
		status = SWITCH_STATUS_FALSE;
		return status;
	}

	switch_event_create(&params, SWITCH_EVENT_REQUEST_PARAMS);
	switch_assert(params);
	switch_event_add_header_string(params, SWITCH_STACK_BOTTOM, "profile", profile_name);
	switch_event_add_header_string(params, SWITCH_STACK_BOTTOM, "reconfig", "true");
	switch_event_add_header_string(params, SWITCH_STACK_BOTTOM, "gateway", gateway_name);

	if (!(xml = switch_xml_open_cfg(cf, &cfg, params))) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Open of %s failed\n", gateway_name);
		status = SWITCH_STATUS_FALSE;
		goto done;
	}

	if ((xprofiles = switch_xml_child(cfg, "profiles"))) {
		if ((xprofile = switch_xml_find_child(xprofiles, "profile", "name", profile->name))) {
			if ((gateways_tag = switch_xml_child(xprofile, "gateways"))) {
				parse_gateways(profile, gateways_tag, strcmp(gateway_name, "_all_") ? gateway_name : NULL);
			}
		}
	}

	status = SWITCH_STATUS_SUCCESS;

done:

	if (profile) sofia_glue_release_profile(profile);
	if (xml) switch_xml_free(xml);

	switch_event_destroy(&params);

	return status;
}

switch_status_t config_sofia(sofia_config_t reload, char *profile_name)
{
	char *cf = "sofia.conf";
	switch_xml_t cfg, xml = NULL, xprofile, param, settings, profiles;
	switch_status_t status = SWITCH_STATUS_SUCCESS;
	sofia_profile_t *profile = NULL;
	char url[512] = "";
	int profile_found = 0;
	switch_event_t *params = NULL;
	sofia_profile_t *profile_already_started = NULL;

	if (!zstr(profile_name) && (profile = sofia_glue_find_profile(profile_name))) {
		if (reload == SOFIA_CONFIG_RESCAN) {
			profile_already_started = profile;
		} else {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "Profile [%s] Already exists.\n", switch_str_nil(profile_name));
			status = SWITCH_STATUS_FALSE;
			sofia_glue_release_profile(profile);
			return status;
		}
	}

	switch_event_create(&params, SWITCH_EVENT_REQUEST_PARAMS);
	switch_assert(params);
	switch_event_add_header_string(params, SWITCH_STACK_BOTTOM, "profile", profile_name);
	if (reload == SOFIA_CONFIG_RESCAN) {
		switch_event_add_header_string(params, SWITCH_STACK_BOTTOM, "reconfig", "true");
	}

	if (!(xml = switch_xml_open_cfg(cf, &cfg, params))) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Open of %s failed\n", cf);
		status = SWITCH_STATUS_FALSE;
		goto done;
	}

	mod_sofia_globals.auto_restart = SWITCH_TRUE;
	mod_sofia_globals.reg_deny_binding_fetch_and_no_lookup = SWITCH_FALSE; /* handle backwards compatilibity - by default use new behavior */
	mod_sofia_globals.rewrite_multicasted_fs_path = SWITCH_FALSE;

	if ((settings = switch_xml_child(cfg, "global_settings"))) {
		for (param = switch_xml_child(settings, "param"); param; param = param->next) {
			char *var = (char *) switch_xml_attr_soft(param, "name");
			char *val = (char *) switch_xml_attr_soft(param, "value");
			if (!strcasecmp(var, "log-level")) {
				su_log_set_level(NULL, atoi(val));
			} else if (!strcasecmp(var, "tracelevel")) {
				mod_sofia_globals.tracelevel = switch_log_str2level(val);
			} else if (!strcasecmp(var, "debug-presence")) {
				mod_sofia_globals.debug_presence = atoi(val);
			} else if (!strcasecmp(var, "debug-sla")) {
				mod_sofia_globals.debug_sla = atoi(val);
			} else if (!strcasecmp(var, "max-reg-threads") && val) {
				int x = atoi(val);

				if (x > 0) {
					mod_sofia_globals.max_reg_threads = x;
				}

			} else if (!strcasecmp(var, "auto-restart")) {
				mod_sofia_globals.auto_restart = switch_true(val);
			} else if (!strcasecmp(var, "reg-deny-binding-fetch-and-no-lookup")) {          /* backwards compatibility */
				mod_sofia_globals.reg_deny_binding_fetch_and_no_lookup = switch_true(val);  /* remove when noone complains about the extra lookup */
				if (switch_true(val)) {
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "Enabling reg-deny-binding-fetch-and-no-lookup - this functionality is "
															 "deprecated and will be removed - let FS devs know if you think it should stay\n");
				}
			} else if (!strcasecmp(var, "rewrite-multicasted-fs-path")) {
				if( (!strcasecmp(val, "to_host")) || (!strcasecmp(val, "1")) ) {
					/* old behaviour */
									mod_sofia_globals.rewrite_multicasted_fs_path = 1;
				} else if (!strcasecmp(val, "original_server_host")) {
									mod_sofia_globals.rewrite_multicasted_fs_path = 2;
				} else if (!strcasecmp(val, "original_hostname")) {
									mod_sofia_globals.rewrite_multicasted_fs_path = 3;
				} else {
					mod_sofia_globals.rewrite_multicasted_fs_path = SWITCH_FALSE;
				}
			} else if (!strcasecmp(var, "capture-server")) {
				mod_sofia_globals.capture_server = switch_core_strdup(mod_sofia_globals.pool, val);
			}
		}
	}

	if ((profiles = switch_xml_child(cfg, "profiles"))) {
		for (xprofile = switch_xml_child(profiles, "profile"); xprofile; xprofile = xprofile->next) {
			char *xprofilename = (char *) switch_xml_attr_soft(xprofile, "name");
			char *xprofiledomain = (char *) switch_xml_attr(xprofile, "domain");
			if (!(settings = switch_xml_child(xprofile, "settings"))) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "No Settings, check the new config!\n");
				sofia_profile_start_failure(NULL, xprofilename);
			} else {
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

				if (!profile_already_started) {

					/* Setup the pool */
					if ((status = switch_core_new_memory_pool(&pool)) != SWITCH_STATUS_SUCCESS) {
						switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Memory Error!\n");
						sofia_profile_start_failure(NULL, xprofilename);
						goto done;
					}

					if (!(profile = (sofia_profile_t *) switch_core_alloc(pool, sizeof(*profile)))) {
						switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Memory Error!\n");
						sofia_profile_start_failure(NULL, xprofilename);
						goto done;
					}

					profile->tls_verify_policy = TPTLS_VERIFY_NONE;
					sofia_set_pflag(profile, PFLAG_AUTO_INVITE_100);
					/* lib default */
					profile->tls_verify_depth = 2;


					switch_mutex_init(&profile->gw_mutex, SWITCH_MUTEX_NESTED, pool);

					profile->trans_timeout = 100;

					profile->auto_rtp_bugs = RTP_BUG_CISCO_SKIP_MARK_BIT_2833;// | RTP_BUG_SONUS_SEND_INVALID_TIMESTAMP_2833;

					profile->pool = pool;

					profile->user_agent = switch_core_sprintf(profile->pool, "FreeSWITCH-mod_sofia/%s", switch_version_full());

					profile->sip_user_ping_max = 3;
					profile->sip_user_ping_min = 1;

					profile->name = switch_core_strdup(profile->pool, xprofilename);
					switch_snprintf(url, sizeof(url), "sofia_reg_%s", xprofilename);

					if (xprofiledomain) {
						profile->domain_name = switch_core_strdup(profile->pool, xprofiledomain);
					}

					profile->dbname = switch_core_strdup(profile->pool, url);
					switch_core_hash_init(&profile->chat_hash);
					switch_core_hash_init(&profile->reg_nh_hash);
					switch_core_hash_init(&profile->mwi_debounce_hash);
					switch_thread_rwlock_create(&profile->rwlock, profile->pool);
					switch_mutex_init(&profile->flag_mutex, SWITCH_MUTEX_NESTED, profile->pool);
					profile->dtmf_duration = 100;
					profile->rtp_digit_delay = 40;
					profile->sip_force_expires = 0;
					profile->sip_force_expires_min = 0;
					profile->sip_force_expires_max = 0;
					profile->sip_expires_max_deviation = 0;
					profile->sip_expires_late_margin = 60;
					profile->sip_subscription_max_deviation = 0;
					profile->tls_ciphers = "ALL:!ADH:!LOW:!EXP:!MD5:@STRENGTH";
					profile->tls_version = SOFIA_TLS_VERSION_TLSv1;
					profile->tls_version |= SOFIA_TLS_VERSION_TLSv1_1;
					profile->tls_version |= SOFIA_TLS_VERSION_TLSv1_2;
					profile->tls_timeout = 300;
					profile->mflags = MFLAG_REFER | MFLAG_REGISTER;
					profile->server_rport_level = 1;
					profile->client_rport_level = 1;
					profile->tls_cert_dir = SWITCH_GLOBAL_dirs.certs_dir;
					sofia_set_pflag(profile, PFLAG_DISABLE_100REL);
					sofia_set_pflag(profile, PFLAG_ENABLE_CHAT);
					profile->auto_restart = 1;
					sofia_set_media_flag(profile, SCMF_AUTOFIX_TIMING);
					sofia_set_media_flag(profile, SCMF_RTP_AUTOFLUSH_DURING_BRIDGE);
					profile->contact_user = SOFIA_DEFAULT_CONTACT_USER;
					sofia_set_pflag(profile, PFLAG_PASS_CALLEE_ID);
					sofia_set_pflag(profile, PFLAG_ALLOW_UPDATE);
					sofia_set_pflag(profile, PFLAG_SEND_DISPLAY_UPDATE);
					sofia_set_pflag(profile, PFLAG_MESSAGE_QUERY_ON_FIRST_REGISTER);
					//sofia_set_pflag(profile, PFLAG_PRESENCE_ON_FIRST_REGISTER);

					sofia_clear_pflag(profile, PFLAG_CHANNEL_XML_FETCH_ON_NIGHTMARE_TRANSFER);
					sofia_clear_pflag(profile, PFLAG_MAKE_EVERY_TRANSFER_A_NIGHTMARE);
					sofia_clear_pflag(profile, PFLAG_FIRE_TRANFER_EVENTS);
					sofia_clear_pflag(profile, PFLAG_BLIND_AUTH_ENFORCE_RESULT);
					sofia_clear_pflag(profile, PFLAG_BLIND_AUTH_REPLY_403);
					sofia_clear_pflag(profile, PFLAG_AUTH_REQUIRE_USER);
					sofia_clear_pflag(profile, PFLAG_AUTH_CALLS_ACL_ONLY);
					sofia_clear_pflag(profile, PFLAG_USE_PORT_FOR_ACL_CHECK);
					profile->shutdown_type = "false";
					profile->local_network = "localnet.auto";
					sofia_set_flag(profile, TFLAG_ENABLE_SOA);
					sofia_set_pflag(profile, PFLAG_CID_IN_1XX);
					profile->mndlb |= SM_NDLB_ALLOW_NONDUP_SDP;
					profile->te = 101;
					profile->ireg_seconds = IREG_SECONDS;
					profile->iping_seconds = IPING_SECONDS;
					profile->iping_freq = IPING_FREQUENCY;
					profile->paid_type = PAID_DEFAULT;
					profile->bind_attempts = 2;
					profile->bind_attempt_interval = 5;
					profile->dtmf_type = DTMF_2833;
					profile->tls_verify_policy = TPTLS_VERIFY_NONE;
					/* lib default */
					profile->tls_verify_depth = 2;
					profile->tls_verify_date = SWITCH_TRUE;
				} else {

					/* you could change profile->foo here if it was a minor change like context or dialplan ... */
					profile->acl_count = 0;
					profile->nat_acl_count = 0;
					profile->reg_acl_count = 0;
					profile->proxy_acl_count = 0;
					sofia_set_pflag(profile, PFLAG_PASS_CALLEE_ID);
					profile->ib_calls = 0;
					profile->ob_calls = 0;
					profile->ib_failed_calls = 0;
					profile->ob_failed_calls = 0;
					profile->shutdown_type = "false";
					profile->rtpip_index = 0;
					profile->rtpip_index6 = 0;

					if (xprofiledomain) {
						profile->domain_name = switch_core_strdup(profile->pool, xprofiledomain);
					}
				}

				for (param = switch_xml_child(settings, "param"); param; param = param->next) {
					char *var = (char *) switch_xml_attr_soft(param, "name");
					char *val = (char *) switch_xml_attr_soft(param, "value");
					int found = 1; // Used to break up long if/elseif chain (MSVC2015 fails (parser stack overflow) otherwise)

					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "%s [%s]\n", var, val);

					if (!strcasecmp(var, "debug") && val) {
						profile->debug = atoi(val);
					} else if (!strcasecmp(var, "parse-invite-tel-params")) {
						profile->parse_invite_tel_params = switch_true(val);
					} else if (!strcasecmp(var, "keepalive-method") && !zstr(val)) {
						if (!strcasecmp(val, "info")) {
							profile->keepalive = KA_INFO;
						} else {
							profile->keepalive = KA_MESSAGE;
						}
					} else if (!strcasecmp(var, "bind-attempts") && val) {
						int ba = atoi(val) - 1;

						if (ba >= 0) {
							profile->bind_attempts = ba;
						}
					} else if (!strcasecmp(var, "bind-attempt-interval") && val) {
						int bai = atoi(val);

						if (bai >= 0) {
							profile->bind_attempt_interval = bai;
						}
					} else if (!strcasecmp(var, "shutdown-on-fail")) {
						profile->shutdown_type = switch_core_strdup(profile->pool, val);
					} else if (!strcasecmp(var, "sip-trace")) {
						if (switch_true(val)) {
							sofia_set_flag(profile, TFLAG_TPORT_LOG);
						} else {
							sofia_clear_flag(profile, TFLAG_TPORT_LOG);
						}
					} else if (!strcasecmp(var, "sip-capture")) {
						if (switch_true(val)) {
							sofia_set_flag(profile, TFLAG_CAPTURE);
							nua_set_params(profile->nua, TPTAG_CAPT(mod_sofia_globals.capture_server), TAG_END());
						} else {
							sofia_clear_flag(profile, TFLAG_CAPTURE);
						}
					} else if (!strcasecmp(var, "socket-tcp-keepalive") && !zstr(val)) {
						profile->socket_tcp_keepalive = atoi(val);
						sofia_set_pflag(profile, PFLAG_SOCKET_TCP_KEEPALIVE);
					} else if (!strcasecmp(var, "tcp-keepalive") && !zstr(val)) {
						profile->tcp_keepalive = atoi(val);
						sofia_set_pflag(profile, PFLAG_TCP_KEEPALIVE);
					} else if (!strcasecmp(var, "tcp-pingpong") && !zstr(val)) {
						profile->tcp_pingpong = atoi(val);
						sofia_set_pflag(profile, PFLAG_TCP_PINGPONG);
					} else if (!strcasecmp(var, "tcp-ping2pong") && !zstr(val)) {
						profile->tcp_ping2pong = atoi(val);
						sofia_set_pflag(profile, PFLAG_TCP_PING2PONG);
					} else if ((!strcasecmp(var, "proxy-refer-replaces") || !strcasecmp(var, "proxy-refer")) && !zstr(val)) {
						if (switch_true(val)) {
							sofia_set_pflag(profile, PFLAG_PROXY_REFER);
						} else {
							sofia_clear_pflag(profile, PFLAG_PROXY_REFER);
						}
					} else if (!strcasecmp(var, "sip-messages-respond-200-ok") && !zstr(val)) {
						if (switch_true(val)) {
							sofia_set_pflag(profile, PFLAG_MESSAGES_RESPOND_200_OK);
						} else {
							sofia_clear_pflag(profile, PFLAG_MESSAGES_RESPOND_200_OK);
						}
					} else if (!strcasecmp(var, "sip-subscribe-respond-200-ok") && !zstr(val)) {
						if (switch_true(val)) {
							sofia_set_pflag(profile, PFLAG_SUBSCRIBE_RESPOND_200_OK);
						} else {
							sofia_clear_pflag(profile, PFLAG_SUBSCRIBE_RESPOND_200_OK);
						}
					} else if (!strcasecmp(var, "odbc-dsn") && !zstr(val)) {
						profile->odbc_dsn = switch_core_strdup(profile->pool, val);
					} else if (!strcasecmp(var, "db-pre-trans-execute") && !zstr(val)) {
						profile->pre_trans_execute = switch_core_strdup(profile->pool, val);
					} else if (!strcasecmp(var, "db-post-trans-execute") && !zstr(val)) {
						profile->post_trans_execute = switch_core_strdup(profile->pool, val);
					} else if (!strcasecmp(var, "db-inner-pre-trans-execute") && !zstr(val)) {
						profile->inner_pre_trans_execute = switch_core_strdup(profile->pool, val);
					} else if (!strcasecmp(var, "db-inner-post-trans-execute") && !zstr(val)) {
						profile->inner_post_trans_execute = switch_core_strdup(profile->pool, val);
					} else if (!strcasecmp(var, "forward-unsolicited-mwi-notify")) {
						if (switch_true(val)) {
							sofia_set_pflag(profile, PFLAG_FORWARD_MWI_NOTIFY);
						} else {
							sofia_clear_pflag(profile, PFLAG_FORWARD_MWI_NOTIFY);
						}
					} else if (!strcasecmp(var, "NDLB-proxy-never-patch-reinvites")) {
						if (switch_true(val)) {
							profile->mndlb |= SM_NDLB_NEVER_PATCH_REINVITE;
						} else {
							profile->mndlb &= ~SM_NDLB_NEVER_PATCH_REINVITE;
						}
					} else if (!strcasecmp(var, "registration-thread-frequency") && !zstr(val)) {
						profile->ireg_seconds = atoi(val);
						if (profile->ireg_seconds < 0) {
							profile->ireg_seconds = IREG_SECONDS;
						}
					} else if (!strcasecmp(var, "ping-mean-interval") && !zstr(val)) {
						profile->iping_seconds = atoi(val);
						if (profile->iping_seconds < 0) {
							profile->iping_seconds = IPING_SECONDS;
						}
					} else if (!strcasecmp(var, "ping-thread-frequency") && !zstr(val)) {
						profile->iping_freq = atoi(val);
						if (profile->iping_freq < 0) {
							profile->iping_freq = IPING_FREQUENCY;
						}
					} else if (!strcasecmp(var, "user-agent-string")) {
						profile->user_agent = switch_core_strdup(profile->pool, val);
					} else if (!strcasecmp(var, "auto-restart")) {
						profile->auto_restart = switch_true(val);
					} else if (!strcasecmp(var, "log-auth-failures")) {
						if (switch_true(val)) {
							sofia_set_pflag(profile, PFLAG_LOG_AUTH_FAIL);
						} else {
							sofia_clear_pflag(profile, PFLAG_LOG_AUTH_FAIL);
						}
					} else if (!strcasecmp(var, "confirm-blind-transfer")) {
						if (switch_true(val)) {
							sofia_set_pflag(profile, PFLAG_CONFIRM_BLIND_TRANSFER);
						} else {
							sofia_clear_pflag(profile, PFLAG_CONFIRM_BLIND_TRANSFER);
						}
					} else if (!strcasecmp(var, "allow-update")) {
						if (switch_true(val)) {
							sofia_set_pflag(profile, PFLAG_ALLOW_UPDATE);
						} else {
							sofia_clear_pflag(profile, PFLAG_ALLOW_UPDATE);
						}
					} else if (!strcasecmp(var, "send-display-update")) {
						if (switch_true(val)) {
							sofia_set_pflag(profile, PFLAG_SEND_DISPLAY_UPDATE);
						} else {
							sofia_clear_pflag(profile, PFLAG_SEND_DISPLAY_UPDATE);
						}
					} else if (!strcasecmp(var, "mwi-use-reg-callid")) {
						if (switch_true(val)) {
							sofia_set_pflag(profile, PFLAG_MWI_USE_REG_CALLID);
						} else {
							sofia_clear_pflag(profile, PFLAG_MWI_USE_REG_CALLID);
						}
					} else if (!strcasecmp(var, "tcp-unreg-on-socket-close")) {
						if (switch_true(val)) {
							sofia_set_pflag(profile, PFLAG_TCP_UNREG_ON_SOCKET_CLOSE);
						} else {
							sofia_clear_pflag(profile, PFLAG_TCP_UNREG_ON_SOCKET_CLOSE);
						}
					} else if (!strcasecmp(var, "tcp-always-nat")) {
						if (switch_true(val)) {
							sofia_set_pflag(profile, PFLAG_TCP_ALWAYS_NAT);
						} else {
							sofia_clear_pflag(profile, PFLAG_TCP_ALWAYS_NAT);
						}
					} else if (!strcasecmp(var, "tls-always-nat")) {
						if (switch_true(val)) {
							sofia_set_pflag(profile, PFLAG_TLS_ALWAYS_NAT);
						} else {
							sofia_clear_pflag(profile, PFLAG_TLS_ALWAYS_NAT);
						}
					} else if (!strcasecmp(var, "presence-proto-lookup")) {
						if (switch_true(val)) {
							sofia_set_pflag(profile, PFLAG_PRESENCE_MAP);
						} else {
							sofia_clear_pflag(profile, PFLAG_PRESENCE_MAP);
						}
					} else if (!strcasecmp(var, "profile-standby")) {
						if (switch_true(val)) {
							sofia_set_pflag(profile, PFLAG_STANDBY);
						} else {
							sofia_clear_pflag(profile, PFLAG_STANDBY);
						}
					} else if (!strcasecmp(var, "liberal-dtmf")) {
						if (switch_true(val)) {
							sofia_set_pflag(profile, PFLAG_LIBERAL_DTMF);
						} else {
							sofia_clear_pflag(profile, PFLAG_LIBERAL_DTMF);
						}
					} else if (!strcasecmp(var, "rtp-digit-delay") && !zstr(val)) {
						int delay = atoi(val);
						if (delay < 0) {
							delay = 0;
						}
						profile->rtp_digit_delay = (uint32_t) delay;
					} else if (!strcasecmp(var, "watchdog-enabled")) {
						profile->watchdog_enabled = switch_true(val);
					} else if (!strcasecmp(var, "watchdog-step-timeout") && !zstr(val)) {
						profile->step_timeout = atoi(val);
					} else if (!strcasecmp(var, "watchdog-event-timeout") && !zstr(val)) {
						profile->event_timeout = atoi(val);
					} else if (!strcasecmp(var, "in-dialog-chat")) {
						if (switch_true(val)) {
							sofia_set_pflag(profile, PFLAG_IN_DIALOG_CHAT);
						} else {
							sofia_clear_pflag(profile, PFLAG_IN_DIALOG_CHAT);
						}
					} else if (!strcasecmp(var, "enable-chat")) {
						if (switch_true(val)) {
							sofia_set_pflag(profile, PFLAG_ENABLE_CHAT);
						} else {
							sofia_clear_pflag(profile, PFLAG_ENABLE_CHAT);
						}
					} else if (!strcasecmp(var, "fire-bye-response-events")) {
						if (switch_true(val)) {
							sofia_set_pflag(profile, PFLAG_FIRE_BYE_RESPONSE_EVENTS);
						} else {
							sofia_clear_pflag(profile, PFLAG_FIRE_BYE_RESPONSE_EVENTS);
						}
					} else if (!strcasecmp(var, "fire-message-events")) {
						if (switch_true(val)) {
							sofia_set_pflag(profile, PFLAG_FIRE_MESSAGE_EVENTS);
						} else {
							sofia_clear_pflag(profile, PFLAG_FIRE_MESSAGE_EVENTS);
						}
					} else if (!strcasecmp(var, "t38-passthru")) {
						if (switch_true(val)) {
							sofia_set_pflag(profile, PFLAG_T38_PASSTHRU);
						} else {
							sofia_clear_pflag(profile, PFLAG_T38_PASSTHRU);
						}
					} else if (!strcasecmp(var, "presence-disable-early")) {
						if (switch_true(val)) {
							sofia_set_pflag(profile, PFLAG_PRESENCE_DISABLE_EARLY);
						} else {
							sofia_clear_pflag(profile, PFLAG_PRESENCE_DISABLE_EARLY);
						}
					} else if (!strcasecmp(var, "ignore-183nosdp")) {
						if (switch_true(val)) {
							sofia_set_pflag(profile, PFLAG_IGNORE_183NOSDP);
						} else {
							sofia_clear_pflag(profile, PFLAG_IGNORE_183NOSDP);
						}
					} else if (!strcasecmp(var, "presence-probe-on-register")) {
						if (switch_true(val)) {
							sofia_set_pflag(profile, PFLAG_PRESENCE_PROBE_ON_REGISTER);
						} else {
							sofia_clear_pflag(profile, PFLAG_PRESENCE_PROBE_ON_REGISTER);
						}
					} else if (!strcasecmp(var, "send-presence-on-register")) {
						if (val && (switch_true(val) || !strcasecmp(val, "all"))) {
							sofia_set_pflag(profile, PFLAG_PRESENCE_ON_REGISTER);
						} else if (val && !strcasecmp(val, "first-only")) {
							sofia_clear_pflag(profile, PFLAG_PRESENCE_ON_REGISTER);
							sofia_set_pflag(profile, PFLAG_PRESENCE_ON_FIRST_REGISTER);
						} else {
							sofia_clear_pflag(profile, PFLAG_PRESENCE_ON_REGISTER);
							sofia_clear_pflag(profile, PFLAG_PRESENCE_ON_FIRST_REGISTER);
						}
					} else if (!strcasecmp(var, "cid-in-1xx")) {
						if (switch_true(val)) {
							sofia_set_pflag(profile, PFLAG_CID_IN_1XX);
						} else {
							sofia_clear_pflag(profile, PFLAG_CID_IN_1XX);
						}
					} else if (!strcasecmp(var, "disable-hold")) {
						if (switch_true(val)) {
							sofia_set_media_flag(profile, SCMF_DISABLE_HOLD);
						} else {
							sofia_clear_media_flag(profile, SCMF_DISABLE_HOLD);
						}
					} else if (!strcasecmp(var, "auto-jitterbuffer-msec") && !zstr(val)) {
						int msec = atoi(val);
						if (msec > 19) {
							profile->jb_msec = switch_core_strdup(profile->pool, val);
						}
					} else if (!strcasecmp(var, "dtmf-type")) {
						if (val && !strcasecmp(val, "rfc2833")) {
							profile->dtmf_type = DTMF_2833;
						} else if (val && !strcasecmp(val, "info")) {
							profile->dtmf_type = DTMF_INFO;
						} else {
							profile->dtmf_type = DTMF_NONE;
						}
					} else if (!strcasecmp(var, "NDLB-force-rport")) {
						if (val && !strcasecmp(val, "safe")) {
							profile->server_rport_level = 3;
							profile->client_rport_level = 1;
						} else if (val && !strcasecmp(val, "disabled")) {
							profile->server_rport_level = 0;
							profile->client_rport_level = 0;
						} else if (val && !strcasecmp(val, "client-only")) {
							profile->client_rport_level = 1;
						} else if (val && !strcasecmp(val, "server-only")) {
							profile->client_rport_level = 0;
							profile->server_rport_level = 1;
						} else if (switch_true(val)) {
							profile->server_rport_level = 2;
							profile->client_rport_level = 1;
						}
					} else if (!strcasecmp(var, "auto-rtp-bugs")) {
						switch_core_media_parse_rtp_bugs(&profile->auto_rtp_bugs, val);
					} else if (!strcasecmp(var, "manual-rtp-bugs")) {
						switch_core_media_parse_rtp_bugs(&profile->manual_rtp_bugs, val);
					} else if (!strcasecmp(var, "manual-video-rtp-bugs")) {
						switch_core_media_parse_rtp_bugs(&profile->manual_video_rtp_bugs, val);
					} else if (!strcasecmp(var, "dbname")) {
						profile->dbname = switch_core_strdup(profile->pool, val);
					} else if (!strcasecmp(var, "presence-hosts")) {
						profile->presence_hosts = switch_core_strdup(profile->pool, val);
					} else if (!strcasecmp(var, "caller-id-type")) {
						profile->cid_type = sofia_cid_name2type(val);
					} else if (!strcasecmp(var, "record-template")) {
						profile->record_template = switch_core_strdup(profile->pool, val);
					} else if (!strcasecmp(var, "record-path")) {
						profile->record_path = switch_core_strdup(profile->pool, val);
					} else if ((!strcasecmp(var, "inbound-no-media") || !strcasecmp(var, "inbound-bypass-media"))) {
						if (switch_true(val)) {
							sofia_set_flag(profile, TFLAG_INB_NOMEDIA);
						} else {
							sofia_clear_flag(profile, TFLAG_INB_NOMEDIA);
						}
					} else if (!strcasecmp(var, "inbound-late-negotiation")) {
						if (switch_true(val)) {
							sofia_set_flag(profile, TFLAG_LATE_NEGOTIATION);
						} else {
							sofia_clear_flag(profile, TFLAG_LATE_NEGOTIATION);
						}
					} else if (!strcasecmp(var, "rtp-notimer-during-bridge")) {
						if (switch_true(val)) {
							sofia_set_pflag(profile, PFLAG_RTP_NOTIMER_DURING_BRIDGE);
						} else {
							sofia_clear_pflag(profile, PFLAG_RTP_NOTIMER_DURING_BRIDGE);
						}
					} else if (!strcasecmp(var, "manual-redirect")) {
						if (switch_true(val)) {
							sofia_set_pflag(profile, PFLAG_MANUAL_REDIRECT);
						} else {
							sofia_clear_pflag(profile, PFLAG_MANUAL_REDIRECT);
						}
					} else if (!strcasecmp(var, "inbound-proxy-media")) {
						if (switch_true(val)) {
							sofia_set_flag(profile, TFLAG_PROXY_MEDIA);
						} else {
							sofia_clear_flag(profile, TFLAG_PROXY_MEDIA);
						}
					} else if (!strcasecmp(var, "inbound-zrtp-passthru")) {
						if (switch_true(val)) {
							sofia_set_flag(profile, TFLAG_ZRTP_PASSTHRU);
						} else {
							sofia_clear_flag(profile, TFLAG_ZRTP_PASSTHRU);
						}
					} else if (!strcasecmp(var, "force-subscription-expires") && !zstr(val)) {
						int tmp = atoi(val);
						if (tmp > 0) {
							profile->force_subscription_expires = tmp;
						}
					} else if (!strcasecmp(var, "force-publish-expires") && !zstr(val)) {
						int tmp = atoi(val);
						if (tmp > 0) {
							profile->force_publish_expires = tmp;
						}
					} else if (!strcasecmp(var, "send-message-query-on-register")) {
						if (switch_true(val)) {
							sofia_set_pflag(profile, PFLAG_MESSAGE_QUERY_ON_REGISTER);
						} else if (val && !strcasecmp(val, "first-only")) {
							sofia_clear_pflag(profile, PFLAG_MESSAGE_QUERY_ON_REGISTER);
							sofia_set_pflag(profile, PFLAG_MESSAGE_QUERY_ON_FIRST_REGISTER);
						} else {
							sofia_clear_pflag(profile, PFLAG_MESSAGE_QUERY_ON_REGISTER);
							sofia_clear_pflag(profile, PFLAG_MESSAGE_QUERY_ON_FIRST_REGISTER);
						}
					} else if (!strcasecmp(var, "inbound-reg-in-new-thread") && val) {
						if (switch_true(val)) {
							sofia_set_pflag(profile, PFLAG_THREAD_PER_REG);
						} else {
							sofia_clear_pflag(profile, PFLAG_THREAD_PER_REG);
						}
					} else if (!strcasecmp(var, "inbound-use-callid-as-uuid")) {
						if (switch_true(val)) {
							sofia_set_pflag(profile, PFLAG_CALLID_AS_UUID);
						} else {
							sofia_clear_pflag(profile, PFLAG_CALLID_AS_UUID);
						}
					} else if (!strcasecmp(var, "outbound-use-uuid-as-callid")) {
						if (switch_true(val)) {
							sofia_set_pflag(profile, PFLAG_UUID_AS_CALLID);
						} else {
							sofia_clear_pflag(profile, PFLAG_UUID_AS_CALLID);
						}
					} else if (!strcasecmp(var, "track-calls")) {
						if (switch_true(val)) {
							sofia_set_pflag(profile, PFLAG_TRACK_CALLS);
						}
					} else if (!strcasecmp(var, "NDLB-received-in-nat-reg-contact")) {
						if (switch_true(val)) {
							sofia_set_pflag(profile, PFLAG_RECIEVED_IN_NAT_REG_CONTACT);
						} else {
							sofia_clear_pflag(profile, PFLAG_RECIEVED_IN_NAT_REG_CONTACT);
						}
					} else if (!strcasecmp(var, "aggressive-nat-detection")) {
						if (switch_true(val)) {
							sofia_set_pflag(profile, PFLAG_AGGRESSIVE_NAT_DETECTION);
						} else {
							sofia_clear_pflag(profile, PFLAG_AGGRESSIVE_NAT_DETECTION);
						}
					} else if (!strcasecmp(var, "disable-rtp-auto-adjust")) {
						if (switch_true(val)) {
							sofia_set_media_flag(profile, SCMF_DISABLE_RTP_AUTOADJ);
						} else {
							sofia_clear_media_flag(profile, SCMF_DISABLE_RTP_AUTOADJ);
						}
					} else if (!strcasecmp(var, "NDLB-support-asterisk-missing-srtp-auth")) {
						if (switch_true(val)) {
							profile->mndlb |= SM_NDLB_DISABLE_SRTP_AUTH;
						} else {
							profile->mndlb &= ~SM_NDLB_DISABLE_SRTP_AUTH;
						}
					} else if (!strcasecmp(var, "user-agent-filter")) {
						profile->user_agent_filter = switch_core_strdup(profile->pool, val);
					} else if (!strcasecmp(var, "max-registrations-per-extension") && !zstr(val)) {
						profile->max_registrations_perext = atoi(val);
					} else if (!strcasecmp(var, "rfc2833-pt") && !zstr(val)) {
						profile->te = (switch_payload_t) atoi(val);
					} else if (!strcasecmp(var, "cng-pt") && !sofia_test_media_flag(profile, SCMF_SUPPRESS_CNG) && !zstr(val)) {
						profile->cng_pt = (switch_payload_t) atoi(val);
					} else if (!strcasecmp(var, "sip-port") && !zstr(val)) {
						if (!strcasecmp(val, "auto")) {
							sofia_set_pflag(profile, PFLAG_AUTO_ASSIGN_PORT);
						} else {
							profile->sip_port = (switch_port_t) atoi(val);
							if (!profile->extsipport) profile->extsipport = profile->sip_port;
						}
					} else if (!strcasecmp(var, "vad") && !zstr(val)) {
						if (!strcasecmp(val, "in")) {
							profile->vflags |= VAD_IN;
						} else if (!strcasecmp(val, "out")) {
							profile->vflags |= VAD_OUT;
						} else if (!strcasecmp(val, "both")) {
							profile->vflags |= VAD_OUT | VAD_IN;
						} else if (strcasecmp(val, "none")) {
							switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Invalid option %s for VAD\n", val);
						}
					} else if (!strcasecmp(var, "ext-rtp-ip")) {
						if (!zstr(val)) {
							char *ip = mod_sofia_globals.guess_ip;

							if (!strcmp(val, "0.0.0.0")) {
								switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "Invalid IP 0.0.0.0 replaced with %s\n",
												  mod_sofia_globals.guess_ip);
							} else if (!strcasecmp(val, "auto-nat")) {
								ip = NULL;
							} else {
								ip = strcasecmp(val, "auto") ? val : mod_sofia_globals.guess_ip;
								sofia_clear_pflag(profile, PFLAG_AUTO_NAT);
							}
							if (ip) {
								if (!strncasecmp(ip, "autonat:", 8)) {
									profile->extrtpip = switch_core_strdup(profile->pool, ip + 8);
									if (zstr(profile->extsipip)) {
										profile->extsipip = switch_core_strdup(profile->pool, profile->extrtpip);
									}
									sofia_set_pflag(profile, PFLAG_AUTO_NAT);
								} else {
									profile->extrtpip = switch_core_strdup(profile->pool, ip);
								}
							}
						} else {
							switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Invalid ext-rtp-ip\n");
						}
					} else if (!strcasecmp(var, "rtp-ip")) {
						char *ip = mod_sofia_globals.guess_ip;
						char buf[64];

						if (zstr(val)) {
							ip = mod_sofia_globals.guess_ip;
						} else if (!strcmp(val, "0.0.0.0")) {
							switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "Invalid IP 0.0.0.0 replaced with %s\n", mod_sofia_globals.guess_ip);
						} else if (!strncasecmp(val, "interface:", 10)) {
							char *ifname = val+10;
							int family = AF_UNSPEC;
							if (!strncasecmp(ifname, "auto/", 5)) { ifname += 5; family = AF_UNSPEC; }
							if (!strncasecmp(ifname, "ipv4/", 5)) { ifname += 5; family = AF_INET;   }
							if (!strncasecmp(ifname, "ipv6/", 5)) { ifname += 5; family = AF_INET6;  }
							if (switch_find_interface_ip(buf, sizeof(buf), NULL, ifname, family) == SWITCH_STATUS_SUCCESS) {
								ip = buf;
								switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Using %s IP for interface %s for rtp-ip\n", ip, val+10);
							} else {
								switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Unknown IP for interface %s for rtp-ip\n", val+10);
							}
						} else {
							ip = strcasecmp(val, "auto") ? val : mod_sofia_globals.guess_ip;
						}

						if (strchr(ip, ':')) {
							if (profile->rtpip_index < MAX_RTPIP) {
								profile->rtpip6[profile->rtpip_index6++] = switch_core_strdup(profile->pool, ip);
							} else {
								switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "Max IPs configured for profile %s.\n", profile->name);
							}
						} else {
							if (profile->rtpip_index6 < MAX_RTPIP) {
								profile->rtpip[profile->rtpip_index++] = switch_core_strdup(profile->pool, ip);
							} else {
								switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "Max IPs configured for profile %s.\n", profile->name);
							}
						}
					} else if (!strcasecmp(var, "sip-ip") && val) {
						char *ip = mod_sofia_globals.guess_ip;
						char buf[64];

						if (!strcmp(val, "0.0.0.0")) {
							switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "Invalid IP 0.0.0.0 replaced with %s\n", mod_sofia_globals.guess_ip);
						} else if (!strncasecmp(val, "interface:", 10)) {
							char *ifname = val+10;
							int family = AF_UNSPEC;
							if (!strncasecmp(ifname, "auto/", 5)) { ifname += 5; family = AF_UNSPEC; }
							if (!strncasecmp(ifname, "ipv4/", 5)) { ifname += 5; family = AF_INET;   }
							if (!strncasecmp(ifname, "ipv6/", 5)) { ifname += 5; family = AF_INET6;  }
							if (switch_find_interface_ip(buf, sizeof(buf), NULL, ifname, family) == SWITCH_STATUS_SUCCESS) {
								ip = buf;
								switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Using %s IP for interface %s for sip-ip\n", ip, val+10);
							} else {
								switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Unknown IP for interface %s for sip-ip\n", val+10);
							}
						} else {
							ip = strcasecmp(val, "auto") ? val : mod_sofia_globals.guess_ip;
						}
						profile->sipip = switch_core_strdup(profile->pool, ip);
					} else if (!strcasecmp(var, "ext-sip-port") && val) {
						int tmp = atoi(val);
						if (tmp > 0) profile->extsipport = (switch_port_t)tmp;
					} else if (!strcasecmp(var, "ext-sip-ip")) {
						if (!zstr(val)) {
							char *ip = mod_sofia_globals.guess_ip;
							char stun_ip[50] = "";
							char *myip = stun_ip;

							switch_copy_string(stun_ip, ip, sizeof(stun_ip));

							if (!strcasecmp(val, "0.0.0.0")) {
								switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "Invalid IP 0.0.0.0 replaced with %s\n",
												  mod_sofia_globals.guess_ip);
							} else if (!strcasecmp(val, "auto-nat")) {
								ip = NULL;
							} else if (strcasecmp(val, "auto")) {
								if (sofia_glue_ext_address_lookup(profile, &myip, &profile->extsipport, val, profile->pool) == SWITCH_STATUS_SUCCESS) {
									ip = myip;
									sofia_clear_pflag(profile, PFLAG_AUTO_NAT);
								} else {
									switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Failed to get external ip.\n");
								}
							}
							if (ip) {
								if (!strncasecmp(ip, "autonat:", 8)) {
									profile->extsipip = switch_core_strdup(profile->pool, ip + 8);
									sofia_set_pflag(profile, PFLAG_AUTO_NAT);
								} else {
									profile->extsipip = switch_core_strdup(profile->pool, ip);
								}
							}
						} else {
							switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Invalid ext-sip-ip\n");
						}
					} else if (!strcasecmp(var, "local-network-acl")) {
						if (val && !strcasecmp(val, "none")) {
							profile->local_network = NULL;
						} else {
							profile->local_network = switch_core_strdup(profile->pool, val);
						}
					} else if (!strcasecmp(var, "force-register-domain")) {
						profile->reg_domain = switch_core_strdup(profile->pool, val);
					} else if (!strcasecmp(var, "force-register-db-domain")) {
						profile->reg_db_domain = switch_core_strdup(profile->pool, val);
					} else if (!strcasecmp(var, "force-subscription-domain")) {
						profile->sub_domain = switch_core_strdup(profile->pool, val);
					} else if (!strcasecmp(var, "bind-params")) {
						profile->bind_params = switch_core_strdup(profile->pool, val);
					} else if (!strcasecmp(var, "sip-domain")) {
						profile->sipdomain = switch_core_strdup(profile->pool, val);
					} else if (!strcasecmp(var, "rtp-timer-name")) {
						profile->timer_name = switch_core_strdup(profile->pool, val);
					} else if (!strcasecmp(var, "hold-music")) {
						profile->hold_music = switch_core_strdup(profile->pool, val);
					} else if (!strcasecmp(var, "outbound-proxy") && val) {
						if (strncasecmp(val, "sip:", 4) && strncasecmp(val, "sips:", 5)) {
							profile->outbound_proxy = switch_core_sprintf(profile->pool, "sip:%s", val);
						} else {
							profile->outbound_proxy = switch_core_strdup(profile->pool, val);
						}
					} else if (!strcasecmp(var, "rtcp-audio-interval-msec")) {
						profile->rtcp_audio_interval_msec = switch_core_strdup(profile->pool, val);
					} else if (!strcasecmp(var, "rtcp-video-interval-msec")) {
						profile->rtcp_video_interval_msec = switch_core_strdup(profile->pool, val);
					} else if (!strcasecmp(var, "session-timeout") && !zstr(val)) {
						int v_session_timeout = atoi(val);
						if (v_session_timeout >= 0) {
							profile->session_timeout = v_session_timeout;
						}
					} else if (!strcasecmp(var, "max-proceeding") && !zstr(val)) {
						int v_max_proceeding = atoi(val);
						if (v_max_proceeding >= 0) {
							profile->max_proceeding = v_max_proceeding;
						}
					} else if (!strcasecmp(var, "rtp-timeout-sec") && !zstr(val)) {
						int v = atoi(val);
						if (v >= 0) {
							profile->rtp_timeout_sec = v;
						}
					} else if (!strcasecmp(var, "rtp-hold-timeout-sec") && !zstr(val)) {
						int v = atoi(val);
						if (v >= 0) {
							profile->rtp_hold_timeout_sec = v;
						}
					} else if (!strcasecmp(var, "disable-transfer")) {
						if (switch_true(val)) {
							profile->mflags &= ~MFLAG_REFER;
						} else {
							profile->mflags |= MFLAG_REFER;
						}
					} else if (!strcasecmp(var, "disable-register")) {
						if (switch_true(val)) {
							profile->mflags &= ~MFLAG_REGISTER;
						} else {
							profile->mflags |= MFLAG_REGISTER;
						}
					} else if (!strcasecmp(var, "media-option") && !zstr(val)) {
						if (!strcasecmp(val, "resume-media-on-hold")) {
							profile->media_options |= MEDIA_OPT_MEDIA_ON_HOLD;
						} else if (!strcasecmp(val, "bypass-media-after-att-xfer")) {
							profile->media_options |= MEDIA_OPT_BYPASS_AFTER_ATT_XFER;
						} else if (!strcasecmp(val, "bypass-media-after-hold")) {
							if (profile->media_options & MEDIA_OPT_MEDIA_ON_HOLD) {
								profile->media_options |= MEDIA_OPT_BYPASS_AFTER_HOLD;
							} else {
								switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR,
												  "bypass-media-after-hold can be set only with resume-media-on-hold media-option\n");
							}
						} else if (!strcasecmp(val, "none")) {
							profile->media_options = 0;
						}
					} else if (!strcasecmp(var, "pnp-provision-url")) {
						profile->pnp_prov_url = switch_core_strdup(profile->pool, val);
					} else if (!strcasecmp(var, "manage-presence")) {
						if (val && !strcasecmp(val, "passive")) {
							profile->pres_type = PRES_TYPE_PASSIVE;

						} else if (val && !strcasecmp(val, "pnp")) {
							profile->pres_type = PRES_TYPE_PNP;
						} else if (switch_true(val)) {
							profile->pres_type = PRES_TYPE_FULL;
						} else {
							profile->pres_type = 0;
						}
					} else if (!strcasecmp(var, "presence-hold-state")) {
						if (val && !strcasecmp(val, "confirmed")) {
							profile->pres_held_type = PRES_HELD_CONFIRMED;
						} else if (val && !strcasecmp(val, "terminated")) {
							profile->pres_held_type = PRES_HELD_TERMINATED;
						} else {
							profile->pres_held_type = 0;
						}
					} else if (!strcasecmp(var, "presence-privacy")) {
						if (switch_true(val)) {
							sofia_set_pflag(profile, PFLAG_PRESENCE_PRIVACY);
						} else {
							sofia_clear_pflag(profile, PFLAG_PRESENCE_PRIVACY);
						}
					} else if (!strcasecmp(var, "update-refresher")) {
						if (switch_true(val)) {
							sofia_set_pflag(profile, PFLAG_UPDATE_REFRESHER);
						} else {
							sofia_clear_pflag(profile, PFLAG_UPDATE_REFRESHER);
						}
					} else if (!strcasecmp(var, "manage-shared-appearance")) {
						if (switch_true(val)) {
							sofia_set_pflag(profile, PFLAG_MANAGE_SHARED_APPEARANCE);
							profile->pres_type = PRES_TYPE_FULL;
							sofia_set_pflag(profile, PFLAG_MULTIREG);

						} else if (val && !strcasecmp(val, "sylantro")) {
							switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR,
											  "Sylantro support has been removed.\n"
											  "It was incomplete anyway, and we fully support the broadsoft SCA shared line spec.");
							sofia_clear_pflag(profile, PFLAG_MANAGE_SHARED_APPEARANCE);
						} else {
							sofia_clear_pflag(profile, PFLAG_MANAGE_SHARED_APPEARANCE);
						}
					} else if (!strcasecmp(var, "disable-srv")) {
						if (switch_true(val)) {
							sofia_set_pflag(profile, PFLAG_DISABLE_SRV);
						} else {
							sofia_clear_pflag(profile, PFLAG_DISABLE_SRV);
						}
					} else if (!strcasecmp(var, "disable-naptr")) {
						if (switch_true(val)) {
							sofia_set_pflag(profile, PFLAG_DISABLE_NAPTR);
						} else {
							sofia_clear_pflag(profile, PFLAG_DISABLE_NAPTR);
						}
					} else if (!strcasecmp(var, "disable-srv503")) {
						if (switch_true(val)) {
							sofia_set_pflag(profile, PFLAG_DISABLE_SRV503);
						} else {
							sofia_clear_pflag(profile, PFLAG_DISABLE_SRV503);
						}
					} else if (!strcasecmp(var, "unregister-on-options-fail")) {
						if (switch_true(val)) {
							sofia_set_pflag(profile, PFLAG_UNREG_OPTIONS_FAIL);
						} else {
							sofia_clear_pflag(profile, PFLAG_UNREG_OPTIONS_FAIL);
						}
					} else if (!strcasecmp(var, "sip-user-ping-max") && !zstr(val)) {
						profile->sip_user_ping_max = atoi(val);
					} else if (!strcasecmp(var, "sip-user-ping-min") && !zstr(val)) {
						profile->sip_user_ping_min = atoi(val);
					} else if (!strcasecmp(var, "require-secure-rtp")) {
						if (switch_true(val)) {
							sofia_set_pflag(profile, PFLAG_SECURE);
						} else {
							sofia_clear_pflag(profile, PFLAG_SECURE);
						}
					} else if (!strcasecmp(var, "auto-invite-100")) {
						if (switch_true(val)) {
							sofia_set_pflag(profile, PFLAG_AUTO_INVITE_100);
						} else {
							sofia_clear_pflag(profile, PFLAG_AUTO_INVITE_100);
						}
					} else {
						found = 0;
					}
					if (found) continue;

					if (!strcasecmp(var, "multiple-registrations")) {
						if (val && !strcasecmp(val, "call-id")) {
							sofia_set_pflag(profile, PFLAG_MULTIREG);
						} else if (val && (!strcasecmp(val, "contact") || switch_true(val))) {
							sofia_set_pflag(profile, PFLAG_MULTIREG);
							sofia_set_pflag(profile, PFLAG_MULTIREG_CONTACT);
						} else if (!switch_true(val)) {
							sofia_clear_pflag(profile, PFLAG_MULTIREG);
							sofia_clear_pflag(profile, PFLAG_MULTIREG_CONTACT);
						}
					} else if (!strcasecmp(var, "supress-cng") || !strcasecmp(var, "suppress-cng")) {
						if (switch_true(val)) {
							sofia_set_media_flag(profile, SCMF_SUPPRESS_CNG);
							profile->cng_pt = 0;
						} else {
							sofia_clear_media_flag(profile, SCMF_SUPPRESS_CNG);
						}
					} else if (!strcasecmp(var, "NDLB-broken-auth-hash")) {
						if (switch_true(val)) {
							profile->ndlb |= PFLAG_NDLB_BROKEN_AUTH_HASH;
						} else {
							profile->ndlb &= ~PFLAG_NDLB_BROKEN_AUTH_HASH;
						}
					} else if (!strcasecmp(var, "NDLB-sendrecv-in-session")) {
						if (switch_true(val)) {
							profile->mndlb |= SM_NDLB_SENDRECV_IN_SESSION;
						} else {
							profile->mndlb &= ~SM_NDLB_SENDRECV_IN_SESSION;
						}
					} else if (!strcasecmp(var, "NDLB-allow-bad-iananame")) {
						if (switch_true(val)) {
							profile->mndlb |= SM_NDLB_ALLOW_BAD_IANANAME;
						} else {
							profile->mndlb &= ~SM_NDLB_ALLOW_BAD_IANANAME;
						}
					} else if (!strcasecmp(var, "NDLB-expires-in-register-response")) {
						if (switch_true(val)) {
							profile->ndlb |= PFLAG_NDLB_EXPIRES_IN_REGISTER_RESPONSE;
						} else {
							profile->ndlb &= ~PFLAG_NDLB_EXPIRES_IN_REGISTER_RESPONSE;
						}
					} else if (!strcasecmp(var, "NDLB-allow-crypto-in-avp")) {
						if (switch_true(val)) {
							profile->mndlb |= SM_NDLB_ALLOW_CRYPTO_IN_AVP;
						} else {
							profile->mndlb &= ~SM_NDLB_ALLOW_CRYPTO_IN_AVP;
						}
					} else if (!strcasecmp(var, "NDLB-allow-nondup-sdp")) {
						if (switch_true(val)) {
							profile->mndlb |= SM_NDLB_ALLOW_NONDUP_SDP;
						} else {
							profile->mndlb &= ~SM_NDLB_ALLOW_NONDUP_SDP;
						}
					} else if (!strcasecmp(var, "pass-rfc2833")) {
						if (switch_true(val)) {
							sofia_set_pflag(profile, PFLAG_PASS_RFC2833);
						} else {
							sofia_clear_pflag(profile, PFLAG_PASS_RFC2833);
						}
					} else if (!strcasecmp(var, "rtp-autofix-timing")) {
						if (switch_true(val)) {
							sofia_set_media_flag(profile, SCMF_AUTOFIX_TIMING);
						} else {
							sofia_clear_media_flag(profile, SCMF_AUTOFIX_TIMING);
						}
					} else if (!strcasecmp(var, "contact-user")) {
						profile->contact_user = switch_core_strdup(profile->pool, val);
					} else if (!strcasecmp(var, "nat-options-ping")) {
						if (val && !strcasecmp(val, "udp-only")) {
							sofia_set_pflag(profile, PFLAG_UDP_NAT_OPTIONS_PING);
						} else if (switch_true(val)) {
							sofia_set_pflag(profile, PFLAG_NAT_OPTIONS_PING);
						} else {
							sofia_clear_pflag(profile, PFLAG_NAT_OPTIONS_PING);
							sofia_clear_pflag(profile, PFLAG_UDP_NAT_OPTIONS_PING);
						}
					} else if (!strcasecmp(var, "all-reg-options-ping")) {
						if (switch_true(val)) {
							sofia_set_pflag(profile, PFLAG_ALL_REG_OPTIONS_PING);
						} else {
							sofia_clear_pflag(profile, PFLAG_ALL_REG_OPTIONS_PING);
						}
					} else if (!strcasecmp(var, "inbound-codec-negotiation")) {
						if (val && !strcasecmp(val, "greedy")) {
							sofia_set_media_flag(profile, SCMF_CODEC_GREEDY);
						} else if (val && !strcasecmp(val, "scrooge")) {
							sofia_set_media_flag(profile, SCMF_CODEC_GREEDY);
							sofia_set_media_flag(profile, SCMF_CODEC_SCROOGE);
						} else {
							sofia_clear_media_flag(profile, SCMF_CODEC_SCROOGE);
							sofia_clear_media_flag(profile, SCMF_CODEC_GREEDY);
						}
					} else if (!strcasecmp(var, "disable-transcoding")) {
						if (switch_true(val)) {
							sofia_set_media_flag(profile, SCMF_DISABLE_TRANSCODING);
						} else {
							sofia_clear_media_flag(profile, SCMF_DISABLE_TRANSCODING);
						}
					} else if (!strcasecmp(var, "rtp-rewrite-timestamps")) {
						if (switch_true(val)) {
							sofia_set_media_flag(profile, SCMF_REWRITE_TIMESTAMPS);
						} else {
							sofia_clear_media_flag(profile, SCMF_REWRITE_TIMESTAMPS);
						}
					} else if (!strcasecmp(var, "auth-calls")) {
						if (switch_true(val)) {
							sofia_set_pflag(profile, PFLAG_AUTH_CALLS);
						} else {
							sofia_clear_pflag(profile, PFLAG_AUTH_CALLS);
						}
					} else if (!strcasecmp(var, "auth-messages")) {
						if (switch_true(val)) {
							sofia_set_pflag(profile, PFLAG_AUTH_MESSAGES);
						} else {
							sofia_clear_pflag(profile, PFLAG_AUTH_MESSAGES);
						}
					} else if (!strcasecmp(var, "auth-subscriptions")) {
						if (switch_true(val)) {
							sofia_set_pflag(profile, PFLAG_AUTH_SUBSCRIPTIONS);
						} else {
							sofia_clear_pflag(profile, PFLAG_AUTH_SUBSCRIPTIONS);
						}
					} else if (!strcasecmp(var, "extended-info-parsing")) {
						if (switch_true(val)) {
							sofia_set_pflag(profile, PFLAG_EXTENDED_INFO_PARSING);
						} else {
							sofia_clear_pflag(profile, PFLAG_EXTENDED_INFO_PARSING);
						}
					} else if (!strcasecmp(var, "nonce-ttl") && !zstr(val)) {
						profile->nonce_ttl = atoi(val);
					} else if (!strcasecmp(var, "max-auth-validity") && !zstr(val)) {
						profile->max_auth_validity = atoi(val);
					} else if (!strcasecmp(var, "auth-require-user")) {
						if (switch_true(val)) {
							sofia_set_pflag(profile, PFLAG_AUTH_REQUIRE_USER);
						} else {
							sofia_clear_pflag(profile, PFLAG_AUTH_REQUIRE_USER);
						}
					} else if (!strcasecmp(var, "accept-blind-reg")) {
						if (switch_true(val)) {
							sofia_set_pflag(profile, PFLAG_BLIND_REG);
						} else {
							sofia_clear_pflag(profile, PFLAG_BLIND_REG);
						}
					} else if (!strcasecmp(var, "3pcc-reinvite-bridged-on-ack")) {
						if (switch_true(val)) {
							sofia_set_pflag(profile, PFLAG_3PCC_REINVITE_BRIDGED_ON_ACK);
						} else {
							sofia_clear_pflag(profile, PFLAG_3PCC_REINVITE_BRIDGED_ON_ACK);
						}
					} else if (!strcasecmp(var, "enable-3pcc")) {
						if (switch_true(val)) {
							sofia_set_pflag(profile, PFLAG_3PCC);
						} else if (!strcasecmp(val, "proxy")) {
							sofia_set_pflag(profile, PFLAG_3PCC_PROXY);
						}
					} else if (!strcasecmp(var, "accept-blind-auth")) {
						if (switch_true(val)) {
							sofia_set_pflag(profile, PFLAG_BLIND_AUTH);
						} else {
							sofia_clear_pflag(profile, PFLAG_BLIND_AUTH);
						}
					} else if (!strcasecmp(var, "auth-all-packets")) {
						if (switch_true(val)) {
							sofia_set_pflag(profile, PFLAG_AUTH_ALL);
						} else {
							sofia_clear_pflag(profile, PFLAG_AUTH_ALL);
						}
					} else if (!strcasecmp(var, "full-id-in-dialplan")) {
						if (switch_true(val)) {
							sofia_set_pflag(profile, PFLAG_FULL_ID);
						} else {
							sofia_clear_pflag(profile, PFLAG_FULL_ID);
						}
					} else if (!strcasecmp(var, "inbound-reg-force-matching-username")) {
						if (switch_true(val)) {
							sofia_set_pflag(profile, PFLAG_CHECKUSER);
						} else {
							sofia_clear_pflag(profile, PFLAG_CHECKUSER);
						}
					} else if (!strcasecmp(var, "enable-timer")) {
						if (!switch_true(val)) {
							sofia_set_pflag(profile, PFLAG_DISABLE_TIMER);
						} else {
							sofia_clear_pflag(profile, PFLAG_DISABLE_TIMER);
						}
					} else if (!strcasecmp(var, "enable-rfc-5626")) {
						if (switch_true(val)) {
							sofia_set_pflag(profile, PFLAG_ENABLE_RFC5626);
						} else {
							sofia_clear_pflag(profile, PFLAG_ENABLE_RFC5626);
						}
					} else if (!strcasecmp(var, "minimum-session-expires") && !zstr(val)) {
						profile->minimum_session_expires = atoi(val);
						/* per RFC 4028: minimum_session_expires must be > 90 */
						if (profile->minimum_session_expires < 90) {
							profile->minimum_session_expires = 90;
						}
					} else if (!strcasecmp(var, "enable-100rel")) {
						if (switch_true(val)) {
							sofia_clear_pflag(profile, PFLAG_DISABLE_100REL);
						} else {
							sofia_set_pflag(profile, PFLAG_DISABLE_100REL);
						}
					} else if (!strcasecmp(var, "enable-compact-headers")) {
						if (switch_true(val)) {
							sofia_set_pflag(profile, PFLAG_SIPCOMPACT);
						} else {
							sofia_clear_pflag(profile, PFLAG_SIPCOMPACT);
						}
					} else if (!strcasecmp(var, "pass-callee-id")) {
						if (switch_true(val)) {
							sofia_set_pflag(profile, PFLAG_PASS_CALLEE_ID);
						} else {
							sofia_clear_pflag(profile, PFLAG_PASS_CALLEE_ID);
						}
					} else if (!strcasecmp(var, "enable-soa")) {
						if (switch_true(val)) {
							sofia_set_flag(profile, TFLAG_ENABLE_SOA);
						} else {
							sofia_clear_flag(profile, TFLAG_ENABLE_SOA);
						}
					} else if (!strcasecmp(var, "parse-all-invite-headers")) {
						if (switch_true(val)) {
							sofia_set_pflag(profile, PFLAG_PARSE_ALL_INVITE_HEADERS);
						} else {
							sofia_clear_pflag(profile, PFLAG_PARSE_ALL_INVITE_HEADERS);
						}
					} else if (!strcasecmp(var, "bitpacking")) {
						if (val && !strcasecmp(val, "aal2")) {
							profile->codec_flags = SWITCH_CODEC_FLAG_AAL2;
						} else {
							profile->codec_flags = 0;
						}
					} else if (!strcasecmp(var, "username")) {
						profile->sdp_username = switch_core_strdup(profile->pool, val);
					} else if (!strcasecmp(var, "context")) {
						profile->context = switch_core_strdup(profile->pool, val);
					} else if (!strcasecmp(var, "apply-nat-acl") && !zstr(val)) {
						if (!strcasecmp(val,"none")) {
							profile->nat_acl_count = 0;
						} else if (profile->nat_acl_count < SOFIA_MAX_ACL) {
							if (!profile->extsipip && profile->sipip && switch_check_network_list_ip(profile->sipip, val)) {
								switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Not adding acl %s because it's the local network\n", val);
							} else {
								profile->nat_acl[profile->nat_acl_count++] = switch_core_strdup(profile->pool, val);
							}
						} else {
							switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Max acl records of %d reached\n", SOFIA_MAX_ACL);
						}
					} else if (!strcasecmp(var, "apply-inbound-acl") && !zstr(val)) {
						if (!strcasecmp(val,"none")) {
							profile->acl_count = 0;
						} else if (profile->acl_count < SOFIA_MAX_ACL) {
							char *list, *pass = NULL, *fail = NULL;

							list = switch_core_strdup(profile->pool, val);

							if ((pass = strchr(list, ':'))) {
								*pass++ = '\0';
								if ((fail = strchr(pass, ':'))) {
									*fail++ = '\0';
								}

								if (zstr(pass)) pass = NULL;
								if (zstr(fail)) fail = NULL;

								profile->acl_pass_context[profile->acl_count] = pass;
								profile->acl_fail_context[profile->acl_count] = fail;
							}

							profile->acl[profile->acl_count++] = list;

						} else {
							switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Max acl records of %d reached\n", SOFIA_MAX_ACL);
						}
					} else if (!strcasecmp(var, "apply-proxy-acl") && !zstr(val)) {
						if (!strcasecmp(val,"none")) {
							profile->proxy_acl_count = 0;
						} else if (profile->proxy_acl_count < SOFIA_MAX_ACL) {
							profile->proxy_acl[profile->proxy_acl_count++] = switch_core_strdup(profile->pool, val);
						} else {
							switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Max acl records of %d reached\n", SOFIA_MAX_ACL);
						}
					} else if (!strcasecmp(var, "apply-register-acl") && !zstr(val)) {
						if (!strcasecmp(val,"none")) {
							profile->reg_acl_count = 0;
						} else if (profile->reg_acl_count < SOFIA_MAX_ACL) {
							profile->reg_acl[profile->reg_acl_count++] = switch_core_strdup(profile->pool, val);
						} else {
							switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Max acl records of %d reached\n", SOFIA_MAX_ACL);
						}

					} else if (!strcasecmp(var, "apply-candidate-acl") && !zstr(val)) {
						if (!strcasecmp(val,"none")) {
							profile->cand_acl_count = 0;
						} else if (profile->cand_acl_count < SWITCH_MAX_CAND_ACL) {
							profile->cand_acl[profile->cand_acl_count++] = switch_core_strdup(profile->pool, val);
						} else {
							switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Max acl records of %d reached\n", SWITCH_MAX_CAND_ACL);
						}
					} else if (!strcasecmp(var, "alias")) {
						sip_alias_node_t *node;
						if (zstr(val)) {
							switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Alias Param has no data...\n");
						} else {
							if ((node = switch_core_alloc(profile->pool, sizeof(*node)))) {
								if ((node->url = switch_core_strdup(profile->pool, val))) {
									node->next = profile->aliases;
									profile->aliases = node;
								}
							}
						}
					} else if (!strcasecmp(var, "dialplan")) {
						profile->dialplan = switch_core_strdup(profile->pool, val);
					} else if (!strcasecmp(var, "max-calls") && !zstr(val)) {
						profile->max_calls = atoi(val);
					} else if (!strcasecmp(var, "codec-prefs")) {
						profile->inbound_codec_string = switch_core_strdup(profile->pool, val);
						profile->outbound_codec_string = switch_core_strdup(profile->pool, val);
					} else if (!strcasecmp(var, "inbound-codec-prefs")) {
						profile->inbound_codec_string = switch_core_strdup(profile->pool, val);
					} else if (!strcasecmp(var, "outbound-codec-prefs")) {
						profile->outbound_codec_string = switch_core_strdup(profile->pool, val);
					} else if (!strcasecmp(var, "challenge-realm")) {
						profile->challenge_realm = switch_core_strdup(profile->pool, val);
					} else if (!strcasecmp(var, "dtmf-duration") && !zstr(val)) {
						uint32_t dur = atoi(val);
						if (dur >= switch_core_min_dtmf_duration(0) && dur <= switch_core_max_dtmf_duration(0)) {
							profile->dtmf_duration = dur;
						} else {
							profile->dtmf_duration = SWITCH_DEFAULT_DTMF_DURATION;
							switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Duration out of bounds, using default of %d!\n",
											  SWITCH_DEFAULT_DTMF_DURATION);
						}

					} else if (!strcasecmp(var, "ws-binding") && !zstr(val)) {
						int tmp;
						char *p;

						profile->ws_ip = switch_core_strdup(profile->pool, val);
						if ((p = strrchr(profile->ws_ip, ':'))) {
							*p++ = '\0';

							if (p && (tmp = atol(p)) && tmp > 0) {
								profile->ws_port = (switch_port_t) tmp;
							}
						}

					} else if (!strcasecmp(var, "wss-binding") && !zstr(val)) {
						int tmp;
						char *p;

						profile->wss_ip = switch_core_strdup(profile->pool, val);
						if ((p = strrchr(profile->wss_ip, ':'))) {
							*p++ = '\0';

							if (p && (tmp = atol(p)) && tmp > 0) {
								profile->wss_port = (switch_port_t) tmp;
							}
						}

						/*
						 * handle TLS params #1
						 */
					} else if (!strcasecmp(var, "tls")) {
						if (switch_true(val)) {
							sofia_set_pflag(profile, PFLAG_TLS);
							if (profile->tls_bind_params) {
								if (!switch_stristr("transport=tls", profile->tls_bind_params)) {
									profile->tls_bind_params = switch_core_sprintf(profile->pool, "%s;transport=tls", profile->tls_bind_params);
								}
							} else {
								profile->tls_bind_params = switch_core_strdup(profile->pool, "transport=tls");
							}
						} else {
							sofia_clear_pflag(profile, PFLAG_TLS);
						}
					} else if (!strcasecmp(var, "tls-bind-params")) {
						if (val && switch_stristr("transport=tls", val)) {
							profile->tls_bind_params = switch_core_strdup(profile->pool, val);
						} else {
							profile->tls_bind_params = switch_core_sprintf(profile->pool, "%s;transport=tls", val);
						}
					} else if (!strcasecmp(var, "tls-only")) {
						profile->tls_only = switch_true(val);
					} else if (!strcasecmp(var, "tls-verify-date")) {
						profile->tls_verify_date = switch_true(val);
					} else if (!strcasecmp(var, "tls-verify-depth") && !zstr(val)) {
						profile->tls_verify_depth = atoi(val);
					} else if (!strcasecmp(var, "tls-verify-policy")) {
						profile->tls_verify_policy = sofia_glue_str2tls_verify_policy(val);
					} else if (!strcasecmp(var, "tls-sip-port") && !zstr(val)) {
						if (!strcasecmp(val, "auto")) {
							sofia_set_pflag(profile, PFLAG_AUTO_ASSIGN_TLS_PORT);
						} else {
							sofia_clear_pflag(profile, PFLAG_AUTO_ASSIGN_TLS_PORT);
							profile->tls_sip_port = (switch_port_t) atoi(val);
						}
					} else if (!strcasecmp(var, "tls-cert-dir") && !zstr(val)) {
						profile->tls_cert_dir = switch_core_strdup(profile->pool, val);
					} else if (!strcasecmp(var, "tls-passphrase") && !zstr(val)) {
						profile->tls_passphrase = switch_core_strdup(profile->pool, val);
					} else if (!strcasecmp(var, "tls-verify-in-subjects") && !zstr(val)) {
						profile->tls_verify_in_subjects_str = switch_core_strdup(profile->pool, val);
					} else if (!strcasecmp(var, "tls-ciphers") && !zstr(val)) {
						profile->tls_ciphers = switch_core_strdup(profile->pool, val);
					} else if (!strcasecmp(var, "tls-version") && !zstr(val)) {
						char *ps = val, *pe = val;
						profile->tls_version = 0;
						while (ps && *pe) {
							int n;
							pe = strchr(ps,',');
							if (!pe && !(pe = memchr(ps,0,1024))) break;
							n = (int)(pe-ps);
							if (n==5 && !strncasecmp(ps, "sslv2", n))
								profile->tls_version |= SOFIA_TLS_VERSION_SSLv2;
							if (n==5 && !strncasecmp(ps, "sslv3", n))
								profile->tls_version |= SOFIA_TLS_VERSION_SSLv3;
							if (n==6 && !strncasecmp(ps, "sslv23", n))
								profile->tls_version |= SOFIA_TLS_VERSION_SSLv2 | SOFIA_TLS_VERSION_SSLv3;
							if (n==5 && !strncasecmp(ps, "tlsv1", n))
								profile->tls_version |= SOFIA_TLS_VERSION_TLSv1;
							if (n==7 && !strncasecmp(ps, "tlsv1.1", n))
								profile->tls_version |= SOFIA_TLS_VERSION_TLSv1_1;
							if (n==7 && !strncasecmp(ps, "tlsv1.2", n))
								profile->tls_version |= SOFIA_TLS_VERSION_TLSv1_2;
							ps=pe+1;
						}
					} else if (!strcasecmp(var, "tls-timeout") && !zstr(val)) {
						int v = atoi(val);
						profile->tls_timeout = v > 0 ? (unsigned int)v : 300;
					} else if (!strcasecmp(var, "timer-T1") && !zstr(val)) {
						int v = atoi(val);
						if (v > 0) {
							profile->timer_t1 = v;
						} else {
							profile->timer_t1 = 500;
						}
					} else if (!strcasecmp(var, "timer-T1X64") && !zstr(val)) {
						int v = atoi(val);
						if (v > 0) {
							profile->timer_t1x64 = v;
						} else {
							profile->timer_t1x64 = 32000;
						}
					} else if (!strcasecmp(var, "timer-T2") && !zstr(val)) {
						int v = atoi(val);
						if (v > 0) {
							profile->timer_t2 = v;
						} else {
							profile->timer_t2 = 4000;
						}
					} else if (!strcasecmp(var, "timer-T4") && !zstr(val)) {
						int v = atoi(val);
						if (v > 0) {
							profile->timer_t4 = v;
						} else {
							profile->timer_t4 = 4000;
						}
					} else if (!strcasecmp(var, "sip-options-respond-503-on-busy")) {
						if (switch_true(val)) {
							sofia_set_pflag(profile, PFLAG_OPTIONS_RESPOND_503_ON_BUSY);
						} else {
							sofia_clear_pflag(profile, PFLAG_OPTIONS_RESPOND_503_ON_BUSY);
						}
					} else if (!strcasecmp(var, "sip-expires-late-margin") && !zstr(val)) {
						int32_t sip_expires_late_margin = atoi(val);
						if (sip_expires_late_margin >= 0) {
							profile->sip_expires_late_margin = sip_expires_late_margin;
						} else {
							profile->sip_expires_late_margin = 60;
						}
					} else if (!strcasecmp(var, "sip-force-expires-min") && !zstr(val)) {
						int32_t sip_force_expires_min = atoi(val);
						if (sip_force_expires_min >= 0) {
							profile->sip_force_expires_min = sip_force_expires_min;
						} else {
							profile->sip_force_expires_min = 0;
						}
					} else if (!strcasecmp(var, "sip-force-expires-max") && !zstr(val)) {
						int32_t sip_force_expires_max = atoi(val);
						if (sip_force_expires_max >= 0) {
							profile->sip_force_expires_max = sip_force_expires_max;
						} else {
							profile->sip_force_expires_max = 0;
						}
					} else if (!strcasecmp(var, "sip-force-expires") && !zstr(val)) {
						int32_t sip_force_expires = atoi(val);
						if (sip_force_expires >= 0) {
							profile->sip_force_expires = sip_force_expires;
						} else {
							profile->sip_force_expires = 0;
						}
					} else if (!strcasecmp(var, "sip-expires-max-deviation") && !zstr(val)) {
						int32_t sip_expires_max_deviation = atoi(val);
						if (sip_expires_max_deviation >= 0) {
							profile->sip_expires_max_deviation = sip_expires_max_deviation;
						} else {
							profile->sip_expires_max_deviation = 0;
						}
					} else if (!strcasecmp(var, "sip-subscription-max-deviation") && !zstr(val)) {
						int32_t sip_subscription_max_deviation = atoi(val);
						if (sip_subscription_max_deviation >= 0) {
							profile->sip_subscription_max_deviation = sip_subscription_max_deviation;
						} else {
							profile->sip_subscription_max_deviation = 0;
						}
					} else if (!strcasecmp(var, "reuse-connections")) {
						switch_bool_t value = switch_true(val);
						if (!value) {
							sofia_set_pflag(profile, PFLAG_NO_CONNECTION_REUSE);
						} else {
							sofia_clear_pflag(profile, PFLAG_NO_CONNECTION_REUSE);
						}
					} else if (!strcasecmp(var, "p-asserted-id-parse")) {
						if (!val) {
							profile->paid_type = PAID_DEFAULT;
						} else 	if (!strncasecmp(val, "default", 7)) {
							profile->paid_type = PAID_DEFAULT;
						} else if (!strncasecmp(val, "user-only", 9)) {
							profile->paid_type = PAID_USER;
						} else if (!strncasecmp(val, "user-domain", 11)) {
							profile->paid_type = PAID_USER_DOMAIN;
						} else if (!strncasecmp(val, "verbatim", 8)) {
							profile->paid_type = PAID_VERBATIM;
						} else {
							profile->paid_type = PAID_DEFAULT;
						}
					} else if (!strcasecmp(var, "channel-xml-fetch-on-nightmare-transfer")) {
						if(switch_true(val)) {
							sofia_set_pflag(profile, PFLAG_CHANNEL_XML_FETCH_ON_NIGHTMARE_TRANSFER);
						}  else {
							sofia_clear_pflag(profile, PFLAG_CHANNEL_XML_FETCH_ON_NIGHTMARE_TRANSFER);
						}
					} else if (!strcasecmp(var, "make-every-transfer-a-nightmare")) {
						if(switch_true(val)) {
							sofia_set_pflag(profile, PFLAG_MAKE_EVERY_TRANSFER_A_NIGHTMARE);
						}  else {
							sofia_clear_pflag(profile, PFLAG_MAKE_EVERY_TRANSFER_A_NIGHTMARE);
						}
					} else if (!strcasecmp(var, "fire-transfer-events")) {
						if(switch_true(val)) {
							sofia_set_pflag(profile, PFLAG_FIRE_TRANFER_EVENTS);
						}  else {
							sofia_clear_pflag(profile, PFLAG_FIRE_TRANFER_EVENTS);
						}
					} else if (!strcasecmp(var, "enforce-blind-auth-result")) {
						if(switch_true(val)) {
							sofia_set_pflag(profile, PFLAG_BLIND_AUTH_ENFORCE_RESULT);
						}  else {
							sofia_clear_pflag(profile, PFLAG_BLIND_AUTH_ENFORCE_RESULT);
						}
					} else if (!strcasecmp(var, "blind-auth-reply-403")) {
						if(switch_true(val)) {
							sofia_set_pflag(profile, PFLAG_BLIND_AUTH_REPLY_403);
						}  else {
							sofia_clear_pflag(profile, PFLAG_BLIND_AUTH_REPLY_403);
						}
					} else if (!strcasecmp(var, "auth-calls-acl-only")) {
						if(switch_true(val)) {
							sofia_set_pflag(profile, PFLAG_AUTH_CALLS_ACL_ONLY);
						}  else {
							sofia_clear_pflag(profile, PFLAG_AUTH_CALLS_ACL_ONLY);
						}
					} else if (!strcasecmp(var, "use-port-for-acl-check")) {
						if(switch_true(val)) {
							sofia_set_pflag(profile, PFLAG_USE_PORT_FOR_ACL_CHECK);
						}  else {
							sofia_clear_pflag(profile, PFLAG_USE_PORT_FOR_ACL_CHECK);
						}
					} else if (!strcasecmp(var, "apply-inbound-acl-x-token")) {
						profile->acl_inbound_x_token_header = switch_core_strdup(profile->pool, val);
					} else if (!strcasecmp(var, "apply-proxy-acl-x-token")) {
						profile->acl_proxy_x_token_header = switch_core_strdup(profile->pool, val);
					} else if (!strcasecmp(var, "proxy-hold")) {
						if(switch_true(val)) {
							sofia_set_pflag(profile, PFLAG_PROXY_HOLD);
						}  else {
							sofia_clear_pflag(profile, PFLAG_PROXY_HOLD);
						}
					} else if (!strcasecmp(var, "proxy-info")) {
						if(switch_true(val)) {
							sofia_set_pflag(profile, PFLAG_PROXY_INFO);
						}  else {
							sofia_clear_pflag(profile, PFLAG_PROXY_INFO);
						}
					} else if (!strcasecmp(var, "proxy-message")) {
						if(switch_true(val)) {
							sofia_set_pflag(profile, PFLAG_PROXY_MESSAGE);
						}  else {
							sofia_clear_pflag(profile, PFLAG_PROXY_MESSAGE);
						}
					} else if (!strcasecmp(var, "proxy-notify-events")) {
						profile->proxy_notify_events = switch_core_strdup(profile->pool, val);
					} else if (!strcasecmp(var, "proxy-info-content-types")) {
						profile->proxy_info_content_types = switch_core_strdup(profile->pool, val);
					}
				}

				if (sofia_test_flag(profile, TFLAG_ZRTP_PASSTHRU) && !sofia_test_flag(profile, TFLAG_LATE_NEGOTIATION)) {
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "ZRTP passthrough implictly enables inbound-late-negotiation\n");
					sofia_set_flag(profile, TFLAG_LATE_NEGOTIATION);
				}

				if (sofia_test_flag(profile, TFLAG_INB_NOMEDIA) && !sofia_test_flag(profile, TFLAG_LATE_NEGOTIATION)) {
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "inbound-bypass-media  implictly enables inbound-late-negotiation\n");
					sofia_set_flag(profile, TFLAG_LATE_NEGOTIATION);
				}

				if (sofia_test_pflag(profile, PFLAG_SEND_DISPLAY_UPDATE) && !sofia_test_pflag(profile, PFLAG_ALLOW_UPDATE)) {
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "send-display-update=true is set, but we can't comply because allow-update=false\n");
					sofia_clear_pflag(profile, PFLAG_SEND_DISPLAY_UPDATE);
				}
				if (sofia_test_pflag(profile, PFLAG_PROXY_HOLD)) {
					if (profile->media_options & MEDIA_OPT_MEDIA_ON_HOLD) {
						profile->media_options &= ~MEDIA_OPT_MEDIA_ON_HOLD;
						switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "proxy-hold=true is set, incompatible with media-option=resume-media-on-hold\n");
					}
					if (profile->media_options & MEDIA_OPT_BYPASS_AFTER_HOLD) {
						profile->media_options &= ~MEDIA_OPT_BYPASS_AFTER_HOLD;
						switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "proxy-hold=true is set, incompatible with media-option=bypass-media-after-hold\n");
					}
				}

				if ((!profile->cng_pt) && (!sofia_test_media_flag(profile, SCMF_SUPPRESS_CNG))) {
					profile->cng_pt = SWITCH_RTP_CNG_PAYLOAD;
				}

				if (!profile->sipip) {
					profile->sipip = switch_core_strdup(profile->pool, mod_sofia_globals.guess_ip);
				}

				if (!profile->rtpip[0] && !profile->rtpip6[0]) {
					profile->rtpip[profile->rtpip_index++] = switch_core_strdup(profile->pool, mod_sofia_globals.guess_ip);
				}

				if (switch_nat_get_type()) {
					char *ip = switch_core_get_variable_dup("nat_public_addr");
					if (ip && !strchr(profile->sipip, ':')) {
						if (!profile->extrtpip) {
							profile->extrtpip = switch_core_strdup(profile->pool, ip);
						}
						if (!profile->extsipip) {
							profile->extsipip = switch_core_strdup(profile->pool, ip);
						}
						sofia_set_pflag(profile, PFLAG_AUTO_NAT);
						switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "NAT detected setting external ip to %s\n", ip);
					}
					switch_safe_free(ip);
				}

				if (profile->nonce_ttl < 60) {
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "Setting nonce TTL to 60 seconds\n");
					profile->nonce_ttl = 60;
				}

				if (!profile->max_auth_validity) {
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "Setting MAX Auth Validity to 0 Attempts\n");
					profile->max_auth_validity = 0;
				}

				if (!profile->sdp_username) {
					profile->sdp_username = switch_core_strdup(profile->pool, "FreeSWITCH");
				}

				if (!profile->rtpip[0] && !profile->rtpip6[0]) {
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "Setting ip to '127.0.0.1'\n");
					profile->rtpip[profile->rtpip_index++] = switch_core_strdup(profile->pool, "127.0.0.1");
				}

				if (!profile->sip_port && !sofia_test_pflag(profile, PFLAG_AUTO_ASSIGN_PORT)) {
					profile->sip_port = (switch_port_t) atoi(SOFIA_DEFAULT_PORT);
					if (!profile->extsipport) profile->extsipport = profile->sip_port;
				}

				if (!profile->dialplan) {
					profile->dialplan = switch_core_strdup(profile->pool, "XML");
				}

				if (!profile->context) {
					profile->context = switch_core_strdup(profile->pool, "default");
				}

				if (!profile->sipdomain) {
					profile->sipdomain = switch_core_strdup(profile->pool, profile->sipip);
				}

				if (profile->pres_type == PRES_TYPE_PNP) {
					if (!profile->pnp_prov_url) {
						profile->pnp_prov_url = switch_core_sprintf(profile->pool, "http://%s/provision/", mod_sofia_globals.guess_ip);
					}

					if (!profile->pnp_notify_profile) {
						profile->pnp_notify_profile = switch_core_strdup(profile->pool, mod_sofia_globals.guess_ip);
					}

					if (!profile->extsipip) {
						profile->extsipip = switch_core_strdup(profile->pool, mod_sofia_globals.guess_ip);
					}

					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "we're configured to provision to [%s] on profile [%s]\n",
									  profile->pnp_prov_url, profile->pnp_notify_profile);
				}

				config_sofia_profile_urls(profile);

				if (profile->tls_cert_dir) {
					if (profile->wss_ip) {
						char *key, *cert;
						key  = switch_core_sprintf(profile->pool, "%s/%s", profile->tls_cert_dir, "wss.key");
						if (switch_file_exists(key, profile->pool) != SWITCH_STATUS_SUCCESS) key = NULL;
						cert = switch_core_sprintf(profile->pool, "%s/%s", profile->tls_cert_dir, "wss.crt");
						if (switch_file_exists(cert, profile->pool) != SWITCH_STATUS_SUCCESS) cert = NULL;
						if ( !key || !cert) {
							key  = switch_core_sprintf(profile->pool, "%s/%s", profile->tls_cert_dir, "wss.pem");
							if ( switch_file_exists(key, profile->pool) != SWITCH_STATUS_SUCCESS ) {
								switch_core_gen_certs(key);
							}
						}
					}
					if (sofia_test_pflag(profile, PFLAG_TLS)) {
						char *key = switch_core_sprintf(profile->pool, "%s/%s", profile->tls_cert_dir, "agent.pem");
						char *ca  =  switch_core_sprintf(profile->pool, "%s/%s", profile->tls_cert_dir, "cafile.pem");;
						if (switch_file_exists(key, profile->pool) != SWITCH_STATUS_SUCCESS) key = NULL;
						if (switch_file_exists(ca, profile->pool) != SWITCH_STATUS_SUCCESS) ca = NULL;
						if ( !key || !ca ) {
							key  = switch_core_sprintf(profile->pool, "%s/%s", profile->tls_cert_dir, "tls.pem");
							if ( switch_file_exists(key, profile->pool) != SWITCH_STATUS_SUCCESS ) {
								switch_core_gen_certs(key);
							}
						}
					}
				}
			}

			if (profile) {
				if (profile_already_started) {
					switch_xml_t gateways_tag, domain_tag, domains_tag, aliases_tag, alias_tag;

					if (sofia_test_flag(profile, TFLAG_ZRTP_PASSTHRU)) {
						sofia_set_flag(profile, TFLAG_LATE_NEGOTIATION);
					}

					if ((gateways_tag = switch_xml_child(xprofile, "gateways"))) {
						parse_gateways(profile, gateways_tag, NULL);
					}

					status = SWITCH_STATUS_SUCCESS;

					if ((domains_tag = switch_xml_child(xprofile, "domains"))) {
						switch_event_t *xml_params;
						switch_event_create(&xml_params, SWITCH_EVENT_REQUEST_PARAMS);
						switch_assert(xml_params);
						switch_event_add_header_string(xml_params, SWITCH_STACK_BOTTOM, "purpose", "gateways");
						switch_event_add_header_string(xml_params, SWITCH_STACK_BOTTOM, "profile", profile->name);

						for (domain_tag = switch_xml_child(domains_tag, "domain"); domain_tag; domain_tag = domain_tag->next) {
							switch_xml_t droot, x_domain_tag;
							const char *dname = switch_xml_attr_soft(domain_tag, "name");
							const char *parse = switch_xml_attr_soft(domain_tag, "parse");
							const char *alias = switch_xml_attr_soft(domain_tag, "alias");

							if (!zstr(dname)) {
								if (!strcasecmp(dname, "all")) {
									switch_xml_t xml_root, x_domains;
									if (switch_xml_locate("directory", NULL, NULL, NULL, &xml_root, &x_domains, xml_params, SWITCH_FALSE) == SWITCH_STATUS_SUCCESS) {
										for (x_domain_tag = switch_xml_child(x_domains, "domain"); x_domain_tag; x_domain_tag = x_domain_tag->next) {
											dname = switch_xml_attr_soft(x_domain_tag, "name");
											parse_domain_tag(profile, x_domain_tag, dname, parse, alias);
										}
										switch_xml_free(xml_root);
									}
								} else if (switch_xml_locate_domain(dname, xml_params, &droot, &x_domain_tag) == SWITCH_STATUS_SUCCESS) {
									parse_domain_tag(profile, x_domain_tag, dname, parse, alias);
									switch_xml_free(droot);
								}
							}
						}

						switch_event_destroy(&xml_params);
					}

					if ((aliases_tag = switch_xml_child(xprofile, "aliases"))) {
						for (alias_tag = switch_xml_child(aliases_tag, "alias"); alias_tag; alias_tag = alias_tag->next) {
							char *aname = (char *) switch_xml_attr_soft(alias_tag, "name");
							if (!zstr(aname)) {

								if (sofia_glue_add_profile(switch_core_strdup(profile->pool, aname), profile) == SWITCH_STATUS_SUCCESS) {
									switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "Adding Alias [%s] for profile [%s]\n", aname, profile->name);
								} else {
									switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "Alias [%s] for profile [%s] (already exists)\n",
													  aname, profile->name);
								}
							}
						}
					}

				} else {
					switch_xml_t aliases_tag, alias_tag;

					if ((aliases_tag = switch_xml_child(xprofile, "aliases"))) {
						for (alias_tag = switch_xml_child(aliases_tag, "alias"); alias_tag; alias_tag = alias_tag->next) {
							char *aname = (char *) switch_xml_attr_soft(alias_tag, "name");
							if (!zstr(aname)) {

								if (sofia_glue_add_profile(switch_core_strdup(profile->pool, aname), profile) == SWITCH_STATUS_SUCCESS) {
									switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "Adding Alias [%s] for profile [%s]\n", aname, profile->name);
								} else {
									switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Error Adding Alias [%s] for profile [%s] (name in use)\n",
													  aname, profile->name);
								}
							}
						}
					}

					if (profile->sipip) {
						switch_event_t *s_event;
						if (!profile->extsipport) profile->extsipport = profile->sip_port;

						launch_sofia_profile_thread(profile);
						if (profile->odbc_dsn) {
							switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "Connecting ODBC Profile %s [%s]\n", profile->name, url);
							switch_yield(1000000);
						} else {
							switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "Started Profile %s [%s]\n", profile->name, url);
						}
						if ((switch_event_create_subclass(&s_event, SWITCH_EVENT_CUSTOM, MY_EVENT_PROFILE_START) == SWITCH_STATUS_SUCCESS)) {
							switch_event_add_header_string(s_event, SWITCH_STACK_BOTTOM, "module_name", "mod_sofia");
							switch_event_add_header_string(s_event, SWITCH_STACK_BOTTOM, "profile_name", profile->name);
							if (profile) {
								switch_event_add_header_string(s_event, SWITCH_STACK_BOTTOM, "profile_uri", profile->url);
							}
							switch_event_fire(&s_event);
						}
					} else {
						switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "Unable to start Profile %s due to no configured sip-ip\n", profile->name);
						sofia_profile_start_failure(profile, profile->name);
					}
					profile = NULL;
				}
				if (profile_found) {
					break;
				}
			}
		}
	}
  done:

	if (profile_already_started) {
		sofia_glue_release_profile(profile_already_started);
	}

	switch_event_destroy(&params);

	if (xml) {
		switch_xml_free(xml);
	}

	if (profile_name && !profile_found) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "No Such Profile '%s'\n", profile_name);
		status = SWITCH_STATUS_FALSE;
	}

	return status;
}

const char *sofia_gateway_status_name(sofia_gateway_status_t status)
{
	static const char *status_names[] = { "DOWN", "UP", NULL };

	if (status < SOFIA_GATEWAY_INVALID) {
		return status_names[status];
	} else {
		return "INVALID";
	}
}

const char *sofia_sip_user_status_name(sofia_sip_user_status_t status)
{
	static const char *status_names[] = { "UNREACHABLE", "REACHABLE", NULL };

	if (status < SOFIA_REG_INVALID) {
		return status_names[status];
	} else {
		return "INVALID";
	}
}

struct cb_helper_sip_user_status {
	char *status;
	size_t status_len;

	char *contact;
	size_t contact_len;

	int count;
};

int sofia_sip_user_status_callback(void *pArg, int argc, char **argv, char **columnNames)
{
	struct cb_helper_sip_user_status *cbt = (struct cb_helper_sip_user_status *) pArg;

	if (argc != 3) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "expected 3 arguments from query, instead got %d\n", argc);
		return 0;
	}

	switch_copy_string(cbt->status, argv[0], cbt->status_len);
	cbt->count = (argv[1] && switch_is_number(argv[1])) ? atoi(argv[1]) : 0;

	switch_copy_string(cbt->contact, argv[2], cbt->contact_len);

	return 1;
}

static void sofia_handle_sip_r_options(switch_core_session_t *session, int status,
									   char const *phrase,
									   nua_t *nua, sofia_profile_t *profile, nua_handle_t *nh, sofia_private_t *sofia_private, sip_t const *sip,
								sofia_dispatch_event_t *de,
									   tagi_t tags[])
{
	sofia_gateway_t *gateway = NULL;
	switch_bool_t do_fire_gateway_state_event = SWITCH_FALSE;

	if (sofia_private && !zstr(sofia_private->gateway_name)) {
		gateway = sofia_reg_find_gateway(sofia_private->gateway_name);
		sofia_private->destroy_me = 1;
	}

	if (gateway) {
		if (status >= 200 && status < 600 && status != 408 && status != 503) {
			if (gateway->state == REG_STATE_FAILED) {
				gateway->state = REG_STATE_UNREGED;
			}

			if (gateway->ping_count < 0) {
				gateway->ping_count = 0;
			}

			if (gateway->ping_count < gateway->ping_max) {
				gateway->ping_count++;

				if (gateway->ping_count >= gateway->ping_min && gateway->status != SOFIA_GATEWAY_UP) {
					gateway->status = SOFIA_GATEWAY_UP;
					gateway->uptime = switch_time_now();
					do_fire_gateway_state_event = SWITCH_TRUE;
				}

				switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_WARNING,
								  "Ping succeeded %s with code %d - count %d/%d/%d, state %s\n",
								  gateway->name, status, gateway->ping_min, gateway->ping_count, gateway->ping_max, sofia_gateway_status_name(gateway->status));
			}
			if (gateway->ping_sent) {
				gateway->ping_time = (float)(switch_time_now() - gateway->ping_sent) / 1000;
				gateway->ping_sent = 0;
			}
		} else {
			if (gateway->state == REG_STATE_REGED) {
				switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_WARNING, "Unregister %s\n", gateway->name);
				gateway->state = REG_STATE_FAILED;
				gateway->ping_time = 0;
			}

			if (gateway->ping_count > 0) {
				gateway->ping_count--;
			}

			if (gateway->ping_count < gateway->ping_min && gateway->status != SOFIA_GATEWAY_DOWN) {
				gateway->status = SOFIA_GATEWAY_DOWN;
				do_fire_gateway_state_event = SWITCH_TRUE;
				gateway->ping_time = 0;
			}

			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_WARNING,
							  "Ping failed %s with code %d - count %d/%d/%d, state %s\n",
							  gateway->name, status, gateway->ping_min, gateway->ping_count, gateway->ping_max, sofia_gateway_status_name(gateway->status));
		}
		if (gateway->ping_monitoring || do_fire_gateway_state_event) {
			sofia_reg_fire_custom_gateway_state_event(gateway, status, phrase);
		}

		gateway->ping = switch_epoch_time_now(NULL) + gateway->ping_freq;
		sofia_reg_release_gateway(gateway);
		gateway->pinging = 0;
	} else if (sip && sip->sip_to && sip->sip_call_id && sip->sip_call_id->i_id && strchr(sip->sip_call_id->i_id, '_')) {
		const char *call_id = strchr(sip->sip_call_id->i_id, '_') + 1;
		char *sql;
		struct cb_helper_sip_user_status sip_user_status;
		char ping_status[255] = "";
		char sip_contact[1024] = "";
		int sip_user_ping_min = profile->sip_user_ping_min;
		int sip_user_ping_max = profile->sip_user_ping_max;

		char *sip_user = switch_mprintf("%s@%s", sip->sip_to->a_url->url_user, sip->sip_to->a_url->url_host);
		int ping_time = 0;

		if (sofia_private && sofia_private->ping_sent) {
			ping_time = (int)(switch_time_now() - sofia_private->ping_sent);
		}

		sip_user_status.status = ping_status;
		sip_user_status.status_len = sizeof(ping_status);
		sip_user_status.contact = sip_contact;
		sip_user_status.contact_len = sizeof(sip_contact);
		sql = switch_mprintf("select ping_status, ping_count, contact from sip_registrations where sip_user='%q' and sip_host='%q' and call_id='%q'",
				     sip->sip_to->a_url->url_user, sip->sip_to->a_url->url_host, call_id);
		sofia_glue_execute_sql_callback(profile, profile->ireg_mutex, sql, sofia_sip_user_status_callback, &sip_user_status);
		switch_safe_free(sql);

		if (status != 200 && status != 486) {
			sip_user_status.count--;
			if (sip_user_status.count >= 0) {
				switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "Ping to sip user '%s@%s' failed with code %d - count %d, state %s\n",
						  sip->sip_to->a_url->url_user, sip->sip_to->a_url->url_host, status, sip_user_status.count, sip_user_status.status);
				sql = switch_mprintf("update sip_registrations set ping_count=%d, ping_time=%d where sip_user='%q' and sip_host='%q' and call_id='%q'",
									 sip_user_status.count, ping_time, sip->sip_to->a_url->url_user, sip->sip_to->a_url->url_host, call_id);
				sofia_glue_execute_sql(profile, &sql, SWITCH_TRUE);
				switch_safe_free(sql);
			}
			if (sip_user_status.count < sip_user_ping_min) {
				if (strcmp(sip_user_status.status, "Unreachable")) {
					switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_WARNING, "Sip user '%s@%s' is now Unreachable\n",
							  sip->sip_to->a_url->url_user, sip->sip_to->a_url->url_host);
					sql = switch_mprintf("update sip_registrations set ping_status='Unreachable', ping_time=%d where sip_user='%q' and sip_host='%q' and call_id='%q'",
										 ping_time, sip->sip_to->a_url->url_user, sip->sip_to->a_url->url_host, call_id);
					sofia_glue_execute_sql(profile, &sql, SWITCH_TRUE);
					switch_safe_free(sql);
					sofia_reg_fire_custom_sip_user_state_event(profile, sip_user, sip_user_status.contact, sip->sip_to->a_url->url_user,
															   sip->sip_to->a_url->url_host, call_id, SOFIA_REG_REACHABLE, status, phrase);

					if (sofia_test_pflag(profile, PFLAG_UNREG_OPTIONS_FAIL)) {
						time_t now = switch_epoch_time_now(NULL);
						switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_WARNING, "Expire sip user '%s@%s' due to options failure\n",
								  sip->sip_to->a_url->url_user, sip->sip_to->a_url->url_host);

						sql = switch_mprintf("update sip_registrations set expires=%ld, ping_time=%d where sip_user='%q' and sip_host='%q' and call_id='%q'",
											 (long) now, ping_time, sip->sip_to->a_url->url_user, sip->sip_to->a_url->url_host, call_id);
						sofia_glue_execute_sql(profile, &sql, SWITCH_TRUE);
						switch_safe_free(sql);
					}
				}
			}
		} else {
			sip_user_status.count++;
			if (sip_user_status.count <= sip_user_ping_max) {
				switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "Ping to sip user '%s@%s' succeeded with code %d - count %d, state %s\n",
						  sip->sip_to->a_url->url_user, sip->sip_to->a_url->url_host, status, sip_user_status.count, sip_user_status.status);
				sql = switch_mprintf("update sip_registrations set ping_count=%d, ping_time=%d where sip_user='%q' and sip_host='%q' and call_id='%q'",
									 sip_user_status.count, ping_time, sip->sip_to->a_url->url_user, sip->sip_to->a_url->url_host, call_id);
				sofia_glue_execute_sql(profile, &sql, SWITCH_TRUE);
				switch_safe_free(sql);
			}
			if (sip_user_status.count >= sip_user_ping_min) {
				if (strcmp(sip_user_status.status, "Reachable")) {
					switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_WARNING, "Sip user '%s@%s' is now Reachable\n",
							  sip->sip_to->a_url->url_user, sip->sip_to->a_url->url_host);
					sql = switch_mprintf("update sip_registrations set ping_status='Reachable' where sip_user='%q' and sip_host='%q' and call_id='%q'",
							     sip->sip_to->a_url->url_user, sip->sip_to->a_url->url_host, call_id);
					sofia_glue_execute_sql(profile, &sql, SWITCH_TRUE);
					switch_safe_free(sql);
					sofia_reg_fire_custom_sip_user_state_event(profile, sip_user, sip_user_status.contact, sip->sip_to->a_url->url_user,
															   sip->sip_to->a_url->url_host, call_id, SOFIA_REG_UNREACHABLE, status, phrase);
				}
			}
		}

		switch_safe_free(sip_user);

	}
}

static void sofia_handle_sip_r_invite(switch_core_session_t *session, int status,
									  char const *phrase,
									  nua_t *nua, sofia_profile_t *profile, nua_handle_t *nh, sofia_private_t *sofia_private, sip_t const *sip,
								sofia_dispatch_event_t *de,
									  tagi_t tags[])
{
	char *call_info = NULL;
	if (sip && session) {
		switch_channel_t *channel = switch_core_session_get_channel(session);
		const char *uuid;
		switch_core_session_t *other_session;
		private_object_t *tech_pvt = switch_core_session_get_private(session);
		char network_ip[80];
		int network_port = 0;
		switch_caller_profile_t *caller_profile = NULL;
		int has_t38 = 0;

		switch_channel_clear_flag(channel, CF_REQ_MEDIA);

		if (status < 200) {
			if (switch_core_session_get_partner(session, &other_session) == SWITCH_STATUS_SUCCESS) {
				if(switch_core_session_compare(session, other_session)) {
					private_object_t *other_tech_pvt = switch_core_session_get_private(other_session);
					if (sofia_test_flag(other_tech_pvt, TFLAG_3PCC)) {
						sofia_set_flag_locked(tech_pvt, TFLAG_SKIP_EARLY);
					}
				}
				switch_core_session_rwunlock(other_session);
			}

			if (sofia_test_flag(tech_pvt, TFLAG_SKIP_EARLY)) {
				return;
			}
		}

		if (status >= 900) {
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "%s status %d received.\n",
							  switch_channel_get_name(channel), status);
			return;
		}

		if (status > 299) {
			switch_channel_set_variable(channel, "sip_hangup_disposition", "recv_refuse");
		}

		if (status >= 400) {
			char status_str[5];
			switch_snprintf(status_str, sizeof(status_str), "%d", status);
			switch_channel_set_variable(channel, "sip_invite_failure_status", status_str);
			switch_channel_set_variable(channel, "sip_invite_failure_phrase", phrase);
			switch_channel_set_variable_partner(channel, "sip_invite_failure_status", status_str);
			switch_channel_set_variable_partner(channel, "sip_invite_failure_phrase", phrase);
		} else {
			switch_channel_set_variable_partner(channel, "sip_invite_failure_status", NULL);
			switch_channel_set_variable_partner(channel, "sip_invite_failure_phrase", NULL);
		}

		if (status >= 400 && sip->sip_reason && sip->sip_reason->re_protocol && (!strcasecmp(sip->sip_reason->re_protocol, "Q.850")
				|| !strcasecmp(sip->sip_reason->re_protocol, "FreeSWITCH")
				|| !strcasecmp(sip->sip_reason->re_protocol, profile->sdp_username)) && sip->sip_reason->re_cause) {
				tech_pvt->q850_cause = atoi(sip->sip_reason->re_cause);
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "Remote Reason: %d\n", tech_pvt->q850_cause);
		}

		sofia_glue_get_addr(de->data->e_msg, network_ip, sizeof(network_ip), &network_port);

		switch_channel_set_variable_printf(channel, "sip_local_network_addr", "%s", profile->extsipip ? profile->extsipip : profile->sipip);
		switch_channel_set_variable(channel, "sip_reply_host", network_ip);
		switch_channel_set_variable_printf(channel, "sip_reply_port", "%d", network_port);

		switch_channel_set_variable_printf(channel, "sip_network_ip", "%s", network_ip);
		switch_channel_set_variable_printf(channel, "sip_network_port", "%d", network_port);

		if ((caller_profile = switch_channel_get_caller_profile(channel)) && !zstr(network_ip) &&
			(zstr(caller_profile->network_addr) || strcmp(caller_profile->network_addr, network_ip))) {
			caller_profile->network_addr = switch_core_strdup(caller_profile->pool, network_ip);
		}

		if (tech_pvt->mparams.last_sdp_response) {
			tech_pvt->mparams.prev_sdp_response = tech_pvt->mparams.last_sdp_response;
		}
		tech_pvt->mparams.last_sdp_response = NULL;

		if (sip->sip_payload && sip->sip_payload->pl_data) {
			switch_core_media_set_sdp_codec_string(session, sip->sip_payload->pl_data, SDP_TYPE_RESPONSE);

			if (!zstr(tech_pvt->mparams.prev_sdp_response) && !strcmp(tech_pvt->mparams.prev_sdp_response, sip->sip_payload->pl_data)) {
				tech_pvt->mparams.last_sdp_response = tech_pvt->mparams.prev_sdp_response;
			} else {
				tech_pvt->mparams.last_sdp_response = switch_core_session_strdup(session, sip->sip_payload->pl_data);
			}
		}

		if (status > 299 && switch_channel_test_app_flag_key("T38", tech_pvt->channel, CF_APP_T38_REQ)) {
			switch_channel_set_private(channel, "t38_options", NULL);
			switch_channel_clear_app_flag_key("T38", tech_pvt->channel, CF_APP_T38);
			switch_channel_clear_app_flag_key("T38", tech_pvt->channel, CF_APP_T38_REQ);
			switch_channel_set_app_flag_key("T38", tech_pvt->channel, CF_APP_T38_FAIL);
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "%s T38 invite failed\n", switch_channel_get_name(tech_pvt->channel));
		}


		if (sofia_test_pflag(profile, PFLAG_MANAGE_SHARED_APPEARANCE)) {
			if (channel && sip->sip_call_info) {
				char *p;
				call_info = sip_header_as_string(nua_handle_home(nh), (void *) sip->sip_call_info);

				if (switch_stristr("appearance", call_info)) {
					switch_channel_set_variable(channel, "presence_call_info_full", call_info);
					if ((p = strchr(call_info, ';'))) {
						switch_channel_set_variable(channel, "presence_call_info", p + 1);
					}
				}
			} else if ((status == 180 || status == 183 || status == 200)) {
				char buf[128] = "";
				char *sql;
				char *state = "active";

				if (status != 200) {
					state = "progressing";
				}

				if (sip &&
					sip->sip_from && sip->sip_from->a_url->url_user && sip->sip_from->a_url->url_host &&
					sip->sip_to && sip->sip_to->a_url->url_user && sip->sip_to->a_url->url_host) {
					sql =
						switch_mprintf("select 'appearance-index=1' from sip_subscriptions where expires > -1 and hostname='%q' and event='call-info' and "
									   "sub_to_user='%q' and sub_to_host='%q'", mod_sofia_globals.hostname, sip->sip_to->a_url->url_user,
									   sip->sip_from->a_url->url_host);
					sofia_glue_execute_sql2str(profile, profile->dbh_mutex, sql, buf, sizeof(buf));

					if (mod_sofia_globals.debug_sla > 1) {
						switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "QUERY SQL %s [%s]\n", sql, buf);
					}
					free(sql);

					if (!zstr(buf)) {
						sql = switch_mprintf("update sip_dialogs set call_info='%q',call_info_state='%q' "
											 "where uuid='%q'", buf, state, switch_core_session_get_uuid(session));

						if (mod_sofia_globals.debug_sla > 1) {
							switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "QUERY SQL %s\n", sql);
						}

						sofia_glue_execute_sql_now(profile, &sql, SWITCH_TRUE);

						switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_WARNING, "Auto-Fixing Broken SLA [<sip:%s>;%s]\n",
										  sip->sip_from->a_url->url_host, buf);
						switch_channel_set_variable_printf(channel, "presence_call_info_full", "<sip:%s>;%s", sip->sip_from->a_url->url_host, buf);
						switch_channel_set_variable(channel, "presence_call_info", buf);
					}
				}
			}
		}

		if ((status == 180 || status == 183 || status > 199)) {
			const char *vval;

			sofia_set_accept_language_channel_variable(channel, sip);

			if (status > 199) {
				sofia_glue_set_extra_headers(session, sip, SOFIA_SIP_RESPONSE_HEADER_PREFIX);
			} else {
				sofia_glue_set_extra_headers(session, sip, SOFIA_SIP_PROGRESS_HEADER_PREFIX);
			}


			if (!(vval = switch_channel_get_variable(channel, "sip_copy_custom_headers")) || switch_true(vval)) {
				switch_core_session_t *other_session;

				if (switch_core_session_get_partner(session, &other_session) == SWITCH_STATUS_SUCCESS) {
					if (status > 199) {
						switch_ivr_transfer_variable(session, other_session, SOFIA_SIP_RESPONSE_HEADER_PREFIX_T);
					} else {
						switch_ivr_transfer_variable(session, other_session, SOFIA_SIP_PROGRESS_HEADER_PREFIX_T);
					}
					switch_core_session_rwunlock(other_session);
				}
			}
		}

		if ((status == 180 || status == 183 || status == 200)) {
			const char *x_freeswitch_support;

			switch_channel_set_flag(channel, CF_MEDIA_ACK);

			if ((x_freeswitch_support = sofia_glue_get_unknown_header(sip, "X-FS-Support"))) {
				tech_pvt->x_freeswitch_support_remote = switch_core_session_strdup(session, x_freeswitch_support);
			}

			if (sip->sip_user_agent && sip->sip_user_agent->g_string) {
				switch_channel_set_variable(channel, "sip_user_agent", sip->sip_user_agent->g_string);
			} else if (sip->sip_server && sip->sip_server->g_string) {
				switch_channel_set_variable(channel, "sip_user_agent", sip->sip_server->g_string);
			}

			sofia_add_invite_header_to_chanvars(channel, nh, sip->sip_allow, "sip_allow");

			sofia_update_callee_id(session, profile, sip, SWITCH_FALSE);

			if (sofia_test_media_flag(tech_pvt->profile, SCMF_AUTOFIX_TIMING)) {
				switch_core_media_reset_autofix(tech_pvt->session, SWITCH_MEDIA_TYPE_AUDIO);
			}

		}

		if (channel && sip && (status == 300 || status == 301 || status == 302 || status == 305) && switch_channel_direction(channel) == SWITCH_CALL_DIRECTION_OUTBOUND) {
			sip_contact_t *p_contact = sip->sip_contact;
			int i = 0;
			char var_name[80];
			const char *diversion_header;
			char *full_contact = NULL;
			char *invite_contact;
			const char *br;
			const char *v;

			if ((v = switch_channel_get_variable(channel, "outbound_redirect_fatal")) && switch_true(v)) {
				su_home_t *home = su_home_new(sizeof(*home));
				switch_assert(home != NULL);

				for (p_contact = sip->sip_contact; p_contact; p_contact = p_contact->m_next) {
					full_contact = sip_header_as_string(home, (void *) p_contact);
					invite_contact = sofia_glue_strip_uri(full_contact);

					switch_snprintf(var_name, sizeof(var_name), "sip_redirect_contact_%d", i);
					switch_channel_set_variable(channel, var_name, full_contact);

					if (i == 0) {
							switch_channel_set_variable(channel, "sip_redirected_to", full_contact);
					}

					if (p_contact->m_url->url_user) {
						switch_snprintf(var_name, sizeof(var_name), "sip_redirect_contact_user_%d", i);
						switch_channel_set_variable(channel, var_name, p_contact->m_url->url_user);
					}
					if (p_contact->m_url->url_host) {
						switch_snprintf(var_name, sizeof(var_name), "sip_redirect_contact_host_%d", i);
						switch_channel_set_variable(channel, var_name, p_contact->m_url->url_host);
					}
					if (p_contact->m_url->url_params) {
						switch_snprintf(var_name, sizeof(var_name), "sip_redirect_contact_params_%d", i);
						switch_channel_set_variable(channel, var_name, p_contact->m_url->url_params);
					}

					free(invite_contact);
					i++;
				}
				if (home) {
					su_home_unref(home);
					home = NULL;
				}
				switch_snprintf(var_name, sizeof(var_name), "sip:%d", status);
				switch_channel_set_variable(channel, SWITCH_PROTO_SPECIFIC_HANGUP_CAUSE_VARIABLE, var_name);
				switch_channel_hangup(channel, SWITCH_CAUSE_REQUESTED_CHAN_UNAVAIL);
				goto end;
			}

			if (!p_contact) {
				switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "Missing contact header in redirect request\n");
				goto end;
			}

			if ((br = switch_channel_get_partner_uuid(channel))) {
				switch_xml_t root = NULL, domain = NULL;
				switch_core_session_t *a_session;
				switch_channel_t *a_channel;

				const char *sip_redirect_profile, *sip_redirect_context, *sip_redirect_dialplan, *sip_redirect_fork;

				if ((a_session = switch_core_session_locate(br)) && (a_channel = switch_core_session_get_channel(a_session))) {
					switch_stream_handle_t stream = { 0 };
					char separator[2] = "|";
					char *redirect_dialstring;
					su_home_t *home = su_home_new(sizeof(*home));
					switch_assert(home != NULL);

					SWITCH_STANDARD_STREAM(stream);

					if (!(sip_redirect_profile = switch_channel_get_variable(channel, "sip_redirect_profile"))) {
						sip_redirect_profile = profile->name;
					}
					if (!(sip_redirect_context = switch_channel_get_variable(channel, "sip_redirect_context"))) {
						sip_redirect_context = "redirected";
					}
					if (!(sip_redirect_dialplan = switch_channel_get_variable(channel, "sip_redirect_dialplan"))) {
						sip_redirect_dialplan = "XML";
					}

					sip_redirect_fork = switch_channel_get_variable(channel, "sip_redirect_fork");

					if (switch_true(sip_redirect_fork)) {
						*separator = ',';
					}

					for (p_contact = sip->sip_contact; p_contact; p_contact = p_contact->m_next) {
						full_contact = sip_header_as_string(home, (void *) p_contact);
						invite_contact = sofia_glue_strip_uri(full_contact);

						switch_snprintf(var_name, sizeof(var_name), "sip_redirect_contact_%d", i);
						switch_channel_set_variable(a_channel, var_name, full_contact);

						if (i == 0) {
							switch_channel_set_variable(channel, "sip_redirected_to", full_contact);
							switch_channel_set_variable(a_channel, "sip_redirected_to", full_contact);
						}

						if (p_contact->m_url->url_user) {
							switch_snprintf(var_name, sizeof(var_name), "sip_redirect_contact_user_%d", i);
							switch_channel_set_variable(channel, var_name, p_contact->m_url->url_user);
							switch_channel_set_variable(a_channel, var_name, p_contact->m_url->url_user);
						}
						if (p_contact->m_url->url_host) {
							switch_snprintf(var_name, sizeof(var_name), "sip_redirect_contact_host_%d", i);
							switch_channel_set_variable(channel, var_name, p_contact->m_url->url_host);
							switch_channel_set_variable(a_channel, var_name, p_contact->m_url->url_host);
						}
						if (p_contact->m_url->url_params) {
							switch_snprintf(var_name, sizeof(var_name), "sip_redirect_contact_params_%d", i);
							switch_channel_set_variable(channel, var_name, p_contact->m_url->url_params);
							switch_channel_set_variable(a_channel, var_name, p_contact->m_url->url_params);
						}

						switch_snprintf(var_name, sizeof(var_name), "sip_redirect_dialstring_%d", i);
						switch_channel_set_variable_printf(channel, var_name, "sofia/%s/%s", sip_redirect_profile, invite_contact);
						switch_channel_set_variable_printf(a_channel, var_name, "sofia/%s/%s", sip_redirect_profile, invite_contact);
						stream.write_function(&stream, "%ssofia/%s/%s", i ? separator : "", sip_redirect_profile, invite_contact);
						free(invite_contact);
						i++;
					}

					redirect_dialstring = stream.data;

					switch_channel_set_variable_printf(channel, "sip_redirect_count", "%d", i);
					switch_channel_set_variable(channel, "sip_redirect_dialstring", redirect_dialstring);
					switch_channel_set_variable(a_channel, "sip_redirect_dialstring", redirect_dialstring);

					p_contact = sip->sip_contact;
					full_contact = sip_header_as_string(home, (void *) sip->sip_contact);

					if ((diversion_header = sofia_glue_get_unknown_header(sip, "diversion"))) {
						switch_channel_set_variable(channel, "sip_redirected_by", diversion_header);
						switch_channel_set_variable(a_channel, "sip_redirected_by", diversion_header);
					}

					if (sofia_test_pflag(profile, PFLAG_MANUAL_REDIRECT)) {
						if (!(v = switch_channel_get_variable(channel, "outbound_redirect_info"))) {
							switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "Redirect: Transfering to %s %s %s\n",
											  p_contact->m_url->url_user, sip_redirect_dialplan, sip_redirect_context);

							if (switch_true(switch_channel_get_variable(channel, "recording_follow_transfer"))) {
								switch_ivr_transfer_recordings(session, a_session);
							}

							switch_ivr_session_transfer(a_session, p_contact->m_url->url_user, sip_redirect_dialplan, sip_redirect_context);
						}
						switch_channel_hangup(channel, SWITCH_CAUSE_REDIRECTION_TO_NEW_DESTINATION);
					} else if( !p_contact->m_url->url_host ) {
						switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_WARNING, "Received redirect with invalid URI\n");
						switch_channel_set_variable(channel, SWITCH_ENDPOINT_DISPOSITION_VARIABLE, "REDIRECT_ERROR");
						switch_channel_hangup(channel, SWITCH_CAUSE_DESTINATION_OUT_OF_ORDER);
					} else if ((!strcmp(profile->sipip, p_contact->m_url->url_host))
							   || (profile->extsipip && !strcmp(profile->extsipip, p_contact->m_url->url_host))
							   || (switch_xml_locate_domain(p_contact->m_url->url_host, NULL, &root, &domain) == SWITCH_STATUS_SUCCESS)) {
						switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "Redirect: Transfering to %s\n",
										  p_contact->m_url->url_user);

						if (switch_true(switch_channel_get_variable(channel, "recording_follow_transfer"))) {
							switch_ivr_transfer_recordings(session, a_session);
						}

						switch_ivr_session_transfer(a_session, p_contact->m_url->url_user, NULL, NULL);
						switch_channel_hangup(channel, SWITCH_CAUSE_REDIRECTION_TO_NEW_DESTINATION);
						switch_xml_free(root);
					} else {
						invite_contact = sofia_glue_strip_uri(full_contact);
						tech_pvt->redirected = switch_core_session_strdup(session, invite_contact);
						free(invite_contact);
					}

					if (home) {
						su_home_unref(home);
						home = NULL;
					}

					free(stream.data);

					switch_core_session_rwunlock(a_session);
				}
			} else {
				su_home_t *home = su_home_new(sizeof(*home));
				switch_assert(home != NULL);
				full_contact = sip_header_as_string(home, (void *) sip->sip_contact);
				invite_contact = sofia_glue_strip_uri(full_contact);

				switch_channel_set_variable(channel, "sip_redirected_to", invite_contact);
				tech_pvt->redirected = switch_core_session_strdup(session, invite_contact);

				free(invite_contact);

				if (home) {
					su_home_unref(home);
					home = NULL;
				}
			}
		}


		if (sip->sip_payload && sip->sip_payload->pl_data && switch_stristr("m=image", sip->sip_payload->pl_data)) {
			has_t38 = 1;
		}

		if (switch_channel_test_flag(channel, CF_PROXY_MODE)) {
			switch_channel_clear_flag(channel, CF_T38_PASSTHRU);
			has_t38 = 0;
		}

		if (switch_channel_test_flag(channel, CF_PROXY_MEDIA) && has_t38) {
			if (switch_core_media_ready(tech_pvt->session, SWITCH_MEDIA_TYPE_AUDIO)) {
				switch_core_media_udptl_mode(tech_pvt->session, SWITCH_MEDIA_TYPE_AUDIO);

				if ((uuid = switch_channel_get_partner_uuid(channel)) && (other_session = switch_core_session_locate(uuid))) {
					if (switch_core_session_compare(session, other_session)) {
						private_object_t *other_tech_pvt = switch_core_session_get_private(other_session);
						if (switch_core_media_ready(tech_pvt->session, SWITCH_MEDIA_TYPE_AUDIO)) {
							switch_core_media_udptl_mode(other_tech_pvt->session, SWITCH_MEDIA_TYPE_AUDIO);
						}
					}
					switch_core_session_rwunlock(other_session);
				}
			}

			has_t38 = 0;
		}

		if (status > 199 && (switch_channel_test_flag(channel, CF_PROXY_MODE) ||
							 switch_channel_test_flag(channel, CF_PROXY_MEDIA) ||
							 (switch_channel_test_flag(tech_pvt->channel, CF_T38_PASSTHRU) && (has_t38 || status > 299)))) {

			if (sofia_test_flag(tech_pvt, TFLAG_SENT_UPDATE)) {
				sofia_clear_flag_locked(tech_pvt, TFLAG_SENT_UPDATE);

				if ((uuid = switch_channel_get_partner_uuid(channel)) && (other_session = switch_core_session_locate(uuid))) {
					const char *r_sdp = NULL;
					switch_core_session_message_t *msg;
					private_object_t *other_tech_pvt = switch_core_session_get_private(other_session);
					//switch_channel_t *other_channel = switch_core_session_get_channel(other_session);

					if (sip->sip_payload && sip->sip_payload->pl_data &&
						sip->sip_content_type && sip->sip_content_type->c_subtype && switch_stristr("sdp", sip->sip_content_type->c_subtype)) {
						tech_pvt->mparams.remote_sdp_str = switch_core_session_strdup(tech_pvt->session, sip->sip_payload->pl_data);
						r_sdp = tech_pvt->mparams.remote_sdp_str;
						switch_core_media_proxy_remote_addr(session, NULL);
					}

					if (status == 415) {
						int new_status = 488;
						switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "Overriding %d %s with %d\n", status, phrase, new_status);
						status = new_status;
					}

					switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "Passing %d %s to other leg\n%s\n", status, phrase, switch_str_nil(r_sdp));

					if (switch_core_session_compare(session, other_session)) {
						private_object_t *other_tech_pvt = switch_core_session_get_private(other_session);
						sofia_set_flag(other_tech_pvt, TFLAG_PASS_ACK);
					}


					if (status == 491 && (switch_channel_test_flag(tech_pvt->channel, CF_T38_PASSTHRU) ||
										  switch_channel_test_flag(channel, CF_PROXY_MODE))) {
						nua_respond(other_tech_pvt->nh, SIP_491_REQUEST_PENDING, TAG_END());
						switch_core_session_rwunlock(other_session);
						goto end;
					} else if (status > 299) {
						switch_core_media_reset_t38(session);
						switch_core_media_reset_t38(other_session);
					} else if (status == 200 && switch_channel_test_flag(channel, CF_T38_PASSTHRU) &&
							   has_t38 && sip->sip_payload && sip->sip_payload->pl_data) {
						switch_t38_options_t *t38_options = switch_core_media_extract_t38_options(session, sip->sip_payload->pl_data);

						if (!t38_options) {
							switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_WARNING, "Could not parse T.38 options from sdp.\n");
							switch_channel_set_variable(channel, SWITCH_ENDPOINT_DISPOSITION_VARIABLE, "T.38 NEGOTIATION ERROR");
							switch_channel_hangup(channel, SWITCH_CAUSE_INCOMPATIBLE_DESTINATION);
							switch_core_session_rwunlock(other_session);
							goto end;
						} else {
							switch_core_media_process_t38_passthru(session, other_session, t38_options);
						}
					}


					msg = switch_core_session_alloc(other_session, sizeof(*msg));
					msg->message_id = SWITCH_MESSAGE_INDICATE_RESPOND;
					msg->from = __FILE__;
					msg->numeric_arg = status;
					msg->string_arg = switch_core_session_strdup(other_session, phrase);

					if (status == 200 && switch_channel_test_flag(tech_pvt->channel, CF_T38_PASSTHRU) && has_t38) {
						msg->pointer_arg = switch_core_session_strdup(other_session, "t38");
					} else if (r_sdp) {
						msg->pointer_arg = switch_core_session_strdup(other_session, r_sdp);
						msg->pointer_arg_size = strlen(r_sdp);
					}

					if (status == 200 && switch_channel_test_flag(channel, CF_T38_PASSTHRU) && has_t38) {
						if (switch_core_media_ready(session, SWITCH_MEDIA_TYPE_AUDIO) &&
							switch_core_media_ready(other_session, SWITCH_MEDIA_TYPE_AUDIO)) {
							switch_channel_clear_flag(channel, CF_NOTIMER_DURING_BRIDGE);
							switch_core_media_udptl_mode(session, SWITCH_MEDIA_TYPE_AUDIO);
							switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_INFO, "Activating T38 Passthru\n");
						}
					}

					switch_core_session_queue_message(other_session, msg);

					switch_core_session_rwunlock(other_session);
				}
				goto end;
			}
		}

		if ((status == 180 || status == 183 || status == 200)) {
			const char *astate = "early";
			url_t *from = NULL, *to = NULL, *contact = NULL;

			if (sip->sip_to) {
				to = sip->sip_to->a_url;
			}
			if (sip->sip_from) {
				from = sip->sip_from->a_url;
			}
			if (sip->sip_contact) {
				contact = sip->sip_contact->m_url;
			}

			if (status == 200) {
				astate = "confirmed";
			}

			if ((!switch_channel_test_flag(channel, CF_EARLY_MEDIA) && !switch_channel_test_flag(channel, CF_ANSWERED) &&
				 !switch_channel_test_flag(channel, CF_RING_READY)) || switch_channel_test_flag(channel, CF_RECOVERING)) {
				const char *from_user = "", *from_host = "", *to_user = "", *to_host = "", *contact_user = "", *contact_host = "";
				const char *user_agent = "", *call_id = "";
				const char *to_tag = "";
				const char *from_tag = "";
				char *sql = NULL;

				if (sip->sip_user_agent) {
					user_agent = switch_str_nil(sip->sip_user_agent->g_string);
				}

				if (sip->sip_call_id) {
					call_id = switch_str_nil(sip->sip_call_id->i_id);
				}

				if (to) {
					from_user = switch_str_nil(to->url_user);
					from_tag = switch_str_nil(sip->sip_to->a_tag);
				}

				if (from) {
					from_host = switch_str_nil(from->url_host);
					to_user = switch_str_nil(from->url_user);
					to_host = switch_str_nil(from->url_host);
					to_tag = switch_str_nil(sip->sip_from->a_tag);
				}

				if (contact) {
					contact_user = switch_str_nil(contact->url_user);
					contact_host = switch_str_nil(contact->url_host);
				}

				if (profile->pres_type) {
					const char *presence_data = switch_channel_get_variable(channel, "presence_data");
					const char *presence_id = switch_channel_get_variable(channel, "presence_id");
					char *full_contact = NULL;
					char *p = NULL;
					time_t now;

					if (sip->sip_contact) {
						full_contact = sip_header_as_string(nua_handle_home(tech_pvt->nh), (void *) sip->sip_contact);
					}

					if (call_info && (p = strchr(call_info, ';'))) {
						p++;
					}

					now = switch_epoch_time_now(NULL);

					sql = switch_mprintf("insert into sip_dialogs "
										 "(call_id,uuid,sip_to_user,sip_to_host,sip_to_tag,sip_from_user,sip_from_host,sip_from_tag,contact_user,"
										 "contact_host,state,direction,user_agent,profile_name,hostname,contact,presence_id,presence_data,"
										 "call_info,rcd,call_info_state) "
										 "values('%q','%q','%q','%q','%q','%q','%q','%q','%q','%q','%q','%q','%q','%q','%q','%q','%q','%q','%q',%ld,'')",
										 call_id,
										 switch_core_session_get_uuid(session),
										 to_user, to_host, to_tag, from_user, from_host, from_tag, contact_user,
										 contact_host, astate, "outbound", user_agent,
										 profile->name, mod_sofia_globals.hostname, switch_str_nil(full_contact),
										 switch_str_nil(presence_id), switch_str_nil(presence_data), switch_str_nil(p), (long) now);
					switch_assert(sql);

					sofia_glue_execute_sql_now(profile, &sql, SWITCH_TRUE);

					if ( full_contact ) {
						su_free(nua_handle_home(tech_pvt->nh), full_contact);
					}
				}
			} else if (status == 200 && (profile->pres_type)) {
				char *sql = NULL;
				const char *presence_data = switch_channel_get_variable(channel, "presence_data");
				const char *presence_id = switch_channel_get_variable(channel, "presence_id");

				sql = switch_mprintf("update sip_dialogs set state='%q',presence_id='%q',presence_data='%q' "
									 "where uuid='%q';\n", astate, switch_str_nil(presence_id), switch_str_nil(presence_data),
									 switch_core_session_get_uuid(session));
				switch_assert(sql);
				sofia_glue_execute_sql_now(profile, &sql, SWITCH_TRUE);
			}

			extract_header_vars(profile, sip, session, nh);
			extract_vars(profile, sip, session);
			switch_channel_clear_flag(tech_pvt->channel, CF_RECOVERING);
		}

	}

  end:

	if (call_info) {
		su_free(nua_handle_home(nh), call_info);
	}

	if (!session && (status == 180 || status == 183 || status == 200)) {
		/* nevermind */
		nua_handle_bind(nh, NULL);
		nua_handle_destroy(nh);
	}
}

/* Pure black magic, if you can't understand this code you are lucky.........*/
void *SWITCH_THREAD_FUNC media_on_hold_thread_run(switch_thread_t *thread, void *obj)
{
	switch_core_session_t *other_session = NULL, *session = (switch_core_session_t *) obj;
	const char *uuid;

	if (switch_core_session_read_lock(session) == SWITCH_STATUS_SUCCESS) {
		switch_channel_t *channel = switch_core_session_get_channel(session);
		private_object_t *tech_pvt = switch_core_session_get_private(session);

		if ((uuid = switch_channel_get_partner_uuid(channel)) && (other_session = switch_core_session_locate(uuid))) {
			if (switch_core_session_compare(session, other_session)) {
				switch_channel_t *other_channel = switch_core_session_get_channel(other_session);
				sofia_set_flag_locked(tech_pvt, TFLAG_HOLD_LOCK);

				switch_yield(250000);
				switch_channel_wait_for_flag(channel, CF_MEDIA_ACK, SWITCH_TRUE, 10000, NULL);
				switch_channel_wait_for_flag(other_channel, CF_MEDIA_ACK, SWITCH_TRUE, 10000, NULL);

				if (switch_channel_direction(tech_pvt->channel) == SWITCH_CALL_DIRECTION_INBOUND) {
					switch_ivr_3p_media(switch_core_session_get_uuid(other_session), SMF_REBRIDGE|SMF_REPLYONLY_B);
				} else {
					switch_ivr_3p_media(switch_core_session_get_uuid(other_session), SMF_REBRIDGE);
				}


				switch_core_media_clear_rtp_flag(tech_pvt->session, SWITCH_MEDIA_TYPE_AUDIO, SWITCH_RTP_FLAG_AUTOADJ);
				switch_core_media_clear_rtp_flag(other_session, SWITCH_MEDIA_TYPE_AUDIO, SWITCH_RTP_FLAG_AUTOADJ);


				switch_core_media_toggle_hold(session, 1);
			}
			switch_core_session_rwunlock(other_session);
		}

		switch_core_session_rwunlock(session);
	}

	return NULL;
}

static void launch_media_on_hold(switch_core_session_t *session)
{
	switch_thread_t *thread;
	switch_threadattr_t *thd_attr = NULL;

	switch_threadattr_create(&thd_attr, switch_core_session_get_pool(session));
	switch_threadattr_detach_set(thd_attr, 1);
	switch_threadattr_stacksize_set(thd_attr, SWITCH_THREAD_STACKSIZE);
	switch_thread_create(&thread, thd_attr, media_on_hold_thread_run, session, switch_core_session_get_pool(session));
}



static void mark_transfer_record(switch_core_session_t *session, const char *br_a, const char *br_b)
{
	switch_core_session_t *br_b_session, *br_a_session;
	switch_channel_t *channel;
	const char *uvar1, *dvar1, *uvar2, *dvar2;

	channel = switch_core_session_get_channel(session);

	if (switch_channel_direction(channel) == SWITCH_CALL_DIRECTION_INBOUND) {
		uvar1 = "sip_from_user";
		dvar1 = "sip_from_host";
	} else {
		uvar1 = "sip_to_user";
		dvar1 = "sip_to_host";
	}


	if ((br_b_session = switch_core_session_locate(br_b)) ) {
		switch_channel_t *br_b_channel = switch_core_session_get_channel(br_b_session);
		switch_caller_profile_t *cp = switch_channel_get_caller_profile(br_b_channel);

		if (switch_channel_direction(br_b_channel) == SWITCH_CALL_DIRECTION_INBOUND) {
			uvar2 = "sip_from_user";
			dvar2 = "sip_from_host";
		} else {
			uvar2 = "sip_to_user";
			dvar2 = "sip_to_host";
		}

		cp->transfer_source = switch_core_sprintf(cp->pool,
												  "%ld:%s:att_xfer:%s@%s/%s@%s",
												  (long) switch_epoch_time_now(NULL),
												  cp->uuid_str,
												  switch_channel_get_variable(channel, uvar1),
												  switch_channel_get_variable(channel, dvar1),
												  switch_channel_get_variable(br_b_channel, uvar2),
												  switch_channel_get_variable(br_b_channel, dvar2));

		switch_channel_add_variable_var_check(br_b_channel, SWITCH_TRANSFER_HISTORY_VARIABLE, cp->transfer_source, SWITCH_FALSE, SWITCH_STACK_PUSH);
		switch_channel_set_variable(br_b_channel, SWITCH_TRANSFER_SOURCE_VARIABLE, cp->transfer_source);

		switch_core_session_rwunlock(br_b_session);
	}



	if ((br_a_session = switch_core_session_locate(br_a)) ) {
		switch_channel_t *br_a_channel = switch_core_session_get_channel(br_a_session);
		switch_caller_profile_t *cp = switch_channel_get_caller_profile(br_a_channel);

		if (switch_channel_direction(br_a_channel) == SWITCH_CALL_DIRECTION_INBOUND) {
			uvar2 = "sip_from_user";
			dvar2 = "sip_from_host";
		} else {
			uvar2 = "sip_to_user";
			dvar2 = "sip_to_host";
		}

		cp->transfer_source = switch_core_sprintf(cp->pool,
												  "%ld:%s:att_xfer:%s@%s/%s@%s",
												  (long) switch_epoch_time_now(NULL),
												  cp->uuid_str,
												  switch_channel_get_variable(channel, uvar1),
												  switch_channel_get_variable(channel, dvar1),
												  switch_channel_get_variable(br_a_channel, uvar2),
												  switch_channel_get_variable(br_a_channel, dvar2));

		switch_channel_add_variable_var_check(br_a_channel, SWITCH_TRANSFER_HISTORY_VARIABLE, cp->transfer_source, SWITCH_FALSE, SWITCH_STACK_PUSH);
		switch_channel_set_variable(br_a_channel, SWITCH_TRANSFER_SOURCE_VARIABLE, cp->transfer_source);

		switch_core_session_rwunlock(br_a_session);
	}


}


static void sofia_handle_sip_i_state(switch_core_session_t *session, int status,
									 char const *phrase,
									 nua_t *nua, sofia_profile_t *profile, nua_handle_t *nh, sofia_private_t *sofia_private, sip_t const *sip,
								sofia_dispatch_event_t *de,
									 tagi_t tags[])
{
	const char *l_sdp = NULL, *r_sdp = NULL;
	int offer_recv = 0, answer_recv = 0, offer_sent = 0, answer_sent = 0;
	int ss_state = nua_callstate_init;
	switch_channel_t *channel = NULL;
	private_object_t *tech_pvt = NULL;
	const char *replaces_str = NULL;
	switch_core_session_t *other_session = NULL;
	switch_channel_t *other_channel = NULL;
	//private_object_t *other_tech_pvt = NULL;
	char st[80] = "";
	int is_dup_sdp = 0;
	switch_event_t *s_event = NULL;
	char *p;
	char *patched_sdp = NULL;

	tl_gets(tags,
			NUTAG_CALLSTATE_REF(ss_state),
			NUTAG_OFFER_RECV_REF(offer_recv),
			NUTAG_ANSWER_RECV_REF(answer_recv),
			NUTAG_OFFER_SENT_REF(offer_sent),
			NUTAG_ANSWER_SENT_REF(answer_sent),
			SIPTAG_REPLACES_STR_REF(replaces_str), SOATAG_LOCAL_SDP_STR_REF(l_sdp), SOATAG_REMOTE_SDP_STR_REF(r_sdp), TAG_END());

	if (session) {
		channel = switch_core_session_get_channel(session);
		tech_pvt = switch_core_session_get_private(session);

		if (!tech_pvt || !tech_pvt->nh) {
			goto done;
		}

		if (!r_sdp && (status == 100 || status == 200)) {
			if (ss_state == nua_callstate_completing) {
				if (tech_pvt->mparams.last_sdp_response) {
					r_sdp = tech_pvt->mparams.last_sdp_response;
				}
			} else if (ss_state == nua_callstate_received || ss_state == nua_callstate_ready) {
				if (tech_pvt->mparams.last_sdp_str) {
					r_sdp = tech_pvt->mparams.last_sdp_str;
				}
			}
		}

		if (tech_pvt->mparams.last_sdp_str) {
			tech_pvt->mparams.prev_sdp_str = tech_pvt->mparams.last_sdp_str;
		}

		if (tech_pvt->mparams.last_sdp_response) {
			tech_pvt->mparams.prev_sdp_response = tech_pvt->mparams.last_sdp_response;
		}

		tech_pvt->mparams.last_sdp_str = NULL;
		tech_pvt->mparams.last_sdp_response = NULL;

		if (r_sdp && (switch_channel_test_flag(channel, CF_PROXY_MODE) || switch_channel_test_flag(channel, CF_PROXY_MEDIA))) {
			const char *var;

			if ((var = switch_channel_get_variable(channel, "bypass_media_sdp_filter"))) {
				if ((patched_sdp = switch_core_media_process_sdp_filter(r_sdp, var, session))) {
					r_sdp = patched_sdp;
				}
			}
		}

		if ((switch_channel_test_flag(channel, CF_PROXY_MODE) || switch_channel_test_flag(channel, CF_PROXY_MEDIA)) ||
			(sofia_test_flag(profile, TFLAG_INB_NOMEDIA) || sofia_test_flag(profile, TFLAG_PROXY_MEDIA))) {

			/* This marr in our code brought to you by people who can't read........ */
			if (profile->ndlb & SM_NDLB_ALLOW_BAD_IANANAME && r_sdp && (p = (char *) switch_stristr("g729a/8000", r_sdp))) {
				p += 4;
				*p++ = '/';
				*p++ = '8';
				*p++ = '0';
				*p++ = '0';
				*p++ = '0';
				*p++ = ' ';
			}
		}
	}


	if (ss_state == nua_callstate_terminated) {

		if ((status == 300 || status == 301 || status == 302 || status == 305) && session) {
			channel = switch_core_session_get_channel(session);
			tech_pvt = switch_core_session_get_private(session);

			if (!tech_pvt || !tech_pvt->nh) {
				goto done;
			}


			if (tech_pvt->redirected) {
				sofia_glue_do_invite(session);
				goto done;
			}
		}

		if (sofia_private) {
			sofia_private->destroy_me = 1;
		}
	}

	if (session) {
		if ((switch_channel_test_flag(channel, CF_ANSWERED) && (status == 180 || status == 183) && !r_sdp) || (ss_state == nua_callstate_ready && status >= 300)) {
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "Channel %s skipping state [%s][%d]\n",
							  switch_channel_get_name(channel), nua_callstate_name(ss_state), status);
			goto done;
		} else if (switch_channel_test_flag(channel, CF_EARLY_MEDIA) && (status == 180 || status == 183) && r_sdp) {
			switch_channel_set_flag(tech_pvt->channel, CF_REINVITE);
		}

		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "Channel %s entering state [%s][%d]\n",
						  switch_channel_get_name(channel), nua_callstate_name(ss_state), status);

		if (r_sdp) {
			switch_channel_set_variable(channel, SWITCH_R_SDP_VARIABLE, r_sdp);

			if (!(profile->mndlb & SM_NDLB_ALLOW_NONDUP_SDP) || (!zstr(tech_pvt->mparams.remote_sdp_str) && !strcmp(tech_pvt->mparams.remote_sdp_str, r_sdp))) {
				switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "Duplicate SDP\n%s\n", r_sdp);
				is_dup_sdp = 1;
			} else {
				switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "Remote SDP:\n%s\n", r_sdp);
				tech_pvt->mparams.remote_sdp_str = switch_core_session_strdup(session, r_sdp);
				if (switch_channel_direction(channel) == SWITCH_CALL_DIRECTION_OUTBOUND) {
					switch_channel_mark_pre_answered(channel);
				}
                //if ((sofia_test_flag(tech_pvt, TFLAG_LATE_NEGOTIATION) || switch_channel_direction(channel) == SWITCH_CALL_DIRECTION_OUTBOUND)) {
				//	switch_core_media_set_sdp_codec_string(session, r_sdp, status < 200 ? SDP_TYPE_REQUEST : SDP_TYPE_RESPONSE);
				//}
				switch_core_media_set_sdp_codec_string(session, r_sdp, SDP_TYPE_REQUEST);
				sofia_glue_pass_sdp(tech_pvt, (char *) r_sdp);
				sofia_set_flag(tech_pvt, TFLAG_NEW_SDP);

			}
		}
	}

	if (status == 988) {
		goto done;
	}

	if (status == 183 && !r_sdp) {
		if ((channel && switch_true(switch_channel_get_variable(channel, "sip_ignore_183nosdp"))) || sofia_test_pflag(profile, PFLAG_IGNORE_183NOSDP)) {
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "%s Ignoring 183 w/o sdp\n", channel ? switch_channel_get_name(channel) : "None");
			goto done;
		}
		status = 180;
	}

	if (status == 180 && r_sdp) {
		status = 183;
	}

	if (channel && profile->pres_type && ss_state == nua_callstate_ready && status == 200) {
		const char* to_tag = "";
		char *sql = NULL;
		to_tag = switch_str_nil(switch_channel_get_variable(channel, "sip_to_tag"));
		sql = switch_mprintf("update sip_dialogs set sip_to_tag='%q' "
				"where uuid='%q' and sip_to_tag = ''", to_tag, switch_core_session_get_uuid(session));

		if (mod_sofia_globals.debug_presence > 1) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "QUERY SQL %s\n", sql);
		}
		sofia_glue_execute_sql_now(profile, &sql, SWITCH_TRUE);
	}

	if (channel && (status == 180 || status == 183) && switch_channel_direction(channel) == SWITCH_CALL_DIRECTION_OUTBOUND) {
		const char *full_to = NULL;
		const char *var;
		if ((var = switch_channel_get_variable(channel, "sip_auto_answer")) && switch_true(var) &&
                    !((var = switch_channel_get_variable(channel, "sip_auto_answer_suppress_notify")) && switch_true(var))) {
			full_to = switch_str_nil(switch_channel_get_variable(channel, "sip_full_to"));

			nua_notify(nh,
					   NUTAG_NEWSUB(1),
					   NUTAG_WITH_THIS_MSG(de->data->e_msg),
					   NUTAG_SUBSTATE(nua_substate_terminated),
					   TAG_IF((full_to), SIPTAG_TO_STR(full_to)),
					   SIPTAG_SUBSCRIPTION_STATE_STR("terminated;reason=noresource"),
					   SIPTAG_EVENT_STR("talk"),
					   TAG_END());
		}
	}



  state_process:

	switch ((enum nua_callstate) ss_state) {
	case nua_callstate_terminated:
	case nua_callstate_terminating:
	case nua_callstate_ready:
	case nua_callstate_completed:
	case nua_callstate_received:
	case nua_callstate_proceeding:
	case nua_callstate_completing:
	case nua_callstate_calling:
		if (!(session && channel && tech_pvt))
			goto done;
	default:
		break;
	}

	switch ((enum nua_callstate) ss_state) {
	case nua_callstate_init:
		break;
	case nua_callstate_authenticating:
		break;
	case nua_callstate_calling:
		tech_pvt->sent_last_invite = 1;
		tech_pvt->sent_invites++;
		break;
	case nua_callstate_proceeding:

		if (sofia_test_flag(tech_pvt, TFLAG_SKIP_EARLY)) {
			sofia_clear_flag_locked(tech_pvt, TFLAG_SKIP_EARLY);
			goto done;
		}

		switch (status) {
		case 180:
			switch_channel_mark_ring_ready(channel);
			break;
		case 182:
			switch_channel_mark_ring_ready_value(channel, SWITCH_RING_READY_QUEUED);
			break;
		default:
			break;
		}

		if (r_sdp) {
			if (switch_channel_test_flag(channel, CF_PROXY_MODE) && r_sdp) {
				char ibuf[35] = "", pbuf[35] = "";
				const char *ptr;

				if ((ptr = switch_stristr("c=IN IP4", r_sdp))) {
					int i = 0;

					ptr += 8;

					while(*ptr == ' ') {
						ptr++;
					}
					while(*ptr && *ptr != ' ' && *ptr != '\r' && *ptr != '\n') {
						ibuf[i++] = *ptr++;
					}

					switch_channel_set_variable(channel, SWITCH_REMOTE_MEDIA_IP_VARIABLE, ibuf);
				}

				if ((ptr = switch_stristr("m=audio", r_sdp))) {
					int i = 0;

					ptr += 7;

					while(*ptr == ' ') {
						ptr++;
					}
					while(*ptr && *ptr != ' ' && *ptr != '\r' && *ptr != '\n') {
						pbuf[i++] = *ptr++;
					}

					switch_channel_set_variable(channel, SWITCH_REMOTE_MEDIA_PORT_VARIABLE, pbuf);
				}

				if (switch_channel_test_flag(channel, CF_PROXY_MEDIA) &&  switch_channel_direction(tech_pvt->channel) == SWITCH_CALL_DIRECTION_INBOUND) {
					switch_channel_set_variable(channel, SWITCH_ENDPOINT_DISPOSITION_VARIABLE, "PROXY MEDIA");
				}
				sofia_set_flag_locked(tech_pvt, TFLAG_EARLY_MEDIA);
				switch_channel_mark_pre_answered(channel);
				sofia_set_flag(tech_pvt, TFLAG_SDP);
				if (switch_channel_test_flag(channel, CF_PROXY_MEDIA) || switch_channel_test_flag(channel, CF_REINVITE)) {
					if (sofia_media_activate_rtp(tech_pvt) != SWITCH_STATUS_SUCCESS) {
						goto done;
					}
				}
				if (switch_core_session_get_partner(session, &other_session) == SWITCH_STATUS_SUCCESS) {
					other_channel = switch_core_session_get_channel(other_session);
					switch_channel_pass_sdp(channel, other_channel, r_sdp);

					//switch_channel_pre_answer(other_channel);
					switch_core_session_queue_indication(other_session, SWITCH_MESSAGE_INDICATE_PROGRESS);
					switch_core_session_rwunlock(other_session);
				}
				goto done;
			} else {
				if (sofia_test_flag(tech_pvt, TFLAG_LATE_NEGOTIATION) &&  switch_channel_direction(tech_pvt->channel) == SWITCH_CALL_DIRECTION_INBOUND) {
					switch_channel_set_variable(channel, SWITCH_ENDPOINT_DISPOSITION_VARIABLE, "DELAYED NEGOTIATION");
				} else if (switch_channel_test_flag(channel, CF_PROXY_MEDIA)) {
					switch_channel_set_variable(channel, SWITCH_ENDPOINT_DISPOSITION_VARIABLE, "PROXY MEDIA");
					switch_core_media_patch_sdp(tech_pvt->session);
					if (sofia_media_activate_rtp(tech_pvt) != SWITCH_STATUS_SUCCESS) {
						nua_respond(nh, SIP_488_NOT_ACCEPTABLE, TAG_END());
						switch_channel_hangup(channel, SWITCH_CAUSE_INCOMPATIBLE_DESTINATION);
					} else{
						switch_channel_mark_pre_answered(channel);
					}

				} else {
					if (sofia_media_tech_media(tech_pvt, (char *) r_sdp) != SWITCH_STATUS_SUCCESS) {
						switch_channel_set_variable(channel, SWITCH_ENDPOINT_DISPOSITION_VARIABLE, "CODEC NEGOTIATION ERROR");
						nua_respond(nh, SIP_488_NOT_ACCEPTABLE, TAG_END());
						switch_channel_hangup(channel, SWITCH_CAUSE_INCOMPATIBLE_DESTINATION);
					}
				}
				goto done;
			}
		}
		break;
	case nua_callstate_completing:
		{
			int send_ack = 1;

			if (!switch_channel_test_flag(channel, CF_ANSWERED)) {
				const char *wait_for_ack = switch_channel_get_variable(channel, "sip_wait_for_aleg_ack");

				if (switch_true(wait_for_ack)) {
					switch_core_session_t *other_session;

					if (switch_core_session_get_partner(session, &other_session) == SWITCH_STATUS_SUCCESS) {
						if (switch_core_session_compare(session, other_session)) {
							private_object_t *other_tech_pvt = switch_core_session_get_private(other_session);

							if (!sofia_test_flag(tech_pvt, TFLAG_3PCC) && !sofia_test_flag(other_tech_pvt, TFLAG_3PCC)) {
								sofia_set_flag(other_tech_pvt, TFLAG_PASS_ACK);
								send_ack = 0;
							}
						}
						switch_core_session_rwunlock(other_session);
					}
				}
			}

			if (switch_channel_test_flag(channel, CF_3P_NOMEDIA_REQUESTED)) {

				if (switch_channel_test_flag(channel, CF_3P_NOMEDIA_REQUESTED_BLEG)) {
					switch_core_session_t *other_session;

					switch_channel_clear_flag(channel, CF_3P_NOMEDIA_REQUESTED_BLEG);

					if (switch_core_session_get_partner(session, &other_session) == SWITCH_STATUS_SUCCESS) {
						if (switch_core_session_compare(session, other_session)) {
							//switch_channel_t *other_channel = switch_core_session_get_channel(other_session);
							private_object_t *other_tech_pvt = switch_core_session_get_private(other_session);

							sofia_glue_clear_soa(other_session, SWITCH_TRUE);

							if (sofia_use_soa(other_tech_pvt)) {
								nua_ack(other_tech_pvt->nh,
										TAG_IF(!zstr(other_tech_pvt->user_via), SIPTAG_VIA_STR(other_tech_pvt->user_via)),
										SIPTAG_CONTACT_STR(other_tech_pvt->reply_contact),
										SOATAG_USER_SDP_STR(r_sdp),
										SOATAG_REUSE_REJECTED(1),
										SOATAG_RTP_SELECT(1),
										SOATAG_AUDIO_AUX("cn telephone-event"),
										TAG_IF(sofia_test_pflag(other_tech_pvt->profile, PFLAG_DISABLE_100REL), NUTAG_INCLUDE_EXTRA_SDP(1)),
										TAG_END());
							} else {
								nua_ack(other_tech_pvt->nh,
										NUTAG_MEDIA_ENABLE(0),
										TAG_IF(!zstr(other_tech_pvt->user_via), SIPTAG_VIA_STR(other_tech_pvt->user_via)),
										SIPTAG_CONTACT_STR(other_tech_pvt->reply_contact),
										TAG_IF(r_sdp, SIPTAG_CONTENT_TYPE_STR("application/sdp")),
										TAG_IF(r_sdp, SIPTAG_PAYLOAD_STR(r_sdp)),
										SOATAG_AUDIO_AUX("cn telephone-event"),
										TAG_END());
							}

							nua_ack(tech_pvt->nh,
									TAG_IF(!zstr(tech_pvt->user_via), SIPTAG_VIA_STR(tech_pvt->user_via)),
									SIPTAG_CONTACT_STR(tech_pvt->reply_contact),
									TAG_END());

						}
						switch_core_session_rwunlock(other_session);
					}
				} else {
					switch_channel_set_variable(channel, SWITCH_R_SDP_VARIABLE, r_sdp);
				}

				switch_channel_clear_flag(channel, CF_3P_NOMEDIA_REQUESTED);
				goto done;

			} else if (switch_channel_test_flag(channel, CF_3P_MEDIA_REQUESTED)) {
				uint8_t match = 0;

				switch_channel_clear_flag(channel, CF_PROXY_MODE);

				switch_core_media_choose_port(tech_pvt->session, SWITCH_MEDIA_TYPE_AUDIO, 0);
				switch_core_media_prepare_codecs(tech_pvt->session, SWITCH_FALSE);

				if (tech_pvt->mparams.num_codecs) {
					match = sofia_media_negotiate_sdp(session, r_sdp, SDP_TYPE_REQUEST);
				}

				if (!match) {
					if (switch_channel_get_state(channel) != CS_NEW) {
						nua_respond(tech_pvt->nh, SIP_488_NOT_ACCEPTABLE, TAG_END());
					}
				} else {
					switch_core_media_gen_local_sdp(session, SDP_TYPE_RESPONSE, NULL, 0, NULL, 0);
					switch_channel_set_variable(channel, SWITCH_ENDPOINT_DISPOSITION_VARIABLE, "RECEIVED");
					sofia_set_flag_locked(tech_pvt, TFLAG_READY);

					sofia_set_flag(tech_pvt, TFLAG_SDP);
					switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "RESTABLISH MEDIA SDP:\n%s\n", tech_pvt->mparams.local_sdp_str);

					switch_channel_set_flag(channel, CF_REQ_MEDIA);
					switch_channel_set_flag(channel, CF_MEDIA_ACK);
					switch_channel_set_flag(channel, CF_MEDIA_SET);

					switch_core_media_activate_rtp(session);


					if (sofia_use_soa(tech_pvt)) {
						nua_ack(tech_pvt->nh,
								TAG_IF(!zstr(tech_pvt->user_via), SIPTAG_VIA_STR(tech_pvt->user_via)),
								SIPTAG_CONTACT_STR(tech_pvt->reply_contact),
								SOATAG_USER_SDP_STR(tech_pvt->mparams.local_sdp_str),
								SOATAG_REUSE_REJECTED(1),
								SOATAG_RTP_SELECT(1),
								SOATAG_AUDIO_AUX("cn telephone-event"),
								TAG_IF(sofia_test_pflag(tech_pvt->profile, PFLAG_DISABLE_100REL), NUTAG_INCLUDE_EXTRA_SDP(1)),
								TAG_END());
					} else {
						nua_ack(tech_pvt->nh,
								NUTAG_MEDIA_ENABLE(0),
								TAG_IF(!zstr(tech_pvt->user_via), SIPTAG_VIA_STR(tech_pvt->user_via)),
								SIPTAG_CONTACT_STR(tech_pvt->reply_contact),
								TAG_IF(tech_pvt->mparams.local_sdp_str, SIPTAG_CONTENT_TYPE_STR("application/sdp")),
								TAG_IF(tech_pvt->mparams.local_sdp_str, SIPTAG_PAYLOAD_STR(tech_pvt->mparams.local_sdp_str)),
								SOATAG_AUDIO_AUX("cn telephone-event"),
								TAG_END());
					}

					switch_channel_clear_flag(channel, CF_3P_MEDIA_REQUESTED);
					goto done;
				}

				switch_channel_set_variable(channel, SWITCH_ENDPOINT_DISPOSITION_VARIABLE, "NO CODECS");
				switch_channel_hangup(channel, SWITCH_CAUSE_INCOMPATIBLE_DESTINATION);

				switch_channel_clear_flag(channel, CF_3P_MEDIA_REQUESTED);
				goto done;
				//ss_state = nua_callstate_ready;
				//goto state_process;
			}

			if (r_sdp && sofia_test_flag(tech_pvt, TFLAG_3PCC_INVITE) && !sofia_test_flag(tech_pvt, TFLAG_SDP)) {
				sofia_set_flag(tech_pvt, TFLAG_SDP);
				if (switch_core_session_get_partner(session, &other_session) == SWITCH_STATUS_SUCCESS) {
					other_channel = switch_core_session_get_channel(other_session);
					//other_tech_pvt = switch_core_session_get_private(other_session);

					switch_channel_pass_sdp(channel, other_channel, r_sdp);
					switch_core_session_queue_indication(other_session, SWITCH_MESSAGE_INDICATE_ANSWER);
					switch_core_session_rwunlock(other_session);
				}
				goto done;

			}

			if (send_ack) {
				tech_send_ack(nh, tech_pvt, NULL);
			} else {
				ss_state = nua_callstate_ready;
				goto state_process;
			}

		}
		goto done;
	case nua_callstate_received:
		tech_pvt->recv_invites++;
		tech_pvt->sent_last_invite = 0;

		if (!sofia_test_flag(tech_pvt, TFLAG_SDP)) {
			if (switch_core_session_get_partner(session, &other_session) == SWITCH_STATUS_SUCCESS) {
				private_object_t *other_tech_pvt = switch_core_session_get_private(other_session);
				int r = sofia_test_flag(other_tech_pvt, TFLAG_REINVITED);
				switch_core_session_rwunlock(other_session);

				if (r) {
					/* Due to a race between simultaneous reinvites to both legs of a bridge,
					  an earlier call to nua_invite silently failed.
					  So we reject the incoming invite with a 491 and redo the failed outgoing invite. */

					switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG,
									  "Other leg already handling a reinvite, so responding with 491\n");

					nua_respond(tech_pvt->nh, SIP_491_REQUEST_PENDING, TAG_END());
					sofia_glue_do_invite(session);
					goto done;
				}
			}


			if (r_sdp && !sofia_test_flag(tech_pvt, TFLAG_SDP)) {
				if (switch_channel_test_flag(channel, CF_PROXY_MODE)) {
					switch_channel_set_variable(channel, SWITCH_ENDPOINT_DISPOSITION_VARIABLE, "RECEIVED_NOMEDIA");
					sofia_set_flag_locked(tech_pvt, TFLAG_READY);
					if (switch_channel_get_state(channel) == CS_NEW) {
						switch_channel_set_state(channel, CS_INIT);
					}
					sofia_set_flag(tech_pvt, TFLAG_SDP);
					goto done;
				} else if (switch_channel_test_flag(tech_pvt->channel, CF_PROXY_MEDIA)) {
					switch_channel_set_variable(channel, SWITCH_ENDPOINT_DISPOSITION_VARIABLE, "PROXY MEDIA");
					sofia_set_flag_locked(tech_pvt, TFLAG_READY);
					if (switch_channel_get_state(channel) == CS_NEW) {
						switch_channel_set_state(channel, CS_INIT);
					}
				} else if (sofia_test_flag(tech_pvt, TFLAG_LATE_NEGOTIATION)) {
					switch_channel_set_variable(channel, SWITCH_ENDPOINT_DISPOSITION_VARIABLE, "DELAYED NEGOTIATION");
					sofia_set_flag_locked(tech_pvt, TFLAG_READY);
					if (switch_channel_get_state(channel) == CS_NEW) {
						switch_channel_set_state(channel, CS_INIT);
					}
				} else {
					uint8_t match = 0;

					if (tech_pvt->mparams.num_codecs) {
						match = sofia_media_negotiate_sdp(session, r_sdp, SDP_TYPE_REQUEST);
					}

					if (!match) {
						if (switch_channel_get_state(channel) != CS_NEW) {
							nua_respond(tech_pvt->nh, SIP_488_NOT_ACCEPTABLE, TAG_END());
						}
					} else {
						nua_handle_t *bnh;
						sip_replaces_t *replaces;
						su_home_t *home = NULL;
						switch_channel_set_variable(channel, SWITCH_ENDPOINT_DISPOSITION_VARIABLE, "RECEIVED");
						sofia_set_flag_locked(tech_pvt, TFLAG_READY);

						if (switch_channel_get_state(channel) == CS_NEW) {
							switch_channel_set_state(channel, CS_INIT);
						} else {
							nua_respond(tech_pvt->nh, SIP_200_OK, TAG_END());
						}
						sofia_set_flag(tech_pvt, TFLAG_SDP);
						if (replaces_str) {
							home = su_home_new(sizeof(*home));
							switch_assert(home != NULL);
							if ((replaces = sip_replaces_make(home, replaces_str))
								&& (bnh = nua_handle_by_replaces(nua, replaces))) {
								sofia_private_t *b_private;

								switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "Processing Replaces Attended Transfer\n");
								while (switch_channel_get_state(channel) < CS_EXECUTE) {
									switch_yield(10000);
								}

								if ((b_private = nua_handle_magic(bnh))) {
									const char *br_b = switch_channel_get_partner_uuid(channel);
									char *br_a = b_private->uuid;


									if (br_b) {
										switch_core_session_t *tmp;

										if (switch_true(switch_channel_get_variable(channel, "recording_follow_transfer")) &&
											(tmp = switch_core_session_locate(br_a))) {
											switch_ivr_transfer_recordings(session, tmp);
											switch_core_session_rwunlock(tmp);
										}

										switch_channel_set_variable_printf(channel, "transfer_to", "att:%s", br_b);

										mark_transfer_record(session, br_a, br_b);
										switch_ivr_uuid_bridge(br_a, br_b);
										switch_channel_set_variable(channel, SWITCH_ENDPOINT_DISPOSITION_VARIABLE, "ATTENDED_TRANSFER");
										sofia_clear_flag_locked(tech_pvt, TFLAG_SIP_HOLD);
										switch_channel_clear_flag(channel, CF_LEG_HOLDING);
										sofia_clear_flag_locked(tech_pvt, TFLAG_HOLD_LOCK);
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
							su_home_unref(home);
							home = NULL;
						}

						goto done;
					}

					switch_channel_set_variable(channel, SWITCH_ENDPOINT_DISPOSITION_VARIABLE, "NO CODECS");
					switch_channel_hangup(channel, SWITCH_CAUSE_INCOMPATIBLE_DESTINATION);
				}
			} else {
				if (sofia_test_pflag(profile, PFLAG_3PCC)) {
					if (switch_channel_test_flag(channel, CF_PROXY_MODE) || switch_channel_test_flag(channel, CF_PROXY_MEDIA)) {
						switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_INFO, "No SDP in INVITE and 3pcc=yes cannot work with bypass or proxy media, hanging up.\n");
						switch_channel_set_variable(channel, SWITCH_ENDPOINT_DISPOSITION_VARIABLE, "3PCC DISABLED");
						switch_channel_hangup(channel, SWITCH_CAUSE_MANDATORY_IE_MISSING);
					} else {
						switch_channel_set_variable(channel, SWITCH_ENDPOINT_DISPOSITION_VARIABLE, "RECEIVED_NOSDP");
						switch_core_media_choose_port(tech_pvt->session, SWITCH_MEDIA_TYPE_AUDIO, 0);
						switch_core_media_prepare_codecs(session, 1);
						switch_channel_set_state(channel, CS_HIBERNATE);
						switch_core_media_gen_local_sdp(session, SDP_TYPE_REQUEST, NULL, 0, NULL, 0);
						sofia_set_flag_locked(tech_pvt, TFLAG_3PCC);

						if (sofia_use_soa(tech_pvt)) {
							nua_respond(tech_pvt->nh, SIP_200_OK,
										SIPTAG_CONTACT_STR(tech_pvt->reply_contact),
										SOATAG_USER_SDP_STR(tech_pvt->mparams.local_sdp_str),
										SOATAG_REUSE_REJECTED(1),
										SOATAG_AUDIO_AUX("cn telephone-event"),
										TAG_IF(sofia_test_pflag(profile, PFLAG_DISABLE_100REL), NUTAG_INCLUDE_EXTRA_SDP(1)), TAG_END());
						} else {
							nua_respond(tech_pvt->nh, SIP_200_OK,
										NUTAG_MEDIA_ENABLE(0),
										SIPTAG_CONTACT_STR(tech_pvt->reply_contact),
										SIPTAG_CONTENT_TYPE_STR("application/sdp"), SIPTAG_PAYLOAD_STR(tech_pvt->mparams.local_sdp_str), TAG_END());
						}
					}
				} else if (sofia_test_pflag(profile, PFLAG_3PCC_PROXY)) {
					//3PCC proxy mode delays the 200 OK until the call is answered
					// so can be made to work with bypass media as we have time to find out what the other end thinks codec offer should be...
					switch_channel_set_variable(channel, SWITCH_ENDPOINT_DISPOSITION_VARIABLE, "RECEIVED_NOSDP");
					sofia_set_flag_locked(tech_pvt, TFLAG_3PCC);
					//switch_core_media_choose_port(tech_pvt->session, SWITCH_MEDIA_TYPE_AUDIO, 0);
					//switch_core_media_gen_local_sdp(session, NULL, 0, NULL, 0);
					sofia_set_flag(tech_pvt, TFLAG_LATE_NEGOTIATION);
					//Moves into CS_INIT so call moves forward into the dialplan
					switch_channel_set_state(channel, CS_INIT);
				} else {
					switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_INFO, "No SDP in INVITE and 3pcc not enabled, hanging up.\n");
					switch_channel_set_variable(channel, SWITCH_ENDPOINT_DISPOSITION_VARIABLE, "3PCC DISABLED");
					switch_channel_hangup(channel, SWITCH_CAUSE_MANDATORY_IE_MISSING);
				}
				goto done;
			}

		} else if (tech_pvt && sofia_test_flag(tech_pvt, TFLAG_SDP) && !r_sdp) {
			sofia_set_flag_locked(tech_pvt, TFLAG_NOSDP_REINVITE);
			if ((switch_channel_test_flag(channel, CF_PROXY_MODE) || switch_channel_test_flag(channel, CF_PROXY_MEDIA)) && sofia_test_pflag(profile, PFLAG_3PCC_PROXY)) {
				sofia_set_flag_locked(tech_pvt, TFLAG_3PCC);
				sofia_clear_flag(tech_pvt, TFLAG_ENABLE_SOA);

				if (switch_core_session_get_partner(session, &other_session) == SWITCH_STATUS_SUCCESS) {
					switch_core_session_message_t *msg;
					if (switch_core_session_compare(session, other_session)) {
						private_object_t *other_tech_pvt = switch_core_session_get_private(other_session);
						sofia_clear_flag(other_tech_pvt, TFLAG_ENABLE_SOA);
					}

					msg = switch_core_session_alloc(other_session, sizeof(*msg));
					msg->message_id = SWITCH_MESSAGE_INDICATE_MEDIA_REDIRECT;
					msg->from = __FILE__;
					msg->string_arg = NULL;
					switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "Passing NOSDP to other leg.\n");
					switch_core_session_queue_message(other_session, msg);
					switch_core_session_rwunlock(other_session);
				} else {
					switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_WARNING,
						  "NOSDP Re-INVITE to a proxy mode channel that is not in a bridge.\n");
					switch_channel_hangup(channel, SWITCH_CAUSE_DESTINATION_OUT_OF_ORDER);
				}
				goto done;
			}

			if (switch_channel_var_true(channel, "sip_unhold_nosdp")) {
				switch_core_media_gen_local_sdp(session, SDP_TYPE_RESPONSE, NULL, 0, "sendrecv",
												zstr(tech_pvt->mparams.local_sdp_str) || !switch_channel_test_flag(channel, CF_PROXY_MODE));
			} else {
				switch_core_media_gen_local_sdp(session, SDP_TYPE_RESPONSE, NULL, 0, NULL,
											zstr(tech_pvt->mparams.local_sdp_str) || !switch_channel_test_flag(channel, CF_PROXY_MODE));
			}

			if (zstr(tech_pvt->mparams.local_sdp_str)) {
				switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_WARNING, "Cannot find a SDP\n");
				switch_channel_hangup(channel, SWITCH_CAUSE_DESTINATION_OUT_OF_ORDER);
			} else {
				if (sofia_use_soa(tech_pvt)) {
					nua_respond(tech_pvt->nh, SIP_200_OK,
								SIPTAG_CONTACT_STR(tech_pvt->reply_contact),
								SOATAG_USER_SDP_STR(tech_pvt->mparams.local_sdp_str),
								SOATAG_REUSE_REJECTED(1),
								SOATAG_AUDIO_AUX("cn telephone-event"),
								TAG_IF(sofia_test_pflag(profile, PFLAG_DISABLE_100REL), NUTAG_INCLUDE_EXTRA_SDP(1)), TAG_END());
				} else {
					nua_respond(tech_pvt->nh, SIP_200_OK,
								NUTAG_MEDIA_ENABLE(0),
								SIPTAG_CONTACT_STR(tech_pvt->reply_contact),
								SIPTAG_CONTENT_TYPE_STR("application/sdp"), SIPTAG_PAYLOAD_STR(tech_pvt->mparams.local_sdp_str), TAG_END());
				}
			}

			goto done;

		} else {
			ss_state = nua_callstate_completed;
			goto state_process;
		}

		break;
	case nua_callstate_early:
		if (answer_recv) {
			uint8_t match = 0;
			sofia_set_flag_locked(tech_pvt, TFLAG_EARLY_MEDIA);
			switch_channel_mark_pre_answered(channel);
			sofia_set_flag(tech_pvt, TFLAG_SDP);


			match = sofia_media_negotiate_sdp(session, r_sdp, SDP_TYPE_RESPONSE);
			if (match) {
				if (switch_core_media_choose_port(tech_pvt->session, SWITCH_MEDIA_TYPE_AUDIO, 0) != SWITCH_STATUS_SUCCESS) {
					goto done;
				}

				switch_core_media_gen_local_sdp(session, SDP_TYPE_RESPONSE, NULL, 0, NULL, 0);

				if (sofia_media_activate_rtp(tech_pvt) != SWITCH_STATUS_SUCCESS) {
					switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "Early Media RTP Error!\n");
					switch_channel_hangup(channel, SWITCH_CAUSE_DESTINATION_OUT_OF_ORDER);
				}
				switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "Processing updated SDP\n");
			} else {
				switch_channel_hangup(channel, SWITCH_CAUSE_DESTINATION_OUT_OF_ORDER);
				switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "Early Media Codec Error!\n");
			}
		}
		break;
	case nua_callstate_completed:
		if (r_sdp) {
			const char *var;
			uint8_t match = 0, is_ok = 1, is_t38 = 0;
			tech_pvt->mparams.hold_laps = 0;

				if ((var = switch_channel_get_variable(channel, "sip_ignore_reinvites")) && switch_true(var)) {
					switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "Ignoring Re-invite\n");
					nua_respond(tech_pvt->nh, SIP_200_OK, TAG_END());
					goto done;
				}

				if (switch_stristr("m=image", r_sdp)) {
					is_t38 = 1;
				}


				if (switch_channel_test_flag(channel, CF_PROXY_MODE) || switch_channel_test_flag(channel, CF_PROXY_MEDIA)) {
					if ((sofia_test_media_flag(profile, SCMF_DISABLE_HOLD)
						 || ((var = switch_channel_get_variable(channel, "rtp_disable_hold")) && switch_true(var)))
						&& ((switch_stristr("sendonly", r_sdp) || switch_stristr("0.0.0.0", r_sdp) || switch_stristr("inactive", r_sdp)) || tech_pvt->mparams.hold_laps)) {
						nua_respond(tech_pvt->nh, SIP_200_OK, TAG_END());

						if (tech_pvt->mparams.hold_laps) {
							tech_pvt->mparams.hold_laps = 0;
						} else {
							tech_pvt->mparams.hold_laps = 1;
						}

						goto done;
					}

					if (switch_core_session_get_partner(session, &other_session) == SWITCH_STATUS_SUCCESS) {
						switch_core_session_message_t *msg;
						private_object_t *other_tech_pvt;
						int media_on_hold = switch_true(switch_channel_get_variable_dup(channel, "bypass_media_resume_on_hold", SWITCH_FALSE, -1));

						switch_core_media_clear_rtp_flag(other_session, SWITCH_MEDIA_TYPE_AUDIO, SWITCH_RTP_FLAG_AUTOADJ);


						if (switch_channel_test_flag(channel, CF_PROXY_MODE) && !is_t38 &&
							((profile->media_options & MEDIA_OPT_MEDIA_ON_HOLD) || media_on_hold)) {
							if (switch_stristr("sendonly", r_sdp) || switch_stristr("0.0.0.0", r_sdp) || switch_stristr("inactive", r_sdp)) {
								tech_pvt->mparams.hold_laps = 1;
								switch_channel_set_variable(channel, SWITCH_R_SDP_VARIABLE, r_sdp);
								switch_channel_clear_flag(channel, CF_PROXY_MODE);
								switch_core_media_set_local_sdp(tech_pvt->session, NULL, SWITCH_FALSE);
								switch_core_media_clear_rtp_flag(tech_pvt->session, SWITCH_MEDIA_TYPE_AUDIO, SWITCH_RTP_FLAG_AUTOADJ);

								if (!switch_channel_media_ready(channel)) {
									//if (switch_channel_direction(tech_pvt->channel) == SWITCH_CALL_DIRECTION_INBOUND) {
										//const char *r_sdp = switch_channel_get_variable(channel, SWITCH_R_SDP_VARIABLE);


										switch_core_media_prepare_codecs(tech_pvt->session, SWITCH_TRUE);
										if (sofia_media_tech_media(tech_pvt, r_sdp) != SWITCH_STATUS_SUCCESS) {
											switch_channel_set_variable(channel, SWITCH_ENDPOINT_DISPOSITION_VARIABLE, "CODEC NEGOTIATION ERROR");
											status = SWITCH_STATUS_FALSE;
											switch_core_session_rwunlock(other_session);
											switch_channel_hangup(channel, SWITCH_CAUSE_DESTINATION_OUT_OF_ORDER);
											goto done;
										}
										//}
								}


								if (!switch_core_media_ready(tech_pvt->session, SWITCH_MEDIA_TYPE_AUDIO)) {
									switch_core_media_prepare_codecs(tech_pvt->session, SWITCH_FALSE);
									if ((status = switch_core_media_choose_port(tech_pvt->session, SWITCH_MEDIA_TYPE_AUDIO, 0)) != SWITCH_STATUS_SUCCESS) {
										switch_channel_hangup(channel, SWITCH_CAUSE_DESTINATION_OUT_OF_ORDER);
										switch_core_session_rwunlock(other_session);
										goto done;
									}
								}

								switch_core_media_gen_local_sdp(session, SDP_TYPE_RESPONSE, NULL, 0, NULL, 1);

								if (sofia_use_soa(tech_pvt)) {
									nua_respond(tech_pvt->nh, SIP_200_OK,
												SIPTAG_CONTACT_STR(tech_pvt->reply_contact),
												SOATAG_USER_SDP_STR(tech_pvt->mparams.local_sdp_str),
												SOATAG_REUSE_REJECTED(1),
												SOATAG_AUDIO_AUX("cn telephone-event"),
												TAG_IF(sofia_test_pflag(profile, PFLAG_DISABLE_100REL), NUTAG_INCLUDE_EXTRA_SDP(1)), TAG_END());
								} else {
									nua_respond(tech_pvt->nh, SIP_200_OK,
												NUTAG_MEDIA_ENABLE(0),
												SIPTAG_CONTACT_STR(tech_pvt->reply_contact),
												SIPTAG_CONTENT_TYPE_STR("application/sdp"), SIPTAG_PAYLOAD_STR(tech_pvt->mparams.local_sdp_str), TAG_END());
								}

								switch_channel_set_flag(channel, CF_PROXY_MODE);
								switch_yield(250000);
								launch_media_on_hold(session);

								switch_core_session_rwunlock(other_session);
								goto done;
							}
						}

						if (switch_channel_test_flag(channel, CF_PROXY_MEDIA)) {
							sofia_media_activate_rtp(tech_pvt);
							switch_core_media_proxy_remote_addr(session, r_sdp);

							if ((tech_pvt->profile->mndlb & SM_NDLB_NEVER_PATCH_REINVITE)) {
								nua_respond(tech_pvt->nh, SIP_200_OK, TAG_END());
								switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "NOT proxying re-invite.\n");
								switch_core_session_rwunlock(other_session);
								goto done;
							}
						}

						other_tech_pvt = switch_core_session_get_private(other_session);

						if (sofia_test_flag(other_tech_pvt, TFLAG_REINVITED)) {
							/* The other leg won the reinvite race */
							switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "Other leg already handling reinvite, so responding with 491\n");
							nua_respond(tech_pvt->nh, SIP_491_REQUEST_PENDING, TAG_END());
							switch_core_session_rwunlock(other_session);
							goto done;
						}
						sofia_set_flag(tech_pvt, TFLAG_REINVITED);

						msg = switch_core_session_alloc(other_session, sizeof(*msg));
						msg->message_id = SWITCH_MESSAGE_INDICATE_MEDIA_REDIRECT;
						msg->from = __FILE__;
						msg->string_arg = switch_core_session_strdup(other_session, r_sdp);
						switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "Passing SDP to other leg.\n%s\n", r_sdp);

						if (sofia_test_flag(tech_pvt, TFLAG_SIP_HOLD)) {
							if (!switch_stristr("sendonly", r_sdp) && !switch_stristr("inactive", r_sdp)) {
								sofia_clear_flag_locked(tech_pvt, TFLAG_SIP_HOLD);
								switch_channel_clear_flag(channel, CF_LEG_HOLDING);
								switch_channel_presence(tech_pvt->channel, "unknown", "unhold", NULL);
							}
						} else if (switch_stristr("sendonly", r_sdp) && !switch_stristr("inactive", r_sdp)) {
							const char *msg = "hold";

							if (sofia_test_pflag(profile, PFLAG_MANAGE_SHARED_APPEARANCE)) {
								const char *info = switch_channel_get_variable(channel, "presence_call_info");
								if (info) {
									if (switch_stristr("private", info)) {
										msg = "hold-private";
									}
								}
							}

							sofia_set_flag_locked(tech_pvt, TFLAG_SIP_HOLD);
							switch_channel_set_flag(channel, CF_LEG_HOLDING);
							switch_channel_presence(tech_pvt->channel, "unknown", msg, NULL);
						}

						switch_core_session_queue_message(other_session, msg);

						switch_core_session_rwunlock(other_session);
					} else {
						switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_WARNING,
										  "Re-INVITE to a no-media channel that is not in a bridge.\n");
						is_ok = 0;
						switch_channel_hangup(channel, SWITCH_CAUSE_DESTINATION_OUT_OF_ORDER);
					}
					goto done;
				} else {
					int hold_related = 0;

					if (sofia_test_flag(tech_pvt, TFLAG_SIP_HOLD)) {
						hold_related = 2;
					} else if (switch_stristr("sendonly", r_sdp) || switch_stristr("0.0.0.0", r_sdp) || switch_stristr("inactive", r_sdp)) {
						hold_related = 1;
					}

					if (!is_t38 && hold_related && switch_core_session_get_partner(session, &other_session) == SWITCH_STATUS_SUCCESS) {
						if (switch_core_session_compare(session, other_session)) {
							switch_core_session_message_t *msg;
							const char *hold_msg = "hold";
							private_object_t *other_tech_pvt = switch_core_session_get_private(other_session);

							if (sofia_test_pflag(profile, PFLAG_PROXY_HOLD)) {
								switch_channel_set_flag(tech_pvt->channel, CF_REINVITE);

								if (tech_pvt->mparams.num_codecs){
									match = sofia_media_negotiate_sdp(session, r_sdp, SDP_TYPE_REQUEST);
								}

								if (!match) {
									switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "Reinvite Codec Error!\n");
									nua_respond(tech_pvt->nh, SIP_488_NOT_ACCEPTABLE, TAG_END());
									switch_core_session_rwunlock(other_session);
									goto done;
								}

								msg = switch_core_session_alloc(other_session, sizeof(*msg));

								if (switch_stristr("inactive", r_sdp)) {
									sofia_set_flag_locked(other_tech_pvt, TFLAG_SIP_HOLD_INACTIVE);
									//switch_channel_set_variable(channel, "sofia_hold_inactive", "true");
								} else {
									sofia_clear_flag_locked(other_tech_pvt, TFLAG_SIP_HOLD_INACTIVE);
								}

								if (hold_related == 1) {
									msg->message_id = SWITCH_MESSAGE_INDICATE_HOLD;
									if (sofia_test_pflag(profile, PFLAG_MANAGE_SHARED_APPEARANCE)) {
										const char *info = switch_channel_get_variable(channel, "presence_call_info");

										if (info) {
											if (switch_stristr("private", info)) {
												hold_msg = "hold-private";
											}
										}
									}
									sofia_set_flag_locked(tech_pvt, TFLAG_SIP_HOLD);
									switch_channel_set_flag(channel, CF_LEG_HOLDING);
									switch_channel_presence(tech_pvt->channel, "unknown", hold_msg, NULL);
								} else {
									hold_msg = "unhold";
									msg->message_id = SWITCH_MESSAGE_INDICATE_UNHOLD;
									sofia_clear_flag_locked(tech_pvt, TFLAG_SIP_HOLD);
									switch_channel_clear_flag(channel, CF_LEG_HOLDING);
									switch_channel_presence(tech_pvt->channel, "unknown", hold_msg, NULL);
								}
								msg->from = __FILE__;
								switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "Indicating %s to other leg.\n%s\n", hold_msg, r_sdp);

								switch_core_session_queue_message(other_session, msg);



								switch_core_media_gen_local_sdp(session, SDP_TYPE_RESPONSE, NULL, 0, NULL, 0);
								if (sofia_use_soa(tech_pvt)){
									nua_respond(tech_pvt->nh, SIP_200_OK,
												SIPTAG_CONTACT_STR(tech_pvt->reply_contact),
												SOATAG_USER_SDP_STR(tech_pvt->mparams.local_sdp_str),
												SOATAG_REUSE_REJECTED(1),
												SOATAG_AUDIO_AUX("cn telephone-event"),
												TAG_IF(sofia_test_pflag(profile, PFLAG_DISABLE_100REL), NUTAG_INCLUDE_EXTRA_SDP(1)), TAG_END());
								} else {
									nua_respond(tech_pvt->nh, SIP_200_OK,
												NUTAG_MEDIA_ENABLE(0),
												SIPTAG_CONTACT_STR(tech_pvt->reply_contact),
												SIPTAG_CONTENT_TYPE_STR("application/sdp"), SIPTAG_PAYLOAD_STR(tech_pvt->mparams.local_sdp_str), TAG_END());
								}
								switch_core_session_rwunlock(other_session);
								goto done;
							}
						}

						switch_core_session_rwunlock(other_session);
					}

					if (switch_channel_test_app_flag_key("T38", tech_pvt->channel, CF_APP_T38_NEGOTIATED)) {
						if (sofia_use_soa(tech_pvt)) {
							nua_respond(tech_pvt->nh, SIP_200_OK,
										SIPTAG_CONTACT_STR(tech_pvt->reply_contact),
										SOATAG_USER_SDP_STR(tech_pvt->mparams.local_sdp_str),
										SOATAG_REUSE_REJECTED(1),
										SOATAG_AUDIO_AUX("cn telephone-event"),
										TAG_IF(sofia_test_pflag(profile, PFLAG_DISABLE_100REL), NUTAG_INCLUDE_EXTRA_SDP(1)), TAG_END());
						} else {
							nua_respond(tech_pvt->nh, SIP_200_OK,
										NUTAG_MEDIA_ENABLE(0),
										SIPTAG_CONTACT_STR(tech_pvt->reply_contact),
										SIPTAG_CONTENT_TYPE_STR("application/sdp"), SIPTAG_PAYLOAD_STR(tech_pvt->mparams.local_sdp_str), TAG_END());
						}
						goto done;
					}

					switch_channel_set_flag(tech_pvt->channel, CF_REINVITE);

					if (tech_pvt->mparams.num_codecs) {
						match = sofia_media_negotiate_sdp(session, r_sdp, SDP_TYPE_REQUEST);
					}


					if (switch_channel_test_flag(channel, CF_PROXY_MODE)) {
						nua_respond(tech_pvt->nh, SIP_200_OK, TAG_END());
						goto done;
					}

					if (match && sofia_test_flag(tech_pvt, TFLAG_NOREPLY)) {
						sofia_clear_flag(tech_pvt, TFLAG_NOREPLY);
						goto done;
					}

					if (match) {
						if (switch_core_media_choose_port(tech_pvt->session, SWITCH_MEDIA_TYPE_AUDIO, 0) != SWITCH_STATUS_SUCCESS) {
							goto done;
						}

						switch_core_media_gen_local_sdp(session, SDP_TYPE_RESPONSE, NULL, 0, NULL, 0);

						if (sofia_media_activate_rtp(tech_pvt) != SWITCH_STATUS_SUCCESS) {
							switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "Reinvite RTP Error!\n");
							is_ok = 0;
							switch_channel_hangup(channel, SWITCH_CAUSE_DESTINATION_OUT_OF_ORDER);
						}
						switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "Processing updated SDP\n");
					} else {
						if (switch_channel_test_flag(channel, CF_PROXY_MODE) || switch_channel_test_flag(channel, CF_PROXY_MEDIA)) {
							nua_respond(tech_pvt->nh, SIP_200_OK, TAG_END());
							goto done;
						}

						switch_channel_clear_flag(tech_pvt->channel, CF_REINVITE);
						switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "Reinvite resulted in codec negotiation failure.\n");
						is_ok = 0;
					}
				}


				if (is_ok) {

					if (switch_core_session_local_crypto_key(tech_pvt->session, SWITCH_MEDIA_TYPE_AUDIO)) {
						switch_core_media_gen_local_sdp(session, SDP_TYPE_RESPONSE, NULL, 0, NULL, 0);
					}

					if (!switch_channel_test_flag(tech_pvt->channel, CF_AWAITING_STREAM_CHANGE)) {
						if (sofia_use_soa(tech_pvt)) {
							nua_respond(tech_pvt->nh, SIP_200_OK,
										SIPTAG_CONTACT_STR(tech_pvt->reply_contact),
										SOATAG_USER_SDP_STR(tech_pvt->mparams.local_sdp_str),
										SOATAG_REUSE_REJECTED(1),
										SOATAG_AUDIO_AUX("cn telephone-event"),
										TAG_IF(sofia_test_pflag(profile, PFLAG_DISABLE_100REL), NUTAG_INCLUDE_EXTRA_SDP(1)), TAG_END());
						} else {
							nua_respond(tech_pvt->nh, SIP_200_OK,
										NUTAG_MEDIA_ENABLE(0),
										SIPTAG_CONTACT_STR(tech_pvt->reply_contact),
										SIPTAG_CONTENT_TYPE_STR("application/sdp"), SIPTAG_PAYLOAD_STR(tech_pvt->mparams.local_sdp_str), TAG_END());
						}
					}

					if (switch_event_create_subclass(&s_event, SWITCH_EVENT_CUSTOM, MY_EVENT_REINVITE) == SWITCH_STATUS_SUCCESS) {
						switch_event_add_header_string(s_event, SWITCH_STACK_BOTTOM, "Unique-ID", switch_core_session_get_uuid(session));
						switch_event_fire(&s_event);
					}
				} else {
					nua_respond(tech_pvt->nh, SIP_488_NOT_ACCEPTABLE, TAG_END());
				}
			}
		break;
	case nua_callstate_ready:
		if (!switch_channel_test_flag(channel, CF_PROXY_MODE) && !switch_channel_test_flag(channel, CF_PROXY_MEDIA) &&
			r_sdp && (!is_dup_sdp || sofia_test_flag(tech_pvt, TFLAG_NEW_SDP)) && switch_core_media_ready(tech_pvt->session, SWITCH_MEDIA_TYPE_AUDIO) && !sofia_test_flag(tech_pvt, TFLAG_NOSDP_REINVITE)) {
			/* sdp changed since 18X w sdp, we're supposed to ignore it but we, of course, were pressured into supporting it */
			uint8_t match = 0;

			sofia_clear_flag(tech_pvt, TFLAG_NEW_SDP);
			switch_channel_set_flag(tech_pvt->channel, CF_REINVITE);


			if (tech_pvt->mparams.num_codecs) {
				match = sofia_media_negotiate_sdp(session, r_sdp, SDP_TYPE_RESPONSE);
			}
			if (match) {
				if (switch_core_media_choose_port(tech_pvt->session, SWITCH_MEDIA_TYPE_AUDIO, 0) != SWITCH_STATUS_SUCCESS) {
					goto done;
				}
				switch_core_media_gen_local_sdp(session, SDP_TYPE_RESPONSE, NULL, 0, NULL, 0);

				switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "Processing updated SDP\n");
				switch_channel_set_flag(tech_pvt->channel, CF_REINVITE);

				if (sofia_media_activate_rtp(tech_pvt) != SWITCH_STATUS_SUCCESS) {
					switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "RTP Error!\n");
					switch_channel_hangup(channel, SWITCH_CAUSE_DESTINATION_OUT_OF_ORDER);
					goto done;
				}
			} else {
				switch_channel_clear_flag(tech_pvt->channel, CF_REINVITE);
				switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "Codec Error! %s\n", r_sdp);
				goto done;

			}
		}

		if (r_sdp && sofia_test_flag(tech_pvt, TFLAG_NOSDP_REINVITE)) {
			sofia_clear_flag_locked(tech_pvt, TFLAG_NOSDP_REINVITE);
			if (switch_channel_test_flag(channel, CF_PROXY_MODE) || switch_channel_test_flag(channel, CF_PROXY_MEDIA)) {
				if (switch_channel_test_flag(channel, CF_PROXY_MEDIA)) {
					if (sofia_media_activate_rtp(tech_pvt) != SWITCH_STATUS_SUCCESS) {
						goto done;
					}
				}

				if (switch_core_session_get_partner(session, &other_session) == SWITCH_STATUS_SUCCESS) {
					other_channel = switch_core_session_get_channel(other_session);
					switch_channel_pass_sdp(channel, other_channel, r_sdp);

					if (sofia_test_flag(tech_pvt, TFLAG_3PCC) && sofia_test_pflag(profile, PFLAG_3PCC_PROXY)) {
						switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "3PCC-PROXY, Got my ACK\n");
						sofia_set_flag(tech_pvt, TFLAG_3PCC_HAS_ACK);
					} else {
						switch_core_session_message_t *msg;

						if (sofia_test_pflag(profile, PFLAG_3PCC_REINVITE_BRIDGED_ON_ACK)) {
							msg = switch_core_session_alloc(other_session, sizeof(*msg));
							msg->message_id = SWITCH_MESSAGE_INDICATE_MEDIA_REDIRECT;
							msg->from = __FILE__;
							msg->string_arg = switch_core_session_strdup(other_session, r_sdp);
							switch_core_session_queue_message(other_session, msg);
						}

						switch_core_session_queue_indication(other_session, SWITCH_MESSAGE_INDICATE_ANSWER);
					}

					switch_core_session_rwunlock(other_session);
				}
			} else {
				uint8_t match = 0;
				int is_ok = 1;

				if (tech_pvt->mparams.num_codecs) {
					match = sofia_media_negotiate_sdp(session, r_sdp, SDP_TYPE_RESPONSE);
				}

				if (match) {
					switch_channel_set_flag(tech_pvt->channel, CF_REINVITE);
					if (sofia_media_activate_rtp(tech_pvt) != SWITCH_STATUS_SUCCESS) {
						switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "RTP Error!\n");
						switch_channel_set_variable(tech_pvt->channel, SWITCH_ENDPOINT_DISPOSITION_VARIABLE, "RTP ERROR");
						is_ok = 0;
					}
					switch_channel_clear_flag(tech_pvt->channel, CF_REINVITE);
				} else {
					switch_channel_set_variable(tech_pvt->channel, SWITCH_ENDPOINT_DISPOSITION_VARIABLE, "CODEC NEGOTIATION ERROR");
					is_ok = 0;
				}

				if (!is_ok) {
					nua_respond(nh, SIP_488_NOT_ACCEPTABLE, TAG_END());
					switch_channel_hangup(tech_pvt->channel, SWITCH_CAUSE_INCOMPATIBLE_DESTINATION);
				}
			}
			goto done;
		}

		if (channel) {
			switch_channel_clear_flag(channel, CF_REQ_MEDIA);
		}
		if (tech_pvt && nh == tech_pvt->nh2) {
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "Cheater Reinvite!\n");
			switch_channel_set_flag(tech_pvt->channel, CF_REINVITE);
			tech_pvt->nh = tech_pvt->nh2;
			tech_pvt->nh2 = NULL;
			if (switch_core_media_choose_port(tech_pvt->session, SWITCH_MEDIA_TYPE_AUDIO, 0) == SWITCH_STATUS_SUCCESS) {
				if (sofia_media_activate_rtp(tech_pvt) != SWITCH_STATUS_SUCCESS) {
					switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "Cheater Reinvite RTP Error!\n");
					switch_channel_hangup(channel, SWITCH_CAUSE_DESTINATION_OUT_OF_ORDER);
				}
			}
			goto done;
		}

		if (channel) {
			if (sofia_test_flag(tech_pvt, TFLAG_EARLY_MEDIA) && !sofia_test_flag(tech_pvt, TFLAG_ANS)) {
				sofia_set_flag_locked(tech_pvt, TFLAG_ANS);
				sofia_set_flag(tech_pvt, TFLAG_SDP);
				switch_channel_mark_answered(channel);

				if (switch_channel_test_flag(channel, CF_PROXY_MODE) || switch_channel_test_flag(channel, CF_PROXY_MEDIA)) {
					if (switch_core_session_get_partner(session, &other_session) == SWITCH_STATUS_SUCCESS) {
						//other_channel = switch_core_session_get_channel(other_session);
						//switch_channel_answer(other_channel);
						switch_core_session_queue_indication(other_session, SWITCH_MESSAGE_INDICATE_ANSWER);
						switch_core_session_rwunlock(other_session);
					}
				}
				goto done;
			}

			if (!r_sdp && !sofia_test_flag(tech_pvt, TFLAG_SDP)) {
				r_sdp = (const char *) switch_channel_get_variable(channel, SWITCH_R_SDP_VARIABLE);
			}

			if (r_sdp && !sofia_test_flag(tech_pvt, TFLAG_SDP)) {
				if (switch_channel_test_flag(channel, CF_PROXY_MODE) || switch_channel_test_flag(channel, CF_PROXY_MEDIA)) {
					sofia_set_flag_locked(tech_pvt, TFLAG_ANS);
					sofia_set_flag_locked(tech_pvt, TFLAG_SDP);
					switch_channel_mark_answered(channel);

					if (switch_channel_test_flag(channel, CF_PROXY_MEDIA)) {
						if (sofia_media_activate_rtp(tech_pvt) != SWITCH_STATUS_SUCCESS) {
							goto done;
						}
					}

					if (switch_core_session_get_partner(session, &other_session) == SWITCH_STATUS_SUCCESS) {
						other_channel = switch_core_session_get_channel(other_session);
						switch_channel_pass_sdp(channel, other_channel, r_sdp);

						//switch_channel_answer(other_channel);
						switch_core_session_queue_indication(other_session, SWITCH_MESSAGE_INDICATE_ANSWER);

						switch_core_session_rwunlock(other_session);
					}

					if (sofia_test_flag(tech_pvt, TFLAG_3PCC) && sofia_test_pflag(profile, PFLAG_3PCC_PROXY)) {
						switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "3PCC-PROXY, Got my ACK\n");
						sofia_set_flag(tech_pvt, TFLAG_3PCC_HAS_ACK);
					}

					goto done;
				} else {
					uint8_t match = 0;


					match = sofia_media_negotiate_sdp(session, r_sdp, SDP_TYPE_RESPONSE);


					sofia_set_flag_locked(tech_pvt, TFLAG_ANS);

					if (match) {
						switch_channel_check_zrtp(channel);

						if (switch_core_media_choose_port(tech_pvt->session, SWITCH_MEDIA_TYPE_AUDIO, 0) == SWITCH_STATUS_SUCCESS) {
							if (sofia_media_activate_rtp(tech_pvt) == SWITCH_STATUS_SUCCESS) {
								switch_channel_mark_answered(channel);
							} else {
								switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "RTP Error!\n");
								switch_channel_hangup(channel, SWITCH_CAUSE_DESTINATION_OUT_OF_ORDER);
							}

							if (sofia_test_flag(tech_pvt, TFLAG_3PCC)) {
								/* Check if we are in 3PCC proxy mode, if so then set the flag to indicate we received the ack */
								if (sofia_test_pflag(profile, PFLAG_3PCC_PROXY)) {
									switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "3PCC-PROXY, Got my ACK\n");
									sofia_set_flag(tech_pvt, TFLAG_3PCC_HAS_ACK);
								} else if (switch_channel_get_state(channel) == CS_HIBERNATE) {
									sofia_set_flag_locked(tech_pvt, TFLAG_READY);
									switch_channel_set_state(channel, CS_INIT);
									sofia_set_flag(tech_pvt, TFLAG_SDP);
								}
							}
							goto done;
						}
					}

					switch_channel_set_variable(channel, SWITCH_ENDPOINT_DISPOSITION_VARIABLE, "NO CODECS");
					switch_channel_hangup(channel, SWITCH_CAUSE_INCOMPATIBLE_DESTINATION);
				}
			}
		}

		break;
	case nua_callstate_terminating:
		if (status == 488 || switch_channel_get_state(channel) == CS_HIBERNATE) {
			tech_pvt->q850_cause = SWITCH_CAUSE_MANDATORY_IE_MISSING;
		}
	case nua_callstate_terminated:
		sofia_set_flag_locked(tech_pvt, TFLAG_BYE);
		if (sofia_test_flag(tech_pvt, TFLAG_NOHUP)) {
			sofia_clear_flag_locked(tech_pvt, TFLAG_NOHUP);
		} else if (switch_channel_up(channel)) {
			int cause;
			if (tech_pvt->q850_cause) {
				cause = tech_pvt->q850_cause;
			} else {
				cause = sofia_glue_sip_cause_to_freeswitch(status);
			}
			if (status) {
				switch_snprintf(st, sizeof(st), "%d", status);
				switch_channel_set_variable(channel, "sip_term_status", st);
				switch_snprintf(st, sizeof(st), "sip:%d", status);
				switch_channel_set_variable(channel, SWITCH_PROTO_SPECIFIC_HANGUP_CAUSE_VARIABLE, st);
				if (phrase) {
					switch_channel_set_variable_partner(channel, "sip_hangup_phrase", phrase);
				}
				sofia_glue_set_extra_headers(session, sip, SOFIA_SIP_BYE_HEADER_PREFIX);
			}
			switch_snprintf(st, sizeof(st), "%d", cause);
			switch_channel_set_variable(channel, "sip_term_cause", st);
			switch_channel_hangup(channel, cause);
			ss_state = nua_callstate_terminated;
		}

		if (ss_state == nua_callstate_terminated) {
			if (tech_pvt->sofia_private) {
				tech_pvt->sofia_private = NULL;
			}

			tech_pvt->nh = NULL;

			if (nh) {
				nua_handle_bind(nh, NULL);
				nua_handle_destroy(nh);
			}
		}

		break;
	}

  done:

	switch_safe_free(patched_sdp);

	if ((enum nua_callstate) ss_state == nua_callstate_ready && channel && session && tech_pvt) {
		sofia_set_flag(tech_pvt, TFLAG_SIMPLIFY);
	}


	return;
}

typedef struct {
	char *exten;
	char *exten_with_params;
	char *event;
	char *reply_uuid;
	char *bridge_to_uuid;
	switch_event_t *vars;
	switch_memory_pool_t *pool;
} nightmare_xfer_helper_t;

void *SWITCH_THREAD_FUNC nightmare_xfer_thread_run(switch_thread_t *thread, void *obj)
{
	nightmare_xfer_helper_t *nhelper = (nightmare_xfer_helper_t *) obj;
	switch_memory_pool_t *pool;
	switch_status_t status = SWITCH_STATUS_FALSE;
	switch_core_session_t *session, *a_session;

	if ((a_session = switch_core_session_locate(nhelper->bridge_to_uuid))) {
		switch_core_session_t *tsession = NULL;
		switch_call_cause_t cause = SWITCH_CAUSE_NORMAL_CLEARING;
		uint32_t timeout = 60;
		char *tuuid_str;

		if ((session = switch_core_session_locate(nhelper->reply_uuid))) {
			private_object_t *tech_pvt = switch_core_session_get_private(session);
			switch_channel_t *channel_a = switch_core_session_get_channel(session);

			if ((status = switch_ivr_originate(NULL, &tsession, &cause, nhelper->exten_with_params, timeout, NULL, NULL, NULL,
											   switch_channel_get_caller_profile(channel_a), nhelper->vars, SOF_NONE, NULL, NULL)) == SWITCH_STATUS_SUCCESS) {
				if (switch_channel_up(channel_a)) {

					if (switch_true(switch_channel_get_variable(channel_a, "recording_follow_transfer"))) {
						switch_ivr_transfer_recordings(session, a_session);
					}


					tuuid_str = switch_core_session_get_uuid(tsession);
					switch_channel_set_variable_printf(channel_a, "transfer_to", "att:%s", tuuid_str);
					mark_transfer_record(session, nhelper->bridge_to_uuid, tuuid_str);
					switch_ivr_uuid_bridge(nhelper->bridge_to_uuid, tuuid_str);
					switch_channel_set_variable(channel_a, SWITCH_ENDPOINT_DISPOSITION_VARIABLE, "ATTENDED_TRANSFER");
				} else {
					switch_channel_hangup(switch_core_session_get_channel(tsession), SWITCH_CAUSE_ORIGINATOR_CANCEL);
					status = SWITCH_STATUS_FALSE;
				}
				switch_core_session_rwunlock(tsession);
			}

			if (status == SWITCH_STATUS_SUCCESS) {
				switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "The nightmare is over.....\n");
			} else {
				switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "1 .. 2 .. Freddie's commin' for you...\n");
			}

			nua_notify(tech_pvt->nh, NUTAG_NEWSUB(1), SIPTAG_CONTENT_TYPE_STR("message/sipfrag"),
					   NUTAG_SUBSTATE(nua_substate_terminated),SIPTAG_SUBSCRIPTION_STATE_STR("terminated;reason=noresource"),
					   SIPTAG_PAYLOAD_STR(status == SWITCH_STATUS_SUCCESS ? "SIP/2.0 200 OK\r\n" :
										  "SIP/2.0 403 Forbidden\r\n"), SIPTAG_EVENT_STR(nhelper->event), TAG_END());

			switch_core_session_rwunlock(session);
		}

		switch_core_session_rwunlock(a_session);
	}

	switch_event_destroy(&nhelper->vars);

	pool = nhelper->pool;
	switch_core_destroy_memory_pool(&pool);

	return NULL;
}

static void launch_nightmare_xfer(nightmare_xfer_helper_t *nhelper)
{
	switch_thread_t *thread;
	switch_threadattr_t *thd_attr = NULL;

	switch_threadattr_create(&thd_attr, nhelper->pool);
	switch_threadattr_detach_set(thd_attr, 1);
	switch_threadattr_stacksize_set(thd_attr, SWITCH_THREAD_STACKSIZE);
	switch_thread_create(&thread, thd_attr, nightmare_xfer_thread_run, nhelper, nhelper->pool);
}

/*---------------------------------------*/

static switch_status_t xfer_hanguphook(switch_core_session_t *session)
{
	switch_channel_t *channel = switch_core_session_get_channel(session);
	switch_channel_state_t state = switch_channel_get_state(channel);

	if (state == CS_HANGUP) {
		switch_core_session_t *ksession;
		const char *uuid = switch_channel_get_variable(channel, "att_xfer_kill_uuid");

		if (uuid && (ksession = switch_core_session_force_locate(uuid))) {
			switch_channel_t *kchannel = switch_core_session_get_channel(ksession);

			switch_channel_clear_flag(kchannel, CF_XFER_ZOMBIE);
			switch_channel_clear_flag(kchannel, CF_TRANSFER);
			if (switch_channel_up(kchannel)) {
				switch_channel_hangup(kchannel, SWITCH_CAUSE_NORMAL_CLEARING);
			}

			switch_core_session_rwunlock(ksession);
		}

		switch_core_event_hook_remove_state_change(session, xfer_hanguphook);

	}

	return SWITCH_STATUS_SUCCESS;
}

nua_handle_t *sofia_global_nua_handle_by_replaces(sip_replaces_t *replaces)
{
	nua_handle_t *nh = NULL;
	switch_hash_index_t *hi;
	const void *var;
	void *val;
	sofia_profile_t *profile;

	switch_mutex_lock(mod_sofia_globals.hash_mutex);
	if (mod_sofia_globals.profile_hash) {
		for (hi = switch_core_hash_first(mod_sofia_globals.profile_hash); hi; hi = switch_core_hash_next(&hi)) {
			switch_core_hash_this(hi, &var, NULL, &val);
			if ((profile = (sofia_profile_t *) val)) {
				if (!(nh = nua_handle_by_replaces(profile->nua, replaces))) {
					nh = nua_handle_by_call_id(profile->nua, replaces->rp_call_id);
				}
				if (nh)
					break;
			}
		}
		switch_safe_free(hi);
	}
	switch_mutex_unlock(mod_sofia_globals.hash_mutex);

	return nh;

}

static switch_status_t sofia_process_proxy_refer(switch_core_session_t *session, const char *refer_to)
{
	switch_core_session_t *other_session;
	private_object_t *tech_pvt = switch_core_session_get_private(session);

	if (switch_core_session_get_partner(session, &other_session) == SWITCH_STATUS_SUCCESS) {
		switch_core_session_message_t *msg;

		tech_pvt->proxy_refer_uuid = switch_core_session_strdup(session, switch_core_session_get_uuid(other_session));
		msg = switch_core_session_alloc(other_session, sizeof(*msg));
		MESSAGE_STAMP_FFL(msg);
		msg->message_id = SWITCH_MESSAGE_INDICATE_DEFLECT;
		msg->string_arg = switch_core_session_strdup(other_session, refer_to);
		msg->string_array_arg[0] = switch_core_session_strdup(other_session, switch_core_session_get_uuid(session));
		msg->from = __FILE__;
		switch_core_session_queue_message(other_session, msg);
		switch_core_session_rwunlock(other_session);
		return SWITCH_STATUS_SUCCESS;
	}

	return SWITCH_STATUS_FALSE;
}

void sofia_handle_sip_i_refer(nua_t *nua, sofia_profile_t *profile, nua_handle_t *nh, switch_core_session_t *session, sip_t const *sip,
								sofia_dispatch_event_t *de, tagi_t tags[])
{
	/* Incoming refer */
	sip_from_t const *from;
	//sip_to_t const *to;
	sip_refer_to_t const *refer_to;
	private_object_t *tech_pvt = switch_core_session_get_private(session);
	char *etmp = NULL, *exten = NULL;
	switch_channel_t *channel_a = switch_core_session_get_channel(session);
	switch_channel_t *channel_b = NULL;
	su_home_t *home = NULL;
	char *full_ref_by = NULL;
	char *full_ref_to = NULL;
	nightmare_xfer_helper_t *nightmare_xfer_helper;
	switch_memory_pool_t *npool;
	switch_event_t *event = NULL;

	if (!(profile->mflags & MFLAG_REFER)) {
		nua_respond(nh, SIP_403_FORBIDDEN, NUTAG_WITH_THIS_MSG(de->data->e_msg), TAG_END());
		goto done;
	}

	if (!sip->sip_cseq || !(etmp = switch_mprintf("refer;id=%u", sip->sip_cseq->cs_seq))) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "Memory Error!\n");
		goto done;
	}

	home = su_home_new(sizeof(*home));
	switch_assert(home != NULL);

	if ((refer_to = sip->sip_refer_to)) {
		full_ref_to = sip_header_as_string(home, (void *) sip->sip_refer_to);
	}

	if (full_ref_to && sofia_test_pflag(profile, PFLAG_PROXY_REFER)) {
		if (sofia_process_proxy_refer(session, full_ref_to) == SWITCH_STATUS_SUCCESS) {
			if (tech_pvt->proxy_refer_msg) {
				msg_ref_destroy(tech_pvt->proxy_refer_msg);
				tech_pvt->proxy_refer_msg = NULL;
			}
			tech_pvt->proxy_refer_msg = msg_ref_create(de->data->e_msg);
			//nua_respond(nh, SIP_202_ACCEPTED, NUTAG_WITH_THIS_MSG(de->data->e_msg), SIPTAG_EXPIRES_STR("60"), TAG_END());
			goto done;
		}
	}

	from = sip->sip_from;
	//to = sip->sip_to;

	nua_respond(nh, SIP_202_ACCEPTED, NUTAG_WITH_THIS_MSG(de->data->e_msg), SIPTAG_EXPIRES_STR("60"), TAG_END());


	switch_channel_set_variable(tech_pvt->channel, SOFIA_REPLACES_HEADER, NULL);

	if (sip->sip_referred_by) {
		full_ref_by = sip_header_as_string(home, (void *) sip->sip_referred_by);
	}

	if (refer_to) {
		char *rep = NULL;

		if (sofia_test_pflag(profile, PFLAG_FULL_ID)) {
			exten = switch_core_session_sprintf(session, "%s@%s", (char *) refer_to->r_url->url_user, (char *) refer_to->r_url->url_host);
		} else {
			exten = (char *) refer_to->r_url->url_user;
		}

		if (refer_to->r_url->url_params) {
			switch_channel_set_variable(tech_pvt->channel, "sip_refer_to_params", refer_to->r_url->url_params);
		}

		switch_core_session_queue_indication(session, SWITCH_MESSAGE_REFER_EVENT);
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "Process REFER to [%s@%s]\n", exten, (char *) refer_to->r_url->url_host);

		switch_channel_set_variable(tech_pvt->channel, "transfer_disposition", "recv_replace");


		if (refer_to->r_url->url_headers) {
			rep = (char *) switch_stristr("Replaces=", refer_to->r_url->url_headers);
		}


		if (rep) {
			sip_replaces_t *replaces;
			nua_handle_t *bnh = NULL;

			const char *br_a = NULL, *br_b = NULL;
			char *buf;
			char *p;

			rep = switch_core_session_strdup(session, rep + 9);

			if ((buf = switch_core_session_alloc(session, strlen(rep) + 1))) {
				rep = url_unescape(buf, (const char *) rep);
				if ((p = strchr(rep, ';'))) {
					*p = '\0';
				}
				switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "Replaces: [%s]\n", rep);
			} else {
				switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "Memory Error!\n");
				goto done;
			}

			if ((replaces = sip_replaces_make(home, rep))) {
				if (!(bnh = nua_handle_by_replaces(nua, replaces))) {
					if (!(bnh = nua_handle_by_call_id(nua, replaces->rp_call_id))) {
						bnh = sofia_global_nua_handle_by_replaces(replaces);
					}
				}
			}

			if (bnh && !sofia_test_pflag(profile, PFLAG_MAKE_EVERY_TRANSFER_A_NIGHTMARE)) {
				sofia_private_t *b_private = NULL;
				private_object_t *b_tech_pvt = NULL;
				switch_core_session_t *b_session = NULL;


				switch_channel_set_variable(channel_a, SOFIA_REPLACES_HEADER, rep);
				if ((b_private = nua_handle_magic(bnh))) {
					int deny_refer_requests = 0;

					if (!(b_session = switch_core_session_locate(b_private->uuid))) {
						goto done;
					}
					b_tech_pvt = (private_object_t *) switch_core_session_get_private(b_session);
					channel_b = switch_core_session_get_channel(b_session);

					switch_channel_set_variable(channel_a, "refer_uuid", b_private->uuid);
					switch_channel_set_variable(channel_b, "transfer_disposition", "replaced");

					br_a = switch_channel_get_partner_uuid(channel_a);
					br_b = switch_channel_get_partner_uuid(channel_b);

					if (!switch_ivr_uuid_exists(br_a)) {
						br_a = NULL;
					}

					if (!switch_ivr_uuid_exists(br_b)) {
						br_b = NULL;
					}

					if (channel_a && switch_true(switch_channel_get_variable(channel_a, "deny_refer_requests"))) {
						deny_refer_requests = 1;
					}

					if (!deny_refer_requests && channel_b && switch_true(switch_channel_get_variable(channel_b, "deny_refer_requests"))) {
						deny_refer_requests = 1;
					}

					if (!deny_refer_requests && br_a) {
						switch_core_session_t *a_session;
						if ((a_session = switch_core_session_locate(br_a))) {
							switch_channel_t *a_channel = switch_core_session_get_channel(a_session);

							if (a_channel && switch_true(switch_channel_get_variable(a_channel, "deny_refer_requests"))) {
								deny_refer_requests = 1;
							}
							switch_core_session_rwunlock(a_session);
						}
					}

					if (!deny_refer_requests && br_b) {
						switch_core_session_t *b_session;
						if ((b_session = switch_core_session_locate(br_b))) {
							switch_channel_t *b_channel = switch_core_session_get_channel(b_session);

							if (b_channel && switch_true(switch_channel_get_variable(b_channel, "deny_refer_requests"))) {
								deny_refer_requests = 1;
							}
							switch_core_session_rwunlock(b_session);
						}
					}

					if (deny_refer_requests) {
						switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_NOTICE, "Denying Attended Transfer, variable [deny_refer_requests] was set to true\n");

						nua_notify(tech_pvt->nh, NUTAG_NEWSUB(1), SIPTAG_CONTENT_TYPE_STR("message/sipfrag;version=2.0"),
							NUTAG_SUBSTATE(nua_substate_terminated),SIPTAG_SUBSCRIPTION_STATE_STR("terminated;reason=noresource"),
							SIPTAG_PAYLOAD_STR("SIP/2.0 403 Forbidden\r\n"), SIPTAG_EVENT_STR(etmp), TAG_END());

					} else if (switch_channel_test_flag(channel_b, CF_ORIGINATOR)) {
						switch_core_session_t *a_session;

						switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_NOTICE,
										  "Attended Transfer on originating session %s\n", switch_core_session_get_uuid(b_session));



						switch_channel_set_variable_printf(channel_b, "transfer_to", "satt:%s", br_a);

						switch_channel_set_variable(channel_b, SWITCH_ENDPOINT_DISPOSITION_VARIABLE, "ATTENDED_TRANSFER");


						sofia_clear_flag_locked(b_tech_pvt, TFLAG_SIP_HOLD);
						switch_channel_clear_flag(channel_b, CF_LEG_HOLDING);
						sofia_clear_flag_locked(tech_pvt, TFLAG_HOLD_LOCK);

						switch_channel_set_variable(channel_b, SWITCH_HOLDING_UUID_VARIABLE, br_a);
						switch_channel_set_flag(channel_b, CF_XFER_ZOMBIE);
						switch_channel_set_flag(channel_b, CF_TRANSFER);


						if ((a_session = switch_core_session_locate(br_a))) {
							const char *moh = profile->hold_music;
							switch_core_session_t *tmpsess = NULL;
							switch_channel_t *a_channel = switch_core_session_get_channel(a_session);
							switch_caller_profile_t *prof = switch_channel_get_caller_profile(channel_b);
							const char *tmp;

							switch_core_event_hook_add_state_change(a_session, xfer_hanguphook);
							switch_channel_set_variable(a_channel, "att_xfer_kill_uuid", switch_core_session_get_uuid(b_session));
							switch_channel_set_variable(a_channel, "att_xfer_destination_number", prof->destination_number);
							switch_channel_set_variable(a_channel, "att_xfer_callee_id_name", prof->callee_id_name);
							switch_channel_set_variable(a_channel, "att_xfer_callee_id_number", prof->callee_id_number);

							if (profile->media_options & MEDIA_OPT_BYPASS_AFTER_ATT_XFER) {
								switch_channel_set_flag(a_channel, CF_BYPASS_MEDIA_AFTER_BRIDGE);
							}


							if ((tmp = switch_channel_get_hold_music(a_channel))) {
								moh = tmp;
							}

							if (!zstr(moh) && !strcasecmp(moh, "silence")) {
								moh = NULL;
							}

							if(sofia_test_pflag(profile, PFLAG_FIRE_TRANFER_EVENTS)) {
								if (switch_event_create_subclass(&event, SWITCH_EVENT_CUSTOM, MY_EVENT_REPLACED) == SWITCH_STATUS_SUCCESS) {
									switch_channel_event_set_data(channel_b, event);
									switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "att_xfer_replaced_by", br_a);
									switch_event_fire(&event);
								}

								if (switch_event_create_subclass(&event, SWITCH_EVENT_CUSTOM, MY_EVENT_TRANSFEROR) == SWITCH_STATUS_SUCCESS) {
									switch_channel_event_set_data(channel_a, event);
									switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "att_xfer_original_call_id", br_a);
									switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "att_xfer_destination_call_id", br_b);
									switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "att_xfer_destination_peer_uuid", switch_core_session_get_uuid(b_session));
									switch_event_fire(&event);
								}

								if (switch_event_create_subclass(&event, SWITCH_EVENT_CUSTOM, MY_EVENT_TRANSFEREE) == SWITCH_STATUS_SUCCESS) {
									switch_channel_event_set_data(a_channel, event);
									switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "att_xfer_replaced_call_id", switch_core_session_get_uuid(b_session));
									switch_event_fire(&event);
								}
							}

							if (moh) {
								char *xdest;
								xdest = switch_core_session_sprintf(a_session, "m:\":endless_playback:%s\"park", moh);
								switch_ivr_session_transfer(a_session, xdest, "inline", NULL);
							} else {
								switch_ivr_session_transfer(a_session, "park", "inline", NULL);
							}
							if (switch_true(switch_channel_get_variable(channel_a, "recording_follow_transfer"))) {
								switch_ivr_transfer_recordings(session, a_session);
							}
							if (switch_true(switch_channel_get_variable(channel_b, "recording_follow_transfer")) && (tmpsess = switch_core_session_locate(br_a))) {
								switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_NOTICE,
										  "Early transfer detected with no media, moving recording bug to other leg\n");
								switch_ivr_transfer_recordings(b_session, tmpsess);
								switch_core_session_rwunlock(tmpsess);
							}

							switch_core_session_rwunlock(a_session);

							nua_notify(tech_pvt->nh, NUTAG_NEWSUB(1), SIPTAG_CONTENT_TYPE_STR("message/sipfrag;version=2.0"),
									   NUTAG_SUBSTATE(nua_substate_terminated),SIPTAG_SUBSCRIPTION_STATE_STR("terminated;reason=noresource"), SIPTAG_PAYLOAD_STR("SIP/2.0 200 OK\r\n"), SIPTAG_EVENT_STR(etmp),
									   TAG_END());

							if (b_tech_pvt && !sofia_test_flag(b_tech_pvt, TFLAG_BYE)) {
								char *q850 = NULL;
								const char *val = NULL;

								sofia_set_flag_locked(b_tech_pvt, TFLAG_BYE);
								val = switch_channel_get_variable(tech_pvt->channel, "disable_q850_reason");
								if (!val || switch_true(val)) {
									q850 = switch_core_session_sprintf(a_session, "Q.850;cause=16;text=\"normal_clearing\"");
								}
								nua_bye(b_tech_pvt->nh,
										SIPTAG_CONTACT(SIP_NONE),
										TAG_IF(!zstr(q850), SIPTAG_REASON_STR(q850)),
										TAG_IF(!zstr(tech_pvt->user_via), SIPTAG_VIA_STR(tech_pvt->user_via)), TAG_END());

							}
						} else {
							nua_notify(tech_pvt->nh, NUTAG_NEWSUB(1), SIPTAG_CONTENT_TYPE_STR("message/sipfrag;version=2.0"),
									   NUTAG_SUBSTATE(nua_substate_terminated),SIPTAG_SUBSCRIPTION_STATE_STR("terminated;reason=noresource"),
									   SIPTAG_PAYLOAD_STR("SIP/2.0 403 Forbidden\r\n"), SIPTAG_EVENT_STR(etmp), TAG_END());
						}

					} else if (br_a && br_b) {
						switch_core_session_t *tmp = NULL;
						switch_event_t *event = NULL;

						switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_NOTICE, "Attended Transfer [%s][%s]\n",
										  switch_str_nil(br_a), switch_str_nil(br_b));

						if ((tmp = switch_core_session_locate(br_b))) {
							switch_channel_t *tchannel = switch_core_session_get_channel(tmp);

							if ((profile->media_options & MEDIA_OPT_BYPASS_AFTER_ATT_XFER)) {
								switch_channel_set_flag(tchannel, CF_BYPASS_MEDIA_AFTER_BRIDGE);
							}

							switch_channel_set_variable(tchannel, "transfer_disposition", "bridge");

							switch_channel_set_flag(tchannel, CF_ATTENDED_TRANSFER);
							switch_core_session_rwunlock(tmp);
						}

						if ((profile->media_options & MEDIA_OPT_BYPASS_AFTER_ATT_XFER) && (tmp = switch_core_session_locate(br_a))) {
							switch_channel_t *tchannel = switch_core_session_get_channel(tmp);
							switch_channel_set_flag(tchannel, CF_BYPASS_MEDIA_AFTER_BRIDGE);
							switch_core_session_rwunlock(tmp);
						}


						if (switch_true(switch_channel_get_variable(channel_a, "recording_follow_transfer")) &&
							(tmp = switch_core_session_locate(br_a))) {
							switch_channel_set_variable(switch_core_session_get_channel(tmp), "transfer_disposition", "bridge");
							switch_ivr_transfer_recordings(session, tmp);
							switch_core_session_rwunlock(tmp);
						}


						if (switch_true(switch_channel_get_variable(channel_b, "recording_follow_transfer")) &&
							(tmp = switch_core_session_locate(br_b))) {
							switch_ivr_transfer_recordings(b_session, tmp);
							switch_core_session_rwunlock(tmp);
						}

						switch_channel_set_variable_printf(channel_a, "transfer_to", "att:%s", br_b);

						mark_transfer_record(session, br_a, br_b);

						if(sofia_test_pflag(profile, PFLAG_FIRE_TRANFER_EVENTS)) {
							if (switch_event_create_subclass(&event, SWITCH_EVENT_CUSTOM, MY_EVENT_REPLACED) == SWITCH_STATUS_SUCCESS) {
								switch_channel_event_set_data(channel_b, event);
								switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "att_xfer_replaced_by", br_a);
								switch_event_fire(&event);
							}

							if (switch_event_create_subclass(&event, SWITCH_EVENT_CUSTOM, MY_EVENT_TRANSFEROR) == SWITCH_STATUS_SUCCESS) {
								switch_channel_event_set_data(channel_a, event);
								switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "att_xfer_original_call_id", br_a);
								switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "att_xfer_destination_call_id", br_b);
								switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "att_xfer_destination_peer_uuid", switch_core_session_get_uuid(b_session));
								switch_event_fire(&event);
							}

							if ((tmp = switch_core_session_locate(br_a))) {
								switch_channel_t *tchannel = switch_core_session_get_channel(tmp);

								if (switch_event_create_subclass(&event, SWITCH_EVENT_CUSTOM, MY_EVENT_TRANSFEREE) == SWITCH_STATUS_SUCCESS) {
									switch_channel_event_set_data(tchannel, event);
									switch_event_fire(&event);
								}

								switch_core_session_rwunlock(tmp);
							}
						}

						switch_ivr_uuid_bridge(br_a, br_b);
						switch_channel_set_variable(channel_b, SWITCH_ENDPOINT_DISPOSITION_VARIABLE, "ATTENDED_TRANSFER");
						nua_notify(tech_pvt->nh, NUTAG_NEWSUB(1), SIPTAG_CONTENT_TYPE_STR("message/sipfrag;version=2.0"),
								   NUTAG_SUBSTATE(nua_substate_terminated),SIPTAG_SUBSCRIPTION_STATE_STR("terminated;reason=noresource"), SIPTAG_PAYLOAD_STR("SIP/2.0 200 OK\r\n"), SIPTAG_EVENT_STR(etmp),
								   TAG_END());

						sofia_clear_flag_locked(b_tech_pvt, TFLAG_SIP_HOLD);
						switch_channel_clear_flag(channel_b, CF_LEG_HOLDING);
						sofia_clear_flag_locked(tech_pvt, TFLAG_HOLD_LOCK);
						switch_channel_set_variable(channel_b, "park_timeout", "2:attended_transfer");
						switch_channel_set_state(channel_b, CS_PARK);
						switch_channel_wait_for_state_timeout(channel_b, CS_PARK, 5000);

					} else {
						if (!br_a && !br_b) {
							switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_WARNING,
											  "Cannot transfer channels that are not in a bridge.\n");
							nua_notify(tech_pvt->nh, NUTAG_NEWSUB(1), SIPTAG_CONTENT_TYPE_STR("message/sipfrag;version=2.0"),
									   NUTAG_SUBSTATE(nua_substate_terminated),SIPTAG_SUBSCRIPTION_STATE_STR("terminated;reason=noresource"), SIPTAG_PAYLOAD_STR("SIP/2.0 403 Forbidden\r\n"),
									   SIPTAG_EVENT_STR(etmp), TAG_END());
						} else {
							switch_core_session_t *t_session, *hup_session;
							switch_channel_t *hup_channel;
							switch_event_t *event = NULL;
							const char *ext;

							if (br_a && !br_b) {
								t_session = switch_core_session_locate(br_a);
								hup_channel = channel_b;
								hup_session = b_session;
							} else {
								private_object_t *h_tech_pvt = (private_object_t *) switch_core_session_get_private(b_session);
								t_session = switch_core_session_locate(br_b);
								hup_channel = channel_a;
								hup_session = session;
								sofia_clear_flag_locked(tech_pvt, TFLAG_SIP_HOLD);
								switch_channel_clear_flag(tech_pvt->channel, CF_LEG_HOLDING);
								sofia_clear_flag_locked(h_tech_pvt, TFLAG_SIP_HOLD);
								switch_channel_clear_flag(h_tech_pvt->channel, CF_LEG_HOLDING);
								switch_channel_hangup(channel_b, SWITCH_CAUSE_ATTENDED_TRANSFER);
							}

							if (t_session) {
								switch_channel_t *t_channel = switch_core_session_get_channel(t_session);
								const char *idest = switch_channel_get_variable(hup_channel, "inline_destination");
								ext = switch_channel_get_variable(hup_channel, "destination_number");

								if (!zstr(full_ref_by)) {
									switch_channel_set_variable(t_channel, SOFIA_SIP_HEADER_PREFIX "Referred-By", full_ref_by);
								}

								if (!zstr(full_ref_to)) {
									switch_channel_set_variable(t_channel, SOFIA_REFER_TO_VARIABLE, full_ref_to);
								}


								if (switch_true(switch_channel_get_variable(hup_channel, "recording_follow_transfer"))) {
									switch_ivr_transfer_recordings(hup_session, t_session);
								}

								if(sofia_test_pflag(profile, PFLAG_FIRE_TRANFER_EVENTS)) {
									if (switch_event_create_subclass(&event, SWITCH_EVENT_CUSTOM, MY_EVENT_TRANSFEROR) == SWITCH_STATUS_SUCCESS) {
										switch_channel_event_set_data(channel_a, event);
										switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "att_xfer_original_call_id", switch_core_session_get_uuid(hup_session));
										switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "att_xfer_destination_call_id", switch_core_session_get_uuid(t_session));
										switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "att_xfer_destination_peer_uuid", switch_channel_get_partner_uuid(t_channel));
										switch_event_fire(&event);
									}

									if (switch_event_create_subclass(&event, SWITCH_EVENT_CUSTOM, MY_EVENT_TRANSFEREE) == SWITCH_STATUS_SUCCESS) {
										switch_channel_event_set_data(t_channel, event);
										switch_event_fire(&event);
									}
								}

								if (idest) {
									switch_ivr_session_transfer(t_session, idest, "inline", NULL);
								} else {
									switch_ivr_session_transfer(t_session, ext, NULL, NULL);
								}

								nua_notify(tech_pvt->nh,
										   NUTAG_NEWSUB(1),
										   SIPTAG_CONTENT_TYPE_STR("message/sipfrag;version=2.0"),
										   NUTAG_SUBSTATE(nua_substate_terminated),SIPTAG_SUBSCRIPTION_STATE_STR("terminated;reason=noresource"),
										   SIPTAG_PAYLOAD_STR("SIP/2.0 200 OK\r\n"), SIPTAG_EVENT_STR(etmp), TAG_END());
								switch_core_session_rwunlock(t_session);
								switch_channel_hangup(hup_channel, SWITCH_CAUSE_ATTENDED_TRANSFER);
							} else {
								switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "Session to transfer to not found.\n");
								nua_notify(tech_pvt->nh, NUTAG_NEWSUB(1), SIPTAG_CONTENT_TYPE_STR("message/sipfrag;version=2.0"),
										   NUTAG_SUBSTATE(nua_substate_terminated),SIPTAG_SUBSCRIPTION_STATE_STR("terminated;reason=noresource"),
										   SIPTAG_PAYLOAD_STR("SIP/2.0 403 Forbidden\r\n"), SIPTAG_EVENT_STR(etmp), TAG_END());
							}
						}
					}
					if (b_session) {
						switch_core_session_rwunlock(b_session);
					}
				}
				nua_handle_unref(bnh);
			} else {		/* the other channel is on a different box, we have to go find them */
				if (exten && (br_a = switch_channel_get_partner_uuid(channel_a))) {
					switch_core_session_t *a_session;
					switch_channel_t *channel = switch_core_session_get_channel(session);

					if ((a_session = switch_core_session_locate(br_a))) {
						const char *port = NULL;
						const char *rep_h = NULL;

						switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG,
										  "REFER from %s replaces %s (%s@%s) with %s on another server\n",
										  switch_core_session_get_uuid(session), rep, exten, (char *) refer_to->r_url->url_host, br_a);

						if (refer_to && refer_to->r_url->url_port) {
							port = refer_to->r_url->url_port;
						}

						channel = switch_core_session_get_channel(a_session);

						exten = NULL;

						if (sofia_test_pflag(profile, PFLAG_CHANNEL_XML_FETCH_ON_NIGHTMARE_TRANSFER)) {
							switch_xml_t xml_root = NULL, xml_channel = NULL;
							switch_event_t *xml_params = NULL;
							const char *xml_url = NULL, *use_profile = profile->name, *dial_prefix = NULL, *absolute_dial_string = NULL;
							switch_xml_t params = NULL, param = NULL;

							switch_event_create(&xml_params, SWITCH_EVENT_REQUEST_PARAMS);
							switch_event_add_header_string(xml_params, SWITCH_STACK_BOTTOM, "purpose", "nightmare_xfer");
							switch_event_add_header_string(xml_params, SWITCH_STACK_BOTTOM, "profile", profile->name);
							switch_event_add_header_string(xml_params, SWITCH_STACK_BOTTOM, "refer-to-user", refer_to->r_url->url_user);
							switch_event_add_header_string(xml_params, SWITCH_STACK_BOTTOM, "refer-to-host", refer_to->r_url->url_host);
							switch_event_add_header_string(xml_params, SWITCH_STACK_BOTTOM, "refer-to-params", refer_to->r_url->url_params ? refer_to->r_url->url_params : "");
							switch_event_add_header_string(xml_params, SWITCH_STACK_BOTTOM, "refer-to-headers", refer_to->r_url->url_headers ? refer_to->r_url->url_headers : "");
							switch_event_add_header_string(xml_params, SWITCH_STACK_BOTTOM, "replaces-call-id", replaces->rp_call_id);
							switch_event_add_header_string(xml_params, SWITCH_STACK_BOTTOM, "refer-from-channel-id", switch_core_session_get_uuid(session));
							switch_event_add_header_string(xml_params, SWITCH_STACK_BOTTOM, "refer-for-channel-id", br_a);

							if (switch_xml_locate("channels", NULL, NULL, NULL,
												  &xml_root, &xml_channel, xml_params, SWITCH_FALSE) == SWITCH_STATUS_SUCCESS) {
								if ((params = switch_xml_child(xml_channel, "params"))) {
									for (param = switch_xml_child(params, "param"); param; param = param->next) {
										const char *name = switch_xml_attr(param, "name");
										const char *value = switch_xml_attr(param, "value");

										if (!(name && value)) continue;

										if (!strcasecmp(name, "sip-url")) {
											xml_url = value;
										} else if (!strcasecmp(name, "dial-prefix")) {
											dial_prefix = value;
										} else if (!strcasecmp(name, "absolute-dial-string")) {
											absolute_dial_string = value;
										} else if (!strcasecmp(name, "sip-profile")) {
											use_profile = value;
										}
									}
								}

								if (absolute_dial_string) {
									exten = switch_core_session_sprintf(session, "%s%s", dial_prefix, absolute_dial_string);
								} else if (xml_url) {
									exten = switch_core_session_sprintf(session, "%ssofia/%s/%s", dial_prefix, use_profile, xml_url);
								}

								switch_xml_free(xml_root);
							}

							switch_event_destroy(&xml_params);
						}


						if (zstr(exten)) {
							exten = switch_core_session_sprintf(session, "sofia/%s/sip:%s@%s%s%s",
																profile->name, refer_to->r_url->url_user,
																refer_to->r_url->url_host, port ? ":" : "", port ? port : "");
						}

						switch_core_new_memory_pool(&npool);
						nightmare_xfer_helper = switch_core_alloc(npool, sizeof(*nightmare_xfer_helper));
						nightmare_xfer_helper->exten = switch_core_strdup(npool, exten);

						if (refer_to->r_url->url_params || refer_to->r_url->url_headers) {
							if (refer_to->r_url->url_headers) {
								nightmare_xfer_helper->exten_with_params = switch_core_sprintf(npool,
																							   "{sip_invite_params=%s?%s}%s",
																							   refer_to->r_url->url_params ? refer_to->r_url->
																							   url_params : "", refer_to->r_url->url_headers, exten);
							} else {
								nightmare_xfer_helper->exten_with_params = switch_core_sprintf(npool,
																							   "{sip_invite_params=%s}%s", refer_to->r_url->url_params,
																							   exten);
							}
						} else {
							nightmare_xfer_helper->exten_with_params = nightmare_xfer_helper->exten;
						}

						switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "Nightmare transfer to '%s'\n", nightmare_xfer_helper->exten_with_params);

						nightmare_xfer_helper->event = switch_core_strdup(npool, etmp);
						nightmare_xfer_helper->reply_uuid = switch_core_strdup(npool, switch_core_session_get_uuid(session));
						nightmare_xfer_helper->bridge_to_uuid = switch_core_strdup(npool, br_a);
						nightmare_xfer_helper->pool = npool;

						if (refer_to->r_url->url_headers) {
							char *h, *v, *hp;
							p = switch_core_session_strdup(session, refer_to->r_url->url_headers);
							while (p && *p) {
								h = p;
								if ((p = strchr(p, '='))) {
									*p++ = '\0';
									v = p;
									if ((p = strchr(p, '&'))) {
										*p++ = '\0';
									}

									url_unescape(h, (const char *) h);
									url_unescape(v, (const char *) v);
									if (strcasecmp("Replaces", h)) {
										hp = switch_core_session_sprintf(session, "%s%s", SOFIA_SIP_HEADER_PREFIX, h);
										switch_channel_set_variable(channel, hp, v);
									} else {
										// use this one instead of rep value from above to keep all parameters
										switch_channel_set_variable(channel, SOFIA_REPLACES_HEADER, v);
									}
									switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "Exporting replaces URL header [%s:%s]\n",
													  h, v);
								}
							}
						}


						switch_event_create(&nightmare_xfer_helper->vars, SWITCH_EVENT_CHANNEL_DATA);

						rep_h = switch_channel_get_variable(channel, SOFIA_REPLACES_HEADER);
						if (rep_h) {
							switch_event_add_header_string(nightmare_xfer_helper->vars, SWITCH_STACK_BOTTOM, SOFIA_REPLACES_HEADER, rep_h);
						} else {
							switch_event_add_header_string(nightmare_xfer_helper->vars, SWITCH_STACK_BOTTOM, SOFIA_REPLACES_HEADER, rep);
						}


						if (!zstr(full_ref_by)) {
							switch_event_add_header_string(nightmare_xfer_helper->vars, SWITCH_STACK_BOTTOM, "Referred-By", full_ref_by);
						}

						if (!zstr(full_ref_to)) {
							switch_event_add_header_string(nightmare_xfer_helper->vars, SWITCH_STACK_BOTTOM, SOFIA_REFER_TO_VARIABLE, full_ref_to);
						}

						if(sofia_test_pflag(profile, PFLAG_FIRE_TRANFER_EVENTS)) {
							if (switch_event_create_subclass(&event, SWITCH_EVENT_CUSTOM, MY_EVENT_TRANSFEROR) == SWITCH_STATUS_SUCCESS) {
								switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "att_xfer_original_call_id", br_a);
								switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "att_xfer_destination_call_id", br_b);
								switch_channel_event_set_data(channel_a, event);
								switch_event_fire(&event);
							}

							if (switch_event_create_subclass(&event, SWITCH_EVENT_CUSTOM, MY_EVENT_TRANSFEREE) == SWITCH_STATUS_SUCCESS) {
								switch_channel_event_set_data(channel, event);
								switch_event_fire(&event);
							}
						}

						switch_event_add_header_string(nightmare_xfer_helper->vars, SWITCH_STACK_BOTTOM, "sip_h_X-FS-Refer-From", switch_core_session_get_uuid(session));
						switch_event_add_header_string(nightmare_xfer_helper->vars, SWITCH_STACK_BOTTOM, "sip_h_X-FS-Refer-For", br_a);

						switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "Good Luck, you'll need it......\n");
						launch_nightmare_xfer(nightmare_xfer_helper);

						switch_core_session_rwunlock(a_session);

					} else {
						goto error;
					}

				} else {
				  error:
					switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "Invalid Transfer! [%s]\n", br_a);
					switch_channel_set_variable(channel_a, SWITCH_ENDPOINT_DISPOSITION_VARIABLE, "ATTENDED_TRANSFER_ERROR");
					nua_notify(tech_pvt->nh, NUTAG_NEWSUB(1), SIPTAG_CONTENT_TYPE_STR("message/sipfrag;version=2.0"),
							   NUTAG_SUBSTATE(nua_substate_terminated),SIPTAG_SUBSCRIPTION_STATE_STR("terminated;reason=noresource"), SIPTAG_PAYLOAD_STR("SIP/2.0 403 Forbidden\r\n"), SIPTAG_EVENT_STR(etmp),
							   TAG_END());
				}
			}
			goto done;
		}

	} else {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "Missing Refer-To\n");
		goto done;
	}

	if (exten) {
		switch_channel_t *channel = switch_core_session_get_channel(session);
		const char *br = switch_channel_get_partner_uuid(channel);
		switch_core_session_t *b_session;

		switch_channel_set_variable_printf(channel, "transfer_to", "blind:%s", br ? br : exten);
		switch_channel_set_variable_printf(channel, "transfer_destination", "blind:%s", exten);

		if (!zstr(br) && (b_session = switch_core_session_locate(br))) {
			const char *var;
			switch_channel_t *b_channel = switch_core_session_get_channel(b_session);
			switch_event_t *event = NULL;

			switch_channel_set_variable(channel, "transfer_fallback_extension", from->a_user);
			if (!zstr(full_ref_by)) {
				switch_channel_set_variable(b_channel, SOFIA_SIP_HEADER_PREFIX "Referred-By", full_ref_by);
			}

			if (!zstr(full_ref_to)) {
				switch_channel_set_variable(b_channel, SOFIA_REFER_TO_VARIABLE, full_ref_to);
			}

			if (switch_true(switch_channel_get_variable(channel, "recording_follow_transfer"))) {
				switch_ivr_transfer_recordings(session, b_session);
			}

			switch_channel_set_variable(channel, SWITCH_ENDPOINT_DISPOSITION_VARIABLE, "BLIND_TRANSFER");

			if (((var = switch_channel_get_variable(channel, "confirm_blind_transfer")) && switch_true(var)) ||
				sofia_test_pflag(profile, PFLAG_CONFIRM_BLIND_TRANSFER)) {

				switch_channel_set_state_flag(b_channel, CF_CONFIRM_BLIND_TRANSFER);
				switch_channel_set_variable(channel, "sip_blind_transfer_event", etmp);
				switch_channel_set_variable(b_channel, "blind_transfer_uuid", switch_core_session_get_uuid(session));
				switch_channel_set_variable(channel, "blind_transfer_uuid", switch_core_session_get_uuid(b_session));

				switch_channel_set_variable(channel, "park_timeout", "600:blind_transfer");
				switch_channel_set_state(channel, CS_PARK);
			} else {
				nua_notify(tech_pvt->nh, NUTAG_NEWSUB(1), SIPTAG_CONTENT_TYPE_STR("message/sipfrag;version=2.0"),
						   NUTAG_SUBSTATE(nua_substate_terminated),
						   SIPTAG_SUBSCRIPTION_STATE_STR("terminated;reason=noresource"),
						   SIPTAG_PAYLOAD_STR("SIP/2.0 200 OK\r\n"), SIPTAG_EVENT_STR(etmp), TAG_END());
			}

            if (refer_to->r_url->url_params) {
                switch_channel_set_variable(b_channel, "sip_h_X-FS-Refer-Params", refer_to->r_url->url_params);
            }

            if(sofia_test_pflag(profile, PFLAG_FIRE_TRANFER_EVENTS)) {
	            if (switch_event_create_subclass(&event, SWITCH_EVENT_CUSTOM, MY_EVENT_TRANSFEROR) == SWITCH_STATUS_SUCCESS) {
	                switch_channel_event_set_data(channel_a, event);
	                switch_event_fire(&event);
	            }

				if (switch_event_create_subclass(&event, SWITCH_EVENT_CUSTOM, MY_EVENT_TRANSFEREE) == SWITCH_STATUS_SUCCESS) {
	                switch_channel_event_set_data(b_channel, event);
	                switch_event_fire(&event);
	            }
            }

			switch_ivr_session_transfer(b_session, exten, NULL, NULL);
			switch_core_session_rwunlock(b_session);
		} else {
			switch_event_t *event;

			if (switch_event_create_subclass(&event, SWITCH_EVENT_CUSTOM, MY_EVENT_ERROR) == SWITCH_STATUS_SUCCESS) {
				switch_event_add_header(event, SWITCH_STACK_BOTTOM, "Error-Type", "blind_transfer");
				switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "Transfer-Exten", exten);
				switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "Full-Refer-To", full_ref_to);
				switch_channel_event_set_data(channel, event);
				switch_event_fire(&event);
			}

			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "Cannot Blind Transfer 1 Legged calls\n");
			switch_channel_set_variable(channel_a, SWITCH_ENDPOINT_DISPOSITION_VARIABLE, "ATTENDED_TRANSFER_ERROR");
			nua_notify(tech_pvt->nh, NUTAG_NEWSUB(1), SIPTAG_CONTENT_TYPE_STR("message/sipfrag;version=2.0"),
					   NUTAG_SUBSTATE(nua_substate_terminated),
					   SIPTAG_SUBSCRIPTION_STATE_STR("terminated;reason=noresource"),
					   SIPTAG_PAYLOAD_STR("SIP/2.0 403 Forbidden\r\n"), SIPTAG_EVENT_STR(etmp), TAG_END());
		}
	}

  done:
	if (home) {
		su_home_unref(home);
		home = NULL;
	}

	switch_safe_free(etmp);
}


static switch_status_t create_info_event(sip_t const *sip,
										 nua_handle_t *nh, switch_event_t **revent)
{
	sip_alert_info_t *alert_info = sip_alert_info(sip);
	switch_event_t *event;

	if (!(sip && switch_event_create(&event, SWITCH_EVENT_RECV_INFO) == SWITCH_STATUS_SUCCESS)) {
		return SWITCH_STATUS_FALSE;
	}

	if (sip && sip->sip_content_type) {
		switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "SIP-Content-Type", sip->sip_content_type->c_type);
	}

	if (sip->sip_from) {
		if (sip->sip_from->a_url->url_user) {
			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "SIP-From-User", sip->sip_from->a_url->url_user);
		}

		if (sip->sip_from->a_url->url_host) {
			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "SIP-From-Host", sip->sip_from->a_url->url_host);
		}
	}

	if (sip->sip_to) {
		if (sip->sip_to->a_url->url_user) {
			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "SIP-To-User", sip->sip_to->a_url->url_user);
		}

		if (sip->sip_to->a_url->url_host) {
			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "SIP-To-Host", sip->sip_to->a_url->url_host);
		}
	}


	if (sip->sip_contact) {
		if (sip->sip_contact->m_url->url_user) {
			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "SIP-Contact-User", sip->sip_contact->m_url->url_user);
		}

		if (sip->sip_contact->m_url->url_host) {
			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "SIP-Contact-Host", sip->sip_contact->m_url->url_host);
		}
	}


	if (sip->sip_call_info) {
		switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "Call-Info",
									   sip_header_as_string(nua_handle_home(nh), (void *) sip->sip_call_info));
	}

	if (alert_info) {
		switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "Alert-Info", sip_header_as_string(nua_handle_home(nh), (void *) alert_info));
	}


	if (sip->sip_payload && sip->sip_payload->pl_data) {
		switch_event_add_body(event, "%s", sip->sip_payload->pl_data);
	}

	*revent = event;

	return SWITCH_STATUS_SUCCESS;
}

switch_status_t sofia_proxy_sip_i_message(nua_t *nua, sofia_profile_t *profile, nua_handle_t *nh, switch_core_session_t *session, sip_t const *sip,
									   sofia_dispatch_event_t *de, tagi_t tags[])
{
	switch_core_session_t *other_session = NULL;

	if (session && switch_core_session_get_partner(session, &other_session) == SWITCH_STATUS_SUCCESS) {
		if (switch_core_session_compare(session, other_session)) {
			private_object_t *other_tech_pvt = NULL;
			const char *ct = NULL;
			char *pl = NULL;


			if (sip && sip->sip_payload && sip->sip_payload->pl_data) {
				pl = sip->sip_payload->pl_data;
			}

			other_tech_pvt = (private_object_t *) switch_core_session_get_private(other_session);

			if (sip->sip_content_type->c_type && sip->sip_content_type->c_subtype) {
				ct = sip->sip_content_type->c_type;
			}

			nua_message(other_tech_pvt->nh,
					 TAG_IF(ct, SIPTAG_CONTENT_TYPE_STR(su_strdup(other_tech_pvt->nh->nh_home, ct))),
					 TAG_IF(!zstr(other_tech_pvt->user_via), SIPTAG_VIA_STR(other_tech_pvt->user_via)),
					 TAG_IF(pl, SIPTAG_PAYLOAD_STR(su_strdup(other_tech_pvt->nh->nh_home, pl))),
					 TAG_END());
		}

		switch_core_session_rwunlock(other_session);

		nua_respond(nh, SIP_202_ACCEPTED, NUTAG_WITH_THIS_MSG(de->data->e_msg), TAG_END());

		return SWITCH_STATUS_SUCCESS;
	}

	return SWITCH_STATUS_FALSE;
}

switch_status_t sofia_proxy_sip_i_info(nua_t *nua, sofia_profile_t *profile, nua_handle_t *nh, switch_core_session_t *session, sip_t const *sip,
								sofia_dispatch_event_t *de, tagi_t tags[])
{
	switch_core_session_t *other_session = NULL;

	if (session && switch_core_session_get_partner(session, &other_session) == SWITCH_STATUS_SUCCESS) {
		if (switch_core_session_compare(session, other_session)) {
			private_object_t *other_tech_pvt = NULL;
			const char *ct = NULL;
			char *pl = NULL;
			switch_channel_t *channel = switch_core_session_get_channel(session);

			if (sip && sip->sip_payload && sip->sip_payload->pl_data) {
				pl = sip->sip_payload->pl_data;
			}

			other_tech_pvt = (private_object_t *) switch_core_session_get_private(other_session);

			if (sip->sip_content_type->c_type && sip->sip_content_type->c_subtype) {
				ct = sip->sip_content_type->c_type;
			}

			if (!strncasecmp(sip->sip_content_type->c_type, "application", 11) && !strcasecmp(sip->sip_content_type->c_subtype, "media_control+xml")) {
				if (switch_channel_test_flag(channel, CF_VIDEO)) {
					switch_core_media_gen_key_frame(session);
					switch_channel_set_flag(channel, CF_VIDEO_REFRESH_REQ);
				}
			}

			nua_info(other_tech_pvt->nh,
					 TAG_IF(ct, SIPTAG_CONTENT_TYPE_STR(su_strdup(other_tech_pvt->nh->nh_home, ct))),
					 TAG_IF(!zstr(other_tech_pvt->user_via), SIPTAG_VIA_STR(other_tech_pvt->user_via)),
					 TAG_IF(pl, SIPTAG_PAYLOAD_STR(su_strdup(other_tech_pvt->nh->nh_home, pl))),
					 TAG_END());
		}

		switch_core_session_rwunlock(other_session);

		nua_respond(nh, SIP_200_OK, NUTAG_WITH_THIS_MSG(de->data->e_msg), TAG_END());

		return SWITCH_STATUS_SUCCESS;
	}

	return SWITCH_STATUS_FALSE;
}


void sofia_handle_sip_i_info(nua_t *nua, sofia_profile_t *profile, nua_handle_t *nh, switch_core_session_t *session, sip_t const *sip,
								sofia_dispatch_event_t *de, tagi_t tags[])
{
	/* placeholder for string searching */
	const char *signal_ptr;
	const char *rec_header;
	const char *clientcode_header;
	switch_dtmf_t dtmf = { 0, switch_core_default_dtmf_duration(0), 0, SWITCH_DTMF_ENDPOINT };
	switch_event_t *event;
	private_object_t *tech_pvt = NULL;
	switch_channel_t *channel = NULL;

	if (session) {
		tech_pvt = (private_object_t *) switch_core_session_get_private(session);
		channel = switch_core_session_get_channel(session);
	}

	if (sofia_test_pflag(profile, PFLAG_EXTENDED_INFO_PARSING)) {
		if (sip && sip->sip_content_type && sip->sip_content_type->c_type && sip->sip_content_type->c_subtype &&
			sip->sip_payload && sip->sip_payload->pl_data) {

			if (!strncasecmp(sip->sip_content_type->c_type, "freeswitch", 10)) {

				if (!strcasecmp(sip->sip_content_type->c_subtype, "session-event")) {
					if (session) {
						if (create_info_event(sip, nh, &event) == SWITCH_STATUS_SUCCESS) {
							if (switch_core_session_queue_event(session, &event) == SWITCH_STATUS_SUCCESS) {
								switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "queued freeswitch event for INFO\n");
								nua_respond(nh, SIP_200_OK, SIPTAG_CONTENT_TYPE_STR("freeswitch/session-event-response"),
											SIPTAG_PAYLOAD_STR("+OK MESSAGE QUEUED"), NUTAG_WITH_THIS_MSG(de->data->e_msg), TAG_END());
							} else {
								switch_event_destroy(&event);
								nua_respond(nh, SIP_200_OK, SIPTAG_CONTENT_TYPE_STR("freeswitch/session-event-response"),
											SIPTAG_PAYLOAD_STR("-ERR MESSAGE NOT QUEUED"), NUTAG_WITH_THIS_MSG(de->data->e_msg), TAG_END());
							}
						}

					} else {
						nua_respond(nh, SIP_200_OK, SIPTAG_CONTENT_TYPE_STR("freeswitch/session-event-response"),
									SIPTAG_PAYLOAD_STR("-ERR INVALID SESSION"), NUTAG_WITH_THIS_MSG(de->data->e_msg), TAG_END());

					}

					return;

				} else if (!strcasecmp(sip->sip_content_type->c_subtype, "api-request")) {
					char *cmd = strdup(sip->sip_payload->pl_data);
					char *arg;
					switch_stream_handle_t stream = { 0 };
					switch_status_t status;

					SWITCH_STANDARD_STREAM(stream);
					switch_assert(stream.data);

					if ((arg = strchr(cmd, ':'))) {
						*arg++ = '\0';
					}

					if ((status = switch_api_execute(cmd, arg, NULL, &stream)) == SWITCH_STATUS_SUCCESS) {
						nua_respond(nh, SIP_200_OK, SIPTAG_CONTENT_TYPE_STR("freeswitch/api-response"),
									SIPTAG_PAYLOAD_STR(stream.data), NUTAG_WITH_THIS_MSG(de->data->e_msg), TAG_END());
					} else {
						nua_respond(nh, SIP_200_OK, SIPTAG_CONTENT_TYPE_STR("freeswitch/api-response"),
									SIPTAG_PAYLOAD_STR("-ERR INVALID COMMAND"), NUTAG_WITH_THIS_MSG(de->data->e_msg), TAG_END());
					}

					switch_safe_free(stream.data);
					switch_safe_free(cmd);
					return;
				}

				nua_respond(nh, SIP_200_OK, NUTAG_WITH_THIS_MSG(de->data->e_msg), TAG_END());

				return;
			}
		}
	}

	if (session) {
		const char *vval;

		/* Barf if we didn't get our private */
		assert(switch_core_session_get_private(session));

		sofia_glue_set_extra_headers(session, sip, SOFIA_SIP_INFO_HEADER_PREFIX);
		if (!zstr(profile->proxy_info_content_types) && sip && sip->sip_content_type && sip->sip_content_type->c_type && sip->sip_content_type->c_subtype &&
			(!strcasecmp(profile->proxy_info_content_types,"all") || strstr(profile->proxy_info_content_types,sip->sip_content_type->c_type))) {
			switch_core_session_t *other_session;

			if (switch_core_session_get_partner(session, &other_session) == SWITCH_STATUS_SUCCESS) {
				char *pl = NULL;
				char *ct = NULL;
				char *extra_headers = NULL;
				char *unknown = NULL;
				private_object_t *other_tech_pvt = switch_core_session_get_private(other_session);

				ct = switch_core_session_strdup(other_session, (char*)sip->sip_content_type->c_type);
				if (sip->sip_payload && sip->sip_payload->pl_data) {
					pl = switch_core_session_strdup(other_session,(char*)sip->sip_payload->pl_data);
				}
				unknown = sofia_glue_get_non_extra_unknown_headers(sip);

                                extra_headers = sofia_glue_get_extra_headers(channel, SOFIA_SIP_INFO_HEADER_PREFIX);

				nua_info(other_tech_pvt->nh,
						SIPTAG_CONTENT_TYPE_STR(ct),
						TAG_IF(!zstr(extra_headers), SIPTAG_HEADER_STR(extra_headers)),
						TAG_IF(!zstr(unknown), SIPTAG_HEADER_STR(unknown)),
						TAG_IF(!zstr(other_tech_pvt->user_via), SIPTAG_VIA_STR(other_tech_pvt->user_via)),
						TAG_IF(!zstr(pl), SIPTAG_PAYLOAD_STR(pl)),
						TAG_END());
				switch_safe_free(extra_headers);
				switch_safe_free(unknown);
				switch_core_session_rwunlock(other_session);
			}
		}

		if (sip && sip->sip_content_type && sip->sip_content_type->c_type && !strcasecmp(sip->sip_content_type->c_type, "freeswitch/data")) {
			char *data = NULL;

			if (sip->sip_payload && sip->sip_payload->pl_data) {
				data = sip->sip_payload->pl_data;
			}

			if ((vval = switch_channel_get_variable(channel, "sip_copy_custom_headers")) && switch_true(vval)) {
				switch_core_session_t *nsession = NULL;

				switch_core_session_get_partner(session, &nsession);

				if (nsession) {
					switch_core_session_message_t *msg;

					switch_ivr_transfer_variable(session, nsession, SOFIA_SIP_INFO_HEADER_PREFIX_T);
					msg = switch_core_session_alloc(nsession, sizeof(*msg));
					MESSAGE_STAMP_FFL(msg);
					msg->message_id = SWITCH_MESSAGE_INDICATE_INFO;

					msg->string_array_arg[2] = switch_core_session_strdup(nsession, data);

					msg->from = __FILE__;
					switch_core_session_queue_message(nsession, msg);

					switch_core_session_rwunlock(nsession);
				}
			}
		}

		if (sip && sip->sip_content_type && sip->sip_content_type->c_subtype && sip->sip_content_type->c_type &&
			!strncasecmp(sip->sip_content_type->c_type, "message", 7) &&
			!strcasecmp(sip->sip_content_type->c_subtype, "update_display")) {
			sofia_update_callee_id(session, profile, sip, SWITCH_TRUE);
			goto end;
		}

		if (sip && sip->sip_content_type && sip->sip_content_type->c_type && sip->sip_content_type->c_subtype &&
			sip->sip_payload && sip->sip_payload->pl_data) {
			if (!strncasecmp(sip->sip_content_type->c_type, "application", 11) && !strcasecmp(sip->sip_content_type->c_subtype, "media_control+xml")) {

				if (switch_channel_test_flag(channel, CF_VIDEO)) {
					switch_core_media_gen_key_frame(session);
					switch_channel_set_flag(channel, CF_VIDEO_REFRESH_REQ);
				}

			} else if (!strncasecmp(sip->sip_content_type->c_type, "application", 11) &&
					   !strcasecmp(sip->sip_content_type->c_subtype, "vnd.nortelnetworks.digits")) {
				int tmp;
				if ((signal_ptr = switch_stristr("d=", sip->sip_payload->pl_data))) {
					signal_ptr = signal_ptr + 2;

					while (*signal_ptr && *signal_ptr == ' ') {
						signal_ptr++;
					}

					if (*signal_ptr	&& (*signal_ptr == '*' || *signal_ptr == '#' || *signal_ptr == 'A' || *signal_ptr == 'B'
										|| *signal_ptr == 'C' || *signal_ptr == 'D')) {
						dtmf.digit = *signal_ptr;
					} else {
						tmp = atoi(signal_ptr);
						dtmf.digit = switch_rfc2833_to_char(tmp);
					}

					dtmf.duration = 100;
				} else {
					switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "Bad signal\n");
					goto end;
				}
			} else if (!strncasecmp(sip->sip_content_type->c_type, "application", 11) && !strcasecmp(sip->sip_content_type->c_subtype, "dtmf-relay")) {
				/* Try and find signal information in the payload */
				if ((signal_ptr = switch_stristr("Signal=", sip->sip_payload->pl_data))) {
					int tmp;
					/* move signal_ptr where we need it (right past Signal=) */
					signal_ptr = signal_ptr + 7;

					/* handle broken devices with spaces after the = (cough) VegaStream (cough) */
					while (*signal_ptr && *signal_ptr == ' ')
						signal_ptr++;

					if (*signal_ptr
						&& (*signal_ptr == '*' || *signal_ptr == '#' || *signal_ptr == 'A' || *signal_ptr == 'B' || *signal_ptr == 'C'
							|| *signal_ptr == 'D')) {
						dtmf.digit = *signal_ptr;
					} else {
						tmp = atoi(signal_ptr);
						dtmf.digit = switch_rfc2833_to_char(tmp);
					}
				} else {
					switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "Bad signal\n");
					goto end;
				}

				if ((signal_ptr = switch_stristr("Duration=", sip->sip_payload->pl_data))) {
					int tmp;
					signal_ptr += 9;

					/* handle broken devices with spaces after the = (cough) VegaStream (cough) */
					while (*signal_ptr && *signal_ptr == ' ')
						signal_ptr++;

					if ((tmp = atoi(signal_ptr)) <= 0) {
						tmp = switch_core_default_dtmf_duration(0);
					}
					dtmf.duration = tmp * 8;
				}
			} else if (!strncasecmp(sip->sip_content_type->c_type, "application", 11) && !strcasecmp(sip->sip_content_type->c_subtype, "dtmf")) {
				int tmp = atoi(sip->sip_payload->pl_data);
				dtmf.digit = switch_rfc2833_to_char(tmp);
			}

			if (dtmf.digit) {
				if (tech_pvt->mparams.dtmf_type == DTMF_INFO ||
						sofia_test_pflag(tech_pvt->profile, PFLAG_LIBERAL_DTMF) || switch_channel_test_flag(tech_pvt->channel, CF_LIBERAL_DTMF)) {
					/* queue it up */
					switch_channel_queue_dtmf(channel, &dtmf);

					/* print debug info */
					switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "INFO DTMF(%c)\n", dtmf.digit);

					if (switch_channel_test_flag(channel, CF_PROXY_MODE)) {
						const char *uuid;
						switch_core_session_t *session_b;

						if ((uuid = switch_channel_get_partner_uuid(channel)) && (session_b = switch_core_session_locate(uuid))) {
							while (switch_channel_has_dtmf(channel)) {
								switch_dtmf_t idtmf = { 0, 0 };
								if (switch_channel_dequeue_dtmf(channel, &idtmf) == SWITCH_STATUS_SUCCESS) {
									switch_core_session_send_dtmf(session_b, &idtmf);
								}
							}

							switch_core_session_rwunlock(session_b);
						}
					}

					/* Send 200 OK response */
					nua_respond(nh, SIP_200_OK, NUTAG_WITH_THIS_MSG(de->data->e_msg), TAG_END());
				} else {
					switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_WARNING,
									  "IGNORE INFO DTMF(%c) (This channel was not configured to use INFO DTMF!)\n", dtmf.digit);
				}
				goto end;
			}
		}

		if ((clientcode_header = sofia_glue_get_unknown_header(sip, "x-clientcode"))) {
			if (!zstr(clientcode_header)) {
				switch_channel_set_variable(channel, "call_clientcode", clientcode_header);
				switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_NOTICE, "Setting CMC to %s\n", clientcode_header);
				nua_respond(nh, SIP_200_OK, NUTAG_WITH_THIS_MSG(de->data->e_msg), TAG_END());
			}
			goto end;
		}

		if ((rec_header = sofia_glue_get_unknown_header(sip, "record"))) {
			if (zstr(profile->record_template)) {
				switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_WARNING, "Record attempted but no template defined.\n");
				nua_respond(nh, 488, "Recording not enabled", NUTAG_WITH_THIS_MSG(de->data->e_msg), TAG_END());
			} else {
				if (!strcasecmp(rec_header, "on")) {
					char *file = NULL, *tmp = NULL;

					if (switch_true(switch_channel_get_variable(channel, "sip_disable_recording"))) {
						switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_WARNING, "Record attempted but is disabled by sip_disable_recording variable.\n");
						nua_respond(nh, 488, "Recording disabled for this channel", NUTAG_WITH_THIS_MSG(de->data->e_msg), TAG_END());
					} else {

						tmp = switch_mprintf("%s%s%s", profile->record_path ? profile->record_path : "${recordings_dir}",
								SWITCH_PATH_SEPARATOR, profile->record_template);
						file = switch_channel_expand_variables(channel, tmp);
						switch_ivr_record_session(session, file, 0, NULL);
						switch_channel_set_variable(channel, "sofia_record_file", file);
						switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_NOTICE, "Recording %s to %s\n", switch_channel_get_name(channel),
								file);
						switch_safe_free(tmp);
						nua_respond(nh, SIP_200_OK, NUTAG_WITH_THIS_MSG(de->data->e_msg), TAG_END());
						if (file != profile->record_template) {
							free(file);
							file = NULL;
						}
					}
				} else {
					const char *file;

					if ((file = switch_channel_get_variable(channel, "sofia_record_file"))) {
						switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_NOTICE, "Done recording %s to %s\n",
										  switch_channel_get_name(channel), file);
						switch_ivr_stop_record_session(session, file);
						nua_respond(nh, SIP_200_OK, NUTAG_WITH_THIS_MSG(de->data->e_msg), TAG_END());
					} else {
						nua_respond(nh, 488, "Nothing to stop", NUTAG_WITH_THIS_MSG(de->data->e_msg), TAG_END());
					}
				}
			}
		}
	}

  end:

	if (create_info_event(sip, nh, &event) == SWITCH_STATUS_SUCCESS) {
		if (channel) {
			switch_channel_event_set_data(channel, event);
		}
		switch_event_fire(&event);
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG1, "dispatched freeswitch event for INFO\n");
	}

	nua_respond(nh, SIP_200_OK, NUTAG_WITH_THIS_MSG(de->data->e_msg), TAG_END());

	return;

}

void sofia_handle_sip_i_reinvite(switch_core_session_t *session,
								 nua_t *nua, sofia_profile_t *profile, nua_handle_t *nh, sofia_private_t *sofia_private, sip_t const *sip,
								sofia_dispatch_event_t *de,
								 tagi_t tags[])
{
	char *call_info = NULL;
	switch_channel_t *channel = NULL;
	private_object_t *tech_pvt = NULL;

	nua_respond(nh, SIP_100_TRYING, TAG_END());

	if (session) {
		channel = switch_core_session_get_channel(session);
		tech_pvt = switch_core_session_get_private(session);


		if (sip->sip_payload && sip->sip_payload->pl_data) {
			tech_pvt->mparams.last_sdp_str = switch_core_session_strdup(session, sip->sip_payload->pl_data);
		}
	}

	if (session && profile && sip && sofia_test_pflag(profile, PFLAG_TRACK_CALLS)) {
		switch_channel_t *channel = switch_core_session_get_channel(session);
		private_object_t *tech_pvt = (private_object_t *) switch_core_session_get_private(session);
		char network_ip[80];
		int network_port = 0;
		char via_space[2048];
		char branch[16] = "";

		sofia_clear_flag(tech_pvt, TFLAG_GOT_ACK);

		sofia_glue_get_addr(de->data->e_msg, network_ip, sizeof(network_ip), &network_port);
		switch_stun_random_string(branch, sizeof(branch) - 1, "0123456789abcdef");

		switch_snprintf(via_space, sizeof(via_space), "SIP/2.0/UDP %s;rport=%d;branch=%s", network_ip, network_port, branch);
		switch_channel_set_variable(channel, "sip_full_via", via_space);
		switch_channel_set_variable_printf(channel, "sip_network_port", "%d", network_port);
		switch_channel_set_variable_printf(channel, "sip_recieved_port", "%d", network_port);
		switch_channel_set_variable_printf(channel, "sip_via_rport", "%d", network_port);

		switch_core_recovery_track(session);
	}

	if (sofia_test_pflag(profile, PFLAG_MANAGE_SHARED_APPEARANCE)) {
		if (channel && sip->sip_call_info) {
			char *p;
			if ((call_info = sip_header_as_string(nua_handle_home(nh), (void *) sip->sip_call_info))) {
				if (switch_stristr("appearance", call_info)) {
					switch_channel_set_variable(channel, "presence_call_info_full", call_info);
					if ((p = strchr(call_info, ';'))) {
						switch_channel_set_variable(channel, "presence_call_info", p + 1);
					}
				}
				su_free(nua_handle_home(nh), call_info);
			}
		}
	}

	if (channel) {
		if (tech_pvt->mparams.last_sdp_str) {
			tech_pvt->mparams.prev_sdp_str = tech_pvt->mparams.last_sdp_str;
		}
		tech_pvt->mparams.last_sdp_str = NULL;

		if (sip->sip_payload && sip->sip_payload->pl_data) {
			if (!zstr(tech_pvt->mparams.prev_sdp_str) && strcmp(tech_pvt->mparams.prev_sdp_str, sip->sip_payload->pl_data)) {
				switch_channel_set_variable(channel, "sip_reinvite_sdp", sip->sip_payload->pl_data);
				tech_pvt->mparams.last_sdp_str = switch_core_session_strdup(session, sip->sip_payload->pl_data);
			} else {
				tech_pvt->mparams.last_sdp_str = tech_pvt->mparams.prev_sdp_str;
			}
		}

		switch_channel_execute_on(channel, "execute_on_sip_reinvite");
	}

}

switch_status_t sofia_locate_user(char* user, switch_core_session_t *session, sip_t const *sip, switch_xml_t* x_user)
{
	char *username, *domain;
	switch_event_t *v_event = NULL;
	switch_status_t result = SWITCH_STATUS_FALSE;

	if (!session) {
		return SWITCH_STATUS_FALSE;
	}

	if (zstr(user)) {
		return SWITCH_STATUS_FALSE;
	}

	if (!(username = switch_core_session_strdup(session, user))) {
		return SWITCH_STATUS_FALSE;
	}

	if (!(domain = strchr(username, '@'))) {
		return SWITCH_STATUS_FALSE;
	}

	*domain++ = '\0';

	if (switch_event_create(&v_event, SWITCH_EVENT_REQUEST_PARAMS) == SWITCH_STATUS_SUCCESS) {
		sip_unknown_t *un;
		for (un = sip->sip_unknown; un; un = un->un_next) {
			switch_event_add_header_string(v_event, SWITCH_STACK_BOTTOM, un->un_name, un->un_value);
		};
	}

	result = switch_xml_locate_user_merged("id", username, domain, NULL, x_user, v_event);

	if (v_event) {
		switch_event_destroy(&v_event);
	}

	return result;
}

void sofia_handle_sip_i_invite(switch_core_session_t *session, nua_t *nua, sofia_profile_t *profile, nua_handle_t *nh, sofia_private_t *sofia_private, sip_t const *sip, sofia_dispatch_event_t *de, tagi_t tags[])
{
	char key[128] = "";
	sip_unknown_t *un;
	sip_remote_party_id_t *rpid = NULL;
	sip_p_asserted_identity_t *passerted = NULL;
	sip_p_preferred_identity_t *ppreferred = NULL;
	sip_privacy_t *privacy = NULL;
	sip_alert_info_t *alert_info = NULL;
	sip_call_info_t *call_info = NULL;
	private_object_t *tech_pvt = NULL;
	switch_channel_t *channel = NULL;
	//const char *channel_name = NULL;
	const char *displayname = NULL;
	const char *destination_number = NULL;
	const char *from_user = NULL, *from_host = NULL;
	const char *referred_by_user = NULL;//, *referred_by_host = NULL;
	const char *context = NULL;
	const char *dialplan = NULL;
	char network_ip[80];
	char proxied_client_ip[80];
	switch_event_t *v_event = NULL;
	switch_xml_t x_user = NULL;
	uint32_t sess_count = switch_core_session_count();
	uint32_t sess_max = switch_core_session_limit(0);
	int is_auth = 0, calling_myself = 0;
	int network_port = 0;
	char *is_nat = NULL;
	char *aniii = NULL;
	char acl_token[512] = "";
	sofia_transport_t transport;
	const char *gw_name = NULL;
	const char *gw_param_name = NULL;
	char *call_info_str = NULL;
	nua_handle_t *bnh = NULL;
	char sip_acl_authed_by[512] = "";
	char sip_acl_token[512] = "";
	const char *dialog_from_user = "", *dialog_from_host = "", *to_user = "", *to_host = "", *contact_user = "", *contact_host = "";
	const char *user_agent = "", *call_id = "";
	url_t *from = NULL, *to = NULL, *contact = NULL;
	const char *to_tag = "";
	const char *from_tag = "";
	char *sql = NULL;
	char *acl_context = NULL;
	const char *r_sdp = NULL;
	int is_tcp = 0, is_tls = 0;
	const char *uparams = NULL;
	char *name_params = NULL;
	const char *req_uri = NULL;
	char *req_user = NULL;
	switch_time_t sip_invite_time;

	if (sip && sip->sip_contact && sip->sip_contact->m_url->url_params) {
		uparams = sip->sip_contact->m_url->url_params;
	} else {
		uparams = NULL;
	}


	if (uparams) {
		if (switch_stristr("transport=tcp", uparams)) {
			is_tcp = 1;
		} else if (switch_stristr("transport=tls", uparams)) {
			is_tls = 1;
		}
	}

	profile->ib_calls++;

	if (sip->sip_payload && sip->sip_payload->pl_data) {
		r_sdp = sip->sip_payload->pl_data;
	}

	if (!session || (sess_count >= sess_max || !sofia_test_pflag(profile, PFLAG_RUNNING))) {
		nua_respond(nh, 503, "Maximum Calls In Progress", SIPTAG_RETRY_AFTER_STR("300"), TAG_END());
		goto fail;
	}

	tech_pvt = switch_core_session_get_private(session);

	sip_invite_time = switch_micro_time_now();

	if (!sip || !sip->sip_request || !sip->sip_request->rq_method_name) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Received an invalid packet!\n");
		nua_respond(nh, SIP_503_SERVICE_UNAVAILABLE, TAG_END());
		goto fail;
	}

	if (!(sip->sip_contact)) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "NO CONTACT!\n");
		nua_respond(nh, 400, "Missing Contact Header", TAG_END());
		goto fail;
	}


	sofia_glue_get_addr(de->data->e_msg, network_ip, sizeof(network_ip), &network_port);

	switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(tech_pvt->session), SWITCH_LOG_DEBUG, "%s receiving invite from %s:%d version: %s\n",
					  switch_channel_get_name(tech_pvt->channel), network_ip, network_port, switch_version_full_human());


	if (sip && sip->sip_via && sip->sip_via->v_protocol && switch_stristr("sip/2.0/ws", sip->sip_via->v_protocol)) {
		is_nat = "websockets";
	}


	if (r_sdp) {
		switch_core_media_set_sdp_codec_string(session, r_sdp, SDP_TYPE_REQUEST);
	}


	if (sofia_test_pflag(profile, PFLAG_AGGRESSIVE_NAT_DETECTION) ||
		(sofia_test_pflag(profile, PFLAG_TLS_ALWAYS_NAT) && (is_tcp || is_tls)) ||
		(!is_tcp && !is_tls && (zstr(network_ip) || !switch_check_network_list_ip(network_ip, profile->local_network)) &&
		 profile->server_rport_level >= 2 && sip->sip_user_agent &&
		 sip->sip_user_agent->g_string &&
		 (!strncasecmp(sip->sip_user_agent->g_string, "Polycom", 7) || !strncasecmp(sip->sip_user_agent->g_string, "KIRK Wireless Server", 20)))
		) {
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
		int contact_private_ip = 1;
		int network_private_ip = 0;
		char *last_acl = NULL;
		const char *contact_host = NULL;

		if (sip) {
			if (sip->sip_via && sip->sip_via->v_host) {
				contact_host = sip->sip_via->v_host;
			} else if (sip->sip_contact) {
				contact_host = sip->sip_contact->m_url->url_host;
			}
		}

		if (!zstr(contact_host)) {
			/* NAT mode double check logic and examples.

			   Example 1: the contact_host is 192.168.1.100 and the network_ip is also 192.168.1.100 the end point
			   is most likely behind nat with us so we need to veto that decision to turn on nat processing.

			   Example 2: the contact_host is 192.168.1.100 and the network_ip is 192.0.2.100 which is a public internet ip
			   the remote endpoint is likely behind a remote nat traversing the public internet.

			   This secondary check is here to double check the conclusion of nat settigs to ensure we don't set net
			   in cases where we don't really need to be doing this.

			   Why would you want to do this?  Well if your FreeSWITCH is behind nat and you want to talk to endpoints behind
			   remote NAT over the public internet in addition to endpoints behind nat with you.  This simplifies that process.

			*/

			for (x = 0; x < profile->nat_acl_count; x++) {
				last_acl = profile->nat_acl[x];
				if ((contact_private_ip = switch_check_network_list_ip(contact_host, last_acl))) {
					break;
				}
			}
			if (contact_private_ip) {
				for (x = 0; x < profile->nat_acl_count; x++) {
					if ((network_private_ip = switch_check_network_list_ip(network_ip, profile->nat_acl[x]))) {
						break;
					}
				}
			}

			if (contact_private_ip && !network_private_ip) {
				is_nat = last_acl;
			}
		}
	}

	if (profile->acl_count) {
		uint32_t x = 0;
		int ok = 1;
		char *last_acl = NULL;
		const char *token = NULL;
		int acl_port = sofia_test_pflag(profile, PFLAG_USE_PORT_FOR_ACL_CHECK) ? network_port : 0;

		for (x = 0; x < profile->acl_count; x++) {
			last_acl = profile->acl[x];
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "verifying acl \"%s\" for ip/port %s:%i.\n",
							  switch_str_nil(last_acl), network_ip, acl_port);
			if ((ok = switch_check_network_list_ip_port_token(network_ip, acl_port, last_acl, &token))) {

				if (profile->acl_pass_context[x]) {
					acl_context = profile->acl_pass_context[x];
				}
				if(!token && profile->acl_inbound_x_token_header) {
					const char * x_auth_token = sofia_glue_get_unknown_header(sip, profile->acl_inbound_x_token_header);
					if (!zstr(x_auth_token)) {
						token = x_auth_token;
					}
				}

				break;
			}

			if (profile->acl_fail_context[x]) {
				acl_context = profile->acl_fail_context[x];
			} else {
				acl_context = NULL;
			}
		}

		if (ok) {
			if (token) {
				switch_set_string(acl_token, token);
			}
			if (sofia_test_pflag(profile, PFLAG_AUTH_CALLS)) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "IP %s Approved by acl \"%s[%s]\". Access Granted.\n",
								  network_ip, switch_str_nil(last_acl), acl_token);
				switch_set_string(sip_acl_authed_by, last_acl);
				switch_set_string(sip_acl_token, acl_token);
				is_auth = 1;
			}
		} else {
			int network_ip_is_proxy = 0;
			const char* x_auth_ip = network_ip;
			/* Check if network_ip is a proxy allowed to send us calls */
			if (profile->proxy_acl_count) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "%d acls to check for proxy\n", profile->proxy_acl_count);
				for (x = 0; x < profile->proxy_acl_count; x++) {
					last_acl = profile->proxy_acl[x];
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "checking %s against acl %s\n", network_ip, last_acl);
					if (switch_check_network_list_ip_port_token(network_ip, network_port, last_acl, &token)) {
						switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "%s is a proxy according to the %s acl\n", network_ip, last_acl);
						network_ip_is_proxy = 1;
						break;
					}
				}
			}

			/*
			 * if network_ip is a proxy allowed to send calls, check for auth
			 * ip header and see if it matches against the inbound acl
			 */
			if (network_ip_is_proxy) {
				const char * x_auth_port = sofia_glue_get_unknown_header(sip, "X-AUTH-PORT");
				int x_auth_port_i = sofia_test_pflag(profile, PFLAG_USE_PORT_FOR_ACL_CHECK) ? zstr(x_auth_port) ? 0 : atoi(x_auth_port) : 0;

				/*
				 * if network_ip is a proxy allowed to send calls,
				 * authorize call if proxy provided matched token header
				 */
				if (profile->acl_proxy_x_token_header) {
					const char * x_auth_token = sofia_glue_get_unknown_header(sip, profile->acl_proxy_x_token_header);
					if (!zstr(x_auth_token)) {
						token = x_auth_token;
						switch_copy_string(proxied_client_ip, x_auth_ip, sizeof(proxied_client_ip));
						ok = 1;
					}
				}

				if (!ok && (x_auth_ip = sofia_glue_get_unknown_header(sip, "X-AUTH-IP")) && !zstr(x_auth_ip)) {
					for (x = 0; x < profile->acl_count; x++) {
						last_acl = profile->acl[x];
						switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "verifying acl \"%s\" from proxy for ip/port %s:%i.\n",
										  switch_str_nil(last_acl), x_auth_ip, x_auth_port_i);
						if ((ok = switch_check_network_list_ip_port_token(x_auth_ip, x_auth_port_i, last_acl, &token))) {

							switch_copy_string(proxied_client_ip, x_auth_ip, sizeof(proxied_client_ip));

							if (profile->acl_pass_context[x]) {
								acl_context = profile->acl_pass_context[x];
							}

							break;
						}

						if (profile->acl_fail_context[x]) {
							acl_context = profile->acl_fail_context[x];
						} else {
							acl_context = NULL;
						}
					}
				} else {
					x_auth_ip = network_ip;
				}
			}

			if (ok) {
				if (token) {
					switch_set_string(acl_token, token);
				}
				if (sofia_test_pflag(profile, PFLAG_AUTH_CALLS)) {
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "IP %s Approved by acl \"%s[%s]\". Access Granted.\n",
					                  x_auth_ip, switch_str_nil(last_acl), acl_token);
					switch_set_string(sip_acl_authed_by, last_acl);
					switch_set_string(sip_acl_token, acl_token);
					is_auth = 1;
				}
			} else {
				if (!sofia_test_pflag(profile, PFLAG_AUTH_CALLS)) {
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "IP %s Rejected by acl \"%s\"\n", x_auth_ip, switch_str_nil(last_acl));
					if (!acl_context) {
						nua_respond(nh, SIP_403_FORBIDDEN, TAG_END());
						goto fail;
					} else {
						switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "IP %s Rejected by acl \"%s\". Falling back to Digest auth.\n",
										  x_auth_ip, switch_str_nil(last_acl));
					}
				}
			}
		}
	}


	if (!is_auth && sofia_test_pflag(profile, PFLAG_AUTH_CALLS) && sofia_test_pflag(profile, PFLAG_AUTH_CALLS_ACL_ONLY)) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "IP/Port %s %i Rejected by acls and auth-calls-acl-only flag is set, rejecting call\n",
						  network_ip, network_port);
		nua_respond(nh, SIP_403_FORBIDDEN, TAG_END());
		goto fail;
	}

	if (!is_auth && sofia_test_pflag(profile, PFLAG_AUTH_CALLS) && sofia_test_pflag(profile, PFLAG_BLIND_AUTH)) {
		char *user = NULL;
		switch_status_t blind_result = SWITCH_STATUS_FALSE;

		if (!strcmp(network_ip, profile->sipip) && network_port == profile->sip_port) {
			calling_myself++;
		}

		if (sip && sip->sip_from) {
			user = switch_core_session_sprintf(session, "%s@%s", sip->sip_from->a_url->url_user, sip->sip_from->a_url->url_host);
			blind_result = sofia_locate_user(user, session, sip, &x_user);
		}
		if (!sofia_test_pflag(profile, PFLAG_BLIND_AUTH_ENFORCE_RESULT) || blind_result == SWITCH_STATUS_SUCCESS) {
			is_auth++;
		} else if (sofia_test_pflag(profile, PFLAG_BLIND_AUTH_REPLY_403)) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "blind auth enforce 403 enabled and couldn't find user %s, rejecting call\n", user);
			nua_respond(nh, SIP_403_FORBIDDEN, TAG_END());
			goto fail;
		}
	}


	tech_pvt->from_user = switch_core_session_strdup(session, sip->sip_from->a_url->url_user);
	tech_pvt->mparams.remote_ip = switch_core_session_strdup(session, network_ip);
	tech_pvt->mparams.remote_port = network_port;

	if (!is_auth &&
		(sofia_test_pflag(profile, PFLAG_AUTH_CALLS)
		 || (!sofia_test_pflag(profile, PFLAG_BLIND_AUTH) && (sip->sip_proxy_authorization || sip->sip_authorization)))) {
		if (!strcmp(network_ip, profile->sipip) && network_port == profile->sip_port) {
			calling_myself++;
		} else {
			switch_event_create(&v_event, SWITCH_EVENT_REQUEST_PARAMS);

			if (sofia_reg_handle_register(nua, profile, nh, sip, de, REG_INVITE, key, sizeof(key), &v_event, NULL, NULL, &x_user)) {
				if (v_event) {
					switch_event_destroy(&v_event);
				}
				if (x_user) {
					switch_xml_free(x_user);
				}

				if (sip->sip_authorization || sip->sip_proxy_authorization) {
					goto fail;
				}

				return;
			}
		}
		is_auth++;
	}

	channel = tech_pvt->channel = switch_core_session_get_channel(session);

	switch_channel_set_variable_printf(channel, "sip_local_network_addr", "%s", profile->extsipip ? profile->extsipip : profile->sipip);
	switch_channel_set_variable_printf(channel, "sip_network_ip", "%s", network_ip);
	switch_channel_set_variable_printf(channel, "sip_network_port", "%d", network_port);
	switch_channel_set_variable_printf(channel, "sip_invite_stamp", "%" SWITCH_TIME_T_FMT, sip_invite_time);

	if (*acl_token) {
		switch_channel_set_variable(channel, "acl_token", acl_token);
		if (sofia_locate_user(acl_token, session, sip, &x_user) == SWITCH_STATUS_SUCCESS) {
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "Authenticating user %s\n", acl_token);
		} else {
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_WARNING, "Error Authenticating user %s\n", acl_token);
			if (sofia_test_pflag(profile, PFLAG_AUTH_REQUIRE_USER)) {
				nua_respond(nh, SIP_480_TEMPORARILY_UNAVAILABLE, TAG_END());
				goto fail;
			}
		}
	}

	if (sip->sip_via || sip->sip_contact) {
		char tmp[35] = "";
		const char *ipv6 = strchr(tech_pvt->mparams.remote_ip, ':');

		if (sip->sip_via) {
			transport = sofia_glue_via2transport(sip->sip_via);
		} else {
			transport = sofia_glue_url2transport(sip->sip_contact->m_url);
		}


		tech_pvt->record_route =
			switch_core_session_sprintf(session,
										"sip:%s%s%s:%d;transport=%s",
										ipv6 ? "[" : "", tech_pvt->mparams.remote_ip, ipv6 ? "]" : "",
										tech_pvt->mparams.remote_port, sofia_glue_transport2str(transport));

		switch_channel_set_variable(channel, "sip_received_ip", tech_pvt->mparams.remote_ip);
		snprintf(tmp, sizeof(tmp), "%d", tech_pvt->mparams.remote_port);
		switch_channel_set_variable(channel, "sip_received_port", tmp);
	}

	if (sip->sip_via) {
		switch_channel_set_variable(channel, "sip_via_protocol", sofia_glue_transport2str(sofia_glue_via2transport(sip->sip_via)));
	}

	if (*key != '\0') {
		tech_pvt->key = switch_core_session_strdup(session, key);
	}


	if (is_auth) {
		switch_channel_set_variable(channel, "sip_authorized", "true");

		if (!zstr(sip_acl_authed_by)) {
			switch_channel_set_variable(channel, "sip_acl_authed_by", sip_acl_authed_by);
		}

		if (!zstr(sip_acl_token)) {
			switch_channel_set_variable(channel, "sip_acl_token", sip_acl_token);
		}

	}

	if (calling_myself) {
		switch_channel_set_variable(channel, "sip_looped_call", "true");
	}

	tech_pvt->caller_profile = switch_caller_profile_new(switch_core_session_get_pool(session),
														 NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, MODNAME, NULL, NULL);
	switch_channel_set_caller_profile(channel, tech_pvt->caller_profile);

	if (x_user) {
		const char *ruser = NULL, *rdomain = NULL, *user = switch_xml_attr(x_user, "id"), *domain = switch_xml_attr(x_user, "domain-name");

		if (v_event) {
			switch_event_header_t *hp;

			for (hp = v_event->headers; hp; hp = hp->next) {
				switch_channel_set_variable(channel, hp->name, hp->value);
			}

			ruser = switch_event_get_header(v_event, "user_name");
			rdomain = switch_event_get_header(v_event, "domain_name");

			switch_channel_set_variable(channel, "requested_user_name", ruser);
			switch_channel_set_variable(channel, "requested_domain_name", rdomain);
		}

		if (!user) user = ruser;
		if (!domain) domain = rdomain;

		switch_ivr_set_user_xml(session, NULL, user, domain, x_user);
		switch_xml_free(x_user);
		x_user = NULL;
	}

	if (v_event) {
		switch_event_destroy(&v_event);
	}

	if (sip->sip_from) {
		from_user = sip->sip_from->a_url->url_user;
		from_host = sip->sip_from->a_url->url_host;
		//channel_name = url_set_chanvars(session, sip->sip_from->a_url, sip_from);

		if (sip->sip_from->a_url->url_params) {
			aniii = switch_find_parameter(sip->sip_from->a_url->url_params, "isup-oli", switch_core_session_get_pool(session));
		}

		if (!zstr(from_user)) {
			if (*from_user == '+') {
				switch_channel_set_variable(channel, "sip_from_user_stripped", (const char *) (from_user + 1));
			} else {
				switch_channel_set_variable(channel, "sip_from_user_stripped", from_user);
			}
		}

		switch_channel_set_variable(channel, "sip_from_comment", sip->sip_from->a_comment);

		if (sip->sip_from->a_params) {
			set_variable_sip_param(channel, "from", sip->sip_from->a_params);
		}

		switch_channel_set_variable(channel, "sofia_profile_name", profile->name);
		switch_channel_set_variable(channel, "sofia_profile_url", profile->url);
		switch_channel_set_variable(channel, "recovery_profile_name", profile->name);
		switch_channel_set_variable(channel, "sofia_profile_domain_name", profile->domain_name);

		if (!zstr(sip->sip_from->a_display)) {
			displayname = sip->sip_from->a_display;
		} else {
			displayname = zstr(from_user) ? "unknown" : from_user;
		}
	}

	if ((rpid = sip_remote_party_id(sip))) {
		if (rpid->rpid_url->url_user) {
			char *full_rpid_header = sip_header_as_string(nh->nh_home, (void *) rpid);
			from_user = rpid->rpid_url->url_user;
			if (!zstr(full_rpid_header)) {
				switch_channel_set_variable(channel, "sip_Remote-Party-ID", full_rpid_header);
			}

		}
		if (!zstr(rpid->rpid_display)) {
			displayname = rpid->rpid_display;
		}
		switch_channel_set_variable(channel, "sip_cid_type", "rpid");
		tech_pvt->cid_type = CID_TYPE_RPID;
	}

	if ((passerted = sip_p_asserted_identity(sip))) {
		if (passerted->paid_url->url_user) {
			char *full_paid_header = sip_header_as_string(nh->nh_home, (void *) passerted);
			//char *full_paid_header = (char *)(passerted->paid_common->h_data);
			from_user = passerted->paid_url->url_user;
			if (!zstr(full_paid_header)) {
				if (profile->paid_type == PAID_DEFAULT || profile->paid_type == PAID_USER) {
					switch_channel_set_variable(channel, "sip_P-Asserted-Identity", from_user);
				} else if (profile->paid_type == PAID_USER_DOMAIN) {
					switch_channel_set_variable(channel, "sip_P-Asserted-Identity",
								switch_core_session_sprintf(session, "%s@%s", passerted->paid_url->url_user, passerted->paid_url->url_host));
				} else if (profile->paid_type == PAID_VERBATIM) {
					switch_channel_set_variable(channel, "sip_P-Asserted-Identity",  full_paid_header);
				}
			}
		}
		if (!zstr(passerted->paid_display)) {
			displayname = passerted->paid_display;
		}
		switch_channel_set_variable(channel, "sip_cid_type", "pid");
		tech_pvt->cid_type = CID_TYPE_PID;
	}

	if ((ppreferred = sip_p_preferred_identity(sip))) {
		if (ppreferred->ppid_url->url_user) {
			char *full_ppid_header = sip_header_as_string(nh->nh_home, (void *) ppreferred);
			from_user = ppreferred->ppid_url->url_user;
			if (!zstr(full_ppid_header)) {
				switch_channel_set_variable(channel, "sip_P-Preferred-Identity", full_ppid_header);
			}

		}
		if (!zstr(ppreferred->ppid_display)) {
			displayname = ppreferred->ppid_display;
		}
		switch_channel_set_variable(channel, "sip_cid_type", "pid");
		tech_pvt->cid_type = CID_TYPE_PID;
	}

	if (from_user) {
		check_decode(from_user, session);

		if ((name_params = strchr(from_user, ';'))) {
			*name_params++ = '\0';
			switch_channel_set_variable(channel, "sip_name_params", name_params);
		}
	}

	extract_header_vars(profile, sip, session, nh);
	sofia_add_invite_header_to_chanvars(channel, nh, sip->sip_allow, "sip_allow");

	req_uri = url_set_chanvars(session, sip->sip_request->rq_url, sip_req);
	if (sip->sip_request->rq_url->url_user) {

		req_user = switch_core_session_strdup(session, sip->sip_request->rq_url->url_user);
		if (profile->parse_invite_tel_params) {
			if (strchr(req_user, ';')) {
				int argc1, x1 = 0;
				char *argv1[32] = { 0 };

				if ((argc1 = switch_separate_string(req_user, ';', argv1, (sizeof(argv1) / sizeof(argv1[0]))))) {
					for (x1 = 0; x1 < argc1; x1++) {
						if (x1 == 0) {
							switch_channel_set_variable(channel, "sip_req_user", argv1[0]);
						} else {
							int argc2 = 0;
							char *argv2[2] = { 0 };
							if ((argc2 = switch_separate_string(argv1[x1], '=', argv2, (sizeof(argv2) / sizeof(argv2[0])))) == 2) {
								char *var_name = NULL;
								var_name = switch_mprintf("sip_invite_%s", argv2[0]);
								switch_channel_set_variable(channel, var_name, argv2[1]);
								switch_safe_free( var_name );
							} else {
								char *var_name = NULL;
								var_name = switch_mprintf("sip_invite_%s", argv1[x1]);
								switch_channel_set_variable(channel, var_name, "true");
								switch_safe_free( var_name );
							}
						}
					}
				}
			}
		}
	}

	if (sofia_test_pflag(profile, PFLAG_FULL_ID)) {
		destination_number = req_uri;
	} else {
		destination_number = req_user;
	}
	if (sip->sip_request->rq_url->url_params && (sofia_glue_find_parameter(sip->sip_request->rq_url->url_params, "intercom=true"))) {
		switch_channel_set_variable(channel, "sip_auto_answer_detected", "true");
	}

	if (!destination_number && sip->sip_to) {
		destination_number = sip->sip_to->a_url->url_user;
	}

	/* The human network, OH THE HUMANITY!!! lets send invites with no number! */
	if (!destination_number && sip->sip_from) {
		destination_number = sip->sip_from->a_url->url_user;
	}

	if (destination_number) {
		check_decode(destination_number, session);
	} else {
		destination_number = "service";
	}

	if (sip->sip_to) {
		const char *host, *user;
		int port, check_nat = 0;
		url_t *transport_url;

		if (sip->sip_record_route) {
			transport_url = sip->sip_record_route->r_url;
		} else {
			transport_url = sip->sip_contact->m_url;
		}

		transport = sofia_glue_url2transport(transport_url);
		tech_pvt->transport = transport;

		url_set_chanvars(session, sip->sip_to->a_url, sip_to);
		if (switch_channel_get_variable(channel, "sip_to_uri")) {
			const char *ipv6;
			const char *tmp, *at, *url = NULL;

			host = switch_channel_get_variable(channel, "sip_to_host");
			user = switch_channel_get_variable(channel, "sip_to_user");

			switch_channel_set_variable(channel, "sip_to_comment", sip->sip_to->a_comment);

			if (sip->sip_to->a_params) {
				set_variable_sip_param(channel, "to", sip->sip_to->a_params);
			}

			if (sip->sip_contact->m_url->url_port) {
				port = atoi(sip->sip_contact->m_url->url_port);
			} else {
				port = sofia_glue_transport_has_tls(transport) ? profile->tls_sip_port : profile->extsipport;
			}

			ipv6 = strchr(host, ':');
			tech_pvt->to_uri =
				switch_core_session_sprintf(session,
											"sip:%s@%s%s%s:%d;transport=%s",
											user, ipv6 ? "[" : "", host, ipv6 ? "]" : "", port, sofia_glue_transport2str(transport));

			if (sofia_glue_check_nat(profile, tech_pvt->mparams.remote_ip)) {
				check_nat = 1;
			}
			url = sofia_glue_get_profile_url(profile, tech_pvt->mparams.remote_ip, transport);

			if (!url) {
				if (check_nat) {
					switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_WARNING, "Nat detected but no external address configured.\n");
				}
				url = profile->url;
			}

			if (!url) {
				switch_channel_hangup(tech_pvt->channel, SWITCH_CAUSE_DESTINATION_OUT_OF_ORDER);
			}

			tmp = sofia_overcome_sip_uri_weakness(session, url, transport, SWITCH_TRUE, NULL, NULL);

			if ((at = strchr(tmp, '@'))) {
				url = switch_core_session_sprintf(session, "sip:%s%s", user, at);
			}

			if (url) {
				const char *brackets = NULL;
				const char *proto = NULL;

				brackets = strchr(url, '>');
				proto = switch_stristr("transport=", url);
				tech_pvt->reply_contact = switch_core_session_sprintf(session, "%s%s%s%s%s",
																	  brackets ? "" : "<", url,
																	  proto ? "" : ";transport=",
																	  proto ? "" : sofia_glue_transport2str(transport), brackets ? "" : ">");
			} else {
				switch_channel_hangup(tech_pvt->channel, SWITCH_CAUSE_DESTINATION_OUT_OF_ORDER);
			}

		} else {
			const char *url = NULL;
			url = sofia_glue_get_profile_url(profile, tech_pvt->mparams.remote_ip, transport);

			if (url) {
				const char *brackets = NULL;
				const char *proto = NULL;

				brackets = strchr(url, '>');
				proto = switch_stristr("transport=", url);
				tech_pvt->reply_contact = switch_core_session_sprintf(session, "%s%s%s%s%s",
																	  brackets ? "" : "<", url,
																	  proto ? "" : ";transport=",
																	  proto ? "" : sofia_glue_transport2str(transport), brackets ? "" : ">");
			} else {
				switch_channel_hangup(tech_pvt->channel, SWITCH_CAUSE_DESTINATION_OUT_OF_ORDER);
			}
		}
	}

	if (sofia_glue_check_nat(profile, tech_pvt->mparams.remote_ip)) {
		tech_pvt->user_via = sofia_glue_create_external_via(session, profile, tech_pvt->transport);
		nua_set_hparams(nh, SIPTAG_VIA_STR(tech_pvt->user_via), TAG_END());
	}

	if (sip->sip_contact) {
		url_set_chanvars(session, sip->sip_contact->m_url, sip_contact);
	}

	if (sip->sip_referred_by) {
		referred_by_user = sip->sip_referred_by->b_url->url_user;
		//referred_by_host = sip->sip_referred_by->b_url->url_host;
		//channel_name = url_set_chanvars(session, sip->sip_referred_by->b_url, sip_referred_by);

		check_decode(referred_by_user, session);

		if (!zstr(referred_by_user)) {
			if (*referred_by_user == '+') {
				switch_channel_set_variable(channel, "sip_referred_by_user_stripped", (const char *) (referred_by_user + 1));
			} else {
				switch_channel_set_variable(channel, "sip_referred_by_user_stripped", referred_by_user);
			}
		}

		switch_channel_set_variable(channel, "sip_referred_by_cid", sip->sip_referred_by->b_cid);

		if (sip->sip_referred_by->b_params) {
			set_variable_sip_param(channel, "referred_by", sip->sip_referred_by->b_params);
		}
	}

	//sofia_glue_set_name(tech_pvt, channel_name);
	switch_core_media_prepare_codecs(tech_pvt->session, SWITCH_FALSE);

	switch_channel_set_variable(channel, SWITCH_ENDPOINT_DISPOSITION_VARIABLE, "INBOUND CALL");

	if (sofia_test_flag(tech_pvt, TFLAG_INB_NOMEDIA)) {
		switch_channel_set_flag(channel, CF_PROXY_MODE);
	}

	if (profile->media_options & MEDIA_OPT_BYPASS_AFTER_HOLD) {
		switch_channel_set_flag(channel, CF_BYPASS_MEDIA_AFTER_HOLD);
	}

	if (sofia_test_flag(tech_pvt, TFLAG_PROXY_MEDIA)) {
		switch_channel_set_flag(channel, CF_PROXY_MEDIA);
	}

	if (sofia_test_flag(tech_pvt, TFLAG_ZRTP_PASSTHRU)) {
		switch_channel_set_flag(channel, CF_ZRTP_PASSTHRU_REQ);
	}

	if (sip->sip_subject && sip->sip_subject->g_string) {
		switch_channel_set_variable(channel, "sip_subject", sip->sip_subject->g_string);
	}

	if (sip->sip_user_agent && !zstr(sip->sip_user_agent->g_string)) {
		switch_channel_set_variable(channel, "sip_user_agent", sip->sip_user_agent->g_string);
	}

	sofia_set_accept_language_channel_variable(channel, sip);

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


	if (sip->sip_multipart) {
		msg_multipart_t *mp;

		for (mp = sip->sip_multipart; mp; mp = mp->mp_next) {
			if (mp->mp_payload && mp->mp_payload->pl_data && mp->mp_content_type && mp->mp_content_type->c_type) {
				char *val = switch_core_session_sprintf(session, "%s:%s", mp->mp_content_type->c_type, mp->mp_payload->pl_data);
				switch_channel_add_variable_var_check(channel, "sip_multipart", val, SWITCH_FALSE, SWITCH_STACK_PUSH);
			}
		}
	}

	if (sip->sip_max_forwards) {
		char max_forwards[32];
		switch_snprintf(max_forwards, sizeof(max_forwards), "%lu", sip->sip_max_forwards->mf_count);
		switch_channel_set_variable(channel, SWITCH_MAX_FORWARDS_VARIABLE, max_forwards);
	}

	if (acl_context) context = acl_context;

	if (!context) {
		context = switch_channel_get_variable(channel, "user_context");
	}

	if (!context) {
		if (profile->context && !strcasecmp(profile->context, "_domain_")) {
			context = from_host;
		} else {
			context = profile->context;
		}
	}

	if (!(dialplan = switch_channel_get_variable(channel, "inbound_dialplan"))) {
		dialplan = profile->dialplan;
	}

	if ((alert_info = sip_alert_info(sip))) {
		char *tmp = sip_header_as_string(nh->nh_home, (void *) alert_info);
		switch_channel_set_variable(channel, "alert_info", tmp);
		su_free(nh->nh_home, tmp);
	}

	if ((call_info = sip_call_info(sip))) {
		call_info_str = sip_header_as_string(nh->nh_home, (void *) call_info);

		if (sofia_test_pflag(profile, PFLAG_MANAGE_SHARED_APPEARANCE) && switch_stristr("appearance", call_info_str)) {
			char *p;

			switch_channel_set_variable(channel, "presence_call_info_full", call_info_str);
			if ((p = strchr(call_info_str, ';'))) {
				p++;
				switch_channel_set_variable(channel, "presence_call_info", p);
			}
		}

		if (call_info->ci_params && (msg_params_find(call_info->ci_params, "answer-after=0"))) {
			switch_channel_set_variable(channel, "sip_auto_answer_detected", "true");
		}

		switch_channel_set_variable(channel, "sip_call_info", call_info_str);

		call_info = call_info->ci_next;

		while (call_info) {
			call_info_str = sip_header_as_string(nh->nh_home, (void *) call_info);
			switch_channel_add_variable_var_check(channel, "sip_call_info", call_info_str, SWITCH_FALSE, SWITCH_STACK_PUSH);
			call_info = call_info->ci_next;
		}

		call_info = sip_call_info(sip);

	} else if (sofia_test_pflag(profile, PFLAG_MANAGE_SHARED_APPEARANCE)) {
		char buf[128] = "";
		char *sql;
		char *state = "progressing";

		if (sip &&
			sip->sip_from && sip->sip_from->a_url->url_user && sip->sip_from->a_url->url_host &&
			sip->sip_to && sip->sip_to->a_url->url_user && sip->sip_to->a_url->url_host) {
			sql =
				switch_mprintf("select 'appearance-index=1' from sip_subscriptions where expires > -1 and hostname='%q' and event='call-info' and "
							   "sub_to_user='%q' and sub_to_host='%q'", mod_sofia_globals.hostname, sip->sip_to->a_url->url_user,
							   sip->sip_from->a_url->url_host);
			sofia_glue_execute_sql2str(profile, profile->dbh_mutex, sql, buf, sizeof(buf));

			if (mod_sofia_globals.debug_sla > 1) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "QUERY SQL %s [%s]\n", sql, buf);
			}
			free(sql);

			if (!zstr(buf)) {
				sql = switch_mprintf("update sip_dialogs set call_info='%q',call_info_state='%q' "
									 "where uuid='%q'", buf, state, switch_core_session_get_uuid(session));

				if (mod_sofia_globals.debug_sla > 1) {
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "QUERY SQL %s\n", sql);
				}

				sofia_glue_execute_sql_now(profile, &sql, SWITCH_TRUE);


				switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_WARNING, "Auto-Fixing Broken SLA [<sip:%s>;%s]\n",
								  sip->sip_from->a_url->url_host, buf);

				switch_channel_set_variable_printf(channel, "presence_call_info_full", "<sip:%s>;%s", sip->sip_from->a_url->url_host, buf);
				switch_channel_set_variable(channel, "presence_call_info", buf);
				call_info_str = switch_core_session_sprintf(session, "<sip:%s>;%s", sip->sip_from->a_url->url_host, buf);
			}
		}
	}


	if (profile->pres_type) {
		const char *presence_id = switch_channel_get_variable(channel, "presence_id");
		if (zstr(presence_id)) {
			const char *user = switch_str_nil(sip->sip_from->a_url->url_user);
			const char *host = switch_str_nil(sip->sip_from->a_url->url_host);
			char *tmp = switch_mprintf("%s@%s", user, host);
			switch_assert(tmp);
			switch_channel_set_variable(channel, "presence_id", tmp);
			free(tmp);
		}
	}


	if (sip->sip_request->rq_url->url_params) {
		gw_param_name = switch_find_parameter(sip->sip_request->rq_url->url_params, "gw", switch_core_session_get_pool(session));
	}

	if (strstr(destination_number, "gw+")) {
		if (sofia_test_pflag(profile, PFLAG_FULL_ID)) {
			char *tmp;
			gw_name = switch_core_session_strdup(session, destination_number + 3);
			if ((tmp = strchr(gw_name, '@'))) {
				*tmp = '\0';
			}
		} else {
			gw_name = destination_number + 3;
		}
	}

	if (gw_name || gw_param_name) {
		sofia_gateway_t *gateway = NULL;
		char *extension = NULL;

		if (gw_name && ((gateway = sofia_reg_find_gateway(gw_name)))) {
			gw_param_name = NULL;
			extension = gateway->extension;
		}

		if (!gateway && gw_param_name) {
			if ((gateway = sofia_reg_find_gateway(gw_param_name))) {
				extension = gateway->real_extension;
			}
		}

		if (gateway) {
			context = switch_core_session_strdup(session, gateway->register_context);
			switch_channel_set_variable(channel, "sip_gateway", gateway->name);

			if (!zstr(extension)) {
				if (!strcasecmp(extension, "auto_to_user")) {
					destination_number = sip->sip_to->a_url->url_user;
				} else if (!strcasecmp(extension, "auto")) {
					if (gw_name) {
						destination_number = sip->sip_to->a_url->url_user;
					}
				} else {
					destination_number = switch_core_session_strdup(session, extension);
				}
			} else if (!gw_param_name) {
				destination_number = sip->sip_to->a_url->url_user;
			}

			gateway->ib_calls++;

			if (gateway->ib_vars) {
				switch_event_header_t *hp;
				for (hp = gateway->ib_vars->headers; hp; hp = hp->next) {
					switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "%s setting variable [%s]=[%s]\n",
									  switch_channel_get_name(channel), hp->name, hp->value);
					switch_channel_set_variable(channel, hp->name, hp->value);
				}
			}

			sofia_reg_release_gateway(gateway);
		}
	}

	if (call_info_str) {
		char *sql;
		char cid[512] = "";
		char *str;
		char *p = NULL;
		const char *user = NULL, *host = NULL, *from_user = NULL, *from_host = NULL;

		if (sip->sip_to) {
			user = sip->sip_to->a_url->url_user;
			host = sip->sip_to->a_url->url_host;
		}

		if (sip->sip_from) {
			from_user = sip->sip_from->a_url->url_user;
			from_host = sip->sip_from->a_url->url_host;
		}

		if (!user) user = from_user;
		if (!host) user = from_host;

		if (user && host && from_user && !strcmp(user, from_user)) {
			if ((p = strchr(call_info_str, ';'))) {
				p++;
			}

			sql = switch_mprintf(
		      "select call_id from sip_dialogs where (call_info='%q' or call_info='%q;appearance-state=held') and "
			  "((sip_from_user='%q' and sip_from_host='%q') or presence_id='%q@%q') and call_id is not null",
				 switch_str_nil(p), switch_str_nil(p), user, host, user, host);

			if ((str = sofia_glue_execute_sql2str(profile, profile->dbh_mutex, sql, cid, sizeof(cid)))) {
				bnh = nua_handle_by_call_id(nua, str);
			}

			if (mod_sofia_globals.debug_sla > 1) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "PICK SQL %s [%s] [%s] %d\n", sql, str, cid, !!bnh);
			}

			free(sql);
		}
	}


	check_decode(displayname, session);

	profile_dup_clean(from_user, tech_pvt->caller_profile->username, tech_pvt->caller_profile->pool);
	profile_dup_clean(dialplan, tech_pvt->caller_profile->dialplan, tech_pvt->caller_profile->pool);
	profile_dup_clean(displayname, tech_pvt->caller_profile->caller_id_name, tech_pvt->caller_profile->pool);
	profile_dup_clean(from_user, tech_pvt->caller_profile->caller_id_number, tech_pvt->caller_profile->pool);
	profile_dup_clean(displayname, tech_pvt->caller_profile->orig_caller_id_name, tech_pvt->caller_profile->pool);
	profile_dup_clean(from_user, tech_pvt->caller_profile->orig_caller_id_number, tech_pvt->caller_profile->pool);
	profile_dup_clean(network_ip, tech_pvt->caller_profile->network_addr, tech_pvt->caller_profile->pool);
	profile_dup_clean(from_user, tech_pvt->caller_profile->ani, tech_pvt->caller_profile->pool);
	profile_dup_clean(aniii, tech_pvt->caller_profile->aniii, tech_pvt->caller_profile->pool);
	profile_dup_clean(context, tech_pvt->caller_profile->context, tech_pvt->caller_profile->pool);
	profile_dup_clean(destination_number, tech_pvt->caller_profile->destination_number, tech_pvt->caller_profile->pool);

	if (!bnh && sip->sip_replaces) {
		if (!(bnh = nua_handle_by_replaces(nua, sip->sip_replaces))) {
			if (!(bnh = nua_handle_by_call_id(nua, sip->sip_replaces->rp_call_id))) {
				bnh = sofia_global_nua_handle_by_replaces(sip->sip_replaces);
			}
		}
	}

	if (sip && sip->sip_replaces) {
		msg_common_t *rp_common = sip->sip_replaces->rp_common;
		switch_channel_set_variable(channel, "sip_replaces_call_id", sip->sip_replaces->rp_call_id);
		if (rp_common && rp_common->h_class->hc_params) {
			int i, n;
			msg_param_t const *params = * (msg_param_t const **) ((char *)rp_common + rp_common->h_class->hc_params);
			for (i = 0; params[i]; i++) {
				msg_param_t param = params[i];
				if (strchr(param, '=')) {
					n = strcspn(param, "=");
					switch_channel_set_variable_name_printf(channel, param + n + 1, "sip_replaces_%.*s", n, param);
				} else {
					switch_channel_set_variable_name_printf(channel, "true", "sip_replaces_%s", param);
				}
			}
		}
	}

	if (bnh) {
		sofia_private_t *b_private = NULL;
		if ((b_private = nua_handle_magic(bnh))) {
			switch_core_session_t *b_session = NULL;
			if ((b_session = switch_core_session_locate(b_private->uuid))) {
				switch_channel_t *b_channel = switch_core_session_get_channel(b_session);
				sofia_handle_sip_i_invite_replaces(session, channel, b_channel, b_private->uuid, tech_pvt, call_info, profile, is_nat, sip);
				switch_core_session_rwunlock(b_session);
			}
		}
		nua_handle_unref(bnh);
	} else if (sip && sip->sip_replaces && sip->sip_replaces->rp_call_id) {
		switch_core_session_t *b_session = NULL;
		if ((b_session = switch_core_session_locate((char*) sip->sip_replaces->rp_call_id))) {
				switch_channel_t *b_channel = switch_core_session_get_channel(b_session);
				sofia_handle_sip_i_invite_replaces(session, channel, b_channel, (char*) sip->sip_replaces->rp_call_id, tech_pvt, call_info, profile, is_nat, sip);
				switch_core_session_rwunlock(b_session);
		}
	}

	if (tech_pvt->caller_profile) {

		int first_history_info = 1;

		if (rpid) {
			if (rpid->rpid_privacy) {
				if (!strcasecmp(rpid->rpid_privacy, "yes")) {
					switch_set_flag(tech_pvt->caller_profile, SWITCH_CPF_HIDE_NAME | SWITCH_CPF_HIDE_NUMBER);
				} else if (!strcasecmp(rpid->rpid_privacy, "full")) {
					switch_set_flag(tech_pvt->caller_profile, SWITCH_CPF_HIDE_NAME | SWITCH_CPF_HIDE_NUMBER);
				} else if (!strcasecmp(rpid->rpid_privacy, "name")) {
					switch_set_flag(tech_pvt->caller_profile, SWITCH_CPF_HIDE_NAME);
				} else if (!strcasecmp(rpid->rpid_privacy, "number")) {
					switch_set_flag(tech_pvt->caller_profile, SWITCH_CPF_HIDE_NUMBER);
				} else {
					switch_clear_flag(tech_pvt->caller_profile, SWITCH_CPF_HIDE_NAME);
					switch_clear_flag(tech_pvt->caller_profile, SWITCH_CPF_HIDE_NUMBER);
				}
			}

			if (rpid->rpid_screen && !strcasecmp(rpid->rpid_screen, "no")) {
				switch_clear_flag(tech_pvt->caller_profile, SWITCH_CPF_SCREEN);
			}
		}

		if ((privacy = sip_privacy(sip))) {
			char *full_priv_header = sip_header_as_string(nh->nh_home, (void *) privacy);
			if (!zstr(full_priv_header)) {
				switch_channel_set_variable(channel, "sip_Privacy", full_priv_header);
			}
			if (msg_params_find(privacy->priv_values, "id")) {
				switch_set_flag(tech_pvt->caller_profile, SWITCH_CPF_HIDE_NAME | SWITCH_CPF_HIDE_NUMBER);
			}
		}

		/* Loop thru unknown Headers Here so we can do something with them */
		for (un = sip->sip_unknown; un; un = un->un_next) {
			if (!strncasecmp(un->un_name, "Accept-Language", 15)) {
				if (!zstr(un->un_value)) {
					char *tmp_name;
					if ((tmp_name = switch_mprintf("%s%s", SOFIA_SIP_HEADER_PREFIX, un->un_name))) {
						switch_channel_set_variable(channel, tmp_name, un->un_value);
						free(tmp_name);
					}
				}
			} else if (!strncasecmp(un->un_name, "Diversion", 9)) {
				/* Basic Diversion Support for Diversion Indication in SIP */
				/* draft-levy-sip-diversion-08 */
				if (!zstr(un->un_value)) {
					char *tmp_name;
					if ((tmp_name = switch_mprintf("%s%s", SOFIA_SIP_HEADER_PREFIX, un->un_name))) {
						switch_channel_set_variable(channel, tmp_name, un->un_value);
						free(tmp_name);
					}
				}
			} else if (!strncasecmp(un->un_name, "History-Info", 12)) {
				if (first_history_info) {
					/* If the header exists first time, make sure to remove old info and re-set the variable */
					switch_channel_set_variable(channel, "sip_history_info", un->un_value);
					first_history_info = 0;
				} else {
					/* Append the History-Info into one long string */
					const char *history_var = switch_channel_get_variable(channel, "sip_history_info");
					if (!zstr(history_var)) {
						char *tmp_str;
						if ((tmp_str = switch_mprintf("%s, %s", history_var, un->un_value))) {
							switch_channel_set_variable(channel, "sip_history_info", tmp_str);
							free(tmp_str);
						} else {
							switch_channel_set_variable(channel, "sip_history_info", un->un_value);
						}
					} else {
						switch_channel_set_variable(channel, "sip_history_info", un->un_value);
					}
				}
			} else if (!strcasecmp(un->un_name, "X-FS-Channel-Name") && !zstr(un->un_value)) {
				switch_channel_set_name(channel, un->un_value);
				switch_channel_set_variable(channel, "push_channel_name", "true");
			} else if (!strcasecmp(un->un_name, "X-FS-Support")) {
				tech_pvt->x_freeswitch_support_remote = switch_core_session_strdup(session, un->un_value);
			} else if (!strcasecmp(un->un_name, "Geolocation")) {
				switch_channel_set_variable(channel, "sip_geolocation", un->un_value);
			} else if (!strcasecmp(un->un_name, "Geolocation-Error")) {
				switch_channel_set_variable(channel, "sip_geolocation_error", un->un_value);
			} else if (!strcasecmp(un->un_name, "userLocation")) {
				switch_channel_set_variable(channel, "sip_user_location", un->un_value);
			} else if (!strncasecmp(un->un_name, "X-", 2) || !strncasecmp(un->un_name, "P-", 2) || !strcasecmp(un->un_name, "User-to-User") || !strncasecmp(un->un_name, "On", 2)) {
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

						switch_channel_set_variable(channel, new_name, un->un_value);
						break;
					}
				}
			}
		}

	}

	tech_pvt->sofia_private = sofia_private;
	tech_pvt->nh = nh;

	if (profile->pres_type && sofia_test_pflag(profile, PFLAG_IN_DIALOG_CHAT)) {
		sofia_presence_set_chat_hash(tech_pvt, sip);
	}

	if (sofia_test_pflag(profile, PFLAG_PARSE_ALL_INVITE_HEADERS)) {
		sofia_parse_all_invite_headers(sip, session, nh);
	}

	if (sip->sip_to) {
		to = sip->sip_to->a_url;
	}
	if (sip->sip_from) {
		from = sip->sip_from->a_url;
	}
	if (sip->sip_contact) {
		contact = sip->sip_contact->m_url;
	}

	if (sip->sip_user_agent) {
		user_agent = switch_str_nil(sip->sip_user_agent->g_string);
	}

	if (sip->sip_call_id) {
		call_id = switch_str_nil(sip->sip_call_id->i_id);
	}

	if (to) {
		to_user = switch_str_nil(to->url_user);
		to_host = switch_str_nil(to->url_host);
		to_tag = switch_str_nil(sip->sip_to->a_tag);
	}

	if (from) {
		dialog_from_user = switch_str_nil(from->url_user);
		dialog_from_host = switch_str_nil(from->url_host);
		from_tag = switch_str_nil(sip->sip_from->a_tag);
	}

	if (contact) {
		contact_user = switch_str_nil(contact->url_user);
		contact_host = switch_str_nil(contact->url_host);
	}

	if (profile->pres_type) {
		const char *presence_data = switch_channel_get_variable(channel, "presence_data");
		const char *presence_id = switch_channel_get_variable(channel, "presence_id");
		char *full_contact = "";
		char *p = NULL;
		time_t now;

		if (sip->sip_contact) {
			full_contact = sip_header_as_string(nua_handle_home(tech_pvt->nh), (void *) sip->sip_contact);
		}

		if (call_info_str && switch_stristr("appearance", call_info_str)) {
			switch_channel_set_variable(channel, "presence_call_info_full", call_info_str);
			if ((p = strchr(call_info_str, ';'))) {
				p++;
				switch_channel_set_variable(channel, "presence_call_info", p);
			}
		}

		now = switch_epoch_time_now(NULL);

		sql = switch_mprintf("insert into sip_dialogs "
							 "(call_id,uuid,sip_to_user,sip_to_host,sip_to_tag,sip_from_user,sip_from_host,sip_from_tag,contact_user,"
							 "contact_host,state,direction,user_agent,profile_name,hostname,contact,presence_id,presence_data,"
							 "call_info,rcd,call_info_state) "
							 "values('%q','%q','%q','%q','%q','%q','%q','%q','%q','%q','%q','%q','%q','%q','%q','%q','%q','%q','%q',%ld,'')",
							 call_id,
							 tech_pvt->sofia_private->uuid,
							 to_user, to_host, to_tag, dialog_from_user, dialog_from_host, from_tag,
							 contact_user, contact_host, "confirmed", "inbound", user_agent,
							 profile->name, mod_sofia_globals.hostname, switch_str_nil(full_contact),
							 switch_str_nil(presence_id), switch_str_nil(presence_data), switch_str_nil(p), now);

		switch_assert(sql);

		sofia_glue_execute_sql_now(profile, &sql, SWITCH_TRUE);

		if ( full_contact ) {
			su_free(nua_handle_home(tech_pvt->nh), full_contact);
		}
	}

	if (is_nat) {
		sofia_set_flag(tech_pvt, TFLAG_NAT);
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "Setting NAT mode based on %s\n", is_nat);
		switch_channel_set_variable(channel, "sip_nat_detected", "true");
	}

	return;

  fail:
	profile->ib_failed_calls++;

	return;

}

void sofia_handle_sip_i_invite_replaces(switch_core_session_t *session, switch_channel_t *channel, switch_channel_t *b_channel, char* uuid, private_object_t *tech_pvt, sip_call_info_t *call_info, sofia_profile_t *profile, char *is_nat, sip_t const *sip)
{
	const char *bridge_uuid;
	switch_caller_profile_t *orig_cp, *cp;
	//const char *sent_name, *sent_number;
	orig_cp = switch_channel_get_caller_profile(b_channel);
	tech_pvt->caller_profile->callee_id_name = switch_core_strdup(tech_pvt->caller_profile->pool, orig_cp->callee_id_name);
	tech_pvt->caller_profile->callee_id_number = switch_core_strdup(tech_pvt->caller_profile->pool, orig_cp->callee_id_number);

	if (!call_info) {
		tech_pvt->caller_profile->caller_id_name = switch_core_strdup(tech_pvt->caller_profile->pool, orig_cp->caller_id_name);
		tech_pvt->caller_profile->caller_id_number = switch_core_strdup(tech_pvt->caller_profile->pool, orig_cp->caller_id_number);
	}

	if (orig_cp) {
		cp = switch_caller_profile_dup(tech_pvt->caller_profile->pool, orig_cp);
		switch_channel_set_originator_caller_profile(channel, cp);
	}

#if 0
	sent_name = switch_channel_get_variable(b_channel, "last_sent_callee_id_name");
	sent_number = switch_channel_get_variable(b_channel, "last_sent_callee_id_number");

	if (!zstr(sent_name) && !zstr(sent_number)) {
		tech_pvt->caller_profile->callee_id_name = switch_core_strdup(tech_pvt->caller_profile->pool, sent_name);
		tech_pvt->caller_profile->callee_id_number = switch_core_strdup(tech_pvt->caller_profile->pool, sent_number);
	} else {
		if (switch_channel_direction(channel) == SWITCH_CALL_DIRECTION_INBOUND) {
			tech_pvt->caller_profile->callee_id_name = switch_core_strdup(tech_pvt->caller_profile->pool, orig_cp->callee_id_name);
			tech_pvt->caller_profile->callee_id_number = switch_core_strdup(tech_pvt->caller_profile->pool, orig_cp->callee_id_number);
		} else {
			tech_pvt->caller_profile->callee_id_name = switch_core_strdup(tech_pvt->caller_profile->pool, orig_cp->caller_id_name);
			tech_pvt->caller_profile->callee_id_number = switch_core_strdup(tech_pvt->caller_profile->pool, orig_cp->caller_id_number);
		}
	}
#endif

	if (is_nat) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_CRIT, "Setting NAT mode based on %s\n", is_nat);
	}

	tech_pvt->caller_profile->dialplan = "inline";

	bridge_uuid = switch_channel_get_partner_uuid(b_channel);
	if (bridge_uuid) {
		switch_core_session_t *bridge_session = NULL;
		if ((bridge_session = switch_core_session_locate(bridge_uuid))) {
			switch_core_session_rwunlock(bridge_session);
		} else {
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_WARNING, "could not locate partner_uuid %s, resetting\n", bridge_uuid);
			bridge_uuid = NULL;
		}
	}

	if (call_info) {
		switch_event_t *event = NULL;

		if (!zstr(bridge_uuid) && switch_channel_test_flag(b_channel, CF_LEG_HOLDING)) {
			const char *b_call_id = switch_channel_get_variable(b_channel, "sip_call_id");

			if (b_call_id) {
				char *sql = switch_mprintf("update sip_dialogs set call_info_state='idle' where call_id='%q'", b_call_id);
				if (mod_sofia_globals.debug_sla > 1) {
					switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "QUERY SQL %s\n", sql);
				}
				sofia_glue_execute_sql_now(profile, &sql, SWITCH_TRUE);

				switch_channel_presence(b_channel, "unknown", "idle", NULL);
			}
			switch_channel_set_flag(tech_pvt->channel, CF_SLA_INTERCEPT);
			tech_pvt->caller_profile->destination_number = switch_core_sprintf(tech_pvt->caller_profile->pool,
																			   "answer,intercept:%s", bridge_uuid);

			if (sofia_test_pflag(profile, PFLAG_FIRE_TRANFER_EVENTS)
				&& sip && sip->sip_call_id
				&& switch_event_create_subclass(&event, SWITCH_EVENT_CUSTOM, MY_EVENT_REPLACED) == SWITCH_STATUS_SUCCESS) {
				switch_channel_event_set_data(b_channel, event);
				switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "att_xfer_replaced_by", sip->sip_call_id->i_id);
				switch_event_fire(&event);
			}
		} else {
			switch_caller_profile_t *bcp = switch_channel_get_caller_profile(b_channel);

			if (switch_channel_test_flag(b_channel, CF_BRIDGE_ORIGINATOR)) {
				switch_channel_set_flag(tech_pvt->channel, CF_BRIDGE_ORIGINATOR);
			}

			if (!zstr(bcp->callee_id_name)) {
				tech_pvt->caller_profile->callee_id_name = switch_core_strdup(tech_pvt->caller_profile->pool, bcp->callee_id_name);
			}

			if (!zstr(bcp->callee_id_number)) {
				tech_pvt->caller_profile->callee_id_number = switch_core_strdup(tech_pvt->caller_profile->pool, bcp->callee_id_number);
			}


			if (!zstr(bcp->caller_id_name)) {
				tech_pvt->caller_profile->caller_id_name = switch_core_strdup(tech_pvt->caller_profile->pool, bcp->caller_id_name);
			}

			if (!zstr(bcp->caller_id_number)) {
				tech_pvt->caller_profile->caller_id_number = switch_core_strdup(tech_pvt->caller_profile->pool, bcp->caller_id_number);
			}

			if (bcp->originatee_caller_profile) {
				switch_caller_profile_t *cp;

				cp = switch_caller_profile_dup(tech_pvt->caller_profile->pool,
											   bcp->originatee_caller_profile);

				switch_channel_set_originatee_caller_profile(tech_pvt->channel, cp);
			}

			tech_pvt->caller_profile->destination_number = switch_core_sprintf(tech_pvt->caller_profile->pool,
																			   "answer,sofia_sla:%s", uuid);
		}
	} else {
		char const *nightmare_xfer_uuid = NULL;
		switch_event_t *event = NULL;
		if (switch_channel_var_true(channel, "sip_replaces_a-leg")) {
			switch_channel_mark_hold(b_channel, SWITCH_FALSE);
			tech_pvt->caller_profile->destination_number = switch_core_sprintf(tech_pvt->caller_profile->pool, "answer,intercept:%s", sip->sip_replaces->rp_call_id);
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "call %s picked up on a-leg\n", sip->sip_replaces->rp_call_id);
			if (sofia_test_pflag(profile, PFLAG_FIRE_TRANFER_EVENTS)
				&& sip && sip->sip_call_id
				&& switch_event_create_subclass(&event, SWITCH_EVENT_CUSTOM, MY_EVENT_INTERCEPTED) == SWITCH_STATUS_SUCCESS) {
				switch_channel_event_set_data(b_channel, event);
				switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "intercepted_by", sip->sip_call_id->i_id);
				switch_event_fire(&event);
			}
		} else {
			if ((nightmare_xfer_uuid = sofia_glue_get_unknown_header(sip, "X-FS-Refer-For"))) {
				switch_channel_set_variable(b_channel, "transfer_refer_for", nightmare_xfer_uuid);
			}
			if ((nightmare_xfer_uuid = sofia_glue_get_unknown_header(sip, "X-FS-Refer-From"))) {
				switch_channel_set_variable(b_channel, "transfer_refer_from", nightmare_xfer_uuid);
			}

			if (!zstr(bridge_uuid)) {
				if (sip->sip_replaces && sip->sip_replaces->rp_params && sip->sip_replaces->rp_call_id && switch_channel_test_flag(b_channel, CF_BRIDGED) &&
					switch_true(switch_find_parameter(*(sip->sip_replaces->rp_params), "early-only", switch_core_session_get_pool(session)))) {
					switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "call %s intercept rejected\n", bridge_uuid);
					tech_pvt->caller_profile->destination_number = switch_core_sprintf(tech_pvt->caller_profile->pool, "hangup:CALL_REJECTED");
				} else {
						switch_channel_mark_hold(b_channel, SWITCH_FALSE);
						tech_pvt->caller_profile->destination_number = switch_core_sprintf(tech_pvt->caller_profile->pool, "answer,intercept:%s", bridge_uuid);
						switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "call %s intercepted\n", bridge_uuid);
						if (sofia_test_pflag(profile, PFLAG_FIRE_TRANFER_EVENTS)
							&& sip && sip->sip_call_id
							&& switch_event_create_subclass(&event, SWITCH_EVENT_CUSTOM, MY_EVENT_INTERCEPTED) == SWITCH_STATUS_SUCCESS) {
								switch_channel_event_set_data(b_channel, event);
								switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "intercepted_by", sip->sip_call_id->i_id);
								switch_event_fire(&event);
						}
				}
			} else {
				const char *b_app = switch_channel_get_variable(b_channel, SWITCH_CURRENT_APPLICATION_VARIABLE);
				const char *b_data = switch_channel_get_variable(b_channel, SWITCH_CURRENT_APPLICATION_DATA_VARIABLE);
				if (b_data && b_app) {
					tech_pvt->caller_profile->destination_number = switch_core_sprintf(tech_pvt->caller_profile->pool, "answer,%s:%s", b_app, b_data);
				} else if (b_app) {
					tech_pvt->caller_profile->destination_number = switch_core_sprintf(tech_pvt->caller_profile->pool, "answer,%s", b_app);
				}
				if (sofia_test_pflag(profile, PFLAG_FIRE_TRANFER_EVENTS)
					&& sip && sip->sip_call_id
					&& switch_event_create_subclass(&event, SWITCH_EVENT_CUSTOM, MY_EVENT_REPLACED) == SWITCH_STATUS_SUCCESS) {
					switch_channel_event_set_data(b_channel, event);
					switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "att_xfer_replaced_by", sip->sip_call_id->i_id);
					switch_event_fire(&event);
				}
				switch_channel_hangup(b_channel, SWITCH_CAUSE_ATTENDED_TRANSFER);
			}
		}
	}

}

void sofia_handle_sip_i_options(int status,
								char const *phrase,
								nua_t *nua, sofia_profile_t *profile, nua_handle_t *nh, sofia_private_t *sofia_private, sip_t const *sip,
								sofia_dispatch_event_t *de,
								tagi_t tags[])
{
	uint32_t sess_count = switch_core_session_count();
	uint32_t sess_max = switch_core_session_limit(0);

	if (sofia_test_pflag(profile, PFLAG_OPTIONS_RESPOND_503_ON_BUSY) &&
			(sess_count >= sess_max || !sofia_test_pflag(profile, PFLAG_RUNNING) || !switch_core_ready_inbound())) {
		nua_respond(nh, 503, "Maximum Calls In Progress", NUTAG_WITH_THIS_MSG(de->data->e_msg), SIPTAG_RETRY_AFTER_STR("300"), TAG_END());
	} else {
		nua_respond(nh, SIP_200_OK, NUTAG_WITH_THIS_MSG(de->data->e_msg),
					TAG_IF(sip->sip_record_route, SIPTAG_RECORD_ROUTE(sip->sip_record_route)), TAG_END());
	}

}

/*
 * This subroutine will take the a_params of a sip_addr_s structure and spin through them.
 * Each param will be used to create a channel variable.
 * In the SIP RFC's, this data is called generic-param.
 * Note that the tag-param is also included in the a_params list.
 *
 * From: "John Doe" <sip:5551212@1.2.3.4>;tag=ed23266b52cbb17eo2;ref=101;mbid=201
 *
 * For example, the header above will produce an a_params list with three entries
 *    tag=ed23266b52cbb17eo2
 *    ref=101
 *    mbid=201
 *
 * The a_params list is parsed and the lvalue is used to create the channel variable name while the
 * rvalue is used to create the channel variable value.
 *
 * If no equal (=) sign is found during parsing, a channel variable name is created with the param and
 * the value is set to NULL.
 *
 * Pointers are used for copying the sip_header_name for performance reasons.  There are no calls to
 * any string functions and no memory is allocated/dealocated.  The only limiter is the size of the
 * sip_header_name array.
*/
static void set_variable_sip_param(switch_channel_t *channel, char *header_type, sip_param_t const *params)
{
	char sip_header_name[128] = "";
	char var1[] = "sip_";
	char *cp, *sh, *sh_end, *sh_save;

	/* Build the static part of the sip_header_name variable from   */
	/* the header_type. If the header type is "referred_by" then    */
	/* sip_header_name = "sip_referred_by_".                        */
	sh = sip_header_name;
	sh_end = sh + sizeof(sip_header_name) - 1;
	for (cp = var1; *cp; cp++, sh++) {
		*sh = *cp;
	}
	*sh = '\0';

	/* Copy the header_type to the sip_header_name. Before copying  */
	/* each character, check that we aren't going to overflow the   */
	/* the sip_header_name buffer.  We have to account for the      */
	/* trailing underscore and NULL that will be added to the end.  */
	for (cp = header_type; (*cp && (sh < (sh_end - 1))); cp++, sh++) {
		*sh = *cp;
	}
	*sh++ = '_';
	*sh = '\0';

	/* sh now points to the NULL at the end of the partially built  */
	/* sip_header_name variable.  This is also the start of the     */
	/* variable part of the sip_header_name built from the lvalue   */
	/* of the params data.                                          */
	sh_save = sh;

	while (params && params[0]) {

		/* Copy the params data to the sip_header_name variable until   */
		/* the end of the params string is reached, an '=' is detected  */
		/* or until the sip_header_name buffer has been exhausted.      */
		for (cp = (char *) (*params); ((*cp != '=') && *cp && (sh < sh_end)); cp++, sh++) {
			*sh = *cp;
		}

		/* cp now points to either the end of the params data or the */
		/* equal (=) sign separating the lvalue and rvalue.          */
		if (*cp == '=')
			cp++;
		*sh = '\0';
		switch_channel_set_variable(channel, sip_header_name, cp);

		/* Bump pointer to next param in the list.  Also reset the      */
		/* sip_header_name pointer to the beginning of the dynamic area */
		params++;
		sh = sh_save;
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
