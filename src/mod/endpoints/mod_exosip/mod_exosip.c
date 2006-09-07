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



#define MY_EVENT_REGISTER "exosip::register"
#define MY_EVENT_EXPIRE "exosip::expire"

#define HAVE_APR
#include <switch.h>
#include <eXosip2/eXosip.h>
#include <osip2/osip_mt.h>
#include <osip_rfc3264.h>
#include <osipparser2/osip_port.h>
#define DBFILE "exosip"

static const char modname[] = "mod_exosip";
#define STRLEN 15

static switch_memory_pool_t *module_pool = NULL;



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
	TFLAG_BYE = (1 << 8),
	TFLAG_ANS = (1 << 9),
	TFLAG_EARLY_MEDIA = (1 << 10),
	TFLAG_SECURE = (1 << 11),
	TFLAG_VAD_IN = ( 1 << 12),
	TFLAG_VAD_OUT = ( 1 << 13),
	TFLAG_VAD = ( 1 << 14),
	TFLAG_TIMER = ( 1 << 15),
	TFLAG_AA = (1 << 16),
	TFLAG_PRE_ANSWER = (1 << 17),
	TFLAG_REINVITE = (1 << 18)
} TFLAGS;


#define PACKET_LEN 160
#define DEFAULT_BYTES_PER_FRAME 160

struct reg_element {
	char *key;
	char *url;
	switch_time_t expires;
	int tid;
};

static struct {
	int debug;
	int bytes_per_frame;
	char *dialplan;
	char *extrtpip;
	char *rtpip;
	char *sipip;
	int port;
	char *codec_string;
	char *codec_order[SWITCH_MAX_CODECS];
	int codec_order_last;
	switch_hash_t *call_hash;
	switch_hash_t *srtp_hash;
	int running;
	int codec_ms;
	int dtmf_duration;
	unsigned int flags;
	switch_mutex_t *reg_mutex;
	switch_core_db_t *db;
	switch_payload_t te;
} globals;

struct private_object {
	unsigned int flags;
	switch_core_session_t *session;
	switch_frame_t read_frame;
	switch_codec_t read_codec;
	switch_codec_t write_codec;
	switch_caller_profile_t *caller_profile;
	int cid;
	int did;
	int tid;
	int32_t timestamp_send;
	int32_t timestamp_recv;
	int payload_num;
	switch_rtp_t *rtp_session;
	struct osip_rfc3264 *sdp_config;
	sdp_message_t *remote_sdp;
	sdp_message_t *local_sdp;
	char remote_sdp_audio_ip[50];
	switch_port_t remote_sdp_audio_port;
	char local_sdp_audio_ip[50];
	switch_port_t local_sdp_audio_port;
	char call_id[50];
	int ssrc;
	switch_time_t last_read;
	char *realm;
	const switch_codec_implementation_t *codecs[SWITCH_MAX_CODECS];
	int num_codecs;
	switch_payload_t te;
	switch_mutex_t *flag_mutex;
};



static char create_interfaces_sql[] =
"CREATE TABLE sip_registrations (\n"
"   key             VARCHAR(255),\n"
"   host            VARCHAR(255),\n"
"   url             VARCHAR(255),\n"
"   expires         INTEGER(8)"
");\n";



SWITCH_DECLARE_GLOBAL_STRING_FUNC(set_global_dialplan, globals.dialplan)
SWITCH_DECLARE_GLOBAL_STRING_FUNC(set_global_extrtpip, globals.extrtpip)
SWITCH_DECLARE_GLOBAL_STRING_FUNC(set_global_rtpip, globals.rtpip)
SWITCH_DECLARE_GLOBAL_STRING_FUNC(set_global_codec_string, globals.codec_string)
SWITCH_DECLARE_GLOBAL_STRING_FUNC(set_global_sipip, globals.sipip)

static switch_status_t exosip_on_init(switch_core_session_t *session);
static switch_status_t exosip_on_hangup(switch_core_session_t *session);
static switch_status_t exosip_on_loopback(switch_core_session_t *session);
static switch_status_t exosip_on_transmit(switch_core_session_t *session);
static switch_status_t exosip_outgoing_channel(switch_core_session_t *session, switch_caller_profile_t *outbound_profile,
											 switch_core_session_t **new_session, switch_memory_pool_t *pool);
static switch_status_t exosip_read_frame(switch_core_session_t *session, switch_frame_t **frame, int timeout,
									   switch_io_flag_t flags, int stream_id);
static switch_status_t exosip_write_frame(switch_core_session_t *session, switch_frame_t *frame, int timeout,
										switch_io_flag_t flags, int stream_id);
static int config_exosip(int reload);
static switch_status_t parse_sdp_media(struct private_object *tech_pvt,
									   sdp_media_t * media,
									   char **dname,
									   char **drate,
									   char **dpayload,
									   const switch_codec_implementation_t **impp);

static switch_status_t exosip_kill_channel(switch_core_session_t *session, int sig);
static switch_status_t activate_rtp(struct private_object *tech_pvt);
static void deactivate_rtp(struct private_object *tech_pvt);

static void tech_set_codecs(struct private_object *tech_pvt)
{
	if (tech_pvt->num_codecs) {
		return;
	}

	if (globals.codec_string) {
		tech_pvt->num_codecs = switch_loadable_module_get_codecs_sorted(tech_pvt->codecs,
																		SWITCH_MAX_CODECS,
																		globals.codec_order,
																		globals.codec_order_last);
		
	} else {
		tech_pvt->num_codecs = switch_loadable_module_get_codecs(switch_core_session_get_pool(tech_pvt->session), tech_pvt->codecs,
																 sizeof(tech_pvt->codecs) / sizeof(tech_pvt->codecs[0]));
	}
}


static struct private_object *get_pvt_by_call_id(int id)
{
	char name[50];
	struct private_object *tech_pvt = NULL;
	snprintf(name, sizeof(name), "%d", id);
	eXosip_lock();
	tech_pvt = (struct private_object *) switch_core_hash_find(globals.call_hash, name);
	eXosip_unlock();
	return tech_pvt;
}

static switch_status_t exosip_on_execute(switch_core_session_t *session)
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
static switch_status_t exosip_on_init(switch_core_session_t *session)
{
	struct private_object *tech_pvt;
	switch_channel_t *channel = NULL;
	char from_uri[512] = "", port[7] = "", *buf = NULL, tmp[512] = "";
	osip_message_t *invite = NULL;

	channel = switch_core_session_get_channel(session);
	assert(channel != NULL);

	tech_pvt = switch_core_session_get_private(session);
	assert(tech_pvt != NULL);

	tech_pvt->read_frame.buflen = SWITCH_RTP_MAX_BUF_LEN;

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "EXOSIP INIT\n");

	if (switch_test_flag(tech_pvt, TFLAG_OUTBOUND)) {
		char *dest_uri;
		char *ip, *err;
		switch_port_t sdp_port;
		char dbuf[256];

		/* do SIP Goodies... */

		/* Decide on local IP and rtp port */
		tech_pvt->local_sdp_audio_port = switch_rtp_request_port();
		sdp_port = tech_pvt->local_sdp_audio_port;
		/* Generate callerid URI */


		if (!strcasecmp(globals.rtpip, "guess")) {
			eXosip_guess_localip(AF_INET, tech_pvt->local_sdp_audio_ip, sizeof(tech_pvt->local_sdp_audio_ip));
		} else {
			switch_copy_string(tech_pvt->local_sdp_audio_ip, globals.rtpip, sizeof(tech_pvt->local_sdp_audio_ip));
		}

		ip = tech_pvt->local_sdp_audio_ip;

		if (globals.extrtpip) {
			if (!strncasecmp(globals.extrtpip, "stun:", 5)) {
				char *stun_ip = globals.extrtpip + 5;

				if (!stun_ip) {
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Stun Failed! NO STUN SERVER\n");
					switch_channel_hangup(channel, SWITCH_CAUSE_DESTINATION_OUT_OF_ORDER);
					return SWITCH_STATUS_FALSE;
				}
				if (switch_stun_lookup(&ip,
									   &sdp_port,
									   stun_ip,
									   SWITCH_STUN_DEFAULT_PORT,
									   &err,
									   switch_core_session_get_pool(session)) != SWITCH_STATUS_SUCCESS) {
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Stun Failed! %s:%d [%s]\n", stun_ip, SWITCH_STUN_DEFAULT_PORT, err);
					switch_channel_hangup(channel, SWITCH_CAUSE_DESTINATION_OUT_OF_ORDER);
					return SWITCH_STATUS_FALSE;
				}
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "Stun Success [%s]:[%d]\n", ip, sdp_port);
			} else {
				ip = globals.extrtpip;
			}
		}
		snprintf(from_uri, sizeof(from_uri), "%s <sip:%s@%s>", 
				 tech_pvt->caller_profile->caller_id_name,
				 tech_pvt->caller_profile->caller_id_number, 
				 ip);

		/* Setup codec negotiation stuffs */
		osip_rfc3264_init(&tech_pvt->sdp_config);
		

		/* Initialize SDP */
		sdp_message_init(&tech_pvt->local_sdp);
		sdp_message_v_version_set(tech_pvt->local_sdp, "0");
		sdp_message_o_origin_set(tech_pvt->local_sdp, "FreeSWITCH", "0", "0", "IN", "IP4", ip);
		sdp_message_s_name_set(tech_pvt->local_sdp, "SIP Call");
		sdp_message_c_connection_add(tech_pvt->local_sdp, -1, "IN", "IP4", ip, NULL, NULL);
		sdp_message_t_time_descr_add(tech_pvt->local_sdp, "0", "0");
		snprintf(port, sizeof(port), "%i", sdp_port);
		sdp_message_m_media_add(tech_pvt->local_sdp, "audio", port, NULL, "RTP/AVP");
		/* Add in every codec we support on this outbound call */
		tech_set_codecs(tech_pvt);



		
		if (tech_pvt->num_codecs > 0) {
			int i, lastcode = -1;

			static const switch_codec_implementation_t *imp;

			for (i = 0; i < tech_pvt->num_codecs; i++) {
				imp = tech_pvt->codecs[i];

				if (imp) {
					uint32_t sps = imp->samples_per_second;

					if (lastcode != imp->ianacode) {
						snprintf(tmp, sizeof(tmp), "%u", imp->ianacode);
						sdp_message_m_payload_add(tech_pvt->local_sdp, 0, osip_strdup(tmp));
						lastcode = imp->ianacode;
					}

					/* Add to SDP config */
					sdp_add_codec(tech_pvt->sdp_config, tech_pvt->codecs[i]->codec_type, imp->ianacode, imp->iananame, sps, 0);

					/* Add to SDP message */
					snprintf(tmp, sizeof(tmp), "%u %s/%d", imp->ianacode, imp->iananame, sps);
					sdp_message_a_attribute_add(tech_pvt->local_sdp, 0, "rtpmap", osip_strdup(tmp));
					memset(tmp, 0, sizeof(tmp));
				} 
			}
		}


		sprintf(dbuf, "%u", tech_pvt->te);
		sdp_message_m_payload_add(tech_pvt->local_sdp, 0, osip_strdup(dbuf));
		sdp_add_codec(tech_pvt->sdp_config, SWITCH_CODEC_TYPE_AUDIO, tech_pvt->te, "telephone-event", 8000, 0);
		sprintf(dbuf, "%u telephone-event/8000", tech_pvt->te);
		sdp_message_a_attribute_add(tech_pvt->local_sdp, 0, "rtpmap", osip_strdup(dbuf));
		sprintf(dbuf, "%u 0-15", tech_pvt->te);
		sdp_message_a_attribute_add(tech_pvt->local_sdp, 0, "fmtp", osip_strdup(dbuf));

		/* Setup our INVITE */
		eXosip_lock();
		if ((dest_uri =
			 (char *) switch_core_session_alloc(session, strlen(tech_pvt->caller_profile->destination_number) + 10)) == 0) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "AIEEEE!\n");
			assert(dest_uri != NULL);
		}
		sprintf(dest_uri, "sip:%s", tech_pvt->caller_profile->destination_number);
		eXosip_call_build_initial_invite(&invite, dest_uri, from_uri, NULL, NULL);
		osip_message_set_supported(invite, "100rel, replaces");
		/* Add SDP to the INVITE */

		sdp_message_to_str(tech_pvt->local_sdp, &buf);
		osip_message_set_body(invite, buf, strlen(buf));
		osip_message_set_content_type(invite, "application/sdp");
		
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "OUTBOUND SDP:\n%s\n", buf);
		free(buf);
		/* Send the INVITE */

		if (tech_pvt->realm) {
			osip_message_set_header(invite, "SrtpRealm", tech_pvt->realm);
		}
		tech_pvt->cid = eXosip_call_send_initial_invite(invite);
		snprintf(tech_pvt->call_id, sizeof(tech_pvt->call_id), "%d", tech_pvt->cid);
		switch_core_hash_insert(globals.call_hash, tech_pvt->call_id, tech_pvt);
		tech_pvt->did = -1;
		eXosip_unlock();
	}

	/* Let Media Work */
	switch_set_flag_locked(tech_pvt, TFLAG_IO);

	/* Move Channel's State Machine to RING */
	switch_channel_set_state(channel, CS_RING);
	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t exosip_on_ring(switch_core_session_t *session)
{
	switch_channel_t *channel = NULL;
	struct private_object *tech_pvt = NULL;

	channel = switch_core_session_get_channel(session);
	assert(channel != NULL);

	tech_pvt = switch_core_session_get_private(session);
	assert(tech_pvt != NULL);

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "EXOSIP RING\n");

	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t exosip_on_hangup(switch_core_session_t *session)
{
	struct private_object *tech_pvt;
	switch_channel_t *channel = NULL;
	int i;

	channel = switch_core_session_get_channel(session);
	assert(channel != NULL);

	tech_pvt = switch_core_session_get_private(session);
	assert(tech_pvt != NULL);

	deactivate_rtp(tech_pvt);
	eXosip_lock();
	switch_core_hash_delete(globals.call_hash, tech_pvt->call_id);
	switch_set_flag_locked(tech_pvt, TFLAG_BYE);
	switch_clear_flag_locked(tech_pvt, TFLAG_IO);
	i = eXosip_call_terminate(tech_pvt->cid, tech_pvt->did);
	eXosip_unlock();

	if (switch_test_flag(tech_pvt, TFLAG_USING_CODEC)) {
		switch_core_codec_destroy(&tech_pvt->read_codec);
		switch_core_codec_destroy(&tech_pvt->write_codec);
	}

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "EXOSIP HANGUP %s %d/%d=%d\n", switch_channel_get_name(channel), tech_pvt->cid, tech_pvt->did, i);
	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t exosip_on_loopback(switch_core_session_t *session)
{
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "EXOSIP LOOPBACK\n");
	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t exosip_on_transmit(switch_core_session_t *session)
{
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "EXOSIP TRANSMIT\n");
	return SWITCH_STATUS_SUCCESS;
}


