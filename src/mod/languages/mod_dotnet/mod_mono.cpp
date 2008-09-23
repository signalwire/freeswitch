/* 
 * FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application - mod_dotnet
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
 * The Original Code is FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application - mod_dotnet
 *
 * The Initial Developer of the Original Code is
 * Michael Giagnocavo <mgg@packetrino.com>
 * Portions created by the Initial Developer are Copyright (C)
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 * 
 * Michael Giagnocavo <mgg@packetrino.com>
 * 
 * mod_mono.cpp -- FreeSWITCH mod_mono main class
 *
 * Most of mod_dotnet is implmented in the mod_dotnet_managed Loader class. 
 * The native code just handles getting the Dotnet runtime up and down
 * and passing pointers into managed code.
 */  


#include <switch.h>
#include <apr_pools.h>
#include <mscoree.h>

SWITCH_BEGIN_EXTERN_C 

	
#ifdef WIN32
#include <shlobj.h>
#define EXPORT __declspec(dllexport)
#elif
#define EXPORT 
#endif	/* 
 */
#include "freeswitch_mono.h"

#define MOD_DOTNET_MANAGED_DLL "mod_dotnet_managed.dll"


struct dotnet_conf_t {
    switch_memory_pool_t *pool;
    ICLRRuntimeHost *pCorRuntime;
    HMODULE lock_module;
	 OSVERSIONINFO osver;
    char *cor_version;
} globals;


SWITCH_MODULE_LOAD_FUNCTION(mod_dotnet_load);
SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_dotnet_shutdown);
SWITCH_MODULE_DEFINITION(mod_dotnet, mod_dotnet_load, mod_dotnet_shutdown, NULL);
SWITCH_STANDARD_API(dotnetrun_api_function);	/* ExecuteBackground */
SWITCH_STANDARD_API(dotnet_api_function);	/* Execute */
SWITCH_STANDARD_APP(dotnet_app_function);	/* Run */


// Sets up delegates (and anything else needed) on the MonoSession object
// Called from MonoSession.Initialize Managed -> this is Unmanaged code so all pointers are marshalled and prevented from GC
// Exported method.
SWITCH_MOD_DECLARE(void) InitDotnetSession(MonoSession * session, inputtype dtmfDelegate, hanguptype hangupDelegate) 
{
	switch_assert(session);
	if (!session) {
		return;
	}
	session->setDTMFCallback(NULL, "");
	session->setHangupHook(NULL);
	session->dtmfDelegateHandle = dtmfDelegate;
	session->hangupDelegateHandle = hangupDelegate;
}


switch_status_t loadModMonoManaged() 
{
	
	/* Find and load mod_dotnet_managed.dll */ 
	char filename[256];
	
	switch_snprintf(filename, 256, "%s%s%s", SWITCH_GLOBAL_dirs.mod_dir, SWITCH_PATH_SEPARATOR, MOD_DOTNET_MANAGED_DLL);

	HRESULT hr;
	wchar_t wCORVersion[256];
	if (globals.cor_version) {
	  MultiByteToWideChar(CP_UTF8, 0, globals.cor_version, -1,
								 wCORVersion, sizeof(wCORVersion) / sizeof(wchar_t));
	}
	else {
	  DWORD bytes;
	  hr = GetCORVersion(wCORVersion, sizeof(wCORVersion) 
													/ sizeof(wchar_t) - 1, &bytes);
	  if (FAILED(hr)) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, 
							"mod_dotnet: GetCORVersion failed to return "
							"the .NET CLR engine version.");
			return SWITCH_STATUS_FALSE;
	  }
	  int len = WideCharToMultiByte(CP_UTF8, 0, wCORVersion, -1, 
											  NULL, 0, NULL, NULL);
	  globals.cor_version = (char *)apr_palloc(globals.pool, len);
	  len = WideCharToMultiByte(CP_UTF8, 0, wCORVersion, -1, 
										 globals.cor_version, len, NULL, NULL);
	}

	hr = CorBindToRuntimeEx(wCORVersion, 
								 L"wks",  // Or "svr" 
								 STARTUP_LOADER_OPTIMIZATION_MULTI_DOMAIN_HOST | 
								 STARTUP_CONCURRENT_GC, 
								 CLSID_CorRuntimeHost, 
								 IID_ICorRuntimeHost, 
								 (void **)&globals.pCorRuntime);
	if (FAILED(hr)) {
	  switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, 
						"mod_dotnet: Could not CorBindToRuntimeEx version "
						"%s for the .NET CLR engine.", globals.cor_version);
	  return SWITCH_STATUS_FALSE;
	}

	hr = globals.pCorRuntime->Start();
	if (FAILED(hr)) {
	  switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR,
						"mod_dotnet: Could not start the "
						".NET CLR engine.");
	  return SWITCH_STATUS_FALSE;
	}

	wchar_t modpath[256];
	mbstowcs(modpath, filename, 255);

	try
	{
		FreeSwitchManaged::mod_dotnet_managed = Assembly::LoadFrom(gcnew String(modpath));
	}
	catch (Exception^) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "mono_domain_assembly_open failed.\n");
		return SWITCH_STATUS_FALSE;
	}
	
	return SWITCH_STATUS_SUCCESS;

}


