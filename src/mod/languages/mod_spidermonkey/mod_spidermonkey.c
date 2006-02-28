/* 
 * FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
 * Copyright (C) 2005/2006, Anthony Minessale II <anthmct@yahoo.com>
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
 * Anthony Minessale II <anthmct@yahoo.com>
 * Portions created by the Initial Developer are Copyright (C)
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 * 
 * Anthony Minessale II <anthmct@yahoo.com>
 *
 *
 * mod_spidermonkey.c -- Javascript Module
 *
 */
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
#include <switch.h>

#ifdef _MSC_VER
#pragma warning(disable: 4311)
#endif

static const char modname[] = "mod_spidermonkey";

static int eval_some_js(char *code, JSContext *cx, JSObject *obj, jsval *rval);

static struct {
	size_t gStackChunkSize;
	jsuword gStackBase;
	int gExitCode;
	JSBool gQuitting;
	FILE *gErrFile;
	FILE *gOutFile;
	int stackDummy;
	JSRuntime *rt;
} globals;


//extern JSClass global_class;
static JSClass global_class = {
    "Global", JSCLASS_HAS_PRIVATE, 
    JS_PropertyStub,  JS_PropertyStub,  JS_PropertyStub,  JS_PropertyStub, 
    JS_EnumerateStub, JS_ResolveStub,   JS_ConvertStub,   JS_FinalizeStub
};


struct dtmf_callback_state {
	struct js_session *session_state;
	char code_buffer[1024];
	int code_buffer_len;
	char ret_buffer[1024];
	int ret_buffer_len;
	int digit_count;
	void *extra;
};

struct js_session {
	switch_core_session *session;
	JSContext *cx;
	JSObject *obj;
	unsigned int flags;
};


static void js_error(JSContext *cx, const char *message, JSErrorReport *report)
{
    if (message) {
        switch_console_printf(SWITCH_CHANNEL_CONSOLE, "%s\n", message);
    }
	
}

static switch_status init_js(void)
{
	memset(&globals, 0, sizeof(globals));
	globals.gQuitting = JS_FALSE;
	globals.gErrFile = NULL;
	globals.gOutFile = NULL;
	globals.gStackChunkSize = 8192;
	globals.gStackBase = (jsuword)&globals.stackDummy;
	globals.gErrFile = stderr;
	globals.gOutFile = stdout;


    if (!(globals.rt = JS_NewRuntime(64L * 1024L * 1024L))) {
		return SWITCH_STATUS_FALSE;
	}

	

	return SWITCH_STATUS_SUCCESS;

}

static switch_status js_dtmf_callback(switch_core_session *session, char *dtmf, void *buf, unsigned int buflen)
{
	char code[2048];
	struct dtmf_callback_state *cb_state = buf;
	struct js_session *jss = cb_state->session_state;
	switch_file_handle *fh = cb_state->extra;
	jsval rval;
	char *ret;
	
	if(!jss) {
		return SWITCH_STATUS_FALSE;
	}
	
	if (cb_state->digit_count || (cb_state->code_buffer[0] > 47 && cb_state->code_buffer[0] < 58)) {
		char *d;
		if (!cb_state->digit_count) {
			cb_state->digit_count = atoi(cb_state->code_buffer);
		}

		for(d = dtmf; *d; d++) {
			cb_state->ret_buffer[cb_state->ret_buffer_len++] = *d;
			if ((cb_state->ret_buffer_len > cb_state->digit_count)||
				(cb_state->ret_buffer_len > sizeof(cb_state->ret_buffer))||
				(cb_state->ret_buffer_len >= cb_state->digit_count)
				) {
				return SWITCH_STATUS_FALSE;
			}
		}
		return SWITCH_STATUS_SUCCESS;
	} else {
		snprintf(code, sizeof(code), "~%s(\"%s\")", cb_state->code_buffer, dtmf);
		eval_some_js(code, jss->cx, jss->obj, &rval);
		ret = JS_GetStringBytes(JS_ValueToString(jss->cx, rval));

		if (*ret == 'F') {
			int step;
			ret++;
			
			step = *ret ? atoi(ret) : 1;
			fh->speed += step ? step : 1;
			return SWITCH_STATUS_SUCCESS;
		}

		if (*ret == 'S') {
			int step;
            ret++;

			step = *ret ? atoi(ret) : 1;
            fh->speed -= step ? step : 1;
			return SWITCH_STATUS_SUCCESS;
		}

		if (*ret == 'N') {
			fh->speed = 0;
			return SWITCH_STATUS_SUCCESS;
		}
		
		if (*ret == 'P') {
			if (switch_test_flag(fh, SWITCH_FILE_PAUSE)) {
				switch_clear_flag(fh, SWITCH_FILE_PAUSE);
			} else {
				switch_set_flag(fh, SWITCH_FILE_PAUSE);
			}
			return SWITCH_STATUS_SUCCESS;
		}

		if (*ret == 'R') {
			unsigned int pos = 0;
			fh->speed = 0;
			switch_core_file_seek(fh, &pos, 0, SEEK_SET);
			return SWITCH_STATUS_SUCCESS;
		}

		if (*ret == '+' || *ret == '-') {
			switch_codec *codec;
			codec = switch_core_session_get_read_codec(jss->session);
			unsigned int samps = 0;
			unsigned int pos = 0;
			if (*ret == '+') {
				ret++;
				samps = atoi(ret) * (codec->implementation->samples_per_second / 1000);
				switch_core_file_seek(fh, &pos, samps, SEEK_CUR);
			} else {
				samps = atoi(ret) * (codec->implementation->samples_per_second / 1000);
				switch_core_file_seek(fh, &pos, fh->pos - samps, SEEK_SET);
			}

			return SWITCH_STATUS_SUCCESS;
		}

		if (!strcmp(ret, "true") || !strcmp(ret, "undefined")) {
			return SWITCH_STATUS_SUCCESS;
		}
	
		if (ret) {
			switch_copy_string(cb_state->ret_buffer, ret, sizeof(cb_state->ret_buffer));
		}
	}

	return SWITCH_STATUS_FALSE;
}

