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
 * mod_sofia.c -- SOFIA SIP Endpoint
 *
 */

/* Best viewed in a 160 x 60 VT100 Terminal or so the line below at least fits across your screen*/
/*************************************************************************************************************************************************************/

#include "mod_sofia.h"

struct mod_sofia_globals mod_sofia_globals;

static switch_frame_t silence_frame = { 0 };
static char silence_data[13] = "";


#define STRLEN 15

static switch_memory_pool_t *module_pool = NULL;

static switch_status_t sofia_on_init(switch_core_session_t *session);


static switch_status_t sofia_on_loopback(switch_core_session_t *session);
static switch_status_t sofia_on_transmit(switch_core_session_t *session);
static switch_call_cause_t sofia_outgoing_channel(switch_core_session_t *session,
												  switch_caller_profile_t *outbound_profile, switch_core_session_t **new_session,
												  switch_memory_pool_t **pool);
static switch_status_t sofia_read_frame(switch_core_session_t *session, switch_frame_t **frame, int timeout, switch_io_flag_t flags, int stream_id);
static switch_status_t sofia_write_frame(switch_core_session_t *session, switch_frame_t *frame, int timeout, switch_io_flag_t flags, int stream_id);
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
	private_object_t *tech_pvt;
	switch_channel_t *channel = NULL;

	channel = switch_core_session_get_channel(session);
	assert(channel != NULL);

	tech_pvt = (private_object_t *) switch_core_session_get_private(session);
	assert(tech_pvt != NULL);

	tech_pvt->read_frame.buflen = SWITCH_RTP_MAX_BUF_LEN;

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "SOFIA INIT\n");
	if (switch_channel_test_flag(channel, CF_NOMEDIA)) {
		switch_set_flag_locked(tech_pvt, TFLAG_NOMEDIA);
		sofia_glue_tech_absorb_sdp(tech_pvt);
	}

	if (switch_test_flag(tech_pvt, TFLAG_OUTBOUND)) {
		if (sofia_glue_do_invite(session) != SWITCH_STATUS_SUCCESS) {
			return SWITCH_STATUS_FALSE;
		}
	}

	/* Move Channel's State Machine to RING */
	switch_channel_set_state(channel, CS_RING);
	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t sofia_on_ring(switch_core_session_t *session)
{
	switch_channel_t *channel = NULL;
	private_object_t *tech_pvt = NULL;

	channel = switch_core_session_get_channel(session);
	assert(channel != NULL);

	tech_pvt = (private_object_t *) switch_core_session_get_private(session);
	assert(tech_pvt != NULL);

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "SOFIA RING\n");

	return SWITCH_STATUS_SUCCESS;
}


static switch_status_t sofia_on_execute(switch_core_session_t *session)
{
	switch_channel_t *channel = NULL;
	private_object_t *tech_pvt = NULL;

	channel = switch_core_session_get_channel(session);
	assert(channel != NULL);

	tech_pvt = (private_object_t *) switch_core_session_get_private(session);
	assert(tech_pvt != NULL);

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "SOFIA EXECUTE\n");

	return SWITCH_STATUS_SUCCESS;
}

// map QSIG cause codes to SIP from RFC4497 section 8.4.1
static int hangup_cause_to_sip(switch_call_cause_t cause)
{
	switch (cause) {
	case SWITCH_CAUSE_NO_ROUTE_TRANSIT_NET:
	case SWITCH_CAUSE_NO_ROUTE_DESTINATION:
		return 404;
	case SWITCH_CAUSE_USER_BUSY:
		return 486;
	case SWITCH_CAUSE_NO_USER_RESPONSE:
		return 408;
	case SWITCH_CAUSE_NO_ANSWER:
		return 480;
	case SWITCH_CAUSE_SUBSCRIBER_ABSENT:
		return 480;
	case SWITCH_CAUSE_CALL_REJECTED:
		return 603;
	case SWITCH_CAUSE_NUMBER_CHANGED:
	case SWITCH_CAUSE_REDIRECTION_TO_NEW_DESTINATION:
		return 410;
	case SWITCH_CAUSE_DESTINATION_OUT_OF_ORDER:
		return 502;
	case SWITCH_CAUSE_INVALID_NUMBER_FORMAT:
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
		return 503;
	case SWITCH_CAUSE_OUTGOING_CALL_BARRED:
	case SWITCH_CAUSE_INCOMING_CALL_BARRED:
	case SWITCH_CAUSE_BEARERCAPABILITY_NOTAUTH:
		return 403;
	case SWITCH_CAUSE_BEARERCAPABILITY_NOTAVAIL:
		return 503;
	case SWITCH_CAUSE_BEARERCAPABILITY_NOTIMPL:
		return 488;
	case SWITCH_CAUSE_FACILITY_NOT_IMPLEMENTED:
	case SWITCH_CAUSE_SERVICE_NOT_IMPLEMENTED:
		return 501;
	case SWITCH_CAUSE_INCOMPATIBLE_DESTINATION:
		return 503;
	case SWITCH_CAUSE_RECOVERY_ON_TIMER_EXPIRE:
		return 504;
	case SWITCH_CAUSE_ORIGINATOR_CANCEL:
		return 487;
	default:
		return 480;
	}

}

