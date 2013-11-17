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
 * Anthony Minessale II <anthm@freeswitch.org>
 * William King <william.king@quentustech.com>
 *
 * mod_rtmp.c -- RTMP Endpoint Module
 *
 */

#define BEEN_PAID

#ifdef BEEN_PAID
/* Thanks to Barracuda Networks Inc. for sponsoring this work */
#endif

#include "mod_rtmp.h"

SWITCH_MODULE_LOAD_FUNCTION(mod_rtmp_load);
SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_rtmp_shutdown);
SWITCH_MODULE_RUNTIME_FUNCTION(mod_rtmp_runtime);
SWITCH_MODULE_DEFINITION(mod_rtmp, mod_rtmp_load, mod_rtmp_shutdown, mod_rtmp_runtime);

static switch_status_t config_profile(rtmp_profile_t *profile, switch_bool_t reload);
static switch_xml_config_item_t *get_instructions(rtmp_profile_t *profile);

switch_state_handler_table_t rtmp_state_handlers = {
	/*.on_init */ rtmp_on_init,
	/*.on_routing */ rtmp_on_routing,
	/*.on_execute */ rtmp_on_execute,
	/*.on_hangup */ rtmp_on_hangup,
	/*.on_exchange_media */ rtmp_on_exchange_media,
	/*.on_soft_execute */ rtmp_on_soft_execute,
	/*.on_consume_media */ NULL,
	/*.on_hibernate */ NULL,
	/*.on_reset */ NULL,
	/*.on_park */ NULL,
	/*.on_reporting */ NULL,
	/*.on_destroy */ rtmp_on_destroy
};

switch_io_routines_t rtmp_io_routines = {
	/*.outgoing_channel */ rtmp_outgoing_channel,
	/*.read_frame */ rtmp_read_frame,
	/*.write_frame */ rtmp_write_frame,
	/*.kill_channel */ rtmp_kill_channel,
	/*.send_dtmf */ rtmp_send_dtmf,
	/*.receive_message */ rtmp_receive_message,
	/*.receive_event */ rtmp_receive_event
};

struct mod_rtmp_globals rtmp_globals;

static void rtmp_set_channel_variables(switch_core_session_t *session)
{
	switch_channel_t *channel = switch_core_session_get_channel(session);
	rtmp_private_t *tech_pvt = switch_core_session_get_private(session);
	rtmp_session_t *rsession = tech_pvt->rtmp_session;
	
	switch_channel_set_variable(channel, "rtmp_profile", rsession->profile->name);
	switch_channel_set_variable(channel, "rtmp_session", rsession->uuid);
	switch_channel_set_variable(channel, "rtmp_flash_version", rsession->flashVer);
	switch_channel_set_variable(channel, "rtmp_swf_url", rsession->swfUrl);
	switch_channel_set_variable(channel, "rtmp_tc_url", rsession->tcUrl);
	switch_channel_set_variable(channel, "rtmp_page_url", rsession->pageUrl);
	switch_channel_set_variable(channel, "rtmp_remote_address", rsession->remote_address);
	switch_channel_set_variable_printf(channel, "rtmp_remote_port", "%d", rsession->remote_port);
}

switch_status_t rtmp_tech_init(rtmp_private_t *tech_pvt, rtmp_session_t *rsession, switch_core_session_t *session)
{
	switch_assert(rsession && session && tech_pvt);
	
	tech_pvt->read_frame.data = tech_pvt->databuf;
	tech_pvt->read_frame.buflen = sizeof(tech_pvt->databuf);
	switch_mutex_init(&tech_pvt->mutex, SWITCH_MUTEX_NESTED, switch_core_session_get_pool(session));
	switch_mutex_init(&tech_pvt->flag_mutex, SWITCH_MUTEX_NESTED, switch_core_session_get_pool(session));
	switch_mutex_init(&tech_pvt->readbuf_mutex, SWITCH_MUTEX_NESTED, switch_core_session_get_pool(session));
	
	switch_buffer_create_dynamic(&tech_pvt->readbuf, 512, 512, 1024000);
	//switch_buffer_add_mutex(tech_pvt->readbuf, tech_pvt->readbuf_mutex);
	
	switch_core_timer_init(&tech_pvt->timer, "soft", 20, (16000 / (1000 / 20)), switch_core_session_get_pool(session));
	
	tech_pvt->session = session;
	tech_pvt->rtmp_session = rsession;
	tech_pvt->channel = switch_core_session_get_channel(session);

	/* Initialize read & write codecs */
	if (switch_core_codec_init(&tech_pvt->read_codec, /* name */ "SPEEX", 
		 /* fmtp */ NULL,  /* rate */ 16000, /* ms */ 20, /* channels */ 1, 
		/* flags */ SWITCH_CODEC_FLAG_ENCODE | SWITCH_CODEC_FLAG_DECODE, 
		/* codec settings */ NULL, switch_core_session_get_pool(session)) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Can't initialize read codec\n");
		
		return SWITCH_STATUS_FALSE;
	}

	if (switch_core_codec_init(&tech_pvt->write_codec, /* name */ "SPEEX", 
		 /* fmtp */ NULL,  /* rate */ 16000, /* ms */ 20, /* channels */ 1, 
		/* flags */ SWITCH_CODEC_FLAG_ENCODE | SWITCH_CODEC_FLAG_DECODE, 
		/* codec settings */ NULL, switch_core_session_get_pool(session)) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Can't initialize write codec\n");
		
		return SWITCH_STATUS_FALSE;
	}
	
	switch_core_session_set_read_codec(session, &tech_pvt->read_codec);
	switch_core_session_set_write_codec(session, &tech_pvt->write_codec);
	
	//static inline uint8_t rtmp_audio_codec(int channels, int bits, int rate, rtmp_audio_format_t format) {
	tech_pvt->audio_codec = 0xB2; //rtmp_audio_codec(1, 16, 0 /* speex is always 8000  */, RTMP_AUDIO_SPEEX);
	
	switch_core_session_set_private(session, tech_pvt);
	
	return SWITCH_STATUS_SUCCESS;
}


/* 
   State methods they get called when the state changes to the specific state 
   returning SWITCH_STATUS_SUCCESS tells the core to execute the standard state method next
   so if you fully implement the state you can return SWITCH_STATUS_FALSE to skip it.
*/
switch_status_t rtmp_on_init(switch_core_session_t *session)
{
	switch_channel_t *channel;
	rtmp_private_t *tech_pvt = NULL;
	rtmp_session_t *rsession = NULL;

	tech_pvt = switch_core_session_get_private(session);
	assert(tech_pvt != NULL);

	rsession = tech_pvt->rtmp_session;

	channel = switch_core_session_get_channel(session);
	assert(channel != NULL);

	switch_channel_set_flag(channel, CF_CNG_PLC);
	
	rtmp_notify_call_state(session);
	
	switch_set_flag_locked(tech_pvt, TFLAG_IO);
	
	/* Move channel's state machine to ROUTING. This means the call is trying
	   to get from the initial start where the call because, to the point
	   where a destination has been identified. If the channel is simply
	   left in the initial state, nothing will happen. */
	switch_channel_set_state(channel, CS_ROUTING);

	
	switch_mutex_lock(rsession->profile->mutex);
	rsession->profile->calls++;
	switch_mutex_unlock(rsession->profile->mutex);

	switch_mutex_lock(rsession->count_mutex);
	rsession->active_sessions++;
	switch_mutex_unlock(rsession->count_mutex);

	return SWITCH_STATUS_SUCCESS;
}

switch_status_t rtmp_on_routing(switch_core_session_t *session)
{
	switch_channel_t *channel = NULL;
	rtmp_private_t *tech_pvt = NULL;

	channel = switch_core_session_get_channel(session);
	assert(channel != NULL);

	tech_pvt = switch_core_session_get_private(session);
	assert(tech_pvt != NULL);

	rtmp_notify_call_state(session);
	switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "%s CHANNEL ROUTING\n", switch_channel_get_name(channel));

	return SWITCH_STATUS_SUCCESS;
}

switch_status_t rtmp_on_execute(switch_core_session_t *session)
{

	switch_channel_t *channel = NULL;
	rtmp_private_t *tech_pvt = NULL;

	channel = switch_core_session_get_channel(session);
	assert(channel != NULL);

	tech_pvt = switch_core_session_get_private(session);
	assert(tech_pvt != NULL);

	rtmp_notify_call_state(session);
	switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "%s CHANNEL EXECUTE\n", switch_channel_get_name(channel));


	return SWITCH_STATUS_SUCCESS;
}

switch_status_t rtmp_on_destroy(switch_core_session_t *session)
{
	switch_channel_t *channel = NULL;
	rtmp_private_t *tech_pvt = NULL;

	channel = switch_core_session_get_channel(session);
	assert(channel != NULL);

	tech_pvt = switch_core_session_get_private(session);

	if (tech_pvt) {
		if (switch_core_codec_ready(&tech_pvt->read_codec)) {
			switch_core_codec_destroy(&tech_pvt->read_codec);
		}

		if (switch_core_codec_ready(&tech_pvt->write_codec)) {
			switch_core_codec_destroy(&tech_pvt->write_codec);
		}
		
		switch_buffer_destroy(&tech_pvt->readbuf);
		switch_core_timer_destroy(&tech_pvt->timer);
	}

	return SWITCH_STATUS_SUCCESS;
}


