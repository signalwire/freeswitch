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
 * Anthony Minessale II <anthm@freeswitch.org>
 * Seven Du <dujinfang@gmail.com>
 * Sam Russell <sam.h.russell@gmail.com>
 *
 * mod_vpx.c -- VP8/9 Video Codec, with transcoding
 *
 */

#include <switch.h>
#ifdef SWITCH_HAVE_YUV
#ifdef SWITCH_HAVE_VPX
#include <vpx/vpx_encoder.h>
#include <vpx/vpx_decoder.h>
#include <vpx/vp8cx.h>
#include <vpx/vp8dx.h>
#include <vpx/vp8.h>

// #define DEBUG_VP9

#define SLICE_SIZE SWITCH_DEFAULT_VIDEO_SIZE
#define KEY_FRAME_MIN_FREQ 250000

/*	http://tools.ietf.org/html/draft-ietf-payload-vp8-10

	The first octets after the RTP header are the VP8 payload descriptor, with the following structure.
#endif

	     0 1 2 3 4 5 6 7
	    +-+-+-+-+-+-+-+-+
	    |X|R|N|S|R| PID | (REQUIRED)
	    +-+-+-+-+-+-+-+-+
	X:  |I|L|T|K| RSV   | (OPTIONAL)
	    +-+-+-+-+-+-+-+-+
	I:  |M| PictureID   | (OPTIONAL)
	    +-+-+-+-+-+-+-+-+
	L:  |   TL0PICIDX   | (OPTIONAL)
	    +-+-+-+-+-+-+-+-+
	T/K:|TID|Y| KEYIDX  | (OPTIONAL)
	    +-+-+-+-+-+-+-+-+


	VP8 Payload Header

	 0 1 2 3 4 5 6 7
	+-+-+-+-+-+-+-+-+
	|Size0|H| VER |P|
	+-+-+-+-+-+-+-+-+
	|     Size1     |
	+-+-+-+-+-+-+-+-+
	|     Size2     |
	+-+-+-+-+-+-+-+-+
	| Bytes 4..N of |
	| VP8 payload   |
	:               :
	+-+-+-+-+-+-+-+-+
	| OPTIONAL RTP  |
	| padding       |
	:               :
	+-+-+-+-+-+-+-+-+
*/


#ifdef _MSC_VER
#pragma pack(push, r1, 1)
#endif

#if SWITCH_BYTE_ORDER == __BIG_ENDIAN

typedef struct {
	unsigned extended:1;
	unsigned reserved1:1;
	unsigned non_referenced:1;
	unsigned start:1;
	unsigned reserved2:1;
	unsigned pid:3;
} vp8_payload_descriptor_t;

typedef struct {
	unsigned have_pid:1;
	unsigned have_p_layer:1;
	unsigned have_layer_ind:1;
	unsigned is_flexible:1;
	unsigned start:1;
	unsigned end:1;
	unsigned have_ss:1;
	unsigned zero:1;
} vp9_payload_descriptor_t;

typedef struct {
	unsigned n_s:3;
	unsigned y:1;
	unsigned g:1;
	unsigned zero:3;
} vp9_ss_t;

typedef struct {
	unsigned t:3;
	unsigned u:1;
	unsigned r:2;
	unsigned zero:2;
} vp9_n_g_t;

typedef struct {
	unsigned temporal_id:3;
	unsigned temporal_up_switch:1;
	unsigned spatial_id:3;
	unsigned inter_layer_predicted:1;
} vp9_p_layer_t;

#else /* ELSE LITTLE */

typedef struct {
	unsigned pid:3;
	unsigned reserved2:1;
	unsigned start:1;
	unsigned non_referenced:1;
	unsigned reserved1:1;
	unsigned extended:1;
} vp8_payload_descriptor_t;

typedef struct {
	unsigned zero:1;
	unsigned have_ss:1;
	unsigned end:1;
	unsigned start:1;
	unsigned is_flexible:1;
	unsigned have_layer_ind:1;
	unsigned have_p_layer:1;
	unsigned have_pid:1;
} vp9_payload_descriptor_t;

typedef struct {
	unsigned zero:3;
	unsigned g:1;
	unsigned y:1;
	unsigned n_s:3;
} vp9_ss_t;

typedef struct {
	unsigned zero:2;
	unsigned r:2;
	unsigned u:1;
	unsigned t:3;
} vp9_n_g_t;

typedef struct {
	unsigned inter_layer_predicted:1;
	unsigned spatial_id:3;
	unsigned temporal_up_switch:1;
	unsigned temporal_id:3;
} vp9_p_layer_t;

#endif

typedef union {
	vp8_payload_descriptor_t vp8;
	vp9_payload_descriptor_t vp9;
} vpx_payload_descriptor_t;

#define kMaxVp9NumberOfSpatialLayers 16

typedef struct {
	switch_bool_t has_received_sli;
	uint8_t picture_id_sli;
	switch_bool_t has_received_rpsi;
	uint64_t picture_id_rpsi;
	int16_t picture_id;  // Negative value to skip pictureId.

	switch_bool_t inter_pic_predicted;  // This layer frame is dependent on previously
	                           // coded frame(s).
	switch_bool_t flexible_mode;
	switch_bool_t ss_data_available;

	int tl0_pic_idx;  // Negative value to skip tl0PicIdx.
	uint8_t temporal_idx;
	uint8_t spatial_idx;
	switch_bool_t temporal_up_switch;
	switch_bool_t inter_layer_predicted;  // Frame is dependent on directly lower spatial
	                             // layer frame.
	uint8_t gof_idx;

	// SS data.
	size_t num_spatial_layers;
	switch_bool_t spatial_layer_resolution_present;
	uint16_t width[kMaxVp9NumberOfSpatialLayers];
	uint16_t height[kMaxVp9NumberOfSpatialLayers];
	// GofInfoVP9 gof;
} vp9_info_t;