static JSBool chan_streamfile(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	struct js_session *jss = JS_GetPrivate(cx, obj);
    switch_channel *channel;
	char *file_name = NULL;
	char *timer_name = NULL;
	char *dtmf_callback = NULL;
	void *bp = NULL;
	int len = 0;
	switch_dtmf_callback_function dtmf_func = NULL;
	struct dtmf_callback_state cb_state = {0};
	switch_file_handle fh;

	channel = switch_core_session_get_channel(jss->session);
    assert(channel != NULL);
	
	if (argc > 0) {
		file_name = JS_GetStringBytes(JS_ValueToString(cx, argv[0]));
		if (switch_strlen_zero(file_name)) {
			return JS_FALSE;
		}
	}
	if (argc > 1) {
		timer_name = JS_GetStringBytes(JS_ValueToString(cx, argv[1]));
		if (switch_strlen_zero(timer_name)) {
			timer_name = NULL;
		}
	}
	if (argc > 2) {
		dtmf_callback = JS_GetStringBytes(JS_ValueToString(cx, argv[2]));
		if (switch_strlen_zero(dtmf_callback)) {
			dtmf_callback = NULL;
		} else {
			memset(&cb_state, 0, sizeof(cb_state));
			switch_copy_string(cb_state.code_buffer, dtmf_callback, sizeof(cb_state.code_buffer));
			cb_state.code_buffer_len = strlen(cb_state.code_buffer);
			cb_state.session_state = jss;
			dtmf_func = js_dtmf_callback;
			bp = &cb_state;
			len = sizeof(cb_state);
		}
	}

	memset(&fh, 0, sizeof(fh));
	cb_state.extra = &fh;

	switch_ivr_play_file(jss->session, &fh, file_name, timer_name, dtmf_func, bp, len);
	*rval = STRING_TO_JSVAL (JS_NewStringCopyZ(cx, cb_state.ret_buffer));

	return (switch_channel_get_state(channel) == CS_EXECUTE) ? JS_TRUE : JS_FALSE;
}

static JSBool chan_speak(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	struct js_session *jss = JS_GetPrivate(cx, obj);
    switch_channel *channel;
	char *tts_name = NULL;
	char *voice_name = NULL;
	char *text = NULL;
	switch_codec *codec;
	char buf[10] = "";
	void *bp = NULL;
	int len = 0;
	//switch_dtmf_callback_function dtmf_func = NULL;

	channel = switch_core_session_get_channel(jss->session);
    assert(channel != NULL);
	
	if (argc > 0) {
		tts_name = JS_GetStringBytes(JS_ValueToString(cx, argv[0]));
	}
	if (argc > 1) {
		voice_name= JS_GetStringBytes(JS_ValueToString(cx, argv[1]));
	}
	if (argc > 2) {
		text = JS_GetStringBytes(JS_ValueToString(cx, argv[2]));
	}
	if (argc > 3) {
		bp = buf;
		len = sizeof(buf);
	}

	if (!tts_name && text) {
		return JS_FALSE;
	}

	codec = switch_core_session_get_read_codec(jss->session);
	switch_ivr_speak_text(jss->session,
						  tts_name,
						  voice_name && strlen(voice_name) ? voice_name : NULL, 
						  NULL,
						  codec->implementation->samples_per_second,
						  NULL,
						  text,
						  bp,
						  len);
	if(len) {
		*rval = STRING_TO_JSVAL ( JS_NewStringCopyZ(cx, buf) );
	}
	return JS_TRUE;
	
}

