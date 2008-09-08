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
 * Bret McDanel <trixter AT 0xdecafbad.com>
 * Marcel Barbulescu <marcelbarbulescu@gmail.com>
 *
 *
 * mod_sofia.h -- SOFIA SIP Endpoint
 *
 */

/*Defines etc..*/
/*************************************************************************************************************************************************************/
#define IREG_SECONDS 30
#define GATEWAY_SECONDS 1
#define SOFIA_QUEUE_SIZE 50000
#define HAVE_APR
#include <switch.h>
#include <switch_version.h>
#ifdef SWITCH_HAVE_ODBC
#include <switch_odbc.h>
#endif
#define SOFIA_NAT_SESSION_TIMEOUT 20
#define SOFIA_MAX_ACL 100
#ifdef _MSC_VER
#define HAVE_FUNCTION 1
#else
#define HAVE_FUNC 1
#endif

#define MODNAME "mod_sofia"
static const switch_state_handler_table_t noop_state_handler = { 0 };
struct sofia_gateway;
typedef struct sofia_gateway sofia_gateway_t;

struct sofia_profile;
typedef struct sofia_profile sofia_profile_t;
#define NUA_MAGIC_T sofia_profile_t

typedef struct sofia_private sofia_private_t;

struct private_object;
typedef struct private_object private_object_t;
#define NUA_HMAGIC_T sofia_private_t

#define SOFIA_SESSION_TIMEOUT "sofia_session_timeout"
#define MY_EVENT_REGISTER "sofia::register"
#define MY_EVENT_UNREGISTER "sofia::unregister"
#define MY_EVENT_EXPIRE "sofia::expire"

#define MULTICAST_EVENT "multicast::event"
#define SOFIA_REPLACES_HEADER "_sofia_replaces_"
#define SOFIA_USER_AGENT "FreeSWITCH-mod_sofia/" SWITCH_VERSION_MAJOR "." SWITCH_VERSION_MINOR "." SWITCH_VERSION_MICRO "-" SWITCH_VERSION_REVISION
#define SOFIA_CHAT_PROTO "sip"
#define SOFIA_SIP_HEADER_PREFIX "sip_h_"
#define SOFIA_SIP_BYE_HEADER_PREFIX "sip_bye_h_"
#define SOFIA_SIP_HEADER_PREFIX_T "~sip_h_"
#define SOFIA_DEFAULT_PORT "5060"
#define SOFIA_DEFAULT_TLS_PORT "5061"
#define SOFIA_REFER_TO_VARIABLE "sip_refer_to"
#define SOFIA_SECURE_MEDIA_VARIABLE "sip_secure_media"
#define SOFIA_SECURE_MEDIA_CONFIRMED_VARIABLE "sip_secure_media_confirmed"
#define SOFIA_HAS_CRYPTO_VARIABLE "sip_has_crypto"
#define SOFIA_CRYPTO_MANDATORY_VARIABLE "sip_crypto_mandatory"

#include <sofia-sip/nua.h>
#include <sofia-sip/sip_status.h>
#include <sofia-sip/sdp.h>
#include <sofia-sip/sip_protos.h>
#include <sofia-sip/auth_module.h>
#include <sofia-sip/su_md5.h>
#include <sofia-sip/su_log.h>
#include <sofia-sip/nea.h>
#include <sofia-sip/msg_addr.h>
#include "nua_stack.h"

typedef enum {
	DTMF_2833,
	DTMF_INFO,
	DTMF_NONE
} sofia_dtmf_t;

struct sofia_private {
	char uuid[SWITCH_UUID_FORMATTED_LENGTH + 1];
	sofia_gateway_t *gateway;
	char gateway_name[512];
	int destroy_nh;
	int destroy_me;
	int is_call;
};

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
	MFLAG_REFER = (1 << 0),
	MFLAG_REGISTER = (1 << 1)
} MFLAGS;