#ifdef _MSC_VER
#pragma pack(pop, r1)
#endif


#define __IS_VP8_KEY_FRAME(byte) !(((byte) & 0x01))
static inline int IS_VP8_KEY_FRAME(uint8_t *data) 
{
	uint8_t S;
	uint8_t DES;
	uint8_t PID;

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
	
	if (S && (PID == 0)) {
		return __IS_VP8_KEY_FRAME(*data);
	} else {
		// if (PID > 0) switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "PID: %d\n", PID);
		return 0;
	}
}

#define IS_VP9_KEY_FRAME(byte) ((((byte) & 0x40) == 0) && ((byte) & 0x0A))
#define IS_VP9_START_PKT(byte) ((byte) & 0x08)

SWITCH_MODULE_LOAD_FUNCTION(mod_vpx_load);
SWITCH_MODULE_DEFINITION(CORE_VPX_MODULE, mod_vpx_load, NULL, NULL);

struct vpx_context {
	switch_codec_t *codec;
	int is_vp9;
	vp9_info_t vp9;
	int lossless;
	vpx_codec_iface_t *encoder_interface;
	vpx_codec_iface_t *decoder_interface;
	unsigned int flags;
	switch_codec_settings_t codec_settings;
	unsigned int bandwidth;
	vpx_codec_enc_cfg_t	config;
	switch_time_t last_key_frame;

	vpx_codec_ctx_t	encoder;
	uint8_t encoder_init;
	vpx_image_t *pic;
	switch_bool_t force_key_frame;
	int fps;
	int format;
	int intra_period;
	int num;
	int partition_index;
	const vpx_codec_cx_pkt_t *pkt;
	vpx_codec_iter_t enc_iter;
	vpx_codec_iter_t dec_iter;
	uint32_t last_ts;
	switch_time_t last_ms;
	vpx_codec_ctx_t	decoder;
	uint8_t decoder_init;
	switch_buffer_t *vpx_packet_buffer;
	int got_key_frame;
	int no_key_frame;
	int got_start_frame;
	uint32_t last_received_timestamp;
	switch_bool_t last_received_complete_picture;
	int need_key_frame;
	int need_encoder_reset;
	int need_decoder_reset;
	int32_t change_bandwidth;
	uint64_t framecount;
	switch_memory_pool_t *pool;
	switch_buffer_t *pbuffer;
	switch_time_t start_time;
};
typedef struct vpx_context vpx_context_t;


static switch_status_t init_decoder(switch_codec_t *codec)
{
	vpx_context_t *context = (vpx_context_t *)codec->private_info;
	vpx_codec_dec_cfg_t cfg = {0, 0, 0};
	vpx_codec_flags_t dec_flags = 0;

	if (context->flags & SWITCH_CODEC_FLAG_DECODE && !context->decoder_init) {
		vp8_postproc_cfg_t ppcfg;

		//if (context->decoder_init) {
		//	vpx_codec_destroy(&context->decoder);
		//	context->decoder_init = 0;
		//}

		cfg.threads = 1;//(switch_core_cpu_count() > 1) ? 2 : 1;

		if (!context->is_vp9) { // vp8 only
			// dec_flags = VPX_CODEC_USE_POSTPROC;
		}

		if (vpx_codec_dec_init(&context->decoder, context->decoder_interface, &cfg, dec_flags) != VPX_CODEC_OK) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Codec %s init error: [%d:%s]\n", vpx_codec_iface_name(context->decoder_interface), context->encoder.err, context->encoder.err_detail);
			return SWITCH_STATUS_FALSE;
		}

		
		context->last_ts = 0;
		context->last_received_timestamp = 0;
		context->last_received_complete_picture = 0;
		context->decoder_init = 1;
		context->got_key_frame = 0;
		context->no_key_frame = 0;
		context->got_start_frame = 0;
		// the types of post processing to be done, should be combination of "vp8_postproc_level"
		ppcfg.post_proc_flag = VP8_DEBLOCK;//VP8_DEMACROBLOCK | VP8_DEBLOCK;
		// the strength of deblocking, valid range [0, 16]
		ppcfg.deblocking_level = 1;
		// Set deblocking settings
		vpx_codec_control(&context->decoder, VP8_SET_POSTPROC, &ppcfg);

		if (context->vpx_packet_buffer) {
			switch_buffer_zero(context->vpx_packet_buffer);
		} else {
			switch_buffer_create_dynamic(&context->vpx_packet_buffer, 512, 512, 0);
		}
	}

	return SWITCH_STATUS_SUCCESS;
}


