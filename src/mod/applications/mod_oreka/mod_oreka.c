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
 * The Original Code is FreeSWITCH Modular Media Switching Software Library / Oreka Recording Module
 *
 * The Initial Developer of the Original Code is
 * Moises Silva <moises.silva@gmail.com>
 * Portions created by the Initial Developer are Copyright (C)
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 * Moises Silva <moises.silva@gmail.com>
 *
 * mod_oreka -- Module for Media Recording with Oreka
 *
 */

#include <switch.h>
#include <g711.h>

static const char SIP_OREKA_HEADER_PREFIX[] = "oreka_sip_h_";
#define OREKA_PRIVATE "_oreka_"
#define OREKA_BUG_NAME_READ "oreka_read"
#define OREKA_BUG_NAME_WRITE "oreka_write"
#define SIP_OREKA_HEADER_PREFIX_LEN (sizeof(SIP_OREKA_HEADER_PREFIX)-1)

SWITCH_MODULE_LOAD_FUNCTION(mod_oreka_load);
SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_oreka_shutdown);
SWITCH_MODULE_DEFINITION(mod_oreka, mod_oreka_load, mod_oreka_shutdown, NULL);

typedef struct oreka_session_s {
	switch_core_session_t *session;
	switch_port_t read_rtp_port;
	switch_port_t write_rtp_port;
	switch_rtp_t *read_rtp_stream;
	switch_rtp_t *write_rtp_stream;
	switch_codec_implementation_t read_impl;
	switch_codec_implementation_t write_impl;
	uint32_t read_cnt;
	uint32_t write_cnt;
	switch_media_bug_t *read_bug;
	switch_event_t *invite_extra_headers;
	switch_event_t *bye_extra_headers;
	int usecnt;
    switch_audio_resampler_t *read_resampler;
    switch_audio_resampler_t *write_resampler;
    int mux_streams;
} oreka_session_t;

struct {
	char local_ipv4_str[256];
	char sip_server_addr_str[256];
	char sip_server_ipv4_str[256];
	int sip_server_port;
	switch_sockaddr_t *sip_server_addr;
	switch_socket_t *sip_socket;
	pid_t our_pid;
    int mux_streams;
} globals;

typedef enum {
	FS_OREKA_START,
	FS_OREKA_STOP
} oreka_recording_status_t;

typedef enum {
	FS_OREKA_READ,
	FS_OREKA_WRITE
} oreka_stream_type_t;

static int oreka_write_udp(oreka_session_t *oreka, switch_stream_handle_t *udp)
{
	switch_size_t udplen = udp->data_len;
	switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(oreka->session), SWITCH_LOG_DEBUG, "Oreka SIP Packet:\n%s", (const char *)udp->data);
	switch_socket_sendto(globals.sip_socket, globals.sip_server_addr, 0, (void *)udp->data, &udplen);
	if (udplen != udp->data_len) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(oreka->session), SWITCH_LOG_ERROR, "Failed to write SIP Packet of len %zd (wrote=%zd)", 
				udp->data_len, udplen);
	}
	return 0;
}

static int oreka_tear_down_rtp(oreka_session_t *oreka, oreka_stream_type_t type)
{
	if (type == FS_OREKA_READ && oreka->read_rtp_stream) {
		switch_rtp_release_port(globals.local_ipv4_str, oreka->read_rtp_port);
		switch_rtp_destroy(&oreka->read_rtp_stream);
		oreka->read_rtp_port = 0;
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(oreka->session), SWITCH_LOG_DEBUG, "Destroyed read rtp\n");
	} else if (oreka->write_rtp_stream) {
		switch_rtp_release_port(globals.local_ipv4_str, oreka->write_rtp_port);
		switch_rtp_destroy(&oreka->write_rtp_stream);
		oreka->write_rtp_port = 0;
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(oreka->session), SWITCH_LOG_DEBUG, "Destroyed write rtp\n");
	}
	return 0;
}

