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
 * fseventhandler.cpp -- JavaScript EventHandler class
 *
 */

#include "fseventhandler.hpp"
#include "fsevent.hpp"
#include "fssession.hpp"

#define MAX_QUEUE_LEN 100000

using namespace std;
using namespace v8;

typedef struct {
	char *cmd;
	char *arg;
	char uuid_str[SWITCH_UUID_FORMATTED_LENGTH + 1];
	int ack;
	switch_memory_pool_t *pool;
} api_command_struct_t;

static const char js_class_name[] = "EventHandler";

FSEventHandler::~FSEventHandler(void)
{
	v8_remove_event_handler(this);

	if (_event_hash) switch_core_hash_destroy(&_event_hash);

	if (_event_queue) {
		void *pop;

		while (switch_queue_trypop(_event_queue, &pop) == SWITCH_STATUS_SUCCESS) {
			switch_event_t *pevent = (switch_event_t *) pop;
			if (pevent) {
				switch_event_destroy(&pevent);
			}
		}
	}

	if (_filters) switch_event_destroy(&_filters);
	if (_mutex) switch_mutex_destroy(_mutex);
	if (_pool) switch_core_destroy_memory_pool(&_pool);
}

void FSEventHandler::Init()
{
	if (switch_core_new_memory_pool(&_pool) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "OH OH no pool\n");
		return;
	}

	switch_mutex_init(&_mutex, SWITCH_MUTEX_NESTED, _pool);
	switch_core_hash_init(&_event_hash);
	switch_queue_create(&_event_queue, MAX_QUEUE_LEN, _pool);

	_filters = NULL;
	memset(&_event_list, 0, sizeof(_event_list));

	v8_add_event_handler(this);
}

string FSEventHandler::GetJSClassName()
{
	return js_class_name;
}

void FSEventHandler::QueueEvent(switch_event_t *event)
{
	switch_event_t *clone;
	int send = 0;

	switch_mutex_lock(_mutex);

	if (_event_list[SWITCH_EVENT_ALL]) {
		send = 1;
	} else if ((_event_list[event->event_id])) {
		if (event->event_id != SWITCH_EVENT_CUSTOM || !event->subclass_name || (switch_core_hash_find(_event_hash, event->subclass_name))) {
			send = 1;
		}
	}

	if (send) {
		if (_filters && _filters->headers) {
			switch_event_header_t *hp;
			const char *hval;

			send = 0;

			for (hp = _filters->headers; hp; hp = hp->next) {
				if ((hval = switch_event_get_header(event, hp->name))) {
					const char *comp_to = hp->value;
					int pos = 1, cmp = 0;

					while (comp_to && *comp_to) {
						if (*comp_to == '+') {
							pos = 1;
						} else if (*comp_to == '-') {
							pos = 0;
						} else if (*comp_to != ' ') {
							break;
						}
						comp_to++;
					}

					if (send && pos) {
						continue;
					}

					if (!comp_to) {
						continue;
					}

					if (*hp->value == '/') {
						switch_regex_t *re = NULL;
						int ovector[30];
						cmp = !!switch_regex_perform(hval, comp_to, &re, ovector, sizeof(ovector) / sizeof(ovector[0]));
						switch_regex_safe_free(re);
					} else {
						cmp = !strcasecmp(hval, comp_to);
					}

					if (cmp) {
						if (pos) {
							send = 1;
						} else {
							send = 0;
							break;
						}
					}
				}
			}
		}
	}

	switch_mutex_unlock(_mutex);

	if (send) {
		if (switch_event_dup(&clone, event) == SWITCH_STATUS_SUCCESS) {
			if (switch_queue_trypush(_event_queue, clone) != SWITCH_STATUS_SUCCESS) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "Lost event to JS EventHandler, you must read the events faster!\n");
				switch_event_destroy(&clone);
			}
		} else {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Memory Error!\n");
		}
	}
}

static const char *MARKER = "1";

void FSEventHandler::DoSubscribe(const v8::FunctionCallbackInfo<v8::Value>& info)
{
	int i, custom = 0;
	bool ret = false;

	for (i = 0; i < info.Length(); i++) {
		String::Utf8Value str(info[i]);
		switch_event_types_t etype;

		if (custom) {
			switch_mutex_lock(_mutex);
			switch_core_hash_insert(_event_hash, js_safe_str(*str), MARKER);
			switch_mutex_unlock(_mutex);
		} else if (switch_name_event(js_safe_str(*str), &etype) == SWITCH_STATUS_SUCCESS) {
			ret = true;

			if (etype == SWITCH_EVENT_ALL) {
				uint32_t x = 0;
				for (x = 0; x < SWITCH_EVENT_ALL; x++) {
					_event_list[x] = 1;
				}
			}

			if (etype <= SWITCH_EVENT_ALL) {
				_event_list[etype] = 1;
			}

			if (etype == SWITCH_EVENT_CUSTOM) {
				custom++;
			}
		}
	}
	
	info.GetReturnValue().Set(ret);
}