static void deactivate_rtp(struct private_object *tech_pvt)
{
	int loops = 0;//, sock = -1;

	if (switch_rtp_ready(tech_pvt->rtp_session)) {
		while (loops < 10 && (switch_test_flag(tech_pvt, TFLAG_READING) || switch_test_flag(tech_pvt, TFLAG_WRITING))) {
			switch_yield(10000);
			loops++;
		}
		/*
		if ((sock = switch_rtp_get_rtp_socket(tech_pvt->rtp_session)) > -1) {
			close(sock);
		}
		*/
		switch_rtp_destroy(&tech_pvt->rtp_session);
		tech_pvt->rtp_session = NULL;
	}
}

static switch_status_t activate_rtp(struct private_object *tech_pvt)
{
	int bw, ms;
	switch_channel_t *channel;
	const char *err;
	char *key = NULL;
	switch_rtp_flag_t flags;

	assert(tech_pvt != NULL);

	channel = switch_core_session_get_channel(tech_pvt->session);
	assert(channel != NULL);


	if (tech_pvt->rtp_session && !switch_test_flag(tech_pvt, TFLAG_REINVITE)) {
		return SWITCH_STATUS_SUCCESS;
	}



	if (switch_test_flag(tech_pvt, TFLAG_USING_CODEC)) {
		bw = tech_pvt->read_codec.implementation->bits_per_second;
		ms = tech_pvt->read_codec.implementation->microseconds_per_frame;
	} else {
		switch_channel_get_raw_mode(channel, NULL, NULL, NULL, &ms, &bw);
		bw *= 8;
	}

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "Activating RTP %s:%d->%s:%d codec: %u ms: %d\n",
					  tech_pvt->local_sdp_audio_ip,
					  tech_pvt->local_sdp_audio_port,
					  tech_pvt->remote_sdp_audio_ip,
					  tech_pvt->remote_sdp_audio_port, tech_pvt->read_codec.implementation->ianacode, ms);


	if (tech_pvt->realm) {
		if (!(key = (char *) switch_core_hash_find(globals.srtp_hash, tech_pvt->realm))) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "undefined Realm %s\n", tech_pvt->realm);
			switch_channel_hangup(channel, SWITCH_CAUSE_DESTINATION_OUT_OF_ORDER);
			switch_set_flag_locked(tech_pvt, TFLAG_BYE);
			switch_clear_flag_locked(tech_pvt, TFLAG_IO);
			return SWITCH_STATUS_FALSE;
		} else {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "using Realm %s\n", tech_pvt->realm);
		}
	}
	flags = SWITCH_RTP_FLAG_MINI | SWITCH_RTP_FLAG_RAW_WRITE;
	if (switch_test_flag(tech_pvt, TFLAG_TIMER)) {
		flags |= SWITCH_RTP_FLAG_USE_TIMER;
	}

	if (switch_test_flag(tech_pvt, TFLAG_AA)) {
		flags |= SWITCH_RTP_FLAG_AUTOADJ;
	}


	if (tech_pvt->rtp_session && switch_test_flag(tech_pvt, TFLAG_REINVITE)) {
		switch_clear_flag_locked(tech_pvt, TFLAG_REINVITE);
		
		if (switch_rtp_set_remote_address(tech_pvt->rtp_session,
										  tech_pvt->remote_sdp_audio_ip,
										  tech_pvt->remote_sdp_audio_port,
										  &err) != SWITCH_STATUS_SUCCESS) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "RTP REPORTS ERROR: [%s]\n", err);
		} else {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "RTP CHANGING DEST TO: [%s:%d]\n", 
							  tech_pvt->remote_sdp_audio_ip, tech_pvt->remote_sdp_audio_port);
		}
		return SWITCH_STATUS_SUCCESS;
	}

	tech_pvt->rtp_session = switch_rtp_new(tech_pvt->local_sdp_audio_ip,
										   tech_pvt->local_sdp_audio_port,
										   tech_pvt->remote_sdp_audio_ip,
										   tech_pvt->remote_sdp_audio_port,
										   tech_pvt->read_codec.implementation->ianacode,
										   0, //tech_pvt->read_codec.implementation->encoded_bytes_per_frame,
										   ms,
										   flags,
										   key,
										   &err, switch_core_session_get_pool(tech_pvt->session));

	if (switch_rtp_ready(tech_pvt->rtp_session)) {
		uint8_t vad_in = switch_test_flag(tech_pvt, TFLAG_VAD_IN) ? 1 : 0;
		uint8_t vad_out = switch_test_flag(tech_pvt, TFLAG_VAD_OUT) ? 1 : 0;
		uint8_t inb = switch_test_flag(tech_pvt, TFLAG_OUTBOUND) ? 0 : 1;

		tech_pvt->ssrc = switch_rtp_get_ssrc(tech_pvt->rtp_session);
		switch_set_flag_locked(tech_pvt, TFLAG_RTP);

		if (tech_pvt->te > 96) {
			switch_rtp_set_telephony_event(tech_pvt->rtp_session, tech_pvt->te);
		}

		if ((vad_in && inb) || (vad_out && !inb)) {
			switch_rtp_enable_vad(tech_pvt->rtp_session, tech_pvt->session, &tech_pvt->read_codec, SWITCH_VAD_FLAG_TALKING);
			switch_set_flag_locked(tech_pvt, TFLAG_VAD);
		}
	} else {
		switch_channel_t *channel = switch_core_session_get_channel(tech_pvt->session);
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "RTP REPORTS ERROR: [%s][%s:%d]\n", err, 
						  tech_pvt->local_sdp_audio_ip,
						  tech_pvt->local_sdp_audio_port);
		switch_channel_hangup(channel, SWITCH_CAUSE_DESTINATION_OUT_OF_ORDER);
		switch_set_flag_locked(tech_pvt, TFLAG_BYE);
		switch_clear_flag_locked(tech_pvt, TFLAG_IO);
		return SWITCH_STATUS_FALSE;
	}

	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t exosip_answer_channel(switch_core_session_t *session)
{
	struct private_object *tech_pvt;
	switch_channel_t *channel = NULL;

	assert(session != NULL);

	channel = switch_core_session_get_channel(session);
	assert(channel != NULL);

	tech_pvt = switch_core_session_get_private(session);
	assert(tech_pvt != NULL);

	if (!switch_test_flag(tech_pvt, TFLAG_ANS) && !switch_channel_test_flag(channel, CF_OUTBOUND) ) {
		char *buf = NULL;
		osip_message_t *answer = NULL;
		char *sdp_str;
		
		/* Transmit 200 OK with SDP */
		eXosip_lock();
		eXosip_call_build_answer(tech_pvt->tid, 200, &answer);
		sdp_message_to_str(tech_pvt->local_sdp, &buf);
		osip_message_set_body(answer, buf, strlen(buf));
		osip_message_set_content_type(answer, "application/sdp");
		free(buf);
		eXosip_call_send_answer(tech_pvt->tid, 200, answer);
		eXosip_unlock();
		switch_set_flag_locked(tech_pvt, TFLAG_ANS);

		sdp_message_to_str(tech_pvt->local_sdp, &sdp_str);
		if (sdp_str) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Answer SDP:\n%s", sdp_str);
			free(sdp_str);
		}

	}


	return SWITCH_STATUS_SUCCESS;
}