static int oreka_setup_rtp(oreka_session_t *oreka, oreka_stream_type_t type)
{
	switch_port_t rtp_port = 0;
	switch_rtp_flag_t flags[SWITCH_RTP_FLAG_INVALID] = {0};
	switch_rtp_t *rtp_stream = NULL;
	switch_codec_implementation_t *codec_impl = NULL;
	switch_status_t status = SWITCH_STATUS_SUCCESS;
	int res = 0;
	const char  *err = "unknown error";
	const char *type_str = type == FS_OREKA_READ ? "read" : "write";


	if (type == FS_OREKA_READ) {
		status = switch_core_session_get_read_impl(oreka->session, &oreka->read_impl);
		codec_impl = &oreka->read_impl;
	} else {
		status = switch_core_session_get_write_impl(oreka->session, &oreka->write_impl);
		codec_impl = &oreka->write_impl;
	}

	if (status != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "No %s codec implementation available!\n", type_str);
		res = -1;
		goto done;
	}

	if (!(rtp_port = switch_rtp_request_port(globals.local_ipv4_str))) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Failed to allocate %s RTP port for IP %s\n", type_str, globals.local_ipv4_str);
		res = -1;
		goto done;
	}
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Allocated %s port %d for local IP %s, destination IP %s\n", type_str,
			rtp_port, globals.local_ipv4_str, globals.sip_server_ipv4_str);
	rtp_stream = switch_rtp_new(globals.local_ipv4_str, rtp_port, 
			globals.sip_server_ipv4_str, rtp_port,
			0, /* PCMU IANA*/
			codec_impl->samples_per_packet,
			codec_impl->microseconds_per_packet,
			flags, NULL, &err, switch_core_session_get_pool(oreka->session));
	if (!rtp_stream) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Failed to create %s RTP stream at %s:%d: %s\n", 
				type_str, globals.local_ipv4_str, rtp_port, err);
		res = -1;
		goto done;
	}

    switch_rtp_intentional_bugs(rtp_stream, RTP_BUG_SEND_LINEAR_TIMESTAMPS); 


done:
	if (res == -1) {
		if (rtp_port) {
			switch_rtp_release_port(globals.local_ipv4_str, rtp_port);
		}
		if (rtp_stream) {
			switch_rtp_destroy(&rtp_stream);
		}
	} else {
		if (type == FS_OREKA_READ) {
			oreka->read_rtp_stream = rtp_stream;
			oreka->read_rtp_port = rtp_port;
		} else {
			oreka->write_rtp_stream = rtp_stream;
			oreka->write_rtp_port = rtp_port;
		}
	}
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Successfully created %s RTP stream at %s:%d at %dms@%dHz\n", 
				type_str, globals.local_ipv4_str, rtp_port, codec_impl->microseconds_per_packet/1000, codec_impl->samples_per_second);
	return res;
}

static void save_extra_headers(switch_event_t *extra_headers, switch_channel_t *channel)
{
	switch_event_header_t *ei = NULL;
	for (ei = switch_channel_variable_first(channel);
	     ei;
	     ei = ei->next) {
		const char *name = ei->name;
		char *value = ei->value;
		if (!strncasecmp(name, SIP_OREKA_HEADER_PREFIX, SIP_OREKA_HEADER_PREFIX_LEN)) {
			switch_event_add_header_string(extra_headers, SWITCH_STACK_BOTTOM, name, value);
		}
	}
	switch_channel_variable_last(channel);

	/* Remove the custom header variables that were saved */
	for (ei = extra_headers->headers;
	     ei;
	     ei = ei->next) {
		char *varname = ei->name;
		switch_channel_set_variable(channel, varname, NULL);
	}
}

static switch_event_t *get_extra_headers(oreka_session_t *oreka, oreka_recording_status_t status)
{
	switch_event_t *extra_headers = NULL;
	switch_channel_t *channel = NULL;
	switch_core_session_t *session = oreka->session;

	channel = switch_core_session_get_channel(session);
	if (status == FS_OREKA_START) {
		if (!oreka->invite_extra_headers) {
			switch_event_create_subclass(&oreka->invite_extra_headers, SWITCH_EVENT_CLONE, NULL);
			switch_assert(oreka->invite_extra_headers);
			save_extra_headers(oreka->invite_extra_headers, channel);
		}
		extra_headers = oreka->invite_extra_headers;
	} else if (status == FS_OREKA_STOP) {
		if (!oreka->bye_extra_headers) {
			switch_event_create_subclass(&oreka->bye_extra_headers, SWITCH_EVENT_CLONE, NULL);
			switch_assert(oreka->bye_extra_headers);
			save_extra_headers(oreka->bye_extra_headers, channel);
		}
		extra_headers = oreka->bye_extra_headers;
	}
	return extra_headers;
}

