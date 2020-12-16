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
 * The Original Code is ported from FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
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
 * Andrey Volk <andywolk@gmail.com>
 *
 * mod_v8.cpp -- JavaScript FreeSWITCH module
 *
 */

/*
 * This module executes JavaScript using Google's V8 JavaScript engine.
 *
 * It extends the available JavaScript classes with the following FS related classes;
 * CoreDB		Adds features to access the core DB (SQLite) in FreeSWITCH. (on request only)
 * DBH			Database Handler. Makes use of connection pooling provided by FreeSWITCH. (on request only)
 * CURL			Adds some extra methods for CURL access. (on request only)
 * DTMF			Object that holds information about a DTMF event.
 * Event		Object that holds information about a FreeSWITCH event.
 * EventHandler	Features for handling FS events.
 * File			Class to reflect the Spidermonkey built-in class "File". Not yet implemented! (on request only)
 * FileIO		Simple class for basic file IO.
 * ODBC			Adds features to access any ODBC available database in the system. (on request only)
 * PCRE			Adds features to do regexp using the PCRE implementeation.
 * Request		Class for extra features during API call from FS (using 'jsapi' function). This class cannot be constructed from JS code!
 *				The Request class is only availble when started from 'jsapi' FS command, and only inside the predefined variable 'request'.
 * Session		Main FS class, includes all functions to handle a session.
 * Socket		Class for communicating over a TCP/IP socket. (on request only)
 * TeleTone		Class used to play tones to a FS channel. (on request only)
 * XML			XML parsing class, using the features from switch_xml. (on request only)
 *
 * Some of the classes above are available on request only, using the command [use('Class');] before using the class for the first time.
 *
 * It also adds quite a few global functions, directly available for the user (see fsglobal.cpp for the implementation).
 *
 * Depedning on how the script was started from FreeSWITCH, some variables might be defined already;
 * session   If the script is started from the dialplan, the variable 'session' holds the session for the current call.
 * request   If the script is started using 'jsapi' function, the variable 'request' is an instance of the Request class.
 * message   If the script is started as a chat application, the actual FreeSWITCH event will be available in the variable 'message'.
 *
 * All classes are implemented in a pair of hpp/cpp files, named after the class. For instance; class "File" is implemented in fsfile.cpp.
 *
 */

#include "mod_v8.h"
#include <fstream>

#ifdef V8_ENABLE_DEBUGGING
#include <v8-debug.h>
#endif

/* Global JavaScript functions */
#include "fsglobal.hpp"

/* Common JavaScript classes */
#include "fsrequest.hpp" /* Only loaded during 'jsapi' and 'jsjson' call */
#include "fspcre.hpp"
#include "fsevent.hpp"
#include "fssession.hpp"
#include "fsdtmf.hpp"
#include "fsfileio.hpp"

/* Optional JavaScript classes (loaded on demand) */
#include "fscoredb.hpp"
#include "fsdbh.hpp"
#include "fscurl.hpp"
#include "fsteletone.hpp"
#include "fssocket.hpp"
#include "fsodbc.hpp"
#include "fsxml.hpp"
#include "fsfile.hpp"
#include "fseventhandler.hpp"

#include <set>

using namespace std;
using namespace v8;

SWITCH_BEGIN_EXTERN_C

/* FreeSWITCH module load definitions */
SWITCH_MODULE_LOAD_FUNCTION(mod_v8_load);
SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_v8_shutdown);
SWITCH_MODULE_DEFINITION_EX(mod_v8, mod_v8_load, mod_v8_shutdown, NULL, SMODF_GLOBAL_SYMBOLS);

/* API interfaces */
static switch_api_interface_t *jsrun_interface = NULL;
static switch_api_interface_t *jsapi_interface = NULL;
static switch_api_interface_t *jsmon_interface = NULL;

/* Module manager for loadable modules */
module_manager_t module_manager = { 0 };

/* Global data for this module */
typedef struct {
	switch_memory_pool_t *pool;
	switch_mutex_t *event_mutex;
	switch_event_node_t *event_node;
	set<FSEventHandler *> *event_handlers;
	char *xml_handler;
#if defined(V8_MAJOR_VERSION) && V8_MAJOR_VERSION >=5
	v8::Platform *v8platform;
	switch_hash_t *compiled_script_hash;
	switch_mutex_t *compiled_script_hash_mutex;
	map<string, Isolate *> *task_manager;
	switch_mutex_t *task_manager_mutex;
	char *script_caching;
	switch_time_t cache_expires_seconds;
	bool performance_monitor;
	switch_mutex_t *mutex;
#endif
} mod_v8_global_t;

static mod_v8_global_t globals = { 0 };

#if defined(V8_MAJOR_VERSION) && V8_MAJOR_VERSION >=5
/* Struct to store cached script data */
typedef struct {
	std::shared_ptr<uint8_t> data;
	int length;
	switch_time_t compile_time;
} v8_compiled_script_cache_t;
#endif

/* Loadable module struct, used for external extension modules */
typedef struct {
	char *filename;
	void *lib;
	const v8_mod_interface_t *module_interface;
	v8_mod_init_t v8_mod_init;
} v8_loadable_module_t;

#ifdef V8_ENABLE_DEBUGGING
static bool debug_enable_callback = false;
static int debug_listen_port = 9999;
static bool debug_wait_for_connection = true;
static bool debug_manual_break = true;
#endif

static void v8_thread_launch(const char *text);
static void v8_event_handler(switch_event_t *event);
static switch_xml_t v8_fetch(const char *section, const char *tag_name, const char *key_name, const char *key_value, switch_event_t *params, void *user_data);

using namespace v8;

static switch_status_t v8_mod_init_built_in(const v8_mod_interface_t *mod_interface)
{
	switch_assert(mod_interface);

	switch_core_hash_insert(module_manager.load_hash, (char *) mod_interface->name, mod_interface);
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Successfully Loaded [%s]\n", mod_interface->name);

	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t v8_mod_load_file(const char *filename)
{
	v8_loadable_module_t *module = NULL;
	switch_dso_lib_t dso = NULL;
	switch_status_t status = SWITCH_STATUS_SUCCESS;
	switch_loadable_module_function_table_t *function_handle = NULL;
	v8_mod_init_t v8_mod_init = NULL;
	const v8_mod_interface_t *module_interface = NULL, *mp;
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

	function_handle = (switch_loadable_module_function_table_t *)switch_dso_data_sym(dso, "v8_mod_init", &derr);

	if (!function_handle || derr) {
		err = derr;
		goto err;
	}

	v8_mod_init = (v8_mod_init_t) (intptr_t) function_handle;

	if (v8_mod_init(&module_interface) != SWITCH_STATUS_SUCCESS) {
		err = "Module load routine returned an error";
		goto err;
	}

	if (!(module = (v8_loadable_module_t *) switch_core_permanent_alloc(sizeof(*module)))) {
		err = "Could not allocate memory\n";
	}

  err:

	if (err || !module) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Error Loading module %s\n**%s**\n", filename, switch_str_nil(err));
		switch_safe_free(derr);
		return SWITCH_STATUS_GENERR;
	}

	module->filename = switch_core_permanent_strdup(filename);
	module->v8_mod_init = v8_mod_init;
	module->module_interface = module_interface;

	module->lib = dso;

	if ((mp = module->module_interface)) {
		switch_core_hash_insert(module_manager.load_hash, (char *) mp->name, (void *) mp);
	}

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Successfully Loaded [%s]\n", module->filename);

	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t v8_load_module(const char *dir, const char *fname)
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

	return v8_mod_load_file(path);
}

