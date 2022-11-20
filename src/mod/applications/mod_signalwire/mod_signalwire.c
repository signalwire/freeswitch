/*
 * mod_signalwire.c -- SignalWire module
 *
 * Copyright (c) 2018 SignalWire, Inc
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include <switch.h>
#include <switch_curl.h>
#include <switch_stun.h>
#include <signalwire-client-c/client.h>

#ifndef WIN32
#include <sys/utsname.h>
#endif

#ifdef WIN32
void sslLoadWindowsCACertificate();
void sslUnLoadWindowsCACertificate();
int sslContextFunction(void* curl, void* sslctx, void* userdata);
#endif

#define SW_KS_JSON_PRINT(_h, _j) do { \
		char *_json = ks_json_print(_j); \
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ALERT, "--- %s ---\n%s\n---\n", _h, _json); \
		ks_json_free(&_json); \
	} while (0)

static int debug_level = 7;

static int signalwire_gateway_exists(void);
static switch_status_t mod_signalwire_load_or_generate_token(void);

/* Prototypes */
SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_signalwire_shutdown);
SWITCH_MODULE_LOAD_FUNCTION(mod_signalwire_load);
SWITCH_MODULE_RUNTIME_FUNCTION(mod_signalwire_runtime);

/* SWITCH_MODULE_DEFINITION(name, load, shutdown, runtime)
 * Defines a switch_loadable_module_function_table_t and a static const char[] modname
 */
SWITCH_MODULE_DEFINITION(mod_signalwire, mod_signalwire_load, mod_signalwire_shutdown, mod_signalwire_runtime);

typedef enum {
	SW_STATE_ADOPTION,
	SW_STATE_OFFLINE,
	SW_STATE_ONLINE,
	SW_STATE_CONFIGURE,
	SW_STATE_START_PROFILE,
	SW_STATE_REGISTER,
	SW_STATE_READY,
} sw_state_t;

static struct {
	int ssl_verify;
	ks_bool_t shutdown;
	ks_bool_t restarting;
	swclt_config_t *config;
	char blade_bootstrap[1024];
	char adoption_service[1024];
	char stun_server[1024];
	switch_port_t stun_port;
	char adoption_token[64];
	char override_context[64];
	ks_size_t adoption_backoff;
	ks_time_t adoption_next;

	char adoption_data_local_ip[256];
	char adoption_data_external_ip[256];
	char adoption_data_uname[1024];

	char relay_connector_id[256];
	
#if SIGNALWIRE_CLIENT_C_VERSION_MAJOR >= 2
	swclt_sess_t *signalwire_session;
#else
	swclt_sess_t signalwire_session;
	swclt_hmon_t signalwire_session_monitor;
#endif
	sw_state_t state;
	ks_bool_t profile_update;
	ks_bool_t profile_reload;
	ks_bool_t signalwire_reconnected;
	switch_xml_t signalwire_profile;
	char signalwire_profile_md5[SWITCH_MD5_DIGEST_STRING_SIZE];

	ks_bool_t kslog_on;

	switch_mutex_t *mutex; // general mutex for this mod
	char gateway_ip[80];
	char gateway_port[10];
} globals;

static void mod_signalwire_kslogger(const char *file, const char *func, int line, int level, const char *fmt, ...)
{
	const char *fp;
	va_list ap;
	char buf[32768];

	if (level > debug_level) return;

	fp = switch_cut_path(file);

	va_start(ap, fmt);

	vsnprintf(buf, sizeof(buf) - 1, fmt, ap);
	buf[sizeof(buf) - 1] = '\0';

	va_end(ap);
	
	switch_log_printf(SWITCH_CHANNEL_ID_LOG, fp, func, line, NULL, level, "%s\n", buf);
}

static switch_status_t switch_find_available_port(switch_port_t *port, const char *ip, int type)
{
	switch_status_t ret = SWITCH_STATUS_SUCCESS;
	switch_memory_pool_t *pool = NULL;
	switch_sockaddr_t *addr = NULL;
	switch_socket_t *sock = NULL;
	switch_bool_t found = SWITCH_FALSE;

	if ((ret = switch_core_new_memory_pool(&pool)) != SWITCH_STATUS_SUCCESS) {
		goto done;
	}
	
	while (!found) {
		if ((ret = switch_sockaddr_info_get(&addr, ip, SWITCH_UNSPEC, *port, 0, pool)) != SWITCH_STATUS_SUCCESS) {
			goto done;
		}
	
		if ((ret = switch_socket_create(&sock, switch_sockaddr_get_family(addr), type, 0, pool)) != SWITCH_STATUS_SUCCESS) {
			goto done;
		}

		if (!(found = (switch_socket_bind(sock, addr) == SWITCH_STATUS_SUCCESS))) {
			*port = *port + 1;
		}
		
		switch_socket_close(sock);
	}

done:
	if (pool) switch_core_destroy_memory_pool(&pool);

	return ret;
}

struct response_data {
	char *data;
	size_t size;
};

static size_t response_data_handler(void *contents, size_t size, size_t nmemb, void *userp)
{
	size_t received = size * nmemb;
	struct response_data *rd = (struct response_data *)userp;

	if (!rd->data) rd->data = ks_pool_alloc(NULL, received + 1);
	else rd->data = ks_pool_resize(rd->data, rd->size + received + 1);

	memcpy(rd->data + rd->size, contents, received);
	rd->size += received;
	rd->data[rd->size] = 0;

	return received;
}

static void save_sip_config(const char *config)
{
	char confpath[1024];
	FILE *fp = NULL;

	switch_snprintf(confpath, sizeof(confpath), "%s%s%s", SWITCH_GLOBAL_dirs.storage_dir, SWITCH_PATH_SEPARATOR, "signalwire-conf.dat");
	fp = fopen(confpath, "w");
	if (!fp) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Unable to open %s to save SignalWire SIP configuration\n", confpath);
		return;
	}

	fputs(config, fp);
	fclose(fp);
}

static void load_sip_config(void)
{
	char confpath[1024];
	char data[32767] = { 0 };
	FILE *fp = NULL;

	switch_snprintf(confpath, sizeof(confpath), "%s%s%s", SWITCH_GLOBAL_dirs.storage_dir, SWITCH_PATH_SEPARATOR, "signalwire-conf.dat");
	if (!(fp = fopen(confpath, "r"))) return;

	if (!fread(data, 1, sizeof(data), fp)) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Unable to read SignalWire SIP configuration from %s\n", confpath);
	}
	fclose(fp);
	if (!zstr_buf(data)) {
		switch_md5_string(globals.signalwire_profile_md5, (void *) data, strlen(data));
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "saved profile MD5 = \"%s\"\n", globals.signalwire_profile_md5);
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "saved profile = \"%s\"\n", (char *)data);
		globals.signalwire_profile = switch_xml_parse_str_dynamic((char *)data, SWITCH_TRUE);
	}
}

