/*
 * mod_rayo for FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
 * Copyright (C) 2013, Grasshopper
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
SWITCH_MODULE_DEFINITION(mod_rayo, mod_rayo_load, mod_rayo_shutdown, NULL);

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

#define RAYO_SIP_REQUEST_HEADER "sip_r_"
#define RAYO_SIP_RESPONSE_HEADER "sip_rh_"
#define RAYO_SIP_PROVISIONAL_RESPONSE_HEADER "sip_ph_"
#define RAYO_SIP_BYE_RESPONSE_HEADER "sip_bye_h_"

#define RAYO_CONFIG_FILE "rayo.conf"

struct rayo_actor;
struct rayo_client;
struct rayo_call;

#define rayo_call_get_uuid(call) RAYO_ID(call)

/**
 * Function pointer wrapper for the handlers hash
 */
struct rayo_xmpp_handler {
	enum rayo_actor_type from_type;
	const char *from_subtype;
	enum rayo_actor_type to_type;
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
	/** true if superuser */
	int is_admin;
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
	/** true if joined */
	int joined;
	/** set if response needs to be sent to IQ request */
	const char *dial_id;
	/** channel destroy event */
	switch_event_t *end_event;
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
	/** JID of subscriber */
	const char *jid;
	/** Number of controlled parties in mixer */
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
	/** console command aliases */
	switch_hash_t *cmd_aliases;
	/** global console */
	struct rayo_client *console;
	/** XMPP context */
	struct xmpp_stream_context *xmpp_context;
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

static struct rayo_message *rayo_call_send(struct rayo_actor *client, struct rayo_actor *call, struct rayo_message *msg, const char *file, int line);
static struct rayo_message *rayo_server_send(struct rayo_actor *client, struct rayo_actor *server, struct rayo_message *msg, const char *file, int line);
static struct rayo_message *rayo_mixer_send(struct rayo_actor *client, struct rayo_actor *mixer, struct rayo_message *msg, const char *file, int line);
static struct rayo_message *rayo_component_send(struct rayo_actor *client, struct rayo_actor *component, struct rayo_message *msg, const char *file, int line);
static struct rayo_message *rayo_client_send(struct rayo_actor *from, struct rayo_actor *client, struct rayo_message *msg, const char *file, int line);
static struct rayo_message *rayo_console_client_send(struct rayo_actor *from, struct rayo_actor *client, struct rayo_message *msg, const char *file, int line);

static void on_client_presence(struct rayo_client *rclient, iks *node);


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
		case PS_UNKNOWN:
		default: return "UNKNOWN";
	}
	return "UNKNOWN";
}

/**
 * Convert Rayo actor type to string
 * @param type the Rayo actor type
 * @return the string value of type or "UNKNOWN"
 */
