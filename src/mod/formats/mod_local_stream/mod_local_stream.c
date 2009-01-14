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
 * Cesar Cepeda <cesar@auronix.com>
 *
 *
 * mod_local_stream.c -- Local Streaming Audio
 *
 */
#include <switch.h>
/* for apr_pstrcat */
#define DEFAULT_PREBUFFER_SIZE 1024 * 64

SWITCH_MODULE_LOAD_FUNCTION(mod_local_stream_load);
SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_local_stream_shutdown);
SWITCH_MODULE_DEFINITION(mod_local_stream, mod_local_stream_load, mod_local_stream_shutdown, NULL);


struct local_stream_source;

static struct {
	switch_mutex_t *mutex;
	switch_hash_t *source_hash;
} globals;

static int RUNNING = 1;
static int THREADS = 0;

struct local_stream_context {
	struct local_stream_source *source;
	switch_mutex_t *audio_mutex;
	switch_buffer_t *audio_buffer;
	int err;
	const char *file;
	const char *func;
	int line;
	switch_file_handle_t *handle;
	struct local_stream_context *next;
};

typedef struct local_stream_context local_stream_context_t;

struct local_stream_source {
	char *name;
	char *location;
	uint8_t channels;
	int rate;
	int interval;
	switch_size_t samples;
	uint32_t prebuf;
	char *timer_name;
	local_stream_context_t *context_list;
	int total;
	switch_dir_t *dir_handle;
	switch_mutex_t *mutex;
	switch_memory_pool_t *pool;
	int shuffle;
	switch_thread_rwlock_t *rwlock;
	int ready;
	int stopped;
};

typedef struct local_stream_source local_stream_source_t;

static unsigned int S = 0;

static int do_rand(void)
{
	double r;
	int index;
	unsigned int seed = ++S + getpid();
	srand(seed);
	r = ((double) rand() / ((double) (RAND_MAX) + (double) (1)));
	index = (int) (r * 9) + 1;
	return index;
}

