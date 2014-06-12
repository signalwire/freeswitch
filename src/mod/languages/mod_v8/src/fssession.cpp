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
 * William King <william.king@quentustech.com>
 *
 * fssession.cpp -- JavaScript Session class
 *
 */

#include "fssession.hpp"
#include "fsevent.hpp"
#include "fsdtmf.hpp"

using namespace std;
using namespace v8;

static const char js_class_name[] = "Session";

#define METHOD_SANITY_CHECK()  if (!this->_session) {	\
		info.GetIsolate()->ThrowException(String::NewFromUtf8(info.GetIsolate(), "No session is active, you must have an active session before calling this method"));\
		return;\
	} else CheckHangupHook(this, NULL)

#define CHANNEL_SANITY_CHECK() do {\
		if (!switch_channel_ready(channel)) {\
			info.GetIsolate()->ThrowException(String::NewFromUtf8(info.GetIsolate(), "Session is not active!"));\
			return;\
		}\
		if (!((switch_channel_test_flag(channel, CF_ANSWERED) || switch_channel_test_flag(channel, CF_EARLY_MEDIA)))) {\
			switch_channel_pre_answer(channel);\
			if (!((switch_channel_test_flag(channel, CF_ANSWERED) || switch_channel_test_flag(channel, CF_EARLY_MEDIA)))) {\
				info.GetIsolate()->ThrowException(String::NewFromUtf8(info.GetIsolate(), "Session is not answered!"));\
				return;\
			}\
		}\
	} while (foo == 1)

#define CHANNEL_SANITY_CHECK_ANSWER() do {\
		if (!switch_channel_ready(channel)) {\
			info.GetIsolate()->ThrowException(String::NewFromUtf8(info.GetIsolate(), "Session is not active!"));\
			return;\
		}\
	} while (foo == 1)

#define CHANNEL_MEDIA_SANITY_CHECK() do {\
		if (!switch_channel_media_ready(channel)) {\
			info.GetIsolate()->ThrowException(String::NewFromUtf8(info.GetIsolate(), "Session is not in media mode!"));\
			return;\
		}\
	} while (foo == 1)

static int foo = 0;

string FSSession::GetJSClassName()
{
	return js_class_name;
}

void FSSession::Init(void)
{
	_session = NULL;
	flags = 0;
	_cause = SWITCH_CAUSE_NONE;
	_stack_depth = 0;
	_hook_state = CS_EXECUTE;
	_destination_number = NULL;
	_dialplan = NULL;
	_caller_id_name = NULL;
	_caller_id_number = NULL;
	_network_addr = NULL;
	_ani = NULL;
	_aniii = NULL;
	_rdnis = NULL;
	_context = NULL;
	_username = NULL;
	_check_state = 1;
	_speech = NULL;
}

switch_core_session_t *FSSession::GetSession()
{
	return _session;
}

void FSSession::Init(switch_core_session_t *session, unsigned int flags)
{
	this->_session = session;
	this->flags = flags;
}

void FSSession::DestroySpeechEngine()
{
	if (_speech) {
		switch_speech_flag_t flags = SWITCH_SPEECH_FLAG_NONE;
		switch_core_codec_destroy(&_speech->codec);
		switch_core_speech_close(&_speech->sh, &flags);
		_speech = NULL;
	}
}

FSSession::~FSSession(void)
{
	_on_hangup.Reset();

	if (_speech && *_speech->sh.name) {
		DestroySpeechEngine();
	}

	if (_session) {
		switch_channel_t *channel = switch_core_session_get_channel(_session);

		switch_channel_set_private(channel, "jsobject", NULL);
		switch_core_event_hook_remove_state_change(_session, HangupHook);

		if (switch_test_flag(this, S_HUP)) {
			switch_channel_hangup(channel, SWITCH_CAUSE_NORMAL_CLEARING);
		}

		switch_safe_free(_dialplan);
		switch_safe_free(_username);
		switch_safe_free(_caller_id_name);
		switch_safe_free(_ani);
		switch_safe_free(_aniii);
		switch_safe_free(_caller_id_number);
		switch_safe_free(_network_addr);
		switch_safe_free(_rdnis);
		switch_safe_free(_destination_number);
		switch_safe_free(_context);
		switch_core_session_rwunlock(_session);
	}
}

FSInputCallbackState::FSInputCallbackState(void)
{
	session_state = NULL;
	memset(&code_buffer, 0, sizeof(code_buffer));
	code_buffer_len = 0;
	memset(&ret_buffer, 0, sizeof(ret_buffer));
	ret_buffer_len = 0;
	digit_count = 0;
	extra = NULL;
	jss_a = NULL;
	jss_b = NULL;
}

FSInputCallbackState::~FSInputCallbackState(void)
{
	function.Reset();
	arg.Reset();
	ret.Reset();
	session_obj_a.Reset();
	session_obj_b.Reset();
}

#define MAX_STACK_DEPTH 2

switch_status_t FSSession::CommonCallback(switch_core_session_t *session, void *input, switch_input_type_t itype, void *buf, unsigned int buflen)
{
	FSInputCallbackState *cb_state = (FSInputCallbackState *)buf;
	switch_event_t *event = NULL;
	int argc = 0;
	Handle<Value> argv[4];
	Handle<Object> Event;
	bool ret = true;
	switch_status_t status = SWITCH_STATUS_FALSE;

	/* Session sanity check first */
	if (!cb_state->session_state || !cb_state->session_state->_session) {
		if (cb_state->session_state && cb_state->session_state->GetIsolate()) {
			cb_state->session_state->GetIsolate()->ThrowException(String::NewFromUtf8(cb_state->session_state->GetIsolate(), "No session is active, you must have an active session before calling this method"));
		} else {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "No session is active, you must have an active session before calling this method\n");
		}
		return SWITCH_STATUS_FALSE;
	} else {
		/* Check hangup hook before we continue */
		if (!CheckHangupHook(cb_state->session_state, NULL)) {
			JSMain::ExitScript(cb_state->session_state->GetIsolate(), NULL);
			return SWITCH_STATUS_FALSE;
		}
	}

	if (++cb_state->session_state->_stack_depth > MAX_STACK_DEPTH) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Maximum recursive callback limit %d reached.\n", MAX_STACK_DEPTH);
		cb_state->session_state->_stack_depth--;
		return SWITCH_STATUS_FALSE;
	}

	Isolate *isolate = cb_state->session_state->GetIsolate();
	HandleScope handle_scope(isolate);

	if (cb_state->jss_a && cb_state->jss_a->_session && cb_state->jss_a->_session == session) {
		argv[argc++] = Local<Object>::New(isolate, cb_state->session_obj_a);
	} else if (cb_state->jss_b && cb_state->jss_b->_session && cb_state->jss_b->_session == session) {
		argv[argc++] = Local<Object>::New(isolate, cb_state->session_obj_b);
	} else {
		argv[argc++] = Local<Object>::New(isolate, cb_state->session_state->GetJavaScriptObject());
	}

	switch (itype) {
	case SWITCH_INPUT_TYPE_EVENT:
		if ((event = (switch_event_t *) input)) {
			Event = FSEvent::New(event, "", cb_state->session_state->GetOwner());
			if (!Event.IsEmpty()) {
				argv[argc++] = String::NewFromUtf8(isolate, "event");
				argv[argc++] = Local<Object>::New(isolate, Event);
			}
		}
		if (Event.IsEmpty()) {
			goto done;
		}
		break;
	case SWITCH_INPUT_TYPE_DTMF:
		{
			switch_dtmf_t *dtmf = (switch_dtmf_t *) input;

			if (dtmf) {
				Event = FSDTMF::New(dtmf, "", cb_state->session_state->GetOwner());
				if (!Event.IsEmpty()) {
					argv[argc++] = String::NewFromUtf8(isolate, "dtmf");
					argv[argc++] = Local<Object>::New(isolate, Event);
				} else {
					goto done;
				}
			}
		}
		break;
	}

	if (!cb_state->arg.IsEmpty()) {
		argv[argc++] = Local<Value>::New(isolate, cb_state->arg);
	}

	CheckHangupHook(cb_state->session_state, &ret);

	if (ret) {
		if (!cb_state->function.IsEmpty()) {
			Handle<Function> func = Local<Function>::New(isolate, cb_state->function);

			if (func->IsFunction()) {
				Handle<Value> res = func->Call(isolate->GetCurrentContext()->Global(), argc, argv);

				if (!res.IsEmpty()) {
					cb_state->ret.Reset(isolate, res);
				} else {
					cb_state->ret.Reset();
				}
			}
		}
	} else {
		JSMain::ExitScript(cb_state->session_state->GetIsolate(), NULL);
		status = SWITCH_STATUS_FALSE;
		goto done;
	}

	status = SWITCH_STATUS_SUCCESS;
  done:
	cb_state->session_state->_stack_depth--;
	return status;
}

