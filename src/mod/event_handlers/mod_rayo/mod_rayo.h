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
 * mod_rayo.h -- Rayo server / node implementation.  Allows MxN clustering of FreeSWITCH and Rayo Clients (like Adhearsion)
 *
 */
#ifndef MOD_RAYO_H
#define MOD_RAYO_H

#include <switch.h>
#include <iksemel.h>

#include "iks_helpers.h"

#define RAYO_VERSION "1"
#define RAYO_BASE "urn:xmpp:rayo:"

#define RAYO_NS RAYO_BASE RAYO_VERSION
#define RAYO_CLIENT_NS RAYO_BASE "client:" RAYO_VERSION

struct rayo_actor;
struct rayo_call;
struct rayo_mixer;
struct rayo_component;

/**
 * A message sent to an actor
 */
struct rayo_message {
	iks *payload;
};

typedef void (* rayo_actor_cleanup_fn)(struct rayo_actor *);
typedef struct rayo_message *(* rayo_actor_send_fn)(struct rayo_actor *, struct rayo_actor *, struct rayo_message *, const char *file, int line);

/**
 * Type of actor
 */
enum rayo_actor_type {
	RAT_PEER_SERVER,
	RAT_CLIENT,
	RAT_SERVER,
	RAT_CALL,
	RAT_MIXER,
	RAT_CALL_COMPONENT,
	RAT_MIXER_COMPONENT
};

/**
 * A rayo actor - this is an entity that can be controlled by a rayo client
 */
struct rayo_actor {
	/** Type of actor */
	enum rayo_actor_type type;
	/** Sub-type of actor */
	char *subtype;
	/** domain part of JID */
	char *domain;
	/** Internal ID */
	char *id;
	/** actor JID */
	char *jid;
	/** Actor pool */
	switch_memory_pool_t *pool;
	/** synchronizes access to this actor */
	switch_mutex_t *mutex;
	/** an atomically incrementing sequence for this actor */
	int seq;
	/** number of users of this actor */
	int ref_count;
	/** destroy flag */
	int destroy;
	/** XMPP message handling function */
	rayo_actor_send_fn send_fn;
	/** optional cleanup */
	rayo_actor_cleanup_fn cleanup_fn;
};

/**
 * A Rayo component
 */
struct rayo_component {
	/** base actor class */
	struct rayo_actor base;
	/** component type (input/output/prompt/etc) */
	const char *type;
	/** parent to this component */
	struct rayo_actor *parent;
	/** owning client JID */
	const char *client_jid;
	/** external ref */
	const char *ref;
};

#define RAYO_ACTOR(x) ((struct rayo_actor *)x)
#define RAYO_COMPONENT(x) ((struct rayo_component *)x)
#define RAYO_CALL(x) ((struct rayo_call *)x)
#define RAYO_MIXER(x) ((struct rayo_mixer *)x)

extern struct rayo_message *rayo_message_create(iks *xml);
extern struct rayo_message *rayo_message_create_dup(iks *xml);
extern void rayo_message_destroy(struct rayo_message *msg);
extern iks *rayo_message_remove_payload(struct rayo_message *msg);

extern struct rayo_actor *rayo_actor_locate(const char *jid, const char *file, int line);
extern struct rayo_actor *rayo_actor_locate_by_id(const char *id, const char *file, int line);
extern int rayo_actor_seq_next(struct rayo_actor *actor);
extern struct rayo_message *rayo_actor_send(struct rayo_actor *from, struct rayo_actor *to, struct rayo_message *msg, const char *file, int line);
extern struct rayo_message *rayo_actor_send_by_jid(struct rayo_actor *from, const char *jid, struct rayo_message *msg, const char *file, int line);
extern void rayo_actor_rdlock(struct rayo_actor *actor, const char *file, int line);
extern void rayo_actor_unlock(struct rayo_actor *actor, const char *file, int line);
extern void rayo_actor_destroy(struct rayo_actor *actor, const char *file, int line);

#define RAYO_LOCATE(jid) rayo_actor_locate(jid, __FILE__, __LINE__)
#define RAYO_LOCATE_BY_ID(id) rayo_actor_locate_by_id(id, __FILE__, __LINE__)
#define RAYO_SET_EVENT_FN(actor, event) rayo_actor_set_event_fn(RAYO_ACTOR(actor), event)
#define RAYO_DOMAIN(x) RAYO_ACTOR(x)->domain
#define RAYO_JID(x) RAYO_ACTOR(x)->jid
#define RAYO_ID(x) RAYO_ACTOR(x)->id
#define RAYO_POOL(x) RAYO_ACTOR(x)->pool
#define RAYO_RDLOCK(x) rayo_actor_rdlock(RAYO_ACTOR(x), __FILE__, __LINE__)
#define RAYO_UNLOCK(x) rayo_actor_unlock(RAYO_ACTOR(x), __FILE__, __LINE__)
#define RAYO_DESTROY(x) rayo_actor_destroy(RAYO_ACTOR(x), __FILE__, __LINE__)
#define RAYO_SEQ_NEXT(x) rayo_actor_seq_next(RAYO_ACTOR(x))
#define RAYO_SEND(from, to, msg) rayo_actor_send(RAYO_ACTOR(from), RAYO_ACTOR(to), msg, __FILE__, __LINE__)
#define RAYO_SEND_BY_JID(from, jid, msg) rayo_actor_send_by_jid(RAYO_ACTOR(from), jid, msg, __FILE__, __LINE__)

extern const char *rayo_call_get_dcp_jid(struct rayo_call *call);

#define rayo_mixer_get_name(mixer) RAYO_ID(mixer)

#define rayo_component_init(component, pool, type, id, parent, client_jid) _rayo_component_init(component, pool, type, id, parent, client_jid, __FILE__, __LINE__)
extern struct rayo_component *_rayo_component_init(struct rayo_component *component, switch_memory_pool_t *pool, const char *type, const char *id, struct rayo_actor *parent, const char *client_jid, const char *file, int line);

typedef iks *(*rayo_actor_xmpp_handler)(struct rayo_actor *, struct rayo_actor *, iks *, void *);
extern void rayo_actor_command_handler_add(enum rayo_actor_type type, const char *subtype, const char *name, rayo_actor_xmpp_handler fn);
extern void rayo_actor_event_handler_add(enum rayo_actor_type from_type, const char *from_subtype, enum rayo_actor_type to_type, const char *to_subtype, const char *name, rayo_actor_xmpp_handler fn);

#endif


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
