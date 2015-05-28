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
 * Anthony Minessale <anthm@freeswitch.org>
 *
 * mod_avcodec -- Codec with libav.org
 *
 */

#include <switch.h>
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/opt.h>
#include <libavutil/imgutils.h>

#define SLICE_SIZE SWITCH_DEFAULT_VIDEO_SIZE
#define FPS 15 // frame rate

SWITCH_MODULE_LOAD_FUNCTION(mod_avcodec_load);

/*  ff_avc_find_startcode is not exposed in the ffmpeg lib but you can use it
	Either include the avc.h which available in the ffmpeg source, or
	just add the declaration like we does following to avoid include that whole avc.h
	The function is implemented in avc.h, guess we'll get rid of this later if we can directly use libx264

#include <libavformat/avc.h>
*/

const uint8_t *ff_avc_find_startcode(const uint8_t *p, const uint8_t *end);

static const uint8_t *fs_avc_find_startcode_internal(const uint8_t *p, const uint8_t *end)
{
    const uint8_t *a = p + 4 - ((intptr_t)p & 3);

    for (end -= 3; p < a && p < end; p++) {
        if (p[0] == 0 && p[1] == 0 && p[2] == 1)
            return p;
    }

    for (end -= 3; p < end; p += 4) {
        uint32_t x = *(const uint32_t*)p;
        if ((x - 0x01010101) & (~x) & 0x80808080) {
            if (p[1] == 0) {
                if (p[0] == 0 && p[2] == 1)
                    return p;
                if (p[2] == 0 && p[3] == 1)
                    return p+1;
            }
            if (p[3] == 0) {
                if (p[2] == 0 && p[4] == 1)
                    return p+2;
                if (p[4] == 0 && p[5] == 1)
                    return p+3;
            }
        }
    }

    for (end += 3; p < end; p++) {
        if (p[0] == 0 && p[1] == 0 && p[2] == 1)
            return p;
    }

    return end + 3;
}

const uint8_t *fs_avc_find_startcode(const uint8_t *p, const uint8_t *end){
    const uint8_t *out= fs_avc_find_startcode_internal(p, end);
    if(p<out && out<end && !out[-1]) out--;
    return out;
}


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
	int got_pps; /* if pps packet received */
	int64_t pts;
	int got_encoded_output;
	int nalu_current_index;
	switch_size_t last_received_timestamp;
	switch_bool_t last_received_complete_picture;
	switch_image_t *img;
	switch_image_t *encimg;
	int need_key_frame;
	switch_bool_t nalu_28_start;

	int change_bandwidth;
	unsigned int bandwidth;
	switch_codec_settings_t codec_settings;
	AVCodecContext *encoder_ctx;
	AVFrame *encoder_avframe;
	AVPacket encoder_avpacket;
	our_h264_nalu_t nalus[MAX_NALUS];
} h264_codec_context_t;

static uint8_t ff_input_buffer_padding[FF_INPUT_BUFFER_PADDING_SIZE] = { 0 };

