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
#ifndef HAVE_CURL
#define HAVE_CURL
#endif

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
#include <libteletone.h>
#ifdef HAVE_CURL
#include <curl/curl.h>
#endif

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


static JSClass global_class = {
    "Global", JSCLASS_HAS_PRIVATE, 
    JS_PropertyStub,  JS_PropertyStub,  JS_PropertyStub,  JS_PropertyStub, 
    JS_EnumerateStub, JS_ResolveStub,   JS_ConvertStub,   JS_FinalizeStub
};

typedef enum {
	TTF_DTMF = (1 << 0)
} teletone_flag_t;

struct dtmf_callback_state {
	struct js_session *session_state;
	char code_buffer[1024];
	size_t code_buffer_len;
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

struct teletone_obj {
	teletone_generation_session_t ts;
	JSContext *cx;
	JSObject *obj;
	switch_core_session *session;
	switch_codec codec;
	switch_buffer *audio_buffer;
	switch_buffer *loop_buffer;
	switch_memory_pool *pool;
	switch_timer *timer;
	switch_timer timer_base;
	char code_buffer[1024];
	char ret_val[1024];
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

static switch_status js_stream_dtmf_callback(switch_core_session *session, char *dtmf, void *buf, unsigned int buflen)
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

		if (!strncasecmp(ret, "speed", 4)) {
			char *p;
			
			if ((p = strchr(ret, ':'))) {
				p++;
				if (*p == '+' || *p == '-') {
					int step;
					if (!(step = atoi(p))) {
						step = 1;
					}
					fh->speed += step;
				} else {
					int speed = atoi(p);
					fh->speed = speed;
				}
				return SWITCH_STATUS_SUCCESS;
			}
			
			return SWITCH_STATUS_FALSE;
		} else if (!strcasecmp(ret, "pause")) {
			if (switch_test_flag(fh, SWITCH_FILE_PAUSE)) {
				switch_clear_flag(fh, SWITCH_FILE_PAUSE);
			} else {
				switch_set_flag(fh, SWITCH_FILE_PAUSE);
			}
			return SWITCH_STATUS_SUCCESS;
		} else if (!strcasecmp(ret, "restart")) {
			unsigned int pos = 0;
			fh->speed = 0;
			switch_core_file_seek(fh, &pos, 0, SEEK_SET);
			return SWITCH_STATUS_SUCCESS;
		} else if (!strncasecmp(ret, "seek", 4)) {
			switch_codec *codec;
			unsigned int samps = 0;
			unsigned int pos = 0;
			char *p;
			codec = switch_core_session_get_read_codec(jss->session);

			if ((p = strchr(ret, ':'))) {
				p++;
				if (*p == '+' || *p == '-') {
                    int step;
                    if (!(step = atoi(p))) {
                        step = 1000;
                    }
					if (step > 0) {
						samps = step * (codec->implementation->samples_per_second / 1000);
						switch_core_file_seek(fh, &pos, samps, SEEK_CUR);		
					} else {
						samps = step * (codec->implementation->samples_per_second / 1000);
						switch_core_file_seek(fh, &pos, fh->pos - samps, SEEK_SET);
					}
                } else {
					samps = atoi(p) * (codec->implementation->samples_per_second / 1000);
					switch_core_file_seek(fh, &pos, samps, SEEK_SET);
                }
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


static switch_status js_record_dtmf_callback(switch_core_session *session, char *dtmf, void *buf, unsigned int buflen)
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

		if (!strcasecmp(ret, "pause")) {
			if (switch_test_flag(fh, SWITCH_FILE_PAUSE)) {
				switch_clear_flag(fh, SWITCH_FILE_PAUSE);
			} else {
				switch_set_flag(fh, SWITCH_FILE_PAUSE);
			}
			return SWITCH_STATUS_SUCCESS;
		} else if (!strcasecmp(ret, "restart")) {
			unsigned int pos = 0;
			fh->speed = 0;
			switch_core_file_seek(fh, &pos, 0, SEEK_SET);
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


static switch_status js_speak_dtmf_callback(switch_core_session *session, char *dtmf, void *buf, unsigned int buflen)
{
	char code[2048];
	struct dtmf_callback_state *cb_state = buf;
	struct js_session *jss = cb_state->session_state;
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

		if (!strcmp(ret, "true") || !strcmp(ret, "undefined")) {
			return SWITCH_STATUS_SUCCESS;
		}
	
		if (ret) {
			switch_copy_string(cb_state->ret_buffer, ret, sizeof(cb_state->ret_buffer));
		}
	}

	return SWITCH_STATUS_FALSE;
}

static JSBool session_recordfile(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	struct js_session *jss = JS_GetPrivate(cx, obj);
    switch_channel *channel;
	char *file_name = NULL;
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
		dtmf_callback = JS_GetStringBytes(JS_ValueToString(cx, argv[1]));
		if (switch_strlen_zero(dtmf_callback)) {
			dtmf_callback = NULL;
		} else {
			memset(&cb_state, 0, sizeof(cb_state));
			switch_copy_string(cb_state.code_buffer, dtmf_callback, sizeof(cb_state.code_buffer));
			cb_state.code_buffer_len = strlen(cb_state.code_buffer);
			cb_state.session_state = jss;
			dtmf_func = js_record_dtmf_callback;
			bp = &cb_state;
			len = sizeof(cb_state);
		}
	}

	memset(&fh, 0, sizeof(fh));
	cb_state.extra = &fh;

	switch_ivr_record_file(jss->session, &fh, file_name, dtmf_func, bp, len);
	*rval = STRING_TO_JSVAL (JS_NewStringCopyZ(cx, cb_state.ret_buffer));

	return (switch_channel_get_state(channel) == CS_EXECUTE) ? JS_TRUE : JS_FALSE;
}

static JSBool session_streamfile(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
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
			dtmf_func = js_stream_dtmf_callback;
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

static JSBool session_speak(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	struct js_session *jss = JS_GetPrivate(cx, obj);
    switch_channel *channel;
	char *tts_name = NULL;
	char *voice_name = NULL;
	char *text = NULL;
	char *dtmf_callback = NULL;
	switch_codec *codec;
	void *bp = NULL;
	int len = 0;
	struct dtmf_callback_state cb_state = {0};
	switch_dtmf_callback_function dtmf_func = NULL;

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
		dtmf_callback = JS_GetStringBytes(JS_ValueToString(cx, argv[3]));
		if (switch_strlen_zero(dtmf_callback)) {
			dtmf_callback = NULL;
		} else {
			memset(&cb_state, 0, sizeof(cb_state));
			switch_copy_string(cb_state.code_buffer, dtmf_callback, sizeof(cb_state.code_buffer));
			cb_state.code_buffer_len = strlen(cb_state.code_buffer);
			cb_state.session_state = jss;
			dtmf_func = js_speak_dtmf_callback;
			bp = &cb_state;
			len = sizeof(cb_state);
		}
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
						  dtmf_func,
						  text,
						  bp,
						  len);

	*rval = STRING_TO_JSVAL (JS_NewStringCopyZ(cx, cb_state.ret_buffer));

	return (switch_channel_get_state(channel) == CS_EXECUTE) ? JS_TRUE : JS_FALSE;
}

static JSBool session_get_digits(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	struct js_session *jss = JS_GetPrivate(cx, obj);
	char *terminators = NULL;
	char *buf;
	int digits;
	int32 timeout = 5000;
	int32 poll_chan = 1;
	
	
	if (argc > 0) {
		char term;
		digits = atoi(JS_GetStringBytes(JS_ValueToString(cx, argv[0])));
		if (argc > 1) {
			terminators = JS_GetStringBytes(JS_ValueToString(cx, argv[1]));
		}
		if (argc > 2) {
			JS_ValueToInt32(cx, argv[2], &timeout);
		}
		if (argc > 3) {
			JS_ValueToInt32(cx, argv[3], &poll_chan);
		}
		buf = switch_core_session_alloc(jss->session, digits);
		switch_ivr_collect_digits_count(jss->session, buf, digits, digits, terminators, &term, timeout, poll_chan ? SWITCH_TRUE : SWITCH_FALSE);
		*rval = STRING_TO_JSVAL ( JS_NewStringCopyZ(cx, buf) );
		return JS_TRUE;
	}

	return JS_FALSE;
}

static JSBool session_answer(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	struct js_session *jss = JS_GetPrivate(cx, obj);
	switch_channel *channel;

	channel = switch_core_session_get_channel(jss->session);
	assert(channel != NULL);

	switch_channel_answer(channel);
	return JS_TRUE;
}

#ifdef HAVE_CURL

struct config_data {
	JSContext *cx;
	JSObject *obj;
	char *name;
};

static size_t realtime_callback(void *ptr, size_t size, size_t nmemb, void *data)
{
    register size_t realsize = size * nmemb;
	char *line, lineb[2048], *nextline = NULL, *val = NULL, *p = NULL;
	jsval rval;
	struct config_data *config_data = data;
	char code[256];

	if (config_data->name) {
		switch_copy_string(lineb, (char *) ptr, sizeof(lineb));
		line = lineb;
		while (line) {
			if ((nextline = strchr(line, '\n'))) {
				*nextline = '\0';
				nextline++;
			}
			
			if ((val = strchr(line, '='))) {
                *val = '\0';
                val++;
                if (val[0] == '>') {
                    *val = '\0';
                    val++;
                }
				
                for (p = line; p && *p == ' '; p++);
                line = p;
                for (p=line+strlen(line)-1;*p == ' '; p--)
                    *p = '\0';
                for (p = val; p && *p == ' '; p++);
                val = p;
                for (p=val+strlen(val)-1;*p == ' '; p--)
                    *p = '\0';

				snprintf(code, sizeof(code), "~%s[\"%s\"] = \"%s\"", config_data->name, line, val);
				eval_some_js(code, config_data->cx, config_data->obj, &rval);

            }

			line = nextline;
		}
	} 
	return realsize;

}


static JSBool js_fetchurl(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	char *url = NULL, *name = NULL;
	CURL *curl_handle = NULL;
	struct config_data config_data;
	
	if ( argc > 0 && (url = JS_GetStringBytes(JS_ValueToString(cx, argv[0])))) {
		if (argc > 1)
			name = JS_GetStringBytes(JS_ValueToString(cx, argv[1]));
		curl_global_init(CURL_GLOBAL_ALL);
		curl_handle = curl_easy_init();
		if (!strncasecmp(url, "https", 5)) {
			curl_easy_setopt(curl_handle, CURLOPT_SSL_VERIFYPEER, 0);
			curl_easy_setopt(curl_handle, CURLOPT_SSL_VERIFYHOST, 0);
		}
		config_data.cx = cx;
		config_data.obj = obj;
		if (name)
			config_data.name = name;
		curl_easy_setopt(curl_handle, CURLOPT_URL, url);
		curl_easy_setopt(curl_handle, CURLOPT_WRITEFUNCTION, realtime_callback);
		curl_easy_setopt(curl_handle, CURLOPT_WRITEDATA, (void *)&config_data);
		curl_easy_setopt(curl_handle, CURLOPT_USERAGENT, "asterisk-js/1.0");
		curl_easy_perform(curl_handle);
		curl_easy_cleanup(curl_handle);
    } else {
		switch_console_printf(SWITCH_CHANNEL_CONSOLE, "Error!\n");
		return JS_FALSE;
	}

	return JS_TRUE;
}
#endif

/* Session Object */
/*********************************************************************************/
enum session_tinyid {
    SESSION_NAME, SESSION_STATE
};

static JSFunctionSpec session_methods[] = {
    {"streamFile", session_streamfile, 1}, 
    {"recordFile", session_recordfile, 1}, 
    {"speak", session_speak, 1}, 
    {"getDigits", session_get_digits, 1},
    {"answer", session_answer, 0}, 
	{0}
};


static JSPropertySpec session_props[] = {
    {"name", SESSION_NAME, JSPROP_READONLY|JSPROP_PERMANENT}, 
    {"state", SESSION_STATE, JSPROP_READONLY|JSPROP_PERMANENT}, 
    {0}
};

static JSBool session_getProperty(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
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
	case SESSION_NAME:
		*vp = STRING_TO_JSVAL(JS_NewStringCopyZ(cx, switch_channel_get_name(channel)));
		break;
	case SESSION_STATE:
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
	JS_PropertyStub,  JS_PropertyStub,  session_getProperty,  JS_PropertyStub, 
    JS_EnumerateStub, JS_ResolveStub,   JS_ConvertStub,   JS_FinalizeStub
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
			  JS_DefineProperties(cx, session_obj, session_props) &&
			  JS_DefineFunctions(cx, session_obj, session_methods))) {
			return session_obj;
		}
	}
	
