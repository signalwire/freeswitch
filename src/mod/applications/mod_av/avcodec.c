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
 * Emmanuel Schmidbauer <eschmidbauer@gmail.com>
 *
 * mod_avcodec -- Codec with libav.org and ffmpeg
 *
 */

#include <switch.h>
#include "mod_av.h"
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/opt.h>
#include <libavutil/imgutils.h>

int SLICE_SIZE = (SWITCH_DEFAULT_VIDEO_SIZE + 100);

#define H264_NALU_BUFFER_SIZE 65536
#define MAX_NALUS 256
#define H263_MODE_B // else Mode A only
#define KEY_FRAME_MIN_FREQ 250000

// #define DUMP_ENCODER_CTX

SWITCH_MODULE_LOAD_FUNCTION(mod_avcodec_load);
SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_avcodec_shutdown);

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

    if (p < out && out < end && !out[-1]) {
		out--;
	}

    return out;
}

static void dump_encoder_ctx(AVCodecContext *ctx)
{
#ifdef DUMP_ENCODER_CTX
#define STRINGIFY(x) #x
#define my_dump_int(x) switch_log_printf(SWITCH_CHANNEL_LOG_CLEAN, SWITCH_LOG_ERROR, STRINGIFY(x) " = %d\n", ctx->x);
#define my_dump_int64(x) switch_log_printf(SWITCH_CHANNEL_LOG_CLEAN, SWITCH_LOG_ERROR, STRINGIFY(x) " = %" SWITCH_INT64_T_FMT "\n", ctx->x);
#define my_dump_float(x) switch_log_printf(SWITCH_CHANNEL_LOG_CLEAN, SWITCH_LOG_ERROR, STRINGIFY(x) " = %f\n", ctx->x);
#define my_dump_enum(x) switch_log_printf(SWITCH_CHANNEL_LOG_CLEAN, SWITCH_LOG_ERROR, STRINGIFY(x) " = %d\n", ctx->x);
#define my_dump_uint(x) switch_log_printf(SWITCH_CHANNEL_LOG_CLEAN, SWITCH_LOG_ERROR, STRINGIFY(x) " = 0x%x\n", ctx->x);

	my_dump_int(log_level_offset);
	my_dump_enum(codec_type); /* see AVMEDIA_TYPE_xxx */
	my_dump_enum(codec_id); /* see AV_CODEC_ID_xxx */
	my_dump_int(codec_tag);
	my_dump_int64(bit_rate);
	my_dump_int(bit_rate_tolerance);
	my_dump_int(global_quality);
	my_dump_int(compression_level);
	my_dump_uint(flags);
	my_dump_uint(flags2);
	my_dump_int(extradata_size);
	my_dump_int(time_base.num);
	my_dump_int(time_base.den);
	my_dump_int(ticks_per_frame);
	my_dump_int(delay);
	my_dump_int(width);
	my_dump_int(height);
	my_dump_int(coded_width);
	my_dump_int(coded_height);
	my_dump_int(gop_size);
	my_dump_enum(pix_fmt);
	my_dump_int(max_b_frames);
	my_dump_float(b_quant_factor);
	my_dump_float(b_quant_offset);
	my_dump_int(has_b_frames);
	my_dump_float(i_quant_factor);
	my_dump_float(i_quant_offset);
	my_dump_float(lumi_masking);
	my_dump_float(temporal_cplx_masking);
	my_dump_float(spatial_cplx_masking);
	my_dump_float(p_masking);
	my_dump_float(dark_masking);
	my_dump_int(slice_count);
	my_dump_int(sample_aspect_ratio.num);
	my_dump_int(sample_aspect_ratio.den);
	my_dump_int(me_cmp);
	my_dump_int(me_sub_cmp);
	my_dump_int(mb_cmp);
	my_dump_int(ildct_cmp);
	my_dump_int(dia_size);
	my_dump_int(last_predictor_count);
	my_dump_int(me_pre_cmp);
	my_dump_int(pre_dia_size);
	my_dump_int(me_subpel_quality);
	my_dump_int(me_range);
	my_dump_uint(slice_flags);
	my_dump_int(mb_decision);
	// my_dump_int(scenechange_threshold);
	// my_dump_int(noise_reduction);
	// my_dump_int(me_threshold);
	// my_dump_int(mb_threshold);
	my_dump_int(intra_dc_precision);
	my_dump_int(skip_top);
	my_dump_int(skip_bottom);
	my_dump_int(mb_lmin);
	my_dump_int(mb_lmax);
	// my_dump_int(me_penalty_compensation);
	my_dump_int(bidir_refine);
	// my_dump_int(brd_scale);
	my_dump_int(keyint_min);
	my_dump_int(refs);
	// my_dump_int(chromaoffset);
	my_dump_int(mv0_threshold);
	// my_dump_int(b_sensitivity);
	my_dump_enum(color_primaries);
	my_dump_enum(color_trc);
	my_dump_enum(colorspace);
	my_dump_enum(color_range);
	my_dump_enum(chroma_sample_location);
	my_dump_int(slices);
	my_dump_enum(field_order);
	my_dump_int(sample_rate); ///< samples per second
	my_dump_int(channels);    ///< number of audio channels
	my_dump_enum(sample_fmt);  ///< sample format
	my_dump_int(frame_size);
	my_dump_int(frame_number);
	my_dump_int(block_align);
	my_dump_int(cutoff);
	my_dump_int64(channel_layout);
	my_dump_int64(request_channel_layout);
	my_dump_enum(audio_service_type);
	my_dump_enum(request_sample_fmt);
#if (LIBAVCODEC_VERSION_INT < AV_VERSION_INT(57,106,102))
	my_dump_int(refcounted_frames);
#endif
	my_dump_float(qcompress);  ///< amount of qscale change between easy & hard scenes (0.0-1.0)
	my_dump_float(qblur);      ///< amount of qscale smoothing over time (0.0-1.0)
	my_dump_int(qmin);
	my_dump_int(qmax);
	my_dump_int(max_qdiff);
	my_dump_int(rc_buffer_size);
	my_dump_int(rc_override_count);
	my_dump_int64(rc_max_rate);
	my_dump_int64(rc_min_rate);
	my_dump_float(rc_max_available_vbv_use);
	my_dump_float(rc_min_vbv_overflow_use);
	my_dump_int(rc_initial_buffer_occupancy);
	my_dump_int(trellis);
	my_dump_int(workaround_bugs);
	my_dump_int(strict_std_compliance);
	my_dump_int(error_concealment);
	my_dump_int(debug);
	my_dump_int(debug_mv);
	my_dump_int(err_recognition);
	my_dump_int64(reordered_opaque);
	my_dump_int(dct_algo);
	my_dump_int(idct_algo);
	my_dump_int(bits_per_coded_sample);
	my_dump_int(bits_per_raw_sample);
	my_dump_int(lowres);
	my_dump_int(thread_count);
	my_dump_int(thread_type);
	my_dump_int(active_thread_type);
	my_dump_int(thread_safe_callbacks);
	my_dump_int(nsse_weight);
	my_dump_int(profile);
	my_dump_int(level);
	my_dump_enum(skip_loop_filter);
	my_dump_enum(skip_idct);
	my_dump_enum(skip_frame);
	my_dump_int(subtitle_header_size);
	my_dump_int(initial_padding);
	my_dump_int(framerate.num);
	my_dump_int(framerate.den);
	my_dump_enum(sw_pix_fmt);
	my_dump_int(pkt_timebase.num);
	my_dump_int(pkt_timebase.den);
	my_dump_int(lowres);
	my_dump_int64(pts_correction_num_faulty_pts); /// Number of incorrect PTS values so far
	my_dump_int64(pts_correction_num_faulty_dts); /// Number of incorrect DTS values so far
	my_dump_int64(pts_correction_last_pts);       /// PTS of the last frame
	my_dump_int64(pts_correction_last_dts);       /// DTS of the last frame
	my_dump_int(sub_charenc_mode);
	my_dump_int(skip_alpha);
	my_dump_int(seek_preroll);
	my_dump_int(debug_mv);
	my_dump_int(sub_text_format);
	my_dump_int(trailing_padding);
	// my_dump_int64(max_pixels);
	// my_dump_int(hwaccel_flags);
	// my_dump_int(apply_cropping);

#if 0
	// depracated
	my_dump_int(rc_strategy);
	my_dump_int(b_frame_strategy);
	my_dump_int(prediction_method);
	my_dump_int(pre_me);
	my_dump_int(intra_quant_bias);
	my_dump_int(inter_quant_bias);
	my_dump_int(xvmc_acceleration);
	my_dump_int(scenechange_factor);
	my_dump_float(rc_qsquish);
	my_dump_float(rc_qmod_amp);
	my_dump_int(rc_qmod_freq);
	my_dump_float(rc_buffer_aggressivity);
	my_dump_float(rc_initial_cplx);
	my_dump_int(coder_type);
	my_dump_int(context_model);
	// my_dump_int(lmin);
	// my_dump_int(lmax);
	my_dump_int(frame_skip_threshold);
	my_dump_int(frame_skip_factor);
	my_dump_int(frame_skip_exp);
	my_dump_int(frame_skip_cmp);
	my_dump_int(min_prediction_order);
	my_dump_int(max_prediction_order);
	my_dump_int64(timecode_frame_start);
	my_dump_int(rtp_payload_size);
	my_dump_int(mv_bits);
	my_dump_int(header_bits);
	my_dump_int(i_tex_bits);
	my_dump_int(p_tex_bits);
	my_dump_int(i_count);
	my_dump_int(p_count);
	my_dump_int(skip_count);
	my_dump_int(misc_bits);
	my_dump_int(frame_bits);
	// my_dump_int64(vbv_delay);
	my_dump_int(side_data_only_packets);
#endif


#undef my_dump_int
#undef my_dump_int64
#undef my_dump_float
#undef my_dump_enum
#undef my_dump_uint

#endif
}


