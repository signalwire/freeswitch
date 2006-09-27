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
 * mod_sofia.c -- SOFIA SIP Endpoint
 *
 */

/* Best viewed in a 160 x 60 VT100 Terminal or so the line below at least fits across your screen*/
/*************************************************************************************************************************************************************/


/*Defines etc..*/
/*************************************************************************************************************************************************************/
#define HAVE_APR
#include <switch.h>

struct sofia_profile;
typedef struct sofia_profile sofia_profile_t;
#define NUA_MAGIC_T sofia_profile_t

struct private_object;
typedef struct private_object private_object_t;
#define NUA_HMAGIC_T switch_core_session_t

#define MY_EVENT_REGISTER "sofia::register"
#define MY_EVENT_EXPIRE "sofia::expire"
#include <sofia-sip/nua.h>
#include <sofia-sip/sip_status.h>
#include <sofia-sip/sdp.h>
#include <sofia-sip/sip_protos.h>

static char reg_sql[] =
"CREATE TABLE sip_registrations (\n"
"   user            VARCHAR(255),\n"
"   host            VARCHAR(255),\n"
"   contact         VARCHAR(255),\n"
"   expires         INTEGER(8)"
");\n";





static const char modname[] = "mod_sofia";
#define STRLEN 15

static switch_memory_pool_t *module_pool = NULL;

#define set_param(ptr,val) if (ptr) {free(ptr) ; ptr = NULL;} if (val) {ptr = strdup(val);}
#define set_anchor(t,m) if (t->Anchor) {delete t->Anchor;} t->Anchor = new SipMessage(m);


/* Local Structures */
/*************************************************************************************************************************************************************/
struct sip_alias_node {
	char *url;
	nua_t *nua;
	struct sip_alias_node *next;
};

typedef struct sip_alias_node sip_alias_node_t;

typedef enum {
	TFLAG_IO = (1 << 0),
	TFLAG_INBOUND = (1 << 1),
	TFLAG_OUTBOUND = (1 << 2),
	TFLAG_READING = (1 << 3),
	TFLAG_WRITING = (1 << 4),
	TFLAG_HUP = (1 << 5),
	TFLAG_RTP = (1 << 6),
	TFLAG_BYE = (1 << 7),
	TFLAG_ANS = (1 << 8),
	TFLAG_EARLY_MEDIA = (1 << 9),
	TFLAG_SECURE = (1 << 10),
	TFLAG_VAD_IN = ( 1 << 11),
	TFLAG_VAD_OUT = ( 1 << 12),
	TFLAG_VAD = ( 1 << 13),
	TFLAG_TIMER = (1 << 14),
	TFLAG_READY = (1 << 15)
} TFLAGS;

static struct {
	switch_hash_t *profile_hash;
	switch_mutex_t *hash_mutex;
	uint32_t callid;
	int32_t running;
	switch_mutex_t *mutex;
} globals;

struct sofia_profile {
	int debug;
	char *name;
	char *dbname;
	char *dialplan;
	char *context;
	char *extrtpip;
	char *rtpip;
	char *sipip;
	char *extsipip;
	char *username;
	char *url;
	char *sipdomain;
	char *timer_name;
	int sip_port;
	char *codec_string;
	char *codec_order[SWITCH_MAX_CODECS];
	int codec_order_last;
	int running;
	int codec_ms;
	int dtmf_duration;
	unsigned int flags;
	uint32_t max_calls;
	nua_t *nua;
	switch_memory_pool_t *pool;
	su_root_t *s_root;
	sip_alias_node_t *aliases;
	switch_payload_t te;
	uint32_t codec_flags;
	switch_mutex_t *reg_mutex;
	switch_core_db_t *db;
};



struct private_object {
	uint32_t flags;
	switch_core_session_t *session;
	switch_frame_t read_frame;
	const switch_codec_implementation_t *codecs[SWITCH_MAX_CODECS];
	int num_codecs;
	switch_codec_t read_codec;
	switch_codec_t write_codec;
	uint32_t codec_index;
	uint32_t codec_ms;
	switch_caller_profile_t *caller_profile;
	int32_t timestamp_send;
	int32_t timestamp_recv;
	switch_rtp_t *rtp_session;
	int ssrc;
	switch_time_t last_read;
	sofia_profile_t *profile;
	char *local_sdp_audio_ip;
	switch_port_t local_sdp_audio_port;
	char *remote_sdp_audio_ip;
	switch_port_t remote_sdp_audio_port;
	char *adv_sdp_audio_ip;
	switch_port_t adv_sdp_audio_port;
	char *from_uri;
	char *to_uri;
	char *from_address;
	char *to_address;
	char *callid;
	char *far_end_contact;
	char *contact_url;
	char *rm_encoding;
	char *remote_sdp_str;
	char *local_sdp_str;
	char *dest;
	unsigned long rm_rate;
	switch_payload_t pt;
	switch_mutex_t *flag_mutex;
	switch_payload_t te;
	nua_handle_t *nh;
	su_home_t *home;
	sip_contact_t *contact;
};

/* Function Prototypes */
/*************************************************************************************************************************************************************/
static switch_status_t sofia_on_init(switch_core_session_t *session);

static switch_status_t sofia_on_hangup(switch_core_session_t *session);

static switch_status_t sofia_on_loopback(switch_core_session_t *session);

static switch_status_t sofia_on_transmit(switch_core_session_t *session);

static switch_status_t sofia_outgoing_channel(switch_core_session_t *session, switch_caller_profile_t *outbound_profile,
											  switch_core_session_t **new_session, switch_memory_pool_t *pool);

static switch_status_t sofia_read_frame(switch_core_session_t *session, switch_frame_t **frame, int timeout,
										switch_io_flag_t flags, int stream_id);

static switch_status_t sofia_write_frame(switch_core_session_t *session, switch_frame_t *frame, int timeout,
										 switch_io_flag_t flags, int stream_id);

static switch_status_t config_sofia(int reload);

static switch_status_t sofia_kill_channel(switch_core_session_t *session, int sig);

static switch_status_t activate_rtp(private_object_t *tech_pvt);

static void deactivate_rtp(private_object_t *tech_pvt);

static void set_local_sdp(private_object_t *tech_pvt);

static void tech_set_codecs(private_object_t *tech_pvt);

static void attach_private(switch_core_session_t *session,
                           sofia_profile_t *profile,
                           private_object_t *tech_pvt,
                           char *channame);

static void terminate_session(switch_core_session_t **session, switch_call_cause_t cause, int line);

static switch_status_t tech_choose_port(private_object_t *tech_pvt);

static void do_invite(switch_core_session_t *session);

static uint8_t negotiate_sdp(switch_core_session_t *session, sdp_session_t *sdp);

static void sip_i_state(int status,
                        char const *phrase,
                        nua_t *nua,
                        sofia_profile_t *profile,
                        nua_handle_t *nh,
                        switch_core_session_t *session,
                        sip_t const *sip,
                        tagi_t tags[]);

static void sip_i_invite(nua_t *nua,
                         sofia_profile_t *profile,
                         nua_handle_t *nh,
                         switch_core_session_t *session,
                         sip_t const *sip,
                         tagi_t tags[]);

static void sip_i_register(nua_t *nua,
						   sofia_profile_t *profile,
						   nua_handle_t *nh,
						   switch_core_session_t *session,
						   sip_t const *sip,
						   tagi_t tags[]);

static void event_callback(nua_event_t   event,
                           int           status,
                           char const   *phrase,
                           nua_t        *nua,
                           sofia_profile_t  *profile,
                           nua_handle_t *nh,
                           switch_core_session_t *session,
                           sip_t const  *sip,
                           tagi_t        tags[]);


static void *SWITCH_THREAD_FUNC profile_thread_run(switch_thread_t *thread, void *obj);

static void launch_profile_thread(sofia_profile_t *profile);

static switch_status_t config_sofia(int reload);


/* BODY OF THE MODULE */
/*************************************************************************************************************************************************************/
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
			switch_event_add_header(s_event, SWITCH_STACK_BOTTOM, "user", "%s", argv[0]);
			switch_event_add_header(s_event, SWITCH_STACK_BOTTOM, "host", "%s", argv[1]);
			switch_event_add_header(s_event, SWITCH_STACK_BOTTOM, "contact", "%s", argv[2]);
			switch_event_add_header(s_event, SWITCH_STACK_BOTTOM, "expires", "%d", argv[3]);
			switch_event_fire(&s_event);
		}
	}
	return 0;
}

