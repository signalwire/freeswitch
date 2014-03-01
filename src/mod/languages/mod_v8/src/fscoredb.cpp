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
 * fscoredb.cpp -- JavaScript CoreDB class
 *
 */

#include "fscoredb.hpp"

using namespace std;
using namespace v8;

static const char js_class_name[] = "CoreDB";

FSCoreDB::~FSCoreDB(void)
{
	_callback.Reset();

	DoClose();
	switch_core_destroy_memory_pool(&_pool);
}

string FSCoreDB::GetJSClassName()
{
	return js_class_name;
}

void FSCoreDB::Init()
{
	_pool = NULL;
	_db = NULL;
	_stmt = NULL;
	_dbname = NULL;
}

void FSCoreDB::DoClose()
{
	if (_stmt) {
		switch_core_db_finalize(_stmt);
		_stmt = NULL;
	}
	if (_db) {
		switch_core_db_close(_db);
		_db = NULL;
	}
}

void *FSCoreDB::Construct(const v8::FunctionCallbackInfo<Value>& info)
{
	switch_memory_pool_t *pool;
	switch_core_db_t *db;

	if (info.Length() > 0) {
		String::Utf8Value str(info[0]);
		const char *dbname = js_safe_str(*str);

		switch_core_new_memory_pool(&pool);

		if (!(db = switch_core_db_open_file(dbname))) {
			switch_core_destroy_memory_pool(&pool);
			info.GetIsolate()->ThrowException(String::NewFromUtf8(info.GetIsolate(), "Cannot Open DB!"));
			return NULL;
		}

		FSCoreDB *dbo = new FSCoreDB(info);
		dbo->_pool = pool;
		dbo->_db = db;
		dbo->_dbname = switch_core_strdup(pool, dbname);

		return dbo;
	}

	info.GetIsolate()->ThrowException(String::NewFromUtf8(info.GetIsolate(), "Invalid arguments"));
	return NULL;
}

int FSCoreDB::Callback(void *pArg, int argc, char **argv, char **columnNames)
{
	FSCoreDB *dbo = static_cast<FSCoreDB *>(pArg);
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
	Handle<Function> func =  Local<Function>::New(dbo->GetIsolate(), dbo->_callback);
	Handle<Value> jsargv[1] = { arg };

	func->Call(dbo->GetIsolate()->GetCurrentContext()->Global(), 1, jsargv);

	return 0;
}

JS_COREDB_FUNCTION_IMPL(Close)
{
	DoClose();
}

JS_COREDB_FUNCTION_IMPL(Exec)
{
	HandleScope handle_scope(info.GetIsolate());

	info.GetReturnValue().Set(0);

	if (!_db) {
		info.GetIsolate()->ThrowException(String::NewFromUtf8(info.GetIsolate(), "Database is not connected"));
		return;
	}

	if (info.Length() > 0) {
		String::Utf8Value str(info[0]);
		const char *sql = js_safe_str(*str);
		char *err = NULL;
		void *arg = NULL;
		switch_core_db_callback_func_t cb_func = NULL;

		if (info.Length() > 1) {
			Handle<Function> func = JSBase::GetFunctionFromArg(info.GetIsolate(), info[1]);
			if (!func.IsEmpty()) {
				_callback.Reset(info.GetIsolate(), func);
				cb_func = FSCoreDB::Callback;
				arg = this;
			}
		}

		switch_core_db_exec(_db, sql, cb_func, arg, &err);

		/* Make sure to release the callback handle again */
		_callback.Reset();

		if (err) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Error %s\n", err);
			switch_core_db_free(err);
			info.GetReturnValue().Set(-1);
		} else {
			info.GetReturnValue().Set(switch_core_db_changes(_db));
		}
	}
}

