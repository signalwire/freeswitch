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
#define JS_BUFFER_SIZE 131072
#ifdef __ICC
#pragma warning (disable:310 193 1418)
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
static void session_destroy(JSContext *cx, JSObject *obj);
static JSBool session_construct(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval);

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
	JS_PropertyStub,  JS_PropertyStub,	JS_PropertyStub,  JS_PropertyStub, 
	JS_EnumerateStub, JS_ResolveStub,	JS_ConvertStub,	  JS_FinalizeStub
};

typedef enum {
	TTF_DTMF = (1 << 0)
} teletone_flag_t;

typedef enum {
	S_HUP = (1 << 0)
} session_flag_t;

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

struct fileio_obj {
	char *path;
	unsigned int flags;
	switch_file_t *fd;
	switch_memory_pool *pool;
	char *buf;
	switch_size_t buflen;
	int32 bufsize;
};

struct db_obj {
	switch_memory_pool *pool;
	switch_core_db *db;
	switch_core_db_stmt *stmt;
	char *dbname;
	char code_buffer[2048];
	JSContext *cx;
	JSObject *obj;
};


static void js_error(JSContext *cx, const char *message, JSErrorReport *report)
{
	if (message) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "%s\n", message);
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
	
	if (!jss) {
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
	
	if (!jss) {
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
	
	if (!jss) {
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

	return (switch_channel_ready(channel)) ? JS_TRUE : JS_FALSE;
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

	return (switch_channel_ready(channel)) ? JS_TRUE : JS_FALSE;
}

static JSBool session_speak(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	struct js_session *jss = JS_GetPrivate(cx, obj);
	switch_channel *channel;
	char *tts_name = NULL;
	char *voice_name = NULL;
	char *text = NULL;
	char *dtmf_callback = NULL;
	char *timer_name = NULL;
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

	if (argc > 4) {
		timer_name = JS_GetStringBytes(JS_ValueToString(cx, argv[4]));
	}

	if (!tts_name && text) {
		return JS_FALSE;
	}

	codec = switch_core_session_get_read_codec(jss->session);
	switch_ivr_speak_text(jss->session,
						  tts_name,
						  voice_name && strlen(voice_name) ? voice_name : NULL, 
						  timer_name,
						  codec->implementation->samples_per_second,
						  dtmf_func,
						  text,
						  bp,
						  len);

	*rval = STRING_TO_JSVAL (JS_NewStringCopyZ(cx, cb_state.ret_buffer));

	return (switch_channel_ready(channel)) ? JS_TRUE : JS_FALSE;
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

static JSBool session_ready(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	struct js_session *jss = JS_GetPrivate(cx, obj);
	switch_channel *channel;

	channel = switch_core_session_get_channel(jss->session);
	assert(channel != NULL);
	*rval = BOOLEAN_TO_JSVAL( switch_channel_ready(channel) ? JS_TRUE : JS_FALSE );

	return JS_TRUE;
}

static JSBool session_wait_for_answer(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	struct js_session *jss = JS_GetPrivate(cx, obj);
	switch_channel *channel;
	switch_time_t started;
	unsigned int elapsed;
	int32 timeout = 60;
	channel = switch_core_session_get_channel(jss->session);
	assert(channel != NULL);
	started = switch_time_now();

	if (argc > 0) {
		JS_ValueToInt32(cx, argv[0], &timeout);
	}

	for(;;) {
		elapsed = (unsigned int)((switch_time_now() - started) / 1000);
		if ((int32)elapsed > timeout || switch_channel_test_flag(channel, CF_ANSWERED) ||
			switch_channel_test_flag(channel, CF_EARLY_MEDIA)) {
			break;
		}
		
		switch_yield(1000);
	}

	return JS_TRUE;
}

static JSBool session_execute(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	JSBool retval = JS_FALSE;
	if (argc > 1) {
		const switch_application_interface *application_interface;
		char *app_name = JS_GetStringBytes(JS_ValueToString(cx, argv[0]));
		char *app_arg = JS_GetStringBytes(JS_ValueToString(cx, argv[1]));
		struct js_session *jss = JS_GetPrivate(cx, obj);

		if ((application_interface = switch_loadable_module_get_application_interface(app_name))) {
			if (application_interface->application_function) {
				application_interface->application_function(jss->session, app_arg);
				retval = JS_TRUE;
			}
		} 
	}
	
	*rval = BOOLEAN_TO_JSVAL( retval );
	return JS_TRUE;
}


static JSBool session_hangup(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	struct js_session *jss = JS_GetPrivate(cx, obj);
	switch_channel *channel;

	channel = switch_core_session_get_channel(jss->session);
	assert(channel != NULL);

	switch_channel_hangup(channel, SWITCH_CAUSE_NORMAL_CLEARING);
	switch_core_session_kill_channel(jss->session, SWITCH_SIG_KILL);
	return JS_TRUE;
}

#ifdef HAVE_CURL

struct config_data {
	JSContext *cx;
	JSObject *obj;
	char *name;
	int fd;
};

static size_t hash_callback(void *ptr, size_t size, size_t nmemb, void *data)
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



static size_t file_callback(void *ptr, size_t size, size_t nmemb, void *data)
{
	register unsigned int realsize = (unsigned int)(size * nmemb);
	struct config_data *config_data = data;

	write(config_data->fd, ptr, realsize);
	return realsize;
}


static JSBool js_fetchurl_hash(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	char *url = NULL, *name = NULL;
	CURL *curl_handle = NULL;
	struct config_data config_data;
	
	if ( argc > 1 ) {
		url = JS_GetStringBytes(JS_ValueToString(cx, argv[0]));
		name = JS_GetStringBytes(JS_ValueToString(cx, argv[1]));

		curl_global_init(CURL_GLOBAL_ALL);
		curl_handle = curl_easy_init();
		if (!strncasecmp(url, "https", 5)) {
			curl_easy_setopt(curl_handle, CURLOPT_SSL_VERIFYPEER, 0);
			curl_easy_setopt(curl_handle, CURLOPT_SSL_VERIFYHOST, 0);
		}
		config_data.cx = cx;
		config_data.obj = obj;
		if (name) {
			config_data.name = name;
		}
		curl_easy_setopt(curl_handle, CURLOPT_URL, url);
		curl_easy_setopt(curl_handle, CURLOPT_WRITEFUNCTION, hash_callback);
		curl_easy_setopt(curl_handle, CURLOPT_WRITEDATA, (void *)&config_data);
		curl_easy_setopt(curl_handle, CURLOPT_USERAGENT, "freeswitch-js/1.0");
		curl_easy_perform(curl_handle);
		curl_easy_cleanup(curl_handle);
	} else {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Error!\n");
		return JS_FALSE;
	}

	return JS_TRUE;
}



static JSBool js_fetchurl_file(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	char *url = NULL, *filename = NULL;
	CURL *curl_handle = NULL;
	struct config_data config_data;
	
	if ( argc > 1 ) {
		url = JS_GetStringBytes(JS_ValueToString(cx, argv[0]));
		filename = JS_GetStringBytes(JS_ValueToString(cx, argv[1]));

		curl_global_init(CURL_GLOBAL_ALL);
		curl_handle = curl_easy_init();
		if (!strncasecmp(url, "https", 5)) {
			curl_easy_setopt(curl_handle, CURLOPT_SSL_VERIFYPEER, 0);
			curl_easy_setopt(curl_handle, CURLOPT_SSL_VERIFYHOST, 0);
		}
		config_data.cx = cx;
		config_data.obj = obj;

		config_data.name = filename;
		if ((config_data.fd = open(filename, O_CREAT | O_RDWR | O_TRUNC)) > -1) {
			curl_easy_setopt(curl_handle, CURLOPT_URL, url);
			curl_easy_setopt(curl_handle, CURLOPT_WRITEFUNCTION, file_callback);
			curl_easy_setopt(curl_handle, CURLOPT_WRITEDATA, (void *)&config_data);
			curl_easy_setopt(curl_handle, CURLOPT_USERAGENT, "freeswitch-js/1.0");
			curl_easy_perform(curl_handle);
			curl_easy_cleanup(curl_handle);
			close(config_data.fd);
		} else {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Error!\n");
		}
	} else {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Error!\n");
	}

	return JS_TRUE;
}
#endif

/* Session Object */
/*********************************************************************************/
enum session_tinyid {
	SESSION_NAME, SESSION_STATE,
	PROFILE_DIALPLAN, PROFILE_CID_NAME, PROFILE_CID_NUM, PROFILE_IP, PROFILE_ANI, PROFILE_ANI2, PROFILE_DEST
};

static JSFunctionSpec session_methods[] = {
	{"streamFile", session_streamfile, 1}, 
	{"recordFile", session_recordfile, 1}, 
	{"speak", session_speak, 1}, 
	{"getDigits", session_get_digits, 1},
	{"answer", session_answer, 0}, 
	{"ready", session_ready, 0}, 
	{"waitForAnswer", session_wait_for_answer, 0}, 
	{"hangup", session_hangup, 0}, 
	{"execute", session_execute, 0}, 
	{0}
};


static JSPropertySpec session_props[] = {
	{"name", SESSION_NAME, JSPROP_READONLY|JSPROP_PERMANENT}, 
	{"state", SESSION_STATE, JSPROP_READONLY|JSPROP_PERMANENT}, 
	{"dialplan", PROFILE_DIALPLAN, JSPROP_READONLY|JSPROP_PERMANENT},
	{"caller_id_name", PROFILE_CID_NAME, JSPROP_READONLY|JSPROP_PERMANENT}, 
	{"caller_id_num", PROFILE_CID_NUM, JSPROP_READONLY|JSPROP_PERMANENT}, 
	{"network_addr", PROFILE_IP, JSPROP_READONLY|JSPROP_PERMANENT}, 
	{"ani", PROFILE_ANI, JSPROP_READONLY|JSPROP_PERMANENT}, 
	{"ani2", PROFILE_ANI2, JSPROP_READONLY|JSPROP_PERMANENT}, 
	{"destination", PROFILE_DEST, JSPROP_READONLY|JSPROP_PERMANENT}, 
	{0}
};

static JSBool session_getProperty(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
	struct js_session *jss = JS_GetPrivate(cx, obj);
	int param = 0;
	JSBool res = JS_TRUE;
	switch_channel *channel;
	switch_caller_profile *caller_profile;
	char *name;

	channel = switch_core_session_get_channel(jss->session);
	assert(channel != NULL);

	caller_profile = switch_channel_get_caller_profile(channel);

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
		*vp = STRING_TO_JSVAL(JS_NewStringCopyZ(cx, switch_channel_state_name(switch_channel_get_state(channel))));
		break;
	case PROFILE_DIALPLAN:
		if (caller_profile) {
			*vp = STRING_TO_JSVAL(JS_NewStringCopyZ(cx, caller_profile->dialplan));
		}
		break;
	case PROFILE_CID_NAME:
		if (caller_profile) {
			*vp = STRING_TO_JSVAL(JS_NewStringCopyZ(cx, caller_profile->caller_id_name));
		}
		break;
	case PROFILE_CID_NUM:
		if (caller_profile) {
			*vp = STRING_TO_JSVAL(JS_NewStringCopyZ(cx, caller_profile->caller_id_number));
		}
		break;
	case PROFILE_IP:
		if (caller_profile) {
			*vp = STRING_TO_JSVAL(JS_NewStringCopyZ(cx, caller_profile->network_addr));
		}
		break;
	case PROFILE_ANI:
		if (caller_profile) {
			*vp = STRING_TO_JSVAL(JS_NewStringCopyZ(cx, caller_profile->ani));
		}
		break;
	case PROFILE_ANI2:
		if (caller_profile) {
			*vp = STRING_TO_JSVAL(JS_NewStringCopyZ(cx, caller_profile->ani2));
		}
		break;
	case PROFILE_DEST:
		if (caller_profile) {
			*vp = STRING_TO_JSVAL(JS_NewStringCopyZ(cx, caller_profile->destination_number));
		}
		break;
	default:
		res = JS_FALSE;
		break;

	}

	return res;
}

JSClass session_class = {
	"Session", JSCLASS_HAS_PRIVATE, 
	JS_PropertyStub,  JS_PropertyStub,	session_getProperty,  JS_PropertyStub, 
	JS_EnumerateStub, JS_ResolveStub,	JS_ConvertStub,	  session_destroy, NULL, NULL, NULL,
	session_construct
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

/* Session Object */
/*********************************************************************************/
static JSBool session_construct(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	switch_memory_pool *pool = NULL;
	if (argc > 2) {
		struct js_session *jss = NULL;
		JSObject *session_obj;
		switch_core_session *session = NULL, *peer_session = NULL;
		switch_caller_profile *caller_profile = NULL;
		char *channel_type = NULL;
		char *dest = NULL;
		char *dialplan = NULL;
		char *cid_name = "";
		char *cid_num = "";
		char *network_addr = "";
		char *ani = "";
		char *ani2 = "";
		char *rdnis = "";
		char *context = "";

		*rval = BOOLEAN_TO_JSVAL( JS_FALSE );

		if (JS_ValueToObject(cx, argv[0], &session_obj)) {
			struct js_session *old_jss = NULL;
			if ((old_jss = JS_GetPrivate(cx, session_obj))) {
				session = old_jss->session;
			}
		}

		channel_type = JS_GetStringBytes(JS_ValueToString(cx, argv[1]));
		dest = JS_GetStringBytes(JS_ValueToString(cx, argv[2]));

		if (argc > 3) {
			dialplan = JS_GetStringBytes(JS_ValueToString(cx, argv[3]));
		}
		if (argc > 4) {
			context = JS_GetStringBytes(JS_ValueToString(cx, argv[4]));
		}
		if (argc > 5) {
			cid_name = JS_GetStringBytes(JS_ValueToString(cx, argv[5]));
		}
		if (argc > 6) {
			cid_num = JS_GetStringBytes(JS_ValueToString(cx, argv[6]));
		}
		if (argc > 7) {
			network_addr = JS_GetStringBytes(JS_ValueToString(cx, argv[7]));
		}
		if (argc > 8) {
			ani = JS_GetStringBytes(JS_ValueToString(cx, argv[8]));
		}
		if (argc > 9) {
			ani2 = JS_GetStringBytes(JS_ValueToString(cx, argv[9]));
		}
		if (argc > 10) {
			rdnis = JS_GetStringBytes(JS_ValueToString(cx, argv[10]));
		}
		
		
		if (switch_core_new_memory_pool(&pool) != SWITCH_STATUS_SUCCESS) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "OH OH no pool\n");
			return JS_FALSE;
		}

		caller_profile = switch_caller_profile_new(pool, dialplan, cid_name, cid_num, network_addr, ani, ani2, rdnis, (char *)modname, context, dest);
		if (switch_core_session_outgoing_channel(session, channel_type, caller_profile, &peer_session, pool) == SWITCH_STATUS_SUCCESS) {
			jss = switch_core_session_alloc(peer_session, sizeof(*jss));
			jss->session = peer_session;
			jss->flags = 0;
			jss->cx = cx;
			jss->obj = obj;
			JS_SetPrivate(cx, obj, jss);
			switch_core_session_thread_launch(peer_session);
			switch_set_flag(jss, S_HUP);
			return JS_TRUE;
		} else {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Cannot Create Channel\n");			
		}
	} else {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "Missing Args\n");
	}

	return JS_TRUE;
}

