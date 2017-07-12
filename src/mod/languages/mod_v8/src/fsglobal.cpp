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
 * fsglobal.cpp -- JavaScript Global functions
 *
 */

#include "mod_v8.h"
#include "fsglobal.hpp"
#include "fssession.hpp"
#include <switch_curl.h>

using namespace std;
using namespace v8;

class CURLCallbackData
{
public:
	Isolate *isolate;
	Persistent<Array> hashObject;
	int fileHandle;
	switch_size_t bufferSize;
	switch_size_t bufferDataLength;
	char *buffer;

	CURLCallbackData() {
		isolate = NULL;
		fileHandle = 0;
		bufferSize = 0;
		bufferDataLength = 0;
		buffer = NULL;
	}

	~CURLCallbackData() {
		hashObject.Reset();
	}
};

size_t FSGlobal::HashCallback(void *ptr, size_t size, size_t nmemb, void *data)
{
	register size_t realsize = size * nmemb;
	char *line, lineb[2048], *nextline = NULL, *val = NULL, *p = NULL;
	CURLCallbackData *config_data = (CURLCallbackData *)data;

	if (!config_data->hashObject.IsEmpty()) {
		switch_copy_string(lineb, (char *) ptr, sizeof(lineb));
		line = lineb;

		HandleScope scope(config_data->isolate);

		/* Get the array object to store data in */
		Local<Array> args = Local<Array>::New(config_data->isolate, config_data->hashObject);

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
				for (p = line + strlen(line) - 1; *p == ' '; p--)
					*p = '\0';
				for (p = val; p && *p == ' '; p++);
				val = p;
				for (p = val + strlen(val) - 1; *p == ' '; p--)
					*p = '\0';

				// Add data to hash
				args->Set(String::NewFromUtf8(config_data->isolate, line), String::NewFromUtf8(config_data->isolate, js_safe_str(val)));
			}

			line = nextline;
		}
	}

	return realsize;
}

size_t FSGlobal::FileCallback(void *ptr, size_t size, size_t nmemb, void *data)
{
	register unsigned int realsize = (unsigned int) (size * nmemb);
	CURLCallbackData *config_data = (CURLCallbackData *)data;

	if ((write(config_data->fileHandle, ptr, realsize) != (int) realsize)) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Unable to write all bytes!\n");
	}

	return realsize;
}

size_t FSGlobal::FetchUrlCallback(void *ptr, size_t size, size_t nmemb, void *data)
{
	register unsigned int realsize = (unsigned int) (size * nmemb);
	CURLCallbackData *config_data = (CURLCallbackData *)data;

	/* Too much data. Do not increase buffer, but abort fetch instead. */
	if (config_data->bufferDataLength + realsize >= config_data->bufferSize) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Data do not fit in the allocated buffer\n");
		return 0;
	}

	memcpy(config_data->buffer + config_data->bufferDataLength, ptr, realsize);
	config_data->bufferDataLength += realsize;
	config_data->buffer[config_data->bufferDataLength] = 0;

	return realsize;
}

