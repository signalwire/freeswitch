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
 * fsfile.cpp -- JavaScript File class - implements a class similar to the Spidermonkey built-in File class.
 *
 */

#include "fsfile.hpp"

using namespace std;
using namespace v8;

static const char js_class_name[] = "File";

FSFile::~FSFile(void)
{
	// Cleanup local objects
}

void FSFile::Init()
{
	// Init local objects
}

string FSFile::GetJSClassName()
{
	return js_class_name;
}

void *FSFile::Construct(const v8::FunctionCallbackInfo<Value>& info)
{
	return new FSFile(info);

	// TODO: Implement the actual constructor code */
}

JS_FILE_FUNCTION_IMPL(Close)
{
	info.GetIsolate()->ThrowException(String::NewFromUtf8(info.GetIsolate(), "Not yet implemented"));
}

JS_FILE_FUNCTION_IMPL(CopyTo)
{
	info.GetIsolate()->ThrowException(String::NewFromUtf8(info.GetIsolate(), "Not yet implemented"));
}

JS_FILE_FUNCTION_IMPL(Flush)
{
	info.GetIsolate()->ThrowException(String::NewFromUtf8(info.GetIsolate(), "Not yet implemented"));
}

JS_FILE_FUNCTION_IMPL(List)
{
	info.GetIsolate()->ThrowException(String::NewFromUtf8(info.GetIsolate(), "Not yet implemented"));
}

JS_FILE_FUNCTION_IMPL(MkDir)
{
	info.GetIsolate()->ThrowException(String::NewFromUtf8(info.GetIsolate(), "Not yet implemented"));
}

JS_FILE_FUNCTION_IMPL(Open)
{
	info.GetIsolate()->ThrowException(String::NewFromUtf8(info.GetIsolate(), "Not yet implemented"));
}

JS_FILE_FUNCTION_IMPL(Read)
{
	info.GetIsolate()->ThrowException(String::NewFromUtf8(info.GetIsolate(), "Not yet implemented"));
}

JS_FILE_FUNCTION_IMPL(ReadAll)
{
	info.GetIsolate()->ThrowException(String::NewFromUtf8(info.GetIsolate(), "Not yet implemented"));
}

JS_FILE_FUNCTION_IMPL(ReadLn)
{
	info.GetIsolate()->ThrowException(String::NewFromUtf8(info.GetIsolate(), "Not yet implemented"));
}

JS_FILE_FUNCTION_IMPL(Remove)
{
	info.GetIsolate()->ThrowException(String::NewFromUtf8(info.GetIsolate(), "Not yet implemented"));
}

JS_FILE_FUNCTION_IMPL(RenameTo)
{
	info.GetIsolate()->ThrowException(String::NewFromUtf8(info.GetIsolate(), "Not yet implemented"));
}

JS_FILE_FUNCTION_IMPL(Seek)
{
	info.GetIsolate()->ThrowException(String::NewFromUtf8(info.GetIsolate(), "Not yet implemented"));
}

JS_FILE_FUNCTION_IMPL(ToString)
{
	info.GetIsolate()->ThrowException(String::NewFromUtf8(info.GetIsolate(), "Not yet implemented"));
}

JS_FILE_FUNCTION_IMPL(ToURL)
{
	info.GetIsolate()->ThrowException(String::NewFromUtf8(info.GetIsolate(), "Not yet implemented"));
}

JS_FILE_FUNCTION_IMPL(Write)
{
	info.GetIsolate()->ThrowException(String::NewFromUtf8(info.GetIsolate(), "Not yet implemented"));
}

JS_FILE_FUNCTION_IMPL(WriteAll)
{
	info.GetIsolate()->ThrowException(String::NewFromUtf8(info.GetIsolate(), "Not yet implemented"));
}

JS_FILE_FUNCTION_IMPL(WriteLn)
{
	info.GetIsolate()->ThrowException(String::NewFromUtf8(info.GetIsolate(), "Not yet implemented"));
}