switch_status_t findLoader() 
{

	try{
		FreeSwitchManaged::loadMethod = FreeSwitchManaged::mod_dotnet_managed->GetType("FreeSWITCH.Loader")->GetMethod("Load");
		FreeSwitchManaged::unloadMethod = FreeSwitchManaged::mod_dotnet_managed->GetType("FreeSWITCH.Loader")->GetMethod("Unload");
		FreeSwitchManaged::runMethod = FreeSwitchManaged::mod_dotnet_managed->GetType("FreeSWITCH.Loader")->GetMethod("Run");
		FreeSwitchManaged::executeMethod = FreeSwitchManaged::mod_dotnet_managed->GetType("FreeSWITCH.Loader")->GetMethod("Execute");
		FreeSwitchManaged::executeBackgroundMethod = FreeSwitchManaged::mod_dotnet_managed->GetType("FreeSWITCH.Loader")->GetMethod("ExecuteBackground");
	}
	catch(Exception^ ex)	{
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Could not find FreeSWITCH.Loader class %s.\n", ex->ToString());
		return SWITCH_STATUS_FALSE;
	}
	
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Found all loader functions.\n");
		
	return SWITCH_STATUS_SUCCESS;

}



SWITCH_MODULE_LOAD_FUNCTION(mod_dotnet_load) 
{
	
	/* connect my internal structure to the blank pointer passed to me */ 
	*module_interface = switch_loadable_module_create_module_interface(pool, modname);
	
	globals.pool = pool;
	
	if (loadModMonoManaged() != SWITCH_STATUS_SUCCESS) {			
		return SWITCH_STATUS_FALSE;
	}
	
	if (findLoader() != SWITCH_STATUS_SUCCESS) {		
		return SWITCH_STATUS_FALSE;
	}

	Object ^objResult;
	try{
		objResult = FreeSwitchManaged::loadMethod->Invoke(nullptr, nullptr);
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Load completed successfully.\n");
	}
	catch(Exception^ ex)	{
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Load did not return true. %s\n", ex->ToString());
		return SWITCH_STATUS_FALSE;
	}


	if (*reinterpret_cast<bool^>(objResult)) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Load completed successfully.\n");
	} else {		
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Load did not return true.\n");
		return SWITCH_STATUS_FALSE;
	}
	
	/* We're good to register */ 
	switch_api_interface_t *api_interface;

	switch_application_interface_t *app_interface;


	SWITCH_ADD_API(api_interface, "dotnetrun", "Run a module (ExecuteBackground)", dotnetrun_api_function, "<module> [<args>]");

	SWITCH_ADD_API(api_interface, "dotnet", "Run a module as an API function (Execute)", dotnet_api_function, "<module> [<args>]");

	SWITCH_ADD_APP(app_interface, "dotnet", "Run Mono IVR", "Run an App on a channel", dotnet_app_function, "<modulename> [<args>]", SAF_NONE);


	return SWITCH_STATUS_SUCCESS;

}



SWITCH_STANDARD_API(dotnetrun_api_function) 
{
	
	if (switch_strlen_zero(cmd)) {
		stream->write_function(stream, "-ERR no args specified!\n");	
		return SWITCH_STATUS_SUCCESS;
	}

	Object ^objResult;
	try{
		objResult = FreeSwitchManaged::executeBackgroundMethod->Invoke(nullptr, gcnew array<Object^>{gcnew String(cmd)});
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Load completed successfully.\n");
	}
	catch(Exception^)	{
		stream->write_function(stream, "-ERR FreeSWITCH.Loader.ExecuteBackground threw an exception.\n");
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Load did not return true.\n");
		return SWITCH_STATUS_FALSE;
	}


	if (*reinterpret_cast<bool^>(objResult)) {
		stream->write_function(stream, "+OK\n");
	} else {	
		stream->write_function(stream, "-ERR ExecuteBackground returned false (unknown module?).\n");
	}
	
	return SWITCH_STATUS_SUCCESS;

}



SWITCH_STANDARD_API(dotnet_api_function) 
{

	if (switch_strlen_zero(cmd)) {
		stream->write_function(stream, "-ERR no args specified!\n");	
		return SWITCH_STATUS_SUCCESS;
	}
	

	Object ^objResult;
	try{
		objResult = FreeSwitchManaged::executeMethod->Invoke(nullptr, gcnew array<Object^>{gcnew String(cmd),gcnew IntPtr(stream), gcnew IntPtr(stream->param_event)});
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Execute completed successfully.\n");
	}
	catch(Exception ^ex)	{
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Exception trying to execute mono %s %s.\n", cmd, ex->ToString());
		return SWITCH_STATUS_FALSE;
	}
	
	if (!*reinterpret_cast<bool^>(objResult)) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Execute failed for %s (unknown module?).\n", cmd);
	}
	
	return SWITCH_STATUS_SUCCESS;
}



SWITCH_STANDARD_APP(dotnet_app_function) 
{

	if (switch_strlen_zero(data)) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "No args specified!\n");
	}
	

	Object ^objResult;
	try{
		objResult = FreeSwitchManaged::runMethod->Invoke(nullptr, gcnew array<Object^>{gcnew String(data),gcnew IntPtr(session)});
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "RunMethod completed successfully.\n");
	}
	catch(Exception ^ex)	{
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Exception trying to execute application mono %s %s.\n", data, ex->ToString());
		return;
	}
	
	if (!*reinterpret_cast<bool^>(objResult)) {	
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Application run failed for %s (unknown module?).\n", data);
	}
}


SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_dotnet_shutdown) 
{
	
	Object ^objResult;
	try{
		objResult = FreeSwitchManaged::unloadMethod->Invoke(nullptr, nullptr);
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "UnloadMethod completed successfully.\n");
	}
	catch(Exception^) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Exception occurred in Loader::Unload.\n");
		return SWITCH_STATUS_FALSE;;
	}

	return SWITCH_STATUS_SUCCESS;
}



SWITCH_END_EXTERN_C
