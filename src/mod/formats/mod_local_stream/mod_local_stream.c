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
 * mod_local_stream.c -- Local Streaming Audio
 *
 */
#include <switch.h>
/* for apr_pstrcat */


SWITCH_MODULE_LOAD_FUNCTION(mod_local_stream_load);
SWITCH_MODULE_DEFINITION(mod_local_stream, mod_local_stream_load, NULL, NULL);
SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_local_stream_shutdown);

struct local_stream_source;

static struct {
	switch_mutex_t *mutex;
	switch_hash_t *source_hash;
} globals;

static int RUNNING = 1;

struct local_stream_context {
	struct local_stream_source *source;
	switch_mutex_t *audio_mutex;
	switch_buffer_t *audio_buffer;
	int err;	
	struct local_stream_context *next;
};
typedef struct local_stream_context local_stream_context_t;


struct local_stream_source {
	char *name;
	char *location;
	uint8_t channels;
	int rate;
	int interval;
	int samples;
	uint32_t prebuf;
	char *timer_name;
	local_stream_context_t *context_list;
	switch_dir_t *dir_handle;
	switch_mutex_t *mutex;
	switch_memory_pool_t *pool;
};
typedef struct local_stream_source local_stream_source_t;

static void *SWITCH_THREAD_FUNC read_stream_thread(switch_thread_t *thread, void *obj)
{
	local_stream_source_t *source = obj;
	switch_file_handle_t fh = {0};
	local_stream_context_t *cp;
	char file_buf[128] = "", path_buf[512] = "";
	switch_timer_t timer = {0};
	int fd = -1;

	if (switch_core_timer_init(&timer, source->timer_name, source->interval, source->samples, source->pool) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CONSOLE, "Can't start timer.\n");
		return NULL;
	}


	while(RUNNING) {
		const char *fname;

		if (switch_dir_open(&source->dir_handle, source->location, source->pool) != SWITCH_STATUS_SUCCESS) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CONSOLE, "Can't open directory: %s\n", source->location);
			return NULL;
		}

		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "open directory: %s\n", source->location);
		switch_yield(1000000);

		while(RUNNING) {
			switch_size_t olen;
			uint8_t abuf[SWITCH_RECOMMENDED_BUFFER_SIZE] =  {0};

			if (fd > -1) {
				char *p;
				if (switch_fd_read_line(fd, path_buf, sizeof(path_buf))) {
					if ((p = strchr(path_buf, '\r')) ||
						(p = strchr(path_buf, '\n'))) {
						*p = '\0';
					}
				} else {
					close(fd);
					fd = -1;
					continue;
				}
			} else {
				if (!(fname = switch_dir_next_file(source->dir_handle, file_buf, sizeof(file_buf)))) {
					break;
				}

				switch_snprintf(path_buf, sizeof(path_buf), "%s%s%s", source->location, SWITCH_PATH_SEPARATOR, fname);

				if (switch_stristr(".loc", path_buf)) {
					if ((fd = open(path_buf, O_RDONLY)) < 0) {
						switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Can't open %s\n", fname);
						switch_yield(1000000);
					}
					continue;
				}
				
			}

			fname = path_buf;
			fh.prebuf = source->prebuf;
			
			if (switch_core_file_open(&fh,
									  (char *)fname,
									  source->channels,
									  source->rate,
									  SWITCH_FILE_FLAG_READ | SWITCH_FILE_DATA_SHORT, source->pool) != SWITCH_STATUS_SUCCESS) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Can't open %s\n", fname);
				switch_yield(1000000);
				continue;
			}
			
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Playing %s\n", fname);

			while (RUNNING) {
				switch_core_timer_next(&timer);
				olen = source->samples;

				if (switch_core_file_read(&fh, abuf, &olen) != SWITCH_STATUS_SUCCESS || !olen) {
					switch_core_file_close(&fh);
					break;
				}
				
				switch_mutex_lock(source->mutex);
				for (cp = source->context_list; cp; cp = cp->next) {
					switch_mutex_lock(cp->audio_mutex);
					switch_buffer_write(cp->audio_buffer, abuf, olen * 2);
					switch_mutex_unlock(cp->audio_mutex);
				}
				switch_mutex_unlock(source->mutex);
			}

		}

		switch_dir_close(source->dir_handle);
		source->dir_handle = NULL;

	}

	if (fd > -1) {
		close(fd);
	}
	
	switch_core_destroy_memory_pool(&source->pool);

	return NULL;
}

