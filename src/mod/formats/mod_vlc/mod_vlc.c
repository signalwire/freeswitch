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
 * William King william.king@quentustech.com
 * Seven Du <dujinfang@gmail.com>
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
 * To playback a video file
 * play_video /tmp/test.mp4
 *
 * Notes: 
 *
 * Audio Requires at least libvlc version 1.2
 * Video Requires at least libvlc 2.0.2
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

/* Change value to -vvv for vlc related debug. Be careful since vlc is at least as verbose as FS about logging */
const char *vlc_args[] = {""};
// const char *vlc_args[] = {"--network-caching", "500"};

libvlc_instance_t *read_inst;
switch_endpoint_interface_t *vlc_endpoint_interface = NULL;

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
	int channels;
	int err;
	int pts;
	libvlc_instance_t *inst_out;
};

typedef struct vlc_file_context vlc_file_context_t;

struct vlc_video_context {
	libvlc_media_player_t *mp;
	libvlc_media_t *m;
	switch_mutex_t *audio_mutex;
	switch_file_handle_t fh;
	switch_memory_pool_t *pool;
	switch_thread_cond_t *started;
	switch_buffer_t *audio_buffer;
	switch_buffer_t *video_buffer;
	int playing;

	switch_mutex_t *video_mutex;

	switch_core_session_t *session;
	switch_frame_t *aud_frame;
	switch_frame_t *vid_frame;
	uint8_t video_packet[1500 + 12];
	void *raw_yuyv_data;
	void *raw_i420_data;
	uint32_t last_video_ts;
	switch_payload_t pt;
	uint32_t seq;
	int width;
	int height;
	int force_width;
	int force_height;
	int new_frame;          // next frame is a new frame(new timestamp)
};

typedef struct vlc_video_context vlc_video_context_t;

SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_vlc_shutdown);
SWITCH_MODULE_LOAD_FUNCTION(mod_vlc_load);
SWITCH_MODULE_DEFINITION(mod_vlc, mod_vlc_load, mod_vlc_shutdown, NULL);

void yuyv_to_i420(uint8_t *pixels, void *out_buffer, int src_width, int src_height)
{
	uint8_t *Y, *U, *V;
	int h, w;

	Y = out_buffer;
	U = Y + src_width * src_height;
	V = U + (src_width * src_height>>2);

	if (0) {//split
		uint8_t *p, *q, *k;
		p = pixels;
		q = pixels;
		k = pixels;
		for (h = 0; h < src_height / 2; ++h)
		{
			for (w=0; w<src_width; w+=4)
			{
				p[0] = q[0];
				p[1] = q[1];
				p[2] = q[4];
				p[3] = q[3];
				p+=4; q+=8;
			}
			memcpy(p, k, src_width);
			k += src_width * 2;
			p += src_width;
			q += src_width * 2;
		}
		memcpy(k, pixels, src_width * src_height);
	}

	for (h=0; h<src_height; ++h)
	{
		for (w=0; w<src_width; ++w)
		{
			Y[w] = pixels[2 * w];
			if (w % 2 == 0 && h % 2 == 0) {
				U[w / 2] = pixels[2*w + 1];
				V[w / 2] = pixels[2*w + 3];
			}
		}
		pixels = pixels + src_width * 2;
		Y = Y + src_width;
		if ( h % 2 == 0) {
			U = U + (src_width >> 1);
			V = V + (src_width >> 1);
		}
	}
}

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
		if (!switch_buffer_write(context->audio_buffer, samples, count * 2 * context->channels)) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Buffer error\n");
		}
	}

	if(!context->playing ) {
		context->playing = 1;
		switch_thread_cond_signal(context->started);
	}
	switch_mutex_unlock(context->audio_mutex);
}

#define FORCE_SPLIT 1

void vlc_play_audio_callback(void *data, const void *samples, unsigned count, int64_t pts) {
	vlc_video_context_t *context = (vlc_video_context_t *) data;
	switch_size_t bytes;

	switch_mutex_lock(context->audio_mutex);

	bytes = switch_buffer_inuse(context->audio_buffer);
	if ( bytes > VLC_BUFFER_SIZE * 4) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "Buffer overflow %d\n", (int)bytes);
		switch_buffer_toss(context->audio_buffer, bytes - VLC_BUFFER_SIZE);
	}

	switch_buffer_write(context->audio_buffer, samples, count * 2);

	if (!context->playing) {
		context->playing = 1;
		if (context->started) switch_thread_cond_signal(context->started);
	}

	switch_mutex_unlock(context->audio_mutex);

	// switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "VLC callback, play audio: %d \n", count);
}

static void *vlc_video_lock_callback(void *data, void **p_pixels)
{
	vlc_video_context_t *context = (vlc_video_context_t *)data;

	switch_mutex_lock(context->video_mutex);
	if (!context->raw_yuyv_data) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "no yuyv data\n");
		return NULL;
	}
	*p_pixels = context->raw_yuyv_data;

	return NULL; /* picture identifier, not needed here */
}

/* dummy callback so it should be good when no video on channel */
static void vlc_video_unlock_dummy_callback(void *data, void *id, void *const *p_pixels)
{
	vlc_video_context_t *context = (vlc_video_context_t *)data;
	assert(id == NULL); /* picture identifier, not needed here */
	switch_mutex_unlock(context->video_mutex);
}