static const char *rayo_actor_type_to_string(enum rayo_actor_type type)
{
	switch(type) {
		case RAT_PEER_SERVER: return "PEER_SERVER";
		case RAT_CLIENT: return "CLIENT";
		case RAT_CALL: return "CALL";
		case RAT_CALL_COMPONENT: return "CALL_COMPONENT";
		case RAT_MIXER: return "MIXER";
		case RAT_MIXER_COMPONENT: return "MIXER_COMPONENT";
		case RAT_SERVER: return "SERVER";
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
	for (hi = switch_core_hash_first(globals.dial_gateways); hi; hi = switch_core_hash_next(hi)) {
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
	snprintf(full_name, sizeof(full_name) - 1, "%i:%s:%s", handler->to_type, handler->to_subtype, name);
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
void rayo_actor_command_handler_add(enum rayo_actor_type type, const char *subtype, const char *name, rayo_actor_xmpp_handler fn)
{
	struct rayo_xmpp_handler *handler = switch_core_alloc(globals.pool, sizeof (*handler));
	handler->to_type = type;
	handler->to_subtype = zstr(subtype) ? "" : switch_core_strdup(globals.pool, subtype);
	handler->fn = fn;
	rayo_command_handler_add(name, handler);
}

/**
 * Get command handler function from hash
 * @param hash the hash to search
 * @param iq
 * @return the command handler function or NULL
 */
rayo_actor_xmpp_handler rayo_actor_command_handler_find(struct rayo_actor *actor, iks *iq)
{
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

	snprintf(full_name, sizeof(full_name) - 1, "%i:%s:%s:%s:%s", actor->type, actor->subtype, iq_type, namespace, name);
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
	snprintf(full_name, sizeof(full_name) - 1, "%i:%s:%i:%s:%s", handler->from_type, handler->from_subtype, handler->to_type, handler->to_subtype, name);
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
void rayo_actor_event_handler_add(enum rayo_actor_type from_type, const char *from_subtype, enum rayo_actor_type to_type, const char *to_subtype, const char *name, rayo_actor_xmpp_handler fn)
{
	struct rayo_xmpp_handler *handler = switch_core_alloc(globals.pool, sizeof (*handler));
	handler->from_type = from_type;
	handler->from_subtype = zstr(from_subtype) ? "" : switch_core_strdup(globals.pool, from_subtype);
	handler->to_type = to_type;
	handler->to_subtype = zstr(to_subtype) ? "" : switch_core_strdup(globals.pool, to_subtype);
	handler->fn = fn;
	rayo_event_handler_add(name, handler);
}

/**
 * Get event handler function from hash
 * @param from the event source
 * @param actor the event destination
 * @param presence the event
 * @return the event handler function or NULL
 */
rayo_actor_xmpp_handler rayo_actor_event_handler_find(struct rayo_actor *from, struct rayo_actor *actor, iks *presence)
{
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
		snprintf(full_name, sizeof(full_name) - 1, "%i:%s:%i:%s:%s:%s:%s", from->type, from->subtype, actor->type, actor->subtype, presence_type, event_namespace, event_name);
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "%s => %s, looking for %s event handler\n", RAYO_JID(from), RAYO_JID(actor), full_name);
		handler = (struct rayo_xmpp_handler *)switch_core_hash_find(globals.event_handlers, full_name);
		if (handler) {
			return handler->fn;
		}
	} else {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "%s => %s, event missing child element\n", RAYO_JID(from), RAYO_JID(actor));
	}
	return NULL;
}

/**
 * Create a new xml message for delivery to an actor.
 * @param xml to create message from.  This value will be freed upon message delivery.
 * @return the message
 */
struct rayo_message *rayo_message_create(iks *xml)
{
	struct rayo_message *msg = malloc(sizeof(*msg));
	msg->payload = xml;
	return msg;
}

/**
 * Create a new xml message for delivery to an actor.  The XML is duplicated before delivery.
 * @param xml to create message from.  This value will not be freed.
 * @return the message
 */
struct rayo_message *rayo_message_create_dup(iks *xml)
{
	struct rayo_message *msg = malloc(sizeof(*msg));
	msg->payload = iks_copy(xml);
	return msg;
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
	return payload;
}

/**
 * Send message to actor
 */
struct rayo_message *rayo_actor_send(struct rayo_actor *from, struct rayo_actor *actor, struct rayo_message *msg, const char *file, int line)
{
	struct rayo_message *reply = NULL;
	iks *payload = msg->payload;
	char *msg_str = iks_string(iks_stack(payload), payload);

	switch_log_printf(SWITCH_CHANNEL_ID_LOG, file, "", line, "", SWITCH_LOG_DEBUG, "%s, %s\n", RAYO_JID(from), msg_str);
	switch_mutex_lock(actor->mutex);
	reply = actor->send_fn(from, actor, msg, file, line);
	switch_mutex_unlock(actor->mutex);

	rayo_message_destroy(msg);

	return reply;
}

/**
 * Send message to actor addressed by JID
 */
struct rayo_message *rayo_actor_send_by_jid(struct rayo_actor *from, const char *jid, struct rayo_message *msg, const char *file, int line)
{
	struct rayo_message *reply = NULL;
	struct rayo_actor *actor = RAYO_LOCATE(jid);
	if (actor) {
		reply = rayo_actor_send(from, actor, msg, file, line);
		RAYO_UNLOCK(actor);
	} else {
		switch_log_printf(SWITCH_CHANNEL_ID_LOG, file, "", line, "", SWITCH_LOG_DEBUG, "%s, failed to locate %s.\n", RAYO_JID(from), jid);
		rayo_message_destroy(msg);
	}
	return reply;
}

/**
 * Get access to Rayo actor with JID.
 * @param jid the JID
 * @return the actor or NULL.  Call RAYO_UNLOCK() when done with pointer.
 */
struct rayo_actor *rayo_actor_locate(const char *jid, const char *file, int line)
{
	struct rayo_actor *actor = NULL;
	switch_mutex_lock(globals.actors_mutex);
	actor = (struct rayo_actor *)switch_core_hash_find(globals.actors, jid);
	if (actor) {
		if (!actor->destroy) {
			actor->ref_count++;
			switch_log_printf(SWITCH_CHANNEL_ID_LOG, file, "", line, "", SWITCH_LOG_DEBUG, "Locate %s: ref count = %i\n", RAYO_JID(actor), actor->ref_count);
		} else {
			actor = NULL;
		}
	}
	switch_mutex_unlock(globals.actors_mutex);
	return actor;
}

/**
 * Get exclusive access to Rayo actor with internal ID
 * @param id the internal ID
 * @return the actor or NULL.  Call RAYO_UNLOCK() when done with pointer.
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
				switch_log_printf(SWITCH_CHANNEL_ID_LOG, file, "", line, "", SWITCH_LOG_DEBUG, "Locate %s: ref count = %i\n", RAYO_JID(actor), actor->ref_count);
			} else {
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
		switch_log_printf(SWITCH_CHANNEL_ID_LOG, file, "", line, "", SWITCH_LOG_DEBUG, "Destroying %s\n", RAYO_JID(actor));
		if (actor->cleanup_fn) {
			actor->cleanup_fn(actor);
		}
		switch_core_hash_delete(globals.destroy_actors, RAYO_JID(actor));
		switch_core_destroy_memory_pool(&pool);
	} else {
		switch_core_hash_insert(globals.destroy_actors, RAYO_JID(actor), actor);
	}
	switch_mutex_unlock(globals.actors_mutex);
}

/**
 * Increment actor ref count - locks from destruction.
 */
void rayo_actor_rdlock(struct rayo_actor *actor, const char *file, int line)
{
	if (actor) {
		switch_mutex_lock(globals.actors_mutex);
		actor->ref_count++;
		switch_log_printf(SWITCH_CHANNEL_ID_LOG, file, "", line, "", SWITCH_LOG_DEBUG, "Lock %s: ref count = %i\n", RAYO_JID(actor), actor->ref_count);
		switch_mutex_unlock(globals.actors_mutex);
	}
}

/**
 * Unlock rayo actor
 */
void rayo_actor_unlock(struct rayo_actor *actor, const char *file, int line)
{
	if (actor) {
		switch_mutex_lock(globals.actors_mutex);
		actor->ref_count--;
		switch_log_printf(SWITCH_CHANNEL_ID_LOG, file, "", line, "", SWITCH_LOG_DEBUG, "Unlock %s: ref count = %i\n", RAYO_JID(actor), actor->ref_count);
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

#define RAYO_CALL_LOCATE(call_uuid) rayo_call_locate(call_uuid, __FILE__, __LINE__)
/**
 * Get exclusive access to Rayo call data.  Use to access call data outside channel thread.
 * @param call_uuid the FreeSWITCH call UUID
 * @return the call or NULL.
 */
static struct rayo_call *rayo_call_locate(const char *call_uuid, const char *file, int line)
{
	struct rayo_actor *actor = rayo_actor_locate_by_id(call_uuid, file, line);
	if (actor && actor->type == RAT_CALL) {
		return RAYO_CALL(actor);
	} else if (actor) {
		RAYO_UNLOCK(actor);
	}
	return NULL;
}

/**
 * Fire <end> event when call is cleaned up completely
 */
static void rayo_call_cleanup(struct rayo_actor *actor)
{
	struct rayo_call *call = RAYO_CALL(actor);
	switch_event_t *event = call->end_event;
	int no_offered_clients = 1;
	switch_hash_index_t *hi = NULL;
	iks *revent;
	iks *end;

	if (!event) {
		/* destroyed before FS session was created (in originate, for example) */
		return;
	}

	revent = iks_new_presence("end", RAYO_NS,
		RAYO_JID(call),
		rayo_call_get_dcp_jid(call));
	iks_insert_attrib(revent, "type", "unavailable");
	end = iks_find(revent, "end");

	if (switch_true(switch_event_get_header(event, "variable_rayo_local_hangup"))) {
		iks_insert(end, RAYO_END_REASON_HANGUP_LOCAL);
	} else {
		/* remote hangup... translate to specific rayo reason */
		switch_call_cause_t cause = SWITCH_CAUSE_NONE;
		char *cause_str = switch_event_get_header(event, "variable_hangup_cause");
		if (cause_str) {
			cause = switch_channel_str2cause(cause_str);
		}
		iks_insert(end, switch_cause_to_rayo_cause(cause));
	}

	#if 0
	{
		char *event_str;
		if (switch_event_serialize(event, &event_str, SWITCH_FALSE) == SWITCH_STATUS_SUCCESS) {
			switch_log_printf(SWITCH_CHANNEL_UUID_LOG(rayo_call_get_uuid(call)), SWITCH_LOG_DEBUG, "%s\n", event_str);
			switch_safe_free(event_str);
		}
	}
	#endif

	/* add signaling headers */
	{
		switch_event_header_t *header;
		/* get all variables prefixed with sip_r_ */
		for (header = event->headers; header; header = header->next) {
			if (!strncmp("variable_sip_r_", header->name, 15)) {
				add_header(end, header->name + 15, header->value);
			}
		}
	}

	/* send <end> to all offered clients */
	for (hi = switch_hash_first(NULL, call->pcps); hi; hi = switch_hash_next(hi)) {
		const void *key;
		void *val;
		const char *client_jid = NULL;
		switch_hash_this(hi, &key, NULL, &val);
		client_jid = (const char *)key;
		switch_assert(client_jid);
		iks_insert_attrib(revent, "to", client_jid);
		switch_log_printf(SWITCH_CHANNEL_UUID_LOG(rayo_call_get_uuid(call)), SWITCH_LOG_DEBUG, "Sending <end> to offered client %s\n", client_jid);
		RAYO_SEND_BY_JID(actor, client_jid, rayo_message_create_dup(revent));
		no_offered_clients = 0;
	}

	if (no_offered_clients) {
		/* send to DCP only */
		switch_log_printf(SWITCH_CHANNEL_UUID_LOG(rayo_call_get_uuid(call)), SWITCH_LOG_DEBUG, "Sending <end> to DCP %s\n", rayo_call_get_dcp_jid(call));
		RAYO_SEND_BY_JID(actor, rayo_call_get_dcp_jid(call), rayo_message_create_dup(revent));
	}

	iks_delete(revent);
	switch_event_destroy(&event);
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
 * @return true if joined
 */
static int rayo_call_is_joined(struct rayo_call *call)
{
	return call->joined;
}

#define RAYO_MIXER_LOCATE(mixer_name) rayo_mixer_locate(mixer_name, __FILE__, __LINE__)
/**
 * Get access to Rayo mixer data.
 * @param mixer_name the mixer name
 * @return the mixer or NULL. Call RAYO_UNLOCK() when done with mixer pointer.
 */
static struct rayo_mixer *rayo_mixer_locate(const char *mixer_name, const char *file, int line)
{
	struct rayo_actor *actor = rayo_actor_locate_by_id(mixer_name, file, line);
	if (actor && actor->type == RAT_MIXER) {
		return RAYO_MIXER(actor);
	} else if (actor) {
		RAYO_UNLOCK(actor);
	}
	return NULL;
}

/**
 * Default message handler - drops messages
 */
static struct rayo_message *rayo_actor_send_ignore(struct rayo_actor *from, struct rayo_actor *to, struct rayo_message *msg, const char *file, int line)
{
	switch_log_printf(SWITCH_CHANNEL_ID_LOG, file, "", line, "", SWITCH_LOG_WARNING, "%s, dropping unexpected message to %s.\n", RAYO_JID(from), RAYO_JID(to));
	return NULL;
}

#define RAYO_ACTOR_INIT(actor, pool, type, subtype, id, jid, cleanup, send) rayo_actor_init(actor, pool, type, subtype, id, jid, cleanup, send, __FILE__, __LINE__)

/**
 * Initialize a rayo actor
 * @param pool to use
 * @param type of actor (MIXER, CALL, SERVER, COMPONENT)
 * @param subtype of actor (input/output/prompt)
 * @param id internal ID
 * @param jid external ID
 * @param cleanup function
 * @param file that called this function
 * @param line that called this function
 * @return the actor
 */
static struct rayo_actor *rayo_actor_init(struct rayo_actor *actor, switch_memory_pool_t *pool, enum rayo_actor_type type, const char *subtype, const char *id, const char *jid, rayo_actor_cleanup_fn cleanup, rayo_actor_send_fn send, const char *file, int line)
{
	char *domain;
	actor->type = type;
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
	switch_mutex_init(&actor->mutex, SWITCH_MUTEX_NESTED, pool);
	actor->cleanup_fn = cleanup;
	if (send == NULL) {
		actor->send_fn = rayo_actor_send_ignore;
	} else {
		actor->send_fn = send;
	}

	/* add to hash of actors, so commands can route to call */
	switch_mutex_lock(globals.actors_mutex);
	if (!zstr(id)) {
		switch_core_hash_insert(globals.actors_by_id, actor->id, actor);
	}
	if (!zstr(jid)) {
		switch_core_hash_insert(globals.actors, RAYO_JID(actor), actor);
	}
	switch_mutex_unlock(globals.actors_mutex);

	switch_log_printf(SWITCH_CHANNEL_ID_LOG, file, "", line, "", SWITCH_LOG_DEBUG, "Init %s\n", RAYO_JID(actor));

	return actor;
}

/**
 * Initialize rayo call
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

	rayo_actor_init(RAYO_ACTOR(call), pool, RAT_CALL, "", uuid, call_jid, rayo_call_cleanup, rayo_call_send, file, line);
	call->dcp_jid = "";
	call->idle_start_time = switch_micro_time_now();
	call->joined = 0;
	switch_core_hash_init(&call->pcps, pool);

	switch_safe_free(call_jid);

	return call;
}

#define rayo_call_create(uuid) _rayo_call_create(uuid, __FILE__, __LINE__)
/**
 * Create Rayo call
 * @param uuid uuid to assign call, if NULL one is picked
 * @param file file that called this function
 * @param line number of file that called this function
 * @return the call
 */
static struct rayo_call *_rayo_call_create(const char *uuid, const char *file, int line)
{
	switch_memory_pool_t *pool;
	struct rayo_call *call;
	switch_core_new_memory_pool(&pool);
	call = switch_core_alloc(pool, sizeof(*call));
	return rayo_call_init(call, pool, uuid, file, line);
}

/**
 * Initialize mixer
 */
static struct rayo_mixer *rayo_mixer_init(struct rayo_mixer *mixer, switch_memory_pool_t *pool, const char *name, const char *file, int line)
{
	char *mixer_jid = switch_mprintf("%s@%s", name, RAYO_JID(globals.server));
	rayo_actor_init(RAYO_ACTOR(mixer), pool, RAT_MIXER, "", name, mixer_jid, NULL, rayo_mixer_send, file, line);
	switch_core_hash_init(&mixer->members, pool);
	switch_core_hash_init(&mixer->subscribers, pool);
	switch_safe_free(mixer_jid);
	return mixer;
}

#define rayo_mixer_create(name) _rayo_mixer_create(name, __FILE__, __LINE__)
/**
 * Create Rayo mixer
 * @param name of this mixer
 * @return the mixer
 */
static struct rayo_mixer *_rayo_mixer_create(const char *name, const char *file, int line)
{
	switch_memory_pool_t *pool;
	struct rayo_mixer *mixer = NULL;
	switch_core_new_memory_pool(&pool);
	mixer = switch_core_alloc(pool, sizeof(*mixer));
	return rayo_mixer_init(mixer, pool, name, file, line);
}

/**
 * Clean up component before destruction
 */
static void rayo_component_cleanup(struct rayo_actor *actor)
{
	/* parent can now be destroyed */
	RAYO_UNLOCK(RAYO_COMPONENT(actor)->parent);
}

/**
 * Initialize Rayo component
 * @param type of this component
 * @param id internal ID of this component
 * @param parent the parent that owns this component
 * @param client_jid the client that created this component
 * @return the component
 */
struct rayo_component *_rayo_component_init(struct rayo_component *component, switch_memory_pool_t *pool, const char *type, const char *id, struct rayo_actor *parent, const char *client_jid, const char *file, int line)
{
	enum rayo_actor_type actor_type;
	char *ref = switch_mprintf("%s-%d", type, rayo_actor_seq_next(parent));
	char *jid = switch_mprintf("%s/%s", RAYO_JID(parent), ref);
	if (zstr(id)) {
		id = jid;
	}
	if (parent->type == RAT_CALL || parent->type == RAT_CALL_COMPONENT) {
		actor_type = RAT_CALL_COMPONENT;
	} else if (parent->type == RAT_MIXER) {
		actor_type = RAT_MIXER_COMPONENT;
	} else {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Created component with parent %s, type (%s)\n",
			RAYO_JID(parent), rayo_actor_type_to_string(parent->type));
		return NULL;
	}

	rayo_actor_init(RAYO_ACTOR(component), pool, actor_type, type, id, jid, rayo_component_cleanup, rayo_component_send, file, line);

	RAYO_RDLOCK(parent);
	component->client_jid = switch_core_strdup(pool, client_jid);
	component->ref = switch_core_strdup(pool, ref);
	component->parent = parent;

	switch_safe_free(ref);
	switch_safe_free(jid);
	return component;
}

/**
 * Send XMPP message to client
 */
static struct rayo_message *rayo_client_send(struct rayo_actor *from, struct rayo_actor *client, struct rayo_message *msg, const char *file, int line)
{
	xmpp_stream_context_send(globals.xmpp_context, RAYO_CLIENT(client)->route, msg->payload);
	return NULL;
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
}

/**
 * Initialize rayo client
 * @param pool the memory pool for this client
 * @param jid for this client
 * @param route to this client
 * @param availability of client
 * @param send message transmission function
 * @param is_admin true if admin client
 * @param peer_server NULL if locally connected client
 * @return the new client
 */
static struct rayo_client *rayo_client_init(struct rayo_client *client, switch_memory_pool_t *pool, const char *jid, const char *route, enum presence_status availability, rayo_actor_send_fn send, int is_admin, struct rayo_peer_server *peer_server)
{
	RAYO_ACTOR_INIT(RAYO_ACTOR(client), pool, RAT_CLIENT, "", jid, jid, rayo_client_cleanup, send);
	client->is_admin = is_admin;
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

	return client;
}

/**
 * Create a new Rayo client
 * @param jid for this client
 * @param route to this client
 * @param availability of client
 * @param send message transmission function
 * @param is_admin true if admin client
 * @param peer_server NULL if locally connected client
 * @return the new client or NULL
 */
static struct rayo_client *rayo_client_create(const char *jid, const char *route, enum presence_status availability, rayo_actor_send_fn send, int is_admin, struct rayo_peer_server *peer_server)
{
	switch_memory_pool_t *pool;
	struct rayo_client *rclient = NULL;

	switch_core_new_memory_pool(&pool);
	if (!(rclient = switch_core_alloc(pool, sizeof(*rclient)))) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Memory Error\n");
		return NULL;
	}
	return rayo_client_init(rclient, pool, jid, route, availability, send, is_admin, peer_server);
}

/**
 * Send XMPP message to peer server
 */
static struct rayo_message *rayo_peer_server_send(struct rayo_actor *from, struct rayo_actor *server, struct rayo_message *msg, const char *file, int line)
{
	xmpp_stream_context_send(globals.xmpp_context, RAYO_JID(server), msg->payload);
	return NULL;
}

/**
 * Destroy peer server and its associated clients
 */
static void rayo_peer_server_cleanup(struct rayo_actor *actor)
{
	switch_hash_index_t *hi;
	struct rayo_peer_server *rserver = RAYO_PEER_SERVER(actor);

	/* a little messy... client will remove itself from the peer server when it is destroyed,
	 * however, there is no guarantee the client will actually be destroyed now so
	 * the server must remove the client.
	 */
	switch_mutex_lock(globals.clients_mutex);
	while ((hi = switch_core_hash_first(rserver->clients))) {
		const void *key;
		void *client;
		switch_core_hash_this(hi, &key, NULL, &client);
		switch_assert(client);
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "Removing %s from peer server %s\n", RAYO_JID(client), RAYO_JID(rserver));
		switch_core_hash_delete(rserver->clients, key);
		RAYO_CLIENT(client)->peer_server = NULL;
		RAYO_UNLOCK(client);
		RAYO_DESTROY(client);
	}
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
	RAYO_ACTOR_INIT(RAYO_ACTOR(rserver), pool, RAT_PEER_SERVER, "", jid, jid, rayo_peer_server_cleanup, rayo_peer_server_send);
	switch_core_hash_init(&rserver->clients, pool);
	return rserver;
}

/**
 * Check if client has control of offered call. Take control if nobody else does.
 * @param rclient the Rayo client
 * @param call the Rayo call
 * @param session the session
 * @param call_jid the call JID
 * @param call_uuid the internal call UUID
 * @return 1 if session has call control
 */
static int rayo_client_has_call_control(struct rayo_client *rclient, struct rayo_call *call, switch_core_session_t *session)
{
	int control = 0;

	if (zstr(RAYO_JID(rclient))) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_CRIT, "Null client JID!!\n");
		return 0;
	}

	/* nobody in charge */
	if (zstr(call->dcp_jid)) {
		/* was offered to this session? */
		if (switch_core_hash_find(call->pcps, RAYO_JID(rclient))) {
			/* take charge */
			call->dcp_jid = switch_core_strdup(RAYO_POOL(call), RAYO_JID(rclient));
			switch_channel_set_variable(switch_core_session_get_channel(session), "rayo_dcp_jid", rayo_call_get_dcp_jid(call));
			control = 1;
			switch_log_printf(SWITCH_CHANNEL_UUID_LOG(rayo_call_get_uuid(call)), SWITCH_LOG_INFO, "%s has control of call\n", rayo_call_get_dcp_jid(call));
		}
	} else if (rclient->is_admin || !strcmp(rayo_call_get_dcp_jid(call), RAYO_JID(rclient))) {
		control = 1;
	}

	if (!control) {
		switch_log_printf(SWITCH_CHANNEL_UUID_LOG(rayo_call_get_uuid(call)), SWITCH_LOG_INFO, "%s does not have control of call\n", RAYO_JID(rclient));
	}

	return control;
}

