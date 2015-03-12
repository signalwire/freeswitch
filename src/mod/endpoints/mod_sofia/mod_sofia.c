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
 * Ken Rice <krice at cometsig.com>
 * Paul D. Tinsley <pdt at jackhammer.org>
 * Bret McDanel <trixter AT 0xdecafbad.com>
 * Raymond Chandler <intralanman@freeswitch.org>
 * Emmanuel Schmidbauer <e.schmidbauer@gmail.com>
 *
 *
 * mod_sofia.c -- SOFIA SIP Endpoint
 *
 */

/* Best viewed in a 160 x 60 VT100 Terminal or so the line below at least fits across your screen*/
/*************************************************************************************************************************************************************/
#include "mod_sofia.h"
#include "sofia-sip/sip_extra.h"

SWITCH_MODULE_LOAD_FUNCTION(mod_sofia_load);
SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_sofia_shutdown);
SWITCH_MODULE_DEFINITION(mod_sofia, mod_sofia_load, mod_sofia_shutdown, NULL);

struct mod_sofia_globals mod_sofia_globals;
switch_endpoint_interface_t *sofia_endpoint_interface;

#define STRLEN 15

static switch_status_t sofia_on_init(switch_core_session_t *session);

static switch_status_t sofia_on_exchange_media(switch_core_session_t *session);
static switch_status_t sofia_on_soft_execute(switch_core_session_t *session);
static switch_call_cause_t sofia_outgoing_channel(switch_core_session_t *session, switch_event_t *var_event,
												  switch_caller_profile_t *outbound_profile, switch_core_session_t **new_session,
												  switch_memory_pool_t **pool, switch_originate_flag_t flags, switch_call_cause_t *cancel_cause);
static switch_status_t sofia_read_frame(switch_core_session_t *session, switch_frame_t **frame, switch_io_flag_t flags, int stream_id);
static switch_status_t sofia_write_frame(switch_core_session_t *session, switch_frame_t *frame, switch_io_flag_t flags, int stream_id);
static switch_status_t sofia_read_video_frame(switch_core_session_t *session, switch_frame_t **frame, switch_io_flag_t flags, int stream_id);
static switch_status_t sofia_write_video_frame(switch_core_session_t *session, switch_frame_t *frame, switch_io_flag_t flags, int stream_id);
static switch_status_t sofia_kill_channel(switch_core_session_t *session, int sig);

/* BODY OF THE MODULE */
/*************************************************************************************************************************************************************/

/*
   State methods they get called when the state changes to the specific state
   returning SWITCH_STATUS_SUCCESS tells the core to execute the standard state method next
   so if you fully implement the state you can return SWITCH_STATUS_FALSE to skip it.
*/
static switch_status_t sofia_on_init(switch_core_session_t *session)
{
	switch_channel_t *channel = switch_core_session_get_channel(session);
	private_object_t *tech_pvt = (private_object_t *) switch_core_session_get_private(session);
	switch_status_t status = SWITCH_STATUS_SUCCESS;

	switch_assert(tech_pvt != NULL);


	switch_mutex_lock(tech_pvt->sofia_mutex);


	switch_core_media_check_dtmf_type(session);

	switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "%s SOFIA INIT\n", switch_channel_get_name(channel));
	if (switch_channel_test_flag(channel, CF_PROXY_MODE) || switch_channel_test_flag(channel, CF_PROXY_MEDIA)) {
		switch_core_media_absorb_sdp(session);
	}

	if (switch_channel_test_flag(tech_pvt->channel, CF_RECOVERING) || switch_channel_test_flag(tech_pvt->channel, CF_RECOVERING_BRIDGE)) {
		sofia_set_flag(tech_pvt, TFLAG_RECOVERED);
	}

	if (sofia_test_flag(tech_pvt, TFLAG_OUTBOUND) || switch_channel_test_flag(tech_pvt->channel, CF_RECOVERING)) {
		if (sofia_glue_do_invite(session) != SWITCH_STATUS_SUCCESS) {
			switch_channel_hangup(channel, SWITCH_CAUSE_DESTINATION_OUT_OF_ORDER);
			assert(switch_channel_get_state(channel) != CS_INIT);
			status = SWITCH_STATUS_FALSE;
			goto end;
		}
	}

  end:

	switch_mutex_unlock(tech_pvt->sofia_mutex);

	return status;
}

static switch_status_t sofia_on_routing(switch_core_session_t *session)
{
	private_object_t *tech_pvt = (private_object_t *) switch_core_session_get_private(session);
	switch_channel_t *channel = switch_core_session_get_channel(session);
	switch_assert(tech_pvt != NULL);

	if (!sofia_test_flag(tech_pvt, TFLAG_HOLD_LOCK)) {
		sofia_clear_flag_locked(tech_pvt, TFLAG_SIP_HOLD);
		switch_channel_clear_flag(channel, CF_LEG_HOLDING);
	}

	switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "%s SOFIA ROUTING\n",
					  switch_channel_get_name(switch_core_session_get_channel(session)));

	return SWITCH_STATUS_SUCCESS;
}


static switch_status_t sofia_on_reset(switch_core_session_t *session)
{
	private_object_t *tech_pvt = (private_object_t *) switch_core_session_get_private(session);
	switch_channel_t *channel = switch_core_session_get_channel(session);
	switch_assert(tech_pvt != NULL);

	if (!sofia_test_flag(tech_pvt, TFLAG_HOLD_LOCK)) {
		sofia_clear_flag_locked(tech_pvt, TFLAG_SIP_HOLD);
		switch_channel_clear_flag(channel, CF_LEG_HOLDING);
	}

	switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "%s SOFIA RESET\n",
					  switch_channel_get_name(switch_core_session_get_channel(session)));


	return SWITCH_STATUS_SUCCESS;
}


static switch_status_t sofia_on_hibernate(switch_core_session_t *session)
{
	private_object_t *tech_pvt = switch_core_session_get_private(session);
	switch_channel_t *channel = switch_core_session_get_channel(session);
	switch_assert(tech_pvt != NULL);

	if (!sofia_test_flag(tech_pvt, TFLAG_HOLD_LOCK)) {
		sofia_clear_flag_locked(tech_pvt, TFLAG_SIP_HOLD);
		switch_channel_clear_flag(channel, CF_LEG_HOLDING);
	}

	switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "%s SOFIA HIBERNATE\n",
					  switch_channel_get_name(switch_core_session_get_channel(session)));


	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t sofia_on_execute(switch_core_session_t *session)
{
	private_object_t *tech_pvt = (private_object_t *) switch_core_session_get_private(session);
	switch_channel_t *channel = switch_core_session_get_channel(session);
	switch_assert(tech_pvt != NULL);

	if (!sofia_test_flag(tech_pvt, TFLAG_HOLD_LOCK)) {
		sofia_clear_flag_locked(tech_pvt, TFLAG_SIP_HOLD);
		switch_channel_clear_flag(channel, CF_LEG_HOLDING);
	}

	switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "%s SOFIA EXECUTE\n",
					  switch_channel_get_name(switch_core_session_get_channel(session)));

	return SWITCH_STATUS_SUCCESS;
}

char *generate_pai_str(private_object_t *tech_pvt)
{
	switch_core_session_t *session = tech_pvt->session;
	const char *callee_name = NULL, *callee_number = NULL;
	const char *var, *header, *ua = switch_channel_get_variable(tech_pvt->channel, "sip_user_agent");
	char *pai = NULL;
	const char *host = switch_channel_get_variable(tech_pvt->channel, "sip_to_host");

	if (zstr(host)) {
		host = tech_pvt->profile->sipip;
	}

	if (!sofia_test_pflag(tech_pvt->profile, PFLAG_PASS_CALLEE_ID) || !sofia_test_pflag(tech_pvt->profile, PFLAG_CID_IN_1XX) ||
		((var = switch_channel_get_variable(tech_pvt->channel, "sip_cid_in_1xx")) && switch_false(var))) {
		return NULL;
	}

	if (zstr((callee_name = switch_channel_get_variable(tech_pvt->channel, "initial_callee_id_name"))) &&
		zstr((callee_name = switch_channel_get_variable(tech_pvt->channel, "effective_callee_id_name"))) &&
		zstr((callee_name = switch_channel_get_variable(tech_pvt->channel, "sip_callee_id_name")))) {
		callee_name = switch_channel_get_variable(tech_pvt->channel, "callee_id_name");
	}

	if (zstr((callee_number = switch_channel_get_variable(tech_pvt->channel, "initial_callee_id_number"))) &&
		zstr((callee_number = switch_channel_get_variable(tech_pvt->channel, "effective_callee_id_number"))) &&
		zstr((callee_number = switch_channel_get_variable(tech_pvt->channel, "sip_callee_id_number"))) &&
		zstr((callee_number = switch_channel_get_variable(tech_pvt->channel, "callee_id_number")))) {

		callee_number = tech_pvt->caller_profile->destination_number;
	}

	if (zstr(callee_name) && !zstr(callee_number)) {
		callee_name = callee_number;
	}

	callee_number = switch_sanitize_number(switch_core_session_strdup(session, callee_number));
	callee_name = switch_sanitize_number(switch_core_session_strdup(session, callee_name));

	if (!zstr(callee_number) && (zstr(ua) || !switch_stristr("polycom", ua))) {
		callee_number = switch_core_session_sprintf(session, "sip:%s@%s", callee_number, host);
	}

	header = (tech_pvt->cid_type == CID_TYPE_RPID && !switch_stristr("aastra", ua)) ? "Remote-Party-ID" : "P-Asserted-Identity";

	if (!zstr(callee_name) && !zstr(callee_number)) {
		check_decode(callee_name, tech_pvt->session);

		if (switch_stristr("update_display", tech_pvt->x_freeswitch_support_remote)) {
			pai = switch_core_session_sprintf(tech_pvt->session, "%s: \"%s\" <%s>%s\n"
											  "X-FS-Display-Name: %s\nX-FS-Display-Number: %s\n",
											  header, callee_name, callee_number,
											  tech_pvt->cid_type == CID_TYPE_RPID && !switch_stristr("aastra", ua) ?
											  ";party=calling;privacy=off;screen=no" : "",
											  callee_name, callee_number);
		} else {
			pai = switch_core_session_sprintf(tech_pvt->session, "%s: \"%s\" <%s>%s\n", header, callee_name, callee_number,
											  tech_pvt->cid_type == CID_TYPE_RPID && !switch_stristr("aastra", ua) ?
											  ";party=calling;privacy=off;screen=no" : "");
		}

	}

	return pai;
}

static stfu_instance_t *sofia_get_jb(switch_core_session_t *session, switch_media_type_t type)
{
	private_object_t *tech_pvt = (private_object_t *) switch_core_session_get_private(session);

	return switch_core_media_get_jb(tech_pvt->session, type);
}

/* map QSIG cause codes to SIP from RFC4497 section 8.4.1 */
static int hangup_cause_to_sip(switch_call_cause_t cause)
{
	switch (cause) {
	case SWITCH_CAUSE_UNALLOCATED_NUMBER:
	case SWITCH_CAUSE_NO_ROUTE_TRANSIT_NET:
	case SWITCH_CAUSE_NO_ROUTE_DESTINATION:
		return 404;
	case SWITCH_CAUSE_USER_BUSY:
		return 486;
	case SWITCH_CAUSE_NO_USER_RESPONSE:
		return 408;
	case SWITCH_CAUSE_NO_ANSWER:
	case SWITCH_CAUSE_SUBSCRIBER_ABSENT:
		return 480;
	case SWITCH_CAUSE_CALL_REJECTED:
		return 603;
	case SWITCH_CAUSE_NUMBER_CHANGED:
	case SWITCH_CAUSE_REDIRECTION_TO_NEW_DESTINATION:
		return 410;
	case SWITCH_CAUSE_DESTINATION_OUT_OF_ORDER:
	case SWITCH_CAUSE_INVALID_PROFILE:
		return 502;
	case SWITCH_CAUSE_INVALID_NUMBER_FORMAT:
	case SWITCH_CAUSE_INVALID_URL:
	case SWITCH_CAUSE_INVALID_GATEWAY:
		return 484;
	case SWITCH_CAUSE_FACILITY_REJECTED:
		return 501;
	case SWITCH_CAUSE_NORMAL_UNSPECIFIED:
		return 480;
	case SWITCH_CAUSE_REQUESTED_CHAN_UNAVAIL:
	case SWITCH_CAUSE_NORMAL_CIRCUIT_CONGESTION:
	case SWITCH_CAUSE_NETWORK_OUT_OF_ORDER:
	case SWITCH_CAUSE_NORMAL_TEMPORARY_FAILURE:
	case SWITCH_CAUSE_SWITCH_CONGESTION:
	case SWITCH_CAUSE_GATEWAY_DOWN:
		return 503;
	case SWITCH_CAUSE_OUTGOING_CALL_BARRED:
	case SWITCH_CAUSE_INCOMING_CALL_BARRED:
	case SWITCH_CAUSE_BEARERCAPABILITY_NOTAUTH:
		return 403;
	case SWITCH_CAUSE_BEARERCAPABILITY_NOTAVAIL:
		return 503;
	case SWITCH_CAUSE_BEARERCAPABILITY_NOTIMPL:
	case SWITCH_CAUSE_INCOMPATIBLE_DESTINATION:
		return 488;
	case SWITCH_CAUSE_FACILITY_NOT_IMPLEMENTED:
	case SWITCH_CAUSE_SERVICE_NOT_IMPLEMENTED:
		return 501;
	case SWITCH_CAUSE_RECOVERY_ON_TIMER_EXPIRE:
		return 504;
	case SWITCH_CAUSE_ORIGINATOR_CANCEL:
		return 487;
	case SWITCH_CAUSE_EXCHANGE_ROUTING_ERROR:
		return 483;
	default:
		return 480;
	}
}

switch_status_t sofia_on_destroy(switch_core_session_t *session)
{
	private_object_t *tech_pvt = (private_object_t *) switch_core_session_get_private(session);
	switch_channel_t *channel = switch_core_session_get_channel(session);
	char *uuid;

	switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "%s SOFIA DESTROY\n", switch_channel_get_name(channel));

	if (tech_pvt) {

		if (tech_pvt->respond_phrase) {
			switch_yield(100000);
		}

		if (!zstr(tech_pvt->call_id)) {
			switch_mutex_lock(tech_pvt->profile->flag_mutex);
			if ((uuid = switch_core_hash_find(tech_pvt->profile->chat_hash, tech_pvt->call_id))) {
				free(uuid);
				uuid = NULL;
				switch_core_hash_delete(tech_pvt->profile->chat_hash, tech_pvt->call_id);
			}
			switch_mutex_unlock(tech_pvt->profile->flag_mutex);
		}


		switch_mutex_lock(tech_pvt->profile->flag_mutex);
		tech_pvt->profile->inuse--;
		switch_mutex_unlock(tech_pvt->profile->flag_mutex);

		switch_media_handle_destroy(session);


		if (sofia_test_pflag(tech_pvt->profile, PFLAG_DESTROY) && !tech_pvt->profile->inuse) {
			sofia_profile_destroy(tech_pvt->profile);
		}
	}

	return SWITCH_STATUS_SUCCESS;

}

switch_status_t sofia_on_hangup(switch_core_session_t *session)
{
	switch_core_session_t *a_session;
	private_object_t *tech_pvt = (private_object_t *) switch_core_session_get_private(session);
	switch_channel_t *channel = switch_core_session_get_channel(session);
	switch_call_cause_t cause = switch_channel_get_cause(channel);
	int sip_cause = hangup_cause_to_sip(cause);
	const char *ps_cause = NULL, *use_my_cause;
	const char *gateway_name = NULL;
	sofia_gateway_t *gateway_ptr = NULL;

	if ((gateway_name = switch_channel_get_variable(channel, "sip_gateway_name"))) {
		gateway_ptr = sofia_reg_find_gateway(gateway_name);
	}

	if (!tech_pvt) {
		return SWITCH_STATUS_SUCCESS;
	}

	switch_mutex_lock(tech_pvt->sofia_mutex);


	if (!switch_channel_test_flag(channel, CF_ANSWERED)) {
		if (switch_channel_direction(channel) == SWITCH_CALL_DIRECTION_OUTBOUND) {
			tech_pvt->profile->ob_failed_calls++;
		} else {
			tech_pvt->profile->ib_failed_calls++;
		}

		if (gateway_ptr) {
			if (switch_channel_direction(channel) == SWITCH_CALL_DIRECTION_OUTBOUND) {
				gateway_ptr->ob_failed_calls++;
			} else {
				gateway_ptr->ib_failed_calls++;
			}
		}
	}

	if (gateway_ptr) {
		sofia_reg_release_gateway(gateway_ptr);
	}

	if (!((use_my_cause = switch_channel_get_variable(channel, "sip_ignore_remote_cause")) && switch_true(use_my_cause))) {
		ps_cause = switch_channel_get_variable(channel, "last_bridge_" SWITCH_PROTO_SPECIFIC_HANGUP_CAUSE_VARIABLE);
	}

	if (!zstr(ps_cause) && (!strncasecmp(ps_cause, "sip:", 4) || !strncasecmp(ps_cause, "sips:", 5))) {
		int new_cause = atoi(sofia_glue_strip_proto(ps_cause));
		if (new_cause) {
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "%s Overriding SIP cause %d with %d from the other leg\n",
							  switch_channel_get_name(channel), sip_cause, new_cause);
			sip_cause = new_cause;
		}
	}

	switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "Channel %s hanging up, cause: %s\n",
					  switch_channel_get_name(channel), switch_channel_cause2str(cause));

	if (tech_pvt->hash_key && !sofia_test_pflag(tech_pvt->profile, PFLAG_DESTROY)) {
		switch_core_hash_delete(tech_pvt->profile->chat_hash, tech_pvt->hash_key);
	}

	if (session && tech_pvt->profile->pres_type) {
		char *sql = switch_mprintf("delete from sip_dialogs where uuid='%q'", switch_core_session_get_uuid(session));
		switch_assert(sql);
		sofia_glue_execute_sql_now(tech_pvt->profile, &sql, SWITCH_TRUE);
	}

	if (tech_pvt->kick && (a_session = switch_core_session_locate(tech_pvt->kick))) {
		switch_channel_t *a_channel = switch_core_session_get_channel(a_session);
		switch_channel_hangup(a_channel, switch_channel_get_cause(channel));
		switch_core_session_rwunlock(a_session);
	}

	if (sofia_test_pflag(tech_pvt->profile, PFLAG_DESTROY)) {
		sofia_set_flag(tech_pvt, TFLAG_BYE);
	} else if (tech_pvt->nh && !sofia_test_flag(tech_pvt, TFLAG_BYE)) {
		char reason[128] = "";
		char *bye_headers = sofia_glue_get_extra_headers(channel, SOFIA_SIP_BYE_HEADER_PREFIX);
		const char *val = NULL;
		const char *max_forwards = switch_channel_get_variable(channel, SWITCH_MAX_FORWARDS_VARIABLE);
		const char *call_info = switch_channel_get_variable(channel, "presence_call_info_full");

		val = switch_channel_get_variable(tech_pvt->channel, "disable_q850_reason");

		if (!val || switch_false(val)) {
			if ((val = switch_channel_get_variable(tech_pvt->channel, "sip_reason"))) {
				switch_snprintf(reason, sizeof(reason), "%s", val);
			} else {
				if (switch_channel_test_flag(channel, CF_INTERCEPT) || cause == SWITCH_CAUSE_PICKED_OFF || cause == SWITCH_CAUSE_LOSE_RACE) {
					switch_snprintf(reason, sizeof(reason), "SIP;cause=200;text=\"Call completed elsewhere\"");
				} else if (cause > 0 && cause < 128) {
					switch_snprintf(reason, sizeof(reason), "Q.850;cause=%d;text=\"%s\"", cause, switch_channel_cause2str(cause));
				} else {
					switch_snprintf(reason, sizeof(reason), "SIP;cause=%d;text=\"%s\"", cause, switch_channel_cause2str(cause));
				}
			}
		}

		if (switch_channel_test_flag(channel, CF_INTERCEPT) || cause == SWITCH_CAUSE_PICKED_OFF || cause == SWITCH_CAUSE_LOSE_RACE) {
			switch_channel_set_variable(channel, "call_completed_elsewhere", "true");
		}

		if (switch_channel_test_flag(channel, CF_ANSWERED) || sofia_test_flag(tech_pvt, TFLAG_ANS)) {
			if (!tech_pvt->got_bye) {
				switch_channel_set_variable(channel, "sip_hangup_disposition", "send_bye");
			}
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "Sending BYE to %s\n", switch_channel_get_name(channel));
			if (!sofia_test_flag(tech_pvt, TFLAG_BYE)) {
				nua_bye(tech_pvt->nh,
						SIPTAG_CONTACT(SIP_NONE),
						TAG_IF(!zstr(reason), SIPTAG_REASON_STR(reason)),
						TAG_IF(call_info, SIPTAG_CALL_INFO_STR(call_info)),
						TAG_IF(!zstr(tech_pvt->user_via), SIPTAG_VIA_STR(tech_pvt->user_via)),
						TAG_IF(!zstr(bye_headers), SIPTAG_HEADER_STR(bye_headers)), TAG_END());
			}
		} else {
			if (switch_channel_direction(channel) == SWITCH_CALL_DIRECTION_OUTBOUND) {
				switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "Sending CANCEL to %s\n", switch_channel_get_name(channel));
				if (!tech_pvt->got_bye) {
					switch_channel_set_variable(channel, "sip_hangup_disposition", "send_cancel");
				}
				if (!sofia_test_flag(tech_pvt, TFLAG_BYE)) {
					nua_cancel(tech_pvt->nh,
							   SIPTAG_CONTACT(SIP_NONE),
							   TAG_IF(call_info, SIPTAG_CALL_INFO_STR(call_info)),
							   TAG_IF(!zstr(reason), SIPTAG_REASON_STR(reason)), TAG_IF(!zstr(bye_headers), SIPTAG_HEADER_STR(bye_headers)), TAG_END());
				}
			} else {
				char *resp_headers = sofia_glue_get_extra_headers(channel, SOFIA_SIP_RESPONSE_HEADER_PREFIX);
				const char *phrase;
				char *added_headers = NULL;


				if (tech_pvt->respond_phrase) {
					//phrase = su_strdup(nua_handle_home(tech_pvt->nh), tech_pvt->respond_phrase);
					phrase = tech_pvt->respond_phrase;
				} else {
					phrase = sip_status_phrase(sip_cause);
				}

				if (tech_pvt->respond_code) {
					sip_cause = tech_pvt->respond_code;
					switch (sip_cause) {
					case 401:
					case 407:
						{
							const char *to_host = switch_channel_get_variable(channel, "sip_challenge_realm");

							if (zstr(to_host)) {
								to_host = switch_channel_get_variable(channel, "sip_to_host");
							}

							switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "Challenging call\n");
							sofia_reg_auth_challenge(tech_pvt->profile, tech_pvt->nh, NULL, REG_INVITE, to_host, 0, 0);
							*reason = '\0';
						}
						break;

					case 484:
						{
							const char *to = switch_channel_get_variable(channel, "sip_to_uri");
							char *to_uri = NULL;

							if (to) {
								char *p;
								to_uri = switch_core_session_sprintf(session, "sip:%s", to);
								if ((p = strstr(to_uri, ":5060"))) {
									*p = '\0';
								}

								tech_pvt->respond_dest = to_uri;

							}

							switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "Overlap Dial with %d %s\n", sip_cause, phrase);

						}
						break;

					default:
						break;

					}
				}

				if (tech_pvt->respond_dest && !sofia_test_pflag(tech_pvt->profile, PFLAG_MANUAL_REDIRECT)) {
					added_headers = sofia_glue_get_extra_headers(channel, SOFIA_SIP_HEADER_PREFIX);
				}


				switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "Responding to INVITE with: %d\n", sip_cause);
				if (!tech_pvt->got_bye) {
					switch_channel_set_variable(channel, "sip_hangup_disposition", "send_refuse");
				}
				if (!sofia_test_flag(tech_pvt, TFLAG_BYE)) {
					char *cid = generate_pai_str(tech_pvt);

					if (sip_cause > 299) {
						switch_channel_clear_app_flag_key("T38", tech_pvt->channel, CF_APP_T38);
						switch_channel_clear_app_flag_key("T38", tech_pvt->channel, CF_APP_T38_REQ);
						switch_channel_set_app_flag_key("T38", tech_pvt->channel, CF_APP_T38_FAIL);
					}

					nua_respond(tech_pvt->nh, sip_cause, phrase,
								TAG_IF(!zstr(reason), SIPTAG_REASON_STR(reason)),
								TAG_IF(cid, SIPTAG_HEADER_STR(cid)),
								TAG_IF(!zstr(bye_headers), SIPTAG_HEADER_STR(bye_headers)),
								TAG_IF(!zstr(resp_headers), SIPTAG_HEADER_STR(resp_headers)),
								TAG_IF(!zstr(added_headers), SIPTAG_HEADER_STR(added_headers)),
								TAG_IF(tech_pvt->respond_dest, SIPTAG_CONTACT_STR(tech_pvt->respond_dest)),
								TAG_IF(!zstr(max_forwards), SIPTAG_MAX_FORWARDS_STR(max_forwards)),
								TAG_END());

					switch_safe_free(resp_headers);
				}
				switch_safe_free(added_headers);
			}
		}
		sofia_set_flag_locked(tech_pvt, TFLAG_BYE);
		switch_safe_free(bye_headers);
	}

	sofia_clear_flag(tech_pvt, TFLAG_IO);

	if (tech_pvt->sofia_private) {
		*tech_pvt->sofia_private->uuid = '\0';
	}

	switch_mutex_unlock(tech_pvt->sofia_mutex);

	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t sofia_on_exchange_media(switch_core_session_t *session)
{
	switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "SOFIA EXCHANGE_MEDIA\n");
	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t sofia_on_soft_execute(switch_core_session_t *session)
{
	switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "SOFIA SOFT_EXECUTE\n");
	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t sofia_answer_channel(switch_core_session_t *session)
{
	private_object_t *tech_pvt = (private_object_t *) switch_core_session_get_private(session);
	switch_channel_t *channel = switch_core_session_get_channel(session);
	switch_status_t status;
	uint32_t session_timeout = tech_pvt->profile->session_timeout;
	const char *val;
	const char *b_sdp = NULL;
	int is_proxy = 0;
	int is_3pcc = 0;
	char *sticky = NULL;
	const char *call_info = switch_channel_get_variable(channel, "presence_call_info_full");

	if (switch_channel_test_flag(channel, CF_CONFERENCE)) {
		tech_pvt->reply_contact = switch_core_session_sprintf(session, "%s;isfocus", tech_pvt->reply_contact);
	}

	//switch_core_media_set_local_sdp
	if(sofia_test_flag(tech_pvt, TFLAG_3PCC_INVITE)) {
		// SNARK: complete hack to get final ack sent when a 3pcc invite has been passed from the other leg in bypass_media mode.
			// This code handles the pass_indication sent after the 3pcc ack is received by the other leg in the is_3pcc && is_proxy case below.
	 	// Is there a better place to hang this...?
		b_sdp = switch_channel_get_variable(channel, SWITCH_B_SDP_VARIABLE);
		switch_core_media_set_local_sdp(session, b_sdp, SWITCH_TRUE);

		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "3PCC-PROXY nomedia - sending ack\n");
		nua_ack(tech_pvt->nh,
				TAG_IF(!zstr(tech_pvt->user_via), SIPTAG_VIA_STR(tech_pvt->user_via)),
				SIPTAG_CONTACT_STR(tech_pvt->reply_contact),
				SOATAG_USER_SDP_STR(tech_pvt->mparams.local_sdp_str),
				SOATAG_REUSE_REJECTED(1),
				SOATAG_RTP_SELECT(1), 
				SOATAG_AUDIO_AUX("cn telephone-event"),
				TAG_IF(sofia_test_pflag(tech_pvt->profile, PFLAG_DISABLE_100REL), NUTAG_INCLUDE_EXTRA_SDP(1)),
				TAG_END());
		sofia_clear_flag(tech_pvt, TFLAG_3PCC_INVITE); // all done
		sofia_set_flag_locked(tech_pvt, TFLAG_ANS);
		sofia_set_flag_locked(tech_pvt, TFLAG_SDP);
		switch_channel_mark_answered(channel);     // ... and remember to actually answer the call!
		return SWITCH_STATUS_SUCCESS;
	}

	if (sofia_test_flag(tech_pvt, TFLAG_ANS) || switch_channel_direction(channel) == SWITCH_CALL_DIRECTION_OUTBOUND) {
		return SWITCH_STATUS_SUCCESS;
	}


	b_sdp = switch_channel_get_variable(channel, SWITCH_B_SDP_VARIABLE);
	is_proxy = (switch_channel_test_flag(channel, CF_PROXY_MODE) || switch_channel_test_flag(channel, CF_PROXY_MEDIA));
	is_3pcc = (sofia_test_pflag(tech_pvt->profile, PFLAG_3PCC_PROXY) && sofia_test_flag(tech_pvt, TFLAG_3PCC));

	if (b_sdp && is_proxy && !is_3pcc) {
		switch_core_media_set_local_sdp(session, b_sdp, SWITCH_TRUE);

		if (switch_channel_test_flag(channel, CF_PROXY_MEDIA)) {
			switch_core_media_patch_sdp(tech_pvt->session);
			if (sofia_media_activate_rtp(tech_pvt) != SWITCH_STATUS_SUCCESS) {
				return SWITCH_STATUS_FALSE;
			}
		}
	} else {
		/* This if statement check and handles the 3pcc proxy mode */
		if (is_3pcc) {

			switch_channel_set_flag(channel, CF_3PCC);

			if(!is_proxy) {
			switch_core_media_prepare_codecs(tech_pvt->session, SWITCH_TRUE);
			tech_pvt->mparams.local_sdp_str = NULL;

			switch_core_media_choose_port(tech_pvt->session, SWITCH_MEDIA_TYPE_AUDIO, 0);
			switch_core_session_set_ice(session);
			switch_core_media_gen_local_sdp(session, SDP_TYPE_RESPONSE, NULL, 0, NULL, 0);
			} else {
				switch_core_media_set_local_sdp(session, b_sdp, SWITCH_TRUE);

				if (switch_channel_test_flag(channel, CF_PROXY_MEDIA)) {
					switch_core_media_patch_sdp(tech_pvt->session);
					if (sofia_media_activate_rtp(tech_pvt) != SWITCH_STATUS_SUCCESS) {
						return SWITCH_STATUS_FALSE;
					}
				}
			}

			/* Send the 200 OK */
			if (!sofia_test_flag(tech_pvt, TFLAG_BYE)) {
				char *extra_headers = sofia_glue_get_extra_headers(channel, SOFIA_SIP_RESPONSE_HEADER_PREFIX);
				if (sofia_use_soa(tech_pvt)) {

					nua_respond(tech_pvt->nh, SIP_200_OK,
								TAG_IF(is_proxy, NUTAG_AUTOANSWER(0)),
								SIPTAG_CONTACT_STR(tech_pvt->profile->url),
								SOATAG_USER_SDP_STR(tech_pvt->mparams.local_sdp_str),
								TAG_IF(call_info, SIPTAG_CALL_INFO_STR(call_info)),
								SOATAG_REUSE_REJECTED(1), 
								SOATAG_RTP_SELECT(1),
								SOATAG_AUDIO_AUX("cn telephone-event"), NUTAG_INCLUDE_EXTRA_SDP(1),
								TAG_IF(!zstr(extra_headers), SIPTAG_HEADER_STR(extra_headers)),
								TAG_IF(switch_stristr("update_display", tech_pvt->x_freeswitch_support_remote),
									   SIPTAG_HEADER_STR("X-FS-Support: " FREESWITCH_SUPPORT)), TAG_END());
				} else {
					nua_respond(tech_pvt->nh, SIP_200_OK,
								NUTAG_MEDIA_ENABLE(0),
								SIPTAG_CONTACT_STR(tech_pvt->profile->url),
								TAG_IF(!zstr(extra_headers), SIPTAG_HEADER_STR(extra_headers)),
								TAG_IF(call_info, SIPTAG_CALL_INFO_STR(call_info)),
								SIPTAG_CONTENT_TYPE_STR("application/sdp"),
								SIPTAG_PAYLOAD_STR(tech_pvt->mparams.local_sdp_str),
								TAG_IF(switch_stristr("update_display", tech_pvt->x_freeswitch_support_remote),
									   SIPTAG_HEADER_STR("X-FS-Support: " FREESWITCH_SUPPORT)), TAG_END());
				}

				switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "3PCC-PROXY, Sent a 200 OK, waiting for ACK\n");
				switch_safe_free(extra_headers);
			}

			/* Unlock the session signal to allow the ack to make it in */
			// Maybe we should timeout?
			switch_mutex_unlock(tech_pvt->sofia_mutex);

			while (switch_channel_ready(channel) && !sofia_test_flag(tech_pvt, TFLAG_3PCC_HAS_ACK)) {
				switch_cond_next();
			}

			/*  Regain lock on sofia */
			switch_mutex_lock(tech_pvt->sofia_mutex);

			if(is_proxy) {
				sofia_clear_flag(tech_pvt, TFLAG_3PCC_HAS_ACK);
				sofia_clear_flag(tech_pvt, TFLAG_3PCC);
				// This sends the message to the other leg that causes it to call the TFLAG_3PCC_INVITE code at the start of this function.
				// Is there another message it would be better to hang this on though?
				switch_core_session_pass_indication(session, SWITCH_MESSAGE_INDICATE_ANSWER);
			}

			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "3PCC-PROXY, Done waiting for ACK\n");
			return SWITCH_STATUS_SUCCESS;
		}

		if ((is_proxy && !b_sdp) || sofia_test_flag(tech_pvt, TFLAG_LATE_NEGOTIATION) ||
			switch_core_media_codec_chosen(tech_pvt->session, SWITCH_MEDIA_TYPE_AUDIO) != SWITCH_STATUS_SUCCESS) {
			sofia_clear_flag_locked(tech_pvt, TFLAG_LATE_NEGOTIATION);

			if (is_proxy) {
				switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "Disabling proxy mode due to call answer with no bridge\n");
				switch_channel_clear_flag(channel, CF_PROXY_MEDIA);
				switch_channel_clear_flag(channel, CF_PROXY_MODE);
			}

			if (switch_channel_direction(tech_pvt->channel) == SWITCH_CALL_DIRECTION_INBOUND) {
				const char *r_sdp = switch_channel_get_variable(channel, SWITCH_R_SDP_VARIABLE);

				switch_core_media_prepare_codecs(tech_pvt->session, SWITCH_TRUE);

				if (sofia_media_tech_media(tech_pvt, r_sdp) != SWITCH_STATUS_SUCCESS) {
					switch_channel_set_variable(channel, SWITCH_ENDPOINT_DISPOSITION_VARIABLE, "CODEC NEGOTIATION ERROR");
					//switch_mutex_lock(tech_pvt->sofia_mutex);
					//nua_respond(tech_pvt->nh, SIP_488_NOT_ACCEPTABLE, TAG_END());
					//switch_mutex_unlock(tech_pvt->sofia_mutex);
					return SWITCH_STATUS_FALSE;
				}
			}
		}

		if ((status = switch_core_media_choose_port(tech_pvt->session, SWITCH_MEDIA_TYPE_AUDIO, 0)) != SWITCH_STATUS_SUCCESS) {
			switch_channel_hangup(channel, SWITCH_CAUSE_DESTINATION_OUT_OF_ORDER);
			return status;
		}

		switch_core_media_gen_local_sdp(session, SDP_TYPE_RESPONSE, NULL, 0, NULL, 0);
		if (sofia_media_activate_rtp(tech_pvt) != SWITCH_STATUS_SUCCESS) {
			switch_channel_hangup(channel, SWITCH_CAUSE_DESTINATION_OUT_OF_ORDER);
		}

		if (tech_pvt->nh) {
			if (tech_pvt->mparams.local_sdp_str) {
				switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "Local SDP %s:\n%s\n", switch_channel_get_name(channel),
								  tech_pvt->mparams.local_sdp_str);
			}
		}

	}

	if (sofia_test_flag(tech_pvt, TFLAG_NAT) ||
		(val = switch_channel_get_variable(channel, "sip-force-contact")) ||
		((val = switch_channel_get_variable(channel, "sip_sticky_contact")) && switch_true(val))) {
		sticky = tech_pvt->record_route;
		session_timeout = SOFIA_NAT_SESSION_TIMEOUT;
		switch_channel_set_variable(channel, "sip_nat_detected", "true");
	}

	if ((val = switch_channel_get_variable(channel, SOFIA_SESSION_TIMEOUT))) {
		int v_session_timeout = atoi(val);
		if (v_session_timeout >= 0) {
			session_timeout = v_session_timeout;
		}
	}

	if (!sofia_test_flag(tech_pvt, TFLAG_BYE)) {
		char *extra_headers = sofia_glue_get_extra_headers(channel, SOFIA_SIP_RESPONSE_HEADER_PREFIX);
		char *cid = NULL;


		cid = generate_pai_str(tech_pvt);


		if (switch_channel_test_flag(tech_pvt->channel, CF_PROXY_MODE) && tech_pvt->mparams.early_sdp) {
			char *a, *b;

			/* start at the s= line to avoid some devices who update the o= between messages */
			a = strstr(tech_pvt->mparams.early_sdp, "s=");
			b = strstr(tech_pvt->mparams.local_sdp_str, "s=");

			if (!a || !b || strcmp(a, b)) {

				/* The SIP RFC for SOA forbids sending a 183 with one sdp then a 200 with another but it won't do us much good unless
				   we do so in this case we will abandon the SOA rules and go rogue.
				*/
				sofia_clear_flag(tech_pvt, TFLAG_ENABLE_SOA);
			}
		}

		if ((tech_pvt->session_timeout = session_timeout)) {
			tech_pvt->session_refresher = switch_channel_direction(channel) == SWITCH_CALL_DIRECTION_OUTBOUND ? nua_local_refresher : nua_remote_refresher;
		} else {
			tech_pvt->session_refresher = nua_no_refresher;
		}

		if (sofia_use_soa(tech_pvt)) {
			nua_respond(tech_pvt->nh, SIP_200_OK,
						NUTAG_AUTOANSWER(0),
						TAG_IF(call_info, SIPTAG_CALL_INFO_STR(call_info)),
						TAG_IF(sticky, NUTAG_PROXY(tech_pvt->record_route)),
						TAG_IF(cid, SIPTAG_HEADER_STR(cid)),
						NUTAG_SESSION_TIMER(tech_pvt->session_timeout),
						NUTAG_SESSION_REFRESHER(tech_pvt->session_refresher),
						SIPTAG_CONTACT_STR(tech_pvt->reply_contact),
						SIPTAG_CALL_INFO_STR(switch_channel_get_variable(tech_pvt->channel, SOFIA_SIP_HEADER_PREFIX "call_info")),
						SOATAG_USER_SDP_STR(tech_pvt->mparams.local_sdp_str),
						SOATAG_REUSE_REJECTED(1),
						SOATAG_AUDIO_AUX("cn telephone-event"),
						TAG_IF(sofia_test_pflag(tech_pvt->profile, PFLAG_DISABLE_100REL), NUTAG_INCLUDE_EXTRA_SDP(1)),
						SOATAG_RTP_SELECT(1),
						TAG_IF(!zstr(extra_headers), SIPTAG_HEADER_STR(extra_headers)),
						TAG_IF(switch_stristr("update_display", tech_pvt->x_freeswitch_support_remote),
							   SIPTAG_HEADER_STR("X-FS-Support: " FREESWITCH_SUPPORT)), TAG_END());
		} else {
			nua_respond(tech_pvt->nh, SIP_200_OK,
						NUTAG_AUTOANSWER(0),
						NUTAG_MEDIA_ENABLE(0),
						TAG_IF(call_info, SIPTAG_CALL_INFO_STR(call_info)),
						TAG_IF(sticky, NUTAG_PROXY(tech_pvt->record_route)),
						TAG_IF(cid, SIPTAG_HEADER_STR(cid)),
						NUTAG_SESSION_TIMER(tech_pvt->session_timeout),
						NUTAG_SESSION_REFRESHER(tech_pvt->session_refresher),
						SIPTAG_CONTACT_STR(tech_pvt->reply_contact),
						SIPTAG_CALL_INFO_STR(switch_channel_get_variable(tech_pvt->channel, SOFIA_SIP_HEADER_PREFIX "call_info")),
						SIPTAG_CONTENT_TYPE_STR("application/sdp"),
						SIPTAG_PAYLOAD_STR(tech_pvt->mparams.local_sdp_str),
						TAG_IF(!zstr(extra_headers), SIPTAG_HEADER_STR(extra_headers)),
						TAG_IF(switch_stristr("update_display", tech_pvt->x_freeswitch_support_remote),
							   SIPTAG_HEADER_STR("X-FS-Support: " FREESWITCH_SUPPORT)), TAG_END());
		}
		switch_safe_free(extra_headers);
		sofia_set_flag_locked(tech_pvt, TFLAG_ANS);
	}

	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t sofia_read_video_frame(switch_core_session_t *session, switch_frame_t **frame, switch_io_flag_t flags, int stream_id)
{
	private_object_t *tech_pvt = (private_object_t *) switch_core_session_get_private(session);

	switch_assert(tech_pvt != NULL);

	if (sofia_test_flag(tech_pvt, TFLAG_HUP)) {
		return SWITCH_STATUS_FALSE;
	}
#if 0
	while (!(tech_pvt->video_read_codec.implementation && switch_core_media_ready(tech_pvt->session, SWITCH_MEDIA_TYPE_VIDEO) && !switch_channel_test_flag(channel, CF_REQ_MEDIA))) {
		switch_ivr_parse_all_messages(tech_pvt->session);

		if (--sanity && switch_channel_ready(channel)) {
			switch_yield(10000);
		} else {
			switch_channel_hangup(tech_pvt->channel, SWITCH_CAUSE_RECOVERY_ON_TIMER_EXPIRE);
			return SWITCH_STATUS_GENERR;
		}
	}
#endif


	return switch_core_media_read_frame(session, frame, flags, stream_id, SWITCH_MEDIA_TYPE_VIDEO);

}

