/* 
 * Spidermonkey Socket Module
 * Copyright (C) 2007, Jonas Gauffin <jonas.gauffin@gmail.com>
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
 * The Initial Developer of the Original Code is
 * Jonas Gauffin
 * Portions created by the Initial Developer are Copyright (C)
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 * 
 * Jonas Gauffin <jonas.gauffin@gmail.com>
 *
 *
 * mod_spidermonkey_socket.c -- Socket Javascript Module
 *
 */
#include "mod_spidermonkey.h"

static const char modname[] = "Socket";

struct js_socket_obj {
	switch_socket_t *socket;
	switch_memory_pool_t *pool;
	char *read_buffer;
	switch_size_t buffer_size;
	int state;
	jsrefcount saveDepth;
};
typedef struct js_socket_obj js_socket_obj_t;

/* Socket Object */
/*********************************************************************************/
static JSBool socket_construct(JSContext * cx, JSObject * obj, uintN argc, jsval * argv, jsval * rval)
{
	js_socket_obj_t *js_socket_obj = 0;
	switch_memory_pool_t *pool;
	switch_socket_t *socket;
	switch_status_t ret;

	switch_core_new_memory_pool(&pool);
	ret = switch_socket_create(&socket, AF_INET, SOCK_STREAM, SWITCH_PROTO_TCP, pool);
	if (ret != SWITCH_STATUS_SUCCESS) {
		switch_core_destroy_memory_pool(&pool);
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CONSOLE, "Failed to create socket, reason: %d.\n", ret);
		return JS_FALSE;
	}
	// allocate information needed by JS to be able to write to the log.
	// (needed since multitple js sessions can write to the same log)
	js_socket_obj = switch_core_alloc(pool, sizeof(js_socket_obj_t));
	js_socket_obj->pool = pool;
	js_socket_obj->socket = socket;
	JS_SetPrivate(cx, obj, js_socket_obj);
	return JS_TRUE;
}

static void socket_destroy(JSContext * cx, JSObject * obj)
{
	js_socket_obj_t *socket = JS_GetPrivate(cx, obj);
	if (socket == NULL)
		return;

	if (socket->socket != 0) {
		socket->saveDepth = JS_SuspendRequest(cx);
		switch_socket_shutdown(socket->socket, SWITCH_SHUTDOWN_READWRITE);
		switch_socket_close(socket->socket);
		switch_core_destroy_memory_pool(&socket->pool);
		JS_ResumeRequest(cx, socket->saveDepth);
	}

}

static JSBool socket_connect(JSContext * cx, JSObject * obj, uintN argc, jsval * argv, jsval * rval)
{
	js_socket_obj_t *socket = JS_GetPrivate(cx, obj);
	if (socket == NULL) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Failed to find js object.\n");
		return JS_FALSE;
	}

	if (argc == 2) {
		char *host = JS_GetStringBytes(JS_ValueToString(cx, argv[0]));
		int32 port;
		switch_sockaddr_t *addr;
		switch_status_t ret;

		JS_ValueToInt32(cx, argv[1], &port);

		ret = switch_sockaddr_info_get(&addr, host, AF_INET, (switch_port_t) port, 0, socket->pool);
		if (ret != SWITCH_STATUS_SUCCESS) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CONSOLE, "switch_sockaddr_info_get failed: %d.\n", ret);
			return JS_FALSE;
		}

		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CONSOLE, "Connecting to: %s:%d.\n", host, port);
		socket->saveDepth = JS_SuspendRequest(cx);
		ret = switch_socket_connect(socket->socket, addr);
		JS_ResumeRequest(cx, socket->saveDepth);
		if (ret != SWITCH_STATUS_SUCCESS) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CONSOLE, "switch_socket_connect failed: %d.\n", ret);
			*rval = BOOLEAN_TO_JSVAL(JS_FALSE);
		} else
			*rval = BOOLEAN_TO_JSVAL(JS_TRUE);

	}

	return JS_TRUE;
}

static JSBool socket_send(JSContext * cx, JSObject * obj, uintN argc, jsval * argv, jsval * rval)
{
	js_socket_obj_t *socket = JS_GetPrivate(cx, obj);
	if (socket == NULL) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Failed to find js object.\n");
		return JS_FALSE;
	}

	if (argc == 1) {
		switch_status_t ret;
		char *buffer = JS_GetStringBytes(JS_ValueToString(cx, argv[0]));
		switch_size_t len = strlen(buffer);
		socket->saveDepth = JS_SuspendRequest(cx);
		ret = switch_socket_send(socket->socket, buffer, &len);
		JS_ResumeRequest(cx, socket->saveDepth);
		if (ret != SWITCH_STATUS_SUCCESS) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CONSOLE, "switch_socket_send failed: %d.\n", ret);
			*rval = BOOLEAN_TO_JSVAL(JS_FALSE);
		} else
			*rval = BOOLEAN_TO_JSVAL(JS_TRUE);
	}

	return JS_TRUE;
}

static JSBool socket_read_bytes(JSContext * cx, JSObject * obj, uintN argc, jsval * argv, jsval * rval)
{
	js_socket_obj_t *socket = JS_GetPrivate(cx, obj);
	if (socket == NULL) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Failed to find js object.\n");
		return JS_FALSE;
	}

	if (argc == 1) {
		int32 bytes_to_read;
		switch_status_t ret;
		switch_size_t len;

		JS_ValueToInt32(cx, argv[0], &bytes_to_read);
		len = (switch_size_t) bytes_to_read;

		if (socket->buffer_size < len) {
			socket->read_buffer = switch_core_alloc(socket->pool, len + 1);
			socket->buffer_size = bytes_to_read + 1;
		}

		socket->saveDepth = JS_SuspendRequest(cx);
		ret = switch_socket_recv(socket->socket, socket->read_buffer, &len);
		JS_ResumeRequest(cx, socket->saveDepth);
		if (ret != SWITCH_STATUS_SUCCESS) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CONSOLE, "switch_socket_send failed: %d.\n", ret);
			*rval = BOOLEAN_TO_JSVAL(JS_FALSE);
		} else {
			socket->read_buffer[len] = 0;
			*rval = STRING_TO_JSVAL(JS_NewStringCopyZ(cx, socket->read_buffer));
		}
	}

	return JS_TRUE;
}