static void check_expire(sofia_profile_t *profile, time_t now)
{
	char sql[1024];
	char *errmsg;

	switch_mutex_lock(profile->reg_mutex);
	snprintf(sql, sizeof(sql), "select * from sip_registrations where expires > 0 and expires < %ld", (long) now);	
	switch_core_db_exec(profile->db, sql, del_callback, NULL, &errmsg);

	if (errmsg) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "SQL ERR [%s][%s]\n", sql, errmsg);
		switch_core_db_free(errmsg);
		errmsg = NULL;
	}
	
	snprintf(sql, sizeof(sql), "delete from sip_registrations where expires > 0 and expires < %ld", (long) now);
	switch_core_db_persistant_execute(profile->db, sql, 1);
	switch_mutex_unlock(profile->reg_mutex);
}

static char *find_reg_url(sofia_profile_t *profile, char *user, char *host, char *val, switch_size_t len)
{
	char *errmsg;
	struct callback_t cbt = {0};


	cbt.val = val;
	cbt.len = len;
	switch_mutex_lock(profile->reg_mutex);
	if (host) {
		snprintf(val, len, "select contact from sip_registrations where user='%s' and host='%s'", user, host);	
	} else {
		snprintf(val, len, "select contact from sip_registrations where user='%s'", user);	
	}

	switch_core_db_exec(profile->db, val, find_callback, &cbt, &errmsg);

	if (errmsg) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "SQL ERR [%s][%s]\n", val, errmsg);
		switch_core_db_free(errmsg);
		errmsg = NULL;
	}

	switch_mutex_unlock(profile->reg_mutex);
	return cbt.matches ? val : NULL;
}


static void set_local_sdp(private_object_t *tech_pvt)
{
	char buf[1024];
	switch_time_t now = switch_time_now();

	assert(tech_pvt != NULL);
	
	snprintf(buf, sizeof(buf), 
			 "v=0\n"
			 "o=FreeSWITCH %d%"APR_TIME_T_FMT" %d%"APR_TIME_T_FMT" IN IP4 %s\n"
			 "s=FreeSWITCH\n"
			 "c=IN IP4 %s\n"
			 "t=0 0\n"
			 "a=sendrecv\n"
			 "m=audio %d RTP/AVP",
			 tech_pvt->adv_sdp_audio_port,
			 now,
			 tech_pvt->adv_sdp_audio_port,
			 now,
			 tech_pvt->adv_sdp_audio_ip,
			 tech_pvt->adv_sdp_audio_ip,
			 tech_pvt->adv_sdp_audio_port
			 );

	if (tech_pvt->rm_encoding) {
		snprintf(buf + strlen(buf), sizeof(buf) - strlen(buf), " %d", tech_pvt->pt);
	} else if (tech_pvt->num_codecs) {
		int i;
		for (i = 0; i < tech_pvt->num_codecs; i++) {
			const switch_codec_implementation_t *imp = tech_pvt->codecs[i];
			snprintf(buf + strlen(buf), sizeof(buf) - strlen(buf), " %d", imp->ianacode);
		}
	}

	if (tech_pvt->te > 96) {
		snprintf(buf + strlen(buf), sizeof(buf) - strlen(buf), " %d", tech_pvt->te);
	}

	snprintf(buf + strlen(buf), sizeof(buf) - strlen(buf), "\n");

	if (tech_pvt->rm_encoding) {
		snprintf(buf + strlen(buf), sizeof(buf) - strlen(buf), "a=rtpmap:%d %s/%ld\n", tech_pvt->pt, tech_pvt->rm_encoding, tech_pvt->rm_rate);
	} else if (tech_pvt->num_codecs) {
		int i;
		for (i = 0; i < tech_pvt->num_codecs; i++) {
			const switch_codec_implementation_t *imp = tech_pvt->codecs[i];
			snprintf(buf + strlen(buf), sizeof(buf) - strlen(buf), "a=rtpmap:%d %s/%d\n", imp->ianacode, imp->iananame, imp->samples_per_second);
			snprintf(buf + strlen(buf), sizeof(buf) - strlen(buf), "a=ptime:%d\n", imp->microseconds_per_frame / 1000);
		}
	}
	
	if (tech_pvt->te > 96) {
		snprintf(buf + strlen(buf), sizeof(buf) - strlen(buf), "a=rtpmap:%d telephone-event/8000\na=fmtp:%d 0-16", tech_pvt->te, tech_pvt->te);
	}



	
	tech_pvt->local_sdp_str = switch_core_session_strdup(tech_pvt->session, buf);

}

static void tech_set_codecs(private_object_t *tech_pvt)
{
	if (tech_pvt->num_codecs) {
		return;
	}

	if (tech_pvt->profile->codec_string) {
		tech_pvt->num_codecs = switch_loadable_module_get_codecs_sorted(tech_pvt->codecs,
																		SWITCH_MAX_CODECS,
																		tech_pvt->profile->codec_order,
																		tech_pvt->profile->codec_order_last);
		
	} else {
		tech_pvt->num_codecs = switch_loadable_module_get_codecs(switch_core_session_get_pool(tech_pvt->session), tech_pvt->codecs,
																 sizeof(tech_pvt->codecs) / sizeof(tech_pvt->codecs[0]));
	}
}

static void attach_private(switch_core_session_t *session,
						   sofia_profile_t *profile,
						   private_object_t *tech_pvt,
						   char *channame)
{
	switch_channel_t *channel;
	char name[256];

	assert(session != NULL);
	assert(profile != NULL);
	assert(tech_pvt != NULL);

	switch_core_session_add_stream(session, NULL);
	channel = switch_core_session_get_channel(session);
	
	switch_mutex_init(&tech_pvt->flag_mutex, SWITCH_MUTEX_NESTED, switch_core_session_get_pool(session));
	switch_mutex_lock(tech_pvt->flag_mutex);
	tech_pvt->flags = profile->flags;
	switch_mutex_unlock(tech_pvt->flag_mutex);
	tech_pvt->profile = profile;
	tech_pvt->te = profile->te;
	tech_pvt->session = session;

	tech_pvt->home = su_home_new(sizeof(*tech_pvt->home));
	su_home_init(tech_pvt->home);

	switch_core_session_set_private(session, tech_pvt);

	tech_set_codecs(tech_pvt);
	snprintf(name, sizeof(name), "sofia/%s/%s", profile->name, channame);
	switch_channel_set_name(channel, name);
}

static void terminate_session(switch_core_session_t **session, switch_call_cause_t cause, int line)
{
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Term called from line: %d\n", line);
	if (*session) {
		switch_channel_t *channel = switch_core_session_get_channel(*session);
		switch_channel_state_t state = switch_channel_get_state(channel);
		struct private_object *tech_pvt = NULL;

		tech_pvt = switch_core_session_get_private(*session);

		if (tech_pvt) {
			if (state > CS_INIT && state < CS_HANGUP) {
				switch_channel_hangup(channel, cause);
			}
			
			if (!switch_test_flag(tech_pvt, TFLAG_READY)) {
				if (state > CS_INIT && state < CS_HANGUP) {
					sofia_on_hangup(*session);
				}
				switch_core_session_destroy(session);
			} 
		} else {
			switch_core_session_destroy(session);
		}
	}
}

static switch_status_t tech_choose_port(private_object_t *tech_pvt)
{
	char *ip = tech_pvt->profile->rtpip;
	switch_port_t sdp_port;
	char *err;
	
	if (tech_pvt->adv_sdp_audio_port) {
		return SWITCH_STATUS_SUCCESS;
	}

	tech_pvt->local_sdp_audio_ip = ip;
	tech_pvt->local_sdp_audio_port = switch_rtp_request_port();
	sdp_port = tech_pvt->local_sdp_audio_port;


	if (tech_pvt->profile->extrtpip) {
		if (!strncasecmp(tech_pvt->profile->extrtpip, "stun:", 5)) {
			char *stun_ip = tech_pvt->profile->extrtpip + 5;
			if (!stun_ip) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Stun Failed! NO STUN SERVER\n");
				terminate_session(&tech_pvt->session, SWITCH_CAUSE_DESTINATION_OUT_OF_ORDER, __LINE__);
				return SWITCH_STATUS_FALSE;
			}
			if (switch_stun_lookup(&ip,
								   &sdp_port,
								   stun_ip,
								   SWITCH_STUN_DEFAULT_PORT,
								   &err,
								   switch_core_session_get_pool(tech_pvt->session)) != SWITCH_STATUS_SUCCESS) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Stun Failed! %s:%d [%s]\n", stun_ip, SWITCH_STUN_DEFAULT_PORT, err);
				terminate_session(&tech_pvt->session, SWITCH_CAUSE_DESTINATION_OUT_OF_ORDER, __LINE__);
				return SWITCH_STATUS_FALSE;
			}
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "Stun Success [%s]:[%d]\n", ip, sdp_port);
		} else {
			ip = tech_pvt->profile->extrtpip;
		}
	}

	tech_pvt->adv_sdp_audio_ip = switch_core_session_strdup(tech_pvt->session, ip);
	tech_pvt->adv_sdp_audio_port = sdp_port;
	return SWITCH_STATUS_SUCCESS;
}

