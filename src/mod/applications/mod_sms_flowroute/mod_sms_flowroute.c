/*
* FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
* Copyright (C) 2005-2015, Anthony Minessale II <anthm@freeswitch.org>
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
* William King <william.king@quentustech.com>
*
* mod_sms_flowroute.c SMS support for Flowroute SMS
*
*/

#include "mod_sms_flowroute.h"

SWITCH_MODULE_LOAD_FUNCTION(mod_sms_flowroute_load);
SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_sms_flowroute_shutdown);
SWITCH_MODULE_DEFINITION(mod_sms_flowroute, mod_sms_flowroute_load, mod_sms_flowroute_shutdown, NULL);

static mod_sms_flowroute_globals_t mod_sms_flowroute_globals;

static void on_accept(h2o_socket_t *listener, const char *error)
{
	mod_sms_flowroute_profile_t *profile = listener->data;
	h2o_socket_t *sock = NULL;

	if ( error != NULL ){
		return;
	}

	if ((sock = h2o_evloop_socket_accept(listener)) == NULL) {
		return;
	}

	h2o_accept(profile->h2o_accept_context, sock);
}

static void mod_sms_flowroute_profile_event_thread_on_timeout(h2o_timeout_entry_t *entry)
{
	/* required to have this callback, to enable any per interval checks or cleanup. */
}

static void *SWITCH_THREAD_FUNC mod_sms_flowroute_profile_event_thread(switch_thread_t *thread, void *obj)
{
	mod_sms_flowroute_profile_t *profile = obj;
	struct sockaddr_in addr;
	int fd, reuseaddr_flag = 1, err = 0;
	h2o_socket_t *sock;
	h2o_timeout_t timeout = {0};
	h2o_timeout_entry_t timeout_entry = {0};

	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = INADDR_ANY;
	addr.sin_port = htons(profile->port);

	err = (fd = socket(AF_INET, SOCK_STREAM, 0));
	if (err == -1 ) {
		fprintf(stderr, "unable to open socket [%d]\n", err);
		return 0;
	}

	err = setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &reuseaddr_flag, sizeof(reuseaddr_flag));
	if (err != 0) {
		fprintf(stderr, "Unable to set socket options [%d]\n", err);
		return 0;
	}

	err = bind(fd, (struct sockaddr *)&addr, sizeof(addr));
	if (err  != 0 ) {
		fprintf(stderr, "Unable to bind to socket [%d]\n", err);
		perror("bind");
		return 0;
	}

	err = listen(fd, SOMAXCONN);
	if ( err != 0) {
		fprintf(stderr, "Unable to listen on socket [%d]\n", err);
		return 0;
	}

	sock = h2o_evloop_socket_create(profile->h2o_context.loop, fd, H2O_SOCKET_FLAG_DONT_READ);
	sock->data = (void *) profile;
	h2o_socket_read_start(sock, on_accept);

	while ( profile->running ) {
		//		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Profile[%s] event thread loop\n", profile->name);
		h2o_timeout_init(profile->h2o_context.loop, &timeout, 1000); /* 1 second loop */
		timeout_entry.cb = mod_sms_flowroute_profile_event_thread_on_timeout;
		h2o_timeout_link(profile->h2o_context.loop, &timeout, &timeout_entry);

		h2o_evloop_run(profile->h2o_context.loop);

		h2o_timeout_unlink(&timeout_entry);
		h2o_timeout_dispose(profile->h2o_context.loop, &timeout);
	}
	h2o_socket_close(sock);
	return 0;
}