SWITCH_END_EXTERN_C

static switch_status_t load_modules(void)
{
	const char *cf = "v8.conf";
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

	switch_core_new_memory_pool(&module_manager.pool);

	switch_core_hash_init(&module_manager.load_hash);

	if ((xml = switch_xml_open_cfg(cf, &cfg, NULL))) {
		switch_xml_t mods, ld;

		if ((mods = switch_xml_child(cfg, "modules"))) {
			for (ld = switch_xml_child(mods, "load"); ld; ld = ld->next) {
				const char *val = switch_xml_attr_soft(ld, "module");
				if (!zstr(val) && strchr(val, '.') && !strstr(val, ext) && !strstr(val, EXT)) {
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CONSOLE, "Invalid extension for %s\n", val);
					continue;
				}
				v8_load_module(SWITCH_GLOBAL_dirs.mod_dir, val);
				count++;
			}
		}
		switch_xml_free(xml);

	} else {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CONSOLE, "Open of %s failed\n", cf);
	}

	return SWITCH_STATUS_SUCCESS;
}

static void load_configuration(void)
{
	const char *cf = "v8.conf";
	switch_xml_t cfg, xml;

	if ((xml = switch_xml_open_cfg(cf, &cfg, NULL))) {
		switch_xml_t settings, param, hook;

		if ((settings = switch_xml_child(cfg, "settings"))) {
			for (param = switch_xml_child(settings, "param"); param; param = param->next) {
				char *var = (char *)switch_xml_attr_soft(param, "name");
				char *val = (char *)switch_xml_attr_soft(param, "value");

#if defined(V8_MAJOR_VERSION) && V8_MAJOR_VERSION >=5
				if (!strcmp(var, "script-caching")) {
					globals.script_caching = switch_core_strdup(globals.pool, val);
				} else if (!strcmp(var, "cache-expires-sec")) {
					int v = atoi(val);
					globals.cache_expires_seconds = (v > 0) ? v : 0;
				} else 
#endif
				if (!strcmp(var, "xml-handler-script")) {
					globals.xml_handler = switch_core_strdup(globals.pool, val);
				}
				else if (!strcmp(var, "xml-handler-bindings")) {
					if (!zstr(globals.xml_handler)) {
						switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "binding '%s' to '%s'\n", globals.xml_handler, val);
						switch_xml_bind_search_function(v8_fetch, switch_xml_parse_section_string(val), NULL);
					}
				}
				else if (!strcmp(var, "startup-script")) {
					if (val) {
						v8_thread_launch(val);
					}
				}
			}

			for (hook = switch_xml_child(settings, "hook"); hook; hook = hook->next) {
				char *event = (char *)switch_xml_attr_soft(hook, "event");
				char *subclass = (char *)switch_xml_attr_soft(hook, "subclass");
				char *script = (char *)switch_xml_attr_soft(hook, "script");
				switch_event_types_t evtype;

				if (!zstr(script)) {
					script = switch_core_strdup(globals.pool, script);
				}

				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "hook params: '%s' | '%s' | '%s'\n", event, subclass, script);

				if (switch_name_event(event, &evtype) == SWITCH_STATUS_SUCCESS) {
					if (!zstr(script)) {
						if (switch_event_bind(modname, evtype, !zstr(subclass) ? subclass : SWITCH_EVENT_SUBCLASS_ANY,
							v8_event_handler, script) == SWITCH_STATUS_SUCCESS) {
							switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "event handler for '%s' set to '%s'\n", switch_event_name(evtype), script);
						}
						else {
							switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "cannot set event handler: unsuccessful bind\n");
						}
					}
					else {
						switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "cannot set event handler: no script name for event type '%s'\n", event);
					}
				}
				else {
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "cannot set event handler: unknown event type '%s'\n", event);
				}
			}
		}

		switch_xml_free(xml);

	}
	else {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CONSOLE, "Open of %s failed\n", cf);
	}
}

static int env_init(JSMain *js)
{
	/* Init all "global" functions first */
	const js_function_t *fs_proc = FSGlobal::GetFunctionDefinitions();
	for (int i = 0; fs_proc[i].name && fs_proc[i].func; i++) {
		js->AddJSExtenderFunction(fs_proc[i].func, fs_proc[i].name);
	}

	/* Init all basic classes made available from FreeSWITCH */
	js->AddJSExtenderClass(FSSession::GetClassDefinition());
	js->AddJSExtenderClass(FSFileIO::GetClassDefinition());
	js->AddJSExtenderClass(FSEvent::GetClassDefinition());
	js->AddJSExtenderClass(FSDTMF::GetClassDefinition());
	js->AddJSExtenderClass(FSPCRE::GetClassDefinition());
	/* To add a class that will always be available inside JS, add the definition here */

	return 1;
}

static void v8_error(Isolate* isolate, TryCatch* try_catch)
{
	HandleScope handle_scope(isolate);
	String::Utf8Value exception(try_catch->Exception());
	const char *exception_string = js_safe_str(*exception);
	Handle<Message> message = try_catch->Message();
	const char *msg = "";
	string filename = __FILE__;
	int line = __LINE__;
	string text = "";
	JSMain *js = JSMain::GetScriptInstanceFromIsolate(isolate);

	if (js && js->GetForcedTermination()) {
		js->ResetForcedTermination();
		switch_log_printf(SWITCH_CHANNEL_ID_LOG, js->GetForcedTerminationScriptFile(), modname, js->GetForcedTerminationLineNumber(), NULL, SWITCH_LOG_NOTICE, "Script exited with info [%s]\n", js->GetForcedTerminationMessage());
		return;
	}

	if (!message.IsEmpty()) {
		String::Utf8Value fname(message->GetScriptResourceName());

		if (*fname) {
			filename = *fname;
		}

		line = message->GetLineNumber();
		msg = exception_string;

		String::Utf8Value sourceline(message->GetSourceLine());
		if (*sourceline) {
			text = *sourceline;
		}
	} else {
		msg = exception_string;
	}

	if (!msg) {
		msg = "";
	}

	if (text.length() > 0) {
		switch_log_printf(SWITCH_CHANNEL_ID_LOG, filename.c_str(), modname, line, NULL, SWITCH_LOG_ERROR, "Exception: %s (near: \"%s\")\n", msg, text.c_str());
	} else {
		switch_log_printf(SWITCH_CHANNEL_ID_LOG, filename.c_str(), modname, line, NULL, SWITCH_LOG_ERROR, "Exception: %s\n", msg);
	}
}