/**
 * Check Rayo server command for errors.
 * @param rclient the Rayo client
 * @param node the <iq> node
 * @return 1 if OK
 */
static iks *rayo_server_command_ok(struct rayo_actor *rclient, struct rayo_actor *server, iks *node)
{
	iks *response = NULL;
	int bad = zstr(iks_find_attrib(node, "id"));

	if (bad) {
		response = iks_new_error(node, STANZA_ERROR_BAD_REQUEST);
	} else if (rclient->type != RAT_CLIENT) {
		/* not a rayo client request */
		response = iks_new_error(node, STANZA_ERROR_NOT_ALLOWED);
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "%s, %s not a client\n", RAYO_JID(rclient), RAYO_JID(server));
	}

	return response;
}

/**
 * Check Rayo call command for errors.
 * @param rclient the Rayo client
 * @param call the Rayo call
 * @param session the session
 * @param node the <iq> node
 * @return 1 if OK
 */
static iks *rayo_call_command_ok(struct rayo_actor *rclient, struct rayo_call *call, switch_core_session_t *session, iks *node)
{
	iks *response = NULL;
	int bad = zstr(iks_find_attrib(node, "id"));

	if (bad) {
		response = iks_new_error(node, STANZA_ERROR_BAD_REQUEST);
	} else if (rclient->type == RAT_CALL_COMPONENT) {
		struct rayo_actor *client = RAYO_LOCATE(RAYO_COMPONENT(rclient)->client_jid);
		if (client) {
	 		iks *response = rayo_call_command_ok(client, call, session, node);
			RAYO_UNLOCK(client);
			return response;
		}
		/* not a client request */
		response = iks_new_error(node, STANZA_ERROR_NOT_ALLOWED);
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "%s, %s call component client is gone\n", RAYO_JID(rclient), RAYO_JID(call));
	} else if (rclient->type != RAT_CLIENT) {
		/* not a client request */
		response = iks_new_error(node, STANZA_ERROR_NOT_ALLOWED);
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "%s, %s not a client request\n", RAYO_JID(rclient), RAYO_JID(call));
	} else if (!rayo_client_has_call_control(RAYO_CLIENT(rclient), call, session)) {
		response = iks_new_error(node, STANZA_ERROR_CONFLICT);
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "%s, %s conflict\n", RAYO_JID(rclient), RAYO_JID(call));
	}

	return response;
}

/**
 * Check Rayo component command for errors.
 * @param rclient the client
 * @param component the component
 * @param node the <iq> node
 * @return 0 if error
 */
static iks *rayo_component_command_ok(struct rayo_actor *rclient, struct rayo_component *component, iks *node)
{
	iks *response = NULL;
	char *from = iks_find_attrib(node, "from");
	int bad = zstr(iks_find_attrib(node, "id"));

	if (bad) {
		response = iks_new_error(node, STANZA_ERROR_BAD_REQUEST);
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "%s, %s bad request\n", RAYO_JID(rclient), RAYO_JID(component));
	} else if (rclient->type != RAT_CLIENT) {
		/* internal message is ok */
		return NULL;
	} else if (!RAYO_CLIENT(rclient)->is_admin && strcmp(component->client_jid, from)) {
		/* does not have control of this component */
		response = iks_new_error(node, STANZA_ERROR_CONFLICT);
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "%s, %s conflict\n", RAYO_JID(rclient), RAYO_JID(component));
	}

	return response;
}

/**
 * Handle server message
 */
static struct rayo_message *rayo_server_send(struct rayo_actor *client, struct rayo_actor *server, struct rayo_message *msg, const char *file, int line)
{
	rayo_actor_xmpp_handler handler = NULL;
	iks *iq = msg->payload;
	iks *response = NULL;

	if (!strcmp("presence", iks_name(iq))) {
		on_client_presence(RAYO_CLIENT(client), iq);
		return NULL;
	}

	/* is this a command a server supports? */
	handler = rayo_actor_command_handler_find(server, iq);
	if (!handler) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "%s, no handler function for command to %s\n", RAYO_JID(client), RAYO_JID(server));
		return rayo_message_create(iks_new_error(iq, STANZA_ERROR_FEATURE_NOT_IMPLEMENTED));
	}

	/* is the command valid? */
	if (!(response = rayo_server_command_ok(client, server, iq))) {
		response = handler(client, server, iq, NULL);
	}

	if (response) {
		return rayo_message_create(response);
	}
	return NULL;
}

/**
 * Handle call message
 */
static struct rayo_message *rayo_call_send(struct rayo_actor *client, struct rayo_actor *call, struct rayo_message *msg, const char *file, int line)
{
	rayo_actor_xmpp_handler handler = NULL;
	iks *iq = msg->payload;
	switch_core_session_t *session;
	iks *response = NULL;

	/* is this a command a call supports? */
	handler = rayo_actor_command_handler_find(call, iq);
	if (!handler) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "%s, no handler function for command\n", RAYO_JID(call));
		return rayo_message_create(iks_new_error(iq, STANZA_ERROR_FEATURE_NOT_IMPLEMENTED));
	}

	/* is the session still available? */
	session = switch_core_session_locate(rayo_call_get_uuid(RAYO_CALL(call)));
	if (!session) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "%s, session not found\n", RAYO_JID(call));
		return rayo_message_create(iks_new_error(iq, STANZA_ERROR_SERVICE_UNAVAILABLE));
	}

	/* is the command valid? */
	if (!(response = rayo_call_command_ok(client, RAYO_CALL(call), session, iq))) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "%s, executing command\n", RAYO_JID(call));
		response = handler(client, call, iq, session);
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "%s, done executing command\n", RAYO_JID(call));
		RAYO_CALL(call)->idle_start_time = switch_micro_time_now();
	}
	switch_core_session_rwunlock(session);

	if (response) {
		return rayo_message_create(response);
	}
	return NULL;
}

/**
 * Handle mixer message
 */
static struct rayo_message *rayo_mixer_send(struct rayo_actor *client, struct rayo_actor *mixer, struct rayo_message *msg, const char *file, int line)
{
	rayo_actor_xmpp_handler handler = NULL;
	iks *iq = msg->payload;
	iks *response = NULL;

	/* is this a command a mixer supports? */
	handler = rayo_actor_command_handler_find(mixer, iq);
	if (!handler) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "%s, no handler function for command\n", RAYO_JID(mixer));
		return rayo_message_create(iks_new_error(iq, STANZA_ERROR_FEATURE_NOT_IMPLEMENTED));
	}

	/* execute the command */
	response = handler(client, mixer, iq, NULL);
	if (response) {
		return rayo_message_create(response);
	}
	return NULL;
}

/**
 * Handle mixer message
 */
static struct rayo_message *rayo_component_send(struct rayo_actor *client, struct rayo_actor *component, struct rayo_message *msg, const char *file, int line)
{
	rayo_actor_xmpp_handler handler = NULL;
	iks *xml_msg = msg->payload;
	iks *response = NULL;