static JSBool socket_read(JSContext * cx, JSObject * obj, uintN argc, jsval * argv, jsval * rval)
{
	js_socket_obj_t *socket = JS_GetPrivate(cx, obj);
	if (socket == NULL) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Failed to find js object.\n");
		return JS_FALSE;
	}

	if (argc >= 0) {
		char *delimiter = "\n";
		switch_status_t ret = SWITCH_STATUS_FALSE;
		switch_size_t len = 1;
		switch_size_t total_length = 0;
		int can_run = TRUE;
		char tempbuf[2];

		if (argc == 1)
			delimiter = JS_GetStringBytes(JS_ValueToString(cx, argv[0]));


		if (socket->read_buffer == 0)
			socket->read_buffer = switch_core_alloc(socket->pool, socket->buffer_size);

		socket->saveDepth = JS_SuspendRequest(cx);
		while (can_run == TRUE) {
			ret = switch_socket_recv(socket->socket, tempbuf, &len);
			if (ret != SWITCH_STATUS_SUCCESS)
				break;

			tempbuf[1] = 0;
			if (tempbuf[0] == delimiter[0])
				break;
			else if (tempbuf[0] == '\r' && delimiter[0] == '\n')
				continue;
			else {
				// Buffer is full, let's increase it.
				if (total_length == socket->buffer_size - 1) {
					switch_size_t new_size = socket->buffer_size + 4196;
					char *new_buffer = switch_core_alloc(socket->pool, socket->buffer_size);
					memcpy(new_buffer, socket->read_buffer, total_length);
					socket->buffer_size = new_size;
					socket->read_buffer = new_buffer;
				}
				socket->read_buffer[total_length] = tempbuf[0];
				++total_length;
			}
		}
		JS_ResumeRequest(cx, socket->saveDepth);

		if (ret != SWITCH_STATUS_SUCCESS) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CONSOLE, "socket receive failed: %d.\n", ret);
			*rval = BOOLEAN_TO_JSVAL(JS_FALSE);
		} else {
			socket->read_buffer[total_length] = 0;
			*rval = STRING_TO_JSVAL(JS_NewStringCopyZ(cx, socket->read_buffer));
		}
	}

	return JS_TRUE;
}

static JSBool socket_close(JSContext * cx, JSObject * obj, uintN argc, jsval * argv, jsval * rval)
{
	js_socket_obj_t *socket = JS_GetPrivate(cx, obj);
	if (socket == NULL) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Failed to find js object.\n");
		return JS_FALSE;
	}

	socket->saveDepth = JS_SuspendRequest(cx);
	switch_socket_shutdown(socket->socket, SWITCH_SHUTDOWN_READWRITE);
	switch_socket_close(socket->socket);
	socket->socket = NULL;
	JS_ResumeRequest(cx, socket->saveDepth);
	return JS_TRUE;
}

static JSFunctionSpec socket_methods[] = {
	{"connect", socket_connect, 1},
	{"close", socket_close, 1},
	{"send", socket_send, 1},
	{"readBytes", socket_read_bytes, 1},
	{"read", socket_read, 1},
	{0}
};

#define SOCKET_ADDRESS 1
#define SOCKET_PORT 2

static JSPropertySpec socket_props[] = {
	{"address", SOCKET_ADDRESS, JSPROP_READONLY | JSPROP_PERMANENT},
	{"port", SOCKET_PORT, JSPROP_READONLY | JSPROP_PERMANENT},
	{0}
};


static JSBool socket_getProperty(JSContext * cx, JSObject * obj, jsval id, jsval * vp)
{
	JSBool res = JS_TRUE;
//  js_socket_obj_t *socket = JS_GetPrivate(cx, obj);
	char *name;
	int param = 0;

	name = JS_GetStringBytes(JS_ValueToString(cx, id));
	/* numbers are our props anything else is a method */
	if (name[0] >= 48 && name[0] <= 57) {
		param = atoi(name);
	} else {
		return JS_TRUE;
	}

	switch (param) {
	case SOCKET_ADDRESS:
		*vp = STRING_TO_JSVAL(JS_NewStringCopyZ(cx, "unknown"));
		break;
	case SOCKET_PORT:
		*vp = INT_TO_JSVAL(1234);
		break;
	}

	return res;
}

JSClass socket_class = {
	modname, JSCLASS_HAS_PRIVATE,
	JS_PropertyStub, JS_PropertyStub, socket_getProperty, JS_PropertyStub,
	JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub, socket_destroy, NULL, NULL, NULL,
	socket_construct
};


switch_status_t socket_load(JSContext * cx, JSObject * obj)
{
	JS_InitClass(cx, obj, NULL, &socket_class, socket_construct, 3, socket_props, socket_methods, socket_props, socket_methods);
	return SWITCH_STATUS_SUCCESS;
}


const sm_module_interface_t socket_module_interface = {
	/*.name = */ modname,
	/*.spidermonkey_load */ socket_load,
	/*.next */ NULL
};

SWITCH_MOD_DECLARE_NONSTD(switch_status_t) spidermonkey_init(const sm_module_interface_t ** module_interface)
{
	*module_interface = &socket_module_interface;
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
