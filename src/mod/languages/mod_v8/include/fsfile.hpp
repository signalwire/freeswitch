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
 * fsfile.hpp -- JavaScript File class header
 *
 */

#ifndef FS_FILE_H
#define FS_FILE_H

#include "mod_v8.h"

/* Macros for easier V8 callback definitions */
#define JS_FILE_GET_PROPERTY_DEF(method_name) JS_GET_PROPERTY_DEF(method_name, FSFile)
#define JS_FILE_SET_PROPERTY_DEF(method_name) JS_SET_PROPERTY_DEF(method_name, FSFile)
#define JS_FILE_FUNCTION_DEF(method_name) JS_FUNCTION_DEF(method_name, FSFile)
#define JS_FILE_GET_PROPERTY_IMPL(method_name) JS_GET_PROPERTY_IMPL(method_name, FSFile)
#define JS_FILE_SET_PROPERTY_IMPL(method_name) JS_SET_PROPERTY_IMPL(method_name, FSFile)
#define JS_FILE_FUNCTION_IMPL(method_name) JS_FUNCTION_IMPL(method_name, FSFile)

class FSFile : public JSBase
{
private:
	void Init();
public:
	FSFile(JSMain *owner) : JSBase(owner) { }
	FSFile(const v8::FunctionCallbackInfo<v8::Value>& info) : JSBase(info) { }
	virtual ~FSFile(void);
	virtual std::string GetJSClassName();

	static const v8_mod_interface_t *GetModuleInterface();

	/* Methods available from JavaScript */
	static void *Construct(const v8::FunctionCallbackInfo<v8::Value>& info);
	JS_FILE_FUNCTION_DEF(Close);
	JS_FILE_FUNCTION_DEF(CopyTo);
	JS_FILE_FUNCTION_DEF(Flush);
	JS_FILE_FUNCTION_DEF(List);
	JS_FILE_FUNCTION_DEF(MkDir);
	JS_FILE_FUNCTION_DEF(Open);
	JS_FILE_FUNCTION_DEF(Read);
	JS_FILE_FUNCTION_DEF(ReadAll);
	JS_FILE_FUNCTION_DEF(ReadLn);
	JS_FILE_FUNCTION_DEF(Remove);
	JS_FILE_FUNCTION_DEF(RenameTo);
	JS_FILE_FUNCTION_DEF(Seek);
	JS_FILE_FUNCTION_DEF(ToString);
	JS_FILE_FUNCTION_DEF(ToURL);
	JS_FILE_FUNCTION_DEF(Write);
	JS_FILE_FUNCTION_DEF(WriteAll);
	JS_FILE_FUNCTION_DEF(WriteLn);

	JS_FILE_GET_PROPERTY_DEF(GetPropCanAppend);
	JS_FILE_GET_PROPERTY_DEF(GetPropCanRead);
	JS_FILE_GET_PROPERTY_DEF(GetPropCanReplace);
	JS_FILE_GET_PROPERTY_DEF(GetPropCanWrite);
	JS_FILE_GET_PROPERTY_DEF(GetPropCreationTime);
	JS_FILE_GET_PROPERTY_DEF(GetPropExists);
	JS_FILE_GET_PROPERTY_DEF(GetPropHasAutoFlush);
	JS_FILE_GET_PROPERTY_DEF(GetPropHasRandomAccess);
	JS_FILE_GET_PROPERTY_DEF(GetPropIsDirectory);
	JS_FILE_GET_PROPERTY_DEF(GetPropIsFile);
	JS_FILE_GET_PROPERTY_DEF(GetPropIsNative);
	JS_FILE_GET_PROPERTY_DEF(GetPropIsOpen);
	JS_FILE_GET_PROPERTY_DEF(GetPropLastModified);
	JS_FILE_GET_PROPERTY_DEF(GetPropLength);
	JS_FILE_GET_PROPERTY_DEF(GetPropMode);
	JS_FILE_GET_PROPERTY_DEF(GetPropName);
	JS_FILE_GET_PROPERTY_DEF(GetPropParent);
	JS_FILE_GET_PROPERTY_DEF(GetPropPath);
	JS_FILE_GET_PROPERTY_DEF(GetPropPosition);
	JS_FILE_SET_PROPERTY_DEF(SetPropPosition);
	JS_FILE_GET_PROPERTY_DEF(GetPropSize);
	JS_FILE_GET_PROPERTY_DEF(GetPropType);
};

#endif /* FS_FILE_H */

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