static void session_destroy(JSContext *cx, JSObject *obj)
{
	struct js_session *jss;
	
	if (cx && obj) {
		if ((jss = JS_GetPrivate(cx, obj))) {
			if (switch_test_flag(jss, S_HUP)) {
				switch_channel *channel;

				if (jss->session) {
					channel = switch_core_session_get_channel(jss->session);
					switch_channel_hangup(channel, SWITCH_CAUSE_NORMAL_CLEARING);
				}
			}
		}
	}
		
	return;
}

/* FileIO Object */
/*********************************************************************************/
static JSBool fileio_construct(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	switch_memory_pool *pool;
	switch_file_t *fd;
	char *path, *flags_str;
	unsigned int flags = 0;
	struct fileio_obj *fio;

	if (argc > 1) {
		path = JS_GetStringBytes(JS_ValueToString(cx, argv[0]));
		flags_str = JS_GetStringBytes(JS_ValueToString(cx, argv[1]));
		
		if (strchr(flags_str, 'r')) {
			flags |= SWITCH_FOPEN_READ;
		}
		if (strchr(flags_str, 'w')) {
			flags |= SWITCH_FOPEN_WRITE;
		}
		if (strchr(flags_str, 'c')) {
			flags |= SWITCH_FOPEN_CREATE;
		}
		if (strchr(flags_str, 'a')) {
			flags |= SWITCH_FOPEN_APPEND;
		}
		if (strchr(flags_str, 't')) {
			flags |= SWITCH_FOPEN_TRUNCATE;
		}
		if (strchr(flags_str, 'b')) {
			flags |= SWITCH_FOPEN_BINARY;
		}
		switch_core_new_memory_pool(&pool);
		if (switch_file_open(&fd, path, flags, SWITCH_FPROT_UREAD|SWITCH_FPROT_UWRITE, pool) != SWITCH_STATUS_SUCCESS) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Cannot Open File!\n");
			switch_core_destroy_memory_pool(&pool);
			return JS_FALSE;
		}
		fio = switch_core_alloc(pool, sizeof(*fio));
		fio->fd = fd;
		fio->pool = pool;
		fio->path = switch_core_strdup(pool, path);
		fio->flags = flags;
		JS_SetPrivate(cx, obj, fio);
		return JS_TRUE;
	}

	return JS_FALSE;
}
static void fileio_destroy(JSContext *cx, JSObject *obj)
{
	struct fileio_obj *fio = JS_GetPrivate(cx, obj);

	if (fio) {
		switch_memory_pool *pool = fio->pool;
		switch_core_destroy_memory_pool(&pool);
		pool = NULL;
	}
}

