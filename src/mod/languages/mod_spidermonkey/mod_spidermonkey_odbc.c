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

static const char modname[] = "ODBC";

struct odbc_obj {
	switch_odbc_handle_t *handle;
	SQLHSTMT stmt;
	SQLCHAR *colbuf;
	int32 cblen;
	SQLCHAR *code;
	int32 codelen;
};
typedef struct odbc_obj odbc_obj_t;

static odbc_obj_t *new_odbc_obj(char *dsn, char *username, char *password)
{
	odbc_obj_t *new_obj;

	if (!(new_obj = malloc(sizeof(*new_obj)))) {
		goto err;
	}

	memset(new_obj, 0, sizeof(odbc_obj_t));
	if (!(new_obj->handle = switch_odbc_handle_new(dsn, username, password))) {
		goto err;
	}

	return new_obj;

  err:
	if (new_obj) {
		if (new_obj->handle) {
			switch_odbc_handle_destroy(&new_obj->handle);
		}
		switch_safe_free(new_obj);
	}

	return NULL;
}

switch_odbc_status_t odbc_obj_connect(odbc_obj_t *obj)
{

	return switch_odbc_handle_connect(obj->handle);
}

static void destroy_odbc_obj(odbc_obj_t ** objp)
{
	odbc_obj_t *obj = *objp;
	if (obj == NULL)
		return;
	if (obj->stmt) {
		SQLFreeHandle(SQL_HANDLE_STMT, obj->stmt);
	}
	if (obj->handle) {
		switch_odbc_handle_destroy(&obj->handle);
	}
	switch_safe_free(obj->colbuf);
	switch_safe_free(obj->code);
	switch_safe_free(obj);
}


/* ODBC Object */
/*********************************************************************************/
static JSBool odbc_construct(JSContext * cx, JSObject * obj, uintN argc, jsval * argv, jsval * rval)
{
	odbc_obj_t *odbc_obj = NULL;
	char *dsn, *username, *password;
	int32 blen = 1024;

	if (argc < 3) {
		return JS_FALSE;
	}

	dsn = JS_GetStringBytes(JS_ValueToString(cx, argv[0]));
	username = JS_GetStringBytes(JS_ValueToString(cx, argv[1]));
	password = JS_GetStringBytes(JS_ValueToString(cx, argv[2]));

	if (argc > 3) {
		int32 len;
		JS_ValueToInt32(cx, argv[3], &len);

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
		odbc_obj = new_odbc_obj(dsn, username, password);
	}

	if (!odbc_obj) {
		return JS_FALSE;
	}

	if (!(odbc_obj->colbuf = (SQLCHAR *) malloc(blen))) {
		destroy_odbc_obj(&odbc_obj);
		return JS_FALSE;
	}

	odbc_obj->cblen = blen;

	blen += 1536;

	if (!(odbc_obj->code = (SQLCHAR *) malloc(blen))) {
		destroy_odbc_obj(&odbc_obj);
		return JS_FALSE;
	}

	odbc_obj->codelen = blen;

	JS_SetPrivate(cx, obj, odbc_obj);

	return JS_TRUE;
}

static void odbc_destroy(JSContext * cx, JSObject * obj)
{
	odbc_obj_t *odbc_obj;
	if (obj == NULL)
		return;
	odbc_obj = (odbc_obj_t *) JS_GetPrivate(cx, obj);
	if (odbc_obj) {
		destroy_odbc_obj(&odbc_obj);
		JS_SetPrivate(cx, obj, NULL);
	}
}

static JSBool odbc_connect(JSContext * cx, JSObject * obj, uintN argc, jsval * argv, jsval * rval)
{
	odbc_obj_t *odbc_obj = (odbc_obj_t *) JS_GetPrivate(cx, obj);
	JSBool tf = JS_TRUE;

	if (odbc_obj) {
		if (odbc_obj_connect(odbc_obj) == SWITCH_ODBC_SUCCESS) {
			tf = JS_TRUE;
		} else {
			tf = JS_FALSE;
		}
	} else {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Database handle is not initialized!\n");
	}

	*rval = BOOLEAN_TO_JSVAL(tf);

	return JS_TRUE;
}

