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
 *
 *
 * mod_exosip.c -- eXoSIP SIP Endpoint
 *
 */


#define HAVE_APR
#include <switch.h>
#include <ccrtp4c.h>
#include <eXosip2/eXosip.h>
#include <osip2/osip_mt.h>
#include <osipparser2/osip_rfc3264.h>
#include <osipparser2/osip_port.h>


static const char modname[] = "mod_exosip";
#define STRLEN 15

static switch_memory_pool *module_pool;

typedef enum {
	PFLAG_ANSWER = (1 << 0),
	PFLAG_HANGUP = (1 << 1),
} PFLAGS;


typedef enum {
	PPFLAG_RING = (1 << 0),
} PPFLAGS;

typedef enum {
	TFLAG_IO = (1 << 0),
	TFLAG_INBOUND = (1 << 1),
	TFLAG_OUTBOUND = (1 << 2),
	TFLAG_DTMF = (1 << 3),
	TFLAG_READING = (1 << 4),
	TFLAG_WRITING = (1 << 5),
	TFLAG_USING_CODEC = (1 << 6),	
	TFLAG_RTP = (1 << 7),
	TFLAG_BYE = (1 << 8)
} TFLAGS;


#define PACKET_LEN 160
#define DEFAULT_BYTES_PER_FRAME 160


static const switch_endpoint_interface exosip_endpoint_interface;

static struct {
	int debug;
	int bytes_per_frame;
	char *dialplan;
	int port;
	int rtp_start;
	int rtp_end;
	switch_hash *call_hash;
	switch_mutex_t *port_lock;
	int running;
	int codec_ms;
} globals;

struct private_object {
	unsigned int flags;		
	switch_core_session *session;
	switch_frame read_frame;
	switch_codec read_codec;
	switch_codec write_codec;
	unsigned char read_buf[1024];
	switch_caller_profile *caller_profile;
	int cid;
	int did;
	int tid;
	int32_t timestamp_send;
	int32_t timestamp_recv;
	int payload_num;
	struct ccrtp4c *rtp_session;
	struct osip_rfc3264 *sdp_config;
	sdp_message_t *remote_sdp;
	sdp_message_t *local_sdp;
	char remote_sdp_audio_ip[50];
	int remote_sdp_audio_port;
	char local_sdp_audio_ip[50];
	int local_sdp_audio_port;
	char call_id[50];
	int ssrc;
	switch_mutex_t *rtp_lock;
};


static int next_rtp_port(void)
{
	int port;

	switch_mutex_lock(globals.port_lock);
	port = globals.rtp_start;
	globals.rtp_start += 2;
	if (port >= globals.rtp_end) {
		port = globals.rtp_start;
	}
	switch_mutex_unlock(globals.port_lock);
	return port;
}


static void set_global_dialplan(char *dialplan)
{
	if (globals.dialplan) {
		free(globals.dialplan);
		globals.dialplan = NULL;
	}
	
	globals.dialplan = strdup(dialplan);
}

static void set_global_dialplan(char *dialplan);
static switch_status exosip_on_init(switch_core_session *session);
static switch_status exosip_on_hangup(switch_core_session *session);
static switch_status exosip_on_loopback(switch_core_session *session);
static switch_status exosip_on_transmit(switch_core_session *session);
static switch_status exosip_outgoing_channel(switch_core_session *session, switch_caller_profile *outbound_profile, switch_core_session **new_session);
static switch_status exosip_read_frame(switch_core_session *session, switch_frame **frame, int timeout, switch_io_flag flags);
static switch_status exosip_write_frame(switch_core_session *session, switch_frame *frame, int timeout, switch_io_flag flags);
static int config_exosip(int reload);
static switch_status parse_sdp_media(sdp_media_t *media, char **dname, char **drate, char **dpayload);
static switch_status exosip_kill_channel(switch_core_session *session, int sig);
static void activate_rtp(struct private_object *tech_pvt);
static void deactivate_rtp(struct private_object *tech_pvt);

static struct private_object *get_pvt_by_call_id(int id)
{
	char name[50];
	snprintf(name, sizeof(name), "%d", id);
	return (struct private_object *) switch_core_hash_find(globals.call_hash, name);
}

static switch_status exosip_on_execute(switch_core_session *session)
{
	return SWITCH_STATUS_SUCCESS;
}


static int sdp_add_codec(struct osip_rfc3264 *cnf, int codec_type, int payload, char *attribute, int rate, int index)
{
	char tmp[4] = "", string[32] = "";
	sdp_media_t *med = NULL;
	sdp_attribute_t *attr = NULL;

	sdp_media_init(&med);
	if (med == NULL)
		return -1;

	if (!index) {
		snprintf(tmp, sizeof(tmp), "%i", payload);
		med->m_proto = osip_strdup("RTP/AVP");
		osip_list_add(med->m_payloads, osip_strdup(tmp), -1);
	}
	if (attribute) {
		sdp_attribute_init(&attr);
		attr->a_att_field = osip_strdup("rtpmap");
		snprintf(string, sizeof(string), "%i %s/%i", payload, attribute, rate);
		attr->a_att_value = osip_strdup(string);
		osip_list_add(med->a_attributes, attr, -1);
	}

	switch (codec_type) {
	case SWITCH_CODEC_TYPE_AUDIO:
		med->m_media = osip_strdup("audio");
		osip_rfc3264_add_audio_media(cnf, med, -1);
		break;
	case SWITCH_CODEC_TYPE_VIDEO:
		med->m_media = osip_strdup("video");
		osip_rfc3264_add_video_media(cnf, med, -1);
		break;
	default:
		break;
	}
	 return 0;
}


