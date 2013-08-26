/* 
 * mod_rtmp for FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
 * Copyright (C) 2011-2012, Barracuda Networks Inc.
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
 * The Original Code is mod_rtmp for FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
 *
 * The Initial Developer of the Original Code is Barracuda Networks Inc.
 * Portions created by the Initial Developer are Copyright (C)
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 * 
 * Mathieu Rene <mrene@avgs.ca>
 *
 * rtmp.c -- RTMP Signalling functions
 *
 */

#include "mod_rtmp.h"

/* AMF */
#include "amf0.h"
#include "io.h"
#include "types.h"

/* RTMP_INVOKE_FUNCTION is a macro that expands to: 
switch_status_t function(rtmp_session_t *rsession, rtmp_state_t *state, int amfnumber, int transaction_id, int argc, amf0_data *argv[])
*/

RTMP_INVOKE_FUNCTION(rtmp_i_connect)
{
	amf0_data *object1 = amf0_object_new(), *object2 = amf0_object_new(), *params = argv[0], *d;
	const char *s;
	
	if ((d = amf0_object_get(params, "app")) && (s = amf0_get_string(d))) {
		rsession->app = switch_core_strdup(rsession->pool, s);
	}
	
	if ((d = amf0_object_get(params, "flashVer")) && (s = amf0_get_string(d))) {
		rsession->flashVer = switch_core_strdup(rsession->pool, s);	
	}
	if ((d = amf0_object_get(params, "swfUrl")) && (s = amf0_get_string(d))) {
		rsession->swfUrl = switch_core_strdup(rsession->pool, s);	
	}
	if ((d = amf0_object_get(params, "tcUrl")) && (s = amf0_get_string(d))) {
		rsession->tcUrl = switch_core_strdup(rsession->pool, s);	
	}
	if ((d = amf0_object_get(params, "pageUrl")) && (s = amf0_get_string(d))) {
		rsession->pageUrl = switch_core_strdup(rsession->pool, s);	
	}

	if ((d = amf0_object_get(params, "capabilities"))) {
		rsession->capabilities = amf0_get_number(d);
	}
	if ((d = amf0_object_get(params, "audioCodecs"))) {
		rsession->audioCodecs = amf0_get_number(d);
	}
	if ((d = amf0_object_get(params, "videoCodecs"))) {
		rsession->videoCodecs = amf0_get_number(d);
	}
	if ((d = amf0_object_get(params, "videoFunction"))) {
		rsession->videoFunction = amf0_get_number(d);
	}

	amf0_object_add(object1, "fmsVer", amf0_number_new(1));
	amf0_object_add(object1, "capabilities", amf0_number_new(31));

	amf0_object_add(object2, "level", amf0_str("status"));
	amf0_object_add(object2, "code",  amf0_str("NetConnection.Connect.Success"));
	amf0_object_add(object2, "description", amf0_str("Connection succeeded"));
	amf0_object_add(object2, "clientId", amf0_number_new(217834719));
	amf0_object_add(object2, "objectEncoding", amf0_number_new(0));
	
	rtmp_set_chunksize(rsession, rsession->profile->chunksize);

	{
		unsigned char ackbuf[] = { INT32(RTMP_DEFAULT_ACK_WINDOW) };
		rtmp_send_message(rsession, 2, 0, RTMP_TYPE_WINDOW_ACK_SIZE, 0, ackbuf, sizeof(ackbuf), MSG_FULLHEADER);
	}
	
	{
		unsigned char ackbuf[] = { INT32(RTMP_DEFAULT_ACK_WINDOW), 0x1 /* Soft limit */};
		rtmp_send_message(rsession, 2, 0, RTMP_TYPE_SET_PEER_BW, 0, ackbuf, sizeof(ackbuf), MSG_FULLHEADER);
	}
	
	{
		unsigned char buf[] = { 
			INT16(RTMP_CTRL_STREAM_BEGIN),
			INT32(0)
		};
		
		rtmp_send_message(rsession, 2, 0, RTMP_TYPE_USERCTRL, 0, buf, sizeof(buf), 0);
	}
			
	/* respond with a success message */
	rtmp_send_invoke_free(rsession, amfnumber, 0, 0,
		amf0_str("_result"),
		amf0_number_new(1), 
		object1, 
		object2, 
		NULL);
	
	rtmp_send_invoke_free(rsession, 3, 0, 0,
		amf0_str("connected"),
		amf0_number_new(0),
		amf0_null_new(),
		amf0_str(rsession->uuid), NULL);
	
	switch_log_printf(SWITCH_CHANNEL_UUID_LOG(rsession->uuid), SWITCH_LOG_NOTICE, "Sent connect reply\n");
	
	return SWITCH_STATUS_SUCCESS;	
}