static void *SWITCH_THREAD_FUNC read_stream_thread(switch_thread_t *thread, void *obj)
{
	local_stream_source_t *source = obj;
	switch_file_handle_t fh = { 0 };
	local_stream_context_t *cp;
	char file_buf[128] = "", path_buf[512] = "";
	switch_timer_t timer = { 0 };
	int fd = -1;
	switch_buffer_t *audio_buffer;
	switch_byte_t *dist_buf;
	switch_size_t used;
	int skip = 0;

	switch_mutex_lock(globals.mutex);
	THREADS++;
	switch_mutex_unlock(globals.mutex);

	if (!source->prebuf) {
		source->prebuf = DEFAULT_PREBUFFER_SIZE;
	}

	switch_buffer_create_dynamic(&audio_buffer, 1024, source->prebuf + 10, 0);
	dist_buf = switch_core_alloc(source->pool, source->prebuf + 10);

	if (source->shuffle) {
		skip = do_rand();
	}

	switch_thread_rwlock_create(&source->rwlock, source->pool);
	
	if (RUNNING) {
		switch_mutex_lock(globals.mutex);
		switch_core_hash_insert(globals.source_hash, source->name, source);
		switch_mutex_unlock(globals.mutex);
		source->ready = 1;
	}

	while (RUNNING && !source->stopped) {
		const char *fname;
		
		if (switch_dir_open(&source->dir_handle, source->location, source->pool) != SWITCH_STATUS_SUCCESS) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CONSOLE, "Can't open directory: %s\n", source->location);
			goto done;
		}

		switch_yield(1000000);

		while (RUNNING) {
			switch_size_t olen;
			uint8_t abuf[SWITCH_RECOMMENDED_BUFFER_SIZE] = { 0 };

			if (fd > -1) {
				char *p;
				if (switch_fd_read_line(fd, path_buf, sizeof(path_buf))) {
					if ((p = strchr(path_buf, '\r')) || (p = strchr(path_buf, '\n'))) {
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

			if (skip > 0) {
				skip--;
				continue;
			}

			fname = path_buf;
			fh.prebuf = source->prebuf;
			fh.pre_buffer_datalen = source->prebuf;

			if (switch_core_file_open(&fh,
									  (char *) fname,
									  source->channels, source->rate, SWITCH_FILE_FLAG_READ | SWITCH_FILE_DATA_SHORT, NULL) != SWITCH_STATUS_SUCCESS) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Can't open %s\n", fname);
				switch_yield(1000000);
				continue;
			}

			source->rate = fh.samplerate;
			source->samples = switch_samples_per_packet(fh.native_rate, source->interval);

			if (switch_core_timer_init(&timer, source->timer_name, source->interval, source->samples, source->pool) != SWITCH_STATUS_SUCCESS) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CONSOLE, "Can't start timer.\n");
				switch_dir_close(source->dir_handle);
				source->dir_handle = NULL;
				goto done;
			}

			while (RUNNING) {
				int is_open;
				switch_core_timer_next(&timer);
				olen = source->samples;
				is_open = switch_test_flag((&fh), SWITCH_FILE_OPEN);

				if (is_open) {
					if (switch_core_file_read(&fh, abuf, &olen) != SWITCH_STATUS_SUCCESS || !olen) {
						switch_core_file_close(&fh);
					}

					switch_buffer_write(audio_buffer, abuf, olen * 2);
				}

				used = switch_buffer_inuse(audio_buffer);

				if (!used && !is_open) {
					break;
				}

				if (!is_open || used >= source->prebuf || (source->total && used > source->samples * 2)) {
					used = switch_buffer_read(audio_buffer, dist_buf, source->samples * 2);
					if (source->total) {

						switch_mutex_lock(source->mutex);
						for (cp = source->context_list; cp; cp = cp->next) {
							if (switch_test_flag(cp->handle, SWITCH_FILE_CALLBACK)) {
								continue;
							}
							switch_mutex_lock(cp->audio_mutex);
							if (switch_buffer_inuse(cp->audio_buffer) > source->samples * 768) {
								switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Leaking stream handle! [%s() %s:%d]\n", cp->func, cp->file,
												  cp->line);
								switch_buffer_zero(cp->audio_buffer);
							} else {
								switch_buffer_write(cp->audio_buffer, dist_buf, used);
							}
							switch_mutex_unlock(cp->audio_mutex);
						}
						switch_mutex_unlock(source->mutex);
					}
				}
			}

			switch_core_timer_destroy(&timer);
			if (RUNNING && source->shuffle) {
				skip = do_rand();
			}
		}

		switch_dir_close(source->dir_handle);
		source->dir_handle = NULL;
	}

 done:

	if (switch_test_flag((&fh), SWITCH_FILE_OPEN)) {
		switch_core_file_close(&fh);
	}

	source->ready = 0;
	switch_mutex_lock(globals.mutex);
	switch_core_hash_delete(globals.source_hash, source->name);
	switch_mutex_unlock(globals.mutex);

	switch_thread_rwlock_wrlock(source->rwlock);
	switch_thread_rwlock_unlock(source->rwlock);
	
	switch_buffer_destroy(&audio_buffer);

	if (fd > -1) {
		close(fd);
	}

	switch_core_destroy_memory_pool(&source->pool);

	switch_mutex_lock(globals.mutex);
	THREADS--;
	switch_mutex_unlock(globals.mutex);

	return NULL;
}

