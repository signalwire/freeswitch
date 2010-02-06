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
 * Ken Rice, Asteria Solutions Group, Inc <ken@asteriasgi.com>
 * Paul D. Tinsley <pdt at jackhammer.org>
 * Bret McDanel <trixter AT 0xdecafbad.com>
 * Brian West <brian@freeswitch.org>
 *
 * sofia_sla.c -- SOFIA SIP Endpoint (support for shared line appearance)
 *  This file (and calls into it) developed by Matthew T Kaufman <matthew@matthew.at>
 *
 */
#include "mod_sofia.h"

static int sofia_sla_sub_callback(void *pArg, int argc, char **argv, char **columnNames);

struct sla_helper {
	char call_id[1024];
};

static int get_call_id_callback(void *pArg, int argc, char **argv, char **columnNames)
{
	struct sla_helper *sh = (struct sla_helper *) pArg;

	switch_set_string(sh->call_id, argv[0]);
	return 0;
}

int sofia_sla_supported(sip_t const *sip)
{
	if (sip && sip->sip_user_agent && sip->sip_user_agent->g_string) {
		const char *ua = sip->sip_user_agent->g_string;

		if (switch_stristr("polycom", ua)) {
			return 1;
		}

		if (switch_stristr("snom", ua)) {
			return 1;
		}

	}

	return 0;
}


void sofia_sla_handle_register(nua_t *nua, sofia_profile_t *profile, sip_t const *sip, long exptime, const char *full_contact)
{
	nua_handle_t *nh = NULL;
	char exp_str[256] = "";
	char my_contact[256] = "";
	char *sql;
	struct sla_helper sh = { {0} };
	char *contact_str = sofia_glue_strip_uri(full_contact);
	sofia_transport_t transport = sofia_glue_url2transport(sip->sip_contact->m_url);
	char network_ip[80];
	int network_port = 0;
	sofia_destination_t *dst;
	char *route_uri = NULL;
	char port_str[25] = "";

	sofia_glue_get_addr(nua_current_request(nua), network_ip, sizeof(network_ip), &network_port);

	sql = switch_mprintf("select call_id from sip_shared_appearance_dialogs where hostname='%q' and profile_name='%q' and contact_str='%q'",
						 mod_sofia_globals.hostname, profile->name, contact_str);
	sofia_glue_execute_sql_callback(profile, profile->ireg_mutex, sql, get_call_id_callback, &sh);

	free(sql);

	if (*sh.call_id) {
		if (!(nh = nua_handle_by_call_id(profile->nua, sh.call_id))) {
			if ((sql = switch_mprintf("delete from sip_shared_appearance_dialogs where hostname='%q' and profile_name='%q' and contact_str='%q'",
									  mod_sofia_globals.hostname, profile->name, contact_str))) {
				sofia_glue_execute_sql(profile, &sql, SWITCH_TRUE);
			}
		}
	}

	if (!nh) {
		nh = nua_handle(nua, NULL, NUTAG_URL(sip->sip_contact->m_url), TAG_NULL());
	}

	nua_handle_bind(nh, &mod_sofia_globals.keep_private);

	switch_snprintf(exp_str, sizeof(exp_str), "%ld", exptime + 30);

	switch_snprintf(port_str, sizeof(port_str), ":%ld", sofia_glue_transport_has_tls(transport) ? profile->tls_sip_port : profile->sip_port);

	if (sofia_glue_check_nat(profile, network_ip)) {
		switch_snprintf(my_contact, sizeof(my_contact), "<sip:%s@%s%s;transport=%s>;expires=%s", profile->sla_contact,
						profile->extsipip, port_str, sofia_glue_transport2str(transport), exp_str);
	} else {
		switch_snprintf(my_contact, sizeof(my_contact), "<sip:%s@%s%s;transport=%s>;expires=%s", profile->sla_contact,
						profile->sipip, port_str, sofia_glue_transport2str(transport), exp_str);
	}

	dst = sofia_glue_get_destination((char *) full_contact);

	if (dst->route_uri) {
		route_uri = sofia_glue_strip_uri(dst->route_uri);
	}

	nua_subscribe(nh,
				  TAG_IF(dst->route_uri, NUTAG_PROXY(route_uri)), TAG_IF(dst->route, SIPTAG_ROUTE_STR(dst->route)),
				  SIPTAG_TO(sip->sip_to),
				  SIPTAG_FROM(sip->sip_to),
				  SIPTAG_CONTACT_STR(my_contact),
				  SIPTAG_EXPIRES_STR(exp_str),
				  SIPTAG_EVENT_STR("dialog;sla;include-session-description"), SIPTAG_ACCEPT_STR("application/dialog-info+xml"), TAG_NULL());

	sofia_glue_free_destination(dst);

	free(contact_str);
}

