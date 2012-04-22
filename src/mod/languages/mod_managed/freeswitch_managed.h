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
 * freeswitch_managed.h -- Header for ManagedSession and globals
 *
 */

#ifndef FREESWITCH_MANAGED_H
#define FREESWITCH_MANAGED_H

SWITCH_BEGIN_EXTERN_C
#include <switch.h>
#include <switch_cpp.h>
typedef void (*hangupFunction) (void);
typedef char *(*inputFunction) (void *, switch_input_type_t);

#ifndef _MANAGED
#include <mono/jit/jit.h>
#include <mono/metadata/assembly.h>
#include <mono/metadata/environment.h>
#include <mono/metadata/mono-config.h>
#include <mono/metadata/threads.h>
#include <mono/metadata/debug-helpers.h>
#endif

#ifndef SWIG
struct mod_managed_globals {
	switch_memory_pool_t *pool;
#ifndef _MANAGED
	MonoDomain *domain;
	MonoAssembly *mod_mono_asm;
	switch_bool_t embedded;

	MonoMethod *loadMethod;
#endif
};
typedef struct mod_managed_globals mod_managed_globals;
extern mod_managed_globals globals;
#endif

#ifdef _MANAGED
#define ATTACH_THREADS
#else
#define ATTACH_THREADS mono_thread_attach(globals.domain);
#endif

#ifdef WIN32
#define RESULT_FREE(x) CoTaskMemFree(x)
#else
#define RESULT_FREE(x) mono_free(x)
#endif

SWITCH_END_EXTERN_C
#ifdef _MANAGED
// this section remove linker error LNK4248 for these opaque structures
	struct switch_core_session {
	char foo[];
};
struct apr_pool_t {
	char foo[];
};
struct switch_channel {
	char foo[];
};
struct apr_thread_t {
	char foo[];
};
struct switch_hash {
	char foo[];
};
struct apr_thread_mutex_t {
	char foo[];
};
struct switch_network_list {
	char foo[];
};
struct switch_xml_binding {
	char foo[];
};
struct apr_sockaddr_t {
	char foo[];
};
struct switch_core_port_allocator {
	char foo[];
};
struct switch_media_bug {
	char foo[];
};
struct switch_rtp {
	char foo[];
};
struct sqlite3_stmt {
	char foo[];
};
struct switch_buffer {
	char foo[];
};
struct switch_ivr_menu {
	char foo[];
};
struct switch_event_node {
	char foo[];
};
struct switch_ivr_digit_stream_parser {
	char foo[];
};
struct sqlite3 {
	char foo[];
};
struct switch_ivr_digit_stream {
	char foo[];
};
struct real_pcre {
	char foo[];
};
struct HashElem {
	char foo[];
};
struct switch_ivr_menu_xml_ctx {
	char foo[];
};
struct apr_file_t {
	char foo[];
};
struct apr_thread_rwlock_t {
	char foo[];
};
struct apr_pollfd_t {
	char foo[];
};
struct apr_queue_t {
	char foo[];
};
struct apr_socket_t {
	char foo[];
};
// LNK Error

using namespace System;
using namespace System::Reflection;
using namespace System::Runtime::InteropServices;

public ref class FreeSwitchManaged {
  public:
	static Assembly ^ mod_dotnet_managed;
	static MethodInfo ^ loadMethod;
};

#endif

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