JS_GLOBAL_FUNCTION_IMPL_STATIC(FetchURLHash)
{
	JS_CHECK_SCRIPT_STATE();
	HandleScope handle_scope(info.GetIsolate());
	switch_CURL *curl_handle = NULL;
	CURLCallbackData config_data;
	int saveDepth = 0;

	if (info.Length() > 0) {
		String::Utf8Value url(info[0]);

		if (info.Length() > 1 && info[1]->IsString()) {
			/* Cast to string */
			Local<String> str = Local<String>::Cast(info[1]);

			/* Try to get existing variable */
			Local<Value> obj = info.GetIsolate()->GetCurrentContext()->Global()->Get(str);

			if (!obj.IsEmpty() && obj->IsArray()) {
				/* The existing var is an array, use it */
				config_data.hashObject.Reset(info.GetIsolate(), Local<Array>::Cast(obj));
			} else if (obj.IsEmpty() || obj->IsUndefined()) {
				/* No existing var (or an existing that is undefined), create a new one */
				Local<Array> arguments = Array::New(info.GetIsolate());
				info.GetIsolate()->GetCurrentContext()->Global()->Set(str, arguments);
				config_data.hashObject.Reset(info.GetIsolate(), arguments);
			} else {
				/* The var exists, but is wrong type - exit with error */
				info.GetIsolate()->ThrowException(String::NewFromUtf8(info.GetIsolate(), "Second argument is the name of an existing var of the wrong type"));
				return;
			}
		} else if (info.Length() > 1 && info[1]->IsArray()) {
			/* The var is an array, use it */
			config_data.hashObject.Reset(info.GetIsolate(), Local<Array>::Cast(info[1]));
		} else if (info.Length() > 1) {
			/* The var exists, but is wrong type - exit with error */
			info.GetIsolate()->ThrowException(String::NewFromUtf8(info.GetIsolate(), "Second argument is of the wrong type"));
			return;
		} else {
			/* Second argument doesn't exist, this is also ok. The hash will be returned as the result */
			Local<Array> arguments = Array::New(info.GetIsolate());
			config_data.hashObject.Reset(info.GetIsolate(), arguments);
		}

		curl_handle = switch_curl_easy_init();

		if (!strncasecmp(js_safe_str(*url), "https", 5)) {
			switch_curl_easy_setopt(curl_handle, CURLOPT_SSL_VERIFYPEER, 0);
			switch_curl_easy_setopt(curl_handle, CURLOPT_SSL_VERIFYHOST, 0);
		}

		config_data.isolate = info.GetIsolate();

		switch_curl_easy_setopt(curl_handle, CURLOPT_URL, js_safe_str(*url));
		switch_curl_easy_setopt(curl_handle, CURLOPT_FOLLOWLOCATION, 1);
		switch_curl_easy_setopt(curl_handle, CURLOPT_NOSIGNAL, 1);
		switch_curl_easy_setopt(curl_handle, CURLOPT_MAXREDIRS, 10);
		switch_curl_easy_setopt(curl_handle, CURLOPT_WRITEFUNCTION, FSGlobal::HashCallback);
		switch_curl_easy_setopt(curl_handle, CURLOPT_WRITEDATA, (void *) &config_data);
		switch_curl_easy_setopt(curl_handle, CURLOPT_USERAGENT, "freeswitch-v8/1.0");

		switch_curl_easy_perform(curl_handle);

		switch_curl_easy_cleanup(curl_handle);

		/* Return the hash */
		info.GetReturnValue().Set(config_data.hashObject);
	} else {
		info.GetIsolate()->ThrowException(String::NewFromUtf8(info.GetIsolate(), "Invalid arguments"));
	}
}

JS_GLOBAL_FUNCTION_IMPL_STATIC(FetchURLFile)
{
	JS_CHECK_SCRIPT_STATE();
	HandleScope handle_scope(info.GetIsolate());
	switch_CURL *curl_handle = NULL;
	CURLCallbackData config_data;
	int saveDepth = 0;

	if (info.Length() > 1) {
		const char *url = NULL, *filename = NULL;
		String::Utf8Value str1(info[0]);
		String::Utf8Value str2(info[1]);
		url = js_safe_str(*str1);
		filename = js_safe_str(*str2);

		curl_handle = switch_curl_easy_init();
		if (!strncasecmp(url, "https", 5)) {
			switch_curl_easy_setopt(curl_handle, CURLOPT_SSL_VERIFYPEER, 0);
			switch_curl_easy_setopt(curl_handle, CURLOPT_SSL_VERIFYHOST, 0);
		}
		config_data.isolate = info.GetIsolate();

		if ((config_data.fileHandle = open(filename, O_CREAT | O_RDWR | O_TRUNC, S_IRUSR | S_IWUSR)) > -1) {
			switch_curl_easy_setopt(curl_handle, CURLOPT_URL, url);
			switch_curl_easy_setopt(curl_handle, CURLOPT_FOLLOWLOCATION, 1);
			switch_curl_easy_setopt(curl_handle, CURLOPT_NOSIGNAL, 1);
			switch_curl_easy_setopt(curl_handle, CURLOPT_MAXREDIRS, 10);
			switch_curl_easy_setopt(curl_handle, CURLOPT_WRITEFUNCTION, FSGlobal::FileCallback);
			switch_curl_easy_setopt(curl_handle, CURLOPT_WRITEDATA, (void *) &config_data);
			switch_curl_easy_setopt(curl_handle, CURLOPT_USERAGENT, "freeswitch-v8/1.0");

			switch_curl_easy_perform(curl_handle);

			switch_curl_easy_cleanup(curl_handle);
			close(config_data.fileHandle);
			info.GetReturnValue().Set(true);
		} else {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Failed to open file [%s]\n", filename);
			info.GetReturnValue().Set(false);
		}
	} else {
		info.GetIsolate()->ThrowException(String::NewFromUtf8(info.GetIsolate(), "Invalid arguments"));
	}
}