static JSBool fileio_read(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	struct fileio_obj *fio = JS_GetPrivate(cx, obj);
	int32 bytes = 0;
	switch_size_t read = 0;

	*rval = BOOLEAN_TO_JSVAL( JS_FALSE );

	if (!(fio->flags & SWITCH_FOPEN_READ)) {
		return JS_TRUE;
	}
	
	if (argc > 0) {
		JS_ValueToInt32(cx, argv[0], &bytes);
	}

	if (bytes) {
		if (!fio->buf || fio->bufsize < bytes) {
			fio->buf = switch_core_alloc(fio->pool, bytes);
			fio->bufsize = bytes;
		}
		read = bytes;
		switch_file_read(fio->fd, fio->buf, &read);
		fio->buflen = read;
		*rval = BOOLEAN_TO_JSVAL( (read > 0) ? JS_TRUE : JS_FALSE );
	} 

	return JS_TRUE;
}

static JSBool fileio_data(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	struct fileio_obj *fio = JS_GetPrivate(cx, obj);
	*rval = STRING_TO_JSVAL (JS_NewStringCopyZ(cx, fio->buf));
	return JS_TRUE;
}

static JSBool fileio_write(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	struct fileio_obj *fio = JS_GetPrivate(cx, obj);
	switch_size_t wrote = 0;
	char *data = NULL;

	if (!(fio->flags & SWITCH_FOPEN_WRITE)) {
		*rval = BOOLEAN_TO_JSVAL( JS_FALSE );
		return JS_TRUE;
	}
	
	if (argc > 0) {
		data = JS_GetStringBytes(JS_ValueToString(cx, argv[0]));
	}

	if (data) {
		wrote = 0;
		*rval = BOOLEAN_TO_JSVAL( (switch_file_write(fio->fd, data, &wrote) == SWITCH_STATUS_SUCCESS) ? JS_TRUE : JS_FALSE);
	}

	*rval = BOOLEAN_TO_JSVAL( JS_FALSE );
	return JS_TRUE;
}

