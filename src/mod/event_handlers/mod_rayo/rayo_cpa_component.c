/*
 * mod_rayo for FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
 * Copyright (C) 2014, Grasshopper
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
 * rayo_cpa_component.c -- input component "cpa" mode implementation
 */
#include <switch.h>

#include "rayo_cpa_component.h"
#include "mod_rayo.h"
#include "rayo_components.h"
#include "rayo_cpa_detector.h"

/**
 * Module globals
 */
struct {
	/** signal subscribers */
	switch_hash_t *subscribers;
	/** synchronizes access to subscribers */
	switch_mutex_t *subscribers_mutex;
	/** module pool */
	switch_memory_pool_t *pool;
} globals;

/**
 * A CPA signal monitored by this component
 */
struct cpa_signal {
	/** name of this signal */
	const char *name;
	/** true if signal causes component termination */
	int terminate;
};

/**
 * CPA component state
 */
struct cpa_component {
	/** component base class */
	struct rayo_component base;
	/** true if ready to forward detector events */
	int ready;
	/** signals this component wants */
	switch_hash_t *signals;
};

#define CPA_COMPONENT(x) ((struct cpa_component *)x)

typedef void (* subscriber_execute_fn)(const char *jid, void *user_data);

/**
 * Request signals
 */
static void subscribe(const char *uuid, const char *signal_type, const char *jid)
{
	char *key = switch_mprintf("%s:%s", uuid, signal_type);
	switch_mutex_lock(globals.subscribers_mutex);
	{
		switch_hash_t *signal_subscribers = switch_core_hash_find(globals.subscribers, key);
		switch_log_printf(SWITCH_CHANNEL_UUID_LOG(uuid), SWITCH_LOG_DEBUG, "Subscribe %s => %s\n", signal_type, jid);
		if (!signal_subscribers) {
			switch_log_printf(SWITCH_CHANNEL_UUID_LOG(uuid), SWITCH_LOG_DEBUG, "Create %s subscriber hash\n", signal_type);
			switch_core_hash_init(&signal_subscribers, NULL);
			switch_core_hash_insert(globals.subscribers, key, signal_subscribers);
		}
		switch_core_hash_insert(signal_subscribers, jid, "1");
	}
	switch_mutex_unlock(globals.subscribers_mutex);
	switch_safe_free(key);
}

/**
 * Stop receiving signals
 */
static void unsubscribe(const char *uuid, const char *signal_type, const char *jid)
{
	char *key = switch_mprintf("%s:%s", uuid, signal_type);
	switch_mutex_lock(globals.subscribers_mutex);
	{
		switch_hash_t *signal_subscribers = switch_core_hash_find(globals.subscribers, key);
		if (signal_subscribers) {
			switch_core_hash_delete(signal_subscribers, jid);
			switch_log_printf(SWITCH_CHANNEL_UUID_LOG(uuid), SWITCH_LOG_DEBUG, "Unsubscribe %s => %s\n", signal_type, jid);

			/* clean up hash if empty */
			if (!switch_core_hash_first(signal_subscribers)) {
				switch_log_printf(SWITCH_CHANNEL_UUID_LOG(uuid), SWITCH_LOG_DEBUG, "Destroy %s subscriber hash\n", signal_type);
				switch_core_hash_destroy(&signal_subscribers);
				switch_core_hash_delete(globals.subscribers, key);
			}
		}
	}
	switch_mutex_unlock(globals.subscribers_mutex);
	switch_safe_free(key);
}

/**
 * Execute function for each subscriber
 */
static void subscriber_execute(const char *uuid, const char *signal_type, subscriber_execute_fn callback, void *user_data)
{
	switch_event_t *subscriber_list = NULL;
	switch_event_header_t *subscriber = NULL;

	/* fetch list of subscribers */
	char *key = switch_mprintf("%s:%s", uuid, signal_type);
	switch_event_create_subclass(&subscriber_list, SWITCH_EVENT_CLONE, NULL);
	switch_log_printf(SWITCH_CHANNEL_UUID_LOG(uuid), SWITCH_LOG_DEBUG, "Subscriber execute %s\n", signal_type);
	switch_mutex_lock(globals.subscribers_mutex);
	{
		switch_hash_index_t *hi = NULL;
		switch_hash_t *signal_subscribers = switch_core_hash_find(globals.subscribers, key);
		if (signal_subscribers) {
			for (hi = switch_core_hash_first(signal_subscribers); hi; hi = switch_core_hash_next(hi)) {
				const void *jid;
				void *dont_care;
				switch_core_hash_this(hi, &jid, NULL, &dont_care);
				switch_event_add_header_string(subscriber_list, SWITCH_STACK_BOTTOM, "execute", (const char *)jid);
			}
		} else {
			switch_log_printf(SWITCH_CHANNEL_UUID_LOG(uuid), SWITCH_LOG_DEBUG, "No subscribers for %s\n", signal_type);
		}
	}
	switch_mutex_unlock(globals.subscribers_mutex);
	switch_safe_free(key);

	/* execute function for each subscriber */
	for (subscriber = subscriber_list->headers; subscriber; subscriber = subscriber->next) {
		callback(subscriber->value, user_data);
	}

	switch_event_destroy(&subscriber_list);
}

