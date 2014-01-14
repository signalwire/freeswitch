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
 * fsodbc.hpp -- JavaScript ODBC class header
 *
 */

#ifndef FS_ODBC_H
#define FS_ODBC_H

#include "mod_v8.h"

#if defined(WIN32) && !defined(HAVE_ODBC)
#define HAVE_ODBC
#endif

#ifdef HAVE_ODBC

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

/* Macros for easier V8 callback definitions */
#define JS_ODBC_GET_PROPERTY_DEF(method_name) JS_GET_PROPERTY_DEF(method_name, FSODBC)
#define JS_ODBC_SET_PROPERTY_DEF(method_name) JS_SET_PROPERTY_DEF(method_name, FSODBC)
#define JS_ODBC_FUNCTION_DEF(method_name) JS_FUNCTION_DEF(method_name, FSODBC)
#define JS_ODBC_GET_PROPERTY_IMPL(method_name) JS_GET_PROPERTY_IMPL(method_name, FSODBC)
#define JS_ODBC_SET_PROPERTY_IMPL(method_name) JS_SET_PROPERTY_IMPL(method_name, FSODBC)
#define JS_ODBC_FUNCTION_IMPL(method_name) JS_FUNCTION_IMPL(method_name, FSODBC)

#define JS_ODBC_GET_PROPERTY_IMPL_STATIC(method_name) JS_GET_PROPERTY_IMPL_STATIC(method_name, FSODBC)
#define JS_ODBC_SET_PROPERTY_IMPL_STATIC(method_name) JS_SET_PROPERTY_IMPL_STATIC(method_name, FSODBC)
#define JS_ODBC_FUNCTION_IMPL_STATIC(method_name) JS_FUNCTION_IMPL_STATIC(method_name, FSODBC)

class FSODBC : public JSBase
{
private:
	switch_odbc_handle_t *_handle;
	SQLHSTMT _stmt;
	SQLCHAR *_colbuf;
	int32_t _cblen;
	std::string _dsn;

	void Init();
public:
	FSODBC(JSMain *owner) : JSBase(owner) { Init(); }
	FSODBC(const v8::FunctionCallbackInfo<v8::Value>& info) : JSBase(info) { Init(); }
	virtual ~FSODBC(void);
	virtual std::string GetJSClassName();

	static const v8_mod_interface_t *GetModuleInterface();

	static FSODBC *New(char *dsn, char *username, char *password, const v8::FunctionCallbackInfo<v8::Value>& info);
	switch_odbc_status_t Connect();

	/* Methods available from JavaScript */
	static void *Construct(const v8::FunctionCallbackInfo<v8::Value>& info);
	JS_ODBC_FUNCTION_DEF(Connect);
	JS_ODBC_FUNCTION_DEF(Disconnect);
	JS_ODBC_FUNCTION_DEF(Exec);
	JS_ODBC_FUNCTION_DEF(Execute);
	JS_ODBC_FUNCTION_DEF(NumRows);
	JS_ODBC_FUNCTION_DEF(NumCols);
	JS_ODBC_FUNCTION_DEF(NextRow);
	JS_ODBC_FUNCTION_DEF(GetData);
	JS_FUNCTION_DEF_STATIC(Close); // This will also destroy the C++ object
	JS_ODBC_GET_PROPERTY_DEF(GetProperty);

};

#endif /* HAVE_ODBC */

#endif /* FS_ODBC_H */

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
