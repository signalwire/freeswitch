/*
 * mod_rayo for FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
 * Copyright (C) 2013-2015, Grasshopper
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
 * The Original Code is mod_rayo for FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
 *
 * The Initial Developer of the Original Code is Grasshopper
 * Portions created by the Initial Developer are Copyright (C)
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 * Chris Rienzo <chris.rienzo@grasshopper.com>
 *
 * xmpp_streams.c -- XMPP s2s and c2s streams
 *
 */
#include <switch.h>
#include <iksemel.h>

#include <openssl/ssl.h>

#include "xmpp_streams.h"
#include "iks_helpers.h"
#include "sasl.h"

#define MAX_QUEUE_LEN 25000

/**
 * Context for all streams
 */
struct xmpp_stream_context {
	/** memory pool to use */
	switch_memory_pool_t *pool;
	/** domain for this context */
	const char *domain;
	/** synchronizes access to streams and routes hashes */
	switch_mutex_t *streams_mutex;
	/** map of stream JID to routable stream */
	switch_hash_t *routes;
	/** map of stream ID to stream */
	switch_hash_t *streams;
	/** map of user ID to password */
	switch_hash_t *users;
	/** shared secret for server dialback */
	const char *dialback_secret;
	/** callback when a new resource is bound */
	xmpp_stream_bind_callback bind_callback;
	/** callback when a new stream is ready */
	xmpp_stream_ready_callback ready_callback;
	/** callback when a stream is destroyed */
	xmpp_stream_destroy_callback destroy_callback;
	/** callback when a stanza is received */
	xmpp_stream_recv_callback recv_callback;
	/** context shutdown flag */
	int shutdown;
	/** prevents context shutdown until all threads are finished */
	switch_thread_rwlock_t *shutdown_rwlock;
	/** path to cert PEM file */
	const char *cert_pem_file;
	/** path to key PEM file */
	const char *key_pem_file;
};

/**
 * State of a stream
 */
enum xmpp_stream_state {
	/** new connection */
	XSS_CONNECT,
	/** encrypted comms established */
	XSS_SECURE,
	/** remote party authenticated */
	XSS_AUTHENTICATED,
	/** client resource bound */
	XSS_RESOURCE_BOUND,
	/** ready to accept requests */
	XSS_READY,
	/** terminating stream */
	XSS_SHUTDOWN,
	/** unrecoverable error */
	XSS_ERROR,
	/** destroyed */
	XSS_DESTROY
};

/**
 * A client/server stream connection
 */
struct xmpp_stream {
	/** stream state */
	enum xmpp_stream_state state;
	/** true if server-to-server connection */
	int s2s;
	/** true if incoming connection */
	int incoming;
	/** Jabber ID of remote party */
	char *jid;
	/** stream ID */
	char *id;
	/** stream pool */
	switch_memory_pool_t *pool;
	/** address of this stream */
	const char *address;
	/** port of this stream */
	int port;
	/** synchronizes access to this stream */
	switch_mutex_t *mutex;
	/** socket to remote party */
	switch_socket_t *socket;
	/** socket poll descriptor */
	switch_pollfd_t *pollfd;
	/** XML stream parser */
	iksparser *parser;
	/** outbound message queue */
	switch_queue_t *msg_queue;
	/** true if no activity last poll */
	int idle;
	/** context for this stream */
	struct xmpp_stream_context *context;
	/** user private data */
	void *user_private;
};

/**
 * A socket listening for new connections
 */
struct xmpp_listener {
	/** listener pool */
	switch_memory_pool_t *pool;
	/** listen address */
	char *addr;
	/** listen port */
	switch_port_t port;
	/** access control list */
	const char *acl;
	/** listen socket */
	switch_socket_t *socket;
	/** pollset for listen socket */
	switch_pollfd_t *read_pollfd;
	/** true if server to server connections only */
	int s2s;
	/** context for new streams */
	struct xmpp_stream_context *context;
};

static void xmpp_stream_new_id(struct xmpp_stream *stream);
static void xmpp_stream_set_id(struct xmpp_stream *stream, const char *id);

/**
 * Convert xmpp stream state to string
 * @param state the xmpp stream state
 * @return the string value of state or "UNKNOWN"
 */
static const char *xmpp_stream_state_to_string(enum xmpp_stream_state state)
{
	switch(state) {
		case XSS_CONNECT: return "CONNECT";
		case XSS_SECURE: return "SECURE";
		case XSS_AUTHENTICATED: return "AUTHENTICATED";
		case XSS_RESOURCE_BOUND: return "RESOURCE_BOUND";
		case XSS_READY: return "READY";
		case XSS_SHUTDOWN: return "SHUTDOWN";
		case XSS_ERROR: return "ERROR";
		case XSS_DESTROY: return "DESTROY";
	}
	return "UNKNOWN";
}

/**
 * Handle XMPP stream logging callback
 * @param user_data the xmpp stream
 * @param data the log message
 * @param size of the log message
 * @param is_incoming true if this is a log for a received message
 */
static void on_stream_log(void *user_data, const char *data, size_t size, int is_incoming)
{
	if (size > 0) {
		struct xmpp_stream *stream = (struct xmpp_stream *)user_data;
		switch_log_printf(SWITCH_CHANNEL_UUID_LOG(stream->id), SWITCH_LOG_DEBUG, "%s, %s:%i, %s_%s %s %s\n", stream->jid, stream->address, stream->port, stream->s2s ? "s2s" : "c2s",
			stream->incoming ? "in" : "out", is_incoming ? "RECV" : "SEND", data);
	}
}

/**
 * Send stanza to stream.
 */
static void xmpp_stream_stanza_send(struct xmpp_stream *stream, iks *msg)
{
	/* send directly if client or outbound s2s stream */
	if (!stream->s2s || !stream->incoming) {
		iks_send(stream->parser, msg);
		iks_delete(msg);
	} else {
		/* route message to outbound server stream */
		xmpp_stream_context_send(stream->context, stream->jid, msg);
		iks_delete(msg);
	}
}

/**
 * Attach stream to connected socket
 * @param stream the stream
 * @param socket the connected socket
 */
static void xmpp_stream_set_socket(struct xmpp_stream *stream, switch_socket_t *socket)
{
	stream->socket = socket;
	switch_socket_create_pollset(&stream->pollfd, stream->socket, SWITCH_POLLIN | SWITCH_POLLERR, stream->pool);

	/* connect XMPP stream parser to socket */
	{
		switch_os_socket_t os_socket;
		switch_os_sock_get(&os_socket, stream->socket);
		iks_connect_fd(stream->parser, os_socket);
		/* TODO connect error checking */
	}
}

/**
 * Assign a new ID to the stream
 * @param stream the stream
 */
static void xmpp_stream_new_id(struct xmpp_stream *stream)
{
	char id[SWITCH_UUID_FORMATTED_LENGTH + 1] = { 0 };
	switch_uuid_str(id, sizeof(id));
	xmpp_stream_set_id(stream, id);
}

/**
 * Send session reply to server <stream> after auth is done
 * @param stream the xmpp stream
 */
static void xmpp_send_server_header_features(struct xmpp_stream *stream)
{
	struct xmpp_stream_context *context = stream->context;
	char *header = switch_mprintf(
		"<stream:stream xmlns='"IKS_NS_SERVER"' xmlns:db='"IKS_NS_XMPP_DIALBACK"'"
		" from='%s' id='%s' xml:lang='en' version='1.0'"
		" xmlns:stream='"IKS_NS_XMPP_STREAMS"'><stream:features>"
		"</stream:features>", context->domain, stream->id);

	iks_send_raw(stream->parser, header);
	free(header);
}

/**
 * Send bind + session reply to client <stream>
 * @param stream the xmpp stream
 */
static void xmpp_send_client_header_bind(struct xmpp_stream *stream)
{
	struct xmpp_stream_context *context = stream->context;
	char *header = switch_mprintf(
		"<stream:stream xmlns='"IKS_NS_CLIENT"' xmlns:db='"IKS_NS_XMPP_DIALBACK"'"
		" from='%s' id='%s' xml:lang='en' version='1.0'"
		" xmlns:stream='"IKS_NS_XMPP_STREAMS"'><stream:features>"
		"<bind xmlns='"IKS_NS_XMPP_BIND"'/>"
		"<session xmlns='"IKS_NS_XMPP_SESSION"'/>"
		"</stream:features>", context->domain, stream->id);

	iks_send_raw(stream->parser, header);
	free(header);
}

/**
 * Handle <presence> message callback
 * @param stream the stream
 * @param node the presence message
 */