static switch_status_t sofia_write_video_frame(switch_core_session_t *session, switch_frame_t *frame, switch_io_flag_t flags, int stream_id)
{
	private_object_t *tech_pvt = (private_object_t *) switch_core_session_get_private(session);
	switch_assert(tech_pvt != NULL);

#if 0
	while (!(tech_pvt->video_read_codec.implementation && switch_core_media_ready(tech_pvt->session, SWITCH_MEDIA_TYPE_VIDEO))) {
		if (switch_channel_ready(channel)) {
			switch_yield(10000);
		} else {
			return SWITCH_STATUS_GENERR;
		}
	}
#endif

	if (sofia_test_flag(tech_pvt, TFLAG_HUP)) {
		return SWITCH_STATUS_FALSE;
	}

	if (!sofia_test_flag(tech_pvt, TFLAG_RTP)) {
		return SWITCH_STATUS_GENERR;
	}

	if (!sofia_test_flag(tech_pvt, TFLAG_IO)) {
		return SWITCH_STATUS_SUCCESS;
	}

	if (SWITCH_STATUS_SUCCESS == switch_core_media_write_frame(session, frame, flags, stream_id, SWITCH_MEDIA_TYPE_VIDEO)) {
		return SWITCH_STATUS_SUCCESS;
	}

	return SWITCH_STATUS_FALSE;
}

static switch_status_t sofia_read_frame(switch_core_session_t *session, switch_frame_t **frame, switch_io_flag_t flags, int stream_id)
{
	private_object_t *tech_pvt = switch_core_session_get_private(session);
	switch_channel_t *channel = switch_core_session_get_channel(session);
	uint32_t sanity = 1000;
	switch_status_t status = SWITCH_STATUS_FALSE;

	switch_assert(tech_pvt != NULL);

	if (!sofia_test_pflag(tech_pvt->profile, PFLAG_RUNNING)) {
		switch_channel_hangup(tech_pvt->channel, SWITCH_CAUSE_NORMAL_CLEARING);
		return SWITCH_STATUS_FALSE;
	}

	if (sofia_test_flag(tech_pvt, TFLAG_HUP)) {
		return SWITCH_STATUS_FALSE;
	}

	while (!(switch_core_media_ready(tech_pvt->session, SWITCH_MEDIA_TYPE_AUDIO) && !switch_channel_test_flag(channel, CF_REQ_MEDIA))) {
		switch_ivr_parse_all_messages(tech_pvt->session);
		if (--sanity && switch_channel_up(channel)) {
			switch_yield(10000);
		} else {
			switch_channel_hangup(tech_pvt->channel, SWITCH_CAUSE_RECOVERY_ON_TIMER_EXPIRE);
			return SWITCH_STATUS_GENERR;
		}
	}


	sofia_set_flag_locked(tech_pvt, TFLAG_READING);

	if (sofia_test_flag(tech_pvt, TFLAG_HUP) || sofia_test_flag(tech_pvt, TFLAG_BYE)) {
		return SWITCH_STATUS_FALSE;
	}

	status = switch_core_media_read_frame(session, frame, flags, stream_id, SWITCH_MEDIA_TYPE_AUDIO);

	sofia_clear_flag_locked(tech_pvt, TFLAG_READING);

	return status;
}

static switch_status_t sofia_write_frame(switch_core_session_t *session, switch_frame_t *frame, switch_io_flag_t flags, int stream_id)
{
	private_object_t *tech_pvt = switch_core_session_get_private(session);
	switch_channel_t *channel = switch_core_session_get_channel(session);
	switch_status_t status = SWITCH_STATUS_SUCCESS;

	switch_assert(tech_pvt != NULL);

	while (!(switch_core_media_ready(tech_pvt->session, SWITCH_MEDIA_TYPE_AUDIO) && !switch_channel_test_flag(channel, CF_REQ_MEDIA))) {
		if (switch_channel_ready(channel)) {
			switch_yield(10000);
		} else {
			return SWITCH_STATUS_GENERR;
		}
	}


	if (sofia_test_flag(tech_pvt, TFLAG_HUP)) {
		return SWITCH_STATUS_FALSE;
	}

#if 0
	if (!sofia_test_flag(tech_pvt, TFLAG_RTP)) {
		return SWITCH_STATUS_GENERR;
	}

	if (!sofia_test_flag(tech_pvt, TFLAG_IO)) {
		return SWITCH_STATUS_SUCCESS;
	}
#endif

	if (sofia_test_flag(tech_pvt, TFLAG_BYE)) {
		return SWITCH_STATUS_FALSE;
	}

	sofia_set_flag_locked(tech_pvt, TFLAG_WRITING);

	if (switch_core_media_write_frame(session, frame, flags, stream_id, SWITCH_MEDIA_TYPE_AUDIO)) {
		status = SWITCH_STATUS_SUCCESS;
	}

	sofia_clear_flag_locked(tech_pvt, TFLAG_WRITING);
	return status;
}

static switch_status_t sofia_kill_channel(switch_core_session_t *session, int sig)
{
	private_object_t *tech_pvt = switch_core_session_get_private(session);

	if (!tech_pvt) {
		return SWITCH_STATUS_FALSE;
	}

	switch (sig) {
	case SWITCH_SIG_BREAK:
		if (switch_core_media_ready(tech_pvt->session, SWITCH_MEDIA_TYPE_AUDIO)) {
			switch_core_media_break(tech_pvt->session, SWITCH_MEDIA_TYPE_AUDIO);
		}
		if (switch_core_media_ready(tech_pvt->session, SWITCH_MEDIA_TYPE_VIDEO)) {
			switch_core_media_break(tech_pvt->session, SWITCH_MEDIA_TYPE_VIDEO);
		}
		break;
	case SWITCH_SIG_KILL:
	default:
		sofia_clear_flag_locked(tech_pvt, TFLAG_IO);
		sofia_set_flag_locked(tech_pvt, TFLAG_HUP);

		if (switch_core_media_ready(tech_pvt->session, SWITCH_MEDIA_TYPE_AUDIO)) {
			switch_core_media_kill_socket(tech_pvt->session, SWITCH_MEDIA_TYPE_AUDIO);
		}
		if (switch_core_media_ready(tech_pvt->session, SWITCH_MEDIA_TYPE_VIDEO)) {
			switch_core_media_kill_socket(tech_pvt->session, SWITCH_MEDIA_TYPE_VIDEO);
		}
		break;
	}
	return SWITCH_STATUS_SUCCESS;
}


