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
 * Brian Fertig <brian.fertig@convergencetek.com>
 * Johny Kadarisman <jkr888@gmail.com>
 * Traun Leyden <tleyden@branchcut.com>
 *
 * mod_python.c -- Python Module
 *
 */

#include <Python.h>

#ifndef _REENTRANT
#define _REENTRANT
#endif

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <switch.h>
#include "mod_python_extra.h"

PyThreadState *mainThreadState = NULL;

void init_freeswitch(void);
int py_thread(const char *text);
static void set_max_recursion_depth(void);
static switch_api_interface_t python_run_interface;

SWITCH_MODULE_LOAD_FUNCTION(mod_python_load);
SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_python_shutdown);
SWITCH_MODULE_DEFINITION_EX(mod_python, mod_python_load, mod_python_shutdown, NULL, SMODF_GLOBAL_SYMBOLS);

static struct {
	switch_memory_pool_t *pool;
	char *xml_handler;
} globals;

struct switch_py_thread {
	struct switch_py_thread *prev, *next;
	char *cmd;
	char *args;
	switch_memory_pool_t *pool;
	PyThreadState *tstate;
};
struct switch_py_thread *thread_pool_head = NULL;
static switch_mutex_t *THREAD_POOL_LOCK = NULL;

static void eval_some_python(const char *funcname, char *args, switch_core_session_t *session, switch_stream_handle_t *stream, switch_event_t *params,
							 char **str, struct switch_py_thread *pt)
{
	PyThreadState *tstate = NULL;
	char *dupargs = NULL;
	char *argv[2] = { 0 };
	int argc;
	int lead = 0;
	char *script = NULL;
	PyObject *module = NULL, *sp = NULL, *stp = NULL, *eve = NULL;
	PyObject *function = NULL;
	PyObject *arg = NULL;
	PyObject *result = NULL;
	switch_channel_t *channel = NULL;
	char *p;

	if (str) {
		*str = NULL;
	}

	if (args) {
		dupargs = strdup(args);
	} else {
		return;
	}

	assert(dupargs != NULL);

	if (!(argc = switch_separate_string(dupargs, ' ', argv, (sizeof(argv) / sizeof(argv[0]))))) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "No module name specified!\n");
		goto done;
	}

	script = strdup(switch_str_nil(argv[0]));

	lead = 1;

	if ((p = strstr(script, "::"))) {
		*p = '\0';
		p += 2;
		if (p) {
			funcname = p;
		}
	}

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "Invoking py module: %s\n", script);

	tstate = PyThreadState_New(mainThreadState->interp);
	if (!tstate) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "error acquiring tstate\n");
		goto done;
	}

	/* Save state in thread struct so we can terminate it later if needed */
	if (pt)
		pt->tstate = tstate;

	// swap in thread state
	PyEval_AcquireThread(tstate);
	init_freeswitch();

	// import the module
	module = PyImport_ImportModule((char *) script);
	if (!module) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Error importing module\n");
		PyErr_Print();
		PyErr_Clear();
		goto done_swap_out;
	}
	// reload the module
	module = PyImport_ReloadModule(module);
	if (!module) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Error reloading module\n");
		PyErr_Print();
		PyErr_Clear();
		goto done_swap_out;
	}
	// get the handler function to be called
	function = PyObject_GetAttrString(module, (char *) funcname);
	if (!function) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Module does not define %s\n", funcname);
		PyErr_Print();
		PyErr_Clear();
		goto done_swap_out;
	}

	if (session) {
		channel = switch_core_session_get_channel(session);
		sp = mod_python_conjure_session(module, session);
	}

	if (params) {
		eve = mod_python_conjure_event(params);
	}

	if (stream) {
		stp = mod_python_conjure_stream(stream);
		if (stream->param_event) {
			eve = mod_python_conjure_event(stream->param_event);
		}
	}

	if (sp && eve && stp) {
		arg = Py_BuildValue("(OOOs)", sp, stp, eve, switch_str_nil(argv[1]));
	} else if (eve && stp) {
		arg = Py_BuildValue("(sOOs)", "na", stp, eve, switch_str_nil(argv[1]));
	} else if (eve) {
		arg = Py_BuildValue("(Os)", eve, switch_str_nil(argv[1]));
	} else if (sp) {
		arg = Py_BuildValue("(Os)", sp, switch_str_nil(argv[1]));
	} else {
		arg = Py_BuildValue("(s)", switch_str_nil(argv[1]));
	}

	// invoke the handler 
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Call python script \n");
	result = PyEval_CallObjectWithKeywords(function, arg, (PyObject *) NULL);
	Py_DECREF(function);
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Finished calling python script \n");

	// check the result and print out any errors
	if (result) {
		if (str) {
			*str = strdup((char *) PyString_AsString(result));
		}
	} else if (!PyErr_ExceptionMatches(PyExc_SystemExit)) {
		// Print error, but ignore SystemExit 
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Error calling python script\n");
		PyErr_Print();
		PyErr_Clear();
		PyRun_SimpleString("python_makes_sense");
		PyGC_Collect();
	}

  done_swap_out:

	if (arg) {
		Py_DECREF(arg);
	}

	if (sp) {
		Py_DECREF(sp);
	}

	if (tstate) {
		// thread state must be cleared explicitly or we'll get memory leaks
		PyThreadState_Clear(tstate);
		PyEval_ReleaseThread(tstate);
		PyThreadState_Delete(tstate);
	}

  done:

	switch_safe_free(dupargs);
	switch_safe_free(script);


}