static ks_status_t load_credentials_from_json(ks_json_t *json)
{
	ks_status_t status = KS_STATUS_SUCCESS;
	ks_json_t *authentication = NULL;
	char *authentication_str = NULL;
	const char *bootstrap = NULL;
	const char *relay_connector_id = NULL;

#if SIGNALWIRE_CLIENT_C_VERSION_MAJOR >= 2
	if ((bootstrap = ks_json_get_string(json, "bootstrap")) == NULL) {
#else
	if ((bootstrap = ks_json_get_object_cstr(json, "bootstrap")) == NULL) {
#endif
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "Unable to connect to SignalWire: missing bootstrap URL\n");
		status = KS_STATUS_FAIL;
		goto done;
	}

#if SIGNALWIRE_CLIENT_C_VERSION_MAJOR >= 2
	if ((relay_connector_id = ks_json_get_string(json, "relay_connector_id")) == NULL) {
#else
	if ((relay_connector_id = ks_json_get_object_cstr(json, "relay_connector_id")) == NULL) {
#endif
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "Unable to connect to SignalWire: missing relay_connector_id\n");
		status = KS_STATUS_FAIL;
		goto done;
	}

	if ((authentication = ks_json_get_object_item(json, "authentication")) == NULL) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "Unable to connect to SignalWire: missing authentication\n");
		status = KS_STATUS_FAIL;
		goto done;
	}

	// update the internal connection target, which is normally assigned in swclt_sess_create()
	if (swclt_sess_target_set(globals.signalwire_session, bootstrap) != KS_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Unable to connect to SignalWire at %s\n", bootstrap);
		status = KS_STATUS_FAIL;
		goto done;
	}

	// update the relay_connector_id passed to profile configuration
	strncpy(globals.relay_connector_id, relay_connector_id, sizeof(globals.relay_connector_id) - 1);
	strncpy(globals.blade_bootstrap, bootstrap, sizeof(globals.blade_bootstrap) - 1);

	// got adopted, update the client config authentication
#if SIGNALWIRE_CLIENT_C_VERSION_MAJOR >= 2
	authentication_str = ks_json_print_unformatted(authentication);
#else
	authentication_str = ks_json_pprint_unformatted(NULL, authentication);
#endif
	swclt_config_set_authentication(globals.config, authentication_str);

#if SIGNALWIRE_CLIENT_C_VERSION_MAJOR >= 2
	switch_safe_free(authentication_str);
#else
	ks_pool_free(&authentication_str);
#endif
done:

	return status;
}

static ks_status_t mod_signalwire_adoption_post(void)
{
	ks_status_t status = KS_STATUS_SUCCESS;
	switch_memory_pool_t *pool = NULL;
	switch_CURL *curl = NULL;
	switch_curl_slist_t *headers = NULL;
	char url[1024];
	char errbuf[CURL_ERROR_SIZE];
	CURLcode res;
	long rescode;
	ks_json_t *json = ks_json_create_object();
	struct response_data rd = { 0 };
	char *jsonstr = NULL;

	// Determine and cache adoption data values that are heavier to figure out
	if (!globals.adoption_data_local_ip[0]) {
		switch_find_local_ip(globals.adoption_data_local_ip, sizeof(globals.adoption_data_local_ip), NULL, AF_INET);
	}

	if (!globals.adoption_data_external_ip[0]) {
		switch_port_t local_port = 6050;
		char *error = NULL;
		char *external_ip;
		switch_port_t external_port;

		if (switch_core_new_memory_pool(&pool) != SWITCH_STATUS_SUCCESS) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "SignalWire adoption failed: could not allocate memory pool\n");
			status = KS_STATUS_FAIL;
			goto done;
		}
		if (switch_find_available_port(&local_port, globals.adoption_data_local_ip, SOCK_STREAM) != SWITCH_STATUS_SUCCESS) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "SignalWire adoption failed: could not get available local port\n");
			status = KS_STATUS_FAIL;
			goto done;
		}

		external_ip = globals.adoption_data_local_ip;
		external_port = local_port;
		if (switch_stun_lookup(&external_ip, &external_port, globals.stun_server, globals.stun_port, &error, pool) != SWITCH_STATUS_SUCCESS) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "SignalWire adoption failed: stun [%s] lookup error: %s\n", globals.stun_server, error);
			status = KS_STATUS_FAIL;
			goto done;
		}
		snprintf(globals.adoption_data_external_ip, sizeof(globals.adoption_data_external_ip), "%s", external_ip);
	}

	if (!globals.adoption_data_uname[0]) {
#ifndef WIN32
		struct utsname buf;
		if (uname(&buf)) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "SignalWire adoption failed: could not get uname\n");
			status = KS_STATUS_FAIL;
			goto done;
		}
		switch_snprintf(globals.adoption_data_uname,
						sizeof(globals.adoption_data_uname),
						"%s %s %s %s %s",
						buf.sysname,
						buf.nodename,
						buf.release,
						buf.version,
						buf.machine);
#else
		// @todo set globals.adoption_data_uname from GetVersion Win32API
#endif
	}


	ks_json_add_string_to_object(json, "client_uuid", globals.adoption_token);
	ks_json_add_string_to_object(json, "hostname", switch_core_get_hostname());
	ks_json_add_string_to_object(json, "ip", globals.adoption_data_local_ip);
	ks_json_add_string_to_object(json, "ext_ip", globals.adoption_data_external_ip);
	ks_json_add_string_to_object(json, "version", switch_version_full());
	ks_json_add_string_to_object(json, "uname", globals.adoption_data_uname);

	jsonstr = ks_json_print_unformatted(json);
	ks_json_delete(&json);

	switch_snprintf(url, sizeof(url), "%s/%s", globals.adoption_service, globals.adoption_token);

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG10, "Checking %s for SignalWire adoption of this FreeSWITCH\n", url);

	curl = switch_curl_easy_init();

	headers = switch_curl_slist_append(headers, "Accept: application/json");
	headers = switch_curl_slist_append(headers, "Accept-Charset: utf-8");
	headers = switch_curl_slist_append(headers, "Content-Type: application/json");

	switch_curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 5);
	switch_curl_easy_setopt(curl, CURLOPT_TIMEOUT, 5);

	if (!strncasecmp(url, "https", 5)) {
		switch_curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, globals.ssl_verify);
		switch_curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, globals.ssl_verify);
	}

	switch_curl_easy_setopt(curl, CURLOPT_URL, url);
	switch_curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
	switch_curl_easy_setopt(curl, CURLOPT_USERAGENT, "mod_signalwire/1");
	switch_curl_easy_setopt(curl, CURLOPT_POSTFIELDS, jsonstr);
	switch_curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, errbuf);
	switch_curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&rd);
	switch_curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, response_data_handler);
#ifdef WIN32
	curl_easy_setopt(curl, CURLOPT_SSL_CTX_FUNCTION, sslContextFunction);