/* Evaluate a prepared statement
  stepSuccessCode expected success code from switch_core_db_step() 
  return true if step return expected success code, false otherwise
*/
void FSCoreDB::StepEx(const v8::FunctionCallbackInfo<Value>& info, int stepSuccessCode)
{
	HandleScope handle_scope(info.GetIsolate());

	info.GetReturnValue().Set(false);

	if (!_db) {
		info.GetIsolate()->ThrowException(String::NewFromUtf8(info.GetIsolate(), "Database is not connected"));
		return;
	}

	if (_stmt) {
		int running = 1;
		while (running < 5000) {
			int result = switch_core_db_step(_stmt);
			if (result == stepSuccessCode) {
				info.GetReturnValue().Set(true);
				break;
			} else if (result == SWITCH_CORE_DB_BUSY) {
				running++;
				switch_cond_next();	/* wait a bit before retrying */
				continue;
			}
			if (switch_core_db_finalize(_stmt) != SWITCH_CORE_DB_OK) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Error %s\n", switch_core_db_errmsg(_db));
			}
			_stmt = NULL;
			break;
		}
	}
}

/* Evaluate a prepared statement, to be used with statements that return data 
  return true while data is available, false when done or error
*/
JS_COREDB_FUNCTION_IMPL(Next)
{
	/* return true until no more rows available */
	StepEx(info, SWITCH_CORE_DB_ROW);
}

/* Evaluate a prepared statement, to be used with statements that return no data 
  return true if statement has finished executing successfully, false otherwise 
*/
JS_COREDB_FUNCTION_IMPL(Step)
{
	/* return true when the statement has finished executing successfully */
	StepEx(info, SWITCH_CORE_DB_DONE);
}

JS_COREDB_FUNCTION_IMPL(Fetch)
{
	HandleScope handle_scope(info.GetIsolate());
	int colcount;
	int x;

	if (!_db) {
		info.GetIsolate()->ThrowException(String::NewFromUtf8(info.GetIsolate(), "Database is not connected"));
		return;
	}

	if (!_stmt) {
		info.GetIsolate()->ThrowException(String::NewFromUtf8(info.GetIsolate(), "No query is active"));
		return;
	}

	colcount = switch_core_db_column_count(_stmt);
	Handle<Array> arg = Array::New(info.GetIsolate(), colcount);

	for (x = 0; x < colcount; x++) {
		const char *var = (char *) switch_core_db_column_name(_stmt, x);
		const char *val = (char *) switch_core_db_column_text(_stmt, x);

		if (var && val) {
			arg->Set(String::NewFromUtf8(info.GetIsolate(), var), String::NewFromUtf8(info.GetIsolate(), val));
		}
	}

	info.GetReturnValue().Set(arg);
}

JS_COREDB_FUNCTION_IMPL(Prepare)
{
	HandleScope handle_scope(info.GetIsolate());

	info.GetReturnValue().Set(false);

	if (!_db) {
		info.GetIsolate()->ThrowException(String::NewFromUtf8(info.GetIsolate(), "Database is not connected"));
		return;
	}

	if (_stmt) {
		switch_core_db_finalize(_stmt);
		_stmt = NULL;
	}

	if (info.Length() > 0) {
		String::Utf8Value str(info[0]);
		const char *sql = js_safe_str(*str);
		if (switch_core_db_prepare(_db, sql, -1, &_stmt, 0)) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Error %s\n", switch_core_db_errmsg(_db));
		} else {
			info.GetReturnValue().Set(true);
		}
	}
}

JS_COREDB_FUNCTION_IMPL(BindText)
{
	HandleScope handle_scope(info.GetIsolate());
	bool status;
	int32_t param_index = -1;
	string param_value;

	info.GetReturnValue().Set(false);

	if (!_db) {
		info.GetIsolate()->ThrowException(String::NewFromUtf8(info.GetIsolate(), "Database is not connected"));
		return;
	}

	/* db_prepare() must be called first */
	if (!_stmt) {
		info.GetIsolate()->ThrowException(String::NewFromUtf8(info.GetIsolate(), "prepare() must be called first"));
		return;
	}

	if (info.Length() < 2) {
		info.GetIsolate()->ThrowException(String::NewFromUtf8(info.GetIsolate(), "Invalid arguments"));
		return;
	}

	/* convert args */
	status = !info[0].IsEmpty() && info[0]->IsInt32() ? true : false;
	param_index = info[0]->Int32Value();
	String::Utf8Value str(info[1]);
	param_value = js_safe_str(*str);
	if ((param_index < 1) || (param_value.length() == 0)) {
		info.GetIsolate()->ThrowException(String::NewFromUtf8(info.GetIsolate(), "Invalid arguments"));
		return;
	}

	/* bind param */
	if (switch_core_db_bind_text(_stmt, param_index, param_value.c_str(), -1, SWITCH_CORE_DB_TRANSIENT)) {
		char *err = switch_mprintf("Database error %s", switch_core_db_errmsg(_db));
		info.GetIsolate()->ThrowException(String::NewFromUtf8(info.GetIsolate(), err));
		free(err);
		return;
	} else {
		info.GetReturnValue().Set(true);
	}
}

