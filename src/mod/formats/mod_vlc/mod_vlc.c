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
 * William King william.king@quentustech.com
 *
 * mod_vlc.c -- VLC streaming module
 *
 * Examples:
 *
 * To playback from an audio source into a file:
 * File: vlc:///path/to/file
 * Stream: http://path.to.file.com:port/file.pls
 * Stream: vlc://ftp://path.to.file.com:port/file.mp3
 *
 * To stream from a call(channel) out to a remote destination:
 * vlc://#transcode{acodec=vorb,channels=1,samplerate=16000}:standard{access=http,mux=ogg,dst=:8080/thing.ogg}
 *
 * Notes: 
 *
 * Requires at least libvlc version 1.2
 *
 */
#include <switch.h>
#include <vlc/vlc.h>
#include <vlc/libvlc_media_player.h>
#include <vlc/libvlc_events.h>

#define VLC_BUFFER_SIZE 65536

static char *vlc_file_supported_formats[SWITCH_MAX_CODECS] = { 0 };

typedef int  (*imem_get_t)(void *data, const char *cookie,
                           int64_t *dts, int64_t *pts, unsigned *flags,
                           size_t *, void **);
typedef void (*imem_release_t)(void *data, const char *cookie, size_t, void *);

/* Change valud to -vvv for vlc related debug. Be careful since vlc is at least as verbose as FS about logging */
const char *vlc_args = "";

libvlc_instance_t *read_inst;

struct vlc_file_context {
	libvlc_media_player_t *mp;
	libvlc_media_t *m;
	switch_file_handle_t fh;
	switch_memory_pool_t *pool;
	switch_buffer_t *audio_buffer;
	switch_mutex_t *audio_mutex;
	switch_thread_cond_t *started;
	char *path;
	int samples;
	int playing;
	int samplerate;
	int err;
	int pts;
	libvlc_instance_t *inst_out;
};

typedef struct vlc_file_context vlc_file_context_t;

SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_vlc_shutdown);
SWITCH_MODULE_LOAD_FUNCTION(mod_vlc_load);
SWITCH_MODULE_DEFINITION(mod_vlc, mod_vlc_load, mod_vlc_shutdown, NULL);

static void vlc_mediaplayer_error_callback(const libvlc_event_t * event, void * data)
{
        vlc_file_context_t *context = (vlc_file_context_t *) data;
        int status = libvlc_media_get_state(context->m);
        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Got a libvlc_MediaPlayerEncounteredError callback. mediaPlayer Status: %d\n", status);
        if (status == libvlc_Error) {
               context->err = 1;
               switch_thread_cond_signal(context->started);
	     }
}
static void vlc_media_state_callback(const libvlc_event_t * event, void * data)
{
        vlc_file_context_t *context = (vlc_file_context_t *) data;
        int new_state = event->u.media_state_changed.new_state;

        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Got a libvlc_MediaStateChanged callback. New state: %d\n", new_state);
        if (new_state == libvlc_Ended || new_state == libvlc_Error) {
                switch_thread_cond_signal(context->started);
        }
}



void vlc_auto_play_callback(void *data, const void *samples, unsigned count, int64_t pts) {

	vlc_file_context_t *context = (vlc_file_context_t *) data;
	
	switch_mutex_lock(context->audio_mutex);
	if (context->audio_buffer) {
		if (!switch_buffer_write(context->audio_buffer, samples, count * 2)) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Buffer error\n");
		}
	}

	if(! context->playing ) {
		context->playing = 1;
		switch_thread_cond_signal(context->started);
	}	
	switch_mutex_unlock(context->audio_mutex);
}

int  vlc_imem_get_callback(void *data, const char *cookie, int64_t *dts, int64_t *pts, unsigned *flags, size_t *size, void **output)
{
	vlc_file_context_t *context = (vlc_file_context_t *) data;
	int samples = 0;
	int bytes = 0;
	
	switch_mutex_lock(context->audio_mutex);
	
	/* If the stream should no longer be sending audio    */
	/* then pretend we have less than one sample of audio */
	/* so that libvlc will close the client connections   */
	if ( context->playing == 0 && switch_buffer_inuse(context->audio_buffer) == 0 ) {
		switch_mutex_unlock(context->audio_mutex);		
		return 1; 
	}
	
	samples = context->samples;
	context->samples = 0;
	
	if ( samples ) {
		bytes = samples * 2;
		*output = malloc(bytes);
		bytes = switch_buffer_read(context->audio_buffer, *output, bytes);
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "VLC imem samples: %d\n", samples);
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "VLC imem bytes: %d\n", bytes);
	} else {
		bytes = 128;
		*output = malloc(bytes);
		memset(*output, 0, bytes);
	}
	switch_mutex_unlock(context->audio_mutex);
	
	*size = (size_t) bytes;
	return 0;
}

