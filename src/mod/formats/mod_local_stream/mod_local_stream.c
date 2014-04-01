/* 
 * FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
 * Copyright (C) 2005-2014, Anthony Minessale II <anthm@freeswitch.org>
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
 * Anthony Minessale II <anthm@freeswitch.org>
 * Portions created by the Initial Developer are Copyright (C)
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 * 
 * Anthony Minessale II <anthm@freeswitch.org>
 * Cesar Cepeda <cesar@auronix.com>
 * Emmanuel Schmidbauer <e.schmidbauer@gmail.com>
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

static int launch_streams(const char *name);
static void launch_thread(const char *name, const char *path, switch_xml_t directory);

static const char *global_cf = "local_stream.conf";

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

#define MAX_CHIME 100
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
	int hup;
	int ready;
	int stopped;
	int part_reload;
	int full_reload;
	int chime_freq;
	int chime_total;
	int chime_max;
	int chime_cur;
	char *chime_list[MAX_CHIME];
	int32_t chime_counter;
	int32_t chime_max_counter;
	switch_file_handle_t chime_fh;
};

typedef struct local_stream_source local_stream_source_t;

static int do_rand(void)
{
	double r;
	int index;
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
	switch_memory_pool_t *temp_pool = NULL;

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

		if (temp_pool) {
			switch_core_destroy_memory_pool(&temp_pool);
		}

		if (switch_core_new_memory_pool(&temp_pool) != SWITCH_STATUS_SUCCESS) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Error creating pool");
			goto done;
		}

		if (switch_dir_open(&source->dir_handle, source->location, temp_pool) != SWITCH_STATUS_SUCCESS) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CONSOLE, "Can't open directory: %s\n", source->location);
			goto done;
		}

		switch_yield(1000000);

		while (RUNNING && !source->stopped) {
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

			if (switch_core_timer_init(&timer, source->timer_name, source->interval, (int)source->samples, temp_pool) != SWITCH_STATUS_SUCCESS) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CONSOLE, "Can't start timer.\n");
				switch_dir_close(source->dir_handle);
				source->dir_handle = NULL;
				goto done;
			}

			while (RUNNING && !source->stopped) {
				int is_open;
				switch_file_handle_t *use_fh = &fh;

				switch_core_timer_next(&timer);
				olen = source->samples;

				if (source->chime_total) {

					if (source->chime_counter > 0) {
						source->chime_counter -= (int32_t)source->samples;
					}

					if (!switch_test_flag((&source->chime_fh), SWITCH_FILE_OPEN) && source->chime_counter <= 0) {
						char *val;

						val = source->chime_list[source->chime_cur++];

						if (source->chime_cur >= source->chime_total) {
							source->chime_cur = 0;
						}

						if (switch_core_file_open(&source->chime_fh,
												  (char *) val,
												  source->channels,
												  source->rate, SWITCH_FILE_FLAG_READ | SWITCH_FILE_DATA_SHORT, NULL) != SWITCH_STATUS_SUCCESS) {
							switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Can't open %s\n", val);
						}
					}

					if (switch_test_flag((&source->chime_fh), SWITCH_FILE_OPEN)) {
						use_fh = &source->chime_fh;
					}
				}

			  retry:

				is_open = switch_test_flag(use_fh, SWITCH_FILE_OPEN);

				if (source->hup) {
					source->hup = 0;
					if (is_open) {
						is_open = 0;

						switch_core_file_close(use_fh);
						if (use_fh == &source->chime_fh) {
							source->chime_counter = source->rate * source->chime_freq;
							use_fh = &fh;
							goto retry;
							//switch_core_file_close(&fh);
						}
					}
				}



				if (is_open) {
					if (switch_core_file_read(use_fh, abuf, &olen) != SWITCH_STATUS_SUCCESS || !olen) {
						switch_core_file_close(use_fh);
						if (use_fh == &source->chime_fh) {
							source->chime_counter = source->rate * source->chime_freq;
						}
						is_open = 0;
					} else {
						if (use_fh == &source->chime_fh && source->chime_max) {
							source->chime_max_counter += (int32_t)source->samples;
							if (source->chime_max_counter >= source->chime_max) {
								source->chime_max_counter = 0;
								switch_core_file_close(use_fh);
								source->chime_counter = source->rate * source->chime_freq;
								use_fh = &fh;
								goto retry;
							}
						}

						switch_buffer_write(audio_buffer, abuf, olen * 2);
					}
				}

				used = switch_buffer_inuse(audio_buffer);

				if (!used && !is_open) {
					break;
				}

				if (!is_open || used >= source->prebuf || (source->total && used > source->samples * 2)) {
					used = switch_buffer_read(audio_buffer, dist_buf, source->samples * 2);
					if (source->total) {
						uint32_t bused = 0;
						switch_mutex_lock(source->mutex);
						for (cp = source->context_list; cp && RUNNING; cp = cp->next) {
							if (switch_test_flag(cp->handle, SWITCH_FILE_CALLBACK)) {
								continue;
							}
							switch_mutex_lock(cp->audio_mutex);
							bused = (uint32_t)switch_buffer_inuse(cp->audio_buffer);
							if (bused > source->samples * 768) {
								switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG1, "Flushing Stream Handle Buffer [%s() %s:%d] size: %u samples: %ld\n", 
												  cp->func, cp->file, cp->line, bused, (long)source->samples);
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

		if (source->full_reload) {
			if (source->rwlock && switch_thread_rwlock_trywrlock(source->rwlock) != SWITCH_STATUS_SUCCESS) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Cannot stop local_stream://%s because it is in use.\n",source->name);
				if (source->part_reload) {
					switch_xml_t cfg, xml, directory, param;
					if (!(xml = switch_xml_open_cfg(global_cf, &cfg, NULL))) {
						switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Open of %s failed\n", global_cf);
					}
					if ((directory = switch_xml_find_child(cfg, "directory", "name", source->name))) {
						for (param = switch_xml_child(directory, "param"); param; param = param->next) {
							char *var = (char *) switch_xml_attr_soft(param, "name");
							char *val = (char *) switch_xml_attr_soft(param, "value");
							if (!strcasecmp(var, "shuffle")) {
								source->shuffle = switch_true(val);
							} else if (!strcasecmp(var, "chime-freq")) {
								int tmp = atoi(val);
								if (tmp > 1) {
									source->chime_freq = tmp;
								}
							} else if (!strcasecmp(var, "chime-max")) {
								int tmp = atoi(val);
								if (tmp > 1) {
									source->chime_max = tmp;
								}
							} else if (!strcasecmp(var, "chime-list")) {
								char *list_dup = switch_core_strdup(source->pool, val);
								source->chime_total =
									switch_separate_string(list_dup, ',', source->chime_list, (sizeof(source->chime_list) / sizeof(source->chime_list[0])));
							} else if (!strcasecmp(var, "interval")) {
								int tmp = atoi(val);
								if (SWITCH_ACCEPTABLE_INTERVAL(tmp)) {
									source->interval = tmp;
								} else {
									switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING,
													  "Interval must be multiple of 10 and less than %d, Using default of 20\n", SWITCH_MAX_INTERVAL);
								}
							}
							if (source->chime_max) {
								source->chime_max *= source->rate;
							}
							if (source->chime_total) {
								source->chime_counter = source->rate * source->chime_freq;
							}
						}
					}
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "local_stream://%s partially reloaded.\n",source->name);
					source->part_reload = 0;
				}
			} else {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "local_stream://%s fully reloaded.\n",source->name);
				launch_streams(source->name);
				goto done;
			}
		}
	}

  done:

	if (switch_test_flag((&fh), SWITCH_FILE_OPEN)) {
		switch_core_file_close(&fh);
	}

	if (switch_test_flag((&source->chime_fh), SWITCH_FILE_OPEN)) {
		switch_core_file_close(&source->chime_fh);
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

	if (temp_pool) {
		switch_core_destroy_memory_pool(&temp_pool);
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

	switch_mutex_lock(globals.mutex);

  top:

	alt_path = switch_mprintf("%s/%d", path, handle->samplerate);

	if ((source = switch_core_hash_find(globals.source_hash, alt_path))) {
		path = alt_path;
	} else {
		source = switch_core_hash_find(globals.source_hash, path);
	}
	if (source) {
		if (switch_thread_rwlock_tryrdlock(source->rwlock) != SWITCH_STATUS_SUCCESS) {
			source = NULL;
		}
	} else {
		if (!switch_stristr("default", alt_path) && !switch_stristr("default", path)) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "Unknown source %s, trying 'default'\n", path);
			free(alt_path);
			path = "default";
			goto top;
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

static void launch_thread(const char *name, const char *path, switch_xml_t directory)
{
	local_stream_source_t *source = NULL;
	switch_memory_pool_t *pool;
	switch_xml_t param;
	switch_thread_t *thread;
	switch_threadattr_t *thd_attr = NULL;

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
	source->hup = 0;
	source->chime_freq = 30;
	for (param = switch_xml_child(directory, "param"); param; param = param->next) {
		char *var = (char *) switch_xml_attr_soft(param, "name");
		char *val = (char *) switch_xml_attr_soft(param, "value");

		if (!strcasecmp(var, "rate")) {
			int tmp = atoi(val);
			if (tmp == 8000 || tmp == 12000 || tmp == 16000 || tmp == 24000 || tmp == 32000 || tmp == 48000) {
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
		} else if (!strcasecmp(var, "chime-freq")) {
			int tmp = atoi(val);
			if (tmp > 1) {
				source->chime_freq = tmp;
			}
		} else if (!strcasecmp(var, "chime-max")) {
			int tmp = atoi(val);
			if (tmp > 1) {
				source->chime_max = tmp;
			}
		} else if (!strcasecmp(var, "chime-list")) {
			char *list_dup = switch_core_strdup(source->pool, val);
			source->chime_total =
				switch_separate_string(list_dup, ',', source->chime_list, (sizeof(source->chime_list) / sizeof(source->chime_list[0])));
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

	if (source->chime_max) {
		source->chime_max *= source->rate;
	}

	if (source->chime_total) {
		source->chime_counter = source->rate * source->chime_freq;
	}

	source->samples = switch_samples_per_packet(source->rate, source->interval);
	switch_mutex_init(&source->mutex, SWITCH_MUTEX_NESTED, source->pool);
	switch_threadattr_create(&thd_attr, source->pool);
	switch_threadattr_detach_set(thd_attr, 1);
	switch_threadattr_stacksize_set(thd_attr, SWITCH_THREAD_STACKSIZE);
	switch_thread_create(&thread, thd_attr, read_stream_thread, source, source->pool);
}

static int launch_streams(const char *name)
{
	switch_xml_t cfg, xml, directory;
	int x = 0;

	if (!(xml = switch_xml_open_cfg(global_cf, &cfg, NULL))) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Open of %s failed\n", global_cf);
		return 0;
	}

	if (zstr(name)) {
		for (directory = switch_xml_child(cfg, "directory"); directory; directory = directory->next) {
			char *name = (char *) switch_xml_attr(directory, "name");
			char *path = (char *) switch_xml_attr(directory, "path");
			launch_thread(name, path, directory);
			x++;
		}
	} else if ((directory = switch_xml_find_child(cfg, "directory", "name", name))) {
		char *path = (char *) switch_xml_attr(directory, "path");
		launch_thread(name, path, directory);
		x++;
	}
	switch_xml_free(xml);

	return x;
}

static void event_handler(switch_event_t *event)
{
	RUNNING = 0;
}

#define RELOAD_LOCAL_STREAM_SYNTAX "<local_stream_name>"
SWITCH_STANDARD_API(reload_local_stream_function)
{
	local_stream_source_t *source = NULL;
	char *mycmd = NULL, *argv[1] = { 0 };
	char *local_stream_name = NULL;
	int argc = 0;


	if (zstr(cmd)) {
		goto usage;
	}

	if (!(mycmd = strdup(cmd))) {
		goto usage;
	}

	if ((argc = switch_separate_string(mycmd, ' ', argv, (sizeof(argv) / sizeof(argv[0])))) < 1) {
		goto usage;
	}

	local_stream_name = argv[0];
	if (zstr(local_stream_name)) {
		goto usage;
	}

	switch_mutex_lock(globals.mutex);
	source = switch_core_hash_find(globals.source_hash, local_stream_name);
	switch_mutex_unlock(globals.mutex);

	if (!source) {
		stream->write_function(stream, "-ERR Cannot locate local_stream %s!\n", local_stream_name);
		goto done;
	}

	source->full_reload = 1;
	source->part_reload = 1;
	stream->write_function(stream, "+OK");
	goto done;

  usage:
	stream->write_function(stream, "-USAGE: %s\n", RELOAD_LOCAL_STREAM_SYNTAX);
	switch_safe_free(mycmd);

  done:

	switch_safe_free(mycmd);
	return SWITCH_STATUS_SUCCESS;
}

#define STOP_LOCAL_STREAM_SYNTAX "<local_stream_name>"
SWITCH_STANDARD_API(stop_local_stream_function)
{
	local_stream_source_t *source = NULL;
	char *mycmd = NULL, *argv[1] = { 0 };
	char *local_stream_name = NULL;
	int argc = 0;


	if (zstr(cmd)) {
		goto usage;
	}

	if (!(mycmd = strdup(cmd))) {
		goto usage;
	}

	if ((argc = switch_separate_string(mycmd, ' ', argv, (sizeof(argv) / sizeof(argv[0])))) < 1) {
		goto usage;
	}

	local_stream_name = argv[0];
	if (zstr(local_stream_name)) {
		goto usage;
	}

	switch_mutex_lock(globals.mutex);
	source = switch_core_hash_find(globals.source_hash, local_stream_name);
	switch_mutex_unlock(globals.mutex);

	if (!source) {
		stream->write_function(stream, "-ERR Cannot locate local_stream %s!\n", local_stream_name);
		goto done;
	}

	source->stopped = 1;
	stream->write_function(stream, "+OK");
	goto done;

  usage:
	stream->write_function(stream, "-USAGE: %s\n", STOP_LOCAL_STREAM_SYNTAX);
	switch_safe_free(mycmd);

  done:

	switch_safe_free(mycmd);
	return SWITCH_STATUS_SUCCESS;
}

#define SHOW_LOCAL_STREAM_SYNTAX "[local_stream_name [xml]]"
SWITCH_STANDARD_API(show_local_stream_function)
{
	local_stream_source_t *source = NULL;
	char *mycmd = NULL, *argv[2] = { 0 };
	char *local_stream_name = NULL;
	int argc = 0;
	switch_hash_index_t *hi;
	const void *var;
	void *val;
	switch_bool_t xml = SWITCH_FALSE;

	switch_mutex_lock(globals.mutex);

	if (zstr(cmd)) {
		for (hi = switch_core_hash_first( globals.source_hash); hi; hi = switch_core_hash_next(&hi)) {
			switch_core_hash_this(hi, &var, NULL, &val);
			if ((source = (local_stream_source_t *) val)) {
				stream->write_function(stream, "%s,%s\n", source->name, source->location);
			}
		}
	} else {
		if (!(mycmd = strdup(cmd))) {
			goto usage;
		}

		if ((argc = switch_separate_string(mycmd, ' ', argv, (sizeof(argv) / sizeof(argv[0]))))) {
			local_stream_name = argv[0];
			if (argc > 1 && !strcasecmp("xml", argv[1])) {
				xml = SWITCH_TRUE;
			}
		}

		if (!local_stream_name) {
			goto usage;
		}

		source = switch_core_hash_find(globals.source_hash, local_stream_name);
		if (source) {
			if (xml) {
				stream->write_function(stream, "<?xml version=\"1.0\"?>\n<local_stream name=\"%s\">\n", source->name);
				stream->write_function(stream, "  <location>%s</location>\n", source->location);
				stream->write_function(stream, "  <channels>%d</channels>\n", source->channels);
				stream->write_function(stream, "  <rate>%d</rate>\n", source->rate);
				stream->write_function(stream, "  <interval>%d<interval>\n", source->interval);
				stream->write_function(stream, "  <samples>%d</samples>\n", source->samples);
				stream->write_function(stream, "  <prebuf>%d</prebuf>\n", source->prebuf);
				stream->write_function(stream, "  <timer>%s</timer>\n", source->timer_name);
				stream->write_function(stream, "  <total>%d</total>\n", source->total);
				stream->write_function(stream, "  <shuffle>%s</shuffle>\n", (source->shuffle) ? "true" : "false");
				stream->write_function(stream, "  <ready>%s</ready>\n", (source->ready) ? "true" : "false");
				stream->write_function(stream, "  <stopped>%s</stopped>\n", (source->stopped) ? "true" : "false");
				stream->write_function(stream, "</local_stream>\n");
			} else {
				stream->write_function(stream, "%s\n", source->name);
				stream->write_function(stream, "  location: %s\n", source->location);
				stream->write_function(stream, "  channels: %d\n", source->channels);
				stream->write_function(stream, "  rate:     %d\n", source->rate);
				stream->write_function(stream, "  interval: %d\n", source->interval);
				stream->write_function(stream, "  samples:  %d\n", source->samples);
				stream->write_function(stream, "  prebuf:   %d\n", source->prebuf);
				stream->write_function(stream, "  timer:    %s\n", source->timer_name);
				stream->write_function(stream, "  total:    %d\n", source->total);
				stream->write_function(stream, "  shuffle:  %s\n", (source->shuffle) ? "true" : "false");
				stream->write_function(stream, "  ready:    %s\n", (source->ready) ? "true" : "false");
				stream->write_function(stream, "  stopped:  %s\n", (source->stopped) ? "true" : "false");
				stream->write_function(stream, "  reloading: %s\n", (source->full_reload) ? "true" : "false");
			}
		} else {
			stream->write_function(stream, "-ERR Cannot locate local_stream %s!\n", local_stream_name);
			goto done;
		}
	}

	stream->write_function(stream, "+OK");
	goto done;

  usage:
	stream->write_function(stream, "-USAGE: %s\n", SHOW_LOCAL_STREAM_SYNTAX);

  done:

	switch_mutex_unlock(globals.mutex);
	switch_safe_free(mycmd);
	return SWITCH_STATUS_SUCCESS;
}

#define START_LOCAL_STREAM_SYNTAX "<local_stream_name>"
SWITCH_STANDARD_API(start_local_stream_function)
{
	local_stream_source_t *source = NULL;
	char *mycmd = NULL, *argv[8] = { 0 };
	char *local_stream_name = NULL;
	int argc = 0;
	int ok = 0;

	if (zstr(cmd)) {
		goto usage;
	}

	if (!(mycmd = strdup(cmd))) {
		goto usage;
	}

	if ((argc = switch_separate_string(mycmd, ' ', argv, (sizeof(argv) / sizeof(argv[0])))) < 1) {
		goto usage;
	}

	local_stream_name = argv[0];

	switch_mutex_lock(globals.mutex);
	source = switch_core_hash_find(globals.source_hash, local_stream_name);
	switch_mutex_unlock(globals.mutex);
	if (source) {
		source->stopped = 0;
		stream->write_function(stream, "+OK stream: %s", source->name);
		goto done;
	}

	if ((ok = launch_streams(local_stream_name))) {
		stream->write_function(stream, "+OK stream: %s", local_stream_name);
		goto done;
	}

  usage:
	stream->write_function(stream, "-USAGE: %s\n", START_LOCAL_STREAM_SYNTAX);

  done:

	switch_safe_free(mycmd);
	return SWITCH_STATUS_SUCCESS;
}

#define HUP_LOCAL_STREAM_SYNTAX "<local_stream_name>"
SWITCH_STANDARD_API(hup_local_stream_function)
{
	local_stream_source_t *source = NULL;
	char *mycmd = NULL, *argv[8] = { 0 };
	char *local_stream_name = NULL;
	int argc = 0;

	if (zstr(cmd)) {
		goto usage;
	}

	if (!(mycmd = strdup(cmd))) {
		goto usage;
	}

	if ((argc = switch_separate_string(mycmd, ' ', argv, (sizeof(argv) / sizeof(argv[0])))) < 1) {
		goto usage;
	}

	local_stream_name = argv[0];

	switch_mutex_lock(globals.mutex);
	source = switch_core_hash_find(globals.source_hash, local_stream_name);
	switch_mutex_unlock(globals.mutex);

	if (source) {
		source->hup = 1;
		stream->write_function(stream, "+OK hup stream: %s", source->name);
		goto done;
	}

	goto done;

  usage:
	stream->write_function(stream, "-USAGE: %s\n", START_LOCAL_STREAM_SYNTAX);

  done:

	switch_safe_free(mycmd);
	return SWITCH_STATUS_SUCCESS;
}

SWITCH_MODULE_LOAD_FUNCTION(mod_local_stream_load)
{
	switch_api_interface_t *commands_api_interface;
	switch_file_interface_t *file_interface;

	supported_formats[0] = "local_stream";


	memset(&globals, 0, sizeof(globals));
	switch_mutex_init(&globals.mutex, SWITCH_MUTEX_NESTED, pool);
	switch_core_hash_init(&globals.source_hash);
	if (!launch_streams(NULL)) {
		return SWITCH_STATUS_GENERR;
	}
	
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



	SWITCH_ADD_API(commands_api_interface, "hup_local_stream", "Skip to next file in local_stream", hup_local_stream_function, RELOAD_LOCAL_STREAM_SYNTAX);
	SWITCH_ADD_API(commands_api_interface, "reload_local_stream", "Reloads a local_stream", reload_local_stream_function, RELOAD_LOCAL_STREAM_SYNTAX);
	SWITCH_ADD_API(commands_api_interface, "stop_local_stream", "Stops and unloads a local_stream", stop_local_stream_function, STOP_LOCAL_STREAM_SYNTAX);
	SWITCH_ADD_API(commands_api_interface, "start_local_stream", "Starts a new local_stream", start_local_stream_function, START_LOCAL_STREAM_SYNTAX);
	SWITCH_ADD_API(commands_api_interface, "show_local_stream", "Shows a local stream", show_local_stream_function, SHOW_LOCAL_STREAM_SYNTAX);

	/* indicate that the module should continue to be loaded */
	return SWITCH_STATUS_SUCCESS;
}

SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_local_stream_shutdown)
{
	RUNNING = 0;
	switch_event_unbind_callback(event_handler);

	while (THREADS > 0) {
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
 * vim:set softtabstop=4 shiftwidth=4 tabstop=4 noet:
 */