typedef enum {
	PFLAG_AUTH_CALLS = (1 << 0),
	PFLAG_BLIND_REG = (1 << 1),
	PFLAG_AUTH_ALL = (1 << 2),
	PFLAG_FULL_ID = (1 << 3),
	PFLAG_PRESENCE = (1 << 4),
	PFLAG_PASS_RFC2833 = (1 << 5),
	PFLAG_DISABLE_TRANSCODING = (1 << 6),
	PFLAG_REWRITE_TIMESTAMPS = (1 << 7),
	PFLAG_RUNNING = (1 << 8),
	PFLAG_RESPAWN = (1 << 9),
	PFLAG_GREEDY = (1 << 10),
	PFLAG_MULTIREG = (1 << 11),
	PFLAG_SUPPRESS_CNG = (1 << 12),
	PFLAG_TLS = (1 << 13),
	PFLAG_CHECKUSER = (1 << 14),
	PFLAG_SECURE = (1 << 15),
	PFLAG_BLIND_AUTH = (1 << 16),
	PFLAG_WORKER_RUNNING = (1 << 17),
	PFLAG_UNREG_OPTIONS_FAIL = (1 << 18),
	PFLAG_DISABLE_TIMER = (1 << 19),
	PFLAG_DISABLE_100REL = (1 << 20),
	PFLAG_AGGRESSIVE_NAT_DETECTION = (1 << 21),
	PFLAG_RECIEVED_IN_NAT_REG_CONTACT = (1 << 22),
	PFLAG_3PCC = (1 << 23),
	PFLAG_DISABLE_RTP_AUTOADJ = (1 << 24),
	PFLAG_DISABLE_SRTP_AUTH = (1 << 25),
	PFLAG_FUNNY_STUN = (1 << 26),
	PFLAG_STUN_ENABLED = (1 << 27),
	PFLAG_STUN_AUTO_DISABLE = (1 << 28)
} PFLAGS;

typedef enum {
	PFLAG_NDLB_TO_IN_200_CONTACT = (1 << 0),
	PFLAG_NDLB_BROKEN_AUTH_HASH = (1 << 1)
} sofia_NDLB_t;

typedef enum {
	STUN_FLAG_SET = (1 << 0),
	STUN_FLAG_PING = (1 << 1),
	STUN_FLAG_FUNNY = (1 << 2)
} STUNFLAGS;

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
	TFLAG_VAD_IN = (1 << 11),
	TFLAG_VAD_OUT = (1 << 12),
	TFLAG_VAD = (1 << 13),
	TFLAG_USE_ME = (1 << 14),
	TFLAG_READY = (1 << 15),
	TFLAG_REINVITE = (1 << 16),
	TFLAG_REFER = (1 << 17),
	TFLAG_NOHUP = (1 << 18),
	TFLAG_XFER = (1 << 19),
	TFLAG_NAT = (1 << 20),
	TFLAG_BUGGY_2833 = (1 << 21),
	TFLAG_SIP_HOLD = (1 << 22),
	TFLAG_INB_NOMEDIA = (1 << 23),
	TFLAG_LATE_NEGOTIATION = (1 << 24),
	TFLAG_SDP = (1 << 25),
	TFLAG_VIDEO = (1 << 26),
	TFLAG_TPORT_LOG = (1 << 27),
	TFLAG_SENT_UPDATE = (1 << 28),
	TFLAG_PROXY_MEDIA = (1 << 29)
} TFLAGS;

struct mod_sofia_globals {
	switch_memory_pool_t *pool;
	switch_hash_t *profile_hash;
	switch_hash_t *gateway_hash;
	switch_mutex_t *hash_mutex;
	uint32_t callid;
	int32_t running;
	int32_t threads;
	switch_mutex_t *mutex;
	char guess_ip[80];
	switch_queue_t *presence_queue;
	switch_queue_t *mwi_queue;
	struct sofia_private destroy_private;
	struct sofia_private keep_private;
	switch_event_node_t *in_node;
	switch_event_node_t *probe_node;
	switch_event_node_t *out_node;
	switch_event_node_t *roster_node;
	switch_event_node_t *custom_node;
	switch_event_node_t *mwi_node;
};
extern struct mod_sofia_globals mod_sofia_globals;

