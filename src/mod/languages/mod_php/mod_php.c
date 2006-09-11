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


#ifndef _REENTRANT
#define _REENTRANT
#endif

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <sapi/embed/php_embed.h>

#ifdef ZTS
        zend_compiler_globals *compiler_globals;
        zend_executor_globals *executor_globals;
        php_core_globals *core_globals;
        sapi_globals_struct *sapi_globals;
#endif


#include <switch.h>

const char modname[] = "mod_php";

static int sapi_mod_php_ub_write(const char *str, unsigned int str_length TSRMLS_DC)
{

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
                        spprintf(&log_buffer, 0, "PHP %s:  %s in %s on line %d", error_type_str, buffer, error_filename, error_lineno);
                        php_log_err(log_buffer TSRMLS_CC);
                        efree(log_buffer);
                }

                if(PG(display_errors)) {
                        char *prepend_string = INI_STR("error_prepend_string");
                        char *append_string = INI_STR("error_append_string");
                        char *error_format = "%s\n%s: %s in %s on line %d\n%s";
                        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, error_format, STR_PRINT(prepend_string), error_type_str, buffer, error_filename, error_lineno, STR_PRINT(append_string));
                }
        }

        // Bail out if we can't recover 
        switch(type) {
                case E_CORE_ERROR:
                case E_ERROR:
                //case E_PARSE: the parser would return 1 (failure), we can bail out nicely 
                case E_COMPILE_ERROR:
                case E_USER_ERROR:
                        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "\nPHP: %s exiting\n", error_filename);
                        EG(exit_status) = 255;
#if MEMORY_LIMIT
                        // restore memory limit 
                        AG(memory_limit) = PG(memory_limit);
#endif
                        efree(buffer);
                        zend_bailout();
                        return;

        }

        // Log if necessary 
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


static void mod_php_log_message(char *message)
{
         switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "%s\n", message);
}

typedef void (*sapi_error_function_t)(int type, const char *error_msg, ...);



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

        argc = switch_separate_string(mydata, ' ',argv,(sizeof(argv) / sizeof(argv[0])));

        sprintf(php_code, "uuid=\"%s\"; include(\"%s\");\n", argv[0], argv[1]);
        //sprintf(php_code, "include('%s');", argv[1]);
	
	sprintf(php_code, "%s %s", data, uuid);

        zend_file_handle script;
        script.type = ZEND_HANDLE_FP;
        script.filename = data;
        script.opened_path = NULL;
        script.free_filename = 0;
        script.handle.fp = fopen(script.filename, "rb");

	// Initialize PHPs CORE
	php_embed_init(argc, argv, &tsrm_ls);

	// Return All of the DEBUG crap to the console and/or a log file
        php_embed_module.ub_write = sapi_mod_php_ub_write;
        php_embed_module.log_message = mod_php_log_message;
        php_embed_module.sapi_error = (sapi_error_function_t)mod_php_error_handler;

	// Let the nice people know we are about to start their script
        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "Starting Script %s\n",data);

	// Force $uuid and $session to exist in PHPs memory space
	zval *php_uuid;
	MAKE_STD_ZVAL(php_uuid);
	//MAKE_STD_ZVAL(php_session);
	//php_uuid->type = IS_STRING;
	//php_uuid->value.str.len = strlen(uuid);
	//php_uuid->value.str.val = estrdup(uuid);
	ZVAL_STRING(php_uuid, uuid , 1);
	//ZVAL_STRING(php_session, session , 1);
	ZEND_SET_SYMBOL(&EG(symbol_table), "uuid", php_uuid);
	//ZEND_SET_SYMBOL(&EG(active_symbol_table), "session", php_session);

	// Force Some INI entries weather the user likes it or not
	zend_alter_ini_entry("register_globals",sizeof("register_globals"),"1", sizeof("1") - 1, PHP_INI_SYSTEM, PHP_INI_STAGE_RUNTIME);

	// Execute the bloody script
        retval = php_execute_script(&script TSRMLS_CC);

	// Clean up after PHP and such
        php_embed_shutdown(tsrm_ls);



	// Return back to the Dialplan
        
// Buh bye now!
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
void*** tsrm_ls = NULL;

        /* connect my internal structure to the blank pointer passed to me */
        *module_interface = &php_module_interface;

        //switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "Hello World!\n");

#ifdef ZTS
	tsrm_startup(1, 1, 0, NULL);
	compiler_globals = ts_resource(compiler_globals_id);
	executor_globals = ts_resource(executor_globals_id);
	core_globals = ts_resource(core_globals_id);
	sapi_globals = ts_resource(sapi_globals_id);
	tsrm_ls = ts_resource(0);
#endif

        /* indicate that the module should continue to be loaded */
        return SWITCH_STATUS_SUCCESS;
}

/*
  //Called when the system shuts down
  SWITCH_MOD_DECLARE(switch_status) switch_module_shutdown(void)
  {
  return SWITCH_STATUS_SUCCESS;
  }



  //If it exists, this is called in it's own thread when the module-load completes
  SWITCH_MOD_DECLARE(switch_status) switch_module_shutdown(void)
  {
  return SWITCH_STATUS_SUCCESS;
  }


*/
