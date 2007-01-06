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
static const switch_state_handler_table_t noop_state_handler = {0};
struct outbound_reg;
typedef struct outbound_reg outbound_reg_t;

struct sofia_profile;
typedef struct sofia_profile sofia_profile_t;
#define NUA_MAGIC_T sofia_profile_t

struct sofia_private {
    char uuid[SWITCH_UUID_FORMATTED_LENGTH + 1];
	outbound_reg_t *oreg;
};

typedef struct sofia_private sofia_private_t;

struct private_object;
typedef struct private_object private_object_t;
#define NUA_HMAGIC_T sofia_private_t

#define MY_EVENT_REGISTER "sofia::register"
#define MY_EVENT_EXPIRE "sofia::expire"
#define MULTICAST_EVENT "multicast::event"
#define SOFIA_REPLACES_HEADER "_sofia_replaces_"
#define SOFIA_USER_AGENT "FreeSWITCH(mod_sofia)"
#define SOFIA_CHAT_PROTO "sip"

#include <sofia-sip/nua.h>
#include <sofia-sip/sip_status.h>
#include <sofia-sip/sdp.h>
#include <sofia-sip/sip_protos.h>
#include <sofia-sip/auth_module.h>
#include <sofia-sip/su_md5.h>
#include <sofia-sip/su_log.h>
#include <sofia-sip/nea.h>

extern su_log_t tport_log[];

static switch_frame_t silence_frame = { 0 };
static char silence_data[13] = "";


static char reg_sql[] =
"CREATE TABLE sip_registrations (\n"
"   user            VARCHAR(255),\n"
"   host            VARCHAR(255),\n"
"   contact         VARCHAR(1024),\n"
"   status          VARCHAR(255),\n"
"   rpid            VARCHAR(255),\n"
"   expires         INTEGER(8)"
");\n";


static char sub_sql[] =
"CREATE TABLE sip_subscriptions (\n"
"   proto           VARCHAR(255),\n"
"   user            VARCHAR(255),\n"
"   host            VARCHAR(255),\n"
"   sub_to_user     VARCHAR(255),\n"
"   sub_to_host     VARCHAR(255),\n"
"   event           VARCHAR(255),\n"
"   contact         VARCHAR(1024),\n"
"   call_id         VARCHAR(255),\n"
"   full_from       VARCHAR(255),\n"
"   full_via        VARCHAR(255),\n"
"   expires         INTEGER(8)"
");\n";


static char auth_sql[] =
"CREATE TABLE sip_authentication (\n"
"   user            VARCHAR(255),\n"
"   host            VARCHAR(255),\n"
"   passwd            VARCHAR(255),\n"
"   nonce           VARCHAR(255),\n"
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
	PFLAG_AUTH_CALLS = (1 << 0),
	PFLAG_BLIND_REG = (1 << 1),
	PFLAG_AUTH_ALL = (1 << 2),
	PFLAG_FULL_ID = (1 << 3),
	PFLAG_PRESENCE = (1 << 4)
} PFLAGS;

typedef enum {
	TFLAG_IO = (1 << 0),
	TFLAG_CHANGE_MEDIA = (1 << 1),
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
	TFLAG_READY = (1 << 15),
	TFLAG_REINVITE = (1 << 16),
	TFLAG_REFER = (1 << 17),
	TFLAG_NOHUP = (1 << 18),
	TFLAG_XFER = (1 << 19),
	TFLAG_NOMEDIA = (1 << 20),
	TFLAG_BUGGY_2833 = (1 << 21),
	TFLAG_SIP_HOLD = (1 << 22),
	TFLAG_INB_NOMEDIA = (1 << 23)
} TFLAGS;

static struct {
	switch_hash_t *profile_hash;
	switch_mutex_t *hash_mutex;
	uint32_t callid;
	int32_t running;
	switch_mutex_t *mutex;
} globals;

typedef enum {
	REG_FLAG_AUTHED = (1 << 0),
} reg_flags_t;

typedef enum {
	REG_STATE_UNREGED,
	REG_STATE_TRYING,
	REG_STATE_REGISTER,
	REG_STATE_REGED,
	REG_STATE_FAILED,
	REG_STATE_EXPIRED
} reg_state_t;

struct outbound_reg {
	sofia_private_t *sofia_private;
	nua_handle_t *nh;
	sofia_profile_t *profile;
	char *name;
	char *register_scheme;
	char *register_realm;
	char *register_username;
	char *register_password;
	char *register_from;
	char *register_to;
	char *register_proxy;
	char *expires_str;
	uint32_t freq;
	time_t expires;
	time_t retry;
	uint32_t flags;
	reg_state_t state;
	switch_memory_pool_t *pool;
	struct outbound_reg *next;
};


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
	unsigned int pflags;
	uint32_t max_calls;
	nua_t *nua;
	switch_memory_pool_t *pool;
	su_root_t *s_root;
	sip_alias_node_t *aliases;
	switch_payload_t te;
	uint32_t codec_flags;
	switch_mutex_t *ireg_mutex;
	switch_mutex_t *oreg_mutex;
	outbound_reg_t *registrations;
	su_home_t *home;
	switch_hash_t *profile_hash;
	switch_hash_t *chat_hash;
};


struct private_object {
	sofia_private_t *sofia_private;
	uint32_t flags;
	switch_payload_t agreed_pt;
	switch_core_session_t *session;
	switch_frame_t read_frame;
	const switch_codec_implementation_t *codecs[SWITCH_MAX_CODECS];
	int num_codecs;
	switch_codec_t read_codec;
	switch_codec_t write_codec;
	uint32_t codec_ms;
	switch_caller_profile_t *caller_profile;
	int32_t timestamp_send;
	//int32_t timestamp_recv;
	switch_rtp_t *rtp_session;
	int ssrc;
	//switch_time_t last_read;
	sofia_profile_t *profile;
	char *local_sdp_audio_ip;
	switch_port_t local_sdp_audio_port;
	char *remote_sdp_audio_ip;
	switch_port_t remote_sdp_audio_port;
	char *adv_sdp_audio_ip;
	switch_port_t adv_sdp_audio_port;
	char *proxy_sdp_audio_ip;
	switch_port_t proxy_sdp_audio_port;
	char *from_uri;
	char *to_uri;
	char *from_address;
	char *to_address;
	char *callid;
	char *far_end_contact;
	char *contact_url;
	char *from_str;
	char *rm_encoding;
	char *rm_fmtp;
	char *fmtp_out;
	char *remote_sdp_str;
	char *local_sdp_str;
	char *dest;
	char *key;
	char *xferto;
	char *kick;
	char *origin;
	char *hash_key;
	char *chat_from;
	char *chat_to;
	char *e_dest;
	unsigned long rm_rate;
	switch_payload_t pt;
	switch_mutex_t *flag_mutex;
	switch_payload_t te;
	nua_handle_t *nh;
	nua_handle_t *nh2;
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

static void set_local_sdp(private_object_t *tech_pvt, char *ip, uint32_t port, char *sr, int force);

static void tech_set_codecs(private_object_t *tech_pvt);

static void attach_private(switch_core_session_t *session,
                           sofia_profile_t *profile,
                           private_object_t *tech_pvt,
                           char *channame);

static void terminate_session(switch_core_session_t **session, switch_call_cause_t cause, int line);

static switch_status_t tech_choose_port(private_object_t *tech_pvt);

static void do_invite(switch_core_session_t *session);

static uint8_t negotiate_sdp(switch_core_session_t *session, sdp_session_t *sdp);

static char *get_auth_data(char *dbname, char *nonce, char *npassword, size_t len, switch_mutex_t *mutex);

static void establish_presence(sofia_profile_t *profile);

static void sip_i_state(int status,
                        char const *phrase,
                        nua_t *nua,
                        sofia_profile_t *profile,
                        nua_handle_t *nh,
						sofia_private_t *sofia_private,
                        sip_t const *sip,
                        tagi_t tags[]);


static void sip_i_refer(nua_t *nua,
						sofia_profile_t *profile,
						nua_handle_t *nh,
                        switch_core_session_t *session,
						sip_t const *sip,
						tagi_t tags[]);

static void sip_i_info(nua_t *nua,
                       sofia_profile_t *profile,
                       nua_handle_t *nh,
                       switch_core_session_t *session,
                       sip_t const *sip,
                       tagi_t tags[]);

static void sip_i_invite(nua_t *nua,
                         sofia_profile_t *profile,
                         nua_handle_t *nh,
						 sofia_private_t *sofia_private,
                         sip_t const *sip,
                         tagi_t tags[]);

static void sip_i_register(nua_t *nua,
						   sofia_profile_t *profile,
						   nua_handle_t *nh,
						   sofia_private_t *sofia_private,
						   sip_t const *sip,
						   tagi_t tags[]);

static void event_callback(nua_event_t   event,
                           int           status,
                           char const   *phrase,
                           nua_t        *nua,
                           sofia_profile_t  *profile,
                           nua_handle_t *nh,
						   sofia_private_t *sofia_private,
                           sip_t const  *sip,
                           tagi_t        tags[]);


static void *SWITCH_THREAD_FUNC profile_thread_run(switch_thread_t *thread, void *obj);

static void launch_profile_thread(sofia_profile_t *profile);

static switch_status_t config_sofia(int reload);

static switch_status_t chat_send(char *proto, char *from, char *to, char *subject, char *body, char *hint);

/* BODY OF THE MODULE */
/*************************************************************************************************************************************************************/

typedef enum {
	AUTH_OK,
	AUTH_FORBIDDEN,
	AUTH_STALE,
} auth_res_t;


static char *get_url_from_contact(char *buf, uint8_t dup)
{
	char *url = NULL, *e;


	if ((url = strchr(buf, '<')) && (e = strchr(url, '>'))) {
		url++;
		if (dup) {
			url = strdup(url);
			e = strchr(url, '>');
		}

		*e = '\0';
	}
	
	return url;
}


static auth_res_t parse_auth(sofia_profile_t *profile, sip_authorization_t const *authorization, char *regstr, char *np, size_t nplen)
{
	int index;
	char *cur;
	su_md5_t ctx;
	char uridigest[2 * SU_MD5_DIGEST_SIZE + 1];
	char bigdigest[2 * SU_MD5_DIGEST_SIZE + 1];
	char *nonce, *uri, *qop, *cnonce, *nc, *response, *input = NULL, *input2 = NULL;
	auth_res_t ret = AUTH_FORBIDDEN;
	char *npassword = NULL;
	int cnt = 0;
	nonce = uri = qop = cnonce = nc = response = NULL;

	if (authorization->au_params) {
		for(index = 0; (cur=(char*)authorization->au_params[index]); index++) {
			char *var, *val, *p, *work; 
			var = val = work = NULL;
			if ((work = strdup(cur))) {
				var = work;
				if ((val = strchr(var, '='))) {
					*val++ = '\0';
					while(*val == '"') {
						*val++ = '\0';
					}
					if ((p = strchr(val, '"'))) {
						*p = '\0';
					}

					if (!strcasecmp(var, "nonce")) {
						nonce = strdup(val);
						cnt++;
					} else if (!strcasecmp(var, "uri")) {
						uri = strdup(val);
						cnt++;
					} else if (!strcasecmp(var, "qop")) {
						qop = strdup(val);
						cnt++;
					} else if (!strcasecmp(var, "cnonce")) {
						cnonce = strdup(val);
						cnt++;
					} else if (!strcasecmp(var, "response")) {
						response = strdup(val);
						cnt++;
					} else if (!strcasecmp(var, "nc")) {
						nc = strdup(val);
						cnt++;
					}
				}
				
				free(work);
			}
		}
	}

	if (cnt != 6) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Invalid Authorization header!\n");
		goto end;
	}

	if (switch_strlen_zero(np)) {
		if (!get_auth_data(profile->dbname, nonce, np, nplen, profile->ireg_mutex)) {
			ret = AUTH_STALE;
			goto end;
		} 
	}

	npassword = np;

	if ((input = switch_mprintf("%s:%q", regstr, uri))) {
		su_md5_init(&ctx);
		su_md5_strupdate(&ctx, input);
		su_md5_hexdigest(&ctx, uridigest);
		su_md5_deinit(&ctx);
	}

	if ((input2 = switch_mprintf("%q:%q:%q:%q:%q:%q", npassword, nonce, nc, cnonce, qop, uridigest))) {
		memset(&ctx, 0, sizeof(ctx));
		su_md5_init(&ctx);
		su_md5_strupdate(&ctx, input2);
		su_md5_hexdigest(&ctx, bigdigest);
		su_md5_deinit(&ctx);

		if (!strcasecmp(bigdigest, response)) {
			ret = AUTH_OK;
		} else {
			ret = AUTH_FORBIDDEN;
		}
	}

 end:
	if (input) {
		switch_safe_free(input);
	}
	if (input2) {
		switch_safe_free(input2);
	}
	switch_safe_free(nonce);
	switch_safe_free(uri);
	switch_safe_free(qop);
	switch_safe_free(cnonce);
	switch_safe_free(nc);
	switch_safe_free(response);

	return ret;

}


static void execute_sql(char *dbname, char *sql, switch_mutex_t *mutex)
{
	switch_core_db_t *db;

	if (mutex) {
		switch_mutex_lock(mutex);
	}

	if (!(db = switch_core_db_open_file(dbname))) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Error Opening DB %s\n", dbname);
		goto end;
	}
	switch_core_db_persistant_execute(db, sql, 25);
	switch_core_db_close(db);

 end:
	if (mutex) {
		switch_mutex_unlock(mutex);
	}
}


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
			switch_event_add_header(s_event, SWITCH_STACK_BOTTOM, "profile-name", "%s", argv[0]);
			switch_event_add_header(s_event, SWITCH_STACK_BOTTOM, "user", "%s", argv[1]);
			switch_event_add_header(s_event, SWITCH_STACK_BOTTOM, "host", "%s", argv[2]);
			switch_event_add_header(s_event, SWITCH_STACK_BOTTOM, "contact", "%s", argv[3]);
			switch_event_add_header(s_event, SWITCH_STACK_BOTTOM, "expires", "%d", argv[4]);
			switch_event_fire(&s_event);
		}
	}
	return 0;
}

static void check_expire(switch_core_db_t *db, sofia_profile_t *profile, time_t now)
{
	char sql[1024];
	char *errmsg;

	if (!db) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Error Opening DB %s\n", profile->dbname);
		return;
	}

	switch_mutex_lock(profile->ireg_mutex);
	snprintf(sql, sizeof(sql), "select '%s',* from sip_registrations where expires > 0 and expires < %ld", profile->name, (long) now);	
	switch_core_db_exec(db, sql, del_callback, NULL, &errmsg);

	if (errmsg) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "SQL ERR [%s][%s]\n", sql, errmsg);
		switch_safe_free(errmsg);
		errmsg = NULL;
	}
	
	snprintf(sql, sizeof(sql), "delete from sip_registrations where expires > 0 and expires < %ld", (long) now);
	switch_core_db_persistant_execute(db, sql, 1000);
	snprintf(sql, sizeof(sql), "delete from sip_authentication where expires > 0 and expires < %ld", (long) now);
	switch_core_db_persistant_execute(db, sql, 1000);
	snprintf(sql, sizeof(sql), "delete from sip_subscriptions where expires > 0 and expires < %ld", (long) now);
	switch_core_db_persistant_execute(db, sql, 1000);

	switch_mutex_unlock(profile->ireg_mutex);

}

static char *find_reg_url(sofia_profile_t *profile, char *user, char *host, char *val, switch_size_t len)
{
	char *errmsg;
	struct callback_t cbt = {0};
	switch_core_db_t *db;

	if (!(db = switch_core_db_open_file(profile->dbname))) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Error Opening DB %s\n", profile->dbname);
		return NULL;
	}

	cbt.val = val;
	cbt.len = len;
	switch_mutex_lock(profile->ireg_mutex);
	if (host) {
		snprintf(val, len, "select contact from sip_registrations where user='%s' and host='%s'", user, host);	
	} else {
		snprintf(val, len, "select contact from sip_registrations where user='%s'", user);	
	}

	switch_core_db_exec(db, val, find_callback, &cbt, &errmsg);

	if (errmsg) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "SQL ERR [%s][%s]\n", val, errmsg);
		switch_safe_free(errmsg);
		errmsg = NULL;
	}

	switch_mutex_unlock(profile->ireg_mutex);

	switch_core_db_close(db);
    if (cbt.matches) {
        return val;
    } else {
        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "Cannot locate registered user %s@%s\n", user, host);
        return NULL;
    }
}


