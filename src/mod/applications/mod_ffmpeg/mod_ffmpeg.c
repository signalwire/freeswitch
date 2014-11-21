/*
 * FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
 * Copyright (C) 2005-2013, Anthony Minessale II <anthm@freeswitch.org>
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
 * Seven Du <dujinfang@gmail.com>
 *
 * mod_ffmpeg -- FS Video File Format
 *
 */

/* compile: need to remove the -pedantic option from modmake.rules to compile */

#include <switch.h>
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/opt.h>
#include <libavutil/imgutils.h>

/* use libx264, comment for use the ffmpeg/avcodec wrapper */
#define H264_CODEC_USE_LIBX264
#define SLICE_SIZE SWITCH_DEFAULT_VIDEO_SIZE

#ifdef H264_CODEC_USE_LIBX264
#include <x264.h>
#endif

#define FPS 15 // frame rate
#define BUFFERSIZE 4096 * 2

SWITCH_MODULE_LOAD_FUNCTION(mod_ffmpeg_load);
SWITCH_MODULE_DEFINITION(mod_ffmpeg, mod_ffmpeg_load, NULL, NULL);

/*  ff_avc_find_startcode is not esposed in the ffmpeg lib but you can use it
	Either include the avc.h which available in the ffmpeg source, or
	just add the declaration like we does following to avoid include that whole avc.h
	The function is implemented in avc.h, guess we'll get rid of this later if we can directly use libx264

#include <libavformat/avc.h>
*/

const uint8_t *ff_avc_find_startcode(const uint8_t *p, const uint8_t *end);

#if 0 // this is not fully tested and supported
struct record_helper {
	switch_core_session_t *session;
	switch_mutex_t *mutex;
	AVFormatContext *avctx;
	AVStream *video_st;
	AVFrame *avframe;
	int up;

	int seq;
	switch_socket_t *sock;
	switch_sockaddr_t *sock_addr;
};

struct ffmpeg_context {
	switch_mutex_t *mutex;
	switch_buffer_t *buf;
	AVFormatContext *avctx;
	AVStream *video_st;
	AVStream *audio_st;
	AVFrame *aframe;
	AVCodec *acodec;
	int file_opened;
	int video_pts;
};

typedef struct ffmpeg_context ffmpeg_context;

static AVStream *add_audio_stream(AVFormatContext *context, enum AVCodecID codec_id)
{
	AVCodecContext *cc;
	AVStream *st;
	AVCodec *codec;

	/* find the audio encoder */
	codec = avcodec_find_encoder(codec_id);
	if (!codec) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "codec not found\n");
		return NULL;
	}

	st = avformat_new_stream(context, codec);

	if (!st) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Could not alloc audio stream\n");
		return NULL;
	}

	st->id = 1;

	cc = st->codec;
	cc->codec = codec;

	/* put sample parameters */
	cc->sample_fmt  = AV_SAMPLE_FMT_S16;
	cc->bit_rate    = 64000;
	cc->sample_rate = 44100;
	cc->channels    = 1;

	// some formats want stream headers to be separate
	if (context->oformat->flags & AVFMT_GLOBALHEADER) {
		cc->flags |= CODEC_FLAG_GLOBAL_HEADER;
	}

	return st;
}

static AVStream *add_video_stream(AVFormatContext *context, enum AVCodecID codec_id)
{
	AVCodecContext *cc;
	AVStream *st;
	AVCodec *codec;

	/* find the video encoder */
	codec = avcodec_find_encoder(codec_id);
	if (!codec) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "codec not found\n");
		return NULL;
	}

	st = avformat_new_stream(context, codec);
	if (!st) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Could not alloc stream\n");
		return NULL;
	}

	cc = st->codec;

	avcodec_get_context_defaults3(cc, codec);

	cc->codec_id = codec_id;

	/* Put sample parameters. */
	// c->bit_rate = 400000;
	/* Resolution must be a multiple of two. */
	cc->width    = 352;
	cc->height   = 288;
	/* timebase: This is the fundamental unit of time (in seconds) in terms
	 * of which frame timestamps are represented. For fixed-fps content,
	 * timebase should be 1/framerate and timestamp increments should be
	 * identical to 1. */
	cc->time_base.den = FPS;
	cc->time_base.num = 1;
	cc->gop_size      = 12; /* emit one intra frame every twelve frames at most */
	cc->pix_fmt       = PIX_FMT_YUV420P;
	cc->max_b_frames = 0;

	/* Some formats want stream headers to be separate. */
	if (context->oformat->flags & AVFMT_GLOBALHEADER) {
		cc->flags |= CODEC_FLAG_GLOBAL_HEADER;
	}

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "flags %x %x\n", context->oformat->flags, cc->flags);

	return st;
}

static switch_status_t ffmpeg_open(switch_file_handle_t *handle, const char *path)
{
	ffmpeg_context *context;
	char *ext;
	unsigned int flags = 0;
	AVFormatContext *avctx = NULL;
	AVOutputFormat *fmt;
	AVStream *audio_st=NULL;
	AVStream *video_st=NULL;
	AVCodecContext *audio_codec;
	AVFrame *aframe = NULL;

	if ((ext = strrchr(path, '.')) == 0) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Invalid Format\n");
		return SWITCH_STATUS_GENERR;
	}

	ext++;

	if ((context = switch_core_alloc(handle->memory_pool, sizeof(ffmpeg_context))) == 0) {
		return SWITCH_STATUS_MEMERR;
	}

	memset(context, 0, sizeof(ffmpeg_context));

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

//write only

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "sample rate: %d\n", handle->samplerate);

	handle->samplerate = 8000;

	aframe = av_frame_alloc();

	if (!aframe) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "memory error!\n");
		goto end;
	}

	context->aframe = aframe;

	av_register_all();

	//avformat_alloc_output_context2(&avctx, NULL, NULL, path);
	avctx = NULL;

	if (!avctx) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Could not deduce output format from file extension: using mpeg4.\n");
		//avformat_alloc_output_context2(&avctx, NULL, "mpeg4", path);
	}

	if (!avctx) {
		goto end;
	}

	context->avctx = avctx;

	fmt = avctx->oformat;

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "audio_codec: 0x%x video_codec: 0x%x\n", fmt->audio_codec, fmt->video_codec);

	if (fmt->audio_codec != AV_CODEC_ID_NONE) {
		audio_st = add_audio_stream(avctx, fmt->audio_codec);
	}

	if (fmt->video_codec != AV_CODEC_ID_NONE) {
		video_st = add_video_stream(avctx, fmt->video_codec);
	}

	if (!video_st) {
		if (!audio_st) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "No audio & video stream!\n");
			goto end;
		}
	}

	context->video_st = video_st;

	audio_codec = audio_st->codec;
	/* open it */
	if (avcodec_open2(audio_codec, NULL, NULL) < 0) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "could not open audio codec\n");
		goto end;
	}

#if 1
	av_dump_format(avctx, 0, path, 1);
#endif

	/* open the output file, if needed */
	if (!(fmt->flags & AVFMT_NOFILE)) {
		if (avio_open(&avctx->pb, path, AVIO_FLAG_WRITE) < 0) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Could not open [%s]\n", path);
			goto end;
		}
		context->file_opened = 1;
	}

	/* Write the stream header, if any. */
	avformat_write_header(avctx, NULL);

	context->audio_st = audio_st;

	if (switch_test_flag(handle, SWITCH_FILE_WRITE_APPEND)) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "unsupported append\n");
	}

	handle->samples = 0;
	handle->samplerate = 8000;
	handle->channels = 1;
	handle->format = 0;
	handle->sections = 0;
	handle->seekable = 0;
	handle->speed = 0;
	handle->pos = 0;
	handle->private_info = context;

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "Opening File [%s] %dhz\n", path, handle->samplerate);

	if (switch_test_flag(handle, SWITCH_FILE_FLAG_WRITE)) {

	}

	switch_buffer_create_dynamic(&context->buf, 512, 512, 1024000);

	return SWITCH_STATUS_SUCCESS;