switch_status_t mod_sms_flowroute_profile_destroy(mod_sms_flowroute_profile_t **old_profile)
{
	mod_sms_flowroute_profile_t *profile = NULL;
	switch_status_t status;

	if ( !old_profile || !*old_profile ) {
		return SWITCH_STATUS_SUCCESS;
	}

	profile = *old_profile;

	switch_core_hash_delete(mod_sms_flowroute_globals.profile_hash, profile->name);

	profile->running = 0;

	if (profile->profile_thread) {
		switch_thread_join(&status, profile->profile_thread);
	}

	switch_safe_free(profile->h2o_accept_context);
	switch_safe_free(profile->h2o_context.loop);
	h2o_context_dispose(&(profile->h2o_context));
	h2o_config_dispose(&(profile->h2o_globalconf));

	switch_core_destroy_memory_pool(&(profile->pool));
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Profile[%s] destroyed\n", profile->name);

	*old_profile = NULL;
	return SWITCH_STATUS_SUCCESS;
}

static int mod_sms_flowroute_profile_request_handler(h2o_handler_t *handler, h2o_req_t *request)
{
	static h2o_generator_t generator = {NULL, NULL};
	h2o_iovec_t body = h2o_strdup(&request->pool, "ACCEPTED\n", SIZE_MAX);
	char *content = strndup(request->entity.base, request->entity.len);
	cJSON *parsed = NULL;
	switch_event_t *evt = NULL;

	/* If there were a better way to cJSON_Parse, but with a str and len, this could remove a strndup */
	parsed = cJSON_Parse(content);

	if ( !parsed ) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Invalid request received[%.*s]", (int) request->entity.len, request->entity.base);
		goto done;
	}

	switch_event_create(&evt, SWITCH_EVENT_MESSAGE);
	switch_event_add_header_string(evt, SWITCH_STACK_BOTTOM, "to", cJSON_GetObjectCstr(parsed, "to"));
	switch_event_add_header_string(evt, SWITCH_STACK_BOTTOM, "body", cJSON_GetObjectCstr(parsed, "body"));
	switch_event_add_header_string(evt, SWITCH_STACK_BOTTOM, "from", cJSON_GetObjectCstr(parsed, "from"));
	switch_event_add_header_string(evt, SWITCH_STACK_BOTTOM, "record_id", cJSON_GetObjectCstr(parsed, "id"));
	switch_event_add_header_string(evt, SWITCH_STACK_BOTTOM, "context", "default");
	switch_event_add_header_string(evt, SWITCH_STACK_BOTTOM, "proto", "sms_flowroute");

    switch_core_chat_send("GLOBAL_SMS", evt);
	switch_event_destroy(&evt);

	request->res.status = 200;
	request->res.reason = "OK";
	h2o_add_header(&request->pool, &request->res.headers, H2O_TOKEN_CONTENT_TYPE, H2O_STRLIT("text/plain; charset=utf-8"));
	h2o_start_response(request, &generator);
	h2o_send(request, &body, 1, 1);

 done:

	cJSON_Delete(parsed);
	switch_safe_free(content);
	return 0;
}