switch_status_t rtmp_on_hangup(switch_core_session_t *session)
{
	switch_channel_t *channel = NULL;
	rtmp_private_t *tech_pvt = NULL;
	rtmp_session_t *rsession = NULL;

	channel = switch_core_session_get_channel(session);
	assert(channel != NULL);

	tech_pvt = switch_core_session_get_private(session);
	assert(tech_pvt != NULL);
	rsession = tech_pvt->rtmp_session;
	switch_clear_flag_locked(tech_pvt, TFLAG_IO);

	if ( rsession == NULL ) {
		/*
		 * If the FS channel is calling hangup, but the rsession is already destroyed, then there is nothing that can be done,
		 * wihtout segfaulting. If there are any actions that need to be done even if the rsession is already destroyed, then move them
		 * above here, or after the done target.
		 */
		goto done;
	}

	switch_thread_rwlock_wrlock(rsession->rwlock);
	//switch_thread_cond_signal(tech_pvt->cond);
	switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "%s CHANNEL HANGUP\n", switch_channel_get_name(channel));

	if (rsession->tech_pvt == tech_pvt) {
		rtmp_private_t *other_tech_pvt = NULL;
		const char *s;
		if ((s = switch_channel_get_variable(channel, RTMP_ATTACH_ON_HANGUP_VARIABLE)) && !zstr(s)) {
			other_tech_pvt = rtmp_locate_private(rsession, s);
		}
		rtmp_attach_private(rsession, other_tech_pvt);
	}

	rtmp_notify_call_state(session);
	rtmp_send_onhangup(session);
	
	/*
	 * If the session_rwlock is already locked, then there is a larger possibility that the rsession
	 * is looping through because the rsession is trying to hang them up. If that is the case, then there
	 * is really no reason to foce this hash_delete. Just timeout, and let the rsession handle the final cleanup 
	 * since it now checks for the existence of the FS session safely.
	 */
	if ( switch_thread_rwlock_trywrlock_timeout(rsession->session_rwlock, 10) == SWITCH_STATUS_SUCCESS) {
		/*
		 * Why the heck would rsession->session_hash ever be null here?!?
		 * We only got here because the tech_pvt->rtmp_session wasn't null....!!!!
		 */
		if ( rsession->session_hash ) {
			switch_core_hash_delete(rsession->session_hash, switch_core_session_get_uuid(session));
		}
		switch_thread_rwlock_unlock(rsession->session_rwlock);
	}
	
#ifndef RTMP_DONT_HOLD
	if (switch_channel_test_flag(channel, CF_HOLD)) {
		switch_channel_mark_hold(channel, SWITCH_FALSE);
		switch_ivr_unhold(session);
	}
#endif

	switch_thread_rwlock_unlock(rsession->rwlock);

 done:
	return SWITCH_STATUS_SUCCESS;
}


switch_status_t rtmp_kill_channel(switch_core_session_t *session, int sig)
{
	switch_channel_t *channel = NULL;
	rtmp_private_t *tech_pvt = NULL;

	channel = switch_core_session_get_channel(session);
	assert(channel != NULL);

	tech_pvt = switch_core_session_get_private(session);
	assert(tech_pvt != NULL);

	switch (sig) {
	case SWITCH_SIG_KILL:
		switch_clear_flag_locked(tech_pvt, TFLAG_IO);
		
		break;
	case SWITCH_SIG_BREAK:
		switch_set_flag_locked(tech_pvt, TFLAG_BREAK);
		break;
	default:
		break;
	}

	return SWITCH_STATUS_SUCCESS;
}

switch_status_t rtmp_on_exchange_media(switch_core_session_t *session)
{
	switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "CHANNEL LOOPBACK\n");
	rtmp_notify_call_state(session);
	return SWITCH_STATUS_SUCCESS;
}

switch_status_t rtmp_on_soft_execute(switch_core_session_t *session)
{
	switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "CHANNEL TRANSMIT\n");
	rtmp_notify_call_state(session);
	return SWITCH_STATUS_SUCCESS;
}

switch_status_t rtmp_send_dtmf(switch_core_session_t *session, const switch_dtmf_t *dtmf)
{
	rtmp_private_t *tech_pvt = switch_core_session_get_private(session);
	switch_assert(tech_pvt != NULL);

	return SWITCH_STATUS_SUCCESS;
}

switch_status_t rtmp_read_frame(switch_core_session_t *session, switch_frame_t **frame, switch_io_flag_t flags, int stream_id)
{
	switch_channel_t *channel = NULL;
	rtmp_private_t *tech_pvt = NULL;
	rtmp_session_t *rsession = NULL;
	//switch_time_t started = switch_time_now();
	//unsigned int elapsed;
	switch_byte_t *data;
	uint16_t len;

	channel = switch_core_session_get_channel(session);
	assert(channel != NULL);

	tech_pvt = switch_core_session_get_private(session);
	assert(tech_pvt != NULL);
	rsession = tech_pvt->rtmp_session;
	
	if (rsession->state >= RS_DESTROY) {
		return SWITCH_STATUS_FALSE;
	}
	
	if (switch_test_flag(tech_pvt, TFLAG_DETACHED)) {
		switch_core_timer_next(&tech_pvt->timer);
		goto cng;
	}
	
	tech_pvt->read_frame.flags = SFF_NONE;
	tech_pvt->read_frame.codec = &tech_pvt->read_codec;
	
	switch_core_timer_next(&tech_pvt->timer);

	if (switch_buffer_inuse(tech_pvt->readbuf) < 2) {
		/* Not enough data in buffer, return CNG frame */
		goto cng;
	} else {
		switch_mutex_lock(tech_pvt->readbuf_mutex);
		switch_buffer_peek(tech_pvt->readbuf, &len, 2);
		if (switch_buffer_inuse(tech_pvt->readbuf) >= len) {
			if (len == 0) {
				switch_mutex_unlock(tech_pvt->readbuf_mutex);
				goto cng;
			} else {
				uint8_t codec;
				
				if (tech_pvt->read_frame.buflen < len) {
					switch_mutex_unlock(tech_pvt->readbuf_mutex);
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Packet of size %u is bigger that the buffer length %u.\n",
						len, tech_pvt->read_frame.buflen);
					return SWITCH_STATUS_FALSE;
				}
				
				switch_buffer_toss(tech_pvt->readbuf, 2);
				switch_buffer_read(tech_pvt->readbuf, &codec, 1); 
				switch_buffer_read(tech_pvt->readbuf, tech_pvt->read_frame.data, len-1);
				tech_pvt->read_frame.datalen = len-1;
				
				if (codec != tech_pvt->audio_codec) {
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Received codec 0x%x instead of 0x%x\n", codec, tech_pvt->audio_codec);
					switch_mutex_unlock(tech_pvt->readbuf_mutex);
					goto cng;
				}
			}
		}
		switch_mutex_unlock(tech_pvt->readbuf_mutex);
	}

	*frame = &tech_pvt->read_frame;
	
	return SWITCH_STATUS_SUCCESS;
	
cng:

	data = (switch_byte_t *) tech_pvt->read_frame.data;
	data[0] = 65;
	data[1] = 0;
	tech_pvt->read_frame.datalen = 2;
	tech_pvt->read_frame.flags = SFF_CNG;
	tech_pvt->read_frame.codec = &tech_pvt->read_codec;

	//switch_core_timer_sync(&tech_pvt->timer);

	*frame = &tech_pvt->read_frame;
	
	return SWITCH_STATUS_SUCCESS;

}

switch_status_t rtmp_write_frame(switch_core_session_t *session, switch_frame_t *frame, switch_io_flag_t flags, int stream_id)
{
	switch_channel_t *channel = NULL;
	rtmp_private_t *tech_pvt = NULL;
	rtmp_session_t *rsession = NULL;
	//switch_frame_t *pframe;
	unsigned char buf[AMF_MAX_SIZE];
	switch_time_t ts;
	
	channel = switch_core_session_get_channel(session);
	assert(channel != NULL);

	tech_pvt = switch_core_session_get_private(session);
	assert(tech_pvt != NULL);
	rsession = tech_pvt->rtmp_session;

	if ( rsession == NULL ) {
		goto error_null;
	}

	switch_thread_rwlock_wrlock(rsession->rwlock);

	if (!switch_test_flag(tech_pvt, TFLAG_IO)) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "TFLAG_IO not set\n");
		goto error;
	}
	
	if (switch_test_flag(tech_pvt, TFLAG_DETACHED) || !switch_test_flag(rsession, SFLAG_AUDIO)) {
		goto success;
	}
	
	if (!rsession || !tech_pvt->audio_codec || !tech_pvt->write_channel) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Missing mandatory value\n");
		goto error;
	}
	
	if (rsession->state >= RS_DESTROY) {
		goto error;
	}

	if (frame->datalen+1 > frame->buflen) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Datalen too big\n");
		goto error;
	}
	
	if (frame->flags & SFF_CNG) {
		goto success;
	}

	/* Build message */
	buf[0] = tech_pvt->audio_codec;
	memcpy(buf+1, frame->data, frame->datalen);
	
	/* Send it down the socket */
	if (!tech_pvt->stream_start_ts) {
		tech_pvt->stream_start_ts = switch_micro_time_now() / 1000;
		ts = 0;
	} else {
		ts = (switch_micro_time_now() / 1000) - tech_pvt->stream_start_ts;
	}

	rtmp_send_message(rsession, RTMP_DEFAULT_STREAM_AUDIO, ts, RTMP_TYPE_AUDIO, rsession->media_streamid, buf, frame->datalen + 1, 0);

 success:
	switch_thread_rwlock_unlock(rsession->rwlock);	
	return SWITCH_STATUS_SUCCESS;

 error:
	switch_thread_rwlock_unlock(rsession->rwlock);
	
 error_null:
	return SWITCH_STATUS_FALSE;
}


