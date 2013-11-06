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
 * rayo_components.c -- Rayo component interface
 *
 */
#include "rayo_components.h"

#include <switch.h>
#include "mod_rayo.h"
#include "iks_helpers.h"

/**
 * Get access to Rayo component data.
 * @param id the component internal ID
 * @return the component or NULL. Call rayo_component_unlock() when done with component pointer.
 */
struct rayo_component *rayo_component_locate(const char *id, const char *file, int line)
{
	struct rayo_actor *actor = rayo_actor_locate_by_id(id, file, line);
	if (actor && is_component_actor(actor)) {
		return RAYO_COMPONENT(actor);
	} else if (actor) {
		 RAYO_UNLOCK(actor);
	}
	return NULL;
}

/**
 * Send component start reply
 * @param component the component
 * @param iq the start request
 */
void rayo_component_send_start(struct rayo_component *component, iks *iq)
{
	iks *response = iks_new_iq_result(iq);
	iks *ref = iks_insert(response, "ref");
	iks_insert_attrib(ref, "xmlns", RAYO_NS);
	iks_insert_attrib_printf(ref, "uri", "xmpp:%s", RAYO_JID(component));
	RAYO_SEND_REPLY(component, iks_find_attrib(response, "to"), response);
}

/**
 * Create component complete event
 * @param component the component
 * @param reason_str the completion reason
 * @param reason_namespace the completion reason namespace
 * @param meta metadata to add as child
 * @param child_of_complete if true metadata is child of complete instead of reason
 * @return the event
 */
iks *rayo_component_create_complete_event_with_metadata(struct rayo_component *component, const char *reason_str, const char *reason_namespace, iks *meta, int child_of_complete)
{
	iks *response = iks_new("presence");
	iks *complete;
	iks *reason;
	iks_insert_attrib(response, "from", RAYO_JID(component));
	iks_insert_attrib(response, "to", component->client_jid);
	iks_insert_attrib(response, "type", "unavailable");
	complete = iks_insert(response, "complete");
	iks_insert_attrib(complete, "xmlns", RAYO_EXT_NS);
	reason = iks_insert(complete, reason_str);
	iks_insert_attrib(reason, "xmlns", reason_namespace);
	if (meta) {
		meta = iks_copy_within(meta, iks_stack(response));
		if (child_of_complete) {
			iks_insert_node(complete, meta);
		} else {
			iks_insert_node(reason, meta);
		}
	}

	return response;
}

/**
 * Create component complete event
 * @param component the component
 * @param reason the completion reason
 * @param reason_namespace the completion reason namespace
 * @return the event
 */
iks *rayo_component_create_complete_event(struct rayo_component *component, const char *reason, const char *reason_namespace)
{
	return rayo_component_create_complete_event_with_metadata(component, reason, reason_namespace, NULL, 0);
}

/**
 * Send rayo component complete event
 */
void rayo_component_send_complete_event(struct rayo_component *component, iks *response)
{
	RAYO_SEND_REPLY(component, iks_find_attrib(response, "to"), response);
	RAYO_UNLOCK(component);
	RAYO_DESTROY(component);
}

/**
 * Send rayo complete
 */
void rayo_component_send_complete(struct rayo_component *component, const char *reason, const char *reason_namespace)
{
	rayo_component_send_complete_event(component, rayo_component_create_complete_event(component, reason, reason_namespace));
}

/**
 * Send rayo complete
 */
void rayo_component_send_complete_with_metadata(struct rayo_component *component, const char *reason, const char *reason_namespace, iks *meta, int child_of_complete)
{
	rayo_component_send_complete_event(component, rayo_component_create_complete_event_with_metadata(component, reason, reason_namespace, meta, child_of_complete));
}

/**
 * Send rayo complete
 */
