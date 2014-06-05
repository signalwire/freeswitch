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
 * Ken Rice <krice@freeswitch.org>
 * Paul D. Tinsley <pdt at jackhammer.org>
 * Bret McDanel <trixter AT 0xdecafbad.com>
 * Marcel Barbulescu <marcelbarbulescu@gmail.com>
 * Raymond Chandler <intralanman@gmail.com>
 * Emmanuel Schmidbauer <e.schmidbauer@gmail.com>
 *
 *
 * mod_sofia.h -- SOFIA SIP Endpoint
 *
 */

/*Defines etc..*/
/*************************************************************************************************************************************************************/
#define SOFIA_RECOVER "sofia"
#define MANUAL_BYE 1
#define SQL_CACHE_TIMEOUT 300
#define DEFAULT_NONCE_TTL 60
#define IREG_SECONDS 30
#define GATEWAY_SECONDS 1
#define SOFIA_QUEUE_SIZE 50000
#define HAVE_APR
#include <switch.h>
#define SOFIA_NAT_SESSION_TIMEOUT 90
#define SOFIA_MAX_ACL 100
#ifdef _MSC_VER
#define HAVE_FUNCTION 1
#else
#define HAVE_FUNC 1
#endif

#define MAX_CODEC_CHECK_FRAMES 50
#define MAX_MISMATCH_FRAMES 5
#define MODNAME "mod_sofia"
#define SOFIA_DEFAULT_CONTACT_USER MODNAME
static const switch_state_handler_table_t noop_state_handler = { 0 };
struct sofia_gateway;
typedef struct sofia_gateway sofia_gateway_t;

struct sofia_gateway_subscription;
typedef struct sofia_gateway_subscription sofia_gateway_subscription_t;

struct sofia_profile;
typedef struct sofia_profile sofia_profile_t;
#define NUA_MAGIC_T sofia_profile_t

typedef struct sofia_private sofia_private_t;

struct private_object;
typedef struct private_object private_object_t;
#define NUA_HMAGIC_T sofia_private_t

#define SOFIA_SESSION_TIMEOUT "sofia_session_timeout"
#define MY_EVENT_REGISTER "sofia::register"
#define MY_EVENT_PRE_REGISTER "sofia::pre_register"
#define MY_EVENT_REGISTER_ATTEMPT "sofia::register_attempt"
#define MY_EVENT_REGISTER_FAILURE "sofia::register_failure"
#define MY_EVENT_UNREGISTER "sofia::unregister"
#define MY_EVENT_EXPIRE "sofia::expire"
#define MY_EVENT_GATEWAY_STATE "sofia::gateway_state"
#define MY_EVENT_NOTIFY_REFER "sofia::notify_refer"
#define MY_EVENT_REINVITE "sofia::reinvite"
#define MY_EVENT_GATEWAY_ADD "sofia::gateway_add"
#define MY_EVENT_GATEWAY_DEL "sofia::gateway_delete"
#define MY_EVENT_RECOVERY "sofia::recovery_recv"
#define MY_EVENT_RECOVERY_SEND "sofia::recovery_send"
#define MY_EVENT_RECOVERY_RECOVERED "sofia::recovery_recovered"
#define MY_EVENT_ERROR "sofia::error"

#define MULTICAST_EVENT "multicast::event"
#define SOFIA_REPLACES_HEADER "_sofia_replaces_"
#define SOFIA_CHAT_PROTO "sip"
#define SOFIA_MULTIPART_PREFIX "sip_mp_"
#define SOFIA_MULTIPART_PREFIX_T "~sip_mp_"
#define SOFIA_SIP_HEADER_PREFIX "sip_h_"
#define SOFIA_SIP_INFO_HEADER_PREFIX "sip_info_h_"
#define SOFIA_SIP_INFO_HEADER_PREFIX_T "~sip_info_h_"
#define SOFIA_SIP_RESPONSE_HEADER_PREFIX "sip_rh_"
#define SOFIA_SIP_RESPONSE_HEADER_PREFIX_T "~sip_rh_"
#define SOFIA_SIP_BYE_HEADER_PREFIX "sip_bye_h_"
#define SOFIA_SIP_BYE_HEADER_PREFIX_T "~sip_bye_h_"
#define SOFIA_SIP_PROGRESS_HEADER_PREFIX "sip_ph_"
#define SOFIA_SIP_PROGRESS_HEADER_PREFIX_T "~sip_ph_"
#define SOFIA_SIP_HEADER_PREFIX_T "~sip_h_"
#define SOFIA_DEFAULT_PORT "5060"
#define SOFIA_DEFAULT_TLS_PORT "5061"
#define SOFIA_REFER_TO_VARIABLE "sip_refer_to"
//#define SOFIA_HAS_CRYPTO_VARIABLE "rtp_has_crypto"
//#define SOFIA_HAS_VIDEO_CRYPTO_VARIABLE "sip_has_video_crypto"
//#define SOFIA_CRYPTO_MANDATORY_VARIABLE "sip_crypto_mandatory"
#define FREESWITCH_SUPPORT "update_display,send_info"

#include <switch_stun.h>
#include <sofia-sip/nua.h>
#include <sofia-sip/sip_status.h>
#include <sofia-sip/sdp.h>
#include <sofia-sip/sip_protos.h>
#include <sofia-sip/auth_module.h>
#include <sofia-sip/su_md5.h>
#include <sofia-sip/su_log.h>
#include <sofia-sip/su_strlst.h>
#include <sofia-sip/nea.h>
#include <sofia-sip/msg_addr.h>
#include <sofia-sip/tport_tag.h>
#include <sofia-sip/sip_extra.h>
#include "nua_stack.h"
#include "sofia-sip/msg_parser.h"
#include "sofia-sip/sip_parser.h"
#include "sofia-sip/tport_tag.h"
#include <sofia-sip/msg.h>
#include <sofia-sip/uniqueid.h>

typedef enum {
	SOFIA_CONFIG_LOAD = 0,
	SOFIA_CONFIG_RESCAN,
	SOFIA_CONFIG_RESPAWN
} sofia_config_t;

typedef struct sofia_dispatch_event_s {
	nua_saved_event_t event[1];
	nua_handle_t *nh;
	nua_event_data_t const *data;
	su_time_t when;
	sip_t *sip;
	nua_t *nua;
	sofia_profile_t *profile;
	int save;
	switch_core_session_t *session;
	switch_core_session_t *init_session;
	switch_memory_pool_t *pool;
	struct sofia_dispatch_event_s *next;
} sofia_dispatch_event_t;

