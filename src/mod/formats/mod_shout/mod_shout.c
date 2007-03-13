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


#define OUTSCALE 4096
#define MP3BUFLEN OUTSCALE * 2

static const char modname[] = "mod_shout";


static char *supported_formats[SWITCH_MAX_CODECS] = {0};

struct shout_context {
	shout_t *shout;
    lame_global_flags *gfp;
    char *stream_url;
	switch_mutex_t *audio_mutex;
	switch_buffer_t *audio_buffer;
    switch_memory_pool_t *memory_pool;
    char decode_buf[MP3BUFLEN];
    struct mpstr mp;
};

typedef struct shout_context shout_context_t;

static inline void free_context(shout_context_t *context)
{
    if (context) {
        if (context->audio_buffer) {
            switch_buffer_destroy(&context->audio_buffer);
        }

        if (context->shout) {
            shout_close(context->shout);
            context->shout = NULL;
        }

        if (context->gfp) {
            lame_close(context->gfp);
            context->gfp = NULL;
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
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, (char*) "%s", data);
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
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, (char*) "%s", data);
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
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, (char*) "%s", data);
		free(data);
	}
}


static size_t stream_callback(void *ptr, size_t size, size_t nmemb, void *data)
{
	register unsigned int realsize = (unsigned int)(size * nmemb);
	shout_context_t *context = data;
    int dlen;
    
    decodeMP3(&context->mp, data, realsize, context->decode_buf, sizeof(context->decode_buf), &dlen);

    switch_mutex_lock(context->audio_mutex);
    switch_buffer_write(context->audio_buffer, context->decode_buf, dlen);
    switch_mutex_unlock(context->audio_mutex);

	return realsize;
}


#define MY_BUF_LEN 1024 * 32
#define MY_BLOCK_SIZE MY_BUF_LEN
static void *SWITCH_THREAD_FUNC stream_thread(switch_thread_t *thread, void *obj)
{
    CURL *curl_handle = NULL;
    shout_context_t *context = (shout_context_t *) obj;

    curl_easy_setopt(curl_handle, CURLOPT_URL, context->stream_url);
    curl_easy_setopt(curl_handle, CURLOPT_WRITEFUNCTION, stream_callback);
    curl_easy_setopt(curl_handle, CURLOPT_WRITEDATA, (void *)context);
    curl_easy_setopt(curl_handle, CURLOPT_USERAGENT, "FreeSWITCH(mod_shout)/1.0");
    curl_easy_perform(curl_handle);
    curl_easy_cleanup(curl_handle);

    return NULL;
}

static void launch_stream_thread(shout_context_t *context)
{
	switch_thread_t *thread;
	switch_threadattr_t *thd_attr = NULL;
	
	switch_threadattr_create(&thd_attr, context->memory_pool);
	switch_threadattr_detach_set(thd_attr, 1);
	switch_threadattr_stacksize_set(thd_attr, SWITCH_THREAD_STACKSIZE);
	switch_thread_create(&thread, thd_attr, stream_thread, context, context->memory_pool);
}