void sofia_sla_handle_sip_i_publish(nua_t *nua, sofia_profile_t *profile, nua_handle_t *nh, sip_t const *sip, tagi_t tags[])
{
	/* at present there's no SLA versions that we deal with that do publish. to be safe, we say "OK" */
	nua_respond(nh, SIP_200_OK, NUTAG_WITH_THIS(nua), TAG_END());
}

void sofia_sla_handle_sip_i_subscribe(nua_t *nua, const char *contact_str, sofia_profile_t *profile, nua_handle_t *nh, sip_t const *sip, tagi_t tags[])
{
	char *aor = NULL;
	char *subscriber = NULL;
	char *sql = NULL;
	char *route_uri = NULL;
	char *sla_contact = NULL;
	char network_ip[80];
	int network_port = 0;
	char port_str[25] = "";

	sofia_transport_t transport = sofia_glue_url2transport(sip->sip_contact->m_url);

	sofia_glue_get_addr(nua_current_request(nua), network_ip, sizeof(network_ip), &network_port);
	/*
	 * XXX MTK FIXME - we don't look at the tag to see if NUTAG_SUBSTATE(nua_substate_terminated) or
	 * a Subscription-State header with state "terminated" and/or expiration of 0. So we never forget
	 * about them here.
	 * likewise, we also don't have a hook against nua_r_notify events, so we can't see nua_substate_terminated there.
	 */

	/*
	 * extracting AOR is weird...
	 * the From is the main extension, not the third-party one...
	 * and the contact has the phone's own network address, not the AOR address
	 * so we do what openser's pua_bla does and...
	 */

	/* We always store the AOR as the sipip and not the request so SLA works with NAT inside out */
	aor = switch_mprintf("sip:%s@%s", sip->sip_contact->m_url->url_user, profile->sipip);

	/*
	 * ok, and now that we HAVE the AOR, we REALLY should go check in the XML config and see if this particular
	 * extension is set up to have shared appearances managed. right now it is all-or-nothing on the profile,
	 * which won't be sufficient for real life. FIXME XXX MTK
	 */

	/* then the subscriber is the user at their network location... this is arguably the wrong way, but works so far... */

	subscriber = switch_mprintf("sip:%s@%s;transport=%s", sip->sip_from->a_url->url_user,
								sip->sip_contact->m_url->url_host, sofia_glue_transport2str(transport));

	if ((sql =
		 switch_mprintf("delete from sip_shared_appearance_subscriptions where subscriber='%q' and profile_name='%q' and hostname='%q'",
						subscriber, profile->name, mod_sofia_globals.hostname))) {
		sofia_glue_execute_sql(profile, &sql, SWITCH_TRUE);
	}

	if ((sql =
		 switch_mprintf("insert into sip_shared_appearance_subscriptions (subscriber, call_id, aor, profile_name, hostname, contact_str, network_ip) "
						"values ('%q','%q','%q','%q','%q','%q','%q')",
						subscriber, sip->sip_call_id->i_id, aor, profile->name, mod_sofia_globals.hostname, contact_str, network_ip))) {
		sofia_glue_execute_sql(profile, &sql, SWITCH_TRUE);
	}

	if (strstr(contact_str, ";fs_nat")) {
		char *p;
		route_uri = sofia_glue_get_url_from_contact((char *) contact_str, 1);
		if ((p = strstr(contact_str, ";fs_"))) {
			*p = '\0';
		}
	}

	if (route_uri) {
		char *p;

		while (route_uri && *route_uri && (*route_uri == '<' || *route_uri == ' ')) {
			route_uri++;
		}
		if ((p = strchr(route_uri, '>'))) {
			*p++ = '\0';
		}
	}

	switch_snprintf(port_str, sizeof(port_str), ":%ld", sofia_glue_transport_has_tls(transport) ? profile->tls_sip_port : profile->sip_port);

	if (sofia_glue_check_nat(profile, network_ip)) {
		sla_contact = switch_mprintf("<sip:%s@%s%s;transport=%s>", profile->sla_contact, profile->extsipip, port_str, sofia_glue_transport2str(transport));
	} else {
		sla_contact = switch_mprintf("<sip:%s@%s%s;transport=%s>", profile->sla_contact, profile->sipip, port_str, sofia_glue_transport2str(transport));
	}

	nua_respond(nh, SIP_202_ACCEPTED, SIPTAG_CONTACT_STR(sla_contact), NUTAG_WITH_THIS(nua), TAG_IF(route_uri, NUTAG_PROXY(route_uri)), SIPTAG_SUBSCRIPTION_STATE_STR("active;expires=300"),	/* you thought the OTHER time was fake... need delta here FIXME XXX MTK */
				SIPTAG_EXPIRES_STR("300"),	/* likewise, totally fake - FIXME XXX MTK */
				/*  sofia_presence says something about needing TAG_IF(sticky, NUTAG_PROXY(sticky)) for NAT stuff? */
				TAG_END());

	switch_safe_free(aor);
	switch_safe_free(subscriber);
	switch_safe_free(route_uri);
	switch_safe_free(sla_contact);
	switch_safe_free(sql);
}