static switch_status_t exosip_read_frame(switch_core_session_t *session, switch_frame_t **frame, int timeout,
									   switch_io_flag_t flags, int stream_id)
{
	struct private_object *tech_pvt = NULL;
	size_t bytes = 0, samples = 0, frames = 0, ms = 0;
	switch_channel_t *channel = NULL;
	int payload = 0;
	switch_time_t now, started = switch_time_now(), last_act = switch_time_now();
	unsigned int elapsed;
	uint32_t hard_timeout = 60000 * 3;

	channel = switch_core_session_get_channel(session);
	assert(channel != NULL);

	tech_pvt = switch_core_session_get_private(session);
	assert(tech_pvt != NULL);

	tech_pvt->read_frame.datalen = 0;
	switch_set_flag_locked(tech_pvt, TFLAG_READING);

	if (switch_test_flag(tech_pvt, TFLAG_USING_CODEC)) {
		bytes = tech_pvt->read_codec.implementation->encoded_bytes_per_frame;
		samples = tech_pvt->read_codec.implementation->samples_per_frame;
		ms = tech_pvt->read_codec.implementation->microseconds_per_frame;
	} else {
		assert(0);
	}
	
	if (tech_pvt->last_read) {
		elapsed = (unsigned int)((switch_time_now() - tech_pvt->last_read) / 1000);
		if (elapsed > 60000) {
			return SWITCH_STATUS_TIMEOUT;
		}
	}

	if (switch_test_flag(tech_pvt, TFLAG_IO)) {
		switch_status_t status;

		if (!switch_test_flag(tech_pvt, TFLAG_RTP)) {
			return SWITCH_STATUS_GENERR;
		}

		assert(tech_pvt->rtp_session != NULL);
		tech_pvt->read_frame.datalen = 0;


		while (!switch_test_flag(tech_pvt, TFLAG_BYE) && switch_test_flag(tech_pvt, TFLAG_IO) && tech_pvt->read_frame.datalen == 0) {
			now = switch_time_now();
			tech_pvt->read_frame.flags = 0;

			status = switch_rtp_zerocopy_read_frame(tech_pvt->rtp_session, &tech_pvt->read_frame);
			if (status != SWITCH_STATUS_SUCCESS && status != SWITCH_STATUS_BREAK) {
				return SWITCH_STATUS_FALSE;
			}
			
			payload = tech_pvt->read_frame.payload;


			elapsed = (unsigned int)((switch_time_now() - started) / 1000);

			if (timeout > -1) {
				if (elapsed >= (unsigned int)timeout) {
					return SWITCH_STATUS_BREAK;
				}
			}
			
			elapsed = (unsigned int)((switch_time_now() - last_act) / 1000);
			if (elapsed >= hard_timeout) {
				return SWITCH_STATUS_BREAK;
			}

			if (switch_rtp_has_dtmf(tech_pvt->rtp_session)) {
				char dtmf[128];
				switch_rtp_dequeue_dtmf(tech_pvt->rtp_session, dtmf, sizeof(dtmf));
				switch_channel_queue_dtmf(channel, dtmf);
			}


			if (tech_pvt->read_frame.datalen > 0) {
				tech_pvt->last_read = switch_time_now();
				if (tech_pvt->read_codec.implementation->encoded_bytes_per_frame) {
					bytes = tech_pvt->read_codec.implementation->encoded_bytes_per_frame;
					frames = (tech_pvt->read_frame.datalen / bytes);
				} else {
					frames = 1;
				}
				samples = frames * tech_pvt->read_codec.implementation->samples_per_frame;
				ms = frames * tech_pvt->read_codec.implementation->microseconds_per_frame;
				tech_pvt->timestamp_recv += (int32_t) samples;
				tech_pvt->read_frame.samples = (int) samples;
				break;
			}

			switch_yield(1000);
		}

	}


	switch_clear_flag_locked(tech_pvt, TFLAG_READING);

	if (switch_test_flag(tech_pvt, TFLAG_BYE)) {
		switch_channel_hangup(channel, SWITCH_CAUSE_NORMAL_CLEARING);
		return SWITCH_STATUS_FALSE;
	}

	*frame = &tech_pvt->read_frame;

	return SWITCH_STATUS_SUCCESS;
}


static switch_status_t exosip_write_frame(switch_core_session_t *session, switch_frame_t *frame, int timeout,
										switch_io_flag_t flags, int stream_id)
{
	struct private_object *tech_pvt;
	switch_channel_t *channel = NULL;
	switch_status_t status = SWITCH_STATUS_SUCCESS;
	int bytes = 0, samples = 0, frames = 0;

	channel = switch_core_session_get_channel(session);
	assert(channel != NULL);

	tech_pvt = switch_core_session_get_private(session);
	assert(tech_pvt != NULL);

	if (!switch_test_flag(tech_pvt, TFLAG_RTP)) {
		return SWITCH_STATUS_GENERR;
	}

	if (!switch_test_flag(tech_pvt, TFLAG_IO)) {
		return SWITCH_STATUS_SUCCESS;
	}

	if (switch_test_flag(tech_pvt, TFLAG_BYE)) {
		switch_channel_hangup(channel, SWITCH_CAUSE_NORMAL_CLEARING);
		return SWITCH_STATUS_FALSE;
	}

	switch_set_flag_locked(tech_pvt, TFLAG_WRITING);

	if (switch_test_flag(tech_pvt, TFLAG_USING_CODEC)) {
		if (tech_pvt->read_codec.implementation->encoded_bytes_per_frame) {
			bytes = tech_pvt->read_codec.implementation->encoded_bytes_per_frame;
			frames = ((int) frame->datalen / bytes);
		} else {
			frames = 1;
		}
		samples = frames * tech_pvt->read_codec.implementation->samples_per_frame;
	} else {
		assert(0);
	}



	//printf("%s %s->%s send %d bytes %d samples in %d frames ts=%d\n", switch_channel_get_name(channel), tech_pvt->local_sdp_audio_ip, tech_pvt->remote_sdp_audio_ip, frame->datalen, samples, frames, tech_pvt->timestamp_send);

	switch_rtp_write_frame(tech_pvt->rtp_session, frame, samples);
	
	tech_pvt->timestamp_send += (int) samples;

	switch_clear_flag_locked(tech_pvt, TFLAG_WRITING);
	return status;
}



static switch_status_t exosip_kill_channel(switch_core_session_t *session, int sig)
{
	struct private_object *tech_pvt;
	switch_channel_t *channel = NULL;

	channel = switch_core_session_get_channel(session);
	assert(channel != NULL);

	tech_pvt = switch_core_session_get_private(session);
	assert(tech_pvt != NULL);

	switch_clear_flag_locked(tech_pvt, TFLAG_IO);
	switch_set_flag_locked(tech_pvt, TFLAG_BYE);

	if (switch_rtp_ready(tech_pvt->rtp_session)) {
		switch_rtp_kill_socket(tech_pvt->rtp_session);
	}

	return SWITCH_STATUS_SUCCESS;

}

static switch_status_t exosip_waitfor_read(switch_core_session_t *session, int ms, int stream_id)
{
	struct private_object *tech_pvt;
	switch_channel_t *channel = NULL;

	channel = switch_core_session_get_channel(session);
	assert(channel != NULL);

	tech_pvt = switch_core_session_get_private(session);
	assert(tech_pvt != NULL);

	return SWITCH_STATUS_SUCCESS;
}


static switch_status_t exosip_waitfor_write(switch_core_session_t *session, int ms, int stream_id)
{
	struct private_object *tech_pvt;
	switch_channel_t *channel = NULL;

	channel = switch_core_session_get_channel(session);
	assert(channel != NULL);

	tech_pvt = switch_core_session_get_private(session);
	assert(tech_pvt != NULL);

	return SWITCH_STATUS_SUCCESS;

}

static switch_status_t exosip_send_dtmf(switch_core_session_t *session, char *digits)
{
	struct private_object *tech_pvt;

	tech_pvt = switch_core_session_get_private(session);
    assert(tech_pvt != NULL);

	return switch_rtp_queue_rfc2833(tech_pvt->rtp_session,
									digits,
									globals.dtmf_duration * (tech_pvt->read_codec.implementation->samples_per_second / 1000));
	
}

static switch_status_t exosip_receive_message(switch_core_session_t *session, switch_core_session_message_t *msg)
{
	switch_channel_t *channel;
	struct private_object *tech_pvt;
			
	channel = switch_core_session_get_channel(session);
	assert(channel != NULL);
			
	tech_pvt = switch_core_session_get_private(session);
	assert(tech_pvt != NULL);


	switch (msg->message_id) {
	case SWITCH_MESSAGE_INDICATE_BRIDGE:
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
	case SWITCH_MESSAGE_INDICATE_PROGRESS:
	  if (msg) {
	    struct private_object *tech_pvt;
	    switch_channel_t *channel = NULL;
		  
	    channel = switch_core_session_get_channel(session);
	    assert(channel != NULL);

	    tech_pvt = switch_core_session_get_private(session);
	    assert(tech_pvt != NULL);

	    if (!switch_test_flag(tech_pvt, TFLAG_EARLY_MEDIA)) {
	      char *buf = NULL;
	      osip_message_t *progress = NULL;

	      switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "Asked to send early media by %s\n", msg->from);

	      /* Transmit 183 Progress with SDP */
	      eXosip_lock();
	      eXosip_call_build_answer(tech_pvt->tid, 183, &progress);
	      if (progress) {
			  char *sdp_str;
			  sdp_message_to_str(tech_pvt->local_sdp, &buf);
			  osip_message_set_body(progress, buf, strlen(buf));
			  osip_message_set_content_type(progress, "application/sdp");
			  free(buf);
			  eXosip_call_send_answer(tech_pvt->tid, 183, progress);
			  switch_set_flag_locked(tech_pvt, TFLAG_EARLY_MEDIA);
			  switch_channel_set_flag(channel, CF_EARLY_MEDIA);
			  sdp_message_to_str(tech_pvt->local_sdp, &sdp_str);
			  if (sdp_str) {
				  switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Progress SDP:\n%s", sdp_str);
				  free(sdp_str);
			  }
	      }
	      eXosip_unlock();
	    }
	  }
		
	  break;
	default:
		break;
	}

	return SWITCH_STATUS_SUCCESS;
}

