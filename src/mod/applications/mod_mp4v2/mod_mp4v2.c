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

static void record_video_thread(switch_core_session_t *session, void *obj)
{
	struct record_helper *eh = obj;
	switch_channel_t *channel = switch_core_session_get_channel(session);
	switch_status_t status;
	switch_frame_t *read_frame;
	uint bytes;
	MP4FileHandle mp4;
	MP4TrackId video;
	unsigned char buf[40960];
	int len = 0;
	uint8_t iframe = 0;
	uint32_t *size = (uint32_t *)buf;
	uint8_t *hdr = NULL;
	uint8_t fragment_type;
	uint8_t nal_type;
	uint8_t start_bit;
	uint8_t *sps = NULL;
	// uint8_t *pps = NULL;
	int sps_set = 0;
	int pps_set = 0;

	eh->up = 1;
	mp4 = eh->fd;

	/* Tell the channel to request a fresh vid frame */
	switch_core_session_request_video_refresh(session);

	len = 0;
	while (switch_channel_ready(channel) && eh->up) {
		status = switch_core_session_read_video_frame(session, &read_frame, SWITCH_IO_FLAG_NONE, 0);

		if (!SWITCH_READ_ACCEPTABLE(status)) {
			break;
		}

		if (switch_test_flag(read_frame, SFF_CNG)) {
			continue;
		}

		bytes = read_frame->datalen;

		if (bytes > 2000) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "xxxxxxxx buffer overflow\n");
			continue;
		}

		hdr = read_frame->data;
		fragment_type = hdr[0] & 0x1f;
		nal_type = hdr[1] & 0x1f;
		start_bit = hdr[1] & 0x80;
		iframe = (((fragment_type == 28 || fragment_type == 29) && nal_type == 5 && start_bit == 128) || fragment_type == 5 || fragment_type ==7 || fragment_type ==8) ? 1 : 0;

#if 0
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "%02x %02x %02x | len:%d m:%d st:%d i:%d\n", hdr[0], hdr[1], hdr[2], bytes, read_frame->m, start_bit, iframe);
#endif

		// echo back
		switch_core_session_write_video_frame(session, read_frame, SWITCH_IO_FLAG_NONE, 0);

		if (fragment_type == 7 && !sps_set) { //sps
			sps = malloc(bytes);
			memcpy(sps, read_frame->data, bytes);
			sps_set = 1;

			switch_mutex_lock(eh->mutex);

			init_video_track(mp4, &video, read_frame);
			if (video == MP4_INVALID_TRACK_ID) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Error add video track!\n");
				switch_mutex_unlock(eh->mutex);
				goto end;
			}

			switch_mutex_unlock(eh->mutex);
			continue;
		} else if (fragment_type == 8 && !pps_set) { //pps
			switch_mutex_lock(eh->mutex);
			MP4AddH264PictureParameterSet(mp4, video, read_frame->data, bytes);
			switch_mutex_unlock(eh->mutex);
			pps_set = 1;
			// continue;
		}

		if ((!sps_set) && (!pps_set)) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Waiting for SPS/PPS\n");
			// continue;
		}

		len += 4 + read_frame->datalen;

		if (len > 40960) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "buffer overflow!!!! %d\n", len);
			len = 0;
			size = (uint32_t *)buf;
			continue;
		}

		*size = htonl(read_frame->datalen);
		memcpy(size + 1, read_frame->data, read_frame->datalen);

		size = (uint32_t *)((uint8_t *)size + 4 + read_frame->datalen);

		if (read_frame->m) {
			int duration = 0;

			switch_mutex_lock(eh->mutex);
			if (!eh->timer.interval) {
				switch_core_timer_init(&eh->timer, "soft", 1, 1, switch_core_session_get_pool(session));
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "init timer\n");
			} else {
				switch_core_timer_sync(&eh->timer);
			}
			switch_mutex_unlock(eh->mutex);

			if (eh->last_pts) {
				duration = eh->timer.samplecount - eh->last_pts;
			}
			eh->last_pts = eh->timer.samplecount;

			switch_mutex_lock(eh->mutex);
			MP4WriteSample(mp4, video, buf, len, duration, 0, iframe);
			switch_mutex_unlock(eh->mutex);
			len = 0;
			size = (uint32_t *)buf;
		}

	}

end:
	eh->up = 0;
	return;
}

