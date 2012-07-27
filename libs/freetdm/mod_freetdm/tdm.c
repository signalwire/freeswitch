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
* tdm.c -- FreeTDM Controllable Channel Module
*
*/

#include <switch.h>
#include "freetdm.h"

void ctdm_init(switch_loadable_module_interface_t *module_interface);

/* Parameters */

#define kSPAN_ID "span"
#define kCHAN_ID "chan"
#define kSPAN_NAME "span_name"

static struct {
    switch_memory_pool_t *pool;
    switch_endpoint_interface_t *endpoint_interface;
} ctdm;

typedef struct {
    int span_id;
    int chan_id;
    ftdm_channel_t *ftdm_channel;
    switch_core_session_t *session;
    switch_codec_t read_codec, write_codec;
    switch_frame_t read_frame;
    
	unsigned char databuf[SWITCH_RECOMMENDED_BUFFER_SIZE];
} ctdm_private_t;

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

switch_state_handler_table_t ctdm_state_handlers = {
	.on_init = channel_on_init,
	.on_destroy = channel_on_destroy
};

switch_io_routines_t ctdm_io_routines = {
    .send_dtmf = channel_send_dtmf,
	.outgoing_channel = channel_outgoing_channel,
	.read_frame = channel_read_frame,
	.write_frame = channel_write_frame,
	.receive_message = channel_receive_message
};

void ctdm_init(switch_loadable_module_interface_t *module_interface)
{
    switch_endpoint_interface_t *endpoint_interface;
    ctdm.pool = module_interface->pool;
    endpoint_interface = switch_loadable_module_create_interface(module_interface, SWITCH_ENDPOINT_INTERFACE);
    endpoint_interface->interface_name = "tdm";
    endpoint_interface->io_routines = &ctdm_io_routines;
    endpoint_interface->state_handler = &ctdm_state_handlers;
    ctdm.endpoint_interface = endpoint_interface;
    
}

static switch_call_cause_t channel_outgoing_channel(switch_core_session_t *session, switch_event_t *var_event,
													switch_caller_profile_t *outbound_profile,
													switch_core_session_t **new_session, 
													switch_memory_pool_t **pool,
													switch_originate_flag_t flags, switch_call_cause_t *cancel_cause)
{
    const char  *szchanid = switch_event_get_header(var_event, kCHAN_ID),
                *span_name = switch_event_get_header(var_event, kSPAN_NAME);
    int chan_id;
    int span_id;
    switch_caller_profile_t *caller_profile;
    ftdm_span_t *span;
    ftdm_channel_t *chan;
    switch_channel_t *channel;
    char name[128];
    const char *dname;
    ftdm_codec_t codec;
    uint32_t interval;
    ftdm_status_t fstatus;
    
    ctdm_private_t *tech_pvt = NULL;
    
    if (zstr(szchanid) || zstr(span_name)) {
        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Both ["kSPAN_ID"] and ["kCHAN_ID"] have to be set.\n");
        goto fail;
    }
    
    chan_id = atoi(szchanid);
    
    if (ftdm_span_find_by_name(span_name, &span) == FTDM_SUCCESS) {
         span_id = ftdm_span_get_id(span);   
    } else {
        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Cannot find span [%s]\n", span_name);
        goto fail;
    }

    
    if (!(*new_session = switch_core_session_request(ctdm.endpoint_interface, SWITCH_CALL_DIRECTION_OUTBOUND, 0, pool))) {
        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Couldn't request session.\n");
        goto fail;
    }
    
    channel = switch_core_session_get_channel(*new_session);
    
    if ((fstatus = ftdm_span_start(span)) != FTDM_SUCCESS && fstatus != FTDM_EINVAL) {
        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Couldn't start span %s.\n", span_name);
        goto fail;
    }
    
    if (ftdm_channel_open(span_id, chan_id, &chan) != FTDM_SUCCESS) {
        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Couldn't open span or channel.\n"); 
        goto fail;
    }
    
    span = ftdm_channel_get_span(chan);
    
    tech_pvt = switch_core_session_alloc(*new_session, sizeof *tech_pvt);
    tech_pvt->chan_id = chan_id;
    tech_pvt->span_id = span_id;
    tech_pvt->ftdm_channel = chan;
    tech_pvt->session = *new_session;
    tech_pvt->read_frame.buflen = sizeof(tech_pvt->databuf);
    tech_pvt->read_frame.data = tech_pvt->databuf;
    switch_core_session_set_private(*new_session, tech_pvt);
    
    
    caller_profile = switch_caller_profile_clone(*new_session, outbound_profile);
    switch_channel_set_caller_profile(channel, caller_profile);
    
    snprintf(name, sizeof(name), "tdm/%d:%d", span_id, chan_id);
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Connect outbound channel %s\n", name);
	switch_channel_set_name(channel, name);
    
    switch_channel_set_state(channel, CS_INIT);
    
	if (FTDM_SUCCESS != ftdm_channel_command(chan, FTDM_COMMAND_GET_CODEC, &codec)) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Failed to retrieve channel codec.\n");
		return SWITCH_STATUS_GENERR;
	}
    
    if (FTDM_SUCCESS != ftdm_channel_command(chan, FTDM_COMMAND_GET_INTERVAL, &interval)) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Failed to retrieve channel interval.\n");
		return SWITCH_STATUS_GENERR;
	}
    
	switch(codec) {
        case FTDM_CODEC_ULAW:
		{
			dname = "PCMU";
		}
            break;
        case FTDM_CODEC_ALAW:
		{
			dname = "PCMA";
		}
            break;
        case FTDM_CODEC_SLIN:
		{
			dname = "L16";
		}
            break;
        default:
		{
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Invalid codec value retrieved from channel, codec value: %d\n", codec);
			goto fail;
		}
	}

    
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
        if (tech_pvt->ftdm_channel) {
            ftdm_channel_close(&tech_pvt->ftdm_channel);
        }
        
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
    
    switch_channel_set_state(channel, CS_CONSUME_MEDIA);   
    return SWITCH_STATUS_SUCCESS;   
}