static void on_stream_presence(struct xmpp_stream *stream, iks *node)
{
	struct xmpp_stream_context *context = stream->context;
	const char *from = iks_find_attrib(node, "from");

	switch_log_printf(SWITCH_CHANNEL_UUID_LOG(stream->id), SWITCH_LOG_DEBUG, "%s, %s:%i, presence, state = %s\n", stream->jid, stream->address, stream->port, xmpp_stream_state_to_string(stream->state));

	if (!from) {
		if (stream->s2s) {
			/* from is required in s2s connections */
			switch_log_printf(SWITCH_CHANNEL_UUID_LOG(stream->id), SWITCH_LOG_DEBUG, "%s, %s:%i, no presence from JID\n", stream->jid, stream->address, stream->port);
			return;
		}

		/* use stream JID if a c2s connection */
		from = stream->jid;
		if (zstr(from)) {
			/* error */
			switch_log_printf(SWITCH_CHANNEL_UUID_LOG(stream->id), SWITCH_LOG_DEBUG, "%s, %s:%i, no presence from JID\n", stream->jid, stream->address, stream->port);
			return;
		}
		iks_insert_attrib(node, "from", from);
	}
	if (context->recv_callback) {
		context->recv_callback(stream, node);
	}
}

/**
 * Send <success> reply to xmpp stream <auth>
 * @param stream the xmpp stream.
 */
static void xmpp_send_auth_success(struct xmpp_stream *stream)
{
	iks_send_raw(stream->parser, "<success xmlns='"IKS_NS_XMPP_SASL"'/>");
}

/**
 * Send <failure> reply to xmpp client <auth>
 * @param stream the xmpp stream to use.
 * @param reason the reason for failure
 */
static void xmpp_send_auth_failure(struct xmpp_stream *stream, const char *reason)
{
	char *reply = switch_mprintf("<failure xmlns='"IKS_NS_XMPP_SASL"'>"
		"<%s/></failure>", reason);
	iks_send_raw(stream->parser, reply);
	free(reply);
}

/**
 * Validate username and password
 * @param authzid authorization id
 * @param authcid authentication id
 * @param password
 * @return 1 if authenticated
 */
static int verify_plain_auth(struct xmpp_stream_context *context, const char *authzid, const char *authcid, const char *password)
{
	char *correct_password;
	if (zstr(authzid) || zstr(authcid) || zstr(password)) {
		return 0;
	}
	correct_password = switch_core_hash_find(context->users, authcid);
	return !zstr(correct_password) && !strcmp(correct_password, password);
}

/**
 * Send sasl reply to xmpp <stream>
 * @param stream the xmpp stream
 */
static void xmpp_send_client_header_auth(struct xmpp_stream *stream)
{
	struct xmpp_stream_context *context = stream->context;
	char *header = switch_mprintf(
		"<stream:stream xmlns='"IKS_NS_CLIENT"' xmlns:db='"IKS_NS_XMPP_DIALBACK"'"
		" from='%s' id='%s' xml:lang='en' version='1.0'"
		" xmlns:stream='"IKS_NS_XMPP_STREAMS"'><stream:features>"
		"<mechanisms xmlns='"IKS_NS_XMPP_SASL"'>"
		"<mechanism>PLAIN</mechanism>"
		"</mechanisms></stream:features>", context->domain, stream->id);
	iks_send_raw(stream->parser, header);
	free(header);
}

/**
 * Send sasl + starttls reply to xmpp <stream>
 * @param stream the xmpp stream
 */
static void xmpp_send_client_header_tls(struct xmpp_stream *stream)
{
	if (stream->context->key_pem_file && stream->context->cert_pem_file) {
		struct xmpp_stream_context *context = stream->context;
		char *header = switch_mprintf(
			"<stream:stream xmlns='"IKS_NS_CLIENT"' xmlns:db='"IKS_NS_XMPP_DIALBACK"'"
			" from='%s' id='%s' xml:lang='en' version='1.0'"
			" xmlns:stream='"IKS_NS_XMPP_STREAMS"'><stream:features>"
			"<starttls xmlns='"IKS_NS_XMPP_TLS"'><required/></starttls>"
			"<mechanisms xmlns='"IKS_NS_XMPP_SASL"'>"
			"<mechanism>PLAIN</mechanism>"
			"</mechanisms></stream:features>", context->domain, stream->id);
		iks_send_raw(stream->parser, header);
		free(header);
	} else {
		/* not set up for TLS, skip it */
		stream->state = XSS_SECURE;
		xmpp_send_client_header_auth(stream);
	}
}

/**
 * Send sasl reply to xmpp <stream>
 * @param stream the xmpp stream
 */
static void xmpp_send_server_header_auth(struct xmpp_stream *stream)
{
	struct xmpp_stream_context *context = stream->context;
	char *header = switch_mprintf(
		"<stream:stream xmlns='"IKS_NS_SERVER"' xmlns:db='"IKS_NS_XMPP_DIALBACK"'"
		" from='%s' id='%s' xml:lang='en' version='1.0'"
		" xmlns:stream='"IKS_NS_XMPP_STREAMS"'>"
		"<stream:features>"
		"</stream:features>",
		context->domain, stream->id);
	iks_send_raw(stream->parser, header);
	free(header);
}

/**
 * Send dialback to receiving server
 */
static void xmpp_send_dialback_key(struct xmpp_stream *stream)
{
	struct xmpp_stream_context *context = stream->context;
	char *dialback_key = iks_server_dialback_key(context->dialback_secret, stream->jid, context->domain, stream->id);
	if (dialback_key) {
		char *dialback = switch_mprintf(
			"<db:result from='%s' to='%s'>%s</db:result>",
			context->domain, stream->jid,
			dialback_key);
		iks_send_raw(stream->parser, dialback);
		free(dialback);
		free(dialback_key);
	} else {
		/* TODO missing shared secret */
	}
}

/**
 * Send initial <stream> header to peer server
 * @param stream the xmpp stream
 */
static void xmpp_send_outbound_server_header(struct xmpp_stream *stream)
{
	struct xmpp_stream_context *context = stream->context;
	char *header = switch_mprintf(
		"<stream:stream xmlns='"IKS_NS_SERVER"' xmlns:db='"IKS_NS_XMPP_DIALBACK"'"
		" from='%s' to='%s' xml:lang='en' version='1.0'"
		" xmlns:stream='"IKS_NS_XMPP_STREAMS"'>", context->domain, stream->jid);
	iks_send_raw(stream->parser, header);
	free(header);
}

/**
 * Handle <starttls> message.
 * @param the xmpp stream
 * @param node the <starttls> packet
 */
static void on_stream_starttls(struct xmpp_stream *stream, iks *node)
{
	/* wait for handshake to start */
	if (iks_proceed_tls(stream->parser, stream->context->cert_pem_file, stream->context->key_pem_file) == IKS_OK) {
		stream->state = XSS_SECURE;
	} else {
		stream->state = XSS_ERROR;
	}
}

/**
 * Handle <auth> message.  Only PLAIN supported.
 * @param stream the xmpp stream
 * @param node the <auth> packet
 */
