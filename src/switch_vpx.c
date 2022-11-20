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

#ifdef DEBUG_VP9
#define VPX_SWITCH_LOG_LEVEL SWITCH_LOG_ERROR
#else
#define VPX_SWITCH_LOG_LEVEL SWITCH_LOG_DEBUG1
#endif

#define SLICE_SIZE SWITCH_DEFAULT_VIDEO_SIZE
#define KEY_FRAME_MIN_FREQ 250000

#define CODEC_TYPE_ANY 0
#define CODEC_TYPE_VP8 8
#define CODEC_TYPE_VP9 9

typedef struct my_vpx_cfg_s {
	char name[64];
	int lossless;
	int cpuused;
	int token_parts;
	int static_thresh;
	int noise_sensitivity;
	int max_intra_bitrate_pct;
	vp9e_tune_content tune_content;

	vpx_codec_enc_cfg_t enc_cfg;
	vpx_codec_dec_cfg_t dec_cfg;
	switch_event_t *codecs;
} my_vpx_cfg_t;

#define SHOW(cfg, field) switch_log_printf(SWITCH_CHANNEL_LOG_CLEAN, SWITCH_LOG_INFO, "    %-28s = %d\n", #field, cfg->field)

static void show_config(my_vpx_cfg_t *my_cfg, vpx_codec_enc_cfg_t *cfg)
{
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "    %-28s = %s\n", "name", my_cfg->name);
	switch_log_printf(SWITCH_CHANNEL_LOG_CLEAN, SWITCH_LOG_INFO, "    %-28s = %d\n", "decoder.threads", my_cfg->dec_cfg.threads);

	SHOW(my_cfg, lossless);
	SHOW(my_cfg, cpuused);
	SHOW(my_cfg, token_parts);
	SHOW(my_cfg, static_thresh);
	SHOW(my_cfg, noise_sensitivity);
	SHOW(my_cfg, max_intra_bitrate_pct);
	SHOW(my_cfg, tune_content);

	SHOW(cfg, g_usage);
	SHOW(cfg, g_threads);
	SHOW(cfg, g_profile);
	SHOW(cfg, g_w);
	SHOW(cfg, g_h);
	SHOW(cfg, g_bit_depth);
	SHOW(cfg, g_input_bit_depth);
	SHOW(cfg, g_timebase.num);
	SHOW(cfg, g_timebase.den);
	SHOW(cfg, g_error_resilient);
	SHOW(cfg, g_pass);
	SHOW(cfg, g_lag_in_frames);
	SHOW(cfg, rc_dropframe_thresh);
	SHOW(cfg, rc_resize_allowed);
	SHOW(cfg, rc_scaled_width);
	SHOW(cfg, rc_scaled_height);
	SHOW(cfg, rc_resize_up_thresh);
	SHOW(cfg, rc_resize_down_thresh);
	SHOW(cfg, rc_end_usage);
	SHOW(cfg, rc_target_bitrate);
	SHOW(cfg, rc_min_quantizer);
	SHOW(cfg, rc_max_quantizer);
	SHOW(cfg, rc_undershoot_pct);
	SHOW(cfg, rc_overshoot_pct);
	SHOW(cfg, rc_buf_sz);
	SHOW(cfg, rc_buf_initial_sz);
	SHOW(cfg, rc_buf_optimal_sz);
	SHOW(cfg, rc_2pass_vbr_bias_pct);
	SHOW(cfg, rc_2pass_vbr_minsection_pct);
	SHOW(cfg, rc_2pass_vbr_maxsection_pct);
	SHOW(cfg, kf_mode);
	SHOW(cfg, kf_min_dist);
	SHOW(cfg, kf_max_dist);
	SHOW(cfg, ss_number_layers);
	SHOW(cfg, ts_number_layers);
	SHOW(cfg, ts_periodicity);
	SHOW(cfg, temporal_layering_mode);

	if (my_cfg->codecs) {
		switch_event_header_t *hp;

		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "======== Codec specific profiles ========\n");

		for (hp = my_cfg->codecs->headers; hp; hp = hp->next) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "    %-28s = %s\n", hp->name, hp->value);
		}
	}
}

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
	unsigned I:1;
	unsigned L:1;
	unsigned T:1;
	unsigned K:1;
	unsigned RSV:4;
	unsigned M:1;
	unsigned PID:15;
	unsigned TL0PICIDX:8;
	unsigned TID:2;
	unsigned Y:1;
	unsigned KEYIDX:5;
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
	unsigned RSV:4;
	unsigned K:1;
	unsigned T:1;
	unsigned L:1;
	unsigned I:1;
	unsigned PID:15;
	unsigned M:1;
	unsigned TL0PICIDX:8;
	unsigned KEYIDX:5;
	unsigned Y:1;
	unsigned TID:2;
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

#ifdef WIN32
#undef SWITCH_MOD_DECLARE_DATA
#define SWITCH_MOD_DECLARE_DATA __declspec(dllexport)
#endif
SWITCH_MODULE_LOAD_FUNCTION(mod_vpx_load);
SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_vpx_shutdown);
SWITCH_MODULE_DEFINITION(CORE_VPX_MODULE, mod_vpx_load, mod_vpx_shutdown, NULL);

struct vpx_context {
	int debug;
	switch_codec_t *codec;
	int is_vp9;
	vp9_info_t vp9;
	vpx_codec_iface_t *encoder_interface;
	vpx_codec_iface_t *decoder_interface;
	unsigned int flags;
	switch_codec_settings_t codec_settings;
	unsigned int bandwidth;
	vpx_codec_enc_cfg_t config;
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
	int decoded_first_frame;
	switch_buffer_t *vpx_packet_buffer;
	int got_key_frame;
	int no_key_frame;
	int got_start_frame;
	uint32_t last_received_timestamp;
	switch_bool_t last_received_complete_picture;
	uint16_t last_received_seq;
	int need_key_frame;
	int need_encoder_reset;
	int need_decoder_reset;
	int32_t change_bandwidth;
	uint64_t framecount;
	switch_memory_pool_t *pool;
	switch_buffer_t *pbuffer;
	switch_time_t start_time;
	switch_image_t *patch_img;
	int16_t picture_id;
};
typedef struct vpx_context vpx_context_t;

#define MAX_PROFILES 100

struct vpx_globals {
	int debug;
	uint32_t max_bitrate;
	uint32_t rtp_slice_size;
	uint32_t key_frame_min_freq;

	uint32_t dec_threads;
	uint32_t enc_threads;

	my_vpx_cfg_t *profiles[MAX_PROFILES];
};

struct vpx_globals vpx_globals = { 0 };

static my_vpx_cfg_t *find_cfg_profile(const char *name, switch_bool_t reconfig);
static void parse_profile(my_vpx_cfg_t *my_cfg, switch_xml_t profile, int codec_type);