static JSBool chan_get_digits(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	struct js_session *jss = JS_GetPrivate(cx, obj);
	char *terminators = NULL;
	char *buf;
	int digits;

	if (argc > 0) {
		char term;
		digits = atoi(JS_GetStringBytes(JS_ValueToString(cx, argv[0])));
		if (argc > 1) {
			terminators = JS_GetStringBytes(JS_ValueToString(cx, argv[1]));
		}
		buf = switch_core_session_alloc(jss->session, digits);
		switch_ivr_collect_digits_count(jss->session, buf, digits, digits, terminators, &term);
		*rval = STRING_TO_JSVAL ( JS_NewStringCopyZ(cx, buf) );
		return JS_TRUE;
	}

	return JS_FALSE;
}

static JSBool chan_answer(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	struct js_session *jss = JS_GetPrivate(cx, obj);
	switch_channel *channel;

	channel = switch_core_session_get_channel(jss->session);
	assert(channel != NULL);

	switch_channel_answer(channel);
	return JS_TRUE;
}

enum its_tinyid {
    CHAN_NAME, CHAN_STATE
};

static JSFunctionSpec chan_methods[] = {
    {"streamFile", chan_streamfile, 1}, 
    {"speak", chan_speak, 1}, 
    {"getDigits", chan_get_digits, 1},
    {"answer", chan_answer, 0}, 
	{0}
};


static JSPropertySpec chan_props[] = {
    {"name", CHAN_NAME, JSPROP_READONLY|JSPROP_PERMANENT}, 
    {"state", CHAN_STATE, JSPROP_READONLY|JSPROP_PERMANENT}, 
    {0}
};

static JSBool chan_getProperty(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
	struct js_session *jss = JS_GetPrivate(cx, obj);
	int param = 0;
	JSBool res = JS_TRUE;
	switch_channel *channel;
	char *name;

	channel = switch_core_session_get_channel(jss->session);
	assert(channel != NULL);

	name = JS_GetStringBytes(JS_ValueToString(cx, id));
	/* numbers are our props anything else is a method */
	if (name[0] >= 48 && name[0] <= 57) {
		param = atoi(name);
	} else {
		return JS_TRUE;
	}
	
	switch(param) {
	case CHAN_NAME:
		*vp = STRING_TO_JSVAL(JS_NewStringCopyZ(cx, switch_channel_get_name(channel)));
		break;
	case CHAN_STATE:
		*vp = STRING_TO_JSVAL(JS_NewStringCopyZ(cx, switch_channel_state_name(switch_channel_get_state(channel)) ));
		break;
	default:
		res = JS_FALSE;
		break;

	}

    return res;
}


JSClass session_class = {
    "Session", JSCLASS_HAS_PRIVATE, 
	JS_PropertyStub,  JS_PropertyStub,  chan_getProperty,  JS_PropertyStub, 
    JS_EnumerateStub, JS_ResolveStub,   JS_ConvertStub,   JS_FinalizeStub
};


static JSBool js_log(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	char *msg;

	if((msg = JS_GetStringBytes(JS_ValueToString(cx, argv[0])))) {
		switch_console_printf(SWITCH_CHANNEL_CONSOLE, "JS_LOG: %s", msg);
		return JS_TRUE;
	} 

	return JS_FALSE;
}


static JSFunctionSpec fs_functions[] = {
	{"console_log", js_log, 2}, 
	{0}
};


static JSObject *new_js_session(JSContext *cx, JSObject *obj, switch_core_session *session, struct js_session *jss, char *name, int flags) 
{
	JSObject *session_obj;
	if ((session_obj = JS_DefineObject(cx, obj, name, &session_class, NULL, 0))) {
		memset(jss, 0, sizeof(struct js_session));
		jss->session = session;
		jss->flags = flags;
		jss->cx = cx;
		jss->obj = obj;
		if ((JS_SetPrivate(cx, session_obj, jss) &&
			  JS_DefineProperties(cx, session_obj, chan_props) &&
			  JS_DefineFunctions(cx, session_obj, chan_methods))) {
			return session_obj;
		}
	}
	
	return NULL;
}

