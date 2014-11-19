/*
 * Copyright 2008-2014 Arsen Chaloyan
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
 * 
 * $Id: apt_log.c 2198 2014-10-16 01:41:19Z achaloyan@gmail.com $
 */

#include <apr_time.h>
#include <apr_file_io.h>
#include <apr_portable.h>
#include <apr_xml.h>
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
	apt_bool_t            append;
	apr_thread_mutex_t   *mutex;
	apr_pool_t           *pool;
};

struct apt_logger_t {
	apt_log_output_e      mode;
	apt_log_priority_e    priority;
	int                   header;
	apt_log_ext_handler_f ext_handler;
	apt_log_file_data_t  *file_data;
	apt_log_masking_e     masking;
};

static apt_logger_t *apt_logger = NULL;

static apt_bool_t apt_do_log(const char *file, int line, apt_log_priority_e priority, const char *format, va_list arg_ptr);

static const char* apt_log_file_path_make(apt_log_file_data_t *file_data);
static apt_bool_t apt_log_file_dump(apt_log_file_data_t *file_data, const char *log_entry, apr_size_t size);
static apr_xml_doc* apt_log_doc_parse(const char *file_path, apr_pool_t *pool);
static apr_size_t apt_log_file_get_size(apt_log_file_data_t *file_data);
static apr_byte_t apt_log_file_exist(apt_log_file_data_t *file_data);

static apt_logger_t* apt_log_instance_alloc(apr_pool_t *pool)
{
	apt_logger_t *logger = apr_palloc(pool,sizeof(apt_logger_t));
	logger->mode = APT_LOG_OUTPUT_CONSOLE;
	logger->priority = APT_PRIO_INFO;
	logger->header = APT_LOG_HEADER_DEFAULT;
	logger->ext_handler = NULL;
	logger->file_data = NULL;
	logger->masking = APT_LOG_MASKING_NONE;
	return logger;
}

APT_DECLARE(apt_bool_t) apt_log_instance_create(apt_log_output_e mode, apt_log_priority_e priority, apr_pool_t *pool)
{
	if(apt_logger) {
		return FALSE;
	}
	apt_logger = apt_log_instance_alloc(pool);
	apt_logger->mode = mode;
	apt_logger->priority = priority;
	return TRUE;
}

