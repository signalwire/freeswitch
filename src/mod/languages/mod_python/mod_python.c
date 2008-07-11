/* 
 * FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
 * Copyright (C) 2005/2006, Anthony Minessale II <anthmct@yahoo.com>
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
 * Anthony Minessale II <anthmct@yahoo.com>
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
static switch_api_interface_t python_run_interface;


SWITCH_MODULE_LOAD_FUNCTION(mod_python_load);
SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_python_shutdown);
SWITCH_MODULE_DEFINITION(mod_python, mod_python_load, mod_python_shutdown, NULL);


static struct {
	switch_memory_pool_t *pool;
	char *xml_handler;
	switch_event_node_t *node;
} globals;


static void eval_some_python(char *args, switch_core_session_t *session, switch_stream_handle_t *stream, switch_event_t *params, char **str)
{
	PyThreadState *tstate = NULL;
	char *dupargs = NULL;
	char *argv[128] = { 0 };
	int argc;
	int lead = 0;
	char *script = NULL;
	PyObject *module = NULL, *sp = NULL, *stp = NULL, *eve = NULL;
	PyObject *function = NULL;
	PyObject *arg = NULL;
	PyObject *result = NULL;
	char *uuid = NULL;

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

	script = argv[0];
	lead = 1;

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "Invoking py module: %s\n", script);

	tstate = PyThreadState_New(mainThreadState->interp);
	if (!tstate) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "error acquiring tstate\n");
		goto done;
	}
	// swap in thread state
	PyEval_AcquireThread(tstate);
	if (session) {
		uuid = switch_core_session_get_uuid(session);
		// record the fact that thread state is swapped in
		switch_channel_t *channel = switch_core_session_get_channel(session);
		switch_channel_set_private(channel, "SwapInThreadState", NULL);
	}
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

	if (params) {
		eve = mod_python_conjure_event(module, params, "params");
	}

	if (stream) {
		stp = mod_python_conjure_stream(module, stream, "stream");
		if (stream->param_event) {
			eve = mod_python_conjure_event(module, stream->param_event, "env");
		}
	}

	// get the handler function to be called
	function = PyObject_GetAttrString(module, "handler");
	if (!function) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Module does not define handler(uuid)\n");
		PyErr_Print();
		PyErr_Clear();
		goto done_swap_out;
	}

	if (uuid) {
		// build a tuple to pass the args, the uuid of session
		arg = Py_BuildValue("(s)", uuid);
		if (!arg) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Error building args\n");
			PyErr_Print();
			PyErr_Clear();
			goto done_swap_out;
		}
	} else {
		arg = PyTuple_New(1);
		PyObject *nada = Py_BuildValue("");
		PyTuple_SetItem(arg, 0, nada);
	}

	if (session) {
		sp = mod_python_conjure_session(module, session, "session");
	}

	// invoke the handler 
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Call python script \n");
	result = PyEval_CallObjectWithKeywords(function, arg, (PyObject *) NULL);
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Finished calling python script \n");

	// check the result and print out any errors
	if (result) {
		if (str) {
			*str = strdup((char *) PyString_AsString(result));
		}
	} else {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Error calling python script\n");
		PyErr_Print();
		PyErr_Clear();
	}
	
 done:
 done_swap_out:
	
	if (sp) {
		Py_XDECREF(sp);
	}

    // swap out thread state
    if (session) {
		//switch_core_session_rwunlock(session);
        // record the fact that thread state is swapped in
        switch_channel_t *channel = switch_core_session_get_channel(session);
        PyThreadState *swapin_tstate = (PyThreadState *) switch_channel_get_private(channel, "SwapInThreadState");
        // so lets assume nothing in the python script swapped any thread state in
        // or out .. thread state will currently be swapped in, and the SwapInThreadState
        // will be null
        if (swapin_tstate == NULL) {
            // clear out threadstate
            switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "clear threadstate \n");
            // we know we are swapped in because swapin_tstate is NULL, and therefore we have the GIL, so
            // it is safe to call PyThreadState_Get.
            PyThreadState *cur_tstate = PyThreadState_Get();
            PyThreadState_Clear(cur_tstate);
            PyEval_ReleaseThread(cur_tstate);
            PyThreadState_Delete(cur_tstate);
        } else {
            // thread state is already swapped out, so, nothing for us to do
            switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "according to chan priv data, already swapped out \n");
        }
    } else {
        // they ran python script from cmd line, behave a bit differently (untested)
        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "No session: Threadstate mod_python.c swap-out! \n");
        PyEval_ReleaseThread(tstate);
    }

	
	switch_safe_free(dupargs);


}


static switch_xml_t python_fetch(const char *section,
							  const char *tag_name, const char *key_name, const char *key_value, switch_event_t *params, void *user_data)
{

	switch_xml_t xml = NULL;
	char *str = NULL;

	if (!switch_strlen_zero(globals.xml_handler)) {
		char *mycmd = strdup(globals.xml_handler);

		switch_assert(mycmd);

		eval_some_python(mycmd, NULL, NULL, params, &str);

		if (str) {
			if (switch_strlen_zero(str)) {
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
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "open of %s failed\n", cf);
		return SWITCH_STATUS_TERM;
	}

	if ((settings = switch_xml_child(cfg, "settings"))) {
		for (param = switch_xml_child(settings, "param"); param; param = param->next) {
			char *var = (char *) switch_xml_attr_soft(param, "name");
			char *val = (char *) switch_xml_attr_soft(param, "value");

			if (!strcmp(var, "xml-handler-script")) {
				globals.xml_handler = switch_core_strdup(globals.pool, val);
			} else if (!strcmp(var, "xml-handler-bindings")) {
				if (!switch_strlen_zero(globals.xml_handler)) {
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "binding '%s' to '%s'\n", globals.xml_handler, val);
					switch_xml_bind_search_function(python_fetch, switch_xml_parse_section_string(val), NULL);
				}
			}
		}
	}

	switch_xml_free(xml);

	return SWITCH_STATUS_SUCCESS;
}

SWITCH_STANDARD_APP(python_function)
{
	eval_some_python((char *) data, session, NULL, NULL, NULL);

}

struct switch_py_thread {
	char *args;
	switch_memory_pool_t *pool;
};

static void *SWITCH_THREAD_FUNC py_thread_run(switch_thread_t *thread, void *obj)
{
	switch_memory_pool_t *pool;
	struct switch_py_thread *pt = (struct switch_py_thread *) obj;

	eval_some_python(pt->args, NULL, NULL, NULL, NULL);

	pool = pt->pool;
	switch_core_destroy_memory_pool(&pool);

	return NULL;
}

SWITCH_STANDARD_API(api_python)
{
	
	eval_some_python((char *) cmd, session, stream, NULL, NULL);

	return SWITCH_STATUS_SUCCESS;
}

SWITCH_STANDARD_API(launch_python)
{
	switch_thread_t *thread;
	switch_threadattr_t *thd_attr = NULL;
	switch_memory_pool_t *pool;
	struct switch_py_thread *pt;

	if (switch_strlen_zero(cmd)) {
		stream->write_function(stream, "USAGE: %s\n", python_run_interface.syntax);
		return SWITCH_STATUS_SUCCESS;
	}

	switch_core_new_memory_pool(&pool);
	assert(pool != NULL);

	pt = switch_core_alloc(pool, sizeof(*pt));
	assert(pt != NULL);

	pt->pool = pool;
	pt->args = switch_core_strdup(pt->pool, cmd);

	switch_threadattr_create(&thd_attr, pt->pool);
	switch_threadattr_detach_set(thd_attr, 1);
	switch_threadattr_stacksize_set(thd_attr, SWITCH_THREAD_STACKSIZE);
	switch_thread_create(&thread, thd_attr, py_thread_run, pt, pt->pool);

	stream->write_function(stream, "OK\n");
	return SWITCH_STATUS_SUCCESS;
}

SWITCH_MODULE_LOAD_FUNCTION(mod_python_load)
{
	switch_api_interface_t *api_interface;
	switch_application_interface_t *app_interface;

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "Python Framework Loading...\n");

	globals.pool = pool;
	do_config();

	if (!Py_IsInitialized()) {

		// initialize python system
		Py_Initialize();

		// create GIL and a threadstate
		PyEval_InitThreads();

		// save threadstate since it's interp field will be needed
		// to create new threadstates, and will be needed for shutdown
		mainThreadState = PyThreadState_Get();

		// swap out threadstate since the call threads will create
		// their own and swap in their threadstate
		PyThreadState_Swap(NULL);

		// release GIL
		PyEval_ReleaseLock();
	}


	/* connect my internal structure to the blank pointer passed to me */
	*module_interface = switch_loadable_module_create_module_interface(pool, modname);
	SWITCH_ADD_API(api_interface, "pyrun", "run a python script", launch_python, "python </path/to/script>");
	SWITCH_ADD_API(api_interface, "python", "run a python script", api_python, "python </path/to/script>");
	SWITCH_ADD_APP(app_interface, "python", "Launch python ivr", "Run a python ivr on a channel", python_function, "<script> [additional_vars [...]]",
				   SAF_SUPPORT_NOMEDIA);

	/* indicate that the module should continue to be loaded */
	return SWITCH_STATUS_SUCCESS;
}

/*
  Called when the system shuts down*/
SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_python_shutdown)
{
	PyInterpreterState *mainInterpreterState;
	PyThreadState *myThreadState;

	PyEval_AcquireLock();
	mainInterpreterState = mainThreadState->interp;
	myThreadState = PyThreadState_New(mainInterpreterState);
	PyThreadState_Swap(myThreadState);
	PyEval_ReleaseLock();

	Py_Finalize();
	PyEval_ReleaseLock();
	switch_event_unbind(&globals.node);
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
 * vim:set softtabstop=4 shiftwidth=4 tabstop=4 expandtab:
 */