static switch_status_t local_stream_file_open(switch_file_handle_t *handle, const char *path)
{
	local_stream_context_t *context;
	local_stream_source_t *source;
	char *alt_path = NULL;
	switch_status_t status = SWITCH_STATUS_SUCCESS;
	
	if (switch_test_flag(handle, SWITCH_FILE_FLAG_WRITE)) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "This format does not support writing!\n");
		return SWITCH_STATUS_FALSE;
	}
	
	alt_path = switch_mprintf("%s/%d", path, handle->samplerate);

	switch_mutex_lock(globals.mutex);
	if ((source = switch_core_hash_find(globals.source_hash, alt_path))) {
		path = alt_path;
	} else {
		source = switch_core_hash_find(globals.source_hash, path);
	}
	switch_mutex_unlock(globals.mutex);

	if (!source) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "unknown source %s\n", path);
		status = SWITCH_STATUS_FALSE;
		goto end;
	}

	if ((context = switch_core_alloc(handle->memory_pool, sizeof(*context))) == 0) {
		status = SWITCH_STATUS_MEMERR;
		goto end;
	}	

	

	handle->samples = 0;
	handle->samplerate = source->rate;
	handle->channels = source->channels;
	handle->format = 0;
	handle->sections = 0;
	handle->seekable = 0;
	handle->speed = 0;
	handle->private_info = context;
	handle->interval = source->interval;
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "Opening Stream [%s] %dhz\n", path, handle->samplerate);
	
	switch_mutex_init(&context->audio_mutex, SWITCH_MUTEX_NESTED, handle->memory_pool);
	if (switch_buffer_create_dynamic(&context->audio_buffer, 512, 1024, 0) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Memory Error!\n");
		status = SWITCH_STATUS_MEMERR;
		goto end;
	}
	
	context->source = source;

	switch_mutex_lock(source->mutex);
	context->next = source->context_list;
	source->context_list = context;
	switch_mutex_unlock(source->mutex);

 end:
	switch_safe_free(alt_path);
	return status;
}

