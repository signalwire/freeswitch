/* 
 * FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
 * Copyright (C) 2005-2010, Anthony Minessale II <anthm@freeswitch.org>
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
 * Anthony Minessale II <anthm@freeswitch.org>
 * Portions created by the Initial Developer are Copyright (C)
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 * 
 * Anthony Minessale II <anthm@freeswitch.org>
 *
 *
 * mod_spidermonkey.c -- Javascript Module
 *
 */
#ifndef HAVE_CURL
#define HAVE_CURL
#endif
#include "mod_spidermonkey.h"

#ifdef HAVE_CURL
#include <curl/curl.h>
#endif
static int foo = 0;
static jsval check_hangup_hook(struct js_session *jss, jsval * rp);

SWITCH_MODULE_LOAD_FUNCTION(mod_spidermonkey_load);
SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_spidermonkey_shutdown);
SWITCH_MODULE_DEFINITION_EX(mod_spidermonkey, mod_spidermonkey_load, mod_spidermonkey_shutdown, NULL, SMODF_GLOBAL_SYMBOLS);

#define METHOD_SANITY_CHECK()  if (!jss || !jss->session) {				\
		eval_some_js("~throw new Error(\"You must call the session.originate method before calling this method!\");", cx, obj, rval); \
		*rval = BOOLEAN_TO_JSVAL(JS_FALSE);								\
		return JS_FALSE;												\
	} else check_hangup_hook(jss, NULL)

#define CHANNEL_SANITY_CHECK() do {										\
		if (!switch_channel_ready(channel)) {							\
			eval_some_js("~throw new Error(\"Session is not active!\");", cx, obj, rval); \
			*rval = BOOLEAN_TO_JSVAL(JS_FALSE);							\
			return JS_FALSE;											\
		}																\
		if (!((switch_channel_test_flag(channel, CF_ANSWERED) || switch_channel_test_flag(channel, CF_EARLY_MEDIA)))) {							\
			eval_some_js("~throw new Error(\"Session is not answered!\");", cx, obj, rval); \
			*rval = BOOLEAN_TO_JSVAL(JS_FALSE);							\
			return JS_FALSE;											\
		}																\
	} while (foo == 1)

#define CHANNEL_SANITY_CHECK_ANSWER() do {										\
		if (!switch_channel_ready(channel)) {							\
			eval_some_js("~throw new Error(\"Session is not active!\");", cx, obj, rval); \
			*rval = BOOLEAN_TO_JSVAL(JS_FALSE);							\
			return JS_FALSE;											\
		}																\
	} while (foo == 1)

#define CHANNEL_MEDIA_SANITY_CHECK() do {								\
		if (!switch_channel_media_ready(channel)) {						\
			eval_some_js("~throw new Error(\"Session is not in media mode!\");", cx, obj, rval); \
			*rval = BOOLEAN_TO_JSVAL(JS_FALSE);							\
			return JS_FALSE;											\
		}																\
	} while (foo == 1)

static void session_destroy(JSContext * cx, JSObject * obj);
static JSBool session_construct(JSContext * cx, JSObject * obj, uintN argc, jsval * argv, jsval * rval);
static JSBool session_originate(JSContext * cx, JSObject * obj, uintN argc, jsval * argv, jsval * rval);
static JSBool session_set_callerdata(JSContext * cx, JSObject * obj, uintN argc, jsval * argv, jsval * rval);
static switch_api_interface_t *js_run_interface = NULL;
static switch_api_interface_t *jsapi_interface = NULL;

static struct {
	size_t gStackChunkSize;
	jsuword gStackBase;
	int gExitCode;
	JSBool gQuitting;
	FILE *gErrFile;
	FILE *gOutFile;
	int stackDummy;
	JSRuntime *rt;
} globals;

static JSClass global_class = {
	"Global", JSCLASS_HAS_PRIVATE,
	JS_PropertyStub, JS_PropertyStub, JS_PropertyStub, JS_PropertyStub,
	JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub, JS_FinalizeStub
};


static struct {
	switch_hash_t *mod_hash;
	switch_hash_t *load_hash;
	switch_memory_pool_t *pool;
} module_manager;

struct sm_loadable_module {
	char *filename;
	void *lib;
	const sm_module_interface_t *module_interface;
	spidermonkey_init_t spidermonkey_init;
};
typedef struct sm_loadable_module sm_loadable_module_t;

typedef enum {
	S_HUP = (1 << 0),
} session_flag_t;

struct input_callback_state {
	struct js_session *session_state;
	char code_buffer[1024];
	size_t code_buffer_len;
	char ret_buffer[1024];
	int ret_buffer_len;
	int digit_count;
	JSFunction *function;
	jsval arg;
	jsval ret;
	JSContext *cx;
	JSObject *obj;
	jsrefcount saveDepth;
	void *extra;
	struct js_session *jss_a;
	struct js_session *jss_b;
	JSObject *session_obj_a;
	JSObject *session_obj_b;
};

struct fileio_obj {
	char *path;
	unsigned int flags;
	switch_file_t *fd;
	switch_memory_pool_t *pool;
	char *buf;
	switch_size_t buflen;
	int32 bufsize;
};

struct request_obj {
	const char *cmd;
	switch_core_session_t *session;
	switch_stream_handle_t *stream;
};


/* Request Object */
/*********************************************************************************/

static JSBool request_write(JSContext * cx, JSObject * obj, uintN argc, jsval * argv, jsval * rval)
{
	struct request_obj *ro = JS_GetPrivate(cx, obj);

	if (!ro) {
		*rval = BOOLEAN_TO_JSVAL(JS_FALSE);
		return JS_TRUE;
	}

	if (argc > 0) {
		char *string = JS_GetStringBytes(JS_ValueToString(cx, argv[0]));
		ro->stream->write_function(ro->stream, "%s", string);
		*rval = BOOLEAN_TO_JSVAL(JS_TRUE);
		return JS_TRUE;
	}

	*rval = BOOLEAN_TO_JSVAL(JS_FALSE);
	return JS_TRUE;
}

static JSBool request_add_header(JSContext * cx, JSObject * obj, uintN argc, jsval * argv, jsval * rval)
{
	struct request_obj *ro = JS_GetPrivate(cx, obj);

	if (!ro) {
		*rval = BOOLEAN_TO_JSVAL(JS_FALSE);
		return JS_TRUE;
	}

	if (argc > 1) {
		char *hname = JS_GetStringBytes(JS_ValueToString(cx, argv[0]));
		char *hval = JS_GetStringBytes(JS_ValueToString(cx, argv[1]));
		switch_event_add_header_string(ro->stream->param_event, SWITCH_STACK_BOTTOM, hname, hval);
		*rval = BOOLEAN_TO_JSVAL(JS_TRUE);
		return JS_TRUE;
	}

	*rval = BOOLEAN_TO_JSVAL(JS_FALSE);
	return JS_TRUE;
}

static JSBool request_get_header(JSContext * cx, JSObject * obj, uintN argc, jsval * argv, jsval * rval)
{
	struct request_obj *ro = JS_GetPrivate(cx, obj);

	if (!ro) {
		*rval = BOOLEAN_TO_JSVAL(JS_FALSE);
		return JS_TRUE;
	}


	if (argc > 0) {
		char *hname = JS_GetStringBytes(JS_ValueToString(cx, argv[0]));
		char *val = switch_event_get_header(ro->stream->param_event, hname);
		*rval = STRING_TO_JSVAL(JS_NewStringCopyZ(cx, val));
		return JS_TRUE;
	}

	*rval = BOOLEAN_TO_JSVAL(JS_FALSE);
	return JS_TRUE;
}

static JSBool request_dump_env(JSContext * cx, JSObject * obj, uintN argc, jsval * argv, jsval * rval)
{
	struct request_obj *ro = JS_GetPrivate(cx, obj);
	char *how = "text";

	if (!ro) {
		*rval = BOOLEAN_TO_JSVAL(JS_FALSE);
		return JS_TRUE;
	}

	if (argc > 0) {
		how = JS_GetStringBytes(JS_ValueToString(cx, argv[0]));
	}

	if (!strcasecmp(how, "xml")) {
		switch_xml_t xml;
		char *xmlstr;
		if ((xml = switch_event_xmlize(ro->stream->param_event, SWITCH_VA_NONE))) {
			xmlstr = switch_xml_toxml(xml, SWITCH_FALSE);
			*rval = STRING_TO_JSVAL(JS_NewStringCopyZ(cx, xmlstr));
			return JS_TRUE;
		}
	} else {
		char *buf;
		switch_event_serialize(ro->stream->param_event, &buf, SWITCH_TRUE);
		if (buf) {
			*rval = STRING_TO_JSVAL(JS_NewStringCopyZ(cx, buf));
			free(buf);
			return JS_TRUE;
		}
	}

	*rval = BOOLEAN_TO_JSVAL(JS_FALSE);
	return JS_FALSE;
}

static void request_destroy(JSContext * cx, JSObject * obj)
{

}

enum request_tinyid {
	REQUEST_COMMAND
};

static JSFunctionSpec request_methods[] = {
	{"write", request_write, 1},
	{"getHeader", request_get_header, 1},
	{"addHeader", request_add_header, 1},
	{"dumpENV", request_dump_env, 1},
	{0}
};

static JSPropertySpec request_props[] = {
	{"command", REQUEST_COMMAND, JSPROP_READONLY | JSPROP_PERMANENT},
	{0}
};

static JSBool request_getProperty(JSContext * cx, JSObject * obj, jsval id, jsval * vp)
{
	JSBool res = JS_TRUE;
	struct request_obj *ro = JS_GetPrivate(cx, obj);
	char *name;
	int param = 0;

	if (!ro) {
		*vp = BOOLEAN_TO_JSVAL(JS_FALSE);
		return JS_TRUE;
	}


	name = JS_GetStringBytes(JS_ValueToString(cx, id));
	/* numbers are our props anything else is a method */
	if (name[0] >= 48 && name[0] <= 57) {
		param = atoi(name);
	} else {
		return JS_TRUE;
	}

	switch (param) {
	case REQUEST_COMMAND:
		*vp = STRING_TO_JSVAL(JS_NewStringCopyZ(cx, ro->cmd));
		break;
	}

	return res;
}

JSClass request_class = {
	"Request", JSCLASS_HAS_PRIVATE,
	JS_PropertyStub, JS_PropertyStub, request_getProperty, DEFAULT_SET_PROPERTY,
	JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub, request_destroy, NULL, NULL, NULL, NULL
};

static JSObject *new_request(JSContext * cx, JSObject * obj, struct request_obj *ro)
{
	JSObject *Request;
	if ((Request = JS_DefineObject(cx, obj, "request", &request_class, NULL, 0))) {
		if ((JS_SetPrivate(cx, Request, ro) && JS_DefineProperties(cx, Request, request_props) && JS_DefineFunctions(cx, Request, request_methods))) {
			return Request;
		}
	}

	return NULL;
}

struct pcre_obj {
	switch_regex_t *re;
	char *string;
	int proceed;
	int ovector[30];
	int freed;
};

/* Pcre Object */
/*********************************************************************************/
static JSBool pcre_construct(JSContext * cx, JSObject * obj, uintN argc, jsval * argv, jsval * rval)
{
	struct pcre_obj *pcre_obj;

	if (!((pcre_obj = malloc(sizeof(*pcre_obj))))) {
		abort();
	}
	memset(pcre_obj, 0, sizeof(*pcre_obj));
	JS_SetPrivate(cx, obj, pcre_obj);
	return JS_TRUE;

}

static void pcre_destroy(JSContext * cx, JSObject * obj)
{
	struct pcre_obj *pcre_obj = JS_GetPrivate(cx, obj);

	if (pcre_obj) {
		if (!pcre_obj->freed && pcre_obj->re) {
			switch_regex_safe_free(pcre_obj->re);
			switch_safe_free(pcre_obj->string);
		}
		switch_safe_free(pcre_obj);
	}
}

static JSBool pcre_compile(JSContext * cx, JSObject * obj, uintN argc, jsval * argv, jsval * rval)
{
	struct pcre_obj *pcre_obj = JS_GetPrivate(cx, obj);
	char *string, *regex_string;

	if (argc > 1) {
		string = JS_GetStringBytes(JS_ValueToString(cx, argv[0]));
		regex_string = JS_GetStringBytes(JS_ValueToString(cx, argv[1]));
		switch_regex_safe_free(pcre_obj->re);
		switch_safe_free(pcre_obj->string);
		pcre_obj->string = strdup(string);
		pcre_obj->proceed = switch_regex_perform(pcre_obj->string, regex_string, &pcre_obj->re, pcre_obj->ovector,
												 sizeof(pcre_obj->ovector) / sizeof(pcre_obj->ovector[0]));
		*rval = BOOLEAN_TO_JSVAL(pcre_obj->proceed ? JS_TRUE : JS_FALSE);
	} else {
		eval_some_js("~throw new Error(\"Invalid Args\");", cx, obj, rval);
		return JS_FALSE;
	}

	return JS_TRUE;
}

static JSBool pcre_substitute(JSContext * cx, JSObject * obj, uintN argc, jsval * argv, jsval * rval)
{
	struct pcre_obj *pcre_obj = JS_GetPrivate(cx, obj);
	char *subst_string;
	char *substituted;

	if (!pcre_obj->proceed) {
		eval_some_js("~throw new Error(\"REGEX is not compiled or has no matches\");", cx, obj, rval);
		return JS_FALSE;
	}

	if (argc > 0) {
		uint32_t len;
		subst_string = JS_GetStringBytes(JS_ValueToString(cx, argv[0]));
		len = (uint32_t) (strlen(pcre_obj->string) + strlen(subst_string) + 10) * pcre_obj->proceed;
		substituted = malloc(len);
		switch_assert(substituted != NULL);
		switch_perform_substitution(pcre_obj->re, pcre_obj->proceed, subst_string, pcre_obj->string, substituted, len, pcre_obj->ovector);
		*rval = STRING_TO_JSVAL(JS_NewStringCopyZ(cx, substituted));
		free(substituted);
	} else {
		eval_some_js("~throw new Error(\"Invalid Args\");", cx, obj, rval);
		return JS_FALSE;
	}

	return JS_TRUE;
}

enum pcre_tinyid {
	PCRE_READY
};

static JSFunctionSpec pcre_methods[] = {
	{"compile", pcre_compile, 2},
	{"substitute", pcre_substitute, 2},
	{0}
};

static JSPropertySpec pcre_props[] = {
	{"ready", PCRE_READY, JSPROP_READONLY | JSPROP_PERMANENT},
	{0}
};

static JSBool pcre_getProperty(JSContext * cx, JSObject * obj, jsval id, jsval * vp)
{
	JSBool res = JS_TRUE;
	struct pcre_obj *pcre_obj = JS_GetPrivate(cx, obj);
	char *name;
	int param = 0;

	if (!pcre_obj) {
		*vp = BOOLEAN_TO_JSVAL(JS_FALSE);
		return JS_TRUE;
	}


	name = JS_GetStringBytes(JS_ValueToString(cx, id));
	/* numbers are our props anything else is a method */
	if (name[0] >= 48 && name[0] <= 57) {
		param = atoi(name);
	} else {
		return JS_TRUE;
	}

	switch (param) {
	case PCRE_READY:
		*vp = BOOLEAN_TO_JSVAL(JS_TRUE);
		break;
	}

	return res;
}

JSClass pcre_class = {
	"PCRE", JSCLASS_HAS_PRIVATE,
	JS_PropertyStub, JS_PropertyStub, pcre_getProperty, DEFAULT_SET_PROPERTY,
	JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub, pcre_destroy, NULL, NULL, NULL,
	pcre_construct
};

struct event_obj {
	switch_event_t *event;
	int freed;
};

