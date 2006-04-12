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
 * switch_log.h -- Logger
 *
 */
/*! \file switch_log.h
    \brief Simple Log

	Logging Routines
*/

#ifndef SWITCH_LOG_H
#define SWITCH_LOG_H

#ifdef __cplusplus
extern "C" {
#endif
#ifdef _FORMATBUG
}
#endif

#include <switch.h>

typedef struct {
	char *data;
	char *file;
	char *func;
	char *content;
	uint32_t line;
	switch_log_level level;
	switch_time_t timestamp;
} switch_log_node;

typedef switch_status (*switch_log_function)(const switch_log_node *node, switch_log_level level);

/*!
  \brief A method akin to printf that allows you to redirect output to a specific log
*/

///\defgroup log Logger Routines
///\ingroup core1
///\{

/*! 
  \brief Initilize the logging engine
  \param pool the memory pool to use
  \note to be called at application startup by the core
*/
SWITCH_DECLARE(switch_status) switch_log_init(switch_memory_pool *pool);

/*! 
  \brief Shut down the logging engine
  \note to be called at application termination by the core
*/
SWITCH_DECLARE(switch_status) switch_log_shutdown(void);


/*! 
  \brief Write log data to the logging engine
  \param channel the log channel to write to
  \param file the current file
  \param func the current function
  \param line the current line
  \param level the current log level
  \param fmt desired format
  \param ... variable args
  \note there are channel macros to supply the first 4 parameters
*/
SWITCH_DECLARE(void) switch_log_printf(switch_text_channel channel, char *file, const char *func, int line, switch_log_level level, char *fmt, ...);

/*! 
  \brief Shut down  the logging engine
  \note to be called at application termination by the core
*/
SWITCH_DECLARE(switch_status) switch_log_bind_logger(switch_log_function function, switch_log_level level);

///\}
#ifdef __cplusplus
}
#endif

#endif