static switch_status_t buffer_h264_nalu(h264_codec_context_t *context, switch_frame_t *frame)
{
	uint8_t nalu_type = 0;
	uint8_t *data = frame->data;
	uint8_t nalu_hdr = *data;
	uint8_t sync_bytes[] = {0, 0, 0, 1};
	switch_buffer_t *buffer = context->nalu_buffer;

	nalu_type = nalu_hdr & 0x1f;

	//switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "nalu=%02x mark=%d seq=%d ts=%d len=%d\n", nalu_hdr, frame->m, frame->seq, frame->timestamp, frame->datalen);

	if (context->got_pps <= 0) {
		context->got_pps--;
		if ((abs(context->got_pps) % 30) == 0) {
			switch_set_flag(frame, SFF_WAIT_KEY_FRAME);
		}
		//switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "waiting pps\n");
		//return SWITCH_STATUS_RESTART;
	}

	if (context->got_pps <= 0 && nalu_type == 7) context->got_pps = 1;

	/* hack for phones sending sps/pps with frame->m = 1 such as grandstream */
	if ((nalu_type == 7 || nalu_type == 8) && frame->m) frame->m = SWITCH_FALSE;

	if (nalu_type == 28) { // 0x1c FU-A
		int start = *(data + 1) & 0x80;
		int end = *(data + 1) & 0x40;

		nalu_type = *(data + 1) & 0x1f;

		if (start && end) return SWITCH_STATUS_RESTART;

		if (start) {
			if (context->nalu_28_start) {
				context->nalu_28_start = 0;
				switch_buffer_zero(buffer);
			}
		} else if (end) {
			context->nalu_28_start = 0;
		} else if (!context->nalu_28_start) {
			return SWITCH_STATUS_RESTART;
		}

		if (start) {
			uint8_t nalu_idc = (nalu_hdr & 0x60) >> 5;
			nalu_type |= (nalu_idc << 5);

			switch_buffer_write(buffer, sync_bytes, sizeof(sync_bytes));
			switch_buffer_write(buffer, &nalu_type, 1);
			context->nalu_28_start = 1;
		}

		switch_buffer_write(buffer, (void *)(data + 2), frame->datalen - 2);
	} else {
		switch_buffer_write(buffer, sync_bytes, sizeof(sync_bytes));
		switch_buffer_write(buffer, frame->data, frame->datalen);
		context->nalu_28_start = 0;
	}

	if (frame->m) {
		switch_buffer_write(buffer, ff_input_buffer_padding, sizeof(ff_input_buffer_padding));
		context->nalu_28_start = 0;
	}

	return SWITCH_STATUS_SUCCESS;
}



static switch_status_t consume_nalu(h264_codec_context_t *context, switch_frame_t *frame)
{
	our_h264_nalu_t *nalu = &context->nalus[context->nalu_current_index];

	if (!nalu->start) {
		frame->datalen = 0;
		frame->m = 0;
		if (context->encoder_avpacket.size > 0) av_free_packet(&context->encoder_avpacket);
		if (context->encoder_avframe->data[0]) av_freep(&context->encoder_avframe->data[0]);
		context->nalu_current_index = 0;
		return SWITCH_STATUS_NOTFOUND;
	}

	assert(nalu->len);

	if (nalu->len <= SLICE_SIZE) {
		uint8_t nalu_hdr = *(uint8_t *)(nalu->start);
		uint8_t nalu_type = nalu_hdr & 0x1f;

		memcpy(frame->data, nalu->start, nalu->len);
		frame->datalen = nalu->len;
		context->nalu_current_index++;
		if (nalu_type == 6 || nalu_type == 7 || nalu_type == 8) {
			frame->m = 0;
			return SWITCH_STATUS_MORE_DATA;
		}

		frame->m = 1;
		return SWITCH_STATUS_SUCCESS;
	} else {
		uint8_t nalu_hdr = *(uint8_t *)(nalu->start);
		uint8_t nri = nalu_hdr & 0x60;
		uint8_t nalu_type = nalu_hdr & 0x1f;
		int left = nalu->len - (nalu->eat - nalu->start);
		uint8_t *p = frame->data;

		if (left <= (SLICE_SIZE - 2)) {
			p[0] = nri | 28; // FU-A
			p[1] = 0x40 | nalu_type;
			memcpy(p+2, nalu->eat, left);
			nalu->eat += left;
			frame->datalen = left + 2;
			frame->m = 1;
			context->nalu_current_index++;
			return SWITCH_STATUS_SUCCESS;
		} else {
			uint8_t start = nalu->start == nalu->eat ? 0x80 : 0;

			p[0] = nri | 28; // FU-A
			p[1] = start | nalu_type;
			if (start) nalu->eat++;
			memcpy(p+2, nalu->eat, SLICE_SIZE - 2);
			nalu->eat += (SLICE_SIZE - 2);
			frame->datalen = SLICE_SIZE;
			return SWITCH_STATUS_MORE_DATA;
		}
	}
}