JS_GLOBAL_FUNCTION_IMPL_STATIC(FetchURL)
{
	JS_CHECK_SCRIPT_STATE();
	HandleScope handle_scope(info.GetIsolate());
	switch_CURL *curl_handle = NULL;
	CURLCallbackData config_data;
	int32_t buffer_size = 65535;
	switch_CURLcode code = (switch_CURLcode)0;
	int saveDepth = 0;

	if (info.Length() >= 1) {
		const char *url;
		String::Utf8Value str(info[0]);
		url = js_safe_str(*str);
		if (info.Length() > 1) {
			buffer_size = info[1]->Int32Value();
		}

		curl_handle = switch_curl_easy_init();
		if (!strncasecmp(url, "https", 5)) {
			switch_curl_easy_setopt(curl_handle, CURLOPT_SSL_VERIFYPEER, 0);
			switch_curl_easy_setopt(curl_handle, CURLOPT_SSL_VERIFYHOST, 0);
		}

		config_data.isolate = info.GetIsolate();
		config_data.bufferSize = buffer_size;
		config_data.buffer = (char *)malloc(config_data.bufferSize);
		config_data.bufferDataLength = 0;

		if (config_data.buffer == NULL) {
			info.GetIsolate()->ThrowException(String::NewFromUtf8(info.GetIsolate(), "Failed to allocate data buffer."));
			switch_curl_easy_cleanup(curl_handle);
			return;
		}

		switch_curl_easy_setopt(curl_handle, CURLOPT_URL, url);
		switch_curl_easy_setopt(curl_handle, CURLOPT_FOLLOWLOCATION, 1);
		switch_curl_easy_setopt(curl_handle, CURLOPT_NOSIGNAL, 1);
		switch_curl_easy_setopt(curl_handle, CURLOPT_MAXREDIRS, 10);
		switch_curl_easy_setopt(curl_handle, CURLOPT_WRITEFUNCTION, FSGlobal::FetchUrlCallback);
		switch_curl_easy_setopt(curl_handle, CURLOPT_WRITEDATA, (void *) &config_data);
		switch_curl_easy_setopt(curl_handle, CURLOPT_USERAGENT, "freeswitch-v8/1.0");

		code = switch_curl_easy_perform(curl_handle);

		switch_curl_easy_cleanup(curl_handle);

		if (code == CURLE_OK) {
			config_data.buffer[config_data.bufferDataLength] = 0;
			info.GetReturnValue().Set(String::NewFromUtf8(info.GetIsolate(), js_safe_str(config_data.buffer)));
		} else {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Curl returned error %u\n", (unsigned) code);
			info.GetReturnValue().Set(false);
		}

		free(config_data.buffer);
	} else {
		info.GetIsolate()->ThrowException(String::NewFromUtf8(info.GetIsolate(), "Invalid arguments"));
	}
}

JS_GLOBAL_FUNCTION_IMPL_STATIC(Exit)
{
	JS_CHECK_SCRIPT_STATE();

	if (info.Length() > 0) {
		HandleScope handle_scope(info.GetIsolate());
		String::Utf8Value str(info[0]);

		if (*str) {
			JSMain::ExitScript(info.GetIsolate(), *str);
			return;
		}
	}

	JSMain::ExitScript(info.GetIsolate(), NULL);
}

