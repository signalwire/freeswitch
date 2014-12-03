/*
 * mod_rayo for FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
 * Copyright (C) 2013-2014, Grasshopper
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
 * mod_rayo.c -- Rayo server / node implementation.  Allows MxN clustering of FreeSWITCH and Rayo Clients (like Adhearsion)
 *
 */
#include <switch.h>
#include <iksemel.h>

#include "mod_rayo.h"
#include "rayo_components.h"
#include "rayo_elements.h"
#include "xmpp_streams.h"

SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_rayo_shutdown);
SWITCH_MODULE_LOAD_FUNCTION(mod_rayo_load);
SWITCH_MODULE_RUNTIME_FUNCTION(mod_rayo_runtime);
SWITCH_MODULE_DEFINITION(mod_rayo, mod_rayo_load, mod_rayo_shutdown, mod_rayo_runtime);

#define RAYO_CAUSE_HANGUP SWITCH_CAUSE_NORMAL_CLEARING
#define RAYO_CAUSE_DECLINE SWITCH_CAUSE_CALL_REJECTED
#define RAYO_CAUSE_BUSY SWITCH_CAUSE_USER_BUSY
#define RAYO_CAUSE_ERROR SWITCH_CAUSE_NORMAL_TEMPORARY_FAILURE

#define RAYO_END_REASON_HANGUP "hungup"
#define RAYO_END_REASON_HANGUP_LOCAL "hangup-command"
#define RAYO_END_REASON_TIMEOUT "timeout"
#define RAYO_END_REASON_BUSY "busy"
#define RAYO_END_REASON_REJECT "rejected"
#define RAYO_END_REASON_ERROR "error"

#define RAYO_SIP_REQUEST_HEADER "sip_h_"
#define RAYO_SIP_RESPONSE_HEADER "sip_rh_"
#define RAYO_SIP_PROVISIONAL_RESPONSE_HEADER "sip_ph_"
#define RAYO_SIP_BYE_RESPONSE_HEADER "sip_bye_h_"

#define RAYO_CONFIG_FILE "rayo.conf"

#define JOINED_CALL 1
#define JOINED_MIXER 2

struct rayo_actor;
struct rayo_client;
struct rayo_call;

#define rayo_call_get_uuid(call) RAYO_ID(call)

/**
 * Function pointer wrapper for the handlers hash
 */
struct rayo_xmpp_handler {
	const char *from_type;
	const char *from_subtype;
	const char *to_type;
	const char *to_subtype;
	rayo_actor_xmpp_handler fn;
};

/**
 * Client availability
 */
enum presence_status {
	PS_UNKNOWN = -1,
	PS_OFFLINE = 0,
	PS_ONLINE = 1
};

/**
 * A xmpp peer server that routes messages to/from clients
 */
struct rayo_peer_server {
	/** base class */
	struct rayo_actor base;
	/** clients connected via this server */
	switch_hash_t *clients;
};
#define RAYO_PEER_SERVER(x) ((struct rayo_peer_server *)x)

/**
 * A Rayo client that controls calls
 */
struct rayo_client {
	/** base class */
	struct rayo_actor base;
	/** availability */
	enum presence_status availability;
	/** set if reachable via s2s */
	struct rayo_peer_server *peer_server;
	/** domain or full JID to route to */
	const char *route;
	/** time when last probe was sent */
	switch_time_t last_probe;
};
#define RAYO_CLIENT(x) ((struct rayo_client *)x)

/**
 * A call controlled by a Rayo client
 */
struct rayo_call {
	/** actor base class */
	struct rayo_actor base;
	/** Definitive controlling party JID */
	char *dcp_jid;
	/** Potential controlling parties */
	switch_hash_t *pcps;
	/** current idle start time */
	switch_time_t idle_start_time;
	/** true if fax is in progress */
	int faxing;
	/** 1 if joined to call, 2 if joined to mixer */
	int joined;
	/** pending join */
	iks *pending_join_request;
	/** ID of joined party TODO this will be many mixers / calls */
	const char *joined_id;
	/** set if response needs to be sent to IQ request */
	const char *dial_request_id;
	/** channel destroy event */
	switch_event_t *end_event;
	/** True if ringing event sent to client */
	int ringing_sent;
	/** true if rayo app has started */
	int rayo_app_started;
	/** delayed delivery of answer event because rayo APP wasn't started yet */
	switch_event_t *answer_event;
	/** True if request to create this call failed */
	int dial_request_failed;
};

/**
 * A conference
 */
struct rayo_mixer {
	/** actor base class */
	struct rayo_actor base;
	/** member JIDs */
	switch_hash_t *members;
	/** subscriber JIDs */
	switch_hash_t *subscribers;
};

/**
 * A member of a mixer
 */
struct rayo_mixer_member {
	/** JID of member */
	const char *jid;
	/** Controlling party JID */
	const char *dcp_jid;
};

/**
 * A subscriber to mixer events
 */
struct rayo_mixer_subscriber {
	/** JID of client */
	const char *jid;
	/** Number of client's calls in mixer */
	int ref_count;
};

/**
 * Module state
 */
static struct {
	/** module memory pool */
	switch_memory_pool_t *pool;
	/** Rayo <iq> set commands mapped to functions */
	switch_hash_t *command_handlers;
	/** Rayo <presence> events mapped to functions */
	switch_hash_t *event_handlers;
	/** Active Rayo actors mapped by JID */
	switch_hash_t *actors;
	/** Rayo actors pending destruction */
	switch_hash_t *destroy_actors;
	/** Active Rayo actors mapped by internal ID */
	switch_hash_t *actors_by_id;
	/** synchronizes access to actors */
	switch_mutex_t *actors_mutex;
	/** map of DCP JID to client */
	switch_hash_t *clients_roster;
	/** synchronizes access to available clients */
	switch_mutex_t *clients_mutex;
	/** server for calls/mixers/etc */
	struct rayo_actor *server;
	/** Maximum idle time before call is considered abandoned */
	int max_idle_ms;
	/** Conference profile to use for mixers */
	char *mixer_conf_profile;
	/** to URI prefixes mapped to gateways */
	switch_hash_t *dial_gateways;
	/** synchronizes access to dial gateways */
	switch_mutex_t *dial_gateways_mutex;
	/** console command aliases */
	switch_hash_t *cmd_aliases;
	/** global console */
	struct rayo_client *console;
	/** XMPP context */
	struct xmpp_stream_context *xmpp_context;
	/** number of message threads */
	int num_message_threads;
	/** message delivery queue */
	switch_queue_t *msg_queue;
	/** shutdown flag */
	int shutdown;
	/** prevents context shutdown until all threads are finished */
	switch_thread_rwlock_t *shutdown_rwlock;
	/** if true, URI is put in from/to of offer if available */
	int offer_uri;
	/** if true, pause inbound calling if all clients are offline */
	int pause_when_offline;
	/** flag to reduce log noise */
	int offline_logged;
	/** if true, channel variables are added to offer */
	int add_variables_to_offer;
} globals;

/**
 * An outbound dial gateway
 */
struct dial_gateway {
	/** URI prefix to match */
	const char *uri_prefix;
	/** dial prefix to match */
	const char *dial_prefix;
	/** number of digits to strip from dialstring */
	int strip;
};

static void rayo_call_send(struct rayo_actor *call, struct rayo_message *msg);
static void rayo_server_send(struct rayo_actor *server, struct rayo_message *msg);
static void rayo_mixer_send(struct rayo_actor *mixer, struct rayo_message *msg);
static void rayo_component_send(struct rayo_actor *component, struct rayo_message *msg);
static void rayo_client_send(struct rayo_actor *client, struct rayo_message *msg);
static void rayo_console_client_send(struct rayo_actor *client, struct rayo_message *msg);

static void on_client_presence(struct rayo_client *rclient, iks *node);

typedef switch_bool_t (* rayo_actor_match_fn)(struct rayo_actor *);

static switch_bool_t is_call_actor(struct rayo_actor *actor);

static void rayo_call_send_end(struct rayo_call *call, switch_event_t *event, int local_hangup, const char *cause_str, const char *cause_q850_str);


/**
 * Entity features returned by service discovery
 */
struct entity_identity {
	/** identity category */
	const char *category;
	/** identity type */
	const char *type;
};

static struct entity_identity rayo_server_identity = { "server", "im" };
static const char *rayo_server_features[] = { IKS_NS_XMPP_ENTITY_CAPABILITIES, IKS_NS_XMPP_DISCO, RAYO_NS, RAYO_CPA_NS, RAYO_FAX_NS, 0 };

static struct entity_identity rayo_mixer_identity = { "client", "rayo_mixer" };
static const char *rayo_mixer_features[] = { 0 };

static struct entity_identity rayo_call_identity = { "client", "rayo_call" };
static const char *rayo_call_features[] = { 0 };

/**
 * Calculate SHA-1 hash of entity capabilities
 * @param identity of entity
 * @param features of identity (NULL terminated)
 * @return base64 hash (free when done)
 */
static char *calculate_entity_sha1_ver(struct entity_identity *identity, const char **features)
{
	int i;
	const char *feature;
	char ver[SHA_1_HASH_BUF_SIZE + 1] = { 0 };
	iksha *sha;

	sha = iks_sha_new();
	iks_sha_hash(sha, (const unsigned char *)identity->category, strlen(identity->category), 0);
	iks_sha_hash(sha, (const unsigned char *)"/", 1, 0);
	iks_sha_hash(sha, (const unsigned char *)identity->type, strlen(identity->type), 0);
	iks_sha_hash(sha, (const unsigned char *)"//", 2, 0);
	i = 0;
	while ((feature = features[i++])) {
		iks_sha_hash(sha, (const unsigned char *)"<", 1, 0);
		iks_sha_hash(sha, (const unsigned char *)feature, strlen(feature), 0);
	}
	iks_sha_hash(sha, (const unsigned char *)"<", 1, 1);
	iks_sha_print_base64(sha, ver);
	iks_sha_delete(sha);

	return strdup(ver);
}

/**
 * @param msg to check
 * @return true if message was sent by admin client (console)
 */
static int is_admin_client_message(struct rayo_message *msg)
{
	return !zstr(msg->from_jid) && !strcmp(RAYO_JID(globals.console), msg->from_jid);
}

/**
 * @param msg to check
 * @return true if from/to bare JIDs match
 */
static int is_internal_message(struct rayo_message *msg)
{
	return msg->from && msg->to && (iks_id_cmp(msg->from, msg->to, IKS_ID_PARTIAL) == 0);
}

/**
 * Presence status
 * @param status the presence status
 * @return the string value of status
 */
static const char *presence_status_to_string(enum presence_status status)
{
	switch(status) {
		case PS_OFFLINE: return "OFFLINE";
		case PS_ONLINE: return "ONLINE";
		case PS_UNKNOWN: return "UNKNOWN";
	}
	return "UNKNOWN";
}

/**
 * Get rayo cause code from FS hangup cause
 * @param cause FS hangup cause
 * @return rayo end cause
 */
static const char *switch_cause_to_rayo_cause(switch_call_cause_t cause)
{
	switch (cause) {
		case SWITCH_CAUSE_NONE:
		case SWITCH_CAUSE_NORMAL_CLEARING:
			return RAYO_END_REASON_HANGUP;

		case SWITCH_CAUSE_UNALLOCATED_NUMBER:
		case SWITCH_CAUSE_NO_ROUTE_TRANSIT_NET:
		case SWITCH_CAUSE_NO_ROUTE_DESTINATION:
		case SWITCH_CAUSE_CHANNEL_UNACCEPTABLE:
			return RAYO_END_REASON_ERROR;

		case SWITCH_CAUSE_CALL_AWARDED_DELIVERED:
			return RAYO_END_REASON_HANGUP;

		case SWITCH_CAUSE_USER_BUSY:
			return RAYO_END_REASON_BUSY;

		case SWITCH_CAUSE_NO_USER_RESPONSE:
		case SWITCH_CAUSE_NO_ANSWER:
			return RAYO_END_REASON_TIMEOUT;

		case SWITCH_CAUSE_SUBSCRIBER_ABSENT:
			return RAYO_END_REASON_ERROR;

		case SWITCH_CAUSE_CALL_REJECTED:
			return RAYO_END_REASON_REJECT;

		case SWITCH_CAUSE_NUMBER_CHANGED:
		case SWITCH_CAUSE_REDIRECTION_TO_NEW_DESTINATION:
		case SWITCH_CAUSE_EXCHANGE_ROUTING_ERROR:
		case SWITCH_CAUSE_DESTINATION_OUT_OF_ORDER:
		case SWITCH_CAUSE_INVALID_NUMBER_FORMAT:
			return RAYO_END_REASON_ERROR;

		case SWITCH_CAUSE_FACILITY_REJECTED:
			return RAYO_END_REASON_REJECT;

		case SWITCH_CAUSE_RESPONSE_TO_STATUS_ENQUIRY:
		case SWITCH_CAUSE_NORMAL_UNSPECIFIED:
			return RAYO_END_REASON_HANGUP;

		case SWITCH_CAUSE_NORMAL_CIRCUIT_CONGESTION:
		case SWITCH_CAUSE_NETWORK_OUT_OF_ORDER:
		case SWITCH_CAUSE_NORMAL_TEMPORARY_FAILURE:
		case SWITCH_CAUSE_SWITCH_CONGESTION:
		case SWITCH_CAUSE_ACCESS_INFO_DISCARDED:
		case SWITCH_CAUSE_REQUESTED_CHAN_UNAVAIL:
		case SWITCH_CAUSE_PRE_EMPTED:
		case SWITCH_CAUSE_FACILITY_NOT_SUBSCRIBED:
		case SWITCH_CAUSE_OUTGOING_CALL_BARRED:
		case SWITCH_CAUSE_INCOMING_CALL_BARRED:
		case SWITCH_CAUSE_BEARERCAPABILITY_NOTAUTH:
		case SWITCH_CAUSE_BEARERCAPABILITY_NOTAVAIL:
		case SWITCH_CAUSE_SERVICE_UNAVAILABLE:
		case SWITCH_CAUSE_BEARERCAPABILITY_NOTIMPL:
		case SWITCH_CAUSE_CHAN_NOT_IMPLEMENTED:
		case SWITCH_CAUSE_FACILITY_NOT_IMPLEMENTED:
		case SWITCH_CAUSE_SERVICE_NOT_IMPLEMENTED:
		case SWITCH_CAUSE_INVALID_CALL_REFERENCE:
		case SWITCH_CAUSE_INCOMPATIBLE_DESTINATION:
		case SWITCH_CAUSE_INVALID_MSG_UNSPECIFIED:
		case SWITCH_CAUSE_MANDATORY_IE_MISSING:
			return RAYO_END_REASON_ERROR;

		case SWITCH_CAUSE_MESSAGE_TYPE_NONEXIST:
		case SWITCH_CAUSE_WRONG_MESSAGE:
		case SWITCH_CAUSE_IE_NONEXIST:
		case SWITCH_CAUSE_INVALID_IE_CONTENTS:
		case SWITCH_CAUSE_WRONG_CALL_STATE:
		case SWITCH_CAUSE_RECOVERY_ON_TIMER_EXPIRE:
		case SWITCH_CAUSE_MANDATORY_IE_LENGTH_ERROR:
		case SWITCH_CAUSE_PROTOCOL_ERROR:
			return RAYO_END_REASON_ERROR;

		case SWITCH_CAUSE_INTERWORKING:
		case SWITCH_CAUSE_SUCCESS:
		case SWITCH_CAUSE_ORIGINATOR_CANCEL:
			return RAYO_END_REASON_HANGUP;

		case SWITCH_CAUSE_CRASH:
		case SWITCH_CAUSE_SYSTEM_SHUTDOWN:
		case SWITCH_CAUSE_LOSE_RACE:
		case SWITCH_CAUSE_MANAGER_REQUEST:
		case SWITCH_CAUSE_BLIND_TRANSFER:
		case SWITCH_CAUSE_ATTENDED_TRANSFER:
		case SWITCH_CAUSE_ALLOTTED_TIMEOUT:
		case SWITCH_CAUSE_USER_CHALLENGE:
		case SWITCH_CAUSE_MEDIA_TIMEOUT:
		case SWITCH_CAUSE_PICKED_OFF:
		case SWITCH_CAUSE_USER_NOT_REGISTERED:
		case SWITCH_CAUSE_PROGRESS_TIMEOUT:
		case SWITCH_CAUSE_INVALID_GATEWAY:
		case SWITCH_CAUSE_GATEWAY_DOWN:
		case SWITCH_CAUSE_INVALID_URL:
		case SWITCH_CAUSE_INVALID_PROFILE:
		case SWITCH_CAUSE_NO_PICKUP:
		case SWITCH_CAUSE_SRTP_READ_ERROR:
			return RAYO_END_REASON_ERROR;
	}
	return RAYO_END_REASON_HANGUP;
}

/**
 * Add <header> to node
 * @param node to add <header> to
 * @param name of header
 * @param value of header
 */
static void add_header(iks *node, const char *name, const char *value)
{
	if (!zstr(name) && !zstr(value)) {
		iks *header = iks_insert(node, "header");
		iks_insert_attrib(header, "name", name);
		iks_insert_attrib(header, "value", value);
	}
}

static void pause_inbound_calling(void)
{
	int32_t arg = 1;
	switch_mutex_lock(globals.clients_mutex);
	switch_core_session_ctl(SCSC_PAUSE_INBOUND, &arg);
	if (!globals.offline_logged) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "Pausing inbound calling\n");
		globals.offline_logged = 1;
	}
	switch_mutex_unlock(globals.clients_mutex);
}

static void resume_inbound_calling(void)
{
	int32_t arg = 0;
	switch_mutex_lock(globals.clients_mutex);
	switch_core_session_ctl(SCSC_PAUSE_INBOUND, &arg);
	if (globals.offline_logged) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "Resuming inbound calling\n");
		globals.offline_logged = 0;
	}
	switch_mutex_unlock(globals.clients_mutex);
}

/**
 * Check online status of rayo client(s) and pause/resume the server
 */
static void pause_when_offline(void)
{
	if (globals.pause_when_offline) {
		int is_online = 0;
		switch_hash_index_t *hi;

		switch_mutex_lock(globals.clients_mutex);

		for (hi = switch_core_hash_first(globals.clients_roster); hi; hi = switch_core_hash_next(&hi)) {
			const void *key;
			void *client;
			switch_core_hash_this(hi, &key, NULL, &client);
			switch_assert(client);
			if (RAYO_CLIENT(client)->availability == PS_ONLINE) {
				is_online = 1;
				break;
			}
		}
		switch_safe_free(hi);

		if (is_online) {
			resume_inbound_calling();
		} else {
			pause_inbound_calling();
		}

		switch_mutex_unlock(globals.clients_mutex);
	}
}

/**
 * Send event to clients
 * @param from event sender
 * @param rayo_event the event to send
 * @param online_only only send to online clients
 */
static void broadcast_event(struct rayo_actor *from, iks *rayo_event, int online_only)
{
	switch_hash_index_t *hi = NULL;
	switch_mutex_lock(globals.clients_mutex);
	for (hi = switch_core_hash_first(globals.clients_roster); hi; hi = switch_core_hash_next(&hi)) {
		struct rayo_client *rclient;
		const void *key;
		void *val;
		switch_core_hash_this(hi, &key, NULL, &val);
		rclient = (struct rayo_client *)val;
		switch_assert(rclient);

		if (!online_only || rclient->availability == PS_ONLINE) {
			iks_insert_attrib(rayo_event, "to", RAYO_JID(rclient));
			RAYO_SEND_MESSAGE_DUP(from, RAYO_JID(rclient), rayo_event);
		}
	}
	switch_mutex_unlock(globals.clients_mutex);
}

/**
 * Add an outbound dialing gateway
 * @param uri_prefix to match
 * @param dial_prefix to use
 * @param strip number of digits to strip from dialstring
 */
static void dial_gateway_add(const char *uri_prefix, const char *dial_prefix, int strip)
{
	struct dial_gateway *gateway = switch_core_alloc(globals.pool, sizeof(*gateway));
	gateway->uri_prefix = uri_prefix ? switch_core_strdup(globals.pool, uri_prefix) : "";
	gateway->dial_prefix = dial_prefix ? switch_core_strdup(globals.pool, dial_prefix) : "";
	gateway->strip = strip > 0 ? strip : 0;
	switch_core_hash_insert(globals.dial_gateways, uri_prefix, gateway);
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "dial-gateway uriprefix = %s, dialprefix = %s, strip = %i\n", uri_prefix, dial_prefix, strip);
}

/**
 * Find outbound dial gateway for the specified dialstring
 */
static struct dial_gateway *dial_gateway_find(const char *uri)
{
	switch_hash_index_t *hi = NULL;
	int match_len = 0;
	struct dial_gateway *gateway = (struct dial_gateway *)switch_core_hash_find(globals.dial_gateways, "default");

	/* find longest prefix match */
	switch_mutex_lock(globals.dial_gateways_mutex);
	for (hi = switch_core_hash_first(globals.dial_gateways); hi; hi = switch_core_hash_next(&hi)) {
		struct dial_gateway *candidate = NULL;
		const void *prefix;
		int prefix_len = 0;
		void *val;
		switch_core_hash_this(hi, &prefix, NULL, &val);
		candidate = (struct dial_gateway *)val;
		switch_assert(candidate);

		prefix_len = strlen(prefix);
		if (!zstr(prefix) && !strncmp(prefix, uri, prefix_len) && prefix_len > match_len) {
			match_len = prefix_len;
			gateway = candidate;
		}
	}
	switch_mutex_unlock(globals.dial_gateways_mutex);
	return gateway;
}

/**
 * Add command handler function
 * @param name the command name
 * @param handler the command handler function
 */
static void rayo_command_handler_add(const char *name, struct rayo_xmpp_handler *handler)
{
	char full_name[1024];
	full_name[1023] = '\0';
	snprintf(full_name, sizeof(full_name) - 1, "%s:%s:%s", handler->to_type, handler->to_subtype, name);
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Adding command: %s\n", full_name);
	switch_core_hash_insert(globals.command_handlers, full_name, handler);
}

/**
 * Add command handler function
 * @param type the actor type
 * @param subtype the actor subtype
 * @param name the command name
 * @param fn the command callback function
 */
void rayo_actor_command_handler_add(const char *type, const char *subtype, const char *name, rayo_actor_xmpp_handler fn)
{
	struct rayo_xmpp_handler *handler = switch_core_alloc(globals.pool, sizeof (*handler));
	handler->to_type = zstr(type) ? "" : switch_core_strdup(globals.pool, type);
	handler->to_subtype = zstr(subtype) ? "" : switch_core_strdup(globals.pool, subtype);
	handler->fn = fn;
	rayo_command_handler_add(name, handler);
}

/**
 * Get command handler function from hash
 * @param hash the hash to search
 * @param msg the command
 * @return the command handler function or NULL
 */
rayo_actor_xmpp_handler rayo_actor_command_handler_find(struct rayo_actor *actor, struct rayo_message *msg)
{
	iks *iq = msg->payload;
	const char *iq_type = iks_find_attrib_soft(iq, "type");
	iks *command = iks_first_tag(iq);
	const char *name = "";
	const char *namespace = "";
	struct rayo_xmpp_handler *handler = NULL;
	char full_name[1024];

	full_name[1023] = '\0';
	if (command) {
		name = iks_name(command);
		namespace = iks_find_attrib_soft(command, "xmlns");
		if (zstr(name)) {
			name = "";
		}
	}

	snprintf(full_name, sizeof(full_name) - 1, "%s:%s:%s:%s:%s", actor->type, actor->subtype, iq_type, namespace, name);
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "%s, looking for %s command\n", RAYO_JID(actor), full_name);
	handler = (struct rayo_xmpp_handler *)switch_core_hash_find(globals.command_handlers, full_name);
	if (handler) {
		return handler->fn;
	}

	return NULL;
}