switch_status_t rtmp_receive_message(switch_core_session_t *session, switch_core_session_message_t *msg)
{
	switch_channel_t *channel;
	rtmp_private_t *tech_pvt;

	channel = switch_core_session_get_channel(session);
	assert(channel != NULL);

	tech_pvt = (rtmp_private_t *) switch_core_session_get_private(session);
	assert(tech_pvt != NULL);

	switch (msg->message_id) {
	case SWITCH_MESSAGE_INDICATE_ANSWER:
		switch_channel_mark_answered(channel);
		rtmp_notify_call_state(session);
		break;
	case SWITCH_MESSAGE_INDICATE_RINGING:
		switch_channel_mark_ring_ready(channel);
		rtmp_notify_call_state(session);
		break;
	case SWITCH_MESSAGE_INDICATE_PROGRESS:
		switch_channel_mark_pre_answered(channel);
		rtmp_notify_call_state(session);
		break;
	case SWITCH_MESSAGE_INDICATE_HOLD:
	case SWITCH_MESSAGE_INDICATE_UNHOLD:
		rtmp_notify_call_state(session);
		break;
	
	case SWITCH_MESSAGE_INDICATE_DISPLAY:
		{
			const char *name = msg->string_array_arg[0], *number = msg->string_array_arg[1];
			char *arg = NULL;
			char *argv[2] = { 0 };
			//int argc;

			if (zstr(name) && !zstr(msg->string_arg)) {
				arg = strdup(msg->string_arg);
				switch_assert(arg);

				switch_separate_string(arg, '|', argv, (sizeof(argv) / sizeof(argv[0])));
				name = argv[0];
				number = argv[1];

			}
			
			if (!zstr(name)) {
				if (zstr(number)) {
					switch_caller_profile_t *caller_profile = switch_channel_get_caller_profile(channel);
					number = caller_profile->destination_number;
				}
				
				if (zstr(tech_pvt->display_callee_id_name) || strcmp(tech_pvt->display_callee_id_name, name)) {
					tech_pvt->display_callee_id_name = switch_core_session_strdup(session, name);
				}
				
				if (zstr(tech_pvt->display_callee_id_number) || strcmp(tech_pvt->display_callee_id_number, number)) {
					tech_pvt->display_callee_id_number = switch_core_session_strdup(session, number);
				}
				
				rtmp_send_display_update(session);
			}
			
			switch_safe_free(arg);
		}
		break;
	default:
		break;
	}

	return SWITCH_STATUS_SUCCESS;
}

/* Make sure when you have 2 sessions in the same scope that you pass the appropriate one to the routines
   that allocate memory or you will have 1 channel with memory allocated from another channel's pool!
*/
switch_call_cause_t rtmp_outgoing_channel(switch_core_session_t *session, switch_event_t *var_event,
													switch_caller_profile_t *outbound_profile,
													switch_core_session_t **newsession, switch_memory_pool_t **inpool, switch_originate_flag_t flags,
													switch_call_cause_t *cancel_cause)
{
	rtmp_private_t *tech_pvt;
	switch_caller_profile_t *caller_profile;
	switch_channel_t *channel;
	switch_call_cause_t cause = SWITCH_CAUSE_DESTINATION_OUT_OF_ORDER;
	rtmp_session_t *rsession = NULL;
	switch_memory_pool_t *pool;
	char *destination = NULL, *auth, *user, *domain;
	*newsession = NULL;
	
	if (zstr(outbound_profile->destination_number)) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "No destination\n");
		goto fail;
	}

	destination = strdup(outbound_profile->destination_number);
	
	if ((auth = strchr(destination, '/'))) {
		*auth++ = '\0';
	}

	/* Locate the user to be called */
	if (!(rsession = rtmp_session_locate(destination))) {
		cause = SWITCH_CAUSE_NO_ROUTE_DESTINATION;
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "No such session id: %s\n", outbound_profile->destination_number);
		goto fail;
	}
	
	if (!(*newsession = switch_core_session_request_uuid(rtmp_globals.rtmp_endpoint_interface, flags, SWITCH_CALL_DIRECTION_OUTBOUND, inpool, switch_event_get_header(var_event, "origination_uuid")))) {
		goto fail;
	}
	
	pool = switch_core_session_get_pool(*newsession);

 	channel = switch_core_session_get_channel(*newsession);
	switch_channel_set_name(channel, switch_core_session_sprintf(*newsession, "rtmp/%s/%s", rsession->profile->name, outbound_profile->destination_number));
	
	caller_profile = switch_caller_profile_dup(pool, outbound_profile);
	switch_channel_set_caller_profile(channel, caller_profile);
	
	tech_pvt = switch_core_alloc(pool, sizeof(rtmp_private_t));
	tech_pvt->rtmp_session = rsession;
	tech_pvt->write_channel = RTMP_DEFAULT_STREAM_AUDIO;
	tech_pvt->session = *newsession;
	tech_pvt->caller_profile = caller_profile;	
	switch_core_session_add_stream(*newsession, NULL);
	
	if (rtmp_tech_init(tech_pvt, rsession, *newsession) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(*newsession), SWITCH_LOG_ERROR, "tech_init failed\n");
		cause = SWITCH_CAUSE_DESTINATION_OUT_OF_ORDER;
		goto fail;
	}
	
	if (!zstr(auth)) {
		tech_pvt->auth = switch_core_session_strdup(*newsession, auth);
		switch_split_user_domain(auth, &user, &domain);
		tech_pvt->auth_user = switch_core_session_strdup(*newsession, user);
		tech_pvt->auth_domain = switch_core_session_strdup(*newsession, domain);
	}

	/*switch_channel_mark_pre_answered(channel);*/
	
	switch_channel_ring_ready(channel);
	rtmp_send_incoming_call(*newsession, var_event);
	
	switch_channel_set_state(channel, CS_INIT);
	switch_set_flag_locked(tech_pvt, TFLAG_IO);
	
	rtmp_set_channel_variables(*newsession);

	switch_core_hash_insert_wrlock(rsession->session_hash, switch_core_session_get_uuid(*newsession), tech_pvt, rsession->session_rwlock);
		
	if (switch_core_session_thread_launch(tech_pvt->session) == SWITCH_STATUS_FALSE) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Couldn't spawn thread\n");	
		goto fail;
	}

	if (rsession) {
		rtmp_session_rwunlock(rsession);
	}
	
	return SWITCH_CAUSE_SUCCESS;
	
fail:
	if (*newsession) {
		if (!switch_core_session_running(*newsession) && !switch_core_session_started(*newsession)) {
			switch_core_session_destroy(newsession);
		}
	}
	if (rsession) {
		rtmp_session_rwunlock(rsession);
	}
	switch_safe_free(destination);
	return cause;

}

switch_status_t rtmp_receive_event(switch_core_session_t *session, switch_event_t *event)
{
	rtmp_private_t *tech_pvt = switch_core_session_get_private(session);
	rtmp_session_t *rsession = tech_pvt->rtmp_session;
	switch_assert(tech_pvt != NULL);

	/* Deliver the event as a custom message to the target rtmp session */
	switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "Session", switch_core_session_get_uuid(session));
	
	rtmp_send_event(rsession, event);

	return SWITCH_STATUS_SUCCESS;
}


rtmp_profile_t *rtmp_profile_locate(const char *name) 
{
	rtmp_profile_t *profile = switch_core_hash_find_rdlock(rtmp_globals.profile_hash, name, rtmp_globals.profile_rwlock);
	
	if (profile) {
		if (switch_thread_rwlock_tryrdlock(profile->rwlock) != SWITCH_STATUS_SUCCESS) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Profile %s is locked\n", name);
			profile = NULL;
		}
	}

	return profile;
}

void rtmp_profile_release(rtmp_profile_t *profile) 
{
	switch_thread_rwlock_unlock(profile->rwlock);
}

rtmp_session_t *rtmp_session_locate(const char *uuid)
{
	rtmp_session_t *rsession = switch_core_hash_find_rdlock(rtmp_globals.session_hash, uuid, rtmp_globals.session_rwlock);
	
	if (!rsession || rsession->state >= RS_DESTROY) {
		return NULL;
	}
	
	switch_thread_rwlock_rdlock(rsession->rwlock);
	
	return rsession;
}

void rtmp_session_rwunlock(rtmp_session_t *rsession)
{
	switch_thread_rwlock_unlock(rsession->rwlock);
}

void rtmp_event_fill(rtmp_session_t *rsession, switch_event_t *event) 
{
	switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "RTMP-Session-ID", rsession->uuid);
	switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "RTMP-Flash-Version", rsession->flashVer);
	switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "RTMP-SWF-URL", rsession->swfUrl);
	switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "RTMP-TC-URL", rsession->tcUrl);
	switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "RTMP-Page-URL", rsession->pageUrl);
	switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "RTMP-Profile", rsession->profile->name);
	switch_event_add_header(event, SWITCH_STACK_BOTTOM, "Network-Port", "%d", rsession->remote_port);
	switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "Network-IP", rsession->remote_address);
	switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "RTMP-Profile", rsession->profile->name);
}