static switch_status_t init_encoder(switch_codec_t *codec)
{
	vpx_context_t *context = (vpx_context_t *)codec->private_info;
	vpx_codec_enc_cfg_t *config = &context->config;
	int token_parts = 1;
	int cpus = switch_core_cpu_count();
	int sane;
	
	if (!context->codec_settings.video.width) {
		context->codec_settings.video.width = 1280;
	}

	if (!context->codec_settings.video.height) {
		context->codec_settings.video.height = 720;
	}

	if (context->codec_settings.video.bandwidth == -1) {
		context->codec_settings.video.bandwidth = 0;
	}
	
	if (context->codec_settings.video.bandwidth) {
		context->bandwidth = context->codec_settings.video.bandwidth;
	} else {
		context->bandwidth = switch_calc_bitrate(context->codec_settings.video.width, context->codec_settings.video.height, 1, 15);

	}

	sane = switch_calc_bitrate(1920, 1080, 2, 30);

	if (context->bandwidth > sane) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(codec->session), SWITCH_LOG_WARNING, "BITRATE TRUNCATED FROM %d TO %d\n", context->bandwidth, sane);
		context->bandwidth = sane;
	}

	context->pkt = NULL;

	switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(codec->session), SWITCH_LOG_NOTICE,
					  "VPX reset encoder picture from %dx%d to %dx%d %u BW\n", 
					  config->g_w, config->g_h, context->codec_settings.video.width, context->codec_settings.video.height, context->bandwidth);

	context->start_time = switch_micro_time_now();
	
	config->g_timebase.num = 1;
	config->g_timebase.den = 1000;
	config->g_pass = VPX_RC_ONE_PASS;
	config->g_w = context->codec_settings.video.width;
	config->g_h = context->codec_settings.video.height;	
	config->rc_target_bitrate = context->bandwidth;
	config->g_lag_in_frames = 0;
	config->kf_max_dist = 360;//2000;
	config->g_threads = cpus - 1;//(cpus > 1) ? 2 : 1;
	
	if (context->is_vp9) {
		//config->rc_dropframe_thresh = 2;
		token_parts = (cpus > 1) ? 3 : 0;

		if (context->lossless) {
			config->rc_min_quantizer = 0;
			config->rc_max_quantizer = 0;
		} else {
			config->rc_min_quantizer = 0;
			config->rc_max_quantizer = 63;
		}

		config->temporal_layering_mode = VP9E_TEMPORAL_LAYERING_MODE_NOLAYERING;
		config->ts_number_layers = 1;
		config->ts_rate_decimator[0] = 1;
		config->ts_periodicity = 1;
		config->ts_layer_id[0] = 0;
	} else {

		// settings
		config->g_profile = 2;
		config->g_error_resilient = VPX_ERROR_RESILIENT_PARTITIONS;
		token_parts = (cpus > 1) ? 3 : 0;

		// rate control settings
		config->rc_dropframe_thresh = 0;
		config->rc_end_usage = VPX_CBR;
		//config->g_pass = VPX_RC_ONE_PASS;
		config->kf_mode = VPX_KF_AUTO;
		config->kf_max_dist = 360;//1000;

		//config->kf_mode = VPX_KF_DISABLED;
		config->rc_resize_allowed = 1;
		//config->rc_min_quantizer = 0;
		//config->rc_max_quantizer = 63;
		config->rc_min_quantizer = 0;
		config->rc_max_quantizer = 63;
		//Rate control adaptation undershoot control.
		//	This value, expressed as a percentage of the target bitrate,
		//	controls the maximum allowed adaptation speed of the codec.
		//	This factor controls the maximum amount of bits that can be
		//	subtracted from the target bitrate in order to compensate for
		//	prior overshoot.
		//	Valid values in the range 0-1000.
		config->rc_undershoot_pct = 100;
		//Rate control adaptation overshoot control.
		//	This value, expressed as a percentage of the target bitrate,
		//	controls the maximum allowed adaptation speed of the codec.
		//	This factor controls the maximum amount of bits that can be
		//	added to the target bitrate in order to compensate for prior
		//	undershoot.
		//	Valid values in the range 0-1000.
		config->rc_overshoot_pct = 15;
		//Decoder Buffer Size.
		//	This value indicates the amount of data that may be buffered
		//	by the decoding application. Note that this value is expressed
		//	in units of time (milliseconds). For example, a value of 5000
		//	indicates that the client will buffer (at least) 5000ms worth
		//	of encoded data. Use the target bitrate (rc_target_bitrate) to
		//	convert to bits/bytes, if necessary.
		config->rc_buf_sz = 5000;
		//Decoder Buffer Initial Size.
		//	This value indicates the amount of data that will be buffered
		//	by the decoding application prior to beginning playback.
		//	This value is expressed in units of time (milliseconds).
		//	Use the target bitrate (rc_target_bitrate) to convert to
		//	bits/bytes, if necessary.
		config->rc_buf_initial_sz = 1000;
		//Decoder Buffer Optimal Size.
		//	This value indicates the amount of data that the encoder should
		//	try to maintain in the decoder's buffer. This value is expressed
		//	in units of time (milliseconds).
		//	Use the target bitrate (rc_target_bitrate) to convert to
		//	bits/bytes, if necessary.
		config->rc_buf_optimal_sz = 1000;
	}

	if (context->encoder_init) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "VPX ENCODER RESET\n");
		if (vpx_codec_enc_config_set(&context->encoder, config) != VPX_CODEC_OK) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Codec init error: [%d:%s]\n", context->encoder.err, context->encoder.err_detail);
		}
	} else if (context->flags & SWITCH_CODEC_FLAG_ENCODE) {

		// #define SHOW(field) fprintf(stderr, "    %-28s = %d\n", #field, config->field);

#ifdef SHOW
		fprintf(stderr, "Codec: %s\n", vpx_codec_iface_name(context->encoder_interface));

		SHOW(g_usage);
		SHOW(g_threads);
		SHOW(g_profile);
		SHOW(g_w);
		SHOW(g_h);
		SHOW(g_bit_depth);
		SHOW(g_input_bit_depth);
		SHOW(g_timebase.num);
		SHOW(g_timebase.den);
		SHOW(g_error_resilient);
		SHOW(g_pass);
		SHOW(g_lag_in_frames);
		SHOW(rc_dropframe_thresh);
		SHOW(rc_resize_allowed);
		SHOW(rc_scaled_width);
		SHOW(rc_scaled_height);
		SHOW(rc_resize_up_thresh);
		SHOW(rc_resize_down_thresh);
		SHOW(rc_end_usage);
		SHOW(rc_target_bitrate);
		SHOW(rc_min_quantizer);
		SHOW(rc_max_quantizer);
		SHOW(rc_undershoot_pct);
		SHOW(rc_overshoot_pct);
		SHOW(rc_buf_sz);
		SHOW(rc_buf_initial_sz);
		SHOW(rc_buf_optimal_sz);
		SHOW(rc_2pass_vbr_bias_pct);
		SHOW(rc_2pass_vbr_minsection_pct);
		SHOW(rc_2pass_vbr_maxsection_pct);
		SHOW(kf_mode);
		SHOW(kf_min_dist);
		SHOW(kf_max_dist);
#endif

		if (vpx_codec_enc_init(&context->encoder, context->encoder_interface, config, 0 & VPX_CODEC_USE_OUTPUT_PARTITION) != VPX_CODEC_OK) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Codec %s init error: [%d:%s]\n", vpx_codec_iface_name(context->encoder_interface), context->encoder.err, context->encoder.err_detail);
			return SWITCH_STATUS_FALSE;
		}
		
		context->encoder_init = 1;

		if (context->is_vp9) {
			if (context->lossless) {
				vpx_codec_control(&context->encoder, VP9E_SET_LOSSLESS, 1);
				vpx_codec_control(&context->encoder, VP8E_SET_CPUUSED, -6);
			} else {
				vpx_codec_control(&context->encoder, VP8E_SET_CPUUSED, -8);
			}

			vpx_codec_control(&context->encoder, VP8E_SET_STATIC_THRESHOLD, 1000);
			vpx_codec_control(&context->encoder, VP8E_SET_TOKEN_PARTITIONS, token_parts);
			vpx_codec_control(&context->encoder, VP9E_SET_TUNE_CONTENT, VP9E_CONTENT_SCREEN);

		} else {
			// The static threshold imposes a change threshold on blocks below which they will be skipped by the encoder.
			vpx_codec_control(&context->encoder, VP8E_SET_STATIC_THRESHOLD, 100);
			//Set cpu usage, a bit lower than normal (-6) but higher than android (-12)
			vpx_codec_control(&context->encoder, VP8E_SET_CPUUSED, -6);
			vpx_codec_control(&context->encoder, VP8E_SET_TOKEN_PARTITIONS, token_parts);
			
			// Enable noise reduction
			vpx_codec_control(&context->encoder, VP8E_SET_NOISE_SENSITIVITY, 1);
			//Set max data rate for Intra frames.
			//	This value controls additional clamping on the maximum size of a keyframe.
			//	It is expressed as a percentage of the average per-frame bitrate, with the
			//	special (and default) value 0 meaning unlimited, or no additional clamping
			//	beyond the codec's built-in algorithm.
			//	For example, to allocate no more than 4.5 frames worth of bitrate to a keyframe, set this to 450.
			//vpx_codec_control(&context->encoder, VP8E_SET_MAX_INTRA_BITRATE_PCT, 0);
		}
	}

	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t switch_vpx_init(switch_codec_t *codec, switch_codec_flag_t flags, const switch_codec_settings_t *codec_settings)
{
	vpx_context_t *context = NULL;
	int encoding, decoding;

	encoding = (flags & SWITCH_CODEC_FLAG_ENCODE);
	decoding = (flags & SWITCH_CODEC_FLAG_DECODE);

	if (!(encoding || decoding) || ((context = switch_core_alloc(codec->memory_pool, sizeof(*context))) == 0)) {
		return SWITCH_STATUS_FALSE;
	}

	memset(context, 0, sizeof(*context));
	context->flags = flags;
	codec->private_info = context;
	context->pool = codec->memory_pool;

	if (codec_settings) {
		context->codec_settings = *codec_settings;
	}

	if (!strcmp(codec->implementation->iananame, "VP9")) {
		context->is_vp9 = 1;
		context->encoder_interface = vpx_codec_vp9_cx();
		context->decoder_interface = vpx_codec_vp9_dx();
	} else {
		context->encoder_interface = vpx_codec_vp8_cx();
		context->decoder_interface = vpx_codec_vp8_dx();
	}

	if (codec->fmtp_in) {
		codec->fmtp_out = switch_core_strdup(codec->memory_pool, codec->fmtp_in);
	}

	if (vpx_codec_enc_config_default(context->encoder_interface, &context->config, 0) != VPX_CODEC_OK) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Encoder config Error\n");
		return SWITCH_STATUS_FALSE;
	}

	context->codec_settings.video.width = 320;
	context->codec_settings.video.height = 240;

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "VPX VER:%s VPX_IMAGE_ABI_VERSION:%d VPX_CODEC_ABI_VERSION:%d\n",
		vpx_codec_version_str(), VPX_IMAGE_ABI_VERSION, VPX_CODEC_ABI_VERSION);

	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t consume_partition(vpx_context_t *context, switch_frame_t *frame)
{
	vpx_payload_descriptor_t *payload_descriptor;
	uint8_t *body;
	uint32_t hdrlen = 0, payload_size = 0, packet_size = 0, start = 0, key = 0;
	switch_size_t remaining_bytes = 0;

	if (!context->pkt) {
		if ((context->pkt = vpx_codec_get_cx_data(&context->encoder, &context->enc_iter))) {
			start = 1;
			if (!context->pbuffer) {
				switch_buffer_create_partition(context->pool, &context->pbuffer, context->pkt->data.frame.buf, context->pkt->data.frame.sz);
			} else {
				switch_buffer_set_partition_data(context->pbuffer, context->pkt->data.frame.buf, context->pkt->data.frame.sz);
			}
		}
	}

	if (context->pbuffer) {
		remaining_bytes = switch_buffer_inuse(context->pbuffer);
	}

	if (!context->pkt || context->pkt->kind != VPX_CODEC_CX_FRAME_PKT || !remaining_bytes) {
		frame->datalen = 0;
		frame->m = 1;
		context->pkt = NULL;
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "writing 0 bytes\n");
		return SWITCH_STATUS_SUCCESS;
	}

	key = (context->pkt->data.frame.flags & VPX_FRAME_IS_KEY);

