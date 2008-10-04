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
#define ATTACH_THREADS
#else
#define ATTACH_THREADS mono_thread_attach(globals.domain);
#endif

ManagedSession::ManagedSession():CoreSession() 
{

} 

ManagedSession::ManagedSession(char *uuid):CoreSession(uuid) 
{

} 

ManagedSession::ManagedSession(switch_core_session_t *session):CoreSession(session) 
{

} 

bool ManagedSession::begin_allow_threads() 
{
	return true;
}

bool ManagedSession::end_allow_threads() 
{
	return true;
}


ManagedSession::~ManagedSession() 
{
	ATTACH_THREADS
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

void ManagedSession::check_hangup_hook() 
{
	ATTACH_THREADS
	if (!hangupDelegate) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "hangupDelegate is null.");
		return;
	}
	hangupDelegate();
}

switch_status_t ManagedSession::run_dtmf_callback(void *input, switch_input_type_t itype) 
{
	ATTACH_THREADS
	if (!dtmfDelegate) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "dtmfDelegate is null.");
		return SWITCH_STATUS_FALSE;;
	}
	char *result = dtmfDelegate(input, itype);
	switch_status_t status = process_callback_result(result);

#if WIN32
	CoTaskMemFree(result); 
#else
	g_free(result)
#endif
	return status;
}