switch_status_t rtmp_session_request(rtmp_profile_t *profile, rtmp_session_t **newsession)
{
	switch_memory_pool_t *pool;
	switch_uuid_t uuid;
	switch_core_new_memory_pool(&pool);
	*newsession = switch_core_alloc(pool, sizeof(rtmp_session_t));
	
	(*newsession)->pool = pool;
	(*newsession)->profile = profile;
	(*newsession)->in_chunksize = (*newsession)->out_chunksize = RTMP_DEFAULT_CHUNKSIZE;
	(*newsession)->recv_ack_window = RTMP_DEFAULT_ACK_WINDOW;
	(*newsession)->next_streamid = 1;
	(*newsession)->io_private = NULL;
		
	switch_uuid_get(&uuid);
	switch_uuid_format((*newsession)->uuid, &uuid);
	switch_mutex_init(&((*newsession)->socket_mutex), SWITCH_MUTEX_NESTED, pool);
	switch_mutex_init(&((*newsession)->count_mutex), SWITCH_MUTEX_NESTED, pool);
	switch_thread_rwlock_create(&((*newsession)->rwlock), pool);
	switch_thread_rwlock_create(&((*newsession)->account_rwlock), pool);

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "New RTMP session [%s]\n", (*newsession)->uuid);
	switch_core_hash_insert_wrlock(rtmp_globals.session_hash, (*newsession)->uuid, *newsession, rtmp_globals.session_rwlock);
	switch_core_hash_insert_wrlock(profile->session_hash, (*newsession)->uuid, *newsession, profile->session_rwlock);
	
	switch_core_hash_init(&(*newsession)->session_hash, pool);
	switch_thread_rwlock_create(&(*newsession)->session_rwlock, pool);

#ifdef RTMP_DEBUG_IO
	{
		char buf[1024];
		snprintf(buf, sizeof(buf), "/tmp/rtmp-%s-in.txt", (*newsession)->uuid);
		(*newsession)->io_debug_in = fopen(buf, "w");
		snprintf(buf, sizeof(buf), "/tmp/rtmp-%s-out.txt", (*newsession)->uuid);
		(*newsession)->io_debug_out = fopen(buf, "w");
	}
#endif
	
	switch_mutex_lock(profile->mutex);
	profile->clients++;
	switch_mutex_unlock(profile->mutex);
	
	{
		switch_event_t *event;
		if (switch_event_create_subclass(&event, SWITCH_EVENT_CUSTOM, RTMP_EVENT_CONNECT) == SWITCH_STATUS_SUCCESS) {
			rtmp_event_fill(*newsession, event);
			switch_event_fire(&event);
		}
	}
	
	return SWITCH_STATUS_SUCCESS;
}

static void rtmp_garbage_colletor(void)
{
	switch_hash_index_t *hi;

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG1, "RTMP Garbage Collection\n");


	switch_thread_rwlock_wrlock(rtmp_globals.session_rwlock);

 top:

	for (hi = switch_hash_first(NULL, rtmp_globals.session_hash); hi; hi = switch_hash_next(hi)) {
		void *val;	
		const void *key;
		switch_ssize_t keylen;
		rtmp_session_t *rsession;

		switch_hash_this(hi, &key, &keylen, &val);
		rsession = (rtmp_session_t *) val;

		if (rsession->state == RS_DESTROY) {
			if (rtmp_real_session_destroy(&rsession) == SWITCH_STATUS_SUCCESS) {
				goto top;
			}
		}
	}
	
	switch_thread_rwlock_unlock(rtmp_globals.session_rwlock);
}

switch_status_t rtmp_session_destroy(rtmp_session_t **rsession) 
{
	switch_status_t status = SWITCH_STATUS_FALSE;

	switch_mutex_lock(rtmp_globals.mutex);
	if (rsession && *rsession) {
		(*rsession)->state = RS_DESTROY;
		*rsession = NULL;
		status = SWITCH_STATUS_SUCCESS;
	}
	switch_mutex_unlock(rtmp_globals.mutex);

	return status;
}

switch_status_t rtmp_real_session_destroy(rtmp_session_t **rsession) 
{
	switch_hash_index_t *hi;
	switch_event_t *event;
	int sess = 0;

	switch_thread_rwlock_rdlock((*rsession)->session_rwlock);
	for (hi = switch_hash_first(NULL, (*rsession)->session_hash); hi; hi = switch_hash_next(hi)) {
		void *val;	
		const void *key;
		switch_ssize_t keylen;
		switch_channel_t *channel;
		switch_core_session_t *session;

		switch_hash_this(hi, &key, &keylen, &val);		
		
		/* If there are any sessions attached, abort the destroy operation */
		if ((session = switch_core_session_locate((char *)key)) != NULL ) {
			channel = switch_core_session_get_channel(session);
			switch_channel_hangup(channel, SWITCH_CAUSE_DESTINATION_OUT_OF_ORDER);
			switch_core_session_rwunlock(session);
			sess++;
		}
	}
	switch_thread_rwlock_unlock((*rsession)->session_rwlock);

	if (sess) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG1, "RTMP session [%s] %p still busy.\n", (*rsession)->uuid, (void *) *rsession);
		return SWITCH_STATUS_FALSE;
	}
	
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG1, "RTMP session [%s] %p will be destroyed.\n", (*rsession)->uuid, (void *) *rsession);
	
	
	if (switch_event_create_subclass(&event, SWITCH_EVENT_CUSTOM, RTMP_EVENT_DISCONNECT) == SWITCH_STATUS_SUCCESS) {
		rtmp_event_fill(*rsession, event);
		switch_event_fire(&event);
	}
	
	switch_core_hash_delete(rtmp_globals.session_hash, (*rsession)->uuid);
	switch_core_hash_delete_wrlock((*rsession)->profile->session_hash, (*rsession)->uuid, (*rsession)->profile->session_rwlock);
	rtmp_clear_registration(*rsession, NULL, NULL);
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "RTMP session ended [%s]\n", (*rsession)->uuid);
   
	
	switch_mutex_lock((*rsession)->profile->mutex);
	if ( (*rsession)->profile->calls < 1 ) {
		(*rsession)->profile->calls = 0;
	} else {
		(*rsession)->profile->calls--;
	}
	switch_mutex_unlock((*rsession)->profile->mutex);

	switch_thread_rwlock_wrlock((*rsession)->rwlock);
	switch_thread_rwlock_unlock((*rsession)->rwlock);
	
#ifdef RTMP_DEBUG_IO
	fclose((*rsession)->io_debug_in);
	fclose((*rsession)->io_debug_out);
#endif	
	
	switch_mutex_lock((*rsession)->profile->mutex);
	(*rsession)->profile->clients--;
	switch_mutex_unlock((*rsession)->profile->mutex);
	
	switch_core_hash_destroy(&(*rsession)->session_hash);
	
	switch_core_destroy_memory_pool(&(*rsession)->pool);
	
	*rsession = NULL;
	
	return SWITCH_STATUS_SUCCESS;
}

switch_call_cause_t rtmp_session_create_call(rtmp_session_t *rsession, switch_core_session_t **newsession, int read_channel, int write_channel, const char *number, const char *auth_user, const char *auth_domain, switch_event_t *event)
{
	switch_memory_pool_t *pool;
	rtmp_private_t *tech_pvt;
	switch_caller_profile_t *caller_profile;
	switch_channel_t *channel;
	const char *dialplan, *context;
	
	if (!(*newsession = switch_core_session_request(rtmp_globals.rtmp_endpoint_interface, SWITCH_CALL_DIRECTION_INBOUND, SOF_NONE, NULL))) {
		return SWITCH_CAUSE_DESTINATION_OUT_OF_ORDER;
	}
	switch_log_printf(SWITCH_CHANNEL_UUID_LOG(rsession->uuid), SWITCH_LOG_INFO, "New FreeSWITCH session created: %s\n",
					  switch_core_session_get_uuid(*newsession));

	pool = switch_core_session_get_pool(*newsession);	
 	channel = switch_core_session_get_channel(*newsession);
	switch_channel_set_name(channel, switch_core_session_sprintf(*newsession, "rtmp/%s/%s", rsession->profile->name, number));
	
	if (!zstr(auth_user) && !zstr(auth_domain)) {
		const char *s = switch_core_session_sprintf(*newsession, "%s@%s", auth_user, auth_domain);
		switch_ivr_set_user(*newsession, s);
		switch_channel_set_variable(channel, "rtmp_authorized", "true");
	}
	
	if (!(context = switch_channel_get_variable(channel, "user_context"))) {
		if (!(context = rsession->profile->context)) {
			context = "public";
		}
	}
	
	if (!(dialplan = switch_channel_get_variable(channel, "inbound_dialplan"))) {
		if (!(dialplan = rsession->profile->dialplan)) {
			dialplan = "XML";
		}
	}
	
	caller_profile = switch_caller_profile_new(pool, switch_str_nil(auth_user), dialplan, 
		SWITCH_DEFAULT_CLID_NAME, 
		!zstr(auth_user) ? auth_user : SWITCH_DEFAULT_CLID_NUMBER,
		rsession->remote_address /* net addr */, 
		NULL /* ani   */, 
		NULL /* anii  */, 
		NULL /* rdnis */, 
		"mod_rtmp", context, number);
		
	switch_channel_set_caller_profile(channel, caller_profile);
	
	tech_pvt = switch_core_alloc(pool, sizeof(rtmp_private_t));
	tech_pvt->rtmp_session = rsession;
	tech_pvt->write_channel = RTMP_DEFAULT_STREAM_AUDIO;
	tech_pvt->session = *newsession;
	tech_pvt->caller_profile = caller_profile;	
	switch_core_session_add_stream(*newsession, NULL);
	
	if (rtmp_tech_init(tech_pvt, rsession, *newsession) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "tech_init failed\n");
		goto fail;
	}
	
	if (!zstr(auth_user) && !zstr(auth_domain)) {
		tech_pvt->auth_user = switch_core_session_strdup(*newsession, auth_user);
		tech_pvt->auth_domain = switch_core_session_strdup(*newsession, auth_domain);
		tech_pvt->auth = switch_core_session_sprintf(*newsession, "%s@%s", auth_user, auth_domain);
	}

	switch_channel_set_state(channel, CS_INIT);
	switch_set_flag_locked(tech_pvt, TFLAG_IO);
	switch_set_flag_locked(tech_pvt, TFLAG_DETACHED);
	rtmp_set_channel_variables(*newsession);

	if (event) {
		switch_event_header_t *hp;

		for (hp = event->headers; hp; hp = hp->next) {
			switch_channel_set_variable_name_printf(channel, hp->value, RTMP_USER_VARIABLE_PREFIX "_%s", hp->name);
		}
	}

	if (switch_core_session_thread_launch(tech_pvt->session) == SWITCH_STATUS_FALSE) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Couldn't spawn thread\n");
		goto fail;
	}

	switch_core_hash_insert_wrlock(rsession->session_hash, switch_core_session_get_uuid(*newsession), tech_pvt, rsession->session_rwlock);
	
	return SWITCH_CAUSE_SUCCESS;
	