/**
 * Add event handler function
 * @param name the event name
 * @param handler the event handler function
 */
static void rayo_event_handler_add(const char *name, struct rayo_xmpp_handler *handler)
{
	char full_name[1024];
	full_name[1023] = '\0';
	snprintf(full_name, sizeof(full_name) - 1, "%s:%s:%s:%s:%s", handler->from_type, handler->from_subtype, handler->to_type, handler->to_subtype, name);
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Adding event: %s\n", full_name);
	switch_core_hash_insert(globals.event_handlers, full_name, handler);
}

/**
 * Add event handler function
 * @param from_type the source actor type
 * @param from_subtype the source actor subtype
 * @param to_type the destination actor type
 * @param to_subtype the destination actor subtype
 * @param name the event name
 * @param fn the event callback function
 */
void rayo_actor_event_handler_add(const char *from_type, const char *from_subtype, const char *to_type, const char *to_subtype, const char *name, rayo_actor_xmpp_handler fn)
{
	struct rayo_xmpp_handler *handler = switch_core_alloc(globals.pool, sizeof (*handler));
	handler->from_type = zstr(from_type) ? "" : switch_core_strdup(globals.pool, from_type);
	handler->from_subtype = zstr(from_subtype) ? "" : switch_core_strdup(globals.pool, from_subtype);
	handler->to_type = zstr(to_type) ? "" : switch_core_strdup(globals.pool, to_type);
	handler->to_subtype = zstr(to_subtype) ? "" : switch_core_strdup(globals.pool, to_subtype);
	handler->fn = fn;
	rayo_event_handler_add(name, handler);
}

/**
 * Get event handler function from hash
 * @param actor the event destination
 * @param msg the event
 * @return the event handler function or NULL
 */
rayo_actor_xmpp_handler rayo_actor_event_handler_find(struct rayo_actor *actor, struct rayo_message *msg)
{
	iks *presence = msg->payload;
	iks *event = iks_first_tag(presence);
	if (event) {
		struct rayo_xmpp_handler *handler = NULL;
		const char *presence_type = iks_find_attrib_soft(presence, "type");
		const char *event_name = iks_name(event);
		const char *event_namespace = iks_find_attrib_soft(event, "xmlns");
		char full_name[1024];
		full_name[1023] = '\0';
		if (zstr(event_name)) {
			return NULL;
		}
		snprintf(full_name, sizeof(full_name) - 1, "%s:%s:%s:%s:%s:%s:%s", msg->from_type, msg->from_subtype, actor->type, actor->subtype, presence_type, event_namespace, event_name);
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "%s => %s, looking for %s event handler\n", msg->from_jid, RAYO_JID(actor), full_name);
		handler = (struct rayo_xmpp_handler *)switch_core_hash_find(globals.event_handlers, full_name);
		if (handler) {
			return handler->fn;
		}
	} else {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "%s => %s, event missing child element\n", msg->from_jid, RAYO_JID(actor));
	}
	return NULL;
}

/**
 * Clean up a message
 * @param msg to destroy
 */
void rayo_message_destroy(struct rayo_message *msg)
{
	if (msg) {
		if (msg->payload) {
			iks_delete(msg->payload);
		}
		switch_safe_free(msg->to_jid);
		switch_safe_free(msg->from_jid);
		switch_safe_free(msg->from_type);
		switch_safe_free(msg->from_subtype);
		switch_safe_free(msg->file);
		free(msg);
	}
}

/**
 * Remove payload from message
 */
iks *rayo_message_remove_payload(struct rayo_message *msg)
{
	iks *payload = msg->payload;
	msg->payload = NULL;
	msg->from = NULL;
	msg->to = NULL;
	return payload;
}

/**
 * Thread that delivers internal XMPP messages
 * @param thread this thread
 * @param obj unused
 * @return NULL
 */
static void *SWITCH_THREAD_FUNC deliver_message_thread(switch_thread_t *thread, void *obj)
{
	struct rayo_message *msg = NULL;
	switch_thread_rwlock_rdlock(globals.shutdown_rwlock);
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "New message delivery thread\n");
	while (!globals.shutdown) {
		if (switch_queue_pop(globals.msg_queue, (void *)&msg) == SWITCH_STATUS_SUCCESS) {
			struct rayo_actor *actor = RAYO_LOCATE(msg->to_jid);
			if (actor) {
				/* deliver to actor */
				switch_mutex_lock(actor->mutex);
				switch_log_printf(SWITCH_CHANNEL_ID_LOG, msg->file, "", msg->line, "", SWITCH_LOG_DEBUG, "Deliver %s => %s %s\n", msg->from_jid, msg->to_jid, iks_string(iks_stack(msg->payload), msg->payload));
				actor->send_fn(actor, msg);
				switch_mutex_unlock(actor->mutex);
				RAYO_RELEASE(actor);
			} else if (!msg->is_reply) {
				/* unknown actor */
				RAYO_SEND_REPLY(globals.server, msg->from_jid, iks_new_error(msg->payload, STANZA_ERROR_ITEM_NOT_FOUND));
			}
			rayo_message_destroy(msg);
		}
	}

	/* clean up remaining messages */
	while(switch_queue_trypop(globals.msg_queue, (void *)&msg) == SWITCH_STATUS_SUCCESS) {
		rayo_message_destroy(msg);
	}

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Message delivery thread finished\n");
	switch_thread_rwlock_unlock(globals.shutdown_rwlock);
	return NULL;
}

/**
 * Create a new message thread
 * @param pool to use
 */
static void start_deliver_message_thread(switch_memory_pool_t *pool)
{
	switch_thread_t *thread;
	switch_threadattr_t *thd_attr = NULL;
	switch_threadattr_create(&thd_attr, pool);
	switch_threadattr_detach_set(thd_attr, 1);
	switch_threadattr_stacksize_set(thd_attr, SWITCH_THREAD_STACKSIZE);
	switch_thread_create(&thread, thd_attr, deliver_message_thread, NULL, pool);
}

/**
 * Stop all message threads
 */
static void stop_deliver_message_threads(void)
{
	globals.shutdown = 1;
	switch_queue_interrupt_all(globals.msg_queue);
	switch_thread_rwlock_wrlock(globals.shutdown_rwlock);
}

/**
 * Send message to actor addressed by JID
 * @param from actor sending the message
 * @param to destination JID
 * @param payload the message payload to deliver
 * @param dup true if payload is to be copied
 * @param reply true if a reply
 * @param file file name
 * @param line line number
 */
void rayo_message_send(struct rayo_actor *from, const char *to, iks *payload, int dup, int reply, const char *file, int line)
{
	const char *msg_name;
	struct rayo_message *msg = malloc(sizeof(*msg));
	if (dup) {
		msg->payload = iks_copy(payload);
	} else {
		msg->payload = payload;
	}
	msg->is_reply = reply;
	msg->to_jid = strdup(zstr(to) ? "" : to);
	if (!zstr(msg->to_jid)) {
		msg->to = iks_id_new(iks_stack(msg->payload), msg->to_jid);
	}
	msg->from_jid = strdup(RAYO_JID(from));
	if (!zstr(msg->from_jid)) {
		msg->from = iks_id_new(iks_stack(msg->payload), msg->from_jid);
	}
	msg->from_type = strdup(zstr(from->type) ? "" : from->type);
	msg->from_subtype = strdup(zstr(from->subtype) ? "" : from->subtype);
	msg->file = strdup(file);
	msg->line = line;

	/* add timestamp to presence events */
	msg_name = iks_name(msg->payload);
	if (!zstr(msg_name) && !strcmp("presence", msg_name)) {
		/* don't add timestamp if there already is one */
		iks *delay = iks_find(msg->payload, "delay");
		if (!delay || strcmp("urn:xmpp:delay", iks_find_attrib_soft(delay, "xmlns"))) {
			switch_time_exp_t tm;
			char timestamp[80];
			switch_size_t retsize;
			delay = iks_insert(msg->payload, "delay");
			iks_insert_attrib(delay, "xmlns", "urn:xmpp:delay");
			switch_time_exp_tz(&tm, switch_time_now(), 0);
			switch_strftime_nocheck(timestamp, &retsize, sizeof(timestamp), "%Y-%m-%dT%TZ", &tm);
			iks_insert_attrib_printf(delay, "stamp", "%s", timestamp);
		}
	}

	if (switch_queue_trypush(globals.msg_queue, msg) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "failed to queue message!\n");
		rayo_message_destroy(msg);
	}
}

/**
 * Get access to Rayo actor with JID.
 * @param jid the JID
 * @return the actor or NULL.  Call RAYO_RELEASE() when done with pointer.
 */
struct rayo_actor *rayo_actor_locate(const char *jid, const char *file, int line)
{
	struct rayo_actor *actor = NULL;
	switch_mutex_lock(globals.actors_mutex);
	if (!strncmp("xmpp:", jid, 5)) {
		jid = jid + 5;
	}
	actor = (struct rayo_actor *)switch_core_hash_find(globals.actors, jid);
	if (actor) {
		if (!actor->destroy) {
			actor->ref_count++;
			switch_log_printf(SWITCH_CHANNEL_ID_LOG, file, "", line, "", SWITCH_LOG_DEBUG, "Locate (jid) %s: ref count = %i\n", RAYO_JID(actor), actor->ref_count);
		} else {
			switch_log_printf(SWITCH_CHANNEL_ID_LOG, file, "", line, "", SWITCH_LOG_WARNING, "Locate (jid) %s: already marked for destruction!\n", jid);
			actor = NULL;
		}
	}
	switch_mutex_unlock(globals.actors_mutex);
	return actor;
}

/**
 * Get exclusive access to Rayo actor with internal ID
 * @param id the internal ID
 * @return the actor or NULL.  Call RAYO_RELEASE() when done with pointer.
 */
struct rayo_actor *rayo_actor_locate_by_id(const char *id, const char *file, int line)
{
	struct rayo_actor *actor = NULL;
	if (!zstr(id)) {
		switch_mutex_lock(globals.actors_mutex);
		actor = (struct rayo_actor *)switch_core_hash_find(globals.actors_by_id, id);
		if (actor) {
			if (!actor->destroy) {
				actor->ref_count++;
				switch_log_printf(SWITCH_CHANNEL_ID_LOG, file, "", line, "", SWITCH_LOG_DEBUG, "Locate (id) %s: ref count = %i\n", RAYO_JID(actor), actor->ref_count);
			} else {
				switch_log_printf(SWITCH_CHANNEL_ID_LOG, file, "", line, "", SWITCH_LOG_WARNING, "Locate (id) %s: already marked for destruction!\n", id);
				actor = NULL;
			}
		}
		switch_mutex_unlock(globals.actors_mutex);
	}
	return actor;
}

/**
 * Destroy a rayo actor
 */
void rayo_actor_destroy(struct rayo_actor *actor, const char *file, int line)
{
	switch_memory_pool_t *pool = actor->pool;
	switch_mutex_lock(globals.actors_mutex);
	if (!actor->destroy) {
		switch_log_printf(SWITCH_CHANNEL_ID_LOG, file, "", line, "", SWITCH_LOG_DEBUG, "Destroy %s requested: ref_count = %i\n", RAYO_JID(actor), actor->ref_count);
		switch_core_hash_delete(globals.actors, RAYO_JID(actor));
		if (!zstr(actor->id)) {
			switch_core_hash_delete(globals.actors_by_id, actor->id);
		}
	}
	actor->destroy = 1;
	if (actor->ref_count <= 0) {
		if (actor->ref_count < 0) {
			/* too many unlocks detected! */
			switch_log_printf(SWITCH_CHANNEL_ID_LOG, file, "", line, "", SWITCH_LOG_WARNING, "Destroying %s, ref_count = %i\n", RAYO_JID(actor), actor->ref_count);
		} else {
			switch_log_printf(SWITCH_CHANNEL_ID_LOG, file, "", line, "", SWITCH_LOG_DEBUG, "Destroying %s\n", RAYO_JID(actor));
		}
		switch_core_hash_delete(globals.destroy_actors, RAYO_JID(actor));
		switch_mutex_unlock(globals.actors_mutex);
		/* safe to destroy parent now */
		if (actor->cleanup_fn) {
			actor->cleanup_fn(actor);
		}
		if (actor->parent) {
			RAYO_RELEASE(actor->parent);
		}
		switch_core_destroy_memory_pool(&pool);
	} else {
		switch_core_hash_insert(globals.destroy_actors, RAYO_JID(actor), actor);
		switch_mutex_unlock(globals.actors_mutex);
	}
}

/**
 * Increment actor ref count - locks from destruction.
 */
void rayo_actor_retain(struct rayo_actor *actor, const char *file, int line)
{
	if (actor) {
		switch_mutex_lock(globals.actors_mutex);
		actor->ref_count++;
		switch_log_printf(SWITCH_CHANNEL_ID_LOG, file, "", line, "", SWITCH_LOG_DEBUG, "Lock %s: ref count = %i\n", RAYO_JID(actor), actor->ref_count);
		switch_mutex_unlock(globals.actors_mutex);
	}
}

/**
 * Release rayo actor reference
 */
void rayo_actor_release(struct rayo_actor *actor, const char *file, int line)
{
	if (actor) {
		switch_mutex_lock(globals.actors_mutex);
		actor->ref_count--;
		if (actor->ref_count < 0) {
			/* too many unlocks detected! */
			switch_log_printf(SWITCH_CHANNEL_ID_LOG, file, "", line, "", SWITCH_LOG_WARNING, "Release %s: ref count = %i\n", RAYO_JID(actor), actor->ref_count);
		} else {
			switch_log_printf(SWITCH_CHANNEL_ID_LOG, file, "", line, "", SWITCH_LOG_DEBUG, "Release %s: ref count = %i\n", RAYO_JID(actor), actor->ref_count);
		}
		if (actor->ref_count <= 0 && actor->destroy) {
			rayo_actor_destroy(actor, file, line);
		}
		switch_mutex_unlock(globals.actors_mutex);
	}
}

/**
 * Get next number in sequence
 */
int rayo_actor_seq_next(struct rayo_actor *actor)
{
	int seq;
	switch_mutex_lock(actor->mutex);
	seq = actor->seq++;
	switch_mutex_unlock(actor->mutex);
	return seq;
}

#define RAYO_CALL_LOCATE(call_uri) rayo_call_locate(call_uri, __FILE__, __LINE__)
/**
 * Get access to Rayo call data.  Use to access call data outside channel thread.
 * @param call_uri the Rayo XMPP URI
 * @return the call or NULL.
 */
static struct rayo_call *rayo_call_locate(const char *call_uri, const char *file, int line)
{
	struct rayo_actor *actor = rayo_actor_locate(call_uri, file, line);
	if (actor && is_call_actor(actor)) {
		return RAYO_CALL(actor);
	} else if (actor) {
		RAYO_RELEASE(actor);
	}
	return NULL;
}

#define RAYO_CALL_LOCATE_BY_ID(call_uuid) rayo_call_locate_by_id(call_uuid, __FILE__, __LINE__)
/**
 * Get access to Rayo call data.  Use to access call data outside channel thread.
 * @param call_uuid the FreeSWITCH call UUID
 * @return the call or NULL.
 */
static struct rayo_call *rayo_call_locate_by_id(const char *call_uuid, const char *file, int line)
{
	struct rayo_actor *actor = rayo_actor_locate_by_id(call_uuid, file, line);
	if (actor && is_call_actor(actor)) {
		return RAYO_CALL(actor);
	} else if (actor) {
		RAYO_RELEASE(actor);
	}
	return NULL;
}

/**
 * Send <end> event to DCP and PCPs
 */
static void rayo_call_send_end(struct rayo_call *call, switch_event_t *event, int local_hangup, const char *cause_str, const char *cause_q850_str)
{
	int no_offered_clients = 1;
	switch_hash_index_t *hi = NULL;
	iks *revent;
	iks *end;
	const char *dcp_jid = rayo_call_get_dcp_jid(call);

	/* build call end event */
	revent = iks_new_presence("end", RAYO_NS, RAYO_JID(call), "foo");
	iks_insert_attrib(revent, "type", "unavailable");
	end = iks_find(revent, "end");

	if (local_hangup) {
		iks_insert(end, RAYO_END_REASON_HANGUP_LOCAL);
	} else {
		/* remote hangup... translate to specific rayo reason */
		iks *reason;
		switch_call_cause_t cause = SWITCH_CAUSE_NONE;
		if (!zstr(cause_str)) {
			cause = switch_channel_str2cause(cause_str);
		}
		reason = iks_insert(end, switch_cause_to_rayo_cause(cause));
		if (!zstr(cause_q850_str)) {
			iks_insert_attrib(reason, "platform-code", cause_q850_str);
		}
	}

	#if 0
	if (event) {
		char *event_str;
		if (switch_event_serialize(event, &event_str, SWITCH_FALSE) == SWITCH_STATUS_SUCCESS) {
			switch_log_printf(SWITCH_CHANNEL_UUID_LOG(rayo_call_get_uuid(call)), SWITCH_LOG_DEBUG, "%s\n", event_str);
			switch_safe_free(event_str);
		}
	}
	#endif

	/* add signaling headers */
	if (event) {
		switch_event_header_t *header;
		/* get all variables prefixed with sip_h_ */
		for (header = event->headers; header; header = header->next) {
			if (!strncmp("variable_sip_h_", header->name, 15)) {
				add_header(end, header->name + 15, header->value);
			}
		}
	}

	/* send <end> to all offered clients */
	for (hi = switch_core_hash_first(call->pcps); hi; hi = switch_core_hash_next(&hi)) {
		const void *key;
		void *val;
		const char *client_jid = NULL;
		switch_core_hash_this(hi, &key, NULL, &val);
		client_jid = (const char *)key;
		switch_assert(client_jid);
		iks_insert_attrib(revent, "to", client_jid);
		switch_log_printf(SWITCH_CHANNEL_UUID_LOG(rayo_call_get_uuid(call)), SWITCH_LOG_DEBUG, "Sending <end> to offered client %s\n", client_jid);
		RAYO_SEND_MESSAGE_DUP(call, client_jid, revent);
		no_offered_clients = 0;
	}

	if (no_offered_clients && !zstr(dcp_jid)) {
		/* send to DCP only */
		switch_log_printf(SWITCH_CHANNEL_UUID_LOG(rayo_call_get_uuid(call)), SWITCH_LOG_DEBUG, "Sending <end> to DCP %s\n", dcp_jid);
		iks_insert_attrib(revent, "to", dcp_jid);
		RAYO_SEND_MESSAGE_DUP(call, dcp_jid, revent);
	}

	iks_delete(revent);
}

/**
 * Fire <end> event when call is cleaned up completely
 */
static void rayo_call_cleanup(struct rayo_actor *actor)
{
	struct rayo_call *call = RAYO_CALL(actor);
	switch_event_t *event = call->end_event;
	const char *dcp_jid = rayo_call_get_dcp_jid(call);

	if (!event || call->dial_request_failed) {
		/* destroyed before FS session was created (in originate, for example) */
		goto done;
	}

	/* send call unjoined event, if not already sent */
	if (call->joined && call->joined_id) {
		if (!zstr(dcp_jid)) {
			iks *unjoined;
			iks *uevent = iks_new_presence("unjoined", RAYO_NS, RAYO_JID(call), dcp_jid);
			unjoined = iks_find(uevent, "unjoined");
			iks_insert_attrib_printf(unjoined, "call-uri", "%s", call->joined_id);
			RAYO_SEND_MESSAGE(call, dcp_jid, uevent);
		}
	}

	rayo_call_send_end(call,
		event,
		switch_true(switch_event_get_header(event, "variable_rayo_local_hangup")),
		switch_event_get_header(event, "variable_hangup_cause"),
		switch_event_get_header(event, "variable_hangup_cause_q850"));

done:

	/* lost the race: pending join failed... send IQ result to client now. */
	if (call->pending_join_request) {
		iks *request = call->pending_join_request;
		iks *result = iks_new_error_detailed(request, STANZA_ERROR_ITEM_NOT_FOUND, "call ended");
		call->pending_join_request = NULL;
		RAYO_SEND_REPLY(call, iks_find_attrib_soft(request, "from"), result);
		iks_delete(call->pending_join_request);
	}

	if (event) {
		switch_event_destroy(&event);
	}
	if (call->answer_event) {
		switch_event_destroy(&call->answer_event);
	}
	switch_core_hash_destroy(&call->pcps);
}

/**
 * @param call the Rayo call
 * @return the Rayo call DCP JID
 */
const char *rayo_call_get_dcp_jid(struct rayo_call *call)
{
	return call->dcp_jid;
}

/**
 * @param call the Rayo call
 * @return true if joined (or a join is in progress)
 */
int rayo_call_is_joined(struct rayo_call *call)
{
	return call->joined || call->pending_join_request;
}

/**
 * @param call to check if faxing
 * @return true if faxing is in progress
 */
int rayo_call_is_faxing(struct rayo_call *call)
{
	return call->faxing;
}

/**
 * Set faxing flag
 * @param call the call to flag
 * @param faxing true if faxing is in progress
 */
void rayo_call_set_faxing(struct rayo_call *call, int faxing)
{
	call->faxing = faxing;
}

#define RAYO_MIXER_LOCATE(mixer_name) rayo_mixer_locate(mixer_name, __FILE__, __LINE__)
/**
 * Get access to Rayo mixer data.
 * @param mixer_name the mixer name
 * @return the mixer or NULL. Call RAYO_RELEASE() when done with mixer pointer.
 */
static struct rayo_mixer *rayo_mixer_locate(const char *mixer_name, const char *file, int line)
{
	struct rayo_actor *actor = rayo_actor_locate_by_id(mixer_name, file, line);
	if (actor && !strcmp(RAT_MIXER, actor->type)) {
		return RAYO_MIXER(actor);
	} else if (actor) {
		RAYO_RELEASE(actor);
	}
	return NULL;
}

/**
 * Default message handler - drops messages
 */
void rayo_actor_send_ignore(struct rayo_actor *to, struct rayo_message *msg)
{
	switch_log_printf(SWITCH_CHANNEL_ID_LOG, msg->file, "", msg->line, "", SWITCH_LOG_WARNING, "%s, dropping unexpected message to %s.\n", msg->from_jid, RAYO_JID(to));
}

#define RAYO_ACTOR_INIT(actor, pool, type, subtype, id, jid, cleanup, send) rayo_actor_init(actor, pool, type, subtype, id, jid, cleanup, send, NULL, __FILE__, __LINE__)
#define RAYO_ACTOR_INIT_PARENT(actor, pool, type, subtype, id, jid, cleanup, send, parent) rayo_actor_init(actor, pool, type, subtype, id, jid, cleanup, send, parent, __FILE__, __LINE__)

/**
 * Initialize a rayo actor
 * @param actor to initialize
 * @param pool to use
 * @param type of actor (MIXER, CALL, SERVER, COMPONENT)
 * @param subtype of actor (input/output/prompt)
 * @param id internal ID
 * @param jid external ID
 * @param cleanup function
 * @param send sent message handler
 * @param parent of actor
 * @param file that called this function
 * @param line that called this function
 * @return the actor or NULL if JID conflict
 */
