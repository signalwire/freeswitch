/* 
 * FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
 * Copyright (C) 2005-2011, Anthony Minessale II <anthm@freeswitch.org>
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
 * Mathieu Rene <mrene@avgs.ca>
 *
 * rtp.c -- RTP Controllable Channel Module
 *
 */

#include <switch.h>
#include "mod_sofia.h"

#define kBINDADDRESS "bind_address"
#define kLOCALADDR "local_addr"
#define kLOCALPORT "local_port"
#define kREMOTEADDR "remote_addr"
#define kREMOTEPORT "remote_port"
#define kCODEC "codec"
#define kPTIME "ptime"
#define kRFC2833PT "rfc2833_pt"
#define kMODE "mode"

static struct {
    switch_memory_pool_t *pool;
    switch_endpoint_interface_t *endpoint_interface;
} crtp;

typedef struct {
    switch_core_session_t *session;
    switch_channel_t *channel;
    switch_codec_t read_codec, write_codec;
    switch_frame_t read_frame;
    
    switch_rtp_bug_flag_t rtp_bugs;
    switch_rtp_t *rtp_session;
    
    const char *bind_address;
    const char *codec;
    int ptime;
    
    const switch_codec_implementation_t *negotiated_codecs[SWITCH_MAX_CODECS];
	int num_negotiated_codecs;
    
    char *origin;
    
    switch_port_t local_port;
    switch_port_t remote_port;

} crtp_private_t;

static switch_status_t channel_on_init(switch_core_session_t *session);
static switch_status_t channel_on_destroy(switch_core_session_t *session);
static switch_call_cause_t channel_outgoing_channel(switch_core_session_t *session, switch_event_t *var_event,
													switch_caller_profile_t *outbound_profile,
													switch_core_session_t **new_session, 
													switch_memory_pool_t **pool,
													switch_originate_flag_t flags, switch_call_cause_t *cancel_cause);
static switch_status_t channel_read_frame(switch_core_session_t *session, switch_frame_t **frame, switch_io_flag_t flags, int stream_id);
static switch_status_t channel_write_frame(switch_core_session_t *session, switch_frame_t *frame, switch_io_flag_t flags, int stream_id);
static switch_status_t channel_receive_message(switch_core_session_t *session, switch_core_session_message_t *msg);
static switch_status_t channel_send_dtmf(switch_core_session_t *session, const switch_dtmf_t *dtmf);

switch_state_handler_table_t crtp_state_handlers = {
	.on_init = channel_on_init,
	.on_destroy = channel_on_destroy
};

switch_io_routines_t crtp_io_routines = {
	.outgoing_channel = channel_outgoing_channel,
	.read_frame = channel_read_frame,
	.write_frame = channel_write_frame,
	.receive_message = channel_receive_message,
    .send_dtmf = channel_send_dtmf
};


void crtp_init(switch_loadable_module_interface_t *module_interface)
{
    switch_endpoint_interface_t *endpoint_interface;
    //switch_api_interface_t *api_interface;
    
    crtp.pool = module_interface->pool;
    endpoint_interface = switch_loadable_module_create_interface(module_interface, SWITCH_ENDPOINT_INTERFACE);
    endpoint_interface->interface_name = "rtp";
    endpoint_interface->io_routines = &crtp_io_routines;
    endpoint_interface->state_handler = &crtp_state_handlers;
    crtp.endpoint_interface = endpoint_interface;
    
    //SWITCH_ADD_API(api_interface, "rtp_test", "test", test_function, "");
}

