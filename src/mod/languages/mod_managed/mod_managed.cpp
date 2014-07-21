/* 
 * FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application - mod_managed
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
 * The Original Code is FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application - mod_managed
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
 *
 * mod_mono.cpp -- FreeSWITCH mod_mono main class
 *
 * Most of mod_mono is implmented in the mod_mono_managed Loader class. 
 * The native code just handles getting the Mono runtime up and down
 * and passing pointers into managed code.
 */  

#include <switch.h>
#include "freeswitch_managed.h" 

#ifdef _MANAGED
#include <mscoree.h>
using namespace System;
using namespace System::Runtime::InteropServices;
#define MOD_MANAGED_VERSION "Microsoft CLR Version"
#else
#define MOD_MANAGED_VERSION "Mono Version"
#endif

SWITCH_BEGIN_EXTERN_C 

SWITCH_MODULE_LOAD_FUNCTION(mod_managed_load);
SWITCH_MODULE_DEFINITION_EX(mod_managed, mod_managed_load, NULL, NULL, SMODF_GLOBAL_SYMBOLS);

SWITCH_STANDARD_API(managedrun_api_function);	/* ExecuteBackground */
SWITCH_STANDARD_API(managed_api_function);	/* Execute */
SWITCH_STANDARD_APP(managed_app_function);	/* Run */
SWITCH_STANDARD_API(managedreload_api_function);	/* Reload */
SWITCH_STANDARD_API(managedlist_api_function); /* List modules */

#define MOD_MANAGED_ASM_NAME "FreeSWITCH.Managed"
#define MOD_MANAGED_ASM_V1 1
#define MOD_MANAGED_ASM_V2 0
#define MOD_MANAGED_ASM_V3 2
#define MOD_MANAGED_ASM_V4 0
#define MOD_MANAGED_DLL MOD_MANAGED_ASM_NAME ".dll"
#define MOD_MANAGED_IMAGE_NAME "FreeSWITCH"
#define MOD_MANAGED_CLASS_NAME "Loader"

mod_managed_globals globals = { 0 };

// Global delegates to call managed functions
typedef int (*runFunction)(const char *data, void *sessionPtr);
typedef int (*executeFunction)(const char *cmd, void *stream, void *Event);
typedef int (*executeBackgroundFunction)(const char* cmd);
typedef int (*reloadFunction)(const char* cmd);
typedef int (*listFunction)(const char* cmd);
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
	session->setDTMFCallback(NULL, "");
	session->setHangupHook(NULL);
	session->dtmfDelegate = dtmfDelegate;
	session->hangupDelegate = hangupDelegate;
}

#ifndef _MANAGED	

#ifdef WIN32
#include <shlobj.h>
#endif	