#if 0
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "flags: %x pts: %lld duration:%lu partition_id: %d\n",
		context->pkt->data.frame.flags, context->pkt->data.frame.pts, context->pkt->data.frame.duration, context->pkt->data.frame.partition_id);
#endif

	/* reset header */
	*(uint8_t *)frame->data = 0;
	payload_descriptor = (vpx_payload_descriptor_t *) frame->data;

	// if !extended
	hdrlen = 1;
	body = ((uint8_t *)frame->data) + hdrlen;
	packet_size = SLICE_SIZE;
	payload_size = packet_size - hdrlen;
	// else add extended TBD

	frame->datalen = hdrlen;

	if (context->is_vp9) {
		payload_descriptor->vp9.start = start;

		if (1) {
			// payload_descriptor->vp9.have_p_layer = key; // key?
			payload_descriptor->vp9.have_pid = 1;

			if (payload_descriptor->vp9.have_pid) {

				if (context->vp9.picture_id < 0) context->vp9.picture_id = 0;

				if (context->vp9.picture_id > 0x7f) {
					*body++ = (context->vp9.picture_id >> 8) | 0x80;
					*body++ = context->vp9.picture_id & 0xff;
					payload_size--;
					frame->datalen++;
				} else {
					*body++ = context->vp9.picture_id;
				}


				payload_size--;
				frame->datalen++;
			}

			if (start) {
				context->vp9.picture_id++;
#ifdef DEBUG_VP9
				// switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "sending pid: %d\n", context->vp9.picture_id);
#endif
			}

			if (key) {
				vp9_ss_t *ss = (vp9_ss_t *)body;

				payload_descriptor->vp9.have_ss = 1;
				payload_descriptor->vp9.have_p_layer = 0;
				ss->n_s = 0;
				ss->g = 0;
				ss->y = 0;
				ss->zero = 0;
				body++;
				payload_size--;
				frame->datalen++;

				if (0) { // y ?
					uint16_t *w;
					uint16_t *h;

					ss->y = 1;

					w = (uint16_t *)body;
					body+=2;
					h = (uint16_t *)body;
					body+=2;

					*w = (uint16_t)context->codec_settings.video.width;
					*h = (uint16_t)context->codec_settings.video.height;

					payload_size-= (ss->n_s + 1) * 4;
					frame->datalen+= (ss->n_s + 1) * 4;
				}
			} else {
				payload_descriptor->vp9.have_p_layer = 1;
			}
		}

	} else {
		payload_descriptor->vp8.start = start;
	}

	if (remaining_bytes <= payload_size) {
		switch_buffer_read(context->pbuffer, body, remaining_bytes);
		context->pkt = NULL;
		frame->datalen += remaining_bytes;
		frame->m = 1;
		return SWITCH_STATUS_SUCCESS;
	} else {
		switch_buffer_read(context->pbuffer, body, payload_size);
		frame->datalen += payload_size;
		frame->m = 0;
		return SWITCH_STATUS_MORE_DATA;
	}

	if (frame->m && context->is_vp9) {
		payload_descriptor->vp9.end = 1;
	}
}