	if (!strcmp("iq", iks_name(xml_msg))) {
		/* is this a command a component supports? */
		handler = rayo_actor_command_handler_find(component, xml_msg);
		if (!handler) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "%s, no component handler function for command\n", RAYO_JID(component));
			return rayo_message_create(iks_new_error(xml_msg, STANZA_ERROR_FEATURE_NOT_IMPLEMENTED));
		}

		/* is the command valid? */
		if (!(response = rayo_component_command_ok(client, RAYO_COMPONENT(component), xml_msg))) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "%s, executing command\n", RAYO_JID(component));
			response = handler(client, component, xml_msg, NULL);
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "%s, done executing command\n", RAYO_JID(component));
		}

		if (response) {
			return rayo_message_create(response);
		}
	} else if (!strcmp("presence", iks_name(xml_msg))) {
		/* is this an event the component wants? */
		handler = rayo_actor_event_handler_find(client, component, xml_msg);
		if (!handler) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "%s, no component handler function for event\n", RAYO_JID(component));
			return NULL;
		}

		/* forward the event */
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "%s, forwarding event\n", RAYO_JID(component));
		handler(client, component, xml_msg, NULL);
	}

	return NULL;
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
static iks *on_rayo_accept(struct rayo_actor *client, struct rayo_actor *call, iks *node, void *session_data)
{
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
static iks *on_rayo_answer(struct rayo_actor *client, struct rayo_actor *call, iks *node, void *session_data)
{
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
static iks *on_rayo_redirect(struct rayo_actor *client, struct rayo_actor *call, iks *node, void *session_data)
{
	switch_core_session_t *session = (switch_core_session_t *)session_data;
	iks *response = NULL;
	iks *redirect = iks_find(node, "redirect");
	char *redirect_to = iks_find_attrib(redirect, "to");

	if (zstr(redirect_to)) {
		response = iks_new_error_detailed(node, STANZA_ERROR_BAD_REQUEST, "Missing redirect to attrib");
	} else {
		switch_core_session_message_t msg = { 0 };
		add_signaling_headers(session, redirect, RAYO_SIP_RESPONSE_HEADER);

		/* Tell the channel to deflect the call */
		msg.from = __FILE__;
		msg.string_arg = switch_core_session_strdup(session, redirect_to);
		msg.message_id = SWITCH_MESSAGE_INDICATE_DEFLECT;
		switch_core_session_receive_message(session, &msg);
		response = iks_new_iq_result(node);
	}
	return response;
}

/**
 * Handle <iq><hangup> or <iq><reject> request
 * @param call the Rayo call
 * @param session the session
 * @param node the <iq> node
 */
static iks *on_rayo_hangup(struct rayo_actor *client, struct rayo_actor *call, iks *node, void *session_data)
{
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
 * @param node the join request
 * @param call_id to join
 * @param media mode (direct/bridge)
 * @return the response
 */
static iks *join_call(struct rayo_call *call, switch_core_session_t *session, iks *node, const char *call_id, const char *media)
{
	iks *response = NULL;
	/* take call out of media path if media = "direct" */
	const char *bypass = !strcmp("direct", media) ? "true" : "false";

	/* check if joining to rayo call */
	struct rayo_call *b_call = RAYO_CALL_LOCATE(call_id);
	if (!b_call) {
		/* not a rayo call */
		response = iks_new_error_detailed(node, STANZA_ERROR_SERVICE_UNAVAILABLE, "b-leg is not a rayo call");
	} else if (b_call->joined) {
		/* don't support multiple joined calls */
		response = iks_new_error_detailed(node, STANZA_ERROR_CONFLICT, "multiple joined calls not supported");
		RAYO_UNLOCK(b_call);
	} else {
		RAYO_UNLOCK(b_call);

		/* bridge this call to call-uri */
		switch_channel_set_variable(switch_core_session_get_channel(session), "bypass_media", bypass);
		if (switch_false(bypass)) {
			switch_channel_pre_answer(switch_core_session_get_channel(session));
		}
		if (switch_ivr_uuid_bridge(rayo_call_get_uuid(call), call_id) == SWITCH_STATUS_SUCCESS) {
			response = iks_new_iq_result(node);
		} else {
			response = iks_new_error_detailed(node, STANZA_ERROR_INTERNAL_SERVER_ERROR, "failed to bridge call");
		}
	}
	return response;
}

/**
 * Join call to a mixer
 * @param call the call that joins
 * @param session the session
 * @param node the join request
 * @return the response
 */
static iks *join_mixer(struct rayo_call *call, switch_core_session_t *session, iks *node, const char *mixer_name)
{
	iks *response = NULL;
	char *conf_args = switch_mprintf("%s@%s", mixer_name, globals.mixer_conf_profile);
	if (switch_core_session_execute_application_async(session, "conference", conf_args) == SWITCH_STATUS_SUCCESS) {
		response = iks_new_iq_result(node);
	} else {
		response = iks_new_error_detailed(node, STANZA_ERROR_INTERNAL_SERVER_ERROR, "failed execute conference app");
	}
	switch_safe_free(conf_args);
	return response;
}

/**
 * Handle <iq><join> request
 * @param call the Rayo call
 * @param session the session
 * @param node the <iq> node
 */
static iks *on_rayo_join(struct rayo_actor *client, struct rayo_actor *call, iks *node, void *session_data)
{
	switch_core_session_t *session = (switch_core_session_t *)session_data;
	iks *response = NULL;
	iks *join = iks_find(node, "join");
	const char *mixer_name;
	const char *call_id;

	/* validate input attributes */
	if (!VALIDATE_RAYO_JOIN(join)) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "Bad join attrib\n");
		response = iks_new_error(node, STANZA_ERROR_BAD_REQUEST);
		goto done;
	}
	mixer_name = iks_find_attrib(join, "mixer-name");
	call_id = iks_find_attrib(join, "call-uri");

	/* can't join both mixer and call */
	if (!zstr(mixer_name) && !zstr(call_id)) {
		response = iks_new_error_detailed(node, STANZA_ERROR_BAD_REQUEST, "mixer-name and call-uri are mutually exclusive");
		goto done;
	}

	/* need to join *something* */
	if (zstr(mixer_name) && zstr(call_id)) {
		response = iks_new_error_detailed(node, STANZA_ERROR_BAD_REQUEST, "mixer-name or call-uri is required");
		goto done;
	}

	if (RAYO_CALL(call)->joined) {
		/* already joined */
		response = iks_new_error_detailed(node, STANZA_ERROR_CONFLICT, "call is already joined");
		goto done;
	}

	if (!zstr(mixer_name)) {
		/* join conference */
		response = join_mixer(RAYO_CALL(call), session, node,  mixer_name);
	} else {
		/* bridge calls */
		response = join_call(RAYO_CALL(call), session, node, call_id, iks_find_attrib(join, "media"));
	}

done:
	return response;
}

/**
 * unjoin call to a bridge
 * @param call the call that unjoined
 * @param session the session
 * @param node the unjoin request
 * @param call_id the b-leg uuid
 * @return the response
 */
static iks *unjoin_call(struct rayo_actor *client, struct rayo_call *call, switch_core_session_t *session, iks *node, const char *call_id)
{
	iks *response = NULL;
	const char *bleg = switch_channel_get_variable(switch_core_session_get_channel(session), SWITCH_BRIDGE_UUID_VARIABLE);

	/* bleg must match call_id */
	if (!zstr(bleg) && !strcmp(bleg, call_id)) {
		/* unbridge call */
		response = iks_new_iq_result(node);
		switch_ivr_park_session(session);
	} else {
		/* not bridged or wrong b-leg UUID */
		response = iks_new_error(node, STANZA_ERROR_SERVICE_UNAVAILABLE);
	}

	return response;
}

/**
 * unjoin call to a mixer
 * @param call the call that unjoined
 * @param session the session
 * @param node the unjoin request
 * @param mixer_name the mixer name
 * @return the response
 */
static iks *unjoin_mixer(struct rayo_actor *client, struct rayo_call *call, switch_core_session_t *session, iks *node, const char *mixer_name)
{
	switch_channel_t *channel = switch_core_session_get_channel(session);
	const char *conf_member_id = switch_channel_get_variable(channel, "conference_member_id");
	const char *conf_name = switch_channel_get_variable(channel, "conference_name");
	char *kick_command;
	iks *response = NULL;
	switch_stream_handle_t stream = { 0 };
	SWITCH_STANDARD_STREAM(stream);

	/* not conferenced, or wrong conference */
	if (zstr(conf_name) || strcmp(mixer_name, conf_name)) {
		response = iks_new_error_detailed_printf(node, STANZA_ERROR_SERVICE_UNAVAILABLE, "not joined to %s", mixer_name);
		goto done;
	} else if (zstr(conf_member_id)) {
		/* shouldn't happen */
		response = iks_new_error_detailed(node, STANZA_ERROR_SERVICE_UNAVAILABLE, "channel doesn't have conference member ID");
		goto done;
	}

	/* ack command */
	response = iks_new_iq_result(node);

	/* kick the member */
	kick_command = switch_core_session_sprintf(session, "%s hup %s", mixer_name, conf_member_id);
	switch_api_execute("conference", kick_command, NULL, &stream);

done:
	switch_safe_free(stream.data);

	return response;
}

/**
 * Handle <iq><unjoin> request
 * @param call the Rayo call
 * @param session the session
 * @param node the <iq> node
 */
static iks *on_rayo_unjoin(struct rayo_actor *client, struct rayo_actor *call, iks *node, void *session_data)
{
	switch_core_session_t *session = (switch_core_session_t *)session_data;
	iks *response = NULL;
	iks *unjoin = iks_find(node, "unjoin");
	const char *call_id = iks_find_attrib(unjoin, "call-uri");
	const char *mixer_name = iks_find_attrib(unjoin, "mixer-name");

	if (!zstr(call_id) && !zstr(mixer_name)) {
		response = iks_new_error(node, STANZA_ERROR_BAD_REQUEST);
	} else if (!RAYO_CALL(call)->joined) {
		/* not joined to anything */
		response = iks_new_error(node, STANZA_ERROR_SERVICE_UNAVAILABLE);
	} else if (!zstr(call_id)) {
		response = unjoin_call(client, RAYO_CALL(call), session, node, call_id);
	} else if (!zstr(mixer_name)) {
		response = unjoin_mixer(client, RAYO_CALL(call), session, node, mixer_name);
	} else {
		/* missing mixer or call */
		response = iks_new_error(node, STANZA_ERROR_BAD_REQUEST);
	}

	return response;
}

/**
 * Thread that handles originating new calls
 * @param thread this thread
 * @param obj the Rayo client
 * @return NULL
 */
static void *SWITCH_THREAD_FUNC rayo_dial_thread(switch_thread_t *thread, void *node)
{
	iks *iq = (iks *)node;
	iks *dial = iks_find(iq, "dial");
	iks *response = NULL;
	const char *dcp_jid = iks_find_attrib(iq, "from");
	const char *dial_to = iks_find_attrib(dial, "to");
	const char *dial_from = iks_find_attrib(dial, "from");
	const char *dial_timeout_ms = iks_find_attrib(dial, "timeout");
	struct dial_gateway *gateway = NULL;
	struct rayo_call *call = NULL;
	switch_stream_handle_t stream = { 0 };
	SWITCH_STANDARD_STREAM(stream);

	/* create call and link to DCP */
	call = rayo_call_create(NULL);
	call->dcp_jid = switch_core_strdup(RAYO_POOL(call), dcp_jid);
	call->dial_id = iks_find_attrib(iq, "id");
	switch_log_printf(SWITCH_CHANNEL_UUID_LOG(rayo_call_get_uuid(call)), SWITCH_LOG_INFO, "%s has control of call\n", dcp_jid);

	/* set rayo channel variables so channel originate event can be identified as coming from Rayo */
	stream.write_function(&stream, "{origination_uuid=%s,rayo_dcp_jid=%s,rayo_call_jid=%s",
		rayo_call_get_uuid(call), dcp_jid, RAYO_JID(call));

	/* set originate channel variables */
	if (!zstr(dial_from)) {
		/* caller ID */
		/* TODO parse caller ID name and number from URI */
		stream.write_function(&stream, ",origination_caller_id_number=%s,origination_caller_id_name=%s", dial_from, dial_from);
	}
	if (!zstr(dial_timeout_ms) && switch_is_number(dial_timeout_ms)) {
		/* timeout */
		int dial_timeout_sec = round((double)atoi(dial_timeout_ms) / 1000.0);
		stream.write_function(&stream, ",originate_timeout=%i", dial_timeout_sec);
	}

	/* set outbound signaling headers - only works on SIP */
	{
		iks *header = NULL;
		for (header = iks_find(dial, "header"); header; header = iks_next_tag(header)) {
			if (!strcmp("header", iks_name(header))) {
				const char *name = iks_find_attrib_soft(header, "name");
				const char *value = iks_find_attrib_soft(header, "value");
				if (!zstr(name) && !zstr(value)) {
					switch_log_printf(SWITCH_CHANNEL_UUID_LOG(rayo_call_get_uuid(call)), SWITCH_LOG_DEBUG, "Adding header: %s: %s\n", name, value);
					stream.write_function(&stream, ",%s%s=%s", RAYO_SIP_REQUEST_HEADER, name, value);
				}
			}
		}
	}

	stream.write_function(&stream, "}");

	/* build dialstring and dial call */
	gateway = dial_gateway_find(dial_to);
	if (gateway) {
		iks *join = iks_find(dial, "join");
		const char *dial_to_stripped = dial_to + gateway->strip;
		switch_stream_handle_t api_stream = { 0 };
		SWITCH_STANDARD_STREAM(api_stream);

		if (join) {
			/* check join args */
			const char *call_id = iks_find_attrib(join, "call-uri");
			const char *mixer_name = iks_find_attrib(join, "mixer-name");

			if (!zstr(call_id) && !zstr(mixer_name)) {
				/* can't join both */
				response = iks_new_error(iq, STANZA_ERROR_BAD_REQUEST);
				goto done;
			} else if (zstr(call_id) && zstr(mixer_name)) {
				/* nobody to join to? */
				response = iks_new_error(iq, STANZA_ERROR_BAD_REQUEST);
				goto done;
			} else if (!zstr(call_id)) {
				/* bridge */
				struct rayo_call *b_call = RAYO_CALL_LOCATE(call_id);
				/* is b-leg available? */
				if (!b_call) {
					response = iks_new_error_detailed(iq, STANZA_ERROR_SERVICE_UNAVAILABLE, "b-leg not found");
					goto done;
				} else if (b_call->joined) {
					response = iks_new_error_detailed(iq, STANZA_ERROR_SERVICE_UNAVAILABLE, "b-leg already joined to another call");
					RAYO_UNLOCK(b_call);
					goto done;
				}
				RAYO_UNLOCK(b_call);
				stream.write_function(&stream, "%s%s &rayo(bridge %s)", gateway->dial_prefix, dial_to_stripped, call_id);
			} else {
				/* conference */
				stream.write_function(&stream, "%s%s &rayo(conference %s@%s)", gateway->dial_prefix, dial_to_stripped, mixer_name, globals.mixer_conf_profile);
			}
		} else {
			stream.write_function(&stream, "%s%s &rayo", gateway->dial_prefix, dial_to_stripped);
		}

		switch_log_printf(SWITCH_CHANNEL_UUID_LOG(rayo_call_get_uuid(call)), SWITCH_LOG_DEBUG, "Using dialstring: %s\n", (char *)stream.data);

		/* <iq><ref> response will be sent when originate event is received- otherwise error is returned */
		if (switch_api_execute("originate", stream.data, NULL, &api_stream) == SWITCH_STATUS_SUCCESS) {
			switch_log_printf(SWITCH_CHANNEL_UUID_LOG(rayo_call_get_uuid(call)), SWITCH_LOG_DEBUG, "Got originate result: %s\n", (char *)api_stream.data);

			/* check for failure */
			if (strncmp("+OK", api_stream.data, strlen("+OK"))) {
				switch_log_printf(SWITCH_CHANNEL_UUID_LOG(rayo_call_get_uuid(call)), SWITCH_LOG_INFO, "Failed to originate call\n");

				if (call->dial_id) {
					/* map failure reason to iq error */
					if (!strncmp("-ERR DESTINATION_OUT_OF_ORDER", api_stream.data, strlen("-ERR DESTINATION_OUT_OF_ORDER"))) {
						/* this -ERR is received when out of sessions */
						response = iks_new_error(iq, STANZA_ERROR_RESOURCE_CONSTRAINT);
					} else {
						response = iks_new_error_detailed(iq, STANZA_ERROR_INTERNAL_SERVER_ERROR, api_stream.data);
					}
				}
			}
		} else if (call->dial_id) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "Failed to exec originate API\n");
			response = iks_new_error_detailed(iq, STANZA_ERROR_INTERNAL_SERVER_ERROR, "Failed to execute originate API");
		}

		switch_safe_free(api_stream.data);
	} else {
		/* will only happen if misconfigured */
		switch_log_printf(SWITCH_CHANNEL_UUID_LOG(rayo_call_get_uuid(call)), SWITCH_LOG_CRIT, "No dial gateway found for %s!\n", dial_to);
		response = iks_new_error_detailed_printf(iq, STANZA_ERROR_INTERNAL_SERVER_ERROR, "No dial gateway found for %s!\n", dial_to);
		goto done;
	}