switch_status_t setMonoDirs() 
{
#ifdef WIN32	
	// Win32 Mono installs can't figure out their own path
	// Guys in #mono say we should just deploy all the libs we need
	// We'll first check for Program Files\Mono to allow people to use the symlink dir for a specific version.
	// Then we'll check HKEY_LOCAL_MACHINE\SOFTWARE\Novell\Mono\2.0\FrameworkAssemblyDirectory and MonoConfigDir
	// After that, we'll scan program files for a Mono-* dir.
	char progFilesPath[MAX_PATH];
	char libPath[MAX_PATH];
	char etcPath[MAX_PATH];
	char findPath[MAX_PATH];
	bool found = false;

	SHGetFolderPath(NULL, CSIDL_PROGRAM_FILES, NULL, SHGFP_TYPE_CURRENT, progFilesPath);

	{ // Check PF\Mono directly
		DWORD attr;
		switch_snprintf(findPath, MAX_PATH, "%s\\Mono", progFilesPath);
		attr = GetFileAttributes(findPath);
		found = (attr != INVALID_FILE_ATTRIBUTES && ((attr & FILE_ATTRIBUTE_DIRECTORY) == FILE_ATTRIBUTE_DIRECTORY));
		if (found) {
			switch_snprintf(libPath, MAX_PATH, "%s\\lib", findPath);
			switch_snprintf(etcPath, MAX_PATH, "%s\\etc", findPath);
		}
	}

	if (!found) {   // Check registry
		DWORD size = MAX_PATH;
		if (ERROR_SUCCESS == RegGetValue(HKEY_LOCAL_MACHINE, "SOFTWARE\\Novell\\Mono\\2.0", "FrameworkAssemblyDirectory", RRF_RT_REG_SZ, NULL, &libPath, &size)) {
			size = MAX_PATH;
			if (ERROR_SUCCESS == RegGetValue(HKEY_LOCAL_MACHINE, "SOFTWARE\\Novell\\Mono\\2.0", "MonoConfigDir", RRF_RT_REG_SZ, NULL, &etcPath, &size)) {
				found = true;
			}
		}
	}

	if (!found) { // Scan program files for Mono-2something
		HANDLE hFind;
		WIN32_FIND_DATA findData;
		switch_snprintf(findPath, MAX_PATH, "%s\\Mono-2*", progFilesPath);
		hFind = FindFirstFile(findPath, &findData);
		if (hFind == INVALID_HANDLE_VALUE) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Error looking for Mono in Program Files.\n");
			return SWITCH_STATUS_FALSE;
		}

		while ((findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != FILE_ATTRIBUTE_DIRECTORY) {
			if (FindNextFile(hFind, &findData) == 0) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Could not find Mono directory in Program Files.\n");
				FindClose(hFind);
				return SWITCH_STATUS_FALSE;
			}
		}
		switch_snprintf(libPath, MAX_PATH, "%s\\%s\\lib", progFilesPath, findData.cFileName);
		switch_snprintf(etcPath, MAX_PATH, "%s\\%s\\etc", progFilesPath, findData.cFileName);
		FindClose(hFind);
	}

	/* Got it */ 
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "Using Mono paths '%s' and '%s'.\n", libPath, etcPath);
	mono_set_dirs(libPath, etcPath);
	return SWITCH_STATUS_SUCCESS;

#else
	// On other platforms, it should just work if it hasn't been relocated
	mono_set_dirs(NULL, NULL);
	return SWITCH_STATUS_SUCCESS;
#endif	
}

switch_status_t loadRuntime() 
{
	/* Find and load mod_mono_managed.exe */ 
	char filename[256];

	if (setMonoDirs() != SWITCH_STATUS_SUCCESS) {
		return SWITCH_STATUS_FALSE;
	}

#ifndef WIN32 	
	// So linux can find the .so
	char xmlConfig[300];
	switch_snprintf(xmlConfig, 300, "<configuration><dllmap dll=\"mod_managed\" target=\"%s%smod_managed.so\"/></configuration>", SWITCH_GLOBAL_dirs.mod_dir, SWITCH_PATH_SEPARATOR);
	mono_config_parse(NULL);
	mono_config_parse_memory(xmlConfig);
#endif

	switch_snprintf(filename, 256, "%s%s%s", SWITCH_GLOBAL_dirs.mod_dir, SWITCH_PATH_SEPARATOR, MOD_MANAGED_DLL);
	globals.domain = mono_jit_init(filename);

	/* Already got a Mono domain? */
	if ((globals.domain = mono_get_root_domain())) {
		mono_thread_attach(globals.domain);
		globals.embedded = SWITCH_TRUE;
	} else {
		if (!(globals.domain = mono_jit_init(filename))) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "mono_jit_init failed.\n");
			return SWITCH_STATUS_FALSE;
		}
	}

	/* Already loaded? */
	MonoAssemblyName *name = mono_assembly_name_new (MOD_MANAGED_ASM_NAME);
	//Note also that it can't be allocated on the stack anymore and you'll need to create and destroy it with the following API:
	//mono_assembly_name_free (name);
	
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "Calling mono_assembly_loaded.\n");

	if (!(globals.mod_mono_asm = mono_assembly_loaded(name))) {
		/* Open the assembly */ 
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "Calling mono_domain_assembly_open.\n");
		globals.mod_mono_asm = mono_domain_assembly_open(globals.domain, filename);
		if (!globals.mod_mono_asm) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "mono_domain_assembly_open failed.\n");
			return SWITCH_STATUS_FALSE;
		}
	}

	return SWITCH_STATUS_SUCCESS;
}