JS_COREDB_FUNCTION_IMPL(BindInt)
{
	HandleScope handle_scope(info.GetIsolate());
	bool status;
	int32_t param_index = -1;
	int32_t param_value = -1;

	info.GetReturnValue().Set(false);

	if (!_db) {
		info.GetIsolate()->ThrowException(String::NewFromUtf8(info.GetIsolate(), "Database is not connected"));
		return;
	}

	/* db_prepare() must be called first */
	if (!_stmt) {
		info.GetIsolate()->ThrowException(String::NewFromUtf8(info.GetIsolate(), "prepare() must be called first"));
		return;
	}

	if (info.Length() < 2) {
		info.GetIsolate()->ThrowException(String::NewFromUtf8(info.GetIsolate(), "Invalid arguments"));
		return;
	}

	/* convert args */
	status = !info[0].IsEmpty() && info[0]->IsInt32() ? true : false;
	param_index = info[0]->Int32Value();

	status = !info[1].IsEmpty() && info[1]->IsInt32() ? true : false;
	param_value = info[1]->Int32Value();

	if (param_index < 1) {
		info.GetIsolate()->ThrowException(String::NewFromUtf8(info.GetIsolate(), "Invalid arguments"));
		return;
	}

	/* bind param */
	if (switch_core_db_bind_int(_stmt, param_index, param_value)) {
		char *err = switch_mprintf("Database error %s", switch_core_db_errmsg(_db));
		info.GetIsolate()->ThrowException(String::NewFromUtf8(info.GetIsolate(), err));
		free(err);
		return;
	} else {
		info.GetReturnValue().Set(true);
	}
}

JS_COREDB_GET_PROPERTY_IMPL(GetProperty)
{
	HandleScope handle_scope(info.GetIsolate());

	if (!this) {
		info.GetReturnValue().Set(false);
		return;
	}

	String::Utf8Value str(property);

	if (!strcmp(js_safe_str(*str), "path")) {
		if (_dbname) {
			info.GetReturnValue().Set(String::NewFromUtf8(info.GetIsolate(), _dbname));
		} else {
			info.GetReturnValue().Set(String::NewFromUtf8(info.GetIsolate(), ""));
		}
	} else {
		info.GetIsolate()->ThrowException(String::NewFromUtf8(info.GetIsolate(), "Bad property"));
	}
}

static const js_function_t db_methods[] = {
	{"exec", FSCoreDB::Exec},
	{"close", FSCoreDB::Close},
	{"next", FSCoreDB::Next},
	{"step", FSCoreDB::Step},
	{"fetch", FSCoreDB::Fetch},
	{"prepare", FSCoreDB::Prepare},
	{"bindText", FSCoreDB::BindText},
	{"bindInt", FSCoreDB::BindInt},
	{0}
};

static const js_property_t db_props[] = {
	{"path", FSCoreDB::GetProperty, JSBase::DefaultSetProperty},
	{0}
};

static const js_class_definition_t db_desc = {
	js_class_name,
	FSCoreDB::Construct,
	db_methods,
	db_props
};

static switch_status_t db_load(const v8::FunctionCallbackInfo<Value>& info)
{
	JSBase::Register(info.GetIsolate(), &db_desc);
	return SWITCH_STATUS_SUCCESS;
}

static const v8_mod_interface_t db_module_interface = {
	/*.name = */ js_class_name,
	/*.js_mod_load */ db_load
};

const v8_mod_interface_t *FSCoreDB::GetModuleInterface()
{
	return &db_module_interface;
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
