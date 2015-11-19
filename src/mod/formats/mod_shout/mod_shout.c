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
 * Rupa Schomaker <rupa@rupa.com>
 * John Wehle <john@feith.com>
 *
 * mod_shout.c -- Icecast Module
 *
 * example filename: shout://user:pass@host.com/mount.mp3
 *
 */
#include <switch.h>
#include "mpg123.h"
#include <shout/shout.h>
#include <lame.h>
#include <switch_curl.h>

#define OUTSCALE 8192 * 2
#define MP3_SCACHE 16384 * 2
#define MP3_DCACHE 8192 * 2
#define MP3_TOOSMALL -1234

SWITCH_MODULE_LOAD_FUNCTION(mod_shout_load);
SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_shout_shutdown);
SWITCH_MODULE_DEFINITION(mod_shout, mod_shout_load, mod_shout_shutdown, NULL);

static char *supported_formats[SWITCH_MAX_CODECS] = { 0 };

static struct {
	char decoder[256];
	float vol;
	uint32_t outscale;
	uint32_t brate;
	uint32_t resample;
	uint32_t quality;
} globals;

mpg123_handle *our_mpg123_new(const char *decoder, int *error)
{
	const char *arch = "auto";
	const char *err = NULL;
	mpg123_handle *mh;
	int x64 = 0;
	int rc = 0;

	if (*globals.decoder) {
		arch = globals.decoder;
	}
#ifndef WIN32
	else if (sizeof(void *) == 4) {
		arch = "i586";
	} else {
		x64 = 1;
	}
#else
	x64 = 1;
#endif
	mh = mpg123_new(arch, &rc);
	if (!mh) {
		err = mpg123_plain_strerror(rc);
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Error allocating mpg123 handle! %s\n", switch_str_nil(err));
		return NULL;
	}

	/* NOTE: keeping the globals.decoder check here for behaviour backwards compat - stkn */
	if (*globals.decoder || globals.outscale || globals.vol) {
		if (globals.outscale) {
			mpg123_param(mh, MPG123_OUTSCALE, globals.outscale, 0);
		}
		if (globals.vol) {
			mpg123_volume(mh, globals.vol);
		}
	} else if (x64) {
		mpg123_param(mh, MPG123_OUTSCALE, 8192, 0);
	}

	return mh;
}


struct shout_context {
	shout_t *shout;
	char curl_error_buff[CURL_ERROR_SIZE];
	lame_global_flags *gfp;
	char *stream_url;
	switch_mutex_t *audio_mutex;
	switch_buffer_t *audio_buffer;
	switch_memory_pool_t *memory_pool;
	unsigned char decode_buf[MP3_DCACHE];
	switch_file_handle_t *handle;
	mpg123_handle *mh;
	int err;
	int mp3err;
	int dlen;
	FILE *fp;
	size_t samplerate;
	uint8_t shout_init;
	uint32_t prebuf;
	int lame_ready;
	int eof;
	int channels;
	int stream_channels;
	unsigned char *mp3buf;
	switch_size_t mp3buflen;
	switch_thread_rwlock_t *rwlock;
	int buffer_seconds;
	switch_thread_t *read_stream_thread;
	switch_thread_t *write_stream_thread;
	curl_socket_t curlfd;
};

typedef struct shout_context shout_context_t;

static void decode_fd(shout_context_t *context, void *data, size_t bytes);

static inline void free_context(shout_context_t *context)
{
	size_t ret;
	switch_status_t st;

	if (context) {
		switch_mutex_lock(context->audio_mutex);
		context->err++;
		switch_mutex_unlock(context->audio_mutex);

		if (context->stream_url) {
			switch_mutex_lock(context->audio_mutex);
			if (context->curlfd > -1) {
				shutdown(context->curlfd, 2);
				context->curlfd = -1;
			}
			switch_mutex_unlock(context->audio_mutex);
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Waiting for stream to terminate: %s\n", context->stream_url);
			if (context->read_stream_thread) {
				switch_thread_join(&st, context->read_stream_thread);
			}
		}

		if (context->write_stream_thread) {
			switch_thread_join(&st, context->write_stream_thread);
		}

		switch_thread_rwlock_wrlock(context->rwlock);

		if (context->mh) {
			mpg123_close(context->mh);
			mpg123_delete(context->mh);
		}

		if (context->fp && context->lame_ready) {
			unsigned char mp3buffer[20480];
			int len;
			int16_t blank[2048] = { 0 }, *r = NULL;


			if (context->channels == 2) {
				r = blank;
			}

			len = lame_encode_buffer(context->gfp, blank, r, sizeof(blank) / 2, mp3buffer, sizeof(mp3buffer));

			if (len) {
				ret = fwrite(mp3buffer, 1, len, context->fp);
			}

			while ((len = lame_encode_flush(context->gfp, mp3buffer, sizeof(mp3buffer))) > 0) {
				ret = fwrite(mp3buffer, 1, len, context->fp);
			}

			lame_mp3_tags_fid(context->gfp, context->fp);

			fclose(context->fp);
			context->fp = NULL;
		}

		if (context->shout) {
			if (context->gfp) {
				unsigned char mp3buffer[8192];
				int len;
				int16_t blank[2048] = { 0 }, *r = NULL;
				int framesize;

				if (context->channels == 2) {
					r = blank;
				}

				len = lame_encode_buffer(context->gfp, blank, r, sizeof(blank) / 2, mp3buffer, sizeof(mp3buffer));

				if (len) {
					ret = shout_send(context->shout, mp3buffer, len);
					if (ret == SHOUTERR_SUCCESS) {
						shout_sync(context->shout);
					}
				}

				framesize = lame_get_framesize(context->gfp);
				if ( framesize ) {
					while ((len = lame_encode_flush(context->gfp, mp3buffer, sizeof(mp3buffer))) > 0) {
						ret = shout_send(context->shout, mp3buffer, len);

						if (ret != SHOUTERR_SUCCESS) {
							break;
						} else {
							shout_sync(context->shout);
						}
					}
				}
			}

			shout_close(context->shout);
			context->shout = NULL;
		}

		if (context->gfp) {
			lame_close(context->gfp);
			context->gfp = NULL;
		}

		if (context->audio_buffer) {
			switch_buffer_destroy(&context->audio_buffer);
		}

		switch_mutex_destroy(context->audio_mutex);

		switch_thread_rwlock_unlock(context->rwlock);
		switch_thread_rwlock_destroy(context->rwlock);
	}
}

static void log_error(char const *fmt, va_list ap)
{
	char *data = NULL;

	if (fmt) {
#ifdef HAVE_VASPRINTF
		int ret;
		ret = vasprintf(&data, fmt, ap);
		if ((ret == -1) || !data) {
			return;
		}
#else
		data = (char *) malloc(2048);
		if (data) {
			vsnprintf(data, 2048, fmt, ap);
		} else {
			return;
		}
#endif
	}
	if (data) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, (char *) "%s", data);
		free(data);
	}
}