static void do_invite(switch_core_session_t *session)
{
	private_object_t *tech_pvt;
    switch_channel_t *channel = NULL;


    channel = switch_core_session_get_channel(session);
    assert(channel != NULL);

    tech_pvt = (private_object_t *) switch_core_session_get_private(session);
    assert(tech_pvt != NULL);

	tech_choose_port(tech_pvt);
	set_local_sdp(tech_pvt);
	switch_set_flag_locked(tech_pvt, TFLAG_READY);
	
	tech_pvt->nh = nua_handle(tech_pvt->profile->nua, NULL, SIPTAG_TO_STR(tech_pvt->dest), TAG_END());
	nua_handle_bind(tech_pvt->nh, session);
	nua_invite(tech_pvt->nh,
			   SOATAG_USER_SDP_STR(tech_pvt->local_sdp_str),
			   SOATAG_RTP_SORT(SOA_RTP_SORT_REMOTE),
			   SOATAG_RTP_SELECT(SOA_RTP_SELECT_ALL),
			   TAG_END());
	

}

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

	switch_channel_set_variable(channel, "endpoint_disposition", "INIT");
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "SOFIA INIT\n");

	if (switch_test_flag(tech_pvt, TFLAG_OUTBOUND)) {
		do_invite(session);
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

	switch_channel_set_variable(channel, "endpoint_disposition", "RING");
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

	switch_channel_set_variable(channel, "endpoint_disposition", "EXECUTE");
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "SOFIA EXECUTE\n");

	return SWITCH_STATUS_SUCCESS;
}

// map QSIG cause codes to SIP ala RFC4497
static int hangup_cause_to_sip(switch_call_cause_t cause) {
	switch (cause) {
		case SWITCH_CAUSE_UNALLOCATED: 
		case SWITCH_CAUSE_NO_ROUTE_TRANSIT_NET:
		case SWITCH_CAUSE_NO_ROUTE_DESTINATION:
			return 404;
		case SWITCH_CAUSE_USER_BUSY:
			return 486;
		case SWITCH_CAUSE_NO_USER_RESPONSE:
			return 408;
		case SWITCH_CAUSE_NO_ANSWER:
			return 480;
		case SWITCH_CAUSE_CALL_REJECTED:
			return 603;
		case SWITCH_CAUSE_NUMBER_CHANGED:
			return 410;
		case SWITCH_CAUSE_DESTINATION_OUT_OF_ORDER:
			return 502;
		case SWITCH_CAUSE_INVALID_NUMBER_FORMAT:
			return 484;
		case SWITCH_CAUSE_FACILITY_REJECTED:
			return 501;
		case SWITCH_CAUSE_NORMAL_UNSPECIFIED:
			return 480;
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
			return 501;
		case SWITCH_CAUSE_INCOMPATIBLE_DESTINATION:
			return 503;
		case SWITCH_CAUSE_RECOVERY_ON_TIMER_EXPIRE:
			return 504;
		default:
			return 500;
	}

}

static switch_status_t sofia_on_hangup(switch_core_session_t *session)
{
	private_object_t *tech_pvt;
	switch_channel_t *channel = NULL;
	switch_call_cause_t cause;
	int sip_cause;

	channel = switch_core_session_get_channel(session);
	assert(channel != NULL);

	cause = switch_channel_get_cause(channel);
	sip_cause = hangup_cause_to_sip(cause);

	tech_pvt = (private_object_t *) switch_core_session_get_private(session);
	assert(tech_pvt != NULL);

	deactivate_rtp(tech_pvt);

	su_home_deinit(tech_pvt->home);

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Channel %s hanging up, cause: %s\n", 
			  switch_channel_get_name(channel), switch_channel_cause2str(cause), sip_cause);

	if (tech_pvt->nh) {
		if (!switch_test_flag(tech_pvt, TFLAG_BYE)) {
			if (switch_test_flag(tech_pvt, TFLAG_ANS)) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Sending BYE\n");
				nua_bye(tech_pvt->nh, TAG_END());
			} else {
				if (switch_test_flag(tech_pvt, TFLAG_INBOUND)) {
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Responding to INVITE with: %d\n", sip_cause);
					nua_respond(tech_pvt->nh, sip_cause, NULL, TAG_END());
				} else {
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Sending CANCEL\n");
					nua_cancel(tech_pvt->nh, TAG_END());
				}
			}
		}
		nua_handle_destroy(tech_pvt->nh);
		tech_pvt->nh = NULL;
	}

	switch_set_flag_locked(tech_pvt, TFLAG_BYE);
	switch_clear_flag_locked(tech_pvt, TFLAG_IO);

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

static void deactivate_rtp(private_object_t *tech_pvt)
{
	int loops = 0;//, sock = -1;
	if (switch_rtp_ready(tech_pvt->rtp_session)) {
		while (loops < 10 && (switch_test_flag(tech_pvt, TFLAG_READING) || switch_test_flag(tech_pvt, TFLAG_WRITING))) {
			switch_yield(10000);
			loops++;
		}
		switch_rtp_destroy(&tech_pvt->rtp_session);
	}
}

static switch_status_t tech_set_codec(private_object_t *tech_pvt)
{
	switch_channel_t *channel;
	assert(tech_pvt->codecs[tech_pvt->codec_index] != NULL);

	if (tech_pvt->read_codec.implementation) {
		return SWITCH_STATUS_SUCCESS;
	}

	channel = switch_core_session_get_channel(tech_pvt->session);
	assert(channel != NULL);

	if (switch_core_codec_init(&tech_pvt->read_codec,  
							   tech_pvt->rm_encoding,
							   tech_pvt->rm_rate,
							   tech_pvt->codec_ms,
							   1,
							   SWITCH_CODEC_FLAG_ENCODE | SWITCH_CODEC_FLAG_DECODE | tech_pvt->profile->codec_flags,
							   NULL,
							   switch_core_session_get_pool(tech_pvt->session)) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Can't load codec?\n");
		terminate_session(&tech_pvt->session, SWITCH_CAUSE_DESTINATION_OUT_OF_ORDER, __LINE__);
		return SWITCH_STATUS_FALSE;
	} else {
		if (switch_core_codec_init(&tech_pvt->write_codec,
								   tech_pvt->rm_encoding,
								   tech_pvt->rm_rate,
								   tech_pvt->codec_ms,
								   1,
								   SWITCH_CODEC_FLAG_ENCODE | SWITCH_CODEC_FLAG_DECODE | tech_pvt->profile->codec_flags,
								   NULL,
								   switch_core_session_get_pool(tech_pvt->session)) != SWITCH_STATUS_SUCCESS) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Can't load codec?\n");
			terminate_session(&tech_pvt->session, SWITCH_CAUSE_DESTINATION_OUT_OF_ORDER, __LINE__);
			return SWITCH_STATUS_FALSE;
		} else {
			int ms;
			tech_pvt->read_frame.rate = tech_pvt->rm_rate;
			ms = tech_pvt->write_codec.implementation->microseconds_per_frame / 1000;
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "Set Codec %s %s/%d %d ms\n",
							  switch_channel_get_name(channel),
							  tech_pvt->codecs[tech_pvt->codec_index]->iananame, tech_pvt->rm_rate, tech_pvt->codec_ms);
			tech_pvt->read_frame.codec = &tech_pvt->read_codec;
				
			switch_core_session_set_read_codec(tech_pvt->session, &tech_pvt->read_codec);
			switch_core_session_set_write_codec(tech_pvt->session, &tech_pvt->write_codec);
		}
	}
	return SWITCH_STATUS_SUCCESS;
}


