/* 
 * FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
 * Copyright (C) 2006, James Martelletti <james@nerdc0re.com>
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
 * James Martelletti <james@nerdc0re.com>
 * Portions created by the Initial Developer are Copyright (C)
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 * 
 * James Martelletti <james@nerdc0re.com>
 *
 *
 * mod_mono.c -- Embedded mono runtime.
 *
 */
#include <mono/jit/jit.h>
#include <mono/metadata/assembly.h>
#include <mono/metadata/environment.h>
#include <mono/metadata/tokentype.h>
#include <mono/metadata/debug-helpers.h>
#include <mono/metadata/image.h>
#include <mono/metadata/threads.h>
//#include <gc/gc.h>
#include <switch.h>

#define SWITCH_MONO_MODULES  "mono/" 
#define SWITCH_MONO_LIBDIR   "lib/"
#define SWITCH_MONO_ASSEMBLY "FreeSwitch.dll"

/* Module functions */
switch_status_t mod_mono_load_modules(const char *module_dir);
MonoClass* mod_mono_find_assembly_class(MonoImage *image);

/* Managed functions */
void mod_mono_switch_console_printf(switch_text_channel_t channel, char *file, const char *func, int line, char *fmt, char *msg);

static const char modname[] = "mod_mono";
static switch_memory_pool_t *mono_pool = NULL;

static struct {
	MonoDomain *domain;
	switch_hash_t *plugins;
} globals;

typedef struct {
	MonoAssembly *assembly;
	MonoClass *class;
	MonoObject *object;	
} mono_plugin;

static switch_loadable_module_interface_t mono_module_interface = {
	/*.module_name */ modname,
};

/*
 * Load mod_mono module and initialise domain
 *
 * This function will initialise the memory pool and plugin hash for this module,
 * it will then initialise a new mono domain.
 */
SWITCH_MOD_DECLARE(switch_status_t) switch_module_load(const switch_loadable_module_interface_t **interface, char *filename)
{
	*interface = &mono_module_interface;

	/* Initialise memory pool */
	if (switch_core_new_memory_pool(&mono_pool) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Could not allocate memory pool\n");
		return SWITCH_STATUS_FALSE;
	}
	
	/* Initialise plugin hash */
	if (switch_core_hash_init(&globals.plugins, mono_pool) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Could not initialise plugins hash\n");
		return SWITCH_STATUS_FALSE;
	}		
	
	/* Construct the path to the FreeSwitch assembly, then check to make sure it exists */
	switch_size_t assembly_dir_len  = strlen(SWITCH_GLOBAL_dirs.base_dir) + strlen(SWITCH_MONO_LIBDIR) + 2; /* Account for / and \0 */
	switch_size_t assembly_file_len = assembly_dir_len + strlen(SWITCH_MONO_ASSEMBLY);
	char *assembly_dir  = (char *) switch_core_alloc(mono_pool, assembly_dir_len);
	char *assembly_file = (char *) switch_core_alloc(mono_pool, assembly_file_len);

	apr_finfo_t *assembly_finfo = (apr_finfo_t *) switch_core_alloc(mono_pool, sizeof(*assembly_finfo));

	snprintf(assembly_dir, assembly_dir_len, "%s/%s", SWITCH_GLOBAL_dirs.base_dir, SWITCH_MONO_LIBDIR);
	snprintf(assembly_file, assembly_file_len, "%s/%s%s", SWITCH_GLOBAL_dirs.base_dir, SWITCH_MONO_LIBDIR, SWITCH_MONO_ASSEMBLY);

	if (apr_stat(assembly_finfo, assembly_file, 0, mono_pool) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Could not find FreeSwitch.NET assembly\n");
		return SWITCH_STATUS_FALSE;
	}

	/* Mono wants to know where it will be able to find the FreeSwitch assembly if it's not in the GAC */
	if (getenv("MONO_PATH") != NULL) {
		switch_size_t mono_path_len = strlen(getenv("MONO_PATH")) + strlen(assembly_dir) + 2; /* Account for : and \0 */ 
		char *mono_path = (char *) switch_core_alloc(mono_pool, mono_path_len);
	
		snprintf(mono_path, mono_path_len, "%s:%s", getenv("MONO_PATH"), assembly_dir);

		setenv("MONO_PATH", mono_path, 1);
	}
	else
		setenv("MONO_PATH", assembly_dir, 1);

	/* Now find where our managed modules are */
	switch_size_t module_dir_len = strlen(SWITCH_GLOBAL_dirs.mod_dir) + strlen(SWITCH_MONO_MODULES) + 2; /* Account for / and \0 */
	char *module_dir = (char *) switch_core_alloc(mono_pool, module_dir_len);

	snprintf(module_dir, module_dir_len, "%s/%s", SWITCH_GLOBAL_dirs.mod_dir, SWITCH_MONO_MODULES);
	
	/* Initialise the mono domain */
	if (!(globals.domain = mono_jit_init("freeswitch"))) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Error initialising mono runtime\n");
		return SWITCH_STATUS_FALSE;
	}

	/* Let user know everything initialised fine */
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Initialised mono runtime\n");

	/* Load our modules */
	if (mod_mono_load_modules(module_dir) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Error loading modules\n");
		return SWITCH_STATUS_FALSE;
	}

	/* Finally, go through and initialise each plugin before returning SUCCESS */ 
	switch_hash_index_t *p = NULL;

	for (p = switch_hash_first(mono_pool, globals.plugins); p; p = switch_hash_next(p)) {
		mono_plugin *plugin = (mono_plugin *) switch_core_alloc(mono_pool, sizeof(*plugin));
		apr_ssize_t *key_length = NULL;
		const void *key = NULL;
		void *value = NULL;

		switch_hash_this(p, &key, key_length, &value);
		plugin = (mono_plugin *) value;
		
		mono_runtime_object_init(plugin->object);
	}
	
	return SWITCH_STATUS_SUCCESS;
}