static switch_status_t init_decoder(switch_codec_t *codec)
{
	vpx_context_t *context = (vpx_context_t *)codec->private_info;

	//if (context->decoder_init) {
	//	vpx_codec_destroy(&context->decoder);
	//	context->decoder_init = 0;
	//}

	if (context->flags & SWITCH_CODEC_FLAG_DECODE && !context->decoder_init) {
		vpx_codec_dec_cfg_t cfg = {0, 0, 0};
		vpx_codec_flags_t dec_flags = 0;
		vp8_postproc_cfg_t ppcfg;
		my_vpx_cfg_t *my_cfg = NULL;
		vpx_codec_err_t err;

		if (context->is_vp9) {
			my_cfg = find_cfg_profile("vp9", SWITCH_FALSE);
		} else {
			my_cfg = find_cfg_profile("vp8", SWITCH_FALSE);
		}

		if (!my_cfg) return SWITCH_STATUS_FALSE;

		cfg.threads = my_cfg->dec_cfg.threads;

		if ((err = vpx_codec_dec_init(&context->decoder, context->decoder_interface, &cfg, dec_flags)) != VPX_CODEC_OK) {
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(codec->session), SWITCH_LOG_ERROR,
				"VPX decoder %s codec init error: [%d:%s:%s]\n",
				vpx_codec_iface_name(context->decoder_interface),
				err, vpx_codec_error(&context->decoder), vpx_codec_error_detail(&context->decoder));
			return SWITCH_STATUS_FALSE;
		}

		context->last_ts = 0;
		context->last_received_timestamp = 0;
		context->last_received_complete_picture = 0;
		context->last_received_seq = 0;
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

static int CODEC_TYPE(const char *string)
{
	if (!strcmp(string, "vp8")) {
		return CODEC_TYPE_VP8;
	} else if (!strcmp(string, "vp9")) {
		return CODEC_TYPE_VP9;
	}

	return CODEC_TYPE_ANY;
}

static void parse_codec_specific_profile(my_vpx_cfg_t *my_cfg, const char *codec_name)
{
	switch_xml_t cfg = NULL;
	switch_xml_t xml = switch_xml_open_cfg("vpx.conf", &cfg, NULL);
	switch_xml_t profiles = cfg ? switch_xml_child(cfg, "profiles") : NULL;

	// open config and find the profile to parse
	if (profiles) {
		switch_event_header_t *hp;

		for (hp = my_cfg->codecs->headers; hp; hp = hp->next) {
			// switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "%s: %s\n", hp->name, hp->value);
			if (!strcmp(hp->name, codec_name)) {
				switch_xml_t profile;
				for (profile = switch_xml_child(profiles, "profile"); profile; profile = profile->next) {
					const char *name = switch_xml_attr(profile, "name");

					if (!strcmp(hp->value, name)) {
						parse_profile(my_cfg, profile, CODEC_TYPE(codec_name));
					}
				}
			}
		}
	}

	if (xml) switch_xml_free(xml);
}

static switch_status_t init_encoder(switch_codec_t *codec)
{
	vpx_context_t *context = (vpx_context_t *)codec->private_info;
	vpx_codec_enc_cfg_t *config = &context->config;
	my_vpx_cfg_t *my_cfg = NULL;
	vpx_codec_err_t err;
	char *codec_name = "vp8";

	if (context->is_vp9) {
		codec_name = "vp9";
	}

	if (!zstr(context->codec_settings.video.config_profile_name)) {
		my_cfg = find_cfg_profile(context->codec_settings.video.config_profile_name, SWITCH_FALSE);
	}

	if (!my_cfg) {
		my_cfg = find_cfg_profile(codec_name, SWITCH_FALSE);
	}

	if (!my_cfg) return SWITCH_STATUS_FALSE;

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "config: %s\n", my_cfg->name);

	if (context->is_vp9) {
		my_cfg->enc_cfg.g_profile = 0; // default build of VP9 only support 0, TODO: remove this
	}

	if (my_cfg->codecs) {
		parse_codec_specific_profile(my_cfg, codec_name);
	}

	if (vpx_globals.debug) show_config(my_cfg, &my_cfg->enc_cfg);

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

	if (context->bandwidth > vpx_globals.max_bitrate) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(codec->session), SWITCH_LOG_WARNING, "REQUESTED BITRATE TRUNCATED FROM %d TO %d\n", context->bandwidth, vpx_globals.max_bitrate);
		context->bandwidth = vpx_globals.max_bitrate;
	}

	switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(codec->session), SWITCH_LOG_NOTICE,
		"VPX encoder reset (WxH/BW) from %dx%d/%u to %dx%d/%u\n",
		config->g_w, config->g_h, config->rc_target_bitrate,
		context->codec_settings.video.width, context->codec_settings.video.height, context->bandwidth);

	context->pkt = NULL;
	context->start_time = switch_micro_time_now();

	*config = my_cfg->enc_cfg; // reset whole config to current defaults

	config->g_w = context->codec_settings.video.width;
	config->g_h = context->codec_settings.video.height;
	config->rc_target_bitrate = context->bandwidth;

	if (context->is_vp9) {
		if (my_cfg->lossless) {
			config->rc_min_quantizer = 0;
			config->rc_max_quantizer = 0;
		}
	}

	if (context->encoder_init) {
		if ((err = vpx_codec_enc_config_set(&context->encoder, config)) != VPX_CODEC_OK) {
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(codec->session), SWITCH_LOG_ERROR,
				"VPX encoder %s codec reconf error: [%d:%s:%s]\n",
				vpx_codec_iface_name(context->encoder_interface),
				err, vpx_codec_error(&context->encoder), vpx_codec_error_detail(&context->encoder));
			return SWITCH_STATUS_FALSE;
		}
	} else if (context->flags & SWITCH_CODEC_FLAG_ENCODE) {
		if (vpx_globals.debug) {
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(codec->session), SWITCH_LOG_INFO, "VPX encoder %s settings:\n", vpx_codec_iface_name(context->encoder_interface));
			show_config(my_cfg, config);
		}

		if ((err = vpx_codec_enc_init(&context->encoder, context->encoder_interface, config, 0 & VPX_CODEC_USE_OUTPUT_PARTITION)) != VPX_CODEC_OK) {
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(codec->session), SWITCH_LOG_ERROR,
				"VPX encoder %s codec init error: [%d:%s:%s]\n",
				vpx_codec_iface_name(context->encoder_interface),
				err, vpx_codec_error(&context->encoder), vpx_codec_error_detail(&context->encoder));
			return SWITCH_STATUS_FALSE;
		}

		context->encoder_init = 1;

		vpx_codec_control(&context->encoder, VP8E_SET_TOKEN_PARTITIONS, my_cfg->token_parts);
		vpx_codec_control(&context->encoder, VP8E_SET_CPUUSED, my_cfg->cpuused);
		vpx_codec_control(&context->encoder, VP8E_SET_STATIC_THRESHOLD, my_cfg->static_thresh);

		if (context->is_vp9) {
			if (my_cfg->lossless) {
				vpx_codec_control(&context->encoder, VP9E_SET_LOSSLESS, 1);
			}

			vpx_codec_control(&context->encoder, VP9E_SET_TUNE_CONTENT, my_cfg->tune_content);
		} else {
			vpx_codec_control(&context->encoder, VP8E_SET_NOISE_SENSITIVITY, my_cfg->noise_sensitivity);

			if (my_cfg->max_intra_bitrate_pct) {
				vpx_codec_control(&context->encoder, VP8E_SET_MAX_INTRA_BITRATE_PCT, my_cfg->max_intra_bitrate_pct);
			}
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

	context->codec_settings.video.width = 320;
	context->codec_settings.video.height = 240;

	switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(codec->session), SWITCH_LOG_DEBUG, "VPX VER:%s VPX_IMAGE_ABI_VERSION:%d VPX_CODEC_ABI_VERSION:%d\n",
		vpx_codec_version_str(), VPX_IMAGE_ABI_VERSION, VPX_CODEC_ABI_VERSION);

	if (!context->is_vp9) {
		context->picture_id =  13; // picture Id may start from random value and must be incremented on each frame
	} else {
		context->vp9.picture_id = 13;
	}

	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t consume_partition(vpx_context_t *context, switch_frame_t *frame)
{
	vpx_payload_descriptor_t *payload_descriptor;
	uint8_t *body, *c = NULL;
	uint32_t hdrlen = 0, payload_size = 0, max_payload_size = 0, start = 0, key = 0;
	switch_size_t remaining_bytes = 0;
	switch_status_t status;

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
	memset(payload_descriptor, 0, sizeof(*payload_descriptor));


	if (context->is_vp9) {
		hdrlen = 1;				/* Send VP9 with 1 byte REQUIRED header. */
	} else {
		hdrlen = 4;				/* Send VP8 with 4 byte extended header, includes 1 byte REQUIRED header, 1 byte X header and 2 bytes of I header with picture_id. */
	}

	body = ((uint8_t *)frame->data) + hdrlen;

	if (context->is_vp9) {
		payload_descriptor->vp9.start = start;

		if (1) {
			// payload_descriptor->vp9.have_p_layer = key; // key?
			payload_descriptor->vp9.have_pid = 1;

			if (payload_descriptor->vp9.have_pid) {
				if (context->vp9.picture_id > 0x7f) {
					*body++ = (context->vp9.picture_id >> 8) | 0x80;
					*body++ = context->vp9.picture_id & 0xff;
					hdrlen += 2;
				} else {
					*body++ = context->vp9.picture_id;
					hdrlen++;
				}
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
				hdrlen++;

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

					hdrlen += (ss->n_s + 1) * 4;
				}
			} else {
				payload_descriptor->vp9.have_p_layer = 1;
			}
		}
	}

	if (!context->is_vp9) {
		payload_descriptor->vp8.start = start;
		
		payload_descriptor->vp8.extended = 1;	/* REQUIRED header. */
		
		payload_descriptor->vp8.I = 1;			/* X header. */
		
		payload_descriptor->vp8.M = 1;			/* I header. */
		c = ((uint8_t *)frame->data) + 2;
		*c++ = (context->picture_id >> 8) | 0x80;
		*c = context->picture_id & 0xff;
		
		payload_descriptor->vp8.L = 0;
		payload_descriptor->vp8.TL0PICIDX = 0;
		
		payload_descriptor->vp8.T = 0;
		payload_descriptor->vp8.TID = 0;
		payload_descriptor->vp8.Y = 0;
		payload_descriptor->vp8.K = 0;
		payload_descriptor->vp8.KEYIDX = 0;
	}

	/*
		Try to split payload to packets evenly(with largest at the end) up to vpx_globals.rtp_slice_size,
		(assume hdrlen constant across all packets of the same picture).
		It keeps packets being transmitted in order.
		Without it last (and thus the smallest one) packet usually arrive out of order
		(before the previous one)
	*/
	max_payload_size = vpx_globals.rtp_slice_size - hdrlen;
	payload_size = remaining_bytes / ((remaining_bytes + max_payload_size - 1) / max_payload_size);

	if (remaining_bytes <= payload_size) {
		switch_buffer_read(context->pbuffer, body, remaining_bytes);
		context->pkt = NULL;
		frame->datalen = hdrlen + remaining_bytes;
		frame->m = 1;

		// increment and wrap picture_id (if needed) after the last picture's packet
		if (context->is_vp9) {
			context->vp9.picture_id++;
			if ((uint16_t)context->vp9.picture_id > 0x7fff) {
				context->vp9.picture_id = 0;
			}
		} else {
			context->picture_id++;
			if ((uint16_t)context->picture_id > 0x7fff) {
				context->picture_id = 0;
			}
		}

		status = SWITCH_STATUS_SUCCESS;
	} else {
		switch_buffer_read(context->pbuffer, body, payload_size);
		frame->datalen = hdrlen + payload_size;
		frame->m = 0;
		status = SWITCH_STATUS_MORE_DATA;
	}

	if (frame->m && context->is_vp9) {
		payload_descriptor->vp9.end = 1;
	}

	return status;
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
	vpx_codec_err_t err;

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

	if (context->codec_settings.video.width != width || context->codec_settings.video.height != height) {
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
		if (!context->last_key_frame || (now - context->last_key_frame) > vpx_globals.key_frame_min_freq) {
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(codec->session), VPX_SWITCH_LOG_LEVEL,
				"VPX encoder keyframe request\n");
			vpx_flags |= VPX_EFLAG_FORCE_KF;
			context->need_key_frame = 0;
			context->last_key_frame = now;
		}
	}

	context->framecount++;

	pts = (now - context->start_time) / 1000;
	//pts = frame->timestamp;

	dur = context->last_ms ? (now - context->last_ms) / 1000 : pts;

	if ((err = vpx_codec_encode(&context->encoder,
						 (vpx_image_t *) frame->img,
						 pts,
						 dur,
						 vpx_flags,
						 VPX_DL_REALTIME)) != VPX_CODEC_OK) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(codec->session), SWITCH_LOG_ERROR, "VPX encode error [%d:%s:%s]\n",
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

	if (context->debug > 0) {
		switch_log_printf(SWITCH_CHANNEL_LOG, context->debug,
					  "VIDEO VPX: seq: %d ts: %u len: %u %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x mark: %d\n",
					  frame->seq, frame->timestamp, frame->datalen,
					  *((uint8_t *)data), *((uint8_t *)data + 1),
					  *((uint8_t *)data + 2), *((uint8_t *)data + 3),
					  *((uint8_t *)data + 4), *((uint8_t *)data + 5),
					  *((uint8_t *)data + 6), *((uint8_t *)data + 7),
					  *((uint8_t *)data + 8), *((uint8_t *)data + 9),
					  *((uint8_t *)data + 10), frame->m);
	}

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

	if (context->debug > 0) {
		switch_log_printf(SWITCH_CHANNEL_LOG, frame->m ? SWITCH_LOG_ERROR : SWITCH_LOG_INFO,
					"[%02x %02x %02x %02x] m=%d len=%4d seq=%d ts=%u ssrc=%u "
					"have_pid=%d "
					"have_p_layer=%d "
					"have_layer_ind=%d "
					"is_flexible=%d "
					"start=%d "
					"end=%d "
					"have_ss=%d "
					"zero=%d\n",
					*data, *(data+1), *(data+2), *(data+3), frame->m, frame->datalen, frame->seq, frame->timestamp, frame->ssrc,
					desc->have_pid,
					desc->have_p_layer,
					desc->have_layer_ind,
					desc->is_flexible,
					desc->start,
					desc->end,
					desc->have_ss,
					desc->zero);
	}

	vp9++;

	if (desc->have_pid) {
#ifdef DEBUG_VP9
		uint16_t pid = 0;
		pid = *vp9 & 0x7f;	//0 bit is M , 1-7 bit is pid.
#endif
		if (*vp9 & 0x80) {	//if (M==1)
			vp9++;
#ifdef DEBUG_VP9
			pid = (pid << 8) + *vp9;
#endif 
		}

		vp9++;

#ifdef DEBUG_VP9
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "have pid: %d start=%d end=%d\n", pid, desc->start, desc->end);
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

	if (!switch_buffer_inuse(context->vpx_packet_buffer)) { // start packet
		if (!desc->start) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "got invalid vp9 packet, packet loss? waiting for a start packet\n");
			goto end;
		}
	}

	len = frame->datalen - (vp9 - data);
	switch_buffer_write(context->vpx_packet_buffer, vp9, len);