#endif

	if ((res = switch_curl_easy_perform(curl))) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Curl Result %d, Error: %s\n", res, errbuf);
		status = KS_STATUS_FAIL;
		goto done;
	}

	switch_curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &rescode);

	if (rescode == 404) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE,
				  "Go to https://signalwire.com to set up your Connector now! Enter connection token %s\n", globals.adoption_token);
		status = KS_STATUS_FAIL;
		goto done;
	}

	if (rescode != 200) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "SignalWire adoption failed with HTTP code %ld, %s\n", rescode, rd.data);
		status = KS_STATUS_FAIL;
		goto done;
	}

	json = ks_json_parse(rd.data);
	if (!json) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Received bad SignalWire adoption response\n%s\n", rd.data);
		status = KS_STATUS_FAIL;
		goto done;
	}

	if ((status = load_credentials_from_json(json)) != KS_STATUS_SUCCESS) {
		goto done;
	}

	ks_json_delete(&json);

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "SignalWire adoption of this FreeSWITCH completed\n");

	// write out the data to save it for reloading in the future
	{
		char authpath[1024];
		FILE *fp = NULL;

		switch_snprintf(authpath, sizeof(authpath), "%s%s%s", SWITCH_GLOBAL_dirs.storage_dir, SWITCH_PATH_SEPARATOR, "adoption-auth.dat");
		fp = fopen(authpath, "w");
		if (!fp) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Unable to open %s to save SignalWire creds\n", authpath);
			status = KS_STATUS_FAIL;
			goto done;
		}

		fputs(rd.data, fp);
		fclose(fp);
	}

	globals.state = SW_STATE_OFFLINE;
	swclt_sess_connect(globals.signalwire_session);

done:
	if (rd.data) ks_pool_free(&rd.data);
#if SIGNALWIRE_CLIENT_C_VERSION_MAJOR >= 2
	switch_safe_free(jsonstr);
#else
	if (jsonstr) ks_json_free_ex((void **)&jsonstr);
#endif
	if (json) ks_json_delete(&json);
	if (curl) {
		curl_easy_cleanup(curl);
		if (headers) curl_slist_free_all(headers);
	}
	if (pool) switch_core_destroy_memory_pool(&pool);
	return status;
}

#define SIGNALWIRE_SYNTAX "token | token-reset | adoption | adopted | reload | update | debug <level> | kslog <on|off|logfile e.g. /tmp/ks.log>"
SWITCH_STANDARD_API(mod_signalwire_api_function)
{
	char *argv[2] = { 0 };
	char *buf = NULL;


	if (!cmd || !(buf = strdup(cmd))) {
		stream->write_function(stream, "-USAGE: signalwire %s\n", SIGNALWIRE_SYNTAX);
		return SWITCH_STATUS_SUCCESS;
	}

    if (switch_separate_string(buf, ' ', argv, (sizeof(argv) / sizeof(argv[0])))) {
		if (!strcmp(argv[0], "token")) {
			if (globals.adoption_token[0]) {
				stream->write_function(stream,
					"     _____ _                   ___       ___\n"
					"    / ___/(_)___ _____  ____ _/ / |     / (_)_______\n"
					"    \\__ \\/ / __ `/ __ \\/ __ `/ /| | /| / / / ___/ _ \\\n"
					"   ___/ / / /_/ / / / / /_/ / / | |/ |/ / / /  /  __/\n"
					"  /____/_/\\__, /_/ /_/\\__,_/_/  |__/|__/_/_/   \\___/\n"
					"         /____/\n"
					"\n /=====================================================================\\\n"
					"  Connection Token: %s\n"
					" \\=====================================================================/\n"
					" Go to https://signalwire.com to set up your Connector now!\n", globals.adoption_token);
			} else {
				stream->write_function(stream, "-ERR connection token not available\n");
			}
			goto done;
		}
		else if (!strcmp(argv[0], "adoption")) {
			if (globals.state == SW_STATE_ADOPTION) {
				globals.adoption_next = ks_time_now();
				stream->write_function(stream, "+OK\n");
			} else {
				stream->write_function(stream, "-ERR adoption not currently pending\n");
			}
			goto done;
		}
		else if (!strcmp(argv[0], "adopted")) {
			stream->write_function(stream, "+OK %s\n", globals.state == SW_STATE_ADOPTION ? "Not Adopted" : "Adopted");
			goto done;
		}
		else if (!strcmp(argv[0], "debug")) {
			if (argv[1]) {
				debug_level = atoi(argv[1]);
			}

			stream->write_function(stream, "+OK debug %d\n", debug_level);
			goto done;
		} else if (!strcmp(argv[0], "kslog")) {
			if (argv[1]) {
				if (!strcmp(argv[1], "on")) {
					ks_global_set_logger(mod_signalwire_kslogger);
				} else if (!strcmp(argv[1], "off")) {
					ks_global_set_logger(NULL);
				}
			}

			stream->write_function(stream, "+OK %s\n", argv[1]);
			goto done;
		} else if (!strcmp(argv[0], "reload")) {
			globals.profile_reload = KS_TRUE;
			stream->write_function(stream, "+OK\n");
			goto done;
		} else if (!strcmp(argv[0], "update")) {
			globals.profile_update = KS_TRUE;
			stream->write_function(stream, "+OK\n");
			goto done;
        	} else if (!strcmp(argv[0], "token-reset")) {
			char tmp[1024];
			
			switch_snprintf(tmp, sizeof(tmp), "%s%s%s", SWITCH_GLOBAL_dirs.storage_dir, SWITCH_PATH_SEPARATOR, "adoption-token.dat");
			if (switch_file_exists(tmp, NULL) == SWITCH_STATUS_SUCCESS) {
				if (unlink(tmp)) {
					stream->write_function(stream, "-ERR Could not delete the old adoption-token.dat file. Token was not re-generated.\n");
					goto done;
				}
			}

			switch_snprintf(tmp, sizeof(tmp), "%s%s%s", SWITCH_GLOBAL_dirs.storage_dir, SWITCH_PATH_SEPARATOR, "adoption-auth.dat");
			if (switch_file_exists(tmp, NULL) == SWITCH_STATUS_SUCCESS) {
				if (unlink(tmp)) {
					stream->write_function(stream, "-ERR Could not delete the old adoption-auth.dat file. Token was not re-generated.\n");
					goto done;
				}
			}

			switch_snprintf(tmp, sizeof(tmp), "%s%s%s", SWITCH_GLOBAL_dirs.storage_dir, SWITCH_PATH_SEPARATOR, "signalwire-conf.dat");
			if (switch_file_exists(tmp, NULL) == SWITCH_STATUS_SUCCESS) {
				if (unlink(tmp)) {
					stream->write_function(stream, "-ERR Could not delete the old signalwire-conf.dat file. Token was not re-generated.\n");
					goto done;
				}
			}

			if (mod_signalwire_load_or_generate_token() != SWITCH_STATUS_SUCCESS) {
				stream->write_function(stream, "-ERR Could not generate a new token.\n");
			} else {
				globals.state = SW_STATE_ADOPTION;
				globals.adoption_next = ks_time_now();
				stream->write_function(stream, "+OK\n");
			}
			
			goto done;
		}
	}

	stream->write_function(stream, "-USAGE: signalwire %s\n", SIGNALWIRE_SYNTAX);
	
done:
	switch_safe_free(buf);
	
	return SWITCH_STATUS_SUCCESS;
}

