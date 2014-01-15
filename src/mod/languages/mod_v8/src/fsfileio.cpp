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
 * William King <william.king@quentustech.com>
 *
 * fsfileio.cpp -- JavaScript FileIO class
 *
 */

#include "fsfileio.hpp"

using namespace std;
using namespace v8;

static const char js_class_name[] = "FileIO";

FSFileIO::~FSFileIO(void)
{
	if (_fd) {
		switch_file_close(_fd);
	}
	switch_core_destroy_memory_pool(&_pool);
}

string FSFileIO::GetJSClassName()
{
	return js_class_name;
}

void FSFileIO::Init()
{
	_path = NULL;
	_flags = 0;
	_fd = NULL;
	_pool = NULL;
	_buf = NULL;
	_buflen = 0;
	_bufsize = 0;
}

void *FSFileIO::Construct(const v8::FunctionCallbackInfo<Value>& info)
{
	HandleScope handle_scope(info.GetIsolate());
	switch_memory_pool_t *pool;
	switch_file_t *fd;
	unsigned int flags = 0;
	FSFileIO *fio;

	if (info.Length() > 1) {
		const char *path, *flags_str;
		String::Utf8Value str1(info[0]);
		String::Utf8Value str2(info[1]);
		path = js_safe_str(*str1);
		flags_str = js_safe_str(*str2);

		if (strchr(flags_str, 'r')) {
			flags |= SWITCH_FOPEN_READ;
		}

		if (strchr(flags_str, 'w')) {
			flags |= SWITCH_FOPEN_WRITE;
		}

		if (strchr(flags_str, 'c')) {
			flags |= SWITCH_FOPEN_CREATE;
		}

		if (strchr(flags_str, 'a')) {
			flags |= SWITCH_FOPEN_APPEND;
		}

		if (strchr(flags_str, 't')) {
			flags |= SWITCH_FOPEN_TRUNCATE;
		}

		if (strchr(flags_str, 'b')) {
			flags |= SWITCH_FOPEN_BINARY;
		}

		switch_core_new_memory_pool(&pool);

		if (switch_file_open(&fd, path, flags, SWITCH_FPROT_UREAD | SWITCH_FPROT_UWRITE, pool) != SWITCH_STATUS_SUCCESS) {
			switch_core_destroy_memory_pool(&pool);
			char *err = switch_mprintf("Cannot Open File: %s", path);
			info.GetIsolate()->ThrowException(String::NewFromUtf8(info.GetIsolate(), err));
			free(err);
			return NULL;
		}

		fio = new FSFileIO(info);
		fio->_fd = fd;
		fio->_pool = pool;
		fio->_path = switch_core_strdup(pool, path);
		fio->_flags = flags;

		return fio;
	}

	info.GetIsolate()->ThrowException(String::NewFromUtf8(info.GetIsolate(), "Invalid Args"));
	return NULL;
}

JS_FILEIO_FUNCTION_IMPL(Read)
{
	HandleScope handle_scope(info.GetIsolate());
	int32_t bytes = 0;
	switch_size_t read = 0;

	if (!(_flags & SWITCH_FOPEN_READ)) {
		info.GetReturnValue().Set(false);
		return;
	}

	if (info.Length() > 0) {
		bytes = info[0]->Int32Value();
	}

	if (bytes) {
		if (!_buf || _bufsize < bytes) {
			_buf = (char *)switch_core_alloc(_pool, bytes+1);
			memset(_buf, 0, bytes+1);
			_bufsize = bytes;
		}

		read = bytes;
		switch_file_read(_fd, _buf, &read);
		_buflen = read;
		info.GetReturnValue().Set((read > 0) ? true : false);
	} else {
		info.GetReturnValue().Set(false);
	}
}

JS_FILEIO_FUNCTION_IMPL(GetData)
{
	HandleScope handle_scope(info.GetIsolate());

	if (!_buflen || !_buf) {
		info.GetReturnValue().Set(false);
	} else {
		info.GetReturnValue().Set(String::NewFromUtf8(info.GetIsolate(), _buf));
	}
}

JS_FILEIO_FUNCTION_IMPL(Write)
{
	HandleScope handle_scope(info.GetIsolate());
	switch_size_t wrote = 0;

	if (!(_flags & SWITCH_FOPEN_WRITE)) {
		info.GetReturnValue().Set(false);
		return;
	}

	if (info.Length() > 0) {
		char *data = NULL;
		String::Utf8Value str(info[0]);
		data = *str;

		if (data) {
			wrote = strlen(data);
			info.GetReturnValue().Set((switch_file_write(_fd, data, &wrote) == SWITCH_STATUS_SUCCESS) ? true : false);
			return;
		}
	}

	info.GetReturnValue().Set(false);
}

JS_FILEIO_GET_PROPERTY_IMPL(GetProperty)
{
	HandleScope handle_scope(info.GetIsolate());
	String::Utf8Value str(property);
	const char *prop = js_safe_str(*str);

	if (!strcmp(prop, "path")) {
		if (_path) {
			info.GetReturnValue().Set(String::NewFromUtf8(info.GetIsolate(), _path));
		} else {
			info.GetReturnValue().Set(false);
		}
	} else if (!strcmp(prop, "open")) {
		info.GetReturnValue().Set(_fd ? true : false);
	} else {
		info.GetIsolate()->ThrowException(String::NewFromUtf8(info.GetIsolate(), "Bad property"));
	}
}

static const js_function_t fileio_proc[] = {
	{"read", FSFileIO::Read},
	{"write", FSFileIO::Write},
	{"data", FSFileIO::GetData},
	{0}
};

static const js_property_t fileio_prop[] = {
	{"path", FSFileIO::GetProperty, JSBase::DefaultSetProperty},
	{"open", FSFileIO::GetProperty, JSBase::DefaultSetProperty},
	{0}
};

static const js_class_definition_t fileio_desc = {
	js_class_name,
	FSFileIO::Construct,
	fileio_proc,
	fileio_prop
};

const js_class_definition_t *FSFileIO::GetClassDefinition()
{
	return &fileio_desc;
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