static void on_stream_auth(struct xmpp_stream *stream, iks *node)
{
	struct xmpp_stream_context *context = stream->context;
	const char *xmlns, *mechanism;
	iks *auth_body;

	switch_log_printf(SWITCH_CHANNEL_UUID_LOG(stream->id), SWITCH_LOG_DEBUG, "%s, %s:%i, auth, state = %s\n", stream->jid, stream->address, stream->port, xmpp_stream_state_to_string(stream->state));

	/* wrong state for authentication */
	if (stream->state != XSS_SECURE) {
		switch_log_printf(SWITCH_CHANNEL_UUID_LOG(stream->id), SWITCH_LOG_WARNING, "%s, %s:%i, auth UNEXPECTED, state = %s\n", stream->jid, stream->address, stream->port, xmpp_stream_state_to_string(stream->state));
		/* on_auth unexpected error */
		stream->state = XSS_ERROR;
		return;
	}

	/* unsupported authentication type */
	xmlns = iks_find_attrib_soft(node, "xmlns");
	if (strcmp(IKS_NS_XMPP_SASL, xmlns)) {
		switch_log_printf(SWITCH_CHANNEL_UUID_LOG(stream->id), SWITCH_LOG_WARNING, "%s, %s:%i, auth, state = %s, unsupported namespace: %s!\n", stream->jid, stream->address, stream->port, xmpp_stream_state_to_string(stream->state), xmlns);
		/* on_auth namespace error */
		stream->state = XSS_ERROR;
		return;
	}

	/* unsupported SASL authentication mechanism */
	mechanism = iks_find_attrib_soft(node, "mechanism");
	if (strcmp("PLAIN", mechanism)) {
		switch_log_printf(SWITCH_CHANNEL_UUID_LOG(stream->id), SWITCH_LOG_WARNING, "%s, %s:%i, auth, state = %s, unsupported SASL mechanism: %s!\n", stream->jid, stream->address, stream->port, xmpp_stream_state_to_string(stream->state), mechanism);
		xmpp_send_auth_failure(stream, "invalid-mechanism");
		stream->state = XSS_ERROR;
		return;
	}

	if ((auth_body = iks_child(node)) && iks_type(auth_body) == IKS_CDATA) {
		/* get user and password from auth */
		char *message = iks_cdata(auth_body);
		char *authzid = NULL, *authcid, *password;
		/* TODO use library for SASL! */
		parse_plain_auth_message(message, &authzid, &authcid, &password);
		switch_log_printf(SWITCH_CHANNEL_UUID_LOG(stream->id), SWITCH_LOG_DEBUG, "%s, %s:%i, auth, state = %s, SASL/PLAIN decoded authzid = \"%s\" authcid = \"%s\"\n", stream->jid, stream->address, stream->port, xmpp_stream_state_to_string(stream->state), authzid, authcid);
		if (verify_plain_auth(context, authzid, authcid, password)) {
			stream->jid = switch_core_strdup(stream->pool, authzid);
			if (!stream->s2s && !strchr(stream->jid, '@')) {
				/* add missing domain on client stream */
				stream->jid = switch_core_sprintf(stream->pool, "%s@%s", stream->jid, context->domain);
			}

			xmpp_send_auth_success(stream);
			stream->state = XSS_AUTHENTICATED;
		} else {
			switch_log_printf(SWITCH_CHANNEL_UUID_LOG(stream->id), SWITCH_LOG_WARNING, "%s, %s:%i, auth, state = %s, invalid user or password!\n", stream->jid, stream->address, stream->port, xmpp_stream_state_to_string(stream->state));
			xmpp_send_auth_failure(stream, "not-authorized");
			stream->state = XSS_ERROR;
		}
		switch_safe_free(authzid);
		switch_safe_free(authcid);
		switch_safe_free(password);
	} else {
		/* missing message */
		stream->state = XSS_ERROR;
	}
}

/**
 * Handle <iq><session> request
 * @param stream the xmpp stream
 * @param node the <iq> node
 * @return NULL
 */
static iks *on_iq_set_xmpp_session(struct xmpp_stream *stream, iks *node)
{
	struct xmpp_stream_context *context = stream->context;
	iks *reply;

	switch(stream->state) {
		case XSS_RESOURCE_BOUND: {
			if (context->ready_callback && !context->ready_callback(stream)) {
				reply = iks_new_error(node, STANZA_ERROR_INTERNAL_SERVER_ERROR);
				stream->state = XSS_ERROR;
			} else {
				reply = iks_new_iq_result(node);
				stream->state = XSS_READY;

				/* add to available streams */
				switch_mutex_lock(context->streams_mutex);
				switch_core_hash_insert(context->routes, stream->jid, stream);
				switch_mutex_unlock(context->streams_mutex);
			}

			break;
		}
		case XSS_AUTHENTICATED:
		case XSS_READY:
		default:
			switch_log_printf(SWITCH_CHANNEL_UUID_LOG(stream->id), SWITCH_LOG_WARNING, "%s, %s:%i, iq UNEXPECTED <session>, state = %s\n", stream->jid, stream->address, stream->port, xmpp_stream_state_to_string(stream->state));
			reply = iks_new_error(node, STANZA_ERROR_SERVICE_UNAVAILABLE);
			break;
	}

	return reply;
}

/**
 * Handle <iq><bind> request
 * @param stream the xmpp stream
 * @param node the <iq> node
 */
static iks *on_iq_set_xmpp_bind(struct xmpp_stream *stream, iks *node)
{
	iks *reply = NULL;

	switch(stream->state) {
		case XSS_AUTHENTICATED: {
			struct xmpp_stream_context *context = stream->context;
			iks *bind = iks_find(node, "bind");
			iks *x;
			/* get optional client resource ID */
			char *resource_id = iks_find_cdata(bind, "resource");

			/* generate resource ID for client if not already set */
			if (zstr(resource_id)) {
				char resource_id_buf[SWITCH_UUID_FORMATTED_LENGTH + 1];
				switch_uuid_str(resource_id_buf, sizeof(resource_id_buf));
				resource_id = switch_core_strdup(stream->pool, resource_id_buf);
			}

			stream->jid = switch_core_sprintf(stream->pool, "%s/%s", stream->jid, resource_id);
			if (context->bind_callback && !context->bind_callback(stream)) {
				stream->jid = NULL;
				reply = iks_new_error(node, STANZA_ERROR_CONFLICT);
			} else {
				stream->state = XSS_RESOURCE_BOUND;

				reply = iks_new_iq_result(node);
				x = iks_insert(reply, "bind");
				iks_insert_attrib(x, "xmlns", IKS_NS_XMPP_BIND);
				iks_insert_cdata(iks_insert(x, "jid"), stream->jid, strlen(stream->jid));
			}
			break;
		}
		default:
			switch_log_printf(SWITCH_CHANNEL_UUID_LOG(stream->id), SWITCH_LOG_WARNING, "%s, %s:%i, iq UNEXPECTED <bind>\n", stream->jid, stream->address, stream->port);
			reply = iks_new_error(node, STANZA_ERROR_NOT_ALLOWED);
			break;
	}

	return reply;
}

/**
 * Handle <iq> message callback
 * @param stream the stream
 * @param iq the packet
 */
static void on_stream_iq(struct xmpp_stream *stream, iks *iq)
{
	struct xmpp_stream_context *context = stream->context;
	switch(stream->state) {
		case XSS_CONNECT:
		case XSS_SECURE: {
			iks *error = iks_new_error(iq, STANZA_ERROR_NOT_AUTHORIZED);
			xmpp_stream_stanza_send(stream, error);
			break;
		}
		case XSS_AUTHENTICATED: {
			iks *cmd = iks_first_tag(iq);
			if (cmd && !strcmp("bind", iks_name(cmd)) && !strcmp(IKS_NS_XMPP_BIND, iks_find_attrib_soft(cmd, "xmlns"))) {
				iks *reply = on_iq_set_xmpp_bind(stream, iq);
				xmpp_stream_stanza_send(stream, reply);
			} else {
				iks *error = iks_new_error(iq, STANZA_ERROR_SERVICE_UNAVAILABLE);
				xmpp_stream_stanza_send(stream, error);
			}
			break;
		}
		case XSS_RESOURCE_BOUND: {
			iks *cmd = iks_first_tag(iq);
			if (cmd && !strcmp("session", iks_name(cmd)) && !strcmp(IKS_NS_XMPP_SESSION, iks_find_attrib_soft(cmd, "xmlns"))) {
				iks *reply = on_iq_set_xmpp_session(stream, iq);
				xmpp_stream_stanza_send(stream, reply);
			} else {
				iks *error = iks_new_error(iq, STANZA_ERROR_SERVICE_UNAVAILABLE);
				xmpp_stream_stanza_send(stream, error);
			}
			break;
		}
		case XSS_READY: {
			/* client requests */
			if (context->recv_callback) {
				context->recv_callback(stream, iq);
			}
			break;
		}
		case XSS_SHUTDOWN:
		case XSS_DESTROY:
		case XSS_ERROR: {
			iks *error = iks_new_error(iq, STANZA_ERROR_UNEXPECTED_REQUEST);
			xmpp_stream_stanza_send(stream, error);
			break;
		}
	};
}

/**
 * Handle </stream>
 * @param stream the stream
 */
static void on_stream_stop(struct xmpp_stream *stream)
{
	if (stream->state != XSS_SHUTDOWN) {
		iks_send_raw(stream->parser, "</stream:stream>");
	}
	stream->state = XSS_DESTROY;
}

/**
 * Handle <stream> from a client
 * @param stream the stream
 * @param node the stream message
 */
