/*
 * mod_v8 for FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
 * Copyright (C) 2013-2014, Peter Olsson <peter@olssononline.se>
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
 * Ported from the Original Code in FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
 *
 * The Initial Developer of the Original Code is
 * Anthony Minessale II <anthm@freeswitch.org>
 * Portions created by the Initial Developer are Copyright (C)
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 * Peter Olsson <peter@olssononline.se>
 * Anthony Minessale II <anthm@freeswitch.org>
 *
 * fsteletone.cpp -- JavaScript TeleTone class
 *
 */

#include "fsteletone.hpp"
#include "fssession.hpp"

using namespace std;
using namespace v8;

static const char js_class_name[] = "TeleTone";

FSTeleTone::~FSTeleTone(void)
{
	/* Release persistent JS handles */
	_function.Reset();
	_arg.Reset();

	if (_timer) {
		switch_core_timer_destroy(_timer);
	}

	teletone_destroy_session(&_ts);
	switch_buffer_destroy(&_audio_buffer);
	switch_core_codec_destroy(&_codec);

	if (_pool) {
		switch_core_destroy_memory_pool(&_pool);
	}
}

string FSTeleTone::GetJSClassName()
{
	return js_class_name;
}

void FSTeleTone::Init()
{
	memset(&_ts, 0, sizeof(_ts));
	_session = NULL;
	memset(&_codec, 0, sizeof(_codec));
	_audio_buffer = NULL;
	_pool = NULL;
	_timer = NULL;
	memset(&_timer_base, 0, sizeof(_timer_base));
	flags = 0;
}

typedef enum {
	TTF_DTMF = (1 << 0)
} teletone_flag_t;

int FSTeleTone::Handler(teletone_generation_session_t *ts, teletone_tone_map_t *map)
{
	FSTeleTone *tto = (FSTeleTone *)ts->user_data;
	int wrote;

	if (!tto) {
		return -1;
	}

	wrote = teletone_mux_tones(ts, map);
	switch_buffer_write(tto->_audio_buffer, ts->buffer, wrote * 2);

	return 0;
}

void *FSTeleTone::Construct(const v8::FunctionCallbackInfo<Value>& info)
{
	FSTeleTone *tto = NULL;
	FSSession *jss = NULL;
	switch_memory_pool_t *pool;
	string timer_name;
	switch_codec_implementation_t read_impl;

	memset(&read_impl, 0, sizeof(read_impl));

	if (info.Length() > 0 && info[0]->IsObject()) {
		Handle<Object> session_obj(Handle<Object>::Cast(info[0]));

		if (!session_obj.IsEmpty()) {
			if (!(jss = JSBase::GetInstance<FSSession>(session_obj)) || !jss->GetSession()) {
				info.GetIsolate()->ThrowException(String::NewFromUtf8(info.GetIsolate(), "Cannot Find Session [1]"));
				return NULL;
			}
		} else {
			info.GetIsolate()->ThrowException(String::NewFromUtf8(info.GetIsolate(), "Cannot Find Session [2]"));
			return NULL;
		}
	} else {
		info.GetIsolate()->ThrowException(String::NewFromUtf8(info.GetIsolate(), "Missing Session Arg"));
		return NULL;
	}

	if (info.Length() > 1) {
		String::Utf8Value str(info[1]);
		timer_name = js_safe_str(*str);
	}

	switch_core_new_memory_pool(&pool);

	if (!(tto = new FSTeleTone(info))) {
		switch_core_destroy_memory_pool(&pool);
		info.GetIsolate()->ThrowException(String::NewFromUtf8(info.GetIsolate(), "Memory Error"));
		return NULL;
	}

	switch_core_session_get_read_impl(jss->GetSession(), &read_impl);

	if (switch_core_codec_init(&tto->_codec,
							   "L16",
							   NULL,
							   NULL,
							   read_impl.actual_samples_per_second,
							   read_impl.microseconds_per_packet / 1000,
							   read_impl.number_of_channels, SWITCH_CODEC_FLAG_ENCODE | SWITCH_CODEC_FLAG_DECODE, NULL, pool) == SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Raw Codec Activated\n");
	} else {
		switch_core_destroy_memory_pool(&pool);
		delete tto;
		info.GetIsolate()->ThrowException(String::NewFromUtf8(info.GetIsolate(), "Raw codec activation failed"));
		return NULL;
	}

	if (timer_name.length() > 0) {
		unsigned int ms = read_impl.microseconds_per_packet / 1000;
		if (switch_core_timer_init(&tto->_timer_base,
			timer_name.c_str(), ms, (read_impl.samples_per_second / 50) * read_impl.number_of_channels, pool) == SWITCH_STATUS_SUCCESS) {
			tto->_timer = &tto->_timer_base;
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Timer INIT Success %u\n", ms);
		} else {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Timer INIT Failed\n");
		}
	}

	switch_buffer_create_dynamic(&tto->_audio_buffer, JS_BLOCK_SIZE, JS_BUFFER_SIZE, 0);
	tto->_pool = pool;
	tto->_session = jss->GetSession();
	teletone_init_session(&tto->_ts, 0, FSTeleTone::Handler, tto);

	return tto;
}

JS_TELETONE_FUNCTION_IMPL(AddTone)
{
	if (info.Length() > 2) {
		int x;
		int nmax = info.Length();
		const char *map_str;
		String::Utf8Value str(info[0]);
		map_str = js_safe_str(*str);

		if ( TELETONE_MAX_TONES < nmax ) {
			nmax = TELETONE_MAX_TONES;
		}

		for (x = 1; x < nmax; x++) {
			String::Utf8Value fval(info[x]);
			if (*fval) {
				_ts.TONES[(int) *map_str].freqs[x - 1] = strtod(*fval, NULL);
			}
		}
		return;
	}

	info.GetIsolate()->ThrowException(String::NewFromUtf8(info.GetIsolate(), "Invalid arguments"));
}

