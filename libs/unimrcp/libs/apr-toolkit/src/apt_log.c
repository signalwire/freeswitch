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

#ifdef WIN32
#pragma warning(disable: 4127)
#else
#include <sys/unistd.h>
#include <syslog.h>
#endif
#include <stdlib.h>
#include <apr_ring.h>
#include <apr_time.h>
#include <apr_file_io.h>
#include <apr_fnmatch.h>
#include <apr_portable.h>
#include <apr_hash.h>
#include <apr_xml.h>
#include "apt_pool.h"
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
typedef struct apt_log_file_settings_t apt_log_file_settings_t;
typedef struct apt_log_file_entry_t apt_log_file_entry_t;
typedef struct apt_syslog_settings_t apt_syslog_settings_t;

struct apt_log_file_entry_t {
	APR_RING_ENTRY(apt_log_file_entry_t) link;
	char                     *name;
	apr_time_t                ctime;
};

struct apt_syslog_settings_t {
	int                       option;
	int                       facility;
};

struct apt_log_file_settings_t {
	apt_bool_t                purge_existing;     
	apr_size_t                max_age;            /* max age in seconds */
	apr_size_t                max_size;           /* max size in bytes */
	apr_size_t                max_count;          /* max number of files used in rotation */
	apr_size_t                pool_reuse_count;   /* max number of log rotation cycles the same pool is used for allocation of temporary data */
};

struct apt_log_file_data_t {
	APR_RING_HEAD(apt_log_file_entry_head_t, apt_log_file_entry_t) file_entry_list;
	apr_size_t                file_entry_count;
	const char               *dir_path;
	const char               *prefix;
	const char               *current_link;
	FILE                     *file;
	const char               *name;
	apr_time_t                ctime;
	apr_size_t                cur_size;
	apr_thread_mutex_t       *mutex;
	apr_pool_t               *pool;               /* this pool must be used for allocation of temporary data only */
	apr_size_t                rotation_count;
	apt_log_file_settings_t   settings;
};

struct apt_log_source_t {
	const char               *name;
	apt_log_priority_e        priority;
	apt_log_masking_e         masking;
};

struct apt_logger_t {
	apt_log_output_e          mode;
	int                       header;
	apr_hash_t               *log_sources;
	apt_log_ext_handler_f     ext_handler;
	apt_log_file_data_t      *file_data;
	apt_bool_t                syslog;
};

static apt_logger_t *apt_logger = NULL;
apt_log_source_t def_log_source;

static apt_bool_t apt_do_log(apt_log_source_t *log_source, const char *file, int line, apt_log_priority_e priority, const char *format, va_list arg_ptr);

static apt_bool_t apt_log_file_open_internal(const char *dir_path, const char *prefix, const apt_log_file_settings_t *settings, apr_pool_t *pool);
static apt_bool_t apt_log_file_create(apt_log_file_data_t *file_data);
static const char* apt_log_file_path_make(const apt_log_file_data_t *file_data, const char *log_file_name, apr_pool_t *pool);
static apt_bool_t apt_log_file_link_current(const apt_log_file_data_t *file_data, const char *path);
static void apt_log_files_purge(const apt_log_file_data_t *file_data);
static void apt_log_files_populate(apt_log_file_data_t *file_data);
static void apt_log_file_entries_clear(apt_log_file_data_t *file_data);
static apt_bool_t apt_log_file_dump(apt_log_file_data_t *file_data, const char *log_entry, apr_size_t size);
static apr_xml_doc* apt_log_doc_parse(const char *file_path, apr_pool_t *pool);

static void apt_log_file_settings_init(apt_log_file_settings_t *settings)
{
	settings->purge_existing = FALSE;
	settings->max_age = 0;
	settings->max_count = MAX_LOG_FILE_COUNT;
	settings->max_size = MAX_LOG_FILE_SIZE;
	settings->pool_reuse_count = 100;
}

static apt_logger_t* apt_log_instance_alloc(apr_pool_t *pool)
{
	apt_logger_t *logger = apr_palloc(pool,sizeof(apt_logger_t));
	logger->mode = APT_LOG_OUTPUT_CONSOLE;
	logger->header = APT_LOG_HEADER_DEFAULT;
	logger->ext_handler = NULL;
	logger->file_data = NULL;
	logger->syslog = FALSE;

	/* Create hash for custom log sources */
	logger->log_sources = apr_hash_make(pool);

	/* Initialize default log source */
	def_log_source.name = "DEFAULT";
	def_log_source.priority = APT_PRIO_INFO;
	def_log_source.masking = APT_LOG_MASKING_NONE;

	return logger;
}

