/*
 * Copyright 2008-2015 Arsen Chaloyan
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

#ifndef APT_LOG_H
#define APT_LOG_H

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
/** Default max number of log files used in rotation */
#define MAX_LOG_FILE_COUNT 100

/** Opaque log source declaration */
typedef struct apt_log_source_t apt_log_source_t;

/** Declaration of log mark to be used by custom log sources */
#define APT_LOG_MARK_DECLARE(LOG_SOURCE)   LOG_SOURCE,__FILE__,__LINE__

/** Use this macro in a header file to declare a custom log source */
#define APT_LOG_SOURCE_DECLARE(SCOPE,LOG_SOURCE) \
	extern apt_log_source_t *LOG_SOURCE; \
	SCOPE##_DECLARE(void) LOG_SOURCE##_init();

/** Use this macro in a source file to implement a custom log source */
#define APT_LOG_SOURCE_IMPLEMENT(SCOPE, LOG_SOURCE, LOG_SOURCE_TAG) \
	apt_log_source_t *LOG_SOURCE = &def_log_source; \
	SCOPE##_DECLARE(void) LOG_SOURCE##_init() {apt_log_source_assign(LOG_SOURCE_TAG,&LOG_SOURCE);}

/** Default (globally available) log source */
extern apt_log_source_t def_log_source;
/** Default log mark providing log source, file and line information */
#define APT_LOG_MARK   APT_LOG_MARK_DECLARE(&def_log_source)

/*
 * Definition of common formats used with apt_log().
 *
 * Note that the generic %p format can not be used for pointers
 * since apr_vformatter doesn't accept it. The format %pp introduced
 * by apr_vformatter can not be used either since it breaks compatibility
 * with generic printf style loggers.
 */
#if defined(WIN32) && APR_SIZEOF_VOIDP == 8
/** Format to log pointer values on Win x64 */
#define APT_PTR_FMT       "0x%I64x"
#else
/** Format to log pointer values */
#define APT_PTR_FMT       "0x%lx"
#endif
/** Format to log string identifiers */
#define APT_SID_FMT       "<%s>"
/** Format to log string identifiers and resources */
#define APT_SIDRES_FMT    "<%s@%s>"
/** Format to log pointers and identifiers */
#define APT_PTRSID_FMT    APT_PTR_FMT " " APT_SID_FMT
/** Format to log names and identifiers */
#define APT_NAMESID_FMT   "%s " APT_SID_FMT
/** Format to log names, identifiers and resources */
#define APT_NAMESIDRES_FMT "%s " APT_SIDRES_FMT

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
	APT_LOG_HEADER_THREAD   = 0x10, /**< enable thread identifier output */

	APT_LOG_HEADER_DEFAULT  = APT_LOG_HEADER_DATE | APT_LOG_HEADER_TIME | APT_LOG_HEADER_PRIORITY
} apt_log_header_e;

/** Mode of log output */
typedef enum {
	APT_LOG_OUTPUT_NONE     = 0x00, /**< disable logging */
	APT_LOG_OUTPUT_CONSOLE  = 0x01, /**< enable console output */
	APT_LOG_OUTPUT_FILE     = 0x02, /**< enable log file output */
	APT_LOG_OUTPUT_SYSLOG   = 0x04  /**< enable syslog output */
} apt_log_output_e;

/** Masking mode of private data */
typedef enum {
	APT_LOG_MASKING_NONE,      /**< log everything as is */
	APT_LOG_MASKING_COMPLETE,  /**< mask private data completely */
	APT_LOG_MASKING_ENCRYPTED  /**< encrypt private data */
} apt_log_masking_e;

/** Opaque logger declaration */
typedef struct apt_logger_t apt_logger_t;

/** Prototype of extended log handler function */
typedef apt_bool_t (*apt_log_ext_handler_f)(const char *file, int line,
											const char *obj, apt_log_priority_e priority,
											const char *format, va_list arg_ptr);

/**
 * Create the singleton instance of the logger.
 * @param mode the log output mode
 * @param priority the log priority level
 * @param pool the memory pool to use
 */
APT_DECLARE(apt_bool_t) apt_log_instance_create(apt_log_output_e mode, apt_log_priority_e priority, apr_pool_t *pool);

/**
 * Create and load the singleton instance of the logger.
 * @param config_file the path to configuration file to load settings from
 * @param pool the memory pool to use
 */
APT_DECLARE(apt_bool_t) apt_log_instance_load(const char *config_file, apr_pool_t *pool);

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
 * @param logger the logger to set
 */
APT_DECLARE(apt_bool_t) apt_log_instance_set(apt_logger_t *logger);

/**
 * Set the default log source.
 * @param log_source the log source to set
 */
APT_DECLARE(void) apt_def_log_source_set(apt_log_source_t *log_source);

/**
 * Find and assign log source by its name.
 * @param name the unique name associated to the log source
 * @param log_source the log source to be returned, if found
 */
APT_DECLARE(apt_bool_t) apt_log_source_assign(const char *name, apt_log_source_t **log_source);