static void log_debug(char const *fmt, va_list ap)
{
	char *data = NULL;

	if (fmt) {
#ifdef HAVE_VASPRINTF
		int ret;
		ret = vasprintf(&data, fmt, ap);
		if ((ret == -1) || !data) {
			return;
		}
#else
		data = (char *) malloc(2048);
		if (data) {
			vsnprintf(data, 2048, fmt, ap);
		} else {
			return;
		}
#endif
	}
	if (data) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, (char *) "%s", data);
		free(data);
	}
}

static void log_msg(char const *fmt, va_list ap)
{
	char *data = NULL;

	if (fmt) {
#ifdef HAVE_VASPRINTF
		int ret;
		ret = vasprintf(&data, fmt, ap);
		if ((ret == -1) || !data) {
			return;
		}
#else
		data = (char *) malloc(2048);
		if (data) {
			vsnprintf(data, 2048, fmt, ap);
		} else {
			return;
		}
#endif
	}
	if (data) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, (char *) "%s", data);
		free(data);
	}
}

static void decode_fd(shout_context_t *context, void *data, size_t bytes)
{
	int decode_status = 0;
	size_t usedlen;

	while (!context->err && !context->eof && switch_buffer_inuse(context->audio_buffer) < bytes) {
		usedlen = 0;

		decode_status = mpg123_read(context->mh, context->decode_buf, sizeof(context->decode_buf), &usedlen);

		if (decode_status == MPG123_NEW_FORMAT) {
			continue;
		} else if (decode_status == MPG123_OK) {
			;
		} else if (decode_status == MPG123_DONE || decode_status == MPG123_NEED_MORE) {
			context->eof++;
		} else if (decode_status == MPG123_ERR || decode_status > 0) {
			if (++context->mp3err >= 5) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Decoder Error!\n");
				context->eof++;
				goto error;
			}
			continue;
		}

		context->mp3err = 0;

		switch_buffer_write(context->audio_buffer, context->decode_buf, usedlen);
	}

	return;

  error:
	switch_mutex_lock(context->audio_mutex);
	context->err++;
	switch_mutex_unlock(context->audio_mutex);
}

static size_t stream_callback(void *ptr, size_t size, size_t nmemb, void *data)
{
	register unsigned int realsize = (unsigned int) (size * nmemb);
	shout_context_t *context = data;
	int decode_status = 0;
	size_t usedlen;
	uint32_t buf_size = 1024 * 128;	/* do not make this 64 or less, stutter will ensue after first 64k buffer is dry */
	switch_size_t used;

	if (context->err) {
		goto error;
	}

	if (!context->stream_channels) {
		long rate = 0;
		int channels = 0;
		int encoding = 0;
		mpg123_getformat(context->mh, &rate, &channels, &encoding);
		context->stream_channels = channels;
	}

	if (context->prebuf) {
		buf_size = context->prebuf;
	}

	if (context->stream_channels) {
		buf_size = buf_size * context->stream_channels;
	}

	/* make sure we aren't over zealous by slowing down the stream when the buffer is too full */
	while (!context->err) {
		switch_mutex_lock(context->audio_mutex);
		used = switch_buffer_inuse(context->audio_buffer);
		switch_mutex_unlock(context->audio_mutex);

		if (used < buf_size) {
			/* switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Buffered %u/%u!\n", used, buf_size); */
			break;
		}

		switch_yield(500000);
	}

	if (context->err) {
		goto error;
	}

	if (mpg123_feed(context->mh, ptr, realsize) != MPG123_OK) {
		goto error;
	}

	do {
		usedlen = 0;

		decode_status = mpg123_read(context->mh, context->decode_buf, sizeof(context->decode_buf), &usedlen);

		if (decode_status == MPG123_NEW_FORMAT) {
			continue;
		} else if (decode_status == MPG123_OK || decode_status == MPG123_NEED_MORE) {
			;
		} else if (decode_status == MPG123_DONE) {
			context->eof++;
		} else if (decode_status == MPG123_ERR || decode_status > 0) {
			if (++context->mp3err >= 5) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Decoder Error! %s\n", context->stream_url);
				context->eof++;
				goto error;
			}
			continue;
		}

		context->mp3err = 0;

		switch_mutex_lock(context->audio_mutex);
		switch_buffer_write(context->audio_buffer, context->decode_buf, usedlen);
		switch_mutex_unlock(context->audio_mutex);
	} while (!context->err && !context->eof && decode_status != MPG123_NEED_MORE);

	if (context->err) {
		goto error;
	}

	return realsize;

  error:
	switch_mutex_lock(context->audio_mutex);
	context->err++;
	switch_mutex_unlock(context->audio_mutex);
	return 0;
}

static int progress_callback(void *clientp,   double dltotal,   double dlnow,   double ultotal,   double ulnow)
{
	shout_context_t *context = (shout_context_t *) clientp;
	return context->err;
}


static int sockopt_callback(void *clientp, curl_socket_t curlfd,
							curlsocktype purpose)
{
	shout_context_t *context = (shout_context_t *) clientp;

	switch_mutex_lock(context->audio_mutex);
	context->curlfd = curlfd;
	switch_mutex_unlock(context->audio_mutex);
	
	return 0;
}

#define MY_BUF_LEN 1024 * 32
#define MY_BLOCK_SIZE MY_BUF_LEN
static void *SWITCH_THREAD_FUNC read_stream_thread(switch_thread_t *thread, void *obj)
{
	switch_CURL *curl_handle = NULL;
	switch_CURLcode cc;
	shout_context_t *context = (shout_context_t *) obj;

	switch_thread_rwlock_rdlock(context->rwlock);
	switch_mutex_lock(context->audio_mutex);
	context->curlfd = -1;
	switch_mutex_unlock(context->audio_mutex);
	curl_handle = switch_curl_easy_init();
	switch_curl_easy_setopt(curl_handle, CURLOPT_URL, context->stream_url);
	curl_easy_setopt(curl_handle, CURLOPT_PROGRESSFUNCTION, progress_callback);
	curl_easy_setopt(curl_handle, CURLOPT_PROGRESSDATA, (void *)context);
	switch_curl_easy_setopt(curl_handle, CURLOPT_FOLLOWLOCATION, 1);
	switch_curl_easy_setopt(curl_handle, CURLOPT_MAXREDIRS, 10);
	switch_curl_easy_setopt(curl_handle, CURLOPT_WRITEFUNCTION, stream_callback);
	switch_curl_easy_setopt(curl_handle, CURLOPT_WRITEDATA, (void *) context);
	switch_curl_easy_setopt(curl_handle, CURLOPT_USERAGENT, "FreeSWITCH(mod_shout)/1.0");
	switch_curl_easy_setopt(curl_handle, CURLOPT_NOSIGNAL, 1);
	switch_curl_easy_setopt(curl_handle, CURLOPT_CONNECTTIMEOUT, 30);	/* eventually timeout connect */
	switch_curl_easy_setopt(curl_handle, CURLOPT_LOW_SPEED_LIMIT, 100);	/* handle trickle connections */
	switch_curl_easy_setopt(curl_handle, CURLOPT_LOW_SPEED_TIME, 30);
	switch_curl_easy_setopt(curl_handle, CURLOPT_ERRORBUFFER, context->curl_error_buff);
	curl_easy_setopt(curl_handle, CURLOPT_SOCKOPTFUNCTION, sockopt_callback);
	curl_easy_setopt(curl_handle, CURLOPT_SOCKOPTDATA, (void *)context);

	cc = switch_curl_easy_perform(curl_handle);

	switch_mutex_lock(context->audio_mutex);
	context->curlfd = -1;
	switch_mutex_unlock(context->audio_mutex);

	if (cc && cc != CURLE_WRITE_ERROR) {	/* write error is ok, we just exited from callback early */
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "CURL returned error:[%d] %s : %s [%s]\n", cc, switch_curl_easy_strerror(cc),
						  context->curl_error_buff, context->stream_url);
	}
	switch_curl_easy_cleanup(curl_handle);
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Read Thread Done\n");

	context->eof++;
	switch_thread_rwlock_unlock(context->rwlock);
	return NULL;
}