static const switch_io_routines_t exosip_io_routines = {
	/*.outgoing_channel */ exosip_outgoing_channel,
	/*.answer_channel */ exosip_answer_channel,
	/*.read_frame */ exosip_read_frame,
	/*.write_frame */ exosip_write_frame,
	/*.kill_channel */ exosip_kill_channel,
	/*.waitfor_read */ exosip_waitfor_read,
	/*.waitfor_read */ exosip_waitfor_write,
	/*.send_dtmf*/ exosip_send_dtmf,
	/*.receive_message*/ exosip_receive_message
};

static const switch_state_handler_table_t exosip_event_handlers = {
	/*.on_init */ exosip_on_init,
	/*.on_ring */ exosip_on_ring,
	/*.on_execute */ exosip_on_execute,
	/*.on_hangup */ exosip_on_hangup,
	/*.on_loopback */ exosip_on_loopback,
	/*.on_transmit */ exosip_on_transmit
};

static const switch_endpoint_interface_t exosip_endpoint_interface = {
	/*.interface_name */ "exosip",
	/*.io_routines */ &exosip_io_routines,
	/*.event_handlers */ &exosip_event_handlers,
	/*.private */ NULL,
	/*.next */ NULL
};

static const switch_loadable_module_interface_t exosip_module_interface = {
	/*.module_name */ modname,
	/*.endpoint_interface */ &exosip_endpoint_interface,
	/*.timer_interface */ NULL,
	/*.dialplan_interface */ NULL,
	/*.codec_interface */ NULL,
	/*.application_interface */ NULL
};


struct callback_t {
	char *val;
	switch_size_t len;
	int matches;
};

static int find_callback(void *pArg, int argc, char **argv, char **columnNames){
	struct callback_t *cbt = (struct callback_t *) pArg;

	switch_copy_string(cbt->val, argv[0], cbt->len);
	cbt->matches++;
	return 0;
}

static int del_callback(void *pArg, int argc, char **argv, char **columnNames){
	switch_event_t *s_event;

	if (argc >=3 ) {
		if (switch_event_create_subclass(&s_event, SWITCH_EVENT_CUSTOM, MY_EVENT_EXPIRE) == SWITCH_STATUS_SUCCESS) {
			switch_event_add_header(s_event, SWITCH_STACK_BOTTOM, "key", "%s", argv[0]);
			switch_event_add_header(s_event, SWITCH_STACK_BOTTOM, "url", "%s", argv[1]);
			switch_event_add_header(s_event, SWITCH_STACK_BOTTOM, "expires", "%d", argv[2]);
			switch_event_fire(&s_event);
		}
	}
	return 0;
}


static char *find_reg_url(switch_core_db_t *db, char *key, char *val, switch_size_t len)
{
	char *errmsg;
	switch_core_db_t *udb = NULL;
	struct callback_t cbt = {0};
	char buf[1024];
	char *host = NULL;

	if (db) {
		udb = db;
	} else {
		udb = switch_core_db_open_file(DBFILE);
	}

	switch_copy_string(buf, key, sizeof(buf));
	key = buf;
	if ((host = strchr(key, '%'))) {
		*host++ = '\0';
	}

	cbt.val = val;
	cbt.len = len;
	switch_mutex_lock(globals.reg_mutex);
	if (host) {
		snprintf(val, len, "select url from sip_registrations where key='%s' and host='%s'", key, host);	
	} else {
		snprintf(val, len, "select url from sip_registrations where key='%s'", key);	
	}

	switch_core_db_exec(udb, val, find_callback, &cbt, &errmsg);

	if (errmsg) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "SQL ERR [%s][%s]\n", val, errmsg);
		switch_core_db_free(errmsg);
		errmsg = NULL;
	}

	if (!db) {
		switch_core_db_close(udb);
	}
	switch_mutex_unlock(globals.reg_mutex);
	return cbt.matches ? val : NULL;
}

static switch_status_t exosip_outgoing_channel(switch_core_session_t *session, switch_caller_profile_t *outbound_profile,
											 switch_core_session_t **new_session, switch_memory_pool_t *pool)
{

	if (!outbound_profile->destination_number) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Invalid Destination!\n");
		return SWITCH_STATUS_GENERR;
	}


	if ((*new_session = switch_core_session_request(&exosip_endpoint_interface, pool)) != 0) {
		struct private_object *tech_pvt;
		switch_channel_t *channel;


		switch_core_session_add_stream(*new_session, NULL);
		if ((tech_pvt =
			 (struct private_object *) switch_core_session_alloc(*new_session, sizeof(struct private_object))) != 0) {
			memset(tech_pvt, 0, sizeof(*tech_pvt));
			tech_pvt->flags = globals.flags;
			channel = switch_core_session_get_channel(*new_session);
			switch_core_session_set_private(*new_session, tech_pvt);
			tech_pvt->session = *new_session;
			tech_pvt->te = globals.te;
			switch_mutex_init(&tech_pvt->flag_mutex, SWITCH_MUTEX_NESTED, switch_core_session_get_pool(*new_session));
		} else {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Hey where is my memory pool?\n");
			switch_core_session_destroy(new_session);
			return SWITCH_STATUS_GENERR;
		}

		if (outbound_profile) {
			char name[128];
			switch_caller_profile_t *caller_profile = NULL;
			char tmp[1024];
			char *url;

			if (*outbound_profile->destination_number == '!') {
				char *p;
				
				outbound_profile->destination_number++;

				if ((p=strchr(outbound_profile->destination_number, '!'))) {
					*p = '\0';
					p++;
					tech_pvt->realm = switch_core_session_strdup(*new_session, outbound_profile->destination_number);
					outbound_profile->destination_number = p;
					if ((p = strchr(tech_pvt->realm, '!'))) {
						*p = '\0';
					}
				}

			}
			snprintf(name, sizeof(name), "Exosip/%s-%04x", outbound_profile->destination_number, rand() & 0xffff);
			switch_channel_set_name(channel, name);

			caller_profile = switch_caller_profile_clone(*new_session, outbound_profile);


			if (!strchr(caller_profile->destination_number, '@') && (url = find_reg_url(NULL, caller_profile->destination_number, tmp, sizeof(tmp)))) {
				caller_profile->rdnis = switch_core_session_strdup(*new_session, caller_profile->destination_number);
				caller_profile->destination_number = switch_core_session_strdup(*new_session, url);
			}


			switch_channel_set_caller_profile(channel, caller_profile);
			tech_pvt->caller_profile = caller_profile;
		} else {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Doh! no caller profile\n");
			switch_core_session_destroy(new_session);
			return SWITCH_STATUS_GENERR;
		}

		switch_channel_set_flag(channel, CF_OUTBOUND);
		switch_set_flag_locked(tech_pvt, TFLAG_OUTBOUND);
		switch_channel_set_state(channel, CS_INIT);
		return SWITCH_STATUS_SUCCESS;
	}

	return SWITCH_STATUS_GENERR;
}


SWITCH_MOD_DECLARE(switch_status_t) switch_module_shutdown(void)
{


	if (globals.running) {
		globals.running = -1;
		while (globals.running) {
			switch_yield(100000);
		}
	}
	switch_core_db_close(globals.db);
	return SWITCH_STATUS_SUCCESS;
}