struct sla_notify_helper {
	sofia_profile_t *profile;
	char *payload;
};

void sofia_sla_handle_sip_r_subscribe(int status,
									  char const *phrase,
									  nua_t *nua, sofia_profile_t *profile, nua_handle_t *nh, sofia_private_t *sofia_private, sip_t const *sip,
									  tagi_t tags[])
{
	if (status >= 300) {
		nua_handle_destroy(nh);
		sofia_private_free(sofia_private);
	} else {
		char *full_contact = sip_header_as_string(nua_handle_home(nh), (void *) sip->sip_contact);
		time_t expires = switch_epoch_time_now(NULL);
		char *sql;
		char *contact_str = sofia_glue_strip_uri(full_contact);

		if (sip && sip->sip_expires) {
			expires += sip->sip_expires->ex_delta + 30;
		}

		if ((sql = switch_mprintf("insert into sip_shared_appearance_dialogs (profile_name, hostname, contact_str, call_id, expires) "
								  "values ('%q','%q','%q','%q','%ld')",
								  profile->name, mod_sofia_globals.hostname, contact_str, sip->sip_call_id->i_id, (long) expires))) {
			sofia_glue_execute_sql(profile, &sql, SWITCH_TRUE);
		}

		free(contact_str);
	}
}

void sofia_sla_handle_sip_i_notify(nua_t *nua, sofia_profile_t *profile, nua_handle_t *nh, sip_t const *sip, tagi_t tags[])
{
	char *sql = NULL;
	struct sla_notify_helper helper;
	char *aor = NULL;
	char *contact = NULL;
	sofia_transport_t transport = sofia_glue_url2transport(sip->sip_contact->m_url);

	/*
	 * things we know we don't do:
	 *   draft-anil-sipping-bla says we should look and see if the specific appearance is in use and if it is
	 *     return an error for the i_notify, to handle the initial line-seize for dialing out case.
	 *     to do that we would need to really track all the appearances *and* override sofia's autoresponder for i_notify
	 *     because at this point, it already sent the 200 for us.
	 *   and we simply don't track all the appearance status by decoding the XML payload out and recording that in
	 *     an SQL line appearance database yet. we'll need to do that in order to do the above, and in order to make
	 *     interoperation possible between devices that disagree on the dialog xml payload OR don't even do it that
	 *     way and instead use things like call-info/line-seize events like the old Broadsoft spec.
	 *     instead we cheat and just reflect the entire payload back to the subscribers (who, because we don't
	 *     yet check each AOR as it comes in to see if it is to be managed, is more subscribers than we probably
	 *     should have). for the current prototype stage, this works ok anyway.
	 *   and because we don't parse the XML, we even reflect it right back to the notifier/sender (which is called
	 *     "target" in the payload XML, of course).
	 *   also because we don't track on a per-appearance basis, there IS NOT a hook back from sofia_glue to add
	 *     an appearance index to the outbound invite for the "next free appearance". this can lead to race 
	 *     conditions where a call shows up on slightly different line key numbers at different phones, making
	 *     "pick up on line X" meaningless if such a race occurs. again, it is a prototype. we can fix it later.
	 */


	/* the dispatcher calls us just because it is aimed at us, so check to see if it is dialog;sla at the very least... */

	if ((!sip->sip_event)
		|| (strcasecmp(sip->sip_event->o_type, "dialog"))
		|| !msg_params_find(sip->sip_event->o_params, "sla")) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "sent to sla-agent but not dialog;sla\n");
		return;
	}

	/* calculate the AOR we're trying to tell people about. should probably double-check before derferencing XXX MTK */
	/* We always store the AOR as the sipip and not the request so SLA works with NAT inside out */
	aor = switch_mprintf("sip:%s@%s", sip->sip_to->a_url->url_user, profile->sipip);

	/* this isn't sufficient because on things like the polycom, the subscriber is the 'main' ext number, but the
	 * 'main' ext number isn't in ANY of the headers they send us in the notify. of course.
	 * as a side effect, the subscriber<>'%q' below isn't sufficient to prevent reflecting the event back
	 * at a phone that has the ext # != third-party#. see above, can only fix by parsing the XML for the 'target'
	 * so we don't reflect it back at anyone who is the "boss" config, but we do reflect it back at the "secretary"
	 * config. if that breaks the phone, just set them all up as the "boss" config where ext#==third-party#
	 */
	contact = switch_mprintf("sip:%s@%s;transport=%s", sip->sip_contact->m_url->url_user,
							 sip->sip_contact->m_url->url_host, sofia_glue_transport2str(transport));

	if (sip->sip_payload && sip->sip_payload->pl_data) {
		sql = switch_mprintf("select subscriber,call_id,aor,profile_name,hostname,contact_str,network_ip from sip_shared_appearance_subscriptions where "
							 "aor='%q' and profile_name='%q' and hostname='%q'", aor, profile->name, mod_sofia_globals.hostname);

		helper.profile = profile;
		helper.payload = sip->sip_payload->pl_data;	/* could just send the WHOLE payload. you'd get the type that way. */

		/* which mutex if any is correct to hold in this callback? XXX MTK FIXME */
		sofia_glue_execute_sql_callback(profile, profile->ireg_mutex, sql, sofia_sla_sub_callback, &helper);

		switch_safe_free(sql);
		switch_safe_free(aor);
		switch_safe_free(contact);
	}
}

