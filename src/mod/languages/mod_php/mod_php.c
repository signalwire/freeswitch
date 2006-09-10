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
 * Brian Fertig <brian.fertig@convergencetek.com>
 *
 * mod_php.c -- PHP Module
 *
 */

#if !defined(ZTS)
#error "ZTS Needs to be defined."
#endif


#ifndef _REENTRANT
#define _REENTRANT
#endif

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <sapi/embed/php_embed.h>
//#include "php.h"
//#include "php_variables.h"
//#include "ext/standard/info.h"
//#include "php_ini.h"
//#include "php_globals.h"
//#include "SAPI.h"
//#include "php_main.h"
//#include "php_version.h"
//#include "TSRM.h"
//#include "ext/standard/php_standard.h"


#include <switch.h>

const char modname[] = "mod_php";

static void php_function(switch_core_session_t *session, char *data)
{
	char *uuid = switch_core_session_get_uuid(session);
	uint32_t ulen = strlen(uuid);
	uint32_t len = strlen((char *) data) + ulen + 2;
	char *mydata = switch_core_session_alloc(session, len);
	int argc, retval;
	char *argv[5];
	char php_code[1024]; 
	void*** tsrm_ls = NULL;
	
	
	snprintf(mydata, len, "%s %s", uuid, data);

	argc = 1; //switch_separate_string(mydata, ' ', argv, (sizeof(argv) / sizeof(argv[0])));

	//sprintf(php_code, "$uuid=\"%s\"; include(\"%s\");\n", argv[0], argv[1]);
	sprintf(php_code, "include('%s');", argv[1]);

	zend_file_handle script;
	script.type = ZEND_HANDLE_FP;
	script.filename = data;
	script.opened_path = NULL;
	script.free_filename = 0;
	script.handle.fp = fopen(script.filename, "rb");	

	//php_embed_init(argc, argv, &tsrm_ls);
	if (php_request_startup(TSRMLS_C) == FAILURE) {
		return;
        }

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "Starting Script %s\n",data);

	retval = php_execute_script(&script TSRMLS_CC);	
	php_request_shutdown(NULL);

        return;


	//PHP_EMBED_START_BLOCK(argc, argv);
		//void*** tsrm_ls = NULL;
		//zend_error_cb = myapp_php_error_cb;
		//zend_eval_string(php_code, NULL, "MOD_PHP" TSRMLS_CC);
//		zend_execute_scripts(ZEND_REQUIRE TSRMLS_CC, NULL, 1, &script);
		//if (zend_execute_scripts(ZEND_REQUIRE TSRMLS_CC, NULL, 1, &script) == SUCCESS)
		    //switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "There was a problem with the file\n");
		//PHP_EMBED_END_BLOCK();
//	php_embed_shutdown(tsrm_ls);
		

	//}else{
	//    switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "There was a problem with the file\n");	   
	//}


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

/*SWITCH_MOD_DECLARE(switch_status_t) switch_module_load(const switch_loadable_module_interface_t **module_interface, char *filename)
{
	// connect my internal structure to the blank pointer passed to me 
	*module_interface = &php_module_interface;
	
	sapi_startup(&mod_php_sapi_module);
        mod_php_sapi_module.startup(&mod_php_sapi_module);


	//switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "Hello World!\n");

	// indicate that the module should continue to be loaded 
	return SWITCH_STATUS_SUCCESS;
}
*/

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


zend_module_entry mod_php_module_entry = {
        STANDARD_MODULE_HEADER,
        "mod_php",
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        NO_VERSION_YET,
        STANDARD_MODULE_PROPERTIES
};


static int sapi_mod_php_ub_write(const char *str, unsigned int str_length TSRMLS_DC)
{ // This function partly based on code from asterisk_php

  FILE *fp = fopen("mod_php.log", "a");
  fwrite(str, str_length, sizeof(char), fp);
  fclose(fp);


        char buffer[4096];
        int i, j = 0;
        for(i = 0; i < str_length; i++) {
                buffer[j++] = str[i];
                if(str[i] == 10) { /* new line */
                        buffer[j] = 0;
                        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "%s", buffer);
                        j = 0;
                }
                else if(str[i] == 0) { /* null character */
                        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "%s", buffer);
                        j = 0;
                }
                if(j == 4095) { /* don't overfill buffer */
                        buffer[j] = 0;
                        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "%s", buffer);
                        j = 0;
                }
        }
        if(j) { /* stuff left over */
                buffer[j] = 0;
                switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "%s", buffer);
        }
        return str_length;
}


