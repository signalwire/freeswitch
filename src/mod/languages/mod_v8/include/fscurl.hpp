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
 * fscurl.hpp -- JavaScript CURL class header
 *
 */

#ifndef FS_CURL_H
#define FS_CURL_H

#include "mod_v8.h"
#include <switch_curl.h>

/* Macros for easier V8 callback definitions */
#define JS_CURL_GET_PROPERTY_DEF(method_name) JS_GET_PROPERTY_DEF(method_name, FSCURL)
#define JS_CURL_SET_PROPERTY_DEF(method_name) JS_SET_PROPERTY_DEF(method_name, FSCURL)
#define JS_CURL_FUNCTION_DEF(method_name) JS_FUNCTION_DEF(method_name, FSCURL)
#define JS_CURL_GET_PROPERTY_IMPL(method_name) JS_GET_PROPERTY_IMPL(method_name, FSCURL)
#define JS_CURL_SET_PROPERTY_IMPL(method_name) JS_SET_PROPERTY_IMPL(method_name, FSCURL)
#define JS_CURL_FUNCTION_IMPL(method_name) JS_FUNCTION_IMPL(method_name, FSCURL)

class FSCURL : public JSBase
{
private:
	switch_CURL *_curl_handle;
	v8::Persistent<v8::Function> _function;
	v8::Persistent<v8::Object> _user_data;
	v8::Persistent<v8::Value> _ret;

	void Init(void);
	static size_t FileCallback(void *ptr, size_t size, size_t nmemb, void *data);
public:
	FSCURL(JSMain *owner) : JSBase(owner) { Init(); }
	FSCURL(const v8::FunctionCallbackInfo<v8::Value>& info) : JSBase(info) { Init(); }
	virtual ~FSCURL(void);
	virtual std::string GetJSClassName();

	static const v8_mod_interface_t *GetModuleInterface();

	/* Methods available from JavaScript */
	static void *Construct(const v8::FunctionCallbackInfo<v8::Value>& info);
	JS_CURL_FUNCTION_DEF(Run);
};

#endif /* FS_CURL_H */

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