JS_TELETONE_FUNCTION_IMPL(OnDTMF)
{
	/* Clear callbacks first */
	_function.Reset();
	_arg.Reset();
	switch_clear_flag(this, TTF_DTMF);

	info.GetReturnValue().Set(false);

	if (info.Length() > 0) {
		Handle<Function> func = JSBase::GetFunctionFromArg(info.GetIsolate(), info[0]);

		if (!func.IsEmpty()) {
			_function.Reset(info.GetIsolate(), func);
			if (info.Length() > 1 && !info[1].IsEmpty()) {
				_arg.Reset(info.GetIsolate(), info[1]);
			}
			switch_set_flag(this, TTF_DTMF);
			info.GetReturnValue().Set(true);
		}
	}
}

JS_TELETONE_FUNCTION_IMPL(Generate)
{
	int32_t loops = 0;

	if (info.Length() > 0) {
		const char *script;
		switch_core_session_t *session;
		switch_frame_t write_frame = { 0 };
		unsigned char *fdata[1024];
		switch_frame_t *read_frame;
		switch_channel_t *channel;

		if (info.Length() > 1) {
			if (!info[1]->IsInt32() || !(loops = info[1]->Int32Value())) {
				info.GetIsolate()->ThrowException(String::NewFromUtf8(info.GetIsolate(), "Cannot get second argument (should be int)"));
				return;
			}
			loops--;
		}

		if (_audio_buffer) {
			switch_buffer_zero(_audio_buffer);
		}

		_ts.debug = 1;
		_ts.debug_stream = switch_core_get_console();

		String::Utf8Value str(info[0]);
		script = js_safe_str(*str);
		teletone_run(&_ts, script);

		session = this->_session;
		write_frame.codec = &_codec;
		write_frame.data = fdata;
		write_frame.buflen = sizeof(fdata);

		channel = switch_core_session_get_channel(session);

		if (_timer) {
			switch_core_service_session(session);
		}

		if (_audio_buffer && loops) {
			switch_buffer_set_loops(_audio_buffer, loops);
		}

		for (;;) {
			if (switch_test_flag(this, TTF_DTMF)) {
				char dtmf[128];
				const char *ret;

				if (switch_channel_has_dtmf(channel)) {
					HandleScope hs(info.GetIsolate());
					uint32_t aargc = 0;
					Handle<Value> aargv[4];

					switch_channel_dequeue_dtmf_string(channel, dtmf, sizeof(dtmf));
					aargv[aargc++] = String::NewFromUtf8(info.GetIsolate(), dtmf);

					if (!_arg.IsEmpty()) {
						aargv[aargc++] = Local<Value>::New(info.GetIsolate(), _arg);
					}

					Handle<Function> func = Local<Function>::New(info.GetIsolate(), _function);
					Handle<Value> res = func->Call(info.GetIsolate()->GetCurrentContext()->Global(), aargc, aargv);

					String::Utf8Value tmp(res);
					ret = js_safe_str(*tmp);

					if (strcmp(ret, "true") && strcmp(ret, "undefined")) {
						info.GetReturnValue().Set(res);
						return;
					}
				}
			}

			if (_timer) {
				if (switch_core_timer_next(_timer) != SWITCH_STATUS_SUCCESS) {
					break;
				}

			} else {
				switch_status_t status;
				status = switch_core_session_read_frame(session, &read_frame, SWITCH_IO_FLAG_NONE, 0);

				if (!SWITCH_READ_ACCEPTABLE(status)) {
					break;
				}
			}
			if (!_audio_buffer || (write_frame.datalen = (uint32_t) switch_buffer_read_loop(_audio_buffer,
																		  fdata, write_frame.codec->implementation->decoded_bytes_per_packet)) <= 0) {
				break;
			}

			write_frame.samples = write_frame.datalen / 2;
			if (switch_core_session_write_frame(session, &write_frame, SWITCH_IO_FLAG_NONE, 0) != SWITCH_STATUS_SUCCESS) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Bad Write\n");
				break;
			}
		}

		if (_timer) {
			switch_core_thread_session_end(session);
		}

		return;
	}

	info.GetIsolate()->ThrowException(String::NewFromUtf8(info.GetIsolate(), "Invalid arguments"));
}

JS_TELETONE_GET_PROPERTY_IMPL(GetNameProperty)
{
	info.GetReturnValue().Set(String::NewFromUtf8(info.GetIsolate(), "TeleTone"));
}

static const js_function_t teletone_methods[] = {
	{"generate", FSTeleTone::Generate},
	{"onDTMF", FSTeleTone::OnDTMF},
	{"addTone", FSTeleTone::AddTone},
	{0}
};

static const js_property_t teletone_props[] = {
	{"name", FSTeleTone::GetNameProperty, JSBase::DefaultSetProperty},
	{0}
};

static const js_class_definition_t teletone_desc = {
	js_class_name,
	FSTeleTone::Construct,
	teletone_methods,
	teletone_props
};

static switch_status_t teletone_load(const v8::FunctionCallbackInfo<Value>& info)
{
	JSBase::Register(info.GetIsolate(), &teletone_desc);
	return SWITCH_STATUS_SUCCESS;
}

static const v8_mod_interface_t teletone_module_interface = {
	/*.name = */ js_class_name,
	/*.js_mod_load */ teletone_load
};

const v8_mod_interface_t *FSTeleTone::GetModuleInterface()
{
	return &teletone_module_interface;
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
