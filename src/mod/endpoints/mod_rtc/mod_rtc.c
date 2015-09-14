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
 *
 *
 * mod_rtc.c -- RTC Endpoint
 *
 */

/* Best viewed in a 160 x 60 VT100 Terminal or so the line below at least fits across your screen*/
/*************************************************************************************************************************************************************/
#include <switch.h>

SWITCH_MODULE_LOAD_FUNCTION(mod_rtc_load);
SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_rtc_shutdown);
SWITCH_MODULE_DEFINITION(mod_rtc, mod_rtc_load, mod_rtc_shutdown, NULL);


switch_endpoint_interface_t *rtc_endpoint_interface;

#define STRLEN 15

static switch_status_t rtc_on_init(switch_core_session_t *session);

static switch_status_t rtc_on_exchange_media(switch_core_session_t *session);
static switch_status_t rtc_on_soft_execute(switch_core_session_t *session);
static switch_call_cause_t rtc_outgoing_channel(switch_core_session_t *session, switch_event_t *var_event,
												  switch_caller_profile_t *outbound_profile, switch_core_session_t **new_session,
												  switch_memory_pool_t **pool, switch_originate_flag_t flags, switch_call_cause_t *cancel_cause);
static switch_status_t rtc_read_frame(switch_core_session_t *session, switch_frame_t **frame, switch_io_flag_t flags, int stream_id);
static switch_status_t rtc_write_frame(switch_core_session_t *session, switch_frame_t *frame, switch_io_flag_t flags, int stream_id);
static switch_status_t rtc_read_video_frame(switch_core_session_t *session, switch_frame_t **frame, switch_io_flag_t flags, int stream_id);
static switch_status_t rtc_write_video_frame(switch_core_session_t *session, switch_frame_t *frame, switch_io_flag_t flags, int stream_id);
static switch_status_t rtc_kill_channel(switch_core_session_t *session, int sig);

typedef struct {
	switch_channel_t *channel;
	switch_core_session_t *session;
	switch_caller_profile_t *caller_profile;
	switch_media_handle_t *media_handle;
	switch_core_media_params_t mparams; 
} private_object_t;

static struct {
	switch_memory_pool_t *pool;
	switch_mutex_t *mutex;
	int running;
} mod_rtc_globals;


/* BODY OF THE MODULE */
/*************************************************************************************************************************************************************/

/* 
   State methods they get called when the state changes to the specific state 
   returning SWITCH_STATUS_SUCCESS tells the core to execute the standard state method next
   so if you fully implement the state you can return SWITCH_STATUS_FALSE to skip it.
*/
static switch_status_t rtc_on_init(switch_core_session_t *session)
{
	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t rtc_on_routing(switch_core_session_t *session)
{
	switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "%s RTC ROUTING\n",
					  switch_channel_get_name(switch_core_session_get_channel(session)));

	return SWITCH_STATUS_SUCCESS;
}


static switch_status_t rtc_on_reset(switch_core_session_t *session)
{

	switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "%s RTC RESET\n",
					  switch_channel_get_name(switch_core_session_get_channel(session)));


	return SWITCH_STATUS_SUCCESS;
}


static switch_status_t rtc_on_hibernate(switch_core_session_t *session)
{

	switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "%s RTC HIBERNATE\n",
					  switch_channel_get_name(switch_core_session_get_channel(session)));


	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t rtc_on_execute(switch_core_session_t *session)
{

	switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "%s RTC EXECUTE\n",
					  switch_channel_get_name(switch_core_session_get_channel(session)));

	return SWITCH_STATUS_SUCCESS;
}


switch_status_t rtc_on_destroy(switch_core_session_t *session)
{
	switch_channel_t *channel = switch_core_session_get_channel(session);


	switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "%s RTC DESTROY\n", switch_channel_get_name(channel));
	switch_media_handle_destroy(session);
	
	return SWITCH_STATUS_SUCCESS;

}

switch_status_t rtc_on_hangup(switch_core_session_t *session)
{
	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t rtc_on_exchange_media(switch_core_session_t *session)
{
	switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "RTC EXCHANGE_MEDIA\n");
	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t rtc_on_soft_execute(switch_core_session_t *session)
{
	switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "RTC SOFT_EXECUTE\n");
	return SWITCH_STATUS_SUCCESS;
}


static switch_status_t rtc_read_video_frame(switch_core_session_t *session, switch_frame_t **frame, switch_io_flag_t flags, int stream_id)
{
	return switch_core_media_read_frame(session, frame, flags, stream_id, SWITCH_MEDIA_TYPE_VIDEO);
}

