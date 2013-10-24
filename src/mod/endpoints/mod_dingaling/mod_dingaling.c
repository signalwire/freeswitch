/* 
 * FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
 * Copyright (C) 2005-2012, Anthony Minessale II <anthm@freeswitch.org>
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
 *
 *
 * mod_dingaling.c -- Jingle Endpoint Module
 *
 */
#include <switch.h>
#include <switch_stun.h>
#include <libdingaling.h>

#define MDL_RTCP_DUR 5000
#define DL_CAND_WAIT 10000000
#define DL_CAND_INITIAL_WAIT 2000000
//#define DL_CAND_WAIT 2000000
//#define DL_CAND_INITIAL_WAIT 5000000

#define DL_EVENT_LOGIN_SUCCESS "dingaling::login_success"
#define DL_EVENT_LOGIN_FAILURE "dingaling::login_failure"
#define DL_EVENT_CONNECTED "dingaling::connected"
#define MDL_CHAT_PROTO "jingle"
#define MDL_CHAT_FROM_GUESS "auto_from"

SWITCH_MODULE_LOAD_FUNCTION(mod_dingaling_load);
SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_dingaling_shutdown);
SWITCH_MODULE_DEFINITION(mod_dingaling, mod_dingaling_load, mod_dingaling_shutdown, NULL);

static switch_memory_pool_t *module_pool = NULL;
switch_endpoint_interface_t *dingaling_endpoint_interface;

static char sub_sql[] =
	"CREATE TABLE jabber_subscriptions (\n"
	"   sub_from      VARCHAR(255),\n" "   sub_to        VARCHAR(255),\n" "   show_pres     VARCHAR(255),\n" "   status        VARCHAR(255)\n" ");\n";


typedef enum {
	TFLAG_IO = (1 << 0),
	TFLAG_INBOUND = (1 << 1),
	TFLAG_OUTBOUND = (1 << 2),
	TFLAG_READING = (1 << 3),
	TFLAG_WRITING = (1 << 4),
	TFLAG_BYE = (1 << 5),
	TFLAG_VOICE = (1 << 6),
	TFLAG_RTP_READY = (1 << 7),
	TFLAG_CODEC_READY = (1 << 8),
	TFLAG_TRANSPORT = (1 << 9),
	TFLAG_ANSWER = (1 << 10),
	TFLAG_VAD_NONE = (1 << 11),
	TFLAG_VAD_IN = (1 << 12),
	TFLAG_VAD_OUT = (1 << 13),
	TFLAG_VAD = (1 << 14),
	TFLAG_DO_CAND = (1 << 15),
	TFLAG_DO_DESC = (1 << 16),
	TFLAG_LANADDR = (1 << 17),
	TFLAG_AUTO = (1 << 18),
	TFLAG_DTMF = (1 << 19),
	TFLAG_TIMER = (1 << 20),
	TFLAG_TERM = (1 << 21),
	TFLAG_TRANSPORT_ACCEPT = (1 << 22),
	TFLAG_READY = (1 << 23),
	TFLAG_NAT_MAP = (1 << 24),
	TFLAG_SECURE = (1 << 25),
	TFLAG_VIDEO_RTP_READY = (1 << 7)
} TFLAGS;

typedef enum {
	GFLAG_MY_CODEC_PREFS = (1 << 0)
} GFLAGS;

#define MAX_ACL 100

static struct {
	int debug;
	char *dialplan;
	char *codec_string;
	char *codec_order[SWITCH_MAX_CODECS];
	int codec_order_last;
	char *codec_rates_string;
	char *codec_rates[SWITCH_MAX_CODECS];
	int codec_rates_last;
	unsigned int flags;
	unsigned int init;
	switch_hash_t *profile_hash;
	int running;
	int handles;
	char guess_ip[80];
	switch_event_node_t *in_node;
	switch_event_node_t *probe_node;
	switch_event_node_t *out_node;
	switch_event_node_t *roster_node;
	int auto_nat;
} globals;

struct mdl_profile {
	char *name;
	char *login;
	char *password;
	char *message;
#ifdef AUTO_REPLY
	char *auto_reply;
#endif
	char *dialplan;
	char *ip;
	char *extip;
	char *lanaddr;
	char *server;
	char *exten;
	char *context;
	char *timer_name;
	char *dbname;
	char *avatar;
	char *odbc_dsn;
	switch_bool_t purge;
	switch_thread_rwlock_t *rwlock;
	switch_mutex_t *mutex;
	ldl_handle_t *handle;
	uint32_t flags;
	uint32_t user_flags;
	char *acl[MAX_ACL];
	uint32_t acl_count;
	char *local_network;
};
typedef struct mdl_profile mdl_profile_t;

/*! \brief The required components to setup a jingle transport */
typedef struct mdl_transport {
	char *remote_ip;
	switch_port_t remote_port;

	switch_port_t local_port;	/*!< The real local port */
    switch_port_t adv_local_port;
	unsigned int ssrc;

	char local_user[17];
    char local_pass[17];    
	char *remote_user;
	char *remote_pass;
	int ptime;
	int payload_count;
	int restart_rtp;

	switch_codec_t read_codec;
	switch_codec_t write_codec;
	
	switch_frame_t read_frame;
	
	uint32_t codec_rate;
	char *codec_name;
	
	switch_payload_t codec_num;
	switch_payload_t r_codec_num;
	
	char *stun_ip;
	uint16_t stun_port;
	
	switch_rtp_t *rtp_session;
	ldl_transport_type_t type;

	int total;
	int accepted;

	int ready;

	int codec_index;

	int vid_width;
	int vid_height;
	int vid_rate;

	switch_byte_t has_crypto;
	int crypto_tag;
	unsigned char local_raw_key[SWITCH_RTP_MAX_CRYPTO_LEN];
	unsigned char remote_raw_key[SWITCH_RTP_MAX_CRYPTO_LEN];
	switch_rtp_crypto_key_type_t crypto_send_type;
	switch_rtp_crypto_key_type_t crypto_recv_type;
	switch_rtp_crypto_key_type_t crypto_type;

	char *local_crypto_key;
	char *remote_crypto_key;
	
	ldl_crypto_data_t *local_crypto_data;
	
} mdl_transport_t;


struct private_object {
	unsigned int flags;
	mdl_profile_t *profile;
	switch_core_session_t *session;
	switch_channel_t *channel;

	switch_caller_profile_t *caller_profile;
	unsigned short samprate;
	switch_mutex_t *mutex;
	const switch_codec_implementation_t *codecs[SWITCH_MAX_CODECS];
	unsigned int num_codecs;

	mdl_transport_t transports[LDL_TPORT_MAX+1];	

	ldl_session_t *dlsession;

	char *us;
	char *them;
	unsigned int cand_id;
	unsigned int desc_id;
	unsigned int dc;

	uint32_t timestamp_send;
	int32_t timestamp_recv;
	uint32_t last_read;

	switch_time_t next_desc;
	switch_time_t next_cand;

	char *recip;
	char *dnis;
	switch_mutex_t *flag_mutex;

	int read_count;
	switch_time_t audio_ready;

};

struct rfc2833_digit {
	char digit;
	int duration;
};


SWITCH_DECLARE_GLOBAL_STRING_FUNC(set_global_dialplan, globals.dialplan);
SWITCH_DECLARE_GLOBAL_STRING_FUNC(set_global_codec_string, globals.codec_string);
SWITCH_DECLARE_GLOBAL_STRING_FUNC(set_global_codec_rates_string, globals.codec_rates_string);

SWITCH_STANDARD_API(dl_login);
SWITCH_STANDARD_API(dl_logout);
SWITCH_STANDARD_API(dl_pres);
SWITCH_STANDARD_API(dl_debug);
SWITCH_STANDARD_API(dingaling);
static switch_status_t channel_on_init(switch_core_session_t *session);
static switch_status_t channel_on_hangup(switch_core_session_t *session);
static switch_status_t channel_on_destroy(switch_core_session_t *session);
static switch_status_t channel_on_routing(switch_core_session_t *session);
static switch_status_t channel_on_exchange_media(switch_core_session_t *session);
static switch_status_t channel_on_soft_execute(switch_core_session_t *session);
static switch_call_cause_t channel_outgoing_channel(switch_core_session_t *session, switch_event_t *var_event,
													switch_caller_profile_t *outbound_profile,
													switch_core_session_t **new_session, switch_memory_pool_t **pool, switch_originate_flag_t flags,
													switch_call_cause_t *cancel_cause);
static switch_status_t channel_read_frame(switch_core_session_t *session, switch_frame_t **frame, switch_io_flag_t flags, int stream_id);
static switch_status_t channel_write_frame(switch_core_session_t *session, switch_frame_t *frame, switch_io_flag_t flags, int stream_id);
static switch_status_t channel_kill_channel(switch_core_session_t *session, int sig);

static ldl_status handle_signalling(ldl_handle_t *handle, ldl_session_t *dlsession, ldl_signal_t dl_signal,
									char *to, char *from, char *subject, char *msg);
static ldl_status handle_response(ldl_handle_t *handle, char *id);
static switch_status_t load_config(void);
static int sin_callback(void *pArg, int argc, char **argv, char **columnNames);

static switch_status_t soft_reload(void);

#define is_special(s) (s && (strstr(s, "ext+") || strstr(s, "user+")))

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

	if (!strcasecmp(in, "busy")) {
		r = "dnd";
	}

	if (!strcasecmp(in, "unavailable")) {
		r = "dnd";
	}

	if (!strcasecmp(in, "idle")) {
		r = "away";
	}

	if (ext && !strcasecmp(ext, "idle")) {
		r = "away";
	} else if (ext && !strcasecmp(ext, "away")) {
		r = "away";
	}

	return r;
}


static switch_cache_db_handle_t *mdl_get_db_handle(mdl_profile_t *profile)
{
	switch_cache_db_handle_t *dbh = NULL;
	char *dsn;
	
	if (!zstr(profile->odbc_dsn)) {
		dsn = profile->odbc_dsn;
	} else {
		dsn = profile->dbname;
	}

	if (switch_cache_db_get_db_handle_dsn(&dbh, dsn) != SWITCH_STATUS_SUCCESS) {
		dbh = NULL;
	}
	
	return dbh;

}


static switch_status_t mdl_execute_sql(mdl_profile_t *profile, char *sql, switch_mutex_t *mutex)
{
	switch_cache_db_handle_t *dbh = NULL;
	switch_status_t status = SWITCH_STATUS_FALSE;

	if (mutex) {
		switch_mutex_lock(mutex);
	}

	if (!(dbh = mdl_get_db_handle(profile))) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Error Opening DB\n");
		goto end;
	}

	status = switch_cache_db_execute_sql(dbh, sql, NULL);

  end:

	switch_cache_db_release_db_handle(&dbh);

	if (mutex) {
		switch_mutex_unlock(mutex);
	}

	return status;
}

char *mdl_execute_sql2str(mdl_profile_t *profile, switch_mutex_t *mutex, char *sql, char *resbuf, size_t len)
{
	switch_cache_db_handle_t *dbh = NULL;

	char *ret = NULL;

	if (mutex) {
		switch_mutex_lock(mutex);
	}

	if (!(dbh = mdl_get_db_handle(profile))) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Error Opening DB\n");
		goto end;
	}

	ret = switch_cache_db_execute_sql2str(dbh, sql, resbuf, len, NULL);

end:

	switch_cache_db_release_db_handle(&dbh);

	if (mutex) {
		switch_mutex_unlock(mutex);
	}

	return ret;

}


static switch_bool_t mdl_execute_sql_callback(mdl_profile_t *profile, switch_mutex_t *mutex, char *sql, switch_core_db_callback_func_t callback,
											 void *pdata)
{
	switch_bool_t ret = SWITCH_FALSE;
	char *errmsg = NULL;
	switch_cache_db_handle_t *dbh = NULL;

	if (mutex) {
		switch_mutex_lock(mutex);
	}

	if (!(dbh = mdl_get_db_handle(profile))) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Error Opening DB\n");
		goto end;
	}

	switch_cache_db_execute_sql_callback(dbh, sql, callback, pdata, &errmsg);

	if (errmsg) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "SQL ERR: [%s] %s\n", sql, errmsg);
		free(errmsg);
	}

  end:

	switch_cache_db_release_db_handle(&dbh);

	if (mutex) {
		switch_mutex_unlock(mutex);
	}

	return ret;
}




static int sub_callback(void *pArg, int argc, char **argv, char **columnNames)
{
	mdl_profile_t *profile = (mdl_profile_t *) pArg;

	char *sub_from = argv[0];
	char *sub_to = argv[1];
	char *type = argv[2];
	char *rpid = argv[3];
	char *status = argv[4];
	//char *proto = argv[5];

	if (zstr(type)) {
		type = NULL;
	} else if (!strcasecmp(type, "unavailable")) {
		status = NULL;
	}
	rpid = translate_rpid(rpid, status);

	//ldl_handle_send_presence(profile->handle, sub_to, sub_from, "probe", rpid, status);
	ldl_handle_send_presence(profile->handle, sub_to, sub_from, type, rpid, status, profile->avatar);


	return 0;
}

static int rost_callback(void *pArg, int argc, char **argv, char **columnNames)
{
	mdl_profile_t *profile = (mdl_profile_t *) pArg;

	char *sub_from = argv[0];
	char *sub_to = argv[1];
	char *show = argv[2];
	char *status = argv[3];

	if (!strcasecmp(status, "n/a")) {
		if (!strcasecmp(show, "dnd")) {
			status = "Busy";
		} else if (!strcasecmp(show, "away")) {
			status = "Idle";
		}
	}

	ldl_handle_send_presence(profile->handle, sub_to, sub_from, NULL, show, status, profile->avatar);

	return 0;
}

static void pres_event_handler(switch_event_t *event)
{
	mdl_profile_t *profile = NULL;
	switch_hash_index_t *hi;
	void *val;
	char *proto = switch_event_get_header(event, "proto");
	char *from = switch_event_get_header(event, "from");
	char *status = switch_event_get_header(event, "status");
	char *rpid = switch_event_get_header(event, "rpid");
	char *type = switch_event_get_header(event, "event_subtype");
	char *sql;
	char pstr[128] = "";

	if (globals.running != 1) {
		return;
	}

	if (!proto) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Error Missing 'proto' header\n");
		return;
	}

	if (!from) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Error Missing 'from' header\n");
		return;
	}

	if (status && !strcasecmp(status, "n/a")) {
		status = NULL;
	}

	switch (event->event_id) {
	case SWITCH_EVENT_PRESENCE_PROBE:
		if (proto) {
			char *subsql;
			char *to = switch_event_get_header(event, "to");
			char *f_host = NULL;
			if (to) {
				if ((f_host = strchr(to, '@'))) {
					f_host++;
				}
			}

			if (f_host && (profile = switch_core_hash_find(globals.profile_hash, f_host))) {
				if (to && (subsql = switch_mprintf("select * from jabber_subscriptions where sub_to='%q' and sub_from='%q'", to, from))) {
					mdl_execute_sql_callback(profile, profile->mutex, subsql, sin_callback, profile);
					switch_safe_free(subsql);
				}
			}
		}
		return;
	case SWITCH_EVENT_PRESENCE_IN:
		if (!status) {
			status = "Available";
		}
		break;
	case SWITCH_EVENT_PRESENCE_OUT:
		type = "unavailable";
		break;
	default:
		break;
	}


	if (!type) {
		type = "";
	}
	if (!rpid) {
		rpid = "";
	}
	if (!status) {
		status = "Away";
	}

	if (proto) {
		switch_snprintf(pstr, sizeof(pstr), "%s+", proto);
	}

	sql =
		switch_mprintf("select sub_from, sub_to,'%q','%q','%q','%q' from jabber_subscriptions where sub_to = '%q%q'", type, rpid, status, proto, pstr,
					   from);

	for (hi = switch_hash_first(NULL, globals.profile_hash); hi; hi = switch_hash_next(hi)) {
		switch_hash_this(hi, NULL, NULL, &val);
		profile = (mdl_profile_t *) val;

		if (!(profile->user_flags & LDL_FLAG_COMPONENT)) {
			continue;
		}


		if (sql) {
			switch_bool_t worked = mdl_execute_sql_callback(profile, profile->mutex, sql, sub_callback, profile);

			if (!worked) {
				continue;
			}
		}


	}

	switch_safe_free(sql);
}

static switch_status_t chat_send(switch_event_t *message_event)
{
	char *user, *host, *f_user = NULL, *ffrom = NULL, *f_host = NULL, *f_resource = NULL;
	mdl_profile_t *profile = NULL;
	const char *proto;
	const char *from; 
	const char *from_full; 
	const char *to_full; 
	const char *to;
	const char *body;
	const char *hint;
	const char *profile_name;

	proto = switch_event_get_header(message_event, "proto");
	from = switch_event_get_header(message_event, "from");
	from_full = switch_event_get_header(message_event, "from_full");
	to_full = switch_event_get_header(message_event, "to_full");
	to = switch_event_get_header(message_event, "to");
	body = switch_event_get_body(message_event);
	hint = switch_event_get_header(message_event, "hint");
	profile_name = switch_event_get_header(message_event, "ldl_profile");

	switch_assert(proto != NULL);
	
	if (from && (f_user = strdup(from))) {
		if ((f_host = strchr(f_user, '@'))) {
			*f_host++ = '\0';
			if ((f_resource = strchr(f_host, '/'))) {
				*f_resource++ = '\0';
			}
		}
	}

	if ((profile_name && (profile = switch_core_hash_find(globals.profile_hash, profile_name)))) {
		from = from_full;
		to = to_full;

		ldl_handle_send_msg(profile->handle, (char *) from, (char *) to, NULL, switch_str_nil(body));
	} else if (to && (user = strdup(to))) {
		if ((host = strchr(user, '@'))) {
			*host++ = '\0';
		}

		if (f_host && ((profile_name && (profile = switch_core_hash_find(globals.profile_hash, profile_name)))
					   || (profile = switch_core_hash_find(globals.profile_hash, f_host)))) {

			if (!strcmp(proto, MDL_CHAT_PROTO)) {
				from = hint;
			} else {
				char *p;
				
				if (!(profile->user_flags & LDL_FLAG_COMPONENT)) {
					from = ffrom = strdup(profile->login);
				} else {
					from = ffrom = switch_mprintf("%s+%s", proto, from);
				}
				
				if ((p = strchr(from, '/'))) {
					*p = '\0';
				}
			}
			if (!(profile->user_flags & LDL_FLAG_COMPONENT) && !strcmp(f_user, MDL_CHAT_FROM_GUESS)) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Using auto_from jid address for profile %s\n", profile->name);
				ldl_handle_send_msg(profile->handle, NULL, (char *) to, NULL, switch_str_nil(body));
			} else {
				ldl_handle_send_msg(profile->handle, (char *) from, (char *) to, NULL, switch_str_nil(body));
			}
			switch_safe_free(ffrom);
		} else {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Invalid Profile %s\n", f_host ? f_host : "NULL");
			return SWITCH_STATUS_FALSE;
		}

		switch_safe_free(user);
		switch_safe_free(f_user);
	}

	return SWITCH_STATUS_SUCCESS;
}


static void roster_event_handler(switch_event_t *event)
{
	char *status = switch_event_get_header(event, "status");
	char *from = switch_event_get_header(event, "from");
	char *event_type = switch_event_get_header(event, "event_type");
	mdl_profile_t *profile = NULL;
	switch_hash_index_t *hi;
	void *val;
	char *sql;

	if (globals.running != 1) {
		return;
	}

	if (status && !strcasecmp(status, "n/a")) {
		status = NULL;
	}

	if (zstr(event_type)) {
		event_type = "presence";
	}

	if (from) {
		sql = switch_mprintf("select *,'%q' from jabber_subscriptions where sub_from='%q'", status ? status : "", from);
	} else {
		sql = switch_mprintf("select *,'%q' from jabber_subscriptions", status ? status : "");
	}

	for (hi = switch_hash_first(NULL, globals.profile_hash); hi; hi = switch_hash_next(hi)) {
		switch_hash_this(hi, NULL, NULL, &val);
		profile = (mdl_profile_t *) val;

		if (!(profile->user_flags & LDL_FLAG_COMPONENT)) {
			continue;
		}


		if (sql) {
			switch_bool_t worked = mdl_execute_sql_callback(profile, profile->mutex, sql, rost_callback, profile);
			if (!worked) {
				continue;
			}
		}

	}

	switch_safe_free(sql);

}

static void ipchanged_event_handler(switch_event_t *event)
{
	const char *cond = switch_event_get_header(event, "condition");

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "EVENT_TRAP: IP change detected\n");

	if (cond && !strcmp(cond, "network-external-address-change")) {
		const char *old_ip4 = switch_event_get_header_nil(event, "network-external-address-previous-v4");
		const char *new_ip4 = switch_event_get_header_nil(event, "network-external-address-change-v4");
		switch_hash_index_t *hi;
		void *val;
		char *tmp;
		mdl_profile_t *profile;
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "IP change detected [%s]->[%s]\n", old_ip4, new_ip4);
		if (globals.profile_hash) {
			for (hi = switch_hash_first(NULL, globals.profile_hash); hi; hi = switch_hash_next(hi)) {
				switch_hash_this(hi, NULL, NULL, &val);
				profile = (mdl_profile_t *) val;
				if (old_ip4 && profile->extip && !strcmp(profile->extip, old_ip4)) {
					tmp = profile->extip;
					profile->extip = strdup(new_ip4);
					switch_safe_free(tmp);
				}
			}
		}
	}
}

static int so_callback(void *pArg, int argc, char **argv, char **columnNames)
{
	mdl_profile_t *profile = (mdl_profile_t *) pArg;

	char *sub_from = argv[0];
	char *sub_to = argv[1];


	ldl_handle_send_presence(profile->handle, sub_to, sub_from, "unavailable", "dnd", "Bub-Bye", profile->avatar);

	return 0;
}


static int sin_callback(void *pArg, int argc, char **argv, char **columnNames)
{
	mdl_profile_t *profile = (mdl_profile_t *) pArg;
	switch_event_t *event;

	//char *sub_from = argv[0];
	char *sub_to = argv[1];

	if (is_special(sub_to)) {
		if (switch_event_create(&event, SWITCH_EVENT_PRESENCE_IN) == SWITCH_STATUS_SUCCESS) {
			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "proto", MDL_CHAT_PROTO);
			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "login", profile->login);
			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "from", sub_to);
			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "rpid", "available");
			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "status", "Online");
			switch_event_fire(&event);
		}
	}

	return 0;
}