MonoMethod * getMethod(const char *name, MonoClass * klass) 
{
	MonoMethodDesc * desc;
	MonoMethod * method;

	desc = mono_method_desc_new(name, TRUE);
	method = mono_method_desc_search_in_class(desc, klass);

	if (!method) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Could not find %s method.\n", name);
		return NULL;
	}

	return method;
}

switch_status_t findLoader() 
{
	/* Find loader class and methods */ 
	MonoClass * loaderClass;
	MonoImage * img = mono_assembly_get_image(globals.mod_mono_asm);

	if (!(loaderClass = mono_class_from_name(img, MOD_MANAGED_IMAGE_NAME, MOD_MANAGED_CLASS_NAME))) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Could not find " MOD_MANAGED_IMAGE_NAME "." MOD_MANAGED_CLASS_NAME " class.\n");
		return SWITCH_STATUS_FALSE;
	}

	if (!(globals.loadMethod = getMethod(MOD_MANAGED_IMAGE_NAME "." MOD_MANAGED_CLASS_NAME ":Load()", loaderClass))) {
		return SWITCH_STATUS_FALSE;
	}

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Found all loader functions.\n");
	return SWITCH_STATUS_SUCCESS;
}
#endif

/**********************************************************
	CLR Code Starts Here
**********************************************************/

#ifdef _MANAGED

switch_status_t loadRuntime() 
{
	/* Find and load mod_dotnet_managed.dll */ 
	char filename[256];
	switch_snprintf(filename, 256, "%s%s%s", SWITCH_GLOBAL_dirs.mod_dir, SWITCH_PATH_SEPARATOR, MOD_MANAGED_DLL);

	wchar_t modpath[256];
	mbstowcs(modpath, filename, 255);
	try {
		FreeSwitchManaged::mod_dotnet_managed = Assembly::LoadFrom(gcnew String(modpath));
	} catch (Exception^ ex) {
		IntPtr msg = Marshal::StringToHGlobalAnsi(ex->ToString());
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Assembly::LoadFrom failed: %s\n", static_cast<const char*>(msg.ToPointer()));
		Marshal::FreeHGlobal(msg);
		return SWITCH_STATUS_FALSE;
	}
	return SWITCH_STATUS_SUCCESS;
}

switch_status_t findLoader() 
{
	try {
		FreeSwitchManaged::loadMethod = FreeSwitchManaged::mod_dotnet_managed->GetType(MOD_MANAGED_IMAGE_NAME "." MOD_MANAGED_CLASS_NAME)->GetMethod("Load");
	} catch(Exception^ ex) {
		IntPtr msg = Marshal::StringToHGlobalAnsi(ex->ToString());
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Could not load " MOD_MANAGED_IMAGE_NAME "." MOD_MANAGED_CLASS_NAME " class: %s\n", static_cast<const char*>(msg.ToPointer()));
		Marshal::FreeHGlobal(msg);
		return SWITCH_STATUS_FALSE;
	}
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Found all " MOD_MANAGED_IMAGE_NAME "." MOD_MANAGED_CLASS_NAME " functions.\n");
	return SWITCH_STATUS_SUCCESS;
}
#endif

SWITCH_MODULE_LOAD_FUNCTION(mod_managed_load) 
{
	int success;
	/* connect my internal structure to the blank pointer passed to me */ 
	*module_interface = switch_loadable_module_create_module_interface(pool, modname);
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "Loading mod_managed (Common Language Infrastructure), " MOD_MANAGED_VERSION "\n");

	globals.pool = pool;
	
	if (loadRuntime() != SWITCH_STATUS_SUCCESS) {			
		return SWITCH_STATUS_FALSE;
	}
	
	if (findLoader() != SWITCH_STATUS_SUCCESS) {		
		return SWITCH_STATUS_FALSE;
	}
#ifdef _MANAGED
	try {
		Object ^objResult = FreeSwitchManaged::loadMethod->Invoke(nullptr, nullptr);
		success = *reinterpret_cast<bool^>(objResult);
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Load completed successfully.\n");
	}
	catch(Exception^ ex) {
		IntPtr msg = Marshal::StringToHGlobalAnsi(ex->ToString());
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Load did not return true. %s\n", static_cast<const char*>(msg.ToPointer()));
		Marshal::FreeHGlobal(msg);
		return SWITCH_STATUS_FALSE;
	}
