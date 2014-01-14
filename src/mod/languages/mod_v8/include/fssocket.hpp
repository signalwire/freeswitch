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
 * fssocket.hpp -- JavaScript Socket class header
 *
 */

#ifndef FS_SOCKET_H
#define FS_SOCKET_H

#include "mod_v8.h"

/* Macros for easier V8 callback definitions */
#define JS_SOCKET_GET_PROPERTY_DEF(method_name) JS_GET_PROPERTY_DEF(method_name, FSSocket)
#define JS_SOCKET_SET_PROPERTY_DEF(method_name) JS_SET_PROPERTY_DEF(method_name, FSSocket)
#define JS_SOCKET_FUNCTION_DEF(method_name) JS_FUNCTION_DEF(method_name, FSSocket)
#define JS_SOCKET_GET_PROPERTY_IMPL(method_name) JS_GET_PROPERTY_IMPL(method_name, FSSocket)
#define JS_SOCKET_SET_PROPERTY_IMPL(method_name) JS_SET_PROPERTY_IMPL(method_name, FSSocket)
#define JS_SOCKET_FUNCTION_IMPL(method_name) JS_FUNCTION_IMPL(method_name, FSSocket)

class FSSocket : public JSBase
{
private:
	switch_socket_t *_socket;
	switch_memory_pool_t *_pool;
	char *_read_buffer;
	switch_size_t _buffer_size;
	int _state;

	void Init();
public:
	FSSocket(JSMain *owner) : JSBase(owner) { Init(); }
	FSSocket(const v8::FunctionCallbackInfo<v8::Value>& info) : JSBase(info) { Init(); }
	virtual ~FSSocket(void);
	virtual std::string GetJSClassName();

	static const v8_mod_interface_t *GetModuleInterface();

	/* Methods available from JavaScript */
	static void *Construct(const v8::FunctionCallbackInfo<v8::Value>& info);
	JS_SOCKET_FUNCTION_DEF(Connect);
	JS_SOCKET_FUNCTION_DEF(Close);
	JS_SOCKET_FUNCTION_DEF(Send);
	JS_SOCKET_FUNCTION_DEF(ReadBytes);
	JS_SOCKET_FUNCTION_DEF(Read);
	JS_SOCKET_GET_PROPERTY_DEF(GetProperty);
};

#endif /* FS_SOCKET_H */

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