switch_status_t FSSession::StreamInputCallback(switch_core_session_t *session, void *input, switch_input_type_t itype, void *buf, unsigned int buflen)
{
	FSInputCallbackState *cb_state = (FSInputCallbackState *)buf;
	FSSession *obj = cb_state->session_state;
	HandleScope handle_scope(obj->GetOwner()->GetIsolate());
	switch_status_t status;
	switch_file_handle_t *fh = (switch_file_handle_t *)cb_state->extra;

	if (!switch_test_flag(fh, SWITCH_FILE_OPEN)) {
		return SWITCH_STATUS_FALSE;
	}

	if ((status = CommonCallback(session, input, itype, buf, buflen)) != SWITCH_STATUS_SUCCESS) {
		return status;
	}

	if (!cb_state->ret.IsEmpty()) {
		Handle<Value> tmp = Local<Value>::New(obj->GetOwner()->GetIsolate(), cb_state->ret);
		String::Utf8Value str(tmp);
		const char *ret = js_safe_str(*str);

		if (!strncasecmp(ret, "speed", 5)) {
			const char *p;

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
		} else if (!strncasecmp(ret, "volume", 6)) {
			const char *p;

			if ((p = strchr(ret, ':'))) {
				p++;
				if (*p == '+' || *p == '-') {
					int step;
					if (!(step = atoi(p))) {
						step = 1;
					}
					fh->vol += step;
				} else {
					int vol = atoi(p);
					fh->vol = vol;
				}
				return SWITCH_STATUS_SUCCESS;
			}

			if (fh->vol) {
				switch_normalize_volume(fh->vol);
			}

			return SWITCH_STATUS_FALSE;
		} else if (!strcasecmp(ret, "pause")) {
			if (switch_test_flag(fh, SWITCH_FILE_PAUSE)) {
				switch_clear_flag(fh, SWITCH_FILE_PAUSE);
			} else {
				switch_set_flag(fh, SWITCH_FILE_PAUSE);
			}
			return SWITCH_STATUS_SUCCESS;
		} else if (!strcasecmp(ret, "truncate")) {
			switch_core_file_truncate(fh, 0);
		} else if (!strcasecmp(ret, "restart")) {
			uint32_t pos = 0;
			fh->speed = 0;
			switch_core_file_seek(fh, &pos, 0, SEEK_SET);
			return SWITCH_STATUS_SUCCESS;
		} else if (!strncasecmp(ret, "seek", 4)) {
			uint32_t samps = 0;
			uint32_t pos = 0;
			const char *p;

			if ((p = strchr(ret, ':'))) {
				p++;
				if (*p == '+' || *p == '-') {
					int step;
					if (!(step = atoi(p))) {
						step = 1000;
					}
					if (step > 0) {
						samps = step * (fh->native_rate / 1000);
						switch_core_file_seek(fh, &pos, samps, SEEK_CUR);
					} else {
						samps = abs(step) * (fh->native_rate / 1000);
						switch_core_file_seek(fh, &pos, fh->pos - samps, SEEK_SET);
					}
				} else {
					samps = atoi(p) * (fh->native_rate / 1000);
					switch_core_file_seek(fh, &pos, samps, SEEK_SET);
				}
			}

			return SWITCH_STATUS_SUCCESS;
		}

		if (!strcmp(ret, "true") || !strcmp(ret, "undefined")) {
			return SWITCH_STATUS_SUCCESS;
		}

		return SWITCH_STATUS_BREAK;
	}

	return SWITCH_STATUS_SUCCESS;
}

switch_status_t FSSession::RecordInputCallback(switch_core_session_t *session, void *input, switch_input_type_t itype, void *buf, unsigned int buflen)
{
	FSInputCallbackState *cb_state = (FSInputCallbackState *)buf;
	FSSession *obj = cb_state->session_state;
	HandleScope handle_scope(obj->GetOwner()->GetIsolate());
	switch_status_t status;
	switch_file_handle_t *fh = (switch_file_handle_t *)cb_state->extra;

	if ((status = CommonCallback(session, input, itype, buf, buflen)) != SWITCH_STATUS_SUCCESS) {
		return status;
	}

	if (!cb_state->ret.IsEmpty()) {
		Handle<Value> tmp = Local<Value>::New(obj->GetOwner()->GetIsolate(), cb_state->ret);
		String::Utf8Value str(tmp);
		const char *ret = js_safe_str(*str);

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

		return SWITCH_STATUS_BREAK;
	}

	return SWITCH_STATUS_SUCCESS;
}

switch_status_t FSSession::CollectInputCallback(switch_core_session_t *session, void *input, switch_input_type_t itype, void *buf, unsigned int buflen)
{
	FSInputCallbackState *cb_state = (FSInputCallbackState *)buf;
	FSSession *obj = cb_state->session_state;
	HandleScope handle_scope(obj->GetOwner()->GetIsolate());
	const char *ret;
	switch_status_t status;

	if ((status = CommonCallback(session, input, itype, buf, buflen)) != SWITCH_STATUS_SUCCESS) {
		return status;
	}

	if (!cb_state->ret.IsEmpty()) {
		Handle<Value> tmp = Local<Value>::New(obj->GetOwner()->GetIsolate(), cb_state->ret);
		String::Utf8Value str(tmp);
		ret = js_safe_str(*str);

		if (!strcmp(ret, "true") || !strcmp(ret, "undefined")) {
			return SWITCH_STATUS_SUCCESS;
		}
	}

	return SWITCH_STATUS_BREAK;
}

JS_SESSION_FUNCTION_IMPL(FlushDigits)
{
	HandleScope handle_scope(info.GetIsolate());
	switch_channel_t *channel;

	METHOD_SANITY_CHECK();
	channel = switch_core_session_get_channel(this->_session);
	CHANNEL_MEDIA_SANITY_CHECK();

	switch_channel_flush_dtmf(switch_core_session_get_channel(this->_session));

	info.GetReturnValue().Set(true);
}

JS_SESSION_FUNCTION_IMPL(FlushEvents)
{
	HandleScope handle_scope(info.GetIsolate());
	switch_event_t *event;

	if (!this->_session) {
		info.GetReturnValue().Set(false);
		return;
	}

	while (switch_core_session_dequeue_event(this->_session, &event, SWITCH_FALSE) == SWITCH_STATUS_SUCCESS) {
		switch_event_destroy(&event);
	}

	info.GetReturnValue().Set(true);
}

