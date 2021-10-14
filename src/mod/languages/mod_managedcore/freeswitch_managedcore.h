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
 * The Original Code is FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application - mod_managedcore
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
 * freeswitch_managed.h -- Header for ManagedSession and managed_globals
 *
 */

#ifndef FREESWITCH_MANAGEDCORE_H
#define FREESWITCH_MANAGEDCORE_H


#include "coreclr_delegates.h"


SWITCH_BEGIN_EXTERN_C
#include <switch.h>
#include <switch_cpp.h>
typedef void (*hangupFunction) (void);
typedef char *(*inputFunction) (void *, switch_input_type_t);
typedef int(*loaderFunction) (void);


#ifndef SWIG
struct mod_managed_globals {
	switch_memory_pool_t *pool;
	load_assembly_and_get_function_pointer_fn load_assembly_and_get_function_pointer;
	loaderFunction loadMethod;
};
typedef struct mod_managed_globals mod_managed_globals;
extern mod_managed_globals managed_globals;
#endif


SWITCH_END_EXTERN_C

class ManagedSession:public CoreSession {
  public:
	ManagedSession(void);
	ManagedSession(char *uuid);
	ManagedSession(switch_core_session_t *session);
	virtual ~ ManagedSession();

	virtual bool begin_allow_threads();
	virtual bool end_allow_threads();
	virtual void check_hangup_hook();
	virtual switch_status_t run_dtmf_callback(void *input, switch_input_type_t itype);

	// P/Invoke function pointer to delegates
	inputFunction dtmfDelegate;
	hangupFunction hangupDelegate;
};

#endif
