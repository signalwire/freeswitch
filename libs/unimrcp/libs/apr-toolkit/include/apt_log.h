/*
 * Copyright 2008 Arsen Chaloyan
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef __APT_LOG_H__
#define __APT_LOG_H__

/**
 * @file apt_log.h
 * @brief Basic Logger
 */ 

#include <stdio.h>
#include <stdarg.h>
#include "apt.h"

APT_BEGIN_EXTERN_C

/** Default max size of the log file (8Mb) */
#define MAX_LOG_FILE_SIZE (8 * 1024 * 1024)
/** Default max number of rotated log files */
#define MAX_LOG_FILE_COUNT 10

/** File:line mark */
#define APT_LOG_MARK	__FILE__,__LINE__

/** Format to log pointer values */
#define APT_PTR_FMT       "0x%x"
/** Format to log string identifiers */
#define APT_SID_FMT       "<%s>"
/** Format to log string identifiers and resources */
#define APT_SIDRES_FMT    "<%s@%s>"
/** Format to log pointers and identifiers */
#define APT_PTRSID_FMT    APT_PTR_FMT" "APT_SID_FMT
/** Format to log pointers, identifiers and resources */
#define APT_PTRSIDRES_FMT APT_PTR_FMT" "APT_SIDRES_FMT


/** Priority of log messages ordered from highest priority to lowest (rfc3164) */
typedef enum {
	APT_PRIO_EMERGENCY, /**< system is unusable */
	APT_PRIO_ALERT,     /**< action must be taken immediately */
	APT_PRIO_CRITICAL,  /**< critical condition */
	APT_PRIO_ERROR,     /**< error condition */
	APT_PRIO_WARNING,   /**< warning condition */
	APT_PRIO_NOTICE,    /**< normal, but significant condition */
	APT_PRIO_INFO,      /**< informational message */
	APT_PRIO_DEBUG,     /**< debug-level message */

	APT_PRIO_COUNT     	/**< number of priorities */
} apt_log_priority_e;

/** Header (format) of log messages */
typedef enum {
	APT_LOG_HEADER_NONE     = 0x00, /**< disable optional headers output */
	APT_LOG_HEADER_DATE     = 0x01, /**< enable date output */
	APT_LOG_HEADER_TIME     = 0x02, /**< enable time output */
	APT_LOG_HEADER_PRIORITY = 0x04, /**< enable priority name output */
	APT_LOG_HEADER_MARK     = 0x08, /**< enable file:line mark output */

	APT_LOG_HEADER_DEFAULT  = APT_LOG_HEADER_DATE | APT_LOG_HEADER_TIME | APT_LOG_HEADER_PRIORITY
} apt_log_header_e;

/** Log output modes */
typedef enum {
	APT_LOG_OUTPUT_NONE     = 0x00, /**< disable logging */
	APT_LOG_OUTPUT_CONSOLE  = 0x01, /**< enable console output */
	APT_LOG_OUTPUT_FILE     = 0x02  /**< enable log file output */
} apt_log_output_e;

/** Opaque logger declaration */
typedef struct apt_logger_t apt_logger_t;

/** Prototype of extended log handler function */
typedef apt_bool_t (*apt_log_ext_handler_f)(const char *file, int line, const char *id, 
											apt_log_priority_e priority, const char *format, va_list arg_ptr);

/**
 * Create the singleton instance of the logger.
 * @param mode the log output mode
 * @param priority the log priority level
 * @param pool the memory pool to use
 */
APT_DECLARE(apt_bool_t) apt_log_instance_create(apt_log_output_e mode, apt_log_priority_e priority, apr_pool_t *pool);

/**
 * Destroy the singleton instance of the logger.
 */
APT_DECLARE(apt_bool_t) apt_log_instance_destroy(void);

/**
 * Get the singleton instance of the logger.
 */
APT_DECLARE(apt_logger_t*) apt_log_instance_get(void);

/**
 * Set the singleton instance of the logger.
 */
APT_DECLARE(apt_bool_t) apt_log_instance_set(apt_logger_t *logger);

/**
 * Open the log file.
 * @param dir_path the path to the log directory
 * @param file_name the name of the log file
 * @param max_file_size the max size of the log file
 * @param max_file_count the max number of files used in log rotation
 * @param pool the memory pool to use
 */
APT_DECLARE(apt_bool_t) apt_log_file_open(
							const char *dir_path, 
							const char *file_name, 
							apr_size_t max_file_size, 
							apr_size_t max_file_count, 
							apr_pool_t *pool);

/**
 * Close the log file.
 */
APT_DECLARE(apt_bool_t) apt_log_file_close(void);

/**
 * Set the logging output.
 * @param mode the mode to set
 */
APT_DECLARE(apt_bool_t) apt_log_output_mode_set(apt_log_output_e mode);

/**
 * Set the logging priority (log level).
 * @param priority the priority to set
 */
APT_DECLARE(apt_bool_t) apt_log_priority_set(apt_log_priority_e priority);

/**
 * Set the header (format) for log messages.
 * @param header the header to set (used as bitmask)
 */
APT_DECLARE(apt_bool_t) apt_log_header_set(int header);

/**
 * Set the extended external log handler.
 * @param handler the handler to pass log events to
 * @remark default logger is used to output the logs to stdout and/or log file,
 *         if external log handler isn't set
 */
APT_DECLARE(apt_bool_t) apt_log_ext_handler_set(apt_log_ext_handler_f handler);

/**
 * Do logging.
 * @param file the file name log entry is generated from
 * @param line the line number log entry is generated from
 * @param priority the priority of the entire log entry
 * @param format the format of the entire log entry
 */
APT_DECLARE(apt_bool_t) apt_log(const char *file, int line, apt_log_priority_e priority, const char *format, ...);

APT_END_EXTERN_C

#endif /*__APT_LOG_H__*/