/*
 * Function for cleanly shutting down mod_mono
 * 
 */
SWITCH_MOD_DECLARE(switch_status_t) switch_module_shutdown(void)
{
	if (globals.domain) {
		mono_jit_cleanup(globals.domain);
		globals.domain = NULL;
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Deallocated mono runtime.\n");
	}

	return SWITCH_STATUS_SUCCESS;
}

/*
 * This function will load the managed modules
 *
 */ 
switch_status_t mod_mono_load_modules(const char *module_dir)
{
	apr_finfo_t *module_finfo = (apr_finfo_t *) switch_core_alloc(mono_pool, sizeof(*module_finfo));
	
	if (apr_stat(module_finfo, module_dir, 0, mono_pool) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Could not access module dir!.\n");
		return SWITCH_STATUS_FALSE;
	}
	
	apr_dir_t *module_dir_handle;
	char *file;
	size_t len;
	char *ptr;
	apr_finfo_t finfo;
	apr_int32_t finfo_flags = APR_FINFO_DIRENT | APR_FINFO_TYPE | APR_FINFO_NAME;
	const char *ext = ".dll";
	const char *EXT = ".DLL";
	MonoAssembly *assembly;
	MonoImage *image;
	gpointer iter;
	iter=NULL;
	mono_plugin *plugin=NULL;

	if (apr_dir_open(&module_dir_handle, module_dir, mono_pool) != SWITCH_STATUS_SUCCESS)
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "Could not open directory: %s\n", module_dir);

	/* Read the modules directory */
	while (apr_dir_read(&finfo, finfo_flags, module_dir_handle) == SWITCH_STATUS_SUCCESS)
	{
		assembly = (MonoAssembly *) switch_core_alloc(mono_pool, sizeof(assembly));
		image = (MonoImage *) switch_core_alloc(mono_pool, sizeof(image));
		const char *fname = finfo.fname;

		if (finfo.filetype != APR_REG)
			continue;
                        
		if (!fname)
			fname = finfo.name;

		if (!(ptr = (char *) fname))
			continue;

		if (!strstr(fname, ext) && !strstr(fname, EXT))
			continue;

		len = strlen(module_dir) + strlen(fname) + 2;
		file = (char *)switch_core_alloc(mono_pool, len);
		snprintf(file, len, "%s%s", module_dir, fname);

		/* Attempt to open the assembly */
		assembly = mono_domain_assembly_open(globals.domain, file);

		if (!assembly) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Not a valid assembly\n");
			continue;
		}

		/* Get the image from assembly */	
		image = mono_assembly_get_image(assembly);
	
		plugin = (mono_plugin *)switch_core_alloc(mono_pool, sizeof(*plugin));

		plugin->assembly = assembly;
		plugin->class    = mod_mono_find_assembly_class(mono_assembly_get_image(plugin->assembly));

		if (!plugin->class)
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "No plugin class\n");

		plugin->object = mono_object_new(globals.domain, plugin->class);

		switch_core_hash_insert(globals.plugins, (char *) mono_image_get_name(mono_assembly_get_image(assembly)), plugin);

		plugin = NULL;
	}	
	return SWITCH_STATUS_SUCCESS;
}

/*
 *
 */ 
MonoClass* mod_mono_find_assembly_class(MonoImage *image)
{
	MonoClass *class, *parent_class = NULL;
	int i, total;

	total = mono_image_get_table_rows(image, MONO_TABLE_TYPEDEF);
	
	for (i = 1; i <= total; ++i)
	{
		class = mono_class_get(image, MONO_TOKEN_TYPE_DEF | i);
		parent_class = mono_class_get_parent(class);
		
		if (parent_class) 
			if (!strcmp("Module", mono_class_get_name(parent_class)))
				return class;
	}
						
	return NULL;
}

/*
 *
 */
void mono_switch_console_printf(switch_text_channel_t channel, char *file, const char *func, int line, char *fmt, char *msg)
{
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, msg);
}

/* For Emacs:
 * Local Variables:
 * mode:c
 * indent-tabs-mode:nil
 * tab-width:4
 * c-basic-offset:4
 * End:
 * For VIM:
 * vim:set softtabstop=4 shiftwidth=4 tabstop=4 expandtab:
 */
