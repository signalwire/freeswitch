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
 * fsteletone.hpp -- JavaScript TeleTone class header
 *
 */

#ifndef FS_TELETONE_H
#define FS_TELETONE_H

#include "mod_v8.h"
#include <libteletone.h>

/* Macros for easier V8 callback definitions */
#define JS_TELETONE_GET_PROPERTY_DEF(method_name) JS_GET_PROPERTY_DEF(method_name, FSTeleTone)
#define JS_TELETONE_SET_PROPERTY_DEF(method_name) JS_SET_PROPERTY_DEF(method_name, FSTeleTone)
#define JS_TELETONE_FUNCTION_DEF(method_name) JS_FUNCTION_DEF(method_name, FSTeleTone)
#define JS_TELETONE_GET_PROPERTY_IMPL(method_name) JS_GET_PROPERTY_IMPL(method_name, FSTeleTone)
#define JS_TELETONE_SET_PROPERTY_IMPL(method_name) JS_SET_PROPERTY_IMPL(method_name, FSTeleTone)
#define JS_TELETONE_FUNCTION_IMPL(method_name) JS_FUNCTION_IMPL(method_name, FSTeleTone)

class FSTeleTone : public JSBase
{
private:
	teletone_generation_session_t _ts;
	switch_core_session_t *_session;
	switch_codec_t _codec;
	switch_buffer_t *_audio_buffer;
	switch_memory_pool_t *_pool;
	switch_timer_t *_timer;
	switch_timer_t _timer_base;
	v8::Persistent<v8::Function> _function;
	v8::Persistent<v8::Value> _arg;
	unsigned int flags;

	void Init();
public:
	FSTeleTone(JSMain *owner) : JSBase(owner) { Init(); }
	FSTeleTone(const v8::FunctionCallbackInfo<v8::Value>& info) : JSBase(info) { Init(); }
	virtual ~FSTeleTone(void);
	virtual std::string GetJSClassName();

	static const v8_mod_interface_t *GetModuleInterface();

	static int Handler(teletone_generation_session_t *ts, teletone_tone_map_t *map);

	/* Methods available from JavaScript */
	static void *Construct(const v8::FunctionCallbackInfo<v8::Value>& info);
	JS_TELETONE_FUNCTION_DEF(Generate);
	JS_TELETONE_FUNCTION_DEF(OnDTMF);
	JS_TELETONE_FUNCTION_DEF(AddTone);
	JS_TELETONE_GET_PROPERTY_DEF(GetNameProperty);
};

#endif /* FS_TELETONE_H */

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