static void sign_off(void)
{
	mdl_profile_t *profile = NULL;
	switch_hash_index_t *hi;
	void *val;
	char *sql;



	sql = switch_mprintf("select * from jabber_subscriptions");


	for (hi = switch_hash_first(NULL, globals.profile_hash); hi; hi = switch_hash_next(hi)) {
		switch_hash_this(hi, NULL, NULL, &val);
		profile = (mdl_profile_t *) val;

		if (!(profile->user_flags & LDL_FLAG_COMPONENT)) {
			continue;
		}


		if (sql) {
			switch_bool_t worked = mdl_execute_sql_callback(profile, profile->mutex, sql, so_callback, profile);
			if (!worked) {
				continue;
			}
		}

	}

	switch_yield(1000000);
	switch_safe_free(sql);

}

static void sign_on(mdl_profile_t *profile)
{
	char *sql;


	if ((sql = switch_mprintf("select * from jabber_subscriptions where sub_to like 'ext+%%' or sub_to like 'user+%%' or sub_to like 'conf+%%'"))) {
		mdl_execute_sql_callback(profile, profile->mutex, sql, sin_callback, profile);
		switch_safe_free(sql);
	}
}

static void terminate_session(switch_core_session_t **session, int line, switch_call_cause_t cause)
{
	if (*session) {
		switch_channel_t *channel = switch_core_session_get_channel(*session);
		switch_channel_state_t state = switch_channel_get_state(channel);
		struct private_object *tech_pvt = NULL;

		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(*session), SWITCH_LOG_DEBUG, "Terminate called from line %d state=%s\n", line,
						  switch_channel_state_name(state));

		tech_pvt = switch_core_session_get_private(*session);


		if (tech_pvt && tech_pvt->profile && tech_pvt->profile->ip && tech_pvt->transports[LDL_TPORT_RTP].local_port) {
			switch_rtp_release_port(tech_pvt->profile->ip, tech_pvt->transports[LDL_TPORT_RTP].local_port);
		}

		if (tech_pvt && tech_pvt->profile && tech_pvt->profile->ip && tech_pvt->transports[LDL_TPORT_VIDEO_RTP].local_port) {
			switch_rtp_release_port(tech_pvt->profile->ip, tech_pvt->transports[LDL_TPORT_VIDEO_RTP].local_port);
		}

		if (!switch_core_session_running(*session) && (!tech_pvt || !switch_test_flag(tech_pvt, TFLAG_READY))) {
			switch_core_session_destroy(session);
			return;
		}

		if (!tech_pvt || switch_test_flag(tech_pvt, TFLAG_TERM)) {
			/*once is enough */
			return;
		}

		if (state < CS_HANGUP) {
			switch_channel_hangup(channel, cause);
		}

		switch_mutex_lock(tech_pvt->flag_mutex);
		if (!switch_test_flag(tech_pvt, TFLAG_OUTBOUND)) {
			switch_set_flag(tech_pvt, TFLAG_TERM);
		}
		switch_set_flag(tech_pvt, TFLAG_BYE);
		switch_clear_flag(tech_pvt, TFLAG_IO);
		switch_mutex_unlock(tech_pvt->flag_mutex);

		*session = NULL;
	}

}

static void dl_logger(char *file, const char *func, int line, int level, char *fmt, ...)
{
	va_list ap;
	char *data = NULL;
	int ret;

	va_start(ap, fmt);
	if ((ret = switch_vasprintf(&data, fmt, ap)) != -1) {
		if (!strncasecmp(data, "+xml:", 5)) {
			switch_xml_t xml;
			char *form;
			char *ll = data + 5;
			char *xmltxt;

			if (ll) {
				if ((xmltxt = strchr(ll, ':'))) {
					*xmltxt++ = '\0';
					if (strlen(xmltxt) > 2) {
						xml = switch_xml_parse_str(xmltxt, strlen(xmltxt));
						form = switch_xml_toxml(xml, SWITCH_FALSE);
						switch_log_printf(SWITCH_CHANNEL_ID_LOG, file, func, line, NULL, level,
										  "%s:\n-------------------------------------------------------------------------------\n" "%s\n", ll, form);
						switch_xml_free(xml);
						free(data);
					}
				}
			}
		} else {
			switch_log_printf(SWITCH_CHANNEL_ID_LOG, file, func, line, NULL, level, "%s\n", data);
		}
	}
	va_end(ap);
}

static int get_codecs(struct private_object *tech_pvt)
{
	char *codec_string = NULL;
	const char *var;
	char *codec_order[SWITCH_MAX_CODECS];
	int codec_order_last;
	char **codec_order_p = NULL;


	switch_assert(tech_pvt != NULL);
	switch_assert(tech_pvt->session != NULL);

	if (!tech_pvt->num_codecs) {

		if ((var = switch_channel_get_variable(tech_pvt->channel, "absolute_codec_string"))) {
			codec_string = (char *)var;
			codec_order_last = switch_separate_string(codec_string, ',', codec_order, SWITCH_MAX_CODECS);
			codec_order_p = codec_order;
		} else {
			codec_string = globals.codec_string;
			codec_order_last = globals.codec_order_last;
			codec_order_p = globals.codec_order;
		}
		
		if (codec_string) {
			if ((tech_pvt->num_codecs = switch_loadable_module_get_codecs_sorted(tech_pvt->codecs,
																				 SWITCH_MAX_CODECS, codec_order_p, codec_order_last)) <= 0) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "NO codecs?\n");
				return 0;
			}
			
		} else if (((tech_pvt->num_codecs = switch_loadable_module_get_codecs(tech_pvt->codecs, SWITCH_MAX_CODECS))) <= 0) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "NO codecs?\n");
			return 0;
		}
	}

	return tech_pvt->num_codecs;
}



static void *SWITCH_THREAD_FUNC handle_thread_run(switch_thread_t *thread, void *obj)
{
	ldl_handle_t *handle = obj;
	mdl_profile_t *profile = NULL;



	profile = ldl_handle_get_private(handle);
	globals.handles++;
	switch_set_flag(profile, TFLAG_IO);
	ldl_handle_run(handle);
	switch_clear_flag(profile, TFLAG_IO);
	globals.handles--;
	ldl_handle_destroy(&profile->handle);
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Handle %s [%s] Destroyed\n", profile->name, profile->login);

	return NULL;
}

static void handle_thread_launch(ldl_handle_t *handle)
{
	switch_thread_t *thread;
	switch_threadattr_t *thd_attr = NULL;

	switch_threadattr_create(&thd_attr, module_pool);
	switch_threadattr_detach_set(thd_attr, 1);
	switch_threadattr_stacksize_set(thd_attr, SWITCH_THREAD_STACKSIZE);
	switch_thread_create(&thread, thd_attr, handle_thread_run, handle, module_pool);

}


switch_status_t mdl_build_crypto(struct private_object *tech_pvt, ldl_transport_type_t ttype, 
									int index, switch_rtp_crypto_key_type_t type, switch_rtp_crypto_direction_t direction)
{
	unsigned char b64_key[512] = "";
	const char *type_str;
	unsigned char *key;
	char *p;


	if (!switch_test_flag(tech_pvt, TFLAG_SECURE)) {
		return SWITCH_STATUS_SUCCESS;
	}


	if (type == AES_CM_128_HMAC_SHA1_80) {
		type_str = SWITCH_RTP_CRYPTO_KEY_80;
	} else {
		type_str = SWITCH_RTP_CRYPTO_KEY_32;
	}

	if (direction == SWITCH_RTP_CRYPTO_SEND) {
		key = tech_pvt->transports[ttype].local_raw_key;
	} else {
		key = tech_pvt->transports[ttype].remote_raw_key;

	}

	switch_rtp_get_random(key, SWITCH_RTP_KEY_LEN);
	switch_b64_encode(key, SWITCH_RTP_KEY_LEN, b64_key, sizeof(b64_key));
	p = strrchr((char *) b64_key, '=');

	while (p && *p && *p == '=') {
		*p-- = '\0';
	}

	tech_pvt->transports[ttype].local_crypto_key = switch_core_session_sprintf(tech_pvt->session, "%d %s inline:%s", index, type_str, b64_key);
	tech_pvt->transports[ttype].local_crypto_data = switch_core_session_alloc(tech_pvt->session, sizeof(ldl_crypto_data_t));
	tech_pvt->transports[ttype].local_crypto_data->tag = switch_core_session_sprintf(tech_pvt->session, "%d", index);
	tech_pvt->transports[ttype].local_crypto_data->suite = switch_core_session_strdup(tech_pvt->session, type_str);
	tech_pvt->transports[ttype].local_crypto_data->key = switch_core_session_sprintf(tech_pvt->session, "inline:%s", (char *)b64_key);
	switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(tech_pvt->session), SWITCH_LOG_DEBUG, "Set Local Key [%s]\n", tech_pvt->transports[ttype].local_crypto_key);

	tech_pvt->transports[ttype].crypto_type = AES_CM_128_NULL_AUTH;


	return SWITCH_STATUS_SUCCESS;
}


static switch_status_t mdl_add_crypto(struct private_object *tech_pvt, 
									  ldl_transport_type_t ttype, const char *key_str, switch_rtp_crypto_direction_t direction)
{
	unsigned char key[SWITCH_RTP_MAX_CRYPTO_LEN];
	switch_rtp_crypto_key_type_t type;
	char *p;


	p = strchr(key_str, ' ');

	if (p && *p && *(p + 1)) {
		p++;
		if (!strncasecmp(p, SWITCH_RTP_CRYPTO_KEY_32, strlen(SWITCH_RTP_CRYPTO_KEY_32))) {
			type = AES_CM_128_HMAC_SHA1_32;
		} else if (!strncasecmp(p, SWITCH_RTP_CRYPTO_KEY_80, strlen(SWITCH_RTP_CRYPTO_KEY_80))) {
			type = AES_CM_128_HMAC_SHA1_80;
		} else {
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(tech_pvt->session), SWITCH_LOG_ERROR, "Parse Error near [%s]\n", p);
			goto bad;
		}

		p = strchr(p, ' ');
		if (p && *p && *(p + 1)) {
			p++;
			if (strncasecmp(p, "inline:", 7)) {
				switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(tech_pvt->session), SWITCH_LOG_ERROR, "Parse Error near [%s]\n", p);
				goto bad;
			}

			p += 7;
			switch_b64_decode(p, (char *) key, sizeof(key));

			if (direction == SWITCH_RTP_CRYPTO_SEND) {
				tech_pvt->transports[ttype].crypto_send_type = type;
				memcpy(tech_pvt->transports[ttype].local_raw_key, key, SWITCH_RTP_KEY_LEN);
			} else {
				tech_pvt->transports[ttype].crypto_recv_type = type;
				memcpy(tech_pvt->transports[ttype].remote_raw_key, key, SWITCH_RTP_KEY_LEN);
			}

			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(tech_pvt->session), SWITCH_LOG_NOTICE, 
							  "%s Setting %s crypto key\n", ldl_transport_type_str(ttype), switch_core_session_get_name(tech_pvt->session));
			tech_pvt->transports[ttype].has_crypto++;

			
			return SWITCH_STATUS_SUCCESS;
		}

	}

 bad:

	switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(tech_pvt->session), SWITCH_LOG_ERROR, "Error!\n");
	return SWITCH_STATUS_FALSE;

}

static void try_secure(struct private_object *tech_pvt, ldl_transport_type_t ttype) 
{

	if (!switch_test_flag(tech_pvt, TFLAG_SECURE)) {
		return;
	}


	if (tech_pvt->transports[ttype].crypto_recv_type) {
		tech_pvt->transports[ttype].crypto_type = tech_pvt->transports[ttype].crypto_recv_type;
	}


	//if (tech_pvt->transports[ttype].crypto_type) {
		switch_rtp_add_crypto_key(tech_pvt->transports[ttype].rtp_session, 
								  SWITCH_RTP_CRYPTO_SEND, 1, tech_pvt->transports[ttype].crypto_type, 
								  tech_pvt->transports[ttype].local_raw_key, SWITCH_RTP_KEY_LEN);
			

		switch_rtp_add_crypto_key(tech_pvt->transports[ttype].rtp_session, 
								  SWITCH_RTP_CRYPTO_RECV, tech_pvt->transports[ttype].crypto_tag, 
								  tech_pvt->transports[ttype].crypto_type, 
								  tech_pvt->transports[ttype].remote_raw_key, SWITCH_RTP_KEY_LEN);
			
		switch_channel_set_variable(tech_pvt->channel, "jingle_secure_audio_confirmed", "true");

		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(tech_pvt->session), SWITCH_LOG_NOTICE, 
						  "%s %s crypto confirmed\n", ldl_transport_type_str(ttype), switch_core_session_get_name(tech_pvt->session));

		//}

}



static int activate_audio_rtp(struct private_object *tech_pvt)
{
	switch_channel_t *channel = switch_core_session_get_channel(tech_pvt->session);
	const char *err;
	int ms = tech_pvt->transports[LDL_TPORT_RTP].ptime;
	switch_rtp_flag_t flags[SWITCH_RTP_FLAG_INVALID] = {0};
	int locked = 0;
	int r = 1;


	//if (switch_rtp_ready(tech_pvt->transports[LDL_TPORT_RTP].rtp_session)) {
	//	return 1;
	//}

	if (!(tech_pvt->transports[LDL_TPORT_RTP].remote_ip && tech_pvt->transports[LDL_TPORT_RTP].remote_port)) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(tech_pvt->session), SWITCH_LOG_DEBUG, "No valid rtp candidates received!\n");
		return 0;
	}

	if (switch_core_codec_ready(&tech_pvt->transports[LDL_TPORT_RTP].read_codec)) {
		locked = 1;
		switch_mutex_lock(tech_pvt->transports[LDL_TPORT_RTP].read_codec.mutex);
		if (switch_rtp_ready(tech_pvt->transports[LDL_TPORT_RTP].rtp_session)) {
			switch_rtp_kill_socket(tech_pvt->transports[LDL_TPORT_RTP].rtp_session);
			switch_rtp_destroy(&tech_pvt->transports[LDL_TPORT_RTP].rtp_session);
		}


	} else {
		if (switch_core_codec_init(&tech_pvt->transports[LDL_TPORT_RTP].read_codec,
								   tech_pvt->transports[LDL_TPORT_RTP].codec_name,
								   NULL,
								   tech_pvt->transports[LDL_TPORT_RTP].codec_rate,
								   ms,
								   1,
								   SWITCH_CODEC_FLAG_ENCODE | SWITCH_CODEC_FLAG_DECODE,
								   NULL, switch_core_session_get_pool(tech_pvt->session)) != SWITCH_STATUS_SUCCESS) {
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(tech_pvt->session), SWITCH_LOG_DEBUG, "Can't load codec?\n");
			switch_channel_hangup(channel, SWITCH_CAUSE_DESTINATION_OUT_OF_ORDER);
			r = 0;
			goto end;
		}
		tech_pvt->transports[LDL_TPORT_RTP].read_codec.session = tech_pvt->session;

		tech_pvt->transports[LDL_TPORT_RTP].read_frame.rate = tech_pvt->transports[LDL_TPORT_RTP].read_codec.implementation->samples_per_second;
		tech_pvt->transports[LDL_TPORT_RTP].read_frame.codec = &tech_pvt->transports[LDL_TPORT_RTP].read_codec;

		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(tech_pvt->session), SWITCH_LOG_DEBUG, "Set Read Codec to %s@%d\n",
						  tech_pvt->transports[LDL_TPORT_RTP].codec_name, (int) tech_pvt->transports[LDL_TPORT_RTP].read_codec.implementation->samples_per_second);

		if (switch_core_codec_init(&tech_pvt->transports[LDL_TPORT_RTP].write_codec,
								   tech_pvt->transports[LDL_TPORT_RTP].codec_name,
								   NULL,
								   tech_pvt->transports[LDL_TPORT_RTP].codec_rate,
								   ms,
								   1,
								   SWITCH_CODEC_FLAG_ENCODE | SWITCH_CODEC_FLAG_DECODE,
								   NULL, switch_core_session_get_pool(tech_pvt->session)) != SWITCH_STATUS_SUCCESS) {
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(tech_pvt->session), SWITCH_LOG_DEBUG, "Can't load codec?\n");
			switch_channel_hangup(channel, SWITCH_CAUSE_DESTINATION_OUT_OF_ORDER);
			r = 0;
			goto end;
		}
		tech_pvt->transports[LDL_TPORT_RTP].write_codec.session = tech_pvt->session;
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(tech_pvt->session), SWITCH_LOG_DEBUG, "Set Write Codec to %s@%d\n",
						  tech_pvt->transports[LDL_TPORT_RTP].codec_name, (int) tech_pvt->transports[LDL_TPORT_RTP].write_codec.implementation->samples_per_second);
		
		switch_core_session_set_read_codec(tech_pvt->session, &tech_pvt->transports[LDL_TPORT_RTP].read_codec);
		switch_core_session_set_write_codec(tech_pvt->session, &tech_pvt->transports[LDL_TPORT_RTP].write_codec);
	}

	if (globals.auto_nat && tech_pvt->profile->local_network && !switch_check_network_list_ip(tech_pvt->transports[LDL_TPORT_RTP].remote_ip, tech_pvt->profile->local_network)) {
		switch_port_t external_port = 0;
		switch_nat_add_mapping((switch_port_t) tech_pvt->transports[LDL_TPORT_RTP].local_port, SWITCH_NAT_UDP, &external_port, SWITCH_FALSE);

		if (external_port) {
			tech_pvt->transports[LDL_TPORT_RTP].adv_local_port = external_port;
			switch_set_flag(tech_pvt, TFLAG_NAT_MAP);
		} else {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "NAT mapping returned 0. Run freeswitch with -nonat since it's not working right.\n");
		}
	}

	if (tech_pvt->transports[LDL_TPORT_RTP].adv_local_port != tech_pvt->transports[LDL_TPORT_RTP].local_port) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(tech_pvt->session), SWITCH_LOG_DEBUG, "SETUP AUDIO RTP %s:%d(%d) -> %s:%d codec: %s(%d) %dh %di\n", 
						  tech_pvt->profile->ip,
						  tech_pvt->transports[LDL_TPORT_RTP].local_port, 
						  tech_pvt->transports[LDL_TPORT_RTP].adv_local_port, 
						  tech_pvt->transports[LDL_TPORT_RTP].remote_ip, 
						  tech_pvt->transports[LDL_TPORT_RTP].remote_port,
						  tech_pvt->transports[LDL_TPORT_RTP].read_codec.implementation->iananame,
						  tech_pvt->transports[LDL_TPORT_RTP].read_codec.implementation->ianacode,
						  tech_pvt->transports[LDL_TPORT_RTP].read_codec.implementation->samples_per_packet,
						  tech_pvt->transports[LDL_TPORT_RTP].read_codec.implementation->microseconds_per_packet
						  
						  );
	} else {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(tech_pvt->session), SWITCH_LOG_DEBUG, "SETUP AUDIO RTP %s:%d -> %s:%d codec: %s(%d) %dh %di\n", 
						  tech_pvt->profile->ip,
						  tech_pvt->transports[LDL_TPORT_RTP].local_port, 
						  tech_pvt->transports[LDL_TPORT_RTP].remote_ip, 
						  tech_pvt->transports[LDL_TPORT_RTP].remote_port,
						  tech_pvt->transports[LDL_TPORT_RTP].read_codec.implementation->iananame,
						  tech_pvt->transports[LDL_TPORT_RTP].read_codec.implementation->ianacode,
						  tech_pvt->transports[LDL_TPORT_RTP].read_codec.implementation->samples_per_packet,
						  tech_pvt->transports[LDL_TPORT_RTP].read_codec.implementation->microseconds_per_packet
						  );
	}

	flags[SWITCH_RTP_FLAG_DATAWAIT]++;
	flags[SWITCH_RTP_FLAG_GOOGLEHACK]++;
	flags[SWITCH_RTP_FLAG_AUTOADJ]++;
	flags[SWITCH_RTP_FLAG_RAW_WRITE]++;
	flags[SWITCH_RTP_FLAG_AUTO_CNG]++;

	if (switch_test_flag(tech_pvt->profile, TFLAG_TIMER)) {
		flags[SWITCH_RTP_FLAG_USE_TIMER]++;
	}

	if (switch_true(switch_channel_get_variable(channel, "disable_rtp_auto_adjust"))) {
		flags[SWITCH_RTP_FLAG_AUTOADJ] = 0;
	}

	if (!(tech_pvt->transports[LDL_TPORT_RTP].rtp_session = switch_rtp_new(tech_pvt->profile->ip,
																		   tech_pvt->transports[LDL_TPORT_RTP].local_port,
																		   tech_pvt->transports[LDL_TPORT_RTP].remote_ip,
																		   tech_pvt->transports[LDL_TPORT_RTP].remote_port,
																		   tech_pvt->transports[LDL_TPORT_RTP].codec_num,
																		   tech_pvt->transports[LDL_TPORT_RTP].read_codec.implementation->samples_per_packet,
																		   tech_pvt->transports[LDL_TPORT_RTP].read_codec.implementation->microseconds_per_packet,
																		   flags, tech_pvt->profile->timer_name, &err, switch_core_session_get_pool(tech_pvt->session)))) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(tech_pvt->session), SWITCH_LOG_DEBUG, "RTP ERROR %s\n", err);
		switch_channel_hangup(channel, SWITCH_CAUSE_DESTINATION_OUT_OF_ORDER);
		r = 0;
		goto end;
	} else {
		uint8_t vad_in = switch_test_flag(tech_pvt, TFLAG_VAD_IN) ? 1 : 0;
		uint8_t vad_out = switch_test_flag(tech_pvt, TFLAG_VAD_OUT) ? 1 : 0;
		uint8_t inb = switch_test_flag(tech_pvt, TFLAG_OUTBOUND) ? 0 : 1;

		switch_rtp_set_ssrc(tech_pvt->transports[LDL_TPORT_RTP].rtp_session, tech_pvt->transports[LDL_TPORT_RTP].ssrc);

		switch_rtp_intentional_bugs(tech_pvt->transports[LDL_TPORT_RTP].rtp_session, RTP_BUG_GEN_ONE_GEN_ALL);


		if (tech_pvt->transports[LDL_TPORT_RTCP].remote_port) {
			switch_rtp_activate_rtcp(tech_pvt->transports[LDL_TPORT_RTP].rtp_session, MDL_RTCP_DUR, 
									 tech_pvt->transports[LDL_TPORT_RTCP].remote_port, SWITCH_FALSE);

		}

		try_secure(tech_pvt, LDL_TPORT_RTP);

		switch_rtp_activate_ice(tech_pvt->transports[LDL_TPORT_RTP].rtp_session, 
								tech_pvt->transports[LDL_TPORT_RTP].remote_user, 
								tech_pvt->transports[LDL_TPORT_RTP].local_user,
								tech_pvt->transports[LDL_TPORT_RTP].remote_pass, NULL, 
								IPR_RTP,
								ICE_GOOGLE_JINGLE, 0);
		
		if ((vad_in && inb) || (vad_out && !inb)) {
			if (switch_rtp_enable_vad(tech_pvt->transports[LDL_TPORT_RTP].rtp_session, tech_pvt->session, &tech_pvt->transports[LDL_TPORT_RTP].read_codec, SWITCH_VAD_FLAG_TALKING) != SWITCH_STATUS_SUCCESS) {
				switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(tech_pvt->session), SWITCH_LOG_DEBUG, "VAD ERROR %s\n", err);
				switch_channel_hangup(channel, SWITCH_CAUSE_DESTINATION_OUT_OF_ORDER);
				r = 0;
				goto end;
			}
			switch_set_flag_locked(tech_pvt, TFLAG_VAD);
		}
		//switch_rtp_set_cng_pt(tech_pvt->transports[LDL_TPORT_RTP].rtp_session, 13);
		switch_rtp_set_telephony_event(tech_pvt->transports[LDL_TPORT_RTP].rtp_session, 101);
		
		if (tech_pvt->transports[LDL_TPORT_RTCP].remote_port) {
			switch_rtp_activate_ice(tech_pvt->transports[LDL_TPORT_RTP].rtp_session, 
									tech_pvt->transports[LDL_TPORT_RTCP].remote_user, 
									tech_pvt->transports[LDL_TPORT_RTCP].local_user,
									tech_pvt->transports[LDL_TPORT_RTCP].remote_pass, 
									NULL, IPR_RTCP,
									ICE_GOOGLE_JINGLE, 0);
			
		}

		

	}

 end:

	if (locked) {
		switch_mutex_unlock(tech_pvt->transports[LDL_TPORT_RTP].read_codec.mutex);
	}

	return r;
}