SWITCH_MOD_DECLARE(switch_status_t) switch_module_load(const switch_loadable_module_interface_t **module_interface, char *filename)
{

	/* NOTE:  **interface is **_interface because the common lib redefines interface to struct in some situations */

	if ((globals.db = switch_core_db_open_file(DBFILE))) {
		switch_core_db_test_reactive(globals.db, "select * from sip_registrations", create_interfaces_sql);
	} else {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Cannot Open SQL Database!\n");
		return SWITCH_STATUS_TERM;
	}

	
	if (switch_core_new_memory_pool(&module_pool) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "OH OH no pool\n");
		return SWITCH_STATUS_TERM;
	}

	switch_core_hash_init(&globals.call_hash, module_pool);
	switch_core_hash_init(&globals.srtp_hash, module_pool);
	switch_mutex_init(&globals.reg_mutex, SWITCH_MUTEX_NESTED, module_pool);


	/* connect my internal structure to the blank pointer passed to me */
	*module_interface = &exosip_module_interface;

	/* indicate that the module should continue to be loaded */
	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t exosip_create_call(eXosip_event_t * event)
{
	switch_core_session_t *session;
	sdp_message_t *remote_sdp = NULL;
	sdp_connection_t *conn = NULL;
	sdp_media_t *remote_med = NULL, *audio_tab[10], *video_tab[10], *t38_tab[10], *app_tab[10];
	char local_sdp_str[1024] = "", port[8] = "";
	int mline = 0, pos = 0;
	switch_channel_t *channel = NULL;
	char name[128];
	char *dpayload = NULL, *dname = NULL, *drate = NULL;
	char *remote_sdp_str = NULL;
	char dbuf[256];

	if ((session = switch_core_session_request(&exosip_endpoint_interface, NULL)) != 0) {
		struct private_object *tech_pvt;
		switch_port_t sdp_port;
		char *ip, *err;
		osip_uri_t *uri;
		osip_from_t *from;
		char *displayname, *username;
		osip_header_t *tedious;
		char *val;
		const switch_codec_implementation_t *imp = NULL;

		switch_core_session_add_stream(session, NULL);
		if ((tech_pvt = (struct private_object *) switch_core_session_alloc(session, sizeof(struct private_object))) != 0) {
			memset(tech_pvt, 0, sizeof(*tech_pvt));
			tech_pvt->flags = globals.flags;
			channel = switch_core_session_get_channel(session);
			switch_core_session_set_private(session, tech_pvt);
			tech_pvt->session = session;
			tech_pvt->te = globals.te;
			switch_mutex_init(&tech_pvt->flag_mutex, SWITCH_MUTEX_NESTED, switch_core_session_get_pool(session));
		} else {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Hey where is my memory pool?\n");
			switch_core_session_destroy(&session);
			return SWITCH_STATUS_MEMERR;
		}

		snprintf(name, sizeof(name), "Exosip/%s-%04x", event->request->from->url->username, rand() & 0xffff);
		switch_channel_set_name(channel, name);
		switch_channel_set_variable(channel, "endpoint_disposition", "INVITE");

		if (osip_message_header_get_byname (event->request, "SrtpRealm", 0, &tedious)) {
			tech_pvt->realm = switch_core_session_strdup(session, osip_header_get_value(tedious));
		}

		if (!(from = osip_message_get_from(event->request))) {
			switch_core_session_destroy(&session);
            return SWITCH_STATUS_MEMERR;
		}
		if (!(val = osip_from_get_displayname(from))) {
			val = event->request->from->url->username;
			if (!val) {
				val = "FreeSWITCH";
			}
		}
		
		displayname = switch_core_session_strdup(session, val);

		if (*displayname == '"') {
			char *p;

			displayname++;
			if ((p = strchr(displayname, '"'))) {
				*p = '\0';
			}
		}

		if (!(uri = osip_from_get_url(from))) {
			username = displayname;
		} else {
			username = osip_uri_get_username(uri);
		}

		if ((tech_pvt->caller_profile = switch_caller_profile_new(switch_core_session_get_pool(session),
																  username,
																  globals.dialplan,
																  displayname,
																  username,
																  event->request->from->url->host,
																  NULL,
																  NULL,
																  NULL,
																  (char *)modname,
																  NULL,
																  event->request->req_uri->username)) != 0) {
			switch_channel_set_caller_profile(channel, tech_pvt->caller_profile);
		}

		switch_set_flag_locked(tech_pvt, TFLAG_INBOUND);
		tech_pvt->did = event->did;
		tech_pvt->cid = event->cid;
		tech_pvt->tid = event->tid;

		

		if ((remote_sdp = eXosip_get_sdp_info(event->request)) == 0) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Cannot Find Remote SDP!\n");
			exosip_on_hangup(session);
			switch_core_session_destroy(&session);
			return SWITCH_STATUS_GENERR;
		}

		if (!strcasecmp(globals.rtpip, "guess")) {
			eXosip_guess_localip(AF_INET, tech_pvt->local_sdp_audio_ip, sizeof(tech_pvt->local_sdp_audio_ip));
		} else {
			switch_copy_string(tech_pvt->local_sdp_audio_ip, globals.rtpip, sizeof(tech_pvt->local_sdp_audio_ip));
		}
		ip = tech_pvt->local_sdp_audio_ip;

		tech_pvt->local_sdp_audio_port = switch_rtp_request_port();
		sdp_port = tech_pvt->local_sdp_audio_port;


		if (globals.extrtpip) {
			if (!strncasecmp(globals.extrtpip, "stun:", 5)) {
				char *stun_ip = globals.extrtpip + 5;
				if (!stun_ip) {
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Stun Failed! NO STUN SERVER\n");
					switch_channel_hangup(channel, SWITCH_CAUSE_DESTINATION_OUT_OF_ORDER);
					return SWITCH_STATUS_FALSE;
				}
				if (switch_stun_lookup(&ip,
									   &sdp_port,
									   stun_ip,
									   SWITCH_STUN_DEFAULT_PORT,
									   &err,
									   switch_core_session_get_pool(session)) != SWITCH_STATUS_SUCCESS) {
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Stun Failed! %s:%d [%s]\n", stun_ip, SWITCH_STUN_DEFAULT_PORT, err);
					switch_channel_hangup(channel, SWITCH_CAUSE_DESTINATION_OUT_OF_ORDER);
					return SWITCH_STATUS_FALSE;
				}
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "Stun Success [%s]:[%d]\n", ip, sdp_port);
			} else {
				ip = globals.extrtpip;
			}
		}
		osip_rfc3264_init(&tech_pvt->sdp_config);

		/* Add in what codecs we support locally */
		tech_set_codecs(tech_pvt);

		sdp_message_init(&tech_pvt->local_sdp);



		if (tech_pvt->num_codecs > 0) {
			int i;
			static const switch_codec_implementation_t *imp = NULL;

			for (i = 0; i < tech_pvt->num_codecs; i++) {
				if ((imp = tech_pvt->codecs[i])) {
					sdp_add_codec(tech_pvt->sdp_config,
								  tech_pvt->codecs[i]->codec_type,
								  imp->ianacode,
								  imp->iananame,
								  imp->samples_per_second,
								  0);

				}
			}
		}
		

		

		osip_rfc3264_prepare_answer(tech_pvt->sdp_config, remote_sdp, local_sdp_str, sizeof(local_sdp_str));		
		
		sdp_message_parse(tech_pvt->local_sdp, local_sdp_str);

		sdp_message_to_str(remote_sdp, &remote_sdp_str);
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "REMOTE SDP:\n%s", remote_sdp_str);


		mline = 0;
		while (0 == osip_rfc3264_match(tech_pvt->sdp_config, remote_sdp, audio_tab, video_tab, t38_tab, app_tab, mline)) {
			if (audio_tab[0] == NULL && video_tab[0] == NULL && t38_tab[0] == NULL && app_tab[0] == NULL) {

				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "Got no compatible codecs!\n");
				goto done;
			}
			for (pos = 0; audio_tab[pos] != NULL; pos++) {
				osip_rfc3264_complete_answer(tech_pvt->sdp_config, remote_sdp, tech_pvt->local_sdp, audio_tab[pos],
											 mline);
				if (parse_sdp_media(tech_pvt, audio_tab[pos], &dname, &drate, &dpayload, &imp) == SWITCH_STATUS_SUCCESS) {
					tech_pvt->payload_num = atoi(dpayload);					
					goto done;
				}
			}
			mline++;
		}
	done:

		free(remote_sdp_str);


		sprintf(dbuf, "%u", tech_pvt->te);
		sdp_message_m_payload_add(tech_pvt->local_sdp, 0, osip_strdup(dbuf));
		sdp_add_codec(tech_pvt->sdp_config, SWITCH_CODEC_TYPE_AUDIO, tech_pvt->te, "telephone-event", 8000, 0);

		sprintf(dbuf, "%u telephone-event/8000", tech_pvt->te);
		sdp_message_a_attribute_add(tech_pvt->local_sdp, 0, "rtpmap", osip_strdup(dbuf));
		sprintf(dbuf, "%u 0-15", tech_pvt->te);
		sdp_message_a_attribute_add(tech_pvt->local_sdp, 0, "fmtp", osip_strdup(dbuf));


		sdp_message_o_origin_set(tech_pvt->local_sdp, "FreeSWITCH", "0", "0", "IN", "IP4", ip);
								 
		sdp_message_s_name_set(tech_pvt->local_sdp, "SIP Call");
		sdp_message_c_connection_add(tech_pvt->local_sdp, -1, "IN", "IP4", ip, NULL, NULL);
		snprintf(port, sizeof(port), "%i", sdp_port);
		sdp_message_m_port_set(tech_pvt->local_sdp, 0, osip_strdup(port));

		conn = eXosip_get_audio_connection(remote_sdp);
		remote_med = eXosip_get_audio_media(remote_sdp);
		snprintf(tech_pvt->remote_sdp_audio_ip, 50, conn->c_addr);

		tech_pvt->remote_sdp_audio_port = (switch_port_t)atoi(remote_med->m_port);

		snprintf(tech_pvt->call_id, sizeof(tech_pvt->call_id), "%d", event->cid);
		eXosip_lock();
		switch_core_hash_insert(globals.call_hash, tech_pvt->call_id, tech_pvt);
		eXosip_unlock();

		if (!dname) {
			exosip_on_hangup(session);
			switch_core_session_destroy(&session);
			return SWITCH_STATUS_GENERR;
		}

		switch_channel_set_state(channel, CS_INIT);


		{
			int rate = atoi(drate);
			int ms = 0; //globals.codec_ms;


			if (imp) {
				ms = imp->microseconds_per_frame / 1000;
			}

			if (switch_core_codec_init(&tech_pvt->read_codec,
									   dname,
									   rate,
									   ms,
									   1,
									   SWITCH_CODEC_FLAG_ENCODE | SWITCH_CODEC_FLAG_DECODE,
									   NULL, switch_core_session_get_pool(session)) != SWITCH_STATUS_SUCCESS) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Can't load codec?\n");
				switch_channel_hangup(channel, SWITCH_CAUSE_DESTINATION_OUT_OF_ORDER);
				return SWITCH_STATUS_FALSE;
			} else {
				if (switch_core_codec_init(&tech_pvt->write_codec,
										   dname,
										   rate,
										   ms,
										   1,
										   SWITCH_CODEC_FLAG_ENCODE | SWITCH_CODEC_FLAG_DECODE,
										   NULL, switch_core_session_get_pool(session)) != SWITCH_STATUS_SUCCESS) {
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Can't load codec?\n");
					switch_channel_hangup(channel, SWITCH_CAUSE_DESTINATION_OUT_OF_ORDER);
					return SWITCH_STATUS_FALSE;
				} else {
					int ms;
					tech_pvt->read_frame.rate = rate;
					switch_set_flag_locked(tech_pvt, TFLAG_USING_CODEC);
					ms = tech_pvt->write_codec.implementation->microseconds_per_frame / 1000;
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "Activate Inbound Codec %s/%d %d ms\n", dname, rate, ms);
					tech_pvt->read_frame.codec = &tech_pvt->read_codec;
					switch_core_session_set_read_codec(session, &tech_pvt->read_codec);
					switch_core_session_set_write_codec(session, &tech_pvt->write_codec);
				}
			}
		}

		switch_safe_free(dname);
		switch_safe_free(drate);
		switch_safe_free(dpayload);

		if (activate_rtp(tech_pvt) != SWITCH_STATUS_SUCCESS) {
			exosip_on_hangup(session);
			switch_core_session_destroy(&session);
            return SWITCH_STATUS_FALSE;
		}

		if (switch_test_flag(tech_pvt, TFLAG_RTP)) {
			switch_core_session_thread_launch(session);
		} else {
			exosip_on_hangup(session);
			switch_core_session_destroy(&session);
			return SWITCH_STATUS_FALSE;
		}
	} else {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Cannot Create new Inbound Channel!\n");
		return SWITCH_STATUS_FALSE;
	}


	return SWITCH_STATUS_SUCCESS;

}

static void destroy_call_by_event(eXosip_event_t *event)
{
	struct private_object *tech_pvt;
	switch_channel_t *channel = NULL;
	switch_call_cause_t cause;

	if ((tech_pvt = get_pvt_by_call_id(event->cid)) == 0) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "cannot destroy nonexistant call [%d]!\n", event->cid);
		return;
	}

	channel = switch_core_session_get_channel(tech_pvt->session);
	assert(channel != NULL);

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "destroy %s\n", switch_channel_get_name(channel));
	exosip_kill_channel(tech_pvt->session, SWITCH_SIG_KILL);


	switch (event->type) {
	case EXOSIP_CALL_RELEASED:
		switch_channel_set_variable(channel, "endpoint_disposition", "RELEASED");
		cause = SWITCH_CAUSE_NORMAL_CLEARING;
		break;
	case EXOSIP_CALL_CLOSED:
		switch_channel_set_variable(channel, "endpoint_disposition", "CLOSED");
		cause = SWITCH_CAUSE_NORMAL_CLEARING;
		break;
	case EXOSIP_CALL_NOANSWER:
		switch_channel_set_variable(channel, "endpoint_disposition", "NO ANSWER");
		cause = SWITCH_CAUSE_NO_ANSWER;
		break;
	case EXOSIP_CALL_REQUESTFAILURE:
		switch_channel_set_variable(channel, "endpoint_disposition", "REQUEST FAILURE");
		cause = SWITCH_CAUSE_REQUESTED_CHAN_UNAVAIL;
		break;
	case EXOSIP_CALL_SERVERFAILURE:
		switch_channel_set_variable(channel, "endpoint_disposition", "SERVER FAILURE");
		cause = SWITCH_CAUSE_CALL_REJECTED;
		break;
	case EXOSIP_CALL_GLOBALFAILURE:
		switch_channel_set_variable(channel, "endpoint_disposition", "GLOBAL FAILURE");
		cause = SWITCH_CAUSE_CALL_REJECTED;
		break;
	default:
		switch_channel_set_variable(channel, "endpoint_disposition", "UNKNOWN");
		cause = SWITCH_CAUSE_SWITCH_CONGESTION;
		break;
	}

	switch_channel_hangup(channel, cause);

}