/**
 * Stop all CPA detectors
 */
static void stop_cpa_detectors(struct cpa_component *cpa)
{
	if (cpa->signals) {
		switch_hash_index_t *hi = NULL;
		for (hi = switch_core_hash_first(cpa->signals); hi; hi = switch_core_hash_next(hi)) {
			const void *signal_type;
			void *cpa_signal = NULL;
			switch_core_hash_this(hi, &signal_type, NULL, &cpa_signal);
			if (cpa_signal) {
				rayo_cpa_detector_stop(RAYO_COMPONENT(cpa)->parent->id, ((struct cpa_signal *)cpa_signal)->name);
				unsubscribe(RAYO_COMPONENT(cpa)->parent->id, ((struct cpa_signal *)cpa_signal)->name, RAYO_JID(cpa));
			}
		}
		switch_core_hash_destroy(&cpa->signals);
		cpa->signals = NULL;
	}
	unsubscribe(RAYO_COMPONENT(cpa)->parent->id, "hangup", RAYO_JID(cpa));
}

/**
 * Stop execution of CPA component
 */
static iks *stop_cpa_component(struct rayo_actor *component, struct rayo_message *msg, void *data)
{
	stop_cpa_detectors(CPA_COMPONENT(component));
	rayo_component_send_complete(RAYO_COMPONENT(component), COMPONENT_COMPLETE_STOP);
	return iks_new_iq_result(msg->payload);
}

/**
 * Forward CPA signal to client
 */
static void rayo_cpa_detector_event(const char *jid, void *user_data)
{
	struct rayo_actor *component = RAYO_LOCATE(jid);
	if (component) {
		if (CPA_COMPONENT(component)->ready) {
			switch_event_t *event = (switch_event_t *)user_data;
			const char *signal_type = switch_event_get_header(event, "signal-type");
			struct cpa_signal *cpa_signal = switch_core_hash_find(CPA_COMPONENT(component)->signals, signal_type);
			switch_log_printf(SWITCH_CHANNEL_UUID_LOG(RAYO_COMPONENT(component)->parent->id), SWITCH_LOG_DEBUG, "Handling CPA event\n");
			if (cpa_signal) {
				const char *value = switch_event_get_header(event, "value");
				const char *duration = switch_event_get_header(event, "duration");
				if (cpa_signal->terminate) {
					iks *complete_event;
					iks *signal_xml;

					stop_cpa_detectors(CPA_COMPONENT(component));

					/* send complete event to client */
					complete_event = rayo_component_create_complete_event(RAYO_COMPONENT(component), "signal", RAYO_CPA_NS);
					signal_xml = iks_find(complete_event, "complete");
					signal_xml = iks_find(signal_xml, "signal");
					iks_insert_attrib(signal_xml, "type", signal_type);
					if (!zstr(value)) {
						iks_insert_attrib(signal_xml, "value", value);
					}
					if (!zstr(duration)) {
						iks_insert_attrib(signal_xml, "duration", duration);
					}
					rayo_component_send_complete_event(RAYO_COMPONENT(component), complete_event);
				} else {
					/* send event to client */
					iks *signal_event = iks_new_presence("signal", RAYO_CPA_NS, RAYO_JID(component), RAYO_COMPONENT(component)->client_jid);
					iks *signal_xml = iks_find(signal_event, "signal");
					iks_insert_attrib(signal_xml, "type", signal_type);
					if (!zstr(value)) {
						iks_insert_attrib(signal_xml, "value", value);
					}
					if (!zstr(duration)) {
						iks_insert_attrib(signal_xml, "duration", duration);
					}
					RAYO_SEND_REPLY(component, RAYO_COMPONENT(component)->client_jid, signal_event);
				}
			}
		} else {
			switch_log_printf(SWITCH_CHANNEL_UUID_LOG(RAYO_COMPONENT(component)->parent->id), SWITCH_LOG_DEBUG, "Skipping CPA event\n");
		}
		RAYO_UNLOCK(component);
	}
}

/**
 * Handle CPA signal-type event
 */
static void on_rayo_cpa_detector_event(switch_event_t *event)
{
	subscriber_execute(switch_event_get_header(event, "Unique-ID"), switch_event_get_header(event, "signal-type"), rayo_cpa_detector_event, event);
}

/**
 * Handle CPA completion because of hangup
 */
static void rayo_cpa_component_hangup(const char *jid, void *user_data)
{
	struct rayo_actor *component = RAYO_LOCATE(jid);
	if (component) {
		stop_cpa_detectors(CPA_COMPONENT(component));
		rayo_component_send_complete(RAYO_COMPONENT(component), COMPONENT_COMPLETE_HANGUP);
		RAYO_UNLOCK(component);
	}
}

/**
 * Handle hungup call event 
 */
static void on_channel_hangup_complete_event(switch_event_t *event)
{
	subscriber_execute(switch_event_get_header(event, "Unique-ID"), "hangup", rayo_cpa_component_hangup, event);
}