static void set_local_sdp(private_object_t *tech_pvt, char *ip, uint32_t port, char *sr, int force)
{
	char buf[1024];
	switch_time_t now = switch_time_now();

	if (!force && !ip && !sr && switch_test_flag(tech_pvt, TFLAG_NOMEDIA)) {
		return;
	}

	if (!ip) {
		if (!(ip = tech_pvt->adv_sdp_audio_ip)) {
			ip = tech_pvt->proxy_sdp_audio_ip;
		}
	}
	if (!port) {
		if (!(port = tech_pvt->adv_sdp_audio_port)) {
			port = tech_pvt->proxy_sdp_audio_port;
		}
	}
	if (!sr) {
		sr = "sendrecv";
	}

	snprintf(buf, sizeof(buf), 
			 "v=0\n"
			 "o=FreeSWITCH %d%"APR_TIME_T_FMT" %d%"APR_TIME_T_FMT" IN IP4 %s\n"
			 "s=FreeSWITCH\n"
			 "c=IN IP4 %s\n"
			 "t=0 0\n"
			 "a=%s\n"
			 "m=audio %d RTP/AVP",
			 port,
			 now,
			 port,
			 now,
			 ip,
			 ip,
			 sr,
			 port
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

	if (tech_pvt->te > 95) {
		snprintf(buf + strlen(buf), sizeof(buf) - strlen(buf), " %d", tech_pvt->te);
	}

	snprintf(buf + strlen(buf), sizeof(buf) - strlen(buf), "\n");

	if (tech_pvt->rm_encoding) {
		snprintf(buf + strlen(buf), sizeof(buf) - strlen(buf), "a=rtpmap:%d %s/%ld\n", tech_pvt->pt, tech_pvt->rm_encoding, tech_pvt->rm_rate);
		if (tech_pvt->fmtp_out) {
			snprintf(buf + strlen(buf), sizeof(buf) - strlen(buf), "a=fmtp:%d %s\n", tech_pvt->pt, tech_pvt->fmtp_out);
		}
		if (tech_pvt->read_codec.implementation) {
			snprintf(buf + strlen(buf), sizeof(buf) - strlen(buf), "a=ptime:%d\n", tech_pvt->read_codec.implementation->microseconds_per_frame / 1000);
		}

	} else if (tech_pvt->num_codecs) {
		int i;
		for (i = 0; i < tech_pvt->num_codecs; i++) {
			const switch_codec_implementation_t *imp = tech_pvt->codecs[i];
			snprintf(buf + strlen(buf), sizeof(buf) - strlen(buf), "a=rtpmap:%d %s/%d\n", imp->ianacode, imp->iananame, imp->samples_per_second);
			if (imp->fmtp) {
				snprintf(buf + strlen(buf), sizeof(buf) - strlen(buf), "a=fmtp:%d %s\n", imp->ianacode, imp->fmtp);
			}
			snprintf(buf + strlen(buf), sizeof(buf) - strlen(buf), "a=ptime:%d\n", imp->microseconds_per_frame / 1000);
		}
	}
	
	if (tech_pvt->te > 95) {
		snprintf(buf + strlen(buf), sizeof(buf) - strlen(buf), "a=rtpmap:%d telephone-event/8000\na=fmtp:%d 0-16\n", tech_pvt->te, tech_pvt->te);
	}

	tech_pvt->local_sdp_str = switch_core_session_strdup(tech_pvt->session, buf);
}

static void tech_set_codecs(private_object_t *tech_pvt)
{
    switch_channel_t *channel;
    char *codec_string = NULL;

	if (switch_test_flag(tech_pvt, TFLAG_NOMEDIA)) {
		return;
	}

	if (tech_pvt->num_codecs) {
		return;
	}

    assert(tech_pvt->session != NULL);

    channel = switch_core_session_get_channel(tech_pvt->session);
    assert (channel != NULL);

    if (!(codec_string = switch_channel_get_variable(channel, "codec_string"))) {
        codec_string = tech_pvt->profile->codec_string;
    }

	if (codec_string) {
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
	switch_channel_t *channel;
	switch_port_t sdp_port;
	char *err;
	char tmp[50];

	channel = switch_core_session_get_channel(tech_pvt->session);
	
	if (switch_test_flag(tech_pvt, TFLAG_NOMEDIA) || tech_pvt->adv_sdp_audio_port) {
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

	snprintf(tmp, sizeof(tmp), "%d", sdp_port);
	switch_channel_set_variable(channel, SWITCH_LOCAL_MEDIA_IP_VARIABLE, tech_pvt->adv_sdp_audio_ip);
	switch_channel_set_variable(channel, SWITCH_LOCAL_MEDIA_PORT_VARIABLE, tmp);
	

	return SWITCH_STATUS_SUCCESS;
}

static void do_invite(switch_core_session_t *session)
{
	char rpid[1024] = { 0 };
	char alert_info[1024] = { 0 };
	char *alertbuf;
	private_object_t *tech_pvt;
    switch_channel_t *channel = NULL;
	switch_caller_profile_t *caller_profile;
	char *cid_name, *cid_num;
	char *e_dest = NULL;
	char *holdstr = "";

    channel = switch_core_session_get_channel(session);
    assert(channel != NULL);

    tech_pvt = (private_object_t *) switch_core_session_get_private(session);
    assert(tech_pvt != NULL);

	caller_profile = switch_channel_get_caller_profile(channel);

	cid_name = (char *) caller_profile->caller_id_name;
	cid_num = (char *) caller_profile->caller_id_number;
	
	if ((tech_pvt->from_str = switch_mprintf("\"%s\" <sip:%s@%s>", 
													 cid_name,
													 cid_num,
													 tech_pvt->profile->sipip
													 ))) {

		char *rep = switch_channel_get_variable(channel, SOFIA_REPLACES_HEADER);

		if ((alertbuf = switch_channel_get_variable(channel, "alert_info"))) {
			snprintf(alert_info, sizeof(alert_info) - 1, "Alert-Info: %s", alertbuf);
		}

		tech_choose_port(tech_pvt);
		set_local_sdp(tech_pvt, NULL, 0, NULL, 0);

		switch_set_flag_locked(tech_pvt, TFLAG_READY);

		// forge a RPID for now KHR  -- Should wrap this in an if statement so it can be turned on and off
		if (switch_test_flag(caller_profile, SWITCH_CPF_SCREEN)) {
			char *priv = "off";
			char *screen = "no";
			if (switch_test_flag(caller_profile, SWITCH_CPF_HIDE_NAME)) {
				priv = "name";
				if (switch_test_flag(caller_profile, SWITCH_CPF_HIDE_NUMBER)) {
					priv = "yes";
				}
			} else if (switch_test_flag(caller_profile, SWITCH_CPF_HIDE_NUMBER)) {
				priv = "yes";
			}
			if (switch_test_flag(caller_profile, SWITCH_CPF_SCREEN)) {
				screen = "yes";
			}

			snprintf(rpid, sizeof(rpid) - 1, "Remote-Party-ID: %s;party=calling;screen=%s;privacy=%s", tech_pvt->from_str, screen, priv);
								
		}

		if (!tech_pvt->nh) {
			tech_pvt->nh = nua_handle(tech_pvt->profile->nua, NULL,
									  SIPTAG_TO_STR(tech_pvt->dest),
									  SIPTAG_FROM_STR(tech_pvt->from_str),
									  SIPTAG_CONTACT_STR(tech_pvt->profile->url),
									  TAG_END());

            if (!(tech_pvt->sofia_private = malloc(sizeof(*tech_pvt->sofia_private)))) {
                abort();
            }
            memset(tech_pvt->sofia_private, 0, sizeof(*tech_pvt->sofia_private));
			switch_copy_string(tech_pvt->sofia_private->uuid, switch_core_session_get_uuid(session), sizeof(tech_pvt->sofia_private->uuid));
			nua_handle_bind(tech_pvt->nh, tech_pvt->sofia_private);

		}


		if (tech_pvt->e_dest && (e_dest = strdup(tech_pvt->e_dest))) {
			char *user = e_dest, *host = NULL;
			char hash_key[256] = "";

			if ((host = strchr(user, '@'))) {
				*host++ = '\0';
			}
			snprintf(hash_key, sizeof(hash_key), "%s%s%s", user, host, cid_num);

			tech_pvt->chat_from = tech_pvt->from_str;
			tech_pvt->chat_to = tech_pvt->dest;
			tech_pvt->hash_key = switch_core_session_strdup(tech_pvt->session, hash_key);
			switch_core_hash_insert(tech_pvt->profile->chat_hash, tech_pvt->hash_key, tech_pvt);
			free(e_dest);			
		}

		holdstr = switch_test_flag(tech_pvt, TFLAG_SIP_HOLD) ? "*" : "";
		nua_invite(tech_pvt->nh,
				   TAG_IF(rpid, SIPTAG_HEADER_STR(rpid)),
				   TAG_IF(alert_info, SIPTAG_HEADER_STR(alert_info)),
				   //SIPTAG_CONTACT_STR(tech_pvt->profile->url),
				   SOATAG_USER_SDP_STR(tech_pvt->local_sdp_str),
				   SOATAG_RTP_SORT(SOA_RTP_SORT_REMOTE),
				   SOATAG_RTP_SELECT(SOA_RTP_SELECT_ALL),
				   TAG_IF(rep, SIPTAG_REPLACES_STR(rep)),
				   SOATAG_HOLD(holdstr),
				   TAG_END());

	} else {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Memory Error!\n");
	}
	
}



static void do_xfer_invite(switch_core_session_t *session)
{
	char rpid[1024];
	private_object_t *tech_pvt;
    switch_channel_t *channel = NULL;
	switch_caller_profile_t *caller_profile;

    channel = switch_core_session_get_channel(session);
    assert(channel != NULL);

    tech_pvt = (private_object_t *) switch_core_session_get_private(session);
    assert(tech_pvt != NULL);

	caller_profile = switch_channel_get_caller_profile(channel);

	

	if ((tech_pvt->from_str = switch_mprintf("\"%s\" <sip:%s@%s>", 
													 (char *) caller_profile->caller_id_name, 
													 (char *) caller_profile->caller_id_number,
													 tech_pvt->profile->sipip
													 ))) {

		char *rep = switch_channel_get_variable(channel, SOFIA_REPLACES_HEADER);

		tech_pvt->nh2 = nua_handle(tech_pvt->profile->nua, NULL,
								  SIPTAG_TO_STR(tech_pvt->dest),
								  SIPTAG_FROM_STR(tech_pvt->from_str),
								  SIPTAG_CONTACT_STR(tech_pvt->profile->url),
								  TAG_END());
			
        
		nua_handle_bind(tech_pvt->nh2, tech_pvt->sofia_private);

		nua_invite(tech_pvt->nh2,
				   TAG_IF(rpid, SIPTAG_HEADER_STR(rpid)),
				   SIPTAG_CONTACT_STR(tech_pvt->profile->url),
				   SOATAG_USER_SDP_STR(tech_pvt->local_sdp_str),
				   SOATAG_RTP_SORT(SOA_RTP_SORT_REMOTE),
				   SOATAG_RTP_SELECT(SOA_RTP_SELECT_ALL),
				   TAG_IF(rep, SIPTAG_REPLACES_STR(rep)),
				   TAG_END());
	} else {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Memory Error!\n");
	}
	
}

static void tech_absorb_sdp(private_object_t *tech_pvt)
{
	switch_channel_t *channel;
	char *sdp_str;

	channel = switch_core_session_get_channel(tech_pvt->session);
	assert(channel != NULL);
	
	if ((sdp_str = switch_channel_get_variable(channel, SWITCH_B_SDP_VARIABLE))) {
		sdp_parser_t *parser;
		sdp_session_t *sdp;
		sdp_media_t *m;

		if ((parser = sdp_parse(tech_pvt->home, sdp_str, (int)strlen(sdp_str), 0))) {
			if ((sdp = sdp_session(parser))) {
				for (m = sdp->sdp_media; m ; m = m->m_next) {
					tech_pvt->proxy_sdp_audio_ip = switch_core_session_strdup(tech_pvt->session, (char *)sdp->sdp_connection->c_address);
					tech_pvt->proxy_sdp_audio_port = (switch_port_t)m->m_port;
					if (tech_pvt->proxy_sdp_audio_ip && tech_pvt->proxy_sdp_audio_port) {
						break;
					}
				}
			}
			sdp_parser_free(parser);
		}	
		tech_pvt->local_sdp_str = switch_core_session_strdup(tech_pvt->session, sdp_str);
	}
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
	if (switch_channel_test_flag(channel, CF_NOMEDIA)) {
		switch_set_flag_locked(tech_pvt, TFLAG_NOMEDIA);
		tech_absorb_sdp(tech_pvt);
	}

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

// map QSIG cause codes to SIP from RFC4497 section 8.4.1
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

static switch_status_t sofia_on_hangup(switch_core_session_t *session)
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

	deactivate_rtp(tech_pvt);

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Channel %s hanging up, cause: %s\n", 
					  switch_channel_get_name(channel), switch_channel_cause2str(cause), sip_cause);

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

	if (tech_pvt->from_str) {
		switch_safe_free(tech_pvt->from_str);
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

static switch_status_t tech_set_codec(private_object_t *tech_pvt, int force)
{
	switch_channel_t *channel;

	if (tech_pvt->read_codec.implementation) {
		if (!force) {
			return SWITCH_STATUS_SUCCESS;
		}
		if (strcasecmp(tech_pvt->read_codec.implementation->iananame, tech_pvt->rm_encoding) ||
			tech_pvt->read_codec.implementation->samples_per_second != tech_pvt->rm_rate) {

			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Changing Codec from %s to %s\n",
							  tech_pvt->read_codec.implementation->iananame, tech_pvt->rm_encoding);
		switch_core_codec_destroy(&tech_pvt->read_codec);
		switch_core_codec_destroy(&tech_pvt->write_codec);
		switch_core_session_reset(tech_pvt->session);
		} else {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Already using %s\n",
							  tech_pvt->read_codec.implementation->iananame);
			return SWITCH_STATUS_SUCCESS;
		}
	}

	channel = switch_core_session_get_channel(tech_pvt->session);
	assert(channel != NULL);

	if (switch_core_codec_init(&tech_pvt->read_codec,  
							   tech_pvt->rm_encoding,
							   tech_pvt->rm_fmtp,
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
								   tech_pvt->rm_fmtp,
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
							  tech_pvt->rm_encoding, tech_pvt->rm_rate, tech_pvt->codec_ms);
			tech_pvt->read_frame.codec = &tech_pvt->read_codec;
				
			switch_core_session_set_read_codec(tech_pvt->session, &tech_pvt->read_codec);
			switch_core_session_set_write_codec(tech_pvt->session, &tech_pvt->write_codec);
			tech_pvt->fmtp_out = switch_core_session_strdup(tech_pvt->session, tech_pvt->write_codec.fmtp_out);
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
	char tmp[50];
	assert(tech_pvt != NULL);

	channel = switch_core_session_get_channel(tech_pvt->session);
	assert(channel != NULL);

	if (switch_test_flag(tech_pvt, TFLAG_NOMEDIA)) {
		return SWITCH_STATUS_SUCCESS;
	}

	if (switch_rtp_ready(tech_pvt->rtp_session) && !switch_test_flag(tech_pvt, TFLAG_REINVITE)) {
		return SWITCH_STATUS_SUCCESS;
	}

	if ((status = tech_set_codec(tech_pvt, 0)) != SWITCH_STATUS_SUCCESS) {
		return status;
	}
	
	bw = tech_pvt->read_codec.implementation->bits_per_second;
	ms = tech_pvt->read_codec.implementation->microseconds_per_frame;

	flags = (switch_rtp_flag_t) (SWITCH_RTP_FLAG_RAW_WRITE | SWITCH_RTP_FLAG_AUTOADJ | SWITCH_RTP_FLAG_DATAWAIT);

	if (switch_test_flag(tech_pvt, TFLAG_BUGGY_2833)) {
		flags |= SWITCH_RTP_FLAG_BUGGY_2833;
	}

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "RTP [%s] %s:%d->%s:%d codec: %u ms: %d\n",
					  switch_channel_get_name(channel),
					  tech_pvt->local_sdp_audio_ip,
					  tech_pvt->local_sdp_audio_port,
					  tech_pvt->remote_sdp_audio_ip,
					  tech_pvt->remote_sdp_audio_port,
					  tech_pvt->agreed_pt,
					  tech_pvt->read_codec.implementation->microseconds_per_frame / 1000);

	snprintf(tmp, sizeof(tmp), "%d", tech_pvt->remote_sdp_audio_port);
	switch_channel_set_variable(channel, SWITCH_LOCAL_MEDIA_IP_VARIABLE, tech_pvt->adv_sdp_audio_ip);
	switch_channel_set_variable(channel, SWITCH_LOCAL_MEDIA_PORT_VARIABLE, tmp);
	
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
            /* Reactivate the NAT buster flag. */
            switch_rtp_set_flag(tech_pvt->rtp_session, SWITCH_RTP_FLAG_AUTOADJ);
		}
		return SWITCH_STATUS_SUCCESS;
	}

	tech_pvt->rtp_session = switch_rtp_new(tech_pvt->local_sdp_audio_ip,
										   tech_pvt->local_sdp_audio_port,
										   tech_pvt->remote_sdp_audio_ip,
										   tech_pvt->remote_sdp_audio_port,
										   tech_pvt->agreed_pt,
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
            switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "RTP Engage VAD for %s ( %s %s )\n", 
                              switch_channel_get_name(switch_core_session_get_channel(tech_pvt->session)),
                              vad_in ? "in" : "", vad_out ? "out" : "");
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

	if (switch_channel_test_flag(channel, CF_NOMEDIA)) {
		switch_set_flag_locked(tech_pvt, TFLAG_NOMEDIA);
		tech_absorb_sdp(tech_pvt);
	}

	if (!switch_test_flag(tech_pvt, TFLAG_ANS) && !switch_channel_test_flag(channel, CF_OUTBOUND)) {
		switch_set_flag_locked(tech_pvt, TFLAG_ANS);


		tech_choose_port(tech_pvt);
		set_local_sdp(tech_pvt, NULL, 0, NULL, 0);
		activate_rtp(tech_pvt);
		
		if (tech_pvt->nh) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Local SDP %s:\n%s\n",
							  switch_channel_get_name(channel),
							  tech_pvt->local_sdp_str);
			nua_respond(tech_pvt->nh, SIP_200_OK, 
						SIPTAG_CONTACT_STR(tech_pvt->profile->url),
						SOATAG_USER_SDP_STR(tech_pvt->local_sdp_str),
						SOATAG_AUDIO_AUX("cn telephone-event"),
						NUTAG_INCLUDE_EXTRA_SDP(1),
						TAG_END());
			
		}
	} 

	return SWITCH_STATUS_SUCCESS;
}


static switch_status_t sofia_read_frame(switch_core_session_t *session, switch_frame_t **frame, int timeout,
										switch_io_flag_t flags, int stream_id)
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
		elapsed = (unsigned int)((switch_time_now() - tech_pvt->last_read) / 1000);
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
                size_t bytes = 0;
                int frames = 1;
				//tech_pvt->last_read = switch_time_now();
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


    switch(sig) {
    case SWITCH_SIG_BREAK:
        switch_rtp_set_flag(tech_pvt->rtp_session, SWITCH_RTP_FLAG_BREAK);
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
	case SWITCH_MESSAGE_INDICATE_NOMEDIA: {
		char *uuid;
		switch_core_session_t *other_session;
		switch_channel_t *other_channel;
		char *ip = NULL, *port = NULL;

		switch_set_flag_locked(tech_pvt, TFLAG_NOMEDIA);
		tech_pvt->local_sdp_str = NULL;
		if ((uuid = switch_channel_get_variable(channel, SWITCH_BRIDGE_VARIABLE)) && (other_session = switch_core_session_locate(uuid))) {
			other_channel = switch_core_session_get_channel(other_session);
			ip = switch_channel_get_variable(other_channel, SWITCH_REMOTE_MEDIA_IP_VARIABLE);
			port = switch_channel_get_variable(other_channel, SWITCH_REMOTE_MEDIA_PORT_VARIABLE);
			switch_core_session_rwunlock(other_session);
			if (ip && port) {
				set_local_sdp(tech_pvt, ip, atoi(port), NULL, 1);
			}
		}
		if (!tech_pvt->local_sdp_str) {
			tech_absorb_sdp(tech_pvt);
		}
		do_invite(session);
	}
		break;
	case SWITCH_MESSAGE_INDICATE_MEDIA: {
		switch_clear_flag_locked(tech_pvt, TFLAG_NOMEDIA);
		tech_pvt->local_sdp_str = NULL;
		if (!switch_rtp_ready(tech_pvt->rtp_session)) {
			tech_set_codecs(tech_pvt);
			tech_choose_port(tech_pvt);
		}
		set_local_sdp(tech_pvt, NULL, 0, NULL, 1);
		do_invite(session);
		while (!switch_rtp_ready(tech_pvt->rtp_session) && switch_channel_get_state(channel) < CS_HANGUP) {
			switch_yield(1000);
		}
	}
		break;

	case SWITCH_MESSAGE_INDICATE_HOLD: {
		switch_set_flag_locked(tech_pvt, TFLAG_SIP_HOLD);
		do_invite(session);
	}
		break;

	case SWITCH_MESSAGE_INDICATE_UNHOLD: {
		switch_clear_flag_locked(tech_pvt, TFLAG_SIP_HOLD);
		do_invite(session);
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
					activate_rtp(a_tech_pvt);
					
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
	case SWITCH_MESSAGE_INDICATE_RINGING:
		nua_respond(tech_pvt->nh, SIP_180_RINGING, SIPTAG_CONTACT_STR(tech_pvt->profile->url), TAG_END());
		break;
	case SWITCH_MESSAGE_INDICATE_PROGRESS: {
		struct private_object *tech_pvt;
	    switch_channel_t *channel = NULL;

	    channel = switch_core_session_get_channel(session);
	    assert(channel != NULL);

	    tech_pvt = switch_core_session_get_private(session);
	    assert(tech_pvt != NULL);

	    if (!switch_test_flag(tech_pvt, TFLAG_EARLY_MEDIA) && !switch_test_flag(tech_pvt, TFLAG_ANS)) {
			switch_set_flag_locked(tech_pvt, TFLAG_EARLY_MEDIA);
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "Asked to send early media by %s\n", msg->from);


			if (switch_test_flag(tech_pvt, TFLAG_NOMEDIA)) {
				tech_absorb_sdp(tech_pvt);
			}

			/* Transmit 183 Progress with SDP */
			tech_choose_port(tech_pvt);
			set_local_sdp(tech_pvt, NULL, 0, NULL, 0);
			activate_rtp(tech_pvt);
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "Ring SDP:\n%s\n", tech_pvt->local_sdp_str);

			nua_respond(tech_pvt->nh,
						SIP_183_SESSION_PROGRESS,
						SIPTAG_CONTACT_STR(tech_pvt->profile->url),
						SOATAG_USER_SDP_STR(tech_pvt->local_sdp_str),
						SOATAG_AUDIO_AUX("cn telephone-event"),
						TAG_END());
			
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
							NUTAG_URL(tech_pvt->chat_to),
							SIPTAG_TO_STR(tech_pvt->chat_to),
							SIPTAG_CONTACT_STR(tech_pvt->profile->url),
							TAG_END());

		nua_message(msg_nh,
					SIPTAG_CONTENT_TYPE_STR("text/html"),
					SIPTAG_PAYLOAD_STR(body),
					TAG_END());
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
	/*.receive_message*/ sofia_receive_message,
	/*.receive_event*/ sofia_receive_event
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

static const switch_chat_interface_t sofia_chat_interface = {
	/*.name */ SOFIA_CHAT_PROTO,
	/*.chat_send */ chat_send,
	
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
	/*.chat_interface */ &sofia_chat_interface
};


static void logger(void *logarg, char const *fmt, va_list ap)
{
	char *data = NULL;

	if (fmt) {
#ifdef HAVE_VASPRINTF
		int ret;
		ret = vasprintf(&data, fmt, ap);
		if ((ret == -1) || !data) {
			return;
		}
#else
		data = (char *) malloc(2048);
		if (data) {
			vsnprintf(data, 2048, fmt, ap);
		} else { 
			return;
		}
#endif
	}
	
	switch_log_printf(SWITCH_CHANNEL_LOG_CLEAN, SWITCH_LOG_CONSOLE, (char*) "%s", data);
	free(data);
}


static switch_status_t sofia_outgoing_channel(switch_core_session_t *session, switch_caller_profile_t *outbound_profile,
											  switch_core_session_t **new_session, switch_memory_pool_t *pool)
{
	switch_status_t status = SWITCH_STATUS_FALSE;
	switch_core_session_t *nsession;
	char *data, *profile_name, *dest;
	sofia_profile_t *profile;
	switch_caller_profile_t *caller_profile = NULL;
	private_object_t *tech_pvt = NULL;
	switch_channel_t *nchannel;
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
		*host = '@';
		tech_pvt->e_dest = switch_core_session_strdup(nsession, dest);
		*host++ = '\0';
		if (find_reg_url(profile, dest, host, buf, sizeof(buf))) {
			tech_pvt->dest = switch_core_session_strdup(nsession, buf);
			
		} else {
			terminate_session(&nsession, SWITCH_CAUSE_NO_ROUTE_DESTINATION, __LINE__);
			goto done;
		}
	} else if (!strchr(dest, '@')) {
		char buf[128];
		tech_pvt->e_dest = switch_core_session_strdup(nsession, dest);
		if (find_reg_url(profile, dest, profile_name, buf, sizeof(buf))) {
            tech_pvt->dest = switch_core_session_strdup(nsession, buf);

        } else {
            terminate_session(&nsession, SWITCH_CAUSE_NO_ROUTE_DESTINATION, __LINE__);
            goto done;
        }
	} else {
		tech_pvt->dest = switch_core_session_alloc(nsession, strlen(dest) + 5);
		snprintf(tech_pvt->dest, strlen(dest) + 5, "sip:%s", dest);
	}

	attach_private(nsession, profile, tech_pvt, dest);

	nchannel = switch_core_session_get_channel(nsession);
	caller_profile = switch_caller_profile_clone(nsession, outbound_profile);
	switch_channel_set_caller_profile(nchannel, caller_profile);
	switch_channel_set_flag(nchannel, CF_OUTBOUND);
	switch_set_flag_locked(tech_pvt, TFLAG_OUTBOUND);
	switch_channel_set_state(nchannel, CS_INIT);
	switch_channel_set_variable(nchannel, "endpoint_disposition", "OUTBOUND");
	*new_session = nsession;
	status = SWITCH_STATUS_SUCCESS;
	if (session) {
		//char *val;
		//switch_channel_t *channel = switch_core_session_get_channel(session);
		switch_ivr_transfer_variable(session, nsession, SOFIA_REPLACES_HEADER);
	}

 done:
	return status;
}


