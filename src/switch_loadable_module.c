/* 
 * FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
 * Copyright (C) 2005/2006, Anthony Minessale II <anthmct@yahoo.com>
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
 * Anthony Minessale II <anthmct@yahoo.com>
 * Portions created by the Initial Developer are Copyright (C)
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 * 
 * Anthony Minessale II <anthmct@yahoo.com>
 *
 *
 * switch_loadable_module.c -- Loadable Modules
 *
 */
#include <switch_console.h>

#ifndef SWITCH_MOD_DIR
#ifdef WIN32
#define SWITCH_MOD_DIR ".\\mod"
#else
#define SWITCH_MOD_DIR "/usr/local/freeswitch/mod"
#endif
#endif

struct switch_loadable_module {
	char *filename;
	const switch_loadable_module_interface *interface;
	void *lib;
	switch_status (*switch_module_load) (switch_loadable_module_interface **, char *);
	switch_status (*switch_module_reload) (void);
	switch_status (*switch_module_pause) (void);
	switch_status (*switch_module_resume) (void);
	switch_status (*switch_module_status) (void);
	switch_status (*switch_module_runtime) (void);
	switch_status (*switch_module_shutdown) (void);

};

struct switch_loadable_module_container {
	switch_hash *module_hash;
	switch_hash *endpoint_hash;
	switch_hash *codec_hash;
	switch_hash *dialplan_hash;
	switch_hash *timer_hash;
	switch_hash *application_hash;
	switch_hash *api_hash;
	switch_memory_pool *pool;
};

static struct switch_loadable_module_container loadable_modules;


static void *switch_loadable_module_exec(switch_thread *thread, void *obj)
{
	switch_status status = SWITCH_STATUS_SUCCESS;
	switch_core_thread_session *ts = obj;
	switch_loadable_module *module = ts->objs[0];
	int restarts;

	assert(module != NULL);

	for (restarts = 0; status != SWITCH_STATUS_TERM; restarts++) {
		status = module->switch_module_runtime();
	}
	switch_console_printf(SWITCH_CHANNEL_CONSOLE, "Thread ended for %s\n", module->interface->module_name);

	if (ts->pool) {
		switch_memory_pool *pool = ts->pool;
		switch_core_destroy_memory_pool(&pool);
	}

	return NULL;
}

static switch_status switch_loadable_module_load_file(char *filename, switch_memory_pool *pool, switch_loadable_module **new_module)
{
	switch_loadable_module *module = NULL;
	apr_dso_handle_t *dso = NULL;
	apr_status_t status;
	apr_dso_handle_sym_t function_handle = NULL;
	switch_status (*load_func_ptr) (switch_loadable_module_interface **, char *) = NULL;
	int loading = 1;
	const char *err = NULL;
	switch_loadable_module_interface *interface = NULL;
	char derr[512];

	assert(filename != NULL);

	*new_module = NULL;
	status = apr_dso_load(&dso, filename, pool);

	while (loading) {
		if (status != APR_SUCCESS) {
			apr_dso_error(dso, derr, sizeof(derr));
			err = derr;
			break;
		}

		status = apr_dso_sym(&function_handle, dso, "switch_module_load");
		load_func_ptr = function_handle;

		if (load_func_ptr == NULL) {
			err = "Cannot Load";
			break;
		}

		if (load_func_ptr(&interface, filename) != SWITCH_STATUS_SUCCESS) {
			err = "Module load routine returned an error";
			interface = NULL;
			break;
		}

		if (! (module = switch_core_permenant_alloc( sizeof(switch_loadable_module) ))) {
			err = "Could not allocate memory\n";
			break;
		}

		loading = 0;
	}

	if (err) {
		switch_console_printf(SWITCH_CHANNEL_CONSOLE, "Error Loading module %s\n**%s**\n", filename, err);
		apr_dso_unload(dso);
		return SWITCH_STATUS_GENERR;
	}

	module->filename = switch_core_permenant_strdup(filename);
	module->interface = interface;
	module->switch_module_load = load_func_ptr;

	if ((status = apr_dso_sym(&function_handle, dso, "switch_module_reload")) == APR_SUCCESS) {
		module->switch_module_reload = function_handle;
	}

	if ((status = apr_dso_sym(&function_handle, dso, "switch_module_pause")) == APR_SUCCESS) {
		module->switch_module_pause = function_handle;
	}

	if ((status = apr_dso_sym(&function_handle, dso, "switch_module_resume")) == APR_SUCCESS) {
		module->switch_module_resume = function_handle;
	}

	if ((status = apr_dso_sym(&function_handle, dso, "switch_module_status")) == APR_SUCCESS) {
		module->switch_module_status = function_handle;
	}

	if ((status = apr_dso_sym(&function_handle, dso, "switch_module_shutdown")) == APR_SUCCESS) {
		module->switch_module_shutdown = function_handle;
	}

	if ((status = apr_dso_sym(&function_handle, dso, "switch_module_runtime")) == APR_SUCCESS) {
		module->switch_module_runtime = function_handle;
	}

	module->lib = dso;

	if (module->switch_module_runtime) {
		switch_core_launch_thread(switch_loadable_module_exec, module);
	}

	*new_module = module;
	switch_console_printf(SWITCH_CHANNEL_CONSOLE, "Successfully Loaded [%s]\n", interface->module_name);

	return SWITCH_STATUS_SUCCESS;

}