static switch_status_t reset_codec_encoder(switch_codec_t *codec)
{
	vpx_context_t *context = (vpx_context_t *)codec->private_info;

	if (context->encoder_init) {
		vpx_codec_destroy(&context->encoder);
	}
	context->last_ts = 0;
	context->last_ms = 0;
	context->framecount = 0;
	context->encoder_init = 0;
	context->pkt = NULL;
	return init_encoder(codec);
}

static switch_status_t switch_vpx_encode(switch_codec_t *codec, switch_frame_t *frame)
{
	vpx_context_t *context = (vpx_context_t *)codec->private_info;
	int width = 0;
	int height = 0;
	uint32_t dur;
	int64_t pts;
	vpx_enc_frame_flags_t vpx_flags = 0;
	switch_time_t now;
	int err;

	if (frame->flags & SFF_SAME_IMAGE) {
		return consume_partition(context, frame);
	}

	if (context->need_encoder_reset != 0) {
		if (reset_codec_encoder(codec) != SWITCH_STATUS_SUCCESS) {
			return SWITCH_STATUS_FALSE;
		}
		context->need_encoder_reset = 0;
	}

	if (frame->img->d_h > 1) {
		width = frame->img->d_w;
		height = frame->img->d_h;
	} else {
		width = frame->img->w;
		height = frame->img->h;
	}

	if (context->config.g_w != width || context->config.g_h != height) {
		context->codec_settings.video.width = width;
		context->codec_settings.video.height = height;
		reset_codec_encoder(codec);
		frame->flags |= SFF_PICTURE_RESET;
		context->need_key_frame = 3;
	}

	
	if (!context->encoder_init) {
		if (init_encoder(codec) != SWITCH_STATUS_SUCCESS) {
			return SWITCH_STATUS_FALSE;
		}
	}

	if (context->change_bandwidth) {
		context->codec_settings.video.bandwidth = context->change_bandwidth;
		context->change_bandwidth = 0;
		if (init_encoder(codec) != SWITCH_STATUS_SUCCESS) {
			return SWITCH_STATUS_FALSE;
		}
	}

	now = switch_time_now();

	if (context->need_key_frame > 0) {
		// force generate a key frame

		if (!context->last_key_frame || (now - context->last_key_frame) > KEY_FRAME_MIN_FREQ) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG1, "VPX KEYFRAME GENERATED\n");
			vpx_flags |= VPX_EFLAG_FORCE_KF;
			context->need_key_frame = 0;
			context->last_key_frame = now;
		}
	}

	context->framecount++;

	pts = (now - context->start_time) / 1000;

	dur = context->last_ms ? (now - context->last_ms) / 1000 : pts;

	if ((err = vpx_codec_encode(&context->encoder,
						 (vpx_image_t *) frame->img,
						 pts,
						 dur,
						 vpx_flags,
						 VPX_DL_REALTIME)) != VPX_CODEC_OK) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "VPX encode error %d:%s:%s\n",
			err, vpx_codec_error(&context->encoder), vpx_codec_error_detail(&context->encoder));
		frame->datalen = 0;
		return SWITCH_STATUS_FALSE;
	}

	context->enc_iter = NULL;
	context->last_ts = frame->timestamp;
	context->last_ms = now;
		
	return consume_partition(context, frame);
}