	return NULL;
}

static int teletone_handler(teletone_generation_session_t *ts, teletone_tone_map_t *map)
{
	struct teletone_obj *tto = ts->user_data;
	int wrote;

	if (!tto) {
		return -1;
	}
	wrote = teletone_mux_tones(ts, map);
	switch_buffer_write(tto->audio_buffer, ts->buffer, wrote * 2);

	return 0;
}

/* TeleTone Object */
/*********************************************************************************/
static JSBool teletone_construct(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	int32 memory = 65535;
	JSObject *session_obj;
	struct teletone_obj *tto = NULL;
	struct js_session *jss = NULL;
	switch_codec *read_codec;
	switch_memory_pool *pool;
	char *timer_name = NULL;

	if (argc > 0) {
		if (JS_ValueToObject(cx, argv[0], &session_obj)) {
			if (!(jss = JS_GetPrivate(cx, session_obj))) {
				switch_console_printf(SWITCH_CHANNEL_CONSOLE, "Cannot Find Session [1]\n");
				return JS_FALSE;
			}
		} else {
			switch_console_printf(SWITCH_CHANNEL_CONSOLE, "Cannot Find Session [2]\n");
			return JS_FALSE;
		}
	} else {
		switch_console_printf(SWITCH_CHANNEL_CONSOLE, "Missing Session Arg\n");
		return JS_FALSE;
	}
	if (argc > 1) {
		timer_name = JS_GetStringBytes(JS_ValueToString(cx, argv[1]));
	}

	if (argc > 2) {
		if (!JS_ValueToInt32(cx, argv[2], &memory)) {
			switch_console_printf(SWITCH_CHANNEL_CONSOLE, "Cannot Convert to INT\n");
			return JS_FALSE;
		}
	} 
	switch_core_new_memory_pool(&pool);

	if (!(tto = switch_core_alloc(pool, sizeof(*tto)))) {
		switch_console_printf(SWITCH_CHANNEL_CONSOLE, "Memory Error\n");
		return JS_FALSE;
	}

	read_codec = switch_core_session_get_read_codec(jss->session);

	if (switch_core_codec_init(&tto->codec,
							   "L16",
							   read_codec->implementation->samples_per_second,
							   read_codec->implementation->microseconds_per_frame / 1000,
							   read_codec->implementation->number_of_channels,
							   SWITCH_CODEC_FLAG_ENCODE | SWITCH_CODEC_FLAG_DECODE,
							   NULL, pool) == SWITCH_STATUS_SUCCESS) {
		switch_console_printf(SWITCH_CHANNEL_CONSOLE, "Raw Codec Activated\n");
	} else {
		switch_console_printf(SWITCH_CHANNEL_CONSOLE, "Raw Codec Activation Failed\n");
		return JS_FALSE;
	}

	if (timer_name) {
		unsigned int ms = read_codec->implementation->microseconds_per_frame / 1000;
		if (switch_core_timer_init(&tto->timer_base,
								   timer_name,
								   ms,
								   (read_codec->implementation->samples_per_second / 50) * read_codec->implementation->number_of_channels,
								   pool) == SWITCH_STATUS_SUCCESS) {
			tto->timer = &tto->timer_base;
			switch_console_printf(SWITCH_CHANNEL_CONSOLE, "Timer INIT Success %d\n", ms);
		} else {
			switch_console_printf(SWITCH_CHANNEL_CONSOLE, "Timer INIT Failed\n");
		}
	}

	switch_buffer_create(pool, &tto->audio_buffer, SWITCH_RECCOMMENDED_BUFFER_SIZE);
	tto->pool = pool;
	tto->obj = obj;
	tto->cx = cx;
	tto->session = jss->session;
	teletone_init_session(&tto->ts, memory, teletone_handler, tto);
	JS_SetPrivate(cx, obj, tto);

	return JS_TRUE;
}

