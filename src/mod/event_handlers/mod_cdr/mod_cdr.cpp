/*
 * FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application Call Detail Recorder module
 * Copyright 2006, Author: Yossi Neiman of Cartis Solutions, Inc. <freeswitch AT cartissolutions.com>
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
 * The Original Code is FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application Call Detail Recorder module
 *
 * The Initial Developer of the Original Code is
 * Yossi Neiman <freeswitch AT cartissolutions.com>
 * Portions created by the Initial Developer are Copyright (C)
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 * 
 * Yossi Neiman <freeswitch AT cartissolutions.com>
 * Marcel Barbulescu <marcelbarbulescu@gmail.com>
 *
 * Description: This source file describes the most basic portions of the CDR module.  These are the functions
 * and structures that the Freeswitch core looks for when opening up the DSO file to create the load, shutdown
 * and runtime threads as necessary.
 *
 * mod_cdr.cpp
 *
 */

#include "cdrcontainer.h"
#include <switch.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

static const char modname[] = "mod_cdr - CDR Engine";
static int RUNNING = 0;
static CDRContainer *newcdrcontainer;
static switch_memory_pool_t *module_pool;
static switch_status_t my_on_hangup(switch_core_session_t *session);
static switch_status_t modcdr_reload(const char *dest, switch_core_session_t *isession, switch_stream_handle_t *stream);
static switch_status_t modcdr_queue_pause(const char *dest, switch_core_session_t *isession, switch_stream_handle_t *stream);
static switch_status_t modcdr_queue_resume(const char *dest, switch_core_session_t *isession, switch_stream_handle_t *stream);
static switch_status_t modcdr_show_active(const char *dest, switch_core_session_t *isession, switch_stream_handle_t *stream);
static switch_status_t modcdr_show_available(const char *dest, switch_core_session_t *isession, switch_stream_handle_t *stream);
static switch_thread_rwlock_t *cdr_rwlock;

/* Now begins the glue that will tie this into the system.
*/

static const switch_state_handler_table_t state_handlers = {
	/*.on_init */ NULL,
	/*.on_ring */ NULL,
	/*.on_execute */ NULL,
	/*.on_hangup */ my_on_hangup,
	/*.on_loopback */ NULL,
	/*.on_transmit */ NULL
};

static switch_api_interface_t modcdr_show_available_api = {
	/*.interface_name */ "modcdr_show_available",
	/*.desc */ "Displays the currently compiled-in mod_cdr backend loggers.",
	/*.function */ modcdr_show_available,
	/*.syntax */ "modcdr_queue_show_available",
	/*.next */ 0
};

static switch_api_interface_t modcdr_show_active_api = {
	/*.interface_name */ "modcdr_show_active",
	/*.desc */ "Displays the currently active mod_cdr backend loggers.",
	/*.function */ modcdr_show_active,
	/*.syntax */ "modcdr_queue_show_active",
	/*.next */ &modcdr_show_available_api
};

static switch_api_interface_t modcdr_queue_resume_api = {
	/*.interface_name */ "modcdr_queue_resume",
	/*.desc */ "Manually resumes the popping of objects from the queue.",
	/*.function */ modcdr_queue_resume,
	/*.syntax */ "modcdr_queue_resume",
	/*.next */ &modcdr_show_active_api
};

static switch_api_interface_t modcdr_queue_pause_api = {
	/*.interface_name */ "modcdr_queue_pause",
	/*.desc */ "Manually pauses the popping of objects from the queue. (DANGER: Can suck your memory away rather quickly.)",
	/*.function */ modcdr_queue_pause,
	/*.syntax */ "modcdr_queue_pause",
	/*.next */ &modcdr_queue_resume_api
};

static switch_api_interface_t modcdr_reload_interface_api = {
	/*.interface_name */ "modcdr_reload",
	/*.desc */ "Reload mod_cdr's configuration",
	/*.function */ modcdr_reload,
	/*.syntax */ "modcdr_reload",
	/*.next */ &modcdr_queue_pause_api
};

