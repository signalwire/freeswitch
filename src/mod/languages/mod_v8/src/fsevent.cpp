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
 * fsevent.cpp -- JavaScript Event class
 *
 */

#include "fsevent.hpp"

using namespace std;
using namespace v8;

static const char js_class_name[] = "Event";

FSEvent::~FSEvent(void)
{
	if (!_freed && _event) {
		switch_event_destroy(&_event);
	}
}

string FSEvent::GetJSClassName()
{
	return js_class_name;
}

void FSEvent::Init()
{
	_event = NULL;
	_freed = 0;
}

/* Check if a header is of the array type */
bool FSEvent::IsArray(const char *var)
{
	bool ret = false;

	if (!zstr(var)) {
		const char *val = switch_event_get_header(_event, var);

		if (val && !strncmp(val, "ARRAY", 5)) {
			ret = true;
		}
	}

	return ret;
}

void FSEvent::SetEvent(switch_event_t *event, int freed)
{
	/* Free existing event if it exists already */
	if (!this->_freed && this->_event) {
		switch_event_destroy(&this->_event);
	}

	this->_event = event;
	this->_freed = freed;
}

switch_event_t **FSEvent::GetEvent()
{
	return &_event;
}

Handle<Object> FSEvent::New(switch_event_t *event, const char *name, JSMain *js)
{
	FSEvent *obj;

	if ((obj = new FSEvent(js))) {
		obj->_event = event;
		obj->_freed = 1;
		obj->RegisterInstance(js->GetIsolate(), js_safe_str(name), true);
		return obj->GetJavaScriptObject();
	}

	return Handle<Object>();
}

void *FSEvent::Construct(const v8::FunctionCallbackInfo<Value>& info)
{
	HandleScope handle_scope(info.GetIsolate());

	if (info.Length() > 0) {
		switch_event_t *event;
		FSEvent *obj;
		switch_event_types_t etype;
		String::Utf8Value str(info[0]);
		const char *ename = js_safe_str(*str);

		if ((obj = new FSEvent(info))) {
			if (switch_name_event(ename, &etype) != SWITCH_STATUS_SUCCESS) {
				char *err = switch_mprintf("Unknown event %s", ename);
				info.GetIsolate()->ThrowException(String::NewFromUtf8(info.GetIsolate(), err));
				free(err);
				delete obj;
				return NULL;
			}

			if (etype == SWITCH_EVENT_CUSTOM) {
				string subclass_name;
				if (info.Length() > 1) {
					String::Utf8Value str2(info[1]);
					if (*str2) {
						subclass_name = js_safe_str(*str2);
					}
				} else {
					subclass_name = "none";
				}

				if (switch_event_create_subclass(&event, etype, subclass_name.c_str()) != SWITCH_STATUS_SUCCESS) {
					info.GetIsolate()->ThrowException(String::NewFromUtf8(info.GetIsolate(), "Failed to create sub class"));
					delete obj;
					return NULL;
				}
			} else {
				if (switch_event_create(&event, etype) != SWITCH_STATUS_SUCCESS) {
					info.GetIsolate()->ThrowException(String::NewFromUtf8(info.GetIsolate(), "Failed to create event"));
					delete obj;
					return NULL;
				}
			}

			/* Third argument tells if the headers should be unique */
			if (event && !info[2].IsEmpty() && info[2]->BooleanValue()) {
				event->flags |= EF_UNIQ_HEADERS;
			}

			obj->_event = event;
			obj->_freed = 0;

			return obj;
		}
	}

	info.GetIsolate()->ThrowException(String::NewFromUtf8(info.GetIsolate(), "Invalid Args"));
	return NULL;
}