static struct rayo_actor *rayo_actor_init(struct rayo_actor *actor, switch_memory_pool_t *pool, const char *type, const char *subtype, const char *id, const char *jid, rayo_actor_cleanup_fn cleanup, rayo_actor_send_fn send, struct rayo_actor *parent, const char *file, int line)
{
	char *domain;
	actor->type = switch_core_strdup(pool, type);
	actor->subtype = switch_core_strdup(pool, subtype);
	actor->pool = pool;
	if (!zstr(id)) {
		actor->id = switch_core_strdup(pool, id);
	}
	/* TODO validate JID with regex */
	if (!zstr(jid)) {
		RAYO_JID(actor) = switch_core_strdup(pool, jid);
		if (!(domain = strrchr(RAYO_JID(actor), '@'))) {
			RAYO_DOMAIN(actor) = RAYO_JID(actor);
		} else if (!zstr(++domain)) {
			RAYO_DOMAIN(actor) = switch_core_strdup(pool, domain);
			/* strip resource from domain if it exists */
			domain = strrchr(RAYO_DOMAIN(actor), '/');
			if (domain) {
				*domain = '\0';
			}
		}
	}
	actor->seq = 1;
	actor->ref_count = 1;
	actor->destroy = 0;
	actor->cleanup_fn = cleanup;
	if (send == NULL) {
		actor->send_fn = rayo_actor_send_ignore;
	} else {
		actor->send_fn = send;
	}

	actor->parent = parent;
	if (!actor->parent) {
		switch_mutex_init(&actor->mutex, SWITCH_MUTEX_NESTED, pool);
	} else {
		/* inherit mutex from parent */
		actor->mutex = actor->parent->mutex;

		/* prevent parent destruction */
		RAYO_RETAIN(actor->parent);
	}

	/* add to hash of actors, so commands can route to call */
	switch_mutex_lock(globals.actors_mutex);
	if (!zstr(jid)) {
		if (switch_core_hash_find(globals.actors, RAYO_JID(actor))) {
			/* duplicate JID, give up! */
			switch_log_printf(SWITCH_CHANNEL_ID_LOG, file, "", line, "", SWITCH_LOG_NOTICE, "JID conflict! %s\n", RAYO_JID(actor));
			switch_mutex_unlock(globals.actors_mutex);
			if (actor->parent) {
				/* unlink from parent */
				RAYO_RELEASE(actor->parent);
				actor->parent = NULL;
			}
			return NULL;
		}
		switch_core_hash_insert(globals.actors, RAYO_JID(actor), actor);
	}
	if (!zstr(id)) {
		if (switch_core_hash_find(globals.actors_by_id, actor->id)) {
			/* duplicate ID - only log for now... */
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "ID conflict! %s\n", actor->id);
		}
		switch_core_hash_insert(globals.actors_by_id, actor->id, actor);
	}
	switch_mutex_unlock(globals.actors_mutex);

	switch_log_printf(SWITCH_CHANNEL_ID_LOG, file, "", line, "", SWITCH_LOG_DEBUG, "Init %s\n", RAYO_JID(actor));

	return actor;
}

/**
 * Initialize rayo call
 * @return the call or NULL if JID conflict
 */
static struct rayo_call *rayo_call_init(struct rayo_call *call, switch_memory_pool_t *pool, const char *uuid, const char *file, int line)
{
	char *call_jid;
	char uuid_id_buf[SWITCH_UUID_FORMATTED_LENGTH + 1];

	if (zstr(uuid)) {
		switch_uuid_str(uuid_id_buf, sizeof(uuid_id_buf));
		uuid = uuid_id_buf;
	}
	call_jid = switch_mprintf("%s@%s", uuid, RAYO_JID(globals.server));

	call = RAYO_CALL(rayo_actor_init(RAYO_ACTOR(call), pool, RAT_CALL, "", uuid, call_jid, rayo_call_cleanup, rayo_call_send, NULL, file, line));
	if (call) {
		call->dcp_jid = "";
		call->idle_start_time = switch_micro_time_now();
		call->joined = 0;
		call->joined_id = NULL;
		call->ringing_sent = 0;
		call->pending_join_request = NULL;
		call->dial_request_id = NULL;
		call->end_event = NULL;
		call->dial_request_failed = 0;
		call->rayo_app_started = 0;
		call->answer_event = NULL;
		switch_core_hash_init(&call->pcps);
	}

	switch_safe_free(call_jid);

	return call;
}

#define rayo_call_create(uuid) _rayo_call_create(uuid, __FILE__, __LINE__)
/**
 * Create Rayo call
 * @param uuid uuid to assign call, if NULL one is picked
 * @param file file that called this function
 * @param line number of file that called this function
 * @return the call, or NULL if JID conflict
 */
static struct rayo_call *_rayo_call_create(const char *uuid, const char *file, int line)
{
	switch_memory_pool_t *pool;
	struct rayo_call *call;
	switch_core_new_memory_pool(&pool);
	call = switch_core_alloc(pool, sizeof(*call));
	call = rayo_call_init(call, pool, uuid, file, line);
	if (!call) {
		switch_core_destroy_memory_pool(&pool);
	}
	return call;
}

/**
 * Mixer destructor
 */
static void rayo_mixer_cleanup(struct rayo_actor *actor)
{
	struct rayo_mixer *mixer = RAYO_MIXER(actor);
	switch_core_hash_destroy(&mixer->members);
	switch_core_hash_destroy(&mixer->subscribers);
}

/**
 * Initialize mixer
 * @return the mixer or NULL if JID conflict
 */
static struct rayo_mixer *rayo_mixer_init(struct rayo_mixer *mixer, switch_memory_pool_t *pool, const char *name, const char *file, int line)
{
	char *mixer_jid = switch_mprintf("%s@%s", name, RAYO_JID(globals.server));
	mixer = RAYO_MIXER(rayo_actor_init(RAYO_ACTOR(mixer), pool, RAT_MIXER, "", name, mixer_jid, rayo_mixer_cleanup, rayo_mixer_send, NULL, file, line));
	if (mixer) {
		switch_core_hash_init(&mixer->members);
		switch_core_hash_init(&mixer->subscribers);
	}
	switch_safe_free(mixer_jid);
	return mixer;
}

#define rayo_mixer_create(name) _rayo_mixer_create(name, __FILE__, __LINE__)
/**
 * Create Rayo mixer
 * @param name of this mixer
 * @return the mixer or NULL if JID conflict
 */
static struct rayo_mixer *_rayo_mixer_create(const char *name, const char *file, int line)
{
	switch_memory_pool_t *pool;
	struct rayo_mixer *mixer = NULL;
	switch_core_new_memory_pool(&pool);
	mixer = rayo_mixer_init(switch_core_alloc(pool, sizeof(*mixer)), pool, name, file, line);
	if (!mixer) {
		switch_core_destroy_memory_pool(&pool);
	}
	return mixer;
}

/**
 * Initialize Rayo component
 * @param type of this component
 * @param subtype of this component
 * @param id internal ID of this component
 * @param parent the parent that owns this component
 * @param client_jid the client that created this component
 * @param cleanup optional cleanup function
 * @param file file that called this function
 * @param line line number that called this function
 * @return the component or NULL if JID conflict
 */
struct rayo_component *_rayo_component_init(struct rayo_component *component, switch_memory_pool_t *pool, const char *type, const char *subtype, const char *id, struct rayo_actor *parent, const char *client_jid, rayo_actor_cleanup_fn cleanup, const char *file, int line)
{
	char *ref = switch_mprintf("%s-%d", subtype, rayo_actor_seq_next(parent));
	char *jid = switch_mprintf("%s/%s", RAYO_JID(parent), ref);
	if (zstr(id)) {
		id = jid;
	}

	component = RAYO_COMPONENT(rayo_actor_init(RAYO_ACTOR(component), pool, type, subtype, id, jid, cleanup, rayo_component_send, parent, file, line));
	if (component) {
		component->client_jid = switch_core_strdup(pool, client_jid);
		component->ref = switch_core_strdup(pool, ref);
	}

	switch_safe_free(ref);
	switch_safe_free(jid);
	return component;
}

/**
 * Send XMPP message to client
 */
void rayo_client_send(struct rayo_actor *client, struct rayo_message *msg)
{
	xmpp_stream_context_send(globals.xmpp_context, RAYO_CLIENT(client)->route, msg->payload);
}

/**
 * Cleanup rayo client
 */
static void rayo_client_cleanup(struct rayo_actor *actor)
{
	/* remove session from map */
	switch_mutex_lock(globals.clients_mutex);
	if (!zstr(RAYO_JID(actor))) {
		switch_core_hash_delete(globals.clients_roster, RAYO_JID(actor));
		if (RAYO_CLIENT(actor)->peer_server) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "Removing %s from peer server %s\n", RAYO_JID(actor), RAYO_JID(RAYO_CLIENT(actor)->peer_server));
			switch_core_hash_delete(RAYO_CLIENT(actor)->peer_server->clients, RAYO_JID(actor));
		}
	}
	switch_mutex_unlock(globals.clients_mutex);

	pause_when_offline();
}

/**
 * Initialize rayo client
 * @param pool the memory pool for this client
 * @param jid for this client
 * @param route to this client
 * @param availability of client
 * @param send message transmission function
 * @param peer_server NULL if locally connected client
 * @return the new client or NULL if JID conflict
 */
static struct rayo_client *rayo_client_init(struct rayo_client *client, switch_memory_pool_t *pool, const char *jid, const char *route, enum presence_status availability, rayo_actor_send_fn send, struct rayo_peer_server *peer_server)
{
	client = RAYO_CLIENT(RAYO_ACTOR_INIT(RAYO_ACTOR(client), pool, RAT_CLIENT, "", jid, jid, rayo_client_cleanup, send));
	if (client) {
		client->availability = availability;
		client->peer_server = peer_server;
		client->last_probe = 0;
		if (route) {
			client->route = switch_core_strdup(pool, route);
		}

		/* make client available for offers */
		switch_mutex_lock(globals.clients_mutex);
		switch_core_hash_insert(globals.clients_roster, RAYO_JID(client), client);
		if (peer_server) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "Adding %s to peer server %s\n", RAYO_JID(client), RAYO_JID(peer_server));
			switch_core_hash_insert(peer_server->clients, RAYO_JID(client), client);
		}
		switch_mutex_unlock(globals.clients_mutex);
	}

	pause_when_offline();

	return client;
}

/**
 * Create a new Rayo client
 * @param jid for this client
 * @param route to this client
 * @param availability of client
 * @param send message transmission function
 * @param peer_server NULL if locally connected client
 * @return the new client or NULL
 */
static struct rayo_client *rayo_client_create(const char *jid, const char *route, enum presence_status availability, rayo_actor_send_fn send, struct rayo_peer_server *peer_server)
{
	switch_memory_pool_t *pool;
	struct rayo_client *rclient = NULL;

	switch_core_new_memory_pool(&pool);
	if (!(rclient = switch_core_alloc(pool, sizeof(*rclient)))) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Memory Error\n");
		return NULL;
	}
	rclient = rayo_client_init(rclient, pool, jid, route, availability, send, peer_server);
	if (!rclient) {
		switch_core_destroy_memory_pool(&pool);
	}
	return rclient;
}

/**
 * Send XMPP message to peer server
 */
void rayo_peer_server_send(struct rayo_actor *server, struct rayo_message *msg)
{
	xmpp_stream_context_send(globals.xmpp_context, RAYO_JID(server), msg->payload);
}

/**
 * Destroy peer server and its associated clients
 */
static void rayo_peer_server_cleanup(struct rayo_actor *actor)
{
	switch_hash_index_t *hi = NULL;
	struct rayo_peer_server *rserver = RAYO_PEER_SERVER(actor);

	/* a little messy... client will remove itself from the peer server when it is destroyed,
	 * however, there is no guarantee the client will actually be destroyed now so
	 * the server must remove the client.
	 */
	switch_mutex_lock(globals.clients_mutex);
	while ((hi = switch_core_hash_first_iter(rserver->clients, hi))) {
		const void *key;
		void *client;
		switch_core_hash_this(hi, &key, NULL, &client);
		switch_assert(client);
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "Removing %s from peer server %s\n", RAYO_JID(client), RAYO_JID(rserver));
		switch_core_hash_delete(rserver->clients, key);
		RAYO_CLIENT(client)->peer_server = NULL;
		RAYO_RELEASE(client);
		RAYO_DESTROY(client);
	}
	switch_core_hash_destroy(&rserver->clients);
	switch_mutex_unlock(globals.clients_mutex);
}

/**
 * Create a new Rayo peer server
 * @param jid of this server
 * @return the peer server
 */
static struct rayo_peer_server *rayo_peer_server_create(const char *jid)
{
	switch_memory_pool_t *pool;
	struct rayo_peer_server *rserver = NULL;

	switch_core_new_memory_pool(&pool);
	if (!(rserver = switch_core_alloc(pool, sizeof(*rserver)))) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Memory Error\n");
		return NULL;
	}
	rserver = RAYO_PEER_SERVER(RAYO_ACTOR_INIT(RAYO_ACTOR(rserver), pool, RAT_PEER_SERVER, "", jid, jid, rayo_peer_server_cleanup, rayo_peer_server_send));
	if (rserver) {
		switch_core_hash_init(&rserver->clients);
	} else {
		switch_core_destroy_memory_pool(&pool);
	}
	return rserver;
}

/**
 * Check if message sender has control of offered call.
 * @param call the Rayo call
 * @param msg the message
 * @return 1 if sender has call control, 0 if sender does not have control
 */
static int has_call_control(struct rayo_call *call, struct rayo_message *msg)
{
	return (!strcmp(rayo_call_get_dcp_jid(call), msg->from_jid) || is_internal_message(msg) || is_admin_client_message(msg));
}

/**
 * Check if message sender has control of offered call. Take control if nobody else does.
 * @param call the Rayo call
 * @param session the session
 * @param msg the message
 * @return 1 if sender has call control
 */
static int take_call_control(struct rayo_call *call, switch_core_session_t *session, struct rayo_message *msg)
{
	int control = 0;

	/* nobody in charge */
	if (zstr(call->dcp_jid)) {
		/* was offered to this session? */
		if (!zstr(msg->from_jid) && switch_core_hash_find(call->pcps, msg->from_jid)) {
			/* take charge */
			call->dcp_jid = switch_core_strdup(RAYO_POOL(call), msg->from_jid);
			switch_channel_set_variable(switch_core_session_get_channel(session), "rayo_dcp_jid", rayo_call_get_dcp_jid(call));
			control = 1;
			switch_log_printf(SWITCH_CHANNEL_UUID_LOG(rayo_call_get_uuid(call)), SWITCH_LOG_INFO, "%s has control of call\n", rayo_call_get_dcp_jid(call));
		}
	} else if (has_call_control(call, msg)) {
		control = 1;
	}

	if (!control) {
		switch_log_printf(SWITCH_CHANNEL_UUID_LOG(rayo_call_get_uuid(call)), SWITCH_LOG_INFO, "%s does not have control of call\n", msg->from_jid);
	}

	return control;
}

/**
 * Check Rayo server command for errors.
 * @param server the server
 * @param msg the command
 * @return 1 if OK
 */
static iks *rayo_server_command_ok(struct rayo_actor *server, struct rayo_message *msg)
{
	iks *node = msg->payload;
	iks *response = NULL;
	int bad = zstr(iks_find_attrib(node, "id"));

	if (bad) {
		response = iks_new_error(node, STANZA_ERROR_BAD_REQUEST);
	}

	return response;
}

/**
 * Check Rayo call command for errors.
 * @param call the Rayo call
 * @param session the session
 * @param msg the command
 * @return 1 if OK
 */
static iks *rayo_call_command_ok(struct rayo_call *call, switch_core_session_t *session, struct rayo_message *msg)
{
	iks *node = msg->payload;
	iks *response = NULL;
	int bad = zstr(iks_find_attrib(node, "id"));

	if (bad) {
		response = iks_new_error(node, STANZA_ERROR_BAD_REQUEST);
	} else if (!take_call_control(call, session, msg)) {
		response = iks_new_error(node, STANZA_ERROR_CONFLICT);
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "%s, %s conflict\n", msg->from_jid, RAYO_JID(call));
	}

	return response;
}

/**
 * Check Rayo component command for errors.
 * @param component the component
 * @param msg the command
 * @return 0 if error
 */
static iks *rayo_component_command_ok(struct rayo_component *component, struct rayo_message *msg)
{
	iks *node = msg->payload;
	iks *response = NULL;
	char *from = iks_find_attrib(node, "from");
	int bad = zstr(iks_find_attrib(node, "id"));

	if (bad) {
		response = iks_new_error(node, STANZA_ERROR_BAD_REQUEST);
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "%s, %s bad request\n", msg->from_jid, RAYO_JID(component));
	} else if (strcmp(component->client_jid, from) && !is_admin_client_message(msg) && !is_internal_message(msg)) {
		/* does not have control of this component */
		response = iks_new_error(node, STANZA_ERROR_CONFLICT);
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "%s, %s conflict\n", msg->from_jid, RAYO_JID(component));
	}

	return response;
}

/**
 * Handle server message
 */
void rayo_server_send(struct rayo_actor *server, struct rayo_message *msg)
{
	iks *response = NULL;
	rayo_actor_xmpp_handler handler = NULL;
	iks *iq = msg->payload;

	if (!strcmp("presence", iks_name(iq))) {
		/* this is a hack - message from internal console */
		struct rayo_actor *client = RAYO_LOCATE(msg->from_jid);
		if (client) {
			if (!strcmp(RAT_CLIENT, client->type)) {
				on_client_presence(RAYO_CLIENT(client), iq);
			}
			RAYO_RELEASE(client);
		}
		return;
	}

	/* is this a command a server supports? */
	handler = rayo_actor_command_handler_find(server, msg);
	if (!handler) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "%s, no handler function for command to %s\n", msg->from_jid, RAYO_JID(server));
		if (!msg->is_reply) {
			RAYO_SEND_REPLY(server, msg->from_jid, iks_new_error(iq, STANZA_ERROR_FEATURE_NOT_IMPLEMENTED));
		}
		return;
	}

	/* is the command valid? */
	if (!(response = rayo_server_command_ok(server, msg))) {
		response = handler(server, msg, NULL);
	}

	if (response) {
		if (!msg->is_reply) {
			RAYO_SEND_REPLY(server, msg->from_jid, response);
		} else {
			iks_delete(response);
		}
	}
}

/**
 * Handle call message
 */
void rayo_call_send(struct rayo_actor *call, struct rayo_message *msg)
{
	rayo_actor_xmpp_handler handler = NULL;
	iks *stanza = msg->payload;
	switch_core_session_t *session;
	iks *response = NULL;

	if (!strcmp("message", iks_name(stanza))) {
		const char *type = iks_find_attrib_soft(stanza, "type");

		if (!strcmp("normal", type)) {
			const char *body = iks_find_cdata(stanza, "body");
			if (!zstr(body)) {
				switch_event_t *event;
				if (switch_event_create(&event, SWITCH_EVENT_SEND_MESSAGE) == SWITCH_STATUS_SUCCESS) {
					switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "content-type", "text/plain");
					switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "uuid", rayo_call_get_uuid(RAYO_CALL(call)));
					switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "subject", iks_find_cdata(stanza, "subject"));
					switch_event_add_body(event, "%s", body);
					switch_event_fire(&event);
				}
			} else if (!msg->is_reply) {
				RAYO_SEND_REPLY(call, msg->from_jid, iks_new_error_detailed(stanza, STANZA_ERROR_BAD_REQUEST, "missing body"));
			}
		} else if (!msg->is_reply) {
			RAYO_SEND_REPLY(call, msg->from_jid, iks_new_error(stanza, STANZA_ERROR_FEATURE_NOT_IMPLEMENTED));
		}
		return;
	}

	/* is this a command a call supports? */
	handler = rayo_actor_command_handler_find(call, msg);
	if (!handler) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "%s, no handler function for command\n", RAYO_JID(call));
		if (!msg->is_reply) {
			RAYO_SEND_REPLY(call, msg->from_jid, iks_new_error(stanza, STANZA_ERROR_FEATURE_NOT_IMPLEMENTED));
		}
		return;
	}

	/* is the session still available? */
	session = switch_core_session_locate(rayo_call_get_uuid(RAYO_CALL(call)));
	if (!session) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "%s, session not found\n", RAYO_JID(call));
		if (!msg->is_reply) {
			RAYO_SEND_REPLY(call, msg->from_jid, iks_new_error(stanza, STANZA_ERROR_ITEM_NOT_FOUND));
		}
		return;
	}

	/* is the command valid? */
	if (!(response = rayo_call_command_ok(RAYO_CALL(call), session, msg))) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "%s, executing command\n", RAYO_JID(call));
		response = handler(call, msg, session);
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "%s, done executing command\n", RAYO_JID(call));
	}
	switch_core_session_rwunlock(session);

	if (response) {
		if (!msg->is_reply) {
			RAYO_SEND_REPLY(call, msg->from_jid, response);
		} else {
			iks_delete(response);
		}
	}
}

/**
 * Handle mixer message
 */
void rayo_mixer_send(struct rayo_actor *mixer, struct rayo_message *msg)
{
	rayo_actor_xmpp_handler handler = NULL;
	iks *iq = msg->payload;
	iks *response = NULL;

	/* is this a command a mixer supports? */
	handler = rayo_actor_command_handler_find(mixer, msg);
	if (!handler) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "%s, no handler function for command\n", RAYO_JID(mixer));
		if (!msg->is_reply) {
			RAYO_SEND_REPLY(mixer, msg->from_jid, iks_new_error(iq, STANZA_ERROR_FEATURE_NOT_IMPLEMENTED));
		}
		return;
	}

	/* execute the command */
	response = handler(mixer, msg, NULL);
	if (response) {
		if (!msg->is_reply) {
			RAYO_SEND_REPLY(mixer, msg->from_jid, response);
		} else {
			iks_delete(response);
		}
	}
}

/**
 * Handle mixer message
 */
void rayo_component_send(struct rayo_actor *component, struct rayo_message *msg)
{
	rayo_actor_xmpp_handler handler = NULL;
	iks *xml_msg = msg->payload;
	iks *response = NULL;

	if (!strcmp("iq", iks_name(xml_msg))) {
		/* is this a command a component supports? */
		handler = rayo_actor_command_handler_find(component, msg);
		if (!handler) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "%s, no component handler function for command\n", RAYO_JID(component));
			if (!msg->is_reply) {
				RAYO_SEND_REPLY(component, msg->from_jid, iks_new_error(xml_msg, STANZA_ERROR_FEATURE_NOT_IMPLEMENTED));
			}
			return;
		}

		/* is the command valid? */
		if (!(response = rayo_component_command_ok(RAYO_COMPONENT(component), msg))) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "%s, executing command\n", RAYO_JID(component));
			response = handler(component, msg, NULL);
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "%s, done executing command\n", RAYO_JID(component));
		}

		if (response) {
			if (!msg->is_reply) {
				RAYO_SEND_REPLY(component, msg->from_jid, response);
			} else {
				iks_delete(response);
			}
			return;
		}
	} else if (!strcmp("presence", iks_name(xml_msg))) {
		/* is this an event the component wants? */
		handler = rayo_actor_event_handler_find(component, msg);
		if (!handler) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "%s, no component handler function for event\n", RAYO_JID(component));
			return;
		}

		/* forward the event */
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "%s, forwarding event\n", RAYO_JID(component));
		response = handler(component, msg, NULL);
		if (response) {
			if (!msg->is_reply) {
				RAYO_SEND_REPLY(component, msg->from_jid, response);
			} else {
				iks_delete(response);
			}
		}
	}
}

/**
 * Add signaling headers to channel -- only works on SIP
 * @param session the channel
 * @param iq_cmd the request containing <header>
 * @param type header type
 */
static void add_signaling_headers(switch_core_session_t *session, iks *iq_cmd, const char *type)
{
	switch_channel_t *channel = switch_core_session_get_channel(session);
	iks *header = NULL;
	for (header = iks_find(iq_cmd, "header"); header; header = iks_next_tag(header)) {
		if (!strcmp("header", iks_name(header))) {
			const char *name = iks_find_attrib_soft(header, "name");
			const char *value = iks_find_attrib_soft(header, "value");
			if (!zstr(name) && !zstr(value)) {
				switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "Adding header: %s: %s\n", name, value);
				switch_channel_set_variable_name_printf(channel, value, "%s%s", type, name);
			}
		}
	}
}

