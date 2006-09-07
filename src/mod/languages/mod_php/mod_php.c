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
 * Alex Leigh <php (at) postfin (dot) com>
 *
 *
 * mod_php.c -- PHP Module
 *
 */
#include <switch.h>
#include "php.h"
#include "php_variables.h"
#include "ext/standard/info.h"
#include "php_ini.h"
#include "php_globals.h"
#include "SAPI.h"
#include "php_main.h"
#include "php_version.h"
#include "TSRM.h"
#include "ext/standard/php_standard.h"

const char modname[] = "mod_php";

static int php_freeswitch_startup(sapi_module_struct *sapi_module)
{
	return SUCCESS;
}

static int sapi_freeswitch_ub_write(const char *str, unsigned int str_length TSRMLS_DC)
{
	
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, (char *)str);
	return str_length;
}


/*
 * sapi_freeswitch_header_handler: Add/update response headers with those provided by the PHP engine.
 */
static int sapi_freeswitch_header_handler(sapi_header_struct * sapi_header, sapi_headers_struct * sapi_headers TSRMLS_DC)
{
	return 0;
}

/*
 * sapi_freeswitch_send_headers: Transmit the headers to the client. This has the
 * effect of starting the response under freeswitch.
 */
static int sapi_freeswitch_send_headers(sapi_headers_struct * sapi_headers TSRMLS_DC)
{
	return SAPI_HEADER_SENT_SUCCESSFULLY;
}

static int sapi_freeswitch_read_post(char *buffer, uint count_bytes TSRMLS_DC)
{
	return 0;
}

/*
 * sapi_freeswitch_read_cookies: Return cookie information into PHP.
 */
static char *sapi_freeswitch_read_cookies(TSRMLS_D)
{
	return NULL;
}

static void sapi_freeswitch_register_server_variables(zval * track_vars_array TSRMLS_DC)
{
}

static void freeswitch_log_message(char *message)
{
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, message);
}


sapi_module_struct fs_sapi_module = {
   "freeswitch",					/* name */
   "FreeSWITCH",					/* pretty name */

   php_freeswitch_startup,			/* startup */
   php_module_shutdown_wrapper,			/* shutdown */

   NULL,					/* activate */
   NULL,					/* deactivate */

   sapi_freeswitch_ub_write,			/* unbuffered write */
   NULL,					/* flush */
   NULL,					/* get uid */
   NULL,					/* getenv */

   php_error,					/* error handler */

   sapi_freeswitch_header_handler,		/* header handler */
   sapi_freeswitch_send_headers,			/* send headers handler */
   NULL,					/* send header handler */

   sapi_freeswitch_read_post,			/* read POST data */
   sapi_freeswitch_read_cookies,			/* read Cookies */

   sapi_freeswitch_register_server_variables,	/* register server variables */
   freeswitch_log_message,			/* Log message */
   NULL,					/* Get request time */

   NULL,					/* Block interruptions */
   NULL,					/* Unblock interruptions */

   STANDARD_SAPI_MODULE_PROPERTIES
};



typedef struct switch_php_obj {
	switch_core_session_t *session;
	int argc;
	char *argv[129];
} switch_php_obj_t;



void freeswitch_error_handler(int type, const char *error_filename, const uint error_lineno, const char *format, va_list args)
{
	char *buffer;
	int buffer_len;
	TSRMLS_FETCH();

	buffer_len = vspprintf(&buffer, PG(log_errors_max_len), format, args);

	/* display/log the error if necessary */
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
			//char *prepend_string = INI_STR("error_prepend_string");
			char *append_string = INI_STR("error_append_string");
			//char *error_format = "%s\n%s: %s in %s on line %d\n%s";    
			switch_log_printf(SWITCH_CHANNEL_ID_LOG_CLEAN, (char *) error_filename, buffer, error_lineno, SWITCH_LOG_DEBUG, STR_PRINT(append_string));
		}
	}

	/* Bail out if we can't recover */
	switch(type) {
		case E_CORE_ERROR:
		case E_ERROR:
		/*case E_PARSE: the parser would return 1 (failure), we can bail out nicely */
		case E_COMPILE_ERROR:
		case E_USER_ERROR:
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "\nPHP: %s exiting\n", error_filename);
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



static void freeswitch_request_ctor(switch_php_obj_t *request_context TSRMLS_DC)
{
/*	ast_register_string_constant("JELLO", "FISH"); */

	zend_error_cb = freeswitch_error_handler;

	SG(request_info).argc = request_context->argc;
	SG(request_info).argv = request_context->argv;

	SG(request_info).path_translated    = estrdup(request_context->argv[0]);
}

static void freeswitch_request_dtor(switch_php_obj_t *request_context TSRMLS_DC)
{
	efree(SG(request_info).path_translated);
}

int freeswitch_module_main(switch_php_obj_t *request_context TSRMLS_DC)
{
	int retval;
	zend_file_handle file_handle;

	if(php_request_startup(TSRMLS_C) == FAILURE) {
		return FAILURE;
	}

	file_handle.type = ZEND_HANDLE_FILENAME;
	file_handle.filename = SG(request_info).path_translated;
	file_handle.free_filename = 0;
	file_handle.opened_path = NULL;

	retval = php_execute_script(&file_handle TSRMLS_CC);
	php_request_shutdown(NULL);

	return retval;
}

static void php_function(switch_core_session_t *session, char *data)
{
	char *uuid = switch_core_session_get_uuid(session);
	switch_php_obj_t *request_context;
	uint32_t ulen = strlen(uuid);
	uint32_t len = strlen((char *) data) + ulen + 2;
	char *mydata = switch_core_session_alloc(session, len);

	switch_copy_string(mydata, uuid, len);
	snprintf(mydata + ulen, len - ulen, " %s", data);
	
	TSRMLS_FETCH();

	request_context = (switch_php_obj_t *) switch_core_session_alloc(session, sizeof(*request_context));

	request_context->session = session;

	if(switch_strlen_zero(data)) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Invalid Params\n");
		return;
	}

	request_context->argc = switch_separate_string(mydata, ' ', 
												   request_context->argv, 
												   (sizeof(request_context->argv) / sizeof(request_context->argv[0])));
	

	SG(server_context) = request_context;

	freeswitch_request_ctor(request_context TSRMLS_CC);
	freeswitch_module_main(request_context TSRMLS_CC);
	freeswitch_request_dtor(request_context TSRMLS_CC);

	/*
	 * This call is ostensibly provided to free the memory from PHP/TSRM when
	 * the thread terminated, but, it leaks a structure in some hash list
	 * according to the developers. Not calling this will leak the entire
	 * interpreter, around 100k, but calling it and then terminating the
	 * thread will leak the struct (around a k). The only answer with the
	 * current TSRM implementation is to reuse the threads that allocate TSRM
	 * resources.
	 */
	/* ts_free_thread(); */

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

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "Hello World!\n");

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