#else
	/* Not sure if this is necesary on the loading thread */ 
	mono_thread_attach(globals.domain);

	/* Run loader */ 
	MonoObject * exception = NULL;
	MonoObject * objResult = mono_runtime_invoke(globals.loadMethod, NULL, NULL, &exception);
	if (exception) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Load threw an exception.\n");
		mono_print_unhandled_exception(exception);
		return SWITCH_STATUS_FALSE;
	}
	success = *(int *) mono_object_unbox(objResult);
#endif
	if (success) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Load completed successfully.\n");
	} else {		
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Load did not return true.\n");
		return SWITCH_STATUS_FALSE;
	}
	
	/* We're good to register */ 
	switch_api_interface_t *api_interface;
	switch_application_interface_t *app_interface;

	SWITCH_ADD_API(api_interface, "managedrun", "Run a module (ExecuteBackground)", managedrun_api_function, "<module> [<args>]");
	SWITCH_ADD_API(api_interface, "managed", "Run a module as an API function (Execute)", managed_api_function, "<module> [<args>]");
	SWITCH_ADD_APP(app_interface, "managed", "Run CLI App", "Run an App on a channel", managed_app_function, "<modulename> [<args>]", SAF_SUPPORT_NOMEDIA);
	SWITCH_ADD_API(api_interface, "managedreload", "Force [re]load of a file", managedreload_api_function, "<filename>");
	SWITCH_ADD_API(api_interface, "managedlist", "Log the list of available APIs and Apps", managedlist_api_function, "");
	return SWITCH_STATUS_NOUNLOAD;
}

#ifdef _MANAGED
#pragma unmanaged
#endif
SWITCH_STANDARD_API(managedrun_api_function) 
{
	if (zstr(cmd)) {
		stream->write_function(stream, "-ERR no args specified!\n");	
		return SWITCH_STATUS_SUCCESS;
	}
#ifndef _MANAGED
	mono_thread_attach(globals.domain);
#endif
	if (executeBackgroundDelegate(cmd)) {
		stream->write_function(stream, "+OK\n");
	} else {	
		stream->write_function(stream, "-ERR ExecuteBackground returned false (unknown module or exception?).\n");
	}
#ifndef _MANAGED
	mono_thread_detach(mono_thread_current());
#endif
	return SWITCH_STATUS_SUCCESS;
}

SWITCH_STANDARD_API(managed_api_function) 
{
	if (zstr(cmd)) {
		stream->write_function(stream, "-ERR no args specified!\n");	
		return SWITCH_STATUS_SUCCESS;
	}
#ifndef _MANAGED
	mono_thread_attach(globals.domain);
#endif
	if (!(executeDelegate(cmd, stream, stream->param_event))) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Execute failed for %s (unknown module or exception).\n", cmd); 
	}
#ifndef _MANAGED
	mono_thread_detach(mono_thread_current());
#endif
	return SWITCH_STATUS_SUCCESS;
}

SWITCH_STANDARD_APP(managed_app_function) 
{
	if (zstr(data)) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "No args specified!\n");
		return;
	}
#ifndef _MANAGED
	mono_thread_attach(globals.domain);
#endif
	if (!(runDelegate(data, session))) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Application run failed for %s (unknown module or exception).\n", data);
	}
#ifndef _MANAGED
	mono_thread_detach(mono_thread_current());
#endif
}

SWITCH_STANDARD_API(managedreload_api_function) 
{
	if (zstr(cmd)) {
		stream->write_function(stream, "-ERR no args specified!\n");	
		return SWITCH_STATUS_SUCCESS;
	}
#ifndef _MANAGED
	mono_thread_attach(globals.domain);
#endif
	if (!(reloadDelegate(cmd))) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Execute failed for %s (unknown module or exception).\n", cmd); 
	}
#ifndef _MANAGED
	mono_thread_detach(mono_thread_current());
#endif
	return SWITCH_STATUS_SUCCESS;
}

SWITCH_STANDARD_API(managedlist_api_function)
{
#ifndef _MANAGED
	mono_thread_attach(globals.domain);
#endif
	listDelegate(cmd);
#ifndef _MANAGED
	mono_thread_detach(mono_thread_current());
#endif
	return SWITCH_STATUS_SUCCESS;
}

SWITCH_END_EXTERN_C