/* Event Object */
/*********************************************************************************/
static JSBool event_construct(JSContext * cx, JSObject * obj, uintN argc, jsval * argv, jsval * rval)
{
	if (argc > 0) {
		switch_event_t *event;
		struct event_obj *eo;
		switch_event_types_t etype;
		char *ename = JS_GetStringBytes(JS_ValueToString(cx, argv[0]));

		if ((eo = malloc(sizeof(*eo)))) {

			if (switch_name_event(ename, &etype) != SWITCH_STATUS_SUCCESS) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "Unknown event %s\n", ename);
				*rval = BOOLEAN_TO_JSVAL(JS_FALSE);
				return JS_TRUE;
			}

			if (etype == SWITCH_EVENT_CUSTOM) {
				char *subclass_name;
				if (argc > 1) {
					subclass_name = JS_GetStringBytes(JS_ValueToString(cx, argv[1]));
				} else {
					subclass_name = "none";
				}

				if (switch_event_create_subclass(&event, etype, subclass_name) != SWITCH_STATUS_SUCCESS) {
					*rval = BOOLEAN_TO_JSVAL(JS_FALSE);
					return JS_TRUE;
				}

			} else {
				if (switch_event_create(&event, etype) != SWITCH_STATUS_SUCCESS) {
					*rval = BOOLEAN_TO_JSVAL(JS_FALSE);
					return JS_TRUE;
				}
			}

			eo->event = event;
			eo->freed = 0;

			JS_SetPrivate(cx, obj, eo);
			return JS_TRUE;
		}
	}

	return JS_FALSE;
}

static void event_destroy(JSContext * cx, JSObject * obj)
{
	struct event_obj *eo = JS_GetPrivate(cx, obj);

	if (eo) {
		if (!eo->freed && eo->event) {
			switch_event_destroy(&eo->event);
		}
		switch_safe_free(eo);
	}
}

static JSBool event_add_header(JSContext * cx, JSObject * obj, uintN argc, jsval * argv, jsval * rval)
{
	struct event_obj *eo = JS_GetPrivate(cx, obj);

	if (!eo || eo->freed) {
		*rval = BOOLEAN_TO_JSVAL(JS_FALSE);
		return JS_TRUE;
	}

	if (argc > 1) {
		char *hname = JS_GetStringBytes(JS_ValueToString(cx, argv[0]));
		char *hval = JS_GetStringBytes(JS_ValueToString(cx, argv[1]));
		switch_event_add_header_string(eo->event, SWITCH_STACK_BOTTOM, hname, hval);
		*rval = BOOLEAN_TO_JSVAL(JS_TRUE);
		return JS_TRUE;
	}

	*rval = BOOLEAN_TO_JSVAL(JS_FALSE);
	return JS_TRUE;
}

static JSBool event_get_header(JSContext * cx, JSObject * obj, uintN argc, jsval * argv, jsval * rval)
{
	struct event_obj *eo = JS_GetPrivate(cx, obj);

	if (!eo) {
		*rval = BOOLEAN_TO_JSVAL(JS_FALSE);
		return JS_TRUE;
	}

	if (argc > 0) {
		char *hname = JS_GetStringBytes(JS_ValueToString(cx, argv[0]));
		char *val = switch_event_get_header(eo->event, hname);
		*rval = STRING_TO_JSVAL(JS_NewStringCopyZ(cx, val));
		return JS_TRUE;
	}

	*rval = BOOLEAN_TO_JSVAL(JS_FALSE);
	return JS_TRUE;
}

static JSBool event_add_body(JSContext * cx, JSObject * obj, uintN argc, jsval * argv, jsval * rval)
{
	struct event_obj *eo = JS_GetPrivate(cx, obj);

	if (!eo || eo->freed) {
		*rval = BOOLEAN_TO_JSVAL(JS_FALSE);
		return JS_TRUE;
	}

	if (argc > 0) {
		char *body = JS_GetStringBytes(JS_ValueToString(cx, argv[0]));
		switch_event_add_body(eo->event, "%s", body);
		*rval = BOOLEAN_TO_JSVAL(JS_TRUE);
		return JS_TRUE;
	}

	*rval = BOOLEAN_TO_JSVAL(JS_FALSE);
	return JS_TRUE;
}

static JSBool event_get_body(JSContext * cx, JSObject * obj, uintN argc, jsval * argv, jsval * rval)
{
	struct event_obj *eo = JS_GetPrivate(cx, obj);

	if (!eo) {
		*rval = BOOLEAN_TO_JSVAL(JS_FALSE);
		return JS_TRUE;
	}

	*rval = STRING_TO_JSVAL(JS_NewStringCopyZ(cx, switch_event_get_body(eo->event)));

	return JS_TRUE;
}

static JSBool event_get_type(JSContext * cx, JSObject * obj, uintN argc, jsval * argv, jsval * rval)
{
	struct event_obj *eo = JS_GetPrivate(cx, obj);

	if (!eo) {
		*rval = BOOLEAN_TO_JSVAL(JS_FALSE);
		return JS_TRUE;
	}

	*rval = STRING_TO_JSVAL(JS_NewStringCopyZ(cx, switch_event_name(eo->event->event_id)));

	return JS_TRUE;
}

static JSBool event_serialize(JSContext * cx, JSObject * obj, uintN argc, jsval * argv, jsval * rval)
{
	struct event_obj *eo = JS_GetPrivate(cx, obj);
	char *buf;
	uint8_t isxml = 0;

	if (!eo) {
		*rval = BOOLEAN_TO_JSVAL(JS_FALSE);
		return JS_TRUE;
	}

	if (argc > 0) {
		char *arg = JS_GetStringBytes(JS_ValueToString(cx, argv[0]));
		if (!strcasecmp(arg, "xml")) {
			isxml++;
		}
	}

	if (isxml) {
		switch_xml_t xml;
		char *xmlstr;
		if ((xml = switch_event_xmlize(eo->event, SWITCH_VA_NONE))) {
			xmlstr = switch_xml_toxml(xml, SWITCH_FALSE);
			*rval = STRING_TO_JSVAL(JS_NewStringCopyZ(cx, xmlstr));
			switch_xml_free(xml);
			free(xmlstr);
		} else {
			*rval = BOOLEAN_TO_JSVAL(JS_FALSE);
		}
	} else {
		if (switch_event_serialize(eo->event, &buf, SWITCH_TRUE) == SWITCH_STATUS_SUCCESS) {
			*rval = STRING_TO_JSVAL(JS_NewStringCopyZ(cx, buf));
			switch_safe_free(buf);
		}
	}

	return JS_TRUE;
}

static JSBool event_fire(JSContext * cx, JSObject * obj, uintN argc, jsval * argv, jsval * rval)
{
	struct event_obj *eo = JS_GetPrivate(cx, obj);

	if (eo) {
		switch_event_fire(&eo->event);
		JS_SetPrivate(cx, obj, NULL);
		switch_safe_free(eo);
		*rval = BOOLEAN_TO_JSVAL(JS_TRUE);
		return JS_TRUE;
	}

	*rval = BOOLEAN_TO_JSVAL(JS_FALSE);
	return JS_TRUE;
}

static JSBool event_destroy_(JSContext * cx, JSObject * obj, uintN argc, jsval * argv, jsval * rval)
{
	struct event_obj *eo = JS_GetPrivate(cx, obj);

	if (eo) {
		if (!eo->freed) {
			switch_event_destroy(&eo->event);
		}
		JS_SetPrivate(cx, obj, NULL);
		switch_safe_free(eo);
		*rval = BOOLEAN_TO_JSVAL(JS_TRUE);
		return JS_TRUE;
	}

	*rval = BOOLEAN_TO_JSVAL(JS_FALSE);
	return JS_TRUE;
}

enum event_tinyid {
	EVENT_READY
};

static JSFunctionSpec event_methods[] = {
	{"addHeader", event_add_header, 1},
	{"getHeader", event_get_header, 1},
	{"addBody", event_add_body, 1},
	{"getBody", event_get_body, 1},
	{"getType", event_get_type, 1},
	{"serialize", event_serialize, 0},
	{"fire", event_fire, 0},
	{"destroy", event_destroy_, 0},
	{0}
};

static JSPropertySpec event_props[] = {
	{"ready", EVENT_READY, JSPROP_READONLY | JSPROP_PERMANENT},
	{0}
};

static JSBool event_getProperty(JSContext * cx, JSObject * obj, jsval id, jsval * vp)
{
	JSBool res = JS_TRUE;
	switch_event_t *event = JS_GetPrivate(cx, obj);
	char *name;
	int param = 0;

	if (!event) {
		*vp = BOOLEAN_TO_JSVAL(JS_FALSE);
		return JS_TRUE;
	}

	name = JS_GetStringBytes(JS_ValueToString(cx, id));
	/* numbers are our props anything else is a method */
	if (name[0] >= 48 && name[0] <= 57) {
		param = atoi(name);
	} else {
		return JS_TRUE;
	}

	switch (param) {
	case EVENT_READY:
		*vp = BOOLEAN_TO_JSVAL(JS_TRUE);
		break;
	}

	return res;
}

JSClass event_class = {
	"Event", JSCLASS_HAS_PRIVATE,
	JS_PropertyStub, JS_PropertyStub, event_getProperty, DEFAULT_SET_PROPERTY,
	JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub, event_destroy, NULL, NULL, NULL,
	event_construct
};

/* Dtmf Object */
/*********************************************************************************/
static JSBool dtmf_construct(JSContext * cx, JSObject * obj, uintN argc, jsval * argv, jsval * rval)
{
	switch_dtmf_t *dtmf;
	int32 duration = switch_core_default_dtmf_duration(0);
	char *ename;

	if (argc > 0) {
		ename = JS_GetStringBytes(JS_ValueToString(cx, argv[0]));
	} else {
		eval_some_js("~throw new Error(\"Invalid Args\");", cx, obj, rval);
		return JS_FALSE;
	}

	if (argc > 1) {
		JS_ValueToInt32(cx, argv[1], &duration);
		if (duration <= 0) {
			duration = switch_core_default_dtmf_duration(0);
		}
	}

	if ((dtmf = malloc(sizeof(*dtmf)))) {
		JS_SetPrivate(cx, obj, dtmf);
		return JS_TRUE;
	}

	return JS_FALSE;
}

static void dtmf_destroy(JSContext * cx, JSObject * obj)
{
	switch_dtmf_t *dtmf = JS_GetPrivate(cx, obj);

	if (dtmf) {
		switch_safe_free(dtmf);
		JS_SetPrivate(cx, obj, NULL);
	}
}

enum dtmf_tinyid {
	DTMF_DIGIT, DTMF_DURATION
};

static JSFunctionSpec dtmf_methods[] = {
	{0}
};


static JSPropertySpec dtmf_props[] = {
	{"digit", DTMF_DIGIT, JSPROP_READONLY | JSPROP_PERMANENT},
	{"duration", DTMF_DURATION, JSPROP_READONLY | JSPROP_PERMANENT},
	{0}
};

static JSBool dtmf_getProperty(JSContext * cx, JSObject * obj, jsval id, jsval * vp)
{
	JSBool res = JS_TRUE;
	switch_dtmf_t *dtmf = JS_GetPrivate(cx, obj);
	char *name;
	int param = 0;

	if (!dtmf) {
		*vp = BOOLEAN_TO_JSVAL(JS_FALSE);
		return JS_TRUE;
	}

	name = JS_GetStringBytes(JS_ValueToString(cx, id));
	/* numbers are our props anything else is a method */
	if (name[0] >= 48 && name[0] <= 57) {
		param = atoi(name);
	} else {
		return JS_TRUE;
	}

	switch (param) {
	case DTMF_DIGIT:
		{
			char tmp[2] = { dtmf->digit, '\0' };
			*vp = STRING_TO_JSVAL(JS_NewStringCopyZ(cx, tmp));
		}
		break;

	case DTMF_DURATION:
		{
			*vp = INT_TO_JSVAL((int) dtmf->duration);
		}
		break;
	}

	return res;
}

JSClass dtmf_class = {
	"DTMF", JSCLASS_HAS_PRIVATE,
	JS_PropertyStub, JS_PropertyStub, dtmf_getProperty, DEFAULT_SET_PROPERTY,
	JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub, dtmf_destroy, NULL, NULL, NULL,
	dtmf_construct
};

static void js_error(JSContext * cx, const char *message, JSErrorReport * report)
{
	const char *filename = __FILE__;
	int line = __LINE__;
	const char *text = "";
	char *ex = "";

	if (message && report) {
		if (report->filename) {
			filename = report->filename;
		}
		line = report->lineno;
		if (report->linebuf) {
			text = report->linebuf;
			ex = "near ";
		}
	}

	if (!message) {
		message = "(N/A)";
	}

	switch_log_printf(SWITCH_CHANNEL_ID_LOG, filename, modname, line, NULL, SWITCH_LOG_ERROR, "%s %s%s\n", ex, message, text);
}

static switch_status_t sm_load_file(char *filename)
{
	sm_loadable_module_t *module = NULL;
	switch_dso_lib_t dso = NULL;
	switch_status_t status = SWITCH_STATUS_SUCCESS;
	switch_loadable_module_function_table_t *function_handle = NULL;
	spidermonkey_init_t spidermonkey_init = NULL;
	const sm_module_interface_t *module_interface = NULL, *mp;
	char *derr = NULL;
	const char *err = NULL;

	switch_assert(filename != NULL);

	if (!(dso = switch_dso_open(filename, 1, &derr))) {
		status = SWITCH_STATUS_FALSE;
	}

	if (derr || status != SWITCH_STATUS_SUCCESS) {
		err = derr;
		goto err;
	}

	function_handle = switch_dso_data_sym(dso, "spidermonkey_init", &derr);

	if (!function_handle || derr) {
		status = SWITCH_STATUS_FALSE;
		err = derr;
		goto err;
	}

	spidermonkey_init = (spidermonkey_init_t) (intptr_t) function_handle;

	if (spidermonkey_init == NULL) {
		err = "Cannot Load";
		goto err;
	}

	if (spidermonkey_init(&module_interface) != SWITCH_STATUS_SUCCESS) {
		err = "Module load routine returned an error";
		goto err;
	}

	if (!(module = (sm_loadable_module_t *) switch_core_permanent_alloc(sizeof(*module)))) {
		err = "Could not allocate memory\n";
	}

  err:

	if (err || !module) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Error Loading module %s\n**%s**\n", filename, switch_str_nil(err));
		switch_safe_free(derr);
		return SWITCH_STATUS_GENERR;
	}

	module->filename = switch_core_permanent_strdup(filename);
	module->spidermonkey_init = spidermonkey_init;
	module->module_interface = module_interface;

	module->lib = dso;

	switch_core_hash_insert(module_manager.mod_hash, (char *) module->filename, (void *) module);

	for (mp = module->module_interface; mp; mp = mp->next) {
		switch_core_hash_insert(module_manager.load_hash, (char *) mp->name, (void *) mp);
	}

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CONSOLE, "Successfully Loaded [%s]\n", module->filename);

	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t sm_load_module(const char *dir, const char *fname)
{
	switch_size_t len = 0;
	char *path;
	char *file;

#ifdef WIN32
	const char *ext = ".dll";
#else
	const char *ext = ".so";
#endif

	if ((file = switch_core_strdup(module_manager.pool, fname)) == 0) {
		return SWITCH_STATUS_FALSE;
	}

	if (*file == '/') {
		path = switch_core_strdup(module_manager.pool, file);
	} else {
		if (strchr(file, '.')) {
			len = strlen(dir);
			len += strlen(file);
			len += 4;
			path = (char *) switch_core_alloc(module_manager.pool, len);
			switch_snprintf(path, len, "%s%s%s", dir, SWITCH_PATH_SEPARATOR, file);
		} else {
			len = strlen(dir);
			len += strlen(file);
			len += 8;
			path = (char *) switch_core_alloc(module_manager.pool, len);
			switch_snprintf(path, len, "%s%s%s%s", dir, SWITCH_PATH_SEPARATOR, file, ext);
		}
	}

	return sm_load_file(path);
}

