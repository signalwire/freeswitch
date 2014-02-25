/*
 * mod_v8_skel for FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
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
 * The Original Code is mod_v8_skel for FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
 *
 * The Initial Developer of the Original Code is
 * Peter Olsson <peter@olssononline.se>
 * Portions created by the Initial Developer are Copyright (C)
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 * Peter Olsson <peter@olssononline.se>
 *
 * mod_v8_skel.cpp -- JavaScript example extension module
 *
 */

/* This class is only to demonstrate how to create an external loadable extension module to mod_v8. */

#include "mod_v8.h"

using namespace std;
using namespace v8;

static const char js_class_name[] = "Skel";

/* Macros for easier V8 callback definitions */
#define JS_SKEL_GET_PROPERTY_DEF(method_name) JS_GET_PROPERTY_DEF(method_name, FSSkel)
#define JS_SKEL_SET_PROPERTY_DEF(method_name) JS_SET_PROPERTY_DEF(method_name, FSSkel)
#define JS_SKEL_FUNCTION_DEF(method_name) JS_FUNCTION_DEF(method_name, FSSkel)
#define JS_SKEL_GET_PROPERTY_IMPL(method_name) JS_GET_PROPERTY_IMPL(method_name, FSSkel)
#define JS_SKEL_SET_PROPERTY_IMPL(method_name) JS_SET_PROPERTY_IMPL(method_name, FSSkel)
#define JS_SKEL_FUNCTION_IMPL(method_name) JS_FUNCTION_IMPL(method_name, FSSkel)

class FSSkel : public JSBase
{
private:
	int x;
	string y;
public:
	FSSkel(const v8::FunctionCallbackInfo<Value>& info);
	virtual ~FSSkel(void);

	virtual string GetJSClassName() { return js_class_name; }

	/* JS methods */

	static void *Construct(const v8::FunctionCallbackInfo<Value>& info);

	JS_SKEL_GET_PROPERTY_DEF(GetPropertyX);
	JS_SKEL_SET_PROPERTY_DEF(SetPropertyX);
	JS_SKEL_GET_PROPERTY_DEF(GetPropertyY);
	JS_SKEL_SET_PROPERTY_DEF(SetPropertyY);
	JS_SKEL_GET_PROPERTY_DEF(GetPropertyZ);
	JS_SKEL_FUNCTION_DEF(MyFunction);
	JS_SKEL_FUNCTION_DEF(MyFunction2);
};

/* =============================================================================== */

FSSkel::FSSkel(const v8::FunctionCallbackInfo<Value>& info) : JSBase(info)
{
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "FSSkel::FSSkel()\n");

	x = 0;
	y = "";
}

FSSkel::~FSSkel(void)
{
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "FSSkel::~FSSkel()\n");
}

void *FSSkel::Construct(const v8::FunctionCallbackInfo<Value>& info)
{
	FSSkel *obj;

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "FSSkel::Constructor\n");

	obj = new FSSkel(info);

	/* Parse input variables */
	if (info.Length() > 0) {
		if (!info[0].IsEmpty() && info[0]->IsInt32()) {
			obj->x = info[0]->Int32Value();
		}

		if (!info[1].IsEmpty() && info[1]->IsString()) {
			String::Utf8Value str(info[1]);
			if (*str) {
				obj->y = *str;
			}
		}
	}

	return obj;
}

JS_SKEL_GET_PROPERTY_IMPL(GetPropertyX)
{
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "FSSkel::GetPropertyX (%d)\n", x);
	info.GetReturnValue().Set(x);
}

JS_SKEL_SET_PROPERTY_IMPL(SetPropertyX)
{
	x = value->Int32Value();
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "FSSkel::SetPropertyX to %d\n", x);
}

JS_SKEL_GET_PROPERTY_IMPL(GetPropertyY)
{
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "FSSkel::GetPropertyY (%s)\n", y.c_str());
	info.GetReturnValue().Set(String::NewFromUtf8(info.GetIsolate(), y.c_str()));
}

JS_SKEL_SET_PROPERTY_IMPL(SetPropertyY)
{
	String::Utf8Value str(value);

	y = js_safe_str(*str);
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "FSSkel::SetPropertyY to '%s'\n", y.c_str());
}

JS_SKEL_GET_PROPERTY_IMPL(GetPropertyZ)
{
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "FSSkel::GetPropertyZ (z)\n");
	info.GetReturnValue().Set(String::NewFromUtf8(info.GetIsolate(), "z"));
}

JS_SKEL_FUNCTION_IMPL(MyFunction)
{
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "FSSkel::MyFunction\n");
}

JS_SKEL_FUNCTION_IMPL(MyFunction2)
{
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "FSSkel::MyFunction2 - will twrow a JavaScript exception\n");
	info.GetIsolate()->ThrowException(String::NewFromUtf8(info.GetIsolate(), "Error in MyFunction2()"));
}

/* Add the JS methods here */
static const js_function_t skel_methods[] = {
	{"myFunction", FSSkel::MyFunction},
	{"myFunction2", FSSkel::MyFunction2},
	{0}
};

/* Add the JS properties here */
static const js_property_t skel_props[] = {
	{"x", FSSkel::GetPropertyX, FSSkel::SetPropertyX},
	{"y", FSSkel::GetPropertyY, FSSkel::SetPropertyY},
	{"z", FSSkel::GetPropertyZ, JSBase::DefaultSetProperty},
	{0}
};

/* The main definition of the JS Class, holding the name of the class, the constructor and the moethods and properties */
static const js_class_definition_t skel_desc = {
	js_class_name,
	FSSkel::Construct,
	skel_methods,
	skel_props
};

SWITCH_BEGIN_EXTERN_C

/* Called from mod_v8 when user scripts [use('Skel');] inside the script. This will load the class into the current JS execution */
static switch_status_t skel_load(const v8::FunctionCallbackInfo<Value>& info)
{
	JSBase::Register(info.GetIsolate(), &skel_desc);
	return SWITCH_STATUS_SUCCESS;
}

/* The module interface that mod_v8 will get when loading this module */
static const v8_mod_interface_t skel_module_interface = {
	/*.name = */ js_class_name,
	/*.js_mod_load */ skel_load
};

/* The exported method called from the module to get this module's inteface */
SWITCH_MOD_DECLARE_NONSTD(switch_status_t) v8_mod_init(const v8_mod_interface_t **module_interface)
{
	*module_interface = &skel_module_interface;
	return SWITCH_STATUS_SUCCESS;
}

SWITCH_END_EXTERN_C

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