/* RFC 2190 MODE A
    0                   1                   2                   3
    0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |F|P|SBIT |EBIT | SRC |I|U|S|A|R      |DBQ| TRB |    TR         |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
*/

#if SWITCH_BYTE_ORDER == __BIG_ENDIAN

typedef struct h263_payload_header_s{
	unsigned f:1;
	unsigned p:1;
	unsigned sbit:3;
	unsigned ebit:3;
	unsigned src:3;
	unsigned i:1;
	unsigned u:1;
	unsigned s:1;
	unsigned a:1;
	unsigned r1:1;
	unsigned r3:3;
	unsigned dbq:2;
	unsigned trb:3;
	unsigned tr:8;
} h263_payload_header_t;

#else // LITTLE

typedef struct h263_payload_header_s {
	unsigned ebit:3;
	unsigned sbit:3;
	unsigned p:1;
	unsigned f:1;
	unsigned r1:1;
	unsigned a:1;
	unsigned s:1;
	unsigned u:1;
	unsigned i:1;
	unsigned src:3;
	unsigned trb:3;
	unsigned dbq:2;
	unsigned r3:3;
	unsigned tr:8;
} h263_payload_header_t;

#endif

typedef struct h263_state_s {
    int gobn;
    int mba;
    uint8_t hmv1, vmv1, hmv2, vmv2;
    int quant;
} h263_state_t;

typedef struct our_h264_nalu_s {
	const uint8_t *start;
	const uint8_t *eat;
	uint32_t len;
	h263_payload_header_t h263_header;
	h263_state_t h263_state;
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
	AVFrame *decoder_avframe;
	our_h264_nalu_t nalus[MAX_NALUS];
	enum AVCodecID av_codec_id;
	uint16_t last_seq; // last received frame->seq
	int hw_encoder;
} h264_codec_context_t;

#ifndef AV_INPUT_BUFFER_PADDING_SIZE
#define AV_INPUT_BUFFER_PADDING_SIZE FF_INPUT_BUFFER_PADDING_SIZE
#endif

static uint8_t ff_input_buffer_padding[AV_INPUT_BUFFER_PADDING_SIZE] = { 0 };

#define MAX_PROFILES 100

typedef struct avcodec_profile_s {
	char name[64];
	int decoder_thread_count;
	AVCodecContext ctx;
	switch_event_t *options;
	switch_event_t *codecs;
} avcodec_profile_t;

struct avcodec_globals {
	int debug;
	uint32_t max_bitrate;
	uint32_t rtp_slice_size;
	uint32_t key_frame_min_freq;
	uint32_t enc_threads;
	uint32_t dec_threads;

	avcodec_profile_t *profiles[MAX_PROFILES];
};

struct avcodec_globals avcodec_globals = { 0 };

const char *get_profile_name(int codec_id)
{
	switch (codec_id) {
		case AV_CODEC_ID_H263: return "H263";
		case AV_CODEC_ID_H263P: return "H263+";
		case AV_CODEC_ID_H264: return "H264";
#ifdef AV_CODEC_ID_H265
		case AV_CODEC_ID_H265: return "H265";
#endif
	}

	return "NONE";
}

static void parse_profile(avcodec_profile_t *aprofile, switch_xml_t profile);

static void parse_codec_specific_profile(avcodec_profile_t *aprofile, const char *codec_name)
{
	switch_xml_t cfg = NULL;
	switch_xml_t xml = switch_xml_open_cfg("avcodec.conf", &cfg, NULL);
	switch_xml_t profiles = cfg ? switch_xml_child(cfg, "profiles") : NULL;

	// open config and find the profile to parse
	if (profiles) {
		switch_event_header_t *hp;

		for (hp = aprofile->codecs->headers; hp; hp = hp->next) {
			// switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "%s: %s\n", hp->name, hp->value);
			if (!strcmp(hp->name, codec_name)) {
				switch_xml_t profile;
				for (profile = switch_xml_child(profiles, "profile"); profile; profile = profile->next) {
					const char *name = switch_xml_attr(profile, "name");

					if (!strcmp(hp->value, name)) {
						parse_profile(aprofile, profile);
					}
				}
			}
		}
	}

	if (xml) switch_xml_free(xml);
}

static void init_profile(avcodec_profile_t *aprofile, const char *name);

static avcodec_profile_t *find_profile(const char *name, switch_bool_t reconfig)
{
	int i;

	for (i = 0; i < MAX_PROFILES; i++) {
		if (!avcodec_globals.profiles[i]) {
			avcodec_globals.profiles[i] = malloc(sizeof(avcodec_profile_t));
			switch_assert(avcodec_globals.profiles[i]);
			memset(avcodec_globals.profiles[i], 0, sizeof(avcodec_profile_t));
			init_profile(avcodec_globals.profiles[i], name);
			return avcodec_globals.profiles[i];
		}

		if (!strcmp(name, avcodec_globals.profiles[i]->name)) {
			if (reconfig) init_profile(avcodec_globals.profiles[i], name);
			return avcodec_globals.profiles[i];
		}
	}

	return NULL;
}

#define UINTVAL(v) (v > 0 ? v : 0);

static void init_profile(avcodec_profile_t *aprofile, const char *name)
{
	switch_set_string(aprofile->name, name);
	aprofile->ctx.colorspace = AVCOL_SPC_RGB;
	aprofile->ctx.color_range = AVCOL_RANGE_JPEG;
	aprofile->ctx.flags = 0;
	aprofile->ctx.me_cmp = -1;
	aprofile->ctx.me_range = -1;
	aprofile->ctx.max_b_frames = -1;
	aprofile->ctx.refs = -1;
	aprofile->ctx.gop_size = -1;
	aprofile->ctx.keyint_min = -1;
	aprofile->ctx.i_quant_factor = -1;
	aprofile->ctx.b_quant_factor = -1;
	aprofile->ctx.qcompress = -1;
	aprofile->ctx.qmin = -1;
	aprofile->ctx.qmax = -1;
	aprofile->ctx.max_qdiff = -1;
	aprofile->ctx.thread_count = avcodec_globals.enc_threads;
	aprofile->decoder_thread_count = avcodec_globals.dec_threads;

	if (!strcasecmp(name, "H264")) {
		aprofile->ctx.profile = FF_PROFILE_H264_BASELINE;
		aprofile->ctx.level = 31;
#ifdef AV_CODEC_FLAG_PSNR
		aprofile->ctx.flags |= AV_CODEC_FLAG_PSNR;
#endif
#ifdef CODEC_FLAG_LOOP_FILTER
		aprofile->ctx.flags |= CODEC_FLAG_LOOP_FILTER;
#endif
	}
}

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
	} else if (nalu_type == 24) { // 0x18 STAP-A
		uint16_t nalu_size;
		int left = frame->datalen - 1;

		data++;

	again:
		if (left > 2) {
			nalu_size = ntohs(*(uint16_t *)data);
			data += 2;
			left -= 2;

			if (nalu_size > left) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "INVALID PACKET\n");
				context->got_pps = 0;
				switch_buffer_zero(buffer);
				return SWITCH_STATUS_FALSE;
			}

			nalu_hdr = *data;
			nalu_type = nalu_hdr & 0x1f;

			if (context->got_pps <= 0 && nalu_type == 7) context->got_pps = 1;

			switch_buffer_write(buffer, sync_bytes, sizeof(sync_bytes));
			switch_buffer_write(buffer, (void *)data, nalu_size);
			data += nalu_size;
			left -= nalu_size;
			goto again;
		}
	} else {
		switch_buffer_write(buffer, sync_bytes, sizeof(sync_bytes));
		switch_buffer_write(buffer, frame->data, frame->datalen);
		context->nalu_28_start = 0;
	}

	if (frame->m) {
		context->nalu_28_start = 0;
	}

	return SWITCH_STATUS_SUCCESS;
}

