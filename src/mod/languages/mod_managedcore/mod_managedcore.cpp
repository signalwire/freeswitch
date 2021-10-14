/*
 * FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application - mod_managedcore
 * Copyright (C) 2008, Michael Giagnocavo <mgg@packetrino.com>
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
 * The Original Code is FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application - mod_managedcore
 *
 * The Initial Developer of the Original Code is
 * Michael Giagnocavo <mgg@packetrino.com>
 * Portions created by the Initial Developer are Copyright (C)
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Michael Giagnocavo <mgg@giagnocavo.net>
 * David Brazier <David.Brazier@360crm.co.uk>
 * Jeff Lenk <jlenk@frontiernet.net>
 * Artur Kraev <ravenox@gmail.com>
 */

#include <switch.h>
#include "freeswitch_managedcore.h"

#define NETHOST_USE_AS_STATIC
#include <nethost.h>
#include <coreclr_delegates.h>
#include <hostfxr.h>

#ifdef _WIN32
#include <Windows.h>

#define STR(s) L ## s
#define CH(c) L ## c
#define DIR_SEPARATOR L'\\'

#else
#include <limits.h>

#define STR(s) s
#define CH(c) c
#define DIR_SEPARATOR '/'
#define MAX_PATH PATH_MAX

#ifndef max
#define max(a,b)            (((a) > (b)) ? (a) : (b))
#endif

#ifndef min
#define min(a,b)            (((a) < (b)) ? (a) : (b))
#endif

#endif



SWITCH_BEGIN_EXTERN_C

SWITCH_MODULE_LOAD_FUNCTION(mod_managedcore_load);
SWITCH_MODULE_DEFINITION_EX(mod_managedcore, mod_managedcore_load, NULL, NULL, SMODF_GLOBAL_SYMBOLS);

SWITCH_STANDARD_API(managedcorerun_api_function);	/* ExecuteBackground */
SWITCH_STANDARD_API(managedcore_api_function);	/* Execute */
SWITCH_STANDARD_APP(managedcore_app_function);	/* Run */
SWITCH_STANDARD_API(managedcorereload_api_function);	/* Reload */
SWITCH_STANDARD_API(managedcorelist_api_function); /* List modules */

#define MOD_MANAGED_ASM_NAME "FreeSWITCH.ManagedCore"
#define MOD_MANAGED_ASM_V1 1
#define MOD_MANAGED_ASM_V2 0
#define MOD_MANAGED_ASM_V3 2
#define MOD_MANAGED_ASM_V4 0
#define MOD_MANAGED_DLL MOD_MANAGED_ASM_NAME ".dll"
#define MOD_MANAGED_RUNTIMECONFIG MOD_MANAGED_ASM_NAME ".runtimeconfig.json"
#define MOD_MANAGED_IMAGE_NAME "FreeSWITCH"
#define MOD_MANAGED_CLASS_NAME "Loader"

mod_managed_globals managed_globals = { 0 };

// Global delegates to call managed functions
typedef int (*runFunction)(const char *data, void *sessionPtr);
typedef int (*executeFunction)(const char *cmd, void *stream, void *Event);
typedef int (*executeBackgroundFunction)(const char* cmd);
typedef int (*reloadFunction)(const char* cmd);
typedef int (*listFunction)(const char *cmd, void *stream, void *Event);
static runFunction runDelegate;
static executeFunction executeDelegate;
static executeBackgroundFunction executeBackgroundDelegate;
static reloadFunction reloadDelegate;
static listFunction listDelegate;

SWITCH_MOD_DECLARE_NONSTD(void) InitManagedDelegates(runFunction run, executeFunction execute, executeBackgroundFunction executeBackground, reloadFunction reload, listFunction list)
{
	runDelegate = run;
	executeDelegate = execute;
	executeBackgroundDelegate = executeBackground;
	reloadDelegate = reload;
	listDelegate = list;
}


// Sets up delegates (and anything else needed) on the ManagedSession object
// Called from ManagedSession.Initialize Managed -> this is Unmanaged code so all pointers are marshalled and prevented from GC
// Exported method.
SWITCH_MOD_DECLARE_NONSTD(void) InitManagedSession(ManagedSession *session, inputFunction dtmfDelegate, hangupFunction hangupDelegate)
{
	switch_assert(session);
	if (!session) {
		return;
	}
	session->setDTMFCallback(NULL, (char *)"");
	session->setHangupHook(NULL);
	session->dtmfDelegate = dtmfDelegate;
	session->hangupDelegate = hangupDelegate;
}