end:

	if (avctx) {
		int i;
		for (i = 0; i < avctx->nb_streams; i++) {
			avcodec_close(avctx->streams[i]->codec);
			av_freep(&avctx->streams[i]->codec);
			av_freep(&avctx->streams[i]);
		}

		if (context->file_opened) {
			/* Close the output file. */
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "closing file\n");
			avio_close(avctx->pb);
		}

		av_free(avctx);
 
		if (aframe) av_free(aframe);
	}

	return SWITCH_STATUS_FALSE;
}

static switch_status_t ffmpeg_truncate(switch_file_handle_t *handle, int64_t offset)
{
	// ffmpeg_context *context = handle->private_info;
	switch_status_t status = SWITCH_STATUS_FALSE;
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "unsupported truncate\n");
	return status;
}

static switch_status_t ffmpeg_close(switch_file_handle_t *handle)
{
	ffmpeg_context *context = handle->private_info;
	AVFormatContext *avctx = context->avctx;
	int i;

	av_write_trailer(avctx);

	for (i = 0; i < avctx->nb_streams; i++) {
		avcodec_close(avctx->streams[i]->codec);
		av_freep(&avctx->streams[i]->codec);
		av_freep(&avctx->streams[i]);
	}

	if (context->file_opened) {
		/* Close the output file. */
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "closing file\n");
		avio_close(avctx->pb);
	}

	av_free(avctx);
 
	if (context->aframe) av_free(context->aframe);

	switch_buffer_destroy(&context->buf);

	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t ffmpeg_seek(switch_file_handle_t *handle, unsigned int *cur_sample, int64_t samples, int whence)
{
	// switch_status_t status;
	//
	// ffmpeg_context *context = handle->private_info;
	//
	// status = switch_file_seek(context->fd, whence, &samples);
	// if (status == SWITCH_STATUS_SUCCESS) {
	// 	handle->pos += samples;
	// }
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "unsupported seek\n");
	return SWITCH_STATUS_FALSE;
}

static switch_status_t ffmpeg_read(switch_file_handle_t *handle, void *data, size_t *len)
{
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "read not implemented\n");
	return SWITCH_STATUS_FALSE;
}

static switch_status_t ffmpeg_write(switch_file_handle_t *handle, void *data, size_t *len)
{
	ffmpeg_context *context = handle->private_info;
	switch_status_t status = SWITCH_STATUS_SUCCESS;
	AVFormatContext *avctx = context->avctx;
	AVFrame *aframe = context->aframe;
	AVStream *audio_st = context->audio_st;
	AVCodecContext *codec = audio_st->codec;
	AVPacket pkt = { 0 };
	int got_packet = 0;

	aframe->nb_samples = 160;  //fixme

	avcodec_fill_audio_frame(aframe, codec->channels, codec->sample_fmt,
						(uint8_t *)data,
						(*len) * codec->channels,
						1);

	av_init_packet(&pkt);

	// avcodec_encode_audio2(codec, &pkt, aframe, &got_packet);

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "got packet %d, size: %d\n", got_packet, pkt.size);

	if (!got_packet || pkt.size < 1) return status;

	pkt.stream_index = audio_st->index;

	/* Write the compressed frame to the media file. */
	switch_mutex_lock(context->mutex);
	if (av_interleaved_write_frame(avctx, &pkt) != 0) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Error while writing audio frame\n");
		switch_mutex_unlock(context->mutex);
		return SWITCH_STATUS_FALSE;
	}
	switch_mutex_unlock(context->mutex);

	// *len = ? //fixme
	return status;
}

static switch_status_t ffmpeg_write_video(switch_file_handle_t *handle, void *data, size_t *len)
{
	uint32_t datalen = *len - 12;
	switch_status_t status = SWITCH_STATUS_SUCCESS;
	uint8_t *hdr = NULL;
	switch_rtp_hdr_t *rtp_hdr = data;
	ffmpeg_context *context = handle->private_info;

	uint8_t code[] = {0, 0, 1};

	AVFormatContext *avctx = context->avctx;
	AVStream *video_st = context->video_st;
	AVPacket pkt;

	int key_frame = 0;
	int ret;

	av_init_packet(&pkt);

	hdr = (uint8_t *)data + 12;

	switch_buffer_write(context->buf, &code, 3);
	switch_buffer_write(context->buf, hdr, datalen);


#if 1
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "read %d %02x %02x %02x %d\n",
			rtp_hdr->m, *hdr, *(hdr+1), *(hdr+2), datalen);
#endif

	if (!rtp_hdr->m) return status;; // continue buffering until we get a marker

	{
		int nal_type = (*hdr &0x1f);
		key_frame = (nal_type == 5);
	}

	pkt.pts = context->video_pts++;

	if (key_frame) {
		pkt.flags |= AV_PKT_FLAG_KEY;
	} else {
		pkt.flags &= ~AV_PKT_FLAG_KEY;
	}

	pkt.stream_index = video_st->index;
	pkt.size         = switch_buffer_inuse(context->buf);
	switch_buffer_peek_zerocopy(context->buf, (const void **)&pkt.data);

		/* Write the compressed frame to the media file. */
		
	switch_mutex_lock(context->mutex);

	if ((ret = av_write_frame(avctx, &pkt)) != 0) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "failed to write %d bytes, ret=%d\n", pkt.size, ret);
		switch_mutex_unlock(context->mutex);
		return SWITCH_STATUS_FALSE;
	}
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "write %d bytes, ret=%d\n", pkt.size, ret);

	switch_mutex_unlock(context->mutex);
	switch_buffer_zero(context->buf);

	// *len = datalen; // FIXME

	return status;
}

static switch_status_t ffmpeg_set_string(switch_file_handle_t *handle, switch_audio_col_t col, const char *string)
{
	return SWITCH_STATUS_FALSE;
}

static switch_status_t ffmpeg_get_string(switch_file_handle_t *handle, switch_audio_col_t col, const char **string)
{
	return SWITCH_STATUS_FALSE;
}

static void *SWITCH_THREAD_FUNC ffmpeg_record_video_thread(switch_thread_t *thread, void *obj)
{
	struct record_helper *eh = obj;
	switch_core_session_t *session = eh->session;
	switch_channel_t *channel = switch_core_session_get_channel(session);
	switch_status_t status;
	switch_frame_t *read_frame;
	switch_buffer_t *buf = NULL;

	AVFormatContext *avctx = eh->avctx;
	AVStream *video_st = eh->video_st;
	AVPacket pkt;

	int key_frame = 0;
	uint8_t *hdr;
	uint64_t pts = 0;

	av_init_packet(&pkt);

	if (switch_buffer_create(switch_core_session_get_pool(eh->session), &buf, BUFFERSIZE) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "memory buffer error!\n");
		goto end;
	}

	eh->up = 1;

	while (switch_channel_ready(channel) && eh->up) {
		int ret;
		uint8_t code[] = {0, 0, 1};

		status = switch_core_session_read_video_frame(session, &read_frame, SWITCH_IO_FLAG_NONE, 0);

		if (!SWITCH_READ_ACCEPTABLE(status)) {
			break;
		}

		if (switch_test_flag(read_frame, SFF_CNG)) {
			continue;
		}

		switch_core_session_write_video_frame(session, read_frame, SWITCH_IO_FLAG_NONE, 0);

		switch_buffer_write(buf, &code, 3);
		switch_buffer_write(buf, read_frame->data, read_frame->datalen);

		hdr = read_frame->data;

#if 0
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "read %d %02x %02x %02x %d\n",
			read_frame->m, *hdr, *(hdr+1), *(hdr+2), read_frame->datalen);
#endif

		if (!read_frame->m) continue; // continue buffering until we get a marker

		{
			int nal_type = (*hdr &0x1f);
			key_frame = (nal_type == 5);
		}

		// if (oc->coded_frame->pts != AV_NOPTS_VALUE) {
		// pts = av_rescale_q(read_frame->timestamp, video_st->codec->time_base, video_st->time_base);        	
		// pkt.pts = read_frame->timestamp;

		pkt.pts = pts++;

		if (key_frame) {
			pkt.flags |= AV_PKT_FLAG_KEY;
		} else {
			pkt.flags &= ~AV_PKT_FLAG_KEY;
		}

		pkt.stream_index = video_st->index;
		pkt.size         = switch_buffer_inuse(buf);
		switch_buffer_peek_zerocopy(buf, (const void **)&pkt.data);

		/* Write the compressed frame to the media file. */
		
		switch_mutex_lock(eh->mutex);

		if ((ret = av_write_frame(avctx, &pkt)) != 0) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "failed to write %d bytes, ret=%d\n", pkt.size, ret);
			switch_mutex_unlock(eh->mutex);
			break;
		}

		switch_mutex_unlock(eh->mutex);
		switch_buffer_zero(buf);

	}

	if (video_st) {
		avcodec_close(video_st->codec);
	}

