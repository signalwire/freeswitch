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

static const char modname[] = "ODBC";

struct odbc_obj {
	char *dsn;
	char *username;
	char *password;
	SQLHENV env;
	SQLHDBC con;
	SQLHSTMT stmt;
	uint32_t state;
	SQLCHAR *colbuf;
	int32 cblen;
	SQLCHAR *code;
	int32 codelen;
};

typedef enum {
	ODBC_STATE_INIT,
	ODBC_STATE_DOWN,
	ODBC_STATE_CONNECTED,
	ODBC_STATE_ERROR
} odbc_state_t;
typedef struct odbc_obj odbc_obj_t;

typedef enum {
	ODBC_SUCCESS = 0,
	ODBC_FAIL = -1
} odbc_status_t;


static odbc_obj_t *new_odbc_obj(char *dsn, char *username, char *password)
{
	odbc_obj_t *new_obj;

	if (!(new_obj = malloc(sizeof(*new_obj)))) {
		goto err;
	}

	if (!(new_obj->dsn = strdup(dsn))) {
		goto err;
	}

	if (!(new_obj->username = strdup(username))) {
		goto err;
	}

	if (!(new_obj->password = strdup(password))) {
		goto err;
	}

	new_obj->env = SQL_NULL_HANDLE;
	new_obj->state = ODBC_STATE_INIT;

	return new_obj;

  err:
	if (new_obj) {
		switch_safe_free(new_obj->dsn);
		switch_safe_free(new_obj->username);
		switch_safe_free(new_obj->password);
		switch_safe_free(new_obj);
	}

	return NULL;
}

odbc_status_t odbc_obj_disconnect(odbc_obj_t * obj)
{
	int result;

	if (obj->state == ODBC_STATE_CONNECTED) {
		result = SQLDisconnect(obj->con);
		if (result == ODBC_SUCCESS) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Disconnected %d from [%s]\n", result, obj->dsn);
		} else {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Error Disconnectiong [%s]\n", obj->dsn);
		}
	} else {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "[%s] already disconnected\n", obj->dsn);
	}

	obj->state = ODBC_STATE_DOWN;

	return ODBC_SUCCESS;
}

odbc_status_t odbc_obj_connect(odbc_obj_t * obj)
{
	int result;
	SQLINTEGER err;
	int16_t mlen;
	unsigned char msg[200], stat[10];

	if (obj->env == SQL_NULL_HANDLE) {
		result = SQLAllocHandle(SQL_HANDLE_ENV, SQL_NULL_HANDLE, &obj->env);

		if ((result != SQL_SUCCESS) && (result != SQL_SUCCESS_WITH_INFO)) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Error AllocHandle\n");
			return ODBC_FAIL;
		}

		result = SQLSetEnvAttr(obj->env, SQL_ATTR_ODBC_VERSION, (void *) SQL_OV_ODBC3, 0);

		if ((result != SQL_SUCCESS) && (result != SQL_SUCCESS_WITH_INFO)) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Error SetEnv\n");
			SQLFreeHandle(SQL_HANDLE_ENV, obj->env);
			return ODBC_FAIL;
		}

		result = SQLAllocHandle(SQL_HANDLE_DBC, obj->env, &obj->con);

		if ((result != SQL_SUCCESS) && (result != SQL_SUCCESS_WITH_INFO)) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Error AllocHDB %d\n", result);
			SQLFreeHandle(SQL_HANDLE_ENV, obj->env);
			return ODBC_FAIL;
		}
		SQLSetConnectAttr(obj->con, SQL_LOGIN_TIMEOUT, (SQLPOINTER *) 10, 0);
	}
	if (obj->state == ODBC_STATE_CONNECTED) {
		odbc_obj_disconnect(obj);
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Re-connecting %s\n", obj->dsn);
	}

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Connecting %s\n", obj->dsn);

	result = SQLConnect(obj->con, (SQLCHAR *) obj->dsn, SQL_NTS, (SQLCHAR *) obj->username, SQL_NTS, (SQLCHAR *) obj->password, SQL_NTS);

	if ((result != SQL_SUCCESS) && (result != SQL_SUCCESS_WITH_INFO)) {
		SQLGetDiagRec(SQL_HANDLE_DBC, obj->con, 1, stat, &err, msg, 100, &mlen);
		SQLFreeHandle(SQL_HANDLE_ENV, obj->env);
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Error SQLConnect=%d errno=%d %s\n", result, (int) err, msg);
		return ODBC_FAIL;
	} else {

		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Connected to [%s]\n", obj->dsn);
		obj->state = ODBC_STATE_CONNECTED;
	}

	return ODBC_SUCCESS;
}

