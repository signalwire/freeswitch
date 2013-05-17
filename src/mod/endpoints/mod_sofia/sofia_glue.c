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
 * Anthony Minessale II <anthm@freeswitch.org>
 * Ken Rice <krice@freeswitch.org>
 * Paul D. Tinsley <pdt at jackhammer.org>
 * Bret McDanel <trixter AT 0xdecafbad.com>
 * Eliot Gable <egable AT.AT broadvox.com>
 *
 *
 * sofia_glue.c -- SOFIA SIP Endpoint (code to tie sofia to freeswitch)
 *
 */
#include "mod_sofia.h"
#include <switch_stun.h>

switch_cache_db_handle_t *_sofia_glue_get_db_handle(sofia_profile_t *profile, const char *file, const char *func, int line);
#define sofia_glue_get_db_handle(_p) _sofia_glue_get_db_handle(_p, __FILE__, __SWITCH_FUNC__, __LINE__)


int sofia_glue_check_nat(sofia_profile_t *profile, const char *network_ip)
{
	switch_assert(network_ip);

	return (profile->extsipip && 
			!switch_check_network_list_ip(network_ip, "loopback.auto") && 
			!switch_check_network_list_ip(network_ip, profile->local_network));
}

private_object_t *sofia_glue_new_pvt(switch_core_session_t *session)
{
	private_object_t *tech_pvt = (private_object_t *) switch_core_session_alloc(session, sizeof(private_object_t));
	switch_mutex_init(&tech_pvt->flag_mutex, SWITCH_MUTEX_NESTED, switch_core_session_get_pool(session));
	switch_mutex_init(&tech_pvt->sofia_mutex, SWITCH_MUTEX_NESTED, switch_core_session_get_pool(session));
	return tech_pvt;
}

void sofia_glue_set_name(private_object_t *tech_pvt, const char *channame)
{
	char name[256];
	char *p;

	switch_snprintf(name, sizeof(name), "sofia/%s/%s", tech_pvt->profile->name, channame);
	if ((p = strchr(name, ';'))) {
		*p = '\0';
	}
	switch_channel_set_name(tech_pvt->channel, name);
}

void sofia_glue_attach_private(switch_core_session_t *session, sofia_profile_t *profile, private_object_t *tech_pvt, const char *channame)
{

	unsigned int x, i;

	switch_assert(session != NULL);
	switch_assert(profile != NULL);
	switch_assert(tech_pvt != NULL);
	
	switch_core_session_add_stream(session, NULL);

	switch_mutex_lock(tech_pvt->flag_mutex);
	switch_mutex_lock(profile->flag_mutex);

	/* copy flags from profile to the sofia private */
	for (x = 0; x < TFLAG_MAX; x++) {
		tech_pvt->flags[x] = profile->flags[x];
	}

	tech_pvt->x_freeswitch_support_local = FREESWITCH_SUPPORT;

	tech_pvt->profile = profile;

	tech_pvt->mparams.rtpip = switch_core_session_strdup(session, profile->rtpip[profile->rtpip_next++]);
	if (profile->rtpip_next >= profile->rtpip_index) {
		profile->rtpip_next = 0;
	}

	profile->inuse++;
	switch_mutex_unlock(profile->flag_mutex);
	switch_mutex_unlock(tech_pvt->flag_mutex);

	if (tech_pvt->bte) {
		tech_pvt->recv_te = tech_pvt->te = tech_pvt->bte;
	} else if (!tech_pvt->te) {
		tech_pvt->mparams.recv_te = tech_pvt->mparams.te = profile->te;
	}

	tech_pvt->mparams.dtmf_type = tech_pvt->profile->dtmf_type;

	if (!sofia_test_media_flag(tech_pvt->profile, SCMF_SUPPRESS_CNG)) {
		if (tech_pvt->bcng_pt) {
			tech_pvt->cng_pt = tech_pvt->bcng_pt;
		} else if (!tech_pvt->cng_pt) {
			tech_pvt->cng_pt = profile->cng_pt;
		}
	}

	tech_pvt->session = session;
	tech_pvt->channel = switch_core_session_get_channel(session);

	if (sofia_test_pflag(profile, PFLAG_TRACK_CALLS)) {
		switch_channel_set_flag(tech_pvt->channel, CF_TRACKABLE);
	}


	if (profile->flags[PFLAG_PASS_RFC2833]) {
		switch_channel_set_flag(tech_pvt->channel, CF_PASS_RFC2833);
	}

	if (sofia_test_pflag(tech_pvt->profile, PFLAG_RTP_NOTIMER_DURING_BRIDGE)) {
		switch_channel_set_flag(tech_pvt->channel, CF_RTP_NOTIMER_DURING_BRIDGE);
	}


	
	switch_core_media_check_dtmf_type(session);
	switch_channel_set_cap(tech_pvt->channel, CC_MEDIA_ACK);
	switch_channel_set_cap(tech_pvt->channel, CC_BYPASS_MEDIA);
	switch_channel_set_cap(tech_pvt->channel, CC_PROXY_MEDIA);
	switch_channel_set_cap(tech_pvt->channel, CC_JITTERBUFFER);
	switch_channel_set_cap(tech_pvt->channel, CC_FS_RTP);
	switch_channel_set_cap(tech_pvt->channel, CC_QUEUEABLE_DTMF_DELAY);



	tech_pvt->mparams.ndlb = tech_pvt->profile->mndlb;
	tech_pvt->mparams.inbound_codec_string = profile->inbound_codec_string;
	tech_pvt->mparams.outbound_codec_string = profile->outbound_codec_string;
	tech_pvt->mparams.auto_rtp_bugs = profile->auto_rtp_bugs;
	tech_pvt->mparams.timer_name = profile->timer_name;
	tech_pvt->mparams.vflags = profile->vflags;
	tech_pvt->mparams.manual_rtp_bugs = profile->manual_rtp_bugs;
	tech_pvt->mparams.manual_video_rtp_bugs = profile->manual_video_rtp_bugs;
	tech_pvt->mparams.extsipip = profile->extsipip;
	tech_pvt->mparams.extrtpip = profile->extrtpip;
	tech_pvt->mparams.local_network = profile->local_network;
	tech_pvt->mparams.mutex = tech_pvt->sofia_mutex;
	tech_pvt->mparams.sipip = profile->sipip;
	tech_pvt->mparams.jb_msec = profile->jb_msec;
	tech_pvt->mparams.rtcp_audio_interval_msec = profile->rtcp_audio_interval_msec;
	tech_pvt->mparams.rtcp_video_interval_msec = profile->rtcp_video_interval_msec;
	tech_pvt->mparams.sdp_username = profile->sdp_username;
	tech_pvt->mparams.cng_pt = tech_pvt->cng_pt;
	tech_pvt->mparams.rtp_timeout_sec = profile->rtp_timeout_sec;
	tech_pvt->mparams.rtp_hold_timeout_sec = profile->rtp_hold_timeout_sec;
	

	switch_media_handle_create(&tech_pvt->media_handle, session, &tech_pvt->mparams);
	switch_media_handle_set_media_flags(tech_pvt->media_handle, tech_pvt->profile->media_flags);


	for(i = 0; i < profile->cand_acl_count; i++) {
		switch_core_media_add_ice_acl(session, SWITCH_MEDIA_TYPE_AUDIO, profile->cand_acl[i]);
		switch_core_media_add_ice_acl(session, SWITCH_MEDIA_TYPE_VIDEO, profile->cand_acl[i]);
	}


	switch_core_session_set_private(session, tech_pvt);

	if (channame) {
		sofia_glue_set_name(tech_pvt, channame);
	}

}




switch_status_t sofia_glue_ext_address_lookup(sofia_profile_t *profile, char **ip, switch_port_t *port,
											  const char *sourceip, switch_memory_pool_t *pool)
{
	char *error = "";
	switch_status_t status = SWITCH_STATUS_FALSE;
	int x;
	switch_port_t stun_port = SWITCH_STUN_DEFAULT_PORT;
	char *stun_ip = NULL;

	if (!sourceip) {
		return status;
	}

	if (!strncasecmp(sourceip, "host:", 5)) {
		status = (*ip = switch_stun_host_lookup(sourceip + 5, pool)) ? SWITCH_STATUS_SUCCESS : SWITCH_STATUS_FALSE;
	} else if (!strncasecmp(sourceip, "stun:", 5)) {
		char *p;

		stun_ip = strdup(sourceip + 5);

		if ((p = strchr(stun_ip, ':'))) {
			int iport;
			*p++ = '\0';
			iport = atoi(p);
			if (iport > 0 && iport < 0xFFFF) {
				stun_port = (switch_port_t) iport;
			}
		}

		if (zstr(stun_ip)) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "STUN Failed! NO STUN SERVER\n");
			goto out;
		}


        for (x = 0; x < 5; x++) {
            if ((status = switch_stun_lookup(ip, port, stun_ip, stun_port, &error, pool)) != SWITCH_STATUS_SUCCESS) {
                switch_yield(100000);
            } else {
				break;
            }
        }

		if (!*ip) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "STUN Failed! No IP returned\n");
			goto out;
		}
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "STUN Success [%s]:[%d]\n", *ip, *port);
		status = SWITCH_STATUS_SUCCESS;
	} else {
		*ip = (char *) sourceip;
		status = SWITCH_STATUS_SUCCESS;
	}

 out:

	switch_safe_free(stun_ip);

	return status;
}


const char *sofia_glue_get_unknown_header(sip_t const *sip, const char *name)
{
	sip_unknown_t *un;
	for (un = sip->sip_unknown; un; un = un->un_next) {
		if (!strcasecmp(un->un_name, name)) {
			if (!zstr(un->un_value)) {
				return un->un_value;
			}
		}
	}
	return NULL;
}

sofia_transport_t sofia_glue_str2transport(const char *str)
{
	if (!strncasecmp(str, "udp", 3)) {
		return SOFIA_TRANSPORT_UDP;
	} else if (!strncasecmp(str, "tcp", 3)) {
		return SOFIA_TRANSPORT_TCP;
	} else if (!strncasecmp(str, "sctp", 4)) {
		return SOFIA_TRANSPORT_SCTP;
	} else if (!strncasecmp(str, "tls", 3)) {
		return SOFIA_TRANSPORT_TCP_TLS;
	}

	return SOFIA_TRANSPORT_UNKNOWN;
}

enum tport_tls_verify_policy sofia_glue_str2tls_verify_policy(const char * str){
	char *ptr_next;
	int len;
	enum tport_tls_verify_policy ret;
	char *ptr_cur = (char *) str;
	ret = TPTLS_VERIFY_NONE;

	while (ptr_cur) {
		if ((ptr_next = strchr(ptr_cur, '|'))) {
			len = ptr_next++ - ptr_cur;
		} else {
			len = strlen(ptr_cur);
		}
		if (!strncasecmp(ptr_cur, "in",len)) {
			ret |= TPTLS_VERIFY_IN;
		} else if (!strncasecmp(ptr_cur, "out",len)) {
			ret |= TPTLS_VERIFY_OUT;
		} else if (!strncasecmp(ptr_cur, "all",len)) {
			ret |= TPTLS_VERIFY_ALL;
		} else if (!strncasecmp(ptr_cur, "subjects_in",len)) {
			ret |= TPTLS_VERIFY_SUBJECTS_IN;
		} else if (!strncasecmp(ptr_cur, "subjects_out",len)) {
			ret |= TPTLS_VERIFY_SUBJECTS_OUT;
		} else if (!strncasecmp(ptr_cur, "subjects_all",len)) {
			ret |= TPTLS_VERIFY_SUBJECTS_ALL;
		}
		ptr_cur = ptr_next;
	}
	return ret;
}

char *sofia_glue_find_parameter_value(switch_core_session_t *session, const char *str, const char *param)
{
	const char *param_ptr;
	char *param_value;
	char *tmp;
	switch_size_t param_len;

	if (zstr(str) || zstr(param) || !session) return NULL;

	if (end_of(param) != '=') {
		param = switch_core_session_sprintf(session, "%s=", param);
		if (zstr(param)) return NULL;
	}

	param_len = strlen(param);
	param_ptr = sofia_glue_find_parameter(str, param);

	if (zstr(param_ptr)) return NULL;

	param_value = switch_core_session_strdup(session, param_ptr + param_len);

	if (zstr(param_value)) return NULL;

	if ((tmp = strchr(param_value, ';'))) *tmp = '\0';

	return param_value;
}

char *sofia_glue_find_parameter(const char *str, const char *param)
{
	char *ptr = NULL;

	ptr = (char *) str;
	while (ptr) {
		if (!strncasecmp(ptr, param, strlen(param)))
			return ptr;

		if ((ptr = strchr(ptr, ';')))
			ptr++;
	}

	return NULL;
}

sofia_transport_t sofia_glue_url2transport(const url_t *url)
{
	char *ptr = NULL;
	int tls = 0;

	if (!url)
		return SOFIA_TRANSPORT_UNKNOWN;

	if (url->url_scheme && !strcasecmp(url->url_scheme, "sips")) {
		tls++;
	}

	if ((ptr = sofia_glue_find_parameter(url->url_params, "transport="))) {
		return sofia_glue_str2transport(ptr + 10);
	}

	return (tls) ? SOFIA_TRANSPORT_TCP_TLS : SOFIA_TRANSPORT_UDP;
}

sofia_transport_t sofia_glue_via2transport(const sip_via_t * via)
{
	char *ptr = NULL;

	if (!via || !via->v_protocol)
		return SOFIA_TRANSPORT_UNKNOWN;

	if ((ptr = strrchr(via->v_protocol, '/'))) {
		ptr++;

		if (!strncasecmp(ptr, "udp", 3)) {
			return SOFIA_TRANSPORT_UDP;
		} else if (!strncasecmp(ptr, "tcp", 3)) {
			return SOFIA_TRANSPORT_TCP;
		} else if (!strncasecmp(ptr, "tls", 3)) {
			return SOFIA_TRANSPORT_TCP_TLS;
		} else if (!strncasecmp(ptr, "sctp", 4)) {
			return SOFIA_TRANSPORT_SCTP;
		} else if (!strncasecmp(ptr, "wss", 3)) {
			return SOFIA_TRANSPORT_WSS;
		} else if (!strncasecmp(ptr, "ws", 2)) {
			return SOFIA_TRANSPORT_WS;
		}
	}

	return SOFIA_TRANSPORT_UNKNOWN;
}

const char *sofia_glue_transport2str(const sofia_transport_t tp)
{
	switch (tp) {
	case SOFIA_TRANSPORT_TCP:
		return "tcp";

	case SOFIA_TRANSPORT_TCP_TLS:
		return "tls";

	case SOFIA_TRANSPORT_SCTP:
		return "sctp";

	case SOFIA_TRANSPORT_WS:
		return "ws";

	case SOFIA_TRANSPORT_WSS:
		return "wss";

	default:
		return "udp";
	}
}