static void launch_read_stream_thread(shout_context_t *context)
{
	switch_threadattr_t *thd_attr = NULL;

	switch_threadattr_create(&thd_attr, context->memory_pool);
	switch_threadattr_stacksize_set(thd_attr, SWITCH_THREAD_STACKSIZE);
	switch_thread_create(&context->read_stream_thread, thd_attr, read_stream_thread, context, context->memory_pool);
}

#define error_check() if (context->err) goto error;

static void *SWITCH_THREAD_FUNC write_stream_thread(switch_thread_t *thread, void *obj)
{
	shout_context_t *context = (shout_context_t *) obj;

	switch_thread_rwlock_rdlock(context->rwlock);

	if (!context->lame_ready) {
		lame_init_params(context->gfp);
		lame_print_config(context->gfp);
		context->lame_ready = 1;
	}

	while (!context->err) {
		unsigned char mp3buf[20480] = "";
		int16_t audio[9600] = { 0 };
		switch_size_t audio_read = 0;
		int rlen = 0;
		long ret = 0;

		switch_mutex_lock(context->audio_mutex);
		if (context->audio_buffer) {
			audio_read = switch_buffer_read(context->audio_buffer, audio, sizeof(audio));
		} else {
			context->err++;
		}
		switch_mutex_unlock(context->audio_mutex);

		error_check();

		if (!audio_read) {
			audio_read = sizeof(audio);
			memset(audio, 255, sizeof(audio));
		}

		if (context->channels == 2) {
			int16_t l[4800] = { 0 };
			int16_t r[4800] = { 0 };
			int j = 0;
			switch_size_t i;

			for (i = 0; i < audio_read / 4; i++) {
				l[i] = audio[j++];
				r[i] = audio[j++];
			}

			if ((rlen = lame_encode_buffer(context->gfp, l, r, (int)(audio_read / 4), mp3buf, sizeof(mp3buf))) < 0) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "MP3 encode error %d!\n", rlen);
				goto error;
			}

		} else if (context->channels == 1) {
			if ((rlen = lame_encode_buffer(context->gfp, (void *) audio, NULL, (int)(audio_read / sizeof(int16_t)), mp3buf, sizeof(mp3buf))) < 0) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "MP3 encode error %d!\n", rlen);
				goto error;
			}
		}

		if (rlen) {
			ret = shout_send(context->shout, mp3buf, rlen);
			if (ret != SHOUTERR_SUCCESS) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Send error: %s\n", shout_get_error(context->shout));
				goto error;
			}
		} else {
			memset(mp3buf, 0, 128);
			ret = shout_send(context->shout, mp3buf, 128);
		}

		shout_sync(context->shout);
		switch_yield(100000);
	}

  error:
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Write Thread Done\n");
	switch_thread_rwlock_unlock(context->rwlock);

	return NULL;
}

static void launch_write_stream_thread(shout_context_t *context)
{
	switch_threadattr_t *thd_attr = NULL;

	if (context->err) {
		return;
	}

	switch_threadattr_create(&thd_attr, context->memory_pool);
	switch_threadattr_stacksize_set(thd_attr, SWITCH_THREAD_STACKSIZE);
	switch_thread_create(&context->write_stream_thread, thd_attr, write_stream_thread, context, context->memory_pool);
}

#define TC_BUFFER_SIZE 1024 * 32