done:

	/* response when error */
	if (response) {
		/* send response to client */
		RAYO_SEND_BY_JID(call, iks_find_attrib(response, "to"), rayo_message_create(response));

		/* destroy call */
		if (call) {
			RAYO_DESTROY(call);
			RAYO_UNLOCK(call);
		}
	}

	iks_delete(dial);
	switch_safe_free(stream.data);

	return NULL;
}

/**
 * Dial a new call
 * @param rclient requesting the call
 * @param server handling the call
 * @param node the request
 */
static iks *on_rayo_dial(struct rayo_actor *client, struct rayo_actor *server, iks *node, void *data)
{
	struct rayo_client *rclient = RAYO_CLIENT(client);
	switch_thread_t *thread;
	switch_threadattr_t *thd_attr = NULL;
	iks *dial = iks_find(node, "dial");
	iks *response = NULL;

	if (!zstr(iks_find_attrib(dial, "to"))) {
		iks *node_dup = iks_copy(node);
		iks_insert_attrib(node_dup, "from", RAYO_JID(rclient)); /* save DCP jid in case it isn't specified */

		/* start dial thread */
		switch_threadattr_create(&thd_attr, RAYO_POOL(rclient));
		switch_threadattr_detach_set(thd_attr, 1);
		switch_threadattr_stacksize_set(thd_attr, SWITCH_THREAD_STACKSIZE);
		switch_thread_create(&thread, thd_attr, rayo_dial_thread, node_dup, RAYO_POOL(rclient));
	} else {
		response = iks_new_error_detailed(node, STANZA_ERROR_BAD_REQUEST, "missing dial to attribute");
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
static iks *on_iq_xmpp_ping(struct rayo_actor *rclient, struct rayo_actor *server, iks *node, void *data)
{
	iks *pong = iks_new("iq");
	char *from = iks_find_attrib(node, "from");
	char *to = iks_find_attrib(node, "to");

	if (zstr(from)) {
		from = RAYO_JID(rclient);
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
static iks *on_iq_get_xmpp_disco(struct rayo_actor *rclient, struct rayo_actor *server, iks *node, void *data)
{
	iks *response = NULL;
	iks *x;
	response = iks_new_iq_result(node);
	x = iks_insert(response, "query");
	iks_insert_attrib(x, "xmlns", IKS_NS_XMPP_DISCO);
	x = iks_insert(x, "feature");
	iks_insert_attrib(x, "var", RAYO_NS);

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
	struct rayo_actor *actor;

	/* must be directed to a client */
	if (zstr(to)) {
		return;
	}

	/* assume client source */
	if (zstr(iks_find_attrib(message, "from"))) {
		iks_insert_attrib(message, "from", RAYO_JID(rclient));
	}

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "%s, recv message, availability = %s\n", RAYO_JID(rclient), presence_status_to_string(rclient->availability));

	actor = RAYO_LOCATE(to);
	if (actor && actor->type == RAT_CLIENT) {
		RAYO_SEND(rclient, actor, rayo_message_create_dup(message));
	}
	RAYO_UNLOCK(actor);
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
	/* TODO rethink this */
	//if (rclient->peer_server && rclient->availability == PS_OFFLINE) {
	//	RAYO_DESTROY(rclient);
	//	RAYO_UNLOCK(rclient);
	//}
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

	if (rclient->availability == PS_UNKNOWN) {
	}

	if (command) {
		struct rayo_actor *actor = RAYO_LOCATE(to);
		if (actor) {
			struct rayo_message *reply = RAYO_SEND(rclient, actor, rayo_message_create_dup(iq));
			if (reply) {
				RAYO_SEND(actor, rclient, reply);
			}
			RAYO_UNLOCK(actor);
		} else {
			RAYO_SEND(globals.server, rclient, rayo_message_create(iks_new_error(iq, STANZA_ERROR_ITEM_NOT_FOUND)));
		}
	} else {
		RAYO_SEND(globals.server, rclient, rayo_message_create(iks_new_error_detailed(iq, STANZA_ERROR_BAD_REQUEST, "empty IQ request")));
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
	for (hi = switch_core_hash_first(mixer->subscribers); hi; hi = switch_core_hash_next(hi)) {
		const void *key;
		void *val;
		struct rayo_mixer_subscriber *subscriber;
		switch_core_hash_this(hi, &key, NULL, &val);
		subscriber = (struct rayo_mixer_subscriber *)val;
		switch_assert(subscriber);
		iks_insert_attrib(rayo_event, "to", subscriber->jid);
		RAYO_SEND_BY_JID(mixer, subscriber->jid, rayo_message_create_dup(rayo_event));
	}
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
	member = (struct rayo_mixer_member *)switch_core_hash_find(mixer->members, uuid);
	if (!member) {
		/* not a member */
		return;
	}
	switch_core_hash_delete(mixer->members, uuid);

	/* flag call as available to join another mixer */
	call = RAYO_CALL_LOCATE(uuid);
	if (call) {
		call->joined = 0;
		RAYO_UNLOCK(call);
	}

	/* send mixer unjoined event to member DCP */
	delete_member_event = iks_new_presence("unjoined", RAYO_NS, member->jid, member->dcp_jid);
	x = iks_find(delete_member_event, "unjoined");
	iks_insert_attrib(x, "mixer-name", rayo_mixer_get_name(mixer));
	RAYO_SEND_BY_JID(mixer, member->dcp_jid, rayo_message_create(delete_member_event));

	/* broadcast member unjoined event to subscribers */
	delete_member_event = iks_new_presence("unjoined", RAYO_NS, RAYO_JID(mixer), "");
	x = iks_find(delete_member_event, "unjoined");
	iks_insert_attrib(x, "call-uri", uuid);
	broadcast_mixer_event(mixer, delete_member_event);
	iks_delete(delete_member_event);

	/* remove member DCP as subscriber to mixer */
	subscriber = (struct rayo_mixer_subscriber *)switch_core_hash_find(mixer->subscribers, member->dcp_jid);
	if (subscriber) {
		subscriber->ref_count--;
		if (subscriber->ref_count <= 0) {
			switch_core_hash_delete(mixer->subscribers, member->dcp_jid);
		}
	}
}

/**
 * Handle mixer destroy event
 */
static void on_mixer_destroy_event(struct rayo_mixer *mixer, switch_event_t *event)
{
	if (mixer) {
		/* remove from hash and destroy */
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "%s, destroying mixer: %s\n", RAYO_JID(mixer), rayo_mixer_get_name(mixer));
		RAYO_UNLOCK(mixer); /* release original lock */
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
	struct rayo_call *call = RAYO_CALL_LOCATE(uuid);

	if (!mixer) {
		/* new mixer */
		const char *mixer_name = switch_event_get_header(event, "Conference-Name");
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "creating mixer: %s\n", mixer_name);
		mixer = rayo_mixer_create(mixer_name);
	}

	if (call) {
		struct rayo_mixer_member *member = NULL;
		/* add member DCP as subscriber to mixer */
		struct rayo_mixer_subscriber *subscriber = (struct rayo_mixer_subscriber *)switch_core_hash_find(mixer->subscribers, call->dcp_jid);
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

		call->joined = 1;

		/* send mixer joined event to member DCP */
		add_member_event = iks_new_presence("joined", RAYO_NS, RAYO_JID(call), call->dcp_jid);
		x = iks_find(add_member_event, "joined");
		iks_insert_attrib(x, "mixer-name", rayo_mixer_get_name(mixer));
		RAYO_SEND_BY_JID(call, call->dcp_jid, rayo_message_create(add_member_event));

		RAYO_UNLOCK(call);
	}

	/* broadcast member joined event to subscribers */
	add_member_event = iks_new_presence("joined", RAYO_NS, RAYO_JID(mixer), "");
	x = iks_find(add_member_event, "joined");
	iks_insert_attrib(x, "call-uri", uuid);
	broadcast_mixer_event(mixer, add_member_event);
	iks_delete(add_member_event);
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
	RAYO_UNLOCK(mixer);
}

/**
 * Handle call originate event - create rayo call and send <iq><ref> to client.
 * @param rclient The Rayo client
 * @param event the originate event
 */
static void on_call_originate_event(struct rayo_client *rclient, switch_event_t *event)
{
	switch_core_session_t *session = NULL;
	const char *uuid = switch_event_get_header(event, "Unique-ID");
	struct rayo_call *call = RAYO_CALL_LOCATE(uuid);

	if (call && (session = switch_core_session_locate(uuid))) {
		iks *response, *ref;

		switch_channel_set_private(switch_core_session_get_channel(session), "rayo_call_private", call);
		switch_core_session_rwunlock(session);

		/* send response to DCP */
		response = iks_new("iq");
		iks_insert_attrib(response, "from", RAYO_JID(globals.server));
		iks_insert_attrib(response, "to", rayo_call_get_dcp_jid(call));
		iks_insert_attrib(response, "id", call->dial_id);
		iks_insert_attrib(response, "type", "result");
		ref = iks_insert(response, "ref");
		iks_insert_attrib(ref, "xmlns", RAYO_NS);

#ifdef RAYO_UUID_IN_REF_URI
	iks_insert_attrib(ref, "uri", uuid);
#else
	iks_insert_attrib_printf(ref, "uri", "xmpp:%s", RAYO_JID(call));
#endif
		RAYO_SEND(call, rclient, rayo_message_create(response));
		call->dial_id = NULL;
	}
	RAYO_UNLOCK(call);
}

/**
 * Handle call end event
 * @param event the hangup event
 */
static void on_call_end_event(switch_event_t *event)
{
	struct rayo_call *call = RAYO_CALL_LOCATE(switch_event_get_header(event, "Unique-ID"));

	if (call) {
#if 0
		char *event_str;
		if (switch_event_serialize(event, &event_str, SWITCH_FALSE) == SWITCH_STATUS_SUCCESS) {
			switch_log_printf(SWITCH_CHANNEL_UUID_LOG(rayo_call_get_uuid(call)), SWITCH_LOG_DEBUG, "%s\n", event_str);
			switch_safe_free(event_str);
		}
#endif
		switch_event_dup(&call->end_event, event);
		RAYO_UNLOCK(call); /* decrement ref from creation */
		RAYO_DESTROY(call);
		RAYO_UNLOCK(call); /* decrement this ref */
	}
}

/**
 * Handle call answer event
 * @param rclient the Rayo client
 * @param event the answer event
 */
static void on_call_answer_event(struct rayo_client *rclient, switch_event_t *event)
{
	struct rayo_call *call = RAYO_CALL_LOCATE(switch_event_get_header(event, "Unique-ID"));
	if (call) {
		iks *revent = iks_new_presence("answered", RAYO_NS,
			switch_event_get_header(event, "variable_rayo_call_jid"),
			switch_event_get_header(event, "variable_rayo_dcp_jid"));
		RAYO_SEND(call, rclient, rayo_message_create(revent));
		RAYO_UNLOCK(call);
	}
}

/**
 * Handle call ringing event
 * @param rclient the Rayo client
 * @param event the ringing event
 */
static void on_call_ringing_event(struct rayo_client *rclient, switch_event_t *event)
{
	struct rayo_call *call = RAYO_CALL_LOCATE(switch_event_get_header(event, "Unique-ID"));
	if (call) {
		iks *revent = iks_new_presence("ringing", RAYO_NS,
			switch_event_get_header(event, "variable_rayo_call_jid"),
			switch_event_get_header(event, "variable_rayo_dcp_jid"));
		RAYO_SEND(call, rclient, rayo_message_create(revent));
		RAYO_UNLOCK(call);
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
	struct rayo_call *call = RAYO_CALL_LOCATE(a_uuid);
	struct rayo_call *b_call;

	if (call) {
		/* send A-leg event */
		iks *revent = iks_new_presence("joined", RAYO_NS,
			switch_event_get_header(event, "variable_rayo_call_jid"),
			switch_event_get_header(event, "variable_rayo_dcp_jid"));
		iks *joined = iks_find(revent, "joined");
		iks_insert_attrib(joined, "call-uri", b_uuid);

		call->joined = 1;

		RAYO_SEND(call, rclient, rayo_message_create(revent));

		/* send B-leg event */
		b_call = RAYO_CALL_LOCATE(b_uuid);
		if (b_call) {
			revent = iks_new_presence("joined", RAYO_NS, RAYO_JID(b_call), rayo_call_get_dcp_jid(b_call));
			joined = iks_find(revent, "joined");
			iks_insert_attrib(joined, "call-uri", a_uuid);

			b_call->joined = 1;

			RAYO_SEND_BY_JID(b_call, rayo_call_get_dcp_jid(b_call), rayo_message_create(revent));
			RAYO_UNLOCK(b_call);
		}
		RAYO_UNLOCK(call);
	}
}

/**
 * Handle call unbridge event
 * @param rclient the Rayo client
 * @param event the unbridge event
 */
static void on_call_unbridge_event(struct rayo_client *rclient, switch_event_t *event)
{
	const char *a_uuid = switch_event_get_header(event, "Unique-ID");
	const char *b_uuid = switch_event_get_header(event, "Bridge-B-Unique-ID");
	struct rayo_call *call = RAYO_CALL_LOCATE(a_uuid);
	struct rayo_call *b_call;

	if (call) {
		/* send A-leg event */
		iks *revent = iks_new_presence("unjoined", RAYO_NS,
			switch_event_get_header(event, "variable_rayo_call_jid"),
			switch_event_get_header(event, "variable_rayo_dcp_jid"));
		iks *joined = iks_find(revent, "unjoined");
		iks_insert_attrib(joined, "call-uri", b_uuid);
		RAYO_SEND(call, rclient, rayo_message_create(revent));

		call->joined = 0;

		/* send B-leg event */
		b_call = RAYO_CALL_LOCATE(b_uuid);
		if (b_call) {
			revent = iks_new_presence("unjoined", RAYO_NS, RAYO_JID(b_call), rayo_call_get_dcp_jid(b_call));
			joined = iks_find(revent, "unjoined");
			iks_insert_attrib(joined, "call-uri", a_uuid);
			RAYO_SEND_BY_JID(b_call, rayo_call_get_dcp_jid(b_call), rayo_message_create(revent));

			b_call->joined = 0;
			RAYO_UNLOCK(b_call);
		}
		RAYO_UNLOCK(call);
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
		case SWITCH_EVENT_CHANNEL_PROGRESS_MEDIA:
			on_call_ringing_event(rclient, event);
			break;
		case SWITCH_EVENT_CHANNEL_ANSWER:
			on_call_answer_event(rclient, event);
			break;
		case SWITCH_EVENT_CHANNEL_BRIDGE:
			on_call_bridge_event(rclient, event);
			break;
		case SWITCH_EVENT_CHANNEL_UNBRIDGE:
			on_call_unbridge_event(rclient, event);
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
		if (actor && actor->type == RAT_CLIENT) {
			if (RAYO_CLIENT(actor)->is_admin) {
				/* FIXME ignore? */
			} else {
				/* process event and route to client */
				rayo_client_handle_event(RAYO_CLIENT(actor), event);
			}
		} else {
			/* TODO orphaned call... maybe allow events to queue so they can be delivered on reconnect? */
			switch_log_printf(SWITCH_CHANNEL_UUID_LOG(uuid), SWITCH_LOG_DEBUG, "Orphaned call event %s to %s\n", switch_event_name(event->event_id), dcp_jid);
		}
		RAYO_UNLOCK(actor);
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
	iks *offer = iks_insert(presence, "offer");

	iks_insert_attrib(presence, "from", RAYO_JID(call));
	iks_insert_attrib(offer, "from", profile->caller_id_number);
	iks_insert_attrib(offer, "to", profile->destination_number);
	iks_insert_attrib(offer, "xmlns", RAYO_NS);

	/* add signaling headers */
	{
		switch_event_header_t *var;
		add_header(offer, "from", switch_channel_get_variable(channel, "sip_full_from"));
		add_header(offer, "to", switch_channel_get_variable(channel, "sip_full_to"));
		add_header(offer, "via", switch_channel_get_variable(channel, "sip_full_via"));

		/* get all variables prefixed with sip_r_ */
		for (var = switch_channel_variable_first(channel); var; var = var->next) {
			if (!strncmp("sip_r_", var->name, 6)) {
				add_header(offer, var->name + 6, var->value);
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
		if (!rayo_call_is_joined(call) && idle_duration_ms > globals.max_idle_ms) {
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_WARNING, "Ending abandoned call.  idle_duration_ms = %i ms\n", idle_duration_ms);
			switch_channel_hangup(channel, RAYO_CAUSE_HANGUP);
		}
	}
	return SWITCH_STATUS_SUCCESS;
}

#define RAYO_USAGE "[bridge <uuid>|conference <name>]"
/**
 * Offer call and park channel
 */
SWITCH_STANDARD_APP(rayo_app)
{
	int ok = 0;
	switch_channel_t *channel = switch_core_session_get_channel(session);
	struct rayo_call *call = (struct rayo_call *)switch_channel_get_private(channel, "rayo_call_private");
	const char *app = ""; /* optional app to execute */
	const char *app_args = ""; /* app args */

	/* is outbound call already under control? */
	if (switch_channel_direction(channel) == SWITCH_CALL_DIRECTION_OUTBOUND) {
		/* check origination args */
		if (!zstr(data)) {
			char *argv[2] = { 0 };
			char *args = switch_core_session_strdup(session, data);
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
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_CRIT, "Missing rayo call!!\n");
			goto done;
		}
		ok = 1;
	} else {
		/* inbound call - offer control */
		switch_hash_index_t *hi = NULL;
		iks *offer = NULL;
		if (call) {
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_INFO, "Call is already under Rayo 3PCC!\n");
			goto done;
		}

		call = rayo_call_create(switch_core_session_get_uuid(session));
		switch_channel_set_variable(switch_core_session_get_channel(session), "rayo_call_jid", RAYO_JID(call));
		switch_channel_set_private(switch_core_session_get_channel(session), "rayo_call_private", call);

		offer = rayo_create_offer(call, session);
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "Offering call for Rayo 3PCC\n");

		/* Offer call to all ONLINE clients */
		/* TODO load balance offers so first session doesn't always get offer first? */
		switch_mutex_lock(globals.clients_mutex);
		for (hi = switch_hash_first(NULL, globals.clients_roster); hi; hi = switch_hash_next(hi)) {
			struct rayo_client *rclient;
			const void *key;
			void *val;
			switch_hash_this(hi, &key, NULL, &val);
			rclient = (struct rayo_client *)val;
			switch_assert(rclient);

			/* is session available to take call? */
			if (rclient->availability == PS_ONLINE) {
				ok = 1;
				switch_core_hash_insert(call->pcps, RAYO_JID(rclient), "1");
				iks_insert_attrib(offer, "to", RAYO_JID(rclient));
				RAYO_SEND_BY_JID(call, RAYO_JID(rclient), rayo_message_create_dup(offer));
			}
		}
		iks_delete(offer);
		switch_mutex_unlock(globals.clients_mutex);

		/* nobody to offer to */
		if (!ok) {
			switch_channel_hangup(channel, RAYO_CAUSE_DECLINE);
		}
	}

done:

	if (ok) {
		switch_channel_set_variable(channel, "hangup_after_bridge", "false");
		switch_channel_set_variable(channel, "transfer_after_bridge", "false");
		switch_channel_set_variable(channel, "park_after_bridge", "true");
		switch_channel_set_variable(channel, SWITCH_SEND_SILENCE_WHEN_IDLE_VARIABLE, "-1"); /* required so that output mixing works */
		switch_core_event_hook_add_read_frame(session, rayo_call_on_read_frame);
		if (switch_channel_direction(channel) == SWITCH_CALL_DIRECTION_OUTBOUND) {
			if (!zstr(app)) {
				switch_core_session_execute_application(session, app, app_args);
			}
		}
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
			actor = RAYO_ACTOR(rayo_client_create(jid, xmpp_stream_get_jid(stream), PS_UNKNOWN, rayo_client_send, 0, rserver));
			RAYO_RDLOCK(actor);
		} else if (actor->type != RAT_CLIENT) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "%s, not a client: %s\n", xmpp_stream_get_jid(stream), jid);
			RAYO_UNLOCK(actor);
			actor = NULL;
		}
	} else {
		actor = RAYO_ACTOR(xmpp_stream_get_private(stream));
		RAYO_RDLOCK(actor);
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
			xmpp_stream_set_private(stream, rayo_peer_server_create(xmpp_stream_get_jid(stream)));
		} else {
			/* send directed presence to domain */
			iks *presence = iks_new("presence");
			iks *x;
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "sending server presence\n");

			iks_insert_attrib(presence, "from", RAYO_JID(globals.server));
			iks_insert_attrib(presence, "to", xmpp_stream_get_jid(stream));
			x = iks_insert(presence, "show");
			iks_insert_cdata(x, "chat", 4);
			RAYO_SEND_BY_JID(globals.server, xmpp_stream_get_jid(stream), rayo_message_create(presence));
		}
	} else {
		/* client belongs to stream */
		xmpp_stream_set_private(stream, rayo_client_create(xmpp_stream_get_jid(stream), xmpp_stream_get_jid(stream), PS_OFFLINE, rayo_client_send, 0, NULL));
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
#if 0
		/* send probe */
		struct rayo_message *reply;
		switch_time_t now = switch_micro_time_now();

		/* throttle probes... */
		if (now - rclient->last_probe > 1000 * 1000 * 10) {
			iks *probe = iks_new("presence");
			rclient->last_probe = now;
			iks_insert_attrib(probe, "type", "probe");
			iks_insert_attrib(probe, "from", RAYO_JID(globals.server));
			iks_insert_attrib(probe, "to", RAYO_JID(rclient));
			reply = RAYO_SEND(globals.server, rclient, rayo_message_create(probe));
			if (reply) {
				rayo_message_destroy(reply);
			}
		}
	} else {
		rclient->last_probe = 0;
#endif
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
			RAYO_UNLOCK(actor);
		}
	} else if (!strcmp("presence", name)) {
		const char *from = iks_find_attrib_soft(stanza, "from");
		struct rayo_actor *actor = xmpp_stream_client_locate(stream, from);
		if (actor) {
			on_client_presence(RAYO_CLIENT(actor), stanza);
			RAYO_UNLOCK(actor);
		}
	} else if (!strcmp("message", name)) {
		const char *from = iks_find_attrib_soft(stanza, "from");
		struct rayo_actor *actor = xmpp_stream_client_locate(stream, from);
		if (actor) {
			rayo_client_presence_check(RAYO_CLIENT(actor));
			on_client_message(RAYO_CLIENT(actor), stanza);
			RAYO_UNLOCK(actor);
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
		RAYO_UNLOCK(actor);
		RAYO_DESTROY(actor);
	}
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

done:
	switch_xml_free(xml);

	return status;
}