static void oreka_destroy(oreka_session_t *oreka)
{
	oreka->usecnt--;
	if (!oreka->usecnt) {
		if (oreka->invite_extra_headers) {
			switch_event_destroy(&oreka->invite_extra_headers);
		}
		if (oreka->bye_extra_headers) {
			switch_event_destroy(&oreka->bye_extra_headers);
		}
		/* Actual memory for the oreka session was taken from the switch core session pool, the core will take care of it */
	}
}

static int oreka_send_sip_message(oreka_session_t *oreka, oreka_recording_status_t status, oreka_stream_type_t type)
{
	switch_stream_handle_t sip_header = { 0 };
	switch_stream_handle_t sdp = { 0 };
	switch_stream_handle_t udp_packet = { 0 };
	switch_caller_profile_t *caller_profile = NULL;
	switch_channel_t *channel = NULL;
	switch_event_t *extra_headers = NULL;
	switch_event_header_t *ei = NULL;
	switch_core_session_t *session = oreka->session;
	const char *method = status == FS_OREKA_START ? "INVITE" : "BYE";
	const char *session_uuid = switch_core_session_get_uuid(oreka->session);
	const char *caller_id_number = NULL;
	const char *caller_id_name = NULL;
	const char *callee_id_number = NULL;
	const char *callee_id_name = NULL;
	int rc = 0;

	channel = switch_core_session_get_channel(session);

	SWITCH_STANDARD_STREAM(sip_header);
	SWITCH_STANDARD_STREAM(sdp);
	SWITCH_STANDARD_STREAM(udp_packet);

	extra_headers = get_extra_headers(oreka, status);

	caller_profile = switch_channel_get_caller_profile(channel);

	/* Get caller meta data */
	caller_id_number = switch_caller_get_field_by_name(caller_profile, "caller_id_number");
	
	caller_id_name = switch_caller_get_field_by_name(caller_profile, "caller_id_name");
	if (zstr(caller_id_name)) {
		caller_id_name = caller_id_number;
	}

	callee_id_number = switch_caller_get_field_by_name(caller_profile, "callee_id_number");
	if (zstr(callee_id_number)) {
		callee_id_number = switch_caller_get_field_by_name(caller_profile, "destination_number");
	}

	callee_id_name = switch_caller_get_field_by_name(caller_profile, "callee_id_name");
	if (zstr(callee_id_name)) {
		callee_id_name = callee_id_number;
	}

	/* Setup the RTP */
	if (status == FS_OREKA_START) {
		if (oreka_setup_rtp(oreka, type)) {
			rc = -1;
			goto done;
		}
	}

	if (status == FS_OREKA_STOP) {
		oreka_tear_down_rtp(oreka, type);
	}

	/* Fill in the SDP first if this is the beginning */
	if (status == FS_OREKA_START) {
		sdp.write_function(&sdp, "v=0\r\n");
		sdp.write_function(&sdp, "o=freeswitch %s 1 IN IP4 %s\r\n", session_uuid, globals.local_ipv4_str);
		sdp.write_function(&sdp, "c=IN IP4 %s\r\n", globals.sip_server_ipv4_str);
		sdp.write_function(&sdp, "s=Phone Recording (%s)\r\n", type == FS_OREKA_READ ? "RX" : "TX");
		sdp.write_function(&sdp, "i=FreeSWITCH Oreka Recorder (pid=%d)\r\n", globals.our_pid);
		sdp.write_function(&sdp, "m=audio %d RTP/AVP 0\r\n", type == FS_OREKA_READ ? oreka->read_rtp_port : oreka->write_rtp_port);
		sdp.write_function(&sdp, "a=rtpmap:0 PCMU/%d\r\n", type == FS_OREKA_READ 
				? oreka->read_impl.samples_per_second : oreka->write_impl.samples_per_second);
	}

	/* Request line */
	sip_header.write_function(&sip_header, "%s sip:%s@%s:5060 SIP/2.0\r\n", method, callee_id_name, globals.local_ipv4_str);

	/* Via */
	sip_header.write_function(&sip_header, "Via: SIP/2.0/UDP %s:5061;branch=z9hG4bK-%s\r\n", globals.local_ipv4_str, session_uuid);

	/* From */
	sip_header.write_function(&sip_header, "From: <sip:%s@%s:5061;tag=1>\r\n", caller_id_number, globals.local_ipv4_str);

	/* To */
	sip_header.write_function(&sip_header, "To: <sip:%s@%s:5060>\r\n", callee_id_number, globals.local_ipv4_str);

	/* Call-ID */
	sip_header.write_function(&sip_header, "Call-ID: %s\r\n", session_uuid);

	/* CSeq */
	sip_header.write_function(&sip_header, "CSeq: 1 %s\r\n", method);

	/* Contact */
	sip_header.write_function(&sip_header, "Contact: sip:freeswitch@%s:5061\r\n", globals.local_ipv4_str);

	/* Max-Forwards */
	sip_header.write_function(&sip_header, "Max-Forwards: 70\r\n", method);

	/* Subject */
	sip_header.write_function(&sip_header, "Subject: %s %s recording of %s\r\n", 
					status == FS_OREKA_START ? "BEGIN": "END",
					type == FS_OREKA_READ ? "RX" : "TX", caller_id_name);

	/* Add any custom extra headers */
	for (ei = extra_headers->headers;
	     ei;
	     ei = ei->next) {
		const char *name = ei->name;
		char *value = ei->value;
		if (!strncasecmp(name, SIP_OREKA_HEADER_PREFIX, SIP_OREKA_HEADER_PREFIX_LEN)) {
			const char *hname = name +  SIP_OREKA_HEADER_PREFIX_LEN;
			sip_header.write_function(&sip_header, "%s: %s\r\n", hname, value);
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "Adding custom oreka SIP header %s: %s\n", hname, value);
		}
	}

	if (status == FS_OREKA_START) {
		/* Content-Type */
		sip_header.write_function(&sip_header, "Content-Type: application/sdp\r\n");

	}

	/* Content-Length */
	sip_header.write_function(&sip_header, "Content-Length: %d\r\n", sdp.data_len);

	udp_packet.write_function(&udp_packet, "%s\r\n%s\n", sip_header.data, sdp.data);

	oreka_write_udp(oreka, &udp_packet);