static int eval_some_js(char *code, JSContext *cx, JSObject *obj, jsval *rval) 
{
	JSScript *script;
	char *cptr;
	char path[512];
	int res = 0;

	JS_ClearPendingException(cx);

	if (code[0] == '~') {
		cptr = code + 1;
		script = JS_CompileScript(cx, obj, cptr, strlen(cptr), "inline", 1);
	} else {
		if (code[0] == '/') {
			script = JS_CompileFile(cx, obj, code);
		} else {
			snprintf(path, sizeof(path), "%s/%s", "/tmp", code);
			script = JS_CompileFile(cx, obj, path);
		}
	}

	if (script) {
		res = JS_ExecuteScript(cx, obj, script, rval) == JS_TRUE ? 1 : 0;
		JS_DestroyScript(cx, script);
	}

	return res;
}

static void js_exec(switch_core_session *session, char *data)
{
	char *code, *next, *arg, *nextarg;
	int res=-1;
	jsval rval;
	JSContext *cx;
	JSObject *javascript_global_object, *session_obj;
	struct js_session jss;
	int x = 0, y = 0;
	char buf[512];
	int flags = 0;
	switch_channel *channel;

	channel = switch_core_session_get_channel(session);
    assert(channel != NULL);

	if (!data) {
		switch_console_printf(SWITCH_CHANNEL_CONSOLE, "js requires an argument (filename|code)\n");
		return;
	}

	code = switch_core_session_strdup(session,(char *)data);
	if (code[0] == '-') {
		if ((next = strchr(code, '|'))) {
			*next = '\0';
			next++;
		}
		code++;
		code = next;
	}
	
	if (!code) {
		switch_console_printf(SWITCH_CHANNEL_CONSOLE, "js requires an argument (filename|code)\n");
		return;
	}
	
    if ((cx = JS_NewContext(globals.rt, globals.gStackChunkSize))) {
		JS_SetErrorReporter(cx, js_error);
		if ((javascript_global_object = JS_NewObject(cx, &global_class, NULL, NULL)) && 
			JS_DefineFunctions(cx, javascript_global_object, fs_functions) &&
			JS_InitStandardClasses(cx, javascript_global_object) &&
			(session_obj = new_js_session(cx, javascript_global_object, session, &jss, "session", flags))) {
			JS_SetGlobalObject(cx, javascript_global_object);
			JS_SetPrivate(cx, javascript_global_object, session);
			res = 0;
	
			do {
				if ((next = strchr(code, '|'))) {
					*next = '\0';
					next++;
				}
				if ((arg = strchr(code, ':'))) {
					for (y=0;(arg=strchr(arg, ':'));y++)
						arg++;
					arg = strchr(code, ':');
					*arg = '\0';
					arg++;
					snprintf(buf, sizeof(buf), "~var Argv = new Array(%d);", y);
					eval_some_js(buf, cx, javascript_global_object, &rval);
					snprintf(buf, sizeof(buf), "~var argc = %d", y);
					eval_some_js(buf, cx, javascript_global_object, &rval);
					do {
						if ((nextarg = strchr(arg, ':'))) {
							*nextarg = '\0';
							nextarg++;
						}
						snprintf(buf, sizeof(buf), "~Argv[%d] = \"%s\";", x++, arg);
						eval_some_js(buf, cx, javascript_global_object, &rval);
						arg = nextarg;
					} while (arg);
				}
				if (!(res=eval_some_js(code, cx, javascript_global_object, &rval))) {
					break;
				}
				code = next;
			} while (code);
		}
	}
	
	if (cx) {
		JS_DestroyContext(cx);
	}
}

static const switch_application_interface ivrtest_application_interface = {
	/*.interface_name */ "javascript",
	/*.application_function */ js_exec,
	NULL, NULL, NULL,
	/*.next*/ NULL
};


static switch_loadable_module_interface spidermonkey_module_interface = {
	/*.module_name */ modname,
	/*.endpoint_interface */ NULL,
	/*.timer_interface */ NULL,
	/*.dialplan_interface */ NULL,
	/*.codec_interface */ NULL,
	/*.application_interface */ &ivrtest_application_interface,
	/*.api_interface */ NULL,
	/*.file_interface */ NULL,
	/*.speech_interface */ NULL,
	/*.directory_interface */ NULL
};

switch_status switch_module_load(const switch_loadable_module_interface **interface, char *filename)
{
	switch_status status;

	if((status = init_js()) != SWITCH_STATUS_SUCCESS) {
		return status;
	}

	/* connect my internal structure to the blank pointer passed to me */
	*interface = &spidermonkey_module_interface;

	switch_console_printf(SWITCH_CHANNEL_CONSOLE, "Hello World!\n");

	/* indicate that the module should continue to be loaded */
	return SWITCH_STATUS_SUCCESS;
}