static switch_xml_t python_fetch(const char *section,
								 const char *tag_name, const char *key_name, const char *key_value, switch_event_t *params, void *user_data)
{

	switch_xml_t xml = NULL;
	char *str = NULL;

	if (!zstr(globals.xml_handler)) {
		char *mycmd = strdup(globals.xml_handler);

		switch_assert(mycmd);

		eval_some_python("xml_fetch", mycmd, NULL, NULL, params, &str, NULL);

		if (str) {
			if (zstr(str)) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "No Result\n");
			} else if (!(xml = switch_xml_parse_str((char *) str, strlen(str)))) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Error Parsing XML Result!\n");
			}
			switch_safe_free(str);
		}

		free(mycmd);
	}

	return xml;
}

static switch_status_t do_config(void)
{
	char *cf = "python.conf";
	switch_xml_t cfg, xml, settings, param;

	if (!(xml = switch_xml_open_cfg(cf, &cfg, NULL))) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Open of %s failed\n", cf);
		return SWITCH_STATUS_TERM;
	}

	if ((settings = switch_xml_child(cfg, "settings"))) {
		for (param = switch_xml_child(settings, "param"); param; param = param->next) {
			char *var = (char *) switch_xml_attr_soft(param, "name");
			char *val = (char *) switch_xml_attr_soft(param, "value");

			if (!strcmp(var, "xml-handler-script")) {
				globals.xml_handler = switch_core_strdup(globals.pool, val);
			} else if (!strcmp(var, "xml-handler-bindings")) {
				if (!zstr(globals.xml_handler)) {
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "binding '%s' to '%s'\n", globals.xml_handler, val);
					switch_xml_bind_search_function(python_fetch, switch_xml_parse_section_string(val), NULL);
				}
			} else if (!strcmp(var, "startup-script")) {
				if (val) {
					py_thread(val);
				}
			}
		}
	}

	switch_xml_free(xml);

	return SWITCH_STATUS_SUCCESS;
}


/**
 * As freeswitch runs with a smaller than normal stack size (240K instead of the usual value .. 1 or 2 MB),
 * we must decrease the default python recursion limit accordingly.  Otherwise, python can easily blow
 * up the stack and the whole switch crashes.  See modlang-134
 */
static void set_max_recursion_depth(void)
{

	// assume that a stack frame is approximately 1K, so divide thread stack size (eg, 240K) by
	// 1K to get the approx number of stack frames we can hold before blowing up.
	int newMaxRecursionDepth = SWITCH_THREAD_STACKSIZE / 1024;

	PyObject *sysModule = PyImport_ImportModule("sys");
	PyObject *setRecursionLimit = PyObject_GetAttrString(sysModule, "setrecursionlimit");
	PyObject *recLimit = Py_BuildValue("(i)", newMaxRecursionDepth);
	PyObject *setrecursion_result = PyEval_CallObjectWithKeywords(setRecursionLimit, recLimit, (PyObject *) NULL);
	if (setrecursion_result) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Set python recursion limit to %d\n", newMaxRecursionDepth);
	} else {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Failed to set recursion limit to %d\n", newMaxRecursionDepth);
		PyErr_Print();
		PyErr_Clear();
		PyRun_SimpleString("python_makes_sense");
		PyGC_Collect();
	}


}

SWITCH_STANDARD_APP(python_function)
{
	eval_some_python("handler", (char *) data, session, NULL, NULL, NULL, NULL);

}

static void *SWITCH_THREAD_FUNC py_thread_run(switch_thread_t *thread, void *obj)
{
	switch_memory_pool_t *pool;
	struct switch_py_thread *pt = (struct switch_py_thread *) obj;

	/* Put thread in pool so we keep track of our threads */
	switch_mutex_lock(THREAD_POOL_LOCK);
	pt->next = thread_pool_head;
	pt->prev = NULL;
	if (pt->next)
		pt->next->prev = pt;
	thread_pool_head = pt;
	switch_mutex_unlock(THREAD_POOL_LOCK);

	/* Run the python script */
	eval_some_python("runtime", pt->args, NULL, NULL, NULL, NULL, pt);

	/* Thread is dead, remove from pool */
	switch_mutex_lock(THREAD_POOL_LOCK);
	if (pt->next)
		pt->next->prev = pt->prev;
	if (pt->prev)
		pt->prev->next = pt->next;
	if (thread_pool_head == pt)
		thread_pool_head = pt->next;
	switch_mutex_unlock(THREAD_POOL_LOCK);

	pool = pt->pool;
	switch_core_destroy_memory_pool(&pool);

	return NULL;
}

SWITCH_STANDARD_API(api_python)
{

	eval_some_python("fsapi", (char *) cmd, session, stream, NULL, NULL, NULL);

	return SWITCH_STATUS_SUCCESS;
}