char *sofia_glue_create_external_via(switch_core_session_t *session, sofia_profile_t *profile, sofia_transport_t transport)
{
	return sofia_glue_create_via(session, profile->extsipip, (sofia_glue_transport_has_tls(transport))
								 ? profile->tls_sip_port : profile->extsipport, transport);
}

char *sofia_glue_create_via(switch_core_session_t *session, const char *ip, switch_port_t port, sofia_transport_t transport)
{
	if (port && port != 5060) {
		if (session) {
			return switch_core_session_sprintf(session, "SIP/2.0/%s %s:%d;rport", sofia_glue_transport2str(transport), ip, port);
		} else {
			return switch_mprintf("SIP/2.0/%s %s:%d;rport", sofia_glue_transport2str(transport), ip, port);
		}
	} else {
		if (session) {
			return switch_core_session_sprintf(session, "SIP/2.0/%s %s;rport", sofia_glue_transport2str(transport), ip);
		} else {
			return switch_mprintf("SIP/2.0/%s %s;rport", sofia_glue_transport2str(transport), ip);
		}
	}
}

char *sofia_glue_strip_uri(const char *str)
{
	char *p;
	char *r;

	if ((p = strchr(str, '<'))) {
		p++;
		r = strdup(p);
		if ((p = strchr(r, '>'))) {
			*p = '\0';
		}
	} else {
		r = strdup(str);
	}

	return r;
}



int sofia_glue_transport_has_tls(const sofia_transport_t tp)
{
	switch (tp) {
	case SOFIA_TRANSPORT_TCP_TLS:
		return 1;

	default:
		return 0;
	}
}

void sofia_glue_get_addr(msg_t *msg, char *buf, size_t buflen, int *port)
{
	su_addrinfo_t *addrinfo = msg_addrinfo(msg);

	if (buf) {
		get_addr(buf, buflen, addrinfo->ai_addr, addrinfo->ai_addrlen);
	}

	if (port) {
		*port = get_port(addrinfo->ai_addr);
	}
}

char *sofia_overcome_sip_uri_weakness(switch_core_session_t *session, const char *uri, const sofia_transport_t transport, switch_bool_t uri_only,
									  const char *params, const char *invite_tel_params)
{
	char *stripped = switch_core_session_strdup(session, uri);
	char *new_uri = NULL;
	char *p;


	stripped = sofia_glue_get_url_from_contact(stripped, 0);

	/* remove our params so we don't make any whiny moronic device piss it's pants and forget who it is for a half-hour */
	if ((p = (char *) switch_stristr(";fs_", stripped))) {
		*p = '\0';
	}

	if (transport && transport != SOFIA_TRANSPORT_UDP) {

		if (switch_stristr("port=", stripped)) {
			new_uri = switch_core_session_sprintf(session, "%s%s%s", uri_only ? "" : "<", stripped, uri_only ? "" : ">");
		} else {

			if (strchr(stripped, ';')) {
				if (params) {
					new_uri = switch_core_session_sprintf(session, "%s%s;transport=%s;%s%s",
														  uri_only ? "" : "<", stripped, sofia_glue_transport2str(transport), params, uri_only ? "" : ">");
				} else {
					new_uri = switch_core_session_sprintf(session, "%s%s;transport=%s%s",
														  uri_only ? "" : "<", stripped, sofia_glue_transport2str(transport), uri_only ? "" : ">");
				}
			} else {
				if (params) {
					new_uri = switch_core_session_sprintf(session, "%s%s;transport=%s;%s%s",
														  uri_only ? "" : "<", stripped, sofia_glue_transport2str(transport), params, uri_only ? "" : ">");
				} else {
					new_uri = switch_core_session_sprintf(session, "%s%s;transport=%s%s",
														  uri_only ? "" : "<", stripped, sofia_glue_transport2str(transport), uri_only ? "" : ">");
				}
			}
		}
	} else {
		if (params) {
			new_uri = switch_core_session_sprintf(session, "%s%s;%s%s", uri_only ? "" : "<", stripped, params, uri_only ? "" : ">");
		} else {
			if (uri_only) {
				new_uri = stripped;
			} else {
				new_uri = switch_core_session_sprintf(session, "<%s>", stripped);
			}
		}
	}



	if (!zstr(invite_tel_params)) {
		char *lhs, *rhs = strchr(new_uri, '@');

		if (!zstr(rhs)) {
			*rhs++ = '\0';
			lhs = new_uri;
			new_uri = switch_core_session_sprintf(session, "%s;%s@%s", lhs, invite_tel_params, rhs);
		}
	}
	
	return new_uri;
}

char *sofia_glue_get_extra_headers(switch_channel_t *channel, const char *prefix)
{
	char *extra_headers = NULL;
	switch_stream_handle_t stream = { 0 };
	switch_event_header_t *hi = NULL;
	const char *exclude_regex = NULL;
	switch_regex_t *re = NULL;
	int ovector[30] = {0};
	int proceed;

	exclude_regex = switch_channel_get_variable(channel, "exclude_outgoing_extra_header");
	SWITCH_STANDARD_STREAM(stream);
	if ((hi = switch_channel_variable_first(channel))) {
		for (; hi; hi = hi->next) {
			const char *name = (char *) hi->name;
			char *value = (char *) hi->value;
			
			if (!strcasecmp(name, "sip_geolocation")) {
				stream.write_function(&stream, "Geolocation: %s\r\n", value);
			}

			if (!strncasecmp(name, prefix, strlen(prefix))) {
				if ( !exclude_regex || !(proceed = switch_regex_perform(name, exclude_regex, &re, ovector, sizeof(ovector) / sizeof(ovector[0])))) {
					const char *hname = name + strlen(prefix);
					stream.write_function(&stream, "%s: %s\r\n", hname, value);
					switch_regex_safe_free(re);
				} else {
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Ignoring Extra Header [%s] , matches exclude_outgoing_extra_header [%s]\n", name, exclude_regex);
				}
			}
		}
		switch_channel_variable_last(channel);
	}

	if (!zstr((char *) stream.data)) {
		extra_headers = stream.data;
	} else {
		switch_safe_free(stream.data);
	}

	return extra_headers;
}

void sofia_glue_set_extra_headers(switch_core_session_t *session, sip_t const *sip, const char *prefix)
{
	sip_unknown_t *un;
	char name[512] = "";
	switch_channel_t *channel = switch_core_session_get_channel(session);
	char *pstr;

	
	if (!sip || !channel) {
		return;
	}

	for (un = sip->sip_unknown; un; un = un->un_next) {
		if ((!strncasecmp(un->un_name, "X-", 2) && strncasecmp(un->un_name, "X-FS-", 5)) || !strncasecmp(un->un_name, "P-", 2)) {
			if (!zstr(un->un_value)) {
				switch_snprintf(name, sizeof(name), "%s%s", prefix, un->un_name);
				switch_channel_set_variable(channel, name, un->un_value);
			}
		}
	}

	pstr = switch_core_session_sprintf(session, "execute_on_%sprefix", prefix);
	switch_channel_execute_on(channel, pstr);
	switch_channel_api_on(channel, pstr);

	switch_channel_execute_on(channel, "execute_on_sip_extra_headers");
	switch_channel_api_on(channel, "api_on_sip_extra_headers");
}

char *sofia_glue_get_extra_headers_from_event(switch_event_t *event, const char *prefix)
{
	char *extra_headers = NULL;
	switch_stream_handle_t stream = { 0 };
	switch_event_header_t *hp;

	SWITCH_STANDARD_STREAM(stream);
	for (hp = event->headers; hp; hp = hp->next) {
		if (!zstr(hp->name) && !zstr(hp->value) && !strncasecmp(hp->name, prefix, strlen(prefix))) {
			char *name = strdup(hp->name);
			const char *hname = name + strlen(prefix);
			stream.write_function(&stream, "%s: %s\r\n", hname, (char *)hp->value);
			free(name);
		}
	}

	if (!zstr((char *) stream.data)) {
		extra_headers = stream.data;
	} else {
		switch_safe_free(stream.data);
	}

	return extra_headers;
}