static void vlc_video_unlock_callback(void *data, void *id, void *const *p_pixels)
{
	vlc_video_context_t *context = (vlc_video_context_t *)data;
	switch_frame_t *frame = context->vid_frame;
	uint32_t decoded_data_len;
	uint32_t flag = 0;
	uint32_t encoded_data_len = 1500;
	uint32_t encoded_rate = 0;
	switch_codec_t *codec = switch_core_session_get_video_write_codec(context->session);

	switch_assert(id == NULL); /* picture identifier, not needed here */
	switch_assert(codec);

	yuyv_to_i420(*p_pixels, context->raw_i420_data, context->width, context->height);

	codec->enc_picture.width = context->width;
	codec->enc_picture.height = context->height;
	decoded_data_len = context->width * context->height * 3 / 2;
	switch_core_codec_encode(codec, NULL, context->raw_i420_data, decoded_data_len, 0, frame->data, &encoded_data_len, &encoded_rate, &flag);

	while(encoded_data_len) {
		// switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "encoded: %s [%d] flag=%d ts=%u\n", codec->implementation->iananame, encoded_data_len, flag, context->last_video_ts);

		frame->datalen = encoded_data_len;
		frame->packetlen = frame->datalen + 12;
		frame->m = flag & SFF_MARKER ? 1 : 0;
		// frame->timestamp = context->ts;
		// if (frame->m) context->ts += 90000 / FPS;
		if (!context->last_video_ts) {
			context->last_video_ts = switch_micro_time_now() / 1000;
			frame->timestamp = context->last_video_ts;
		}

		if (context->new_frame) {
			int delta = switch_micro_time_now() / 1000 - context->last_video_ts;
			frame->timestamp += delta * 90;
			context->last_video_ts = switch_micro_time_now() / 1000;
			context->new_frame = 0;
		}

		if (frame->m) { // next frame is a new frame
			context->new_frame = 1;
		}

		if (1) {
			/* set correct mark and ts */
			switch_rtp_hdr_t *rtp = (switch_rtp_hdr_t *)frame->packet;

			memset(rtp, 0, 12);
			rtp->version = 2;
			rtp->m = frame->m;
			rtp->ts = htonl(frame->timestamp);
			rtp->ssrc = (uint32_t) ((intptr_t) rtp + (uint32_t) switch_epoch_time_now(NULL));

			switch_set_flag(frame, SFF_RAW_RTP);
		}

		switch_set_flag(frame, SFF_RAW_RTP);
		switch_set_flag(frame, SFF_PROXY_PACKET);

		switch_core_session_write_video_frame(context->session, frame, SWITCH_IO_FLAG_NONE, 0);

		encoded_data_len = 1500;
		switch_core_codec_encode(codec, NULL, NULL, 0, 0, frame->data, &encoded_data_len, &encoded_rate, &flag);
	}

	switch_mutex_unlock(context->video_mutex);
}

static void do_buffer_frame(vlc_video_context_t *context)
{
	switch_frame_t *frame = context->vid_frame;
	uint32_t size = sizeof(*frame) + frame->packetlen;

	switch_mutex_lock(context->video_mutex);

	if (switch_buffer_inuse(context->video_buffer) > VLC_BUFFER_SIZE * 10) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "buffer overflow %d\n", (int)switch_buffer_inuse(context->video_buffer));
		switch_buffer_zero(context->video_buffer);
	}
	switch_buffer_write(context->video_buffer, &size, sizeof(uint32_t));
	switch_buffer_write(context->video_buffer, frame, sizeof(*frame));
	switch_buffer_write(context->video_buffer, frame->packet, frame->packetlen);
	switch_mutex_unlock(context->video_mutex);
}

static void vlc_video_channel_unlock_callback(void *data, void *id, void *const *p_pixels)
{
	vlc_video_context_t *context = (vlc_video_context_t *)data;
	uint32_t decoded_data_len;
	uint32_t flag = 0;
	uint32_t encoded_data_len = 1500;
	uint32_t encoded_rate = 0;
	switch_codec_t *codec = switch_core_session_get_video_write_codec(context->session);
	switch_frame_t *frame = context->vid_frame;

	switch_assert(id == NULL); /* picture identifier, not needed here */
	switch_assert(codec);

	yuyv_to_i420(*p_pixels, context->raw_i420_data, context->width, context->height);

	codec->enc_picture.width = context->width;
	codec->enc_picture.height = context->height;
	encoded_data_len = 1500;
	decoded_data_len = context->width * context->height * 3 / 2;

	frame->packet = context->video_packet;
	frame->data = context->video_packet + 12;

	switch_core_codec_encode(codec, NULL, context->raw_i420_data, decoded_data_len, 0, frame->data, &encoded_data_len, &encoded_rate, &flag);

	while(encoded_data_len) {
		// switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "encoded: %s [%d] flag=%d ts=%u\n", codec->implementation->iananame, encoded_data_len, flag, context->ts);

		frame->datalen = encoded_data_len;
		frame->packetlen = frame->datalen + 12;
		frame->m = flag & SFF_MARKER ? 1 : 0;
		if (!context->last_video_ts) {
			context->last_video_ts = switch_micro_time_now() / 1000;
			frame->timestamp = context->last_video_ts;
		}

		if (context->new_frame) {
			int delta = switch_micro_time_now() / 1000 - context->last_video_ts;
			frame->timestamp += delta * 90;
			context->last_video_ts = switch_micro_time_now() / 1000;
			context->new_frame = 0;
		}

		if (frame->m) { // next frame is a new frame
			context->new_frame = 1;
		}

		if (1) {
			/* set correct mark and ts */
			switch_rtp_hdr_t *rtp = (switch_rtp_hdr_t *)frame->packet;

			memset(rtp, 0, 12);
			rtp->version = 2;
			rtp->m = frame->m;
			rtp->ts = htonl(frame->timestamp);
			rtp->ssrc = (uint32_t) ((intptr_t) rtp + (uint32_t) switch_epoch_time_now(NULL));

			switch_set_flag(frame, SFF_RAW_RTP);
		}

		switch_set_flag(frame, SFF_RAW_RTP);
		switch_set_flag(frame, SFF_PROXY_PACKET);

		do_buffer_frame(context);

		encoded_data_len = 1500;
		switch_core_codec_encode(codec, NULL, NULL, 0, 0, frame->data, &encoded_data_len, &encoded_rate, &flag);
	}

	switch_mutex_unlock(context->video_mutex);
}

static void vlc_video_display_callback(void *data, void *id)
{
	/* VLC wants to display the video */
	(void) data;
	assert(id == NULL);
}

