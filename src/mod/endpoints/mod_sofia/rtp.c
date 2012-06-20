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

#define kRSDP "r_sdp"
#define kLSDP "l_sdp"
#define kBINDADDRESS "bind_address"
#define kCODECSTRING "codec_string"

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
    
    
    const switch_codec_implementation_t *negotiated_codecs[SWITCH_MAX_CODECS];
	int num_negotiated_codecs;
    
    char *origin;
    
    int local_port;

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

SWITCH_STANDARD_API(test_function)
{
    return SWITCH_STATUS_SUCCESS;
}

void crtp_init(switch_loadable_module_interface_t *module_interface)
{
    switch_endpoint_interface_t *endpoint_interface;
    switch_api_interface_t *api_interface;
    crtp.pool = module_interface->pool;
    endpoint_interface = switch_loadable_module_create_interface(module_interface, SWITCH_ENDPOINT_INTERFACE);
    endpoint_interface->interface_name = "rtp";
    endpoint_interface->io_routines = &crtp_io_routines;
    endpoint_interface->state_handler = &crtp_state_handlers;
    crtp.endpoint_interface = endpoint_interface;
    
//    SWITCH_ADD_API(api_interface, "rtp_test", "test", test_function, "");
}


typedef struct parsed_sdp_s {
    const char *c; /*!< Connection */
    
    
} parsed_sdp_t;


//static switch_status_t check_codec 

/* 
 * Setup the local RTP side
 * A lot of values can be chosen by the MG, those will have $ as a placeholder.
 * We need to validate the SDP, making sure we can support everything that's offered on our behalf, 
 * amend it if we don't support everything, or systematically reject it if we cannot support it.
 *
 * Would this function be called using NULL as l_sdp, we will generate an SDP payload.
 */

static switch_status_t setup_local_rtp(crtp_private_t *tech_pvt, const char *l_sdp, const char *codec_string)
{
    switch_core_session_t const * session = tech_pvt->session;    
    int num_codecs;
    const switch_codec_implementation_t *codecs[SWITCH_MAX_CODECS] = { 0 };
    
    char *codec_order[SWITCH_MAX_CODECS] = { 0 };
    int codec_order_last;

    
    /* Load in the list of codecs we support. If we have a codec string we use our priorities first */
    if (codec_string) {
		char *tmp_codec_string;
		if ((tmp_codec_string = switch_core_session_strdup(tech_pvt->session, codec_string))) {
			codec_order_last = switch_separate_string(tmp_codec_string, ',', codec_order, SWITCH_MAX_CODECS);
			num_codecs = switch_loadable_module_get_codecs_sorted(codecs, SWITCH_MAX_CODECS, codec_order, codec_order_last);
		}
	} else {
		num_codecs = switch_loadable_module_get_codecs(codecs, switch_arraylen(codecs));
	}
    
    /* TODO: Look at remote settings */
    
    if (zstr(l_sdp)) {
        /* Generate a local SDP here */
        const char *sdpbuf = switch_core_session_sprintf(session, "v=0\nIN IP4 %s\nm=audio %d RTP/AVP %d");
        
    } else {
        /* Parse the SDP and remove anything we cannot support, then validate it to make sure it contains at least one codec 
         * so that we reject invalid ones. */
#if 1
        uint8_t match = 0;
        int first = 0, last = 0;
        int ptime = 0, dptime = 0, maxptime = 0, dmaxptime = 0;
        int sendonly = 0, recvonly = 0;
        int greedy = 0, x = 0, skip = 0, mine = 0;
        int got_crypto = 0, got_audio = 0, got_avp = 0, got_savp = 0, got_udptl = 0;
#endif
        int sdp_modified = 0;
        int cur_codec = 0;
        
        sdp_parser_t *parser = NULL;
        sdp_session_t *sdp, *lsdp;
        sdp_media_t *m;
        sdp_attribute_t *attr;
        su_home_t *sdp_home;
        
        if (!(parser = sdp_parse(NULL, l_sdp, (int) strlen(l_sdp), sdp_f_megaco /* accept $ values */ | sdp_f_insane /* accept omitted o= */))) {
            switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Malformed SDP\n");
            goto fail;
        }
        
        if (!(sdp = sdp_session(parser))) {
            switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Couldn't get session from sdp.\n");
            goto fail;
        }
        sdp_home = sdp_parser_home(parser);
        for (m = sdp->sdp_media; m; m = m->m_next) {
            
            if (m->m_type == sdp_media_audio) {
                sdp_rtpmap_t *map;
                
                for (attr = m->m_attributes; attr; attr = attr->a_next) {
                    if (!strcasecmp(attr->a_name, "ptime") && attr->a_value) {
                        ptime = atoi(attr->a_value);
                    } else if (!strcasecmp(attr->a_name, "maxptime") && attr->a_value) {
                        maxptime = atoi(attr->a_value);
                    }
                }
            
                sdp_connection_t *connection;
                connection = sdp->sdp_connection;
                if (m->m_connections) {
                    connection = m->m_connections;
                }
            
                /* Check for wildcards in m= (media) and c= (connection) */
                if (!zstr(connection->c_address)) {
                    if (!strcmp(connection->c_address, "$")) {
                        connection->c_address =  su_strdup(sdp_home, tech_pvt->bind_address);
                        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Using bind address: %s\n", tech_pvt->bind_address);
                        switch_channel_set_variable(tech_pvt->channel, kBINDADDRESS, tech_pvt->bind_address);    
                        sdp_modified = 1;
                    } else if (strcmp(connection->c_address, tech_pvt->bind_address)) {
                        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "MGC requested to bind on [%s] which is different than the configuration [%s]\n",
                                        connection->c_address, tech_pvt->bind_address);
                        goto fail;
                    }
                    
                    if (m->m_port == MEGACO_CHOOSE) {
                        tech_pvt->local_port = m->m_port = switch_rtp_request_port(tech_pvt->bind_address);
                        switch_channel_set_variable_printf(tech_pvt->channel, "rtp_local_port", "%d", tech_pvt->local_port);
                        if (!tech_pvt->local_port) {
                            /* Port request failed */
                            switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "No available RTP ports on [%s]\n", tech_pvt->bind_address);
                            goto fail;
                        }
                        sdp_modified = 1;
                        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Using local port: %d\n", tech_pvt->local_port);
                    }
                }
                
                /* Validate codecs */
                for (map = m->m_rtpmaps; map; map = map->rm_next) {
                    if (map->rm_any) {
                        /* Pick our first favorite codec */
                        if (codecs[cur_codec]) {
                            switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Choosing a codec: %s/%d\n", codecs[cur_codec]->iananame, codecs[cur_codec]->ianacode);
                            map->rm_encoding = su_strdup(sdp_home, codecs[cur_codec]->iananame);
                            map->rm_pt = codecs[cur_codec]->ianacode;
                            map->rm_any = 0;
                            map->rm_predef = codecs[cur_codec]->ianacode < 96;
                            cur_codec++;
                        } else {
                            switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "No more codecs in preferences!\n");
                            goto fail;
                        }
                    }
                }
            } else if (m->m_type == sdp_media_image && m->m_port) {
                /* TODO: Handle T38 */
                
            }
        }
        char sdpbuf[2048] = "";
        sdp_printer_t *printer = sdp_print(sdp_home, sdp, sdpbuf, sizeof(sdpbuf), 0);
        switch_channel_set_variable(tech_pvt->channel, kLSDP, sdpbuf);
        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Setting local SDP: [%s]\n", sdpbuf);
        sdp_printer_free(printer);
        
        goto done;
        
        fail:
        if (tech_pvt->local_port) {
            switch_rtp_release_port(tech_pvt->bind_address, tech_pvt->local_port);
        }
        
        if (parser) {
            sdp_parser_free(parser);
        }
        
        return SWITCH_STATUS_FALSE;
    }
    