/* 
   State methods they get called when the state changes to the specific state 
   returning SWITCH_STATUS_SUCCESS tells the core to execute the standard state method next
   so if you fully implement the state you can return SWITCH_STATUS_FALSE to skip it.
*/
static switch_status exosip_on_init(switch_core_session *session)
{
	struct private_object *tech_pvt;
	switch_channel *channel = NULL;
	char from_uri[512] = "", localip[128] = "", port[7] = "", *buf = NULL, tmp[512] = "";
	osip_message_t *invite = NULL;

	channel = switch_core_session_get_channel(session);
	assert(channel != NULL);

	tech_pvt = switch_core_session_get_private(session);
	assert(tech_pvt != NULL);

	tech_pvt->read_frame.data = tech_pvt->read_buf;

	switch_console_printf(SWITCH_CHANNEL_CONSOLE, "EXOSIP INIT\n");

	if (switch_test_flag(tech_pvt, TFLAG_OUTBOUND)) {
		char *dest_uri;
		switch_codec_interface *codecs[512];
		int num_codecs = 0;
		/* do SIP Goodies...*/

		/* Generate callerid URI */
		eXosip_guess_localip(AF_INET, localip, 128);
		snprintf(from_uri, sizeof(from_uri), "<sip:%s@%s>", tech_pvt->caller_profile->caller_id_number, localip);
		/* Setup codec negotiation stuffs */
		osip_rfc3264_init(&tech_pvt->sdp_config);
		/* Decide on local IP and rtp port */
		strncpy(tech_pvt->local_sdp_audio_ip, localip, sizeof(tech_pvt->local_sdp_audio_ip));
		tech_pvt->local_sdp_audio_port = next_rtp_port();
		/* Initialize SDP */
		sdp_message_init(&tech_pvt->local_sdp);
		sdp_message_v_version_set(tech_pvt->local_sdp, "0");
		sdp_message_o_origin_set(tech_pvt->local_sdp, "OpenSWITCH2", "0", "0", "IN", "IP4", tech_pvt->local_sdp_audio_ip);
		sdp_message_s_name_set(tech_pvt->local_sdp, "SIP Call");
		sdp_message_c_connection_add(tech_pvt->local_sdp, -1, "IN", "IP4", tech_pvt->local_sdp_audio_ip, NULL, NULL);
		sdp_message_t_time_descr_add(tech_pvt->local_sdp, "0", "0");
		snprintf(port, sizeof(port), "%i", tech_pvt->local_sdp_audio_port);
		sdp_message_m_media_add(tech_pvt->local_sdp, "audio", port, NULL, "RTP/AVP");
		/* Add in every codec we support on this outbound call */
		if ((num_codecs = loadable_module_get_codecs(switch_core_session_get_pool(session), codecs, sizeof(codecs)/sizeof(codecs[0]))) > 0) {
			int i;
			static const switch_codec_implementation *imp;
			for (i = 0; i < num_codecs; i++) {
				int x = 0;

				snprintf(tmp, sizeof(tmp), "%i", codecs[i]->ianacode);
				sdp_message_m_payload_add(tech_pvt->local_sdp, 0, osip_strdup(tmp));
				for (imp = codecs[i]->implementations ; imp ; imp = imp->next) {
					/* Add to SDP config */
					sdp_add_codec(tech_pvt->sdp_config, codecs[i]->codec_type, codecs[i]->ianacode, codecs[i]->iananame, imp->samples_per_second, x++);
					/* Add to SDP message */

					snprintf(tmp, sizeof(tmp), "%i %s/%i", codecs[i]->ianacode, codecs[i]->iananame, imp->samples_per_second);
					sdp_message_a_attribute_add(tech_pvt->local_sdp, 0, "rtpmap", osip_strdup(tmp));
					memset(tmp, 0, sizeof(tmp));
				}
			}
		}
		/* Setup our INVITE */
		eXosip_lock();
		if (!(dest_uri = (char *) switch_core_session_alloc(session, strlen(tech_pvt->caller_profile->destination_number) + 10))) {
			switch_console_printf(SWITCH_CHANNEL_CONSOLE, "AIEEEE!\n");
			assert(dest_uri != NULL);
		}
		sprintf(dest_uri, "sip:%s", tech_pvt->caller_profile->destination_number);
		eXosip_call_build_initial_invite(&invite, dest_uri, from_uri, NULL, NULL);
		osip_message_set_supported(invite, "100rel, replaces");
		/* Add SDP to the INVITE */
		sdp_message_to_str(tech_pvt->local_sdp, &buf);
		osip_message_set_body(invite, buf, strlen(buf));
		osip_message_set_content_type(invite, "application/sdp");
		switch_console_printf(SWITCH_CHANNEL_CONSOLE, "OUTBOUND SDP:\n%s\n", buf);
		free(buf);
		/* Send the INVITE */
		tech_pvt->cid = eXosip_call_send_initial_invite(invite);
		snprintf(tech_pvt->call_id, sizeof(tech_pvt->call_id), "%d", tech_pvt->cid);
		switch_core_hash_insert(globals.call_hash, tech_pvt->call_id, tech_pvt);
		tech_pvt->did = -1;
		eXosip_unlock();
	} 

	/* Let Media Work */
	switch_set_flag(tech_pvt, TFLAG_IO);

	/* Move Channel's State Machine to RING */
	switch_channel_set_state(channel, CS_RING);
	return SWITCH_STATUS_SUCCESS;
}

static switch_status exosip_on_ring(switch_core_session *session)
{
	switch_channel *channel = NULL;
	struct private_object *tech_pvt = NULL;

	channel = switch_core_session_get_channel(session);
	assert(channel != NULL);

	tech_pvt = switch_core_session_get_private(session);
	assert(tech_pvt != NULL);

	switch_console_printf(SWITCH_CHANNEL_CONSOLE, "EXOSIP RING\n");

	return SWITCH_STATUS_SUCCESS;
}

static switch_status exosip_on_hangup(switch_core_session *session)
{
	struct private_object *tech_pvt;
	switch_channel *channel = NULL;
	int i;

	channel = switch_core_session_get_channel(session);
	assert(channel != NULL);

	tech_pvt = switch_core_session_get_private(session);
	assert(tech_pvt != NULL);


	switch_core_hash_delete(globals.call_hash, tech_pvt->call_id);	
	

	switch_set_flag(tech_pvt, TFLAG_BYE);
	switch_clear_flag(tech_pvt, TFLAG_IO);

	deactivate_rtp(tech_pvt);

	i = eXosip_call_terminate(tech_pvt->cid, tech_pvt->did);

	if (switch_test_flag(tech_pvt, TFLAG_USING_CODEC)) {
		switch_core_codec_destroy(&tech_pvt->read_codec);
		switch_core_codec_destroy(&tech_pvt->write_codec);
	}

	switch_console_printf(SWITCH_CHANNEL_CONSOLE, "EXOSIP HANGUP %s %d/%d=%d\n", switch_channel_get_name(channel), tech_pvt->cid, tech_pvt->did, i);
	return SWITCH_STATUS_SUCCESS;
}

static switch_status exosip_on_loopback(switch_core_session *session)
{
	switch_console_printf(SWITCH_CHANNEL_CONSOLE, "EXOSIP LOOPBACK\n");
	return SWITCH_STATUS_SUCCESS;
}

static switch_status exosip_on_transmit(switch_core_session *session)
{
	switch_console_printf(SWITCH_CHANNEL_CONSOLE, "EXOSIP TRANSMIT\n");
	return SWITCH_STATUS_SUCCESS;
}