enum fileio_tinyid {
	FILEIO_PATH
};

static JSFunctionSpec fileio_methods[] = {
	{"read", fileio_read, 1},
	{"write", fileio_write, 1},
	{"data", fileio_data, 0},
	{0}
};


static JSPropertySpec fileio_props[] = {
	{"path", FILEIO_PATH, JSPROP_READONLY|JSPROP_PERMANENT}, 
	{0}
};


static JSBool fileio_getProperty(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
	JSBool res = JS_TRUE;
	struct fileio_obj *fio = JS_GetPrivate(cx, obj);
	char *name;
	int param = 0;
	
	name = JS_GetStringBytes(JS_ValueToString(cx, id));
    /* numbers are our props anything else is a method */
    if (name[0] >= 48 && name[0] <= 57) {
        param = atoi(name);
    } else {
        return JS_TRUE;
    }
	
	switch(param) {
	case FILEIO_PATH:
		*vp = STRING_TO_JSVAL(JS_NewStringCopyZ(cx, fio->path));
		break;
	}

	return res;
}

JSClass fileio_class = {
	"FileIO", JSCLASS_HAS_PRIVATE, 
	JS_PropertyStub,  JS_PropertyStub,	fileio_getProperty,  JS_PropertyStub, 
	JS_EnumerateStub, JS_ResolveStub,	JS_ConvertStub,	  fileio_destroy, NULL, NULL, NULL,
	fileio_construct
};