JS_SESSION_FUNCTION_IMPL(RecordFile)
{
	HandleScope handle_scope(info.GetIsolate());
	switch_channel_t *channel;
	string file_name;
	void *bp = NULL;
	int len = 0;
	switch_input_callback_function_t dtmf_func = NULL;
	FSInputCallbackState cb_state;
	switch_file_handle_t fh = { 0 };
	int32_t limit = 0;
	switch_input_args_t args = { 0 };
	bool ret = true;

	METHOD_SANITY_CHECK();
	channel = switch_core_session_get_channel(this->_session);
	CHANNEL_SANITY_CHECK();

	CHANNEL_MEDIA_SANITY_CHECK();

	if (info.Length() > 0) {
		String::Utf8Value str(info[0]);
		file_name = js_safe_str(*str);
		if (zstr(file_name.c_str())) {
			info.GetReturnValue().Set(false);
			return;
		}
	}

	if (info.Length() > 1) {
		Handle<Function> func = JSBase::GetFunctionFromArg(info.GetIsolate(), info[1]);

		if (!func.IsEmpty()) {
			cb_state.session_state = this;
			cb_state.function.Reset(info.GetIsolate(), func);
			if (info.Length() > 2 && !info[2].IsEmpty()) {
				cb_state.arg.Reset(info.GetIsolate(), info[2]);
			}

			dtmf_func = RecordInputCallback;
			bp = &cb_state;
			len = sizeof(cb_state);
		}

		if (info.Length() > 3) {
			limit = info[3]->Int32Value();
		}

		if (info.Length() > 4) {
			fh.thresh = info[4]->Int32Value();
		}

		if (info.Length() > 5) {
			fh.silence_hits = info[5]->Int32Value();
		}
	}

	cb_state.extra = &fh;
	cb_state.ret.Reset(info.GetIsolate(), Boolean::New(info.GetIsolate(), false));
	args.input_callback = dtmf_func;
	args.buf = bp;
	args.buflen = len;

	switch_ivr_record_file(this->_session, &fh, file_name.c_str(), &args, limit);
	info.GetReturnValue().Set(cb_state.ret);

	CheckHangupHook(this, &ret);
	if (!ret) JSMain::ExitScript(info.GetIsolate(), NULL);
}

JS_SESSION_FUNCTION_IMPL(CollectInput)
{
	HandleScope handle_scope(info.GetIsolate());
	switch_channel_t *channel;
	void *bp = NULL;
	int len = 0;
	int32_t abs_timeout = 0;
	int32_t digit_timeout = 0;
	switch_input_callback_function_t dtmf_func = NULL;
	FSInputCallbackState cb_state;
	switch_input_args_t args = { 0 };
	bool ret = true;

	METHOD_SANITY_CHECK();
	channel = switch_core_session_get_channel(this->_session);
	CHANNEL_SANITY_CHECK();
	CHANNEL_MEDIA_SANITY_CHECK();

	if (info.Length() > 0) {
		Handle<Function> func = JSBase::GetFunctionFromArg(info.GetIsolate(), info[0]);

		if (!func.IsEmpty()) {
			cb_state.function.Reset(info.GetIsolate(), func);

			if (info.Length() > 1 && !info[1].IsEmpty()) {
				cb_state.arg.Reset(info.GetIsolate(), info[1]);
			}

			cb_state.session_state = this;
			dtmf_func = CollectInputCallback;
			bp = &cb_state;
			len = sizeof(cb_state);
		}
	}

	if (info.Length() == 3) {
		abs_timeout = info[2]->Int32Value();
	} else if (info.Length() > 3) {
		digit_timeout = info[2]->Int32Value();
		abs_timeout = info[3]->Int32Value();
	}

	cb_state.ret.Reset(info.GetIsolate(), Boolean::New(info.GetIsolate(), false));
	args.input_callback = dtmf_func;
	args.buf = bp;
	args.buflen = len;

	switch_ivr_collect_digits_callback(this->_session, &args, digit_timeout, abs_timeout);
	info.GetReturnValue().Set(cb_state.ret);

	CheckHangupHook(this, &ret);
	if (!ret) JSMain::ExitScript(info.GetIsolate(), NULL);
}

JS_SESSION_FUNCTION_IMPL(SayPhrase)
{
	HandleScope handle_scope(info.GetIsolate());
	switch_channel_t *channel;
	string phrase_name;
	string phrase_data;
	string phrase_lang;
	string tmp;
	void *bp = NULL;
	int len = 0;
	switch_input_callback_function_t dtmf_func = NULL;
	FSInputCallbackState cb_state;
	switch_input_args_t args = { 0 };
	bool ret = true;

	METHOD_SANITY_CHECK();
	channel = switch_core_session_get_channel(this->_session);
	CHANNEL_SANITY_CHECK();
	CHANNEL_MEDIA_SANITY_CHECK();

	if (info.Length() > 0) {
		String::Utf8Value str(info[0]);
		phrase_name = js_safe_str(*str);
		if (zstr(phrase_name.c_str())) {
			info.GetIsolate()->ThrowException(String::NewFromUtf8(info.GetIsolate(), "Invalid phrase name"));
			return;
		}
	} else {
		info.GetIsolate()->ThrowException(String::NewFromUtf8(info.GetIsolate(), "Invalid arguments"));
		return;
	}

	if (info.Length() > 1) {
		String::Utf8Value str(info[1]);
		phrase_data = js_safe_str(*str);
	}

	if (info.Length() > 2) {
		String::Utf8Value str(info[2]);
		tmp = js_safe_str(*str);
		if (!zstr(tmp.c_str())) {
			phrase_lang = tmp;
		}
	}

	if (info.Length() > 3) {
		Handle<Function> func = JSBase::GetFunctionFromArg(info.GetIsolate(), info[3]);

		if (!func.IsEmpty()) {
			cb_state.function.Reset(info.GetIsolate(), func);

			if (info.Length() > 4 && !info[4].IsEmpty()) {
				cb_state.arg.Reset(info.GetIsolate(), info[4]);
			}

			cb_state.session_state = this;
			dtmf_func = CollectInputCallback;
			bp = &cb_state;
			len = sizeof(cb_state);
		}
	}

	cb_state.ret.Reset(info.GetIsolate(), Boolean::New(info.GetIsolate(), false));
	args.input_callback = dtmf_func;
	args.buf = bp;
	args.buflen = len;

	switch_ivr_phrase_macro(this->_session, phrase_name.c_str(), phrase_data.c_str(), phrase_lang.c_str(), &args);
	info.GetReturnValue().Set(cb_state.ret);

	CheckHangupHook(this, &ret);
	if (!ret) JSMain::ExitScript(info.GetIsolate(), NULL);
}

bool FSSession::CheckHangupHook(FSSession *obj, bool *ret)
{
	if (!obj) {
		return true;
	}

	Isolate *isolate = obj->GetIsolate();
	HandleScope handle_scope(isolate);
	Handle<Value> argv[2];
	int argc = 0;
	bool res = true;
	string resp;

	if (!obj->_check_state && !obj->_on_hangup.IsEmpty() && (obj->_hook_state == CS_HANGUP || obj->_hook_state == CS_ROUTING)) {
		obj->_check_state++;
		argv[argc++] = Local<Object>::New(obj->GetOwner()->GetIsolate(), obj->GetJavaScriptObject());

		if (obj->_hook_state == CS_HANGUP) {
			argv[argc++] = String::NewFromUtf8(isolate, "hangup");
		} else {
			argv[argc++] = String::NewFromUtf8(isolate, "transfer");
		}

		// Run the hangup hook
		Handle<Function> func = Local<Function>::New(isolate, obj->_on_hangup);

		if (!func.IsEmpty() && func->IsFunction()) {
			Handle<Value> res = func->Call(isolate->GetCurrentContext()->Global(), argc, argv);

			if (!res.IsEmpty()) {
				String::Utf8Value str(res);
				resp = js_safe_str(*str);
			}
		}

		if (resp.length() > 0) {
			res = !strcasecmp(resp.c_str(), "exit") ? false : true;
		}
	}

	if (ret) {
		*ret = res;
	}

	return res;
}

switch_status_t FSSession::HangupHook(switch_core_session_t *session)
{
	switch_channel_t *channel = switch_core_session_get_channel(session);
	switch_channel_state_t state = switch_channel_get_state(channel);
	FSSession *obj = NULL;

	if (state == CS_HANGUP || state == CS_ROUTING) {
		if ((obj = (FSSession *)switch_channel_get_private(channel, "jsobject"))) {
			obj->_hook_state = state;
			obj->_check_state = 0;
		}
	}

	return SWITCH_STATUS_SUCCESS;
}