unsigned video_format_setup_callback(void **opaque, char *chroma, unsigned *width, unsigned *height, unsigned *pitches, unsigned *lines)
{

	vlc_video_context_t *context = (vlc_video_context_t *) (*opaque);
	unsigned frame_size;

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "chroma: %s, width: %u, height: %u, pitches: %u, lines: %u\n", chroma, *width, *height, *pitches, *lines);

	/* You have to use YUYV here or it will crash */
	switch_set_string(chroma, "YUYV");

	if (context->force_width && context->force_height) { /* resize */
		*width = context->force_width;
		*height = context->force_height;
	}

	*pitches = (*width) * 2;
	*lines = (*height);

	context->width = *width;
	context->height = *height;

	frame_size = (*width) * (*height) * 4 * 2;
	context->raw_yuyv_data = malloc(frame_size);
	if (context->raw_yuyv_data == NULL) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "memory error\n");
		return 0;
	}

	context->raw_i420_data = malloc(frame_size);
	if (context->raw_i420_data == NULL) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "memory error\n");
		free(context->raw_yuyv_data);
		return 0;
	}

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "chroma: %s, width: %u, height: %u, pitches: %u, lines: %u\n", chroma, *width, *height, *pitches, *lines);

	return 1;
}

void video_format_clean_callback(void *opaque)
{

	vlc_video_context_t *context = (vlc_video_context_t *)opaque;
	switch_safe_free(context->raw_yuyv_data);
	switch_safe_free(context->raw_i420_data);
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "cleanup\n");
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
		bytes = samples * 2 * context->channels;
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
		} else if (! strncmp(context->path, "rtp", 3)){
			context->m = libvlc_media_new_path(read_inst, context->path);
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "VLC Path is rtp %s\n", context->path);
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

		if (!handle->samplerate) {
			handle->samplerate = 16000;
		}

		context->samplerate = handle->samplerate;
		context->channels = handle->channels;

		libvlc_audio_set_format(context->mp, "S16N", context->samplerate, handle->channels);
		
		m_event_manager = libvlc_media_event_manager(context->m);
		libvlc_event_attach(m_event_manager, libvlc_MediaStateChanged, vlc_media_state_callback, (void *) context);

		mp_event_manager = libvlc_media_player_event_manager(context->mp);
		libvlc_event_attach(mp_event_manager, libvlc_MediaPlayerEncounteredError, vlc_mediaplayer_error_callback, (void *) context);
		
		libvlc_audio_set_callbacks(context->mp, vlc_auto_play_callback, NULL,NULL,NULL,NULL, (void *) context);

		libvlc_media_player_play(context->mp);

	} else if (switch_test_flag(handle, SWITCH_FILE_FLAG_WRITE)) {
		const char * opts[10] = {
			*vlc_args,
			switch_mprintf("--sout=%s", path)
		};
		int opts_count = 10;
		
		if ( !handle->samplerate)
			handle->samplerate = 16000;
		
		context->samplerate = handle->samplerate;
		context->channels = handle->channels;

		opts[2] = switch_mprintf("--imem-get=%ld", vlc_imem_get_callback);
		opts[3] = switch_mprintf("--imem-release=%ld", vlc_imem_release_callback);
		opts[4] = switch_mprintf("--imem-cat=%d", 4);
		opts[5] = "--demux=rawaud";
		opts[6] = "--rawaud-fourcc=s16l";
		opts[7] = switch_mprintf("--rawaud-samplerate=%d", context->samplerate);
		opts[8] = switch_mprintf("--imem-data=%ld", context);
		//opts[9] = "--rawaud-channels=1";

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
	size_t bytes = *len * sizeof(int16_t) * handle->channels, read;
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


SWITCH_STANDARD_APP(play_video_function)
{
	switch_channel_t *channel = switch_core_session_get_channel(session);
	switch_memory_pool_t *pool = switch_core_session_get_pool(session);
	switch_frame_t audio_frame = { 0 }, video_frame = { 0 };
	switch_codec_t codec = { 0 }, vid_codec = { 0 }, *read_vid_codec;
	switch_timer_t timer = { 0 };
	switch_payload_t pt = 0;
	switch_dtmf_t dtmf = { 0 };
	switch_frame_t *read_frame;
	switch_codec_implementation_t read_impl = { 0 };
	switch_core_session_message_t msg = { 0 };
	vlc_video_context_t *context;
	char *path = (char *)data;
	const char *tmp;

	switch_size_t audio_datalen;

	switch_channel_set_flag(channel, CF_VIDEO_PASSIVE);

	context = switch_core_session_alloc(session, sizeof(vlc_video_context_t));
	switch_assert(context);
	memset(context, 0, sizeof(vlc_file_context_t));

	if ((tmp = switch_channel_get_variable(channel, "vlc_force_width"))) {
		context->force_width = atoi(tmp);
	}

	if ((tmp = switch_channel_get_variable(channel, "vlc_force_height"))) {
		context->force_height = atoi(tmp);
	}

	switch_buffer_create_dynamic(&(context->audio_buffer), VLC_BUFFER_SIZE, VLC_BUFFER_SIZE * 8, 0);

/*
	if (0) { // SPS PPS
		unsigned char outbuf[1024] = { 0 };
		unsigned char *in, *out = outbuf;
		switch_size_t ilen, olen = 0;

		sprintf((char *)out, "packetization-mode=1;profile-level-id=42c01f;sprop-parameter-sets=");
		out += strlen((char *)out);

		for (int i = 0; i < nal_count; ++i) {
			switch (nals[i].i_type) {
				case NAL_SPS:
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "sps %d: %d %d %d %d %x\n", nals[i].i_payload, nals[i].p_payload[0], nals[i].p_payload[1], nals[i].p_payload[2], nals[i].p_payload[3], nals[i].p_payload[4]);
					in = nals[i].p_payload + 4; // skipe 00 00 00 01
					ilen = nals[i].i_payload - 4;
					switch_b64_encode(in, ilen, out, 512);
					break;
				case  NAL_PPS:
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "pps %d: %d %d %d %d %x\n", nals[i].i_payload, nals[i].p_payload[0], nals[i].p_payload[1], nals[i].p_payload[2], nals[i].p_payload[3], nals[i].p_payload[4]);
					in = nals[i].p_payload + 4; // skip 00 00 00 01
					ilen = nals[i].i_payload - 4;
					olen = (switch_size_t)strlen((char *)out);
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "%d\n", 512);

					if (olen > 0) {
						out += olen;
						*out++ = ',';
					}
					switch_b64_encode(in, ilen, out, olen);
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "props:%s\n", outbuf);
					break;
				default:
					break;
			}
		}

		switch_channel_set_variable(channel, "sip_force_video_fmtp", (char *)outbuf);
	}
*/

	switch_channel_answer(channel);
	switch_core_session_get_read_impl(session, &read_impl);
	switch_core_session_receive_message(session, &msg);

	if ((read_vid_codec = switch_core_session_get_video_read_codec(session))) {
		pt = read_vid_codec->agreed_pt;
	}

	context->pt = pt;
	audio_frame.codec = &codec;
	video_frame.codec = read_vid_codec;
	video_frame.packet = context->video_packet;
	video_frame.data = context->video_packet + 12;

	switch_channel_set_variable(channel, SWITCH_PLAYBACK_TERMINATOR_USED, "");

	if (switch_core_timer_init(&timer, "soft", read_impl.microseconds_per_packet / 1000,
							   read_impl.samples_per_packet, pool) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "Timer Activation Fail\n");
		switch_channel_set_variable(channel, SWITCH_CURRENT_APPLICATION_RESPONSE_VARIABLE, "Timer activation failed!");
		goto end;
	}

	if (switch_core_codec_init(&codec,
							   "L16",
							   NULL,
							   read_impl.actual_samples_per_second,
							   read_impl.microseconds_per_packet/1000,
							   1, SWITCH_CODEC_FLAG_ENCODE | SWITCH_CODEC_FLAG_DECODE,
							   NULL, switch_core_session_get_pool(session)) == SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "Audio Codec Activation Success\n");
	} else {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "Audio Codec Activation Fail\n");
		switch_channel_set_variable(channel, SWITCH_CURRENT_APPLICATION_RESPONSE_VARIABLE, "Audio codec activation failed");
		goto end;
	}

	if (switch_core_codec_init(&vid_codec,
							   "H264",
							   NULL,
							   0,
							   0,
							   1, SWITCH_CODEC_FLAG_ENCODE | SWITCH_CODEC_FLAG_DECODE,
							   NULL, switch_core_session_get_pool(session)) == SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "Video Codec Activation Success\n");
	} else {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "Video Codec Activation Fail\n");
		switch_channel_set_variable(channel, SWITCH_CURRENT_APPLICATION_RESPONSE_VARIABLE, "Video codec activation failed");
		goto end;
	}

	audio_datalen = codec.implementation->actual_samples_per_second / 1000 * (read_impl.microseconds_per_packet / 1000);

	context->session = session;
	context->pool = pool;
	context->aud_frame = &audio_frame;
	context->vid_frame = &video_frame;
	context->playing = 0;
	// context->err = 0;

	switch_mutex_init(&context->audio_mutex, SWITCH_MUTEX_NESTED, context->pool);
	switch_mutex_init(&context->video_mutex, SWITCH_MUTEX_NESTED, context->pool);

	switch_thread_cond_create(&(context->started), context->pool);

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "VLC open %s for reading\n", path);

	/* Determine if this is a url or a path */
	/* TODO: Change this so that it tries local files first, and then if it fails try location. */
	if(! strncmp(path, "http", 4)){
		context->m = libvlc_media_new_location(read_inst, path);
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "VLC Path is http %s\n", path);
	} else if (! strncmp(path, "rtp", 3)){
		context->m = libvlc_media_new_path(read_inst, path);
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "VLC Path is rtp %s\n", path);
	} else if (! strncmp(path, "mms", 3)){
		context->m = libvlc_media_new_path(read_inst, path);
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "VLC Path is mms %s\n", path);
	} else if (! strncmp(path, "rtsp", 3)){
		context->m = libvlc_media_new_path(read_inst, path);
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "VLC Path is rtsp %s\n", path);
	} else if (! strncmp(path, "/", 1)){
		context->m = libvlc_media_new_path(read_inst, path);
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "VLC Path is file %s\n", path);
	} else {
		context->m = libvlc_media_new_location(read_inst, path);
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "VLC Path is unknown type %s\n", path);
	}

	if ( context->m == NULL ) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "VLC error opening %s for reading\n", data);
		switch_channel_hangup(channel, SWITCH_CAUSE_DESTINATION_OUT_OF_ORDER);
		return ;
	}

	context->mp = libvlc_media_player_new_from_media(context->m);

	libvlc_audio_set_format(context->mp, "S16N", read_impl.actual_samples_per_second, 1);
	libvlc_audio_set_callbacks(context->mp, vlc_play_audio_callback, NULL,NULL,NULL,NULL, (void *) context);

	if (switch_channel_test_flag(channel, CF_VIDEO)) {
		// libvlc_video_set_format(context->mp, "YUYV", VIDEOWIDTH, VIDEOHEIGHT, VIDEOWIDTH * 2);
		libvlc_video_set_format_callbacks(context->mp, video_format_setup_callback, video_format_clean_callback);
		libvlc_video_set_callbacks(context->mp, vlc_video_lock_callback, vlc_video_unlock_callback, vlc_video_display_callback, context);
	} else {
		libvlc_video_set_format_callbacks(context->mp, video_format_setup_callback, video_format_clean_callback);
		libvlc_video_set_callbacks(context->mp, vlc_video_lock_callback, vlc_video_unlock_dummy_callback, vlc_video_display_callback, context);
	}

	// start play
	if (-1 == libvlc_media_player_play(context->mp)) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "VLC error playing %s\n", path);
		switch_channel_hangup(channel, SWITCH_CAUSE_DESTINATION_OUT_OF_ORDER);
	};

	// switch_core_service_session_av(session, SWITCH_FALSE, SWITCH_TRUE);

	while (switch_channel_ready(channel)) {

		switch_core_timer_next(&timer);

		switch_core_session_read_frame(session, &read_frame, SWITCH_IO_FLAG_NONE, 0);

		if (switch_channel_test_flag(channel, CF_BREAK)) {
			switch_channel_clear_flag(channel, CF_BREAK);
			break;
		}

		switch_ivr_parse_all_events(session);

		//check for dtmf interrupts
		if (switch_channel_has_dtmf(channel)) {
			const char * terminators = switch_channel_get_variable(channel, SWITCH_PLAYBACK_TERMINATORS_VARIABLE);
			switch_channel_dequeue_dtmf(channel, &dtmf);

			if (terminators && !strcasecmp(terminators, "none"))
			{
				terminators = NULL;
			}

			if (terminators && strchr(terminators, dtmf.digit)) {

				char sbuf[2] = {dtmf.digit, '\0'};
				switch_channel_set_variable(channel, SWITCH_PLAYBACK_TERMINATOR_USED, sbuf);
				break;
			}

		}

		{
			libvlc_state_t status = libvlc_media_get_state(context->m);
			if (status == libvlc_Ended || status == libvlc_Error || status == libvlc_Stopped ) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "VLC done. status = %d\n", status);
				break;
			}
		}

		if (switch_buffer_inuse(context->audio_buffer) >= audio_datalen * 2) {
			const void *decoded_data;
			// switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "%d %d\n", (int)switch_buffer_inuse(context->audio_buffer), (int)audio_datalen * 2);
			switch_buffer_peek_zerocopy(context->audio_buffer, &decoded_data);
			audio_frame.data = (void *)decoded_data;
			audio_frame.datalen = audio_datalen*2;
			audio_frame.buflen = audio_datalen*2;
			switch_core_session_write_frame(context->session, &audio_frame, SWITCH_IO_FLAG_NONE, 0);
			switch_buffer_toss(context->audio_buffer, audio_datalen * 2);
		}

	}

	switch_yield(50000);

	if( context->mp ) libvlc_media_player_stop(context->mp);
	if( context->m ) libvlc_media_release(context->m);

	context->playing = 0;

	switch_core_thread_session_end(session);
	switch_channel_set_variable(channel, SWITCH_CURRENT_APPLICATION_RESPONSE_VARIABLE, "OK");

