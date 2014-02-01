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
 * fseventhandler.hpp -- JavaScript EventHandler class header
 *
 */

#ifndef FS_EVENTHANDLER_H
#define FS_EVENTHANDLER_H

#include "mod_v8.h"

/* Macros for easier V8 callback definitions */
#define JS_EVENTHANDLER_GET_PROPERTY_DEF(method_name) JS_GET_PROPERTY_DEF(method_name, FSEventHandler)
#define JS_EVENTHANDLER_SET_PROPERTY_DEF(method_name) JS_SET_PROPERTY_DEF(method_name, FSEventHandler)
#define JS_EVENTHANDLER_FUNCTION_DEF(method_name) JS_FUNCTION_DEF(method_name, FSEventHandler)
#define JS_EVENTHANDLER_GET_PROPERTY_IMPL(method_name) JS_GET_PROPERTY_IMPL(method_name, FSEventHandler)
#define JS_EVENTHANDLER_SET_PROPERTY_IMPL(method_name) JS_SET_PROPERTY_IMPL(method_name, FSEventHandler)
#define JS_EVENTHANDLER_FUNCTION_IMPL(method_name) JS_FUNCTION_IMPL(method_name, FSEventHandler)
#define JS_EVENTHANDLER_FUNCTION_IMPL_STATIC(method_name) JS_FUNCTION_IMPL_STATIC(method_name, FSEventHandler)
#define JS_EVENTHANDLER_GET_PROPERTY_IMPL_STATIC(method_name) JS_GET_PROPERTY_IMPL_STATIC(method_name, FSEventHandler)

class FSEventHandler : public JSBase
{
private:
	switch_mutex_t *_mutex;
	switch_memory_pool_t *_pool;
	switch_hash_t *_event_hash;
	switch_queue_t *_event_queue;
	uint8_t _event_list[SWITCH_EVENT_ALL + 1];
	switch_event_t *_filters;

	void Init();
	void DoSubscribe(const v8::FunctionCallbackInfo<v8::Value>& info);
public:
	FSEventHandler(JSMain *owner) : JSBase(owner) { Init(); }
	FSEventHandler(const v8::FunctionCallbackInfo<v8::Value>& info) : JSBase(info) { Init(); }
	virtual ~FSEventHandler(void);
	virtual std::string GetJSClassName();

	static const v8_mod_interface_t *GetModuleInterface();

	/* Public method to queue an event to this instance */
	void QueueEvent(switch_event_t *event);

	/* Methods available from JavaScript */
	static void *Construct(const v8::FunctionCallbackInfo<v8::Value>& info);
	JS_EVENTHANDLER_FUNCTION_DEF(Subscribe);
	JS_EVENTHANDLER_FUNCTION_DEF(UnSubscribe);
	JS_EVENTHANDLER_FUNCTION_DEF(AddFilter);
	JS_EVENTHANDLER_FUNCTION_DEF(DeleteFilter);
	JS_EVENTHANDLER_FUNCTION_DEF(GetEvent);
	JS_EVENTHANDLER_FUNCTION_DEF(SendEvent);
	JS_EVENTHANDLER_FUNCTION_DEF(ExecuteApi);
	JS_EVENTHANDLER_FUNCTION_DEF(ExecuteBgApi);
	JS_FUNCTION_DEF_STATIC(Destroy);
	JS_GET_PROPERTY_DEF_STATIC(GetReadyProperty);
};

#endif /* FS_EVENTHANDLER_H */

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