switch_status_t mod_sms_flowroute_profile_create(mod_sms_flowroute_profile_t **new_profile, char *name, int debug, int port,
												 char *access_key, char *secret_key, char *host)
{
	mod_sms_flowroute_profile_t *profile = NULL;
	switch_memory_pool_t *pool = NULL;
	switch_threadattr_t *thd_attr;
	char auth[256] = {0};
	unsigned int auth_size = 0;

	switch_core_new_memory_pool(&pool);

	profile = switch_core_alloc(pool, sizeof(mod_sms_flowroute_profile_t));

	profile->pool = pool;
	profile->debug = debug;
	profile->running = 1;
	profile->name = name ? switch_core_strdup(profile->pool, name) : "default";
	profile->access_key = access_key ? switch_core_strdup(profile->pool, access_key) : "access_key";
	profile->secret_key = secret_key ? switch_core_strdup(profile->pool, secret_key) : "secret_key";
	profile->host = host ? switch_core_strdup(profile->pool, host) : "https://api.flowroute.com/v2/messages";
	profile->port = port ? port : 8000;

	auth_size = snprintf(auth, 256, "%s:%s", profile->access_key, profile->secret_key);
	switch_b64_encode((unsigned char *)auth, auth_size, profile->auth_b64, 512);
	profile->auth_b64_size = strlen((const char *)profile->auth_b64);

	if ( h2o_url_parse(profile->host, SIZE_MAX, &profile->url_parsed) != 0) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Profile[%s] error processing url[%s]\n", profile->name, profile->host);
		goto err;
	}

	h2o_config_init(&(profile->h2o_globalconf));
	profile->h2o_hostconf = h2o_config_register_host(&(profile->h2o_globalconf), h2o_iovec_init(H2O_STRLIT("mod_sms_flowroute")), 2048);

	/* Register h2o handlers here */
	profile->h2o_pathconf = h2o_config_register_path(profile->h2o_hostconf, "/", 0);
	profile->h2o_handler = h2o_create_handler(profile->h2o_pathconf, sizeof(h2o_handler_t));
	profile->h2o_handler->on_req = mod_sms_flowroute_profile_request_handler;

	h2o_context_init(&(profile->h2o_context), h2o_evloop_create(), &(profile->h2o_globalconf));

	profile->queue = h2o_multithread_create_queue(profile->h2o_context.loop);

	profile->h2o_accept_context = calloc(1, sizeof(h2o_accept_ctx_t));
	profile->h2o_accept_context->ctx = &(profile->h2o_context);
	profile->h2o_accept_context->hosts = profile->h2o_globalconf.hosts;

	switch_threadattr_create(&thd_attr, pool);
	switch_threadattr_stacksize_set(thd_attr, SWITCH_THREAD_STACKSIZE);
	switch_thread_create(&(profile->profile_thread), thd_attr, mod_sms_flowroute_profile_event_thread, (void *) profile, pool);

	switch_core_hash_insert(mod_sms_flowroute_globals.profile_hash, name, (void *) profile);
	*new_profile = profile;
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Profile[%s] created\n", profile->name);

	return SWITCH_STATUS_SUCCESS;

 err:
	return SWITCH_STATUS_GENERR;
}


static int on_body(h2o_http1client_t *client, const char *errstr)
{
	h2o_http1client_ctx_t *ctx = client->ctx;
	mod_sms_flowroute_message_t *msg = H2O_STRUCT_FROM_MEMBER(mod_sms_flowroute_message_t, ctx, ctx);

	if (errstr != NULL && errstr != h2o_http1client_error_is_eos) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "SMS Send error on_body[%s]\n", errstr);
		goto err;
	}

	fwrite(client->sock->input->bytes, 1, client->sock->input->size, stdout);
	h2o_buffer_consume(&client->sock->input, client->sock->input->size);

	if (errstr == h2o_http1client_error_is_eos) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "SMS Send EOS\n");
	}

	msg->status = 0;
	switch_mutex_unlock(msg->mutex);
	return 0;

 err:
	msg->status = 3;
	switch_mutex_unlock(msg->mutex);
	return -1;
}

static h2o_http1client_body_cb on_head(h2o_http1client_t *client, const char *errstr, int minor_version, int status, h2o_iovec_t msg_iovec,
								h2o_http1client_header_t *headers, size_t num_headers)
{
	size_t i;
	switch_log_level_t loglevel = SWITCH_LOG_DEBUG;
	h2o_http1client_ctx_t *ctx = client->ctx;
	mod_sms_flowroute_message_t *msg = H2O_STRUCT_FROM_MEMBER(mod_sms_flowroute_message_t, ctx, ctx);

	if (errstr != NULL && errstr != h2o_http1client_error_is_eos) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "SMS Send error on_head[%s]\n", errstr);
		goto err;
	}

	if ( status != 200 ) {
		loglevel = SWITCH_LOG_ERROR;
	}

	switch_log_printf(SWITCH_CHANNEL_LOG, loglevel, "HTTP/1.%d %d %.*s\n", minor_version, status, (int)msg_iovec.len, msg_iovec.base);
	for (i = 0; i != num_headers; ++i) {
		switch_log_printf(SWITCH_CHANNEL_LOG, loglevel, "%.*s: %.*s\n",
						  (int)headers[i].name_len, headers[i].name, (int)headers[i].value_len, headers[i].value);
	}

	if (errstr == h2o_http1client_error_is_eos) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "SMS Send error on_head no body received[%s]\n", errstr);
		goto err;
	}

	return on_body;

 err:
	msg->status = 2;
	switch_mutex_unlock(msg->mutex);
	return NULL;
}