switch_status_t sofia_on_hangup(switch_core_session_t *session)
{
	switch_core_session_t *a_session;
	private_object_t *tech_pvt;
	switch_channel_t *channel = NULL;
	switch_call_cause_t cause;
	int sip_cause;

	channel = switch_core_session_get_channel(session);
	assert(channel != NULL);

	tech_pvt = (private_object_t *) switch_core_session_get_private(session);
	assert(tech_pvt != NULL);

	cause = switch_channel_get_cause(channel);
	sip_cause = hangup_cause_to_sip(cause);

	sofia_glue_deactivate_rtp(tech_pvt);

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Channel %s hanging up, cause: %s\n",
					  switch_channel_get_name(channel), switch_channel_cause2str(cause));

	if (tech_pvt->hash_key) {
		switch_core_hash_delete(tech_pvt->profile->chat_hash, tech_pvt->hash_key);
	}

	if (tech_pvt->kick && (a_session = switch_core_session_locate(tech_pvt->kick))) {
		switch_channel_t *a_channel = switch_core_session_get_channel(a_session);
		switch_channel_hangup(a_channel, switch_channel_get_cause(channel));
		switch_core_session_rwunlock(a_session);
	}

	if (tech_pvt->nh) {
		if (!switch_test_flag(tech_pvt, TFLAG_BYE)) {
			if (switch_test_flag(tech_pvt, TFLAG_ANS)) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Sending BYE to %s\n", switch_channel_get_name(channel));
				nua_bye(tech_pvt->nh, TAG_END());
			} else {
				if (!switch_test_flag(tech_pvt, TFLAG_OUTBOUND)) {
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Responding to INVITE with: %d\n", sip_cause);
					nua_respond(tech_pvt->nh, sip_cause, NULL, TAG_END());
				} else {
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Sending CANCEL to %s\n", switch_channel_get_name(channel));
					nua_cancel(tech_pvt->nh, TAG_END());
				}
			}
			switch_set_flag_locked(tech_pvt, TFLAG_BYE);
		}
	}

	switch_clear_flag_locked(tech_pvt, TFLAG_IO);

	if (tech_pvt->home) {
		su_home_unref(tech_pvt->home);
		tech_pvt->home = NULL;
	}

	if (tech_pvt->sofia_private) {
		*tech_pvt->sofia_private->uuid = '\0';
	}

	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t sofia_on_loopback(switch_core_session_t *session)
{
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "SOFIA LOOPBACK\n");
	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t sofia_on_transmit(switch_core_session_t *session)
{
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "SOFIA TRANSMIT\n");
	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t sofia_answer_channel(switch_core_session_t *session)
{
	private_object_t *tech_pvt;
	switch_channel_t *channel = NULL;
	switch_status_t status;

	assert(session != NULL);

	channel = switch_core_session_get_channel(session);
	assert(channel != NULL);

	tech_pvt = (private_object_t *) switch_core_session_get_private(session);
	assert(tech_pvt != NULL);

	if (!switch_test_flag(tech_pvt, TFLAG_ANS) && !switch_channel_test_flag(channel, CF_OUTBOUND)) {
		switch_set_flag_locked(tech_pvt, TFLAG_ANS);

		if (switch_channel_test_flag(channel, CF_NOMEDIA)) {
			char *sdp = NULL;
			switch_set_flag_locked(tech_pvt, TFLAG_NOMEDIA);
			if ((sdp = switch_channel_get_variable(channel, SWITCH_B_SDP_VARIABLE))) {
				tech_pvt->local_sdp_str = switch_core_session_strdup(session, sdp);
			}
		} else {
			if (switch_test_flag(tech_pvt, TFLAG_LATE_NEGOTIATION)) {
				char *r_sdp = switch_channel_get_variable(channel, SWITCH_R_SDP_VARIABLE);
				if (sofia_glue_tech_media(tech_pvt, r_sdp) != SWITCH_STATUS_SUCCESS) {
					switch_channel_set_variable(channel, SWITCH_ENDPOINT_DISPOSITION_VARIABLE, "CODEC NEGOTIATION ERROR");
					nua_respond(tech_pvt->nh, SIP_488_NOT_ACCEPTABLE, TAG_END());
					return SWITCH_STATUS_FALSE;
				}
				switch_clear_flag_locked(tech_pvt, TFLAG_LATE_NEGOTIATION);
			}

			if ((status = sofia_glue_tech_choose_port(tech_pvt)) != SWITCH_STATUS_SUCCESS) {
				return status;
			}

			sofia_glue_set_local_sdp(tech_pvt, NULL, 0, NULL, 0);
			sofia_glue_activate_rtp(tech_pvt);

			if (tech_pvt->nh) {
				if (tech_pvt->local_sdp_str) {
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Local SDP %s:\n%s\n", switch_channel_get_name(channel),
									  tech_pvt->local_sdp_str);
				}
			}
		}
		nua_respond(tech_pvt->nh, SIP_200_OK,
					SIPTAG_CONTACT_STR(tech_pvt->profile->url),
					SOATAG_USER_SDP_STR(tech_pvt->local_sdp_str), SOATAG_AUDIO_AUX("cn telephone-event"), NUTAG_INCLUDE_EXTRA_SDP(1), TAG_END());
	}

	return SWITCH_STATUS_SUCCESS;
}