static int activate_video_rtp(struct private_object *tech_pvt)
{
	switch_channel_t *channel = switch_core_session_get_channel(tech_pvt->session);
	const char *err;
	int ms = 0;
	switch_rtp_flag_t flags[SWITCH_RTP_FLAG_INVALID] = {0};
	int r = 1, locked = 0;

	
	if (switch_rtp_ready(tech_pvt->transports[LDL_TPORT_VIDEO_RTP].rtp_session)) {
			r = 1; goto end;
	}

	if (!(tech_pvt->transports[LDL_TPORT_VIDEO_RTP].remote_ip && tech_pvt->transports[LDL_TPORT_VIDEO_RTP].remote_port)) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(tech_pvt->session), SWITCH_LOG_DEBUG, "No valid video_rtp candidates received!\n");
		r = 0; goto end;
	}

	if (zstr(tech_pvt->transports[LDL_TPORT_VIDEO_RTP].codec_name)) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(tech_pvt->session), SWITCH_LOG_DEBUG, "No valid video_rtp codecs received!\n");
		r = 0; goto end;		
	}

	if (switch_core_codec_ready(&tech_pvt->transports[LDL_TPORT_VIDEO_RTP].read_codec)) {
		locked = 1;
		switch_mutex_lock(tech_pvt->transports[LDL_TPORT_VIDEO_RTP].read_codec.mutex);
		if (switch_rtp_ready(tech_pvt->transports[LDL_TPORT_VIDEO_RTP].rtp_session)) {
			switch_rtp_kill_socket(tech_pvt->transports[LDL_TPORT_RTP].rtp_session);
			switch_rtp_destroy(&tech_pvt->transports[LDL_TPORT_VIDEO_RTP].rtp_session);
		}

	} else {
		if (switch_core_codec_init(&tech_pvt->transports[LDL_TPORT_VIDEO_RTP].read_codec,
								   tech_pvt->transports[LDL_TPORT_VIDEO_RTP].codec_name,
								   NULL,
								   tech_pvt->transports[LDL_TPORT_VIDEO_RTP].codec_rate,
								   ms,
								   1,
								   SWITCH_CODEC_FLAG_ENCODE | SWITCH_CODEC_FLAG_DECODE,
								   NULL, switch_core_session_get_pool(tech_pvt->session)) != SWITCH_STATUS_SUCCESS) {
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(tech_pvt->session), SWITCH_LOG_DEBUG, "Can't load codec?\n");
			switch_channel_hangup(channel, SWITCH_CAUSE_DESTINATION_OUT_OF_ORDER);
			r = 0; goto end;
		}
		tech_pvt->transports[LDL_TPORT_VIDEO_RTP].read_frame.rate = tech_pvt->transports[LDL_TPORT_VIDEO_RTP].read_codec.implementation->samples_per_second;
		tech_pvt->transports[LDL_TPORT_VIDEO_RTP].read_frame.codec = &tech_pvt->transports[LDL_TPORT_VIDEO_RTP].read_codec;

		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(tech_pvt->session), SWITCH_LOG_DEBUG, "Set Read Codec to %s@%d\n",
						  tech_pvt->transports[LDL_TPORT_VIDEO_RTP].codec_name, (int) tech_pvt->transports[LDL_TPORT_VIDEO_RTP].read_codec.implementation->samples_per_second);

		if (switch_core_codec_init(&tech_pvt->transports[LDL_TPORT_VIDEO_RTP].write_codec,
								   tech_pvt->transports[LDL_TPORT_VIDEO_RTP].codec_name,
								   NULL,
								   tech_pvt->transports[LDL_TPORT_VIDEO_RTP].codec_rate,
								   ms,
								   1,
								   SWITCH_CODEC_FLAG_ENCODE | SWITCH_CODEC_FLAG_DECODE,
								   NULL, switch_core_session_get_pool(tech_pvt->session)) != SWITCH_STATUS_SUCCESS) {
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(tech_pvt->session), SWITCH_LOG_DEBUG, "Can't load codec?\n");
			switch_channel_hangup(channel, SWITCH_CAUSE_DESTINATION_OUT_OF_ORDER);
			r = 0; goto end;
		}
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(tech_pvt->session), SWITCH_LOG_DEBUG, "Set Write Codec to %s@%d\n",
						  tech_pvt->transports[LDL_TPORT_VIDEO_RTP].codec_name, (int) tech_pvt->transports[LDL_TPORT_VIDEO_RTP].write_codec.implementation->samples_per_second);

		switch_core_session_set_video_read_codec(tech_pvt->session, &tech_pvt->transports[LDL_TPORT_VIDEO_RTP].read_codec);
		switch_core_session_set_video_write_codec(tech_pvt->session, &tech_pvt->transports[LDL_TPORT_VIDEO_RTP].write_codec);
	}

	if (globals.auto_nat && tech_pvt->profile->local_network && !switch_check_network_list_ip(tech_pvt->transports[LDL_TPORT_VIDEO_RTP].remote_ip, tech_pvt->profile->local_network)) {
		switch_port_t external_port = 0;
		switch_nat_add_mapping((switch_port_t) tech_pvt->transports[LDL_TPORT_VIDEO_RTP].local_port, SWITCH_NAT_UDP, &external_port, SWITCH_FALSE);

		if (external_port) {
			tech_pvt->transports[LDL_TPORT_VIDEO_RTP].adv_local_port = external_port;
			switch_set_flag(tech_pvt, TFLAG_NAT_MAP);
		} else {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "NAT mapping returned 0. Run freeswitch with -nonat since it's not working right.\n");
		}
	}


	if (tech_pvt->transports[LDL_TPORT_VIDEO_RTP].adv_local_port != tech_pvt->transports[LDL_TPORT_VIDEO_RTP].local_port) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(tech_pvt->session), SWITCH_LOG_DEBUG, "SETUP VIDEO RTP %s:%d(%d) -> %s:%d codec: %s(%d) %dh %di\n", 
						  tech_pvt->profile->ip,
						  tech_pvt->transports[LDL_TPORT_VIDEO_RTP].local_port, 
						  tech_pvt->transports[LDL_TPORT_VIDEO_RTP].adv_local_port, 
						  tech_pvt->transports[LDL_TPORT_VIDEO_RTP].remote_ip, 
						  tech_pvt->transports[LDL_TPORT_VIDEO_RTP].remote_port,
						  tech_pvt->transports[LDL_TPORT_VIDEO_RTP].read_codec.implementation->iananame,
						  tech_pvt->transports[LDL_TPORT_VIDEO_RTP].read_codec.implementation->ianacode,
						  tech_pvt->transports[LDL_TPORT_VIDEO_RTP].read_codec.implementation->samples_per_packet,
						  tech_pvt->transports[LDL_TPORT_VIDEO_RTP].read_codec.implementation->microseconds_per_packet
						  
						  );
	} else {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(tech_pvt->session), SWITCH_LOG_DEBUG, "SETUP VIDEO RTP %s:%d -> %s:%d codec: %s(%d) %dh %di\n", 
						  tech_pvt->profile->ip,
						  tech_pvt->transports[LDL_TPORT_VIDEO_RTP].local_port, 
						  tech_pvt->transports[LDL_TPORT_VIDEO_RTP].remote_ip, 
						  tech_pvt->transports[LDL_TPORT_VIDEO_RTP].remote_port,
						  tech_pvt->transports[LDL_TPORT_VIDEO_RTP].read_codec.implementation->iananame,
						  tech_pvt->transports[LDL_TPORT_VIDEO_RTP].read_codec.implementation->ianacode,
						  tech_pvt->transports[LDL_TPORT_VIDEO_RTP].read_codec.implementation->samples_per_packet,
						  tech_pvt->transports[LDL_TPORT_VIDEO_RTP].read_codec.implementation->microseconds_per_packet
						  );
	}

	flags[SWITCH_RTP_FLAG_DATAWAIT]++;
	flags[SWITCH_RTP_FLAG_GOOGLEHACK]++;
	flags[SWITCH_RTP_FLAG_AUTOADJ]++;
	flags[SWITCH_RTP_FLAG_RAW_WRITE]++;
	flags[SWITCH_RTP_FLAG_VIDEO]++;

	if (switch_true(switch_channel_get_variable(channel, "disable_rtp_auto_adjust"))) {
		flags[SWITCH_RTP_FLAG_AUTOADJ] = 0;
	}

	if (!(tech_pvt->transports[LDL_TPORT_VIDEO_RTP].rtp_session = switch_rtp_new(tech_pvt->profile->ip,
																				 tech_pvt->transports[LDL_TPORT_VIDEO_RTP].local_port,
																				 tech_pvt->transports[LDL_TPORT_VIDEO_RTP].remote_ip,
																				 tech_pvt->transports[LDL_TPORT_VIDEO_RTP].remote_port,
																				 tech_pvt->transports[LDL_TPORT_VIDEO_RTP].codec_num,
																				 1,
																				 90000,
																				 flags, NULL, &err, switch_core_session_get_pool(tech_pvt->session)))) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(tech_pvt->session), SWITCH_LOG_DEBUG, "RTP ERROR %s\n", err);
		switch_channel_hangup(channel, SWITCH_CAUSE_DESTINATION_OUT_OF_ORDER);
		r = 0; goto end;
	} else {
		switch_rtp_set_ssrc(tech_pvt->transports[LDL_TPORT_VIDEO_RTP].rtp_session, tech_pvt->transports[LDL_TPORT_VIDEO_RTP].ssrc);

		if (tech_pvt->transports[LDL_TPORT_VIDEO_RTCP].remote_port) {
			switch_rtp_activate_rtcp(tech_pvt->transports[LDL_TPORT_VIDEO_RTP].rtp_session, MDL_RTCP_DUR, 
									 tech_pvt->transports[LDL_TPORT_VIDEO_RTCP].remote_port, SWITCH_FALSE);
		}
		try_secure(tech_pvt, LDL_TPORT_VIDEO_RTP);


		switch_rtp_activate_ice(tech_pvt->transports[LDL_TPORT_VIDEO_RTP].rtp_session, 
								tech_pvt->transports[LDL_TPORT_VIDEO_RTP].remote_user, 
								tech_pvt->transports[LDL_TPORT_VIDEO_RTP].local_user,
								NULL, NULL, IPR_RTP, ICE_GOOGLE_JINGLE, 0);//tech_pvt->transports[LDL_TPORT_VIDEO_RTP].remote_pass);
		switch_channel_set_flag(channel, CF_VIDEO);
		switch_set_flag(tech_pvt, TFLAG_VIDEO_RTP_READY);
		//switch_rtp_set_default_payload(tech_pvt->transports[LDL_TPORT_VIDEO_RTP].rtp_session, tech_pvt->transports[LDL_TPORT_VIDEO_RTP].codec_num);
		//switch_rtp_set_recv_pt(tech_pvt->transports[LDL_TPORT_VIDEO_RTP].rtp_session, tech_pvt->transports[LDL_TPORT_VIDEO_RTP].codec_num);
		

		if (tech_pvt->transports[LDL_TPORT_VIDEO_RTCP].remote_port) {
			
			switch_rtp_activate_ice(tech_pvt->transports[LDL_TPORT_VIDEO_RTP].rtp_session, 
									tech_pvt->transports[LDL_TPORT_VIDEO_RTCP].remote_user, 
									tech_pvt->transports[LDL_TPORT_VIDEO_RTCP].local_user,
									NULL, NULL, IPR_RTCP, ICE_GOOGLE_JINGLE, 0);//tech_pvt->transports[LDL_TPORT_VIDEO_RTCP].remote_pass);
		}


		
	}

 end:
	if (locked) {
		switch_mutex_unlock(tech_pvt->transports[LDL_TPORT_VIDEO_RTP].read_codec.mutex);
	}

	return r;
}



static int activate_rtp(struct private_object *tech_pvt)
{
	int r = 0;

	if (tech_pvt->transports[LDL_TPORT_RTP].ready) {
		r += activate_audio_rtp(tech_pvt);
	}

	if (tech_pvt->transports[LDL_TPORT_VIDEO_RTP].ready) {
		if (switch_test_flag(tech_pvt, TFLAG_OUTBOUND) || tech_pvt->transports[LDL_TPORT_VIDEO_RTCP].accepted) {
			r += activate_video_rtp(tech_pvt);
		}
	}

	return r;
}


static int do_tport_candidates(struct private_object *tech_pvt, ldl_transport_type_t ttype, ldl_candidate_t *cand, int force)
{
	switch_channel_t *channel = switch_core_session_get_channel(tech_pvt->session);
	char *advip = tech_pvt->profile->extip ? tech_pvt->profile->extip : tech_pvt->profile->ip;
	char *err = NULL, *address = NULL;
	
	if (!force && tech_pvt->transports[ttype].ready) {
		return 0;
	}

	if (switch_test_flag(tech_pvt, TFLAG_LANADDR)) {
		advip = tech_pvt->profile->ip;
	}
	address = advip;

	if (address && !strncasecmp(address, "host:", 5)) {
		char *lookup = switch_stun_host_lookup(address + 5, switch_core_session_get_pool(tech_pvt->session));

		if (zstr(lookup)) {
			address = address + 5;
		} else {
			address = lookup;
		}
	}

	memset(cand, 0, sizeof(*cand));
	switch_stun_random_string(tech_pvt->transports[ttype].local_user, 16, NULL);
	switch_stun_random_string(tech_pvt->transports[ttype].local_pass, 16, NULL);

	cand->port = tech_pvt->transports[ttype].adv_local_port;
	cand->address = address;

	if (!strncasecmp(advip, "stun:", 5)) {
		char *stun_ip = advip + 5;

		if (tech_pvt->transports[ttype].stun_ip) {
			cand->address = tech_pvt->transports[ttype].stun_ip;
			cand->port = tech_pvt->transports[ttype].stun_port;
		} else {
			if (!stun_ip) {
				switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(tech_pvt->session), SWITCH_LOG_ERROR, "Stun Failed! NO STUN SERVER!\n");
				switch_channel_hangup(channel, SWITCH_CAUSE_DESTINATION_OUT_OF_ORDER);
				return 0;
			}

			cand->address = tech_pvt->profile->ip;
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(tech_pvt->session), SWITCH_LOG_DEBUG, "Stun Lookup Local %s:%d\n", cand->address,
							  cand->port);
			if (switch_stun_lookup
				(&cand->address, &cand->port, stun_ip, SWITCH_STUN_DEFAULT_PORT, &err,
				 switch_core_session_get_pool(tech_pvt->session)) != SWITCH_STATUS_SUCCESS) {
				switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(tech_pvt->session), SWITCH_LOG_ERROR, "Stun Failed! %s:%d [%s]\n", stun_ip,
								  SWITCH_STUN_DEFAULT_PORT, err);
				switch_channel_hangup(channel, SWITCH_CAUSE_DESTINATION_OUT_OF_ORDER);
				return 0;
			}
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(tech_pvt->session), SWITCH_LOG_INFO, "Stun Success %s:%d\n", cand->address, cand->port);
		}
		cand->type = "stun";
		tech_pvt->transports[ttype].stun_ip = switch_core_session_strdup(tech_pvt->session, cand->address);
		tech_pvt->transports[ttype].stun_port = cand->port;
	} else {
		cand->type = "local";
	}

	cand->name = (char *)ldl_transport_type_str(ttype);
	cand->username = tech_pvt->transports[ttype].local_user;
	cand->password = tech_pvt->transports[ttype].local_pass;
	cand->pref = 1;
	cand->protocol = "udp";
	switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(tech_pvt->session), SWITCH_LOG_DEBUG, 
					  "Send %s Candidate %s:%d [%s]\n", ldl_transport_type_str(ttype), cand->address, cand->port,
					  cand->username);


		
	tech_pvt->transports[ttype].ready = 1;
	
	return 1;
}


static int do_candidates(struct private_object *tech_pvt, int force)
{
	ldl_candidate_t cand[4] = {{0}};
	int idx = 0;

	if (switch_test_flag(tech_pvt, TFLAG_DO_CAND)) {
		return 1;
	}

	tech_pvt->next_cand += DL_CAND_WAIT;
	if (switch_test_flag(tech_pvt, TFLAG_BYE) || !tech_pvt->dlsession) {
		return 0;
	}
	switch_set_flag_locked(tech_pvt, TFLAG_DO_CAND);

	idx += do_tport_candidates(tech_pvt, LDL_TPORT_RTP, &cand[idx], force);
	idx += do_tport_candidates(tech_pvt, LDL_TPORT_RTCP, &cand[idx], force);

	if (tech_pvt->transports[LDL_TPORT_VIDEO_RTP].codec_index > -1) {
		idx += do_tport_candidates(tech_pvt, LDL_TPORT_VIDEO_RTP, &cand[idx], force);
		idx += do_tport_candidates(tech_pvt, LDL_TPORT_VIDEO_RTCP, &cand[idx], force);
	}

	if (idx && cand[0].name) {
		if (ldl_session_gateway(tech_pvt->dlsession) && switch_test_flag(tech_pvt, TFLAG_OUTBOUND)) {
			tech_pvt->cand_id = ldl_session_transport(tech_pvt->dlsession, cand, idx);
		} else {
			tech_pvt->cand_id = ldl_session_candidates(tech_pvt->dlsession, cand, idx);
		}
	}

	switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(tech_pvt->session), SWITCH_LOG_DEBUG, "Accepted %u of %u rtp candidates.\n", 
					  tech_pvt->transports[LDL_TPORT_RTP].accepted, tech_pvt->transports[LDL_TPORT_RTP].total);
	switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(tech_pvt->session), SWITCH_LOG_DEBUG, "Accepted %u of %u rtcp candidates.\n", 
					  tech_pvt->transports[LDL_TPORT_RTCP].accepted, tech_pvt->transports[LDL_TPORT_RTCP].total);

	switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(tech_pvt->session), SWITCH_LOG_DEBUG, "Accepted %u of %u video_rtp candidates\n", 
					  tech_pvt->transports[LDL_TPORT_VIDEO_RTP].accepted, tech_pvt->transports[LDL_TPORT_VIDEO_RTP].total);

	switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(tech_pvt->session), SWITCH_LOG_DEBUG, "Accepted %u of %u video_rctp candidates\n", 
					  tech_pvt->transports[LDL_TPORT_VIDEO_RTCP].accepted, tech_pvt->transports[LDL_TPORT_VIDEO_RTCP].total);



	if ((tech_pvt->transports[LDL_TPORT_RTP].ready && tech_pvt->transports[LDL_TPORT_RTCP].ready)) {
		switch_set_flag_locked(tech_pvt, TFLAG_TRANSPORT);
		switch_set_flag_locked(tech_pvt, TFLAG_RTP_READY);
		tech_pvt->audio_ready = switch_micro_time_now();
	}


	switch_clear_flag_locked(tech_pvt, TFLAG_DO_CAND);
	return 1;

}




static char *lame(char *in)
{
	if (!strncasecmp(in, "ilbc", 4)) {
		return "iLBC";
	} else {
		return in;
	}
}