/* DB Object */
/*********************************************************************************/
static JSBool db_construct(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	switch_memory_pool *pool;
	switch_core_db *db;
	struct db_obj *dbo;

	if (argc > 0) {
		char *dbname = JS_GetStringBytes(JS_ValueToString(cx, argv[0]));
		switch_core_new_memory_pool(&pool);
		if (! (db = switch_core_db_open_file(dbname))) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Cannot Open DB!\n");
			switch_core_destroy_memory_pool(&pool);
			return JS_FALSE;
		}
		dbo = switch_core_alloc(pool, sizeof(*dbo));
		dbo->pool = pool;
		dbo->dbname = switch_core_strdup(pool, dbname);
		dbo->cx = cx;
		dbo->obj = obj;
		dbo->db = db;
		JS_SetPrivate(cx, obj, dbo);
		return JS_TRUE;
	}

	return JS_FALSE;
}

static void db_destroy(JSContext *cx, JSObject *obj)
{
	struct db_obj *dbo = JS_GetPrivate(cx, obj);
	
	if (dbo) {
		switch_memory_pool *pool = dbo->pool;
		if (dbo->stmt) {
			switch_core_db_finalize(dbo->stmt);
			dbo->stmt = NULL;
		}
		switch_core_db_close(dbo->db);
		switch_core_destroy_memory_pool(&pool);
        pool = NULL;
	}
}


static int db_callback(void *pArg, int argc, char **argv, char **columnNames)
{
	struct db_obj *dbo = pArg;
	char code[1024];
	jsval rval;
	int x = 0;

	snprintf(code, sizeof(code), "~var _Db_RoW_ = {}");
	eval_some_js(code, dbo->cx, dbo->obj, &rval);

	for(x=0; x < argc; x++) {
		snprintf(code, sizeof(code), "~_Db_RoW_[\"%s\"] = \"%s\"", columnNames[x], argv[x]);
		eval_some_js(code, dbo->cx, dbo->obj, &rval);
	}
	snprintf(code, sizeof(code), "~%s(_Db_RoW_)", dbo->code_buffer);
	eval_some_js(code, dbo->cx, dbo->obj, &rval);

	snprintf(code, sizeof(code), "~delete _Db_RoW_");
	eval_some_js(code, dbo->cx, dbo->obj, &rval);
	
	return 0;
}

static JSBool db_exec(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	struct db_obj *dbo = JS_GetPrivate(cx, obj);
	*rval = BOOLEAN_TO_JSVAL( JS_TRUE );	

	if (argc > 0) {		
		char *sql = JS_GetStringBytes(JS_ValueToString(cx, argv[0]));
		char *err = NULL;
		void *arg = NULL;
		switch_core_db_callback_func cb_func = NULL;

			
		if (argc > 1) {
			char *js_func = JS_GetStringBytes(JS_ValueToString(cx, argv[1]));
			switch_copy_string(dbo->code_buffer, js_func, sizeof(dbo->code_buffer));
			cb_func = db_callback;
			arg = dbo;
		}

		switch_core_db_exec(dbo->db, sql, cb_func, arg, &err);
		if (err) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Error %s\n", err);
			switch_core_db_free(err);
			*rval = BOOLEAN_TO_JSVAL( JS_FALSE );
		}
	}
	return JS_TRUE;
}

static JSBool db_next(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	struct db_obj *dbo = JS_GetPrivate(cx, obj);
	*rval = BOOLEAN_TO_JSVAL( JS_FALSE );
	
	if (dbo->stmt) {
		int result = switch_core_db_step(dbo->stmt);
		int running = 1;

		while (running < 5000) {
			if (result == SQLITE_ROW) {
				*rval = BOOLEAN_TO_JSVAL( JS_TRUE );	
				break;
			} else if (result == SQLITE_BUSY) {
				running++;
				continue;
			}
			switch_core_db_finalize(dbo->stmt);
			dbo->stmt = NULL;
			break;
		}
	}

	return JS_TRUE;
}