done:
	if (sip_header.data) {
		free(sip_header.data);
	}

	if (sdp.data) {
		free(sdp.data);
	}

	if (udp_packet.data) {
		free(udp_packet.data);
	}

	if (status == FS_OREKA_STOP) {
		oreka_destroy(oreka);
	}

	return rc;
}

static switch_bool_t oreka_audio_callback(switch_media_bug_t *bug, void *user_data, switch_abc_type_t type)
{
	oreka_session_t *oreka = user_data;
	switch_core_session_t *session = oreka->session;
	switch_frame_t pcmu_frame = { 0 };
    switch_frame_t *linear_frame, raw_frame = { 0 };
	uint8_t pcmu_data[SWITCH_RECOMMENDED_BUFFER_SIZE];
	uint8_t raw_data[SWITCH_RECOMMENDED_BUFFER_SIZE] = { 0 };
	uint8_t resample_data[SWITCH_RECOMMENDED_BUFFER_SIZE];
    uint32_t linear_len = 0;
	uint32_t i = 0;
	int16_t *linear_samples = NULL;

    

	if (type == SWITCH_ABC_TYPE_READ_REPLACE || type == SWITCH_ABC_TYPE_WRITE_REPLACE || type == SWITCH_ABC_TYPE_READ_PING) {
        int16_t *data;

        if (type == SWITCH_ABC_TYPE_READ_REPLACE || type == SWITCH_ABC_TYPE_READ_PING) {

            if (type == SWITCH_ABC_TYPE_READ_REPLACE) {
                linear_frame = switch_core_media_bug_get_read_replace_frame(bug);
            } else {
                switch_status_t status;

                raw_frame.data = raw_data;
                raw_frame.buflen = SWITCH_RECOMMENDED_BUFFER_SIZE;
                linear_frame = &raw_frame;

                status = switch_core_media_bug_read(bug, &raw_frame, SWITCH_FALSE);
                if (status != SWITCH_STATUS_SUCCESS && status != SWITCH_STATUS_BREAK) {
                    return SWITCH_TRUE;
                }
            }

            if (oreka->read_resampler) {
                data = (int16_t *) linear_frame->data;
                switch_resample_process(oreka->read_resampler, data, (int) linear_frame->datalen / 2);
                linear_len = oreka->read_resampler->to_len * 2;
                memcpy(resample_data, oreka->read_resampler->to, linear_len);
                linear_samples = (int16_t *)resample_data;
            } else {
                linear_samples = linear_frame->data;
                linear_len = linear_frame->datalen;
            }
        }

        if (type == SWITCH_ABC_TYPE_WRITE_REPLACE) {
            linear_frame = switch_core_media_bug_get_write_replace_frame(bug);

            if (oreka->write_resampler) {
                data = (int16_t *) linear_frame->data;
                switch_resample_process(oreka->write_resampler, data, (int) linear_frame->datalen / 2);
                linear_len = oreka->write_resampler->to_len * 2;
                memcpy(resample_data, oreka->write_resampler->to, linear_len);
                linear_samples = (int16_t *)resample_data;
            } else {
                linear_samples = linear_frame->data;
                linear_len = linear_frame->datalen;
            }
        }

		/* convert the L16 frame into PCMU */
		memset(&pcmu_frame, 0, sizeof(pcmu_frame));
		for (i = 0; i < linear_len / sizeof(int16_t); i++) {
			pcmu_data[i] = linear_to_ulaw(linear_samples[i]);
		}
		pcmu_frame.source = __SWITCH_FUNC__;
		pcmu_frame.data = pcmu_data;
		pcmu_frame.datalen = i;
		pcmu_frame.payload = 0;
	}

	switch (type) {
	case SWITCH_ABC_TYPE_INIT:
		{
            switch_codec_implementation_t read_impl;

            switch_core_session_get_read_impl(session, &read_impl);

            if (read_impl.actual_samples_per_second != 8000) {
                switch_resample_create(&oreka->read_resampler,
                                       read_impl.actual_samples_per_second,
                                       8000,
                                       320, SWITCH_RESAMPLE_QUALITY, 1);

                switch_resample_create(&oreka->write_resampler,
                                       read_impl.actual_samples_per_second,
                                       8000,
                                       320, SWITCH_RESAMPLE_QUALITY, 1);                
            }


			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_INFO, "Starting Oreka recording for audio stream\n");
			oreka_send_sip_message(oreka, FS_OREKA_START, FS_OREKA_READ);
            if (!oreka->mux_streams) {
                oreka_send_sip_message(oreka, FS_OREKA_START, FS_OREKA_WRITE);
            }
		}
		break;
	case SWITCH_ABC_TYPE_CLOSE:
		{
            if (oreka->mux_streams) {
                int16_t *data;

                raw_frame.data = raw_data;
                raw_frame.buflen = SWITCH_RECOMMENDED_BUFFER_SIZE;
                linear_frame = &raw_frame;

                while (switch_core_media_bug_read(bug, &raw_frame, SWITCH_TRUE) == SWITCH_STATUS_SUCCESS) {
                    linear_frame = &raw_frame;


                    if (oreka->read_resampler) {
                        data = (int16_t *) linear_frame->data;
                        switch_resample_process(oreka->read_resampler, data, (int) linear_frame->datalen / 2);
                        linear_len = oreka->read_resampler->to_len * 2;
                        memcpy(resample_data, oreka->read_resampler->to, linear_len);
                        linear_samples = (int16_t *)resample_data;
                    } else {
                        linear_samples = linear_frame->data;
                        linear_len = linear_frame->datalen;
                    }
                    
                    memset(&pcmu_frame, 0, sizeof(pcmu_frame));
                    for (i = 0; i < linear_len / sizeof(int16_t); i++) {
                        pcmu_data[i] = linear_to_ulaw(linear_samples[i]);
                    }
                    pcmu_frame.source = __SWITCH_FUNC__;
                    pcmu_frame.data = pcmu_data;
                    pcmu_frame.datalen = i;
                    pcmu_frame.payload = 0;

                    switch_rtp_write_frame(oreka->read_rtp_stream, &pcmu_frame);
                }
            }


            if (oreka->read_resampler) {
                switch_resample_destroy(&oreka->read_resampler);
            }

            if (oreka->write_resampler) {
                switch_resample_destroy(&oreka->write_resampler);
            }

			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_INFO, "Stopping Oreka recording for audio stream\n");
			oreka_send_sip_message(oreka, FS_OREKA_STOP, FS_OREKA_READ);
            if (!oreka->mux_streams) {
                oreka_send_sip_message(oreka, FS_OREKA_STOP, FS_OREKA_WRITE);
            }
		}
		break;
	case SWITCH_ABC_TYPE_READ_REPLACE:
    case SWITCH_ABC_TYPE_READ_PING:
		{
			if (pcmu_frame.datalen) {
				if (switch_rtp_write_frame(oreka->read_rtp_stream, &pcmu_frame) > 0) {
					oreka->read_cnt++;
					if (oreka->read_cnt < 10) {
						switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "Oreka wrote %u bytes! (read)\n", pcmu_frame.datalen);
					}
				} else {
					switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "Failed to write %u bytes! (read)\n", pcmu_frame.datalen);
				}
			}
		}
		break;
	case SWITCH_ABC_TYPE_WRITE_REPLACE:
		{
			if (pcmu_frame.datalen) {
				if (switch_rtp_write_frame(oreka->write_rtp_stream, &pcmu_frame) > 0) {
					oreka->write_cnt++;
					if (oreka->write_cnt < 10) {
						switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "Oreka wrote %u bytes! (write)\n", pcmu_frame.datalen);
					}
				} else {
					switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "Failed to write %u bytes! (write)\n", pcmu_frame.datalen);
				}
			}
		}
        break;
	default:
		break;
	}

	return SWITCH_TRUE;
}