switch_status_t sofia_glue_do_invite(switch_core_session_t *session)
{
	char *alert_info = NULL;
	const char *max_forwards = NULL;
	const char *alertbuf;
	private_object_t *tech_pvt = switch_core_session_get_private(session);
	switch_channel_t *channel = switch_core_session_get_channel(session);
	switch_caller_profile_t *caller_profile;
	const char *cid_name, *cid_num;
	char *e_dest = NULL;
	const char *holdstr = "";
	char *extra_headers = NULL;
	switch_status_t status = SWITCH_STATUS_FALSE;
	uint32_t session_timeout = 0;
	const char *val;
	const char *rep;
	const char *call_id = NULL;
	char *route = NULL;
	char *route_uri = NULL;
	sofia_destination_t *dst = NULL;
	sofia_cid_type_t cid_type = tech_pvt->profile->cid_type;
	sip_cseq_t *cseq = NULL;
	const char *invite_record_route = switch_channel_get_variable(tech_pvt->channel, "sip_invite_record_route");
	const char *invite_route_uri = switch_channel_get_variable(tech_pvt->channel, "sip_invite_route_uri");
	const char *invite_full_from = switch_channel_get_variable(tech_pvt->channel, "sip_invite_full_from");
	const char *invite_full_to = switch_channel_get_variable(tech_pvt->channel, "sip_invite_full_to");
	const char *handle_full_from = switch_channel_get_variable(tech_pvt->channel, "sip_handle_full_from");
	const char *handle_full_to = switch_channel_get_variable(tech_pvt->channel, "sip_handle_full_to");
	const char *force_full_from = switch_channel_get_variable(tech_pvt->channel, "sip_force_full_from");
	const char *force_full_to = switch_channel_get_variable(tech_pvt->channel, "sip_force_full_to");
	char *mp = NULL, *mp_type = NULL;
	char *record_route = NULL;
	const char *recover_via = NULL;
	int require_timer = 1;

	if (switch_channel_test_flag(tech_pvt->channel, CF_RECOVERING)) {
		const char *recover_contact = switch_channel_get_variable(tech_pvt->channel, "sip_recover_contact");
		recover_via = switch_channel_get_variable(tech_pvt->channel, "sip_recover_via");

		if (!zstr(invite_record_route)) {
			record_route = switch_core_session_sprintf(session, "Record-Route: %s", invite_record_route);
		}
		
		if (recover_contact) {
			char *tmp = switch_core_session_strdup(session, recover_contact);
			tech_pvt->redirected = sofia_glue_get_url_from_contact(tmp, 0);
		}
	}


	if ((rep = switch_channel_get_variable(channel, SOFIA_REPLACES_HEADER))) {
		switch_channel_set_variable(channel, SOFIA_REPLACES_HEADER, NULL);
	}

	switch_assert(tech_pvt != NULL);

	sofia_clear_flag_locked(tech_pvt, TFLAG_SDP);

	caller_profile = switch_channel_get_caller_profile(channel);

	if (!caller_profile) {
		switch_channel_hangup(channel, SWITCH_CAUSE_DESTINATION_OUT_OF_ORDER);
		return SWITCH_STATUS_FALSE;
	}


	if ((val = switch_channel_get_variable_dup(channel, "sip_require_timer", SWITCH_FALSE, -1)) && switch_false(val)) {
		require_timer = 0;
	}


	cid_name = caller_profile->caller_id_name;
	cid_num = caller_profile->caller_id_number;
	switch_core_media_prepare_codecs(tech_pvt->session, SWITCH_FALSE);
	switch_core_media_check_video_codecs(tech_pvt->session);
	check_decode(cid_name, session);
	check_decode(cid_num, session);


	if ((alertbuf = switch_channel_get_variable(channel, "alert_info"))) {
		alert_info = switch_core_session_sprintf(tech_pvt->session, "Alert-Info: %s", alertbuf);
	}

	max_forwards = switch_channel_get_variable(channel, SWITCH_MAX_FORWARDS_VARIABLE);

	if ((status = switch_core_media_choose_port(tech_pvt->session, SWITCH_MEDIA_TYPE_AUDIO, 0)) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(tech_pvt->session), SWITCH_LOG_ERROR, "Port Error!\n");
		return status;
	}

	if (!switch_channel_get_private(tech_pvt->channel, "t38_options") || zstr(tech_pvt->mparams.local_sdp_str)) {
		switch_core_media_gen_local_sdp(session, NULL, 0, NULL, 0);
	}

	sofia_set_flag_locked(tech_pvt, TFLAG_READY);

	if (!tech_pvt->nh) {
		char *d_url = NULL, *url = NULL, *url_str = NULL;
		sofia_private_t *sofia_private;
		char *invite_contact = NULL, *to_str, *use_from_str, *from_str;
		const char *t_var;
		char *rpid_domain = NULL, *p;
		const char *priv = "off";
		const char *screen = "no";
		const char *invite_params = switch_channel_get_variable(tech_pvt->channel, "sip_invite_params");
		const char *invite_to_params = switch_channel_get_variable(tech_pvt->channel, "sip_invite_to_params");
		const char *invite_tel_params = switch_channel_get_variable(switch_core_session_get_channel(session), "sip_invite_tel_params");
		const char *invite_to_uri = switch_channel_get_variable(tech_pvt->channel, "sip_invite_to_uri");
		const char *invite_from_uri = switch_channel_get_variable(tech_pvt->channel, "sip_invite_from_uri");
		const char *invite_contact_params = switch_channel_get_variable(tech_pvt->channel, "sip_invite_contact_params");
		const char *invite_from_params = switch_channel_get_variable(tech_pvt->channel, "sip_invite_from_params");
		const char *from_var = switch_channel_get_variable(tech_pvt->channel, "sip_from_uri");
		const char *from_display = switch_channel_get_variable(tech_pvt->channel, "sip_from_display");
		const char *invite_req_uri = switch_channel_get_variable(tech_pvt->channel, "sip_invite_req_uri");
		const char *invite_domain = switch_channel_get_variable(tech_pvt->channel, "sip_invite_domain");
		
		const char *use_name, *use_number;

		if (zstr(tech_pvt->dest)) {
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(tech_pvt->session), SWITCH_LOG_ERROR, "URL Error!\n");
			return SWITCH_STATUS_FALSE;
		}

		if ((d_url = sofia_glue_get_url_from_contact(tech_pvt->dest, 1))) {
			url = d_url;
		} else {
			url = tech_pvt->dest;
		}

		url_str = url;

		if (!tech_pvt->from_str) {
			const char *sipip;
			const char *format;

			sipip = tech_pvt->profile->sipip;

			if (!zstr(tech_pvt->mparams.remote_ip) && sofia_glue_check_nat(tech_pvt->profile, tech_pvt->mparams.remote_ip)) {
				sipip = tech_pvt->profile->extsipip;
			}

			format = strchr(sipip, ':') ? "\"%s\" <sip:%s%s[%s]>" : "\"%s\" <sip:%s%s%s>";

			if (!zstr(invite_domain)) {
				sipip = invite_domain;
			}

			tech_pvt->from_str = switch_core_session_sprintf(tech_pvt->session, format, cid_name, cid_num, !zstr(cid_num) ? "@" : "", sipip);
		}

		if (from_var) {
			if (strncasecmp(from_var, "sip:", 4) || strncasecmp(from_var, "sips:", 5)) {
				use_from_str = switch_core_session_strdup(tech_pvt->session, from_var);
			} else {
				use_from_str = switch_core_session_sprintf(tech_pvt->session, "sip:%s", from_var);
			}
		} else if (!zstr(tech_pvt->gateway_from_str)) {
			use_from_str = tech_pvt->gateway_from_str;
		} else {
			use_from_str = tech_pvt->from_str;
		}

		if (!zstr(tech_pvt->gateway_from_str)) {
			rpid_domain = switch_core_session_strdup(session, tech_pvt->gateway_from_str);
		} else if (!zstr(tech_pvt->from_str)) {
			rpid_domain = switch_core_session_strdup(session, use_from_str);
		}

		sofia_glue_get_url_from_contact(rpid_domain, 0);
		if ((rpid_domain = strrchr(rpid_domain, '@'))) {
			rpid_domain++;
			if ((p = strchr(rpid_domain, ';'))) {
				*p = '\0';
			}
		}

		if (sofia_test_pflag(tech_pvt->profile, PFLAG_AUTO_NAT)) {
			if (!zstr(tech_pvt->mparams.remote_ip) && !zstr(tech_pvt->profile->extsipip) && sofia_glue_check_nat(tech_pvt->profile, tech_pvt->mparams.remote_ip)) {
				rpid_domain = tech_pvt->profile->extsipip;
			} else {
				rpid_domain = tech_pvt->profile->sipip;
			}
		}

		if (!zstr(invite_domain)) {
			rpid_domain = (char *)invite_domain;
		}

		if (zstr(rpid_domain)) {
			rpid_domain = "cluecon.com";
		}

		/*
		 * Ignore transport chanvar and uri parameter for gateway connections
		 * since all of them have been already taken care of in mod_sofia.c:sofia_outgoing_channel()
		 */
		if (tech_pvt->transport == SOFIA_TRANSPORT_UNKNOWN && zstr(tech_pvt->gateway_name)) {
			if ((p = (char *) switch_stristr("port=", url))) {
				p += 5;
				tech_pvt->transport = sofia_glue_str2transport(p);
			} else {
				if ((t_var = switch_channel_get_variable(channel, "sip_transport"))) {
					tech_pvt->transport = sofia_glue_str2transport(t_var);
				}
			}

			if (tech_pvt->transport == SOFIA_TRANSPORT_UNKNOWN) {
				tech_pvt->transport = SOFIA_TRANSPORT_UDP;
			}
		}

		if (!zstr(tech_pvt->mparams.remote_ip) && sofia_glue_check_nat(tech_pvt->profile, tech_pvt->mparams.remote_ip)) {
			tech_pvt->user_via = sofia_glue_create_external_via(session, tech_pvt->profile, tech_pvt->transport);
		}

		if (!sofia_test_pflag(tech_pvt->profile, PFLAG_TLS) && sofia_glue_transport_has_tls(tech_pvt->transport)) {
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(tech_pvt->session), SWITCH_LOG_ERROR, "TLS not supported by profile\n");
			return SWITCH_STATUS_FALSE;
		}

		if (zstr(tech_pvt->invite_contact)) {
			const char *contact;
			if ((contact = switch_channel_get_variable(channel, "sip_contact_user"))) {
				char *ip_addr = tech_pvt->profile->sipip;
				char *ipv6;

				if ( !zstr(tech_pvt->mparams.remote_ip) && sofia_glue_check_nat(tech_pvt->profile, tech_pvt->mparams.remote_ip ) ) {
					ip_addr = tech_pvt->profile->extsipip;
				}

				ipv6 = strchr(ip_addr, ':');

				if (sofia_glue_transport_has_tls(tech_pvt->transport)) {
					tech_pvt->invite_contact = switch_core_session_sprintf(session, "sip:%s@%s%s%s:%d", contact,
																		   ipv6 ? "[" : "", ip_addr, ipv6 ? "]" : "", tech_pvt->profile->tls_sip_port);
				} else {
					tech_pvt->invite_contact = switch_core_session_sprintf(session, "sip:%s@%s%s%s:%d", contact,
																		   ipv6 ? "[" : "", ip_addr, ipv6 ? "]" : "", tech_pvt->profile->extsipport);
				}
			} else {
				if (sofia_glue_transport_has_tls(tech_pvt->transport)) {
					tech_pvt->invite_contact = tech_pvt->profile->tls_url;
				} else {
					if (!zstr(tech_pvt->mparams.remote_ip) && sofia_glue_check_nat(tech_pvt->profile, tech_pvt->mparams.remote_ip)) {
						tech_pvt->invite_contact = tech_pvt->profile->public_url;
					} else {
						tech_pvt->invite_contact = tech_pvt->profile->url;
					}
				}
			}
		}

		url_str = sofia_overcome_sip_uri_weakness(session, url, tech_pvt->transport, SWITCH_TRUE, invite_params, invite_tel_params);
		invite_contact = sofia_overcome_sip_uri_weakness(session, tech_pvt->invite_contact, tech_pvt->transport, SWITCH_FALSE, invite_contact_params, NULL);
		from_str = sofia_overcome_sip_uri_weakness(session, invite_from_uri ? invite_from_uri : use_from_str, 0, SWITCH_TRUE, invite_from_params, NULL);
		to_str = sofia_overcome_sip_uri_weakness(session, invite_to_uri ? invite_to_uri : tech_pvt->dest_to, 0, SWITCH_FALSE, invite_to_params, NULL);

		switch_channel_set_variable(channel, "sip_outgoing_contact_uri", invite_contact);


		/*
		  Does the "genius" who wanted SIP to be "text-based" so it was "easier to read" even use it now,
		  or did he just suggest it to make our lives miserable?
		*/
		use_from_str = from_str;

		if (!switch_stristr("sip:", use_from_str)) {
			use_from_str = switch_core_session_sprintf(session, "sip:%s", use_from_str);
		}

		if (!from_display && !strcasecmp(tech_pvt->caller_profile->caller_id_name, "_undef_")) {
			from_str = switch_core_session_sprintf(session, "<%s>", use_from_str);
		} else {
			char *name = switch_core_session_strdup(session, from_display ? from_display : tech_pvt->caller_profile->caller_id_name);
			check_decode(name, session);
			from_str = switch_core_session_sprintf(session, "\"%s\" <%s>", name, use_from_str);
		}

		if (!(call_id = switch_channel_get_variable(channel, "sip_invite_call_id"))) {
			if (sofia_test_pflag(tech_pvt->profile, PFLAG_UUID_AS_CALLID)) {
				call_id = switch_core_session_get_uuid(session);
			}
		}

		if (handle_full_from) {
			from_str = (char *) handle_full_from;
		}

		if (handle_full_to) {
			to_str = (char *) handle_full_to;
		}


		if (force_full_from) {
			from_str = (char *) force_full_from;
		}

		if (force_full_to) {
			to_str = (char *) force_full_to;
		}


		if (invite_req_uri) {
			url_str = (char *) invite_req_uri;
		}

		if (url_str) {
			char *s = NULL;
			if (!strncasecmp(url_str, "sip:", 4)) {
				s = url_str + 4;
			}
			if (!strncasecmp(url_str, "sips:", 5)) {
				s = url_str + 5;
			}

			/* tel: patch from jaybinks, added by MC
               It compiles but I don't have a way to test it
			*/
			if (!strncasecmp(url_str, "tel:", 4)) {
				switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(tech_pvt->session),
								  SWITCH_LOG_ERROR, "URL Error! tel: uri's not supported at this time\n");
				return SWITCH_STATUS_FALSE;
			}
			if (!s) {
				s = url_str;
			}
			switch_channel_set_variable(channel, "sip_req_uri", s);
		}
		
		switch_channel_set_variable(channel, "sip_to_host", sofia_glue_get_host(to_str, switch_core_session_get_pool(session)));
		switch_channel_set_variable(channel, "sip_from_host", sofia_glue_get_host(from_str, switch_core_session_get_pool(session)));


		if (!(tech_pvt->nh = nua_handle(tech_pvt->profile->nua, NULL,
										NUTAG_URL(url_str),
										TAG_IF(call_id, SIPTAG_CALL_ID_STR(call_id)),
										TAG_IF(!zstr(record_route), SIPTAG_HEADER_STR(record_route)),
										SIPTAG_TO_STR(to_str), SIPTAG_FROM_STR(from_str), SIPTAG_CONTACT_STR(invite_contact), TAG_END()))) {

			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_CRIT, 
							  "Error creating HANDLE!\nurl_str=[%s]\ncall_id=[%s]\nto_str=[%s]\nfrom_str=[%s]\ninvite_contact=[%s]\n",
							  url_str,
							  call_id ? call_id : "N/A",
							  to_str,
							  from_str,
							  invite_contact);
			
			switch_safe_free(d_url);
			return SWITCH_STATUS_FALSE;
		}

		if (tech_pvt->dest && (strstr(tech_pvt->dest, ";fs_nat") || strstr(tech_pvt->dest, ";received")
							   || ((val = switch_channel_get_variable(channel, "sip_sticky_contact")) && switch_true(val)))) {
			sofia_set_flag(tech_pvt, TFLAG_NAT);
			tech_pvt->record_route = switch_core_session_strdup(tech_pvt->session, url_str);
			route_uri = tech_pvt->record_route;
			session_timeout = SOFIA_NAT_SESSION_TIMEOUT;
			switch_channel_set_variable(channel, "sip_nat_detected", "true");
		}

		if ((val = switch_channel_get_variable(channel, "sip_cid_type"))) {
			cid_type = sofia_cid_name2type(val);
		}

		if (switch_channel_test_flag(tech_pvt->channel, CF_RECOVERING) && switch_channel_direction(tech_pvt->channel) == SWITCH_CALL_DIRECTION_INBOUND) {
			if (zstr((use_name = switch_channel_get_variable(tech_pvt->channel, "effective_callee_id_name"))) &&
				zstr((use_name = switch_channel_get_variable(tech_pvt->channel, "sip_callee_id_name")))) {
				if (!(use_name = switch_channel_get_variable(tech_pvt->channel, "sip_to_display"))) {
					use_name = switch_channel_get_variable(tech_pvt->channel, "sip_to_user");
				}
			}

			if (zstr((use_number = switch_channel_get_variable(tech_pvt->channel, "effective_callee_id_number"))) &&
				zstr((use_number = switch_channel_get_variable(tech_pvt->channel, "sip_callee_id_number")))) {
				use_number = switch_channel_get_variable(tech_pvt->channel, "sip_to_user");
			}

			if (zstr(use_name) && zstr(use_name = tech_pvt->caller_profile->callee_id_name)) {
				use_name = tech_pvt->caller_profile->caller_id_name;
			}

			if (zstr(use_number) && zstr(use_number = tech_pvt->caller_profile->callee_id_number)) {
				use_number = tech_pvt->caller_profile->caller_id_number;
			}
		} else {
			use_name = tech_pvt->caller_profile->caller_id_name;
			use_number = tech_pvt->caller_profile->caller_id_number;
		}

		check_decode(use_name, session);

		switch (cid_type) {
		case CID_TYPE_PID:
			if (switch_test_flag(caller_profile, SWITCH_CPF_SCREEN)) {
				if (zstr(tech_pvt->caller_profile->caller_id_name) || !strcasecmp(tech_pvt->caller_profile->caller_id_name, "_undef_")) {
					tech_pvt->asserted_id = switch_core_session_sprintf(tech_pvt->session, "<sip:%s@%s>",
																		use_number, rpid_domain);
				} else {
					tech_pvt->asserted_id = switch_core_session_sprintf(tech_pvt->session, "\"%s\"<sip:%s@%s>",
																		use_name, use_number, rpid_domain);
				}
			} else {
				if (zstr(tech_pvt->caller_profile->caller_id_name) || !strcasecmp(tech_pvt->caller_profile->caller_id_name, "_undef_")) {
					tech_pvt->preferred_id = switch_core_session_sprintf(tech_pvt->session, "<sip:%s@%s>",
																		 tech_pvt->caller_profile->caller_id_number, rpid_domain);
				} else {
					tech_pvt->preferred_id = switch_core_session_sprintf(tech_pvt->session, "\"%s\"<sip:%s@%s>",
																		 tech_pvt->caller_profile->caller_id_name,
																		 tech_pvt->caller_profile->caller_id_number, rpid_domain);
				}
			}

			if (switch_test_flag(caller_profile, SWITCH_CPF_HIDE_NUMBER)) {
				tech_pvt->privacy = "id";
			} else {
				tech_pvt->privacy = "none";
			}

			break;
		case CID_TYPE_RPID:
			{
				if (switch_test_flag(caller_profile, SWITCH_CPF_HIDE_NAME)) {
					priv = "name";
					if (switch_test_flag(caller_profile, SWITCH_CPF_HIDE_NUMBER)) {
						priv = "full";
					}
				} else if (switch_test_flag(caller_profile, SWITCH_CPF_HIDE_NUMBER)) {
					priv = "full";
				}

				if (switch_test_flag(caller_profile, SWITCH_CPF_SCREEN)) {
					screen = "yes";
				}

				if (zstr(tech_pvt->caller_profile->caller_id_name) || !strcasecmp(tech_pvt->caller_profile->caller_id_name, "_undef_")) {
					tech_pvt->rpid = switch_core_session_sprintf(tech_pvt->session, "<sip:%s@%s>;party=calling;screen=%s;privacy=%s",
																 use_number, rpid_domain, screen, priv);
				} else {
					tech_pvt->rpid = switch_core_session_sprintf(tech_pvt->session, "\"%s\"<sip:%s@%s>;party=calling;screen=%s;privacy=%s",
																 use_name, use_number, rpid_domain, screen, priv);
				}
			}
			break;
		default:
			break;
		}


		switch_safe_free(d_url);

		if (!(sofia_private = su_alloc(tech_pvt->nh->nh_home, sizeof(*sofia_private)))) {
			abort();
		}

		memset(sofia_private, 0, sizeof(*sofia_private));
		sofia_private->is_call = 2;
		sofia_private->is_static++;

		if (switch_channel_test_flag(tech_pvt->channel, CF_RECOVERING)) {
			sofia_private->is_call++;
		}

		tech_pvt->sofia_private = sofia_private;
		switch_copy_string(tech_pvt->sofia_private->uuid, switch_core_session_get_uuid(session), sizeof(tech_pvt->sofia_private->uuid));
		nua_handle_bind(tech_pvt->nh, tech_pvt->sofia_private);
	}

	if (tech_pvt->e_dest && sofia_test_pflag(tech_pvt->profile, PFLAG_IN_DIALOG_CHAT)) {
		char *user = NULL, *host = NULL;
		char hash_key[256] = "";

		e_dest = strdup(tech_pvt->e_dest);
		switch_assert(e_dest != NULL);
		user = e_dest;

		if ((host = strchr(user, '@'))) {
			*host++ = '\0';
		}
		switch_snprintf(hash_key, sizeof(hash_key), "%s%s%s", user, host, cid_num);

		tech_pvt->chat_from = tech_pvt->from_str;
		tech_pvt->chat_to = tech_pvt->dest;
		if (tech_pvt->profile->pres_type) {
			tech_pvt->hash_key = switch_core_session_strdup(tech_pvt->session, hash_key);
			switch_mutex_lock(tech_pvt->profile->flag_mutex);
			switch_core_hash_insert(tech_pvt->profile->chat_hash, tech_pvt->hash_key, tech_pvt);
			switch_mutex_unlock(tech_pvt->profile->flag_mutex);
		}
		free(e_dest);
	}

	holdstr = sofia_test_flag(tech_pvt, TFLAG_SIP_HOLD) ? "*" : "";

	if (!switch_channel_get_variable(channel, "sofia_profile_name")) {
		switch_channel_set_variable(channel, "sofia_profile_name", tech_pvt->profile->name);
		switch_channel_set_variable(channel, "recovery_profile_name", tech_pvt->profile->name);
	}

	extra_headers = sofia_glue_get_extra_headers(channel, SOFIA_SIP_HEADER_PREFIX);

	session_timeout = tech_pvt->profile->session_timeout;

	if ((val = switch_channel_get_variable(channel, SOFIA_SESSION_TIMEOUT))) {
		int v_session_timeout = atoi(val);
		if (v_session_timeout >= 0) {
			session_timeout = v_session_timeout;
		}
	}

	if (switch_channel_test_flag(channel, CF_PROXY_MEDIA)) {
		if (switch_core_media_ready(tech_pvt->session, SWITCH_MEDIA_TYPE_AUDIO)) {
			switch_core_media_proxy_remote_addr(session, NULL);
		}
		switch_core_media_patch_sdp(tech_pvt->session);
	}

	if (!zstr(tech_pvt->dest)) {
		dst = sofia_glue_get_destination(tech_pvt->dest);

		if (dst->route_uri) {
			route_uri = sofia_overcome_sip_uri_weakness(tech_pvt->session, dst->route_uri, tech_pvt->transport, SWITCH_TRUE, NULL, NULL);
		}

		if (dst->route) {
			route = dst->route;
		}
	}

	if ((val = switch_channel_get_variable(channel, "sip_route_uri"))) {
		route_uri = switch_core_session_strdup(session, val);
		route = NULL;
	}

	if (route_uri) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(tech_pvt->session), SWITCH_LOG_DEBUG, "%s Setting proxy route to %s\n", route_uri,
						  switch_channel_get_name(channel));
		tech_pvt->route_uri = switch_core_session_strdup(tech_pvt->session, route_uri);
	}


	if ((val = switch_channel_get_variable(tech_pvt->channel, "sip_invite_cseq"))) {
		uint32_t callsequence = (uint32_t) strtoul(val, NULL, 10);
		cseq = sip_cseq_create(tech_pvt->nh->nh_home, callsequence, SIP_METHOD_INVITE);
	}


	switch_channel_clear_flag(channel, CF_MEDIA_ACK);

	if (handle_full_from) {
		tech_pvt->nh->nh_has_invite = 1;
	}

	if ((mp = sofia_media_get_multipart(session, SOFIA_MULTIPART_PREFIX, tech_pvt->mparams.local_sdp_str, &mp_type))) {
		sofia_clear_flag(tech_pvt, TFLAG_ENABLE_SOA);
	}

	if ((tech_pvt->session_timeout = session_timeout)) {
		tech_pvt->session_refresher = switch_channel_direction(channel) == SWITCH_CALL_DIRECTION_OUTBOUND ? nua_local_refresher : nua_remote_refresher;
	} else {
		tech_pvt->session_refresher = nua_no_refresher;
	}

	if (tech_pvt->mparams.local_sdp_str) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(tech_pvt->session), SWITCH_LOG_DEBUG,
						  "Local SDP:\n%s\n", tech_pvt->mparams.local_sdp_str);
	}

	if (sofia_use_soa(tech_pvt)) {
		nua_invite(tech_pvt->nh,
				   NUTAG_AUTOANSWER(0),
				   //TAG_IF(zstr(tech_pvt->mparams.local_sdp_str), NUTAG_AUTOACK(0)),
				   //TAG_IF(!zstr(tech_pvt->mparams.local_sdp_str), NUTAG_AUTOACK(1)),
				   // The code above is breaking things...... grrr WE need this because we handle our own acks and there are 3pcc cases in there too
				   NUTAG_AUTOACK(0),
				   NUTAG_SESSION_TIMER(tech_pvt->session_timeout),
				   NUTAG_SESSION_REFRESHER(tech_pvt->session_refresher),
				   TAG_IF(sofia_test_flag(tech_pvt, TFLAG_RECOVERED), NUTAG_INVITE_TIMER(UINT_MAX)),
				   TAG_IF(invite_full_from, SIPTAG_FROM_STR(invite_full_from)),
				   TAG_IF(invite_full_to, SIPTAG_TO_STR(invite_full_to)),
				   TAG_IF(tech_pvt->redirected, NUTAG_URL(tech_pvt->redirected)),
				   TAG_IF(!zstr(recover_via), SIPTAG_VIA_STR(recover_via)),
				   TAG_IF(!zstr(tech_pvt->user_via), SIPTAG_VIA_STR(tech_pvt->user_via)),
				   TAG_IF(!zstr(tech_pvt->rpid), SIPTAG_REMOTE_PARTY_ID_STR(tech_pvt->rpid)),
				   TAG_IF(!zstr(tech_pvt->preferred_id), SIPTAG_P_PREFERRED_IDENTITY_STR(tech_pvt->preferred_id)),
				   TAG_IF(!zstr(tech_pvt->asserted_id), SIPTAG_P_ASSERTED_IDENTITY_STR(tech_pvt->asserted_id)),
				   TAG_IF(!zstr(tech_pvt->privacy), SIPTAG_PRIVACY_STR(tech_pvt->privacy)),
				   TAG_IF(!zstr(alert_info), SIPTAG_HEADER_STR(alert_info)),
				   TAG_IF(!zstr(extra_headers), SIPTAG_HEADER_STR(extra_headers)),
				   TAG_IF(sofia_test_pflag(tech_pvt->profile, PFLAG_PASS_CALLEE_ID), SIPTAG_HEADER_STR("X-FS-Support: " FREESWITCH_SUPPORT)),
				   TAG_IF(!zstr(max_forwards), SIPTAG_MAX_FORWARDS_STR(max_forwards)),
				   TAG_IF(!zstr(route_uri), NUTAG_PROXY(route_uri)),
				   TAG_IF(!zstr(invite_route_uri), NUTAG_INITIAL_ROUTE_STR(invite_route_uri)),
				   TAG_IF(!zstr(route), SIPTAG_ROUTE_STR(route)),
				   TAG_IF(tech_pvt->profile->minimum_session_expires, NUTAG_MIN_SE(tech_pvt->profile->minimum_session_expires)),
				   TAG_IF(cseq, SIPTAG_CSEQ(cseq)),
				   TAG_IF(zstr(tech_pvt->mparams.local_sdp_str), SIPTAG_PAYLOAD_STR("")),
				   TAG_IF(!zstr(tech_pvt->mparams.local_sdp_str), SOATAG_ADDRESS(tech_pvt->mparams.adv_sdp_audio_ip)),
				   TAG_IF(!zstr(tech_pvt->mparams.local_sdp_str), SOATAG_USER_SDP_STR(tech_pvt->mparams.local_sdp_str)),
				   TAG_IF(!zstr(tech_pvt->mparams.local_sdp_str), SOATAG_REUSE_REJECTED(1)),
				   TAG_IF(!zstr(tech_pvt->mparams.local_sdp_str), SOATAG_ORDERED_USER(1)),
				   TAG_IF(!zstr(tech_pvt->mparams.local_sdp_str), SOATAG_RTP_SORT(SOA_RTP_SORT_REMOTE)),
				   TAG_IF(!zstr(tech_pvt->mparams.local_sdp_str), SOATAG_RTP_SELECT(SOA_RTP_SELECT_ALL)),
				   TAG_IF(rep, SIPTAG_REPLACES_STR(rep)),
				   TAG_IF(!require_timer, NUTAG_TIMER_AUTOREQUIRE(0)),
				   TAG_IF(!zstr(tech_pvt->mparams.local_sdp_str), SOATAG_HOLD(holdstr)), TAG_END());
	} else {
		nua_invite(tech_pvt->nh,
				   NUTAG_AUTOANSWER(0),
				   NUTAG_AUTOACK(0),
				   NUTAG_SESSION_TIMER(tech_pvt->session_timeout),
				   NUTAG_SESSION_REFRESHER(tech_pvt->session_refresher),
				   TAG_IF(sofia_test_flag(tech_pvt, TFLAG_RECOVERED), NUTAG_INVITE_TIMER(UINT_MAX)),
				   TAG_IF(invite_full_from, SIPTAG_FROM_STR(invite_full_from)),
				   TAG_IF(invite_full_to, SIPTAG_TO_STR(invite_full_to)),
				   TAG_IF(tech_pvt->redirected, NUTAG_URL(tech_pvt->redirected)),
				   TAG_IF(!zstr(recover_via), SIPTAG_VIA_STR(recover_via)),
				   TAG_IF(!zstr(tech_pvt->user_via), SIPTAG_VIA_STR(tech_pvt->user_via)),
				   TAG_IF(!zstr(tech_pvt->rpid), SIPTAG_REMOTE_PARTY_ID_STR(tech_pvt->rpid)),
				   TAG_IF(!zstr(tech_pvt->preferred_id), SIPTAG_P_PREFERRED_IDENTITY_STR(tech_pvt->preferred_id)),
				   TAG_IF(!zstr(tech_pvt->asserted_id), SIPTAG_P_ASSERTED_IDENTITY_STR(tech_pvt->asserted_id)),
				   TAG_IF(!zstr(tech_pvt->privacy), SIPTAG_PRIVACY_STR(tech_pvt->privacy)),
				   TAG_IF(!zstr(alert_info), SIPTAG_HEADER_STR(alert_info)),
				   TAG_IF(!zstr(extra_headers), SIPTAG_HEADER_STR(extra_headers)),
				   TAG_IF(sofia_test_pflag(tech_pvt->profile, PFLAG_PASS_CALLEE_ID), SIPTAG_HEADER_STR("X-FS-Support: " FREESWITCH_SUPPORT)),
				   TAG_IF(!zstr(max_forwards), SIPTAG_MAX_FORWARDS_STR(max_forwards)),
				   TAG_IF(!zstr(route_uri), NUTAG_PROXY(route_uri)),
				   TAG_IF(!zstr(route), SIPTAG_ROUTE_STR(route)),
				   TAG_IF(!zstr(invite_route_uri), NUTAG_INITIAL_ROUTE_STR(invite_route_uri)),
				   TAG_IF(tech_pvt->profile->minimum_session_expires, NUTAG_MIN_SE(tech_pvt->profile->minimum_session_expires)),
				   TAG_IF(!require_timer, NUTAG_TIMER_AUTOREQUIRE(0)),
				   TAG_IF(cseq, SIPTAG_CSEQ(cseq)),
				   NUTAG_MEDIA_ENABLE(0),
				   SIPTAG_CONTENT_TYPE_STR(mp_type ? mp_type : "application/sdp"),
				   SIPTAG_PAYLOAD_STR(mp ? mp : tech_pvt->mparams.local_sdp_str), TAG_IF(rep, SIPTAG_REPLACES_STR(rep)), SOATAG_HOLD(holdstr), TAG_END());
	}

	sofia_glue_free_destination(dst);
	switch_safe_free(extra_headers);
	switch_safe_free(mp);
	tech_pvt->redirected = NULL;

	return SWITCH_STATUS_SUCCESS;
}

