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
 * Andrey Volk <andywolk@gmail.com>
 * Portions created by the Initial Developer are Copyright (C)
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 * Peter Olsson <peter@olssononline.se>
 * Andrey Volk <andywolk@gmail.com>
 *
 * fsdbh.cpp -- JavaScript DBH class
 *
 */

#include "fsdbh.hpp"

using namespace std;
using namespace v8;

static const char js_class_name[] = "DBH";

FSDBH::~FSDBH()
{
	if (dbh) _release();
	clear_error();
}

bool FSDBH::_release()
{
	if (dbh) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "DBH handle %p released.\n", (void *)dbh);
		switch_cache_db_release_db_handle(&dbh);
		return true;
	}

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "DBH NOT Connected.\n");
	return false;
}

string FSDBH::GetJSClassName()
{
	return js_class_name;
}

void FSDBH::Init()
{
	dbh = NULL;
}

FSDBH *FSDBH::New(char *dsn, char *user, char *pass, const v8::FunctionCallbackInfo<Value>& info)
{
	FSDBH *new_obj = NULL;

	char *tmp = NULL;

	if (!(new_obj = new FSDBH(info))) {
		goto err;
	}

	new_obj->dbh = NULL;
	new_obj->err = NULL;

	if (!zstr(user) || !zstr(pass)) {
		tmp = switch_mprintf("%s%s%s%s%s", dsn,
			zstr(user) ? "" : ":",
			zstr(user) ? "" : user,
			zstr(pass) ? "" : ":",
			zstr(pass) ? "" : pass
		);

		dsn = tmp;
	}

	if (!zstr(dsn) && switch_cache_db_get_db_handle_dsn(&(new_obj->dbh), dsn) == SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "DBH handle %p Connected.\n", (void *)(new_obj->dbh));
	}
	else {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Connection failed.  DBH NOT Connected.\n");
		goto err;
	}

	new_obj->_dsn = dsn;
	return new_obj;

  err:
	if (new_obj) {
		delete new_obj;
	}

	return NULL;
}

void *FSDBH::Construct(const v8::FunctionCallbackInfo<Value>& info)
{
	FSDBH *dbh_obj = NULL;
	char *dsn, *username = NULL, *password = NULL;

	if (info.Length() < 1 || info.Length() > 3) {
		info.GetIsolate()->ThrowException(String::NewFromUtf8(info.GetIsolate(), "Invalid parameters"));
		return NULL;
	}

	String::Utf8Value str1(info[0]);
	dsn = *str1;

	if (info.Length() > 1)
	{
		String::Utf8Value str2(info[1]);
		username = *str2;
	}

	if (info.Length() > 2)
	{
		String::Utf8Value str3(info[2]);
		password = *str3;
	}

	if (zstr(username)) {
		username = NULL;
	}

	if (zstr(password)) {
		password = NULL;
	}

	if (dsn) {
		dbh_obj = New(dsn, username, password, info);
	}

	if (!dbh_obj) {
		info.GetIsolate()->ThrowException(String::NewFromUtf8(info.GetIsolate(), "Failed to create new DBH instance"));
		return NULL;
	}

	return dbh_obj;
}

int FSDBH::Callback(void *pArg, int argc, char **argv, char **columnNames)
{
	FSDBH *dbo = static_cast<FSDBH *>(pArg);
	int x = 0;

	if (!dbo) {
		return 0;
	}

	HandleScope handle_scope(dbo->GetIsolate());

	if (dbo->_callback.IsEmpty()) {
		dbo->GetIsolate()->ThrowException(String::NewFromUtf8(dbo->GetIsolate(), "No callback specified"));
		return 0;
	}

	Handle<Array> arg = Array::New(dbo->GetIsolate(), argc);

	for (x = 0; x < argc; x++) {
		if (columnNames[x] && argv[x]) {
			arg->Set(String::NewFromUtf8(dbo->GetIsolate(), columnNames[x]), String::NewFromUtf8(dbo->GetIsolate(), argv[x]));
		}
	}

	HandleScope scope(dbo->GetIsolate());
	Handle<Function> func = Local<Function>::New(dbo->GetIsolate(), dbo->_callback);
	Handle<Value> jsargv[1] = { arg };

	func->Call(dbo->GetIsolate()->GetCurrentContext()->Global(), 1, jsargv);

	return 0;
}

void FSDBH::clear_error()
{
	switch_safe_free(err);
}

JS_DBH_FUNCTION_IMPL(last_error)
{
	if (err)
		info.GetReturnValue().Set(String::NewFromUtf8(info.GetIsolate(), err));
	else
		info.GetReturnValue().Set(String::NewFromUtf8(info.GetIsolate(), ""));
	return;
}