JS_GLOBAL_FUNCTION_IMPL_STATIC(Log)
{
	JS_CHECK_SCRIPT_STATE();
	HandleScope handle_scope(info.GetIsolate());
	char *level_str, *msg;
	switch_log_level_t level = SWITCH_LOG_DEBUG;
	char *file = NULL;
	int line = 0;

	if (info.Length() > 0) {
		/* Get filename and line number from stack */
		file = JSMain::GetStackInfo(info.GetIsolate(), &line);
	}

	if (info.Length() > 1) {
		String::Utf8Value str(info[0]);

		if ((level_str = *str)) {
			level = switch_log_str2level(level_str);
			if (level == SWITCH_LOG_INVALID) {
				level = SWITCH_LOG_DEBUG;
			}
		}

		String::Utf8Value str2(info[1]);
		if ((msg = *str2) && *msg != '\0') {
			const char lastchar = msg[strlen(msg)-1];
			switch_log_printf(SWITCH_CHANNEL_ID_LOG, file, "console_log", line, NULL, level, "%s%s", msg, lastchar != '\n' ? "\n" : "");
			switch_safe_free(file);
			return;
		}
	} else if (info.Length() > 0) {
		String::Utf8Value str(info[0]);
		if ((msg = *str) && *msg != '\0') {
			const char lastchar = msg[strlen(msg)-1];
			switch_log_printf(SWITCH_CHANNEL_ID_LOG, file, "console_log", line, NULL, level, "%s%s", msg, lastchar != '\n' ? "\n" : "");
			switch_safe_free(file);
			return;
		}
	}

	switch_safe_free(file);
}

JS_GLOBAL_FUNCTION_IMPL_STATIC(GlobalSet)
{
	JS_CHECK_SCRIPT_STATE();
	HandleScope handle_scope(info.GetIsolate());
	const char *var_name = NULL, *val = NULL, *val2 = NULL;
	bool tf = true;

	if (info.Length() > 1) {
		String::Utf8Value str1(info[0]);
		String::Utf8Value str2(info[1]);
		var_name = js_safe_str(*str1);
		val = js_safe_str(*str2);
		if (info.Length() == 2) {
			switch_core_set_variable(var_name, val);
			info.GetReturnValue().Set(true);
			return;
		} else {
			String::Utf8Value str3(info[2]);
			val2 = js_safe_str(*str3);
			if (switch_core_set_var_conditional(var_name, val, val2) != SWITCH_TRUE) {
				tf = false;
			}
			info.GetReturnValue().Set(tf);
			return;
		}
	}

	info.GetIsolate()->ThrowException(String::NewFromUtf8(info.GetIsolate(), "var name not supplied!"));
}

JS_GLOBAL_FUNCTION_IMPL_STATIC(GlobalGet)
{
	JS_CHECK_SCRIPT_STATE();
	HandleScope handle_scope(info.GetIsolate());
	const char *var_name = NULL;
	char *val = NULL;

	if (info.Length() > 0) {
		String::Utf8Value str(info[0]);
		var_name = js_safe_str(*str);
		val = switch_core_get_variable_dup(var_name);
		info.GetReturnValue().Set(String::NewFromUtf8(info.GetIsolate(), js_safe_str(val)));
		switch_safe_free(val);
		return;
	}

	info.GetIsolate()->ThrowException(String::NewFromUtf8(info.GetIsolate(), "var name not supplied!"));
}