RTMP_INVOKE_FUNCTION(rtmp_i_createStream)
{	
	rtmp_send_invoke_free(rsession, amfnumber, 0, 0,
		amf0_str("_result"),
		amf0_number_new(transaction_id),
		amf0_null_new(),
		amf0_number_new(rsession->next_streamid),
		NULL);
		
	switch_log_printf(SWITCH_CHANNEL_UUID_LOG(rsession->uuid), SWITCH_LOG_INFO, "Replied to createStream (%u)\n", rsession->next_streamid);
	
	rsession->next_streamid++;
	
	return SWITCH_STATUS_SUCCESS;
}

RTMP_INVOKE_FUNCTION(rtmp_i_noop)
{
	return SWITCH_STATUS_SUCCESS;
}


RTMP_INVOKE_FUNCTION(rtmp_i_receiveaudio)
{
        switch_bool_t enabled = argv[1] ? amf0_boolean_get_value(argv[1]) : SWITCH_FALSE;

		if (enabled) {
			switch_set_flag(rsession, SFLAG_AUDIO);
		} else {
			switch_clear_flag(rsession, SFLAG_AUDIO);
		}

        switch_log_printf(SWITCH_CHANNEL_UUID_LOG(rsession->uuid), SWITCH_LOG_INFO, "%sending audio\n", enabled ? "S" : "Not s");

        return SWITCH_STATUS_SUCCESS;
}

RTMP_INVOKE_FUNCTION(rtmp_i_receivevideo)
{
        switch_bool_t enabled = argv[1] ? amf0_boolean_get_value(argv[1]) : SWITCH_FALSE;

		if (enabled) {
			switch_set_flag(rsession, SFLAG_VIDEO);
			if (rsession->tech_pvt) {
				switch_set_flag(rsession->tech_pvt, TFLAG_VID_WAIT_KEYFRAME);
			}
		} else {
			switch_clear_flag(rsession, SFLAG_VIDEO);
		}

        switch_log_printf(SWITCH_CHANNEL_UUID_LOG(rsession->uuid), SWITCH_LOG_INFO, "%sending video\n", enabled ? "S" : "Not s");

        return SWITCH_STATUS_SUCCESS;
}