static h2o_http1client_head_cb on_connect(h2o_http1client_t *client, const char *errstr, h2o_iovec_t **reqbufs, size_t *reqbufcnt,
								   int *method_is_head)
{
	h2o_http1client_ctx_t *ctx = client->ctx;
	mod_sms_flowroute_message_t *msg = H2O_STRUCT_FROM_MEMBER(mod_sms_flowroute_message_t, ctx, ctx);

	if (errstr != NULL) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "SMS Send error on_connect[%s]\n", errstr);
		goto err;
	}

	*reqbufs = (h2o_iovec_t *)client->data;
	*reqbufcnt = 1;
	*method_is_head = 0;

	return on_head;

 err:
	msg->status = 1;
	switch_mutex_unlock(msg->mutex);
	return NULL;
}

switch_status_t mod_sms_flowroute_profile_send_message(mod_sms_flowroute_profile_t *profile, switch_event_t *event)
{
	mod_sms_flowroute_message_t *msg = NULL;
	char *to = NULL, *from = NULL, *text = NULL;
	switch_status_t status = SWITCH_STATUS_GENERR;
	int wait_loops = 10; /* 10 seconds */
	cJSON *body = NULL;

	msg = calloc(1, sizeof(mod_sms_flowroute_message_t));
	msg->req.base = calloc(1, 2048);
	msg->ctx.getaddr_receiver = &msg->getaddr_receiver;
	msg->ctx.io_timeout = &msg->io_timeout;
	msg->ctx.loop = profile->h2o_context.loop;
	msg->profile = profile;
	msg->status = -1;
	h2o_timeout_init(msg->ctx.loop, &msg->io_timeout, 5000); /* 5 seconds */
	h2o_multithread_register_receiver(profile->queue, msg->ctx.getaddr_receiver, h2o_hostinfo_getaddr_receiver);

	msg->ctx.ssl_ctx = SSL_CTX_new(TLSv1_2_client_method());
	SSL_CTX_load_verify_locations(msg->ctx.ssl_ctx, NULL, "/etc/ssl/certs/");
	SSL_CTX_set_verify(msg->ctx.ssl_ctx, SSL_VERIFY_PEER | SSL_VERIFY_FAIL_IF_NO_PEER_CERT, NULL);

	switch_mutex_init(&msg->mutex, SWITCH_MUTEX_UNNESTED, profile->pool);
	switch_mutex_lock(msg->mutex);

	body = cJSON_CreateObject();

	to = switch_event_get_header(event, "to");
	if ( !to ) {
		to = switch_event_get_header(event, "destination_addr");
	}

	from = switch_event_get_header(event, "from");
	if ( !from ) {
		from = switch_event_get_header(event, "source_addr");
	}

	cJSON_AddItemToObject(body, "to", cJSON_CreateString(to));
	cJSON_AddItemToObject(body, "from", cJSON_CreateString(from));
	cJSON_AddItemToObject(body, "body", cJSON_CreateString((const char *) switch_event_get_body(event)));

	text = cJSON_Print(body);
	cJSON_Delete(body);

	msg->req.len = snprintf(msg->req.base, 2048, "POST %.*s HTTP/1.1\r\n"
							"Authorization: Basic %.*s\r\n"
							"Host: %.*s\r\n"
							"Accept: */*\r\n"
							"Content-Type: application/json\r\n"
							"Content-Length: %d\r\n"
							"\r\n%s",
							(int) profile->url_parsed.path.len, profile->url_parsed.path.base,
							profile->auth_b64_size, profile->auth_b64,
							(int) profile->url_parsed.authority.len, profile->url_parsed.authority.base,
							(int)strlen(text), text);

	if ( profile->debug ) {
		char *msg_txt = NULL;
		switch_event_serialize(event, &msg_txt, SWITCH_FALSE);
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Profile[%s] sending message from event\n%s\n", profile->name, msg_txt);
		switch_safe_free(msg_txt);
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Profile[%s] sending message json:\n%s\n", profile->name, text);
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "REQUEST\n\n%.*s\n\n", (int) msg->req.len, msg->req.base);
	}

	h2o_http1client_connect(NULL, &msg->req, &(msg->ctx), profile->url_parsed.host, h2o_url_get_port(&profile->url_parsed), 1, on_connect);

	do {
		switch_yield(1000000);
		wait_loops--;
		status = switch_mutex_trylock(msg->mutex);
	} while ( wait_loops > 0 && status != SWITCH_STATUS_SUCCESS);

	if ( status != SWITCH_STATUS_SUCCESS ) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Profile[%s] send_message thread timed out on send\n", profile->name);
		goto err;
	}

	if ( msg->status > 0 ) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Profile[%s] send_message resulted in failure status %d\n", profile->name, msg->status);
		goto err;
	}

	h2o_timeout_dispose(msg->ctx.loop, msg->ctx.io_timeout);
	switch_mutex_destroy(msg->mutex);
	switch_safe_free(msg->req.base);
	switch_safe_free(msg);
	return SWITCH_STATUS_SUCCESS;

 err:
	if ( msg && msg->mutex ) {
		switch_mutex_destroy(msg->mutex);
	}
	h2o_timeout_dispose(msg->ctx.loop, msg->ctx.io_timeout);
	switch_safe_free(msg->req.base);
	switch_safe_free(msg);
	return SWITCH_STATUS_GENERR;
}