static switch_status_t open_encoder(h264_codec_context_t *context, uint32_t width, uint32_t height)
{

	if (!context->encoder) context->encoder = avcodec_find_encoder(AV_CODEC_ID_H264);

	if (!context->encoder) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Cannot find AV_CODEC_ID_H264 encoder\n");
		return SWITCH_STATUS_FALSE;
	}

	if (context->encoder_ctx) {
		if (avcodec_is_open(context->encoder_ctx)) {
			avcodec_close(context->encoder_ctx);
		}
		av_free(context->encoder_ctx);
		context->encoder_ctx = NULL;
	}

	context->encoder_ctx = avcodec_alloc_context3(context->encoder);

	if (!context->encoder_ctx) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Could not allocate video encoder context\n");
		return SWITCH_STATUS_FALSE;
	}

	if (width && height) {
		context->codec_settings.video.width = width;
		context->codec_settings.video.height = height;
	}

	if (!context->codec_settings.video.width) {
		context->codec_settings.video.width = 1280;
	}

	if (!context->codec_settings.video.height) {
		context->codec_settings.video.height = 720;
	}

	if (context->codec_settings.video.bandwidth) {
		context->bandwidth = context->codec_settings.video.bandwidth;
	} else {
		context->bandwidth = switch_calc_bitrate(context->codec_settings.video.width, context->codec_settings.video.height, 0, 0);
	}

	if (context->bandwidth > 5120) {
		context->bandwidth = 5120;
	}

	//context->encoder_ctx->bit_rate = context->bandwidth * 1024;
	context->encoder_ctx->width = context->codec_settings.video.width;
	context->encoder_ctx->height = context->codec_settings.video.height;
	/* frames per second */
	context->encoder_ctx->time_base = (AVRational){1, 90};
	context->encoder_ctx->gop_size = FPS * 10; /* emit one intra frame every 3 seconds */
	context->encoder_ctx->max_b_frames = 0;
	context->encoder_ctx->pix_fmt = AV_PIX_FMT_YUV420P;
	context->encoder_ctx->thread_count = 1;//switch_core_cpu_count() > 4 ? 4 : 1;

	context->encoder_ctx->bit_rate = context->bandwidth * 1024;
	context->encoder_ctx->rc_max_rate = context->bandwidth * 1024;
	context->encoder_ctx->rc_buffer_size = context->bandwidth * 1024 * 2;
	
	context->encoder_ctx->rtp_payload_size = SLICE_SIZE;
	context->encoder_ctx->profile = FF_PROFILE_H264_BASELINE;
	context->encoder_ctx->level = 31;
	av_opt_set(context->encoder_ctx->priv_data, "preset", "veryfast", 0);
	av_opt_set(context->encoder_ctx->priv_data, "tune", "zerolatency", 0);
	av_opt_set(context->encoder_ctx->priv_data, "profile", "baseline", 0);
	//av_opt_set_int(context->encoder_ctx->priv_data, "slice-max-size", SLICE_SIZE, 0);

	if (avcodec_open2(context->encoder_ctx, context->encoder, NULL) < 0) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Could not open codec\n");
		return SWITCH_STATUS_FALSE;
	}

	return SWITCH_STATUS_SUCCESS;
}

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

		if (codec_settings) {
			context->codec_settings = *codec_settings;
		}

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
			// never mind
		}

		switch_buffer_create_dynamic(&(context->nalu_buffer), H264_NALU_BUFFER_SIZE, H264_NALU_BUFFER_SIZE * 8, 0);
		codec->private_info = context;

		return SWITCH_STATUS_SUCCESS;
	}

error:
	// todo, do some clean up
	return SWITCH_STATUS_FALSE;
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