RTMP_INVOKE_FUNCTION(rtmp_i_play)
{
	amf0_data *obj = amf0_object_new();
	amf0_data *object = amf0_object_new();
	
	switch_log_printf(SWITCH_CHANNEL_UUID_LOG(rsession->uuid), SWITCH_LOG_DEBUG, "Got play for %s on stream %d\n", switch_str_nil(amf0_get_string(argv[1])),
		state->stream_id);

	/* Set outgoing chunk size to 1024 bytes */
	rtmp_set_chunksize(rsession, 1024);
	
	rsession->media_streamid = state->stream_id;

	/* Send StreamBegin on the current stream */
	{
		unsigned char buf[] = { 
			INT16(RTMP_CTRL_STREAM_BEGIN),
			INT32(rsession->media_streamid)
		};
		rtmp_send_message(rsession, 2, 0, RTMP_TYPE_USERCTRL, 0, buf, sizeof(buf), 0);
	}
	

	{
		unsigned char buf[] = {
			INT16(RTMP_CTRL_SET_BUFFER_LENGTH),
			INT32(rsession->media_streamid),
			INT32(rsession->profile->buffer_len)
		};
		rtmp_send_message(rsession, 2, 0, RTMP_TYPE_USERCTRL, 0, buf, sizeof(buf), 0);
	}	

	/* Send onStatus */
	amf0_object_add(object, "level", amf0_str("status"));
	amf0_object_add(object, "code", amf0_str("NetStream.Play.Reset"));
	amf0_object_add(object, "description", amf0_str("description"));
	amf0_object_add(object, "details", amf0_str("details"));
	amf0_object_add(object, "clientid", amf0_number_new(217834719));
	
	rtmp_send_invoke_free(rsession, RTMP_DEFAULT_STREAM_NOTIFY, 0, rsession->media_streamid,
		amf0_str("onStatus"),
		amf0_number_new(1),
		amf0_null_new(),
		object, NULL);
	
	object = amf0_object_new();

	amf0_object_add(object, "level", amf0_str("status"));
	amf0_object_add(object, "code", amf0_str("NetStream.Play.Start"));
	amf0_object_add(object, "description", amf0_str("description"));
	amf0_object_add(object, "details", amf0_str("details"));
	amf0_object_add(object, "clientid", amf0_number_new(217834719));

	rtmp_send_invoke_free(rsession, RTMP_DEFAULT_STREAM_NOTIFY, 0, rsession->media_streamid,
		amf0_str("onStatus"),
		amf0_number_new(1),
		amf0_null_new(),
		object, NULL);

	amf0_object_add(obj, "code", amf0_str("NetStream.Data.Start"));
	
	rtmp_send_notify_free(rsession, RTMP_DEFAULT_STREAM_NOTIFY, 0, rsession->media_streamid, 
		amf0_str("onStatus"),
		obj, NULL);
		
	rtmp_send_notify_free(rsession, RTMP_DEFAULT_STREAM_NOTIFY, 0, rsession->media_streamid,
		amf0_str("|RtmpSampleAccess"),
		amf0_boolean_new(1),
		amf0_boolean_new(1), NULL);
		
	return SWITCH_STATUS_SUCCESS;
}

RTMP_INVOKE_FUNCTION(rtmp_i_publish)
{
	
	unsigned char buf[] = {
		INT16(RTMP_CTRL_STREAM_BEGIN),
		INT32(state->stream_id)
	};
	
	rtmp_send_message(rsession, 2, 0, RTMP_TYPE_USERCTRL, 0, buf, sizeof(buf), 0);
	
	rtmp_send_invoke_free(rsession, amfnumber, 0, 0,
		amf0_str("_result"),
		amf0_number_new(transaction_id), 
		amf0_null_new(), 
		amf0_null_new(),
		NULL);
		
		
	switch_log_printf(SWITCH_CHANNEL_UUID_LOG(rsession->uuid), SWITCH_LOG_INFO, "Got publish on stream %u.\n", state->stream_id);
	
	return SWITCH_STATUS_SUCCESS;
}

RTMP_INVOKE_FUNCTION(rtmp_i_makeCall)
{
	switch_core_session_t *newsession = NULL;		
	char *number = NULL;
	
	if ((number = amf0_get_string(argv[1]))) {
		switch_event_t *event = NULL;
		char *auth, *user = NULL, *domain = NULL;
		
		if ((auth = amf0_get_string(argv[2])) && !zstr(auth)) {
			switch_split_user_domain(auth, &user, &domain);
			if (rtmp_session_check_user(rsession, user, domain) != SWITCH_STATUS_SUCCESS) {
				switch_log_printf(SWITCH_CHANNEL_UUID_LOG(rsession->uuid), SWITCH_LOG_WARNING, "Unauthorized call to %s, client is not logged in account [%s@%s]\n",
					number, switch_str_nil(user), switch_str_nil(domain));
				return SWITCH_STATUS_FALSE;
			}
		} else if (rsession->profile->auth_calls && !rsession->account) {
			switch_log_printf(SWITCH_CHANNEL_UUID_LOG(rsession->uuid), SWITCH_LOG_WARNING, "Unauthorized call to %s, client is not logged in\n", number);
			return SWITCH_STATUS_FALSE;
		}
		
		if (amf0_is_object(argv[3])) {
			amf_object_to_event(argv[3], &event);
		}
		
		if (rtmp_session_create_call(rsession, &newsession, 0, RTMP_DEFAULT_STREAM_AUDIO, number, user, domain, event) != SWITCH_CAUSE_SUCCESS) {
			switch_log_printf(SWITCH_CHANNEL_UUID_LOG(rsession->uuid), SWITCH_LOG_ERROR, "Couldn't create call.\n");
		}
		
		if (event) {
			switch_event_destroy(&event);
		}
	}
	
	if (newsession) {
		rtmp_private_t *new_pvt = switch_core_session_get_private(newsession);
		rtmp_send_invoke_free(rsession, 3, 0, 0,
			amf0_str("onMakeCall"),
			amf0_number_new(transaction_id),
			amf0_null_new(),
			amf0_str(switch_core_session_get_uuid(newsession)),
			amf0_str(switch_str_nil(number)),
			amf0_str(switch_str_nil(new_pvt->auth)),
			NULL);
			
		rtmp_attach_private(rsession, switch_core_session_get_private(newsession));
	}
	
	return SWITCH_STATUS_SUCCESS;
}