SWITCH_STANDARD_APP(oreka_start_function)
{
	switch_status_t status;
	switch_channel_t *channel = switch_core_session_get_channel(session);
	oreka_session_t *oreka = NULL;
	switch_media_bug_t *bug = NULL;
	char *argv[6];
	int argc;
    int flags = 0;
	char *lbuf = NULL;
    const char *var;

	if ((oreka = (oreka_session_t *) switch_channel_get_private(channel, OREKA_PRIVATE))) {
		if (!zstr(data) && !strcasecmp(data, "stop")) {
			switch_channel_set_private(channel, OREKA_PRIVATE, NULL);
			if (oreka->read_bug) {
				switch_core_media_bug_remove(session, &oreka->read_bug);
				oreka->read_bug = NULL;
			}
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "Stopped oreka recorder\n");
		} else {
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_WARNING, "Cannot run oreka recording 2 times on the same session!\n");
		}
		return;
	}

	oreka = switch_core_session_alloc(session, sizeof(*oreka));
	switch_assert(oreka);
	memset(oreka, 0, sizeof(*oreka));

    oreka->mux_streams = globals.mux_streams;

    if ((var = switch_channel_get_variable(channel, "oreka_mux_streams"))) {
        oreka->mux_streams = switch_true(var);
    }

	if (data && (lbuf = switch_core_session_strdup(session, data))
		&& (argc = switch_separate_string(lbuf, ' ', argv, (sizeof(argv) / sizeof(argv[0]))))) {
#if 0
		if (!strncasecmp(argv[x], "server", sizeof("server"))) {
			/* parse server=192.168.1.144 string */
		}
#endif
	}

	oreka->session = session;

    if (oreka->mux_streams) {
        flags = SMBF_READ_STREAM | SMBF_WRITE_STREAM | SMBF_READ_PING | SMBF_ANSWER_REQ;
    } else {
        flags = SMBF_READ_REPLACE | SMBF_WRITE_REPLACE | SMBF_ANSWER_REQ;
    }
    
	status = switch_core_media_bug_add(session, OREKA_BUG_NAME_READ, NULL, oreka_audio_callback, oreka, 0, flags, &bug);
                                       
	if (status != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "Failed to attach oreka to media stream!\n");
		return;
	}
	oreka->read_bug = bug;
	oreka->usecnt++;
	bug = NULL;
	oreka->usecnt++;
	switch_channel_set_private(channel, OREKA_PRIVATE, oreka);

}