static switch_status_t sofia_read_frame(switch_core_session_t *session, switch_frame_t **frame, int timeout, switch_io_flag_t flags, int stream_id)
{
	private_object_t *tech_pvt = NULL;
	switch_channel_t *channel = NULL;
	int payload = 0;

	channel = switch_core_session_get_channel(session);
	assert(channel != NULL);

	tech_pvt = (private_object_t *) switch_core_session_get_private(session);
	assert(tech_pvt != NULL);

	if (switch_test_flag(tech_pvt, TFLAG_HUP)) {
		return SWITCH_STATUS_FALSE;
	}

	while (!(tech_pvt->read_codec.implementation && switch_rtp_ready(tech_pvt->rtp_session))) {
		if (switch_channel_ready(channel)) {
			switch_yield(10000);
		} else {
			return SWITCH_STATUS_GENERR;
		}
	}


	tech_pvt->read_frame.datalen = 0;
	switch_set_flag_locked(tech_pvt, TFLAG_READING);

#if 0
	if (tech_pvt->last_read) {
		elapsed = (unsigned int) ((switch_time_now() - tech_pvt->last_read) / 1000);
		if (elapsed > 60000) {
			return SWITCH_STATUS_TIMEOUT;
		}
	}
#endif


	if (switch_test_flag(tech_pvt, TFLAG_IO)) {
		switch_status_t status;

		if (!switch_test_flag(tech_pvt, TFLAG_RTP)) {
			return SWITCH_STATUS_GENERR;
		}

		assert(tech_pvt->rtp_session != NULL);
		tech_pvt->read_frame.datalen = 0;


		while (switch_test_flag(tech_pvt, TFLAG_IO) && tech_pvt->read_frame.datalen == 0) {
			tech_pvt->read_frame.flags = SFF_NONE;

			status = switch_rtp_zerocopy_read_frame(tech_pvt->rtp_session, &tech_pvt->read_frame);
			if (status != SWITCH_STATUS_SUCCESS && status != SWITCH_STATUS_BREAK) {
				return SWITCH_STATUS_FALSE;
			}



			payload = tech_pvt->read_frame.payload;

#if 0
			elapsed = (unsigned int) ((switch_time_now() - started) / 1000);

			if (timeout > -1) {
				if (elapsed >= (unsigned int) timeout) {
					return SWITCH_STATUS_BREAK;
				}
			}

			elapsed = (unsigned int) ((switch_time_now() - last_act) / 1000);
			if (elapsed >= hard_timeout) {
				return SWITCH_STATUS_BREAK;
			}
#endif
			if (switch_rtp_has_dtmf(tech_pvt->rtp_session)) {
				char dtmf[128];
				switch_rtp_dequeue_dtmf(tech_pvt->rtp_session, dtmf, sizeof(dtmf));
				switch_channel_queue_dtmf(channel, dtmf);
			}


			if (tech_pvt->read_frame.datalen > 0) {
				size_t bytes = 0;
				int frames = 1;

				if (!switch_test_flag((&tech_pvt->read_frame), SFF_CNG)) {
					if ((bytes = tech_pvt->read_codec.implementation->encoded_bytes_per_frame)) {
						frames = (tech_pvt->read_frame.datalen / bytes);
					}
					tech_pvt->read_frame.samples = (int) (frames * tech_pvt->read_codec.implementation->samples_per_frame);
				}
				break;
			}
		}
	}

	switch_clear_flag_locked(tech_pvt, TFLAG_READING);

	if (tech_pvt->read_frame.datalen == 0) {
		*frame = NULL;
		return SWITCH_STATUS_GENERR;
	}

	*frame = &tech_pvt->read_frame;

	return SWITCH_STATUS_SUCCESS;
}


static switch_status_t sofia_write_frame(switch_core_session_t *session, switch_frame_t *frame, int timeout, switch_io_flag_t flags, int stream_id)
{
	private_object_t *tech_pvt;
	switch_channel_t *channel = NULL;
	switch_status_t status = SWITCH_STATUS_SUCCESS;
	int bytes = 0, samples = 0, frames = 0;

	channel = switch_core_session_get_channel(session);
	assert(channel != NULL);

	tech_pvt = (private_object_t *) switch_core_session_get_private(session);
	assert(tech_pvt != NULL);

	while (!(tech_pvt->read_codec.implementation && switch_rtp_ready(tech_pvt->rtp_session))) {
		if (switch_channel_ready(channel)) {
			switch_yield(10000);
		} else {
			return SWITCH_STATUS_GENERR;
		}
	}

	if (switch_test_flag(tech_pvt, TFLAG_HUP)) {
		return SWITCH_STATUS_FALSE;
	}

	if (!switch_test_flag(tech_pvt, TFLAG_RTP)) {
		return SWITCH_STATUS_GENERR;
	}

	if (!switch_test_flag(tech_pvt, TFLAG_IO)) {
		return SWITCH_STATUS_SUCCESS;
	}

	switch_set_flag_locked(tech_pvt, TFLAG_WRITING);

	if (!switch_test_flag(frame, SFF_CNG)) {
		if (tech_pvt->read_codec.implementation->encoded_bytes_per_frame) {
			bytes = tech_pvt->read_codec.implementation->encoded_bytes_per_frame;
			frames = ((int) frame->datalen / bytes);
		} else
			frames = 1;

		samples = frames * tech_pvt->read_codec.implementation->samples_per_frame;
	}
#if 0
	printf("%s %s->%s send %d bytes %d samples in %d frames ts=%d\n",
		   switch_channel_get_name(channel),
		   tech_pvt->local_sdp_audio_ip, tech_pvt->remote_sdp_audio_ip, frame->datalen, samples, frames, tech_pvt->timestamp_send);
#endif

	tech_pvt->timestamp_send += samples;
	//switch_rtp_write_frame(tech_pvt->rtp_session, frame, tech_pvt->timestamp_send);
	switch_rtp_write_frame(tech_pvt->rtp_session, frame, 0);

	switch_clear_flag_locked(tech_pvt, TFLAG_WRITING);
	return status;
}