JS_GLOBAL_FUNCTION_IMPL_STATIC(Include)
{
	JS_CHECK_SCRIPT_STATE();
	if (info.Length() < 1) {
		/* Bad arguments, return exception */
		info.GetIsolate()->ThrowException(String::NewFromUtf8(info.GetIsolate(), "Invalid arguments"));
	} else {
		/* Try to get the current script path */
		string scriptPath = "";
		Handle<Value> val = info.GetIsolate()->GetCurrentContext()->Global()->Get(String::NewFromUtf8(info.GetIsolate(), "scriptPath"));
		if (!val.IsEmpty() && val->IsString()) {
			String::Utf8Value tmp(val);
			if (*tmp) {
				scriptPath = *tmp;
			}
		}

		/* Loop all arguments until we find a valid file */
		for (int i = 0; i < info.Length(); i++) {
			HandleScope handle_scope(info.GetIsolate());
			String::Utf8Value str(info[i]);
			char *path = NULL;
			const char *script_name = NULL;

			if (*str) {
				if (switch_is_file_path(*str)) {
					script_name = *str;
				} else {
					/* First try to use the same path as the executing script */
					if (scriptPath.length() > 0) {
						path = switch_mprintf("%s%s%s", scriptPath.c_str(), SWITCH_PATH_SEPARATOR, *str);
						if (path && JSMain::FileExists(path)) {
							script_name = path;
						} else {
							switch_safe_free(path);
						}
					}

					/* At last, use FS script directory */
					if (!script_name && (path = switch_mprintf("%s%s%s", SWITCH_GLOBAL_dirs.script_dir, SWITCH_PATH_SEPARATOR, *str))) {
						script_name = path;
					}
				}
			}

			if (script_name && JSMain::FileExists(script_name)) {
				string js_file = JSMain::LoadFileToString(script_name);

				if (js_file.length() > 0) {
#if defined(V8_MAJOR_VERSION) && V8_MAJOR_VERSION >=5
					MaybeLocal<v8::Script> script;
					LoadScript(&script, info.GetIsolate(), js_file.c_str(), script_name);
					
					if (script.IsEmpty()) {
						info.GetReturnValue().Set(false);
					} else {
						info.GetReturnValue().Set(script.ToLocalChecked()->Run());
					}
#else
					Handle<String> source = String::NewFromUtf8(info.GetIsolate(), js_file.c_str());
					Handle<Script> script = Script::Compile(source, info[i]);
					info.GetReturnValue().Set(script->Run());
#endif
					switch_safe_free(path);
					return;
				}
			}
			switch_safe_free(path);
		}

		info.GetIsolate()->ThrowException(String::NewFromUtf8(info.GetIsolate(), "Failed to include javascript file"));
	}
}

JS_GLOBAL_FUNCTION_IMPL_STATIC(Sleep)
{
	JS_CHECK_SCRIPT_STATE();
	HandleScope handle_scope(info.GetIsolate());
	int32_t msec = 0;

	if (info.Length() > 0) {
		msec = info[0]->Int32Value();
	}

	if (msec) {
		switch_yield(msec * 1000);
	} else {
		info.GetIsolate()->ThrowException(String::NewFromUtf8(info.GetIsolate(), "No time specified"));
	}
}

JS_GLOBAL_FUNCTION_IMPL_STATIC(Use)
{
	JS_CHECK_SCRIPT_STATE();
	HandleScope handle_scope(info.GetIsolate());

	if (info.Length() > 0) {
		String::Utf8Value str(info[0]);
		char *mod_name = *str;

		if (mod_name) {
			const v8_mod_interface_t *mp;

			if ((mp = (v8_mod_interface_t *)switch_core_hash_find(module_manager.load_hash, mod_name))) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Loading %s\n", mod_name);
				mp->v8_mod_load(info);
			} else {
				char *err = switch_mprintf("Error loading %s", mod_name);
				info.GetIsolate()->ThrowException(String::NewFromUtf8(info.GetIsolate(), err));
				free(err);
			}
		}
	} else {
		info.GetIsolate()->ThrowException(String::NewFromUtf8(info.GetIsolate(), "Invalid arguments"));
	}
}