switch_status_t mod_sms_flowroute_interface_chat_send(switch_event_t *event)
{
	mod_sms_flowroute_profile_t *profile = NULL;
	char *profile_name = switch_event_get_header(event, "sms_flowroute_profile");

	if (zstr(profile_name)) {
		profile_name = "default";
	}

	profile = switch_core_hash_find(mod_sms_flowroute_globals.profile_hash, profile_name);

	if (!profile) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CONSOLE, "NO SUCH SMS_FLOWROUTE PROFILE[%s].", profile_name);
		return SWITCH_STATUS_GENERR;
	}

	mod_sms_flowroute_profile_send_message(profile, event);

	return SWITCH_STATUS_SUCCESS;
}

/* static switch_status_t name (switch_event_t *message, const char *data) */
SWITCH_STANDARD_CHAT_APP(mod_sms_flowroute_chat_send_function)
{
	mod_sms_flowroute_profile_t *profile = NULL;

	profile = switch_core_hash_find(mod_sms_flowroute_globals.profile_hash, data);

	if ( !profile ) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CONSOLE, "NO SUCH SMS_FLOWROUTE PROFILE[%s].", data);
		return SWITCH_STATUS_GENERR;
	}

	mod_sms_flowroute_profile_send_message(profile, message);
	return SWITCH_STATUS_SUCCESS;
}

/* static void name (switch_core_session_t *session, const char *data) */
SWITCH_STANDARD_APP(mod_sms_flowroute_app_send_function)
{
	switch_event_header_t *chan_var = NULL;
	switch_channel_t *channel = switch_core_session_get_channel(session);
	switch_event_t *message = NULL;

	if (switch_event_create(&message, SWITCH_EVENT_MESSAGE) != SWITCH_STATUS_SUCCESS) {
		return;
	}

	/* Copy over recognized channel vars. Then call the chat send function */
	/* Cycle through all of the channel headers, and ones with 'sms_flowroute_' prefix copy over without the prefix */
	for ( chan_var = switch_channel_variable_first(channel); chan_var; chan_var = chan_var->next) {
		if ( !strncmp(chan_var->name, "sms_flowroute_", 14) ) {
			switch_event_add_header_string(message, SWITCH_STACK_BOTTOM, chan_var->name + 14, chan_var->value);
		}
	}

	/* Unlock the channel variables */
	switch_channel_variable_last(channel);
	mod_sms_flowroute_chat_send_function(message, data);
	return;
}