static JSBool odbc_execute(JSContext * cx, JSObject * obj, uintN argc, jsval * argv, jsval * rval)
{
	odbc_obj_t *odbc_obj = (odbc_obj_t *) JS_GetPrivate(cx, obj);
	char *sql;
	JSBool tf = JS_FALSE;
	SQLHSTMT stmt;

	if (argc < 1) {
		goto done;
	}

	if (!odbc_obj || switch_odbc_handle_get_state(odbc_obj->handle) != SWITCH_ODBC_STATE_CONNECTED) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Database is not connected!\n");
		goto done;
	}

	sql = JS_GetStringBytes(JS_ValueToString(cx, argv[0]));

	if (switch_odbc_handle_exec(odbc_obj->handle, sql, &stmt, NULL) != SWITCH_ODBC_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "[ODBC] Execute failed for: %s\n", sql);
		goto done;
	}

	SQLFreeHandle(SQL_HANDLE_STMT, stmt);

	tf = JS_TRUE;

  done:

	*rval = BOOLEAN_TO_JSVAL(tf);

	return JS_TRUE;
}

static JSBool odbc_exec(JSContext * cx, JSObject * obj, uintN argc, jsval * argv, jsval * rval)
{
	odbc_obj_t *odbc_obj = (odbc_obj_t *) JS_GetPrivate(cx, obj);
	char *sql;
	JSBool tf = JS_FALSE;

	if (argc < 1) {
		goto done;
	}

	if (!odbc_obj || switch_odbc_handle_get_state(odbc_obj->handle) != SWITCH_ODBC_STATE_CONNECTED) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Database is not connected!\n");
		goto done;
	}

	if (odbc_obj->stmt) {
		SQLFreeHandle(SQL_HANDLE_STMT, odbc_obj->stmt);
		odbc_obj->stmt = NULL;
	}

	sql = JS_GetStringBytes(JS_ValueToString(cx, argv[0]));

	if (switch_odbc_handle_exec(odbc_obj->handle, sql, &odbc_obj->stmt, NULL) != SWITCH_ODBC_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "[ODBC] query failed: %s\n", sql);
		goto done;
	}

	tf = JS_TRUE;

  done:

	*rval = BOOLEAN_TO_JSVAL(tf);

	return JS_TRUE;
}

static JSBool odbc_num_rows(JSContext * cx, JSObject * obj, uintN argc, jsval * argv, jsval * rval)
{
	odbc_obj_t *odbc_obj = (odbc_obj_t *) JS_GetPrivate(cx, obj);
	SQLLEN row_count = 0;

	if (!odbc_obj || switch_odbc_handle_get_state(odbc_obj->handle) != SWITCH_ODBC_STATE_CONNECTED) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Database is not connected!\n");
		goto done;
	}

	if (odbc_obj->stmt) {
		SQLRowCount(odbc_obj->stmt, &row_count);
	}

  done:

	*rval = INT_TO_JSVAL(row_count);

	return JS_TRUE;

}


static JSBool odbc_num_cols(JSContext * cx, JSObject * obj, uintN argc, jsval * argv, jsval * rval)
{
	odbc_obj_t *odbc_obj = (odbc_obj_t *) JS_GetPrivate(cx, obj);

	SQLSMALLINT cols = 0;

	if (!odbc_obj || switch_odbc_handle_get_state(odbc_obj->handle) != SWITCH_ODBC_STATE_CONNECTED) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Database is not connected!\n");
		goto done;
	}

	if (odbc_obj->stmt) {
		SQLNumResultCols(odbc_obj->stmt, &cols);
	}

  done:

	*rval = INT_TO_JSVAL(cols);

	return JS_TRUE;

}