JS_FILE_GET_PROPERTY_IMPL(GetPropCanAppend)
{
	info.GetIsolate()->ThrowException(String::NewFromUtf8(info.GetIsolate(), "Not yet implemented"));
}

JS_FILE_GET_PROPERTY_IMPL(GetPropCanRead)
{
	info.GetIsolate()->ThrowException(String::NewFromUtf8(info.GetIsolate(), "Not yet implemented"));
}

JS_FILE_GET_PROPERTY_IMPL(GetPropCanReplace)
{
	info.GetIsolate()->ThrowException(String::NewFromUtf8(info.GetIsolate(), "Not yet implemented"));
}

JS_FILE_GET_PROPERTY_IMPL(GetPropCanWrite)
{
	info.GetIsolate()->ThrowException(String::NewFromUtf8(info.GetIsolate(), "Not yet implemented"));
}

JS_FILE_GET_PROPERTY_IMPL(GetPropCreationTime)
{
	info.GetIsolate()->ThrowException(String::NewFromUtf8(info.GetIsolate(), "Not yet implemented"));
}

JS_FILE_GET_PROPERTY_IMPL(GetPropExists)
{
	info.GetIsolate()->ThrowException(String::NewFromUtf8(info.GetIsolate(), "Not yet implemented"));
}

JS_FILE_GET_PROPERTY_IMPL(GetPropHasAutoFlush)
{
	info.GetIsolate()->ThrowException(String::NewFromUtf8(info.GetIsolate(), "Not yet implemented"));
}

JS_FILE_GET_PROPERTY_IMPL(GetPropHasRandomAccess)
{
	info.GetIsolate()->ThrowException(String::NewFromUtf8(info.GetIsolate(), "Not yet implemented"));
}

JS_FILE_GET_PROPERTY_IMPL(GetPropIsDirectory)
{
	info.GetIsolate()->ThrowException(String::NewFromUtf8(info.GetIsolate(), "Not yet implemented"));
}

JS_FILE_GET_PROPERTY_IMPL(GetPropIsFile)
{
	info.GetIsolate()->ThrowException(String::NewFromUtf8(info.GetIsolate(), "Not yet implemented"));
}

JS_FILE_GET_PROPERTY_IMPL(GetPropIsNative)
{
	info.GetIsolate()->ThrowException(String::NewFromUtf8(info.GetIsolate(), "Not yet implemented"));
}

JS_FILE_GET_PROPERTY_IMPL(GetPropIsOpen)
{
	info.GetIsolate()->ThrowException(String::NewFromUtf8(info.GetIsolate(), "Not yet implemented"));
}

JS_FILE_GET_PROPERTY_IMPL(GetPropLastModified)
{
	info.GetIsolate()->ThrowException(String::NewFromUtf8(info.GetIsolate(), "Not yet implemented"));
}

JS_FILE_GET_PROPERTY_IMPL(GetPropLength)
{
	info.GetIsolate()->ThrowException(String::NewFromUtf8(info.GetIsolate(), "Not yet implemented"));
}

JS_FILE_GET_PROPERTY_IMPL(GetPropMode)
{
	info.GetIsolate()->ThrowException(String::NewFromUtf8(info.GetIsolate(), "Not yet implemented"));
}

JS_FILE_GET_PROPERTY_IMPL(GetPropName)
{
	info.GetIsolate()->ThrowException(String::NewFromUtf8(info.GetIsolate(), "Not yet implemented"));
}

JS_FILE_GET_PROPERTY_IMPL(GetPropParent)
{
	info.GetIsolate()->ThrowException(String::NewFromUtf8(info.GetIsolate(), "Not yet implemented"));
}

JS_FILE_GET_PROPERTY_IMPL(GetPropPath)
{
	info.GetIsolate()->ThrowException(String::NewFromUtf8(info.GetIsolate(), "Not yet implemented"));
}

JS_FILE_GET_PROPERTY_IMPL(GetPropPosition)
{
	info.GetIsolate()->ThrowException(String::NewFromUtf8(info.GetIsolate(), "Not yet implemented"));
}

