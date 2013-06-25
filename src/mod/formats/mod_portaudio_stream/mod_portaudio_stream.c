/*
 * FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
 * Copyright (C) 2005-2012, Anthony Minessale II <anthm@freeswitch.org>
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
 * mod_portaudio_stream.c -- Portaudio Streaming interface Audio
 *
 */
#include "switch.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "pablio.h"
#include <string.h>

#define DEFAULT_PREBUFFER_SIZE 1024 * 64
#define SAMPLE_TYPE  paInt16
#define PREFERRED_RATE  8000

SWITCH_MODULE_LOAD_FUNCTION(mod_portaudio_stream_load);
SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_portaudio_stream_shutdown);
SWITCH_MODULE_DEFINITION(mod_portaudio_stream, mod_portaudio_stream_load, mod_portaudio_stream_shutdown, NULL);
static switch_memory_pool_t *module_pool = NULL;

struct portaudio_stream_source;

static struct {
	int running;
	int threads;
	switch_mutex_t *mutex;
	switch_hash_t *source_hash;

} globals;


struct portaudio_stream_context {
	struct portaudio_stream_source *source;
	switch_mutex_t *audio_mutex;
	switch_buffer_t *audio_buffer;
	int err;
	const char *func;
	const char *file;
	int line;
	switch_file_handle_t *handle;
	struct portaudio_stream_context *next;
};

typedef struct portaudio_stream_context portaudio_stream_context_t;

struct portaudio_stream_source {
	char *sourcename;
	int sourcedev;
	int rate;
	int interval;
	char *timer_name;
	int total;
	int ready;
	int stopped;
	uint8_t channels;
	switch_size_t samples;
	uint32_t prebuf;
	portaudio_stream_context_t *context_list;
	switch_mutex_t *mutex;
	switch_memory_pool_t *pool;
	switch_thread_rwlock_t *rwlock;
	PABLIO_Stream *audio_stream;
	switch_frame_t read_frame;
	switch_timer_t timer;
	switch_codec_t read_codec;
	switch_codec_t write_codec;
	switch_mutex_t *device_lock;
	unsigned char databuf[SWITCH_RECOMMENDED_BUFFER_SIZE];
};

typedef struct portaudio_stream_source portaudio_stream_source_t;



static int get_dev_by_number(char *numstr, int in)
{
	int numDevices = Pa_GetDeviceCount();
	const PaDeviceInfo *pdi;
	char *end_ptr;
	int number;

	number = (int) strtol(numstr, &end_ptr, 10);

	if (end_ptr == numstr || number < 0) {
		return -1;
	}

	if (number > -1 && number < numDevices && (pdi = Pa_GetDeviceInfo(number))) {
		if (in && pdi->maxInputChannels) {
			return number;
		} else if (!in && pdi->maxOutputChannels) {
			return number;
		}
	}

	return -1;
}

static int get_dev_by_name(char *name, int in)
{
	int i;
	int numDevices;
	const PaDeviceInfo *pdi;
	numDevices = Pa_GetDeviceCount();

	if (numDevices < 0) {
		switch_log_printf(SWITCH_CHANNEL_LOG_CLEAN, SWITCH_LOG_ERROR, "ERROR: Pa_CountDevices returned 0x%x\n", numDevices);
		return -2;
	}

	for (i = 0; i < numDevices; i++) {
		int match = 0;
		pdi = Pa_GetDeviceInfo(i);

		if (zstr(name)) {
			match = 1;
		} else if (pdi && pdi->name && strstr(pdi->name, name)) {
			match = 1;
		}

		if (match) {
			if (in && pdi->maxInputChannels) {
				return i;
			} else if (!in && pdi->maxOutputChannels) {
				return i;
			}
		}
	}

	return -1;
}