SWITCH_STANDARD_APP(record_mp4_function)
{
	switch_status_t status;
	switch_frame_t *read_frame;
	switch_channel_t *channel = switch_core_session_get_channel(session);
	struct record_helper eh = { 0 };
	MP4FileHandle fd;
	MP4TrackId audio;
	switch_mutex_t *mutex = NULL;
	switch_codec_implementation_t read_impl = { 0 };
	switch_dtmf_t dtmf = { 0 };
	int count = 0, sanity = 30;
	switch_codec_t codec;
	int duration = 0;
	switch_event_t *event;

	memset(&codec, 0, sizeof(switch_codec_t));

	switch_channel_answer(channel);
	switch_core_session_get_read_impl(session, &read_impl);

	switch_channel_set_variable(channel, SWITCH_PLAYBACK_TERMINATOR_USED, "");

	while (switch_channel_up(channel) && !switch_channel_test_flag(channel, CF_VIDEO)) {
		switch_yield(10000);

		if (count) count--;

		if (count == 0) {
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_INFO, "%s waiting for video.\n", switch_channel_get_name(channel));
			count = 100;
			if (!--sanity) {
				switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_WARNING, "%s timeout waiting for video.\n",
								  switch_channel_get_name(channel));
				switch_channel_set_variable(channel, SWITCH_CURRENT_APPLICATION_RESPONSE_VARIABLE, "Got timeout while waiting for video");
				return;
			}
		}
	}

	if (!switch_channel_ready(channel)) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_CRIT, "%s not ready.\n", switch_channel_get_name(channel));
		switch_channel_set_variable(channel, SWITCH_CURRENT_APPLICATION_RESPONSE_VARIABLE, "Channel not ready");
		return;
	}

	if ((fd = MP4CreateEx((char*)data, 0, 1, 1, NULL, 0, NULL, 0)) == MP4_INVALID_FILE_HANDLE) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_CRIT, "Error opening file %s\n", (char *) data);
		switch_channel_set_variable(channel, SWITCH_CURRENT_APPLICATION_RESPONSE_VARIABLE, "Got error while opening file");
		return;
	}

	audio = MP4AddULawAudioTrack(fd, read_impl.samples_per_second);
	MP4SetTrackIntegerProperty(fd, audio, "mdia.minf.stbl.stsd.ulaw.channels", read_impl.number_of_channels);
	MP4SetTrackIntegerProperty(fd, audio, "mdia.minf.stbl.stsd.ulaw.sampleSize", 8);

	// audio = MP4AddAACAudioTrack(fd, 8000);
	// audio = MP4AddAudioTrack(fd, 8000, MP4_INVALID_DURATION, MP4_MPEG4_AUDIO_TYPE);

	// MP4SetTrackIntegerProperty(fd, audio, "mdia.minf.stbl.stsd.aac.channels", 1);
	// MP4SetTrackIntegerProperty(fd, audio, "mdia.minf.stbl.stsd.aac.sampleSize", 8);

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "ms: %d, ts: %d\n", read_impl.microseconds_per_packet, read_impl.samples_per_second /(read_impl.microseconds_per_packet / 1000) / 2);

	/* MP4SetAudioProfileLevel sets the minumum profile/level of MPEG-4 audio support necessary to render the contents of the file.
	ISO/IEC 14496-1:2001 MPEG-4 Systems defines the following values:
	0x00 Reserved
	0x01 Main Profile @ Level 1
	0x02 Main Profile @ Level 2
	0x03 Main Profile @ Level 3
	0x04 Main Profile @ Level 4
	0x05 Scalable Profile @ Level 1
	0x06 Scalable Profile @ Level 2
	0x07 Scalable Profile @ Level 3
	0x08 Scalable Profile @ Level 4
	0x09 Speech Profile @ Level 1
	0x0A Speech Profile @ Level 2
	0x0B Synthesis Profile @ Level 1
	0x0C Synthesis Profile @ Level 2
	0x0D Synthesis Profile @ Level 3
	0x0E-0x7F Reserved
	0x80-0xFD User private
	0xFE No audio profile specified
	0xFF No audio required
	*/
    // MP4SetAudioProfileLevel(fd, 0x7F);
	MP4SetAudioProfileLevel(fd, 0x0F);
	if (0) {
		// uint8_t c[] = {0x15, 0x88}; // 00010 1011 0001 000
		uint8_t c[] = {0x12, 0x08}; // 00010 0100 0001 000
		MP4SetTrackESConfiguration(fd, audio, c, sizeof(c));
	}

    if (read_impl.ianacode != 0) {
		if (switch_core_codec_init(&codec,
								   "PCMU",
								   NULL,
								   NULL,
								   read_impl.samples_per_second,
								   read_impl.microseconds_per_packet / 1000,
								   read_impl.number_of_channels, SWITCH_CODEC_FLAG_ENCODE | SWITCH_CODEC_FLAG_DECODE,
								   NULL, switch_core_session_get_pool(session)) == SWITCH_STATUS_SUCCESS) {
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "Audio Codec Activation Success\n");
		} else {
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "Audio Codec Activation Fail\n");
			switch_channel_set_variable(channel, SWITCH_CURRENT_APPLICATION_RESPONSE_VARIABLE, "Audio codec activation failed");
			goto end;
		}
		switch_core_session_set_read_codec(session, &codec);
	}

	if (switch_channel_test_flag(channel, CF_VIDEO)) {
		switch_codec_implementation_t video_read_impl = { 0 };

		switch_core_session_get_video_read_impl(session, &video_read_impl);

		if (video_read_impl.iananame && (!strcasecmp(video_read_impl.iananame, "H264"))) {
			switch_channel_set_flag(channel, CF_VIDEO_PASSIVE);
			switch_channel_set_flag(channel, CF_VIDEO_READY);

			switch_mutex_init(&mutex, SWITCH_MUTEX_NESTED, switch_core_session_get_pool(session));
			eh.mutex = mutex;
			eh.fd = fd;
			switch_core_media_start_video_function(session, record_video_thread, &eh);
		} else {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "only H264 video is supported, you have %s\n", video_read_impl.iananame);
			switch_channel_set_flag(channel, CF_VIDEO_ECHO);
			switch_channel_set_flag(channel, CF_VIDEO_READY);
		}
	}

	if (switch_event_create(&event, SWITCH_EVENT_RECORD_START) == SWITCH_STATUS_SUCCESS) {
		switch_channel_event_set_data(channel, event);
		switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "Record-File-Path", (char *)data);
		switch_event_fire(&event);
	}

	while (switch_channel_ready(channel)) {
		status = switch_core_session_read_frame(session, &read_frame, SWITCH_IO_FLAG_SINGLE_READ, 0);

		if (switch_channel_test_flag(channel, CF_BREAK)) {
			switch_channel_clear_flag(channel, CF_BREAK);
			eh.up = 0;
			break;
		}

		switch_ivr_parse_all_events(session);

		//check for dtmf interrupts
		if (switch_channel_has_dtmf(channel)) {
			const char * terminators = switch_channel_get_variable(channel, SWITCH_PLAYBACK_TERMINATORS_VARIABLE);
			switch_channel_dequeue_dtmf(channel, &dtmf);

			if (terminators && !strcasecmp(terminators, "none")) {
				terminators = NULL;
			}

			if (terminators && strchr(terminators, dtmf.digit)) {
				char sbuf[2] = {dtmf.digit, '\0'};
				switch_channel_set_variable(channel, SWITCH_PLAYBACK_TERMINATOR_USED, sbuf);
				eh.up = 0;
				break;
			}
		}

		if (!SWITCH_READ_ACCEPTABLE(status)) {
			eh.up = 0;
			break;
		}

        switch_core_session_write_frame(session, read_frame, SWITCH_IO_FLAG_NONE, 0);

		if (switch_test_flag(read_frame, SFF_CNG)) {
			// switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "cng, datalen:%d\n", read_frame->datalen);
			continue;
			// memset(read_frame->data, 0x0, 160);
		}

		if (mutex) switch_mutex_lock(mutex);

		if (!eh.timer.interval) {
			switch_core_timer_init(&eh.timer, "soft", 1, 1, switch_core_session_get_pool(session));
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "init timer\n");
		}

		duration = read_frame->datalen / read_impl.number_of_channels;

		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "%d %d\n", duration, read_frame->datalen);

		MP4WriteSample(fd, audio, read_frame->data, read_frame->datalen, duration, 0, 1);

		if (mutex) switch_mutex_unlock(mutex);
	}

	switch_channel_set_variable(channel, SWITCH_CURRENT_APPLICATION_RESPONSE_VARIABLE, "OK");

	if (switch_event_create(&event, SWITCH_EVENT_RECORD_STOP) == SWITCH_STATUS_SUCCESS) {
		switch_channel_event_set_data(channel, event);
		switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "Record-File-Path", (char *)data);
		switch_event_fire(&event);
	}