static void on_client_stream_start(struct xmpp_stream *stream, iks *node)
{
	struct xmpp_stream_context *context = stream->context;
	const char *to = iks_find_attrib_soft(node, "to");
	const char *xmlns = iks_find_attrib_soft(node, "xmlns");

	/* to is optional, must be server domain if set */
	if (!zstr(to) && strcmp(context->domain, to)) {
		switch_log_printf(SWITCH_CHANNEL_UUID_LOG(stream->id), SWITCH_LOG_INFO, "%s, %s:%i, wrong server domain!\n", stream->jid, stream->address, stream->port);
		stream->state = XSS_ERROR;
		return;
	}

	/* xmlns = client */
	if (zstr(xmlns) || strcmp(xmlns, IKS_NS_CLIENT)) {
		switch_log_printf(SWITCH_CHANNEL_UUID_LOG(stream->id), SWITCH_LOG_INFO, "%s, %s:%i, wrong stream namespace!\n", stream->jid, stream->address, stream->port);
		stream->state = XSS_ERROR;
		return;
	}

	switch (stream->state) {
		case XSS_CONNECT:
			xmpp_send_client_header_tls(stream);
			break;
		case XSS_SECURE:
			xmpp_send_client_header_auth(stream);
			break;
		case XSS_AUTHENTICATED:
			/* client bind required */
			xmpp_stream_new_id(stream);
			xmpp_send_client_header_bind(stream);
			break;
		case XSS_SHUTDOWN:
			/* strange... I expect IKS_NODE_STOP, this is a workaround. */
			stream->state = XSS_DESTROY;
			break;
		case XSS_RESOURCE_BOUND:
		case XSS_READY:
		case XSS_ERROR:
		case XSS_DESTROY:
			/* bad state */
			switch_log_printf(SWITCH_CHANNEL_UUID_LOG(stream->id), SWITCH_LOG_INFO, "%s, %s:%i, bad state!\n", stream->jid, stream->address, stream->port);
			stream->state = XSS_ERROR;
			break;
	}
}

/**
 * Handle <db:result type='valid'>
 */
static void on_stream_dialback_result_valid(struct xmpp_stream *stream, iks *node)
{
	struct xmpp_stream_context *context = stream->context;

	/* TODO check domain pair and allow access if pending request exists */
	switch_log_printf(SWITCH_CHANNEL_UUID_LOG(stream->id), SWITCH_LOG_DEBUG, "%s, %s:%i, valid dialback result\n", stream->jid, stream->address, stream->port);

	if (context->ready_callback && !context->ready_callback(stream)) {
		stream->state = XSS_ERROR;
	} else {
		/* this stream is routable */
		stream->state = XSS_READY;

		/* add to available streams */
		switch_mutex_lock(context->streams_mutex);
		switch_core_hash_insert(context->routes, stream->jid, stream);
		switch_mutex_unlock(context->streams_mutex);
	}
}

/**
 * Handle <db:result type='valid'>
 */
static void on_stream_dialback_result_invalid(struct xmpp_stream *stream, iks *node)
{
	/* close stream */
	stream->state = XSS_ERROR;
	switch_log_printf(SWITCH_CHANNEL_UUID_LOG(stream->id), SWITCH_LOG_INFO, "%s, %s:%i, invalid dialback result!\n", stream->jid, stream->address, stream->port);
}

/**
 * Handle <db:result type='error'>
 */
static void on_stream_dialback_result_error(struct xmpp_stream *stream, iks *node)
{
	/* close stream */
	stream->state = XSS_ERROR;
	switch_log_printf(SWITCH_CHANNEL_UUID_LOG(stream->id), SWITCH_LOG_INFO, "%s, %s:%i, error dialback result!\n", stream->jid, stream->address, stream->port);
}

/**
 * Handle <db:result>
 */
static void on_stream_dialback_result_key(struct xmpp_stream *stream, iks *node)
{
	struct xmpp_stream_context *context = stream->context;
	const char *from = iks_find_attrib_soft(node, "from");
	const char *to = iks_find_attrib_soft(node, "to");
	iks *cdata = iks_child(node);
	iks *reply;
	const char *dialback_key = NULL;

	if (cdata && iks_type(cdata) == IKS_CDATA) {
		dialback_key = iks_cdata(cdata);
	}
	if (zstr(dialback_key)) {
		iks *error = iks_new_error_detailed(node, STANZA_ERROR_BAD_REQUEST, "Missing dialback key");
		switch_log_printf(SWITCH_CHANNEL_UUID_LOG(stream->id), SWITCH_LOG_INFO, "%s, %s:%i, dialback result missing key!\n", stream->jid, stream->address, stream->port);
		iks_send(stream->parser, error);
		iks_delete(error);
		stream->state = XSS_ERROR;
		return;
	}

	if (zstr(from)) {
		iks *error = iks_new_error_detailed(node, STANZA_ERROR_BAD_REQUEST, "Missing from");
		switch_log_printf(SWITCH_CHANNEL_UUID_LOG(stream->id), SWITCH_LOG_INFO, "%s, %s:%i, dialback result missing from!\n", stream->jid, stream->address, stream->port);
		iks_send(stream->parser, error);
		iks_delete(error);
		stream->state = XSS_ERROR;
		return;
	}

	if (zstr(to)) {
		iks *error = iks_new_error_detailed(node, STANZA_ERROR_BAD_REQUEST, "Missing to");
		switch_log_printf(SWITCH_CHANNEL_UUID_LOG(stream->id), SWITCH_LOG_INFO, "%s, %s:%i, dialback result missing to!\n", stream->jid, stream->address, stream->port);
		iks_send(stream->parser, error);
		iks_delete(error);
		stream->state = XSS_ERROR;
		return;
	}

	if (strcmp(context->domain, to)) {
		iks *error = iks_new_error(node, STANZA_ERROR_ITEM_NOT_FOUND);
		switch_log_printf(SWITCH_CHANNEL_UUID_LOG(stream->id), SWITCH_LOG_INFO, "%s, %s:%i, invalid domain!\n", stream->jid, stream->address, stream->port);
		iks_send(stream->parser, error);
		iks_delete(error);
		stream->state = XSS_ERROR;
		return;
	}

	/* this stream is not routable */
	stream->state = XSS_READY;
	stream->jid = switch_core_strdup(stream->pool, from);

	if (context->ready_callback && !context->ready_callback(stream)) {
		iks *error = iks_new_error(node, STANZA_ERROR_INTERNAL_SERVER_ERROR);
		iks_send(stream->parser, error);
		iks_delete(error);
		stream->state = XSS_ERROR;
		return;
	}

	/* TODO validate key */
	reply = iks_new("db:result");
	iks_insert_attrib(reply, "from", to);
	iks_insert_attrib(reply, "to", from);
	iks_insert_attrib(reply, "type", "valid");
	iks_send(stream->parser, reply);
	iks_delete(reply);
}

/**
 * Handle <db:result>
 */
static void on_stream_dialback_result(struct xmpp_stream *stream, iks *node)
{
	const char *type = iks_find_attrib_soft(node, "type");

	if (stream->state == XSS_ERROR || stream->state == XSS_DESTROY) {
		stream->state = XSS_ERROR;
		return;
	}

	if (zstr(type)) {
		on_stream_dialback_result_key(stream, node);
	} else if (!strcmp("valid", type)) {
		on_stream_dialback_result_valid(stream, node);
	} else if (!strcmp("invalid", type)) {
		on_stream_dialback_result_invalid(stream, node);
	} else if (!strcmp("error", type)) {
		on_stream_dialback_result_error(stream, node);
	}
}

/**
 * Handle <db:verify>
 */
static void on_stream_dialback_verify(struct xmpp_stream *stream, iks *node)
{
	struct xmpp_stream_context *context = stream->context;
	const char *from = iks_find_attrib_soft(node, "from");
	const char *id = iks_find_attrib_soft(node, "id");
	const char *to = iks_find_attrib_soft(node, "to");
	iks *cdata = iks_child(node);
	iks *reply;
	const char *dialback_key = NULL;
	char *expected_key = NULL;
	int valid;

	if (stream->state == XSS_ERROR || stream->state == XSS_DESTROY) {
		stream->state = XSS_ERROR;
		return;
	}

	if (cdata && iks_type(cdata) == IKS_CDATA) {
		dialback_key = iks_cdata(cdata);
	}
	if (zstr(dialback_key)) {
		iks *error = iks_new_error_detailed(node, STANZA_ERROR_BAD_REQUEST, "Missing dialback key");
		switch_log_printf(SWITCH_CHANNEL_UUID_LOG(stream->id), SWITCH_LOG_INFO, "%s, %s:%i, dialback verify missing key!\n", stream->jid, stream->address, stream->port);
		iks_send(stream->parser, error);
		iks_delete(error);
		return;
	}

	if (zstr(id)) {
		iks *error = iks_new_error_detailed(node, STANZA_ERROR_BAD_REQUEST, "Missing id");
		switch_log_printf(SWITCH_CHANNEL_UUID_LOG(stream->id), SWITCH_LOG_INFO, "%s, %s:%i, dialback verify missing stream ID!\n", stream->jid, stream->address, stream->port);
		iks_send(stream->parser, error);
		iks_delete(error);
		return;
	}

	if (zstr(from)) {
		iks *error = iks_new_error_detailed(node, STANZA_ERROR_BAD_REQUEST, "Missing from");
		switch_log_printf(SWITCH_CHANNEL_UUID_LOG(stream->id), SWITCH_LOG_INFO, "%s, %s:%i, dialback verify missing from!\n", stream->jid, stream->address, stream->port);
		iks_send(stream->parser, error);
		iks_delete(error);
		return;
	}

	if (zstr(to)) {
		iks *error = iks_new_error_detailed(node, STANZA_ERROR_BAD_REQUEST, "Missing to");
		switch_log_printf(SWITCH_CHANNEL_UUID_LOG(stream->id), SWITCH_LOG_INFO, "%s, %s:%i, dialback verify missing to!\n", stream->jid, stream->address, stream->port);
		iks_send(stream->parser, error);
		iks_delete(error);
		return;
	}

	if (strcmp(context->domain, to)) {
		iks *error = iks_new_error(node, STANZA_ERROR_ITEM_NOT_FOUND);
		switch_log_printf(SWITCH_CHANNEL_UUID_LOG(stream->id), SWITCH_LOG_INFO, "%s, %s:%i, invalid domain!\n", stream->jid, stream->address, stream->port);
		iks_send(stream->parser, error);
		iks_delete(error);
		return;
	}

	expected_key = iks_server_dialback_key(context->dialback_secret, from, to, id);
	valid = expected_key && !strcmp(expected_key, dialback_key);

	reply = iks_new("db:verify");
	iks_insert_attrib(reply, "from", to);
	iks_insert_attrib(reply, "to", from);
	iks_insert_attrib(reply, "id", id);
	iks_insert_attrib(reply, "type", valid ? "valid" : "invalid");
	iks_send(stream->parser, reply);
	iks_delete(reply);
	free(expected_key);

	if (!valid) {
		/* close the stream */
		stream->state = XSS_ERROR;
	}
}