/**
 * Dump rayo actor stats
 */
static void rayo_actor_dump(struct rayo_actor *actor, switch_stream_handle_t *stream)
{
	if (actor->type == RAT_CLIENT) {
		stream->write_function(stream, "TYPE='%s',SUBTYPE='%s',ID='%s',JID='%s',DOMAIN='%s',REFS=%i,STATUS='%s'", rayo_actor_type_to_string(actor->type), actor->subtype, actor->id, RAYO_JID(actor), RAYO_DOMAIN(actor), actor->ref_count, presence_status_to_string(RAYO_CLIENT(actor)->availability));
	} else {
		stream->write_function(stream, "TYPE='%s',SUBTYPE='%s',ID='%s',JID='%s',DOMAIN='%s',REFS=%i", rayo_actor_type_to_string(actor->type), actor->subtype, actor->id, RAYO_JID(actor), RAYO_DOMAIN(actor), actor->ref_count);
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
	for (hi = switch_core_hash_first(globals.actors); hi; hi = switch_core_hash_next(hi)) {
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

	for (hi = switch_core_hash_first(globals.destroy_actors); hi; hi = switch_core_hash_next(hi)) {
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
static struct rayo_message *rayo_console_client_send(struct rayo_actor *from, struct rayo_actor *actor, struct rayo_message *msg, const char *file, int line)
{
	iks *response = msg->payload;

	if (response) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CONSOLE, "\nRECV: from %s, %s\n", RAYO_JID(from), iks_string(iks_stack(response), response));
	} else {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CONSOLE, "\nRECV: (null) from %s\n", RAYO_JID(from));
	}

	return NULL;
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
	client = rayo_client_create(jid, NULL, PS_OFFLINE, rayo_console_client_send, 1, NULL);
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

	/* check if aliased */
	const char *alias = switch_core_hash_find(globals.cmd_aliases, command_str);
	if (!zstr(alias)) {
		command_str = alias;
	}

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
	} else {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CONSOLE, "bad request xml\n");
	}
	iks_parser_delete(p);
}

