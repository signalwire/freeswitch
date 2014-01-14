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
 * fscoredb.hpp -- JavaScript CoreDB class header
 *
 */

#ifndef FS_COREDB_H
#define FS_COREDB_H

#include "mod_v8.h"

/* Macros for easier V8 callback definitions */
#define JS_COREDB_GET_PROPERTY_DEF(method_name) JS_GET_PROPERTY_DEF(method_name, FSCoreDB)
#define JS_COREDB_SET_PROPERTY_DEF(method_name) JS_SET_PROPERTY_DEF(method_name, FSCoreDB)
#define JS_COREDB_FUNCTION_DEF(method_name) JS_FUNCTION_DEF(method_name, FSCoreDB)
#define JS_COREDB_GET_PROPERTY_IMPL(method_name) JS_GET_PROPERTY_IMPL(method_name, FSCoreDB)
#define JS_COREDB_SET_PROPERTY_IMPL(method_name) JS_SET_PROPERTY_IMPL(method_name, FSCoreDB)
#define JS_COREDB_FUNCTION_IMPL(method_name) JS_FUNCTION_IMPL(method_name, FSCoreDB)

class FSCoreDB : public JSBase
{
private:
	switch_memory_pool_t *_pool;
	switch_core_db_t *_db;
	switch_core_db_stmt_t *_stmt;
	const char *_dbname;
	v8::Persistent<v8::Function> _callback;

	void Init();
	void DoClose();
	void StepEx(const v8::FunctionCallbackInfo<v8::Value>& info, int stepSuccessCode);
	static int Callback(void *pArg, int argc, char **argv, char **columnNames);
public:
	FSCoreDB(JSMain *owner) : JSBase(owner) { Init(); }
	FSCoreDB(const v8::FunctionCallbackInfo<v8::Value>& info) : JSBase(info) { Init(); }
	virtual ~FSCoreDB(void);
	virtual std::string GetJSClassName();

	static const v8_mod_interface_t *GetModuleInterface();

	/* Methods available from JavaScript */
	static void *Construct(const v8::FunctionCallbackInfo<v8::Value>& info);
	JS_COREDB_FUNCTION_DEF(Exec);
	JS_COREDB_FUNCTION_DEF(Close);
	JS_COREDB_FUNCTION_DEF(Next);
	JS_COREDB_FUNCTION_DEF(Step);
	JS_COREDB_FUNCTION_DEF(Fetch);
	JS_COREDB_FUNCTION_DEF(Prepare);
	JS_COREDB_FUNCTION_DEF(BindText);
	JS_COREDB_FUNCTION_DEF(BindInt);
	JS_COREDB_GET_PROPERTY_DEF(GetProperty);
};

#endif /* FS_COREDB_H */

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