end:

	if (eh.up) {
		while (eh.up) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "waiting video thread to be done ...\n");
			switch_yield(100000);
			switch_cond_next();
		}
	}

	if (eh.timer.interval) {
		switch_core_timer_destroy(&eh.timer);
	}

	switch_core_media_end_video_function(session);
	switch_channel_clear_flag(channel, CF_VIDEO_PASSIVE);

	switch_core_session_set_read_codec(session, NULL);

	if (fd != MP4_INVALID_FILE_HANDLE) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "closing file %s\n", (char *)data);
		MP4Close(fd, 0);
	}

	if (switch_core_codec_ready(&codec)) switch_core_codec_destroy(&codec);
}

// SWITCH_STANDARD_APP(play_mp4_function)
// {

// }

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
	switch_memory_pool_t *pool;
	MP4FileHandle fd;
	MP4TrackId audio;
	MP4TrackId video;
	switch_codec_t audio_codec;
	switch_codec_t video_codec;
	switch_mutex_t *mutex;
	switch_buffer_t *buf;
	int sps_set;
	int pps_set;
	switch_timer_t timer;
	uint64_t last_pts;
	int offset;
	int audio_start;
	uint8_t audio_type; // MP4 Audio Type
	MP4Duration audio_duration;
	switch_image_t *last_img;
};