#define CONCAT_LOCATION(_x,_y) _x ":" #_y
#define MAKE_LOCATION(_x,_y) CONCAT_LOCATION(_x,_y)
#define HERE MAKE_LOCATION(__FILE__, __LINE__)
#define MPGERROR() {err = "MPG123 Error at " HERE "."; mpg123err = mpg123_strerror(context->mh); goto error; }
static switch_status_t shout_file_open(switch_file_handle_t *handle, const char *path)
{
	shout_context_t *context;
	char *host, *file;
	char *username, *password, *port;
	char *err = NULL;
	const char *mpg123err = NULL;
	int portno = 0;
	long rate = 0;
	int channels = 0;
	int encoding = 0;
	const char *var = NULL;

	if ((context = switch_core_alloc(handle->memory_pool, sizeof(*context))) == 0) {
		return SWITCH_STATUS_MEMERR;
	}

	if (!handle->samplerate) {
		handle->samplerate = 8000;
	}

	context->memory_pool = handle->memory_pool;
	context->samplerate = handle->samplerate;
	context->handle = handle;
	context->buffer_seconds = 1;

	switch_thread_rwlock_create(&(context->rwlock), context->memory_pool);

	switch_thread_rwlock_rdlock(context->rwlock);

	switch_mutex_init(&context->audio_mutex, SWITCH_MUTEX_NESTED, context->memory_pool);

	if (handle->params && (var = switch_event_get_header(handle->params, "buffer_seconds"))) {
		int bs = atol(var);
		if (bs < 1) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Buffer Seconds %d too low\n", bs);
		} else if (bs > 60) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Buffer Seconds %d too high\n", bs);
		} else {
			context->buffer_seconds = bs;
		}
	}

	if (switch_test_flag(handle, SWITCH_FILE_FLAG_READ)) {
		if (switch_buffer_create_dynamic(&context->audio_buffer, TC_BUFFER_SIZE, TC_BUFFER_SIZE * 2, 0) != SWITCH_STATUS_SUCCESS) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Memory Error!\n");
			goto error;
		}

		context->mh = our_mpg123_new(NULL, NULL);
		if (mpg123_format_all(context->mh) != MPG123_OK) {
			MPGERROR();
		}
		if (mpg123_param(context->mh, MPG123_FORCE_RATE, (long)context->samplerate, 0) != MPG123_OK) {
			MPGERROR();
		}

		if (handle->handler) {
			if (mpg123_open_feed(context->mh) != MPG123_OK) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Error opening mpg feed\n");
				mpg123err = mpg123_strerror(context->mh);
				goto error;
			}
			context->stream_url = switch_core_sprintf(context->memory_pool, "http://%s", path);
			context->prebuf = handle->prebuf;
			launch_read_stream_thread(context);
			switch_cond_next();
		} else {
			handle->seekable = 1;

			if (mpg123_open(context->mh, path) != MPG123_OK) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Error opening %s\n", path);
				mpg123err = mpg123_strerror(context->mh);
				goto error;
			}

		}

		if (handle->handler) {
			int sanity = 1000;

			while(--sanity > 0 && !switch_buffer_inuse(context->audio_buffer)) {
				switch_yield(20000);
			}

			if (!sanity) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Error opening %s (data stream timeout)\n", path);
				goto error;
			}
		}

		mpg123_getformat(context->mh, &rate, &channels, &encoding);

		if (!channels || !rate) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Error opening %s (invalid rate or channel count)\n", path);
			goto error;
		}

		handle->channels = channels;
		handle->samplerate = rate;

	} else if (switch_test_flag(handle, SWITCH_FILE_FLAG_WRITE)) {
		if (!(context->gfp = lame_init())) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Could not allocate lame\n");
			goto error;
		}

		if (!handle->handler) {
			id3tag_init(context->gfp);
			id3tag_v2_only(context->gfp);
			id3tag_pad_v2(context->gfp);

		}
		context->channels = handle->channels;
		
		if (globals.brate) {
			lame_set_brate(context->gfp, globals.brate);
		} else {
			lame_set_brate(context->gfp, 16 * (handle->samplerate / 8000) * handle->channels);
		}
		
		lame_set_num_channels(context->gfp, handle->channels);
		lame_set_in_samplerate(context->gfp, handle->samplerate);
		
		if (globals.resample) {
			lame_set_out_samplerate(context->gfp, globals.resample);
		} else {
			lame_set_out_samplerate(context->gfp, handle->samplerate);
		}

		if (handle->channels == 2) {
			lame_set_mode(context->gfp, STEREO);
		} else {
			lame_set_mode(context->gfp, MONO);
		}

		if (globals.quality) {
			lame_set_quality(context->gfp, globals.quality);
		} else {
			lame_set_quality(context->gfp, 2);      /* 2=high  5 = medium  7=low */
		}

		lame_set_errorf(context->gfp, log_error);
		lame_set_debugf(context->gfp, log_debug);
		lame_set_msgf(context->gfp, log_msg);

		if (handle->handler) {
			if (switch_buffer_create_dynamic(&context->audio_buffer, MY_BLOCK_SIZE, MY_BUF_LEN, 0) != SWITCH_STATUS_SUCCESS) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Memory Error!\n");
				goto error;
			}
			lame_set_bWriteVbrTag(context->gfp, 0);
			lame_mp3_tags_fid(context->gfp, NULL);

			username = switch_core_strdup(handle->memory_pool, path);
			if (!(password = strchr(username, ':'))) {
				err = "invalid url";
				goto error;
			}
			*password++ = '\0';

			if (!(host = strchr(password, '@'))) {
				err = "invalid url";
				goto error;
			}
			*host++ = '\0';

			if ((file = strchr(host, '/'))) {
				*file++ = '\0';
			} else {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Invalid URL: %s\n", path);
				goto error;
			}

			if ((port = strchr(host, ':'))) {
				*port++ = '\0';
				if (port) {
					portno = atoi(port);
				}
			}

			if (!portno) {
				portno = 8000;
			}

			if (!(context->shout = shout_new())) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Could not allocate shout_t\n");
				goto error;
			}

			if (shout_set_host(context->shout, host) != SHOUTERR_SUCCESS) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Error setting hostname: %s\n", shout_get_error(context->shout));
				goto error;
			}

			if (shout_set_protocol(context->shout, SHOUT_PROTOCOL_HTTP) != SHOUTERR_SUCCESS) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Error setting protocol: %s\n", shout_get_error(context->shout));
				goto error;
			}

			if (shout_set_port(context->shout, portno) != SHOUTERR_SUCCESS) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Error setting port: %s\n", shout_get_error(context->shout));
				goto error;
			}

			if (shout_set_password(context->shout, password) != SHOUTERR_SUCCESS) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Error setting password: %s\n", shout_get_error(context->shout));
				goto error;
			}

			if (shout_set_mount(context->shout, file) != SHOUTERR_SUCCESS) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Error setting mount: %s\n", shout_get_error(context->shout));
				goto error;
			}

			if (shout_set_user(context->shout, username) != SHOUTERR_SUCCESS) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Error setting user: %s\n", shout_get_error(context->shout));
				goto error;
			}

			if (shout_set_url(context->shout, "http://www.freeswitch.org") != SHOUTERR_SUCCESS) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Error setting name: %s\n", shout_get_error(context->shout));
				goto error;
			}

			if (shout_set_description(context->shout, "FreeSWITCH mod_shout Broadcasting Module") != SHOUTERR_SUCCESS) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Error setting description: %s\n", shout_get_error(context->shout));
				goto error;
			}

			if (shout_set_audio_info(context->shout, "bitrate", "24000") != SHOUTERR_SUCCESS) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Error setting bitrate: %s\n", shout_get_error(context->shout));
				goto error;
			}

			if (shout_set_format(context->shout, SHOUT_FORMAT_MP3) != SHOUTERR_SUCCESS) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Error setting format: %s\n", shout_get_error(context->shout));
				goto error;
			}

		} else {
			const char *mask = "wb+";
			
			if (switch_test_flag(handle, SWITCH_FILE_WRITE_APPEND)) {
				mask = "ab+";
			}
			/* lame being lame and all has FILE * coded into it's API for some functions so we gotta use it */
			if (!(context->fp = fopen(path, mask))) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Error opening %s\n", path);
				goto error;
			}
		}
	}

	handle->samples = 0;
	handle->format = 0;
	handle->sections = 0;
	handle->speed = 0;
	handle->private_info = context;
	switch_thread_rwlock_unlock(context->rwlock);

	return SWITCH_STATUS_SUCCESS;

  error:
	switch_thread_rwlock_unlock(context->rwlock);
	if (err) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Error: %s\n", err);
	}
	if (mpg123err) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Error from mpg123: %s\n", mpg123err);
	}
	free_context(context);
	return SWITCH_STATUS_GENERR;

}