RTMP_INVOKE_FUNCTION(rtmp_i_sendDTMF)
{
	/* Send DTMFs on the active channel */
	switch_dtmf_t dtmf = { 0 };
	switch_channel_t *channel;
	char *digits;
	
	if (!rsession->tech_pvt) {
		return SWITCH_STATUS_FALSE;
	}
	
	channel = switch_core_session_get_channel(rsession->tech_pvt->session);

	if (amf0_is_number(argv[2])) {
		dtmf.duration = amf0_get_number(argv[2]);	
	} else if (!zstr(amf0_get_string(argv[2]))) {
		dtmf.duration = atoi(amf0_get_string(argv[2]));
	}
	
	if ((digits = amf0_get_string(argv[1]))) {
		size_t len = strlen(digits);
		size_t j;
		for (j = 0; j < len; j++) {
			dtmf.digit = digits[j];
			switch_channel_queue_dtmf(channel, &dtmf);
		}
	}
	
	return SWITCH_STATUS_SUCCESS;
}


RTMP_INVOKE_FUNCTION(rtmp_i_login)
{
	char *user, *auth, *domain, *ddomain = NULL;
	
	
	user = amf0_get_string(argv[1]);
	auth = amf0_get_string(argv[2]);
	
	if (zstr(user) || zstr(auth)) {
		return SWITCH_STATUS_FALSE;
	}
	
	if ((domain = strchr(user, '@'))) {
		*domain++ = '\0';
	}
	
	if (zstr(domain)) {
		ddomain = switch_core_get_domain(SWITCH_TRUE);
		domain = ddomain;
	}


	if (rtmp_check_auth(rsession, user, domain, auth) == SWITCH_STATUS_SUCCESS) {
		rtmp_session_login(rsession, user, domain);		
	} else {
		rtmp_send_invoke_free(rsession, 3, 0, 0,
			amf0_str("onLogin"),
			amf0_number_new(0),
			amf0_null_new(),
			amf0_str("failure"),
			amf0_null_new(),
			amf0_null_new(), NULL);
	}
	

	switch_safe_free(ddomain);

	return SWITCH_STATUS_SUCCESS;
}

RTMP_INVOKE_FUNCTION(rtmp_i_logout)
{
	char *auth = amf0_get_string(argv[1]);
	char *user = NULL, *domain = NULL;
	
	/* Unregister from that user */
	rtmp_clear_registration(rsession, auth, NULL);
	
	switch_split_user_domain(auth, &user, &domain);
	
	if (!zstr(user) && !zstr(domain)) {
		rtmp_session_logout(rsession, user, domain);
	}
	
	return SWITCH_STATUS_SUCCESS;
}

RTMP_INVOKE_FUNCTION(rtmp_i_register)
{
	char *auth = amf0_get_string(argv[1]);
	const char *user = NULL, *domain = NULL;
	char *dup = NULL;
	switch_status_t status;
	
	if (!rsession->account) {
		return SWITCH_STATUS_FALSE;
	}
	
	if (!zstr(auth)) {
		dup = strdup(auth);
		switch_split_user_domain(dup, (char**)&user, (char**)&domain);
	} else {
		dup = auth = switch_mprintf("%s@%s", rsession->account->user, rsession->account->domain);
		user = rsession->account->user;
		domain = rsession->account->domain;
	}
	
	if (rtmp_session_check_user(rsession, user, domain) == SWITCH_STATUS_SUCCESS) {
		rtmp_add_registration(rsession, auth, amf0_get_string(argv[2]));
		status = SWITCH_STATUS_SUCCESS;	
	} else {
		status = SWITCH_STATUS_FALSE;
	}
	
	switch_safe_free(dup);
	
	return status;
}

