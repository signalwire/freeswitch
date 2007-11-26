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
#include "mpg123.h"
#include "mpglib.h"
#include <switch.h>
#include <shout/shout.h>
#include <lame.h>
#include <curl/curl.h>

#define OUTSCALE 8192 * 2
#define MP3_SCACHE 16384 * 2
#define MP3_DCACHE 8192 *2

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
	char decode_buf[MP3_DCACHE];
	struct mpstr mp;
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
};

typedef struct shout_context shout_context_t;


static size_t decode_fd(shout_context_t * context, void *data, size_t bytes);

static inline void free_context(shout_context_t * context)
{
	if (context) {
		context->err++;

		if (context->fd) {
			switch_file_close(context->fd);
			context->fd = NULL;
		}

		if (context->fp) {
			unsigned char mp3buffer[1024];
			int len;

			while ((len = lame_encode_flush(context->gfp, mp3buffer, sizeof(mp3buffer))) > 0) {
				fwrite(mp3buffer, 1, len, context->fp);
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

			ExitMP3(&context->mp);
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

static size_t decode_fd(shout_context_t * context, void *data, size_t bytes)
{
	int decode_status = 0;
	int dlen = 0;
	int x = 0;
	char *in;
	int inlen;
	char *out;
	int outlen;
	int usedlen;
	char inbuf[MP3_SCACHE];
	int done = 0;

	size_t lp;
	size_t rb = 0;

	while (switch_buffer_inuse(context->audio_buffer) < bytes) {
		lp = sizeof(inbuf);
		if ((switch_file_read(context->fd, inbuf, &lp) != SWITCH_STATUS_SUCCESS) || lp == 0) {
			goto error;
		}

		inlen = (int) lp;
		in = inbuf;

		out = context->decode_buf;
		outlen = (int) sizeof(context->decode_buf);
		usedlen = 0;
		x = 0;

		if (inlen < bytes) {
			done = 1;
		}

		do {
			decode_status = decodeMP3(&context->mp, in, inlen, out, outlen, &dlen);

			if (context->err) {
				goto error;
			}

			if (!x) {
				in = NULL;
				inlen = 0;
				x++;
			}

			if (decode_status == MP3_TOOSMALL) {
				if (context->audio_buffer) {
					switch_buffer_write(context->audio_buffer, context->decode_buf, usedlen);
				} else {
					goto error;
				}

				out = context->decode_buf;
				outlen = sizeof(context->decode_buf);
				usedlen = 0;
				continue;

			}

			if (decode_status == MP3_ERR) {
				if (++context->mp3err >= 20) {
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Decoder Error!\n");
				}
				dlen = 0;
				//continue;
                goto error;
			}

			context->mp3err = 0;

			usedlen += dlen;
			out += dlen;
			outlen -= dlen;
			dlen = 0;


		} while (decode_status != MP3_NEED_MORE);


		if (context->audio_buffer) {
			switch_buffer_write(context->audio_buffer, context->decode_buf, usedlen);
		} else {
			goto error;
		}

		if (done) {
			break;
		}
	}

	if (done || switch_buffer_inuse(context->audio_buffer) >= bytes) {
		rb = switch_buffer_read(context->audio_buffer, data, bytes);
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
	int dlen = 0;
	int x = 0;
	char *in;
	int inlen;
	char *out;
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
		decode_status = decodeMP3(&context->mp, in, inlen, out, outlen, &dlen);

		error_check();

		if (!x) {
			in = NULL;
			inlen = 0;
			x++;
		}

		if (decode_status == MP3_TOOSMALL) {
			switch_mutex_lock(context->audio_mutex);
			if (context->audio_buffer) {
				switch_buffer_write(context->audio_buffer, context->decode_buf, usedlen);
			} else {
				goto error;
			}
			out = context->decode_buf;
			outlen = sizeof(context->decode_buf);
			usedlen = 0;
			switch_mutex_unlock(context->audio_mutex);

		} else if (decode_status == MP3_ERR) {

			if (++context->mp3err >= 20) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Decoder Error!\n");
				goto error;
			}

			ExitMP3(&context->mp);
			InitMP3(&context->mp, OUTSCALE, context->samplerate);

			return realsize;
		}

		context->mp3err = 0;
		usedlen += dlen;
		out += dlen;
		outlen -= dlen;
		dlen = 0;
	} while (decode_status != MP3_NEED_MORE);


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
static void *SWITCH_THREAD_FUNC read_stream_thread(switch_thread_t * thread, void *obj)
{
	CURL *curl_handle = NULL;
	shout_context_t *context = (shout_context_t *) obj;

	curl_handle = curl_easy_init();
	curl_easy_setopt(curl_handle, CURLOPT_URL, context->stream_url);
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

static void launch_read_stream_thread(shout_context_t * context)
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

static void *SWITCH_THREAD_FUNC write_stream_thread(switch_thread_t * thread, void *obj)
{
	shout_context_t *context = (shout_context_t *) obj;

	while (!context->err && context->thread_running) {
		unsigned char mp3buf[8192] = "";
		unsigned char audio[8192] = "";
		switch_size_t audio_read = 0;
		int rlen;
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

		if ((rlen = lame_encode_buffer(context->gfp, (void *) audio, NULL, audio_read / sizeof(int16_t), mp3buf, sizeof(mp3buf))) < 0) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "MP3 encode error %d!\n", rlen);
			goto error;
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

static void launch_write_stream_thread(shout_context_t * context)
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

	if (switch_test_flag(handle, SWITCH_FILE_FLAG_READ)) {
		if (switch_buffer_create_dynamic(&context->audio_buffer, MY_BLOCK_SIZE, MY_BUF_LEN, 0) != SWITCH_STATUS_SUCCESS) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Memory Error!\n");
			goto error;
		}

		switch_mutex_init(&context->audio_mutex, SWITCH_MUTEX_NESTED, context->memory_pool);
		InitMP3(&context->mp, OUTSCALE, context->samplerate);
		if (handle->handler) {
			context->stream_url = switch_core_sprintf(context->memory_pool, "http://%s", path);
            context->prebuf = handle->prebuf;
			launch_read_stream_thread(context);
		} else {
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

		lame_set_num_channels(context->gfp, handle->channels);
		lame_set_in_samplerate(context->gfp, handle->samplerate);
		lame_set_brate(context->gfp, 24);
		lame_set_mode(context->gfp, 3);
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
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Cannot seek on a stream.\n");
		return SWITCH_STATUS_FALSE;
	} else {
		switch_mutex_lock(context->audio_mutex);
		if (context->audio_buffer) {
			if (context->fd) {
				switch_file_seek(context->fd, whence, &samples);
			} else if (context->fp) {
				*cur_sample = fseek(context->fp, *cur_sample, whence);
			}

            ExitMP3(&context->mp);
            InitMP3(&context->mp, OUTSCALE, context->samplerate);
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
			if (!switch_buffer_write(context->audio_buffer, data, nsamples * sizeof(int16_t))) {
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

		if ((rlen = lame_encode_buffer(context->gfp, audio, NULL, nsamples, mp3buf, sizeof(mp3buf))) < 0) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "MP3 encode error %d!\n", rlen);
			return SWITCH_STATUS_FALSE;
		}
		if (rlen) {
			if (fwrite(mp3buf, 1, rlen, context->fp) < 0) {
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

static switch_file_interface_t shout_file_interface = {
	/*.interface_name */ modname,
	/*.file_open */ shout_file_open,
	/*.file_close */ shout_file_close,
	/*.file_read */ shout_file_read,
	/*.file_write */ shout_file_write,
	/*.file_seek */ shout_file_seek,
	/*.file_set_string */ shout_file_set_string,
	/*.file_get_string */ shout_file_get_string,
	/*.extens */ NULL,
	/*.next */ NULL,
};

static switch_loadable_module_interface_t shout_module_interface = {
	/*.module_name */ modname,
	/*.endpoint_interface */ NULL,
	/*.timer_interface */ NULL,
	/*.dialplan_interface */ NULL,
	/*.codec_interface */ NULL,
	/*.application_interface */ NULL,
	/*.api_interface */ NULL,
	/*.file_interface */ &shout_file_interface,
	/*.speech_interface */ NULL,
	/*.directory_interface */ NULL
};

SWITCH_MODULE_LOAD_FUNCTION(mod_shout_load)
{
	supported_formats[0] = "shout";
	supported_formats[1] = "mp3";
	shout_file_interface.extens = supported_formats;

	curl_global_init(CURL_GLOBAL_ALL);

	/* connect my internal structure to the blank pointer passed to me */
	*module_interface = &shout_module_interface;

	shout_init();
	InitMP3Constants();

	/* indicate that the module should continue to be loaded */
	return SWITCH_STATUS_SUCCESS;
}


SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_shout_shutdown)
{
	curl_global_cleanup();
	return SWITCH_STATUS_SUCCESS;
}

/* For Emacs:
 * Local Variables:
 * mode:c
 * indent-tabs-mode:nil
 * tab-width:4
 * c-basic-offset:4
 * End:
 * For VIM:
 * vim:set softtabstop=4 shiftwidth=4 tabstop=4 expandtab:
 */