/**
 * Open the log file.
 * @param dir_path the path to the log directory
 * @param file_name the name of the log file
 * @param max_file_size the max size of the log file
 * @param max_file_count the max number of files used in log rotation
 * @param append whether to append or to truncate (start over) the log file
 * @param pool the memory pool to use
 * @deprecated @see apt_log_file_open_ex()
 */
APT_DECLARE(apt_bool_t) apt_log_file_open(
							const char *dir_path,
							const char *file_name,
							apr_size_t max_file_size,
							apr_size_t max_file_count,
							apt_bool_t append,
							apr_pool_t *pool);

/**
 * Open the log file (extended version).
 * @param dir_path the path to the log directory
 * @param prefix the prefix used to compose the log file name
 * @param config_file the path to configuration file to load settings from
 * @param pool the memory pool to use
 */
APT_DECLARE(apt_bool_t) apt_log_file_open_ex(const char *dir_path, const char *prefix, const char *config_file, apr_pool_t *pool);

/**
 * Close the log file.
 */
APT_DECLARE(apt_bool_t) apt_log_file_close(void);

/**
 * Open the syslog.
 * @param prefix the prefix used to compose the log file name
 * @param config_file the path to configuration file to load settings from
 * @param pool the memory pool to use
 */
APT_DECLARE(apt_bool_t) apt_syslog_open(const char *prefix, const char *config_file, apr_pool_t *pool);

/**
 * Close the syslog.
 */
APT_DECLARE(apt_bool_t) apt_syslog_close(void);

/**
 * Set the logging output mode.
 * @param mode the mode to set
 */
APT_DECLARE(apt_bool_t) apt_log_output_mode_set(apt_log_output_e mode);

/**
 * Check the logging output mode to be enabled (set) or not.
 * @param mode the mode to check
 */
APT_DECLARE(apt_bool_t) apt_log_output_mode_check(apt_log_output_e mode);

/**
 * Translate the output mode string to bitmask of apt_log_output_e values.
 * @param str the string to translate
 */
APT_DECLARE(int) apt_log_output_mode_translate(char *str);

/**
 * Set the logging priority (log level).
 * @param priority the priority to set
 */
APT_DECLARE(apt_bool_t) apt_log_priority_set(apt_log_priority_e priority);

/**
 * Translate the priority (log level) string to enum.
 * @param str the string to translate
 */
APT_DECLARE(apt_log_priority_e) apt_log_priority_translate(const char *str);

/**
 * Set the header (format) for log messages.
 * @param header the header to set (used as bitmask)
 */
APT_DECLARE(apt_bool_t) apt_log_header_set(int header);

/**
 * Translate the header string to bitmask of apt_log_header_e values.
 * @param str the string to translate
 */
APT_DECLARE(int) apt_log_header_translate(char *str);

/**
 * Set the masking mode of private data.
 * @param masking the masking mode to set
 */
APT_DECLARE(apt_bool_t) apt_log_masking_set(apt_log_masking_e masking);

/**
 * Get the current masking mode of private data.
 */
APT_DECLARE(apt_log_masking_e) apt_log_masking_get(void);

/**
 * Translate the masking mode string to enum.
 * @param str the string to translate
 */
APT_DECLARE(apt_log_masking_e) apt_log_masking_translate(const char *str);

/**
 * Mask private data based on the masking mode.
 * @param data_in the data to mask
 * @param length the length of the data to mask on input, the length of the masked data on output
 * @param pool the memory pool to use if needed
 * @return The masked data.
 */
APT_DECLARE(const char*) apt_log_data_mask(const char *data_in, apr_size_t *length, apr_pool_t *pool);

/**
 * Set the extended external log handler.
 * @param handler the handler to pass log events to
 * @remark default logger is used to output the logs to stdout and/or log file,
 *         if external log handler isn't set
 */
APT_DECLARE(apt_bool_t) apt_log_ext_handler_set(apt_log_ext_handler_f handler);

/**
 * Do logging.
 * @param log_source the log source
 * @param file the file name log entry is generated from
 * @param line the line number log entry is generated from
 * @param priority the priority of the entire log entry
 * @param format the format of the entire log entry
 */
APT_DECLARE(apt_bool_t) apt_log(apt_log_source_t *log_source, const char *file, int line, apt_log_priority_e priority, const char *format, ...);

/**
 * Do logging (this version uses an object externally associated with the logger).
 * @param log_source the log source
 * @param file the file name log entry is generated from
 * @param line the line number log entry is generated from
 * @param priority the priority of the entire log entry
 * @param obj the associated object
 * @param format the format of the entire log entry
 */
APT_DECLARE(apt_bool_t) apt_obj_log(apt_log_source_t *log_source, const char *file, int line, apt_log_priority_e priority, void *obj, const char *format, ...);

/**
 * Do logging (this version accepts va_list argument).
 * @param log_source the log source
 * @param file the file name log entry is generated from
 * @param line the line number log entry is generated from
 * @param priority the priority of the entire log entry
 * @param format the format of the entire log entry
 * @param arg_ptr the arguments
 */
APT_DECLARE(apt_bool_t) apt_va_log(apt_log_source_t *log_source, const char *file, int line, apt_log_priority_e priority, const char *format, va_list arg_ptr);

APT_END_EXTERN_C

#endif /* APT_LOG_H */
