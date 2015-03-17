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
 * Seven Du <dujinfang@gmail.com>
 *
 * mod_avformat -- FS Video File Format using libav.org
 *
 */

#include <switch.h>
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/opt.h>
#include <libavutil/imgutils.h>
#include <libavutil/avstring.h>
#include <libavutil/channel_layout.h>
// #include <libavutil/timestamp.h>
#include <libavresample/avresample.h>

SWITCH_MODULE_LOAD_FUNCTION(mod_avformat_load);
SWITCH_MODULE_DEFINITION(mod_avformat, mod_avformat_load, NULL, NULL);

static char *const get_error_text(const int error)
{
    static char error_buffer[255];
    av_strerror(error, error_buffer, sizeof(error_buffer));
    return error_buffer;
}

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

	/* U/V */
	for(i = 0; i < pict->height / 2; i++) {
		memcpy(&pict->data[1][i * pict->linesize[1]], u + i * img->stride[1], pict->width / 2);
		memcpy(&pict->data[2][i * pict->linesize[2]], v + i * img->stride[2], pict->width / 2);
	}
}

/* App interface */

// a wrapper around a single output AVStream
typedef struct OutputStream {
	AVStream *st;
	AVFrame *frame;
	AVFrame *tmp_frame;
	int64_t next_pts;
	struct AVAudioResampleContext *resample_ctx;
	// audio
	int channels;
	int sample_rate;
	//video
	int width;
	int height;
} OutputStream;

typedef struct record_helper_s {
	switch_mutex_t *mutex;
	AVFormatContext *oc;
	OutputStream *video_st;
	switch_timer_t *timer;
	int in_callback;
	switch_queue_t *video_queue;
	switch_thread_t *video_thread;
} record_helper_t;

static void log_packet(const AVFormatContext *fmt_ctx, const AVPacket *pkt)
{
	// AVRational *time_base = &fmt_ctx->streams[pkt->stream_index]->time_base;

	// printf("pts:%s pts_time:%s dts:%s dts_time:%s duration:%s duration_time:%s stream_index:%d\n",
	// 	   av_ts2str(pkt->pts), av_ts2timestr(pkt->pts, time_base),
	// 	   av_ts2str(pkt->dts), av_ts2timestr(pkt->dts, time_base),
	// 	   av_ts2str(pkt->duration), av_ts2timestr(pkt->duration, time_base),
	// 	   pkt->stream_index);
}

static int avformat_alloc_output_context2(AVFormatContext **avctx, AVOutputFormat *oformat,
                                   const char *format, const char *filename)
{
    AVFormatContext *s = avformat_alloc_context();
    int ret = 0;

    *avctx = NULL;
    if (!s)
        goto nomem;

    if (!oformat) {
        if (format) {
            oformat = av_guess_format(format, NULL, NULL);
            if (!oformat) {
                av_log(s, AV_LOG_ERROR, "Requested output format '%s' is not a suitable output format\n", format);
                ret = AVERROR(EINVAL);
                goto error;
            }
        } else {
            oformat = av_guess_format(NULL, filename, NULL);
            if (!oformat) {
                ret = AVERROR(EINVAL);
                av_log(s, AV_LOG_ERROR, "Unable to find a suitable output format for '%s'\n",
                       filename);
                goto error;
            }
        }
    }

    s->oformat = oformat;
    if (s->oformat->priv_data_size > 0) {
        s->priv_data = av_mallocz(s->oformat->priv_data_size);
        if (!s->priv_data)
            goto nomem;
        if (s->oformat->priv_class) {
            *(const AVClass**)s->priv_data= s->oformat->priv_class;
            av_opt_set_defaults(s->priv_data);
        }
    } else
        s->priv_data = NULL;

    if (filename)
        av_strlcpy(s->filename, filename, sizeof(s->filename));
    *avctx = s;
    return 0;
nomem:
    av_log(s, AV_LOG_ERROR, "Out of memory\n");
    ret = AVERROR(ENOMEM);
error:
    avformat_free_context(s);
    return ret;
}

static int write_frame(AVFormatContext *fmt_ctx, const AVRational *time_base, AVStream *st, AVPacket *pkt)
{
	/* rescale output packet timestamp values from codec to stream timebase */
	av_packet_rescale_ts(pkt, *time_base, st->time_base);
	pkt->stream_index = st->index;

	/* Write the compressed frame to the media file. */
	if (0) log_packet(fmt_ctx, pkt);
	return av_interleaved_write_frame(fmt_ctx, pkt);
}

