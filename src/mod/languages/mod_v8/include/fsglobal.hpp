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
 * fsglobal.hpp -- JavaScript Global functions
 *
 */

#ifndef FS_GLOBAL_H
#define FS_GLOBAL_H

#include "javascript.hpp"
#include <switch.h>

/* Macros for easier V8 callback definitions */
#define JS_GLOBAL_FUNCTION_IMPL_STATIC(method_name) JS_FUNCTION_IMPL_STATIC(method_name, FSGlobal)

/* All globally available functions (not related to objects) */
class FSGlobal
{
private:
	static size_t HashCallback(void *ptr, size_t size, size_t nmemb, void *data);
	static size_t FileCallback(void *ptr, size_t size, size_t nmemb, void *data);
	static size_t FetchUrlCallback(void *ptr, size_t size, size_t nmemb, void *data);
public:
	static const js_function_t *GetFunctionDefinitions();

	/* Methods available from JavaScript */
	JS_FUNCTION_DEF_STATIC(Log);
	JS_FUNCTION_DEF_STATIC(GlobalGet);
	JS_FUNCTION_DEF_STATIC(GlobalSet);
	JS_FUNCTION_DEF_STATIC(Exit);
	JS_FUNCTION_DEF_STATIC(Include);
	JS_FUNCTION_DEF_STATIC(Bridge);
	JS_FUNCTION_DEF_STATIC(Email);
	JS_FUNCTION_DEF_STATIC(ApiExecute);
	JS_FUNCTION_DEF_STATIC(Use);
	JS_FUNCTION_DEF_STATIC(Sleep);
	JS_FUNCTION_DEF_STATIC(FileDelete);
	JS_FUNCTION_DEF_STATIC(System);
	JS_FUNCTION_DEF_STATIC(FetchURL);
	JS_FUNCTION_DEF_STATIC(FetchURLHash);
	JS_FUNCTION_DEF_STATIC(FetchURLFile);
};

#endif /* FS_GLOBAL_H */

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