done:
    return SWITCH_STATUS_SUCCESS;
    
}

#if 0
static void setup_rtp(crtp_private_t *tech_pvt, const char *r_sdp, const char *l_sdp)
{
    switch_core_session_t const * session = tech_pvt->session;
    uint8_t match = 0;
    int first = 0, last = 0;
	int ptime = 0, dptime = 0, maxptime = 0, dmaxptime = 0;
	int sendonly = 0, recvonly = 0;
	int greedy = 0, x = 0, skip = 0, mine = 0;
    int got_crypto = 0, got_audio = 0, got_avp = 0, got_savp = 0, got_udptl = 0;

    sdp_parser_t *parser = NULL, *l_parser = NULL;
	sdp_session_t *sdp, *lsdp;
    sdp_media_t *m;
	sdp_attribute_t *attr;
    
    
    int scrooge = 0;
    
    
    if (zstr(r_sdp)) {
        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "No SDP\n");
        goto fail;
    }
    
    if (!(parser = sdp_parse(NULL, r_sdp, (int) strlen(r_sdp), 0))) {
        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Malformed SDP\n");
        goto fail;
	}
    
	if (!(sdp = sdp_session(parser))) {
        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Couldn't get session from sdp.\n");
        goto fail;
	}

    for (m = sdp->sdp_media; m; m = m->m_next) {
        sdp_connection_t *connection;
        ptime = dptime;
		maxptime = dmaxptime;
        
		if (m->m_proto == sdp_proto_srtp) {
			got_savp++;
		} else if (m->m_proto == sdp_proto_rtp) {
			got_avp++;
		} else if (m->m_proto == sdp_proto_udptl) {
			got_udptl++;
		}
        
        if (got_udptl && m->m_type == sdp_media_image && m->m_port) {
			//switch_t38_options_t *t38_options = tech_process_udptl(tech_pvt, sdp, m);
            
            /* TODO: Process T38 */

        } else if (m->m_type == sdp_media_audio && m->m_port && !got_audio) {
            sdp_rtpmap_t *map;

            connection = sdp->sdp_connection;
			if (m->m_connections) {
				connection = m->m_connections;
			}
            
			if (!connection) {
				switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(tech_pvt->session), SWITCH_LOG_ERROR, "Cannot find a c= line in the sdp at media or session level!\n");
				match = 0;
				break;
			}
            
            /* Begin Codec Negotiation */
            for (map = m->m_rtpmaps; map; map = map->rm_next) {
				int32_t i;
				uint32_t near_rate = 0;
				const switch_codec_implementation_t *mimp = NULL, *near_match = NULL;
				const char *rm_encoding;
				uint32_t map_bit_rate = 0;
				int codec_ms = 0;
				switch_codec_fmtp_t codec_fmtp = { 0 };
                
				if (x++ < skip) {
					continue;
				}
                
				if (!(rm_encoding = map->rm_encoding)) {
					rm_encoding = "";
				}
                
				if (!strcasecmp(rm_encoding, "telephone-event")) {
					if (!best_te || map->rm_rate == tech_pvt->rm_rate) {
						best_te = (switch_payload_t) map->rm_pt;
					}9
				}
				
				if (!cng_pt && !strcasecmp(rm_encoding, "CN")) {
					cng_pt = (switch_payload_t) map->rm_pt;
					if (tech_pvt->rtp_session) {
						switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "Set comfort noise payload to %u\n", cng_pt);
						switch_rtp_set_cng_pt(tech_pvt->rtp_session, tech_pvt->cng_pt);
					}
				}
                
				if (match) {
					continue;
				}
                
				if (greedy) {
					first = mine;
					last = first + 1;
				} else {
					first = 0;
					last = num_codecs;
				}
                
				codec_ms = ptime;
                
				if (maxptime && (!codec_ms || codec_ms > maxptime)) {
					codec_ms = maxptime;
				}
                
				if (!codec_ms) {
					codec_ms = switch_default_ptime(rm_encoding, map->rm_pt);
				}
                
				map_bit_rate = switch_known_bitrate((switch_payload_t)map->rm_pt);
				
				if (!ptime && !strcasecmp(map->rm_encoding, "g723")) {
					ptime = codec_ms = 30;
				}
				
				if (zstr(map->rm_fmtp)) {
					if (!strcasecmp(map->rm_encoding, "ilbc")) {
						ptime = codec_ms = 30;
						map_bit_rate = 13330;
					}
				} else {
					if ((switch_core_codec_parse_fmtp(map->rm_encoding, map->rm_fmtp, map->rm_rate, &codec_fmtp)) == SWITCH_STATUS_SUCCESS) {
						if (codec_fmtp.bits_per_second) {
							map_bit_rate = codec_fmtp.bits_per_second;
						}
						if (codec_fmtp.microseconds_per_packet) {
							codec_ms = (codec_fmtp.microseconds_per_packet / 1000);
						}
					}
				}
                
				
				for (i = first; i < last && i < total_codecs; i++) {
					const switch_codec_implementation_t *imp = codec_array[i];
					uint32_t bit_rate = imp->bits_per_second;
					uint32_t codec_rate = imp->samples_per_second;
					if (imp->codec_type != SWITCH_CODEC_TYPE_AUDIO) {
						continue;
					}
                    
					switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "Audio Codec Compare [%s:%d:%u:%d:%u]/[%s:%d:%u:%d:%u]\n",
									  rm_encoding, map->rm_pt, (int) map->rm_rate, codec_ms, map_bit_rate,
									  imp->iananame, imp->ianacode, codec_rate, imp->microseconds_per_packet / 1000, bit_rate);
					if ((zstr(map->rm_encoding) || (tech_pvt->profile->ndlb & PFLAG_NDLB_ALLOW_BAD_IANANAME)) && map->rm_pt < 96) {
						match = (map->rm_pt == imp->ianacode) ? 1 : 0;
					} else {
						match = strcasecmp(rm_encoding, imp->iananame) ? 0 : 1;
					}
                    
					if (match && bit_rate && map_bit_rate && map_bit_rate != bit_rate && strcasecmp(map->rm_encoding, "ilbc")) {
						/* nevermind */
						match = 0;
					}
					
					if (match) {
						if (scrooge) {
							switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG,
											  "Bah HUMBUG! Sticking with %s@%uh@%ui\n",
											  imp->iananame, imp->samples_per_second, imp->microseconds_per_packet / 1000);
						} else {
							if ((ptime && codec_ms && codec_ms * 1000 != imp->microseconds_per_packet) || map->rm_rate != codec_rate) {
								near_rate = map->rm_rate;
								near_match = imp;
								match = 0;
								continue;
							}
						}
						mimp = imp;
						break;
					} else {
						match = 0;
					}
				}
            }
                
            /* End */
            
            
        }
    }

fail:
    if (parser) {
        sdp_parser_free(parser);
    }
}
#endif

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
    crtp_private_t *tech_pvt;
    const char *r_sdp = switch_event_get_header(var_event, kRSDP);
    const char *l_sdp = switch_event_get_header(var_event, kLSDP);
    const char *codec_string = switch_event_get_header_nil(var_event, kCODECSTRING);

    
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
    
    if (setup_local_rtp(tech_pvt, l_sdp, codec_string) != SWITCH_STATUS_SUCCESS) {
        goto fail;
    }
    
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