end:

	if (buf) switch_buffer_destroy(&buf);

	eh->up = 0;
	return NULL;
}

SWITCH_STANDARD_APP(record_ffmpeg_function)
{
	switch_status_t status;
	switch_frame_t *read_frame;
	switch_channel_t *channel = switch_core_session_get_channel(session);
	struct record_helper eh = { 0 };
	switch_thread_t *thread;
	switch_threadattr_t *thd_attr = NULL;
	switch_mutex_t *mutex = NULL;
	switch_codec_t codec;
	//switch_codec_t *vid_codec;
	switch_codec_implementation_t read_impl = { 0 };
	switch_dtmf_t dtmf = { 0 };
	int count = 0, sanity = 30;

	//ffmpeg
	AVFormatContext *avctx;
	AVOutputFormat *fmt=NULL;
	AVStream *audio_st=NULL;
	AVCodecContext *audio_codec;
	AVFrame *avframe=NULL;

	/* Tell the channel to request a fresh vid frame */

	switch_core_session_get_read_impl(session, &read_impl);
	switch_channel_answer(channel);
	switch_core_session_refresh_video(session);

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

	if (read_impl.ianacode != 70) {
		if (switch_core_codec_init(&codec,
								   "L16",
								   NULL,
								   8000,
								   20,
								   1, SWITCH_CODEC_FLAG_ENCODE | SWITCH_CODEC_FLAG_DECODE,
								   NULL, switch_core_session_get_pool(session)) == SWITCH_STATUS_SUCCESS) {
			switch_core_session_set_read_codec(session, &codec);
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_INFO, "Codec Activation Success\n");
		} else {
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "Codec Activation Fail\n");
			goto end;
		}
	}

	avframe = av_frame_alloc();

	if (!avframe) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "memory error!\n");
		goto end;
	}

	av_register_all();

	//avformat_alloc_output_context2(&avctx, NULL, NULL, data);
	avctx = NULL;

	if (!avctx) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Could not deduce output format from file extension: using mpeg4.\n");
		//avformat_alloc_output_context2(&avctx, NULL, "mpeg4", data);
	}

	if (!avctx) {
		goto end;
	}

	fmt = avctx->oformat;

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "audio_codec: 0x%x video_codec: 0x%x\n", fmt->audio_codec, fmt->video_codec);

	if (fmt->audio_codec != AV_CODEC_ID_NONE) {
		audio_st = add_audio_stream(avctx, fmt->audio_codec);
	}

	if (switch_channel_test_flag(channel, CF_VIDEO)) {

		if (fmt->video_codec != AV_CODEC_ID_NONE) {
			eh.video_st = add_video_stream(avctx, fmt->video_codec);
		}

		if (!eh.video_st) {
			if (!audio_st) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "No audio & video stream!\n");
				goto end;
			}
		}
	}

	audio_codec = audio_st->codec;

	/* open it */
	if (avcodec_open2(audio_codec, NULL, NULL) < 0) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "could not open audio codec\n");
		goto end;
	}

#if 1
	av_dump_format(avctx, 0, data, 1);
#endif

	/* open the output file, if needed */
	if (!(fmt->flags & AVFMT_NOFILE)) {
		if (avio_open(&avctx->pb, data, AVIO_FLAG_WRITE) < 0) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Could not open [%s]\n", data);
			goto end;
		}
	}

	/* Write the stream header, if any. */
	avformat_write_header(avctx, NULL);

	if (switch_channel_test_flag(channel, CF_VIDEO)) {
		//vid_codec = switch_core_session_get_video_read_codec(session);

		switch_mutex_init(&mutex, SWITCH_MUTEX_NESTED, switch_core_session_get_pool(session));
		eh.mutex = mutex;
		eh.avctx = avctx;
		eh.session = session;
		switch_threadattr_create(&thd_attr, switch_core_session_get_pool(session));
		switch_threadattr_detach_set(thd_attr, 1);
		switch_threadattr_stacksize_set(thd_attr, SWITCH_THREAD_STACKSIZE);
		switch_thread_create(&thread, thd_attr, ffmpeg_record_video_thread, &eh, switch_core_session_get_pool(session));
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

			if (terminators && !strcasecmp(terminators, "none"))
			{
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

		if (switch_test_flag(read_frame, SFF_CNG)) {
			continue;
		}

		switch_core_session_write_frame(session, read_frame, SWITCH_IO_FLAG_NONE, 0);

		avframe->nb_samples = 160;

		avcodec_fill_audio_frame(avframe, audio_codec->channels, audio_codec->sample_fmt,
							(uint8_t *)read_frame->data,
							read_frame->datalen * audio_codec->channels,
							1);

		{
			AVPacket pkt = { 0 };
			int got_packet;

			av_init_packet(&pkt);

			avcodec_encode_audio2(audio_codec, &pkt, avframe, &got_packet);

			// switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "got packet %d, size: %d\n", got_packet, pkt.size);

			if (!got_packet || pkt.size < 1) continue;

			pkt.stream_index = audio_st->index;

			/* Write the compressed frame to the media file. */
			switch_mutex_lock(eh.mutex);
			if (av_interleaved_write_frame(avctx, &pkt) != 0) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Error while writing audio frame\n");
				switch_mutex_unlock(eh.mutex);
				break;
			}
			switch_mutex_unlock(eh.mutex);

		}
	}

	av_write_trailer(avctx);

	switch_channel_set_variable(channel, SWITCH_CURRENT_APPLICATION_RESPONSE_VARIABLE, "OK");

end:

	switch_core_session_set_read_codec(session, NULL);

	if (switch_core_codec_ready(&codec)) {
		switch_core_codec_destroy(&codec);
	}

	if (eh.up) {
		while (eh.up) {
			switch_cond_next();
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "waiting video thread to stop\n");
		}
	}

	if (audio_st) avcodec_close(audio_st->codec);

	{
		int i;
		for (i = 0; i < avctx->nb_streams; i++) {
			av_freep(&avctx->streams[i]->codec);
			av_freep(&avctx->streams[i]);
		}
	}

	if (!(fmt->flags & AVFMT_NOFILE)) {
		/* Close the output file. */
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "closing file [%s]\n", data);
		avio_close(avctx->pb);
	}

	if (avctx) av_free(avctx);
	if (avframe) av_free(avframe);

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "==== done =========  done ==== \n");

}

#define IP "192.168.1.1"

static int write_packet(void *opaque, uint8_t *buf, int size) {
	struct record_helper *eh = opaque;
	switch_size_t bytes = size;
	int i = -1;
	switch_rtp_hdr_t *hdr;
	switch_status_t status;

	while (size > 188) {
		if (size > 188 * 7) {
			bytes = 188 * 7;
			size -= 188 * 7;
			i++;
		} else {
			bytes = size;
			i++;
			size = 0;
		}

		hdr = (switch_rtp_hdr_t *)(buf + 188 * 7 * i - 12);
		memset(hdr, 0, 12);

		hdr->ssrc = 0x1111;
		hdr->ts = switch_time_now();
		hdr->ts = htonl(hdr->ts);
		hdr->version = 0x2;
		hdr->seq = eh->seq++;
		hdr->pt = 33;

		if (bytes < 188 * 7) hdr->m = 1;

		bytes += 12;

		status = switch_socket_sendto(eh->sock, eh->sock_addr, 0, (void *)hdr , &bytes);

		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "========== sending %ld %d\n", bytes, status);

	}

	return size;
}