static switch_status exosip_outgoing_channel(switch_core_session *session, switch_caller_profile *outbound_profile, switch_core_session **new_session)
{
	if ((*new_session = switch_core_session_request(&exosip_endpoint_interface, NULL))) {
		struct private_object *tech_pvt;
		switch_channel *channel, *orig_channel;
		switch_caller_profile *caller_profile, *originator_caller_profile = NULL;

		if ((tech_pvt = (struct private_object *) switch_core_session_alloc(*new_session, sizeof(struct private_object)))) {
			memset(tech_pvt, 0, sizeof(*tech_pvt));
			channel = switch_core_session_get_channel(*new_session);
			switch_core_session_set_private(*new_session, tech_pvt);
			switch_mutex_init(&tech_pvt->rtp_lock, SWITCH_MUTEX_NESTED, switch_core_session_get_pool(*new_session));
			tech_pvt->session = *new_session;
		} else {
			switch_console_printf(SWITCH_CHANNEL_CONSOLE, "Hey where is my memory pool?\n");
			switch_core_session_destroy(new_session);
			return SWITCH_STATUS_GENERR;
		}

		if (outbound_profile) {
			char name[128];
			caller_profile = switch_caller_profile_clone(*new_session, outbound_profile);
			switch_channel_set_caller_profile(channel, caller_profile);
			tech_pvt->caller_profile = caller_profile;
			snprintf(name, sizeof(name), "Exosip/%s-%04x", caller_profile->destination_number, rand() & 0xffff);
			switch_channel_set_name(channel, name);
		} else {
			switch_console_printf(SWITCH_CHANNEL_CONSOLE, "Doh! no caller profile\n");
			switch_core_session_destroy(new_session);
			return SWITCH_STATUS_GENERR;
		}

		/* (session == NULL) means it was originated from the core not from another channel */
		if (session && (orig_channel = switch_core_session_get_channel(session))) {
			switch_caller_profile *cloned_profile;

			if ((originator_caller_profile = switch_channel_get_caller_profile(orig_channel))) {
				cloned_profile = switch_caller_profile_clone(*new_session, originator_caller_profile);
				switch_channel_set_originator_caller_profile(channel, cloned_profile);
			}
		}
		
		switch_channel_set_flag(channel, CF_OUTBOUND);
		switch_set_flag(tech_pvt, TFLAG_OUTBOUND);
		switch_channel_set_state(channel, CS_INIT);
		return SWITCH_STATUS_SUCCESS;
	}

	return SWITCH_STATUS_GENERR;
}


static void deactivate_rtp(struct private_object *tech_pvt)
{
	int loops = 0;
	if (tech_pvt->rtp_session) {
		switch_mutex_lock(tech_pvt->rtp_lock);

		while(loops < 10 && (switch_test_flag(tech_pvt, TFLAG_READING) || switch_test_flag(tech_pvt, TFLAG_WRITING))) {
			switch_yield(10000);
			loops++;
		}

		ccrtp4c_destroy(&tech_pvt->rtp_session);
		tech_pvt->rtp_session = NULL;
		switch_mutex_unlock(tech_pvt->rtp_lock);
	}
}

static void activate_rtp(struct private_object *tech_pvt)
{
	int bw, ms;
	switch_channel *channel;

	assert(tech_pvt != NULL);

	channel = switch_core_session_get_channel(tech_pvt->session);
	assert(channel != NULL);



	switch_mutex_lock(tech_pvt->rtp_lock);
	if (switch_test_flag(tech_pvt, TFLAG_USING_CODEC)) {
		bw = tech_pvt->read_codec.implementation->bits_per_second;
		ms = tech_pvt->read_codec.implementation->microseconds_per_frame;
	} else {
		switch_channel_get_raw_mode(channel, NULL, NULL, NULL, &ms, &bw);
		bw *= 8;
	}

	switch_console_printf(SWITCH_CHANNEL_CONSOLE, "Activating RTP %s:%d->%s:%d codec: %d ms: %d\n",
						  tech_pvt->local_sdp_audio_ip,
						  tech_pvt->local_sdp_audio_port,
						  tech_pvt->remote_sdp_audio_ip,
						  tech_pvt->remote_sdp_audio_port,
						  tech_pvt->read_codec.codec_interface->ianacode,
						  ms
						  );




	tech_pvt->rtp_session = ccrtp4c_new(
										tech_pvt->local_sdp_audio_ip,
										tech_pvt->local_sdp_audio_port,
										tech_pvt->remote_sdp_audio_ip,
										tech_pvt->remote_sdp_audio_port,
										tech_pvt->read_codec.codec_interface->ianacode,
										ms ,
										ms * 20);

	if (tech_pvt->rtp_session) {
		tech_pvt->ssrc = ccrtp4c_get_ssrc(tech_pvt->rtp_session);
		ccrtp4c_start(tech_pvt->rtp_session);
		switch_set_flag(tech_pvt, TFLAG_RTP);
	} else {
		switch_console_printf(SWITCH_CHANNEL_CONSOLE, "Oh oh?\n");
	}

	switch_mutex_unlock(tech_pvt->rtp_lock);
}

static switch_status exosip_answer_channel(switch_core_session *session)
{
	struct private_object *tech_pvt;
	switch_channel *channel = NULL;

	channel = switch_core_session_get_channel(session);
	assert(channel != NULL);

	tech_pvt = switch_core_session_get_private(session);
	assert(tech_pvt != NULL);

	if (!switch_channel_test_flag(channel, CF_OUTBOUND)) {
		char *buf = NULL;
		osip_message_t *answer = NULL;


		/* Transmit 200 OK with SDP */
		eXosip_lock();
		eXosip_call_build_answer(tech_pvt->tid, 200, &answer);
		sdp_message_to_str(tech_pvt->local_sdp, &buf);
		osip_message_set_body(answer, buf, strlen(buf));
		osip_message_set_content_type(answer, "application/sdp");
		free(buf); 
		eXosip_call_send_answer(tech_pvt->tid, 200, answer);
		eXosip_unlock();
	} 


	return SWITCH_STATUS_SUCCESS;
}