static uint8_t negotiate_sdp(switch_core_session_t *session, sdp_session_t *sdp)
{
	uint8_t match = 0;
	private_object_t *tech_pvt;
	sdp_media_t *m;
	sdp_attribute_t *a;
	switch_channel_t *channel;

	tech_pvt = switch_core_session_get_private(session);
	assert(tech_pvt != NULL);                                                                                                                               

	channel = switch_core_session_get_channel(session);
	
	if ((tech_pvt->origin = switch_core_session_strdup(session, (char *) sdp->sdp_origin->o_username))) {
		if (strstr(tech_pvt->origin, "CiscoSystemsSIP-GW-UserAgent")) {
			switch_set_flag_locked(tech_pvt, TFLAG_BUGGY_2833);
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Activate Buggy RFC2833 Mode!\n");
		}
	}

	for (a = sdp->sdp_attributes; a; a = a->a_next) {
		if (!strcasecmp(a->a_name, "sendonly")) {
			switch_set_flag_locked(tech_pvt, TFLAG_SIP_HOLD);
		} else if (!strcasecmp(a->a_name, "sendrecv")) {
			switch_clear_flag_locked(tech_pvt, TFLAG_SIP_HOLD);
		}
	}

	for (m = sdp->sdp_media; m ; m = m->m_next) {
		if (m->m_type == sdp_media_audio) {
			sdp_rtpmap_t *map;

			for (map = m->m_rtpmaps; map; map = map->rm_next) {
				int32_t i;
				
				if (!strcasecmp(map->rm_encoding, "telephone-event")) {
					tech_pvt->te = (switch_payload_t)map->rm_pt;
				}
				
				for (i = 0; i < tech_pvt->num_codecs; i++) {
					const switch_codec_implementation_t *imp = tech_pvt->codecs[i];
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Codec Compare [%s:%d]/[%s:%d]\n", 
									  map->rm_encoding, map->rm_pt, imp->iananame, imp->ianacode);
					if (map->rm_pt < 96) {
						match = (map->rm_pt == imp->ianacode) ? 1 : 0;
					} else {
						match = strcasecmp(map->rm_encoding, imp->iananame) ? 0 : 1;
					}

					if (match && (map->rm_rate == imp->samples_per_second)) {
						char tmp[50];
						tech_pvt->rm_encoding = switch_core_session_strdup(session, (char *)map->rm_encoding);
						tech_pvt->pt = (switch_payload_t)map->rm_pt;
						tech_pvt->rm_rate = map->rm_rate;
						tech_pvt->codec_ms = imp->microseconds_per_frame / 1000;
						tech_pvt->remote_sdp_audio_ip = switch_core_session_strdup(session, (char *)sdp->sdp_connection->c_address);
						tech_pvt->rm_fmtp = switch_core_session_strdup(session, (char *)map->rm_fmtp);
						tech_pvt->remote_sdp_audio_port = (switch_port_t)m->m_port;
						tech_pvt->agreed_pt = (switch_payload_t)map->rm_pt;
						snprintf(tmp, sizeof(tmp), "%d", tech_pvt->remote_sdp_audio_port);
						switch_channel_set_variable(channel, SWITCH_REMOTE_MEDIA_IP_VARIABLE, tech_pvt->remote_sdp_audio_ip);
						switch_channel_set_variable(channel, SWITCH_REMOTE_MEDIA_PORT_VARIABLE, tmp);
						break;
					} else {
						match = 0;
					}
				}

				if (match) {
					if (tech_set_codec(tech_pvt, 1) != SWITCH_STATUS_SUCCESS) {
						match = 0;
					}
					break;
				}
			}
		}
	}

	return match;
}

// map sip responses to QSIG cause codes ala RFC4497 section 8.4.4
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
	case 405:
		return SWITCH_CAUSE_SERVICE_UNAVAILABLE;
	case 406:
	case 415:
	case 501:
		return SWITCH_CAUSE_SERVICE_NOT_IMPLEMENTED;
	case 482:
	case 483:
		return SWITCH_CAUSE_EXCHANGE_ROUTING_ERROR;
	case 487:
		return SWITCH_CAUSE_ORIGINATOR_CANCEL;

	default: 
		return SWITCH_CAUSE_NORMAL_UNSPECIFIED;

	}
}


static void set_hash_key(char *hash_key, int32_t len, sip_t const *sip)
{

	snprintf(hash_key, len, "%s%s%s",
			 (char *) sip->sip_from->a_url->url_user,
			 (char *) sip->sip_from->a_url->url_host,
			 (char *) sip->sip_to->a_url->url_user
			 );	


#if 0
	/* nicer one we cant use in both directions >=0 */
	snprintf(hash_key, len, "%s%s%s%s%s%s",
			 (char *) sip->sip_to->a_url->url_user,
			 (char *) sip->sip_to->a_url->url_host,
			 (char *) sip->sip_to->a_url->url_params,
			 
			 (char *) sip->sip_from->a_url->url_user,
			 (char *) sip->sip_from->a_url->url_host,
			 (char *) sip->sip_from->a_url->url_params
			 );	
#endif

}

static void set_chat_hash(private_object_t *tech_pvt, sip_t const *sip)
{
	char hash_key[256] = "";
	char buf[512];

	if (!sip || tech_pvt->hash_key) {
		return;
	}

	if (find_reg_url(tech_pvt->profile, (char *) sip->sip_from->a_url->url_user, (char *) sip->sip_from->a_url->url_host, buf, sizeof(buf))) {
		tech_pvt->chat_from = sip_header_as_string(tech_pvt->home, (void *)sip->sip_to);
		tech_pvt->chat_to = switch_core_session_strdup(tech_pvt->session, buf);
		set_hash_key(hash_key, sizeof(hash_key), sip);
	} else {
		return;
	}

	

	tech_pvt->hash_key = switch_core_session_strdup(tech_pvt->session, hash_key);
	switch_core_hash_insert(tech_pvt->profile->chat_hash, tech_pvt->hash_key, tech_pvt);

}


static void sip_i_message(int status,
						char const *phrase, 
						nua_t *nua,
						sofia_profile_t *profile,
						nua_handle_t *nh,
						sofia_private_t *sofia_private,
						sip_t const *sip,
						tagi_t tags[])
{
	if (sip) {
		sip_from_t const *from = sip->sip_from;
		char *from_user = NULL;
		char *from_host = NULL;
		sip_to_t const *to = sip->sip_to;
		char *to_user = NULL;
		char *to_host = NULL;
		sip_subject_t const *sip_subject = sip->sip_subject;
		sip_payload_t *payload = sip->sip_payload;
		const char *subject = "n/a";
		char *msg = NULL;

		if (sip->sip_content_type) {
			if (strstr((char*)sip->sip_content_type->c_subtype, "composing")) {
				return;
			}
		}

		if (from) {
			from_user = (char *) from->a_url->url_user;
			from_host = (char *) from->a_url->url_host;
		}

		if (to) {
			to_user = (char *) to->a_url->url_user;
			to_host = (char *) to->a_url->url_host;
		}

        
		if (!to_user) {
			return;
		}

		if (payload) {
			msg = payload->pl_data;
		}

		if (sip_subject) {
			subject = sip_subject->g_value;
		}

		if (nh) {			
			char hash_key[512];
			private_object_t *tech_pvt;
			switch_channel_t *channel;
			switch_event_t *event;
			char *to_addr;
			char *from_addr;
			char *p;
			char *full_from;
			char proto[512] = SOFIA_CHAT_PROTO;
			
			full_from = sip_header_as_string(profile->home, (void *)sip->sip_from);

			if ((p=strchr(to_user, '+'))) {
				switch_copy_string(proto, to_user, sizeof(proto));
				p = strchr(proto, '+');
				*p++ = '\0';
				
				if ((to_addr = strdup(p))) {
					if((p = strchr(to_addr, '+'))) {
						*p = '@';
					}
				}
				
			} else {
				to_addr = switch_mprintf("%s@%s", to_user, to_host);
			}

			from_addr = switch_mprintf("%s@%s", from_user, from_host);


			set_hash_key(hash_key, sizeof(hash_key), sip);
			if ((tech_pvt = (private_object_t *) switch_core_hash_find(profile->chat_hash, hash_key))) {
				channel = switch_core_session_get_channel(tech_pvt->session);
				if (switch_event_create(&event, SWITCH_EVENT_MESSAGE) == SWITCH_STATUS_SUCCESS) {
					switch_event_add_header(event, SWITCH_STACK_BOTTOM, "proto", SOFIA_CHAT_PROTO);
					switch_event_add_header(event, SWITCH_STACK_BOTTOM, "login", "%s", profile->url);
					switch_event_add_header(event, SWITCH_STACK_BOTTOM, "from", "%s", tech_pvt->hash_key);
					switch_event_add_header(event, SWITCH_STACK_BOTTOM, "to", "%s", to_addr);
					switch_event_add_header(event, SWITCH_STACK_BOTTOM, "subject", "SIMPLE MESSAGE");
					if (msg) {
						switch_event_add_body(event, msg);
					}
					if (switch_core_session_queue_event(tech_pvt->session, &event) != SWITCH_STATUS_SUCCESS) {
						switch_event_add_header(event, SWITCH_STACK_BOTTOM, "delivery-failure", "true");
						switch_event_fire(&event);
					}
				}
			} else {
				switch_chat_interface_t *ci;
				
				if ((ci = switch_loadable_module_get_chat_interface(proto))) {
					ci->chat_send(SOFIA_CHAT_PROTO, from_addr, to_addr, "", msg, full_from);
				} else {
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Invaid Chat Interface [%s]!\n", proto);
				}
				
			}
			switch_safe_free(to_addr);
			switch_safe_free(from_addr);
			if (full_from) {
				su_free(profile->home, full_from);
			}
		}

	}
}

static void pass_sdp(private_object_t *tech_pvt, char *sdp) 
{
	char *val;
	switch_channel_t *channel;
	switch_core_session_t *other_session;
	switch_channel_t *other_channel;
	
	channel = switch_core_session_get_channel(tech_pvt->session);
	assert(channel != NULL);
	
	if ((val = switch_channel_get_variable(channel, SWITCH_ORIGINATOR_VARIABLE)) && (other_session = switch_core_session_locate(val))) {
		other_channel = switch_core_session_get_channel(other_session);
		assert(other_channel != NULL);
		if (!switch_channel_get_variable(other_channel, SWITCH_B_SDP_VARIABLE)) {
			switch_channel_set_variable(other_channel, SWITCH_B_SDP_VARIABLE, sdp);
		}

		if (!switch_test_flag(tech_pvt, TFLAG_CHANGE_MEDIA) && (
			switch_channel_test_flag(other_channel, CF_OUTBOUND) && 
			//switch_channel_test_flag(other_channel, CF_NOMEDIA) && 
			switch_channel_test_flag(channel, CF_OUTBOUND) && 
			switch_channel_test_flag(channel, CF_NOMEDIA))) {
			switch_ivr_nomedia(val, SMF_FORCE);
			switch_set_flag_locked(tech_pvt, TFLAG_CHANGE_MEDIA);
		}
		
		switch_core_session_rwunlock(other_session);
	}
}


static void sip_i_state(int status,
						char const *phrase, 
						nua_t *nua,
						sofia_profile_t *profile,
						nua_handle_t *nh,
						sofia_private_t *sofia_private,
						sip_t const *sip,
						tagi_t tags[])
	 