static void *SWITCH_THREAD_FUNC stream_ts_thread(switch_thread_t *thread, void *obj)
{
	struct record_helper *eh = obj;
	switch_core_session_t *session = eh->session;
	switch_channel_t *channel = switch_core_session_get_channel(session);
	switch_status_t status;
	switch_frame_t *read_frame;
	switch_buffer_t *buf;

	AVFormatContext *oc;
	AVOutputFormat *fmt;
	AVStream *video_st = NULL; // better to be NULL than uninitialised
	AVPacket pkt;
	uint8_t *buf_callback = (uint8_t *)av_malloc(BUFFERSIZE);
	AVIOContext *io; 

	// uint8_t *fragment_type;
	int key_frame = 0;
	// int nal_type;
	uint8_t *hdr;
	uint64_t pts = 0;

	eh->seq = 0;

	io = avio_alloc_context(buf_callback + 12, BUFFERSIZE, 1, eh, NULL, write_packet, NULL);

	if (io == NULL) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "IO is NULL\n");
		return NULL;
	}

	if (switch_socket_create(&eh->sock, AF_INET, SOCK_DGRAM, 0, switch_core_session_get_pool(session)) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "Socket Error 1\n");
		return NULL;
	}

	if (switch_sockaddr_info_get(&eh->sock_addr, IP, SWITCH_UNSPEC,
								 11000, 0, switch_core_session_get_pool(session)) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "Socket Error 3\n");
		return NULL;
	}

	if (switch_buffer_create(switch_core_session_get_pool(eh->session), &buf, 65536) != SWITCH_STATUS_SUCCESS) return NULL;

	av_register_all();

	av_init_packet(&pkt);


	fmt = av_guess_format(NULL, "filename.ts", NULL); // CODEC_ID_H264 -> mp4, mov; CODEC_ID_THEORA -> ogg; CODEC_ID_MPEG4 -> mpegts, avi; CODEC_ID_VP8 -> webm

	if (!fmt) {
		printf("Could not deduce output format from file extension: using MP4.\n");
		fmt = av_guess_format("mpeg4", NULL, NULL);
	}
	if (!fmt) {
		printf("av_guess_format Error!\n");
		return NULL;
	}


	oc = avformat_alloc_context();

	if (!oc) {
		printf("Could not deduce output format from file extension: using MP4.\n");

		return NULL;
	}

	oc->pb = io;
	oc->oformat = fmt;

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "x:%d y:%d\n", AV_CODEC_ID_NONE, fmt->video_codec);

	if (fmt->video_codec != AV_CODEC_ID_NONE) {
		video_st = add_video_stream(oc, fmt->video_codec);
	}

	// av_dump_format(oc, 0, filename, 1);

	/* open the output file, if needed */
	// if (!(fmt->flags & AVFMT_NOFILE)) {
	//     if (avio_open(&oc->pb, filename, AVIO_FLAG_WRITE) < 0) {
	//         fprintf(stderr, "Could not open '%s'\n", filename);
	//         return NULL;
	//     }
	// }

	/* Write the stream header, if any. */
	avformat_write_header(oc, NULL);

	eh->up = 1;


	while (switch_channel_ready(channel) && eh->up) {
		int ret;
		uint8_t code[] = {0, 0, 1};

		status = switch_core_session_read_video_frame(session, &read_frame, SWITCH_IO_FLAG_NONE, 0);

		if (!SWITCH_READ_ACCEPTABLE(status)) {
			break;
		}

		if (switch_test_flag(read_frame, SFF_CNG)) {
			continue;
		}

		switch_core_session_write_video_frame(session, read_frame, SWITCH_IO_FLAG_NONE, 0);

		switch_buffer_write(buf, &code, 3);
		switch_buffer_write(buf, read_frame->data, read_frame->datalen);

		hdr = read_frame->data;

		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "read %d %02x %02x %02x %d\n",
			read_frame->m, *hdr, *(hdr+1), *(hdr+2), read_frame->datalen);

		if (!read_frame->m) continue; // continue buffering until we get a marker

		{
			int nal_type = (*hdr &0x1f);
			key_frame = (nal_type == 5);
		}

		// if (oc->coded_frame->pts != AV_NOPTS_VALUE) {
		// pts = av_rescale_q(read_frame->timestamp, video_st->codec->time_base, video_st->time_base);        	
		// pkt.pts = read_frame->timestamp;

		pkt.pts = pts;
		pts+=1;

		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "pts %llu\n", (long long unsigned int)pkt.pts);
		// }

		if (key_frame) {
			pkt.flags |= AV_PKT_FLAG_KEY;
		} else {
			pkt.flags &= ~AV_PKT_FLAG_KEY;
		}

		pkt.stream_index = video_st->index;
		pkt.size         = switch_buffer_inuse(buf);
		switch_buffer_peek_zerocopy(buf, (const void **)&pkt.data);

		/* Write the compressed frame to the media file. */
		// ret = av_interleaved_write_frame(oc, &pkt);
		ret = av_write_frame(oc, &pkt);

		switch_buffer_zero(buf);

		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "write: %d, ret %d\n", pkt.size, ret);

	}

	av_write_trailer(oc);

	// if (0 && video_st) close_video(oc, video_st);

	{int i;
	for (i = 0; i < oc->nb_streams; i++) {
		av_freep(&oc->streams[i]->codec);
		av_freep(&oc->streams[i]);
	}}

	if (eh->sock) {
		switch_socket_close(eh->sock);
	}

switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "flags %d\n", fmt->flags);

 //    if (!(fmt->flags & AVFMT_NOFILE)) {
 //        /* Close the output file. */
 //        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "closing output file\n");
 //        avio_close(oc->pb);
	// }

	// av_free(oc);

	// av_free(fmt); // fixme? this will be freed with the following IO
	av_free(io);
	av_free(buf_callback);
	switch_buffer_destroy(&buf);
	eh->up = 0;
	return NULL;
}

SWITCH_STANDARD_APP(stream_ts_function)
{
	switch_status_t status;
	switch_frame_t *read_frame;
	switch_channel_t *channel = switch_core_session_get_channel(session);
	struct record_helper eh = { 0 };
	switch_thread_t *thread;
	switch_threadattr_t *thd_attr = NULL;
	// int fd;
	switch_mutex_t *mutex = NULL;
	//switch_codec_t *vid_codec;
	switch_codec_implementation_t read_impl = { 0 };
	switch_dtmf_t dtmf = { 0 };
	int count = 0, sanity = 30;
	switch_core_session_message_t msg = { 0 };

	/* Tell the channel to request a fresh vid frame */
	msg.from = __FILE__;
	msg.message_id = SWITCH_MESSAGE_INDICATE_VIDEO_REFRESH_REQ;

	switch_core_session_get_read_impl(session, &read_impl);
	switch_channel_answer(channel);
	switch_core_session_receive_message(session, &msg);

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

	if (switch_channel_test_flag(channel, CF_VIDEO)) {
		//vid_codec = switch_core_session_get_video_read_codec(session);

		switch_mutex_init(&mutex, SWITCH_MUTEX_NESTED, switch_core_session_get_pool(session));
		eh.mutex = mutex;
		// eh.fd = fd;
		eh.session = session;
		switch_threadattr_create(&thd_attr, switch_core_session_get_pool(session));
		switch_threadattr_detach_set(thd_attr, 1);
		switch_threadattr_stacksize_set(thd_attr, SWITCH_THREAD_STACKSIZE);
		switch_thread_create(&thread, thd_attr, stream_ts_thread, &eh, switch_core_session_get_pool(session));
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

			if (terminators && !strcasecmp(terminators, "none"))
			{
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

		if (switch_test_flag(read_frame, SFF_CNG)) {
			continue;
		}

		switch_core_session_write_frame(session, read_frame, SWITCH_IO_FLAG_NONE, 0);
	}

	switch_channel_set_variable(channel, SWITCH_CURRENT_APPLICATION_RESPONSE_VARIABLE, "OK");

  // end:

	if (eh.up) {
		while (eh.up) {
			switch_cond_next();
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "waiting video thread to be end\n");
		}
	}
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "==== done =========   ts done ==== \n");

}

#endif

/* codec interface */

#define H264_NALU_BUFFER_SIZE 65536
#define MAX_NALUS 100