end:

	if (context->audio_buffer) {
		switch_buffer_destroy(&context->audio_buffer);
	}

	if (timer.interval) {
		switch_core_timer_destroy(&timer);
	}

	if (switch_core_codec_ready(&codec)) {
		switch_core_codec_destroy(&codec);
	}

	if (switch_core_codec_ready(&vid_codec)) {
		switch_core_codec_destroy(&vid_codec);
	}

	switch_channel_clear_flag(channel, CF_VIDEO_PASSIVE);
}

static switch_status_t channel_on_init(switch_core_session_t *session);
static switch_status_t channel_on_consume_media(switch_core_session_t *session);
static switch_status_t channel_on_destroy(switch_core_session_t *session);

static switch_call_cause_t vlc_outgoing_channel(switch_core_session_t *session, switch_event_t *var_event,
													switch_caller_profile_t *outbound_profile,
													switch_core_session_t **new_session,
													switch_memory_pool_t **pool,
													switch_originate_flag_t flags, switch_call_cause_t *cancel_cause);
static switch_status_t vlc_read_frame(switch_core_session_t *session, switch_frame_t **frame, switch_io_flag_t flags, int stream_id);
static switch_status_t vlc_write_frame(switch_core_session_t *session, switch_frame_t *frame, switch_io_flag_t flags, int stream_id);
static switch_status_t vlc_read_video_frame(switch_core_session_t *session, switch_frame_t **frame, switch_io_flag_t flags, int stream_id);
static switch_status_t vlc_receive_message(switch_core_session_t *session, switch_core_session_message_t *msg);
static switch_status_t vlc_kill_channel(switch_core_session_t *session, int sig);

