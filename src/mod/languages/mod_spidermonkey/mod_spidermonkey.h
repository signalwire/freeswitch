/* 
 * FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
 * Copyright (C) 2005-2010, Anthony Minessale II <anthm@freeswitch.org>
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
 * Anthony Minessale II <anthm@freeswitch.org>
 * Portions created by the Initial Developer are Copyright (C)
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 * 
 * Anthony Minessale II <anthm@freeswitch.org>
 *
 *
 * mod_spidermonkey.h -- Javascript Module
 *
 */
#ifndef SWITCH_MOD_SPIDERMONKEY_H
#define SWITCH_MOD_SPIDERMONKEY_H


#include <switch.h>
#include "jstypes.h"
#include "jsarena.h"
#include "jsutil.h"
#include "jsprf.h"
#include "jsapi.h"
#include "jsatom.h"
#include "jscntxt.h"
#include "jsdbgapi.h"
#include "jsemit.h"
#include "jsfun.h"
#include "jsgc.h"
#include "jslock.h"
#include "jsobj.h"
#include "jsparse.h"
#include "jsscope.h"
#include "jsscript.h"

SWITCH_BEGIN_EXTERN_C
#define JS_BUFFER_SIZE 1024 * 32
#define JS_BLOCK_SIZE JS_BUFFER_SIZE
#ifdef __ICC
#pragma warning (disable:310 193 1418)
#endif
#ifdef _MSC_VER
#pragma warning(disable: 4311)
#endif
#ifdef WIN32
#if defined(SWITCH_SM_DECLARE_STATIC)
#define SWITCH_SM_DECLARE(type)		type __cdecl
#elif defined(SM_EXPORTS)
#define SWITCH_SM_DECLARE(type)		__declspec(dllexport) type __cdecl
#else
#define SWITCH_SM_DECLARE(type)		__declspec(dllimport) type __cdecl
#endif
#else //not win32
#define SWITCH_SM_DECLARE(type) type
#endif
int eval_some_js(const char *code, JSContext * cx, JSObject * obj, jsval * rval)
{
	JSScript *script = NULL;
	const char *cptr;
	char *path = NULL;
	const char *script_name = NULL;
	int result = 0;

	JS_ClearPendingException(cx);

	if (code[0] == '~') {
		cptr = code + 1;
		script = JS_CompileScript(cx, obj, cptr, strlen(cptr), "inline", 1);
	} else {
		if (switch_is_file_path(code)) {
			script_name = code;
		} else if ((path = switch_mprintf("%s%s%s", SWITCH_GLOBAL_dirs.script_dir, SWITCH_PATH_SEPARATOR, code))) {
			script_name = path;
		}
		if (script_name) {
			if (switch_file_exists(script_name, NULL) == SWITCH_STATUS_SUCCESS) {
				script = JS_CompileFile(cx, obj, script_name);
			} else {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Cannot Open File: %s\n", script_name);
			}
		}
	}

	if (script) {
		result = JS_ExecuteScript(cx, obj, script, rval) == JS_TRUE ? 1 : 0;
		JS_DestroyScript(cx, script);
	} else {
		result = -1;
	}

	switch_safe_free(path);
	return result;
}


typedef switch_status_t (*spidermonkey_load_t) (JSContext * cx, JSObject * obj);

struct sm_module_interface {
	const char *name;
	spidermonkey_load_t spidermonkey_load;
	const struct sm_module_interface *next;
};

typedef struct sm_module_interface sm_module_interface_t;
typedef switch_status_t (*spidermonkey_init_t) (const sm_module_interface_t ** module_interface);

struct js_session_speech {
	switch_speech_handle_t sh;
	switch_codec_t codec;
};

struct js_session {
	switch_core_session_t *session;
	JSContext *cx;
	JSObject *obj;
	unsigned int flags;
	switch_call_cause_t cause;
	JSFunction *on_hangup;
	int stack_depth;
	switch_channel_state_t hook_state;
	char *destination_number;
	char *dialplan;
	char *caller_id_name;
	char *caller_id_number;
	char *network_addr;
	char *ani;
	char *aniii;
	char *rdnis;
	char *context;
	char *username;
	int check_state;
	struct js_session_speech *speech;
};

JSBool DEFAULT_SET_PROPERTY(JSContext * cx, JSObject * obj, jsval id, jsval * vp)
{
	eval_some_js("~throw new Error(\"this property cannot be changed!\");", cx, obj, vp);
	return JS_FALSE;
}



SWITCH_END_EXTERN_C
#endif