static inline int is_valid_h263_dimension(int width, int height)
{
	return ((width == 128 && height == 96) ||
			(width == 176 && height == 144) ||
			(width == 352 && height == 288) ||
			(width == 704 && height == 576) ||
			(width == 1408 && height == 1152));
}

static switch_status_t buffer_h263_packets(h264_codec_context_t *context, switch_frame_t *frame)
{
	uint8_t *data = frame->data;
	int header_len = 4; // Mode A, default
	h263_payload_header_t *h = (h263_payload_header_t *)data;
	int delta = 0;

	if (h->f) {
		if (h->p) {
			header_len = 12; // Mode C
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "H263 Mode C is unspported\n");
		} else {
			header_len = 8; // Mode B
		}
	}

#if 0
	//emulate packet loss
	static int z = 0;
	if ((z++ % 200 == 0) && h->i) return SWITCH_STATUS_RESTART;
#endif

#if 0
	if (h->i == 0 && frame->m) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Got a H263 Key Frame\n");
	}
#endif

	delta = frame->seq - context->last_seq;

	if (delta > 1) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "packet loss? frame seq: %d last seq: %d, delta = %d\n", frame->seq, context->last_seq, delta);
		if (delta > 2) { // wait for key frame
			if (h->i) {
				switch_set_flag(frame, SFF_WAIT_KEY_FRAME);
				return SWITCH_STATUS_RESTART;
			} else { // key frame
				context->last_seq = frame->seq;
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "Received a H263 key frame after delta %d\n", delta);
			}
		}
		// return SWITCH_STATUS_RESTART;
	} else {
		context->last_seq = frame->seq;
	}

	if (0) {
		static char *h263_src[] = {
			"NONE",
			"subQCIF",
			"QCIF",
			"CIF",
			"4CIF",
			"16CIF",
			"Reserved-110",
			"Reserved-111"
		};
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "SRC: %s\n", h263_src[h->src]);
	}

	if (frame->datalen <= header_len) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "INVALID PACKET\n");
		return SWITCH_STATUS_FALSE;
	}

	if (h->sbit) {
		int inuse = switch_buffer_inuse(context->nalu_buffer);
		const void *old_data = NULL;
		if (!inuse) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "Ignore incomplete packet\n");
			return SWITCH_STATUS_RESTART;
		}

		switch_buffer_peek_zerocopy(context->nalu_buffer, &old_data);
		*((uint8_t *)old_data + inuse - 1) |= (((*(data + header_len) << h->sbit) & 0xFF) >> h->sbit);
		switch_buffer_write(context->nalu_buffer, data + header_len + 1, frame->datalen - header_len - 1);
	} else {
		switch_buffer_write(context->nalu_buffer, data + header_len, frame->datalen - header_len);
	}

	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t buffer_h263_rfc4629_packets(h264_codec_context_t *context, switch_frame_t *frame)
{
	uint8_t *data = frame->data;
	uint16_t header = ntohs(*((uint16_t *)frame->data));
	int startcode, vrc, picture_header;
	int len = frame->datalen;

	if (frame->datalen < 2) {
		return SWITCH_STATUS_FALSE;
	}

	startcode      = (header & 0x0400) >> 9;
	vrc            =  header & 0x0200;
	picture_header = (header & 0x01f8) >> 3;
	data += 2;
	len -= 2;

	if (vrc) {
		data += 1;
		len -= 1;
	}

	if (picture_header) {
		data += picture_header;
		len -= picture_header;
	}

	if (len < 0) return SWITCH_STATUS_FALSE;

	if (startcode) {
		uint8_t zeros[2] = { 0 };
		switch_buffer_write(context->nalu_buffer, zeros, 2);
	}

	switch_buffer_write(context->nalu_buffer, data, len);

	return SWITCH_STATUS_SUCCESS;
}

#ifndef H263_MODE_B
/* this function is depracated from ffmpeg 3.0 and
   https://lists.libav.org/pipermail/libav-devel/2015-October/072782.html
*/
void rtp_callback(struct AVCodecContext *avctx, void *data, int size, int mb_nb)
{
	uint8_t *d = data;
	uint32_t code = (ntohl(*(uint32_t *)data) & 0xFFFFFC00) >> 10;
	h264_codec_context_t *context = (h264_codec_context_t *)avctx->opaque;

	switch_assert(context);

	if (code == 0x20) { // start
		context->nalu_current_index = 0;
		context->nalus[context->nalu_current_index].h263_header.src = (*(d + 4) & 0x0B) >> 2;
		context->nalus[context->nalu_current_index].h263_header.i   = (*(d + 4) & 0x02) >> 1;
	} else {
		context->nalus[context->nalu_current_index].h263_header.src = context->nalus[0].h263_header.src;
		context->nalus[context->nalu_current_index].h263_header.i   = context->nalus[0].h263_header.i;
	}

	context->nalus[context->nalu_current_index].start = data;
	context->nalus[context->nalu_current_index].len = size;
	context->nalu_current_index++;

#if 0
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR,
		"size: %d mb_nb: %d [%02x %02x %02x %02x] %x index: %d %s\n",
		size, mb_nb, *d, *(d+1), *(d+2), *(d+3), code, context->nalu_current_index,
		size > 1500 ? "===============Exceedding MTU===============" : "");
#endif

}
#endif

#ifdef WIN32
const uint8_t *fs_h263_find_resync_marker_reverse(const uint8_t *av_restrict start,
												  const uint8_t *av_restrict end)
#else
const uint8_t *fs_h263_find_resync_marker_reverse(const uint8_t *restrict start,
												  const uint8_t *restrict end)
#endif
{
	const uint8_t *p = end - 1;
	start += 1; /* Make sure we never return the original start. */
	for (; p > start; p -= 2) {
		if (!*p) {
			if      (!p[ 1] && p[2]) return p;
			else if (!p[-1] && p[1]) return p - 1;
		}
	}
	return end;
}

#ifdef H263_MODE_B

static void fs_rtp_parse_h263_rfc2190(h264_codec_context_t *context, AVPacket *pkt)
{
	int len, sbits = 0, ebits = 0;
	h263_payload_header_t h = { 0 };
	h263_state_t state = { 0 };
	int size = pkt->size;
	const uint8_t *buf = pkt->data;
	const uint8_t *p = buf;
	const uint8_t *buf_base = buf;
	uint32_t code = (ntohl(*(uint32_t *)buf) & 0xFFFFFC00) >> 10;
	int mb_info_size = 0;
	int mb_info_pos = 0, mb_info_count = 0;
	const uint8_t *mb_info;

	mb_info = av_packet_get_side_data(pkt, AV_PKT_DATA_H263_MB_INFO, &mb_info_size);
	mb_info_count = mb_info_size / 12;

	if (size < 4) return;

	if (code == 0x20) { /* Picture Start Code */
		h.tr  = (ntohl(*(uint32_t *)buf) & 0x000003FC) >> 2;
		p += 4;
		h.src = ((*p) & 0x1C) >> 2;
		h.i   = ((*p) & 0x02) >> 1;
		h.u   = ((*p) & 0x01);
		p++;
		h.s   = ((*p) & 0x80) >> 7;
		h.a   = ((*p) & 0x40) >> 6;
	}

	while (size > 0) {
		h263_state_t packet_start_state = state;
		len = (SLICE_SIZE - 8) < size ? (SLICE_SIZE - 8) : size;

		/* Look for a better place to split the frame into packets. */
		if (len < size) {
			const uint8_t *end = fs_h263_find_resync_marker_reverse(buf,
																	buf + len);
			len = end - buf;
			if (len == SLICE_SIZE - 8) {
				/* Skip mb info prior to the start of the current ptr */
				while (mb_info_pos < mb_info_count) {
					uint32_t pos = *(uint32_t *)&mb_info[12 * mb_info_pos] / 8;
					if (pos >= buf - buf_base) break;
					mb_info_pos++;
				}
				/* Find the first mb info past the end pointer */
				while (mb_info_pos + 1 < mb_info_count) {
					uint32_t pos = *(uint32_t *)&mb_info[12 * (mb_info_pos + 1)] / 8;
					if (pos >= end - buf_base) break;
					mb_info_pos++;
				}

				if (mb_info_pos < mb_info_count) {
					const uint8_t *ptr = &mb_info[12 * mb_info_pos];
					uint32_t bit_pos = *(uint32_t *)ptr;
					uint32_t pos = (bit_pos + 7) / 8;

					if (pos <= end - buf_base) {
						state.quant = ptr[4];
						state.gobn  = ptr[5];
						state.mba   = *(uint16_t *)&ptr[6];
						state.hmv1  = (int8_t) ptr[8];
						state.vmv1  = (int8_t) ptr[9];
						state.hmv2  = (int8_t) ptr[10];
						state.vmv2  = (int8_t) ptr[11];
						ebits = 8 * pos - bit_pos;
						len   = pos - (buf - buf_base);
						mb_info_pos++;
					} else {
						switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING,
						    "Unable to split H263 packet! mb_info_pos=%d mb_info_count=%d pos=%d max=%"SWITCH_SIZE_T_FMT"\n", mb_info_pos, mb_info_count, pos, (switch_size_t)(end - buf_base));
					}
				} else {
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Should Not Happen!!! mb_info_pos=%d mb_info_count=%d mb_info_size=%d\n", mb_info_pos, mb_info_count, mb_info_size);
				}
			}
		}

		if (size > 2 && !buf[0] && !buf[1]) {
			our_h264_nalu_t *nalu = &context->nalus[context->nalu_current_index];

			h.f = 0; // Mode A
			h.sbit = sbits;
			h.ebit = ebits;
			nalu->start = buf;
			nalu->len = len;
			nalu->h263_header = h;
			context->nalu_current_index++;
		} else {
			our_h264_nalu_t *nalu = &context->nalus[context->nalu_current_index];

			h.f = 1; // Mode B
			h.ebit = ebits;
			h.sbit = sbits;
			nalu->start = buf;
			nalu->len = len;
			nalu->h263_header = h;
			nalu->h263_state = packet_start_state;
			context->nalu_current_index++;
		}

		if (ebits) {
			sbits = 8 - ebits;
			len--;
		} else {
			sbits = 0;
		}
		buf  += len;
		size -= len;
		ebits = 0;
	}
}
#endif