typedef struct our_h264_nalu_s
{
	const uint8_t *start;
	const uint8_t *eat;
	uint32_t len;
} our_h264_nalu_t;

typedef struct h264_codec_context_s {
	switch_buffer_t *nalu_buffer;
	AVCodec *decoder;
	AVCodec *encoder;
	AVCodecContext *decoder_ctx;
	AVCodecContext *encoder_ctx;
	int got_pps; /* if pps packet received */
	int64_t pts;
	int got_encoded_output;
	AVFrame *encoder_avframe;
	AVPacket encoder_avpacket;
	our_h264_nalu_t nalus[MAX_NALUS];
	int nalu_current_index;
	switch_size_t last_received_timestamp;
	switch_bool_t last_received_complete_picture;
	switch_image_t *img;

#ifdef H264_CODEC_USE_LIBX264
	/*x264*/

	x264_t *x264_handle;
	x264_param_t x264_params;
	x264_nal_t *x264_nals;
	int x264_nal_count;
	int cur_nalu_index;
	int need_key_frame;
#endif

} h264_codec_context_t;

#ifdef H264_CODEC_USE_LIBX264

#define RTP_SLICE_SIZE SWITCH_DEFAULT_VIDEO_SIZE //NALU Slice Size

static switch_status_t init_x264(h264_codec_context_t *context, uint32_t width, uint32_t height)
{
	x264_t *xh = context->x264_handle;
	x264_param_t *xp = &context->x264_params;
	int ret = 0;

	if (xh) {
		xp->i_width = width;
		xp->i_height = height;
		ret = x264_encoder_reconfig(xh, xp);

		if (ret == 0) return SWITCH_STATUS_SUCCESS;

		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "Cannot Reset error:%d\n", ret);
		return SWITCH_STATUS_FALSE;
	}

	x264_param_default(xp);
	// xp->i_level_idc = 31; // baseline
	// CPU Flags
	xp->i_threads  = 1;//X264_SYNC_LOOKAHEAD_AUTO;
	// xp->i_lookahead_threads = X264_SYNC_LOOKAHEAD_AUTO;
	// Video Properties
	xp->i_width	  = width;
	xp->i_height  = height;
	xp->i_frame_total = 0;
	xp->i_keyint_max = FPS * 10;
	// Bitstream parameters
	xp->i_bframe	 = 0;
	xp->i_frame_reference = 0;
	// xp->b_open_gop	= 0;
	// xp->i_bframe_pyramid = 0;
	// xp->i_bframe_adaptive = X264_B_ADAPT_TRELLIS;

	//xp->vui.i_sar_width = 1080;
	//xp->vui.i_sar_height = 720;
	// xp->i_log_level	 = X264_LOG_DEBUG;
	xp->i_log_level	 = X264_LOG_NONE;
	// Rate control Parameters
	xp->rc.i_bitrate = 378;//kbps
	// Muxing parameters
	xp->i_fps_den  = 1;
	xp->i_fps_num  = FPS;
	xp->i_timebase_den = xp->i_fps_num;
	xp->i_timebase_num = xp->i_fps_den;
	xp->i_slice_max_size = RTP_SLICE_SIZE;
	//Set Profile 0=baseline other than 1=MainProfile
	x264_param_apply_profile(xp, x264_profile_names[0]);
	xh = x264_encoder_open(xp);

	if (!xh) return SWITCH_STATUS_FALSE;

	// copy params back to xp;
	x264_encoder_parameters(xh, xp);
	context->x264_handle = xh;
	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t nalu_slice(h264_codec_context_t *context, switch_frame_t *frame)
{
	int nalu_len;
	uint8_t *buffer;
	int start_code_len = 3;
	x264_nal_t *nal = &context->x264_nals[context->cur_nalu_index];
	switch_status_t status = SWITCH_STATUS_SUCCESS;

	//*flag &= ~SFF_MARKER;
	frame->m = 0;

	if (context->cur_nalu_index >= context->x264_nal_count) {
		frame->datalen = 0;
		frame->m = 0;
		context->cur_nalu_index = 0;
		return SWITCH_STATUS_NOTFOUND;
	}

	if (nal->b_long_startcode) start_code_len++;

	nalu_len = nal->i_payload - start_code_len;
	buffer = nal->p_payload + start_code_len;

	// switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "nalu:%d/%d nalu_len:%d\n",
		// context->cur_nalu_index, context->x264_nal_count, nalu_len);

	switch_assert(nalu_len > 0);

	// if ((*buffer & 0x1f) == 7) switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Got SPS\n");

	memcpy(frame->data, buffer, nalu_len);
	frame->datalen = nalu_len;
	if (context->cur_nalu_index == context->x264_nal_count - 1) {
		frame->m = 1;
	} else {
		status = SWITCH_STATUS_MORE_DATA;
	}

	context->cur_nalu_index++;

	return status;
}

#endif

static uint8_t ff_input_buffer_padding[FF_INPUT_BUFFER_PADDING_SIZE] = { 0 };

static void buffer_h264_nalu(h264_codec_context_t *context, switch_frame_t *frame)
{
	//	uint8_t nalu_idc = 0;
	uint8_t nalu_type = 0;
	uint8_t *data = frame->data;
	uint8_t nalu_hdr = *data;
	uint8_t sync_bytes[] = {0, 0, 0, 1};
	switch_buffer_t *buffer = context->nalu_buffer;

	if (!frame) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "no frame in codec!!\n");
		return;
	}

	//	nalu_idc = (nalu_hdr & 0x60) >> 5;
	nalu_type = nalu_hdr & 0x1f;

	// switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "%02x %d\n", nalu_hdr, frame->datalen);

	if (!context->got_pps && nalu_type != 7) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "waiting pps\n");
		switch_set_flag(frame, SFF_WAIT_KEY_FRAME);
		return;
	}

	if (!context->got_pps) context->got_pps = 1;

	switch_buffer_write(buffer, sync_bytes, sizeof(sync_bytes));
	switch_buffer_write(buffer, frame->data, frame->datalen);

	if (frame->m) {
		switch_buffer_write(buffer, ff_input_buffer_padding, sizeof(ff_input_buffer_padding));
	}
}

#ifndef H264_CODEC_USE_LIBX264

static switch_status_t consume_nalu(h264_codec_context_t *context, void *data, uint32_t *len, uint32_t *flag)
{
	our_h264_nalu_t *nalu = &context->nalus[context->nalu_current_index];

	if (!nalu->start) {
		*len = 0;
		*flag = 1;
		if (context->encoder_avpacket.size > 0) av_free_packet(&context->encoder_avpacket);
		if (context->encoder_avframe->data) av_freep(&context->encoder_avframe->data[0]);
		context->nalu_current_index = 0;
		return SWITCH_STATUS_SUCCESS;
	}

	assert(nalu->len);

	if (nalu->len <= SLICE_SIZE) {
		uint8_t nalu_hdr = *(uint8_t *)(nalu->start);
		uint8_t nalu_type = nalu_hdr & 0x1f;

		memcpy(data, nalu->start, nalu->len);
		*len = nalu->len;
		*flag = (nalu_type == 6 || nalu_type == 7 || nalu_type == 8) ? 0 : 1;
		context->nalu_current_index++;
		return SWITCH_STATUS_SUCCESS;
	} else {
		uint8_t nalu_hdr = *(uint8_t *)(nalu->start);
		uint8_t nri = nalu_hdr & 0x60;
		uint8_t nalu_type = nalu_hdr & 0x1f;
		int left = nalu->len - (nalu->eat - nalu->start);
		uint8_t *p = data;

		if (left <= (1400 - 2)) {
			p[0] = nri | 28; // FU-A
			p[1] = 0x40 | nalu_type;
			memcpy(p+2, nalu->eat, left);
			nalu->eat += left;
			*len = left + 2;
			*flag = 1;
			context->nalu_current_index++;
			return SWITCH_STATUS_SUCCESS;
		} else {
			uint8_t start = nalu->start == nalu->eat ? 0x80 : 0;

			p[0] = nri | 28; // FU-A
			p[1] = start | nalu_type;
			if (start) nalu->eat++;
			memcpy(p+2, nalu->eat, 1400 - 2);
			nalu->eat += (1400 - 2);
			*len = 1400;
			*flag = 0;
			return SWITCH_STATUS_SUCCESS;
		}
	}
}