/**
 * Handle <stream> from an outbound peer server
 */
static void on_outbound_server_stream_start(struct xmpp_stream *stream, iks *node)
{
	const char *xmlns = iks_find_attrib_soft(node, "xmlns");

	/* xmlns = server */
	if (zstr(xmlns) || strcmp(xmlns, IKS_NS_SERVER)) {
		switch_log_printf(SWITCH_CHANNEL_UUID_LOG(stream->id), SWITCH_LOG_INFO, "%s, %s:%i, wrong stream namespace!\n", stream->jid, stream->address, stream->port);
		stream->state = XSS_ERROR;
		return;
	}

	switch (stream->state) {
		case XSS_CONNECT: {
			/* get stream ID and send dialback */
			const char *id = iks_find_attrib_soft(node, "id");
			if (zstr(id)) {
				switch_log_printf(SWITCH_CHANNEL_UUID_LOG(stream->id), SWITCH_LOG_INFO, "%s, %s:%i, missing stream ID!\n", stream->jid, stream->address, stream->port);
				stream->state = XSS_ERROR;
				return;
			}
			xmpp_stream_set_id(stream, id);

			/* send dialback */
			xmpp_send_dialback_key(stream);
			break;
		}
		case XSS_SHUTDOWN:
			/* strange... I expect IKS_NODE_STOP, this is a workaround. */
			stream->state = XSS_DESTROY;
			break;
		case XSS_SECURE:
		case XSS_AUTHENTICATED:
		case XSS_RESOURCE_BOUND:
		case XSS_READY:
		case XSS_ERROR:
		case XSS_DESTROY:
			/* bad state */
			stream->state = XSS_ERROR;
			break;
	}
}

/**
 * Handle <stream> from an inbound peer server
 * @param stream the stream
 * @param node the stream message
 */
static void on_inbound_server_stream_start(struct xmpp_stream *stream, iks *node)
{
	struct xmpp_stream_context *context = stream->context;
	const char *to = iks_find_attrib_soft(node, "to");
	const char *xmlns = iks_find_attrib_soft(node, "xmlns");

	/* to is required, must be server domain */
	if (zstr(to) || strcmp(context->domain, to)) {
		switch_log_printf(SWITCH_CHANNEL_UUID_LOG(stream->id), SWITCH_LOG_INFO, "%s, %s:%i, wrong server domain!\n", stream->jid, stream->address, stream->port);
		stream->state = XSS_ERROR;
		return;
	}

	/* xmlns = server */
	if (zstr(xmlns) || strcmp(xmlns, IKS_NS_SERVER)) {
		switch_log_printf(SWITCH_CHANNEL_UUID_LOG(stream->id), SWITCH_LOG_INFO, "%s, %s:%i, wrong stream namespace!\n", stream->jid, stream->address, stream->port);
		stream->state = XSS_ERROR;
		return;
	}

	switch (stream->state) {
		case XSS_CONNECT:
			xmpp_send_server_header_auth(stream);
			break;
		case XSS_SECURE:
			break;
		case XSS_AUTHENTICATED: {
			if (context->ready_callback && !context->ready_callback(stream)) {
				stream->state = XSS_ERROR;
				break;
			}

			/* all set */
			xmpp_send_server_header_features(stream);
			stream->state = XSS_READY;

			/* add to available streams */
			switch_mutex_lock(context->streams_mutex);
			switch_core_hash_insert(context->routes, stream->jid, stream);
			switch_mutex_unlock(context->streams_mutex);
			break;
		}
		case XSS_SHUTDOWN:
			/* strange... I expect IKS_NODE_STOP, this is a workaround. */
			stream->state = XSS_DESTROY;
			break;
		case XSS_RESOURCE_BOUND:
		case XSS_READY:
		case XSS_ERROR:
		case XSS_DESTROY:
			/* bad state */
			stream->state = XSS_ERROR;
			break;
	}
}

/**
 * Handle XML stream callback
 * @param user_data the xmpp stream
 * @param type stream type (start/normal/stop/etc)
 * @param node optional XML node
 * @return IKS_OK
 */
static int on_stream(void *user_data, int type, iks *node)
{
	struct xmpp_stream *stream = (struct xmpp_stream *)user_data;

	stream->idle = 0;

	switch_log_printf(SWITCH_CHANNEL_UUID_LOG(stream->id), SWITCH_LOG_DEBUG, "%s, %s:%i, state = %s, node type = %s\n", stream->jid, stream->address, stream->port, xmpp_stream_state_to_string(stream->state), iks_node_type_to_string(type));

	switch(type) {
		case IKS_NODE_START:
			/* <stream> */
			if (node) {
				if (stream->s2s) {
					if (stream->incoming) {
						on_inbound_server_stream_start(stream, node);
					} else {
						on_outbound_server_stream_start(stream, node);
					}
				} else {
					on_client_stream_start(stream, node);
				}
			} else {
				stream->state = XSS_ERROR;
				switch_log_printf(SWITCH_CHANNEL_UUID_LOG(stream->id), SWITCH_LOG_INFO, "%s, %s:%i, missing node!\n", stream->jid, stream->address, stream->port);
			}
			break;
		case IKS_NODE_NORMAL:
			/* stanza */
			if (node) {
				const char *name = iks_name(node);
				if (!strcmp("iq", name) || !strcmp("message", name)) {
					on_stream_iq(stream, node);
				} else if (!strcmp("presence", name)) {
					on_stream_presence(stream, node);
				} else if (!strcmp("auth", name)) {
					on_stream_auth(stream, node);
				} else if (!strcmp("starttls", name)) {
					on_stream_starttls(stream, node);
				} else if (!strcmp("db:result", name)) {
					on_stream_dialback_result(stream, node);
				} else if (!strcmp("db:verify", name)) {
					on_stream_dialback_verify(stream, node);
				} else {
					/* unknown first-level element */
					switch_log_printf(SWITCH_CHANNEL_UUID_LOG(stream->id), SWITCH_LOG_DEBUG, "%s, %s:%i, unknown first-level element: %s\n", stream->jid, stream->address, stream->port, name);
				}
			}
			break;
		case IKS_NODE_ERROR:
			/* <error> */
			break;
		case IKS_NODE_STOP:
			on_stream_stop(stream);
			break;
	}

	if (node) {
		iks_delete(node);
	}

	return IKS_OK;
}

/**
 * Cleanup xmpp stream
 */
static void xmpp_stream_destroy(struct xmpp_stream *stream)
{
	struct xmpp_stream_context *context = stream->context;
	switch_memory_pool_t *pool = stream->pool;
	stream->state = XSS_DESTROY;

	/* remove from available streams */
	switch_mutex_lock(context->streams_mutex);
	if (stream->jid) {
		switch_core_hash_delete(context->routes, stream->jid);
	}
	if (stream->id) {
		switch_core_hash_delete(context->streams, stream->id);
	}
	switch_mutex_unlock(context->streams_mutex);

	/* close connection */
	if (stream->parser) {
		iks_disconnect(stream->parser);
		iks_parser_delete(stream->parser);
	}

	if (stream->socket) {
		switch_socket_shutdown(stream->socket, SWITCH_SHUTDOWN_READWRITE);
		switch_socket_close(stream->socket);
	}

	/* flush pending messages */
	if (stream->msg_queue) {
		char *msg;
		while (switch_queue_trypop(stream->msg_queue, (void *)&msg) == SWITCH_STATUS_SUCCESS) {
			iks_free(msg);
		}
	}

	if (context->destroy_callback) {
		context->destroy_callback(stream);
	}

	switch_core_destroy_memory_pool(&pool);
}