static switch_status_t parse_sdp_media(struct private_object *tech_pvt,
									   sdp_media_t * media,
									   char **dname,
									   char **drate,
									   char **dpayload,
									   const switch_codec_implementation_t **impp)
{
	int pos = 0;
	sdp_attribute_t *attr = NULL;
	char *name, *payload, *rate;
	switch_status_t status = SWITCH_STATUS_GENERR;
	char workspace[512];
	const switch_codec_implementation_t *imp = NULL;

	while (osip_list_eol(media->a_attributes, pos) == 0) {
		attr = (sdp_attribute_t *) osip_list_get(media->a_attributes, pos);
		if (attr != NULL && strcasecmp(attr->a_att_field, "rtpmap") == 0) {
			switch_payload_t pt;
			uint32_t r;
			int32_t i;
			uint8_t match = 0;
			name = rate = payload = NULL;

			switch_copy_string(workspace, attr->a_att_value, sizeof(workspace));
			payload = workspace;
			if ((name = strchr(workspace, ' ')) != 0) {
                *(name++) = '\0';
			}
			if ((rate = strchr(name, '/'))) {
				*rate++ = '\0';
			}
			pt = (switch_payload_t)atoi(payload);
			r = atoi(rate);
			
			if (!strcasecmp(name, "telephone-event")) {
				tech_pvt->te = pt;
				attr = NULL;
				pos++;
				continue;
			}

			for(i = 0; !match && i < tech_pvt->num_codecs; i++) {

				for (imp = tech_pvt->codecs[i]; imp; imp = imp->next) {

					if (pt < 97) {
						match = (pt == imp->ianacode) ? 1 : 0;
					} else {
						match = strcasecmp(name, imp->iananame) ? 0 : 1;
					}
					
					if (match && (r == imp->samples_per_second)) {
						break;
					}
				}
				
			}

			if (match) {
				*dname = strdup(name);
				*drate = strdup(rate);
				*dpayload = strdup(payload);
				*impp = imp;
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "Found negotiated codec Payload: %s Name: %s Rate: %s\n",
								  *dpayload, *dname, *drate);	
				return SWITCH_STATUS_SUCCESS;
			}

		}

		attr = NULL;
		pos++;
	}

	return status;
}


static char *get_header_value(eXosip_event_t *je, char *name)
{
	osip_header_t *hp = NULL;
	osip_message_header_get_byname (je->request, name, 0, &hp);
	
	return hp ? hp->hvalue : NULL;
}


static void handle_message_new(eXosip_event_t *je)
{
	if (MSG_IS_REGISTER(je->request)) {
		int x = 0;
		osip_contact_t *contact;
		osip_uri_t *contact_uri = NULL;
		char *lame = NULL;
		char *url;
		char *expires = NULL;
		osip_message_t *tmp = NULL;
		char buf[1024];
		char *sql = NULL;
		time_t exptime;
		switch_event_t *s_event;

		for(;;) {
			if (osip_message_get_contact(je->request, x++, &contact) < 0) {
				break;
			}

			switch_safe_free(lame);
			osip_contact_to_str((const osip_contact_t *) contact, &lame);
			contact_uri = osip_contact_get_url(contact);

			if ((url = strchr(lame, ':'))) {
				char *p;
				url++;
				if ((p = strchr(url, '>'))) {
					*p = '\0';
				}


				if ((expires = get_header_value(je, "expires"))) {
					exptime = time(NULL) + atoi(expires) + 20;
				} else {
					exptime = time(NULL) + 3600;
				}

				
				if (!find_reg_url(globals.db, je->request->from->url->username, buf, sizeof(buf))) {
					sql = switch_core_db_mprintf("insert into sip_registrations values ('%s','%s','%s',%ld)", 
												 je->request->from->url->username,
												 je->request->from->url->host,
												 url, exptime);
				} else {
					sql = switch_core_db_mprintf("update sip_registrations set url='%s', expires=%ld where key = '%s'",
												 url,
												 exptime,
												 je->request->from->url->username);
					
				}

				if (switch_event_create_subclass(&s_event, SWITCH_EVENT_CUSTOM, MY_EVENT_REGISTER) == SWITCH_STATUS_SUCCESS) {
					switch_event_add_header(s_event, SWITCH_STACK_BOTTOM, "key", "%s", je->request->from->url->username);
					switch_event_add_header(s_event, SWITCH_STACK_BOTTOM, "url", "%s", url);
					switch_event_add_header(s_event, SWITCH_STACK_BOTTOM, "expires", "%ld", exptime);
					switch_event_fire(&s_event);
				}
				if (sql) {
					switch_mutex_lock(globals.reg_mutex);
					switch_core_db_persistant_execute(globals.db, sql, 25);
					switch_core_db_free(sql);
					sql = NULL;
					switch_mutex_unlock(globals.reg_mutex);
				}
				eXosip_lock();
				if (eXosip_message_build_answer(je->tid, 200, &tmp) < 0) {
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "build_answer failed\n");
					eXosip_unlock();
					continue;
				}
				eXosip_message_send_answer(je->tid, 200, tmp);
				eXosip_unlock();
			}
				
		}
		switch_safe_free(lame);		
	} 
}


static int handle_call_transfer(eXosip_event_t *event)
{
    osip_message_t *sip = event->request;
    osip_header_t *refer_hdr;
	int res;

    if (osip_message_header_get_byname (sip, "Refer-To", 0, &refer_hdr) < 0) {
		eXosip_call_send_answer(event->tid, 400, NULL);
		return 0;
    }

    switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG,  "Refer: %s\n", refer_hdr->hvalue);

	res = eXosip_call_send_answer(event->tid, 202, NULL);
    if (res) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "message_send_answer info failed! %d\n", event->tid);
    }

	{
		osip_from_t *refer = NULL;
		osip_uri_t *refer_uri = NULL;
		char *refer_str = NULL;
		
		if (osip_from_init(&refer) < 0) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Memory error\n");
			return -1;
		}

		if (osip_from_parse(refer, refer_hdr->hvalue) < 0) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Parse error\n");
			osip_from_free(refer);
			refer = NULL;
			return -1;
		}

		refer_uri = osip_from_get_url (refer);

		if (osip_uri_to_str(refer_uri, &refer_str) < 0) {
			osip_from_free(refer);
			return -1;
		}

		printf("TEST %s\n", refer_str);
#if 0

		transfer_to = opbx_bridged_channel(p->owner);
		if (transfer_to) {
			/* 	    int extout = 0; */

			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG,  "Got SIP blind transfer, applying to '%s'\n", transfer_to->name);

            /* Must release c's lock now, because it will not longer
			   be accessible after the transfer! */
			*nounlock = true;

			/* 	    opbx_setstate(transfer_to, OPBX_STATE_RING); */
			/* 	    hook_channel(transfer_to, NULL); */

			opbx_mutex_unlock(&p->owner->lock);
			/* 	    opbx_masq_park_call(p->owner, NULL, 0, &extout); */

			/* 	    switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG,  "Parked to %d\n", extout); */

			opbx_async_goto(transfer_to, p->friend->context, refer_str, 1);

			{
				osip_message_t *req = NULL;
				if (eXosip_call_build_notify(event->did,
											 EXOSIP_SUBCRSTATE_ACTIVE,
											 &req) < 0) {
					opbx_log(LOG_WARNING, "eXosip_call_build_notify failed\n");
				} else {
					const char sip_frag[] = "SIP/2.0 100 Trying\r\n\r\n";
					osip_message_set_header(req, osip_strdup("Event"),
											osip_strdup("refer"));
					osip_message_set_body(req, sip_frag, strlen(sip_frag));
					osip_message_set_content_type(req, "message/sipfrag");
					if (eXosip_call_send_request(event->did, req) < 0) {
						opbx_log(LOG_WARNING,
								 "eXosip_call_send_request failed\n");
					} else {
						opbx_log(LOG_DEBUG, "Notify ok\n");
					}
				}
			}
		} else {
			opbx_log(LOG_WARNING, "No channel up!");
		}
#endif
		osip_free(refer_str);
		osip_from_free(refer);
    }
    return 0;
}