static switch_status_t load_modules(void)
{
	char *cf = "spidermonkey.conf";
	switch_xml_t cfg, xml;
	unsigned int count = 0;

#ifdef WIN32
	const char *ext = ".dll";
	const char *EXT = ".DLL";
#elif defined (MACOSX) || defined (DARWIN)
	const char *ext = ".dylib";
	const char *EXT = ".DYLIB";
#else
	const char *ext = ".so";
	const char *EXT = ".SO";
#endif

	memset(&module_manager, 0, sizeof(module_manager));
	switch_core_new_memory_pool(&module_manager.pool);

	switch_core_hash_init(&module_manager.mod_hash, module_manager.pool);
	switch_core_hash_init(&module_manager.load_hash, module_manager.pool);

	if ((xml = switch_xml_open_cfg(cf, &cfg, NULL))) {
		switch_xml_t mods, ld;

		if ((mods = switch_xml_child(cfg, "modules"))) {
			for (ld = switch_xml_child(mods, "load"); ld; ld = ld->next) {
				const char *val = switch_xml_attr_soft(ld, "module");
				if (!zstr(val) && strchr(val, '.') && !strstr(val, ext) && !strstr(val, EXT)) {
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CONSOLE, "Invalid extension for %s\n", val);
					continue;
				}
				sm_load_module(SWITCH_GLOBAL_dirs.mod_dir, val);
				count++;
			}
		}
		switch_xml_free(xml);

	} else {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CONSOLE, "Open of %s failed\n", cf);
	}

	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t init_js(void)
{
	memset(&globals, 0, sizeof(globals));
	globals.gQuitting = JS_FALSE;
	globals.gErrFile = NULL;
	globals.gOutFile = NULL;
	globals.gStackChunkSize = 8192;
	globals.gStackBase = (jsuword) & globals.stackDummy;
	globals.gErrFile = stderr;
	globals.gOutFile = stdout;

	if (!(globals.rt = JS_NewRuntime(64L * 1024L * 1024L))) {
		return SWITCH_STATUS_FALSE;
	}

	if (load_modules() != SWITCH_STATUS_SUCCESS) {
		return SWITCH_STATUS_FALSE;
	}

	return SWITCH_STATUS_SUCCESS;
}

JSObject *new_js_event(switch_event_t *event, char *name, JSContext * cx, JSObject * obj)
{
	struct event_obj *eo;
	JSObject *Event = NULL;

	if ((eo = malloc(sizeof(*eo)))) {
		eo->event = event;
		eo->freed = 1;
		if ((Event = JS_DefineObject(cx, obj, name, &event_class, NULL, 0))) {
			if ((JS_SetPrivate(cx, Event, eo) && JS_DefineProperties(cx, Event, event_props) && JS_DefineFunctions(cx, Event, event_methods))) {
			}
		}
	}
	return Event;
}

JSObject *new_js_dtmf(switch_dtmf_t *dtmf, char *name, JSContext * cx, JSObject * obj)
{
	JSObject *DTMF = NULL;
	switch_dtmf_t *ddtmf;

	if ((ddtmf = malloc(sizeof(*ddtmf)))) {
		*ddtmf = *dtmf;
		if ((DTMF = JS_DefineObject(cx, obj, name, &dtmf_class, NULL, 0))) {
			JS_SetPrivate(cx, DTMF, ddtmf);
			JS_DefineProperties(cx, DTMF, dtmf_props);
			JS_DefineFunctions(cx, DTMF, dtmf_methods);
		}
	}
	return DTMF;
}

#define MAX_STACK_DEPTH 2

static switch_status_t js_common_callback(switch_core_session_t *session, void *input, switch_input_type_t itype, void *buf, unsigned int buflen)
{
	switch_event_t *event = NULL;
	struct input_callback_state *cb_state = buf;
	struct js_session *jss = cb_state->session_state;
	uintN argc = 0;
	jsval argv[4];
	JSObject *Event = NULL;
	jsval ret, nval, *rval = &nval;
	JSContext *cx = cb_state->cx;
	JSObject *obj = cb_state->obj;
	switch_uuid_t uuid;
	char uuid_str[SWITCH_UUID_FORMATTED_LENGTH + 1];
	char var_name[SWITCH_UUID_FORMATTED_LENGTH + 25];
	char *p;
	switch_status_t status = SWITCH_STATUS_FALSE;

	if (!jss || !jss->session) {
		return SWITCH_STATUS_FALSE;
	}

	if (++jss->stack_depth > MAX_STACK_DEPTH) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Maximum recursive callback limit %d reached.\n", MAX_STACK_DEPTH);
		jss->stack_depth--;
		return SWITCH_STATUS_FALSE;
	}

	switch_uuid_get(&uuid);
	switch_uuid_format(uuid_str, &uuid);

	switch_snprintf(var_name, sizeof(var_name), "__event_%s", uuid_str);
	for (p = var_name; p && *p; p++) {
		if (*p == '-') {
			*p = '_';
		}
	}

	JS_ResumeRequest(cb_state->cx, cb_state->saveDepth);
	METHOD_SANITY_CHECK();

	if (cb_state->jss_a && cb_state->jss_a->session && cb_state->jss_a->session == session) {
		argv[argc++] = OBJECT_TO_JSVAL(cb_state->session_obj_a);
	} else if (cb_state->jss_b && cb_state->jss_b->session && cb_state->jss_b->session == session) {
		argv[argc++] = OBJECT_TO_JSVAL(cb_state->session_obj_b);
	} else {
		argv[argc++] = OBJECT_TO_JSVAL(cb_state->session_state->obj);
	}

	switch (itype) {
	case SWITCH_INPUT_TYPE_EVENT:
		if ((event = (switch_event_t *) input)) {
			if ((Event = new_js_event(event, var_name, cb_state->cx, cb_state->obj))) {
				argv[argc++] = STRING_TO_JSVAL(JS_NewStringCopyZ(cb_state->cx, "event"));
				argv[argc++] = OBJECT_TO_JSVAL(Event);
			}
		}
		if (!Event) {
			goto done;
		}
		break;
	case SWITCH_INPUT_TYPE_DTMF:
		{
			switch_dtmf_t *dtmf = (switch_dtmf_t *) input;

			if (dtmf) {
				if ((Event = new_js_dtmf(dtmf, var_name, cb_state->cx, cb_state->obj))) {
					argv[argc++] = STRING_TO_JSVAL(JS_NewStringCopyZ(cb_state->cx, "dtmf"));
					argv[argc++] = OBJECT_TO_JSVAL(Event);
				} else {
					goto done;
				}
			}
		}
		break;
	}

	if (cb_state->arg) {
		argv[argc++] = cb_state->arg;
	}

	check_hangup_hook(jss, &ret);

	if (ret == JS_TRUE) {
		JS_CallFunction(cb_state->cx, cb_state->obj, cb_state->function, argc, argv, &cb_state->ret);
	}

	status = SWITCH_STATUS_SUCCESS;
  done:
	cb_state->saveDepth = JS_SuspendRequest(cb_state->cx);
	jss->stack_depth--;
	return status;
}