static void setup_codecs(struct private_object *tech_pvt)
{
	ldl_payload_t payloads[LDL_MAX_PAYLOADS] = { {0} };
	int idx = 0, i = 0;
	int dft_audio = -1, dft_video = -1;

	memset(payloads, 0, sizeof(payloads));

	for (idx = 0; idx < tech_pvt->num_codecs && (dft_audio == -1 || dft_video == -1); idx++) {
		if (dft_audio < 0 && tech_pvt->codecs[idx]->codec_type == SWITCH_CODEC_TYPE_AUDIO) {
			dft_audio = idx;
		}
		if (dft_video < 0 && tech_pvt->codecs[idx]->codec_type == SWITCH_CODEC_TYPE_VIDEO) {
			dft_video = idx;
		}
	}
	
	if (dft_audio == -1 && dft_video == -1) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(tech_pvt->session), SWITCH_LOG_ERROR, "Cannot find a codec.\n");  
		return;
	}

	idx = 0;

	payloads[0].type = LDL_TPORT_RTP;
	if (tech_pvt->transports[LDL_TPORT_RTP].codec_index < 0) {
		if (dft_audio > -1) {
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(tech_pvt->session), SWITCH_LOG_DEBUG, "Don't have my audio codec yet here's one\n");
			tech_pvt->transports[LDL_TPORT_RTP].codec_name = lame(tech_pvt->codecs[dft_audio]->iananame);
			tech_pvt->transports[LDL_TPORT_RTP].codec_num = tech_pvt->codecs[dft_audio]->ianacode;
			tech_pvt->transports[LDL_TPORT_RTP].codec_rate = tech_pvt->codecs[dft_audio]->samples_per_second;
			tech_pvt->transports[LDL_TPORT_RTP].r_codec_num = tech_pvt->codecs[dft_audio]->ianacode;
			tech_pvt->transports[LDL_TPORT_RTP].codec_index = dft_audio;

			payloads[0].name = lame(tech_pvt->codecs[dft_audio]->iananame);
			payloads[0].id = tech_pvt->codecs[dft_audio]->ianacode;
			payloads[0].rate = tech_pvt->codecs[dft_audio]->samples_per_second;
			payloads[0].bps = tech_pvt->codecs[dft_audio]->bits_per_second;
			payloads[0].ptime = tech_pvt->codecs[dft_audio]->microseconds_per_packet / 1000;
			idx++;
		} else {
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(tech_pvt->session), SWITCH_LOG_DEBUG, "Don't have an audio codec.\n");
		}
	} else {
		payloads[0].name = lame(tech_pvt->codecs[tech_pvt->transports[LDL_TPORT_RTP].codec_index]->iananame);
		payloads[0].id = tech_pvt->codecs[tech_pvt->transports[LDL_TPORT_RTP].codec_index]->ianacode;
		payloads[0].rate = tech_pvt->codecs[tech_pvt->transports[LDL_TPORT_RTP].codec_index]->samples_per_second;
		payloads[0].bps = tech_pvt->codecs[tech_pvt->transports[LDL_TPORT_RTP].codec_index]->bits_per_second;
		payloads[0].ptime = tech_pvt->codecs[tech_pvt->transports[LDL_TPORT_RTP].codec_index]->microseconds_per_packet / 1000;
		idx++;
	}


	payloads[1].type = LDL_TPORT_VIDEO_RTP;
	if (tech_pvt->transports[LDL_TPORT_VIDEO_RTP].codec_index < 0) {
		if (dft_video > -1) {
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(tech_pvt->session), SWITCH_LOG_DEBUG, "Don't have my video codec yet here's one\n");
			tech_pvt->transports[LDL_TPORT_VIDEO_RTP].codec_name = lame(tech_pvt->codecs[dft_video]->iananame);
			tech_pvt->transports[LDL_TPORT_VIDEO_RTP].codec_num = tech_pvt->codecs[dft_video]->ianacode;
			tech_pvt->transports[LDL_TPORT_VIDEO_RTP].codec_rate = tech_pvt->codecs[dft_video]->samples_per_second;
			tech_pvt->transports[LDL_TPORT_VIDEO_RTP].r_codec_num = tech_pvt->codecs[dft_video]->ianacode;
			tech_pvt->transports[LDL_TPORT_VIDEO_RTP].codec_index = dft_video;

			payloads[1].name = lame(tech_pvt->codecs[dft_video]->iananame);
			payloads[1].id = tech_pvt->codecs[dft_video]->ianacode;
			payloads[1].rate = tech_pvt->codecs[dft_video]->samples_per_second;
			payloads[1].bps = tech_pvt->codecs[dft_video]->bits_per_second;
			payloads[1].width = 600;
			payloads[1].height = 400;
			payloads[1].framerate = 30;
			
			idx++;
		} else {
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(tech_pvt->session), SWITCH_LOG_DEBUG, "Don't have video codec.\n");
		}
	} else {
		payloads[1].name = lame(tech_pvt->codecs[tech_pvt->transports[LDL_TPORT_VIDEO_RTP].codec_index]->iananame);
		payloads[1].id = tech_pvt->codecs[tech_pvt->transports[LDL_TPORT_VIDEO_RTP].codec_index]->ianacode;
		payloads[1].rate = tech_pvt->codecs[tech_pvt->transports[LDL_TPORT_VIDEO_RTP].codec_index]->samples_per_second;
		payloads[1].bps = tech_pvt->codecs[tech_pvt->transports[LDL_TPORT_VIDEO_RTP].codec_index]->bits_per_second;
		idx++;
	}

	for(i = 0; i < idx; i++) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(tech_pvt->session), SWITCH_LOG_DEBUG, "Send Describe [%s@%d]\n", payloads[i].name, payloads[i].rate);
	}


	if (!payloads[1].id && tech_pvt->transports[LDL_TPORT_VIDEO_RTP].local_port) {
		switch_rtp_release_port(tech_pvt->profile->ip, tech_pvt->transports[LDL_TPORT_VIDEO_RTP].local_port);
		tech_pvt->transports[LDL_TPORT_VIDEO_RTP].local_port = 0;
	}


	tech_pvt->desc_id = ldl_session_describe(tech_pvt->dlsession, payloads, idx,
											 switch_test_flag(tech_pvt, TFLAG_OUTBOUND) ? LDL_DESCRIPTION_INITIATE : LDL_DESCRIPTION_ACCEPT,
											 &tech_pvt->transports[LDL_TPORT_RTP].ssrc, &tech_pvt->transports[LDL_TPORT_VIDEO_RTP].ssrc, 
											 tech_pvt->transports[LDL_TPORT_RTP].local_crypto_data, 
											 tech_pvt->transports[LDL_TPORT_VIDEO_RTP].local_crypto_data);


}

static int do_describe(struct private_object *tech_pvt, int force)
{

	if (!tech_pvt->session) {
		return 0;
	}

	tech_pvt->next_desc += DL_CAND_WAIT;

	if (switch_test_flag(tech_pvt, TFLAG_BYE)) {
		return 0;
	}


	switch_set_flag_locked(tech_pvt, TFLAG_DO_CAND);
	if (!get_codecs(tech_pvt)) {
		terminate_session(&tech_pvt->session, __LINE__, SWITCH_CAUSE_INCOMPATIBLE_DESTINATION);
		switch_set_flag_locked(tech_pvt, TFLAG_BYE);
		switch_clear_flag_locked(tech_pvt, TFLAG_IO);
		return 0;
	}


	if (force || !switch_test_flag(tech_pvt, TFLAG_CODEC_READY)) {
		setup_codecs(tech_pvt);
		switch_set_flag_locked(tech_pvt, TFLAG_CODEC_READY);
	}
	switch_clear_flag_locked(tech_pvt, TFLAG_DO_CAND);
	return 1;
}

static switch_status_t negotiate_media(switch_core_session_t *session)
{
	switch_status_t ret = SWITCH_STATUS_FALSE;
	switch_channel_t *channel = switch_core_session_get_channel(session);
	struct private_object *tech_pvt = NULL;
	switch_time_t started;
	switch_time_t now;
	unsigned int elapsed, audio_elapsed;

	tech_pvt = switch_core_session_get_private(session);
	switch_assert(tech_pvt != NULL);

	switch_set_flag_locked(tech_pvt, TFLAG_IO);

	started = switch_micro_time_now();

	/* jingle has no ringing indication so we will just pretend that we got one */
	switch_channel_mark_ring_ready(channel);

	if (switch_test_flag(tech_pvt, TFLAG_OUTBOUND)) {
		tech_pvt->next_cand = started + DL_CAND_WAIT;
		tech_pvt->next_desc = started;
	} else {
		tech_pvt->next_cand = started + DL_CAND_WAIT;
		tech_pvt->next_desc = started + DL_CAND_WAIT;
	}

	while (!(switch_test_flag(tech_pvt, TFLAG_CODEC_READY) &&
			 switch_test_flag(tech_pvt, TFLAG_RTP_READY) &&
			 (switch_test_flag(tech_pvt, TFLAG_OUTBOUND) || switch_test_flag(tech_pvt, TFLAG_VIDEO_RTP_READY)) &&
			 switch_test_flag(tech_pvt, TFLAG_ANSWER) && switch_test_flag(tech_pvt, TFLAG_TRANSPORT_ACCEPT) && //tech_pvt->read_count &&
			 tech_pvt->transports[LDL_TPORT_RTP].remote_ip && tech_pvt->transports[LDL_TPORT_RTP].remote_port && switch_test_flag(tech_pvt, TFLAG_TRANSPORT))) {
		now = switch_micro_time_now();
		elapsed = (unsigned int) ((now - started) / 1000);


		if (switch_test_flag(tech_pvt, TFLAG_RTP_READY) && !switch_test_flag(tech_pvt, TFLAG_VIDEO_RTP_READY)) {
			audio_elapsed = (unsigned int) ((now - tech_pvt->audio_ready) / 1000);
			if (audio_elapsed > 1000) {
				switch_set_flag(tech_pvt, TFLAG_VIDEO_RTP_READY);
			}
		}

		if (switch_channel_down(channel) || switch_test_flag(tech_pvt, TFLAG_BYE)) {
			goto out;
		}


		if (now >= tech_pvt->next_desc) {
			if (!do_describe(tech_pvt, 0)) {
				goto out;
			}
		}

		if (tech_pvt->next_cand && now >= tech_pvt->next_cand) {
			if (!do_candidates(tech_pvt, 0)) {
				goto out;
			}
		}
		if (elapsed > 60000) {
			terminate_session(&tech_pvt->session, __LINE__, SWITCH_CAUSE_NORMAL_CLEARING);
			switch_set_flag_locked(tech_pvt, TFLAG_BYE);
			switch_clear_flag_locked(tech_pvt, TFLAG_IO);
			goto done;
		}

		if (switch_test_flag(tech_pvt, TFLAG_BYE) || !switch_test_flag(tech_pvt, TFLAG_IO)) {
			goto done;
		}

		if (switch_rtp_ready(tech_pvt->transports[LDL_TPORT_RTP].rtp_session)) {
			switch_rtp_ping(tech_pvt->transports[LDL_TPORT_RTP].rtp_session);
		}

		if (switch_rtp_ready(tech_pvt->transports[LDL_TPORT_VIDEO_RTP].rtp_session)) {
			switch_rtp_ping(tech_pvt->transports[LDL_TPORT_VIDEO_RTP].rtp_session);
		}		

		switch_yield(20000);
	}

	if (switch_channel_down(channel) || switch_test_flag(tech_pvt, TFLAG_BYE)) {
		goto done;
	}

	if (!activate_rtp(tech_pvt)) {
		goto done;
	}

	if (switch_test_flag(tech_pvt, TFLAG_OUTBOUND)) {
		if (!do_candidates(tech_pvt, 0)) {
			goto done;
		}
		if (switch_test_flag(tech_pvt, TFLAG_TRANSPORT_ACCEPT)) {
			switch_channel_answer(channel);
		}
	}
	ret = SWITCH_STATUS_SUCCESS;

	switch_channel_audio_sync(channel); 


	goto done;

  out:
	terminate_session(&session, __LINE__, SWITCH_CAUSE_DESTINATION_OUT_OF_ORDER);
  done:

	return ret;
}

/* 
   State methods they get called when the state changes to the specific state 
   returning SWITCH_STATUS_SUCCESS tells the core to execute the standard state method next
   so if you fully implement the state you can return SWITCH_STATUS_FALSE to skip it.
*/
static switch_status_t channel_on_init(switch_core_session_t *session)
{
	switch_channel_t *channel = switch_core_session_get_channel(session);
	struct private_object *tech_pvt = NULL;
	switch_status_t status = SWITCH_STATUS_SUCCESS;

	tech_pvt = switch_core_session_get_private(session);
	switch_assert(tech_pvt != NULL);

	tech_pvt->transports[LDL_TPORT_RTP].read_frame.buflen = SWITCH_RTP_MAX_BUF_LEN;

	switch_set_flag(tech_pvt, TFLAG_READY);

	if (negotiate_media(session) == SWITCH_STATUS_SUCCESS) {
		if (switch_channel_direction(channel) == SWITCH_CALL_DIRECTION_OUTBOUND) {
			switch_channel_mark_answered(channel);
		}
		status = SWITCH_STATUS_SUCCESS;
	}

	return status;
}

static switch_status_t channel_on_routing(switch_core_session_t *session)
{
	switch_channel_t *channel = switch_core_session_get_channel(session);
	struct private_object *tech_pvt = NULL;

	tech_pvt = switch_core_session_get_private(session);
	switch_assert(tech_pvt != NULL);

	switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "%s CHANNEL ROUTING\n", switch_channel_get_name(channel));

	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t channel_on_execute(switch_core_session_t *session)
{

	switch_channel_t *channel = NULL;
	struct private_object *tech_pvt = NULL;

	channel = switch_core_session_get_channel(session);
	switch_assert(channel != NULL);

	tech_pvt = switch_core_session_get_private(session);
	switch_assert(tech_pvt != NULL);

	switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "%s CHANNEL EXECUTE\n", switch_channel_get_name(channel));


	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t channel_on_destroy(switch_core_session_t *session)
{
	//switch_channel_t *channel = switch_core_session_get_channel(session);
	struct private_object *tech_pvt = NULL;

	tech_pvt = switch_core_session_get_private(session);

	if (tech_pvt) {
		if (tech_pvt->transports[LDL_TPORT_RTP].rtp_session) {
			switch_rtp_destroy(&tech_pvt->transports[LDL_TPORT_RTP].rtp_session);
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "NUKE RTP\n");
			tech_pvt->transports[LDL_TPORT_RTP].rtp_session = NULL;
		}

		if (tech_pvt->transports[LDL_TPORT_VIDEO_RTP].rtp_session) {
			switch_rtp_destroy(&tech_pvt->transports[LDL_TPORT_VIDEO_RTP].rtp_session);
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "NUKE RTP\n");
			tech_pvt->transports[LDL_TPORT_VIDEO_RTP].rtp_session = NULL;
		}

		if (switch_test_flag(tech_pvt, TFLAG_NAT_MAP)) {
			switch_nat_del_mapping((switch_port_t) tech_pvt->transports[LDL_TPORT_RTP].adv_local_port, SWITCH_NAT_UDP);
			switch_clear_flag(tech_pvt, TFLAG_NAT_MAP);
		}

		if (switch_core_codec_ready(&tech_pvt->transports[LDL_TPORT_RTP].read_codec)) {
			switch_core_codec_destroy(&tech_pvt->transports[LDL_TPORT_RTP].read_codec);
		}

		if (switch_core_codec_ready(&tech_pvt->transports[LDL_TPORT_RTP].write_codec)) {
			switch_core_codec_destroy(&tech_pvt->transports[LDL_TPORT_RTP].write_codec);
		}

		if (switch_core_codec_ready(&tech_pvt->transports[LDL_TPORT_VIDEO_RTP].read_codec)) {
			switch_core_codec_destroy(&tech_pvt->transports[LDL_TPORT_VIDEO_RTP].read_codec);
		}

		if (switch_core_codec_ready(&tech_pvt->transports[LDL_TPORT_RTP].write_codec)) {
			switch_core_codec_destroy(&tech_pvt->transports[LDL_TPORT_RTP].write_codec);
		}

		if (tech_pvt->dlsession) {
			ldl_session_destroy(&tech_pvt->dlsession);
		}

		if (tech_pvt->profile) {
			switch_thread_rwlock_unlock(tech_pvt->profile->rwlock);
			
			if (tech_pvt->profile->purge) {
				mdl_profile_t *profile = tech_pvt->profile;
				if (switch_core_hash_delete(globals.profile_hash, profile->name) == SWITCH_STATUS_SUCCESS) {
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Profile %s deleted successfully\n", profile->name);
				}
			}
		}
	}

	return SWITCH_STATUS_SUCCESS;
}


static switch_status_t channel_on_hangup(switch_core_session_t *session)
{
	switch_channel_t *channel = switch_core_session_get_channel(session);
	struct private_object *tech_pvt = NULL;

	tech_pvt = switch_core_session_get_private(session);
	switch_assert(tech_pvt != NULL);

	if (tech_pvt->profile->ip && tech_pvt->transports[LDL_TPORT_RTP].local_port) {
		switch_rtp_release_port(tech_pvt->profile->ip, tech_pvt->transports[LDL_TPORT_RTP].local_port);
	}

	if (tech_pvt->profile->ip && tech_pvt->transports[LDL_TPORT_VIDEO_RTP].local_port) {
		switch_rtp_release_port(tech_pvt->profile->ip, tech_pvt->transports[LDL_TPORT_VIDEO_RTP].local_port);
	}

	switch_clear_flag_locked(tech_pvt, TFLAG_IO);
	switch_clear_flag_locked(tech_pvt, TFLAG_VOICE);
	switch_set_flag_locked(tech_pvt, TFLAG_BYE);

	/* Dunno why, but if googletalk calls us for the first time, as soon as the call ends
	   they think we are offline for no reason so we send this presence packet to stop it from happening
	   We should find out why.....
	 */
	if ((tech_pvt->profile->user_flags & LDL_FLAG_COMPONENT) && is_special(tech_pvt->them)) {
		ldl_handle_send_presence(tech_pvt->profile->handle, tech_pvt->them, tech_pvt->us, NULL, NULL, "Click To Call", tech_pvt->profile->avatar);
	}

	if (!switch_test_flag(tech_pvt, TFLAG_TERM) && tech_pvt->dlsession) {
		ldl_session_terminate(tech_pvt->dlsession);
		switch_set_flag_locked(tech_pvt, TFLAG_TERM);
	}
	

	switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "%s CHANNEL HANGUP\n", switch_channel_get_name(channel));

	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t channel_kill_channel(switch_core_session_t *session, int sig)
{
	switch_channel_t *channel = switch_core_session_get_channel(session);
	struct private_object *tech_pvt = NULL;

	if (!(tech_pvt = switch_core_session_get_private(session))) {
		return SWITCH_STATUS_SUCCESS;
	}

	switch (sig) {
	case SWITCH_SIG_KILL:
		switch_clear_flag_locked(tech_pvt, TFLAG_IO);
		switch_clear_flag_locked(tech_pvt, TFLAG_VOICE);
		switch_set_flag_locked(tech_pvt, TFLAG_BYE);

		if (switch_rtp_ready(tech_pvt->transports[LDL_TPORT_RTP].rtp_session)) {
			switch_rtp_kill_socket(tech_pvt->transports[LDL_TPORT_RTP].rtp_session);
		}
		break;
	case SWITCH_SIG_BREAK:
		if (switch_rtp_ready(tech_pvt->transports[LDL_TPORT_RTP].rtp_session)) {
			switch_rtp_set_flag(tech_pvt->transports[LDL_TPORT_RTP].rtp_session, SWITCH_RTP_FLAG_BREAK);
		}
		break;
	}

	switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "%s CHANNEL KILL\n", switch_channel_get_name(channel));


	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t channel_on_exchange_media(switch_core_session_t *session)
{
	switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "CHANNEL LOOPBACK\n");
	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t channel_on_soft_execute(switch_core_session_t *session)
{
	switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "CHANNEL TRANSMIT\n");
	return SWITCH_STATUS_SUCCESS;
}


static switch_status_t channel_send_dtmf(switch_core_session_t *session, const switch_dtmf_t *dtmf)
{
	struct private_object *tech_pvt = NULL;

	tech_pvt = switch_core_session_get_private(session);
	switch_assert(tech_pvt != NULL);

	switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "DTMF [%c]\n", dtmf->digit);

	return switch_rtp_queue_rfc2833(tech_pvt->transports[LDL_TPORT_RTP].rtp_session, dtmf);

}