{
	char const *l_sdp = NULL, *r_sdp = NULL;
	int offer_recv = 0, answer_recv = 0, offer_sent = 0, answer_sent = 0;
	int ss_state = nua_callstate_init;
	switch_channel_t *channel = NULL;
	private_object_t *tech_pvt = NULL;
	switch_core_session_t *session = NULL;
	const char *replaces_str = NULL;
	char *uuid;
	switch_core_session_t *other_session = NULL;
	switch_channel_t *other_channel = NULL;
	char st[80] = "";


    if (sofia_private) {
        if (!switch_strlen_zero(sofia_private->uuid)) {
            if (!(session = switch_core_session_locate(sofia_private->uuid))) {
                /* too late */
                return;            
            }
        }
    }

	
	tl_gets(tags, 
			NUTAG_CALLSTATE_REF(ss_state),
			NUTAG_OFFER_RECV_REF(offer_recv),
			NUTAG_ANSWER_RECV_REF(answer_recv),
			NUTAG_OFFER_SENT_REF(offer_sent),
			NUTAG_ANSWER_SENT_REF(answer_sent),
			SIPTAG_REPLACES_STR_REF(replaces_str),
			SOATAG_LOCAL_SDP_STR_REF(l_sdp),
			SOATAG_REMOTE_SDP_STR_REF(r_sdp),
			TAG_END()); 

	if (session) {
		channel = switch_core_session_get_channel(session);
		assert(channel != NULL);

		tech_pvt = switch_core_session_get_private(session);
		assert(tech_pvt != NULL);
		
		tech_pvt->nh = nh;
		
		if (switch_channel_test_flag(channel, CF_NOMEDIA)) {
            switch_set_flag(tech_pvt, TFLAG_NOMEDIA);
        }

		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Channel %s entering state [%s]\n", 
						  switch_channel_get_name(channel),
						  nua_callstate_name(ss_state));

		if (r_sdp) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Remote SDP:\n%s\n", r_sdp);			
			tech_pvt->remote_sdp_str = switch_core_session_strdup(session, (char *)r_sdp);
			switch_channel_set_variable(channel, SWITCH_R_SDP_VARIABLE, (char *) r_sdp);
			pass_sdp(tech_pvt, (char *) r_sdp);

		}
	}

	if (status == 988) {
		goto done;
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
			if (status == 180 && !(switch_channel_test_flag(channel, CF_NO_INDICATE))) {
				if (switch_test_flag(tech_pvt, TFLAG_NOMEDIA)) {
					if ((uuid = switch_channel_get_variable(channel, SWITCH_BRIDGE_VARIABLE)) && (other_session = switch_core_session_locate(uuid))) {
						switch_core_session_message_t msg;
						msg.message_id = SWITCH_MESSAGE_INDICATE_RINGING;
						msg.from = __FILE__;
						switch_core_session_receive_message(other_session, &msg);
						switch_core_session_rwunlock(other_session);
					}
					
				} else {
					switch_core_session_message_t *msg;
					if ((msg = malloc(sizeof(*msg)))) {
						memset(msg, 0, sizeof(*msg));
						msg->message_id = SWITCH_MESSAGE_INDICATE_RINGING;
						msg->from = __FILE__;
						switch_core_session_queue_message(session, msg);
						switch_set_flag(msg, SCSMF_DYNAMIC);
					}
				}
			}
			if (r_sdp) {
				if (switch_test_flag(tech_pvt, TFLAG_NOMEDIA)) {
					switch_set_flag_locked(tech_pvt, TFLAG_EARLY_MEDIA);
					switch_channel_set_flag(channel, CF_EARLY_MEDIA);
					if ((uuid = switch_channel_get_variable(channel, SWITCH_BRIDGE_VARIABLE)) && (other_session = switch_core_session_locate(uuid))) {
						other_channel = switch_core_session_get_channel(other_session);
						switch_channel_pre_answer(other_channel);
						switch_core_session_rwunlock(other_session);
					}
                    goto done;
				} else {
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
						switch_set_flag_locked(tech_pvt, TFLAG_EARLY_MEDIA);
						switch_channel_set_flag(channel, CF_EARLY_MEDIA);
                        goto done;
					}
					switch_channel_set_variable(channel, "endpoint_disposition", "NO CODECS");
					nua_respond(nh, SIP_488_NOT_ACCEPTABLE, 
								TAG_END());
				}
			}
		}
		break;
	case nua_callstate_completing:
		nua_ack(nh, TAG_END());
		break;
	case nua_callstate_received: 
		if (session && switch_core_session_running(session)) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "Re-Entering Call State Received!\n");
            goto done;
		}

		if (channel) {
			if (r_sdp) {
				if (switch_test_flag(tech_pvt, TFLAG_NOMEDIA)) {
					switch_channel_set_variable(channel, "endpoint_disposition", "RECEIVED_NOMEDIA");
					switch_channel_set_state(channel, CS_INIT);
					switch_set_flag_locked(tech_pvt, TFLAG_READY);
					switch_core_session_thread_launch(session);
                    goto done;
				} else {
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
						nua_handle_t *bnh;
						sip_replaces_t *replaces;
						switch_channel_set_variable(channel, "endpoint_disposition", "RECEIVED");
						switch_channel_set_state(channel, CS_INIT);
						switch_set_flag_locked(tech_pvt, TFLAG_READY);

						switch_core_session_thread_launch(session);
						
						if (replaces_str && (replaces = sip_replaces_make(tech_pvt->home, replaces_str)) && (bnh = nua_handle_by_replaces(nua, replaces))) {
							sofia_private_t *b_private;

							switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Processing Replaces Attended Transfer\n");
							while (switch_channel_get_state(channel) < CS_EXECUTE) {
								switch_yield(10000);
							}

							if ((b_private = nua_handle_magic(bnh))) {
								char *br_b = switch_channel_get_variable(channel, SWITCH_BRIDGE_VARIABLE);
								char *br_a = b_private->uuid;

								if (br_b) {
									switch_ivr_uuid_bridge(br_a, br_b);
									switch_channel_set_variable(channel, "endpoint_disposition", "ATTENDED_TRANSFER");
									switch_channel_hangup(channel, SWITCH_CAUSE_ATTENDED_TRANSFER);
								} else {
									switch_channel_set_variable(channel, "endpoint_disposition", "ATTENDED_TRANSFER_ERROR");
									switch_channel_hangup(channel, SWITCH_CAUSE_DESTINATION_OUT_OF_ORDER);
								}
							} else {
								switch_channel_set_variable(channel, "endpoint_disposition", "ATTENDED_TRANSFER_ERROR");
								switch_channel_hangup(channel, SWITCH_CAUSE_DESTINATION_OUT_OF_ORDER);
							}
							nua_handle_unref(bnh);
						}
                        goto done;
					}

					switch_channel_set_variable(channel, "endpoint_disposition", "NO CODECS");
					nua_respond(nh, SIP_488_NOT_ACCEPTABLE, 
								TAG_END());
				}
			}
		}

		break;		
	case nua_callstate_early:
		break;
	case nua_callstate_completed:
		if (tech_pvt && r_sdp) {
			if (r_sdp) {
				if (switch_test_flag(tech_pvt, TFLAG_NOMEDIA)) {
                    goto done;
				} else {
					sdp_parser_t *parser = sdp_parse(tech_pvt->home, r_sdp, (int)strlen(r_sdp), 0);
					sdp_session_t *sdp;
					uint8_t match = 0;

					if (tech_pvt->num_codecs) {
						if ((sdp = sdp_session(parser))) {
							match = negotiate_sdp(session, sdp);
						}
					}
					if (match) {
						tech_choose_port(tech_pvt);
						set_local_sdp(tech_pvt, NULL, 0, NULL, 0);
						switch_set_flag_locked(tech_pvt, TFLAG_REINVITE);
						activate_rtp(tech_pvt);
						switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Processing Reinvite\n");
						if (parser) {
							sdp_parser_free(parser);
						}
					}
				}
			}
		}
		break;
	case nua_callstate_ready:
		if (tech_pvt && nh == tech_pvt->nh2) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Cheater Reinvite!\n");
			switch_set_flag_locked(tech_pvt, TFLAG_REINVITE);
			tech_pvt->nh = tech_pvt->nh2;
			tech_pvt->nh2 = NULL;
			tech_choose_port(tech_pvt);
			activate_rtp(tech_pvt);
			goto done;
		}

		if (channel) {
			if (r_sdp) {
				if (switch_test_flag(tech_pvt, TFLAG_NOMEDIA)) {
					switch_set_flag_locked(tech_pvt, TFLAG_ANS);
                    switch_channel_mark_answered(channel);
					if ((uuid = switch_channel_get_variable(channel, SWITCH_BRIDGE_VARIABLE)) && (other_session = switch_core_session_locate(uuid))) {
						other_channel = switch_core_session_get_channel(other_session);
						switch_channel_answer(other_channel);
						switch_core_session_rwunlock(other_session);
					}
					goto done;
				} else {
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
                        switch_channel_mark_answered(channel);
						goto done;
					}
					
					switch_channel_set_variable(channel, "endpoint_disposition", "NO CODECS");
					nua_respond(nh, SIP_488_NOT_ACCEPTABLE, TAG_END());
				}
			} else if (switch_test_flag(tech_pvt, TFLAG_EARLY_MEDIA)) {
				switch_set_flag_locked(tech_pvt, TFLAG_ANS);
				switch_channel_set_variable(channel, "endpoint_disposition", "ANSWER");
                switch_channel_mark_answered(channel);
                if ((uuid = switch_channel_get_variable(channel, SWITCH_BRIDGE_VARIABLE)) && (other_session = switch_core_session_locate(uuid))) {
                    other_channel = switch_core_session_get_channel(other_session);
                    switch_channel_answer(other_channel);
                    switch_core_session_rwunlock(other_session);
                }
				goto done;
			} //else probably an ack
		}
		
		break;
	case nua_callstate_terminating:
		break;
	case nua_callstate_terminated: 
		if (session) {
			if (!switch_test_flag(tech_pvt, TFLAG_BYE)) {
                
				switch_set_flag_locked(tech_pvt, TFLAG_BYE);
				if (switch_test_flag(tech_pvt, TFLAG_NOHUP)) {
					switch_clear_flag_locked(tech_pvt, TFLAG_NOHUP);
				} else {
					snprintf(st, sizeof(st), "%d", status);
					switch_channel_set_variable(channel, "sip_term_status", st);
					terminate_session(&session, sip_cause_to_freeswitch(status), __LINE__);
				}
			}

            if (tech_pvt->sofia_private) {
                free(tech_pvt->sofia_private);
                tech_pvt->sofia_private = NULL;
            }
			tech_pvt->nh = NULL;
		} else if (sofia_private) {
            free(sofia_private);
        }

		if (nh) {
            nua_handle_bind(nh, NULL);
			nua_handle_destroy(nh);
		}
		break;
	}

 done:

    if (session) {
        switch_core_session_rwunlock(session);
    }
}


static char *get_auth_data(char *dbname, char *nonce, char *npassword, size_t len, switch_mutex_t *mutex)
{
	switch_core_db_t *db;
	switch_core_db_stmt_t *stmt;
	char *sql = NULL, *ret = NULL;

	if (mutex) {
		switch_mutex_lock(mutex);
	}

	if (!(db = switch_core_db_open_file(dbname))) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Error Opening DB %s\n", dbname);
		goto end;
	}

	sql = switch_mprintf("select passwd from sip_authentication where nonce='%q'", nonce);
	if (switch_core_db_prepare(db, sql, -1, &stmt, 0)) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Statement Error!\n");
		goto fail;
	} else {
		int running = 1;
		int colcount;

		while (running < 5000) {
			int result = switch_core_db_step(stmt);

			if (result == SQLITE_ROW) {
				if ((colcount = switch_core_db_column_count(stmt))) {
					switch_copy_string(npassword, (char *)switch_core_db_column_text(stmt, 0), len);
					ret = npassword;
				}
				break;
			} else if (result == SQLITE_BUSY) {
				running++;
				switch_yield(1000);
				continue;
			}
			break;
		}
			
		switch_core_db_finalize(stmt);
	}
	

 fail:

	switch_core_db_close(db);

 end:
	if (mutex) {
		switch_mutex_unlock(mutex);
	}

	if (sql) {
		switch_safe_free(sql);
	}
	
	return ret;
}


typedef enum {
	REG_REGISTER,
	REG_INVITE
} sofia_regtype_t;

static uint8_t handle_register(nua_t *nua,
							   sofia_profile_t *profile,
							   nua_handle_t *nh,
							   sip_t const *sip,
							   sofia_regtype_t regtype,
							   char *key,
							   uint32_t keylen)
{
	sip_from_t const *from = sip->sip_from;
	sip_expires_t const *expires = sip->sip_expires;
	sip_authorization_t const *authorization = sip->sip_authorization;
	sip_contact_t const *contact = sip->sip_contact;
	switch_xml_t domain, xml, user, param, xparams;
	char params[1024] = "";
	char *sql;
	switch_event_t *s_event;
	char *from_user = (char *) from->a_url->url_user;
	char *from_host = (char *) from->a_url->url_host;
	char contact_str[1024] = "";
	char buf[512];
	char *passwd = NULL;
	uint8_t stale = 0, ret = 0, forbidden = 0;
	auth_res_t auth_res;
	long exptime = 60;
	switch_event_t *event;
	char *rpid = "unknown";
	const char *display = "\"user\"";

	if (contact) {
		char *port = (char *) contact->m_url->url_port;
		display = contact->m_display;

		if (switch_strlen_zero(display)) {
			if (from) {
				display = from->a_display;
				if (switch_strlen_zero(display)) {
					display = "\"user\"";
				}
			}
		} else {
			display = "\"user\"";
		}
		
		if (!port) {
			port = "5060";
		}

		if (contact->m_url->url_params) {
			snprintf(contact_str, sizeof(contact_str), "%s <sip:%s@%s:%s;%s>", 
					 display, contact->m_url->url_user, contact->m_url->url_host, port, contact->m_url->url_params);
		} else {
			snprintf(contact_str, sizeof(contact_str), "%s <sip:%s@%s:%s>", 
					 display, contact->m_url->url_user, contact->m_url->url_host, port);
		}
	}
	
	if (expires) {
		exptime = expires->ex_delta;
	} else if (contact->m_expires) {
		exptime = atol(contact->m_expires);
	} 

	if (regtype == REG_REGISTER) {
		authorization = sip->sip_authorization;
	} else if (regtype == REG_INVITE) {
		authorization = sip->sip_proxy_authorization;
	}

	if ((profile->pflags & PFLAG_BLIND_REG)) {
		goto reg;
	}

	if (authorization) {
		if ((auth_res = parse_auth(profile, authorization, (char *)sip->sip_request->rq_method_name, key, keylen)) == AUTH_STALE) {
            stale = 1;
        }

		if (auth_res != AUTH_OK && !stale) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "send %s for [%s@%s]\n",
							  forbidden ? "forbidden" : "challange",
							  from_user, from_host);
			if (auth_res == AUTH_FORBIDDEN) {
				nua_respond(nh, SIP_403_FORBIDDEN, NUTAG_WITH_THIS(nua), TAG_END());
			} else {
				nua_respond(nh, SIP_401_UNAUTHORIZED, NUTAG_WITH_THIS(nua), TAG_END());
			}
			return 1;
		}
	} 

	if (!authorization || stale) {
		snprintf(params, sizeof(params), "from_user=%s&from_host=%s&contact=%s",
				 from_user,
				 from_host,
				 contact_str
				 );

		
		if (switch_xml_locate("directory", "domain", "name", from_host, &xml, &domain, params) != SWITCH_STATUS_SUCCESS) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "can't find domain for [%s@%s]\n", from_user, from_host);
			nua_respond(nh, SIP_401_UNAUTHORIZED, NUTAG_WITH_THIS(nua), SIPTAG_CONTACT(contact), TAG_END());
			return 1;
		}

		if (!(user = switch_xml_find_child(domain, "user", "id", from_user))) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "can't find user [%s@%s]\n", from_user, from_host);
			nua_respond(nh, SIP_401_UNAUTHORIZED, NUTAG_WITH_THIS(nua), SIPTAG_CONTACT(contact), TAG_END());
			switch_xml_free(xml);
			return 1;
		}

		if (!(xparams = switch_xml_child(user, "params"))) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "can't find params for user [%s@%s]\n", from_user, from_host);
			nua_respond(nh, SIP_401_UNAUTHORIZED, NUTAG_WITH_THIS(nua), SIPTAG_CONTACT(contact), TAG_END());
			switch_xml_free(xml);
			return 1;
		}
	

		for (param = switch_xml_child(xparams, "param"); param; param = param->next) {
			char *var = (char *) switch_xml_attr_soft(param, "name");
			char *val = (char *) switch_xml_attr_soft(param, "value");
		
			//switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "param [%s]=[%s]\n", var, val);
		
			if (!strcasecmp(var, "password")) {
				passwd = val;
			}
		}
	
		if (passwd) {
			switch_uuid_t uuid;
			char uuid_str[SWITCH_UUID_FORMATTED_LENGTH + 1];
			char *sql, *auth_str;

			su_md5_t ctx;
			char hexdigest[2 * SU_MD5_DIGEST_SIZE + 1];
			char *input;

			input = switch_mprintf("%s:%s:%s", from_user, from_host, passwd);
			su_md5_init(&ctx);
			su_md5_strupdate(&ctx, input);
			su_md5_hexdigest(&ctx, hexdigest);
			su_md5_deinit(&ctx);
			switch_safe_free(input);

			switch_uuid_get(&uuid);
			switch_uuid_format(uuid_str, &uuid);

			sql = switch_mprintf("delete from sip_authentication where user='%q' and host='%q';\n"
                                 "insert into sip_authentication values('%q','%q','%q','%q', %ld)",
                                 from_user,
                                 from_host,
                                 from_user,
                                 from_host,
                                 hexdigest,
                                 uuid_str,
                                 time(NULL) + 60);
			auth_str = switch_mprintf("Digest realm=\"%q\", nonce=\"%q\",%s algorithm=MD5, qop=\"auth\"", from_host, uuid_str, 
											  stale ? " stale=\"true\"," : "");


			if (regtype == REG_REGISTER) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Requesting Registration from: [%s@%s]\n", from_user, from_host);
				nua_respond(nh, SIP_401_UNAUTHORIZED,
							NUTAG_WITH_THIS(nua),
							SIPTAG_WWW_AUTHENTICATE_STR(auth_str),
							TAG_END());
			} else if (regtype == REG_INVITE) {
				nua_respond(nh, SIP_407_PROXY_AUTH_REQUIRED,
							NUTAG_WITH_THIS(nua),
							SIPTAG_PROXY_AUTHENTICATE_STR(auth_str),
							TAG_END());

			}

			execute_sql(profile->dbname, sql, profile->ireg_mutex);
			switch_safe_free(sql);
			switch_safe_free(auth_str);
			ret = 1;
		} else {
			ret = 0;
		}
		
		switch_xml_free(xml);

		if (ret) {
			return ret;
		}
	}
 reg:

	if (exptime) {
		if (!find_reg_url(profile, from_user, from_host, buf, sizeof(buf))) {
			sql = switch_mprintf("insert into sip_registrations values ('%q','%q','%q','Registered', '%q', %ld)", 
								 from_user,
								 from_host,
								 contact_str,
								 rpid,
								 (long) time(NULL) + (long)exptime * 2);

		} else {
			sql = switch_mprintf("update sip_registrations set contact='%q', expires=%ld, rpid='%q' where user='%q' and host='%q'",
								 contact_str,
								 (long) time(NULL) + (long)exptime * 2,
								 rpid,
								 from_user,
								 from_host);
		
		}

		if (switch_event_create_subclass(&s_event, SWITCH_EVENT_CUSTOM, MY_EVENT_REGISTER) == SWITCH_STATUS_SUCCESS) {
			switch_event_add_header(s_event, SWITCH_STACK_BOTTOM, "profile-name", "%s", profile->name);
			switch_event_add_header(s_event, SWITCH_STACK_BOTTOM, "from-user", "%s", from_user);
			switch_event_add_header(s_event, SWITCH_STACK_BOTTOM, "from-host", "%s", from_host);
			switch_event_add_header(s_event, SWITCH_STACK_BOTTOM, "contact", "%s", contact_str);
			switch_event_add_header(s_event, SWITCH_STACK_BOTTOM, "rpid", "%s", rpid);
			switch_event_add_header(s_event, SWITCH_STACK_BOTTOM, "expires", "%ld", (long)exptime);
			switch_event_fire(&s_event);
		}

		if (sql) {
			execute_sql(profile->dbname, sql, profile->ireg_mutex);
			switch_safe_free(sql);
			sql = NULL;
		}
	
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Register:\nFrom:    [%s@%s]\nContact: [%s]\nExpires: [%ld]\n", 
						  from_user, 
						  from_host,
					  contact_str,
					  (long)exptime
					  );


		if (switch_event_create(&event, SWITCH_EVENT_PRESENCE_IN) == SWITCH_STATUS_SUCCESS) {
			switch_event_add_header(event, SWITCH_STACK_BOTTOM, "proto", "sip");
			switch_event_add_header(event, SWITCH_STACK_BOTTOM, "login", "%s", profile->url);
			switch_event_add_header(event, SWITCH_STACK_BOTTOM, "rpid", "%s", rpid);
			switch_event_add_header(event, SWITCH_STACK_BOTTOM, "from", "%s@%s", from_user, from_host);
			
			switch_event_add_header(event, SWITCH_STACK_BOTTOM, "status", "Registered");
			switch_event_add_header(event, SWITCH_STACK_BOTTOM, "event_type", "presence");
			switch_event_fire(&event);
		}
	} else {
		if ((sql = switch_mprintf("delete from sip_subscriptions where user='%q' and host='%q'", from_user, from_host))) {
			execute_sql(profile->dbname, sql, profile->ireg_mutex);
			switch_safe_free(sql);
			sql = NULL;
		}
		if (switch_event_create(&event, SWITCH_EVENT_PRESENCE_OUT) == SWITCH_STATUS_SUCCESS) {
			switch_event_add_header(event, SWITCH_STACK_BOTTOM, "proto", "sip");
			switch_event_add_header(event, SWITCH_STACK_BOTTOM, "login", "%s", profile->url);
			switch_event_add_header(event, SWITCH_STACK_BOTTOM, "from", "%s+%s@%s", SOFIA_CHAT_PROTO, from_user, from_host);
		
			switch_event_add_header(event, SWITCH_STACK_BOTTOM, "status", "unavailable");
			switch_event_add_header(event, SWITCH_STACK_BOTTOM, "rpid", rpid);
			switch_event_add_header(event, SWITCH_STACK_BOTTOM, "event_type", "presence");
			switch_event_fire(&event);
		}
	}


	if (switch_event_create(&event, SWITCH_EVENT_ROSTER) == SWITCH_STATUS_SUCCESS) {
		switch_event_add_header(event, SWITCH_STACK_BOTTOM, "proto", "sip");
		switch_event_add_header(event, SWITCH_STACK_BOTTOM, "from", "%s@%s", from_user, from_host);
		switch_event_fire(&event);
	}

	if (regtype == REG_REGISTER) {
		nua_respond(nh, SIP_200_OK, SIPTAG_CONTACT(contact),
					NUTAG_WITH_THIS(nua),
					TAG_END());
		return 1;
	}

	return 0;
}


static int sub_reg_callback(void *pArg, int argc, char **argv, char **columnNames)
{
	sofia_profile_t *profile = (sofia_profile_t *) pArg;
	//char *proto = argv[0];
	char *user = argv[1];
	char *host = argv[2];
	switch_event_t *event;
	char *status = NULL;
	if (switch_strlen_zero(status)) {
		status = "Available";
	}
	if (switch_event_create(&event, SWITCH_EVENT_PRESENCE_IN) == SWITCH_STATUS_SUCCESS) {
		switch_event_add_header(event, SWITCH_STACK_BOTTOM, "proto", SOFIA_CHAT_PROTO);
		switch_event_add_header(event, SWITCH_STACK_BOTTOM, "login", "%s", profile->url);
		switch_event_add_header(event, SWITCH_STACK_BOTTOM, "from", "%s@%s", user, host);
		switch_event_add_header(event, SWITCH_STACK_BOTTOM, "status", "%s", status);
		switch_event_add_header(event, SWITCH_STACK_BOTTOM, "event_type", "presence");
		switch_event_add_header(event, SWITCH_STACK_BOTTOM, "event_subtype", "probe");
		switch_event_fire(&event);
	}

	return 0;
}