fail:

	if (!switch_core_session_running(*newsession) && !switch_core_session_started(*newsession)) {
		switch_core_session_destroy(newsession);
	}

	return SWITCH_CAUSE_DESTINATION_OUT_OF_ORDER;	
}

switch_status_t rtmp_profile_start(const char *profilename)
{
	switch_memory_pool_t *pool;
	rtmp_profile_t *profile;
	
	switch_assert(profilename);
	
	switch_core_new_memory_pool(&pool);
	profile = switch_core_alloc(pool, sizeof(*profile));
	profile->pool = pool;
	profile->name = switch_core_strdup(pool, profilename);

	if (config_profile(profile, SWITCH_FALSE) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Config failed\n");
		goto fail;
	}

	switch_thread_rwlock_create(&profile->rwlock, pool);
	switch_mutex_init(&profile->mutex, SWITCH_MUTEX_NESTED, pool);
	switch_core_hash_init(&profile->session_hash, pool);
	switch_thread_rwlock_create(&profile->session_rwlock, pool);
	switch_thread_rwlock_create(&profile->reg_rwlock, pool);
	switch_core_hash_init(&profile->reg_hash, pool);
	
	if (!strcmp(profile->io_name, "tcp")) {
		if (rtmp_tcp_init(profile, profile->bind_address, &profile->io, pool) != SWITCH_STATUS_SUCCESS) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Couldn't initialize I/O layer\n");
			goto fail;
		}	
	} else {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "No such I/O module [%s]\n", profile->io_name);
		goto fail;
	}
	
	switch_core_hash_insert_wrlock(rtmp_globals.profile_hash, profile->name, profile, rtmp_globals.profile_rwlock);
	
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "Started profile %s\n", profile->name);
	
	return SWITCH_STATUS_SUCCESS;
fail:
	switch_core_destroy_memory_pool(&pool);
	return SWITCH_STATUS_FALSE;
}

switch_status_t rtmp_profile_destroy(rtmp_profile_t **profile) {
	int sanity = 0;
	switch_hash_index_t *hi;
	switch_xml_config_item_t *instructions = get_instructions(*profile);
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "Stopping profile: %s\n", (*profile)->name);
	
	switch_core_hash_delete_wrlock(rtmp_globals.profile_hash, (*profile)->name, rtmp_globals.profile_rwlock);
	
	switch_thread_rwlock_wrlock((*profile)->rwlock);
	
	/* Kill all sessions */	
	while ((hi = switch_hash_first(NULL, (*profile)->session_hash))) {
		void *val;
		rtmp_session_t *session;
		const void *key;
		switch_ssize_t keylen;
		switch_hash_this(hi, &key, &keylen, &val);
		
		session = val;				
			
		rtmp_session_destroy(&session);
	}
	
	if ((*profile)->io->running > 0) {
		(*profile)->io->running = 0;
	
		while (sanity++ < 100 && (*profile)->io->running == 0) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Waiting for thread to end\n");
			switch_yield(500000);
		}
	}
	
	switch_thread_rwlock_unlock((*profile)->rwlock);
	
	switch_xml_config_cleanup(instructions);
	
	switch_core_hash_destroy(&(*profile)->session_hash);
	switch_core_hash_destroy(&(*profile)->reg_hash);
	
	switch_core_destroy_memory_pool(&(*profile)->pool);
	
	free(instructions);
	
	return SWITCH_STATUS_SUCCESS;
}


void rtmp_add_registration(rtmp_session_t *rsession, const char *auth, const char *nickname) 
{
	rtmp_reg_t *current_reg;
	rtmp_reg_t *reg;
	switch_event_t *event;

	if (zstr(auth)) {
		return;
	}
	
	reg = switch_core_alloc(rsession->pool, sizeof(*reg));
	reg->uuid = rsession->uuid;

	if (!zstr(nickname)) {
		reg->nickname = switch_core_strdup(rsession->pool, nickname);		
	}
	
	switch_thread_rwlock_wrlock(rsession->profile->reg_rwlock);
	if ((current_reg = switch_core_hash_find(rsession->profile->reg_hash, auth))) {
		for (;current_reg && current_reg->next; current_reg = current_reg->next);
		current_reg->next = reg;
	} else {
		switch_core_hash_insert(rsession->profile->reg_hash, auth, reg);
	}
	switch_thread_rwlock_unlock(rsession->profile->reg_rwlock);
	
	if (switch_event_create_subclass(&event, SWITCH_EVENT_CUSTOM, RTMP_EVENT_REGISTER) == SWITCH_STATUS_SUCCESS) {
		char *user, *domain, *dup;
		char *url = NULL;
		char *token = NULL;
		char network_port_c[6];
		snprintf(network_port_c, sizeof(network_port_c), "%d", rsession->remote_port);
		rtmp_event_fill(rsession, event);
		
		dup = strdup(auth);
		switch_split_user_domain(dup, &user, &domain);

		url = switch_mprintf("rtmp/%s/%s@%s", rsession->uuid, user, domain);
		token = switch_mprintf("rtmp/%s/%s@%s/%s", rsession->uuid, user, domain, nickname);

		reg->user = switch_core_strdup(rsession->pool, user);
		reg->domain = switch_core_strdup(rsession->pool, domain);
			
		switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "User", user);
		switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "Domain", domain);
		switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "Nickname", switch_str_nil(nickname));
		switch_event_fire(&event);
		switch_core_add_registration(user, domain, token, url, 0, rsession->remote_address, network_port_c, "tcp", "");
		free(dup);
		switch_safe_free(url);
		switch_safe_free(token);
	}

}

static void rtmp_clear_reg_auth(rtmp_session_t *rsession, const char *auth, const char *nickname)
{
	rtmp_reg_t *reg, *prev = NULL;
	switch_thread_rwlock_wrlock(rsession->profile->reg_rwlock);
	if ((reg = switch_core_hash_find(rsession->profile->reg_hash, auth))) {
		for (; reg; reg = reg->next) {
			if (!zstr(reg->uuid) && !strcmp(reg->uuid, rsession->uuid) && (zstr(nickname) || !strcmp(reg->nickname, nickname))) {
				switch_event_t *event;
				if (prev) {
					prev->next = reg->next;
				} else {
					/* Replace hash entry by its next ptr */
					switch_core_hash_insert(rsession->profile->reg_hash, auth, reg->next);
				}

				if (switch_event_create_subclass(&event, SWITCH_EVENT_CUSTOM, RTMP_EVENT_UNREGISTER) == SWITCH_STATUS_SUCCESS) {
					rtmp_event_fill(rsession, event);
					switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "User", reg->user);
					switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "Domain", reg->domain);
					switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "Nickname", switch_str_nil(reg->nickname));
					switch_event_fire(&event);
				}
			}
			prev = reg;
		}
	}
	switch_thread_rwlock_unlock(rsession->profile->reg_rwlock);
}


void rtmp_clear_registration(rtmp_session_t *rsession, const char *auth, const char *nickname)
{	
	rtmp_account_t *account;
	
	if (zstr(auth)) {
		/* Reg data is pool-allocated, no need to free them */
		switch_thread_rwlock_rdlock(rsession->account_rwlock);
		for (account = rsession->account; account; account = account->next) {
			char *token = NULL;
			char buf[1024];
			snprintf(buf, sizeof(buf), "%s@%s", account->user, account->domain);
			rtmp_clear_reg_auth(rsession, buf, nickname);
			token = switch_mprintf("rtmp/%s/%s@%s/%s", rsession->uuid, account->user, account->domain, nickname);
			switch_core_del_registration(account->user, account->domain, token);
			switch_safe_free(token);
		}
		switch_thread_rwlock_unlock(rsession->account_rwlock);
	} else {
		rtmp_clear_reg_auth(rsession, auth, nickname);
	}
	
}

switch_status_t rtmp_session_login(rtmp_session_t *rsession, const char *user, const char *domain)
{
	rtmp_account_t *account = switch_core_alloc(rsession->pool, sizeof(*account));
	switch_event_t *event;
	
	account->user = switch_core_strdup(rsession->pool, user);
	account->domain = switch_core_strdup(rsession->pool, domain);
	
	switch_thread_rwlock_wrlock(rsession->account_rwlock);
	account->next = rsession->account;
	rsession->account = account;
	switch_thread_rwlock_unlock(rsession->account_rwlock);
	
	rtmp_send_invoke_free(rsession, 3, 0, 0,
		amf0_str("onLogin"),
		amf0_number_new(0),
		amf0_null_new(),
		amf0_str("success"),
		amf0_str(user),
		amf0_str(domain), NULL);
		
	if (switch_event_create_subclass(&event, SWITCH_EVENT_CUSTOM, RTMP_EVENT_LOGIN) == SWITCH_STATUS_SUCCESS) {
		rtmp_event_fill(rsession, event);
		switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "User", user);
		switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "Domain", domain);
		switch_event_fire(&event);
	}
	
	switch_log_printf(SWITCH_CHANNEL_UUID_LOG(rsession->uuid), SWITCH_LOG_INFO, "RTMP Session [%s] is now logged into %s@%s\n", rsession->uuid, user, domain);	
	
	return SWITCH_STATUS_SUCCESS;
}