static switch_status_t sofia_send_dtmf(switch_core_session_t *session, const switch_dtmf_t *dtmf)
{
	private_object_t *tech_pvt;
	char message[128] = "";
	switch_core_media_dtmf_t dtmf_type;

	tech_pvt = (private_object_t *) switch_core_session_get_private(session);
	switch_assert(tech_pvt != NULL);

	dtmf_type = tech_pvt->mparams.dtmf_type;

	/* We only can send INFO when we have no media */
	if (!switch_core_media_ready(tech_pvt->session, SWITCH_MEDIA_TYPE_AUDIO) ||
		!switch_channel_media_ready(tech_pvt->channel) || switch_channel_test_flag(tech_pvt->channel, CF_PROXY_MODE)) {
		dtmf_type = DTMF_INFO;
	}

	switch (dtmf_type) {
	case DTMF_2833:
		{
			return switch_core_media_queue_rfc2833(tech_pvt->session, SWITCH_MEDIA_TYPE_AUDIO, dtmf);
		}
	case DTMF_INFO:
		{
			if (dtmf->digit == 'w') {
				switch_yield(500000);
			} else if (dtmf->digit == 'W') {
				switch_yield(1000000);
			} else {
				snprintf(message, sizeof(message), "Signal=%c\r\nDuration=%d\r\n", dtmf->digit, dtmf->duration / 8);
				switch_mutex_lock(tech_pvt->sofia_mutex);
				nua_info(tech_pvt->nh, SIPTAG_CONTENT_TYPE_STR("application/dtmf-relay"), SIPTAG_PAYLOAD_STR(message), TAG_END());
				switch_mutex_unlock(tech_pvt->sofia_mutex);
			}
		}
		break;
	case DTMF_NONE:
		break;
	default:
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_WARNING, "Unhandled DTMF type!\n");
		break;
	}

	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t sofia_receive_message(switch_core_session_t *session, switch_core_session_message_t *msg)
{
	switch_channel_t *channel = switch_core_session_get_channel(session);
	private_object_t *tech_pvt = switch_core_session_get_private(session);
	switch_status_t status = SWITCH_STATUS_SUCCESS;

	if (msg->message_id == SWITCH_MESSAGE_INDICATE_SIGNAL_DATA) {
		sofia_dispatch_event_t *de = (sofia_dispatch_event_t *) msg->pointer_arg;
		switch_mutex_lock(tech_pvt->sofia_mutex);
		if (switch_core_session_in_thread(session)) {
			de->session = session;
		}

		sofia_process_dispatch_event(&de);


		switch_mutex_unlock(tech_pvt->sofia_mutex);
		goto end;
	}


	if (switch_channel_down(channel) || !tech_pvt || sofia_test_flag(tech_pvt, TFLAG_BYE)) {
		status = SWITCH_STATUS_FALSE;
		goto end;
	}

	/* ones that do not need to lock sofia mutex */
	switch (msg->message_id) {
	case SWITCH_MESSAGE_INDICATE_KEEPALIVE:
		{
			if (msg->numeric_arg) {
				sofia_set_flag_locked(tech_pvt, TFLAG_KEEPALIVE);
			} else {
				sofia_clear_flag_locked(tech_pvt, TFLAG_KEEPALIVE);
			}
		}
		break;
	case SWITCH_MESSAGE_HEARTBEAT_EVENT:
		{
			char pl[160] = "";

			switch_snprintf(pl, sizeof(pl), "KEEP-ALIVE %d\n", ++tech_pvt->keepalive);

			if (sofia_test_flag(tech_pvt, TFLAG_KEEPALIVE)) {
				if (tech_pvt->profile->keepalive == KA_MESSAGE) {
					nua_message(tech_pvt->nh,
								SIPTAG_CONTENT_TYPE_STR("text/plain"),
								TAG_IF(!zstr(tech_pvt->user_via), SIPTAG_VIA_STR(tech_pvt->user_via)),
								SIPTAG_PAYLOAD_STR(pl),
								TAG_END());
				} else if (tech_pvt->profile->keepalive == KA_INFO) {
					nua_info(tech_pvt->nh,
							 SIPTAG_CONTENT_TYPE_STR("text/plain"),
							 TAG_IF(!zstr(tech_pvt->user_via), SIPTAG_VIA_STR(tech_pvt->user_via)),
							 SIPTAG_PAYLOAD_STR(pl),
							 TAG_END());
				}
			}
		}
		break;
	case SWITCH_MESSAGE_INDICATE_RECOVERY_REFRESH:
	case SWITCH_MESSAGE_INDICATE_APPLICATION_EXEC:
		break;
	case SWITCH_MESSAGE_INDICATE_MEDIA_RENEG:
		{
			if (msg->string_arg) {
				sofia_set_media_flag(tech_pvt->profile, SCMF_RENEG_ON_REINVITE);
				sofia_clear_flag(tech_pvt, TFLAG_ENABLE_SOA);
			}

			sofia_glue_do_invite(session);
		}
		break;
	case SWITCH_MESSAGE_INDICATE_BRIDGE:
			switch_channel_set_variable(channel, SOFIA_REPLACES_HEADER, NULL);

			if (switch_true(switch_channel_get_variable(channel, "sip_auto_simplify"))) {
				sofia_set_flag(tech_pvt, TFLAG_SIMPLIFY);
			}


		break;
	case SWITCH_MESSAGE_INDICATE_BLIND_TRANSFER_RESPONSE:
		{
			const char *event;
			const char *uuid;
			char *xdest;

			if (msg->string_arg) {
				nua_notify(tech_pvt->nh, NUTAG_NEWSUB(1), SIPTAG_CONTENT_TYPE_STR("message/sipfrag;version=2.0"),
						   NUTAG_SUBSTATE(nua_substate_terminated),
						   SIPTAG_SUBSCRIPTION_STATE_STR("terminated;reason=noresource"),
						   SIPTAG_PAYLOAD_STR(msg->string_arg),
						   SIPTAG_EVENT_STR("refer"), TAG_END());
				goto end;
			}

			event = switch_channel_get_variable(channel, "sip_blind_transfer_event");
			uuid = switch_channel_get_variable(channel, "blind_transfer_uuid");

			if (event && uuid) {
				char payload_str[255] = "SIP/2.0 403 Forbidden\r\n";
				if (msg->numeric_arg) {
					switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG,
							"%s Completing blind transfer with success\n", switch_channel_get_name(channel));
					switch_set_string(payload_str, "SIP/2.0 200 OK\r\n");
				} else if (uuid) {
					switch_core_session_t *other_session = switch_core_session_locate(uuid);
					if (other_session) {
						switch_channel_t *other_channel = switch_core_session_get_channel(other_session);
						const char *invite_failure_status = switch_channel_get_variable(other_channel, "sip_invite_failure_status");
						const char *invite_failure_str = switch_channel_get_variable(other_channel, "sip_invite_failure_status");
						if (!zstr(invite_failure_status) && !zstr(invite_failure_str)) {
							snprintf(payload_str, sizeof(payload_str), "SIP/2.0 %s %s\r\n", invite_failure_status, invite_failure_str);
							switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG,
									"%s Completing blind transfer with custom failure: %s %s\n",
									switch_channel_get_name(channel), invite_failure_status, invite_failure_str);
						}
						switch_core_session_rwunlock(other_session);
					}
				}
				switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG,
						"%s Completing blind transfer with status: %s\n", switch_channel_get_name(channel), payload_str);
				nua_notify(tech_pvt->nh, NUTAG_NEWSUB(1), SIPTAG_CONTENT_TYPE_STR("message/sipfrag;version=2.0"),
						   NUTAG_SUBSTATE(nua_substate_terminated),
						   SIPTAG_SUBSCRIPTION_STATE_STR("terminated;reason=noresource"),
						   SIPTAG_PAYLOAD_STR(payload_str),
						   SIPTAG_EVENT_STR(event), TAG_END());


				if (!msg->numeric_arg) {
					xdest = switch_core_session_sprintf(session, "intercept:%s", uuid);
					switch_ivr_session_transfer(session, xdest, "inline", NULL);
				}
			}

		}
		goto end;
	case SWITCH_MESSAGE_INDICATE_CLEAR_PROGRESS:
		if (!switch_channel_test_flag(channel, CF_ANSWERED)) {
			sofia_clear_flag(tech_pvt, TFLAG_EARLY_MEDIA);
		}
		goto end;
	case SWITCH_MESSAGE_INDICATE_ANSWER:
	case SWITCH_MESSAGE_INDICATE_PROGRESS:
		{
			const char *var;
			const char *presence_data = switch_channel_get_variable(channel, "presence_data");
			const char *presence_id = switch_channel_get_variable(channel, "presence_id");


			if ((var = switch_channel_get_variable(channel, "sip_force_nat_mode")) && switch_true(var)) {
				sofia_set_flag(tech_pvt, TFLAG_NAT);
				switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "Setting NAT mode based on manual variable\n");
				switch_channel_set_variable(channel, "sip_nat_detected", "true");
			}

			if ((var = switch_channel_get_variable(channel, "sip_enable_soa"))) {
				if (switch_true(var)) {
					sofia_set_flag(tech_pvt, TFLAG_ENABLE_SOA);
				} else {
					sofia_clear_flag(tech_pvt, TFLAG_ENABLE_SOA);
				}
			}


			if (presence_id || presence_data) {
				char *sql = switch_mprintf("update sip_dialogs set presence_id='%q',presence_data='%q' "
										   "where uuid='%s';\n", switch_str_nil(presence_id), switch_str_nil(presence_data),
										   switch_core_session_get_uuid(session));
				switch_assert(sql);
				sofia_glue_execute_sql_now(tech_pvt->profile, &sql, SWITCH_TRUE);
			}

			if (sofia_test_media_flag(tech_pvt->profile, SCMF_AUTOFIX_TIMING)) {
				switch_core_media_reset_autofix(tech_pvt->session, SWITCH_MEDIA_TYPE_AUDIO);
			}
		}
		break;
	default:
		break;
	}

	/* ones that do need to lock sofia mutex */
	switch_mutex_lock(tech_pvt->sofia_mutex);

	if (switch_channel_down(channel) || !tech_pvt || sofia_test_flag(tech_pvt, TFLAG_BYE)) {
		status = SWITCH_STATUS_FALSE;
		goto end_lock;
	}

	switch (msg->message_id) {

	case SWITCH_MESSAGE_INDICATE_VIDEO_REFRESH_REQ:
		{
			const char *pl = "<?xml version=\"1.0\" encoding=\"utf-8\" ?>\n<media_control>\n<vc_primitive>\n<to_encoder>\n<picture_fast_update>\n</picture_fast_update>\n</to_encoder>\n</vc_primitive>\n</media_control>";
			time_t now = switch_epoch_time_now(NULL);

			if (!tech_pvt->last_vid_info || (now - tech_pvt->last_vid_info) > 1) {
				
				tech_pvt->last_vid_info = now;

				if (!zstr(msg->string_arg)) {
					pl = msg->string_arg;
				}

				nua_info(tech_pvt->nh, SIPTAG_CONTENT_TYPE_STR("application/media_control+xml"), SIPTAG_PAYLOAD_STR(pl), TAG_END());
			}

		}
		break;
	case SWITCH_MESSAGE_INDICATE_BROADCAST:
		{
			const char *ip = NULL, *port = NULL;
			ip = switch_channel_get_variable(channel, SWITCH_REMOTE_MEDIA_IP_VARIABLE);
			port = switch_channel_get_variable(channel, SWITCH_REMOTE_MEDIA_PORT_VARIABLE);
			if (ip && port) {
				switch_core_media_gen_local_sdp(session, SDP_TYPE_REQUEST, ip, (switch_port_t)atoi(port), msg->string_arg, 1);
			}

			if (!sofia_test_flag(tech_pvt, TFLAG_BYE)) {
				if (sofia_use_soa(tech_pvt)) {
					nua_respond(tech_pvt->nh, SIP_200_OK,
								SIPTAG_CONTACT_STR(tech_pvt->reply_contact),
								SOATAG_USER_SDP_STR(tech_pvt->mparams.local_sdp_str),
								SOATAG_REUSE_REJECTED(1),
								SOATAG_AUDIO_AUX("cn telephone-event"), NUTAG_INCLUDE_EXTRA_SDP(1),
								TAG_END());
				} else {
					nua_respond(tech_pvt->nh, SIP_200_OK,
								NUTAG_MEDIA_ENABLE(0),
								SIPTAG_CONTACT_STR(tech_pvt->reply_contact),
								SIPTAG_CONTENT_TYPE_STR("application/sdp"), SIPTAG_PAYLOAD_STR(tech_pvt->mparams.local_sdp_str), TAG_END());
				}
				switch_channel_mark_answered(channel);
			}
		}
		break;
	case SWITCH_MESSAGE_INDICATE_NOMEDIA:
		{
			sofia_glue_do_invite(session);
		}
		break;

	case SWITCH_MESSAGE_INDICATE_MEDIA_REDIRECT:
		{
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "%s Sending media re-direct:\n%s\n",
							  switch_channel_get_name(channel), msg->string_arg);
			switch_core_media_set_local_sdp(session, msg->string_arg, SWITCH_TRUE);

			if(zstr(tech_pvt->mparams.local_sdp_str)) {
				sofia_set_flag(tech_pvt, TFLAG_3PCC_INVITE);
			}

			sofia_set_flag_locked(tech_pvt, TFLAG_SENT_UPDATE);

			if (!switch_channel_test_flag(channel, CF_PROXY_MEDIA)) {
				switch_channel_set_flag(channel, CF_REQ_MEDIA);
			}
			sofia_glue_do_invite(session);

		}
		break;

	case SWITCH_MESSAGE_INDICATE_T38_DESCRIPTION:
		{
			switch_t38_options_t *t38_options = switch_channel_get_private(tech_pvt->channel, "t38_options");

			if (!t38_options) {
				nua_respond(tech_pvt->nh, SIP_488_NOT_ACCEPTABLE, TAG_END());
				goto end_lock;
			}

			switch_core_media_start_udptl(tech_pvt->session, t38_options);

			switch_core_media_set_udptl_image_sdp(tech_pvt->session, t38_options, msg->numeric_arg);

			if (!sofia_test_flag(tech_pvt, TFLAG_BYE)) {
				char *extra_headers = sofia_glue_get_extra_headers(channel, SOFIA_SIP_RESPONSE_HEADER_PREFIX);
				if (sofia_use_soa(tech_pvt)) {
					nua_respond(tech_pvt->nh, SIP_200_OK,
								NUTAG_AUTOANSWER(0),
								SIPTAG_CONTACT_STR(tech_pvt->reply_contact),
								SIPTAG_CALL_INFO_STR(switch_channel_get_variable(tech_pvt->channel, SOFIA_SIP_HEADER_PREFIX "call_info")),
								SOATAG_USER_SDP_STR(tech_pvt->mparams.local_sdp_str),
								SOATAG_REUSE_REJECTED(1),
								SOATAG_ORDERED_USER(1),
								SOATAG_AUDIO_AUX("cn telephone-event"), NUTAG_INCLUDE_EXTRA_SDP(1),
								TAG_IF(!zstr(extra_headers), SIPTAG_HEADER_STR(extra_headers)), TAG_END());
				} else {
					nua_respond(tech_pvt->nh, SIP_200_OK,
								NUTAG_AUTOANSWER(0),
								NUTAG_MEDIA_ENABLE(0),
								SIPTAG_CONTACT_STR(tech_pvt->reply_contact),
								SIPTAG_CALL_INFO_STR(switch_channel_get_variable(tech_pvt->channel, SOFIA_SIP_HEADER_PREFIX "call_info")),
								SIPTAG_CONTENT_TYPE_STR("application/sdp"),
								SIPTAG_PAYLOAD_STR(tech_pvt->mparams.local_sdp_str), TAG_IF(!zstr(extra_headers), SIPTAG_HEADER_STR(extra_headers)), TAG_END());
				}
				switch_safe_free(extra_headers);
				sofia_set_flag_locked(tech_pvt, TFLAG_ANS);
			}
		}
		break;

	case SWITCH_MESSAGE_INDICATE_REQUEST_IMAGE_MEDIA:
		{
			switch_t38_options_t *t38_options = switch_channel_get_private(tech_pvt->channel, "t38_options");

			if (t38_options) {
				switch_core_media_set_udptl_image_sdp(tech_pvt->session, t38_options, msg->numeric_arg);

				if (!switch_channel_test_flag(channel, CF_PROXY_MEDIA)) {
					switch_channel_set_flag(channel, CF_REQ_MEDIA);
				}
				sofia_set_flag_locked(tech_pvt, TFLAG_SENT_UPDATE);
				sofia_glue_do_invite(session);
			} else {
				switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_WARNING, "%s Request to send IMAGE on channel with not t38 options.\n",
								  switch_channel_get_name(channel));
			}
		}
		break;

	case SWITCH_MESSAGE_INDICATE_MEDIA:
		{
			uint32_t send_invite = 1;
			const char *r_sdp = switch_channel_get_variable(channel, SWITCH_R_SDP_VARIABLE);

			switch_channel_clear_flag(channel, CF_PROXY_MODE);
			switch_core_media_set_local_sdp(session, NULL, SWITCH_FALSE);

			if (!(switch_channel_test_flag(channel, CF_ANSWERED) || switch_channel_test_flag(channel, CF_EARLY_MEDIA))) {
				if (switch_channel_direction(tech_pvt->channel) == SWITCH_CALL_DIRECTION_INBOUND) {

					switch_core_media_prepare_codecs(tech_pvt->session, SWITCH_TRUE);
					if (sofia_media_tech_media(tech_pvt, r_sdp) != SWITCH_STATUS_SUCCESS) {
						switch_channel_set_variable(channel, SWITCH_ENDPOINT_DISPOSITION_VARIABLE, "CODEC NEGOTIATION ERROR");
						status = SWITCH_STATUS_FALSE;
						goto end_lock;
					}
					send_invite = 0;
				}
			}


			switch_core_media_set_sdp_codec_string(tech_pvt->session, r_sdp, SDP_TYPE_RESPONSE);
			switch_channel_set_variable(tech_pvt->channel, "absolute_codec_string", switch_channel_get_variable(tech_pvt->channel, "ep_codec_string"));
			switch_core_media_prepare_codecs(tech_pvt->session, SWITCH_TRUE);
			
			if ((status = switch_core_media_choose_port(tech_pvt->session, SWITCH_MEDIA_TYPE_AUDIO, 0)) != SWITCH_STATUS_SUCCESS) {
				switch_channel_hangup(channel, SWITCH_CAUSE_DESTINATION_OUT_OF_ORDER);
				goto end_lock;
			}
			
			switch_core_media_gen_local_sdp(session, SDP_TYPE_REQUEST, NULL, 0, NULL, 1);

			if (!msg->numeric_arg) {
				if (send_invite) {
					if (!switch_channel_test_flag(channel, CF_PROXY_MEDIA)) {
						switch_channel_set_flag(channel, CF_REQ_MEDIA);
					}
					sofia_glue_do_invite(session);
				} else {
					status = SWITCH_STATUS_FALSE;
				}
			}
		}
		break;

	case SWITCH_MESSAGE_INDICATE_PHONE_EVENT:
		{
			const char *event = "talk";
			const char *full_to = NULL;

			if (!zstr(msg->string_arg) && strcasecmp(msg->string_arg, event)) {
				if (!strcasecmp(msg->string_arg, "hold")) {
					event = "hold";
				} else if (strncasecmp(msg->string_arg, "talk", 4)) {
					switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_WARNING, "Invalid event.\n");
				}
			}

			if (!switch_channel_test_flag(channel, CF_ANSWERED) && switch_channel_direction(channel) == SWITCH_CALL_DIRECTION_INBOUND) {
				switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR,
								  "Operation not permitted on an inbound non-answered call leg!\n");
			} else {
				full_to = switch_str_nil(switch_channel_get_variable(channel, "sip_full_to"));				
				nua_notify(tech_pvt->nh, NUTAG_NEWSUB(1), NUTAG_SUBSTATE(nua_substate_active),
						   TAG_IF((full_to), SIPTAG_TO_STR(full_to)),SIPTAG_SUBSCRIPTION_STATE_STR("active"),
						   SIPTAG_EVENT_STR(event), TAG_END());
			}

		}
		break;
	case SWITCH_MESSAGE_INDICATE_MESSAGE:
		{
			char *ct = "text/plain";
			int ok = 0;

			if (!zstr(msg->string_array_arg[3]) && !strcmp(msg->string_array_arg[3], tech_pvt->caller_profile->uuid)) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "Not sending message back to sender\n");
				break;
			}

			if (!zstr(msg->string_array_arg[0]) && !zstr(msg->string_array_arg[1])) {
				ct = switch_core_session_sprintf(session, "%s/%s", msg->string_array_arg[0], msg->string_array_arg[1]);
				ok = 1;
			}

			if (switch_stristr("send_message", tech_pvt->x_freeswitch_support_remote)) {
				ok = 1;
			}

			if (switch_true(switch_channel_get_variable(channel, "fs_send_unsupported_message"))) {
				ok = 1;
			}

			if (ok) {
				const char *pl = NULL;

				if (!zstr(msg->string_array_arg[2])) {
					pl = msg->string_array_arg[2];
				}

				nua_message(tech_pvt->nh,
						 SIPTAG_CONTENT_TYPE_STR(ct),
						 TAG_IF(!zstr(tech_pvt->user_via), SIPTAG_VIA_STR(tech_pvt->user_via)),
						 TAG_IF(pl, SIPTAG_PAYLOAD_STR(pl)),
						 TAG_END());
			} else {
				switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG,
								  "%s send_message is not supported.\n", switch_channel_get_name(channel));
			}
		}
		break;
	case SWITCH_MESSAGE_INDICATE_INFO:
		{
			char *ct = "freeswitch/data";
			int ok = 0;

			if (!zstr(msg->string_array_arg[0]) && !zstr(msg->string_array_arg[1])) {
				ct = switch_core_session_sprintf(session, "%s/%s", msg->string_array_arg[0], msg->string_array_arg[1]);
				ok = 1;
			}

			if (switch_stristr("send_info", tech_pvt->x_freeswitch_support_remote)) {
				ok = 1;
			}

			/* TODO: 1.4 remove this stanza */
			if (switch_true(switch_channel_get_variable(channel, "fs_send_unspported_info"))) {
				switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_WARNING,
								  "fs_send_unspported_info is deprecated in favor of correctly spelled fs_send_unsupported_info\n");
				ok = 1;
			}

			if (switch_true(switch_channel_get_variable(channel, "fs_send_unsupported_info"))) {
				ok = 1;
			}

			if (ok) {
				char *headers = sofia_glue_get_extra_headers(channel, SOFIA_SIP_INFO_HEADER_PREFIX);
				const char *pl = NULL;

				if (!zstr(msg->string_array_arg[2])) {
					pl = msg->string_array_arg[2];
				}

				nua_info(tech_pvt->nh,
						 SIPTAG_CONTENT_TYPE_STR(ct),
						 TAG_IF(!zstr(headers), SIPTAG_HEADER_STR(headers)),
						 TAG_IF(!zstr(tech_pvt->user_via), SIPTAG_VIA_STR(tech_pvt->user_via)),
						 TAG_IF(pl, SIPTAG_PAYLOAD_STR(pl)),
						 TAG_END());

				switch_safe_free(headers);
			} else {
				switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "%s send_info is not supported.\n", switch_channel_get_name(channel));
			}
		}
		break;
	case SWITCH_MESSAGE_INDICATE_SIMPLIFY:
		{
			char *ref_to, *ref_by;
			const char *uuid;
			const char *call_id = NULL, *to_user = NULL, *to_host = NULL, *to_tag = NULL, *from_tag = NULL, *from_user = NULL, *from_host = NULL;

			if ((uuid = switch_channel_get_partner_uuid(channel))) {
				switch_core_session_t *rsession;
				if ((rsession = switch_core_session_locate(uuid))) {
					switch_channel_t *rchannel = switch_core_session_get_channel(rsession);
					call_id = switch_channel_get_variable(rchannel, "sip_call_id");

					to_user = switch_channel_get_variable(rchannel, "sip_to_user");

					if (switch_channel_direction(rchannel) == SWITCH_CALL_DIRECTION_OUTBOUND) {
						to_host = switch_channel_get_variable(rchannel, "sip_to_host");
						from_user = switch_channel_get_variable(channel, "sip_from_user");
						from_host = switch_channel_get_variable(channel, "sip_from_host");
						to_tag = switch_channel_get_variable(rchannel, "sip_to_tag");
						from_tag = switch_channel_get_variable(rchannel, "sip_from_tag");
					} else {
						to_host = switch_channel_get_variable(channel, "sip_to_host");
						from_user = switch_channel_get_variable(rchannel, "sip_from_user");
						from_host = switch_channel_get_variable(rchannel, "sip_from_host");
						from_tag = switch_channel_get_variable(rchannel, "sip_to_tag");
						to_tag = switch_channel_get_variable(rchannel, "sip_from_tag");
					}

					switch_core_session_rwunlock(rsession);
				}
			}

			if (to_user && to_host && from_user && from_host && call_id && to_tag && from_tag) {
				char in[512] = "", out[1536] = "";

				switch_snprintf(in, sizeof(in), "%s;to-tag=%s;from-tag=%s", call_id, to_tag, from_tag);
				switch_url_encode(in, out, sizeof(out));

				ref_to = switch_mprintf("<sip:%s@%s?Replaces=%s>", to_user, to_host, out);
				ref_by = switch_mprintf("<sip:%s@%s>", from_user, from_host);

				nua_refer(tech_pvt->nh, SIPTAG_REFER_TO_STR(ref_to), SIPTAG_REFERRED_BY_STR(ref_by), TAG_END());
				switch_safe_free(ref_to);
				switch_safe_free(ref_by);
			}
		}
		break;
	case SWITCH_MESSAGE_INDICATE_DISPLAY:
		{
			const char *name = NULL, *number = NULL;
			const char *call_info = NULL;

			if (!sofia_test_pflag(tech_pvt->profile, PFLAG_SEND_DISPLAY_UPDATE)) {
				goto end_lock;
			}

			name = msg->string_array_arg[0];
			number = msg->string_array_arg[1];
			call_info = switch_channel_get_variable(channel, "presence_call_info_full");

			if (!zstr(name)) {
				char message[256] = "";
				const char *ua = switch_channel_get_variable(tech_pvt->channel, "sip_user_agent");
				switch_event_t *event;

				check_decode(name, tech_pvt->session);


				if (zstr(number)) {
					number = tech_pvt->caller_profile->destination_number;
				}

				switch_ivr_eavesdrop_update_display(session, name, number);

				if (!sofia_test_flag(tech_pvt, TFLAG_UPDATING_DISPLAY) && switch_channel_test_flag(channel, CF_ANSWERED)) {
					if (zstr(tech_pvt->last_sent_callee_id_name) || strcmp(tech_pvt->last_sent_callee_id_name, name) ||
						zstr(tech_pvt->last_sent_callee_id_number) || strcmp(tech_pvt->last_sent_callee_id_number, number)) {

						if (switch_stristr("update_display", tech_pvt->x_freeswitch_support_remote)) {
							snprintf(message, sizeof(message), "X-FS-Display-Name: %s\nX-FS-Display-Number: %s\n", name, number);

							if (switch_channel_test_flag(tech_pvt->channel, CF_LAZY_ATTENDED_TRANSFER)) {
								snprintf(message + strlen(message), sizeof(message) - strlen(message), "X-FS-Lazy-Attended-Transfer: true\n");
								switch_channel_clear_flag(tech_pvt->channel, CF_LAZY_ATTENDED_TRANSFER);
								switch_channel_clear_flag(tech_pvt->channel, CF_ATTENDED_TRANSFER);
							}

							if (switch_channel_test_flag(tech_pvt->channel, CF_ATTENDED_TRANSFER)) {
								snprintf(message + strlen(message), sizeof(message) - strlen(message), "X-FS-Attended-Transfer: true\n");
								switch_channel_clear_flag(tech_pvt->channel, CF_ATTENDED_TRANSFER);
							}

							nua_info(tech_pvt->nh, SIPTAG_CONTENT_TYPE_STR("message/update_display"),
									 TAG_IF(!zstr_buf(message), SIPTAG_HEADER_STR(message)),
									 TAG_IF(!zstr(tech_pvt->user_via), SIPTAG_VIA_STR(tech_pvt->user_via)), TAG_END());
						} else if (ua && switch_stristr("snom", ua)) {
							const char *ver_str = NULL;
							int version = 0;

							ver_str = switch_stristr( "/", ua);

							if ( ver_str ) {
								char *argv[4] = { 0 };
								char *dotted = strdup( ver_str + 1 );
								if ( dotted ) {
									switch_separate_string(dotted, '.', argv, (sizeof(argv) / sizeof(argv[0])));
									if ( argv[0] && argv[1] && argv[2] ) {
										version = ( atoi(argv[0]) * 10000 )  + ( atoi(argv[1]) * 100 ) + atoi(argv[2]);
									}
								}
								switch_safe_free( dotted );
							}

							if ( version >= 80424 ) {
								if (zstr(name)) {
									snprintf(message, sizeof(message), "From: %s\r\nTo:\r\n", number);
								} else {
									snprintf(message, sizeof(message), "From: \"%s\" %s\r\nTo:\r\n", name, number);
								}
							} else {
								if (zstr(name)) {
									snprintf(message, sizeof(message), "From:\r\nTo: %s\r\n", number);
								} else {
									snprintf(message, sizeof(message), "From:\r\nTo: \"%s\" %s\r\n", name, number);
								}
							}

							nua_info(tech_pvt->nh, SIPTAG_CONTENT_TYPE_STR("message/sipfrag"),
									 TAG_IF(!zstr(tech_pvt->user_via), SIPTAG_VIA_STR(tech_pvt->user_via)), SIPTAG_PAYLOAD_STR(message), TAG_END());
						} else if ((ua && (switch_stristr("polycom", ua)))) {
							if ( switch_stristr("UA/4", ua) ) {
								snprintf(message, sizeof(message), "P-Asserted-Identity: \"%s\" <sip:%s@%s>", name, number, tech_pvt->profile->sipip);
							} else {
								snprintf(message, sizeof(message), "P-Asserted-Identity: \"%s\" <%s>", name, number);
							}
							sofia_set_flag_locked(tech_pvt, TFLAG_UPDATING_DISPLAY);
							nua_update(tech_pvt->nh,
									   NUTAG_SESSION_TIMER(tech_pvt->session_timeout),
									   NUTAG_SESSION_REFRESHER(tech_pvt->session_refresher),
									   TAG_IF(call_info, SIPTAG_CALL_INFO_STR(call_info)),
									   TAG_IF(!zstr(tech_pvt->route_uri), NUTAG_PROXY(tech_pvt->route_uri)),
									   TAG_IF(!zstr_buf(message), SIPTAG_HEADER_STR(message)),
									   TAG_IF(!zstr(tech_pvt->user_via), SIPTAG_VIA_STR(tech_pvt->user_via)), TAG_END());
						} else if ((ua && (switch_stristr("aastra", ua) && !switch_stristr("Intelligate", ua)))) {
							snprintf(message, sizeof(message), "P-Asserted-Identity: \"%s\" <sip:%s@%s>", name, number, tech_pvt->profile->sipip);

							sofia_set_flag_locked(tech_pvt, TFLAG_UPDATING_DISPLAY);
							nua_update(tech_pvt->nh,
									   NUTAG_SESSION_TIMER(tech_pvt->session_timeout),
									   NUTAG_SESSION_REFRESHER(tech_pvt->session_refresher),
									   TAG_IF(call_info, SIPTAG_CALL_INFO_STR(call_info)),
									   TAG_IF(!zstr(tech_pvt->route_uri), NUTAG_PROXY(tech_pvt->route_uri)),
									   TAG_IF(!zstr_buf(message), SIPTAG_HEADER_STR(message)),
									   TAG_IF(!zstr(tech_pvt->user_via), SIPTAG_VIA_STR(tech_pvt->user_via)), TAG_END());
						} else if ((ua && (switch_stristr("cisco/spa50", ua) || switch_stristr("cisco/spa525", ua)))) {
							snprintf(message, sizeof(message), "P-Asserted-Identity: \"%s\" <sip:%s@%s>", name, number, tech_pvt->profile->sipip);

							sofia_set_flag_locked(tech_pvt, TFLAG_UPDATING_DISPLAY);
							nua_update(tech_pvt->nh,
									   NUTAG_SESSION_TIMER(tech_pvt->session_timeout),
									   NUTAG_SESSION_REFRESHER(tech_pvt->session_refresher),
									   TAG_IF(call_info, SIPTAG_CALL_INFO_STR(call_info)),
									   TAG_IF(!zstr(tech_pvt->route_uri), NUTAG_PROXY(tech_pvt->route_uri)),
									   TAG_IF(!zstr_buf(message), SIPTAG_HEADER_STR(message)),
									   TAG_IF(!zstr(tech_pvt->user_via), SIPTAG_VIA_STR(tech_pvt->user_via)), TAG_END());
						} else if ((ua && (switch_stristr("Yealink", ua)))) {
							snprintf(message, sizeof(message), "P-Asserted-Identity: \"%s\" <sip:%s@%s>", name, number, tech_pvt->profile->sipip);

							sofia_set_flag_locked(tech_pvt, TFLAG_UPDATING_DISPLAY);
							nua_update(tech_pvt->nh,
									   NUTAG_SESSION_TIMER(tech_pvt->session_timeout),
									   NUTAG_SESSION_REFRESHER(tech_pvt->session_refresher),
						TAG_IF(call_info, SIPTAG_CALL_INFO_STR(call_info)),
									   TAG_IF(!zstr(tech_pvt->route_uri), NUTAG_PROXY(tech_pvt->route_uri)),
									   TAG_IF(!zstr_buf(message), SIPTAG_HEADER_STR(message)),
									   TAG_IF(!zstr(tech_pvt->user_via), SIPTAG_VIA_STR(tech_pvt->user_via)), TAG_END());
						}

						tech_pvt->last_sent_callee_id_name = switch_core_session_strdup(tech_pvt->session, name);
						tech_pvt->last_sent_callee_id_number = switch_core_session_strdup(tech_pvt->session, number);


						if (switch_event_create(&event, SWITCH_EVENT_CALL_UPDATE) == SWITCH_STATUS_SUCCESS) {
							const char *uuid = switch_channel_get_partner_uuid(channel);
							switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "Direction", "SEND");


							switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "Sent-Callee-ID-Name", name);
							switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "Sent-Callee-ID-Number", number);

							//switch_channel_set_profile_var(channel, "callee_id_name", name);
							//switch_channel_set_profile_var(channel, "callee_id_number", number);

							if (uuid) {
								switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "Bridged-To", uuid);
							}
							switch_channel_event_set_data(channel, event);
							switch_event_fire(&event);
						}
					} else {
						switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "Not sending same id again \"%s\" <%s>\n", name, number);
					}
				}
			}
		}
		break;

	case SWITCH_MESSAGE_INDICATE_HOLD:
		{

			if (msg->numeric_arg) {
				switch_core_media_toggle_hold(session, 1);
			} else {

				sofia_set_flag_locked(tech_pvt, TFLAG_SIP_HOLD);
				switch_channel_set_flag(channel, CF_LEG_HOLDING);
				sofia_glue_do_invite(session);
				if (!zstr(msg->string_arg)) {
					char message[256] = "";
					const char *ua = switch_channel_get_variable(tech_pvt->channel, "sip_user_agent");

					if (ua && switch_stristr("snom", ua)) {
						snprintf(message, sizeof(message), "From:\r\nTo: \"%s\" %s\r\n", msg->string_arg, tech_pvt->caller_profile->destination_number);
						nua_info(tech_pvt->nh, SIPTAG_CONTENT_TYPE_STR("message/sipfrag"),
								 TAG_IF(!zstr(tech_pvt->user_via), SIPTAG_VIA_STR(tech_pvt->user_via)), SIPTAG_PAYLOAD_STR(message), TAG_END());
					} else if (ua && switch_stristr("polycom", ua)) {
						snprintf(message, sizeof(message), "P-Asserted-Identity: \"%s\" <%s>", msg->string_arg, tech_pvt->caller_profile->destination_number);
						nua_update(tech_pvt->nh,
								   NUTAG_SESSION_TIMER(tech_pvt->session_timeout),
								   NUTAG_SESSION_REFRESHER(tech_pvt->session_refresher),
								   TAG_IF(!zstr(tech_pvt->route_uri), NUTAG_PROXY(tech_pvt->route_uri)),
								   TAG_IF(!zstr_buf(message), SIPTAG_HEADER_STR(message)),
								   TAG_IF(!zstr(tech_pvt->user_via), SIPTAG_VIA_STR(tech_pvt->user_via)), TAG_END());
					}
				}
			}
		}
		break;

	case SWITCH_MESSAGE_INDICATE_UNHOLD:
		{
			sofia_clear_flag_locked(tech_pvt, TFLAG_SIP_HOLD);
			switch_channel_clear_flag(channel, CF_LEG_HOLDING);
			sofia_glue_do_invite(session);
		}
		break;
	case SWITCH_MESSAGE_INDICATE_REDIRECT:

