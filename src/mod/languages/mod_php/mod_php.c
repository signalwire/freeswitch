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
 * Anthony Minessale II <anthmct@yahoo.com>
 *
 *
 * mod_php.c -- PHP Module
 *
 */

#ifndef _REENTRANT
#define _REENTRANT
#endif

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <sapi/embed/php_embed.h>

#include <switch.h>

const char modname[] = "mod_php";



static void php_function(switch_core_session_t *session, char *data)
{
	char *uuid = switch_core_session_get_uuid(session);
	uint32_t ulen = strlen(uuid);
	uint32_t len = strlen((char *) data) + ulen + 2;
	char *mydata = switch_core_session_alloc(session, len);
	int argc;
	char *argv[5];
	char php_code[1024]; 
	void*** tsrm_ls = NULL;

	snprintf(mydata, len, "%s %s", uuid, data);

	argc = switch_separate_string(mydata, ' ',
								  argv,
								  (sizeof(argv) / sizeof(argv[0])));
	
	sprintf(php_code, "$uuid=\"%s\"; include(\"%s\");\n", argv[0], argv[1]);
	php_embed_init(argc, argv, &tsrm_ls);

	//PHP_EMBED_START_BLOCK(argc, argv);
	zend_eval_string(php_code, NULL, "Embedded code" TSRMLS_CC);
	//PHP_EMBED_END_BLOCK();
	php_embed_shutdown(tsrm_ls);



}

static const switch_application_interface_t php_application_interface = {
	/*.interface_name */ "php",
	/*.application_function */ php_function
};

static switch_loadable_module_interface_t php_module_interface = {
	/*.module_name */ modname,
	/*.endpoint_interface */ NULL,
	/*.timer_interface */ NULL,
	/*.dialplan_interface */ NULL,
	/*.codec_interface */ NULL,
	/*.application_interface */ &php_application_interface,
	/*.api_interface */ NULL,
	/*.file_interface */ NULL,
	/*.speech_interface */ NULL,
	/*.directory_interface */ NULL
};

SWITCH_MOD_DECLARE(switch_status_t) switch_module_load(const switch_loadable_module_interface_t **module_interface, char *filename)
{
	/* connect my internal structure to the blank pointer passed to me */
	*module_interface = &php_module_interface;

	//switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "Hello World!\n");

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