/**
 * Handle <iq><accept> request
 * @param call the Rayo call
 * @param session the session
 * @param node the <iq> node
 */
static iks *on_rayo_accept(struct rayo_actor *call, struct rayo_message *msg, void *session_data)
{
	iks *node = msg->payload;
	switch_core_session_t *session = (switch_core_session_t *)session_data;
	iks *response = NULL;

	/* send ringing */
	add_signaling_headers(session, iks_find(node, "accept"), RAYO_SIP_RESPONSE_HEADER);
	switch_channel_pre_answer(switch_core_session_get_channel(session));
	response = iks_new_iq_result(node);
	return response;
}

/**
 * Handle <iq><answer> request
 * @param call the Rayo call
 * @param session the session
 * @param node the <iq> node
 */
static iks *on_rayo_answer(struct rayo_actor *call, struct rayo_message *msg, void *session_data)
{
	iks *node = msg->payload;
	switch_core_session_t *session = (switch_core_session_t *)session_data;
	iks *response = NULL;

	/* send answer to call */
	add_signaling_headers(session, iks_find(node, "answer"), RAYO_SIP_RESPONSE_HEADER);
	switch_channel_answer(switch_core_session_get_channel(session));
	response = iks_new_iq_result(node);
	return response;
}

/**
 * Handle <iq><redirect> request
 * @param call the Rayo call
 * @param session the session
 * @param node the <iq> node
 */
static iks *on_rayo_redirect(struct rayo_actor *call, struct rayo_message *msg, void *session_data)
{
	iks *node = msg->payload;
	switch_core_session_t *session = (switch_core_session_t *)session_data;
	switch_channel_t *channel = switch_core_session_get_channel(session);
	iks *response = NULL;
	iks *redirect = iks_find(node, "redirect");
	char *redirect_to = iks_find_attrib(redirect, "to");

	if (zstr(redirect_to)) {
		response = iks_new_error_detailed(node, STANZA_ERROR_BAD_REQUEST, "Missing redirect to attrib");
	} else if (switch_channel_test_flag(channel, CF_ANSWERED)) {
		/* call is answered- must deflect */
		switch_core_session_message_t msg = { 0 };
		add_signaling_headers(session, redirect, RAYO_SIP_REQUEST_HEADER);
		msg.from = __FILE__;
		msg.string_arg = switch_core_session_strdup(session, redirect_to);
		msg.message_id = SWITCH_MESSAGE_INDICATE_DEFLECT;
		switch_core_session_receive_message(session, &msg);
		response = iks_new_iq_result(node);
	} else if (switch_channel_direction(channel) == SWITCH_CALL_DIRECTION_INBOUND) {
		/* Inbound call not answered - redirect */
		switch_core_session_message_t msg = { 0 };
		add_signaling_headers(session, redirect, RAYO_SIP_RESPONSE_HEADER);
		msg.from = __FILE__;
		msg.string_arg = switch_core_session_strdup(session, redirect_to);
		msg.message_id = SWITCH_MESSAGE_INDICATE_REDIRECT;
		switch_core_session_receive_message(session, &msg);
		response = iks_new_iq_result(node);
	} else {
		response = iks_new_error_detailed(node, STANZA_ERROR_UNEXPECTED_REQUEST, "Call must be answered");
	}
	return response;
}

/**
 * Handle <iq><hangup> or <iq><reject> request
 * @param call the Rayo call
 * @param session the session
 * @param node the <iq> node
 */
static iks *on_rayo_hangup(struct rayo_actor *call, struct rayo_message *msg, void *session_data)
{
	iks *node = msg->payload;
	switch_core_session_t *session = (switch_core_session_t *)session_data;
	iks *response = NULL;
	iks *hangup = iks_first_tag(node);
	iks *reason = iks_first_tag(hangup);
	int hangup_cause = RAYO_CAUSE_HANGUP;

	/* get hangup cause */
	if (!reason && !strcmp("hangup", iks_name(hangup))) {
		/* no reason in <hangup> */
		hangup_cause = RAYO_CAUSE_HANGUP;
	} else if (reason && !strcmp("reject", iks_name(hangup))) {
		char *reason_name = iks_name(reason);
		/* reason required for <reject> */
		if (!strcmp("busy", reason_name)) {
			hangup_cause = RAYO_CAUSE_BUSY;
		} else if (!strcmp("decline", reason_name)) {
			hangup_cause = RAYO_CAUSE_DECLINE;
		} else if (!strcmp("error", reason_name)) {
			hangup_cause = RAYO_CAUSE_ERROR;
		} else {
			response = iks_new_error_detailed(node, STANZA_ERROR_BAD_REQUEST, "invalid reject reason");
		}
	} else {
		response = iks_new_error(node, STANZA_ERROR_BAD_REQUEST);
	}

	/* do hangup */
	if (!response) {
		switch_channel_set_variable(switch_core_session_get_channel(session), "rayo_local_hangup", "true");
		add_signaling_headers(session, hangup, RAYO_SIP_REQUEST_HEADER);
		add_signaling_headers(session, hangup, RAYO_SIP_RESPONSE_HEADER);
		switch_ivr_kill_uuid(rayo_call_get_uuid(call), hangup_cause);
		response = iks_new_iq_result(node);
	}

	return response;
}

/**
 * Join calls together
 * @param call the call that joins
 * @param session the session
 * @param msg the rayo join message
 * @param call_uri to join
 * @param media mode (direct/bridge)
 * @return the response
 */
static iks *join_call(struct rayo_call *call, switch_core_session_t *session, struct rayo_message *msg, const char *call_uri, const char *media)
{
	iks *node = msg->payload;
	iks *response = NULL;
	/* take call out of media path if media = "direct" */
	const char *bypass = !strcmp("direct", media) ? "true" : "false";

	/* check if joining to rayo call */
	struct rayo_call *b_call = RAYO_CALL_LOCATE(call_uri);
	if (b_call) {
		if (!call->rayo_app_started) {
			/* A-leg not under rayo control yet */
			response = iks_new_error_detailed(node, STANZA_ERROR_UNEXPECTED_REQUEST, "a-leg is not ready to join");
		} else if (!b_call->rayo_app_started) {
			/* B-leg not under rayo control yet */
			response = iks_new_error_detailed(node, STANZA_ERROR_UNEXPECTED_REQUEST, "b-leg is not ready to join");
		} else if (!has_call_control(b_call, msg)) {
			/* not allowed to join to this call */
			response = iks_new_error(node, STANZA_ERROR_NOT_ALLOWED);
		} else if (b_call->joined) {
			/* don't support multiple joined calls */
			response = iks_new_error_detailed(node, STANZA_ERROR_CONFLICT, "multiple joined calls not supported");
		} else {
			/* bridge this call to call-uri */
			switch_channel_set_variable(switch_core_session_get_channel(session), "bypass_media", bypass);
			if (switch_false(bypass)) {
				switch_channel_pre_answer(switch_core_session_get_channel(session));
			}
			call->pending_join_request = iks_copy(node);
			if (switch_ivr_uuid_bridge(rayo_call_get_uuid(call), rayo_call_get_uuid(b_call)) != SWITCH_STATUS_SUCCESS) {
				iks *request = call->pending_join_request;
				iks *result = iks_new_error(request, STANZA_ERROR_SERVICE_UNAVAILABLE);
				call->pending_join_request = NULL;
				RAYO_SEND_REPLY(call, iks_find_attrib_soft(request, "from"), result);
				iks_delete(call->pending_join_request);
			}
		}
		RAYO_RELEASE(b_call);
	} else {
		/* not a rayo call */
		response = iks_new_error_detailed(node, STANZA_ERROR_SERVICE_UNAVAILABLE, "b-leg is gone");
	}
	return response;
}

/**
 * Execute command on session's conference
 * @param session to execute conference API on
 * @param conf_name of conference
 * @param command to send to conference
 * @param node IQ request
 * @return response on failure
 */
static iks *exec_conference_api(switch_core_session_t *session, const char *conf_name, const char *command, iks *node)
{
	iks *response = NULL;
	switch_stream_handle_t stream = { 0 };
	const char *conf_member_id = switch_channel_get_variable(switch_core_session_get_channel(session), "conference_member_id");
	SWITCH_STANDARD_STREAM(stream);
	switch_api_execute("conference", switch_core_session_sprintf(session, "%s %s %s", conf_name, command, conf_member_id), NULL, &stream);
	if (!zstr(stream.data) && strncmp("OK", stream.data, 2)) {
		response = iks_new_error_detailed_printf(node, STANZA_ERROR_SERVICE_UNAVAILABLE, "%s", stream.data);
	}
	switch_safe_free(stream.data);
	return response;
}

/**
 * Execute conference app on session
 * @param session to execute conference API on
 * @param command to send to conference (conference name, member flags, etc)
 * @param node IQ request
 * @return response on failure
 */
static iks *exec_conference_app(switch_core_session_t *session, const char *command, iks *node)
{
	iks *response = NULL;
	switch_event_t *execute_event = NULL;
	switch_channel_t *channel = switch_core_session_get_channel(session);

	/* conference requires local media on channel */
	if (!switch_channel_media_ready(channel) && switch_channel_pre_answer(channel) != SWITCH_STATUS_SUCCESS) {
		/* shit */
		response = iks_new_error_detailed(node, STANZA_ERROR_INTERNAL_SERVER_ERROR, "failed to start media");
		return response;
	}

	/* send execute conference event to session */
	if (switch_event_create(&execute_event, SWITCH_EVENT_COMMAND) == SWITCH_STATUS_SUCCESS) {
		switch_event_add_header_string(execute_event, SWITCH_STACK_BOTTOM, "call-command", "execute");
		switch_event_add_header_string(execute_event, SWITCH_STACK_BOTTOM, "execute-app-name", "conference");
		switch_event_add_header_string(execute_event, SWITCH_STACK_BOTTOM, "execute-app-arg", command);
		//switch_event_add_header_string(execute_event, SWITCH_STACK_BOTTOM, "event_uuid", uuid);
		switch_event_add_header_string(execute_event, SWITCH_STACK_BOTTOM, "event-lock", "true");
		if (!switch_channel_test_flag(channel, CF_PROXY_MODE)) {
			switch_channel_set_flag(channel, CF_BLOCK_BROADCAST_UNTIL_MEDIA);
		}

		if (switch_core_session_queue_private_event(session, &execute_event, SWITCH_FALSE) != SWITCH_STATUS_SUCCESS) {
			response = iks_new_error_detailed(node, STANZA_ERROR_INTERNAL_SERVER_ERROR, "failed to join mixer (queue event failed)");
			if (execute_event) {
				switch_event_destroy(&execute_event);
			}
			return response;
		}
	}
	return response;
}

/**
 * Join call to a mixer
 * @param call the call that joins
 * @param session the session
 * @param msg the join request
 * @param mixer_name the mixer to join
 * @param direction the media direction
 * @return the response
 */
static iks *join_mixer(struct rayo_call *call, switch_core_session_t *session, struct rayo_message *msg, const char *mixer_name, const char *direction)
{
	iks *node = msg->payload;
	iks *response = NULL;

	if (!call->rayo_app_started) {
		/* A-leg not under rayo control yet */
		response = iks_new_error_detailed(node, STANZA_ERROR_UNEXPECTED_REQUEST, "call is not ready to join");
	} else if (call->joined_id) {
		/* adjust join conference params */
		if (!strcmp("duplex", direction)) {
			if ((response = exec_conference_api(session, mixer_name, "unmute", node)) ||
				(response = exec_conference_api(session, mixer_name, "undeaf", node))) {
				return response;
			}
		} else if (!strcmp("recv", direction)) {
			if ((response = exec_conference_api(session, mixer_name, "mute", node)) ||
				(response = exec_conference_api(session, mixer_name, "undeaf", node))) {
				return response;
			}
		} else {
			if ((response = exec_conference_api(session, mixer_name, "unmute", node)) ||
				(response = exec_conference_api(session, mixer_name, "deaf", node))) {
				return response;
			}
		}
		response = iks_new_iq_result(node);
	} else {
		/* join new conference */
		const char *conf_args = switch_core_session_sprintf(session, "%s@%s", mixer_name, globals.mixer_conf_profile);
		if (!strcmp("send", direction)) {
			conf_args = switch_core_session_sprintf(session, "%s+flags{deaf}", conf_args);
		} else if (!strcmp("recv", direction)) {
			conf_args = switch_core_session_sprintf(session, "%s+flags{mute}", conf_args);
		}

		call->pending_join_request = iks_copy(node);
		response = exec_conference_app(session, conf_args, node);
		if (response) {
			iks_delete(call->pending_join_request);
			call->pending_join_request = NULL;
		}
	}
	return response;
}

/**
 * Handle <iq><join> request
 * @param call the Rayo call
 * @param session the session
 * @param msg the rayo join message
 */
static iks *on_rayo_join(struct rayo_actor *call, struct rayo_message *msg, void *session_data)
{
	switch_core_session_t *session = (switch_core_session_t *)session_data;
	iks *response = NULL;
	iks *join = iks_find(msg->payload, "join");
	const char *join_id;
	const char *mixer_name;
	const char *call_uri;

	/* validate input attributes */
	if (!VALIDATE_RAYO_JOIN(join)) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "Bad join attrib\n");
		response = iks_new_error(msg->payload, STANZA_ERROR_BAD_REQUEST);
		goto done;
	}
	mixer_name = iks_find_attrib(join, "mixer-name");
	call_uri = iks_find_attrib(join, "call-uri");

	if (!zstr(mixer_name)) {
		join_id = mixer_name;
	} else {
		join_id = call_uri;
	}

	/* can't join both mixer and call */
	if (!zstr(mixer_name) && !zstr(call_uri)) {
		response = iks_new_error_detailed(msg->payload, STANZA_ERROR_BAD_REQUEST, "mixer-name and call-uri are mutually exclusive");
		goto done;
	}

	/* need to join *something* */
	if (zstr(mixer_name) && zstr(call_uri)) {
		response = iks_new_error_detailed(msg->payload, STANZA_ERROR_BAD_REQUEST, "mixer-name or call-uri is required");
		goto done;
	}

	if ((RAYO_CALL(call)->joined == JOINED_CALL) ||
		(RAYO_CALL(call)->joined == JOINED_MIXER && strcmp(RAYO_CALL(call)->joined_id, join_id))) {
		/* already joined */
		response = iks_new_error_detailed(msg->payload, STANZA_ERROR_CONFLICT, "call is already joined");
		goto done;
	}

	if (rayo_call_is_faxing(RAYO_CALL(call))) {
		/* can't join a call while it's faxing */
		response = iks_new_error_detailed(msg->payload, STANZA_ERROR_UNEXPECTED_REQUEST, "fax is in progress");
		goto done;
	}

	if (RAYO_CALL(call)->pending_join_request) {
		/* don't allow concurrent join requests */
		response = iks_new_error_detailed(msg->payload, STANZA_ERROR_UNEXPECTED_REQUEST, "(un)join request is pending");
		goto done;
	}

	if (!zstr(mixer_name)) {
		/* join conference */
		response = join_mixer(RAYO_CALL(call), session, msg, mixer_name, iks_find_attrib(join, "direction"));
	} else {
		/* bridge calls */
		response = join_call(RAYO_CALL(call), session, msg, call_uri, iks_find_attrib(join, "media"));
	}

done:
	return response;
}

/**
 * unjoin call to a bridge
 * @param call the call that unjoined
 * @param session the session
 * @param msg the unjoin request
 * @param call_uri the b-leg xmpp URI
 * @return the response
 */
static iks *unjoin_call(struct rayo_call *call, switch_core_session_t *session, struct rayo_message *msg, const char *call_uri)
{
	iks *node = msg->payload;
	iks *response = NULL;

	if (!strcmp(call_uri, call->joined_id)) {
		/* unbridge call */
		call->pending_join_request = iks_copy(node);
		switch_ivr_park_session(session);
	} else {
		/* not bridged or wrong b-leg URI */
		response = iks_new_error_detailed_printf(node, STANZA_ERROR_SERVICE_UNAVAILABLE, "expected URI: %s", call->joined_id);
	}

	return response;
}

/**
 * unjoin call to a mixer
 * @param call the call that unjoined
 * @param session the session
 * @param msg the unjoin request
 * @param mixer_name the mixer name
 * @return the response
 */
static iks *unjoin_mixer(struct rayo_call *call, switch_core_session_t *session, struct rayo_message *msg, const char *mixer_name)
{
	switch_channel_t *channel = switch_core_session_get_channel(session);
	const char *conf_member_id = switch_channel_get_variable(channel, "conference_member_id");
	const char *conf_name = switch_channel_get_variable(channel, "conference_name");
	iks *node = msg->payload;
	iks *response = NULL;

	/* not conferenced, or wrong conference */
	if (zstr(conf_name) || strcmp(mixer_name, conf_name)) {
		response = iks_new_error_detailed_printf(node, STANZA_ERROR_SERVICE_UNAVAILABLE, "not joined to %s", mixer_name);
		goto done;
	} else if (zstr(conf_member_id)) {
		/* shouldn't happen */
		response = iks_new_error_detailed(node, STANZA_ERROR_SERVICE_UNAVAILABLE, "channel doesn't have conference member ID");
		goto done;
	}

	/* kick the member */
	response = exec_conference_api(session, mixer_name, "hup", node);
	if (!response) {
		/* ack command */
		response = iks_new_iq_result(node);
	}

done:

	return response;
}

/**
 * Handle <iq><unjoin> request
 * @param call the Rayo call
 * @param session the session
 * @param node the <iq> node
 */
static iks *on_rayo_unjoin(struct rayo_actor *call, struct rayo_message *msg, void *session_data)
{
	switch_core_session_t *session = (switch_core_session_t *)session_data;
	iks *response = NULL;
	iks *unjoin = iks_find(msg->payload, "unjoin");
	const char *call_uri = iks_find_attrib(unjoin, "call-uri");
	const char *mixer_name = iks_find_attrib(unjoin, "mixer-name");

	if (!zstr(call_uri) && !zstr(mixer_name)) {
		response = iks_new_error(msg->payload, STANZA_ERROR_BAD_REQUEST);
	} else if (RAYO_CALL(call)->pending_join_request) {
		/* need to let pending request finish first */
		response = iks_new_error_detailed(msg->payload, STANZA_ERROR_UNEXPECTED_REQUEST, "(un)join request is pending");
	} else if (!RAYO_CALL(call)->joined) {
		/* not joined to anything */
		response = iks_new_error_detailed(msg->payload, STANZA_ERROR_SERVICE_UNAVAILABLE, "not joined to anything");
	} else if (RAYO_CALL(call)->joined == JOINED_MIXER && !zstr(call_uri)) {
		/* joined to mixer, not call */
		response = iks_new_error_detailed(msg->payload, STANZA_ERROR_SERVICE_UNAVAILABLE, "not joined to call");
	} else if (RAYO_CALL(call)->joined == JOINED_CALL && !zstr(mixer_name)) {
		/* joined to call, not mixer */
		response = iks_new_error_detailed(msg->payload, STANZA_ERROR_SERVICE_UNAVAILABLE, "not joined to mixer");
	} else if (!zstr(call_uri)) {
		response = unjoin_call(RAYO_CALL(call), session, msg, call_uri);
	} else if (!zstr(mixer_name)) {
		response = unjoin_mixer(RAYO_CALL(call), session, msg, mixer_name);
	} else {
		/* unjoin everything */
		if (RAYO_CALL(call)->joined == JOINED_MIXER) {
			response = unjoin_mixer(RAYO_CALL(call), session, msg, RAYO_CALL(call)->joined_id);
		} else if (RAYO_CALL(call)->joined == JOINED_CALL) {
			response = unjoin_call(RAYO_CALL(call), session, msg, RAYO_CALL(call)->joined_id);
		} else {
			/* shouldn't happen */
			response = iks_new_error(msg->payload, STANZA_ERROR_INTERNAL_SERVER_ERROR);
		}
	}

	return response;
}

/**
 * @return 1 if display name is valid
 */
static int is_valid_display_name(char *display)
{
	if (zstr(display)) {
		return 0;
	}
	return 1;
}

/**
 * @return 1 if SIP URI is valid
 */
static int is_valid_sip_uri(char *uri)
{
	/* just some basic checks to prevent failure when passing URI as caller ID */
	if (zstr(uri) || strchr(uri, '<') || strchr(uri, '>')) {
		return 0;
	}
	return 1;
}

#define RAYO_URI_SCHEME_UNKNOWN 0
#define RAYO_URI_SCHEME_TEL 1
#define RAYO_URI_SCHEME_SIP 2

/**
 * Parse dial "from" parameter
 * @param pool to use
 * @param from the parameter to parse
 * @param uri the URI
 * @param display the display name
 * @return scheme
 */
static int parse_dial_from(switch_memory_pool_t *pool, const char *from, char **uri, char **display)
{
	if (!zstr(from)) {
		char *l_display = switch_core_strdup(pool, from);
		char *l_uri;

		*display = NULL;
		*uri = NULL;

		/* TODO regex would be better */

		/* split display-name and URI */
		l_uri = strrchr(l_display, ' ');
		if (l_uri) {
			*l_uri++ = '\0';
			if (!zstr(l_display)) {
				/* remove "" from display-name */
				if (l_display[0] == '"') {
					int len;
					*l_display++ = '\0';
					len = strlen(l_display);
					if (len < 2 || l_display[len - 1] != '"') {
						return RAYO_URI_SCHEME_UNKNOWN;
					}
					l_display[len - 1] = '\0';
				}
				if (!is_valid_display_name(l_display)) {
					return RAYO_URI_SCHEME_UNKNOWN;
				}
				*display = l_display;
			}
		} else {
			l_uri = l_display;
		}
		if (zstr(l_uri)) {
			return RAYO_URI_SCHEME_UNKNOWN;
		}

		/* remove <> from URI */
		if (l_uri[0] == '<') {
			int len;
			*l_uri++ = '\0';
			len = strlen(l_uri);
			if (len < 2 || l_uri[len - 1] != '>') {
				return RAYO_URI_SCHEME_UNKNOWN;
			}
			l_uri[len - 1] = '\0';
			if (zstr(l_uri)) {
				return RAYO_URI_SCHEME_UNKNOWN;
			}
		}
		*uri = l_uri;

		/* figure out URI scheme and validate it */
		if (!strncmp("sip:", l_uri, 4) || !strncmp("sips:", l_uri, 5)) {
			/* validate SIP URI */
			if (is_valid_sip_uri(l_uri)) {
				return RAYO_URI_SCHEME_SIP;
			}
		} else if (!strncmp("tel:", l_uri, 4)) {
			l_uri += 4;
			*uri = l_uri;
		}
		if (!zstr(l_uri)) {
			return RAYO_URI_SCHEME_TEL;
		}
	}
	return RAYO_URI_SCHEME_UNKNOWN;
}

struct dial_thread_data {
	switch_memory_pool_t *pool;
	iks *node;
};


/**
 * Thread that handles originating new calls
 * @param thread this thread
 * @param obj the Rayo client
 * @return NULL
 */