end:

#ifdef DEBUG_VP9
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "buffered %d bytes, buffer size: %" SWITCH_SIZE_T_FMT "\n", len, switch_buffer_inuse(context->vpx_packet_buffer));
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

	if (context->debug > 0 && context->debug < 4) {
		vp9_payload_descriptor_t *desc = (vp9_payload_descriptor_t *)frame->data;
		uint8_t *data = frame->data;
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "%02x %02x %02x %02x m=%d start=%d end=%d m=%d len=%d\n",
			*data, *(data+1), *(data+2), *(data+3), frame->m, desc->start, desc->end, frame->m, frame->datalen);
	}

	if (context->is_vp9) {
		is_keyframe = IS_VP9_KEY_FRAME(*(unsigned char *)frame->data);
		is_start = IS_VP9_START_PKT(*(unsigned char *)frame->data);

		if (is_keyframe) {
			switch_log_printf(SWITCH_CHANNEL_LOG, VPX_SWITCH_LOG_LEVEL, "================Got a key frame!!!!========================\n");
		}

		if (context->last_received_seq && context->last_received_seq + 1 != frame->seq) {
			switch_log_printf(SWITCH_CHANNEL_LOG, VPX_SWITCH_LOG_LEVEL, "Packet loss detected last=%d got=%d lost=%d\n", context->last_received_seq, frame->seq, frame->seq - context->last_received_seq);
			if (is_keyframe && context->vpx_packet_buffer) switch_buffer_zero(context->vpx_packet_buffer);
		}

		context->last_received_seq = frame->seq;
	} else { // vp8
		is_start = (*(unsigned char *)frame->data & 0x10);
		is_keyframe = IS_VP8_KEY_FRAME((uint8_t *)frame->data);
	}

    if (!is_keyframe && context->got_key_frame <= 0) {
		context->no_key_frame++;
		//switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "no keyframe, %d\n", context->no_key_frame);
		if (context->no_key_frame > 50) {
			if ((is_keyframe = is_start)) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG1, "no keyframe, treating start as key. frames=%d\n", context->no_key_frame);
			}
		}
    }

	if (context->debug > 0 && is_keyframe) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "GOT KEY FRAME %d\n", context->got_key_frame);
	}

	if (context->need_decoder_reset != 0) {
		vpx_codec_destroy(&context->decoder);
		context->decoder_init = 0;
		status = init_decoder(codec);
		context->need_decoder_reset = 0;
	}

	if (status != SWITCH_STATUS_SUCCESS) goto end;

	if (!context->decoder_init) {
		status = init_decoder(codec);
	}

	if (status != SWITCH_STATUS_SUCCESS) goto end;

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
		switch_set_flag(frame, SFF_IS_KEYFRAME);
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
		vpx_codec_err_t err;

		switch_buffer_peek_zerocopy(context->vpx_packet_buffer, (void *)&data);

		context->dec_iter = NULL;
		err = vpx_codec_decode(decoder, data, (unsigned int)len, NULL, 0);

		if (err != VPX_CODEC_OK) {
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(codec->session), (context->decoded_first_frame ? SWITCH_LOG_ERROR : VPX_SWITCH_LOG_LEVEL),
				"VPX error decoding %" SWITCH_SIZE_T_FMT " bytes, [%d:%s:%s]\n",
				len, err, vpx_codec_error(decoder), vpx_codec_error_detail(decoder));

			if (err == VPX_CODEC_MEM_ERROR) {
				switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(codec->session), SWITCH_LOG_WARNING, "VPX MEM ERROR, resetting decoder!\n");
				context->need_decoder_reset = 1;
			}

			switch_goto_status(SWITCH_STATUS_RESTART, end);
		} else {
			if (!context->decoded_first_frame) context->decoded_first_frame = 1;
		}

		if (vpx_codec_control(decoder, VP8D_GET_FRAME_CORRUPTED, &corrupted) != VPX_CODEC_OK) {
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(codec->session), SWITCH_LOG_WARNING, "VPX control error!\n");
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

	if (frame->img && (codec->flags & SWITCH_CODEC_FLAG_VIDEO_PATCHING)) {
		switch_img_free(&context->patch_img);
		switch_img_copy(frame->img, &context->patch_img);
		frame->img = context->patch_img;
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
	case SCC_CODEC_SPECIFIC:
		{
			const char *command = (const char *)cmd_data;

			if (ctype == SCCT_INT) {
			} else if (ctype == SCCT_STRING && !zstr(command)) {
				if (!strcasecmp(command, "VP8E_SET_CPUUSED")) {
					vpx_codec_control(&context->encoder, VP8E_SET_CPUUSED, *(int *)cmd_arg);
				} else if (!strcasecmp(command, "VP8E_SET_TOKEN_PARTITIONS")) {
					vpx_codec_control(&context->encoder, VP8E_SET_TOKEN_PARTITIONS, *(int *)cmd_arg);
				} else if (!strcasecmp(command, "VP8E_SET_NOISE_SENSITIVITY")) {
					vpx_codec_control(&context->encoder, VP8E_SET_NOISE_SENSITIVITY, *(int *)cmd_arg);
				}
			}

		}
		break;
	case SCC_DEBUG:
		{
			int32_t level = *((uint32_t *) cmd_data);
			context->debug = level;
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

		switch_img_free(&context->patch_img);

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

static void init_vp8(my_vpx_cfg_t *my_cfg)
{
	vpx_codec_enc_config_default(vpx_codec_vp8_cx(), &my_cfg->enc_cfg, 0);

	my_cfg->dec_cfg.threads = vpx_globals.dec_threads;
	my_cfg->enc_cfg.g_threads = vpx_globals.enc_threads;
	my_cfg->static_thresh = 100;
	my_cfg->noise_sensitivity = 1;

	my_cfg->cpuused = -6;
	my_cfg->enc_cfg.g_profile = 2;
	my_cfg->enc_cfg.g_timebase.num = 1;
	my_cfg->enc_cfg.g_timebase.den = 1000;
	my_cfg->enc_cfg.g_error_resilient = VPX_ERROR_RESILIENT_PARTITIONS;
	my_cfg->enc_cfg.rc_resize_allowed = 1;
	my_cfg->enc_cfg.rc_end_usage = VPX_CBR;
	my_cfg->enc_cfg.rc_target_bitrate = switch_parse_bandwidth_string("1mb");
	my_cfg->enc_cfg.rc_min_quantizer = 4;
	my_cfg->enc_cfg.rc_max_quantizer = 63;
	my_cfg->enc_cfg.rc_overshoot_pct = 50;
	my_cfg->enc_cfg.rc_buf_sz = 5000;
	my_cfg->enc_cfg.rc_buf_initial_sz = 1000;
	my_cfg->enc_cfg.rc_buf_optimal_sz = 1000;
	my_cfg->enc_cfg.kf_max_dist = 360;
}

static void init_vp9(my_vpx_cfg_t *my_cfg)
{
	vpx_codec_enc_config_default(vpx_codec_vp9_cx(), &my_cfg->enc_cfg, 0);

	my_cfg->dec_cfg.threads = vpx_globals.dec_threads;
	my_cfg->enc_cfg.g_threads = vpx_globals.enc_threads;
	my_cfg->static_thresh = 1000;

	my_cfg->cpuused = -8;
	my_cfg->enc_cfg.g_profile = 0;
	my_cfg->enc_cfg.g_lag_in_frames = 0;
	my_cfg->enc_cfg.g_timebase.den = 1000;
	my_cfg->enc_cfg.g_error_resilient = VPX_ERROR_RESILIENT_PARTITIONS;
	my_cfg->enc_cfg.rc_resize_allowed = 1;
	my_cfg->enc_cfg.rc_end_usage = VPX_CBR;
	my_cfg->enc_cfg.rc_target_bitrate = switch_parse_bandwidth_string("1mb");
	my_cfg->enc_cfg.rc_min_quantizer = 4;
	my_cfg->enc_cfg.rc_max_quantizer = 63;
	my_cfg->enc_cfg.rc_overshoot_pct = 50;
	my_cfg->enc_cfg.rc_buf_sz = 5000;
	my_cfg->enc_cfg.rc_buf_initial_sz = 1000;
	my_cfg->enc_cfg.rc_buf_optimal_sz = 1000;
	my_cfg->enc_cfg.kf_max_dist = 360;
	my_cfg->tune_content = VP9E_CONTENT_SCREEN;
}

static my_vpx_cfg_t *find_cfg_profile(const char *name, switch_bool_t reconfig)
{
	int i;

	for (i = 0; i < MAX_PROFILES; i++) {
		if (!vpx_globals.profiles[i]) {
			vpx_globals.profiles[i] = malloc(sizeof(my_vpx_cfg_t));
			switch_assert(vpx_globals.profiles[i]);
			memset(vpx_globals.profiles[i], 0, sizeof(my_vpx_cfg_t));
			switch_set_string(vpx_globals.profiles[i]->name, name);

			if (!strcmp(name, "vp9")) {
				init_vp9(vpx_globals.profiles[i]);
			} else {
				init_vp8(vpx_globals.profiles[i]);
			}

			vpx_globals.profiles[i]->token_parts = switch_core_cpu_count() > 1 ? 3 : 0;

			return vpx_globals.profiles[i];
		}

		if (!strcmp(name, vpx_globals.profiles[i]->name)) {
			if (reconfig) {
				memset(vpx_globals.profiles[i], 0, sizeof(my_vpx_cfg_t));
				switch_set_string(vpx_globals.profiles[i]->name, name);
			}

			return vpx_globals.profiles[i];
		}
	}

	return NULL;
}

#define UINTVAL(v) (v > 0 ? v : 0);
#define _VPX_CHECK_ERR(fmt, ...) switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "VPX config param \"%s\" -> \"%s\" value \"%s\" " fmt "\n", profile_name, name, value, __VA_ARGS__)
#define _VPX_CHECK_ERRDEF(var, fmt, ...) _VPX_CHECK_ERR(fmt ", leave default %d", __VA_ARGS__, var)
#define _VPX_CHECK_ERRDEF_INVL(var) _VPX_CHECK_ERRDEF(var, "%s", "is invalid")
#define _VPX_CHECK_ERRDEF_NOTAPPL(var) _VPX_CHECK_ERRDEF(var, "%s", "is not applicable")
#define _VPX_CHECK_MIN(var, val, min) do { int lval = val; if (lval < (min)) _VPX_CHECK_ERRDEF(var, "is lower than %d", min); else var = lval; } while(0)
#define _VPX_CHECK_MAX(var, val, max) do { int lval = val; if (lval > (max)) _VPX_CHECK_ERRDEF(var, "is larger than %d", max); else var = lval; } while(0)
#define _VPX_CHECK_MIN_MAX(var, val, min, max) do { int lval = val; if ((lval < (min)) || (lval > (max))) _VPX_CHECK_ERRDEF(var, "not in [%d..%d]", min, max); else var = lval; } while(0)

static void parse_profile(my_vpx_cfg_t *my_cfg, switch_xml_t profile, int codec_type)
{
	switch_xml_t param = NULL;
	const char *profile_name = profile->name;

	vpx_codec_dec_cfg_t *dec_cfg = NULL;
	vpx_codec_enc_cfg_t *enc_cfg = NULL;

	dec_cfg = &my_cfg->dec_cfg;
	enc_cfg = &my_cfg->enc_cfg;

	for (param = switch_xml_child(profile, "param"); param; param = param->next) {
		const char *name = switch_xml_attr(param, "name");
		const char *value = switch_xml_attr(param, "value");
		int val;

		if (!enc_cfg || !dec_cfg) break;
		if (zstr(name) || zstr(value)) continue;

		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "%s: %s = %s\n", my_cfg->name, name, value);

		val = atoi(value);

		if (!strcmp(name, "dec-threads")) {
			_VPX_CHECK_MIN(dec_cfg->threads, switch_parse_cpu_string(value), 1);
		} else if (!strcmp(name, "enc-threads")) {
			_VPX_CHECK_MIN(enc_cfg->g_threads, switch_parse_cpu_string(value), 1);
		} else if (!strcmp(name, "g-profile")) {
			_VPX_CHECK_MIN_MAX(enc_cfg->g_profile, val, 0, 3);
#if 0
		} else if (!strcmp(name, "g-timebase")) {
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
				enc_cfg->g_timebase.num = num;
				enc_cfg->g_timebase.den = den;
			}
#endif
		} else if (!strcmp(name, "g-error-resilient")) {
			char *s = strdup(value);
			if (s) {
				vpx_codec_er_flags_t res = 0;
				int argc;
				char *argv[10];
				int i;

				argc = switch_separate_string(s, '|', argv, (sizeof(argv) / sizeof(argv[0])));
				for (i = 0; i < argc; i++) {
					if (!strcasecmp(argv[i], "DEFAULT")) {
						res |= VPX_ERROR_RESILIENT_DEFAULT;
					} else if (!strcasecmp(argv[i], "PARTITIONS")) {
						res |= VPX_ERROR_RESILIENT_PARTITIONS;
					} else {
						_VPX_CHECK_ERR("has invalid token \"%s\"", argv[i]);
					}
				}

				free(s);
				enc_cfg->g_error_resilient = res;
			}
		} else if (!strcmp(name, "g-pass")) {
			if (!strcasecmp(value, "ONE_PASS")) {
				enc_cfg->g_pass = VPX_RC_ONE_PASS;
			} else if (!strcasecmp(value, "FIRST_PASS")) {
				enc_cfg->g_pass = VPX_RC_FIRST_PASS;
			} else if (!strcasecmp(value, "LAST_PASS")) {
				enc_cfg->g_pass = VPX_RC_FIRST_PASS;
			} else {
				_VPX_CHECK_ERRDEF_INVL(enc_cfg->g_pass);
			}
		} else if (!strcmp(name, "g-lag-in-frames")) {
			_VPX_CHECK_MIN_MAX(enc_cfg->g_lag_in_frames, val, 0, 25);
		} else if (!strcmp(name, "rc_dropframe_thresh")) {
			_VPX_CHECK_MIN_MAX(enc_cfg->rc_dropframe_thresh, val, 0, 100);
		} else if (!strcmp(name, "rc-resize-allowed")) {
			_VPX_CHECK_MIN_MAX(enc_cfg->rc_resize_allowed, val, 0, 1);
		} else if (!strcmp(name, "rc-scaled-width")) {
			_VPX_CHECK_MIN(enc_cfg->rc_scaled_width, val, 0);
		} else if (!strcmp(name, "rc-scaled-height")) {
			_VPX_CHECK_MIN(enc_cfg->rc_scaled_height, val, 0);
		} else if (!strcmp(name, "rc-resize-up-thresh")) {
			_VPX_CHECK_MIN_MAX(enc_cfg->rc_resize_up_thresh, val, 0, 100);
		} else if (!strcmp(name, "rc-resize-down-thresh")) {
			_VPX_CHECK_MIN_MAX(enc_cfg->rc_resize_down_thresh, val, 0, 100);
		} else if (!strcmp(name, "rc-end-usage")) {
			if (!strcasecmp(value, "VBR")) {
				enc_cfg->rc_end_usage = VPX_VBR;
			} else if (!strcasecmp(value, "CBR")) {
				enc_cfg->rc_end_usage = VPX_CBR;
			} else if (!strcasecmp(value, "CQ")) {
				enc_cfg->rc_end_usage = VPX_CQ;
			} else if (!strcasecmp(value, "Q")) {
				enc_cfg->rc_end_usage = VPX_Q;
			} else {
				_VPX_CHECK_ERRDEF_INVL(enc_cfg->rc_end_usage);
			}
		} else if (!strcmp(name, "rc-target-bitrate")) {
			_VPX_CHECK_MIN(enc_cfg->rc_target_bitrate, switch_parse_bandwidth_string(value), 1);
		} else if (!strcmp(name, "rc-min-quantizer")) {
			_VPX_CHECK_MIN_MAX(enc_cfg->rc_min_quantizer, val, 0, 63);
		} else if (!strcmp(name, "rc-max-quantizer")) {
			_VPX_CHECK_MIN_MAX(enc_cfg->rc_max_quantizer, val, 0, 63);
		} else if (!strcmp(name, "rc-undershoot-pct")) {
			if (codec_type == CODEC_TYPE_VP9) {
				_VPX_CHECK_MIN_MAX(enc_cfg->rc_undershoot_pct, val, 0, 100);
			} else {
				_VPX_CHECK_MIN_MAX(enc_cfg->rc_undershoot_pct, val, 0, 1000);
			}
		} else if (!strcmp(name, "rc-overshoot-pct")) {
			if (codec_type == CODEC_TYPE_VP9) {
				_VPX_CHECK_MIN_MAX(enc_cfg->rc_overshoot_pct, val, 0, 100);
			} else {
				_VPX_CHECK_MIN_MAX(enc_cfg->rc_overshoot_pct, val, 0, 1000);
			}
		} else if (!strcmp(name, "rc-buf-sz")) {
			_VPX_CHECK_MIN(enc_cfg->rc_buf_sz, val, 1);
		} else if (!strcmp(name, "rc-buf-initial-sz")) {
			_VPX_CHECK_MIN(enc_cfg->rc_buf_initial_sz, val, 1);
		} else if (!strcmp(name, "rc-buf-optimal-sz")) {
			_VPX_CHECK_MIN(enc_cfg->rc_buf_optimal_sz, val, 1);
		} else if (!strcmp(name, "rc-2pass-vbr-bias-pct")) {
			_VPX_CHECK_MIN_MAX(enc_cfg->rc_2pass_vbr_bias_pct, val, 0, 100);
		} else if (!strcmp(name, "rc-2pass-vbr-minsection-pct")) {
			_VPX_CHECK_MIN(enc_cfg->rc_2pass_vbr_minsection_pct, val, 1);
		} else if (!strcmp(name, "rc-2pass-vbr-maxsection-pct")) {
			_VPX_CHECK_MIN(enc_cfg->rc_2pass_vbr_maxsection_pct, val, 1);
		} else if (!strcmp(name, "kf-mode")) {
			if (!strcasecmp(value, "AUTO")) {
				enc_cfg->kf_mode = VPX_KF_AUTO;
			} else if (!strcasecmp(value, "DISABLED")) {
				enc_cfg->kf_mode = VPX_KF_DISABLED;
			} else {
				_VPX_CHECK_ERRDEF_INVL(enc_cfg->kf_mode);
			}
		} else if (!strcmp(name, "kf-min-dist")) {
			_VPX_CHECK_MIN(enc_cfg->kf_min_dist, val, 0);
		} else if (!strcmp(name, "kf-max-dist")) {
			_VPX_CHECK_MIN(enc_cfg->kf_max_dist, val, 0);
		} else if (!strcmp(name, "ss-number-layers")) {
			_VPX_CHECK_MIN_MAX(enc_cfg->ss_number_layers, val, 0, VPX_SS_MAX_LAYERS);
		} else if (!strcmp(name, "ts-number-layers")) {
			if (codec_type == CODEC_TYPE_VP8) {
				_VPX_CHECK_MIN_MAX(enc_cfg->ts_number_layers, val, 0, VPX_SS_MAX_LAYERS);
			} else if (codec_type == CODEC_TYPE_VP9) {
				_VPX_CHECK_MIN_MAX(enc_cfg->ts_number_layers, val, enc_cfg->ts_number_layers, enc_cfg->ts_number_layers); // lock it
			} else {
				_VPX_CHECK_ERRDEF_NOTAPPL(enc_cfg->ts_number_layers);
			}
		} else if (!strcmp(name, "ts-periodicity")) {
			if (codec_type == CODEC_TYPE_VP9) {
				_VPX_CHECK_MIN_MAX(enc_cfg->ts_periodicity, val, enc_cfg->ts_periodicity, enc_cfg->ts_periodicity); // lock it
			} else {
				_VPX_CHECK_MIN_MAX(enc_cfg->ts_periodicity, val, 0, 16);
			}
		} else if (!strcmp(name, "temporal-layering-mode")) {
			if (codec_type == CODEC_TYPE_VP9) {
				_VPX_CHECK_MIN_MAX(enc_cfg->temporal_layering_mode, val, enc_cfg->temporal_layering_mode, enc_cfg->temporal_layering_mode); // lock it
			} else {
				_VPX_CHECK_MIN_MAX(enc_cfg->temporal_layering_mode, val, 0, 3);
			}
		} else if (!strcmp(name, "lossless")) {
			if (codec_type == CODEC_TYPE_VP9) {
				_VPX_CHECK_MIN_MAX(my_cfg->lossless, val, 0, 1);
			} else {
				_VPX_CHECK_ERRDEF_NOTAPPL(my_cfg->lossless);
			}
		} else if (!strcmp(name, "cpuused")) {
			if (codec_type == CODEC_TYPE_VP8) {
				_VPX_CHECK_MIN_MAX(my_cfg->cpuused, val, -16, 16);
			} else {
				_VPX_CHECK_MIN_MAX(my_cfg->cpuused, val, -8, 8);
			}
		} else if (!strcmp(name, "token-parts")) {
			_VPX_CHECK_MIN_MAX(my_cfg->token_parts, switch_parse_cpu_string(value), VP8_ONE_TOKENPARTITION, VP8_EIGHT_TOKENPARTITION);
		} else if (!strcmp(name, "static-thresh")) {
			_VPX_CHECK_MIN(my_cfg->static_thresh, val, 0);
		} else if (!strcmp(name, "noise-sensitivity")) {
			if (codec_type == CODEC_TYPE_VP8) {
				_VPX_CHECK_MIN_MAX(my_cfg->noise_sensitivity, val, 0, 6);
			} else {
				_VPX_CHECK_ERRDEF_NOTAPPL(my_cfg->noise_sensitivity);
			}
		} else if (!strcmp(name, "max-intra-bitrate-pct")) {
			if (codec_type == CODEC_TYPE_VP8) {
				_VPX_CHECK_MIN(my_cfg->max_intra_bitrate_pct, val, 0);
			} else {
				_VPX_CHECK_ERRDEF_NOTAPPL(my_cfg->max_intra_bitrate_pct);
			}
		} else if (!strcmp(name, "vp9e-tune-content")) {
			if (codec_type == CODEC_TYPE_VP9) {
				if (!strcasecmp(value, "DEFAULT")) {
					my_cfg->tune_content = VP9E_CONTENT_DEFAULT;
				} else if (!strcasecmp(value, "SCREEN")) {
					my_cfg->tune_content = VP9E_CONTENT_SCREEN;
				} else {
					_VPX_CHECK_ERRDEF_INVL(my_cfg->tune_content);
				}
			} else {
				_VPX_CHECK_ERRDEF_NOTAPPL(my_cfg->tune_content);
			}
		}
	} // for param
}