static switch_status_t open_encoder(h264_codec_context_t *context, uint32_t width, uint32_t height)
{

	context->encoder_ctx->width = width;
	context->encoder_ctx->height = height;

	if (avcodec_is_open(context->encoder_ctx)) avcodec_close(context->encoder_ctx);

	if (avcodec_open2(context->encoder_ctx, context->encoder, NULL) < 0) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Could not open codec\n");
		return SWITCH_STATUS_FALSE;
	}

	return SWITCH_STATUS_SUCCESS;
}
#endif

static switch_status_t switch_h264_init(switch_codec_t *codec, switch_codec_flag_t flags, const switch_codec_settings_t *codec_settings)
{
	int encoding, decoding;

	encoding = (flags & SWITCH_CODEC_FLAG_ENCODE);
	decoding = (flags & SWITCH_CODEC_FLAG_DECODE);

	if (!(encoding || decoding)) {
		return SWITCH_STATUS_FALSE;
	} else {
		h264_codec_context_t *context = NULL;
		if (codec->fmtp_in) {
			codec->fmtp_out = switch_core_strdup(codec->memory_pool, codec->fmtp_in);
		}

		context = switch_core_alloc(codec->memory_pool, sizeof(h264_codec_context_t));
		switch_assert(context);
		memset(context, 0, sizeof(*context));

		if (decoding) {
			context->decoder = avcodec_find_decoder(AV_CODEC_ID_H264);

			if (!context->decoder) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Cannot find codec AV_CODEC_ID_H264\n");
				goto error;
			}

			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "codec: id=%d %s\n", context->decoder->id, context->decoder->long_name);

			context->decoder_ctx = avcodec_alloc_context3(context->decoder);

			if (avcodec_open2(context->decoder_ctx, context->decoder, NULL) < 0) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Error openning codec\n");
				goto error;
			}
		}

		if (encoding) {
			context->encoder = avcodec_find_encoder(AV_CODEC_ID_H264);
			if (!context->encoder) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Cannot find AV_CODEC_ID_H264 decoder\n");
				goto skip;
			}

			context->encoder_ctx = avcodec_alloc_context3(context->encoder);
			if (!context->encoder_ctx) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Could not allocate video encoder context\n");
				goto error;
			}

			context->encoder_ctx->bit_rate = 400000;
			context->encoder_ctx->width = 352;
			context->encoder_ctx->height = 288;
			/* frames per second */
			context->encoder_ctx->time_base= (AVRational){1, FPS};
			context->encoder_ctx->gop_size = FPS * 3; /* emit one intra frame every 3 seconds */
			context->encoder_ctx->max_b_frames = 0;
			context->encoder_ctx->pix_fmt = AV_PIX_FMT_YUV420P;
			// context->encoder_ctx->thread_count = 1;
			// context->encoder_ctx->rtp_payload_size = RTP_SLICE_SIZE;
			av_opt_set(context->encoder_ctx->priv_data, "preset", "fast", 0);
		}

skip:

		switch_buffer_create_dynamic(&(context->nalu_buffer), H264_NALU_BUFFER_SIZE, H264_NALU_BUFFER_SIZE * 8, 0);
		codec->private_info = context;

		return SWITCH_STATUS_SUCCESS;
	}

error:
	// todo, do some clean up
	return SWITCH_STATUS_FALSE;
}

#ifndef H264_CODEC_USE_LIBX264

static void __attribute__((unused)) fill_avframe(AVFrame *pict, switch_image_t *img)
{
	int i;
	uint8_t *y = img->planes[0];
	uint8_t *u = img->planes[1];
	uint8_t *v = img->planes[2];

	/* Y */
	for (i = 0; i < pict->height; i++) {
		memcpy(&pict->data[0][i * pict->linesize[0]], y + i * img->stride[0], pict->width);
	}

	//U V
	for(i = 0; i < pict->height / 2; i++) {
		memcpy(&pict->data[1][i * pict->linesize[1]], u + i * img->stride[1], pict->width / 2);
		memcpy(&pict->data[2][i * pict->linesize[2]], v + i * img->stride[2], pict->width / 2);
	}

}

//										  switch_image_t *img,
//										  void *encoded_data, uint32_t *encoded_data_len,
//										  unsigned int *flag)

static switch_status_t switch_h264_encode(switch_codec_t *codec, switch_frame_t *frame)
{
	h264_codec_context_t *context = (h264_codec_context_t *)codec->private_info;
	AVCodecContext *avctx= context->encoder_ctx;
	int ret;
	int *got_output = &context->got_encoded_output;
	AVFrame *avframe;
	AVPacket *pkt = &context->encoder_avpacket;
	uint32_t width = 0;
	uint32_t height = 0;
	switch_image_t *img = frame->img;
	void *encoded_data = frame->data;
	uint32_t *encoded_data_len = frame->datalen;
	unsigned int *flag = &frame->flags;

	if (*encoded_data_len < SWITCH_DEFAULT_VIDEO_SIZE) return SWITCH_STATUS_FALSE;

	if (frame->flags & SFF_SAME_IMAGE) {
		// read from buffered
		return consume_nalu(context, encoded_data, encoded_data_len, flag);
	}

	width = img->d_w;
	height = img->d_h;

	if (!avcodec_is_open(avctx)) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "initializing encoder %dx%d\n", width, height);
		open_encoder(context, width, height);
	}

	if (avctx->width != width || avctx->height != height) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "picture size changed from %dx%d to %dx%d, reinitializing encoder",
			avctx->width, avctx->height, width, height);
		open_encoder(context, width, height);
	}

	av_init_packet(pkt);
	pkt->data = NULL;      // packet data will be allocated by the encoder
	pkt->size = 0;

	if (!context->encoder_avframe) context->encoder_avframe = avcodec_alloc_frame();

	avframe = context->encoder_avframe;

	if (!avframe) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Error allocate frame!\n");
		goto error;
	}

	avframe->format = avctx->pix_fmt;
	avframe->width  = avctx->width;
	avframe->height = avctx->height;

	/* the image can be allocated by any means and av_image_alloc() is
	 * just the most convenient way if av_malloc() is to be used */
	ret = av_image_alloc(avframe->data, avframe->linesize, avctx->width, avctx->height, avctx->pix_fmt, 32);
	if (ret < 0) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Could not allocate raw picture buffer\n");
		goto error;
	}

	switch_assert(encoded_data);

	if (*got_output) { // Could be more delayed frames
		ret = avcodec_encode_video2(avctx, pkt, NULL, got_output);
		if (ret < 0) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Encoding Error %d\n", ret);
			goto error;
		}

		if (*got_output) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Encoded frame %llu (size=%5d) nalu_type=0x%x %d\n", context->pts, pkt->size, *((uint8_t *)pkt->data +4), *got_output);
			goto process;
		}
	}

	fill_avframe(avframe, img);

	avframe->pts = context->pts++;

	/* encode the image */
	ret = avcodec_encode_video2(avctx, pkt, avframe, got_output);

	if (ret < 0) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Encoding Error %d\n", ret);
		goto error;
	}

process:

	if (*got_output) {
		const uint8_t *p = pkt->data;
		int i = 0;

		/* split into nalus */
		memset(context->nalus, 0, sizeof(context->nalus));

		while ((p = ff_avc_find_startcode(p, pkt->data+pkt->size)) < (pkt->data + pkt->size)) {
			if (!context->nalus[i].start) {
				while (!(*p++)) ; /* eat the sync bytes, what ever 0 0 1 or 0 0 0 1 */
				context->nalus[i].start = p;
				context->nalus[i].eat = p;
			} else {
				context->nalus[i].len = p - context->nalus[i].start;
				while (!(*p++)) ; /* eat the sync bytes, what ever 0 0 1 or 0 0 0 1 */
				i++;
				context->nalus[i].start = p;
				context->nalus[i].eat = p;
			}
			if (i >= MAX_NALUS - 2) break;
		}

		context->nalus[i].len = p - context->nalus[i].start;
		context->nalu_current_index = 0;
		return consume_nalu(context, encoded_data, encoded_data_len, flag);
	}

