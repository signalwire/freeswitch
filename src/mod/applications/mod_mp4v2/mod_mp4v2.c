/*
 * FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
 * Copyright (C) 2005-2015, Anthony Minessale II <anthm@freeswitch.org>
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
 * Seven Du <dujinfang@gmail.com>
 * Portions created by the Initial Developer are Copyright (C)
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 *
 * mod_mp4v2 -- MP4 File Format for FreeSWITCH
 *
 *
 * status: For write, codec is hard coded to PCMU for audio and H264 for video
 *         tested with lib mp4v2-2.0.0
 *         Read from mp4 is not supported yet.
 */

#include <switch.h>

#include <mp4v2/mp4v2.h>

#define TIMESCALE 1000
#define SampleLenFieldSize 4
#define PTIME 20
// #define HAS_SPS_PARSER

SWITCH_MODULE_LOAD_FUNCTION(mod_mp4v2_load);
SWITCH_MODULE_DEFINITION(mod_mp4v2, mod_mp4v2_load, NULL, NULL);

struct record_helper {
	switch_core_session_t *session;
	switch_mutex_t *mutex;
	MP4FileHandle fd;
	MP4TrackId video_track;
	MP4TrackId audio_track;
	switch_timer_t timer;
	int up;
	uint64_t last_pts;
};

#ifdef HAS_SPS_PARSER
#include "bs.h"
static void parse_sps_video_size(uint8_t *sps_buffer, int len, int *width, int *height)
{
	sps_t sps = { 0 };
	bs_t b = { 0 };

	bs_init(&b, sps_buffer, len);
	read_sps(&sps, &b);

	*width = ((sps.pic_width_in_mbs_minus1 +1)*16) - sps.frame_crop_left_offset*2 - sps.frame_crop_right_offset*2;
	*height= ((2 - sps.frame_mbs_only_flag)* (sps.pic_height_in_map_units_minus1 +1) * 16) - (sps.frame_crop_top_offset * 2) - (sps.frame_crop_bottom_offset * 2);

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "H264 Profile: %d size: %dx%d\n", sps.profile_idc, *width, *height);
}
#else
// use hardcoded value
static void parse_sps_video_size(uint8_t *sps_buffer, int len, int *width, int *height)
{
	*width = 1280;
	*height = 720;
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "We have no idea about the video size without decoding the video or actually parse the SPS, using hardcoded %dx%d\n", *width, *height);
}
#endif

static void init_video_track(MP4FileHandle mp4, MP4TrackId *video, switch_frame_t *frame)
{
	int width = 0;
	int height = 0;
	uint8_t *sps_buffer = frame->data;
	uint32_t sps_bytes = frame->datalen;

	sps_buffer++;

	if (frame->img) {
		width = frame->img->d_w;
		height = frame->img->d_h;
	} else {
		parse_sps_video_size(sps_buffer, sps_bytes, &width, &height);
	}

	MP4SetTimeScale(mp4, TIMESCALE);

	*video = MP4AddH264VideoTrack(mp4, TIMESCALE, MP4_INVALID_DURATION, width, height, *(sps_buffer), *(sps_buffer+1), *(sps_buffer+2), SampleLenFieldSize - 1);

	if (*video == MP4_INVALID_TRACK_ID) {
		return;
	}

	MP4AddH264SequenceParameterSet(mp4, *video, --sps_buffer, sps_bytes);

	/*
	MP4SetVideoProfileLevel sets the minumum profile/level of MPEG-4 video support necessary to render the contents of the file.

	ISO/IEC 14496-1:2001 MPEG-4 Systems defines the following values:
	0x00 Reserved
	0x01 Simple Profile @ Level 3
	0x02 Simple Profile @ Level 2
	0x03 Simple Profile @ Level 1
	0x04 Simple Scalable Profile @ Level 2
	0x05 Simple Scalable Profile @ Level 1
	0x06 Core Profile @ Level 2
	0x07 Core Profile @ Level 1
	0x08 Main Profile @ Level 4
	0x09 Main Profile @ Level 3
	0x0A Main Profile @ Level 2
	0x0B N-Bit Profile @ Level 2
	0x0C Hybrid Profile @ Level 2
	0x0D Hybrid Profile @ Level 1
	0x0E Basic Animated Texture @ Level 2
	0x0F Basic Animated Texture @ Level 1
	0x10 Scalable Texture @ Level 3
	0x11 Scalable Texture @ Level 2
	0x12 Scalable Texture @ Level 1
	0x13 Simple Face Animation @ Level 2
	0x14 Simple Face Animation @ Level 1
	0x15-0x7F Reserved
	0x80-0xFD User private
	0xFE No audio profile specified
	0xFF No audio required
	*/
	MP4SetVideoProfileLevel(mp4, 0x7F);
}

