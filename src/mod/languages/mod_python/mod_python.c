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
 * Johny Kadarisman <jkr888 at gmail.com>
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

void init_freeswitch(void);

const char modname[] = "mod_python";

static void python_function(switch_core_session_t *session, char *data)
{
	char *uuid = switch_core_session_get_uuid(session);
    char *argv[1];
    FILE* pythonfile;

    argv[0] = uuid;
    pythonfile = fopen(data, "r");

    Py_Initialize();
    PySys_SetArgv(1, argv);
    init_freeswitch();
    PyRun_SimpleFile(pythonfile, "");
    Py_Finalize();

}

static const switch_application_interface_t python_application_interface = {
	/*.interface_name */ "python",
	/*.application_function */ python_function
};

static switch_loadable_module_interface_t python_module_interface = {
	/*.module_name */ modname,
	/*.endpoint_interface */ NULL,
	/*.timer_interface */ NULL,
	/*.dialplan_interface */ NULL,
	/*.codec_interface */ NULL,
	/*.application_interface */ &python_application_interface,
	/*.api_interface */ NULL,
	/*.file_interface */ NULL,
	/*.speech_interface */ NULL,
	/*.directory_interface */ NULL
};

SWITCH_MOD_DECLARE(switch_status_t) switch_module_load(const switch_loadable_module_interface_t **module_interface, char *filename)
{
	/* connect my internal structure to the blank pointer passed to me */
	*module_interface = &python_module_interface;

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "Python Framework Loading...\n");

	/* indicate that the module should continue to be loaded */
	return SWITCH_STATUS_SUCCESS;
}

/*
  Called when the system shuts down
  SWITCH_MOD_DECLARE(switch_status) switch_module_shutdown(void)
  {
  return SWITCH_STATUS_SUCCESS;
  }
*/

/*
  If it exists, this is called in it's own thread when the module-load completes
  SWITCH_MOD_DECLARE(switch_status) switch_module_shutdown(void)
  {
  return SWITCH_STATUS_SUCCESS;
  }
*/


/* Return the number of arguments of the application command line */

/* For Emacs:
 * Local Variables:
 * mode:c
 * indent-tabs-mode:nil
 * tab-width:4
 * c-basic-offset:4
 * End:
 * For VIM:
 * vim:set softtabstop=4 shiftwidth=4 tabstop=4 expandtab:
 */