switch_status_t rtmp_session_logout(rtmp_session_t *rsession, const char *user, const char *domain)
{
	rtmp_account_t *account;
	switch_event_t *event;
	
	switch_thread_rwlock_wrlock(rsession->account_rwlock);
	for (account = rsession->account; account; account = account->next) {
		if (!strcmp(account->user, user) && !strcmp(account->domain, domain)) {
			rsession->account = account->next;
		}
	}
	switch_thread_rwlock_unlock(rsession->account_rwlock);
	
	rtmp_send_invoke_free(rsession, 3, 0, 0,
		amf0_str("onLogout"),
		amf0_number_new(0),
		amf0_null_new(),
		amf0_str(user),
		amf0_str(domain), NULL);
	
	if (switch_event_create_subclass(&event, SWITCH_EVENT_CUSTOM, RTMP_EVENT_LOGOUT) == SWITCH_STATUS_SUCCESS) {
		rtmp_event_fill(rsession, event);
		switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "User", user);
		switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "Domain", domain);
		switch_event_fire(&event);
	}
	
	switch_log_printf(SWITCH_CHANNEL_UUID_LOG(rsession->uuid), SWITCH_LOG_INFO, "RTMP Session [%s] is now logged out of %s@%s\n", rsession->uuid, user, domain);
	
	return SWITCH_STATUS_SUCCESS;
}

switch_status_t rtmp_session_check_user(rtmp_session_t *rsession, const char *user, const char *domain)
{
	rtmp_account_t *account;
	switch_status_t status = SWITCH_STATUS_FALSE;
	
	switch_thread_rwlock_rdlock(rsession->account_rwlock);
	if (user && domain) {
		for (account = rsession->account; account; account = account->next) {
			if (account->user && account->domain && !strcmp(account->user, user) && !strcmp(account->domain, domain)) {
				status = SWITCH_STATUS_SUCCESS;
				break;
			}
		}
	}
	switch_thread_rwlock_unlock(rsession->account_rwlock);
	
	return status;
}

void rtmp_attach_private(rtmp_session_t *rsession, rtmp_private_t *tech_pvt) 
{	
	switch_event_t *event;
	
	if (rsession->tech_pvt) {
		/* Detach current call */
		switch_set_flag_locked(rsession->tech_pvt, TFLAG_DETACHED);
#ifndef RTMP_DONT_HOLD
		switch_ivr_hold(rsession->tech_pvt->session, NULL, SWITCH_TRUE);
		switch_channel_mark_hold(rsession->tech_pvt->channel, SWITCH_FALSE);
#endif
		if (switch_event_create_subclass(&event, SWITCH_EVENT_CUSTOM, RTMP_EVENT_DETACH) == SWITCH_STATUS_SUCCESS) {
			rtmp_event_fill(rsession, event);
			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "Call-ID", 
				switch_core_session_get_uuid(rsession->tech_pvt->session));
			switch_event_fire(&event);
		}
		
		rsession->tech_pvt = NULL;
	}
	
	if (tech_pvt && switch_test_flag(tech_pvt, TFLAG_THREE_WAY)) {
		const char *s = switch_channel_get_variable(tech_pvt->channel, RTMP_THREE_WAY_UUID_VARIABLE);
		/* 2nd call of a three-way: attach to other call instead */
		if (!zstr(s)) {
			tech_pvt = rtmp_locate_private(rsession, s);
		} else {
			tech_pvt = NULL;
		}
	}
	
	rsession->tech_pvt = tech_pvt;
	
	if (tech_pvt) {
		/* Attach new call */
		switch_clear_flag_locked(tech_pvt, TFLAG_DETACHED);
		
#ifndef RTMP_DONT_HOLD
		if (switch_channel_test_flag(tech_pvt->channel, CF_HOLD)) {
			switch_channel_mark_hold(tech_pvt->channel, SWITCH_FALSE);
			switch_ivr_unhold(tech_pvt->session);	
		}
#endif
		if (switch_event_create_subclass(&event, SWITCH_EVENT_CUSTOM, RTMP_EVENT_ATTACH) == SWITCH_STATUS_SUCCESS) {
			rtmp_event_fill(rsession, event);
			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "Call-ID", switch_core_session_get_uuid(tech_pvt->session));
			switch_event_fire(&event);
		}
	}

	/* Let the UI know to which call it has connected */
	rtmp_session_send_onattach(rsession);
}

rtmp_private_t *rtmp_locate_private(rtmp_session_t *rsession, const char *uuid)
{
	return switch_core_hash_find_rdlock(rsession->session_hash, uuid, rsession->session_rwlock);
}

static switch_xml_config_item_t *get_instructions(rtmp_profile_t *profile) {
	switch_xml_config_item_t *dup;
	static switch_xml_config_int_options_t opt_chunksize = { 
		SWITCH_TRUE,  /* enforce min */
		128,
		SWITCH_TRUE, /* Enforce Max */
		65536
	};
	static switch_xml_config_int_options_t opt_bufferlen = {
		SWITCH_FALSE,
		0,
		SWITCH_TRUE,
		INT32_MAX
	};
	switch_xml_config_item_t instructions[] = {
		/* parameter name        type                 reloadable   pointer                         default value     options structure */
		SWITCH_CONFIG_ITEM("context", SWITCH_CONFIG_STRING, CONFIG_RELOADABLE, &profile->context, "public", &switch_config_string_strdup,
						   "", "The dialplan context to use for inbound calls"),
		SWITCH_CONFIG_ITEM("dialplan", SWITCH_CONFIG_STRING, CONFIG_RELOADABLE, &profile->dialplan, "XML", &switch_config_string_strdup,
						   "", "The dialplan to use for inbound calls"),
		SWITCH_CONFIG_ITEM("bind-address", SWITCH_CONFIG_STRING, 0, &profile->bind_address, "0.0.0.0:1935", &switch_config_string_strdup,
						   "ip:port", "IP and port to bind"),
		SWITCH_CONFIG_ITEM("io", SWITCH_CONFIG_STRING, 0, &profile->io_name, "tcp", &switch_config_string_strdup,
						   "io module", "I/O module to use (if unsure use tcp)"),
		SWITCH_CONFIG_ITEM("auth-calls", SWITCH_CONFIG_BOOL, CONFIG_RELOADABLE, &profile->auth_calls, SWITCH_FALSE, NULL, "true|false", "Set to true in order to reject unauthenticated calls"),
		SWITCH_CONFIG_ITEM("chunksize", SWITCH_CONFIG_INT, CONFIG_RELOADABLE, &profile->chunksize, 128, &opt_chunksize, "", "RTMP Sending chunksize"),
		SWITCH_CONFIG_ITEM("buffer-len", SWITCH_CONFIG_INT, CONFIG_RELOADABLE, &profile->buffer_len, 500, &opt_bufferlen, "", "Length of the receiving buffer to be used by the flash clients, in miliseconds"),
		SWITCH_CONFIG_ITEM_END()
	};
	
	dup = malloc(sizeof(instructions));
	memcpy(dup, instructions, sizeof(instructions));
	return dup;
}

static switch_status_t config_profile(rtmp_profile_t *profile, switch_bool_t reload)
{
	switch_xml_t cfg, xml, x_profiles, x_profile, x_settings;
	switch_status_t status = SWITCH_STATUS_FALSE;
	switch_xml_config_item_t *instructions = (profile ? get_instructions(profile) : NULL);
	switch_event_t *event = NULL;
	int count;
	const char *file = "rtmp.conf";
	
	if (!(xml = switch_xml_open_cfg(file, &cfg, NULL))) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Could not open %s\n", file);
		goto done;
	}
	
	if (!(x_profiles = switch_xml_child(cfg, "profiles"))) {
		goto done;
	}
	
	for (x_profile = switch_xml_child(x_profiles, "profile"); x_profile; x_profile = x_profile->next) {
		const char *name = switch_xml_attr_soft(x_profile, "name");
		if (strcmp(name, profile->name)) {
			continue;
		}
		
		if (!(x_settings = switch_xml_child(x_profile, "settings"))) {
			goto done;
		}
		
		
		count = switch_event_import_xml(switch_xml_child(x_settings, "param"), "name", "value", &event);
		status = switch_xml_config_parse_event(event, count, reload, instructions);
	}
	
	
done:
	if (xml) {
		switch_xml_free(xml);	
	}
	switch_safe_free(instructions);
	if (event) {
		switch_event_destroy(&event);
	}
	return status;
}

static void rtmp_event_handler(switch_event_t *event)
{
	rtmp_session_t *rsession;
	const char *uuid;
	
	if (!event) {
		return;
	}
	
	uuid = switch_event_get_header(event, "RTMP-Session-ID");
	if (zstr(uuid)) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "RTMP Custom event without RTMP-Session-ID\n");
		return;
	}
	
	if ((rsession = rtmp_session_locate(uuid))) {
		rtmp_send_event(rsession, event);
		rtmp_session_rwunlock(rsession);
	}
}