static inline char *get_audio_codec_name(uint8_t audio_type)
{
	switch (audio_type) {
		case MP4_MP3_AUDIO_TYPE:
			return "MP3";
		case MP4_ULAW_AUDIO_TYPE:
			return "PCMU";
		case MP4_PCM16_LITTLE_ENDIAN_AUDIO_TYPE:
			return "L16";
		case MP4_MPEG4_AUDIO_TYPE:
			return "AAC";
		default:
			return "ERROR";
	}
}

static int get_aac_sample_rate_index(unsigned int sampleRate)
{
    if (92017 <= sampleRate) return 0;
    if (75132 <= sampleRate) return 1;
    if (55426 <= sampleRate) return 2;
    if (46009 <= sampleRate) return 3;
    if (37566 <= sampleRate) return 4;
    if (27713 <= sampleRate) return 5;
    if (23004 <= sampleRate) return 6;
    if (18783 <= sampleRate) return 7;
    if (13856 <= sampleRate) return 8;
    if (11502 <= sampleRate) return 9;
    if (9391 <= sampleRate) return 10;

    return 11;
}

struct mp4_file_context {
	switch_file_handle_t *handle;
	switch_memory_pool_t *pool;
	MP4FileHandle fd;
	MP4TrackId audio;
	MP4TrackId video;
	uint32_t audio_frame_size;
	switch_codec_t audio_codec;
	switch_codec_t video_codec;
	switch_mutex_t *mutex;
	switch_buffer_t *buf;
	uint32_t last_chunk_size;
	int sps_set;
	int pps_set;
	switch_timer_t timer;
	uint64_t last_pts;
	int offset;
	int audio_start;
	uint8_t audio_type; // MP4 Audio Type
	MP4Duration audio_duration;
	switch_thread_t *video_thread;
	switch_queue_t *video_queue;
};

typedef struct mp4_file_context mp4_file_context_t;

static switch_status_t do_write_video(switch_file_handle_t *handle, switch_frame_t *frame);

static void *SWITCH_THREAD_FUNC video_write_thread_run(switch_thread_t *thread, void *obj)
{
	mp4_file_context_t *context = (mp4_file_context_t *)obj;
	void *pop = NULL;
	switch_image_t *last_img = NULL;
	switch_status_t status = SWITCH_STATUS_SUCCESS;
	switch_status_t encode_status = SWITCH_STATUS_SUCCESS;
	uint8_t data[SWITCH_DEFAULT_VIDEO_SIZE];

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "video_write_thread start\n");

	while (switch_queue_pop(context->video_queue, &pop) == SWITCH_STATUS_SUCCESS) {
		switch_frame_t frame = { 0 };

		if (!pop) break;

		if (!last_img) { // first img
			last_img = (switch_image_t *)pop;
			continue;
		}

		frame.data = data;
		frame.img = last_img;
		// switch_set_flag(&frame, SFF_DYNAMIC);

		do {
			frame.datalen = SWITCH_DEFAULT_VIDEO_SIZE;
			encode_status = switch_core_codec_encode_video(&context->video_codec, &frame);

			if (encode_status == SWITCH_STATUS_SUCCESS || encode_status == SWITCH_STATUS_MORE_DATA) {
				switch_assert((encode_status == SWITCH_STATUS_SUCCESS && frame.m) || !frame.m);

				if (frame.datalen == 0) break;

				status = do_write_video(context->handle, &frame);
			}
		} while(status == SWITCH_STATUS_SUCCESS && encode_status == SWITCH_STATUS_MORE_DATA);

		switch_img_free(&last_img);
		last_img = (switch_image_t *)pop;
	}

	switch_img_free(&last_img);
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "video_write_thread done\n");

	return NULL;
}