struct sofia_private {
	char uuid[SWITCH_UUID_FORMATTED_LENGTH + 1];
	char gateway_name[256];
	char auth_gateway_name[256];
	char *call_id;
	char *network_ip;
	char *network_port;
	char *key;
	char *user;
	char *realm;
	int destroy_nh;
	int destroy_me;
	int is_call;
	int is_static;
};

#define set_param(ptr,val) if (ptr) {free(ptr) ; ptr = NULL;} if (val) {ptr = strdup(val);}
#define set_anchor(t,m) if (t->Anchor) {delete t->Anchor;} t->Anchor = new SipMessage(m);
#define sofia_private_free(_pvt) if (_pvt && ! _pvt->is_static) {free(_pvt);} _pvt = NULL;


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
	PFLAG_AUTH_CALLS,
	PFLAG_AUTH_MESSAGES,
	PFLAG_PASS_RFC2833,
	PFLAG_BLIND_REG,
	PFLAG_AUTH_ALL,
	PFLAG_FULL_ID,
	PFLAG_MULTIREG_CONTACT,
	PFLAG_DISABLE_TRANSCODING,
	PFLAG_RUNNING,
	PFLAG_RESPAWN,
	PFLAG_MULTIREG,
	PFLAG_TLS,
	PFLAG_CHECKUSER,
	PFLAG_SECURE,
	PFLAG_BLIND_AUTH,
	PFLAG_WORKER_RUNNING,
	PFLAG_UNREG_OPTIONS_FAIL,
	PFLAG_DISABLE_TIMER,
	PFLAG_ENABLE_RFC5626,
	PFLAG_DISABLE_100REL,
	PFLAG_AGGRESSIVE_NAT_DETECTION,
	PFLAG_RECIEVED_IN_NAT_REG_CONTACT,
	PFLAG_3PCC,
	PFLAG_3PCC_PROXY,
	PFLAG_3PCC_REINVITE_BRIDGED_ON_ACK,
	PFLAG_CALLID_AS_UUID,
	PFLAG_UUID_AS_CALLID,
	PFLAG_MANAGE_SHARED_APPEARANCE,
	PFLAG_STANDBY,
	PFLAG_DISABLE_SRV,
	PFLAG_DISABLE_SRV503,
	PFLAG_DISABLE_NAPTR,
	PFLAG_NAT_OPTIONS_PING,
	PFLAG_UDP_NAT_OPTIONS_PING,
	PFLAG_ALL_REG_OPTIONS_PING,
	PFLAG_MESSAGE_QUERY_ON_REGISTER,
	PFLAG_MESSAGE_QUERY_ON_FIRST_REGISTER,
	PFLAG_MANUAL_REDIRECT,
	PFLAG_T38_PASSTHRU,
	PFLAG_AUTO_NAT,
	PFLAG_SIPCOMPACT,
	PFLAG_PRESENCE_PRIVACY,
	PFLAG_PASS_CALLEE_ID,
	PFLAG_LOG_AUTH_FAIL,
	PFLAG_FORWARD_MWI_NOTIFY,
	PFLAG_TRACK_CALLS,
	PFLAG_DESTROY,
	PFLAG_EXTENDED_INFO_PARSING,
	PFLAG_CID_IN_1XX,
	PFLAG_IN_DIALOG_CHAT,
	PFLAG_DEL_SUBS_ON_REG_REUSE,
	PFLAG_IGNORE_183NOSDP,
	PFLAG_PRESENCE_PROBE_ON_REGISTER,
	PFLAG_PRESENCE_ON_REGISTER,
	PFLAG_PRESENCE_ON_FIRST_REGISTER,
	PFLAG_NO_CONNECTION_REUSE,
	PFLAG_RTP_NOTIMER_DURING_BRIDGE,
	PFLAG_LIBERAL_DTMF,
 	PFLAG_AUTO_ASSIGN_PORT,
 	PFLAG_AUTO_ASSIGN_TLS_PORT,
	PFLAG_SHUTDOWN,
	PFLAG_PRESENCE_MAP,
	PFLAG_OPTIONS_RESPOND_503_ON_BUSY,
	PFLAG_PRESENCE_DISABLE_EARLY,
	PFLAG_CONFIRM_BLIND_TRANSFER,
	PFLAG_THREAD_PER_REG,
	PFLAG_MWI_USE_REG_CALLID,
	PFLAG_FIRE_MESSAGE_EVENTS,
	PFLAG_SEND_DISPLAY_UPDATE,
	PFLAG_RUNNING_TRANS,
	PFLAG_SOCKET_TCP_KEEPALIVE,
	PFLAG_TCP_KEEPALIVE,
	PFLAG_TCP_PINGPONG,
	PFLAG_TCP_PING2PONG,
	PFLAG_MESSAGES_RESPOND_200_OK,
	PFLAG_SUBSCRIBE_RESPOND_200_OK,
	PFLAG_PARSE_ALL_INVITE_HEADERS,
	PFLAG_TCP_UNREG_ON_SOCKET_CLOSE,
	PFLAG_TLS_ALWAYS_NAT,
	PFLAG_TCP_ALWAYS_NAT,
	PFLAG_ENABLE_CHAT,
	PFLAG_AUTH_SUBSCRIPTIONS,
	PFLAG_PROXY_REFER_REPLACES,
	/* No new flags below this line */
	PFLAG_MAX
} PFLAGS;

typedef enum {
	PFLAG_NDLB_TO_IN_200_CONTACT = (1 << 0),
	PFLAG_NDLB_BROKEN_AUTH_HASH = (1 << 1),
	PFLAG_NDLB_EXPIRES_IN_REGISTER_RESPONSE = (1 << 2)
} sofia_NDLB_t;