#define MAX_REDIR 128

		if (!zstr(msg->string_arg)) {

			if (!switch_channel_test_flag(channel, CF_ANSWERED) && !sofia_test_flag(tech_pvt, TFLAG_BYE)) {
				char *dest = (char *) msg->string_arg;
				char *argv[MAX_REDIR] = { 0 };
				char *mydata = NULL, *newdest = NULL;
				int argc = 0, i;
				switch_size_t len = 0;

				if (strchr(dest, ',')) {
					mydata = switch_core_session_strdup(session, dest);
					len = strlen(mydata) * 2;
					newdest = switch_core_session_alloc(session, len);

					argc = switch_split(mydata, ',', argv);

					for (i = 0; i < argc; i++) {
						if (!strchr(argv[i], '<') && !strchr(argv[i], '>')) {
							if (argc > 1) {
								if (i == argc - 1) {
									switch_snprintf(newdest + strlen(newdest), len - strlen(newdest), "\"unknown\" <%s>;q=%1.3f",
													argv[i], (double)((double)(MAX_REDIR + 1 - i))/1000);
								} else {
									switch_snprintf(newdest + strlen(newdest), len - strlen(newdest), "\"unknown\" <%s>;q=%1.3f,",
													argv[i], (double)((double)(MAX_REDIR + 1 - i))/1000);
								}
							} else {
								if (i == argc - 1) {
									switch_snprintf(newdest + strlen(newdest), len - strlen(newdest), "\"unknown\" <%s>", argv[i]);
								} else {
									switch_snprintf(newdest + strlen(newdest), len - strlen(newdest), "\"unknown\" <%s>,", argv[i]);
								}
							}
						} else {
							if (i == argc - 1) {
								switch_snprintf(newdest + strlen(newdest), len - strlen(newdest), "%s", argv[i]);
							} else {
								switch_snprintf(newdest + strlen(newdest), len - strlen(newdest), "%s,", argv[i]);
							}
						}
					}

					dest = newdest;
				} else {

					if (!strchr(dest, '<') && !strchr(dest, '>')) {
						dest = switch_core_session_sprintf(session, "\"unknown\" <%s>", dest);
					}
				}

				switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "Redirecting to %s\n", dest);

				tech_pvt->respond_dest = dest;

				if (argc > 1) {
					tech_pvt->respond_code = 300;
					tech_pvt->respond_phrase = "Multiple Choices";
				} else {
					tech_pvt->respond_code = 302;
					tech_pvt->respond_phrase = "Moved Temporarily";
				}

				switch_channel_hangup(tech_pvt->channel, sofia_glue_sip_cause_to_freeswitch(tech_pvt->respond_code));

			} else {
				switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_WARNING, "Too late for redirecting, already answered\n");

			}
		}
		break;
	case SWITCH_MESSAGE_INDICATE_DEFLECT:
		{
			char *extra_headers = sofia_glue_get_extra_headers(channel, SOFIA_SIP_HEADER_PREFIX);
			char ref_to[1024] = "";
			const char *var;

			if (!strcasecmp(msg->string_arg, "sip:")) {
				const char *format = strchr(tech_pvt->profile->sipip, ':') ? "sip:%s@[%s]" : "sip:%s@%s";
				switch_snprintf(ref_to, sizeof(ref_to), format, msg->string_arg, tech_pvt->profile->sipip);
			} else {
				switch_set_string(ref_to, msg->string_arg);
			}
			nua_refer(tech_pvt->nh, SIPTAG_REFER_TO_STR(ref_to), SIPTAG_REFERRED_BY_STR(tech_pvt->contact_url),
						TAG_IF(!zstr(extra_headers), SIPTAG_HEADER_STR(extra_headers)),
						TAG_END());
			switch_mutex_unlock(tech_pvt->sofia_mutex);
			sofia_wait_for_reply(tech_pvt, 9999, 10);
			switch_mutex_lock(tech_pvt->sofia_mutex);
			if ((var = switch_channel_get_variable(tech_pvt->channel, "sip_refer_reply"))) {
				msg->string_reply = switch_core_session_strdup(session, var);
			} else {
				msg->string_reply = "no reply";
			}
			switch_channel_hangup(tech_pvt->channel, SWITCH_CAUSE_BLIND_TRANSFER);
			switch_safe_free(extra_headers);
		}
		break;

	case SWITCH_MESSAGE_INDICATE_RESPOND:
		{

			if (msg->numeric_arg || msg->string_arg) {
				int code = msg->numeric_arg;
				const char *reason = NULL;

				if (code > 0) {
					reason = msg->string_arg;
				} else {
					if (!zstr(msg->string_arg)) {
						if ((code = atoi(msg->string_arg))) {
							if ((reason = strchr(msg->string_arg, ' '))) {
								reason++;
							}
						}
					}
				}

				if (!code) {
					code = 488;
				}

				if (!switch_channel_test_flag(channel, CF_ANSWERED) && code >= 300) {
					if (sofia_test_flag(tech_pvt, TFLAG_BYE)) {
						goto end_lock;
					}
				}

				if (zstr(reason) && code != 407 && code != 302) {
					reason = sip_status_phrase(code);
					if (zstr(reason)) {
						reason = "Because";
					}
				}

				if (code == 302 && !zstr(msg->string_arg)) {
					char *p;

					if ((p = strchr(msg->string_arg, ' '))) {
						*p = '\0';
						msg->string_arg = p;
					}

					msg->message_id = SWITCH_MESSAGE_INDICATE_REDIRECT;
					switch_core_session_receive_message(session, msg);
					goto end_lock;
				} else {
					if (!sofia_test_flag(tech_pvt, TFLAG_BYE)) {
						char *extra_headers = sofia_glue_get_extra_headers(channel, SOFIA_SIP_PROGRESS_HEADER_PREFIX);
						char *sdp = (char *) msg->pointer_arg;

						switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "Responding with %d [%s]\n", code, reason);
						sofia_clear_flag(tech_pvt, TFLAG_REINVITED);

						if (!zstr((sdp))) {
							if (!strcasecmp(sdp, "t38")) {
								switch_t38_options_t *t38_options = switch_channel_get_private(tech_pvt->channel, "t38_options");
								if (t38_options) {
									switch_core_media_set_udptl_image_sdp(tech_pvt->session, t38_options, 0);
									if (switch_core_media_ready(tech_pvt->session, SWITCH_MEDIA_TYPE_AUDIO)) {
										switch_channel_clear_flag(tech_pvt->channel, CF_NOTIMER_DURING_BRIDGE);
										switch_core_media_udptl_mode(tech_pvt->session, SWITCH_MEDIA_TYPE_AUDIO);
									}
								}
							} else {
								switch_core_media_set_local_sdp(session, sdp, SWITCH_TRUE);
							}

							if (switch_channel_test_flag(channel, CF_PROXY_MEDIA)) {
								switch_core_media_patch_sdp(tech_pvt->session);
								switch_core_media_proxy_remote_addr(session, NULL);
							}
							if (sofia_use_soa(tech_pvt)) {
								nua_respond(tech_pvt->nh, code, su_strdup(nua_handle_home(tech_pvt->nh), reason), SIPTAG_CONTACT_STR(tech_pvt->reply_contact),
											SOATAG_USER_SDP_STR(tech_pvt->mparams.local_sdp_str),
											SOATAG_REUSE_REJECTED(1),
											SOATAG_AUDIO_AUX("cn telephone-event"), NUTAG_INCLUDE_EXTRA_SDP(1),
											TAG_IF(!zstr(extra_headers), SIPTAG_HEADER_STR(extra_headers)), TAG_END());
							} else {
								nua_respond(tech_pvt->nh, code, su_strdup(nua_handle_home(tech_pvt->nh), reason), SIPTAG_CONTACT_STR(tech_pvt->reply_contact),
											NUTAG_MEDIA_ENABLE(0),
											SIPTAG_CONTENT_TYPE_STR("application/sdp"),
											SIPTAG_PAYLOAD_STR(tech_pvt->mparams.local_sdp_str),
											TAG_IF(!zstr(extra_headers), SIPTAG_HEADER_STR(extra_headers)), TAG_END());
							}
							if (sofia_test_pflag(tech_pvt->profile, PFLAG_3PCC_PROXY) && sofia_test_flag(tech_pvt, TFLAG_3PCC)) {
								/* Unlock the session signal to allow the ack to make it in */
								// Maybe we should timeout?
								switch_mutex_unlock(tech_pvt->sofia_mutex);

								while (switch_channel_ready(channel) && !sofia_test_flag(tech_pvt, TFLAG_3PCC_HAS_ACK)) {
									switch_cond_next();
								}

								/*  Regain lock on sofia */
								switch_mutex_lock(tech_pvt->sofia_mutex);

								switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "3PCC-PROXY, Done waiting for ACK\n");
								sofia_clear_flag(tech_pvt, TFLAG_3PCC);
								sofia_clear_flag(tech_pvt, TFLAG_3PCC_HAS_ACK);
								switch_core_session_pass_indication(session, SWITCH_MESSAGE_INDICATE_ANSWER);
							}
						} else {
							if (msg->numeric_arg && !(switch_channel_test_flag(channel, CF_ANSWERED) && code == 488)) {
								if (code > 399) {
									switch_call_cause_t cause = sofia_glue_sip_cause_to_freeswitch(code);
									if (code == 401 || cause == 407) cause = SWITCH_CAUSE_USER_CHALLENGE;

									tech_pvt->respond_code = code;
									tech_pvt->respond_phrase = switch_core_session_strdup(tech_pvt->session, reason);
									switch_channel_hangup(tech_pvt->channel, cause);
								} else {
									switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_WARNING, "Cannot respond.\n");
								}
							} else {
								nua_respond(tech_pvt->nh, code, su_strdup(nua_handle_home(tech_pvt->nh), reason), SIPTAG_CONTACT_STR(tech_pvt->reply_contact),
											TAG_IF(!zstr(extra_headers), SIPTAG_HEADER_STR(extra_headers)), TAG_END());
							}

						}
						switch_safe_free(extra_headers);
					}
				}

			}
		}
		break;
	case SWITCH_MESSAGE_INDICATE_RINGING:
		{
			switch_ring_ready_t ring_ready_val = msg->numeric_arg;

			if (!switch_channel_test_flag(channel, CF_RING_READY) && !sofia_test_flag(tech_pvt, TFLAG_BYE) &&
				!switch_channel_test_flag(channel, CF_EARLY_MEDIA) && !switch_channel_test_flag(channel, CF_ANSWERED)) {
				char *extra_header = sofia_glue_get_extra_headers(channel, SOFIA_SIP_PROGRESS_HEADER_PREFIX);
				const char *call_info = switch_channel_get_variable(channel, "presence_call_info_full");
				char *cid = generate_pai_str(tech_pvt);

				/* Set sip_to_tag to local tag for inbound channels. */
				if (switch_channel_direction(channel) == SWITCH_CALL_DIRECTION_INBOUND) {
					const char* to_tag = "";
					to_tag = switch_str_nil(nta_leg_get_tag(tech_pvt->nh->nh_ds->ds_leg));
					if(to_tag) {
						switch_channel_set_variable(channel, "sip_to_tag", to_tag);
					}
				}

				switch (ring_ready_val) {

				case SWITCH_RING_READY_QUEUED:

					nua_respond(tech_pvt->nh, SIP_182_QUEUED,
								SIPTAG_CONTACT_STR(tech_pvt->reply_contact),
								TAG_IF(cid, SIPTAG_HEADER_STR(cid)),
								TAG_IF(call_info, SIPTAG_CALL_INFO_STR(call_info)),
								TAG_IF(!zstr(extra_header), SIPTAG_HEADER_STR(extra_header)),
								TAG_IF(switch_stristr("update_display", tech_pvt->x_freeswitch_support_remote),
									   SIPTAG_HEADER_STR("X-FS-Support: " FREESWITCH_SUPPORT)), TAG_END());
					break;

				case SWITCH_RING_READY_RINGING:
				default:

					nua_respond(tech_pvt->nh, SIP_180_RINGING,
								SIPTAG_CONTACT_STR(tech_pvt->reply_contact),
								TAG_IF(cid, SIPTAG_HEADER_STR(cid)),
								TAG_IF(call_info, SIPTAG_CALL_INFO_STR(call_info)),
								TAG_IF(!zstr(extra_header), SIPTAG_HEADER_STR(extra_header)),
								TAG_IF(switch_stristr("update_display", tech_pvt->x_freeswitch_support_remote),
									   SIPTAG_HEADER_STR("X-FS-Support: " FREESWITCH_SUPPORT)), TAG_END());

					break;
				}


				switch_safe_free(extra_header);
				switch_channel_mark_ring_ready(channel);
			}
		}
		break;
	case SWITCH_MESSAGE_INDICATE_ANSWER:
		status = sofia_answer_channel(session);
		break;
	case SWITCH_MESSAGE_INDICATE_PROGRESS:
		{
			char *sticky = NULL;
			const char *val = NULL;
			const char *call_info = switch_channel_get_variable(channel, "presence_call_info_full");
			const char *b_sdp = NULL;
			int is_proxy = 0, is_3pcc = 0;

			b_sdp = switch_channel_get_variable(channel, SWITCH_B_SDP_VARIABLE);
			is_proxy = (switch_channel_test_flag(channel, CF_PROXY_MODE) || switch_channel_test_flag(channel, CF_PROXY_MEDIA));
			is_3pcc = (sofia_test_pflag(tech_pvt->profile, PFLAG_3PCC_PROXY) && sofia_test_flag(tech_pvt, TFLAG_3PCC));

			if (b_sdp && is_proxy && !is_3pcc) {
				switch_core_media_set_local_sdp(session, b_sdp, SWITCH_TRUE);

				if (switch_channel_test_flag(channel, CF_PROXY_MEDIA)) {
					switch_core_media_patch_sdp(tech_pvt->session);
					if (sofia_media_activate_rtp(tech_pvt) != SWITCH_STATUS_SUCCESS) {
						status = SWITCH_STATUS_FALSE;
						goto end_lock;
					}
				}
			} else {
				if (is_3pcc) {
					switch_channel_set_flag(channel, CF_3PCC);

					if(!is_proxy) {
						switch_core_media_prepare_codecs(tech_pvt->session, SWITCH_TRUE);
						tech_pvt->mparams.local_sdp_str = NULL;

						switch_core_media_choose_port(tech_pvt->session, SWITCH_MEDIA_TYPE_AUDIO, 0);
						switch_core_session_set_ice(session);
						switch_core_media_gen_local_sdp(session, SDP_TYPE_REQUEST, NULL            ,    0, NULL, 0);
					} else {
						switch_core_media_set_local_sdp(session, b_sdp, SWITCH_TRUE);

						if (switch_channel_test_flag(channel, CF_PROXY_MEDIA)) {
							switch_core_media_patch_sdp(tech_pvt->session);
							if (sofia_media_activate_rtp(tech_pvt) != SWITCH_STATUS_SUCCESS) {
								switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "failed to activate rtp\n");
								status = SWITCH_STATUS_FALSE;
								goto end_lock;
							}
						}
					}
					/* Send the 183 */
					if (!sofia_test_flag(tech_pvt, TFLAG_BYE)) {
						char *extra_headers = sofia_glue_get_extra_headers(channel, SOFIA_SIP_PROGRESS_HEADER_PREFIX);
						if (sofia_use_soa(tech_pvt)) {

							nua_respond(tech_pvt->nh, SIP_183_SESSION_PROGRESS,
										TAG_IF(is_proxy, NUTAG_AUTOANSWER(0)),
										SIPTAG_CONTACT_STR(tech_pvt->profile->url),
										SOATAG_USER_SDP_STR(tech_pvt->mparams.local_sdp_str),
										TAG_IF(call_info, SIPTAG_CALL_INFO_STR(call_info)),
										SOATAG_REUSE_REJECTED(1),
										SOATAG_RTP_SELECT(1),
										SOATAG_AUDIO_AUX("cn telephone-event"), NUTAG_INCLUDE_EXTRA_SDP(1),
										TAG_IF(!zstr(extra_headers), SIPTAG_HEADER_STR(extra_headers)),
										TAG_IF(switch_stristr("update_display", tech_pvt->x_freeswitch_support_remote),
											   SIPTAG_HEADER_STR("X-FS-Support: " FREESWITCH_SUPPORT)), TAG_END());
						} else {
							nua_respond(tech_pvt->nh, SIP_183_SESSION_PROGRESS,
										NUTAG_MEDIA_ENABLE(0),
										SIPTAG_CONTACT_STR(tech_pvt->profile->url),
										TAG_IF(!zstr(extra_headers), SIPTAG_HEADER_STR(extra_headers)),
										TAG_IF(call_info, SIPTAG_CALL_INFO_STR(call_info)),
										SIPTAG_CONTENT_TYPE_STR("application/sdp"),
										SIPTAG_PAYLOAD_STR(tech_pvt->mparams.local_sdp_str),
										TAG_IF(switch_stristr("update_display", tech_pvt->x_freeswitch_support_remote),
											   SIPTAG_HEADER_STR("X-FS-Support: " FREESWITCH_SUPPORT)), TAG_END());
						}

						switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "3PCC-PROXY, Sent a 183 SESSION PROGRESS, waiting for PRACK\n");
						switch_safe_free(extra_headers);
					}

					/* Unlock the session signal to allow the ack to make it in */
					// Maybe we should timeout?
					switch_mutex_unlock(tech_pvt->sofia_mutex);

					while (switch_channel_ready(channel) && !sofia_test_flag(tech_pvt, TFLAG_3PCC_HAS_ACK)) {
						switch_cond_next();
					}

					/*  Regain lock on sofia */
					switch_mutex_lock(tech_pvt->sofia_mutex);

					if(is_proxy || !switch_channel_get_variable(channel, SWITCH_R_SDP_VARIABLE)) {
						sofia_clear_flag(tech_pvt, TFLAG_3PCC_HAS_ACK);
						sofia_clear_flag(tech_pvt, TFLAG_3PCC);
						// This sends the message to the other leg that causes it to call the TFLAG_3PCC_INVITE code at the start of this function.
						// Is there another message it would be better to hang this on though?
						switch_core_session_pass_indication(session, SWITCH_MESSAGE_INDICATE_ANSWER);
					}

					switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "3PCC-PROXY, Done waiting for PRACK\n");
					status = SWITCH_STATUS_SUCCESS;
					goto end_lock;
				}
			}

			if (!sofia_test_flag(tech_pvt, TFLAG_ANS) && !sofia_test_flag(tech_pvt, TFLAG_EARLY_MEDIA)) {

				sofia_set_flag_locked(tech_pvt, TFLAG_EARLY_MEDIA);
				switch_log_printf(SWITCH_CHANNEL_ID_SESSION, msg->_file, msg->_func, msg->_line,
								  (const char*)session, SWITCH_LOG_INFO, "Sending early media\n");

				/* Transmit 183 Progress with SDP */
				if (switch_channel_test_flag(channel, CF_PROXY_MODE) || switch_channel_test_flag(channel, CF_PROXY_MEDIA)) {
					const char *sdp = NULL;
					if ((sdp = switch_channel_get_variable(channel, SWITCH_B_SDP_VARIABLE))) {
						switch_core_media_set_local_sdp(session, sdp, SWITCH_TRUE);
					}
					if (switch_channel_test_flag(channel, CF_PROXY_MEDIA)) {

						switch_core_media_patch_sdp(tech_pvt->session);

						if (sofia_media_activate_rtp(tech_pvt) != SWITCH_STATUS_SUCCESS) {
							status = SWITCH_STATUS_FALSE;
							goto end_lock;
						}
					}
				} else {
					if (sofia_test_flag(tech_pvt, TFLAG_LATE_NEGOTIATION) ||
						switch_core_media_codec_chosen(tech_pvt->session, SWITCH_MEDIA_TYPE_AUDIO) != SWITCH_STATUS_SUCCESS) {
						sofia_clear_flag_locked(tech_pvt, TFLAG_LATE_NEGOTIATION);
						if (switch_channel_direction(tech_pvt->channel) == SWITCH_CALL_DIRECTION_INBOUND) {
							const char *r_sdp = switch_channel_get_variable(channel, SWITCH_R_SDP_VARIABLE);


							switch_core_media_prepare_codecs(tech_pvt->session, SWITCH_TRUE);
							if (zstr(r_sdp) || sofia_media_tech_media(tech_pvt, r_sdp) != SWITCH_STATUS_SUCCESS) {
								switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR,
												  "CODEC NEGOTIATION ERROR.  SDP:\n%s\n", r_sdp ? r_sdp : "NO SDP!");
								switch_channel_set_variable(channel, SWITCH_ENDPOINT_DISPOSITION_VARIABLE, "CODEC NEGOTIATION ERROR");
								//nua_respond(tech_pvt->nh, SIP_488_NOT_ACCEPTABLE, TAG_END());
								status = SWITCH_STATUS_FALSE;
								goto end_lock;
							}
						}
					}

					switch_channel_check_zrtp(tech_pvt->channel);

					if ((status = switch_core_media_choose_port(tech_pvt->session, SWITCH_MEDIA_TYPE_AUDIO, 0)) != SWITCH_STATUS_SUCCESS) {
						switch_channel_hangup(channel, SWITCH_CAUSE_DESTINATION_OUT_OF_ORDER);
						goto end_lock;
					}
					switch_core_media_gen_local_sdp(session, SDP_TYPE_RESPONSE, NULL, 0, NULL, 0);
					if (sofia_media_activate_rtp(tech_pvt) != SWITCH_STATUS_SUCCESS) {
						switch_channel_hangup(channel, SWITCH_CAUSE_DESTINATION_OUT_OF_ORDER);
					}
					if (tech_pvt->mparams.local_sdp_str) {
						switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "Ring SDP:\n%s\n", tech_pvt->mparams.local_sdp_str);
					}
				}
				switch_channel_mark_pre_answered(channel);


				if (sofia_test_flag(tech_pvt, TFLAG_NAT) ||
					(val = switch_channel_get_variable(channel, "sip-force-contact")) ||
					((val = switch_channel_get_variable(channel, "sip_sticky_contact")) && switch_true(val))) {
					sticky = tech_pvt->record_route;
					switch_channel_set_variable(channel, "sip_nat_detected", "true");
				}

				if (!sofia_test_flag(tech_pvt, TFLAG_BYE)) {
					char *extra_header = sofia_glue_get_extra_headers(channel, SOFIA_SIP_PROGRESS_HEADER_PREFIX);
					char *cid = NULL;

					cid = generate_pai_str(tech_pvt);


					if (switch_channel_test_flag(tech_pvt->channel, CF_PROXY_MODE) &&
						tech_pvt->mparams.early_sdp && strcmp(tech_pvt->mparams.early_sdp, tech_pvt->mparams.local_sdp_str)) {
						/* The SIP RFC for SOA forbids sending a 183 with one sdp then a 200 with another but it won't do us much good unless
						   we do so in this case we will abandon the SOA rules and go rogue.
						 */
						sofia_clear_flag(tech_pvt, TFLAG_ENABLE_SOA);
					}

					tech_pvt->mparams.early_sdp = switch_core_session_strdup(tech_pvt->session, tech_pvt->mparams.local_sdp_str);

					if (sofia_use_soa(tech_pvt)) {
						nua_respond(tech_pvt->nh,
									SIP_183_SESSION_PROGRESS,
									NUTAG_AUTOANSWER(0),
									TAG_IF(sticky, NUTAG_PROXY(tech_pvt->record_route)),
									TAG_IF(cid, SIPTAG_HEADER_STR(cid)),
									SIPTAG_CONTACT_STR(tech_pvt->reply_contact),
									SOATAG_REUSE_REJECTED(1),
									SOATAG_RTP_SELECT(1),
									SOATAG_ADDRESS(tech_pvt->mparams.adv_sdp_audio_ip),
									SOATAG_USER_SDP_STR(tech_pvt->mparams.local_sdp_str), SOATAG_AUDIO_AUX("cn telephone-event"),
									TAG_IF(call_info, SIPTAG_CALL_INFO_STR(call_info)),
									TAG_IF(!zstr(extra_header), SIPTAG_HEADER_STR(extra_header)),
									TAG_IF(switch_stristr("update_display", tech_pvt->x_freeswitch_support_remote),
										   SIPTAG_HEADER_STR("X-FS-Support: " FREESWITCH_SUPPORT)), TAG_END());
					} else {
						nua_respond(tech_pvt->nh,
									SIP_183_SESSION_PROGRESS,
									NUTAG_AUTOANSWER(0),
									NUTAG_MEDIA_ENABLE(0),
									TAG_IF(sticky, NUTAG_PROXY(tech_pvt->record_route)),
									TAG_IF(cid, SIPTAG_HEADER_STR(cid)),
									SIPTAG_CONTACT_STR(tech_pvt->reply_contact),
									SIPTAG_CONTENT_TYPE_STR("application/sdp"),
									SIPTAG_PAYLOAD_STR(tech_pvt->mparams.local_sdp_str),
									TAG_IF(call_info, SIPTAG_CALL_INFO_STR(call_info)),
									TAG_IF(!zstr(extra_header), SIPTAG_HEADER_STR(extra_header)),
									TAG_IF(switch_stristr("update_display", tech_pvt->x_freeswitch_support_remote),
										   SIPTAG_HEADER_STR("X-FS-Support: " FREESWITCH_SUPPORT)), TAG_END());
					}
					switch_safe_free(extra_header);
				}
			}
		}
		break;

	case SWITCH_MESSAGE_INDICATE_UDPTL_MODE:
		{
			switch_t38_options_t *t38_options = switch_channel_get_private(channel, "t38_options");

			if (!t38_options) {
				nua_respond(tech_pvt->nh, SIP_488_NOT_ACCEPTABLE, TAG_END());
			}
		}
		break;

	default:
		break;
	}

  end_lock:

	//if (msg->message_id == SWITCH_MESSAGE_INDICATE_ANSWER || msg->message_id == SWITCH_MESSAGE_INDICATE_PROGRESS) {
	//sofia_send_callee_id(session, NULL, NULL);
	//}

	switch_mutex_unlock(tech_pvt->sofia_mutex);

  end:

	if (switch_channel_down(channel) || !tech_pvt || sofia_test_flag(tech_pvt, TFLAG_BYE)) {
		status = SWITCH_STATUS_FALSE;
	}

	return status;

}

static switch_status_t sofia_receive_event(switch_core_session_t *session, switch_event_t *event)
{
	struct private_object *tech_pvt = switch_core_session_get_private(session);
	char *body;
	nua_handle_t *msg_nh;

	switch_assert(tech_pvt != NULL);

	if (!(body = switch_event_get_body(event))) {
		body = "";
	}

	if (tech_pvt->hash_key) {
		switch_mutex_lock(tech_pvt->sofia_mutex);
		msg_nh = nua_handle(tech_pvt->profile->nua, NULL,
							SIPTAG_FROM_STR(tech_pvt->chat_from),
							NUTAG_URL(tech_pvt->chat_to), SIPTAG_TO_STR(tech_pvt->chat_to), TAG_END());
		nua_handle_bind(msg_nh, &mod_sofia_globals.destroy_private);
		nua_message(msg_nh, SIPTAG_CONTENT_TYPE_STR("text/html"), SIPTAG_PAYLOAD_STR(body), TAG_END());
		switch_mutex_unlock(tech_pvt->sofia_mutex);
	}

	return SWITCH_STATUS_SUCCESS;
}

typedef switch_status_t (*sofia_command_t) (char **argv, int argc, switch_stream_handle_t *stream);

static const char *sofia_state_names[] = {
	"UNREGED",
	"TRYING",
	"REGISTER",
	"REGED",
	"UNREGISTER",
	"FAILED",
	"FAIL_WAIT",
	"EXPIRED",
	"NOREG",
	"TIMEOUT",
	NULL
};

const char *sofia_state_string(int state)
{
	if (state >= REG_STATE_LAST) return "";

	return sofia_state_names[state];
}

struct cb_helper_sql2str {
	char *buf;
	size_t len;
	int matches;
};

struct cb_helper {
	uint32_t row_process;
	sofia_profile_t *profile;
	switch_stream_handle_t *stream;
	switch_bool_t dedup;
};



static int show_reg_callback(void *pArg, int argc, char **argv, char **columnNames)
{
	struct cb_helper *cb = (struct cb_helper *) pArg;
	char exp_buf[128] = "";
	int exp_secs = 0;
	switch_time_exp_t tm;

	cb->row_process++;

	if (argv[6]) {
		time_t now = switch_epoch_time_now(NULL);
		switch_time_t etime = atoi(argv[6]);
		switch_size_t retsize;

		exp_secs = (int)(etime - now);
		switch_time_exp_lt(&tm, switch_time_from_sec(etime));
		switch_strftime_nocheck(exp_buf, &retsize, sizeof(exp_buf), "%Y-%m-%d %T", &tm);
	}

	cb->stream->write_function(cb->stream,
							   "Call-ID:    \t%s\n"
							   "User:       \t%s@%s\n"
							   "Contact:    \t%s\n"
							   "Agent:      \t%s\n"
							   "Status:     \t%s(%s) EXP(%s) EXPSECS(%d)\n"
							   "Ping-Status:\t%s\n"
							   "Host:       \t%s\n"
							   "IP:         \t%s\n"
							   "Port:       \t%s\n"
							   "Auth-User:  \t%s\n"
							   "Auth-Realm: \t%s\n"
							   "MWI-Account:\t%s@%s\n\n",
							   switch_str_nil(argv[0]), switch_str_nil(argv[1]), switch_str_nil(argv[2]), switch_str_nil(argv[3]),
							   switch_str_nil(argv[7]), switch_str_nil(argv[4]), switch_str_nil(argv[5]), exp_buf, exp_secs, switch_str_nil(argv[18]),
							   switch_str_nil(argv[11]), switch_str_nil(argv[12]), switch_str_nil(argv[13]), switch_str_nil(argv[14]),
							   switch_str_nil(argv[15]), switch_str_nil(argv[16]), switch_str_nil(argv[17]));
	return 0;
}

static int show_reg_callback_xml(void *pArg, int argc, char **argv, char **columnNames)
{
	struct cb_helper *cb = (struct cb_helper *) pArg;
	char exp_buf[128] = "";
	switch_time_exp_t tm;
	const int buflen = 2048;
	char xmlbuf[2048];
	int exp_secs = 0;

	cb->row_process++;

	if (argv[6]) {
		time_t now = switch_epoch_time_now(NULL);
		switch_time_t etime = atoi(argv[6]);
		switch_size_t retsize;

		exp_secs = (int)(etime - now);
		switch_time_exp_lt(&tm, switch_time_from_sec(etime));
		switch_strftime_nocheck(exp_buf, &retsize, sizeof(exp_buf), "%Y-%m-%d %T", &tm);
	}

	cb->stream->write_function(cb->stream, "    <registration>\n");
	cb->stream->write_function(cb->stream, "        <call-id>%s</call-id>\n", switch_str_nil(argv[0]));
	cb->stream->write_function(cb->stream, "        <user>%s@%s</user>\n", switch_str_nil(argv[1]), switch_str_nil(argv[2]));
	cb->stream->write_function(cb->stream, "        <contact>%s</contact>\n", switch_amp_encode(switch_str_nil(argv[3]), xmlbuf, buflen));
	cb->stream->write_function(cb->stream, "        <agent>%s</agent>\n", switch_amp_encode(switch_str_nil(argv[7]), xmlbuf, buflen));
	cb->stream->write_function(cb->stream, "        <status>%s(%s) exp(%s) expsecs(%d)</status>\n", switch_str_nil(argv[4]), switch_str_nil(argv[5]),
							   exp_buf, exp_secs);
	cb->stream->write_function(cb->stream, "        <ping-status>%s</ping-status>\n", switch_str_nil(argv[18]));
	cb->stream->write_function(cb->stream, "        <host>%s</host>\n", switch_str_nil(argv[11]));
	cb->stream->write_function(cb->stream, "        <network-ip>%s</network-ip>\n", switch_str_nil(argv[12]));
	cb->stream->write_function(cb->stream, "        <network-port>%s</network-port>\n", switch_str_nil(argv[13]));
	cb->stream->write_function(cb->stream, "        <sip-auth-user>%s</sip-auth-user>\n",
							   switch_url_encode(switch_str_nil(argv[14]), xmlbuf, sizeof(xmlbuf)));
	cb->stream->write_function(cb->stream, "        <sip-auth-realm>%s</sip-auth-realm>\n", switch_str_nil(argv[15]));
	cb->stream->write_function(cb->stream, "        <mwi-account>%s@%s</mwi-account>\n", switch_str_nil(argv[16]), switch_str_nil(argv[17]));
	cb->stream->write_function(cb->stream, "    </registration>\n");

	return 0;
}

static int sql2str_callback(void *pArg, int argc, char **argv, char **columnNames)
{
	struct cb_helper_sql2str *cbt = (struct cb_helper_sql2str *) pArg;

	switch_copy_string(cbt->buf, argv[0], cbt->len);
	cbt->matches++;
	return 0;
}

static uint32_t sofia_profile_reg_count(sofia_profile_t *profile)
{
	struct cb_helper_sql2str cb;
	char reg_count[80] = "";
	char *sql;
	cb.buf = reg_count;
	cb.len = sizeof(reg_count);
	sql = switch_mprintf("select count(*) from sip_registrations where profile_name = '%q'", profile->name);
	sofia_glue_execute_sql_callback(profile, profile->dbh_mutex, sql, sql2str_callback, &cb);
	free(sql);
	return strtoul(reg_count, NULL, 10);
}

static const char *status_names[] = { "DOWN", "UP", NULL };

static switch_status_t cmd_status(char **argv, int argc, switch_stream_handle_t *stream)
{
	sofia_profile_t *profile = NULL;
	sofia_gateway_t *gp;
	switch_hash_index_t *hi;
	void *val;
	const void *vvar;
	int c = 0;
	int ac = 0;
	const char *line = "=================================================================================================";

	if (argc > 0) {
		if (argc == 1) {
			/* show summary of all gateways */

			uint32_t ib_failed = 0;
			uint32_t ib = 0;
			uint32_t ob_failed = 0;
			uint32_t ob = 0;

			stream->write_function(stream, "%25s\t%32s\t%s\t%9s\t%s\t%s\n", "Profile::Gateway-Name", "    Data    ", "State", "Ping Time", "IB Calls(F/T)", "OB Calls(F/T)");
			stream->write_function(stream, "%s\n", line);
			switch_mutex_lock(mod_sofia_globals.hash_mutex);
			for (hi = switch_core_hash_first(mod_sofia_globals.profile_hash); hi; hi = switch_core_hash_next(&hi)) {
				switch_core_hash_this(hi, &vvar, NULL, &val);
				profile = (sofia_profile_t *) val;
				if (sofia_test_pflag(profile, PFLAG_RUNNING)) {

					if (!strcmp(vvar, profile->name)) {	/* not an alias */
						for (gp = profile->gateways; gp; gp = gp->next) {
							char *pkey = switch_mprintf("%s::%s", profile->name, gp->name);

							switch_assert(gp->state < REG_STATE_LAST);

							c++;
							ib_failed += gp->ib_failed_calls;
							ib += gp->ib_calls;
							ob_failed += gp->ob_failed_calls;
							ob += gp->ob_calls;

							stream->write_function(stream, "%25s\t%32s\t%s\t%6.2f\t%u/%u\t%u/%u",
												   pkey, gp->register_to, sofia_state_names[gp->state], gp->ping_time,
												   gp->ib_failed_calls, gp->ib_calls, gp->ob_failed_calls, gp->ob_calls);

							if (gp->state == REG_STATE_FAILED || gp->state == REG_STATE_TRYING) {
								time_t now = switch_epoch_time_now(NULL);
								if (gp->reg_timeout > now) {
									stream->write_function(stream, " (retry: %ds)", gp->reg_timeout - now);
								} else {
									stream->write_function(stream, " (retry: NEVER)");
								}
							}
							stream->write_function(stream, "\n");
						}
					}
				}
			}
			switch_mutex_unlock(mod_sofia_globals.hash_mutex);
			stream->write_function(stream, "%s\n", line);
			stream->write_function(stream, "%d gateway%s: Inbound(Failed/Total): %u/%u,"
								   "Outbound(Failed/Total):%u/%u\n", c, c == 1 ? "" : "s", ib_failed, ib, ob_failed, ob);

			return SWITCH_STATUS_SUCCESS;
		}

		if (!strcasecmp(argv[0], "gateway")) {
			if ((gp = sofia_reg_find_gateway(argv[1]))) {
				switch_assert(gp->state < REG_STATE_LAST);

				stream->write_function(stream, "%s\n", line);
				stream->write_function(stream, "Name    \t%s\n", switch_str_nil(gp->name));
				stream->write_function(stream, "Profile \t%s\n", gp->profile->name);
				stream->write_function(stream, "Scheme  \t%s\n", switch_str_nil(gp->register_scheme));
				stream->write_function(stream, "Realm   \t%s\n", switch_str_nil(gp->register_realm));
				stream->write_function(stream, "Username\t%s\n", switch_str_nil(gp->register_username));
				stream->write_function(stream, "Password\t%s\n", zstr(gp->register_password) ? "no" : "yes");
				stream->write_function(stream, "From    \t%s\n", switch_str_nil(gp->register_from));
				stream->write_function(stream, "Contact \t%s\n", switch_str_nil(gp->register_contact));
				stream->write_function(stream, "Exten   \t%s\n", switch_str_nil(gp->extension));
				stream->write_function(stream, "To      \t%s\n", switch_str_nil(gp->register_to));
				stream->write_function(stream, "Proxy   \t%s\n", switch_str_nil(gp->register_proxy));
				stream->write_function(stream, "Context \t%s\n", switch_str_nil(gp->register_context));
				stream->write_function(stream, "Expires \t%s\n", switch_str_nil(gp->expires_str));
				stream->write_function(stream, "Freq    \t%d\n", gp->freq);
				stream->write_function(stream, "Ping    \t%d\n", gp->ping);
				stream->write_function(stream, "PingFreq\t%d\n", gp->ping_freq);
				stream->write_function(stream, "PingTime\t%0.2f\n", gp->ping_time);
				stream->write_function(stream, "PingState\t%d/%d/%d\n", gp->ping_min, gp->ping_count, gp->ping_max);
				stream->write_function(stream, "State   \t%s\n", sofia_state_names[gp->state]);
				stream->write_function(stream, "Status  \t%s%s\n", status_names[gp->status], gp->pinging ? " (ping)" : "");
				stream->write_function(stream, "Uptime  \t%lds\n", gp->status == SOFIA_GATEWAY_UP ? (switch_time_now()-gp->uptime)/1000000 : 0);
				stream->write_function(stream, "CallsIN \t%u\n", gp->ib_calls);
				stream->write_function(stream, "CallsOUT\t%u\n", gp->ob_calls);
				stream->write_function(stream, "FailedCallsIN\t%u\n", gp->ib_failed_calls);
				stream->write_function(stream, "FailedCallsOUT\t%u\n", gp->ob_failed_calls);
				stream->write_function(stream, "%s\n", line);
				sofia_reg_release_gateway(gp);
			} else {
				stream->write_function(stream, "Invalid Gateway!\n");
			}
		} else if (!strcasecmp(argv[0], "profile")) {
			struct cb_helper cb;
			char *sql = NULL;
			uint32_t x = 0;

			cb.row_process = 0;


			if ((argv[1]) && (profile = sofia_glue_find_profile(argv[1]))) {
				if (!argv[2] || (strcasecmp(argv[2], "reg") && strcasecmp(argv[2], "user"))) {
					stream->write_function(stream, "%s\n", line);
					stream->write_function(stream, "Name             \t%s\n", switch_str_nil(argv[1]));
					stream->write_function(stream, "Domain Name      \t%s\n", profile->domain_name ? profile->domain_name : "N/A");
					if (strcasecmp(argv[1], profile->name)) {
						stream->write_function(stream, "Alias Of         \t%s\n", switch_str_nil(profile->name));
					}
					stream->write_function(stream, "Auto-NAT         \t%s\n", sofia_test_pflag(profile, PFLAG_AUTO_NAT) ? "true" : "false");
					stream->write_function(stream, "DBName           \t%s\n", profile->dbname ? profile->dbname : switch_str_nil(profile->odbc_dsn));
					stream->write_function(stream, "Pres Hosts       \t%s\n", switch_str_nil(profile->presence_hosts));
					stream->write_function(stream, "Dialplan         \t%s\n", switch_str_nil(profile->dialplan));
					stream->write_function(stream, "Context          \t%s\n", switch_str_nil(profile->context));
					stream->write_function(stream, "Challenge Realm  \t%s\n", zstr(profile->challenge_realm) ? "auto_to" : profile->challenge_realm);
					for (x = 0; x < profile->rtpip_index; x++) {
						stream->write_function(stream, "RTP-IP           \t%s\n", switch_str_nil(profile->rtpip[x]));
					}
					if (profile->extrtpip) {
						stream->write_function(stream, "Ext-RTP-IP       \t%s\n", profile->extrtpip);
					}

					stream->write_function(stream, "SIP-IP           \t%s\n", switch_str_nil(profile->sipip));
					if (profile->extsipip) {
						stream->write_function(stream, "Ext-SIP-IP       \t%s\n", profile->extsipip);
					}
					if (! sofia_test_pflag(profile, PFLAG_TLS) || ! profile->tls_only) {
						stream->write_function(stream, "URL              \t%s\n", switch_str_nil(profile->url));
						stream->write_function(stream, "BIND-URL         \t%s\n", switch_str_nil(profile->bindurl));
					}
					if (sofia_test_pflag(profile, PFLAG_TLS)) {
						stream->write_function(stream, "TLS-URL          \t%s\n", switch_str_nil(profile->tls_url));
						stream->write_function(stream, "TLS-BIND-URL     \t%s\n", switch_str_nil(profile->tls_bindurl));
					}
					if (profile->ws_bindurl) {
						stream->write_function(stream, "WS-BIND-URL     \t%s\n", switch_str_nil(profile->ws_bindurl));
					}
					if (profile->wss_bindurl) {
						stream->write_function(stream, "WSS-BIND-URL     \t%s\n", switch_str_nil(profile->wss_bindurl));
					}
					stream->write_function(stream, "HOLD-MUSIC       \t%s\n", zstr(profile->hold_music) ? "N/A" : profile->hold_music);
					stream->write_function(stream, "OUTBOUND-PROXY   \t%s\n", zstr(profile->outbound_proxy) ? "N/A" : profile->outbound_proxy);
					stream->write_function(stream, "CODECS IN        \t%s\n", switch_str_nil(profile->inbound_codec_string));
					stream->write_function(stream, "CODECS OUT       \t%s\n", switch_str_nil(profile->outbound_codec_string));

					stream->write_function(stream, "TEL-EVENT        \t%d\n", profile->te);
					if (profile->dtmf_type == DTMF_2833) {
						stream->write_function(stream, "DTMF-MODE        \trfc2833\n");
					} else if (profile->dtmf_type == DTMF_INFO) {
						stream->write_function(stream, "DTMF-MODE        \tinfo\n");
					} else {
						stream->write_function(stream, "DTMF-MODE        \tnone\n");
					}
					stream->write_function(stream, "CNG              \t%d\n", profile->cng_pt);
					stream->write_function(stream, "SESSION-TO       \t%d\n", profile->session_timeout);
					stream->write_function(stream, "MAX-DIALOG       \t%d\n", profile->max_proceeding);
					stream->write_function(stream, "NOMEDIA          \t%s\n", sofia_test_flag(profile, TFLAG_INB_NOMEDIA) ? "true" : "false");
					stream->write_function(stream, "LATE-NEG         \t%s\n", sofia_test_flag(profile, TFLAG_LATE_NEGOTIATION) ? "true" : "false");
					stream->write_function(stream, "PROXY-MEDIA      \t%s\n", sofia_test_flag(profile, TFLAG_PROXY_MEDIA) ? "true" : "false");
					stream->write_function(stream, "ZRTP-PASSTHRU    \t%s\n", sofia_test_flag(profile, TFLAG_ZRTP_PASSTHRU) ? "true" : "false");
					stream->write_function(stream, "AGGRESSIVENAT    \t%s\n",
										   sofia_test_pflag(profile, PFLAG_AGGRESSIVE_NAT_DETECTION) ? "true" : "false");
					if (profile->user_agent_filter) {
						stream->write_function(stream, "USER-AGENT-FILTER\t%s\n", switch_str_nil(profile->user_agent_filter));
					}
					if (profile->max_registrations_perext > 0) {
						stream->write_function(stream, "MAX-REG-PEREXT   \t%d\n", profile->max_registrations_perext);
					}
					stream->write_function(stream, "CALLS-IN         \t%u\n", profile->ib_calls);
					stream->write_function(stream, "FAILED-CALLS-IN  \t%u\n", profile->ib_failed_calls);
					stream->write_function(stream, "CALLS-OUT        \t%u\n", profile->ob_calls);
					stream->write_function(stream, "FAILED-CALLS-OUT \t%u\n", profile->ob_failed_calls);
					stream->write_function(stream, "REGISTRATIONS    \t%lu\n", sofia_profile_reg_count(profile));
				}

				cb.profile = profile;
				cb.stream = stream;

				if (!sql && argv[2] && !strcasecmp(argv[2], "pres") && argv[3]) {
					sql = switch_mprintf("select call_id,sip_user,sip_host,contact,status,"
										 "rpid,expires,user_agent,server_user,server_host,profile_name,hostname,"
										 "network_ip,network_port,sip_username,sip_realm,mwi_user,mwi_host,ping_status"
										 " from sip_registrations where profile_name='%q' and presence_hosts like '%%%q%%'", profile->name, argv[3]);
				}
				if (!sql && argv[2] && !strcasecmp(argv[2], "reg") && argv[3]) {
					sql = switch_mprintf("select call_id,sip_user,sip_host,contact,status,"
										 "rpid,expires,user_agent,server_user,server_host,profile_name,hostname,"
										 "network_ip,network_port,sip_username,sip_realm,mwi_user,mwi_host, ping_status"
										 " from sip_registrations where profile_name='%q' and contact like '%%%q%%'", profile->name, argv[3]);
				}
				if (!sql && argv[2] && !strcasecmp(argv[2], "reg")) {
					sql = switch_mprintf("select call_id,sip_user,sip_host,contact,status,"
										 "rpid,expires,user_agent,server_user,server_host,profile_name,hostname,"
										 "network_ip,network_port,sip_username,sip_realm,mwi_user,mwi_host,ping_status"
										 " from sip_registrations where profile_name='%q'", profile->name);
				}
				if (!sql && argv[2] && !strcasecmp(argv[2], "user") && argv[3]) {
					char *dup = strdup(argv[3]);
					char *host = NULL, *user = NULL;
					char *sqlextra = NULL;

					switch_assert(dup);

					if ((host = strchr(dup, '@'))) {
						*host++ = '\0';
						user = dup;
					} else {
						host = dup;
					}

					if (zstr(user)) {
						sqlextra = switch_mprintf("(sip_host='%q')", host);
					} else if (zstr(host)) {
						sqlextra = switch_mprintf("(sip_user='%q')", user);
					} else {
						sqlextra = switch_mprintf("(sip_user='%q' and sip_host='%q')", user, host);
					}

					sql = switch_mprintf("select call_id,sip_user,sip_host,contact,status,"
										 "rpid,expires,user_agent,server_user,server_host,profile_name,hostname,"
										 "network_ip,network_port,sip_username,sip_realm,mwi_user,mwi_host,ping_status"
										 " from sip_registrations where profile_name='%q' and %s", profile->name, sqlextra);
					switch_safe_free(dup);
					switch_safe_free(sqlextra);
				}

				if (sql) {
					stream->write_function(stream, "\nRegistrations:\n%s\n", line);

					sofia_glue_execute_sql_callback(profile, profile->dbh_mutex, sql, show_reg_callback, &cb);
					switch_safe_free(sql);

					stream->write_function(stream, "Total items returned: %d\n", cb.row_process);
					stream->write_function(stream, "%s\n", line);
				}

				sofia_glue_release_profile(profile);

			} else {
				stream->write_function(stream, "Invalid Profile!\n");
			}
		} else {
			stream->write_function(stream, "Invalid Syntax!\n");
		}

		return SWITCH_STATUS_SUCCESS;
	}

	stream->write_function(stream, "%25s\t%s\t  %40s\t%s\n", "Name", "   Type", "Data", "State");
	stream->write_function(stream, "%s\n", line);
	switch_mutex_lock(mod_sofia_globals.hash_mutex);
	for (hi = switch_core_hash_first(mod_sofia_globals.profile_hash); hi; hi = switch_core_hash_next(&hi)) {
		switch_core_hash_this(hi, &vvar, NULL, &val);
		profile = (sofia_profile_t *) val;
		if (sofia_test_pflag(profile, PFLAG_RUNNING)) {

			if (strcmp(vvar, profile->name)) {
				ac++;
				stream->write_function(stream, "%25s\t%s\t  %40s\t%s\n", vvar, "  alias", profile->name, "ALIASED");
			} else {
				if (! sofia_test_pflag(profile, PFLAG_TLS) || ! profile->tls_only) {
					stream->write_function(stream, "%25s\t%s\t  %40s\t%s (%u)\n", profile->name, "profile", profile->url,
									   sofia_test_pflag(profile, PFLAG_RUNNING) ? "RUNNING" : "DOWN", profile->inuse);
				}

				if (sofia_test_pflag(profile, PFLAG_TLS)) {
					stream->write_function(stream, "%25s\t%s\t  %40s\t%s (%u) (TLS)\n", profile->name, "profile", profile->tls_url,
										   sofia_test_pflag(profile, PFLAG_RUNNING) ? "RUNNING" : "DOWN", profile->inuse);
				}

				c++;

				for (gp = profile->gateways; gp; gp = gp->next) {
					char *pkey = switch_mprintf("%s::%s", profile->name, gp->name);

					switch_assert(gp->state < REG_STATE_LAST);

					stream->write_function(stream, "%25s\t%s\t  %40s\t%s", pkey, "gateway", gp->register_to, sofia_state_names[gp->state]);
					free(pkey);

					if (gp->state == REG_STATE_FAILED || gp->state == REG_STATE_TRYING) {
						time_t now = switch_epoch_time_now(NULL);
						if (gp->retry > now) {
							stream->write_function(stream, " (retry: %ds)", gp->retry - now);
						} else {
							stream->write_function(stream, " (retry: NEVER)");
						}
					}
					stream->write_function(stream, "\n");
				}
			}
		}
	}
	switch_mutex_unlock(mod_sofia_globals.hash_mutex);
	stream->write_function(stream, "%s\n", line);
	stream->write_function(stream, "%d profile%s %d alias%s\n", c, c == 1 ? "" : "s", ac, ac == 1 ? "" : "es");
	return SWITCH_STATUS_SUCCESS;
}