static switch_status_t activate_rtp(private_object_t *tech_pvt)
{
	int bw, ms;
	switch_channel_t *channel;
	const char *err = NULL;
	switch_rtp_flag_t flags;
	switch_status_t status;

	assert(tech_pvt != NULL);

	channel = switch_core_session_get_channel(tech_pvt->session);
	assert(channel != NULL);


	if (switch_rtp_ready(tech_pvt->rtp_session)) {
		return SWITCH_STATUS_SUCCESS;
	}

	if ((status = tech_set_codec(tech_pvt)) != SWITCH_STATUS_SUCCESS) {
		return status;
	}
	
	bw = tech_pvt->read_codec.implementation->bits_per_second;
	ms = tech_pvt->read_codec.implementation->microseconds_per_frame;

	flags = (switch_rtp_flag_t) (SWITCH_RTP_FLAG_RAW_WRITE | SWITCH_RTP_FLAG_AUTOADJ | SWITCH_RTP_FLAG_DATAWAIT);

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "RTP [%s] %s:%d->%s:%d codec: %u ms: %d\n",
					  switch_channel_get_name(channel),
					  tech_pvt->local_sdp_audio_ip,
					  tech_pvt->local_sdp_audio_port,
					  tech_pvt->remote_sdp_audio_ip,
					  tech_pvt->remote_sdp_audio_port,
					  tech_pvt->read_codec.implementation->ianacode,
					  tech_pvt->read_codec.implementation->microseconds_per_frame / 1000);

	tech_pvt->rtp_session = switch_rtp_new(tech_pvt->local_sdp_audio_ip,
										   tech_pvt->local_sdp_audio_port,
										   tech_pvt->remote_sdp_audio_ip,
										   tech_pvt->remote_sdp_audio_port,
										   tech_pvt->read_codec.implementation->ianacode,
										   tech_pvt->read_codec.implementation->encoded_bytes_per_frame,
										   tech_pvt->codec_ms * 1000,
										   (switch_rtp_flag_t) flags,
										   NULL,
										   tech_pvt->profile->timer_name,
										   &err,
										   switch_core_session_get_pool(tech_pvt->session));
	
	if (switch_rtp_ready(tech_pvt->rtp_session)) {
		uint8_t vad_in = switch_test_flag(tech_pvt, TFLAG_VAD_IN) ? 1 : 0;
		uint8_t vad_out = switch_test_flag(tech_pvt, TFLAG_VAD_OUT) ? 1 : 0;
		uint8_t inb = switch_test_flag(tech_pvt, TFLAG_OUTBOUND) ? 0 : 1;

		tech_pvt->ssrc = switch_rtp_get_ssrc(tech_pvt->rtp_session);
		switch_set_flag_locked(tech_pvt, TFLAG_RTP);
		switch_set_flag_locked(tech_pvt, TFLAG_IO);
		
		if ((vad_in && inb) || (vad_out && !inb)) {
			switch_rtp_enable_vad(tech_pvt->rtp_session, tech_pvt->session, &tech_pvt->read_codec, SWITCH_VAD_FLAG_TALKING);
			switch_set_flag_locked(tech_pvt, TFLAG_VAD);
		}
	} else {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "RTP REPORTS ERROR: [%s]\n", err);
		terminate_session(&tech_pvt->session, SWITCH_CAUSE_DESTINATION_OUT_OF_ORDER, __LINE__);
		switch_clear_flag_locked(tech_pvt, TFLAG_IO);
		return SWITCH_STATUS_FALSE;
	}
		
	switch_set_flag_locked(tech_pvt, TFLAG_IO);
	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t sofia_answer_channel(switch_core_session_t *session)
{
	private_object_t *tech_pvt;
	switch_channel_t *channel = NULL;

	assert(session != NULL);

	channel = switch_core_session_get_channel(session);
	assert(channel != NULL);

	tech_pvt = (private_object_t *) switch_core_session_get_private(session);
	assert(tech_pvt != NULL);

	if (!switch_test_flag(tech_pvt, TFLAG_ANS) && !switch_channel_test_flag(channel, CF_OUTBOUND)) {
		switch_set_flag_locked(tech_pvt, TFLAG_ANS);
		tech_choose_port(tech_pvt);
		set_local_sdp(tech_pvt);
		activate_rtp(tech_pvt);
		if (tech_pvt->nh) {
			nua_respond(tech_pvt->nh, SIP_200_OK, 
						//SIPTAG_CONTACT(tech_pvt->contact),
						SOATAG_USER_SDP_STR(tech_pvt->local_sdp_str), TAG_END());
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Local SDP:\n%s\n", tech_pvt->local_sdp_str);
		}
	}

	return SWITCH_STATUS_SUCCESS;
}


static switch_status_t sofia_read_frame(switch_core_session_t *session, switch_frame_t **frame, int timeout,
										switch_io_flag_t flags, int stream_id)
{
	private_object_t *tech_pvt = NULL;
	size_t bytes = 0, samples = 0, frames = 0, ms = 0;
	switch_channel_t *channel = NULL;
	int payload = 0;

	//switch_time_t now, started = switch_time_now(), last_act = switch_time_now();
	//unsigned int elapsed;
	//uint32_t hard_timeout = 60000 * 3;

	channel = switch_core_session_get_channel(session);
	assert(channel != NULL);

	tech_pvt = (private_object_t *) switch_core_session_get_private(session);
	assert(tech_pvt != NULL);

	if (switch_test_flag(tech_pvt, TFLAG_HUP)) {
		return SWITCH_STATUS_FALSE;
	}

	tech_pvt->read_frame.datalen = 0;
	switch_set_flag_locked(tech_pvt, TFLAG_READING);


	bytes = tech_pvt->read_codec.implementation->encoded_bytes_per_frame;
	samples = tech_pvt->read_codec.implementation->samples_per_frame;
	ms = tech_pvt->read_codec.implementation->microseconds_per_frame;
	
	if (tech_pvt->last_read) {
#if 0
		elapsed = (unsigned int)((switch_time_now() - tech_pvt->last_read) / 1000);
		if (elapsed > 60000) {
			return SWITCH_STATUS_TIMEOUT;
		}
#endif
	}

	if (switch_test_flag(tech_pvt, TFLAG_IO)) {
		switch_status_t status;

		if (!switch_test_flag(tech_pvt, TFLAG_RTP)) {
			return SWITCH_STATUS_GENERR;
		}

		assert(tech_pvt->rtp_session != NULL);
		tech_pvt->read_frame.datalen = 0;


		while (!switch_test_flag(tech_pvt, TFLAG_BYE) && switch_test_flag(tech_pvt, TFLAG_IO) && tech_pvt->read_frame.datalen == 0) {
			//now = switch_time_now();
			tech_pvt->read_frame.flags = SFF_NONE;

			status = switch_rtp_zerocopy_read_frame(tech_pvt->rtp_session, &tech_pvt->read_frame);
			if (status != SWITCH_STATUS_SUCCESS && status != SWITCH_STATUS_BREAK) {
				return SWITCH_STATUS_FALSE;
			}

			
			
			payload = tech_pvt->read_frame.payload;

#if 0
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
#endif
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
				} else
					frames = 1;

				samples = frames * tech_pvt->read_codec.implementation->samples_per_frame;
				ms = frames * tech_pvt->read_codec.implementation->microseconds_per_frame;
				tech_pvt->timestamp_recv += (int32_t) samples;
				tech_pvt->read_frame.samples = (int) samples;
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


static switch_status_t sofia_write_frame(switch_core_session_t *session, switch_frame_t *frame, int timeout,
										 switch_io_flag_t flags, int stream_id)
{
	private_object_t *tech_pvt;
	switch_channel_t *channel = NULL;
	switch_status_t status = SWITCH_STATUS_SUCCESS;
	int bytes = 0, samples = 0, frames = 0;

	channel = switch_core_session_get_channel(session);
	assert(channel != NULL);

	tech_pvt = (private_object_t *) switch_core_session_get_private(session);
	assert(tech_pvt != NULL);

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


	if (tech_pvt->read_codec.implementation->encoded_bytes_per_frame) {
		bytes = tech_pvt->read_codec.implementation->encoded_bytes_per_frame;
		frames = ((int) frame->datalen / bytes);
	} else
		frames = 1;

	samples = frames * tech_pvt->read_codec.implementation->samples_per_frame;

#if 0
	printf("%s %s->%s send %d bytes %d samples in %d frames ts=%d\n",
		   switch_channel_get_name(channel),
		   tech_pvt->local_sdp_audio_ip,
		   tech_pvt->remote_sdp_audio_ip,
		   frame->datalen,
		   samples,
		   frames,
		   tech_pvt->timestamp_send);
#endif

	switch_rtp_write_frame(tech_pvt->rtp_session, frame, samples);
	
	tech_pvt->timestamp_send += (int) samples;

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

	switch_clear_flag_locked(tech_pvt, TFLAG_IO);
	switch_set_flag_locked(tech_pvt, TFLAG_HUP);

	if (switch_rtp_ready(tech_pvt->rtp_session)) {
		switch_rtp_kill_socket(tech_pvt->rtp_session);
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
									digits,
									tech_pvt->profile->dtmf_duration * (tech_pvt->read_codec.implementation->samples_per_second / 1000));
	
}

static switch_status_t sofia_receive_message(switch_core_session_t *session, switch_core_session_message_t *msg)
{
	switch_channel_t *channel;
	private_object_t *tech_pvt;
			
	channel = switch_core_session_get_channel(session);
	assert(channel != NULL);
			
	tech_pvt = (private_object_t *) switch_core_session_get_private(session);
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
	case SWITCH_MESSAGE_INDICATE_PROGRESS: {
		struct private_object *tech_pvt;
	    switch_channel_t *channel = NULL;
		  
	    channel = switch_core_session_get_channel(session);
	    assert(channel != NULL);

	    tech_pvt = switch_core_session_get_private(session);
	    assert(tech_pvt != NULL);
		
	    if (!switch_test_flag(tech_pvt, TFLAG_EARLY_MEDIA)) {
			switch_set_flag_locked(tech_pvt, TFLAG_EARLY_MEDIA);
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "Asked to send early media by %s\n", msg->from);

			/* Transmit 183 Progress with SDP */
			tech_choose_port(tech_pvt);
			set_local_sdp(tech_pvt);
			activate_rtp(tech_pvt);
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "183 SDP:\n%s\n", tech_pvt->local_sdp_str);
			nua_respond(tech_pvt->nh, SIP_183_SESSION_PROGRESS,
						//SIPTAG_CONTACT(tech_pvt->contact),
						SOATAG_USER_SDP_STR(tech_pvt->local_sdp_str), TAG_END());
	    }
	}
		break;
	default:
		break;
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
	/*.send_dtmf*/ sofia_send_dtmf,
	/*.receive_message*/ sofia_receive_message
};