static JSBool db_fetch(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	struct db_obj *dbo = JS_GetPrivate(cx, obj);
	int colcount = switch_core_db_column_count(dbo->stmt);
	char code[1024];
	int x;

	snprintf(code, sizeof(code), "~var _dB_RoW_DaTa_ = {}");
	eval_some_js(code, dbo->cx, dbo->obj, rval);
	if (*rval == JS_FALSE) {
		return JS_TRUE; 
	}
	for (x = 0; x < colcount; x++) {
		snprintf(code, sizeof(code), "~_dB_RoW_DaTa_[\"%s\"] = \"%s\"", 
				 (char *) switch_core_db_column_name(dbo->stmt, x),
				 (char *) switch_core_db_column_text(dbo->stmt, x));

		eval_some_js(code, dbo->cx, dbo->obj, rval);
		if (*rval == JS_FALSE) {
			return JS_TRUE; 
		}
	}

	JS_GetProperty(cx, obj, "_dB_RoW_DaTa_", rval);

	return JS_TRUE;
}


static JSBool db_prepare(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	struct db_obj *dbo = JS_GetPrivate(cx, obj);

	*rval = BOOLEAN_TO_JSVAL( JS_FALSE );	

	if (dbo->stmt) {
		switch_core_db_finalize(dbo->stmt);
		dbo->stmt = NULL;
	}

	if (argc > 0) {		
		char *sql = JS_GetStringBytes(JS_ValueToString(cx, argv[0]));

		if(switch_core_db_prepare(dbo->db, sql, 0, &dbo->stmt, 0)) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Error %s\n", switch_core_db_errmsg(dbo->db));
		} else {
			*rval = BOOLEAN_TO_JSVAL( JS_TRUE );
		}
	}
	return JS_TRUE;
}

enum db_tinyid {
	DB_NAME
};

static JSFunctionSpec db_methods[] = {
	{"exec", db_exec, 1},
	{"next", db_next, 0},
	{"fetch", db_fetch, 1},
	{"prepare", db_prepare, 0},
	{0}
};


static JSPropertySpec db_props[] = {
	{"path", DB_NAME, JSPROP_READONLY|JSPROP_PERMANENT}, 
	{0}
};


static JSBool db_getProperty(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
	JSBool res = JS_TRUE;
	struct db_obj *dbo = JS_GetPrivate(cx, obj);
	char *name;
	int param = 0;
	
	name = JS_GetStringBytes(JS_ValueToString(cx, id));
    /* numbers are our props anything else is a method */
    if (name[0] >= 48 && name[0] <= 57) {
        param = atoi(name);
    } else {
        return JS_TRUE;
    }
	
	switch(param) {
	case DB_NAME:
		*vp = STRING_TO_JSVAL(JS_NewStringCopyZ(cx, dbo->dbname));
		break;
	}

	return res;
}