/* static switch_status_t name (_In_opt_z_ const char *cmd, _In_opt_ switch_core_session_t *session, _In_ switch_stream_handle_t *stream) */
SWITCH_STANDARD_API(mod_sms_flowroute_debug_api)
{
	mod_sms_flowroute_globals.debug = switch_true(cmd);

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CONSOLE, "debug is %s\n", (mod_sms_flowroute_globals.debug ? "on" : "off") );
	return SWITCH_STATUS_SUCCESS;
}

/* static switch_status_t name (_In_opt_z_ const char *cmd, _In_opt_ switch_core_session_t *session, _In_ switch_stream_handle_t *stream) */
SWITCH_STANDARD_API(mod_sms_flowroute_send_api)
{
	mod_sms_flowroute_profile_t *profile = NULL;
	switch_status_t status = SWITCH_STATUS_SUCCESS;
	switch_event_t *message = NULL;
	char *argv[1024] = { 0 };
	int argc = 0;
	char *cmd_dup = strdup(cmd);

	if (!(argc = switch_separate_string(cmd_dup, '|', argv, (sizeof(argv) / sizeof(argv[0])))) || argc != 4 ) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Invalid format. Must be | separated like: profile|destination|source|message\n");
		switch_goto_status(SWITCH_STATUS_GENERR, done);
	}

	profile = switch_core_hash_find(mod_sms_flowroute_globals.profile_hash, argv[0]);

	if ( !profile ) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CONSOLE, "NO SUCH SMS_FLOWROUTE PROFILE[%s].", argv[0]);
		switch_goto_status(SWITCH_STATUS_GENERR, done);
	}

	if (switch_event_create(&message, SWITCH_EVENT_MESSAGE) != SWITCH_STATUS_SUCCESS) {
		switch_goto_status(SWITCH_STATUS_GENERR, done);
	}

	switch_event_add_header_string(message, SWITCH_STACK_BOTTOM, "destination_addr", argv[1]);
	switch_event_add_header_string(message, SWITCH_STACK_BOTTOM, "source_addr", argv[2]);
	switch_event_set_body(message, argv[3]);

	if (mod_sms_flowroute_profile_send_message(profile, message) != SWITCH_STATUS_SUCCESS) {
		switch_goto_status(SWITCH_STATUS_GENERR, done);
	}

 done:
	switch_safe_free(cmd_dup);
	return status;

}

switch_status_t mod_sms_flowroute_do_config()
{
	char *conf = "sms_flowroute.conf";
	switch_xml_t xml, cfg, profiles, profile, params, param;

	if (!(xml = switch_xml_open_cfg(conf, &cfg, NULL))) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "open of %s failed\n", conf);
		goto err;
	}

	if ( (profiles = switch_xml_child(cfg, "profiles")) != NULL) {
		for (profile = switch_xml_child(profiles, "profile"); profile; profile = profile->next) {
			mod_sms_flowroute_profile_t *new_profile = NULL;
			int debug = 0, port = 0;
			char *access_key = NULL, *secret_key = NULL, *host = NULL;
			char *name = (char *)switch_xml_attr_soft(profile, "name");

			// Load params
			if ( (params = switch_xml_child(profile, "params")) != NULL) {
				for (param = switch_xml_child(params, "param"); param; param = param->next) {
					char *var = (char *) switch_xml_attr_soft(param, "name");

					if ( ! strncmp(var, "debug", 5) ) {
						debug = atoi(switch_xml_attr_soft(param, "value"));
					} else if ( ! strncmp(var, "port", 4) ) {
						port = atoi(switch_xml_attr_soft(param, "value"));
					} else if ( ! strncmp(var, "access-key", 10) ) {
						access_key = (char *) switch_xml_attr_soft(param, "value");
					} else if ( ! strncmp(var, "secret-key", 10) ) {
						secret_key = (char *) switch_xml_attr_soft(param, "value");
					} else if ( ! strncmp(var, "host", 4) ) {
						host = (char *) switch_xml_attr_soft(param, "value");
					}
				}
			}

			if ( mod_sms_flowroute_profile_create(&new_profile, name, debug, port, access_key, secret_key, host) == SWITCH_STATUS_SUCCESS) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Created profile[%s]\n", name);
			} else {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Failed to create profile[%s]\n", name);
			}
		}
	} else {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Profiles config is missing\n");
		goto err;
	}

	switch_xml_free(xml);
	return SWITCH_STATUS_SUCCESS;

 err:
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Configuration failed\n");
	if(xml){
		switch_xml_free(xml);
	}
	return SWITCH_STATUS_GENERR;
}

