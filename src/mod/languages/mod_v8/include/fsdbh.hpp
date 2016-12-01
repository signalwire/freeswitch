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
 * Andrey Volk <andywolk@gmail.com>
 * Portions created by the Initial Developer are Copyright (C)
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 * Peter Olsson <peter@olssononline.se>
 * Andrey Volk <andywolk@gmail.com>
 *
 * fsdbh.hpp -- JavaScript DBH class header
 *
 */

#ifndef FS_DBH_H
#define FS_DBH_H

#include "mod_v8.h"

/* Macros for easier V8 callback definitions */
#define JS_DBH_GET_PROPERTY_DEF(method_name) JS_GET_PROPERTY_DEF(method_name, FSDBH)
#define JS_DBH_SET_PROPERTY_DEF(method_name) JS_SET_PROPERTY_DEF(method_name, FSDBH)
#define JS_DBH_FUNCTION_DEF(method_name) JS_FUNCTION_DEF(method_name, FSDBH)
#define JS_DBH_GET_PROPERTY_IMPL(method_name) JS_GET_PROPERTY_IMPL(method_name, FSDBH)
#define JS_DBH_SET_PROPERTY_IMPL(method_name) JS_SET_PROPERTY_IMPL(method_name, FSDBH)
#define JS_DBH_FUNCTION_IMPL(method_name) JS_FUNCTION_IMPL(method_name, FSDBH)

#define JS_DBH_GET_PROPERTY_IMPL_STATIC(method_name) JS_GET_PROPERTY_IMPL_STATIC(method_name, FSDBH)
#define JS_DBH_SET_PROPERTY_IMPL_STATIC(method_name) JS_SET_PROPERTY_IMPL_STATIC(method_name, FSDBH)
#define JS_DBH_FUNCTION_IMPL_STATIC(method_name) JS_FUNCTION_IMPL_STATIC(method_name, FSDBH)

class FSDBH : public JSBase
{
private:
	v8::Persistent<v8::Function> _callback;
	std::string _dsn;
	switch_cache_db_handle_t *dbh;
	char *err;

	void Init();

	bool _release();
	void clear_error();

	static int Callback(void *pArg, int argc, char **argv, char **columnNames);
public:
	FSDBH(JSMain *owner) : JSBase(owner) { Init(); }
	FSDBH(const v8::FunctionCallbackInfo<v8::Value>& info) : JSBase(info) { Init(); }

	virtual ~FSDBH(void);
	virtual std::string GetJSClassName();

	static const v8_mod_interface_t *GetModuleInterface();

	static FSDBH *New(char *dsn, char *username, char *password, const v8::FunctionCallbackInfo<v8::Value>& info);

	/* Methods available from JavaScript */
	static void *Construct(const v8::FunctionCallbackInfo<v8::Value>& info);
	JS_DBH_FUNCTION_DEF(affected_rows);
	JS_DBH_FUNCTION_DEF(connected);
	JS_DBH_FUNCTION_DEF(last_error);
	JS_DBH_FUNCTION_DEF(query);
	JS_DBH_FUNCTION_DEF(release);
	JS_DBH_FUNCTION_DEF(test_reactive);
	JS_DBH_GET_PROPERTY_DEF(GetProperty);
};

#endif /* FS_DB_H */

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