/* Add an output stream. */
static switch_status_t add_stream(OutputStream *ost, AVFormatContext *oc, AVCodec **codec, enum AVCodecID codec_id)
{
	AVCodecContext *c;
	switch_status_t status = SWITCH_STATUS_FALSE;
	int threads = switch_core_cpu_count();
	/* find the encoder */
	*codec = avcodec_find_encoder(codec_id);
	if (!(*codec)) {
		// switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Could not find encoder for '%s'\n", avcodec_get_name(codec_id));
		return status;
	}

	ost->st = avformat_new_stream(oc, *codec);
	if (!ost->st) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Could not allocate stream\n");
		return status;
	}
	ost->st->id = oc->nb_streams - 1;
	c = ost->st->codec;
	// switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "id:%d den:%d num:%d\n", ost->st->id, ost->st->time_base.den, ost->st->time_base.num);

	if (threads > 4) {
		threads = 4;
	}

	switch ((*codec)->type) {
	case AVMEDIA_TYPE_AUDIO:
		c->sample_fmt  = (*codec)->sample_fmts ? (*codec)->sample_fmts[0] : AV_SAMPLE_FMT_FLTP;
		c->bit_rate    = 128000;
		c->sample_rate = ost->sample_rate = 44100;
		c->channels    = ost->channels;
		c->channel_layout = av_get_default_channel_layout(c->channels);
		break;

	case AVMEDIA_TYPE_VIDEO:
		c->codec_id = codec_id;
		c->bit_rate = 450000;
		/* Resolution must be a multiple of two. */
		c->width    = ost->width;
		c->height   = ost->height;
		c->time_base.den = 1000;
		c->time_base.num = 1;
		c->gop_size      = 25; /* emit one intra frame every x frames at most */
		c->pix_fmt       = AV_PIX_FMT_YUV420P;
		c->thread_count  = threads;
		c->rc_initial_buffer_occupancy = 1024 * 1024 * 8;
		if (codec_id == AV_CODEC_ID_VP8) {
			av_set_options_string(c, "quality=realtime", "=", ":");
		}
		break;
	default:
		break;
	}

	/* Some formats want stream headers to be separate. */
	if (oc->oformat->flags & AVFMT_GLOBALHEADER) {
		c->flags |= CODEC_FLAG_GLOBAL_HEADER;
	}

	return SWITCH_STATUS_SUCCESS;
}

static AVFrame *alloc_picture(enum AVPixelFormat pix_fmt, int width, int height)
{
	AVFrame *picture;
	int ret;

	picture = av_frame_alloc();
	if (!picture) return NULL;

	picture->format = pix_fmt;
	picture->width  = width;
	picture->height = height;

	/* allocate the buffers for the frame data */
	ret = av_frame_get_buffer(picture, 32);
	if (ret < 0) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Could not allocate frame data.\n");
		return NULL;
	}

	return picture;
}

static switch_status_t open_video(AVFormatContext *oc, AVCodec *codec, OutputStream *ost)
{
	int ret;
	AVCodecContext *c = ost->st->codec;
	switch_status_t status = SWITCH_STATUS_FALSE;

	/* open the codec */
	ret = avcodec_open2(c, codec, NULL);
	if (ret < 0) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Could not open video codec: %s\n", get_error_text(ret));
		return status;
	}

	/* allocate and init a re-usable frame */
	ost->frame = alloc_picture(c->pix_fmt, c->width, c->height);
	switch_assert(ost->frame);

	// switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "pix_fmt: %d\n", c->pix_fmt);
	switch_assert(c->pix_fmt == AV_PIX_FMT_YUV420P); // always I420 for NOW

	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t open_audio(AVFormatContext *oc, AVCodec *codec, OutputStream *ost)
{
	AVCodecContext *c;
	int ret;
	switch_status_t status = SWITCH_STATUS_FALSE;

	c = ost->st->codec;

	ret = avcodec_open2(c, codec, NULL);
	if (ret < 0) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Could not open audio codec: %s\n", get_error_text(ret));
		return status;
	}

	ost->frame = av_frame_alloc();
	switch_assert(ost->frame);

	ost->frame->sample_rate    = c->sample_rate;
	ost->frame->format         = AV_SAMPLE_FMT_S16;
	ost->frame->channel_layout = c->channel_layout;

	if (c->codec->capabilities & CODEC_CAP_VARIABLE_FRAME_SIZE) {
		ost->frame->nb_samples = 10000;
	} else {
		ost->frame->nb_samples = c->frame_size;
	}

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "sample_rate: %d nb_samples: %d\n", ost->frame->sample_rate, ost->frame->nb_samples);

	// disable resampler for now before we figure out the correct params
	if (0 && c->sample_fmt != AV_SAMPLE_FMT_S16) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "sample_fmt %d != AV_SAMPLE_FMT_S16, start resampler\n", c->sample_fmt);

		ost->resample_ctx = avresample_alloc_context();

		if (!ost->resample_ctx) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Could not allocate resampler context\n");
			return status;
		}

		/* set options */
		av_opt_set_int(ost->resample_ctx, "in_channel_count",   c->channels,       0);
		av_opt_set_int(ost->resample_ctx, "in_sample_rate",     c->sample_rate,    0);
		av_opt_set_int(ost->resample_ctx, "in_sample_fmt",      AV_SAMPLE_FMT_S16, 0);
		av_opt_set_int(ost->resample_ctx, "in_channel_layout",  c->channel_layout, 0);
		av_opt_set_int(ost->resample_ctx, "out_channel_count",  c->channels,       0);
		av_opt_set_int(ost->resample_ctx, "out_sample_rate",    c->sample_rate,    0);
		av_opt_set_int(ost->resample_ctx, "out_sample_fmt",     c->sample_fmt,     0);
		av_opt_set_int(ost->resample_ctx, "out_channel_layout", c->channel_layout, 0);

		if ((ret = avresample_open(ost->resample_ctx)) < 0) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Failed to initialize the resampling context\n");
			return status;
		}
	}

	ret = av_frame_get_buffer(ost->frame, 0);
	if (ret < 0) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Could not allocate audio frame.\n");
		return status;
	}

	if (ost->resample_ctx) {
		ost->tmp_frame = av_frame_alloc();
		switch_assert(ost->tmp_frame);

		ost->tmp_frame->sample_rate    = c->sample_rate;
		ost->tmp_frame->format         = c->sample_fmt;
		ost->tmp_frame->channel_layout = c->channel_layout;
		ost->tmp_frame->nb_samples     = ost->frame->nb_samples;

		ret = av_frame_get_buffer(ost->tmp_frame, 0);
		if (ret < 0) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Could not allocate audio frame.\n");
			return status;
		}
	}

	return SWITCH_STATUS_SUCCESS;
}



