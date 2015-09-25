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
 *
 * fsodbc.cpp -- JavaScript ODBC class
 *
 */

#include "fsodbc.hpp"

using namespace std;
using namespace v8;

#ifdef HAVE_ODBC

static const char js_class_name[] = "ODBC";

FSODBC::~FSODBC(void)
{
	if (_stmt) {
		SQLFreeHandle(SQL_HANDLE_STMT, _stmt);
	}

	if (_handle) {
		switch_odbc_handle_destroy(&_handle);
	}

	switch_safe_free(_colbuf);
}

string FSODBC::GetJSClassName()
{
	return js_class_name;
}

void FSODBC::Init()
{
	_handle = NULL;
	_stmt = NULL;
	_colbuf = NULL;
	_cblen = 0;
}

FSODBC *FSODBC::New(char *dsn, char *username, char *password, const v8::FunctionCallbackInfo<Value>& info)
{
	FSODBC *new_obj = NULL;

	if (!(new_obj = new FSODBC(info))) {
		goto err;
	}

	if (!(new_obj->_handle = switch_odbc_handle_new(dsn, username, password))) {
		goto err;
	}

	new_obj->_dsn = dsn;
	return new_obj;

  err:
	if (new_obj) {
		if (new_obj->_handle) {
			switch_odbc_handle_destroy(&new_obj->_handle);
		}
		delete new_obj;
	}

	return NULL;
}

switch_odbc_status_t FSODBC::Connect()
{
	return switch_odbc_handle_connect(_handle);
}

void *FSODBC::Construct(const v8::FunctionCallbackInfo<Value>& info)
{
	FSODBC *odbc_obj = NULL;
	char *dsn, *username, *password;
	int32_t blen = 1024;

	if (info.Length() < 3) {
		info.GetIsolate()->ThrowException(String::NewFromUtf8(info.GetIsolate(), "Invalid parameters"));
		return NULL;
	}

	String::Utf8Value str1(info[0]);
	String::Utf8Value str2(info[1]);
	String::Utf8Value str3(info[2]);
	dsn = *str1;
	username = *str2;
	password = *str3;

	if (info.Length() > 3) {
		int32_t len = info[3]->Int32Value();

		if (len > 0) {
			blen = len;
		}
	}

	if (zstr(username)) {
		username = NULL;
	}

	if (zstr(password)) {
		password = NULL;
	}

	if (dsn) {
		odbc_obj = New(dsn, username, password, info);
	}

	if (!odbc_obj) {
		info.GetIsolate()->ThrowException(String::NewFromUtf8(info.GetIsolate(), "Failed to create new ODBC instance"));
		return NULL;
	}

	if (!(odbc_obj->_colbuf = (SQLCHAR *) malloc(blen))) {
		delete odbc_obj;
		info.GetIsolate()->ThrowException(String::NewFromUtf8(info.GetIsolate(), "Memory error"));
		return NULL;
	}

	odbc_obj->_cblen = blen;

	return odbc_obj;
}

JS_ODBC_FUNCTION_IMPL(Connect)
{
	HandleScope handle_scope(info.GetIsolate());
	bool tf = true;

	if (Connect() == SWITCH_ODBC_SUCCESS) {
		tf = true;
	} else {
		tf = false;
	}

	info.GetReturnValue().Set(tf);
}

JS_ODBC_FUNCTION_IMPL(Execute)
{
	HandleScope handle_scope(info.GetIsolate());
	const char *sql;
	bool tf = false;
	SQLHSTMT local_stmt;
	String::Utf8Value str(info[0]);

	if (info.Length() < 1) {
		goto done;
	}

	if (switch_odbc_handle_get_state(_handle) != SWITCH_ODBC_STATE_CONNECTED) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Database is not connected!\n");
		goto done;
	}

	sql = js_safe_str(*str);

	if (switch_odbc_handle_exec(_handle, sql, &local_stmt, NULL) != SWITCH_ODBC_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "[ODBC] Execute failed for: %s\n", sql);
		goto done;
	}

	SQLFreeHandle(SQL_HANDLE_STMT, local_stmt);

	tf = true;

  done:

	info.GetReturnValue().Set(tf);
}

JS_ODBC_FUNCTION_IMPL(Exec)
{
	HandleScope handle_scope(info.GetIsolate());
	const char *sql;
	bool tf = false;
	String::Utf8Value str(info[0]);

	if (info.Length() < 1) {
		goto done;
	}

	if (switch_odbc_handle_get_state(_handle) != SWITCH_ODBC_STATE_CONNECTED) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Database is not connected!\n");
		goto done;
	}

	if (_stmt) {
		SQLFreeHandle(SQL_HANDLE_STMT, _stmt);
		_stmt = NULL;
	}

	sql = js_safe_str(*str);

	if (switch_odbc_handle_exec(_handle, sql, &_stmt, NULL) != SWITCH_ODBC_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "[ODBC] query failed: %s\n", sql);
		goto done;
	}

	tf = true;

  done:

	info.GetReturnValue().Set(tf);
}

JS_ODBC_FUNCTION_IMPL(NumRows)
{
	HandleScope handle_scope(info.GetIsolate());
	SQLLEN row_count = 0;

	if (switch_odbc_handle_get_state(_handle) != SWITCH_ODBC_STATE_CONNECTED) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Database is not connected!\n");
		goto done;
	}

	if (_stmt) {
		SQLRowCount(_stmt, &row_count);
	}

  done:

	info.GetReturnValue().Set(Integer::New(info.GetIsolate(), (int32_t)row_count));
}