static switch_status_t engage_device(portaudio_stream_source_t *source, int restart)
{
	PaStreamParameters inputParameters, outputParameters;
	PaError err;
	int sample_rate = source->rate;
	int codec_ms = source->interval;

	switch_mutex_init(&source->device_lock, SWITCH_MUTEX_NESTED, module_pool);

	if (source->timer.timer_interface) {
		switch_core_timer_sync(&source->timer);
	}

	if (source->audio_stream) {
		return SWITCH_STATUS_SUCCESS;
	}

	if (!switch_core_codec_ready(&source->read_codec)) {
		if (switch_core_codec_init(&source->read_codec,
								   "L16",
								   NULL, sample_rate, codec_ms, 1, SWITCH_CODEC_FLAG_ENCODE | SWITCH_CODEC_FLAG_DECODE, NULL,
								   NULL) != SWITCH_STATUS_SUCCESS) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Can't load codec?\n");
			return SWITCH_STATUS_FALSE;
		}
	}

	switch_assert(source->read_codec.implementation);

	if (!switch_core_codec_ready(&source->write_codec)) {
		if (switch_core_codec_init(&source->write_codec,
								   "L16",
								   NULL,
								   sample_rate, codec_ms, 1, SWITCH_CODEC_FLAG_ENCODE | SWITCH_CODEC_FLAG_DECODE, NULL, NULL) != SWITCH_STATUS_SUCCESS) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Can't load codec?\n");
			switch_core_codec_destroy(&source->read_codec);
			return SWITCH_STATUS_FALSE;
		}
	}


	if (!source->timer.timer_interface) {
		if (switch_core_timer_init(&source->timer,
								   source->timer_name, codec_ms, source->read_codec.implementation->samples_per_packet,
								   module_pool) != SWITCH_STATUS_SUCCESS) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "setup timer failed!\n");
			switch_core_codec_destroy(&source->read_codec);
			switch_core_codec_destroy(&source->write_codec);
			return SWITCH_STATUS_FALSE;
		}
	}

	source->read_frame.rate = sample_rate;
	source->read_frame.codec = &source->read_codec;

	switch_mutex_lock(source->device_lock);
	/* LOCKED ************************************************************************************************** */
	inputParameters.device = source->sourcedev;
	inputParameters.channelCount = 1;
	inputParameters.sampleFormat = SAMPLE_TYPE;
	inputParameters.suggestedLatency = Pa_GetDeviceInfo(inputParameters.device)->defaultLowInputLatency;
	inputParameters.hostApiSpecificStreamInfo = NULL;
	outputParameters.device = source->sourcedev;
	outputParameters.channelCount = 1;
	outputParameters.sampleFormat = SAMPLE_TYPE;
	outputParameters.suggestedLatency = Pa_GetDeviceInfo(outputParameters.device)->defaultLowOutputLatency;
	outputParameters.hostApiSpecificStreamInfo = NULL;


	err = OpenAudioStream(&source->audio_stream, &inputParameters, NULL, sample_rate, paClipOff, source->read_codec.implementation->samples_per_packet, 0);
	/* UNLOCKED ************************************************************************************************* */
	if (err != paNoError) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Error opening audio device retrying\n");
		switch_yield(1000000);
		err = OpenAudioStream(&source->audio_stream, &inputParameters, &outputParameters, sample_rate, paClipOff,
							  source->read_codec.implementation->samples_per_packet, 0);
	}

	switch_mutex_unlock(source->device_lock);
	if (err != paNoError) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Can't open audio device\n");
		switch_core_codec_destroy(&source->read_codec);
		switch_core_timer_destroy(&source->timer);
		return SWITCH_STATUS_FALSE;
	}


	return SWITCH_STATUS_SUCCESS;
}