static void *SWITCH_THREAD_FUNC video_thread_run(switch_thread_t *thread, void *obj)
{
	record_helper_t *eh = (record_helper_t *) obj;
	void *pop;
	switch_image_t *img, *last_img = NULL, *tmp_img = NULL;

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "video thread start\n");

	for(;;) {
		AVPacket pkt = { 0 };
		int got_packet;
		int ret = -1;

		img = NULL;

		if (switch_queue_pop_timeout(eh->video_queue, &pop, 66000) == SWITCH_STATUS_SUCCESS) {
			if (!pop) break;
			img = (switch_image_t *) pop;

			if (last_img && (last_img->d_w != img->d_w || last_img->d_h != img->d_h)) {
				/* scale to match established stream */
				switch_img_scale(img, &tmp_img, last_img->d_w, last_img->d_h);
				switch_img_free(&img);
				img = tmp_img;
				tmp_img = NULL;
			}

			switch_img_free(&last_img);
			last_img = img;
		} else {
			if (last_img) {
				img = last_img;
			} else {
				continue;
			}
		}

		if (!img) {
			continue;
		}

		switch_mutex_lock(eh->mutex);

		eh->in_callback = 1;
		
		av_init_packet(&pkt);		

		if (eh->video_st->frame) {
			ret = av_frame_make_writable(eh->video_st->frame);
		}

		if (ret < 0) continue;

		fill_avframe(eh->video_st->frame, img);
		switch_core_timer_sync(eh->timer);

		if (eh->video_st->frame->pts == eh->timer->samplecount) {
			// never use the same pts, or the encoder coughs
			eh->video_st->frame->pts++;
		} else {
			eh->video_st->frame->pts = eh->timer->samplecount;
		}
		// eh->video_st->frame->pts = switch_time_now() / 1000 - eh->video_st->next_pts;
		// switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "pts: %lld\n", eh->video_st->frame->pts);

		/* encode the image */
		ret = avcodec_encode_video2(eh->video_st->st->codec, &pkt, eh->video_st->frame, &got_packet);

		if (ret < 0) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Encoding Error %d\n", ret);
			continue;
		}

		if (got_packet) {
			ret = write_frame(eh->oc, &eh->video_st->st->codec->time_base, eh->video_st->st, &pkt);
			av_free_packet(&pkt);
		}

		eh->in_callback = 0;
		switch_mutex_unlock(eh->mutex);
	}

	switch_img_free(&last_img);
	
	while(switch_queue_trypop(eh->video_queue, &pop) == SWITCH_STATUS_SUCCESS) {
		if (!pop) break;
		img = (switch_image_t *) pop;
		switch_img_free(&img);
	}

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "video thread done\n");

	return NULL;
}


static switch_status_t video_read_callback(switch_core_session_t *session, switch_frame_t *frame, void *user_data)
{
	record_helper_t *eh = user_data;
	switch_image_t *img = NULL;

	if (frame->img) {
		switch_img_copy(frame->img, &img);
		switch_queue_push(eh->video_queue, img);
	}

	return SWITCH_STATUS_SUCCESS;;
}

static void close_stream(AVFormatContext *oc, OutputStream *ost)
{
	avcodec_close(ost->st->codec);
	av_frame_free(&ost->frame);
	if (ost->tmp_frame) av_frame_free(&ost->tmp_frame);
}