static switch_status_t buffer_vp8_packets(vpx_context_t *context, switch_frame_t *frame)
{
	uint8_t *data = frame->data;
	uint8_t S;
	uint8_t DES;
	//	uint8_t PID;
	int len;
#if 0
	int key = 0;

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, 
					  "VIDEO VPX: seq: %d ts: %u len: %ld %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x mark: %d\n",
					  frame->seq, frame->timestamp, frame->datalen,
					  *((uint8_t *)data), *((uint8_t *)data + 1),
					  *((uint8_t *)data + 2), *((uint8_t *)data + 3),
					  *((uint8_t *)data + 4), *((uint8_t *)data + 5),
					  *((uint8_t *)data + 6), *((uint8_t *)data + 7),
					  *((uint8_t *)data + 8), *((uint8_t *)data + 9),
					  *((uint8_t *)data + 10), frame->m);
#endif

	
	DES = *data;
	data++;
	S = (DES & 0x10);
	//	PID = DES & 0x07;

	//switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "DATA LEN %d S BIT %d PID: %d\n", frame->datalen, S, PID);
			
	if (DES & 0x80) { // X
		uint8_t X = *data;
		//switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "X BIT SET\n");
		data++;
		if (X & 0x80) { // I
			uint8_t M = (*data) & 0x80;
			//switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "I BIT SET\n");
			data++;
			if (M) {
				//switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "M BIT SET\n");
				data++;
			}
		}
		if (X & 0x40) {
			//switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "L BIT SET\n");
			data++; // L
		}
		if (X & 0x30) {
			//switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "T/K BIT SET\n");
			data++; // T/K
		}
	}

	if (!switch_buffer_inuse(context->vpx_packet_buffer) && !S) {
		if (context->got_key_frame > 0) {
			context->got_key_frame = 0;
			context->got_start_frame = 0;
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG2, "packet loss?\n");
		}
		return SWITCH_STATUS_MORE_DATA;
	}

	if (S) {
		switch_buffer_zero(context->vpx_packet_buffer);
		context->last_received_timestamp = frame->timestamp;
#if 0
		if (PID == 0) {
			key = __IS_VP8_KEY_FRAME(*data);
		}
#endif
	}

	len = frame->datalen - (data - (uint8_t *)frame->data);
	//switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "POST PARSE: DATA LEN %d KEY %d KEYBYTE = %0x\n", len, key, *data);
	
	if (len <= 0) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Invalid packet %d\n", len);
		return SWITCH_STATUS_RESTART;
	}

	if (context->last_received_timestamp != frame->timestamp) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG2, "wrong timestamp %u, expect %u, packet loss?\n", frame->timestamp, context->last_received_timestamp);
		switch_buffer_zero(context->vpx_packet_buffer);
		return SWITCH_STATUS_RESTART;
	}

	switch_buffer_write(context->vpx_packet_buffer, data, len);
	return SWITCH_STATUS_SUCCESS;
}

// https://tools.ietf.org/id/draft-ietf-payload-vp9-01.txt

static switch_status_t buffer_vp9_packets(vpx_context_t *context, switch_frame_t *frame)
{
	uint8_t *data = (uint8_t *)frame->data;
	uint8_t *vp9  = (uint8_t *)frame->data;
	vp9_payload_descriptor_t *desc = (vp9_payload_descriptor_t *)vp9;
	int len = 0;

#ifdef DEBUG_VP9
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "%02x %02x %02x %02x m=%d len=%4d "
					"have_pid=%d "
					"have_p_layer=%d "
					"have_layer_ind=%d "
					"is_flexible=%d "
					"start=%d "
					"end=%d "
					"have_ss=%d "
					"zero=%d\n",
					*data, *(data+1), *(data+2), *(data+3), frame->m, frame->datalen,
					desc->have_pid,
					desc->have_p_layer,
					desc->have_layer_ind,
					desc->is_flexible,
					desc->start,
					desc->end,
					desc->have_ss,
					desc->zero);
#endif

	vp9++;

	if (desc->have_pid) {
		uint16_t pid = 0;

		pid = *vp9 & 0x7f;	//0 bit is M , 1-7 bit is pid.

		if (*vp9 & 0x80) {	//if (M==1)
			vp9++;
			pid = (pid << 8) + *vp9;
		}

		vp9++;

#ifdef DEBUG_VP9
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "have pid: %d start=%d end=%d\n", pid, desc->start, desc->end);
#endif

	}

	if (desc->have_layer_ind) {
#ifdef DEBUG_VP9
		vp9_p_layer_t *layer = (vp9_p_layer_t *)vp9;

		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "temporal_id=%d temporal_up_switch=%d spatial_id=%d inter_layer_predicted=%d\n",
			layer->temporal_id, layer->temporal_up_switch, layer->spatial_id, layer->inter_layer_predicted);