JS_GLOBAL_FUNCTION_IMPL_STATIC(ApiExecute)
{
	JS_CHECK_SCRIPT_STATE();
	HandleScope handle_scope(info.GetIsolate());

	if (info.Length() > 0) {
		String::Utf8Value str(info[0]);
		const char *cmd = js_safe_str(*str);
		string arg;
		switch_core_session_t *session = NULL;
		switch_stream_handle_t stream = { 0 };

		if (!strcasecmp(cmd, "jsapi")) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Invalid API Call!\n");
			info.GetReturnValue().Set(false);
			return;
		}

		if (info.Length() > 1) {
			String::Utf8Value str2(info[1]);
			arg = js_safe_str(*str2);
		}

		if (info.Length() > 2) {
			if (!info[2].IsEmpty() && info[2]->IsObject()) {
				Handle<Object> session_obj = Handle<Object>::Cast(info[2]);
				FSSession *obj = JSBase::GetInstance<FSSession>(session_obj);
				if (obj) {
					session = obj->GetSession();
				}
			}
		}

		SWITCH_STANDARD_STREAM(stream);
		switch_api_execute(cmd, arg.c_str(), session, &stream);

		info.GetReturnValue().Set(String::NewFromUtf8(info.GetIsolate(), switch_str_nil((char *) stream.data)));
		switch_safe_free(stream.data);
	} else {
		info.GetReturnValue().Set(String::NewFromUtf8(info.GetIsolate(), ""));
	}
}

JS_GLOBAL_FUNCTION_IMPL_STATIC(Email)
{
	JS_CHECK_SCRIPT_STATE();
	HandleScope handle_scope(info.GetIsolate());
	string to, from, headers, body, file, convert_cmd, convert_ext;

	if (info.Length() > 0) {
		String::Utf8Value str(info[0]);
		to = js_safe_str(*str);
	}

	if (info.Length() > 1) {
		String::Utf8Value str(info[1]);
		from = js_safe_str(*str);
	}

	if (info.Length() > 2) {
		String::Utf8Value str(info[2]);
		headers = js_safe_str(*str);
	}

	if (info.Length() > 3) {
		String::Utf8Value str(info[3]);
		body = js_safe_str(*str);
	}

	if (info.Length() > 4) {
		String::Utf8Value str(info[4]);
		file = js_safe_str(*str);
	}

	if (info.Length() > 5) {
		String::Utf8Value str(info[5]);
		convert_cmd = js_safe_str(*str);
	}

	if (info.Length() > 6) {
		String::Utf8Value str(info[6]);
		convert_ext = js_safe_str(*str);
	}

	if (to.c_str() && from.c_str() && headers.c_str() && body.c_str() && switch_simple_email(to.c_str(), from.c_str(), headers.c_str(), body.c_str(), file.c_str(), convert_cmd.c_str(), convert_ext.c_str()) == SWITCH_TRUE) {
		info.GetReturnValue().Set(true);
		return;
    }

	info.GetReturnValue().Set(false);
}

JS_GLOBAL_FUNCTION_IMPL_STATIC(Bridge)
{
	JS_CHECK_SCRIPT_STATE();
	HandleScope handle_scope(info.GetIsolate());
	FSSession *jss_a = NULL, *jss_b = NULL;
	Handle<Object> obj_a;
	Handle<Object> obj_b;
	void *bp = NULL;
	switch_input_callback_function_t dtmf_func = NULL;
	FSInputCallbackState cb_state;

	info.GetReturnValue().Set(false);

	if (info.Length() > 1) {
		if (info[0]->IsObject()) {
			obj_a = Handle<Object>::Cast(info[0]);

			if (!(jss_a = JSBase::GetInstance<FSSession>(obj_a))) {
				info.GetIsolate()->ThrowException(String::NewFromUtf8(info.GetIsolate(), "Cannot find session A"));
				return;
			}
		}
		if (info[1]->IsObject()) {
			obj_b = Handle<Object>::Cast(info[1]);
			if (!(jss_b = JSBase::GetInstance<FSSession>(obj_b))) {
				info.GetIsolate()->ThrowException(String::NewFromUtf8(info.GetIsolate(), "Cannot find session B"));
				return;
			}
		}
	}

	if (!(jss_a && jss_a->GetSession())) {
		info.GetIsolate()->ThrowException(String::NewFromUtf8(info.GetIsolate(), "session A is not ready!"));
		return;
	}

	if (!(jss_b && jss_b->GetSession())) {
		info.GetIsolate()->ThrowException(String::NewFromUtf8(info.GetIsolate(), "session B is not ready!"));
		return;
	}

	if (info.Length() > 2) {
		Handle<Function> func = JSBase::GetFunctionFromArg(info.GetIsolate(), info[2]);

		if (!func.IsEmpty()) {
			cb_state.function.Reset(info.GetIsolate(), func);

			if (info.Length() > 3 && !info[3].IsEmpty()) {
				cb_state.arg.Reset(info.GetIsolate(), info[3]);
			}

			cb_state.jss_a = jss_a;
			cb_state.jss_b = jss_b;
			cb_state.session_obj_a.Reset(info.GetIsolate(), obj_a);
			cb_state.session_obj_b.Reset(info.GetIsolate(), obj_b);
			cb_state.session_state = jss_a;
			cb_state.context.Reset(info.GetIsolate(), info.GetIsolate()->GetCurrentContext());
			dtmf_func = FSSession::CollectInputCallback;
			bp = &cb_state;
		}
	}

	JS_EXECUTE_LONG_RUNNING_C_CALL_WITH_UNLOCKER(switch_ivr_multi_threaded_bridge(jss_a->GetSession(), jss_b->GetSession(), dtmf_func, bp, bp));

	info.GetReturnValue().Set(true);
}