static const switch_state_handler_table_t sofia_event_handlers = {
	/*.on_init */ sofia_on_init,
	/*.on_ring */ sofia_on_ring,
	/*.on_execute */ sofia_on_execute,
	/*.on_hangup */ sofia_on_hangup,
	/*.on_loopback */ sofia_on_loopback,
	/*.on_transmit */ sofia_on_transmit
};

static const switch_endpoint_interface_t sofia_endpoint_interface = {
	/*.interface_name */ "sofia",
	/*.io_routines */ &sofia_io_routines,
	/*.event_handlers */ &sofia_event_handlers,
	/*.private */ NULL,
	/*.next */ NULL
};

static const switch_loadable_module_interface_t sofia_module_interface = {
	/*.module_name */ modname,
	/*.endpoint_interface */ &sofia_endpoint_interface,
	/*.timer_interface */ NULL,
	/*.dialplan_interface */ NULL,
	/*.codec_interface */ NULL,
	/*.application_interface */ NULL
};

static switch_status_t sofia_outgoing_channel(switch_core_session_t *session, switch_caller_profile_t *outbound_profile,
											  switch_core_session_t **new_session, switch_memory_pool_t *pool)
{
	switch_status_t status = SWITCH_STATUS_FALSE;
	switch_core_session_t *nsession;
	char *data, *profile_name, *dest;
	sofia_profile_t *profile;
	switch_caller_profile_t *caller_profile = NULL;
	private_object_t *tech_pvt = NULL;
	switch_channel_t *channel;
	char *host;

	*new_session = NULL;

	if (!(nsession = switch_core_session_request(&sofia_endpoint_interface, pool))) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Error Creating Session\n");
		goto done;
	}

	if (!(tech_pvt = (struct private_object *) switch_core_session_alloc(nsession, sizeof(*tech_pvt)))) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Error Creating Session\n");
		terminate_session(&nsession, SWITCH_CAUSE_DESTINATION_OUT_OF_ORDER, __LINE__);
		goto done;
	}

	data = switch_core_session_strdup(nsession, outbound_profile->destination_number);
	profile_name = data;
	
	if (!(dest = strchr(profile_name, '/'))) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Invalid URL\n");
        terminate_session(&nsession, SWITCH_CAUSE_DESTINATION_OUT_OF_ORDER, __LINE__);
        goto done;
	}

	*dest++ = '\0';
	
	if (!(profile = (sofia_profile_t *) switch_core_hash_find(globals.profile_hash, profile_name))) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Invalid Profile\n");
        terminate_session(&nsession, SWITCH_CAUSE_DESTINATION_OUT_OF_ORDER, __LINE__);
        goto done;
	}

	if ((host = strchr(dest, '%'))) {
		char buf[128];
		*host++ = '\0';
		find_reg_url(profile, dest, host, buf, sizeof(buf));
		tech_pvt->dest = switch_core_session_strdup(nsession, buf);
		dest = tech_pvt->dest + 4;
	} else {
		tech_pvt->dest = switch_core_session_alloc(nsession, strlen(dest) + 5);
		snprintf(tech_pvt->dest, strlen(dest) + 5, "sip:%s", dest);
	}

	channel = switch_core_session_get_channel(nsession);
	attach_private(nsession, profile, tech_pvt, dest);	
	caller_profile = switch_caller_profile_clone(nsession, outbound_profile);
	switch_channel_set_caller_profile(channel, caller_profile);
	switch_channel_set_flag(channel, CF_OUTBOUND);
	switch_set_flag_locked(tech_pvt, TFLAG_OUTBOUND);
	switch_channel_set_state(channel, CS_INIT);
	switch_channel_set_variable(channel, "endpoint_disposition", "OUTBOUND");
	*new_session = nsession;
	status = SWITCH_STATUS_SUCCESS;
 done:
	return status;
}


static uint8_t negotiate_sdp(switch_core_session_t *session, sdp_session_t *sdp)
{
	uint8_t match = 0;
	private_object_t *tech_pvt;
	sdp_media_t *m;

	tech_pvt = switch_core_session_get_private(session);
	assert(tech_pvt != NULL);                                                                                                                               
	
	for (m = sdp->sdp_media; m ; m = m->m_next) {
		if (m->m_type == sdp_media_audio) {
			sdp_rtpmap_t *map;

			for (map = m->m_rtpmaps; map; map = map->rm_next) {
				int32_t i;
								
				if (!strcmp(map->rm_encoding, "telephone-event")) {
					tech_pvt->te = (switch_payload_t)map->rm_pt;
				}

				for (i = 0; i < tech_pvt->num_codecs; i++) {
					const switch_codec_implementation_t *imp = tech_pvt->codecs[i];
								
					if (map->rm_pt < 97) {
						match = (map->rm_pt == imp->ianacode) ? 1 : 0;
					} else {
						match = strcasecmp(map->rm_encoding, imp->iananame) ? 0 : 1;
					}
								
					if (match && (map->rm_rate == imp->samples_per_second)) {
						tech_pvt->rm_encoding = switch_core_session_strdup(session, (char *)map->rm_encoding);
						tech_pvt->pt = (switch_payload_t)map->rm_pt;
						tech_pvt->rm_rate = map->rm_rate;
						tech_pvt->codec_ms = imp->microseconds_per_frame / 1000;
						tech_pvt->remote_sdp_audio_ip = switch_core_session_strdup(session, (char *)sdp->sdp_connection->c_address);
						tech_pvt->remote_sdp_audio_port = (switch_port_t)m->m_port;
						break;
					} else {
						match = 0;
					}
				}

				if (match) {
					if (tech_set_codec(tech_pvt) != SWITCH_STATUS_SUCCESS) {
						match = 0;
					}
					break;
				}
			}
		}
	}

	return match;
}

// map sip responses to QSIG cause codes ala RFC4497
static switch_call_cause_t sip_cause_to_freeswitch(int status) {
	switch (status) {
		case 200:
			return SWITCH_CAUSE_NORMAL_CLEARING;
		case 401:
		case 402: 
		case 403:
		case 407:
		case 603:
			return SWITCH_CAUSE_CALL_REJECTED;
		case 404:
		case 485:
		case 604:	
			return SWITCH_CAUSE_UNALLOCATED;
		case 408: 
		case 504:
			return SWITCH_CAUSE_RECOVERY_ON_TIMER_EXPIRE;
		case 410: 
			return SWITCH_CAUSE_NUMBER_CHANGED;
		case 413: 
		case 414:
		case 416:
		case 420:
		case 421:
		case 423:
		case 505:
		case 513:
			return SWITCH_CAUSE_INTERWORKING;
		case 480:
			return SWITCH_CAUSE_NO_USER_RESPONSE;
		case 400:
		case 481:
		case 500:
		case 503:
			return SWITCH_CAUSE_NORMAL_TEMPORARY_FAILURE;
		case 486:
		case 600:
			return SWITCH_CAUSE_USER_BUSY;
		case 484:
			return SWITCH_CAUSE_INVALID_NUMBER_FORMAT;
		case 488:
		case 606:
			return SWITCH_CAUSE_BEARERCAPABILITY_NOTIMPL;
		case 502:
			return SWITCH_CAUSE_NETWORK_OUT_OF_ORDER;
		default: 
			return SWITCH_CAUSE_NORMAL_UNSPECIFIED;
	}
}

static void sip_i_state(int status,
						char const *phrase, 
						nua_t *nua,
						sofia_profile_t *profile,
						nua_handle_t *nh,
						switch_core_session_t *session,
						sip_t const *sip,
						tagi_t tags[])
	 