JS_EVENT_FUNCTION_IMPL(AddHeader)
{
	HandleScope handle_scope(info.GetIsolate());

	if (!_event || _freed) {
		info.GetReturnValue().Set(false);
		return;
	}

	if (info.Length() > 1) {
		String::Utf8Value str1(info[0]);
		String::Utf8Value str2(info[1]);
		const char *hname = js_safe_str(*str1);
		const char *hval = js_safe_str(*str2);
		switch_stack_t stack_kind = SWITCH_STACK_BOTTOM;

		/* Check if we should push this value to the end of an array */
		if (!info[2].IsEmpty() && info[2]->BooleanValue()) {
			stack_kind = SWITCH_STACK_PUSH;
		}

		switch_event_add_header_string(_event, stack_kind, hname, hval);
		info.GetReturnValue().Set(true);
		return;
	}

	info.GetReturnValue().Set(false);
}

JS_EVENT_FUNCTION_IMPL(GetHeader)
{
	HandleScope handle_scope(info.GetIsolate());

	if (!_event) {
		info.GetReturnValue().Set(false);
		return;
	}

	if (info.Length() > 0) {
		String::Utf8Value str(info[0]);
		const char *hname = js_safe_str(*str);
		const char *val = NULL;
		int idx = -1;

		/* Check if caller expects to get data from an array */
		if (info.Length() > 1 && !info[1].IsEmpty()) {
			idx = info[1]->Int32Value();

			if (idx < 0 || !IsArray(hname)) {
				idx = -1;
			}
		}

		if (idx > -1) {
			val = switch_event_get_header_idx(_event, hname, idx);
		} else {
			val = switch_event_get_header(_event, hname);
		}

		if (!val && idx > -1) {
			/* Return null if we fetched and array value that didn't exist (so we know when to exit a loop) */
			info.GetReturnValue().Set(Null(info.GetIsolate()));
		} else {
			info.GetReturnValue().Set(String::NewFromUtf8(info.GetIsolate(), js_safe_str(val)));
		}

		return;
	}

	info.GetReturnValue().Set(false);
}

JS_EVENT_FUNCTION_IMPL(IsArrayHeader)
{
	if (!_event) {
		info.GetReturnValue().Set(false);
		return;
	}

	if (info.Length() > 0) {
		String::Utf8Value str(info[0]);
		info.GetReturnValue().Set(IsArray(js_safe_str(*str)));
	} else {
		info.GetReturnValue().Set(false);
	}
}

JS_EVENT_FUNCTION_IMPL(AddBody)
{
	HandleScope handle_scope(info.GetIsolate());

	if (!_event || _freed) {
		info.GetReturnValue().Set(false);
		return;
	}

	if (info.Length() > 0) {
		String::Utf8Value str(info[0]);
		const char *body = js_safe_str(*str);
		switch_event_add_body(_event, "%s", body);
		info.GetReturnValue().Set(true);
		return;
	}

	info.GetReturnValue().Set(false);
}

JS_EVENT_FUNCTION_IMPL(GetBody)
{
	HandleScope handle_scope(info.GetIsolate());
	const char *body;

	if (!_event) {
		info.GetReturnValue().Set(false);
		return;
	}

	if ((body = switch_event_get_body(_event))) {
		info.GetReturnValue().Set(String::NewFromUtf8(info.GetIsolate(), body));
	} else {
		info.GetReturnValue().Set(String::NewFromUtf8(info.GetIsolate(), ""));
	}
}

JS_EVENT_FUNCTION_IMPL(GetType)
{
	HandleScope handle_scope(info.GetIsolate());
	const char *event_name;

	if (!_event) {
		info.GetReturnValue().Set(false);
		return;
	}

	event_name = switch_event_name(_event->event_id);
	info.GetReturnValue().Set(String::NewFromUtf8(info.GetIsolate(), js_safe_str(event_name)));
}

