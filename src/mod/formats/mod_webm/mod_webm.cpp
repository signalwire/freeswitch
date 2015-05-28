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
 * mod_webm -- webm File Format support using libwebm.
 *
 */

#include <switch.h>

// #include "mkvreader.hpp"
// #include "mkvparser.hpp"
// #include "mkvmuxer.hpp"
#include "mkvwriter.hpp"
// #include "mkvmuxerutil.hpp"
// #include "sample_muxer_metadata.h"

// using mkvmuxer::int64;
// using mkvmuxer::uint64;
#include "mod_webm.h"

#ifdef _MSC_VER
// Disable MSVC warnings that suggest making code non-portable.
#pragma warning(disable : 4996)
#endif

SWITCH_MODULE_LOAD_FUNCTION(mod_webm_load);
SWITCH_MODULE_DEFINITION(mod_webm, mod_webm_load, NULL, NULL);

#define IS_VP8_KEY_FRAME(byte) ((((byte) & 0x01) ^ 0x01) ? true : false)
#define IS_VP9_KEY_FRAME(byte) (((byte) & 0x01) ? true : false)

// #define AUDIO_CODEC "OPUS"
#define AUDIO_CODEC "VORBIS"

struct webm_file_context {
	switch_memory_pool_t *pool;
	mkvmuxer::AudioTrack* audio;
	mkvmuxer::VideoTrack* video;
	uint64_t audio_track_id;
	uint64_t video_track_id;
	switch_codec_t audio_codec;
	switch_codec_t video_codec;
	switch_mutex_t *mutex;
	switch_buffer_t *buf;
	switch_buffer_t *audio_buffer;
	switch_timer_t timer;
	// uint64_t last_pts;
	int offset;
	uint64_t audio_duration;
	int audio_start;
	int vid_ready;
	int audio_ready;
	mkvmuxer::MkvWriter *writer;
	mkvmuxer::Segment *segment;
};

typedef struct webm_file_context webm_file_context_t;