#define RTMP_CONTACT_FUNCTION_SYNTAX "profile/user@domain[/[!]nickname]"
SWITCH_STANDARD_API(rtmp_contact_function)
{
	int argc;
	char *argv[5];
	char *dup = NULL;
	char *szprofile = NULL, *user = NULL; 
	const char *nickname = NULL;
	rtmp_profile_t *profile = NULL;
	rtmp_reg_t *reg;
	switch_bool_t first = SWITCH_TRUE;
	
	if (zstr(cmd)) {
		goto usage;
	}
	
	dup = strdup(cmd);
	argc = switch_split(dup, '/', argv);
	
	if (argc < 2 || zstr(argv[0]) || zstr(argv[1])) {
		goto usage;
	}
	
	szprofile = argv[0];
	if (!strchr(argv[1], '@')) {
		goto usage;
	} 
	
	user = argv[1];
	nickname = argv[2];
	
	if (!(profile = rtmp_profile_locate(szprofile))) {
		stream->write_function(stream, "-ERR No such profile\n");
		goto done;
	}
	
	switch_thread_rwlock_rdlock(profile->reg_rwlock);
	if ((reg = switch_core_hash_find(profile->reg_hash, user))) {		
		for (; reg; reg = reg->next) {
			if (zstr(nickname) || 
				(nickname[0] == '!' && (zstr(reg->nickname) || strcmp(reg->nickname, nickname+1))) || 
				(!zstr(reg->nickname) && !strcmp(reg->nickname, nickname))) {
				if (!first) {
					stream->write_function(stream, ",");
				} else {
					first = SWITCH_FALSE;
				}
				stream->write_function(stream, "rtmp/%s/%s", reg->uuid, user);
			}
		}
	} else {
		stream->write_function(stream, "error/user_not_registered");
	}
	switch_thread_rwlock_unlock(profile->reg_rwlock);
	goto done;
	
usage:
	stream->write_function(stream, "Usage: rtmp_contact "RTMP_CONTACT_FUNCTION_SYNTAX"\n");

done:
	if (profile) {
		rtmp_profile_release(profile);
	}
	switch_safe_free(dup);
	return SWITCH_STATUS_SUCCESS;
}

static const char *state2name(int state)
{
 switch(state) {
 case RS_HANDSHAKE:
	 return "HANDSHAKE";
 case RS_HANDSHAKE2:
	 return "HANDSHAKE2";
 case RS_ESTABLISHED:
	 return "ESTABLISHED";
 default:
	 return "DESTROY (PENDING)";
 }
}

#define RTMP_FUNCTION_SYNTAX "profile [profilename] [start | stop | rescan | restart]\nstatus profile [profilename]\nstatus profile [profilename] [reg | sessions]\nsession [session_id] [kill | login [user@domain] | logout [user@domain]]"
SWITCH_STANDARD_API(rtmp_function)
{
	int argc;
	char *argv[10];
	char *dup = NULL;
	
	if (zstr(cmd)) {
		goto usage;
	}
	
	dup = strdup(cmd);
	argc = switch_split(dup, ' ', argv);
	
	if (argc < 1 || zstr(argv[0])) {
		goto usage;
	}
	
	if (!strcmp(argv[0], "profile")) {
		if (zstr(argv[1]) || zstr(argv[2])) {
			goto usage;
		}
		if (!strcmp(argv[2], "start")) {
			rtmp_profile_t *profile = rtmp_profile_locate(argv[1]);
			if (profile) {
				rtmp_profile_release(profile);
				stream->write_function(stream, "-ERR Profile %s is already started\n", argv[2]);
			} else {
				rtmp_profile_start(argv[1]);
				stream->write_function(stream, "+OK\n");
			}
		} else if (!strcmp(argv[2], "stop")) {
			rtmp_profile_t *profile = rtmp_profile_locate(argv[1]);
			if (profile) {
				rtmp_profile_release(profile);
				rtmp_profile_destroy(&profile);
				stream->write_function(stream, "+OK\n");
			} else {
				stream->write_function(stream, "-ERR No such profile\n");
			}
		} else if (!strcmp(argv[2], "rescan")) {
			rtmp_profile_t *profile = rtmp_profile_locate(argv[1]);
			if (config_profile(profile, SWITCH_TRUE) == SWITCH_STATUS_SUCCESS) {
				stream->write_function(stream, "+OK\n");
			} else {
				stream->write_function(stream, "-ERR Config error\n");
			}
			rtmp_profile_release(profile);
		} else if (!strcmp(argv[2], "restart")) {
			rtmp_profile_t *profile = rtmp_profile_locate(argv[1]);
			if (profile) {
				rtmp_profile_release(profile);
				rtmp_profile_destroy(&profile);
				rtmp_profile_start(argv[1]);
				stream->write_function(stream, "+OK\n");
			} else {
				rtmp_profile_start(argv[1]);
				stream->write_function(stream, "-OK (wasn't started, started anyways)\n");
			}
		} else {
			goto usage;
		}
	} else if (!strcmp(argv[0], "status")) {
		if (!zstr(argv[1]) && !strcmp(argv[1], "profile") && !zstr(argv[2])) {
			rtmp_profile_t *profile;
			
			if ((profile = rtmp_profile_locate(argv[2]))) {
				stream->write_function(stream, "Profile: %s\n", profile->name);
				stream->write_function(stream, "I/O Backend: %s\n", profile->io->name);
				stream->write_function(stream, "Bind address: %s\n", profile->io->address);
				stream->write_function(stream, "Active calls: %d\n", profile->calls);
				
				if (!zstr(argv[3]) && !strcmp(argv[3], "sessions"))
				{
					switch_hash_index_t *hi;
					stream->write_function(stream, "\nSessions:\n");
					stream->write_function(stream, "uuid,address,user,domain,flashVer,state\n");
					switch_thread_rwlock_rdlock(profile->session_rwlock);
					for (hi = switch_hash_first(NULL, profile->session_hash); hi; hi = switch_hash_next(hi)) {
						void *val;	
						const void *key;
						switch_ssize_t keylen;
						rtmp_session_t *item;
						switch_hash_this(hi, &key, &keylen, &val);
											
						item = (rtmp_session_t *)val;
						stream->write_function(stream, "%s,%s:%d,%s,%s,%s,%s\n", 
											   item->uuid, item->remote_address, item->remote_port,
											   item->account ? item->account->user : NULL,
											   item->account ? item->account->domain : NULL,
											   item->flashVer, state2name(item->state));
						
					}
					switch_thread_rwlock_unlock(profile->session_rwlock);
				} else if (!zstr(argv[3]) && !strcmp(argv[3], "reg")) {
					switch_hash_index_t *hi;
					stream->write_function(stream, "\nRegistrations:\n");
					stream->write_function(stream, "user,nickname,uuid\n");
					
					switch_thread_rwlock_rdlock(profile->reg_rwlock);
					for (hi = switch_hash_first(NULL, profile->reg_hash); hi; hi = switch_hash_next(hi)) {
						void *val;	
						const void *key;
						switch_ssize_t keylen;
						rtmp_reg_t *item;
						switch_hash_this(hi, &key, &keylen, &val);
											
						item = (rtmp_reg_t *)val;
						for (;item;item = item->next) {
							stream->write_function(stream, "%s,%s,%s\n",
								key, switch_str_nil(item->nickname), item->uuid);
						}
					}
					switch_thread_rwlock_unlock(profile->reg_rwlock);	
				} else {
                    stream->write_function(stream, "Dialplan: %s\n", profile->dialplan);
                    stream->write_function(stream, "Context: %s\n", profile->context);
				}
				
				rtmp_profile_release(profile);
			} else {
				stream->write_function(stream, "-ERR No such profile [%s]\n", argv[2]);
			}
		} else {
			switch_hash_index_t *hi;
			switch_thread_rwlock_rdlock(rtmp_globals.profile_rwlock);
			for (hi = switch_hash_first(NULL, rtmp_globals.profile_hash); hi; hi = switch_hash_next(hi)) {
				void *val;	
				const void *key;
				switch_ssize_t keylen;
				rtmp_profile_t *item;
				switch_hash_this(hi, &key, &keylen, &val);
									
				item = (rtmp_profile_t *)val;
				stream->write_function(stream, "%s\t%s:%s\tprofile\n", item->name, item->io->name, item->io->address);
				
			}
			switch_thread_rwlock_unlock(rtmp_globals.profile_rwlock);
		}
		
	} else if (!strcmp(argv[0], "session")) {
		rtmp_session_t *rsession;
		
		if (zstr(argv[1]) || zstr(argv[2])) {
			goto usage;
		}
		
		rsession = rtmp_session_locate(argv[1]);
		if (!rsession) {
			stream->write_function(stream, "-ERR No such session\n");
			goto done;
		}
		
		if (!strcmp(argv[2], "login")) {
			char *user, *domain;
			if (zstr(argv[3])) {
				goto usage;
			}
			switch_split_user_domain(argv[3], &user, &domain);
			
			if (!zstr(user) && !zstr(domain)) {
				rtmp_session_login(rsession, user, domain);
				stream->write_function(stream, "+OK\n");
			} else {
				stream->write_function(stream, "-ERR I need user@domain\n");
			}
		} else 	if (!strcmp(argv[2], "logout")) {
			char *user, *domain;
			if (zstr(argv[3])) {
				goto usage;
			}
			switch_split_user_domain(argv[3], &user, &domain);
			
			if (!zstr(user) && !zstr(domain)) {
				rtmp_session_logout(rsession, user, domain);
				stream->write_function(stream, "+OK\n");
			} else {
				stream->write_function(stream, "-ERR I need user@domain\n");
			}
		} else 	if (!strcmp(argv[2], "kill")) {
			rtmp_session_rwunlock(rsession);
			rtmp_session_destroy(&rsession);
			stream->write_function(stream, "+OK\n");
		} else if (!strcmp(argv[2], "call")) {
			switch_core_session_t *newsession = NULL;
			char *dest = argv[3];
			char *user = argv[4];
			char *domain = NULL;
			
			if (!zstr(user) && (domain = strchr(user, '@'))) {
				*domain++ = '\0';
			}
			
			if (!zstr(dest)) {
					if (rtmp_session_create_call(rsession, &newsession, 0, RTMP_DEFAULT_STREAM_AUDIO, dest, user, domain, NULL) != SWITCH_CAUSE_SUCCESS) {
						stream->write_function(stream, "-ERR Couldn't create new call\n");
					} else {
						rtmp_private_t *new_pvt = switch_core_session_get_private(newsession);
						rtmp_send_invoke_free(rsession, 3, 0, 0,
							amf0_str("onMakeCall"),
							amf0_number_new(0),
							amf0_null_new(),
							amf0_str(switch_core_session_get_uuid(newsession)),
							amf0_str(switch_str_nil(dest)),
							amf0_str(switch_str_nil(new_pvt->auth)),
							NULL);

						rtmp_attach_private(rsession, switch_core_session_get_private(newsession));
						stream->write_function(stream, "+OK\n");
					}
			} else {
				stream->write_function(stream, "-ERR Missing destination number\n");
			}
		} else if (!strcmp(argv[2], "ping")) {
			rtmp_ping(rsession);
			stream->write_function(stream, "+OK\n");
		} else {
			stream->write_function(stream, "-ERR No such session action [%s]\n", argv[2]);
		}
		
		if (rsession) {
			rtmp_session_rwunlock(rsession);
		}
	} else {
		goto usage;
	}
	
	goto done;
	
usage:
	stream->write_function(stream, "-ERR Usage: "RTMP_FUNCTION_SYNTAX"\n");

done:
	switch_safe_free(dup);
	return SWITCH_STATUS_SUCCESS;
}

