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
 * mod_shout.c -- Icecast Module
 *
 * example filename: shout://user:pass@host.com/mount.mp3
 *
 */
#include "libmpg123/mpg123.h"
#include <switch.h>
#include <shout/shout.h>
#include <lame.h>
#include <curl/curl.h>

#define OUTSCALE 8192 * 2
#define MP3_SCACHE 16384 * 2
#define MP3_DCACHE 8192 *2
#define MP3_TOOSMALL -1234

SWITCH_MODULE_LOAD_FUNCTION(mod_shout_load);
SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_shout_shutdown);
SWITCH_MODULE_DEFINITION(mod_shout, mod_shout_load, mod_shout_shutdown, NULL);

static char *supported_formats[SWITCH_MAX_CODECS] = { 0 };

struct shout_context {
	shout_t *shout;
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
	switch_file_t *fd;
	FILE *fp;
	int samplerate;
	uint8_t thread_running;
	uint8_t shout_init;
	uint32_t prebuf;
	int lame_ready;
	int eof;
	int channels;
};

typedef struct shout_context shout_context_t;

static size_t decode_fd(shout_context_t *context, void *data, size_t bytes);

static inline void free_context(shout_context_t *context)
{
	int ret;

	if (context) {
		context->err++;

		if (context->fd) {
			switch_file_close(context->fd);
			context->fd = NULL;
		}

		if (context->fp) {
			unsigned char mp3buffer[8192];
			int len;
			int16_t blank[2048] = {0}, *r = NULL;
			
			
			if (context->channels == 2) {
				r = blank;
			}
			
			len = lame_encode_buffer(context->gfp, blank, r, sizeof(blank) / 2, mp3buffer, sizeof(mp3buffer));

			if (len) {
				ret = fwrite(mp3buffer, 1, len, context->fp);
			}

			while ((len = lame_encode_flush(context->gfp, mp3buffer, sizeof(mp3buffer))) > 0) {
				ret = fwrite(mp3buffer, 1, len, context->fp);
				if (ret < 0) {
					break;
				}
			}
			
			lame_mp3_tags_fid(context->gfp, context->fp);

			fclose(context->fp);
			context->fp = NULL;
		}

		if (context->audio_buffer) {
			switch_mutex_lock(context->audio_mutex);
			switch_buffer_destroy(&context->audio_buffer);
			switch_mutex_unlock(context->audio_mutex);
		}

		if (context->shout) {
			shout_close(context->shout);
			context->shout = NULL;
		}

		if (context->gfp) {
			lame_close(context->gfp);
			context->gfp = NULL;
		}

		if (context->stream_url) {
			int sanity = 0;

			while (context->thread_running) {
				switch_yield(500000);
				if (++sanity > 10) {
					break;
				}
			}

			
		}

		if (context->mh) {
			mpg123_close(context->mh);
			mpg123_delete(context->mh);
		}
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

static size_t decode_fd(shout_context_t *context, void *data, size_t bytes)
{
	int decode_status = 0;
	size_t dlen = 0;
	int x = 0;
	unsigned char *in;
	int inlen = 0;
	unsigned char *out;
	int outlen;
	int usedlen;
	unsigned char inbuf[MP3_SCACHE];
	int done = 0;
	size_t used;
	size_t lp;
	size_t rb = 0;
	
	while (context->eof < 2 && switch_buffer_inuse(context->audio_buffer) < bytes) {
		lp = sizeof(inbuf);
		if (!context->eof && ((switch_file_read(context->fd, inbuf, &lp) != SWITCH_STATUS_SUCCESS) || lp == 0)) {
			context->eof++;
		}

		inlen = (int) lp;
		in = inbuf;

		out = context->decode_buf;
		outlen = (int) sizeof(context->decode_buf);
		usedlen = 0;
		x = 0;

		if (lp < bytes) {
			done = 1;
		}
		
		do {
			if (context->eof) {
				decode_status = mpg123_read(context->mh, out, outlen, &dlen);
			} else {
				decode_status = mpg123_decode(context->mh, in, inlen, out, outlen, &dlen);
			}

			if (context->err) {
				goto error;
			}

			if (!x) {
				in = NULL;
				inlen = 0;
				x++;
			}

			if (decode_status == MPG123_NEW_FORMAT) {
				continue;
			} else if (decode_status == MPG123_OK) {
				usedlen = dlen;
				break;
			} else if (decode_status == MPG123_DONE || (context->eof && decode_status == MPG123_NEED_MORE)) {
				context->eof++;
				goto end;
			} else if (decode_status == MPG123_ERR || decode_status > 0) {
				if (++context->mp3err >= 5) {
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Decoder Error!\n");
					context->eof++;
					goto end;
				}
				continue;
			}

			context->mp3err = 0;
			
			usedlen += dlen;
			out += dlen;
			outlen -= dlen;
			dlen = 0;


		} while (decode_status != MPG123_NEED_MORE);

		if (context->audio_buffer) {
			switch_buffer_write(context->audio_buffer, context->decode_buf, usedlen);
		} else {
			goto error;
		}

		if (done) {
			break;
		}
	}

 end:
	
	used = switch_buffer_inuse(context->audio_buffer);

	if (context->eof || done || used >= bytes) {
		if (!(rb = switch_buffer_read(context->audio_buffer, data, bytes))) {
			goto error;
		}
		return rb;
	}

	return 0;

 error:
	switch_mutex_lock(context->audio_mutex);
	context->err++;
	switch_mutex_unlock(context->audio_mutex);
	return 0;
}

#define error_check() if (context->err) goto error;

static size_t stream_callback(void *ptr, size_t size, size_t nmemb, void *data)
{
	register unsigned int realsize = (unsigned int) (size * nmemb);
	shout_context_t *context = data;
	int decode_status = 0;
	size_t dlen = 0;
	int x = 0;
	unsigned char *in;
	int inlen;
	unsigned char *out;
	int outlen;
	int usedlen;
	uint32_t used, buf_size = 1024 * 64;
	
	in = ptr;
	inlen = realsize;
	out = context->decode_buf;
	outlen = sizeof(context->decode_buf);
	usedlen = 0;

	error_check();

	if (context->prebuf) {
		buf_size = context->prebuf;
	}

	/* make sure we aren't over zealous by slowing down the stream when the buffer is too full */
	for (;;) {
		error_check();

		switch_mutex_lock(context->audio_mutex);
		if (!context->audio_buffer) {
			context->err++;
			break;
		}

		used = switch_buffer_inuse(context->audio_buffer);
		switch_mutex_unlock(context->audio_mutex);

		if (used < buf_size) {
			//switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Buffered %u/%u!\n", used, buf_size);
			break;
		}

		switch_yield(500000);
	}

	error_check();

	do {
		decode_status = mpg123_decode(context->mh, in, inlen, out, outlen, &dlen);

		error_check();

		if (!x) {
			in = NULL;
			inlen = 0;
			x++;
		}

		if (decode_status == MPG123_NEW_FORMAT) {
			continue;
		} else if (decode_status == MPG123_OK) {
			usedlen = dlen;
			break;
		} else if (decode_status == MPG123_ERR) {

			if (++context->mp3err >= 20) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Decoder Error!\n");
				goto error;
			}

			mpg123_close(context->mh);
			context->mh = mpg123_new(NULL, NULL);
			mpg123_open_feed(context->mh);
			mpg123_param(context->mh, MPG123_FORCE_RATE, context->samplerate, 0);
			mpg123_param(context->mh, MPG123_FLAGS, MPG123_MONO_MIX, 0);
			mpg123_param(context->mh, MPG123_FLAGS, MPG123_SEEKBUFFER|MPG123_MONO_MIX, 0);
			return realsize;
		}

		context->mp3err = 0;
		usedlen += dlen;
		out += dlen;
		outlen -= dlen;
		dlen = 0;
	} while (decode_status != MPG123_NEED_MORE);


	switch_mutex_lock(context->audio_mutex);
	if (context->audio_buffer) {
		switch_buffer_write(context->audio_buffer, context->decode_buf, usedlen);
	} else {
		goto error;
	}
	switch_mutex_unlock(context->audio_mutex);

	return realsize;

  error:
	switch_mutex_lock(context->audio_mutex);
	context->err++;
	switch_mutex_unlock(context->audio_mutex);
	return 0;
}


#define MY_BUF_LEN 1024 * 32
#define MY_BLOCK_SIZE MY_BUF_LEN
static void *SWITCH_THREAD_FUNC read_stream_thread(switch_thread_t *thread, void *obj)
{
	CURL *curl_handle = NULL;
	shout_context_t *context = (shout_context_t *) obj;

	curl_handle = curl_easy_init();
	curl_easy_setopt(curl_handle, CURLOPT_URL, context->stream_url);
	curl_easy_setopt(curl_handle, CURLOPT_FOLLOWLOCATION, 1);
	curl_easy_setopt(curl_handle, CURLOPT_MAXREDIRS, 10);
	curl_easy_setopt(curl_handle, CURLOPT_WRITEFUNCTION, stream_callback);
	curl_easy_setopt(curl_handle, CURLOPT_WRITEDATA, (void *) context);
	curl_easy_setopt(curl_handle, CURLOPT_USERAGENT, "FreeSWITCH(mod_shout)/1.0");
	curl_easy_perform(curl_handle);
	curl_easy_cleanup(curl_handle);
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Thread Done\n");
	switch_mutex_lock(context->audio_mutex);
	context->err++;
	switch_mutex_unlock(context->audio_mutex);
	context->thread_running = 0;
	return NULL;
}

static void launch_read_stream_thread(shout_context_t *context)
{
	switch_thread_t *thread;
	switch_threadattr_t *thd_attr = NULL;

	if (context->err) {
		return;
	}

	context->thread_running = 1;
	switch_threadattr_create(&thd_attr, context->memory_pool);
	switch_threadattr_detach_set(thd_attr, 1);
	switch_threadattr_stacksize_set(thd_attr, SWITCH_THREAD_STACKSIZE);
	switch_thread_create(&thread, thd_attr, read_stream_thread, context, context->memory_pool);
}

static void *SWITCH_THREAD_FUNC write_stream_thread(switch_thread_t *thread, void *obj)
{
	shout_context_t *context = (shout_context_t *) obj;

	if (!context->lame_ready) {
		lame_init_params(context->gfp);
		lame_print_config(context->gfp);
		context->lame_ready = 1;
	}

	while (!context->err && context->thread_running) {
		unsigned char mp3buf[8192] = "";
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
			int i, j = 0;

			for (i = 0; i < audio_read / 4; i++) {
				l[i] = audio[j++];
				r[i] = audio[j++];
			}

			if ((rlen = lame_encode_buffer(context->gfp, l, r, audio_read / 4, mp3buf, sizeof(mp3buf))) < 0) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "MP3 encode error %d!\n", rlen);
				goto error;
			}

		} else if (context->channels == 1) {
			if ((rlen = lame_encode_buffer(context->gfp, (void *) audio, NULL, audio_read / sizeof(int16_t), mp3buf, sizeof(mp3buf))) < 0) {
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
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Thread Done\n");
	context->thread_running = 0;
	return NULL;
}

static void launch_write_stream_thread(shout_context_t *context)
{
	switch_thread_t *thread;
	switch_threadattr_t *thd_attr = NULL;

	if (context->err) {
		return;
	}

	context->thread_running = 1;
	switch_threadattr_create(&thd_attr, context->memory_pool);
	switch_threadattr_detach_set(thd_attr, 1);
	switch_threadattr_stacksize_set(thd_attr, SWITCH_THREAD_STACKSIZE);
	switch_thread_create(&thread, thd_attr, write_stream_thread, context, context->memory_pool);
}

#define TC_BUFFER_SIZE 1024 * 32
static switch_status_t shout_file_open(switch_file_handle_t *handle, const char *path)
{
	shout_context_t *context;
	char *host, *file;
	char *username, *password, *port;
	char *err = NULL;
	int portno = 0;

	if ((context = switch_core_alloc(handle->memory_pool, sizeof(*context))) == 0) {
		return SWITCH_STATUS_MEMERR;
	}

	if (!handle->samplerate) {
		handle->samplerate = 8000;
	}

	context->memory_pool = handle->memory_pool;
	context->samplerate = handle->samplerate;
	context->handle = handle;

	if (switch_test_flag(handle, SWITCH_FILE_FLAG_READ)) {
		if (switch_buffer_create_dynamic(&context->audio_buffer, TC_BUFFER_SIZE, TC_BUFFER_SIZE * 2, 0) != SWITCH_STATUS_SUCCESS) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Memory Error!\n");
			goto error;
		}

		switch_mutex_init(&context->audio_mutex, SWITCH_MUTEX_NESTED, context->memory_pool);
		context->mh = mpg123_new(NULL, NULL);
		mpg123_open_feed(context->mh);
		mpg123_format_all(context->mh);
		mpg123_param(context->mh, MPG123_FORCE_RATE, context->samplerate, 0);

		if (handle->handler) {
			mpg123_param(context->mh, MPG123_FLAGS, MPG123_SEEKBUFFER|MPG123_MONO_MIX, 0);
			context->stream_url = switch_core_sprintf(context->memory_pool, "http://%s", path);
			context->prebuf = handle->prebuf;
			launch_read_stream_thread(context);
		} else {
			mpg123_param(context->mh, MPG123_FLAGS, MPG123_MONO_MIX, 0);
			if (switch_file_open(&context->fd, path, SWITCH_FOPEN_READ, SWITCH_FPROT_UREAD | SWITCH_FPROT_UWRITE, handle->memory_pool) != 
				SWITCH_STATUS_SUCCESS) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Error opening %s\n", path);
				goto error;
			}
			
		}
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
		lame_set_brate(context->gfp, 24 * (handle->samplerate / 8000) * handle->channels);
		lame_set_num_channels(context->gfp, handle->channels);
		lame_set_in_samplerate(context->gfp, handle->samplerate);
		lame_set_out_samplerate(context->gfp, handle->samplerate);

		if (handle->channels == 2) {
			lame_set_mode(context->gfp, STEREO);
		} else {
			lame_set_mode(context->gfp, MONO);
		}
		lame_set_quality(context->gfp, 2);	/* 2=high  5 = medium  7=low */

		lame_set_errorf(context->gfp, log_error);
		lame_set_debugf(context->gfp, log_debug);
		lame_set_msgf(context->gfp, log_msg);

		if (handle->handler) {
			if (switch_buffer_create_dynamic(&context->audio_buffer, MY_BLOCK_SIZE, MY_BUF_LEN, 0) != SWITCH_STATUS_SUCCESS) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Memory Error!\n");
				goto error;
			}
			switch_mutex_init(&context->audio_mutex, SWITCH_MUTEX_NESTED, context->memory_pool);
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
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Error setting name: %s\n", shout_get_error(context->shout));
				goto error;
			}

			if (shout_set_audio_info(context->shout, "bitrate", "24000") != SHOUTERR_SUCCESS) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Error setting user: %s\n", shout_get_error(context->shout));
				goto error;
			}

			if (shout_set_format(context->shout, SHOUT_FORMAT_MP3) != SHOUTERR_SUCCESS) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Error setting user: %s\n", shout_get_error(context->shout));
				goto error;
			}

		} else {
			handle->seekable = 1;
			/* lame being lame and all has FILE * coded into it's API for some functions so we gotta use it */
			if (!(context->fp = fopen(path, "wb+"))) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Error opening %s\n", path);
				return SWITCH_STATUS_GENERR;
			}
		}
	}

	handle->samples = 0;
	handle->format = 0;
	handle->sections = 0;
	handle->speed = 0;
	handle->private_info = context;

	return SWITCH_STATUS_SUCCESS;

  error:
	if (err) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Error: %s\n", err);
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

	if (handle->handler) {
		return SWITCH_STATUS_FALSE;
	} else {
		switch_mutex_lock(context->audio_mutex);
		if (context->audio_buffer) {
			if (context->fd) {
				switch_file_seek(context->fd, whence, &samples);
			} else if (context->fp) {
				*cur_sample = fseek(context->fp, *cur_sample, whence);
			}

			mpg123_close(context->mh);
			context->mh = mpg123_new(NULL, NULL);
			mpg123_open_feed(context->mh);
			mpg123_param(context->mh, MPG123_FORCE_RATE, context->samplerate, 0);
			mpg123_param(context->mh, MPG123_FLAGS, MPG123_MONO_MIX, 0);
			switch_buffer_zero(context->audio_buffer);

		} else {
			context->err++;
		}
		switch_mutex_unlock(context->audio_mutex);
		return SWITCH_STATUS_SUCCESS;
	}
}

