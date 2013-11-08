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
 * components.c -- Rayo component implementations
 *
 */
#ifndef RAYO_COMPONENTS_H
#define RAYO_COMPONENTS_H

#include <switch.h>
#include <iksemel.h>

#include "mod_rayo.h"

#define RAYO_EXT_NS RAYO_BASE "ext:" RAYO_VERSION
#define RAYO_EXT_COMPLETE_NS RAYO_BASE "ext:complete:" RAYO_VERSION

#define RAYO_OUTPUT_NS RAYO_BASE "output:" RAYO_VERSION
#define RAYO_OUTPUT_COMPLETE_NS RAYO_BASE "output:complete:" RAYO_VERSION

#define RAYO_INPUT_NS RAYO_BASE "input:" RAYO_VERSION
#define RAYO_INPUT_COMPLETE_NS RAYO_BASE "input:complete:" RAYO_VERSION

#define RAYO_RECORD_NS RAYO_BASE "record:" RAYO_VERSION
#define RAYO_RECORD_COMPLETE_NS RAYO_BASE "record:complete:" RAYO_VERSION

#define RAYO_PROMPT_NS RAYO_BASE "prompt:" RAYO_VERSION
#define RAYO_PROMPT_COMPLETE_NS RAYO_BASE "prompt:complete:" RAYO_VERSION

#define RAYO_FAX_NS RAYO_BASE "fax:" RAYO_VERSION
#define RAYO_FAX_COMPLETE_NS RAYO_BASE "fax:complete:" RAYO_VERSION

#define COMPONENT_COMPLETE_STOP "stop", RAYO_EXT_COMPLETE_NS
#define COMPONENT_COMPLETE_ERROR "error", RAYO_EXT_COMPLETE_NS
#define COMPONENT_COMPLETE_HANGUP "hangup", RAYO_EXT_COMPLETE_NS

extern switch_status_t rayo_components_load(switch_loadable_module_interface_t **module_interface, switch_memory_pool_t *pool, const char *config_file);
extern switch_status_t rayo_input_component_load(switch_loadable_module_interface_t **module_interface, switch_memory_pool_t *pool, const char *config_file);
extern switch_status_t rayo_output_component_load(switch_loadable_module_interface_t **module_interface, switch_memory_pool_t *pool, const char *config_file);
extern switch_status_t rayo_prompt_component_load(switch_loadable_module_interface_t **module_interface, switch_memory_pool_t *pool, const char *config_file);
extern switch_status_t rayo_record_component_load(switch_loadable_module_interface_t **module_interface, switch_memory_pool_t *pool, const char *config_file);
extern switch_status_t rayo_fax_components_load(switch_loadable_module_interface_t **module_interface, switch_memory_pool_t *pool, const char *config_file);

extern switch_status_t rayo_components_shutdown(void);
extern switch_status_t rayo_input_component_shutdown(void);
extern switch_status_t rayo_output_component_shutdown(void);
extern switch_status_t rayo_prompt_component_shutdown(void);
extern switch_status_t rayo_record_component_shutdown(void);
extern switch_status_t rayo_fax_components_shutdown(void);

extern void rayo_component_send_start(struct rayo_component *component, iks *iq);
extern void rayo_component_send_iq_error(struct rayo_component *component, iks *iq, const char *error_name, const char *error_type);
extern void rayo_component_send_iq_error_detailed(struct rayo_component *component, iks *iq, const char *error_name, const char *error_type, const char *detail);
extern void rayo_component_send_complete(struct rayo_component *component, const char *reason, const char *reason_namespace);
extern void rayo_component_send_complete_event(struct rayo_component *component, iks *response);
extern void rayo_component_send_complete_with_metadata(struct rayo_component *component, const char *reason, const char *reason_namespace, iks *meta, int child_of_complete);
extern void rayo_component_send_complete_with_metadata_string(struct rayo_component *component, const char *reason, const char *reason_namespace, const char *meta, int child_of_complete);

extern iks *rayo_component_create_complete_event(struct rayo_component *component, const char *reason, const char *reason_namespace);
extern iks *rayo_component_create_complete_event_with_metadata(struct rayo_component *component, const char *reason, const char *reason_namespace, iks *meta, int child_of_complete);

extern void rayo_component_api_execute_async(struct rayo_component *component, const char *cmd, const char *args);

#define RAYO_COMPONENT_LOCATE(id) rayo_component_locate(id, __FILE__, __LINE__)
extern struct rayo_component *rayo_component_locate(const char *id, const char *file, int line);

#endif

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