/**
 * Start CPA
 */
iks *rayo_cpa_component_start(struct rayo_actor *call, struct rayo_message *msg, void *session_data)
{
	iks *iq = msg->payload;
	switch_core_session_t *session = (switch_core_session_t *)session_data;
	iks *input = iks_find(iq, "input");
	switch_memory_pool_t *pool = NULL;
	struct cpa_component *component = NULL;
	int have_grammar = 0;
	iks *grammar = NULL;

	/* create CPA component */
	switch_core_new_memory_pool(&pool);
	component = switch_core_alloc(pool, sizeof(*component));
	component = CPA_COMPONENT(rayo_component_init((struct rayo_component *)component, pool, RAT_CALL_COMPONENT, "cpa", NULL, call, iks_find_attrib(iq, "from")));
	if (!component) {
		return iks_new_error_detailed(iq, STANZA_ERROR_INTERNAL_SERVER_ERROR, "Failed to create CPA entity");
	}

	switch_core_hash_init(&component->signals, pool);

	/* start CPA detectors */
	for (grammar = iks_find(input, "grammar"); grammar; grammar = iks_next_tag(grammar)) {
		if (!strcmp("grammar", iks_name(grammar))) {
			const char *error_str = "";
			const char *url = iks_find_attrib_soft(grammar, "url");
			char *url_dup;
			char *url_params;

			if (zstr(url)) {
				stop_cpa_detectors(component);
				RAYO_UNLOCK(component);
				RAYO_DESTROY(component);
				return iks_new_error_detailed(iq, STANZA_ERROR_BAD_REQUEST, "Missing grammar URL");
			}
			have_grammar = 1;

			url_dup = strdup(url);
			if ((url_params = strchr(url_dup, '?'))) {
				*url_params = '\0';
				url_params++;
			}

			if (switch_core_hash_find(component->signals, url)) {
				free(url_dup);
				stop_cpa_detectors(component);
				RAYO_UNLOCK(component);
				RAYO_DESTROY(component);
				return iks_new_error_detailed(iq, STANZA_ERROR_BAD_REQUEST, "Duplicate URL");
			}

			/* start detector */
			/* TODO return better reasons... */
			if (rayo_cpa_detector_start(switch_core_session_get_uuid(session), url_dup, &error_str)) {
				struct cpa_signal *cpa_signal = switch_core_alloc(pool, sizeof(*cpa_signal));
				cpa_signal->terminate = !zstr(url_params) && strstr(url_params, "terminate=true");
				cpa_signal->name = switch_core_strdup(pool, url_dup);
				switch_core_hash_insert(component->signals, cpa_signal->name, cpa_signal);
				subscribe(switch_core_session_get_uuid(session), cpa_signal->name, RAYO_JID(component));
			} else {
				free(url_dup);
				stop_cpa_detectors(component);
				RAYO_UNLOCK(component);
				RAYO_DESTROY(component);
				return iks_new_error_detailed(iq, STANZA_ERROR_INTERNAL_SERVER_ERROR, error_str);
			}

			free(url_dup);
		}
	}

	if (!have_grammar) {
		stop_cpa_detectors(component);
		RAYO_UNLOCK(component);
		RAYO_DESTROY(component);
		return iks_new_error_detailed(iq, STANZA_ERROR_BAD_REQUEST, "No grammar defined");
	}

	/* acknowledge command */
	rayo_component_send_start(RAYO_COMPONENT(component), iq);

	/* TODO hangup race condition */	
	subscribe(switch_core_session_get_uuid(session), "hangup", RAYO_JID(component));

	/* ready to forward detector events */
	component->ready = 1;

	return NULL;
}

/**
 * Load input CPA
 * @param module_interface
 * @param pool memory pool
 * @param config_file
 * @return SWITCH_STATUS_SUCCESS if successfully loaded
 */
switch_status_t rayo_cpa_component_load(switch_loadable_module_interface_t **module_interface, switch_memory_pool_t *pool, const char *config_file)
{
	rayo_actor_command_handler_add(RAT_CALL_COMPONENT, "cpa", "set:"RAYO_EXT_NS":stop", stop_cpa_component);
	switch_event_bind("rayo_cpa_component", SWITCH_EVENT_CUSTOM, "rayo::cpa", on_rayo_cpa_detector_event, NULL);
	switch_event_bind("rayo_cpa_component", SWITCH_EVENT_CHANNEL_HANGUP_COMPLETE, NULL, on_channel_hangup_complete_event, NULL);
	
	globals.pool = pool;
	switch_core_hash_init(&globals.subscribers, pool);
	switch_mutex_init(&globals.subscribers_mutex, SWITCH_MUTEX_NESTED, pool);

	return rayo_cpa_detector_load(module_interface, pool, config_file);
}

/**
 * Stop input CPA
 */
void rayo_cpa_component_shutdown(void)
{
	switch_event_unbind_callback(on_rayo_cpa_detector_event);
	switch_event_unbind_callback(on_channel_hangup_complete_event);
	rayo_cpa_detector_shutdown();
	switch_core_hash_destroy(&globals.subscribers);
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