/**
 * @param stream the xmpp stream to check
 * @return 0 if stream is dead
 */
static int xmpp_stream_ready(struct xmpp_stream *stream)
{
	return stream->state != XSS_ERROR && stream->state != XSS_DESTROY;
}

#define KEEP_ALIVE_INTERVAL_NS (60 * 1000 * 1000)

/**
 * Thread that handles xmpp XML stream
 * @param thread this thread
 * @param obj the xmpp stream
 * @return NULL
 */
static void *SWITCH_THREAD_FUNC xmpp_stream_thread(switch_thread_t *thread, void *obj)
{
	struct xmpp_stream *stream = (struct xmpp_stream *)obj;
	struct xmpp_stream_context *context = stream->context;
	int err_count = 0;
	switch_time_t last_activity = 0;
	int ping_id = 1;

	if (stream->incoming) {
		switch_thread_rwlock_rdlock(context->shutdown_rwlock);
	}

	switch_log_printf(SWITCH_CHANNEL_UUID_LOG(stream->id), SWITCH_LOG_DEBUG, "%s:%i, New %s_%s stream\n", stream->address, stream->port, stream->s2s ? "s2s" : "c2s", stream->incoming ? "in" : "out");

	if (stream->s2s && !stream->incoming) {
		xmpp_send_outbound_server_header(stream);
	}

	while (xmpp_stream_ready(stream)) {
		char *msg;
		int result;
		switch_time_t now = switch_micro_time_now();

		/* read any messages from client */
		stream->idle = 1;
		result = iks_recv(stream->parser, 0);
		switch (result) {
		case IKS_OK:
			err_count = 0;
			break;
		case IKS_NET_TLSFAIL:
		case IKS_NET_RWERR:
		case IKS_NET_NOCONN:
		case IKS_NET_NOSOCK:
			switch_log_printf(SWITCH_CHANNEL_UUID_LOG(stream->id), SWITCH_LOG_INFO, "%s, %s:%i, iks_recv() error = %s, ending session\n", stream->jid, stream->address, stream->port, iks_net_error_to_string(result));
			stream->state = XSS_ERROR;
			goto done;
		default:
			if (err_count++ == 0) {
				switch_log_printf(SWITCH_CHANNEL_UUID_LOG(stream->id), SWITCH_LOG_INFO, "%s, %s:%i, iks_recv() error = %s\n", stream->jid, stream->address, stream->port, iks_net_error_to_string(result));
			}
			if (err_count >= 50) {
				switch_log_printf(SWITCH_CHANNEL_UUID_LOG(stream->id), SWITCH_LOG_INFO, "%s, %s:%i, too many iks_recv() error = %s, ending session\n", stream->jid, stream->address, stream->port, iks_net_error_to_string(result));
				stream->state = XSS_ERROR;
				goto done;
			}
		}

		/* send queued stanzas once stream is authorized for outbound stanzas */
		if (!stream->s2s || stream->state == XSS_READY) {
			while (switch_queue_trypop(stream->msg_queue, (void *)&msg) == SWITCH_STATUS_SUCCESS) {
				if (!stream->s2s || !stream->incoming) {
					iks_send_raw(stream->parser, msg);
				} else {
					/* TODO sent out wrong stream! */
				}
				iks_free(msg);
				stream->idle = 0;
			}
		}

		/* check for shutdown */
		if (stream->state != XSS_DESTROY && context->shutdown && stream->state != XSS_SHUTDOWN) {
			switch_log_printf(SWITCH_CHANNEL_UUID_LOG(stream->id), SWITCH_LOG_INFO, "%s, %s:%i, detected shutdown\n", stream->jid, stream->address, stream->port);
			iks_send_raw(stream->parser, "</stream:stream>");
			stream->state = XSS_SHUTDOWN;
			stream->idle = 0;
		}

		if (stream->idle) {
			int fdr = 0;

			/* send keep-alive ping if idle for a long time */
			if (stream->s2s && !stream->incoming && stream->state == XSS_READY && now - last_activity > KEEP_ALIVE_INTERVAL_NS) {
				char *ping = switch_mprintf("<iq to=\"%s\" from=\"%s\" type=\"get\" id=\"internal-%d\"><ping xmlns=\""IKS_NS_XMPP_PING"\"/></iq>",
					stream->jid, stream->context->domain, ping_id++);
				switch_log_printf(SWITCH_CHANNEL_UUID_LOG(stream->id), SWITCH_LOG_DEBUG, "%s, %s:%i, keep alive\n", stream->jid, stream->address, stream->port);
				last_activity = now;
				iks_send_raw(stream->parser, ping);
				free(ping);
			}

			switch_poll(stream->pollfd, 1, &fdr, 20000);
		} else {
			last_activity = now;
			switch_os_yield();
		}
	}

  done:

	if (stream->incoming) {
		xmpp_stream_destroy(stream);
		switch_thread_rwlock_unlock(context->shutdown_rwlock);
	}

	return NULL;
}

/**
 * Initialize the xmpp stream
 * @param context the stream context
 * @param stream the stream to initialize
 * @param pool for this stream
 * @param address remote address
 * @param port remote port
 * @param s2s true if a server-to-server stream
 * @param incoming true if incoming stream
 * @return the stream
 */
static struct xmpp_stream *xmpp_stream_init(struct xmpp_stream_context *context, struct xmpp_stream *stream, switch_memory_pool_t *pool, const char *address, int port, int s2s, int incoming)
{
	stream->context = context;
	stream->pool = pool;
	if (incoming) {
		xmpp_stream_new_id(stream);
	}
	switch_mutex_init(&stream->mutex, SWITCH_MUTEX_NESTED, pool);
	if (!zstr(address)) {
		stream->address = switch_core_strdup(pool, address);
	}
	if (port > 0) {
		stream->port = port;
	}
	stream->s2s = s2s;
	stream->incoming = incoming;
	switch_queue_create(&stream->msg_queue, MAX_QUEUE_LEN, pool);

	/* set up XMPP stream parser */
	stream->parser = iks_stream_new(stream->s2s ? IKS_NS_SERVER : IKS_NS_CLIENT, stream, on_stream);

	/* enable logging of XMPP stream */
	iks_set_log_hook(stream->parser, on_stream_log);

	return stream;
}

/**
 * Create a new xmpp stream
 * @param context the stream context
 * @param pool the memory pool for this stream
 * @param address remote address
 * @param port remote port
 * @param s2s true if server-to-server stream
 * @param incoming true if incoming stream
 * @return the new stream or NULL
 */
static struct xmpp_stream *xmpp_stream_create(struct xmpp_stream_context *context, switch_memory_pool_t *pool, const char *address, int port, int s2s, int incoming)
{
	struct xmpp_stream *stream = NULL;
	if (!(stream = switch_core_alloc(pool, sizeof(*stream)))) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Memory Error\n");
		return NULL;
	}
	return xmpp_stream_init(context, stream, pool, address, port, s2s, incoming);
}

/**
 * Thread that handles XMPP XML stream
 * @param thread this thread
 * @param obj the XMPP stream
 * @return NULL
 */