typedef enum {
	REG_FLAG_AUTHED = (1 << 0),
	REG_FLAG_CALLERID = (1 << 1)
} reg_flags_t;

typedef enum {
	REG_STATE_UNREGED,
	REG_STATE_TRYING,
	REG_STATE_REGISTER,
	REG_STATE_REGED,
	REG_STATE_UNREGISTER,
	REG_STATE_FAILED,
	REG_STATE_EXPIRED,
	REG_STATE_NOREG,
	REG_STATE_LAST
} reg_state_t;

typedef enum {
	SOFIA_TRANSPORT_UNKNOWN = 0,
	SOFIA_TRANSPORT_UDP,
	SOFIA_TRANSPORT_TCP,
	SOFIA_TRANSPORT_TCP_TLS,
	SOFIA_TRANSPORT_SCTP
} sofia_transport_t;

typedef enum {
	SOFIA_GATEWAY_DOWN,
	SOFIA_GATEWAY_UP
} sofia_gateway_status_t;

struct sofia_gateway {
	sofia_private_t *sofia_private;
	nua_handle_t *nh;
	sofia_profile_t *profile;
	char *name;
	char *register_scheme;
	char *register_realm;
	char *register_username;
	char *auth_username;
	char *register_password;
	char *register_from;
	char *register_contact;
	char *register_to;
	char *register_proxy;
	char *register_sticky_proxy;
	char *register_context;
	char *expires_str;
	char *register_url;
	sofia_transport_t register_transport;
	uint32_t freq;
	time_t expires;
	time_t retry;
	time_t ping;
	int pinging;
	sofia_gateway_status_t status;
	uint32_t ping_freq;
	uint32_t flags;
	int32_t retry_seconds;
	reg_state_t state;
	switch_memory_pool_t *pool;
	int deleted;
	struct sofia_gateway *next;
};

struct sofia_profile {
	int debug;
	char *name;
	char *domain_name;
	char *dbname;
	char *dialplan;
	char *context;
	char *extrtpip;
	char *rtpip;
	char *sipip;
	char *extsipip;
	char *username;
	char *url;
	char *bindurl;
	char *tls_url;
	char *tls_bindurl;
	char *sipdomain;
	char *timer_name;
	char *hold_music;
	char *bind_params;
	char *tls_bind_params;
	char *tls_cert_dir;
	char *reg_domain;
	char *reg_db_domain;
	char *user_agent;
	char *record_template;
	sofia_dtmf_t dtmf_type;
	int sip_port;
	int tls_sip_port;
	int tls_version;
	char *codec_string;
	int running;
	int dtmf_duration;
	unsigned int flags;
	unsigned int pflags;
	unsigned int mflags;
	unsigned int ndlb;
	uint32_t max_calls;
	uint32_t nonce_ttl;
	nua_t *nua;
	switch_memory_pool_t *pool;
	su_root_t *s_root;
	sip_alias_node_t *aliases;
	switch_payload_t te;
	switch_payload_t cng_pt;
	uint32_t codec_flags;
	switch_mutex_t *ireg_mutex;
	switch_mutex_t *gateway_mutex;
	sofia_gateway_t *gateways;
	su_home_t *home;
	switch_hash_t *chat_hash;
	switch_core_db_t *master_db;
	switch_thread_rwlock_t *rwlock;
	switch_mutex_t *flag_mutex;
	uint32_t inuse;
	time_t started;
	uint32_t session_timeout;
	uint32_t max_proceeding;
	uint32_t rtp_timeout_sec;
	uint32_t rtp_hold_timeout_sec;
	char *odbc_dsn;
	char *odbc_user;
	char *odbc_pass;
	switch_odbc_handle_t *master_odbc;
	switch_queue_t *sql_queue;
	char *acl[SOFIA_MAX_ACL];
	uint32_t acl_count;
	char *reg_acl[SOFIA_MAX_ACL];
	uint32_t reg_acl_count;
	char *nat_acl[SOFIA_MAX_ACL];
	uint32_t nat_acl_count;
	int rport_level;
};

