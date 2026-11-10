/*
 * FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
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
 *
 * mod_logfile_prefix.h -- Structured logfile prefix formatting
 *
 */

#ifndef MOD_LOGFILE_PREFIX_H
#define MOD_LOGFILE_PREFIX_H

#include <switch.h>

typedef struct mod_logfile_prefix_config_s {
	switch_bool_t log_uuid;
	switch_bool_t log_tags;
	switch_event_t *channel_vars;
} mod_logfile_prefix_config_t;

typedef switch_status_t (*mod_logfile_write_callback_t)(void *context, const char *data, switch_size_t *length);
typedef switch_status_t (*mod_logfile_reopen_callback_t)(void *context);

void mod_logfile_prefix_config_init(mod_logfile_prefix_config_t *config);
void mod_logfile_prefix_config_destroy(mod_logfile_prefix_config_t *config);
switch_status_t mod_logfile_prefix_add_channel_vars(mod_logfile_prefix_config_t *config, const char *data);
char *mod_logfile_prefix_build(const mod_logfile_prefix_config_t *config, const switch_log_node_t *node);
char *mod_logfile_prefix_lines(const char *prefix, const char *data);
switch_status_t mod_logfile_complete_write(void *context, const char *data, switch_size_t length,
	mod_logfile_write_callback_t write_callback, mod_logfile_reopen_callback_t reopen_callback,
	switch_size_t *committed);

#ifdef MOD_LOGFILE_PREFIX_TEST
void mod_logfile_prefix_test_fail_allocation_after(int successful_allocations);
void mod_logfile_prefix_test_reset_failures(void);
#endif

#endif