static void launch_video_write_thread(mp4_file_context_t *context, switch_memory_pool_t *pool)
{
	//switch_thread_t *thread;
	switch_threadattr_t *thd_attr = NULL;

	switch_threadattr_create(&thd_attr, pool);
	switch_threadattr_detach_set(thd_attr, 1);
	switch_threadattr_stacksize_set(thd_attr, SWITCH_THREAD_STACKSIZE);
	// switch_threadattr_priority_set(thd_attr, SWITCH_PRI_REALTIME);
	switch_thread_create(&context->video_thread, thd_attr, video_write_thread_run, context, pool);
}


static int flush_video_queue(switch_queue_t *q, int min)
{
	void *pop;

	if (switch_queue_size(q) > min) {
		while (switch_queue_trypop(q, &pop) == SWITCH_STATUS_SUCCESS) {
			switch_image_t *img = (switch_image_t *) pop;
			switch_img_free(&img);
			if (min && switch_queue_size(q) <= min) {
				break;
			}
		}
	}

	return switch_queue_size(q);
}

static switch_status_t mp4_file_open(switch_file_handle_t *handle, const char *path)
{
	mp4_file_context_t *context;
	char *ext;
	unsigned int flags = 0;
	const char *tmp = NULL;

	if ((ext = strrchr(path, '.')) == 0) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Invalid Format\n");
		return SWITCH_STATUS_GENERR;
	}
	ext++;

	if ((context = switch_core_alloc(handle->memory_pool, sizeof(mp4_file_context_t))) == 0) {
		return SWITCH_STATUS_MEMERR;
	}

	memset(context, 0, sizeof(mp4_file_context_t));

	context->handle = handle;
	context->offset = 0;

	if (handle->params && (tmp = switch_event_get_header(handle->params, "mp4v2_video_offset"))) {
		context->offset = atoi(tmp);
	}

	context->audio_type = MP4_ULAW_AUDIO_TYPE; // default

	if (handle->params && (tmp = switch_event_get_header(handle->params, "mp4v2_audio_codec"))) {
		if (!strcasecmp(tmp, "PCMU")) {
			context->audio_type = MP4_ULAW_AUDIO_TYPE;
		} else if (!strcasecmp(tmp, "MP3")) {
			context->audio_type = MP4_MP3_AUDIO_TYPE;
		} else if (!strcasecmp(tmp, "AAC")) {
			context->audio_type = MP4_MPEG4_AUDIO_TYPE;
		} else if (!strcasecmp(tmp, "L16")) {
			context->audio_type = MP4_PCM16_LITTLE_ENDIAN_AUDIO_TYPE;
		}
	}

	switch_mutex_init(&context->mutex, SWITCH_MUTEX_NESTED, handle->memory_pool);

	if (switch_test_flag(handle, SWITCH_FILE_FLAG_WRITE)) {
		flags |= SWITCH_FOPEN_WRITE | SWITCH_FOPEN_CREATE;
		if (switch_test_flag(handle, SWITCH_FILE_WRITE_APPEND) || switch_test_flag(handle, SWITCH_FILE_WRITE_OVER)) {
			flags |= SWITCH_FOPEN_READ;
		} else {
			flags |= SWITCH_FOPEN_TRUNCATE;
		}
	}

	if (switch_test_flag(handle, SWITCH_FILE_FLAG_READ)) {
		flags |= SWITCH_FOPEN_READ;
	}

	if (handle->mm.samplerate) {
		handle->samplerate = handle->mm.samplerate;
	} else {
		handle->mm.samplerate = handle->samplerate;
	}

	if (!handle->mm.ab) {
		handle->mm.ab = 128;
	}

	if (!handle->mm.vb) {
		handle->mm.vb = switch_calc_bitrate(handle->mm.vw, handle->mm.vh, 1, handle->mm.fps);
	}

	// MP4_CREATE_64BIT_DATA if file > 4G

	if ((context->fd = MP4CreateEx(path, 0, 1, 1, NULL, 0, NULL, 0)) == MP4_INVALID_FILE_HANDLE) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Error opening file %s\n", path);
		return SWITCH_STATUS_GENERR;
	}

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "sample rate: %d, channels: %d\n", handle->samplerate, handle->channels);

	if (context->audio_type == MP4_ULAW_AUDIO_TYPE) {
		context->audio = MP4AddULawAudioTrack(context->fd, handle->samplerate);
		MP4SetTrackIntegerProperty(context->fd, context->audio, "mdia.minf.stbl.stsd.ulaw.channels", handle->channels);
		MP4SetTrackIntegerProperty(context->fd, context->audio, "mdia.minf.stbl.stsd.ulaw.sampleSize", 8);
	} else if (context->audio_type == MP4_MP3_AUDIO_TYPE) {
		// handle->samplerate = 44100;
		context->audio = MP4AddAudioTrack(context->fd, handle->samplerate, handle->samplerate, MP4_MP3_AUDIO_TYPE);
		MP4SetTrackName(context->fd, context->audio, ".mp3");
		MP4SetTrackIntegerProperty(context->fd, context->audio, "mdia.minf.stbl.stsd.mp4a.channels", handle->channels);
		// MP4SetTrackIntegerProperty(context->fd, context->audio, "mdia.minf.stbl.stsd..mp3.channels", handle->channels);
	} else if (context->audio_type == MP4_PCM16_LITTLE_ENDIAN_AUDIO_TYPE) {
		context->audio = MP4AddAudioTrack(context->fd, handle->samplerate, handle->samplerate, MP4_PCM16_LITTLE_ENDIAN_AUDIO_TYPE);
		MP4SetTrackName(context->fd, context->audio, "lpcm");
		MP4SetTrackIntegerProperty(context->fd, context->audio, "mdia.minf.stbl.stsd.mp4a.channels", handle->channels);
		MP4SetTrackIntegerProperty(context->fd, context->audio, "mdia.minf.stbl.stsd.lpcm.channels", handle->channels);
		MP4SetTrackIntegerProperty(context->fd, context->audio, "mdia.minf.stbl.stsd.lpcm.sampleSize", 16);
	} else if (context->audio_type == MP4_MPEG4_AUDIO_TYPE) {
		/* AAC object types */
		#define AAC_MAIN 1
		#define AAC_LOW  2
		#define AAC_SSR  3
		#define AAC_LTP  4

		uint16_t info = 0;

		info |= AAC_LOW << 11; // aacObjectType (5bit)
		info |= get_aac_sample_rate_index(handle->samplerate) << 7; //(4bit)
		info |= handle->channels << 3; //(4bit)
		info = htons(info);

		context->audio = MP4AddAudioTrack(context->fd, handle->samplerate, 1024, MP4_MPEG4_AUDIO_TYPE);
		MP4SetTrackESConfiguration(context->fd, context->audio, (uint8_t *)&info, sizeof(info));
		MP4SetTrackIntegerProperty(context->fd, context->audio, "mdia.minf.stbl.stsd.mp4a.channels", handle->channels);
	}

	handle->format = 0;
	handle->sections = 0;
	handle->seekable = 0;
	handle->speed = 0;
	handle->pos = 0;
	handle->private_info = context;
	context->pool = handle->memory_pool;

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "Opening File [%s] %dhz %s\n",
		path, handle->samplerate, switch_test_flag(handle, SWITCH_FILE_FLAG_VIDEO) ? " with VIDEO" : "");

	if (switch_core_codec_init(&context->audio_codec,
							   get_audio_codec_name(context->audio_type),
							   NULL,
							   NULL,
							   handle->samplerate,
							   PTIME,//ms
							   handle->channels, SWITCH_CODEC_FLAG_ENCODE,
							   NULL, handle->memory_pool) == SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Audio Codec Activation Success\n");
	} else {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Audio Codec Activation Fail\n");
		goto end;
	}

	if (context->audio_type == MP4_MP3_AUDIO_TYPE) { // fetch frame size
		uint32_t size;
		uint32_t flag = 0xFFFFFFFF;

		switch_core_codec_encode(&context->audio_codec, NULL, &flag, 0, 0,
			(void *)&context->audio_frame_size, &size, NULL, &flag);
	} else if (context->audio_type == MP4_MPEG4_AUDIO_TYPE) {
		context->audio_frame_size = 1024;
	}

	if (switch_test_flag(handle, SWITCH_FILE_FLAG_VIDEO)) {
		switch_codec_settings_t codec_settings = {{ 0 }};
		codec_settings.video.bandwidth = handle->mm.vb;
		codec_settings.video.fps = handle->mm.fps;

		if (switch_core_codec_init(&context->video_codec,
								   "H264",
								   NULL,
								   NULL,
								   90000,
								   0,//ms
								   1, SWITCH_CODEC_FLAG_ENCODE,
								   &codec_settings, handle->memory_pool) == SWITCH_STATUS_SUCCESS) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Video Codec H264 Activation Success\n");
		} else {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Video Codec H264 Activation Fail\n");
			goto end;
		}

		switch_queue_create(&context->video_queue, 60, handle->memory_pool);
		launch_video_write_thread(context, handle->memory_pool);
	}

	if (switch_test_flag(handle, SWITCH_FILE_FLAG_WRITE)) {
		MP4SetAudioProfileLevel(context->fd, 0x7F);
	}

	switch_buffer_create_dynamic(&context->buf, 512, 512, 1024000);

	return SWITCH_STATUS_SUCCESS;