static void parse_codecs(my_vpx_cfg_t *my_cfg, switch_xml_t codecs)
{
	switch_xml_t codec = NULL;

	if (!codecs) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "no codecs in %s\n", my_cfg->name);
		return;
	}

	codec = switch_xml_child(codecs, "codec");

	if (my_cfg->codecs) {
		switch_event_destroy(&my_cfg->codecs);
	}

	switch_event_create(&my_cfg->codecs, SWITCH_EVENT_CLONE);

	for (; codec; codec = codec->next) {
		const char *name = switch_xml_attr(codec, "name");
		const char *profile = switch_xml_attr(codec, "profile");

		if (zstr(name) || zstr(profile)) continue;

		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "codec: %s, profile: %s\n", name, profile);

		switch_event_add_header_string(my_cfg->codecs, SWITCH_STACK_BOTTOM, name, profile);
	}
}

static void load_config()
{
	switch_xml_t cfg = NULL, xml = NULL;
	my_vpx_cfg_t *my_cfg = NULL;

	memset(&vpx_globals, 0, sizeof(vpx_globals));

	vpx_globals.max_bitrate = switch_calc_bitrate(1920, 1080, 5, 60);
	vpx_globals.rtp_slice_size = SLICE_SIZE;
	vpx_globals.key_frame_min_freq = KEY_FRAME_MIN_FREQ;

	xml = switch_xml_open_cfg("vpx.conf", &cfg, NULL);

	if (xml) {
		switch_xml_t settings = switch_xml_child(cfg, "settings");
		switch_xml_t profiles = switch_xml_child(cfg, "profiles");

		if (settings) {
			switch_xml_t param;

			for (param = switch_xml_child(settings, "param"); param; param = param->next) {
				const char *profile_name = "settings"; // for _VPX_CHECK_*() macroses only
				const char *name = switch_xml_attr(param, "name");
				const char *value = switch_xml_attr(param, "value");

				if (zstr(name) || zstr(value)) continue;

				if (!strcmp(name, "debug")) {
					vpx_globals.debug = atoi(value);
				} else if (!strcmp(name, "max-bitrate")) {
					_VPX_CHECK_MIN(vpx_globals.max_bitrate, switch_parse_bandwidth_string(value), 1);
				} else if (!strcmp(name, "rtp-slice-size")) {
					_VPX_CHECK_MIN_MAX(vpx_globals.rtp_slice_size, atoi(value), 500, 1500);
				} else if (!strcmp(name, "key-frame-min-freq")) {
					_VPX_CHECK_MIN_MAX(vpx_globals.key_frame_min_freq, atoi(value) * 1000, 10 * 1000, 3000 * 1000);
				} else if (!strcmp(name, "dec-threads")) {
					int val = switch_parse_cpu_string(value);
					_VPX_CHECK_MIN(vpx_globals.dec_threads, val, 1);
				} else if (!strcmp(name, "enc-threads")) {
					int val = switch_parse_cpu_string(value);
					_VPX_CHECK_MIN(vpx_globals.enc_threads, val, 1);
				}
			}
		}

		if (profiles) {
			switch_xml_t profile = switch_xml_child(profiles, "profile");

			for (; profile; profile = profile->next) {
				switch_xml_t codecs = switch_xml_child(profile, "codecs");
				const char *profile_name = switch_xml_attr(profile, "name");
				my_vpx_cfg_t *my_cfg = NULL;

				if (zstr(profile_name)) continue;

				my_cfg = find_cfg_profile(profile_name, SWITCH_TRUE);

				if (!my_cfg) continue;

				parse_profile(my_cfg, profile, CODEC_TYPE(profile_name));
				parse_codecs(my_cfg, codecs);
			} // for profile
		} // profiles

		switch_xml_free(xml);
	} // xml

	if (vpx_globals.max_bitrate <= 0) {
		vpx_globals.max_bitrate = switch_calc_bitrate(1920, 1080, 5, 60);
	}

	if (vpx_globals.rtp_slice_size < 500 || vpx_globals.rtp_slice_size > 1500) {
		vpx_globals.rtp_slice_size = SLICE_SIZE;
	}

	if (vpx_globals.key_frame_min_freq < 10000 || vpx_globals.key_frame_min_freq > 3 * 1000000) {
		vpx_globals.key_frame_min_freq = KEY_FRAME_MIN_FREQ;
	}

	my_cfg = find_cfg_profile("vp8", SWITCH_FALSE);

	if (my_cfg) {
		if (!my_cfg->enc_cfg.g_threads) my_cfg->enc_cfg.g_threads = 1;
		if (!my_cfg->dec_cfg.threads) my_cfg->dec_cfg.threads = switch_parse_cpu_string("cpu/2/4");
	}

	my_cfg = find_cfg_profile("vp9", SWITCH_FALSE);

	if (my_cfg) {
		if (!my_cfg->enc_cfg.g_threads) my_cfg->enc_cfg.g_threads = 1;
		if (!my_cfg->dec_cfg.threads) my_cfg->dec_cfg.threads = switch_parse_cpu_string("cpu/2/4");
	}
}