SWITCH_STANDARD_APP(record_av_function)
{
	switch_status_t status;
	switch_frame_t *read_frame;
	switch_channel_t *channel = switch_core_session_get_channel(session);
	record_helper_t eh = { 0 };
	switch_timer_t timer = { 0 };
	switch_mutex_t *mutex = NULL;
	switch_codec_t codec;//, *vid_codec;
	switch_codec_implementation_t read_impl = { 0 };
	switch_dtmf_t dtmf = { 0 };
	switch_buffer_t *buffer = NULL;
	int inuse = 0, bytes = 0;
	switch_vid_params_t vid_params = { 0 };
	int force_sample_rate;

	OutputStream video_st = { 0 }, audio_st = { 0 };
	AVOutputFormat *fmt = NULL;
	const char *format = NULL;
	AVFormatContext *oc = NULL;
	AVCodec *audio_codec, *video_codec;
	int has_audio = 0, has_video = 0;
	int ret;

	switch_channel_answer(channel);
	switch_core_session_get_read_impl(session, &read_impl);
	switch_core_session_request_video_refresh(session);

	switch_channel_set_variable(channel, SWITCH_PLAYBACK_TERMINATOR_USED, "");

	if (!switch_channel_ready(channel)) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_CRIT, "%s not ready.\n", switch_channel_get_name(channel));
		switch_channel_set_variable(channel, SWITCH_CURRENT_APPLICATION_RESPONSE_VARIABLE, "Channel not ready");
		goto done;
	}

	switch_channel_set_flag(channel, CF_VIDEO_DECODED_READ);
	switch_core_media_get_vid_params(session, &vid_params);
	switch_channel_set_flag(channel, CF_VIDEO_ECHO);
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "video size: %dx%d\n", vid_params.width, vid_params.height);

	force_sample_rate = read_impl.samples_per_second;

	if (!strncmp(data, "rtmp://", 7)) {
		format = "flv";
		force_sample_rate = 44100;
	}

	if (switch_core_codec_init(&codec,
							   "L16",
							   NULL,
							   force_sample_rate,
							   read_impl.microseconds_per_packet / 1000,
							   read_impl.number_of_channels, SWITCH_CODEC_FLAG_ENCODE | SWITCH_CODEC_FLAG_DECODE,
							   NULL, switch_core_session_get_pool(session)) == SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "Audio Codec Activation Success\n");
	} else {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "Audio Codec Activation Fail\n");
		switch_channel_set_variable(channel, SWITCH_CURRENT_APPLICATION_RESPONSE_VARIABLE, "Audio codec activation failed");
		goto end;
	}

	switch_buffer_create_dynamic(&buffer, 8192, 65536, 0);

	av_register_all();
	avformat_alloc_output_context2(&oc, NULL, format, data);

	if (!oc) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "Could not deduce output format from file extension\n");
		goto end;
	}

	fmt = oc->oformat;

	/* open the output file, if needed */
	if (!(fmt->flags & AVFMT_NOFILE)) {
		ret = avio_open(&oc->pb, data, AVIO_FLAG_WRITE);
		if (ret < 0) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Could not open '%s': %s\n", data, get_error_text(ret));
			goto end;
		}
	} else {
		avformat_network_init();
 	}

	if (fmt->video_codec != AV_CODEC_ID_NONE &&
		switch_channel_test_flag(channel, CF_VIDEO) &&
		vid_params.width > 0 && vid_params.height > 0) {

		char codec_str[256];
		const AVCodecDescriptor *desc;

		if (!strncmp(data, "rtmp://", 7)) {
		 	if (fmt->video_codec != AV_CODEC_ID_H264 ) {
				fmt->video_codec = AV_CODEC_ID_H264; // force H264
			}

			if (fmt->audio_codec != AV_CODEC_ID_AAC) {
				fmt->audio_codec = AV_CODEC_ID_AAC;  // force AAC
			}
		}

		desc = avcodec_descriptor_get(fmt->video_codec);

		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "use video codec: [%d] %s (%s)\n", fmt->video_codec, desc->name, desc->long_name);

		video_st.width = vid_params.width;
		video_st.height = vid_params.height;
		video_st.next_pts = switch_time_now() / 1000;
		if (add_stream(&video_st, oc, &video_codec, fmt->video_codec) == SWITCH_STATUS_SUCCESS &&
			open_video(oc, video_codec, &video_st) == SWITCH_STATUS_SUCCESS) {
			avcodec_string(codec_str, sizeof(codec_str), video_st.st->codec, 1);
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "use video codec implementation %s\n", codec_str);
			has_video = 1;
		}
	}

	if (fmt->audio_codec != AV_CODEC_ID_NONE) {
		audio_st.channels = read_impl.number_of_channels;
		audio_st.sample_rate = force_sample_rate;

		add_stream(&audio_st, oc, &audio_codec, fmt->audio_codec);
		if (open_audio(oc, audio_codec, &audio_st) != SWITCH_STATUS_SUCCESS) {
			goto end;
		}

		has_audio = 1;
	}

	av_dump_format(oc, 0, data, 1);

	/* Write the stream header, if any. */
	ret = avformat_write_header(oc, NULL);
	if (ret < 0) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Error occurred when opening output file: %s\n", get_error_text(ret));
		goto end;
	}

	if (has_video) {
		switch_threadattr_t *thd_attr = NULL;

		switch_mutex_init(&mutex, SWITCH_MUTEX_NESTED, switch_core_session_get_pool(session));
		eh.mutex = mutex;
		eh.video_st = &video_st;
		eh.oc = oc;
		if (switch_core_timer_init(&timer, "soft", 1, 1, switch_core_session_get_pool(session)) != SWITCH_STATUS_SUCCESS) {
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "Timer Activation Fail\n");
			goto end;
		}
		eh.timer = &timer;
		switch_queue_create(&eh.video_queue, SWITCH_CORE_QUEUE_LEN, switch_core_session_get_pool(session));
		switch_core_session_set_video_read_callback(session, video_read_callback, (void *)&eh);

		switch_threadattr_create(&thd_attr, switch_core_session_get_pool(session));
		//switch_threadattr_priority_set(thd_attr, SWITCH_PRI_REALTIME);
		switch_threadattr_stacksize_set(thd_attr, SWITCH_THREAD_STACKSIZE);
		switch_thread_create(&eh.video_thread, thd_attr, video_thread_run, &eh, switch_core_session_get_pool(session));
	}

	switch_core_session_set_read_codec(session, &codec);

	{
		char buf[SWITCH_RECOMMENDED_BUFFER_SIZE] = {0};
		switch_size_t datalen = codec.implementation->decoded_bytes_per_packet;
		switch_size_t samples = datalen / 2 / codec.implementation->number_of_channels;
		int offset = 1200;
		int fps = codec.implementation->actual_samples_per_second / samples;
		int lead_frames = (offset * fps) / 1000;
		
		for (int x = 0; x < lead_frames; x++) {
			switch_buffer_write(buffer, buf, datalen);
		}		
	}
		
	while (switch_channel_ready(channel)) {

		status = switch_core_session_read_frame(session, &read_frame, SWITCH_IO_FLAG_SINGLE_READ, 0);

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

		if (!SWITCH_READ_ACCEPTABLE(status)) {
			break;
		}

		if (switch_test_flag(read_frame, SFF_CNG)) {
			continue;
		}

		if (mutex) switch_mutex_lock(mutex);

		switch_buffer_write(buffer, read_frame->data, read_frame->datalen);
		bytes = audio_st.frame->nb_samples * 2 * audio_st.st->codec->channels;
		inuse = switch_buffer_inuse(buffer);

		while (inuse >= bytes) {
			AVPacket pkt = { 0 };
			int got_packet = 0;

			av_init_packet(&pkt);

			// switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "inuse: %d samples: %d bytes: %d\n", inuse, audio_st.frame->nb_samples, bytes);

			if (audio_st.resample_ctx) { // need resample
				int out_samples = avresample_get_out_samples(audio_st.resample_ctx, audio_st.frame->nb_samples);

				av_frame_make_writable(audio_st.frame);
				av_frame_make_writable(audio_st.tmp_frame);
				switch_buffer_read(buffer, audio_st.frame->data[0], bytes);
				/* convert to destination format */
				ret = avresample_convert(audio_st.resample_ctx,
						(uint8_t **)audio_st.frame->data, 0, out_samples,
						audio_st.tmp_frame->data, 0, audio_st.frame->nb_samples);

				if (ret < 0) {
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Error while converting %d samples, error text: %s\n",
						audio_st.frame->nb_samples, get_error_text(ret));
					inuse = switch_buffer_inuse(buffer);
					continue;
				}

				audio_st.tmp_frame->pts = audio_st.next_pts;
				audio_st.next_pts += audio_st.frame->nb_samples;

				ret = avcodec_encode_audio2(audio_st.st->codec, &pkt, audio_st.tmp_frame, &got_packet);
			} else {
				av_frame_make_writable(audio_st.frame);
				switch_buffer_read(buffer, audio_st.frame->data[0], bytes);
				audio_st.frame->pts = audio_st.next_pts;
				audio_st.next_pts  += audio_st.frame->nb_samples;

				ret = avcodec_encode_audio2(audio_st.st->codec, &pkt, audio_st.frame, &got_packet);
			}

			if (ret < 0) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "Error encoding audio frame: %d\n", ret);
				inuse = switch_buffer_inuse(buffer);
				continue;
			}

			if (got_packet) {
				// switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "got pkt: %d\n", pkt.size);
				ret = write_frame(oc, &audio_st.st->codec->time_base, audio_st.st, &pkt);
				if (ret < 0) {
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Error while writing audio frame: %s\n", get_error_text(ret));
					goto end;
				}
			}

			inuse = switch_buffer_inuse(buffer);
		}

		if (mutex) switch_mutex_unlock(mutex);

		//switch_core_session_write_frame(session, read_frame, SWITCH_IO_FLAG_NONE, 0);
	}

	switch_core_session_set_video_read_callback(session, NULL, NULL);

	if (has_video) {
		AVPacket pkt = { 0 };
		int got_packet = 0;
		int ret = 0;
		switch_status_t st;

		switch_queue_push(eh.video_queue, NULL);
		switch_thread_join(&st, eh.video_thread);

	again:
		av_init_packet(&pkt);
		ret = avcodec_encode_video2(video_st.st->codec, &pkt, NULL, &got_packet);

		if (ret < 0) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Encoding Error %d\n", ret);
			goto end;
		}

		if (got_packet) {
			ret = write_frame(oc, &video_st.st->codec->time_base, video_st.st, &pkt);
			av_free_packet(&pkt);
			goto again;
		}
	}

	av_write_trailer(oc);
	switch_channel_set_variable(channel, SWITCH_CURRENT_APPLICATION_RESPONSE_VARIABLE, "OK");

  end:

	if (oc) {
		if (has_video) close_stream(oc, &video_st);
		if (has_audio) close_stream(oc, &audio_st);

		if (fmt) {
			if (!(fmt->flags & AVFMT_NOFILE)) {
				avio_close(oc->pb);
			} else {
				avformat_network_deinit();
			}
		}

		/* free the stream */
		avformat_free_context(oc);
	}

	if (timer.interval) {
		switch_core_timer_destroy(&timer);
	}

	switch_core_media_end_video_function(session);
	switch_core_session_set_read_codec(session, NULL);
	switch_core_codec_destroy(&codec);

 done:
	switch_core_session_video_reset(session);
}
/* end of App interface */


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