static void *SWITCH_THREAD_FUNC xmpp_outbound_stream_thread(switch_thread_t *thread, void *obj)
{
	struct xmpp_stream *stream = (struct xmpp_stream *)obj;
	struct xmpp_stream_context *context = stream->context;
	switch_socket_t *socket;
	int warned = 0;

	switch_thread_rwlock_rdlock(context->shutdown_rwlock);

	/* connect to server */
	while (!context->shutdown) {
		struct xmpp_stream *new_stream = NULL;
		switch_memory_pool_t *pool;
		switch_sockaddr_t *sa;

		if (switch_sockaddr_info_get(&sa, stream->address, SWITCH_UNSPEC, stream->port, 0, stream->pool) != SWITCH_STATUS_SUCCESS) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "%s:%i, failed to get sockaddr info!\n", stream->address, stream->port);
			goto fail;
		}

		if (switch_socket_create(&socket, switch_sockaddr_get_family(sa), SOCK_STREAM, SWITCH_PROTO_TCP, stream->pool) != SWITCH_STATUS_SUCCESS) {
			if (!warned) {
				switch_log_printf(SWITCH_CHANNEL_UUID_LOG(stream->id), SWITCH_LOG_ERROR, "%s:%i, failed to create socket!\n", stream->address, stream->port);
			}
			goto sock_fail;
		}

		switch_socket_opt_set(socket, SWITCH_SO_KEEPALIVE, 1);
		switch_socket_opt_set(socket, SWITCH_SO_TCP_NODELAY, 1);

		if (switch_socket_connect(socket, sa) != SWITCH_STATUS_SUCCESS) {
			if (!warned) {
				switch_log_printf(SWITCH_CHANNEL_UUID_LOG(stream->id), SWITCH_LOG_ERROR, "%s:%i, Socket Error!\n", stream->address, stream->port);
			}
			goto sock_fail;
		}

		if (warned) {
			switch_log_printf(SWITCH_CHANNEL_UUID_LOG(stream->id), SWITCH_LOG_ERROR, "%s:%i, connected!\n", stream->address, stream->port);
			warned = 0;
		}

		/* run the stream thread */
		xmpp_stream_set_socket(stream, socket);
		xmpp_stream_thread(thread, stream);

		/* re-establish connection if not shutdown */
		if (!context->shutdown) {
			/* create new stream for reconnection */
			switch_core_new_memory_pool(&pool);
			new_stream = xmpp_stream_create(stream->context, pool, stream->address, stream->port, 1, 0);
			new_stream->jid = switch_core_strdup(pool, stream->jid);
			xmpp_stream_destroy(stream);
			stream = new_stream;

			switch_yield(1000 * 1000); /* 1000 ms */
			continue;
		}
		break;

   sock_fail:
		if (socket) {
			switch_socket_close(socket);
			socket = NULL;
		}
		if (!warned) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Socket Error! Could not connect to %s:%i\n", stream->address, stream->port);
			warned = 1;
		}
		switch_yield(1000 * 1000); /* 1000 ms */
	}

  fail:

	xmpp_stream_destroy(stream);

	switch_thread_rwlock_unlock(context->shutdown_rwlock);
	return NULL;
}

/**
 * Set the id for this stream
 * @param stream
 * @param id
 */
static void xmpp_stream_set_id(struct xmpp_stream *stream, const char *id)
{
	struct xmpp_stream_context *context = stream->context;
	if (!zstr(stream->id)) {
		switch_mutex_lock(context->streams_mutex);
		switch_core_hash_delete(context->streams, stream->id);
		switch_mutex_unlock(context->streams_mutex);
	}
	if (!zstr(id)) {
		stream->id = switch_core_strdup(stream->pool, id);
		switch_mutex_lock(context->streams_mutex);
		switch_core_hash_insert(context->streams, stream->id, stream);
		switch_mutex_unlock(context->streams_mutex);
	} else {
		stream->id = NULL;
	}
}

/**
 * Destroy the listener
 * @param server the server
 */
static void xmpp_listener_destroy(struct xmpp_listener *listener)
{
	switch_memory_pool_t *pool = listener->pool;

	/* shutdown socket */
	if (listener->socket) {
		switch_socket_shutdown(listener->socket, SWITCH_SHUTDOWN_READWRITE);
		switch_socket_close(listener->socket);
	}
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "xmpp listener %s:%u closed\n", listener->addr, listener->port);
	switch_core_destroy_memory_pool(&pool);
}

/**
 * Open a new XMPP stream with a peer server
 * @param peer_domain of server - if not set, address is used
 * @param peer_address of server - if not set, domain is used
 * @param peer_port of server - if not set default port is used
 */
switch_status_t xmpp_stream_context_connect(struct xmpp_stream_context *context, const char *peer_domain, const char *peer_address, int peer_port)
{
	struct xmpp_stream *stream;
	switch_memory_pool_t *pool;
	switch_thread_t *thread;
	switch_threadattr_t *thd_attr = NULL;

	if (peer_port <= 0) {
		peer_port = IKS_JABBER_SERVER_PORT;
	}

	if (zstr(peer_address)) {
		peer_address = peer_domain;
	} else if (zstr(peer_domain)) {
		peer_domain = peer_address;
	}

	/* start outbound stream thread */
	switch_core_new_memory_pool(&pool);
	stream = xmpp_stream_create(context, pool, peer_address, peer_port, 1, 0);
	stream->jid = switch_core_strdup(pool, peer_domain);
	switch_threadattr_create(&thd_attr, pool);
			switch_threadattr_detach_set(thd_attr, 1);
			switch_threadattr_stacksize_set(thd_attr, SWITCH_THREAD_STACKSIZE);
			switch_thread_create(&thread, thd_attr, xmpp_outbound_stream_thread, stream, pool);

	return SWITCH_STATUS_SUCCESS;
}

/**
 * Thread that listens for new XMPP connections
 * @param thread this thread
 * @param obj the listener
 * @return NULL
 */
static void *SWITCH_THREAD_FUNC xmpp_listener_thread(switch_thread_t *thread, void *obj)
{
	struct xmpp_listener *listener = (struct xmpp_listener *)obj;
	struct xmpp_stream_context *context = listener->context;
	switch_memory_pool_t *pool = NULL;
	uint32_t errs = 0;
	int warned = 0;

	switch_thread_rwlock_rdlock(context->shutdown_rwlock);

	/* bind to XMPP port */
	while (!context->shutdown) {
		switch_status_t rv;
		switch_sockaddr_t *sa;
		rv = switch_sockaddr_info_get(&sa, listener->addr, SWITCH_UNSPEC, listener->port, 0, listener->pool);
		if (rv)
			goto fail;
		rv = switch_socket_create(&listener->socket, switch_sockaddr_get_family(sa), SOCK_STREAM, SWITCH_PROTO_TCP, listener->pool);
		if (rv)
			goto sock_fail;
		rv = switch_socket_opt_set(listener->socket, SWITCH_SO_REUSEADDR, 1);
		if (rv)
			goto sock_fail;
#ifdef WIN32
		/* Enable dual-stack listening on Windows (if the listening address is IPv6), it's default on Linux */
		if (switch_sockaddr_get_family(sa) == AF_INET6) {
			rv = switch_socket_opt_set(listener->socket, 16384, 0);
			if (rv) goto sock_fail;
		}
#endif
		rv = switch_socket_bind(listener->socket, sa);
		if (rv)
			goto sock_fail;
		rv = switch_socket_listen(listener->socket, 5);
		if (rv)
			goto sock_fail;

		rv = switch_socket_create_pollset(&listener->read_pollfd, listener->socket, SWITCH_POLLIN | SWITCH_POLLERR, listener->pool);
		if (rv) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Create pollset for %s listener socket %s:%u error!\n", listener->s2s ? "s2s" : "c2s", listener->addr, listener->port);
			goto sock_fail;
		}

		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "xmpp %s listener bound to %s:%u\n", listener->s2s ? "s2s" : "c2s", listener->addr, listener->port);

		break;
   sock_fail:
		if (listener->socket) {
			switch_socket_close(listener->socket);
			listener->socket = NULL;
		}
		if (!warned) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Socket Error! xmpp %s listener could not bind to %s:%u\n", listener->s2s ? "s2s" : "c2s", listener->addr, listener->port);
			warned = 1;
		}
		switch_yield(1000 * 100); /* 100 ms */
	}

	/* Listen for XMPP client connections */
	while (!context->shutdown) {
		switch_socket_t *socket = NULL;
		switch_status_t rv;
		int32_t fdr;

		if (pool == NULL && switch_core_new_memory_pool(&pool) != SWITCH_STATUS_SUCCESS) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Failed to create memory pool for new client connection!\n");
			goto fail;
		}

		/* is there a new connection? */
		rv = switch_poll(listener->read_pollfd, 1, &fdr, 1000 * 1000 /* 1000 ms */);
		if (rv != SWITCH_STATUS_SUCCESS) {
			continue;
		}

		/* accept the connection */
		if ((rv = switch_socket_accept(&socket, listener->socket, pool))) {
			if (context->shutdown) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "Shutting down xmpp listener\n");
				goto end;
			} else {
				/* I wish we could use strerror_r here but its not defined everywhere =/ */
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Accept connection error [%s]\n", strerror(errno));
				if (++errs > 100) {
					goto end;
				}
			}
		} else { /* got a new connection */
			switch_thread_t *thread;
			switch_threadattr_t *thd_attr = NULL;
			struct xmpp_stream *stream;
			switch_sockaddr_t *sa = NULL;
			char remote_ip[50] = { 0 };
			int remote_port = 0;

			errs = 0;

			/* get remote address and port */
			if (switch_socket_addr_get(&sa, SWITCH_TRUE, socket) == SWITCH_STATUS_SUCCESS && sa) {
				switch_get_addr(remote_ip, sizeof(remote_ip), sa);
				remote_port = switch_sockaddr_get_port(sa);
			}

			if (zstr_buf(remote_ip)) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "Failed to get IP of incoming connection.\n");
				switch_socket_shutdown(socket, SWITCH_SHUTDOWN_READWRITE);
				switch_socket_close(socket);
				continue;
			}

			/* check if connection is allowed */
			if (listener->acl) {
				if (!switch_check_network_list_ip(remote_ip, listener->acl)) {
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "ACL %s denies access to %s.\n", listener->acl, remote_ip);
					switch_socket_shutdown(socket, SWITCH_SHUTDOWN_READWRITE);
					switch_socket_close(socket);
					continue;
				}
			}

			/* start connection thread */
			if (!(stream = xmpp_stream_create(context, pool, remote_ip, remote_port, listener->s2s, 1))) {
				switch_socket_shutdown(socket, SWITCH_SHUTDOWN_READWRITE);
				switch_socket_close(socket);
				break;
			}
			xmpp_stream_set_socket(stream, socket);
			pool = NULL; /* connection now owns the pool */
			switch_threadattr_create(&thd_attr, stream->pool);
			switch_threadattr_detach_set(thd_attr, 1);
			switch_threadattr_stacksize_set(thd_attr, SWITCH_THREAD_STACKSIZE);
			switch_thread_create(&thread, thd_attr, xmpp_stream_thread, stream, stream->pool);
		}
	}

  end:

	if (pool) {
		switch_core_destroy_memory_pool(&pool);
	}

  fail:

	xmpp_listener_destroy(listener);

	switch_thread_rwlock_unlock(context->shutdown_rwlock);
	return NULL;
}