void sofia_glue_do_xfer_invite(switch_core_session_t *session)
{
	private_object_t *tech_pvt = switch_core_session_get_private(session);
	switch_channel_t *channel = switch_core_session_get_channel(session);
	switch_caller_profile_t *caller_profile;
	const char *sipip, *format, *contact_url;

	switch_assert(tech_pvt != NULL);
	switch_mutex_lock(tech_pvt->sofia_mutex);
	caller_profile = switch_channel_get_caller_profile(channel);

	if (!zstr(tech_pvt->mparams.remote_ip) && sofia_glue_check_nat(tech_pvt->profile, tech_pvt->mparams.remote_ip)) {
		sipip = tech_pvt->profile->extsipip;
		contact_url = tech_pvt->profile->public_url;
	} else {
		sipip = tech_pvt->profile->extsipip ? tech_pvt->profile->extsipip : tech_pvt->profile->sipip;
		contact_url = tech_pvt->profile->url;
	}

	format = strchr(sipip, ':') ? "\"%s\" <sip:%s@[%s]>" : "\"%s\" <sip:%s@%s>";

	if ((tech_pvt->from_str = switch_core_session_sprintf(session, format, caller_profile->caller_id_name, caller_profile->caller_id_number, sipip))) {

		const char *rep = switch_channel_get_variable(channel, SOFIA_REPLACES_HEADER);

		tech_pvt->nh2 = nua_handle(tech_pvt->profile->nua, NULL,
								   SIPTAG_TO_STR(tech_pvt->dest), SIPTAG_FROM_STR(tech_pvt->from_str), SIPTAG_CONTACT_STR(contact_url), TAG_END());

		nua_handle_bind(tech_pvt->nh2, tech_pvt->sofia_private);

		nua_invite(tech_pvt->nh2,
				   SIPTAG_CONTACT_STR(contact_url),
				   TAG_IF(!zstr(tech_pvt->user_via), SIPTAG_VIA_STR(tech_pvt->user_via)),
				   SOATAG_ADDRESS(tech_pvt->mparams.adv_sdp_audio_ip),
				   SOATAG_USER_SDP_STR(tech_pvt->mparams.local_sdp_str),
				   SOATAG_REUSE_REJECTED(1),
				   SOATAG_ORDERED_USER(1),
				   SOATAG_RTP_SORT(SOA_RTP_SORT_REMOTE), SOATAG_RTP_SELECT(SOA_RTP_SELECT_ALL), TAG_IF(rep, SIPTAG_REPLACES_STR(rep)), TAG_END());
	} else {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(tech_pvt->session), SWITCH_LOG_ERROR, "Memory Error!\n");
	}
	switch_mutex_unlock(tech_pvt->sofia_mutex);
}