#if SIGNALWIRE_CLIENT_C_VERSION_MAJOR >= 2
static void mod_signalwire_session_state_handler(swclt_sess_t *sess, void *cb_data)
{
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "SignalWire Session State Change: %s\n", swclt_sess_state_str(sess->state));

	if (sess->state != SWCLT_STATE_OFFLINE) {
		// Connected with NEW or RESTORED session
		globals.signalwire_reconnected = KS_TRUE;
	} else {
		// Disconnected
	}
}
#else
static void mod_signalwire_session_state_handler(swclt_sess_t sess, swclt_hstate_change_t *state_change_info, const char *cb_data)
{
	SWCLT_HSTATE new_state = state_change_info->new_state;

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "SignalWire Session State Change: %s\n", swclt_hstate_describe_change(state_change_info));

	if (new_state == SWCLT_HSTATE_ONLINE) {
		// Connected with NEW or RESTORED session
		globals.signalwire_reconnected = KS_TRUE;
	} else if (new_state == SWCLT_HSTATE_OFFLINE) {
		// Disconnected
	}
}
#endif

static void __on_provisioning_events(
#if SIGNALWIRE_CLIENT_C_VERSION_MAJOR >= 2
swclt_sess_t *sess, 
#else
swclt_sess_t sess, 
#endif
blade_broadcast_rqu_t *rqu, void *cb_data)
{
	if (!strcmp(rqu->event, "update")) {
		globals.profile_update = KS_TRUE;
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "SignalWire SIP profile update requested\n");
	}
}

static switch_xml_t xml_config_handler(const char *section, const char *tag_name, const char *key_name, const char *key_value, switch_event_t *params,
									   void *user_data)
{
	char *profileName = NULL;
	char *reconfigValue = NULL;
	switch_xml_t signalwire_profile_dup = NULL;
	
	if (!section || strcmp(section, "configuration")) return NULL;
	if (!key_name || strcmp(key_name, "name")) return NULL;
	if (!key_value || strcmp(key_value, "sofia.conf")) return NULL;
	if (!params) return NULL;
	profileName = switch_event_get_header(params, "profile");
	if (!profileName || strcmp(profileName, "signalwire")) return NULL;
	reconfigValue = switch_event_get_header(params, "reconfig");
	if (!reconfigValue || strcmp(reconfigValue, "true")) return NULL;

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Received XML lookup for SignalWire SIP profile\n");

	if (globals.signalwire_profile) {
		signalwire_profile_dup = switch_xml_dup(globals.signalwire_profile);
	}
	return signalwire_profile_dup;
}

static switch_status_t mod_signalwire_load_or_generate_token(void)
{
	switch_status_t status = SWITCH_STATUS_SUCCESS;
	char tokenpath[1024];
	
	switch_snprintf(tokenpath, sizeof(tokenpath), "%s%s%s", SWITCH_GLOBAL_dirs.storage_dir, SWITCH_PATH_SEPARATOR, "adoption-token.dat");
	if (switch_file_exists(tokenpath, NULL) != SWITCH_STATUS_SUCCESS) {
		// generate first time uuid
		ks_uuid_t uuid;
		const char *token;
		FILE *fp = fopen(tokenpath, "w");
		if (!fp) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Unable to open %s to save SignalWire connection token\n", tokenpath);
			status = SWITCH_STATUS_TERM;
			goto done;
		}

		ks_uuid(&uuid);
		token = ks_uuid_str(NULL, &uuid);

		fputs(token, fp);
		fclose(fp);
		
		strncpy(globals.adoption_token, token, sizeof(globals.adoption_token) - 1);
		
		ks_pool_free(&token);
	} else {
		char token[64];
		FILE *fp = fopen(tokenpath, "r");
		if (!fp) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Unable to open %s to read SignalWire connection token\n", tokenpath);
			status = SWITCH_STATUS_TERM;
			goto done;
		}
		if (!fgets(token, sizeof(token), fp)) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Unable to read SignalWire connection token from %s\n", tokenpath);
			fclose(fp);
			status = SWITCH_STATUS_TERM;
			goto done;
		}
		fclose(fp);

		// trim newline markers in case they exist, only want the token
		for (size_t len = strlen(token); len > 0 && (token[len - 1] == '\r' || token[len - 1] == '\n'); --len) {
			token[len - 1] = '\0';
		}

		snprintf(globals.adoption_token, sizeof(globals.adoption_token), "%s", token);
	}

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO,
					  "\n /=====================================================================\\\n"
					  "| Connection Token: %s               |\n"
					  " \\=====================================================================/\n"
					  " Go to https://signalwire.com to set up your Connector now!\n", globals.adoption_token);

done:
	return status;
}

static switch_status_t load_config()
{
	char *cf = "signalwire.conf";
	switch_xml_t cfg, xml;
	const char *data;

	globals.ssl_verify = 1;
	switch_set_string(globals.blade_bootstrap, "edge.<space>.signalwire.com/api/relay/wss");
	switch_set_string(globals.adoption_service, "https://adopt.signalwire.com/adoption");
	switch_set_string(globals.stun_server, "stun.freeswitch.org");
	globals.stun_port = SWITCH_STUN_DEFAULT_PORT;
	
	if (!(xml = switch_xml_open_cfg(cf, &cfg, NULL))) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG1, "open of %s failed\n", cf);
		// don't need the config
	} else {
		switch_xml_t settings, param, tmp;
		if ((settings = switch_xml_child(cfg, "settings"))) {
			for (param = switch_xml_child(settings, "param"); param; param = param->next) {
				char *var = (char *) switch_xml_attr_soft(param, "name");
				char *val = (char *) switch_xml_attr_soft(param, "value");

				if (!strcasecmp(var, "kslog") && !ks_zstr(val)) {
					if (!strcmp(val, "off")) {
						globals.kslog_on = KS_FALSE;
					} else if (!strcmp(val, "on")) {
						globals.kslog_on = KS_TRUE;
					}
				} else if (!strcasecmp(var, "blade-bootstrap") && !ks_zstr(val)) {
					switch_set_string(globals.blade_bootstrap, val);
				} else if (!strcasecmp(var, "adoption-service") && !ks_zstr(val)) {
					switch_set_string(globals.adoption_service, val);
				} else if (!strcasecmp(var, "stun-server") && !ks_zstr(val)) {
					char *p, *ss = strdup(val);
					
					if ((p = strchr(ss, ':'))) {
						int port;
						*p++ = '\0';

						port = atoi(p);
						if (port > 0 && port < 65536) {
							globals.stun_port = port;
						}
					}
					
					switch_set_string(globals.stun_server, ss);
					switch_safe_free(ss);
				} else if (!strcasecmp(var, "ssl-verify")) {
					globals.ssl_verify = switch_true(val) ? 1 : 0;
				} else if (!strcasecmp(var, "override-context") && !ks_zstr(val)) {
					switch_set_string(globals.override_context, val);
				}
			}
			if ((tmp = switch_xml_child(settings, "authentication"))) {
				const char *txt = switch_xml_txt(tmp);
				if (!ks_zstr(txt)) {
					swclt_config_set_authentication(globals.config, txt);
				}
			}
		}
		switch_xml_free(xml);
	}

	if ((data = getenv("SW_BLADE_BOOTSTRAP"))) {
		switch_set_string(globals.blade_bootstrap, data);
	}

	if ((data = getenv("SW_ADOPTION_SERVICE"))) {
	        snprintf(globals.adoption_service, sizeof(globals.adoption_service), "%s", data);
	}

	swclt_config_load_from_env(globals.config);

	return SWITCH_STATUS_SUCCESS;
}

