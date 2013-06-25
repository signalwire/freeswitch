/* 
 * FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
 * Copyright (C) 2005-2012, Anthony Minessale II <anthm@freeswitch.org>
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
 * mod_spidermonkey_teletone.c -- TeleTone Javascript Module
 *
 */
#include "mod_spidermonkey.h"
#include <libteletone.h>

static const char modname[] = "TeleTone";

typedef enum {
	TTF_DTMF = (1 << 0)
} teletone_flag_t;


struct teletone_obj {
	teletone_generation_session_t ts;
	JSContext *cx;
	JSObject *obj;
	switch_core_session_t *session;
	switch_codec_t codec;
	switch_buffer_t *audio_buffer;
	switch_memory_pool_t *pool;
	switch_timer_t *timer;
	switch_timer_t timer_base;
	JSFunction *function;
	jsval arg;
	jsval ret;
	unsigned int flags;
};


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
static JSBool teletone_construct(JSContext * cx, JSObject * obj, uintN argc, jsval * argv, jsval * rval)
{
	JSObject *session_obj;
	struct teletone_obj *tto = NULL;
	struct js_session *jss = NULL;
	switch_memory_pool_t *pool;
	char *timer_name = NULL;
	switch_codec_implementation_t read_impl = { 0 };



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

	switch_core_new_memory_pool(&pool);

	if (!(tto = switch_core_alloc(pool, sizeof(*tto)))) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Memory Error\n");
		return JS_FALSE;
	}

	switch_core_session_get_read_impl(jss->session, &read_impl);

	if (switch_core_codec_init(&tto->codec,
							   "L16",
							   NULL,
							   read_impl.actual_samples_per_second,
							   read_impl.microseconds_per_packet / 1000,
							   read_impl.number_of_channels, SWITCH_CODEC_FLAG_ENCODE | SWITCH_CODEC_FLAG_DECODE, NULL, pool) == SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Raw Codec Activated\n");
	} else {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Raw Codec Activation Failed\n");
		return JS_FALSE;
	}

	if (timer_name) {
		unsigned int ms = read_impl.microseconds_per_packet / 1000;
		if (switch_core_timer_init(&tto->timer_base,
								   timer_name, ms, (read_impl.samples_per_second / 50) * read_impl.number_of_channels, pool) == SWITCH_STATUS_SUCCESS) {
			tto->timer = &tto->timer_base;
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Timer INIT Success %u\n", ms);
		} else {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Timer INIT Failed\n");
		}
	}

	switch_buffer_create_dynamic(&tto->audio_buffer, JS_BLOCK_SIZE, JS_BUFFER_SIZE, 0);
	tto->pool = pool;
	tto->obj = obj;
	tto->cx = cx;
	tto->session = jss->session;
	teletone_init_session(&tto->ts, 0, teletone_handler, tto);
	JS_SetPrivate(cx, obj, tto);

	return JS_TRUE;
}

static void teletone_destroy(JSContext * cx, JSObject * obj)
{
	struct teletone_obj *tto = JS_GetPrivate(cx, obj);
	switch_memory_pool_t *pool;
	if (tto) {
		if (tto->timer) {
			switch_core_timer_destroy(tto->timer);
		}
		teletone_destroy_session(&tto->ts);
		switch_buffer_destroy(&tto->audio_buffer);
		switch_core_codec_destroy(&tto->codec);
		pool = tto->pool;
		tto->pool = NULL;
		if (pool) {
			switch_core_destroy_memory_pool(&pool);
		}
	}
}

static JSBool teletone_add_tone(JSContext * cx, JSObject * obj, uintN argc, jsval * argv, jsval * rval)
{
	struct teletone_obj *tto = JS_GetPrivate(cx, obj);
	if (argc > 2) {
		int x;
        int nmax = argc;
		char *fval;
		char *map_str;
		map_str = JS_GetStringBytes(JS_ValueToString(cx, argv[0]));

		if ( TELETONE_MAX_TONES < nmax ) {
			nmax = TELETONE_MAX_TONES;
		}

		for (x = 1; x < nmax; x++) {
			fval = JS_GetStringBytes(JS_ValueToString(cx, argv[x]));
			tto->ts.TONES[(int) *map_str].freqs[x - 1] = strtod(fval, NULL);
		}
		return JS_TRUE;
	}

	return JS_FALSE;
}

static JSBool teletone_on_dtmf(JSContext * cx, JSObject * obj, uintN argc, jsval * argv, jsval * rval)
{
	struct teletone_obj *tto = JS_GetPrivate(cx, obj);
	if (argc > 0) {
		tto->function = JS_ValueToFunction(cx, argv[0]);
		if (argc > 1) {
			tto->arg = argv[1];
		}
		switch_set_flag(tto, TTF_DTMF);
	}
	return JS_TRUE;
}