typedef struct mp4_file_context mp4_file_context_t;

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

	context->offset = -100;
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

		context->audio = MP4AddAudioTrack(context->fd, handle->samplerate, handle->samplerate, MP4_MPEG4_AUDIO_TYPE);
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
							   20,//ms
							   handle->channels, SWITCH_CODEC_FLAG_ENCODE,
							   NULL, handle->memory_pool) == SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Audio Codec Activation Success\n");
	} else {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Audio Codec Activation Fail\n");
		goto end;
	}

	if (switch_test_flag(handle, SWITCH_FILE_FLAG_VIDEO)) {
		if (switch_core_codec_init(&context->video_codec,
								   "H264",
								   NULL,
								   NULL,
								   90000,
								   0,//ms
								   1, SWITCH_CODEC_FLAG_ENCODE,
								   NULL, handle->memory_pool) == SWITCH_STATUS_SUCCESS) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Video Codec H264 Activation Success\n");
		} else {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Video Codec H264 Activation Fail\n");
			goto end;
		}
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

	if (context->fd) {
		MP4Close(context->fd, 0);
		context->fd = NULL;
	}

	if (switch_core_codec_ready(&context->audio_codec)) switch_core_codec_destroy(&context->audio_codec);
	if (switch_core_codec_ready(&context->video_codec)) switch_core_codec_destroy(&context->video_codec);

	if (context->timer.interval) {
		switch_core_timer_destroy(&context->timer);
	}

	switch_img_free(&context->last_img);
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

	context->audio_duration += *len;


	if (context->audio_type == MP4_PCM16_LITTLE_ENDIAN_AUDIO_TYPE) {
		size = datalen;
		memcpy(buf, data, datalen);
	} else {
		switch_core_codec_encode(&context->audio_codec, NULL,
								data, datalen,
								handle->samplerate,
								buf, &size, &encoded_rate, NULL);
	}

	switch_mutex_lock(context->mutex);

	if (!context->timer.interval) {
		switch_core_timer_init(&context->timer, "soft", 1, 1, context->pool);
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "init timer\n");
	} else if(!context->audio_start) { // try make up some sampels if the video already start
		int i, count;
		uint8_t buf0[SWITCH_RECOMMENDED_BUFFER_SIZE] = { 0 };

		context->audio_start++;
		switch_core_timer_sync(&context->timer);

		count = context->timer.samplecount - context->offset;

		if (count > 0) {
			count /= *len;
		}

		if (context->audio_type != MP4_ULAW_AUDIO_TYPE) {
			count = 0; // todo: make this feature work for mp3/aac
		}

		if (count){
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "video is short, make up %lu samples\n", count * (*len));
			MP4WriteSample(context->fd, context->audio, buf0, size, 0, 0, 1);
		}

		for (i = 1; i < count; i++) {
			MP4WriteSample(context->fd, context->audio, buf0, size, *len, 0, 1);
		}
	}

	if (context->audio_type == MP4_MPEG4_AUDIO_TYPE && size == 0) {
		// don't write 0
	} else {
		MP4WriteSample(context->fd, context->audio, buf, size, context->audio_duration, 0, 1);
		context->audio_duration = 0;
	}

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
	mp4_file_context_t *context = handle->private_info;

	hdr = (uint8_t *)frame->data;
	fragment_type = hdr[0] & 0x1f;
	nal_type = hdr[1] & 0x1f;
	start_bit = hdr[1] & 0x80;
	is_iframe = (((fragment_type == 28 || fragment_type == 29) && nal_type == 5 && start_bit == 128) || fragment_type == 5 || fragment_type ==7 || fragment_type ==8) ? 1 : 0;

	// switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "%02x %02x %02x | len:%d m:%d st:%d i:%d\n", hdr[0], hdr[1], hdr[2], datalen, frame->m, start_bit, is_iframe);

	size = htonl(datalen);
	switch_buffer_write(context->buf, &size, 4);
	switch_buffer_write(context->buf, hdr, datalen);

	switch_mutex_lock(context->mutex);

	if (fragment_type == 7 && !context->sps_set) { //sps
		context->sps_set = 1;

		init_video_track(context->fd, &context->video, frame);
		if (context->video == MP4_INVALID_TRACK_ID) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Error add video track!\n");
			switch_goto_status(SWITCH_STATUS_FALSE, end);
		}
	} else if (fragment_type == 8 && context->sps_set && !context->pps_set) { //pps
		MP4AddH264PictureParameterSet(context->fd, context->video, hdr, datalen);
		context->pps_set = 1;
	}

	if (nal_type == 7 || nal_type == 8 || frame->m == 0) {
	} else if (context->sps_set && context->pps_set) {
		uint32_t used = switch_buffer_inuse(context->buf);
		const void *data;
		int duration = 0;

		if (!context->timer.interval) {
			switch_core_timer_init(&context->timer, "soft", 1, 1, context->pool);
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "init timer\n");
		} else {
			switch_core_timer_sync(&context->timer);
		}

		duration = context->timer.samplecount - context->last_pts;

		// switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "samplecount: %u, duration: %u\n", context->timer.samplecount, duration);
		switch_buffer_peek_zerocopy(context->buf, &data);

		if (context->last_pts == 0) { // first img, write at the very beginning so we don't see blank screen
			duration /= 2;
			MP4WriteSample(context->fd, context->video, data, used, duration, 0, is_iframe);

			if (duration > context->offset) {
				duration -= context->offset;
			} else {
				duration = 0;
			}
		}

		context->last_pts = context->timer.samplecount;

		// switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "samplecount: %u, duration: %u\n", context->timer.samplecount, duration);

		if (duration) {
			MP4WriteSample(context->fd, context->video, data, used, duration, 0, is_iframe);
		}
		switch_buffer_zero(context->buf);
	}