static inline void rtmp_register_invoke_function(const char *name, rtmp_invoke_function_t func) 
{
	switch_core_hash_insert(rtmp_globals.invoke_hash, name, (void*)(intptr_t)func);
}

static switch_status_t console_complete_hashtable(switch_hash_t *hash, const char *line, const char *cursor, switch_console_callback_match_t **matches)
{
	switch_hash_index_t *hi;
	void *val;
	const void *vvar;
	switch_console_callback_match_t *my_matches = NULL;
	switch_status_t status = SWITCH_STATUS_FALSE;

	for (hi = switch_hash_first(NULL, hash); hi; hi = switch_hash_next(hi)) {
		switch_hash_this(hi, &vvar, NULL, &val);
		switch_console_push_match(&my_matches, (const char *) vvar);
	}

	if (my_matches) {
		*matches = my_matches;
		status = SWITCH_STATUS_SUCCESS;
	}

	return status;
}

static switch_status_t list_sessions(const char *line, const char *cursor, switch_console_callback_match_t **matches)
{
	switch_status_t status;
	switch_thread_rwlock_rdlock(rtmp_globals.session_rwlock);
	status = console_complete_hashtable(rtmp_globals.session_hash, line, cursor, matches);
	switch_thread_rwlock_unlock(rtmp_globals.session_rwlock);
	return status;
}


static switch_status_t list_profiles(const char *line, const char *cursor, switch_console_callback_match_t **matches)
{
	switch_status_t status;
	switch_thread_rwlock_rdlock(rtmp_globals.profile_rwlock);
	status = console_complete_hashtable(rtmp_globals.profile_hash, line, cursor, matches);
	switch_thread_rwlock_unlock(rtmp_globals.profile_rwlock);
	return status;
}

SWITCH_MODULE_LOAD_FUNCTION(mod_rtmp_load)
{
	switch_api_interface_t *api_interface;
	rtmp_globals.pool = pool;

	memset(&rtmp_globals, 0, sizeof(rtmp_globals));

	switch_mutex_init(&rtmp_globals.mutex, SWITCH_MUTEX_NESTED, pool);
	switch_core_hash_init(&rtmp_globals.profile_hash, pool);
	switch_core_hash_init(&rtmp_globals.session_hash, pool);
	switch_core_hash_init(&rtmp_globals.invoke_hash, pool);
	switch_thread_rwlock_create(&rtmp_globals.profile_rwlock, pool);
	switch_thread_rwlock_create(&rtmp_globals.session_rwlock, pool);
		
	rtmp_register_invoke_function("connect", rtmp_i_connect);
	rtmp_register_invoke_function("createStream", rtmp_i_createStream);
	rtmp_register_invoke_function("closeStream", rtmp_i_noop);
	rtmp_register_invoke_function("deleteStream", rtmp_i_noop);
	rtmp_register_invoke_function("play", rtmp_i_play);
	rtmp_register_invoke_function("publish", rtmp_i_publish);
	rtmp_register_invoke_function("makeCall", rtmp_i_makeCall);
	rtmp_register_invoke_function("login", rtmp_i_login);
	rtmp_register_invoke_function("logout", rtmp_i_logout);
	rtmp_register_invoke_function("sendDTMF", rtmp_i_sendDTMF);
	rtmp_register_invoke_function("register", rtmp_i_register);
	rtmp_register_invoke_function("unregister", rtmp_i_unregister);
	rtmp_register_invoke_function("answer", rtmp_i_answer);
	rtmp_register_invoke_function("attach", rtmp_i_attach);
	rtmp_register_invoke_function("hangup", rtmp_i_hangup);
	rtmp_register_invoke_function("transfer", rtmp_i_transfer);
	rtmp_register_invoke_function("three_way", rtmp_i_three_way);
	rtmp_register_invoke_function("join", rtmp_i_join);
	rtmp_register_invoke_function("sendevent", rtmp_i_sendevent);
	rtmp_register_invoke_function("receiveAudio", rtmp_i_receiveaudio);
	rtmp_register_invoke_function("receiveVideo", rtmp_i_receivevideo);
	rtmp_register_invoke_function("log", rtmp_i_log);
	
	*module_interface = switch_loadable_module_create_module_interface(pool, modname);
	rtmp_globals.rtmp_endpoint_interface = switch_loadable_module_create_interface(*module_interface, SWITCH_ENDPOINT_INTERFACE);
	rtmp_globals.rtmp_endpoint_interface->interface_name = "rtmp";
	rtmp_globals.rtmp_endpoint_interface->io_routines = &rtmp_io_routines;
	rtmp_globals.rtmp_endpoint_interface->state_handler = &rtmp_state_handlers;
	
	SWITCH_ADD_API(api_interface, "rtmp", "rtmp management", rtmp_function, RTMP_FUNCTION_SYNTAX);
	SWITCH_ADD_API(api_interface, "rtmp_contact", "rtmp contact", rtmp_contact_function, RTMP_CONTACT_FUNCTION_SYNTAX);

	switch_console_set_complete("add rtmp status");
	switch_console_set_complete("add rtmp status profile ::rtmp::list_profiles");
	switch_console_set_complete("add rtmp status profile ::rtmp::list_profiles sessions");
	switch_console_set_complete("add rtmp status profile ::rtmp::list_profiles reg");
	switch_console_set_complete("add rtmp profile ::rtmp::list_profiles start");
	switch_console_set_complete("add rtmp profile ::rtmp::list_profiles stop");
	switch_console_set_complete("add rtmp profile ::rtmp::list_profiles restart");
	switch_console_set_complete("add rtmp profile ::rtmp::list_profiles rescan");
	switch_console_set_complete("add rtmp session ::rtmp::list_sessions kill");
	switch_console_set_complete("add rtmp session ::rtmp::list_sessions login");
	switch_console_set_complete("add rtmp session ::rtmp::list_sessions logout");
	
	switch_console_add_complete_func("::rtmp::list_profiles", list_profiles);
	switch_console_add_complete_func("::rtmp::list_sessions", list_sessions);
	
	switch_event_bind("mod_rtmp", SWITCH_EVENT_CUSTOM, RTMP_EVENT_CUSTOM, rtmp_event_handler, NULL);
	
	{
		switch_xml_t cfg, xml, x_profiles, x_profile;
		const char *file = "rtmp.conf";

		if (!(xml = switch_xml_open_cfg(file, &cfg, NULL))) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Could not open %s\n", file);
			goto done;
		}

		if (!(x_profiles = switch_xml_child(cfg, "profiles"))) {
			goto done;
		}

		for (x_profile = switch_xml_child(x_profiles, "profile"); x_profile; x_profile = x_profile->next) {
			const char *name = switch_xml_attr_soft(x_profile, "name");
			rtmp_profile_start(name);
		}
		done:
		if (xml) {
			switch_xml_free(xml);	
		}
	}

	rtmp_globals.running = 1;
	
	return SWITCH_STATUS_SUCCESS;
}

SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_rtmp_shutdown)
{
	switch_hash_index_t *hi;

	switch_mutex_lock(rtmp_globals.mutex);
	while ((hi = switch_hash_first(NULL, rtmp_globals.profile_hash))) {
		void *val;	
		const void *key;
		switch_ssize_t keylen;
		rtmp_profile_t *item;
		switch_hash_this(hi, &key, &keylen, &val);
							
		item = (rtmp_profile_t *)val;
		
		switch_mutex_unlock(rtmp_globals.mutex);
		rtmp_profile_destroy(&item);
		switch_mutex_lock(rtmp_globals.mutex);	
	}
	switch_mutex_unlock(rtmp_globals.mutex);
	
	switch_event_unbind_callback(rtmp_event_handler);
	
	switch_core_hash_destroy(&rtmp_globals.profile_hash);
	switch_core_hash_destroy(&rtmp_globals.session_hash);
	switch_core_hash_destroy(&rtmp_globals.invoke_hash);
	
	rtmp_globals.running = 0;

	return SWITCH_STATUS_SUCCESS;
}

SWITCH_MODULE_RUNTIME_FUNCTION(mod_rtmp_runtime)
{

	while(rtmp_globals.running) {
		rtmp_garbage_colletor();
		switch_yield(10000000);
	}

	return SWITCH_STATUS_TERM;
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