/* map sip responses to QSIG cause codes ala RFC4497 section 8.4.4 */
switch_call_cause_t sofia_glue_sip_cause_to_freeswitch(int status)
{
	switch (status) {
	case 200:
		return SWITCH_CAUSE_NORMAL_CLEARING;
	case 401:
	case 402:
	case 403:
	case 407:
	case 603:
		return SWITCH_CAUSE_CALL_REJECTED;
	case 404:
		return SWITCH_CAUSE_UNALLOCATED_NUMBER;
	case 485:
	case 604:
		return SWITCH_CAUSE_NO_ROUTE_DESTINATION;
	case 408:
	case 504:
		return SWITCH_CAUSE_RECOVERY_ON_TIMER_EXPIRE;
	case 410:
		return SWITCH_CAUSE_NUMBER_CHANGED;
	case 413:
	case 414:
	case 416:
	case 420:
	case 421:
	case 423:
	case 505:
	case 513:
		return SWITCH_CAUSE_INTERWORKING;
	case 480:
		return SWITCH_CAUSE_NO_USER_RESPONSE;
	case 400:
	case 481:
	case 500:
	case 503:
		return SWITCH_CAUSE_NORMAL_TEMPORARY_FAILURE;
	case 486:
	case 600:
		return SWITCH_CAUSE_USER_BUSY;
	case 484:
		return SWITCH_CAUSE_INVALID_NUMBER_FORMAT;
	case 488:
	case 606:
		return SWITCH_CAUSE_INCOMPATIBLE_DESTINATION;
	case 502:
		return SWITCH_CAUSE_NETWORK_OUT_OF_ORDER;
	case 405:
		return SWITCH_CAUSE_SERVICE_UNAVAILABLE;
	case 406:
	case 415:
	case 501:
		return SWITCH_CAUSE_SERVICE_NOT_IMPLEMENTED;
	case 482:
	case 483:
		return SWITCH_CAUSE_EXCHANGE_ROUTING_ERROR;
	case 487:
		return SWITCH_CAUSE_ORIGINATOR_CANCEL;
	default:
		return SWITCH_CAUSE_NORMAL_UNSPECIFIED;
	}
}

void sofia_glue_pass_sdp(private_object_t *tech_pvt, char *sdp)
{
	const char *val;
	switch_core_session_t *other_session;
	switch_channel_t *other_channel;

	if ((val = switch_channel_get_partner_uuid(tech_pvt->channel))
		&& (other_session = switch_core_session_locate(val))) {
		other_channel = switch_core_session_get_channel(other_session);
		switch_channel_set_variable(other_channel, SWITCH_B_SDP_VARIABLE, sdp);

		if (!sofia_test_flag(tech_pvt, TFLAG_CHANGE_MEDIA) && !switch_channel_test_flag(tech_pvt->channel, CF_RECOVERING) &&
			(switch_channel_direction(other_channel) == SWITCH_CALL_DIRECTION_OUTBOUND &&
 switch_channel_direction(tech_pvt->channel) == SWITCH_CALL_DIRECTION_OUTBOUND && switch_channel_test_flag(tech_pvt->channel, CF_PROXY_MODE))) {
			switch_ivr_nomedia(val, SMF_FORCE);
			sofia_set_flag_locked(tech_pvt, TFLAG_CHANGE_MEDIA);
		}
		switch_core_session_rwunlock(other_session);
	}
}

char *sofia_glue_get_path_from_contact(char *buf)
{
	char *p, *e, *path = NULL, *contact = NULL;

	if (!buf) return NULL;

	contact = sofia_glue_get_url_from_contact(buf, SWITCH_TRUE);

	if (!contact) return NULL;
	
	if ((p = strstr(contact, "fs_path="))) {
		p += 8;

		if (!zstr(p)) {
			path = strdup(p);
		}
	}

	if (!path) return NULL;

	if ((e = strrchr(path, ';'))) {
		*e = '\0';
	}

	switch_url_decode(path);

	free(contact);

	return path;
}

char *sofia_glue_get_url_from_contact(char *buf, uint8_t to_dup)
{
	char *url = NULL, *e;

	switch_assert(buf);
	
	while(*buf == ' ') {
		buf++;
	}

	if (*buf == '"') {
		buf++;
		if((e = strchr(buf, '"'))) {
			buf = e+1;
		}
	}

	while(*buf == ' ') {
		buf++;
	}

	if ((url = strchr(buf, '<')) && (e = switch_find_end_paren(url, '<', '>'))) {
		url++;
		if (to_dup) {
			url = strdup(url);
			e = strchr(url, '>');
		}

		*e = '\0';
	} else {
		if (to_dup) {
			url = strdup(buf);
		} else {
			url = buf;
		}
	}
	return url;
}

switch_status_t sofia_glue_profile_rdlock__(const char *file, const char *func, int line, sofia_profile_t *profile)
{
	switch_status_t status = switch_thread_rwlock_tryrdlock(profile->rwlock);
	if (status != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_ID_LOG, file, func, line, NULL, SWITCH_LOG_ERROR, "Profile %s is locked\n", profile->name);
		return status;
	}
#ifdef SOFIA_DEBUG_RWLOCKS
	switch_log_printf(SWITCH_CHANNEL_ID_LOG, file, func, line, NULL, SWITCH_LOG_ERROR, "XXXXXXXXXXXXXX LOCK %s\n", profile->name);
#endif
	return status;
}

switch_bool_t sofia_glue_profile_exists(const char *key)
{
	switch_bool_t tf = SWITCH_FALSE;
	
	switch_mutex_lock(mod_sofia_globals.hash_mutex);
	if (switch_core_hash_find(mod_sofia_globals.profile_hash, key)) {
		tf = SWITCH_TRUE;
	}
	switch_mutex_unlock(mod_sofia_globals.hash_mutex);

	return tf;
}

sofia_profile_t *sofia_glue_find_profile__(const char *file, const char *func, int line, const char *key)
{
	sofia_profile_t *profile;

	switch_mutex_lock(mod_sofia_globals.hash_mutex);
	if ((profile = (sofia_profile_t *) switch_core_hash_find(mod_sofia_globals.profile_hash, key))) {
		if (!sofia_test_pflag(profile, PFLAG_RUNNING)) {
#ifdef SOFIA_DEBUG_RWLOCKS
			switch_log_printf(SWITCH_CHANNEL_ID_LOG, file, func, line, NULL, SWITCH_LOG_ERROR, "Profile %s is not running\n", profile->name);
#endif
			profile = NULL;
			goto done;
		}
		if (sofia_glue_profile_rdlock__(file, func, line, profile) != SWITCH_STATUS_SUCCESS) {
			profile = NULL;
		}
	} else {
#ifdef SOFIA_DEBUG_RWLOCKS
		switch_log_printf(SWITCH_CHANNEL_ID_LOG, file, func, line, NULL, SWITCH_LOG_ERROR, "Profile %s is not in the hash\n", key);
#endif
	}

 done:
	switch_mutex_unlock(mod_sofia_globals.hash_mutex);

	return profile;
}

void sofia_glue_release_profile__(const char *file, const char *func, int line, sofia_profile_t *profile)
{
	if (profile) {
#ifdef SOFIA_DEBUG_RWLOCKS
		switch_log_printf(SWITCH_CHANNEL_ID_LOG, file, func, line, NULL, SWITCH_LOG_ERROR, "XXXXXXXXXXXXXX UNLOCK %s\n", profile->name);
#endif
		switch_thread_rwlock_unlock(profile->rwlock);
	}
}

switch_status_t sofia_glue_add_profile(char *key, sofia_profile_t *profile)
{
	switch_status_t status = SWITCH_STATUS_FALSE;

	switch_mutex_lock(mod_sofia_globals.hash_mutex);
	if (!switch_core_hash_find(mod_sofia_globals.profile_hash, key)) {
		status = switch_core_hash_insert(mod_sofia_globals.profile_hash, key, profile);
	}
	switch_mutex_unlock(mod_sofia_globals.hash_mutex);

	return status;
}


void sofia_glue_del_every_gateway(sofia_profile_t *profile)
{
	sofia_gateway_t *gp = NULL;

	switch_mutex_lock(mod_sofia_globals.hash_mutex);
	for (gp = profile->gateways; gp; gp = gp->next) {
		sofia_glue_del_gateway(gp);
	}
	switch_mutex_unlock(mod_sofia_globals.hash_mutex);
}


void sofia_glue_gateway_list(sofia_profile_t *profile, switch_stream_handle_t *stream, int up)
{
	sofia_gateway_t *gp = NULL;
	char *r = (char *) stream->data;

	switch_mutex_lock(mod_sofia_globals.hash_mutex);
	for (gp = profile->gateways; gp; gp = gp->next) {
		int reged = (gp->status == SOFIA_GATEWAY_UP);
		
		if (up ? reged : !reged) {
			stream->write_function(stream, "%s ", gp->name);
		}
	}

	if (r) {
		end_of(r) = '\0';
	}

	switch_mutex_unlock(mod_sofia_globals.hash_mutex);
}


void sofia_glue_del_gateway(sofia_gateway_t *gp)
{
	if (!gp->deleted) {
		if (gp->state != REG_STATE_NOREG) {
			gp->retry = 0;
			gp->state = REG_STATE_UNREGISTER;
		}

		gp->deleted = 1;
	}
}

void sofia_glue_restart_all_profiles(void)
{
	switch_hash_index_t *hi;
	const void *var;
	void *val;
	sofia_profile_t *pptr;
	switch_xml_t xml_root;
	const char *err;

	if ((xml_root = switch_xml_open_root(1, &err))) {
		switch_xml_free(xml_root);
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "Reload XML [%s]\n", err);
	}

	switch_mutex_lock(mod_sofia_globals.hash_mutex);
	if (mod_sofia_globals.profile_hash) {
		for (hi = switch_hash_first(NULL, mod_sofia_globals.profile_hash); hi; hi = switch_hash_next(hi)) {
			switch_hash_this(hi, &var, NULL, &val);
			if ((pptr = (sofia_profile_t *) val)) {
				int rsec = 10;
				int diff = (int) (switch_epoch_time_now(NULL) - pptr->started);
				int remain = rsec - diff;
				if (sofia_test_pflag(pptr, PFLAG_RESPAWN) || !sofia_test_pflag(pptr, PFLAG_RUNNING)) {
					continue;
				}

				if (diff < rsec) {
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR,
									  "Profile %s must be up for at least %d seconds to stop/restart.\nPlease wait %d second%s\n",
									  pptr->name, rsec, remain, remain == 1 ? "" : "s");
					continue;
				}
				sofia_set_pflag_locked(pptr, PFLAG_RESPAWN);
				sofia_clear_pflag_locked(pptr, PFLAG_RUNNING);
			}
		}
	}
	switch_mutex_unlock(mod_sofia_globals.hash_mutex);

}


void sofia_glue_global_siptrace(switch_bool_t on)
{
	switch_hash_index_t *hi;
	const void *var;
	void *val;
	sofia_profile_t *pptr;

	switch_mutex_lock(mod_sofia_globals.hash_mutex);
	if (mod_sofia_globals.profile_hash) {
		for (hi = switch_hash_first(NULL, mod_sofia_globals.profile_hash); hi; hi = switch_hash_next(hi)) {
			switch_hash_this(hi, &var, NULL, &val);
			if ((pptr = (sofia_profile_t *) val)) {
				nua_set_params(pptr->nua, TPTAG_LOG(on), TAG_END());				
			}
		}
	}
	switch_mutex_unlock(mod_sofia_globals.hash_mutex);

}

void sofia_glue_global_standby(switch_bool_t on)
{
	switch_hash_index_t *hi;
	const void *var;
	void *val;
	sofia_profile_t *pptr;

	switch_mutex_lock(mod_sofia_globals.hash_mutex);
	if (mod_sofia_globals.profile_hash) {
		for (hi = switch_hash_first(NULL, mod_sofia_globals.profile_hash); hi; hi = switch_hash_next(hi)) {
			switch_hash_this(hi, &var, NULL, &val);
			if ((pptr = (sofia_profile_t *) val)) {
				if (on) {
					sofia_set_pflag_locked(pptr, PFLAG_STANDBY);
				} else {
					sofia_clear_pflag_locked(pptr, PFLAG_STANDBY);
				}
			}
		}
	}
	switch_mutex_unlock(mod_sofia_globals.hash_mutex);

}

void sofia_glue_global_capture(switch_bool_t on)
{
       switch_hash_index_t *hi;
       const void *var;
       void *val;
       sofia_profile_t *pptr;

       switch_mutex_lock(mod_sofia_globals.hash_mutex);
       if (mod_sofia_globals.profile_hash) {
               for (hi = switch_hash_first(NULL, mod_sofia_globals.profile_hash); hi; hi = switch_hash_next(hi)) {
                       switch_hash_this(hi, &var, NULL, &val);
                       if ((pptr = (sofia_profile_t *) val)) {
                               nua_set_params(pptr->nua, TPTAG_CAPT(on ? mod_sofia_globals.capture_server : NULL), TAG_END());
                       }
               }
       }
       switch_mutex_unlock(mod_sofia_globals.hash_mutex);

}


void sofia_glue_global_watchdog(switch_bool_t on)
{
	switch_hash_index_t *hi;
	const void *var;
	void *val;
	sofia_profile_t *pptr;

	switch_mutex_lock(mod_sofia_globals.hash_mutex);
	if (mod_sofia_globals.profile_hash) {
		for (hi = switch_hash_first(NULL, mod_sofia_globals.profile_hash); hi; hi = switch_hash_next(hi)) {
			switch_hash_this(hi, &var, NULL, &val);
			if ((pptr = (sofia_profile_t *) val)) {
				pptr->watchdog_enabled = (on ? 1 : 0);
			}
		}
	}
	switch_mutex_unlock(mod_sofia_globals.hash_mutex);

}