static switch_status_t rtc_write_video_frame(switch_core_session_t *session, switch_frame_t *frame, switch_io_flag_t flags, int stream_id)
{
	private_object_t *tech_pvt = (private_object_t *) switch_core_session_get_private(session);
	switch_assert(tech_pvt != NULL);


	if (SWITCH_STATUS_SUCCESS == switch_core_media_write_frame(session, frame, flags, stream_id, SWITCH_MEDIA_TYPE_VIDEO)) {
		return SWITCH_STATUS_SUCCESS;
	}

	return SWITCH_STATUS_FALSE;
}

static switch_status_t rtc_read_frame(switch_core_session_t *session, switch_frame_t **frame, switch_io_flag_t flags, int stream_id)
{
	switch_status_t status = SWITCH_STATUS_FALSE;

	status = switch_core_media_read_frame(session, frame, flags, stream_id, SWITCH_MEDIA_TYPE_AUDIO);

	return status;
}

static switch_status_t rtc_write_frame(switch_core_session_t *session, switch_frame_t *frame, switch_io_flag_t flags, int stream_id)
{
	switch_status_t status = SWITCH_STATUS_SUCCESS;

	status = switch_core_media_write_frame(session, frame, flags, stream_id, SWITCH_MEDIA_TYPE_AUDIO);

	return status;
}

static switch_status_t rtc_kill_channel(switch_core_session_t *session, int sig)
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


static switch_status_t rtc_send_dtmf(switch_core_session_t *session, const switch_dtmf_t *dtmf)
{
	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t rtc_receive_message(switch_core_session_t *session, switch_core_session_message_t *msg)
{
	switch_channel_t *channel = switch_core_session_get_channel(session);
	private_object_t *tech_pvt = switch_core_session_get_private(session);
	switch_status_t status = SWITCH_STATUS_SUCCESS;
	const char *var;

	if (switch_channel_down(channel) || !tech_pvt) {
		status = SWITCH_STATUS_FALSE;
		return SWITCH_STATUS_FALSE;
	}

	/* ones that do not need to lock rtp mutex */
	switch (msg->message_id) {

	case SWITCH_MESSAGE_INDICATE_CLEAR_PROGRESS:
		break;
	case SWITCH_MESSAGE_INDICATE_ANSWER:
	case SWITCH_MESSAGE_INDICATE_PROGRESS:
		{
			
			if (((var = switch_channel_get_variable(channel, "rtp_secure_media"))) &&
				(switch_true(var) || switch_core_media_crypto_str2type(var) != CRYPTO_INVALID)) {
				switch_channel_set_flag(tech_pvt->channel, CF_SECURE);
			}
		}
		break;

	default:
		break;
	}


	return status;

}

static switch_status_t rtc_receive_event(switch_core_session_t *session, switch_event_t *event)
{
	return SWITCH_STATUS_SUCCESS;
}

static switch_jb_t *rtc_get_jb(switch_core_session_t *session, switch_media_type_t type)
{
	private_object_t *tech_pvt = (private_object_t *) switch_core_session_get_private(session);

	return switch_core_media_get_jb(tech_pvt->session, type);
}

switch_io_routines_t rtc_io_routines = {
	/*.outgoing_channel */ rtc_outgoing_channel,
	/*.read_frame */ rtc_read_frame,
	/*.write_frame */ rtc_write_frame,
	/*.kill_channel */ rtc_kill_channel,
	/*.send_dtmf */ rtc_send_dtmf,
	/*.receive_message */ rtc_receive_message,
	/*.receive_event */ rtc_receive_event,
	/*.state_change */ NULL,
	/*.read_video_frame */ rtc_read_video_frame,
	/*.write_video_frame */ rtc_write_video_frame,
	/*.state_run*/ NULL,
	/*.get_jb*/ rtc_get_jb
};

switch_state_handler_table_t rtc_event_handlers = {
	/*.on_init */ rtc_on_init,
	/*.on_routing */ rtc_on_routing,
	/*.on_execute */ rtc_on_execute,
	/*.on_hangup */ rtc_on_hangup,
	/*.on_exchange_media */ rtc_on_exchange_media,
	/*.on_soft_execute */ rtc_on_soft_execute,
	/*.on_consume_media */ NULL,
	/*.on_hibernate */ rtc_on_hibernate,
	/*.on_reset */ rtc_on_reset,
	/*.on_park */ NULL,
	/*.on_reporting */ NULL,
	/*.on_destroy */ rtc_on_destroy
};


void rtc_set_name(private_object_t *tech_pvt, const char *channame)
{
	char name[256];

	switch_snprintf(name, sizeof(name), "rtc/%s", channame);
	switch_channel_set_name(tech_pvt->channel, name);
}



void rtc_attach_private(switch_core_session_t *session, private_object_t *tech_pvt, const char *channame)
{

	switch_assert(session != NULL);
	switch_assert(tech_pvt != NULL);
	
	switch_core_session_add_stream(session, NULL);

	tech_pvt->session = session;
	tech_pvt->channel = switch_core_session_get_channel(session);
	switch_core_media_check_dtmf_type(session);
	switch_channel_set_cap(tech_pvt->channel, CC_JITTERBUFFER);
	switch_channel_set_cap(tech_pvt->channel, CC_FS_RTP);
	switch_media_handle_create(&tech_pvt->media_handle, session, &tech_pvt->mparams);
	switch_core_session_set_private(session, tech_pvt);

	if (channame) {
		rtc_set_name(tech_pvt, channame);
	}
}


private_object_t *rtc_new_pvt(switch_core_session_t *session)
{
	private_object_t *tech_pvt = (private_object_t *) switch_core_session_alloc(session, sizeof(private_object_t));
	return tech_pvt;
}

static switch_call_cause_t rtc_outgoing_channel(switch_core_session_t *session, switch_event_t *var_event,
												  switch_caller_profile_t *outbound_profile, switch_core_session_t **new_session,
												  switch_memory_pool_t **pool, switch_originate_flag_t flags, switch_call_cause_t *cancel_cause)
{
	switch_call_cause_t cause = SWITCH_CAUSE_DESTINATION_OUT_OF_ORDER;
	switch_core_session_t *nsession = NULL;
	switch_caller_profile_t *caller_profile = NULL;
	private_object_t *tech_pvt = NULL;
	switch_channel_t *nchannel;
	const char *hval = NULL;

	*new_session = NULL;


	if (!(nsession = switch_core_session_request_uuid(rtc_endpoint_interface, SWITCH_CALL_DIRECTION_OUTBOUND, 
													  flags, pool, switch_event_get_header(var_event, "origination_uuid")))) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Error Creating Session\n");
		goto error;
	}

	tech_pvt = rtc_new_pvt(nsession);

	nchannel = switch_core_session_get_channel(nsession);

	if (outbound_profile) {
		caller_profile = switch_caller_profile_clone(nsession, outbound_profile);
		switch_channel_set_caller_profile(nchannel, caller_profile);
	}

	if ((hval = switch_event_get_header(var_event, "media_webrtc")) && switch_true(hval)) {
		switch_channel_set_variable(nchannel, "rtc_secure_media", SWITCH_RTP_CRYPTO_KEY_80);
	}

	if ((hval = switch_event_get_header(var_event, "rtc_secure_media"))) {
		switch_channel_set_variable(nchannel, "rtc_secure_media", hval);
	}
	
	rtc_attach_private(nsession, tech_pvt, NULL);


	if (switch_channel_get_state(nchannel) == CS_NEW) {
		switch_channel_set_state(nchannel, CS_INIT);
	}

	tech_pvt->caller_profile = caller_profile;
	*new_session = nsession;
	cause = SWITCH_CAUSE_SUCCESS;


	if (session) {
		switch_ivr_transfer_variable(session, nsession, "rtc_video_fmtp");
	}

	goto done;

  error:

	if (nsession) {
		switch_core_session_destroy(&nsession);
	}

	if (pool) {
		*pool = NULL;
	}

  done:

	return cause;
}