/**
 * Send command to rayo actor
 */
static int command_api(const char *cmd, switch_stream_handle_t *stream)
{
	char *cmd_dup = strdup(cmd);
	char *argv[2] = { 0 };
	int argc = switch_separate_string(cmd_dup, ' ', argv, sizeof(argv) / sizeof(argv[0]));

	if (argc != 2) {
		free(cmd_dup);
		return 0;
	}

	/* send command */
	send_console_command(globals.console, argv[0], argv[1]);
	stream->write_function(stream, "+OK\n");

	free(cmd_dup);
	return 1;
}

/**
 * Send message from console
 */
static void send_console_message(struct rayo_client *client, const char *to, const char *message_str)
{
	struct rayo_actor *actor = RAYO_LOCATE(to);
	if (actor) {
		struct rayo_message *reply;
		iks *message = NULL, *x;
		message = iks_new("message");
		iks_insert_attrib(message, "to", to);
		iks_insert_attrib(message, "from", RAYO_JID(client));
		iks_insert_attrib_printf(message, "id", "console-%i", RAYO_SEQ_NEXT(client));
		iks_insert_attrib(message, "type", "chat");
		x = iks_insert(message, "body");
		iks_insert_cdata(x, message_str, strlen(message_str));
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CONSOLE, "\nSEND: to %s, %s\n", to, iks_string(iks_stack(message), message));
		reply = RAYO_SEND(client, actor, rayo_message_create(message));
		if (reply) {
			/* ignore reply */
			rayo_message_destroy(reply);
		}
		RAYO_UNLOCK(actor);
	}
}

/**
 * Send message to rayo actor
 */
static int message_api(const char *msg, switch_stream_handle_t *stream)
{
	char *msg_dup = strdup(msg);
	char *argv[2] = { 0 };
	int argc = switch_separate_string(msg_dup, ' ', argv, sizeof(argv) / sizeof(argv[0]));

	if (argc != 2) {
		free(msg_dup);
		return 0;
	}

	/* send message */
	send_console_message(globals.console, argv[0], argv[1]);
	stream->write_function(stream, "+OK\n");

	free(msg_dup);
	return 1;
}

/**
 * Send presence from console
 */
static void send_console_presence(struct rayo_client *client, const char *to, int is_online)
{
	struct rayo_actor *actor = RAYO_LOCATE(to);
	if (actor) {
		struct rayo_message *reply;
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
		reply = RAYO_SEND(client, actor, rayo_message_create(presence));
		if (reply) {
			/* ignore reply */
			rayo_message_destroy(reply);
		}
		RAYO_UNLOCK(actor);
	}
}

/**
 * Send console presence
 */
static int presence_api(const char *cmd, switch_stream_handle_t *stream)
{
	char *cmd_dup = strdup(cmd);
	char *argv[2] = { 0 };
	int argc = switch_separate_string(cmd_dup, ' ', argv, sizeof(argv) / sizeof(argv[0]));
	int is_online = 0;

	if (argc != 2) {
		free(cmd_dup);
		return 0;
	}

	if (!strcmp("online", argv[1])) {
		is_online = 1;
	} else if (strcmp("offline", argv[1])) {
		free(cmd_dup);
		return 0;
	}

	/* send presence */
	send_console_presence(globals.console, argv[0], is_online);
	stream->write_function(stream, "+OK\n");
	free(cmd_dup);
	return 1;
}

#define RAYO_API_SYNTAX "status | (cmd <jid> <command>) | (msg <jid> <message text>) | (presence <jid> <online|offline>)"
SWITCH_STANDARD_API(rayo_api)
{
	int success = 0;
	if (!strncmp("status", cmd, 6)) {
		success = dump_api(cmd + 6, stream);
	} else if (!strncmp("cmd", cmd, 3)) {
		success = command_api(cmd + 3, stream);
	} else if (!strncmp("msg", cmd, 3)) {
		success = message_api(cmd + 3, stream);
	} else if (!strncmp("presence", cmd, 8)) {
		success = presence_api(cmd + 8, stream);
	}

	if (!success) {
		stream->write_function(stream, "-ERR: USAGE %s\n", RAYO_API_SYNTAX);
	}

	return SWITCH_STATUS_SUCCESS;
}

/**
 * Console auto-completion for all internal actors
 */