#endif

		vp9++;
		if (!desc->is_flexible) {
			vp9++; // TL0PICIDX
		}
	}

	//When P and F are both set to one, indicating a non-key frame in flexible mode
	if (desc->have_p_layer && desc->is_flexible) { // P & F set, P_DIFF
		if (*vp9 & 1) { // N
			vp9++;
			if (*vp9 & 1) { // N
				vp9++;
				if (*vp9 & 1) {
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Invalid VP9 packet!");
					switch_buffer_zero(context->vpx_packet_buffer);
					goto end;
				}
			}
		}
		vp9++;
	}

	if (desc->have_ss) {
		vp9_ss_t *ss = (vp9_ss_t *)(vp9++);

#ifdef DEBUG_VP9
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "have ss: %02x n_s: %d y:%d g:%d\n", *(uint8_t *)ss, ss->n_s, ss->y, ss->g);
#endif
		if (ss->y) {
			int i;

			for (i=0; i<=ss->n_s; i++) {
#ifdef DEBUG_VP9
				int width = ntohs(*(uint16_t *)vp9);
				int height = ntohs(*(uint16_t *)(vp9 + 2));
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "SS: %d %dx%d\n", i, width, height);
#endif
				vp9 += 4;
			}
		}

		if (ss->g) {
			int i;
			uint8_t ng = *vp9++;	//N_G indicates the number of frames in a GOF

			for (i = 0; ng > 0 && i < ng; i++) {
				vp9_n_g_t *n_g = (vp9_n_g_t *)(vp9++);
				vp9 += n_g->r;
			}
		}
	}

	if (vp9 - data >= frame->datalen) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG1, "Invalid VP9 Packet %" SWITCH_SSIZE_T_FMT " > %d\n", vp9 - data, frame->datalen);
		switch_buffer_zero(context->vpx_packet_buffer);
		goto end;
	}

	if (switch_buffer_inuse(context->vpx_packet_buffer)) { // middle packet
		if (desc->start) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG1, "got invalid vp9 packet, packet loss? resetting buffer\n");
			switch_buffer_zero(context->vpx_packet_buffer);
		}
	} else { // start packet
		if (!desc->start) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "got invalid vp9 packet, packet loss? waiting for a start packet\n");
			goto end;
		}
	}

	len = frame->datalen - (vp9 - data);
	switch_buffer_write(context->vpx_packet_buffer, vp9, len);

end:

#ifdef DEBUG_VP9
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "buffered %d bytes, buffer size: %" SWITCH_SIZE_T_FMT "\n", len, switch_buffer_inuse(context->vpx_packet_buffer));
#endif

	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t switch_vpx_decode(switch_codec_t *codec, switch_frame_t *frame)
{
	vpx_context_t *context = (vpx_context_t *)codec->private_info;
	switch_size_t len;
	vpx_codec_ctx_t *decoder = NULL;
	switch_status_t status = SWITCH_STATUS_SUCCESS;
	int is_start = 0, is_keyframe = 0, get_refresh = 0;

#if 0
	vp9_payload_descriptor_t *desc = (vp9_payload_descriptor_t *)frame->data;
	uint8_t *data = frame->data;
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "%02x %02x %02x %02x m=%d start=%d end=%d m=%d len=%d\n",
		*data, *(data+1), *(data+2), *(data+3), frame->m, desc->start, desc->end, frame->m, frame->datalen);
#endif

	if (context->is_vp9) {
		is_keyframe = IS_VP9_KEY_FRAME(*(unsigned char *)frame->data);
		is_start = IS_VP9_START_PKT(*(unsigned char *)frame->data);

#ifdef DEBUG_VP9
		if (is_keyframe) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "================Got a key frame!!!!========================\n");
		}
