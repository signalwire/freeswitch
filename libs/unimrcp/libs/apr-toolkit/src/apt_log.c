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

#include <apr_time.h>
#include <apr_file_io.h>
#include "apt_log.h"

#define MAX_LOG_ENTRY_SIZE 4096
#define MAX_PRIORITY_NAME_LENGTH 9

static const char priority_snames[APT_PRIO_COUNT][MAX_PRIORITY_NAME_LENGTH+1] =
{
	"[EMERG]  ",
	"[ALERT]  ",
	"[CRITIC] ",
	"[ERROR]  ",
	"[WARN]   ",
	"[NOTICE] ",
	"[INFO]   ",
	"[DEBUG]  "
};

typedef struct apt_log_file_data_t apt_log_file_data_t;

struct apt_log_file_data_t {
	const char           *log_dir_path;
	const char           *log_file_name;
	FILE                 *file;
	apr_size_t            cur_size;
	apr_size_t            max_size;
	apr_size_t            cur_file_index;
	apr_size_t            max_file_count;
	apr_thread_mutex_t   *mutex;
	apr_pool_t           *pool;
};

struct apt_logger_t {
	apt_log_output_e      mode;
	apt_log_priority_e    priority;
	int                   header;
	apt_log_ext_handler_f ext_handler;
	apt_log_file_data_t  *file_data;
};

static apt_logger_t *apt_logger = NULL;

static apt_bool_t apt_do_log(const char *file, int line, apt_log_priority_e priority, const char *format, va_list arg_ptr);

static const char* apt_log_file_path_make(apt_log_file_data_t *file_data);
static apt_bool_t apt_log_file_dump(apt_log_file_data_t *file_data, const char *log_entry, apr_size_t size);


APT_DECLARE(apt_bool_t) apt_log_instance_create(apt_log_output_e mode, apt_log_priority_e priority, apr_pool_t *pool)
{
	if(apt_logger) {
		return FALSE;
	}
	apt_logger = apr_palloc(pool,sizeof(apt_logger_t));
	apt_logger->mode = mode;
	apt_logger->priority = priority;
	apt_logger->header = APT_LOG_HEADER_DEFAULT;
	apt_logger->ext_handler = NULL;
	apt_logger->file_data = NULL;
	return TRUE;
}

APT_DECLARE(apt_bool_t) apt_log_instance_destroy()
{
	if(!apt_logger) {
		return FALSE;
	}

	if(apt_logger->file_data) {
		apt_log_file_close();
	}
	apt_logger = NULL;
	return TRUE;
}

APT_DECLARE(apt_logger_t*) apt_log_instance_get()
{
	return apt_logger;
}

APT_DECLARE(apt_bool_t) apt_log_instance_set(apt_logger_t *logger)
{
	if(apt_logger){
		return FALSE;
	}
	apt_logger = logger;
	return TRUE;
}

APT_DECLARE(apt_bool_t) apt_log_file_open(const char *dir_path, const char *file_name, apr_size_t max_file_size, apr_size_t max_file_count, apr_pool_t *pool)
{
	const char *log_file_path;
	apt_log_file_data_t *file_data;
	if(!apt_logger || !dir_path || !file_name) {
		return FALSE;
	}

	if(apt_logger->file_data) {
		return FALSE;
	}

	file_data = apr_palloc(pool,sizeof(apt_log_file_data_t));
	file_data->log_dir_path = dir_path;
	file_data->log_file_name = file_name;
	file_data->cur_file_index = 0;
	file_data->cur_size = 0;
	file_data->max_file_count = max_file_count;
	file_data->max_size = max_file_size;
	file_data->mutex = NULL;
	file_data->pool = pool;

	if(!file_data->max_size) {
		file_data->max_file_count = MAX_LOG_FILE_SIZE;
	}
	if(!file_data->max_file_count) {
		file_data->max_file_count = MAX_LOG_FILE_COUNT;
	}

	/* create mutex */
	if(apr_thread_mutex_create(&file_data->mutex,APR_THREAD_MUTEX_DEFAULT,pool) != APR_SUCCESS) {
		return FALSE;
	}
	/* open log file */
	log_file_path = apt_log_file_path_make(file_data);
	file_data->file = fopen(log_file_path,"wb");
	if(!file_data->file) {
		apr_thread_mutex_destroy(file_data->mutex);
		return FALSE;
	}

	apt_logger->file_data = file_data;
	return TRUE;
}

APT_DECLARE(apt_bool_t) apt_log_file_close()
{
	apt_log_file_data_t *file_data;
	if(!apt_logger || !apt_logger->file_data) {
		return FALSE;
	}
	file_data = apt_logger->file_data;
	if(file_data->file) {
		/* close log file */
		fclose(file_data->file);
		file_data->file = NULL;
		/* destroy mutex */
		apr_thread_mutex_destroy(file_data->mutex);
		file_data->mutex = NULL;
	}
	apt_logger->file_data = NULL;
	return TRUE;
}