SWITCH_DECLARE(switch_status) switch_loadable_module_init()
{

	char *file;
	size_t len;
	char *ptr;
	apr_finfo_t finfo;
	apr_dir_t *module_dir_handle;
	apr_int32_t finfo_flags = APR_FINFO_DIRENT|APR_FINFO_TYPE|APR_FINFO_NAME;
	switch_loadable_module *new_module;
#ifdef WIN32
	const char *ext = ".dll";
	const char *EXT = ".DLL";
#else
	const char *ext = ".so";
	const char *EXT = ".SO";
#endif

	memset(&loadable_modules, 0, sizeof(loadable_modules));
	switch_core_new_memory_pool(&loadable_modules.pool);

	if (apr_dir_open(&module_dir_handle, SWITCH_MOD_DIR, loadable_modules.pool) != APR_SUCCESS) {
		switch_console_printf(SWITCH_CHANNEL_CONSOLE, "Can't open directory: %s\n", SWITCH_MOD_DIR);
		return SWITCH_STATUS_GENERR;
	}

	switch_core_hash_init(&loadable_modules.module_hash, loadable_modules.pool);
	switch_core_hash_init(&loadable_modules.endpoint_hash, loadable_modules.pool);
	switch_core_hash_init(&loadable_modules.codec_hash, loadable_modules.pool);
	switch_core_hash_init(&loadable_modules.timer_hash, loadable_modules.pool);
	switch_core_hash_init(&loadable_modules.application_hash, loadable_modules.pool);
	switch_core_hash_init(&loadable_modules.api_hash, loadable_modules.pool);
	switch_core_hash_init(&loadable_modules.dialplan_hash, loadable_modules.pool);

	while (apr_dir_read(&finfo, finfo_flags, module_dir_handle) == APR_SUCCESS) {
		const char *fname = finfo.fname;

		if (finfo.filetype != APR_REG) {
			continue;
		}

		if (!fname) {
			fname = finfo.name;
		}

		if (!(ptr = (char *) fname)) {
			continue;
		}
	
		if (!strstr(fname, ext) && !strstr(fname, EXT)) {
			continue;
		}


		len = strlen(SWITCH_MOD_DIR) + strlen(fname) + 3;
		file = (char *) switch_core_alloc(loadable_modules.pool, len);
		snprintf(file, len, "%s/%s", SWITCH_MOD_DIR, fname);

		if (switch_loadable_module_load_file(file, loadable_modules.pool, &new_module) == SWITCH_STATUS_SUCCESS) {
			switch_core_hash_insert(loadable_modules.module_hash, (char *) fname, new_module);

			if (new_module->interface->endpoint_interface) {
				const switch_endpoint_interface *ptr;
				for (ptr = new_module->interface->endpoint_interface; ptr; ptr = ptr->next) {
					switch_console_printf(SWITCH_CHANNEL_CONSOLE, "Adding Endpoint '%s'\n", ptr->interface_name);
					switch_core_hash_insert(loadable_modules.endpoint_hash,
											(char *) ptr->interface_name,
											(void *) ptr);
				}
			}
			
			if (new_module->interface->codec_interface) {
				const switch_codec_implementation *impl;
				const switch_codec_interface *ptr;

				for(ptr = new_module->interface->codec_interface; ptr; ptr = ptr->next) {
					for(impl = ptr->implementations; impl ; impl = impl->next) {
						switch_console_printf(SWITCH_CHANNEL_CONSOLE,
											  "Adding Codec '%s' (%s) %dkhz %dms\n",
											  ptr->iananame,
											  ptr->interface_name,
											  impl->samples_per_second,
											  impl->microseconds_per_frame / 1000);
					}

					switch_core_hash_insert(loadable_modules.codec_hash,
											(char *) ptr->iananame,
											(void *) ptr);
				}
			}

			if (new_module->interface->dialplan_interface) {
				const switch_dialplan_interface *ptr;

				for(ptr = new_module->interface->dialplan_interface; ptr; ptr = ptr->next) {
					switch_console_printf(SWITCH_CHANNEL_CONSOLE, "Adding Dialplan '%s'\n", ptr->interface_name);
					switch_core_hash_insert(loadable_modules.dialplan_hash,
											(char *) ptr->interface_name,
											(void *) ptr);
				}
			}

			if (new_module->interface->timer_interface) {
				const switch_timer_interface *ptr;

				for(ptr = new_module->interface->timer_interface; ptr; ptr = ptr->next) {
					switch_console_printf(SWITCH_CHANNEL_CONSOLE, "Adding Timer '%s'\n", ptr->interface_name);
					switch_core_hash_insert(loadable_modules.timer_hash,
											(char *) ptr->interface_name,
											(void *) ptr);
				}
			}

			if (new_module->interface->application_interface) {
				const switch_application_interface *ptr;

				for(ptr = new_module->interface->application_interface; ptr; ptr = ptr->next) {
					switch_console_printf(SWITCH_CHANNEL_CONSOLE, "Adding Application '%s'\n", ptr->interface_name);
					switch_core_hash_insert(loadable_modules.application_hash,
											(char *) ptr->interface_name,
											(void *) ptr);
				}
			}

			if (new_module->interface->api_interface) {
				const switch_api_interface *ptr;

				for(ptr = new_module->interface->api_interface; ptr; ptr = ptr->next) {
					switch_console_printf(SWITCH_CHANNEL_CONSOLE, "Adding API Function '%s'\n", ptr->interface_name);
					switch_core_hash_insert(loadable_modules.api_hash,
											(char *) ptr->interface_name,
											(void *) ptr);
				}
			}

		}

	}
	apr_dir_close(module_dir_handle);

	return SWITCH_STATUS_SUCCESS;
}