{
	char const *l_sdp = NULL, *r_sdp = NULL;
	//int audio = nua_active_inactive, video = nua_active_inactive, chat = nua_active_inactive;
	int offer_recv = 0, answer_recv = 0, offer_sent = 0, answer_sent = 0;
	int ss_state = nua_callstate_init;
	switch_channel_t *channel = NULL;
	private_object_t *tech_pvt = NULL;
	
	tl_gets(tags, 
			NUTAG_CALLSTATE_REF(ss_state),
			NUTAG_OFFER_RECV_REF(offer_recv),
			NUTAG_ANSWER_RECV_REF(answer_recv),
			NUTAG_OFFER_SENT_REF(offer_sent),
			NUTAG_ANSWER_SENT_REF(answer_sent),
			SOATAG_LOCAL_SDP_STR_REF(l_sdp),
			SOATAG_REMOTE_SDP_STR_REF(r_sdp),
			TAG_END()); 

	if (session) {
		channel = switch_core_session_get_channel(session);
		assert(channel != NULL);

		tech_pvt = switch_core_session_get_private(session);
		assert(tech_pvt != NULL);
		
		tech_pvt->nh = nh;
		
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Channel %s entering state [%s]\n", 
						  switch_channel_get_name(channel),
						  nua_callstate_name(ss_state));

		if (r_sdp) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Remote SDP:\n%s\n", r_sdp);			
			tech_pvt->remote_sdp_str = switch_core_session_strdup(session, (char *)r_sdp);
		}
	}


	switch ((enum nua_callstate)ss_state) {
	case nua_callstate_init:
		break;
	case nua_callstate_authenticating:
		break;
	case nua_callstate_calling:
		break;
	case nua_callstate_proceeding:
		if (channel) {
			if (r_sdp) {
				sdp_parser_t *parser = sdp_parse(tech_pvt->home, r_sdp, (int)strlen(r_sdp), 0);
				sdp_session_t *sdp;
				uint8_t match = 0;

				if (tech_pvt->num_codecs) {
					if ((sdp = sdp_session(parser))) {
						match = negotiate_sdp(session, sdp);
					}
				}

				if (parser) {
					sdp_parser_free(parser);
				}


				if (match) {
					tech_choose_port(tech_pvt);
					activate_rtp(tech_pvt);
					switch_channel_set_variable(channel, "endpoint_disposition", "EARLY MEDIA");
					switch_channel_pre_answer(channel);
					return;
				}
				switch_channel_set_variable(channel, "endpoint_disposition", "NO CODECS");
				nua_respond(nh, SIP_488_NOT_ACCEPTABLE, 
							//SIPTAG_CONTACT(tech_pvt->contact), 
							TAG_END());
			}
		}

		break;
	case nua_callstate_completing:
		nua_ack(nh, TAG_END());
		break;
	case nua_callstate_received: 
		if (channel) {
			if (r_sdp) {
				sdp_parser_t *parser = sdp_parse(tech_pvt->home, r_sdp, (int)strlen(r_sdp), 0);
				sdp_session_t *sdp;
				uint8_t match = 0;



				if (tech_pvt->num_codecs) {
					if ((sdp = sdp_session(parser))) {
						match = negotiate_sdp(session, sdp);
					}
				}

				if (parser) {
					sdp_parser_free(parser);
				}


				if (match) {
					switch_channel_set_variable(channel, "endpoint_disposition", "RECEIVED");
					switch_channel_set_state(channel, CS_INIT);
					switch_set_flag_locked(tech_pvt, TFLAG_READY);
					switch_core_session_thread_launch(session);
					return;
				}
				switch_channel_set_variable(channel, "endpoint_disposition", "NO CODECS");
				nua_respond(nh, SIP_488_NOT_ACCEPTABLE, 
							//SIPTAG_CONTACT(tech_pvt->contact), 
							TAG_END());
			}
		}

		break;		
	case nua_callstate_early:
		break;
	case nua_callstate_completed:
		break;
	case nua_callstate_ready:
		if (channel) {
			if (r_sdp) {
				sdp_parser_t *parser = sdp_parse(tech_pvt->home, r_sdp, (int)strlen(r_sdp), 0);
				sdp_session_t *sdp;
				uint8_t match = 0;

				if (tech_pvt->num_codecs) {
					if ((sdp = sdp_session(parser))) {
						match = negotiate_sdp(session, sdp);
					}
				}

				if (parser) {
					sdp_parser_free(parser);
				}


				if (match) {
					switch_set_flag_locked(tech_pvt, TFLAG_ANS);
					switch_channel_set_variable(channel, "endpoint_disposition", "ANSWER");
					tech_choose_port(tech_pvt);
					activate_rtp(tech_pvt);
					switch_channel_answer(channel);
					return;
				}

				switch_channel_set_variable(channel, "endpoint_disposition", "NO CODECS");
				nua_respond(nh, SIP_488_NOT_ACCEPTABLE, 
							//SIPTAG_CONTACT(tech_pvt->contact), 
							TAG_END());
			} else if (switch_test_flag(tech_pvt, TFLAG_EARLY_MEDIA)) {
				switch_set_flag_locked(tech_pvt, TFLAG_ANS);
				switch_channel_set_variable(channel, "endpoint_disposition", "ANSWER");
				switch_channel_answer(channel);
				return;
			} //else probably an ack

		}

		break;
	case nua_callstate_terminating:
		break;
	case nua_callstate_terminated:
		if (session) {
			switch_set_flag_locked(tech_pvt, TFLAG_BYE);
			terminate_session(&session, sip_cause_to_freeswitch(status), __LINE__);
		}
		break;
	}


}

static void sip_i_invite(nua_t *nua, 
						 sofia_profile_t *profile,
						 nua_handle_t *nh, 
						 switch_core_session_t *session, 
						 sip_t const *sip,
						 tagi_t tags[])
{

	
	if (!session) {
		if ((session = switch_core_session_request(&sofia_endpoint_interface, NULL))) {
			private_object_t *tech_pvt = NULL;
			switch_channel_t *channel = NULL;
			sip_from_t const *from = sip->sip_from;
			sip_to_t const *to = sip->sip_to;
			char *displayname;
			char username[256];


			if (!(tech_pvt = (private_object_t *) switch_core_session_alloc(session, sizeof(private_object_t)))) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Hey where is my memory pool?\n");
				terminate_session(&session, SWITCH_CAUSE_DESTINATION_OUT_OF_ORDER, __LINE__);
				return;
			}

			displayname = switch_core_session_strdup(session, (char *) from->a_display);
			if (*displayname == '"') {
				char *p;
				
				displayname++;
				if ((p = strchr(displayname, '"'))) {
					*p = '\0';
				}
			}
			
			snprintf(username, sizeof(username), "%s@%s", (char *) from->a_url->url_user, (char *) from->a_url->url_host);
			attach_private(session, profile, tech_pvt, username);

			snprintf(username, sizeof(username), "sip:%s@%s", (char *) from->a_url->url_user, (char *) from->a_url->url_host);
			tech_pvt->contact = sip_contact_create(tech_pvt->home, URL_STRING_MAKE(username), NULL);
			
			channel = switch_core_session_get_channel(session);
			switch_channel_set_variable(channel, "endpoint_disposition", "INBOUND CALL");
			
			if ((tech_pvt->caller_profile = switch_caller_profile_new(switch_core_session_get_pool(session),
																	  (char *) from->a_url->url_user,
																	  profile->dialplan,
																	  displayname,
																	  (char *) from->a_url->url_user,
																	  (char *) from->a_url->url_host,
																	  NULL,
																	  NULL,
																	  NULL,
																	  (char *)modname,
																	  profile->context,
																	  (char *) to->a_url->url_user)) != 0) {
				switch_channel_set_caller_profile(channel, tech_pvt->caller_profile);
			}
			switch_set_flag_locked(tech_pvt, TFLAG_INBOUND);
			nua_handle_bind(nh, session);
		}
	}
}