static int is_device(const AVClass *avclass)
{
#if 0
	if (!avclass) return 0;


	return  avclass->category == AV_CLASS_CATEGORY_DEVICE_VIDEO_OUTPUT ||
			avclass->category == AV_CLASS_CATEGORY_DEVICE_VIDEO_INPUT ||
			avclass->category == AV_CLASS_CATEGORY_DEVICE_AUDIO_OUTPUT ||
			avclass->category == AV_CLASS_CATEGORY_DEVICE_AUDIO_INPUT ||
			avclass->category == AV_CLASS_CATEGORY_DEVICE_OUTPUT ||
			avclass->category == AV_CLASS_CATEGORY_DEVICE_INPUT;
#endif

	return 0;

}

void show_formats(switch_stream_handle_t *stream) {
	AVInputFormat *ifmt  = NULL;
	AVOutputFormat *ofmt = NULL;
	const char *last_name;
	// int is_dev;

	stream->write_function(stream, "============= File Formats ==============================:\n"
		   " D. = Demuxing supported\n"
		   " .M = Muxing supported\n"
		   "----------------------\n");

	last_name = "000";

	for (;;) {
		int decode = 0;
		int encode = 0;
		int is_dev = 0;
		const char *name      = NULL;
		const char *long_name = NULL;

		while ((ofmt = av_oformat_next(ofmt))) {
			is_dev = is_device(ofmt->priv_class);

			if ((name == NULL || strcmp(ofmt->name, name) < 0) &&
				strcmp(ofmt->name, last_name) > 0) {
				name      = ofmt->name;
				long_name = ofmt->long_name;
				encode    = 1;
			}
		}

		while ((ifmt = av_iformat_next(ifmt))) {
			is_dev = is_device(ifmt->priv_class);

			if ((name == NULL || strcmp(ifmt->name, name) < 0) &&
				strcmp(ifmt->name, last_name) > 0) {
				name      = ifmt->name;
				long_name = ifmt->long_name;
				encode    = 0;
			}

			if (name && strcmp(ifmt->name, name) == 0) decode = 1;
		}

		if (name == NULL) break;

		last_name = name;

		stream->write_function(stream, "%s%s%s %-15s %s\n",
			is_dev ? "*" : " ",
			decode ? "D" : " ",
			encode ? "M" : " ",
			name, long_name ? long_name:" ");
	}

}