#define VPX_API_SYNTAX "<reload|debug <on|off>>"
SWITCH_STANDARD_API(vpx_api_function)
{
	if (session) {
		return SWITCH_STATUS_FALSE;
	}

	if (zstr(cmd)) {
		goto usage;
	}

	if (!strcasecmp(cmd, "reload")) {
		const char *err;
		my_vpx_cfg_t *my_cfg;
		int i;

		switch_xml_reload(&err);
		stream->write_function(stream, "Reload XML [%s]\n", err);

		load_config();

		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "    %-26s = %d\n", "rtp-slice-size", vpx_globals.rtp_slice_size);
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "    %-26s = %d\n", "key-frame-min-freq", vpx_globals.key_frame_min_freq);

		for (i = 0; i < MAX_PROFILES; i++) {
			my_cfg = vpx_globals.profiles[i];

			if (!my_cfg) break;

			if (!strcmp(my_cfg->name, "vp8")) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "Codec: %s\n", vpx_codec_iface_name(vpx_codec_vp8_cx()));
			} else if (!strcmp(my_cfg->name, "vp9")) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "Codec: %s\n", vpx_codec_iface_name(vpx_codec_vp9_cx()));
			} else {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "Codec: %s\n", my_cfg->name);
			}

			show_config(my_cfg, &my_cfg->enc_cfg);
		}

		stream->write_function(stream, "+OK\n");
	} else if (!strcasecmp(cmd, "debug")) {
		stream->write_function(stream, "+OK debug %s\n", vpx_globals.debug ? "on" : "off");
	} else if (!strcasecmp(cmd, "debug on")) {
		vpx_globals.debug = 1;
		stream->write_function(stream, "+OK debug on\n");
	} else if (!strcasecmp(cmd, "debug off")) {
		vpx_globals.debug = 0;
		stream->write_function(stream, "+OK debug off\n");
	}

	return SWITCH_STATUS_SUCCESS;

  usage:
	stream->write_function(stream, "USAGE: %s\n", VPX_API_SYNTAX);
	return SWITCH_STATUS_SUCCESS;
}