typedef enum {
	TFLAG_IO,
	TFLAG_CHANGE_MEDIA,
	TFLAG_OUTBOUND,
	TFLAG_READING,
	TFLAG_WRITING,
	TFLAG_HUP,
	TFLAG_RTP,
	TFLAG_BYE,
	TFLAG_ANS,
	TFLAG_EARLY_MEDIA,
	TFLAG_3PCC,
	TFLAG_READY,
	TFLAG_REFER,
	TFLAG_NOHUP,
	TFLAG_NOSDP_REINVITE,
	TFLAG_NAT,
	TFLAG_SIMPLIFY,
	TFLAG_SIP_HOLD,
	TFLAG_INB_NOMEDIA,
	TFLAG_LATE_NEGOTIATION,
	TFLAG_SDP,
	TFLAG_NEW_SDP,
	TFLAG_TPORT_LOG,
	TFLAG_SENT_UPDATE,
	TFLAG_PROXY_MEDIA,
	TFLAG_ZRTP_PASSTHRU,
	TFLAG_HOLD_LOCK,
	TFLAG_3PCC_HAS_ACK,
	TFLAG_UPDATING_DISPLAY,
	TFLAG_ENABLE_SOA,
	TFLAG_RECOVERED,
	TFLAG_AUTOFLUSH_DURING_BRIDGE,
	TFLAG_3PCC_INVITE,
	TFLAG_NOREPLY,
	TFLAG_GOT_ACK,
	TFLAG_CAPTURE,
	TFLAG_REINVITED,
	TFLAG_PASS_ACK,
	TFLAG_KEEPALIVE,
	/* No new flags below this line */
	TFLAG_MAX
} TFLAGS;

#define SOFIA_MAX_MSG_QUEUE 64
#define SOFIA_MSG_QUEUE_SIZE 1000

struct mod_sofia_globals {
	switch_memory_pool_t *pool;
	switch_hash_t *profile_hash;
	switch_hash_t *gateway_hash;
	switch_mutex_t *hash_mutex;
	uint32_t callid;
	int32_t running;
	int32_t threads;
	int cpu_count;
	int max_msg_queues;
	switch_mutex_t *mutex;
	char guess_ip[80];
	char hostname[512];
	switch_queue_t *presence_queue;
	switch_queue_t *msg_queue;
	switch_thread_t *msg_queue_thread[SOFIA_MAX_MSG_QUEUE];
	int msg_queue_len;
	struct sofia_private destroy_private;
	struct sofia_private keep_private;
	int guess_mask;
	char guess_mask_str[16];
	int debug_presence;
	int debug_sla;
	int auto_restart;
	int reg_deny_binding_fetch_and_no_lookup; /* backwards compatibility */
	int auto_nat;
	int tracelevel;
	char *capture_server;
	int rewrite_multicasted_fs_path;
	int presence_flush;
	switch_thread_t *presence_thread;
	uint32_t max_reg_threads;
	time_t presence_epoch;
};
extern struct mod_sofia_globals mod_sofia_globals;

typedef enum {
	REG_FLAG_AUTHED,
	REG_FLAG_CALLERID,

	/* No new flags below this line */
	REG_FLAG_MAX
} reg_flags_t;

typedef enum {
	CID_TYPE_RPID,
	CID_TYPE_PID,
	CID_TYPE_NONE
} sofia_cid_type_t;

typedef enum {
	REG_STATE_UNREGED,
	REG_STATE_TRYING,
	REG_STATE_REGISTER,
	REG_STATE_REGED,
	REG_STATE_UNREGISTER,
	REG_STATE_FAILED,
	REG_STATE_FAIL_WAIT,
	REG_STATE_EXPIRED,
	REG_STATE_NOREG,
	REG_STATE_TIMEOUT,
	REG_STATE_LAST
} reg_state_t;

typedef enum {
	SOFIA_TRANSPORT_UNKNOWN = 0,
	SOFIA_TRANSPORT_UDP,
	SOFIA_TRANSPORT_TCP,
	SOFIA_TRANSPORT_TCP_TLS,
	SOFIA_TRANSPORT_SCTP,
	SOFIA_TRANSPORT_WS,
	SOFIA_TRANSPORT_WSS
} sofia_transport_t;

typedef enum {
	SOFIA_TLS_VERSION_SSLv2 = (1 << 0),
	SOFIA_TLS_VERSION_SSLv3 = (1 << 1),
	SOFIA_TLS_VERSION_TLSv1 = (1 << 2),
	SOFIA_TLS_VERSION_TLSv1_1 = (1 << 3),
	SOFIA_TLS_VERSION_TLSv1_2 = (1 << 4),
} sofia_tls_version_t;

typedef enum {
	SOFIA_GATEWAY_DOWN,
	SOFIA_GATEWAY_UP,

	SOFIA_GATEWAY_INVALID
} sofia_gateway_status_t;

typedef enum {
	SUB_STATE_UNSUBED,
	SUB_STATE_TRYING,
	SUB_STATE_SUBSCRIBE,
	SUB_STATE_SUBED,
	SUB_STATE_UNSUBSCRIBE,
	SUB_STATE_FAILED,
	SUB_STATE_FAIL_WAIT,
	SUB_STATE_EXPIRED,
	SUB_STATE_NOSUB,
	v_STATE_LAST
} sub_state_t;

struct sofia_gateway_subscription {
	sofia_gateway_t *gateway;
	sofia_private_t *sofia_private;
	nua_handle_t *nh;
	char *expires_str;
	char *event;				/* eg, 'message-summary' to subscribe to MWI events */
	char *content_type;			/* eg, application/simple-message-summary in the case of MWI events */
	char *request_uri;
	uint32_t freq;
	int32_t retry_seconds;
	time_t expires;
	time_t retry;
	sub_state_t state;
	struct sofia_gateway_subscription *next;
};

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
	char *options_from_uri;
	char *options_to_uri;
	char *options_user_agent;
	char *register_contact;
	char *extension;
	char *real_extension;
	char *register_to;
	char *register_proxy;
	char *register_sticky_proxy;
	char *outbound_sticky_proxy;
	char *register_context;
	char *expires_str;
	char *register_url;
	char *from_domain;
	sofia_transport_t register_transport;
	uint32_t freq;
	time_t expires;
	time_t retry;
	time_t ping;
	time_t reg_timeout;
	int pinging;
	sofia_gateway_status_t status;
	uint32_t ping_freq;
	int ping_count;
	int ping_max;
	int ping_min;
	uint8_t flags[REG_FLAG_MAX];
	int32_t retry_seconds;
	int32_t reg_timeout_seconds;
	int32_t failure_status;
	reg_state_t state;
	switch_memory_pool_t *pool;
	int deleted;
	switch_event_t *ib_vars;
	switch_event_t *ob_vars;
	uint32_t ib_calls;
	uint32_t ob_calls;
	uint32_t ib_failed_calls;
	uint32_t ob_failed_calls;
	char uuid_str[SWITCH_UUID_FORMATTED_LENGTH + 1];
	int failures;
	struct sofia_gateway *next;
	sofia_gateway_subscription_t *subscriptions;
	int distinct_to;
	sofia_cid_type_t cid_type;
};