static switch_status_t webm_file_open(switch_file_handle_t *handle, const char *path)
{
	webm_file_context_t *context;
	char *ext;
	unsigned int flags = 0;
	const char *tmp = NULL;
	char *fmtp;

	if ((ext = strrchr((char *)path, '.')) == 0) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Invalid Format\n");
		return SWITCH_STATUS_GENERR;
	}
	ext++;

	if ((context = (webm_file_context_t *)switch_core_alloc(handle->memory_pool, sizeof(webm_file_context_t))) == 0) {
		return SWITCH_STATUS_MEMERR;
	}

	memset(context, 0, sizeof(webm_file_context_t));

	context->offset = -100;
	if (handle->params && (tmp = switch_event_get_header(handle->params, "webmv2_video_offset"))) {
		context->offset = atoi(tmp);
	}

	switch_mutex_init(&context->mutex, SWITCH_MUTEX_NESTED, handle->memory_pool);
	switch_core_timer_init(&context->timer, "soft", 1, 1000, context->pool);
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "init timer\n");

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

	context->writer = new mkvmuxer::MkvWriter();

	if (!context->writer->Open(path)) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Error opening file %s\n", path);
		return SWITCH_STATUS_GENERR;
	}

	context->segment = new mkvmuxer::Segment();

	if (!context->segment || !context->segment->Init(context->writer)) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Error init segment\n");
		return SWITCH_STATUS_GENERR;
	}

	// context->segment.set_mode(mkvmuxer::Segment::kLive);
	context->segment->set_mode(mkvmuxer::Segment::kFile);
	context->segment->OutputCues(true);

	mkvmuxer::SegmentInfo* const info = context->segment->GetSegmentInfo();
	info->set_timecode_scale(1000000);
	info->set_muxing_app("FreeSWITCH");
	info->set_writing_app(switch_version_full());

	context->audio_track_id = context->segment->AddAudioTrack(handle->samplerate, handle->channels, 0);
	if (!context->audio_track_id) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Error add audio track!\n");
		goto end;
	}
	context->audio = static_cast<mkvmuxer::AudioTrack*>(context->segment->GetTrackByNumber(context->audio_track_id));
	context->audio->set_codec_id("A_" AUDIO_CODEC);
	switch_buffer_create_dynamic(&context->audio_buffer, 512, 512, 0);

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "sample rate: %d, channels: %d\n", handle->samplerate, handle->channels);

	handle->format = 0;
	handle->sections = 0;
	handle->seekable = 0;
	handle->speed = 0;
	handle->pos = 0;
	handle->private_info = context;
	context->pool = handle->memory_pool;
	// handle->flags |= SWITCH_FILE_NATIVE;

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "Opening File [%s] %dhz %s\n",
		path, handle->samplerate, switch_test_flag(handle, SWITCH_FILE_FLAG_VIDEO) ? " with VIDEO" : "");

	fmtp = switch_core_sprintf(context->pool,
							   "useinbandfec=1;minptime=20;ptime=20;samplerate=%d%s", handle->samplerate, handle->channels == 2 ? ",stereo=1" : "");

	if (switch_core_codec_init(&context->audio_codec,
							   AUDIO_CODEC,
							   NULL,
							   fmtp,
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
							   "VP8",
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

	if (!strcmp(AUDIO_CODEC, "VORBIS")) {
		uint16_t size = 0;
		uint8_t *codec_private_data = NULL;
		switch_core_codec_control(&context->audio_codec, SCC_GET_CODEC_PRIVATE, SCCT_INT, (void *)&size, NULL, (void **)&codec_private_data);
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "======codec_private_data size: %d data: %p\n", size, codec_private_data);
		context->audio->SetCodecPrivate(codec_private_data, size);
	}

	if (1) { // for better quality?
		int bw = 4096;
		switch_core_codec_control(&context->video_codec, SCC_VIDEO_BANDWIDTH, SCCT_INT, (void *)&bw, NULL, NULL);
	}

	switch_buffer_create_dynamic(&context->buf, 512, 512, 1024000);

	return SWITCH_STATUS_SUCCESS;

end:

	if (context->segment) delete context->segment;
	if (context->writer) delete context->writer;

	return SWITCH_STATUS_GENERR;
}

static switch_status_t webm_file_truncate(switch_file_handle_t *handle, int64_t offset)
{
	webm_file_context_t *context = (webm_file_context_t *)handle->private_info;
	switch_status_t status = SWITCH_STATUS_FALSE;

	return status;
}

static switch_status_t webm_file_close(switch_file_handle_t *handle)
{
	webm_file_context_t *context = (webm_file_context_t *)handle->private_info;

	context->segment->Finalize();
	delete context->segment;
	delete context->writer;

	if (switch_core_codec_ready(&context->audio_codec)) switch_core_codec_destroy(&context->audio_codec);
	if (switch_core_codec_ready(&context->video_codec)) switch_core_codec_destroy(&context->video_codec);

	if (context->timer.interval) {
		switch_core_timer_destroy(&context->timer);
	}

	switch_buffer_destroy(&context->buf);
	switch_buffer_destroy(&context->audio_buffer);

	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t webm_file_seek(switch_file_handle_t *handle, unsigned int *cur_sample, int64_t samples, int whence)
{
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "seek not implemented\n");
	return SWITCH_STATUS_FALSE;
}

static switch_status_t webm_file_read(switch_file_handle_t *handle, void *data, size_t *len)
{
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "read not implemented\n");
	return SWITCH_STATUS_FALSE;
}