typedef struct {
    switch_core_session_t *session;
    switch_channel_t *channel;
    switch_codec_t read_codec, write_codec;
    switch_codec_t video_codec;
    switch_frame_t read_frame;
    switch_frame_t read_video_frame;
    void *audio_data[SWITCH_RECOMMENDED_BUFFER_SIZE];
    void *video_data[SWITCH_RECOMMENDED_BUFFER_SIZE * 20];
    const char *destination_number;
    vlc_video_context_t *context;
    switch_timer_t timer;
} vlc_private_t;

switch_state_handler_table_t vlc_state_handlers = {
	/*on_init */ channel_on_init,
	/*on_routing */ NULL,
	/*on_execute */ NULL,
	/*on_hangup*/ NULL,
	/*on_exchange_media*/ NULL,
	/*on_soft_execute*/ NULL,
	/*on_consume_media*/ channel_on_consume_media,
	/*on_hibernate*/ NULL,
	/*on_reset*/ NULL,
	/*on_park*/ NULL,
	/*on_reporting*/ NULL,
	/*on_destroy*/ channel_on_destroy
};

switch_io_routines_t vlc_io_routines = {
	/*outgoing_channel*/ vlc_outgoing_channel,
	/*read_frame*/ vlc_read_frame,
	/*write_frame*/ vlc_write_frame,
	/*kill_channel*/ vlc_kill_channel,
	/*send_dtmf*/ NULL,
	/*receive_message*/ vlc_receive_message,
	/*receive_event*/ NULL,
	/*state_change*/ NULL,
	/*read_video_frame*/ vlc_read_video_frame,
	/*write_video_frame*/ NULL,
	/*state_run*/ NULL
};