struct private_object {
	sofia_private_t *sofia_private;
	uint32_t flags;
	switch_payload_t agreed_pt;
	switch_core_session_t *session;
	switch_channel_t *channel;
	switch_frame_t read_frame;
	char *codec_order[SWITCH_MAX_CODECS];
	int codec_order_last;
	const switch_codec_implementation_t *codecs[SWITCH_MAX_CODECS];
	int num_codecs;
	switch_codec_t read_codec;
	switch_codec_t write_codec;
	uint32_t codec_ms;
	switch_caller_profile_t *caller_profile;
	uint32_t timestamp_send;
	switch_rtp_t *rtp_session;
	int ssrc;
	sofia_profile_t *profile;
	char *local_sdp_audio_ip;
	switch_port_t local_sdp_audio_port;
	char *remote_sdp_audio_ip;
	switch_port_t remote_sdp_audio_port;
	char *adv_sdp_audio_ip;
	switch_port_t adv_sdp_audio_port;
	char *proxy_sdp_audio_ip;
	switch_port_t proxy_sdp_audio_port;
	char *reply_contact;
	char *from_uri;
	char *to_uri;
	char *from_address;
	char *to_address;
	char *callid;
	char *far_end_contact;
	char *contact_url;
	char *from_str;
	char *rpid;
	char *gateway_from_str;
	char *rm_encoding;
	char *iananame;
	char *rm_fmtp;
	char *fmtp_out;
	char *remote_sdp_str;
	char *local_sdp_str;
	char *orig_local_sdp_str;
	char *dest;
	char *dest_to;
	char *key;
	char *xferto;
	char *kick;
	char *origin;
	char *hash_key;
	char *chat_from;
	char *chat_to;
	char *e_dest;
	char *call_id;
	char *invite_contact;
	char *local_url;
	char *gateway_name;
	char *local_crypto_key;
	char *remote_crypto_key;
	char *record_route;
	char *extrtpip;
	char *stun_ip;
	switch_port_t stun_port;
	uint32_t stun_flags;
	int crypto_tag;
	unsigned char local_raw_key[SWITCH_RTP_MAX_CRYPTO_LEN];
	unsigned char remote_raw_key[SWITCH_RTP_MAX_CRYPTO_LEN];
	switch_rtp_crypto_key_type_t crypto_send_type;
	switch_rtp_crypto_key_type_t crypto_recv_type;
	switch_rtp_crypto_key_type_t crypto_type;
	unsigned long rm_rate;
	switch_payload_t pt;
	switch_mutex_t *flag_mutex;
	switch_payload_t te;
	switch_payload_t bte;
	switch_payload_t cng_pt;
	switch_payload_t bcng_pt;
	sofia_transport_t transport;
	nua_handle_t *nh;
	nua_handle_t *nh2;
	sip_contact_t *contact;
	uint32_t owner_id;
	uint32_t session_id;
	uint32_t max_missed_packets;
	uint32_t max_missed_hold_packets;
	/** VIDEO **/
	switch_frame_t video_read_frame;
	switch_codec_t video_read_codec;
	switch_codec_t video_write_codec;
	switch_rtp_t *video_rtp_session;
	switch_port_t adv_sdp_video_port;
	switch_port_t local_sdp_video_port;
	char *video_rm_encoding;
	switch_payload_t video_pt;
	unsigned long video_rm_rate;
	uint32_t video_codec_ms;
	char *remote_sdp_video_ip;
	switch_port_t remote_sdp_video_port;
	char *video_rm_fmtp;
	switch_payload_t video_agreed_pt;
	char *video_fmtp_out;
	uint32_t video_count;
	sofia_dtmf_t dtmf_type;
	int q850_cause;
	char *remote_ip;
	int remote_port;
	int got_bye;
};

struct callback_t {
	char *val;
	switch_size_t len;
	int matches;
};

typedef enum {
	REG_REGISTER,
	REG_AUTO_REGISTER,
	REG_INVITE,
} sofia_regtype_t;

typedef enum {
	AUTH_OK,
	AUTH_FORBIDDEN,
	AUTH_STALE,
} auth_res_t;