static switch_status_t channel_read_frame(switch_core_session_t *session, switch_frame_t **frame, switch_io_flag_t flags, int stream_id)
{
	struct private_object *tech_pvt = NULL;
	switch_channel_t *channel = switch_core_session_get_channel(session);
	//int payload = 0;

	tech_pvt = (struct private_object *) switch_core_session_get_private(session);
	switch_assert(tech_pvt != NULL);

	while (!(tech_pvt->transports[LDL_TPORT_RTP].read_codec.implementation && switch_rtp_ready(tech_pvt->transports[LDL_TPORT_RTP].rtp_session))) {
		if (switch_channel_ready(channel)) {
			switch_yield(10000);
		} else {
			return SWITCH_STATUS_GENERR;
		}
	}


	tech_pvt->transports[LDL_TPORT_RTP].read_frame.datalen = 0;
	switch_set_flag_locked(tech_pvt, TFLAG_READING);

#if 0
	if (tech_pvt->last_read) {
		elapsed = (unsigned int) ((switch_micro_time_now() - tech_pvt->last_read) / 1000);
		if (elapsed > 60000) {
			return SWITCH_STATUS_TIMEOUT;
		}
	}
#endif


	if (switch_test_flag(tech_pvt, TFLAG_IO)) {
		//switch_status_t status;
		
		switch_assert(tech_pvt->transports[LDL_TPORT_RTP].rtp_session != NULL);
		tech_pvt->transports[LDL_TPORT_RTP].read_frame.datalen = 0;


		while (switch_test_flag(tech_pvt, TFLAG_IO) && tech_pvt->transports[LDL_TPORT_RTP].read_frame.datalen == 0) {
			tech_pvt->transports[LDL_TPORT_RTP].read_frame.flags = SFF_NONE;

			switch_rtp_zerocopy_read_frame(tech_pvt->transports[LDL_TPORT_RTP].rtp_session, &tech_pvt->transports[LDL_TPORT_RTP].read_frame, flags);

			tech_pvt->read_count++;
#if 0
			if (tech_pvt->read_count == 1 && !switch_test_flag(tech_pvt, TFLAG_OUTBOUND)) {
				setup_codecs(tech_pvt);
			}
#endif

			//payload = tech_pvt->transports[LDL_TPORT_RTP].read_frame.payload;

#if 0
			elapsed = (unsigned int) ((switch_micro_time_now() - started) / 1000);

			if (timeout > -1) {
				if (elapsed >= (unsigned int) timeout) {
					return SWITCH_STATUS_BREAK;
				}
			}

			elapsed = (unsigned int) ((switch_micro_time_now() - last_act) / 1000);
			if (elapsed >= hard_timeout) {
				return SWITCH_STATUS_BREAK;
			}
#endif
			if (switch_rtp_has_dtmf(tech_pvt->transports[LDL_TPORT_RTP].rtp_session)) {
				switch_dtmf_t dtmf = { 0 };
				switch_rtp_dequeue_dtmf(tech_pvt->transports[LDL_TPORT_RTP].rtp_session, &dtmf);
				switch_channel_queue_dtmf(channel, &dtmf);
			}


			if (tech_pvt->transports[LDL_TPORT_RTP].read_frame.datalen > 0) {
				size_t bytes = 0;
				int frames = 1;

				if (!switch_test_flag((&tech_pvt->transports[LDL_TPORT_RTP].read_frame), SFF_CNG)) {
					if ((bytes = tech_pvt->transports[LDL_TPORT_RTP].read_codec.implementation->encoded_bytes_per_packet)) {
						frames = (tech_pvt->transports[LDL_TPORT_RTP].read_frame.datalen / bytes);
					}
					tech_pvt->transports[LDL_TPORT_RTP].read_frame.samples = (int) (frames * tech_pvt->transports[LDL_TPORT_RTP].read_codec.implementation->samples_per_packet);
				}
				break;
			}
		}
	}

	switch_clear_flag_locked(tech_pvt, TFLAG_READING);

	if (tech_pvt->transports[LDL_TPORT_RTP].read_frame.datalen == 0) {
		switch_set_flag((&tech_pvt->transports[LDL_TPORT_RTP].read_frame), SFF_CNG);
		tech_pvt->transports[LDL_TPORT_RTP].read_frame.datalen = 2;
	}

	*frame = &tech_pvt->transports[LDL_TPORT_RTP].read_frame;

	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t channel_write_frame(switch_core_session_t *session, switch_frame_t *frame, switch_io_flag_t flags, int stream_id)
{
	struct private_object *tech_pvt;
	switch_channel_t *channel = switch_core_session_get_channel(session);
	switch_status_t status = SWITCH_STATUS_SUCCESS;
	int bytes = 0, samples = 0, frames = 0;

	tech_pvt = (struct private_object *) switch_core_session_get_private(session);
	switch_assert(tech_pvt != NULL);

	while (!(tech_pvt->transports[LDL_TPORT_RTP].read_codec.implementation && switch_rtp_ready(tech_pvt->transports[LDL_TPORT_RTP].rtp_session))) {
		if (switch_channel_ready(channel)) {
			switch_yield(10000);
		} else {
			return SWITCH_STATUS_GENERR;
		}
	}

	if (!switch_core_codec_ready(&tech_pvt->transports[LDL_TPORT_RTP].read_codec) || !tech_pvt->transports[LDL_TPORT_RTP].read_codec.implementation) {
		return SWITCH_STATUS_GENERR;
	}

	if (!switch_test_flag(tech_pvt, TFLAG_IO)) {
		return SWITCH_STATUS_SUCCESS;
	}

	switch_set_flag_locked(tech_pvt, TFLAG_WRITING);

	if (!switch_test_flag(frame, SFF_CNG)) {
		if (tech_pvt->transports[LDL_TPORT_RTP].read_codec.implementation->encoded_bytes_per_packet) {
			bytes = tech_pvt->transports[LDL_TPORT_RTP].read_codec.implementation->encoded_bytes_per_packet;
			frames = ((int) frame->datalen / bytes);
		} else
			frames = 1;

		samples = frames * tech_pvt->transports[LDL_TPORT_RTP].read_codec.implementation->samples_per_packet;
	}
#if 0
	printf("%s %s->%s send %d bytes %d samples in %d frames ts=%d\n",
		   switch_channel_get_name(channel),
		   tech_pvt->local_sdp_audio_ip, tech_pvt->remote_sdp_audio_ip, frame->datalen, samples, frames, tech_pvt->timestamp_send);
#endif

	tech_pvt->timestamp_send += samples;
	//switch_rtp_write_frame(tech_pvt->transports[LDL_TPORT_RTP].rtp_session, frame, tech_pvt->timestamp_send);
	if (switch_rtp_write_frame(tech_pvt->transports[LDL_TPORT_RTP].rtp_session, frame) < 0) {
		status = SWITCH_STATUS_GENERR;
	}

	switch_clear_flag_locked(tech_pvt, TFLAG_WRITING);
	return status;
}


static switch_status_t channel_read_video_frame(switch_core_session_t *session, switch_frame_t **frame, switch_io_flag_t flags, int stream_id)
{
	struct private_object *tech_pvt = NULL;
	switch_channel_t *channel = switch_core_session_get_channel(session);
	//int payload = 0;
	//switch_status_t status;

	tech_pvt = (struct private_object *) switch_core_session_get_private(session);
	switch_assert(tech_pvt != NULL);
	
	if (!switch_test_flag(tech_pvt, TFLAG_IO)) {
		return SWITCH_STATUS_GENERR;
	}

	while (!(tech_pvt->transports[LDL_TPORT_VIDEO_RTP].read_codec.implementation && switch_rtp_ready(tech_pvt->transports[LDL_TPORT_VIDEO_RTP].rtp_session))) {
		if (switch_channel_ready(channel)) {
			switch_yield(10000);
		} else {
			return SWITCH_STATUS_GENERR;
		}
	}
	
	tech_pvt->transports[LDL_TPORT_VIDEO_RTP].read_frame.datalen = 0;
	
	while (switch_test_flag(tech_pvt, TFLAG_IO) && tech_pvt->transports[LDL_TPORT_VIDEO_RTP].read_frame.datalen == 0) {
		tech_pvt->transports[LDL_TPORT_VIDEO_RTP].read_frame.flags = SFF_NONE;
		switch_rtp_zerocopy_read_frame(tech_pvt->transports[LDL_TPORT_VIDEO_RTP].rtp_session, &tech_pvt->transports[LDL_TPORT_VIDEO_RTP].read_frame, flags);
	}

	if (tech_pvt->transports[LDL_TPORT_VIDEO_RTP].read_frame.datalen == 0) {
		switch_set_flag((&tech_pvt->transports[LDL_TPORT_RTP].read_frame), SFF_CNG);
		tech_pvt->transports[LDL_TPORT_VIDEO_RTP].read_frame.datalen = 2;
	}

	*frame = &tech_pvt->transports[LDL_TPORT_VIDEO_RTP].read_frame;
	
	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t channel_write_video_frame(switch_core_session_t *session, switch_frame_t *frame, switch_io_flag_t flags, int stream_id)
{
	struct private_object *tech_pvt = (struct private_object *)switch_core_session_get_private(session);
	switch_channel_t *channel = switch_core_session_get_channel(session);
	int wrote = 0;

	switch_assert(tech_pvt != NULL);

	while (!(tech_pvt->transports[LDL_TPORT_VIDEO_RTP].read_codec.implementation && switch_rtp_ready(tech_pvt->transports[LDL_TPORT_VIDEO_RTP].rtp_session))) {
		if (switch_channel_ready(channel)) {
			switch_yield(10000);
		} else {
			return SWITCH_STATUS_GENERR;
		}
	}

	if (!switch_test_flag(tech_pvt, TFLAG_IO)) {
		return SWITCH_STATUS_SUCCESS;
	}

	if (!switch_test_flag(frame, SFF_CNG)) {
		wrote = switch_rtp_write_frame(tech_pvt->transports[LDL_TPORT_VIDEO_RTP].rtp_session, frame);
	}

	return wrote > 0 ? SWITCH_STATUS_SUCCESS : SWITCH_STATUS_GENERR;
}

static switch_status_t channel_answer_channel(switch_core_session_t *session)
{
	struct private_object *tech_pvt;

	tech_pvt = switch_core_session_get_private(session);
	switch_assert(tech_pvt != NULL);

	if (!switch_test_flag(tech_pvt, TFLAG_OUTBOUND)) {
		switch_set_flag_locked(tech_pvt, TFLAG_ANSWER);
	}

	return SWITCH_STATUS_SUCCESS;
}


static switch_status_t channel_receive_message(switch_core_session_t *session, switch_core_session_message_t *msg)
{
	struct private_object *tech_pvt;

	tech_pvt = switch_core_session_get_private(session);
	switch_assert(tech_pvt != NULL);

	switch (msg->message_id) {
	case SWITCH_MESSAGE_INDICATE_ANSWER:
		channel_answer_channel(session);
		break;
	case SWITCH_MESSAGE_INDICATE_BRIDGE:
		rtp_flush_read_buffer(tech_pvt->transports[LDL_TPORT_RTP].rtp_session, SWITCH_RTP_FLUSH_STICK);
		break;
	case SWITCH_MESSAGE_INDICATE_UNBRIDGE:
		rtp_flush_read_buffer(tech_pvt->transports[LDL_TPORT_RTP].rtp_session, SWITCH_RTP_FLUSH_UNSTICK);
		break;
	case SWITCH_MESSAGE_INDICATE_STUN_ERROR:
		//switch_channel_hangup(switch_core_session_get_channel(session), SWITCH_CAUSE_NORMAL_CLEARING);
		break;
	default:
		break;
	}

	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t channel_receive_event(switch_core_session_t *session, switch_event_t *event)
{
	struct private_object *tech_pvt;
	char *subject, *body;

	tech_pvt = switch_core_session_get_private(session);
	switch_assert(tech_pvt != NULL);


	if (!(body = switch_event_get_body(event))) {
		body = "";
	}

	if (!(subject = switch_event_get_header(event, "subject"))) {
		subject = "None";
	}

	ldl_session_send_msg(tech_pvt->dlsession, subject, body);

	return SWITCH_STATUS_SUCCESS;
}

switch_state_handler_table_t dingaling_event_handlers = {
	/*.on_init */ channel_on_init,
	/*.on_routing */ channel_on_routing,
	/*.on_execute */ channel_on_execute,
	/*.on_hangup */ channel_on_hangup,
	/*.on_exchange_media */ channel_on_exchange_media,
	/*.on_soft_execute */ channel_on_soft_execute,
	/*.on_consume_media */ NULL,
	/*.on_hibernate */ NULL,
	/*.on_reset */ NULL,
	/*.on_park */ NULL,
	/*.on_reporting */ NULL,
	/*.on_destroy */ channel_on_destroy
};

switch_io_routines_t dingaling_io_routines = {
	/*.outgoing_channel */ channel_outgoing_channel,
	/*.transports[LDL_TPORT_RTP].read_frame */ channel_read_frame,
	/*.write_frame */ channel_write_frame,
	/*.kill_channel */ channel_kill_channel,
	/*.send_dtmf */ channel_send_dtmf,
	/*.receive_message */ channel_receive_message,
	/*.receive_event */ channel_receive_event,
	/*.state_change */ NULL,
	/*.read_video_frame */ channel_read_video_frame,
	/*.write_video_frame */ channel_write_video_frame
};



/* Make sure when you have 2 sessions in the same scope that you pass the appropriate one to the routines
   that allocate memory or you will have 1 channel with memory allocated from another channel's pool!
*/
static switch_call_cause_t channel_outgoing_channel(switch_core_session_t *session, switch_event_t *var_event,
													switch_caller_profile_t *outbound_profile,
													switch_core_session_t **new_session, switch_memory_pool_t **pool, switch_originate_flag_t flags,
													switch_call_cause_t *cancel_cause)
{
	if ((*new_session = switch_core_session_request(dingaling_endpoint_interface, SWITCH_CALL_DIRECTION_OUTBOUND, flags, pool)) != 0) {
		struct private_object *tech_pvt;
		switch_channel_t *channel;
		switch_caller_profile_t *caller_profile = NULL;
		mdl_profile_t *mdl_profile = NULL;
		ldl_session_t *dlsession = NULL;
		char *profile_name;
		char *callto;
		char idbuf[1024];
		char *full_id = NULL;
		char sess_id[11] = "";
		char *dnis = NULL;
		char workspace[1024] = "";
		char *p, *u, ubuf[512] = "", *user = NULL, *f_cid_msg = NULL;
		const char *cid_msg = NULL;
		ldl_user_flag_t flags = LDL_FLAG_OUTBOUND;
		const char *var;

		switch_copy_string(workspace, outbound_profile->destination_number, sizeof(workspace));
		profile_name = workspace;

		if ((callto = strchr(profile_name, '/'))) {
			*callto++ = '\0';
		} else {
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(*new_session), SWITCH_LOG_DEBUG, "Invalid URL!\n");
			terminate_session(new_session, __LINE__, SWITCH_CAUSE_DESTINATION_OUT_OF_ORDER);
			return SWITCH_CAUSE_INVALID_NUMBER_FORMAT;
		}

		if ((dnis = strchr(callto, ':'))) {
			*dnis++ = '\0';
		}

		for (p = callto; p && *p; p++) {
			*p = (char) tolower(*p);
		}

		if ((p = strchr(profile_name, '@'))) {
			*p++ = '\0';
			u = profile_name;
			profile_name = p;
			switch_snprintf(ubuf, sizeof(ubuf), "%s@%s/talk", u, profile_name);
			user = ubuf;
		}

		if ((mdl_profile = switch_core_hash_find(globals.profile_hash, profile_name))) {
			if (!(mdl_profile->user_flags & LDL_FLAG_COMPONENT)) {
				user = ldl_handle_get_login(mdl_profile->handle);
			} else {
				if (!user) {
					const char *id_num;

					if (!(id_num = outbound_profile->caller_id_number)) {
						if (!(id_num = outbound_profile->caller_id_name)) {
							id_num = "nobody";
						}
					}

					if (strchr(id_num, '@')) {
						switch_snprintf(ubuf, sizeof(ubuf), "%s/talk", id_num);
						user = ubuf;
					} else {
						switch_snprintf(ubuf, sizeof(ubuf), "%s@%s/talk", id_num, profile_name);
						user = ubuf;
					}
				}
			}

			if (mdl_profile->purge) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Profile '%s' is marked for deletion, disallowing outgoing call\n",
								  mdl_profile->name);
				terminate_session(new_session, __LINE__, SWITCH_CAUSE_NORMAL_UNSPECIFIED);
				return SWITCH_CAUSE_NORMAL_UNSPECIFIED;
			}

			if (switch_thread_rwlock_tryrdlock(mdl_profile->rwlock) != SWITCH_STATUS_SUCCESS) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Can't do read lock on profile '%s'\n", mdl_profile->name);
				terminate_session(new_session, __LINE__, SWITCH_CAUSE_NORMAL_UNSPECIFIED);
				return SWITCH_CAUSE_NORMAL_UNSPECIFIED;
			}

			


			if (!ldl_handle_ready(mdl_profile->handle)) {
				switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(*new_session), SWITCH_LOG_DEBUG, "Doh! we are not logged in yet!\n");
				terminate_session(new_session, __LINE__, SWITCH_CAUSE_DESTINATION_OUT_OF_ORDER);
				return SWITCH_CAUSE_DESTINATION_OUT_OF_ORDER;
			}
			if (switch_stristr("voice.google.com", callto)) {
				full_id = callto;
				flags |= LDL_FLAG_GATEWAY;
			} else if (!(full_id = ldl_handle_probe(mdl_profile->handle, callto, user, idbuf, sizeof(idbuf)))) {
				switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(*new_session), SWITCH_LOG_DEBUG, "Unknown Recipient!\n");
				terminate_session(new_session, __LINE__, SWITCH_CAUSE_NO_USER_RESPONSE);
				return SWITCH_CAUSE_NO_USER_RESPONSE;
			}
		} else {
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(*new_session), SWITCH_LOG_DEBUG, "Unknown Profile!\n");
			terminate_session(new_session, __LINE__, SWITCH_CAUSE_DESTINATION_OUT_OF_ORDER);
			return SWITCH_CAUSE_DESTINATION_OUT_OF_ORDER;
		}


		switch_core_session_add_stream(*new_session, NULL);
		if ((tech_pvt = (struct private_object *) switch_core_session_alloc(*new_session, sizeof(struct private_object))) != 0) {
			memset(tech_pvt, 0, sizeof(*tech_pvt));
			tech_pvt->profile = mdl_profile;
			switch_mutex_init(&tech_pvt->flag_mutex, SWITCH_MUTEX_NESTED, switch_core_session_get_pool(*new_session));
			tech_pvt->flags |= globals.flags;
			tech_pvt->flags |= mdl_profile->flags;
			channel = switch_core_session_get_channel(*new_session);
			switch_core_session_set_private(*new_session, tech_pvt);
			tech_pvt->session = *new_session;
			tech_pvt->channel = switch_core_session_get_channel(tech_pvt->session);
			tech_pvt->transports[LDL_TPORT_RTP].codec_index = -1;
			tech_pvt->transports[LDL_TPORT_VIDEO_RTP].codec_index = -1;

			switch_set_flag(tech_pvt, TFLAG_SECURE);
			mdl_build_crypto(tech_pvt, LDL_TPORT_RTP, 1, AES_CM_128_HMAC_SHA1_80, SWITCH_RTP_CRYPTO_SEND);
			mdl_build_crypto(tech_pvt, LDL_TPORT_VIDEO_RTP, 1, AES_CM_128_HMAC_SHA1_80, SWITCH_RTP_CRYPTO_SEND);



			if (!(tech_pvt->transports[LDL_TPORT_RTP].local_port = switch_rtp_request_port(mdl_profile->ip))) {
				switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(*new_session), SWITCH_LOG_CRIT, "No RTP port available!\n");
				terminate_session(new_session, __LINE__, SWITCH_CAUSE_DESTINATION_OUT_OF_ORDER);
				return SWITCH_CAUSE_DESTINATION_OUT_OF_ORDER;
			}
			tech_pvt->transports[LDL_TPORT_RTP].adv_local_port = tech_pvt->transports[LDL_TPORT_RTP].local_port;
			tech_pvt->transports[LDL_TPORT_RTCP].adv_local_port = tech_pvt->transports[LDL_TPORT_RTP].local_port + 1;
			if (!(tech_pvt->transports[LDL_TPORT_VIDEO_RTP].local_port = switch_rtp_request_port(mdl_profile->ip))) {
				switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(*new_session), SWITCH_LOG_CRIT, "No RTP port available!\n");
				terminate_session(new_session, __LINE__, SWITCH_CAUSE_DESTINATION_OUT_OF_ORDER);
				return SWITCH_CAUSE_DESTINATION_OUT_OF_ORDER;
			}
			tech_pvt->transports[LDL_TPORT_VIDEO_RTP].adv_local_port = tech_pvt->transports[LDL_TPORT_VIDEO_RTP].local_port;
			tech_pvt->transports[LDL_TPORT_VIDEO_RTCP].adv_local_port = tech_pvt->transports[LDL_TPORT_VIDEO_RTP].local_port + 1;

			

			tech_pvt->recip = switch_core_session_strdup(*new_session, full_id);
			if (dnis) {
				tech_pvt->dnis = switch_core_session_strdup(*new_session, dnis);
			}
		} else {
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(*new_session), SWITCH_LOG_CRIT, "Hey where is my memory pool?\n");
			terminate_session(new_session, __LINE__, SWITCH_CAUSE_DESTINATION_OUT_OF_ORDER);
			return SWITCH_CAUSE_DESTINATION_OUT_OF_ORDER;
		}

		if (outbound_profile) {
			char name[128];

			switch_snprintf(name, sizeof(name), "dingaling/%s", outbound_profile->destination_number);
			switch_channel_set_name(channel, name);

			caller_profile = switch_caller_profile_clone(*new_session, outbound_profile);
			switch_channel_set_caller_profile(channel, caller_profile);
			tech_pvt->caller_profile = caller_profile;
		} else {
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(*new_session), SWITCH_LOG_DEBUG, "Doh! no caller profile\n");
			terminate_session(new_session, __LINE__, SWITCH_CAUSE_DESTINATION_OUT_OF_ORDER);
			return SWITCH_CAUSE_DESTINATION_OUT_OF_ORDER;
		}

		switch_set_flag_locked(tech_pvt, TFLAG_OUTBOUND);

		switch_stun_random_string(sess_id, 10, "0123456789");
		tech_pvt->us = switch_core_session_strdup(*new_session, user);
		tech_pvt->them = switch_core_session_strdup(*new_session, full_id);
		ldl_session_create(&dlsession, mdl_profile->handle, sess_id, full_id, user, flags);

		if (session) {
			switch_channel_t *calling_channel = switch_core_session_get_channel(session);
			cid_msg = switch_channel_get_variable(calling_channel, "dl_cid_msg");
		}

		if (!cid_msg) {
			f_cid_msg = switch_mprintf("Incoming Call From %s %s\n", outbound_profile->caller_id_name, outbound_profile->caller_id_number);
			cid_msg = f_cid_msg;
		}

		if ((flags & LDL_FLAG_GATEWAY)) {
			cid_msg = NULL;
		}

		if (cid_msg) {
			char *them;
			them = strdup(tech_pvt->them);
			if (them) {
				char *ptr;
				if ((ptr = strchr(them, '/'))) {
					*ptr = '\0';
				}
				ldl_handle_send_msg(mdl_profile->handle, tech_pvt->us, them, "", switch_str_nil(cid_msg));
			}
			switch_safe_free(them);
		}
		switch_safe_free(f_cid_msg);


		ldl_session_set_private(dlsession, *new_session);
		ldl_session_set_value(dlsession, "dnis", dnis);
		ldl_session_set_value(dlsession, "caller_id_name", outbound_profile->caller_id_name);
		ldl_session_set_value(dlsession, "caller_id_number", outbound_profile->caller_id_number);
		tech_pvt->dlsession = dlsession;

		if ((var = switch_event_get_header(var_event, "absolute_codec_string"))) {
			switch_channel_set_variable(channel, "absolute_codec_string", var);
		}

		if (!get_codecs(tech_pvt)) {
			terminate_session(new_session, __LINE__, SWITCH_CAUSE_BEARERCAPABILITY_NOTAVAIL);
			return SWITCH_CAUSE_BEARERCAPABILITY_NOTAVAIL;
		}
		switch_channel_set_state(channel, CS_INIT);
		return SWITCH_CAUSE_SUCCESS;

	}

	return SWITCH_CAUSE_DESTINATION_OUT_OF_ORDER;

}

static switch_status_t list_profiles(const char *line, const char *cursor, switch_console_callback_match_t **matches)
{
	mdl_profile_t *profile = NULL;
	switch_hash_index_t *hi;
	void *val;
	const void *vvar;
	switch_console_callback_match_t *my_matches = NULL;
	switch_status_t status = SWITCH_STATUS_FALSE;

	for (hi = switch_hash_first(NULL, globals.profile_hash); hi; hi = switch_hash_next(hi)) {
		switch_hash_this(hi, &vvar, NULL, &val);
		profile = (mdl_profile_t *) val;
		if (!strncmp("dl_logout", line, 9)) {
			if (profile->handle) {
				switch_console_push_match(&my_matches, profile->name);
			}
		} else if (!strncmp("dl_login", line, 8)) {
			if (!switch_test_flag(profile, TFLAG_IO)) {
				char *profile_name = switch_mprintf("profile=%s", profile->name);
				switch_console_push_match(&my_matches, profile_name);
				free(profile_name);
			}
		} else if (!strncmp("dl_pres", line, 7)) {
			if (profile->user_flags & LDL_FLAG_COMPONENT) {
				switch_console_push_match(&my_matches, profile->name);
			}
		} else {
			switch_console_push_match(&my_matches, profile->name);
		} 
	}

	if (my_matches) {
		*matches = my_matches;
		status = SWITCH_STATUS_SUCCESS;
	}

	return status;
}