static void *SWITCH_THREAD_FUNC rayo_dial_thread(switch_thread_t *thread, void *user)
{
	struct dial_thread_data *dtdata = (struct dial_thread_data *)user;
	iks *iq = dtdata->node;
	iks *dial = iks_find(iq, "dial");
	iks *response = NULL;
	const char *dcp_jid = iks_find_attrib(iq, "from");
	const char *dial_to = iks_find_attrib(dial, "to");
	char *dial_to_dup = NULL;
	const char *dial_from = iks_find_attrib(dial, "from");
	const char *dial_timeout_ms = iks_find_attrib(dial, "timeout");
	const char *requested_call_uri = iks_find_attrib(dial, "uri");
	const char *uuid = NULL;
	switch_event_t *originate_vars = NULL;
	struct dial_gateway *gateway = NULL;
	struct rayo_call *call = NULL;
	uint32_t dial_timeout_sec = 0;

	/* TODO dial_to needs validation */

	/* Check if optional URI is valid. */
	if (!zstr(requested_call_uri)) {

		/* Split node and domain from URI */
		char *requested_call_uri_dup = switch_core_strdup(dtdata->pool, requested_call_uri);
		char *requested_call_uri_domain = strchr(requested_call_uri_dup, '@');
		if (requested_call_uri_domain) {
			*requested_call_uri_domain = '\0';
			requested_call_uri_domain++;
		}

		/* is domain missing */
		if (zstr(requested_call_uri_domain)) {
			response = iks_new_error_detailed(iq, STANZA_ERROR_BAD_REQUEST, "Bad uri");
			goto done;
		}

		/* is domain correct? */
		if (strcmp(requested_call_uri_domain, RAYO_JID(globals.server))) {
			response = iks_new_error_detailed(iq, STANZA_ERROR_BAD_REQUEST, "Bad uri (invalid domain)");
			goto done;
		}

		/* is node identifier missing? */
		if (zstr(requested_call_uri_dup)) {
			response = iks_new_error_detailed(iq, STANZA_ERROR_BAD_REQUEST, "Bad uri (missing node)");
			goto done;
		}

		/* strip optional xmpp: from node identifier */
		if (!strncasecmp("xmpp:", requested_call_uri_dup, 5)) {
			requested_call_uri_dup += 5;
			if (zstr(requested_call_uri_dup)) {
				response = iks_new_error_detailed(iq, STANZA_ERROR_BAD_REQUEST, "Bad uri (missing node)");
				goto done;
			}
		}

		/* success! */
		uuid = requested_call_uri_dup;
	}

	/* create call and link to DCP */
	call = rayo_call_create(uuid);
	if (!call) {
		response = iks_new_error(iq, STANZA_ERROR_CONFLICT);
		goto done;
	}
	call->dcp_jid = switch_core_strdup(RAYO_POOL(call), dcp_jid);
	call->dial_request_id = switch_core_strdup(RAYO_POOL(call), iks_find_attrib_soft(iq, "id"));
	switch_log_printf(SWITCH_CHANNEL_UUID_LOG(rayo_call_get_uuid(call)), SWITCH_LOG_INFO, "%s has control of call\n", dcp_jid);
	uuid = switch_core_strdup(dtdata->pool, rayo_call_get_uuid(call));

	/* create container for origination variables */
	if (switch_event_create_plain(&originate_vars, SWITCH_EVENT_CHANNEL_DATA) != SWITCH_STATUS_SUCCESS) {
		abort();
	}

	/* add dialstring vars to origination variables */
	if (*dial_to == '{') {
		dial_to_dup = switch_core_strdup(dtdata->pool, dial_to);
		switch_log_printf(SWITCH_CHANNEL_UUID_LOG(rayo_call_get_uuid(call)), SWITCH_LOG_DEBUG, "dial: parsing dialstring channel variables\n");
		switch_event_create_brackets(dial_to_dup, '{', '}', ',', &originate_vars, (char **)&dial_to, SWITCH_FALSE);
	}

	/* set originate channel variables */
	switch_event_add_header_string(originate_vars, SWITCH_STACK_BOTTOM, "origination_uuid", rayo_call_get_uuid(call));
	switch_event_add_header_string(originate_vars, SWITCH_STACK_BOTTOM, "rayo_dcp_jid", dcp_jid);
	switch_event_add_header_string(originate_vars, SWITCH_STACK_BOTTOM, "rayo_call_jid", RAYO_JID(call));

	if (!zstr(dial_from)) {
		char *from_uri = NULL;
		char *from_display;
		int scheme = parse_dial_from(dtdata->pool, dial_from, &from_uri, &from_display);
		if (scheme == RAYO_URI_SCHEME_UNKNOWN) {
			response = iks_new_error_detailed(iq, STANZA_ERROR_BAD_REQUEST, "Bad from URI");
			goto done;
		} else if (scheme == RAYO_URI_SCHEME_SIP) {
			/* SIP URI */
			if (!zstr(from_uri)) {
				switch_event_add_header_string(originate_vars, SWITCH_STACK_BOTTOM, "sip_from_uri", from_uri);
				switch_log_printf(SWITCH_CHANNEL_UUID_LOG(rayo_call_get_uuid(call)), SWITCH_LOG_DEBUG, "dial: sip_from_uri=%s\n", from_uri);
			}
			if (!zstr(from_display)) {
				switch_event_add_header_string(originate_vars, SWITCH_STACK_BOTTOM, "sip_from_display", from_display);
				switch_log_printf(SWITCH_CHANNEL_UUID_LOG(rayo_call_get_uuid(call)), SWITCH_LOG_DEBUG, "dial: sip_from_display=%s\n", from_display);
			}
		}
		if (!zstr(from_uri)) {
			switch_event_add_header_string(originate_vars, SWITCH_STACK_BOTTOM, "origination_caller_id_number", from_uri);
			switch_log_printf(SWITCH_CHANNEL_UUID_LOG(rayo_call_get_uuid(call)), SWITCH_LOG_DEBUG, "dial: origination_caller_id_number=%s\n", from_uri);
		}
		if (!zstr(from_display)) {
			switch_event_add_header_string(originate_vars, SWITCH_STACK_BOTTOM, "origination_caller_id_name", from_display);
			switch_log_printf(SWITCH_CHANNEL_UUID_LOG(rayo_call_get_uuid(call)), SWITCH_LOG_DEBUG, "dial: origination_caller_id_name=%s\n", from_display);
		} else if (scheme == RAYO_URI_SCHEME_TEL && !zstr(from_uri)) {
			/* set caller ID name to same as number if telephone number and a name wasn't specified */
			switch_event_add_header_string(originate_vars, SWITCH_STACK_BOTTOM, "origination_caller_id_name", from_uri);
			switch_log_printf(SWITCH_CHANNEL_UUID_LOG(rayo_call_get_uuid(call)), SWITCH_LOG_DEBUG, "dial: origination_caller_id_name=%s\n", from_uri);
		}
	}
	if (!zstr(dial_timeout_ms) && switch_is_number(dial_timeout_ms)) {
		dial_timeout_sec = round((double)atoi(dial_timeout_ms) / 1000.0);
	}

	/* set outbound signaling headers - only works on SIP */
	{
		iks *header = NULL;
		for (header = iks_find(dial, "header"); header; header = iks_next_tag(header)) {
			if (!strcmp("header", iks_name(header))) {
				const char *name = iks_find_attrib_soft(header, "name");
				const char *value = iks_find_attrib_soft(header, "value");
				if (!zstr(name) && !zstr(value)) {
					char *header_name = switch_core_sprintf(dtdata->pool, "%s%s", RAYO_SIP_REQUEST_HEADER, name);
					switch_log_printf(SWITCH_CHANNEL_UUID_LOG(rayo_call_get_uuid(call)), SWITCH_LOG_DEBUG, "dial: Adding SIP header: %s: %s\n", name, value);
					switch_event_add_header_string(originate_vars, SWITCH_STACK_BOTTOM, header_name, value);
				}
			}
		}
	}

	/* build dialstring and dial call */
	gateway = dial_gateway_find(dial_to);
	if (gateway) {
		iks *join = iks_find(dial, "join");
		const char *dial_to_stripped = dial_to + gateway->strip;
		switch_core_session_t *called_session = NULL;
		switch_call_cause_t cause = SWITCH_CAUSE_NORMAL_CLEARING;
		const char *dialstring = NULL;

		if (join) {
			/* check join args */
			const char *call_uri = iks_find_attrib(join, "call-uri");
			const char *mixer_name = iks_find_attrib(join, "mixer-name");

			if (!zstr(call_uri) && !zstr(mixer_name)) {
				/* can't join both */
				response = iks_new_error(iq, STANZA_ERROR_BAD_REQUEST);
				goto done;
			} else if (zstr(call_uri) && zstr(mixer_name)) {
				/* nobody to join to? */
				response = iks_new_error(iq, STANZA_ERROR_BAD_REQUEST);
				goto done;
			} else if (!zstr(call_uri)) {
				/* bridge */
				struct rayo_call *peer_call = RAYO_CALL_LOCATE(call_uri);
				/* is peer call available? */
				if (!peer_call) {
					response = iks_new_error_detailed(iq, STANZA_ERROR_SERVICE_UNAVAILABLE, "peer call not found");
					goto done;
				} else if (peer_call->joined) {
					response = iks_new_error_detailed(iq, STANZA_ERROR_SERVICE_UNAVAILABLE, "peer call already joined");
					RAYO_RELEASE(peer_call);
					goto done;
				}
				switch_event_add_header(originate_vars, SWITCH_STACK_BOTTOM, "rayo_origination_args", "bridge %s", rayo_call_get_uuid(peer_call));
				RAYO_RELEASE(peer_call);
			} else {
				/* conference */
				switch_event_add_header(originate_vars, SWITCH_STACK_BOTTOM, "rayo_origination_args", "conference %s@%s", mixer_name, globals.mixer_conf_profile);
			}
		}

		dialstring = switch_core_sprintf(dtdata->pool, "%s%s", gateway->dial_prefix, dial_to_stripped);
		switch_log_printf(SWITCH_CHANNEL_UUID_LOG(rayo_call_get_uuid(call)), SWITCH_LOG_DEBUG, "dial: Using dialstring: %s\n", dialstring);

		/* <iq><ref> response will be sent when originate event is received- otherwise error is returned */
		if (switch_ivr_originate(NULL, &called_session, &cause, dialstring, dial_timeout_sec, NULL, NULL, NULL, NULL, originate_vars, SOF_NONE, NULL) == SWITCH_STATUS_SUCCESS && called_session) {
			/* start APP */
			switch_caller_extension_t *extension = NULL;
			switch_channel_t *called_channel = switch_core_session_get_channel(called_session);
			switch_log_printf(SWITCH_CHANNEL_UUID_LOG(uuid), SWITCH_LOG_DEBUG, "dial: Call originated\n");
			if ((extension = switch_caller_extension_new(called_session, "rayo", NULL)) == 0) {
				switch_log_printf(SWITCH_CHANNEL_UUID_LOG(rayo_call_get_uuid(call)), SWITCH_LOG_CRIT, "Memory Error!\n");
				abort();
			}
			switch_caller_extension_add_application(called_session, extension, "rayo", NULL);
			switch_channel_set_caller_extension(called_channel, extension);
			switch_channel_set_state(called_channel, CS_EXECUTE);
			switch_core_session_rwunlock(called_session);
		} else {
			switch_log_printf(SWITCH_CHANNEL_UUID_LOG(uuid), SWITCH_LOG_DEBUG, "dial: Failed to originate call: %s\n", switch_channel_cause2str(cause));
			switch_mutex_lock(RAYO_ACTOR(call)->mutex);
			if (!zstr(call->dial_request_id)) {
				call->dial_request_failed = 1;
				call->dial_request_id = NULL;

				/* map failure reason to iq error */
				switch (cause) {
					case SWITCH_CAUSE_DESTINATION_OUT_OF_ORDER:
						/* out of sessions, typically */
						response = iks_new_error_detailed(iq, STANZA_ERROR_RESOURCE_CONSTRAINT, "DESTINATION_OUT_OF_ORDER");
						break;
					case SWITCH_CAUSE_SUBSCRIBER_ABSENT:
					case SWITCH_CAUSE_USER_NOT_REGISTERED: {
						/* call session was never created, so we must fake it so that a call error is sent and
						   not a dial error */
						/* send ref response to DCP immediately followed with failure */
						iks *ref;
						iks *ref_response = iks_new("iq");
						iks_insert_attrib(ref_response, "from", RAYO_JID(globals.server));
						iks_insert_attrib(ref_response, "to", dcp_jid);
						iks_insert_attrib(ref_response, "id", iks_find_attrib_soft(iq, "id"));
						iks_insert_attrib(ref_response, "type", "result");
						ref = iks_insert(ref_response, "ref");
						iks_insert_attrib(ref, "xmlns", RAYO_NS);
						iks_insert_attrib_printf(ref, "uri", "xmpp:%s", RAYO_JID(call));
						RAYO_SEND_MESSAGE(globals.server, dcp_jid, ref_response);

						/* send subscriber-absent call hangup reason */
						rayo_call_send_end(call, NULL, 0, "SUBSCRIBER_ABSENT", "20");

						/* destroy call */
						RAYO_DESTROY(call);
						RAYO_RELEASE(call);
						break;
					}
					case SWITCH_CAUSE_EXCHANGE_ROUTING_ERROR:
						/* max forwards */
						response = iks_new_error_detailed(iq, STANZA_ERROR_RESOURCE_CONSTRAINT, "EXCHANGE_ROUTING_ERROR");
						break;
					case SWITCH_CAUSE_CHAN_NOT_IMPLEMENTED:
						/* unsupported endpoint type */
						response = iks_new_error_detailed(iq, STANZA_ERROR_BAD_REQUEST, "CHAN_NOT_IMPLEMENTED");
						break;
					case SWITCH_CAUSE_INVALID_URL:
						response = iks_new_error_detailed(iq, STANZA_ERROR_BAD_REQUEST, "INVALID_URL");
						break;
					case SWITCH_CAUSE_INVALID_GATEWAY:
						response = iks_new_error_detailed(iq, STANZA_ERROR_BAD_REQUEST, "INVALID_GATEWAY");
						break;
					case SWITCH_CAUSE_INVALID_PROFILE:
						response = iks_new_error_detailed(iq, STANZA_ERROR_BAD_REQUEST, "INVALID_PROFILE");
						break;
					case SWITCH_CAUSE_SYSTEM_SHUTDOWN:
						response = iks_new_error_detailed(iq, STANZA_ERROR_RESOURCE_CONSTRAINT, "SYSTEM_SHUTDOWN");
						break;
					case SWITCH_CAUSE_GATEWAY_DOWN:
						response = iks_new_error_detailed(iq, STANZA_ERROR_RESOURCE_CONSTRAINT, "GATEWAY_DOWN");
						break;
					case SWITCH_CAUSE_INVALID_NUMBER_FORMAT:
						response = iks_new_error_detailed(iq, STANZA_ERROR_RESOURCE_CONSTRAINT, "INVALID_NUMBER_FORMAT");
						break;
					default:
						response = iks_new_error_detailed(iq, STANZA_ERROR_INTERNAL_SERVER_ERROR, switch_channel_cause2str(cause));
						break;
				}
			}
			switch_mutex_unlock(RAYO_ACTOR(call)->mutex);
		}
	} else {
		/* will only happen if misconfigured */
		switch_log_printf(SWITCH_CHANNEL_UUID_LOG(rayo_call_get_uuid(call)), SWITCH_LOG_CRIT, "dial: No dial gateway found for %s!\n", dial_to);
		call->dial_request_failed = 1;
		call->dial_request_id = NULL;
		response = iks_new_error_detailed_printf(iq, STANZA_ERROR_INTERNAL_SERVER_ERROR, "dial: No dial gateway found for %s!\n", dial_to);
		goto done;
	}

done:

	/* response when error */
	if (response) {
		/* send response to client */
		RAYO_SEND_REPLY(globals.server, iks_find_attrib(response, "to"), response);

		/* destroy call */
		if (call) {
			RAYO_DESTROY(call);
			RAYO_RELEASE(call);
		}
	}

	iks_delete(iq);

	if (originate_vars) {
		switch_event_destroy(&originate_vars);
	}

	{
		switch_memory_pool_t *pool = dtdata->pool;
		switch_core_destroy_memory_pool(&pool);
	}

	return NULL;
}


/**
 * Dial a new call
 * @param rclient requesting the call
 * @param server handling the call
 * @param node the request
 */
static iks *on_rayo_dial(struct rayo_actor *server, struct rayo_message *msg, void *data)
{
	iks *node = msg->payload;
	switch_thread_t *thread;
	switch_threadattr_t *thd_attr = NULL;
	iks *dial = iks_find(node, "dial");
	iks *response = NULL;
	const char *dial_to = iks_find_attrib(dial, "to");

	if (zstr(dial_to)) {
		response = iks_new_error_detailed(node, STANZA_ERROR_BAD_REQUEST, "missing dial to attribute");
	} else if (strchr(dial_to, ' ')) {
		response = iks_new_error_detailed(node, STANZA_ERROR_BAD_REQUEST, "malformed dial string");
	} else {
		switch_memory_pool_t *pool;
		struct dial_thread_data *dtdata = NULL;
		switch_core_new_memory_pool(&pool);
		dtdata = switch_core_alloc(pool, sizeof(*dtdata));
		dtdata->pool = pool;
		dtdata->node = iks_copy(node);

		iks_insert_attrib(dtdata->node, "from", msg->from_jid); /* save DCP jid in case it isn't specified */

		/* start dial thread */
		switch_threadattr_create(&thd_attr, pool);
		switch_threadattr_detach_set(thd_attr, 1);
		switch_threadattr_stacksize_set(thd_attr, SWITCH_THREAD_STACKSIZE);
		switch_thread_create(&thread, thd_attr, rayo_dial_thread, dtdata, pool);
	}

	return response;
}

/**
 * Handle <iq><ping> request
 * @param rclient the Rayo client
 * @param server the Rayo server
 * @param node the <iq> node
 * @return NULL
 */
static iks *on_iq_xmpp_ping(struct rayo_actor *server, struct rayo_message *msg, void *data)
{
	iks *node = msg->payload;
	iks *pong = iks_new("iq");
	char *from = iks_find_attrib(node, "from");
	char *to = iks_find_attrib(node, "to");

	if (zstr(from)) {
		from = msg->from_jid;
	}

	if (zstr(to)) {
		to = RAYO_JID(server);
	}

	iks_insert_attrib(pong, "type", "result");
	iks_insert_attrib(pong, "from", to);
	iks_insert_attrib(pong, "to", from);
	iks_insert_attrib(pong, "id", iks_find_attrib(node, "id"));

	return pong;
}

/**
 * Handle service discovery request
 * @param rclient the Rayo client
 * @param server the Rayo server
 * @param node the <iq> node
 * @return NULL
 */
static iks *on_iq_get_xmpp_disco(struct rayo_actor *server, struct rayo_message *msg, void *data)
{
	iks *node = msg->payload;
	iks *response = NULL;
	iks *x;
	iks *feature;
	iks *identity;
	int i = 0;
	const char *feature_string;
	response = iks_new_iq_result(node);
	x = iks_insert(response, "query");
	iks_insert_attrib(x, "xmlns", IKS_NS_XMPP_DISCO);
	identity = iks_insert(x, "identity");
	iks_insert_attrib(identity, "category", rayo_server_identity.category);
	iks_insert_attrib(identity, "type", rayo_server_identity.type);
	i = 0;
	while((feature_string = rayo_server_features[i++])) {
		feature = iks_insert(x, "feature");
		iks_insert_attrib(feature, "var", feature_string);
	}

	/* TODO The response MUST also include features for the application formats and transport methods supported by
	 * the responding entity, as described in the relevant specifications.
	 */

	return response;
}

/**
 * Handle message from client
 * @param rclient that sent the command
 * @param message the message
 */
static void on_client_message(struct rayo_client *rclient, iks *message)
{
	const char *to = iks_find_attrib(message, "to");

	/* must be directed to a client */
	if (zstr(to)) {
		return;
	}

	/* assume client source */
	if (zstr(iks_find_attrib(message, "from"))) {
		iks_insert_attrib(message, "from", RAYO_JID(rclient));
	}

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "%s, recv message, availability = %s\n", RAYO_JID(rclient), presence_status_to_string(rclient->availability));

	RAYO_SEND_MESSAGE_DUP(rclient, to, message);
}

/**
 * Handle <presence> message from a client
 * @param rclient the client
 * @param node the presence message
 */
static void on_client_presence(struct rayo_client *rclient, iks *node)
{
	char *type = iks_find_attrib(node, "type");
	enum presence_status status = PS_UNKNOWN;

	/*
	   From RFC-6121:
	   Entity is available when <presence/> received.
	   Entity is unavailable when <presence type='unavailable'/> is received.

	   From Rayo-XEP:
	   Entity is available when <presence to='foo' from='bar'><show>chat</show></presence> is received.
	   Entity is unavailable when <presence to='foo' from='bar'><show>dnd</show></presence> is received.
	*/

	/* figure out if online/offline */
	if (zstr(type)) {
		/* <presence><show>chat</show></presence> */
		char *status_str = iks_find_cdata(node, "show");
		if (!zstr(status_str)) {
			if (!strcmp("chat", status_str)) {
				status = PS_ONLINE;
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "%s got chat presence\n", RAYO_JID(rclient));
			} else if (!strcmp("dnd", status_str)) {
				status = PS_OFFLINE;
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "%s got dnd presence\n", RAYO_JID(rclient));
			}
		} else {
			/* <presence/> */
			status = PS_ONLINE;
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "%s got empty presence\n", RAYO_JID(rclient));
		}
	} else if (!strcmp("unavailable", type)) {
		status = PS_OFFLINE;
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "%s got unavailable presence\n", RAYO_JID(rclient));
	} else if (!strcmp("error", type)) {
		/* TODO presence error */
	} else if (!strcmp("probe", type)) {
		/* TODO presence probe */
	} else if (!strcmp("subscribe", type)) {
		/* TODO presence subscribe */
	} else if (!strcmp("subscribed", type)) {
		/* TODO presence subscribed */
	} else if (!strcmp("unsubscribe", type)) {
		/* TODO presence unsubscribe */
	} else if (!strcmp("unsubscribed", type)) {
		/* TODO presence unsubscribed */
	}

	if (status == PS_ONLINE && rclient->availability != PS_ONLINE) {
		rclient->availability = PS_ONLINE;
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "%s is ONLINE\n", RAYO_JID(rclient));
	} else if (status == PS_OFFLINE && rclient->availability != PS_OFFLINE) {
		rclient->availability = PS_OFFLINE;
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "%s is OFFLINE\n", RAYO_JID(rclient));
	}

	/* destroy if not a local client (connected via peer_server) and is OFFLINE */
	if (rclient->peer_server && rclient->availability == PS_OFFLINE) {
		RAYO_DESTROY(rclient);
		RAYO_RELEASE(rclient);
	}

	pause_when_offline();
}

/**
 * Handle command from client
 * @param rclient that sent the command
 * @param iq the command
 */
static void rayo_client_command_recv(struct rayo_client *rclient, iks *iq)
{
	iks *command = iks_first_tag(iq);
	const char *to = iks_find_attrib(iq, "to");

	/* assume server destination */
	if (zstr(to)) {
		to = RAYO_JID(globals.server);
		iks_insert_attrib(iq, "to", to);
	}

	/* assume client source */
	if (zstr(iks_find_attrib(iq, "from"))) {
		iks_insert_attrib(iq, "from", RAYO_JID(rclient));
	}

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "%s, recv iq, availability = %s\n", RAYO_JID(rclient), presence_status_to_string(rclient->availability));

	if (command) {
		RAYO_SEND_MESSAGE_DUP(rclient, to, iq);
	} else {
		const char *type = iks_find_attrib_soft(iq, "type");
		if (strcmp("error", type) && strcmp("result", type)) {
			RAYO_SEND_REPLY(globals.server, RAYO_JID(rclient), iks_new_error_detailed(iq, STANZA_ERROR_BAD_REQUEST, "empty IQ request"));
		}
	}
}

/**
 * Send event to mixer subscribers
 * @param mixer the mixer
 * @param rayo_event the event to send
 */