static ks_status_t load_credentials(void)
{
	ks_status_t status = KS_STATUS_SUCCESS;
	char authpath[1024];
	char data[2048];
	FILE *fp = NULL;
	ks_json_t *json = NULL;
	
	switch_snprintf(authpath, sizeof(authpath), "%s%s%s", SWITCH_GLOBAL_dirs.storage_dir, SWITCH_PATH_SEPARATOR, "adoption-auth.dat");
	if (!(fp = fopen(authpath, "r"))) goto done;

	if (!fgets(data, sizeof(data), fp)) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Unable to read SignalWire authentication data from %s\n", authpath);
		fclose(fp);
		status = KS_STATUS_FAIL;
		goto done;
	}
	fclose(fp);

	json = ks_json_parse(data);
	if (!json) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Unable to parse SignalWire authentication data from %s\n", authpath);
		status = KS_STATUS_FAIL;
		goto done;
	}
	status = load_credentials_from_json(json);
	ks_json_delete(&json);

done:
	return status;
}

#if SIGNALWIRE_CLIENT_C_VERSION_MAJOR >= 2
static void mod_signalwire_session_auth_failed_handler(swclt_sess_t *sess)
#else
static void mod_signalwire_session_auth_failed_handler(swclt_sess_t sess)
#endif
{
	char path[1024];

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "SignalWire authentication failed\n");

	switch_snprintf(path, sizeof(path), "%s%s%s", SWITCH_GLOBAL_dirs.storage_dir, SWITCH_PATH_SEPARATOR, "adoption-auth.dat");
	unlink(path);

	switch_snprintf(path, sizeof(path), "%s%s%s", SWITCH_GLOBAL_dirs.storage_dir, SWITCH_PATH_SEPARATOR, "signalwire-conf.dat");
	unlink(path);

	globals.restarting = KS_TRUE;

	globals.adoption_backoff = 0;
	globals.adoption_next = 0;

	globals.state = SW_STATE_ADOPTION;
}

/* Dialplan INTERFACE */
SWITCH_STANDARD_DIALPLAN(dialplan_hunt)
{
	switch_caller_extension_t *extension = NULL;
	switch_channel_t *channel = switch_core_session_get_channel(session);
	const char *network_ip = switch_channel_get_variable(channel, "sip_network_ip");
	const char *network_port = switch_channel_get_variable(channel, "sip_network_port");

	if (!caller_profile) {
		if (!(caller_profile = switch_channel_get_caller_profile(channel))) {
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "Error obtaining caller profile!\n");
			goto done;
		}
	}

	if (globals.override_context[0] != '\0') {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_INFO, "Overriding dialplan context from %s to %s\n",caller_profile->context,globals.override_context);
		caller_profile->context = globals.override_context;
	}

	switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_INFO, "Processing %s <%s>->%s in context %s\n",
					  caller_profile->caller_id_name, caller_profile->caller_id_number, caller_profile->destination_number, caller_profile->context);

	if ((extension = switch_caller_extension_new(session, "signalwire", caller_profile->destination_number)) == NULL) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_CRIT, "Memory Error!\n");
		goto done;
	}

	switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "call from %s:%s\n", network_ip, network_port);

	switch_mutex_lock(globals.mutex);

	if (network_ip &&
		!zstr_buf(globals.gateway_ip) && !strcmp(globals.gateway_ip, network_ip)) {
			// good to go
			char transfer_to[1024];

			switch_snprintf(transfer_to, sizeof(transfer_to), "%s %s %s", caller_profile->destination_number, "XML", caller_profile->context);
			switch_caller_extension_add_application(session, extension, "transfer", transfer_to);
	} else {
		switch_caller_extension_add_application(session, extension, "respond", "500");
	}

	switch_mutex_unlock(globals.mutex);

done:
	return extension;
}

/**
 * Module load or unload callback from core
 * @param event the event
 */
static void on_module_load_unload(switch_event_t *event)
{
	const char *type = switch_event_get_header(event, "type");
	const char *name = switch_event_get_header(event, "name");
	if (!zstr(type) && !zstr(name) && !strcmp(type, "endpoint") && !strcmp(name, "sofia")) {
		globals.profile_reload = KS_TRUE;
	}
}

/**
 * Sofia sofia::gateway_state change callback
 * @param event the event
 */
static void on_sofia_gateway_state(switch_event_t *event)
{
	const char *ip = switch_event_get_header(event, "Register-Network-IP");
	const char *port = switch_event_get_header(event, "Register-Network-Port");
	const char *state = switch_event_get_header(event, "State");
	const char *gateway = switch_event_get_header(event, "Gateway");

	if (!ip || !port || !state || !gateway) {
		return;
	}

	if (!strcmp(gateway, "signalwire")) {
		switch_mutex_lock(globals.mutex);

		if (!strcmp(state, "REGED")) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "SignalWire SIP Gateway registered to %s:%s\n", ip, port);
			switch_set_string(globals.gateway_ip, ip);
			switch_set_string(globals.gateway_port, port);
		} else if (!strcmp(state, "DOWN")) {
			globals.gateway_ip[0] = '\0';
			globals.gateway_port[0] = '\0';
		}

		switch_mutex_unlock(globals.mutex);
	}
}