static char *v8_get_script_path(const char *script_file)
{
	const char *p;
	char *ret = NULL;
	const char delims[] = "/\\";
	const char *i;

	if (script_file) {
		for (i = delims; *i; i++) {
			if ((p = strrchr(script_file, *i))) {
				js_strdup(ret, script_file);
				*(ret + (p - script_file)) = '\0';
				return ret;
			}
		}
		js_strdup(ret, ".");
		return ret;
	} else {
		return NULL;
	}
}

#if defined(V8_MAJOR_VERSION) && V8_MAJOR_VERSION >=5
void perf_log(const char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);

	switch_mutex_lock(globals.mutex);
	if (globals.performance_monitor) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, fmt, ap);
	}
	switch_mutex_unlock(globals.mutex);

	va_end(ap);
}

template< typename T >
struct array_deleter
{
	void operator ()(T const * p)
	{
		delete[] p;
	}
};

static void destructor(void *ptr)
{
	delete (v8_compiled_script_cache_t*)ptr;
}

void LoadScript(MaybeLocal<v8::Script> *v8_script, Isolate *isolate, const char *script_data, const char *script_file)
{
	switch_time_t start = switch_time_now();

	ScriptCompiler::CachedData *cached_data = 0;
	v8_compiled_script_cache_t *stored_compiled_script_cache = NULL;
	ScriptCompiler::CompileOptions options;

	/*
		Do not cache if the caching is disabled
		Do not cache inline scripts
	*/
	if (!switch_true(globals.script_caching) || !strcasecmp(script_file, "inline") || zstr(script_file)) {
		options = ScriptCompiler::kNoCompileOptions;
		perf_log("Javascript caching is disabled.\n", script_file);
	} else {
		options = ScriptCompiler::kConsumeCodeCache;

		switch_mutex_lock(globals.compiled_script_hash_mutex);

		void *hash_found = switch_core_hash_find(globals.compiled_script_hash, script_file);
		if (hash_found)
		{
			stored_compiled_script_cache = new v8_compiled_script_cache_t;
			*stored_compiled_script_cache = *((v8_compiled_script_cache_t *)hash_found);
		}

		switch_mutex_unlock(globals.compiled_script_hash_mutex);

		if (stored_compiled_script_cache)
		{
			switch_time_t time_left_since_compile_sec = (switch_time_now() - stored_compiled_script_cache->compile_time) / 1000000;
			if (time_left_since_compile_sec <= globals.cache_expires_seconds || globals.cache_expires_seconds == 0) {
				cached_data = new ScriptCompiler::CachedData(stored_compiled_script_cache->data.get(), stored_compiled_script_cache->length, ScriptCompiler::CachedData::BufferNotOwned);
			} else {
				perf_log("Javascript ['%s'] cache expired.\n", script_file);
				switch_core_hash_delete_locked(globals.compiled_script_hash, script_file, globals.compiled_script_hash_mutex);
			}

		}
		
		if (!cached_data) options = ScriptCompiler::kProduceCodeCache;

	}

	ScriptCompiler::Source source(String::NewFromUtf8(isolate, script_data), cached_data);
	*v8_script = ScriptCompiler::Compile(isolate->GetCurrentContext(), &source, options);	

	if (!v8_script->IsEmpty()) {

		if (options == ScriptCompiler::kProduceCodeCache && !source.GetCachedData()->rejected) {
			int length = source.GetCachedData()->length;
			uint8_t* raw_cached_data = new uint8_t[length];
			v8_compiled_script_cache_t *compiled_script_cache = new v8_compiled_script_cache_t;
			memcpy(raw_cached_data, source.GetCachedData()->data, static_cast<size_t>(length));
			compiled_script_cache->data.reset(raw_cached_data, array_deleter<uint8_t>());
			compiled_script_cache->length = length;
			compiled_script_cache->compile_time = switch_time_now();

			switch_mutex_lock(globals.compiled_script_hash_mutex);
			switch_core_hash_insert_destructor(globals.compiled_script_hash, script_file, compiled_script_cache, destructor);
			switch_mutex_unlock(globals.compiled_script_hash_mutex);
			
			perf_log("Javascript ['%s'] cache was produced.\n", script_file);

		} else if (options == ScriptCompiler::kConsumeCodeCache) {

			if (source.GetCachedData()->rejected) {
				perf_log("Javascript ['%s'] cache was rejected.\n", script_file);
				switch_core_hash_delete_locked(globals.compiled_script_hash, script_file, globals.compiled_script_hash_mutex);
			} else {
				perf_log("Javascript ['%s'] execution using cache.\n", script_file);
			}

		}
	}

	if (stored_compiled_script_cache)
		delete stored_compiled_script_cache;

	switch_time_t end = switch_time_now();
	perf_log("Javascript ['%s'] loaded in %u microseconds.\n", script_file, (end - start));

}
#endif