static switch_status_t sofia_kill_channel(switch_core_session_t *session, int sig)
{
	private_object_t *tech_pvt;
	switch_channel_t *channel = NULL;

	channel = switch_core_session_get_channel(session);
	assert(channel != NULL);

	tech_pvt = (private_object_t *) switch_core_session_get_private(session);
	assert(tech_pvt != NULL);


	switch (sig) {
	case SWITCH_SIG_BREAK:
		if (switch_rtp_ready(tech_pvt->rtp_session)) {
			switch_rtp_set_flag(tech_pvt->rtp_session, SWITCH_RTP_FLAG_BREAK);
		}
		break;
	case SWITCH_SIG_KILL:
	default:
		switch_clear_flag_locked(tech_pvt, TFLAG_IO);
		switch_set_flag_locked(tech_pvt, TFLAG_HUP);

		if (switch_rtp_ready(tech_pvt->rtp_session)) {
			switch_rtp_kill_socket(tech_pvt->rtp_session);
		}
		break;
	}

	return SWITCH_STATUS_SUCCESS;

}

static switch_status_t sofia_waitfor_read(switch_core_session_t *session, int ms, int stream_id)
{
	private_object_t *tech_pvt;
	switch_channel_t *channel = NULL;

	channel = switch_core_session_get_channel(session);
	assert(channel != NULL);

	tech_pvt = (private_object_t *) switch_core_session_get_private(session);
	assert(tech_pvt != NULL);

	return SWITCH_STATUS_SUCCESS;
}


static switch_status_t sofia_waitfor_write(switch_core_session_t *session, int ms, int stream_id)
{
	private_object_t *tech_pvt;
	switch_channel_t *channel = NULL;

	channel = switch_core_session_get_channel(session);
	assert(channel != NULL);

	tech_pvt = (private_object_t *) switch_core_session_get_private(session);
	assert(tech_pvt != NULL);

	return SWITCH_STATUS_SUCCESS;

}

static switch_status_t sofia_send_dtmf(switch_core_session_t *session, char *digits)
{
	private_object_t *tech_pvt;

	tech_pvt = (private_object_t *) switch_core_session_get_private(session);
	assert(tech_pvt != NULL);

	return switch_rtp_queue_rfc2833(tech_pvt->rtp_session,
									digits, tech_pvt->profile->dtmf_duration * (tech_pvt->read_codec.implementation->samples_per_second / 1000));

}