JSClass db_class = {
	"DB", JSCLASS_HAS_PRIVATE, 
	JS_PropertyStub,  JS_PropertyStub,	db_getProperty,  JS_PropertyStub, 
	JS_EnumerateStub, JS_ResolveStub,	JS_ConvertStub,	  db_destroy, NULL, NULL, NULL,
	db_construct
};

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
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Cannot Find Session [1]\n");
				return JS_FALSE;
			}
		} else {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Cannot Find Session [2]\n");
			return JS_FALSE;
		}
	} else {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Missing Session Arg\n");
		return JS_FALSE;
	}
	if (argc > 1) {
		timer_name = JS_GetStringBytes(JS_ValueToString(cx, argv[1]));
	}

	if (argc > 2) {
		if (!JS_ValueToInt32(cx, argv[2], &memory)) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Cannot Convert to INT\n");
			return JS_FALSE;
		}
	} 
	switch_core_new_memory_pool(&pool);

	if (!(tto = switch_core_alloc(pool, sizeof(*tto)))) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Memory Error\n");
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
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Raw Codec Activated\n");
	} else {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Raw Codec Activation Failed\n");
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
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Timer INIT Success %u\n", ms);
		} else {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Timer INIT Failed\n");
		}
	}

	switch_buffer_create(pool, &tto->audio_buffer, JS_BUFFER_SIZE);
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
		if (tto->timer) {
			switch_core_timer_destroy(tto->timer);
		}
		teletone_destroy_session(&tto->ts);
		switch_core_codec_destroy(&tto->codec);
		pool = tto->pool;
		tto->pool = NULL;
		if (pool) {
			switch_core_destroy_memory_pool(&pool);
		}
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
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Cannot Convert to INT\n");
				return JS_FALSE;
			}
			loops--;
			if (!tto->loop_buffer) {
				switch_buffer_create(tto->pool, &tto->loop_buffer, JS_BUFFER_SIZE);
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
				if (switch_core_timer_next(tto->timer)< 0) {
					break;
				}

			} else {
				if (switch_core_session_read_frame(session, &read_frame, -1, 0) != SWITCH_STATUS_SUCCESS) {
					break;
				}
			}
			if ((write_frame.datalen = (uint32_t)switch_buffer_read(tto->audio_buffer, fdata, write_frame.codec->implementation->bytes_per_frame)) <= 0) {
				if (loops > 0) { 
					switch_buffer *tmp;

					/* Switcharoo*/
					tmp = tto->audio_buffer;
					tto->audio_buffer = tto->loop_buffer;
					tto->loop_buffer = tmp;
					loops--;
					/* try again */
					if ((write_frame.datalen = (uint32_t)switch_buffer_read(tto->audio_buffer, fdata, write_frame.codec->implementation->bytes_per_frame)) <= 0) {
						break;
					}
				} else {
					continue;
				}
			}

			write_frame.samples = write_frame.datalen / 2;
			for (stream_id = 0; stream_id < switch_core_session_get_stream_count(session); stream_id++) {
				if (switch_core_session_write_frame(session, &write_frame, -1, stream_id) != SWITCH_STATUS_SUCCESS) {
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Bad Write\n");
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
	JS_PropertyStub,  JS_PropertyStub,	teletone_getProperty,  JS_PropertyStub, 
	JS_EnumerateStub, JS_ResolveStub,	JS_ConvertStub,	  teletone_destroy, NULL, NULL, NULL,
	teletone_construct
};


/* Built-In*/
/*********************************************************************************/
static JSBool js_log(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	char *msg;

	if ((msg = JS_GetStringBytes(JS_ValueToString(cx, argv[0])))) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "JS_LOG: %s", msg);
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
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Invalid Arguements\n");
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

static JSBool js_api_execute(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{

	if (argc > 1) {
		char *cmd = JS_GetStringBytes(JS_ValueToString(cx, argv[0]));
		char *arg = JS_GetStringBytes(JS_ValueToString(cx, argv[1]));
		char retbuf[2048] = "";
		switch_api_execute(cmd, arg, retbuf, sizeof(retbuf));
		*rval = STRING_TO_JSVAL (JS_NewStringCopyZ(cx, retbuf));
	} else {
		*rval = STRING_TO_JSVAL (JS_NewStringCopyZ(cx, ""));
	}

	return JS_TRUE;
}

static JSBool js_bridge(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	struct js_session *jss_a = NULL, *jss_b = NULL;
	JSObject *session_obj_a = NULL, *session_obj_b = NULL;
	int32 timelimit = 60;

	if (argc > 1) {
		if (JS_ValueToObject(cx, argv[0], &session_obj_a)) {
			if (!(jss_a = JS_GetPrivate(cx, session_obj_a))) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Cannot Find Session [1]\n");
				return JS_FALSE;
			}
		} 
		if (JS_ValueToObject(cx, argv[1], &session_obj_b)) {
			if (!(jss_b = JS_GetPrivate(cx, session_obj_b))) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Cannot Find Session [1]\n");
				return JS_FALSE;
			}
		} 
	}
	
	if (argc > 3) {
		if (!JS_ValueToInt32(cx, argv[3], &timelimit)) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Cannot Convert to INT\n");
			return JS_FALSE;
		}
	}
	if (!(jss_a && jss_b)) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Failure! %s %s\n", jss_a ? "y" : "n", jss_b ? "y" : "n");
		return JS_FALSE;
	}

	switch_ivr_multi_threaded_bridge(jss_a->session, jss_b->session, timelimit, NULL, NULL, NULL);
	return JS_TRUE;
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
		if ( argc > 4) {
			file = JS_GetStringBytes(JS_ValueToString(cx, argv[4]));
		}
		snprintf(filename, 80, "%smail.%ld%04x", SWITCH_GLOBAL_dirs.temp_dir, time(NULL), rand() & 0xffff);

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
				
				if (l > 0) {
					out[bytes++] = c64[((b%16)<<(6-l))%64];
				}
				if (l != 0) while (l < 6) {
					out[bytes++] = '=', l += 2;
				}
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

		if (fd) {
			close(fd);
		}
		if (ifd) {
			close(ifd);
		}
		snprintf(buf, B64BUFFLEN, "/bin/cat %s | /usr/sbin/sendmail -tf \"%s\" %s", filename, from, to);
		system(buf);
		unlink(filename);


		if (file) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "Emailed file [%s] to [%s]\n", filename, to);
		} else {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "Emailed data to [%s]\n", to);
		}
		return JS_TRUE;
	}
	

	return JS_FALSE;
}