JS_EVENT_FUNCTION_IMPL(Serialize)
{
	HandleScope handle_scope(info.GetIsolate());
	char *buf;
	uint8_t isxml = 0, isjson = 0;

	if (!_event) {
		info.GetReturnValue().Set(false);
		return;
	}

	if (info.Length() > 0) {
		String::Utf8Value str(info[0]);
		const char *arg = js_safe_str(*str);
		if (!strcasecmp(arg, "xml")) {
			isxml++;
		} else if (!strcasecmp(arg, "json")) {
			isjson++;
		}
	}

	if (isxml) {
		switch_xml_t xml;
		char *xmlstr;
		if ((xml = switch_event_xmlize(_event, SWITCH_VA_NONE))) {
			xmlstr = switch_xml_toxml(xml, SWITCH_FALSE);
			info.GetReturnValue().Set(String::NewFromUtf8(info.GetIsolate(), js_safe_str(xmlstr)));
			switch_xml_free(xml);
			switch_safe_free(xmlstr);
		} else {
			info.GetReturnValue().Set(false);
		}
	} else if (isjson) {
		if (switch_event_serialize_json(_event, &buf) == SWITCH_STATUS_SUCCESS) {
			info.GetReturnValue().Set(String::NewFromUtf8(info.GetIsolate(), js_safe_str(buf)));
			switch_safe_free(buf);
		}
	} else {
		if (switch_event_serialize(_event, &buf, SWITCH_TRUE) == SWITCH_STATUS_SUCCESS) {
			info.GetReturnValue().Set(String::NewFromUtf8(info.GetIsolate(), js_safe_str(buf)));
			switch_safe_free(buf);
		}
	}
}

JS_EVENT_FUNCTION_IMPL_STATIC(Fire)
{
	JS_CHECK_SCRIPT_STATE();
	HandleScope handle_scope(info.GetIsolate());
	FSEvent *obj = JSBase::GetInstance<FSEvent>(info.Holder());

	if (obj && obj->_event) {
		switch_event_fire(&obj->_event);
		obj->_freed = 1;
		delete obj;
		info.GetReturnValue().Set(true);
		return;
	}

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "No valid event to fire away\n");
	info.GetReturnValue().Set(false);
}

JS_EVENT_FUNCTION_IMPL(ChatExecute)
{
	HandleScope handle_scope(info.GetIsolate());

	if (_event) {
		if (info.Length() > 0) {
			String::Utf8Value str(info[0]);
			const char *app = js_safe_str(*str);
			string arg;

			if (info.Length() > 1) {
				String::Utf8Value str2(info[1]);
				if (*str2) {
					arg = js_safe_str(*str2);
				}
			}

			switch_core_execute_chat_app(_event, app, arg.c_str());

			info.GetReturnValue().Set(true);
			return;
		}
	}

	info.GetReturnValue().Set(false);
}

JS_EVENT_FUNCTION_IMPL_STATIC(Destroy)
{
	JS_CHECK_SCRIPT_STATE();
	HandleScope handle_scope(info.GetIsolate());
	FSEvent *obj = JSBase::GetInstance<FSEvent>(info.Holder());

	if (obj) {
		delete obj;
		info.GetReturnValue().Set(true);
		return;
	}

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Object already destroyed\n");
	info.GetReturnValue().Set(false);
}

JS_EVENT_GET_PROPERTY_IMPL(GetProperty)
{
	HandleScope handle_scope(info.GetIsolate());
	String::Utf8Value str(property);

	if (!strcmp(js_safe_str(*str), "ready")) {
		info.GetReturnValue().Set(_event ? true : false);
	} else {
		info.GetIsolate()->ThrowException(String::NewFromUtf8(info.GetIsolate(), "Bad property"));
	}
}

static const js_function_t event_proc[] = {
	{"addHeader", FSEvent::AddHeader},
	{"getHeader", FSEvent::GetHeader},
	{"isArrayHeader", FSEvent::IsArrayHeader},
	{"addBody", FSEvent::AddBody},
	{"getBody", FSEvent::GetBody},
	{"getType", FSEvent::GetType},
	{"serialize", FSEvent::Serialize},
	{"fire", FSEvent::Fire},
	{"chatExecute", FSEvent::ChatExecute},
	{"destroy", FSEvent::Destroy},
	{0}
};

static const js_property_t event_prop[] = {
	{"ready", FSEvent::GetProperty, JSBase::DefaultSetProperty},
	{0}
};

static const js_class_definition_t event_desc = {
	js_class_name,
	FSEvent::Construct,
	event_proc,
	event_prop
};

const js_class_definition_t *FSEvent::GetClassDefinition()
{
	return &event_desc;
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
