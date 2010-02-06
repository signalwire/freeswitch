/* 
 * FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
 * Copyright (C) 2005-2010, Anthony Minessale II <anthm@freeswitch.org>
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
 * Anthony Minessale II <anthm@freeswitch.org>
 * Portions created by the Initial Developer are Copyright (C)
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 * 
 * Anthony Minessale II <anthm@freeswitch.org>
 *
 *
 * mod_spidermonkey_skel.c -- Skel Javascript Module
 *
 */
#include "mod_spidermonkey.h"

static const char modname[] = "Skel";

/* Skel Object */
/*********************************************************************************/
static JSBool skel_construct(JSContext * cx, JSObject * obj, uintN argc, jsval * argv, jsval * rval)
{
	return JS_TRUE;
}

static void skel_destroy(JSContext * cx, JSObject * obj)
{
}

static JSBool skel_my_method(JSContext * cx, JSObject * obj, uintN argc, jsval * argv, jsval * rval)
{
	return JS_FALSE;
}

enum skel_tinyid {
	SKEL_NAME
};

static JSFunctionSpec skel_methods[] = {
	{"myMethod", skel_my_method, 1},
	{0}
};


static JSPropertySpec skel_props[] = {
	{"name", SKEL_NAME, JSPROP_READONLY | JSPROP_PERMANENT},
	{0}
};


static JSBool skel_getProperty(JSContext * cx, JSObject * obj, jsval id, jsval * vp)
{
	JSBool res = JS_TRUE;

	return res;
}

JSClass skel_class = {
	modname, JSCLASS_HAS_PRIVATE,
	JS_PropertyStub, JS_PropertyStub, skel_getProperty, DEFAULT_SET_PROPERTY,
	JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub, skel_destroy, NULL, NULL, NULL,
	skel_construct
};


switch_status_t spidermonkey_load(JSContext * cx, JSObject * obj)
{
	JS_InitClass(cx, obj, NULL, &skel_class, skel_construct, 3, skel_props, skel_methods, skel_props, skel_methods);
	return SWITCH_STATUS_SUCCESS;
}


const sm_module_interface_t skel_module_interface = {
	/*.name = */ modname,
	/*.spidermonkey_load */ spidermonkey_load,
	/*.next */ NULL
};

SWITCH_MOD_DECLARE(switch_status_t) spidermonkey_init(const sm_module_interface_t ** module_interface)
{
	*module_interface = &skel_module_interface;
	return SWITCH_STATUS_SUCCESS;
}

/* For Emacs:
 * Local Variables:
 * mode:c
 * indent-tabs-mode:t
 * tab-width:4
 * c-basic-offset:4
 * End:
 * For VIM:
 * vim:set softtabstop=4 shiftwidth=4 tabstop=4:
 */