static switch_status_t js_stream_input_callback(switch_core_session_t *session, void *input, switch_input_type_t itype, void *buf, unsigned int buflen)
{
	char *ret;
	switch_status_t status;
	struct input_callback_state *cb_state = buf;
	switch_file_handle_t *fh = cb_state->extra;
	struct js_session *jss = cb_state->session_state;

	if (!switch_test_flag(fh, SWITCH_FILE_OPEN)) {
		return SWITCH_STATUS_FALSE;
	}

	if ((status = js_common_callback(session, input, itype, buf, buflen)) != SWITCH_STATUS_SUCCESS) {
		return status;
	}

	if ((ret = JS_GetStringBytes(JS_ValueToString(cb_state->cx, cb_state->ret)))) {
		if (!strncasecmp(ret, "speed", 5)) {
			char *p;

			if ((p = strchr(ret, ':'))) {
				p++;
				if (*p == '+' || *p == '-') {
					int step;
					if (!(step = atoi(p))) {
						step = 1;
					}
					fh->speed += step;
				} else {
					int speed = atoi(p);
					fh->speed = speed;
				}
				return SWITCH_STATUS_SUCCESS;
			}

			return SWITCH_STATUS_FALSE;
		} else if (!strncasecmp(ret, "volume", 6)) {
			char *p;

			if ((p = strchr(ret, ':'))) {
				p++;
				if (*p == '+' || *p == '-') {
					int step;
					if (!(step = atoi(p))) {
						step = 1;
					}
					fh->vol += step;
				} else {
					int vol = atoi(p);
					fh->vol = vol;
				}
				return SWITCH_STATUS_SUCCESS;
			}

			if (fh->vol) {
				switch_normalize_volume(fh->vol);
			}

			return SWITCH_STATUS_FALSE;
		} else if (!strcasecmp(ret, "pause")) {
			if (switch_test_flag(fh, SWITCH_FILE_PAUSE)) {
				switch_clear_flag(fh, SWITCH_FILE_PAUSE);
			} else {
				switch_set_flag(fh, SWITCH_FILE_PAUSE);
			}
			return SWITCH_STATUS_SUCCESS;
		} else if (!strcasecmp(ret, "truncate")) {
			switch_core_file_truncate(fh, 0);
		} else if (!strcasecmp(ret, "restart")) {
			uint32_t pos = 0;
			fh->speed = 0;
			switch_core_file_seek(fh, &pos, 0, SEEK_SET);
			return SWITCH_STATUS_SUCCESS;
		} else if (!strncasecmp(ret, "seek", 4)) {
			switch_codec_t *codec;
			uint32_t samps = 0;
			uint32_t pos = 0;
			char *p;
			codec = switch_core_session_get_read_codec(jss->session);

			if ((p = strchr(ret, ':'))) {
				p++;
				if (*p == '+' || *p == '-') {
					int step;
					if (!(step = atoi(p))) {
						step = 1000;
					}
					if (step > 0) {
						samps = step * (codec->implementation->actual_samples_per_second / 1000);
						switch_core_file_seek(fh, &pos, samps, SEEK_CUR);
					} else {
						samps = abs(step) * (codec->implementation->actual_samples_per_second / 1000);
						switch_core_file_seek(fh, &pos, fh->pos - samps, SEEK_SET);
					}
				} else {
					samps = atoi(p) * (codec->implementation->actual_samples_per_second / 1000);
					switch_core_file_seek(fh, &pos, samps, SEEK_SET);
				}
			}

			return SWITCH_STATUS_SUCCESS;
		}

		if (!strcmp(ret, "true") || !strcmp(ret, "undefined")) {
			return SWITCH_STATUS_SUCCESS;
		}

		return SWITCH_STATUS_BREAK;

	}
	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t js_record_input_callback(switch_core_session_t *session, void *input, switch_input_type_t itype, void *buf, unsigned int buflen)
{
	char *ret;
	switch_status_t status;
	struct input_callback_state *cb_state = buf;
	switch_file_handle_t *fh = cb_state->extra;

	if ((status = js_common_callback(session, input, itype, buf, buflen)) != SWITCH_STATUS_SUCCESS) {
		return status;
	}

	if ((ret = JS_GetStringBytes(JS_ValueToString(cb_state->cx, cb_state->ret)))) {
		if (!strcasecmp(ret, "pause")) {
			if (switch_test_flag(fh, SWITCH_FILE_PAUSE)) {
				switch_clear_flag(fh, SWITCH_FILE_PAUSE);
			} else {
				switch_set_flag(fh, SWITCH_FILE_PAUSE);
			}
			return SWITCH_STATUS_SUCCESS;
		} else if (!strcasecmp(ret, "restart")) {
			unsigned int pos = 0;
			fh->speed = 0;
			switch_core_file_seek(fh, &pos, 0, SEEK_SET);
			return SWITCH_STATUS_SUCCESS;
		}

		if (!strcmp(ret, "true") || !strcmp(ret, "undefined")) {
			return SWITCH_STATUS_SUCCESS;
		}

		return SWITCH_STATUS_BREAK;

	}

	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t js_collect_input_callback(switch_core_session_t *session, void *input, switch_input_type_t itype, void *buf, unsigned int buflen)
{
	char *ret;
	switch_status_t status;
	struct input_callback_state *cb_state = buf;

	if ((status = js_common_callback(session, input, itype, buf, buflen)) != SWITCH_STATUS_SUCCESS) {
		return status;
	}

	if ((ret = JS_GetStringBytes(JS_ValueToString(cb_state->cx, cb_state->ret)))) {
		if (!strcmp(ret, "true") || !strcmp(ret, "undefined")) {
			return SWITCH_STATUS_SUCCESS;
		}
	}

	return SWITCH_STATUS_BREAK;
}

static JSBool session_flush_digits(JSContext * cx, JSObject * obj, uintN argc, jsval * argv, jsval * rval)
{
	struct js_session *jss = JS_GetPrivate(cx, obj);
	switch_channel_t *channel;

	METHOD_SANITY_CHECK();
	channel = switch_core_session_get_channel(jss->session);
	CHANNEL_MEDIA_SANITY_CHECK();

	switch_channel_flush_dtmf(switch_core_session_get_channel(jss->session));

	*rval = BOOLEAN_TO_JSVAL(JS_TRUE);
	return JS_TRUE;
}

static JSBool session_flush_events(JSContext * cx, JSObject * obj, uintN argc, jsval * argv, jsval * rval)
{
	struct js_session *jss = JS_GetPrivate(cx, obj);
	switch_event_t *event;

	if (!jss || !jss->session) {
		*rval = BOOLEAN_TO_JSVAL(JS_FALSE);
		return JS_TRUE;
	}

	while (switch_core_session_dequeue_event(jss->session, &event, SWITCH_FALSE) == SWITCH_STATUS_SUCCESS) {
		switch_event_destroy(&event);
	}

	*rval = BOOLEAN_TO_JSVAL(JS_TRUE);
	return JS_TRUE;

}

static JSBool session_recordfile(JSContext * cx, JSObject * obj, uintN argc, jsval * argv, jsval * rval)
{
	struct js_session *jss = JS_GetPrivate(cx, obj);
	switch_channel_t *channel;
	char *file_name = NULL;
	void *bp = NULL;
	int len = 0;
	switch_input_callback_function_t dtmf_func = NULL;
	struct input_callback_state cb_state = { 0 };
	switch_file_handle_t fh = { 0 };
	JSFunction *function;
	int32 limit = 0;
	switch_input_args_t args = { 0 };
	jsval ret = JS_TRUE;

	METHOD_SANITY_CHECK();
	channel = switch_core_session_get_channel(jss->session);
	CHANNEL_SANITY_CHECK();

	CHANNEL_MEDIA_SANITY_CHECK();

	if (argc > 0) {
		file_name = JS_GetStringBytes(JS_ValueToString(cx, argv[0]));
		if (zstr(file_name)) {
			return JS_FALSE;
		}
	}
	if (argc > 1) {
		if ((function = JS_ValueToFunction(cx, argv[1]))) {
			memset(&cb_state, 0, sizeof(cb_state));
			cb_state.session_state = jss;
			cb_state.function = function;
			cb_state.cx = cx;
			cb_state.obj = obj;
			if (argc > 2) {
				cb_state.arg = argv[2];
			}

			dtmf_func = js_record_input_callback;
			bp = &cb_state;
			len = sizeof(cb_state);
		}

		if (argc > 3) {
			JS_ValueToInt32(cx, argv[3], &limit);
		}

		if (argc > 4) {
			int32 thresh;
			JS_ValueToInt32(cx, argv[4], &thresh);
			fh.thresh = thresh;
		}

		if (argc > 5) {
			int32 silence_hits;
			JS_ValueToInt32(cx, argv[5], &silence_hits);
			fh.silence_hits = silence_hits;
		}
	}

	cb_state.extra = &fh;
	cb_state.ret = BOOLEAN_TO_JSVAL(JS_FALSE);
	cb_state.saveDepth = JS_SuspendRequest(cx);
	args.input_callback = dtmf_func;
	args.buf = bp;
	args.buflen = len;

	switch_ivr_record_file(jss->session, &fh, file_name, &args, limit);
	JS_ResumeRequest(cx, cb_state.saveDepth);
	check_hangup_hook(jss, &ret);
	*rval = cb_state.ret;

	return ret;
}

static JSBool session_collect_input(JSContext * cx, JSObject * obj, uintN argc, jsval * argv, jsval * rval)
{
	struct js_session *jss = JS_GetPrivate(cx, obj);
	switch_channel_t *channel;
	void *bp = NULL;
	int len = 0;
	int32 abs_timeout = 0;
	int32 digit_timeout = 0;

	switch_input_callback_function_t dtmf_func = NULL;
	struct input_callback_state cb_state = { 0 };
	JSFunction *function;
	switch_input_args_t args = { 0 };
	jsval ret = JS_TRUE;


	METHOD_SANITY_CHECK();
	channel = switch_core_session_get_channel(jss->session);
	CHANNEL_SANITY_CHECK();
	CHANNEL_MEDIA_SANITY_CHECK();

	if (argc > 0) {
		if ((function = JS_ValueToFunction(cx, argv[0]))) {
			memset(&cb_state, 0, sizeof(cb_state));
			cb_state.function = function;

			if (argc > 1) {
				cb_state.arg = argv[1];
			}

			cb_state.session_state = jss;
			cb_state.cx = cx;
			cb_state.obj = obj;
			dtmf_func = js_collect_input_callback;
			bp = &cb_state;
			len = sizeof(cb_state);
		}
	}

	if (argc == 3) {
		JS_ValueToInt32(jss->cx, argv[2], &abs_timeout);
	} else if (argc > 3) {
		JS_ValueToInt32(jss->cx, argv[2], &digit_timeout);
		JS_ValueToInt32(jss->cx, argv[3], &abs_timeout);
	}

	cb_state.saveDepth = JS_SuspendRequest(cx);
	args.input_callback = dtmf_func;
	args.buf = bp;
	args.buflen = len;

	switch_ivr_collect_digits_callback(jss->session, &args, digit_timeout, abs_timeout);
	JS_ResumeRequest(cx, cb_state.saveDepth);
	check_hangup_hook(jss, &ret);
	*rval = cb_state.ret;

	return ret;
}

/* session.sayphrase(phrase_name, phrase_data, language, dtmf_callback, dtmf_callback_args)*/

static JSBool session_sayphrase(JSContext * cx, JSObject * obj, uintN argc, jsval * argv, jsval * rval)
{
	struct js_session *jss = JS_GetPrivate(cx, obj);
	switch_channel_t *channel;
	char *phrase_name = NULL;
	char *phrase_data = NULL;
	char *phrase_lang = NULL;
	char *tmp = NULL;
	//char *input_callback = NULL;
	void *bp = NULL;
	int len = 0;
	switch_input_callback_function_t dtmf_func = NULL;
	struct input_callback_state cb_state = { 0 };
	JSFunction *function;
	switch_input_args_t args = { 0 };
	jsval ret = JS_TRUE;

	METHOD_SANITY_CHECK();
	channel = switch_core_session_get_channel(jss->session);
	CHANNEL_SANITY_CHECK();
	CHANNEL_MEDIA_SANITY_CHECK();

	if (argc > 0) {
		phrase_name = JS_GetStringBytes(JS_ValueToString(cx, argv[0]));
		if (zstr(phrase_name)) {
			return JS_FALSE;
		}
	} else {
		return JS_FALSE;
	}

	if (argc > 1) {
		phrase_data = JS_GetStringBytes(JS_ValueToString(cx, argv[1]));
	}

	if (argc > 2) {
		tmp = JS_GetStringBytes(JS_ValueToString(cx, argv[2]));
		if (!zstr(tmp)) {
			phrase_lang = tmp;
		}
	}

	if (argc > 3) {
		if ((function = JS_ValueToFunction(cx, argv[3]))) {
			memset(&cb_state, 0, sizeof(cb_state));
			cb_state.function = function;

			if (argc > 4) {
				cb_state.arg = argv[4];
			}

			cb_state.session_state = jss;
			cb_state.cx = cx;
			cb_state.obj = obj;
			dtmf_func = js_collect_input_callback;
			bp = &cb_state;
			len = sizeof(cb_state);
		}
	}

	cb_state.ret = BOOLEAN_TO_JSVAL(JS_FALSE);
	cb_state.saveDepth = JS_SuspendRequest(cx);
	args.input_callback = dtmf_func;
	args.buf = bp;
	args.buflen = len;

	switch_ivr_phrase_macro(jss->session, phrase_name, phrase_data, phrase_lang, &args);
	JS_ResumeRequest(cx, cb_state.saveDepth);
	check_hangup_hook(jss, &ret);
	*rval = cb_state.ret;

	return ret;
}

static jsval check_hangup_hook(struct js_session *jss, jsval * rp)
{
	jsval argv[3] = { 0 };
	int argc = 0;
	jsval ret = JS_TRUE;
	char *resp;

	if (!jss->check_state && jss->on_hangup && (jss->hook_state == CS_HANGUP || jss->hook_state == CS_ROUTING)) {
		jss->check_state++;
		argv[argc++] = OBJECT_TO_JSVAL(jss->obj);
		if (jss->hook_state == CS_HANGUP) {
			argv[argc++] = STRING_TO_JSVAL(JS_NewStringCopyZ(jss->cx, "hangup"));
		} else {
			argv[argc++] = STRING_TO_JSVAL(JS_NewStringCopyZ(jss->cx, "transfer"));
		}
		JS_CallFunction(jss->cx, jss->obj, jss->on_hangup, argc, argv, &ret);
		resp = JS_GetStringBytes(JS_ValueToString(jss->cx, ret));
		if (!zstr(resp)) {
			ret = !strcasecmp(resp, "exit") ? JS_FALSE : JS_TRUE;
		}
	}

	if (rp) {
		*rp = ret;
	}

	return ret;
}

static switch_status_t hanguphook(switch_core_session_t *session)
{
	switch_channel_t *channel = switch_core_session_get_channel(session);
	struct js_session *jss = NULL;

	if ((jss = switch_channel_get_private(channel, "jss"))) {
		switch_channel_state_t state = switch_channel_get_state(channel);
		if (jss->hook_state != state) {
			jss->hook_state = state;
			jss->check_state = 0;
		}
	}

	return SWITCH_STATUS_SUCCESS;
}

static JSBool session_hanguphook(JSContext * cx, JSObject * obj, uintN argc, jsval * argv, jsval * rval)
{
	JSFunction *function;
	struct js_session *jss;
	*rval = BOOLEAN_TO_JSVAL(JS_FALSE);

	if ((jss = JS_GetPrivate(cx, obj)) && jss->session) {
		if (argc > 0) {
			if ((function = JS_ValueToFunction(cx, argv[0]))) {
				switch_channel_t *channel = switch_core_session_get_channel(jss->session);
				jss->on_hangup = function;
				jss->hook_state = switch_channel_get_state(channel);
				switch_channel_set_private(channel, "jss", jss);
				switch_core_event_hook_add_state_change(jss->session, hanguphook);
				*rval = BOOLEAN_TO_JSVAL(JS_FALSE);
			}
		}
	}

	return JS_TRUE;
}

static JSBool session_streamfile(JSContext * cx, JSObject * obj, uintN argc, jsval * argv, jsval * rval)
{
	struct js_session *jss = JS_GetPrivate(cx, obj);
	switch_channel_t *channel;
	char *file_name = NULL;
	//char *input_callback = NULL;
	void *bp = NULL;
	int len = 0;
	switch_input_callback_function_t dtmf_func = NULL;
	struct input_callback_state cb_state = { 0 };
	switch_file_handle_t fh = { 0 };
	JSFunction *function;
	switch_input_args_t args = { 0 };
	const char *prebuf;
	char posbuf[35] = "";
	jsval ret = JS_TRUE;

	METHOD_SANITY_CHECK();
	channel = switch_core_session_get_channel(jss->session);
	CHANNEL_SANITY_CHECK();
	CHANNEL_MEDIA_SANITY_CHECK();

	if (argc > 0) {
		file_name = JS_GetStringBytes(JS_ValueToString(cx, argv[0]));
		if (zstr(file_name)) {
			return JS_FALSE;
		}
	}

	if (argc > 1) {
		if ((function = JS_ValueToFunction(cx, argv[1]))) {
			memset(&cb_state, 0, sizeof(cb_state));
			cb_state.function = function;

			if (argc > 2) {
				cb_state.arg = argv[2];
			}

			cb_state.session_state = jss;
			cb_state.cx = cx;
			cb_state.obj = obj;
			dtmf_func = js_stream_input_callback;
			bp = &cb_state;
			len = sizeof(cb_state);
		}
	}

	if (argc > 3) {
		int32 samps;
		JS_ValueToInt32(cx, argv[3], &samps);
		fh.samples = samps;
	}

	if ((prebuf = switch_channel_get_variable(channel, "stream_prebuffer"))) {
		int maybe = atoi(prebuf);
		if (maybe > 0) {
			fh.prebuf = maybe;
		}
	}

	cb_state.extra = &fh;
	cb_state.ret = BOOLEAN_TO_JSVAL(JS_FALSE);
	cb_state.saveDepth = JS_SuspendRequest(cx);
	args.input_callback = dtmf_func;
	args.buf = bp;
	args.buflen = len;
	switch_ivr_play_file(jss->session, &fh, file_name, &args);
	JS_ResumeRequest(cx, cb_state.saveDepth);
	check_hangup_hook(jss, &ret);
	*rval = cb_state.ret;

	switch_snprintf(posbuf, sizeof(posbuf), "%u", fh.offset_pos);
	switch_channel_set_variable(channel, "last_file_position", posbuf);

	return ret;
}



static JSBool session_sleep(JSContext * cx, JSObject * obj, uintN argc, jsval * argv, jsval * rval)
{
	struct js_session *jss = JS_GetPrivate(cx, obj);
	switch_channel_t *channel;
	void *bp = NULL;
	int len = 0;
	switch_input_callback_function_t dtmf_func = NULL;
	struct input_callback_state cb_state = { 0 };
	JSFunction *function;
	switch_input_args_t args = { 0 };
	int32 ms = 0;
	jsval ret = JS_TRUE;
	int32 sync = 0;

	METHOD_SANITY_CHECK();
	channel = switch_core_session_get_channel(jss->session);
	CHANNEL_SANITY_CHECK();
	CHANNEL_MEDIA_SANITY_CHECK();

	if (argc > 0) {
		JS_ValueToInt32(cx, argv[0], &ms);
	}

	if (ms <= 0) {
		return JS_FALSE;
	}

	if (argc > 1) {
		if ((function = JS_ValueToFunction(cx, argv[1]))) {
			memset(&cb_state, 0, sizeof(cb_state));
			cb_state.function = function;

			if (argc > 2) {
				cb_state.arg = argv[2];
			}

			cb_state.session_state = jss;
			cb_state.cx = cx;
			cb_state.obj = obj;
			dtmf_func = js_stream_input_callback;
			bp = &cb_state;
			len = sizeof(cb_state);
		}
	}

	if (argc > 2) {
		JS_ValueToInt32(cx, argv[2], &sync);
	}

	cb_state.ret = BOOLEAN_TO_JSVAL(JS_FALSE);
	cb_state.saveDepth = JS_SuspendRequest(cx);
	args.input_callback = dtmf_func;
	args.buf = bp;
	args.buflen = len;
	switch_ivr_sleep(jss->session, ms, sync, &args);
	JS_ResumeRequest(cx, cb_state.saveDepth);
	check_hangup_hook(jss, &ret);
	*rval = cb_state.ret;

	return ret;
}

static JSBool session_set_variable(JSContext * cx, JSObject * obj, uintN argc, jsval * argv, jsval * rval)
{
	struct js_session *jss = JS_GetPrivate(cx, obj);
	switch_channel_t *channel;

	METHOD_SANITY_CHECK();

	channel = switch_core_session_get_channel(jss->session);

	if (argc > 1) {
		char *var, *val;

		var = JS_GetStringBytes(JS_ValueToString(cx, argv[0]));
		val = JS_GetStringBytes(JS_ValueToString(cx, argv[1]));
		switch_channel_set_variable(channel, var, val);
		*rval = BOOLEAN_TO_JSVAL(JS_TRUE);
	} else {
		*rval = BOOLEAN_TO_JSVAL(JS_FALSE);
	}

	return JS_TRUE;
}

static JSBool session_get_variable(JSContext * cx, JSObject * obj, uintN argc, jsval * argv, jsval * rval)
{
	struct js_session *jss = JS_GetPrivate(cx, obj);
	switch_channel_t *channel;

	METHOD_SANITY_CHECK();

	channel = switch_core_session_get_channel(jss->session);

	if (argc > 0) {
		const char *var, *val;

		var = JS_GetStringBytes(JS_ValueToString(cx, argv[0]));
		val = switch_channel_get_variable(channel, var);

		if (val) {
			*rval = STRING_TO_JSVAL(JS_NewStringCopyZ(cx, val));
		} else {
			*rval = STRING_TO_JSVAL(JS_NewStringCopyZ(cx, ""));
		}
	} else {
		*rval = BOOLEAN_TO_JSVAL(JS_FALSE);
	}

	return JS_TRUE;
}

static void destroy_speech_engine(struct js_session *jss)
{
	if (jss->speech) {
		switch_speech_flag_t flags = SWITCH_SPEECH_FLAG_NONE;
		switch_core_codec_destroy(&jss->speech->codec);
		switch_core_speech_close(&jss->speech->sh, &flags);
		jss->speech = NULL;
	}
}

static switch_status_t init_speech_engine(struct js_session *jss, char *engine, char *voice)
{
	switch_codec_t *read_codec;
	switch_speech_flag_t flags = SWITCH_SPEECH_FLAG_NONE;
	uint32_t rate = 0;
	int interval = 0;

	read_codec = switch_core_session_get_read_codec(jss->session);
	rate = read_codec->implementation->actual_samples_per_second;
	interval = read_codec->implementation->microseconds_per_packet / 1000;

	if (switch_core_codec_init(&jss->speech->codec,
							   "L16",
							   NULL,
							   rate,
							   interval,
							   1, SWITCH_CODEC_FLAG_ENCODE | SWITCH_CODEC_FLAG_DECODE, NULL,
							   switch_core_session_get_pool(jss->session)) == SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Raw Codec Activation Success L16@%uhz 1 channel %dms\n", rate, interval);
	} else {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Raw Codec Activation Failed L16@%uhz 1 channel %dms\n", rate, interval);
		return SWITCH_STATUS_FALSE;
	}

	if (switch_core_speech_open(&jss->speech->sh, engine, voice, rate, interval,
								&flags, switch_core_session_get_pool(jss->session)) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Invalid TTS module!\n");
		switch_core_codec_destroy(&jss->speech->codec);
		return SWITCH_STATUS_FALSE;
	}

	return SWITCH_STATUS_SUCCESS;

}

static JSBool session_speak(JSContext * cx, JSObject * obj, uintN argc, jsval * argv, jsval * rval)
{
	struct js_session *jss = JS_GetPrivate(cx, obj);
	switch_channel_t *channel;
	char *tts_name = NULL;
	char *voice_name = NULL;
	char *text = NULL;
	switch_codec_t *codec;
	void *bp = NULL;
	int len = 0;
	struct input_callback_state cb_state = { 0 };
	switch_input_callback_function_t dtmf_func = NULL;
	JSFunction *function;
	switch_input_args_t args = { 0 };
	jsval ret = JS_TRUE;

	METHOD_SANITY_CHECK();


	*rval = BOOLEAN_TO_JSVAL(JS_FALSE);
	channel = switch_core_session_get_channel(jss->session);
	CHANNEL_SANITY_CHECK();
	CHANNEL_MEDIA_SANITY_CHECK();

	if (argc < 3) {
		return JS_FALSE;
	}

	tts_name = JS_GetStringBytes(JS_ValueToString(cx, argv[0]));
	voice_name = JS_GetStringBytes(JS_ValueToString(cx, argv[1]));
	text = JS_GetStringBytes(JS_ValueToString(cx, argv[2]));

	if (zstr(tts_name)) {
		eval_some_js("~throw new Error(\"Invalid TTS Name\");", cx, obj, rval);
		return JS_FALSE;
	}

	if (zstr(text)) {
		eval_some_js("~throw new Error(\"Invalid Text\");", cx, obj, rval);
		return JS_FALSE;
	}

	if (jss->speech && strcasecmp(jss->speech->sh.name, tts_name)) {
		destroy_speech_engine(jss);
	}

	if (jss->speech) {
		switch_core_speech_text_param_tts(&jss->speech->sh, "voice", voice_name);
	} else {
		jss->speech = switch_core_session_alloc(jss->session, sizeof(*jss->speech));
		switch_assert(jss->speech != NULL);
		if (init_speech_engine(jss, tts_name, voice_name) != SWITCH_STATUS_SUCCESS) {
			eval_some_js("~throw new Error(\"Cannot allocate speech engine!\");", cx, obj, rval);
			jss->speech = NULL;
			return JS_FALSE;
		}
	}

	if (argc > 3) {
		if ((function = JS_ValueToFunction(cx, argv[3]))) {
			memset(&cb_state, 0, sizeof(cb_state));
			cb_state.function = function;
			if (argc > 4) {
				cb_state.arg = argv[4];
			}

			cb_state.cx = cx;
			cb_state.obj = obj;
			cb_state.session_state = jss;
			dtmf_func = js_collect_input_callback;
			bp = &cb_state;
			len = sizeof(cb_state);
		}
	}

	codec = switch_core_session_get_read_codec(jss->session);
	cb_state.ret = BOOLEAN_TO_JSVAL(JS_FALSE);
	cb_state.saveDepth = JS_SuspendRequest(cx);
	args.input_callback = dtmf_func;
	args.buf = bp;
	args.buflen = len;

	switch_core_speech_flush_tts(&jss->speech->sh);
	switch_ivr_speak_text_handle(jss->session, &jss->speech->sh, &jss->speech->codec, NULL, text, &args);
	JS_ResumeRequest(cx, cb_state.saveDepth);
	check_hangup_hook(jss, &ret);
	*rval = cb_state.ret;

	return ret;
}

static JSBool session_get_digits(JSContext * cx, JSObject * obj, uintN argc, jsval * argv, jsval * rval)
{
	struct js_session *jss = JS_GetPrivate(cx, obj);
	char *terminators = NULL;
	char buf[513] = { 0 };
	int32 digits = 0, timeout = 5000, digit_timeout = 0, abs_timeout = 0;
	switch_channel_t *channel;

	METHOD_SANITY_CHECK();
	channel = switch_core_session_get_channel(jss->session);
	CHANNEL_SANITY_CHECK();

	if (argc > 0) {
		char term;
		JS_ValueToInt32(cx, argv[0], &digits);

		if (digits > sizeof(buf) - 1) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Exceeded max digits of %" SWITCH_SIZE_T_FMT "\n", sizeof(buf) - 1);
			return JS_FALSE;
		}

		if (argc > 1) {
			terminators = JS_GetStringBytes(JS_ValueToString(cx, argv[1]));
		}

		if (argc > 2) {
			JS_ValueToInt32(cx, argv[2], &timeout);
		}

		if (argc > 3) {
			JS_ValueToInt32(cx, argv[3], &digit_timeout);
		}

		if (argc > 4) {
			JS_ValueToInt32(cx, argv[4], &abs_timeout);
		}

		switch_ivr_collect_digits_count(jss->session, buf, sizeof(buf), digits, terminators, &term, timeout, digit_timeout, abs_timeout);
		*rval = STRING_TO_JSVAL(JS_NewStringCopyZ(cx, buf));
		return JS_TRUE;
	}

	return JS_FALSE;
}

static JSBool session_autohangup(JSContext * cx, JSObject * obj, uintN argc, jsval * argv, jsval * rval)
{
	struct js_session *jss = JS_GetPrivate(cx, obj);
	*rval = BOOLEAN_TO_JSVAL(JS_FALSE);

	METHOD_SANITY_CHECK();

	if (argv[0]) {
		JSBool tf;
		JS_ValueToBoolean(cx, argv[0], &tf);
		if (tf == JS_TRUE) {
			switch_set_flag(jss, S_HUP);
		} else {
			switch_clear_flag(jss, S_HUP);
		}
		*rval = BOOLEAN_TO_JSVAL(tf);
	}

	return JS_TRUE;
}

static JSBool session_answer(JSContext * cx, JSObject * obj, uintN argc, jsval * argv, jsval * rval)
{
	struct js_session *jss = JS_GetPrivate(cx, obj);
	switch_channel_t *channel;

	METHOD_SANITY_CHECK();
	channel = switch_core_session_get_channel(jss->session);
	CHANNEL_SANITY_CHECK_ANSWER();

	switch_channel_answer(channel);
	return JS_TRUE;
}

static JSBool session_pre_answer(JSContext * cx, JSObject * obj, uintN argc, jsval * argv, jsval * rval)
{
	struct js_session *jss = JS_GetPrivate(cx, obj);
	switch_channel_t *channel;

	METHOD_SANITY_CHECK();
	channel = switch_core_session_get_channel(jss->session);
	CHANNEL_SANITY_CHECK_ANSWER();

	switch_channel_pre_answer(channel);
	return JS_TRUE;
}

static JSBool session_cdr(JSContext * cx, JSObject * obj, uintN argc, jsval * argv, jsval * rval)
{
	struct js_session *jss = JS_GetPrivate(cx, obj);
	switch_xml_t cdr;

	/*Always a pessimist... sheesh! */
	*rval = BOOLEAN_TO_JSVAL(JS_FALSE);

	if (switch_ivr_generate_xml_cdr(jss->session, &cdr) == SWITCH_STATUS_SUCCESS) {
		char *xml_text;
		if ((xml_text = switch_xml_toxml(cdr, SWITCH_FALSE))) {
			*rval = STRING_TO_JSVAL(JS_NewStringCopyZ(cx, xml_text));
		}
		switch_safe_free(xml_text);
		switch_xml_free(cdr);
	}

	return JS_TRUE;
}

static JSBool session_ready(JSContext * cx, JSObject * obj, uintN argc, jsval * argv, jsval * rval)
{
	struct js_session *jss = JS_GetPrivate(cx, obj);

	*rval = BOOLEAN_TO_JSVAL((jss && jss->session && switch_channel_ready(switch_core_session_get_channel(jss->session))) ? JS_TRUE : JS_FALSE);

	return JS_TRUE;
}

static JSBool session_media_ready(JSContext * cx, JSObject * obj, uintN argc, jsval * argv, jsval * rval)
{
	struct js_session *jss = JS_GetPrivate(cx, obj);

	*rval = BOOLEAN_TO_JSVAL((jss && jss->session && switch_channel_media_ready(switch_core_session_get_channel(jss->session))) ? JS_TRUE : JS_FALSE);

	return JS_TRUE;
}


static JSBool session_answered(JSContext * cx, JSObject * obj, uintN argc, jsval * argv, jsval * rval)
{
	struct js_session *jss = JS_GetPrivate(cx, obj);

	*rval =
		BOOLEAN_TO_JSVAL((jss && jss->session
						  && switch_channel_test_flag(switch_core_session_get_channel(jss->session), CF_ANSWERED)) ? JS_TRUE : JS_FALSE);

	return JS_TRUE;
}

static JSBool session_wait_for_media(JSContext * cx, JSObject * obj, uintN argc, jsval * argv, jsval * rval)
{
	struct js_session *jss = JS_GetPrivate(cx, obj);
	switch_channel_t *channel;
	switch_time_t started;
	unsigned int elapsed;
	int32 timeout = 60000;
	jsrefcount saveDepth;
	jsval ret = JS_TRUE;

	METHOD_SANITY_CHECK();

	channel = switch_core_session_get_channel(jss->session);
	CHANNEL_MEDIA_SANITY_CHECK();

	started = switch_micro_time_now();

	if (argc > 0) {
		JS_ValueToInt32(cx, argv[0], &timeout);
		if (timeout < 1000) {
			timeout = 1000;
		}
	}

	if (check_hangup_hook(jss, NULL) != JS_TRUE) {
		return JS_FALSE;
	}
	saveDepth = JS_SuspendRequest(cx);
	for (;;) {
		if (((elapsed = (unsigned int) ((switch_micro_time_now() - started) / 1000)) > (switch_time_t) timeout)
			|| switch_channel_down(channel)) {
			*rval = BOOLEAN_TO_JSVAL(JS_FALSE);
			break;
		}

		if (switch_channel_ready(channel)
			&& (switch_channel_test_flag(channel, CF_ANSWERED) || switch_channel_test_flag(channel, CF_EARLY_MEDIA))) {
			*rval = BOOLEAN_TO_JSVAL(JS_TRUE);
			break;
		}

		switch_cond_next();
	}
	JS_ResumeRequest(cx, saveDepth);
	check_hangup_hook(jss, &ret);

	return ret;
}

static JSBool session_wait_for_answer(JSContext * cx, JSObject * obj, uintN argc, jsval * argv, jsval * rval)
{
	struct js_session *jss = JS_GetPrivate(cx, obj);
	switch_channel_t *channel;
	switch_time_t started;
	unsigned int elapsed;
	int32 timeout = 60000;
	jsrefcount saveDepth;
	jsval ret = JS_TRUE;

	METHOD_SANITY_CHECK();
	channel = switch_core_session_get_channel(jss->session);

	started = switch_micro_time_now();

	if (argc > 0) {
		JS_ValueToInt32(cx, argv[0], &timeout);
		if (timeout < 1000) {
			timeout = 1000;
		}
	}

	if (check_hangup_hook(jss, NULL) != JS_TRUE) {
		return JS_FALSE;
	}

	saveDepth = JS_SuspendRequest(cx);
	for (;;) {
		if (((elapsed = (unsigned int) ((switch_micro_time_now() - started) / 1000)) > (switch_time_t) timeout)
			|| switch_channel_down(channel)) {
			*rval = BOOLEAN_TO_JSVAL(JS_FALSE);
			break;
		}

		if (switch_channel_ready(channel) && switch_channel_test_flag(channel, CF_ANSWERED)) {
			*rval = BOOLEAN_TO_JSVAL(JS_TRUE);
			break;
		}

		switch_cond_next();
	}
	JS_ResumeRequest(cx, saveDepth);
	check_hangup_hook(jss, &ret);
	return ret;
}

static JSBool session_detach(JSContext * cx, JSObject * obj, uintN argc, jsval * argv, jsval * rval)
{
	struct js_session *jss = JS_GetPrivate(cx, obj);
	jsval ret = JS_TRUE;
	switch_call_cause_t cause = 0;
	switch_channel_t *channel;
	switch_core_session_t *session;
	METHOD_SANITY_CHECK();

	if ((session = jss->session)) {
		jss->session = NULL;

		if (argc > 1) {
			if (JSVAL_IS_INT(argv[0])) {
				int32 i = 0;
				JS_ValueToInt32(cx, argv[0], &i);
				cause = i;
			} else {
				const char *cause_name = JS_GetStringBytes(JS_ValueToString(cx, argv[0]));
				cause = switch_channel_str2cause(cause_name);
			}
		}

		if (cause) {
			channel = switch_core_session_get_channel(session);
			switch_channel_hangup(channel, cause);
		}

		switch_core_session_rwunlock(session);
		*rval = JS_TRUE;
	} else {
		*rval = JS_FALSE;
	}

	return ret;
}

static JSBool session_execute(JSContext * cx, JSObject * obj, uintN argc, jsval * argv, jsval * rval)
{
	JSBool retval = JS_FALSE;
	switch_channel_t *channel;
	struct js_session *jss = JS_GetPrivate(cx, obj);
	jsval ret = JS_TRUE;

	METHOD_SANITY_CHECK();

	channel = switch_core_session_get_channel(jss->session);
	/* you can execute some apps before you answer  CHANNEL_SANITY_CHECK(); */

	if (argc > 0) {
		switch_application_interface_t *application_interface;
		char *app_name = JS_GetStringBytes(JS_ValueToString(cx, argv[0]));
		char *app_arg = NULL;
		jsrefcount saveDepth;

		METHOD_SANITY_CHECK();

		if (argc > 1) {
			app_arg = JS_GetStringBytes(JS_ValueToString(cx, argv[1]));
		}

		if ((application_interface = switch_loadable_module_get_application_interface(app_name))) {
			if (application_interface->application_function) {
				if (check_hangup_hook(jss, NULL) != JS_TRUE) {
					return JS_FALSE;
				}

				saveDepth = JS_SuspendRequest(cx);
				switch_core_session_exec(jss->session, application_interface, app_arg);
				JS_ResumeRequest(cx, saveDepth);
				check_hangup_hook(jss, &ret);
				retval = JS_TRUE;
			}
			UNPROTECT_INTERFACE(application_interface);
		}
	}

	*rval = BOOLEAN_TO_JSVAL(retval);
	return ret;
}

static JSBool session_get_event(JSContext * cx, JSObject * obj, uintN argc, jsval * argv, jsval * rval)
{
	struct js_session *jss = JS_GetPrivate(cx, obj);
	switch_event_t *event;

	METHOD_SANITY_CHECK();

	if (switch_core_session_dequeue_event(jss->session, &event, SWITCH_FALSE) == SWITCH_STATUS_SUCCESS) {
		JSObject *Event;
		struct event_obj *eo;

		if ((eo = malloc(sizeof(*eo)))) {
			eo->event = event;
			eo->freed = 0;

			if ((Event = JS_DefineObject(cx, obj, "__event__", &event_class, NULL, 0))) {
				if ((JS_SetPrivate(cx, Event, eo) && JS_DefineProperties(cx, Event, event_props) && JS_DefineFunctions(cx, Event, event_methods))) {
					*rval = OBJECT_TO_JSVAL(Event);
					return JS_TRUE;
				}
			}
		}
	}

	*rval = BOOLEAN_TO_JSVAL(JS_FALSE);
	return JS_TRUE;
}

static JSBool session_send_event(JSContext * cx, JSObject * obj, uintN argc, jsval * argv, jsval * rval)
{
	struct js_session *jss = JS_GetPrivate(cx, obj);
	JSObject *Event;
	struct event_obj *eo;

	METHOD_SANITY_CHECK();

	if (argc > 0) {
		if (JS_ValueToObject(cx, argv[0], &Event)) {
			if ((eo = JS_GetPrivate(cx, Event))) {
				if (switch_core_session_receive_event(jss->session, &eo->event) != SWITCH_STATUS_SUCCESS) {
					*rval = BOOLEAN_TO_JSVAL(JS_FALSE);
					return JS_TRUE;
				}

				JS_SetPrivate(cx, Event, NULL);
			}
		}
	}

	*rval = BOOLEAN_TO_JSVAL(JS_TRUE);
	return JS_TRUE;
}

static JSBool session_hangup(JSContext * cx, JSObject * obj, uintN argc, jsval * argv, jsval * rval)
{
	struct js_session *jss = JS_GetPrivate(cx, obj);
	switch_channel_t *channel;
	char *cause_name = NULL;
	switch_call_cause_t cause = SWITCH_CAUSE_NORMAL_CLEARING;

	METHOD_SANITY_CHECK();
	channel = switch_core_session_get_channel(jss->session);
	CHANNEL_SANITY_CHECK();

	if (argc > 1) {
		if (JSVAL_IS_INT(argv[0])) {
			int32 i = 0;
			JS_ValueToInt32(cx, argv[0], &i);
			cause = i;
		} else {
			cause_name = JS_GetStringBytes(JS_ValueToString(cx, argv[0]));
			cause = switch_channel_str2cause(cause_name);
		}
	}

	switch_channel_hangup(channel, cause);
	switch_core_session_kill_channel(jss->session, SWITCH_SIG_KILL);
	return JS_TRUE;
}

#ifdef HAVE_CURL

struct config_data {
	JSContext *cx;
	JSObject *obj;
	char *name;
	int fd;
};

struct fetch_url_data {
	JSContext *cx;
	JSObject *obj;
	switch_size_t buffer_size;
	switch_size_t data_len;
	char *buffer;
};

static size_t hash_callback(void *ptr, size_t size, size_t nmemb, void *data)
{
	register size_t realsize = size * nmemb;
	char *line, lineb[2048], *nextline = NULL, *val = NULL, *p = NULL;
	jsval rval;
	struct config_data *config_data = data;
	char code[256];

	if (config_data->name) {
		switch_copy_string(lineb, (char *) ptr, sizeof(lineb));
		line = lineb;
		while (line) {
			if ((nextline = strchr(line, '\n'))) {
				*nextline = '\0';
				nextline++;
			}

			if ((val = strchr(line, '='))) {
				*val = '\0';
				val++;
				if (val[0] == '>') {
					*val = '\0';
					val++;
				}

				for (p = line; p && *p == ' '; p++);
				line = p;
				for (p = line + strlen(line) - 1; *p == ' '; p--)
					*p = '\0';
				for (p = val; p && *p == ' '; p++);
				val = p;
				for (p = val + strlen(val) - 1; *p == ' '; p--)
					*p = '\0';

				switch_snprintf(code, sizeof(code), "~%s[\"%s\"] = \"%s\"", config_data->name, line, val);
				eval_some_js(code, config_data->cx, config_data->obj, &rval);

			}

			line = nextline;
		}
	}
	return realsize;
}

static size_t file_callback(void *ptr, size_t size, size_t nmemb, void *data)
{
	register unsigned int realsize = (unsigned int) (size * nmemb);
	struct config_data *config_data = data;

	if ((write(config_data->fd, ptr, realsize) != (int) realsize)) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Unable to write all bytes!\n");
	}
	return realsize;
}