void show_codecs(switch_stream_handle_t *stream)
{
	const AVCodecDescriptor **codecs = NULL;
	unsigned i, nb_codecs = get_codecs_sorted(&codecs);

	stream->write_function(stream, "================ Codecs ===============================:\n"
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
}

SWITCH_STANDARD_API(av_format_api_function)
{
	if (zstr(cmd)) {
		show_codecs(stream);
		stream->write_function(stream, "\n");
		show_formats(stream);
	} else {
		if (!strcmp(cmd, "show formats")) {
			show_formats(stream);
		} else if (!strcmp(cmd, "show codecs")) {
			show_codecs(stream);
		} else {
			stream->write_function(stream, "Usage: ffmpeg show <formats|codecs>");
		}
	}

	return SWITCH_STATUS_SUCCESS;
}

static void log_callback(void *ptr, int level, const char *fmt, va_list vl)
{
	switch_log_level_t switch_level = SWITCH_LOG_DEBUG;

	if (level > AV_LOG_INFO) return;

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

	// switch_level = SWITCH_LOG_ERROR; // hardcoded for debug
	switch_log_vprintf(SWITCH_CHANNEL_LOG_CLEAN, switch_level, fmt, vl);
}

/* file interface */

struct av_file_context {
	switch_memory_pool_t *pool;
	switch_mutex_t *mutex;
	switch_buffer_t *buf;
	switch_buffer_t *audio_buffer;
	switch_timer_t timer;
	int offset;
	int audio_start;
	int vid_ready;
	int audio_ready;

	OutputStream video_st;
	OutputStream audio_st;
	AVFormatContext *oc;
	AVCodec *audio_codec;
	AVCodec *video_codec;

	int has_audio;
	int has_video;

	record_helper_t eh;
};

typedef struct av_file_context av_file_context_t;

static switch_status_t av_file_open(switch_file_handle_t *handle, const char *path)
{
	av_file_context_t *context;
	char *ext;
	unsigned int flags = 0;
	const char *tmp = NULL;
	AVOutputFormat *fmt;
	const char *format = NULL;
	int ret;
	char file[1024];

	switch_set_string(file, path);

	if ((ext = strrchr((char *)path, '.')) == 0) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Invalid Format\n");
		return SWITCH_STATUS_GENERR;
	} else if (handle->stream_name && (!strcasecmp(handle->stream_name, "rtmp") || !strcasecmp(handle->stream_name, "youtube"))) {
		format = "flv";
		handle->samplerate = 44100;
		switch_snprintf(file, sizeof(file), "rtmp://%s", path);
	}

	ext++;

	if ((context = (av_file_context_t *)switch_core_alloc(handle->memory_pool, sizeof(av_file_context_t))) == 0) {
		return SWITCH_STATUS_MEMERR;
	}

	memset(context, 0, sizeof(av_file_context_t));

	context->offset = 0; // 1200 ?
	if (handle->params && (tmp = switch_event_get_header(handle->params, "av_video_offset"))) {
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

	switch_buffer_create_dynamic(&context->audio_buffer, 512, 512, 1024000);

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "sample rate: %d, channels: %d\n", handle->samplerate, handle->channels);

	av_register_all();
	avformat_alloc_output_context2(&context->oc, NULL, format, (char *)file);

	if (!context->oc) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "Could not deduce output format from file extension\n");
		goto end;
	}

	fmt = context->oc->oformat;

	/* open the output file, if needed */
	if (!(fmt->flags & AVFMT_NOFILE)) {
		ret = avio_open(&context->oc->pb, file, AVIO_FLAG_WRITE);
		if (ret < 0) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Could not open '%s': %s\n", file, get_error_text(ret));
			goto end;
		}
	} else {
		avformat_network_init();
	}

	if (fmt->video_codec != AV_CODEC_ID_NONE) {
		const AVCodecDescriptor *desc;

		if (handle->stream_name && (!strcasecmp(handle->stream_name, "rtmp") || !strcasecmp(handle->stream_name, "youtube"))) {
			if (fmt->video_codec != AV_CODEC_ID_H264 ) {
				fmt->video_codec = AV_CODEC_ID_H264; // force H264
			}

			if (fmt->audio_codec != AV_CODEC_ID_AAC) {
				fmt->audio_codec = AV_CODEC_ID_AAC;  // force AAC
			}
		}

		desc = avcodec_descriptor_get(fmt->video_codec);

		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "use video codec: [%d] %s (%s)\n", fmt->video_codec, desc->name, desc->long_name);
	}

	if (fmt->audio_codec != AV_CODEC_ID_NONE) {
		context->audio_st.channels = handle->channels;
		context->audio_st.sample_rate = handle->samplerate;

		add_stream(&context->audio_st, context->oc, &context->audio_codec, fmt->audio_codec);
		if (open_audio(context->oc, context->audio_codec, &context->audio_st) != SWITCH_STATUS_SUCCESS) {
			goto end;
		}

		context->has_audio = 1;
	}

	av_dump_format(context->oc, 0, file, 1);

	handle->format = 0;
	handle->sections = 0;
	handle->seekable = 0;
	handle->speed = 0;
	handle->pos = 0;
	handle->private_info = context;
	context->pool = handle->memory_pool;

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "Opening File [%s] %dhz %s\n",
		file, handle->samplerate, switch_test_flag(handle, SWITCH_FILE_FLAG_VIDEO) ? " with VIDEO" : "");

	return SWITCH_STATUS_SUCCESS;