SWITCH_MODULE_LOAD_FUNCTION(mod_dingaling_load)
{
	switch_chat_interface_t *chat_interface;
	switch_api_interface_t *api_interface;

	module_pool = pool;

	memset(&globals, 0, sizeof(globals));

	load_config();

	if (switch_event_reserve_subclass(DL_EVENT_LOGIN_SUCCESS) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Couldn't register subclass %s!\n", DL_EVENT_LOGIN_SUCCESS);
		return SWITCH_STATUS_GENERR;
	}

	if (switch_event_reserve_subclass(DL_EVENT_LOGIN_FAILURE) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Couldn't register subclass %s!\n", DL_EVENT_LOGIN_FAILURE);
		return SWITCH_STATUS_GENERR;
	}

	if (switch_event_reserve_subclass(DL_EVENT_CONNECTED) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Couldn't register subclass %s!\n", DL_EVENT_CONNECTED);
		return SWITCH_STATUS_GENERR;
	}

	if (switch_event_bind_removable(modname, SWITCH_EVENT_PRESENCE_IN, SWITCH_EVENT_SUBCLASS_ANY, pres_event_handler, NULL, &globals.in_node) !=
		SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Couldn't bind!\n");
		return SWITCH_STATUS_GENERR;
	}

	if (switch_event_bind_removable(modname, SWITCH_EVENT_PRESENCE_PROBE, SWITCH_EVENT_SUBCLASS_ANY, pres_event_handler, NULL, &globals.probe_node) !=
		SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Couldn't bind!\n");
		return SWITCH_STATUS_GENERR;
	}

	if (switch_event_bind_removable(modname, SWITCH_EVENT_PRESENCE_OUT, SWITCH_EVENT_SUBCLASS_ANY, pres_event_handler, NULL, &globals.out_node) !=
		SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Couldn't bind!\n");
		return SWITCH_STATUS_GENERR;
	}

	if (switch_event_bind_removable(modname, SWITCH_EVENT_ROSTER, SWITCH_EVENT_SUBCLASS_ANY, roster_event_handler, NULL, &globals.roster_node)
		!= SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Couldn't bind!\n");
		return SWITCH_STATUS_GENERR;
	}

	if (switch_event_bind(modname, SWITCH_EVENT_TRAP, SWITCH_EVENT_SUBCLASS_ANY, ipchanged_event_handler, NULL) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Couldn't bind!\n");
		return SWITCH_STATUS_GENERR;
	}


	/* connect my internal structure to the blank pointer passed to me */
	*module_interface = switch_loadable_module_create_module_interface(pool, modname);
	dingaling_endpoint_interface = switch_loadable_module_create_interface(*module_interface, SWITCH_ENDPOINT_INTERFACE);
	dingaling_endpoint_interface->interface_name = "dingaling";
	dingaling_endpoint_interface->io_routines = &dingaling_io_routines;
	dingaling_endpoint_interface->state_handler = &dingaling_event_handlers;

#define PRES_SYNTAX "dl_pres <profile_name>"
#define LOGOUT_SYNTAX "dl_logout <profile_name>"
#define LOGIN_SYNTAX "Existing Profile:\ndl_login profile=<profile_name>\nDynamic Profile:\ndl_login var1=val1;var2=val2;varN=valN\n"
#define DEBUG_SYNTAX "dl_debug [true|false]"
#define DINGALING_SYNTAX "dingaling [status|reload]"

	SWITCH_ADD_API(api_interface, "dl_debug", "DingaLing Debug", dl_debug, DEBUG_SYNTAX);
	SWITCH_ADD_API(api_interface, "dl_pres", "DingaLing Presence", dl_pres, PRES_SYNTAX);
	SWITCH_ADD_API(api_interface, "dl_logout", "DingaLing Logout", dl_logout, LOGOUT_SYNTAX);
	SWITCH_ADD_API(api_interface, "dl_login", "DingaLing Login", dl_login, LOGIN_SYNTAX);
	SWITCH_ADD_API(api_interface, "dingaling", "DingaLing Menu", dingaling, DINGALING_SYNTAX);
	SWITCH_ADD_CHAT(chat_interface, MDL_CHAT_PROTO, chat_send);

	switch_console_set_complete("add dl_debug ::[true:false");
	switch_console_set_complete("add dl_pres ::dingaling::list_profiles");
	switch_console_set_complete("add dl_logout ::dingaling::list_profiles");
	switch_console_set_complete("add dl_login ::dingaling::list_profiles");
	switch_console_set_complete("add dl_login login=");
	switch_console_set_complete("add dingaling status");
	switch_console_set_complete("add dingaling reload");
	switch_console_add_complete_func("::dingaling::list_profiles", list_profiles);

	/* indicate that the module should continue to be loaded */
	return SWITCH_STATUS_SUCCESS;
}

static ldl_status handle_loop(ldl_handle_t *handle)
{
	if (!globals.running) {
		return LDL_STATUS_FALSE;
	}
	return LDL_STATUS_SUCCESS;
}

static switch_status_t init_profile(mdl_profile_t *profile, uint8_t login)
{
	ldl_handle_t *handle;

	if (!profile) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Invalid Profile\n");
		return SWITCH_STATUS_FALSE;
	}

	if (!(profile->login && profile->password && profile->dialplan && profile->message && profile->ip && profile->name && profile->exten)) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG,
						  "Invalid Profile\n" "login[%s]\n" "pass[%s]\n" "dialplan[%s]\n"
						  "message[%s]\n" "rtp-ip[%s]\n" "name[%s]\n" "exten[%s]\n",
						  switch_str_nil(profile->login),
						  switch_str_nil(profile->password),
						  switch_str_nil(profile->dialplan),
						  switch_str_nil(profile->message), switch_str_nil(profile->ip), switch_str_nil(profile->name), switch_str_nil(profile->exten));

		return SWITCH_STATUS_FALSE;
	}

	if (switch_test_flag(profile, TFLAG_TIMER) && !profile->timer_name) {
		profile->timer_name = switch_core_strdup(module_pool, "soft");
	}

	if (!login) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Created Profile for %s@%s\n", profile->login, profile->dialplan);
		switch_core_hash_insert(globals.profile_hash, profile->name, profile);
		return SWITCH_STATUS_SUCCESS;
	}

	if (ldl_handle_init(&handle,
						profile->login,
						profile->password,
						profile->server,
						profile->user_flags, profile->message, handle_loop, handle_signalling, handle_response, profile) == LDL_STATUS_SUCCESS) {
		profile->purge = SWITCH_FALSE;
		switch_thread_rwlock_create(&profile->rwlock, module_pool);

		profile->handle = handle;
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Started Thread for %s@%s\n", profile->login, profile->dialplan);
		switch_core_hash_insert(globals.profile_hash, profile->name, profile);
		handle_thread_launch(handle);
	}

	return SWITCH_STATUS_SUCCESS;
}

SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_dingaling_shutdown)
{
	sign_off();

	if (globals.running) {
		int x = 0;
		globals.running = 0;
		ldl_global_terminate();
		while (globals.handles > 0) {
			switch_yield(100000);
			x++;
			if (x > 100) {
				break;
			}
		}

		if (globals.init) {
			ldl_global_destroy();
		}
	}

	switch_event_free_subclass(DL_EVENT_LOGIN_SUCCESS);
	switch_event_free_subclass(DL_EVENT_LOGIN_FAILURE);
	switch_event_free_subclass(DL_EVENT_CONNECTED);
	switch_event_unbind(&globals.in_node);
	switch_event_unbind(&globals.probe_node);
	switch_event_unbind(&globals.out_node);
	switch_event_unbind(&globals.roster_node);
	switch_event_unbind_callback(ipchanged_event_handler);


	switch_core_hash_destroy(&globals.profile_hash);

	switch_safe_free(globals.dialplan);
	switch_safe_free(globals.codec_string);
	switch_safe_free(globals.codec_rates_string);

	return SWITCH_STATUS_SUCCESS;
}


static void set_profile_val(mdl_profile_t *profile, char *var, char *val)
{
	if (!var)
		return;

	if (!strcasecmp(var, "login")) {
		profile->login = switch_core_strdup(module_pool, val);
	} else if (!strcasecmp(var, "password")) {
		profile->password = switch_core_strdup(module_pool, val);
	} else if (!strcasecmp(var, "avatar")) {
		profile->avatar = switch_core_strdup(module_pool, val);
	} else if (!strcasecmp(var, "odbc-dsn") && !zstr(val)) {
		profile->odbc_dsn = switch_core_strdup(module_pool, val);
	} else if (!strcasecmp(var, "use-rtp-timer") && switch_true(val)) {
		switch_set_flag(profile, TFLAG_TIMER);
	} else if (!strcasecmp(var, "dialplan") && !zstr(val)) {
		profile->dialplan = switch_core_strdup(module_pool, val);
#ifdef AUTO_REPLY				// gotta fix looping on this
	} else if (!strcasecmp(var, "auto-reply")) {
		profile->auto_reply = switch_core_strdup(module_pool, val);
#endif
	} else if (!strcasecmp(var, "name") && !zstr(val)) {
		profile->name = switch_core_strdup(module_pool, val);
	} else if (!strcasecmp(var, "message") && !zstr(val)) {
		profile->message = switch_core_strdup(module_pool, val);
	} else if (!strcasecmp(var, "local-network-acl") && !zstr(val)) {
		profile->local_network = switch_core_strdup(module_pool, val);
	} else if (!strcasecmp(var, "rtp-ip")) {
		profile->ip = switch_core_strdup(module_pool, strcasecmp(switch_str_nil(val), "auto") ? switch_str_nil(val) : globals.guess_ip);
	} else if (!strcasecmp(var, "ext-rtp-ip")) {
		char *ip = globals.guess_ip;
		if (val && !strcasecmp(val, "auto-nat")) {
			ip = globals.auto_nat ? switch_core_get_variable_pdup("nat_public_addr", module_pool) : globals.guess_ip;
		} else if (val && !strcasecmp(val, "auto")) {
			globals.auto_nat = 0;
			ip = globals.guess_ip;
		} else {
			globals.auto_nat = 0;
			ip = zstr(val) ? globals.guess_ip : val;
		}
		profile->extip = switch_core_strdup(module_pool, ip);
	} else if (!strcasecmp(var, "server") && !zstr(val)) {
		profile->server = switch_core_strdup(module_pool, val);
	} else if (!strcasecmp(var, "rtp-timer-name") && !zstr(val)) {
		profile->timer_name = switch_core_strdup(module_pool, val);
	} else if (!strcasecmp(var, "lanaddr") && !zstr(val)) {
		profile->lanaddr = switch_core_strdup(module_pool, val);
	} else if (!strcasecmp(var, "candidate-acl")) {
		if (profile->acl_count < MAX_ACL) {
			profile->acl[profile->acl_count++] = strdup(val);
		} else {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Max acl records of %d reached\n", MAX_ACL);
		}
	} else if (!strcasecmp(var, "tls")) {
		if (switch_true(val)) {
			profile->user_flags |= LDL_FLAG_TLS;
		}
	} else if (!strcasecmp(var, "sasl")) {
		if (val && !strcasecmp(val, "plain")) {
			profile->user_flags |= LDL_FLAG_SASL_PLAIN;
		} else if (val && !strcasecmp(val, "md5")) {
			profile->user_flags |= LDL_FLAG_SASL_MD5;
		}
	} else if (!strcasecmp(var, "use-jingle") && switch_true(val)) {
		profile->user_flags |= LDL_FLAG_JINGLE;
	} else if (!strcasecmp(var, "exten") && !zstr(val)) {
		profile->exten = switch_core_strdup(module_pool, val);
	} else if (!strcasecmp(var, "context") && !zstr(val)) {
		profile->context = switch_core_strdup(module_pool, val);
	} else if (!strcasecmp(var, "auto-login") && !zstr(val)) {
		if (switch_true(val)) {
			switch_set_flag(profile, TFLAG_AUTO);
		}
	} else if (!strcasecmp(var, "vad") && val) {
		if (!strcasecmp(val, "in")) {
			switch_set_flag(profile, TFLAG_VAD_IN);
		} else if (!strcasecmp(val, "out")) {
			switch_set_flag(profile, TFLAG_VAD_OUT);
		} else if (!strcasecmp(val, "both")) {
			switch_set_flag(profile, TFLAG_VAD_IN);
			switch_set_flag(profile, TFLAG_VAD_OUT);
		} else if (!strcasecmp(val, "none")) {
			switch_set_flag(profile, TFLAG_VAD_NONE);
		} else {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Invalid option %s for VAD\n", val);
		}
	}
}

SWITCH_STANDARD_API(dl_debug)
{
	int on, cur;

	if (session) {
		return SWITCH_STATUS_FALSE;
	}

	if (cmd) {
		on = switch_true(cmd);
		cur = ldl_global_debug(on);
	} else {
		cur = ldl_global_debug(-1);
	}


	stream->write_function(stream, "DEBUG IS NOW %s\n", cur ? "ON" : "OFF");

	return SWITCH_STATUS_SUCCESS;
}

SWITCH_STANDARD_API(dl_pres)
{
	mdl_profile_t *profile;

	if (session) {
		return SWITCH_STATUS_FALSE;
	}

	if (!cmd) {
		stream->write_function(stream, "USAGE: %s\n", PRES_SYNTAX);
		return SWITCH_STATUS_SUCCESS;
	}

	if ((profile = switch_core_hash_find(globals.profile_hash, cmd))) {
		if (profile->user_flags & LDL_FLAG_COMPONENT) {
			sign_on(profile);
			stream->write_function(stream, "OK\n");
		} else {
			stream->write_function(stream, "NO PROFILE %s NOT A COMPONENT\n", cmd);
		}
	} else {
		stream->write_function(stream, "NO SUCH PROFILE %s\n", cmd);
	}

	return SWITCH_STATUS_SUCCESS;
}

SWITCH_STANDARD_API(dl_logout)
{
	mdl_profile_t *profile;

	if (session) {
		return SWITCH_STATUS_FALSE;
	}

	if (!cmd) {
		stream->write_function(stream, "USAGE: %s\n", LOGOUT_SYNTAX);
		return SWITCH_STATUS_SUCCESS;
	}

	if (((profile = switch_core_hash_find(globals.profile_hash, cmd))) && profile->handle) {
		ldl_handle_stop(profile->handle);
		stream->write_function(stream, "OK\n");
	} else if (profile) {
		stream->write_function(stream, "NOT LOGGED IN\n");
	} else {
		stream->write_function(stream, "NO SUCH PROFILE %s\n", cmd);
	}

	return SWITCH_STATUS_SUCCESS;
}

SWITCH_STANDARD_API(dingaling)
{
	char *argv[10] = { 0 };
	int argc = 0;
	void *val;
	char *myarg = NULL;
	mdl_profile_t *profile = NULL;
	switch_hash_index_t *hi;
	switch_status_t status = SWITCH_STATUS_SUCCESS;

	if (session)
		return status;

	if (zstr(cmd) || !(myarg = strdup(cmd))) {
		stream->write_function(stream, "USAGE: %s\n", DINGALING_SYNTAX);
		return SWITCH_STATUS_FALSE;
	}

	if ((argc = switch_separate_string(myarg, ' ', argv, (sizeof(argv) / sizeof(argv[0])))) != 1) {
		stream->write_function(stream, "USAGE: %s\n", DINGALING_SYNTAX);
		goto done;
	}

	if (argv[0] && !strncasecmp(argv[0], "status", 6)) {
		stream->write_function(stream, "--DingaLing status--\n");
		stream->write_function(stream, "login	|	connected\n");
		for (hi = switch_hash_first(NULL, globals.profile_hash); hi; hi = switch_hash_next(hi)) {
			switch_hash_this(hi, NULL, NULL, &val);
			profile = (mdl_profile_t *) val;
			stream->write_function(stream, "%s	|	", profile->login);
			if (profile->handle && ldl_handle_authorized(profile->handle)) {
				stream->write_function(stream, "AUTHORIZED");
			} else if (profile->handle && ldl_handle_connected(profile->handle)) {
				stream->write_function(stream, "CONNECTED");
			} else {
				stream->write_function(stream, "UNCONNECTED");
			}
			stream->write_function(stream, "\n");
		}
	} else if (argv[0] && !strncasecmp(argv[0], "reload", 6)) {
		soft_reload();
		stream->write_function(stream, "OK\n");
	} else {
		stream->write_function(stream, "USAGE: %s\n", DINGALING_SYNTAX);
	}

  done:
	switch_safe_free(myarg);
	return status;
}

SWITCH_STANDARD_API(dl_login)
{
	char *argv[20] = { 0 };
	int argc = 0;
	char *var, *val, *myarg = NULL;
	mdl_profile_t *profile = NULL;
	int x;
	switch_status_t status = SWITCH_STATUS_SUCCESS;

	if (session) {
		status = SWITCH_STATUS_FALSE;
		goto done;
	}

	if (zstr(cmd)) {
		stream->write_function(stream, "USAGE: %s\n", LOGIN_SYNTAX);
		status = SWITCH_STATUS_SUCCESS;
		goto done;
	}

	myarg = strdup(cmd);

	argc = switch_separate_string(myarg, ';', argv, (sizeof(argv) / sizeof(argv[0])));

	if (argv[0] && !strncasecmp(argv[0], "profile=", 8)) {
		char *profile_name = argv[0] + 8;
		profile = switch_core_hash_find(globals.profile_hash, profile_name);

		if (profile) {
			if (switch_test_flag(profile, TFLAG_IO)) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Profile already exists.\n");
				stream->write_function(stream, "Profile already exists\n");
				status = SWITCH_STATUS_SUCCESS;
				goto done;
			}

		}
	} else {
		profile = switch_core_alloc(module_pool, sizeof(*profile));

		for (x = 0; x < argc; x++) {
			var = argv[x];
			if (var && (val = strchr(var, '='))) {
				*val++ = '\0';
				set_profile_val(profile, var, val);
			}
		}
	}


	if (profile && init_profile(profile, 1) == SWITCH_STATUS_SUCCESS) {
		stream->write_function(stream, "OK\n");
	} else {
		stream->write_function(stream, "FAIL\n");
	}
  done:
	switch_safe_free(myarg);

	return status;
}


static switch_bool_t match_profile(mdl_profile_t *profile, mdl_profile_t *new_profile)
{
	if (profile == new_profile) {
		return SWITCH_TRUE;
	}

	if (((!new_profile->name && !profile->name) ||
		 (new_profile->name && profile->name && !strcasecmp(new_profile->name, profile->name))) &&
		((!new_profile->login && !profile->login) ||
		 (new_profile->login && profile->login && !strcasecmp(new_profile->login, profile->login))) &&
		((!new_profile->password && !profile->password) ||
		 (new_profile->password && profile->password && !strcasecmp(new_profile->password, profile->password))) &&
		((!new_profile->message && !profile->message) ||
		 (new_profile->message && profile->message && !strcasecmp(new_profile->message, profile->message))) &&
		((!new_profile->avatar && !profile->avatar) || (new_profile->avatar && profile->avatar && !strcasecmp(new_profile->avatar, profile->avatar))) &&
#ifdef AUTO_REPLY
		((!new_profile->auto_reply && !profile->auto_reply) ||
		 (new_profile->auto_reply && profile->auto_reply && !strcasecmp(new_profile->auto_reply, profile->auto_reply))) &&
#endif
		((!new_profile->dialplan && !profile->dialplan) ||
		 (new_profile->dialplan && profile->dialplan && !strcasecmp(new_profile->dialplan, profile->dialplan))) &&
		((!new_profile->local_network && !profile->local_network) ||
		 (new_profile->local_network && profile->local_network && !strcasecmp(new_profile->local_network, profile->local_network))) &&
		((!new_profile->ip && !profile->ip) ||
		 (new_profile->ip && profile->ip && !strcasecmp(new_profile->ip, profile->ip))) &&
		((!new_profile->extip && !profile->extip) ||
		 (new_profile->extip && profile->extip && !strcasecmp(new_profile->extip, profile->extip))) &&
		((!new_profile->server && !profile->server) ||
		 (new_profile->server && profile->server && !strcasecmp(new_profile->server, profile->server))) &&
		((!new_profile->timer_name && !profile->timer_name) ||
		 (new_profile->timer_name && profile->timer_name && !strcasecmp(new_profile->timer_name, profile->timer_name))) &&
		((!new_profile->lanaddr && !profile->lanaddr) ||
		 (new_profile->lanaddr && profile->lanaddr && !strcasecmp(new_profile->lanaddr, profile->lanaddr))) &&
		((!new_profile->exten && !profile->exten) ||
		 (new_profile->exten && profile->exten && !strcasecmp(new_profile->exten, profile->exten))) &&
		((!new_profile->context && !profile->context) ||
		 (new_profile->context && profile->context && !strcasecmp(new_profile->context, profile->context))) &&
		(new_profile->user_flags == profile->user_flags) && (new_profile->acl_count == profile->acl_count)
		) {
		uint32_t i;
		
		if (!(((!new_profile->odbc_dsn && !profile->odbc_dsn) ||
			   (new_profile->odbc_dsn && profile->odbc_dsn && !strcasecmp(new_profile->odbc_dsn, profile->odbc_dsn))) 
			  )) {
			return SWITCH_FALSE;
		}
		

		for (i = 0; i < new_profile->acl_count; i++) {
			if (strcasecmp(new_profile->acl[i], profile->acl[i]) != 0) {
				return SWITCH_FALSE;
			}
		}
	}

	return SWITCH_TRUE;
}