typedef enum {
	PRES_TYPE_NONE = 0,
	PRES_TYPE_FULL = 1,
	PRES_TYPE_PASSIVE = 2,
	PRES_TYPE_PNP = 3
} sofia_presence_type_t;

typedef enum {
	PRES_HELD_EARLY = 0,
	PRES_HELD_CONFIRMED = 1,
	PRES_HELD_TERMINATED = 2
} sofia_presence_held_calls_type_t;

typedef enum {
	MEDIA_OPT_NONE = 0,
	MEDIA_OPT_MEDIA_ON_HOLD = (1 << 0),
	MEDIA_OPT_BYPASS_AFTER_ATT_XFER = (1 << 1),
	MEDIA_OPT_BYPASS_AFTER_HOLD = (1 << 2)
} sofia_media_options_t;

typedef enum {
	   PAID_DEFAULT = 0,
	   PAID_USER,
	   PAID_USER_DOMAIN,
	   PAID_VERBATIM
} sofia_paid_type_t;

#define MAX_RTPIP 50

typedef enum {
	KA_MESSAGE,
	KA_INFO
} ka_type_t;

struct sofia_profile {
	int debug;
	int parse_invite_tel_params;
	char *name;
	char *domain_name;
	char *dbname;
	char *dialplan;
	char *context;
	char *shutdown_type;
	char *extrtpip;
	char *rtpip[MAX_RTPIP];
	char *jb_msec;
	switch_payload_t te;
	switch_payload_t recv_te;
	uint32_t rtpip_index;
	uint32_t rtpip_next;
	char *rtcp_audio_interval_msec;
	char *rtcp_video_interval_msec;

	char *sdp_username;
	char *sipip;
	char *extsipip;
	char *url;
	char *public_url;
	char *bindurl;
	char *ws_bindurl;
	char *wss_bindurl;
	char *tls_url;
	char *tls_public_url;
	char *tls_bindurl;
	char *tcp_public_contact;
	char *tls_public_contact;
	char *tcp_contact;
	char *tls_contact;
	char *sla_contact;
	char *sipdomain;
	char *timer_name;
	char *hold_music;
	char *outbound_proxy;
	char *bind_params;
	char *tls_bind_params;
	char *tls_cert_dir;
	char *reg_domain;
	char *sub_domain;
	char *reg_db_domain;
	char *user_agent;
	char *record_template;
	char *record_path;
	char *presence_hosts;
	char *presence_privacy;
	char *challenge_realm;
	char *pnp_prov_url;
	char *pnp_notify_profile;
	sofia_cid_type_t cid_type;
	switch_core_media_dtmf_t dtmf_type;
	int auto_restart;
	switch_port_t sip_port;
	switch_port_t extsipport;
	switch_port_t tls_sip_port;
	char *tls_ciphers;
	int tls_version;
	unsigned int tls_timeout;
	char *inbound_codec_string;
	char *outbound_codec_string;
	int running;
	int dtmf_duration;
	uint8_t flags[TFLAG_MAX];
	uint8_t pflags[PFLAG_MAX];
	switch_core_media_flag_t media_flags[SCMF_MAX];
	unsigned int mflags;
	unsigned int ndlb;
	unsigned int mndlb;
	uint32_t max_calls;
	uint32_t nonce_ttl;
	nua_t *nua;
	switch_memory_pool_t *pool;
	su_root_t *s_root;
	sip_alias_node_t *aliases;
	switch_payload_t cng_pt;
	uint32_t codec_flags;
	switch_mutex_t *ireg_mutex;
	switch_mutex_t *dbh_mutex;
	switch_mutex_t *gateway_mutex;
	sofia_gateway_t *gateways;
	//su_home_t *home;
	switch_hash_t *chat_hash;
	switch_hash_t *reg_nh_hash;
	switch_hash_t *mwi_debounce_hash;
	//switch_core_db_t *master_db;
	switch_thread_rwlock_t *rwlock;
	switch_mutex_t *flag_mutex;
	uint32_t inuse;
	time_t started;
	uint32_t session_timeout;
	uint32_t minimum_session_expires;
	uint32_t max_proceeding;
	uint32_t rtp_timeout_sec;
	uint32_t rtp_hold_timeout_sec;
	char *odbc_dsn;
	char *pre_trans_execute;
	char *post_trans_execute;
	char *inner_pre_trans_execute;
	char *inner_post_trans_execute;
	switch_sql_queue_manager_t *qm;
	char *acl[SOFIA_MAX_ACL];
	char *acl_pass_context[SOFIA_MAX_ACL];
	char *acl_fail_context[SOFIA_MAX_ACL];
	uint32_t acl_count;
	char *proxy_acl[SOFIA_MAX_ACL];
	uint32_t proxy_acl_count;
	char *reg_acl[SOFIA_MAX_ACL];
	uint32_t reg_acl_count;
	char *nat_acl[SOFIA_MAX_ACL];
	uint32_t nat_acl_count;
	char *cand_acl[SWITCH_MAX_CAND_ACL];
	uint32_t cand_acl_count;
	int server_rport_level;
	int client_rport_level;
	sofia_presence_type_t pres_type;
	sofia_presence_held_calls_type_t pres_held_type;
	sofia_media_options_t media_options;
	uint32_t force_subscription_expires;
	uint32_t force_publish_expires;
	char *user_agent_filter;
	uint32_t max_registrations_perext;
	switch_rtp_bug_flag_t auto_rtp_bugs;
	switch_rtp_bug_flag_t manual_rtp_bugs;
	switch_rtp_bug_flag_t manual_video_rtp_bugs;
	uint32_t ib_calls;
	uint32_t ob_calls;
	uint32_t ib_failed_calls;
	uint32_t ob_failed_calls;
	uint32_t timer_t1;
	uint32_t timer_t1x64;
	uint32_t timer_t2;
	uint32_t timer_t4;
	char *contact_user;
	char *local_network;
	uint32_t trans_timeout;
	switch_time_t last_sip_event;
	switch_time_t last_root_step;
	uint32_t step_timeout;
	uint32_t event_timeout;
	int watchdog_enabled;
	switch_mutex_t *gw_mutex;
	uint32_t queued_events;
	uint32_t last_cseq;
	int tls_only;
	int tls_verify_date;
	enum tport_tls_verify_policy tls_verify_policy;
	int tls_verify_depth;
	char *tls_passphrase;
	char *tls_verify_in_subjects_str;
	su_strlst_t *tls_verify_in_subjects;
	uint32_t sip_force_expires;
	uint32_t sip_expires_max_deviation;
	uint32_t sip_expires_late_margin;
	uint32_t sip_subscription_max_deviation;
	int ireg_seconds;
	sofia_paid_type_t paid_type;
	uint32_t rtp_digit_delay;
	switch_queue_t *event_queue;
	switch_thread_t *thread;
	switch_core_media_vflag_t vflags;
	char *ws_ip;
	switch_port_t ws_port;
	char *wss_ip;
	switch_port_t wss_port;
	int socket_tcp_keepalive;
	int tcp_keepalive;
	int tcp_pingpong;
	int tcp_ping2pong;
	ka_type_t keepalive;
	int bind_attempts;
	int bind_attempt_interval;
};