end:
	if (context->fd) {
		MP4Close(context->fd, 0);
		context->fd = NULL;
	}
	return SWITCH_STATUS_FALSE;
}

static switch_status_t mp4_file_truncate(switch_file_handle_t *handle, int64_t offset)
{
	mp4_file_context_t *context = handle->private_info;
	switch_status_t status;

	if ((status = switch_file_trunc(context->fd, offset)) == SWITCH_STATUS_SUCCESS) {
		handle->pos = 0;
	}

	return status;

}

static switch_status_t mp4_file_close(switch_file_handle_t *handle)
{
	mp4_file_context_t *context = handle->private_info;
	switch_status_t status;

	if (context->fd) {
		MP4Close(context->fd, MP4_CLOSE_DO_NOT_COMPUTE_BITRATE);
		context->fd = NULL;
	}

	if (switch_core_codec_ready(&context->audio_codec)) switch_core_codec_destroy(&context->audio_codec);
	if (switch_core_codec_ready(&context->video_codec)) switch_core_codec_destroy(&context->video_codec);

	if (context->timer.interval) {
		switch_core_timer_destroy(&context->timer);
	}

	if (context->video_queue) {
		switch_queue_term(context->video_queue);
		flush_video_queue(context->video_queue, 0);
	}

	if (context->video_thread) {
		switch_thread_join(&status, context->video_thread);
	}

	switch_buffer_destroy(&context->buf);

	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t mp4_file_seek(switch_file_handle_t *handle, unsigned int *cur_sample, int64_t samples, int whence)
{
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "seek not implemented\n");
	return SWITCH_STATUS_FALSE;
}