end:

	return SWITCH_STATUS_GENERR;
}

static switch_status_t av_file_truncate(switch_file_handle_t *handle, int64_t offset)
{
	return SWITCH_STATUS_FALSE;
}

static switch_status_t av_file_close(switch_file_handle_t *handle)
{
	av_file_context_t *context = (av_file_context_t *)handle->private_info;
	switch_status_t status;

	if (context->eh.video_queue) {
		switch_queue_push(context->eh.video_queue, NULL);
	}

	if (context->eh.video_thread) {
		switch_thread_join(&status, context->eh.video_thread);
	}

	if (context->oc) {
		av_write_trailer(context->oc);

		if (context->has_video) close_stream(context->oc, &context->video_st);
		if (context->has_audio) close_stream(context->oc, &context->audio_st);

		if (!(context->oc->oformat->flags & AVFMT_NOFILE)) {
			avio_close(context->oc->pb);
		} else {
			avformat_network_deinit();
		}

		/* free the stream */
		avformat_free_context(context->oc);
	}

	if (context->timer.interval) {
		switch_core_timer_destroy(&context->timer);
	}

	switch_buffer_destroy(&context->audio_buffer);

	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t av_file_seek(switch_file_handle_t *handle, unsigned int *cur_sample, int64_t samples, int whence)
{
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "seek not implemented\n");
	return SWITCH_STATUS_FALSE;
}

static switch_status_t av_file_read(switch_file_handle_t *handle, void *data, size_t *len)
{
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "read not implemented\n");
	return SWITCH_STATUS_FALSE;
}

static switch_status_t av_file_write(switch_file_handle_t *handle, void *data, size_t *len)
{

	uint32_t datalen = *len * 2 * handle->channels;
	switch_status_t status = SWITCH_STATUS_SUCCESS;
	// uint8_t buf[SWITCH_RECOMMENDED_BUFFER_SIZE] = { 0 }, *bp = buf;
	// uint32_t encoded_rate;
	av_file_context_t *context = (av_file_context_t *)handle->private_info;
	// uint32_t size = 0;
	uint32_t bytes;
	int inuse;

	if (!context->vid_ready) {
		return status;
	}

	if (context->offset) {
		char buf[SWITCH_RECOMMENDED_BUFFER_SIZE] = {0};
		switch_size_t samples = *len;
		int fps = handle->samplerate / samples;
		int lead_frames = (context->offset * fps) / 1000;

		for (int x = 0; x < lead_frames; x++) {
			switch_buffer_write(context->audio_buffer, buf, datalen);
		}
		context->offset = 0;
	}

	if (context->mutex) switch_mutex_lock(context->mutex);

	switch_buffer_write(context->audio_buffer, data, datalen);
	bytes = context->audio_st.frame->nb_samples * 2 * context->audio_st.st->codec->channels;
	inuse = switch_buffer_inuse(context->audio_buffer);

	while (inuse >= bytes) {
		AVPacket pkt = { 0 };
		int got_packet = 0;
		int ret;

		av_init_packet(&pkt);

		// switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "inuse: %d samples: %d bytes: %d\n", inuse, audio_st.frame->nb_samples, bytes);

		if (0 && context->audio_st.resample_ctx) { // need resample
			int out_samples = avresample_get_out_samples(context->audio_st.resample_ctx, context->audio_st.frame->nb_samples);

			av_frame_make_writable(context->audio_st.frame);
			av_frame_make_writable(context->audio_st.tmp_frame);
			switch_buffer_read(context->audio_buffer, context->audio_st.frame->data[0], bytes);
			/* convert to destination format */
			ret = avresample_convert(context->audio_st.resample_ctx,
					(uint8_t **)context->audio_st.frame->data, 0, out_samples,
					context->audio_st.tmp_frame->data, 0, context->audio_st.frame->nb_samples);

			if (ret < 0) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Error while converting %d samples, error text: %s\n",
					context->audio_st.frame->nb_samples, get_error_text(ret));
				inuse = switch_buffer_inuse(context->audio_buffer);
				continue;
			}

			context->audio_st.tmp_frame->pts = context->audio_st.next_pts;
			context->audio_st.next_pts += context->audio_st.frame->nb_samples;

			ret = avcodec_encode_audio2(context->audio_st.st->codec, &pkt, context->audio_st.tmp_frame, &got_packet);
		} else {
			av_frame_make_writable(context->audio_st.frame);
			switch_buffer_read(context->audio_buffer, context->audio_st.frame->data[0], bytes);
			context->audio_st.frame->pts = context->audio_st.next_pts;
			context->audio_st.next_pts  += context->audio_st.frame->nb_samples;

			ret = avcodec_encode_audio2(context->audio_st.st->codec, &pkt, context->audio_st.frame, &got_packet);
		}

		if (ret < 0) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "Error encoding audio frame: %d\n", ret);
			inuse = switch_buffer_inuse(context->audio_buffer);
			continue;
		}

		if (got_packet) {
			ret = write_frame(context->oc, &context->audio_st.st->codec->time_base, context->audio_st.st, &pkt);
			if (ret < 0) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Error while writing audio frame: %s\n", get_error_text(ret));
				switch_goto_status(SWITCH_STATUS_FALSE, end);
			}
		}

		inuse = switch_buffer_inuse(context->audio_buffer);
	}

	if (context->mutex) switch_mutex_unlock(context->mutex);