#define OREKA_XML_CONFIG "oreka.conf"
static int load_config(void)
{
	switch_xml_t cfg, xml, settings, param;
	if (!(xml = switch_xml_open_cfg(OREKA_XML_CONFIG, &cfg, NULL))) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Failed to open XML configuration '%s'\n", OREKA_XML_CONFIG);
		return -1;
	}

	if ((settings = switch_xml_child(cfg, "settings"))) {
		for (param = switch_xml_child(settings, "param"); param; param = param->next) {
			char *var = (char *) switch_xml_attr_soft(param, "name");
			char *val = (char *) switch_xml_attr_soft(param, "value");
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Found parameter %s=%s\n", var, val);
			if (!strcasecmp(var, "sip-server-addr")) {
				snprintf(globals.sip_server_addr_str, sizeof(globals.sip_server_addr_str), "%s", val);
			} else if (!strcasecmp(var, "sip-server-port")) {
				globals.sip_server_port = atoi(val);
			} else if (!strcasecmp(var, "mux-all-streams")) {
                globals.mux_streams = 1;
			}
		}
	}

	switch_xml_free(xml);
	return 0;
}

SWITCH_MODULE_LOAD_FUNCTION(mod_oreka_load)
{
	switch_application_interface_t *app_interface = NULL;
	int mask = 0;
#if 0
	switch_status_t status = SWITCH_STATUS_FALSE;
	int x = 0;
	switch_size_t len = 0;
	switch_size_t ilen = 0;
	char dummy_output[] = "Parangaricutirimicuaro";
	char dummy_input[sizeof(dummy_output)] = "";
	switch_sockaddr_t *from_addr = NULL;
#endif

	memset(&globals, 0, sizeof(globals));

	if (load_config()) {
		return SWITCH_STATUS_UNLOAD;
	}

	if (zstr(globals.sip_server_addr_str)) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "No sip server address specified!\n");
		return SWITCH_STATUS_UNLOAD;
	}

	if (!globals.sip_server_port) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "No sip server port specified!\n");
		return SWITCH_STATUS_UNLOAD;
	}

	//switch_sockaddr_info_get(&globals.sip_server_addr, "sigchld.sangoma.local", SWITCH_UNSPEC, 5080, 0, pool);
	switch_sockaddr_info_get(&globals.sip_server_addr, globals.sip_server_addr_str, SWITCH_UNSPEC, globals.sip_server_port, 0, pool);

	if (!globals.sip_server_addr) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Invalid sip server address specified: %s!\n", globals.sip_server_addr_str);
		return SWITCH_STATUS_UNLOAD;
	}

	if (switch_socket_create(&globals.sip_socket, switch_sockaddr_get_family(globals.sip_server_addr), SOCK_DGRAM, 0, pool) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Failed to create socket!\n");
		return SWITCH_STATUS_UNLOAD;
	}

	switch_find_local_ip(globals.local_ipv4_str, sizeof(globals.local_ipv4_str), &mask, AF_INET);
	switch_get_addr(globals.sip_server_ipv4_str, sizeof(globals.sip_server_ipv4_str), globals.sip_server_addr);
	globals.our_pid = getpid();

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, 
		"Loading mod_oreka, sip_server_addr=%s, sip_server_ipv4_str=%s, sip_server_port=%d, local_ipv4_str=%s\n", 
		globals.sip_server_addr_str, globals.sip_server_ipv4_str, globals.sip_server_port, globals.local_ipv4_str);