static switch_status_t mp4_file_read(switch_file_handle_t *handle, void *data, size_t *len)
{
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "read not implemented\n");
	return SWITCH_STATUS_FALSE;
}

static switch_status_t mp4_file_write(switch_file_handle_t *handle, void *data, size_t *len)
{
	uint32_t datalen = *len * 2 * handle->channels;
	switch_status_t status = SWITCH_STATUS_SUCCESS;
	uint8_t buf[SWITCH_RECOMMENDED_BUFFER_SIZE];
	uint32_t encoded_rate;
	mp4_file_context_t *context = handle->private_info;
	uint32_t size = 0;
	uint32_t flag = 0;

	if (context->audio_type == MP4_PCM16_LITTLE_ENDIAN_AUDIO_TYPE) {
		size = datalen;
		memcpy(buf, data, datalen);
	} else {
		switch_core_codec_encode(&context->audio_codec, NULL,
								data, datalen,
								handle->samplerate,
								buf, &size, &encoded_rate, &flag);
	}

	switch_mutex_lock(context->mutex);

	if (!context->timer.interval) {
		switch_core_timer_init(&context->timer, "soft", 1, 1, context->pool);
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "init timer\n");
	}

	if (size > 0) {
		MP4WriteSample(context->fd, context->audio, buf, size, context->audio_frame_size ? context->audio_frame_size : *len, 0, 1);
	}

	context->audio_duration += *len;

	switch_mutex_unlock(context->mutex);

	return status;
}