static switch_status_t sofia_receive_message(switch_core_session_t *session, switch_core_session_message_t *msg)
{
	switch_channel_t *channel;
	private_object_t *tech_pvt;
	switch_status_t status;

	channel = switch_core_session_get_channel(session);
	assert(channel != NULL);

	tech_pvt = (private_object_t *) switch_core_session_get_private(session);
	assert(tech_pvt != NULL);


	switch (msg->message_id) {
	case SWITCH_MESSAGE_INDICATE_NOMEDIA:{
			char *uuid;
			switch_core_session_t *other_session;
			switch_channel_t *other_channel;
			char *ip = NULL, *port = NULL;

			switch_set_flag_locked(tech_pvt, TFLAG_NOMEDIA);
			tech_pvt->local_sdp_str = NULL;
			if ((uuid = switch_channel_get_variable(channel, SWITCH_SIGNAL_BOND_VARIABLE))
				&& (other_session = switch_core_session_locate(uuid))) {
				other_channel = switch_core_session_get_channel(other_session);
				ip = switch_channel_get_variable(other_channel, SWITCH_REMOTE_MEDIA_IP_VARIABLE);
				port = switch_channel_get_variable(other_channel, SWITCH_REMOTE_MEDIA_PORT_VARIABLE);
				switch_core_session_rwunlock(other_session);
				if (ip && port) {
					sofia_glue_set_local_sdp(tech_pvt, ip, atoi(port), NULL, 1);
				}
			}
			if (!tech_pvt->local_sdp_str) {
				sofia_glue_tech_absorb_sdp(tech_pvt);
			}
			sofia_glue_do_invite(session);
		}
		break;
	case SWITCH_MESSAGE_INDICATE_MEDIA:{
			switch_clear_flag_locked(tech_pvt, TFLAG_NOMEDIA);
			tech_pvt->local_sdp_str = NULL;
			if (!switch_rtp_ready(tech_pvt->rtp_session)) {
				sofia_glue_sofia_glue_tech_set_codecs(tech_pvt);
				if ((status = sofia_glue_tech_choose_port(tech_pvt)) != SWITCH_STATUS_SUCCESS) {
					return status;
				}
			}
			sofia_glue_set_local_sdp(tech_pvt, NULL, 0, NULL, 1);
			sofia_glue_do_invite(session);
			while (!switch_rtp_ready(tech_pvt->rtp_session) && switch_channel_get_state(channel) < CS_HANGUP) {
				switch_yield(1000);
			}
		}
		break;

	case SWITCH_MESSAGE_INDICATE_HOLD:{
			switch_set_flag_locked(tech_pvt, TFLAG_SIP_HOLD);
			sofia_glue_do_invite(session);
		}
		break;

	case SWITCH_MESSAGE_INDICATE_UNHOLD:{
			switch_clear_flag_locked(tech_pvt, TFLAG_SIP_HOLD);
			sofia_glue_do_invite(session);
		}
		break;
	case SWITCH_MESSAGE_INDICATE_BRIDGE:

		if (switch_test_flag(tech_pvt, TFLAG_XFER)) {
			switch_clear_flag_locked(tech_pvt, TFLAG_XFER);
			if (msg->pointer_arg) {
				switch_core_session_t *a_session, *b_session = msg->pointer_arg;

				if ((a_session = switch_core_session_locate(tech_pvt->xferto))) {
					private_object_t *a_tech_pvt = switch_core_session_get_private(a_session);
					private_object_t *b_tech_pvt = switch_core_session_get_private(b_session);

					switch_set_flag_locked(a_tech_pvt, TFLAG_REINVITE);
					a_tech_pvt->remote_sdp_audio_ip = switch_core_session_strdup(a_session, b_tech_pvt->remote_sdp_audio_ip);
					a_tech_pvt->remote_sdp_audio_port = b_tech_pvt->remote_sdp_audio_port;
					a_tech_pvt->local_sdp_audio_ip = switch_core_session_strdup(a_session, b_tech_pvt->local_sdp_audio_ip);
					a_tech_pvt->local_sdp_audio_port = b_tech_pvt->local_sdp_audio_port;
					sofia_glue_activate_rtp(a_tech_pvt);

					b_tech_pvt->kick = switch_core_session_strdup(b_session, tech_pvt->xferto);
					switch_core_session_rwunlock(a_session);
				}


				msg->pointer_arg = NULL;
				return SWITCH_STATUS_FALSE;
			}
		}
		if (tech_pvt->rtp_session && switch_test_flag(tech_pvt, TFLAG_TIMER)) {
			switch_rtp_clear_flag(tech_pvt->rtp_session, SWITCH_RTP_FLAG_USE_TIMER);
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "De-activate timed RTP!\n");
		}
		break;
	case SWITCH_MESSAGE_INDICATE_UNBRIDGE:
		if (tech_pvt->rtp_session && switch_test_flag(tech_pvt, TFLAG_TIMER)) {
			switch_rtp_set_flag(tech_pvt->rtp_session, SWITCH_RTP_FLAG_USE_TIMER);
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Re-activate timed RTP!\n");
		}
		break;
	case SWITCH_MESSAGE_INDICATE_REDIRECT:
		if (msg->string_arg) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Re-directing to %s\n", msg->string_arg);
			nua_respond(tech_pvt->nh, SIP_302_MOVED_TEMPORARILY, SIPTAG_CONTACT_STR(msg->string_arg), TAG_END());
		}
		break;
	case SWITCH_MESSAGE_INDICATE_RINGING:
		nua_respond(tech_pvt->nh, SIP_180_RINGING, SIPTAG_CONTACT_STR(tech_pvt->profile->url), TAG_END());
		break;
	case SWITCH_MESSAGE_INDICATE_PROGRESS:{
			if (!switch_test_flag(tech_pvt, TFLAG_ANS)) {
				switch_set_flag_locked(tech_pvt, TFLAG_EARLY_MEDIA);
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "Asked to send early media by %s\n", msg->from);

				/* Transmit 183 Progress with SDP */
				if (switch_channel_test_flag(channel, CF_NOMEDIA)) {
					char *sdp = NULL;
					switch_set_flag_locked(tech_pvt, TFLAG_NOMEDIA);
					if ((sdp = switch_channel_get_variable(channel, SWITCH_B_SDP_VARIABLE))) {
						tech_pvt->local_sdp_str = switch_core_session_strdup(session, sdp);
					}
				} else {
					if (switch_test_flag(tech_pvt, TFLAG_LATE_NEGOTIATION)) {
						char *r_sdp = switch_channel_get_variable(channel, SWITCH_R_SDP_VARIABLE);
						if (sofia_glue_tech_media(tech_pvt, r_sdp) != SWITCH_STATUS_SUCCESS) {
							switch_channel_set_variable(channel, SWITCH_ENDPOINT_DISPOSITION_VARIABLE, "CODEC NEGOTIATION ERROR");
							nua_respond(tech_pvt->nh, SIP_488_NOT_ACCEPTABLE, TAG_END());
							return SWITCH_STATUS_FALSE;
						}
						switch_clear_flag_locked(tech_pvt, TFLAG_LATE_NEGOTIATION);
					}

					if ((status = sofia_glue_tech_choose_port(tech_pvt)) != SWITCH_STATUS_SUCCESS) {
						return status;
					}
					sofia_glue_set_local_sdp(tech_pvt, NULL, 0, NULL, 0);
					sofia_glue_activate_rtp(tech_pvt);
					if (tech_pvt->local_sdp_str) {
						switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "Ring SDP:\n%s\n", tech_pvt->local_sdp_str);
					}
				}

				nua_respond(tech_pvt->nh,
							SIP_183_SESSION_PROGRESS,
							SIPTAG_CONTACT_STR(tech_pvt->profile->url),
							SOATAG_USER_SDP_STR(tech_pvt->local_sdp_str), SOATAG_AUDIO_AUX("cn telephone-event"), TAG_END());
			}
		}
		break;
	default:
		break;
	}

	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t sofia_receive_event(switch_core_session_t *session, switch_event_t *event)
{
	switch_channel_t *channel;
	struct private_object *tech_pvt;
	char *body;
	nua_handle_t *msg_nh;

	channel = switch_core_session_get_channel(session);
	assert(channel != NULL);

	tech_pvt = switch_core_session_get_private(session);
	assert(tech_pvt != NULL);


	if (!(body = switch_event_get_body(event))) {
		body = "";
	}

	if (tech_pvt->hash_key) {
		msg_nh = nua_handle(tech_pvt->profile->nua, NULL,
							SIPTAG_FROM_STR(tech_pvt->chat_from),
							NUTAG_URL(tech_pvt->chat_to), SIPTAG_TO_STR(tech_pvt->chat_to), SIPTAG_CONTACT_STR(tech_pvt->profile->url), TAG_END());

		nua_message(msg_nh, SIPTAG_CONTENT_TYPE_STR("text/html"), SIPTAG_PAYLOAD_STR(body), TAG_END());
	}

	return SWITCH_STATUS_SUCCESS;
}