/* Replace this with more robust version later */
JS_GLOBAL_FUNCTION_IMPL_STATIC(System)
{
	JS_CHECK_SCRIPT_STATE();
	HandleScope handle_scope(info.GetIsolate());
	char *cmd;
	int result;
	info.GetReturnValue().Set(false);

	if (info.Length() > 0) {
		String::Utf8Value str(info[0]);
		cmd = *str;
		if (cmd) {
			result = switch_system(cmd, SWITCH_TRUE);
			info.GetReturnValue().Set(Integer::New(info.GetIsolate(), result));
			return;
		}
	}

	info.GetIsolate()->ThrowException(String::NewFromUtf8(info.GetIsolate(), "Invalid arguments"));
}

JS_GLOBAL_FUNCTION_IMPL_STATIC(FileDelete)
{
	JS_CHECK_SCRIPT_STATE();
	HandleScope handle_scope(info.GetIsolate());
	const char *path;

	info.GetReturnValue().Set(false);

	if (info.Length() > 0) {
		String::Utf8Value str(info[0]);
		path = *str;
		if (path) {
			if ((switch_file_remove(path, NULL)) == SWITCH_STATUS_SUCCESS) {
				info.GetReturnValue().Set(true);
			}
			return;
		}
	}

	info.GetIsolate()->ThrowException(String::NewFromUtf8(info.GetIsolate(), "Invalid arguments"));
}

static const js_function_t fs_proc[] = {
	{"console_log", FSGlobal::Log},				// Deprecated
	{"consoleLog", FSGlobal::Log},
	{"log", FSGlobal::Log},						// Short version of 'consoleLog()'
	{"getGlobalVariable", FSGlobal::GlobalGet},
	{"setGlobalVariable", FSGlobal::GlobalSet},
	{"exit", FSGlobal::Exit},
	{"include", FSGlobal::Include},
	{"require", FSGlobal::Include},				// Extra version of 'include()'
	{"bridge", FSGlobal::Bridge},
	{"email", FSGlobal::Email},
	{"apiExecute", FSGlobal::ApiExecute},
	{"use", FSGlobal::Use},
	{"msleep", FSGlobal::Sleep},
	{"fileDelete", FSGlobal::FileDelete},
	{"system", FSGlobal::System},
	{"fetchURL", FSGlobal::FetchURL},			// Deprecated
	{"fetchURLHash", FSGlobal::FetchURLHash},	// Deprecated
	{"fetchURLFile", FSGlobal::FetchURLFile},	// Deprecated
	{"fetchUrl", FSGlobal::FetchURL},
	{"fetchUrlHash", FSGlobal::FetchURLHash},
	{"fetchUrlFile", FSGlobal::FetchURLFile},
	{0}
};

const js_function_t *FSGlobal::GetFunctionDefinitions()
{
	return fs_proc;
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