JS_ODBC_FUNCTION_IMPL(NumCols)
{
	HandleScope handle_scope(info.GetIsolate());

	SQLSMALLINT cols = 0;

	if (switch_odbc_handle_get_state(_handle) != SWITCH_ODBC_STATE_CONNECTED) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Database is not connected!\n");
		goto done;
	}

	if (_stmt) {
		SQLNumResultCols(_stmt, &cols);
	}

  done:

	info.GetReturnValue().Set(cols);
}

JS_ODBC_FUNCTION_IMPL(NextRow)
{
	HandleScope handle_scope(info.GetIsolate());
	int result = 0;
	bool tf = false;

	if (switch_odbc_handle_get_state(_handle) != SWITCH_ODBC_STATE_CONNECTED) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Database is not connected!\n");
		goto done;
	}

	if (_stmt) {
		if ((result = SQLFetch(_stmt) == SQL_SUCCESS)) {
			tf = true;
		}
	}

  done:

	info.GetReturnValue().Set(tf);
}

JS_ODBC_FUNCTION_IMPL(GetData)
{
	HandleScope handle_scope(info.GetIsolate());

	if (switch_odbc_handle_get_state(_handle) != SWITCH_ODBC_STATE_CONNECTED) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Database is not connected!\n");
		goto done;
	}

	if (_stmt) {
		SQLSMALLINT nColumns = 0, x = 0;

		if (SQLNumResultCols(_stmt, &nColumns) != SQL_SUCCESS) {
			goto done;
		}

		Handle<Array> arg = Array::New(GetIsolate(), nColumns);

		for (x = 1; x <= nColumns; x++) {
			SQLSMALLINT NameLength, DataType, DecimalDigits, Nullable;
			SQLULEN ColumnSize;
			SQLCHAR name[1024] = "";
			SQLCHAR *data = _colbuf;
			SQLLEN pcbValue;
			
			SQLDescribeCol(_stmt, x, name, sizeof(name), &NameLength, &DataType, &ColumnSize, &DecimalDigits, &Nullable);
			SQLGetData(_stmt, x, SQL_C_CHAR, _colbuf, _cblen, &pcbValue);

			if (name) {
				if (SQL_NULL_DATA == pcbValue) {
					arg->Set(String::NewFromUtf8(GetIsolate(), (const char *)name), Null(info.GetIsolate()));
				} else {
	                arg->Set(String::NewFromUtf8(GetIsolate(), (const char *)name), String::NewFromUtf8(GetIsolate(), data ? (const char *)data : ""));
				}
			}
		}

		info.GetReturnValue().Set(arg);
		return;
	}

  done:

	info.GetReturnValue().Set(false);
}

JS_ODBC_FUNCTION_IMPL_STATIC(Close)
{
	JS_CHECK_SCRIPT_STATE();
	HandleScope handle_scope(info.GetIsolate());
	FSODBC *obj = JSBase::GetInstance<FSODBC>(info);

	if (obj) {
		delete obj;
	}
}

JS_ODBC_FUNCTION_IMPL(Disconnect)
{
	HandleScope handle_scope(info.GetIsolate());

	if (switch_odbc_handle_get_state(_handle) != SWITCH_ODBC_STATE_CONNECTED) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "Database is not connected!\n");
		return;
	}

	if (_stmt) {
		SQLFreeHandle(SQL_HANDLE_STMT, _stmt);
		_stmt = NULL;
	}

	switch_odbc_handle_disconnect(_handle);
}

JS_ODBC_GET_PROPERTY_IMPL(GetProperty)
{
	HandleScope handle_scope(info.GetIsolate());

	String::Utf8Value str(property);

	if (!strcmp(js_safe_str(*str), "name")) {
		info.GetReturnValue().Set(String::NewFromUtf8(info.GetIsolate(), _dsn.length() > 0 ? _dsn.c_str() : ""));
	} else {
		info.GetIsolate()->ThrowException(String::NewFromUtf8(info.GetIsolate(), "Bad property"));
	}
}

static const js_function_t odbc_methods[] = {
	{"connect", FSODBC::Connect},
	{"disconnect", FSODBC::Disconnect},
	{"exec", FSODBC::Exec},				// Deprecated
	{"query", FSODBC::Exec},
	{"execute", FSODBC::Execute},
	{"numRows", FSODBC::NumRows},
	{"numCols", FSODBC::NumCols},
	{"nextRow", FSODBC::NextRow},
	{"getData", FSODBC::GetData},
	{"close", FSODBC::Close},
	{0}
};

static const js_property_t odbc_props[] = {
	{"name", FSODBC::GetProperty, JSBase::DefaultSetProperty},
	{0}
};

static const js_class_definition_t odbc_desc = {
	js_class_name,
	FSODBC::Construct,
	odbc_methods,
	odbc_props
};

static switch_status_t odbc_load(const v8::FunctionCallbackInfo<Value>& info)
{
	JSBase::Register(info.GetIsolate(), &odbc_desc);
	return SWITCH_STATUS_SUCCESS;
}

static const v8_mod_interface_t odbc_module_interface = {
	/*.name = */ js_class_name,
	/*.js_mod_load */ odbc_load
};

const v8_mod_interface_t *FSODBC::GetModuleInterface()
{
	return &odbc_module_interface;
}

#endif /* HAVE_ODBC */

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