static int v8_parse_and_execute(switch_core_session_t *session, const char *input_code, switch_stream_handle_t *api_stream, v8_event_t *v8_event, v8_xml_handler_t* xml_handler)
{
	string res;
	JSMain *js;
	Isolate *isolate;
	char *arg, *argv[512];
	int argc = 0;
	unsigned int flags = 0;
	char *path = NULL;
	string result_string;
	int result = 0;

	if (zstr(input_code)) {
		return -1;
	}

	js = new JSMain();
	isolate = js->GetIsolate();

	env_init(js);

	/* Try to read lock the session first */
	if (session) {
		if (switch_core_session_read_lock_hangup(session) != SWITCH_STATUS_SUCCESS) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Read Lock Failure.\n");
			session = NULL;
		}
	}

	/* Execute the actual script */
	//isolate->Enter();
	{
		Locker lock(isolate);
		Isolate::Scope iscope(isolate);
		{
			const char *script;

			// Create a stack-allocated handle scope.
			HandleScope scope(isolate);

			// Store our object internally
			isolate->SetData(ISOLATE_DATA_OBJECT, js);

			// Set isolate related data.
			switch_uuid_t task_id;
			switch_uuid_get(&task_id);
			char str_task_id[SWITCH_UUID_FORMATTED_LENGTH + 1];
			switch_uuid_format(str_task_id, &task_id);

			js_isolate_private_data_t *private_data = new js_isolate_private_data_t();
			private_data->str_task_id = str_task_id;
			private_data->input_code = input_code;
			private_data->start_time = switch_time_now();

			// Store private data internally
			isolate->SetData(ISOLATE_DATA_PRIVATE, private_data);

			// Add isolate to the task manager 
			switch_mutex_lock(globals.task_manager_mutex);
			(*globals.task_manager)[str_task_id] = isolate;
			switch_mutex_unlock(globals.task_manager_mutex);

			// New global template
			Handle<ObjectTemplate> global = ObjectTemplate::New(isolate);

			if (global.IsEmpty()) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Failed to create JS global object template\n");
			} else {
				/* Add all global functions */
				for (size_t i = 0; i < js->GetExtenderFunctions().size(); i++) {
					js_function_t *proc = js->GetExtenderFunctions()[i];
					global->Set(String::NewFromUtf8(isolate, proc->name), FunctionTemplate::New(isolate, proc->func));
				}

				// Create a new context.
				Local<Context> context = Context::New(isolate, NULL, global);

				if (context.IsEmpty()) {
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Failed to create JS context\n");
				} else {
					// Enter the created context for compiling and running the script.
					Context::Scope context_scope(context);

#ifdef V8_ENABLE_DEBUGGING
					Persistent<Context> *debug_context = new Persistent<Context>();
					isolate->SetData(ISOLATE_DATA_DEBUG, debug_context);
					debug_context->Reset(isolate, context);

					//v8::Locker lck(isolate);

					if (debug_enable_callback) {
						Debug::SetDebugMessageDispatchHandler(V8DispatchDebugMessages, true);
					}

					if (debug_listen_port > 0) {
						char *name = switch_mprintf("mod_v8-%d", (int)switch_thread_self());
						Debug::EnableAgent(name, debug_listen_port, debug_wait_for_connection);
						switch_safe_free(name);
					}
#endif

					/* Register all plugin classes */
					for (size_t i = 0; i < js->GetExtenderClasses().size(); i++) {
						JSBase::Register(isolate, js->GetExtenderClasses()[i]);
					}

					/* Register all instances of specific plugin classes */
					for (size_t i = 0; i < js->GetExtenderInstances().size(); i++) {
						registered_instance_t *inst = js->GetExtenderInstances()[i];
						inst->obj->RegisterInstance(isolate, inst->name, inst->auto_destroy);
					}

					/* Emaculent conception of session object into the script if one is available */
					if (session) {
						FSSession *obj = new FSSession(js);
						obj->Init(session, flags);
						obj->RegisterInstance(isolate, "session", true);
					} else {
						/* Add a session object as a boolean instead, just to make it safe to check if it exists as expected */
						context->Global()->Set(String::NewFromUtf8(isolate, "session"), Boolean::New(isolate, false));
					}

					if (v8_event) {
						if (v8_event->event && v8_event->var_name)
						    FSEvent::New(v8_event->event, v8_event->var_name, js);
					}

					if (api_stream) {
						/* The JS class "Request" is only needed when a api_stream object is passed here */
						JSBase::Register(isolate, FSRequest::GetClassDefinition());

						FSRequest *ptr = new FSRequest(js);
						ptr->Init(input_code, api_stream);
						ptr->RegisterInstance(isolate, "request", true);
					}

					if (xml_handler)
					{
						/* Add xml handler global variables */

						Handle<Array> XML_REQUEST = Array::New(isolate, 4);

						XML_REQUEST->Set(String::NewFromUtf8(isolate, "key_name"), String::NewFromUtf8(isolate, js_safe_str(xml_handler->key_name)));
						XML_REQUEST->Set(String::NewFromUtf8(isolate, "key_value"), String::NewFromUtf8(isolate, js_safe_str(xml_handler->key_value)));
						XML_REQUEST->Set(String::NewFromUtf8(isolate, "section"), String::NewFromUtf8(isolate, js_safe_str(xml_handler->section)));
						XML_REQUEST->Set(String::NewFromUtf8(isolate, "tag_name"), String::NewFromUtf8(isolate, js_safe_str(xml_handler->tag_name)));

						context->Global()->Set(String::NewFromUtf8(isolate, "XML_REQUEST"), XML_REQUEST);

						if (xml_handler->params) {
							FSEvent::New(xml_handler->params, "params", js);
						}
					}

					script = input_code;

					if (*script != '~') {
						if ((arg = (char *)strchr(script, ' '))) {
							*arg++ = '\0';
							argc = switch_separate_string(arg, ' ', argv, (sizeof(argv) / sizeof(argv[0])));
						}

						// Add arguments before running script.
						Local<Array> arguments = Array::New(isolate, argc);
						for (int y = 0; y < argc; y++) {
							arguments->Set(Integer::New(isolate, y), String::NewFromUtf8(isolate, argv[y]));
						}
						context->Global()->Set(String::NewFromUtf8(isolate, "argv"), arguments);
						context->Global()->Set(String::NewFromUtf8(isolate, "argc"), Integer::New(isolate, argc));
					}

					const char *script_data = NULL;
					const char *script_file = NULL;
					string s;

					if (*script == '~') {
						script_data = script + 1;
						script_file = "inline";
					} else {
						const char *script_name = NULL;

						if (switch_is_file_path(script)) {
							script_name = script;
						} else if ((path = switch_mprintf("%s%s%s", SWITCH_GLOBAL_dirs.script_dir, SWITCH_PATH_SEPARATOR, script))) {
							script_name = path;
						}

						if (script_name) {
							if (JSMain::FileExists(script_name)) {
								s = JSMain::LoadFileToString(script_name);
								script_data = s.c_str();
								script_file = script_name;
							} else {
								switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Cannot Open File: %s\n", script_name);
							}
						}
					}

					if (!script_data) {
						switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "No script to execute!\n");
					} else {
						/* Store our base directory in variable 'scriptPath' */
						char *scriptPath = v8_get_script_path(script_file);
						if (scriptPath) {
							context->Global()->Set(String::NewFromUtf8(isolate, "scriptPath"), String::NewFromUtf8(isolate, scriptPath));
							switch_safe_free(scriptPath);
						}

						TryCatch try_catch(isolate);

						// Compile the source code.
#if defined(V8_MAJOR_VERSION) && V8_MAJOR_VERSION >=5
						switch_time_t start = switch_time_now();
						MaybeLocal<v8::Script> v8_script;
						LoadScript(&v8_script, isolate, script_data, script_file);
#else
						// Create a string containing the JavaScript source code.
						Handle<String> source = String::NewFromUtf8(isolate, script_data);
						Handle<Script> v8_script = Script::Compile(source, Local<Value>::New(isolate, String::NewFromUtf8(isolate, script_file)));
#endif

						if (try_catch.HasCaught()) {
							v8_error(isolate, &try_catch);
						} else {
							// Run the script
#ifdef V8_ENABLE_DEBUGGING
							/* Break before we actually start executing the script */
							if (debug_manual_break) {
								Debug::DebugBreak(isolate);
							}
#endif

#if defined(V8_MAJOR_VERSION) && V8_MAJOR_VERSION >=5
							Handle<Value> script_result;

							if (!v8_script.IsEmpty()) {
								script_result = v8_script.ToLocalChecked()->Run();
							}

							switch_mutex_lock(globals.mutex);
							if (globals.performance_monitor) {
								switch_time_t end = switch_time_now();
								unsigned int delay = (end - start);
								switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "Javascript execution time: %u microseconds\n", delay);
							}
							switch_mutex_unlock(globals.mutex);
#else
							Handle<Value> result = v8_script->Run();
#endif
							if (try_catch.HasCaught()) {
								v8_error(isolate, &try_catch);
							} else {
								if (js->GetForcedTermination()) {
									js->ResetForcedTermination();
									switch_log_printf(SWITCH_CHANNEL_ID_LOG, js->GetForcedTerminationScriptFile(), modname, js->GetForcedTerminationLineNumber(), NULL, SWITCH_LOG_NOTICE, "Script exited with info [%s]\n", js->GetForcedTerminationMessage());
								}

								if (!script_result.IsEmpty()) {
									// Return result as string
									String::Utf8Value ascii(script_result);
									if (*ascii) {
										res = *ascii;
									}
								}

								if (xml_handler)
								{
									Local<Value> value = context->Global()->Get(String::NewFromUtf8(isolate, "XML_STRING"));
									String::Utf8Value str(value);
									if (strcmp(js_safe_str(*str), "undefined"))
									{
										xml_handler->XML_STRING = strdup(js_safe_str(*str));
									}
								}
							}
						}

#ifndef V8_FORCE_GC_AFTER_EXECUTION
						/* Clear up all destroyable C++ instances */
						js->DisposeActiveInstances();
#endif
					}
#ifdef V8_ENABLE_DEBUGGING
					isolate->SetData(ISOLATE_DATA_DEBUG, NULL);
					if (debug_listen_port > 0) {
						Debug::DisableAgent();
					}
					debug_context->Reset();
					delete debug_context;
#endif
				}
			}

			// Remove isolate from the task manager
			switch_mutex_lock(globals.task_manager_mutex);
			globals.task_manager->erase(str_task_id);
			switch_mutex_unlock(globals.task_manager_mutex);
			
			isolate->SetData(ISOLATE_DATA_PRIVATE, NULL);
			isolate->SetData(ISOLATE_DATA_OBJECT, NULL);

			delete private_data;
		}