static switch_status_t mp4_file_read_video(switch_file_handle_t *handle, switch_frame_t *frame, switch_video_read_flag_t flags)
{
	return SWITCH_STATUS_FALSE;
}

static switch_status_t do_write_video(switch_file_handle_t *handle, switch_frame_t *frame)
{
	uint32_t datalen = frame->datalen;
	switch_status_t status = SWITCH_STATUS_SUCCESS;
	int is_iframe = 0;
	uint32_t size;
	uint8_t *hdr = NULL;
	uint8_t fragment_type;
	uint8_t nal_type;
	uint8_t start_bit;
	uint8_t end_bit;
	mp4_file_context_t *context = handle->private_info;

	hdr = (uint8_t *)frame->data;
	fragment_type = hdr[0] & 0x1f;
	nal_type = hdr[1] & 0x1f;
	start_bit = hdr[1] & 0x80;
	end_bit = hdr[1] & 0x40;

	is_iframe = fragment_type == 5 || (fragment_type == 28 && nal_type == 5);

	// switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "%02x %02x %02x | len:%d m:%d st:%d i:%d\n", hdr[0], hdr[1], hdr[2], datalen, frame->m, start_bit, is_iframe);

	if (fragment_type == 7 && !context->sps_set) { //sps
		context->sps_set = 1;

		init_video_track(context->fd, &context->video, frame);
		if (context->video == MP4_INVALID_TRACK_ID) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Error add video track!\n");
			return SWITCH_STATUS_FALSE;
		}
	} else if (fragment_type == 8 && context->sps_set && !context->pps_set) { //pps
		MP4AddH264PictureParameterSet(context->fd, context->video, hdr, datalen);
		context->pps_set = 1;
	}

	if (fragment_type == 28) {
		if (start_bit && end_bit) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "WTF?\n");
		}

		if (start_bit) {
			nal_type |= (hdr[0] & 0x60);

			size = htonl(datalen);
			switch_buffer_write(context->buf, &size, 4);
			switch_buffer_write(context->buf, &nal_type, 1);
			switch_buffer_write(context->buf, hdr + 2, datalen - 2);
			context->last_chunk_size = datalen - 1;
		} else if (end_bit) {
			uint32_t used;
			const void *data;
			uint32_t *chunk_size = NULL;

			switch_buffer_write(context->buf, hdr + 2, datalen - 2);
			context->last_chunk_size += datalen - 2;
			used = switch_buffer_inuse(context->buf);
			switch_buffer_peek_zerocopy(context->buf, &data);
			chunk_size = (uint32_t *)((uint8_t *)data + used - context->last_chunk_size - 4);
			*chunk_size = htonl(context->last_chunk_size);
		} else {
			switch_buffer_write(context->buf, hdr + 2, datalen - 2);
			context->last_chunk_size += datalen - 2;
		}
	} else {
		size = htonl(datalen);
		switch_buffer_write(context->buf, &size, 4);
		switch_buffer_write(context->buf, hdr, datalen);
	}

	if (!frame->m) {
		return SWITCH_STATUS_SUCCESS;
	}

	switch_mutex_lock(context->mutex);

	if (context->sps_set && context->pps_set) {
		uint32_t used = switch_buffer_inuse(context->buf);
		const void *data;
		int duration = 0;
		int ts = 0;

		if (frame->img && frame->img->user_priv) {
			ts = *(int *)frame->img->user_priv;
		} else {
			switch_core_timer_sync(&context->timer);
			ts = context->timer.samplecount;
		}

		duration = ts - context->last_pts;

		if (duration <= 0) duration = 1;

		// switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "samplecount: %u, duration: %u\n", context->timer.samplecount, duration);
		switch_buffer_peek_zerocopy(context->buf, &data);

		context->last_pts = ts;

		MP4WriteSample(context->fd, context->video, data, used, duration, 0, is_iframe);
		switch_buffer_zero(context->buf);
	}

	switch_mutex_unlock(context->mutex);

	{
		int delta = context->timer.samplecount * (handle->samplerate / 1000) - context->audio_duration;

		if (delta > (int)handle->samplerate) {
			uint8_t data[SWITCH_RECOMMENDED_BUFFER_SIZE] = { 0 };
			size_t samples = handle->samplerate / 1000 * PTIME;

			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "missed audio %d samples at %d\n", delta, (int)context->audio_duration / (handle->samplerate / 1000));

			while ((delta -= samples) > 0) {
				mp4_file_write(handle, data, &samples);
				samples = handle->samplerate / 1000 * PTIME;
			}
		}
	}

	return status;
}