void *FSEventHandler::Construct(const v8::FunctionCallbackInfo<v8::Value>& info)
{
	FSEventHandler *obj = new FSEventHandler(info);
	obj->DoSubscribe(info);
	return obj;
}

JS_EVENTHANDLER_FUNCTION_IMPL(Subscribe)
{
	DoSubscribe(info);
}

JS_EVENTHANDLER_FUNCTION_IMPL(UnSubscribe)
{
	int i, custom = 0;
	bool ret = false;

	for (i = 0; i < info.Length(); i++) {
		String::Utf8Value str(info[i]);
		switch_event_types_t etype;

		if (custom) {
			switch_mutex_lock(_mutex);
			switch_core_hash_delete(_event_hash, js_safe_str(*str));
			switch_mutex_unlock(_mutex);
		} else if (switch_name_event(js_safe_str(*str), &etype) == SWITCH_STATUS_SUCCESS) {
			uint32_t x = 0;
			ret = true;

			if (etype == SWITCH_EVENT_CUSTOM) {
				custom++;
			} else if (etype == SWITCH_EVENT_ALL) {
				for (x = 0; x <= SWITCH_EVENT_ALL; x++) {
					_event_list[x] = 0;
				}
			} else {
				if (_event_list[SWITCH_EVENT_ALL]) {
					_event_list[SWITCH_EVENT_ALL] = 0;
					for (x = 0; x < SWITCH_EVENT_ALL; x++) {
						_event_list[x] = 1;
					}
				}
				_event_list[etype] = 0;
			}
		}
	}
	
	info.GetReturnValue().Set(ret);
}

JS_EVENTHANDLER_FUNCTION_IMPL(DeleteFilter)
{
	if (info.Length() < 1) {
		info.GetReturnValue().Set(false);
	} else {
		String::Utf8Value str(info[0]);
		const char *headerName = js_safe_str(*str);

		if (zstr(headerName)) {
			info.GetReturnValue().Set(false);
			return;
		}

		switch_mutex_lock(_mutex);

		if (!_filters) {
			switch_event_create_plain(&_filters, SWITCH_EVENT_CLONE);
		}

		if (!strcasecmp(headerName, "all")) {
			switch_event_destroy(&_filters);
			switch_event_create_plain(&_filters, SWITCH_EVENT_CLONE);
		} else {
			switch_event_del_header(_filters, headerName);
		}

		info.GetReturnValue().Set(true);

		switch_mutex_unlock(_mutex);
	}
}

JS_EVENTHANDLER_FUNCTION_IMPL(AddFilter)
{
	if (info.Length() < 2) {
		info.GetReturnValue().Set(false);
	} else {
		String::Utf8Value str1(info[0]);
		String::Utf8Value str2(info[1]);
		const char *headerName = js_safe_str(*str1);
		const char *headerVal = js_safe_str(*str2);

		if (zstr(headerName) || zstr(headerVal)) {
			info.GetReturnValue().Set(false);
			return;
		}

		switch_mutex_lock(_mutex);

		if (!_filters) {
			switch_event_create_plain(&_filters, SWITCH_EVENT_CLONE);
		}

		switch_event_add_header_string(_filters, SWITCH_STACK_BOTTOM, headerName, headerVal);

		info.GetReturnValue().Set(true);

		switch_mutex_unlock(_mutex);
	}
}

JS_EVENTHANDLER_FUNCTION_IMPL(GetEvent)
{
	void *pop = NULL;
	int timeout = 0;
	switch_event_t *pevent = NULL;

	if (info.Length() > 0 && !info[0].IsEmpty()) {
		timeout = info[0]->Int32Value();
	}

	if (timeout > 0) {
		if (switch_queue_pop_timeout(_event_queue, &pop, (switch_interval_time_t) timeout * 1000) == SWITCH_STATUS_SUCCESS && pop) {
			pevent = (switch_event_t *) pop;
		}
	} else {
		if (switch_queue_trypop(_event_queue, &pop) == SWITCH_STATUS_SUCCESS && pop) {
			pevent = (switch_event_t *) pop;
		}
	}

	if (pevent) {
		FSEvent *evt = new FSEvent(info);
		evt->SetEvent(pevent, 0);
		evt->RegisterInstance(info.GetIsolate(), "", true);
		info.GetReturnValue().Set(evt->GetJavaScriptObject());
	} else {
		info.GetReturnValue().Set(Null(info.GetIsolate()));
	}
}