void ConvertToChar_t(const char* inStr, size_t inStrLen, char_t* outStr, size_t outStrLen)
{
	if (sizeof(char_t) == sizeof(char))
	{
		strncpy((char*)outStr, inStr, min(inStrLen, outStrLen));
		return;
	}

	mbstowcs((wchar_t*)outStr, inStr, min(inStrLen, outStrLen));
}

void ConvertToChar(const char_t* inStr, size_t inStrLen, char* outStr, size_t outStrLen)
{
	if (sizeof(char_t) == sizeof(char))
	{
		strncpy(outStr, (char*)inStr, min(inStrLen, outStrLen));
		return;
	}

	wcstombs(outStr, (const wchar_t*)inStr, min(inStrLen, outStrLen));
}


switch_status_t loadRuntime()
{
	char_t hostfxr_path_t[MAX_PATH];
	size_t hostfxr_path_size = sizeof(hostfxr_path_t) / sizeof(char_t);

	if (get_hostfxr_path(hostfxr_path_t, &hostfxr_path_size, NULL)) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Unable to locate Core HostFXR\n");
		return SWITCH_STATUS_FALSE;
	}

	char *derr = NULL;
	char hostfxr_path[MAX_PATH];
	ConvertToChar(hostfxr_path_t, MAX_PATH, hostfxr_path, MAX_PATH);
	switch_dso_lib_t lib_t = switch_dso_open(hostfxr_path, 0, &derr);

	hostfxr_initialize_for_runtime_config_fn hostfxr_initialize_for_runtime_config_fptr = (hostfxr_initialize_for_runtime_config_fn)switch_dso_func_sym(lib_t, "hostfxr_initialize_for_runtime_config", &derr);
	hostfxr_get_runtime_delegate_fn hostfxr_get_runtime_delegate_fptr = (hostfxr_get_runtime_delegate_fn)switch_dso_func_sym(lib_t, "hostfxr_get_runtime_delegate", &derr);
	hostfxr_close_fn hostfxr_close_fptr = (hostfxr_close_fn)switch_dso_func_sym(lib_t, "hostfxr_close", &derr);

	if (!hostfxr_initialize_for_runtime_config_fptr || !hostfxr_get_runtime_delegate_fptr || !hostfxr_close_fptr) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Unable to get Core HostFXR exports: %s\n", hostfxr_path);
		return SWITCH_STATUS_FALSE;
	}

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "Loaded Core HostFXR: %s\n", hostfxr_path);

	char runtimeconfigpath[MAX_PATH];
	switch_snprintf(runtimeconfigpath, MAX_PATH, "%s%s%s", SWITCH_GLOBAL_dirs.mod_dir, SWITCH_PATH_SEPARATOR, MOD_MANAGED_RUNTIMECONFIG);
	char_t runtimeconfigpath_t[MAX_PATH];
	ConvertToChar_t(runtimeconfigpath, MAX_PATH, runtimeconfigpath_t, MAX_PATH);


	hostfxr_handle handle = NULL;
	if (hostfxr_initialize_for_runtime_config_fptr(runtimeconfigpath_t, NULL, &handle) || !handle)
	{
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Unable to initialize handle (%s)\n", runtimeconfigpath);
		hostfxr_close_fptr(handle);
		return SWITCH_STATUS_FALSE;
	}

	load_assembly_and_get_function_pointer_fn load_assembly_and_get_function_pointer;

	if (hostfxr_get_runtime_delegate_fptr(handle, hdt_load_assembly_and_get_function_pointer, (void**)&load_assembly_and_get_function_pointer) || !load_assembly_and_get_function_pointer) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Unable to get runtime delegate\n");
		hostfxr_close_fptr(handle);
		return SWITCH_STATUS_FALSE;
	}

	managed_globals.load_assembly_and_get_function_pointer = load_assembly_and_get_function_pointer;

	hostfxr_close_fptr(handle);

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "Initialized Core HostFXR\n");
	return SWITCH_STATUS_SUCCESS;
}