static switch_status_t local_stream_file_open(switch_file_handle_t *handle, const char *path)
{
	local_stream_context_t *context;
	local_stream_source_t *source;
	char *alt_path = NULL;
	switch_status_t status = SWITCH_STATUS_SUCCESS;

	/* already buffering a step back, so always disable it */
	handle->pre_buffer_datalen = 0;

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
	if (source) {
		if (switch_thread_rwlock_tryrdlock(source->rwlock) != SWITCH_STATUS_SUCCESS) {
			source = NULL;
		}
	}
	switch_mutex_unlock(globals.mutex);

	if (!source) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Unknown source %s\n", path);
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
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Opening Stream [%s] %dhz\n", path, handle->samplerate);

	switch_mutex_init(&context->audio_mutex, SWITCH_MUTEX_NESTED, handle->memory_pool);
	if (switch_buffer_create_dynamic(&context->audio_buffer, 512, 1024, 0) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Memory Error!\n");
		status = SWITCH_STATUS_MEMERR;
		goto end;
	}

	context->source = source;
	context->file = handle->file;
	context->func = handle->func;
	context->line = handle->line;
	context->handle = handle;
	switch_mutex_lock(source->mutex);
	context->next = source->context_list;
	source->context_list = context;
	source->total++;
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
	context->source->total--;
	switch_mutex_unlock(context->source->mutex);
	switch_buffer_destroy(&context->audio_buffer);
	switch_thread_rwlock_unlock(context->source->rwlock);
	
	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t local_stream_file_read(switch_file_handle_t *handle, void *data, size_t *len)
{
	local_stream_context_t *context = handle->private_info;
	switch_size_t bytes = 0;
	size_t need = *len * 2;

	if (!context->source->ready) {
		*len = 0;
		return SWITCH_STATUS_FALSE;
	}

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
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Open of %s failed\n", cf);
		return;
	}

	for (directory = switch_xml_child(cfg, "directory"); directory; directory = directory->next) {
		char *path = (char *) switch_xml_attr(directory, "path");
		char *name = (char *) switch_xml_attr(directory, "name");

		if (!(name && path)) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Invalid config!\n");
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
		source->prebuf = DEFAULT_PREBUFFER_SIZE;
		source->stopped = 0;

		for (param = switch_xml_child(directory, "param"); param; param = param->next) {
			char *var = (char *) switch_xml_attr_soft(param, "name");
			char *val = (char *) switch_xml_attr_soft(param, "value");

			if (!strcasecmp(var, "rate")) {
				int tmp = atoi(val);
				if (tmp == 8000 || tmp == 16000 || tmp == 32000 || tmp == 48000) {
					source->rate = tmp;
				}
			} else if (!strcasecmp(var, "shuffle")) {
				source->shuffle = switch_true(val);
			} else if (!strcasecmp(var, "prebuf")) {
				int tmp = atoi(val);
				if (tmp > 0) {
					source->prebuf = (uint32_t) tmp;
				}
			} else if (!strcasecmp(var, "channels")) {
				int tmp = atoi(val);
				if (tmp == 1 || tmp == 2) {
					source->channels = (uint8_t) tmp;
				}
			} else if (!strcasecmp(var, "interval")) {
				int tmp = atoi(val);
				if (SWITCH_ACCEPTABLE_INTERVAL(tmp)) {
					source->interval = tmp;
				} else {
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING,
									  "Interval must be multiple of 10 and less than %d, Using default of 20\n", SWITCH_MAX_INTERVAL);
				}
			} else if (!strcasecmp(var, "timer-name")) {
				source->timer_name = switch_core_strdup(source->pool, val);
			}
		}

		source->samples = switch_samples_per_packet(source->rate, source->interval);

		switch_mutex_init(&source->mutex, SWITCH_MUTEX_NESTED, source->pool);

		switch_threadattr_create(&thd_attr, source->pool);
		switch_threadattr_detach_set(thd_attr, 1);
		switch_threadattr_stacksize_set(thd_attr, SWITCH_THREAD_STACKSIZE);
		switch_thread_create(&thread, thd_attr, read_stream_thread, source, source->pool);
	}

	switch_xml_free(xml);
}

static void event_handler(switch_event_t *event)
{
	RUNNING = 0;
}

#define STOP_LOCAL_STREAM_SYNTAX "<local_stream_name>"
SWITCH_STANDARD_API(stop_local_stream_function)
{
	local_stream_source_t *source = NULL;
	char *mycmd = NULL, *argv[1] = { 0 };
	char *local_stream_name = NULL;
	int argc = 0;


	if (switch_strlen_zero(cmd)) {
		goto usage;
	}

	if (!(mycmd = strdup(cmd))) {
		goto usage;
	}

	if ((argc = switch_separate_string(mycmd, ' ', argv, (sizeof(argv) / sizeof(argv[0])))) < 1) {
		goto usage;
	}

	local_stream_name = argv[0];
	if (switch_strlen_zero(local_stream_name)) {
		goto usage;
	}

	switch_mutex_lock(globals.mutex);
	source = switch_core_hash_find(globals.source_hash, local_stream_name);
	switch_mutex_unlock(globals.mutex);

	if (!source) {
		stream->write_function(stream, "-ERR Cannot locate local_stream %s!\n",local_stream_name);
		goto done;
	}

	source->stopped = 1;
	stream->write_function(stream,"+OK");
	goto done;

 usage:
	stream->write_function(stream, "-USAGE: %s\n", STOP_LOCAL_STREAM_SYNTAX);
	switch_safe_free(mycmd);

 done:

	switch_safe_free(mycmd);
	return SWITCH_STATUS_SUCCESS;
}