static void destroy_odbc_obj(odbc_obj_t ** objp)
{
	odbc_obj_t *obj = *objp;

	odbc_obj_disconnect(obj);

	SQLFreeHandle(SQL_HANDLE_STMT, obj->stmt);
	SQLFreeHandle(SQL_HANDLE_DBC, obj->con);
	SQLFreeHandle(SQL_HANDLE_ENV, obj->env);

	switch_safe_free(obj->dsn);
	switch_safe_free(obj->username);
	switch_safe_free(obj->password);
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

	if (dsn && username && password) {
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
	odbc_obj_t *odbc_obj = (odbc_obj_t *) JS_GetPrivate(cx, obj);

	if (odbc_obj) {
		destroy_odbc_obj(&odbc_obj);
	}
}

static JSBool odbc_connect(JSContext * cx, JSObject * obj, uintN argc, jsval * argv, jsval * rval)
{
	odbc_obj_t *odbc_obj = (odbc_obj_t *) JS_GetPrivate(cx, obj);
	JSBool tf = JS_TRUE;

	if (odbc_obj) {
		if (odbc_obj_connect(odbc_obj) == ODBC_SUCCESS) {
			tf = JS_TRUE;
		} else {
			tf = JS_FALSE;
		}
	}

	*rval = BOOLEAN_TO_JSVAL(tf);

	return JS_TRUE;
}

static JSBool odbc_exec(JSContext * cx, JSObject * obj, uintN argc, jsval * argv, jsval * rval)
{
	odbc_obj_t *odbc_obj = (odbc_obj_t *) JS_GetPrivate(cx, obj);
	char *sql;
	JSBool tf = JS_FALSE;
	int result;

	if (argc < 1) {
		goto done;
	}

	if (odbc_obj->state != ODBC_STATE_CONNECTED) {
		goto done;
	}

	if (odbc_obj->stmt) {
		SQLFreeHandle(SQL_HANDLE_STMT, odbc_obj->stmt);
		odbc_obj->stmt = NULL;
	}

	sql = JS_GetStringBytes(JS_ValueToString(cx, argv[0]));

	if (SQLAllocHandle(SQL_HANDLE_STMT, odbc_obj->con, &odbc_obj->stmt) != SQL_SUCCESS) {
		goto done;
	}

	if (SQLPrepare(odbc_obj->stmt, (unsigned char *) sql, SQL_NTS) != SQL_SUCCESS) {
		goto done;
	}

	result = SQLExecute(odbc_obj->stmt);

	if (result != SQL_SUCCESS && result != SQL_SUCCESS_WITH_INFO) {
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

	SQLSMALLINT rows = 0;

	if (odbc_obj->state != ODBC_STATE_CONNECTED) {
		goto done;
	}

	if (odbc_obj->stmt) {
		SQLNumResultCols(odbc_obj->stmt, &rows);
	}

  done:

	*rval = INT_TO_JSVAL(rows);

	return JS_TRUE;

}


static JSBool odbc_next_row(JSContext * cx, JSObject * obj, uintN argc, jsval * argv, jsval * rval)
{
	odbc_obj_t *odbc_obj = (odbc_obj_t *) JS_GetPrivate(cx, obj);
	int result = 0;
	JSBool tf = JS_FALSE;

	if (odbc_obj->state != ODBC_STATE_CONNECTED) {
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

static char *escape_data(char *in)
{
	switch_size_t nlen = strlen(in);
	uint32_t qc = 0;
	char *p, *q, *r;

	for (p = in; p && *p; p++) {
		if (*p == '"') {
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
			*r++ = '\\';
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

	if (odbc_obj->state != ODBC_STATE_CONNECTED) {
		goto done;
	}

	if (odbc_obj->stmt) {
		SQLSMALLINT c = 0, x = 0;
		int result;
		char code[66560];

		snprintf(code, sizeof(code), "~var _oDbC_dB_RoW_DaTa_ = {}");
		eval_some_js(code, cx, obj, rval);
		if (*rval == JS_FALSE) {
			return JS_TRUE;
		}

		result = SQLNumResultCols(odbc_obj->stmt, &c);
		if (result == SQL_SUCCESS || result == SQL_SUCCESS_WITH_INFO) {
			for (x = 1; x <= c; x++) {
				SQLSMALLINT NameLength, DataType, DecimalDigits, Nullable;
				SQLUINTEGER ColumnSize;
				SQLCHAR name[1024] = "";
				SQLCHAR *data = odbc_obj->colbuf;
				SQLCHAR *esc = NULL;

				SQLDescribeCol(odbc_obj->stmt, x, name, sizeof(name), &NameLength, &DataType, &ColumnSize, &DecimalDigits, &Nullable);
				SQLGetData(odbc_obj->stmt, x, SQL_C_CHAR, odbc_obj->colbuf, odbc_obj->cblen, NULL);

				if (strchr((char *) odbc_obj->colbuf, '"')) {	/* please don't */
					esc = (SQLCHAR *) escape_data((char *) odbc_obj->colbuf);
					data = esc;
				}

				snprintf((char *) odbc_obj->code, odbc_obj->codelen, "~_oDbC_dB_RoW_DaTa_[\"%s\"] = \"%s\"", name, data);
				switch_safe_free(esc);

				eval_some_js((char *) odbc_obj->code, cx, obj, rval);

				if (*rval == JS_FALSE) {
					return JS_TRUE;
				}
			}

			JS_GetProperty(cx, obj, "_oDbC_dB_RoW_DaTa_", rval);
			return JS_TRUE;
		}

	}

  done:

	*rval = BOOLEAN_TO_JSVAL(tf);

	return JS_TRUE;

}

enum odbc_tinyid {
	odbc_NAME
};

static JSFunctionSpec odbc_methods[] = {
	{"connect", odbc_connect, 1},
	{"exec", odbc_exec, 1},
	{"numRows", odbc_num_rows, 1},
	{"nextRow", odbc_next_row, 1},
	{"getData", odbc_get_data, 1},
	{0}
};


static JSPropertySpec odbc_props[] = {
//  {"name", odbc_NAME, JSPROP_READONLY|JSPROP_PERMANENT}, 
	{0}
};


static JSBool odbc_getProperty(JSContext * cx, JSObject * obj, jsval id, jsval * vp)
{
	JSBool res = JS_TRUE;

	return res;
}

JSClass odbc_class = {
	modname, JSCLASS_HAS_PRIVATE,
	JS_PropertyStub, JS_PropertyStub, odbc_getProperty, JS_PropertyStub,
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

SWITCH_MOD_DECLARE(switch_status_t) spidermonkey_init(const sm_module_interface_t ** module_interface)
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
 * vim:set softtabstop=4 shiftwidth=4 tabstop=4 expandtab:
 */