static switch_status exosip_read_frame(switch_core_session *session, switch_frame **frame, int timeout, switch_io_flag flags) 
{
	struct private_object *tech_pvt = NULL;
	size_t bytes = 0, samples = 0, frames=0, ms=0;
	switch_channel *channel = NULL;
	switch_time_t reference, now;
	int mult = 2;

	channel = switch_core_session_get_channel(session);
	assert(channel != NULL);

	tech_pvt = switch_core_session_get_private(session);
	assert(tech_pvt != NULL);

	tech_pvt->read_frame.datalen = 0;
	switch_set_flag(tech_pvt, TFLAG_READING);

	if (switch_test_flag(tech_pvt, TFLAG_USING_CODEC)) {
		bytes = tech_pvt->read_codec.implementation->encoded_bytes_per_frame;
		samples = tech_pvt->read_codec.implementation->samples_per_frame;
		ms = tech_pvt->read_codec.implementation->microseconds_per_frame;
	} else {
		assert(0);
	}

	if (switch_test_flag(tech_pvt, TFLAG_IO)) {
		if (!switch_test_flag(tech_pvt, TFLAG_RTP)) {
			activate_rtp(tech_pvt);
		}

		assert(tech_pvt->rtp_session != NULL);
		tech_pvt->read_frame.datalen = 0;
		
		reference = switch_time_now();
		reference += (ms * mult);
		while(!switch_test_flag(tech_pvt, TFLAG_BYE) && switch_test_flag(tech_pvt, TFLAG_IO) && tech_pvt->read_frame.datalen == 0) {
			if ((tech_pvt->read_frame.datalen = 
				 ccrtp4c_read(tech_pvt->rtp_session,
							  tech_pvt->read_frame.data,
							  sizeof(tech_pvt->read_buf),
							  &tech_pvt->timestamp_recv))) {
				bytes = tech_pvt->read_codec.implementation->encoded_bytes_per_frame;
				frames = (tech_pvt->read_frame.datalen / bytes);
				samples = frames * tech_pvt->read_codec.implementation->samples_per_frame;
				ms = frames * tech_pvt->read_codec.implementation->microseconds_per_frame;
				tech_pvt->timestamp_recv += samples;
				break;
			}
			
			now = switch_time_now();
			if (now >= reference) {
				printf("TO\n");
				//memset(tech_pvt->read_buf, 0, bytes *2);
				//tech_pvt->timestamp_recv += (samples * mult);
				//reference += (ms * mult);
				//tech_pvt->read_frame.datalen = bytes *2;
				//break;
			}

			switch_yield(100);
		}
		
		//printf("%s %s->%s recv %d bytes %d samples in %d frames taking up %d ms ts=%d\n", switch_channel_get_name(channel), tech_pvt->local_sdp_audio_ip, tech_pvt->local_sdp_audio_ip, tech_pvt->read_frame.datalen, samples, frames, ms, tech_pvt->timestamp_recv);
		

		//switch_mutex_unlock(tech_pvt->rtp_lock);

	} else {
		memset(tech_pvt->read_buf, 0, 160);
		tech_pvt->read_frame.datalen = 160;
	}

	switch_clear_flag(tech_pvt, TFLAG_READING);

	if (switch_test_flag(tech_pvt, TFLAG_BYE)) {
		switch_channel_hangup(channel);
		return SWITCH_STATUS_FALSE;
	}

	*frame = &tech_pvt->read_frame;

	
	return SWITCH_STATUS_SUCCESS;
}


static switch_status exosip_write_frame(switch_core_session *session, switch_frame *frame, int timeout, switch_io_flag flags)
{
	struct private_object *tech_pvt;
	switch_channel *channel = NULL;
	switch_status status = SWITCH_STATUS_SUCCESS;
	int bytes=0, samples=0, ms=0, frames=0;

	channel = switch_core_session_get_channel(session);
	assert(channel != NULL);

	tech_pvt = switch_core_session_get_private(session);
	assert(tech_pvt != NULL);
	
	if (!switch_test_flag(tech_pvt, TFLAG_RTP)) {
		activate_rtp(tech_pvt);
		//return SWITCH_STATUS_SUCCESS;
	}

	if (!switch_test_flag(tech_pvt, TFLAG_IO)) {
		return SWITCH_STATUS_SUCCESS;
	}

	if (switch_test_flag(tech_pvt, TFLAG_BYE)) {
		switch_channel_hangup(channel);
		return SWITCH_STATUS_FALSE;
	}

	switch_set_flag(tech_pvt, TFLAG_WRITING);
	//switch_mutex_lock(tech_pvt->rtp_lock);

	if (switch_test_flag(tech_pvt, TFLAG_USING_CODEC)) {
		bytes = tech_pvt->read_codec.implementation->encoded_bytes_per_frame;
		frames = (frame->datalen / bytes);
		samples = frames * tech_pvt->read_codec.implementation->samples_per_frame;
		ms = frames * tech_pvt->read_codec.implementation->microseconds_per_frame / 1000;
	} else {
		assert(0);
	}
	

	//printf("%s %s->%s send %d bytes %d samples in %d frames taking up %d ms ts=%d\n", switch_channel_get_name(channel), tech_pvt->local_sdp_audio_ip, tech_pvt->remote_sdp_audio_ip, frame->datalen, samples, frames, ms, tech_pvt->timestamp_send);
	
	tech_pvt->timestamp_send += (int)samples;
	ccrtp4c_write(tech_pvt->rtp_session, frame->data, frame->datalen, &tech_pvt->timestamp_send);

	


	switch_clear_flag(tech_pvt, TFLAG_WRITING);
	//switch_mutex_unlock(tech_pvt->rtp_lock);
	return status;
}



static switch_status exosip_kill_channel(switch_core_session *session, int sig)
{
	struct private_object *tech_pvt;
	switch_channel *channel = NULL;

	channel = switch_core_session_get_channel(session);
	assert(channel != NULL);

	tech_pvt = switch_core_session_get_private(session);
	assert(tech_pvt != NULL);


	switch_clear_flag(tech_pvt, TFLAG_IO);
	switch_set_flag(tech_pvt, TFLAG_BYE);
	
	return SWITCH_STATUS_SUCCESS;

}

static switch_status exosip_waitfor_read(switch_core_session *session, int ms)
{
	struct private_object *tech_pvt;
	switch_channel *channel = NULL;

	channel = switch_core_session_get_channel(session);
	assert(channel != NULL);

	tech_pvt = switch_core_session_get_private(session);
	assert(tech_pvt != NULL);

	return SWITCH_STATUS_SUCCESS;
}


static switch_status exosip_waitfor_write(switch_core_session *session, int ms)
{
	struct private_object *tech_pvt;
	switch_channel *channel = NULL;

	channel = switch_core_session_get_channel(session);
	assert(channel != NULL);

	tech_pvt = switch_core_session_get_private(session);
	assert(tech_pvt != NULL);
	
	return SWITCH_STATUS_SUCCESS;

}
	
static const switch_io_routines exosip_io_routines = {
	/*.outgoing_channel*/	exosip_outgoing_channel,
	/*.answer_channel*/		exosip_answer_channel,
	/*.read_frame*/			exosip_read_frame,
	/*.write_frame*/		exosip_write_frame,
	/*.kill_channel*/		exosip_kill_channel,
	/*.waitfor_read*/		exosip_waitfor_read,
	/*.waitfor_read*/		exosip_waitfor_write
};

static const switch_event_handler_table  exosip_event_handlers = {
	/*.on_init*/			exosip_on_init,
	/*.on_ring*/			exosip_on_ring,
	/*.on_execute*/			exosip_on_execute,
	/*.on_hangup*/			exosip_on_hangup,
	/*.on_loopback*/		exosip_on_loopback,
	/*.on_transmit*/		exosip_on_transmit
};