static switch_status_t shout_file_open(switch_file_handle_t *handle, char *path)
{
	shout_context_t *context;
    char *host, *file;
    char *username, *password;
    char *err = NULL;

	if ((context = switch_core_alloc(handle->memory_pool, sizeof(*context))) == 0) {
		return SWITCH_STATUS_MEMERR;
	}

	if (!(context->shout = shout_new())) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Could not allocate shout_t\n");
		goto error;
	}

    if (!(context->gfp = lame_init())) {
        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Could not allocate lame\n");
        goto error;
    }

    context->memory_pool = handle->memory_pool;

    lame_set_num_channels(context->gfp, handle->channels);
    lame_set_in_samplerate(context->gfp, handle->samplerate);
    lame_set_brate(context->gfp, 24);
    lame_set_mode(context->gfp, 3);
    lame_set_quality(context->gfp, 2);   /* 2=high  5 = medium  7=low */

    lame_set_errorf(context->gfp, log_error);
    lame_set_debugf(context->gfp, log_debug);
    lame_set_msgf(context->gfp, log_msg);
    lame_set_bWriteVbrTag(context->gfp, 0);
    lame_mp3_tags_fid(context->gfp, NULL);
    lame_init_params(context->gfp);
    lame_print_config(context->gfp);

    if (switch_test_flag(handle, SWITCH_FILE_FLAG_READ)) {
        if (switch_buffer_create_dynamic(&context->audio_buffer, MY_BLOCK_SIZE, MY_BUF_LEN, 0) != SWITCH_STATUS_SUCCESS) {
            switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Memory Error!\n");
            goto error;
        }
        switch_mutex_init(&context->audio_mutex, SWITCH_MUTEX_NESTED, context->memory_pool);
        InitMP3(&context->mp, OUTSCALE);
        context->stream_url = switch_core_sprintf(context->memory_pool, "http://%s", path);
        launch_stream_thread(context);
    } else if (!switch_test_flag(handle, SWITCH_FILE_FLAG_WRITE)) {
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

        if (shout_set_host(context->shout, host) != SHOUTERR_SUCCESS) {
            switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Error setting hostname: %s\n", shout_get_error(context->shout));
            goto error;
        }

        if (shout_set_protocol(context->shout, SHOUT_PROTOCOL_HTTP) != SHOUTERR_SUCCESS) {
            switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Error setting protocol: %s\n", shout_get_error(context->shout));
            goto error;
        }
    
        if (shout_set_port(context->shout, 8000) != SHOUTERR_SUCCESS) {
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

        if (shout_set_url(context->shout, "mod_shout") != SHOUTERR_SUCCESS) {
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

        if (shout_open(context->shout) != SHOUTERR_SUCCESS) {
            switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Error opening stream: %s\n", shout_get_error(context->shout));
            goto error;
        }
    }

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
	//shout_context_t *context = handle->private_info;
	
	return SWITCH_STATUS_FALSE;

}

static switch_status_t shout_file_read(switch_file_handle_t *handle, void *data, size_t *len)
{
	shout_context_t *context = handle->private_info;
    size_t bytes = *len;

    switch_mutex_lock(context->audio_mutex);
    *len = switch_buffer_read(context->audio_buffer, data, bytes);
    switch_mutex_unlock(context->audio_mutex);

	return SWITCH_STATUS_FALSE;
}

static switch_status_t shout_file_write(switch_file_handle_t *handle, void *data, size_t *len)
{
	shout_context_t *context = handle->private_info;


    unsigned char mp3buf[2048] = "";
    long ret = 0;
    int rlen;
    int16_t *audio = data;
    int nsamples = *len;

    if ((rlen = lame_encode_buffer(context->gfp, audio, NULL, nsamples, mp3buf, sizeof(mp3buf))) < 0) {
        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "MP3 encode error %d!\n", rlen);
        return SWITCH_STATUS_FALSE;
    }

    if (rlen) {
        ret = shout_send(context->shout, mp3buf, rlen);
        if (ret != SHOUTERR_SUCCESS) {
            switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Send error: %s\n", shout_get_error(context->shout));
            return SWITCH_STATUS_FALSE;
        }
    }
    shout_sync(context->shout);

    return SWITCH_STATUS_SUCCESS;
}

static switch_status_t shout_file_set_string(switch_file_handle_t *handle, switch_audio_col_t col, const char *string)
{
	shout_context_t *context = handle->private_info;

    switch(col) {
    case SWITCH_AUDIO_COL_STR_TITLE:
        if (shout_set_name(context->shout, string) != SHOUTERR_SUCCESS) {
            switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Error setting name: %s\n", shout_get_error(context->shout));
        }
        break;
    case SWITCH_AUDIO_COL_STR_COMMENT:
        if (shout_set_url(context->shout, string) != SHOUTERR_SUCCESS) {
            switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Error setting name: %s\n", shout_get_error(context->shout));
        }
        break;
    default:
        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "Value Ignored\n");
        break;
    }
	return SWITCH_STATUS_FALSE;
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

SWITCH_MOD_DECLARE(switch_status_t) switch_module_load(const switch_loadable_module_interface_t **module_interface, char *filename)
{
    supported_formats[0] = "shout";
	shout_file_interface.extens = supported_formats;

    curl_global_init(CURL_GLOBAL_ALL);

	/* connect my internal structure to the blank pointer passed to me */
	*module_interface = &shout_module_interface;

	shout_init();

	/* indicate that the module should continue to be loaded */
	return SWITCH_STATUS_SUCCESS;
}


SWITCH_MOD_DECLARE(switch_status_t) switch_module_shutdown(void)
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