struct private_object {
	sofia_private_t *sofia_private;
	uint8_t flags[TFLAG_MAX];
	switch_core_session_t *session;
	switch_channel_t *channel;
	switch_media_handle_t *media_handle;
	switch_core_media_params_t mparams;
	switch_caller_profile_t *caller_profile;
	sofia_profile_t *profile;
	char *reply_contact;
	char *from_uri;
	char *to_uri;
	char *callid;
	char *contact_url;
	char *from_str;
	char *rpid;
	char *asserted_id;
	char *preferred_id;
	char *privacy;
	char *gateway_from_str;
	char *dest;
	char *dest_to;
	char *key;
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
	char *record_route;
	char *route_uri;
	char *x_freeswitch_support_remote;
	char *x_freeswitch_support_local;
	char *last_sent_callee_id_name;
	char *last_sent_callee_id_number;
	switch_mutex_t *flag_mutex;
	switch_mutex_t *sofia_mutex;
	switch_payload_t te;
	switch_payload_t recv_te;
	switch_payload_t bte;
	switch_payload_t cng_pt;
	switch_payload_t bcng_pt;
	sofia_transport_t transport;
	nua_handle_t *nh;
	nua_handle_t *nh2;
	sip_contact_t *contact;
	int q850_cause;
	int got_bye;
	nua_event_t want_event;
	switch_rtp_bug_flag_t rtp_bugs;
	char *user_via;
	char *redirected;
	sofia_cid_type_t cid_type;
	uint32_t session_timeout;
	enum nua_session_refresher session_refresher;
	char *respond_phrase;
	int respond_code;
	char *respond_dest;
	time_t last_vid_info;
	uint32_t keepalive;
};


struct callback_t {
	char *val;
	switch_size_t len;
	switch_console_callback_match_t *list;
	int matches;
	time_t time;
	const char *contact_str;
	long exptime;
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
	AUTH_RENEWED,
} auth_res_t;

typedef struct {
	char *to;
	char *contact;
	char *route;
	char *route_uri;
} sofia_destination_t;

typedef struct {
	char network_ip[80];
	int network_port;
	const char *is_nat;
	int is_auto_nat;
	int fs_path;
} sofia_nat_parse_t;


#define NUTAG_WITH_THIS_MSG(msg) nutag_with, tag_ptr_v(msg)



#define sofia_test_media_flag(obj, flag) ((obj)->media_flags[flag] ? 1 : 0)
#define sofia_set_media_flag(obj, flag) (obj)->media_flags[flag] = 1
#define sofia_set_media_flag_locked(obj, flag) assert(obj->flag_mutex != NULL);\
switch_mutex_lock(obj->flag_mutex);\
(obj)->media_flags[flag] = 1;\
switch_mutex_unlock(obj->flag_mutex);
#define sofia_clear_media_flag_locked(obj, flag) switch_mutex_lock(obj->flag_mutex); (obj)->media_flags[flag] = 0; switch_mutex_unlock(obj->flag_mutex);
#define sofia_clear_media_flag(obj, flag) (obj)->media_flags[flag] = 0


#define sofia_test_pflag(obj, flag) ((obj)->pflags[flag] ? 1 : 0)
#define sofia_set_pflag(obj, flag) (obj)->pflags[flag] = 1
#define sofia_set_pflag_locked(obj, flag) assert(obj->flag_mutex != NULL);\
switch_mutex_lock(obj->flag_mutex);\
(obj)->pflags[flag] = 1;\
switch_mutex_unlock(obj->flag_mutex);
#define sofia_clear_pflag_locked(obj, flag) switch_mutex_lock(obj->flag_mutex); (obj)->pflags[flag] = 0; switch_mutex_unlock(obj->flag_mutex);
#define sofia_clear_pflag(obj, flag) (obj)->pflags[flag] = 0



#define sofia_set_flag_locked(obj, flag) assert(obj->flag_mutex != NULL);\
switch_mutex_lock(obj->flag_mutex);\
(obj)->flags[flag] = 1;\
switch_mutex_unlock(obj->flag_mutex);
#define sofia_set_flag(obj, flag) (obj)->flags[flag] = 1
#define sofia_clear_flag(obj, flag) (obj)->flags[flag] = 0
#define sofia_clear_flag_locked(obj, flag) switch_mutex_lock(obj->flag_mutex); (obj)->flags[flag] = 0; switch_mutex_unlock(obj->flag_mutex);
#define sofia_test_flag(obj, flag) ((obj)->flags[flag] ? 1 : 0)

/* Function Prototypes */
/*************************************************************************************************************************************************************/

void sofia_glue_global_standby(switch_bool_t on);

switch_status_t sofia_media_activate_rtp(private_object_t *tech_pvt);

const char *sofia_media_get_codec_string(private_object_t *tech_pvt);

void sofia_glue_attach_private(switch_core_session_t *session, sofia_profile_t *profile, private_object_t *tech_pvt, const char *channame);