error:
	*encoded_data_len = 0;
	return SWITCH_STATUS_FALSE;
}

#endif

#ifdef H264_CODEC_USE_LIBX264

static switch_status_t switch_h264_encode(switch_codec_t *codec,
										  switch_frame_t *frame)
										  //switch_image_t *img,
										  //void *encoded_data, uint32_t *encoded_data_len,
										  //unsigned int *flag)
{
	h264_codec_context_t *context = (h264_codec_context_t *)codec->private_info;
	uint32_t width = 0;
	uint32_t height = 0;
	x264_picture_t pic = { 0 }, pic_out = { 0 };
	int result;
	switch_image_t *img = frame->img;
	void *encoded_data = frame->data;
	uint32_t *encoded_data_len = &frame->datalen;
	unsigned int *flag = &frame->flags;

	if (*flag & SFF_WAIT_KEY_FRAME) context->need_key_frame = 1;

	//if (*encoded_data_len < SWITCH_DEFAULT_VIDEO_SIZE) return SWITCH_STATUS_FALSE;

	if (!context) return SWITCH_STATUS_FALSE;

	if (!context->x264_handle) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "initializing x264 handle %dx%d\n", width, height);
		init_x264(context, width, height);
	}

	if (frame->flags & SFF_SAME_IMAGE) {
		return nalu_slice(context, frame);
	}

	width = img->d_w;
	height = img->d_h;

	if (context->x264_params.i_width != width || context->x264_params.i_height != height) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "picture size changed from %dx%d to %dx%d, reinitializing encoder",
			context->x264_params.i_width, context->x264_params.i_width, width, height);
		init_x264(context, width, height);
	}

	switch_assert(encoded_data);

	x264_picture_init(&pic);
	pic.img.i_csp = X264_CSP_I420;
	pic.img.i_plane = 3;
	pic.img.i_stride[0] = img->stride[0];
	pic.img.i_stride[1] = img->stride[1];
	pic.img.i_stride[2] = img->stride[2];
	pic.img.plane[0] = img->planes[0];
	pic.img.plane[1] = img->planes[1];
	pic.img.plane[2] = img->planes[2];
	// pic.i_pts = (context->pts++);
	// pic.i_pts = (context->pts+=90000/FPS);
	pic.i_pts = 0;

	if (context->need_key_frame) {
		// pic->i_type = X264_TYPE_KEYFRAME;
		pic.i_type = X264_TYPE_IDR;
		context->need_key_frame = 0;
	}

	result = x264_encoder_encode(context->x264_handle, &context->x264_nals, &context->x264_nal_count, &pic, &pic_out);

	if (result < 0) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "encode error\n");
		goto error;
	}

	// switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "encode result:%d nal_count:%d daylayed: %d, max_delayed: %d\n", result, context->x264_nal_count, x264_encoder_delayed_frames(context->x264_handle), x264_encoder_maximum_delayed_frames(context->x264_handle));

	if (0) { //debug
		int i;
		x264_nal_t *nals = context->x264_nals;
		for (i = 0; i < context->x264_nal_count; i++) {
			// int start_code_len = 3 + nals[i].b_long_startcode;
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "encoded: %d %d %d %d| %d %d %d %x %x %x %x %x %x\n",
				nals[i].i_type, nals[i].i_ref_idc, nals[i].i_payload, nals[i].b_long_startcode, *(nals[i].p_payload), *(nals[i].p_payload + 1), *(nals[i].p_payload + 2), *(nals[i].p_payload+3), *(nals[i].p_payload + 4), *(nals[i].p_payload + 5), *(nals[i].p_payload + 6), *(nals[i].p_payload + 7), *(nals[i].p_payload + 8));
		}
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "encoder output dts:%ld\n", (long)pic_out.i_dts);
	}

	context->cur_nalu_index = 0;
	return nalu_slice(context, frame);

error:

	*encoded_data_len = 0;
	return SWITCH_STATUS_NOTFOUND;
}

#endif

static switch_status_t switch_h264_decode(switch_codec_t *codec, switch_frame_t *frame)
{
	h264_codec_context_t *context = (h264_codec_context_t *)codec->private_info;
	AVCodecContext *avctx= context->decoder_ctx;

	switch_assert(frame);

	// switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "len: %d ts: %u mark:%d\n", frame->datalen, ntohl(frame->timestamp), frame->m);

	//if (context->last_received_timestamp && context->last_received_timestamp != frame->timestamp &&
	//	(!frame->m) && (!context->last_received_complete_picture)) {
		// possible packet loss
	//	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "Packet Loss, skip privousely received packets\n");
	//	switch_buffer_zero(context->nalu_buffer);
	//}

	context->last_received_timestamp = frame->timestamp;
	context->last_received_complete_picture = frame->m ? SWITCH_TRUE : SWITCH_FALSE;

	buffer_h264_nalu(context, frame);

	if (frame->m) {
		uint32_t size = switch_buffer_inuse(context->nalu_buffer);
		AVPacket pkt = { 0 };
		AVFrame *picture;
		int got_picture = 0;
		int decoded_len;

		if (size > FF_INPUT_BUFFER_PADDING_SIZE) {
			av_init_packet(&pkt);
			pkt.data = NULL;
			pkt.size = 0;
			switch_buffer_peek_zerocopy(context->nalu_buffer, (const void **)&pkt.data);

			pkt.size = size - FF_INPUT_BUFFER_PADDING_SIZE;
			picture = av_frame_alloc();
			assert(picture);
			decoded_len = avcodec_decode_video2(avctx, picture, &got_picture, &pkt);

			// switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "buffer: %d got pic: %d len: %d [%dx%d]\n", size, got_picture, decoded_len, avctx->width, avctx->height);

			if (got_picture && decoded_len > 0) {
				int width = avctx->width;
				int height = avctx->height;

				if (!context->img) {
					context->img = switch_img_wrap(NULL, SWITCH_IMG_FMT_I420, width, height, 0, picture->data[0]);
					assert(context->img);
				}
				context->img->w = picture->linesize[0];
				context->img->h = picture->linesize[1];
				context->img->d_w = width;
				context->img->d_h = height;
				context->img->planes[0] = picture->data[0];
				context->img->planes[1] = picture->data[1];
				context->img->planes[2] = picture->data[2];
				context->img->stride[0] = picture->linesize[0];
				context->img->stride[1] = picture->linesize[1];
				context->img->stride[2] = picture->linesize[2];

				frame->img = context->img;

			}

			av_frame_free(&picture);
			av_free_packet(&pkt);
		}

		switch_buffer_zero(context->nalu_buffer);
		return SWITCH_STATUS_SUCCESS;
	}

	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t switch_h264_control(switch_codec_t *codec, 
										   switch_codec_control_command_t cmd, 
										   switch_codec_control_type_t ctype,
										   void *cmd_data,
										   switch_codec_control_type_t *rtype,
										   void **ret_data) {



	h264_codec_context_t *context = (h264_codec_context_t *)codec->private_info;

	switch(cmd) {
	case SCC_VIDEO_REFRESH:
		context->need_key_frame = 1;		
		break;
	default:
		break;
	}

	return SWITCH_STATUS_SUCCESS;
}


static switch_status_t switch_h264_destroy(switch_codec_t *codec)
{
	h264_codec_context_t *context = (h264_codec_context_t *)codec->private_info;

	if (!context) return SWITCH_STATUS_SUCCESS;

	switch_buffer_destroy(&context->nalu_buffer);
	if (context->decoder_ctx) {
		if (avcodec_is_open(context->decoder_ctx)) avcodec_close(context->decoder_ctx);
		av_free(context->decoder_ctx);
	}

	if (context->img) switch_img_free(context->img);

	if (context->encoder_ctx) {
		if (avcodec_is_open(context->encoder_ctx)) avcodec_close(context->encoder_ctx);
		av_free(context->encoder_ctx);
	}

	if (context->encoder_avframe) {
		av_frame_free(&context->encoder_avframe);
	}

#ifdef H264_CODEC_USE_LIBX264
	if (context->x264_handle) {
		x264_encoder_close(context->x264_handle);
	}
#endif

	return SWITCH_STATUS_SUCCESS;
}

/* end of codec interface */


/* API interface */

static char get_media_type_char(enum AVMediaType type)
{
	switch (type) {
		case AVMEDIA_TYPE_VIDEO:    return 'V';
		case AVMEDIA_TYPE_AUDIO:    return 'A';
		case AVMEDIA_TYPE_DATA:     return 'D';
		case AVMEDIA_TYPE_SUBTITLE: return 'S';
		case AVMEDIA_TYPE_ATTACHMENT:return 'T';
		default:                    return '?';
	}
}

static const AVCodec *next_codec_for_id(enum AVCodecID id, const AVCodec *prev,
										int encoder)
{
	while ((prev = av_codec_next(prev))) {
		if (prev->id == id &&
			(encoder ? av_codec_is_encoder(prev) : av_codec_is_decoder(prev)))
			return prev;
	}
	return NULL;
}

static int compare_codec_desc(const void *a, const void *b)
{
	const AVCodecDescriptor * const *da = a;
	const AVCodecDescriptor * const *db = b;

	return (*da)->type != (*db)->type ? (*da)->type - (*db)->type :
		   strcmp((*da)->name, (*db)->name);
}

static unsigned get_codecs_sorted(const AVCodecDescriptor ***rcodecs)
{
	const AVCodecDescriptor *desc = NULL;
	const AVCodecDescriptor **codecs;
	unsigned nb_codecs = 0, i = 0;

	while ((desc = avcodec_descriptor_next(desc)))
		nb_codecs++;
	if (!(codecs = av_malloc(nb_codecs * sizeof(*codecs)))) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "MEM Error!\n");
		return 0;
	}
	desc = NULL;
	while ((desc = avcodec_descriptor_next(desc)))
		codecs[i++] = desc;
	switch_assert(i == nb_codecs);
	qsort(codecs, nb_codecs, sizeof(*codecs), compare_codec_desc);
	*rcodecs = codecs;
	return nb_codecs;
}