static switch_status_t shout_file_close(switch_file_handle_t *handle)
{
	shout_context_t *context = handle->private_info;

	free_context(context);

	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t shout_file_seek(switch_file_handle_t *handle, unsigned int *cur_sample, int64_t samples, int whence)
{
	shout_context_t *context = handle->private_info;
	off_t seek_samples;

	if (handle->handler || switch_test_flag(handle, SWITCH_FILE_FLAG_WRITE)) {
		return SWITCH_STATUS_FALSE;
	} else {
		if (whence == SWITCH_SEEK_CUR) {
			samples -= switch_buffer_inuse(context->audio_buffer) / sizeof(int16_t);
		}

		switch_buffer_zero(context->audio_buffer);
		seek_samples = mpg123_seek(context->mh, (off_t) samples, whence);

		if (seek_samples >= 0) {
			handle->pos = *cur_sample = seek_samples;
			return SWITCH_STATUS_SUCCESS;
		}

		return SWITCH_STATUS_FALSE;
	}
}

static switch_status_t shout_file_read(switch_file_handle_t *handle, void *data, size_t *len)
{
	shout_context_t *context = handle->private_info;
	size_t rb = 0, bytes = *len * sizeof(int16_t) * handle->real_channels, newbytes = 0;

	*len = 0;

	if (!context) {
		return SWITCH_STATUS_FALSE;
	}

	if (!handle->handler)
		decode_fd(context, data, bytes);

	switch_mutex_lock(context->audio_mutex);
	rb = switch_buffer_read(context->audio_buffer, data, bytes);
	switch_mutex_unlock(context->audio_mutex);

	if (!rb && (context->eof || context->err)) {
		return SWITCH_STATUS_FALSE;
	}

	/* switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "rb: %d, bytes: %d\n", (int) rb, (int) bytes); */

	if (rb) {
		*len = rb / sizeof(int16_t) / handle->real_channels;
	} else {
		/* no data, so insert N seconds of silence */
		newbytes = (2 * handle->samplerate * handle->real_channels) * context->buffer_seconds;
		if (newbytes < bytes) {
			bytes = newbytes;
		}
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Padding mp3 stream with %ds of empty audio. (%s)\n", 
						  context->buffer_seconds, context->stream_url);

		memset(data, 255, bytes);
		*len = bytes / sizeof(int16_t) / handle->real_channels;
	}

	handle->sample_count += *len;

	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t shout_file_write(switch_file_handle_t *handle, void *data, size_t *len)
{
	shout_context_t *context;
	int rlen = 0;
	int16_t *audio = data;
	size_t nsamples = *len;

	if (!handle) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Error no handle\n");
		return SWITCH_STATUS_FALSE;
	}

	if (!(context = handle->private_info)) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Error no context\n");
		return SWITCH_STATUS_FALSE;
	}

	if (context->err) {
		return SWITCH_STATUS_FALSE;
	}

	if (context->shout && !context->shout_init) {

		if (!context->gfp) {
			return SWITCH_STATUS_FALSE;
		}

		context->shout_init++;
		if (shout_open(context->shout) != SHOUTERR_SUCCESS) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Error opening stream: %s\n", shout_get_error(context->shout));
			context->err++;
			return SWITCH_STATUS_FALSE;
		}
		launch_write_stream_thread(context);
	}

	if (handle->handler && context->audio_mutex) {
		switch_mutex_lock(context->audio_mutex);
		if (context->audio_buffer) {
			if (!switch_buffer_write(context->audio_buffer, data, (nsamples * sizeof(int16_t) * handle->real_channels))) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Buffer error\n");
				context->err++;
			}
		} else {
			context->err++;
		}

		switch_mutex_unlock(context->audio_mutex);
		if (context->err) {
			return SWITCH_STATUS_FALSE;
		}

		handle->sample_count += *len;
		return SWITCH_STATUS_SUCCESS;
	}

	if (!context->lame_ready) {
		lame_init_params(context->gfp);
		lame_print_config(context->gfp);
		context->lame_ready = 1;
	}

	if (context->mp3buflen < nsamples * 4) {
		context->mp3buflen = nsamples * 4;
		context->mp3buf = switch_core_alloc(context->memory_pool, context->mp3buflen);
	}

	if (handle->real_channels == 2) {
		if ((rlen = lame_encode_buffer_interleaved(context->gfp, audio, (int)nsamples, context->mp3buf, (int)context->mp3buflen)) < 0) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "MP3 encode error %d!\n", rlen);
			return SWITCH_STATUS_FALSE;
		}
	} else if (handle->real_channels == 1) {
		if ((rlen = lame_encode_buffer(context->gfp, audio, NULL, (int)nsamples, context->mp3buf, (int)context->mp3buflen)) < 0) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "MP3 encode error %d!\n", rlen);
			return SWITCH_STATUS_FALSE;
		}
	} else {
		rlen = 0;
	}

	if (rlen) {
		int ret = (int)fwrite(context->mp3buf, 1, rlen, context->fp);
		if (ret < 0) {
			return SWITCH_STATUS_FALSE;
		}
	}

	handle->sample_count += *len;

	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t shout_file_set_string(switch_file_handle_t *handle, switch_audio_col_t col, const char *string)
{
	shout_context_t *context = handle->private_info;
	switch_status_t status = SWITCH_STATUS_FALSE;

	if (!context->shout) {
		switch (col) {
		case SWITCH_AUDIO_COL_STR_TITLE:
			id3tag_set_title(context->gfp, string);
			break;
		case SWITCH_AUDIO_COL_STR_COMMENT:
			id3tag_set_comment(context->gfp, string);
			break;
		case SWITCH_AUDIO_COL_STR_ARTIST:
			id3tag_set_artist(context->gfp, string);
			break;
		case SWITCH_AUDIO_COL_STR_DATE:
			id3tag_set_year(context->gfp, string);
			break;
		case SWITCH_AUDIO_COL_STR_SOFTWARE:
			id3tag_set_album(context->gfp, string);
			break;
		case SWITCH_AUDIO_COL_STR_COPYRIGHT:
			id3tag_set_genre(context->gfp, string);
			break;
		default:
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "Value Ignored %d, %s\n", col, string);
			break;
		}

		return status;
	}

	switch (col) {
	case SWITCH_AUDIO_COL_STR_TITLE:
		if (shout_set_name(context->shout, string) == SHOUTERR_SUCCESS) {
			status = SWITCH_STATUS_SUCCESS;
		} else {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Error setting name: %s\n", shout_get_error(context->shout));
		}
		break;
	case SWITCH_AUDIO_COL_STR_COMMENT:
		if (shout_set_url(context->shout, string) == SHOUTERR_SUCCESS) {
			status = SWITCH_STATUS_SUCCESS;
		} else {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Error setting name: %s\n", shout_get_error(context->shout));
		}
		break;
	case SWITCH_AUDIO_COL_STR_ARTIST:
		if (shout_set_description(context->shout, string) == SHOUTERR_SUCCESS) {
			status = SWITCH_STATUS_SUCCESS;
		} else {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Error setting name: %s\n", shout_get_error(context->shout));
		}
		break;
	default:
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "Value Ignored %d, %s\n", col, string);
		break;
	}

	return status;
}

static switch_status_t shout_file_get_string(switch_file_handle_t *handle, switch_audio_col_t col, const char **string)
{
	return SWITCH_STATUS_FALSE;
}

static switch_bool_t telecast_callback(switch_media_bug_t *bug, void *user_data, switch_abc_type_t type)
{
	switch_buffer_t *buffer = (switch_buffer_t *) user_data;
	uint8_t data[SWITCH_RECOMMENDED_BUFFER_SIZE];
	switch_frame_t frame = { 0 };

	frame.data = data;
	frame.buflen = SWITCH_RECOMMENDED_BUFFER_SIZE;

	switch (type) {
	case SWITCH_ABC_TYPE_INIT:
		break;
	case SWITCH_ABC_TYPE_CLOSE:
		break;
	case SWITCH_ABC_TYPE_READ_PING:
		if (buffer) {
			if (switch_core_media_bug_read(bug, &frame, SWITCH_TRUE) == SWITCH_STATUS_SUCCESS) {
				switch_buffer_lock(buffer);
				switch_buffer_write(buffer, frame.data, frame.datalen);
				switch_buffer_unlock(buffer);
			}
		} else {
			return SWITCH_FALSE;
		}
		break;

	case SWITCH_ABC_TYPE_READ:
	case SWITCH_ABC_TYPE_WRITE:

	default:
		break;
	}

	return SWITCH_TRUE;
}