static switch_call_cause_t channel_outgoing_channel(switch_core_session_t *session, switch_event_t *var_event,
													switch_caller_profile_t *outbound_profile,
													switch_core_session_t **new_session, 
													switch_memory_pool_t **pool,
													switch_originate_flag_t flags, switch_call_cause_t *cancel_cause)
{
    switch_channel_t *channel;
    char name[128];
    const char *dname = "PCMU";
    uint32_t interval = 20;
    crtp_private_t *tech_pvt = NULL;
    //const char *l_sdp = switch_event_get_header(var_event, kLSDP);
    //const char *codec_string = switch_event_get_header_nil(var_event, kCODECSTRING);

    
    if (!(*new_session = switch_core_session_request(crtp.endpoint_interface, SWITCH_CALL_DIRECTION_OUTBOUND, 0, pool))) {
        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Couldn't request session.\n");
        goto fail;
    }
    
    channel = switch_core_session_get_channel(*new_session);
    
    
    
    tech_pvt = switch_core_session_alloc(*new_session, sizeof *tech_pvt);
    tech_pvt->session = *new_session;
    tech_pvt->channel = channel;
    tech_pvt->bind_address = switch_core_session_strdup(*new_session, switch_event_get_header_nil(var_event, kBINDADDRESS));
    switch_core_session_set_private(*new_session, tech_pvt);
    
    
    snprintf(name, sizeof(name), "rtp/ctrl"); /* TODO add addresses */
	switch_channel_set_name(channel, name);
    
    switch_channel_set_state(channel, CS_INIT);

	if (switch_core_codec_init(&tech_pvt->read_codec,
							   dname,
							   NULL,
							   8000,
							   interval,
							   1,
							   SWITCH_CODEC_FLAG_ENCODE | SWITCH_CODEC_FLAG_DECODE,
							   NULL, switch_core_session_get_pool(tech_pvt->session)) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Can't load codec?\n");
        goto fail;
	} else {
		if (switch_core_codec_init(&tech_pvt->write_codec,
								   dname,
								   NULL,
								   8000,
								   interval,
								   1,
								   SWITCH_CODEC_FLAG_ENCODE | SWITCH_CODEC_FLAG_DECODE,
								   NULL, switch_core_session_get_pool(tech_pvt->session)) != SWITCH_STATUS_SUCCESS) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Can't load codec?\n");
			switch_core_codec_destroy(&tech_pvt->read_codec);
            goto fail;
		}
	}
    
    if (switch_core_session_set_read_codec(*new_session, &tech_pvt->read_codec) != SWITCH_STATUS_SUCCESS) {
        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Can't set read codec?\n");
        goto fail;
    }
    
    if (switch_core_session_set_write_codec(*new_session, &tech_pvt->write_codec) != SWITCH_STATUS_SUCCESS) {
        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Can't set write codec?\n");        
    }
    
    if (switch_core_session_thread_launch(*new_session) != SWITCH_STATUS_SUCCESS) {
        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Couldn't start session thread.\n"); 
        goto fail;
    }
    
    switch_channel_mark_answered(channel);
    
    return SWITCH_CAUSE_SUCCESS;
    
fail:
     if (tech_pvt) {
        if (tech_pvt->read_codec.implementation) {
			switch_core_codec_destroy(&tech_pvt->read_codec);
		}
		
		if (tech_pvt->write_codec.implementation) {
			switch_core_codec_destroy(&tech_pvt->write_codec);
		}
    }
    
    if (*new_session) {
        switch_core_session_destroy(new_session);
    }
    return SWITCH_CAUSE_DESTINATION_OUT_OF_ORDER;
}

static switch_status_t channel_on_init(switch_core_session_t *session)
{
    
    switch_channel_t *channel = switch_core_session_get_channel(session);
    
    switch_channel_set_state(channel, CS_ROUTING);
    
    return SWITCH_STATUS_SUCCESS;   
}

static switch_status_t channel_on_destroy(switch_core_session_t *session)
{
    crtp_private_t *tech_pvt = switch_core_session_get_private(session);
    
 	if ((tech_pvt = switch_core_session_get_private(session))) {
        
		if (tech_pvt->read_codec.implementation) {
			switch_core_codec_destroy(&tech_pvt->read_codec);
		}
		
		if (tech_pvt->write_codec.implementation) {
			switch_core_codec_destroy(&tech_pvt->write_codec);
		}
	}
    
    return SWITCH_STATUS_SUCCESS;
}


static switch_status_t channel_read_frame(switch_core_session_t *session, switch_frame_t **frame, switch_io_flag_t flags, int stream_id)
{
    crtp_private_t *tech_pvt;
    switch_channel_t *channel;
    
    channel = switch_core_session_get_channel(session);
	assert(channel != NULL);
	
	tech_pvt = switch_core_session_get_private(session);
	assert(tech_pvt != NULL);
    
    
    return SWITCH_STATUS_SUCCESS;
}

static switch_status_t channel_write_frame(switch_core_session_t *session, switch_frame_t *frame, switch_io_flag_t flags, int stream_id)
{
    crtp_private_t *tech_pvt;
    switch_channel_t *channel;
    
    channel = switch_core_session_get_channel(session);
	assert(channel != NULL);
	
	tech_pvt = switch_core_session_get_private(session);
	assert(tech_pvt != NULL);
    
    
    return SWITCH_STATUS_SUCCESS;

}

static switch_status_t channel_send_dtmf(switch_core_session_t *session, const switch_dtmf_t *dtmf)
{
	crtp_private_t *tech_pvt = NULL;
    
	tech_pvt = switch_core_session_get_private(session);
	assert(tech_pvt != NULL);
    
    
	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t channel_receive_message(switch_core_session_t *session, switch_core_session_message_t *msg)
{
    return SWITCH_STATUS_SUCCESS;
}