void rayo_component_send_complete_with_metadata_string(struct rayo_component *component, const char *reason, const char *reason_namespace, const char *meta, int child_of_complete)
{
	iks *meta_xml = NULL;
	iksparser *p = iks_dom_new(&meta_xml);
	if (iks_parse(p, meta, 0, 1) != IKS_OK) {
		/* unexpected ... */
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "%s Failed to parse metadata for complete event: %s\n",
			RAYO_JID(component), meta);
		/* send without... */
		rayo_component_send_complete(component, reason, reason_namespace);
	} else {
		rayo_component_send_complete_with_metadata(component, reason, reason_namespace, meta_xml, child_of_complete);
	}
	if (meta_xml) {
		iks_delete(meta_xml);
	}
	iks_parser_delete(p);
}

/**
 * Background API data
 */
struct component_bg_api_cmd {
	const char *cmd;
	const char *args;
	switch_memory_pool_t *pool;
	struct rayo_component *component;
};

/**
 * Thread that outputs to component
 * @param thread this thread
 * @param obj the Rayo mixer context
 * @return NULL
 */
static void *SWITCH_THREAD_FUNC component_bg_api_thread(switch_thread_t *thread, void *obj)
{
	struct component_bg_api_cmd *cmd = (struct component_bg_api_cmd *)obj;
	switch_stream_handle_t stream = { 0 };
	switch_memory_pool_t *pool = cmd->pool;
	SWITCH_STANDARD_STREAM(stream);

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "BGAPI EXEC: %s %s\n", cmd->cmd, cmd->args);
	if (switch_api_execute(cmd->cmd, cmd->args, NULL, &stream) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "BGAPI EXEC FAILURE\n");
		rayo_component_send_complete(cmd->component, COMPONENT_COMPLETE_ERROR);
	} else {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "BGAPI EXEC RESULT: %s\n", (char *)stream.data);
	}
	switch_safe_free(stream.data);
	switch_core_destroy_memory_pool(&pool);
	return NULL;
}

/**
 * Run a background API command
 * @param cmd API command
 * @param args API args
 */
void rayo_component_api_execute_async(struct rayo_component *component, const char *cmd, const char *args)
{
	switch_thread_t *thread;
	switch_threadattr_t *thd_attr = NULL;
	struct component_bg_api_cmd *bg_cmd = NULL;
	switch_memory_pool_t *pool;

	/* set up command */
	switch_core_new_memory_pool(&pool);
	bg_cmd = switch_core_alloc(pool, sizeof(*bg_cmd));
	bg_cmd->pool = pool;
	bg_cmd->cmd = switch_core_strdup(pool, cmd);
	bg_cmd->args = switch_core_strdup(pool, args);
	bg_cmd->component = component;

	/* create thread */
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "%s BGAPI START\n", RAYO_JID(component));
	switch_threadattr_create(&thd_attr, pool);
	switch_threadattr_detach_set(thd_attr, 1);
	switch_threadattr_stacksize_set(thd_attr, SWITCH_THREAD_STACKSIZE);
	switch_thread_create(&thread, thd_attr, component_bg_api_thread, bg_cmd, pool);
}

/**
 * Handle configuration
 */
switch_status_t rayo_components_load(switch_loadable_module_interface_t **module_interface, switch_memory_pool_t *pool, const char *config_file)
{
	if (rayo_input_component_load(module_interface, pool, config_file) != SWITCH_STATUS_SUCCESS ||
		rayo_output_component_load(module_interface, pool, config_file) != SWITCH_STATUS_SUCCESS ||
		rayo_prompt_component_load(module_interface, pool, config_file) != SWITCH_STATUS_SUCCESS ||
		rayo_record_component_load(module_interface, pool, config_file) != SWITCH_STATUS_SUCCESS ||
		rayo_receivefax_component_load(module_interface, pool, config_file) != SWITCH_STATUS_SUCCESS) {
		return SWITCH_STATUS_TERM;
	}
	return SWITCH_STATUS_SUCCESS;
}

/**
 * Handle shutdown
 */
switch_status_t rayo_components_shutdown(void)
{
	rayo_input_component_shutdown();
	rayo_output_component_shutdown();
	rayo_prompt_component_shutdown();
	rayo_record_component_shutdown();
	rayo_receivefax_component_shutdown();

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
 * vim:set softtabstop=4 shiftwidth=4 tabstop=4 noet
 */