APT_DECLARE(apt_bool_t) apt_log_instance_load(const char *config_file, apr_pool_t *pool)
{
	apr_xml_doc *doc;
	const apr_xml_elem *elem;
	const apr_xml_elem *root;
	char *text;

	if(apt_logger) {
		return FALSE;
	}
	apt_logger = apt_log_instance_alloc(pool);

	/* Parse XML document */
	doc = apt_log_doc_parse(config_file,pool);
	if(!doc) {
		return FALSE;
	}

	root = doc->root;

	/* Match document name */
	if(!root || strcasecmp(root->name,"aptlogger") != 0) {
		/* Unknown document */
		return FALSE;
	}

	/* Navigate through document */
	for(elem = root->first_child; elem; elem = elem->next) {
		if(!elem->first_cdata.first || !elem->first_cdata.first->text) 
			continue;

		text = apr_pstrdup(pool,elem->first_cdata.first->text);
		apr_collapse_spaces(text,text);
		
		if(strcasecmp(elem->name,"priority") == 0) {
			apt_logger->priority = apt_log_priority_translate(text);
		}
		else if(strcasecmp(elem->name,"output") == 0) {
			apt_logger->mode = apt_log_output_mode_translate(text);
		}
		else if(strcasecmp(elem->name,"headers") == 0) {
			apt_logger->header = apt_log_header_translate(text);
		}
		else if(strcasecmp(elem->name,"masking") == 0) {
			apt_logger->masking = apt_log_masking_translate(text);
		}
		else {
			/* Unknown element */
		}
	}
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

APT_DECLARE(apt_bool_t) apt_log_file_open(
							const char *dir_path,
							const char *file_name,
							apr_size_t max_file_size,
							apr_size_t max_file_count,
							apt_bool_t append,
							apr_pool_t *pool)
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
	file_data->log_dir_path = apr_pstrdup(pool,dir_path);
	file_data->log_file_name = apr_pstrdup(pool,file_name);
	file_data->cur_file_index = 0;
	file_data->cur_size = 0;
	file_data->max_file_count = max_file_count;
	file_data->max_size = max_file_size;
	file_data->append = append;
	file_data->mutex = NULL;
	file_data->pool = pool;

	if(!file_data->max_size) {
		file_data->max_size = MAX_LOG_FILE_SIZE;
	}
	if(!file_data->max_file_count) {
		file_data->max_file_count = MAX_LOG_FILE_COUNT;
	}

	if(file_data->append == TRUE) {
		/* iteratively find the last created file */
		while(file_data->cur_file_index<file_data->max_file_count)
		{
			if(apt_log_file_exist(file_data) == 0)
			{
				if(file_data->cur_file_index > 0)
					file_data->cur_file_index--;
				file_data->cur_size = apt_log_file_get_size(file_data);
				break;
			}
			file_data->cur_file_index++;
		}

		/* if all the files have been created start rewriting from beginning */
		if(file_data->cur_file_index>=file_data->max_file_count)
		{
			file_data->cur_file_index=0;
			file_data->cur_size=0;
			log_file_path = apt_log_file_path_make(file_data);
			file_data->file = fopen(log_file_path,"wb"); /* truncate the first file to zero length */
			fclose(file_data->file);
		}
	}

	/* create mutex */
	if(apr_thread_mutex_create(&file_data->mutex,APR_THREAD_MUTEX_DEFAULT,pool) != APR_SUCCESS) {
		return FALSE;
	}
	/* open log file */
	log_file_path = apt_log_file_path_make(file_data);
	file_data->file = fopen(log_file_path,file_data->append == TRUE ? "ab" : "wb");
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

APT_DECLARE(apt_bool_t) apt_log_output_mode_check(apt_log_output_e mode)
{
	if(!apt_logger) {
		return FALSE;
	}
	return (apt_logger->mode & mode) ? TRUE : FALSE;
}

APT_DECLARE(int) apt_log_output_mode_translate(char *str)
{
	int mode = APT_LOG_OUTPUT_NONE;
	char *name;
	char *last;
	name = apr_strtok(str, ",", &last);
	while(name) {
		if(strcasecmp(name, "CONSOLE") == 0)
			mode |=  APT_LOG_OUTPUT_CONSOLE;
		else if(strcasecmp(name, "FILE") == 0)
			mode |=  APT_LOG_OUTPUT_FILE;
		
		name = apr_strtok(NULL, ",", &last);
	}
	return mode;
}

APT_DECLARE(apt_bool_t) apt_log_priority_set(apt_log_priority_e priority)
{
	if(!apt_logger || priority >= APT_PRIO_COUNT) {
		return FALSE;
	}
	apt_logger->priority = priority;
	return TRUE;
}

APT_DECLARE(apt_log_priority_e) apt_log_priority_translate(const char *str)
{
	if(strcasecmp(str, "EMERGENCY") == 0)
		return APT_PRIO_EMERGENCY;
	else if(strcasecmp(str, "ALERT") == 0)
		return APT_PRIO_ALERT;
	else if(strcasecmp(str, "CRITICAL") == 0)
		return APT_PRIO_CRITICAL;
	else if(strcasecmp(str, "ERROR") == 0)
		return APT_PRIO_ERROR;
	else if(strcasecmp(str, "WARNING") == 0)
		return APT_PRIO_WARNING;
	else if(strcasecmp(str, "NOTICE") == 0)
		return APT_PRIO_NOTICE;
	else if(strcasecmp(str, "INFO") == 0)
		return APT_PRIO_INFO;
	else if(strcasecmp(str, "DEBUG") == 0)
		return APT_PRIO_DEBUG;

	return APT_PRIO_DEBUG;
}

APT_DECLARE(apt_bool_t) apt_log_header_set(int header)
{
	if(!apt_logger) {
		return FALSE;
	}
	apt_logger->header = header;
	return TRUE;
}

APT_DECLARE(int) apt_log_header_translate(char *str)
{
	int header = APT_LOG_OUTPUT_NONE;
	char *name;
	char *last;
	name = apr_strtok(str, ",", &last);
	while(name) {
		if(strcasecmp(name, "DATE") == 0)
			header |=  APT_LOG_HEADER_DATE;
		else if(strcasecmp(name, "TIME") == 0)
			header |=  APT_LOG_HEADER_TIME;
		else if(strcasecmp(name, "PRIORITY") == 0)
			header |=  APT_LOG_HEADER_PRIORITY;
		else if(strcasecmp(name, "MARK") == 0)
			header |=  APT_LOG_HEADER_MARK;
		else if(strcasecmp(name, "THREAD") == 0)
			header |=  APT_LOG_HEADER_THREAD;

		name = apr_strtok(NULL, ",", &last);
	}
	return header;
}

APT_DECLARE(apt_bool_t) apt_log_masking_set(apt_log_masking_e masking)
{
	if(!apt_logger) {
		return FALSE;
	}
	apt_logger->masking = masking;
	return TRUE;
}

APT_DECLARE(apt_log_masking_e) apt_log_masking_get(void)
{
	if(!apt_logger) {
		return APT_LOG_MASKING_NONE;
	}
	return apt_logger->masking;
}

APT_DECLARE(apt_log_masking_e) apt_log_masking_translate(const char *str)
{
	if(strcasecmp(str, "COMPLETE") == 0)
		return APT_LOG_MASKING_COMPLETE;
	else if(strcasecmp(str, "ENCRYPTED") == 0)
		return APT_LOG_MASKING_ENCRYPTED;
	return APT_LOG_MASKING_NONE;
}

#define APT_MASKED_CONTENT "*** masked ***"

APT_DECLARE(const char*) apt_log_data_mask(const char *data_in, apr_size_t *length, apr_pool_t *pool)
{
	if(!apt_logger) {
		return NULL;
	}
	if(apt_logger->masking == APT_LOG_MASKING_COMPLETE) {
		*length = sizeof(APT_MASKED_CONTENT) - 1;
		return APT_MASKED_CONTENT;
	}
	return data_in;
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

APT_DECLARE(apt_bool_t) apt_obj_log(const char *file, int line, apt_log_priority_e priority, void *obj, const char *format, ...)
{
	apt_bool_t status = TRUE;
	if(!apt_logger) {
		return FALSE;
	}
	if(priority <= apt_logger->priority) {
		va_list arg_ptr;
		va_start(arg_ptr, format);
		if(apt_logger->ext_handler) {
			status = apt_logger->ext_handler(file,line,obj,priority,format,arg_ptr);
		}
		else {
			status = apt_do_log(file,line,priority,format,arg_ptr);
		}
		va_end(arg_ptr); 
	}
	return status;
}

APT_DECLARE(apt_bool_t) apt_va_log(const char *file, int line, apt_log_priority_e priority, const char *format, va_list arg_ptr)
{
	apt_bool_t status = TRUE;
	if(!apt_logger) {
		return FALSE;
	}
	if(priority <= apt_logger->priority) {
		if(apt_logger->ext_handler) {
			status = apt_logger->ext_handler(file,line,NULL,priority,format,arg_ptr);
		}
		else {
			status = apt_do_log(file,line,priority,format,arg_ptr);
		}
	}
	return status;
}

static APR_INLINE unsigned long apt_thread_id_get(void)
{
#ifdef WIN32
	return (unsigned long) GetCurrentThreadId();
#else
	return (unsigned long) apr_os_thread_current();
#endif
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
	if(apt_logger->header & APT_LOG_HEADER_THREAD) {
		offset += apr_snprintf(log_entry+offset,max_size-offset,"%05lu ",apt_thread_id_get());
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
	const char *log_file_name = apr_psprintf(file_data->pool,"%s-%.2"APR_SIZE_T_FMT".log",
									file_data->log_file_name,
									file_data->cur_file_index);
	apr_filepath_merge(&log_file_path,
		file_data->log_dir_path,
		log_file_name,
		APR_FILEPATH_NATIVE,
		file_data->pool);
	return log_file_path;
}

static apr_size_t apt_log_file_get_size(apt_log_file_data_t *file_data)
{
	FILE* fp;
	const char *log_file_path;
	apr_size_t ret;
	
	log_file_path = apt_log_file_path_make(file_data);
	fp = fopen(log_file_path,"rb");
	
	if(!fp) return 0;

	fseek(fp,0,SEEK_END);
	ret = ftell(fp);

	fclose(fp);

	return ret;
}

static apr_byte_t apt_log_file_exist(apt_log_file_data_t *file_data)
{
	FILE* fp;
	const char *log_file_path;
	
	log_file_path = apt_log_file_path_make(file_data);
	fp = fopen(log_file_path,"rb");
	
	if(!fp) return 0;

	fclose(fp);

	return 1;
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

static apr_xml_doc* apt_log_doc_parse(const char *file_path, apr_pool_t *pool)
{
	apr_xml_parser *parser = NULL;
	apr_xml_doc *xml_doc = NULL;
	apr_file_t *fd = NULL;
	apr_status_t rv;

	rv = apr_file_open(&fd,file_path,APR_READ|APR_BINARY,0,pool);
	if(rv != APR_SUCCESS) {
		return NULL;
	}

	rv = apr_xml_parse_file(pool,&parser,&xml_doc,fd,2000);
	if(rv != APR_SUCCESS) {
		xml_doc = NULL;
	}
	
	apr_file_close(fd);
	return xml_doc;
}