static void fs_rtp_parse_h263_rfc4629(h264_codec_context_t *context, AVPacket *pkt)
{
	int len;
	uint8_t *buf = pkt->data;
	our_h264_nalu_t *nalu;
	int size = pkt->size;

	while (size > 0) {
		nalu = &context->nalus[context->nalu_current_index];
		len = (SLICE_SIZE - 2) > size ? size : (SLICE_SIZE - 2);

		/* Look for a better place to split the frame into packets. */
		if (len < size) {
			const uint8_t *end = fs_h263_find_resync_marker_reverse(buf, buf + len);
			len = end - buf;
		}

		nalu->start = buf;
		nalu->len = len;

		context->nalu_current_index++;

		buf += len;
		size -= len;
	}
}

static switch_status_t consume_h263_bitstream(h264_codec_context_t *context, switch_frame_t *frame)
{
	our_h264_nalu_t *nalu = &context->nalus[context->nalu_current_index];

	if (!nalu->h263_header.f) { // Mode A
		h263_payload_header_t *h = frame->data;
		*h = nalu->h263_header;
		memcpy(((uint8_t *)frame->data) + sizeof(*h), nalu->start, nalu->len);
		frame->datalen = nalu->len + sizeof(*h);
		context->nalu_current_index++;

#ifdef H263_MODE_B
	} else { //Mode B

/*
    0                   1                   2                   3
    0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |F|P|SBIT |EBIT | SRC | QUANT   |  GOBN   |   MBA           |R  |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |I|U|S|A| HMV1        | VMV1        | HMV2        | VMV2        |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
*/
		h263_payload_header_t *h = frame->data;
		uint8_t *p = frame->data;

#if 0
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "header: f:%d p:%d src:%d sbit:%d ebit:%d\n",
			nalu->h263_header.f,
			nalu->h263_header.p,
			nalu->h263_header.src,
			nalu->h263_header.sbit,
			nalu->h263_header.ebit);

		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "state: gobn:%x mba:%x quant:%x hmv1:%x vmv1:%x hmv2:%x vmv2:%x\n",
			nalu->h263_state.gobn,
			nalu->h263_state.mba,
			nalu->h263_state.quant,
			nalu->h263_state.hmv1,
			nalu->h263_state.vmv1,
			nalu->h263_state.hmv2,
			nalu->h263_state.vmv2);
#endif
		*h = nalu->h263_header;
		p++;
		*p &= 0xE0;
		*p |= (nalu->h263_state.quant & 0x1F);
		p++;
		*p  = ((nalu->h263_state.gobn << 3) & 0xF8);
		*p |= ((nalu->h263_state.mba >> 6) & 0x07);
		p++;
		*p  = ((nalu->h263_state.mba & 0x1F) << 2);
		p++;
		*p  = (nalu->h263_header.i << 7);
		*p |= (nalu->h263_header.u << 6);
		*p |= (nalu->h263_header.s << 5);
		*p |= (nalu->h263_header.a << 4);
		*p |= ((nalu->h263_state.hmv1 >> 3) & 0x0F);
		p++;
		*p  = ((nalu->h263_state.hmv1 & 0x07) << 5);
		*p |= ((nalu->h263_state.vmv1 >> 2) & 0x1F);
		p++;
		*p  = ((nalu->h263_state.vmv1 & 0x03) << 6);
		*p |= ((nalu->h263_state.hmv2 >> 1) & 0x3F);
		p++;
		*p  = ((nalu->h263_state.hmv2 & 0x01) << 7);
		*p |= (nalu->h263_state.vmv2);
		p++;

		memcpy(p, nalu->start, nalu->len);
		frame->datalen = nalu->len + 8;
		context->nalu_current_index++;
#endif
	}

	if (!context->nalus[context->nalu_current_index].len) {
		av_packet_unref(&context->encoder_avpacket);
		frame->m = 1;
	}

#if 0
	{
		uint8_t *p = frame->data;
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "len: %d, mark:%d %02x %02x %02x %02x\n", frame->datalen, frame->m, *p, *(p+1), *(p+2), *(p+3));
		if (frame->m && (nalu->h263_header.i == 0)) {
			// switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "Key frame generated!!\n");
		}
	}
#endif

	return frame->m ? SWITCH_STATUS_SUCCESS : SWITCH_STATUS_MORE_DATA;
}


static switch_status_t consume_h263p_bitstream(h264_codec_context_t *context, switch_frame_t *frame)
{
	our_h264_nalu_t *nalu = &context->nalus[context->nalu_current_index];
	uint8_t *data = frame->data;
	const uint8_t *p = nalu->start;
	int len = nalu->len;

	if (*p == 0 && *(p+1) == 0) {
		*data++ = 0x04;
		p += 2;
		len -= 2;
	} else {
		*data++ = 0;
	}

	*data++ = 0;
	memcpy(data, p, len);
	frame->datalen = len + 2;
	context->nalu_current_index++;

	if (!context->nalus[context->nalu_current_index].len) frame->m = 1;

#if 0
	{
		uint8_t *p = frame->data;
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "len: %d, mark:%d %02x %02x %02x %02x\n", frame->datalen, frame->m, *p, *(p+1), *(p+2), *(p+3));
	}
#endif

	if (frame->m) {
		av_packet_unref(&context->encoder_avpacket);
		return SWITCH_STATUS_SUCCESS;
	}

	return SWITCH_STATUS_MORE_DATA;
}

static switch_status_t consume_h264_bitstream(h264_codec_context_t *context, switch_frame_t *frame)
{
	AVPacket *pkt = &context->encoder_avpacket;
	our_h264_nalu_t *nalu = &context->nalus[context->nalu_current_index];
	uint8_t nalu_hdr = *(uint8_t *)(nalu->start);
	uint8_t nalu_type = nalu_hdr & 0x1f;
	uint8_t nri = nalu_hdr & 0x60;
	int left = nalu->len - (nalu->eat - nalu->start);
	uint8_t *p = frame->data;
	uint8_t start = nalu->start == nalu->eat ? 0x80 : 0;
	int n = nalu->len / SLICE_SIZE;
	int slice_size = nalu->len / (n + 1) + 1 + 2;

	if (nalu->len <= SLICE_SIZE) {
		memcpy(frame->data, nalu->start, nalu->len);
		frame->datalen = nalu->len;
		context->nalu_current_index++;

		if (context->nalus[context->nalu_current_index].len) {
			frame->m = 0;
			return SWITCH_STATUS_MORE_DATA;
		}

		if (pkt->size > 0) av_packet_unref(pkt);

		switch_clear_flag(frame, SFF_CNG);
		frame->m = 1;

		return SWITCH_STATUS_SUCCESS;
	}

	if (left <= (slice_size - 2)) {
		p[0] = nri | 28; // FU-A
		p[1] = 0x40 | nalu_type;
		memcpy(p+2, nalu->eat, left);
		nalu->eat += left;
		frame->datalen = left + 2;
		context->nalu_current_index++;

		if (!context->nalus[context->nalu_current_index].len) {
			if (pkt->size > 0) av_packet_unref(pkt);
			frame->m = 1;
			return SWITCH_STATUS_SUCCESS;
		}

		return SWITCH_STATUS_MORE_DATA;
	}

	p[0] = nri | 28; // FU-A
	p[1] = start | nalu_type;
	if (start) nalu->eat++;
	memcpy(p+2, nalu->eat, slice_size - 2);
	nalu->eat += (slice_size - 2);
	frame->datalen = slice_size;
	frame->m = 0;
	return SWITCH_STATUS_MORE_DATA;
}