static int resub_callback(void *pArg, int argc, char **argv, char **columnNames)
{
	sofia_profile_t *profile = (sofia_profile_t *) pArg;
	char *user = argv[0];
	char *host = argv[1];
	char *status = argv[2];
	char *rpid = argv[3];
	char *proto = argv[4];
	switch_event_t *event;

	if (switch_strlen_zero(proto)) {
		proto = NULL;
	}

	if (switch_event_create(&event, SWITCH_EVENT_PRESENCE_IN) == SWITCH_STATUS_SUCCESS) {
		switch_event_add_header(event, SWITCH_STACK_BOTTOM, "proto", proto ? proto : SOFIA_CHAT_PROTO);
		switch_event_add_header(event, SWITCH_STACK_BOTTOM, "login", "%s", profile->url);
		switch_event_add_header(event, SWITCH_STACK_BOTTOM, "from", "%s@%s", user, host);
		switch_event_add_header(event, SWITCH_STACK_BOTTOM, "status", "%s", status);
		switch_event_add_header(event, SWITCH_STACK_BOTTOM, "rpid", "%s", rpid);
		switch_event_add_header(event, SWITCH_STACK_BOTTOM, "event_type", "presence");
		switch_event_fire(&event);
	}

	return 0;
}

static int sub_callback(void *pArg, int argc, char **argv, char **columnNames)
{
	sofia_profile_t *profile = (sofia_profile_t *) pArg;
	char *pl;
	char *id, *note;
	uint32_t in = atoi(argv[0]);
	char *status = argv[1];
	char *rpid = argv[2];
	char *proto = argv[3];
	char *user = argv[4];
	char *host = argv[5];
	char *sub_to_user = argv[6];
	char *sub_to_host = argv[7];
	char *event = argv[8];
	char *contact = argv[9];
	char *callid = argv[10];
	char *full_from = argv[11];
	char *full_via = argv[12];
	nua_handle_t *nh;
	char *to;
	char *open;
	char *tmp;

	if (!rpid) {
		rpid = "unknown";
	}

	if (in) {
		note = switch_mprintf("<dm:note>%s</dm:note>", status);
		open = "open";
	} else {
		note = NULL;
		open = "closed";
	}

    if (!strcasecmp(sub_to_host, host)) {
        /* same host */
		id = switch_mprintf("sip:%s+%s@%s", proto, sub_to_user, sub_to_host);
	} else if (strcasecmp(proto, SOFIA_CHAT_PROTO)) {
		/*encapsulate*/
		id = switch_mprintf("sip:%s+%s+%s@%s", proto, sub_to_user, sub_to_host, host);
	} else {
		id = switch_mprintf("sip:%s@%s", sub_to_user, sub_to_host);
	}

	to = switch_mprintf("sip:%s@%s", user, host);
	pl = switch_mprintf("<?xml version='1.0' encoding='UTF-8'?>\r\n"
								"<presence xmlns='urn:ietf:params:xml:ns:pidf'\r\n"
								"xmlns:dm='urn:ietf:params:xml:ns:pidf:data-model'\r\n"
								"xmlns:rpid='urn:ietf:params:xml:ns:pidf:rpid'\r\n"
								"xmlns:c='urn:ietf:params:xml:ns:pidf:cipid'\r\n"
								"entity='pres:%s'>\r\n"
								"<tuple id='t6a5ed77e'>\r\n"
								"<status>\r\n"
								"<basic>%s</basic>\r\n"
								"</status>\r\n"
								"</tuple>\r\n"
								"<dm:person id='p06360c4a'>\r\n"
								"<rpid:activities>\r\n"
								"<rpid:%s/>\r\n"
								"</rpid:activities>%s</dm:person>\r\n"
								"</presence>", id, open, rpid, note);

	

	nh = nua_handle(profile->nua, NULL,	TAG_END());
	tmp = contact;
	contact = get_url_from_contact(tmp, 0);

	nua_notify(nh,
			   NUTAG_URL(contact),
			   SIPTAG_TO_STR(full_from),
			   SIPTAG_FROM_STR(id),
			   SIPTAG_CONTACT_STR(profile->url),
			   SIPTAG_CALL_ID_STR(callid),
			   SIPTAG_VIA_STR(full_via),
			   SIPTAG_SUBSCRIPTION_STATE_STR("active;expires=3600"),
			   SIPTAG_EVENT_STR(event),
			   SIPTAG_CONTENT_TYPE_STR("application/pidf+xml"),
			   SIPTAG_PAYLOAD_STR(pl),
			   TAG_END());

	switch_safe_free(id);
	switch_safe_free(note);
	switch_safe_free(pl);
	switch_safe_free(to);

	return 0;
}

static void sip_i_subscribe(int status,
						char const *phrase, 
						nua_t *nua,
						sofia_profile_t *profile,
						nua_handle_t *nh,
						sofia_private_t *sofia_private,
						sip_t const *sip,
						tagi_t tags[])
{
		if (sip) {
			long exp, exp_raw;
			sip_to_t const *to = sip->sip_to;
			sip_from_t const *from = sip->sip_from;
			sip_contact_t const *contact = sip->sip_contact;
			char *from_user = NULL;
			char *from_host = NULL;
			char *to_user = NULL;
			char *to_host = NULL;
			char *sql, *event = NULL;
			char *proto = "sip";
			char *d_user = NULL;
			char *contact_str = "";
			char *call_id = NULL;
			char *to_str = NULL;
			char *full_from = NULL;
			char *full_via = NULL;
			switch_core_db_t *db;
			char *errmsg;
			char *sstr;
			const char *display = "\"user\"";
			switch_event_t *sevent;

			if (contact) {
				char *port = (char *) contact->m_url->url_port;

				display = contact->m_display;
				
				if (switch_strlen_zero(display)) {
					if (from) {
						display = from->a_display;
						if (switch_strlen_zero(display)) {
							display = "\"user\"";
						}
					}
				} else {
					display = "\"user\"";
				}

				if (!port) {
					port = "5060";
				}

				if (contact->m_url->url_params) {
					contact_str = switch_mprintf("%s <sip:%s@%s:%s;%s>", 
												 display,
												 contact->m_url->url_user,
												 contact->m_url->url_host, port, contact->m_url->url_params);
				} else {
					contact_str = switch_mprintf("%s <sip:%s@%s:%s>", 
												 display,
												 contact->m_url->url_user,
												 contact->m_url->url_host, port);
				}
			}
			
			if (to) {
				to_str = switch_mprintf("sip:%s@%s", to->a_url->url_user, to->a_url->url_host);//, to->a_url->url_port);
			}

			if (to) {
				to_user = (char *) to->a_url->url_user;
				to_host = (char *) to->a_url->url_host;
			}


			if (strstr(to_user, "ext+") || strstr(to_user, "user+") || strstr(to_user, "conf+")) {
				char proto[80];
				char *p;

				switch_copy_string(proto, to_user, sizeof(proto));
				if ((p = strchr(proto, '+'))) {
					*p = '\0';
				}
				
				if (switch_event_create(&sevent, SWITCH_EVENT_PRESENCE_IN) == SWITCH_STATUS_SUCCESS) {
					switch_event_add_header(sevent, SWITCH_STACK_BOTTOM, "proto", SOFIA_CHAT_PROTO);
					switch_event_add_header(sevent, SWITCH_STACK_BOTTOM, "login", "%s", profile->name);
					switch_event_add_header(sevent, SWITCH_STACK_BOTTOM, "from", "%s@%s",  to_user, to_host);
					switch_event_add_header(sevent, SWITCH_STACK_BOTTOM, "rpid", "unknown");
					switch_event_add_header(sevent, SWITCH_STACK_BOTTOM, "status", "Click To Call");
					switch_event_fire(&sevent);
				}
			}

			if (strchr(to_user, '+')) {
				char *h;
				if ((proto = (d_user = strdup(to_user)))) {
					if ((to_user = strchr(d_user, '+'))) {
						*to_user++ = '\0';
						if ((h = strchr(to_user, '+')) || (h = strchr(to_user, '@'))) {
							*h++ = '\0';
							to_host = h;
						}
					}
				}

				if (!(proto && to_user && to_host)) {
					nua_respond(nh, SIP_404_NOT_FOUND, NUTAG_WITH_THIS(nua), TAG_END());
					goto end;
				}
			}

			call_id = sip_header_as_string(profile->home, (void *)sip->sip_call_id);
			event = sip_header_as_string(profile->home, (void *)sip->sip_event);
			full_from = sip_header_as_string(profile->home, (void *)sip->sip_from);
			full_via = sip_header_as_string(profile->home, (void *)sip->sip_via);

			exp_raw = (sip->sip_expires ? sip->sip_expires->ex_delta : 3600);
			exp = (long) time(NULL) + exp_raw;
			
            if (sip && sip->sip_from) {
                from_user = (char *) sip->sip_from->a_url->url_user;
                from_host = (char *) sip->sip_from->a_url->url_host;
            } else {
                from_user = "n/a";
                from_host = "n/a";
            }

			if ((sql = switch_mprintf("delete from sip_subscriptions where "
											  "proto='%q' and user='%q' and host='%q' and sub_to_user='%q' and sub_to_host='%q' and event='%q';\n"
											  "insert into sip_subscriptions values ('%q','%q','%q','%q','%q','%q','%q','%q','%q','%q',%ld)",
											  proto,
											  from_user,
											  from_host,
											  to_user,
											  to_host,
											  event,
											  proto,
											  from_user,
											  from_host,
											  to_user,
											  to_host,
											  event,
											  contact_str,
											  call_id,
											  full_from,
											  full_via,
											  exp
									  ))) {
				execute_sql(profile->dbname, sql, profile->ireg_mutex);
				switch_safe_free(sql);
			}

			sstr = switch_mprintf("active;expires=%ld", exp_raw);

			nua_respond(nh, SIP_202_ACCEPTED,
						NUTAG_WITH_THIS(nua),
						SIPTAG_SUBSCRIPTION_STATE_STR(sstr),
						SIPTAG_FROM(sip->sip_to),
						SIPTAG_TO(sip->sip_from),
						SIPTAG_CONTACT_STR(to_str),
						TAG_END());



			switch_safe_free(sstr);
			
			if (!(db = switch_core_db_open_file(profile->dbname))) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Error Opening DB %s\n", profile->dbname);
				goto end;
			}
			if ((sql = switch_mprintf("select * from sip_subscriptions where user='%q' and host='%q'", 
									  to_user, to_host, to_user, to_host))) {
				switch_mutex_lock(profile->ireg_mutex);
				switch_core_db_exec(db, sql, sub_reg_callback, profile, &errmsg);
				switch_mutex_unlock(profile->ireg_mutex);
				switch_safe_free(sql);
			}
			switch_core_db_close(db);
		end:
		
			if (event) {
				su_free(profile->home, event);
			}
			if (call_id) {
				su_free(profile->home, call_id);
			}
			if (full_from) {
				su_free(profile->home, full_from);
			}
			if (full_via) {
				su_free(profile->home, full_via);
			}

			switch_safe_free(d_user);
			switch_safe_free(to_str);
			switch_safe_free(contact_str);
		}
}

static void sip_r_subscribe(int status,
						char const *phrase, 
						nua_t *nua,
						sofia_profile_t *profile,
						nua_handle_t *nh,
						sofia_private_t *sofia_private,
						sip_t const *sip,
						tagi_t tags[])
{

}


/*---------------------------------------*/
static void sip_i_refer(nua_t *nua,
						sofia_profile_t *profile,
						nua_handle_t *nh,
                        switch_core_session_t *session,
						sip_t const *sip,
						tagi_t tags[])
{
	/* Incoming refer */
	sip_from_t const *from;
	sip_to_t const *to;
	sip_refer_to_t const *refer_to;
    private_object_t *tech_pvt = NULL;
    char *etmp = NULL, *exten = NULL;
    switch_channel_t *channel_a = NULL, *channel_b = NULL;

    tech_pvt = switch_core_session_get_private(session);
    channel_a = switch_core_session_get_channel(session);

    if (!sip->sip_cseq || !(etmp = switch_mprintf("refer;id=%u", sip->sip_cseq->cs_seq))) {
        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Memory Error!\n");
        goto done;
    }


    if (switch_channel_test_flag(channel_a, CF_NOMEDIA)) {
        nua_notify(tech_pvt->nh, SIPTAG_CONTENT_TYPE_STR("message/sipfrag"),
                   SIPTAG_PAYLOAD_STR("SIP/2.0 403 Forbidden"),
                   SIPTAG_EVENT_STR(etmp),
                   TAG_END());
        goto done;
    }
		
    from = sip->sip_from;
    to = sip->sip_to;

    if ((refer_to = sip->sip_refer_to)) {
        if (profile->pflags & PFLAG_FULL_ID) {
            exten = switch_mprintf("%s@%s", (char *) refer_to->r_url->url_user, (char *) refer_to->r_url->url_host);
        } else {
            exten = (char *) refer_to->r_url->url_user;
        }

        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Process REFER to [%s@%s]\n", exten, (char *) refer_to->r_url->url_host);

        if (refer_to->r_url->url_headers) {
            sip_replaces_t *replaces;
            nua_handle_t *bnh;
            char *rep;

            if ((rep = strchr(refer_to->r_url->url_headers, '='))) {
                char *br_a = NULL, *br_b = NULL;
                char *buf;
                rep++;

					

                if ((buf = switch_core_session_alloc(session, strlen(rep) + 1))) {
                    rep = url_unescape(buf, (const char *) rep); 
                    switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Replaces: [%s]\n", rep);
                } else {
                    switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Memory Error!\n");
                    goto done;
                }
                if ((replaces = sip_replaces_make(tech_pvt->home, rep)) && (bnh = nua_handle_by_replaces(nua, replaces))) {
                    sofia_private_t *b_private = NULL;
                    private_object_t *b_tech_pvt = NULL;
                    switch_core_session_t *b_session = NULL;

                    switch_channel_set_variable(channel_a, SOFIA_REPLACES_HEADER, rep);	
                    if ((b_private = nua_handle_magic(bnh))) {
                        if (!(b_session = switch_core_session_locate(b_private->uuid))) {
                            goto done;
                        }
                        b_tech_pvt = (private_object_t *) switch_core_session_get_private(b_session);
                        channel_b = switch_core_session_get_channel(b_session);
				
                        br_a = switch_channel_get_variable(channel_a, SWITCH_BRIDGE_VARIABLE);
                        br_b = switch_channel_get_variable(channel_b, SWITCH_BRIDGE_VARIABLE);

                        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "Attended Transfer [%s][%s]\n", br_a, br_b);
                            
                        if (br_a && br_b) {
                            switch_ivr_uuid_bridge(br_a, br_b);
                            switch_channel_set_variable(channel_b, "endpoint_disposition", "ATTENDED_TRANSFER");
                            switch_set_flag_locked(tech_pvt, TFLAG_BYE);
                            switch_set_flag_locked(b_tech_pvt, TFLAG_BYE);
                            nua_notify(tech_pvt->nh, SIPTAG_CONTENT_TYPE_STR("message/sipfrag"),
                                       SIPTAG_PAYLOAD_STR("SIP/2.0 200 OK"),
                                       SIPTAG_EVENT_STR(etmp),
                                       TAG_END());
                                
                        } else {
                            if (!br_a && !br_b) {
                                switch_set_flag_locked(tech_pvt, TFLAG_NOHUP);
                                switch_set_flag_locked(b_tech_pvt, TFLAG_XFER);
                                b_tech_pvt->xferto = switch_core_session_strdup(b_session, switch_core_session_get_uuid(session));
                            } else if (!br_a && br_b) {
                                switch_core_session_t *br_b_session;

                                if ((br_b_session = switch_core_session_locate(br_b))) {
                                    private_object_t *br_b_tech_pvt = switch_core_session_get_private(br_b_session);
                                    switch_channel_t *br_b_channel = switch_core_session_get_channel(br_b_session);
                                    
                                    switch_set_flag_locked(tech_pvt, TFLAG_NOHUP);
										
                                    switch_channel_clear_state_handler(br_b_channel, NULL);
                                    switch_channel_set_state_flag(br_b_channel, CF_TRANSFER);
                                    switch_channel_set_state(br_b_channel, CS_TRANSMIT);

                                    switch_set_flag_locked(tech_pvt, TFLAG_REINVITE);
                                    tech_pvt->local_sdp_audio_ip = switch_core_session_strdup(session, b_tech_pvt->local_sdp_audio_ip);
                                    tech_pvt->local_sdp_audio_port = b_tech_pvt->local_sdp_audio_port;

                                    tech_pvt->remote_sdp_audio_ip = switch_core_session_strdup(session, br_b_tech_pvt->remote_sdp_audio_ip);
                                    tech_pvt->remote_sdp_audio_port = br_b_tech_pvt->remote_sdp_audio_port;
                                    activate_rtp(tech_pvt);
	
                                    br_b_tech_pvt->kick = switch_core_session_strdup(br_b_session, switch_core_session_get_uuid(session));
										

                                    switch_core_session_rwunlock(br_b_session);
                                }

                                switch_channel_hangup(channel_b, SWITCH_CAUSE_ATTENDED_TRANSFER);
                            }
                            switch_set_flag_locked(tech_pvt, TFLAG_BYE);
                            nua_notify(tech_pvt->nh, SIPTAG_CONTENT_TYPE_STR("message/sipfrag"),
                                       SIPTAG_PAYLOAD_STR("SIP/2.0 200 OK"),
                                       SIPTAG_EVENT_STR(etmp),
                                       TAG_END());

                        }
                        if (b_session) {
                            switch_core_session_rwunlock(b_session);
                        }
                    }
                    nua_handle_unref(bnh);
                } else { /* the other channel is on a different box, we have to go find them */
                    if (exten && (br_a = switch_channel_get_variable(channel_a, SWITCH_BRIDGE_VARIABLE))) {
                        switch_core_session_t *a_session;
                        switch_channel_t *channel = switch_core_session_get_channel(session);
							
                        if ((a_session = switch_core_session_locate(br_a))) {
                            switch_core_session_t *tsession;
                            switch_call_cause_t cause = SWITCH_CAUSE_NORMAL_CLEARING;
                            uint32_t timeout = 60;
                            char *tuuid_str;

                            channel = switch_core_session_get_channel(a_session);

                            exten = switch_mprintf("sofia/%s/%s@%s:%s", 
                                                   profile->name,
                                                   (char *) refer_to->r_url->url_user,
                                                   (char *) refer_to->r_url->url_host,
                                                   refer_to->r_url->url_port
                                                   );

                            switch_channel_set_variable(channel, SOFIA_REPLACES_HEADER, rep);
								
                            if (switch_ivr_originate(a_session,
                                                     &tsession,
                                                     &cause,
                                                     exten,
                                                     timeout,
                                                     &noop_state_handler,
                                                     NULL,
                                                     NULL,
                                                     NULL) != SWITCH_STATUS_SUCCESS) {
                                switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Cannot Create Outgoing Channel! [%s]\n", exten);
                                nua_notify(tech_pvt->nh, SIPTAG_CONTENT_TYPE_STR("message/sipfrag"),
                                           SIPTAG_PAYLOAD_STR("SIP/2.0 403 Forbidden"),
                                           SIPTAG_EVENT_STR(etmp),
                                           TAG_END());
                                goto done;
                            } 

                            switch_core_session_rwunlock(a_session);
                            tuuid_str = switch_core_session_get_uuid(tsession);
                            switch_ivr_uuid_bridge(br_a, tuuid_str);
                            switch_channel_set_variable(channel_a, "endpoint_disposition", "ATTENDED_TRANSFER");
                            switch_set_flag_locked(tech_pvt, TFLAG_BYE);
                            nua_notify(tech_pvt->nh, SIPTAG_CONTENT_TYPE_STR("message/sipfrag"),
                                       SIPTAG_PAYLOAD_STR("SIP/2.0 200 OK"),
                                       SIPTAG_EVENT_STR(etmp),
                                       TAG_END());
                        } else {
                            goto error;
                        }

                    } else { error:
                        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Invalid Transfer! [%s]\n", br_a);
                        switch_channel_set_variable(channel_a, "endpoint_disposition", "ATTENDED_TRANSFER_ERROR");
                        nua_notify(tech_pvt->nh, SIPTAG_CONTENT_TYPE_STR("message/sipfrag"),
                                   SIPTAG_PAYLOAD_STR("SIP/2.0 403 Forbidden"),
                                   SIPTAG_EVENT_STR(etmp),
                                   TAG_END());
                    }
                }
            } else {
                switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Cannot parse Replaces!\n");
            }
            goto done;
        }

    } else {
        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Missing Refer-To\n");
        goto done;
    }

    if (exten) {
        switch_channel_t *channel = switch_core_session_get_channel(session);
        char *br;
				
        if ((br = switch_channel_get_variable(channel, SWITCH_BRIDGE_VARIABLE))) {
            switch_core_session_t *b_session;
				
            if ((b_session = switch_core_session_locate(br))) {
                switch_channel_set_variable(channel, "TRANSFER_FALLBACK", (char *) from->a_user);
                switch_ivr_session_transfer(b_session, exten, profile->dialplan, profile->context);
                switch_core_session_rwunlock(b_session);
            } 

            switch_channel_set_variable(channel, "endpoint_disposition", "BLIND_TRANSFER");
                
            /*
              nua_notify(tech_pvt->nh, SIPTAG_CONTENT_TYPE_STR("message/sipfrag"),
              SIPTAG_PAYLOAD_STR("SIP/2.0 200 OK"),
              SIPTAG_EVENT_STR(etmp),
              TAG_END());
            */
        } else {
            exten = switch_mprintf("sip:%s@%s:%s", 
                                   (char *) refer_to->r_url->url_user,
                                   (char *) refer_to->r_url->url_host,
                                   refer_to->r_url->url_port);
            tech_pvt->dest = switch_core_session_strdup(session, exten);
				
				
            switch_set_flag_locked(tech_pvt, TFLAG_NOHUP);

            /*
              nua_notify(tech_pvt->nh, SIPTAG_CONTENT_TYPE_STR("message/sipfrag"),
              SIPTAG_PAYLOAD_STR("SIP/2.0 200 OK"),
              SIPTAG_EVENT_STR(etmp),
              TAG_END());
            */
            do_xfer_invite(session);

        }
    }

 done:
    if (exten && strchr(exten, '@')) {
        switch_safe_free(exten);
    }
    if (etmp) {
        switch_safe_free(etmp);
    }
	

}


