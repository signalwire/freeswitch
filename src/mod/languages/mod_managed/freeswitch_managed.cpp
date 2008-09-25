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
 * Michael Giagnocavo <mgg@packetrino.com>
 * Jeff Lenk <jlenk@frontiernet.net> - Modified class to support Dotnet
 * 
 * freeswitch_cli.cpp -- CLI CoreSession subclasses
 *
 */  


#include <switch.h>
#include <switch_cpp.h>
#ifndef _MANAGED
#include <glib.h>
#include <mono/jit/jit.h>
#include <mono/metadata/assembly.h>
#include <mono/metadata/environment.h>
#include <mono/metadata/threads.h>
#include <mono/metadata/metadata.h>
#endif

#include "freeswitch_managed.h"

#ifdef _MANAGED

ManagedSession::ManagedSession():CoreSession() 
{

} 

ManagedSession::ManagedSession(char *uuid):CoreSession(uuid) 
{

} 

ManagedSession::ManagedSession(switch_core_session_t *session):CoreSession(session) 
{

} 

ManagedSession::~ManagedSession() 
{
	// Do auto-hangup ourselves because CoreSession can't call check_hangup_hook 
	// after ManagedSession destruction (cause at point it's pure virtual)
	if (session) {
		channel = switch_core_session_get_channel(session);
		if (switch_test_flag(this, S_HUP) && !switch_channel_test_flag(channel, CF_TRANSFER)) {
			switch_channel_hangup(channel, SWITCH_CAUSE_NORMAL_CLEARING);
			setAutoHangup(0);
		}
		// Don't let any callbacks use this CoreSession anymore
		switch_channel_set_private(channel, "CoreSession", NULL);
	}
}

bool ManagedSession::begin_allow_threads() 
{
	return true;
}

bool ManagedSession::end_allow_threads() 
{
	return true;
}

void ManagedSession::check_hangup_hook() 
{
	if (!hangupDelegateHandle) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "hangupDelegateHandle didn't get an object.");
		return;
	}
	hangupDelegateHandle();
}

switch_status_t ManagedSession::run_dtmf_callback(void *input, switch_input_type_t itype) 
{
	if (!dtmfDelegateHandle) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "dtmfDelegateHandle didn't get an object.");
		return SWITCH_STATUS_FALSE;;
	}
	char* result = dtmfDelegateHandle(input, itype);

	switch_status_t status = process_callback_result(result);
	return status;
}

#else

ManagedSession::ManagedSession():CoreSession() 
{

} 

ManagedSession::ManagedSession(char *uuid):CoreSession(uuid) 
{

} 

ManagedSession::ManagedSession(switch_core_session_t *session):CoreSession(session) 
{

} 

ManagedSession::~ManagedSession() 
{
	mono_thread_attach(globals.domain);
		
	if (dtmfDelegateHandle) {
		mono_gchandle_free(dtmfDelegateHandle);
	}
		
	if (hangupDelegateHandle) {
		mono_gchandle_free(hangupDelegateHandle);
	}

	// Do auto-hangup ourselves because CoreSession can't call check_hangup_hook 
	// after ManagedSession destruction (cause at point it's pure virtual)
	if (session) {
		channel = switch_core_session_get_channel(session);
		if (switch_test_flag(this, S_HUP) && !switch_channel_test_flag(channel, CF_TRANSFER)) {
			switch_channel_hangup(channel, SWITCH_CAUSE_NORMAL_CLEARING);
			setAutoHangup(0);
		}
		// Don't let any callbacks use this CoreSession anymore
		switch_channel_set_private(channel, "CoreSession", NULL);
	}
}

bool ManagedSession::begin_allow_threads() 
{
	return true;
}

bool ManagedSession::end_allow_threads() 
{
	return true;
}

void ManagedSession::check_hangup_hook() 
{
	mono_thread_attach(globals.domain);
	if (!hangupDelegateHandle) {
		return;
	}
		
	MonoObject * hangupDelegate = mono_gchandle_get_target(hangupDelegateHandle);
	if (!hangupDelegate) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "hangupDelegateHandle didn't get an object.");
		return;
	}
		
	MonoObject * ex = NULL;
	mono_runtime_delegate_invoke(hangupDelegate, NULL, &ex);
	if (ex) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "hangupDelegate threw an exception.");
	}
}

switch_status_t ManagedSession::run_dtmf_callback(void *input, switch_input_type_t itype) 
{
	mono_thread_attach(globals.domain);
	if (!dtmfDelegateHandle) {
		return SWITCH_STATUS_SUCCESS;
	}
	MonoObject * dtmfDelegate = mono_gchandle_get_target(dtmfDelegateHandle);
	if (!dtmfDelegate) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "dtmfDelegateHandle didn't get an object.");
		return SWITCH_STATUS_SUCCESS;
	}
	
	void *args[2];
	args[0] = &input;
	args[1] = &itype;
	MonoObject * ex = NULL;
	MonoObject * res = mono_runtime_delegate_invoke(dtmfDelegate, args, &ex);
	if (ex) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "dtmfDelegate threw an exception.");
		return SWITCH_STATUS_FALSE;
	}

	char *resPtr = mono_string_to_utf8((MonoString *) res);
	switch_status_t status = process_callback_result(resPtr);
	g_free(resPtr);
	return status;
}

#endif