JS_SESSION_FUNCTION_IMPL(SetHangupHook)
{
	HandleScope handle_scope(info.GetIsolate());
	info.GetReturnValue().Set(false);

	if (this->_session) {
		switch_channel_t *channel = switch_core_session_get_channel(this->_session);

		/* Reset hangup hook first */
		if (!this->_on_hangup.IsEmpty()) {
			switch_channel_set_private(channel, "jsobject", NULL);
			switch_core_event_hook_remove_state_change(this->_session, HangupHook);
			this->_on_hangup.Reset();
			this->_hook_state = switch_channel_get_state(channel);
		}

		if (info.Length() > 0) {
			Handle<Function> func = JSBase::GetFunctionFromArg(info.GetIsolate(), info[0]);

			if (!func.IsEmpty()) {
				this->_on_hangup.Reset(info.GetIsolate(), func);
				this->_hook_state = switch_channel_get_state(channel);
				switch_channel_set_private(channel, "jsobject", this);
				switch_core_event_hook_add_state_change(this->_session, HangupHook);
				info.GetReturnValue().Set(true);
			}
		}
	}
}

JS_SESSION_FUNCTION_IMPL(StreamFile)
{
	HandleScope handle_scope(info.GetIsolate());
	switch_channel_t *channel;
	string file_name;
	void *bp = NULL;
	int len = 0;
	switch_input_callback_function_t dtmf_func = NULL;
	FSInputCallbackState cb_state;
	switch_file_handle_t fh = { 0 };
	switch_input_args_t args = { 0 };
	const char *prebuf;
	char posbuf[35] = "";
	bool ret = true;

	METHOD_SANITY_CHECK();
	channel = switch_core_session_get_channel(this->_session);
	CHANNEL_SANITY_CHECK();
	CHANNEL_MEDIA_SANITY_CHECK();

	if (info.Length() > 0) {
		String::Utf8Value str(info[0]);
		file_name = js_safe_str(*str);
		if (zstr(file_name.c_str())) {
			info.GetIsolate()->ThrowException(String::NewFromUtf8(info.GetIsolate(), "Invalid filename"));
			return;
		}
	}

	if (info.Length() > 1) {
		Handle<Function> func = JSBase::GetFunctionFromArg(info.GetIsolate(), info[1]);

		if (!func.IsEmpty()) {
			cb_state.function.Reset(info.GetIsolate(), func);

			if (info.Length() > 2 && !info[2].IsEmpty()) {
				cb_state.arg.Reset(info.GetIsolate(), info[2]);
			}

			cb_state.session_state = this;
			dtmf_func = StreamInputCallback;
			bp = &cb_state;
			len = sizeof(cb_state);
		}
	}

	if (info.Length() > 3) {
		fh.samples = info[3]->Int32Value();
	}

	if ((prebuf = switch_channel_get_variable(channel, "stream_prebuffer"))) {
		int maybe = atoi(prebuf);
		if (maybe > 0) {
			fh.prebuf = maybe;
		}
	}

	cb_state.extra = &fh;
	cb_state.ret.Reset(info.GetIsolate(), Boolean::New(info.GetIsolate(), false));
	args.input_callback = dtmf_func;
	args.buf = bp;
	args.buflen = len;
	switch_ivr_play_file(this->_session, &fh, file_name.c_str(), &args);
	info.GetReturnValue().Set(cb_state.ret);

	switch_snprintf(posbuf, sizeof(posbuf), "%u", fh.offset_pos);
	switch_channel_set_variable(channel, "last_file_position", posbuf);

	CheckHangupHook(this, &ret);
	if (!ret) JSMain::ExitScript(info.GetIsolate(), NULL);
}

JS_SESSION_FUNCTION_IMPL(Sleep)
{
	HandleScope handle_scope(info.GetIsolate());
	switch_channel_t *channel;
	void *bp = NULL;
	int len = 0;
	switch_input_callback_function_t dtmf_func = NULL;
	FSInputCallbackState cb_state;
	switch_input_args_t args = { 0 };
	int32_t ms = 0;
	bool ret = true;
	int32_t sync = 0;

	METHOD_SANITY_CHECK();
	channel = switch_core_session_get_channel(this->_session);
	CHANNEL_SANITY_CHECK();
	CHANNEL_MEDIA_SANITY_CHECK();

	if (info.Length() > 0) {
		ms = info[0]->Int32Value();
	}

	if (ms <= 0) {
		return;
	}

	if (info.Length() > 1) {
		Handle<Function> func = JSBase::GetFunctionFromArg(info.GetIsolate(), info[1]);

		if (!func.IsEmpty()) {
			cb_state.function.Reset(info.GetIsolate(), func);

			if (info.Length() > 2 && !info[2].IsEmpty()) {
				cb_state.arg.Reset(info.GetIsolate(), info[2]);
			}

			cb_state.session_state = this;
			dtmf_func = CollectInputCallback;
			bp = &cb_state;
			len = sizeof(cb_state);
		}
	}

	if (info.Length() > 2) {
		sync = info[2]->Int32Value();
	}

	cb_state.ret.Reset(info.GetIsolate(), Boolean::New(info.GetIsolate(), false));
	args.input_callback = dtmf_func;
	args.buf = bp;
	args.buflen = len;
	switch_ivr_sleep(this->_session, ms, (switch_bool_t)sync, &args);
	info.GetReturnValue().Set(cb_state.ret);

	CheckHangupHook(this, &ret);
	if (!ret) JSMain::ExitScript(info.GetIsolate(), NULL);
}

JS_SESSION_FUNCTION_IMPL(SetVariable)
{
	HandleScope handle_scope(info.GetIsolate());
	switch_channel_t *channel;

	METHOD_SANITY_CHECK();
	channel = switch_core_session_get_channel(this->_session);

	if (info.Length() > 1) {
		const char *var, *val;
		String::Utf8Value str1(info[0]);
		String::Utf8Value str2(info[1]);

		var = js_safe_str(*str1);
		val = *str2;
		switch_channel_set_variable_var_check(channel, var, val, SWITCH_FALSE);
		info.GetReturnValue().Set(true);
	} else {
		info.GetReturnValue().Set(false);
	}
}

JS_SESSION_FUNCTION_IMPL(GetVariable)
{
	HandleScope handle_scope(info.GetIsolate());
	switch_channel_t *channel;

	METHOD_SANITY_CHECK();
	channel = switch_core_session_get_channel(this->_session);

	if (info.Length() > 0) {
		const char *var, *val;
		String::Utf8Value str(info[0]);

		var = js_safe_str(*str);
		val = switch_channel_get_variable(channel, var);

		info.GetReturnValue().Set(String::NewFromUtf8(info.GetIsolate(), js_safe_str(val)));
	} else {
		info.GetReturnValue().Set(false);
	}
}

switch_status_t FSSession::InitSpeechEngine(const char *engine, const char *voice)
{
	switch_codec_t *read_codec;
	switch_speech_flag_t flags = SWITCH_SPEECH_FLAG_NONE;
	uint32_t rate = 0;
	int interval = 0;

	read_codec = switch_core_session_get_read_codec(this->_session);
	rate = read_codec->implementation->actual_samples_per_second;
	interval = read_codec->implementation->microseconds_per_packet / 1000;

	if (switch_core_codec_init(&this->_speech->codec,
							   "L16",
							   NULL,
							   rate,
							   interval,
							   1, SWITCH_CODEC_FLAG_ENCODE | SWITCH_CODEC_FLAG_DECODE, NULL,
							   switch_core_session_get_pool(this->_session)) == SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Raw Codec Activation Success L16@%uhz 1 channel %dms\n", rate, interval);
	} else {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Raw Codec Activation Failed L16@%uhz 1 channel %dms\n", rate, interval);
		return SWITCH_STATUS_FALSE;
	}

	if (switch_core_speech_open(&this->_speech->sh, engine, voice, rate, interval, read_codec->implementation->number_of_channels,
								&flags, switch_core_session_get_pool(this->_session)) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Invalid TTS module!\n");
		switch_core_codec_destroy(&this->_speech->codec);
		return SWITCH_STATUS_FALSE;
	}

	return SWITCH_STATUS_SUCCESS;
}