SWITCH_MODULE_LOAD_FUNCTION(mod_vpx_load)
{
	switch_codec_interface_t *codec_interface;
	switch_api_interface_t *vpx_api_interface;

	memset(&vpx_globals, 0, sizeof(struct vpx_globals));
	load_config();

	/* connect my internal structure to the blank pointer passed to me */
	*module_interface = switch_loadable_module_create_module_interface(pool, modname);

	SWITCH_ADD_CODEC(codec_interface, "VP8 Video");
	switch_core_codec_add_video_implementation(pool, codec_interface, 99, "VP8", NULL,
											   switch_vpx_init, switch_vpx_encode, switch_vpx_decode, switch_vpx_control, switch_vpx_destroy);
	SWITCH_ADD_CODEC(codec_interface, "VP9 Video");
	switch_core_codec_add_video_implementation(pool, codec_interface, 99, "VP9", NULL,
											   switch_vpx_init, switch_vpx_encode, switch_vpx_decode, switch_vpx_control, switch_vpx_destroy);

	SWITCH_ADD_API(vpx_api_interface, "vpx",
				   "VPX API", vpx_api_function, VPX_API_SYNTAX);

	switch_console_set_complete("add vpx reload");
	switch_console_set_complete("add vpx debug");
	switch_console_set_complete("add vpx debug on");
	switch_console_set_complete("add vpx debug off");

	/* indicate that the module should continue to be loaded */
	return SWITCH_STATUS_SUCCESS;
}

SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_vpx_shutdown)
{
	int i;

	for (i = 0; i < MAX_PROFILES; i++) {
		my_vpx_cfg_t *my_cfg = vpx_globals.profiles[i];

		if (!my_cfg) break;

		if (my_cfg->codecs) {
			switch_event_destroy(&my_cfg->codecs);
		}

		free(my_cfg);
	}

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