switch_status_t list_internal(const char *line, const char *cursor, switch_console_callback_match_t **matches)
{
	switch_hash_index_t *hi;
	void *val;
	const void *vvar;
	switch_console_callback_match_t *my_matches = NULL;
	switch_status_t status = SWITCH_STATUS_FALSE;
	struct rayo_actor *actor;

	switch_mutex_lock(globals.actors_mutex);
	for (hi = switch_hash_first(NULL, globals.actors); hi; hi = switch_hash_next(hi)) {
		switch_hash_this(hi, &vvar, NULL, &val);

		actor = (struct rayo_actor *) val;
		if (actor->type != RAT_CLIENT && actor->type != RAT_PEER_SERVER) {
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
 * Console auto-completion for all external actors
 */
switch_status_t list_external(const char *line, const char *cursor, switch_console_callback_match_t **matches)
{
	switch_hash_index_t *hi;
	void *val;
	const void *vvar;
	switch_console_callback_match_t *my_matches = NULL;
	switch_status_t status = SWITCH_STATUS_FALSE;
	struct rayo_actor *actor;

	switch_mutex_lock(globals.actors_mutex);
	for (hi = switch_hash_first(NULL, globals.actors); hi; hi = switch_hash_next(hi)) {
		switch_hash_this(hi, &vvar, NULL, &val);

		actor = (struct rayo_actor *) val;
		if (actor->type == RAT_CLIENT || actor->type == RAT_PEER_SERVER) {
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
 * Console auto-completion for all actors
 */
switch_status_t list_all(const char *line, const char *cursor, switch_console_callback_match_t **matches)
{
	switch_hash_index_t *hi;
	void *val;
	const void *vvar;
	switch_console_callback_match_t *my_matches = NULL;
	switch_status_t status = SWITCH_STATUS_FALSE;

	switch_mutex_lock(globals.actors_mutex);
	for (hi = switch_hash_first(NULL, globals.actors); hi; hi = switch_hash_next(hi)) {
		switch_hash_this(hi, &vvar, NULL, &val);
		switch_console_push_match(&my_matches, (const char *) vvar);
	}
	switch_mutex_unlock(globals.actors_mutex);

	if (my_matches) {
		*matches = my_matches;
		status = SWITCH_STATUS_SUCCESS;
	}

	return status;
}

/**
 * Add an alias to an API command
 * @param alias_name
 * @param alias_cmd
 */
static void rayo_add_cmd_alias(const char *alias_name, const char *alias_cmd)
{
	char *cmd = switch_core_sprintf(globals.pool, "add rayo cmd ::rayo::list_actors %s", alias_name);
	switch_console_set_complete(cmd);
	switch_core_hash_insert(globals.cmd_aliases, alias_name, alias_cmd);
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
	switch_core_hash_init(&globals.command_handlers, pool);
	switch_core_hash_init(&globals.event_handlers, pool);
	switch_core_hash_init(&globals.clients_roster, pool);
	switch_mutex_init(&globals.clients_mutex, SWITCH_MUTEX_NESTED, pool);
	switch_core_hash_init(&globals.actors, pool);
	switch_core_hash_init(&globals.destroy_actors, pool);
	switch_core_hash_init(&globals.actors_by_id, pool);
	switch_mutex_init(&globals.actors_mutex, SWITCH_MUTEX_NESTED, pool);
	switch_core_hash_init(&globals.dial_gateways, pool);
	switch_core_hash_init(&globals.cmd_aliases, pool);

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
	switch_event_bind(modname, SWITCH_EVENT_CHANNEL_ANSWER, NULL, route_call_event, NULL);
	switch_event_bind(modname, SWITCH_EVENT_CHANNEL_BRIDGE, NULL, route_call_event, NULL);
	switch_event_bind(modname, SWITCH_EVENT_CHANNEL_UNBRIDGE, NULL, route_call_event, NULL);

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

	/* create admin client */
	globals.console = rayo_console_client_create();

	switch_console_set_complete("add rayo status");
	switch_console_set_complete("add rayo cmd ::rayo::list_internal");
	switch_console_set_complete("add rayo msg ::rayo::list_external");
	switch_console_set_complete("add rayo presence ::rayo::list_all online");
	switch_console_set_complete("add rayo presence ::rayo::list_all offline");
	switch_console_add_complete_func("::rayo::list_internal", list_internal);
	switch_console_add_complete_func("::rayo::list_external", list_external);
	switch_console_add_complete_func("::rayo::list_all", list_all);

	rayo_add_cmd_alias("ping", "<iq type=\"get\"><ping xmlns=\""IKS_NS_XMPP_PING"\"/></iq>");
	rayo_add_cmd_alias("answer", "<answer xmlns=\""RAYO_NS"\"/>");
	rayo_add_cmd_alias("hangup", "<hangup xmlns=\""RAYO_NS"\"/>");
	rayo_add_cmd_alias("stop", "<stop xmlns=\""RAYO_EXT_NS"\"/>");
	rayo_add_cmd_alias("pause", "<pause xmlns=\""RAYO_OUTPUT_NS"\"/>");
	rayo_add_cmd_alias("resume", "<resume xmlns=\""RAYO_OUTPUT_NS"\"/>");
	rayo_add_cmd_alias("speed-up", "<speed-up xmlns=\""RAYO_OUTPUT_NS"\"/>");
	rayo_add_cmd_alias("speed-down", "<speed-down xmlns=\""RAYO_OUTPUT_NS"\"/>");
	rayo_add_cmd_alias("volume-up", "<volume-up xmlns=\""RAYO_OUTPUT_NS"\"/>");
	rayo_add_cmd_alias("volume-down", "<volume-down xmlns=\""RAYO_OUTPUT_NS"\"/>");
	rayo_add_cmd_alias("record", "<record xmlns=\""RAYO_RECORD_NS"\"/>");
	rayo_add_cmd_alias("record_pause", "<pause xmlns=\""RAYO_RECORD_NS"\"/>");
	rayo_add_cmd_alias("record_resume", "<resume xmlns=\""RAYO_RECORD_NS"\"/>");
	rayo_add_cmd_alias("prompt_barge", "<prompt xmlns=\""RAYO_PROMPT_NS"\" barge-in=\"true\">"
		"<output xmlns=\""RAYO_OUTPUT_NS"\" repeat-times=\"5\"><document content-type=\"application/ssml+xml\"><![CDATA[<speak><p>Please press a digit.</p></speak>]]></document></output>"
		"<input xmlns=\""RAYO_INPUT_NS"\" mode=\"dtmf\" initial-timeout=\"5000\" inter-digit-timeout=\"3000\">"
		"<grammar content-type=\"application/srgs+xml\">"
		"<![CDATA[<grammar mode=\"dtmf\"><rule id=\"digit\" scope=\"public\"><one-of><item>0</item><item>1</item><item>2</item><item>3</item><item>4</item><item>5</item><item>6</item><item>7</item><item>8</item><item>9</item></one-of></rule></grammar>]]>"
		"</grammar></input>"
		"</prompt>");

	rayo_add_cmd_alias("prompt_no_barge", "<prompt xmlns=\""RAYO_PROMPT_NS"\" barge-in=\"false\">"
		"<output xmlns=\""RAYO_OUTPUT_NS"\" repeat-times=\"2\"><document content-type=\"application/ssml+xml\"><![CDATA[<speak><p>Please press a digit.</p></speak>]]></document></output>"
		"<input xmlns=\""RAYO_INPUT_NS"\" mode=\"dtmf\" initial-timeout=\"5000\" inter-digit-timeout=\"3000\">"
		"<grammar content-type=\"application/srgs+xml\">"
		"<![CDATA[<grammar mode=\"dtmf\"><rule id=\"digit\" scope=\"public\"><one-of><item>0</item><item>1</item><item>2</item><item>3</item><item>4</item><item>5</item><item>6</item><item>7</item><item>8</item><item>9</item></one-of></rule></grammar>]]>"
		"</grammar></input>"
		"</prompt>");

	rayo_add_cmd_alias("prompt_long", "<prompt xmlns=\""RAYO_PROMPT_NS"\" barge-in=\"true\">"
		"<output xmlns=\""RAYO_OUTPUT_NS"\" repeat-times=\"100\"><document content-type=\"application/ssml+xml\"><![CDATA[<speak><audio src=\"http://phono.com/audio/troporocks.mp3\"/></speak>]]></document></output>"
		"<input xmlns=\""RAYO_INPUT_NS"\" mode=\"dtmf\" initial-timeout=\"5000\" inter-digit-timeout=\"3000\">"
		"<grammar content-type=\"application/srgs+xml\">"
		"<![CDATA[<grammar mode=\"dtmf\"><rule id=\"digit\" scope=\"public\"><one-of><item>0</item><item>1</item><item>2</item><item>3</item><item>4</item><item>5</item><item>6</item><item>7</item><item>8</item><item>9</item></one-of></rule></grammar>]]>"
		"</grammar></input>"
		"</prompt>");

	rayo_add_cmd_alias("prompt_multi_digit", "<prompt xmlns=\""RAYO_PROMPT_NS"\" barge-in=\"true\">"
		"<output xmlns=\""RAYO_OUTPUT_NS"\" repeat-times=\"100\"><document content-type=\"application/ssml+xml\"><![CDATA[<speak><audio src=\"http://phono.com/audio/troporocks.mp3\"/></speak>]]></document></output>"
		"<input xmlns=\""RAYO_INPUT_NS"\" mode=\"dtmf\" initial-timeout=\"5000\" inter-digit-timeout=\"3000\">"
		"<grammar content-type=\"application/srgs+xml\">"
		"<![CDATA[<grammar mode=\"dtmf\"><rule id=\"digits\" scope=\"public\"><item repeat=\"1-4\"><one-of><item>0</item><item>1</item><item>2</item><item>3</item><item>4</item><item>5</item><item>6</item><item>7</item><item>8</item><item>9</item></one-of></item></rule></grammar>]]>"
		"</grammar></input>"
		"</prompt>");

	rayo_add_cmd_alias("prompt_terminator", "<prompt xmlns=\""RAYO_PROMPT_NS"\" barge-in=\"true\">"
		"<output xmlns=\""RAYO_OUTPUT_NS"\" repeat-times=\"100\"><document content-type=\"application/ssml+xml\"><![CDATA[<speak><audio src=\"http://phono.com/audio/troporocks.mp3\"/></speak>]]></document></output>"
		"<input xmlns=\""RAYO_INPUT_NS"\" mode=\"dtmf\" initial-timeout=\"5000\" inter-digit-timeout=\"3000\" terminator=\"#\">"
		"<grammar content-type=\"application/srgs+xml\">"
		"<![CDATA[<grammar mode=\"dtmf\"><rule id=\"digits\" scope=\"public\"><item repeat=\"1-4\"><one-of><item>0</item><item>1</item><item>2</item><item>3</item><item>4</item><item>5</item><item>6</item><item>7</item><item>8</item><item>9</item></one-of></item></rule></grammar>]]>"
		"</grammar></input>"
		"</prompt>");

	rayo_add_cmd_alias("prompt_input_bad", "<prompt xmlns=\""RAYO_PROMPT_NS"\" barge-in=\"true\">"
		"<output xmlns=\""RAYO_OUTPUT_NS"\" repeat-times=\"100\"><document content-type=\"application/ssml+xml\"><![CDATA[<speak><audio src=\"http://phono.com/audio/troporocks.mp3\"/></speak>]]></document></output>"
		"<input xmlns=\""RAYO_INPUT_NS"\" mode=\"dtf\" initial-timeout=\"5000\" inter-digit-timeout=\"3000\">"
		"<grammar content-type=\"application/srgs+xml\">"
		"<![CDATA[<grammar mode=\"dtmf\"><rule id=\"digits\" scope=\"public\"><item repeat=\"4\"><one-of><item>0</item><item>1</item><item>2</item><item>3</item><item>4</item><item>5</item><item>6</item><item>7</item><item>8</item><item>9</item></one-of></item></rule></grammar>]]>"
		"</grammar></input>"
		"</prompt>");

	rayo_add_cmd_alias("prompt_output_bad", "<prompt xmlns=\""RAYO_PROMPT_NS"\" barge-in=\"true\">"
		"<output xmlns=\""RAYO_OUTPUT_NS"\" repeat-time=\"100\"><document content-type=\"application/ssml+xml\"><![CDATA[<speak><audio src=\"http://phono.com/audio/troporocks.mp3\"/></speak>]]></document></output>"
		"<input xmlns=\""RAYO_INPUT_NS"\" mode=\"dtmf\" initial-timeout=\"5000\" inter-digit-timeout=\"3000\">"
		"<grammar content-type=\"application/srgs+xml\">"
		"<![CDATA[<grammar mode=\"dtmf\"><rule id=\"digits\" scope=\"public\"><item repeat=\"4\"><one-of><item>0</item><item>1</item><item>2</item><item>3</item><item>4</item><item>5</item><item>6</item><item>7</item><item>8</item><item>9</item></one-of></item></rule></grammar>]]>"
		"</grammar></input>"
		"</prompt>");

	return SWITCH_STATUS_SUCCESS;
}

/**
 * Shutdown module.  Notifies threads to stop.
 */
SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_rayo_shutdown)
{
	if (globals.console) {
		RAYO_UNLOCK(globals.console);
		RAYO_DESTROY(globals.console);
	}

	switch_console_del_complete_func("::rayo::list_actors");
	switch_console_set_complete("del rayo");

	/* wait for threads to finish */
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Waiting for XMPP threads to stop\n");
	xmpp_stream_context_destroy(globals.xmpp_context);

	rayo_components_shutdown();

	/* cleanup module */
	switch_event_unbind_callback(route_call_event);
	switch_event_unbind_callback(on_call_end_event);
	switch_event_unbind_callback(route_mixer_event);

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Module shutdown\n");

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
 * vim:set softtabstop=4 shiftwidth=4 tabstop=4
 */