struct holder {
	switch_stream_handle_t *stream;
	switch_memory_pool_t *pool;
	char *host;
	char *port;
	char *uri;
};

static int web_callback(void *pArg, int argc, char **argv, char **columnNames)
{
	struct holder *holder = (struct holder *) pArg;
	char title_b4[128] = "";
	char title_aft[128 * 3 + 1] = "";
	char *mp3, *m3u;
	int uri_offset = 1;

	const char *uuid = argv[0];
	const char *created = argv[1];
	const char *cid_name = argv[2];
	const char *cid_num = argv[3];
	const char *dest = argv[4];
	const char *application = argv[5] ? argv[5] : "N/A";
	const char *application_data = argv[6] ? argv[6] : "N/A";
	const char *read_codec = argv[7];
	const char *read_rate = argv[8];

	holder->stream->write_function(holder->stream,
								   "<tr><td>%s</td><td>%s</td><td>%s</td><td>%s</td><td>%s</td><td>%s</td><td>%s</td><td>%s</td><td>",
								   created, cid_name, cid_num, dest, application, application_data, read_codec, read_rate);

	snprintf(title_b4, sizeof(title_b4), "%s <%s>", cid_name, cid_num);
	switch_url_encode(title_b4, title_aft, sizeof(title_aft));

	if (!strncmp(holder->uri, "/webapi", 7)) uri_offset = 4;

	mp3 = switch_mprintf("http://%s:%s/%s/mp3/%s/%s.mp3", holder->host, holder->port, holder->uri + uri_offset, uuid, cid_num);
	m3u = switch_mprintf("http://%s:%s/%s/m3u/mp3/%s/%s.mp3.m3u", holder->host, holder->port, holder->uri + uri_offset, uuid, cid_num);

	holder->stream->write_function(holder->stream, "[<a href=%s>mp3</a>] ", mp3);
	holder->stream->write_function(holder->stream, "[<a href=%s>m3u</a>]</td></tr>\n", m3u);

	switch_safe_free(mp3);
	switch_safe_free(m3u);
	return 0;
}

void do_telecast(switch_stream_handle_t *stream)
{
	char *path_info = switch_event_get_header(stream->param_event, "http-path-info");
	char *uuid = strdup(path_info + 4);
	switch_core_session_t *tsession;
	char *fname = "stream.mp3";

	if ((fname = strchr(uuid, '/'))) {
		*fname++ = '\0';
	}

	if (!(tsession = switch_core_session_locate(uuid))) {
		char *ref = switch_event_get_header(stream->param_event, "http-referer");
		stream->write_function(stream, "Content-type: text/html\r\n\r\n<h2>Not Found!</h2>\n" "<META http-equiv=\"refresh\" content=\"1;URL=%s\">", ref);
	} else {
		switch_media_bug_t *bug = NULL;
		switch_buffer_t *buffer = NULL;
		switch_mutex_t *mutex;
		switch_channel_t *channel = switch_core_session_get_channel(tsession);
		lame_global_flags *gfp = NULL;
		switch_codec_implementation_t read_impl = { 0 };
		switch_core_session_get_read_impl(tsession, &read_impl);

		if (switch_channel_test_flag(channel, CF_PROXY_MODE)) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Stepping into media path so this will work!\n");
			switch_ivr_media(uuid, SMF_REBRIDGE);
		}

		if (!(gfp = lame_init())) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Could not allocate lame\n");
			goto end;
		}
		lame_set_num_channels(gfp, read_impl.number_of_channels);
		lame_set_in_samplerate(gfp, read_impl.actual_samples_per_second);
		lame_set_brate(gfp, 16 * (read_impl.actual_samples_per_second / 8000) * read_impl.number_of_channels);
		lame_set_mode(gfp, 3);
		lame_set_quality(gfp, 2);
		lame_set_errorf(gfp, log_error);
		lame_set_debugf(gfp, log_debug);
		lame_set_msgf(gfp, log_msg);
		lame_set_bWriteVbrTag(gfp, 0);
		lame_mp3_tags_fid(gfp, NULL);
		lame_init_params(gfp);
		lame_print_config(gfp);

		switch_mutex_init(&mutex, SWITCH_MUTEX_NESTED, switch_core_session_get_pool(tsession));
		switch_buffer_create_dynamic(&buffer, 1024, 2048, 0);
		switch_buffer_add_mutex(buffer, mutex);

		if (switch_core_media_bug_add(tsession, "telecast", NULL,
									  telecast_callback, buffer, 0,
									  SMBF_READ_STREAM | SMBF_WRITE_STREAM | SMBF_READ_PING, &bug) != SWITCH_STATUS_SUCCESS) {
			goto end;
		}

		stream->write_function(stream, "Content-type: audio/mpeg\r\n" "Content-Disposition: inline; filename=\"%s\"\r\n\r\n", fname);

		while (switch_channel_ready(channel)) {
			unsigned char mp3buf[TC_BUFFER_SIZE] = "";
			int rlen;
			uint8_t buf[1024];
			switch_size_t bytes = 0;

			if (switch_buffer_inuse(buffer) >= 1024) {
				switch_buffer_lock(buffer);
				bytes = switch_buffer_read(buffer, buf, sizeof(buf));
				switch_buffer_unlock(buffer);
			} else {
				if (!bytes) {
					switch_cond_next();
					continue;
				}
				memset(buf, 0, bytes);
			}

			if ((rlen = lame_encode_buffer(gfp, (void *) buf, NULL, (int)(bytes / 2), mp3buf, sizeof(mp3buf))) < 0) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "MP3 encode error %d!\n", rlen);
				goto end;
			}

			if (rlen) {
				if (stream->raw_write_function(stream, mp3buf, rlen)) {
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Disconnected\n");
					goto end;
				}
			}
		}

	  end:

		switch_safe_free(uuid);

		if (gfp) {
			lame_close(gfp);
			gfp = NULL;
		}

		if (bug) {
			switch_core_media_bug_remove(tsession, &bug);
		}

		if (buffer) {
			switch_buffer_destroy(&buffer);
		}

		switch_core_session_rwunlock(tsession);
	}
}

