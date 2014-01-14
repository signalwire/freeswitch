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
 * fsrequest.cpp -- JavaScript Request class
 *
 */

#include "fsrequest.hpp"

using namespace std;
using namespace v8;

static const char js_class_name[] = "Request";

FSRequest::~FSRequest(void)
{
}

string FSRequest::GetJSClassName()
{
	return js_class_name;
}

void FSRequest::Init()
{
	_cmd = NULL;
	_stream = NULL;
}

void FSRequest::Init(const char *cmd, switch_stream_handle_t *stream)
{
	this->_cmd = cmd;
	this->_stream = stream;
}

JS_REQUEST_FUNCTION_IMPL(Write)
{
	HandleScope handle_scope(info.GetIsolate());

	if (info.Length() > 0) {
		String::Utf8Value str(info[0]);
		if (*str) this->_stream->write_function(this->_stream, "%s", *str);
		info.GetReturnValue().Set(true);
		return;
	}

	info.GetReturnValue().Set(false);
}

JS_REQUEST_FUNCTION_IMPL(AddHeader)
{
	HandleScope handle_scope(info.GetIsolate());

	if (info.Length() > 1) {
		String::Utf8Value str1(info[0]);
		String::Utf8Value str2(info[1]);
		const char *hname = js_safe_str(*str1);
		const char *hval = js_safe_str(*str2);
		switch_event_add_header_string(this->_stream->param_event, SWITCH_STACK_BOTTOM, hname, hval);
		info.GetReturnValue().Set(true);
		return;
	}

	info.GetReturnValue().Set(false);
}

JS_REQUEST_FUNCTION_IMPL(GetHeader)
{
	HandleScope handle_scope(info.GetIsolate());

	if (info.Length() > 0) {
		String::Utf8Value str(info[0]);
		const char *hname = js_safe_str(*str);
		char *val = switch_event_get_header(this->_stream->param_event, hname);
		info.GetReturnValue().Set(String::NewFromUtf8(info.GetIsolate(), js_safe_str(val)));
		return;
	}

	info.GetReturnValue().Set(false);
}

JS_REQUEST_FUNCTION_IMPL(DumpEnv)
{
	HandleScope handle_scope(info.GetIsolate());
	string how = "text";

	if (info.Length() > 0) {
		String::Utf8Value str(info[0]);
		how = js_safe_str(*str);
	}

	if (!strcasecmp(how.c_str(), "xml")) {
		switch_xml_t xml;
		if ((xml = switch_event_xmlize(this->_stream->param_event, SWITCH_VA_NONE))) {
			char *xmlstr;
			if ((xmlstr = switch_xml_toxml(xml, SWITCH_FALSE))) {
				info.GetReturnValue().Set(String::NewFromUtf8(info.GetIsolate(), xmlstr));
				switch_safe_free(xmlstr);
				switch_xml_free(xml);
				return;
			} else {
				switch_xml_free(xml);
			}
		}
	} else if (!strcasecmp(how.c_str(), "json")) {
		char *buf = NULL;
		if (switch_event_serialize_json(this->_stream->param_event, &buf) == SWITCH_STATUS_SUCCESS) {
			info.GetReturnValue().Set(String::NewFromUtf8(info.GetIsolate(), js_safe_str(buf)));
			switch_safe_free(buf);
			return;
		} else {
			switch_safe_free(buf);
		}
	} else {
		char *buf = NULL;
		if (switch_event_serialize(this->_stream->param_event, &buf, SWITCH_TRUE) == SWITCH_STATUS_SUCCESS) {
			info.GetReturnValue().Set(String::NewFromUtf8(info.GetIsolate(), js_safe_str(buf)));
			switch_safe_free(buf);
			return;
		} else {
			switch_safe_free(buf);
		}
	}

	info.GetReturnValue().Set(false);
}

JS_REQUEST_GET_PROPERTY_IMPL(GetProperty)
{
	HandleScope handle_scope(info.GetIsolate());
	String::Utf8Value str(property);

	if (!strcmp(js_safe_str(*str), "command")) {
		if (this->_cmd) {
			info.GetReturnValue().Set(String::NewFromUtf8(info.GetIsolate(), this->_cmd));
		} else {
			info.GetReturnValue().Set(String::NewFromUtf8(info.GetIsolate(), ""));
		}
	} else {
		info.GetIsolate()->ThrowException(String::NewFromUtf8(info.GetIsolate(), "Bad property"));
	}
}

static const js_function_t request_proc[] = {
	{"write", FSRequest::Write},
	{"getHeader", FSRequest::GetHeader},
	{"addHeader", FSRequest::AddHeader},
	{"dumpENV", FSRequest::DumpEnv},		// Deprecated
	{"dumpEnv", FSRequest::DumpEnv},
	{0}
};

static const js_property_t request_prop[] = {
	{"command", FSRequest::GetProperty, JSBase::DefaultSetProperty},
	{0}
};

static const js_class_definition_t request_desc = {
	js_class_name,
	NULL,			/* No constructor given, since it's not allowed to construct this from JS code */
	request_proc,
	request_prop
};

const js_class_definition_t *FSRequest::GetClassDefinition()
{
	return &request_desc;
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
