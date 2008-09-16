/* 
 * FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application - mod_mono
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
 * The Original Code is FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application - mod_mono
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
 * freeswitch_mono.h -- Header for MonoSession and globals
 *
 */

#ifndef FREESWITCH_MONO_H
#define FREESWITCH_MONO_H

SWITCH_BEGIN_EXTERN_C

#include <switch.h>
#include <switch_cpp.h>
#include <mono/jit/jit.h>
#include <mono/metadata/assembly.h>

#ifndef SWIG
struct mod_mono_globals {
	MonoDomain *domain;
	MonoAssembly *mod_mono_asm;
	switch_memory_pool_t *pool;
	switch_bool_t embedded;

	MonoMethod *loadMethod;
	MonoMethod *unloadMethod;
	MonoMethod *runMethod;
	MonoMethod *executeMethod;
	MonoMethod *executeBackgroundMethod;
};

typedef struct mod_mono_globals mod_mono_globals;
extern mod_mono_globals globals;
#endif

SWITCH_END_EXTERN_C

class MonoSession : public CoreSession 
{
public:
	MonoSession();
	MonoSession(char *uuid);
	MonoSession(switch_core_session_t *session);
	virtual ~MonoSession();        

	virtual bool begin_allow_threads();
	virtual bool end_allow_threads();
	virtual void check_hangup_hook();

	virtual switch_status_t run_dtmf_callback(void *input, switch_input_type_t itype);

	guint32 dtmfDelegateHandle; // GCHandle to the input delegate 
	guint32 hangupDelegateHandle; // GCHandle to the hangup delegate
};

#endif