static void xml_gateway_status(sofia_gateway_t *gp, switch_stream_handle_t *stream)
{
	char xmlbuf[2096];
	const int buflen = 2096;

	stream->write_function(stream, "  <gateway>\n");
	stream->write_function(stream, "    <name>%s</name>\n", switch_str_nil(gp->name));
	stream->write_function(stream, "    <profile>%s</profile>\n", gp->profile->name);
	stream->write_function(stream, "    <scheme>%s</scheme>\n", switch_str_nil(gp->register_scheme));
	stream->write_function(stream, "    <realm>%s</realm>\n", switch_str_nil(gp->register_realm));
	stream->write_function(stream, "    <username>%s</username>\n", switch_str_nil(gp->register_username));
	stream->write_function(stream, "    <password>%s</password>\n", zstr(gp->register_password) ? "no" : "yes");
	stream->write_function(stream, "    <from>%s</from>\n", switch_amp_encode(switch_str_nil(gp->register_from), xmlbuf, buflen));
	stream->write_function(stream, "    <contact>%s</contact>\n", switch_amp_encode(switch_str_nil(gp->register_contact), xmlbuf, buflen));
	stream->write_function(stream, "    <exten>%s</exten>\n", switch_amp_encode(switch_str_nil(gp->extension), xmlbuf, buflen));
	stream->write_function(stream, "    <to>%s</to>\n", switch_str_nil(gp->register_to));
	stream->write_function(stream, "    <proxy>%s</proxy>\n", switch_str_nil(gp->register_proxy));
	stream->write_function(stream, "    <context>%s</context>\n", switch_str_nil(gp->register_context));
	stream->write_function(stream, "    <expires>%s</expires>\n", switch_str_nil(gp->expires_str));
	stream->write_function(stream, "    <freq>%d</freq>\n", gp->freq);
	stream->write_function(stream, "    <ping>%d</ping>\n", gp->ping);
	stream->write_function(stream, "    <pingfreq>%d</pingfreq>\n", gp->ping_freq);
	stream->write_function(stream, "    <pingmin>%d</pingmin>\n", gp->ping_min);
	stream->write_function(stream, "    <pingcount>%d</pingcount>\n", gp->ping_count);	
	stream->write_function(stream, "    <pingmax>%d</pingmax>\n", gp->ping_max);
	stream->write_function(stream, "    <pingtime>%0.2f</pingtime>\n", gp->ping_time);
	stream->write_function(stream, "    <pinging>%d</pinging>\n", gp->pinging);
	stream->write_function(stream, "    <state>%s</state>\n", sofia_state_names[gp->state]);
	stream->write_function(stream, "    <status>%s</status>\n", status_names[gp->status]);
	stream->write_function(stream, "    <uptime-usec>%ld</uptime-usec>\n", gp->status == SOFIA_GATEWAY_UP ? switch_time_now()-gp->uptime : 0);
	stream->write_function(stream, "    <calls-in>%u</calls-in>\n", gp->ib_calls);
	stream->write_function(stream, "    <calls-out>%u</calls-out>\n", gp->ob_calls);
	stream->write_function(stream, "    <failed-calls-in>%u</failed-calls-in>\n", gp->ib_failed_calls);
	stream->write_function(stream, "    <failed-calls-out>%u</failed-calls-out>\n", gp->ob_failed_calls);

	if (gp->state == REG_STATE_FAILED || gp->state == REG_STATE_TRYING) {
		time_t now = switch_epoch_time_now(NULL);
		if (gp->retry > now) {
			stream->write_function(stream, "    <retry>%ds</retry>\n", gp->retry - now);
		} else {
			stream->write_function(stream, "    <retry>NEVER</retry>\n");
		}
	}

	stream->write_function(stream, "  </gateway>\n");
}

static switch_status_t cmd_xml_status(char **argv, int argc, switch_stream_handle_t *stream)
{
	sofia_profile_t *profile = NULL;
	sofia_gateway_t *gp;
	switch_hash_index_t *hi;
	void *val;
	const void *vvar;
	int c = 0;
	int ac = 0;
	const char *header = "<?xml version=\"1.0\" encoding=\"ISO-8859-1\"?>";

	if (argc > 0) {
		if (argc == 1) {
			/* show summary of all gateways */

			stream->write_function(stream, "%s\n", header);
			stream->write_function(stream, "<gateways>\n", header);

			switch_mutex_lock(mod_sofia_globals.hash_mutex);
			for (hi = switch_core_hash_first(mod_sofia_globals.profile_hash); hi; hi = switch_core_hash_next(&hi)) {
				switch_core_hash_this(hi, &vvar, NULL, &val);
				profile = (sofia_profile_t *) val;
				if (sofia_test_pflag(profile, PFLAG_RUNNING)) {

					if (!strcmp(vvar, profile->name)) {	/* not an alias */
						for (gp = profile->gateways; gp; gp = gp->next) {
							switch_assert(gp->state < REG_STATE_LAST);

							xml_gateway_status(gp, stream);
						}
					}
				}
			}
			switch_mutex_unlock(mod_sofia_globals.hash_mutex);
			stream->write_function(stream, "</gateways>\n");

		} else if (argc == 1 && !strcasecmp(argv[0], "profile")) {
		} else if (!strcasecmp(argv[0], "gateway")) {
			if ((gp = sofia_reg_find_gateway(argv[1]))) {
				switch_assert(gp->state < REG_STATE_LAST);
				stream->write_function(stream, "%s\n", header);
				xml_gateway_status(gp, stream);
				sofia_reg_release_gateway(gp);
			} else {
				stream->write_function(stream, "Invalid Gateway!\n");
			}
		} else if (!strcasecmp(argv[0], "profile")) {
			struct cb_helper cb;
			char *sql = NULL;
			uint32_t x = 0;

			cb.row_process = 0;

			if ((argv[1]) && (profile = sofia_glue_find_profile(argv[1]))) {
				stream->write_function(stream, "%s\n", header);
				stream->write_function(stream, "<profile>\n");
				if (!argv[2] || (strcasecmp(argv[2], "reg") && strcasecmp(argv[2], "user"))) {
					stream->write_function(stream, "  <profile-info>\n");
					stream->write_function(stream, "    <name>%s</name>\n", switch_str_nil(argv[1]));
					stream->write_function(stream, "    <domain-name>%s</domain-name>\n", profile->domain_name ? profile->domain_name : "N/A");
					if (strcasecmp(argv[1], profile->name)) {
						stream->write_function(stream, "    <alias-of>%s</alias-of>\n", switch_str_nil(profile->name));
					}
					stream->write_function(stream, "    <auto-nat>%s</auto-nat>\n", sofia_test_pflag(profile, PFLAG_AUTO_NAT) ? "true" : "false");
					stream->write_function(stream, "    <db-name>%s</db-name>\n", profile->dbname ? profile->dbname : switch_str_nil(profile->odbc_dsn));
					stream->write_function(stream, "    <pres-hosts>%s</pres-hosts>\n", switch_str_nil(profile->presence_hosts));
					stream->write_function(stream, "    <dialplan>%s</dialplan>\n", switch_str_nil(profile->dialplan));
					stream->write_function(stream, "    <context>%s</context>\n", switch_str_nil(profile->context));
					stream->write_function(stream, "    <challenge-realm>%s</challenge-realm>\n",
										   zstr(profile->challenge_realm) ? "auto_to" : profile->challenge_realm);
					for (x = 0; x < profile->rtpip_index; x++) {
						stream->write_function(stream, "    <rtp-ip>%s</rtp-ip>\n", switch_str_nil(profile->rtpip[x]));
					}
					if (profile->extrtpip) {
						stream->write_function(stream, "    <ext-rtp-ip>%s</ext-rtp-ip>\n", profile->extrtpip);
					}
					stream->write_function(stream, "    <sip-ip>%s</sip-ip>\n", switch_str_nil(profile->sipip));
					if (profile->extsipip) {
						stream->write_function(stream, "    <ext-sip-ip>%s</ext-sip-ip>\n", profile->extsipip);
					}
					if (! sofia_test_pflag(profile, PFLAG_TLS) || ! profile->tls_only) {
						stream->write_function(stream, "    <url>%s</url>\n", switch_str_nil(profile->url));
						stream->write_function(stream, "    <bind-url>%s</bind-url>\n", switch_str_nil(profile->bindurl));
					}
					if (sofia_test_pflag(profile, PFLAG_TLS)) {
						stream->write_function(stream, "    <tls-url>%s</tls-url>\n", switch_str_nil(profile->tls_url));
						stream->write_function(stream, "    <tls-bind-url>%s</tls-bind-url>\n", switch_str_nil(profile->tls_bindurl));
					}
					if (profile->ws_bindurl) {
						stream->write_function(stream, "    <ws-bind-url>%s</ws-bind-url>\n", switch_str_nil(profile->ws_bindurl));
					}
					if (profile->wss_bindurl) {
						stream->write_function(stream, "    <wss-bind-url>%s</wss-bind-url>\n", switch_str_nil(profile->wss_bindurl));
					}
					stream->write_function(stream, "    <hold-music>%s</hold-music>\n", zstr(profile->hold_music) ? "N/A" : profile->hold_music);
					stream->write_function(stream, "    <outbound-proxy>%s</outbound-proxy>\n",
										   zstr(profile->outbound_proxy) ? "N/A" : profile->outbound_proxy);
					stream->write_function(stream, "    <inbound-codecs>%s</inbound-codecs>\n", switch_str_nil(profile->inbound_codec_string));
					stream->write_function(stream, "    <outbound-codecs>%s</outbound-codecs>\n", switch_str_nil(profile->outbound_codec_string));

					stream->write_function(stream, "    <tel-event>%d</tel-event>\n", profile->te);
					if (profile->dtmf_type == DTMF_2833) {
						stream->write_function(stream, "    <dtmf-mode>rfc2833</dtmf-mode>\n");
					} else if (profile->dtmf_type == DTMF_INFO) {
						stream->write_function(stream, "    <dtmf-mode>info</dtmf-mode>\n");
					} else {
						stream->write_function(stream, "    <dtmf-mode>none</dtmf-mode>\n");
					}
					stream->write_function(stream, "    <cng>%d</cng>\n", profile->cng_pt);
					stream->write_function(stream, "    <session-to>%d</session-to>\n", profile->session_timeout);
					stream->write_function(stream, "    <max-dialog>%d</max-dialog>\n", profile->max_proceeding);
					stream->write_function(stream, "    <nomedia>%s</nomedia>\n", sofia_test_flag(profile, TFLAG_INB_NOMEDIA) ? "true" : "false");
					stream->write_function(stream, "    <late-neg>%s</late-neg>\n", sofia_test_flag(profile, TFLAG_LATE_NEGOTIATION) ? "true" : "false");
					stream->write_function(stream, "    <proxy-media>%s</proxy-media>\n", sofia_test_flag(profile, TFLAG_PROXY_MEDIA) ? "true" : "false");
					stream->write_function(stream, "    <zrtp-passthru>%s</zrtp-passthru>\n", sofia_test_flag(profile, TFLAG_ZRTP_PASSTHRU) ? "true" : "false");
					stream->write_function(stream, "    <aggressive-nat>%s</aggressive-nat>\n",
										   sofia_test_pflag(profile, PFLAG_AGGRESSIVE_NAT_DETECTION) ? "true" : "false");
					if (profile->user_agent_filter) {
						stream->write_function(stream, "    <user-agent-filter>%s</user-agent-filter>\n", switch_str_nil(profile->user_agent_filter));
					}
					if (profile->max_registrations_perext > 0) {
						stream->write_function(stream, "    <max-registrations-per-extension>%d</max-registrations-per-extension>\n",
										   profile->max_registrations_perext);
					}
					stream->write_function(stream, "    <calls-in>%u</calls-in>\n", profile->ib_calls);
					stream->write_function(stream, "    <calls-out>%u</calls-out>\n", profile->ob_calls);
					stream->write_function(stream, "    <failed-calls-in>%u</failed-calls-in>\n", profile->ib_failed_calls);
					stream->write_function(stream, "    <failed-calls-out>%u</failed-calls-out>\n", profile->ob_failed_calls);
					stream->write_function(stream, "    <registrations>%lu</registrations>\n", sofia_profile_reg_count(profile));
					stream->write_function(stream, "  </profile-info>\n");
				}

				cb.profile = profile;
				cb.stream = stream;

				if (!sql && argv[2] && !strcasecmp(argv[2], "pres") && argv[3]) {

					sql = switch_mprintf("select call_id,sip_user,sip_host,contact,status,"
										 "rpid,expires,user_agent,server_user,server_host,profile_name,hostname,"
										 "network_ip,network_port,sip_username,sip_realm,mwi_user,mwi_host,ping_status"
										 " from sip_registrations where profile_name='%q' and presence_hosts like '%%%q%%'", profile->name, argv[3]);
				}
				if (!sql && argv[2] && !strcasecmp(argv[2], "reg") && argv[3]) {

					sql = switch_mprintf("select call_id,sip_user,sip_host,contact,status,"
										 "rpid,expires,user_agent,server_user,server_host,profile_name,hostname,"
										 "network_ip,network_port,sip_username,sip_realm,mwi_user,mwi_host,ping_status"
										 " from sip_registrations where profile_name='%q' and contact like '%%%q%%'", profile->name, argv[3]);
				}
				if (!sql && argv[2] && !strcasecmp(argv[2], "reg")) {

					sql = switch_mprintf("select call_id,sip_user,sip_host,contact,status,"
										 "rpid,expires,user_agent,server_user,server_host,profile_name,hostname,"
										 "network_ip,network_port,sip_username,sip_realm,mwi_user,mwi_host,ping_status"
										 " from sip_registrations where profile_name='%q'", profile->name);
				}
				if (!sql && argv[2] && !strcasecmp(argv[2], "user") && argv[3]) {
					char *dup = strdup(argv[3]);
					char *host = NULL, *user = NULL;
					char *sqlextra = NULL;

					switch_assert(dup);

					if ((host = strchr(dup, '@'))) {
						*host++ = '\0';
						user = dup;
					} else {
						host = dup;
					}

					if (zstr(user)) {
						sqlextra = switch_mprintf("(sip_host='%q')", host);
					} else if (zstr(host)) {
						sqlextra = switch_mprintf("(sip_user='%q')", user);
					} else {
						sqlextra = switch_mprintf("(sip_user='%q' and sip_host='%q')", user, host);
					}

					sql = switch_mprintf("select call_id,sip_user,sip_host,contact,status,"
										 "rpid,expires,user_agent,server_user,server_host,profile_name,hostname,"
										 "network_ip,network_port,sip_username,sip_realm,mwi_user,mwi_host,ping_status"
										 " from sip_registrations where profile_name='%q' and %s", profile->name, sqlextra);
					switch_safe_free(dup);
					switch_safe_free(sqlextra);
				}

				if (sql) {
					stream->write_function(stream, "  <registrations>\n");

					sofia_glue_execute_sql_callback(profile, profile->dbh_mutex, sql, show_reg_callback_xml, &cb);
					switch_safe_free(sql);

					stream->write_function(stream, "  </registrations>\n");
				}

				stream->write_function(stream, "</profile>\n");

				sofia_glue_release_profile(profile);
			} else {
				stream->write_function(stream, "Invalid Profile!\n");
			}
		} else {
			stream->write_function(stream, "Invalid Syntax!\n");
		}

		return SWITCH_STATUS_SUCCESS;
	}

	stream->write_function(stream, "%s\n", header);
	stream->write_function(stream, "<profiles>\n");
	switch_mutex_lock(mod_sofia_globals.hash_mutex);
	for (hi = switch_core_hash_first(mod_sofia_globals.profile_hash); hi; hi = switch_core_hash_next(&hi)) {
		switch_core_hash_this(hi, &vvar, NULL, &val);
		profile = (sofia_profile_t *) val;
		if (sofia_test_pflag(profile, PFLAG_RUNNING)) {

			if (strcmp(vvar, profile->name)) {
				ac++;
				stream->write_function(stream, "<alias>\n<name>%s</name>\n<type>%s</type>\n<data>%s</data>\n<state>%s</state>\n</alias>\n", vvar, "alias",
									   profile->name, "ALIASED");
			} else {
				if (! sofia_test_pflag(profile, PFLAG_TLS) || ! profile->tls_only){
					stream->write_function(stream, "<profile>\n<name>%s</name>\n<type>%s</type>\n<data>%s</data>\n<state>%s (%u)</state>\n</profile>\n",
									   profile->name, "profile", profile->url, sofia_test_pflag(profile, PFLAG_RUNNING) ? "RUNNING" : "DOWN",
									   profile->inuse);
				}

				if (sofia_test_pflag(profile, PFLAG_TLS)) {
					stream->write_function(stream, "<profile>\n<name>%s</name>\n<type>%s</type>\n<data>%s</data>\n<state>%s (%u) (TLS)</state>\n</profile>\n",
									   profile->name, "profile", profile->tls_url, sofia_test_pflag(profile, PFLAG_RUNNING) ? "RUNNING" : "DOWN",
									   profile->inuse);
				}
				if (profile->ws_bindurl){
					stream->write_function(stream, "<profile>\n<name>%s</name>\n<type>%s</type>\n<data>%s</data>\n<state>%s (%u) (WS)</state>\n</profile>\n",
									   profile->name, "profile", profile->ws_bindurl, sofia_test_pflag(profile, PFLAG_RUNNING) ? "RUNNING" : "DOWN",
									   profile->inuse);
				}
				if (profile->wss_bindurl){
					stream->write_function(stream, "<profile>\n<name>%s</name>\n<type>%s</type>\n<data>%s</data>\n<state>%s (%u) (WSS)</state>\n</profile>\n",
									   profile->name, "profile", profile->wss_bindurl, sofia_test_pflag(profile, PFLAG_RUNNING) ? "RUNNING" : "DOWN",
									   profile->inuse);
				}

				c++;

				for (gp = profile->gateways; gp; gp = gp->next) {
					switch_assert(gp->state < REG_STATE_LAST);
					stream->write_function(stream, "<gateway>\n<name>%s</name>\n<type>%s</type>\n<data>%s</data>\n<state>%s</state>\n</gateway>\n",
										   gp->name, "gateway", gp->register_to, sofia_state_names[gp->state]);
					if (gp->state == REG_STATE_FAILED || gp->state == REG_STATE_TRYING) {
						time_t now = switch_epoch_time_now(NULL);
						if (gp->retry > now) {
							stream->write_function(stream, " (retry: %ds)", gp->retry - now);
						} else {
							stream->write_function(stream, " (retry: NEVER)");
						}
					}
					stream->write_function(stream, "\n");
				}
			}
		}
	}
	switch_mutex_unlock(mod_sofia_globals.hash_mutex);
	stream->write_function(stream, "</profiles>\n");
	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t cmd_profile(char **argv, int argc, switch_stream_handle_t *stream)
{
	sofia_profile_t *profile = NULL;
	char *profile_name = argv[0];
	const char *err;

	if (argc < 2) {
		stream->write_function(stream, "Invalid Args!\n");
		return SWITCH_STATUS_SUCCESS;
	}

	if (!strcasecmp(argv[1], "start")) {

		switch_xml_reload(&err);
		stream->write_function(stream, "Reload XML [%s]\n", err);

		if (config_sofia(SOFIA_CONFIG_RESCAN, argv[0]) == SWITCH_STATUS_SUCCESS) {
			stream->write_function(stream, "%s started successfully\n", argv[0]);
		} else {
			stream->write_function(stream, "Failure starting %s\n", argv[0]);
		}
		return SWITCH_STATUS_SUCCESS;
	}

	if (argv[1] && !strcasecmp(argv[0], "restart") && !strcasecmp(argv[1], "all")) {
		sofia_glue_restart_all_profiles();
		return SWITCH_STATUS_SUCCESS;
	}

	if (zstr(profile_name) || !(profile = sofia_glue_find_profile(profile_name))) {
		stream->write_function(stream, "Invalid Profile [%s]", switch_str_nil(profile_name));
		return SWITCH_STATUS_SUCCESS;
	}

	if (!strcasecmp(argv[1], "killgw")) {
		sofia_gateway_t *gateway_ptr;
		if (argc < 3) {
			stream->write_function(stream, "-ERR missing gw name\n");
			goto done;
		}

		if (!strcasecmp(argv[2], "_all_")) {
			sofia_glue_del_every_gateway(profile);
			stream->write_function(stream, "+OK every gateway marked for deletion.\n");
		} else {
			if ((gateway_ptr = sofia_reg_find_gateway(argv[2]))) {
				sofia_glue_del_gateway(gateway_ptr);
				sofia_reg_release_gateway(gateway_ptr);
				stream->write_function(stream, "+OK gateway marked for deletion.\n");
			} else {
				stream->write_function(stream, "-ERR no such gateway.\n");
			}
		}

		goto done;
	}

	if (!strcasecmp(argv[1], "rescan")) {

		switch_xml_reload(&err);
		stream->write_function(stream, "Reload XML [%s]\n", err);

		if (config_sofia(SOFIA_CONFIG_RESCAN, profile->name) == SWITCH_STATUS_SUCCESS) {
			stream->write_function(stream, "+OK scan complete\n");
		} else {
			stream->write_function(stream, "-ERR cannot find config for profile %s\n", profile->name);
		}
		goto done;
	}

	if (!strcasecmp(argv[1], "check_sync")) {
		if (argc > 2) {
			sofia_reg_check_call_id(profile, argv[2]);
			stream->write_function(stream, "+OK syncing all registrations matching specified call_id\n");
		} else {
			sofia_reg_check_sync(profile);
			stream->write_function(stream, "+OK syncing all registrations\n");
		}

		goto done;
	}


	if (!strcasecmp(argv[1], "flush_inbound_reg")) {
		int reboot = 0;

		if (argc > 2) {
			if (!strcasecmp(argv[2], "reboot")) {
				reboot = 1;
				argc = 2;
			}
		}

		if (argc > 2) {
			if (argc > 3 && !strcasecmp(argv[3], "reboot")) {
				reboot = 1;
			}

			sofia_reg_expire_call_id(profile, argv[2], reboot);
			stream->write_function(stream, "+OK %s all registrations matching specified call_id\n", reboot ? "rebooting" : "flushing");
		} else {
			sofia_reg_check_expire(profile, 0, reboot);
			stream->write_function(stream, "+OK %s all registrations\n", reboot ? "rebooting" : "flushing");
		}

		goto done;
	}

	if (!strcasecmp(argv[1], "recover")) {
		if (argv[2] && !strcasecmp(argv[2], "flush")) {
			sofia_glue_profile_recover(profile, SWITCH_TRUE);

			stream->write_function(stream, "Flushing recovery database.\n");
		} else {
			int x = sofia_glue_profile_recover(profile, SWITCH_FALSE);
			if (x) {
				stream->write_function(stream, "Recovered %d session(s)\n", x);
			} else {
				stream->write_function(stream, "No sessions to recover.\n");
			}
		}

		goto done;
	}

	if (!strcasecmp(argv[1], "register")) {
		char *gname = argv[2];
		sofia_gateway_t *gateway_ptr;

		if (zstr(gname)) {
			stream->write_function(stream, "No gateway name provided!\n");
			goto done;
		}

		if (!strcasecmp(gname, "all")) {
			for (gateway_ptr = profile->gateways; gateway_ptr; gateway_ptr = gateway_ptr->next) {
				gateway_ptr->retry = 0;
				gateway_ptr->state = REG_STATE_UNREGED;
			}
			stream->write_function(stream, "+OK\n");
		} else if ((gateway_ptr = sofia_reg_find_gateway(gname))) {
			gateway_ptr->retry = 0;
			gateway_ptr->state = REG_STATE_UNREGED;
			stream->write_function(stream, "+OK\n");
			sofia_reg_release_gateway(gateway_ptr);
		} else {
			stream->write_function(stream, "Invalid gateway!\n");
		}

		goto done;
	}

	if (!strcasecmp(argv[1], "unregister")) {
		char *gname = argv[2];
		sofia_gateway_t *gateway_ptr;

		if (zstr(gname)) {
			stream->write_function(stream, "No gateway name provided!\n");
			goto done;
		}

		if (!strcasecmp(gname, "all")) {
			for (gateway_ptr = profile->gateways; gateway_ptr; gateway_ptr = gateway_ptr->next) {
				gateway_ptr->retry = 0;
				gateway_ptr->state = REG_STATE_UNREGISTER;
			}
			stream->write_function(stream, "+OK\n");
		} else if ((gateway_ptr = sofia_reg_find_gateway(gname))) {
			gateway_ptr->retry = 0;
			gateway_ptr->state = REG_STATE_UNREGISTER;
			stream->write_function(stream, "+OK\n");
			sofia_reg_release_gateway(gateway_ptr);
		} else {
			stream->write_function(stream, "Invalid gateway!\n");
		}
		goto done;
	}

	if (!strcasecmp(argv[1], "stop") || !strcasecmp(argv[1], "restart")) {
		int rsec = 10;
		int diff = (int) (switch_epoch_time_now(NULL) - profile->started);
		int remain = rsec - diff;
		if (diff < rsec) {
			stream->write_function(stream, "Profile %s must be up for at least %d seconds to stop/restart.\nPlease wait %d second%s\n",
								   profile->name, rsec, remain, remain == 1 ? "" : "s");
		} else {

			switch_xml_reload(&err);
			stream->write_function(stream, "Reload XML [%s]\n", err);

			if (!strcasecmp(argv[1], "stop")) {
				sofia_clear_pflag_locked(profile, PFLAG_RUNNING);
				if (argv[2] && !strcasecmp(argv[2], "wait")) {
					int loops = 20 * 2;
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "Waiting for %s to finish SIP transactions.\n", profile->name);
					while (!sofia_test_pflag(profile, PFLAG_SHUTDOWN)) {
						switch_yield(500000);
						if (!--loops) {
							switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "Timeout Waiting for %s to finish SIP transactions.\n", profile->name);
							break;
						}
					}
				}
				stream->write_function(stream, "stopping: %s", profile->name);
			} else {
				sofia_set_pflag_locked(profile, PFLAG_RESPAWN);
				sofia_clear_pflag_locked(profile, PFLAG_RUNNING);
				stream->write_function(stream, "restarting: %s", profile->name);
			}
		}
		goto done;
	}

	if (!strcasecmp(argv[1], "siptrace")) {
		if (argc > 2) {
			int value = switch_true(argv[2]);
			nua_set_params(profile->nua, TPTAG_LOG(value), TAG_END());
			stream->write_function(stream, "%s sip debugging on %s", value ? "Enabled" : "Disabled", profile->name);
		} else {
			stream->write_function(stream, "Usage: sofia profile <name> siptrace <on/off>\n");
		}
		goto done;
	}

		if (!strcasecmp(argv[1], "capture")) {
			   if (argc > 2) {
					   int value = switch_true(argv[2]);
					   nua_set_params(profile->nua, TPTAG_CAPT(value ? mod_sofia_globals.capture_server : NULL), TAG_END());
					   stream->write_function(stream, "%s sip capturing on %s", value ? "Enabled" : "Disabled", profile->name);
			   } else {
					   stream->write_function(stream, "Usage: sofia profile <name> capture <on/off>\n");
			   }
			   goto done;
		}

	if (!strcasecmp(argv[1], "watchdog")) {
		if (argc > 2) {
			int value = switch_true(argv[2]);
			profile->watchdog_enabled = value;
			stream->write_function(stream, "%s sip debugging on %s", value ? "Enabled" : "Disabled", profile->name);
		} else {
			stream->write_function(stream, "Usage: sofia profile <name> watchdog <on/off>\n");
		}
		goto done;
	}


	if (!strcasecmp(argv[1], "gwlist")) {
		int up = 1;

		if (argc > 2) {
			if (!strcasecmp(argv[2], "down")) {
				up = 0;
			}
		}

		sofia_glue_gateway_list(profile, stream, up);
		goto done;
	}


	stream->write_function(stream, "-ERR Unknown command!\n");

  done:
	if (profile) {
		sofia_glue_release_profile(profile);
	}

	return SWITCH_STATUS_SUCCESS;
}