void sofia_glue_del_profile(sofia_profile_t *profile)
{
	sofia_gateway_t *gp;
	char *aliases[512];
	int i = 0, j = 0;
	switch_hash_index_t *hi;
	const void *var;
	void *val;
	sofia_profile_t *pptr;

	switch_mutex_lock(mod_sofia_globals.hash_mutex);
	if (mod_sofia_globals.profile_hash) {
		for (hi = switch_hash_first(NULL, mod_sofia_globals.profile_hash); hi; hi = switch_hash_next(hi)) {
			switch_hash_this(hi, &var, NULL, &val);
			if ((pptr = (sofia_profile_t *) val) && pptr == profile) {
				aliases[i++] = strdup((char *) var);
				if (i == 512) {
					abort();
				}
			}
		}

		for (j = 0; j < i && j < 512; j++) {
			switch_core_hash_delete(mod_sofia_globals.profile_hash, aliases[j]);
			free(aliases[j]);
		}

		for (gp = profile->gateways; gp; gp = gp->next) {
			char *pkey = switch_mprintf("%s::%s", profile->name, gp->name);

			switch_core_hash_delete(mod_sofia_globals.gateway_hash, gp->name);
			switch_core_hash_delete(mod_sofia_globals.gateway_hash, pkey);
			switch_safe_free(pkey);
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "deleted gateway %s from profile %s\n", gp->name, profile->name);
		}
		profile->gateways = NULL;
	}
	switch_mutex_unlock(mod_sofia_globals.hash_mutex);
}

int sofia_recover_callback(switch_core_session_t *session) 
{

	switch_channel_t *channel = switch_core_session_get_channel(session);
	private_object_t *tech_pvt = NULL;
	sofia_profile_t *profile = NULL;
	const char *tmp;
	const char *rr;
	int r = 0;
	const char *profile_name = switch_channel_get_variable_dup(channel, "recovery_profile_name", SWITCH_FALSE, -1);
	

	if (zstr(profile_name)) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_CRIT, "Missing profile\n");
		return 0;
	}

	if (!(profile = sofia_glue_find_profile(profile_name))) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_CRIT, "Invalid profile %s\n", profile_name);
		return 0;
	}


	tech_pvt = (private_object_t *) switch_core_session_alloc(session, sizeof(private_object_t));
	tech_pvt->channel = channel;

	switch_mutex_init(&tech_pvt->flag_mutex, SWITCH_MUTEX_NESTED, switch_core_session_get_pool(session));
	switch_mutex_init(&tech_pvt->sofia_mutex, SWITCH_MUTEX_NESTED, switch_core_session_get_pool(session));

	tech_pvt->mparams.remote_ip = (char *) switch_channel_get_variable(channel, "sip_network_ip");
	tech_pvt->mparams.remote_port = atoi(switch_str_nil(switch_channel_get_variable(channel, "sip_network_port")));
	tech_pvt->caller_profile = switch_channel_get_caller_profile(channel);

	if ((tmp = switch_channel_get_variable(tech_pvt->channel, "rtp_2833_send_payload"))) {
		int te = atoi(tmp);
		if (te > 64) {
			tech_pvt->te = te;
		} 
	}

	if ((tmp = switch_channel_get_variable(tech_pvt->channel, "rtp_2833_recv_payload"))) {
		int te = atoi(tmp);
		if (te > 64) {
			tech_pvt->recv_te = te;
		} 
	}

	rr = switch_channel_get_variable(channel, "sip_invite_record_route");

	if (switch_channel_direction(channel) == SWITCH_CALL_DIRECTION_OUTBOUND) {
		int break_rfc = switch_true(switch_channel_get_variable(channel, "sip_recovery_break_rfc"));
		tech_pvt->dest = switch_core_session_sprintf(session, "sip:%s", switch_channel_get_variable(channel, "sip_req_uri"));
		switch_channel_set_variable(channel, "sip_handle_full_from", switch_channel_get_variable(channel, break_rfc ? "sip_full_to" : "sip_full_from"));
		switch_channel_set_variable(channel, "sip_handle_full_to", switch_channel_get_variable(channel, break_rfc ? "sip_full_from" : "sip_full_to"));
	} else {
		tech_pvt->redirected = switch_core_session_sprintf(session, "sip:%s", switch_channel_get_variable(channel, "sip_contact_uri"));

		if (zstr(rr)) {
			switch_channel_set_variable_printf(channel, "sip_invite_route_uri", "<sip:%s@%s:%s;lr>",
											   switch_channel_get_variable(channel, "sip_from_user"),
											   switch_channel_get_variable(channel, "sip_network_ip"), switch_channel_get_variable(channel, "sip_network_port")
											   );
		}

		tech_pvt->dest = switch_core_session_sprintf(session, "sip:%s", switch_channel_get_variable(channel, "sip_from_uri"));

		if (!switch_channel_get_variable_dup(channel, "sip_handle_full_from", SWITCH_FALSE, -1)) {
			switch_channel_set_variable(channel, "sip_handle_full_from", switch_channel_get_variable(channel, "sip_full_to"));
		}

		if (!switch_channel_get_variable_dup(channel, "sip_handle_full_to", SWITCH_FALSE, -1)) {
			switch_channel_set_variable(channel, "sip_handle_full_to", switch_channel_get_variable(channel, "sip_full_from"));
		}
	}

	if (rr) {
		switch_channel_set_variable(channel, "sip_invite_route_uri", rr);
	}

	tech_pvt->dest_to = tech_pvt->dest;

	sofia_glue_attach_private(session, profile, tech_pvt, NULL);
	switch_channel_set_name(tech_pvt->channel, switch_channel_get_variable(channel, "channel_name"));

	
	switch_channel_set_variable(channel, "sip_invite_call_id", switch_channel_get_variable(channel, "sip_call_id"));

	if (switch_true(switch_channel_get_variable(channel, "sip_nat_detected"))) {
		switch_channel_set_variable_printf(channel, "sip_route_uri", "sip:%s@%s:%s",
										   switch_channel_get_variable(channel, "sip_req_user"),
										   switch_channel_get_variable(channel, "sip_network_ip"), switch_channel_get_variable(channel, "sip_network_port")
										   );
	}

	if (session) {
		const char *use_uuid;

		switch_channel_set_flag(channel, CF_RECOVERING);

		if ((use_uuid = switch_channel_get_variable(channel, "origination_uuid"))) {
			if (switch_core_session_set_uuid(session, use_uuid) == SWITCH_STATUS_SUCCESS) {
				switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "%s set UUID=%s\n", switch_channel_get_name(channel),
								  use_uuid);
			} else {
				switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_CRIT, "%s set UUID=%s FAILED\n",
								  switch_channel_get_name(channel), use_uuid);
			}
		}
	
		switch_core_media_recover_session(session);
	
	}

	r++;

	return r;

}



int sofia_glue_recover(switch_bool_t flush)
{
	sofia_profile_t *profile;
	int r = 0;
	switch_console_callback_match_t *matches;


	if (list_profiles_full(NULL, NULL, &matches, SWITCH_FALSE) == SWITCH_STATUS_SUCCESS) {
		switch_console_callback_match_node_t *m;
		for (m = matches->head; m; m = m->next) {
			if ((profile = sofia_glue_find_profile(m->val))) {
				r += sofia_glue_profile_recover(profile, flush);
				sofia_glue_release_profile(profile);
			}
		}
		switch_console_free_matches(&matches);
	}
	return r;
}

int sofia_glue_profile_recover(sofia_profile_t *profile, switch_bool_t flush)
{
	int r = 0;

	if (profile) {
		sofia_clear_pflag_locked(profile, PFLAG_STANDBY);

		if (flush) {
			switch_core_recovery_flush(SOFIA_RECOVER, profile->name);
		} else {
			r = switch_core_recovery_recover(SOFIA_RECOVER, profile->name);
		}
	}

	return r;
}


int sofia_glue_init_sql(sofia_profile_t *profile)
{
	char *test_sql = NULL;

	char reg_sql[] =
		"CREATE TABLE sip_registrations (\n"
		"   call_id          VARCHAR(255),\n"
		"   sip_user         VARCHAR(255),\n"
		"   sip_host         VARCHAR(255),\n"
		"   presence_hosts   VARCHAR(255),\n"
		"   contact          VARCHAR(1024),\n"
		"   status           VARCHAR(255),\n"
		"   rpid             VARCHAR(255),\n"
		"   expires          INTEGER,\n"
		"   user_agent       VARCHAR(255),\n"
		"   server_user      VARCHAR(255),\n"
		"   server_host      VARCHAR(255),\n"
		"   profile_name     VARCHAR(255),\n"
		"   hostname         VARCHAR(255),\n"
		"   network_ip       VARCHAR(255),\n"
		"   network_port     VARCHAR(6),\n"
		"   sip_username     VARCHAR(255),\n"
		"   sip_realm        VARCHAR(255),\n"
		"   mwi_user         VARCHAR(255),\n"
		"   mwi_host         VARCHAR(255),\n"
		"   orig_server_host VARCHAR(255),\n"
		"   orig_hostname    VARCHAR(255),\n"
		"   sub_host         VARCHAR(255)\n"
		");\n";

	char pres_sql[] =
		"CREATE TABLE sip_presence (\n"
		"   sip_user        VARCHAR(255),\n"
		"   sip_host        VARCHAR(255),\n"
		"   status          VARCHAR(255),\n"
		"   rpid            VARCHAR(255),\n"
		"   expires         INTEGER,\n"
		"   user_agent      VARCHAR(255),\n"
		"   profile_name    VARCHAR(255),\n"
		"   hostname        VARCHAR(255),\n"
		"   network_ip      VARCHAR(255),\n"
		"   network_port    VARCHAR(6),\n"
		"   open_closed     VARCHAR(255)\n"
		");\n";

	char dialog_sql[] =
		"CREATE TABLE sip_dialogs (\n"
		"   call_id         VARCHAR(255),\n"
		"   uuid            VARCHAR(255),\n"
		"   sip_to_user     VARCHAR(255),\n"
		"   sip_to_host     VARCHAR(255),\n"
		"   sip_from_user   VARCHAR(255),\n"
		"   sip_from_host   VARCHAR(255),\n"
		"   contact_user    VARCHAR(255),\n"
		"   contact_host    VARCHAR(255),\n"
		"   state           VARCHAR(255),\n"
		"   direction       VARCHAR(255),\n"
		"   user_agent      VARCHAR(255),\n"
		"   profile_name    VARCHAR(255),\n"
		"   hostname        VARCHAR(255),\n"
		"   contact         VARCHAR(255),\n"
		"   presence_id     VARCHAR(255),\n"
		"   presence_data   VARCHAR(255),\n"
		"   call_info       VARCHAR(255),\n"
		"   call_info_state VARCHAR(255) default '',\n"
		"   expires         INTEGER default 0,\n"
		"   status          VARCHAR(255),\n"
		"   rpid            VARCHAR(255),\n"
		"   sip_to_tag      VARCHAR(255),\n"
		"   sip_from_tag    VARCHAR(255),\n"
		"   rcd             INTEGER not null default 0\n"
		");\n";

	char sub_sql[] =
		"CREATE TABLE sip_subscriptions (\n"
		"   proto           VARCHAR(255),\n"
		"   sip_user        VARCHAR(255),\n"
		"   sip_host        VARCHAR(255),\n"
		"   sub_to_user     VARCHAR(255),\n"
		"   sub_to_host     VARCHAR(255),\n"
		"   presence_hosts  VARCHAR(255),\n"
		"   event           VARCHAR(255),\n"
		"   contact         VARCHAR(1024),\n"
		"   call_id         VARCHAR(255),\n"
		"   full_from       VARCHAR(255),\n"
		"   full_via        VARCHAR(255),\n"
		"   expires         INTEGER,\n"
		"   user_agent      VARCHAR(255),\n"
		"   accept          VARCHAR(255),\n"
		"   profile_name    VARCHAR(255),\n"
		"   hostname        VARCHAR(255),\n"
		"   network_port    VARCHAR(6),\n"
		"   network_ip      VARCHAR(255),\n"
		"   version         INTEGER DEFAULT 0 NOT NULL,\n"
		"   orig_proto      VARCHAR(255),\n"
		"   full_to         VARCHAR(255)\n"
		");\n";

	char auth_sql[] =
		"CREATE TABLE sip_authentication (\n"
		"   nonce           VARCHAR(255),\n"
		"   expires         INTEGER,"
		"   profile_name    VARCHAR(255),\n"
		"   hostname        VARCHAR(255),\n"
		"   last_nc         INTEGER\n"
		");\n";

	/* should we move this glue to sofia_sla or keep it here where all db init happens? XXX MTK */
	char shared_appearance_sql[] =
		"CREATE TABLE sip_shared_appearance_subscriptions (\n"
		"   subscriber        VARCHAR(255),\n"
		"   call_id           VARCHAR(255),\n"
		"   aor               VARCHAR(255),\n"
		"   profile_name      VARCHAR(255),\n"
		"   hostname          VARCHAR(255),\n"
		"   contact_str       VARCHAR(255),\n"
		"   network_ip        VARCHAR(255)\n"
		");\n";

	char shared_appearance_dialogs_sql[] =
		"CREATE TABLE sip_shared_appearance_dialogs (\n"
		"   profile_name      VARCHAR(255),\n"
		"   hostname          VARCHAR(255),\n"
		"   contact_str       VARCHAR(255),\n"
		"   call_id           VARCHAR(255),\n"
		"   network_ip        VARCHAR(255),\n"
		"   expires           INTEGER\n"
		");\n";

	
	int x;
	char *indexes[] = {
		"create index sr_call_id on sip_registrations (call_id)",
		"create index sr_sip_user on sip_registrations (sip_user)",
		"create index sr_sip_host on sip_registrations (sip_host)",
		"create index sr_sub_host on sip_registrations (sub_host)",
		"create index sr_mwi_user on sip_registrations (mwi_user)",
		"create index sr_mwi_host on sip_registrations (mwi_host)",
		"create index sr_profile_name on sip_registrations (profile_name)",
		"create index sr_presence_hosts on sip_registrations (presence_hosts)",
		"create index sr_contact on sip_registrations (contact)",
		"create index sr_expires on sip_registrations (expires)",
		"create index sr_hostname on sip_registrations (hostname)",
		"create index sr_status on sip_registrations (status)",
		"create index sr_network_ip on sip_registrations (network_ip)",
		"create index sr_network_port on sip_registrations (network_port)",
		"create index sr_sip_username on sip_registrations (sip_username)",
		"create index sr_sip_realm on sip_registrations (sip_realm)",
		"create index sr_orig_server_host on sip_registrations (orig_server_host)",
		"create index sr_orig_hostname on sip_registrations (orig_hostname)",
		"create index ss_call_id on sip_subscriptions (call_id)",
		"create index ss_hostname on sip_subscriptions (hostname)",
		"create index ss_network_ip on sip_subscriptions (network_ip)",
		"create index ss_sip_user on sip_subscriptions (sip_user)",
		"create index ss_sip_host on sip_subscriptions (sip_host)",
		"create index ss_presence_hosts on sip_subscriptions (presence_hosts)",
		"create index ss_event on sip_subscriptions (event)",
		"create index ss_proto on sip_subscriptions (proto)",
		"create index ss_sub_to_user on sip_subscriptions (sub_to_user)",
		"create index ss_sub_to_host on sip_subscriptions (sub_to_host)",
		"create index ss_expires on sip_subscriptions (expires)",
		"create index ss_orig_proto on sip_subscriptions (orig_proto)",
		"create index ss_network_port on sip_subscriptions (network_port)",
		"create index ss_profile_name on sip_subscriptions (profile_name)",
		"create index ss_version on sip_subscriptions (version)",
		"create index ss_full_from on sip_subscriptions (full_from)",
		"create index ss_contact on sip_subscriptions (contact)",
		"create index sd_uuid on sip_dialogs (uuid)",
		"create index sd_hostname on sip_dialogs (hostname)",
		"create index sd_presence_data on sip_dialogs (presence_data)",
		"create index sd_call_info on sip_dialogs (call_info)",
		"create index sd_call_info_state on sip_dialogs (call_info_state)",
		"create index sd_expires on sip_dialogs (expires)",
		"create index sd_rcd on sip_dialogs (rcd)",
		"create index sd_sip_to_tag on sip_dialogs (sip_to_tag)",
		"create index sd_sip_from_user on sip_dialogs (sip_from_user)",
		"create index sd_sip_from_host on sip_dialogs (sip_from_host)",
		"create index sd_sip_to_host on sip_dialogs (sip_to_host)",
		"create index sd_presence_id on sip_dialogs (presence_id)",
		"create index sd_call_id on sip_dialogs (call_id)",
		"create index sd_sip_from_tag on sip_dialogs (sip_from_tag)",
		"create index sp_hostname on sip_presence (hostname)",
		"create index sp_open_closed on sip_presence (open_closed)",
		"create index sp_sip_user on sip_presence (sip_user)",
		"create index sp_sip_host on sip_presence (sip_host)",
		"create index sp_profile_name on sip_presence (profile_name)",
		"create index sp_expires on sip_presence (expires)",
		"create index sa_nonce on sip_authentication (nonce)",
		"create index sa_hostname on sip_authentication (hostname)",
		"create index sa_expires on sip_authentication (expires)",
		"create index sa_last_nc on sip_authentication (last_nc)",
		"create index ssa_hostname on sip_shared_appearance_subscriptions (hostname)",
		"create index ssa_network_ip on sip_shared_appearance_subscriptions (network_ip)",
		"create index ssa_subscriber on sip_shared_appearance_subscriptions (subscriber)",
		"create index ssa_profile_name on sip_shared_appearance_subscriptions (profile_name)",
		"create index ssa_aor on sip_shared_appearance_subscriptions (aor)",
		"create index ssd_profile_name on sip_shared_appearance_dialogs (profile_name)",
		"create index ssd_hostname on sip_shared_appearance_dialogs (hostname)",
		"create index ssd_contact_str on sip_shared_appearance_dialogs (contact_str)",
		"create index ssd_call_id on sip_shared_appearance_dialogs (call_id)",
		"create index ssd_expires on sip_shared_appearance_dialogs (expires)",
		NULL
	};
		
	switch_cache_db_handle_t *dbh = sofia_glue_get_db_handle(profile);
	char *test2;
	char *err;

	if (!dbh) {
		return 0;
	}
		

	test_sql = switch_mprintf("delete from sip_registrations where (sub_host is null or contact like '%%TCP%%' "
							  "or status like '%%TCP%%' or status like '%%TLS%%') and hostname='%q' "
							  "and network_ip like '%%' and network_port like '%%' and sip_username "
							  "like '%%' and mwi_user  like '%%' and mwi_host like '%%' "
							  "and orig_server_host like '%%' and orig_hostname like '%%'", mod_sofia_globals.hostname);


	switch_cache_db_test_reactive(dbh, test_sql, "drop table sip_registrations", reg_sql);
	
	test2 = switch_mprintf("%s;%s", test_sql, test_sql);
			
	if (switch_cache_db_execute_sql(dbh, test2, &err) != SWITCH_STATUS_SUCCESS) {

		if (switch_stristr("read-only", err)) {
			free(err);
		} else {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "GREAT SCOTT!!! Cannot execute batched statements! [%s]\n"
							  "If you are using mysql, make sure you are using MYODBC 3.51.18 or higher and enable FLAG_MULTI_STATEMENTS\n", err);
			
			switch_cache_db_release_db_handle(&dbh);
			free(test2);
			free(test_sql);
			free(err);
			return 0;
		}
	}

	free(test2);


	free(test_sql);

	test_sql = switch_mprintf("delete from sip_subscriptions where hostname='%q' and full_to='XXX'", mod_sofia_globals.hostname);
							  
	switch_cache_db_test_reactive(dbh, test_sql, "DROP TABLE sip_subscriptions", sub_sql);

	free(test_sql);
	test_sql = switch_mprintf("delete from sip_dialogs where hostname='%q' and (expires <> -9999 or rpid='' or sip_from_tag='' or rcd > 0)",
							  mod_sofia_globals.hostname);


	switch_cache_db_test_reactive(dbh, test_sql, "DROP TABLE sip_dialogs", dialog_sql);
		
	free(test_sql);
	test_sql = switch_mprintf("delete from sip_presence where hostname='%q' or open_closed=''", mod_sofia_globals.hostname);

	switch_cache_db_test_reactive(dbh, test_sql, "DROP TABLE sip_presence", pres_sql);

	free(test_sql);
	test_sql = switch_mprintf("delete from sip_authentication where hostname='%q' or last_nc >= 0", mod_sofia_globals.hostname);

	switch_cache_db_test_reactive(dbh, test_sql, "DROP TABLE sip_authentication", auth_sql);

	free(test_sql);
	test_sql = switch_mprintf("delete from sip_shared_appearance_subscriptions where contact_str='' or hostname='%q' and network_ip like '%%'",
							  mod_sofia_globals.hostname);

	switch_cache_db_test_reactive(dbh, test_sql, "DROP TABLE sip_shared_appearance_subscriptions", shared_appearance_sql);

	free(test_sql);
	test_sql = switch_mprintf("delete from sip_shared_appearance_dialogs where contact_str='' or hostname='%q' and network_ip like '%%'",
							  mod_sofia_globals.hostname);

	switch_cache_db_test_reactive(dbh, test_sql, "DROP TABLE sip_shared_appearance_dialogs", shared_appearance_dialogs_sql);
		
	free(test_sql);

	for (x = 0; indexes[x]; x++) {
		switch_cache_db_execute_sql(dbh, indexes[x], NULL);
	}

	switch_cache_db_release_db_handle(&dbh);

	return 1;

}