static size_t fetch_url_callback(void *ptr, size_t size, size_t nmemb, void *data)
{
	register unsigned int realsize = (unsigned int) (size * nmemb);
	struct fetch_url_data *config_data = data;

	/* Too much data. Do not increase buffer, but abort fetch instead. */
	if (config_data->data_len + realsize >= config_data->buffer_size) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Data do not fit in the allocated buffer\n");
		return 0;
	}

	memcpy(config_data->buffer + config_data->data_len, ptr, realsize);
	config_data->data_len += realsize;
	config_data->buffer[config_data->data_len] = 0;

	return realsize;
}


static JSBool js_fetchurl_hash(JSContext * cx, JSObject * obj, uintN argc, jsval * argv, jsval * rval)
{
	char *url = NULL, *name = NULL;
	CURL *curl_handle = NULL;
	struct config_data config_data;
	int saveDepth = 0;

	if (argc > 1) {
		url = JS_GetStringBytes(JS_ValueToString(cx, argv[0]));
		name = JS_GetStringBytes(JS_ValueToString(cx, argv[1]));

		curl_handle = curl_easy_init();
		if (!strncasecmp(url, "https", 5)) {
			curl_easy_setopt(curl_handle, CURLOPT_SSL_VERIFYPEER, 0);
			curl_easy_setopt(curl_handle, CURLOPT_SSL_VERIFYHOST, 0);
		}
		config_data.cx = cx;
		config_data.obj = obj;
		if (name) {
			config_data.name = name;
		}
		curl_easy_setopt(curl_handle, CURLOPT_URL, url);
		curl_easy_setopt(curl_handle, CURLOPT_FOLLOWLOCATION, 1);
		curl_easy_setopt(curl_handle, CURLOPT_MAXREDIRS, 10);
		curl_easy_setopt(curl_handle, CURLOPT_WRITEFUNCTION, hash_callback);
		curl_easy_setopt(curl_handle, CURLOPT_WRITEDATA, (void *) &config_data);
		curl_easy_setopt(curl_handle, CURLOPT_USERAGENT, "freeswitch-js/1.0");

		saveDepth = JS_SuspendRequest(cx);
		curl_easy_perform(curl_handle);
		JS_ResumeRequest(cx, saveDepth);

		curl_easy_cleanup(curl_handle);
	} else {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Error!\n");
		return JS_FALSE;
	}

	return JS_TRUE;
}