APT_DECLARE(apt_bool_t) apt_log_instance_create(apt_log_output_e mode, apt_log_priority_e priority, apr_pool_t *pool)
{
	if(apt_logger) {
		return FALSE;
	}
	apt_logger = apt_log_instance_alloc(pool);
	apt_logger->mode = mode;
	def_log_source.priority = priority;
	return TRUE;
}

static apt_bool_t apt_log_sources_load(const apr_xml_elem *sources_root, apr_pool_t *pool)
{
	const apr_xml_elem *elem;
	const apr_xml_attr *attr;
	const char *name;
	apt_log_priority_e priority;
	apt_log_masking_e masking;

	/* Navigate through log sources */
	for(elem = sources_root->first_child; elem; elem = elem->next) {
		if(strcasecmp(elem->name,"source") == 0) {
			name = NULL;
			priority = def_log_source.priority;
			masking = def_log_source.masking;

			for(attr = elem->attr; attr; attr = attr->next) {
				if(strcasecmp(attr->name,"name") == 0) {
					name = apr_pstrdup(pool,attr->value);
				}
				else if(strcasecmp(attr->name,"priority") == 0) {
					priority = apt_log_priority_translate(attr->value);
				}
				else if(strcasecmp(attr->name,"masking") == 0) {
					masking = apt_log_masking_translate(attr->value);
				}
			}

			if(name) {
				/* Create and store custom log source */
				apt_log_source_t *log_source = apr_palloc(pool,sizeof(apt_log_source_t));
				log_source->name = name;
				log_source->priority = priority;
				log_source->masking = masking;
				apr_hash_set(apt_logger->log_sources,log_source->name,APR_HASH_KEY_STRING,log_source);
			}
		}
		else {
			/* Unknown element */
		}
	}
	return TRUE;
}