static switch_status_t setup_tech_pvt(switch_core_session_t *session, const char *path)
{
	switch_channel_t *channel = switch_core_session_get_channel(session);
	switch_memory_pool_t *pool = switch_core_session_get_pool(session);
	// switch_dtmf_t dtmf = { 0 };
	vlc_video_context_t *context;
	vlc_private_t *tech_pvt;
	switch_status_t status = SWITCH_STATUS_SUCCESS;

	tech_pvt = switch_core_session_alloc(session, sizeof *tech_pvt);
	switch_assert(tech_pvt);
	tech_pvt->session = session;
	tech_pvt->channel = channel;
	tech_pvt->destination_number = switch_core_session_strdup(session, path);
	switch_core_session_set_private(session, tech_pvt);

	context = switch_core_session_alloc(session, sizeof(vlc_video_context_t));
	switch_assert(context);
	memset(context, 0, sizeof(vlc_file_context_t));
	tech_pvt->context = context;

	switch_buffer_create_dynamic(&(context->audio_buffer), VLC_BUFFER_SIZE, VLC_BUFFER_SIZE * 8, 0);

	if (switch_channel_test_flag(channel, CF_VIDEO)) {
		switch_buffer_create_dynamic(&(context->video_buffer), VLC_BUFFER_SIZE * 2, VLC_BUFFER_SIZE * 16, 0);
	}

	if (switch_core_timer_init(&tech_pvt->timer, "soft", 20,
							   8000 / (1000 / 20), pool) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "Timer Activation Fail\n");
		status = SWITCH_STATUS_FALSE;
		goto fail;
	}

	context->session = session;
	context->pool = pool;
	context->aud_frame = &tech_pvt->read_frame;
	context->vid_frame = &tech_pvt->read_video_frame;
	context->playing = 0;
	// context->err = 0;

	switch_mutex_init(&context->audio_mutex, SWITCH_MUTEX_NESTED, context->pool);
	switch_mutex_init(&context->video_mutex, SWITCH_MUTEX_NESTED, context->pool);

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "VLC open %s for reading\n", path);

	/* Determine if this is a url or a path */
	/* TODO: Change this so that it tries local files first, and then if it fails try location. */
	if(! strncmp(path, "http", 4)){
		context->m = libvlc_media_new_location(read_inst, path);
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "VLC Path is http %s\n", path);
	} else if (! strncmp(path, "rtp", 3)){
		context->m = libvlc_media_new_path(read_inst, path);
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "VLC Path is rtp %s\n", path);
	} else if (! strncmp(path, "mms", 3)){
		context->m = libvlc_media_new_path(read_inst, path);
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "VLC Path is mms %s\n", path);
	} else if (! strncmp(path, "rtsp", 3)){
		context->m = libvlc_media_new_path(read_inst, path);
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "VLC Path is rtsp %s\n", path);
	} else if (! strncmp(path, "/", 1)){
		context->m = libvlc_media_new_path(read_inst, path);
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "VLC Path is file %s\n", path);
	} else {
		context->m = libvlc_media_new_location(read_inst, path);
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "VLC Path is unknown type %s\n", path);
	}

	if ( context->m == NULL ) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "VLC error opening %s for reading\n", path);
		switch_channel_hangup(channel, SWITCH_CAUSE_DESTINATION_OUT_OF_ORDER);
		goto fail;
	}

	context->mp = libvlc_media_player_new_from_media(context->m);

	libvlc_audio_set_format(context->mp, "S16N", 8000, 1);
	libvlc_audio_set_callbacks(context->mp, vlc_play_audio_callback, NULL,NULL,NULL,NULL, (void *) context);

	if (switch_channel_test_flag(channel, CF_VIDEO)) {
		// libvlc_video_set_format(context->mp, "YUYV", VIDEOWIDTH, VIDEOHEIGHT, VIDEOWIDTH * 2);
		libvlc_video_set_format_callbacks(context->mp, video_format_setup_callback, video_format_clean_callback);
		libvlc_video_set_callbacks(context->mp, vlc_video_lock_callback, vlc_video_channel_unlock_callback, vlc_video_display_callback, context);
	} else {
		libvlc_video_set_format_callbacks(context->mp, video_format_setup_callback, video_format_clean_callback);
		libvlc_video_set_callbacks(context->mp, vlc_video_lock_callback, vlc_video_unlock_dummy_callback, vlc_video_display_callback, context);
	}

	return SWITCH_STATUS_SUCCESS;

fail:

	return SWITCH_STATUS_FALSE;
}