static switch_status_t switch_h264_encode(switch_codec_t *codec, switch_frame_t *frame)
{
	h264_codec_context_t *context = (h264_codec_context_t *)codec->private_info;
	AVCodecContext *avctx = context->encoder_ctx;
	int ret;
	int *got_output = &context->got_encoded_output;
	AVFrame *avframe = NULL;
	AVPacket *pkt = &context->encoder_avpacket;
	uint32_t width = 0;
	uint32_t height = 0;
	switch_image_t *img = frame->img;

	switch_assert(frame);
	frame->m = 0;

	if (frame->datalen < SWITCH_DEFAULT_VIDEO_SIZE) return SWITCH_STATUS_FALSE;

	if (frame->flags & SFF_SAME_IMAGE) {
		// read from nalu buffer
		return consume_nalu(context, frame);
	}

	width = img->d_w;
	height = img->d_h;

	if (!avctx || !avcodec_is_open(avctx)) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "initializing encoder %dx%d\n", width, height);
		if (open_encoder(context, width, height) != SWITCH_STATUS_SUCCESS) {
			goto error;
		}
		avctx = context->encoder_ctx;
	}

	if (avctx->width != width || avctx->height != height) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "picture size changed from %dx%d to %dx%d, reinitializing encoder\n",
						  avctx->width, avctx->height, width, height);
		if (open_encoder(context, width, height) != SWITCH_STATUS_SUCCESS) {
			goto error;
		}
		avctx = context->encoder_ctx;
	}

	if (context->change_bandwidth) {
		context->codec_settings.video.bandwidth = context->change_bandwidth;
		context->change_bandwidth = 0;
		if (open_encoder(context, width, height) != SWITCH_STATUS_SUCCESS) {
			goto error;
		}
		avctx = context->encoder_ctx;
		switch_set_flag(frame, SFF_WAIT_KEY_FRAME);
	}

	av_init_packet(pkt);
	pkt->data = NULL;      // packet data will be allocated by the encoder
	pkt->size = 0;

	avframe = context->encoder_avframe;

	if (avframe) {
		if (avframe->width != width || avframe->height != height) {
			av_frame_free(&avframe);
		}
	}

	if (!avframe) {
		avframe = av_frame_alloc();
		context->encoder_avframe = avframe;

		if (!avframe) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Error allocate frame!\n");
			goto error;
		}

		avframe->format = avctx->pix_fmt;
		avframe->width  = avctx->width;
		avframe->height = avctx->height;
		avframe->pts = frame->timestamp / 1000;

		ret = av_frame_get_buffer(avframe, 32);

		if (ret < 0) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Could not allocate raw picture buffer\n");
			av_frame_free(&context->encoder_avframe);
			goto error;
		}
	}

	if (*got_output) { // Could be more delayed frames
		ret = avcodec_encode_video2(avctx, pkt, NULL, got_output);
		if (ret < 0) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Encoding Error %d\n", ret);
			goto error;
		}

		if (*got_output) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG1, "Encoded frame %" SWITCH_INT64_T_FMT " (size=%5d) nalu_type=0x%x %d\n", context->pts, pkt->size, *((uint8_t *)pkt->data +4), *got_output);
			goto process;
		}
	}

	fill_avframe(avframe, img);

	//avframe->pts = context->pts++;

	if (context->need_key_frame) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "Send AV KEYFRAME\n");
		 av_opt_set_int(context->encoder_ctx->priv_data, "intra-refresh", 1, 0);
		 avframe->pict_type = AV_PICTURE_TYPE_I;
	}

	/* encode the image */
	ret = avcodec_encode_video2(avctx, pkt, avframe, got_output);

	if (ret < 0) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Encoding Error %d\n", ret);
		goto error;
	}

	if (context->need_key_frame) {
		avframe->pict_type = 0;
		context->need_key_frame = 0;
	}

process:

	if (*got_output) {
		const uint8_t *p = pkt->data;
		int i = 0;

		//switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG1, "Encoded frame %" SWITCH_INT64_T_FMT " (size=%5d) nalu_type=0x%x %d\n", context->pts, pkt->size, *((uint8_t *)pkt->data +4), *got_output);

		/* split into nalus */
		memset(context->nalus, 0, sizeof(context->nalus));

		while ((p = fs_avc_find_startcode(p, pkt->data+pkt->size)) < (pkt->data + pkt->size)) {
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
		return consume_nalu(context, frame);
	}

error:
	frame->datalen = 0;
	return SWITCH_STATUS_FALSE;
}