static JSBool js_fetchurl_file(JSContext * cx, JSObject * obj, uintN argc, jsval * argv, jsval * rval)
{
	char *url = NULL, *filename = NULL;
	CURL *curl_handle = NULL;
	struct config_data config_data;
	int saveDepth = 0;

	if (argc > 1) {
		url = JS_GetStringBytes(JS_ValueToString(cx, argv[0]));
		filename = JS_GetStringBytes(JS_ValueToString(cx, argv[1]));

		curl_global_init(CURL_GLOBAL_ALL);
		curl_handle = curl_easy_init();
		if (!strncasecmp(url, "https", 5)) {
			curl_easy_setopt(curl_handle, CURLOPT_SSL_VERIFYPEER, 0);
			curl_easy_setopt(curl_handle, CURLOPT_SSL_VERIFYHOST, 0);
		}
		config_data.cx = cx;
		config_data.obj = obj;

		config_data.name = filename;
		if ((config_data.fd = open(filename, O_CREAT | O_RDWR | O_TRUNC, S_IRUSR | S_IWUSR)) > -1) {
			curl_easy_setopt(curl_handle, CURLOPT_URL, url);
			curl_easy_setopt(curl_handle, CURLOPT_FOLLOWLOCATION, 1);
			curl_easy_setopt(curl_handle, CURLOPT_MAXREDIRS, 10);
			curl_easy_setopt(curl_handle, CURLOPT_WRITEFUNCTION, file_callback);
			curl_easy_setopt(curl_handle, CURLOPT_WRITEDATA, (void *) &config_data);
			curl_easy_setopt(curl_handle, CURLOPT_USERAGENT, "freeswitch-js/1.0");

			saveDepth = JS_SuspendRequest(cx);
			curl_easy_perform(curl_handle);
			JS_ResumeRequest(cx, saveDepth);

			curl_easy_cleanup(curl_handle);
			close(config_data.fd);
		} else {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Error!\n");
		}
	} else {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Error!\n");
	}

	return JS_TRUE;
}

static JSBool js_fetchurl(JSContext * cx, JSObject * obj, uintN argc, jsval * argv, jsval * rval)
{
	char *url = NULL;
	CURL *curl_handle = NULL;
	struct fetch_url_data config_data;
	int32 buffer_size = 65535;
	CURLcode code = 0;
	int saveDepth = 0;

	if (argc >= 1) {
		url = JS_GetStringBytes(JS_ValueToString(cx, argv[0]));
		if (argc > 1) {
			JS_ValueToInt32(cx, argv[1], &buffer_size);
		}

		curl_global_init(CURL_GLOBAL_ALL);
		curl_handle = curl_easy_init();
		if (!strncasecmp(url, "https", 5)) {
			curl_easy_setopt(curl_handle, CURLOPT_SSL_VERIFYPEER, 0);
			curl_easy_setopt(curl_handle, CURLOPT_SSL_VERIFYHOST, 0);
		}

		config_data.buffer_size = buffer_size;
		config_data.buffer = malloc(config_data.buffer_size);
		config_data.data_len = 0;
		if (config_data.buffer == NULL) {
			eval_some_js("~throw new Error(\"Failed to allocate data buffer.\");", cx, obj, rval);
			return JS_TRUE;
		}

		curl_easy_setopt(curl_handle, CURLOPT_URL, url);
		curl_easy_setopt(curl_handle, CURLOPT_FOLLOWLOCATION, 1);
		curl_easy_setopt(curl_handle, CURLOPT_MAXREDIRS, 10);
		curl_easy_setopt(curl_handle, CURLOPT_WRITEFUNCTION, fetch_url_callback);
		curl_easy_setopt(curl_handle, CURLOPT_WRITEDATA, (void *) &config_data);
		curl_easy_setopt(curl_handle, CURLOPT_USERAGENT, "freeswitch-js/1.0");

		saveDepth = JS_SuspendRequest(cx);
		code = curl_easy_perform(curl_handle);
		JS_ResumeRequest(cx, saveDepth);

		curl_easy_cleanup(curl_handle);

		if (code != CURLE_WRITE_ERROR) {
			config_data.buffer[config_data.data_len] = 0;
			*rval = STRING_TO_JSVAL(JS_NewStringCopyZ(cx, config_data.buffer));
		} else {
			char errmsg[256];
			switch_snprintf(errmsg, 256, "~throw new Error(\"Curl returned error %u.\");", (unsigned) code);
			eval_some_js(errmsg, cx, obj, rval);
		}

		free(config_data.buffer);

	} else {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Error!\n");
	}

	return JS_TRUE;
}
#endif

/* Session Object */
/*********************************************************************************/
enum session_tinyid {
	SESSION_NAME, SESSION_STATE,
	PROFILE_DIALPLAN, PROFILE_CID_NAME, PROFILE_CID_NUM, PROFILE_IP, PROFILE_ANI, PROFILE_ANI_II, PROFILE_DEST,
	SESSION_UUID, SESSION_CAUSE, SESSION_CAUSECODE
};

static JSFunctionSpec session_methods[] = {
	{"originate", session_originate, 2},
	{"setCallerData", session_set_callerdata, 2},
	{"setHangupHook", session_hanguphook, 1},
	{"setAutoHangup", session_autohangup, 1},
	{"sayPhrase", session_sayphrase, 1},
	{"streamFile", session_streamfile, 1},
	{"collectInput", session_collect_input, 1},
	{"recordFile", session_recordfile, 1},
	{"flushEvents", session_flush_events, 1},
	{"flushDigits", session_flush_digits, 1},
	{"speak", session_speak, 1},
	{"setVariable", session_set_variable, 1},
	{"getVariable", session_get_variable, 1},
	{"getDigits", session_get_digits, 1},
	{"answer", session_answer, 0},
	{"preAnswer", session_pre_answer, 0},
	{"generateXmlCdr", session_cdr, 0},
	{"ready", session_ready, 0},
	{"answered", session_answered, 0},
	{"mediaReady", session_media_ready, 0},
	{"waitForAnswer", session_wait_for_answer, 0},
	{"waitForMedia", session_wait_for_media, 0},
	{"getEvent", session_get_event, 0},
	{"sendEvent", session_send_event, 0},
	{"hangup", session_hangup, 0},
	{"execute", session_execute, 0},
	{"destroy", session_detach, 0},
	{"sleep", session_sleep, 1},
	{0}
};

static JSPropertySpec session_props[] = {
	{"name", SESSION_NAME, JSPROP_READONLY | JSPROP_PERMANENT},
	{"state", SESSION_STATE, JSPROP_READONLY | JSPROP_PERMANENT},
	{"dialplan", PROFILE_DIALPLAN, JSPROP_READONLY | JSPROP_PERMANENT},
	{"caller_id_name", PROFILE_CID_NAME, JSPROP_READONLY | JSPROP_PERMANENT},
	{"caller_id_num", PROFILE_CID_NUM, JSPROP_READONLY | JSPROP_PERMANENT},
	{"caller_id_number", PROFILE_CID_NUM, JSPROP_READONLY | JSPROP_PERMANENT},
	{"network_addr", PROFILE_IP, JSPROP_READONLY | JSPROP_PERMANENT},
	{"network_address", PROFILE_IP, JSPROP_READONLY | JSPROP_PERMANENT},
	{"ani", PROFILE_ANI, JSPROP_READONLY | JSPROP_PERMANENT},
	{"aniii", PROFILE_ANI_II, JSPROP_READONLY | JSPROP_PERMANENT},
	{"destination", PROFILE_DEST, JSPROP_READONLY | JSPROP_PERMANENT},
	{"uuid", SESSION_UUID, JSPROP_READONLY | JSPROP_PERMANENT},
	{"cause", SESSION_CAUSE, JSPROP_READONLY | JSPROP_PERMANENT},
	{"causecode", SESSION_CAUSECODE, JSPROP_READONLY | JSPROP_PERMANENT},
	{0}
};

static JSBool session_getProperty(JSContext * cx, JSObject * obj, jsval id, jsval * vp)
{
	struct js_session *jss = JS_GetPrivate(cx, obj);
	int param = 0;
	switch_channel_t *channel = NULL;
	switch_caller_profile_t *caller_profile = NULL;
	char *name = NULL;

	if (jss && jss->session) {
		channel = switch_core_session_get_channel(jss->session);
		caller_profile = switch_channel_get_caller_profile(channel);
	}

	name = JS_GetStringBytes(JS_ValueToString(cx, id));

	/* numbers are our props anything else is a method */
	if (name[0] >= 48 && name[0] <= 57) {
		param = atoi(name);
	} else {
		return JS_TRUE;
	}

	if (!channel) {
		switch (param) {
		case SESSION_CAUSE:
			if (jss) {
				*vp = STRING_TO_JSVAL(JS_NewStringCopyZ(cx, switch_channel_cause2str(jss->cause)));
			}
			break;
		case SESSION_CAUSECODE:
			if (jss) {
				*vp = INT_TO_JSVAL(jss->cause);
			}
			break;
		default:
			*vp = BOOLEAN_TO_JSVAL(JS_FALSE);
		}
		return JS_TRUE;
	}

	switch (param) {
	case SESSION_CAUSE:
		*vp = STRING_TO_JSVAL(JS_NewStringCopyZ(cx, switch_channel_cause2str(switch_channel_get_cause(channel))));
		break;
	case SESSION_CAUSECODE:
		*vp = INT_TO_JSVAL(switch_channel_get_cause(channel));
		break;
	case SESSION_NAME:
		*vp = STRING_TO_JSVAL(JS_NewStringCopyZ(cx, switch_channel_get_name(channel)));
		break;
	case SESSION_UUID:
		*vp = STRING_TO_JSVAL(JS_NewStringCopyZ(cx, switch_channel_get_uuid(channel)));
		break;
	case SESSION_STATE:
		*vp = STRING_TO_JSVAL(JS_NewStringCopyZ(cx, switch_channel_state_name(switch_channel_get_state(channel))));
		break;
	case PROFILE_DIALPLAN:
		if (caller_profile) {
			*vp = STRING_TO_JSVAL(JS_NewStringCopyZ(cx, caller_profile->dialplan));
		}
		break;
	case PROFILE_CID_NAME:
		if (caller_profile) {
			*vp = STRING_TO_JSVAL(JS_NewStringCopyZ(cx, caller_profile->caller_id_name));
		}
		break;
	case PROFILE_CID_NUM:
		if (caller_profile) {
			*vp = STRING_TO_JSVAL(JS_NewStringCopyZ(cx, caller_profile->caller_id_number));
		}
		break;
	case PROFILE_IP:
		if (caller_profile) {
			*vp = STRING_TO_JSVAL(JS_NewStringCopyZ(cx, caller_profile->network_addr));
		}
		break;
	case PROFILE_ANI:
		if (caller_profile) {
			*vp = STRING_TO_JSVAL(JS_NewStringCopyZ(cx, caller_profile->ani));
		}
		break;
	case PROFILE_ANI_II:
		if (caller_profile) {
			*vp = STRING_TO_JSVAL(JS_NewStringCopyZ(cx, caller_profile->aniii));
		}
		break;
	case PROFILE_DEST:
		if (caller_profile) {
			*vp = STRING_TO_JSVAL(JS_NewStringCopyZ(cx, caller_profile->destination_number));
		}
		break;
	default:
		*vp = BOOLEAN_TO_JSVAL(JS_FALSE);
		break;
	}

	return JS_TRUE;
}

JSClass session_class = {
	"Session", JSCLASS_HAS_PRIVATE,
	JS_PropertyStub, JS_PropertyStub, session_getProperty, DEFAULT_SET_PROPERTY,
	JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub, session_destroy, NULL, NULL, NULL,
	session_construct
};

static JSObject *new_js_session(JSContext * cx, JSObject * obj, switch_core_session_t *session, struct js_session **jss, char *name, int flags)
{
	JSObject *session_obj;
	if ((session_obj = JS_DefineObject(cx, obj, name, &session_class, NULL, 0))) {
		*jss = malloc(sizeof(**jss));
		switch_assert(*jss);
		memset(*jss, 0, sizeof(**jss));

		(*jss)->session = session;
		(*jss)->flags = flags;
		(*jss)->cx = cx;
		(*jss)->obj = session_obj;
		(*jss)->stack_depth = 0;
		if ((JS_SetPrivate(cx, session_obj, *jss) &&
			 JS_DefineProperties(cx, session_obj, session_props) && JS_DefineFunctions(cx, session_obj, session_methods))) {
			if (switch_core_session_read_lock_hangup(session) != SWITCH_STATUS_SUCCESS) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Read Lock Failure.\n");
				free(*jss);
				return NULL;
			}
			return session_obj;
		} else {
			free(*jss);
		}
	}

	return NULL;
}

/* Session Object */
/*********************************************************************************/