static void *SWITCH_THREAD_FUNC read_stream_thread(switch_thread_t *thread, void *obj)
{
	portaudio_stream_source_t *source = obj;
	portaudio_stream_context_t *cp;
	int samples = 0;
	int bused, bytesToWrite;


	switch_mutex_lock(globals.mutex);
	globals.threads++;
	switch_mutex_unlock(globals.mutex);

	if (!source->prebuf) {
		source->prebuf = DEFAULT_PREBUFFER_SIZE;
	}



	switch_mutex_lock(globals.mutex);
	switch_core_hash_insert(globals.source_hash, source->sourcename, source);
	switch_mutex_unlock(globals.mutex);


	switch_thread_rwlock_create(&source->rwlock, source->pool);

	if (engage_device(source, 0) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, " Dev %d cant be engaged !\n", (int) source->sourcedev);
	} else {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, " Dev %d engaged at %d rate!\n", (int) source->sourcedev, (int) source->rate);
		if (globals.running && !source->stopped) {
			source->ready = 1;

			if (!source->audio_stream) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "No Audio Stream wops!\n");
				source->stopped = 0;
				source->ready = 0;
			} else {
				while (globals.running && !source->stopped) {
					samples = 0;
					switch_mutex_lock(source->device_lock);
					samples = ReadAudioStream(source->audio_stream, source->databuf,
								  source->read_codec.implementation->samples_per_packet, 0, &source->timer);
					switch_mutex_unlock(source->device_lock);


					if (samples) {
						bytesToWrite = source->samples;
						if (samples < bytesToWrite) {
							bytesToWrite = samples;
						}
						bytesToWrite *= source->audio_stream->bytesPerFrame;

						if (source->total) {

							switch_mutex_lock(source->mutex);
							for (cp = source->context_list; cp; cp = cp->next) {

								switch_mutex_lock(cp->audio_mutex);

								bused = switch_buffer_inuse(cp->audio_buffer);
								if (bused > source->samples * 768) {
									switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Leaking stream handle! [%s() %s:%d] %d used %d max\n",
													  cp->func, cp->file, cp->line, (int) bused, (int) (source->samples * 768));
									switch_buffer_zero(cp->audio_buffer);
								} else {
									switch_buffer_write(cp->audio_buffer, source->databuf, bytesToWrite);
								}

								switch_mutex_unlock(cp->audio_mutex);
							}
							switch_mutex_unlock(source->mutex);
						}

					}

				}
			}

		}
	}


	source->ready = 0;

	switch_mutex_lock(globals.mutex);
	switch_core_hash_delete(globals.source_hash, source->sourcename);
	switch_mutex_unlock(globals.mutex);

	switch_thread_rwlock_wrlock(source->rwlock);
	switch_thread_rwlock_unlock(source->rwlock);


	switch_mutex_lock(source->device_lock);
	CloseAudioStream(source->audio_stream);
	if (switch_core_codec_ready(&source->read_codec)) {
		switch_core_codec_destroy(&source->read_codec);
		switch_core_codec_destroy(&source->write_codec);
	}
	if (switch_core_codec_ready(&source->write_codec)) {
		switch_core_codec_destroy(&source->write_codec);
	}
	switch_mutex_unlock(source->device_lock);


	switch_core_destroy_memory_pool(&source->pool);

	switch_mutex_lock(globals.mutex);
	globals.threads--;
	switch_mutex_unlock(globals.mutex);

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, " thread ending succesfully !\n");
	switch_thread_exit(thread, SWITCH_STATUS_SUCCESS);

	return NULL;
}


static switch_status_t portaudio_stream_file_open(switch_file_handle_t *handle, const char *path)
{
	portaudio_stream_context_t *context;
	portaudio_stream_source_t *source;
	switch_memory_pool_t *pool;
	switch_status_t status = SWITCH_STATUS_FALSE;
	switch_thread_t *thread;
	switch_threadattr_t *thd_attr = NULL;
	uint32_t rate = PREFERRED_RATE;
	char *npath;
	int devNumber;
	int tmp;

	handle->pre_buffer_datalen = 0;

	if (switch_test_flag(handle, SWITCH_FILE_FLAG_WRITE)) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "This format does not support writing! (yet)\n");
		return status;
	}

	npath = switch_core_strdup(module_pool, path);

	tmp = handle->samplerate;
	if (tmp == 8000 || tmp == 16000 || tmp == 32000 || tmp == 48000) {
		rate = tmp;
	}

	if (*path == '#') {
		devNumber = get_dev_by_number(npath + 1, 1);
	} else {
		devNumber = get_dev_by_name(npath, 1);
	}
	npath = switch_mprintf("device-%d at %d", devNumber, rate);

	switch_mutex_lock(globals.mutex);
	source = switch_core_hash_find(globals.source_hash, npath);

	/* dev isnt there, try to start thread */
	if (!source) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, " source isnt Created, create and start thread!\n");

		if (switch_core_new_memory_pool(&pool) != SWITCH_STATUS_SUCCESS) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, " :S no pool\n");
		} else {
			source = switch_core_alloc(pool, sizeof(*source));
			if (source != NULL) {
				source->pool = pool;
				source->sourcedev = devNumber;
				source->sourcename = switch_core_strdup(source->pool, npath);
				source->rate = rate;
				source->interval = 20;
				source->channels = 1;
				source->timer_name = "soft";
				source->prebuf = DEFAULT_PREBUFFER_SIZE;
				source->stopped = 0;
				source->ready = 0;
				source->samples = switch_samples_per_packet(source->rate, source->interval);

				switch_mutex_init(&source->mutex, SWITCH_MUTEX_NESTED, source->pool);

				switch_threadattr_create(&thd_attr, source->pool);
				switch_threadattr_detach_set(thd_attr, 1);

				switch_threadattr_stacksize_set(thd_attr, SWITCH_THREAD_STACKSIZE);
				switch_thread_create(&thread, thd_attr, read_stream_thread, source, source->pool);
			}
		}
	}
	switch_mutex_unlock(globals.mutex);
	switch_yield(1000000);
	/* dev already engaged */
	if (source) {

		/*wait for source to be ready */
		while (source->ready == 0) {
			switch_yield(100000);
		}

		if (switch_thread_rwlock_tryrdlock(source->rwlock) != SWITCH_STATUS_SUCCESS) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, " error rwlock !\n");
			source = NULL;
		}
	}

	if (source) {
		status = SWITCH_STATUS_SUCCESS;

		if ((context = switch_core_alloc(handle->memory_pool, sizeof(*context))) == 0) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, " error allocating context!\n");
			status = SWITCH_STATUS_MEMERR;
		} else {
			/* everything goes fine at this point */
			handle->samples = 0;
			handle->samplerate = source->rate;
			handle->channels = 1;
			handle->format = 0;
			handle->sections = 0;
			handle->seekable = 0;
			handle->speed = 0;
			handle->private_info = context;
			handle->interval = source->interval;

			switch_mutex_init(&context->audio_mutex, SWITCH_MUTEX_NESTED, handle->memory_pool);
			if (switch_buffer_create_dynamic(&context->audio_buffer, 512, 1024, 0) != SWITCH_STATUS_SUCCESS) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Memory Error!\n");
				status = SWITCH_STATUS_MEMERR;
			} else {
				/* context created... then continue */
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
			}
		}
	} else {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Unknown source %s\n", path);
		status = SWITCH_STATUS_FALSE;
	}

	return status;
}