JS_EVENTHANDLER_FUNCTION_IMPL(SendEvent)
{
	if (info.Length() == 0) {
		info.GetReturnValue().Set(false);
	} else {
		if (!info[0].IsEmpty() && info[0]->IsObject()) {
			FSEvent *evt = JSBase::GetInstance<FSEvent>(info[0]->ToObject());
			switch_event_t **event;

			if (!evt || !(event = evt->GetEvent())) {
				info.GetReturnValue().Set(false);
			} else {
				string session_uuid;

				if (info.Length() > 1) {
					if (!info[1].IsEmpty() && info[1]->IsObject()) {
						/* The second argument is a session object */
						FSSession *sess = JSBase::GetInstance<FSSession>(info[1]->ToObject());
						switch_core_session_t *tmp;

						if (sess && (tmp = sess->GetSession())) {
							session_uuid = switch_core_session_get_uuid(tmp);
						}
					} else {
						/* The second argument is a session uuid string */
						String::Utf8Value str(info[1]);
						session_uuid = js_safe_str(*str);
					}
				}

				if (session_uuid.length() > 0) {
					/* This is a session event */
					switch_core_session_t *session;
					switch_status_t status = SWITCH_STATUS_FALSE;

					if ((session = switch_core_session_locate(session_uuid.c_str()))) {
						if ((status = switch_core_session_queue_private_event(session, event, SWITCH_FALSE)) == SWITCH_STATUS_SUCCESS) {
							info.GetReturnValue().Set(true);
						} else {
							info.GetReturnValue().Set(false);
						}
						switch_core_session_rwunlock(session);
					} else {
						info.GetReturnValue().Set(false);
						switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Invalid session id [%s]\n", switch_str_nil(session_uuid.c_str()));
					}
				} else {
					/* "Normal" event */
					switch_event_fire(event);
				}
			}
		}
	}
}

JS_EVENTHANDLER_FUNCTION_IMPL(ExecuteApi)
{
	if (info.Length() > 0) {
		String::Utf8Value str(info[0]);
		const char *cmd = js_safe_str(*str);
		string arg;
		switch_stream_handle_t stream = { 0 };

		if (!strcasecmp(cmd, "jsapi")) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Possible recursive API Call is not allowed\n");
			info.GetReturnValue().Set(false);
			return;
		}

		if (info.Length() > 1) {
			String::Utf8Value str2(info[1]);
			arg = js_safe_str(*str2);
		}

		SWITCH_STANDARD_STREAM(stream);
		switch_api_execute(cmd, arg.c_str(), NULL, &stream);

		info.GetReturnValue().Set(String::NewFromUtf8(info.GetIsolate(), switch_str_nil((char *) stream.data)));
		switch_safe_free(stream.data);
	} else {
		info.GetReturnValue().Set(String::NewFromUtf8(info.GetIsolate(), "-ERR"));
	}
}

static void *SWITCH_THREAD_FUNC api_exec(switch_thread_t *thread, void *obj)
{
	api_command_struct_t *acs = (api_command_struct_t *) obj;
	switch_stream_handle_t stream = { 0 };
	char *reply, *freply = NULL;
	switch_status_t status;
	switch_event_t *event;

	if (!acs) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Internal error.\n");
		return NULL;
	}

	acs->ack = 1;

	SWITCH_STANDARD_STREAM(stream);

	status = switch_api_execute(acs->cmd, acs->arg, NULL, &stream);

	if (status == SWITCH_STATUS_SUCCESS) {
		reply = (char *)stream.data;
	} else {
		freply = switch_mprintf("-ERR %s Command not found!\n", acs->cmd);
		reply = freply;
	}

	if (!reply) {
		reply = (char *)"Command returned no output!";
	}

	if (switch_event_create(&event, SWITCH_EVENT_BACKGROUND_JOB) == SWITCH_STATUS_SUCCESS) {
		switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "Job-UUID", acs->uuid_str);
		switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "Job-Command", acs->cmd);
		if (acs->arg) {
			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "Job-Command-Arg", acs->arg);
		}
		switch_event_add_body(event, "%s", reply);
		switch_event_fire(&event);
	}

	switch_safe_free(stream.data);
	switch_safe_free(freply);

	switch_memory_pool_t *pool = acs->pool;
	if (acs->ack == -1) {
		int sanity = 2000;
		while (acs->ack == -1) {
			switch_cond_next();
			if (--sanity <= 0)
				break;
		}
	}

	acs = NULL;
	switch_core_destroy_memory_pool(&pool);
	pool = NULL;

	return NULL;
}