RTMP_INVOKE_FUNCTION(rtmp_i_unregister)
{
	rtmp_clear_registration(rsession, amf0_get_string(argv[1]), amf0_get_string(argv[2]));
	
	return SWITCH_STATUS_SUCCESS;
}

RTMP_INVOKE_FUNCTION(rtmp_i_answer)
{
	switch_channel_t *channel = NULL;
	char *uuid = amf0_get_string(argv[1]);
	
	if (!zstr(uuid)) {
		rtmp_private_t *new_tech_pvt = rtmp_locate_private(rsession, uuid);
		if (new_tech_pvt) {
			switch_channel_mark_answered(switch_core_session_get_channel(new_tech_pvt->session));
			rtmp_attach_private(rsession, new_tech_pvt);
		}
		return SWITCH_STATUS_FALSE;
	}
	
	if (!rsession->tech_pvt) {
		return SWITCH_STATUS_FALSE;
	}
	
	/* No UUID specified but we're attached to a channel, mark it as answered */
	channel = switch_core_session_get_channel(rsession->tech_pvt->session);
	switch_channel_mark_answered(channel);
	rtmp_attach_private(rsession, rsession->tech_pvt);
	
	return SWITCH_STATUS_SUCCESS;
}

RTMP_INVOKE_FUNCTION(rtmp_i_attach)
{
	rtmp_private_t *tech_pvt = NULL;
	char *uuid = amf0_get_string(argv[1]);
	
	if (!zstr(uuid)) {
		tech_pvt = rtmp_locate_private(rsession, uuid);
	}
	/* Will detach if an empty (or invalid) uuid is received */	
	rtmp_attach_private(rsession, tech_pvt);
	
	return SWITCH_STATUS_SUCCESS;
}

RTMP_INVOKE_FUNCTION(rtmp_i_hangup)
{
	/* CallID (or null/nothing to hangup the current call) */
	char *uuid = amf0_get_string(argv[1]);
	char *scause;
	switch_channel_t *channel = NULL;
	switch_call_cause_t cause = SWITCH_CAUSE_NORMAL_CLEARING;

	if (!zstr(uuid)) {
		rtmp_private_t *tech_pvt = rtmp_locate_private(rsession, uuid);
		if (tech_pvt) {
			channel = switch_core_session_get_channel(tech_pvt->session);	
		}
	}
	
	if (!channel) {
		if (!rsession->tech_pvt) {
			return SWITCH_STATUS_FALSE;
		}
		channel = switch_core_session_get_channel(rsession->tech_pvt->session);			
	}
	
	if (amf0_is_number(argv[2])) {
		cause = amf0_get_number(argv[2]);
	} else if ((scause = amf0_get_string(argv[2])) && !zstr(scause)) {
		cause = switch_channel_str2cause(scause);
	}	
	
	switch_channel_hangup(channel, cause);
	
	return SWITCH_STATUS_SUCCESS;
}

RTMP_INVOKE_FUNCTION(rtmp_i_transfer)
{
	char *uuid = amf0_get_string(argv[1]);
	char *dest = amf0_get_string(argv[2]);
	rtmp_private_t *tech_pvt;
	
	if (zstr(uuid) || zstr(dest)) {
		return SWITCH_STATUS_FALSE;
	}
	
	if ((tech_pvt = rtmp_locate_private(rsession, uuid))) {
		const char *other_uuid = switch_channel_get_partner_uuid(tech_pvt->channel);
		switch_core_session_t *session;
		
		if (!zstr(other_uuid) && (session = switch_core_session_locate(other_uuid))) {
			switch_ivr_session_transfer(session, dest, NULL, NULL);
			switch_core_session_rwunlock(session);
		}
	}
	
	return SWITCH_STATUS_SUCCESS;
}