static const switch_endpoint_interface exosip_endpoint_interface = {
	/*.interface_name*/		"exosip",
	/*.io_routines*/		&exosip_io_routines,
	/*.event_handlers*/		&exosip_event_handlers,
	/*.private*/			NULL,
	/*.next*/				NULL
};

static const switch_loadable_module_interface exosip_module_interface = {
	/*.module_name*/			modname,
	/*.endpoint_interface*/		&exosip_endpoint_interface,
	/*.timer_interface*/		NULL,
	/*.dialplan_interface*/		NULL,
	/*.codec_interface*/		NULL,
	/*.application_interface*/	NULL
};


SWITCH_MOD_DECLARE(switch_status) switch_module_shutdown(void)
{
	if (globals.running) {
		globals.running = -1;
		while(globals.running) {
			switch_yield(1000);
		}
	}
	eXosip_quit();
	return SWITCH_STATUS_SUCCESS;
}

SWITCH_MOD_DECLARE(switch_status) switch_module_load(const switch_loadable_module_interface **interface) {
	/* NOTE:  **interface is **_interface because the common lib redefines interface to struct in some situations */

	if (switch_core_new_memory_pool(&module_pool) != SWITCH_STATUS_SUCCESS) {
		switch_console_printf(SWITCH_CHANNEL_CONSOLE, "OH OH no pool\n");
		return SWITCH_STATUS_TERM;
	}
	
	/* connect my internal structure to the blank pointer passed to me */
	*interface = &exosip_module_interface;

	/* indicate that the module should continue to be loaded */
	return SWITCH_STATUS_SUCCESS;
}

static switch_status exosip_create_call(eXosip_event_t *event)
{
	switch_core_session *session;
	sdp_message_t *remote_sdp = NULL;
	sdp_connection_t *conn = NULL;
	sdp_media_t *remote_med = NULL, *audio_tab[10], *video_tab[10], *t38_tab[10], *app_tab[10];
	char local_sdp_str[8192] = "", port[8] = "";
	int mline = 0, pos = 0;
	switch_channel *channel = NULL;
	char name[128];
	char *dpayload, *dname, *drate;
	char *remote_sdp_str = NULL;

	if ((session = switch_core_session_request(&exosip_endpoint_interface, NULL))) {
		struct private_object *tech_pvt;
		switch_codec_interface *codecs[512];
		int num_codecs = 0;


		if ((tech_pvt = (struct private_object *) switch_core_session_alloc(session, sizeof(struct private_object)))) {
			memset(tech_pvt, 0, sizeof(*tech_pvt));
			channel = switch_core_session_get_channel(session);
			switch_core_session_set_private(session, tech_pvt);
			tech_pvt->session = session;
			switch_mutex_init(&tech_pvt->rtp_lock, SWITCH_MUTEX_NESTED, switch_core_session_get_pool(session));
		} else {
			switch_console_printf(SWITCH_CHANNEL_CONSOLE, "Hey where is my memory pool?\n");
			switch_core_session_destroy(&session);
			return SWITCH_STATUS_MEMERR;
		}
		
		if ((tech_pvt->caller_profile = switch_caller_profile_new(session,
															   globals.dialplan,
															   event->request->from->displayname,
															   event->request->from->url->username,
															   event->request->from->url->username,
															   NULL,
															   event->request->req_uri->username))) {
			switch_channel_set_caller_profile(channel, tech_pvt->caller_profile);
		}

		switch_set_flag(tech_pvt, TFLAG_INBOUND);
		tech_pvt->did = event->did;
		tech_pvt->cid = event->cid;
		tech_pvt->tid = event->tid;

		snprintf(name, sizeof(name), "Exosip/%s-%04x", tech_pvt->caller_profile->destination_number, rand() & 0xffff);
		switch_channel_set_name(channel, name);
		
		if (!(remote_sdp = eXosip_get_sdp_info(event->request))) {
			switch_console_printf(SWITCH_CHANNEL_CONSOLE, "Cannot Find Remote SDP!\n");
			exosip_on_hangup(session);
			switch_core_session_destroy(&session);
			return SWITCH_STATUS_GENERR;
		}

		eXosip_guess_localip(AF_INET, tech_pvt->local_sdp_audio_ip, 50);
		tech_pvt->local_sdp_audio_port = next_rtp_port();
		osip_rfc3264_init(&tech_pvt->sdp_config);
		/* Add in what codecs we support locally */
		
		if ((num_codecs = loadable_module_get_codecs(switch_core_session_get_pool(session), codecs, sizeof(codecs)/sizeof(codecs[0]))) > 0) {
			int i;
			static const switch_codec_implementation *imp;

			for (i = 0; i < num_codecs; i++) {
				int x = 0;
				for (imp = codecs[i]->implementations ; imp ; imp = imp->next) {
					sdp_add_codec(tech_pvt->sdp_config, codecs[i]->codec_type, codecs[i]->ianacode, codecs[i]->iananame, imp->samples_per_second, x++);
				}
			}
		}
		osip_rfc3264_prepare_answer(tech_pvt->sdp_config, remote_sdp, local_sdp_str, 8192);
		sdp_message_init(&tech_pvt->local_sdp);
		sdp_message_parse(tech_pvt->local_sdp, local_sdp_str);

		sdp_message_to_str(remote_sdp, &remote_sdp_str);
		switch_console_printf(SWITCH_CHANNEL_CONSOLE, "LOCAL SDP:\n%s\nREMOTE SDP:\n%s", local_sdp_str,remote_sdp_str);
		
		mline = 0;
		while (0==osip_rfc3264_match(tech_pvt->sdp_config, remote_sdp, audio_tab, video_tab, t38_tab, app_tab, mline)) {
			if (audio_tab[0] == NULL && video_tab[0] == NULL && t38_tab[0] == NULL && app_tab[0] == NULL) {

				switch_console_printf(SWITCH_CHANNEL_CONSOLE, "Got no compatible codecs!\n");
				break;
			}
			for (pos=0; audio_tab[pos]!=NULL; pos++) {
				osip_rfc3264_complete_answer(tech_pvt->sdp_config, remote_sdp, tech_pvt->local_sdp, audio_tab[pos], mline);
				if (parse_sdp_media(audio_tab[pos], &dname, &drate, &dpayload) == SWITCH_STATUS_SUCCESS) {
					tech_pvt->payload_num = atoi(dpayload);
					break;
				}
			}
			mline++;
		}
		free(remote_sdp_str);
		sdp_message_o_origin_set(tech_pvt->local_sdp, "OpenSWITCH2", "0", "0", "IN", "IP4", tech_pvt->local_sdp_audio_ip);
		sdp_message_s_name_set(tech_pvt->local_sdp, "SIP Call");
		sdp_message_c_connection_add(tech_pvt->local_sdp, -1, "IN", "IP4", tech_pvt->local_sdp_audio_ip, NULL, NULL);
		snprintf(port, sizeof(port), "%i", tech_pvt->local_sdp_audio_port);
		sdp_message_m_port_set(tech_pvt->local_sdp, 0, osip_strdup(port));

		conn = eXosip_get_audio_connection(remote_sdp);
		remote_med = eXosip_get_audio_media(remote_sdp);
		snprintf(tech_pvt->remote_sdp_audio_ip, 50, conn->c_addr);

		tech_pvt->remote_sdp_audio_port = atoi(remote_med->m_port);
		
		snprintf(tech_pvt->call_id, sizeof(tech_pvt->call_id), "%d", event->cid);
		switch_core_hash_insert(globals.call_hash, tech_pvt->call_id, tech_pvt);	

		if (!dname) {
			exosip_on_hangup(session);
			switch_core_session_destroy(&session);
			return SWITCH_STATUS_GENERR;
		}

		switch_channel_set_state(channel, CS_INIT);


		if (1) {
			int rate = atoi(drate);

			if (switch_core_codec_init(&tech_pvt->read_codec,
									dname,
									rate,
									globals.codec_ms,
									SWITCH_CODEC_FLAG_ENCODE | SWITCH_CODEC_FLAG_DECODE,
									NULL) != SWITCH_STATUS_SUCCESS) {
				switch_console_printf(SWITCH_CHANNEL_CONSOLE, "Can't load codec?\n");
				switch_channel_hangup(channel);
				return SWITCH_STATUS_FALSE;
			} else {
				if (switch_core_codec_init(&tech_pvt->write_codec,
										dname,
										rate,
										globals.codec_ms,
										SWITCH_CODEC_FLAG_ENCODE | SWITCH_CODEC_FLAG_DECODE,
										NULL) != SWITCH_STATUS_SUCCESS) {
					switch_console_printf(SWITCH_CHANNEL_CONSOLE, "Can't load codec?\n");
					switch_channel_hangup(channel);
					return SWITCH_STATUS_FALSE;
				} else {
					int ms;
					switch_set_flag(tech_pvt, TFLAG_USING_CODEC);
					ms = tech_pvt->write_codec.implementation->microseconds_per_frame / 1000;
					switch_console_printf(SWITCH_CHANNEL_CONSOLE, "Activate Inbound Codec %s/%d %d ms\n", dname, rate, ms);
					tech_pvt->read_frame.codec = &tech_pvt->read_codec;
					switch_core_session_set_read_codec(session, &tech_pvt->read_codec);
					switch_core_session_set_write_codec(session, &tech_pvt->write_codec);
				}
			}
		}
		
		switch_core_session_thread_launch(session);
	} else {
		switch_console_printf(SWITCH_CHANNEL_CONSOLE, "Cannot Create new Inbound Channel!\n");
	}


	return 0;



}