APT_DECLARE(apt_bool_t) apt_log_output_mode_set(apt_log_output_e mode)
{
	if(!apt_logger) {
		return FALSE;
	}
	apt_logger->mode = mode;
	return TRUE;
}

APT_DECLARE(apt_bool_t) apt_log_priority_set(apt_log_priority_e priority)
{
	if(!apt_logger || priority >= APT_PRIO_COUNT) {
		return FALSE;
	}
	apt_logger->priority = priority;
	return TRUE;
}

APT_DECLARE(apt_bool_t) apt_log_header_set(int header)
{
	if(!apt_logger) {
		return FALSE;
	}
	apt_logger->header = header;
	return TRUE;
}

APT_DECLARE(apt_bool_t) apt_log_ext_handler_set(apt_log_ext_handler_f handler)
{
	if(!apt_logger) {
		return FALSE;
	}
	apt_logger->ext_handler = handler;
	return TRUE;
}

APT_DECLARE(apt_bool_t) apt_log(const char *file, int line, apt_log_priority_e priority, const char *format, ...)
{
	apt_bool_t status = TRUE;
	if(!apt_logger) {
		return FALSE;
	}
	if(priority <= apt_logger->priority) {
		va_list arg_ptr;
		va_start(arg_ptr, format);
		if(apt_logger->ext_handler) {
			status = apt_logger->ext_handler(file,line,NULL,priority,format,arg_ptr);
		}
		else {
			status = apt_do_log(file,line,priority,format,arg_ptr);
		}
		va_end(arg_ptr); 
	}
	return status;
}

static apt_bool_t apt_do_log(const char *file, int line, apt_log_priority_e priority, const char *format, va_list arg_ptr)
{
	char log_entry[MAX_LOG_ENTRY_SIZE];
	apr_size_t max_size = MAX_LOG_ENTRY_SIZE - 2;
	apr_size_t offset = 0;
	apr_time_exp_t result;
	apr_time_t now = apr_time_now();
	apr_time_exp_lt(&result,now);

	if(apt_logger->header & APT_LOG_HEADER_DATE) {
		offset += apr_snprintf(log_entry+offset,max_size-offset,"%4d-%02d-%02d ",
							result.tm_year+1900,
							result.tm_mon+1,
							result.tm_mday);
	}
	if(apt_logger->header & APT_LOG_HEADER_TIME) {
		offset += apr_snprintf(log_entry+offset,max_size-offset,"%02d:%02d:%02d:%06d ",
							result.tm_hour,
							result.tm_min,
							result.tm_sec,
							result.tm_usec);
	}
	if(apt_logger->header & APT_LOG_HEADER_MARK) {
		offset += apr_snprintf(log_entry+offset,max_size-offset,"%s:%03d ",file,line);
	}
	if(apt_logger->header & APT_LOG_HEADER_PRIORITY) {
		memcpy(log_entry+offset,priority_snames[priority],MAX_PRIORITY_NAME_LENGTH);
		offset += MAX_PRIORITY_NAME_LENGTH;
	}

	offset += apr_vsnprintf(log_entry+offset,max_size-offset,format,arg_ptr);
	log_entry[offset++] = '\n';
	log_entry[offset] = '\0';
	if((apt_logger->mode & APT_LOG_OUTPUT_CONSOLE) == APT_LOG_OUTPUT_CONSOLE) {
		fwrite(log_entry,offset,1,stdout);
	}
	
	if((apt_logger->mode & APT_LOG_OUTPUT_FILE) == APT_LOG_OUTPUT_FILE && apt_logger->file_data) {
		apt_log_file_dump(apt_logger->file_data,log_entry,offset);
	}
	return TRUE;
}

static const char* apt_log_file_path_make(apt_log_file_data_t *file_data)
{
	char *log_file_path = NULL;
	const char *log_file_name = apr_psprintf(file_data->pool,"%s-%"APR_SIZE_T_FMT".log",file_data->log_file_name,file_data->cur_file_index);
	apr_filepath_merge(&log_file_path,file_data->log_dir_path,log_file_name,0,file_data->pool);
	return log_file_path;
}

static apt_bool_t apt_log_file_dump(apt_log_file_data_t *file_data, const char *log_entry, apr_size_t size)
{
	apr_thread_mutex_lock(file_data->mutex);

	file_data->cur_size += size;
	if(file_data->cur_size > file_data->max_size) {
		const char *log_file_path;
		/* close current log file */
		fclose(file_data->file);
		/* roll over the next log file */
		file_data->cur_file_index++;
		file_data->cur_file_index %= file_data->max_file_count;
		/* open log file */
		log_file_path = apt_log_file_path_make(file_data);
		file_data->file = fopen(log_file_path,"wb");
		if(!file_data->file) {
			return FALSE;
		}

		file_data->cur_size = size;
	}
	/* write to log file */
	fwrite(log_entry,1,size,file_data->file);
	fflush(file_data->file);

	apr_thread_mutex_unlock(file_data->mutex);
	return TRUE;
}