static void sip_i_publish(nua_t *nua, 
						  sofia_profile_t *profile,
						  nua_handle_t *nh, 
						  sofia_private_t *sofia_private,
						  sip_t const *sip,
						  tagi_t tags[])
{
	if (sip) {
		sip_from_t const *from = sip->sip_from;
		char *from_user = NULL;
		char *from_host = NULL;
		char *rpid = "unknown";
		sip_payload_t *payload = sip->sip_payload;
		char *event_type;

		if (from) {
			from_user = (char *) from->a_url->url_user;
			from_host = (char *) from->a_url->url_host;
		}

		if (payload) {
			switch_xml_t xml, note, person, tuple, status, basic, act;
			switch_event_t *event;
			uint8_t in = 0;
			char *sql;

			if ((xml = switch_xml_parse_str(payload->pl_data, strlen(payload->pl_data)))) {
				char *status_txt = "", *note_txt = "";
				
				if ((tuple = switch_xml_child(xml, "tuple")) && (status = switch_xml_child(tuple, "status")) && (basic = switch_xml_child(status, "basic"))) {
					status_txt = basic->txt;
				}
					
				if ((person = switch_xml_child(xml, "dm:person")) && (note = switch_xml_child(person, "dm:note"))) {
					note_txt = note->txt;
				}

				if (person && (act = switch_xml_child(person, "rpid:activities"))) {
					if ((rpid = strchr(act->child->name, ':'))) {
						rpid++;
					} else {
						rpid = act->child->name;
					}
				}

				if (!strcasecmp(status_txt, "open")) {
					if (switch_strlen_zero(note_txt)) {
						note_txt = "Available";
					}
					in = 1;
				} else if (!strcasecmp(status_txt, "closed")) {
					if (switch_strlen_zero(note_txt)) {
						note_txt = "Unavailable";
					}
				}
				
				if ((sql = switch_mprintf("update sip_registrations set status='%q',rpid='%q' where user='%q' and host='%q'", 
										  note_txt, rpid, from_user, from_host))) {
					execute_sql(profile->dbname, sql, profile->ireg_mutex);
					switch_safe_free(sql);
				}
				
				event_type = sip_header_as_string(profile->home, (void *)sip->sip_event);

				if (in) {
					if (switch_event_create(&event, SWITCH_EVENT_PRESENCE_IN) == SWITCH_STATUS_SUCCESS) {
						switch_event_add_header(event, SWITCH_STACK_BOTTOM, "proto", SOFIA_CHAT_PROTO);
						switch_event_add_header(event, SWITCH_STACK_BOTTOM, "rpid", "%s", rpid);
						switch_event_add_header(event, SWITCH_STACK_BOTTOM, "login", "%s", profile->url);
						switch_event_add_header(event, SWITCH_STACK_BOTTOM, "from", "%s@%s", from_user, from_host);
						
						switch_event_add_header(event, SWITCH_STACK_BOTTOM, "status", "%s", note_txt);
						switch_event_add_header(event, SWITCH_STACK_BOTTOM, "event_type", "%s", event_type);
						switch_event_fire(&event);
					}
				} else {
					if (switch_event_create(&event, SWITCH_EVENT_PRESENCE_OUT) == SWITCH_STATUS_SUCCESS) {
						switch_event_add_header(event, SWITCH_STACK_BOTTOM, "proto", SOFIA_CHAT_PROTO);
						switch_event_add_header(event, SWITCH_STACK_BOTTOM, "rpid", "%s", rpid);
						switch_event_add_header(event, SWITCH_STACK_BOTTOM, "login", "%s", profile->url);
						switch_event_add_header(event, SWITCH_STACK_BOTTOM, "from", "%s@%s", from_user, from_host);

						switch_event_add_header(event, SWITCH_STACK_BOTTOM, "event_type", "%s", event_type);
						switch_event_fire(&event);
					}
				}

				if (event_type) {
					su_free(profile->home, event_type);
				}				

				switch_xml_free(xml);
			}
			
		}
		
	}

	nua_respond(nh, SIP_200_OK, NUTAG_WITH_THIS(nua), TAG_END());

}

static void sip_i_info(nua_t *nua,
                       sofia_profile_t *profile,
                       nua_handle_t *nh,
                       switch_core_session_t *session,
                       sip_t const *sip,
                       tagi_t tags[]) {

	//placeholder for string searching
	char *signal_ptr;

	//Try and find signal information in the payload
	signal_ptr = strstr(sip->sip_payload->pl_data, "Signal=");

	//See if we found a match
	if(signal_ptr) {
		struct private_object *tech_pvt = NULL;
		switch_channel_t *channel = NULL;
		char dtmf_digit[2] = {0,0};

		//Get the channel
		channel = switch_core_session_get_channel(session);

		//Barf if we didn't get it
		assert(channel != NULL);

		//make sure we have our privates
		tech_pvt = switch_core_session_get_private(session);

		//Barf if we didn't get it
		assert(tech_pvt != NULL);

		//move signal_ptr where we need it (right past Signal=)
		signal_ptr = signal_ptr + 7;

		//put the digit somewhere we can muck with
		strncpy(dtmf_digit, signal_ptr, 1);

		//queue it up
		switch_channel_queue_dtmf(channel, dtmf_digit);
		
		//print debug info
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "INFO DTMF(%s)\n", dtmf_digit);

	} else { //unknown info type
		sip_from_t const *from;

		from = sip->sip_from;

		//print in the logs if something comes through we don't understand
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Unknown INFO Recieved: %s%s" URL_PRINT_FORMAT "[%s]\n",
		                  from->a_display ? from->a_display : "", from->a_display ? " " : "",
		                  URL_PRINT_ARGS(from->a_url), sip->sip_payload->pl_data);
	}

	return;
}

static void sip_i_invite(nua_t *nua, 
						 sofia_profile_t *profile,
						 nua_handle_t *nh, 
						 sofia_private_t *sofia_private,
						 sip_t const *sip,
						 tagi_t tags[])
{
	switch_core_session_t *session = NULL;
	char key[128] = "";
	sip_unknown_t *un;
    private_object_t *tech_pvt = NULL;
    switch_channel_t *channel = NULL;
    sip_from_t const *from = sip->sip_from;
    sip_to_t const *to = sip->sip_to;
    char *displayname;
    char *username, *to_username = NULL;
    char *url_user = (char *) from->a_url->url_user;
    char *to_user, *to_host, *to_port;
    char *req_user, *req_host, *req_port;
    char *contact_user, *contact_host, *contact_port;
    char *via_rport, *via_host, *via_port;
    char *from_port;
    char uri[1024];

		
    if ((profile->pflags & PFLAG_AUTH_CALLS)) {
        if (handle_register(nua, profile, nh, sip, REG_INVITE, key, sizeof(key))) {
            return;
        }
    }

    if (!(session = switch_core_session_request(&sofia_endpoint_interface, NULL))) {
        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Session Alloc Failed!\n");
        return;
    }

    if (!(tech_pvt = (private_object_t *) switch_core_session_alloc(session, sizeof(private_object_t)))) {
        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Hey where is my memory pool?\n");
        terminate_session(&session, SWITCH_CAUSE_DESTINATION_OUT_OF_ORDER, __LINE__);
        return;
    }

    if (!switch_strlen_zero(key)) {
        tech_pvt->key = switch_core_session_strdup(session, key);
    }

    to_user = (char *) to->a_url->url_user;
    to_host = (char *) to->a_url->url_host;
    if (!(to_port = (char *) to->a_url->url_port)) {
        to_port = "5060";
    }
			
    if (switch_strlen_zero(to_user)) { /* if sofia doesnt parse the To: right, we'll have to do it */
        if ((to_user = sip_header_as_string(tech_pvt->home, (sip_header_t *) to))) {
            char *p;
            if (*to_user == '<') {
                to_user++;
            }
            if ((p = strchr((to_user += 4), '@'))) {
                *p++ = '\0';
                to_host = p;
                if ((p = strchr(to_host, '>'))) {
                    *p = '\0';
                }
            }
        }
    }

    if (switch_strlen_zero(url_user)) {
        url_user = "service";
    }
			
    if (!switch_strlen_zero(from->a_display)) {
        displayname = switch_core_session_strdup(session, (char *) from->a_display);
        if (*displayname == '"') {
            char *p;
				
            displayname++;
            if ((p = strchr(displayname, '"'))) {
                *p = '\0';
            }
        }
    } else {
        displayname = url_user;
    }
			
    if (!(username = switch_mprintf("%s@%s", url_user, (char *) from->a_url->url_host))) {
        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Memory Error!\n");
        return;
    }

    if (profile->pflags & PFLAG_FULL_ID)  {
        if (!(to_username = switch_mprintf("%s@%s:%s", (char *) to_user, (char *) to_host, to_port))) {
            switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Memory Error!\n");
            switch_safe_free(username);
            return;
        }
    }

    attach_private(session, profile, tech_pvt, username);


    channel = switch_core_session_get_channel(session);
    switch_channel_set_variable(channel, "endpoint_disposition", "INBOUND CALL");
    set_chat_hash(tech_pvt, sip);

    if (switch_test_flag(tech_pvt, TFLAG_INB_NOMEDIA)) {
        switch_set_flag_locked(tech_pvt, TFLAG_NOMEDIA);
        switch_channel_set_flag(channel, CF_NOMEDIA);
    }

			
    switch_channel_set_variable(channel, "sip_from_user", (char *) from->a_url->url_user);
    if (from->a_url->url_user && *from->a_url->url_user == '+') {
        switch_channel_set_variable(channel, "sip_from_user_stripped", (char *)(from->a_url->url_user+1));
    } else {
        switch_channel_set_variable(channel, "sip_from_user_stripped", (char *)from->a_url->url_user);
    }
    switch_channel_set_variable(channel, "sip_from_host", (char *) from->a_url->url_host);

            
    if (!(from_port = (char *) from->a_url->url_port)) {
        from_port = "5060";
    }

    switch_channel_set_variable(channel, "sip_from_port", from_port);


    snprintf(uri, sizeof(uri), "%s@%s:%s", (char *) from->a_url->url_user, (char *) from->a_url->url_host, from_port);
    switch_channel_set_variable(channel, "sip_from_uri", uri);
            
            
    switch_channel_set_variable(channel, "sip_to_user", to_user);
    switch_channel_set_variable(channel, "sip_to_host", to_host);
    switch_channel_set_variable(channel, "sip_to_port", to_port);

    snprintf(uri, sizeof(uri), "%s@%s:%s", to_user, to_host, to_port);
    switch_channel_set_variable(channel, "sip_to_uri", uri);


    req_user = (char *) sip->sip_request->rq_url->url_user;
    req_host = (char *) sip->sip_request->rq_url->url_host;
    if (!(req_port = (char *) sip->sip_request->rq_url->url_port)) {
        req_port = "5060";
    }
            
    switch_channel_set_variable(channel, "sip_req_user", req_user);
    switch_channel_set_variable(channel, "sip_req_host", req_host);
    switch_channel_set_variable(channel, "sip_req_port", req_port);

    contact_user = (char *) sip->sip_contact->m_url->url_user;
    contact_host = (char *) sip->sip_contact->m_url->url_host;
    if (!(contact_port = (char *) sip->sip_contact->m_url->url_port)) {
        contact_port = "5060";
    }
            
    switch_channel_set_variable(channel, "sip_contact_user", contact_user);
    switch_channel_set_variable(channel, "sip_contact_host", contact_host);
    switch_channel_set_variable(channel, "sip_contact_port", contact_port);

    via_host = (char *) sip->sip_via->v_host;
    if (!(via_port = (char *) sip->sip_via->v_port)) {
        via_port = "5060";
    }
    if (!(via_rport = (char *) sip->sip_via->v_rport)) {
        via_rport = "5060";
    }
            
    switch_channel_set_variable(channel, "sip_via_host", via_host);
    switch_channel_set_variable(channel, "sip_via_port", via_port);
    switch_channel_set_variable(channel, "sip_via_rport", via_rport);

            
    snprintf(uri, sizeof(uri), "%s@%s:%s", req_user, req_host, req_port);
    switch_channel_set_variable(channel, "sip_req_uri", uri);


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
                                                              (profile->context && !strcasecmp(profile->context, "_domain_")) ? 
                                                              (char *) from->a_url->url_host : profile->context,
                                                              to_username ? to_username : (char *) to_user
                                                              )) != 0) {

				
        for (un=sip->sip_unknown; un; un=un->un_next) {
            if (!strncasecmp(un->un_name, "Alert-Info", 10)) {
                if (!switch_strlen_zero(un->un_value)) { 
                    switch_channel_set_variable(channel, "alert_info", (char *)un->un_value);
                }
                // Loop thru Known Headers Here so we can do something with them
            } else if (!strncasecmp(un->un_name, "Remote-Party-ID", 15)) {
                int argc, x, screen = 1;
                char *mydata, *argv[10] = { 0 };
                if (!switch_strlen_zero(un->un_value)) { 
                    if ((mydata = strdup(un->un_value))) {
                        argc = switch_separate_string(mydata, ';', argv, (sizeof(argv) / sizeof(argv[0]))); 

                        // Do We really need this at this time 
                        // clid_uri = argv[0];

                        for (x=1; x < argc && argv[x]; x++){
                            // we dont need to do anything with party yet we should only be seeing party=calling here anyway
                            // maybe thats a dangerous assumption bit oh well yell at me later
                            // if (!strncasecmp(argv[x], "party", 5)) {
                            //	party = argv[x];
                            // } else 
                            if (!strncasecmp(argv[x], "privacy=", 8)) {
                                char *arg = argv[x] + 9;

                                if (!strcasecmp(arg, "yes")) {
                                    switch_set_flag(tech_pvt->caller_profile, SWITCH_CPF_HIDE_NAME | SWITCH_CPF_HIDE_NUMBER);
                                } else if (!strcasecmp(arg, "full")) {
                                    switch_set_flag(tech_pvt->caller_profile, SWITCH_CPF_HIDE_NAME | SWITCH_CPF_HIDE_NUMBER);
                                } else if (!strcasecmp(arg, "name")) {
                                    switch_set_flag(tech_pvt->caller_profile, SWITCH_CPF_HIDE_NAME);
                                } else if (!strcasecmp(arg, "number")) {
                                    switch_set_flag(tech_pvt->caller_profile, SWITCH_CPF_HIDE_NUMBER);
                                } else {
                                    switch_clear_flag(tech_pvt->caller_profile, SWITCH_CPF_HIDE_NAME);
                                    switch_clear_flag(tech_pvt->caller_profile, SWITCH_CPF_HIDE_NUMBER);
                                }

                            } else if (!strncasecmp(argv[x], "screen=", 7) && screen > 0) {
                                char *arg = argv[x] + 8;
                                if (!strcasecmp(arg, "no")) {
                                    screen = 0;
                                    switch_clear_flag(tech_pvt->caller_profile, SWITCH_CPF_SCREEN);
                                }
                            }
                        }
                        free(mydata);
                    }
                }
                break;
            }
        }

        switch_channel_set_caller_profile(channel, tech_pvt->caller_profile);
        switch_safe_free(username);
        switch_safe_free(to_username);
    }

    if (!(tech_pvt->sofia_private = malloc(sizeof(*tech_pvt->sofia_private)))) {
        abort();
    }
    memset(tech_pvt->sofia_private, 0, sizeof(*tech_pvt->sofia_private));
    switch_copy_string(tech_pvt->sofia_private->uuid, switch_core_session_get_uuid(session), sizeof(tech_pvt->sofia_private->uuid));
    nua_handle_bind(nh, tech_pvt->sofia_private);

}

static void sip_i_register(nua_t *nua,
						   sofia_profile_t *profile,
						   nua_handle_t *nh,
						   sofia_private_t *sofia_private,
						   sip_t const *sip,
						   tagi_t tags[])
{
    char key[128] = "";
	handle_register(nua, profile, nh, sip, REG_REGISTER, key, sizeof(key));
}


static void sip_i_options(int status,
						  char const *phrase,
						  nua_t *nua,
						  sofia_profile_t *profile,
						  nua_handle_t *nh,
						  sofia_private_t *sofia_private,
						  sip_t const *sip,
						  tagi_t tags[])
{
	nua_respond(nh, SIP_200_OK, 
				//SOATAG_USER_SDP_STR(tech_pvt->local_sdp_str),
				//SOATAG_AUDIO_AUX("cn telephone-event"),
				//NUTAG_INCLUDE_EXTRA_SDP(1),
				TAG_END());
}


static void sip_r_register(int status,
						   char const *phrase,
						   nua_t *nua,
						   sofia_profile_t *profile,
						   nua_handle_t *nh,
						   sofia_private_t *sofia_private,
						   sip_t const *sip,
						   tagi_t tags[])
{
	if (sofia_private && sofia_private->oreg) {
		if (status == 200) {
			sofia_private->oreg->state = REG_STATE_REGISTER;
		} else {
			sofia_private->oreg->state = REG_STATE_FAILED;
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "received %d on register!\n", status);
		}
	}
}