static JSBool teletone_generate(JSContext * cx, JSObject * obj, uintN argc, jsval * argv, jsval * rval)
{
	struct teletone_obj *tto = JS_GetPrivate(cx, obj);
	int32 loops = 0;

	if (argc > 0) {
		char *script;
		switch_core_session_t *session;
		switch_frame_t write_frame = { 0 };
		unsigned char *fdata[1024];
		switch_frame_t *read_frame;
		switch_channel_t *channel;

		if (argc > 1) {
			if (!JS_ValueToInt32(cx, argv[1], &loops)) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Cannot Convert to INT\n");
				return JS_FALSE;
			}
			loops--;
		}

		if (tto->audio_buffer) {
			switch_buffer_zero(tto->audio_buffer);
		}

		tto->ts.debug = 1;
		tto->ts.debug_stream = switch_core_get_console();

		script = JS_GetStringBytes(JS_ValueToString(cx, argv[0]));
		teletone_run(&tto->ts, script);

		session = tto->session;
		write_frame.codec = &tto->codec;
		write_frame.data = fdata;
		write_frame.buflen = sizeof(fdata);

		channel = switch_core_session_get_channel(session);

		if (tto->timer) {
			switch_core_service_session(session);
		}

		if (loops) {
			switch_buffer_set_loops(tto->audio_buffer, loops);
		}

		for (;;) {

			if (switch_test_flag(tto, TTF_DTMF)) {
				char dtmf[128];
				char *ret;

				if (switch_channel_has_dtmf(channel)) {
					uintN aargc = 0;
					jsval aargv[4];

					switch_channel_dequeue_dtmf_string(channel, dtmf, sizeof(dtmf));
					aargv[aargc++] = STRING_TO_JSVAL(JS_NewStringCopyZ(cx, dtmf));
					JS_CallFunction(cx, obj, tto->function, aargc, aargv, &tto->ret);
					ret = JS_GetStringBytes(JS_ValueToString(cx, tto->ret));
					if (strcmp(ret, "true") && strcmp(ret, "undefined")) {
						*rval = tto->ret;
						return JS_TRUE;
					}
				}
			}

			if (tto->timer) {
				if (switch_core_timer_next(tto->timer) != SWITCH_STATUS_SUCCESS) {
					break;
				}

			} else {
				switch_status_t status;
				status = switch_core_session_read_frame(session, &read_frame, SWITCH_IO_FLAG_NONE, 0);

				if (!SWITCH_READ_ACCEPTABLE(status)) {
					break;
				}
			}
			if ((write_frame.datalen = (uint32_t) switch_buffer_read_loop(tto->audio_buffer,
																		  fdata, write_frame.codec->implementation->decoded_bytes_per_packet)) <= 0) {
				break;
			}

			write_frame.samples = write_frame.datalen / 2;
			if (switch_core_session_write_frame(session, &write_frame, SWITCH_IO_FLAG_NONE, 0) != SWITCH_STATUS_SUCCESS) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Bad Write\n");
				break;
			}
		}

		if (tto->timer) {
			switch_core_thread_session_end(session);
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
	{"name", TELETONE_NAME, JSPROP_READONLY | JSPROP_PERMANENT},
	{0}
};


static JSBool teletone_getProperty(JSContext * cx, JSObject * obj, jsval id, jsval * vp)
{
	JSBool res = JS_TRUE;

	return res;
}

JSClass teletone_class = {
	modname, JSCLASS_HAS_PRIVATE,
	JS_PropertyStub, JS_PropertyStub, teletone_getProperty, DEFAULT_SET_PROPERTY,
	JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub, teletone_destroy, NULL, NULL, NULL,
	teletone_construct
};


switch_status_t teletone_load(JSContext * cx, JSObject * obj)
{
	JS_InitClass(cx, obj, NULL, &teletone_class, teletone_construct, 3, teletone_props, teletone_methods, teletone_props, teletone_methods);
	return SWITCH_STATUS_SUCCESS;
}


const sm_module_interface_t teletone_module_interface = {
	/*.name = */ modname,
	/*.spidermonkey_load */ teletone_load,
	/*.next */ NULL
};

SWITCH_MOD_DECLARE_NONSTD(switch_status_t) spidermonkey_init(const sm_module_interface_t ** module_interface)
{
	*module_interface = &teletone_module_interface;
	return SWITCH_STATUS_SUCCESS;
}

/* For Emacs:
 * Local Variables:
 * mode:c
 * indent-tabs-mode:t
 * tab-width:4
 * c-basic-offset:4
 * End:
 * For VIM:
 * vim:set softtabstop=4 shiftwidth=4 tabstop=4 noet:
 */