static const switch_io_routines_t sofia_io_routines = {
	/*.outgoing_channel */ sofia_outgoing_channel,
	/*.answer_channel */ sofia_answer_channel,
	/*.read_frame */ sofia_read_frame,
	/*.write_frame */ sofia_write_frame,
	/*.kill_channel */ sofia_kill_channel,
	/*.waitfor_read */ sofia_waitfor_read,
	/*.waitfor_read */ sofia_waitfor_write,
	/*.send_dtmf */ sofia_send_dtmf,
	/*.receive_message */ sofia_receive_message,
	/*.receive_event */ sofia_receive_event
};

static const switch_state_handler_table_t sofia_event_handlers = {
	/*.on_init */ sofia_on_init,
	/*.on_ring */ sofia_on_ring,
	/*.on_execute */ sofia_on_execute,
	/*.on_hangup */ sofia_on_hangup,
	/*.on_loopback */ sofia_on_loopback,
	/*.on_transmit */ sofia_on_transmit
};

const switch_endpoint_interface_t sofia_endpoint_interface = {
	/*.interface_name */ "sofia",
	/*.io_routines */ &sofia_io_routines,
	/*.event_handlers */ &sofia_event_handlers,
	/*.private */ NULL,
	/*.next */ NULL
};

static const switch_chat_interface_t sofia_chat_interface = {
	/*.name */ SOFIA_CHAT_PROTO,
	/*.sofia_presence_chat_send */ sofia_presence_chat_send,

};

static switch_status_t sofia_manage(char *relative_oid, switch_management_action_t action, char *data, switch_size_t datalen)
{
	return SWITCH_STATUS_SUCCESS;
}

static const switch_management_interface_t sofia_management_interface = {
	/*.relative_oid */ "1",
	/*.management_function */ sofia_manage
};

static const switch_loadable_module_interface_t sofia_module_interface = {
	/*.module_name */ modname,
	/*.endpoint_interface */ &sofia_endpoint_interface,
	/*.timer_interface */ NULL,
	/*.dialplan_interface */ NULL,
	/*.codec_interface */ NULL,
	/*.application_interface */ NULL,
	/*.api_interface */ NULL,
	/*.file_interface */ NULL,
	/*.speech_interface */ NULL,
	/*.directory_interface */ NULL,
	/*.chat_interface */ &sofia_chat_interface,
	/*.say_interface */ NULL,
	/*.asr_interface */ NULL,
	/*.management_interface */ &sofia_management_interface
};