static apt_bool_t apt_log_file_settings_load(apt_log_file_settings_t *settings, const apr_xml_elem *elem, apr_pool_t *pool)
{
	const apr_xml_attr *attr;

	for (attr = elem->attr; attr; attr = attr->next) {
		if (strcasecmp(attr->name, "purge-existing") == 0) {
			if (strcasecmp(attr->value, "false") == 0) {
				settings->purge_existing = FALSE;
			}
			else if (strcasecmp(attr->value, "true") == 0) {
				settings->purge_existing = TRUE;
			}
		}
		else if (strcasecmp(attr->name, "max-age") == 0) {
			settings->max_age = atol(attr->value) * 24 * 3600;
		}
		else if (strcasecmp(attr->name, "max-count") == 0) {
			settings->max_count = atol(attr->value);
		}
		else if (strcasecmp(attr->name, "max-size") == 0) {
			settings->max_size = atol(attr->value) * 1024 * 1024;
		}
		else if (strcasecmp(attr->name, "pool-reuse-count") == 0) {
			settings->pool_reuse_count = atol(attr->value);
		}
	}

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
		apt_logger = NULL;
		return FALSE;
	}

	root = doc->root;

	/* Match document name */
	if(!root || strcasecmp(root->name,"aptlogger") != 0) {
		/* Unknown document */
		apt_logger = NULL;
		return FALSE;
	}

	/* Navigate through document */
	for(elem = root->first_child; elem; elem = elem->next) {
		if(!elem->first_cdata.first || !elem->first_cdata.first->text) 
			continue;

		text = apr_pstrdup(pool,elem->first_cdata.first->text);
		apr_collapse_spaces(text,text);
		
		if(strcasecmp(elem->name,"priority") == 0) {
			def_log_source.priority = apt_log_priority_translate(text);
		}
		else if(strcasecmp(elem->name,"output") == 0) {
			apt_logger->mode = apt_log_output_mode_translate(text);
		}
		else if(strcasecmp(elem->name,"headers") == 0) {
			apt_logger->header = apt_log_header_translate(text);
		}
		else if(strcasecmp(elem->name,"masking") == 0) {
			def_log_source.masking = apt_log_masking_translate(text);
		}
		else if(strcasecmp(elem->name,"sources") == 0) {
			apt_log_sources_load(elem,pool);
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

	if (apt_logger->syslog == TRUE) {
		apt_syslog_close();
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

APT_DECLARE(void) apt_def_log_source_set(apt_log_source_t *log_source)
{
	if(log_source)
		def_log_source = *log_source;
}

APT_DECLARE(apt_bool_t) apt_log_source_assign(const char *name, apt_log_source_t **log_source)
{
	apt_log_source_t *found_log_source;

	if(!apt_logger) {
		return FALSE;
	}

	found_log_source = apr_hash_get(apt_logger->log_sources,name,APR_HASH_KEY_STRING);
	if(!found_log_source) {
		return FALSE;
	}

	if(log_source) {
		*log_source = found_log_source;
	}
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
	apt_log_file_settings_t settings;

	/* Initialize file settings */
	apt_log_file_settings_init(&settings);

	settings.max_size = max_file_size;
	settings.max_count = max_file_count;

	return apt_log_file_open_internal(dir_path, file_name, &settings, pool);
}

APT_DECLARE(apt_bool_t) apt_log_file_open_ex(const char *dir_path, const char *prefix, const char *config_file, apr_pool_t *pool)
{
	apr_xml_doc *doc;
	apt_log_file_settings_t settings;

	/* Initialize file settings */
	apt_log_file_settings_init(&settings);

	/* Parse XML document */
	doc = apt_log_doc_parse(config_file, pool);
	if (doc) {
		const apr_xml_elem *elem;
		const apr_xml_elem *root = doc->root;

		/* Match document name */
		if (root && strcasecmp(root->name, "aptlogfile") == 0) {
			/* Navigate through document */
			for (elem = root->first_child; elem; elem = elem->next) {
				if (strcasecmp(elem->name, "settings") == 0) {
					apt_log_file_settings_load(&settings, elem, pool);
				}
				else {
					/* Unknown element */
				}
			}
		}
	}

	return apt_log_file_open_internal(dir_path, prefix, &settings, pool);
}

static apt_bool_t apt_log_file_open_internal(const char *dir_path, const char *prefix, const apt_log_file_settings_t *settings, apr_pool_t *pool)
{
	char log_file_link_name[256];
	apt_log_file_data_t *file_data;
	if (!apt_logger || !dir_path || !prefix || !settings) {
		return FALSE;
	}

	if (apt_logger->file_data) {
		return FALSE;
	}

	file_data = apr_palloc(pool, sizeof(apt_log_file_data_t));
	APR_RING_INIT(&file_data->file_entry_list, apt_log_file_entry_t, link);
	file_data->file_entry_count = 0;
	file_data->dir_path = apr_pstrdup(pool, dir_path);
	file_data->prefix = apr_pstrdup(pool, prefix);
	file_data->name = NULL;
	file_data->ctime = 0;
	file_data->cur_size = 0;
	file_data->mutex = NULL;
	file_data->pool = apt_pool_create();
	file_data->rotation_count = 0;
	file_data->settings = *settings;

	if (!file_data->settings.max_size) {
		file_data->settings.max_size = MAX_LOG_FILE_SIZE;
	}

	/* create mutex */
	if (apr_thread_mutex_create(&file_data->mutex, APR_THREAD_MUTEX_DEFAULT, pool) != APR_SUCCESS) {
		apr_pool_destroy(file_data->pool);
		return FALSE;
	}

	if (file_data->settings.purge_existing == TRUE) {
		apt_log_files_purge(file_data);
	}
	else {
		apt_log_files_populate(file_data);
	}

	/* compose current link name */
	apr_snprintf(log_file_link_name, sizeof(log_file_link_name), "%s_current.log", file_data->prefix);
	/* compose path to current link using permanent pool */
	file_data->current_link = apt_log_file_path_make(file_data, log_file_link_name, pool);

	/* create new log file */
	if (apt_log_file_create(file_data) == FALSE) {
		apt_log_file_entries_clear(file_data);
		apr_pool_destroy(file_data->pool);
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
		/* destroy file entries */
		apt_log_file_entries_clear(file_data);
		/* destroy mutex */
		apr_thread_mutex_destroy(file_data->mutex);
		file_data->mutex = NULL;
		apr_pool_destroy(file_data->pool);
		file_data->pool = NULL;
	}
	apt_logger->file_data = NULL;
	return TRUE;
}

#ifndef WIN32
static int apt_syslog_option_translate(const char *str, apr_pool_t *pool)
{
	int option = 0;
	char *name;
	char *last;
	char *value = apr_pstrdup(pool, str);
	name = apr_strtok(value, ",", &last);
	while (name) {
		if (strcasecmp(str, "LOG_CONS") == 0)
			option |= LOG_CONS;
		else if (strcasecmp(str, "LOG_NDELAY") == 0)
			option |= LOG_NDELAY;
		else if (strcasecmp(str, "LOG_NOWAIT") == 0)
			option |= LOG_NOWAIT;
		else if (strcasecmp(str, "LOG_ODELAY") == 0)
			option |= LOG_ODELAY;
		else if (strcasecmp(str, "LOG_PERROR") == 0)
			option |= LOG_PERROR;
		else if (strcasecmp(str, "LOG_PID") == 0)
			option |= LOG_PID;

		name = apr_strtok(NULL, ",", &last);
	}

	return option;
}

static int apt_syslog_facility_translate(const char *str)
{
	if (strcasecmp(str, "LOG_AUTH") == 0)
		return LOG_AUTH;
	else if (strcasecmp(str, "LOG_AUTHPRIV") == 0)
		return LOG_AUTHPRIV;
	else if (strcasecmp(str, "LOG_CRON") == 0)
		return LOG_CRON;
	else if (strcasecmp(str, "LOG_DAEMON") == 0)
		return LOG_DAEMON;
	else if (strcasecmp(str, "LOG_FTP") == 0)
		return LOG_FTP;
	else if (strcasecmp(str, "LOG_KERN") == 0)
		return LOG_KERN;
	else if (strcasecmp(str, "LOG_LOCAL0") == 0)
		return LOG_LOCAL0;
	else if (strcasecmp(str, "LOG_LOCAL1") == 0)
		return LOG_LOCAL1;
	else if (strcasecmp(str, "LOG_LOCAL2") == 0)
		return LOG_LOCAL2;
	else if (strcasecmp(str, "LOG_LOCAL3") == 0)
		return LOG_LOCAL3;
	else if (strcasecmp(str, "LOG_LOCAL4") == 0)
		return LOG_LOCAL4;
	else if (strcasecmp(str, "LOG_LOCAL5") == 0)
		return LOG_LOCAL5;
	else if (strcasecmp(str, "LOG_LOCAL6") == 0)
		return LOG_LOCAL6;
	else if (strcasecmp(str, "LOG_LOCAL7") == 0)
		return LOG_LOCAL7;
	else if (strcasecmp(str, "LOG_MAIL") == 0)
		return LOG_MAIL;
	else if (strcasecmp(str, "LOG_NEWS") == 0)
		return LOG_NEWS;
	else if (strcasecmp(str, "LOG_SYSLOG") == 0)
		return LOG_SYSLOG;
	else if (strcasecmp(str, "LOG_USER") == 0)
		return LOG_USER;
	else if (strcasecmp(str, "LOG_UUCP") == 0)
		return LOG_UUCP;

	return LOG_USER;
}

static apt_bool_t apt_syslog_settings_load(apt_syslog_settings_t *settings, const apr_xml_elem *elem, apr_pool_t *pool)
{
	const apr_xml_attr *attr;

	for (attr = elem->attr; attr; attr = attr->next) {
		if (strcasecmp(attr->name, "option") == 0) {
			settings->option = apt_syslog_option_translate(attr->value, pool);
		}
		else if (strcasecmp(attr->name, "facility") == 0) {
			settings->facility = apt_syslog_facility_translate(attr->value);
		}
	}

	return TRUE;
}
#endif

APT_DECLARE(apt_bool_t) apt_syslog_open(const char *prefix, const char *config_file, apr_pool_t *pool)
{
	if (!apt_logger || !prefix) {
		return FALSE;
	}

	if (apt_logger->syslog == TRUE) {
		return FALSE;
	}

#ifndef WIN32
	apt_syslog_settings_t settings;
	settings.option = LOG_PID;
	settings.facility = LOG_USER;

	/* Parse XML document */
	if (config_file) {
		apr_xml_doc *doc = apt_log_doc_parse(config_file, pool);
		if (doc) {
			const apr_xml_elem *elem;
			const apr_xml_elem *root = doc->root;

			/* Match document name */
			if (root && strcasecmp(root->name, "aptsyslog") == 0) {
				/* Navigate through document */
				for (elem = root->first_child; elem; elem = elem->next) {
					if (strcasecmp(elem->name, "settings") == 0) {
						apt_syslog_settings_load(&settings, elem, pool);
					}
					else {
						/* Unknown element */
					}
				}
			}
		}
	}

	openlog(prefix, settings.option, settings.facility);
#endif
	apt_logger->syslog = TRUE;
	return TRUE;
}

APT_DECLARE(apt_bool_t) apt_syslog_close()
{
	if (!apt_logger || apt_logger->syslog == FALSE) {
		return FALSE;
	}

#ifndef WIN32
	closelog();
#endif
	apt_logger->syslog = FALSE;
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
		else if (strcasecmp(name, "FILE") == 0)
			mode |= APT_LOG_OUTPUT_FILE;
		else if (strcasecmp(name, "SYSLOG") == 0)
			mode |= APT_LOG_OUTPUT_SYSLOG;

		name = apr_strtok(NULL, ",", &last);
	}
	return mode;
}

APT_DECLARE(apt_bool_t) apt_log_priority_set(apt_log_priority_e priority)
{
	if(!apt_logger || priority >= APT_PRIO_COUNT) {
		return FALSE;
	}
	def_log_source.priority = priority;
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
	def_log_source.masking = masking;
	return TRUE;
}

APT_DECLARE(apt_log_masking_e) apt_log_masking_get(void)
{
	if(!apt_logger) {
		return APT_LOG_MASKING_NONE;
	}
	return def_log_source.masking;
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
	if(def_log_source.masking == APT_LOG_MASKING_COMPLETE) {
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

APT_DECLARE(apt_bool_t) apt_log(apt_log_source_t *log_source, const char *file, int line, apt_log_priority_e priority, const char *format, ...)
{
	apt_bool_t status = TRUE;
	if(!apt_logger || !log_source) {
		return FALSE;
	}
	
	if(priority <= log_source->priority) {
		va_list arg_ptr;
		va_start(arg_ptr, format);
		if(apt_logger->ext_handler) {
			status = apt_logger->ext_handler(file,line,NULL,priority,format,arg_ptr);
		}
		else {
			status = apt_do_log(log_source,file,line,priority,format,arg_ptr);
		}
		va_end(arg_ptr); 
	}
	return status;
}

APT_DECLARE(apt_bool_t) apt_obj_log(apt_log_source_t *log_source, const char *file, int line, apt_log_priority_e priority, void *obj, const char *format, ...)
{
	apt_bool_t status = TRUE;
	if(!apt_logger || !log_source) {
		return FALSE;
	}

	if(priority <= log_source->priority) {
		va_list arg_ptr;
		va_start(arg_ptr, format);
		if(apt_logger->ext_handler) {
			status = apt_logger->ext_handler(file,line,obj,priority,format,arg_ptr);
		}
		else {
			status = apt_do_log(log_source,file,line,priority,format,arg_ptr);
		}
		va_end(arg_ptr); 
	}
	return status;
}

APT_DECLARE(apt_bool_t) apt_va_log(apt_log_source_t *log_source, const char *file, int line, apt_log_priority_e priority, const char *format, va_list arg_ptr)
{
	apt_bool_t status = TRUE;
	if(!apt_logger || !log_source) {
		return FALSE;
	}

	if(priority <= log_source->priority) {
		if(apt_logger->ext_handler) {
			status = apt_logger->ext_handler(file,line,NULL,priority,format,arg_ptr);
		}
		else {
			status = apt_do_log(log_source,file,line,priority,format,arg_ptr);
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

static apt_bool_t apt_do_log(apt_log_source_t *log_source, const char *file, int line, apt_log_priority_e priority, const char *format, va_list arg_ptr)
{
	char log_entry[MAX_LOG_ENTRY_SIZE];
	apr_size_t max_size = MAX_LOG_ENTRY_SIZE - 2;
	apr_size_t offset = 0;
	apr_size_t data_offset;
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

	data_offset = offset;
	offset += apr_vsnprintf(log_entry+offset,max_size-offset,format,arg_ptr);
	log_entry[offset++] = '\n';
	log_entry[offset] = '\0';
	if((apt_logger->mode & APT_LOG_OUTPUT_CONSOLE) == APT_LOG_OUTPUT_CONSOLE) {
		fwrite(log_entry,offset,1,stdout);
	}
	
	if((apt_logger->mode & APT_LOG_OUTPUT_FILE) == APT_LOG_OUTPUT_FILE && apt_logger->file_data) {
		apt_log_file_dump(apt_logger->file_data,log_entry,offset);
	}

#ifndef WIN32
	if((apt_logger->mode & APT_LOG_OUTPUT_SYSLOG) == APT_LOG_OUTPUT_SYSLOG) {
		syslog(priority,"%s",log_entry + data_offset);
	}
#endif
	return TRUE;
}

static apt_bool_t apt_log_file_create(apt_log_file_data_t *file_data)
{
	const char *log_file_path;
	apr_time_exp_t result;
	/* compose log file name based on current date and time */
	file_data->ctime = apr_time_now();
	apr_time_exp_lt(&result, file_data->ctime);

	/* compose log file name */
	file_data->name = apr_psprintf(file_data->pool, "%s_%4d.%02d.%02d_%02d.%02d.%02d.%06d.log",
		file_data->prefix,
		result.tm_year + 1900, result.tm_mon + 1, result.tm_mday,
		result.tm_hour, result.tm_min, result.tm_sec,
		result.tm_usec);

	/* compose log file path */
	log_file_path = apt_log_file_path_make(file_data, file_data->name, file_data->pool);

	/* open new log file */
	file_data->file = fopen(log_file_path, "wb");
	if (!file_data->file) {
		return FALSE;
	}
	
	/* link current log file */
	apt_log_file_link_current(file_data, log_file_path);
	return TRUE;
}

static const char* apt_log_file_path_make(const apt_log_file_data_t *file_data, const char *log_file_name, apr_pool_t *pool)
{
	char *log_file_path = NULL;
	apr_filepath_merge(&log_file_path, file_data->dir_path, log_file_name, APR_FILEPATH_NATIVE,	pool);
	return log_file_path;
}

static apt_bool_t apt_log_file_link_current(const apt_log_file_data_t *file_data, const char *path)
{
#ifndef _WIN32
	// Update the link to the current log file
	unlink(file_data->current_link);
	if (symlink(path, file_data->current_link) != 0)
	{
		// Error
		return FALSE;
	}
#endif
	return TRUE;
}

static void apt_log_file_entry_add(apt_log_file_data_t *file_data, const char *name, apr_time_t ctime)
{
	apt_log_file_entry_t *file_entry = malloc(sizeof(apt_log_file_entry_t));
	APR_RING_ELEM_INIT(file_entry, link);
	file_entry->name = strdup(name);
	file_entry->ctime = ctime;

	/* add to the list */
	APR_RING_INSERT_TAIL(&file_data->file_entry_list, file_entry, apt_log_file_entry_t, link);
	file_data->file_entry_count++;
}

static void apt_log_file_entry_remove(apt_log_file_data_t *file_data, apt_log_file_entry_t *file_entry)
{
	APR_RING_REMOVE(file_entry, link);
	file_data->file_entry_count--;

	if (file_entry->name)
		free(file_entry->name);
	free(file_entry);
}

static void apt_log_file_entries_clear(apt_log_file_data_t *file_data)
{
	while (file_data->file_entry_count) {
		apt_log_file_entry_t *file_entry = APR_RING_FIRST(&file_data->file_entry_list);

		apt_log_file_entry_remove(file_data, file_entry);
	}
}

static void apt_log_file_count_check(apt_log_file_data_t *file_data)
{
	if (!file_data->settings.max_count)
		return;

	while (file_data->file_entry_count >= file_data->settings.max_count) {
		apt_log_file_entry_t *file_entry = APR_RING_FIRST(&file_data->file_entry_list);

		const char *path = apt_log_file_path_make(file_data, file_entry->name, file_data->pool);
		apr_file_remove(path, file_data->pool);
		
		apt_log_file_entry_remove(file_data, file_entry);
	}
}

static void apt_log_file_age_check(apt_log_file_data_t *file_data)
{
	apr_time_t ref_time;
	const char *path;
	if (!file_data->settings.max_age)
		return;

	ref_time = apr_time_now() - apr_time_from_sec(file_data->settings.max_age);

	while (file_data->file_entry_count) {
		apt_log_file_entry_t *file_entry = APR_RING_FIRST(&file_data->file_entry_list);
		if (file_entry->ctime >= ref_time) {
			break;
		}

		path = apt_log_file_path_make(file_data, file_entry->name, file_data->pool);
		apr_file_remove(path, file_data->pool);

		apt_log_file_entry_remove(file_data, file_entry);
	}
}

static APR_INLINE const char* apt_log_file_pattern_make(const apt_log_file_data_t *file_data)
{
	return apr_psprintf(file_data->pool, "%s*.log", file_data->prefix);
}

static void apt_log_files_purge(const apt_log_file_data_t *file_data)
{
	apr_dir_t *dir;
	apr_finfo_t file_info;
	const char *pattern;
	apr_uint32_t mask = APR_FINFO_TYPE | APR_FINFO_NAME;

	if (apr_dir_open(&dir, file_data->dir_path, file_data->pool) != APR_SUCCESS)
		return;

	pattern = apt_log_file_pattern_make(file_data);

	while (apr_dir_read(&file_info, mask, dir) == APR_SUCCESS)
	{
		if (file_info.filetype == APR_REG && apr_fnmatch(pattern, file_info.name, 0) == APR_SUCCESS)
		{
			const char *path = apt_log_file_path_make(file_data, file_info.name, file_data->pool);
			apr_file_remove(path, file_data->pool);
		}
	}
	apr_dir_close(dir);
}

static void apt_log_files_populate(apt_log_file_data_t *file_data)
{
	apr_dir_t *dir;
	apr_finfo_t file_info;
	const char *pattern;
	apr_uint32_t mask = APR_FINFO_TYPE | APR_FINFO_NAME;
	apr_time_t cur_time = 0;
	apr_time_t expiration_interval = 0;

	if (apr_dir_open(&dir, file_data->dir_path, file_data->pool) != APR_SUCCESS)
		return;

	pattern = apt_log_file_pattern_make(file_data);

	if (file_data->settings.max_age) {
		mask |= APR_FINFO_CTIME;
		cur_time = apr_time_now();
		expiration_interval = apr_time_from_sec(file_data->settings.max_age);
	}
	while (apr_dir_read(&file_info, mask, dir) == APR_SUCCESS) {
		if (file_info.filetype == APR_REG && apr_fnmatch(pattern, file_info.name, 0) == APR_SUCCESS) {
			if (expiration_interval && cur_time >= file_info.ctime + expiration_interval) {
				/* exceeds max file age */
				const char *path = apt_log_file_path_make(file_data, file_info.name, file_data->pool);
				apr_file_remove(path, file_data->pool);
				continue;
			}

			if(file_data->settings.max_count) {
				apt_log_file_entry_add(file_data, file_info.name, file_info.ctime);
			}
		}
	}
	apr_dir_close(dir);

	/* check max-count */
	apt_log_file_count_check(file_data);
}

static apt_bool_t apt_log_file_rotate(apt_log_file_data_t *file_data)
{
	/* close current log file */
	fclose(file_data->file);

	if (file_data->settings.max_count || file_data->settings.max_age) {
		apt_log_file_entry_add(file_data, file_data->name, file_data->ctime);

		/* check max-count */
		apt_log_file_count_check(file_data);

		/* check max-age */
		apt_log_file_age_check(file_data);
	}

	file_data->rotation_count++;
	if (file_data->rotation_count == file_data->settings.pool_reuse_count) {
		file_data->rotation_count = 0;

		/* clear pool to release memory used for allocation of temporary data */
		apr_pool_clear(file_data->pool);
	}

	return apt_log_file_create(file_data);
}

static apt_bool_t apt_log_file_dump(apt_log_file_data_t *file_data, const char *log_entry, apr_size_t size)
{
	apr_thread_mutex_lock(file_data->mutex);

	file_data->cur_size += size;
	if(file_data->cur_size > file_data->settings.max_size) {
		/* rotate log files */
		if (apt_log_file_rotate(file_data) == FALSE) {
			apr_thread_mutex_unlock(file_data->mutex);
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