static int contact_callback(void *pArg, int argc, char **argv, char **columnNames)
{
	struct cb_helper *cb = (struct cb_helper *) pArg;
	char *contact;

	cb->row_process++;

	if (!zstr(argv[0]) && (contact = sofia_glue_get_url_from_contact(argv[0], 1))) {
		if (cb->dedup) {
			char *tmp = switch_mprintf("%ssofia/%s/sip:%s", argv[2], argv[1], sofia_glue_strip_proto(contact));

			if (!strstr((char *)cb->stream->data, tmp)) {
				cb->stream->write_function(cb->stream, "%s,", tmp);
			}

			free(tmp);

		} else {
			cb->stream->write_function(cb->stream, "%ssofia/%s/sip:%s,", argv[2], argv[1], sofia_glue_strip_proto(contact));
		}
		free(contact);
	}

	return 0;
}

SWITCH_STANDARD_API(sofia_count_reg_function)
{
	char *data;
	char *user = NULL;
	char *domain = NULL;
	char *concat = NULL;
	char *profile_name = NULL;
	char *p;
	char *reply = "-1";
	sofia_profile_t *profile = NULL;

	if (!cmd) {
		stream->write_function(stream, "%s", "");
		return SWITCH_STATUS_SUCCESS;
	}

	data = strdup(cmd);
	switch_assert(data);

	if ((p = strchr(data, '/'))) {
		profile_name = data;
		*p++ = '\0';
		user = p;
	} else {
		user = data;
	}

	if ((domain = strchr(user, '@'))) {
		*domain++ = '\0';
		if ((concat = strchr(domain, '/'))) {
			*concat++ = '\0';
		}
	} else {
		if ((concat = strchr(user, '/'))) {
			*concat++ = '\0';
		}
	}

	if (!profile_name && domain) {
		profile_name = domain;
	}

	if (user && profile_name) {
		char *sql;

		if (!(profile = sofia_glue_find_profile(profile_name))) {
			profile_name = domain;
			domain = NULL;
		}

		if (!profile && profile_name) {
			profile = sofia_glue_find_profile(profile_name);
		}

		if (profile) {
			struct cb_helper_sql2str cb;
			char reg_count[80] = "";

			cb.buf = reg_count;
			cb.len = sizeof(reg_count);

			if (!domain || !strchr(domain, '.')) {
				domain = profile->name;
			}

			if (zstr(user)) {
				sql = switch_mprintf("select count(*) "
									 "from sip_registrations where (sip_host='%q' or presence_hosts like '%%%q%%')",
									 domain, domain);

			} else {
				sql = switch_mprintf("select count(*) "
									 "from sip_registrations where sip_user='%q' and (sip_host='%q' or presence_hosts like '%%%q%%')",
									 user, domain, domain);
			}
			switch_assert(sql);
			sofia_glue_execute_sql_callback(profile, profile->dbh_mutex, sql, sql2str_callback, &cb);
			switch_safe_free(sql);
			if (!zstr(reg_count)) {
				stream->write_function(stream, "%s", reg_count);
			} else {
				stream->write_function(stream, "0");
			}
			reply = NULL;

		}
	}

	if (reply) {
		stream->write_function(stream, "%s", reply);
	}

	switch_safe_free(data);

	if (profile) {
		sofia_glue_release_profile(profile);
	}

	return SWITCH_STATUS_SUCCESS;
}

SWITCH_STANDARD_API(sofia_username_of_function)
{
	char *data;
	char *user = NULL;
	char *domain = NULL;
	char *profile_name = NULL;
	char *p;
	char *reply = "";
	sofia_profile_t *profile = NULL;

	if (!cmd) {
		stream->write_function(stream, "%s", "");
		return SWITCH_STATUS_SUCCESS;
	}

	data = strdup(cmd);
	switch_assert(data);

	if ((p = strchr(data, '/'))) {
		profile_name = data;
		*p++ = '\0';
		user = p;
	} else {
		user = data;
	}

	if ((domain = strchr(user, '@'))) {
		*domain++ = '\0';
	}

	if (!profile_name && domain) {
		profile_name = domain;
	}

	if (user && profile_name) {
		char *sql;

		if (!(profile = sofia_glue_find_profile(profile_name))) {
			profile_name = domain;
			domain = NULL;
		}

		if (!profile && profile_name) {
			profile = sofia_glue_find_profile(profile_name);
		}

		if (profile) {
			struct cb_helper_sql2str cb;
			char username[256] = "";

			cb.buf = username;
			cb.len = sizeof(username);

			if (!domain || !strchr(domain, '.')) {
				domain = profile->name;
			}

			switch_assert(!zstr(user));

			sql = switch_mprintf("select sip_username "
									"from sip_registrations where sip_user='%q' and (sip_host='%q' or presence_hosts like '%%%q%%')",
									user, domain, domain);

			switch_assert(sql);

			sofia_glue_execute_sql_callback(profile, profile->dbh_mutex, sql, sql2str_callback, &cb);
			switch_safe_free(sql);
			if (!zstr(username)) {
				stream->write_function(stream, "%s", username);
			} else {
				stream->write_function(stream, "");
			}
			reply = NULL;

		}
	}

	if (reply) {
		stream->write_function(stream, "%s", reply);
	}

	switch_safe_free(data);

	if (profile) {
		sofia_glue_release_profile(profile);
	}

	return SWITCH_STATUS_SUCCESS;
}

static void select_from_profile(sofia_profile_t *profile,
								const char *user,
								const char *domain,
								const char *concat,
								const char *exclude_contact,
								switch_stream_handle_t *stream,
								switch_bool_t dedup)
{
	struct cb_helper cb;
	char *sql;

	cb.row_process = 0;

	cb.profile = profile;
	cb.stream = stream;
	cb.dedup = dedup;

	if (exclude_contact) {
		sql = switch_mprintf("select contact, profile_name, '%q' "
							 "from sip_registrations where profile_name='%q' "
							 "and upper(sip_user)=upper('%q') " 
							 "and (sip_host='%q' or presence_hosts like '%%%q%%') "
							 "and contact not like '%%%s%%'", (concat != NULL) ? concat : "", profile->name, user, domain, domain, exclude_contact);
	} else {
		sql = switch_mprintf("select contact, profile_name, '%q' "
							 "from sip_registrations where profile_name='%q' "
							 "and upper(sip_user)=upper('%q') "
							 "and (sip_host='%q' or presence_hosts like '%%%q%%')",
							 (concat != NULL) ? concat : "", profile->name, user, domain, domain);
	}

	switch_assert(sql);
	sofia_glue_execute_sql_callback(profile, profile->dbh_mutex, sql, contact_callback, &cb);
	switch_safe_free(sql);
}

SWITCH_STANDARD_API(sofia_contact_function)
{
	char *data;
	char *user = NULL;
	char *domain = NULL, *dup_domain = NULL;
	char *concat = NULL;
	char *profile_name = NULL;
	char *p;
	sofia_profile_t *profile = NULL;
	const char *exclude_contact = NULL;
	char *reply = "error/facility_not_subscribed";
	switch_stream_handle_t mystream = { 0 };

	if (!cmd) {
		stream->write_function(stream, "%s", "");
		return SWITCH_STATUS_SUCCESS;
	}

	if (session) {
		switch_channel_t *channel = switch_core_session_get_channel(session);
		exclude_contact = switch_channel_get_variable(channel, "sip_exclude_contact");
	}


	data = strdup(cmd);
	switch_assert(data);

	if ((p = strchr(data, '/'))) {
		profile_name = data;
		*p++ = '\0';
		user = p;
	} else {
		user = data;
	}

	if ((domain = strchr(user, '@'))) {
		*domain++ = '\0';
		if ((concat = strchr(domain, '/'))) {
			*concat++ = '\0';
		}
	} else {
		if ((concat = strchr(user, '/'))) {
			*concat++ = '\0';
		}
	}

	if (zstr(domain)) {
		dup_domain = switch_core_get_domain(SWITCH_TRUE);
		domain = dup_domain;
	}

	if (!user) goto end;

	if (zstr(profile_name) || strcmp(profile_name, "*") || zstr(domain)) {
		if (!zstr(profile_name)) {
			profile = sofia_glue_find_profile(profile_name);
		}

		if (!profile && !zstr(domain)) {
			profile = sofia_glue_find_profile(domain);
		}
	}

	if (profile || !zstr(domain)) {
		SWITCH_STANDARD_STREAM(mystream);
		switch_assert(mystream.data);
	}

	if (profile) {
		if (zstr(domain)) {
			domain = profile->name;
		}

		if (!zstr(profile->domain_name) && !zstr(profile_name) && !strcmp(profile_name, profile->name)) {
			domain = profile->domain_name;
		}

		select_from_profile(profile, user, domain, concat, exclude_contact, &mystream, SWITCH_FALSE);
		sofia_glue_release_profile(profile);

	} else if (!zstr(domain)) {
		switch_mutex_lock(mod_sofia_globals.hash_mutex);
		if (mod_sofia_globals.profile_hash) {
			switch_hash_index_t *hi;
			const void *var;
			void *val;

			for (hi = switch_core_hash_first(mod_sofia_globals.profile_hash); hi; hi = switch_core_hash_next(&hi)) {
				switch_core_hash_this(hi, &var, NULL, &val);
				if ((profile = (sofia_profile_t *) val) && !strcmp((char *)var, profile->name)) {
					select_from_profile(profile, user, domain, concat, exclude_contact, &mystream, SWITCH_TRUE);
					profile = NULL;
				}
			}
		}
		switch_mutex_unlock(mod_sofia_globals.hash_mutex);
	}

	reply = (char *) mystream.data;

 end:

	if (zstr(reply)) {
		reply = "error/user_not_registered";
	} else if (end_of(reply) == ',') {
		end_of(reply) = '\0';
	}

	stream->write_function(stream, "%s", reply);
	reply = NULL;

	switch_safe_free(mystream.data);

	switch_safe_free(data);
	switch_safe_free(dup_domain);

	return SWITCH_STATUS_SUCCESS;
}

struct list_result {
	int row_process;
	int single_col;
	switch_stream_handle_t *stream;

};
static int list_result_callback(void *pArg, int argc, char **argv, char **columnNames)
{
	struct list_result *cbt = (struct list_result *) pArg;
	int i = 0;

	cbt->row_process++;

	if (cbt->row_process == 1) {
		for ( i = 0; i < argc; i++) {
			cbt->stream->write_function(cbt->stream,"%s", columnNames[i]);
			if (i < argc - 1) {
				cbt->stream->write_function(cbt->stream,"|");
			}
		}
		cbt->stream->write_function(cbt->stream,"\n");

	}
	for ( i = 0; i < argc; i++) {
		cbt->stream->write_function(cbt->stream,"%s", zstr(argv[i]) ? "unknown" : argv[i]);
		if (i < argc - 1) {
			cbt->stream->write_function(cbt->stream,"|");
		}
	}
	if (!cbt->single_col)
		cbt->stream->write_function(cbt->stream,"\n");
	return 0;
}


static void get_presence_data(sofia_profile_t *profile, const char *user, const char *domain, const char *search, switch_stream_handle_t *stream)
{
	struct list_result cb;
	char *sql;
	char *select;

	cb.row_process = 1;
	cb.single_col = 1;
	cb.stream = stream;

	if (!strcasecmp(search, "status")) {
		select = switch_mprintf(" p.status ");
	} else if (!strcasecmp(search, "rpid")) {
		select = switch_mprintf(" p.rpid ");
	} else if (!strcasecmp(search, "user_agent")) {
		select = switch_mprintf(" r.user_agent ");
	}  else {
		cb.row_process = 0;
		cb.single_col = 0;
		select = switch_mprintf(" p.status, p.rpid, r.user_agent,  r.network_ip, r.network_port ");
	}

	sql = switch_mprintf(" select %q from sip_registrations as r left join sip_presence as p "
		" on p.sip_host = r.sip_host and p.profile_name = r.profile_name and p.hostname = r.orig_hostname "
		" and p.sip_user = r.sip_user "
		" where r.sip_realm = '%q' and r.sip_user = '%q' and r.profile_name = '%q' ", select, domain, user, profile->name);

	switch_assert(sql);
	sofia_glue_execute_sql_callback(profile, profile->dbh_mutex, sql, list_result_callback, &cb);
	switch_safe_free(sql);
	switch_safe_free(select);
}

/* [list|status|rpid|user_agent] [profile/]<user>@domain */
SWITCH_STANDARD_API(sofia_presence_data_function)
{
	char *argv[6] = { 0 };
	int argc;
	char *data;
	char *user = NULL;
	char *domain = NULL, *dup_domain = NULL;
	char *concat = NULL;
	char *search = NULL;
	char *profile_name = NULL;
	char *p;
	sofia_profile_t *profile = NULL;

	if (!cmd) {
		stream->write_function(stream, "%s", "");
		return SWITCH_STATUS_SUCCESS;
	}

	data = strdup(cmd);
	switch_assert(data);


	argc = switch_separate_string(data, ' ', argv, (sizeof(argv) / sizeof(argv[0])));
	if (argc < 2) {
		stream->write_function(stream, "%s", "");
		return SWITCH_STATUS_SUCCESS;
	}
	search = argv[0];

	if ((p = strchr(argv[1], '/'))) {
		profile_name = argv[1];
		*p++ = '\0';
		user = p;
	} else {
		user = argv[1];
	}

	if ((domain = strchr(user, '@'))) {
		*domain++ = '\0';
		if ((concat = strchr(domain, '/'))) {
			*concat++ = '\0';
		}
	} else {
		if ((concat = strchr(user, '/'))) {
			*concat++ = '\0';
		}
	}

	if (zstr(domain)) {
		dup_domain = switch_core_get_domain(SWITCH_TRUE);
		domain = dup_domain;
	}

	if (!user) goto end;

	if (zstr(profile_name) || strcmp(profile_name, "*") || zstr(domain)) {
		if (!zstr(profile_name)) {
			profile = sofia_glue_find_profile(profile_name);
		}

		if (!profile && !zstr(domain)) {
			profile = sofia_glue_find_profile(domain);
		}
	}

	if (profile) {
		if (zstr(domain)) {
			domain = profile->name;
		}

		if (!zstr(profile->domain_name) && !zstr(profile_name) && !strcmp(profile_name, profile->name)) {
			domain = profile->domain_name;
		}

		get_presence_data(profile, user, domain, search, stream);
		sofia_glue_release_profile(profile);

	} else if (!zstr(domain)) {
		switch_mutex_lock(mod_sofia_globals.hash_mutex);
		if (mod_sofia_globals.profile_hash) {
			switch_hash_index_t *hi;
			const void *var;
			void *val;

			for (hi = switch_core_hash_first(mod_sofia_globals.profile_hash); hi; hi = switch_core_hash_next(&hi)) {
				switch_core_hash_this(hi, &var, NULL, &val);
				if ((profile = (sofia_profile_t *) val) && !strcmp((char *)var, profile->name)) {
					get_presence_data(profile, user, domain, search, stream);
					profile = NULL;
				}
			}
		}
		switch_mutex_unlock(mod_sofia_globals.hash_mutex);
	}

	if (!strcasecmp(search, "list"))
		stream->write_function(stream, "+OK\n");

 end:
	switch_safe_free(data);
	switch_safe_free(dup_domain);

	return SWITCH_STATUS_SUCCESS;
}

/* <gateway_name> [ivar|ovar|var] <name> */
SWITCH_STANDARD_API(sofia_gateway_data_function)
{
	char *argv[4];
	char *mydata;
	int argc;
	sofia_gateway_t *gateway;
	char *gwname, *param, *varname;
	const char *val = NULL;

	if (zstr(cmd)) {
		stream->write_function(stream, "-ERR Parameter missing\n");
		return SWITCH_STATUS_SUCCESS;
	}
	if (!(mydata = strdup(cmd))) {
		return SWITCH_STATUS_FALSE;
	}

	if (!(argc = switch_separate_string(mydata, ' ', argv, (sizeof(argv) / sizeof(argv[0])))) || !argv[0]) {
		goto end;
	}

	gwname = argv[0];
	param = argv[1];
	varname = argv[2];

	if (zstr(gwname) || zstr(param) || zstr(varname)) {
		goto end;
	}

	if (!(gateway = sofia_reg_find_gateway(gwname))) {
		goto end;
	}

	if (!strcasecmp(param, "ivar") && gateway->ib_vars && (val = switch_event_get_header(gateway->ib_vars, varname))) {
		stream->write_function(stream, "%s", val);
	} else if (!strcasecmp(param, "ovar") && gateway->ob_vars && (val = switch_event_get_header(gateway->ob_vars, varname))) {
		stream->write_function(stream, "%s", val);
	} else if (!strcasecmp(param, "var")) {
		if (gateway->ib_vars && (val = switch_event_get_header(gateway->ib_vars, varname))) {
			stream->write_function(stream, "%s", val);
		} else if (gateway->ob_vars && (val = switch_event_get_header(gateway->ob_vars, varname))) {
			stream->write_function(stream, "%s", val);
		}
	}

	sofia_reg_release_gateway(gateway);

  end:
	switch_safe_free(mydata);
	return SWITCH_STATUS_SUCCESS;
}

SWITCH_STANDARD_API(sofia_function)
{
	char *argv[1024] = { 0 };
	int argc = 0;
	char *mycmd = NULL;
	switch_status_t status = SWITCH_STATUS_SUCCESS;
	sofia_command_t func = NULL;
	int lead = 1;
	static const char usage_string[] = "USAGE:\n"
		"--------------------------------------------------------------------------------\n"
		"sofia global siptrace <on|off>\n"
		"sofia        capture  <on|off>\n"
		"             watchdog <on|off>\n\n"
		"sofia profile <name> [start | stop | restart | rescan] [wait]\n"
		"                     flush_inbound_reg [<call_id> | <[user]@domain>] [reboot]\n"
		"                     check_sync [<call_id> | <[user]@domain>]\n"
		"                     [register | unregister] [<gateway name> | all]\n"
		"                     killgw <gateway name>\n"
		"                     [stun-auto-disable | stun-enabled] [true | false]]\n"
		"                     siptrace <on|off>\n"
		"                     capture  <on|off>\n"
		"                     watchdog <on|off>\n\n"
		"sofia <status|xmlstatus> profile <name> [reg [<contact str>]] | [pres <pres str>] | [user <user@domain>]\n"
		"sofia <status|xmlstatus> gateway <name>\n\n"
		"sofia loglevel <all|default|tport|iptsec|nea|nta|nth_client|nth_server|nua|soa|sresolv|stun> [0-9]\n"
		"sofia tracelevel <console|alert|crit|err|warning|notice|info|debug>\n\n"
		"sofia help\n"
		"--------------------------------------------------------------------------------\n";

	if (zstr(cmd)) {
		stream->write_function(stream, "%s", usage_string);
		goto done;
	}

	if (!(mycmd = strdup(cmd))) {
		status = SWITCH_STATUS_MEMERR;
		goto done;
	}

	if (!(argc = switch_separate_string(mycmd, ' ', argv, (sizeof(argv) / sizeof(argv[0])))) || !argv[0]) {
		stream->write_function(stream, "%s", usage_string);
		goto done;
	}

	if (!strcasecmp(argv[0], "profile")) {
		func = cmd_profile;
	} else if (!strcasecmp(argv[0], "status")) {
		func = cmd_status;
	} else if (!strcasecmp(argv[0], "xmlstatus")) {
		func = cmd_xml_status;
	} else if (!strcasecmp(argv[0], "tracelevel")) {
		if (argv[1]) {
			mod_sofia_globals.tracelevel = switch_log_str2level(argv[1]);
		}
		stream->write_function(stream, "+OK tracelevel is %s", switch_log_level2str(mod_sofia_globals.tracelevel));
		goto done;
	} else if (!strcasecmp(argv[0], "loglevel")) {
		if (argc > 2 && argv[2] && switch_is_number(argv[2])) {
			int level = atoi(argv[2]);
			if (sofia_set_loglevel(argv[1], level) == SWITCH_STATUS_SUCCESS) {
				stream->write_function(stream, "Sofia log level for component [%s] has been set to [%d]", argv[1], level);
			} else {
				stream->write_function(stream, "%s", usage_string);
			}
		} else if (argc > 1 && argv[1]) {
			int level = sofia_get_loglevel(argv[1]);
			if (level >= 0) {
				stream->write_function(stream, "Sofia-sip loglevel for [%s] is [%d]", argv[1], level);
			} else {
				stream->write_function(stream, "%s", usage_string);
			}
		} else {
			stream->write_function(stream, "%s", usage_string);
		}
		goto done;
	} else if (!strcasecmp(argv[0], "help")) {
		stream->write_function(stream, "%s", usage_string);
		goto done;
	} else if (!strcasecmp(argv[0], "global")) {
		int ston = -1;
		int cton = -1;
		int wdon = -1;
		int stbyon = -1;

		if (argc > 1) {
			if (!strcasecmp(argv[1], "debug")) {

				if (argc > 2) {
					if (strstr(argv[2], "presence")) {
						mod_sofia_globals.debug_presence = 10;
						stream->write_function(stream, "+OK Debugging presence\n");
					}

					if (strstr(argv[2], "sla")) {
						mod_sofia_globals.debug_sla = 10;
						stream->write_function(stream, "+OK Debugging sla\n");
					}

					if (strstr(argv[2], "none")) {
						stream->write_function(stream, "+OK Debugging nothing\n");
						mod_sofia_globals.debug_presence = 0;
						mod_sofia_globals.debug_sla = 0;
					}
				}

				stream->write_function(stream, "+OK Debugging summary: presence: %s sla: %s\n",
									   mod_sofia_globals.debug_presence ? "on" : "off",
									   mod_sofia_globals.debug_sla ? "on" : "off");

				goto done;
			}

			if (!strcasecmp(argv[1], "siptrace")) {
				if (argc > 2) {
					ston = switch_true(argv[2]);
				}
			}

			if (!strcasecmp(argv[1], "standby")) {
				if (argc > 2) {
					stbyon = switch_true(argv[2]);
				}
			}

			if (!strcasecmp(argv[1], "capture")) {
							if (argc > 2) {
										cton = switch_true(argv[2]);
								}
						}

			if (!strcasecmp(argv[1], "watchdog")) {
				if (argc > 2) {
					wdon = switch_true(argv[2]);
				}
			}
		}

		if (ston != -1) {
			sofia_glue_global_siptrace(ston);
			stream->write_function(stream, "+OK Global siptrace %s", ston ? "on" : "off");
		} else if (cton != -1) {
			sofia_glue_global_capture(cton);
			stream->write_function(stream, "+OK Global capture %s", cton ? "on" : "off");
		} else if (wdon != -1) {
			sofia_glue_global_watchdog(wdon);
			stream->write_function(stream, "+OK Global watchdog %s", wdon ? "on" : "off");
		} else if (stbyon != -1) {
			sofia_glue_global_standby(stbyon);
			stream->write_function(stream, "+OK Global standby %s", stbyon ? "on" : "off");
		} else {
			stream->write_function(stream, "-ERR Usage: siptrace <on|off>|capture <on|off>|watchdog <on|off>|debug <sla|presence|none");
		}

		goto done;

	} else if (!strcasecmp(argv[0], "recover")) {
		if (argv[1] && !strcasecmp(argv[1], "flush")) {
			sofia_glue_recover(SWITCH_TRUE);
			stream->write_function(stream, "Flushing recovery database.\n");
		} else {
			int x = sofia_glue_recover(SWITCH_FALSE);
			switch_event_t *event = NULL;

			if (x) {
				if (switch_event_create_subclass(&event, SWITCH_EVENT_CUSTOM,
					MY_EVENT_RECOVERY_RECOVERED) == SWITCH_STATUS_SUCCESS) {
					switch_event_add_header(event, SWITCH_STACK_BOTTOM, "recovered_calls", "%d", x);
					switch_event_fire(&event);
				}

				stream->write_function(stream, "Recovered %d call(s)\n", x);
			} else {
				if (switch_event_create_subclass(&event, SWITCH_EVENT_CUSTOM,
					MY_EVENT_RECOVERY_RECOVERED) == SWITCH_STATUS_SUCCESS) {
					switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "recovered_calls", "0");
					switch_event_fire(&event);
				}

				stream->write_function(stream, "No calls to recover.\n");
			}
		}

		goto done;
	}

	if (func) {
		status = func(&argv[lead], argc - lead, stream);
	} else {
		stream->write_function(stream, "Unknown Command [%s]\n", argv[0]);
	}

  done:
	switch_safe_free(mycmd);
	return status;
}

switch_io_routines_t sofia_io_routines = {
	/*.outgoing_channel */ sofia_outgoing_channel,
	/*.read_frame */ sofia_read_frame,
	/*.write_frame */ sofia_write_frame,
	/*.kill_channel */ sofia_kill_channel,
	/*.send_dtmf */ sofia_send_dtmf,
	/*.receive_message */ sofia_receive_message,
	/*.receive_event */ sofia_receive_event,
	/*.state_change */ NULL,
	/*.read_video_frame */ sofia_read_video_frame,
	/*.write_video_frame */ sofia_write_video_frame,
	/*.state_run*/ NULL,
	/*.get_jb*/ sofia_get_jb
};

switch_state_handler_table_t sofia_event_handlers = {
	/*.on_init */ sofia_on_init,
	/*.on_routing */ sofia_on_routing,
	/*.on_execute */ sofia_on_execute,
	/*.on_hangup */ sofia_on_hangup,
	/*.on_exchange_media */ sofia_on_exchange_media,
	/*.on_soft_execute */ sofia_on_soft_execute,
	/*.on_consume_media */ NULL,
	/*.on_hibernate */ sofia_on_hibernate,
	/*.on_reset */ sofia_on_reset,
	/*.on_park */ NULL,
	/*.on_reporting */ NULL,
	/*.on_destroy */ sofia_on_destroy
};

static switch_status_t sofia_manage(char *relative_oid, switch_management_action_t action, char *data, switch_size_t datalen)
{
	return SWITCH_STATUS_SUCCESS;
}

static int protect_dest_uri(switch_caller_profile_t *cp)
{
	char *p = cp->destination_number, *o = p;
	char *q = NULL, *e = NULL, *qenc = NULL;
	switch_size_t enclen = 0;
	int mod = 0;

	if (!(e = strchr(p, '@'))) {
		return 0;
	}

	while((p = strchr(p, '/'))) {
		q = p++;
	}

	if (q) {
		const char *i;
		int go = 0;

		for (i = q+1; i && *i && *i != '@'; i++) {
			if (strchr(SWITCH_URL_UNSAFE, *i)) {
				go = 1;
			}
		}
		
		if (!go) return 0;
		
		*q++ = '\0';
	} else {
		return 0;
	}
	
	if (!strncasecmp(q, "sips:", 5)) {
		q += 5;
	} else if (!strncasecmp(q, "sip:", 4)) {
		q += 4;
	}

	if (!(e = strchr(q, '@'))) {
		return 0;
	}

	*e++ = '\0';

	if (switch_needs_url_encode(q)) {
		enclen = (strlen(q) * 2)  + 2;
		qenc = switch_core_alloc(cp->pool, enclen);
		switch_url_encode(q, qenc, enclen);
		mod = 1;
	}
	
	cp->destination_number = switch_core_sprintf(cp->pool, "%s/%s@%s", o, qenc ? qenc : q, e);

	return mod;
}