/* Macro expands to: switch_status_t mod_signalwire_load(switch_loadable_module_interface_t **module_interface, switch_memory_pool_t *pool) */
SWITCH_MODULE_LOAD_FUNCTION(mod_signalwire_load)
{
	switch_api_interface_t *api_interface = NULL;
	switch_dialplan_interface_t *dialplan_interface;
	const char *kslog_env = NULL;
	switch_status_t status = SWITCH_STATUS_SUCCESS;

	memset(&globals, 0, sizeof(globals));

	kslog_env = getenv("KSLOG");
	if (kslog_env && kslog_env[0] && kslog_env[0] != '0') globals.kslog_on = KS_TRUE;

	/* connect my internal structure to the blank pointer passed to me */
	*module_interface = switch_loadable_module_create_module_interface(pool, modname);

	ks_global_set_logger(mod_signalwire_kslogger);

	SWITCH_ADD_API(api_interface, "signalwire", "SignalWire API", mod_signalwire_api_function, SIGNALWIRE_SYNTAX);
	switch_console_set_complete("add signalwire debug");
	switch_console_set_complete("add signalwire debug 1");
	switch_console_set_complete("add signalwire debug 2");
	switch_console_set_complete("add signalwire debug 3");
	switch_console_set_complete("add signalwire debug 4");
	switch_console_set_complete("add signalwire debug 5");
	switch_console_set_complete("add signalwire debug 6");
	switch_console_set_complete("add signalwire debug 7");
	switch_console_set_complete("add signalwire kslog");
	switch_console_set_complete("add signalwire kslog on");
	switch_console_set_complete("add signalwire kslog off");
	switch_console_set_complete("add signalwire token");
	switch_console_set_complete("add signalwire token-reset");
	switch_console_set_complete("add signalwire adoption");
	switch_console_set_complete("add signalwire adopted");
	switch_console_set_complete("add signalwire update");
	switch_console_set_complete("add signalwire reload");

	switch_xml_bind_search_function(xml_config_handler, SWITCH_XML_SECTION_CONFIG, NULL);

	ks_ssl_init_skip(KS_TRUE);

	swclt_init(KS_LOG_LEVEL_DEBUG);

	if (globals.kslog_on == KS_FALSE) {
		ks_global_set_logger(NULL);
	} else {
		ks_global_set_logger(mod_signalwire_kslogger);
	}
	
#ifdef WIN32
	sslLoadWindowsCACertificate();
#endif

	// Configuration
	swclt_config_create(&globals.config);
	load_config();

	switch_mutex_init(&globals.mutex, SWITCH_MUTEX_NESTED, pool);

	switch_event_bind("mod_signalwire", SWITCH_EVENT_MODULE_LOAD, NULL, on_module_load_unload, NULL);
	switch_event_bind("mod_signalwire", SWITCH_EVENT_MODULE_UNLOAD, NULL, on_module_load_unload, NULL);
	switch_event_bind("mod_signalwire", SWITCH_EVENT_CUSTOM, "sofia::gateway_state", on_sofia_gateway_state, NULL);

	SWITCH_ADD_DIALPLAN(dialplan_interface, "signalwire", dialplan_hunt);

	// Load credentials if they exist from a prior adoption
	load_credentials();

	// SignalWire
	swclt_sess_create(&globals.signalwire_session,
					  globals.blade_bootstrap,
					  globals.config);
	swclt_sess_set_auth_failed_cb(globals.signalwire_session, mod_signalwire_session_auth_failed_handler);

	if (!globals.signalwire_session) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "signalwire_session create error\n");
		switch_goto_status(SWITCH_STATUS_TERM, err);
	}

#if SIGNALWIRE_CLIENT_C_VERSION_MAJOR >= 2
	swclt_sess_set_state_change_cb(globals.signalwire_session, mod_signalwire_session_state_handler, NULL);
#else
	swclt_hmon_register(&globals.signalwire_session_monitor, globals.signalwire_session, mod_signalwire_session_state_handler, NULL);
#endif

	// @todo register nodestore callbacks here if needed

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CONSOLE, "Welcome to\n"
	"     _____ _                   ___       ___\n"
	"    / ___/(_)___ _____  ____ _/ / |     / (_)_______\n"
	"    \\__ \\/ / __ `/ __ \\/ __ `/ /| | /| / / / ___/ _ \\\n"
	"   ___/ / / /_/ / / / / /_/ / / | |/ |/ / / /  /  __/\n"
	"  /____/_/\\__, /_/ /_/\\__,_/_/  |__/|__/_/_/   \\___/\n"
	"         /____/\n");

	// storage_dir was missing in clean install
	switch_dir_make_recursive(SWITCH_GLOBAL_dirs.storage_dir, SWITCH_DEFAULT_DIR_PERMS, pool);

	if ((status = mod_signalwire_load_or_generate_token()) != SWITCH_STATUS_SUCCESS) {
		goto err;
	}

	if (swclt_sess_has_authentication(globals.signalwire_session)) {
		// Load cached profile if we already have one.  We'll still connect to SignalWire and
		// fetch a new profile in the background.
		load_sip_config();
		if (globals.signalwire_profile) {
			globals.state = SW_STATE_START_PROFILE;
		} else {
			globals.state = SW_STATE_OFFLINE;
		}
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CONSOLE, "Connecting to SignalWire\n");
		swclt_sess_connect(globals.signalwire_session);
	} else {
		globals.state = SW_STATE_ADOPTION;
	}

	goto done;

err:
#if SIGNALWIRE_CLIENT_C_VERSION_MAJOR >= 2
	if (globals.signalwire_session) swclt_sess_destroy(&globals.signalwire_session);
#else
	if (globals.signalwire_session) ks_handle_destroy(&globals.signalwire_session);
#endif
	swclt_config_destroy(&globals.config);
	ks_global_set_logger(NULL);

done:
	
	return status;
}

/*
  Called when the system shuts down
  Macro expands to: switch_status_t mod_signalwire_shutdown() */
SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_signalwire_shutdown)
{
	/* Cleanup dynamically allocated config settings */

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "Disconnecting from SignalWire\n");

	switch_event_unbind_callback(on_module_load_unload);
	switch_event_unbind_callback(on_sofia_gateway_state);

	// stop things that might try to use blade or kafka while they are shutting down
	globals.shutdown = KS_TRUE;

	swclt_sess_disconnect(globals.signalwire_session);
#if SIGNALWIRE_CLIENT_C_VERSION_MAJOR >= 2
	while (globals.signalwire_session->state == SWCLT_STATE_ONLINE) {
#else
	while (swclt_hstate_current_get(globals.signalwire_session) == SWCLT_HSTATE_ONLINE) {
#endif
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Sleeping for pending disconnect\n");
		ks_sleep_ms(1000);
	}
	
	//signalwire_dialplan_shutdown();
	// @todo signalwire profile unbinding and unloading
	switch_xml_unbind_search_function_ptr(xml_config_handler);
	
	// kill signalwire, so nothing more can come into the system
#if SIGNALWIRE_CLIENT_C_VERSION_MAJOR >= 2
	swclt_sess_destroy(&globals.signalwire_session);
#else
	ks_handle_destroy(&globals.signalwire_session);
#endif
	// cleanup config
	swclt_config_destroy(&globals.config);

	// shutdown libblade (but not libks?)
	swclt_shutdown();

#ifdef WIN32
	// free certificate pointers previously loaded
	sslUnLoadWindowsCACertificate();
#endif

	return SWITCH_STATUS_SUCCESS;
}