RTMP_INVOKE_FUNCTION(rtmp_i_join)
{
	char *uuid[] = { amf0_get_string(argv[1]), amf0_get_string(argv[2]) };
	const char *other_uuid[2];
	rtmp_private_t *tech_pvt[2];
	
	if (zstr(uuid[0]) || zstr(uuid[1])) {
		return SWITCH_STATUS_SUCCESS;
	}
	
	if (!(tech_pvt[0] = rtmp_locate_private(rsession, uuid[0])) ||
	    !(tech_pvt[1] = rtmp_locate_private(rsession, uuid[1]))) {
		return SWITCH_STATUS_FALSE;
	}
	
	if (tech_pvt[0] == tech_pvt[1]) {
		return SWITCH_STATUS_FALSE;
	}
	
	if ((other_uuid[0] = switch_channel_get_partner_uuid(tech_pvt[0]->channel)) &&
	    (other_uuid[1] = switch_channel_get_partner_uuid(tech_pvt[1]->channel))) {

#ifndef RTMP_DONT_HOLD
		if (switch_test_flag(tech_pvt[0], TFLAG_DETACHED)) {
			switch_ivr_unhold(tech_pvt[0]->session);
		}
		if (switch_test_flag(tech_pvt[1], TFLAG_DETACHED)) {
			switch_ivr_unhold(tech_pvt[1]->session);
		}
#endif
		
		switch_ivr_uuid_bridge(other_uuid[0], other_uuid[1]);
	}
	
	return SWITCH_STATUS_SUCCESS;
}

/*

3-way:

[0] is always the current active call
[1] is the call to be brought into the call, and that will be running the three_way application

- Set the current app of other[1] to three_way with other_uuid[0]
- Put tech_pvt[0] to sleep: set state to CS_HIBERNATE
- set CF_TRANSFER, set state CS_EXECUTE (do we need CF_TRANSFER here?)

- setup a state handler in other[1] to detect when it hangs up

Check list:
tech_pvt[0] or other[0] hangs up 
	If we were attached to the call, switch the active call to tech_pvt[1]
tech_pvt[1] or other[1] hangs up 
	Clear up any 3-way indications on the tech_pvt[0]

*/

static switch_status_t three_way_on_soft_execute(switch_core_session_t *session);
#if 0
static switch_status_t three_way_on_hangup(switch_core_session_t *session);
#endif 

static const switch_state_handler_table_t three_way_state_handlers_remote = {
	/*.on_init */ NULL,
	/*.on_routing */ NULL,
	/*.on_execute */ NULL,
	/*.on_hangup */ NULL,
	/*.on_exchange_media */ NULL,
	/*.on_soft_execute */ three_way_on_soft_execute,
	/*.on_consume_media */ NULL,
	/*.on_hibernate */ NULL
};