#if 0
	if (switch_socket_bind(globals.sip_socket, globals.sip_addr) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Failed to bind to SIP address: %s!\n", strerror(errno));
		return SWITCH_STATUS_UNLOAD;
	}
#endif

#if 0
	len = sizeof(dummy_output);
#ifndef WIN32
	switch_socket_opt_set(globals.sip_socket, SWITCH_SO_NONBLOCK, TRUE);

	status = switch_socket_sendto(globals.sip_socket, globals.sip_addr, 0, (void *)dummy_output, &len);
	if (status != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Failed to send UDP message! (status=%d)\n", status);
	}

	status = switch_sockaddr_create(&from_addr, pool);
	if (status != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Failed to creat socket address\n");
	}

	while (!ilen) {
		ilen = sizeof(dummy_input);
		status = switch_socket_recvfrom(from_addr, globals.sip_socket, 0, (void *)dummy_input, &ilen);
		if (status != SWITCH_STATUS_SUCCESS && status != SWITCH_STATUS_BREAK) {
			break;
		}

		if (++x > 1000) {
			break;
		}

		switch_cond_next();
	}

	switch_socket_opt_set(globals.sip_socket, SWITCH_SO_NONBLOCK, FALSE);
#endif
#endif

	*module_interface = switch_loadable_module_create_module_interface(pool, modname);

	SWITCH_ADD_APP(app_interface, "oreka_record", "Send media to Oreka recording server", "Send media to Oreka recording server", 
	oreka_start_function, "[stop]", SAF_NONE); 
	return SWITCH_STATUS_SUCCESS;
}

SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_oreka_shutdown)
{
	switch_socket_close(globals.sip_socket);
	return SWITCH_STATUS_UNLOAD;
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