static switch_status_t consume_nalu(h264_codec_context_t *context, switch_frame_t *frame)
{
	AVPacket *pkt = &context->encoder_avpacket;
	our_h264_nalu_t *nalu = &context->nalus[context->nalu_current_index];

	if (!nalu->len) {
		frame->datalen = 0;
		frame->m = 0;
		if (pkt->size > 0) av_packet_unref(pkt);
		context->nalu_current_index = 0;
		return SWITCH_STATUS_NOTFOUND;
	}

	if (context->av_codec_id == AV_CODEC_ID_H263) {
		return consume_h263_bitstream(context, frame);
	}

	if (context->av_codec_id == AV_CODEC_ID_H263P) {
		return consume_h263p_bitstream(context, frame);
	}

	return consume_h264_bitstream(context, frame);
}

static void set_h264_private_data(h264_codec_context_t *context, avcodec_profile_t *profile)
{
	if (context->hw_encoder) {
		av_opt_set(context->encoder_ctx->priv_data, "preset", "llhp", 0);
		av_opt_set_int(context->encoder_ctx->priv_data, "2pass", 1, 0);
		av_opt_set_int(context->encoder_ctx->priv_data, "delay", 0, 0);
		av_opt_set(context->encoder_ctx->priv_data, "forced-idr", "true", 0);
		return;
	}

	av_opt_set(context->encoder_ctx->priv_data, "preset", "veryfast", 0);
	av_opt_set(context->encoder_ctx->priv_data, "intra-refresh", "1", 0);
	av_opt_set(context->encoder_ctx->priv_data, "tune", "animation+zerolatency", 0);
	//av_opt_set(context->encoder_ctx->priv_data, "sc_threshold", "40", 0);
	//av_opt_set(context->encoder_ctx->priv_data, "crf", "18", 0);

	if (profile->options) {
		switch_event_header_t *hp;

		for (hp = profile->options->headers; hp; hp = hp->next) {
			// switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "%s: %s\n", hp->name, hp->value);
			av_opt_set(context->encoder_ctx->priv_data, hp->name, hp->value, 0);
		}
	}

	context->encoder_ctx->colorspace = profile->ctx.colorspace;
	context->encoder_ctx->color_range = profile->ctx.color_range;

	context->encoder_ctx->flags |= profile->ctx.flags; // CODEC_FLAG_LOOP_FILTER;   // flags=+loop
	if (profile->ctx.me_cmp >= 0) context->encoder_ctx->me_cmp = profile->ctx.me_cmp;  // cmp=+chroma, where CHROMA = 1
	if (profile->ctx.me_range >= 0) context->encoder_ctx->me_range = profile->ctx.me_range;
	if (profile->ctx.max_b_frames >= 0) context->encoder_ctx->max_b_frames = profile->ctx.max_b_frames;
	if (profile->ctx.refs >= 0) context->encoder_ctx->refs = profile->ctx.refs;
	if (profile->ctx.gop_size >= 0) context->encoder_ctx->gop_size = profile->ctx.gop_size;
	if (profile->ctx.keyint_min >= 0) context->encoder_ctx->keyint_min = profile->ctx.keyint_min;
	if (profile->ctx.i_quant_factor >= 0) context->encoder_ctx->i_quant_factor = profile->ctx.i_quant_factor;
	if (profile->ctx.b_quant_factor >= 0) context->encoder_ctx->b_quant_factor = profile->ctx.b_quant_factor;
	if (profile->ctx.qcompress >= 0) context->encoder_ctx->qcompress = profile->ctx.qcompress;
	if (profile->ctx.qmin >= 0) context->encoder_ctx->qmin = profile->ctx.qmin;
	if (profile->ctx.qmax >= 0) context->encoder_ctx->qmax = profile->ctx.qmax;
	if (profile->ctx.max_qdiff >= 0) context->encoder_ctx->max_qdiff = profile->ctx.max_qdiff;
}

static switch_status_t open_encoder(h264_codec_context_t *context, uint32_t width, uint32_t height)
{
	avcodec_profile_t *aprofile = NULL;
	char codec_string[1024];

	if (!context->encoder) {
		if (context->av_codec_id == AV_CODEC_ID_H264) {
			if (context->codec_settings.video.try_hardware_encoder && (context->encoder = avcodec_find_encoder_by_name("nvenc_h264"))) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "NVENC HW CODEC ENABLED\n");
				context->hw_encoder = 1;
			} else {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "NVENC HW CODEC NOT PRESENT\n");
			}
		}
	}

	if (!context->encoder) {
		context->encoder = avcodec_find_encoder(context->av_codec_id);
	}

	if (!context->encoder) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Cannot find encoder id: %d\n", context->av_codec_id);
		return SWITCH_STATUS_FALSE;
	}

	if (context->av_codec_id == AV_CODEC_ID_H263 && (!is_valid_h263_dimension(width, height))) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "You want %dx%d, but valid sizes are 128x96, 176x144, 352x288, 704x576, and 1408x1152. Try H.263+\n", width, height);
		return SWITCH_STATUS_FALSE;
	}

	if (!zstr(context->codec_settings.video.config_profile_name)) {
		aprofile = find_profile(context->codec_settings.video.config_profile_name, SWITCH_FALSE);
	}

	if (!aprofile) {
		aprofile = find_profile(get_profile_name(context->av_codec_id), SWITCH_FALSE);
	}

	if (!aprofile) return SWITCH_STATUS_FALSE;

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Using config profile: [%s]\n", aprofile->name);

	if (aprofile->codecs) {
		parse_codec_specific_profile(aprofile, get_profile_name(context->av_codec_id));
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
		double fps = context->codec_settings.video.fps > 0 ? context->codec_settings.video.fps : 15.0;

		context->bandwidth = switch_calc_bitrate(context->codec_settings.video.width, context->codec_settings.video.height, 1, fps);
	}

	if (context->bandwidth > avcodec_globals.max_bitrate) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "BITRATE TRUNCATED TO %d\n", avcodec_globals.max_bitrate);
		context->bandwidth = avcodec_globals.max_bitrate;
	}

	context->bandwidth *= 3;

	context->encoder_ctx->bit_rate = context->bandwidth * 1024;
	context->encoder_ctx->rc_min_rate = context->encoder_ctx->bit_rate;
	context->encoder_ctx->rc_max_rate = context->encoder_ctx->bit_rate;
	context->encoder_ctx->rc_buffer_size = context->encoder_ctx->bit_rate;
	context->encoder_ctx->qcompress = 0.6;
	context->encoder_ctx->gop_size = 1000;
	context->encoder_ctx->keyint_min = 1000;
	
	context->encoder_ctx->width = context->codec_settings.video.width;
	context->encoder_ctx->height = context->codec_settings.video.height;
	context->encoder_ctx->time_base = (AVRational){1, 90};
	context->encoder_ctx->max_b_frames = aprofile->ctx.max_b_frames;
	context->encoder_ctx->pix_fmt = AV_PIX_FMT_YUV420P;
	context->encoder_ctx->thread_count = aprofile->ctx.thread_count;

	if (context->av_codec_id == AV_CODEC_ID_H263 || context->av_codec_id == AV_CODEC_ID_H263P) {
#ifndef H263_MODE_B
#    if defined(__ICL) || defined (__INTEL_COMPILER)
#        define FF_DISABLE_DEPRECATION_WARNINGS __pragma(warning(push)) __pragma(warning(disable:1478))
#        define FF_ENABLE_DEPRECATION_WARNINGS  __pragma(warning(pop))
#    elif defined(_MSC_VER)
#        define FF_DISABLE_DEPRECATION_WARNINGS __pragma(warning(push)) __pragma(warning(disable:4996))
#        define FF_ENABLE_DEPRECATION_WARNINGS  __pragma(warning(pop))
#    else
#        define FF_DISABLE_DEPRECATION_WARNINGS _Pragma("GCC diagnostic ignored \"-Wdeprecated-declarations\"")
#        define FF_ENABLE_DEPRECATION_WARNINGS  _Pragma("GCC diagnostic warning \"-Wdeprecated-declarations\"")
#    endif
FF_DISABLE_DEPRECATION_WARNINGS
		context->encoder_ctx->rtp_callback = rtp_callback;
FF_ENABLE_DEPRECATION_WARNINGS
#endif
		context->encoder_ctx->opaque = context;
		av_opt_set_int(context->encoder_ctx->priv_data, "mb_info", SLICE_SIZE - 8, 0);
	} else if (context->av_codec_id == AV_CODEC_ID_H264) {
		context->encoder_ctx->profile = aprofile->ctx.profile;
		context->encoder_ctx->level = aprofile->ctx.level;

		set_h264_private_data(context, aprofile);
	}

GCC_DIAG_OFF(deprecated-declarations)
	avcodec_string(codec_string, sizeof(codec_string), context->encoder_ctx, 0);