JS_SESSION_FUNCTION_IMPL(Speak)
{
	HandleScope handle_scope(info.GetIsolate());
	switch_channel_t *channel;
	const char *tts_name = NULL;
	const char *voice_name = NULL;
	const char *text = NULL;
	void *bp = NULL;
	int len = 0;
	FSInputCallbackState cb_state;
	switch_input_callback_function_t dtmf_func = NULL;
	switch_input_args_t args = { 0 };
	bool ret = true;

	METHOD_SANITY_CHECK();
	info.GetReturnValue().Set(false);
	channel = switch_core_session_get_channel(this->_session);
	CHANNEL_SANITY_CHECK();
	CHANNEL_MEDIA_SANITY_CHECK();

	if (info.Length() < 3) {
		info.GetIsolate()->ThrowException(String::NewFromUtf8(info.GetIsolate(), "Invalid arguments"));
		return;
	}

	String::Utf8Value str1(info[0]);
	String::Utf8Value str2(info[1]);
	String::Utf8Value str3(info[2]);
	tts_name = js_safe_str(*str1);
	voice_name = js_safe_str(*str2);
	text = js_safe_str(*str3);

	if (zstr(tts_name)) {
		info.GetIsolate()->ThrowException(String::NewFromUtf8(info.GetIsolate(), "Invalid TTS Name"));
		return;
	}

	if (zstr(text)) {
		info.GetIsolate()->ThrowException(String::NewFromUtf8(info.GetIsolate(), "Invalid Text"));
		return;
	}

	if (this->_speech && this->_speech->speaking) {
		info.GetIsolate()->ThrowException(String::NewFromUtf8(info.GetIsolate(), "Recursive call not allowed"));
		return;
	}


	if (this->_speech && strcasecmp(this->_speech->sh.name, tts_name)) {
		DestroySpeechEngine();
	}

	if (this->_speech) {
		const char *s = "voice";
		switch_core_speech_text_param_tts(&this->_speech->sh, (char *)s, voice_name);
	} else {
		this->_speech = (js_session_speech_t *)switch_core_session_alloc(this->_session, sizeof(*this->_speech));
		switch_assert(this->_speech != NULL);
		if (InitSpeechEngine(tts_name, voice_name) != SWITCH_STATUS_SUCCESS) {
			this->_speech = NULL;
			info.GetIsolate()->ThrowException(String::NewFromUtf8(info.GetIsolate(), "Cannot allocate speech engine!"));
			return;
		}
	}

	if (info.Length() > 3) {
		Handle<Function> func = JSBase::GetFunctionFromArg(info.GetIsolate(), info[3]);

		if (!func.IsEmpty()) {
			cb_state.function.Reset(info.GetIsolate(), func);

			if (info.Length() > 4 && !info[4].IsEmpty()) {
				cb_state.arg.Reset(info.GetIsolate(), info[4]);
			}

			cb_state.session_state = this;
			dtmf_func = CollectInputCallback;
			bp = &cb_state;
			len = sizeof(cb_state);
		}
	}

	cb_state.ret.Reset(info.GetIsolate(), Boolean::New(info.GetIsolate(), false));
	args.input_callback = dtmf_func;
	args.buf = bp;
	args.buflen = len;

	switch_core_speech_flush_tts(&this->_speech->sh);
	if (switch_core_codec_ready(&this->_speech->codec)) {
		this->_speech->speaking = 1;
		switch_ivr_speak_text_handle(this->_session, &this->_speech->sh, &this->_speech->codec, NULL, (char *)text, &args);
		this->_speech->speaking = 0;
	}

	info.GetReturnValue().Set(cb_state.ret);

	CheckHangupHook(this, &ret);
	if (!ret) JSMain::ExitScript(info.GetIsolate(), NULL);
}

JS_SESSION_FUNCTION_IMPL(GetDigits)
{
	HandleScope handle_scope(info.GetIsolate());
	string terminators;
	char buf[513] = { 0 };
	int32_t digits = 0, timeout = 5000, digit_timeout = 0, abs_timeout = 0;
	switch_channel_t *channel;

	METHOD_SANITY_CHECK();
	channel = switch_core_session_get_channel(this->_session);
	CHANNEL_SANITY_CHECK();

	if (info.Length() > 0) {
		char term;
		digits = info[0]->Int32Value();

		if (digits > sizeof(buf) - 1) {
			char *err = switch_mprintf("Exceeded max digits of %" SWITCH_SIZE_T_FMT, sizeof(buf) - 1);
			info.GetIsolate()->ThrowException(String::NewFromUtf8(info.GetIsolate(), err));
			free(err);
			return;
		}

		if (info.Length() > 1) {
			String::Utf8Value str(info[1]);
			terminators = js_safe_str(*str);
		}

		if (info.Length() > 2) {
			timeout = info[2]->Int32Value();
		}

		if (info.Length() > 3) {
			digit_timeout = info[3]->Int32Value();
		}

		if (info.Length() > 4) {
			abs_timeout = info[4]->Int32Value();
		}

		switch_ivr_collect_digits_count(this->_session, buf, sizeof(buf), digits, terminators.c_str(), &term, timeout, digit_timeout, abs_timeout);
		info.GetReturnValue().Set(String::NewFromUtf8(info.GetIsolate(), buf));
	}
}

JS_SESSION_FUNCTION_IMPL(SetAutoHangup)
{
	HandleScope handle_scope(info.GetIsolate());
	info.GetReturnValue().Set(false);

	METHOD_SANITY_CHECK();

	if (info.Length() > 0) {
		bool tf = info[0]->BooleanValue();
		if (tf) {
			switch_set_flag(this, S_HUP);
		} else {
			switch_clear_flag(this, S_HUP);
		}
		info.GetReturnValue().Set(tf);
	}
}

JS_SESSION_FUNCTION_IMPL(Answer)
{
	HandleScope handle_scope(info.GetIsolate());
	switch_channel_t *channel;

	METHOD_SANITY_CHECK();
	channel = switch_core_session_get_channel(this->_session);
	CHANNEL_SANITY_CHECK_ANSWER();

	switch_channel_answer(channel);

	info.GetReturnValue().Set(true);
}

JS_SESSION_FUNCTION_IMPL(PreAnswer)
{
	HandleScope handle_scope(info.GetIsolate());
	switch_channel_t *channel;

	METHOD_SANITY_CHECK();
	channel = switch_core_session_get_channel(this->_session);
	CHANNEL_SANITY_CHECK_ANSWER();

	switch_channel_pre_answer(channel);

	info.GetReturnValue().Set(true);
}

JS_SESSION_FUNCTION_IMPL(GenerateXmlCdr)
{
	HandleScope handle_scope(info.GetIsolate());
	switch_xml_t cdr = NULL;

	info.GetReturnValue().Set(false);

	if (switch_ivr_generate_xml_cdr(this->_session, &cdr) == SWITCH_STATUS_SUCCESS) {
		char *xml_text;
		if ((xml_text = switch_xml_toxml(cdr, SWITCH_FALSE))) {
			info.GetReturnValue().Set(String::NewFromUtf8(info.GetIsolate(), xml_text));
		}
		switch_safe_free(xml_text);
		switch_xml_free(cdr);
	}
}

JS_SESSION_FUNCTION_IMPL(Ready)
{
	HandleScope handle_scope(info.GetIsolate());

	info.GetReturnValue().Set((this && this->_session && switch_channel_ready(switch_core_session_get_channel(this->_session))) ? true : false);
}

JS_SESSION_FUNCTION_IMPL(MediaReady)
{
	HandleScope handle_scope(info.GetIsolate());

	info.GetReturnValue().Set((this && this->_session && switch_channel_media_ready(switch_core_session_get_channel(this->_session))) ? true : false);
}

JS_SESSION_FUNCTION_IMPL(RingReady)
{
	HandleScope handle_scope(info.GetIsolate());

	info.GetReturnValue().Set((this && this->_session && switch_channel_test_flag(switch_core_session_get_channel(this->_session), CF_RING_READY)) ? true : false);
}

JS_SESSION_FUNCTION_IMPL(Answered)
{
	HandleScope handle_scope(info.GetIsolate());

	info.GetReturnValue().Set((this && this->_session && switch_channel_test_flag(switch_core_session_get_channel(this->_session), CF_ANSWERED)) ? true : false);
}