#define sofia_test_pflag(obj, flag) ((obj)->pflags & flag)
#define sofia_set_pflag(obj, flag) (obj)->pflags |= (flag)
#define sofia_set_pflag_locked(obj, flag) assert(obj->flag_mutex != NULL);\
switch_mutex_lock(obj->flag_mutex);\
(obj)->pflags |= (flag);\
switch_mutex_unlock(obj->flag_mutex);
#define sofia_clear_pflag_locked(obj, flag) switch_mutex_lock(obj->flag_mutex); (obj)->pflags &= ~(flag); switch_mutex_unlock(obj->flag_mutex);
#define sofia_clear_pflag(obj, flag) (obj)->pflags &= ~(flag)
#define sofia_copy_pflags(dest, src, flags) (dest)->pflags &= ~(flags);	(dest)->pflags |= ((src)->pflags & (flags))

/* Function Prototypes */
/*************************************************************************************************************************************************************/

switch_status_t sofia_glue_activate_rtp(private_object_t *tech_pvt, switch_rtp_flag_t myflags);

void sofia_glue_deactivate_rtp(private_object_t *tech_pvt);

void sofia_glue_set_local_sdp(private_object_t *tech_pvt, const char *ip, uint32_t port, const char *sr, int force);

void sofia_glue_tech_prepare_codecs(private_object_t *tech_pvt);

void sofia_glue_attach_private(switch_core_session_t *session, sofia_profile_t *profile, private_object_t *tech_pvt, const char *channame);

switch_status_t sofia_glue_tech_choose_port(private_object_t *tech_pvt, int force);

switch_status_t sofia_glue_do_invite(switch_core_session_t *session);

uint8_t sofia_glue_negotiate_sdp(switch_core_session_t *session, sdp_session_t *sdp);

void sofia_presence_establish_presence(sofia_profile_t *profile);

void sofia_handle_sip_i_refer(nua_t *nua, sofia_profile_t *profile, nua_handle_t *nh, switch_core_session_t *session, sip_t const *sip, tagi_t tags[]);

void sofia_handle_sip_i_info(nua_t *nua, sofia_profile_t *profile, nua_handle_t *nh, switch_core_session_t *session, sip_t const *sip, tagi_t tags[]);

void sofia_handle_sip_i_invite(nua_t *nua, sofia_profile_t *profile, nua_handle_t *nh, sofia_private_t *sofia_private, sip_t const *sip, tagi_t tags[]);

void sofia_reg_handle_sip_i_register(nua_t *nua, sofia_profile_t *profile, nua_handle_t *nh, sofia_private_t *sofia_private, sip_t const *sip, tagi_t tags[]);

void sofia_event_callback(nua_event_t event,
					int status,
					char const *phrase,
					nua_t *nua, sofia_profile_t *profile, nua_handle_t *nh, sofia_private_t *sofia_private, sip_t const *sip, tagi_t tags[]);

void *SWITCH_THREAD_FUNC sofia_profile_thread_run(switch_thread_t *thread, void *obj);

void launch_sofia_profile_thread(sofia_profile_t *profile);

switch_status_t sofia_presence_chat_send(char *proto, char *from, char *to, char *subject, char *body, char *hint);
void sofia_glue_tech_absorb_sdp(private_object_t *tech_pvt);
switch_status_t sofia_glue_tech_media(private_object_t *tech_pvt, const char *r_sdp);
char *sofia_reg_find_reg_url(sofia_profile_t *profile, const char *user, const char *host, char *val, switch_size_t len);
void event_handler(switch_event_t *event);
void sofia_presence_event_handler(switch_event_t *event);
void sofia_presence_mwi_event_handler(switch_event_t *event);
void sofia_presence_cancel(void);
switch_status_t config_sofia(int reload, char *profile_name);
void sofia_reg_auth_challenge(nua_t *nua, sofia_profile_t *profile, nua_handle_t *nh, sofia_regtype_t regtype, const char *realm, int stale);
auth_res_t sofia_reg_parse_auth(sofia_profile_t *profile, sip_authorization_t const *authorization, sip_t const *sip, const char *regstr, 
								char *np, size_t nplen, char *ip, switch_event_t **v_event, long exptime, sofia_regtype_t regtype, const char *to_user);