static JSBool odbc_next_row(JSContext * cx, JSObject * obj, uintN argc, jsval * argv, jsval * rval)
{
	odbc_obj_t *odbc_obj = (odbc_obj_t *) JS_GetPrivate(cx, obj);
	int result = 0;
	JSBool tf = JS_FALSE;

	if (!odbc_obj || switch_odbc_handle_get_state(odbc_obj->handle) != SWITCH_ODBC_STATE_CONNECTED) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Database is not connected!\n");
		goto done;
	}


	if (odbc_obj->stmt) {
		if ((result = SQLFetch(odbc_obj->stmt) == SQL_SUCCESS)) {
			tf = JS_TRUE;
		}
	}

  done:

	*rval = BOOLEAN_TO_JSVAL(tf);

	return JS_TRUE;
}

static char *escape_data(char *in, char escapeChar)
{
	switch_size_t nlen = strlen(in);
	uint32_t qc = 0;
	char *p, *q, *r;

	for (p = in; p && *p; p++) {
		if (*p == '"') {
			qc += 2;
		}
		if (*p == '\'') {
			qc += 2;
		}
	}

	nlen += qc + 1;

	if (!(q = (char *) malloc(nlen))) {
		return NULL;
	}

	r = q;
	qc = 0;
	for (p = in; p && *p; p++) {
		if (*p == '"') {
			*r++ = escapeChar;
		}
		if (*p == '\'') {
			*r++ = escapeChar;
		}
		*r++ = *p;
		if (++qc > nlen) {
			break;
		}
	}

	*r++ = '\0';

	return q;

}


static JSBool odbc_get_data(JSContext * cx, JSObject * obj, uintN argc, jsval * argv, jsval * rval)
{

	odbc_obj_t *odbc_obj = (odbc_obj_t *) JS_GetPrivate(cx, obj);
	JSBool tf = JS_FALSE;

	if (!odbc_obj || switch_odbc_handle_get_state(odbc_obj->handle) != SWITCH_ODBC_STATE_CONNECTED) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Database is not connected!\n");
		goto done;
	}

	if (odbc_obj->stmt) {
		SQLSMALLINT nColumns = 0, x = 0;

		eval_some_js("~var _oDbC_dB_RoW_DaTa_ = {}", cx, obj, rval);
		if (*rval == JS_FALSE) {
			return JS_TRUE;
		}

		if (SQLNumResultCols(odbc_obj->stmt, &nColumns) != SQL_SUCCESS)
			return JS_FALSE;

		for (x = 1; x <= nColumns; x++) {
			SQLSMALLINT NameLength, DataType, DecimalDigits, Nullable;
			SQLULEN ColumnSize;
			SQLCHAR name[1024] = "";
			SQLCHAR *data = odbc_obj->colbuf;
			SQLCHAR *esc = NULL;

			SQLDescribeCol(odbc_obj->stmt, x, name, sizeof(name), &NameLength, &DataType, &ColumnSize, &DecimalDigits, &Nullable);
			SQLGetData(odbc_obj->stmt, x, SQL_C_CHAR, odbc_obj->colbuf, odbc_obj->cblen, NULL);

			if (strchr((char *) odbc_obj->colbuf, '"')) {	/* please don't */
				esc = (SQLCHAR *) escape_data((char *) odbc_obj->colbuf, '\\');
				data = esc;
			}

			switch_snprintf((char *) odbc_obj->code, odbc_obj->codelen, "~_oDbC_dB_RoW_DaTa_[\"%s\"] = \"%s\"", name, data);
			switch_safe_free(esc);

			eval_some_js((char *) odbc_obj->code, cx, obj, rval);

			if (*rval == JS_FALSE) {
				return JS_TRUE;
			}
		}

		JS_GetProperty(cx, obj, "_oDbC_dB_RoW_DaTa_", rval);
		return JS_TRUE;


	}

  done:

	*rval = BOOLEAN_TO_JSVAL(tf);

	return JS_TRUE;

}

