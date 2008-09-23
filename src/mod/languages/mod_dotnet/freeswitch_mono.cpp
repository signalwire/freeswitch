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
 * Jeff Lenk <jlenk@frontiernet.net> - Modified class to support Dotnet
 * 
 * freeswitch_mono.cpp -- Dotnet-specific CoreSession subclass
 *
 */  
	
#include <switch.h>
#include <switch_cpp.h>
#include "freeswitch_mono.h"

MonoSession::MonoSession():CoreSession() 
{

} 

MonoSession::MonoSession(char *uuid):CoreSession(uuid) 
{

} 

MonoSession::MonoSession(switch_core_session_t *session):CoreSession(session) 
{

} 

MonoSession::~MonoSession() 
{

	// Do auto-hangup ourselves because CoreSession can't call check_hangup_hook 
	// after MonoSession destruction (cause at point it's pure virtual)
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



bool MonoSession::begin_allow_threads() 
{
	return true;
}



bool MonoSession::end_allow_threads() 
{
	return true;
}


void MonoSession::check_hangup_hook() 
{
	
	if (!hangupDelegateHandle) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "hangupDelegateHandle didn't get an object.");
		return;
	}
	hangupDelegateHandle();
}


switch_status_t MonoSession::run_dtmf_callback(void *input, switch_input_type_t itype) 
{

	if (!dtmfDelegateHandle) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "dtmfDelegateHandle didn't get an object.");
		return SWITCH_STATUS_FALSE;;
	}
	char* result = dtmfDelegateHandle(input, itype);

	switch_status_t status = process_callback_result(result);
	return status;
}