void sofia_glue_execute_sql(sofia_profile_t *profile, char **sqlp, switch_bool_t sql_already_dynamic)
{
	char *sql;

	switch_assert(sqlp && *sqlp);
	sql = *sqlp;	

	switch_sql_queue_manager_push(profile->qm, sql, 1, !sql_already_dynamic);

	if (sql_already_dynamic) {
		*sqlp = NULL;
	}
}


void sofia_glue_execute_sql_now(sofia_profile_t *profile, char **sqlp, switch_bool_t sql_already_dynamic)
{
	char *sql;

	switch_assert(sqlp && *sqlp);
	sql = *sqlp;	

	switch_mutex_lock(profile->dbh_mutex);
	switch_sql_queue_manager_push_confirm(profile->qm, sql, 0, !sql_already_dynamic);
	switch_mutex_unlock(profile->dbh_mutex);

	if (sql_already_dynamic) {
		*sqlp = NULL;
	}
}

void sofia_glue_execute_sql_soon(sofia_profile_t *profile, char **sqlp, switch_bool_t sql_already_dynamic)
{
	char *sql;

	switch_assert(sqlp && *sqlp);
	sql = *sqlp;	

	switch_sql_queue_manager_push(profile->qm, sql, 0, !sql_already_dynamic);

	if (sql_already_dynamic) {
		*sqlp = NULL;
	}
}


switch_cache_db_handle_t *_sofia_glue_get_db_handle(sofia_profile_t *profile, const char *file, const char *func, int line)
{
	switch_cache_db_handle_t *dbh = NULL;
	char *dsn;
	
	if (!zstr(profile->odbc_dsn)) {
		dsn = profile->odbc_dsn;
	} else {
		dsn = profile->dbname;
	}

	if (_switch_cache_db_get_db_handle_dsn(&dbh, dsn, file, func, line) != SWITCH_STATUS_SUCCESS) {
		dbh = NULL;
	}
	
	return dbh;
}

void sofia_glue_actually_execute_sql_trans(sofia_profile_t *profile, char *sql, switch_mutex_t *mutex)
{
	switch_cache_db_handle_t *dbh = NULL;

	if (mutex) {
		switch_mutex_lock(mutex);
	}


	if (!(dbh = sofia_glue_get_db_handle(profile))) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Error Opening DB\n");

		goto end;
	}

	switch_cache_db_persistant_execute_trans_full(dbh, sql, 1,
												  profile->pre_trans_execute,
												  profile->post_trans_execute,
												  profile->inner_pre_trans_execute,
												  profile->inner_post_trans_execute
												  );

	switch_cache_db_release_db_handle(&dbh);

 end:

	if (mutex) {
		switch_mutex_unlock(mutex);
	}
}

void sofia_glue_actually_execute_sql(sofia_profile_t *profile, char *sql, switch_mutex_t *mutex)
{
	switch_cache_db_handle_t *dbh = NULL;
	char *err = NULL;

	if (mutex) {
		switch_mutex_lock(mutex);
	}

	if (!(dbh = sofia_glue_get_db_handle(profile))) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Error Opening DB\n");

		if (mutex) {
			switch_mutex_unlock(mutex);
		}

		return;
	}

	switch_cache_db_execute_sql(dbh, sql, &err);

	if (mutex) {
		switch_mutex_unlock(mutex);
	}

	if (err) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "SQL ERR: [%s]\n%s\n", err, sql);
		free(err);
	}

	switch_cache_db_release_db_handle(&dbh);
}

switch_bool_t sofia_glue_execute_sql_callback(sofia_profile_t *profile,
											  switch_mutex_t *mutex, char *sql, switch_core_db_callback_func_t callback, void *pdata)
{
	switch_bool_t ret = SWITCH_FALSE;
	char *errmsg = NULL;
	switch_cache_db_handle_t *dbh = NULL;

	if (mutex) {
		switch_mutex_lock(mutex);
	}

	if (!(dbh = sofia_glue_get_db_handle(profile))) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Error Opening DB\n");

		if (mutex) {
			switch_mutex_unlock(mutex);
		}

		return ret;
	}

	switch_cache_db_execute_sql_callback(dbh, sql, callback, pdata, &errmsg);

	if (mutex) {
		switch_mutex_unlock(mutex);
	}

	if (errmsg) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "SQL ERR: [%s] %s\n", sql, errmsg);
		free(errmsg);
	}

	switch_cache_db_release_db_handle(&dbh);


	sofia_glue_fire_events(profile);

	return ret;
}

char *sofia_glue_execute_sql2str(sofia_profile_t *profile, switch_mutex_t *mutex, char *sql, char *resbuf, size_t len)
{
	char *ret = NULL;
	char *err = NULL;
	switch_cache_db_handle_t *dbh = NULL;

	if (mutex) {
		switch_mutex_lock(mutex);
	}

	if (!(dbh = sofia_glue_get_db_handle(profile))) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Error Opening DB\n");

		if (mutex) {
			switch_mutex_unlock(mutex);
		}
		
		return NULL;
	}

	ret = switch_cache_db_execute_sql2str(dbh, sql, resbuf, len, &err);

	if (mutex) {
		switch_mutex_unlock(mutex);
	}

	if (err) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "SQL ERR: [%s]\n%s\n", err, sql);
		free(err);
	}

	switch_cache_db_release_db_handle(&dbh);


	sofia_glue_fire_events(profile);

	return ret;
}

char *sofia_glue_get_register_host(const char *uri)
{
	char *register_host = NULL;
	const char *s;
	char *p = NULL;

	if (zstr(uri)) {
		return NULL;
	}

	if ((s = switch_stristr("sip:", uri))) {
		s += 4;
	} else if ((s = switch_stristr("sips:", uri))) {
		s += 5;
	}

	if (!s) {
		return NULL;
	}

	register_host = strdup(s);
	
	/* remove port for register_host for testing nat acl take into account 
	   ipv6 addresses which are required to have brackets around the addr 
	*/
	
	if ((p = strchr(register_host, ']'))) {
		if (*(p + 1) == ':') {
			*(p + 1) = '\0';
		}
	} else {
		if ((p = strrchr(register_host, ':'))) {
			*p = '\0';
		}
	}
	
	/* register_proxy should always start with "sip:" or "sips:" */
	assert(register_host);

	return register_host;
}

const char *sofia_glue_strip_proto(const char *uri)
{
	char *p;

	if ((p = strchr(uri, ':'))) {
		return p + 1;
	}

	return uri;
}

sofia_cid_type_t sofia_cid_name2type(const char *name)
{
	if (!strcasecmp(name, "rpid")) {
		return CID_TYPE_RPID;
	}

	if (!strcasecmp(name, "pid")) {
		return CID_TYPE_PID;
	}

	return CID_TYPE_NONE;

}

/* all the values of the structure are initialized to NULL  */
/* in case of failure the function returns NULL */
/* sofia_destination->route can be NULL */
sofia_destination_t *sofia_glue_get_destination(char *data)
{
	sofia_destination_t *dst = NULL;
	char *to = NULL;
	char *contact = NULL;
	char *route = NULL;
	char *route_uri = NULL;
	char *eoc = NULL;
	char *p = NULL;

	if (zstr(data)) {
		return NULL;
	}

	if (!(dst = (sofia_destination_t *) malloc(sizeof(sofia_destination_t)))) {
		return NULL;
	}

	/* return a copy of what is in the buffer between the first < and > */
	if (!(contact = sofia_glue_get_url_from_contact(data, 1))) {
		goto mem_fail;
	}

	if ((eoc = strstr(contact, ";fs_path="))) {
		*eoc = '\0';

		if (!(route = strdup(eoc + 9))) {
			goto mem_fail;
		}

		for (p = route; p && *p; p++) {
			if (*p == '>' || *p == ';') {
				*p = '\0';
				break;
			}
		}

		switch_url_decode(route);

		if (!(route_uri = strdup(route))) {
			goto mem_fail;
		}
		if ((p = strchr(route_uri, ','))) {
			do {
				*p = '\0';
			} while ((--p > route_uri) && *p == ' ');
		}
	}

	if (!(to = strdup(data))) {
		goto mem_fail;
	}

	if ((eoc = strstr(to, ";fs_path="))) {
		*eoc++ = '>';
		*eoc = '\0';
	}

	if ((p = strstr(contact, ";fs_"))) {
		*p = '\0';
	}

	dst->contact = contact;
	dst->to = to;
	dst->route = route;
	dst->route_uri = route_uri;
	return dst;

 mem_fail:
	switch_safe_free(contact);
	switch_safe_free(to);
	switch_safe_free(route);
	switch_safe_free(route_uri);
	switch_safe_free(dst);
	return NULL;
}

