/* 
 * FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
 * Copyright (C) 2005/2006, Anthony Minessale II <anthm@freeswitch.org>
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
 *
 *
 * sofia_sla.c -- SOFIA SIP Endpoint (support for shared line appearance)
 *  This file (and calls into it) developed by Matthew T Kaufman <matthew@matthew.at>
 *
 */
#include "mod_sofia.h"


static int sofia_sla_sub_callback(void *pArg, int argc, char **argv, char **columnNames);


void sofia_sla_handle_register(nua_t *nua, sofia_profile_t *profile, sip_t const *sip)
{
	nua_handle_t *nh;

	/* TODO:
	 *  check to see if it says in the group or extension xml that we are handling SLA for this AOR
	 *  check to see if we're already subscribed and the call-id in the subscribe matches. if so,
	 *    we can skip this, which would keep us from re-subscribing which would also keep us from
	 *    leaking so horribly much memory like we do now
	*/

	nh = nua_handle(nua, NULL, NUTAG_URL(sip->sip_contact->m_url), TAG_NULL());

	/* we make up and bind a sofia_private so that the existing event handler destruction code won't be confused by us */
	/* (though it isn't clear that this is sufficient... we still have break cases for nua_i_notify and nua_r_notify
	 *  in sofia_event_callback's destruction end because if we don't, the handle gets destroyed. or maybe it is
	 *  something else i'm doing wrong? MTK

	 mod_sofia_globals.keep_private is a magic static private things can share for this purpose: ACM
	*/

	nua_handle_bind(nh, &mod_sofia_globals.keep_private);

	
	nua_subscribe(nh,
		SIPTAG_TO(sip->sip_to),
		SIPTAG_FROM(sip->sip_to), // ?
		SIPTAG_CONTACT_STR(profile->sla_contact),
		SIPTAG_EXPIRES_STR("3500"),		/* ok, this is totally fake here XXX MTK */
		SIPTAG_EVENT_STR("dialog;sla"),	/* some phones want ;include-session-description too? */
		TAG_NULL());
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

	aor = switch_mprintf("sip:%s@%s",sip->sip_contact->m_url->url_user, sip->sip_from->a_url->url_host);

	/*
	 * ok, and now that we HAVE the AOR, we REALLY should go check in the XML config and see if this particular
	 * extension is set up to have shared appearances managed. right now it is all-or-nothing on the profile,
	 * which won't be sufficient for real life. FIXME XXX MTK
	*/

	/* then the subscriber is the user at their network location... this is arguably the wrong way, but works so far... */

	subscriber = switch_mprintf("sip:%s@%s",sip->sip_from->a_url->url_user, sip->sip_contact->m_url->url_host);
 

	if ((sql =
		switch_mprintf("delete from sip_shared_appearance_subscriptions where subscriber='%q' and profile name='%q' and hostname='%q'",
			subscriber, profile->name, mod_sofia_globals.hostname
			))) {
		sofia_glue_execute_sql(profile, &sql, SWITCH_TRUE);
	}

	if ((sql =
		 switch_mprintf("insert into sip_shared_appearance_subscriptions (subscriber, call_id, aor, profile_name, hostname, contact_str) "
		               "values ('%q','%q','%q','%q','%q','%q')",
		                subscriber, sip->sip_call_id->i_id, aor, profile->name, mod_sofia_globals.hostname, contact_str))) {
		sofia_glue_execute_sql(profile, &sql, SWITCH_TRUE);
	}


	if (strstr(contact_str, ";fs_nat")) {
		char *p;
		route_uri = sofia_glue_get_url_from_contact((char *)contact_str, 1);
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

	nua_respond(nh, SIP_202_ACCEPTED, SIPTAG_CONTACT_STR(profile->sla_contact), NUTAG_WITH_THIS(nua),
				TAG_IF(route_uri, NUTAG_PROXY(route_uri)),
				SIPTAG_SUBSCRIPTION_STATE_STR("active;expires=300"), /* you thought the OTHER time was fake... need delta here FIXME XXX MTK */
				SIPTAG_EXPIRES_STR("300"), /* likewise, totally fake - FIXME XXX MTK */
	/*  sofia_presence says something about needing TAG_IF(sticky, NUTAG_PROXY(sticky)) for NAT stuff? */
	 TAG_END());

	switch_safe_free(aor);
	switch_safe_free(subscriber);
	switch_safe_free(route_uri);
	switch_safe_free(sql);
}

struct sla_notify_helper {
	sofia_profile_t *profile;
	char *payload;
};

void sofia_sla_handle_sip_r_subscribe(nua_t *nua, sofia_profile_t *profile, nua_handle_t *nh, sip_t const *sip, tagi_t tags[])
{
	/* apparently, we do nothing */
}

void sofia_sla_handle_sip_i_notify(nua_t *nua, sofia_profile_t *profile, nua_handle_t *nh, sip_t const *sip, tagi_t tags[])
{
	char *sql = NULL;
	struct sla_notify_helper helper;
	char *aor = NULL;
	char *contact = NULL;

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

	if (  (!sip->sip_event) 
	   || (strcasecmp(sip->sip_event->o_type, "dialog"))
	   || !msg_params_find(sip->sip_event->o_params, "sla") ) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING,"sent to sla-agent but not dialog;sla\n");
		return;
	}

	/* calculate the AOR we're trying to tell people about. should probably double-check before derferencing XXX MTK */
	aor = switch_mprintf("sip:%s@%s",sip->sip_to->a_url->url_user, sip->sip_to->a_url->url_host);

	/* this isn't sufficient because on things like the polycom, the subscriber is the 'main' ext number, but the
	 * 'main' ext number isn't in ANY of the headers they send us in the notify. of course.
	 * as a side effect, the subscriber<>'%q' below isn't sufficient to prevent reflecting the event back
	 * at a phone that has the ext # != third-party#. see above, can only fix by parsing the XML for the 'target'
	 * so we don't reflect it back at anyone who is the "boss" config, but we do reflect it back at the "secretary"
	 * config. if that breaks the phone, just set them all up as the "boss" config where ext#==third-party#
	 */
	contact = switch_mprintf("sip:%s@%s",sip->sip_contact->m_url->url_user, sip->sip_contact->m_url->url_host);

	if (sip->sip_payload && sip->sip_payload->pl_data) {
		sql = switch_mprintf("select subscriber,call_id,aor,profile_name,hostname,contact_str from sip_shared_appearance_subscriptions where "
		"aor='%q' and subscriber<>'%q' and profile_name='%q' and hostname='%q'",
		aor, contact, profile->name, mod_sofia_globals.hostname); 


		helper.profile = profile;
		helper.payload = sip->sip_payload->pl_data; 	/* could just send the WHOLE payload. you'd get the type that way. */

		/* which mutex if any is correct to hold in this callback? XXX MTK FIXME */
		sofia_glue_execute_sql_callback(profile, SWITCH_FALSE, profile->ireg_mutex, sql, sofia_sla_sub_callback, &helper);

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
	nua_handle_t *nh;
	char *route_uri = NULL;


	nh = nua_handle_by_call_id(helper->profile->nua, call_id);  /* that's all you need to find the subscription's nh */

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

		nua_notify(nh,
				   SIPTAG_SUBSCRIPTION_STATE_STR("active;expires=300"), /* XXX MTK FIXME - this is totally fake calculation */
				   TAG_IF(route_uri, NUTAG_PROXY(route_uri)),
				   SIPTAG_CONTENT_TYPE_STR("application/dialog-info+xml"),	/* could've just kept the type from the payload */
				   SIPTAG_PAYLOAD_STR(helper->payload),
				   TAG_END());
		switch_safe_free(route_uri);
	}
	return 0;
}