static void handle_answer(eXosip_event_t *event)
{
	osip_message_t *ack = NULL;
	sdp_message_t *remote_sdp = NULL;
	sdp_connection_t *conn = NULL;
	sdp_media_t *remote_med = NULL;
	struct private_object *tech_pvt;
	char *dpayload = NULL, *dname = NULL, *drate = NULL;
	switch_channel_t *channel;
	const switch_codec_implementation_t *imp = NULL;
	uint8_t pre_answer = 0, reinvite = 0, isack = 0;


	if ((tech_pvt = get_pvt_by_call_id(event->cid)) == 0) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "cannot answer nonexistant call [%d]!\n", event->cid);
		return;
	}

	if (event->type == EXOSIP_CALL_RINGING) {
		pre_answer = 1;
		if (switch_test_flag(tech_pvt, TFLAG_PRE_ANSWER)) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "one pre-answer is enough for call [%d]!\n", event->cid);
			return;
		}
	} else if (event->type == EXOSIP_CALL_REINVITE) {
		reinvite = 1;
	} else if (event->type == EXOSIP_CALL_ACK) {
		isack = 1;
	}

	channel = switch_core_session_get_channel(tech_pvt->session);
	assert(channel != NULL);


	if (reinvite || isack) {
		if ((remote_sdp = eXosip_get_sdp_info(event->request)) == 0) {
			if (!isack) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Cant Find SDP?\n");
				switch_channel_hangup(channel, SWITCH_CAUSE_DESTINATION_OUT_OF_ORDER);
			}
			return;
		}
	} else {

		if (!event->response) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Someone answered... with no SDP information - WTF?!?\n");
			switch_channel_hangup(channel, SWITCH_CAUSE_DESTINATION_OUT_OF_ORDER);
			return;
		}


		/* Get all of the remote SDP elements... stuff */
		if ((remote_sdp = eXosip_get_sdp_info(event->response)) == 0) {
			/* Exosip is daft, they send the same event for both 180 and 183 WTF!!*/
			if (!pre_answer) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Cant Find SDP?\n");
				switch_channel_hangup(channel, SWITCH_CAUSE_DESTINATION_OUT_OF_ORDER);
			} else {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "I am daft, don't mind me.\n");
			}
		
			return;
		}
	}

	switch_channel_set_variable(channel, "endpoint_disposition", "ANSWER");

	conn = eXosip_get_audio_connection(remote_sdp);
	remote_med = eXosip_get_audio_media(remote_sdp);

	if (!strcmp(conn->c_addr, "0.0.0.0")) {
		eXosip_lock();
		eXosip_call_build_ack(event->did, &ack);
		eXosip_call_send_ack(event->did, ack);
		eXosip_unlock();

		switch_safe_free(dname);
		switch_safe_free(drate);
		switch_safe_free(dpayload);
		return;
	}
	
	/* Grab IP/port */
	tech_pvt->remote_sdp_audio_port = (switch_port_t)atoi(remote_med->m_port);
	snprintf(tech_pvt->remote_sdp_audio_ip, 50, conn->c_addr);

	/* Grab codec elements */
	if (parse_sdp_media(tech_pvt, remote_med, &dname, &drate, &dpayload, &imp) == SWITCH_STATUS_SUCCESS) {
		tech_pvt->payload_num = atoi(dpayload);
	}

	/* Assign them thar IDs */
	tech_pvt->did = event->did;
	tech_pvt->tid = event->tid;

	
	if (switch_test_flag(tech_pvt, TFLAG_USING_CODEC) && strcasecmp(dname, tech_pvt->read_codec.implementation->iananame)) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Changing Codec from %s to %s\n", tech_pvt->read_codec.implementation->iananame, dname);
		switch_core_codec_destroy(&tech_pvt->read_codec);
		switch_core_codec_destroy(&tech_pvt->write_codec);
		switch_core_session_reset(tech_pvt->session);
		switch_clear_flag_locked(tech_pvt, TFLAG_USING_CODEC);
	}

	if (reinvite || isack) {
		switch_set_flag_locked(tech_pvt, TFLAG_REINVITE);
	}

	if (!switch_test_flag(tech_pvt, TFLAG_USING_CODEC)) {
	
		int rate = atoi(drate);
		int ms = 0; //globals.codec_ms;

		if (imp) {
			ms = imp->microseconds_per_frame / 1000;
		}

		if (switch_core_codec_init(&tech_pvt->read_codec,
								   dname,
								   rate,
								   ms,
								   1,
								   SWITCH_CODEC_FLAG_ENCODE | SWITCH_CODEC_FLAG_DECODE,
								   NULL, switch_core_session_get_pool(tech_pvt->session)) != SWITCH_STATUS_SUCCESS) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Can't load codec?\n");
			switch_channel_hangup(channel, SWITCH_CAUSE_DESTINATION_OUT_OF_ORDER);
			return;
		} else {
			if (switch_core_codec_init(&tech_pvt->write_codec,
									   dname,
									   rate,
									   ms,
									   1,
									   SWITCH_CODEC_FLAG_ENCODE | SWITCH_CODEC_FLAG_DECODE,
									   NULL,
									   switch_core_session_get_pool(tech_pvt->session)) != SWITCH_STATUS_SUCCESS) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Can't load codec?\n");
				switch_channel_hangup(channel, SWITCH_CAUSE_DESTINATION_OUT_OF_ORDER);
				return;
			} else {
				int ms;
				tech_pvt->read_frame.rate = rate;
				switch_set_flag_locked(tech_pvt, TFLAG_USING_CODEC);
				ms = tech_pvt->write_codec.implementation->microseconds_per_frame / 1000;
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "Activate Outbound Codec %s/%d %d ms\n", dname, rate, ms);
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

	switch_safe_free(dname);
	switch_safe_free(drate);
	switch_safe_free(dpayload);


	if (activate_rtp(tech_pvt) != SWITCH_STATUS_SUCCESS) {
		exosip_on_hangup(tech_pvt->session);
		switch_channel_hangup(channel, SWITCH_CAUSE_NORMAL_CLEARING);
		return;
	}


	if (switch_test_flag(tech_pvt, TFLAG_RTP)) {
		channel = switch_core_session_get_channel(tech_pvt->session);
		assert(channel != NULL);
		if (pre_answer) {
			switch_set_flag_locked(tech_pvt, TFLAG_PRE_ANSWER);
			switch_channel_pre_answer(channel);
		} else {
			switch_channel_answer(channel);
		} 
	}
}




static const char *event_names[] = {
      "EXOSIP_REGISTRATION_NEW",         
      "EXOSIP_REGISTRATION_SUCCESS",     
      "EXOSIP_REGISTRATION_FAILURE",     
      "EXOSIP_REGISTRATION_REFRESHED",   
      "EXOSIP_REGISTRATION_TERMINATED",  
      "EXOSIP_CALL_INVITE",          
      "EXOSIP_CALL_REINVITE",        
      "EXOSIP_CALL_NOANSWER",        
      "EXOSIP_CALL_PROCEEDING",      
      "EXOSIP_CALL_RINGING",         
      "EXOSIP_CALL_ANSWERED",        
      "EXOSIP_CALL_REDIRECTED",      
      "EXOSIP_CALL_REQUESTFAILURE",  
      "EXOSIP_CALL_SERVERFAILURE",   
      "EXOSIP_CALL_GLOBALFAILURE",   
      "EXOSIP_CALL_ACK",             
      "EXOSIP_CALL_CANCELLED",       
      "EXOSIP_CALL_TIMEOUT",         
      "EXOSIP_CALL_MESSAGE_NEW",            
      "EXOSIP_CALL_MESSAGE_PROCEEDING",     
      "EXOSIP_CALL_MESSAGE_ANSWERED",       
      "EXOSIP_CALL_MESSAGE_REDIRECTED",     
      "EXOSIP_CALL_MESSAGE_REQUESTFAILURE", 
      "EXOSIP_CALL_MESSAGE_SERVERFAILURE",  
      "EXOSIP_CALL_MESSAGE_GLOBALFAILURE",  
      "EXOSIP_CALL_CLOSED",          
      "EXOSIP_CALL_RELEASED",           
      "EXOSIP_MESSAGE_NEW",            
      "EXOSIP_MESSAGE_PROCEEDING",     
      "EXOSIP_MESSAGE_ANSWERED",       
      "EXOSIP_MESSAGE_REDIRECTED",     
      "EXOSIP_MESSAGE_REQUESTFAILURE", 
      "EXOSIP_MESSAGE_SERVERFAILURE",  
      "EXOSIP_MESSAGE_GLOBALFAILURE",  
      "EXOSIP_SUBSCRIPTION_UPDATE",       
      "EXOSIP_SUBSCRIPTION_CLOSED",       
      "EXOSIP_SUBSCRIPTION_NOANSWER",        
      "EXOSIP_SUBSCRIPTION_PROCEEDING",      
      "EXOSIP_SUBSCRIPTION_ANSWERED",        
      "EXOSIP_SUBSCRIPTION_REDIRECTED",      
      "EXOSIP_SUBSCRIPTION_REQUESTFAILURE",  
      "EXOSIP_SUBSCRIPTION_SERVERFAILURE",   
      "EXOSIP_SUBSCRIPTION_GLOBALFAILURE",   
      "EXOSIP_SUBSCRIPTION_NOTIFY",          
      "EXOSIP_SUBSCRIPTION_RELEASED",        
      "EXOSIP_IN_SUBSCRIPTION_NEW",          
      "EXOSIP_IN_SUBSCRIPTION_RELEASED",     
      "EXOSIP_NOTIFICATION_NOANSWER",        
      "EXOSIP_NOTIFICATION_PROCEEDING",      
      "EXOSIP_NOTIFICATION_ANSWERED",        
      "EXOSIP_NOTIFICATION_REDIRECTED",      
      "EXOSIP_NOTIFICATION_REQUESTFAILURE",  
      "EXOSIP_NOTIFICATION_SERVERFAILURE",   
      "EXOSIP_NOTIFICATION_GLOBALFAILURE",   
      "EXOSIP_EVENT_COUNT"                
};

static void log_event(eXosip_event_t * je)
{
	char buf[100];
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "EVENT [%s]\n", event_names[je->type]);

	buf[0] = '\0';
	if (je->type == EXOSIP_CALL_NOANSWER) {
		snprintf(buf, 99, "<- (%i %i) No answer", je->cid, je->did);
	} else if (je->type == EXOSIP_CALL_CLOSED) {
		snprintf(buf, 99, "<- (%i %i) Call Closed", je->cid, je->did);
	} else if (je->type == EXOSIP_CALL_RELEASED) {
		snprintf(buf, 99, "<- (%i %i) Call released", je->cid, je->did);
	} else if (je->type == EXOSIP_MESSAGE_NEW && je->request != NULL && MSG_IS_MESSAGE(je->request)) {
		char *tmp = NULL;

		if (je->request != NULL) {
			osip_body_t *body;
			osip_from_to_str(je->request->from, &tmp);

			osip_message_get_body(je->request, 0, &body);
			if (body != NULL && body->body != NULL) {
				snprintf(buf, 99, "<- (%i) from: %s TEXT: %s", je->tid, tmp, body->body);
			}
			osip_free(tmp);
		} else {
			snprintf(buf, 99, "<- (%i) New event for unknown request?", je->tid);
		}
	} else if (je->type == EXOSIP_MESSAGE_NEW) {
		char *tmp = NULL;

		osip_from_to_str(je->request->from, &tmp);
		snprintf(buf, 99, "<- (%i) %s from: %s", je->tid, je->request->sip_method, tmp);
		osip_free(tmp);
	} else if (je->type == EXOSIP_MESSAGE_PROCEEDING
			   || je->type == EXOSIP_MESSAGE_ANSWERED
			   || je->type == EXOSIP_MESSAGE_REDIRECTED
			   || je->type == EXOSIP_MESSAGE_REQUESTFAILURE
			   || je->type == EXOSIP_MESSAGE_SERVERFAILURE || je->type == EXOSIP_MESSAGE_GLOBALFAILURE) {
		if (je->response != NULL && je->request != NULL) {
			char *tmp = NULL;

			osip_to_to_str(je->request->to, &tmp);
			snprintf(buf, 99, "<- (%i) [%i %s for %s] to: %s",
					 je->tid, je->response->status_code, je->response->reason_phrase, je->request->sip_method, tmp);
			osip_free(tmp);
		} else if (je->request != NULL) {
			snprintf(buf, 99, "<- (%i) Error for %s request", je->tid, je->request->sip_method);
		} else {
			snprintf(buf, 99, "<- (%i) Error for unknown request", je->tid);
		}
	} else if (je->response == NULL && je->request != NULL && je->cid > 0) {
		char *tmp = NULL;

		osip_from_to_str(je->request->from, &tmp);
		snprintf(buf, 99, "<- (%i %i) %s from: %s", je->cid, je->did, je->request->cseq->method, tmp);
		osip_free(tmp);
	} else if (je->response != NULL && je->cid > 0) {
		char *tmp = NULL;

		osip_to_to_str(je->request->to, &tmp);
		snprintf(buf, 99, "<- (%i %i) [%i %s] for %s to: %s",
				 je->cid, je->did, je->response->status_code,
				 je->response->reason_phrase, je->request->sip_method, tmp);
		osip_free(tmp);
	} else if (je->response == NULL && je->request != NULL && je->rid > 0) {
		char *tmp = NULL;

		osip_from_to_str(je->request->from, &tmp);
		snprintf(buf, 99, "<- (%i) %s from: %s", je->rid, je->request->cseq->method, tmp);
		osip_free(tmp);
	} else if (je->response != NULL && je->rid > 0) {
		char *tmp = NULL;

		osip_from_to_str(je->request->from, &tmp);
		snprintf(buf, 99, "<- (%i) [%i %s] from: %s",
				 je->rid, je->response->status_code, je->response->reason_phrase, tmp);
		osip_free(tmp);
	} else if (je->response == NULL && je->request != NULL && je->sid > 0) {
		char *tmp = NULL;
		char *stat = NULL;
		osip_header_t *sub_state;

		osip_message_header_get_byname(je->request, "subscription-state", 0, &sub_state);
		if (sub_state != NULL && sub_state->hvalue != NULL)
			stat = sub_state->hvalue;

		osip_uri_to_str(je->request->from->url, &tmp);
		snprintf(buf, 99, "<- (%i) [%s] %s from: %s", je->sid, stat, je->request->cseq->method, tmp);
		osip_free(tmp);
	} else if (je->response != NULL && je->sid > 0) {
		char *tmp = NULL;

		osip_uri_to_str(je->request->to->url, &tmp);
		snprintf(buf, 99, "<- (%i) [%i %s] from: %s",
				 je->sid, je->response->status_code, je->response->reason_phrase, tmp);
		osip_free(tmp);
	} else if (je->response == NULL && je->request != NULL) {
		char *tmp = NULL;

		osip_from_to_str(je->request->from, &tmp);
		snprintf(buf, 99, "<- (c=%i|d=%i|s=%i|n=%i) %s from: %s",
				 je->cid, je->did, je->sid, je->nid, je->request->sip_method, tmp);
		osip_free(tmp);
	} else if (je->response != NULL) {
		char *tmp = NULL;

		osip_from_to_str(je->request->from, &tmp);
		snprintf(buf, 99, "<- (c=%i|d=%i|s=%i|n=%i) [%i %s] for %s from: %s",
				 je->cid, je->did, je->sid, je->nid,
				 je->response->status_code, je->response->reason_phrase, je->request->sip_method, tmp);
		osip_free(tmp);
	} else {
		snprintf(buf, 99, "<- (c=%i|d=%i|s=%i|n=%i|t=%i) %s",
				 je->cid, je->did, je->sid, je->nid, je->tid, je->textinfo);
	}
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "\n%s\n", buf);
	/* Print it out */
}