JS_EVENTHANDLER_FUNCTION_IMPL(ExecuteBgApi)
{
	string cmd;
	string arg;
	string jobuuid;
	api_command_struct_t *acs = NULL;
	switch_memory_pool_t *pool;
	switch_thread_t *thread;
	switch_threadattr_t *thd_attr = NULL;
	switch_uuid_t uuid;
	int sanity = 2000;

	if (info.Length() > 0) {
		String::Utf8Value str(info[0]);
		cmd = js_safe_str(*str);

		if (info.Length() > 1) {
			String::Utf8Value str2(info[1]);
			arg = js_safe_str(*str2);
		}

		if (info.Length() > 2) {
			String::Utf8Value str2(info[2]);
			jobuuid = js_safe_str(*str2);
		}
	} else {
		info.GetReturnValue().Set(false);
		return;
	}

	if (cmd.length() == 0) {
		info.GetReturnValue().Set(false);
		return;
	}

	switch_core_new_memory_pool(&pool);
	acs = (api_command_struct_t *)switch_core_alloc(pool, sizeof(*acs));
	switch_assert(acs);
	acs->pool = pool;

	acs->cmd = switch_core_strdup(acs->pool, cmd.c_str());

	if (arg.c_str()) {
		acs->arg = switch_core_strdup(acs->pool, arg.c_str());
	}

	switch_threadattr_create(&thd_attr, acs->pool);
	switch_threadattr_detach_set(thd_attr, 1);
	switch_threadattr_stacksize_set(thd_attr, SWITCH_THREAD_STACKSIZE);

	if (jobuuid.length() > 0) {
		switch_copy_string(acs->uuid_str, jobuuid.c_str(), sizeof(acs->uuid_str));
	} else {
		switch_uuid_get(&uuid);
		switch_uuid_format(acs->uuid_str, &uuid);
	}

	info.GetReturnValue().Set(String::NewFromUtf8(info.GetIsolate(), acs->uuid_str));

	switch_thread_create(&thread, thd_attr, api_exec, acs, acs->pool);

	while (!acs->ack) {
		switch_cond_next();
		if (--sanity <= 0)
			break;
	}

	if (acs->ack == -1) {
		acs->ack--;
	}
}

JS_EVENTHANDLER_FUNCTION_IMPL_STATIC(Destroy)
{
	JS_CHECK_SCRIPT_STATE();

	FSEventHandler *obj = JSBase::GetInstance<FSEventHandler>(info.Holder());

	if (obj) {
		delete obj;
		info.GetReturnValue().Set(true);
	} else {
		info.GetReturnValue().Set(false);
	}
}

JS_EVENTHANDLER_GET_PROPERTY_IMPL_STATIC(GetReadyProperty)
{
	JS_CHECK_SCRIPT_STATE();

	FSEventHandler *obj = JSBase::GetInstance<FSEventHandler>(info.Holder());

	if (obj) {
		info.GetReturnValue().Set(true);
	} else {
		info.GetReturnValue().Set(false);
	}
}

static const js_function_t eventhandler_methods[] = {
	{"subscribe", FSEventHandler::Subscribe},
	{"unSubscribe", FSEventHandler::UnSubscribe},
	{"addFilter", FSEventHandler::AddFilter},
	{"deleteFilter", FSEventHandler::DeleteFilter},
	{"getEvent", FSEventHandler::GetEvent},
	{"sendEvent", FSEventHandler::SendEvent},
	{"executeApi", FSEventHandler::ExecuteApi},
	{"executeBgApi", FSEventHandler::ExecuteBgApi},
	{"destroy", FSEventHandler::Destroy},
	{0}
};

static const js_property_t eventhandler_props[] = {
	{"ready", FSEventHandler::GetReadyProperty, JSBase::DefaultSetProperty},
	{0}
};

static const js_class_definition_t eventhandler_desc = {
	js_class_name,
	FSEventHandler::Construct,
	eventhandler_methods,
	eventhandler_props
};

static switch_status_t eventhandler_load(const v8::FunctionCallbackInfo<Value>& info)
{
	JSBase::Register(info.GetIsolate(), &eventhandler_desc);
	return SWITCH_STATUS_SUCCESS;
}

static const v8_mod_interface_t eventhandler_module_interface = {
	/*.name = */ js_class_name,
	/*.js_mod_load */ eventhandler_load
};

const v8_mod_interface_t *FSEventHandler::GetModuleInterface()
{
	return &eventhandler_module_interface;
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