GCC_DIAG_ON(deprecated-declarations)

	dump_encoder_ctx(context->encoder_ctx);

	if (avcodec_open2(context->encoder_ctx, context->encoder, NULL) < 0) {
		if (!context->hw_encoder) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Could not open codec %s\n", codec_string);
			return SWITCH_STATUS_FALSE;
		}

		if (context->encoder_ctx) {
			if (avcodec_is_open(context->encoder_ctx)) {
				avcodec_close(context->encoder_ctx);
			}
			av_free(context->encoder_ctx);
			context->encoder_ctx = NULL;
		}

		context->encoder = NULL;
		context->hw_encoder = 0;
		context->codec_settings.video.try_hardware_encoder = 0;
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Could not open hardware codec %s, trying software encoder\n", codec_string);
		return open_encoder(context, width, height);
	}

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "codec opened: %s\n", codec_string);

	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t switch_h264_init(switch_codec_t *codec, switch_codec_flag_t flags, const switch_codec_settings_t *codec_settings)
{
	int encoding, decoding;
	h264_codec_context_t *context = NULL;
	avcodec_profile_t *profile = NULL;

	encoding = (flags & SWITCH_CODEC_FLAG_ENCODE);
	decoding = (flags & SWITCH_CODEC_FLAG_DECODE);

	if (!(encoding || decoding)) {
		return SWITCH_STATUS_FALSE;
	}

	if (codec->fmtp_in) {
		codec->fmtp_out = switch_core_strdup(codec->memory_pool, codec->fmtp_in);
	}

	context = switch_core_alloc(codec->memory_pool, sizeof(h264_codec_context_t));
	switch_assert(context);
	memset(context, 0, sizeof(*context));

	if (codec_settings) {
		context->codec_settings = *codec_settings;
	}

	if (!strcmp(codec->implementation->iananame, "H263")) {
		context->av_codec_id = AV_CODEC_ID_H263;
	} else if (!strcmp(codec->implementation->iananame, "H263-1998")) {
		context->av_codec_id = AV_CODEC_ID_H263P;
	} else {
		context->av_codec_id = AV_CODEC_ID_H264;
	}

	profile = find_profile(get_profile_name(context->av_codec_id), SWITCH_FALSE);

	if (!profile) {
		goto error;
	}

	if (decoding) {
		context->decoder = avcodec_find_decoder(context->av_codec_id);

		if (!context->decoder && context->av_codec_id == AV_CODEC_ID_H263P) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Cannot find AV_CODEC_ID_H263P decoder, trying AV_CODEC_ID_H263 instead\n");
			context->decoder = avcodec_find_decoder(AV_CODEC_ID_H263);
		}

		if (!context->decoder) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Cannot find codec id %d\n", context->av_codec_id);
			goto error;
		}

		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "codec: id=%d %s\n", context->decoder->id, context->decoder->long_name);
		
		context->decoder_ctx = avcodec_alloc_context3(context->decoder);
		context->decoder_ctx->thread_count = profile->decoder_thread_count;
		if (avcodec_open2(context->decoder_ctx, context->decoder, NULL) < 0) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Error openning codec\n");
			goto error;
		}
	}

	switch_buffer_create_dynamic(&(context->nalu_buffer), H264_NALU_BUFFER_SIZE, H264_NALU_BUFFER_SIZE * 8, 0);
	codec->private_info = context;

	return SWITCH_STATUS_SUCCESS;

error:
	// todo, do some clean up
	return SWITCH_STATUS_FALSE;
}

static void av_unused fill_avframe(AVFrame *pict, switch_image_t *img)
{
	switch_I420_copy2(img->planes, img->stride,
					  pict->data, pict->linesize,
					  img->d_w, img->d_h);
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

	width = img->d_w;
	height = img->d_h;

	if (context->av_codec_id == AV_CODEC_ID_H263 && (!is_valid_h263_dimension(width, height))) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING,
						  "You want %dx%d, but valid H263 sizes are 128x96, 176x144, 352x288, 704x576, and 1408x1152. Try H.263+\n", width, height);
		goto error;
	}

	if (frame->flags & SFF_SAME_IMAGE) {
		// read from nalu buffer
		return consume_nalu(context, frame);
	}

	if (!avctx || !avcodec_is_open(avctx)) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "initializing encoder %dx%d\n", width, height);
		if (open_encoder(context, width, height) != SWITCH_STATUS_SUCCESS) {
			goto error;
		}
		avctx = context->encoder_ctx;
	}

	if (avctx->width != width || avctx->height != height) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "picture size changed from %dx%d to %dx%d, reinitializing encoder\n",
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

#if 0
	if (*got_output) { // TODO: Could be more delayed frames, flush when frame == NULL
		ret = avcodec_encode_video2(avctx, pkt, NULL, got_output);
		if (ret < 0) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Encoding Error %d\n", ret);
			goto error;
		}

		if (*got_output) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG5, "Encoded frame %" SWITCH_INT64_T_FMT " (size=%5d) nalu_type=0x%x %d\n", context->pts, pkt->size, *((uint8_t *)pkt->data +4), *got_output);
			goto process;
		}
	}
#endif

	fill_avframe(avframe, img);

	avframe->pts = context->pts++;

	if (context->need_key_frame) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG5, "Send AV KEYFRAME\n");
		 avframe->pict_type = AV_PICTURE_TYPE_I;
		 avframe->key_frame = 1;
	}

	/* encode the image */
	memset(context->nalus, 0, sizeof(context->nalus));
	context->nalu_current_index = 0;
GCC_DIAG_OFF(deprecated-declarations)
	ret = avcodec_encode_video2(avctx, pkt, avframe, got_output);
GCC_DIAG_ON(deprecated-declarations)

	if (ret < 0) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Encoding Error %d\n", ret);
		goto error;
	}

	if (context->need_key_frame) {
		avframe->pict_type = 0;
		avframe->key_frame = 0;
		context->need_key_frame = 0;
	}

// process:

	if (*got_output) {
		const uint8_t *p = pkt->data;
		int i = 0;

		*got_output = 0;

		if (context->av_codec_id == AV_CODEC_ID_H263) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG5,
							  "Encoded frame %" SWITCH_INT64_T_FMT " (size=%5d) [0x%02x 0x%02x 0x%02x 0x%02x] got_output: %d slices: %d\n",
							  context->pts, pkt->size, *((uint8_t *)pkt->data), *((uint8_t *)(pkt->data + 1)), *((uint8_t *)(pkt->data + 2)),
							  *((uint8_t *)(pkt->data + 3)), *got_output, avctx->slices);

#ifdef H263_MODE_B
			fs_rtp_parse_h263_rfc2190(context, pkt);