static JSBool odbc_close(JSContext * cx, JSObject * obj, uintN argc, jsval * argv, jsval * rval)
{
	odbc_destroy(cx, obj);
	return JS_TRUE;
}


static JSBool odbc_disconnect(JSContext * cx, JSObject * obj, uintN argc, jsval * argv, jsval * rval)
{
	odbc_obj_t *odbc_obj = (odbc_obj_t *) JS_GetPrivate(cx, obj);

	if (!odbc_obj) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Database handle is not initialized!\n");
		goto done;
	}

	if (switch_odbc_handle_get_state(odbc_obj->handle) != SWITCH_ODBC_STATE_CONNECTED) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Database is not connected!\n");
		goto done;
	}

	if (odbc_obj->stmt) {
		SQLFreeHandle(SQL_HANDLE_STMT, odbc_obj->stmt);
		odbc_obj->stmt = NULL;
	}

	switch_odbc_handle_disconnect(odbc_obj->handle);

  done:

	return JS_TRUE;
}

enum odbc_tinyid {
	odbc_NAME
};

static JSFunctionSpec odbc_methods[] = {
	{"connect", odbc_connect, 1},
	{"disconnect", odbc_disconnect, 1},
	{"exec", odbc_exec, 1},
	{"query", odbc_exec, 1},
	{"execute", odbc_execute, 1},
	{"numRows", odbc_num_rows, 1},
	{"numCols", odbc_num_cols, 1},
	{"nextRow", odbc_next_row, 1},
	{"getData", odbc_get_data, 1},
	{"close", odbc_close, 1},
	{0}
};


static JSPropertySpec odbc_props[] = {
	{"name", odbc_NAME, JSPROP_READONLY | JSPROP_PERMANENT},
	{0}
};

static JSBool odbc_setProperty(JSContext * cx, JSObject * obj, jsval id, jsval * vp)
{
	char *name = JS_GetStringBytes(JS_ValueToString(cx, id));

	if (strcmp(name, "_oDbC_dB_RoW_DaTa_")) {
		eval_some_js("~throw new Error(\"this property cannot be changed!\");", cx, obj, vp);
		*vp = BOOLEAN_TO_JSVAL(JS_FALSE);
	}
	return JS_TRUE;
}

static JSBool odbc_getProperty(JSContext * cx, JSObject * obj, jsval id, jsval * vp)
{
	int param;
	char *name = JS_GetStringBytes(JS_ValueToString(cx, id));

	/* numbers are our props anything else is a method */
	if (name[0] >= 48 && name[0] <= 57) {
		param = atoi(name);
	} else {
		return JS_TRUE;
	}

	*vp = BOOLEAN_TO_JSVAL(JS_FALSE);
	return JS_TRUE;
}

JSClass odbc_class = {
	modname, JSCLASS_HAS_PRIVATE,
	JS_PropertyStub, JS_PropertyStub, odbc_getProperty, odbc_setProperty,
	JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub, odbc_destroy, NULL, NULL, NULL,
	odbc_construct
};


switch_status_t odbc_load(JSContext * cx, JSObject * obj)
{
	JS_InitClass(cx, obj, NULL, &odbc_class, odbc_construct, 3, odbc_props, odbc_methods, odbc_props, odbc_methods);
	return SWITCH_STATUS_SUCCESS;
}


const sm_module_interface_t odbc_module_interface = {
	/*.name = */ modname,
	/*.spidermonkey_load */ odbc_load,
	/*.next */ NULL
};

SWITCH_MOD_DECLARE_NONSTD(switch_status_t) spidermonkey_init(const sm_module_interface_t ** module_interface)
{
	*module_interface = &odbc_module_interface;
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