static switch_status_t channel_on_destroy(switch_core_session_t *session)
{
    ctdm_private_t *tech_pvt = switch_core_session_get_private(session);
    
 	if ((tech_pvt = switch_core_session_get_private(session))) {
        
		if (tech_pvt->read_codec.implementation) {
			switch_core_codec_destroy(&tech_pvt->read_codec);
		}
		
		if (tech_pvt->write_codec.implementation) {
			switch_core_codec_destroy(&tech_pvt->write_codec);
		}
        
        ftdm_channel_close(&tech_pvt->ftdm_channel);
	}

    return SWITCH_STATUS_SUCCESS;
}


static switch_status_t channel_read_frame(switch_core_session_t *session, switch_frame_t **frame, switch_io_flag_t flags, int stream_id)
{
    ftdm_wait_flag_t wflags = FTDM_READ;
    ftdm_status_t status;
    ctdm_private_t *tech_pvt;
    const char *name;
    switch_channel_t *channel;
    int chunk;
    uint32_t span_id, chan_id;
    ftdm_size_t len;
    char dtmf[128] = "";
    
    channel = switch_core_session_get_channel(session);
	assert(channel != NULL);
	
	tech_pvt = switch_core_session_get_private(session);
	assert(tech_pvt != NULL);
    
	name = switch_channel_get_name(channel);

top:
    wflags = FTDM_READ;
    chunk = ftdm_channel_get_io_interval(tech_pvt->ftdm_channel) * 2;
    status = ftdm_channel_wait(tech_pvt->ftdm_channel, &wflags, chunk);
    
    
	span_id = ftdm_channel_get_span_id(tech_pvt->ftdm_channel);
	chan_id = ftdm_channel_get_id(tech_pvt->ftdm_channel);

    if (status == FTDM_FAIL) {
        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Failed to read from channel %s device %d:%d!\n", name, span_id, chan_id);
        goto fail;
    }

    if (status == FTDM_TIMEOUT) {
        goto top;
    }

    if (!(wflags & FTDM_READ)) {
        goto top;
    }

    len = tech_pvt->read_frame.buflen;
    if (ftdm_channel_read(tech_pvt->ftdm_channel, tech_pvt->read_frame.data, &len) != FTDM_SUCCESS) {
        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "Failed to read from channel %s device %d:%d!\n", name, span_id, chan_id);
    }

    *frame = &tech_pvt->read_frame;
    tech_pvt->read_frame.datalen = (uint32_t)len;
    tech_pvt->read_frame.samples = tech_pvt->read_frame.datalen;
    tech_pvt->read_frame.codec = &tech_pvt->read_codec;

    if (ftdm_channel_get_codec(tech_pvt->ftdm_channel) == FTDM_CODEC_SLIN) {
        tech_pvt->read_frame.samples /= 2;
    }

    while (ftdm_channel_dequeue_dtmf(tech_pvt->ftdm_channel, dtmf, sizeof(dtmf))) {
        switch_dtmf_t _dtmf = { 0, switch_core_default_dtmf_duration(0) };
        char *p;
        for (p = dtmf; p && *p; p++) {
            if (is_dtmf(*p)) {
                _dtmf.digit = *p;
                ftdm_log(FTDM_LOG_DEBUG, "Queuing DTMF [%c] in channel %s device %d:%d\n", *p, name, span_id, chan_id);
                switch_channel_queue_dtmf(channel, &_dtmf);
            }
        }
    }

    return SWITCH_STATUS_SUCCESS;