JS_SESSION_FUNCTION_IMPL(WaitForMedia)
{
	HandleScope handle_scope(info.GetIsolate());
	switch_channel_t *channel;
	switch_time_t started;
	unsigned int elapsed;
	int32_t timeout = 60000;
	bool ret = true;

	METHOD_SANITY_CHECK();
	channel = switch_core_session_get_channel(this->_session);
	CHANNEL_MEDIA_SANITY_CHECK();

	started = switch_micro_time_now();

	if (info.Length() > 0) {
		timeout = info[0]->Int32Value();
		if (timeout < 1000) {
			timeout = 1000;
		}
	}

	if (!CheckHangupHook(this, NULL)) {
		JSMain::ExitScript(info.GetIsolate(), NULL);
		return;
	}

	for (;;) {
		if (((elapsed = (unsigned int) ((switch_micro_time_now() - started) / 1000)) > (switch_time_t) timeout)
			|| switch_channel_down(channel)) {
			info.GetReturnValue().Set(false);
			break;
		}

		if (switch_channel_ready(channel)
			&& (switch_channel_test_flag(channel, CF_ANSWERED) || switch_channel_test_flag(channel, CF_EARLY_MEDIA))) {
			info.GetReturnValue().Set(true);
			break;
		}

		switch_cond_next();
	}

	CheckHangupHook(this, &ret);
	if (!ret) JSMain::ExitScript(info.GetIsolate(), NULL);
}

JS_SESSION_FUNCTION_IMPL(WaitForAnswer)
{
	HandleScope handle_scope(info.GetIsolate());
	switch_channel_t *channel;
	switch_time_t started;
	unsigned int elapsed;
	int32_t timeout = 60000;
	bool ret = true;

	METHOD_SANITY_CHECK();
	channel = switch_core_session_get_channel(this->_session);

	started = switch_micro_time_now();

	if (info.Length() > 0) {
		timeout = info[0]->Int32Value();
		if (timeout < 1000) {
			timeout = 1000;
		}
	}

	if (!CheckHangupHook(this, NULL)) {
		JSMain::ExitScript(info.GetIsolate(), NULL);
		return;
	}

	for (;;) {
		if (((elapsed = (unsigned int) ((switch_micro_time_now() - started) / 1000)) > (switch_time_t) timeout)
			|| switch_channel_down(channel)) {
			info.GetReturnValue().Set(false);
			break;
		}

		if (switch_channel_ready(channel) && switch_channel_test_flag(channel, CF_ANSWERED)) {
			info.GetReturnValue().Set(true);
			break;
		}

		switch_cond_next();
	}

	CheckHangupHook(this, &ret);
	if (!ret) JSMain::ExitScript(info.GetIsolate(), NULL);
}

JS_SESSION_FUNCTION_IMPL(Detach)
{
	HandleScope handle_scope(info.GetIsolate());
	switch_call_cause_t cause = SWITCH_CAUSE_NONE;
	switch_channel_t *channel;
	switch_core_session_t *session;

	METHOD_SANITY_CHECK();

	if ((session = this->_session)) {
		this->_session = NULL;

		if (info.Length() > 0) {
			if (info[0]->IsInt32()) {
				int32_t i = 0;
				i = info[0]->Int32Value();
				cause = (switch_call_cause_t)i;
			} else {
				String::Utf8Value str(info[0]);
				const char *cause_name = js_safe_str(*str);
				cause = switch_channel_str2cause(cause_name);
			}
		}

		if (cause != SWITCH_CAUSE_NONE) {
			channel = switch_core_session_get_channel(session);
			switch_channel_hangup(channel, cause);
		}

		switch_core_session_rwunlock(session);
		info.GetReturnValue().Set(true);
	} else {
		info.GetReturnValue().Set(false);
	}
}

JS_SESSION_FUNCTION_IMPL(Execute)
{
	HandleScope handle_scope(info.GetIsolate());
	bool retval = false;
	bool ret = true;

	METHOD_SANITY_CHECK();

	/* you can execute some apps before you answer  CHANNEL_SANITY_CHECK(); */

	if (info.Length() > 0) {
		switch_application_interface_t *application_interface;
		String::Utf8Value str(info[0]);
		const char *app_name = js_safe_str(*str);
		string app_arg;

		METHOD_SANITY_CHECK();

		if (info.Length() > 1) {
			String::Utf8Value str2(info[1]);
			app_arg = js_safe_str(*str2);
		}

		if ((application_interface = switch_loadable_module_get_application_interface(app_name))) {
			if (application_interface->application_function) {
				if (!CheckHangupHook(this, NULL)) {
					JSMain::ExitScript(info.GetIsolate(), NULL);
					UNPROTECT_INTERFACE(application_interface);
					return;
				}

				switch_core_session_exec(this->_session, application_interface, app_arg.c_str());
				CheckHangupHook(this, &ret);
				retval = true;
			}
			UNPROTECT_INTERFACE(application_interface);
		}
	}

	info.GetReturnValue().Set(retval);
	if (!ret) JSMain::ExitScript(info.GetIsolate(), NULL);
}

JS_SESSION_FUNCTION_IMPL(GetEvent)
{
	HandleScope handle_scope(info.GetIsolate());
	switch_event_t *event;

	METHOD_SANITY_CHECK();

	if (switch_core_session_dequeue_event(this->_session, &event, SWITCH_FALSE) == SWITCH_STATUS_SUCCESS) {
		Handle<Object> Event;
		FSEvent *evt;

		if ((evt = new FSEvent(info))) {
			evt->SetEvent(event, 0);
			evt->RegisterInstance(info.GetIsolate(), "", true);
			Event = evt->GetJavaScriptObject();

			if (!Event.IsEmpty()) {
				info.GetReturnValue().Set(Event);
				return;
			}
		}
	}

	info.GetReturnValue().Set(false);
}

JS_SESSION_FUNCTION_IMPL(SendEvent)
{
	HandleScope handle_scope(info.GetIsolate());
	Handle<Object> Event;
	FSEvent *eo;

	METHOD_SANITY_CHECK();

	if (info.Length() > 0 && info[0]->IsObject()) {
		Handle<Object> jso = Handle<Object>::Cast(info[0]);

		if (!jso.IsEmpty()) {
			switch_event_t **evt;

			if ((eo = JSBase::GetInstance<FSEvent>(jso)) && (evt = eo->GetEvent())) {
				if (switch_core_session_receive_event(this->_session, evt) != SWITCH_STATUS_SUCCESS) {
					info.GetReturnValue().Set(false);
					return;
				}

				delete eo;
			}
		}
	}

	info.GetReturnValue().Set(true);
}

JS_SESSION_FUNCTION_IMPL(Hangup)
{
	HandleScope handle_scope(info.GetIsolate());
	switch_channel_t *channel;
	switch_call_cause_t cause = SWITCH_CAUSE_NORMAL_CLEARING;

	METHOD_SANITY_CHECK();
	channel = switch_core_session_get_channel(this->_session);

	if (switch_channel_up(channel)) {
		if (info.Length() > 0) {
			if (info[0]->IsInt32()) {
				int32_t i = info[0]->Int32Value();
				cause = (switch_call_cause_t)i;
			} else {
				String::Utf8Value str(info[0]);
				const char *cause_name = js_safe_str(*str);
				cause = switch_channel_str2cause(cause_name);
			}
		}

		switch_channel_hangup(channel, cause);
		switch_core_session_kill_channel(this->_session, SWITCH_SIG_KILL);

		this->_hook_state = CS_HANGUP;
		CheckHangupHook(this, NULL);

		info.GetReturnValue().Set(true);
	} else {
		info.GetReturnValue().Set(false);
	}
}