#endif

			context->nalu_current_index = 0;
			return consume_nalu(context, frame);
		} else if (context->av_codec_id == AV_CODEC_ID_H263P){
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG5,
							  "Encoded frame %" SWITCH_INT64_T_FMT " (size=%5d) [0x%02x 0x%02x 0x%02x 0x%02x] got_output: %d slices: %d\n",
							  context->pts, pkt->size, *((uint8_t *)pkt->data), *((uint8_t *)(pkt->data + 1)), *((uint8_t *)(pkt->data + 2)),
							  *((uint8_t *)(pkt->data + 3)), *got_output, avctx->slices);
			fs_rtp_parse_h263_rfc4629(context, pkt);
			context->nalu_current_index = 0;
			return consume_nalu(context, frame);
		} else {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG5,
							  "Encoded frame %" SWITCH_INT64_T_FMT " (size=%5d) nalu_type=0x%x %d\n",
							  context->pts, pkt->size, *((uint8_t *)pkt->data +4), *got_output);
		}
		/* split into nalus */
		memset(context->nalus, 0, sizeof(context->nalus));

		while ((p = fs_avc_find_startcode(p, pkt->data+pkt->size)) < (pkt->data + pkt->size)) {
			if (!context->nalus[i].start) {
				while (!(*p++)) ; /* eat the sync bytes, what ever 0 0 1 or 0 0 0 1 */
				context->nalus[i].start = p;
				context->nalus[i].eat = p;

				if (mod_av_globals.debug && (*p & 0x1f) == 7) {
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "KEY FRAME GENERATED\n");
				}
			} else {
				context->nalus[i].len = p - context->nalus[i].start;
				while (!(*p++)) ; /* eat the sync bytes, what ever 0 0 1 or 0 0 0 1 */
				i++;
				context->nalus[i].start = p;
				context->nalus[i].eat = p;
			}
			if (i >= MAX_NALUS - 2) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "TOO MANY SLICES!\n");
				break;
			}
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

	if (context->av_codec_id == AV_CODEC_ID_H263) {
		status = buffer_h263_packets(context, frame);
	} else if (context->av_codec_id == AV_CODEC_ID_H263P) {
		status = buffer_h263_rfc4629_packets(context, frame);
	} else {
		status = buffer_h264_nalu(context, frame);
	}

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

		if (size > 0) {
			av_init_packet(&pkt);
			switch_buffer_write(context->nalu_buffer, ff_input_buffer_padding, sizeof(ff_input_buffer_padding));
			switch_buffer_peek_zerocopy(context->nalu_buffer, (const void **)&pkt.data);
			pkt.size = size;

			if (!context->decoder_avframe) context->decoder_avframe = av_frame_alloc();
			picture = context->decoder_avframe;
			switch_assert(picture);
GCC_DIAG_OFF(deprecated-declarations)
			decoded_len = avcodec_decode_video2(avctx, picture, &got_picture, &pkt);
GCC_DIAG_ON(deprecated-declarations)

			// switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "buffer: %d got pic: %d len: %d [%dx%d]\n", size, got_picture, decoded_len, picture->width, picture->height);

			if (got_picture && decoded_len > 0) {
				int width = picture->width;
				int height = picture->height;

				if (!context->img || (context->img->d_w != width || context->img->d_h != height)) {
					switch_img_free(&context->img);
					context->img = switch_img_alloc(NULL, SWITCH_IMG_FMT_I420, width, height, 1);
					switch_assert(context->img);
				}
#if 0
				context->img->w = picture->linesize[0];
				context->img->h = picture->linesize[1];
				context->img->d_w = width;
				context->img->d_h = height;
#endif
				switch_I420_copy2(picture->data, picture->linesize,
								 context->img->planes, context->img->stride,
								 width, height);

				frame->img = context->img;
			}

			av_frame_unref(picture);
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
										   switch_codec_control_type_t atype,
										   void *cmd_arg,
										   switch_codec_control_type_t *rtype,
										   void **ret_data) {



	h264_codec_context_t *context = (h264_codec_context_t *)codec->private_info;

	switch(cmd) {
	case SCC_VIDEO_GEN_KEYFRAME:
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

	if (context->decoder_avframe) {
		av_frame_free(&context->decoder_avframe);
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

static const AVCodec *next_codec_for_id(enum AVCodecID id, const AVCodec *prev, void **index,
										int encoder)
{
#if (LIBAVCODEC_VERSION_INT < AV_VERSION_INT(58,10,100))
	while ((prev = av_codec_next(prev))) {
#else
	while ((prev = av_codec_iterate(index))) {
#endif
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
	while ((desc = avcodec_descriptor_next(desc))) {
		codecs[i++] = desc;
	}
	switch_assert(i == nb_codecs);
	qsort(codecs, nb_codecs, sizeof(*codecs), compare_codec_desc);
	*rcodecs = codecs;
	return nb_codecs;
}

static void print_codecs_for_id(switch_stream_handle_t *stream, enum AVCodecID id, int encoder)
{
	const AVCodec *codec = NULL;
	void *index = 0;

	stream->write_function(stream, " (%s: ", encoder ? "encoders" : "decoders");

	while ((codec = next_codec_for_id(id, codec, &index, encoder))) {
		stream->write_function(stream, "%s ", codec->name);
	}

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
		void *index = 0;

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
		while ((codec = next_codec_for_id(desc->id, codec, &index, 0))) {
			if (strcmp(codec->name, desc->name)) {
				print_codecs_for_id(stream ,desc->id, 0);
				break;
			}
		}
		codec = NULL; index = 0;
		while ((codec = next_codec_for_id(desc->id, codec, &index, 1))) {
			if (strcmp(codec->name, desc->name)) {
				print_codecs_for_id(stream, desc->id, 1);
				break;
			}
		}

		stream->write_function(stream, "\n");

	}

	av_free(codecs);
}

static void parse_profile(avcodec_profile_t *aprofile, switch_xml_t profile)
{
	switch_xml_t options = switch_xml_child(profile, "options");
	switch_xml_t param = NULL;
	const char *profile_name = switch_xml_attr(profile, "name");
	AVCodecContext *ctx = NULL;

	if (zstr(profile_name)) return;

	ctx = &aprofile->ctx;

	for (param = switch_xml_child(profile, "param"); param; param = param->next) {
		const char *name = switch_xml_attr(param, "name");
		const char *value = switch_xml_attr(param, "value");
		int val;

		if (zstr(name) || zstr(value)) continue;

		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "%s: %s = %s\n", profile_name, name, value);

		val = atoi(value);

		ctx->profile = FF_PROFILE_H264_BASELINE;
		ctx->level = 31;

		if (!strcmp(name, "dec-threads")) {
			aprofile->decoder_thread_count = switch_parse_cpu_string(value);
		} else if (!strcmp(name, "enc-threads")) {
			ctx->thread_count = switch_parse_cpu_string(value);
		} else if (!strcmp(name, "profile")) {
			ctx->profile = UINTVAL(val);

			if (ctx->profile == 0 && !strcasecmp(aprofile->name, "H264")) {
				if (!strcasecmp(value, "baseline")) {
					ctx->profile = FF_PROFILE_H264_BASELINE;
				} else if (!strcasecmp(value, "main")) {
					ctx->profile = FF_PROFILE_H264_MAIN;
				} else if (!strcasecmp(value, "high")) {
					ctx->profile = FF_PROFILE_H264_HIGH;
				}
			}
		} else if (!strcmp(name, "level")) {
			ctx->level = UINTVAL(val);
		} else if (!strcmp(name, "timebase")) {
			int num = 0;
			int den = 0;
			char *slash = strchr(value, '/');

			num = UINTVAL(val);

			if (slash) {
				slash++;
				den = atoi(slash);

				if (den < 0) den = 0;
			}

			if (num && den) {
				ctx->time_base.num = num;
				ctx->time_base.den = den;
			}
		} else if (!strcmp(name, "flags")) {
			char *s = strdup(value);
			int flags = 0;

			if (s) {
				int argc;
				char *argv[20];
				int i;

				argc = switch_separate_string(s, '|', argv, (sizeof(argv) / sizeof(argv[0])));

				for (i = 0; i < argc; i++) {
					if (!strcasecmp(argv[i], "UNALIGNED")) {
#ifdef AV_CODEC_FLAG_UNALIGNED
						flags |= AV_CODEC_FLAG_UNALIGNED;
#endif
					} else if (!strcasecmp(argv[i], "QSCALE")) {
#ifdef AV_CODEC_FLAG_QSCALE
						flags |= AV_CODEC_FLAG_QSCALE;
#endif
					} else if (!strcasecmp(argv[i], "4MV")) {
#ifdef AV_CODEC_FLAG_4MV
						flags |= AV_CODEC_FLAG_4MV;
#endif
					} else if (!strcasecmp(argv[i], "CORRUPT")) {
#ifdef AV_CODEC_FLAG_OUTPUT_CORRUPT
						flags |= AV_CODEC_FLAG_OUTPUT_CORRUPT;
#endif
					} else if (!strcasecmp(argv[i], "QPEL")) {
#ifdef AV_CODEC_FLAG_QPEL
						flags |= AV_CODEC_FLAG_QPEL;
#endif
					} else if (!strcasecmp(argv[i], "PASS1")) {
#ifdef AV_CODEC_FLAG_PASS1
						flags |= AV_CODEC_FLAG_PASS1;
#endif
					} else if (!strcasecmp(argv[i], "PASS2")) {
#ifdef AV_CODEC_FLAG_PASS2
						flags |= AV_CODEC_FLAG_PASS2;
#endif
					} else if (!strcasecmp(argv[i], "LOOP_FILTER")) {
#ifdef AV_CODEC_FLAG_LOOP_FILTER
						flags |= AV_CODEC_FLAG_LOOP_FILTER;
#endif
					} else if (!strcasecmp(argv[i], "GRAY")) {
#ifdef AV_CODEC_FLAG_GRAY
						flags |= AV_CODEC_FLAG_GRAY;
#endif
					} else if (!strcasecmp(argv[i], "PSNR")) {
#ifdef AV_CODEC_FLAG_PSNR
						flags |= AV_CODEC_FLAG_PSNR;
#endif
					} else if (!strcasecmp(argv[i], "TRUNCATED")) {
#ifdef AV_CODEC_FLAG_TRUNCATED
						flags |= AV_CODEC_FLAG_TRUNCATED;
#endif
					} else if (!strcasecmp(argv[i], "INTERLACED_DCT")) {
#ifdef AV_CODEC_FLAG_INTERLACED_DCT
						flags |= AV_CODEC_FLAG_INTERLACED_DCT;
#endif
					} else if (!strcasecmp(argv[i], "LOW_DELAY")) {
#ifdef AV_CODEC_FLAG_LOW_DELAY
						flags |= AV_CODEC_FLAG_LOW_DELAY;
#endif
					} else if (!strcasecmp(argv[i], "HEADER")) {
#ifdef AV_CODEC_FLAG_GLOBAL_HEADER
						flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
#endif
					} else if (!strcasecmp(argv[i], "BITEXACT")) {
#ifdef AV_CODEC_FLAG_BITEXACT
						flags |= AV_CODEC_FLAG_BITEXACT;
#endif
					} else if (!strcasecmp(argv[i], "AC_PRED")) {
#ifdef AV_CODEC_FLAG_AC_PRED
						flags |= AV_CODEC_FLAG_AC_PRED;
#endif
					} else if (!strcasecmp(argv[i], "INTERLACED_ME")) {
#ifdef AV_CODEC_FLAG_INTERLACED_ME
						flags |= AV_CODEC_FLAG_INTERLACED_ME;
#endif
					} else if (!strcasecmp(argv[i], "CLOSED_GOP")) {
#ifdef AV_CODEC_FLAG_CLOSED_GOP
						flags |= AV_CODEC_FLAG_CLOSED_GOP;
#endif
					}
				}

				free(s);
				ctx->flags = flags;
			}
		} else if (!strcmp(name, "me-cmp")) {
			ctx->me_cmp = UINTVAL(val);
		} else if (!strcmp(name, "me-range")) {
			ctx->me_range = UINTVAL(val);
		} else if (!strcmp(name, "max-b-frames")) {
			ctx->max_b_frames = UINTVAL(val);
		} else if (!strcmp(name, "refs")) {
			ctx->refs = UINTVAL(val);
		} else if (!strcmp(name, "gop-size")) {
			ctx->gop_size = UINTVAL(val);
		} else if (!strcmp(name, "keyint-min")) {
			ctx->keyint_min = UINTVAL(val);
		} else if (!strcmp(name, "i-quant-factor")) {
			ctx->i_quant_factor = UINTVAL(val);
		} else if (!strcmp(name, "b-quant-factor")) {
			ctx->b_quant_factor = UINTVAL(val);
		} else if (!strcmp(name, "qcompress")) {
			ctx->qcompress = UINTVAL(val);
		} else if (!strcmp(name, "qmin")) {
			ctx->qmin = UINTVAL(val);
		} else if (!strcmp(name, "qmax")) {
			ctx->qmax = UINTVAL(val);
		} else if (!strcmp(name, "max-qdiff")) {
			ctx->max_qdiff = UINTVAL(val);
		} else if (!strcmp(name, "colorspace")) {
			ctx->colorspace = UINTVAL(val);

			if (ctx->colorspace > AVCOL_SPC_NB) {
				ctx->colorspace = AVCOL_SPC_RGB;
			}
		} else if (!strcmp(name, "color-range")) {
			ctx->color_range = UINTVAL(val);

			if (ctx->color_range >  AVCOL_RANGE_NB) {
				ctx->color_range = 0;
			}
		}
	} // for param

	if (options) {
		switch_xml_t option = switch_xml_child(options, "option");

		if (aprofile->options) {
			switch_event_destroy(&aprofile->options);
		}

		switch_event_create(&aprofile->options, SWITCH_EVENT_CLONE);
		aprofile->options->flags |= EF_UNIQ_HEADERS;

		for (; option; option = option->next) {
			const char *name = switch_xml_attr(option, "name");
			const char *value = switch_xml_attr(option, "value");

			if (zstr(name) || zstr(value)) continue;

			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "%s: %s\n", name, value);

			switch_event_add_header_string(aprofile->options, SWITCH_STACK_BOTTOM, name, value);
		}
	} // for options
}

static void parse_codecs(avcodec_profile_t *aprofile, switch_xml_t codecs)
{
	switch_xml_t codec = NULL;

	if (!codecs) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "no codecs defined in mod_av profile %s\n", aprofile->name);
		return;
	}

	codec = switch_xml_child(codecs, "codec");

	if (aprofile->codecs) {
		switch_event_destroy(&aprofile->codecs);
	}

	switch_event_create(&aprofile->codecs, SWITCH_EVENT_CLONE);

	for (; codec; codec = codec->next) {
		const char *codec_name = switch_xml_attr(codec, "name");
		const char *profile_name = switch_xml_attr(codec, "profile");

		if (zstr(codec_name) || zstr(profile_name)) continue;

		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "codec: %s, profile: %s\n", codec_name, profile_name);

		switch_event_add_header_string(aprofile->codecs, SWITCH_STACK_BOTTOM, codec_name, profile_name);
	}
}