static switch_status_t shout_file_read(switch_file_handle_t *handle, void *data, size_t *len)
{
	shout_context_t *context = handle->private_info;
	size_t rb = 0, bytes = *len * sizeof(int16_t);

	*len = 0;

	if (context->fd) {
		rb = decode_fd(context, data, bytes);
	} else {
		switch_mutex_lock(context->audio_mutex);
		if (context->audio_buffer) {
			rb = switch_buffer_read(context->audio_buffer, data, bytes);
		} else {
			switch_mutex_lock(context->audio_mutex);
			context->err++;
			switch_mutex_unlock(context->audio_mutex);
		}
		switch_mutex_unlock(context->audio_mutex);
	}

	if (context->err) {
		return SWITCH_STATUS_FALSE;
	}

	if (rb) {
		*len = rb / sizeof(int16_t);
	} else {
		memset(data, 255, bytes);
		*len = bytes / sizeof(int16_t);
	}

	handle->sample_count += *len;

	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t shout_file_write(switch_file_handle_t *handle, void *data, size_t *len)
{
	shout_context_t *context = handle->private_info;
	unsigned char mp3buf[8192] = "";
	int rlen;
	int16_t *audio = data;
	int nsamples = *len;

	if (context->shout && !context->shout_init) {
		context->shout_init++;
		if (shout_open(context->shout) != SHOUTERR_SUCCESS) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Error opening stream: %s\n", shout_get_error(context->shout));
			context->err++;
			return SWITCH_STATUS_FALSE;
		}
		launch_write_stream_thread(context);
	}

	if (handle->handler) {
		switch_mutex_lock(context->audio_mutex);
		if (context->audio_buffer) {
			if (!switch_buffer_write(context->audio_buffer, data, (nsamples * sizeof(int16_t) * handle->channels))) {
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
	} else {

		if (!context->lame_ready) {
			lame_init_params(context->gfp);
			lame_print_config(context->gfp);
			context->lame_ready = 1;
		}

		if (handle->channels == 2) {
			int16_t l[4096] = { 0 };
			int16_t r[4096] = { 0 };
			int i, j = 0;

			for (i = 0; i < nsamples; i++) {
				l[i] = audio[j++];
				r[i] = audio[j++];
			}

			if ((rlen = lame_encode_buffer(context->gfp, l, r, nsamples, mp3buf, sizeof(mp3buf))) < 0) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "MP3 encode error %d!\n", rlen);
				return SWITCH_STATUS_FALSE;
			}

		} else if (handle->channels == 1) {
			if ((rlen = lame_encode_buffer(context->gfp, audio, NULL, nsamples, mp3buf, sizeof(mp3buf))) < 0) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "MP3 encode error %d!\n", rlen);
				return SWITCH_STATUS_FALSE;
			}
		} else {
			rlen = 0;
		}

		if (rlen) {
			int ret = fwrite(mp3buf, 1, rlen, context->fp);
			if (ret < 0) {
				return SWITCH_STATUS_FALSE;
			}
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
			break;
			id3tag_set_album(context->gfp, string);
		case SWITCH_AUDIO_COL_STR_COPYRIGHT:
			id3tag_set_genre(context->gfp, string);
			break;
		default:
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "Value Ignored\n");
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
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "Value Ignored\n");
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
			if (switch_core_media_bug_read(bug, &frame) == SWITCH_STATUS_SUCCESS) {
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
	char title_aft[128 * 3] = "";
	char *mp3, *m3u;

	/*
	   0  uuid  VARCHAR(255),
	   1  created  VARCHAR(255),
	   2  name  VARCHAR(255),
	   3  state  VARCHAR(255),
	   4  cid_name  VARCHAR(255),
	   5  cid_num  VARCHAR(255),
	   6  ip_addr  VARCHAR(255),
	   7  dest  VARCHAR(255),
	   8  application  VARCHAR(255),
	   9  application_data  VARCHAR(255),
	   10 read_codec  VARCHAR(255),
	   11 read_rate  VARCHAR(255),
	   12 write_codec  VARCHAR(255),
	   13 write_rate  VARCHAR(255)
	 */

	holder->stream->write_function(holder->stream,
								   "<tr><td>%s</td><td>%s</td><td>%s</td><td>%s</td><td>%s</td><td>%s</td><td>%s</td><td>%s</td><td>",
								   argv[1], argv[4], argv[5], argv[7], argv[8] ? argv[8] : "N/A", argv[9] ? argv[9] : "N/A", argv[10], argv[11]);

	snprintf(title_b4, sizeof(title_b4), "%s <%s>", argv[4], argv[5]);
	switch_url_encode(title_b4, title_aft, sizeof(title_aft));

	mp3 = switch_mprintf("http://%s:%s%s/mp3/%s/%s.mp3", holder->host, holder->port, holder->uri, argv[0], argv[5]);
	m3u = switch_mprintf("http://%s:%s%s/m3u/mp3/%s/%s.mp3.m3u", holder->host, holder->port, holder->uri, argv[0], argv[5]);

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
		switch_codec_t *read_codec;

		if (!(gfp = lame_init())) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Could not allocate lame\n");
			goto end;
		}
		read_codec = switch_core_session_get_read_codec(tsession);

		lame_set_num_channels(gfp, read_codec->implementation->number_of_channels);
		lame_set_in_samplerate(gfp, read_codec->implementation->actual_samples_per_second);
		lame_set_brate(gfp, 24);
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

		if (switch_core_media_bug_add(tsession, telecast_callback, buffer, 0,
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
					switch_yield(1000);
					continue;
				}
				memset(buf, 0, bytes);
			}

			if ((rlen = lame_encode_buffer(gfp, (void *) buf, NULL, bytes / 2, mp3buf, sizeof(mp3buf))) < 0) {
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
	lame_set_brate(gfp, 24);
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

		if ((rlen = lame_encode_buffer(gfp, (void *) buf, NULL, samples, mp3buf, sizeof(mp3buf))) < 0) {
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
	switch_core_db_t *db = switch_core_db_handle();
	const char *sql = "select * from channels";
	struct holder holder;
	char *errmsg;

	holder.host = switch_event_get_header(stream->param_event, "http-host");
	holder.port = switch_event_get_header(stream->param_event, "http-port");
	holder.uri = switch_event_get_header(stream->param_event, "http-uri");
	holder.stream = stream;

	stream->write_function(stream, "Content-type: text/html\r\n\r\n");
	stream->write_function(stream,
						   "<table align=center border=1 cellpadding=6 cellspacing=0>"
						   "<tr><td>%s</td><td>%s</td><td>%s</td><td>%s</td><td>%s</td><td>%s</td><td>%s</td><td>%s</td><td>%s</td></tr>\n",
						   "Created", "CID Name", "CID Num", "Ext", "App", "Data", "Codec", "Rate", "Listen");

	switch_core_db_exec(db, sql, web_callback, &holder, &errmsg);

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

SWITCH_MODULE_LOAD_FUNCTION(mod_shout_load)
{
	switch_api_interface_t *shout_api_interface;
	switch_file_interface_t *file_interface;

	supported_formats[0] = "shout";
	supported_formats[1] = "mp3";

	curl_global_init(CURL_GLOBAL_ALL);

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

	/* connect my internal structure to the blank pointer passed to me */
	//*module_interface = &shout_module_interface;

	shout_init();
	mpg123_init();

	SWITCH_ADD_API(shout_api_interface, "telecast", "telecast", telecast_api_function, TELECAST_SYNTAX);

	/* indicate that the module should continue to be loaded */
	return SWITCH_STATUS_SUCCESS;
}

SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_shout_shutdown)
{
	curl_global_cleanup();
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
 * vim:set softtabstop=4 shiftwidth=4 tabstop=4 expandtab:
 */
