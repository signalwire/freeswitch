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
 * fspcre.cpp -- JavaScript PCRE class
 *
 */

#include "fspcre.hpp"

using namespace std;
using namespace v8;

static const char js_class_name[] = "PCRE";

FSPCRE::~FSPCRE(void)
{
	if (!_freed && _re) {
		switch_regex_safe_free(_re);
		switch_safe_free(_str);
	}
}

string FSPCRE::GetJSClassName()
{
	return js_class_name;
}

void FSPCRE::Init()
{
	_re = NULL;
	_str = NULL;
	_proceed = 0;
	memset(&_ovector, 0, sizeof(_ovector));
	_freed = 0;
}

void *FSPCRE::Construct(const v8::FunctionCallbackInfo<Value>& info)
{
	return new FSPCRE(info);
}

JS_PCRE_FUNCTION_IMPL(Compile)
{
	HandleScope handle_scope(info.GetIsolate());
	const char *string, *regex_string;

	if (info.Length() > 1) {
		String::Utf8Value str1(info[0]);
		String::Utf8Value str2(info[1]);
		string = js_safe_str(*str1);
		regex_string = js_safe_str(*str2);
		switch_regex_safe_free(this->_re);
		switch_safe_free(this->_str);
		js_strdup(this->_str, string);
		this->_proceed = switch_regex_perform(this->_str, regex_string, &this->_re, this->_ovector,
												 sizeof(this->_ovector) / sizeof(this->_ovector[0]));
		info.GetReturnValue().Set(this->_proceed ? true : false);
	} else {
		info.GetIsolate()->ThrowException(String::NewFromUtf8(info.GetIsolate(), "Invalid args"));
	}
}

JS_PCRE_FUNCTION_IMPL(Substitute)
{
	HandleScope handle_scope(info.GetIsolate());
	const char *subst_string;
	char *substituted;

	if (!this->_proceed) {
		info.GetIsolate()->ThrowException(String::NewFromUtf8(info.GetIsolate(), "REGEX is not compiled or has no matches"));
		return;
	}

	if (info.Length() > 0) {
		uint32_t len;
		String::Utf8Value str(info[0]);
		subst_string = js_safe_str(*str);
		len = (uint32_t) (strlen(this->_str) + strlen(subst_string) + 10) * this->_proceed;
		substituted = (char *)malloc(len);
		switch_assert(substituted != NULL);
		switch_perform_substitution(this->_re, this->_proceed, subst_string, this->_str, substituted, len, this->_ovector);
		info.GetReturnValue().Set(String::NewFromUtf8(info.GetIsolate(), substituted));
		free(substituted);
	} else {
		info.GetIsolate()->ThrowException(String::NewFromUtf8(info.GetIsolate(), "Invalid Args"));
	}
}

JS_PCRE_GET_PROPERTY_IMPL(GetProperty)
{
	HandleScope handle_scope(info.GetIsolate());
	String::Utf8Value str(property);

	if (!strcmp(js_safe_str(*str), "ready")) {
		info.GetReturnValue().Set(true);
	} else {
		info.GetIsolate()->ThrowException(String::NewFromUtf8(info.GetIsolate(), "Bad property"));
	}
}

static const js_function_t pcre_proc[] = {
	{"compile", FSPCRE::Compile},
	{"substitute", FSPCRE::Substitute},
	{0}
};

static const js_property_t pcre_prop[] = {
	{"ready", FSPCRE::GetProperty, JSBase::DefaultSetProperty},
	{0}
};

static const js_class_definition_t pcre_desc = {
	js_class_name,
	FSPCRE::Construct,
	pcre_proc,
	pcre_prop
};

const js_class_definition_t *FSPCRE::GetClassDefinition()
{
	return &pcre_desc;
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
