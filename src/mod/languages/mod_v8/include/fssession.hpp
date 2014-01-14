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
 * fssession.hpp -- JavaScript Session class header
 *
 */

#ifndef FS_SESSION_H
#define FS_SESSION_H

#include "javascript.hpp"
#include <switch.h>

typedef struct {
	switch_speech_handle_t sh;
	switch_codec_t codec;
	int speaking;
} js_session_speech_t;

typedef enum {
	S_HUP = (1 << 0),
} session_flag_t;

/* Macros for easier V8 callback definitions */
#define JS_SESSION_GET_PROPERTY_DEF(method_name) JS_GET_PROPERTY_DEF(method_name, FSSession)
#define JS_SESSION_SET_PROPERTY_DEF(method_name) JS_SET_PROPERTY_DEF(method_name, FSSession)
#define JS_SESSION_FUNCTION_DEF(method_name) JS_FUNCTION_DEF(method_name, FSSession)
#define JS_SESSION_GET_PROPERTY_IMPL(method_name) JS_GET_PROPERTY_IMPL(method_name, FSSession)
#define JS_SESSION_SET_PROPERTY_IMPL(method_name) JS_SET_PROPERTY_IMPL(method_name, FSSession)
#define JS_SESSION_FUNCTION_IMPL(method_name) JS_FUNCTION_IMPL(method_name, FSSession)

class FSSession : public JSBase
{
private:
	switch_core_session_t *_session;		// The FS session
	unsigned int flags;						// Flags related to this session
	switch_call_cause_t _cause;				// Hangup cause
	v8::Persistent<v8::Function> _on_hangup;// Hangup hook
	int _stack_depth;
	switch_channel_state_t _hook_state;
	char *_destination_number;
	char *_dialplan;
	char *_caller_id_name;
	char *_caller_id_number;
	char *_network_addr;
	char *_ani;
	char *_aniii;
	char *_rdnis;
	char *_context;
	char *_username;
	int _check_state;
	js_session_speech_t *_speech;

	void Init(void);
	switch_status_t InitSpeechEngine(const char *engine, const char *voice);
	void DestroySpeechEngine();
	static switch_status_t CommonCallback(switch_core_session_t *session, void *input, switch_input_type_t itype, void *buf, unsigned int buflen);
	static switch_status_t StreamInputCallback(switch_core_session_t *session, void *input, switch_input_type_t itype, void *buf, unsigned int buflen);
	static switch_status_t RecordInputCallback(switch_core_session_t *session, void *input, switch_input_type_t itype, void *buf, unsigned int buflen);
	static bool CheckHangupHook(FSSession *obj, bool *ret);
	static switch_status_t HangupHook(switch_core_session_t *session);
public:
	FSSession(JSMain *owner) : JSBase(owner) { Init(); }
	FSSession(const v8::FunctionCallbackInfo<v8::Value>& info) : JSBase(info) { Init(); }
	virtual ~FSSession(void);
	virtual std::string GetJSClassName();

	static const js_class_definition_t *GetClassDefinition();

	switch_core_session_t *GetSession();
	void Init(switch_core_session_t *session, unsigned int flags);
	static switch_status_t CollectInputCallback(switch_core_session_t *session, void *input, switch_input_type_t itype, void *buf, unsigned int buflen);

	/* Methods available from JavaScript */
	static void *Construct(const v8::FunctionCallbackInfo<v8::Value>& info);
	JS_SESSION_FUNCTION_DEF(Originate);
	JS_SESSION_FUNCTION_DEF(SetCallerdata);
	JS_SESSION_FUNCTION_DEF(SetHangupHook);
	JS_SESSION_FUNCTION_DEF(SetAutoHangup);
	JS_SESSION_FUNCTION_DEF(SayPhrase);
	JS_SESSION_FUNCTION_DEF(StreamFile);
	JS_SESSION_FUNCTION_DEF(CollectInput);
	JS_SESSION_FUNCTION_DEF(RecordFile);
	JS_SESSION_FUNCTION_DEF(FlushEvents);
	JS_SESSION_FUNCTION_DEF(FlushDigits);
	JS_SESSION_FUNCTION_DEF(Speak);
	JS_SESSION_FUNCTION_DEF(SetVariable);
	JS_SESSION_FUNCTION_DEF(GetVariable);
	JS_SESSION_FUNCTION_DEF(GetDigits);
	JS_SESSION_FUNCTION_DEF(Answer);
	JS_SESSION_FUNCTION_DEF(PreAnswer);
	JS_SESSION_FUNCTION_DEF(GenerateXmlCdr);
	JS_SESSION_FUNCTION_DEF(Ready);
	JS_SESSION_FUNCTION_DEF(Answered);
	JS_SESSION_FUNCTION_DEF(MediaReady);
	JS_SESSION_FUNCTION_DEF(RingReady);
	JS_SESSION_FUNCTION_DEF(WaitForAnswer);
	JS_SESSION_FUNCTION_DEF(WaitForMedia);
	JS_SESSION_FUNCTION_DEF(GetEvent);
	JS_SESSION_FUNCTION_DEF(SendEvent);
	JS_SESSION_FUNCTION_DEF(Hangup);
	JS_SESSION_FUNCTION_DEF(Execute);
	JS_SESSION_FUNCTION_DEF(Detach);
	JS_SESSION_FUNCTION_DEF(Sleep);
	JS_SESSION_FUNCTION_DEF(Bridge);
	JS_SESSION_GET_PROPERTY_DEF(GetProperty);
};

class FSInputCallbackState
{
public:
	FSSession *session_state;
	char code_buffer[1024];
	size_t code_buffer_len;
	char ret_buffer[1024];
	int ret_buffer_len;
	int digit_count;
	v8::Persistent<v8::Function> function;
	v8::Persistent<v8::Value> arg;
	v8::Persistent<v8::Value> ret;
	void *extra;
	FSSession *jss_a;
	FSSession *jss_b;
	v8::Persistent<v8::Object> session_obj_a;
	v8::Persistent<v8::Object> session_obj_b;

	FSInputCallbackState(void);
	~FSInputCallbackState(void);
};

#endif /* FS_SESSION_H */

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