#ifdef V8_FORCE_GC_AFTER_EXECUTION
		/* Force GC to run. This should not be needed, but is left here for reference */
		V8::ContextDisposedNotification();
		while (!V8::IdleNotification()) {}
		js->DisposeActiveInstances();
#endif
	}
	//isolate->Exit();

	if (res.length() == 0) {
		result = -1;
	} else {
		result = 0;

		if (strcasecmp(res.c_str(), "undefined")) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "Javascript result: [%s]\n", res.c_str());
		}
	}

	switch_safe_free(path);
	delete js;

	return result;
}

static switch_xml_t v8_fetch(const char *section,
	const char *tag_name, const char *key_name, const char *key_value, switch_event_t *params, void *user_data)
{
	switch_xml_t xml = NULL;
	char *mycmd = NULL;

	if (!zstr(globals.xml_handler)) {

		mycmd = strdup(globals.xml_handler);
		switch_assert(mycmd);

		v8_xml_handler_t xml_handler;
		xml_handler.section = section;
		xml_handler.tag_name = tag_name;
		xml_handler.key_name = key_name;
		xml_handler.key_value = key_value;
		xml_handler.params = params;
		xml_handler.user_data = user_data;
		xml_handler.XML_STRING = NULL; //Init as NULL. A script's global var XML_STRING will be duplicated to it!

		v8_parse_and_execute(NULL, mycmd, NULL, NULL, &xml_handler);

		if (xml_handler.XML_STRING) {
			if (zstr(xml_handler.XML_STRING)) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "No Result\n");
			}
			else if (!(xml = switch_xml_parse_str_dynamic(xml_handler.XML_STRING, SWITCH_TRUE))) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Error Parsing XML Result!\n");
			}
		}

		// Don't forget to free XML_STRING
		switch_safe_free(xml_handler.XML_STRING);

	}

	switch_safe_free(mycmd);

	return xml;
}

static void v8_event_handler(switch_event_t *event)
{
	char *script = NULL;

	if (event->bind_user_data) {
		script = strdup((char *)event->bind_user_data);
	}

	v8_event_t v8_event;

	v8_event.event = event;
	v8_event.var_name = "event";

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "v8 event hook: execute '%s'\n", (char *)script);
	v8_parse_and_execute(NULL, script, NULL, &v8_event, NULL);

	switch_safe_free(script);
}

SWITCH_BEGIN_EXTERN_C

SWITCH_STANDARD_APP(v8_dp_function)
{
	v8_parse_and_execute(session, data, NULL, NULL, NULL);
}

SWITCH_STANDARD_CHAT_APP(v8_chat_function)
{
	v8_event_t v8_event;

	v8_event.event = message;
	v8_event.var_name = "message";

	v8_parse_and_execute(NULL, data, NULL, &v8_event, NULL);

	return SWITCH_STATUS_SUCCESS;
}

SWITCH_END_EXTERN_C

typedef struct {
	switch_memory_pool_t *pool;
	char *code;
} v8_task_t;

static void *SWITCH_THREAD_FUNC v8_thread_run(switch_thread_t *thread, void *obj)
{
	v8_task_t *task = (v8_task_t *) obj;
	switch_memory_pool_t *pool;

	v8_parse_and_execute(NULL, task->code, NULL, NULL, NULL);

	if ((pool = task->pool)) {
		switch_core_destroy_memory_pool(&pool);
	}

	return NULL;
}

static void v8_thread_launch(const char *text)
{
	switch_thread_t *thread;
	switch_threadattr_t *thd_attr = NULL;
	v8_task_t *task;
	switch_memory_pool_t *pool;

	if (zstr(text)) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "missing required input!\n");
		return;
	}

	if (switch_core_new_memory_pool(&pool) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "OH OH no pool\n");
		return;
	}

	task = (v8_task_t *)switch_core_alloc(pool, sizeof(*task));
	task->pool = pool;
	task->code = switch_core_strdup(pool, text);

	switch_threadattr_create(&thd_attr, pool);
	switch_threadattr_detach_set(thd_attr, 1);
	switch_threadattr_stacksize_set(thd_attr, SWITCH_THREAD_STACKSIZE);
	switch_thread_create(&thread, thd_attr, v8_thread_run, task, pool);
}

void v8_add_event_handler(void *event_handler)
{
	FSEventHandler *eh = static_cast<FSEventHandler *>(event_handler);

	if (eh) {
		switch_mutex_lock(globals.event_mutex);
		globals.event_handlers->insert(eh);
		switch_mutex_unlock(globals.event_mutex);
	}
}

void v8_remove_event_handler(void *event_handler)
{
	FSEventHandler *eh = static_cast<FSEventHandler *>(event_handler);

	if (eh) {
		switch_mutex_lock(globals.event_mutex);

		set<FSEventHandler *>::iterator it = globals.event_handlers->find(eh);

		if (it != globals.event_handlers->end()) {
			globals.event_handlers->erase(it);
		}

		switch_mutex_unlock(globals.event_mutex);
	}
}

SWITCH_BEGIN_EXTERN_C