static void load_config()
{
	switch_xml_t cfg = NULL, xml = NULL;

	avcodec_globals.max_bitrate = 0;
	avcodec_globals.dec_threads = 1;
	avcodec_globals.enc_threads = switch_parse_cpu_string("cpu/2/4");

	xml = switch_xml_open_cfg("avcodec.conf", &cfg, NULL);

	if (xml) {
		switch_xml_t settings = switch_xml_child(cfg, "settings");
		switch_xml_t profiles = switch_xml_child(cfg, "profiles");

		if (settings) {
			switch_xml_t param;

			for (param = switch_xml_child(settings, "param"); param; param = param->next) {
				const char *name = switch_xml_attr(param, "name");
				const char *value = switch_xml_attr(param, "value");

				if (zstr(name) || zstr(value)) continue;

				if (!strcmp(name, "max-bitrate")) {
					avcodec_globals.max_bitrate = switch_parse_bandwidth_string(value);
				} else if (!strcmp(name, "rtp-slice-size")) {
					int val = atoi(value);
					avcodec_globals.rtp_slice_size = UINTVAL(val);
				} else if (!strcmp(name, "key-frame-min-freq")) {
					int val = atoi(value);
					avcodec_globals.key_frame_min_freq = UINTVAL(val);
					avcodec_globals.key_frame_min_freq *= 1000;
				} else if (!strcmp(name, "dec-threads")) {
					avcodec_globals.dec_threads = switch_parse_cpu_string(value);
				} else if (!strcmp(name, "enc-threads")) {
					avcodec_globals.enc_threads = switch_parse_cpu_string(value);
				}
			}
		}

		if (profiles) {
			switch_xml_t profile = switch_xml_child(profiles, "profile");

			for (; profile; profile = profile->next) {
				switch_xml_t codecs = switch_xml_child(profile, "codecs");
				const char *profile_name = switch_xml_attr(profile, "name");
				avcodec_profile_t *aprofile = NULL;

				if (zstr(profile_name)) continue;

				aprofile = find_profile(profile_name, SWITCH_TRUE);

				if (!aprofile) {
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "cannot find profile %s\n", profile_name);
					continue;
				}

				parse_profile(aprofile, profile);
				parse_codecs(aprofile, codecs);
			} // for profile
		} // profiles

		switch_xml_free(xml);
	} // xml

	if (avcodec_globals.max_bitrate <= 0) {
		avcodec_globals.max_bitrate = switch_calc_bitrate(1920, 1080, 5, 60);
	}

	if (avcodec_globals.rtp_slice_size < 500 || avcodec_globals.rtp_slice_size > 1500) {
		avcodec_globals.rtp_slice_size = SWITCH_DEFAULT_VIDEO_SIZE;
	}

	SLICE_SIZE = avcodec_globals.rtp_slice_size;

	if (avcodec_globals.key_frame_min_freq < 10000 || avcodec_globals.key_frame_min_freq > 3 * 1000000) {
		avcodec_globals.key_frame_min_freq = KEY_FRAME_MIN_FREQ;
	}

	// make sure profiles created
	find_profile("H263", SWITCH_FALSE);
	find_profile("H263+", SWITCH_FALSE);
	find_profile("H264", SWITCH_FALSE);
	find_profile("H265", SWITCH_FALSE);
}

SWITCH_MODULE_LOAD_FUNCTION(mod_avcodec_load)
{
	switch_codec_interface_t *codec_interface;

	memset(&avcodec_globals, 0, sizeof(struct avcodec_globals));
	load_config();

	SWITCH_ADD_CODEC(codec_interface, "H264 Video");
	switch_core_codec_add_video_implementation(pool, codec_interface, 99, "H264", NULL,
											   switch_h264_init, switch_h264_encode, switch_h264_decode, switch_h264_control, switch_h264_destroy);

	SWITCH_ADD_CODEC(codec_interface, "H263 Video");
	switch_core_codec_add_video_implementation(pool, codec_interface, 34, "H263", NULL,
											   switch_h264_init, switch_h264_encode, switch_h264_decode, switch_h264_control, switch_h264_destroy);

	SWITCH_ADD_CODEC(codec_interface, "H263+ Video");
	switch_core_codec_add_video_implementation(pool, codec_interface, 115, "H263-1998", NULL,
											   switch_h264_init, switch_h264_encode, switch_h264_decode, switch_h264_control, switch_h264_destroy);

	/* indicate that the module should continue to be loaded */
	return SWITCH_STATUS_SUCCESS;
}

SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_avcodec_shutdown)
{
	int i;

	for (i = 0; i < MAX_PROFILES; i++) {
		avcodec_profile_t *profile = avcodec_globals.profiles[i];

		if (!profile) break;

		if (profile->options) {
			switch_event_destroy(&profile->options);
		}

		if (profile->codecs) {
			switch_event_destroy(&profile->codecs);
		}

		free(profile);
	}

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