switch_status_t sofia_glue_do_invite(switch_core_session_t *session);

uint8_t sofia_media_negotiate_sdp(switch_core_session_t *session, const char *r_sdp, switch_sdp_type_t type);

void sofia_handle_sip_i_refer(nua_t *nua, sofia_profile_t *profile, nua_handle_t *nh, switch_core_session_t *session, sip_t const *sip,
								sofia_dispatch_event_t *de, tagi_t tags[]);

void sofia_handle_sip_i_info(nua_t *nua, sofia_profile_t *profile, nua_handle_t *nh, switch_core_session_t *session, sip_t const *sip,
								sofia_dispatch_event_t *de, tagi_t tags[]);

void sofia_handle_sip_i_invite(switch_core_session_t *session, nua_t *nua, sofia_profile_t *profile, nua_handle_t *nh, sofia_private_t *sofia_private, sip_t const *sip, sofia_dispatch_event_t *de, tagi_t tags[]);


void sofia_reg_handle_sip_i_register(nua_t *nua, sofia_profile_t *profile, nua_handle_t *nh, sofia_private_t **sofia_private, sip_t const *sip,
								sofia_dispatch_event_t *de,
									 tagi_t tags[]);

void sofia_event_callback(nua_event_t event,
						  int status,
						  char const *phrase,
						  nua_t *nua, sofia_profile_t *profile, nua_handle_t *nh, sofia_private_t *sofia_private, sip_t const *sip,
						  tagi_t tags[]);

void *SWITCH_THREAD_FUNC sofia_profile_thread_run(switch_thread_t *thread, void *obj);

void launch_sofia_profile_thread(sofia_profile_t *profile);

switch_status_t sofia_presence_chat_send(switch_event_t *message_event);

/*
 * \brief Sets the "ep_codec_string" channel variable, parsing r_sdp and taing codec_string in consideration
 * \param channel Current channel
 * \param codec_string The profile's codec string or NULL if inexistant
 * \param sdp The parsed SDP content
 */
void sofia_media_set_r_sdp_codec_string(switch_core_session_t *session, const char *codec_string, sdp_session_t *sdp);
switch_status_t sofia_media_tech_media(private_object_t *tech_pvt, const char *r_sdp);
char *sofia_reg_find_reg_url(sofia_profile_t *profile, const char *user, const char *host, char *val, switch_size_t len);
void event_handler(switch_event_t *event);
void sofia_presence_event_handler(switch_event_t *event);


void sofia_presence_cancel(void);
switch_status_t config_sofia(sofia_config_t reload, char *profile_name);
void sofia_reg_auth_challenge(sofia_profile_t *profile, nua_handle_t *nh, sofia_dispatch_event_t *de,
							  sofia_regtype_t regtype, const char *realm, int stale, long exptime);
auth_res_t sofia_reg_parse_auth(sofia_profile_t *profile, sip_authorization_t const *authorization,
								sip_t const *sip,
								sofia_dispatch_event_t *de, const char *regstr, char *np, size_t nplen, char *ip, int network_port, switch_event_t **v_event,
								long exptime, sofia_regtype_t regtype, const char *to_user, switch_event_t **auth_params, long *reg_count, switch_xml_t *user_xml);


void sofia_reg_handle_sip_r_challenge(int status,
									  char const *phrase,
									  nua_t *nua, sofia_profile_t *profile,
									  nua_handle_t *nh, sofia_private_t *sofia_private,
									  switch_core_session_t *session, sofia_gateway_t *gateway, sip_t const *sip,
								sofia_dispatch_event_t *de, tagi_t tags[]);
void sofia_reg_handle_sip_r_register(int status,
									 char const *phrase,
									 nua_t *nua, sofia_profile_t *profile, nua_handle_t *nh, sofia_private_t *sofia_private, sip_t const *sip,
								sofia_dispatch_event_t *de,
									 tagi_t tags[]);
void sofia_handle_sip_i_options(int status, char const *phrase, nua_t *nua, sofia_profile_t *profile, nua_handle_t *nh, sofia_private_t *sofia_private,
								sip_t const *sip,
								sofia_dispatch_event_t *de, tagi_t tags[]);
void sofia_presence_handle_sip_i_publish(nua_t *nua, sofia_profile_t *profile, nua_handle_t *nh, sofia_private_t *sofia_private, sip_t const *sip,
								sofia_dispatch_event_t *de,
										 tagi_t tags[]);
void sofia_presence_handle_sip_i_message(int status, char const *phrase, nua_t *nua, sofia_profile_t *profile, nua_handle_t *nh,
										 switch_core_session_t *session, sofia_private_t *sofia_private, sip_t const *sip,
								sofia_dispatch_event_t *de, tagi_t tags[]);
void sofia_presence_handle_sip_r_subscribe(int status, char const *phrase, nua_t *nua, sofia_profile_t *profile, nua_handle_t *nh,
										   sofia_private_t *sofia_private, sip_t const *sip,
								sofia_dispatch_event_t *de, tagi_t tags[]);
void sofia_presence_handle_sip_i_subscribe(int status, char const *phrase, nua_t *nua, sofia_profile_t *profile, nua_handle_t *nh,
										   sofia_private_t *sofia_private, sip_t const *sip,
								sofia_dispatch_event_t *de, tagi_t tags[]);

void sofia_glue_execute_sql(sofia_profile_t *profile, char **sqlp, switch_bool_t sql_already_dynamic);
void sofia_glue_actually_execute_sql(sofia_profile_t *profile, char *sql, switch_mutex_t *mutex);
void sofia_glue_actually_execute_sql_trans(sofia_profile_t *profile, char *sql, switch_mutex_t *mutex);
void sofia_glue_execute_sql_now(sofia_profile_t *profile, char **sqlp, switch_bool_t sql_already_dynamic);
void sofia_glue_execute_sql_soon(sofia_profile_t *profile, char **sqlp, switch_bool_t sql_already_dynamic);
void sofia_reg_check_expire(sofia_profile_t *profile, time_t now, int reboot);
void sofia_reg_check_gateway(sofia_profile_t *profile, time_t now);
void sofia_sub_check_gateway(sofia_profile_t *profile, time_t now);
void sofia_reg_unregister(sofia_profile_t *profile);