#define START_LOCAL_STREAM_SYNTAX "<local_stream_name> [<path>] [<rate>] [<shuffle>] [<prebuf>] [<channels>] [<interval>] [<timer_name>]"
SWITCH_STANDARD_API(start_local_stream_function)
{
	local_stream_source_t *source = NULL;
	switch_memory_pool_t *pool;
	switch_thread_t *thread;
	switch_threadattr_t *thd_attr = NULL;
	char *mycmd = NULL, *argv[8] = { 0 };
	char *local_stream_name = NULL, *path = NULL, *timer_name = NULL;
	uint32_t prebuf = 1;
	int rate = 8000, shuffle = 1, interval = 20;
	uint8_t channels = 1;
	int argc = 0;
	char *cf = "local_stream.conf";
	switch_xml_t cfg, xml, directory, param;
	int tmp;

	if (switch_strlen_zero(cmd)) {
		goto usage;
	}

	if (!(mycmd = strdup(cmd))) {
		goto usage;
	}

	if ((argc = switch_separate_string(mycmd, ' ', argv, (sizeof(argv) / sizeof(argv[0])))) < 1) {
		goto usage;
	}

	local_stream_name = argv[0];

	if (argv[1]) {
		path = strdup(argv[1]);
	}

	if (argv[2]) {
		tmp = atoi(argv[2]);
		if (tmp == 8000 || tmp == 16000 || tmp == 32000) {
			rate = tmp;
		}
	}

	shuffle = argv[3] ? switch_true(argv[3]) : 1;
	prebuf = argv[4] ? atoi(argv[4]) : DEFAULT_PREBUFFER_SIZE;

	if (argv[5]) {
		tmp = atoi(argv[5]);
		if (tmp == 1 || tmp == 2) {
			channels = (uint8_t)tmp;
		}
	}

	interval = argv[6] ? atoi(argv[6]) : 20;
	
	if (!SWITCH_ACCEPTABLE_INTERVAL(interval)){
		interval = 20;
	}

	if (!path){
		if (!(xml = switch_xml_open_cfg(cf, &cfg, NULL))) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Open of %s failed\n", cf);
			stream->write_function(stream, "-ERR unable to open file %s!\n",cf);
			goto done;
		}

		for (directory = switch_xml_child(cfg, "directory"); directory; directory = directory->next) {
			char *name = (char *) switch_xml_attr(directory, "name");
			if (!name || !local_stream_name || strcasecmp(name, local_stream_name)){
				continue;
			}
			else {
				path = (char *) switch_xml_attr(directory, "path");
				if (!(name && path)) {
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Invalid config!\n");
					continue;
				}

				for (param = switch_xml_child(directory, "param"); param; param = param->next) {
					char *var = (char *) switch_xml_attr_soft(param, "name");
					char *val = (char *) switch_xml_attr_soft(param, "value");

					if (!strcasecmp(var, "rate")) {
						tmp = atoi(val);
						if (tmp == 8000 || tmp == 16000 || tmp == 32000) {
							rate = tmp;
						}
					} else if (!strcasecmp(var, "shuffle")) {
						shuffle = switch_true(val);
					} else if (!strcasecmp(var, "prebuf")) {
						tmp = atoi(val);
						if (tmp > 0) {
							prebuf = (uint32_t) tmp;
						}
					} else if (!strcasecmp(var, "channels")) {
						tmp = atoi(val);
						if (tmp == 1 || tmp == 2) {
							channels = (uint8_t) tmp;
						}
					} else if (!strcasecmp(var, "interval")) {
						tmp = atoi(val);
						if (SWITCH_ACCEPTABLE_INTERVAL(tmp)) {
							interval = tmp;
						} else {
							switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING,
											  "Interval must be multiple of 10 and less than %d, Using default of 20\n", SWITCH_MAX_INTERVAL);
						}
					} else if (!strcasecmp(var, "timer-name")) {
						timer_name = strdup(val);
					}
				}
				break;
			}

		}

		if (path) {
			path = strdup(path);
		}
		switch_xml_free(xml);
	}
	
	if (switch_strlen_zero(local_stream_name) || switch_strlen_zero(path)) {
		goto usage;
	}

	switch_mutex_lock(globals.mutex);
	source = switch_core_hash_find(globals.source_hash, local_stream_name);
	switch_mutex_unlock(globals.mutex);
	if (source) {
		source->stopped = 0;
		stream->write_function(stream,"+OK");
		goto done; 
	}

	if (switch_core_new_memory_pool(&pool) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "OH OH no pool for new local_stream\n");
		stream->write_function(stream, "-ERR unable to allocate memory for local_stream %s!\n",local_stream_name);
		goto done;
	}

	source = switch_core_alloc(pool, sizeof(*source));
	assert(source != NULL);
	source->pool = pool;

	source->name = switch_core_strdup(source->pool, local_stream_name);
	source->location = switch_core_strdup(source->pool, path);
	source->rate = rate;
	source->interval = interval;
	source->channels = channels;
	source->timer_name = switch_core_strdup(source->pool, timer_name ? timer_name : (argv[7] ? argv[7] : "soft"));
	source->prebuf = prebuf;
	source->stopped = 0;

	source->samples = switch_samples_per_packet(source->rate, source->interval);

	switch_mutex_init(&source->mutex, SWITCH_MUTEX_NESTED, source->pool);

	switch_threadattr_create(&thd_attr, source->pool);
	switch_threadattr_detach_set(thd_attr, 1);
	switch_threadattr_stacksize_set(thd_attr, SWITCH_THREAD_STACKSIZE);
	switch_thread_create(&thread, thd_attr, read_stream_thread, source, source->pool);

	stream->write_function(stream,"+OK");
	goto done;

 usage:
	stream->write_function(stream, "-USAGE: %s\n", START_LOCAL_STREAM_SYNTAX);

 done:

	switch_safe_free(path);
	switch_safe_free(timer_name);
	switch_safe_free(mycmd);
	return SWITCH_STATUS_SUCCESS;
}