/* runs on other_session[1] */
static switch_status_t three_way_on_soft_execute(switch_core_session_t *other_session)
{
	switch_channel_t *other_channel = switch_core_session_get_channel(other_session);
	const char *uuid = switch_channel_get_variable(other_channel, RTMP_THREE_WAY_UUID_VARIABLE);
	const char *my_uuid = switch_channel_get_variable(other_channel, SWITCH_SOFT_HOLDING_UUID_VARIABLE);
	switch_core_session_t *my_session;
	switch_channel_t *my_channel;
	rtmp_private_t *tech_pvt;
	
	if (zstr(uuid) || zstr(my_uuid)) {
		return SWITCH_STATUS_SUCCESS;
	}
	
	if (zstr(my_uuid) || !(my_session = switch_core_session_locate(my_uuid))) {
		return SWITCH_STATUS_SUCCESS;
	}
	
	if (!switch_core_session_check_interface(my_session, rtmp_globals.rtmp_endpoint_interface)) {
		/* In case someone tempers with my variables, since we get tech_pvt from there */
		switch_core_session_rwunlock(my_session);
		return SWITCH_STATUS_SUCCESS;
	}
	
	my_channel = switch_core_session_get_channel(my_session);
	tech_pvt = switch_core_session_get_private(my_session);
	
	switch_ivr_eavesdrop_session(other_session, uuid, NULL, ED_MUX_READ | ED_MUX_WRITE);
	
	/* 3-way call ended, whatever the reason
	 * We need to go back to our original state. */
	if (!switch_channel_up(other_channel)) {
		/* channel[1] hung up, check if we have special post-bridge actions, and hangup otherwise */
		/* if my_channel isn't ready, it means something else has control of it, leave it alone */
		if (switch_channel_ready(my_channel)) {
			const char *s;
			if ((s = switch_channel_get_variable(my_channel, SWITCH_PARK_AFTER_BRIDGE_VARIABLE)) && switch_true(s)) {
				switch_ivr_park_session(my_session);
			} else if ((s = switch_channel_get_variable(my_channel, SWITCH_TRANSFER_AFTER_BRIDGE_VARIABLE)) && !zstr(s)) {
				int argc;
				char *argv[4] = { 0 };
				char *mydata = switch_core_session_strdup(my_session, s);

				switch_channel_set_variable(my_channel, SWITCH_TRANSFER_AFTER_BRIDGE_VARIABLE, NULL);

				if ((argc = switch_split(mydata, ':', argv)) >= 1) {
					switch_ivr_session_transfer(my_session, argv[0], argv[1], argv[2]);
				} else {
					switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(my_session), SWITCH_LOG_ERROR, "No extension specified.\n");
				}
			} else {
				switch_channel_hangup(my_channel, SWITCH_CAUSE_NORMAL_CLEARING);
			}		
		}
	} else if (switch_channel_ready(other_channel)) {
		/* channel[1] didn't hangup, must be channel[0] then, rebridge this one with its original partner */
		switch_ivr_uuid_bridge(switch_core_session_get_uuid(other_session), my_uuid);
	} else {
		/* channel[1] being taken out of our control, take the other leg out of CS_HIBERNATE if its ready, or else leave it alone */	
		if (switch_channel_ready(my_channel)) {
			switch_channel_set_state(my_channel, CS_EXECUTE);	
		}
	}
	
	switch_channel_clear_state_handler(other_channel, &three_way_state_handlers_remote);
	
	switch_channel_set_variable(other_channel, SWITCH_SOFT_HOLDING_UUID_VARIABLE, NULL);
	switch_channel_set_variable(my_channel, SWITCH_SOFT_HOLDING_UUID_VARIABLE, NULL);
	switch_channel_set_variable(other_channel, RTMP_THREE_WAY_UUID_VARIABLE, NULL);
	
	switch_clear_flag(tech_pvt, TFLAG_THREE_WAY);

	if (my_session) {
		switch_core_session_rwunlock(my_session);	
	}
	
	return SWITCH_STATUS_SUCCESS;
}