end:
	switch_mutex_unlock(context->mutex);

	return status;
}

static switch_status_t mp4_file_write_video(switch_file_handle_t *handle, switch_frame_t *frame)
{
	switch_status_t status = SWITCH_STATUS_SUCCESS;
	switch_status_t encode_status = SWITCH_STATUS_SUCCESS;
	mp4_file_context_t *context = handle->private_info;

	if (!frame->img) {
		return do_write_video(handle, frame);
	} else {
		switch_frame_t eframe = { 0 };
		uint8_t data[SWITCH_RECOMMENDED_BUFFER_SIZE];


		if (!context->last_img) {
			switch_img_copy(frame->img, &context->last_img);
			return status;
		}

		eframe.data = data + 12;
		eframe.datalen = SWITCH_RECOMMENDED_BUFFER_SIZE - 12;
		eframe.img = context->last_img;
		do {
			frame->datalen = SWITCH_DEFAULT_VIDEO_SIZE;
			encode_status = switch_core_codec_encode_video(&context->video_codec, &eframe);

			if (encode_status == SWITCH_STATUS_SUCCESS || encode_status == SWITCH_STATUS_MORE_DATA) {
				switch_assert((encode_status == SWITCH_STATUS_SUCCESS && eframe.m) || !eframe.m);
				if (eframe.datalen > 0) status = do_write_video(handle, &eframe);
			}
		} while(status == SWITCH_STATUS_SUCCESS && encode_status == SWITCH_STATUS_MORE_DATA);
	}

	switch_img_copy(frame->img, &context->last_img);

	return status;
}

static switch_status_t mp4_file_set_string(switch_file_handle_t *handle, switch_audio_col_t col, const char *string)
{
	return SWITCH_STATUS_FALSE;
}

static switch_status_t mp4_file_get_string(switch_file_handle_t *handle, switch_audio_col_t col, const char **string)
{
	return SWITCH_STATUS_FALSE;
}

static char *supported_formats[2] = { 0 };

SWITCH_MODULE_LOAD_FUNCTION(mod_mp4v2_load)
{
	switch_application_interface_t *app_interface;
	switch_file_interface_t *file_interface;

	supported_formats[0] = "mp4";

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

	// SWITCH_ADD_APP(app_interface, "play_mp4", "play an mp4 file", "play an mp4 file", play_mp4_function, "<file>", SAF_NONE);
	SWITCH_ADD_APP(app_interface, "record_mp4", "record an mp4 file", "record an mp4 file", record_mp4_function, "<file>", SAF_NONE);

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