static switch_call_cause_t sofia_outgoing_channel(switch_core_session_t *session, switch_event_t *var_event,
												  switch_caller_profile_t *outbound_profile, switch_core_session_t **new_session,
												  switch_memory_pool_t **pool, switch_originate_flag_t flags, switch_call_cause_t *cancel_cause)
{
	switch_call_cause_t cause = SWITCH_CAUSE_DESTINATION_OUT_OF_ORDER;
	switch_core_session_t *nsession = NULL;
	char *data, *profile_name, *dest;	//, *dest_num = NULL;
	sofia_profile_t *profile = NULL;
	switch_caller_profile_t *caller_profile = NULL;
	private_object_t *tech_pvt = NULL;
	switch_channel_t *nchannel;
	char *host = NULL, *dest_to = NULL;
	const char *hval = NULL;
	char *not_const = NULL;
	int cid_locked = 0;
	switch_channel_t *o_channel = NULL;
	sofia_gateway_t *gateway_ptr = NULL;
	int mod = 0;

	*new_session = NULL;

	if (!outbound_profile || zstr(outbound_profile->destination_number)) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Invalid Empty Destination\n");
		goto error;
	}

	if (!switch_true(switch_event_get_header(var_event, "sofia_suppress_url_encoding"))) {
		mod = protect_dest_uri(outbound_profile);
	}

	if (!(nsession = switch_core_session_request_uuid(sofia_endpoint_interface, SWITCH_CALL_DIRECTION_OUTBOUND,
													  flags, pool, switch_event_get_header(var_event, "origination_uuid")))) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Error Creating Session\n");
		goto error;
	}

	tech_pvt = sofia_glue_new_pvt(nsession);

	data = switch_core_session_strdup(nsession, outbound_profile->destination_number);
	if ((dest_to = strchr(data, '^'))) {
		*dest_to++ = '\0';
	}
	profile_name = data;

	nchannel = switch_core_session_get_channel(nsession);

	if (session) {
		o_channel = switch_core_session_get_channel(session);
	}


	if ((hval = switch_event_get_header(var_event, "sip_invite_to_uri"))) {
		dest_to = switch_core_session_strdup(nsession, hval);
	}

	if (!strncasecmp(profile_name, "gateway/", 8)) {
		char *gw, *params;

		if (!(gw = strchr(profile_name, '/'))) {
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "Invalid URL \'%s\'\n", profile_name);
			cause = SWITCH_CAUSE_INVALID_URL;
			goto error;
		}

		*gw++ = '\0';

		if (!(dest = strchr(gw, '/'))) {
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "Invalid URL \'%s\'\n", gw);
			cause = SWITCH_CAUSE_INVALID_URL;
			goto error;
		}

		*dest++ = '\0';

		if (!(gateway_ptr = sofia_reg_find_gateway(gw))) {
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "Invalid Gateway \'%s\'\n", gw);
			cause = SWITCH_CAUSE_INVALID_GATEWAY;
			goto error;
		}

		if (gateway_ptr->status != SOFIA_GATEWAY_UP) {
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "Gateway \'%s\' is down!\n", gw);
			cause = SWITCH_CAUSE_GATEWAY_DOWN;
			gateway_ptr->ob_failed_calls++;
			goto error;
		}

		tech_pvt->transport = gateway_ptr->register_transport;
		tech_pvt->cid_type = gateway_ptr->cid_type;
		cid_locked = 1;

		/*
		 * Handle params, strip them off the destination and add them to the
		 * invite contact.
		 *
		 */

		if ((params = strchr(dest, ';'))) {
			char *tp_param;

			*params++ = '\0';

			if ((tp_param = (char *) switch_stristr("port=", params))) {
				tp_param += 5;
				tech_pvt->transport = sofia_glue_str2transport(tp_param);
				if (tech_pvt->transport == SOFIA_TRANSPORT_UNKNOWN) {
					cause = SWITCH_CAUSE_DESTINATION_OUT_OF_ORDER;
					gateway_ptr->ob_failed_calls++;
					goto error;
				}
			}
		}

		if (tech_pvt->transport != gateway_ptr->register_transport) {
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR,
							  "You are trying to use a different transport type for this gateway (overriding the register-transport), this is unsupported!\n");
			cause = SWITCH_CAUSE_DESTINATION_OUT_OF_ORDER;
			goto error;
		}

		profile = gateway_ptr->profile;

		if (profile && sofia_test_pflag(profile, PFLAG_STANDBY)) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "System Paused\n");
			cause = SWITCH_CAUSE_SYSTEM_SHUTDOWN;
			goto error;
		}

		tech_pvt->gateway_name = switch_core_session_strdup(nsession, gateway_ptr->name);
		switch_channel_set_variable(nchannel, "sip_gateway_name", gateway_ptr->name);

		if (!sofia_test_flag(gateway_ptr, REG_FLAG_CALLERID)) {
			tech_pvt->gateway_from_str = switch_core_session_strdup(nsession, gateway_ptr->register_from);
		}

		if (!strchr(dest, '@')) {
			tech_pvt->dest = switch_core_session_sprintf(nsession, "sip:%s%s@%s", gateway_ptr->destination_prefix, dest, sofia_glue_strip_proto(gateway_ptr->register_proxy));
		} else {
			tech_pvt->dest = switch_core_session_sprintf(nsession, "sip:%s%s", gateway_ptr->destination_prefix, dest);
		}

		if ((host = switch_core_session_strdup(nsession, tech_pvt->dest))) {
			char *pp = strchr(host, '@');
			if (pp) {
				host = pp + 1;
			} else {
				host = NULL;
				dest_to = NULL;
			}
		}

		if (params) {
			tech_pvt->invite_contact = switch_core_session_sprintf(nsession, "%s;%s", gateway_ptr->register_contact, params);
			tech_pvt->dest = switch_core_session_sprintf(nsession, "%s;%s", tech_pvt->dest, params);
		} else {
			tech_pvt->invite_contact = switch_core_session_strdup(nsession, gateway_ptr->register_contact);
		}

		gateway_ptr->ob_calls++;

		if (!zstr(gateway_ptr->from_domain) && !switch_channel_get_variable(nchannel, "sip_invite_domain")) {

			if (!strcasecmp(gateway_ptr->from_domain, "auto-aleg-full")) {
				const char *sip_full_from = switch_channel_get_variable(o_channel, "sip_full_from");

				if (!zstr(sip_full_from)) {
					switch_channel_set_variable(nchannel, "sip_force_full_from", sip_full_from);
				}

			} else if (!strcasecmp(gateway_ptr->from_domain, "auto-aleg-domain")) {
				const char *sip_from_host = switch_channel_get_variable(o_channel, "sip_from_host");

				if (!zstr(sip_from_host)) {
					switch_channel_set_variable(nchannel, "sip_invite_domain", sip_from_host);
				}
			} else {
				switch_channel_set_variable(nchannel, "sip_invite_domain", gateway_ptr->from_domain);
			}
		}

		if (!zstr(gateway_ptr->outbound_sticky_proxy) && !switch_channel_get_variable(nchannel, "sip_route_uri")) {
			switch_channel_set_variable(nchannel, "sip_route_uri", gateway_ptr->outbound_sticky_proxy);
		}

	} else {
		if (!(dest = strchr(profile_name, '/'))) {
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "Invalid URL\n");
			cause = SWITCH_CAUSE_INVALID_URL;
			goto error;
		}
		*dest++ = '\0';

		if (!(profile = sofia_glue_find_profile(profile_name))) {
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "Invalid Profile\n");
			cause = SWITCH_CAUSE_INVALID_PROFILE;
			goto error;
		}

		if (profile && sofia_test_pflag(profile, PFLAG_STANDBY)) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "System Paused\n");
			cause = SWITCH_CAUSE_SYSTEM_SHUTDOWN;
			goto error;
		}

		if (profile->domain_name && strcmp(profile->domain_name, profile->name)) {
			profile_name = profile->domain_name;
		}

		if (!strncasecmp(dest, "sip:", 4) || !strncasecmp(dest, "sips:", 5)) {
			char *c;
			tech_pvt->dest = switch_core_session_strdup(nsession, dest);
			if ((c = strchr(tech_pvt->dest, ':'))) {
				c++;
				tech_pvt->e_dest = switch_core_session_strdup(nsession, c);
			}
		} else if (!mod && !strchr(dest, '@') && (host = strchr(dest, '%'))) {
			char buf[1024];
			*host = '@';
			tech_pvt->e_dest = switch_core_session_strdup(nsession, dest);
			*host++ = '\0';
			if (sofia_reg_find_reg_url(profile, dest, host, buf, sizeof(buf))) {
				tech_pvt->dest = switch_core_session_strdup(nsession, buf);
				tech_pvt->local_url = switch_core_session_sprintf(nsession, "%s@%s", dest, host);
			} else {
				switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_WARNING, "Cannot locate registered user %s@%s\n", dest, host);
				cause = SWITCH_CAUSE_USER_NOT_REGISTERED;
				goto error;
			}
		} else if (!(host = strchr(dest, '@'))) {
			char buf[1024];
			tech_pvt->e_dest = switch_core_session_strdup(nsession, dest);
			if (sofia_reg_find_reg_url(profile, dest, profile_name, buf, sizeof(buf))) {
				tech_pvt->dest = switch_core_session_strdup(nsession, buf);
				tech_pvt->local_url = switch_core_session_sprintf(nsession, "%s@%s", dest, profile_name);
				host = profile_name;
			} else {
				switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_WARNING, "Cannot locate registered user %s@%s\n", dest, profile_name);
				cause = SWITCH_CAUSE_USER_NOT_REGISTERED;
				goto error;
			}
		} else {
			host++;

			if (!strchr(host, '.') || switch_true(switch_event_get_header(var_event, "sip_gethostbyname"))) {
				struct sockaddr_in sa;
				struct hostent *he = gethostbyname(host);
				char buf[50] = "", *tmp;
				const char *ip;

				if (he) {
					memcpy(&sa.sin_addr, he->h_addr, sizeof(struct in_addr));
					ip = switch_inet_ntop(AF_INET, &sa.sin_addr, buf, sizeof(buf));
					tmp = switch_string_replace(dest, host, ip);
					//host = switch_core_session_strdup(nsession, ip);
					//dest = switch_core_session_strdup(nsession, tmp);
					switch_channel_set_variable_printf(nchannel, "sip_route_uri", "sip:%s", tmp);
					free(tmp);
				}
			}

			tech_pvt->dest = switch_core_session_alloc(nsession, strlen(dest) + 5);
			tech_pvt->e_dest = switch_core_session_strdup(nsession, dest);
			switch_snprintf(tech_pvt->dest, strlen(dest) + 5, "sip:%s", dest);
		}
	}

	switch_channel_set_variable_printf(nchannel, "sip_local_network_addr", "%s", profile->extsipip ? profile->extsipip : profile->sipip);
	switch_channel_set_variable(nchannel, "sip_profile_name", profile_name);

	if (switch_stristr("fs_path", tech_pvt->dest)) {
		char *remote_host = NULL;
		const char *s;

		if ((s = switch_stristr("fs_path=", tech_pvt->dest))) {
			s += 8;
		}

		if (s) {
			remote_host = switch_core_session_strdup(nsession, s);
			switch_url_decode(remote_host);
		}
		if (!zstr(remote_host)) {
			switch_split_user_domain(remote_host, NULL, &tech_pvt->mparams.remote_ip);
		}
	}

	if (zstr(tech_pvt->mparams.remote_ip)) {
		switch_split_user_domain(switch_core_session_strdup(nsession, tech_pvt->dest), NULL, &tech_pvt->mparams.remote_ip);
	}

	if (dest_to) {
		if (strchr(dest_to, '@')) {
			tech_pvt->dest_to = switch_core_session_sprintf(nsession, "sip:%s", dest_to);
		} else {
			tech_pvt->dest_to = switch_core_session_sprintf(nsession, "sip:%s@%s", dest_to, host ? host : profile->sipip);
		}
	}

	if (!tech_pvt->dest_to) {
		tech_pvt->dest_to = tech_pvt->dest;
	}

	if (!zstr(tech_pvt->dest) && switch_stristr("transport=ws", tech_pvt->dest)) {
		switch_channel_set_variable(nchannel, "media_webrtc", "true");
		switch_core_session_set_ice(nsession);
	}


	sofia_glue_attach_private(nsession, profile, tech_pvt, dest);

	if (tech_pvt->local_url) {
		switch_channel_set_variable(nchannel, "sip_local_url", tech_pvt->local_url);
		if (profile->pres_type) {
			const char *presence_id = switch_channel_get_variable(nchannel, "presence_id");
			if (zstr(presence_id)) {
				switch_channel_set_variable(nchannel, "presence_id", tech_pvt->local_url);
			}
		}
	}
	switch_channel_set_variable(nchannel, "sip_destination_url", tech_pvt->dest);
#if 0
	dest_num = switch_core_session_strdup(nsession, dest);
	if ((p = strchr(dest_num, '@'))) {
		*p = '\0';

		if ((p = strrchr(dest_num, '/'))) {
			dest_num = p + 1;
		} else if ((p = (char *) switch_stristr("sip:", dest_num))) {
			dest_num = p + 4;
		} else if ((p = (char *) switch_stristr("sips:", dest_num))) {
			dest_num = p + 5;
		}
	}


	if (profile->pres_type) {
		char *sql;
		time_t now;

		const char *presence_id = switch_channel_get_variable(nchannel, "presence_id");
		const char *presence_data = switch_channel_get_variable(nchannel, "presence_data");

		if (zstr(presence_id)) {
			presence_id = switch_event_get_header(var_event, "presence_id");
		}

		if (zstr(presence_data)) {
			presence_data = switch_event_get_header(var_event, "presence_data");
		}

		now = switch_epoch_time_now(NULL);
		sql = switch_mprintf("insert into sip_dialogs (uuid,presence_id,presence_data,profile_name,hostname,rcd,call_info_state) "
							 "values ('%q', '%q', '%q', '%q', '%q', %ld, '')", switch_core_session_get_uuid(nsession),
							 switch_str_nil(presence_id), switch_str_nil(presence_data), profile->name, mod_sofia_globals.hostname, (long) now);
		sofia_glue_execute_sql_now(profile, &sql, SWITCH_TRUE);
	}
#endif

	caller_profile = switch_caller_profile_clone(nsession, outbound_profile);


	caller_profile->destination_number = switch_sanitize_number(caller_profile->destination_number);
	not_const = (char *) caller_profile->caller_id_name;
	caller_profile->caller_id_name = switch_sanitize_number(not_const);
	not_const = (char *) caller_profile->caller_id_number;
	caller_profile->caller_id_number = switch_sanitize_number(not_const);


	//caller_profile->destination_number = switch_core_strdup(caller_profile->pool, dest_num);
	switch_channel_set_caller_profile(nchannel, caller_profile);


	if (gateway_ptr && gateway_ptr->ob_vars) {
		switch_event_header_t *hp;
		for (hp = gateway_ptr->ob_vars->headers; hp; hp = hp->next) {
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "%s setting variable [%s]=[%s]\n",
							  switch_channel_get_name(nchannel), hp->name, hp->value);
			if (!strncmp(hp->name, "p:", 2)) {
				switch_channel_set_profile_var(nchannel, hp->name + 2, hp->value);
			} else {
				switch_channel_set_variable(nchannel, hp->name, hp->value);
			}
		}
	}




	sofia_set_flag_locked(tech_pvt, TFLAG_OUTBOUND);
	sofia_clear_flag_locked(tech_pvt, TFLAG_LATE_NEGOTIATION);
	if (switch_channel_get_state(nchannel) == CS_NEW) {
		switch_channel_set_state(nchannel, CS_INIT);
	}
	tech_pvt->caller_profile = caller_profile;
	*new_session = nsession;
	cause = SWITCH_CAUSE_SUCCESS;

	if ((hval = switch_event_get_header(var_event, "sip_enable_soa"))) {
		if (switch_true(hval)) {
			sofia_set_flag(tech_pvt, TFLAG_ENABLE_SOA);
		} else {
			sofia_clear_flag(tech_pvt, TFLAG_ENABLE_SOA);
		}
	}

	if ((hval = switch_event_get_header(var_event, "sip_auto_answer")) && switch_true(hval)) {
		switch_channel_set_variable_printf(nchannel, "sip_h_Call-Info", "<sip:%s>;answer-after=0", profile->sipip);
		switch_channel_set_variable(nchannel, "sip_invite_params", "intercom=true");
	}

	if (((hval = switch_event_get_header(var_event, "effective_callee_id_name")) ||
		 (hval = switch_event_get_header(var_event, "sip_callee_id_name"))) && !zstr(hval)) {
		caller_profile->callee_id_name = switch_core_strdup(caller_profile->pool, hval);
	}

	if (((hval = switch_event_get_header(var_event, "effective_callee_id_number")) ||
		 (hval = switch_event_get_header(var_event, "sip_callee_id_number"))) && !zstr(hval)) {
		caller_profile->callee_id_number = switch_core_strdup(caller_profile->pool, hval);
	}

	if (session) {
		const char *vval = NULL;

		switch_ivr_transfer_variable(session, nsession, SOFIA_REPLACES_HEADER);

		if (!(vval = switch_channel_get_variable(o_channel, "sip_copy_custom_headers")) || switch_true(vval)) {
			switch_ivr_transfer_variable(session, nsession, SOFIA_SIP_HEADER_PREFIX_T);
		}

		if (!(vval = switch_channel_get_variable(o_channel, "sip_copy_multipart")) || switch_true(vval)) {
			switch_ivr_transfer_variable(session, nsession, "sip_multipart");
		}
		switch_ivr_transfer_variable(session, nsession, "rtp_video_fmtp");
		switch_ivr_transfer_variable(session, nsession, "sip-force-contact");
		switch_ivr_transfer_variable(session, nsession, "sip_sticky_contact");
		if (!cid_locked) {
			switch_ivr_transfer_variable(session, nsession, "sip_cid_type");
		}

		if (switch_core_session_compare(session, nsession)) {
			/* It's another sofia channel! so lets cache what they use as a pt for telephone event so
			   we can keep it the same
			 */
			private_object_t *ctech_pvt;
			ctech_pvt = switch_core_session_get_private(session);
			switch_assert(ctech_pvt != NULL);
			tech_pvt->bte = ctech_pvt->te;
			tech_pvt->bcng_pt = ctech_pvt->cng_pt;
			if (!cid_locked) {
				tech_pvt->cid_type = ctech_pvt->cid_type;
			}

			if (sofia_test_flag(tech_pvt, TFLAG_ENABLE_SOA)) {
				sofia_set_flag(ctech_pvt, TFLAG_ENABLE_SOA);
			} else {
				sofia_clear_flag(ctech_pvt, TFLAG_ENABLE_SOA);
			}

			if (switch_channel_test_flag(o_channel, CF_ZRTP_PASSTHRU_REQ)) {
				const char *x = NULL;
				switch_core_media_pass_zrtp_hash2(session, nsession);
				switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "[zrtp_passthru] Setting a-leg inherit_codec=true\n");
				switch_channel_set_variable(o_channel, "inherit_codec", "true");
				if ((x = switch_channel_get_variable(o_channel, "ep_codec_string"))) {
					switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "[zrtp_passthru] Setting b-leg absolute_codec_string='%s'\n", x);
					switch_channel_set_variable(nchannel, "absolute_codec_string", x);
				}
			}

			/* SNARK: lets copy this across so we can see if we're the other leg of 3PCC + bypass_media... */
			if (sofia_test_flag(ctech_pvt, TFLAG_3PCC) && (switch_channel_test_flag(o_channel, CF_PROXY_MODE) || switch_channel_test_flag(o_channel, CF_PROXY_MEDIA))) {
				sofia_set_flag(tech_pvt, TFLAG_3PCC_INVITE);
				sofia_set_flag(tech_pvt, TFLAG_LATE_NEGOTIATION);
			} else {
				sofia_clear_flag(tech_pvt, TFLAG_3PCC_INVITE);
			}
		}

		switch_core_media_check_outgoing_proxy(nsession, session);

	}

	goto done;

  error:
	if (gateway_ptr) {
		sofia_reg_release_gateway(gateway_ptr);
	}

	if (nsession) {
		switch_core_session_destroy(&nsession);
	}
	if (pool) {
		*pool = NULL;
	}
  done:

	if (profile) {
		if (cause == SWITCH_CAUSE_SUCCESS) {
			profile->ob_calls++;
		} else {
			profile->ob_failed_calls++;
		}
		sofia_glue_release_profile(profile);
	}
	return cause;
}

static int notify_csta_callback(void *pArg, int argc, char **argv, char **columnNames)
{
	nua_handle_t *nh;
	sofia_profile_t *ext_profile = NULL, *profile = (sofia_profile_t *) pArg;
	int i = 0;
	char *user = argv[i++];
	char *host = argv[i++];
	char *contact_in = argv[i++];
	char *profile_name = argv[i++];
	char *call_id = argv[i++];
	char *full_from = argv[i++];
	char *full_to = argv[i++];
	int expires = atoi(argv[i++]);
	char *body = argv[i++];
	char *ct = argv[i++];
	char *id = NULL;
	char *contact;
	sip_cseq_t *cseq = NULL;
	uint32_t callsequence;
	sofia_destination_t *dst = NULL;
	char *route_uri = NULL;

	time_t epoch_now = switch_epoch_time_now(NULL);
	time_t expires_in = (expires - epoch_now);
	char *extra_headers = switch_mprintf("Subscription-State: active, %d\r\n", expires_in);

	if (profile_name && strcasecmp(profile_name, profile->name)) {
		if ((ext_profile = sofia_glue_find_profile(profile_name))) {
			profile = ext_profile;
		}
	}

	id = switch_mprintf("sip:%s@%s", user, host);
	switch_assert(id);
	contact = sofia_glue_get_url_from_contact(contact_in, 1);


	dst = sofia_glue_get_destination((char *) contact);

	if (dst->route_uri) {
		route_uri = sofia_glue_strip_uri(dst->route_uri);
	}

	callsequence = sofia_presence_get_cseq(profile);

	//nh = nua_handle(profile->nua, NULL, NUTAG_URL(dst->contact), SIPTAG_FROM_STR(id), SIPTAG_TO_STR(id), SIPTAG_CONTACT_STR(profile->url), TAG_END());
	nh = nua_handle(profile->nua, NULL, NUTAG_URL(dst->contact), SIPTAG_FROM_STR(full_to), SIPTAG_TO_STR(full_from), SIPTAG_CONTACT_STR(profile->url), TAG_END());
	cseq = sip_cseq_create(nh->nh_home, callsequence, SIP_METHOD_NOTIFY);

	nua_handle_bind(nh, &mod_sofia_globals.destroy_private);

	nua_notify(nh, NUTAG_NEWSUB(1),
			   TAG_IF(dst->route_uri, NUTAG_PROXY(route_uri)), TAG_IF(dst->route, SIPTAG_ROUTE_STR(dst->route)), TAG_IF(call_id, SIPTAG_CALL_ID_STR(call_id)),
			   SIPTAG_EVENT_STR("as-feature-event"), SIPTAG_CONTENT_TYPE_STR(ct), TAG_IF(!zstr(extra_headers), SIPTAG_HEADER_STR(extra_headers)), TAG_IF(!zstr(body), SIPTAG_PAYLOAD_STR(body)), SIPTAG_CSEQ(cseq), TAG_END());



	switch_safe_free(route_uri);
	sofia_glue_free_destination(dst);

	free(id);
	free(contact);

	if (ext_profile) {
		sofia_glue_release_profile(ext_profile);
	}

	return 0;
}

static int notify_callback(void *pArg, int argc, char **argv, char **columnNames)
{

	nua_handle_t *nh;
	sofia_profile_t *ext_profile = NULL, *profile = (sofia_profile_t *) pArg;
	char *user = argv[0];
	char *host = argv[1];
	char *contact_in = argv[2];
	char *profile_name = argv[3];
	char *ct = argv[4];
	char *es = argv[5];
	char *body = argv[6];
	char *id = NULL;
	char *contact;
	sofia_destination_t *dst = NULL;
	char *route_uri = NULL;

	if (profile_name && strcasecmp(profile_name, profile->name)) {
		if ((ext_profile = sofia_glue_find_profile(profile_name))) {
			profile = ext_profile;
		}
	}

	id = switch_mprintf("sip:%s@%s", user, host);
	switch_assert(id);
	contact = sofia_glue_get_url_from_contact(contact_in, 1);


	dst = sofia_glue_get_destination((char *) contact);

	if (dst->route_uri) {
		route_uri = sofia_glue_strip_uri(dst->route_uri);
	}

	nh = nua_handle(profile->nua, NULL, NUTAG_URL(dst->contact), SIPTAG_FROM_STR(id), SIPTAG_TO_STR(id), SIPTAG_CONTACT_STR(profile->url), TAG_END());

	nua_handle_bind(nh, &mod_sofia_globals.destroy_private);

	nua_notify(nh, NUTAG_NEWSUB(1), SIPTAG_SUBSCRIPTION_STATE_STR("terminated;reason=noresource"),
			   TAG_IF(dst->route_uri, NUTAG_PROXY(route_uri)), TAG_IF(dst->route, SIPTAG_ROUTE_STR(dst->route)),
			   SIPTAG_EVENT_STR(es), SIPTAG_CONTENT_TYPE_STR(ct), TAG_IF(!zstr(body), SIPTAG_PAYLOAD_STR(body)), TAG_END());


	switch_safe_free(route_uri);
	sofia_glue_free_destination(dst);

	free(id);
	free(contact);

	if (ext_profile) {
		sofia_glue_release_profile(ext_profile);
	}

	return 0;
}