static switch_status_t destroy_profile(char *name)
{
	mdl_profile_t *profile = NULL;

	if ((profile = switch_core_hash_find(globals.profile_hash, name))) {
		if (profile->user_flags & LDL_FLAG_COMPONENT) {
			switch_mutex_destroy(profile->mutex);
		}

		if (switch_thread_rwlock_trywrlock(profile->rwlock) != SWITCH_STATUS_SUCCESS) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Profile %s is busy\n", profile->name);
			profile->purge = SWITCH_TRUE;
			if (profile->handle) {
				ldl_handle_stop(profile->handle);
			}
		} else {
			switch_thread_rwlock_unlock(profile->rwlock);
			profile->purge = SWITCH_TRUE;

			if (profile->handle) {
				ldl_handle_stop(profile->handle);
			}

			if (switch_core_hash_delete(globals.profile_hash, profile->name) == SWITCH_STATUS_SUCCESS) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Profile %s deleted successfully\n", profile->name);
			}
		}
	}

	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t soft_reload(void)
{
	char *cf = "dingaling.conf";
	mdl_profile_t *profile = NULL, *old_profile = NULL;
	switch_xml_t cfg, xml, /*settings, */ param, xmlint;

	void *data = NULL;
	switch_hash_t *name_hash;
	switch_hash_index_t *hi;
	switch_core_hash_init(&name_hash, module_pool);

	if (!(xml = switch_xml_open_cfg(cf, &cfg, NULL))) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Open of %s failed\n", cf);
		return SWITCH_STATUS_TERM;
	}

	if (!(xmlint = switch_xml_child(cfg, "profile"))) {
		if ((xmlint = switch_xml_child(cfg, "interface"))) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "!!!!!!! DEPRICATION WARNING 'interface' is now 'profile' !!!!!!!\n");
		}
	}

	for (; xmlint; xmlint = xmlint->next) {
		char *type = (char *) switch_xml_attr_soft(xmlint, "type");
		for (param = switch_xml_child(xmlint, "param"); param; param = param->next) {
			char *var = (char *) switch_xml_attr_soft(param, "name");
			char *val = (char *) switch_xml_attr_soft(param, "value");

			if (!profile) {
				profile = switch_core_alloc(module_pool, sizeof(*profile));
			}

			set_profile_val(profile, var, val);
		}

		if (profile && type && !strcasecmp(type, "component")) {
			char dbname[256];
			switch_cache_db_handle_t *dbh = NULL;

			if (!profile->login && profile->name) {
				profile->login = switch_core_strdup(module_pool, profile->name);
			}

			switch_set_flag(profile, TFLAG_AUTO);
			profile->message = "";
			profile->user_flags |= LDL_FLAG_COMPONENT;
			switch_mutex_init(&profile->mutex, SWITCH_MUTEX_NESTED, module_pool);
			switch_snprintf(dbname, sizeof(dbname), "dingaling_%s", profile->name);
			profile->dbname = switch_core_strdup(module_pool, dbname);

			if ((dbh = mdl_get_db_handle(profile))) {
				switch_cache_db_test_reactive(dbh, "select * from jabber_subscriptions", NULL, sub_sql);
				switch_cache_db_release_db_handle(&dbh);
			}
		}

		if (profile) {
			switch_core_hash_insert(name_hash, profile->name, profile->login);

			if ((old_profile = switch_core_hash_find(globals.profile_hash, profile->name))) {
				if (match_profile(old_profile, profile)) {
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Found pre-existing profile %s [%s]\n", profile->name, profile->login);
				} else {
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Overwriting pre-existing profile %s [%s]\n", profile->name, profile->login);
					destroy_profile(old_profile->name);
					init_profile(profile, switch_test_flag(profile, TFLAG_AUTO) ? 1 : 0);
				}
			} else {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Found new profile %s [%s]\n", profile->name, profile->login);
				init_profile(profile, switch_test_flag(profile, TFLAG_AUTO) ? 1 : 0);
			}

			profile = NULL;
		}
	}

	switch_xml_free(xml);

	for (hi = switch_hash_first(NULL, globals.profile_hash); hi; hi = switch_hash_next(hi)) {
		switch_hash_this(hi, NULL, NULL, &data);
		profile = (mdl_profile_t *) data;

		if (switch_core_hash_find(name_hash, profile->name)) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "New profile %s [%s] activated\n", profile->name, profile->login);
		} else {
			switch_core_hash_insert(name_hash, profile->name, profile->name);
		}
	}

	for (hi = switch_hash_first(NULL, name_hash); hi; hi = switch_hash_next(hi)) {
		switch_hash_this(hi, NULL, NULL, &data);

		if ((profile = switch_core_hash_find(globals.profile_hash, (char *) data))) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Deleting unused profile %s [%s]\n", profile->name, profile->login);
			destroy_profile(profile->name);
		}
	}

	switch_core_hash_destroy(&name_hash);

	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t load_config(void)
{
	char *cf = "dingaling.conf";
	mdl_profile_t *profile = NULL;
	switch_xml_t cfg, xml, settings, param, xmlint;

	memset(&globals, 0, sizeof(globals));
	globals.running = 1;
	globals.auto_nat = (switch_nat_get_type() ? 1 : 0);

	switch_find_local_ip(globals.guess_ip, sizeof(globals.guess_ip), NULL, AF_INET);

	switch_core_hash_init(&globals.profile_hash, module_pool);

	if (!(xml = switch_xml_open_cfg(cf, &cfg, NULL))) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Open of %s failed\n", cf);
		return SWITCH_STATUS_TERM;
	}

	if ((settings = switch_xml_child(cfg, "settings"))) {
		for (param = switch_xml_child(settings, "param"); param; param = param->next) {
			char *var = (char *) switch_xml_attr_soft(param, "name");
			char *val = (char *) switch_xml_attr_soft(param, "value");

			if (!strcasecmp(var, "debug")) {
				globals.debug = atoi(val);
			} else if (!strcasecmp(var, "codec-prefs")) {
				set_global_codec_string(val);
				globals.codec_order_last = switch_separate_string(globals.codec_string, ',', globals.codec_order, SWITCH_MAX_CODECS);
			} else if (!strcasecmp(var, "codec-rates")) {
				set_global_codec_rates_string(val);
				globals.codec_rates_last = switch_separate_string(globals.codec_rates_string, ',', globals.codec_rates, SWITCH_MAX_CODECS);
			}
		}
	}

	if (!(xmlint = switch_xml_child(cfg, "profile"))) {
		if ((xmlint = switch_xml_child(cfg, "interface"))) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "!!!!!!! DEPRICATION WARNING 'interface' is now 'profile' !!!!!!!\n");
		}
	}

	for (; xmlint; xmlint = xmlint->next) {
		char *type = (char *) switch_xml_attr_soft(xmlint, "type");
		for (param = switch_xml_child(xmlint, "param"); param; param = param->next) {
			char *var = (char *) switch_xml_attr_soft(param, "name");
			char *val = (char *) switch_xml_attr_soft(param, "value");

			if (!globals.init) {
				ldl_global_init(globals.debug);
				ldl_global_set_logger(dl_logger);
				globals.init = 1;
			}

			if (!profile) {
				profile = switch_core_alloc(module_pool, sizeof(*profile));
			}

			set_profile_val(profile, var, val);
		}


		if (profile && type && !strcasecmp(type, "component")) {
			char dbname[256];
			switch_cache_db_handle_t *dbh = NULL;

			if (!profile->login && profile->name) {
				profile->login = switch_core_strdup(module_pool, profile->name);
			}

			switch_set_flag(profile, TFLAG_AUTO);
			profile->message = "";
			profile->user_flags |= LDL_FLAG_COMPONENT;
			switch_mutex_init(&profile->mutex, SWITCH_MUTEX_NESTED, module_pool);
			switch_snprintf(dbname, sizeof(dbname), "dingaling_%s", profile->name);
			profile->dbname = switch_core_strdup(module_pool, dbname);


			if ((dbh = mdl_get_db_handle(profile))) {
				switch_cache_db_test_reactive(dbh, "select * from jabber_subscriptions", NULL, sub_sql);
				switch_cache_db_release_db_handle(&dbh);
			}
		}

		if (profile) {
			init_profile(profile, switch_test_flag(profile, TFLAG_AUTO) ? 1 : 0);
			profile = NULL;
		}
	}

	if (profile) {
		init_profile(profile, switch_test_flag(profile, TFLAG_AUTO) ? 1 : 0);
		profile = NULL;
	}

	if (!globals.dialplan) {
		set_global_dialplan("default");
	}

	if (!globals.init) {
		ldl_global_init(globals.debug);
		ldl_global_set_logger(dl_logger);
		globals.init = 1;
	}


	switch_xml_free(xml);

	return SWITCH_STATUS_SUCCESS;
}


static void do_vcard(ldl_handle_t *handle, char *to, char *from, char *id)
{
	switch_event_t *params = NULL;
	char *real_to, *to_user, *xmlstr = NULL, *to_host = NULL;
	switch_xml_t domain, xml = NULL, user, vcard;
	int sent = 0;

	if (!strncasecmp(to, "user+", 5)) {
		real_to = to + 5;
	} else {
		real_to = to;
	}


	if ((to_user = strdup(real_to))) {
		if ((to_host = strchr(to_user, '@'))) {
			*to_host++ = '\0';
		}
	} else {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Memory Error!\n");
		goto end;
	}

	if (!to_host) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Missing Host!\n");
		goto end;
	}

	switch_event_create(&params, SWITCH_EVENT_REQUEST_PARAMS);
	switch_assert(params);
	switch_event_add_header(params, SWITCH_STACK_BOTTOM, "to", "%s@%s", to_user, to_host);
	switch_event_add_header_string(params, SWITCH_STACK_BOTTOM, "from", from);
	switch_event_add_header_string(params, SWITCH_STACK_BOTTOM, "object", "vcard");

	if (switch_xml_locate("directory", "domain", "name", to_host, &xml, &domain, params, SWITCH_FALSE) != SWITCH_STATUS_SUCCESS) {
		//switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "can't find domain for [%s@%s]\n", to_user, to_host);
		goto end;
	}

	if (!(user = switch_xml_find_child(domain, "user", "id", to_user))) {
		//switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "can't find user [%s@%s]\n", to_user, to_host);
		goto end;
	}

	if (!(vcard = switch_xml_child(user, "vcard"))) {
		//switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "can't find <vcard> tag for user [%s@%s]\n", to_user, to_host);
		goto end;
	}

	switch_xml_set_attr(vcard, "xmlns", "vcard-tmp");

	if ((xmlstr = switch_xml_toxml(vcard, SWITCH_FALSE))) {
		ldl_handle_send_vcard(handle, to, from, id, xmlstr);
		sent = 1;
	} else {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Memory Error!\n");
	}

  end:

	switch_event_destroy(&params);

	if (!sent) {
		ldl_handle_send_vcard(handle, to, from, id, NULL);
	}

	if (xml)
		switch_xml_free(xml);
	switch_safe_free(to_user);
	switch_safe_free(params);
	switch_safe_free(xmlstr);
}

static switch_status_t parse_candidates(ldl_session_t *dlsession, switch_core_session_t *session, ldl_transport_type_t ttype, const char *subject) 
{
	
	ldl_candidate_t *candidates;
	unsigned int len = 0;
	unsigned int x, choice = 0, ok = 0;
	uint8_t lanaddr = 0;
	struct private_object *tech_pvt = NULL;
	switch_status_t status = LDL_STATUS_SUCCESS;

	if (!(tech_pvt = switch_core_session_get_private(session))) {
		return SWITCH_STATUS_FALSE;
	}

	if (ldl_session_get_candidates(dlsession, ttype, &candidates, &len) != LDL_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "Candidate Error!\n");
		switch_set_flag(tech_pvt, TFLAG_BYE);
		switch_clear_flag(tech_pvt, TFLAG_IO);
		status = LDL_STATUS_FALSE;
		goto end;
	}


	tech_pvt->transports[ttype].total = len;

	switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "%u %s candidates\n", len, ldl_transport_type_str(ttype));

	if (tech_pvt->profile->acl_count) {
		for (x = 0; x < len; x++) {
			uint32_t y = 0;

			if (strcasecmp(candidates[x].protocol, "udp")) {
				switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "candidate %s:%d has an unsupported protocol!\n",
								  candidates[x].address, candidates[x].port);
				continue;
			}

			for (y = 0; y < tech_pvt->profile->acl_count; y++) {
																																																																																																															
				if (switch_check_network_list_ip(candidates[x].address, tech_pvt->profile->acl[y])) {
					choice = x;
					ok = 1;
				}
																																																																																																																																															
				if (ok) {
					switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "candidate %s:%d PASS ACL %s\n",
									  candidates[x].address, candidates[x].port, tech_pvt->profile->acl[y]);
					goto end_candidates;
				} else {
					switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "candidate %s:%d FAIL ACL %s\n",
									  candidates[x].address, candidates[x].port, tech_pvt->profile->acl[y]);
				}
			}
		}
	} else {
		for (x = 0; x < len; x++) {
			

			if (tech_pvt->profile->lanaddr) {
				lanaddr = strncasecmp(candidates[x].address, tech_pvt->profile->lanaddr, strlen(tech_pvt->profile->lanaddr)) ? 0 : 1;
			}

			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "%s candidates %s:%d\n", 
							  ldl_transport_type_str(ttype), candidates[x].address,
							  candidates[x].port);


			// 192.0.0.0 - 192.0.127.255 is marked as reserved, should we filter all of them?
			if (!strcasecmp(candidates[x].protocol, "udp") &&
				(!strcasecmp(candidates[x].type, "local") || !strcasecmp(candidates[x].type, "stun") || !strcasecmp(candidates[x].type, "relay")) &&
				((tech_pvt->profile->lanaddr &&
				  lanaddr) || (strncasecmp(candidates[x].address, "10.", 3) &&
							   strncasecmp(candidates[x].address, "192.168.", 8) &&
							   strncasecmp(candidates[x].address, "127.", 4) &&
							   strncasecmp(candidates[x].address, "255.", 4) &&
							   strncasecmp(candidates[x].address, "0.", 2) &&
							   strncasecmp(candidates[x].address, "1.", 2) &&
							   strncasecmp(candidates[x].address, "2.", 2) &&
							   strncasecmp(candidates[x].address, "172.16.", 7) &&
							   strncasecmp(candidates[x].address, "172.17.", 7) &&
							   strncasecmp(candidates[x].address, "172.18.", 7) &&
							   strncasecmp(candidates[x].address, "172.19.", 7) &&
							   strncasecmp(candidates[x].address, "172.2", 5) &&
							   strncasecmp(candidates[x].address, "172.30.", 7) &&
							   strncasecmp(candidates[x].address, "172.31.", 7) &&
							   strncasecmp(candidates[x].address, "192.0.2.", 8) && strncasecmp(candidates[x].address, "169.254.", 8)
							   ))) {
				choice = x;
				ok = 1;
			}
		}
	}


 end_candidates:
	
	if (ok) {
		ldl_payload_t payloads[5];
		char *key;
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG,
						  "Acceptable %s Candidate %s:%d\n", ldl_transport_type_str(ttype), candidates[choice].address, candidates[choice].port);


		if (tech_pvt->transports[ttype].accepted) {
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "Already Accepted [%s:%d]\n", 
							  tech_pvt->transports[ttype].remote_ip, tech_pvt->transports[ttype].remote_port);
			//goto end;
		}


		if (tech_pvt->transports[ttype].remote_ip) {
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "Already picked an IP [%s]\n", tech_pvt->transports[ttype].remote_ip);
			//goto end;
		}

		
		memset(payloads, 0, sizeof(payloads));

		tech_pvt->transports[ttype].accepted++;

		if (ttype == LDL_TPORT_VIDEO_RTP) {
			if ((key = ldl_session_get_value(dlsession, "video:crypto:1"))) {
				mdl_add_crypto(tech_pvt, ttype, key, SWITCH_RTP_CRYPTO_RECV);
			} else {
				tech_pvt->transports[ttype].crypto_type = 0;
			}
		} else if (ttype == LDL_TPORT_RTP) {
			if ((key = ldl_session_get_value(dlsession, "audio:crypto:1"))) {
				mdl_add_crypto(tech_pvt, ttype, key, SWITCH_RTP_CRYPTO_RECV);
			} else {
				tech_pvt->transports[ttype].crypto_type = 0;
			}
		}
		
		if (!switch_test_flag(tech_pvt, TFLAG_OUTBOUND)) {
			switch_set_flag_locked(tech_pvt, TFLAG_TRANSPORT_ACCEPT);
			//ldl_session_accept_candidate(dlsession, &candidates[choice]);
		}

		if (!strcasecmp(subject, "candidates")) {
			//switch_set_flag_locked(tech_pvt, TFLAG_TRANSPORT_ACCEPT);
			switch_set_flag_locked(tech_pvt, TFLAG_ANSWER);
		}

		if (lanaddr) {
			switch_set_flag_locked(tech_pvt, TFLAG_LANADDR);
		}

		if (!get_codecs(tech_pvt)) {
			terminate_session(&session, __LINE__, SWITCH_CAUSE_DESTINATION_OUT_OF_ORDER);
			status = LDL_STATUS_FALSE;
			goto end;
		}


		tech_pvt->transports[ttype].remote_ip = switch_core_session_strdup(session, candidates[choice].address);
		ldl_session_set_ip(dlsession, tech_pvt->transports[ttype].remote_ip);
		tech_pvt->transports[ttype].remote_port = candidates[choice].port;
		tech_pvt->transports[ttype].remote_user = switch_core_session_strdup(session, candidates[choice].username);
		tech_pvt->transports[ttype].remote_pass = switch_core_session_strdup(session, candidates[choice].password);

		if (!switch_test_flag(tech_pvt, TFLAG_OUTBOUND)) {
			if (!do_candidates(tech_pvt, 0)) {
				terminate_session(&session, __LINE__, SWITCH_CAUSE_DESTINATION_OUT_OF_ORDER);
				status = LDL_STATUS_FALSE;

				goto end;
			}
		}

		if (!switch_test_flag(tech_pvt, TFLAG_OUTBOUND)) {

			if (tech_pvt->transports[LDL_TPORT_VIDEO_RTP].accepted &&
				tech_pvt->transports[LDL_TPORT_VIDEO_RTCP].accepted) {
				activate_video_rtp(tech_pvt);
			}


			if (tech_pvt->transports[LDL_TPORT_RTP].accepted &&
				tech_pvt->transports[LDL_TPORT_RTCP].accepted) {
				activate_audio_rtp(tech_pvt);
			}


			tech_pvt->transports[ttype].restart_rtp++;
		}


		status = LDL_STATUS_SUCCESS;
	}

 end:

	return status;

}


static ldl_status parse_payloads_type(ldl_session_t *dlsession, switch_core_session_t *session, 
										ldl_transport_type_t ttype, ldl_payload_t *payloads, unsigned int len)
{
	struct private_object *tech_pvt = NULL;
	switch_status_t status = LDL_STATUS_SUCCESS;
	unsigned int x, y;
	int match = 0;

	tech_pvt = switch_core_session_get_private(session);	

	switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "%u payloads\n", len);
	for (x = 0; x < len; x++) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "Available Payload %s %u\n", payloads[x].name,
						  payloads[x].id);
		for (y = 0; y < tech_pvt->num_codecs; y++) {
			char *name = tech_pvt->codecs[y]->iananame;

			if ((ttype == LDL_TPORT_VIDEO_RTP && tech_pvt->codecs[y]->codec_type != SWITCH_CODEC_TYPE_VIDEO) || 
				(ttype == LDL_TPORT_RTP && tech_pvt->codecs[y]->codec_type != SWITCH_CODEC_TYPE_AUDIO)) {
				continue;
			}

			if (!strncasecmp(name, "ilbc", 4)) {
				name = "ilbc";
			}
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "compare %s %d/%d to %s %d/%d\n",
							  payloads[x].name, payloads[x].id, payloads[x].rate,
							  name, tech_pvt->codecs[y]->ianacode, tech_pvt->codecs[y]->samples_per_second);

			if (tech_pvt->codecs[y]->ianacode > 95) {
				match = strcasecmp(name, payloads[x].name) ? 0 : 1;
			} else {
				match = (payloads[x].id == tech_pvt->codecs[y]->ianacode) ? 1 : 0;
			}
						
			if (match && payloads[x].rate == tech_pvt->codecs[y]->samples_per_second) {
				tech_pvt->transports[ttype].codec_index = y;
				switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "Choosing %s Payload index %u %s %u\n", 
								  ldl_transport_type_str(ttype),
								  y,
								  payloads[x].name, payloads[x].id);
				tech_pvt->transports[ttype].codec_name = tech_pvt->codecs[y]->iananame;
				tech_pvt->transports[ttype].codec_num = tech_pvt->codecs[y]->ianacode;
				tech_pvt->transports[ttype].r_codec_num = (switch_payload_t) (payloads[x].id);
				tech_pvt->transports[ttype].codec_rate = payloads[x].rate;
				tech_pvt->transports[ttype].ptime = payloads[x].ptime;
				tech_pvt->transports[ttype].payload_count++;

				if (ttype == LDL_TPORT_VIDEO_RTP) {
					tech_pvt->transports[ttype].vid_width = payloads[x].width;
					tech_pvt->transports[ttype].vid_height = payloads[x].height;
					tech_pvt->transports[ttype].vid_rate = payloads[x].framerate;
				}

				if (!switch_test_flag(tech_pvt, TFLAG_OUTBOUND)) {


					if (!do_describe(tech_pvt, 0)) {
						terminate_session(&session, __LINE__, SWITCH_CAUSE_DESTINATION_OUT_OF_ORDER);
						status = LDL_STATUS_FALSE;
						goto done;
					}
				}
				status = LDL_STATUS_SUCCESS;
				goto done;
			}
		}
	}

 done:

	return status;

}

static ldl_status parse_payloads(ldl_session_t *dlsession, switch_core_session_t *session, ldl_payload_t *payloads, unsigned int len)
{
	int match = 0;
	struct private_object *tech_pvt = NULL;
	ldl_status status;

	tech_pvt = switch_core_session_get_private(session);

	
	if ((status = parse_payloads_type(dlsession, session, LDL_TPORT_RTP, payloads, len)) == LDL_STATUS_SUCCESS) {
		match++;
	}

	if (tech_pvt->transports[LDL_TPORT_VIDEO_RTP].ready) {
		if ((status = parse_payloads_type(dlsession, session, LDL_TPORT_VIDEO_RTP, payloads, len)) == LDL_STATUS_SUCCESS) {
			match++;
		}
	}

	if (!match && !switch_test_flag(tech_pvt, TFLAG_OUTBOUND)) {
		if (!do_describe(tech_pvt, 0)) {
			terminate_session(&session, __LINE__, SWITCH_CAUSE_DESTINATION_OUT_OF_ORDER);
			status = LDL_STATUS_FALSE;
		}
	}


	return status;

}
		