JS_SESSION_GET_PROPERTY_IMPL(GetProperty)
{
	HandleScope handle_scope(info.GetIsolate());
	switch_channel_t *channel = NULL;
	switch_caller_profile_t *caller_profile = NULL;

	if (this->_session) {
		channel = switch_core_session_get_channel(this->_session);
		caller_profile = switch_channel_get_caller_profile(channel);
	}

	String::Utf8Value str(property);
	const char *prop = js_safe_str(*str);

	if (!strcmp(prop, "cause")) {
		if (channel) {
			info.GetReturnValue().Set(String::NewFromUtf8(info.GetIsolate(), switch_channel_cause2str(switch_channel_get_cause(channel))));
		} else {
			info.GetReturnValue().Set(String::NewFromUtf8(info.GetIsolate(), switch_channel_cause2str(this->_cause)));
		}
	} else if (!strcmp(prop, "causecode")) {
		if (channel) {
			info.GetReturnValue().Set(Integer::New(info.GetIsolate(), switch_channel_get_cause(channel)));
		} else {
			info.GetReturnValue().Set(Integer::New(info.GetIsolate(), this->_cause));
		}
	} else if (!strcmp(prop, "name")) {
		info.GetReturnValue().Set(String::NewFromUtf8(info.GetIsolate(), switch_channel_get_name(channel)));
	} else if (!strcmp(prop, "uuid")) {
		info.GetReturnValue().Set(String::NewFromUtf8(info.GetIsolate(), switch_channel_get_uuid(channel)));
	} else if (!strcmp(prop, "state")) {
		info.GetReturnValue().Set(String::NewFromUtf8(info.GetIsolate(), switch_channel_state_name(switch_channel_get_state(channel))));
	} else if (!strcmp(prop, "dialplan")) {
		if (caller_profile) {
			info.GetReturnValue().Set(String::NewFromUtf8(info.GetIsolate(), caller_profile->dialplan));
		} else {
			info.GetReturnValue().Set(String::NewFromUtf8(info.GetIsolate(), ""));
		}
	} else if (!strcmp(prop, "caller_id_name")) {
		if (caller_profile) {
			info.GetReturnValue().Set(String::NewFromUtf8(info.GetIsolate(), caller_profile->caller_id_name));
		} else {
			info.GetReturnValue().Set(String::NewFromUtf8(info.GetIsolate(), ""));
		}
	} else if (!strcmp(prop, "caller_id_num") || !strcmp(prop, "caller_id_number")) {
		if (caller_profile) {
			info.GetReturnValue().Set(String::NewFromUtf8(info.GetIsolate(), caller_profile->caller_id_number));
		} else {
			info.GetReturnValue().Set(String::NewFromUtf8(info.GetIsolate(), ""));
		}
	} else if (!strcmp(prop, "network_addr") || !strcasecmp(prop, "network_address")) {
		if (caller_profile) {
			info.GetReturnValue().Set(String::NewFromUtf8(info.GetIsolate(), caller_profile->network_addr));
		} else {
			info.GetReturnValue().Set(String::NewFromUtf8(info.GetIsolate(), ""));
		}
	} else if (!strcmp(prop, "ani")) {
		if (caller_profile) {
			info.GetReturnValue().Set(String::NewFromUtf8(info.GetIsolate(), caller_profile->ani));
		} else {
			info.GetReturnValue().Set(String::NewFromUtf8(info.GetIsolate(), ""));
		}
	} else if (!strcmp(prop, "aniii")) {
		if (caller_profile) {
			info.GetReturnValue().Set(String::NewFromUtf8(info.GetIsolate(), caller_profile->aniii));
		} else {
			info.GetReturnValue().Set(String::NewFromUtf8(info.GetIsolate(), ""));
		}
	} else if (!strcmp(prop, "destination")) {
		if (caller_profile) {
			info.GetReturnValue().Set(String::NewFromUtf8(info.GetIsolate(), caller_profile->destination_number));
		} else {
			info.GetReturnValue().Set(String::NewFromUtf8(info.GetIsolate(), ""));
		}
	} else {
		info.GetIsolate()->ThrowException(String::NewFromUtf8(info.GetIsolate(), "Bad property"));
	}
}

void *FSSession::Construct(const v8::FunctionCallbackInfo<Value>& info)
{
	HandleScope handle_scope(info.GetIsolate());
	FSSession *session_obj = new FSSession(info);

	switch_assert(session_obj);

	if (info.Length() > 0) {
		String::Utf8Value str(info[0]);
		const char *uuid = js_safe_str(*str);

		if (!strchr(uuid, '/')) {
			session_obj->_session = switch_core_session_locate(uuid);
			switch_set_flag(session_obj, S_HUP);
		} else {
			FSSession *old_obj = NULL;

			if (info.Length() > 1 && info[1]->IsObject()) {
				old_obj = JSBase::GetInstance<FSSession>(Handle<Object>::Cast(info[1]));
			}
			if (switch_ivr_originate(old_obj ? old_obj->_session : NULL,
									 &session_obj->_session, &session_obj->_cause, uuid, 60, NULL, NULL, NULL, NULL, NULL, SOF_NONE, NULL) == SWITCH_STATUS_SUCCESS) {
				switch_set_flag(session_obj, S_HUP);
			} else {
				/* This will return the Session object, but with no C++ instance related to it */
				/* After each call to [new Session("/chan/test")] you should check the property originateCause, which will hold a value if origination failed */
				info.This()->Set(String::NewFromUtf8(info.GetIsolate(), "originateCause"), String::NewFromUtf8(info.GetIsolate(), switch_channel_cause2str(session_obj->_cause)));
				delete session_obj;
				return NULL;
			}
		}
	}

	return session_obj;
}

JS_SESSION_FUNCTION_IMPL(SetCallerdata)
{
	HandleScope handle_scope(info.GetIsolate());
	info.GetReturnValue().Set(false);

	if (info.Length() > 1) {
		const char *var, *val;
		char **toset = NULL;
		String::Utf8Value str1(info[0]);
		String::Utf8Value str2(info[1]);
		var = js_safe_str(*str1);
		val = js_safe_str(*str2);

		if (!strcasecmp(var, "dialplan")) {
			toset = &this->_dialplan;
		} else if (!strcasecmp(var, "username")) {
			toset = &this->_username;
		} else if (!strcasecmp(var, "caller_id_name")) {
			toset = &this->_caller_id_name;
		} else if (!strcasecmp(var, "ani")) {
			toset = &this->_ani;
		} else if (!strcasecmp(var, "aniii")) {
			toset = &this->_aniii;
		} else if (!strcasecmp(var, "caller_id_number")) {
			toset = &this->_caller_id_number;
		} else if (!strcasecmp(var, "network_addr")) {
			toset = &this->_network_addr;
		} else if (!strcasecmp(var, "rdnis")) {
			toset = &this->_rdnis;
		} else if (!strcasecmp(var, "destination_number")) {
			toset = &this->_destination_number;
		} else if (!strcasecmp(var, "context")) {
			toset = &this->_context;
		}

		if (toset) {
			switch_safe_free(*toset);
			*toset = strdup(val);
			info.GetReturnValue().Set(true);
		}
	}
}