static int sofia_sla_sub_callback(void *pArg, int argc, char **argv, char **columnNames)
{
	struct sla_notify_helper *helper = pArg;
	/* char *subscriber = argv[0]; */
	char *call_id = argv[1];
	/* char *aor = argv[2]; */
	/* char *profile_name = argv[3]; */
	/* char *hostname = argv[4]; */
	char *contact_str = argv[5];
	char *network_ip = argv[6];
	nua_handle_t *nh;
	char *route_uri = NULL;
	char *xml_fixup = NULL;
	char *fixup = NULL;
	nh = nua_handle_by_call_id(helper->profile->nua, call_id);	/* that's all you need to find the subscription's nh */

	if (nh) {

		if (strstr(contact_str, ";fs_nat")) {
			char *p;
			route_uri = sofia_glue_get_url_from_contact(contact_str, 1);
			if ((p = strstr(contact_str, ";fs_"))) {
				*p = '\0';
			}
		}

		if (route_uri) {
			char *p;

			while (route_uri && *route_uri && (*route_uri == '<' || *route_uri == ' ')) {
				route_uri++;
			}
			if ((p = strchr(route_uri, '>'))) {
				*p++ = '\0';
			}
		}

		if (helper->profile->extsipip) {
			if (sofia_glue_check_nat(helper->profile, network_ip)) {
				fixup = switch_string_replace(helper->payload, helper->profile->sipip, helper->profile->extsipip);
			} else {
				fixup = switch_string_replace(helper->payload, helper->profile->extsipip, helper->profile->sipip);
			}
			xml_fixup = fixup;
		} else {
			xml_fixup = helper->payload;
		}

		nua_notify(nh, SIPTAG_SUBSCRIPTION_STATE_STR("active;expires=300"),	/* XXX MTK FIXME - this is totally fake calculation */
				   TAG_IF(route_uri, NUTAG_PROXY(route_uri)), SIPTAG_CONTENT_TYPE_STR("application/dialog-info+xml"),	/* could've just kept the type from the payload */
				   SIPTAG_PAYLOAD_STR(xml_fixup), TAG_END());
		switch_safe_free(route_uri);
		if (fixup && fixup != helper->payload) {
			free(fixup);
		}
	}
	return 0;
}