static void broadcast_mixer_event(struct rayo_mixer *mixer, iks *rayo_event)
{
	switch_hash_index_t *hi = NULL;
	switch_mutex_lock(RAYO_ACTOR(mixer)->mutex);
	for (hi = switch_core_hash_first(mixer->subscribers); hi; hi = switch_core_hash_next(&hi)) {
		const void *key;
		void *val;
		struct rayo_mixer_subscriber *subscriber;
		switch_core_hash_this(hi, &key, NULL, &val);
		subscriber = (struct rayo_mixer_subscriber *)val;
		switch_assert(subscriber);
		iks_insert_attrib(rayo_event, "to", subscriber->jid);
		RAYO_SEND_MESSAGE_DUP(mixer, subscriber->jid, rayo_event);
	}
	switch_mutex_unlock(RAYO_ACTOR(mixer)->mutex);
}

/**
 * Handle mixer delete member event
 */
static void on_mixer_delete_member_event(struct rayo_mixer *mixer, switch_event_t *event)
{
	iks *delete_member_event, *x;
	const char *uuid = switch_event_get_header(event, "Unique-ID");
	struct rayo_call *call;
	struct rayo_mixer_member *member;
	struct rayo_mixer_subscriber *subscriber;

	/* not a rayo mixer */
	if (!mixer) {
		return;
	}

	/* remove member from mixer */
	switch_mutex_lock(RAYO_ACTOR(mixer)->mutex);
	member = (struct rayo_mixer_member *)switch_core_hash_find(mixer->members, uuid);
	if (!member) {
		/* not a member */
		switch_mutex_unlock(RAYO_ACTOR(mixer)->mutex);
		return;
	}
	switch_core_hash_delete(mixer->members, uuid);
	switch_mutex_unlock(RAYO_ACTOR(mixer)->mutex);

	/* flag call as available to join another mixer */
	call = RAYO_CALL_LOCATE_BY_ID(uuid);
	if (call) {
		switch_mutex_lock(RAYO_ACTOR(call)->mutex);
		call->joined = 0;
		call->joined_id = NULL;
		switch_mutex_unlock(RAYO_ACTOR(call)->mutex);
		RAYO_RELEASE(call);
	}

	/* send mixer unjoined event to member DCP */
	delete_member_event = iks_new_presence("unjoined", RAYO_NS, member->jid, member->dcp_jid);
	x = iks_find(delete_member_event, "unjoined");
	iks_insert_attrib(x, "mixer-name", rayo_mixer_get_name(mixer));
	RAYO_SEND_MESSAGE(mixer, member->dcp_jid, delete_member_event);

	/* broadcast member unjoined event to subscribers */
	delete_member_event = iks_new_presence("unjoined", RAYO_NS, RAYO_JID(mixer), "");
	x = iks_find(delete_member_event, "unjoined");
	iks_insert_attrib_printf(x, "call-uri", "xmpp:%s@%s", uuid, RAYO_JID(globals.server));
	broadcast_mixer_event(mixer, delete_member_event);
	iks_delete(delete_member_event);

	/* remove member DCP as subscriber to mixer */
	switch_mutex_lock(RAYO_ACTOR(mixer)->mutex);
	subscriber = (struct rayo_mixer_subscriber *)switch_core_hash_find(mixer->subscribers, member->dcp_jid);
	if (subscriber) {
		subscriber->ref_count--;
		if (subscriber->ref_count <= 0) {
			switch_core_hash_delete(mixer->subscribers, member->dcp_jid);
		}
	}
	switch_mutex_unlock(RAYO_ACTOR(mixer)->mutex);
}

/**
 * Handle mixer destroy event
 */
static void on_mixer_destroy_event(struct rayo_mixer *mixer, switch_event_t *event)
{
	if (mixer) {
		iks *presence;

		/* notify online clients of mixer destruction */
		presence = iks_new("presence");
		iks_insert_attrib(presence, "from", RAYO_JID(mixer));
		iks_insert_attrib(presence, "type", "unavailable");
		broadcast_event(RAYO_ACTOR(mixer), presence, 1);
		iks_delete(presence);

		/* remove from hash and destroy */
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "%s, destroying mixer: %s\n", RAYO_JID(mixer), rayo_mixer_get_name(mixer));
		RAYO_RELEASE(mixer); /* release original lock */
		RAYO_DESTROY(mixer);
	} else {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "destroy: NULL mixer\n");
	}
}

/**
 * Handle mixer add member event
 */
static void on_mixer_add_member_event(struct rayo_mixer *mixer, switch_event_t *event)
{
	iks *add_member_event = NULL, *x;
	const char *uuid = switch_event_get_header(event, "Unique-ID");
	struct rayo_call *call = RAYO_CALL_LOCATE_BY_ID(uuid);
	struct rayo_mixer *lmixer = NULL;

	if (!mixer) {
		char *ver;
		iks *presence, *c;

		/* new mixer */
		const char *mixer_name = switch_event_get_header(event, "Conference-Name");
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "creating mixer: %s\n", mixer_name);
		mixer = rayo_mixer_create(mixer_name);
		if (mixer) {
			/* notify online clients of mixer presence */
			ver = calculate_entity_sha1_ver(&rayo_mixer_identity, rayo_mixer_features);

			presence = iks_new_presence("c", IKS_NS_XMPP_ENTITY_CAPABILITIES, RAYO_JID(mixer), "");
			c = iks_find(presence, "c");
			iks_insert_attrib(c, "hash", "sha-1");
			iks_insert_attrib(c, "node", RAYO_MIXER_NS);
			iks_insert_attrib(c, "ver", ver);
			free(ver);

			broadcast_event(RAYO_ACTOR(mixer), presence, 1);
		} else {
			/* must have lost the race to another add member event...  there should be a mixer with mixer_name already */
			mixer = lmixer = RAYO_MIXER_LOCATE(mixer_name);
			if (!mixer) {
				/* this is unexpected */
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "failed to find mixer: %s\n", mixer_name);
				return;
			}
		}
	}

	if (call) {
		struct rayo_mixer_member *member = NULL;
		/* add member DCP as subscriber to mixer */
		struct rayo_mixer_subscriber *subscriber;
		switch_mutex_lock(RAYO_ACTOR(mixer)->mutex);
		subscriber = (struct rayo_mixer_subscriber *)switch_core_hash_find(mixer->subscribers, call->dcp_jid);
		if (!subscriber) {
			subscriber = switch_core_alloc(RAYO_POOL(mixer), sizeof(*subscriber));
			subscriber->ref_count = 0;
			subscriber->jid = switch_core_strdup(RAYO_POOL(mixer), call->dcp_jid);
			switch_core_hash_insert(mixer->subscribers, call->dcp_jid, subscriber);
		}
		subscriber->ref_count++;

		/* add call as member of mixer */
		member = switch_core_alloc(RAYO_POOL(mixer), sizeof(*member));
		member->jid = switch_core_strdup(RAYO_POOL(mixer), RAYO_JID(call));
		member->dcp_jid = subscriber->jid;
		switch_core_hash_insert(mixer->members, uuid, member);

		switch_mutex_unlock(RAYO_ACTOR(mixer)->mutex);

		switch_mutex_lock(RAYO_ACTOR(call)->mutex);
		call->joined = JOINED_MIXER;
		call->joined_id = switch_core_strdup(RAYO_POOL(call), rayo_mixer_get_name(mixer));

		/* send IQ result to client now. */
		if (call->pending_join_request) {
			iks *request = call->pending_join_request;
			iks *result = iks_new_iq_result(request);
			iks *ref = iks_insert(result, "ref");
			iks_insert_attrib(ref, "xmlns", RAYO_NS);
			iks_insert_attrib_printf(ref, "uri", "xmpp:%s", RAYO_JID(mixer));
			call->pending_join_request = NULL;
			RAYO_SEND_REPLY(call, iks_find_attrib_soft(request, "from"), result);
			iks_delete(request);
		}
		switch_mutex_unlock(RAYO_ACTOR(call)->mutex);

		/* send mixer joined event to member DCP */
		add_member_event = iks_new_presence("joined", RAYO_NS, RAYO_JID(call), call->dcp_jid);
		x = iks_find(add_member_event, "joined");
		iks_insert_attrib(x, "mixer-name", rayo_mixer_get_name(mixer));
		RAYO_SEND_MESSAGE(call, call->dcp_jid, add_member_event);

		RAYO_RELEASE(call);
	}

	/* broadcast member joined event to subscribers */
	add_member_event = iks_new_presence("joined", RAYO_NS, RAYO_JID(mixer), "");
	x = iks_find(add_member_event, "joined");
	iks_insert_attrib_printf(x, "call-uri", "xmpp:%s@%s", uuid, RAYO_JID(globals.server));
	broadcast_mixer_event(mixer, add_member_event);
	iks_delete(add_member_event);

	if (lmixer) {
		RAYO_RELEASE(lmixer);
	}
}

/**
 * Receives mixer events from FreeSWITCH core and routes them to the proper Rayo client(s).
 * @param event received from FreeSWITCH core.  It will be destroyed by the core after this function returns.
 */
static void route_mixer_event(switch_event_t *event)
{
	const char *action = switch_event_get_header(event, "Action");
	const char *profile = switch_event_get_header(event, "Conference-Profile-Name");
	const char *mixer_name = switch_event_get_header(event, "Conference-Name");
	struct rayo_mixer *mixer = NULL;

	if (strcmp(profile, globals.mixer_conf_profile)) {
		/* don't care about other conferences */
		goto done;
	}

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "looking for mixer: %s\n", mixer_name);
	mixer = RAYO_MIXER_LOCATE(mixer_name);

	if (!strcmp("add-member", action)) {
		on_mixer_add_member_event(mixer, event);
	} else if (!strcmp("conference-destroy", action)) {
		on_mixer_destroy_event(mixer, event);
	} else if (!strcmp("del-member", action)) {
		on_mixer_delete_member_event(mixer, event);
	}
	/* TODO speaking events */

done:
	RAYO_RELEASE(mixer);
}

/**
 * Handle call originate event - create rayo call and send <iq><ref> to client.
 * @param rclient The Rayo client
 * @param event the originate event
 */
static void on_call_originate_event(struct rayo_client *rclient, switch_event_t *event)
{
	const char *uuid = switch_event_get_header(event, "Unique-ID");
	struct rayo_call *call = RAYO_CALL_LOCATE_BY_ID(uuid);

	if (call) {
		iks *response, *ref;

		switch_log_printf(SWITCH_CHANNEL_UUID_LOG(RAYO_ID(call)), SWITCH_LOG_DEBUG, "Got originate event\n");

		switch_mutex_lock(RAYO_ACTOR(call)->mutex);
		if (!zstr(call->dial_request_id)) {
			/* send response to DCP */
			response = iks_new("iq");
			iks_insert_attrib(response, "from", RAYO_JID(globals.server));
			iks_insert_attrib(response, "to", rayo_call_get_dcp_jid(call));
			iks_insert_attrib(response, "id", call->dial_request_id);
			iks_insert_attrib(response, "type", "result");
			ref = iks_insert(response, "ref");
			iks_insert_attrib(ref, "xmlns", RAYO_NS);

			iks_insert_attrib_printf(ref, "uri", "xmpp:%s", RAYO_JID(call));
			RAYO_SEND_MESSAGE(call, RAYO_JID(rclient), response);
			call->dial_request_id = NULL;
		}
		switch_mutex_unlock(RAYO_ACTOR(call)->mutex);
	}
	RAYO_RELEASE(call);
}

/**
 * Handle call end event
 * @param event the hangup event
 */
static void on_call_end_event(switch_event_t *event)
{
	struct rayo_call *call = RAYO_CALL_LOCATE_BY_ID(switch_event_get_header(event, "Unique-ID"));

	if (call) {
#if 0
		char *event_str;
		if (switch_event_serialize(event, &event_str, SWITCH_FALSE) == SWITCH_STATUS_SUCCESS) {
			switch_log_printf(SWITCH_CHANNEL_UUID_LOG(rayo_call_get_uuid(call)), SWITCH_LOG_DEBUG, "%s\n", event_str);
			switch_safe_free(event_str);
		}
#endif
		switch_log_printf(SWITCH_CHANNEL_UUID_LOG(RAYO_ID(call)), SWITCH_LOG_DEBUG, "Got channel destroy event\n");

		switch_mutex_lock(RAYO_ACTOR(call)->mutex);
		if (zstr(call->dial_request_id) && !call->dial_request_failed) {
			switch_event_dup(&call->end_event, event);
			RAYO_DESTROY(call);
			RAYO_RELEASE(call); /* decrement ref from creation */
		}
		switch_mutex_unlock(RAYO_ACTOR(call)->mutex);
		RAYO_RELEASE(call); /* decrement this ref */
	}
}

/**
 * Handle call answer event
 * @param rclient the Rayo client
 * @param event the answer event
 */
static void on_call_answer_event(struct rayo_client *rclient, switch_event_t *event)
{
	struct rayo_call *call = RAYO_CALL_LOCATE_BY_ID(switch_event_get_header(event, "Unique-ID"));
	if (call) {
		switch_mutex_lock(RAYO_ACTOR(call)->mutex);
		if (call->rayo_app_started) {
			iks *revent = iks_new_presence("answered", RAYO_NS,
				switch_event_get_header(event, "variable_rayo_call_jid"),
				switch_event_get_header(event, "variable_rayo_dcp_jid"));
			RAYO_SEND_MESSAGE(call, RAYO_JID(rclient), revent);
		} else if (!call->answer_event) {
			/* delay sending this event until the rayo APP has started */
			switch_event_dup(&call->answer_event, event);
		}
		switch_mutex_unlock(RAYO_ACTOR(call)->mutex);
		RAYO_RELEASE(call);
	}
}

/**
 * Handle call ringing event
 * @param rclient the Rayo client
 * @param event the ringing event
 */
static void on_call_ringing_event(struct rayo_client *rclient, switch_event_t *event)
{
	const char *call_direction = switch_event_get_header(event, "Call-Direction");
	if (call_direction && !strcmp(call_direction, "outbound")) {
		struct rayo_call *call = RAYO_CALL_LOCATE_BY_ID(switch_event_get_header(event, "Unique-ID"));
		if (call) {
			switch_mutex_lock(RAYO_ACTOR(call)->mutex);
			if (!call->ringing_sent) {
				iks *revent = iks_new_presence("ringing", RAYO_NS,
					switch_event_get_header(event, "variable_rayo_call_jid"),
					switch_event_get_header(event, "variable_rayo_dcp_jid"));
				call->ringing_sent = 1;
				RAYO_SEND_MESSAGE(call, RAYO_JID(rclient), revent);
			}
			switch_mutex_unlock(RAYO_ACTOR(call)->mutex);
			RAYO_RELEASE(call);
		}
	}
}

/**
 * Handle call bridge event
 * @param rclient the Rayo client
 * @param event the bridge event
 */
static void on_call_bridge_event(struct rayo_client *rclient, switch_event_t *event)
{
	const char *a_uuid = switch_event_get_header(event, "Unique-ID");
	const char *b_uuid = switch_event_get_header(event, "Bridge-B-Unique-ID");
	struct rayo_call *call = RAYO_CALL_LOCATE_BY_ID(a_uuid);
	struct rayo_call *b_call;

	if (call) {
		iks *revent;
		iks *joined;

		call->joined = JOINED_CALL;
		call->joined_id = switch_core_sprintf(RAYO_POOL(call), "xmpp:%s@%s", b_uuid, RAYO_JID(globals.server));

		/* send IQ result to client now. */
		if (call->pending_join_request) {
			iks *request = call->pending_join_request;
			iks *result = iks_new_iq_result(request);
			call->pending_join_request = NULL;
			RAYO_SEND_REPLY(call, iks_find_attrib_soft(request, "from"), result);
			iks_delete(request);
		}

		b_call = RAYO_CALL_LOCATE_BY_ID(b_uuid);
		if (b_call) {
			b_call->joined = JOINED_CALL;
			b_call->joined_id = switch_core_sprintf(RAYO_POOL(b_call), "xmpp:%s@%s", a_uuid, RAYO_JID(globals.server));

			/* send IQ result to client now. */
			if (b_call->pending_join_request) {
				iks *request = b_call->pending_join_request;
				iks *result = iks_new_iq_result(request);
				b_call->pending_join_request = NULL;
				RAYO_SEND_REPLY(call, iks_find_attrib_soft(request, "from"), result);
				iks_delete(request);
			}

			/* send B-leg event */
			revent = iks_new_presence("joined", RAYO_NS, RAYO_JID(b_call), rayo_call_get_dcp_jid(b_call));
			joined = iks_find(revent, "joined");
			iks_insert_attrib_printf(joined, "call-uri", "%s", b_call->joined_id);

			RAYO_SEND_MESSAGE(b_call, rayo_call_get_dcp_jid(b_call), revent);
			RAYO_RELEASE(b_call);
		}

		/* send A-leg event */
		revent = iks_new_presence("joined", RAYO_NS,
			switch_event_get_header(event, "variable_rayo_call_jid"),
			switch_event_get_header(event, "variable_rayo_dcp_jid"));
		joined = iks_find(revent, "joined");
		iks_insert_attrib_printf(joined, "call-uri", "%s", call->joined_id);

		RAYO_SEND_MESSAGE(call, RAYO_JID(rclient), revent);

		RAYO_RELEASE(call);
	}
}

/**
 * Handle call park event - this is fired after unjoining a call
 * @param rclient the Rayo client
 * @param event the unbridge event
 */
static void on_call_park_event(struct rayo_client *rclient, switch_event_t *event)
{
	const char *a_uuid = switch_event_get_header(event, "Unique-ID");
	struct rayo_call *call = RAYO_CALL_LOCATE_BY_ID(a_uuid);

	if (call) {
		if (call->joined) {
			iks *revent;
			iks *unjoined;
			const char *joined_id = call->joined_id;

			call->joined = 0;
			call->joined_id = NULL;

			/* send IQ result to client now. */
			if (call->pending_join_request) {
				iks *request = call->pending_join_request;
				iks *result = iks_new_iq_result(request);
				call->pending_join_request = NULL;
				RAYO_SEND_REPLY(call, iks_find_attrib_soft(request, "from"), result);
				iks_delete(request);
			}

			/* send A-leg event */
			revent = iks_new_presence("unjoined", RAYO_NS,
				switch_event_get_header(event, "variable_rayo_call_jid"),
				switch_event_get_header(event, "variable_rayo_dcp_jid"));
			unjoined = iks_find(revent, "unjoined");
			iks_insert_attrib_printf(unjoined, "call-uri", "%s", joined_id);
			RAYO_SEND_MESSAGE(call, RAYO_JID(rclient), revent);
		}
		RAYO_RELEASE(call);
	}
}

/**
 * Handle call execute application event
 * @param rclient the Rayo client
 * @param event the execute event
 */
static void on_call_execute_event(struct rayo_client *rclient, switch_event_t *event)
{
	struct rayo_call *call = RAYO_CALL_LOCATE_BY_ID(switch_event_get_header(event, "Unique-ID"));
	if (call) {
		switch_log_printf(SWITCH_CHANNEL_UUID_LOG(RAYO_ID(call)), SWITCH_LOG_DEBUG, "Application %s execute\n", switch_event_get_header(event, "Application"));
		RAYO_RELEASE(call);
	}
}

/**
 * Handle call execute application complete event
 * @param rclient the Rayo client
 * @param event the execute complete event
 */
static void on_call_execute_complete_event(struct rayo_client *rclient, switch_event_t *event)
{
	struct rayo_call *call = RAYO_CALL_LOCATE_BY_ID(switch_event_get_header(event, "Unique-ID"));
	if (call) {
		const char *app = switch_event_get_header(event, "Application");
		switch_log_printf(SWITCH_CHANNEL_UUID_LOG(RAYO_ID(call)), SWITCH_LOG_DEBUG, "Application %s execute complete: %s \n",
			app,
			switch_event_get_header(event, "Application-Response"));
		RAYO_RELEASE(call);
	}
}

/**
 * Handle events to deliver to client connection
 * @param rclient the Rayo client connection to receive the event
 * @param event the event.
 */
static void rayo_client_handle_event(struct rayo_client *rclient, switch_event_t *event)
{
	if (event) {
		switch (event->event_id) {
		case SWITCH_EVENT_CHANNEL_ORIGINATE:
			on_call_originate_event(rclient, event);
			break;
		case SWITCH_EVENT_CHANNEL_PROGRESS:
		case SWITCH_EVENT_CHANNEL_PROGRESS_MEDIA:
			on_call_ringing_event(rclient, event);
			break;
		case SWITCH_EVENT_CHANNEL_ANSWER:
			on_call_answer_event(rclient, event);
			break;
		case SWITCH_EVENT_CHANNEL_BRIDGE:
			on_call_bridge_event(rclient, event);
			break;
		case SWITCH_EVENT_CHANNEL_PARK:
			on_call_park_event(rclient, event);
			break;
		case SWITCH_EVENT_CHANNEL_EXECUTE:
			on_call_execute_event(rclient, event);
			break;
		case SWITCH_EVENT_CHANNEL_EXECUTE_COMPLETE:
			on_call_execute_complete_event(rclient, event);
			break;
		default:
			/* don't care */
			break;
		}
	}
}

/**
 * Receives events from FreeSWITCH core and routes them to the proper Rayo client.
 * @param event received from FreeSWITCH core.  It will be destroyed by the core after this function returns.
 */
static void route_call_event(switch_event_t *event)
{
	char *uuid = switch_event_get_header(event, "unique-id");
	char *dcp_jid = switch_event_get_header(event, "variable_rayo_dcp_jid");
	char *event_subclass = switch_event_get_header(event, "Event-Subclass");

	switch_log_printf(SWITCH_CHANNEL_UUID_LOG(uuid), SWITCH_LOG_DEBUG, "got event %s %s\n", switch_event_name(event->event_id), zstr(event_subclass) ? "" : event_subclass);

	/* this event is for a rayo client */
	if (!zstr(dcp_jid)) {
		struct rayo_actor *actor;
		switch_log_printf(SWITCH_CHANNEL_UUID_LOG(uuid), SWITCH_LOG_DEBUG, "%s rayo event %s\n", dcp_jid, switch_event_name(event->event_id));

		actor = RAYO_LOCATE(dcp_jid);
		if (actor && !strcmp(RAT_CLIENT, actor->type)) {
			/* route to client */
			rayo_client_handle_event(RAYO_CLIENT(actor), event);
		} else {
			/* TODO orphaned call... maybe allow events to queue so they can be delivered on reconnect? */
			switch_log_printf(SWITCH_CHANNEL_UUID_LOG(uuid), SWITCH_LOG_DEBUG, "Orphaned call event %s to %s\n", switch_event_name(event->event_id), dcp_jid);
		}
		RAYO_RELEASE(actor);
	}
}

/**
 * Create server.
 * @param domain the domain name
 * @return the domain
 */
static struct rayo_actor *rayo_server_create(const char *domain)
{
	switch_memory_pool_t *pool;
	struct rayo_actor *new_server = NULL;

	switch_core_new_memory_pool(&pool);
	new_server = switch_core_alloc(pool, sizeof(*new_server));
	RAYO_ACTOR_INIT(RAYO_ACTOR(new_server), pool, RAT_SERVER, "", domain, domain, NULL, rayo_server_send);

	return new_server;
}

/**
 * Create an offer for a call
 * @param call the call
 * @param session the session
 * @return the offer
 */