static JSBool session_construct(JSContext * cx, JSObject * obj, uintN argc, jsval * argv, jsval * rval)
{
	struct js_session *jss = NULL;
	JSObject *session_obj = NULL;

	jss = malloc(sizeof(*jss));
	switch_assert(jss);
	memset(jss, 0, sizeof(*jss));
	jss->cx = cx;
	jss->obj = obj;
	JS_SetPrivate(cx, obj, jss);

	*rval = BOOLEAN_TO_JSVAL(JS_FALSE);

	if (argc > 0) {
		char *uuid = JS_GetStringBytes(JS_ValueToString(cx, argv[0]));

		if (!strchr(uuid, '/')) {
			jss->session = switch_core_session_locate(uuid);
			switch_set_flag(jss, S_HUP);
			*rval = BOOLEAN_TO_JSVAL(JS_TRUE);
		} else {
			struct js_session *old_jss = NULL;

			if (argc > 1) {
				if (JS_ValueToObject(cx, argv[1], &session_obj) && session_obj) {
					old_jss = JS_GetPrivate(cx, session_obj);
				}
			}
			if (switch_ivr_originate(old_jss ? old_jss->session : NULL,
									 &jss->session, &jss->cause, uuid, 60, NULL, NULL, NULL, NULL, NULL, SOF_NONE, NULL) == SWITCH_STATUS_SUCCESS) {
				switch_set_flag(jss, S_HUP);
				switch_channel_set_state(switch_core_session_get_channel(jss->session), CS_SOFT_EXECUTE);
				*rval = BOOLEAN_TO_JSVAL(JS_TRUE);
			} else {
				*rval = STRING_TO_JSVAL(JS_NewStringCopyZ(cx, switch_channel_cause2str(jss->cause)));
			}
		}
	}

	return JS_TRUE;
}

static JSBool session_set_callerdata(JSContext * cx, JSObject * obj, uintN argc, jsval * argv, jsval * rval)
{
	struct js_session *jss = NULL;
	jss = JS_GetPrivate(cx, obj);
	*rval = BOOLEAN_TO_JSVAL(JS_FALSE);

	if (argc > 1) {
		char *var, *val, **toset = NULL;
		var = JS_GetStringBytes(JS_ValueToString(cx, argv[0]));
		val = JS_GetStringBytes(JS_ValueToString(cx, argv[1]));

		if (!strcasecmp(var, "dialplan")) {
			toset = &jss->dialplan;
		} else if (!strcasecmp(var, "username")) {
			toset = &jss->username;
		} else if (!strcasecmp(var, "caller_id_name")) {
			toset = &jss->caller_id_name;
		} else if (!strcasecmp(var, "ani")) {
			toset = &jss->ani;
		} else if (!strcasecmp(var, "aniii")) {
			toset = &jss->aniii;
		} else if (!strcasecmp(var, "caller_id_number")) {
			toset = &jss->caller_id_number;
		} else if (!strcasecmp(var, "network_addr")) {
			toset = &jss->network_addr;
		} else if (!strcasecmp(var, "rdnis")) {
			toset = &jss->rdnis;
		} else if (!strcasecmp(var, "destination_number")) {
			toset = &jss->destination_number;
		} else if (!strcasecmp(var, "context")) {
			toset = &jss->context;
		}

		if (toset) {
			switch_safe_free(*toset);
			*toset = strdup(val);
		}

	}

	return JS_TRUE;
}

static JSBool session_originate(JSContext * cx, JSObject * obj, uintN argc, jsval * argv, jsval * rval)
{
	struct js_session *jss = NULL;
	switch_memory_pool_t *pool = NULL;

	jss = JS_GetPrivate(cx, obj);
	jss->cause = SWITCH_CAUSE_DESTINATION_OUT_OF_ORDER;


	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "This method is deprecated, please use new Session(\"<dial string>\", a_leg) \n");

	if (jss->session) {
		eval_some_js("~throw new Error(\"cannot call this method on an initialized session\");", cx, obj, rval);
		return JS_FALSE;
	}

	if (argc > 1) {
		JSObject *session_obj;
		switch_core_session_t *session = NULL, *peer_session = NULL;
		switch_caller_profile_t *caller_profile = NULL, *orig_caller_profile = NULL;
		const char *dest = NULL;
		const char *dialplan = NULL;
		const char *cid_name = "";
		const char *cid_num = "";
		const char *network_addr = "";
		const char *ani = "";
		const char *aniii = "";
		const char *rdnis = "";
		const char *context = "";
		const char *username = NULL;
		char *to = NULL;
		char *tmp;
		jsrefcount saveDepth;
		switch_status_t status;

		*rval = BOOLEAN_TO_JSVAL(JS_FALSE);

		if (JS_ValueToObject(cx, argv[0], &session_obj)) {
			struct js_session *old_jss = NULL;
			if (session_obj && (old_jss = JS_GetPrivate(cx, session_obj))) {

				if (old_jss == jss) {
					eval_some_js("~throw new Error(\"Supplied a_leg session is the same as our session\");", cx, obj, rval);
					return JS_FALSE;

				};

				if (!old_jss->session) {
					eval_some_js("~throw new Error(\"Supplied a_leg session is not initilaized!\");", cx, obj, rval);
					return JS_FALSE;
				}

				session = old_jss->session;
				orig_caller_profile = switch_channel_get_caller_profile(switch_core_session_get_channel(session));
				dialplan = orig_caller_profile->dialplan;
				cid_name = orig_caller_profile->caller_id_name;
				cid_num = orig_caller_profile->caller_id_number;
				ani = orig_caller_profile->ani;
				aniii = orig_caller_profile->aniii;
				rdnis = orig_caller_profile->rdnis;
				context = orig_caller_profile->context;
				username = orig_caller_profile->username;
			}

		}

		if (!zstr(jss->dialplan))
			dialplan = jss->dialplan;
		if (!zstr(jss->caller_id_name))
			cid_name = jss->caller_id_name;
		if (!zstr(jss->caller_id_number))
			cid_num = jss->caller_id_number;
		if (!zstr(jss->ani))
			ani = jss->ani;
		if (!zstr(jss->aniii))
			aniii = jss->aniii;
		if (!zstr(jss->rdnis))
			rdnis = jss->rdnis;
		if (!zstr(jss->context))
			context = jss->context;
		if (!zstr(jss->username))
			username = jss->username;

		dest = JS_GetStringBytes(JS_ValueToString(cx, argv[1]));

		if (!strchr(dest, '/')) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "Invalid Channel String\n");
			goto done;
		}

		if (argc > 2) {
			tmp = JS_GetStringBytes(JS_ValueToString(cx, argv[2]));
			if (!zstr(tmp)) {
				to = tmp;
			}
		}

		if (switch_core_new_memory_pool(&pool) != SWITCH_STATUS_SUCCESS) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "OH OH no pool\n");
			return JS_FALSE;
		}

		caller_profile = switch_caller_profile_new(pool, username, dialplan, cid_name, cid_num, network_addr, ani, aniii, rdnis, modname, context, dest);

		saveDepth = JS_SuspendRequest(cx);
		status =
			switch_ivr_originate(session, &peer_session, &jss->cause, dest, to ? atoi(to) : 60, NULL, NULL, NULL, caller_profile, NULL, SOF_NONE, NULL);
		JS_ResumeRequest(cx, saveDepth);

		if (status != SWITCH_STATUS_SUCCESS) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "Cannot Create Outgoing Channel! [%s]\n", dest);
			goto done;
		}

		jss->session = peer_session;
		switch_set_flag(jss, S_HUP);
		*rval = BOOLEAN_TO_JSVAL(JS_TRUE);
		switch_channel_set_state(switch_core_session_get_channel(jss->session), CS_SOFT_EXECUTE);

	} else {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "Missing Args\n");
	}

  done:
	return JS_TRUE;
}

static void session_destroy(JSContext * cx, JSObject * obj)
{
	struct js_session *jss;

	if (cx && obj) {
		if ((jss = JS_GetPrivate(cx, obj))) {
			JS_SetPrivate(cx, obj, NULL);
			if (jss->speech && *jss->speech->sh.name) {
				destroy_speech_engine(jss);
			}

			if (jss->session) {
				switch_core_session_t *session = jss->session;
				switch_channel_t *channel = switch_core_session_get_channel(session);

				switch_channel_set_private(channel, "jss", NULL);
				switch_core_event_hook_remove_state_change(session, hanguphook);

				if (switch_test_flag(jss, S_HUP)) {
					switch_channel_hangup(channel, SWITCH_CAUSE_NORMAL_CLEARING);
				}

				switch_safe_free(jss->dialplan);
				switch_safe_free(jss->username);
				switch_safe_free(jss->caller_id_name);
				switch_safe_free(jss->ani);
				switch_safe_free(jss->aniii);
				switch_safe_free(jss->caller_id_number);
				switch_safe_free(jss->network_addr);
				switch_safe_free(jss->rdnis);
				switch_safe_free(jss->destination_number);
				switch_safe_free(jss->context);
				switch_core_session_rwunlock(session);
			}

			free(jss);

		}
	}

	return;
}

/* FileIO Object */
/*********************************************************************************/
static JSBool fileio_construct(JSContext * cx, JSObject * obj, uintN argc, jsval * argv, jsval * rval)
{
	switch_memory_pool_t *pool;
	switch_file_t *fd;
	char *path, *flags_str;
	unsigned int flags = 0;
	struct fileio_obj *fio;

	if (argc > 1) {
		path = JS_GetStringBytes(JS_ValueToString(cx, argv[0]));
		flags_str = JS_GetStringBytes(JS_ValueToString(cx, argv[1]));

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
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Cannot Open File: %s\n", path);
			switch_core_destroy_memory_pool(&pool);
			*rval = BOOLEAN_TO_JSVAL(JS_FALSE);
			return JS_TRUE;
		}
		fio = switch_core_alloc(pool, sizeof(*fio));
		fio->fd = fd;
		fio->pool = pool;
		fio->path = switch_core_strdup(pool, path);
		fio->flags = flags;
		JS_SetPrivate(cx, obj, fio);
		return JS_TRUE;
	}

	return JS_TRUE;
}
static void fileio_destroy(JSContext * cx, JSObject * obj)
{
	struct fileio_obj *fio = JS_GetPrivate(cx, obj);

	if (fio) {
		switch_memory_pool_t *pool;
		if (fio->fd) {
			switch_file_close(fio->fd);
		}
		pool = fio->pool;
		switch_core_destroy_memory_pool(&pool);
		pool = NULL;
	}
}

static JSBool fileio_read(JSContext * cx, JSObject * obj, uintN argc, jsval * argv, jsval * rval)
{
	struct fileio_obj *fio = JS_GetPrivate(cx, obj);
	int32 bytes = 0;
	switch_size_t read = 0;

	*rval = BOOLEAN_TO_JSVAL(JS_FALSE);

	if (!fio) {
		*rval = BOOLEAN_TO_JSVAL(JS_FALSE);
		return JS_TRUE;
	}

	if (!(fio->flags & SWITCH_FOPEN_READ)) {
		return JS_TRUE;
	}

	if (argc > 0) {
		JS_ValueToInt32(cx, argv[0], &bytes);
	}

	if (bytes) {
		if (!fio->buf || fio->bufsize < bytes) {
			fio->buf = switch_core_alloc(fio->pool, bytes);
			fio->bufsize = bytes;
		}
		read = bytes;
		switch_file_read(fio->fd, fio->buf, &read);
		fio->buflen = read;
		*rval = BOOLEAN_TO_JSVAL((read > 0) ? JS_TRUE : JS_FALSE);
	}

	return JS_TRUE;
}

static JSBool fileio_data(JSContext * cx, JSObject * obj, uintN argc, jsval * argv, jsval * rval)
{
	struct fileio_obj *fio = JS_GetPrivate(cx, obj);

	if (!fio) {
		*rval = BOOLEAN_TO_JSVAL(JS_FALSE);
		return JS_TRUE;
	}

	*rval = STRING_TO_JSVAL(JS_NewStringCopyZ(cx, fio->buf));
	return JS_TRUE;
}

static JSBool fileio_write(JSContext * cx, JSObject * obj, uintN argc, jsval * argv, jsval * rval)
{
	struct fileio_obj *fio = JS_GetPrivate(cx, obj);
	switch_size_t wrote = 0;
	char *data = NULL;

	if (!fio) {
		*rval = BOOLEAN_TO_JSVAL(JS_FALSE);
		return JS_TRUE;
	}

	if (!(fio->flags & SWITCH_FOPEN_WRITE)) {
		*rval = BOOLEAN_TO_JSVAL(JS_FALSE);
		return JS_TRUE;
	}

	if (argc > 0) {
		data = JS_GetStringBytes(JS_ValueToString(cx, argv[0]));
	}

	if (data) {
		wrote = strlen(data);
		*rval = BOOLEAN_TO_JSVAL((switch_file_write(fio->fd, data, &wrote) == SWITCH_STATUS_SUCCESS) ? JS_TRUE : JS_FALSE);
	}

	*rval = BOOLEAN_TO_JSVAL(JS_FALSE);
	return JS_TRUE;
}

enum fileio_tinyid {
	FILEIO_PATH, FILEIO_OPEN
};

static JSFunctionSpec fileio_methods[] = {
	{"read", fileio_read, 1},
	{"write", fileio_write, 1},
	{"data", fileio_data, 0},
	{0}
};

static JSPropertySpec fileio_props[] = {
	{"path", FILEIO_PATH, JSPROP_READONLY | JSPROP_PERMANENT},
	{"open", FILEIO_OPEN, JSPROP_READONLY | JSPROP_PERMANENT},
	{0}
};

static JSBool fileio_getProperty(JSContext * cx, JSObject * obj, jsval id, jsval * vp)
{
	JSBool res = JS_TRUE;
	struct fileio_obj *fio = JS_GetPrivate(cx, obj);
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
	case FILEIO_PATH:
		if (fio) {
			*vp = STRING_TO_JSVAL(JS_NewStringCopyZ(cx, fio->path));
		} else {
			*vp = BOOLEAN_TO_JSVAL(JS_FALSE);
		}
		break;
	case FILEIO_OPEN:
		*vp = BOOLEAN_TO_JSVAL(fio ? JS_TRUE : JS_FALSE);
		break;
	}

	return res;
}

JSClass fileio_class = {
	"FileIO", JSCLASS_HAS_PRIVATE,
	JS_PropertyStub, JS_PropertyStub, fileio_getProperty, DEFAULT_SET_PROPERTY,
	JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub, fileio_destroy, NULL, NULL, NULL,
	fileio_construct
};

/* Built-In*/
/*********************************************************************************/
static JSBool js_exit(JSContext * cx, JSObject * obj, uintN argc, jsval * argv, jsval * rval)
{
	char *supplied_error, code_buf[256] = "";

	if (argc > 0 && (supplied_error = JS_GetStringBytes(JS_ValueToString(cx, argv[0])))) {
		switch_snprintf(code_buf, sizeof(code_buf), "~throw new Error(\"%s\");", supplied_error);
		eval_some_js(code_buf, cx, obj, rval);
	}
	return JS_FALSE;
}

static JSBool js_log(JSContext * cx, JSObject * obj, uintN argc, jsval * argv, jsval * rval)
{
	char *level_str, *msg;
	switch_log_level_t level = SWITCH_LOG_DEBUG;
	JSScript *script = NULL;
	const char *file = __FILE__;
	int line = __LINE__;
	JSStackFrame *caller;

	caller = JS_GetScriptedCaller(cx, NULL);
	script = JS_GetFrameScript(cx, caller);

	if (script) {
		file = JS_GetScriptFilename(cx, script);
		line = JS_GetScriptBaseLineNumber(cx, script);
	}

	if (argc > 1) {
		if ((level_str = JS_GetStringBytes(JS_ValueToString(cx, argv[0])))) {
			level = switch_log_str2level(level_str);
			if (level == SWITCH_LOG_INVALID) {
				level = SWITCH_LOG_DEBUG;
			}
		}

		if ((msg = JS_GetStringBytes(JS_ValueToString(cx, argv[1])))) {
			switch_log_printf(SWITCH_CHANNEL_ID_LOG, file, "console_log", line, NULL, level, "%s", msg);
			return JS_TRUE;
		}
	} else if (argc > 0) {
		if ((msg = JS_GetStringBytes(JS_ValueToString(cx, argv[0])))) {
			switch_log_printf(SWITCH_CHANNEL_ID_LOG, file, "console_log", line, NULL, level, "%s", msg);
			return JS_TRUE;
		}
	}

	return JS_FALSE;
}