static switch_status_t switch_h264_decode(switch_codec_t *codec, switch_frame_t *frame)
{
	h264_codec_context_t *context = (h264_codec_context_t *)codec->private_info;
	AVCodecContext *avctx= context->decoder_ctx;
	switch_status_t status;

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

	status = buffer_h264_nalu(context, frame);

	if (status == SWITCH_STATUS_RESTART) {
		switch_set_flag(frame, SFF_WAIT_KEY_FRAME);
		switch_buffer_zero(context->nalu_buffer);
		context->nalu_28_start = 0;
		return SWITCH_STATUS_MORE_DATA;
	}

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
				int i;
				
				if (!context->img || (context->img->d_w != width || context->img->d_h != height)) {
					//context->img = switch_img_wrap(NULL, SWITCH_IMG_FMT_I420, width, height, 0, picture->data[0]);
					context->img = switch_img_alloc(NULL, SWITCH_IMG_FMT_I420, width, height, 1);
					assert(context->img);
				}

				context->img->w = picture->linesize[0];
				context->img->h = picture->linesize[1];
				context->img->d_w = width;
				context->img->d_h = height;

				//context->img->planes[0] = picture->data[0];
				//context->img->planes[1] = picture->data[1];
				//context->img->planes[2] = picture->data[2];
				//context->img->stride[0] = picture->linesize[0];
				//context->img->stride[1] = picture->linesize[1];
				//context->img->stride[2] = picture->linesize[2];
				
				for (i = 0; i < height; i++) {
					memcpy(context->img->planes[SWITCH_PLANE_Y] + context->img->stride[SWITCH_PLANE_Y] * i, 
						   picture->data[SWITCH_PLANE_Y] + picture->linesize[SWITCH_PLANE_Y] * i, width);
				}
				
				for (i = 0; i < height / 2; i++) {
					memcpy(context->img->planes[SWITCH_PLANE_U] + context->img->stride[SWITCH_PLANE_U] * i, 
						   picture->data[SWITCH_PLANE_U] + picture->linesize[SWITCH_PLANE_U] * i, width / 2);
					memcpy(context->img->planes[SWITCH_PLANE_V] + context->img->stride[SWITCH_PLANE_V] * i, 
						   picture->data[SWITCH_PLANE_V] + picture->linesize[SWITCH_PLANE_V] * i, width / 2);
				}

				frame->img = context->img;
			}

			av_frame_free(&picture);
			av_free_packet(&pkt);
		}

		switch_buffer_zero(context->nalu_buffer);
		context->nalu_28_start = 0;
		//switch_set_flag(frame, SFF_USE_VIDEO_TIMESTAMP);
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
	case SCC_VIDEO_BANDWIDTH:
		{
			switch(ctype) {
			case SCCT_INT:
				context->change_bandwidth = *((int *) cmd_data);
				break;
			case SCCT_STRING:
				{
					char *bwv = (char *) cmd_data;
					context->change_bandwidth = switch_parse_bandwidth_string(bwv);
				}
				break;
			default:
				break;
			}
		}
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

	switch_img_free(&context->encimg);

	switch_buffer_destroy(&context->nalu_buffer);
	if (context->decoder_ctx) {
		if (avcodec_is_open(context->decoder_ctx)) avcodec_close(context->decoder_ctx);
		av_free(context->decoder_ctx);
	}

	switch_img_free(&context->img);

	if (context->encoder_ctx) {
		if (avcodec_is_open(context->encoder_ctx)) avcodec_close(context->encoder_ctx);
		av_free(context->encoder_ctx);
	}

	if (context->encoder_avframe) {
		av_frame_free(&context->encoder_avframe);
	}

	return SWITCH_STATUS_SUCCESS;
}

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


SWITCH_STANDARD_API(av_codec_api_function)
{
	show_codecs(stream);

	return SWITCH_STATUS_SUCCESS;
}

SWITCH_MODULE_LOAD_FUNCTION(mod_avcodec_load)
{
	switch_codec_interface_t *codec_interface;
	switch_api_interface_t *api_interface;

	SWITCH_ADD_CODEC(codec_interface, "H264 Video");
	switch_core_codec_add_video_implementation(pool, codec_interface, 99, "H264", NULL,
											   switch_h264_init, switch_h264_encode, switch_h264_decode, switch_h264_control, switch_h264_destroy);

	SWITCH_ADD_API(api_interface, "av_codec", "av_codec information", av_codec_api_function, "");

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