/**
 * Add a new socket to listen for XMPP client/server connections.
 * @param context the XMPP context
 * @param addr the IP address
 * @param port the port
 * @param is_s2s true if s2s
 * @param acl name of optional access control list
 * @return SWITCH_STATUS_SUCCESS if successful
 */
switch_status_t xmpp_stream_context_listen(struct xmpp_stream_context *context, const char *addr, int port, int is_s2s, const char *acl)
{
	switch_memory_pool_t *pool;
	struct xmpp_listener *new_listener = NULL;
	switch_thread_t *thread;
	switch_threadattr_t *thd_attr = NULL;

	if (zstr(addr)) {
		return SWITCH_STATUS_FALSE;
	}

	switch_core_new_memory_pool(&pool);
	new_listener = switch_core_alloc(pool, sizeof(*new_listener));
	new_listener->pool = pool;
	new_listener->addr = switch_core_strdup(pool, addr);
	if (!zstr(acl)) {
		new_listener->acl = switch_core_strdup(pool, acl);
	}

	new_listener->s2s = is_s2s;
	if (port <= 0) {
		new_listener->port = is_s2s ? IKS_JABBER_SERVER_PORT : IKS_JABBER_PORT;
	} else {
		new_listener->port = port;
	}
	new_listener->context = context;

	/* start the server thread */
	switch_threadattr_create(&thd_attr, pool);
	switch_threadattr_detach_set(thd_attr, 1);
	switch_threadattr_stacksize_set(thd_attr, SWITCH_THREAD_STACKSIZE);
	switch_thread_create(&thread, thd_attr, xmpp_listener_thread, new_listener, pool);

	return SWITCH_STATUS_SUCCESS;
}

/**
 * Queue a message for delivery
 */
void xmpp_stream_context_send(struct xmpp_stream_context *context, const char *jid, iks *msg)
{
	if (!zstr(jid)) {
		if (msg) {
			struct xmpp_stream *stream;
			switch_mutex_lock(context->streams_mutex);
			stream = switch_core_hash_find(context->routes, jid);
			if (stream) {
				char *raw = iks_string(NULL, msg);
				if (switch_queue_trypush(stream->msg_queue, raw) != SWITCH_STATUS_SUCCESS) {
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "%s, %s:%i, failed to deliver outbound message via %s!\n", stream->jid, stream->address, stream->port, jid);
					iks_free(raw);
				}
			} else {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "%s stream is gone\n", jid);
				/* TODO automatically open connection if valid domain JID? */
			}
			switch_mutex_unlock(context->streams_mutex);
		} else {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "missing message\n");
		}
	} else {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "missing stream JID\n");
	}
}

/**
 * Dump xmpp stream stats
 */
void xmpp_stream_context_dump(struct xmpp_stream_context *context, switch_stream_handle_t *stream)
{
	switch_hash_index_t *hi;
	switch_mutex_lock(context->streams_mutex);
	stream->write_function(stream, "\nACTIVE STREAMS\n");
	for (hi = switch_core_hash_first(context->streams); hi; hi = switch_core_hash_next(&hi)) {
		struct xmpp_stream *s = NULL;
		const void *key;
		void *val;
		switch_core_hash_this(hi, &key, NULL, &val);
		s = (struct xmpp_stream *)val;
		switch_assert(s);
		stream->write_function(stream, "        TYPE='%s_%s',ID='%s',JID='%s',REMOTE_ADDRESS='%s',REMOTE_PORT=%i,STATE='%s'\n", s->s2s ? "s2s" : "c2s", s->incoming ? "in" : "out", s->id, s->jid, s->address, s->port, xmpp_stream_state_to_string(s->state));
	}
	switch_mutex_unlock(context->streams_mutex);
}

/**
 * Create a new XMPP stream context
 * @param domain for new streams
 * @param domain_secret domain shared secret for server dialback
 * @param bind_cb callback function when a resource is bound to a new stream
 * @param ready callback function when new stream is ready
 * @param recv callback function when a new stanza is received
 * @param destroy callback function when a stream is destroyed
 * @return the context
 */
struct xmpp_stream_context *xmpp_stream_context_create(const char *domain, const char *domain_secret, xmpp_stream_bind_callback bind_cb, xmpp_stream_ready_callback ready, xmpp_stream_recv_callback recv, xmpp_stream_destroy_callback destroy)
{
	switch_memory_pool_t *pool;
	struct xmpp_stream_context *context;

	switch_core_new_memory_pool(&pool);
	context = switch_core_alloc(pool, sizeof(*context));
	context->pool = pool;
	switch_mutex_init(&context->streams_mutex, SWITCH_MUTEX_NESTED, context->pool);
	switch_core_hash_init(&context->routes);
	switch_core_hash_init(&context->streams);
	context->dialback_secret = switch_core_strdup(context->pool, domain_secret);
	context->bind_callback = bind_cb;
	context->ready_callback = ready;
	context->destroy_callback = destroy;
	context->recv_callback = recv;
	context->shutdown = 0;
	context->domain = switch_core_strdup(context->pool, domain);
	switch_thread_rwlock_create(&context->shutdown_rwlock, context->pool);
	switch_core_hash_init(&context->users);

	return context;
}

/**
 * Add an authorized user
 * @param context the context to add user to
 * @param user the username
 * @param password the password
 */
void xmpp_stream_context_add_user(struct xmpp_stream_context *context, const char *user, const char *password)
{
	switch_core_hash_insert(context->users, user, switch_core_strdup(context->pool, password));
}

/**
 * Destroy an XMPP stream context.  All open streams are closed.
 * @param context to destroy
 */
void xmpp_stream_context_destroy(struct xmpp_stream_context *context)
{
	switch_memory_pool_t *pool;
	context->shutdown = 1;
	/* wait for threads to finish */
	switch_thread_rwlock_wrlock(context->shutdown_rwlock);
	switch_core_hash_destroy(&context->routes);
	switch_core_hash_destroy(&context->streams);
	switch_core_hash_destroy(&context->users);
	pool = context->pool;
	switch_core_destroy_memory_pool(&pool);
}

/**
 * @param stream
 * @return true if server-to-server stream
 */
int xmpp_stream_is_s2s(struct xmpp_stream *stream)
{
	return stream->s2s;
}

/**
 * @param stream
 * @return true if incoming stream
 */
int xmpp_stream_is_incoming(struct xmpp_stream *stream)
{
	return stream->incoming;
}

/**
 * @param stream
 * @return the stream JID
 */
const char *xmpp_stream_get_jid(struct xmpp_stream *stream)
{
	return stream->jid;
}

/**
 * Set private data for this stream
 */
void xmpp_stream_set_private(struct xmpp_stream *stream, void *user_private)
{
	stream->user_private = user_private;
}

/**
 * Get private data for this stream
 */
void *xmpp_stream_get_private(struct xmpp_stream *stream)
{
	return stream->user_private;
}

/**
 * Add PEM cert file to stream for new SSL connections
 */
void xmpp_stream_context_add_cert(struct xmpp_stream_context *context, const char *cert_pem_file)
{
	context->cert_pem_file = switch_core_strdup(context->pool, cert_pem_file);
}

/**
 * Add PEM key file to stream for new SSL connections
 */
void xmpp_stream_context_add_key(struct xmpp_stream_context *context, const char *key_pem_file)
{
	context->key_pem_file = switch_core_strdup(context->pool, key_pem_file);
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