static void print_codecs_for_id(switch_stream_handle_t *stream, enum AVCodecID id, int encoder)
{
	const AVCodec *codec = NULL;

	stream->write_function(stream, " (%s: ", encoder ? "encoders" : "decoders");

	while ((codec = next_codec_for_id(id, codec, encoder)))
		stream->write_function(stream, "%s ", codec->name);

	stream->write_function(stream, ")");
}

SWITCH_STANDARD_API(ffmpeg_api_function)
{
	const AVCodecDescriptor **codecs = NULL;
	unsigned i, nb_codecs = get_codecs_sorted(&codecs);

	stream->write_function(stream, "===============================================:\n"
		   " V..... = Video\n"
		   " A..... = Audio\n"
		   " S..... = Subtitle\n"
		   " .F.... = Frame-level multithreading\n"
		   " ..S... = Slice-level multithreading\n"
		   " ...X.. = Codec is experimental\n"
		   " ....B. = Supports draw_horiz_band\n"
		   " .....D = Supports direct rendering method 1\n"
		   " ----------------------------------------------\n\n");

	for (i = 0; i < nb_codecs; i++) {
		const AVCodecDescriptor *desc = codecs[i];
		const AVCodec *codec = NULL;

		stream->write_function(stream, " ");
		stream->write_function(stream, avcodec_find_decoder(desc->id) ? "D" : ".");
		stream->write_function(stream, avcodec_find_encoder(desc->id) ? "E" : ".");

		stream->write_function(stream, "%c", get_media_type_char(desc->type));
		stream->write_function(stream, (desc->props & AV_CODEC_PROP_INTRA_ONLY) ? "I" : ".");
		stream->write_function(stream, (desc->props & AV_CODEC_PROP_LOSSY)      ? "L" : ".");
		stream->write_function(stream, (desc->props & AV_CODEC_PROP_LOSSLESS)   ? "S" : ".");

		stream->write_function(stream, " %-20s %s", desc->name, desc->long_name ? desc->long_name : "");

		/* print decoders/encoders when there's more than one or their
		 * names are different from codec name */
		while ((codec = next_codec_for_id(desc->id, codec, 0))) {
			if (strcmp(codec->name, desc->name)) {
				print_codecs_for_id(stream ,desc->id, 0);
				break;
			}
		}
		codec = NULL;
		while ((codec = next_codec_for_id(desc->id, codec, 1))) {
			if (strcmp(codec->name, desc->name)) {
				print_codecs_for_id(stream, desc->id, 1);
				break;
			}
		}

		stream->write_function(stream, "\n");

	}

	av_free(codecs);
	return SWITCH_STATUS_SUCCESS;
}

static void log_callback(void *ptr, int level, const char *fmt, va_list vl)
{
	switch_log_level_t switch_level = SWITCH_LOG_DEBUG;

	switch(level) {
		case AV_LOG_QUIET:   switch_level = SWITCH_LOG_CONSOLE; break;
		case AV_LOG_PANIC:   switch_level = SWITCH_LOG_ERROR;   break;
		case AV_LOG_FATAL:   switch_level = SWITCH_LOG_ERROR;   break;
		case AV_LOG_ERROR:   switch_level = SWITCH_LOG_ERROR;   break;
		case AV_LOG_WARNING: switch_level = SWITCH_LOG_WARNING; break;
		case AV_LOG_INFO:    switch_level = SWITCH_LOG_INFO;    break;
		case AV_LOG_VERBOSE: switch_level = SWITCH_LOG_INFO;    break;
		case AV_LOG_DEBUG:   switch_level = SWITCH_LOG_DEBUG;   break;
		default: break;
	}

	switch_level = SWITCH_LOG_ERROR; // hardcoded for debug
	switch_log_vprintf(SWITCH_CHANNEL_LOG, switch_level, fmt, vl);
}

static char *supported_formats[SWITCH_MAX_CODECS] = { 0 };

SWITCH_MODULE_LOAD_FUNCTION(mod_ffmpeg_load)
{
	// switch_application_interface_t *app_interface;
	// switch_file_interface_t *file_interface;
	switch_codec_interface_t *codec_interface;
	switch_api_interface_t *api_interface;

	supported_formats[0] = "mp4";
	supported_formats[1] = "ts";

	/* connect my internal structure to the blank pointer passed to me */
	*module_interface = switch_loadable_module_create_module_interface(pool, modname);

	SWITCH_ADD_CODEC(codec_interface, "H264 Video");
	switch_core_codec_add_video_implementation(pool, codec_interface, 99, "H264", NULL,
											   switch_h264_init, switch_h264_encode, switch_h264_decode, switch_h264_control, switch_h264_destroy);

#if 0
	file_interface = switch_loadable_module_create_interface(*module_interface, SWITCH_FILE_INTERFACE);
	file_interface->interface_name = modname;
	file_interface->extens = supported_formats;
	file_interface->file_open = ffmpeg_open;
	file_interface->file_close = ffmpeg_close;
	file_interface->file_truncate = ffmpeg_truncate;
	file_interface->file_read = ffmpeg_read;
	file_interface->file_write = ffmpeg_write;
	file_interface->file_write_video = ffmpeg_write_video;
	file_interface->file_seek = ffmpeg_seek;
	file_interface->file_set_string = ffmpeg_set_string;
	file_interface->file_get_string = ffmpeg_get_string;

	// SWITCH_ADD_APP(app_interface, "play_ffmpeg", "play a video file", "play a video file", play_ffmpeg_function, "<file>", SAF_NONE);
	SWITCH_ADD_APP(app_interface, "record_ffmpeg", "record a video file", "record a video file", record_ffmpeg_function, "<file>", SAF_NONE);
	SWITCH_ADD_APP(app_interface, "stream_ts", "stream ts to rtp", "stream ts to rtp", stream_ts_function, "<file>", SAF_NONE);
#endif

	SWITCH_ADD_API(api_interface, "ffmpeg", "ffmpeg", ffmpeg_api_function, "");

	av_log_set_callback(log_callback);
	av_log_set_level(AV_LOG_DEBUG);
	av_register_all();

	av_log(NULL, AV_LOG_INFO, "%s %d\n", "av_log callback installed, level=", av_log_get_level());

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