static switch_call_cause_t sofia_outgoing_channel(switch_core_session_t *session,
												  switch_caller_profile_t *outbound_profile, switch_core_session_t **new_session,
												  switch_memory_pool_t **pool)
{
	switch_call_cause_t cause = SWITCH_CAUSE_DESTINATION_OUT_OF_ORDER;
	switch_core_session_t *nsession;
	char *data, *profile_name, *dest;
	sofia_profile_t *profile;
	switch_caller_profile_t *caller_profile = NULL;
	private_object_t *tech_pvt = NULL;
	switch_channel_t *nchannel;
	char *host, *dest_to;

	*new_session = NULL;

	if (!(nsession = switch_core_session_request(&sofia_endpoint_interface, pool))) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Error Creating Session\n");
		goto done;
	}

	if (!(tech_pvt = (struct private_object *) switch_core_session_alloc(nsession, sizeof(*tech_pvt)))) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Error Creating Session\n");
		sofia_glue_terminate_session(&nsession, SWITCH_CAUSE_DESTINATION_OUT_OF_ORDER, __LINE__);
		goto done;
	}

	data = switch_core_session_strdup(nsession, outbound_profile->destination_number);
	profile_name = data;

	if (!strncasecmp(profile_name, "gateway", 7)) {
		char *gw;
		outbound_reg_t *gateway_ptr;


		if (!(gw = strchr(profile_name, '/'))) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Invalid URL\n");
			sofia_glue_terminate_session(&nsession, SWITCH_CAUSE_DESTINATION_OUT_OF_ORDER, __LINE__);
			cause = SWITCH_CAUSE_INVALID_NUMBER_FORMAT;
			goto done;
		}

		*gw++ = '\0';

		if (!(dest = strchr(gw, '/'))) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Invalid URL\n");
			sofia_glue_terminate_session(&nsession, SWITCH_CAUSE_DESTINATION_OUT_OF_ORDER, __LINE__);
			cause = SWITCH_CAUSE_INVALID_NUMBER_FORMAT;
			goto done;
		}

		*dest++ = '\0';

		if (!(gateway_ptr = sofia_reg_find_gateway(gw))) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Invalid Gateway\n");
			sofia_glue_terminate_session(&nsession, SWITCH_CAUSE_DESTINATION_OUT_OF_ORDER, __LINE__);
			cause = SWITCH_CAUSE_INVALID_NUMBER_FORMAT;
			goto done;
		}

		profile = gateway_ptr->profile;
		tech_pvt->from_str = switch_core_session_strdup(nsession, gateway_ptr->register_from);
		if (!strchr(dest, '@')) {
			tech_pvt->dest = switch_core_session_sprintf(nsession, "sip:%s@%s", dest, gateway_ptr->register_proxy + 4);
		} else {
			tech_pvt->dest = switch_core_session_sprintf(nsession, "sip:%s", dest);
		}
		tech_pvt->invite_contact = switch_core_session_strdup(nsession, gateway_ptr->register_contact);
	} else {
		if (!(dest = strchr(profile_name, '/'))) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Invalid URL\n");
			sofia_glue_terminate_session(&nsession, SWITCH_CAUSE_DESTINATION_OUT_OF_ORDER, __LINE__);
			cause = SWITCH_CAUSE_INVALID_NUMBER_FORMAT;
			goto done;
		}
		*dest++ = '\0';

		if (!(profile = sofia_glue_find_profile(profile_name))) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Invalid Profile\n");
			sofia_glue_terminate_session(&nsession, SWITCH_CAUSE_DESTINATION_OUT_OF_ORDER, __LINE__);
			cause = SWITCH_CAUSE_DESTINATION_OUT_OF_ORDER;
			goto done;
		}

		if ((dest_to = strchr(dest, '^'))) {
			*dest_to++ = '\0';
			tech_pvt->dest_to = switch_core_session_alloc(nsession, strlen(dest_to) + 5);
			snprintf(tech_pvt->dest_to, strlen(dest_to) + 5, "sip:%s", dest_to);
		}

		if ((host = strchr(dest, '%'))) {
			char buf[128];
			*host = '@';
			tech_pvt->e_dest = switch_core_session_strdup(nsession, dest);
			*host++ = '\0';
			if (sofia_reg_find_reg_url(profile, dest, host, buf, sizeof(buf))) {
				tech_pvt->dest = switch_core_session_strdup(nsession, buf);

			} else {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "Cannot locate registered user %s@%s\n", dest, host);
				cause = SWITCH_CAUSE_NO_ROUTE_DESTINATION;
				sofia_glue_terminate_session(&nsession, cause, __LINE__);
				goto done;
			}
		} else if (!strchr(dest, '@')) {
			char buf[128];
			tech_pvt->e_dest = switch_core_session_strdup(nsession, dest);
			if (sofia_reg_find_reg_url(profile, dest, profile_name, buf, sizeof(buf))) {
				tech_pvt->dest = switch_core_session_strdup(nsession, buf);

			} else {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "Cannot locate registered user %s@%s\n", dest, profile_name);
				cause = SWITCH_CAUSE_NO_ROUTE_DESTINATION;
				sofia_glue_terminate_session(&nsession, cause, __LINE__);
				goto done;
			}
		} else {
			tech_pvt->dest = switch_core_session_alloc(nsession, strlen(dest) + 5);
			snprintf(tech_pvt->dest, strlen(dest) + 5, "sip:%s", dest);
		}
	}

	if (!tech_pvt->dest_to) {
		tech_pvt->dest_to = tech_pvt->dest;
	}

	sofia_glue_attach_private(nsession, profile, tech_pvt, dest);

	nchannel = switch_core_session_get_channel(nsession);
	caller_profile = switch_caller_profile_clone(nsession, outbound_profile);
	switch_channel_set_caller_profile(nchannel, caller_profile);
	switch_channel_set_flag(nchannel, CF_OUTBOUND);
	switch_set_flag_locked(tech_pvt, TFLAG_OUTBOUND);
	switch_channel_set_state(nchannel, CS_INIT);
	*new_session = nsession;
	cause = SWITCH_CAUSE_SUCCESS;
	if (session) {
		//char *val;
		//switch_channel_t *channel = switch_core_session_get_channel(session);
		switch_ivr_transfer_variable(session, nsession, SOFIA_REPLACES_HEADER);
		switch_ivr_transfer_variable(session, nsession, SOFIA_SIP_HEADER_PREFIX_T);
		if (switch_core_session_compare(session, nsession)) {
			/* It's another sofia channel! so lets cache what they use as a pt for telephone event so 
			   we can keep it the same
			 */
			private_object_t *ctech_pvt;
			ctech_pvt = switch_core_session_get_private(session);
			assert(ctech_pvt != NULL);
			tech_pvt->bte = ctech_pvt->te;
			tech_pvt->bcng_pt = ctech_pvt->cng_pt;
		}
	}

  done:
	return cause;
}