void do_broadcast(switch_stream_handle_t *stream)
{
	char *path_info = switch_event_get_header(stream->param_event, "http-path-info");
	char *file;
	lame_global_flags *gfp = NULL;
	switch_file_handle_t fh = { 0 };
	unsigned char mp3buf[TC_BUFFER_SIZE] = "";
	uint8_t buf[1024];
	int rlen;
	int is_local = 0;
	uint32_t interval = 20000;

	if (strstr(path_info + 7, "://")) {
		file = strdup(path_info + 7);
		is_local++;
	} else {
		file = switch_mprintf("%s/streamfiles/%s", SWITCH_GLOBAL_dirs.base_dir, path_info + 7);
	}
	assert(file);

	if (switch_core_file_open(&fh, file, 0, 0, SWITCH_FILE_FLAG_READ | SWITCH_FILE_DATA_SHORT, NULL) != SWITCH_STATUS_SUCCESS) {
		memset(&fh, 0, sizeof(fh));
		stream->write_function(stream, "Content-type: text/html\r\n\r\n<h2>File not found</h2>\n");
		goto end;
	}

	if (switch_test_flag((&fh), SWITCH_FILE_NATIVE)) {
		stream->write_function(stream, "Content-type: text/html\r\n\r\n<h2>File format not supported</h2>\n");
		goto end;
	}

	if (!(gfp = lame_init())) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Could not allocate lame\n");
		goto end;
	}

	lame_set_num_channels(gfp, fh.channels);
	lame_set_in_samplerate(gfp, fh.samplerate);
	lame_set_brate(gfp, 16 * (fh.samplerate / 8000) * fh.channels);
	lame_set_mode(gfp, 3);
	lame_set_quality(gfp, 2);
	lame_set_errorf(gfp, log_error);
	lame_set_debugf(gfp, log_debug);
	lame_set_msgf(gfp, log_msg);
	lame_set_bWriteVbrTag(gfp, 0);
	lame_mp3_tags_fid(gfp, NULL);
	lame_init_params(gfp);
	lame_print_config(gfp);

	stream->write_function(stream, "Content-type: audio/mpeg\r\n" "Content-Disposition: inline; filename=\"%s.mp3\"\r\n\r\n", path_info + 7);

	if (fh.interval) {
		interval = fh.interval * 1000;
	}

	for (;;) {
		switch_size_t samples = sizeof(buf) / 2;

		switch_core_file_read(&fh, buf, &samples);

		if (is_local) {
			switch_yield(interval);
		}

		if (!samples) {
			break;
		}

		if ((rlen = lame_encode_buffer(gfp, (void *) buf, NULL, (int)samples, mp3buf, sizeof(mp3buf))) < 0) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "MP3 encode error %d!\n", rlen);
			goto end;
		}

		if (rlen) {
			if (stream->raw_write_function(stream, mp3buf, rlen)) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Disconnected\n");
				goto end;
			}
		}
	}

	while ((rlen = lame_encode_flush(gfp, mp3buf, sizeof(mp3buf))) > 0) {
		if (stream->raw_write_function(stream, mp3buf, rlen)) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Disconnected\n");
			goto end;
		}
	}

  end:

	if (fh.channels) {
		switch_core_file_close(&fh);
	}

	switch_safe_free(file);

	if (gfp) {
		lame_close(gfp);
		gfp = NULL;
	}
}

void do_index(switch_stream_handle_t *stream)
{
	switch_cache_db_handle_t *db;
	const char *sql = "select uuid, created, cid_name, cid_num, dest, application, application_data, read_codec, read_rate from channels";
	struct holder holder;
	char *errmsg;

	if (switch_core_db_handle(&db) != SWITCH_STATUS_SUCCESS) {
		return;
	}

	holder.host = switch_event_get_header(stream->param_event, "http-host");
	holder.port = switch_event_get_header(stream->param_event, "http-port");
	holder.uri = switch_event_get_header(stream->param_event, "http-uri");
	holder.stream = stream;

	stream->write_function(stream, "Content-type: text/html\r\n\r\n");
	stream->write_function(stream,
						   "<table align=center border=1 cellpadding=6 cellspacing=0>"
						   "<tr><td>%s</td><td>%s</td><td>%s</td><td>%s</td><td>%s</td><td>%s</td><td>%s</td><td>%s</td><td>%s</td></tr>\n",
						   "Created", "CID Name", "CID Num", "Ext", "App", "Data", "Codec", "Rate", "Listen");

	switch_cache_db_execute_sql_callback(db, sql, web_callback, &holder, &errmsg);

	stream->write_function(stream, "</table>");

	if (errmsg) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Error [%s]\n", errmsg);
		switch_safe_free(errmsg);
	}
}

#define TELECAST_SYNTAX ""
SWITCH_STANDARD_API(telecast_api_function)
{
	char *host = NULL, *port = NULL, *uri = NULL, *path_info = NULL;

	if (session) {
		return SWITCH_STATUS_FALSE;
	}

	if (stream->param_event) {
		host = switch_event_get_header(stream->param_event, "http-host");
		port = switch_event_get_header(stream->param_event, "http-port");
		uri = switch_event_get_header(stream->param_event, "http-uri");
		path_info = switch_event_get_header(stream->param_event, "http-path-info");
	}

	if (!path_info) {
		return SWITCH_STATUS_FALSE;
	} else {
		if (!strncmp(path_info, "index", 5)) {
			do_index(stream);
			return SWITCH_STATUS_SUCCESS;
		}

		if (!strncmp(path_info, "m3u/", 4)) {
			char *p;

			if ((p = strstr(path_info, ".m3u"))) {
				*p = '\0';
			}

			stream->write_function(stream, "Content-type: audio/x-mpegurl\r\n\r\nhttp://%s:%s%s/%s\n", host, port, uri, path_info + 4);
			return SWITCH_STATUS_SUCCESS;
		}

		if (!strncmp(path_info, "mp3/", 4)) {
			do_telecast(stream);
			return SWITCH_STATUS_SUCCESS;
		}

		if (!strncmp(path_info, "stream/", 7)) {
			do_broadcast(stream);
			return SWITCH_STATUS_SUCCESS;
		}
	}

	stream->write_function(stream, "Content-type: text/html\r\n\r\n<h2>Invalid URL</h2>\n");

	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t load_config(void)
{
	char *cf = "shout.conf";
	switch_xml_t cfg, xml, settings, param;

	memset(&globals, 0, sizeof(globals));

	if (!(xml = switch_xml_open_cfg(cf, &cfg, NULL))) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Open of %s failed\n", cf);
		return SWITCH_STATUS_TERM;
	}

	if ((settings = switch_xml_child(cfg, "settings"))) {
		for (param = switch_xml_child(settings, "param"); param; param = param->next) {
			char *var = (char *) switch_xml_attr_soft(param, "name");
			char *val = (char *) switch_xml_attr_soft(param, "value");

			if (!strcmp(var, "decoder")) {
				switch_set_string(globals.decoder, val);
			} else if (!strcmp(var, "volume")) {
				globals.vol = (float) atof(val);
			} else if (!strcmp(var, "outscale")) {
				int tmp = atoi(val);
				if (tmp > 0) {
					globals.outscale = tmp;
				}
			} else if (!strcmp(var, "encode-brate")) {
				int tmp = atoi(val);
				if (tmp > 0) {
					globals.brate = tmp;
				}
			} else if (!strcmp(var, "encode-resample")) {
				int tmp = atoi(val);
				if (tmp > 0) {
					globals.resample = tmp;
				}
			} else if (!strcmp(var, "encode-quality")) {
				int tmp = atoi(val);
				if (tmp > 0) {
					globals.quality = tmp;
				}
			}
		}
	}


	switch_xml_free(xml);

	return SWITCH_STATUS_SUCCESS;
}

/* codec interface */

struct mp3_context {
	lame_global_flags *gfp;
};