static void sip_r_challenge(int status,
                            char const *phrase,
                            nua_t *nua,
                            sofia_profile_t *profile,
                            nua_handle_t *nh,
                            switch_core_session_t *session,
                            sip_t const *sip,
                            tagi_t tags[])
{
	outbound_reg_t *oreg = NULL;
	sip_www_authenticate_t const *authenticate = NULL;
	char const *realm = NULL; 
	char *p = NULL, *duprealm = NULL, *qrealm = NULL;
	char const *scheme = NULL;
	int index;
	char *cur;
	char authentication[256] = "";
	int ss_state;
	
	if (session) {
		private_object_t *tech_pvt;
		if ((tech_pvt = switch_core_session_get_private(session)) && switch_test_flag(tech_pvt, TFLAG_REFER)) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "received reply from refer\n");
			return;
		}
	}


	if (sip->sip_www_authenticate) {
		authenticate = sip->sip_www_authenticate;
	} else if (sip->sip_proxy_authenticate) {
		authenticate = sip->sip_proxy_authenticate;
	} else {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Missing Authenticate Header!\n");
		return;
	}
	scheme = (char const *) authenticate->au_scheme;
	if (authenticate->au_params) {
		for(index = 0; (cur=(char*)authenticate->au_params[index]); index++) {
			if ((realm = strstr(cur, "realm="))) {
				realm += 6;
				break;
			}
		}
	}

	if (!(scheme && realm)) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "No scheme and realm!\n");
		return;
	}

	if (profile) {
		outbound_reg_t *oregp;

		if ((duprealm = strdup(realm))) {
			qrealm = duprealm;
	
			while(*qrealm && *qrealm == '"') {
				qrealm++;
			}

			if ((p = strchr(qrealm, '"'))) {
				*p = '\0';
			}

			for (oregp = profile->registrations; oregp; oregp = oregp->next) {
				if (scheme && qrealm && !strcasecmp(oregp->register_scheme, scheme) && !strcasecmp(oregp->register_realm, qrealm)) {
					oreg = oregp;
					break;
				}
			}
			if (!oreg) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "No Match for Scheme [%s] Realm [%s]\n", scheme, qrealm);
				return;
			}
			switch_safe_free(duprealm);
		} else {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Memory Error!\n");
			return;
		}
	}
		
	snprintf(authentication, sizeof(authentication), "%s:%s:%s:%s", scheme, realm, 
			 oreg->register_username,
			 oreg->register_password);
		
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Authenticating '%s' with '%s'.\n",
					  profile->username, authentication);
		
		
	ss_state = nua_callstate_authenticating;
		
	tl_gets(tags,
			NUTAG_CALLSTATE_REF(ss_state),
			SIPTAG_WWW_AUTHENTICATE_REF(authenticate),
			TAG_END());
		
	nua_authenticate(nh, SIPTAG_EXPIRES_STR(oreg->expires_str), NUTAG_AUTH(authentication), TAG_END());
	
}

static void event_callback(nua_event_t event,
						   int status,
						   char const *phrase,
						   nua_t *nua,
						   sofia_profile_t *profile,
						   nua_handle_t *nh,
						   sofia_private_t *sofia_private,
						   sip_t const *sip,
						   tagi_t tags[])
{
	struct private_object *tech_pvt = NULL;
	auth_res_t auth_res = AUTH_FORBIDDEN;
	switch_core_session_t *session = NULL;
    switch_channel_t *channel = NULL;

    if (sofia_private) {
        if (!switch_strlen_zero(sofia_private->uuid)) {

            if ((session = switch_core_session_locate(sofia_private->uuid))) {
                tech_pvt = switch_core_session_get_private(session);
                channel = switch_core_session_get_channel(tech_pvt->session);
                if (switch_channel_test_flag(channel, CF_NOMEDIA)) {
                    switch_set_flag(tech_pvt, TFLAG_NOMEDIA);
                }

            } else {
                /* too late */
                return;            
            }
        }
    }


	if (status != 100 && status != 200) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "event [%s] status [%d][%s] session: %s\n",
						  nua_event_name (event), status, phrase,
						  session ? switch_channel_get_name(channel) : "n/a"
						  );
	}

	if ((profile->pflags & PFLAG_AUTH_ALL) && tech_pvt && tech_pvt->key && sip) {
		sip_authorization_t const *authorization = NULL;

        if (sip->sip_authorization) {
			authorization = sip->sip_authorization;
		} else if (sip->sip_proxy_authorization) {
			authorization = sip->sip_proxy_authorization;
		}

		if (authorization) {
			auth_res = parse_auth(profile, authorization, (char *)sip->sip_request->rq_method_name, tech_pvt->key, strlen(tech_pvt->key));
		}

		if (auth_res != AUTH_OK) {
			switch_channel_hangup(channel, SWITCH_CAUSE_DESTINATION_OUT_OF_ORDER);
			nua_respond(nh, SIP_401_UNAUTHORIZED, TAG_END());
			goto done;
		}

		if (channel) {
			switch_channel_set_variable(channel, "sip_authorized", "true");
		}
	}

	if (sip && (status == 401 || status == 407)) {
		sip_r_challenge(status, phrase, nua, profile, nh, session, sip, tags);
		goto done;
	}
	
	switch (event) {
	case nua_r_shutdown:    
	case nua_r_get_params:    
	case nua_r_invite:
    case nua_r_unregister:
    case nua_r_options:
	case nua_i_fork:
	case nua_r_info:
    case nua_r_bye:
	case nua_i_bye:
	case nua_r_unsubscribe:
	case nua_r_publish:
	case nua_r_message:
	case nua_r_notify:
    case nua_i_notify:
	case nua_i_cancel:
	case nua_i_error:
	case nua_i_active:
	case nua_i_ack:
	case nua_i_terminated:
	case nua_r_set_params:
        break;
	case nua_r_register:
		sip_r_register(status, phrase, nua, profile, nh, sofia_private, sip, tags);
		break;
	case nua_i_options:
		sip_i_options(status, phrase, nua, profile, nh, sofia_private, sip, tags);
		break;
    case nua_i_invite:
        if (!session) {
            sip_i_invite(nua, profile, nh, sofia_private, sip, tags);
        }
		break;
	case nua_i_publish:
		sip_i_publish(nua, profile, nh, sofia_private, sip, tags);
		break;
    case nua_i_register:
 		sip_i_register (nua, profile, nh, sofia_private, sip, tags);
        break;
	case nua_i_prack:
		break;
	case nua_i_state:
		sip_i_state(status, phrase, nua, profile, nh, sofia_private, sip, tags);
		break;
	case nua_i_message:
		sip_i_message(status, phrase, nua, profile, nh, sofia_private, sip, tags);
		break;
	case nua_i_info:
		sip_i_info(nua, profile, nh, session, sip, tags);
		break;
	case nua_r_refer:
		break;
	case nua_i_refer:
        if (session) {
            sip_i_refer(nua, profile, nh, session, sip, tags);
        }
		break;
    case nua_r_subscribe:
		sip_r_subscribe(status, phrase, nua, profile, nh, sofia_private, sip, tags);
		break;
	case nua_i_subscribe:
		sip_i_subscribe(status, phrase, nua, profile, nh, sofia_private, sip, tags);
		break;
	default:
		if (status > 100) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "%s: unknown event %d: %03d %s\n", 
							  nua_event_name (event), event, status, phrase);
        } else {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "%s: unknown event %d\n", nua_event_name (event), event);
        }
		break;
	}

 done:

	if (session) {
		switch_core_session_rwunlock(session);
	}
}


static void unreg(sofia_profile_t *profile)
{
	outbound_reg_t *oregp;
    for (oregp = profile->registrations; oregp; oregp = oregp->next) {
        if (oregp->sofia_private) {
            free(oregp->sofia_private);
            nua_handle_bind(oregp->nh, NULL);
            oregp->sofia_private = NULL;
        }
		nua_handle_destroy(oregp->nh);
	}
}

static void check_oreg(sofia_profile_t *profile, time_t now)
{
	outbound_reg_t *oregp;
	for (oregp = profile->registrations; oregp; oregp = oregp->next) {
		int ss_state = nua_callstate_authenticating;
		reg_state_t ostate = oregp->state;

		switch(ostate) {
		case REG_STATE_REGISTER:
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE,  "registered %s\n", oregp->name);
			oregp->expires = now + oregp->freq;
			oregp->state = REG_STATE_REGED;
			break;
		case REG_STATE_UNREGED:
			if ((oregp->nh = nua_handle(oregp->profile->nua, NULL,
										NUTAG_URL(oregp->register_proxy),
										SIPTAG_TO_STR(oregp->register_to),
										NUTAG_CALLSTATE_REF(ss_state),
										SIPTAG_FROM_STR(oregp->register_from), TAG_END()))) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE,  "registering %s\n", oregp->name);	
                
                if (!(oregp->sofia_private = malloc(sizeof(*oregp->sofia_private)))) {
                    abort();
                }
                memset(oregp->sofia_private, 0, sizeof(*oregp->sofia_private));

				oregp->sofia_private->oreg = oregp;
				nua_handle_bind(oregp->nh, oregp->sofia_private);

				nua_register(oregp->nh,
							SIPTAG_FROM_STR(oregp->register_from),
							SIPTAG_CONTACT_STR(oregp->register_from),
							SIPTAG_EXPIRES_STR(oregp->expires_str),
							NUTAG_REGISTRAR(oregp->register_proxy),
							NUTAG_OUTBOUND("no-options-keepalive"),
							NUTAG_OUTBOUND("no-validate"),
							NUTAG_KEEPALIVE(0),
							TAG_NULL());
				oregp->retry = now + 10;
				oregp->state = REG_STATE_TRYING;
			} else {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR,  "Error registering %s\n", oregp->name);
				oregp->state = REG_STATE_FAILED;
			}
			break;

		case REG_STATE_TRYING:
			if (oregp->retry && now >= oregp->retry) {
				oregp->state = REG_STATE_UNREGED;
				oregp->retry = 0;
			}
			break;
		default:
			if (oregp->expires && now >= oregp->expires) {
				oregp->state = REG_STATE_UNREGED;
				oregp->expires = 0;
			}
			break;
		}
	}

}

#define IREG_SECONDS 30
#define OREG_SECONDS 1
static void *SWITCH_THREAD_FUNC profile_thread_run(switch_thread_t *thread, void *obj)
{
	sofia_profile_t *profile = (sofia_profile_t *) obj;
	switch_memory_pool_t *pool;
	sip_alias_node_t *node;
	uint32_t ireg_loops = 0;
	uint32_t oreg_loops = 0;
	switch_core_db_t *db;
	switch_event_t *s_event;

	profile->s_root = su_root_create(NULL);
	profile->home = su_home_new(sizeof(*profile->home));

	profile->nua = nua_create(profile->s_root, /* Event loop */
							  event_callback, /* Callback for processing events */
							  profile, /* Additional data to pass to callback */
							  NUTAG_URL(profile->url),
							  NTATAG_UDP_MTU(65536),
							  TAG_END()); /* Last tag should always finish the sequence */

	nua_set_params(profile->nua,
				   //NUTAG_EARLY_MEDIA(1),				   
				   NUTAG_AUTOANSWER(0),
				   NUTAG_AUTOALERT(0),
				   NUTAG_ALLOW("REGISTER"),
				   NUTAG_ALLOW("REFER"),
				   NUTAG_ALLOW("INFO"),
				   TAG_IF((profile->pflags & PFLAG_PRESENCE), NUTAG_ALLOW("PUBLISH")),
				   TAG_IF((profile->pflags & PFLAG_PRESENCE), NUTAG_ALLOW("NOTIFY")),
				   TAG_IF((profile->pflags & PFLAG_PRESENCE), NUTAG_ALLOW("SUBSCRIBE")),
				   TAG_IF((profile->pflags & PFLAG_PRESENCE), NUTAG_ENABLEMESSAGE(1)),
				   TAG_IF((profile->pflags & PFLAG_PRESENCE), NUTAG_ALLOW_EVENTS("presence")),
				   TAG_IF((profile->pflags & PFLAG_PRESENCE), NUTAG_ALLOW_EVENTS("presence.winfo")),
				   SIPTAG_SUPPORTED_STR("100rel, precondition"),
				   SIPTAG_USER_AGENT_STR(SOFIA_USER_AGENT),
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
					   NUTAG_AUTOALERT(0),
					   NUTAG_ALLOW("REGISTER"),
					   NUTAG_ALLOW("REFER"),
					   NUTAG_ALLOW("INFO"),
					   TAG_IF((profile->pflags & PFLAG_PRESENCE), NUTAG_ALLOW("PUBLISH")),
					   TAG_IF((profile->pflags & PFLAG_PRESENCE), NUTAG_ENABLEMESSAGE(1)),
					   SIPTAG_SUPPORTED_STR("100rel, precondition"),
					   SIPTAG_USER_AGENT_STR(SOFIA_USER_AGENT),
					   TAG_END());
		
	}


	if ((db = switch_core_db_open_file(profile->dbname))) {
		switch_core_db_test_reactive(db, "select contact from sip_registrations", reg_sql);
		switch_core_db_test_reactive(db, "select contact from sip_subscriptions", sub_sql);
		switch_core_db_test_reactive(db, "select * from sip_authentication", auth_sql);
	} else {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Cannot Open SQL Database!\n");
		return NULL;
	}


	switch_mutex_init(&profile->ireg_mutex, SWITCH_MUTEX_NESTED, profile->pool);
	switch_mutex_init(&profile->oreg_mutex, SWITCH_MUTEX_NESTED, profile->pool);

	ireg_loops = IREG_SECONDS;
	oreg_loops = OREG_SECONDS;

	if (switch_event_create(&s_event, SWITCH_EVENT_PUBLISH) == SWITCH_STATUS_SUCCESS) {
		switch_event_add_header(s_event, SWITCH_STACK_BOTTOM, "service", "_sip._udp");
		switch_event_add_header(s_event, SWITCH_STACK_BOTTOM, "port", "%d", profile->sip_port);
		switch_event_fire(&s_event);
	}

	if (switch_event_create(&s_event, SWITCH_EVENT_PUBLISH) == SWITCH_STATUS_SUCCESS) {
		switch_event_add_header(s_event, SWITCH_STACK_BOTTOM, "service", "_sip._tcp");
		switch_event_add_header(s_event, SWITCH_STACK_BOTTOM, "port", "%d", profile->sip_port);
		switch_event_fire(&s_event);
	}

	if (switch_event_create(&s_event, SWITCH_EVENT_PUBLISH) == SWITCH_STATUS_SUCCESS) {
		switch_event_add_header(s_event, SWITCH_STACK_BOTTOM, "service", "_sip._sctp");
		switch_event_add_header(s_event, SWITCH_STACK_BOTTOM, "port", "%d", profile->sip_port);
		switch_event_fire(&s_event);
	}


	switch_mutex_lock(globals.hash_mutex);
	switch_core_hash_insert(globals.profile_hash, profile->name, profile);
	switch_mutex_unlock(globals.hash_mutex);

	if (profile->pflags & PFLAG_PRESENCE) {
		establish_presence(profile);
	}




	while(globals.running == 1) {
		if (++ireg_loops >= IREG_SECONDS) {
			check_expire(db, profile, time(NULL));
			ireg_loops = 0;
		}

		if (++oreg_loops >= OREG_SECONDS) {
			check_oreg(profile, time(NULL));
			oreg_loops = 0;
		}

		su_root_step(profile->s_root, 1000);
	}

	switch_core_db_close(db);
	unreg(profile);
	su_home_unref(profile->home);
	

	if (switch_event_create(&s_event, SWITCH_EVENT_UNPUBLISH) == SWITCH_STATUS_SUCCESS) {
		switch_event_add_header(s_event, SWITCH_STACK_BOTTOM, "service", "_sip._udp");
		switch_event_add_header(s_event, SWITCH_STACK_BOTTOM, "port", "%d", profile->sip_port);
		switch_event_fire(&s_event);
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
	switch_thread_create(&thread, thd_attr, profile_thread_run, profile, profile->pool);
}