static int config_exosip(int reload)
{
	char *cf = "exosip.conf";
	switch_xml_t cfg, xml, settings, param;

	globals.bytes_per_frame = DEFAULT_BYTES_PER_FRAME;

	if (!(xml = switch_xml_open_cfg(cf, &cfg, NULL))) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "open of %s failed\n", cf);
		return SWITCH_STATUS_TERM;
	}

	globals.dtmf_duration = 100;
	globals.te = 101;

	if ((settings = switch_xml_child(cfg, "settings"))) {
		for (param = switch_xml_child(settings, "param"); param; param = param->next) {
			char *var = (char *) switch_xml_attr_soft(param, "name");
			char *val = (char *) switch_xml_attr_soft(param, "value");

			if (!strcmp(var, "debug")) {
				globals.debug = atoi(val);
			} else if (!strcmp(var, "use-rtp-timer") && switch_true(val)) {
				  switch_set_flag(&globals, TFLAG_TIMER);
			} else if (!strcmp(var, "use-rtp-auto-adjust") && switch_true(val)) {
				  switch_set_flag(&globals, TFLAG_AA);
			} else if (!strcmp(var, "port")) {
				globals.port = atoi(val);
			} else if (!strcmp(var, "rfc2833-pt")) {
				globals.te = (switch_payload_t) atoi(val);
			} else if (!strcmp(var, "vad")) {
				if (!strcasecmp(val, "in")) {
					switch_set_flag(&globals, TFLAG_VAD_IN);
				} else if (!strcasecmp(val, "out")) {
					switch_set_flag(&globals, TFLAG_VAD_OUT);
				} else if (!strcasecmp(val, "both")) {
					switch_set_flag(&globals, TFLAG_VAD_IN);
					switch_set_flag(&globals, TFLAG_VAD_OUT);
				} else {
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Invald option %s for VAD\n", val);
				}
			} else if (!strcmp(var, "ext-rtp-ip")) {
				set_global_extrtpip(val);
			} else if (!strcmp(var, "rtp-ip")) {
				set_global_rtpip(val);
			} else if (!strcmp(var, "sip-ip")) {
				set_global_sipip(val);
			} else if (!strcmp(var, "dialplan")) {
				set_global_dialplan(val);
			} else if (!strncasecmp(var, "srtp:", 5)) {
				char *name = var + 5;
				if (name) {
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Add Realm [%s][%s]\n", name, val);
					switch_core_hash_insert(globals.srtp_hash, switch_core_strdup(module_pool, name), switch_core_strdup(module_pool, val));
				}
			} else if (!strcmp(var, "codec-prefs")) {
				set_global_codec_string(val);
				globals.codec_order_last = switch_separate_string(globals.codec_string, ',', globals.codec_order, SWITCH_MAX_CODECS);
			} else if (!strcmp(var, "codec-ms")) {
				globals.codec_ms = atoi(val);
			} else if (!strcmp(var, "dtmf-duration")) {
				int dur = atoi(val);
				if (dur > 10 && dur < 8000) {
					globals.dtmf_duration = dur;
				} else {
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Duration out of bounds!\n");
				}
			}
		}
	}
	

	if (!globals.rtpip) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Setting ip to 'guess'\n");
		set_global_rtpip("guess");
	}

	if (!globals.codec_ms) {
		globals.codec_ms = 0;
	}

	if (!globals.port) {
		globals.port = 5060;
	}
	
	switch_xml_free(xml);

	if (!globals.dialplan) {
		set_global_dialplan("default");
	}


	/* Setup the user agent */
	eXosip_set_user_agent("FreeSWITCH");


	return 0;

}


static void check_expire(time_t now)
{
	char sql[1024];
	char *errmsg;

	switch_mutex_lock(globals.reg_mutex);
	snprintf(sql, sizeof(sql), "select url from sip_registrations where expires > 0 and expires < %ld", (long) now);	
	switch_core_db_exec(globals.db, sql, del_callback, NULL, &errmsg);

	if (errmsg) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "SQL ERR [%s][%s]\n", sql, errmsg);
		switch_core_db_free(errmsg);
		errmsg = NULL;
	}
	
	snprintf(sql, sizeof(sql), "delete from sip_registrations where expires > 0 and expires < %ld", (long) now);
	switch_core_db_persistant_execute(globals.db, sql, 1);
	switch_mutex_unlock(globals.reg_mutex);
}



SWITCH_MOD_DECLARE(switch_status_t) switch_module_runtime(void)
{
	eXosip_event_t *event = NULL;
	switch_event_t *s_event;
	time_t now = 0, next = 0;
	int interval = 30;

	config_exosip(0);

	if (globals.debug) {
		osip_trace_initialize((osip_trace_level_t) globals.debug, stdout);
	}

	if (eXosip_init()) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "eXosip_init initialization failed!\n");
		return SWITCH_STATUS_TERM;
	}

	if (eXosip_listen_addr(IPPROTO_UDP, globals.sipip, globals.port, AF_INET, 0)) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "eXosip_listen_addr failed!\n");
		return SWITCH_STATUS_TERM;
	}

	if (switch_event_create(&s_event, SWITCH_EVENT_PUBLISH) == SWITCH_STATUS_SUCCESS) {
		switch_event_add_header(s_event, SWITCH_STACK_BOTTOM, "service", "_sip._udp");
		switch_event_add_header(s_event, SWITCH_STACK_BOTTOM, "port", "%d", globals.port);
		switch_event_fire(&s_event);
	}
	
	globals.running = 1;
	while (globals.running > 0) {
		if ((event = eXosip_event_wait(0, 100)) == 0) {
			now = time(NULL);
			if (now >= next) {
				check_expire(now);
				next = now + interval;
			}
			switch_yield(1000);
			continue;
		}

		eXosip_lock();
		eXosip_automatic_action();
		eXosip_unlock();

		log_event(event);

		switch (event->type) {
		case EXOSIP_MESSAGE_NEW:
			handle_message_new(event);
			break;
		case EXOSIP_CALL_INVITE:
			if (exosip_create_call(event) != SWITCH_STATUS_SUCCESS) {
				destroy_call_by_event(event);
			}
			break;
		case EXOSIP_CALL_REINVITE:
			/* See what the reinvite is about - on hold or whatever */
			handle_answer(event);
			break;
		case EXOSIP_CALL_MESSAGE_NEW:
			if (event->request != NULL && MSG_IS_REFER(event->request)) {
				handle_call_transfer(event);
			}
			break;
		case EXOSIP_CALL_ACK:
			handle_answer(event);
			/* If audio is not flowing and this has SDP - fire it up! */
			break;
		case EXOSIP_CALL_ANSWERED:
			handle_answer(event);
			break;
		case EXOSIP_CALL_PROCEEDING:
			/* This is like a 100 Trying... yeah */
			break;
		case EXOSIP_CALL_RINGING:
			handle_answer(event);
			break;
		case EXOSIP_CALL_REDIRECTED:
			break;
		case EXOSIP_CALL_CLOSED:
			destroy_call_by_event(event);
			break;
		case EXOSIP_CALL_RELEASED:
			destroy_call_by_event(event);
			break;
		case EXOSIP_CALL_NOANSWER:
			destroy_call_by_event(event);
			break;
		case EXOSIP_CALL_REQUESTFAILURE:
			destroy_call_by_event(event);
			break;
		case EXOSIP_CALL_SERVERFAILURE:
			destroy_call_by_event(event);
			break;
		case EXOSIP_CALL_GLOBALFAILURE:
			destroy_call_by_event(event);
			break;
			/* Registration related stuff */
		case EXOSIP_REGISTRATION_NEW:
			break;
		default:
			/* Unknown event... casually absorb it for now */
			break;
		}

		//switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "There was an event (%d) [%s]\n", event->type, event->textinfo);
		/* Free the event */
		eXosip_event_free(event);
	}

	eXosip_quit();
	globals.running = 0;
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Monitor Thread Exiting\n");
	//switch_sleep(2000000);

	return SWITCH_STATUS_TERM;
}