JS_DBH_FUNCTION_IMPL(query)
{
	HandleScope handle_scope(info.GetIsolate());

	info.GetReturnValue().Set(true);

	clear_error();

	if (info.Length() < 1 || info.Length() > 2) {
		info.GetIsolate()->ThrowException(String::NewFromUtf8(info.GetIsolate(), "Invalid parameters"));
		return info.GetReturnValue().Set(false);
	}

	if (!dbh) {
		info.GetIsolate()->ThrowException(String::NewFromUtf8(info.GetIsolate(), "Database is not connected"));
		return info.GetReturnValue().Set(false);
	}

	String::Utf8Value str(info[0]);
	const char *sql = js_safe_str(*str);

	if (zstr(sql)) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Missing SQL query.\n");
		return info.GetReturnValue().Set(false);
	}

	void *arg = NULL;
	switch_core_db_callback_func_t cb_func = NULL;

	Handle<Function> func = Handle<Function>();

	if (info.Length() > 1)
	    func = JSBase::GetFunctionFromArg(info.GetIsolate(), info[1]);

	if (!func.IsEmpty()) {
		_callback.Reset(info.GetIsolate(), func);
		cb_func = FSDBH::Callback;
		arg = this;
	}

	if (dbh) {
		if (!func.IsEmpty()) {
			if (switch_cache_db_execute_sql_callback(dbh, sql, cb_func, arg, &err) == SWITCH_STATUS_SUCCESS) {
				return;
			}
		}
		else { /* if no callback func arg is passed from javascript, an empty initialized struct will be sent - see freeswitch.i */
			if (switch_cache_db_execute_sql(dbh, (char *)sql, &err) == SWITCH_STATUS_SUCCESS) {
				return;
			}
		}

		if (err) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Error %s\n", err);
			switch_safe_free(err);
			info.GetReturnValue().Set(false);
		}
	}
	else {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "DBH NOT Connected.\n");
		info.GetReturnValue().Set(false);
	}

	/* Make sure to release the callback handle again */
	_callback.Reset();

	return;
}

JS_DBH_FUNCTION_IMPL(release)
{
	return info.GetReturnValue().Set(_release());
}

JS_DBH_FUNCTION_IMPL(test_reactive)
{
	HandleScope handle_scope(info.GetIsolate());

	info.GetReturnValue().Set(true);

	clear_error();

	if (info.Length() < 3) {
		info.GetIsolate()->ThrowException(String::NewFromUtf8(info.GetIsolate(), "Invalid parameters"));
		return info.GetReturnValue().Set(false);
	}

	if (!dbh) {
		info.GetIsolate()->ThrowException(String::NewFromUtf8(info.GetIsolate(), "Database is not connected"));
		return info.GetReturnValue().Set(false);
	}

	String::Utf8Value str0(info[0]);
	String::Utf8Value str1(info[1]);
	String::Utf8Value str2(info[2]);
	const char *test_sql = js_safe_str(*str0);
	const char *drop_sql = js_safe_str(*str1);
	const char *reactive_sql = js_safe_str(*str2);

	if (dbh) {
		if (!zstr(test_sql) && !zstr(reactive_sql)) {
			if (switch_cache_db_test_reactive(dbh, test_sql, drop_sql, reactive_sql) == SWITCH_TRUE) {
				return;
			}
		}
		else {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Missing parameters.\n");
			return info.GetReturnValue().Set(false);
		}
	}
	else {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "DBH NOT Connected.\n");
		return info.GetReturnValue().Set(false);
	}
	return;
}

JS_DBH_FUNCTION_IMPL(connected)
{
	return info.GetReturnValue().Set(dbh ? true : false);
}

JS_DBH_FUNCTION_IMPL(affected_rows)
{
	if (dbh) {
		return info.GetReturnValue().Set(switch_cache_db_affected_rows(dbh));
	}

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "DBH is NOT Connected.\n");
	return info.GetReturnValue().Set(0);
}

JS_DBH_GET_PROPERTY_IMPL(GetProperty)
{
	HandleScope handle_scope(info.GetIsolate());

	String::Utf8Value str(property);

	if (!strcmp(js_safe_str(*str), "dsn")) {
			info.GetReturnValue().Set(String::NewFromUtf8(info.GetIsolate(), _dsn.c_str()));
	}
	else {
		info.GetIsolate()->ThrowException(String::NewFromUtf8(info.GetIsolate(), "Bad property"));
	}
}

static const js_function_t dbh_methods[] = {
	{ "affected_rows", FSDBH::affected_rows },
	{ "connected", FSDBH::connected },
	{ "last_error", FSDBH::last_error },
	{ "query", FSDBH::query },
	{ "release", FSDBH::release },
	{ "test_reactive", FSDBH::test_reactive },
	{0}
};

static const js_property_t dbh_props[] = {
	{"dsn", FSDBH::GetProperty, JSBase::DefaultSetProperty},
	{0}
};

static const js_class_definition_t dbh_desc = {
	js_class_name,
	FSDBH::Construct,
	dbh_methods,
	dbh_props
};

static switch_status_t dbh_load(const v8::FunctionCallbackInfo<Value>& info)
{
	JSBase::Register(info.GetIsolate(), &dbh_desc);
	return SWITCH_STATUS_SUCCESS;
}

static const v8_mod_interface_t dbh_module_interface = {
	/*.name = */ js_class_name,
	/*.js_mod_load */ dbh_load
};

const v8_mod_interface_t *FSDBH::GetModuleInterface()
{
	return &dbh_module_interface;
}

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