end:
	return status;
}

static switch_status_t av_file_read_video(switch_file_handle_t *handle, switch_frame_t *frame, switch_video_read_flag_t flags)
{
	return SWITCH_STATUS_FALSE;
}

static switch_status_t av_file_write_video(switch_file_handle_t *handle, switch_frame_t *frame)
{
	switch_status_t status = SWITCH_STATUS_SUCCESS;
	av_file_context_t *context = (av_file_context_t *)handle->private_info;

	if (!frame->img) {
		switch_goto_status(SWITCH_STATUS_FALSE, end);
	}

	if (!context->has_video) {
		context->video_st.width = frame->img->d_w;
		context->video_st.height = frame->img->d_h;
		context->video_st.next_pts = switch_time_now() / 1000;
		if (add_stream(&context->video_st, context->oc, &context->video_codec, context->oc->oformat->video_codec) == SWITCH_STATUS_SUCCESS &&
			open_video(context->oc, context->video_codec, &context->video_st) == SWITCH_STATUS_SUCCESS) {

			char codec_str[256];
			int ret;

			avcodec_string(codec_str, sizeof(codec_str), context->video_st.st->codec, 1);
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "use video codec implementation %s\n", codec_str);
			context->has_video = 1;

			// av_dump_format(context->oc, 0, "/tmp/av.mp4", 1);

			ret = avformat_write_header(context->oc, NULL);
			if (ret < 0) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Error occurred when opening output file: %s\n", get_error_text(ret));
				switch_goto_status(SWITCH_STATUS_FALSE, end);
			}

		} else {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Error adding video stream\n");
			switch_goto_status(SWITCH_STATUS_FALSE, end);
		}
	}

	if (!context->vid_ready) {
		switch_threadattr_t *thd_attr = NULL;

		switch_mutex_init(&context->mutex, SWITCH_MUTEX_NESTED, handle->memory_pool);
		context->eh.mutex = context->mutex;
		context->eh.video_st = &context->video_st;
		context->eh.oc = context->oc;
		if (switch_core_timer_init(&context->timer, "soft", 1, 1, handle->memory_pool) != SWITCH_STATUS_SUCCESS) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Timer Activation Fail\n");
			switch_goto_status(SWITCH_STATUS_FALSE, end);
		}
		context->eh.timer = &context->timer;
		switch_queue_create(&context->eh.video_queue, SWITCH_CORE_QUEUE_LEN, handle->memory_pool);

		switch_threadattr_create(&thd_attr, handle->memory_pool);
		//switch_threadattr_priority_set(thd_attr, SWITCH_PRI_REALTIME);
		switch_threadattr_stacksize_set(thd_attr, SWITCH_THREAD_STACKSIZE);
		switch_thread_create(&context->eh.video_thread, thd_attr, video_thread_run, &context->eh, handle->memory_pool);
	}

	if (context->has_video) {
		switch_image_t *img = NULL;

		switch_img_copy(frame->img, &img);
		switch_queue_push(context->eh.video_queue, img);
	}

	context->vid_ready = 1;

end:
	return status;
}

static switch_status_t av_file_set_string(switch_file_handle_t *handle, switch_audio_col_t col, const char *string)
{
	return SWITCH_STATUS_FALSE;
}

static switch_status_t av_file_get_string(switch_file_handle_t *handle, switch_audio_col_t col, const char **string)
{
	return SWITCH_STATUS_FALSE;
}


static char *supported_formats[SWITCH_MAX_CODECS] = { 0 };

SWITCH_MODULE_LOAD_FUNCTION(mod_avformat_load)
{
	switch_api_interface_t *api_interface;
	switch_application_interface_t *app_interface;
	switch_file_interface_t *file_interface;
	int i = 0;

	supported_formats[i++] = "av";
	supported_formats[i++] = "rtmp";
	supported_formats[i++] = "mp4";

	/* connect my internal structure to the blank pointer passed to me */
	*module_interface = switch_loadable_module_create_module_interface(pool, modname);

	file_interface = (switch_file_interface_t *)switch_loadable_module_create_interface(*module_interface, SWITCH_FILE_INTERFACE);
	file_interface->interface_name = modname;
	file_interface->extens = supported_formats;
	file_interface->file_open = av_file_open;
	file_interface->file_close = av_file_close;
	file_interface->file_truncate = av_file_truncate;
	file_interface->file_read = av_file_read;
	file_interface->file_write = av_file_write;
	file_interface->file_read_video = av_file_read_video;
	file_interface->file_write_video = av_file_write_video;
	file_interface->file_seek = av_file_seek;
	file_interface->file_set_string = av_file_set_string;
	file_interface->file_get_string = av_file_get_string;

	SWITCH_ADD_API(api_interface, "av_format", "av information", av_format_api_function, "");

	SWITCH_ADD_APP(app_interface, "record_av", "record video using libavformat", "record video using libavformat", record_av_function, "<file>", SAF_NONE);

	av_log_set_callback(log_callback);
	av_log_set_level(AV_LOG_INFO);
	avformat_network_init();
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