static iks *rayo_create_offer(struct rayo_call *call, switch_core_session_t *session)
{
	switch_channel_t *channel = switch_core_session_get_channel(session);
	switch_caller_profile_t *profile = switch_channel_get_caller_profile(channel);
	iks *presence = iks_new("presence");
	iks *c = iks_insert(presence, "c");
	iks *offer = iks_insert(presence, "offer");
	const char *val;
	char *ver;

	/* <presence> */
	iks_insert_attrib(presence, "from", RAYO_JID(call));

	/* <c> */
	ver = calculate_entity_sha1_ver(&rayo_call_identity, rayo_call_features);
	iks_insert_attrib(c, "xmlns", IKS_NS_XMPP_ENTITY_CAPABILITIES);
	iks_insert_attrib(c, "hash", "sha-1");
	iks_insert_attrib(c, "node", RAYO_CALL_NS);
	iks_insert_attrib(c, "ver", ver);
	free(ver);

	/* <offer> */
	iks_insert_attrib(offer, "xmlns", RAYO_NS);
	if (globals.offer_uri && (val = switch_channel_get_variable(channel, "sip_from_uri"))) {
		/* is a SIP call - pass the URI */
		if (!strchr(val, ':')) {
			iks_insert_attrib_printf(offer, "from", "sip:%s", val);
		} else {
			iks_insert_attrib(offer, "from", val);
		}
	} else {
		/* pass caller ID */
		iks_insert_attrib(offer, "from", profile->caller_id_number);
	}

	if (globals.offer_uri && (val = switch_channel_get_variable(channel, "sip_to_uri"))) {
		/* is a SIP call - pass the URI */
		if (!strchr(val, ':')) {
			iks_insert_attrib_printf(offer, "to", "sip:%s", val);
		} else {
			iks_insert_attrib(offer, "to", val);
		}
	} else {
		/* pass dialed number */
		iks_insert_attrib(offer, "to", profile->destination_number);
	}

	/* add headers to offer */
	{
		switch_event_header_t *var;
		add_header(offer, "from", switch_channel_get_variable(channel, "sip_full_from"));
		add_header(offer, "to", switch_channel_get_variable(channel, "sip_full_to"));
		add_header(offer, "via", switch_channel_get_variable(channel, "sip_full_via"));

		/* add all SIP header variables and (if configured) all other variables */
		for (var = switch_channel_variable_first(channel); var; var = var->next) {
			if (!strncmp("sip_h_", var->name, 6)) {
				add_header(offer, var->name + 6, var->value);
			}
			if (globals.add_variables_to_offer) {
				char var_name[1024];
				snprintf(var_name, 1024, "variable-%s", var->name);
				add_header(offer, var_name, var->value);
			}
		}
		switch_channel_variable_last(channel);
	}

	return presence;
}

/**
 * Monitor rayo call activity - detect idle
 */
static switch_status_t rayo_call_on_read_frame(switch_core_session_t *session, switch_frame_t **frame, switch_io_flag_t flags, int i)
{
	switch_channel_t *channel = switch_core_session_get_channel(session);
	struct rayo_call *call = (struct rayo_call *)switch_channel_get_private(channel, "rayo_call_private");
	if (call) {
		switch_time_t now = switch_micro_time_now();
		switch_time_t idle_start = call->idle_start_time;
		int idle_duration_ms = (now - idle_start) / 1000;
		/* detect idle session (rayo-client has stopped controlling call) and terminate call */
		if (rayo_call_is_joined(call) || rayo_call_is_faxing(call) || RAYO_ACTOR(call)->ref_count > 1) {
			call->idle_start_time = now;
		} else if (idle_duration_ms > globals.max_idle_ms) {
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_WARNING, "Ending abandoned call.  idle_duration_ms = %i ms\n", idle_duration_ms);
			switch_channel_hangup(channel, RAYO_CAUSE_HANGUP);
		}
	}
	return SWITCH_STATUS_SUCCESS;
}

/**
 * @param rclient to check
 * @param offer_filters optional list of username or username@server to match with client JID.
 * @param offer_filter_count
 * @return 1 if client is online and optional filter(s) match the client.  0 otherwise.
 */
static int should_offer_to_client(struct rayo_client *rclient, char **offer_filters, int offer_filter_count)
{
	if (!rclient->availability == PS_ONLINE) {
		return 0;
	}

	if (offer_filter_count == 0) {
		/* online and no filters to match */
		return 1;
	} else {
		/* check if one of the filters matches the client */
		int i;
		const char *client_jid = RAYO_JID(rclient);
		size_t client_jid_len = strlen(client_jid);
		for (i = 0; i < offer_filter_count; i++) {
			char *offer_filter = offer_filters[i];
			if (!zstr(offer_filter)) {
				size_t offer_filter_len = strlen(offer_filter);
				if (strchr(offer_filter, '@')) {
					if (offer_filter_len <= client_jid_len && !strncmp(offer_filter, client_jid, offer_filter_len)) {
						/* username + server match */
						return 1;
					}
				} else if (offer_filter_len < client_jid_len && !strncmp(offer_filter, client_jid, offer_filter_len) && client_jid[offer_filter_len] == '@') {
					/* username match */
					return 1;
				}
			}
		}
	}
	return 0;
}

#define RAYO_USAGE "[client username 1,client username n]"
/**
 * Offer call and park channel
 */
SWITCH_STANDARD_APP(rayo_app)
{
	int ok = 0;
	switch_channel_t *channel = switch_core_session_get_channel(session);
	struct rayo_call *call = RAYO_CALL_LOCATE_BY_ID(switch_core_session_get_uuid(session));
	const char *app = ""; /* optional app to execute */
	const char *app_args = ""; /* app args */

	/* don't need to keep call reference count incremented in session- call is destroyed after all apps finish */
	if (call) {
		RAYO_RELEASE(call);
	}

	/* is outbound call already under control? */
	if (switch_channel_direction(channel) == SWITCH_CALL_DIRECTION_OUTBOUND) {
		const char *origination_args = switch_channel_get_variable(channel, "rayo_origination_args");
		/* check origination args */
		if (!zstr(origination_args)) {
			char *argv[2] = { 0 };
			char *args = switch_core_session_strdup(session, origination_args);
			int argc = switch_separate_string(args, ' ', argv, sizeof(argv) / sizeof(argv[0]));
			if (argc) {
				if (!strcmp("conference", argv[0])) {
					app = "conference";
					app_args = argv[1];
				} else if (!strcmp("bridge", argv[0])) {
					app = "intercept";
					app_args = argv[1];
				} else {
					switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_INFO, "Invalid rayo args: %s\n", data);
					goto done;
				}
			}
		}
		if (!call) {
			/* this scenario can only happen if a call was originated through a mechanism other than <dial> 
			   and then the rayo APP was executed to offer control */
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_INFO, "Outbound call that wasn't created with <dial>, will try to offer control\n");
		}
		ok = 1;
	}

	if (!call) {
		/* offer control */
		switch_hash_index_t *hi = NULL;
		iks *offer = NULL;
		char *clients_to_offer[16] = { 0 };
		int clients_to_offer_count = 0;

		call = rayo_call_create(switch_core_session_get_uuid(session));
		if (!call) {
			/* nothing that can be done to recover... */
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_CRIT, "Failed to create call entity!\n");
			switch_channel_hangup(channel, SWITCH_CAUSE_NORMAL_TEMPORARY_FAILURE);
			return;
		}

		switch_channel_set_variable(switch_core_session_get_channel(session), "rayo_call_jid", RAYO_JID(call));

		offer = rayo_create_offer(call, session);
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "Offering call for Rayo 3PCC\n");

		if (!zstr(data)) {
			char *data_dup = switch_core_session_strdup(session, data);
			clients_to_offer_count = switch_separate_string(data_dup, ',', clients_to_offer, sizeof(clients_to_offer) / sizeof(clients_to_offer[0]));
		}

		/* It is now safe for inbound call to be fully controlled by rayo client */
		if (switch_channel_direction(channel) == SWITCH_CALL_DIRECTION_INBOUND) {
			switch_mutex_lock(RAYO_ACTOR(call)->mutex);
			call->rayo_app_started = 1;
			switch_mutex_unlock(RAYO_ACTOR(call)->mutex);
		}

		/* Offer call to all (or specified) ONLINE clients */
		/* TODO load balance offers so first session doesn't always get offer first? */
		switch_mutex_lock(globals.clients_mutex);
		for (hi = switch_core_hash_first(globals.clients_roster); hi; hi = switch_core_hash_next(&hi)) {
			struct rayo_client *rclient;
			const void *key;
			void *val;
			switch_core_hash_this(hi, &key, NULL, &val);
			rclient = (struct rayo_client *)val;
			switch_assert(rclient);

			/* is session available to take call? */
			if (should_offer_to_client(rclient, clients_to_offer, clients_to_offer_count)) {
				ok = 1;
				switch_core_hash_insert(call->pcps, RAYO_JID(rclient), "1");
				iks_insert_attrib(offer, "to", RAYO_JID(rclient));
				RAYO_SEND_MESSAGE_DUP(call, RAYO_JID(rclient), offer);
			}
		}
		switch_mutex_unlock(globals.clients_mutex);
		iks_delete(offer);

		/* nobody to offer to */
		if (!ok) {
			pause_when_offline();
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_NOTICE, "Rejecting rayo call - there are no online rayo clients to offer call to\n");
			switch_channel_hangup(channel, SWITCH_CAUSE_NORMAL_TEMPORARY_FAILURE);
		}
	}

done:

	if (ok) {
		switch_channel_set_private(switch_core_session_get_channel(session), "rayo_call_private", call);
		switch_channel_set_variable(channel, "hangup_after_bridge", "false");
		switch_channel_set_variable(channel, "transfer_after_bridge", "");
		switch_channel_set_variable(channel, "park_after_bridge", "true");
		switch_channel_set_variable(channel, "hold_hangup_xfer_exten", "park:inline:");
		switch_channel_set_variable(channel, SWITCH_SEND_SILENCE_WHEN_IDLE_VARIABLE, "-1"); /* required so that output mixing works */
		switch_core_event_hook_add_read_frame(session, rayo_call_on_read_frame);

		if (switch_channel_direction(channel) == SWITCH_CALL_DIRECTION_OUTBOUND) {
			/* At this point, this outbound call might already be under control of a rayo client that is waiting for answer before sending
			  commands.  The answered event might have been sent before we are ready to execute commands, so we delayed sending
			  those events if the rayo APP hadn't started yet.  This delay would have only been a few milliseconds.
			*/
			switch_mutex_lock(RAYO_ACTOR(call)->mutex);
			call->rayo_app_started = 1;
			if (call->answer_event) {
				struct rayo_client *rclient = RAYO_CLIENT(RAYO_LOCATE(rayo_call_get_dcp_jid(call)));
				if (rclient) {
					on_call_answer_event(rclient, call->answer_event);
					switch_event_destroy(&call->answer_event);
					RAYO_RELEASE(rclient);
				}
			}
			switch_mutex_unlock(RAYO_ACTOR(call)->mutex);

			/* Outbound calls might have a nested join to another call or conference - do that now */
			if (!zstr(app)) {
				switch_core_session_execute_application(session, app, app_args);
			}
		}

		/* Ready for remote control */
		switch_ivr_park(session, NULL);
	}
}

/**
 * Stream locates client
 */
static struct rayo_actor *xmpp_stream_client_locate(struct xmpp_stream *stream, const char *jid)
{
	struct rayo_actor *actor = NULL;
	if (xmpp_stream_is_s2s(stream)) {
		actor = RAYO_LOCATE(jid);
		if (!actor) {
			/* previously unknown client - add it */
			struct rayo_peer_server *rserver = RAYO_PEER_SERVER(xmpp_stream_get_private(stream));
			actor = RAYO_ACTOR(rayo_client_create(jid, xmpp_stream_get_jid(stream), PS_UNKNOWN, rayo_client_send, rserver));
			RAYO_RETAIN(actor);
		} else if (strcmp(RAT_CLIENT, actor->type)) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "%s, not a client: %s\n", xmpp_stream_get_jid(stream), jid);
			RAYO_RELEASE(actor);
			actor = NULL;
		}
	} else {
		actor = RAYO_ACTOR(xmpp_stream_get_private(stream));
		RAYO_RETAIN(actor);
	}
	return actor;
}

/**
 * Handle new stream creation
 * @param stream the new stream
 */
static void on_xmpp_stream_ready(struct xmpp_stream *stream)
{
	if (xmpp_stream_is_s2s(stream)) {
		if (xmpp_stream_is_incoming(stream)) {
			/* peer server belongs to a s2s inbound stream */
			struct rayo_peer_server *peer = rayo_peer_server_create(xmpp_stream_get_jid(stream));
			if (peer) {
				xmpp_stream_set_private(stream, peer);
			} else {
				/* this went really bad... */
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "failed to create peer server entity!\n");
			}
		} else {
			/* send directed presence to domain */
			iks *presence = iks_new("presence");
			iks *x;
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "sending server presence\n");

			iks_insert_attrib(presence, "from", RAYO_JID(globals.server));
			iks_insert_attrib(presence, "to", xmpp_stream_get_jid(stream));
			x = iks_insert(presence, "show");
			iks_insert_cdata(x, "chat", 4);
			RAYO_SEND_MESSAGE(globals.server, xmpp_stream_get_jid(stream), presence);
		}
	} else {
		/* client belongs to stream */
		struct rayo_client *client = rayo_client_create(xmpp_stream_get_jid(stream), xmpp_stream_get_jid(stream), PS_OFFLINE, rayo_client_send, NULL);
		if (client) {
			xmpp_stream_set_private(stream, client);
		} else {
			/* this went really bad... */
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "failed to create client entity!\n");
		}
	}
}

/**
 * Checks client availability.  If unknown, client presence is probed.
 * @param rclient to check
 */
static void rayo_client_presence_check(struct rayo_client *rclient)
{
	if (rclient->availability == PS_UNKNOWN) {
		/* for now, set online */
		rclient->availability = PS_ONLINE;
	}
}

/**
 * Handle stream stanza
 * @param stream the stream
 * @param stanza the stanza to process
 */
static void on_xmpp_stream_recv(struct xmpp_stream *stream, iks *stanza)
{
	const char *name = iks_name(stanza);
	if (!strcmp("iq", name)) {
		const char *from = iks_find_attrib_soft(stanza, "from");
		struct rayo_actor *actor = xmpp_stream_client_locate(stream, from);
		if (actor) {
			rayo_client_presence_check(RAYO_CLIENT(actor));
			rayo_client_command_recv(RAYO_CLIENT(actor), stanza);
			RAYO_RELEASE(actor);
		}
	} else if (!strcmp("presence", name)) {
		const char *from = iks_find_attrib_soft(stanza, "from");
		struct rayo_actor *actor = xmpp_stream_client_locate(stream, from);
		if (actor) {
			on_client_presence(RAYO_CLIENT(actor), stanza);
			RAYO_RELEASE(actor);
		}
	} else if (!strcmp("message", name)) {
		const char *from = iks_find_attrib_soft(stanza, "from");
		struct rayo_actor *actor = xmpp_stream_client_locate(stream, from);
		if (actor) {
			rayo_client_presence_check(RAYO_CLIENT(actor));
			on_client_message(RAYO_CLIENT(actor), stanza);
			RAYO_RELEASE(actor);
		}
	}
}

/**
 * Handle stream destruction
 */
static void on_xmpp_stream_destroy(struct xmpp_stream *stream)
{
	/* destroy peer server / client associated with this stream */
	void *actor = xmpp_stream_get_private(stream);
	if (actor) {
		RAYO_RELEASE(actor);
		RAYO_DESTROY(actor);
	}
}

/**
 * A command alias
 */
struct rayo_cmd_alias {
	/** number of additional arguments for alias */
	int args;
	/** the alias template */
	const char *cmd;
};

/**
 * Add an alias to an API command
 * @param alias_name
 * @param alias_target
 * @param alias_cmd
 * @param alias_args
 */
static void rayo_add_cmd_alias(const char *alias_name, const char *alias_target, const char *alias_cmd, const char *alias_args)
{
	struct rayo_cmd_alias *alias = switch_core_alloc(globals.pool, sizeof(*alias));
	alias->args = 0;
	if (switch_is_number(alias_args)) {
		alias->args = atoi(alias_args);
		if (alias->args < 0) {
			alias->args = 0;
		}
	}
	alias->cmd = alias_cmd;
	switch_core_hash_insert(globals.cmd_aliases, alias_name, alias);

	/* set up autocomplete of alias */
	if (zstr(alias_target)) {
		alias_target = "all";
	}
	switch_console_set_complete(switch_core_sprintf(globals.pool, "add rayo %s ::rayo::list_%s", alias_name, alias_target));
}

/**
 * Process module XML configuration
 * @param pool memory pool to allocate from
 * @param config_file to use
 * @return SWITCH_STATUS_SUCCESS on successful configuration
 */
static switch_status_t do_config(switch_memory_pool_t *pool, const char *config_file)
{
	switch_xml_t cfg, xml;
	switch_status_t status = SWITCH_STATUS_SUCCESS;

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Configuring module\n");
	if (!(xml = switch_xml_open_cfg(config_file, &cfg, NULL))) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "open of %s failed\n", config_file);
		return SWITCH_STATUS_TERM;
	}

	/* set defaults */
	globals.max_idle_ms = 30000;
	globals.mixer_conf_profile = "sla";
	globals.num_message_threads = 8;
	globals.offer_uri = 1;
	globals.pause_when_offline = 0;
	globals.add_variables_to_offer = 0;

	/* get params */
	{
		switch_xml_t settings = switch_xml_child(cfg, "settings");
		if (settings) {
			switch_xml_t param;
			for (param = switch_xml_child(settings, "param"); param; param = param->next) {
				const char *var = switch_xml_attr_soft(param, "name");
				const char *val = switch_xml_attr_soft(param, "value");
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "param: %s = %s\n", var, val);
				if (!strcasecmp(var, "max-idle-sec")) {
					if (switch_is_number(val)) {
						int max_idle_sec = atoi(val);
						if (max_idle_sec > 0) {
							globals.max_idle_ms = max_idle_sec * 1000;
						}
					}
				} else if (!strcasecmp(var, "mixer-conf-profile")) {
					if (!zstr(val)) {
						globals.mixer_conf_profile = switch_core_strdup(pool, val);
					}
				} else if (!strcasecmp(var, "message-threads")) {
					if (switch_is_number(val)) {
						int num_message_threads = atoi(val);
						if (num_message_threads > 0) {
							globals.num_message_threads = num_message_threads;
						}
					}
				} else if (!strcasecmp(var, "offer-uri")) {
					if (switch_false(val)) {
						globals.offer_uri = 0;
					}
				} else if (!strcasecmp(var, "pause-when-offline")) {
					if (switch_true(val)) {
						globals.pause_when_offline = 1;
					}
				} else if (!strcasecmp(var, "add-variables-to-offer")) {
					if (switch_true(val)) {
						globals.add_variables_to_offer = 1;
					}
				} else {
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "Unsupported param: %s\n", var);
				}
			}
		}
	}

	/* configure dial gateways */
	{
		switch_xml_t dial_gateways = switch_xml_child(cfg, "dial-gateways");

		/* set defaults */
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Setting default dial-gateways\n");
		dial_gateway_add("default", "sofia/gateway/outbound/", 0);
		dial_gateway_add("tel:", "sofia/gateway/outbound/", 4);
		dial_gateway_add("user", "", 0);
		dial_gateway_add("sofia", "", 0);

		if (dial_gateways) {
			switch_xml_t dg;
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Setting configured dial-gateways\n");
			for (dg = switch_xml_child(dial_gateways, "dial-gateway"); dg; dg = dg->next) {
				const char *uri_prefix = switch_xml_attr_soft(dg, "uriprefix");
				const char *dial_prefix = switch_xml_attr_soft(dg, "dialprefix");
				const char *strip_str = switch_xml_attr_soft(dg, "strip");
				int strip = 0;

				if (!zstr(strip_str) && switch_is_number(strip_str)) {
					strip = atoi(strip_str);
					if (strip < 0) {
						strip = 0;
					}
				}
				if (!zstr(uri_prefix)) {
					dial_gateway_add(uri_prefix, dial_prefix, strip);
				}
			}
		}
	}

	/* configure domain */
	{
		switch_xml_t domain = switch_xml_child(cfg, "domain");
		if (domain) {
			switch_xml_t l;
			const char *shared_secret = switch_xml_attr_soft(domain, "shared-secret");
			const char *name = switch_xml_attr_soft(domain, "name");
			const char *cert = switch_xml_attr_soft(domain, "cert");
			const char *key = switch_xml_attr_soft(domain, "key");
			if (zstr(name)) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Missing <domain name=\"... failed to configure rayo server\n");
				status = SWITCH_STATUS_FALSE;
				goto done;
			}
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "Rayo domain set to %s\n", name);

			if (zstr(shared_secret)) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "Missing shared secret for %s domain.  Server dialback will not work\n", name);
			}

			globals.xmpp_context = xmpp_stream_context_create(name, shared_secret, on_xmpp_stream_ready, on_xmpp_stream_recv, on_xmpp_stream_destroy);
			globals.server = rayo_server_create(name);

			/* set up TLS */
			if (!zstr(cert)) {
				xmpp_stream_context_add_cert(globals.xmpp_context, cert);
			}
			if (!zstr(key)) {
				xmpp_stream_context_add_key(globals.xmpp_context, key);
			}

			/* configure authorized users for this domain */
			l = switch_xml_child(domain, "users");
			if (l) {
				switch_xml_t u;
				for (u = switch_xml_child(l, "user"); u; u = u->next) {
					const char *user = switch_xml_attr_soft(u, "name");
					const char *password = switch_xml_attr_soft(u, "password");
					xmpp_stream_context_add_user(globals.xmpp_context, user, password);
				}
			}

			/* get listeners for this domain */
			for (l = switch_xml_child(domain, "listen"); l; l = l->next) {
				const char *address = switch_xml_attr_soft(l, "address");
				const char *port = switch_xml_attr_soft(l, "port");
				const char *type = switch_xml_attr_soft(l, "type");
				const char *acl = switch_xml_attr_soft(l, "acl");
				int is_s2s = 0;
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "%s listener: %s:%s\n", type, address, port);
				is_s2s = !strcmp("s2s", type);
				if (!is_s2s && strcmp("c2s", type)) {
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "Type must be \"c2s\" or \"s2s\"!\n");
					status = SWITCH_STATUS_FALSE;
					goto done;
				}
				if (zstr(address)) {
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "Missing address!\n");
					status = SWITCH_STATUS_FALSE;
					goto done;
				}
				if (!zstr(port) && !switch_is_number(port)) {
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "Port must be an integer!\n");
					status = SWITCH_STATUS_FALSE;
					goto done;
				}
				if (xmpp_stream_context_listen(globals.xmpp_context, address, atoi(port), is_s2s, acl) != SWITCH_STATUS_SUCCESS) {
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "Failed to create %s listener: %s:%s\n", type, address, port);
 				}
			}

			/* get outbound server connections */
			for (l = switch_xml_child(domain, "connect"); l; l = l->next) {
				const char *domain = switch_xml_attr_soft(l, "domain");
				const char *address = switch_xml_attr_soft(l, "address");
				const char *port = switch_xml_attr_soft(l, "port");
				if (!zstr(port) && !switch_is_number(port)) {
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "Outbound server port must be an integer!\n");
					status = SWITCH_STATUS_FALSE;
					goto done;
				}
				if (zstr(address) && zstr(domain)) {
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "Missing outbound server address!\n");
					status = SWITCH_STATUS_FALSE;
					goto done;
				}
				xmpp_stream_context_connect(globals.xmpp_context, domain, address, atoi(port));
			}
		}
	}

	/* get aliases */
	{
		switch_xml_t aliases = switch_xml_child(cfg, "aliases");
		if (aliases) {
			switch_xml_t alias;
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Setting configured aliases\n");
			for (alias = switch_xml_child(aliases, "alias"); alias; alias = alias->next) {
				const char *alias_name = switch_xml_attr_soft(alias, "name");
				const char *alias_target = switch_xml_attr_soft(alias, "target");
				const char *alias_args = switch_xml_attr_soft(alias, "args");
				if (!zstr(alias_name) && !zstr(alias->txt)) {
					rayo_add_cmd_alias(alias_name, switch_core_strdup(pool, alias_target), switch_core_strdup(pool, alias->txt), switch_core_strdup(pool, alias_args));
				}
			}
		}
	}