void vlc_imem_release_callback(void *data, const char *cookie, size_t size, void *unknown)
{
	free(unknown);
}

static switch_status_t vlc_file_open(switch_file_handle_t *handle, const char *path)
{
	vlc_file_context_t *context;
	libvlc_event_manager_t *mp_event_manager, *m_event_manager;
	
	context = switch_core_alloc(handle->memory_pool, sizeof(*context));
	context->pool = handle->memory_pool;

	context->path = switch_core_strdup(context->pool, path);

	switch_buffer_create_dynamic(&(context->audio_buffer), VLC_BUFFER_SIZE, VLC_BUFFER_SIZE * 8, 0);
	switch_mutex_init(&context->audio_mutex, SWITCH_MUTEX_NESTED, context->pool);
	switch_thread_cond_create(&(context->started), context->pool);

	if (switch_test_flag(handle, SWITCH_FILE_FLAG_READ)) {
		
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "VLC open %s for reading\n", path);
		
		/* Determine if this is a url or a path */
		/* TODO: Change this so that it tries local files first, and then if it fails try location. */
		if(! strncmp(context->path, "http", 4)){
			context->m = libvlc_media_new_location(read_inst, context->path);
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "VLC Path is http %s\n", context->path);
		} else if (! strncmp(context->path, "mms", 3)){
			context->m = libvlc_media_new_path(read_inst, context->path);
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "VLC Path is mms %s\n", context->path);
		} else if (! strncmp(context->path, "/", 1)){
			context->m = libvlc_media_new_path(read_inst, context->path);
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "VLC Path is file %s\n", context->path);
		} else {
			context->m = libvlc_media_new_location(read_inst, context->path);
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "VLC Path is unknown type %s\n", context->path);
		}

		if ( context->m == NULL ) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "VLC error opening %s for reading\n", path);
			return SWITCH_STATUS_GENERR;
		}
		
		context->playing = 0;
		context->err = 0;
		
		context->mp = libvlc_media_player_new_from_media(context->m);
		
		if ( !handle->samplerate)
			handle->samplerate = 16000;

		context->samplerate = handle->samplerate;
		
		libvlc_audio_set_format(context->mp, "S16N", context->samplerate, 1);
		
		m_event_manager = libvlc_media_event_manager(context->m);
		libvlc_event_attach(m_event_manager, libvlc_MediaStateChanged, vlc_media_state_callback, (void *) context);

		mp_event_manager = libvlc_media_player_event_manager(context->mp);
		libvlc_event_attach(mp_event_manager, libvlc_MediaPlayerEncounteredError, vlc_mediaplayer_error_callback, (void *) context);
		
		libvlc_audio_set_callbacks(context->mp, vlc_auto_play_callback, NULL,NULL,NULL,NULL, (void *) context);

		libvlc_media_player_play(context->mp);
		
	} else if (switch_test_flag(handle, SWITCH_FILE_FLAG_WRITE)) {
		const char * opts[10] = {
			vlc_args,
			switch_mprintf("--sout=%s", path)
		};
		int opts_count = 10;
		
		if ( !handle->samplerate)
			handle->samplerate = 16000;
		
		context->samplerate = handle->samplerate;

		opts[2] = switch_mprintf("--imem-get=%ld", vlc_imem_get_callback);
		opts[3] = switch_mprintf("--imem-release=%ld", vlc_imem_release_callback);
		opts[4] = switch_mprintf("--imem-cat=%d", 4);
		opts[5] = "--demux=rawaud";
		opts[6] = "--rawaud-fourcc=s16l";
		opts[7] = switch_mprintf("--rawaud-samplerate=%d", context->samplerate);
		opts[8] = switch_mprintf("--imem-data=%ld", context);
		opts[9] = "--rawaud-channels=1";

		/* Prepare to write to an output stream. */
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "VLC open %s for writing\n", path);

		/* load the vlc engine. */
		context->inst_out = libvlc_new(opts_count, opts);
		
		/* Tell VLC the audio will come from memory, and to use the callbacks to fetch it. */
		context->m = libvlc_media_new_location(context->inst_out, "imem/rawaud://");
		context->mp = libvlc_media_player_new_from_media(context->m);
		context->samples = 0;
		context->pts = 0;
		context->playing = 1;		
		libvlc_media_player_play(context->mp);
	} else {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "VLC tried to open %s for unknown reason\n", path);
		return SWITCH_STATUS_GENERR;
	}

	handle->private_info = context;
	
	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t vlc_file_read(switch_file_handle_t *handle, void *data, size_t *len)
{
	vlc_file_context_t *context = handle->private_info;
	size_t bytes = *len * sizeof(int16_t), read;
	libvlc_state_t status;
	
	if (!context) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "VLC read handle context is NULL\n");
		return SWITCH_STATUS_GENERR;
	}

	if (context->err) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "VLC error\n");
		return SWITCH_STATUS_GENERR;
	}

	status = libvlc_media_get_state(context->m);
	
	if (status == libvlc_Error) {
		return SWITCH_STATUS_GENERR;
	}

	switch_mutex_lock(context->audio_mutex); 
	while (context->playing == 0 && status != libvlc_Ended && status != libvlc_Error) {
		switch_thread_cond_wait(context->started, context->audio_mutex);		
		status = libvlc_media_get_state(context->m);
	}

	if (context->err == 1) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "VLC error\n");
		return SWITCH_STATUS_FALSE;
	}

	switch_mutex_unlock(context->audio_mutex);
	
	switch_mutex_lock(context->audio_mutex);
	read = switch_buffer_read(context->audio_buffer, data, bytes);
	switch_mutex_unlock(context->audio_mutex);
	
        status = libvlc_media_get_state(context->m);
	if (!read && (status == libvlc_Stopped || status == libvlc_Ended || status == libvlc_Error)) {
		return SWITCH_STATUS_FALSE;
	} else if (!read) {
		read = 2;
		memset(data, 0, read);
	}
	
	if (read)
		*len = read/2;
	
	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t vlc_file_write(switch_file_handle_t *handle, void *data, size_t *len)
{
	vlc_file_context_t *context = handle->private_info;
	size_t bytes = *len * sizeof(int16_t);
	
	switch_mutex_lock(context->audio_mutex);
	context->samples += *len;
	switch_buffer_write(context->audio_buffer, data, bytes);
	switch_mutex_unlock(context->audio_mutex);	
	
	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t vlc_file_close(switch_file_handle_t *handle)
{
	vlc_file_context_t *context = handle->private_info;
	int sanity = 0;	
	
	context->playing = 0;
	
	/* The clients need to empty the last of the audio buffer */
	while ( switch_buffer_inuse(context->audio_buffer) > 0 ) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "VLC waiting to close the files: %d \n", (int) switch_buffer_inuse(context->audio_buffer));
		switch_yield(500000);
		if (++sanity > 10) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "Giving up waiting for client to empty the audio buffer\n");
			break;
		}
	}

	/* Let the clients get the last of the audio stream */
	sanity = 0;
	while ( 3 == libvlc_media_get_state(context->m) ) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "VLC waiting for clients: %d \n", libvlc_media_get_state(context->m));
		switch_yield(500000); 
                if (++sanity > 10) {
                        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "Giving up waiting for client to get the last of the audio stream\n");
                        break;
		}
	}

	if( context->mp ) 
		libvlc_media_player_stop(context->mp);
	
	if( context->m ) 
		libvlc_media_release(context->m);
	
	if ( context->inst_out != NULL )
		libvlc_release(context->inst_out);

	return SWITCH_STATUS_SUCCESS;
}