static void event_handler(switch_event_t *event)
{
	if (event) {
		switch_mutex_lock(globals.event_mutex);

		set<FSEventHandler *>::iterator it;

		for (it = globals.event_handlers->begin(); it != globals.event_handlers->end(); ++it) {
			if (*it) {
				(*it)->QueueEvent(event);
			}
		}

		switch_mutex_unlock(globals.event_mutex);
	}
}

SWITCH_STANDARD_API(jsapi_function)
{
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

	v8_parse_and_execute(session, (char *) cmd, stream, NULL, NULL);

	return SWITCH_STATUS_SUCCESS;
}


SWITCH_STANDARD_JSON_API(json_function)
{
	char *json_text = NULL;
	cJSON *path = NULL, *data = NULL;
	switch_stream_handle_t stream = { 0 };

	if ((data = cJSON_GetObjectItem(json, "data"))) {
		path = cJSON_GetObjectItem(data, "path");
	}

	if (!(path && data)) {
		goto end;
	}

	SWITCH_STANDARD_STREAM(stream);

	json_text = cJSON_PrintUnformatted(data);
	switch_event_create(&stream.param_event, SWITCH_EVENT_REQUEST_PARAMS);
	switch_event_add_header_string(stream.param_event, SWITCH_STACK_BOTTOM, "JSON", json_text);
	switch_safe_free(json_text);

	v8_parse_and_execute(session, (char *) path->valuestring, &stream, NULL, NULL);

	*json_reply = cJSON_Parse((char *)stream.data);

 end:

	if (!*json_reply) {
		*json_reply = cJSON_CreateObject();
		cJSON_AddItemToObject(*json_reply, "error", cJSON_CreateString("parse error in return val or invalid data supplied"));
	}

	switch_event_destroy(&stream.param_event);
	switch_safe_free(stream.data);

	return SWITCH_STATUS_SUCCESS;
}

SWITCH_STANDARD_API(launch_async)
{
	if (zstr(cmd)) {
		stream->write_function(stream, "USAGE: %s\n", jsrun_interface->syntax);
		return SWITCH_STATUS_SUCCESS;
	}

	v8_thread_launch(cmd);
	stream->write_function(stream, "+OK\n");
	return SWITCH_STATUS_SUCCESS;
}

SWITCH_STANDARD_API(jsmon_function)
{
	if (zstr(cmd)) {
		stream->write_function(stream, "USAGE: %s\n", jsmon_interface->syntax);
		return SWITCH_STATUS_SUCCESS;
	}

	if (!strcasecmp(cmd, "on")) {
		switch_mutex_lock(globals.mutex);
		globals.performance_monitor = true;
		switch_mutex_unlock(globals.mutex);
		stream->write_function(stream, "Performance monitor has been enabled.\n", jsmon_interface->syntax);
	} else if (!strcasecmp(cmd, "off")) {
		switch_mutex_lock(globals.mutex);
		globals.performance_monitor = false;
		switch_mutex_unlock(globals.mutex);
		stream->write_function(stream, "Performance monitor has been disabled.\n", jsmon_interface->syntax);
	} else {
		stream->write_function(stream, "USAGE: %s\n", jsmon_interface->syntax);
		return SWITCH_STATUS_SUCCESS;
	}
	
	stream->write_function(stream, "+OK\n");
	return SWITCH_STATUS_SUCCESS;
}

SWITCH_STANDARD_API(kill_function)
{
	if (!zstr(cmd)) {
		switch_mutex_lock(globals.task_manager_mutex);

		auto isolate_it = globals.task_manager->find(cmd);
		if (isolate_it != globals.task_manager->end()) {
			Isolate * isolate = isolate_it->second;
			JSMain *js = JSMain::GetScriptInstanceFromIsolate(isolate);
			if (js)
				js->ExitScript(isolate, "Script termination requested by jskill API.", true);
		}

		switch_mutex_unlock(globals.task_manager_mutex);

		stream->write_function(stream, "+OK\n");
	}
	else {
		stream->write_function(stream, "false");
	}

	return SWITCH_STATUS_SUCCESS;
}

inline static void stream_write_safe_d(switch_stream_handle_t *stream, const char *str) {
	if (!str) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Memory Error!\n");
		stream->write_function(stream, "-ERR Memory Error!\n");
	}
	else {
		stream->write_function(stream, "%s", str);
	}
}
#define stream_write_safe(output_text) stream_write_safe_d(stream, output_text)