switch_status_t findLoader()
{
	/* Find loader class and methods */
	char loaderpath[MAX_PATH];
	switch_snprintf(loaderpath, MAX_PATH, "%s%s%s", SWITCH_GLOBAL_dirs.mod_dir, SWITCH_PATH_SEPARATOR, MOD_MANAGED_DLL);
	char_t loaderpath_t[MAX_PATH];
	ConvertToChar_t(loaderpath, MAX_PATH, loaderpath_t, MAX_PATH);
	char_t typeName_t[MAX_PATH];
	ConvertToChar_t("FreeSWITCH.Loader, FreeSWITCH.ManagedCore", MAX_PATH, typeName_t, MAX_PATH);
	char_t methodName_t[MAX_PATH];
	ConvertToChar_t("Load", MAX_PATH, methodName_t, MAX_PATH);
	char_t delegateTypeName_t[MAX_PATH];
	ConvertToChar_t("FreeSWITCH.Loader+LoadDelegate, FreeSWITCH.ManagedCore", MAX_PATH, delegateTypeName_t, MAX_PATH);

	loaderFunction load = NULL;
	if (managed_globals.load_assembly_and_get_function_pointer(loaderpath_t, typeName_t, methodName_t, delegateTypeName_t, NULL, (void**)&load) ||
		!load) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Unable to load loader assembly and get loader entry function pointer\n");
		return SWITCH_STATUS_FALSE;
	}
	managed_globals.loadMethod = load;

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "Loaded Core HostFXR Loader: %s\n", loaderpath);
	return SWITCH_STATUS_SUCCESS;
}

SWITCH_MODULE_LOAD_FUNCTION(mod_managedcore_load)
{
	int success;
	/* connect my internal structure to the blank pointer passed to me */
	*module_interface = switch_loadable_module_create_module_interface(pool, modname);
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "Loading mod_managedcore (Common Language Infrastructure)\n");

	managed_globals.pool = pool;

	if (loadRuntime() != SWITCH_STATUS_SUCCESS) {
		return SWITCH_STATUS_FALSE;
	}

	if (findLoader() != SWITCH_STATUS_SUCCESS) {
		return SWITCH_STATUS_FALSE;
	}

	success = managed_globals.loadMethod();

	if (success) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Load completed successfully.\n");
	} else {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Load did not return true.\n");
		return SWITCH_STATUS_FALSE;
	}

	/* We're good to register */
	switch_api_interface_t *api_interface;
	switch_application_interface_t *app_interface;

	SWITCH_ADD_API(api_interface, "managedrun", "Run a module (ExecuteBackground)", managedcorerun_api_function, "<module> [<args>]");
	SWITCH_ADD_API(api_interface, "managed", "Run a module as an API function (Execute)", managedcore_api_function, "<module> [<args>]");
	SWITCH_ADD_APP(app_interface, "managed", "Run CLI App", "Run an App on a channel", managedcore_app_function, "<modulename> [<args>]", SAF_SUPPORT_NOMEDIA);
	SWITCH_ADD_API(api_interface, "managedreload", "Force [re]load of a file", managedcorereload_api_function, "<filename>");
	SWITCH_ADD_API(api_interface, "managedlist", "Log the list of available APIs and Apps", managedcorelist_api_function, "");
	return SWITCH_STATUS_NOUNLOAD;
}

SWITCH_STANDARD_API(managedcorerun_api_function)
{
	if (zstr(cmd)) {
		stream->write_function(stream, "-ERR no args specified!\n");
		return SWITCH_STATUS_SUCCESS;
	}

	if (executeBackgroundDelegate(cmd)) {
		stream->write_function(stream, "+OK\n");
	} else {
		stream->write_function(stream, "-ERR ExecuteBackground returned false (unknown module or exception?).\n");
	}

	return SWITCH_STATUS_SUCCESS;
}

SWITCH_STANDARD_API(managedcore_api_function)
{
	if (zstr(cmd)) {
		stream->write_function(stream, "-ERR no args specified!\n");
		return SWITCH_STATUS_SUCCESS;
	}

	if (!(executeDelegate(cmd, stream, stream->param_event))) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Execute failed for %s (unknown module or exception).\n", cmd);
	}

	return SWITCH_STATUS_SUCCESS;
}

SWITCH_STANDARD_APP(managedcore_app_function)
{
	if (zstr(data)) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "No args specified!\n");
		return;
	}

	if (!(runDelegate(data, session))) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Application run failed for %s (unknown module or exception).\n", data);
	}
}

SWITCH_STANDARD_API(managedcorereload_api_function)
{
	if (zstr(cmd)) {
		stream->write_function(stream, "-ERR no args specified!\n");
		return SWITCH_STATUS_SUCCESS;
	}

	if (!(reloadDelegate(cmd))) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Execute failed for %s (unknown module or exception).\n", cmd);
	}

	return SWITCH_STATUS_SUCCESS;
}

SWITCH_STANDARD_API(managedcorelist_api_function)
{
	listDelegate(cmd, stream, stream->param_event);
	return SWITCH_STATUS_SUCCESS;
}

SWITCH_END_EXTERN_C