/* switch_status_t name (switch_loadable_module_interface_t **module_interface, switch_memory_pool_t *pool) */
SWITCH_MODULE_LOAD_FUNCTION(mod_sms_flowroute_load)
{
	switch_api_interface_t *mod_sms_flowroute_api_interface;
	switch_chat_interface_t *mod_sms_flowroute_chat_interface;
	switch_chat_application_interface_t *mod_sms_flowroute_chat_app_interface;
	switch_application_interface_t *mod_sms_flowroute_app_interface;

	/* connect my internal structure to the blank pointer passed to me */
	*module_interface = switch_loadable_module_create_module_interface(pool, modname);

	memset(&mod_sms_flowroute_globals, 0, sizeof(mod_sms_flowroute_globals_t));
	mod_sms_flowroute_globals.pool = pool;
	mod_sms_flowroute_globals.debug = 0;
	switch_core_hash_init(&(mod_sms_flowroute_globals.profile_hash));

	if ( mod_sms_flowroute_do_config() != SWITCH_STATUS_SUCCESS ) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Failed to load due to bad configs\n");
		return SWITCH_STATUS_TERM;
	}

	/*	SSL_load_error_strings();
	SSL_library_init();
	OpenSSL_add_all_algorithms();*/


	SWITCH_ADD_CHAT(mod_sms_flowroute_chat_interface, "sms_flowroute", mod_sms_flowroute_interface_chat_send);
	SWITCH_ADD_API(mod_sms_flowroute_api_interface, "sms_flowroute_send", "mod_sms_flowroute send", mod_sms_flowroute_send_api, NULL);
	SWITCH_ADD_API(mod_sms_flowroute_api_interface, "sms_flowroute_debug", "mod_sms_flowroute toggle debug", mod_sms_flowroute_debug_api, NULL);
	SWITCH_ADD_CHAT_APP(mod_sms_flowroute_chat_app_interface, "sms_flowroute_send", "send message to profile", "send message to profile",
						mod_sms_flowroute_chat_send_function, "", SCAF_NONE);
	SWITCH_ADD_APP(mod_sms_flowroute_app_interface, "sms_flowroute_send", NULL, NULL, mod_sms_flowroute_app_send_function,
				   "sms_flowroute_send", SAF_SUPPORT_NOMEDIA | SAF_ROUTING_EXEC);

	/* indicate that the module should continue to be loaded */
	return SWITCH_STATUS_SUCCESS;
}

SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_sms_flowroute_shutdown)
{
	switch_hash_index_t *hi;
	mod_sms_flowroute_profile_t *profile = NULL;

	while ((hi = switch_core_hash_first(mod_sms_flowroute_globals.profile_hash))) {
		switch_core_hash_this(hi, NULL, NULL, (void **)&profile);
		mod_sms_flowroute_profile_destroy(&profile);
		switch_safe_free(hi);
	}

	switch_core_hash_destroy(&(mod_sms_flowroute_globals.profile_hash));

	return SWITCH_STATUS_SUCCESS;
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