static void destroy_call_by_event(eXosip_event_t *event)
{
	struct private_object *tech_pvt;
	switch_channel *channel = NULL;

	if (!(tech_pvt = get_pvt_by_call_id(event->cid))) {
		switch_console_printf(SWITCH_CHANNEL_CONSOLE, "Um in case you are interested, Can't find the pvt [%d]!\n", event->cid);
		return;
	}

	channel = switch_core_session_get_channel(tech_pvt->session);
	assert(channel != NULL);

	switch_console_printf(SWITCH_CHANNEL_CONSOLE, "destroy %s\n", switch_channel_get_name(channel));
	exosip_kill_channel(tech_pvt->session, SWITCH_SIG_KILL);
	switch_channel_hangup(channel);
	
}

static switch_status parse_sdp_media(sdp_media_t *media, char **dname, char **drate, char **dpayload)
{
	int pos = 0;
	sdp_attribute_t *attr = NULL;
	char *name, *rate, *payload;
	switch_status status = SWITCH_STATUS_GENERR;

	while (osip_list_eol(media->a_attributes, pos) == 0) {
		attr = (sdp_attribute_t *)osip_list_get(media->a_attributes, pos);
		if (attr != NULL && strcasecmp(attr->a_att_field, "rtpmap") == 0) {
			payload = attr->a_att_value;
			if ((name = strchr(payload, ' '))) {
				*(name++) = '\0';
				/* Name and payload are required */
				*dpayload = strdup(payload);
				status = SWITCH_STATUS_SUCCESS;
				if ((rate = strchr(name, '/'))) {
					*(rate++) = '\0';
					*drate = strdup(rate);
					*dname = strdup(name);
				} else {
					*dname = strdup(name);
					*drate = strdup("8000");
				}
			} else {
				*dpayload = strdup("10");
				*dname = strdup("L16");
				*drate = strdup("8000");
			}
			switch_console_printf(SWITCH_CHANNEL_CONSOLE, "Found negotiated codec Payload: %s Name: %s Rate: %s\n", *dpayload, *dname, *drate);
			break;
		}
		attr = NULL;
		pos++;
	}

	return status;
}

static void handle_answer(eXosip_event_t *event)
{
	osip_message_t *ack = NULL;
	sdp_message_t *remote_sdp = NULL;
	sdp_connection_t *conn = NULL;
	sdp_media_t *remote_med = NULL;
	struct private_object *tech_pvt;
	char *dpayload = NULL, *dname = NULL, *drate = NULL;
	switch_channel *channel;


	if (!(tech_pvt = get_pvt_by_call_id(event->cid))) {
		switch_console_printf(SWITCH_CHANNEL_CONSOLE, "Um in case you are interested, Can't find the pvt!\n");
		return;
	}

	channel = switch_core_session_get_channel(tech_pvt->session);
	assert(channel != NULL);

	if (!event->response) {
		switch_console_printf(SWITCH_CHANNEL_CONSOLE, "Someone answered... with no SDP information - WTF?!?\n");
		switch_channel_hangup(channel);
		return;
	}

	/* Get all of the remote SDP elements... stuff */
	if (!(remote_sdp = eXosip_get_sdp_info(event->response))) {
		switch_console_printf(SWITCH_CHANNEL_CONSOLE, "Cant Find SDP?\n");
		switch_channel_hangup(channel);
		return;
	}


	conn = eXosip_get_audio_connection(remote_sdp);
	remote_med = eXosip_get_audio_media(remote_sdp);

	/* Grab IP/port */
	tech_pvt->remote_sdp_audio_port = atoi(remote_med->m_port);
	snprintf(tech_pvt->remote_sdp_audio_ip, 50, conn->c_addr);

	/* Grab codec elements */
	if (parse_sdp_media(remote_med, &dname, &drate, &dpayload) == SWITCH_STATUS_SUCCESS) {
		tech_pvt->payload_num = atoi(dpayload);
	}

	/* Assign them thar IDs */
	tech_pvt->did = event->did;
	tech_pvt->tid = event->tid;
	

	if (1) {
		int rate = atoi(drate);


		if (switch_core_codec_init(&tech_pvt->read_codec, dname, rate, globals.codec_ms, SWITCH_CODEC_FLAG_ENCODE | SWITCH_CODEC_FLAG_DECODE, NULL) != SWITCH_STATUS_SUCCESS) {
			switch_console_printf(SWITCH_CHANNEL_CONSOLE, "Can't load codec?\n");
			switch_channel_hangup(channel);
			return;
		} else {
			if (switch_core_codec_init(&tech_pvt->write_codec, dname, rate, globals.codec_ms, SWITCH_CODEC_FLAG_ENCODE |SWITCH_CODEC_FLAG_DECODE, NULL) != SWITCH_STATUS_SUCCESS) {
				switch_console_printf(SWITCH_CHANNEL_CONSOLE, "Can't load codec?\n");
				switch_channel_hangup(channel);
				return;
			} else {
				int ms;
				switch_set_flag(tech_pvt, TFLAG_USING_CODEC);
				ms = tech_pvt->write_codec.implementation->microseconds_per_frame / 1000;
				switch_console_printf(SWITCH_CHANNEL_CONSOLE, "Activate Outbound Codec %s/%d %d ms\n", dname, rate, ms);
				tech_pvt->read_frame.codec = &tech_pvt->read_codec;
				switch_core_session_set_read_codec(tech_pvt->session, &tech_pvt->read_codec);
				switch_core_session_set_write_codec(tech_pvt->session, &tech_pvt->write_codec);
			}
		}
	}


	eXosip_lock();
	eXosip_call_build_ack(event->did, &ack);
	eXosip_call_send_ack(event->did, ack);
	eXosip_unlock();
	
	free(dname);
	free(drate);
	free(dpayload);



	channel = switch_core_session_get_channel(tech_pvt->session);
	assert(channel != NULL);

	switch_channel_answer(channel);

}