static void teletone_destroy(JSContext *cx, JSObject *obj)
{
	struct teletone_obj *tto = JS_GetPrivate(cx, obj);
	switch_memory_pool *pool;
	if (tto) {
		pool = tto->pool;
		if (tto->timer) {
			switch_core_timer_destroy(tto->timer);
		}
		teletone_destroy_session(&tto->ts);
		switch_core_destroy_memory_pool(&pool);
		switch_core_codec_destroy(&tto->codec);
	}
}

static JSBool teletone_add_tone(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	struct teletone_obj *tto = JS_GetPrivate(cx, obj);
	if (argc > 2) { 
		int x;
		char *fval;
		char *map_str;
		map_str = JS_GetStringBytes(JS_ValueToString(cx, argv[0]));

		for(x = 1; x < TELETONE_MAX_TONES; x++) {
			fval = JS_GetStringBytes(JS_ValueToString(cx, argv[x]));
			tto->ts.TONES[(int)*map_str].freqs[x-1] = strtod(fval, NULL);
		}
		return JS_TRUE;
	}
	
	return JS_FALSE;
}

static JSBool teletone_on_dtmf(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	struct teletone_obj *tto = JS_GetPrivate(cx, obj);
	if (argc > 0) {
		char *func = JS_GetStringBytes(JS_ValueToString(cx, argv[0]));
		switch_copy_string(tto->code_buffer, func, sizeof(tto->code_buffer));
		switch_set_flag(tto, TTF_DTMF);
	}
	return JS_TRUE;
}

