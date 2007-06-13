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


PyThreadState *mainThreadState = NULL;

void init_freeswitch(void);
static switch_api_interface_t python_run_interface;

SWITCH_MODULE_LOAD_FUNCTION(mod_python_load);
SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_python_shutdown);
SWITCH_MODULE_DEFINITION(mod_python, mod_python_load, mod_python_shutdown, NULL);

static void eval_some_python(char *uuid, char *args)
{
	PyThreadState *tstate = NULL;
	FILE *pythonfile = NULL;
	char *dupargs = NULL;
	char *argv[128] = {0};
	int argc;
	int lead = 0;
	char *script = NULL, *script_path = NULL, *path = NULL;

	if (args) {
		dupargs = strdup(args);
	} else {
		return;
	}

	assert(dupargs != NULL);
	
	if (!(argc = switch_separate_string(dupargs, ' ', argv, (sizeof(argv) / sizeof(argv[0]))))) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "No script name specified!\n");
		goto done;
	}

	script = argv[0];
	lead = 1;

	if (switch_is_file_path(script)) {
		script_path = script;
		if ((script = strrchr(script_path, *SWITCH_PATH_SEPARATOR))) {
			script++;
		} else {
			script = script_path;
		}
	} else if ((path = switch_mprintf("%s%s%s", SWITCH_GLOBAL_dirs.script_dir, SWITCH_PATH_SEPARATOR, script))) {
		script_path = path;
	}
	if (script_path) {
		if (!switch_file_exists(script_path, NULL) == SWITCH_STATUS_SUCCESS) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Cannot Open File: %s\n", script_path);
			goto done;
		}
	}


	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "running %s\n", script_path);


	if ((pythonfile = fopen(script_path, "r"))) {

		PyEval_AcquireLock();
		tstate = PyThreadState_New(mainThreadState->interp);
		if (!tstate) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "error acquiring tstate\n");
			goto done;
		}
		PyThreadState_Swap(tstate);
		init_freeswitch(); 
		PyRun_SimpleString("from freeswitch import *");
		if (uuid) {
			char code[128];
			snprintf(code, sizeof(code), "session = PySession(\"%s\");", uuid);
			PyRun_SimpleString(code);
		}
		PySys_SetArgv(argc - lead, &argv[lead]);
		PyRun_SimpleFile(pythonfile, script);
		PyThreadState_Swap(NULL);
		PyThreadState_Clear(tstate);
		PyThreadState_Delete(tstate);
		PyEval_ReleaseLock();

		
	} else {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "error running %s\n", script_path);
	}


 done:

	if (pythonfile) {
		fclose(pythonfile);
	}

	switch_safe_free(dupargs);
	switch_safe_free(path);
}

static void python_function(switch_core_session_t *session, char *data)
{
	eval_some_python(switch_core_session_get_uuid(session), (char *)data);
	
}

struct switch_py_thread {
	char *args;
	switch_memory_pool_t *pool;
};

static void *SWITCH_THREAD_FUNC py_thread_run(switch_thread_t *thread, void *obj)
{
	switch_memory_pool_t *pool;
	struct switch_py_thread *pt = (struct switch_py_thread *) obj;

	eval_some_python(NULL, strdup(pt->args));

	pool = pt->pool;
	switch_core_destroy_memory_pool(&pool);

	return NULL;
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

static switch_application_interface_t python_application_interface = {
	/*.interface_name */ "python",
	/*.application_function */ python_function,
	NULL, NULL, NULL,
	/* flags */ SAF_NONE,
	/* should we support no media mode here?  If so, we need to detect the mode, and either disable the media functions or indicate media if/when we need */
	/*.next */ NULL
};

static switch_api_interface_t python_run_interface = {
	/*.interface_name */ "python",
	/*.desc */ "run a python script",
	/*.function */ launch_python,
	/*.syntax */ "python </path/to/script>",
	/*.next */ NULL
};

static switch_loadable_module_interface_t python_module_interface = {
	/*.module_name */ modname,
	/*.endpoint_interface */ NULL,
	/*.timer_interface */ NULL,
	/*.dialplan_interface */ NULL,
	/*.codec_interface */ NULL,
	/*.application_interface */ &python_application_interface,
	/*.api_interface */ &python_run_interface,
	/*.file_interface */ NULL,
	/*.speech_interface */ NULL,
	/*.directory_interface */ NULL
};

SWITCH_MODULE_LOAD_FUNCTION(mod_python_load)
{
	/* connect my internal structure to the blank pointer passed to me */
	*module_interface = &python_module_interface;
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "Python Framework Loading...\n");
	
	Py_Initialize();
	PyEval_InitThreads();

	mainThreadState = PyThreadState_Get();

	PyThreadState_Swap(NULL); 

	PyEval_ReleaseLock();	

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
    return SWITCH_STATUS_SUCCESS;

}




/* Return the number of arguments of the application command line */