static void log_event(eXosip_event_t *je)
{
	char buf[100];

	buf[0] = '\0';
	if (je->type == EXOSIP_CALL_NOANSWER) {
		snprintf (buf, 99, "<- (%i %i) No answer", je->cid, je->did);
	} else if (je->type == EXOSIP_CALL_CLOSED) {
		snprintf (buf, 99, "<- (%i %i) Call Closed", je->cid, je->did);
	} else if (je->type == EXOSIP_CALL_RELEASED) {
		snprintf (buf, 99, "<- (%i %i) Call released", je->cid, je->did);
	} else if (je->type == EXOSIP_MESSAGE_NEW
			   && je->request!=NULL && MSG_IS_MESSAGE(je->request)) {
		char *tmp = NULL;
    
		if (je->request != NULL) {
			osip_body_t *body;
			osip_from_to_str (je->request->from, &tmp);
      
			osip_message_get_body (je->request, 0, &body);
			if (body != NULL && body->body != NULL) {
				snprintf (buf, 99, "<- (%i) from: %s TEXT: %s",
						  je->tid, tmp, body->body);
			}
			osip_free (tmp);
		} else {
			snprintf (buf, 99, "<- (%i) New event for unknown request?", je->tid);
		}
	} else if (je->type == EXOSIP_MESSAGE_NEW) {
		char *tmp = NULL;
    
		osip_from_to_str (je->request->from, &tmp);
		snprintf (buf, 99, "<- (%i) %s from: %s",
				  je->tid, je->request->sip_method, tmp);
		osip_free (tmp);
	} else if (je->type == EXOSIP_MESSAGE_PROCEEDING
             || je->type == EXOSIP_MESSAGE_ANSWERED
             || je->type == EXOSIP_MESSAGE_REDIRECTED
             || je->type == EXOSIP_MESSAGE_REQUESTFAILURE
             || je->type == EXOSIP_MESSAGE_SERVERFAILURE
			   || je->type == EXOSIP_MESSAGE_GLOBALFAILURE) {
		if (je->response != NULL && je->request != NULL) {
			char *tmp = NULL;
      
			osip_to_to_str (je->request->to, &tmp);
			snprintf (buf, 99, "<- (%i) [%i %s for %s] to: %s",
					  je->tid, je->response->status_code,
					  je->response->reason_phrase, je->request->sip_method, tmp);
			osip_free (tmp);
		} else if (je->request != NULL) {
			snprintf (buf, 99, "<- (%i) Error for %s request",
					  je->tid, je->request->sip_method);
		} else {
			snprintf (buf, 99, "<- (%i) Error for unknown request", je->tid);
		}
	} else if (je->response == NULL && je->request != NULL && je->cid > 0) {
		char *tmp = NULL;
    
		osip_from_to_str (je->request->from, &tmp);
		snprintf (buf, 99, "<- (%i %i) %s from: %s",
				  je->cid, je->did, je->request->cseq->method, tmp);
		osip_free (tmp);
	} else if (je->response != NULL && je->cid > 0) {
		char *tmp = NULL;
    
		osip_to_to_str (je->request->to, &tmp);
		snprintf (buf, 99, "<- (%i %i) [%i %s] for %s to: %s",
				  je->cid, je->did, je->response->status_code,
				  je->response->reason_phrase, je->request->sip_method, tmp);
		osip_free (tmp);
	} else if (je->response == NULL && je->request != NULL && je->rid > 0) {
		char *tmp = NULL;
    
		osip_from_to_str (je->request->from, &tmp);
		snprintf (buf, 99, "<- (%i) %s from: %s",
				  je->rid, je->request->cseq->method, tmp);
		osip_free (tmp);
	} else if (je->response != NULL && je->rid > 0) {
		char *tmp = NULL;
    
		osip_from_to_str (je->request->from, &tmp);
		snprintf (buf, 99, "<- (%i) [%i %s] from: %s",
				  je->rid, je->response->status_code,
				  je->response->reason_phrase, tmp);
		osip_free (tmp);
	} else if (je->response == NULL && je->request != NULL && je->sid > 0) {
		char *tmp = NULL;
		char *stat = NULL;
		osip_header_t *sub_state;
    
		osip_message_header_get_byname (je->request, "subscription-state",
										0, &sub_state);
		if (sub_state != NULL && sub_state->hvalue != NULL)
			stat = sub_state->hvalue;
    
		osip_uri_to_str (je->request->from->url, &tmp);
		snprintf (buf, 99, "<- (%i) [%s] %s from: %s",
				  je->sid, stat, je->request->cseq->method, tmp);
		osip_free (tmp);
	} else if (je->response != NULL && je->sid > 0) {
		char *tmp = NULL;
    
		osip_uri_to_str (je->request->to->url, &tmp);
		snprintf (buf, 99, "<- (%i) [%i %s] from: %s",
				  je->sid, je->response->status_code,
				  je->response->reason_phrase, tmp);
		osip_free (tmp);
	} else if (je->response == NULL && je->request != NULL) {
		char *tmp = NULL;
    
		osip_from_to_str (je->request->from, &tmp);
		snprintf (buf, 99, "<- (c=%i|d=%i|s=%i|n=%i) %s from: %s",
				  je->cid, je->did, je->sid, je->nid,
				  je->request->sip_method, tmp);
		osip_free (tmp);
	} else if (je->response != NULL) {
		char *tmp = NULL;
    
		osip_from_to_str (je->request->from, &tmp);
		snprintf (buf, 99, "<- (c=%i|d=%i|s=%i|n=%i) [%i %s] for %s from: %s",
				  je->cid, je->did, je->sid, je->nid,
				  je->response->status_code, je->response->reason_phrase,
				  je->request->sip_method, tmp);
		osip_free (tmp);
	} else {
		snprintf (buf, 99, "<- (c=%i|d=%i|s=%i|n=%i|t=%i) %s",
				  je->cid, je->did, je->sid, je->nid, je->tid, je->textinfo);
	}
	switch_console_printf(SWITCH_CHANNEL_CONSOLE, "\n%s\n", buf);
	/* Print it out */
}



