/* 
 * FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
 * Copyright (C) 2005-2014, Anthony Minessale II <anthm@freeswitch.org>
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
#include <switch_curl.h>


static const char modname[] = "CURL";

struct curl_obj {
	switch_CURL *curl_handle;
	JSContext *cx;
	JSObject *obj;
	JSFunction *function;
	JSObject *user_data;
	jsrefcount saveDepth;
	jsval ret;
};


static size_t file_callback(void *ptr, size_t size, size_t nmemb, void *data)
{
	register unsigned int realsize = (unsigned int) (size * nmemb);
	struct curl_obj *co = data;
	uintN argc = 0;
	jsval argv[4];


	if (!co) {
		return 0;
	}
	if (co->function) {
		char *ret;
		argv[argc++] = STRING_TO_JSVAL(JS_NewStringCopyN(co->cx, (char *) ptr, realsize));
		if (co->user_data) {
			argv[argc++] = OBJECT_TO_JSVAL(co->user_data);
		}
		JS_ResumeRequest(co->cx, co->saveDepth);
		JS_CallFunction(co->cx, co->obj, co->function, argc, argv, &co->ret);
		co->saveDepth = JS_SuspendRequest(co->cx);

		if ((ret = JS_GetStringBytes(JS_ValueToString(co->cx, co->ret)))) {
			if (!strcmp(ret, "true") || !strcmp(ret, "undefined")) {
				return realsize;
			} else {
				return 0;
			}
		}
	}

	return realsize;
}


/* Curl Object */
/*********************************************************************************/
static JSBool curl_construct(JSContext * cx, JSObject * obj, uintN argc, jsval * argv, jsval * rval)
{
	struct curl_obj *co = NULL;

	co = malloc(sizeof(*co));
	switch_assert(co);

	memset(co, 0, sizeof(*co));

	co->cx = cx;
	co->obj = obj;

	JS_SetPrivate(cx, obj, co);

	return JS_TRUE;
}

static void curl_destroy(JSContext * cx, JSObject * obj)
{
	struct curl_obj *co = JS_GetPrivate(cx, obj);
	switch_safe_free(co);
	JS_SetPrivate(cx, obj, NULL);
}

static JSBool curl_run(JSContext * cx, JSObject * obj, uintN argc, jsval * argv, jsval * rval)
{
	struct curl_obj *co = JS_GetPrivate(cx, obj);
	char *method = NULL, *url, *cred = NULL;
	char *url_p = NULL, *data = NULL, *durl = NULL;
	long httpRes = 0;
	struct curl_slist *headers = NULL;
	int32 timeout = 0;
	char ct[80] = "Content-Type: application/x-www-form-urlencoded";

	if (argc < 2 || !co) {
		return JS_FALSE;
	}


	method = JS_GetStringBytes(JS_ValueToString(cx, argv[0]));
	url = JS_GetStringBytes(JS_ValueToString(cx, argv[1]));

	co->curl_handle = switch_curl_easy_init();
	if (!strncasecmp(url, "https", 5)) {
		switch_curl_easy_setopt(co->curl_handle, CURLOPT_SSL_VERIFYPEER, 0);
		switch_curl_easy_setopt(co->curl_handle, CURLOPT_SSL_VERIFYHOST, 0);
	}


	if (argc > 2) {
		data = JS_GetStringBytes(JS_ValueToString(cx, argv[2]));
	}

	if (argc > 3) {
		co->function = JS_ValueToFunction(cx, argv[3]);
	}

	if (argc > 4) {
		JS_ValueToObject(cx, argv[4], &co->user_data);
	}

	if (argc > 5) {
		cred = JS_GetStringBytes(JS_ValueToString(cx, argv[5]));
		if (!zstr(cred)) {
			switch_curl_easy_setopt(co->curl_handle, CURLOPT_HTTPAUTH, (long)CURLAUTH_ANY);
			switch_curl_easy_setopt(co->curl_handle, CURLOPT_USERPWD, cred);
		}
	}

	if (argc > 6) {
		JS_ValueToInt32(cx, argv[6], &timeout);
		if (timeout > 0) {
			switch_curl_easy_setopt(co->curl_handle, CURLOPT_TIMEOUT, timeout);
		}
	}

	if (argc > 7) {
		char *content_type = JS_GetStringBytes(JS_ValueToString(cx, argv[7]));
		switch_snprintf(ct, sizeof(ct), "Content-Type: %s", content_type);
	}

	headers = curl_slist_append(headers, ct);

	switch_curl_easy_setopt(co->curl_handle, CURLOPT_HTTPHEADER, headers);

	url_p = url;

	if (!strcasecmp(method, "post")) {
		switch_curl_easy_setopt(co->curl_handle, CURLOPT_POST, 1);
		if (!data) {
			data = "";
		}
		switch_curl_easy_setopt(co->curl_handle, CURLOPT_POSTFIELDS, data);
	} else if (!zstr(data)) {
		durl = switch_mprintf("%s?%s", url, data);
		url_p = durl;
	}

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Running: method: [%s] url: [%s] data: [%s] cred=[%s] cb: [%s]\n",
					  method, url_p, data, switch_str_nil(cred), co->function ? "yes" : "no");

	switch_curl_easy_setopt(co->curl_handle, CURLOPT_URL, url_p);
	switch_curl_easy_setopt(co->curl_handle, CURLOPT_NOSIGNAL, 1);
	switch_curl_easy_setopt(co->curl_handle, CURLOPT_WRITEFUNCTION, file_callback);
	switch_curl_easy_setopt(co->curl_handle, CURLOPT_WRITEDATA, (void *) co);

	switch_curl_easy_setopt(co->curl_handle, CURLOPT_USERAGENT, "freeswitch-spidermonkey-curl/1.0");

	co->saveDepth = JS_SuspendRequest(cx);
	switch_curl_easy_perform(co->curl_handle);

	switch_curl_easy_getinfo(co->curl_handle, CURLINFO_RESPONSE_CODE, &httpRes);
	switch_curl_easy_cleanup(co->curl_handle);
	curl_slist_free_all(headers);
	co->curl_handle = NULL;
	co->function = NULL;
	JS_ResumeRequest(cx, co->saveDepth);
	switch_safe_free(durl);


	return JS_TRUE;
}

static JSFunctionSpec curl_methods[] = {
	{"run", curl_run, 2},
	{0}
};


static JSPropertySpec curl_props[] = {
	{0}
};

static JSBool curl_getProperty(JSContext * cx, JSObject * obj, jsval id, jsval * vp)
{
	JSBool res = JS_TRUE;
	return res;
}

JSClass curl_class = {
	modname, JSCLASS_HAS_PRIVATE,
	JS_PropertyStub, JS_PropertyStub, curl_getProperty, DEFAULT_SET_PROPERTY,
	JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub, curl_destroy, NULL, NULL, NULL,
	curl_construct
};


switch_status_t curl_load(JSContext * cx, JSObject * obj)
{
	JS_InitClass(cx, obj, NULL, &curl_class, curl_construct, 3, curl_props, curl_methods, curl_props, curl_methods);
	return SWITCH_STATUS_SUCCESS;
}


const sm_module_interface_t curl_module_interface = {
	/*.name = */ modname,
	/*.spidermonkey_load */ curl_load,
	/*.next */ NULL
};

SWITCH_MOD_DECLARE_NONSTD(switch_status_t) spidermonkey_init(const sm_module_interface_t ** module_interface)
{
	*module_interface = &curl_module_interface;
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
