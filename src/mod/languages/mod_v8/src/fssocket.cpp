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
 * Ported from the Original Code in Spidermonkey Socket Module
 *
 * The Initial Developer of the Original Code is
 * Jonas Gauffin <jonas.gauffin@gmail.com>
 * Portions created by the Initial Developer are Copyright (C)
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 * Peter Olsson <peter@olssononline.se>
 * Jonas Gauffin <jonas.gauffin@gmail.com>
 *
 * fssocket.cpp -- JavaScript Socket class
 *
 */

#include "fssocket.hpp"

using namespace std;
using namespace v8;

static const char js_class_name[] = "Socket";

FSSocket::~FSSocket(void)
{
	if (_socket) {
		switch_socket_shutdown(_socket, SWITCH_SHUTDOWN_READWRITE);
		switch_socket_close(_socket);
	}

	if (_pool) {
		switch_core_destroy_memory_pool(&_pool);
	}
}

string FSSocket::GetJSClassName()
{
	return js_class_name;
}

void FSSocket::Init()
{
	_socket = NULL;
	_pool = NULL;
	_read_buffer = NULL;
	_buffer_size = 0;
	_state = 0;
}

void *FSSocket::Construct(const v8::FunctionCallbackInfo<Value>& info)
{
	FSSocket *js_socket_obj = NULL;
	switch_memory_pool_t *pool;
	switch_socket_t *socket;
	switch_status_t ret;

	switch_core_new_memory_pool(&pool);

	ret = switch_socket_create(&socket, AF_INET, SOCK_STREAM, SWITCH_PROTO_TCP, pool);

	if (ret != SWITCH_STATUS_SUCCESS) {
		switch_core_destroy_memory_pool(&pool);
		char *err = switch_mprintf("Failed to create socket, reason: %d", ret);
		info.GetIsolate()->ThrowException(String::NewFromUtf8(info.GetIsolate(), err));
		free(err);
		return NULL;
	}

	js_socket_obj = new FSSocket(info);
	js_socket_obj->_pool = pool;
	js_socket_obj->_socket = socket;

	return js_socket_obj;
}

JS_SOCKET_FUNCTION_IMPL(Connect)
{
	if (info.Length() == 2) {
		String::Utf8Value str(info[0]);
		const char *host = js_safe_str(*str);
		int32_t port;
		switch_sockaddr_t *addr;
		switch_status_t ret;

		port = info[1]->Int32Value();

		/* Recreate socket if it has been closed */
		if (!this->_socket) {
			if ((ret = switch_socket_create(&this->_socket, AF_INET, SOCK_STREAM, SWITCH_PROTO_TCP, this->_pool)) != SWITCH_STATUS_SUCCESS) {
				char *err = switch_mprintf("Failed to create socket, reason: %d", ret);
				info.GetIsolate()->ThrowException(String::NewFromUtf8(info.GetIsolate(), err));
				free(err);
				return;
			}
		}

		ret = switch_sockaddr_info_get(&addr, host, AF_INET, (switch_port_t) port, 0, this->_pool);

		if (ret != SWITCH_STATUS_SUCCESS) {
			char *err = switch_mprintf("switch_sockaddr_info_get failed: %d", ret);
			info.GetIsolate()->ThrowException(String::NewFromUtf8(info.GetIsolate(), err));
			free(err);
			return;
		}

		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CONSOLE, "Connecting to: %s:%d.\n", host, port);
		ret = switch_socket_connect(this->_socket, addr);

		if (ret != SWITCH_STATUS_SUCCESS) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CONSOLE, "switch_socket_connect failed: %d.\n", ret);
			info.GetReturnValue().Set(false);
		} else
			info.GetReturnValue().Set(true);
	}
}

JS_SOCKET_FUNCTION_IMPL(Send)
{
	if (!this->_socket) {
		info.GetIsolate()->ThrowException(String::NewFromUtf8(info.GetIsolate(), "Socket is not active"));
		return;
	}

	if (info.Length() == 1) {
		switch_status_t ret;
		String::Utf8Value str(info[0]);
		const char *buffer = js_safe_str(*str);
		switch_size_t len = strlen(buffer);
		ret = switch_socket_send(this->_socket, buffer, &len);

		if (ret != SWITCH_STATUS_SUCCESS) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CONSOLE, "switch_socket_send failed: %d.\n", ret);
			info.GetReturnValue().Set(false);
		} else
			info.GetReturnValue().Set(true);
	} else {
		info.GetReturnValue().Set(false);
	}
}

JS_SOCKET_FUNCTION_IMPL(ReadBytes)
{
	if (!this->_socket) {
		info.GetIsolate()->ThrowException(String::NewFromUtf8(info.GetIsolate(), "Socket is not active"));
		return;
	}

	if (info.Length() == 1) {
		int32_t bytes_to_read;
		switch_status_t ret;
		switch_size_t len;

		bytes_to_read = info[0]->Int32Value();
		len = (switch_size_t) bytes_to_read;

		if (this->_buffer_size < len) {
			this->_read_buffer = (char *)switch_core_alloc(this->_pool, len + 1);
			this->_buffer_size = bytes_to_read + 1;
		}

		ret = switch_socket_recv(this->_socket, this->_read_buffer, &len);
		if (ret != SWITCH_STATUS_SUCCESS) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CONSOLE, "switch_socket_send failed: %d.\n", ret);
			info.GetReturnValue().Set(false);
			return;
		} else {
			this->_read_buffer[len] = 0;
			info.GetReturnValue().Set(String::NewFromUtf8(info.GetIsolate(), js_safe_str(this->_read_buffer)));
			return;
		}
	}

	info.GetReturnValue().Set(false);
}