static void *monitor_thread_run(void)
{
	eXosip_event_t *event = NULL;
	
	globals.running = 1;
	while (globals.running > 0) {
		if (!(event = eXosip_event_wait(0,100))) {
			switch_yield(100);
			continue;
		}

		eXosip_lock();
		eXosip_automatic_action ();
		eXosip_unlock();

		log_event(event);

		switch(event->type) {
		case EXOSIP_CALL_INVITE:
			exosip_create_call(event);
			break;
		case EXOSIP_CALL_REINVITE:
			/* See what the reinvite is about - on hold or whatever */
			//handle_reinvite(event);
			switch_console_printf(SWITCH_CHANNEL_CONSOLE, "Got a reinvite.\n");
			break;
		case EXOSIP_CALL_MESSAGE_NEW:
			if (event->request != NULL && MSG_IS_REFER(event->request)) {
				//handle_call_transfer(event);
			}
			break;
		case EXOSIP_CALL_ACK:
			/* If audio is not flowing and this has SDP - fire it up! */
			break;
		case EXOSIP_CALL_ANSWERED:
			switch_console_printf(SWITCH_CHANNEL_CONSOLE, "The call was answered.\n");
			handle_answer(event);
			break;
		case EXOSIP_CALL_PROCEEDING:
			/* This is like a 100 Trying... yeah */
			break;
		case EXOSIP_CALL_RINGING:
			//handle_ringing(event);
			break;
		case EXOSIP_CALL_REDIRECTED:
			switch_console_printf(SWITCH_CHANNEL_CONSOLE, "Call was redirect\n");
			break;
		case EXOSIP_CALL_CLOSED:
			destroy_call_by_event(event);
			break;
		case EXOSIP_CALL_RELEASED:
			destroy_call_by_event(event);
			break;
		case EXOSIP_CALL_NOANSWER:
			switch_console_printf(SWITCH_CHANNEL_CONSOLE, "The call was not answered.\n");
			destroy_call_by_event(event);
			break;
		case EXOSIP_CALL_REQUESTFAILURE:
			switch_console_printf(SWITCH_CHANNEL_CONSOLE, "Request failure\n");
			destroy_call_by_event(event);
			break;
		case EXOSIP_CALL_SERVERFAILURE:
			switch_console_printf(SWITCH_CHANNEL_CONSOLE, "Server failure\n");
			destroy_call_by_event(event);
			break;
		case EXOSIP_CALL_GLOBALFAILURE:
			switch_console_printf(SWITCH_CHANNEL_CONSOLE, "Global failure\n");
			destroy_call_by_event(event);
			break;
			/* Registration related stuff */
		case EXOSIP_REGISTRATION_NEW:
			switch_console_printf(SWITCH_CHANNEL_CONSOLE, "Received registration attempt\n");
			break;
		default:
			/* Unknown event... casually absorb it for now */
			break;
		}

		//switch_console_printf(SWITCH_CHANNEL_CONSOLE, "There was an event (%d) [%s]\n", event->type, event->textinfo);
		/* Free the event */
		eXosip_event_free(event);
	}

	switch_console_printf(SWITCH_CHANNEL_CONSOLE, "Monitor Thread Exiting\n");
	globals.running = 0;
	return NULL;
}


static int config_exosip(int reload) 
{
	switch_config cfg;
	char *var, *val;
	char *cf = "exosip.conf";

	globals.bytes_per_frame = DEFAULT_BYTES_PER_FRAME;
	switch_core_hash_init(&globals.call_hash, module_pool);

	if (!switch_config_open_file(&cfg, cf)) {
		switch_console_printf(SWITCH_CHANNEL_CONSOLE, "open of %s failed\n", cf);
		return SWITCH_STATUS_TERM;
	}

	globals.rtp_start = 16384;
	globals.rtp_end = 32768;

	while (switch_config_next_pair(&cfg, &var, &val)) {
		if (!strcasecmp(cfg.category, "settings")) {
			if (!strcmp(var, "debug")) {
				globals.debug = atoi(val);
			} else if (!strcmp(var, "port")) {
				globals.port = atoi(val);
			} else if (!strcmp(var, "dialplan")) {
				set_global_dialplan(val);
			} else if (!strcmp(var, "rtp_min_port")) {
				globals.rtp_start = atoi(val);
			} else if (!strcmp(var, "rtp_max_port")) {
				globals.rtp_end = atoi(val);
			} else if (!strcmp(var, "codec_ms")) {
				globals.codec_ms = atoi(val);
			}
		}
	}
	
	if (!globals.codec_ms) {
		globals.codec_ms = 20;
	}

	if (!globals.port) {
		globals.port = 5060;
	}

	switch_config_close_file(&cfg);

	if (!globals.dialplan) {
		set_global_dialplan("default");
	}

	if (eXosip_init ()) {
		switch_console_printf(SWITCH_CHANNEL_CONSOLE, "eXosip_init initialization failed!\n");
		return SWITCH_STATUS_GENERR;
	}
	if (eXosip_listen_addr (IPPROTO_UDP, NULL, globals.port, AF_INET, 0)) {
		switch_console_printf(SWITCH_CHANNEL_CONSOLE, "eXosip_listen_addr failed!\n");
		return SWITCH_STATUS_GENERR;
	}

	switch_mutex_init(&globals.port_lock, SWITCH_MUTEX_NESTED, module_pool);


	/* Setup the user agent */
	eXosip_set_user_agent("OPENSWITCH 2.0");


	monitor_thread_run();
	return 0;

}


SWITCH_MOD_DECLARE(switch_status) switch_module_runtime(void)
{
	config_exosip(0);
	return SWITCH_STATUS_TERM;
}