SWITCH_DECLARE(void) loadable_module_shutdown(void)
{
	switch_hash_index_t* hi;
	void *val;
	switch_loadable_module *module;

	for (hi = switch_hash_first(loadable_modules.pool, loadable_modules.module_hash); hi; hi = switch_hash_next(hi)) {
		switch_hash_this(hi, NULL, NULL, &val);
		module = (switch_loadable_module *) val;
		switch_console_printf(SWITCH_CHANNEL_CONSOLE, "Checking %s\t", module->interface->module_name);
		if (module->switch_module_shutdown) {
			switch_console_printf(SWITCH_CHANNEL_CONSOLE_CLEAN, "(yes)\n", module->interface->module_name);
			module->switch_module_shutdown();
		} else {
			switch_console_printf(SWITCH_CHANNEL_CONSOLE_CLEAN, "(no)\n", module->interface->module_name);
		}
	}

}

SWITCH_DECLARE(switch_endpoint_interface *) loadable_module_get_endpoint_interface(char *name)
{
	return switch_core_hash_find(loadable_modules.endpoint_hash, name);
}

SWITCH_DECLARE(switch_codec_interface *) loadable_module_get_codec_interface(char *name)
{
	return switch_core_hash_find(loadable_modules.codec_hash, name);
}

SWITCH_DECLARE(switch_dialplan_interface *) loadable_module_get_dialplan_interface(char *name)
{
	return switch_core_hash_find(loadable_modules.dialplan_hash, name);
}

SWITCH_DECLARE(switch_timer_interface *) loadable_module_get_timer_interface(char *name)
{
	return switch_core_hash_find(loadable_modules.timer_hash, name);
}

SWITCH_DECLARE(switch_application_interface *) loadable_module_get_application_interface(char *name)
{
	return switch_core_hash_find(loadable_modules.application_hash, name);
}

SWITCH_DECLARE(switch_api_interface *) loadable_module_get_api_interface(char *name)
{
	return switch_core_hash_find(loadable_modules.api_hash, name);
}

SWITCH_DECLARE(int) loadable_module_get_codecs(switch_memory_pool *pool, switch_codec_interface **array, int arraylen)
{
	switch_hash_index_t* hi;
	void *val;
	int i = 0;

	for (hi = switch_hash_first(pool, loadable_modules.codec_hash); hi; hi = switch_hash_next(hi)) {
		switch_hash_this(hi, NULL, NULL, &val);
		array[i++] = val;
		if (i > arraylen) {
			break;
		}
	}

	return i;

}

SWITCH_DECLARE(int) loadable_module_get_codecs_sorted(switch_memory_pool *pool, switch_codec_interface **array, int arraylen, char **prefs, int preflen)
{
	int x, i = 0;
	switch_codec_interface *codec_interface;

	for(x = 0; x < preflen; x++) {
		if ((codec_interface = loadable_module_get_codec_interface(prefs[x]))) {
			array[i++] = codec_interface;
		}
	}

	return i;
}

SWITCH_DECLARE(switch_status) switch_api_execute(char *cmd, char *arg, char *retbuf, size_t len)
{
	switch_api_interface *api;
	switch_status status;
	switch_event *event;

	if ((api = loadable_module_get_api_interface(cmd))) {
		status = api->function(arg, retbuf, len);
	} else {
		status = SWITCH_STATUS_FALSE;
		snprintf(retbuf, len, "INVALID COMMAND [%s]", cmd);
	}

	if (switch_event_create(&event, SWITCH_EVENT_API) == SWITCH_STATUS_SUCCESS) {
		if (cmd) {
			switch_event_add_header(event, SWITCH_STACK_BOTTOM, "API-Command", cmd);
		}
		if (arg) {
			switch_event_add_header(event, SWITCH_STACK_BOTTOM, "API-Command-Arguement", arg);
		}
		switch_event_add_body(event, retbuf);
		switch_event_fire(&event);
	}


	return status;
}