void sofia_glue_free_destination(sofia_destination_t *dst)
{
	if (dst) {
		switch_safe_free(dst->contact);
		switch_safe_free(dst->route);
		switch_safe_free(dst->route_uri);
		switch_safe_free(dst->to);
		switch_safe_free(dst);
	}
}

switch_status_t sofia_glue_send_notify(sofia_profile_t *profile, const char *user, const char *host, const char *event, const char *contenttype,
									   const char *body, const char *o_contact, const char *network_ip, const char *call_id)
{
	char *id = NULL;
	nua_handle_t *nh;
	sofia_destination_t *dst = NULL;
	char *contact_str, *contact, *user_via = NULL;
	char *route_uri = NULL, *p;

	contact = sofia_glue_get_url_from_contact((char *) o_contact, 1);

	if ((p = strstr(contact, ";fs_"))) {
		*p = '\0';
	}

	if (!zstr(network_ip) && sofia_glue_check_nat(profile, network_ip)) {
		char *ptr = NULL;
		//const char *transport_str = NULL;


		id = switch_mprintf("sip:%s@%s", user, profile->extsipip);
		switch_assert(id);

		if ((ptr = sofia_glue_find_parameter(o_contact, "transport="))) {
			sofia_transport_t transport = sofia_glue_str2transport(ptr);
			//transport_str = sofia_glue_transport2str(transport);
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
			user_via = sofia_glue_create_external_via(NULL, profile, SOFIA_TRANSPORT_UDP);
			contact_str = profile->public_url;
		}

	} else {
		contact_str = profile->url;
		id = switch_mprintf("sip:%s@%s", user, host);
	}

	dst = sofia_glue_get_destination((char *) o_contact);
	switch_assert(dst);

	if (dst->route_uri) {
		route_uri = sofia_glue_strip_uri(dst->route_uri);
	}

	nh = nua_handle(profile->nua, NULL, NUTAG_URL(contact), SIPTAG_FROM_STR(id), SIPTAG_TO_STR(id), SIPTAG_CONTACT_STR(contact_str), TAG_END());
	nua_handle_bind(nh, &mod_sofia_globals.destroy_private);

	nua_notify(nh,
			   NUTAG_NEWSUB(1),
			   TAG_IF(dst->route_uri, NUTAG_PROXY(route_uri)), TAG_IF(dst->route, SIPTAG_ROUTE_STR(dst->route)),
			   TAG_IF(user_via, SIPTAG_VIA_STR(user_via)),
			   SIPTAG_SUBSCRIPTION_STATE_STR("terminated;reason=noresource"),
			   TAG_IF(event, SIPTAG_EVENT_STR(event)),
			   TAG_IF(call_id, SIPTAG_CALL_ID_STR(call_id)),
			   TAG_IF(contenttype, SIPTAG_CONTENT_TYPE_STR(contenttype)), TAG_IF(body, SIPTAG_PAYLOAD_STR(body)), TAG_END());

	switch_safe_free(contact);
	switch_safe_free(route_uri);
	switch_safe_free(id);
	sofia_glue_free_destination(dst);
	switch_safe_free(user_via);

	return SWITCH_STATUS_SUCCESS;
}


int sofia_glue_tech_simplify(private_object_t *tech_pvt)
{
	const char *uuid, *network_addr_a = NULL, *network_addr_b = NULL, *simplify, *simplify_other_channel;
	switch_channel_t *other_channel = NULL, *inbound_channel = NULL;
	switch_core_session_t *other_session = NULL, *inbound_session = NULL;
	uint8_t did_simplify = 0;
	int r = 0;

	if (!switch_channel_test_flag(tech_pvt->channel, CF_ANSWERED) || switch_channel_test_flag(tech_pvt->channel, CF_SIMPLIFY)) {
		goto end;
	}

	if (switch_channel_test_flag(tech_pvt->channel, CF_BRIDGED) && 
		(uuid = switch_channel_get_partner_uuid(tech_pvt->channel)) && (other_session = switch_core_session_locate(uuid))) {

		other_channel = switch_core_session_get_channel(other_session);

		if (switch_channel_test_flag(other_channel, CF_ANSWERED)) {	/* Check if the other channel is answered */
			simplify = switch_channel_get_variable(tech_pvt->channel, "sip_auto_simplify");
			simplify_other_channel = switch_channel_get_variable(other_channel, "sip_auto_simplify");

			r = 1;

			if (switch_true(simplify) && !switch_channel_test_flag(tech_pvt->channel, CF_BRIDGE_ORIGINATOR)) {
				network_addr_a = switch_channel_get_variable(tech_pvt->channel, "network_addr");
				network_addr_b = switch_channel_get_variable(other_channel, "network_addr");
				inbound_session = other_session;
				inbound_channel = other_channel;
			} else if (switch_true(simplify_other_channel) && !switch_channel_test_flag(other_channel, CF_BRIDGE_ORIGINATOR)) {
				network_addr_a = switch_channel_get_variable(other_channel, "network_addr");
				network_addr_b = switch_channel_get_variable(tech_pvt->channel, "network_addr");
				inbound_session = tech_pvt->session;
				inbound_channel = tech_pvt->channel;
			}

			if (inbound_channel && inbound_session && !zstr(network_addr_a) && !zstr(network_addr_b) && !strcmp(network_addr_a, network_addr_b)) {
				if (strcmp(network_addr_a, switch_str_nil(tech_pvt->profile->sipip))
					&& strcmp(network_addr_a, switch_str_nil(tech_pvt->profile->extsipip))) {

					switch_core_session_message_t *msg;
					
					switch_log_printf(SWITCH_CHANNEL_ID_LOG, __FILE__, __SWITCH_FUNC__, __LINE__, switch_channel_get_uuid(inbound_channel),
									  SWITCH_LOG_NOTICE, "Will simplify channel [%s]\n", switch_channel_get_name(inbound_channel));
					
					msg = switch_core_session_alloc(inbound_session, sizeof(*msg));
					MESSAGE_STAMP_FFL(msg);
					msg->message_id = SWITCH_MESSAGE_INDICATE_SIMPLIFY;
					msg->from = __FILE__;
					switch_core_session_receive_message(inbound_session, msg);

					did_simplify = 1;

					switch_core_recovery_track(inbound_session);

					switch_channel_set_flag(inbound_channel, CF_SIMPLIFY);
					
				}
			}

			if (!did_simplify && inbound_channel) {
				switch_log_printf(SWITCH_CHANNEL_ID_LOG, __FILE__, __SWITCH_FUNC__, __LINE__, switch_channel_get_uuid(inbound_channel), SWITCH_LOG_NOTICE,
								  "Could not simplify channel [%s]\n", switch_channel_get_name(inbound_channel));
			}
		}

		switch_core_session_rwunlock(other_session);
	}


 end:

	return r;
}

void sofia_glue_pause_jitterbuffer(switch_core_session_t *session, switch_bool_t on)
{
	switch_core_session_message_t *msg;
	msg = switch_core_session_alloc(session, sizeof(*msg));
	MESSAGE_STAMP_FFL(msg);
	msg->message_id = SWITCH_MESSAGE_INDICATE_JITTER_BUFFER;
	msg->string_arg = switch_core_session_strdup(session, on ? "pause" : "resume");
	msg->from = __FILE__;

	switch_core_session_queue_message(session, msg);
}


void sofia_glue_build_vid_refresh_message(switch_core_session_t *session, const char *pl)
{
	switch_core_session_message_t *msg;
	msg = switch_core_session_alloc(session, sizeof(*msg));
	MESSAGE_STAMP_FFL(msg);
	msg->message_id = SWITCH_MESSAGE_INDICATE_VIDEO_REFRESH_REQ;
	if (pl) {
		msg->string_arg = switch_core_session_strdup(session, pl);
	}
	msg->from = __FILE__;

	switch_core_session_queue_message(session, msg);
}


char *sofia_glue_gen_contact_str(sofia_profile_t *profile, sip_t const *sip, nua_handle_t *nh, sofia_dispatch_event_t *de, sofia_nat_parse_t *np)
{
	char *contact_str = NULL;
	const char *contact_host;//, *contact_user;
	sip_contact_t const *contact;
	char *port;
	const char *display = "\"user\"";
	char new_port[25] = "";
	sofia_nat_parse_t lnp = { { 0 } };
	const char *ipv6;
	sip_from_t const *from;

	if (!sip || !sip->sip_contact || !sip->sip_contact->m_url) {
		return NULL;
	}

	from = sip->sip_from;
	contact = sip->sip_contact;

	if (!np) {
		np = &lnp;
	}

	sofia_glue_get_addr(de->data->e_msg, np->network_ip, sizeof(np->network_ip), &np->network_port);
	
	if (sofia_glue_check_nat(profile, np->network_ip)) {
		np->is_auto_nat = 1;
	}

	port = (char *) contact->m_url->url_port;
	contact_host = sip->sip_contact->m_url->url_host;
	//contact_user = sip->sip_contact->m_url->url_user;

	display = contact->m_display;


	if (zstr(display)) {
		if (from) {
			display = from->a_display;
			if (zstr(display)) {
				display = "\"user\"";
			}
		}
	} else {
		display = "\"user\"";
	}

	if (sofia_test_pflag(profile, PFLAG_AGGRESSIVE_NAT_DETECTION)) {
		if (sip->sip_via) {
			const char *v_port = sip->sip_via->v_port;
			const char *v_host = sip->sip_via->v_host;

			if (v_host && sip->sip_via->v_received) {
				np->is_nat = "via received";
			} else if (v_host && strcmp(np->network_ip, v_host)) {
				np->is_nat = "via host";
			} else if (v_port && atoi(v_port) != np->network_port) {
				np->is_nat = "via port";
			}
		}
	}

	if (!np->is_nat && sip && sip->sip_via && sip->sip_via->v_port &&
		atoi(sip->sip_via->v_port) == 5060 && np->network_port != 5060 ) {
		np->is_nat = "via port";
	}

	if (!np->is_nat && profile->nat_acl_count) {
		uint32_t x = 0;
		int ok = 1;
		char *last_acl = NULL;

		if (!zstr(contact_host)) {
			for (x = 0; x < profile->nat_acl_count; x++) {
				last_acl = profile->nat_acl[x];
				if (!(ok = switch_check_network_list_ip(contact_host, last_acl))) {
					break;
				}
			}

			if (ok) {
				np->is_nat = last_acl;
			}
		}
	}

	if (np->is_nat && profile->local_network && switch_check_network_list_ip(np->network_ip, profile->local_network)) {
		if (profile->debug) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "IP %s is on local network, not seting NAT mode.\n", np->network_ip);
		}
		np->is_nat = NULL;
	}

	if (sip->sip_record_route && sip->sip_record_route->r_url) {
		char *full_contact = sip_header_as_string(nh->nh_home, (void *) contact);
		char *route = sofia_glue_strip_uri(sip_header_as_string(nh->nh_home, (void *) sip->sip_record_route));
		char *full_contact_dup;
		char *route_encoded;
		int route_encoded_len;
		full_contact_dup = sofia_glue_get_url_from_contact(full_contact, 1);
		route_encoded_len = (int)(strlen(route) * 3) + 1;
		switch_zmalloc(route_encoded, route_encoded_len);
		switch_url_encode(route, route_encoded, route_encoded_len);
		contact_str = switch_mprintf("%s <%s;fs_path=%s>", display, full_contact_dup, route_encoded);
		free(full_contact_dup);
		free(route_encoded);
	}
	else if (np->is_nat && np->fs_path) {
		char *full_contact = sip_header_as_string(nh->nh_home, (void *) contact);
		char *full_contact_dup;
		char *path_encoded;
		int path_encoded_len;
		char *path_val;
		const char *tp;

		full_contact_dup = sofia_glue_get_url_from_contact(full_contact, 1);

		if ((tp = switch_stristr("transport=", full_contact_dup))) {
			tp += 10;
		}
		
		if (zstr(tp)) {
			tp = "udp";
		}

		path_val = switch_mprintf("sip:%s:%d;transport=%s", np->network_ip, np->network_port, tp);
		path_encoded_len = (int)(strlen(path_val) * 3) + 1;

		switch_zmalloc(path_encoded, path_encoded_len);
		switch_copy_string(path_encoded, ";fs_path=", 10);
		switch_url_encode(path_val, path_encoded + 9, path_encoded_len - 9);
		
		contact_str = switch_mprintf("%s <%s;fs_nat=yes%s>", display, full_contact_dup, path_encoded);

		free(full_contact_dup);
		free(path_encoded);
		free(path_val);

	} else {

		if (zstr(contact_host)) {
			np->is_nat = "No contact host";
		}
		
		if (np->is_nat) {
			contact_host = np->network_ip;
			switch_snprintf(new_port, sizeof(new_port), ":%d", np->network_port);
			port = NULL;
		}
		
		
		if (port) {
			switch_snprintf(new_port, sizeof(new_port), ":%s", port);
		}
		
		ipv6 = strchr(contact_host, ':');
		

		if (contact->m_url->url_params) {
			contact_str = switch_mprintf("%s <sip:%s%s%s%s%s%s;%s>%s",
										 display, contact->m_url->url_user,
										 contact->m_url->url_user ? "@" : "",
										 ipv6 ? "[" : "",
										 contact_host, ipv6 ? "]" : "", new_port, contact->m_url->url_params, np->is_nat ? ";fs_nat=yes" : "");
		} else {
			contact_str = switch_mprintf("%s <sip:%s%s%s%s%s%s>%s",
										 display,
										 contact->m_url->url_user,
										 contact->m_url->url_user ? "@" : "",
										 ipv6 ? "[" : "", contact_host, ipv6 ? "]" : "", new_port, np->is_nat ? ";fs_nat=yes" : "");
		}
	}
		
	return contact_str;
}

char *sofia_glue_get_host(const char *str, switch_memory_pool_t *pool)
{
	char *s, *p;

	if ((p = strchr(str, '@'))) {
		p++;
	} else {
		return NULL;
	}

	if (pool) {
		s = switch_core_strdup(pool, p);
	} else {
		s = strdup(p);
	}

	for (p = s; p && *p; p++) {
		if ((*p == ';') || (*p == '>')) {
			*p = '\0';
			break;
		}
	}

	return s;
}

void sofia_glue_fire_events(sofia_profile_t *profile)
{
	void *pop = NULL;

	while (profile->event_queue && switch_queue_trypop(profile->event_queue, &pop) == SWITCH_STATUS_SUCCESS && pop) {
		switch_event_t *event = (switch_event_t *) pop;
		switch_event_fire(&event);
	}

}

void sofia_event_fire(sofia_profile_t *profile, switch_event_t **event)
{
	switch_queue_push(profile->event_queue, *event);
	*event = NULL;
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