SWITCH_MOD_DECLARE(switch_status_t) switch_module_load(const switch_loadable_module_interface_t **module_interface, char *filename)
{

	silence_frame.data = silence_data;
	silence_frame.datalen = sizeof(silence_data);
	silence_frame.buflen = sizeof(silence_data);
	silence_frame.flags = SFF_CNG;


	if (switch_core_new_memory_pool(&module_pool) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "OH OH no pool\n");
		return SWITCH_STATUS_TERM;
	}

	memset(&mod_sofia_globals, 0, sizeof(mod_sofia_globals));
	switch_mutex_init(&mod_sofia_globals.mutex, SWITCH_MUTEX_NESTED, module_pool);

	switch_find_local_ip(mod_sofia_globals.guess_ip, sizeof(mod_sofia_globals.guess_ip), AF_INET);

	if (switch_event_bind((char *) modname, SWITCH_EVENT_CUSTOM, MULTICAST_EVENT, event_handler, NULL) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Couldn't bind!\n");
		return SWITCH_STATUS_TERM;
	}

	switch_core_hash_init(&mod_sofia_globals.profile_hash, module_pool);
	switch_core_hash_init(&mod_sofia_globals.gateway_hash, module_pool);
	switch_mutex_init(&mod_sofia_globals.hash_mutex, SWITCH_MUTEX_NESTED, module_pool);

	config_sofia(0);


	if (switch_event_bind((char *) modname, SWITCH_EVENT_PRESENCE_IN, SWITCH_EVENT_SUBCLASS_ANY, sofia_presence_event_handler, NULL)
		!= SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Couldn't bind!\n");
		return SWITCH_STATUS_GENERR;
	}

	if (switch_event_bind((char *) modname, SWITCH_EVENT_PRESENCE_OUT, SWITCH_EVENT_SUBCLASS_ANY, sofia_presence_event_handler, NULL)
		!= SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Couldn't bind!\n");
		return SWITCH_STATUS_GENERR;
	}

	if (switch_event_bind((char *) modname, SWITCH_EVENT_PRESENCE_PROBE, SWITCH_EVENT_SUBCLASS_ANY, sofia_presence_event_handler, NULL)
		!= SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Couldn't bind!\n");
		return SWITCH_STATUS_GENERR;
	}

	if (switch_event_bind((char *) modname, SWITCH_EVENT_ROSTER, SWITCH_EVENT_SUBCLASS_ANY, sofia_presence_event_handler, NULL)
		!= SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Couldn't bind!\n");
		return SWITCH_STATUS_GENERR;
	}

	if (switch_event_bind((char *) modname, SWITCH_EVENT_MESSAGE_WAITING, SWITCH_EVENT_SUBCLASS_ANY, sofia_presence_mwi_event_handler, NULL)
		!= SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Couldn't bind!\n");
		return SWITCH_STATUS_GENERR;
	}

	/* connect my internal structure to the blank pointer passed to me */
	*module_interface = &sofia_module_interface;

	/* indicate that the module should continue to be loaded */
	return SWITCH_STATUS_SUCCESS;

}

SWITCH_MOD_DECLARE(switch_status_t) switch_module_shutdown(void)
{

	sofia_presence_cancel();

	switch_mutex_lock(mod_sofia_globals.mutex);
	if (mod_sofia_globals.running == 1) {
		mod_sofia_globals.running = -1;
	}
	switch_mutex_unlock(mod_sofia_globals.mutex);

	while (mod_sofia_globals.running) {
		switch_yield(1000);
	}

	su_deinit();

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
 * vim:set softtabstop=4 shiftwidth=4 tabstop=4 expandtab:
 */