static void sip_i_register(nua_t *nua,
						   sofia_profile_t *profile,
						   nua_handle_t *nh,
						   switch_core_session_t *session,
						   sip_t const *sip,
						   tagi_t tags[])
{
	sip_from_t const *from = sip->sip_from;
	sip_expires_t const *expires = sip->sip_expires;
	//sip_to_t const *to = sip->sip_to;
	sip_contact_t const *contact = sip->sip_contact;
	// SIP_407_PROXY_AUTH_REQUIRED
	switch_xml_t domain, xml, user, param;
	char params[1024] = "";
	char *sql;
	switch_event_t *s_event;
	char *from_user = (char *) from->a_url->url_user;
	char *from_host = (char *) from->a_url->url_host;
	char *contact_user = (char *) contact->m_url->url_user;
	char *contact_host = (char *) contact->m_url->url_host;
	char buf[512];
	
	snprintf(params, sizeof(params), "from_user=%s&from_host=%s&contact_user=%s&contact_host=%s",
			 from_user,
			 from_host,
			 contact_user,
			 contact_host 
			 );


	if (switch_xml_locate("directory", "domain", "name", (char *)from->a_url->url_host, &xml, &domain, params) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "open of directory failed\n");
		nua_respond(nh, SIP_401_UNAUTHORIZED, SIPTAG_CONTACT(contact), TAG_END());
		return;
	}

	if (!(user = switch_xml_find_child(domain, "user", "id", from_user))) {
		nua_respond(nh, SIP_401_UNAUTHORIZED, SIPTAG_CONTACT(contact), TAG_END());
		switch_xml_free(xml);
		return;
	}
	

	for (param = switch_xml_child(user, "param"); param; param = param->next) {
		char *var = (char *) switch_xml_attr_soft(param, "name");
		char *val = (char *) switch_xml_attr_soft(param, "value");

		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "param [%s]=[%s]\n", var, val);
	}


	if (!find_reg_url(profile, from_user, from_host, buf, sizeof(buf))) {
		sql = switch_core_db_mprintf("insert into sip_registrations values ('%q','%q','sip:%q@%q',%ld)", 
									 from_user,
									 from_host,
									 contact_user,
									 contact_host,
									 (long) time(NULL) + (long)expires->ex_delta);
	} else {
		sql = switch_core_db_mprintf("update sip_registrations set contact='sip:%q@%q', expires=%ld where user='%q' and host='%q'",
									 contact_user,
                                     contact_host,
									 (long) time(NULL) + (long)expires->ex_delta,
									 from_user,
									 from_host);
		
	}
	
	if (switch_event_create_subclass(&s_event, SWITCH_EVENT_CUSTOM, MY_EVENT_REGISTER) == SWITCH_STATUS_SUCCESS) {
		switch_event_add_header(s_event, SWITCH_STACK_BOTTOM, "from_user", "%s", from_user);
		switch_event_add_header(s_event, SWITCH_STACK_BOTTOM, "from_host", "%s", from_host);
		switch_event_add_header(s_event, SWITCH_STACK_BOTTOM, "contact_user", "%s", contact_user);
		switch_event_add_header(s_event, SWITCH_STACK_BOTTOM, "contact_host", "%s", contact_host);
		switch_event_add_header(s_event, SWITCH_STACK_BOTTOM, "expires", "%ld", (long)expires->ex_delta);
		switch_event_fire(&s_event);
	}
	if (sql) {
		switch_mutex_lock(profile->reg_mutex);
		switch_core_db_persistant_execute(profile->db, sql, 25);
					switch_core_db_free(sql);
					sql = NULL;
					switch_mutex_unlock(profile->reg_mutex);
	}
	
	switch_xml_free(xml);

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Got a Register from [%s@%s] contact [%s@%s] expires %ld\n", 
					  from_user, 
					  from_host,
					  contact_user,
					  contact_host,
					  (long)expires->ex_delta
					  );

	
	nua_respond(nh, SIP_200_OK, SIPTAG_CONTACT(contact), TAG_END());
	//nua_respond(nh, SIP_407_PROXY_AUTH_REQUIRED, TAG_END());
}

static void event_callback(nua_event_t   event,
						   int           status,
						   char const   *phrase,
						   nua_t        *nua,
						   sofia_profile_t  *profile,
						   nua_handle_t *nh,
						   switch_core_session_t *session,
						   sip_t const  *sip,
						   tagi_t        tags[])
{
	if (session) {
		if (switch_core_session_read_lock(session) != SWITCH_STATUS_SUCCESS) {
			/* too late */
			return;
		}
	}
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "channel [%s] event [%s] status [%d] [%s]\n",
					  session ? switch_channel_get_name(switch_core_session_get_channel(session)) : "null",nua_event_name (event), status, phrase);
	
	switch (event) {
	case nua_r_shutdown:    
		//sip_r_shutdown(status, phrase, nua, profile, nh, session, sip, tags);
		break;

	case nua_r_get_params:    
		//sip_r_get_params(status, phrase, nua, profile, nh, session, sip, tags);
		break;

	case nua_r_register:
		//sip_r_register(status, phrase, nua, profile, nh, session, sip, tags);
		break;
    
	case nua_r_unregister:
		//sip_r_unregister(status, phrase, nua, profile, nh, session, sip, tags);
		break;
    
	case nua_r_options:
		//sip_r_options(status, phrase, nua, profile, nh, session, sip, tags);
		break;

	case nua_r_invite:
		//sip_r_invite(status, phrase, nua, profile, nh, session, sip, tags);
		break;

	case nua_i_fork:
		//sip_i_fork(status, phrase, nua, profile, nh, session, sip, tags);
		break;
    
	case nua_i_invite:
		sip_i_invite(nua, profile, nh, session, sip, tags);
		break;

    case nua_i_register:
 		sip_i_register (nua, profile, nh, session, sip, tags);
        break;

	case nua_i_prack:
		//sip_i_prack(nua, profile, nh, session, sip, tags);
		break;

	case nua_i_state:
		sip_i_state(status, phrase, nua, profile, nh, session, sip, tags);
		break;
    
	case nua_r_bye:
		//sip_r_bye(status, phrase, nua, profile, nh, session, sip, tags);
		break;

	case nua_i_bye:
		//sip_i_bye(nua, profile, nh, session, sip, tags);
		break;

	case nua_r_message:
		//sip_r_message(status, phrase, nua, profile, nh, session, sip, tags);
		break;

	case nua_i_message:
		//sip_i_message(nua, profile, nh, session, sip, tags);
		break;

	case nua_r_info:
		//sip_r_info(status, phrase, nua, profile, nh, session, sip, tags);
		break;

	case nua_i_info:
		//sip_i_info(nua, profile, nh, session, sip, tags);
		break;

	case nua_r_refer:
		//sip_r_refer(status, phrase, nua, profile, nh, session, sip, tags);
		break;

	case nua_i_refer:
		//sip_i_refer(nua, profile, nh, session, sip, tags);
		break;
     
	case nua_r_subscribe:
		//sip_r_subscribe(status, phrase, nua, profile, nh, session, sip, tags);
		break;

	case nua_r_unsubscribe:
		//sip_r_unsubscribe(status, phrase, nua, profile, nh, session, sip, tags);
		break;

	case nua_r_publish:
		//sip_r_publish(status, phrase, nua, profile, nh, session, sip, tags);
		break;
    
	case nua_r_notify:
		//sip_r_notify(status, phrase, nua, profile, nh, session, sip, tags);
		break;
     
	case nua_i_notify:
		//sip_i_notify(nua, profile, nh, session, sip, tags);
		break;

	case nua_i_cancel:
		//sip_i_cancel(nua, profile, nh, session, sip, tags);
		break;

	case nua_i_error:
		//sip_i_error(nua, profile, nh, session, status, phrase, tags);
		break;

	case nua_i_active:
	case nua_i_ack:
	case nua_i_terminated:
		break;

	default:
		if (status > 100)
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "%s: unknown event %d: %03d %s\n", 
							  nua_event_name (event), event, status, phrase);
		else
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "%s: unknown event %d\n", nua_event_name (event), event);
	
		//tl_print(stdout, "", tags);
		break;

	}

	if (session) {
		switch_core_session_rwunlock(session);
	}
}

#define REG_SECONDS 30
static void *SWITCH_THREAD_FUNC profile_thread_run(switch_thread_t *thread, void *obj)
{
	sofia_profile_t *profile = (sofia_profile_t *) obj;
	switch_memory_pool_t *pool;
	sip_alias_node_t *node;
	uint32_t loops = 0;


	profile->s_root = su_root_create(NULL);
	profile->nua = nua_create(profile->s_root, /* Event loop */
							  event_callback, /* Callback for processing events */
							  profile, /* Additional data to pass to callback */
							  NUTAG_URL(profile->url),
							  TAG_END()); /* Last tag should always finish the sequence */

	nua_set_params(profile->nua,
				   NUTAG_EARLY_MEDIA(1),				   
				   NUTAG_AUTOANSWER(0),
				   NUTAG_AUTOALERT(0),
				   NUTAG_ALLOW("REGISTER"),
				   SIPTAG_SUPPORTED_STR("100rel, precondition"),
				   TAG_END());
				   

	for (node = profile->aliases; node; node = node->next) {
		node->nua = nua_create(profile->s_root, /* Event loop */
							   event_callback, /* Callback for processing events */
							   profile, /* Additional data to pass to callback */
							   NUTAG_URL(node->url),
							   TAG_END()); /* Last tag should always finish the sequence */

		nua_set_params(node->nua,
					   NUTAG_EARLY_MEDIA(1),
					   NUTAG_AUTOANSWER(0),
					   NUTAG_ALLOW("REGISTER"),
					   SIPTAG_SUPPORTED_STR("100rel, precondition"),
					   NUTAG_AUTOALERT(0),
					   TAG_END());
		
	}


	if ((profile->db = switch_core_db_open_file(profile->dbname))) {
		switch_core_db_test_reactive(profile->db, "select * from sip_registrations", reg_sql);
	} else {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Cannot Open SQL Database!\n");
		return NULL;
	}

	switch_mutex_init(&profile->reg_mutex, SWITCH_MUTEX_NESTED, profile->pool);

	switch_mutex_lock(globals.mutex);
	globals.running = 1;
	switch_mutex_unlock(globals.mutex);

	loops = REG_SECONDS;

	while(globals.running == 1) {
		if (++loops >= REG_SECONDS) {
			check_expire(profile, time(NULL));
			loops = 0;
		}

		su_root_step(profile->s_root, 1000);
		//su_root_run(profile->s_root);
	}

	su_root_destroy(profile->s_root);

	pool = profile->pool;
	switch_core_destroy_memory_pool(&pool);
	switch_mutex_lock(globals.mutex);
	globals.running = 0;
	switch_mutex_unlock(globals.mutex);
	
	return NULL;
}

