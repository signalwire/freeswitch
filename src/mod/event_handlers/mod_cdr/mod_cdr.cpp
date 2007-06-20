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

SWITCH_MODULE_LOAD_FUNCTION(mod_cdr_load);
SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_cdr_shutdown);
SWITCH_MODULE_RUNTIME_FUNCTION(mod_cdr_runtime);
SWITCH_MODULE_DEFINITION(mod_cdr, mod_cdr_load, mod_cdr_shutdown, mod_cdr_runtime);

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

static switch_status_t my_on_hangup(switch_core_session_t *session)
{
	switch_thread_rwlock_rdlock(cdr_rwlock);
	newcdrcontainer->add_cdr(session);
	switch_thread_rwlock_unlock(cdr_rwlock);
	return SWITCH_STATUS_SUCCESS;
}

#define AVAIL_DESCR "Displays the currently compiled-in mod_cdr backend loggers."
#define ACTIVE_DESCR "Displays the currently active mod_cdr backend loggers."
#define RESUME_DESCR "Manually resumes the popping of objects from the queue."
#define PAUSE_DESCR "Manually pauses the popping of objects from the queue. (DANGER: Can suck your memory away rather quickly.)"
SWITCH_MODULE_LOAD_FUNCTION(mod_cdr_load)
{
	switch_api_interface_t *api_interface;

	/* connect my internal structure to the blank pointer passed to me */
	*module_interface = switch_loadable_module_create_module_interface(pool, modname);
	SWITCH_ADD_API(api_interface, "modcdr_reload", "Reload mod_cdr's configuration", modcdr_reload, "");
	SWITCH_ADD_API(api_interface, "modcdr_queue_pause", PAUSE_DESCR, modcdr_queue_pause, "");
	SWITCH_ADD_API(api_interface, "modcdr_queue_resume", RESUME_DESCR, modcdr_queue_resume, "");
	SWITCH_ADD_API(api_interface, "modcdr_show_active", ACTIVE_DESCR, modcdr_show_active, "");
	SWITCH_ADD_API(api_interface, "modcdr_show_available", AVAIL_DESCR, modcdr_show_available, "");
	
	switch_core_add_state_handler(&state_handlers);
	
	module_pool = pool;

	switch_thread_rwlock_create(&cdr_rwlock,module_pool);
	newcdrcontainer = new CDRContainer(module_pool);  // Instantiates the new object, automatically loads config
	
	/* indicate that the module should continue to be loaded */
	return SWITCH_STATUS_SUCCESS;
}

SWITCH_MODULE_RUNTIME_FUNCTION(mod_cdr_runtime)
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

SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_cdr_shutdown)
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