static const switch_loadable_module_interface_t cdr_module_interface = {
	/*.module_name */ modname,
	/*.endpoint_interface */ NULL,
	/*.timer_interface */ NULL,
	/*.dialplan_interface */ NULL,
	/*.codec_interface */ NULL,
	/*.application_interface */ NULL,
	/* api_interface */ &modcdr_reload_interface_api
};

static switch_status_t my_on_hangup(switch_core_session_t *session)
{
	switch_thread_rwlock_rdlock(cdr_rwlock);
	newcdrcontainer->add_cdr(session);
	switch_thread_rwlock_unlock(cdr_rwlock);
	return SWITCH_STATUS_SUCCESS;
}

SWITCH_MOD_DECLARE(switch_status_t) switch_module_load(const switch_loadable_module_interface **interface, char *filename)
{
	/* connect my internal structure to the blank pointer passed to me */
	*interface = &cdr_module_interface;
	
	switch_core_add_state_handler(&state_handlers);
	
	if (switch_core_new_memory_pool(&module_pool) != SWITCH_STATUS_SUCCESS) 
	{
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "OH OH - Can't swim, no pool\n");
		return SWITCH_STATUS_TERM;
	}

	switch_thread_rwlock_create(&cdr_rwlock,module_pool);
	newcdrcontainer = new CDRContainer(module_pool);  // Instantiates the new object, automatically loads config
	
	/* indicate that the module should continue to be loaded */
	return SWITCH_STATUS_SUCCESS;
}

SWITCH_MOD_DECLARE(switch_status_t) switch_module_runtime(void)
{
	RUNNING = 1;
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "mod_cdr made it to runtime.  Wee!\n");
	newcdrcontainer->process_records();
	
	return RUNNING ? SWITCH_STATUS_SUCCESS : SWITCH_STATUS_TERM;
}

SWITCH_STANDARD_API(modcdr_reload)
{
#ifdef SWITCH_QUEUE_ENHANCED
	switch_thread_rwlock_wrlock(cdr_rwlock);
	newcdrcontainer->reload(stream);
	switch_thread_rwlock_unlock(cdr_rwlock);
	stream->write_function(stream, "XML Reloaded and mod_cdr reloaded.\n");
#else
	stream->write_function(stream,"modcdr_reload is only supported with the apr_queue_t enhancements and SWITCH_QUEUE_ENHANCED defined.\n");
#endif
	return SWITCH_STATUS_SUCCESS;
}

SWITCH_STANDARD_API(modcdr_queue_pause)
{
#ifdef SWITCH_QUEUE_ENHANCED
	newcdrcontainer->queue_pause(stream);
#else
	stream->write_function(stream,"modcdr_queue_pause is only supported with the apr_queue_t enhancements and SWITCH_QUEUE_ENHANCED defined.\n");
#endif
	return SWITCH_STATUS_SUCCESS;
}

SWITCH_STANDARD_API(modcdr_queue_resume)
{
#ifdef SWITCH_QUEUE_ENHANCED
	newcdrcontainer->queue_resume(stream);
#else
	stream->write_function(stream,"modcdr_queue_pause is only supported with the apr_queue_t enhancements and SWITCH_QUEUE_ENHANCED defined.\n");
#endif
	return SWITCH_STATUS_SUCCESS;
}

SWITCH_STANDARD_API(modcdr_show_active)
{
	newcdrcontainer->active(stream);
	return SWITCH_STATUS_SUCCESS;
}

SWITCH_STANDARD_API(modcdr_show_available)
{
	newcdrcontainer->available(stream);
	return SWITCH_STATUS_SUCCESS;
}

SWITCH_MOD_DECLARE(switch_status_t) switch_module_shutdown(void)
{
	delete newcdrcontainer;
	switch_thread_rwlock_destroy(cdr_rwlock);
	return SWITCH_STATUS_SUCCESS;
}

/* For Emacs:
 * Local Variables:
 * mode:c++
 * indent-tabs-mode:t
 * tab-width:4
 * c-basic-offset:4
 * End:
 * For VIM:
 * vim:set softtabstop=4 shiftwidth=4 tabstop=4 expandtab:
 */