static JSBool teletone_generate(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	struct teletone_obj *tto = JS_GetPrivate(cx, obj);
	int32 loops = 0;

	if (argc > 0) {
		char *script;
		switch_core_session *session;
		switch_frame write_frame = {0};
		unsigned char *fdata[1024];
		switch_frame *read_frame;
		int stream_id;
		switch_core_thread_session thread_session;
		switch_channel *channel;

		if (argc > 1) {
			if (!JS_ValueToInt32(cx, argv[1], &loops)) {
				switch_console_printf(SWITCH_CHANNEL_CONSOLE, "Cannot Convert to INT\n");
				return JS_FALSE;
			}
			loops--;
			if (!tto->loop_buffer) {
				switch_buffer_create(tto->pool, &tto->loop_buffer, SWITCH_RECCOMMENDED_BUFFER_SIZE);
			}
		} 

		if (tto->audio_buffer) {
			switch_buffer_zero(tto->audio_buffer);
		}
		if (tto->loop_buffer) {
			switch_buffer_zero(tto->loop_buffer);
		}
		tto->ts.debug = 1;
		tto->ts.debug_stream = switch_core_get_console();

        script = JS_GetStringBytes(JS_ValueToString(cx, argv[0]));
		teletone_run(&tto->ts, script);

		session = tto->session;
		write_frame.codec = &tto->codec;
		write_frame.data = fdata;

		channel = switch_core_session_get_channel(session);
		
		if (tto->timer) {
			for (stream_id = 0; stream_id < switch_core_session_get_stream_count(session); stream_id++) {
				switch_core_service_session(session, &thread_session, stream_id);
			}
		}

		for(;;) {
			if (switch_test_flag(tto, TTF_DTMF)) {
				char dtmf[128];
				char code[512];
				char *ret;
				jsval tt_rval;
				if (switch_channel_has_dtmf(channel)) {
					switch_channel_dequeue_dtmf(channel, dtmf, sizeof(dtmf));
					snprintf(code, sizeof(code), "~%s(\"%s\")", tto->code_buffer, dtmf);
					eval_some_js(code, cx, obj, &tt_rval);
					ret = JS_GetStringBytes(JS_ValueToString(cx, tt_rval));
					if (strcmp(ret, "true") && strcmp(ret, "undefined")) {
						switch_copy_string(tto->ret_val, ret, sizeof(tto->ret_val));
						*rval = STRING_TO_JSVAL (JS_NewStringCopyZ(cx, tto->ret_val));
						return JS_TRUE;
					}
				}
			}
			
			if (tto->timer) {
				int x;

				if ((x = switch_core_timer_next(tto->timer)) < 0) {
					break;
				}

			} else {
				if (switch_core_session_read_frame(session, &read_frame, -1, 0) != SWITCH_STATUS_SUCCESS) {
					break;
				}
			}
			if ((write_frame.datalen = switch_buffer_read(tto->audio_buffer, fdata, write_frame.codec->implementation->bytes_per_frame)) <= 0) {
				if (loops > 0) { 
					switch_buffer *tmp;

					/* Switcharoo*/
					tmp = tto->audio_buffer;
					tto->audio_buffer = tto->loop_buffer;
					tto->loop_buffer = tmp;
					loops--;
					/* try again */
					if ((write_frame.datalen = switch_buffer_read(tto->audio_buffer, fdata, write_frame.codec->implementation->bytes_per_frame)) <= 0) {
						break;
					}
				} else {
					break;
				}
			}

			write_frame.samples = write_frame.datalen / 2;
			for (stream_id = 0; stream_id < switch_core_session_get_stream_count(session); stream_id++) {
				if (switch_core_session_write_frame(session, &write_frame, -1, stream_id) != SWITCH_STATUS_SUCCESS) {
					switch_console_printf(SWITCH_CHANNEL_CONSOLE, "Bad Write\n");
					break;
				}
			}
			if (tto->loop_buffer && loops) {
				switch_buffer_write(tto->loop_buffer, write_frame.data, write_frame.datalen);
			}
		}

		if (tto->timer) {
			switch_core_thread_session_end(&thread_session);
		}
		return JS_TRUE;
	}
	
	return JS_FALSE;
}