fail:
    return SWITCH_STATUS_GENERR;
}

static switch_status_t channel_write_frame(switch_core_session_t *session, switch_frame_t *frame, switch_io_flag_t flags, int stream_id)
{
    ftdm_wait_flag_t wflags = FTDM_WRITE;
    ctdm_private_t *tech_pvt;
    const char *name;
    switch_channel_t *channel;
    uint32_t span_id, chan_id;
    ftdm_size_t len;
    unsigned char data[SWITCH_RECOMMENDED_BUFFER_SIZE] = {0};
    
    channel = switch_core_session_get_channel(session);
	assert(channel != NULL);
	
	tech_pvt = switch_core_session_get_private(session);
	assert(tech_pvt != NULL);
    
	span_id = ftdm_channel_get_span_id(tech_pvt->ftdm_channel);
	chan_id = ftdm_channel_get_id(tech_pvt->ftdm_channel);
    
	name = switch_channel_get_name(channel);   
    
    if (switch_test_flag(frame, SFF_CNG)) {
		frame->data = data;
		frame->buflen = sizeof(data);
		if ((frame->datalen = tech_pvt->write_codec.implementation->encoded_bytes_per_packet) > frame->buflen) {
			goto fail;
		}
		memset(data, 255, frame->datalen);
	}
    
    wflags = FTDM_WRITE;	
	ftdm_channel_wait(tech_pvt->ftdm_channel, &wflags, ftdm_channel_get_io_interval(tech_pvt->ftdm_channel) * 10);
	
	if (!(wflags & FTDM_WRITE)) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Dropping frame! (write not ready) in channel %s device %d:%d!\n", name, span_id, chan_id);
		return SWITCH_STATUS_SUCCESS;
	}
    
	len = frame->datalen;
	if (ftdm_channel_write(tech_pvt->ftdm_channel, frame->data, frame->buflen, &len) != FTDM_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Failed to write to channel %s device %d:%d!\n", name, span_id, chan_id);
	}
    
    return SWITCH_STATUS_SUCCESS;

fail:
    return SWITCH_STATUS_GENERR;
}

static switch_status_t channel_send_dtmf(switch_core_session_t *session, const switch_dtmf_t *dtmf)
{
	ctdm_private_t *tech_pvt = NULL;
	char tmp[2] = "";
    
	tech_pvt = switch_core_session_get_private(session);
	assert(tech_pvt != NULL);
        
	tmp[0] = dtmf->digit;
	ftdm_channel_command(tech_pvt->ftdm_channel, FTDM_COMMAND_SEND_DTMF, tmp);
    
	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t channel_receive_message(switch_core_session_t *session, switch_core_session_message_t *msg)
{
    return SWITCH_STATUS_SUCCESS;
}