/* Macro expands to: switch_status_t mod_vlc_load(switch_loadable_module_interface_t **module_interface, switch_memory_pool_t *pool) */
SWITCH_MODULE_LOAD_FUNCTION(mod_vlc_load)
{
	switch_file_interface_t *file_interface;
	
	/* connect my internal structure to the blank pointer passed to me */
	*module_interface = switch_loadable_module_create_module_interface(pool, modname);

	vlc_file_supported_formats[0] = "vlc";

	file_interface = switch_loadable_module_create_interface(*module_interface, SWITCH_FILE_INTERFACE);
	file_interface->interface_name = modname;
	file_interface->extens = vlc_file_supported_formats;
	file_interface->file_open = vlc_file_open;
	file_interface->file_close = vlc_file_close;
	file_interface->file_read = vlc_file_read;
	file_interface->file_write = vlc_file_write;

	/* load the vlc engine. */
	read_inst = libvlc_new(1, &vlc_args);

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Initialized VLC instance\n");

	/* indicate that the module should continue to be loaded */
	return SWITCH_STATUS_SUCCESS;
}

/*
  Called when the system shuts down
  Macro expands to: switch_status_t mod_vlc_shutdown() */
SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_vlc_shutdown)
{
	if ( read_inst != NULL )
		libvlc_release(read_inst);
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
 * vim:set softtabstop=4 shiftwidth=4 tabstop=4 noet
 */