void sofia_glue_pass_sdp(private_object_t *tech_pvt, char *sdp);
switch_call_cause_t sofia_glue_sip_cause_to_freeswitch(int status);
void sofia_glue_do_xfer_invite(switch_core_session_t *session);
uint8_t sofia_reg_handle_register_token(nua_t *nua, sofia_profile_t *profile, nua_handle_t *nh, sip_t const *sip,
								  sofia_dispatch_event_t *de,
								  sofia_regtype_t regtype, char *key,
								  uint32_t keylen, switch_event_t **v_event, const char *is_nat, sofia_private_t **sofia_private_p, switch_xml_t *user_xml, const char *sw_acl_token);
#define sofia_reg_handle_register(_nua_, _profile_, _nh_, _sip_, _de_, _regtype_, _key_, _keylen_, _v_event_, _is_nat_, _sofia_private_p_, _user_xml_) sofia_reg_handle_register_token(_nua_, _profile_, _nh_, _sip_, _de_, _regtype_, _key_, _keylen_, _v_event_, _is_nat_, _sofia_private_p_, _user_xml_, NULL)
extern switch_endpoint_interface_t *sofia_endpoint_interface;
void sofia_presence_set_chat_hash(private_object_t *tech_pvt, sip_t const *sip);
switch_status_t sofia_on_hangup(switch_core_session_t *session);
char *sofia_glue_get_url_from_contact(char *buf, uint8_t to_dup);
char *sofia_glue_get_path_from_contact(char *buf);
void sofia_presence_set_hash_key(char *hash_key, int32_t len, sip_t const *sip);
void sofia_glue_sql_close(sofia_profile_t *profile, time_t prune);
int sofia_glue_init_sql(sofia_profile_t *profile);
char *sofia_overcome_sip_uri_weakness(switch_core_session_t *session, const char *uri, const sofia_transport_t transport, switch_bool_t uri_only,
									  const char *params, const char *invite_tel_params);
switch_bool_t sofia_glue_execute_sql_callback(sofia_profile_t *profile, switch_mutex_t *mutex, char *sql, switch_core_db_callback_func_t callback,
											  void *pdata);
char *sofia_glue_execute_sql2str(sofia_profile_t *profile, switch_mutex_t *mutex, char *sql, char *resbuf, size_t len);
void sofia_glue_del_profile(sofia_profile_t *profile);

switch_status_t sofia_glue_add_profile(char *key, sofia_profile_t *profile);
void sofia_glue_release_profile__(const char *file, const char *func, int line, sofia_profile_t *profile);
#define sofia_glue_release_profile(x) sofia_glue_release_profile__(__FILE__, __SWITCH_FUNC__, __LINE__,  x)

sofia_profile_t *sofia_glue_find_profile__(const char *file, const char *func, int line, const char *key);
#define sofia_glue_find_profile(x) sofia_glue_find_profile__(__FILE__, __SWITCH_FUNC__, __LINE__,  x)

#define sofia_glue_profile_rdlock(x) sofia_glue_profile_rdlock__(__FILE__, __SWITCH_FUNC__, __LINE__, x)
switch_status_t sofia_glue_profile_rdlock__(const char *file, const char *func, int line, sofia_profile_t *profile);

switch_status_t sofia_reg_add_gateway(sofia_profile_t *profile, const char *key, sofia_gateway_t *gateway);
sofia_gateway_t *sofia_reg_find_gateway__(const char *file, const char *func, int line, const char *key);
#define sofia_reg_find_gateway(x) sofia_reg_find_gateway__(__FILE__, __SWITCH_FUNC__, __LINE__,  x)

sofia_gateway_t *sofia_reg_find_gateway_by_realm__(const char *file, const char *func, int line, const char *key);
#define sofia_reg_find_gateway_by_realm(x) sofia_reg_find_gateway_by_realm__(__FILE__, __SWITCH_FUNC__, __LINE__,  x)

sofia_gateway_subscription_t *sofia_find_gateway_subscription(sofia_gateway_t *gateway_ptr, const char *event);

#define sofia_reg_gateway_rdlock(x) sofia_reg_gateway_rdlock__(__FILE__, __SWITCH_FUNC__, __LINE__,  x)
switch_status_t sofia_reg_gateway_rdlock__(const char *file, const char *func, int line, sofia_gateway_t *gateway);

void sofia_reg_release_gateway__(const char *file, const char *func, int line, sofia_gateway_t *gateway);
#define sofia_reg_release_gateway(x) sofia_reg_release_gateway__(__FILE__, __SWITCH_FUNC__, __LINE__, x);

#define sofia_use_soa(_t) sofia_test_flag(_t, TFLAG_ENABLE_SOA)

#define check_decode(_var, _session) do {								\
		assert(_session);												\
		if (!zstr(_var)) {								\
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
		if (_session) break;											\
	} while(!_session)


/*
 * Transport handling helper functions
 */
sofia_transport_t sofia_glue_via2transport(const sip_via_t * via);
sofia_transport_t sofia_glue_url2transport(const url_t *url);
sofia_transport_t sofia_glue_str2transport(const char *str);
enum tport_tls_verify_policy sofia_glue_str2tls_verify_policy(const char * str);

const char *sofia_glue_transport2str(const sofia_transport_t tp);
char *sofia_glue_find_parameter(const char *str, const char *param);
char *sofia_glue_find_parameter_value(switch_core_session_t *session, const char *str, const char *param);
char *sofia_glue_create_via(switch_core_session_t *session, const char *ip, switch_port_t port, sofia_transport_t transport);
char *sofia_glue_create_external_via(switch_core_session_t *session, sofia_profile_t *profile, sofia_transport_t transport);
char *sofia_glue_strip_uri(const char *str);

int sofia_glue_transport_has_tls(const sofia_transport_t tp);
const char *sofia_glue_get_unknown_header(sip_t const *sip, const char *name);
switch_status_t sofia_media_build_crypto(private_object_t *tech_pvt, int index, switch_rtp_crypto_key_type_t type, switch_rtp_crypto_direction_t direction);

void sofia_presence_event_thread_start(void);
void sofia_reg_expire_call_id(sofia_profile_t *profile, const char *call_id, int reboot);
void sofia_reg_check_call_id(sofia_profile_t *profile, const char *call_id);
void sofia_reg_check_sync(sofia_profile_t *profile);