RTMP_INVOKE_FUNCTION(rtmp_i_three_way)
{
	/* The first uuid is the (local) uuid of the current call, the 2nd one should be already detached */
	char *uuid[] = { amf0_get_string(argv[1]), amf0_get_string(argv[2]) };
	rtmp_private_t *tech_pvt[2];
	const char *other_uuid[2];
	switch_core_session_t *other_session[2] = { 0 };
	switch_channel_t *other_channel[2] = { 0 };
	
	if (zstr(uuid[0]) || zstr(uuid[1]) || 
		!(tech_pvt[0] = rtmp_locate_private(rsession, uuid[0])) ||
		!(tech_pvt[1] = rtmp_locate_private(rsession, uuid[1]))) {
		return SWITCH_STATUS_FALSE;
	}
	
	/* Make sure we don't 3-way with the same call, and that it doesnt turn into a 4-way, we aren't that permissive */
	if (tech_pvt[0] == tech_pvt[1] || switch_test_flag(tech_pvt[0], TFLAG_THREE_WAY) || 
			switch_test_flag(tech_pvt[1], TFLAG_THREE_WAY)) {
		return SWITCH_STATUS_FALSE;
	}
	
	if (!(other_uuid[0] = switch_channel_get_partner_uuid(tech_pvt[0]->channel)) ||
	    !(other_uuid[1] = switch_channel_get_partner_uuid(tech_pvt[1]->channel))) {
		return SWITCH_STATUS_FALSE; /* Both calls aren't bridged */
	}

	if (!(other_session[0] = switch_core_session_locate(other_uuid[0])) ||
	    !(other_session[1] = switch_core_session_locate(other_uuid[1]))) {
		goto done;
	}
	
	other_channel[0] = switch_core_session_get_channel(other_session[0]);
	other_channel[1] = switch_core_session_get_channel(other_session[1]);
	
	/* Save which uuid is the 3-way target */
	switch_channel_set_variable(other_channel[1], RTMP_THREE_WAY_UUID_VARIABLE, uuid[0]);
	switch_channel_set_variable(tech_pvt[1]->channel, RTMP_THREE_WAY_UUID_VARIABLE, uuid[0]);	
	
	/* Attach redirect */
	switch_set_flag(tech_pvt[1], TFLAG_THREE_WAY);
	
	/* Set soft_holding_uuid to the uuid of the other matching channel, so they can can be bridged back when the 3-way is over */
	switch_channel_set_variable(tech_pvt[1]->channel, SWITCH_SOFT_HOLDING_UUID_VARIABLE, other_uuid[1]);
	switch_channel_set_variable(other_channel[1], SWITCH_SOFT_HOLDING_UUID_VARIABLE, uuid[1]);

	/* Start the 3-way on the 2nd channel using a media bug */
	switch_channel_add_state_handler(other_channel[1], &three_way_state_handlers_remote);

	switch_channel_set_flag(tech_pvt[1]->channel, CF_TRANSFER);
	switch_channel_set_state(tech_pvt[1]->channel, CS_HIBERNATE);
	switch_channel_set_flag(other_channel[1], CF_TRANSFER);
	switch_channel_set_state(other_channel[1], CS_SOFT_EXECUTE);

done:

	if (other_session[0]) {
		switch_core_session_rwunlock(other_session[0]);
	}
	
	if (other_session[1]) {
		switch_core_session_rwunlock(other_session[1]);
	}
	
	return SWITCH_STATUS_SUCCESS;
}

RTMP_INVOKE_FUNCTION(rtmp_i_sendevent)
{
	amf0_data *obj = NULL;
	switch_event_t *event = NULL;
	switch_status_t status = SWITCH_STATUS_SUCCESS;
	const char *uuid = NULL;
	
	if (argv[1] && argv[1]->type == AMF0_TYPE_OBJECT) {
		obj = argv[1];
	} else if (argv[2] && argv[2]->type == AMF0_TYPE_OBJECT) {
		uuid = amf0_get_string(argv[1]);
		obj = argv[2];
	} else {
		switch_log_printf(SWITCH_CHANNEL_UUID_LOG(rsession->uuid), SWITCH_LOG_ERROR, "Bad argument for sendevent");
		return SWITCH_STATUS_FALSE;
	}
	
	
	if (switch_event_create_subclass(&event, zstr(uuid) ? SWITCH_EVENT_CUSTOM : SWITCH_EVENT_MESSAGE, 
		zstr(uuid) ? RTMP_EVENT_CLIENTCUSTOM : NULL) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_UUID_LOG(rsession->uuid), SWITCH_LOG_ERROR, "Couldn't create event\n");
		return SWITCH_STATUS_FALSE;
	}
	
	rtmp_event_fill(rsession, event);

	/* Build event using amf array */
	if ((status = amf_object_to_event(obj, &event)) != SWITCH_STATUS_SUCCESS) {
		switch_event_destroy(&event);
		return SWITCH_STATUS_FALSE;
	}
	
	if (!zstr(uuid)) {
		rtmp_private_t *session_pvt = rtmp_locate_private(rsession, uuid);
		if (session_pvt) {
			if (switch_core_session_queue_event(session_pvt->session, &event) != SWITCH_STATUS_SUCCESS) {
				switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session_pvt->session), SWITCH_LOG_ERROR, "Couldn't queue event to session\n");
				switch_event_destroy(&event);
				status = SWITCH_STATUS_FALSE;
			} else {
				status = SWITCH_STATUS_SUCCESS;
			}
		}
	}
	
	switch_event_fire(&event);
	
	return SWITCH_STATUS_SUCCESS;
}

RTMP_INVOKE_FUNCTION(rtmp_i_log)
{
	const char *data = amf0_get_string(argv[1]);
	
	switch_log_printf(SWITCH_CHANNEL_UUID_LOG(rsession->uuid), SWITCH_LOG_INFO, "Log: %s\n", data);
	
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