static ldl_status handle_signalling(ldl_handle_t *handle, ldl_session_t *dlsession, ldl_signal_t dl_signal, char *to, char *from, char *subject,
									char *msg)
{
	mdl_profile_t *profile = NULL;
	switch_core_session_t *session = NULL;
	switch_channel_t *channel = NULL;
	struct private_object *tech_pvt = NULL;
	switch_event_t *event;
	ldl_status status = LDL_STATUS_SUCCESS;
	char *sql;

	switch_assert(handle != NULL);

	if (!(profile = ldl_handle_get_private(handle))) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "ERROR NO PROFILE!\n");
		status = LDL_STATUS_FALSE;
		goto done;
	}

	if (!dlsession) {
		if (profile->user_flags & LDL_FLAG_COMPONENT) {
			switch (dl_signal) {
			case LDL_SIGNAL_VCARD:
				do_vcard(handle, to, from, subject);
				break;
			case LDL_SIGNAL_UNSUBSCRIBE:

				if ((sql = switch_mprintf("delete from jabber_subscriptions where sub_from='%q' and sub_to='%q';", from, to))) {
					mdl_execute_sql(profile, sql, profile->mutex);
					free(sql);
				}

				break;

			case LDL_SIGNAL_SUBSCRIBE:
				if (profile->user_flags & LDL_FLAG_COMPONENT && ldl_jid_domcmp(from, to)) {
					switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "Attempt to add presence from/to our own domain [%s][%s]\n",
									  from, to);
				} else {
					switch_mutex_lock(profile->mutex);
					if ((sql = switch_mprintf("delete from jabber_subscriptions where sub_from='%q' and sub_to='%q'", from, to))) {
						mdl_execute_sql(profile, sql, NULL);
						free(sql);
					}
					if ((sql = switch_mprintf("insert into jabber_subscriptions values('%q','%q','%q','%q');\n",
											  switch_str_nil(from), switch_str_nil(to), switch_str_nil(msg), switch_str_nil(subject)))) {
						mdl_execute_sql(profile, sql, NULL);
						free(sql);
					}
					switch_mutex_unlock(profile->mutex);
					if (is_special(to)) {
						ldl_handle_send_presence(profile->handle, to, from, NULL, NULL, "Click To Call", profile->avatar);
					} else {
						ldl_handle_send_presence(profile->handle, to, from, NULL, NULL, "Authenticated.\nCome to ClueCon!\nhttp://www.cluecon.com",
												 profile->avatar);
					}
#if 0
					if (is_special(to)) {
						if (switch_event_create(&event, SWITCH_EVENT_PRESENCE_IN) == SWITCH_STATUS_SUCCESS) {
							switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "proto", MDL_CHAT_PROTO);
							switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "login", profile->login);
							switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "from", to);
							//switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "rpid", "unknown");
							switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "status", "Click To Call");
							switch_event_fire(&event);
						}
					}
#endif
				}
				break;
			case LDL_SIGNAL_ROSTER:
				if (switch_event_create(&event, SWITCH_EVENT_ROSTER) == SWITCH_STATUS_SUCCESS) {
					switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "proto", MDL_CHAT_PROTO);
					switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "from", from);
					switch_event_fire(&event);
				}
				break;
			case LDL_SIGNAL_PRESENCE_PROBE:
				if (is_special(to)) {
					ldl_handle_send_presence(profile->handle, to, from, NULL, NULL, "Click To Call", profile->avatar);
				} else {
					if (switch_event_create(&event, SWITCH_EVENT_PRESENCE_PROBE) == SWITCH_STATUS_SUCCESS) {
						switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "proto", MDL_CHAT_PROTO);
						switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "login", profile->login);
						switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "from", from);
						switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "to", to);
						switch_event_fire(&event);
					}
				}
				break;
			case LDL_SIGNAL_PRESENCE_IN:

				if ((sql = switch_mprintf("update jabber_subscriptions set show_pres='%q', status='%q' where sub_from='%q'",
										  switch_str_nil(msg), switch_str_nil(subject), switch_str_nil(from)))) {
					mdl_execute_sql(profile, sql, profile->mutex);
					free(sql);
				}

				if (switch_event_create(&event, SWITCH_EVENT_PRESENCE_IN) == SWITCH_STATUS_SUCCESS) {
					switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "proto", MDL_CHAT_PROTO);
					switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "login", profile->login);
					switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "from", from);
					switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "rpid", msg);
					switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "status", subject);
					switch_event_fire(&event);
				}


				if (is_special(to)) {
					ldl_handle_send_presence(profile->handle, to, from, NULL, NULL, "Click To Call", profile->avatar);
				}
#if 0
				if (is_special(to)) {
					if (switch_event_create(&event, SWITCH_EVENT_PRESENCE_IN) == SWITCH_STATUS_SUCCESS) {
						switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "proto", MDL_CHAT_PROTO);
						switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "login", profile->login);
						switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "from", to);
						//switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "rpid", "unknown");
						switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "status", "Click To Call");
						switch_event_fire(&event);
					}
				}
#endif
				break;

			case LDL_SIGNAL_PRESENCE_OUT:

				if ((sql = switch_mprintf("update jabber_subscriptions set show_pres='%q', status='%q' where sub_from='%q'",
										  switch_str_nil(msg), switch_str_nil(subject), switch_str_nil(from)))) {
					mdl_execute_sql(profile, sql, profile->mutex);
					free(sql);
				}
				if (switch_event_create(&event, SWITCH_EVENT_PRESENCE_OUT) == SWITCH_STATUS_SUCCESS) {
					switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "proto", MDL_CHAT_PROTO);
					switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "login", profile->login);
					switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "from", from);
					switch_event_fire(&event);
				}
				break;
			default:
				break;
			}
		}

		switch (dl_signal) {
		case LDL_SIGNAL_MSG:{
				char *proto = MDL_CHAT_PROTO;
				char *pproto = NULL, *ffrom = NULL;
				char *hint;
				switch_event_t *event;
				char *from_user, *from_host;
#ifdef AUTO_REPLY
				if (profile->auto_reply) {
					ldl_handle_send_msg(handle,
										(profile->user_flags & LDL_FLAG_COMPONENT) ? to : ldl_handle_get_login(profile->handle), from, "",
										switch_str_nil(profile->auto_reply));
				}
#endif

				if (strchr(to, '+')) {
					pproto = strdup(to);
					if ((to = strchr(pproto, '+'))) {
						*to++ = '\0';
					}
					proto = pproto;
				}

				hint = from;

				if (strchr(from, '/') && (ffrom = strdup(from))) {
					char *p;
					if ((p = strchr(ffrom, '/'))) {
						*p = '\0';
					}
					from = ffrom;
				}

				from_user = strdup(from);
				if ((from_host = strchr(from_user, '@'))) {
					*from_host++ = '\0';
				}


				if (switch_event_create(&event, SWITCH_EVENT_MESSAGE) == SWITCH_STATUS_SUCCESS) {
					switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "proto", MDL_CHAT_PROTO);
					switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "from", from);
					switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "from_user", from_user);
					if (from_host) {
						switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "from_host", from_host);
					}
					switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "to", to);
					switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "subject", subject);
					switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "type", "text/plain");
					switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "hint", hint);
					switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "from_full", hint);
					switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "ldl_profile", profile->name);
					
					if (msg) {
						switch_event_add_body(event, "%s", msg);
					}
				} else {
					abort();
				}
				
				switch_safe_free(from_user);

				if (!zstr(msg)) {
					if (strcasecmp(proto, MDL_CHAT_PROTO)) { /* yes no ! on purpose */
						switch_core_chat_send(proto, event);
					}
					
					switch_core_chat_send("GLOBAL", event);
				}

				switch_event_destroy(&event);

				switch_safe_free(pproto);
				switch_safe_free(ffrom);
			}
			break;
		case LDL_SIGNAL_LOGIN_SUCCESS:
			if (switch_event_create_subclass(&event, SWITCH_EVENT_CUSTOM, DL_EVENT_LOGIN_SUCCESS) == SWITCH_STATUS_SUCCESS) {
				switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "login", ldl_handle_get_login(profile->handle));
				switch_event_fire(&event);
			}
			if (profile->user_flags & LDL_FLAG_COMPONENT) {
				sign_on(profile);
			}

			break;
		case LDL_SIGNAL_LOGIN_FAILURE:
			if (switch_event_create_subclass(&event, SWITCH_EVENT_CUSTOM, DL_EVENT_LOGIN_FAILURE) == SWITCH_STATUS_SUCCESS) {
				switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "login", ldl_handle_get_login(profile->handle));
				switch_event_fire(&event);
			}
			break;
		case LDL_SIGNAL_CONNECTED:
			if (switch_event_create_subclass(&event, SWITCH_EVENT_CUSTOM, DL_EVENT_CONNECTED) == SWITCH_STATUS_SUCCESS) {
				switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "login", ldl_handle_get_login(profile->handle));
				switch_event_fire(&event);
			}
			break;
		default:
			break;

		}
		status = LDL_STATUS_SUCCESS;
		goto done;
	}


	if ((session = ldl_session_get_private(dlsession))) {
		tech_pvt = switch_core_session_get_private(session);
		switch_assert(tech_pvt != NULL);

		channel = switch_core_session_get_channel(session);

		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "using Existing session for %s\n", ldl_session_get_id(dlsession));

		if (switch_channel_down(channel)) {
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "Call %s is already over\n", switch_channel_get_name(channel));
			status = LDL_STATUS_FALSE;
			goto done;
		}

	} else {
		if (dl_signal != LDL_SIGNAL_INITIATE && !msg) {
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "Session is already dead\n");
			status = LDL_STATUS_FALSE;
			goto done;
		}

		if (profile->purge) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Profile '%s' is marked for deletion, disallowing incoming call\n", profile->name);
			status = LDL_STATUS_FALSE;
			goto done;
		}

		if (switch_thread_rwlock_tryrdlock(profile->rwlock) != SWITCH_STATUS_SUCCESS) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Can't do read lock on profile '%s'\n", profile->name);
			status = LDL_STATUS_FALSE;
			goto done;
		}

		if ((session = switch_core_session_request(dingaling_endpoint_interface, SWITCH_CALL_DIRECTION_INBOUND, SOF_NONE, NULL)) != 0) {
			switch_core_session_add_stream(session, NULL);

			if ((tech_pvt = (struct private_object *) switch_core_session_alloc(session, sizeof(struct private_object))) != 0) {
				char *exten;
				char *context;
				char *cid_name;
				char *cid_num;
				char *tmp, *t, *them = NULL;

				memset(tech_pvt, 0, sizeof(*tech_pvt));
				switch_mutex_init(&tech_pvt->flag_mutex, SWITCH_MUTEX_NESTED, switch_core_session_get_pool(session));
				tech_pvt->flags |= globals.flags;
				tech_pvt->flags |= profile->flags;
				channel = switch_core_session_get_channel(session);
				switch_core_session_set_private(session, tech_pvt);
				tech_pvt->dlsession = dlsession;

				tech_pvt->session = session;
				tech_pvt->channel = switch_core_session_get_channel(session);
				tech_pvt->transports[LDL_TPORT_RTP].codec_index = -1;
				tech_pvt->transports[LDL_TPORT_VIDEO_RTP].codec_index = -1;
				tech_pvt->profile = profile;

				switch_set_flag(tech_pvt, TFLAG_SECURE);
				mdl_build_crypto(tech_pvt, LDL_TPORT_RTP, 1, AES_CM_128_HMAC_SHA1_80, SWITCH_RTP_CRYPTO_SEND);
				mdl_build_crypto(tech_pvt, LDL_TPORT_VIDEO_RTP, 1, AES_CM_128_HMAC_SHA1_80, SWITCH_RTP_CRYPTO_SEND);


				if (!(tech_pvt->transports[LDL_TPORT_RTP].local_port = switch_rtp_request_port(profile->ip))) {
					switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_CRIT, "No RTP port available!\n");
					terminate_session(&session, __LINE__, SWITCH_CAUSE_DESTINATION_OUT_OF_ORDER);
					status = LDL_STATUS_FALSE;
					goto done;
				}
				tech_pvt->transports[LDL_TPORT_RTP].adv_local_port = tech_pvt->transports[LDL_TPORT_RTP].local_port;
				tech_pvt->transports[LDL_TPORT_RTCP].adv_local_port = tech_pvt->transports[LDL_TPORT_RTP].local_port + 1;

				if (!(tech_pvt->transports[LDL_TPORT_VIDEO_RTP].local_port = switch_rtp_request_port(profile->ip))) {
					switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_CRIT, "No RTP port available!\n");
					terminate_session(&session, __LINE__, SWITCH_CAUSE_DESTINATION_OUT_OF_ORDER);
					status = LDL_STATUS_FALSE;
					goto done;
				}
				tech_pvt->transports[LDL_TPORT_VIDEO_RTP].adv_local_port = tech_pvt->transports[LDL_TPORT_VIDEO_RTP].local_port;
				tech_pvt->transports[LDL_TPORT_VIDEO_RTCP].adv_local_port = tech_pvt->transports[LDL_TPORT_VIDEO_RTP].local_port + 1;



				switch_set_flag_locked(tech_pvt, TFLAG_ANSWER);
				tech_pvt->recip = switch_core_session_strdup(session, from);
				if (!(exten = ldl_session_get_value(dlsession, "dnis"))) {
					exten = profile->exten;
					/* if it's _auto_ set the extension to match the username portion of the called address */
					if (!strcmp(exten, "_auto_")) {
						if ((t = ldl_session_get_callee(dlsession))) {
							if ((them = strdup(t))) {
								char *a, *b, *p;
								if ((p = strchr(them, '/'))) {
									*p = '\0';
								}

								if ((a = strchr(them, '+')) && (b = strrchr(them, '+')) && a != b) {
									*b++ = '\0';
									switch_channel_set_variable(channel, "dl_user", them);
									switch_channel_set_variable(channel, "dl_host", b);
								}
								exten = them;
							}
						}
					}
				}

				if (!(context = ldl_session_get_value(dlsession, "context"))) {
					context = profile->context;
				}

				if (!(cid_name = ldl_session_get_value(dlsession, "caller_id_name"))) {
					cid_name = tech_pvt->recip;
				}

				if (!(cid_num = ldl_session_get_value(dlsession, "caller_id_number"))) {
					cid_num = tech_pvt->recip;
				}


				if (switch_stristr("voice.google.com", from)) {
					char *id = switch_core_session_strdup(session, from);
					char *p;
					
					if ((p = strchr(id, '@'))) {
						*p++ = '\0';
						cid_name = "Google Voice";
						cid_num = id;
					}
					
					ldl_session_set_gateway(dlsession);
					
					do_candidates(tech_pvt, 1);
				}



				/* context of "_auto_" means set it to the domain */
				if (profile->context && !strcmp(profile->context, "_auto_")) {
					context = profile->name;
				}

				tech_pvt->them = switch_core_session_strdup(session, ldl_session_get_callee(dlsession));
				tech_pvt->us = switch_core_session_strdup(session, ldl_session_get_caller(dlsession));

				if (tech_pvt->us && (tmp = strdup(tech_pvt->us))) {
					char *p, *q;

					if ((p = strchr(tmp, '@'))) {
						*p++ = '\0';
						if ((q = strchr(p, '/'))) {
							*q = '\0';
						}
						switch_channel_set_variable(channel, "dl_from_user", tmp);
						switch_channel_set_variable(channel, "dl_from_host", p);
					}

					switch_safe_free(tmp);
				}
				
				if (!tech_pvt->caller_profile) {
					switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG,
									  "Creating an identity for %s %s <%s> %s\n", ldl_session_get_id(dlsession), cid_name, cid_num, exten);

					if ((tech_pvt->caller_profile = switch_caller_profile_new(switch_core_session_get_pool(session),
																			  ldl_handle_get_login(profile->handle),
																			  profile->dialplan,
																			  cid_name,
																			  cid_num,
																			  ldl_session_get_ip(dlsession),
																			  ldl_session_get_value(dlsession, "ani"),
																			  ldl_session_get_value(dlsession, "aniii"),
																			  ldl_session_get_value(dlsession, "rdnis"), modname, context, exten)) != 0) {
						char name[128];
						switch_snprintf(name, sizeof(name), "dingaling/%s", tech_pvt->caller_profile->destination_number);
						switch_channel_set_name(channel, name);
						switch_channel_set_caller_profile(channel, tech_pvt->caller_profile);
					}
				}

				switch_safe_free(them);

				switch_set_flag_locked(tech_pvt, TFLAG_TRANSPORT_ACCEPT);
			} else {
				switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "Hey where is my memory pool?\n");
				terminate_session(&session, __LINE__, SWITCH_CAUSE_DESTINATION_OUT_OF_ORDER);
				status = LDL_STATUS_FALSE;
				goto done;
			}
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "Creating a session for %s\n", ldl_session_get_id(dlsession));
			ldl_session_set_private(dlsession, session);
			
			switch_channel_set_name(channel, "DingaLing/new");
			switch_channel_set_state(channel, CS_INIT);
			if (switch_core_session_thread_launch(session) != SWITCH_STATUS_SUCCESS) {
				switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_CRIT, "Error spawning thread\n");
				terminate_session(&session, __LINE__, SWITCH_CAUSE_DESTINATION_OUT_OF_ORDER);
				status = LDL_STATUS_FALSE;
				goto done;
			}
		} else {
			status = LDL_STATUS_FALSE;
			goto done;
		}

	}

	switch (dl_signal) {
	case LDL_SIGNAL_MSG:
		if (msg) {
			if (*msg == '+') {
				char *p = msg + 1;
				switch_dtmf_t dtmf = { 0, switch_core_default_dtmf_duration(0) };
				while (p && *p) {
					dtmf.digit = *p;
					switch_channel_queue_dtmf(channel, &dtmf);
					p++;
				}
				switch_set_flag_locked(tech_pvt, TFLAG_DTMF);
				if (switch_rtp_ready(tech_pvt->transports[LDL_TPORT_RTP].rtp_session)) {
					switch_rtp_set_flag(tech_pvt->transports[LDL_TPORT_RTP].rtp_session, SWITCH_RTP_FLAG_BREAK);
				}
			}
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "SESSION MSG [%s]\n", msg);
		}

		if (switch_event_create(&event, SWITCH_EVENT_MESSAGE) == SWITCH_STATUS_SUCCESS) {
			char *hint = NULL, *p, *freeme = NULL;

			hint = from;
			if (strchr(from, '/')) {
				freeme = strdup(from);
				p = strchr(freeme, '/');
				*p = '\0';
				from = freeme;
			}

			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "proto", MDL_CHAT_PROTO);
			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "login", ldl_handle_get_login(profile->handle));
			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "hint", hint);
			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "from", from);
			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "to", to);
			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "subject", subject);
			if (msg) {
				switch_event_add_body(event, "%s", msg);
			}
			if (switch_core_session_queue_event(tech_pvt->session, &event) != SWITCH_STATUS_SUCCESS) {
				switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "delivery-failure", "true");
				switch_event_fire(&event);
			}

			switch_safe_free(freeme);
		}
		break;
	case LDL_SIGNAL_TRANSPORT_ACCEPT:
		switch_set_flag_locked(tech_pvt, TFLAG_TRANSPORT_ACCEPT);

		if (ldl_session_gateway(dlsession)) {
			do_candidates(tech_pvt, 1);
		}

		break;
	case LDL_SIGNAL_INITIATE:
		if (dl_signal) {
			ldl_payload_t *payloads;
			unsigned int len = 0;
			
			if (switch_test_flag(tech_pvt, TFLAG_OUTBOUND)) {
				if (msg && !strcasecmp(msg, "accept")) {
					switch_set_flag_locked(tech_pvt, TFLAG_ANSWER);
					switch_set_flag_locked(tech_pvt, TFLAG_TRANSPORT_ACCEPT);
					if (!do_candidates(tech_pvt, 0)) {
						terminate_session(&session, __LINE__, SWITCH_CAUSE_DESTINATION_OUT_OF_ORDER);
						status = LDL_STATUS_FALSE;
						goto done;
					}
				}
			}

			if (tech_pvt->transports[LDL_TPORT_RTP].codec_index > -1) {
				switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "Already decided on a codec\n");
				break;
			}


			if (!get_codecs(tech_pvt)) {
				terminate_session(&session, __LINE__, SWITCH_CAUSE_DESTINATION_OUT_OF_ORDER);
				status = LDL_STATUS_FALSE;
				goto done;
			}

			if (ldl_session_get_payloads(dlsession, &payloads, &len) == LDL_STATUS_SUCCESS) {
				status = parse_payloads(dlsession, session, payloads, len);
				goto done;
			}

		}

		break;
	case LDL_SIGNAL_CANDIDATES:
		if (dl_signal) {
			status = SWITCH_STATUS_SUCCESS;

			status = parse_candidates(dlsession, session, LDL_TPORT_RTP, subject);
			status = parse_candidates(dlsession, session, LDL_TPORT_VIDEO_RTP, subject); 
			status = parse_candidates(dlsession, session, LDL_TPORT_RTCP, subject);
			status = parse_candidates(dlsession, session, LDL_TPORT_VIDEO_RTCP, subject); 
		}

		break;
	case LDL_SIGNAL_REJECT:
		if (channel) {
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "reject %s\n", switch_channel_get_name(channel));
			terminate_session(&session, __LINE__, SWITCH_CAUSE_CALL_REJECTED);
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "End Call (Rejected)\n");
			goto done;
		}
		break;
	case LDL_SIGNAL_REDIRECT:
		do_describe(tech_pvt, 1);
		tech_pvt->next_cand = switch_micro_time_now();
		if (channel) switch_channel_mark_ring_ready(channel);
		break;

	case LDL_SIGNAL_ERROR:
	case LDL_SIGNAL_TERMINATE:
		if (channel) {
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "hungup %s\n", switch_channel_get_name(channel));
			terminate_session(&session, __LINE__, SWITCH_CAUSE_NORMAL_CLEARING);
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "End Call\n");
			goto done;
		}
		break;

	default:
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "ERROR\n");
		break;
	}


  done:


	return status;
}

static ldl_status handle_response(ldl_handle_t *handle, char *id)
{
	return LDL_STATUS_SUCCESS;
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