SWITCH_MODULE_LOAD_FUNCTION(mod_local_stream_load)
{
	switch_api_interface_t *commands_api_interface;
	switch_file_interface_t *file_interface;
	supported_formats[0] = "local_stream";

	*module_interface = switch_loadable_module_create_module_interface(pool, modname);
	file_interface = switch_loadable_module_create_interface(*module_interface, SWITCH_FILE_INTERFACE);
	file_interface->interface_name = modname;
	file_interface->extens = supported_formats;
	file_interface->file_open = local_stream_file_open;
	file_interface->file_close = local_stream_file_close;
	file_interface->file_read = local_stream_file_read;

	if (switch_event_bind(modname, SWITCH_EVENT_SHUTDOWN, SWITCH_EVENT_SUBCLASS_ANY, event_handler, NULL) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Couldn't bind event handler!\n");
	}

	memset(&globals, 0, sizeof(globals));
	switch_mutex_init(&globals.mutex, SWITCH_MUTEX_NESTED, pool);
	switch_core_hash_init(&globals.source_hash, pool);
	launch_threads();

	SWITCH_ADD_API(commands_api_interface, "stop_local_stream", "Stops and unloads a local_stream", stop_local_stream_function, STOP_LOCAL_STREAM_SYNTAX);
	SWITCH_ADD_API(commands_api_interface, "start_local_stream", "Starts a new local_stream", start_local_stream_function, START_LOCAL_STREAM_SYNTAX);

	/* indicate that the module should continue to be loaded */
	return SWITCH_STATUS_SUCCESS;
}

SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_local_stream_shutdown)
{
	RUNNING = 0;
	switch_event_unbind_callback(event_handler);

	while(THREADS > 0) {
		switch_yield(100000);
	}

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
 * vim:set softtabstop=4 shiftwidth=4 tabstop=4:
 */