static switch_status_t channel_on_init(switch_core_session_t *session)
{
	switch_channel_t *channel = switch_core_session_get_channel(session);

	switch_channel_set_state(channel, CS_CONSUME_MEDIA);

	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t channel_on_consume_media(switch_core_session_t *session)
{
	// switch_channel_t *channel = switch_core_session_get_channel(session);
	vlc_private_t *tech_pvt = switch_core_session_get_private(session);

	// return SWITCH_STATUS_SUCCESS;

	switch_assert(tech_pvt && tech_pvt->context);

	// // start play
	// if (-1 == libvlc_media_player_play(tech_pvt->context->mp)) {
	// 	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "VLC error playing %s\n", tech_pvt->destination_number);
	// 	return SWITCH_STATUS_FALSE;
	// };

	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t channel_on_destroy(switch_core_session_t *session)
{
	vlc_private_t *tech_pvt = switch_core_session_get_private(session);

	switch_assert(tech_pvt && tech_pvt->context);

	if ((tech_pvt = switch_core_session_get_private(session))) {

		if (tech_pvt->read_codec.implementation) {
			switch_core_codec_destroy(&tech_pvt->read_codec);
		}

		if (tech_pvt->write_codec.implementation) {
			switch_core_codec_destroy(&tech_pvt->write_codec);
		}
	}

	switch_yield(50000);

	if (tech_pvt->context->mp) libvlc_media_player_stop(tech_pvt->context->mp);
	if (tech_pvt->context->m) libvlc_media_release(tech_pvt->context->m);

	tech_pvt->context->playing = 0;

	if (tech_pvt->context->audio_buffer) {
		switch_buffer_destroy(&tech_pvt->context->audio_buffer);
	}

	if (tech_pvt->timer.interval) {
		switch_core_timer_destroy(&tech_pvt->timer);
	}

	return SWITCH_STATUS_SUCCESS;
}

static switch_call_cause_t vlc_outgoing_channel(switch_core_session_t *session, switch_event_t *var_event,
													switch_caller_profile_t *outbound_profile,
													switch_core_session_t **new_session,
													switch_memory_pool_t **pool,
													switch_originate_flag_t flags, switch_call_cause_t *cancel_cause)
{
	switch_channel_t *channel;
	char name[256];
	vlc_private_t *tech_pvt = NULL;
	switch_caller_profile_t *caller_profile;
	// const char *err;

	switch_assert(vlc_endpoint_interface);

	if (session) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "session: %s\n", switch_core_session_get_uuid(session));
	}

	if (!(*new_session = switch_core_session_request(vlc_endpoint_interface, SWITCH_CALL_DIRECTION_OUTBOUND, 0, pool))) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Couldn't request session.\n");
		goto fail;
	}

	channel = switch_core_session_get_channel(*new_session);
	snprintf(name, sizeof(name), "vlc/%s", outbound_profile->destination_number);
	switch_channel_set_name(channel, name);
	switch_channel_set_flag(channel, CF_VIDEO);

	if (setup_tech_pvt(*new_session, outbound_profile->destination_number) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Error steup tech_pvt!\n");
		goto fail;
	}

	tech_pvt = switch_core_session_get_private(*new_session);
	caller_profile = switch_caller_profile_clone(*new_session, outbound_profile);
	switch_channel_set_caller_profile(channel, caller_profile);


	if (switch_core_codec_init(&tech_pvt->read_codec,
				"L16",
				NULL,
				8000,
				20,
				1,
				/*SWITCH_CODEC_FLAG_ENCODE |*/ SWITCH_CODEC_FLAG_DECODE,
				NULL, switch_core_session_get_pool(tech_pvt->session)) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Can't load codec?\n");
		goto fail;
	}

	if (switch_core_codec_init(&tech_pvt->write_codec,
				"L16",
				NULL,
				8000,
				20,
				1,
				SWITCH_CODEC_FLAG_ENCODE /*| SWITCH_CODEC_FLAG_DECODE*/,
				NULL, switch_core_session_get_pool(tech_pvt->session)) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Can't load codec?\n");
		goto fail;
	}

	if (switch_core_codec_init(&tech_pvt->video_codec,
				"H264",
				NULL,
				90000,
				0,
				1, SWITCH_CODEC_FLAG_ENCODE | SWITCH_CODEC_FLAG_DECODE,
				NULL, switch_core_session_get_pool(tech_pvt->session)) == SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(*new_session), SWITCH_LOG_DEBUG, "Video Codec Activation Success\n");
	} else {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(*new_session), SWITCH_LOG_ERROR, "Video Codec Activation Fail\n");
		goto fail;
	}

	if (switch_core_session_set_read_codec(*new_session, &tech_pvt->read_codec) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Can't set read codec?\n");
		goto fail;
	}

	if (switch_core_session_set_write_codec(*new_session, &tech_pvt->write_codec) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Can't set write codec?\n");
		goto fail;
	}

	if (switch_core_session_set_video_read_codec(*new_session, &tech_pvt->video_codec) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Can't set read codec?\n");
		goto fail;
	}

	if (switch_core_session_set_video_write_codec(*new_session, &tech_pvt->video_codec) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Can't set write codec?\n");
		goto fail;
	}

	switch_channel_set_state(channel, CS_INIT);

	if (switch_core_session_thread_launch(*new_session) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Couldn't start session thread.\n");
		goto fail;
	}

	switch_channel_mark_answered(channel);

	// start play
	if (-1 == libvlc_media_player_play(tech_pvt->context->mp)) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "VLC error playing %s\n", tech_pvt->destination_number);
		goto fail;
	};

	return SWITCH_CAUSE_SUCCESS;

fail:
	if (tech_pvt) {
		if (tech_pvt->read_codec.implementation) {
			switch_core_codec_destroy(&tech_pvt->read_codec);
		}

		if (tech_pvt->write_codec.implementation) {
			switch_core_codec_destroy(&tech_pvt->write_codec);
		}

		if (tech_pvt->video_codec.implementation) {
			switch_core_codec_destroy(&tech_pvt->video_codec);
		}
	}

	if (*new_session) {
		switch_core_session_destroy(new_session);
	}

	return SWITCH_CAUSE_DESTINATION_OUT_OF_ORDER;
}

static switch_status_t vlc_read_frame(switch_core_session_t *session, switch_frame_t **frame, switch_io_flag_t flags, int stream_id)
{
	vlc_private_t *tech_pvt;
	switch_channel_t *channel;
	switch_status_t status = SWITCH_STATUS_SUCCESS;
	libvlc_state_t vlc_status;
	vlc_video_context_t *context;
	switch_size_t audio_datalen = 160;

	channel = switch_core_session_get_channel(session);
	assert(channel != NULL);

	tech_pvt = switch_core_session_get_private(session);
	assert(tech_pvt != NULL);

	switch_yield(20000);

	// goto cng;

	context = tech_pvt->context;
	assert(context);

	vlc_status = libvlc_media_get_state(context->m);

	if (vlc_status == libvlc_Ended || vlc_status == libvlc_Error || vlc_status == libvlc_Stopped ) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "VLC done. status = %d\n", status);
		switch_channel_hangup(channel, SWITCH_CAUSE_SUCCESS);
		return SWITCH_STATUS_SUCCESS;
	}

	switch_mutex_lock(context->audio_mutex);

	if (switch_buffer_inuse(context->audio_buffer) >= audio_datalen * 2) {
		tech_pvt->read_frame.data = tech_pvt->audio_data;
		// switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "%d %d\n", (int)switch_buffer_inuse(context->audio_buffer), (int)audio_datalen * 2);
		switch_buffer_read(context->audio_buffer, tech_pvt->read_frame.data, audio_datalen * 2);
		tech_pvt->read_frame.datalen = audio_datalen * 2;
		tech_pvt->read_frame.buflen = audio_datalen * 2;
		tech_pvt->read_frame.flags &= ~SFF_CNG;
		tech_pvt->read_frame.codec = &tech_pvt->read_codec;
		*frame = &tech_pvt->read_frame;
		switch_mutex_unlock(context->audio_mutex);
		return SWITCH_STATUS_SUCCESS;
	}

	switch_mutex_unlock(context->audio_mutex);
	goto cng;