char *sofia_glue_get_register_host(const char *uri);
const char *sofia_glue_strip_proto(const char *uri);
void sofia_glue_del_gateway(sofia_gateway_t *gp);
void sofia_glue_gateway_list(sofia_profile_t *profile, switch_stream_handle_t *stream, int up);
void sofia_glue_del_every_gateway(sofia_profile_t *profile);
void sofia_reg_send_reboot(sofia_profile_t *profile, const char *callid, const char *user, const char *host, const char *contact, const char *user_agent,
						   const char *network_ip);
void sofia_glue_restart_all_profiles(void);
const char *sofia_state_string(int state);
void sofia_wait_for_reply(struct private_object *tech_pvt, nua_event_t event, uint32_t timeout);

/*
 * Logging control functions
 */

/*!
 * \brief Changes the loglevel of a sofia component
 * \param name the sofia component on which to change the loglevel, or "all" to change them all
 * \note Valid components are "all", "default" (sofia's default logger), "tport", "iptsec", "nea", "nta", "nth_client", "nth_server", "nua", "soa", "sresolv", "stun"
 * \return SWITCH_STATUS_SUCCESS or SWITCH_STATUS_FALSE if the component isnt valid, or the level is out of range
 */
switch_status_t sofia_set_loglevel(const char *name, int level);

/*!
 * \brief Gets the loglevel of a sofia component
 * \param name the sofia component on which to change the loglevel
 * \note Valid components are "default" (sofia's default logger), "tport", "iptsec", "nea", "nta", "nth_client", "nth_server", "nua", "soa", "sresolv", "stun"
 * \return the component's loglevel, or -1 if the component isn't valid
 */
int sofia_get_loglevel(const char *name);
switch_status_t list_profiles_full(const char *line, const char *cursor, switch_console_callback_match_t **matches, switch_bool_t show_aliases);
switch_status_t list_profiles(const char *line, const char *cursor, switch_console_callback_match_t **matches);

sofia_cid_type_t sofia_cid_name2type(const char *name);
void sofia_glue_get_addr(msg_t *msg, char *buf, size_t buflen, int *port);
sofia_destination_t *sofia_glue_get_destination(char *data);
void sofia_glue_free_destination(sofia_destination_t *dst);
switch_status_t sofia_glue_send_notify(sofia_profile_t *profile, const char *user, const char *host, const char *event, const char *contenttype,
									   const char *body, const char *o_contact, const char *network_ip, const char *call_id);
char *sofia_glue_get_extra_headers(switch_channel_t *channel, const char *prefix);
void sofia_glue_set_extra_headers(switch_core_session_t *session, sip_t const *sip, const char *prefix);
char *sofia_glue_get_extra_headers_from_event(switch_event_t *event, const char *prefix);
void sofia_update_callee_id(switch_core_session_t *session, sofia_profile_t *profile, sip_t const *sip, switch_bool_t send);
void sofia_send_callee_id(switch_core_session_t *session, const char *name, const char *number);
int sofia_sla_supported(sip_t const *sip);
int sofia_glue_recover(switch_bool_t flush);
int sofia_glue_profile_recover(sofia_profile_t *profile, switch_bool_t flush);
void sofia_profile_destroy(sofia_profile_t *profile);
switch_status_t sip_dig_function(_In_opt_z_ const char *cmd, _In_opt_ switch_core_session_t *session, _In_ switch_stream_handle_t *stream);
const char *sofia_gateway_status_name(sofia_gateway_status_t status);
void sofia_reg_fire_custom_gateway_state_event(sofia_gateway_t *gateway, int status, const char *phrase);
uint32_t sofia_reg_reg_count(sofia_profile_t *profile, const char *user, const char *host);
char *sofia_media_get_multipart(switch_core_session_t *session, const char *prefix, const char *sdp, char **mp_type);
int sofia_glue_tech_simplify(private_object_t *tech_pvt);
switch_console_callback_match_t *sofia_reg_find_reg_url_multi(sofia_profile_t *profile, const char *user, const char *host);
switch_console_callback_match_t *sofia_reg_find_reg_url_with_positive_expires_multi(sofia_profile_t *profile, const char *user, const char *host, time_t reg_time, const char *contact_str, long exptime);
switch_bool_t sofia_glue_profile_exists(const char *key);
void sofia_glue_global_siptrace(switch_bool_t on);
void sofia_glue_global_capture(switch_bool_t on);
void sofia_glue_global_watchdog(switch_bool_t on);
uint32_t sofia_presence_get_cseq(sofia_profile_t *profile);

void sofia_glue_build_vid_refresh_message(switch_core_session_t *session, const char *pl);
char *sofia_glue_gen_contact_str(sofia_profile_t *profile, sip_t const *sip, nua_handle_t *nh, sofia_dispatch_event_t *de, sofia_nat_parse_t *np);
void sofia_glue_pause_jitterbuffer(switch_core_session_t *session, switch_bool_t on);
void sofia_process_dispatch_event(sofia_dispatch_event_t **dep);
void sofia_process_dispatch_event_in_thread(sofia_dispatch_event_t **dep);
char *sofia_glue_get_host(const char *str, switch_memory_pool_t *pool);
void sofia_presence_check_subscriptions(sofia_profile_t *profile, time_t now);
void sofia_msg_thread_start(int idx);
void crtp_init(switch_loadable_module_interface_t *module_interface);
int sofia_recover_callback(switch_core_session_t *session);
void sofia_glue_set_name(private_object_t *tech_pvt, const char *channame);
private_object_t *sofia_glue_new_pvt(switch_core_session_t *session);
switch_status_t sofia_init(void);
void sofia_glue_fire_events(sofia_profile_t *profile);
void sofia_event_fire(sofia_profile_t *profile, switch_event_t **event);
void sofia_queue_message(sofia_dispatch_event_t *de);
int sofia_glue_check_nat(sofia_profile_t *profile, const char *network_ip);

switch_status_t sofia_glue_ext_address_lookup(sofia_profile_t *profile, char **ip, switch_port_t *port,
											  const char *sourceip, switch_memory_pool_t *pool);
void sofia_reg_check_socket(sofia_profile_t *profile, const char *call_id, const char *network_addr, const char *network_ip);
void sofia_reg_close_handles(sofia_profile_t *profile);

void write_csta_xml_chunk(switch_event_t *event, switch_stream_handle_t stream, const char *csta_event, char *fwd_type);
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