JS_SESSION_FUNCTION_IMPL(Originate)
{
	HandleScope handle_scope(info.GetIsolate());
	switch_memory_pool_t *pool = NULL;

	this->_cause = SWITCH_CAUSE_DESTINATION_OUT_OF_ORDER;

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "This method is deprecated, please use new Session(\"<dial string>\", a_leg) \n");

	if (this->_session) {
		info.GetIsolate()->ThrowException(String::NewFromUtf8(info.GetIsolate(), "cannot call this method on an initialized session"));
		return;
	}

	if (info.Length() > 1) {
		Handle<Object> session_obj;
		switch_core_session_t *session = NULL, *peer_session = NULL;
		switch_caller_profile_t *caller_profile = NULL, *orig_caller_profile = NULL;
		string dest;
		const char *dialplan = NULL;
		const char *cid_name = "";
		const char *cid_num = "";
		const char *network_addr = "";
		const char *ani = "";
		const char *aniii = "";
		const char *rdnis = "";
		const char *context = "";
		const char *username = NULL;
		string to;
		char *tmp;
		switch_status_t status;

		info.GetReturnValue().Set(false);

		if (info[0]->IsObject()) {
			session_obj = Handle<Object>::Cast(info[0]);
			FSSession *old_obj = NULL;
			if (!session_obj.IsEmpty() && (old_obj = JSBase::GetInstance<FSSession>(session_obj))) {

				if (old_obj == this) {
					info.GetIsolate()->ThrowException(String::NewFromUtf8(info.GetIsolate(), "Supplied a_leg session is the same as our session"));
					return;
				}

				if (!old_obj->_session) {
					info.GetIsolate()->ThrowException(String::NewFromUtf8(info.GetIsolate(), "Supplied a_leg session is not initilaized!"));
					return;
				}

				session = old_obj->_session;
				orig_caller_profile = switch_channel_get_caller_profile(switch_core_session_get_channel(session));
				dialplan = orig_caller_profile->dialplan;
				cid_name = orig_caller_profile->caller_id_name;
				cid_num = orig_caller_profile->caller_id_number;
				ani = orig_caller_profile->ani;
				aniii = orig_caller_profile->aniii;
				rdnis = orig_caller_profile->rdnis;
				context = orig_caller_profile->context;
				username = orig_caller_profile->username;
			}
		}

		if (!zstr(this->_dialplan) && zstr(dialplan))
			dialplan = this->_dialplan;
		if (!zstr(this->_caller_id_name) && zstr(cid_name))
			cid_name = this->_caller_id_name;
		if (!zstr(this->_caller_id_number) && zstr(cid_num))
			cid_num = this->_caller_id_number;
		if (!zstr(this->_ani) && zstr(ani))
			ani = this->_ani;
		if (!zstr(this->_aniii) && zstr(aniii))
			aniii = this->_aniii;
		if (!zstr(this->_rdnis) && zstr(rdnis))
			rdnis = this->_rdnis;
		if (!zstr(this->_context) && zstr(context))
			context = this->_context;
		if (!zstr(this->_username) && zstr(username))
			username = this->_username;

		String::Utf8Value str(info[1]);
		dest = js_safe_str(*str);

		if (!dest.c_str() || !strchr(dest.c_str(), '/')) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "Invalid Channel String\n");
			return;
		}

		if (info.Length() > 2) {
			String::Utf8Value strTmp(info[2]);
			tmp = *strTmp;
			if (!zstr(tmp)) {
				to = tmp;
			}
		}

		if (switch_core_new_memory_pool(&pool) != SWITCH_STATUS_SUCCESS) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "OH OH no pool\n");
			info.GetIsolate()->ThrowException(String::NewFromUtf8(info.GetIsolate(), "Could not create new pool"));
			return;
		}

		caller_profile = switch_caller_profile_new(pool, username, dialplan, cid_name, cid_num, network_addr, ani, aniii, rdnis, "mod_v8", context, dest.c_str());

		status =
			switch_ivr_originate(session, &peer_session, &this->_cause, dest.c_str(), to.length() > 0 ? atoi(to.c_str()) : 60, NULL, NULL, NULL, caller_profile, NULL, SOF_NONE, NULL);

		if (status != SWITCH_STATUS_SUCCESS) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "Cannot Create Outgoing Channel! [%s]\n", dest.c_str());
			return;
		}

		this->_session = peer_session;
		switch_set_flag(this, S_HUP);
		info.GetReturnValue().Set(true);
	} else {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "Missing Args\n");
	}
}

JS_SESSION_FUNCTION_IMPL(Bridge)
{
	HandleScope handle_scope(info.GetIsolate());
	FSSession *jss_b = NULL;
	Handle<Object> obj_b;
	void *bp = NULL;
	switch_input_callback_function_t dtmf_func = NULL;
	FSInputCallbackState cb_state;

	info.GetReturnValue().Set(false);

	if (info.Length() > 0) {
		if (info[0]->IsObject()) {
			obj_b = Handle<Object>::Cast(info[0]);
			
			if (!(jss_b = JSBase::GetInstance<FSSession>(obj_b))) {
				info.GetIsolate()->ThrowException(String::NewFromUtf8(info.GetIsolate(), "Cannot find session B"));
				return;
			}
		}
	}

	if (!_session) {
		info.GetIsolate()->ThrowException(String::NewFromUtf8(info.GetIsolate(), "session A is not ready!"));
		return;
	}

	if (!(jss_b && jss_b->_session)) {
		info.GetIsolate()->ThrowException(String::NewFromUtf8(info.GetIsolate(), "session B is not ready!"));
		return;
	}

	if (info.Length() > 1) {
		Handle<Function> func = JSBase::GetFunctionFromArg(info.GetIsolate(), info[1]);

		if (!func.IsEmpty()) {
			cb_state.function.Reset(info.GetIsolate(), func);

			if (info.Length() > 2 && !info[2].IsEmpty()) {
				cb_state.arg.Reset(info.GetIsolate(), info[2]);
			}

			cb_state.jss_a = this;
			cb_state.jss_b = jss_b;
			cb_state.session_obj_a.Reset(info.GetIsolate(), info.Holder());
			cb_state.session_obj_b.Reset(info.GetIsolate(), obj_b);
			cb_state.session_state = this;

			dtmf_func = CollectInputCallback;
			bp = &cb_state;
		}
	}

	switch_ivr_multi_threaded_bridge(_session, jss_b->_session, dtmf_func, bp, bp);

	info.GetReturnValue().Set(true);
}

static const js_function_t session_proc[] = {
	{"originate", FSSession::Originate},				// Deprecated - use constructor instead
	{"setCallerData", FSSession::SetCallerdata},
	{"setHangupHook", FSSession::SetHangupHook},
	{"setAutoHangup", FSSession::SetAutoHangup},
	{"sayPhrase", FSSession::SayPhrase},
	{"streamFile", FSSession::StreamFile},
	{"collectInput", FSSession::CollectInput},
	{"recordFile", FSSession::RecordFile},
	{"flushEvents", FSSession::FlushEvents},
	{"flushDigits", FSSession::FlushDigits},
	{"speak", FSSession::Speak},
	{"setVariable", FSSession::SetVariable},
	{"getVariable", FSSession::GetVariable},
	{"getDigits", FSSession::GetDigits},
	{"answer", FSSession::Answer},
	{"preAnswer", FSSession::PreAnswer},
	{"generateXmlCdr", FSSession::GenerateXmlCdr},
	{"ready", FSSession::Ready},
	{"answered", FSSession::Answered},
	{"mediaReady", FSSession::MediaReady},
	{"ringReady", FSSession::RingReady},
	{"waitForAnswer", FSSession::WaitForAnswer},	// Deprecated
	{"waitForMedia", FSSession::WaitForMedia},		// Deprecated
	{"getEvent", FSSession::GetEvent},
	{"sendEvent", FSSession::SendEvent},
	{"hangup", FSSession::Hangup},
	{"execute", FSSession::Execute},
	{"destroy", FSSession::Detach},
	{"sleep", FSSession::Sleep},
	{"bridge", FSSession::Bridge},
	{0}
};

static const js_property_t session_prop[] = {
	{"name", FSSession::GetProperty, JSBase::DefaultSetProperty},
	{"state", FSSession::GetProperty, JSBase::DefaultSetProperty},
	{"dialplan", FSSession::GetProperty, JSBase::DefaultSetProperty},
	{"caller_id_name", FSSession::GetProperty, JSBase::DefaultSetProperty},
	{"caller_id_num", FSSession::GetProperty, JSBase::DefaultSetProperty},
	{"caller_id_number", FSSession::GetProperty, JSBase::DefaultSetProperty},
	{"network_addr", FSSession::GetProperty, JSBase::DefaultSetProperty},
	{"network_address", FSSession::GetProperty, JSBase::DefaultSetProperty},
	{"ani", FSSession::GetProperty, JSBase::DefaultSetProperty},
	{"aniii", FSSession::GetProperty, JSBase::DefaultSetProperty},
	{"destination", FSSession::GetProperty, JSBase::DefaultSetProperty},
	{"uuid", FSSession::GetProperty, JSBase::DefaultSetProperty},
	{"cause", FSSession::GetProperty, JSBase::DefaultSetProperty},
	{"causecode", FSSession::GetProperty, JSBase::DefaultSetProperty},
	{0}
};

static const js_class_definition_t session_desc = {
	js_class_name,
	FSSession::Construct,
	session_proc,
	session_prop
};

const js_class_definition_t *FSSession::GetClassDefinition()
{
	return &session_desc;
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