static switch_status_t switch_mp3_init(switch_codec_t *codec, switch_codec_flag_t flags, const switch_codec_settings_t *codec_settings)
{
	struct mp3_context *context = NULL;
	int encoding, decoding;

	encoding = (flags & SWITCH_CODEC_FLAG_ENCODE);
	decoding = (flags & SWITCH_CODEC_FLAG_DECODE);

	if (!(encoding || decoding) || (!(context = switch_core_alloc(codec->memory_pool, sizeof(struct mp3_context))))) {
		return SWITCH_STATUS_FALSE;
	} else {
		const switch_codec_implementation_t *impl = codec->implementation;

		if (codec->fmtp_in) {
			codec->fmtp_out = switch_core_strdup(codec->memory_pool, codec->fmtp_in);
		}

		memset(context, 0, sizeof(struct mp3_context));

		context->gfp = lame_init();

		id3tag_init(context->gfp);
		id3tag_v2_only(context->gfp);
		id3tag_pad_v2(context->gfp);

		lame_set_num_channels(context->gfp, 1);
		lame_set_in_samplerate(context->gfp, impl->actual_samples_per_second);
		lame_set_out_samplerate(context->gfp, impl->actual_samples_per_second);

		if (impl->number_of_channels == 2) {
			lame_set_mode(context->gfp, STEREO);
		} else if (impl->number_of_channels == 1) {
			lame_set_mode(context->gfp, MONO);
		} else {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "%d channels not supported\n", impl->number_of_channels);
		}

		lame_set_brate(context->gfp, 16 * (impl->actual_samples_per_second / 8000) * impl->number_of_channels);
		lame_set_quality(context->gfp, 2);
		lame_set_errorf(context->gfp, log_error);
		lame_set_debugf(context->gfp, log_debug);
		lame_set_msgf(context->gfp, log_msg);

		lame_init_params(context->gfp);
		lame_print_config(context->gfp);

		if (encoding) {
			lame_set_bWriteVbrTag(context->gfp, 0);
			lame_mp3_tags_fid(context->gfp, NULL);
			lame_set_disable_reservoir(context->gfp, 1);
		}

		if (decoding) {
		}

		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "MP3 framesize: %d\n", lame_get_framesize(context->gfp));

		codec->private_info = context;

		return SWITCH_STATUS_SUCCESS;
	}
}

static switch_status_t switch_mp3_destroy(switch_codec_t *codec)
{
	struct mp3_context *context = codec->private_info;

	if (context && context->gfp) lame_close(context->gfp);

	codec->private_info = NULL;

	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t switch_mp3_encode(switch_codec_t *codec,
										switch_codec_t *other_codec,
										void *decoded_data,
										uint32_t decoded_data_len,
										uint32_t decoded_rate,
										void *encoded_data, uint32_t *encoded_data_len,
										uint32_t *encoded_rate,
										unsigned int *flag)
{
	struct mp3_context *context = codec->private_info;
	int len;

	if (!context) {
		return SWITCH_STATUS_FALSE;
	}

	if (codec->implementation->number_of_channels == 2) {
		len = lame_encode_buffer_interleaved(context->gfp, decoded_data, decoded_data_len / 4, encoded_data, *encoded_data_len);
	} else {
		len = lame_encode_buffer(context->gfp, decoded_data, NULL, decoded_data_len / 2, encoded_data, *encoded_data_len);
	}

	if (len < 0) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "encode error %d\n", len);
		return SWITCH_STATUS_FALSE;
	}

	*encoded_data_len = len;

	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t switch_mp3_decode(switch_codec_t *codec,
										  switch_codec_t *other_codec,
										  void *encoded_data,
										  uint32_t encoded_data_len,
										  uint32_t encoded_rate, void *decoded_data, uint32_t *decoded_data_len, uint32_t *decoded_rate,
										  unsigned int *flag)
{
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "decode not implemented!\n");
	return SWITCH_STATUS_FALSE;
}

SWITCH_MODULE_LOAD_FUNCTION(mod_shout_load)
{
	switch_api_interface_t *shout_api_interface;
	switch_file_interface_t *file_interface;
	switch_codec_interface_t *codec_interface;
	int mpf = 10000, spf = 80, bpf = 160, count = 1;
	int RATES[] = {8000, 11025, 16000, 22050, 32000, 44100, 48000};
	int i;

	supported_formats[0] = "shout";
	supported_formats[1] = "mp3";
	supported_formats[2] = "mpga";

	/* connect my internal structure to the blank pointer passed to me */
	*module_interface = switch_loadable_module_create_module_interface(pool, modname);
	file_interface = switch_loadable_module_create_interface(*module_interface, SWITCH_FILE_INTERFACE);
	file_interface->interface_name = modname;
	file_interface->extens = supported_formats;
	file_interface->file_open = shout_file_open;
	file_interface->file_close = shout_file_close;
	file_interface->file_read = shout_file_read;
	file_interface->file_write = shout_file_write;
	file_interface->file_seek = shout_file_seek;
	file_interface->file_set_string = shout_file_set_string;
	file_interface->file_get_string = shout_file_get_string;

	shout_init();
	mpg123_init();
	load_config();

	SWITCH_ADD_API(shout_api_interface, "telecast", "telecast", telecast_api_function, TELECAST_SYNTAX);

	SWITCH_ADD_CODEC(codec_interface, "MP3");

	for (count = 1; count <=4; count++) {
		for (i = 0; i < sizeof(RATES) / sizeof(RATES[0]); i++) {
			switch_core_codec_add_implementation(pool, codec_interface, SWITCH_CODEC_TYPE_AUDIO,
												 98, 	 /* the IANA code number */
												 "MP3",  /* the IANA code name */
												 NULL,   /* default fmtp to send (can be overridden by the init function) */
												 RATES[i], /* samples transferred per second */
												 RATES[i], /* actual samples transferred per second */
												 16 * RATES[i] / 8000, /* bits transferred per second */
												 mpf * count,  /* number of microseconds per frame */
												 spf * count * RATES[i] / 8000, /* number of samples per frame */
												 bpf * count * RATES[i] / 8000, /* number of bytes per frame decompressed */
												 0,	/* number of bytes per frame compressed */
												 1, /* number of channels represented */
												 1,	/* number of frames per network packet */
												switch_mp3_init, switch_mp3_encode, switch_mp3_decode, switch_mp3_destroy);

			switch_core_codec_add_implementation(pool, codec_interface, SWITCH_CODEC_TYPE_AUDIO,
												 98, 	 /* the IANA code number */
												 "MP3",  /* the IANA code name */
												 NULL,   /* default fmtp to send (can be overridden by the init function) */
												 RATES[i], /* samples transferred per second */
												 RATES[i], /* actual samples transferred per second */
												 16 * RATES[i] / 8000 * 2, /* bits transferred per second */
												 mpf * count,  /* number of microseconds per frame */
												 spf * count * RATES[i] / 8000, /* number of samples per frame */
												 bpf * count * RATES[i] / 8000 * 2, /* number of bytes per frame decompressed */
												 0,	/* number of bytes per frame compressed */
												 2, /* number of channels represented */
												 1,	/* number of frames per network packet */
												switch_mp3_init, switch_mp3_encode, switch_mp3_decode, switch_mp3_destroy);
		}
	}

	/* indicate that the module should continue to be loaded */
	return SWITCH_STATUS_SUCCESS;
}

SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_shout_shutdown)
{
	mpg123_exit();
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