JS_FILE_SET_PROPERTY_IMPL(SetPropPosition)
{
	info.GetIsolate()->ThrowException(String::NewFromUtf8(info.GetIsolate(), "Not yet implemented"));
}

JS_FILE_GET_PROPERTY_IMPL(GetPropSize)
{
	info.GetIsolate()->ThrowException(String::NewFromUtf8(info.GetIsolate(), "Not yet implemented"));
}

JS_FILE_GET_PROPERTY_IMPL(GetPropType)
{
	info.GetIsolate()->ThrowException(String::NewFromUtf8(info.GetIsolate(), "Not yet implemented"));
}

static const js_function_t file_proc[] = {
	{"close", FSFile::Close},
	{"copyTo", FSFile::CopyTo},
	{"flush", FSFile::Flush},
	{"list", FSFile::List},
	{"mkdir", FSFile::MkDir},
	{"open", FSFile::Open},
	{"read", FSFile::Read},
	{"readAll", FSFile::ReadAll},
	{"readln", FSFile::ReadLn},
	{"remove", FSFile::Remove},
	{"renameTo", FSFile::RenameTo},
	{"seek", FSFile::Seek},
	{"toString", FSFile::ToString},
	{"toURL", FSFile::ToURL},
	{"write", FSFile::Write},
	{"writeAll", FSFile::WriteAll},
	{"writeln", FSFile::WriteLn},
	{0}
};

static const js_property_t file_prop[] = {
	{"canAppend", FSFile::GetPropCanAppend, JSBase::DefaultSetProperty},
	{"canRead", FSFile::GetPropCanRead, JSBase::DefaultSetProperty},
	{"canReplace", FSFile::GetPropCanReplace, JSBase::DefaultSetProperty},
	{"canWrite", FSFile::GetPropCanWrite, JSBase::DefaultSetProperty},
	{"creationTime", FSFile::GetPropCreationTime, JSBase::DefaultSetProperty},
	{"exists", FSFile::GetPropExists, JSBase::DefaultSetProperty},
	{"hasAutoFlush", FSFile::GetPropHasAutoFlush, JSBase::DefaultSetProperty},
	{"hasRandomAccess", FSFile::GetPropHasRandomAccess, JSBase::DefaultSetProperty},
	{"isDirectory", FSFile::GetPropIsDirectory, JSBase::DefaultSetProperty},
	{"isFile", FSFile::GetPropIsFile, JSBase::DefaultSetProperty},
	{"isNative", FSFile::GetPropIsNative, JSBase::DefaultSetProperty},
	{"isOpen", FSFile::GetPropIsOpen, JSBase::DefaultSetProperty},
	{"lastModified", FSFile::GetPropLastModified, JSBase::DefaultSetProperty},
	{"length", FSFile::GetPropLength, JSBase::DefaultSetProperty},
	{"mode", FSFile::GetPropMode, JSBase::DefaultSetProperty},
	{"name", FSFile::GetPropName, JSBase::DefaultSetProperty},
	{"parent", FSFile::GetPropParent, JSBase::DefaultSetProperty},
	{"path", FSFile::GetPropPath, JSBase::DefaultSetProperty},
	{"position", FSFile::GetPropPosition, FSFile::SetPropPosition},
	{"size", FSFile::GetPropSize, JSBase::DefaultSetProperty},
	{"type", FSFile::GetPropType, JSBase::DefaultSetProperty},
	{0}
};

/*
- currentDir
- error
- input
- output
- separator
*/

static const js_class_definition_t file_desc = {
	js_class_name,
	FSFile::Construct,
	file_proc,
	file_prop
};

static switch_status_t file_load(const v8::FunctionCallbackInfo<Value>& info)
{
	JSBase::Register(info.GetIsolate(), &file_desc);
	return SWITCH_STATUS_SUCCESS;
}

static const v8_mod_interface_t file_module_interface = {
	/*.name = */ js_class_name,
	/*.js_mod_load */ file_load
};

const v8_mod_interface_t *FSFile::GetModuleInterface()
{
	return &file_module_interface;
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