int py_thread(const char *text)
{
	switch_thread_t *thread;
	switch_threadattr_t *thd_attr = NULL;
	switch_memory_pool_t *pool;
	struct switch_py_thread *pt;

	switch_core_new_memory_pool(&pool);
	assert(pool != NULL);

	pt = switch_core_alloc(pool, sizeof(*pt));
	assert(pt != NULL);

	pt->pool = pool;
	pt->args = switch_core_strdup(pt->pool, text);

	switch_threadattr_create(&thd_attr, pt->pool);
	switch_threadattr_detach_set(thd_attr, 1);
	switch_threadattr_stacksize_set(thd_attr, SWITCH_THREAD_STACKSIZE);
	switch_thread_create(&thread, thd_attr, py_thread_run, pt, pt->pool);

	return 0;
}

SWITCH_STANDARD_API(launch_python)
{

	if (zstr(cmd)) {
		stream->write_function(stream, "USAGE: %s\n", python_run_interface.syntax);
		return SWITCH_STATUS_SUCCESS;
	}

	py_thread(cmd);
	stream->write_function(stream, "OK\n");
	return SWITCH_STATUS_SUCCESS;
}

SWITCH_MODULE_LOAD_FUNCTION(mod_python_load)
{
	switch_api_interface_t *api_interface;
	switch_application_interface_t *app_interface;
	char *pp = getenv("PYTHONPATH");

	if (pp) {
		char *path = switch_mprintf("%s:%s", pp, SWITCH_GLOBAL_dirs.script_dir);
		setenv("PYTHONPATH", path, 1);
		free(path);
	} else {
		setenv("PYTHONPATH", SWITCH_GLOBAL_dirs.script_dir, 1);
	}

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "Python Framework Loading...\n");

	globals.pool = pool;

	if (!Py_IsInitialized()) {

		// initialize python system
		Py_Initialize();

		// create GIL and a threadstate
		PyEval_InitThreads();

		// save threadstate since it's interp field will be needed
		// to create new threadstates, and will be needed for shutdown
		mainThreadState = PyThreadState_Get();

		// set the maximum recursion depth
		set_max_recursion_depth();

		// swap out threadstate since the call threads will create
		// their own and swap in their threadstate
		PyThreadState_Swap(NULL);

		// release GIL
		PyEval_ReleaseLock();
	}

	switch_mutex_init(&THREAD_POOL_LOCK, SWITCH_MUTEX_NESTED, pool);

	do_config();

	/* connect my internal structure to the blank pointer passed to me */
	*module_interface = switch_loadable_module_create_module_interface(pool, modname);
	SWITCH_ADD_API(api_interface, "pyrun", "run a python script", launch_python, "python </path/to/script>");
	SWITCH_ADD_API(api_interface, "python", "run a python script", api_python, "python </path/to/script>");
	SWITCH_ADD_APP(app_interface, "python", "Launch python ivr", "Run a python ivr on a channel", python_function, "<script> [additional_vars [...]]",
				   SAF_SUPPORT_NOMEDIA);

	/* indicate that the module should continue to be loaded */
	return SWITCH_STATUS_NOUNLOAD;
}

/*
  Called when the system shuts down*/
SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_python_shutdown)
{
	PyInterpreterState *mainInterpreterState;
	PyThreadState *myThreadState;
	int thread_cnt = 0;
	struct switch_py_thread *pt = NULL;
	struct switch_py_thread *nextpt;
	int i;

	/* Kill all remaining threads */
	pt = thread_pool_head;
	PyEval_AcquireLock();
	while (pt) {
		thread_cnt++;
		nextpt = pt->next;

		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "Forcibly terminating script [%s]\n", pt->args);

		/* Kill python script */
		PyThreadState_Swap(pt->tstate);
		PyThreadState_SetAsyncExc(pt->tstate->thread_id, PyExc_SystemExit);

		pt = nextpt;
	}
	PyThreadState_Swap(mainThreadState);
	PyEval_ReleaseLock();
	switch_yield(1000000);

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "Had to kill %d threads\n", thread_cnt);

	/* Give threads a few seconds to terminate. 
	   Not using switch_thread_join() since if threads refuse to die
	   then freeswitch will hang */
	for (i = 0; i < 10 && thread_pool_head; i++) {
		switch_yield(1000000);
	}
	if (thread_pool_head) {
		/* Not all threads died in time */
		pt = thread_pool_head;
		while (pt) {
			nextpt = pt->next;
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Script [%s] didn't exit in time\n", pt->args);
			pt = nextpt;
		}
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Forcing python shutdown. This might cause freeswitch to crash!\n");
	}


	PyEval_AcquireLock();
	mainInterpreterState = mainThreadState->interp;
	myThreadState = PyThreadState_New(mainInterpreterState);
	PyThreadState_Swap(myThreadState);
	PyEval_ReleaseLock();

	Py_Finalize();
	PyEval_ReleaseLock();

	return SWITCH_STATUS_UNLOAD;

}

/* Return the number of arguments of the application command line */

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