static void general_event_handler(switch_event_t *event)
{
	switch (event->event_id) {
	case SWITCH_EVENT_NOTIFY:
		{
			const char *profile_name = switch_event_get_header(event, "profile");
			const char *ct = switch_event_get_header(event, "content-type");
			const char *es = switch_event_get_header(event, "event-string");
			const char *user = switch_event_get_header(event, "user");
			const char *host = switch_event_get_header(event, "host");
			const char *call_id = switch_event_get_header(event, "call-id");
			const char *uuid = switch_event_get_header(event, "uuid");
			const char *body = switch_event_get_body(event);
			const char *to_uri = switch_event_get_header(event, "to-uri");
			const char *from_uri = switch_event_get_header(event, "from-uri");
			const char *extra_headers = switch_event_get_header(event, "extra-headers");
			const char *contact_uri = switch_event_get_header(event, "contact-uri");
			const char *no_sub_state = switch_event_get_header(event, "no-sub-state");

			sofia_profile_t *profile;

			if (contact_uri) {
				if (!es) {
					es = "message-summary";
				}

				if (!ct) {
					ct = "application/simple-message-summary";
				}

				if (!profile_name) {
					profile_name = "default";
				}

				if (!(profile = sofia_glue_find_profile(profile_name))) {
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Can't find profile %s\n", profile_name);
					return;
				}

				if (to_uri && from_uri) {
					sofia_destination_t *dst = NULL;
					nua_handle_t *nh;
					char *route_uri = NULL;
					char *sip_sub_st = NULL;

					dst = sofia_glue_get_destination((char *) contact_uri);

					if (dst->route_uri) {
						route_uri = sofia_glue_strip_uri(dst->route_uri);
					}

					nh = nua_handle(profile->nua,
									NULL,
									NUTAG_URL(dst->contact),
									SIPTAG_FROM_STR(from_uri),
									SIPTAG_TO_STR(to_uri),
									SIPTAG_CONTACT_STR(profile->url),
									TAG_END());

					nua_handle_bind(nh, &mod_sofia_globals.destroy_private);

					if (!switch_true(no_sub_state)) {
						sip_sub_st = "terminated;reason=noresource";
					}

					nua_notify(nh,
							   NUTAG_NEWSUB(1), TAG_IF(sip_sub_st, SIPTAG_SUBSCRIPTION_STATE_STR(sip_sub_st)),
							   TAG_IF(dst->route_uri, NUTAG_PROXY(dst->route_uri)), TAG_IF(dst->route, SIPTAG_ROUTE_STR(dst->route)), TAG_IF(call_id, SIPTAG_CALL_ID_STR(call_id)),
							   SIPTAG_EVENT_STR(es), TAG_IF(ct, SIPTAG_CONTENT_TYPE_STR(ct)), TAG_IF(!zstr(body), SIPTAG_PAYLOAD_STR(body)),
							   TAG_IF(!zstr(extra_headers), SIPTAG_HEADER_STR(extra_headers)), TAG_END());

					switch_safe_free(route_uri);
					sofia_glue_free_destination(dst);

					sofia_glue_release_profile(profile);
				}

				return;
			} else if (to_uri || from_uri) {
				if (!es) {
					es = "message-summary";
				}

				if (!ct) {
					ct = "application/simple-message-summary";
				}

				if (!profile_name) {
					profile_name = "default";
				}

				if (!(profile = sofia_glue_find_profile(profile_name))) {
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Can't find profile %s\n", profile_name);
					return;
				}

				if (to_uri && from_uri && ct && es) {
					sofia_destination_t *dst = NULL;
					nua_handle_t *nh;
					char *route_uri = NULL;

					dst = sofia_glue_get_destination((char *) to_uri);

					if (dst->route_uri) {
						route_uri = sofia_glue_strip_uri(dst->route_uri);
					}


					nh = nua_handle(profile->nua,
									NULL,
									NUTAG_URL(to_uri),
									SIPTAG_FROM_STR(from_uri),
									SIPTAG_TO_STR(to_uri),
									SIPTAG_CONTACT_STR(profile->url),
									TAG_END());

					nua_handle_bind(nh, &mod_sofia_globals.destroy_private);

					nua_notify(nh,
							   NUTAG_NEWSUB(1), SIPTAG_SUBSCRIPTION_STATE_STR("terminated;reason=noresource"),
							   TAG_IF(dst->route_uri, NUTAG_PROXY(dst->route_uri)), TAG_IF(dst->route, SIPTAG_ROUTE_STR(dst->route)),
							   SIPTAG_EVENT_STR(es), TAG_IF(ct, SIPTAG_CONTENT_TYPE_STR(ct)), TAG_IF(!zstr(body), SIPTAG_PAYLOAD_STR(body)),
							   TAG_IF(!zstr(extra_headers), SIPTAG_HEADER_STR(extra_headers)), TAG_END());


					switch_safe_free(route_uri);
					sofia_glue_free_destination(dst);

					sofia_glue_release_profile(profile);
				}

				return;
			}

			if (uuid && ct && es) {
				switch_core_session_t *session;
				private_object_t *tech_pvt;

				if ((session = switch_core_session_locate(uuid))) {
					if ((tech_pvt = switch_core_session_get_private(session))) {
						nua_notify(tech_pvt->nh,
								   NUTAG_NEWSUB(1), SIPTAG_SUBSCRIPTION_STATE_STR("terminated;reason=noresource"),
								   SIPTAG_EVENT_STR(es), SIPTAG_CONTENT_TYPE_STR(ct), TAG_IF(!zstr(body), SIPTAG_PAYLOAD_STR(body)), TAG_END());
					}
					switch_core_session_rwunlock(session);
				}
			} else if (profile_name && ct && es && user && host && (profile = sofia_glue_find_profile(profile_name))) {
				char *sql;

				if (call_id) {
					sql = switch_mprintf("select sip_user,sip_host,contact,profile_name,'%q','%q','%q' "
										 "from sip_registrations where call_id='%q'", ct, es, switch_str_nil(body), call_id);
				} else {
					if (!strcasecmp(es, "message-summary")) {
						sql = switch_mprintf("select sip_user,sip_host,contact,profile_name,'%q','%q','%q' "
											 "from sip_registrations where mwi_user='%s' and mwi_host='%q'",
											 ct, es, switch_str_nil(body), switch_str_nil(user), switch_str_nil(host)
							);
					} else {
						sql = switch_mprintf("select sip_user,sip_host,contact,profile_name,'%q','%q','%q' "
											 "from sip_registrations where sip_user='%s' and sip_host='%q'",
											 ct, es, switch_str_nil(body), switch_str_nil(user), switch_str_nil(host)
							);

					}
				}


				switch_mutex_lock(profile->dbh_mutex);
				sofia_glue_execute_sql_callback(profile, NULL, sql, notify_callback, profile);
				switch_mutex_unlock(profile->dbh_mutex);
				sofia_glue_release_profile(profile);

				free(sql);
			}

		}
		break;
	case SWITCH_EVENT_PHONE_FEATURE:
		{
			const char *profile_name = switch_event_get_header(event, "profile");
			const char *user = switch_event_get_header(event, "user");
			const char *host = switch_event_get_header(event, "host");
			const char *call_id = switch_event_get_header(event, "call-id");
			const char *csta_event = switch_event_get_header(event, "Feature-Event");

			char *ct = "application/x-as-feature-event+xml";

			sofia_profile_t *profile;

			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Phone Feature NOTIFY\n");
			if (profile_name && user && host && (profile = sofia_glue_find_profile(profile_name))) {
				char *sql;
				switch_stream_handle_t stream = { 0 };
				SWITCH_STANDARD_STREAM(stream);

				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "we have all required vars\n");

				if (csta_event) {
					if (!strcmp(csta_event, "init")) {
						char *boundary_string = "UniqueFreeSWITCHBoundary";
						switch_stream_handle_t dnd_stream = { 0 };

						switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Sending multipart with DND and CFWD\n");

						if (switch_event_get_header(event, "forward_immediate")) {
							switch_stream_handle_t fwdi_stream = { 0 };
							SWITCH_STANDARD_STREAM(fwdi_stream);
							write_csta_xml_chunk(event, fwdi_stream, "ForwardingEvent", "forwardImmediate");
							switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "[%s] is %d bytes long\n", (char *)fwdi_stream.data, (int)strlen(fwdi_stream.data));
							stream.write_function(&stream, "--%s\r\nContent-Type: application/x-as-feature-event+xml\r\nContent-Length: %d\r\nContent-ID: <%si@%s>\r\n\r\n%s", boundary_string, strlen(fwdi_stream.data), user, host, fwdi_stream.data);
							switch_safe_free(fwdi_stream.data);
						}
						if (switch_event_get_header(event, "forward_busy")) {
							switch_stream_handle_t fwdb_stream = { 0 };
							SWITCH_STANDARD_STREAM(fwdb_stream);
							write_csta_xml_chunk(event, fwdb_stream, "ForwardingEvent", "forwardBusy");
							switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "[%s] is %d bytes long\n", (char *)fwdb_stream.data, (int)strlen(fwdb_stream.data));
							stream.write_function(&stream, "--%s\r\nContent-Type: application/x-as-feature-event+xml\r\nContent-Length: %d\r\nContent-ID: <%sb@%s>\r\n\r\n%s", boundary_string, strlen(fwdb_stream.data), user, host, fwdb_stream.data);
							switch_safe_free(fwdb_stream.data);
						}
						if (switch_event_get_header(event, "forward_no_answer")) {
							switch_stream_handle_t fwdna_stream = { 0 };
							SWITCH_STANDARD_STREAM(fwdna_stream);
							write_csta_xml_chunk(event, fwdna_stream, "ForwardingEvent", "forwardNoAns");
							switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "[%s] is %d bytes long\n", (char *)fwdna_stream.data, (int)strlen(fwdna_stream.data));
							stream.write_function(&stream, "--%s\r\nContent-Type: application/x-as-feature-event+xml\r\nContent-Length: %d\r\nContent-ID: <%sn@%s>\r\n\r\n%s", boundary_string, strlen(fwdna_stream.data), user, host, fwdna_stream.data);
							switch_safe_free(fwdna_stream.data);
						}

						SWITCH_STANDARD_STREAM(dnd_stream);
						write_csta_xml_chunk(event, dnd_stream, "DoNotDisturbEvent", NULL);
						switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "[%s] is %d bytes long\n", (char *)dnd_stream.data, (int)strlen(dnd_stream.data));
						stream.write_function(&stream, "--%s\r\nContent-Type: application/x-as-feature-event+xml\r\nContent-Length: %d\r\nContent-ID: <%sd@%s>\r\n\r\n%s", boundary_string, strlen(dnd_stream.data), user, host, dnd_stream.data);
						switch_safe_free(dnd_stream.data);

						stream.write_function(&stream, "--%s--\r\n", boundary_string);

						ct = switch_mprintf("multipart/mixed; boundary=\"%s\"", boundary_string);
					} else {
						char *fwd_type = NULL;

						if (switch_event_get_header(event, "forward_immediate")) {
							fwd_type = "forwardImmediate";
						} else if (switch_event_get_header(event, "forward_busy")) {
							fwd_type = "forwardBusy";
						} else if (switch_event_get_header(event, "forward_no_answer")) {
							fwd_type = "forwardNoAns";
						}

						// this will need some work to handle the different types of forwarding events
						write_csta_xml_chunk(event, stream, csta_event, fwd_type);
					}
				}

				if (call_id) {
					sql = switch_mprintf("select sip_user,sip_host,contact,profile_name,call_id,full_from,full_to,expires,'%q', '%q' "
										 "from sip_subscriptions where event='as-feature-event' and call_id='%q'", stream.data, ct, call_id);
				} else {
					sql = switch_mprintf("select sip_user,sip_host,contact,profile_name,call_id,full_from,full_to,expires,'%q', '%q' "
										 "from sip_subscriptions where event='as-feature-event' and sip_user='%s' and sip_host='%q'", stream.data, ct, switch_str_nil(user), switch_str_nil(host)
										 );
				}

				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Query: %s\n", sql);
				switch_safe_free(stream.data);
				switch_mutex_lock(profile->ireg_mutex);
				sofia_glue_execute_sql_callback(profile, NULL, sql, notify_csta_callback, profile);
				switch_mutex_unlock(profile->ireg_mutex);
				sofia_glue_release_profile(profile);

				free(sql);
			} else {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "missing something\n");
			}
		}
		break;
	case SWITCH_EVENT_SEND_MESSAGE:
		{
			const char *profile_name = switch_event_get_header(event, "profile");
			const char *ct = switch_event_get_header(event, "content-type");
			const char *user = switch_event_get_header(event, "user");
			const char *host = switch_event_get_header(event, "host");
			const char *subject = switch_event_get_header(event, "subject");
			const char *uuid = switch_event_get_header(event, "uuid");
			const char *body = switch_event_get_body(event);

			sofia_profile_t *profile;
			nua_handle_t *nh;

			if (ct && user && host) {
				char *id = NULL;
				char *contact, *p;
				switch_console_callback_match_t *list = NULL;
				switch_console_callback_match_node_t *m;

				if (!profile_name || !(profile = sofia_glue_find_profile(profile_name))) {
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Can't find profile %s\n", profile_name);
					return;
				}

				if (!(list = sofia_reg_find_reg_url_multi(profile, user, host))) {
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Can't find registered user %s@%s\n", user, host);
					return;
				}

				id = switch_mprintf("sip:%s@%s", user, host);

				switch_assert(id);

				for (m = list->head; m; m = m->next) {
					contact = sofia_glue_get_url_from_contact(m->val, 0);

					if ((p = strstr(contact, ";fs_"))) {
						*p = '\0';
					}

					nh = nua_handle(profile->nua,
									NULL, NUTAG_URL(contact), SIPTAG_FROM_STR(id), SIPTAG_TO_STR(id), SIPTAG_CONTACT_STR(profile->url), TAG_END());

					nua_message(nh, NUTAG_NEWSUB(1), SIPTAG_CONTENT_TYPE_STR(ct),
								TAG_IF(!zstr(body), SIPTAG_PAYLOAD_STR(body)), TAG_IF(!zstr(subject), SIPTAG_SUBJECT_STR(subject)), TAG_END());
				}

				free(id);
				switch_console_free_matches(&list);

				sofia_glue_release_profile(profile);
			} else if (uuid && ct) {
				switch_core_session_t *session;
				private_object_t *tech_pvt;

				if ((session = switch_core_session_locate(uuid))) {
					if ((tech_pvt = switch_core_session_get_private(session))) {
						nua_message(tech_pvt->nh,
									SIPTAG_CONTENT_TYPE_STR(ct), SIPTAG_PAYLOAD_STR(body),
									TAG_IF(!zstr(body), SIPTAG_PAYLOAD_STR(body)), TAG_IF(!zstr(subject), SIPTAG_SUBJECT_STR(subject)), TAG_END());
					}
					switch_core_session_rwunlock(session);
				}
			}
		}
		break;
	case SWITCH_EVENT_SEND_INFO:
		{
			const char *profile_name = switch_event_get_header(event, "profile");
			const char *ct = switch_event_get_header(event, "content-type");
			const char *cd = switch_event_get_header(event, "content-disposition");
			const char *to_uri = switch_event_get_header(event, "to-uri");
			const char *local_user_full = switch_event_get_header(event, "local-user");
			const char *from_uri = switch_event_get_header(event, "from-uri");
			const char *call_info = switch_event_get_header(event, "call-info");
			const char *alert_info = switch_event_get_header(event, "alert-info");
			const char *call_id = switch_event_get_header(event, "call-id");
			const char *body = switch_event_get_body(event);
			sofia_profile_t *profile = NULL;
			nua_handle_t *nh;
			char *local_dup = NULL;
			char *local_user, *local_host;
			char buf[1024] = "";
			char *p;

			if (!profile_name) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Missing Profile Name\n");
				goto done;
			}

			if (!call_id && !to_uri && !local_user_full) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Missing To-URI header\n");
				goto done;
			}

			if (!call_id && !from_uri) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Missing From-URI header\n");
				goto done;
			}


			if (!(profile = sofia_glue_find_profile(profile_name))) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Can't find profile %s\n", profile_name);
				goto done;
			}

			if (call_id) {
				nh = nua_handle_by_call_id(profile->nua, call_id);

				if (!nh) {
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Invalid Call-ID %s\n", call_id);
					goto done;
				}
			} else {
				if (local_user_full) {
					local_dup = strdup(local_user_full);
					local_user = local_dup;
					if ((local_host = strchr(local_user, '@'))) {
						*local_host++ = '\0';
					}

					if (!local_user || !local_host || !sofia_reg_find_reg_url(profile, local_user, local_host, buf, sizeof(buf))) {
						switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Can't find local user\n");
						goto done;
					}

					to_uri = sofia_glue_get_url_from_contact(buf, 0);

					if ((p = strstr(to_uri, ";fs_"))) {
						*p = '\0';
					}

				}

				nh = nua_handle(profile->nua,
								NULL, NUTAG_URL(to_uri), SIPTAG_FROM_STR(from_uri), SIPTAG_TO_STR(to_uri), SIPTAG_CONTACT_STR(profile->url), TAG_END());

				nua_handle_bind(nh, &mod_sofia_globals.destroy_private);
			}

			nua_info(nh,
					 TAG_IF(ct, SIPTAG_CONTENT_TYPE_STR(ct)),
					 TAG_IF(cd, SIPTAG_CONTENT_DISPOSITION_STR(cd)),
					 TAG_IF(alert_info, SIPTAG_ALERT_INFO_STR(alert_info)),
					 TAG_IF(call_info, SIPTAG_CALL_INFO_STR(call_info)), TAG_IF(!zstr(body), SIPTAG_PAYLOAD_STR(body)), TAG_END());

			if (call_id && nh) {
				nua_handle_unref(nh);
			}

			if (profile) {
				sofia_glue_release_profile(profile);
			}

		  done:

			switch_safe_free(local_dup);

		}
		break;
	case SWITCH_EVENT_TRAP:
		{
			const char *cond = switch_event_get_header(event, "condition");
			switch_hash_index_t *hi;
			const void *var;
			void *val;
			sofia_profile_t *profile;

			if (zstr(cond)) {
				cond = "";
			}

			if (!strcmp(cond, "network-external-address-change") && mod_sofia_globals.auto_restart) {
				const char *old_ip4 = switch_event_get_header_nil(event, "network-external-address-previous-v4");
				const char *new_ip4 = switch_event_get_header_nil(event, "network-external-address-change-v4");

				switch_mutex_lock(mod_sofia_globals.hash_mutex);
				if (mod_sofia_globals.profile_hash && !zstr(old_ip4) && !zstr(new_ip4)) {
					for (hi = switch_core_hash_first(mod_sofia_globals.profile_hash); hi; hi = switch_core_hash_next(&hi)) {
						switch_core_hash_this(hi, &var, NULL, &val);

						if ((profile = (sofia_profile_t *) val)) {
							if (!zstr(profile->extsipip) && !strcmp(profile->extsipip, old_ip4)) {
								profile->extsipip = switch_core_strdup(profile->pool, new_ip4);
							}

							if (!zstr(profile->extrtpip) && !strcmp(profile->extrtpip, old_ip4)) {
								profile->extrtpip = switch_core_strdup(profile->pool, new_ip4);
							}
						}
					}
				}
				switch_mutex_unlock(mod_sofia_globals.hash_mutex);
				sofia_glue_restart_all_profiles();
			} else if (!strcmp(cond, "network-address-change") && mod_sofia_globals.auto_restart) {
				const char *old_ip4 = switch_event_get_header_nil(event, "network-address-previous-v4");
				const char *new_ip4 = switch_event_get_header_nil(event, "network-address-change-v4");
				const char *old_ip6 = switch_event_get_header_nil(event, "network-address-previous-v6");
				const char *new_ip6 = switch_event_get_header_nil(event, "network-address-change-v6");

				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "EVENT_TRAP: IP change detected\n");
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "IP change detected [%s]->[%s] [%s]->[%s]\n", old_ip4, new_ip4, old_ip6, new_ip6);

				strncpy(mod_sofia_globals.guess_ip, new_ip4, sizeof(mod_sofia_globals.guess_ip));

				switch_mutex_lock(mod_sofia_globals.hash_mutex);
				if (mod_sofia_globals.profile_hash) {
					for (hi = switch_core_hash_first(mod_sofia_globals.profile_hash); hi; hi = switch_core_hash_next(&hi)) {
						int rb = 0;
						uint32_t x = 0;
						switch_core_hash_this(hi, &var, NULL, &val);
						if ((profile = (sofia_profile_t *) val) && profile->auto_restart) {
							if (!strcmp(profile->sipip, old_ip4)) {
								profile->sipip = switch_core_strdup(profile->pool, new_ip4);
								rb++;
							}

							for (x = 0; x < profile->rtpip_index; x++) {

								if (!strcmp(profile->rtpip[x], old_ip4)) {
									profile->rtpip[x] = switch_core_strdup(profile->pool, new_ip4);
									rb++;
								}

								if (!strcmp(profile->rtpip[x], old_ip6)) {
									profile->rtpip[x] = switch_core_strdup(profile->pool, new_ip6);
									rb++;
								}
							}


							if (!strcmp(profile->sipip, old_ip6)) {
								profile->sipip = switch_core_strdup(profile->pool, new_ip6);
								rb++;
							}

							if (rb) {
								sofia_set_pflag_locked(profile, PFLAG_RESPAWN);
								sofia_clear_pflag_locked(profile, PFLAG_RUNNING);
							}
						}
					}
				}
				switch_mutex_unlock(mod_sofia_globals.hash_mutex);
			}

		}
		break;
	default:
		break;
	}
}

void write_csta_xml_chunk(switch_event_t *event, switch_stream_handle_t stream, const char *csta_event, char *fwdtype)
{
	const char *device = switch_event_get_header(event, "device");
	
	switch_assert(csta_event);

	stream.write_function(&stream, "<?xml version=\"1.0\" encoding=\"ISO-8859-1\"?>\n<%s xmlns=\"http://www.ecma-international.org/standards/ecma-323/csta/ed3\">\n", csta_event);

	if (device) {
		stream.write_function(&stream, "  <device>%s</device>\n", device);
	}

	if (!strcmp(csta_event, "DoNotDisturbEvent")) {
		const char *dndstatus = switch_event_get_header(event, "doNotDisturbOn");

		if (dndstatus) {
			stream.write_function(&stream, "  <doNotDisturbOn>%s</doNotDisturbOn>\n", dndstatus);
		}
	} else if(!strcmp(csta_event, "ForwardingEvent")) {
		const char *fwdstatus = NULL;
		const char *fwdto = NULL;
		const char *ringcount = NULL;

		if (fwdtype && !zstr(fwdtype)) {
			if (!strcmp("forwardImmediate", fwdtype)) {
				fwdto = switch_event_get_header(event, "forward_immediate");
				fwdstatus = switch_event_get_header(event, "forward_immediate_enabled");
			} else if (!strcmp("forwardBusy", fwdtype)) {
				fwdto = switch_event_get_header(event, "forward_busy");
				fwdstatus = switch_event_get_header(event, "forward_busy_enabled");
			} else if (!strcmp("forwardNoAns", fwdtype)) {
				fwdto = switch_event_get_header(event, "forward_no_answer");
				fwdstatus = switch_event_get_header(event, "forward_no_answer_enabled");
				ringcount = switch_event_get_header(event, "ringCount");
			}

			if (fwdtype) {
				stream.write_function(&stream, "  <forwardingType>%s</forwardingType>\n", fwdtype);
			}
			if (fwdstatus) {
				stream.write_function(&stream, "  <forwardStatus>%s</forwardStatus>\n", fwdstatus);
			}
			if (fwdto) {
				stream.write_function(&stream, "  <forwardTo>%s</forwardTo>\n", fwdto);
			}
			if (ringcount) {
				stream.write_function(&stream, "  <ringCount>%s</ringCount>\n", ringcount);
			}
		}
	}

	stream.write_function(&stream, "</%s>\n", csta_event);
}

switch_status_t list_profiles_full(const char *line, const char *cursor, switch_console_callback_match_t **matches, switch_bool_t show_aliases)
{
	sofia_profile_t *profile = NULL;
	switch_hash_index_t *hi;
	void *val;
	const void *vvar;
	switch_console_callback_match_t *my_matches = NULL;
	switch_status_t status = SWITCH_STATUS_FALSE;

	switch_mutex_lock(mod_sofia_globals.hash_mutex);
	for (hi = switch_core_hash_first(mod_sofia_globals.profile_hash); hi; hi = switch_core_hash_next(&hi)) {
		switch_core_hash_this(hi, &vvar, NULL, &val);

		profile = (sofia_profile_t *) val;
		if (!show_aliases && strcmp((char *)vvar, profile->name)) {
			continue;
		}

		if (sofia_test_pflag(profile, PFLAG_RUNNING)) {
			switch_console_push_match(&my_matches, (const char *) vvar);
		}
	}
	switch_mutex_unlock(mod_sofia_globals.hash_mutex);

	if (my_matches) {
		*matches = my_matches;
		status = SWITCH_STATUS_SUCCESS;
	}


	return status;
}

switch_status_t list_profiles(const char *line, const char *cursor, switch_console_callback_match_t **matches)
{
	return list_profiles_full(line, cursor, matches, SWITCH_TRUE);
}

static switch_status_t list_gateways(const char *line, const char *cursor, switch_console_callback_match_t **matches)
{
	sofia_profile_t *profile = NULL;
	switch_hash_index_t *hi;
	void *val;
	const void *vvar;
	switch_console_callback_match_t *my_matches = NULL;
	switch_status_t status = SWITCH_STATUS_FALSE;

	switch_mutex_lock(mod_sofia_globals.hash_mutex);
	for (hi = switch_core_hash_first(mod_sofia_globals.profile_hash); hi; hi = switch_core_hash_next(&hi)) {
		switch_core_hash_this(hi, &vvar, NULL, &val);
		profile = (sofia_profile_t *) val;
		if (sofia_test_pflag(profile, PFLAG_RUNNING)) {
			sofia_gateway_t *gp;
			switch_mutex_lock(profile->gw_mutex);
			for (gp = profile->gateways; gp; gp = gp->next) {
				switch_console_push_match(&my_matches, gp->name);
			}
			switch_mutex_unlock(profile->gw_mutex);
		}
	}
	switch_mutex_unlock(mod_sofia_globals.hash_mutex);

	if (my_matches) {
		*matches = my_matches;
		status = SWITCH_STATUS_SUCCESS;
	}

	return status;
}


static switch_status_t list_profile_gateway(const char *line, const char *cursor, switch_console_callback_match_t **matches)
{
	sofia_profile_t *profile = NULL;
	switch_console_callback_match_t *my_matches = NULL;
	switch_status_t status = SWITCH_STATUS_FALSE;
	char *dup = NULL;
	//int argc;
	char *argv[4] = { 0 };

	if (zstr(line)) {
		return SWITCH_STATUS_FALSE;
	}

	dup = strdup(line);
	switch_split(dup, ' ', argv);

	if (zstr(argv[2]) || !strcmp(argv[2], " ")) {
		goto end;
	}

	if ((profile = sofia_glue_find_profile(argv[2]))) {
		sofia_gateway_t *gp;
		for (gp = profile->gateways; gp; gp = gp->next) {
			switch_console_push_match(&my_matches, gp->name);
		}
		sofia_glue_release_profile(profile);
	}

	if (my_matches) {
		*matches = my_matches;
		status = SWITCH_STATUS_SUCCESS;
	}

  end:

	switch_safe_free(dup);

	return status;
}

SWITCH_STANDARD_APP(sofia_sla_function)
{
	private_object_t *tech_pvt;
	switch_core_session_t *bargee_session;
	switch_channel_t *channel = switch_core_session_get_channel(session);

	if (zstr(data)) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "Usage: <uuid>\n");
		return;
	}

	switch_channel_answer(channel);

	if ((bargee_session = switch_core_session_locate((char *)data))) {
		if (bargee_session == session) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "BARGE: %s (cannot barge on myself)\n", (char *) data);
		} else {

			if (switch_core_session_check_interface(bargee_session, sofia_endpoint_interface)) {
				tech_pvt = switch_core_session_get_private(bargee_session);
				switch_channel_clear_flag(tech_pvt->channel, CF_SLA_BARGING);
				switch_channel_set_flag(tech_pvt->channel, CF_SLA_BARGE);
				switch_ivr_transfer_variable(bargee_session, session, SWITCH_SIGNAL_BOND_VARIABLE);
			}

			if (switch_core_session_check_interface(session, sofia_endpoint_interface)) {
				tech_pvt = switch_core_session_get_private(session);
				switch_channel_set_flag(tech_pvt->channel, CF_SLA_BARGING);
			}

			switch_channel_set_variable(channel, "sip_barging_uuid", (char *)data);
		}

		switch_core_session_rwunlock(bargee_session);
	}

	switch_channel_execute_on(channel, "execute_on_sip_barge");

	switch_ivr_eavesdrop_session(session, data, NULL, ED_MUX_READ | ED_MUX_WRITE | ED_COPY_DISPLAY);
}


SWITCH_MODULE_LOAD_FUNCTION(mod_sofia_load)
{
	switch_chat_interface_t *chat_interface;
	switch_api_interface_t *api_interface;
	switch_management_interface_t *management_interface;
	switch_application_interface_t *app_interface;
	struct in_addr in;

	memset(&mod_sofia_globals, 0, sizeof(mod_sofia_globals));
	mod_sofia_globals.destroy_private.destroy_nh = 1;
	mod_sofia_globals.destroy_private.is_static = 1;
	mod_sofia_globals.keep_private.is_static = 1;
	mod_sofia_globals.pool = pool;
	switch_mutex_init(&mod_sofia_globals.mutex, SWITCH_MUTEX_NESTED, mod_sofia_globals.pool);

	switch_find_local_ip(mod_sofia_globals.guess_ip, sizeof(mod_sofia_globals.guess_ip), &mod_sofia_globals.guess_mask, AF_INET);
	in.s_addr = mod_sofia_globals.guess_mask;
	switch_set_string(mod_sofia_globals.guess_mask_str, inet_ntoa(in));

	strcpy(mod_sofia_globals.hostname, switch_core_get_switchname());


	switch_core_hash_init(&mod_sofia_globals.profile_hash);
	switch_core_hash_init(&mod_sofia_globals.gateway_hash);
	switch_mutex_init(&mod_sofia_globals.hash_mutex, SWITCH_MUTEX_NESTED, mod_sofia_globals.pool);

	switch_mutex_lock(mod_sofia_globals.mutex);
	mod_sofia_globals.running = 1;
	switch_mutex_unlock(mod_sofia_globals.mutex);

	mod_sofia_globals.auto_nat = (switch_nat_get_type() ? 1 : 0);

	switch_queue_create(&mod_sofia_globals.presence_queue, SOFIA_QUEUE_SIZE, mod_sofia_globals.pool);

	mod_sofia_globals.cpu_count = switch_core_cpu_count();
	mod_sofia_globals.max_msg_queues = (mod_sofia_globals.cpu_count / 2) + 1;
	if (mod_sofia_globals.max_msg_queues < 2) {
		mod_sofia_globals.max_msg_queues = 2;
	}

	if (mod_sofia_globals.max_msg_queues > SOFIA_MAX_MSG_QUEUE) {
		mod_sofia_globals.max_msg_queues = SOFIA_MAX_MSG_QUEUE;
	}

	switch_queue_create(&mod_sofia_globals.msg_queue, SOFIA_MSG_QUEUE_SIZE * mod_sofia_globals.max_msg_queues, mod_sofia_globals.pool);

	/* start one message thread */
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "Starting initial message thread.\n");
	sofia_msg_thread_start(0);


	if (sofia_init() != SWITCH_STATUS_SUCCESS) {
		return SWITCH_STATUS_GENERR;
	}

	if (config_sofia(SOFIA_CONFIG_LOAD, NULL) != SWITCH_STATUS_SUCCESS) {
		mod_sofia_globals.running = 0;
		return SWITCH_STATUS_GENERR;
	}

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Waiting for profiles to start\n");
	switch_yield(1500000);

	if (switch_event_bind(modname, SWITCH_EVENT_CUSTOM, MULTICAST_EVENT, event_handler, NULL) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Couldn't bind!\n");
		return SWITCH_STATUS_TERM;
	}

	if (switch_event_bind(modname, SWITCH_EVENT_CONFERENCE_DATA, SWITCH_EVENT_SUBCLASS_ANY, sofia_presence_event_handler, NULL) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Couldn't bind!\n");
		return SWITCH_STATUS_GENERR;
	}

	if (switch_event_bind(modname, SWITCH_EVENT_PRESENCE_IN, SWITCH_EVENT_SUBCLASS_ANY, sofia_presence_event_handler, NULL) != SWITCH_STATUS_SUCCESS) {

		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Couldn't bind!\n");
		return SWITCH_STATUS_GENERR;
	}

	if (switch_event_bind(modname, SWITCH_EVENT_PRESENCE_OUT, SWITCH_EVENT_SUBCLASS_ANY, sofia_presence_event_handler, NULL) != SWITCH_STATUS_SUCCESS) {

		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Couldn't bind!\n");
		return SWITCH_STATUS_GENERR;
	}

	if (switch_event_bind(modname, SWITCH_EVENT_PRESENCE_PROBE, SWITCH_EVENT_SUBCLASS_ANY, sofia_presence_event_handler, NULL) != SWITCH_STATUS_SUCCESS) {

		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Couldn't bind!\n");
		return SWITCH_STATUS_GENERR;
	}

	if (switch_event_bind(modname, SWITCH_EVENT_ROSTER, SWITCH_EVENT_SUBCLASS_ANY, sofia_presence_event_handler, NULL) != SWITCH_STATUS_SUCCESS) {

		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Couldn't bind!\n");
		return SWITCH_STATUS_GENERR;
	}

	if (switch_event_bind(modname, SWITCH_EVENT_MESSAGE_WAITING, SWITCH_EVENT_SUBCLASS_ANY, sofia_presence_event_handler, NULL) != SWITCH_STATUS_SUCCESS) {

		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Couldn't bind!\n");
		return SWITCH_STATUS_GENERR;
	}

	if (switch_event_bind(modname, SWITCH_EVENT_TRAP, SWITCH_EVENT_SUBCLASS_ANY, general_event_handler, NULL) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Couldn't bind!\n");
		return SWITCH_STATUS_GENERR;
	}

	if (switch_event_bind(modname, SWITCH_EVENT_NOTIFY, SWITCH_EVENT_SUBCLASS_ANY, general_event_handler, NULL) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Couldn't bind!\n");
		return SWITCH_STATUS_GENERR;
	}

	if (switch_event_bind(modname, SWITCH_EVENT_PHONE_FEATURE, SWITCH_EVENT_SUBCLASS_ANY, general_event_handler, NULL) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Couldn't bind!\n");
		return SWITCH_STATUS_GENERR;
	}

	if (switch_event_bind(modname, SWITCH_EVENT_SEND_MESSAGE, SWITCH_EVENT_SUBCLASS_ANY, general_event_handler, NULL) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Couldn't bind!\n");
		return SWITCH_STATUS_GENERR;
	}

	if (switch_event_bind(modname, SWITCH_EVENT_SEND_INFO, SWITCH_EVENT_SUBCLASS_ANY, general_event_handler, NULL) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Couldn't bind!\n");
		return SWITCH_STATUS_GENERR;
	}

	/* connect my internal structure to the blank pointer passed to me */
	*module_interface = switch_loadable_module_create_module_interface(pool, modname);
	sofia_endpoint_interface = switch_loadable_module_create_interface(*module_interface, SWITCH_ENDPOINT_INTERFACE);
	sofia_endpoint_interface->interface_name = "sofia";
	sofia_endpoint_interface->io_routines = &sofia_io_routines;
	sofia_endpoint_interface->state_handler = &sofia_event_handlers;
	sofia_endpoint_interface->recover_callback = sofia_recover_callback;

	management_interface = switch_loadable_module_create_interface(*module_interface, SWITCH_MANAGEMENT_INTERFACE);
	management_interface->relative_oid = "1001";
	management_interface->management_function = sofia_manage;

	SWITCH_ADD_APP(app_interface, "sofia_sla", "private sofia sla function",
				   "private sofia sla function", sofia_sla_function, "<uuid>", SAF_NONE);


	SWITCH_ADD_API(api_interface, "sofia", "Sofia Controls", sofia_function, "<cmd> <args>");
	SWITCH_ADD_API(api_interface, "sofia_gateway_data", "Get data from a sofia gateway", sofia_gateway_data_function,
				   "<gateway_name> [ivar|ovar|var] <name>");
	switch_console_set_complete("add sofia help");
	switch_console_set_complete("add sofia status");
	switch_console_set_complete("add sofia xmlstatus");

	switch_console_set_complete("add sofia loglevel ::[all:default:tport:iptsec:nea:nta:nth_client:nth_server:nua:soa:sresolv:stun ::[0:1:2:3:4:5:6:7:8:9");
	switch_console_set_complete("add sofia tracelevel ::[console:alert:crit:err:warning:notice:info:debug");

	switch_console_set_complete("add sofia global siptrace ::[on:off");
	switch_console_set_complete("add sofia global standby ::[on:off");
	switch_console_set_complete("add sofia global capture  ::[on:off");
	switch_console_set_complete("add sofia global watchdog ::[on:off");

	switch_console_set_complete("add sofia global debug ::[presence:sla:none");


	switch_console_set_complete("add sofia profile");
	switch_console_set_complete("add sofia profile restart all");

	switch_console_set_complete("add sofia profile ::sofia::list_profiles start");
	switch_console_set_complete("add sofia profile ::sofia::list_profiles stop wait");
	switch_console_set_complete("add sofia profile ::sofia::list_profiles rescan");
	switch_console_set_complete("add sofia profile ::sofia::list_profiles restart");

	switch_console_set_complete("add sofia profile ::sofia::list_profiles flush_inbound_reg");
	switch_console_set_complete("add sofia profile ::sofia::list_profiles check_sync");
	switch_console_set_complete("add sofia profile ::sofia::list_profiles register ::sofia::list_profile_gateway");
	switch_console_set_complete("add sofia profile ::sofia::list_profiles unregister ::sofia::list_profile_gateway");
	switch_console_set_complete("add sofia profile ::sofia::list_profiles killgw ::sofia::list_profile_gateway");
	switch_console_set_complete("add sofia profile ::sofia::list_profiles siptrace on");
	switch_console_set_complete("add sofia profile ::sofia::list_profiles siptrace off");
	switch_console_set_complete("add sofia profile ::sofia::list_profiles capture on");
	switch_console_set_complete("add sofia profile ::sofia::list_profiles capture off");
	switch_console_set_complete("add sofia profile ::sofia::list_profiles watchdog on");
	switch_console_set_complete("add sofia profile ::sofia::list_profiles watchdog off");

	switch_console_set_complete("add sofia profile ::sofia::list_profiles gwlist up");
	switch_console_set_complete("add sofia profile ::sofia::list_profiles gwlist down");

	switch_console_set_complete("add sofia status profile ::sofia::list_profiles");
	switch_console_set_complete("add sofia status profile ::sofia::list_profiles reg");
	switch_console_set_complete("add sofia status gateway ::sofia::list_gateways");
	switch_console_set_complete("add sofia xmlstatus profile ::sofia::list_profiles");
	switch_console_set_complete("add sofia xmlstatus profile ::sofia::list_profiles reg");
	switch_console_set_complete("add sofia xmlstatus gateway ::sofia::list_gateways");

	switch_console_add_complete_func("::sofia::list_profiles", list_profiles);
	switch_console_add_complete_func("::sofia::list_gateways", list_gateways);
	switch_console_add_complete_func("::sofia::list_profile_gateway", list_profile_gateway);


	SWITCH_ADD_API(api_interface, "sofia_username_of", "Sofia Username Lookup", sofia_username_of_function, "[profile/]<user>@<domain>");
	SWITCH_ADD_API(api_interface, "sofia_contact", "Sofia Contacts", sofia_contact_function, "[profile/]<user>@<domain>");
	SWITCH_ADD_API(api_interface, "sofia_count_reg", "Count Sofia registration", sofia_count_reg_function, "[profile/]<user>@<domain>");
	SWITCH_ADD_API(api_interface, "sofia_dig", "SIP DIG", sip_dig_function, "<url>");
	SWITCH_ADD_API(api_interface, "sofia_presence_data", "Sofia Presence Data", sofia_presence_data_function, "[list|status|rpid|user_agent] [profile/]<user>@domain");
	SWITCH_ADD_CHAT(chat_interface, SOFIA_CHAT_PROTO, sofia_presence_chat_send);

	crtp_init(*module_interface);

	/* indicate that the module should continue to be loaded */
	return SWITCH_STATUS_SUCCESS;
}

SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_sofia_shutdown)
{
	int sanity = 0;
	int i;
	switch_status_t st;

	switch_console_del_complete_func("::sofia::list_profiles");
	switch_console_set_complete("del sofia");

	switch_mutex_lock(mod_sofia_globals.mutex);
	if (mod_sofia_globals.running == 1) {
		mod_sofia_globals.running = 0;
	}
	switch_mutex_unlock(mod_sofia_globals.mutex);

	switch_event_unbind_callback(sofia_presence_event_handler);

	switch_event_unbind_callback(general_event_handler);
	switch_event_unbind_callback(event_handler);

	switch_queue_push(mod_sofia_globals.presence_queue, NULL);
	switch_queue_interrupt_all(mod_sofia_globals.presence_queue);

	while (mod_sofia_globals.threads) {
		switch_cond_next();
		if (++sanity >= 60000) {
			break;
		}
	}


	for (i = 0; mod_sofia_globals.msg_queue_thread[i]; i++) {
		switch_queue_push(mod_sofia_globals.msg_queue, NULL);
		switch_queue_interrupt_all(mod_sofia_globals.msg_queue);
	}


	for (i = 0; mod_sofia_globals.msg_queue_thread[i]; i++) {
		switch_thread_join(&st, mod_sofia_globals.msg_queue_thread[i]);
	}

	if (mod_sofia_globals.presence_thread) {
		switch_thread_join(&st, mod_sofia_globals.presence_thread);
	}

	//switch_yield(1000000);
	su_deinit();

	switch_mutex_lock(mod_sofia_globals.hash_mutex);
	switch_core_hash_destroy(&mod_sofia_globals.profile_hash);
	switch_core_hash_destroy(&mod_sofia_globals.gateway_hash);
	switch_mutex_unlock(mod_sofia_globals.hash_mutex);

	return SWITCH_STATUS_SUCCESS;
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