static switch_status_t webm_file_write(switch_file_handle_t *handle, void *data, size_t *len)
{

	uint32_t datalen = *len * 2 * handle->channels;
	switch_status_t status = SWITCH_STATUS_SUCCESS;
	uint8_t buf[SWITCH_RECOMMENDED_BUFFER_SIZE] = { 0 }, *bp = buf;
	uint32_t encoded_rate;
	webm_file_context_t *context = (webm_file_context_t *)handle->private_info;
	uint32_t size = 0;


	if (!context->vid_ready) {
		return status;
	}

	context->audio_duration += *len;

	if (switch_test_flag(handle, SWITCH_FILE_NATIVE)) {
		size = datalen;
		memcpy(buf, data, datalen);
	} else {
		uint32_t rb;

		if (!context->audio_ready) {
			int offset = 1200;
			int fps = handle->samplerate / *len;
			int lead_frames = (offset * fps) / 1000;

			for (int x = 0; x < lead_frames; x++) {
				switch_buffer_write(context->audio_buffer, buf, datalen);
			}
			context->audio_ready = 1;
		}

		switch_buffer_write(context->audio_buffer, data, datalen);
		rb = switch_buffer_read(context->audio_buffer, data, datalen);
		datalen = rb;
		size = SWITCH_RECOMMENDED_BUFFER_SIZE;
		switch_core_codec_encode(&context->audio_codec, NULL,
								data, datalen,
								handle->samplerate,
								buf, &size, &encoded_rate, NULL);
	}

	
	if (size > 0) {
		// timecode still need to figure out for sync

		switch_mutex_lock(context->mutex);
		switch_core_timer_sync(&context->timer);
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Writing audio %d bytes, ts: %lld\n", size, context->timer.samplecount);
		bool ret = context->segment->AddFrame(bp, size, context->audio_track_id, context->timer.samplecount * 1000LL, true);
		// bool ret = context->segment->AddFrame((const uint8_t *)buf, size, context->audio_track_id, context->audio_duration, true);
		switch_mutex_unlock(context->mutex);

		if (!ret) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Error writing audio %d bytes, pts: %lld or %lld\n", size, context->timer.samplecount * 1000LL, context->audio_duration);
		}
	}

	return status;
}

static switch_status_t webm_file_read_video(switch_file_handle_t *handle, switch_frame_t *frame, switch_video_read_flag_t flags)
{
	return SWITCH_STATUS_FALSE;
}

static switch_status_t buffer_vp8_packets(webm_file_context_t *context, switch_frame_t *frame, bool *is_keyframe)
{
	uint8_t *data = (uint8_t *)frame->data;
	uint8_t S;
	uint8_t DES;
	uint8_t PID;
	int len;

	DES = *data;
	data++;
	S = DES & 0x10;
	PID = DES & 0x07;

	if (DES & 0x80) { // X
		uint8_t X = *data;
		data++;
		if (X & 0x80) { // I
			uint8_t M = (*data) & 0x80;
			data++;
			if (M) data++;
		}
		if (X & 0x40) data++; // L
		if (X & 0x30) data++; // T/K
	}

	len = frame->datalen - (data - (uint8_t *)frame->data);

	if (len <= 0) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Invalid packet %d\n", len);
		return SWITCH_STATUS_RESTART;
	}

	if (S && (PID == 0)) {
		*is_keyframe = IS_VP8_KEY_FRAME(*data);
	}

	switch_buffer_write(context->buf, data, len);

	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t do_write_video(switch_file_handle_t *handle, switch_frame_t *frame)
{
	uint32_t datalen = frame->datalen;
	switch_status_t status = SWITCH_STATUS_SUCCESS;
	int is_iframe = 0;
	uint8_t *hdr = NULL;
	uint8_t start_bit;
	uint32_t used;
	bool is_key = false;
	webm_file_context_t *context = (webm_file_context_t *)handle->private_info;

	hdr = (uint8_t *)frame->data;

	switch_assert(frame->img);

	// switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "%02x %02x %02x | len:%d m:%d st:%d i:%d\n", hdr[0], hdr[1], hdr[2], datalen, frame->m, start_bit, is_iframe);

	

	if (!context->video) {
		context->video_track_id = context->segment->AddVideoTrack(frame->img->d_w, frame->img->d_h, 0);
		if (!context->video_track_id) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Error add video track!\n");
			switch_goto_status(SWITCH_STATUS_FALSE, end);
		}
		context->video = static_cast<mkvmuxer::VideoTrack*>(context->segment->GetTrackByNumber(context->video_track_id));
		context->video->SetStereoMode(0);
		context->video->set_codec_id("V_VP8");
		context->video->set_uid(0xDEADBEEF); // debug ?
	}

	buffer_vp8_packets(context, frame, &is_key);

	if (frame->m && (used = switch_buffer_inuse(context->buf))) {
		const void *data;
		int duration = 0;

		
		switch_buffer_peek_zerocopy(context->buf, &data);
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "VID samplecount: %u\n", context->timer.samplecount);

		switch_mutex_lock(context->mutex);
		switch_core_timer_sync(&context->timer);
		bool ret = false;
		ret = context->segment->AddFrame((const uint8_t *)data, used, context->video_track_id, context->timer.samplecount * 1000LL, is_key);
		switch_mutex_unlock(context->mutex);

		if (!ret) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Error add frame %d bytes, timecode: %llu\n", used, context->timer.samplecount * 1000LL);
			switch_goto_status(SWITCH_STATUS_FALSE, end);
		}

		switch_buffer_zero(context->buf);
	}