static switch_status_t config_sofia(int reload)
{
	char *cf = "sofia.conf";
	switch_xml_t cfg, xml = NULL, xprofile, param, settings, profiles, registration, registrations;
	switch_status_t status = SWITCH_STATUS_SUCCESS;
	sofia_profile_t *profile = NULL;
	char url[512] = "";

	switch_mutex_lock(globals.mutex);
	globals.running = 1;
	switch_mutex_unlock(globals.mutex);

	if (!(xml = switch_xml_open_cfg(cf, &cfg, NULL))) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "open of %s failed\n", cf);
		status = SWITCH_STATUS_FALSE;
		goto done;
	}

	if ((settings = switch_xml_child(cfg, "global_settings"))) {
		for (param = switch_xml_child(settings, "param"); param; param = param->next) {
			char *var = (char *) switch_xml_attr_soft(param, "name");
			char *val = (char *) switch_xml_attr_soft(param, "value");
			if (!strcasecmp(var, "log-level")) {
				su_log_set_level(NULL, atoi(val));
			} else if (!strcasecmp(var, "log-level-trace")) {
				su_log_set_level(tport_log, atoi(val));
			}
		}
	}

	if ((profiles = switch_xml_child(cfg, "profiles"))) {
		for (xprofile = switch_xml_child(profiles, "profile"); xprofile; xprofile = xprofile->next) {
			if (!(settings = switch_xml_child(xprofile, "settings"))) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "No Settings, check the new config!\n", cf);
			} else {
				char *xprofilename = (char *) switch_xml_attr_soft(xprofile, "name");
				switch_memory_pool_t *pool = NULL;

				
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
				switch_core_hash_init(&profile->chat_hash, profile->pool);

				profile->dtmf_duration = 100;		
				profile->codec_ms = 20;

				for (param = switch_xml_child(settings, "param"); param; param = param->next) {
					char *var = (char *) switch_xml_attr_soft(param, "name");
					char *val = (char *) switch_xml_attr_soft(param, "value");

					if (!strcasecmp(var, "debug")) {
						profile->debug = atoi(val);
					} else if (!strcasecmp(var, "use-rtp-timer") && switch_true(val)) {
						switch_set_flag(profile, TFLAG_TIMER);
					} else if (!strcasecmp(var, "inbound-no-media") && switch_true(val)) {
						switch_set_flag(profile, TFLAG_INB_NOMEDIA);
					} else if (!strcasecmp(var, "rfc2833-pt")) {
						profile->te = (switch_payload_t) atoi(val);
					} else if (!strcasecmp(var, "sip-port")) {
						profile->sip_port = atoi(val);
					} else if (!strcasecmp(var, "vad")) {
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
					} else if (!strcasecmp(var, "ext-rtp-ip")) {
						profile->extrtpip = switch_core_strdup(profile->pool, val);
					} else if (!strcasecmp(var, "rtp-ip")) {
						profile->rtpip = switch_core_strdup(profile->pool, val);
					} else if (!strcasecmp(var, "sip-ip")) {
						profile->sipip = switch_core_strdup(profile->pool, val);
					} else if (!strcasecmp(var, "sip-domain")) {
						profile->sipdomain = switch_core_strdup(profile->pool, val);
					} else if (!strcasecmp(var, "rtp-timer-name")) {
						profile->timer_name = switch_core_strdup(profile->pool, val);
					} else if (!strcasecmp(var, "manage-presence")) {
						if (switch_true(val)) {
							profile->pflags |= PFLAG_PRESENCE;
						}
					} else if (!strcasecmp(var, "auth-calls")) {
						if (switch_true(val)) {
							profile->pflags |= PFLAG_AUTH_CALLS;
						}
					} else if (!strcasecmp(var, "accept-blind-reg")) {
						if (switch_true(val)) {
							profile->pflags |= PFLAG_BLIND_REG;
						}
					} else if (!strcasecmp(var, "auth-all-packets")) {
						if (switch_true(val)) {
							profile->pflags |= PFLAG_AUTH_ALL;
						}
					} else if (!strcasecmp(var, "full-id-in-dialplan")) {
						if (switch_true(val)) {
							profile->pflags |= PFLAG_FULL_ID;
						}
					} else if (!strcasecmp(var, "ext-sip-ip")) {
						profile->extsipip = switch_core_strdup(profile->pool, val);
					} else if (!strcasecmp(var, "bitpacking")) {
						if (!strcasecmp(val, "aal2")) {
							profile->codec_flags = SWITCH_CODEC_FLAG_AAL2;
						} 
					} else if (!strcasecmp(var, "username")) {
						profile->username = switch_core_strdup(profile->pool, val);
					} else if (!strcasecmp(var, "context")) {
						profile->context = switch_core_strdup(profile->pool, val);
					} else if (!strcasecmp(var, "alias")) {
						sip_alias_node_t *node;
						if ((node = switch_core_alloc(profile->pool, sizeof(*node)))) {
							if ((node->url = switch_core_strdup(profile->pool, val))) {
								node->next = profile->aliases;
								profile->aliases = node;
							}
						}
					} else if (!strcasecmp(var, "dialplan")) {
						profile->dialplan = switch_core_strdup(profile->pool, val);
					} else if (!strcasecmp(var, "max-calls")) {
						profile->max_calls = atoi(val);
					} else if (!strcasecmp(var, "codec-prefs")) {
						profile->codec_string = switch_core_strdup(profile->pool, val);
						profile->codec_order_last = switch_separate_string(profile->codec_string, ',', profile->codec_order, SWITCH_MAX_CODECS);
					} else if (!strcasecmp(var, "codec-ms")) {
						profile->codec_ms = atoi(val);
					} else if (!strcasecmp(var, "dtmf-duration")) {
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
					profile->dialplan = switch_core_strdup(profile->pool, "XML");
				}

				if (!profile->sipdomain) {
					profile->sipdomain = switch_core_strdup(profile->pool, profile->sipip);
				}

				snprintf(url, sizeof(url), "sip:mod_sofia@%s:%d", profile->sipip, profile->sip_port);
				profile->url = switch_core_strdup(profile->pool, url);
			}
			if (profile) {
				if ((registrations = switch_xml_child(xprofile, "registrations"))) {
					for (registration = switch_xml_child(registrations, "registration"); registration; registration = registration->next) {
						char *name = (char *) switch_xml_attr_soft(registration, "name");
						outbound_reg_t *oreg;

						if (switch_strlen_zero(name)) {
							name = "anonymous";
						}

						if ((oreg = switch_core_alloc(profile->pool, sizeof(*oreg)))) {
							oreg->pool = profile->pool;
							oreg->profile = profile;
							oreg->name = switch_core_strdup(oreg->pool, name);
							oreg->freq = 0;

							for (param = switch_xml_child(registration, "param"); param; param = param->next) {
								char *var = (char *) switch_xml_attr_soft(param, "name");
								char *val = (char *) switch_xml_attr_soft(param, "value");
								
								if (!strcmp(var, "register-scheme")) {
									oreg->register_scheme = switch_core_strdup(oreg->pool, val);
								} else if (!strcmp(var, "register-realm")) {
									oreg->register_realm = switch_core_strdup(oreg->pool, val);
								} else if (!strcmp(var, "register-username")) {
									oreg->register_username = switch_core_strdup(oreg->pool, val);
								} else if (!strcmp(var, "register-password")) {
									oreg->register_password = switch_core_strdup(oreg->pool, val);
								} else if (!strcmp(var, "register-from")) {
									oreg->register_from = switch_core_strdup(oreg->pool, val);
								} else if (!strcmp(var, "register-to")) {
									oreg->register_to = switch_core_strdup(oreg->pool, val);
								} else if (!strcmp(var, "register-proxy")) {
									oreg->register_proxy = switch_core_strdup(oreg->pool, val);
								} else if (!strcmp(var, "register-frequency")) {
									oreg->expires_str = switch_core_strdup(oreg->pool, val);
								}
							}
							if (switch_strlen_zero(oreg->register_scheme)) {
								oreg->register_scheme = switch_core_strdup(oreg->pool, "Digest");
							}
							if (switch_strlen_zero(oreg->register_realm)) {
								oreg->register_realm = switch_core_strdup(oreg->pool, "freeswitch.org");
							}
							if (switch_strlen_zero(oreg->register_username)) {
								oreg->register_username = switch_core_strdup(oreg->pool, "freeswitch");
							}
							if (switch_strlen_zero(oreg->register_password)) {
								oreg->register_password = switch_core_strdup(oreg->pool, "works");
							}							
							if (switch_strlen_zero(oreg->register_from)) {
								oreg->register_from = switch_core_strdup(oreg->pool, "FreeSWITCH <sip:freeswitch@freeswitch.org>");
							}
							if (switch_strlen_zero(oreg->register_to)) {
								oreg->register_to = switch_core_strdup(oreg->pool, "sip:freeswitch@freeswitch.org");
							}
							if (switch_strlen_zero(oreg->register_proxy)) {
								oreg->register_proxy = switch_core_strdup(oreg->pool, "sip:freeswitch@freeswitch.org");
							}

							if (switch_strlen_zero(oreg->expires_str)) {
								oreg->expires_str = switch_core_strdup(oreg->pool, "300");
							}

							oreg->freq = atoi(oreg->expires_str);
							oreg->next = profile->registrations;
							profile->registrations = oreg;

						}
					}
				}

				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "Started Profile %s [%s]\n", profile->name, url);
				launch_profile_thread(profile);
				profile = NULL;
			}
		}
	}
 done:
	if (xml) {
		switch_xml_free(xml);
	}

	return status;

}

static void event_handler(switch_event_t *event)
{
	char *subclass, *sql;

	if ((subclass = switch_event_get_header(event, "orig-event-subclass")) && !strcasecmp(subclass, MY_EVENT_REGISTER)) {
		char *from_user = switch_event_get_header(event, "orig-from-user");
		char *from_host = switch_event_get_header(event, "orig-from-host");
		char *contact_str = switch_event_get_header(event, "orig-contact");
		char *exp_str = switch_event_get_header(event, "orig-expires");
		char *rpid = switch_event_get_header(event, "orig-rpid");
		long expires = (long)time(NULL) + atol(exp_str);
		char *profile_name = switch_event_get_header(event, "orig-profile-name");
		sofia_profile_t *profile;
		char buf[512];

		if (!rpid) {
			rpid = "unknown";
		}

		if (!profile_name || !(profile = (sofia_profile_t *) switch_core_hash_find(globals.profile_hash, profile_name))) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Invalid Profile\n");
			return;
		}


		if (!find_reg_url(profile, from_user, from_host, buf, sizeof(buf))) {
			sql = switch_mprintf("insert into sip_registrations values ('%q','%q','%q','Regestered', '%q', %ld)", 
								 from_user,
								 from_host,
								 contact_str,
								 rpid,
								 expires);
		} else {
			sql = switch_mprintf("update sip_registrations set contact='%q', rpid='%q', expires=%ld where user='%q' and host='%q'",
								 contact_str,
								 rpid,
								 expires,
								 from_user,
								 from_host);
			
		}
	
		if (sql) {
			execute_sql(profile->dbname, sql, profile->ireg_mutex);
			switch_safe_free(sql);
			sql = NULL;
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Propagating registration for %s@%s->%s\n", 
							  from_user, from_host, contact_str);

		}

	}
}


static switch_status_t chat_send(char *proto, char *from, char *to, char *subject, char *body, char *hint)
{
	char buf[256];
	char *user, *host;
	sofia_profile_t *profile;
	char *ffrom = NULL;
	nua_handle_t *msg_nh;
	char *contact;

	if (to && (user = strdup(to))) {
		if ((host = strchr(user, '@'))) {
			*host++ = '\0';
		}
		
		if (!host || !(profile = (sofia_profile_t *) switch_core_hash_find(globals.profile_hash, host))) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Chat proto [%s]\nfrom [%s]\nto [%s]\n%s\nInvalid Profile %s\n", 
                              proto,
                              from,
                              to,
                              body ? body : "[no body]",
                              host ? host : "NULL");
			return SWITCH_STATUS_FALSE;
		}

        if (!find_reg_url(profile, user, host, buf, sizeof(buf))) {
			return SWITCH_STATUS_FALSE;
		}

		if (!strcmp(proto, SOFIA_CHAT_PROTO)) {
			from = hint;
		} else {
			char *fp, *p, *fu = NULL;

			if (!(fp = strdup(from))) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Memory Error!\n");
				return SWITCH_STATUS_FALSE;
			}

			if ((p = strchr(fp, '@'))) {
				*p = '\0';
				fu = strdup(fp);
				*p = '+';
			}
			
			ffrom = switch_mprintf("\"%s\" <sip:%s+%s@%s>", fu, proto, fp, profile->name);			
			from = ffrom;
			switch_safe_free(fu);
			switch_safe_free(fp);
		}

		contact = get_url_from_contact(buf, 1);
		msg_nh = nua_handle(profile->nua, NULL,
							SIPTAG_FROM_STR(from),
							NUTAG_URL(contact),
							SIPTAG_TO_STR(buf), // if this cries, add contact here too, change the 1 to 0 and omit the safe_free
							SIPTAG_CONTACT_STR(profile->url),
							TAG_END());

		switch_safe_free(contact);


		nua_message(msg_nh,
					SIPTAG_CONTENT_TYPE_STR("text/html"),
					SIPTAG_PAYLOAD_STR(body),
					TAG_END());
		
		
		switch_safe_free(ffrom);
		free(user);
	}
		
	return SWITCH_STATUS_SUCCESS;
}

static void cancel_presence(void) 
{
	char *sql, *errmsg = NULL;
	switch_core_db_t *db;
	sofia_profile_t *profile;
	switch_hash_index_t *hi;
    void *val;

	if ((sql = switch_mprintf("select 0,'unavailable','unavailable',* from sip_subscriptions where event='presence'"))) {
		for (hi = switch_hash_first(apr_hash_pool_get(globals.profile_hash), globals.profile_hash); hi; hi = switch_hash_next(hi)) {
			switch_hash_this(hi, NULL, NULL, &val);
			profile = (sofia_profile_t *) val;
			if (!(profile->pflags & PFLAG_PRESENCE)) {
				continue;
			}

			if (!(db = switch_core_db_open_file(profile->dbname))) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Error Opening DB %s\n", profile->dbname);
				continue;
			}
			switch_mutex_lock(profile->ireg_mutex);
			switch_core_db_exec(db, sql, sub_callback, profile, &errmsg);
			switch_mutex_unlock(profile->ireg_mutex);
			switch_core_db_close(db);
		}
		switch_safe_free(sql);
	}

}

static void establish_presence(sofia_profile_t *profile) 
{
	char *sql, *errmsg = NULL;
	switch_core_db_t *db;

	if (!(db = switch_core_db_open_file(profile->dbname))) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Error Opening DB %s\n", profile->dbname);
		return;
	}

	if ((sql = switch_mprintf("select user,host,'Registered','unknown','' from sip_registrations"))) {
		switch_mutex_lock(profile->ireg_mutex);
		switch_core_db_exec(db, sql, resub_callback, profile, &errmsg);
		switch_mutex_unlock(profile->ireg_mutex);
		switch_safe_free(sql);
	}

	if ((sql = switch_mprintf("select sub_to_user,sub_to_host,'Online','unknown',proto from sip_subscriptions "
							  "where proto='ext' or proto='user' or proto='conf'"))) {
		switch_mutex_lock(profile->ireg_mutex);
		switch_core_db_exec(db, sql, resub_callback, profile, &errmsg);
		switch_mutex_unlock(profile->ireg_mutex);
		switch_safe_free(sql);
	}

	switch_core_db_close(db);

}



static char *translate_rpid(char *in, char *ext)
{
	char *r = NULL;

	if (in && (strstr(in, "null") || strstr(in, "NULL"))) {
		in = NULL;
	}

	if (!in) {
		in = ext;
	}

	if (!in) {
		return NULL;
	}
	
	if (!strcasecmp(in, "dnd")) {
		r = "busy";
	}

	if (ext && !strcasecmp(ext, "away")) {
		r = "idle";
	}
	
	return r;
}

static void pres_event_handler(switch_event_t *event)
{
	sofia_profile_t *profile;
	switch_hash_index_t *hi;
    void *val;
	char *from = switch_event_get_header(event, "from");
	char *proto = switch_event_get_header(event, "proto");
	char *rpid = switch_event_get_header(event, "rpid");
	char *status= switch_event_get_header(event, "status");
	char *event_type = switch_event_get_header(event, "event_type");
	//char *event_subtype = switch_event_get_header(event, "event_subtype");
	char *sql = NULL;
	char *euser = NULL, *user = NULL, *host = NULL;
	char *errmsg;
	switch_core_db_t *db;


	if (rpid && !strcasecmp(rpid, "n/a")) {
		rpid = NULL;
	}

	if (status && !strcasecmp(status, "n/a")) {
		status = NULL;
	}

	if (rpid) {
		rpid = translate_rpid(rpid, status);
	} 

	if (!status) {
		status = "Available";

		if (rpid) {
			if (!strcasecmp(rpid, "busy")) {
				status = "Busy";
			} else if (!strcasecmp(rpid, "unavailable")) {
				status = "Idle";
			} else if (!strcasecmp(rpid, "away")) {
				status = "Idle";
			}
		}
	}

	if (!rpid) {
		rpid = "unknown";
	}

	if (event->event_id == SWITCH_EVENT_ROSTER) {

		if (from) {
			sql = switch_mprintf("select 1,'%q','%q',* from sip_subscriptions where event='presence' and full_from like '%%%q%%'", status, rpid, from);
		} else {
			sql = switch_mprintf("select 1,'%q','%q',* from sip_subscriptions where event='presence'", status, rpid);
		}

		for (hi = switch_hash_first(apr_hash_pool_get(globals.profile_hash), globals.profile_hash); hi; hi = switch_hash_next(hi)) {
			switch_hash_this(hi, NULL, NULL, &val);
			profile = (sofia_profile_t *) val;
			if (!(profile->pflags & PFLAG_PRESENCE)) {
				continue;
			}

			if (sql) {
				if (!(db = switch_core_db_open_file(profile->dbname))) {
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Error Opening DB %s\n", profile->dbname);
					continue;
				}
				switch_mutex_lock(profile->ireg_mutex);
				switch_core_db_exec(db, sql, sub_callback, profile, &errmsg);
				switch_mutex_unlock(profile->ireg_mutex);
				switch_core_db_close(db);
			}

		}

		return;
	}

	if (switch_strlen_zero(event_type)) {
		event_type="presence";
	}

	if ((user = strdup(from))) {
		if ((host = strchr(user, '@'))) {
			char *p;
			*host++ = '\0';
			if ((p = strchr(host, '/'))) {
				*p = '\0';
			}
		} else {
			switch_safe_free(user);
			return;
		}
		if ((euser = strchr(user, '+'))) {
			euser++;
		} else {
			euser = user;
		}
		
	} else {
		return;
	}


	switch(event->event_id) {
    case SWITCH_EVENT_PRESENCE_PROBE: 
        if (proto) {
            switch_core_db_t *db = NULL;
            char *to = switch_event_get_header(event, "to");
            char *user, *euser, *host, *p;

            if (!to || !(user = strdup(to))) {
                return;
            }

            if ((host = strchr(user, '@'))) {
                *host++ = '\0';
            }
            euser = user;
            if ((p = strchr(euser, '+'))) {
                euser = (p+1);
            }

            if (euser && host && 
                (sql = switch_mprintf("select user,host,status,rpid,'' from sip_registrations where user='%q' and host='%q'", euser, host)) &&
                (profile = (sofia_profile_t *) switch_core_hash_find(globals.profile_hash, host))) {
                if (!(db = switch_core_db_open_file(profile->dbname))) {
                    switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Error Opening DB %s\n", profile->dbname);
                    switch_safe_free(user);
                    switch_safe_free(sql);
                    return;
                }

                switch_mutex_lock(profile->ireg_mutex);
                switch_core_db_exec(db, sql, resub_callback, profile, &errmsg);
                switch_mutex_unlock(profile->ireg_mutex);
                switch_safe_free(sql);
            }
            switch_safe_free(user);
            switch_core_db_close(db);
        }
        return;
	case SWITCH_EVENT_PRESENCE_IN:
		sql = switch_mprintf("select 1,'%q','%q',* from sip_subscriptions where proto='%q' and event='%q' and sub_to_user='%q' and sub_to_host='%q'", 
							 status , rpid, proto, event_type, euser, host);
		break;
	case SWITCH_EVENT_PRESENCE_OUT:
		sql = switch_mprintf("select 0,'%q','%q',* from sip_subscriptions where proto='%q' and event='%q' and sub_to_user='%q' and sub_to_host='%q'", 
							 status, rpid, proto, event_type, euser, host);
		break;
	default:
		break;
	}

    for (hi = switch_hash_first(apr_hash_pool_get(globals.profile_hash), globals.profile_hash); hi; hi = switch_hash_next(hi)) {
        switch_hash_this(hi, NULL, NULL, &val);
        profile = (sofia_profile_t *) val;
        if (!(profile->pflags & PFLAG_PRESENCE)) {
			continue;
        }

		if (sql) {
			if (!(db = switch_core_db_open_file(profile->dbname))) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Error Opening DB %s\n", profile->dbname);
				continue;
			}
			switch_mutex_lock(profile->ireg_mutex);
			switch_core_db_exec(db, sql, sub_callback, profile, &errmsg);
			switch_mutex_unlock(profile->ireg_mutex);
			
			switch_core_db_close(db);
		}
	}

	switch_safe_free(sql);
	switch_safe_free(user);
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

	memset(&globals, 0, sizeof(globals));
	switch_mutex_init(&globals.mutex, SWITCH_MUTEX_NESTED, module_pool);
	

	if (switch_event_bind((char *) modname, SWITCH_EVENT_CUSTOM, MULTICAST_EVENT, event_handler, NULL) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Couldn't bind!\n");
		return SWITCH_STATUS_TERM;
	}

	su_init();
	su_log_redirect(NULL, logger, NULL);
	su_log_redirect(tport_log, logger, NULL);

	switch_core_hash_init(&globals.profile_hash, module_pool);
	switch_mutex_init(&globals.hash_mutex, SWITCH_MUTEX_NESTED, module_pool);

	config_sofia(0);


	if (switch_event_bind((char *) modname, SWITCH_EVENT_PRESENCE_IN, SWITCH_EVENT_SUBCLASS_ANY, pres_event_handler, NULL) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Couldn't bind!\n");
		return SWITCH_STATUS_GENERR;
	}

	if (switch_event_bind((char *) modname, SWITCH_EVENT_PRESENCE_OUT, SWITCH_EVENT_SUBCLASS_ANY, pres_event_handler, NULL) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Couldn't bind!\n");
		return SWITCH_STATUS_GENERR;
	}

	if (switch_event_bind((char *) modname, SWITCH_EVENT_PRESENCE_PROBE, SWITCH_EVENT_SUBCLASS_ANY, pres_event_handler, NULL) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Couldn't bind!\n");
		return SWITCH_STATUS_GENERR;
	}

	if (switch_event_bind((char *) modname, SWITCH_EVENT_ROSTER, SWITCH_EVENT_SUBCLASS_ANY, pres_event_handler, NULL) != SWITCH_STATUS_SUCCESS) {
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

	cancel_presence();

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

/* For Emacs:
 * Local Variables:
 * mode:c
 * indent-tabs-mode:nil
 * tab-width:4
 * c-basic-offset:4
 * End:
 * For VIM:
 * vim:set softtabstop=4 shiftwidth=4 tabstop=4 expandtab:
 */