static switch_status_t mp4_file_write_video(switch_file_handle_t *handle, switch_frame_t *frame)
{
	mp4_file_context_t *context = handle->private_info;

	if (!frame->img) {
		return do_write_video(handle, frame);
	} else {
		switch_image_t *img = NULL;

		if (!context->timer.interval) {
			switch_mutex_lock(context->mutex);
			switch_core_timer_init(&context->timer, "soft", 1, 1, context->pool);
			switch_mutex_unlock(context->mutex);
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "init timer\n");
		} else {
			switch_mutex_lock(context->mutex);
			switch_core_timer_sync(&context->timer);
			switch_mutex_unlock(context->mutex);
		}

		switch_img_copy(frame->img, &img);
		switch_assert(img);
		img->user_priv = malloc(sizeof(int));
		*(int *)img->user_priv = context->timer.samplecount;

		if (switch_queue_trypush(context->video_queue, img) != SWITCH_STATUS_SUCCESS) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "video queue full, discard one frame\n");
		}
	}

	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t mp4_file_set_string(switch_file_handle_t *handle, switch_audio_col_t col, const char *string)
{
	return SWITCH_STATUS_FALSE;
}

static switch_status_t mp4_file_get_string(switch_file_handle_t *handle, switch_audio_col_t col, const char **string)
{
	return SWITCH_STATUS_FALSE;
}

static char *supported_formats[3] = { 0 };

SWITCH_MODULE_LOAD_FUNCTION(mod_mp4v2_load)
{
	switch_file_interface_t *file_interface;

	supported_formats[0] = "mp4v2";
	supported_formats[1] = "mp4";

	/* connect my internal structure to the blank pointer passed to me */
	*module_interface = switch_loadable_module_create_module_interface(pool, modname);

	file_interface = switch_loadable_module_create_interface(*module_interface, SWITCH_FILE_INTERFACE);
	file_interface->interface_name = modname;
	file_interface->extens = supported_formats;
	file_interface->file_open = mp4_file_open;
	file_interface->file_close = mp4_file_close;
	file_interface->file_truncate = mp4_file_truncate;
	file_interface->file_read = mp4_file_read;
	file_interface->file_write = mp4_file_write;
	file_interface->file_read_video = mp4_file_read_video;
	file_interface->file_write_video = mp4_file_write_video;
	file_interface->file_seek = mp4_file_seek;
	file_interface->file_set_string = mp4_file_set_string;
	file_interface->file_get_string = mp4_file_get_string;

	/* indicate that the module should continue to be loaded */
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
