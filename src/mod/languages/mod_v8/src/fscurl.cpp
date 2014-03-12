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
 * fscurl.cpp -- JavaScript CURL class
 *
 */

#include "fscurl.hpp"

using namespace std;
using namespace v8;

static const char js_class_name[] = "CURL";

FSCURL::~FSCURL(void)
{
	_function.Reset();
	_user_data.Reset();
	_ret.Reset();

	if (_curl_handle) {
		switch_curl_easy_cleanup(_curl_handle);
	}
}

void FSCURL::Init(void)
{
	_curl_handle = NULL;
}

string FSCURL::GetJSClassName()
{
	return js_class_name;
}

size_t FSCURL::FileCallback(void *ptr, size_t size, size_t nmemb, void *data)
{
	FSCURL *obj = static_cast<FSCURL *>(data);
	register unsigned int realsize = (unsigned int) (size * nmemb);
	uint32_t argc = 0;
	Handle<Value> argv[4];

	if (!obj) {
		return 0;
	}

	HandleScope handle_scope(obj->GetIsolate());
	Handle<Function> func;
	
	if (!obj->_function.IsEmpty()) {
		func = Local<Function>::New(obj->GetIsolate(), obj->_function);
	}

	if (!func.IsEmpty()) {
		char *ret;
		if (ptr) {
			argv[argc++] = String::NewFromUtf8(obj->GetIsolate(), (char *)ptr);
		} else {
			argv[argc++] = String::NewFromUtf8(obj->GetIsolate(), "");
		}
		if (!obj->_user_data.IsEmpty()) {
			argv[argc++] = Local<Value>::New(obj->GetIsolate(), Persistent<Value>::Cast(obj->_user_data));
		}

		Handle<Value> res = func->Call(obj->GetIsolate()->GetCurrentContext()->Global(), argc, argv);

		if (!res.IsEmpty()){
			obj->_ret.Reset(obj->GetIsolate(), res);
		} else {
			obj->_ret.Reset();
		}

		String::Utf8Value str(Local<Value>::New(obj->GetIsolate(), res));

		if ((ret = *str)) {
			if (!strcmp(ret, "true") || !strcmp(ret, "undefined")) {
				return realsize;
			} else {
				return 0;
			}
		}
	}

	return realsize;
}

void *FSCURL::Construct(const v8::FunctionCallbackInfo<Value>& info)
{
	FSCURL *obj = new FSCURL(info);

	switch_assert(obj);
	return obj;
}

JS_CURL_FUNCTION_IMPL(Run)
{
	HandleScope handle_scope(info.GetIsolate());
	const char *method, *url;
	const char *url_p = NULL;
	char *durl = NULL;
	string data, cred;
	long httpRes = 0;
	struct curl_slist *headers = NULL;
	int32_t timeout = 0;
	char ct[80] = "Content-Type: application/x-www-form-urlencoded";

	if (info.Length() < 2) {
		info.GetIsolate()->ThrowException(String::NewFromUtf8(info.GetIsolate(), "Invalid arguments"));
		return;
	}

	String::Utf8Value str1(info[0]);
	String::Utf8Value str2(info[1]);

	method = js_safe_str(*str1);
	url = js_safe_str(*str2);

	_curl_handle = switch_curl_easy_init();
	if (!strncasecmp(url, "https", 5)) {
		switch_curl_easy_setopt(_curl_handle, CURLOPT_SSL_VERIFYPEER, 0);
		switch_curl_easy_setopt(_curl_handle, CURLOPT_SSL_VERIFYHOST, 0);
	}

	if (info.Length() > 2) {
		String::Utf8Value str3(info[2]);
		data = js_safe_str(*str3);
	}

	if (info.Length() > 3) {
		Handle<Function> func = JSBase::GetFunctionFromArg(info.GetIsolate(), info[3]);
		if (!func.IsEmpty() && func->IsFunction()) {
			_function.Reset(info.GetIsolate(), func);
		}
	}

	if (info.Length() > 4) {
		_user_data.Reset(info.GetIsolate(), Handle<Object>::Cast(info[4]));
	}

	if (info.Length() > 5) {
		String::Utf8Value str4(info[5]);
		cred = js_safe_str(*str4);
		if (cred.length() > 0) {
			switch_curl_easy_setopt(_curl_handle, CURLOPT_HTTPAUTH, (long)CURLAUTH_ANY);
			switch_curl_easy_setopt(_curl_handle, CURLOPT_USERPWD, cred.c_str());
		}
	}

	if (info.Length() > 6) {
		timeout = info[6]->Int32Value();
		if (timeout > 0) {
			switch_curl_easy_setopt(_curl_handle, CURLOPT_TIMEOUT, timeout);
		}
	}

	if (info.Length() > 7) {
		String::Utf8Value str5(info[7]);
		const char *content_type = js_safe_str(*str5);
		switch_snprintf(ct, sizeof(ct), "Content-Type: %s", content_type);
	}

	headers = curl_slist_append(headers, ct);

	switch_curl_easy_setopt(_curl_handle, CURLOPT_HTTPHEADER, headers);

	url_p = url;

	if (!strcasecmp(method, "post")) {
		switch_curl_easy_setopt(_curl_handle, CURLOPT_POST, 1);
		if (!data.c_str()) {
			data = "";
		}
		switch_curl_easy_setopt(_curl_handle, CURLOPT_POSTFIELDS, data.c_str());
	} else if (data.length() > 0) {
		durl = switch_mprintf("%s?%s", url, data.c_str());
		url_p = durl;
	}

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Running: method: [%s] url: [%s] data: [%s] cred=[%s] cb: [%s]\n",
		method, url_p, data.c_str(), switch_str_nil(cred.c_str()), !_function.IsEmpty() ? "yes" : "no");

	switch_curl_easy_setopt(_curl_handle, CURLOPT_URL, url_p);
	switch_curl_easy_setopt(_curl_handle, CURLOPT_NOSIGNAL, 1);
	switch_curl_easy_setopt(_curl_handle, CURLOPT_WRITEFUNCTION, FSCURL::FileCallback);
	switch_curl_easy_setopt(_curl_handle, CURLOPT_WRITEDATA, (void *) this);

	switch_curl_easy_setopt(_curl_handle, CURLOPT_USERAGENT, "freeswitch-v8-curl/1.0");

	switch_curl_easy_perform(_curl_handle);

	switch_curl_easy_getinfo(_curl_handle, CURLINFO_RESPONSE_CODE, &httpRes);
	switch_curl_easy_cleanup(_curl_handle);
	curl_slist_free_all(headers);
	_curl_handle = NULL;
	_function.Reset();
	_user_data.Reset();
	switch_safe_free(durl);

	if (!_ret.IsEmpty()) {
		info.GetReturnValue().Set(_ret);
		_ret.Reset();
	}
}

static const js_function_t curl_methods[] = {
	{"run", FSCURL::Run},
	{0}
};

static const js_property_t curl_props[] = {
	{0}
};

static const js_class_definition_t curl_desc = {
	js_class_name,
	FSCURL::Construct,
	curl_methods,
	curl_props
};

static switch_status_t curl_load(const v8::FunctionCallbackInfo<Value>& info)
{
	JSBase::Register(info.GetIsolate(), &curl_desc);
	return SWITCH_STATUS_SUCCESS;
}

static const v8_mod_interface_t curl_module_interface = {
	/*.name = */ js_class_name,
	/*.js_mod_load */ curl_load
};

const v8_mod_interface_t *FSCURL::GetModuleInterface()
{
	return &curl_module_interface;
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