static switch_status_t portaudio_stream_file_close(switch_file_handle_t *handle)
{
	portaudio_stream_context_t *cp, *last = NULL, *context = handle->private_info;


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


static switch_status_t portaudio_stream_file_read(switch_file_handle_t *handle, void *data, size_t *len)
{
	portaudio_stream_context_t *context = handle->private_info;
	switch_size_t bytes = 0;
	int bytesPerSample = context->source->audio_stream->bytesPerFrame;
	size_t need = *len * bytesPerSample;

	if (!context->source->ready) {
		*len = 0;
		return SWITCH_STATUS_FALSE;
	}

	switch_mutex_lock(context->audio_mutex);
	if ((bytes = switch_buffer_read(context->audio_buffer, data, need))) {
		*len = bytes / bytesPerSample;
	} else {
		if (need > 2560) {
			need = 2560;
		}
		memset(data, 255, need);
		*len = need / bytesPerSample;
	}
	switch_mutex_unlock(context->audio_mutex);



	handle->sample_count += *len;
	return SWITCH_STATUS_SUCCESS;


}

/* Registration */

static char *supported_formats[SWITCH_MAX_CODECS] = { 0 };

static void shutdown_event_handler(switch_event_t *event)
{
	globals.running = 0;
}

SWITCH_MODULE_LOAD_FUNCTION(mod_portaudio_stream_load)
{
	switch_file_interface_t *file_interface;
	supported_formats[0] = "portaudio_stream";

	module_pool = pool;

	Pa_Initialize();

	*module_interface = switch_loadable_module_create_module_interface(pool, modname);
	file_interface = switch_loadable_module_create_interface(*module_interface, SWITCH_FILE_INTERFACE);
	file_interface->interface_name = modname;
	file_interface->extens = supported_formats;
	file_interface->file_open = portaudio_stream_file_open;
	file_interface->file_close = portaudio_stream_file_close;
	file_interface->file_read = portaudio_stream_file_read;

	if (switch_event_bind(modname, SWITCH_EVENT_SHUTDOWN, SWITCH_EVENT_SUBCLASS_ANY, shutdown_event_handler, NULL) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Couldn't bind shutdown event handler!\n");
	}

	memset(&globals, 0, sizeof(globals));
	globals.running = 1;
	globals.threads = 0;
	switch_mutex_init(&globals.mutex, SWITCH_MUTEX_NESTED, module_pool);
	switch_core_hash_init(&globals.source_hash, module_pool);


	/* indicate that the module should continue to be loaded */
	return SWITCH_STATUS_SUCCESS;
}

SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_portaudio_stream_shutdown)
{
	globals.running = 0;
	switch_event_unbind_callback(shutdown_event_handler);

	while (globals.threads > 0) {
		switch_yield(100000);
	}

	Pa_Terminate();

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