void sofia_reg_handle_sip_r_challenge(int status,
									  char const *phrase,
									  nua_t *nua, sofia_profile_t *profile, 
									  nua_handle_t *nh, switch_core_session_t *session, sofia_gateway_t *gateway, sip_t const *sip, tagi_t tags[]);
void sofia_reg_handle_sip_r_register(int status,
					char const *phrase,
					nua_t *nua, sofia_profile_t *profile, nua_handle_t *nh, sofia_private_t *sofia_private, sip_t const *sip, tagi_t tags[]);
void sofia_handle_sip_i_options(int status,
				   char const *phrase,
				   nua_t *nua, sofia_profile_t *profile, nua_handle_t *nh, sofia_private_t *sofia_private, sip_t const *sip, tagi_t tags[]);
void sofia_presence_handle_sip_i_publish(nua_t *nua, sofia_profile_t *profile, nua_handle_t *nh, sofia_private_t *sofia_private, sip_t const *sip, tagi_t tags[]);
void sofia_presence_handle_sip_i_message(int status,
				   char const *phrase,
				   nua_t *nua, sofia_profile_t *profile, nua_handle_t *nh, sofia_private_t *sofia_private, sip_t const *sip, tagi_t tags[]);
void sofia_presence_handle_sip_r_subscribe(int status,
					 char const *phrase,
					 nua_t *nua, sofia_profile_t *profile, nua_handle_t *nh, sofia_private_t *sofia_private, sip_t const *sip, tagi_t tags[]);
void sofia_presence_handle_sip_i_subscribe(int status,
					 char const *phrase,
					 nua_t *nua, sofia_profile_t *profile, nua_handle_t *nh, sofia_private_t *sofia_private, sip_t const *sip, tagi_t tags[]);

void sofia_glue_execute_sql(sofia_profile_t *profile, char **sqlp, switch_bool_t sql_already_dynamic);
void sofia_glue_actually_execute_sql(sofia_profile_t *profile, switch_bool_t master, char *sql, switch_mutex_t *mutex);
void sofia_reg_check_expire(sofia_profile_t *profile, time_t now, int reboot);
void sofia_reg_check_gateway(sofia_profile_t *profile, time_t now);
void sofia_reg_unregister(sofia_profile_t *profile);
switch_status_t sofia_glue_ext_address_lookup(sofia_profile_t *profile, private_object_t *tech_pvt, char **ip, switch_port_t *port, char *sourceip, switch_memory_pool_t *pool);

void sofia_glue_pass_sdp(private_object_t *tech_pvt, char *sdp);
int sofia_glue_get_user_host(char *in, char **user, char **host);
switch_call_cause_t sofia_glue_sip_cause_to_freeswitch(int status);
void sofia_glue_do_xfer_invite(switch_core_session_t *session);
uint8_t sofia_reg_handle_register(nua_t *nua, sofia_profile_t *profile, nua_handle_t *nh, sip_t const *sip, 
								  sofia_regtype_t regtype, char *key, uint32_t keylen, switch_event_t **v_event, const char *is_nat);
extern switch_endpoint_interface_t *sofia_endpoint_interface;
void sofia_presence_set_chat_hash(private_object_t *tech_pvt, sip_t const *sip);
switch_status_t sofia_on_hangup(switch_core_session_t *session);
char *sofia_glue_get_url_from_contact(char *buf, uint8_t to_dup);
void sofia_presence_set_hash_key(char *hash_key, int32_t len, sip_t const *sip);
void sofia_glue_sql_close(sofia_profile_t *profile);
int sofia_glue_init_sql(sofia_profile_t *profile);
char *sofia_overcome_sip_uri_weakness(switch_core_session_t *session, const char *uri, const sofia_transport_t transport, switch_bool_t uri_only, const char *params);
switch_bool_t sofia_glue_execute_sql_callback(sofia_profile_t *profile,
											  switch_bool_t master,
											  switch_mutex_t *mutex,
											  char *sql,
											  switch_core_db_callback_func_t callback,
											  void *pdata);
