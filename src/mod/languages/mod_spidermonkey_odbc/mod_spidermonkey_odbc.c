/* 
 * FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
 * Copyright (C) 2005/2006, Anthony Minessale II <anthmct@yahoo.com>
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
 * Anthony Minessale II <anthmct@yahoo.com>
 * Portions created by the Initial Developer are Copyright (C)
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 * 
 * Anthony Minessale II <anthmct@yahoo.com>
 *
 *
 * mod_spidermonkey_odbc.c -- ODBC Javascript Module
 *
 */
#include "mod_spidermonkey.h"

#include <sql.h>
#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable:4201)
#include <sqlext.h>
#pragma warning(pop)
#else
#include <sqlext.h>
#endif
#include <sqltypes.h>

static const char modname[] = "odbc";

/* ODBC Object */
/*********************************************************************************/
static JSBool odbc_construct(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	return JS_TRUE;
}

static void odbc_destroy(JSContext *cx, JSObject *obj)
{
}

static JSBool odbc_my_method(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	return JS_FALSE;
}

enum odbc_tinyid {
	odbc_NAME
};

static JSFunctionSpec odbc_methods[] = {
	{"myMethod", odbc_my_method, 1},
	{0}
};


static JSPropertySpec odbc_props[] = {
	{"name", odbc_NAME, JSPROP_READONLY|JSPROP_PERMANENT}, 
	{0}
};


static JSBool odbc_getProperty(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
	JSBool res = JS_TRUE;

	return res;
}

JSClass odbc_class = {
	modname, JSCLASS_HAS_PRIVATE, 
	JS_PropertyStub,  JS_PropertyStub,	odbc_getProperty,  JS_PropertyStub, 
	JS_EnumerateStub, JS_ResolveStub,	JS_ConvertStub,	  odbc_destroy, NULL, NULL, NULL,
	odbc_construct
};


switch_status_t spidermonkey_load(JSContext *cx, JSObject *obj)
{
	JS_InitClass(cx,
				 obj,
				 NULL,
				 &odbc_class,
				 odbc_construct,
				 3,
				 odbc_props,
				 odbc_methods,
				 odbc_props,
				 odbc_methods
				 );
	return SWITCH_STATUS_SUCCESS;
}


const sm_module_interface_t odbc_module_interface = {
	/*.name = */ modname,
	/*.spidermonkey_load*/ spidermonkey_load,
	/*.next*/ NULL
};

SWITCH_MOD_DECLARE(switch_status_t) spidermonkey_init(const sm_module_interface_t **module_interface)
{
	*module_interface = &odbc_module_interface;
	return SWITCH_STATUS_SUCCESS;
}

/* For Emacs:
 * Local Variables:
 * mode:c
 * indent-tabs-mode:nil
 * tab-width:4
 * c-basic-offset:4
 * End:
 * For VIM:
 * vim:set softtabstop=4 shiftwidth=4 tabstop=4 expandtab:
 */