static void launch_profile_thread(sofia_profile_t *profile)
{
	switch_thread_t *thread;
	switch_threadattr_t *thd_attr = NULL;
	
	switch_threadattr_create(&thd_attr, profile->pool);
	switch_threadattr_detach_set(thd_attr, 1);
	switch_threadattr_stacksize_set(thd_attr, SWITCH_THREAD_STACKSIZE);
	switch_mutex_lock(globals.hash_mutex);
	switch_core_hash_insert(globals.profile_hash, profile->name, profile);
	switch_mutex_unlock(globals.hash_mutex);
	switch_thread_create(&thread, thd_attr, profile_thread_run, profile, profile->pool);
}



static switch_status_t config_sofia(int reload)
{
	char *cf = "sofia.conf";
	switch_xml_t cfg, xml = NULL, xprofile, param;
	switch_status_t status = SWITCH_STATUS_SUCCESS;

	if (!(xml = switch_xml_open_cfg(cf, &cfg, NULL))) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "open of %s failed\n", cf);
		status = SWITCH_STATUS_FALSE;
		goto done;
	}

	for (xprofile = switch_xml_child(cfg, "profile"); xprofile; xprofile = xprofile->next) {
		char *xprofilename = (char *) switch_xml_attr_soft(xprofile, "name");
		sofia_profile_t *profile;
		switch_memory_pool_t *pool = NULL;
		char url[512] = "";

		/* Setup the pool */
		if ((status = switch_core_new_memory_pool(&pool)) != SWITCH_STATUS_SUCCESS) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Memory Error!\n");
			goto done;
		}

		if (!(profile = (sofia_profile_t *) switch_core_alloc(pool, sizeof(*profile)))) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Memory Error!\n");
			goto done;
		}

		if (!xprofilename) {
			xprofilename = "unnamed";
		}
		

		

		profile->pool = pool;

		profile->name = switch_core_strdup(profile->pool, xprofilename);
		snprintf(url, sizeof(url), "sofia_reg_%s", xprofilename);
		profile->dbname = switch_core_strdup(profile->pool, url);

		profile->dtmf_duration = 100;		
		profile->codec_ms = 20;

		for (param = switch_xml_child(xprofile, "param"); param; param = param->next) {
			char *var = (char *) switch_xml_attr_soft(param, "name");
			char *val = (char *) switch_xml_attr_soft(param, "value");

			if (!strcmp(var, "debug")) {
				profile->debug = atoi(val);
			} else if (!strcmp(var, "use-rtp-timer") && switch_true(val)) {
			  	switch_set_flag(profile, TFLAG_TIMER);
			} else if (!strcmp(var, "rfc2833-pt")) {
                profile->te = (switch_payload_t) atoi(val);
			} else if (!strcmp(var, "sip-port")) {
				profile->sip_port = atoi(val);
			} else if (!strcmp(var, "vad")) {
				if (!strcasecmp(val, "in")) {
					switch_set_flag(profile, TFLAG_VAD_IN);
				} else if (!strcasecmp(val, "out")) {
					switch_set_flag(profile, TFLAG_VAD_OUT);
				} else if (!strcasecmp(val, "both")) {
					switch_set_flag(profile, TFLAG_VAD_IN);
					switch_set_flag(profile, TFLAG_VAD_OUT);
				} else {
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Invald option %s for VAD\n", val);
				}
			} else if (!strcmp(var, "ext-rtp-ip")) {
				profile->extrtpip = switch_core_strdup(profile->pool, val);
			} else if (!strcmp(var, "rtp-ip")) {
				profile->rtpip = switch_core_strdup(profile->pool, val);
			} else if (!strcmp(var, "sip-ip")) {
				profile->sipip = switch_core_strdup(profile->pool, val);
			} else if (!strcmp(var, "sip-domain")) {
				profile->sipdomain = switch_core_strdup(profile->pool, val);
			} else if (!strcmp(var, "rtp-timer-name")) {
				profile->timer_name = switch_core_strdup(profile->pool, val);
			} else if (!strcmp(var, "ext-sip-ip")) {
				profile->extsipip = switch_core_strdup(profile->pool, val);
			} else if (!strcmp(var, "bitpacking")) {
				if (!strcasecmp(val, "aal2")) {
					profile->codec_flags = SWITCH_CODEC_FLAG_AAL2;
				} 
			} else if (!strcmp(var, "username")) {
				profile->username = switch_core_strdup(profile->pool, val);
			} else if (!strcmp(var, "context")) {
				profile->context = switch_core_strdup(profile->pool, val);
			} else if (!strcmp(var, "alias")) {
				sip_alias_node_t *node;
				if ((node = switch_core_alloc(profile->pool, sizeof(*node)))) {
					if ((node->url = switch_core_strdup(profile->pool, val))) {
						node->next = profile->aliases;
						profile->aliases = node;
					}
				}
			} else if (!strcmp(var, "dialplan")) {
				profile->dialplan = switch_core_strdup(profile->pool, val);
			} else if (!strcmp(var, "max-calls")) {
				profile->max_calls = atoi(val);
			} else if (!strcmp(var, "codec-prefs")) {
				profile->codec_string = switch_core_strdup(profile->pool, val);
				profile->codec_order_last = switch_separate_string(profile->codec_string, ',', profile->codec_order, SWITCH_MAX_CODECS);
			} else if (!strcmp(var, "codec-ms")) {
				profile->codec_ms = atoi(val);
			} else if (!strcmp(var, "dtmf-duration")) {
				int dur = atoi(val);
				if (dur > 10 && dur < 8000) {
					profile->dtmf_duration = dur;
				} else {
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Duration out of bounds!\n");
				}
			}
		}

		if (switch_test_flag(profile, TFLAG_TIMER) && !profile->timer_name) {
			profile->timer_name = switch_core_strdup(profile->pool, "soft");			
		}

		if (!profile->username) {
			profile->username = switch_core_strdup(profile->pool, "FreeSWITCH");
		}

		if (!profile->rtpip) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "Setting ip to '127.0.0.1'\n");
			profile->rtpip = switch_core_strdup(profile->pool, "127.0.0.1");
		}

		if (!profile->sip_port) {
			profile->sip_port = 5060;
		}

		if (!profile->dialplan) {
			profile->dialplan = switch_core_strdup(profile->pool, "default");
		}

		if (!profile->sipdomain) {
			profile->sipdomain = switch_core_strdup(profile->pool, profile->sipip);
		}

		snprintf(url, sizeof(url), "sip:%s:%d", profile->sipip, profile->sip_port);
		profile->url = switch_core_strdup(profile->pool, url);
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "Started Profile %s [%s]\n", profile->name, url);
		launch_profile_thread(profile);

	}
	
 done:
	if (xml) {
		switch_xml_free(xml);
	}

	return status;

}

SWITCH_MOD_DECLARE(switch_status_t) switch_module_load(const switch_loadable_module_interface_t **module_interface, char *filename)
{

	if (switch_core_new_memory_pool(&module_pool) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "OH OH no pool\n");
		return SWITCH_STATUS_TERM;
	}

	memset(&globals, 0, sizeof(globals));
	switch_mutex_init(&globals.mutex, SWITCH_MUTEX_NESTED, module_pool);
	
	su_init();


	switch_core_hash_init(&globals.profile_hash, module_pool);
	switch_mutex_init(&globals.hash_mutex, SWITCH_MUTEX_NESTED, module_pool);

	config_sofia(0);


	/* connect my internal structure to the blank pointer passed to me */
	*module_interface = &sofia_module_interface;

	/* indicate that the module should continue to be loaded */
	return SWITCH_STATUS_SUCCESS;

}

SWITCH_MOD_DECLARE(switch_status_t) switch_module_shutdown(void)
{

	switch_mutex_lock(globals.mutex);
	if (globals.running == 1) {
		globals.running = -1;
	}
	switch_mutex_unlock(globals.mutex);

	while(globals.running) {
		switch_yield(1000);
	}

	su_deinit();

	return SWITCH_STATUS_SUCCESS;
}
