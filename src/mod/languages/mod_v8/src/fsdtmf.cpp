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
 * fsdtmf.cpp -- JavaScript DTMF class
 *
 */

#include "fsdtmf.hpp"

using namespace std;
using namespace v8;

static const char js_class_name[] = "DTMF";

FSDTMF::~FSDTMF(void)
{
	switch_safe_free(_dtmf);
}

string FSDTMF::GetJSClassName()
{
	return js_class_name;
}

Handle<Object> FSDTMF::New(switch_dtmf_t *dtmf, const char *name, JSMain *js)
{
	FSDTMF *obj;
	switch_dtmf_t *ddtmf;

	if ((obj = new FSDTMF(js))) {
		if ((ddtmf = (switch_dtmf_t *)malloc(sizeof(*ddtmf)))) {
			*ddtmf = *dtmf;
			obj->_dtmf = ddtmf;
			obj->RegisterInstance(obj->GetIsolate(), js_safe_str(name), true);
			return obj->GetJavaScriptObject();
		} else {
			delete obj;
		}
	}
	
	return Handle<Object>();
}

void *FSDTMF::Construct(const v8::FunctionCallbackInfo<Value>& info)
{
	HandleScope handle_scope(info.GetIsolate());
	switch_dtmf_t *dtmf;
	int32_t duration = switch_core_default_dtmf_duration(0);
	const char *dtmf_char;

	if (info.Length() <= 0) {
		info.GetIsolate()->ThrowException(String::NewFromUtf8(info.GetIsolate(), "Invalid Args"));
		return NULL;
	}

	String::Utf8Value str(info[0]);
	dtmf_char = *str;

	if (info.Length() > 1) {
		duration = info[1]->Int32Value();
		if (duration <= 0) {
			duration = switch_core_default_dtmf_duration(0);
		}
	}

	if ((dtmf = (switch_dtmf_t *)malloc(sizeof(*dtmf)))) {
		FSDTMF *obj = new FSDTMF(info);
		obj->_dtmf = dtmf;

		if (dtmf_char && *dtmf_char) {
			obj->_dtmf->digit = *dtmf_char;
		}

		obj->_dtmf->duration = duration;

		return obj;
	}

	info.GetIsolate()->ThrowException(String::NewFromUtf8(info.GetIsolate(), "Memory error"));
	return NULL;
}

JS_DTMF_GET_PROPERTY_IMPL(GetProperty)
{
	HandleScope handle_scope(info.GetIsolate());
	FSDTMF *obj = JSBase::GetInstance<FSDTMF>(info.Holder());

	if (!obj) {
		info.GetReturnValue().Set(false);
		return;
	}

	String::Utf8Value str(property);
	const char *prop = js_safe_str(*str);

	if (!strcmp(prop, "digit")) {
		char tmp[2] = { obj->_dtmf->digit, '\0' };
		info.GetReturnValue().Set(String::NewFromUtf8(info.GetIsolate(), tmp));
	} else if (!strcmp(prop, "duration")) {
		info.GetReturnValue().Set(Integer::New(info.GetIsolate(), obj->_dtmf->duration));
	} else {
		info.GetIsolate()->ThrowException(String::NewFromUtf8(info.GetIsolate(), "Bad property"));
	}
}

static const js_function_t dtmf_proc[] = {
	{0}
};

static const js_property_t dtmf_prop[] = {
	{"digit", FSDTMF::GetProperty, JSBase::DefaultSetProperty},
	{"duration", FSDTMF::GetProperty, JSBase::DefaultSetProperty},
	{0}
};

static const js_class_definition_t dtmf_desc = {
	js_class_name,
	FSDTMF::Construct,
	dtmf_proc,
	dtmf_prop
};

const js_class_definition_t *FSDTMF::GetClassDefinition()
{
	return &dtmf_desc;
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