SWITCH_STANDARD_API(process_status_function)
{
	char *mydata = NULL, *argv[3] = { 0 };
	const char *as = NULL, *delim = ",";
	char *output_text = NULL;
	cJSON *json = NULL, *row;
	switch_xml_t xml = NULL, xml_row, xml_field;
	int rows = 0, f_off = 0, count = 0;
	char tmp_str[50];
	std::vector<js_isolate_private_data_t> tasks;

	if (cmd && *cmd && (mydata = strdup(cmd))) {
		switch_separate_string(mydata, ' ', argv, (sizeof(argv) / sizeof(argv[0])));
		if (argv[1] && !strcasecmp(argv[0], "as")) {
			as = argv[1];
			if (!strcasecmp(as, "csv")) {
				if (argv[2]) delim = argv[2];
			}
		}
	}

	if (!as) {
		as = "plain";
	}

	if (!strcasecmp(as, "json")) {
		if (!(json = cJSON_CreateArray())) {
			goto end;
		}
	} else if (!strcasecmp(as, "xml")) {
		if (!(xml = switch_xml_new("result"))) {
			goto end;
		}
	} else if (!strcasecmp(as, "delim") || !strcasecmp(as, "csv")) {
		stream->write_function(stream, "%s%s", "task_id", delim);
		stream->write_function(stream, "%s%s", "input_code", delim);
		stream->write_function(stream, "%s%s", "execution_time", delim);

		stream->write_function(stream, "%s%s", "total_physical_size", delim);
		stream->write_function(stream, "%s%s", "total_heap_size_executable", delim);
		stream->write_function(stream, "%s%s", "total_heap_size", delim);
		stream->write_function(stream, "%s%s", "used_heap_size", delim);
		stream->write_function(stream, "%s%s", "heap_size_limit", delim);
		stream->write_function(stream, "%s%s", "malloced_memory", delim);
		stream->write_function(stream, "%s%s", "peak_malloced_memory", "\n");		
	} else {
		stream->write_function(stream, "JavaScript process status.\n");
	}
	
	switch_mutex_lock(globals.task_manager_mutex);

	for (auto isolate_pair : *globals.task_manager) {
		Isolate *isolate = isolate_pair.second;

		js_isolate_private_data_t *isolate_private_data = (js_isolate_private_data_t*)isolate->GetData(ISOLATE_DATA_PRIVATE);
		js_isolate_private_data_t private_data = *isolate_private_data;

		isolate->GetHeapStatistics(&private_data.stats);

		tasks.push_back(private_data);
	}

	switch_mutex_unlock(globals.task_manager_mutex);

	for (auto isolate_private_data : tasks) {
		count++;

		js_isolate_private_data_t *private_data = (js_isolate_private_data_t *)&(isolate_private_data);

		switch_time_t end = switch_time_now();
		unsigned int delay = (end - private_data->start_time) / 1000;

		if (!strcasecmp(as, "plain")) {

			stream->write_function(stream, "\nTask id: %s\n", private_data->str_task_id.c_str());
			stream->write_function(stream, "input_code: %s\n", (private_data->input_code[0] == '~' ? "inline" : private_data->input_code.c_str()));
			stream->write_function(stream, "execution_time: %u ms\n", delay);

			stream->write_function(stream, "total_physical_size: %u\n", private_data->stats.total_physical_size());
			stream->write_function(stream, "total_heap_size_executable: %u\n", private_data->stats.total_heap_size_executable());
			stream->write_function(stream, "total_heap_size: %u\n", private_data->stats.total_heap_size());
			stream->write_function(stream, "used_heap_size: %u\n", private_data->stats.used_heap_size());
			stream->write_function(stream, "heap_size_limit: %u\n", private_data->stats.heap_size_limit());
			stream->write_function(stream, "malloced_memory: %u\n", private_data->stats.malloced_memory());
			stream->write_function(stream, "peak_malloced_memory: %u\n", private_data->stats.peak_malloced_memory());
		} else if (!strcasecmp(as, "json")) {
			if (!(row = cJSON_CreateObject())) {				
				goto end;
			}

			cJSON_AddItemToArray(json, row);

			cJSON_AddItemToObject(row, "task_id", cJSON_CreateString(private_data->str_task_id.c_str()));
			cJSON_AddItemToObject(row, "input_code", cJSON_CreateString((private_data->input_code[0] == '~' ? "inline" : private_data->input_code.c_str())));
			cJSON_AddItemToObject(row, "execution_time", cJSON_CreateNumber(delay));

			cJSON_AddItemToObject(row, "total_physical_size", cJSON_CreateNumber(private_data->stats.total_physical_size()));
			cJSON_AddItemToObject(row, "total_heap_size_executable", cJSON_CreateNumber(private_data->stats.total_heap_size_executable()));
			cJSON_AddItemToObject(row, "total_heap_size", cJSON_CreateNumber(private_data->stats.total_heap_size()));
			cJSON_AddItemToObject(row, "used_heap_size", cJSON_CreateNumber(private_data->stats.used_heap_size()));
			cJSON_AddItemToObject(row, "heap_size_limit", cJSON_CreateNumber(private_data->stats.heap_size_limit()));
			cJSON_AddItemToObject(row, "malloced_memory", cJSON_CreateNumber(private_data->stats.malloced_memory()));
			cJSON_AddItemToObject(row, "peak_malloced_memory", cJSON_CreateNumber(private_data->stats.peak_malloced_memory()));
		} else if (!strcasecmp(as, "delim") || !strcasecmp(as, "csv")) {
			stream->write_function(stream, "%s%s", private_data->str_task_id.c_str(), delim);
			stream->write_function(stream, "%s%s", (private_data->input_code[0] == '~' ? "inline" : private_data->input_code.c_str()), delim);

			switch_snprintf(tmp_str, sizeof(tmp_str), "%u", delay);
			stream->write_function(stream, "%s%s", tmp_str, delim);

			switch_snprintf(tmp_str, sizeof(tmp_str), "%u", private_data->stats.total_physical_size());
			stream->write_function(stream, "%s%s", tmp_str, delim);

			switch_snprintf(tmp_str, sizeof(tmp_str), "%u", private_data->stats.total_heap_size_executable());
			stream->write_function(stream, "%s%s", tmp_str, delim);

			switch_snprintf(tmp_str, sizeof(tmp_str), "%u", private_data->stats.total_heap_size());
			stream->write_function(stream, "%s%s", tmp_str, delim);

			switch_snprintf(tmp_str, sizeof(tmp_str), "%u", private_data->stats.used_heap_size());
			stream->write_function(stream, "%s%s", tmp_str, delim);

			switch_snprintf(tmp_str, sizeof(tmp_str), "%u", private_data->stats.heap_size_limit());
			stream->write_function(stream, "%s%s", tmp_str, delim);

			switch_snprintf(tmp_str, sizeof(tmp_str), "%u", private_data->stats.malloced_memory());
			stream->write_function(stream, "%s%s", tmp_str, delim);

			switch_snprintf(tmp_str, sizeof(tmp_str), "%u", private_data->stats.peak_malloced_memory());
			stream->write_function(stream, "%s%s", tmp_str, "\n");

		} else if (!strcasecmp(as, "xml")) {
			if (!(xml_row = switch_xml_add_child_d(xml, "row", rows++))) {
				goto end;
			}

			switch_snprintf(tmp_str, sizeof(tmp_str), "%d", rows);
			switch_xml_set_attr(switch_xml_set_flag(xml_row, SWITCH_XML_DUP), strdup("row_id"), strdup(tmp_str));

			if (!(xml_field = switch_xml_add_child_d(xml_row, "task_id", f_off++))) {				
				goto end;
			} 
			switch_xml_set_txt_d(xml_field, private_data->str_task_id.c_str());

			if (!(xml_field = switch_xml_add_child_d(xml_row, "input_code", f_off++))) {
				goto end;
			}
			switch_xml_set_txt_d(xml_field, (private_data->input_code[0] == '~' ? "inline" : private_data->input_code.c_str()));

			if (!(xml_field = switch_xml_add_child_d(xml_row, "execution_time", f_off++))) {
				goto end;
			}
			switch_snprintf(tmp_str, sizeof(tmp_str), "%u", delay);
			switch_xml_set_txt_d(xml_field, tmp_str);

			if (!(xml_field = switch_xml_add_child_d(xml_row, "total_physical_size", f_off++))) {
				goto end;
			}
			switch_snprintf(tmp_str, sizeof(tmp_str), "%u", private_data->stats.total_physical_size());
			switch_xml_set_txt_d(xml_field, tmp_str);

			if (!(xml_field = switch_xml_add_child_d(xml_row, "total_heap_size_executable", f_off++))) {
				goto end;
			}
			switch_snprintf(tmp_str, sizeof(tmp_str), "%u", private_data->stats.total_heap_size_executable());
			switch_xml_set_txt_d(xml_field, tmp_str);

			if (!(xml_field = switch_xml_add_child_d(xml_row, "total_heap_size", f_off++))) {
				goto end;
			}
			switch_snprintf(tmp_str, sizeof(tmp_str), "%u", private_data->stats.total_heap_size());
			switch_xml_set_txt_d(xml_field, tmp_str);

			if (!(xml_field = switch_xml_add_child_d(xml_row, "used_heap_size", f_off++))) {
				goto end;
			}
			switch_snprintf(tmp_str, sizeof(tmp_str), "%u", private_data->stats.used_heap_size());
			switch_xml_set_txt_d(xml_field, tmp_str);

			if (!(xml_field = switch_xml_add_child_d(xml_row, "heap_size_limit", f_off++))) {
				goto end;
			}
			switch_snprintf(tmp_str, sizeof(tmp_str), "%u", private_data->stats.heap_size_limit());
			switch_xml_set_txt_d(xml_field, tmp_str);

			if (!(xml_field = switch_xml_add_child_d(xml_row, "malloced_memory", f_off++))) {
				goto end;
			}
			switch_snprintf(tmp_str, sizeof(tmp_str), "%u", private_data->stats.malloced_memory());
			switch_xml_set_txt_d(xml_field, tmp_str);

			if (!(xml_field = switch_xml_add_child_d(xml_row, "peak_malloced_memory", f_off++))) {
				goto end;
			}
			switch_snprintf(tmp_str, sizeof(tmp_str), "%u", private_data->stats.peak_malloced_memory());
			switch_xml_set_txt_d(xml_field, tmp_str);

		}
	}

	if (!strcasecmp(as, "json")) {
		cJSON *result;

		if (!(result = cJSON_CreateObject())) {
			stream->write_function(stream, "-ERR Error creating json object!\n");
			goto end;
		}
		else {
			cJSON_AddItemToObject(result, "row_count", cJSON_CreateNumber(count));
			cJSON_AddItemToObject(result, "rows", json);

			output_text = cJSON_PrintUnformatted(result);
			json = result;
		}

		stream_write_safe(output_text);

	} else if (!strcasecmp(as, "xml")) {
		switch_snprintf(tmp_str, sizeof(tmp_str), "%u", count);
		switch_xml_set_attr_d(xml, "row_count", tmp_str);

		output_text = switch_xml_toxml(xml, SWITCH_FALSE);

		stream_write_safe(output_text);

	} else if (!strcasecmp(as, "delim") || !strcasecmp(as, "csv")) {
		stream->write_function(stream, "%s%u total.%s", "\n", count, "\n");
	}

end:
	
	switch_xml_free(xml);
	cJSON_Delete(json);
	switch_safe_free(output_text);
	switch_safe_free(mydata);

	return SWITCH_STATUS_SUCCESS;
}