JS_SOCKET_FUNCTION_IMPL(Read)
{
	string delimiter = "\n";
	switch_status_t ret = SWITCH_STATUS_FALSE;
	switch_size_t len = 1;
	switch_size_t total_length = 0;
	int can_run = TRUE;
	char tempbuf[2];

	if (!this->_socket) {
		info.GetIsolate()->ThrowException(String::NewFromUtf8(info.GetIsolate(), "Socket is not active"));
		return;
	}

	if (info.Length() == 1) {
		String::Utf8Value str(info[0]);
		delimiter = js_safe_str(*str);
	}

	if (this->_read_buffer == 0) {
		this->_read_buffer = (char *)switch_core_alloc(this->_pool, this->_buffer_size);
	}

	while (can_run == TRUE) {
		ret = switch_socket_recv(this->_socket, tempbuf, &len);
		if (ret != SWITCH_STATUS_SUCCESS)
			break;

		tempbuf[1] = 0;
		if (tempbuf[0] == delimiter[0])
			break;
		else if (tempbuf[0] == '\r' && delimiter[0] == '\n')
			continue;
		else {
			// Buffer is full, let's increase it.
			if (total_length == this->_buffer_size - 1) {
				switch_size_t new_size = this->_buffer_size + 4196;
				char *new_buffer = (char *)switch_core_alloc(this->_pool, this->_buffer_size);
				memcpy(new_buffer, this->_read_buffer, total_length);
				this->_buffer_size = new_size;
				this->_read_buffer = new_buffer;
			}
			this->_read_buffer[total_length] = tempbuf[0];
			++total_length;
		}
	}

	if (ret != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CONSOLE, "socket receive failed: %d.\n", ret);
		info.GetReturnValue().Set(false);
	} else {
		this->_read_buffer[total_length] = 0;
		info.GetReturnValue().Set(String::NewFromUtf8(info.GetIsolate(), js_safe_str(this->_read_buffer)));
	}
}

JS_SOCKET_FUNCTION_IMPL(Close)
{
	switch_socket_shutdown(this->_socket, SWITCH_SHUTDOWN_READWRITE);
	switch_socket_close(this->_socket);
	this->_socket = NULL;
}

JS_SOCKET_GET_PROPERTY_IMPL(GetProperty)
{
	HandleScope handle_scope(info.GetIsolate());
	String::Utf8Value str(property);

	if (!this->_socket) {
		info.GetIsolate()->ThrowException(String::NewFromUtf8(info.GetIsolate(), "Socket is not active"));
		return;
	}

	if (!strcmp(js_safe_str(*str), "address")) {
		switch_sockaddr_t *sa = NULL;
		char tmp[30];

		switch_socket_addr_get(&sa, SWITCH_TRUE, this->_socket);

		if (sa && switch_get_addr(tmp, sizeof(tmp), sa) && !zstr(tmp)) {
			info.GetReturnValue().Set(String::NewFromUtf8(info.GetIsolate(), tmp));
		} else {
			info.GetReturnValue().Set(String::NewFromUtf8(info.GetIsolate(), "unknown"));
		}
	} else if (!strcmp(js_safe_str(*str), "port")) {
		switch_sockaddr_t *sa = NULL;

		switch_socket_addr_get(&sa, SWITCH_TRUE, this->_socket);

		if (sa) {
			info.GetReturnValue().Set(Integer::New(info.GetIsolate(), switch_sockaddr_get_port(sa)));
		} else {
			info.GetReturnValue().Set(Integer::New(info.GetIsolate(), 0));
		}
	} else {
		info.GetIsolate()->ThrowException(String::NewFromUtf8(info.GetIsolate(), "Bad property"));
	}
}

static const js_function_t socket_methods[] = {
	{"connect", FSSocket::Connect},
	{"close", FSSocket::Close},
	{"send", FSSocket::Send},
	{"readBytes", FSSocket::ReadBytes},
	{"read", FSSocket::Read},
	{0}
};

static const js_property_t socket_props[] = {
	{"address", FSSocket::GetProperty, JSBase::DefaultSetProperty},
	{"port", FSSocket::GetProperty, JSBase::DefaultSetProperty},
	{0}
};

static const js_class_definition_t socket_desc = {
	js_class_name,
	FSSocket::Construct,
	socket_methods,
	socket_props
};

static switch_status_t socket_load(const v8::FunctionCallbackInfo<Value>& info)
{
	JSBase::Register(info.GetIsolate(), &socket_desc);
	return SWITCH_STATUS_SUCCESS;
}

static const v8_mod_interface_t socket_module_interface = {
	/*.name = */ js_class_name,
	/*.js_mod_load */ socket_load
};

const v8_mod_interface_t *FSSocket::GetModuleInterface()
{
	return &socket_module_interface;
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