done:
	switch_xml_free(xml);

	return status;
}

/**
 * Dump rayo actor stats
 */
static void rayo_actor_dump(struct rayo_actor *actor, switch_stream_handle_t *stream)
{
	if (!strcmp(RAT_CLIENT, actor->type)) {
		stream->write_function(stream, "TYPE='%s',SUBTYPE='%s',ID='%s',JID='%s',DOMAIN='%s',REFS=%i,STATUS='%s'", actor->type, actor->subtype, actor->id, RAYO_JID(actor), RAYO_DOMAIN(actor), actor->ref_count, presence_status_to_string(RAYO_CLIENT(actor)->availability));
	} else {
		stream->write_function(stream, "TYPE='%s',SUBTYPE='%s',ID='%s',JID='%s',DOMAIN='%s',REFS=%i", actor->type, actor->subtype, actor->id, RAYO_JID(actor), RAYO_DOMAIN(actor), actor->ref_count);
	}
}

/**
 * Dump rayo actors
 */
static int dump_api(const char *cmd, switch_stream_handle_t *stream)
{
	switch_hash_index_t *hi;
	if (!zstr(cmd)) {
		return 0;
	}

	stream->write_function(stream, "\nENTITIES\n");
	switch_mutex_lock(globals.actors_mutex);
	for (hi = switch_core_hash_first(globals.actors); hi; hi = switch_core_hash_next(&hi)) {
		struct rayo_actor *actor = NULL;
		const void *key;
		void *val;
		switch_core_hash_this(hi, &key, NULL, &val);
		actor = (struct rayo_actor *)val;
		switch_assert(actor);
		stream->write_function(stream, "        ");
		rayo_actor_dump(actor, stream);
		stream->write_function(stream, "\n");
	}

	for (hi = switch_core_hash_first(globals.destroy_actors); hi; hi = switch_core_hash_next(&hi)) {
		struct rayo_actor *actor = NULL;
		const void *key;
		void *val;
		switch_core_hash_this(hi, &key, NULL, &val);
		actor = (struct rayo_actor *)val;
		switch_assert(actor);
		stream->write_function(stream, "(DEAD)  ");
		rayo_actor_dump(actor, stream);
		stream->write_function(stream, "\n");
	}
	switch_mutex_unlock(globals.actors_mutex);

	xmpp_stream_context_dump(globals.xmpp_context, stream);

	return 1;
}

/**
 * Process response to console command_api
 */
void rayo_console_client_send(struct rayo_actor *actor, struct rayo_message *msg)
{
	iks *response = msg->payload;

	if (response) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CONSOLE, "\nRECV: from %s, %s\n", msg->from_jid, iks_string(iks_stack(response), response));
	} else {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CONSOLE, "\nRECV: (null) from %s\n", msg->from_jid);
	}
}

/**
 * Create a new Rayo console client
 * @return the new client or NULL
 */
static struct rayo_client *rayo_console_client_create(void)
{
	struct rayo_client *client = NULL;
	char *jid = NULL;
	char id[SWITCH_UUID_FORMATTED_LENGTH + 1] = { 0 };
	switch_uuid_str(id, sizeof(id));
	jid = switch_mprintf("%s@%s/console", id, RAYO_JID(globals.server));
	client = rayo_client_create(jid, NULL, PS_OFFLINE, rayo_console_client_send, NULL);
	free(jid);
	return client;
}

/**
 * Send command from console
 */
static void send_console_command(struct rayo_client *client, const char *to, const char *command_str)
{
	iks *command = NULL;
	iksparser *p = iks_dom_new(&command);

	if (iks_parse(p, command_str, 0, 1) == IKS_OK && command) {
		char *str;
		iks *iq = NULL;

		/* is command already wrapped in IQ? */
		if (!strcmp(iks_name(command), "iq")) {
			/* command already IQ */
			iq = command;
		} else {
			/* create IQ to wrap command */
			iq = iks_new_within("iq", iks_stack(command));
			iks_insert_node(iq, command);
		}

		/* fill in command attribs */
		iks_insert_attrib(iq, "to", to);
		if (!iks_find_attrib(iq, "type")) {
			iks_insert_attrib(iq, "type", "set");
		}
		if (!iks_find_attrib(iq, "id")) {
			iks_insert_attrib_printf(iq, "id", "console-%i", RAYO_SEQ_NEXT(client));
		}
		iks_insert_attrib(iq, "from", RAYO_JID(client));

		/* send command */
		str = iks_string(iks_stack(iq), iq);
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CONSOLE, "\nSEND: to %s, %s\n", to, str);
		rayo_client_command_recv(client, iq);
		iks_delete(command);
	} else {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CONSOLE, "bad request xml\n");
	}
	iks_parser_delete(p);
}

/**
 * Send command to rayo actor
 */
static int command_api(char *cmd, switch_stream_handle_t *stream)
{
	char *argv[2] = { 0 };
	if (!zstr(cmd)) {
		int argc = switch_separate_string(cmd, ' ', argv, sizeof(argv) / sizeof(argv[0]));
		if (argc != 2) {
			return 0;
		}
	} else {
		return 0;
	}

	/* send command */
	send_console_command(globals.console, argv[0], argv[1]);
	stream->write_function(stream, "+OK\n");

	return 1;
}

/**
 * Send command to rayo actor
 */
static int alias_api(struct rayo_cmd_alias *alias, char *args, switch_stream_handle_t *stream)
{
	char *argv[10] = { 0 };
	int argc, i;
	char *cmd;
	char *jid;

	if (zstr(alias->cmd)) {
		stream->write_function(stream, "-ERR missing alias template.  Check configuration.\n");
	}

	if (zstr(args)) {
		stream->write_function(stream, "-ERR no args\n");
		return 1;
	}

	/* check args */
	argc = switch_separate_string(args, ' ', argv, sizeof(argv) / sizeof(argv[0]));
	if (argc != alias->args + 1) {
		stream->write_function(stream, "-ERR wrong number of args (%i/%i)\n", argc, alias->args + 1);
		return 1;
	}

	jid = argv[0];

	/* build command from args */
	cmd = strdup(alias->cmd);
	for (i = 1; i < argc; i++) {
		char *cmd_new;
		char to_replace[4] = { 0 };
		sprintf(to_replace, "$%i", i);
		cmd_new = switch_string_replace(cmd, to_replace, argv[i]);
		free(cmd);
		cmd = cmd_new;
	}

	/* send command */
	send_console_command(globals.console, jid, cmd);
	stream->write_function(stream, "+OK\n");
	free(cmd);

	return 1;
}

/**
 * Send message from console
 */
static void send_console_message(struct rayo_client *client, const char *to, const char *type, const char *message_str)
{
	iks *message = NULL, *x;
	message = iks_new("message");
	iks_insert_attrib(message, "to", to);
	iks_insert_attrib(message, "from", RAYO_JID(client));
	iks_insert_attrib_printf(message, "id", "console-%i", RAYO_SEQ_NEXT(client));
	iks_insert_attrib(message, "type", type);
	x = iks_insert(message, "body");
	iks_insert_cdata(x, message_str, strlen(message_str));
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CONSOLE, "\nSEND: to %s, %s\n", to, iks_string(iks_stack(message), message));
	RAYO_SEND_MESSAGE(client, to, message);
}

/**
 * Send message to rayo actor
 */
static int message_api(char *cmd, switch_stream_handle_t *stream)
{
	char *argv[3] = { 0 };
	if (!zstr(cmd)) {
		int argc = switch_separate_string(cmd, ' ', argv, sizeof(argv) / sizeof(argv[0]));
		if (argc != 3) {
			return 0;
		}
	} else {
		return 0;
	}

	/* send message */
	send_console_message(globals.console, argv[0], argv[1], argv[2]);
	stream->write_function(stream, "+OK\n");

	return 1;
}

/**
 * Send presence from console
 */
static void send_console_presence(struct rayo_client *client, const char *to, int is_online)
{
	iks *presence = NULL, *x;
	presence = iks_new("presence");
	iks_insert_attrib(presence, "to", to);
	iks_insert_attrib(presence, "from", RAYO_JID(client));
	iks_insert_attrib_printf(presence, "id", "console-%i", RAYO_SEQ_NEXT(client));
	if (!is_online) {
		iks_insert_attrib(presence, "type", "unavailable");
	}
	x = iks_insert(presence, "show");
	iks_insert_cdata(x, is_online ? "chat" : "dnd", 0);
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CONSOLE, "\nSEND: to %s, %s\n", to, iks_string(iks_stack(presence), presence));
	RAYO_SEND_MESSAGE(client, to, presence);
}

/**
 * Send console presence
 */
static int presence_api(char *cmd, switch_stream_handle_t *stream)
{
	int is_online = 0;
	char *argv[2] = { 0 };
	if (!zstr(cmd)) {
		int argc = switch_separate_string(cmd, ' ', argv, sizeof(argv) / sizeof(argv[0]));
		if (argc != 2) {
			return 0;
		}
	} else {
		return 0;
	}

	if (!strcmp("online", argv[1])) {
		is_online = 1;
	} else if (strcmp("offline", argv[1])) {
		return 0;
	}

	/* send presence */
	send_console_presence(globals.console, argv[0], is_online);
	stream->write_function(stream, "+OK\n");
	return 1;
}

#define RAYO_API_SYNTAX "status | (<alias> <jid>) | (cmd <jid> <command>) | (msg <jid> <message text>) | (presence <jid> <online|offline>)"
SWITCH_STANDARD_API(rayo_api)
{
	struct rayo_cmd_alias *alias;
	char *cmd_dup = NULL;
	char *argv[2] = { 0 };
	int success = 0;

	if (zstr(cmd) ) {
		goto done;
	}

	cmd_dup = strdup(cmd);
	switch_separate_string(cmd_dup, ' ', argv, sizeof(argv) / sizeof(argv[0]));

	/* check if a command alias */
	alias = switch_core_hash_find(globals.cmd_aliases, argv[0]);

	if (alias) {
		success = alias_api(alias, argv[1], stream);
	} else if (!strcmp("cmd", argv[0])) {
		success = command_api(argv[1], stream);
	} else if (!strcmp("status", argv[0])) {
		success = dump_api(argv[1], stream);
	} else if (!strcmp("msg", argv[0])) {
		success = message_api(argv[1], stream);
	} else if (!strcmp("presence", argv[0])) {
		success = presence_api(argv[1], stream);
	}

done:
	if (!success) {
		stream->write_function(stream, "-ERR: USAGE %s\n", RAYO_API_SYNTAX);
	}

	switch_safe_free(cmd_dup);

	return SWITCH_STATUS_SUCCESS;
}

/**
 * Console auto-completion for actors given validation function
 */
static switch_status_t list_actors(const char *line, const char *cursor, switch_console_callback_match_t **matches, rayo_actor_match_fn match)
{
	switch_hash_index_t *hi;
	void *val;
	const void *vvar;
	switch_console_callback_match_t *my_matches = NULL;
	switch_status_t status = SWITCH_STATUS_FALSE;
	struct rayo_actor *actor;

	switch_mutex_lock(globals.actors_mutex);
	for (hi = switch_core_hash_first(globals.actors); hi; hi = switch_core_hash_next(&hi)) {
		switch_core_hash_this(hi, &vvar, NULL, &val);

		actor = (struct rayo_actor *) val;
		if (match(actor)) {
			switch_console_push_match(&my_matches, (const char *) vvar);
		}
	}
	switch_mutex_unlock(globals.actors_mutex);

	if (my_matches) {
		*matches = my_matches;
		status = SWITCH_STATUS_SUCCESS;
	}

	return status;
}

/**
 * @return true if internal actor
 */
static switch_bool_t is_internal_actor(struct rayo_actor *actor)
{
	return strcmp(RAT_CLIENT, actor->type) && strcmp(RAT_PEER_SERVER, actor->type);
}

/**
 * Console auto-completion for all internal actors
 */
static switch_status_t list_internal(const char *line, const char *cursor, switch_console_callback_match_t **matches)
{
	return list_actors(line, cursor, matches, is_internal_actor);
}

/**
 * @return true if external actor
 */
static switch_bool_t is_external_actor(struct rayo_actor *actor)
{
	return !strcmp(RAT_CLIENT, actor->type) || !strcmp(RAT_PEER_SERVER, actor->type);
}

/**
 * Console auto-completion for all external actors
 */
static switch_status_t list_external(const char *line, const char *cursor, switch_console_callback_match_t **matches)
{
	return list_actors(line, cursor, matches, is_external_actor);
}

/**
 * @return true
 */
static switch_bool_t is_any_actor(struct rayo_actor *actor)
{
	return SWITCH_TRUE;
}

/**
 * Console auto-completion for all actors
 */
static switch_status_t list_all(const char *line, const char *cursor, switch_console_callback_match_t **matches)
{
	return list_actors(line, cursor, matches, is_any_actor);
}

/**
 * @return true if a server
 */
static switch_bool_t is_server_actor(struct rayo_actor *actor)
{
	return !strcmp(RAT_SERVER, actor->type);
}

/**
 * Console auto-completion for all servers
 */
static switch_status_t list_server(const char *line, const char *cursor, switch_console_callback_match_t **matches)
{
	return list_actors(line, cursor, matches, is_server_actor);
}

/**
 * @return true if a call
 */
static switch_bool_t is_call_actor(struct rayo_actor *actor)
{
	return !strcmp(RAT_CALL, actor->type);
}

/**
 * Console auto-completion for all calls
 */
static switch_status_t list_call(const char *line, const char *cursor, switch_console_callback_match_t **matches)
{
	return list_actors(line, cursor, matches, is_call_actor);
}

/**
 * @return true if a component
 */
switch_bool_t is_component_actor(struct rayo_actor *actor)
{
	return !strncmp(RAT_COMPONENT, actor->type, strlen(RAT_COMPONENT));
}

/**
 * Console auto-completion for all components
 */
static switch_status_t list_component(const char *line, const char *cursor, switch_console_callback_match_t **matches)
{
	return list_actors(line, cursor, matches, is_component_actor);
}

/**
 * @return true if a record component
 */
static switch_bool_t is_record_actor(struct rayo_actor *actor)
{
	return is_component_actor(actor) && !strcmp(actor->subtype, "record");
}

/**
 * Console auto-completion for all components
 */
static switch_status_t list_record(const char *line, const char *cursor, switch_console_callback_match_t **matches)
{
	return list_actors(line, cursor, matches, is_record_actor);
}

/**
 * @return true if an output component
 */
static switch_bool_t is_output_actor(struct rayo_actor *actor)
{
	return is_component_actor(actor) && !strcmp(actor->subtype, "output");
}

/**
 * Console auto-completion for all components
 */
static switch_status_t list_output(const char *line, const char *cursor, switch_console_callback_match_t **matches)
{
	return list_actors(line, cursor, matches, is_output_actor);
}

/**
 * @return true if an input component
 */
static switch_bool_t is_input_actor(struct rayo_actor *actor)
{
	return is_component_actor(actor) && !strcmp(actor->subtype, "input");
}

/**
 * Console auto-completion for all components
 */
static switch_status_t list_input(const char *line, const char *cursor, switch_console_callback_match_t **matches)
{
	return list_actors(line, cursor, matches, is_input_actor);
}

/**
 * Load module
 */
SWITCH_MODULE_LOAD_FUNCTION(mod_rayo_load)
{
	switch_api_interface_t *api_interface;
	switch_application_interface_t *app_interface;

	*module_interface = switch_loadable_module_create_module_interface(pool, modname);

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Loading module\n");

	memset(&globals, 0, sizeof(globals));
	globals.pool = pool;
	switch_core_hash_init(&globals.command_handlers);
	switch_core_hash_init(&globals.event_handlers);
	switch_core_hash_init(&globals.clients_roster);
	switch_mutex_init(&globals.clients_mutex, SWITCH_MUTEX_NESTED, pool);
	switch_core_hash_init(&globals.actors);
	switch_core_hash_init(&globals.destroy_actors);
	switch_core_hash_init(&globals.actors_by_id);
	switch_mutex_init(&globals.actors_mutex, SWITCH_MUTEX_NESTED, pool);
	switch_core_hash_init(&globals.dial_gateways);
	switch_mutex_init(&globals.dial_gateways_mutex, SWITCH_MUTEX_NESTED, pool);
	switch_core_hash_init(&globals.cmd_aliases);
	switch_thread_rwlock_create(&globals.shutdown_rwlock, pool);
	switch_queue_create(&globals.msg_queue, 25000, pool);
	globals.offline_logged = 1;

	/* server commands */
	rayo_actor_command_handler_add(RAT_SERVER, "", "get:"IKS_NS_XMPP_PING":ping", on_iq_xmpp_ping);
	rayo_actor_command_handler_add(RAT_SERVER, "", "get:"IKS_NS_XMPP_DISCO":query", on_iq_get_xmpp_disco);
	rayo_actor_command_handler_add(RAT_SERVER, "", "set:"RAYO_NS":dial", on_rayo_dial);

	/* Rayo call commands */
	rayo_actor_command_handler_add(RAT_CALL, "", "set:"RAYO_NS":accept", on_rayo_accept);
	rayo_actor_command_handler_add(RAT_CALL, "", "set:"RAYO_NS":answer", on_rayo_answer);
	rayo_actor_command_handler_add(RAT_CALL, "", "set:"RAYO_NS":redirect", on_rayo_redirect);
	rayo_actor_command_handler_add(RAT_CALL, "", "set:"RAYO_NS":reject", on_rayo_hangup); /* handles both reject and hangup */
	rayo_actor_command_handler_add(RAT_CALL, "", "set:"RAYO_NS":hangup", on_rayo_hangup); /* handles both reject and hangup */
	rayo_actor_command_handler_add(RAT_CALL, "", "set:"RAYO_NS":join", on_rayo_join);
	rayo_actor_command_handler_add(RAT_CALL, "", "set:"RAYO_NS":unjoin", on_rayo_unjoin);

	switch_event_bind(modname, SWITCH_EVENT_CHANNEL_ORIGINATE, NULL, route_call_event, NULL);
	switch_event_bind(modname, SWITCH_EVENT_CHANNEL_PROGRESS_MEDIA, NULL, route_call_event, NULL);
	switch_event_bind(modname, SWITCH_EVENT_CHANNEL_PROGRESS, NULL, route_call_event, NULL);
	switch_event_bind(modname, SWITCH_EVENT_CHANNEL_ANSWER, NULL, route_call_event, NULL);
	switch_event_bind(modname, SWITCH_EVENT_CHANNEL_BRIDGE, NULL, route_call_event, NULL);
	switch_event_bind(modname, SWITCH_EVENT_CHANNEL_PARK, NULL, route_call_event, NULL);
	switch_event_bind(modname, SWITCH_EVENT_CHANNEL_EXECUTE, NULL, route_call_event, NULL);
	switch_event_bind(modname, SWITCH_EVENT_CHANNEL_EXECUTE_COMPLETE, NULL, route_call_event, NULL);

	switch_event_bind(modname, SWITCH_EVENT_CHANNEL_DESTROY, NULL, on_call_end_event, NULL);

	switch_event_bind(modname, SWITCH_EVENT_CUSTOM, "conference::maintenance", route_mixer_event, NULL);

	SWITCH_ADD_APP(app_interface, "rayo", "Offer call control to Rayo client(s)", "", rayo_app, RAYO_USAGE, SAF_SUPPORT_NOMEDIA);
	SWITCH_ADD_API(api_interface, "rayo", "Query rayo status", rayo_api, RAYO_API_SYNTAX);

	/* set up rayo components */
	if (rayo_components_load(module_interface, pool, RAYO_CONFIG_FILE) != SWITCH_STATUS_SUCCESS) {
		return SWITCH_STATUS_TERM;
	}

	/* configure / open sockets */
	if(do_config(globals.pool, RAYO_CONFIG_FILE) != SWITCH_STATUS_SUCCESS) {
		return SWITCH_STATUS_TERM;
	}

	/* start up message threads */
	{
		int i;
		for (i = 0; i < globals.num_message_threads; i++) {
			start_deliver_message_thread(pool);
		}
	}

	/* create admin client */
	globals.console = rayo_console_client_create();
	if (!globals.console) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Failed to create console client entity!\n");
		return SWITCH_STATUS_TERM;
	}

	switch_console_set_complete("add rayo status");
	switch_console_set_complete("add rayo msg ::rayo::list_all");
	switch_console_set_complete("add rayo msg ::rayo::list_all chat");
	switch_console_set_complete("add rayo msg ::rayo::list_all groupchat");
	switch_console_set_complete("add rayo msg ::rayo::list_all headline");
	switch_console_set_complete("add rayo msg ::rayo::list_all normal");
	switch_console_set_complete("add rayo presence ::rayo::list_server online");
	switch_console_set_complete("add rayo presence ::rayo::list_server offline");
	switch_console_add_complete_func("::rayo::list_all", list_all);
	switch_console_add_complete_func("::rayo::list_internal", list_internal);
	switch_console_add_complete_func("::rayo::list_external", list_external);
	switch_console_add_complete_func("::rayo::list_server", list_server);
	switch_console_add_complete_func("::rayo::list_call", list_call);
	switch_console_add_complete_func("::rayo::list_component", list_component);
	switch_console_add_complete_func("::rayo::list_record", list_record);
	switch_console_add_complete_func("::rayo::list_output", list_output);
	switch_console_add_complete_func("::rayo::list_input", list_input);

	return SWITCH_STATUS_SUCCESS;
}

/**
 * Shutdown module.  Notifies threads to stop.
 */
SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_rayo_shutdown)
{
	switch_console_del_complete_func("::rayo::list_all");
	switch_console_del_complete_func("::rayo::list_internal");
	switch_console_del_complete_func("::rayo::list_external");
	switch_console_del_complete_func("::rayo::list_server");
	switch_console_del_complete_func("::rayo::list_call");
	switch_console_del_complete_func("::rayo::list_component");
	switch_console_del_complete_func("::rayo::list_record");
	switch_console_del_complete_func("::rayo::list_output");
	switch_console_del_complete_func("::rayo::list_input");
	switch_console_set_complete("del rayo");

	/* stop XMPP streams */
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Waiting for XMPP threads to stop\n");
	xmpp_stream_context_destroy(globals.xmpp_context);

	/* stop message threads */
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Waiting for message threads to stop\n");
	stop_deliver_message_threads();

	if (globals.console) {
		RAYO_RELEASE(globals.console);
		RAYO_DESTROY(globals.console);
		globals.console = NULL;
	}

	if (globals.server) {
		RAYO_RELEASE(globals.server);
		RAYO_DESTROY(globals.server);
		globals.server = NULL;
	}

	rayo_components_shutdown();

	/* cleanup module */
	switch_event_unbind_callback(route_call_event);
	switch_event_unbind_callback(on_call_end_event);
	switch_event_unbind_callback(route_mixer_event);

	switch_core_hash_destroy(&globals.command_handlers);
	switch_core_hash_destroy(&globals.event_handlers);
	switch_core_hash_destroy(&globals.clients_roster);
	switch_core_hash_destroy(&globals.actors);
	switch_core_hash_destroy(&globals.destroy_actors);
	switch_core_hash_destroy(&globals.actors_by_id);
	switch_core_hash_destroy(&globals.dial_gateways);
	switch_core_hash_destroy(&globals.cmd_aliases);

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Module shutdown\n");

	return SWITCH_STATUS_SUCCESS;
}

/**
 * Checks status of connected clients
 */
SWITCH_MODULE_RUNTIME_FUNCTION(mod_rayo_runtime)
{
	if (globals.pause_when_offline) {
		switch_thread_rwlock_rdlock(globals.shutdown_rwlock);
		while (!globals.shutdown) {
			switch_sleep(1000 * 1000); /* 1 second */
			pause_when_offline();
		}
		switch_thread_rwlock_unlock(globals.shutdown_rwlock);
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Runtime thread is done\n");
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