void mod_php_error_handler(int type, const char *error_filename, const uint error_lineno, const char *format, va_list args)
{
        char *buffer;
        int buffer_len;
        TSRMLS_FETCH();

        buffer_len = vspprintf(&buffer, PG(log_errors_max_len), format, args);

        if((EG(error_reporting) & type || (type & E_CORE)) && (PG(log_errors) || PG(display_errors))) {
                char *error_type_str;

                switch (type) {
                        case E_ERROR:
                        case E_CORE_ERROR:
                        case E_COMPILE_ERROR:
                        case E_USER_ERROR:
                                error_type_str = "Fatal error";
                                break;
                        case E_WARNING:
                        case E_CORE_WARNING:
                        case E_COMPILE_WARNING:
                        case E_USER_WARNING:
                                error_type_str = "Warning";
                                break;
                        case E_PARSE:
                                error_type_str = "Parse error";
                                break;
                        case E_NOTICE:
                        case E_USER_NOTICE:
                                error_type_str = "Notice";
                                break;
                        default:
                                error_type_str = "Unknown error";
                                break;
                }

                if(PG(log_errors)) {
                        char *log_buffer;
#ifdef PHP_WIN32
                        if(type == E_CORE_ERROR || type == E_CORE_WARNING) {
                                MessageBox(NULL, buffer, error_type_str, MB_OK|ZEND_SERVICE_MB_STYLE);
                        }
#endif
                        spprintf(&log_buffer, 0, "PHP %s:  %s in %s on line %d", error_type_str, buffer, error_filename, error_lineno);
                        php_log_err(log_buffer TSRMLS_CC);
                        efree(log_buffer);
                }

                if(PG(display_errors)) {
                        char *prepend_string = INI_STR("error_prepend_string");
                        char *append_string = INI_STR("error_append_string");
                        char *error_format = "%s\n%s: %s in %s on line %d\n%s";
                        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, error_format, STR_PRINT(prepend_string), error_type_str, buffer, error_filename, error_lineno, 
STR_PRINT(append_string));
                }
        }

        /* Bail out if we can't recover */
        switch(type) {
                case E_CORE_ERROR:
                case E_ERROR:
                /*case E_PARSE: the parser would return 1 (failure), we can bail out nicely */
                case E_COMPILE_ERROR:
                case E_USER_ERROR:
                        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "\nPHP: %s exiting\n", error_filename);
                        EG(exit_status) = 255;
#if MEMORY_LIMIT
                        /* restore memory limit */
                        AG(memory_limit) = PG(memory_limit);
#endif
                        efree(buffer);
                        zend_bailout();
                        return;
                        break;
        }

        /* Log if necessary */
        if(PG(track_errors) && EG(active_symbol_table)) {
                pval *tmp;

                ALLOC_ZVAL(tmp);
                INIT_PZVAL(tmp);
                Z_STRVAL_P(tmp) = (char *) estrndup(buffer, buffer_len);
                Z_STRLEN_P(tmp) = buffer_len;
                Z_TYPE_P(tmp) = IS_STRING;
                zend_hash_update(EG(active_symbol_table), "php_errormsg", sizeof("php_errormsg"), (void **) & tmp, sizeof(pval *), NULL);
        }
        efree(buffer);
}

static int sapi_mod_php_header_handler(sapi_header_struct * sapi_header, sapi_headers_struct * sapi_headers TSRMLS_DC)
{
        return 0;
}

static int sapi_mod_php_send_headers(sapi_headers_struct * sapi_headers TSRMLS_DC)
{
        return SAPI_HEADER_SENT_SUCCESSFULLY;
}

static int sapi_mod_php_read_post(char *buffer, uint count_bytes TSRMLS_DC)
{
        return 0;
}

static int mod_php_startup(sapi_module_struct *sapi_module)
{
        if(php_module_startup(sapi_module, &mod_php_module_entry, 1) == FAILURE) {
                return FAILURE;
        }
        return SUCCESS;
}

static void mod_php_log_message(char *message)
{
         switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "%s\n", message);
}


static char *sapi_mod_php_read_cookies(TSRMLS_D)
{
        return NULL;
}

static int mod_php_startup(sapi_module_struct *sapi_module);

sapi_module_struct mod_php_sapi_module = {
   "mod_php",                                  /* name */
   "mod_php",                                  /* pretty name */

   mod_php_startup,                        /* startup */
   NULL,                 /* shutdown */

   NULL,                                        /* activate */
   NULL,                                        /* deactivate */

   sapi_mod_php_ub_write,                      /* unbuffered write */
   NULL,                                        /* flush */
   NULL,                                        /* get uid */
   NULL,                                        /* getenv */

   php_error,                                   /* error handler */

   sapi_mod_php_header_handler,                /* header handler */
   sapi_mod_php_send_headers,                  /* send headers handler */
   NULL,                                        /* send header handler */

   sapi_mod_php_read_post,                     /* read POST data */
   sapi_mod_php_read_cookies,                  /* read Cookies */

   NULL,					/* register server variables */
   mod_php_log_message,                        /* Log message */
   NULL,                                        /* Get request time */

   NULL,                                        /* Block interruptions */
   NULL,                                        /* Unblock interruptions */

   STANDARD_SAPI_MODULE_PROPERTIES
};

SWITCH_MOD_DECLARE(switch_status_t) switch_module_load(const switch_loadable_module_interface_t **module_interface, char *filename)
{
        /* connect my internal structure to the blank pointer passed to me */
        *module_interface = &php_module_interface;

        sapi_startup(&mod_php_sapi_module);
        mod_php_sapi_module.startup(&mod_php_sapi_module);


        //switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "Hello World!\n");

        /* indicate that the module should continue to be loaded */
        return SWITCH_STATUS_SUCCESS;
}