enum teletone_tinyid {
    TELETONE_NAME
};

static JSFunctionSpec teletone_methods[] = {
    {"generate", teletone_generate, 1},
    {"onDTMF", teletone_on_dtmf, 1},
    {"addTone", teletone_add_tone, 10},  
	{0}
};


static JSPropertySpec teletone_props[] = {
    {"name", TELETONE_NAME, JSPROP_READONLY|JSPROP_PERMANENT}, 
    {0}
};


static JSBool teletone_getProperty(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
	JSBool res = JS_TRUE;

    return res;
}

JSClass teletone_class = {
    "TeleTone", JSCLASS_HAS_PRIVATE, 
	JS_PropertyStub,  JS_PropertyStub,  teletone_getProperty,  JS_PropertyStub, 
    JS_EnumerateStub, JS_ResolveStub,   JS_ConvertStub,   teletone_destroy, NULL, NULL, NULL,
	teletone_construct
};


/* Built-In*/
/*********************************************************************************/
static JSBool js_log(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	char *msg;

	if((msg = JS_GetStringBytes(JS_ValueToString(cx, argv[0])))) {
		switch_console_printf(SWITCH_CHANNEL_CONSOLE, "JS_LOG: %s", msg);
		return JS_TRUE;
	} 

	return JS_FALSE;
}

