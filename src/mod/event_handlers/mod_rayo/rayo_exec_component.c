/*
 * mod_rayo for FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
 * Copyright (C) 2013-2018, Grasshopper
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
 * rayo_exec_component.c -- Rayo call application execution component
 *
 */
#include "rayo_components.h"
#include "rayo_elements.h"


/**
 * An exec component
 */
struct exec_component {
	/** component base class */
	struct rayo_component base;
	/** Dialplan app */
	const char *app;
	/** Dialplan app args */
	char *args;
};

#define EXEC_COMPONENT(x) ((struct exec_component *)x)

/**
 * Wrapper for executing dialplan app
 */
SWITCH_STANDARD_APP(rayo_app_exec)
{
	if (!zstr(data)) {
		struct rayo_component *component = RAYO_COMPONENT_LOCATE(data);
		if (component) {
			switch_status_t status;
			switch_channel_set_variable(switch_core_session_get_channel(session), SWITCH_CURRENT_APPLICATION_RESPONSE_VARIABLE, "");
			status = switch_core_session_execute_application(session, EXEC_COMPONENT(component)->app, EXEC_COMPONENT(component)->args);
			if (status != SWITCH_STATUS_SUCCESS) {
				rayo_component_send_complete(component, COMPONENT_COMPLETE_ERROR);
			} else {
				const char *resp = switch_channel_get_variable(switch_core_session_get_channel(session), SWITCH_CURRENT_APPLICATION_RESPONSE_VARIABLE);
				if (zstr(resp)) {
					rayo_component_send_complete(component, COMPONENT_COMPLETE_DONE);
				} else {
					/* send complete event to client */
					iks *response = iks_new("app");
					iks_insert_attrib(response, "xmlns", RAYO_EXEC_COMPLETE_NS);
					iks_insert_attrib(response, "response", resp);
					rayo_component_send_complete_with_metadata(component, COMPONENT_COMPLETE_DONE, response, 1);
					iks_delete(response);
				}
			}
			RAYO_RELEASE(component);
		}
	} else {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "Missing rayo exec component JID\n");
	}
	switch_channel_set_variable(switch_core_session_get_channel(session), SWITCH_CURRENT_APPLICATION_RESPONSE_VARIABLE, "");
}

/**
 * Create a record component
 */
static struct rayo_component *exec_component_create(struct rayo_actor *call, const char *client_jid, iks *exec)
{
	switch_memory_pool_t *pool;
	struct exec_component *exec_component = NULL;

	switch_core_new_memory_pool(&pool);
	exec_component = switch_core_alloc(pool, sizeof(*exec_component));
	exec_component = EXEC_COMPONENT(rayo_component_init(RAYO_COMPONENT(exec_component), pool, RAT_CALL_COMPONENT, "exec", NULL, call, client_jid));
	if (exec_component) {
		exec_component->app = switch_core_strdup(pool, iks_find_attrib_soft(exec, "app"));
		exec_component->args = switch_core_strdup(pool, iks_find_attrib_soft(exec, "args"));
	} else {
		switch_core_destroy_memory_pool(&pool);
		return NULL;
	}

	return RAYO_COMPONENT(exec_component);
}

/**
 * Execute dialplan APP on rayo call
 */
static iks *start_exec_app_component(struct rayo_actor *call, struct rayo_message *msg, void *data)
{
	iks *iq = msg->payload;
	iks *exec = iks_find(iq, "app");
	struct rayo_component *exec_component = NULL;
	switch_core_session_t *session = NULL;

	/* validate record attributes */
	if (!VALIDATE_RAYO_APP(exec)) {
		return iks_new_error(iq, STANZA_ERROR_BAD_REQUEST);
	}

	exec_component = exec_component_create(call, iks_find_attrib(iq, "from"), exec);
	if (!exec_component) {
		return iks_new_error_detailed(iq, STANZA_ERROR_INTERNAL_SERVER_ERROR, "Failed to create exec entity");
	}

	session = switch_core_session_locate(call->id);
	if (session) {
		if (switch_core_session_execute_application_async(session, switch_core_session_strdup(session, "rayo-app-exec"), switch_core_session_strdup(session, RAYO_JID(exec_component))) != SWITCH_STATUS_SUCCESS) {
			switch_core_session_rwunlock(session);
			RAYO_RELEASE(exec_component);
			RAYO_DESTROY(exec_component);
			return iks_new_error_detailed(iq, STANZA_ERROR_INTERNAL_SERVER_ERROR, "failed to execute app");
		}
		switch_core_session_rwunlock(session);
	} else {
		RAYO_RELEASE(exec_component);
		RAYO_DESTROY(exec_component);
		return iks_new_error_detailed(iq, STANZA_ERROR_INTERNAL_SERVER_ERROR, "Call is gone");
	}

	rayo_component_send_start(exec_component, iq);

	return NULL;
}

/**
 * Initialize exec component
 * @param module_interface
 * @param pool memory pool to allocate from
 * @param config_file to use
 * @return SWITCH_STATUS_SUCCESS if successful
 */
switch_status_t rayo_exec_component_load(switch_loadable_module_interface_t **module_interface, switch_memory_pool_t *pool, const char *config_file)
{
	switch_application_interface_t *app_interface;
	SWITCH_ADD_APP(app_interface, "rayo-app-exec", "Wrapper dialplan app for internal use only", "", rayo_app_exec, "", SAF_SUPPORT_NOMEDIA | SAF_ZOMBIE_EXEC);
	rayo_actor_command_handler_add(RAT_CALL, "", "set:"RAYO_EXEC_NS":app", start_exec_app_component);
	return SWITCH_STATUS_SUCCESS;
}

/**
 * Shutdown exec component
 * @return SWITCH_STATUS_SUCCESS if successful
 */
switch_status_t rayo_exec_component_shutdown(void)
{
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
