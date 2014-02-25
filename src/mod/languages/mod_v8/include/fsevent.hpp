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
 * The Original Code is mod_v8 for FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
 *
 * The Initial Developer of the Original Code is
 * Peter Olsson <peter@olssononline.se>
 * Portions created by the Initial Developer are Copyright (C)
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 * Peter Olsson <peter@olssononline.se>
 *
 * fsevent.hpp -- JavaScript Event class header
 *
 */

#ifndef FS_EVENT_H
#define FS_EVENT_H

#include "javascript.hpp"
#include <switch.h>

/* Macros for easier V8 callback definitions */
#define JS_EVENT_GET_PROPERTY_DEF(method_name) JS_GET_PROPERTY_DEF(method_name, FSEvent)
#define JS_EVENT_SET_PROPERTY_DEF(method_name) JS_SET_PROPERTY_DEF(method_name, FSEvent)
#define JS_EVENT_FUNCTION_DEF(method_name) JS_FUNCTION_DEF(method_name, FSEvent)
#define JS_EVENT_GET_PROPERTY_IMPL(method_name) JS_GET_PROPERTY_IMPL(method_name, FSEvent)
#define JS_EVENT_SET_PROPERTY_IMPL(method_name) JS_SET_PROPERTY_IMPL(method_name, FSEvent)
#define JS_EVENT_FUNCTION_IMPL(method_name) JS_FUNCTION_IMPL(method_name, FSEvent)

#define JS_EVENT_GET_PROPERTY_IMPL_STATIC(method_name) JS_GET_PROPERTY_IMPL_STATIC(method_name, FSEvent)
#define JS_EVENT_SET_PROPERTY_IMPL_STATIC(method_name) JS_SET_PROPERTY_IMPL_STATIC(method_name, FSEvent)
#define JS_EVENT_FUNCTION_IMPL_STATIC(method_name) JS_FUNCTION_IMPL_STATIC(method_name, FSEvent)

class FSEvent : public JSBase
{
private:
	switch_event_t *_event;
	int _freed;

	void Init();
	bool IsArray(const char *var);
public:
	FSEvent(JSMain *owner) : JSBase(owner) { Init(); }
	FSEvent(const v8::FunctionCallbackInfo<v8::Value>& info) : JSBase(info) { Init(); }
	virtual ~FSEvent(void);
	virtual std::string GetJSClassName();

	static const js_class_definition_t *GetClassDefinition();

	void SetEvent(switch_event_t *event, int freed = 0);
	switch_event_t **GetEvent();

	/* Methods available from JavaScript */
	static void *Construct(const v8::FunctionCallbackInfo<v8::Value>& info);
	static v8::Handle<v8::Object> New(switch_event_t *event, const char *name, JSMain *js);
	JS_EVENT_FUNCTION_DEF(AddHeader);
	JS_EVENT_FUNCTION_DEF(GetHeader);
	JS_EVENT_FUNCTION_DEF(IsArrayHeader);
	JS_EVENT_FUNCTION_DEF(AddBody);
	JS_EVENT_FUNCTION_DEF(GetBody);
	JS_EVENT_FUNCTION_DEF(GetType);
	JS_EVENT_FUNCTION_DEF(Serialize);
	JS_EVENT_FUNCTION_DEF(ChatExecute);
	JS_FUNCTION_DEF_STATIC(Fire); // This will also destroy the C++ object
	JS_FUNCTION_DEF_STATIC(Destroy); // This will also destroy the C++ object
	JS_EVENT_GET_PROPERTY_DEF(GetProperty);
};

#endif /* FS_EVENT_H */

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