SWITCH_MODULE_LOAD_FUNCTION(mod_v8_load)
{
	switch_application_interface_t *app_interface;
	switch_chat_application_interface_t *chat_app_interface;
	switch_json_api_interface_t *json_api_interface;

	if (switch_event_bind_removable(modname, SWITCH_EVENT_ALL, SWITCH_EVENT_SUBCLASS_ANY, event_handler, NULL, &globals.event_node) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Couldn't bind to events\n");
		return SWITCH_STATUS_GENERR;
	}

	globals.pool = pool;

#if defined(V8_MAJOR_VERSION) && V8_MAJOR_VERSION >=5
	switch_mutex_init(&globals.compiled_script_hash_mutex, SWITCH_MUTEX_NESTED, globals.pool);
	switch_mutex_init(&globals.task_manager_mutex, SWITCH_MUTEX_NESTED, globals.pool);
	switch_mutex_init(&globals.mutex, SWITCH_MUTEX_NESTED, globals.pool);
#endif
	switch_mutex_init(&globals.event_mutex, SWITCH_MUTEX_NESTED, globals.pool);
	globals.event_handlers = new set<FSEventHandler *>();

	if (load_modules() != SWITCH_STATUS_SUCCESS) {
		delete globals.event_handlers;
		switch_event_unbind(&globals.event_node);
		return SWITCH_STATUS_FALSE;
	}

#if defined(V8_MAJOR_VERSION) && V8_MAJOR_VERSION >=5
	globals.v8platform = NULL;
	globals.cache_expires_seconds = 0;
	globals.performance_monitor = false;
	globals.script_caching = switch_core_strdup(globals.pool, "disabled");

	JSMain::Initialize(&globals.v8platform);

	switch_core_hash_init(&globals.compiled_script_hash);
	globals.task_manager = new map<string, Isolate *>();
#else
	JSMain::Initialize();
#endif

	/* Make all "built in" modules available to load on demand */
	v8_mod_init_built_in(FSCoreDB::GetModuleInterface());
	v8_mod_init_built_in(FSDBH::GetModuleInterface());
	v8_mod_init_built_in(FSCURL::GetModuleInterface());
#ifdef HAVE_ODBC
	/* Only add ODBC class if ODBC is available in the system */
	v8_mod_init_built_in(FSODBC::GetModuleInterface());
#endif
	v8_mod_init_built_in(FSSocket::GetModuleInterface());
	v8_mod_init_built_in(FSTeleTone::GetModuleInterface());
	v8_mod_init_built_in(FSXML::GetModuleInterface());
	v8_mod_init_built_in(FSFile::GetModuleInterface());
	v8_mod_init_built_in(FSEventHandler::GetModuleInterface());

	/* connect my internal structure to the blank pointer passed to me */
	*module_interface = switch_loadable_module_create_module_interface(pool, modname);
	SWITCH_ADD_API(jsrun_interface, "jsrun", "run a script", launch_async, "jsrun <script> [additional_vars [...]]");
	SWITCH_ADD_API(jsapi_interface, "jsapi", "execute an api call", jsapi_function, "jsapi <script> [additional_vars [...]]");
	SWITCH_ADD_API(jsmon_interface, "jsmon", "toggle performance monitor", jsmon_function, "jsmon on|off");
	SWITCH_ADD_API(jsrun_interface, "jsps", "process status", process_status_function, "jsps [as plain|json|xml|delim|csv [<delimeter>]]");
	SWITCH_ADD_API(jsrun_interface, "jskill", "kill a task", kill_function, "jskill <task_id>");
	SWITCH_ADD_APP(app_interface, "javascript", "Launch JS ivr", "Run a javascript ivr on a channel", v8_dp_function, "<script> [additional_vars [...]]", SAF_SUPPORT_NOMEDIA);
	SWITCH_ADD_CHAT_APP(chat_app_interface, "javascript", "execute a js script", "execute a js script", v8_chat_function, "<script>", SCAF_NONE);

	SWITCH_ADD_JSON_API(json_api_interface, "jsjson", "JSON JS Gateway", json_function, "");

	load_configuration();

	/* indicate that the module should continue to be loaded */
	return SWITCH_STATUS_NOUNLOAD;
}

SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_v8_shutdown)
{
	switch_event_unbind(&globals.event_node);

	delete globals.event_handlers;
	switch_mutex_destroy(globals.event_mutex);

#if defined(V8_MAJOR_VERSION) && V8_MAJOR_VERSION >=5
	delete globals.v8platform;

	switch_core_hash_destroy(&globals.compiled_script_hash);
	switch_mutex_destroy(globals.compiled_script_hash_mutex);
	switch_mutex_destroy(globals.task_manager_mutex);
	delete globals.task_manager;
	switch_mutex_destroy(globals.mutex);
#endif

	switch_core_hash_destroy(&module_manager.load_hash);
	switch_core_destroy_memory_pool(&module_manager.pool);

	return SWITCH_STATUS_SUCCESS;
}

SWITCH_END_EXTERN_C

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