end:
	

	return status;
}

static switch_status_t webm_file_write_video(switch_file_handle_t *handle, switch_frame_t *frame)
{
	switch_status_t status = SWITCH_STATUS_SUCCESS;
	switch_status_t encode_status = SWITCH_STATUS_SUCCESS;
	webm_file_context_t *context = (webm_file_context_t *)handle->private_info;

	if (!frame->img) {
		status = do_write_video(handle, frame);
	} else {
		switch_frame_t eframe = { 0 };
		uint8_t data[SWITCH_RTP_MAX_BUF_LEN];

		eframe.data = data + 12;
		eframe.datalen = SWITCH_RTP_MAX_BUF_LEN - 12;
		eframe.img = frame->img;
		do {
			frame->datalen = SWITCH_DEFAULT_VIDEO_SIZE;
			encode_status = switch_core_codec_encode_video(&context->video_codec, &eframe);

			if (encode_status == SWITCH_STATUS_SUCCESS || encode_status == SWITCH_STATUS_MORE_DATA) {
				switch_assert((encode_status == SWITCH_STATUS_SUCCESS && eframe.m) || !eframe.m);
				if (eframe.datalen > 0) status = do_write_video(handle, &eframe);
			}
		} while(status == SWITCH_STATUS_SUCCESS && encode_status == SWITCH_STATUS_MORE_DATA);
	}

	if (status == SWITCH_STATUS_SUCCESS) {
		context->vid_ready = 1;
	}

	return status;
}

static switch_status_t webm_file_set_string(switch_file_handle_t *handle, switch_audio_col_t col, const char *string)
{
	return SWITCH_STATUS_FALSE;
}

static switch_status_t webm_file_get_string(switch_file_handle_t *handle, switch_audio_col_t col, const char **string)
{
	return SWITCH_STATUS_FALSE;
}

static char *supported_formats[2] = { 0 };

SWITCH_MODULE_LOAD_FUNCTION(mod_webm_load)
{
	switch_file_interface_t *file_interface;

	supported_formats[0] = (char *)"webm";

	/* connect my internal structure to the blank pointer passed to me */
	*module_interface = switch_loadable_module_create_module_interface(pool, modname);

	file_interface = (switch_file_interface_t *)switch_loadable_module_create_interface(*module_interface, SWITCH_FILE_INTERFACE);
	file_interface->interface_name = modname;
	file_interface->extens = supported_formats;
	file_interface->file_open = webm_file_open;
	file_interface->file_close = webm_file_close;
	file_interface->file_truncate = webm_file_truncate;
	file_interface->file_read = webm_file_read;
	file_interface->file_write = webm_file_write;
	file_interface->file_read_video = webm_file_read_video;
	file_interface->file_write_video = webm_file_write_video;
	file_interface->file_seek = webm_file_seek;
	file_interface->file_set_string = webm_file_set_string;
	file_interface->file_get_string = webm_file_get_string;

	mod_vorbis_load(module_interface, pool);

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
 * vim:set softtabstop=4 shiftwidth=4 tabstop=4 noet:
 */