cng:
	*frame = &tech_pvt->read_frame;
	tech_pvt->read_frame.codec = &tech_pvt->read_codec;
	tech_pvt->read_frame.flags |= SFF_CNG;
	tech_pvt->read_frame.datalen = 0;

	// switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "read cng frame\n");
	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t vlc_write_frame(switch_core_session_t *session, switch_frame_t *frame, switch_io_flag_t flags, int stream_id)
{
	return	SWITCH_STATUS_SUCCESS;
}

static switch_status_t vlc_read_video_frame(switch_core_session_t *session, switch_frame_t **frame, switch_io_flag_t flags, int stream_id)
{
	switch_channel_t *channel;
	vlc_private_t *tech_pvt;
	vlc_video_context_t *context;
	uint32_t size;

	channel = switch_core_session_get_channel(session);
	switch_assert(channel != NULL);

	tech_pvt = switch_core_session_get_private(session);
	switch_assert(tech_pvt != NULL);

	context = tech_pvt->context;
	switch_assert(tech_pvt->context);

	switch_mutex_lock(context->video_mutex);

	while (switch_buffer_inuse(context->video_buffer) < sizeof(uint32_t)) {
		switch_mutex_unlock(context->video_mutex);
		switch_yield(10000);
		// switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "waiting for video\n");
		switch_mutex_lock(context->video_mutex);
	}

	switch_buffer_read(context->video_buffer, &size, sizeof(uint32_t));

	if (size == 0) {
		goto unlock;
	}

	if (switch_buffer_inuse(context->video_buffer) < sizeof(switch_frame_t)) {
		goto unlock;
	}

	switch_buffer_read(context->video_buffer, &tech_pvt->read_video_frame, sizeof(switch_frame_t));

	if (switch_buffer_inuse(context->video_buffer) < tech_pvt->read_video_frame.packetlen) {
		goto unlock;
	}

	switch_buffer_read(context->video_buffer, tech_pvt->video_data, tech_pvt->read_video_frame.packetlen);

	tech_pvt->read_video_frame.packet = tech_pvt->video_data;
	tech_pvt->read_video_frame.data = tech_pvt->video_data + 12;

	switch_mutex_unlock(context->video_mutex);

	*frame = &tech_pvt->read_video_frame;
	switch_set_flag(*frame, SFF_RAW_RTP);
	switch_clear_flag(*frame, SFF_CNG);

	// switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "read video %d\n", (*frame)->packetlen);
	return SWITCH_STATUS_SUCCESS;

	goto cng;

unlock:

	switch_mutex_unlock(context->video_mutex);

cng:
	*frame = &tech_pvt->read_frame;
	tech_pvt->read_frame.codec = &tech_pvt->read_codec;
	tech_pvt->read_frame.flags |= SFF_CNG;
	tech_pvt->read_frame.datalen = 0;

	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t vlc_receive_message(switch_core_session_t *session, switch_core_session_message_t *msg)
{
	vlc_private_t *tech_pvt = switch_core_session_get_private(session);

	assert(tech_pvt != NULL);

	switch (msg->message_id) {
		case SWITCH_MESSAGE_INDICATE_DEBUG_MEDIA:
			break;
		case SWITCH_MESSAGE_INDICATE_AUDIO_SYNC:
			break;
		case SWITCH_MESSAGE_INDICATE_JITTER_BUFFER:
			break;
		default:
			break;
	}
	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t vlc_kill_channel(switch_core_session_t *session, int sig)
{
	vlc_private_t *tech_pvt = switch_core_session_get_private(session);
	switch_channel_t *channel = switch_core_session_get_channel(session);

	if (!tech_pvt) return SWITCH_STATUS_FALSE;

	if (!tech_pvt->context) return SWITCH_STATUS_FALSE;

	switch (sig) {
	case SWITCH_SIG_BREAK:
	case SWITCH_SIG_KILL:
		if (switch_channel_test_flag(channel, CF_VIDEO)) {
			uint32_t size = 0;
			switch_buffer_write(tech_pvt->context->video_buffer, &size, sizeof(size));
		}
		break;
	default:
		break;
	}

	return SWITCH_STATUS_SUCCESS;
}

/* Macro expands to: switch_status_t mod_vlc_load(switch_loadable_module_interface_t **module_interface, switch_memory_pool_t *pool) */
SWITCH_MODULE_LOAD_FUNCTION(mod_vlc_load)
{
	switch_file_interface_t *file_interface;
	switch_application_interface_t *app_interface;

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

	vlc_endpoint_interface = switch_loadable_module_create_interface(*module_interface, SWITCH_ENDPOINT_INTERFACE);
	vlc_endpoint_interface->interface_name = "vlc";
	vlc_endpoint_interface->io_routines = &vlc_io_routines;
	vlc_endpoint_interface->state_handler = &vlc_state_handlers;

	/* load the vlc engine. */
	read_inst = libvlc_new(sizeof(vlc_args)/sizeof(char *), vlc_args);

	if ( ! read_inst ) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "FAILED TO LOAD\n");
			return SWITCH_STATUS_GENERR;
	}

	SWITCH_ADD_APP(app_interface, "play_video", "play an videofile", "play an video file", play_video_function, "<file>", SAF_SUPPORT_NOMEDIA);

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