#endif
	} else { // vp8
		is_start = (*(unsigned char *)frame->data & 0x10);
		is_keyframe = IS_VP8_KEY_FRAME((uint8_t *)frame->data);

#ifdef DEBUG_VP9
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "VP8\n");
#endif
	}

    if (!is_keyframe && context->got_key_frame <= 0) {
		context->no_key_frame++;
		//switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "no keyframe, %d\n", context->no_key_frame);
		if (context->no_key_frame > 50) {
			if ((is_keyframe = is_start)) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "no keyframe, treating start as key. frames=%d\n", context->no_key_frame);
			}
		}
    }

	// if (is_keyframe) switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "got key %d\n", context->got_key_frame);

	if (context->need_decoder_reset != 0) {
		vpx_codec_destroy(&context->decoder);
		context->decoder_init = 0;
		init_decoder(codec);
		context->need_decoder_reset = 0;
	}
	
	if (!context->decoder_init) {
		init_decoder(codec);
	}

	if (!context->decoder_init) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "VPX decoder is not initialized!\n");
		return SWITCH_STATUS_FALSE;
	}

	decoder = &context->decoder;
	
	// switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "len: %d ts: %u mark:%d\n", frame->datalen, frame->timestamp, frame->m);

	// context->last_received_timestamp = frame->timestamp;
	context->last_received_complete_picture = frame->m ? SWITCH_TRUE : SWITCH_FALSE;

	if (is_start) {
		context->got_start_frame = 1;
	}
	
	if (is_keyframe) {
		if (context->got_key_frame <= 0) {
			context->got_key_frame = 1;
			context->no_key_frame = 0;
		} else {
			context->got_key_frame++;
		}
	} else if (context->got_key_frame <= 0) {
		if ((--context->got_key_frame % 200) == 0) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG1, "Waiting for key frame %d\n", context->got_key_frame);
		}

		get_refresh = 1;

		if (!context->got_start_frame) {
			switch_goto_status(SWITCH_STATUS_MORE_DATA, end);
		}
	}


	status = context->is_vp9 ? buffer_vp9_packets(context, frame) : buffer_vp8_packets(context, frame);


	if (context->dec_iter && (frame->img = (switch_image_t *) vpx_codec_get_frame(decoder, &context->dec_iter))) {
		switch_goto_status(SWITCH_STATUS_SUCCESS, end);
	}

	// switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "====READ buf:%ld got_key:%d st:%d m:%d\n", switch_buffer_inuse(context->vpx_packet_buffer), context->got_key_frame, status, frame->m);

	len = switch_buffer_inuse(context->vpx_packet_buffer);

	//if (frame->m && (status != SWITCH_STATUS_SUCCESS || !len)) {
		//switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "WTF????? %d %ld\n", status, len);
	//}

	if (status == SWITCH_STATUS_SUCCESS && frame->m && len) {
		uint8_t *data;
		int corrupted = 0;
		int err;

		switch_buffer_peek_zerocopy(context->vpx_packet_buffer, (void *)&data);

		context->dec_iter = NULL;
		err = vpx_codec_decode(decoder, data, (unsigned int)len, NULL, 0);

		if (err != VPX_CODEC_OK) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG1, "Error decoding %" SWITCH_SIZE_T_FMT " bytes, [%d:%s:%s]\n",
							  len, err, vpx_codec_error(decoder), vpx_codec_error_detail(decoder));
			switch_goto_status(SWITCH_STATUS_RESTART, end);
		}

		if (vpx_codec_control(decoder, VP8D_GET_FRAME_CORRUPTED, &corrupted) != VPX_CODEC_OK) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "VPX control error!\n");
			switch_goto_status(SWITCH_STATUS_RESTART, end);
		}
		
		if (corrupted) {
			frame->img = NULL;
#ifdef DEBUG_VP9
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "corrupted!!\n");
#endif
		} else {
			frame->img = (switch_image_t *) vpx_codec_get_frame(decoder, &context->dec_iter);

#ifdef DEBUG_VP9
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "decoded: %dx%d\n", frame->img->d_w, frame->img->d_h);
#endif
		}
		
		switch_buffer_zero(context->vpx_packet_buffer);
		
		if (!frame->img) {
			//context->need_decoder_reset = 1;
			context->got_key_frame = 0;
			context->got_start_frame = 0;
			status = SWITCH_STATUS_RESTART;
		}
	}

end:

	if (status == SWITCH_STATUS_RESTART) {
		switch_buffer_zero(context->vpx_packet_buffer);
		//context->need_decoder_reset = 1;
		context->got_key_frame = 0;
		context->got_start_frame = 0;
		//switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "RESET VPX\n");
	}

	if (!frame->img || status == SWITCH_STATUS_RESTART) {
		status = SWITCH_STATUS_MORE_DATA;
	}

	if (context->got_key_frame <= 0 || get_refresh) {
		switch_set_flag(frame, SFF_WAIT_KEY_FRAME);
	}

	return status;
}


static switch_status_t switch_vpx_control(switch_codec_t *codec, 
										  switch_codec_control_command_t cmd, 
										  switch_codec_control_type_t ctype,
										  void *cmd_data,
										  switch_codec_control_type_t atype,
										  void *cmd_arg,
										  switch_codec_control_type_t *rtype,
										  void **ret_data) 
{

	vpx_context_t *context = (vpx_context_t *)codec->private_info;

	switch(cmd) {
	case SCC_VIDEO_RESET:
		{
			int mask = *((int *) cmd_data);
			if (mask & 1) {
				context->need_encoder_reset = 1;
			}
			if (mask & 2) {
				context->need_decoder_reset = 1;
			}
		}
		break;
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


static switch_status_t switch_vpx_destroy(switch_codec_t *codec)
{
	vpx_context_t *context = (vpx_context_t *)codec->private_info;

	if (context) {
		if ((codec->flags & SWITCH_CODEC_FLAG_ENCODE)) {
			vpx_codec_destroy(&context->encoder);
		}

		if ((codec->flags & SWITCH_CODEC_FLAG_DECODE)) {
			vpx_codec_destroy(&context->decoder);
		}

		if (context->pic) {
			vpx_img_free(context->pic);
			context->pic = NULL;
		}
		if (context->vpx_packet_buffer) {
			switch_buffer_destroy(&context->vpx_packet_buffer);
			context->vpx_packet_buffer = NULL;
		}
	}
	return SWITCH_STATUS_SUCCESS;
}

SWITCH_MODULE_LOAD_FUNCTION(mod_vpx_load)
{
	switch_codec_interface_t *codec_interface;

	/* connect my internal structure to the blank pointer passed to me */
	*module_interface = switch_loadable_module_create_module_interface(pool, modname);
	SWITCH_ADD_CODEC(codec_interface, "VP8 Video");
	switch_core_codec_add_video_implementation(pool, codec_interface, 99, "VP8", NULL,
											   switch_vpx_init, switch_vpx_encode, switch_vpx_decode, switch_vpx_control, switch_vpx_destroy);
	SWITCH_ADD_CODEC(codec_interface, "VP9 Video");
	switch_core_codec_add_video_implementation(pool, codec_interface, 99, "VP9", NULL,
											   switch_vpx_init, switch_vpx_encode, switch_vpx_decode, switch_vpx_control, switch_vpx_destroy);

	/* indicate that the module should continue to be loaded */
	return SWITCH_STATUS_SUCCESS;
}

#endif
#endif
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