static switch_status_t local_stream_file_close(switch_file_handle_t *handle)
{
	local_stream_context_t *cp, *last = NULL, *context = handle->private_info;

	switch_mutex_lock(context->source->mutex);
	for (cp = context->source->context_list; cp; cp = cp->next) {
		if (cp == context) {
			if (last) {
				last->next = cp->next;
			} else {
				context->source->context_list = cp->next;
			}
			break;
		}
		last = cp;
	}
	switch_mutex_unlock(context->source->mutex);
	switch_buffer_destroy(&context->audio_buffer);
	
	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t local_stream_file_seek(switch_file_handle_t *handle, unsigned int *cur_sample, int64_t samples, int whence)
{
	return SWITCH_STATUS_FALSE;
}

static switch_status_t local_stream_file_read(switch_file_handle_t *handle, void *data, size_t *len)
{
	local_stream_context_t *context = handle->private_info;
	switch_size_t bytes = 0;
	size_t need = *len * 2;

	switch_mutex_lock(context->audio_mutex);
	if ((bytes = switch_buffer_read(context->audio_buffer, data, need))) {
		*len = bytes / 2;
	} else {
		if (need > 2560) {
			need = 2560;
		}
		memset(data, 255, need);
		*len = need / 2;
	}
	switch_mutex_unlock(context->audio_mutex);
	handle->sample_count += *len;
	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t local_stream_file_write(switch_file_handle_t *handle, void *data, size_t *len)
{
	return SWITCH_STATUS_FALSE;
}

static switch_status_t local_stream_file_set_string(switch_file_handle_t *handle, switch_audio_col_t col, const char *string)
{
	return SWITCH_STATUS_FALSE;
}

static switch_status_t local_stream_file_get_string(switch_file_handle_t *handle, switch_audio_col_t col, const char **string)
{
	return SWITCH_STATUS_FALSE;
}

/* Registration */

static char *supported_formats[SWITCH_MAX_CODECS] = { 0 };

static void launch_threads(void)
{
	char *cf = "local_stream.conf";
	switch_xml_t cfg, xml, directory, param;
	switch_memory_pool_t *pool;
	local_stream_source_t *source;
	switch_thread_t *thread;
	switch_threadattr_t *thd_attr = NULL;


	if (!(xml = switch_xml_open_cfg(cf, &cfg, NULL))) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "open of %s failed\n", cf);
		return;
	}

	for (directory = switch_xml_child(cfg, "directory"); directory; directory = directory->next) {
		char *path = (char *) switch_xml_attr(directory, "path");
		char *name = (char *) switch_xml_attr(directory, "name");
		
		if (!(name && path)) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "invalid config!\n");
			continue;
		}

		if (switch_core_new_memory_pool(&pool) != SWITCH_STATUS_SUCCESS) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "OH OH no pool\n");
			abort();
		}

		source = switch_core_alloc(pool, sizeof(*source));
		assert(source != NULL);
		source->pool = pool;
		
		source->name = switch_core_strdup(source->pool, name);
		source->location = switch_core_strdup(source->pool, path);
		source->rate = 8000;
		source->interval = 20;
		source->channels = 1;
		source->timer_name = "soft";

		
		for (param = switch_xml_child(directory, "param"); param; param = param->next) {
			char *var = (char *) switch_xml_attr_soft(param, "name");
			char *val = (char *) switch_xml_attr_soft(param, "value");

			if (!strcasecmp(var, "rate")) {
				int tmp = atoi(val);
				if (tmp == 8000 || tmp == 16000) {
					source->rate = tmp;
				}
			} else if (!strcasecmp(var, "prebuf")) {
				int tmp = atoi(val);
				if (tmp > 0) {
					source->prebuf = (uint32_t) tmp;
				}
			} else if (!strcasecmp(var, "channels")) {
				int tmp = atoi(val);
				if (tmp == 1 || tmp == 2) {
					source->channels = (uint8_t)tmp;
				}
			} else if (!strcasecmp(var, "interval")) {
				source->interval = atoi(val);
			} else if (!strcasecmp(var, "timer-name")) {
				source->timer_name = switch_core_strdup(source->pool, val);
			}
		}
		
		source->samples = switch_bytes_per_frame(source->rate, source->interval);

		switch_core_hash_insert(globals.source_hash, source->name, source);
		
		switch_mutex_init(&source->mutex, SWITCH_MUTEX_NESTED, source->pool);
		
		switch_threadattr_create(&thd_attr, source->pool);
		switch_threadattr_detach_set(thd_attr, 1);
		switch_threadattr_stacksize_set(thd_attr, SWITCH_THREAD_STACKSIZE);
		switch_thread_create(&thread, thd_attr, read_stream_thread, source, source->pool);
		
	}

	switch_xml_free(xml);

}


SWITCH_MODULE_LOAD_FUNCTION(mod_local_stream_load)
{
	switch_file_interface_t *file_interface;
	supported_formats[0] = "local_stream";
	
	*module_interface = switch_loadable_module_create_module_interface(pool, modname);
	file_interface = switch_loadable_module_create_interface(*module_interface, SWITCH_FILE_INTERFACE);
	file_interface->interface_name = modname;
	file_interface->extens = supported_formats;
	file_interface->file_open = local_stream_file_open;
	file_interface->file_close = local_stream_file_close;
	file_interface->file_read = local_stream_file_read;
	file_interface->file_write = local_stream_file_write;
	file_interface->file_seek = local_stream_file_seek;
	file_interface->file_set_string = local_stream_file_set_string;
	file_interface->file_get_string = local_stream_file_get_string;

	memset(&globals, 0, sizeof(globals));
	switch_mutex_init(&globals.mutex, SWITCH_MUTEX_NESTED, pool);
	switch_core_hash_init(&globals.source_hash, pool);
	launch_threads();
	
	/* indicate that the module should continue to be loaded */
	return SWITCH_STATUS_SUCCESS;
}

SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_local_stream_shutdown)
{
	RUNNING = 0;
	switch_yield(500000);
	switch_core_hash_destroy(&globals.source_hash);
	return SWITCH_STATUS_SUCCESS;
}

/* For Emacs:
 * Local Variables:
 * mode:c
 * indent-tabs-mode:t
 * tab-width:4
 * c-basic-offset:4
 * End:
 * For VIM:
 * vim:set softtabstop=4 shiftwidth=4 tabstop=4 expandtab:
 */