static JSBool js_include(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	char *code;
	if ( argc > 0 && (code = JS_GetStringBytes(JS_ValueToString(cx, argv[0])))) {
		eval_some_js(code, cx, obj, rval);
		return JS_TRUE;
	}
	switch_console_printf(SWITCH_CHANNEL_CONSOLE, "Invalid Arguements\n");
	return JS_FALSE;
}

#define B64BUFFLEN 1024
static const char c64[65] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

static int write_buf(int fd, char *buf) {

	int len = (int)strlen(buf);
	if (fd && write(fd, buf, len) != len) {
		close(fd);
		return 0;
	}

	return 1;
}

static JSBool js_email(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	char *to = NULL, *from = NULL, *headers, *body = NULL, *file = NULL;
	char *bound = "XXXX_boundary_XXXX";
	char filename[80], buf[B64BUFFLEN];
	int fd = 0, ifd = 0;
	int x=0, y=0, bytes=0, ilen=0;
	unsigned int b=0, l=0;
	unsigned char in[B64BUFFLEN];
	unsigned char out[B64BUFFLEN+512];
	char *path = NULL;

	
	if ( 
		 argc > 3 && 
		 (from = JS_GetStringBytes(JS_ValueToString(cx, argv[0]))) &&
		 (to = JS_GetStringBytes(JS_ValueToString(cx, argv[1]))) &&
		 (headers = JS_GetStringBytes(JS_ValueToString(cx, argv[2]))) &&
		 (body = JS_GetStringBytes(JS_ValueToString(cx, argv[3]))) 
		 ) {
		if ( argc > 4)
			file = JS_GetStringBytes(JS_ValueToString(cx, argv[4]));

		snprintf(filename, 80, "%smail.%ld", SWITCH_GLOBAL_dirs.temp_dir, switch_time_now());

		if ((fd = open(filename, O_WRONLY | O_CREAT | O_TRUNC, 0644))) {
			if (file) {
				path = file;
				if ((ifd = open(path, O_RDONLY)) < 1) {
					return JS_FALSE;
				}

				snprintf(buf, B64BUFFLEN, "MIME-Version: 1.0\nContent-Type: multipart/mixed; boundary=\"%s\"\n", bound);
				if (!write_buf(fd, buf)) {
					return JS_FALSE;
				}
			}
			
			if (!write_buf(fd, headers))
				return JS_FALSE;

			if (!write_buf(fd, "\n\n"))
				return JS_FALSE;
			
			if (file) {
				snprintf(buf, B64BUFFLEN, "--%s\nContent-Type: text/plain\n\n", bound);
				if (!write_buf(fd, buf))
					return JS_FALSE;
			}
			
			if (!write_buf(fd, body))
				return JS_FALSE;
			
			if (file) {
				snprintf(buf, B64BUFFLEN, "\n\n--%s\nContent-Type: application/octet-stream\nContent-Transfer-Encoding: base64\nContent-Description: Sound attachment.\nContent-Disposition: attachment; filename=\"%s\"\n\n", bound, file);
				if (!write_buf(fd, buf))
					return JS_FALSE;
				
				while((ilen=read(ifd, in, B64BUFFLEN))) {
					for (x=0;x<ilen;x++) {
						b = (b<<8) + in[x];
						l += 8;
						while (l >= 6) {
							out[bytes++] = c64[(b>>(l-=6))%64];
							if (++y!=72)
								continue;
							out[bytes++] = '\n';
							y=0;
						}
					}
					if (write(fd, &out, bytes) != bytes) { 
						return -1;
					} else 
						bytes=0;
					
				}
				
				if (l > 0)
					out[bytes++] = c64[((b%16)<<(6-l))%64];
				if (l != 0) while (l < 6)
					out[bytes++] = '=', l += 2;
	
				if (write(fd, &out, bytes) != bytes) { 
					return -1;
				}

			}
			

			
			if (file) {
				snprintf(buf, B64BUFFLEN, "\n\n--%s--\n.\n", bound);
				if (!write_buf(fd, buf))
					return JS_FALSE;
			}
		}

		if (fd)
			close(fd);
		if (ifd)
			close(ifd);

		snprintf(buf, B64BUFFLEN, "/bin/cat %s | /usr/sbin/sendmail -tf \"%s\" %s", filename, from, to);
		system(buf);
		unlink(filename);


		if (file) {
			switch_console_printf(SWITCH_CHANNEL_CONSOLE, "Emailed file [%s] to [%s]\n", filename, to);
		} else {
			switch_console_printf(SWITCH_CHANNEL_CONSOLE, "Emailed data to [%s]\n", to);
		}
		return JS_TRUE;
	}
	

	return JS_FALSE;
}

static JSFunctionSpec fs_functions[] = {
	{"console_log", js_log, 2}, 
	{"include", js_include, 1}, 
	{"email", js_email, 2}, 
#ifdef HAVE_CURL
	{"fetchURL", js_fetchurl, 1}, 
#endif 
	{0}
};


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
			snprintf(path, sizeof(path), "%s/%s", SWITCH_GLOBAL_dirs.script_dir, code);
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
	

			JS_InitClass(cx,
						 javascript_global_object,
						 NULL,
						 &teletone_class,
						 teletone_construct,
						 3,
						 teletone_props,
						 teletone_methods,
						 teletone_props,
						 teletone_methods
						 );



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