static int rtc_recover_callback(switch_core_session_t *session) 
{
	private_object_t *tech_pvt = rtc_new_pvt(session);
	rtc_attach_private(session, tech_pvt, NULL);
	
	return 1;
}

SWITCH_MODULE_LOAD_FUNCTION(mod_rtc_load)
{
	memset(&mod_rtc_globals, 0, sizeof(mod_rtc_globals));
	mod_rtc_globals.pool = pool;
	switch_mutex_init(&mod_rtc_globals.mutex, SWITCH_MUTEX_NESTED, mod_rtc_globals.pool);

	switch_mutex_lock(mod_rtc_globals.mutex);
	mod_rtc_globals.running = 1;
	switch_mutex_unlock(mod_rtc_globals.mutex);

	/* connect my internal structure to the blank pointer passed to me */
	*module_interface = switch_loadable_module_create_module_interface(pool, modname);

	rtc_endpoint_interface = switch_loadable_module_create_interface(*module_interface, SWITCH_ENDPOINT_INTERFACE);
	rtc_endpoint_interface->interface_name = "rtc";
	rtc_endpoint_interface->io_routines = &rtc_io_routines;
	rtc_endpoint_interface->state_handler = &rtc_event_handlers;
	rtc_endpoint_interface->recover_callback = rtc_recover_callback;

	/* indicate that the module should continue to be loaded */
	return SWITCH_STATUS_SUCCESS;
}

SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_rtc_shutdown)
{
	switch_mutex_lock(mod_rtc_globals.mutex);
	if (mod_rtc_globals.running == 1) {
		mod_rtc_globals.running = 0;
	}
	switch_mutex_unlock(mod_rtc_globals.mutex);

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