char *sofia_glue_execute_sql2str(sofia_profile_t *profile, switch_mutex_t *mutex, char *sql, char *resbuf, size_t len);
void sofia_glue_check_video_codecs(private_object_t *tech_pvt);
void sofia_glue_del_profile(sofia_profile_t *profile);

switch_status_t sofia_glue_add_profile(char *key, sofia_profile_t *profile);
void sofia_glue_release_profile__(const char *file, const char *func, int line, sofia_profile_t *profile);
#define sofia_glue_release_profile(x) sofia_glue_release_profile__(__FILE__, __SWITCH_FUNC__, __LINE__,  x)

sofia_profile_t *sofia_glue_find_profile__(const char *file, const char *func, int line, const char *key);
#define sofia_glue_find_profile(x) sofia_glue_find_profile__(__FILE__, __SWITCH_FUNC__, __LINE__,  x)

switch_status_t sofia_reg_add_gateway(char *key, sofia_gateway_t *gateway);
sofia_gateway_t *sofia_reg_find_gateway__(const char *file, const char *func, int line, const char *key);
#define sofia_reg_find_gateway(x) sofia_reg_find_gateway__(__FILE__, __SWITCH_FUNC__, __LINE__,  x)

void sofia_reg_release_gateway__(const char *file, const char *func, int line, sofia_gateway_t *gateway);
#define sofia_reg_release_gateway(x) sofia_reg_release_gateway__(__FILE__, __SWITCH_FUNC__, __LINE__, x);

#define check_decode(_var, _session) do {								\
		assert(_session);												\
		if (!switch_strlen_zero(_var)) {								\
			int d = 0;													\
			char *p;													\
			if (strchr(_var, '%')) {									\
				char *tmp = switch_core_session_strdup(_session, _var);	\
				switch_url_decode(tmp);									\
				_var = tmp;												\
				d++;													\
			}															\
			if ((p = strchr(_var, '"'))) {								\
				if (!d) {												\
					char *tmp = switch_core_session_strdup(_session, _var); \
					_var = tmp;											\
				}														\
				if ((p = strchr(_var, '"'))) {							\
					_var = p+1;											\
				}														\
				if ((p = strrchr(_var, '"'))) {							\
					*p = '\0';											\
				}														\
			}															\
		}																\
																		\
		if(_session) break;												\
	} while(!_session)


/*
 * Transport handling helper functions
 */
sofia_transport_t sofia_glue_via2transport(const sip_via_t *via);
sofia_transport_t sofia_glue_url2transport(const url_t *url);
sofia_transport_t sofia_glue_str2transport(const char *str);

const char *sofia_glue_transport2str(const sofia_transport_t tp);
char * sofia_glue_find_parameter(const char *str, const char *param);

int sofia_glue_transport_has_tls(const sofia_transport_t tp);
const char *sofia_glue_get_unknown_header(sip_t const *sip, const char *name);
switch_status_t sofia_glue_build_crypto(private_object_t *tech_pvt, int index, switch_rtp_crypto_key_type_t type, switch_rtp_crypto_direction_t direction);
void sofia_glue_tech_patch_sdp(private_object_t *tech_pvt);
switch_status_t sofia_glue_tech_proxy_remote_addr(private_object_t *tech_pvt);
void sofia_presence_event_thread_start(void);
void sofia_reg_expire_call_id(sofia_profile_t *profile, const char *call_id, int reboot);
switch_status_t sofia_glue_tech_choose_video_port(private_object_t *tech_pvt, int force);
switch_status_t sofia_glue_tech_set_video_codec(private_object_t *tech_pvt, int force);
const char *sofia_glue_strip_proto(const char *uri);
switch_status_t reconfig_sofia(sofia_profile_t *profile);
void sofia_glue_del_gateway(sofia_gateway_t *gp);
void sofia_reg_send_reboot(sofia_profile_t *profile, const char *user, const char *host, const char *contact, const char *user_agent);
void sofia_glue_restart_all_profiles(void);