static JSBool js_global_set(JSContext * cx, JSObject * obj, uintN argc, jsval * argv, jsval * rval)
{
	char *var_name = NULL, *val = NULL;
	if (argc > 1) {
		var_name = JS_GetStringBytes(JS_ValueToString(cx, argv[0]));
		val = JS_GetStringBytes(JS_ValueToString(cx, argv[1]));
		switch_core_set_variable(var_name, val);
		return JS_TRUE;
	}
	/* this is so the wrong error message to throw for this one */
	eval_some_js("~throw new Error(\"var name not supplied!\");", cx, obj, rval);
	return JS_FALSE;
}

static JSBool js_global_get(JSContext * cx, JSObject * obj, uintN argc, jsval * argv, jsval * rval)
{
	char *var_name = NULL, *val = NULL;

	if (argc > 0) {
		var_name = JS_GetStringBytes(JS_ValueToString(cx, argv[0]));
		val = switch_core_get_variable(var_name);
		*rval = STRING_TO_JSVAL(JS_NewStringCopyZ(cx, val));
		return JS_TRUE;
	}

	eval_some_js("~throw new Error(\"var name not supplied!\");", cx, obj, rval);
	return JS_FALSE;
}

static JSBool js_include(JSContext * cx, JSObject * obj, uintN argc, jsval * argv, jsval * rval)
{
	char *code;
	if (argc > 0 && (code = JS_GetStringBytes(JS_ValueToString(cx, argv[0])))) {
		if (eval_some_js(code, cx, obj, rval) <= 0) {
			return JS_FALSE;
		}
		return JS_TRUE;
	}
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Invalid Arguements\n");
	return JS_FALSE;
}

static JSBool js_api_sleep(JSContext * cx, JSObject * obj, uintN argc, jsval * argv, jsval * rval)
{
	int32 msec = 0;

	if (argc > 0) {
		JS_ValueToInt32(cx, argv[0], &msec);
	}

	if (msec) {
		int saveDepth;
		saveDepth = JS_SuspendRequest(cx);
		switch_yield(msec * 1000);
		JS_ResumeRequest(cx, saveDepth);
	} else {
		eval_some_js("~throw new Error(\"No Time specified\");", cx, obj, rval);
	}

	return JS_TRUE;

}

static JSBool js_api_use(JSContext * cx, JSObject * obj, uintN argc, jsval * argv, jsval * rval)
{
	char *mod_name = NULL;

	if (argc > 0 && (mod_name = JS_GetStringBytes(JS_ValueToString(cx, argv[0])))) {
		const sm_module_interface_t *mp;

		if ((mp = switch_core_hash_find(module_manager.load_hash, mod_name))) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Loading %s\n", mod_name);
			mp->spidermonkey_load(cx, obj);
		} else {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Error loading %s\n", mod_name);
		}
	} else {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Invalid Filename\n");
	}

	return JS_TRUE;
}

static JSBool js_api_execute(JSContext * cx, JSObject * obj, uintN argc, jsval * argv, jsval * rval)
{
	if (argc > 0) {
		char *cmd = JS_GetStringBytes(JS_ValueToString(cx, argv[0]));
		char *arg = NULL;
		switch_core_session_t *session = NULL;
		switch_stream_handle_t stream = { 0 };

		if (!strcasecmp(cmd, "jsapi")) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Invalid API Call!\n");
			*rval = BOOLEAN_TO_JSVAL(JS_FALSE);
			return JS_TRUE;
		}

		if (argc > 1) {
			arg = JS_GetStringBytes(JS_ValueToString(cx, argv[1]));
		}

		if (argc > 2) {
			JSObject *session_obj;
			struct js_session *jss;
			if (JS_ValueToObject(cx, argv[2], &session_obj)) {
				if ((jss = JS_GetPrivate(cx, session_obj))) {
					session = jss->session;
				}
			}
		}

		SWITCH_STANDARD_STREAM(stream);
		switch_api_execute(cmd, arg, session, &stream);

		*rval = STRING_TO_JSVAL(JS_NewStringCopyZ(cx, switch_str_nil((char *) stream.data)));
		switch_safe_free(stream.data);

	} else {
		*rval = STRING_TO_JSVAL(JS_NewStringCopyZ(cx, ""));
	}

	return JS_TRUE;
}

static JSBool js_bridge(JSContext * cx, JSObject * obj, uintN argc, jsval * argv, jsval * rval)
{
	struct js_session *jss_a = NULL, *jss_b = NULL;
	JSObject *session_obj_a = NULL, *session_obj_b = NULL;
	void *bp = NULL;
	int len = 0;
	switch_input_callback_function_t dtmf_func = NULL;
	struct input_callback_state cb_state = { 0 };
	JSFunction *function;

	*rval = BOOLEAN_TO_JSVAL(JS_FALSE);

	if (argc > 1) {
		if (JS_ValueToObject(cx, argv[0], &session_obj_a)) {
			if (!(jss_a = JS_GetPrivate(cx, session_obj_a))) {
				eval_some_js("~throw new Error(\"Cannot find session A\");", cx, obj, rval);
				return JS_FALSE;
			}
		}
		if (JS_ValueToObject(cx, argv[1], &session_obj_b)) {
			if (!(jss_b = JS_GetPrivate(cx, session_obj_b))) {
				eval_some_js("~throw new Error(\"Cannot find session B\");", cx, obj, rval);
				return JS_FALSE;
			}
		}
	}

	if (!(jss_a && jss_a->session)) {
		eval_some_js("~throw new Error(\"session A is not ready!\");", cx, obj, rval);
		return JS_FALSE;
	}

	if (!(jss_b && jss_b->session)) {
		eval_some_js("~throw new Error(\"session B is not ready!\");", cx, obj, rval);
		return JS_FALSE;
	}

	if (argc > 2) {
		if ((function = JS_ValueToFunction(cx, argv[2]))) {
			memset(&cb_state, 0, sizeof(cb_state));
			cb_state.function = function;

			if (argc > 3) {
				cb_state.arg = argv[3];
			}

			cb_state.cx = cx;
			cb_state.obj = obj;
			cb_state.jss_a = jss_a;
			cb_state.jss_b = jss_b;
			cb_state.session_obj_a = session_obj_a;
			cb_state.session_obj_b = session_obj_b;
			cb_state.session_state = jss_a;
			dtmf_func = js_collect_input_callback;
			bp = &cb_state;
			len = sizeof(cb_state);
		}
	}

	cb_state.saveDepth = JS_SuspendRequest(cx);
	switch_ivr_multi_threaded_bridge(jss_a->session, jss_b->session, dtmf_func, bp, bp);
	JS_ResumeRequest(cx, cb_state.saveDepth);

	*rval = BOOLEAN_TO_JSVAL(JS_TRUE);

	return JS_TRUE;
}

/* Replace this with more robust version later */
static JSBool js_system(JSContext * cx, JSObject * obj, uintN argc, jsval * argv, jsval * rval)
{
	char *cmd;
	int saveDepth, result;
	*rval = BOOLEAN_TO_JSVAL(JS_FALSE);

	if (argc > 0 && (cmd = JS_GetStringBytes(JS_ValueToString(cx, argv[0])))) {
		saveDepth = JS_SuspendRequest(cx);
		result = switch_system(cmd, SWITCH_TRUE);
		JS_ResumeRequest(cx, saveDepth);
		*rval = INT_TO_JSVAL(result);
		return JS_TRUE;
	}

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Invalid Arguements\n");
	return JS_FALSE;
}

static JSBool js_file_unlink(JSContext * cx, JSObject * obj, uintN argc, jsval * argv, jsval * rval)
{
	const char *path;
	*rval = BOOLEAN_TO_JSVAL(JS_FALSE);
	if (argc > 0 && (path = (const char *) JS_GetStringBytes(JS_ValueToString(cx, argv[0])))) {
		if ((switch_file_remove(path, NULL)) == SWITCH_STATUS_SUCCESS) {
			*rval = BOOLEAN_TO_JSVAL(JS_TRUE);
		}
		return JS_TRUE;
	}
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Invalid Arguements\n");
	return JS_FALSE;
}

static JSFunctionSpec fs_functions[] = {
	{"console_log", js_log, 2},
	{"consoleLog", js_log, 2},
	{"getGlobalVariable", js_global_get, 2},
	{"setGlobalVariable", js_global_set, 2},
	{"exit", js_exit, 0},
	{"include", js_include, 1},
	{"bridge", js_bridge, 2},
	{"apiExecute", js_api_execute, 2},
	{"use", js_api_use, 1},
	{"msleep", js_api_sleep, 1},
	{"fileDelete", js_file_unlink, 1},
	{"system", js_system, 1},
#ifdef HAVE_CURL
	{"fetchURL", js_fetchurl, 1},
	{"fetchURLHash", js_fetchurl_hash, 1},
	{"fetchURLFile", js_fetchurl_file, 1},
	{"fetchUrl", js_fetchurl, 1},
	{"fetchUrlHash", js_fetchurl_hash, 1},
	{"fetchUrlFile", js_fetchurl_file, 1},
#endif
	{0}
};

static int env_init(JSContext * cx, JSObject * javascript_object)
{
	JS_DefineFunctions(cx, javascript_object, fs_functions);

	JS_InitStandardClasses(cx, javascript_object);

	JS_InitClass(cx, javascript_object, NULL, &session_class, session_construct, 3, session_props, session_methods, session_props, session_methods);

	JS_InitClass(cx, javascript_object, NULL, &fileio_class, fileio_construct, 3, fileio_props, fileio_methods, fileio_props, fileio_methods);

	JS_InitClass(cx, javascript_object, NULL, &event_class, event_construct, 3, event_props, event_methods, event_props, event_methods);

	JS_InitClass(cx, javascript_object, NULL, &dtmf_class, dtmf_construct, 3, dtmf_props, dtmf_methods, dtmf_props, dtmf_methods);

	JS_InitClass(cx, javascript_object, NULL, &pcre_class, pcre_construct, 3, pcre_props, pcre_methods, pcre_props, pcre_methods);

	return 1;
}

static void js_parse_and_execute(switch_core_session_t *session, const char *input_code, struct request_obj *ro)
{
	JSObject *javascript_global_object = NULL;
	char buf[1024], *arg, *argv[512];
	const char *script;
	int argc = 0, x = 0, y = 0;
	unsigned int flags = 0;
	struct js_session *jss;
	JSContext *cx = NULL;
	jsval rval;

	if (zstr(input_code)) {
		return;
	}

	if ((cx = JS_NewContext(globals.rt, globals.gStackChunkSize))) {
		JS_BeginRequest(cx);
		JS_SetErrorReporter(cx, js_error);
		javascript_global_object = JS_NewObject(cx, &global_class, NULL, NULL);
		env_init(cx, javascript_global_object);
		JS_SetGlobalObject(cx, javascript_global_object);

		/* Emaculent conception of session object into the script if one is available */
		if (!(session && new_js_session(cx, javascript_global_object, session, &jss, "session", flags))) {
			switch_snprintf(buf, sizeof(buf), "~var session = false;");
			eval_some_js(buf, cx, javascript_global_object, &rval);
		}
		if (ro) {
			new_request(cx, javascript_global_object, ro);
		}

	} else {
		abort();
	}

	script = input_code;

	if (*script != '~') {
		if ((arg = strchr(script, ' '))) {
			*arg++ = '\0';
			argc = switch_separate_string(arg, ' ', argv, (sizeof(argv) / sizeof(argv[0])));
		}

		if (!argc) {
			switch_snprintf(buf, sizeof(buf), "~var argv = new Array();");
			eval_some_js(buf, cx, javascript_global_object, &rval);
		} else {
			/* create a js doppleganger of this argc/argv */
			switch_snprintf(buf, sizeof(buf), "~var argv = new Array(%d);", argc);
			eval_some_js(buf, cx, javascript_global_object, &rval);
			switch_snprintf(buf, sizeof(buf), "~var argc = %d", argc);
			eval_some_js(buf, cx, javascript_global_object, &rval);

			for (y = 0; y < argc; y++) {
				switch_snprintf(buf, sizeof(buf), "~argv[%d] = \"%s\";", x++, argv[y]);
				eval_some_js(buf, cx, javascript_global_object, &rval);
			}
		}
	}

	if (cx) {
		eval_some_js(script, cx, javascript_global_object, &rval);
		JS_DestroyContext(cx);
	}

	return;
}

SWITCH_STANDARD_APP(js_dp_function)
{
	js_parse_and_execute(session, data, NULL);
}

struct js_task {
	switch_memory_pool_t *pool;
	char *code;
};

static void *SWITCH_THREAD_FUNC js_thread_run(switch_thread_t *thread, void *obj)
{
	struct js_task *task = (struct js_task *) obj;
	switch_memory_pool_t *pool;

	js_parse_and_execute(NULL, task->code, NULL);

	if ((pool = task->pool)) {
		switch_core_destroy_memory_pool(&pool);
	}

	return NULL;
}

static void js_thread_launch(const char *text)
{
	switch_thread_t *thread;
	switch_threadattr_t *thd_attr = NULL;
	struct js_task *task;
	switch_memory_pool_t *pool;

	if (zstr(text)) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "missing required input!\n");
		return;
	}

	if (switch_core_new_memory_pool(&pool) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "OH OH no pool\n");
		return;
	}

	task = switch_core_alloc(pool, sizeof(*task));
	task->pool = pool;
	task->code = switch_core_strdup(pool, text);

	switch_threadattr_create(&thd_attr, pool);
	switch_threadattr_detach_set(thd_attr, 1);
	switch_threadattr_stacksize_set(thd_attr, SWITCH_THREAD_STACKSIZE);
	switch_thread_create(&thread, thd_attr, js_thread_run, task, pool);
}

SWITCH_STANDARD_API(jsapi_function)
{
	struct request_obj ro = { 0 };
	char *path_info = NULL;

	if (stream->param_event) {
		path_info = switch_event_get_header(stream->param_event, "http-path-info");
	}

	if (zstr(cmd) && path_info) {
		cmd = path_info;
	}

	if (zstr(cmd)) {
		stream->write_function(stream, "USAGE: %s\n", jsapi_interface->syntax);
		return SWITCH_STATUS_SUCCESS;
	}

	ro.cmd = cmd;
	ro.session = session;
	ro.stream = stream;

	js_parse_and_execute(session, (char *) cmd, &ro);

	return SWITCH_STATUS_SUCCESS;
}

SWITCH_STANDARD_API(launch_async)
{
	if (zstr(cmd)) {
		stream->write_function(stream, "USAGE: %s\n", js_run_interface->syntax);
		return SWITCH_STATUS_SUCCESS;
	}

	js_thread_launch(cmd);
	stream->write_function(stream, "OK\n");
	return SWITCH_STATUS_SUCCESS;
}

SWITCH_MODULE_LOAD_FUNCTION(mod_spidermonkey_load)
{
	switch_application_interface_t *app_interface;
	switch_status_t status;

	if ((status = init_js()) != SWITCH_STATUS_SUCCESS) {
		return status;
	}

	/* connect my internal structure to the blank pointer passed to me */
	*module_interface = switch_loadable_module_create_module_interface(pool, modname);
	SWITCH_ADD_API(js_run_interface, "jsrun", "run a script", launch_async, "jsrun <script> [additional_vars [...]]");
	SWITCH_ADD_API(jsapi_interface, "jsapi", "execute an api call", jsapi_function, "jsapi <script> [additional_vars [...]]");
	SWITCH_ADD_APP(app_interface, "javascript", "Launch JS ivr", "Run a javascript ivr on a channel", js_dp_function, "<script> [additional_vars [...]]",
				   SAF_SUPPORT_NOMEDIA);

	curl_global_init(CURL_GLOBAL_ALL);

	/* indicate that the module should continue to be loaded */
	return SWITCH_STATUS_NOUNLOAD;
}

SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_spidermonkey_shutdown)
{
	// this causes a crash
	//JS_DestroyRuntime(globals.rt);

	curl_global_cleanup();

	switch_core_hash_destroy(&module_manager.mod_hash);
	switch_core_hash_destroy(&module_manager.load_hash);
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