static JSFunctionSpec fs_functions[] = {
	{"console_log", js_log, 2}, 
	{"include", js_include, 1}, 
	{"email", js_email, 2}, 
	{"bridge", js_bridge, 2},
	{"apiExecute", js_api_execute, 2},
#ifdef HAVE_CURL
	{"fetchURLHash", js_fetchurl_hash, 1}, 
	{"fetchURLFile", js_fetchurl_file, 1}, 
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

static int env_init(JSContext *cx, JSObject *javascript_object)
{

	JS_DefineFunctions(cx, javascript_object, fs_functions);

	JS_InitStandardClasses(cx, javascript_object);
				
	JS_InitClass(cx,
				 javascript_object,
				 NULL,
				 &teletone_class,
				 teletone_construct,
				 3,
				 teletone_props,
				 teletone_methods,
				 teletone_props,
				 teletone_methods
				 );

	JS_InitClass(cx,
				 javascript_object,
				 NULL,
				 &session_class,
				 session_construct,
				 3,
				 session_props,
				 session_methods,
				 session_props,
				 session_methods
				 );

	JS_InitClass(cx,
				 javascript_object,
				 NULL,
				 &fileio_class,
				 fileio_construct,
				 3,
				 fileio_props,
				 fileio_methods,
				 fileio_props,
				 fileio_methods
				 );

	JS_InitClass(cx,
				 javascript_object,
				 NULL,
				 &db_class,
				 db_construct,
				 3,
				 db_props,
				 db_methods,
				 db_props,
				 db_methods
				 );

	return 1;
}

static void js_parse_and_execute(switch_core_session *session, char *input_code)
{
	JSObject *javascript_global_object = NULL;
	char buf[1024], *script, *arg, *argv[512];
	int argc = 0, x = 0, y = 0;
	unsigned int flags = 0;
	struct js_session jss;
	JSContext *cx = NULL;
	jsval rval;

	
	if ((cx = JS_NewContext(globals.rt, globals.gStackChunkSize))) {
		JS_SetErrorReporter(cx, js_error);
		javascript_global_object = JS_NewObject(cx, &global_class, NULL, NULL);
		env_init(cx, javascript_global_object);
		JS_SetGlobalObject(cx, javascript_global_object);

		/* Emaculent conception of session object into the script if one is available */
		if (session && new_js_session(cx, javascript_global_object, session, &jss, "session", flags)) {
			JS_SetPrivate(cx, javascript_global_object, session);
		}
	} else {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Allocation Error!\n");
		return;
	}

	script = input_code;

	if ((arg = strchr(script, ' '))) {
		*arg++ = '\0';
		argc = switch_separate_string(arg, ':', argv, (sizeof(argv) / sizeof(argv[0])));
	}
	
	if (argc) { /* create a js doppleganger of this argc/argv*/
		snprintf(buf, sizeof(buf), "~var argv = new Array(%d);", argc);
		eval_some_js(buf, cx, javascript_global_object, &rval);
		snprintf(buf, sizeof(buf), "~var argc = %d", argc);
		eval_some_js(buf, cx, javascript_global_object, &rval);

		for (y = 0; y < argc; y++) {
			snprintf(buf, sizeof(buf), "~argv[%d] = \"%s\";", x++, argv[y]);
			eval_some_js(buf, cx, javascript_global_object, &rval);

		}
	}

	if (cx) {
		eval_some_js(script, cx, javascript_global_object, &rval);
		JS_DestroyContext(cx);
	}
}




static void *SWITCH_THREAD_FUNC js_thread_run(switch_thread *thread, void *obj)
{
	char *input_code = obj;

	js_parse_and_execute(NULL, input_code);

	if (input_code) {
		free(input_code);
	}

	return NULL;
}


static switch_memory_pool *module_pool = NULL;

static void js_thread_launch(char *text)
{
	switch_thread *thread;
	switch_threadattr_t *thd_attr = NULL;

	if (!module_pool) {
		if (switch_core_new_memory_pool(&module_pool) != SWITCH_STATUS_SUCCESS) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "OH OH no pool\n");
			return;
		}
	}
	
	switch_threadattr_create(&thd_attr, module_pool);
	switch_threadattr_detach_set(thd_attr, 1);
	switch_threadattr_stacksize_set(thd_attr, SWITCH_THREAD_STACKSIZE);
	switch_thread_create(&thread, thd_attr, js_thread_run, strdup(text), module_pool);
}


static switch_status launch_async(char *text, char *out, size_t outlen)
{

	if (switch_strlen_zero(text)) {
		switch_copy_string(out, "INVALID", outlen);
		return SWITCH_STATUS_SUCCESS;
	}

	js_thread_launch(text);
	switch_copy_string(out, "OK", outlen);
	return SWITCH_STATUS_SUCCESS;
}


static const switch_application_interface ivrtest_application_interface = {
	/*.interface_name */ "javascript",
	/*.application_function */ js_parse_and_execute,
	NULL, NULL, NULL,
	/*.next*/ NULL
};

static struct switch_api_interface js_run_interface = {
	/*.interface_name */ "jsrun",
	/*.desc */ "run a script",
	/*.function */ launch_async,
	/*.next */ NULL
};

static switch_loadable_module_interface spidermonkey_module_interface = {
	/*.module_name */ modname,
	/*.endpoint_interface */ NULL,
	/*.timer_interface */ NULL,
	/*.dialplan_interface */ NULL,
	/*.codec_interface */ NULL,
	/*.application_interface */ &ivrtest_application_interface,
	/*.api_interface */ &js_run_interface,
	/*.file_interface */ NULL,
	/*.speech_interface */ NULL,
	/*.directory_interface */ NULL
};

SWITCH_MOD_DECLARE(switch_status) switch_module_load(const switch_loadable_module_interface **interface, char *filename)
{
	switch_status status;

	if ((status = init_js()) != SWITCH_STATUS_SUCCESS) {
		return status;
	}

	/* connect my internal structure to the blank pointer passed to me */
	*interface = &spidermonkey_module_interface;

	/* indicate that the module should continue to be loaded */
	return SWITCH_STATUS_SUCCESS;
}