static void mod_signalwire_state_adoption(void)
{
	// keep trying to check adoption token for authentication
	if (ks_time_now() >= globals.adoption_next) {
		// Use a very very simple backoff algorithm, every time we try, backoff another minute
		// so that after first try we wait 1 minute, after next try we wait 2 minutes, at third
		// try we are waiting 3 minutes, upto a max backoff of 15 minutes between adoption checks
		if (globals.adoption_backoff < 15) globals.adoption_backoff++;
		globals.adoption_next = ks_time_now() + (globals.adoption_backoff * 60 * KS_USEC_PER_SEC);
		if (mod_signalwire_adoption_post() != KS_STATUS_SUCCESS) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "Next SignalWire adoption check in %"SWITCH_SIZE_T_FMT" minutes\n", globals.adoption_backoff);
		}
	}
	if (globals.signalwire_reconnected) {
		// OK to continue as is
		globals.signalwire_reconnected = KS_FALSE;
	}
}

static void mod_signalwire_state_offline(void)
{
	if (globals.signalwire_reconnected) {
		globals.signalwire_reconnected = KS_FALSE;
		globals.state = SW_STATE_ONLINE;
	}
}

static void mod_signalwire_state_online(void)
{
	globals.signalwire_reconnected = KS_FALSE;
	if (!swclt_sess_provisioning_setup(globals.signalwire_session, __on_provisioning_events, NULL)) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "Connected to SignalWire\n");
		globals.state = SW_STATE_CONFIGURE;
	} else {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Failed to connect to SignalWire\n");
		ks_sleep_ms(4000);
		globals.state = SW_STATE_OFFLINE;
		globals.restarting = KS_TRUE;
	}
}

static void mod_signalwire_state_configure(void)
{
	switch_memory_pool_t *pool = NULL;
	char local_ip[64];
	switch_port_t local_port = 6050;
	char local_endpoint[256];
	char *external_ip;
	switch_port_t external_port;
	char external_endpoint[256];
	char *error = NULL;
#if SIGNALWIRE_CLIENT_C_VERSION_MAJOR >= 2
	swclt_cmd_reply_t *reply = NULL;
#else
	swclt_cmd_t cmd;
#endif

	if (globals.signalwire_reconnected) {
		globals.signalwire_reconnected = KS_FALSE;
		globals.state = SW_STATE_ONLINE;
	}

	// already restarting/updating...
	globals.profile_reload = KS_FALSE;
	globals.profile_update = KS_FALSE;

	if (switch_core_new_memory_pool(&pool) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "SignalWire configure failed: could not allocate memory pool\n");
		goto done;
	}

	switch_find_local_ip(local_ip, sizeof(local_ip), NULL, AF_INET);

	if (switch_find_available_port(&local_port, local_ip, SOCK_STREAM) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "SignalWire configure failed: could not get available local port\n");
		ks_sleep_ms(4000);
		goto done;
	}

	snprintf(local_endpoint, sizeof(local_endpoint), "%s:%u", local_ip, local_port);

	external_ip = local_ip;
	external_port = local_port;

	if (switch_stun_lookup(&external_ip, &external_port, globals.stun_server, globals.stun_port, &error, pool) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "SignalWire configure failed: stun [%s] lookup error: %s\n", globals.stun_server, error);
		ks_sleep_ms(4000);
		goto done;
	}

	snprintf(external_endpoint, sizeof(external_endpoint), "%s:%u", external_ip, external_port);

#if SIGNALWIRE_CLIENT_C_VERSION_MAJOR >= 2
	if (!swclt_sess_provisioning_configure(globals.signalwire_session, "freeswitch", local_endpoint, external_endpoint, globals.relay_connector_id, &reply)) {
		if (reply->type == SWCLT_CMD_TYPE_RESULT) {
#else
	if (!swclt_sess_provisioning_configure(globals.signalwire_session, "freeswitch", local_endpoint, external_endpoint, globals.relay_connector_id, &cmd)) {
		SWCLT_CMD_TYPE cmd_type;
		swclt_cmd_type(cmd, &cmd_type);
		if (cmd_type == SWCLT_CMD_TYPE_RESULT) {
			const ks_json_t *result;
#endif
			signalwire_provisioning_configure_response_t *configure_res;
#if SIGNALWIRE_CLIENT_C_VERSION_MAJOR >= 2
			if (!SIGNALWIRE_PROVISIONING_CONFIGURE_RESPONSE_PARSE(reply->pool, reply->json, &configure_res)) {
#else
			swclt_cmd_result(cmd, &result);
			result = ks_json_get_object_item(result, "result");
			if (!SIGNALWIRE_PROVISIONING_CONFIGURE_RESPONSE_PARSE(ks_handle_pool(cmd), result, &configure_res)) {
#endif
				ks_json_t *configuration = configure_res->configuration;
#if SIGNALWIRE_CLIENT_C_VERSION_MAJOR >= 2
				const char *configuration_profile = ks_json_get_string(configuration, "profile");
#else
				const char *configuration_profile = ks_json_get_object_cstr(configuration, "profile");
#endif
				if (globals.signalwire_profile) {
					switch_xml_free(globals.signalwire_profile);
					globals.signalwire_profile = NULL;
				}
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "\"%s\"\n", configuration_profile);
				globals.signalwire_profile = switch_xml_parse_str_dynamic((char *)configuration_profile, SWITCH_TRUE);
				if (!globals.signalwire_profile) {
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Failed to parse configuration profile\n");
				} else {
					char digest[SWITCH_MD5_DIGEST_STRING_SIZE] = { 0 };
					switch_md5_string(digest, (void *) configuration_profile, strlen(configuration_profile));
					save_sip_config(configuration_profile);
					if (!signalwire_gateway_exists() || zstr_buf(globals.signalwire_profile_md5) || strcmp(globals.signalwire_profile_md5, digest)) {
						// not registered or new profile - update md5 and load it
						strcpy(globals.signalwire_profile_md5, digest);
						globals.state = SW_STATE_START_PROFILE;
						switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "profile MD5 = \"%s\"\n", globals.signalwire_profile_md5);
					} else {
						// already registered
						globals.state = SW_STATE_READY;
					}
				}
			}
		}
	}
#if SIGNALWIRE_CLIENT_C_VERSION_MAJOR >= 2
	swclt_cmd_reply_destroy(&reply);
#else
	ks_handle_destroy(&cmd);
#endif
	if (globals.state == SW_STATE_CONFIGURE) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "Failed to receive valid configuration from SignalWire\n");
		ks_sleep_ms(4000);
	} else {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "Received configuration from SignalWire\n");
	}

done:
	if (pool) switch_core_destroy_memory_pool(&pool);
}

static int signalwire_gateway_exists(void)
{
	int exists = 0;
	switch_stream_handle_t stream = { 0 };
	SWITCH_STANDARD_STREAM(stream);
	if (switch_api_execute("sofia", "profile signalwire gwlist", NULL, &stream) == SWITCH_STATUS_SUCCESS && stream.data) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "gwlist = \"%s\"\n", (char *)stream.data);
		exists = (strstr((char *)stream.data, "Invalid Profile") == NULL) &&
			(strstr((char*)stream.data, "signalwire") != NULL);
	}
	switch_safe_free(stream.data);
	return exists;
}

static int signalwire_profile_is_started(void)
{
	int started = 0;
	switch_stream_handle_t stream = { 0 };
	SWITCH_STANDARD_STREAM(stream);
	if (switch_api_execute("sofia", "status profile signalwire", NULL, &stream) == SWITCH_STATUS_SUCCESS && stream.data) {
		started = (strstr((char *)stream.data, "Invalid Profile") == NULL) &&
			(strstr((char *)stream.data, "signalwire") != NULL);
	}
	switch_safe_free(stream.data);
	return started;
}

static int signalwire_profile_rescan(void)
{
	int success = 0;
	switch_stream_handle_t stream = { 0 };
	SWITCH_STANDARD_STREAM(stream);
	if (switch_api_execute("sofia", "profile signalwire rescan", NULL, &stream) == SWITCH_STATUS_SUCCESS) {
		success = signalwire_profile_is_started();
	}
	switch_safe_free(stream.data);
	return success;
}

static int signalwire_profile_start(void)
{
	int success = 0;
	switch_stream_handle_t stream = { 0 };
	SWITCH_STANDARD_STREAM(stream);
	if (switch_api_execute("sofia", "profile signalwire start", NULL, &stream) == SWITCH_STATUS_SUCCESS) {
		success = signalwire_profile_is_started();
	}
	switch_safe_free(stream.data);
	return success;
}

static void signalwire_profile_killgw(void)
{
	switch_stream_handle_t stream = { 0 };
	SWITCH_STANDARD_STREAM(stream);
	switch_api_execute("sofia", "profile signalwire killgw signalwire", NULL, &stream);
	switch_safe_free(stream.data);
}

static void mod_signalwire_state_start_profile(void)
{
	if (globals.profile_update) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "SignalWire SIP profile update initiated\n");
		globals.state = SW_STATE_CONFIGURE;
		globals.profile_update = KS_FALSE;
		return;
	}
	globals.profile_reload = KS_FALSE; // already here

	// ignore SignalWire reconnections until register is attempted

	if (switch_loadable_module_exists("mod_sofia") != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Waiting for mod_sofia to load\n");
	} else if (signalwire_profile_is_started()) {
		// kill gateway if already up and rescan the profile
		if (signalwire_gateway_exists()) {
			signalwire_profile_killgw();
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "SignalWire SIP gateway killed\n");
		}
		if (signalwire_profile_rescan()) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "SignalWire SIP profile rescanned\n");
			globals.state = SW_STATE_REGISTER;
		}
	} else {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Starting SignalWire SIP profile\n");
		signalwire_profile_start(); // assume success - it gets checked in next state
		globals.state = SW_STATE_REGISTER;
	}
}

static void mod_signalwire_state_register(void)
{
	if (globals.profile_update) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "SignalWire SIP profile update initiated\n");
		globals.state = SW_STATE_CONFIGURE;
		globals.profile_update = KS_FALSE;
		return;
	} else if (globals.profile_reload) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "SignalWire SIP profile reload initiated\n");
		globals.state = SW_STATE_START_PROFILE;
		globals.profile_reload = KS_FALSE;
		return;
	}
	// ignore SignalWire reconnections until register is attempted

	if (switch_loadable_module_exists("mod_sofia") != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Waiting for mod_sofia to load\n");
		globals.state = SW_STATE_START_PROFILE;
	} else if (signalwire_gateway_exists() || signalwire_profile_rescan()) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "SignalWire SIP gateway started\n");
		globals.state = SW_STATE_READY;
	} else {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "Failed to start SignalWire SIP gateway\n");
		globals.state = SW_STATE_CONFIGURE;
		ks_sleep_ms(5000);
	}
}

static void mod_signalwire_state_ready()
{
	if (globals.profile_update) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "Signalwire SIP profile update initiated\n");
		globals.state = SW_STATE_CONFIGURE;
		globals.profile_update = KS_FALSE;
	} else if (globals.profile_reload) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "SignalWire SIP profile reload initiated\n");
		globals.state = SW_STATE_START_PROFILE;
		globals.profile_reload = KS_FALSE;
	} else if (globals.signalwire_reconnected) {
		globals.signalwire_reconnected = KS_FALSE;
		globals.state = SW_STATE_ONLINE;
	}
}

SWITCH_MODULE_RUNTIME_FUNCTION(mod_signalwire_runtime)
{
	while (!globals.shutdown) {
		if (globals.restarting) {
			swclt_sess_disconnect(globals.signalwire_session);
#if SIGNALWIRE_CLIENT_C_VERSION_MAJOR >= 2
			while (globals.signalwire_session->state == SWCLT_STATE_ONLINE) {
#else
			while (swclt_hstate_current_get(globals.signalwire_session) == SWCLT_HSTATE_ONLINE) {
#endif
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Sleeping for pending disconnect\n");
				ks_sleep_ms(1000);
			}

			// kill signalwire, so nothing more can come into the system
#if SIGNALWIRE_CLIENT_C_VERSION_MAJOR >= 2
			swclt_sess_destroy(&globals.signalwire_session);
#else
			ks_handle_destroy(&globals.signalwire_session);
#endif

			// Create a new session and start over
			swclt_sess_create(&globals.signalwire_session,
					  globals.blade_bootstrap,
					  globals.config);
			swclt_sess_set_auth_failed_cb(globals.signalwire_session, mod_signalwire_session_auth_failed_handler);

#if SIGNALWIRE_CLIENT_C_VERSION_MAJOR >= 2
			swclt_sess_set_state_change_cb(globals.signalwire_session, mod_signalwire_session_state_handler, NULL);
#else
			swclt_hmon_register(&globals.signalwire_session_monitor, globals.signalwire_session, mod_signalwire_session_state_handler, NULL);
#endif

			globals.restarting = KS_FALSE;
			continue;
		}

		switch(globals.state) {
		case SW_STATE_ADOPTION: // waiting for adoption to occur
			mod_signalwire_state_adoption();
			break;
		case SW_STATE_OFFLINE: // waiting for session to go online
			mod_signalwire_state_offline();
			break;
		case SW_STATE_ONLINE: // provisioning service setup
			mod_signalwire_state_online();
			break;
		case SW_STATE_CONFIGURE: // provisioning configuration
			mod_signalwire_state_configure();
			break;
		case SW_STATE_START_PROFILE:
			mod_signalwire_state_start_profile();
			break;
		case SW_STATE_REGISTER:
			mod_signalwire_state_register();
			break;
		case SW_STATE_READY: // ready for runtime
			mod_signalwire_state_ready();
			break;
		default: break;
		}
		ks_sleep_ms(1000);
	}

	return SWITCH_STATUS_TERM;
}

/* For Emacs:
 * Local Variables:
 * mode:c
 * indent-tabs-mode:t
 * tab-width:4
 * c-basic-offset:4
 * End:
 * For VIM:
 * vim:set softtabstop=4 shiftwidth=4 tabstop=4 noet
 */
