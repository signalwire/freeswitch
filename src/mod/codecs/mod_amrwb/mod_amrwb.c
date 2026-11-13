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
 * Anthony Minessale II <anthm@freeswitch.org>
 * Brian K. West <brian@freeswitch.org>
 * Dragos Oancea <dragos.oancea@athonet.com>
 * Federico Favaro <federico.favaro@athonet.com>
 * Marco Sinibaldi <marco.sinibaldi@athonet.com>
 *
 * The amrwb codec itself is not distributed with this module.
 *
 * mod_amrwb.c -- GSM-AMRWB Codec Module
 *
 */

#include "switch.h"

SWITCH_MODULE_LOAD_FUNCTION(mod_amrwb_load);
SWITCH_MODULE_DEFINITION(mod_amrwb, mod_amrwb_load, NULL, NULL);

#ifndef AMRWB_PASSTHROUGH
#include "opencore-amrwb/dec_if.h" /*AMR-WB decoder API*/
#include "vo-amrwbenc/enc_if.h" /*AMR-WB encoder API*/

#include "amr_payload.h"

typedef enum {
	AMRWB_OPT_OCTET_ALIGN = (1 << 0),
	AMRWB_OPT_CRC = (1 << 1),
	AMRWB_OPT_MODE_CHANGE_NEIGHBOR = (1 << 2),
	AMRWB_OPT_ROBUST_SORTING = (1 << 3),
	AMRWB_OPT_INTERLEAVING = (1 << 4)
} amrwb_flag_t;

typedef enum {
	AMRWB_BITRATE_7K = 0,
	AMRWB_BITRATE_8K,
	AMRWB_BITRATE_12K,
	AMRWB_BITRATE_14K,
	AMRWB_BITRATE_16K,
	AMRWB_BITRATE_18K,
	AMRWB_BITRATE_20K,
	AMRWB_BITRATE_23K,
	AMRWB_BITRATE_24K
} amrwb_bitrate_t;

struct amrwb_context {
	void *encoder_state;
	void *decoder_state;
	uint16_t enc_modes;          /* bitmask of modes allowed by mode-set; 9 modes, so a byte is too small */
	switch_byte_t enc_mode;
	uint32_t change_period;
	switch_byte_t max_ptime;
	switch_byte_t ptime;
	switch_byte_t channels;
	switch_byte_t flags;
	int max_red;
	int debug;
	int frames_per_packet;       /* 20 ms frames per RTP packet (ptime / 20) */
	int interleaving;            /* "interleaving" fmtp value: max frame-blocks per group, 0 = off */
	amr_pl_deinterleaver_t dil;
};

#define SWITCH_AMRWB_DEFAULT_BITRATE AMRWB_BITRATE_24K

static struct {
	switch_byte_t default_bitrate;
	switch_byte_t volte;
	switch_byte_t adjust_bitrate;
	switch_byte_t force_oa; /*force OA when originating*/
	switch_byte_t mode_set_overwrite;
	int debug;
} globals;

/* speech bits per frame type (RFC 4867 table 2): modes 0-8, SID, 10-13 reserved, 14 SPEECH_LOST, 15 NO_DATA */
static const int amrwb_frame_bits[16] = { 132, 177, 253, 285, 317, 365, 397, 461, 477, 40, -1, -1, -1, -1, 0, 0 };
static const amr_pl_desc_t amrwb_desc = { "AMR-WB", amrwb_frame_bits, 8, 320 };

#define SWITCH_AMRWB_MAX_MODE 8      /* highest speech mode */
#define SWITCH_AMRWB_SID 9
#define SWITCH_AMRWB_SPEECH_LOST 14
#define SWITCH_AMRWB_FRAME_BYTES 640 /* decoded bytes per 20 ms frame */
#define SWITCH_AMRWB_STORAGE_MAX 61  /* encoder output: TOC + 60 bytes */

static void switch_amrwb_info(switch_codec_t *codec, const amr_pl_packet_t *pkt, int encoded_data_len, int octet_aligned, const char *print_text)
{
	int i;

	switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(codec->session), SWITCH_LOG_DEBUG, "%s (%s): %d bytes, CMR [%d], %d frame(s)%s\n",
					  print_text, octet_aligned ? "OA" : "BE", encoded_data_len, pkt->cmr, pkt->nframes, pkt->interleaved ? " interleaved" : "");
	if (pkt->interleaved) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(codec->session), SWITCH_LOG_DEBUG, "%s: ILL [%d] ILP [%d]\n", print_text, pkt->ill, pkt->ilp);
	}
	for (i = 0; i < pkt->nframes; i++) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(codec->session), SWITCH_LOG_DEBUG, "%s: frame %d FT [0x%x] Q [%d] speech bytes [%d]\n",
						  print_text, i, pkt->frames[i].ft, pkt->frames[i].q, amr_pl_frame_bytes(&amrwb_desc, pkt->frames[i].ft));
	}
}

/* the closest mode allowed by mode-set, searching from `target` towards `from` */
static switch_byte_t amrwb_allowed_mode(struct amrwb_context *context, int target, int from)
{
	int step = target >= from ? -1 : 1, m;

	if (target < 0) target = 0;
	if (target > SWITCH_AMRWB_MAX_MODE) target = SWITCH_AMRWB_MAX_MODE;
	if (!context->enc_modes || globals.mode_set_overwrite) return (switch_byte_t) target;
	for (m = target; m >= 0 && m <= SWITCH_AMRWB_MAX_MODE; m += step) {
		if (context->enc_modes & (1 << m)) return (switch_byte_t) m;
		if (m == from) break;
	}
	return (switch_byte_t) from;
}
#endif

static switch_status_t switch_amrwb_init(switch_codec_t *codec, switch_codec_flag_t flags, const switch_codec_settings_t *codec_settings)
{
#ifdef AMRWB_PASSTHROUGH
	codec->flags |= SWITCH_CODEC_FLAG_PASSTHROUGH;
	if (codec->fmtp_in) {
		codec->fmtp_out = switch_core_strdup(codec->memory_pool, codec->fmtp_in);
	}

	return SWITCH_STATUS_SUCCESS;
#else
	struct amrwb_context *context = NULL;
	int encoding, decoding;
	int x, i, argc, fmtptmp_pos;
	char *argv[32];
	char fmtptmp[192];
	char *fmtp_dup = NULL;

	encoding = (flags & SWITCH_CODEC_FLAG_ENCODE);
	decoding = (flags & SWITCH_CODEC_FLAG_DECODE);

	if (!(encoding || decoding) || (!(context = switch_core_alloc(codec->memory_pool, sizeof(struct amrwb_context))))) {

		return SWITCH_STATUS_FALSE;
	} else {

		/* "mode" may mean two different things:
		 * "Octed Aligned" or "Bandwidth Efficient" encoding mode ,
		 * or the actual bitrate  which is set with FMTP param "mode-set". */
		/* https://tools.ietf.org/html/rfc4867 */

		/* set the default mode just in case there's no "mode-set" FMTP param */
		context->enc_mode = globals.default_bitrate;
		context->frames_per_packet = codec->implementation->microseconds_per_packet / 20000;
		if (context->frames_per_packet < 1) {
			context->frames_per_packet = 1;
		}
		amr_pl_dil_init(&context->dil);

		/* octet-align = 0  - per RFC - if there's no `octet-align` FMTP value then BE is employed */
		if (!globals.force_oa) {
			switch_clear_flag(context, AMRWB_OPT_OCTET_ALIGN);
		} else {
			switch_set_flag(context, AMRWB_OPT_OCTET_ALIGN);
		}

		if (codec->fmtp_in) {
			fmtp_dup = strdup(codec->fmtp_in);
			switch_assert(fmtp_dup);

			/* VoLTE offers carry many parameters: don't stop at the first few */
			argc = switch_separate_string(fmtp_dup, ';', argv, (sizeof(argv) / sizeof(argv[0])));

			for (x = 0; x < argc; x++) {
				char *data = argv[x];
				char *arg;

				while (*data && *data == ' ') {
					data++;
				}

				if ((arg = strchr(data, '='))) {
					*arg++ = '\0';
					if (!strcasecmp(data, "octet-align")) {
						if (atoi(arg)) {
							switch_set_flag(context, AMRWB_OPT_OCTET_ALIGN);
						}
					} else if (!strcasecmp(data, "mode-change-neighbor")) {
						if (atoi(arg)) {
							switch_set_flag(context, AMRWB_OPT_MODE_CHANGE_NEIGHBOR);
						}
					} else if (!strcasecmp(data, "crc")) {
						if (atoi(arg)) {
							switch_set_flag(context, AMRWB_OPT_CRC);
						}
					} else if (!strcasecmp(data, "robust-sorting")) {
						if (atoi(arg)) {
							switch_set_flag(context, AMRWB_OPT_ROBUST_SORTING);
						}
					} else if (!strcasecmp(data, "interleaving")) {
						/* the value is the maximum number of frame-blocks in an interleaving group */
						if ((context->interleaving = atoi(arg)) > 0) {
							switch_set_flag(context, AMRWB_OPT_INTERLEAVING);
						} else {
							context->interleaving = 0;
						}
					} else if (!strcasecmp(data, "mode-change-period")) {
						context->change_period = atoi(arg);
					} else if (!strcasecmp(data, "ptime")) {
						context->ptime = (switch_byte_t) atoi(arg);
					} else if (!strcasecmp(data, "channels")) {
						context->channels = (switch_byte_t) atoi(arg);
					} else if (!strcasecmp(data, "maxptime")) {
						context->max_ptime = (switch_byte_t) atoi(arg);
					} else if (!strcasecmp(data, "max-red")) {
						context->max_red = atoi(arg);
					} else if (!strcasecmp(data, "mode-set")) {
						int y, m_argc;
						char *m_argv[SWITCH_AMRWB_MAX_MODE + 1]; /* AMRWB has 9 modes */

						m_argc = switch_separate_string(arg, ',', m_argv, (sizeof(m_argv) / sizeof(m_argv[0])));

						for (y = 0; y < m_argc; y++) {
							int m = atoi(m_argv[y]);
							if (m >= 0 && m <= SWITCH_AMRWB_MAX_MODE) {
								context->enc_modes |= (1 << m);
								context->enc_mode = (switch_byte_t) m;
							} else {
								switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(codec->session), SWITCH_LOG_WARNING, "AMR-WB: ignoring invalid mode [%s] in mode-set\n", m_argv[y]);
							}
						}
					}
				}
			}

			free(fmtp_dup);
		}

		if (switch_test_flag(context, AMRWB_OPT_INTERLEAVING) && !switch_test_flag(context, AMRWB_OPT_OCTET_ALIGN)) {
			/* RFC 4867 3.8: interleaving only exists in octet-aligned mode */
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(codec->session), SWITCH_LOG_WARNING, "AMR-WB: interleaving requires octet-align=1, using octet-aligned mode\n");
			switch_set_flag(context, AMRWB_OPT_OCTET_ALIGN);
		}
		if (switch_test_flag(context, AMRWB_OPT_CRC) || switch_test_flag(context, AMRWB_OPT_ROBUST_SORTING)) {
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(codec->session), SWITCH_LOG_WARNING, "AMR-WB: crc / robust-sorting are not supported\n");
		}

		if (context->enc_modes && !globals.mode_set_overwrite) {
			/* choose the highest mode (bitrate) for high audio quality. */
			for (i = SWITCH_AMRWB_MAX_MODE; i > -1; i--) {
				if (context->enc_modes & (1 << i)) {
					context->enc_mode = (switch_byte_t) i;
					break;
				}
			}

			/* re-create mode-set */
			fmtptmp_pos = switch_snprintf(fmtptmp, sizeof(fmtptmp), "mode-set=");

			for (i = 0; i <= SWITCH_AMRWB_MAX_MODE; ++i) {
				if (context->enc_modes & (1 << i)) {
					fmtptmp_pos += switch_snprintf(fmtptmp + fmtptmp_pos, sizeof(fmtptmp) - fmtptmp_pos, fmtptmp_pos > strlen("mode-set=") ? ",%d" : "%d", i);
				}
			}

		} else {
			/* use default mode-set */
			fmtptmp_pos = switch_snprintf(fmtptmp, sizeof(fmtptmp), "mode-set=%d", context->enc_mode);
		}

		if (globals.adjust_bitrate) {
			switch_set_flag(codec, SWITCH_CODEC_FLAG_HAS_ADJ_BITRATE);
		}

		if (!globals.volte) {
			fmtptmp_pos += switch_snprintf(fmtptmp + fmtptmp_pos, sizeof(fmtptmp) - fmtptmp_pos, ";octet-align=%d",
					switch_test_flag(context, AMRWB_OPT_OCTET_ALIGN) ? 1 : 0);
		} else {
			fmtptmp_pos += switch_snprintf(fmtptmp + fmtptmp_pos, sizeof(fmtptmp) - fmtptmp_pos, ";octet-align=%d;max-red=0;mode-change-capability=2",
					switch_test_flag(context, AMRWB_OPT_OCTET_ALIGN) ? 1 : 0);
		}
		if (context->interleaving) {
			/* declarative parameter: answer with the value we were given */
			switch_snprintf(fmtptmp + fmtptmp_pos, sizeof(fmtptmp) - fmtptmp_pos, ";interleaving=%d", context->interleaving);
		}

		codec->fmtp_out = switch_core_strdup(codec->memory_pool, fmtptmp);

		context->encoder_state = NULL;
		context->decoder_state = NULL;

		if (encoding) {
			context->encoder_state = E_IF_init();
		}

		if (decoding) {
			context->decoder_state = D_IF_init();
		}

		codec->private_info = context;

		return SWITCH_STATUS_SUCCESS;
	}
#endif
}

static switch_status_t switch_amrwb_destroy(switch_codec_t *codec)
{
#ifndef AMRWB_PASSTHROUGH
	struct amrwb_context *context = codec->private_info;

	if (context->encoder_state) {
		E_IF_exit(context->encoder_state);
}
	if (context->decoder_state) {
		D_IF_exit(context->decoder_state);
	}
	codec->private_info = NULL;
#endif
	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t switch_amrwb_encode(switch_codec_t *codec,
										   switch_codec_t *other_codec,
										   void *decoded_data,
										   uint32_t decoded_data_len,
										   uint32_t decoded_rate, void *encoded_data, uint32_t *encoded_data_len, uint32_t *encoded_rate,
										   unsigned int *flag)
{
#ifdef AMRWB_PASSTHROUGH
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "This codec is only usable in passthrough mode!\n");
	return SWITCH_STATUS_FALSE;
#else
	struct amrwb_context *context = codec->private_info;
	amr_pl_packet_t pkt;
	uint8_t storage[SWITCH_AMRWB_STORAGE_MAX + 8];
	int i, n, len, nframes;

	if (!context) {
		return SWITCH_STATUS_FALSE;
	}

	/* one 20 ms frame per 320 samples; a 40/60/80 ms implementation packs several frames per packet */
	nframes = (int)(decoded_data_len / SWITCH_AMRWB_FRAME_BYTES);
	if (nframes < 1) {
		return SWITCH_STATUS_FALSE;
	}
	if (nframes > AMR_PL_MAX_FRAMES) {
		nframes = AMR_PL_MAX_FRAMES;
	}

	memset(&pkt, 0, sizeof(pkt));
	pkt.cmr = 15; /* no mode request */
	pkt.nframes = nframes;
	if (context->interleaving) {
		/* The ILL/ILP header must be present once interleaving is negotiated. We send groups of one
		   packet (ILL=0): FreeSWITCH stamps RTP packets in transmission order, and an interleaved
		   packet must carry the timestamp of its first frame-block (RFC 4867 4.1), which we can't set. */
		pkt.interleaved = 1;
	}

	for (i = 0; i < nframes; i++) {
		n = E_IF_encode(context->encoder_state, context->enc_mode, (int16_t *) decoded_data + i * 320, storage, 0);
		if (n < 1 || amr_pl_frame_from_storage(&amrwb_desc, storage, n, &pkt.frames[i]) < 0) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "AMRWB encoder: E_IF_encode() ERROR!\n");
			return SWITCH_STATUS_FALSE;
		}
	}

	len = amr_pl_build(&amrwb_desc, &pkt, switch_test_flag(context, AMRWB_OPT_OCTET_ALIGN), (uint8_t *) encoded_data, SWITCH_RECOMMENDED_BUFFER_SIZE);
	if (len < 0) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "AMRWB encoder: cannot build the RTP payload\n");
		return SWITCH_STATUS_FALSE;
	}
	*encoded_data_len = (uint32_t) len;

	if (globals.debug) {
		switch_amrwb_info(codec, &pkt, len, switch_test_flag(context, AMRWB_OPT_OCTET_ALIGN), "AMRWB encoder");
	}

	return SWITCH_STATUS_SUCCESS;
#endif
}

#ifndef AMRWB_PASSTHROUGH
/* decode one frame into 320 samples; lost = no data for this frame (packet loss) */
static void amrwb_decode_frame(struct amrwb_context *context, const amr_pl_frame_t *f, int lost, int16_t *out)
{
	uint8_t storage[SWITCH_AMRWB_STORAGE_MAX + 8];
	amr_pl_frame_t fr = *f;
	int bfi = 0;

	if (lost) {
		/* frame lost on the network: SPEECH_LOST lets the decoder conceal it */
		memset(&fr, 0, sizeof(fr));
		fr.ft = SWITCH_AMRWB_SPEECH_LOST;
		fr.q = 1;
	} else if (!fr.q) {
		/* Q=0: the frame is damaged */
		bfi = 1;
	}
	amr_pl_frame_to_storage(&amrwb_desc, &fr, storage);
	D_IF_decode(context->decoder_state, storage, out, bfi);
}
#endif

static switch_status_t switch_amrwb_decode(switch_codec_t *codec,
										   switch_codec_t *other_codec,
										   void *encoded_data,
										   uint32_t encoded_data_len,
										   uint32_t encoded_rate, void *decoded_data, uint32_t *decoded_data_len, uint32_t *decoded_rate,
										   unsigned int *flag)
{
#ifdef AMRWB_PASSTHROUGH
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "This codec is only usable in passthrough mode!\n");
	return SWITCH_STATUS_FALSE;
#else
	struct amrwb_context *context = codec->private_info;
	amr_pl_packet_t pkt;
	uint32_t cap;
	int i, out_frames, octet_aligned;

	if (!context) {
		return SWITCH_STATUS_FALSE;
	}

	octet_aligned = switch_test_flag(context, AMRWB_OPT_OCTET_ALIGN) ? 1 : 0;
	if (amr_pl_parse(&amrwb_desc, (const uint8_t *) encoded_data, (int) encoded_data_len, octet_aligned, context->interleaving > 0, &pkt) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(codec->session), SWITCH_LOG_ERROR, "AMRWB decoder (%s): malformed payload (%u bytes)\n",
						  octet_aligned ? "OA" : "BE", encoded_data_len);
		return SWITCH_STATUS_FALSE;
	}

	if (globals.debug) {
		switch_amrwb_info(codec, &pkt, (int) encoded_data_len, octet_aligned, "AMRWB decoder");
	}

	/* decoded_data_len comes in as the size of the output buffer */
	cap = *decoded_data_len ? *decoded_data_len : codec->implementation->decoded_bytes_per_packet;
	out_frames = pkt.nframes;
	if ((uint32_t) out_frames * SWITCH_AMRWB_FRAME_BYTES > cap) {
		out_frames = (int)(cap / SWITCH_AMRWB_FRAME_BYTES);
	}

	if (pkt.interleaved) {
		/* frames come out in time order, one interleaving group late; as many per packet as it carried */
		amr_pl_dil_push(&context->dil, &pkt);
		for (i = 0; i < out_frames; i++) {
			amr_pl_frame_t f;
			int lost = 0;

			if (!amr_pl_dil_pop(&context->dil, &f, &lost)) {
				/* still filling the first group */
				memset(&f, 0, sizeof(f));
				f.ft = AMR_PL_FT_NO_DATA;
				f.q = 1;
			}
			amrwb_decode_frame(context, &f, lost, (int16_t *) decoded_data + i * 320);
		}
	} else {
		for (i = 0; i < out_frames; i++) {
			amrwb_decode_frame(context, &pkt.frames[i], 0, (int16_t *) decoded_data + i * 320);
		}
	}

	*decoded_data_len = (uint32_t) out_frames * SWITCH_AMRWB_FRAME_BYTES;

	return SWITCH_STATUS_SUCCESS;
#endif
}

#ifndef AMRWB_PASSTHROUGH
static switch_status_t switch_amrwb_control(switch_codec_t *codec,
										   switch_codec_control_command_t cmd,
										   switch_codec_control_type_t ctype,
										   void *cmd_data,
										   switch_codec_control_type_t atype,
										   void *cmd_arg,
										   switch_codec_control_type_t *rtype,
										   void **ret_data)
{
	struct amrwb_context *context = codec->private_info;

	switch(cmd) {
	case SCC_DEBUG:
		{
			int32_t level = *((uint32_t *) cmd_data);
			context->debug = level;
		}
		break;
	case SCC_AUDIO_ADJUST_BITRATE:
		{
			const char *cmd = (const char *)cmd_data;
			int cur = context->enc_mode;
			const char *how = "minimum";

			/* stay within 0..8 and within the negotiated mode-set */
			if (!strcasecmp(cmd, "increase")) {
				context->enc_mode = amrwb_allowed_mode(context, cur + 2, cur); /*this is the mode, not the actual bitrate*/
				how = "increase";
			} else if (!strcasecmp(cmd, "decrease")) {
				context->enc_mode = amrwb_allowed_mode(context, cur - 2, cur);
				how = "decrease";
			} else if (!strcasecmp(cmd, "default")) {
				context->enc_mode = amrwb_allowed_mode(context, globals.default_bitrate, cur);
				how = "default";
			} else {
				/*minimum bitrate (AMRWB mode)*/
				context->enc_mode = amrwb_allowed_mode(context, 0, cur);
			}
			if (globals.debug || context->debug) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG,
						"AMRWB encoder: Adjusting mode to %d (%s)\n", context->enc_mode, how);
			}
		}
		break;
	default:
		break;
	}

	return SWITCH_STATUS_SUCCESS;
}
#endif

static char *generate_fmtp(switch_memory_pool_t *pool , int octet_align)
{
	char buf[256] = { 0 };

	snprintf(buf + strlen(buf), sizeof(buf) - strlen(buf), "octet-align=%d; ", octet_align);

#ifndef AMRWB_PASSTHROUGH
	snprintf(buf + strlen(buf), sizeof(buf) - strlen(buf), "mode-set=%d; ", globals.default_bitrate);

	if (globals.volte) {
		snprintf(buf + strlen(buf), sizeof(buf) - strlen(buf), "max-red=0; mode-change-capability=2; ");
	}
#endif

	if (end_of(buf) == ' ') {
		*(end_of_p(buf) - 1) = '\0';
	}

	return switch_core_strdup(pool, buf);
}

#ifndef AMRWB_PASSTHROUGH

#define AMRWB_DEBUG_SYNTAX "<on|off>"
SWITCH_STANDARD_API(mod_amrwb_debug)
{
	if (zstr(cmd)) {
		stream->write_function(stream, "-USAGE: %s\n", AMRWB_DEBUG_SYNTAX);
	} else {
		if (!strcasecmp(cmd, "on")) {
			globals.debug = 1;
			stream->write_function(stream, "AMRWB Debug: on\n");
		} else if (!strcasecmp(cmd, "off")) {
				globals.debug = 0;
				stream->write_function(stream, "AMRWB Debug: off\n");
		} else {
				stream->write_function(stream, "-USAGE: %s\n", AMRWB_DEBUG_SYNTAX);
		}
	}
	return SWITCH_STATUS_SUCCESS;
}
#endif

/* Registration */
SWITCH_MODULE_LOAD_FUNCTION(mod_amrwb_load)
{
	switch_codec_interface_t *codec_interface;
	char *default_fmtp_oa = NULL;
	char *default_fmtp_be = NULL;

#ifndef AMRWB_PASSTHROUGH
	switch_api_interface_t *commands_api_interface;
	char *cf = "amrwb.conf";
	switch_xml_t cfg, xml, settings, param;

	memset(&globals, 0, sizeof(globals));
	globals.default_bitrate = SWITCH_AMRWB_DEFAULT_BITRATE;

	if ((xml = switch_xml_open_cfg(cf, &cfg, NULL))) {
		if ((settings = switch_xml_child(cfg, "settings"))) {
			for (param = switch_xml_child(settings, "param"); param; param = param->next) {
				char *var = (char *) switch_xml_attr_soft(param, "name");
				char *val = (char *) switch_xml_attr_soft(param, "value");
				if (!strcasecmp(var, "default-bitrate")) {
					globals.default_bitrate = (switch_byte_t) atoi(val);
				}
				if (!strcasecmp(var, "volte")) {
					/* ETSI TS 126 236 compatibility:  http://www.etsi.org/deliver/etsi_ts/126200_126299/126236/10.00.00_60/ts_126236v100000p.pdf */
					globals.volte = (switch_byte_t) atoi(val);
				}
				if (!strcasecmp(var, "adjust-bitrate")) {
					globals.adjust_bitrate = (switch_byte_t) atoi(val);
				}
				if (!strcasecmp(var, "force-oa")) {
					globals.force_oa = (switch_byte_t) atoi(val);
				}
				if (!strcasecmp(var, "mode-set-overwrite")) {
					globals.mode_set_overwrite = (switch_byte_t) atoi(val);
				}
			}
		}
	}

	if (xml) {
		switch_xml_free(xml);
	}
#endif

	/* connect my internal structure to the blank pointer passed to me */
	*module_interface = switch_loadable_module_create_module_interface(pool, modname);

#ifndef AMRWB_PASSTHROUGH
	SWITCH_ADD_API(commands_api_interface, "amrwb_debug", "Set AMR-WB Debug", mod_amrwb_debug, AMRWB_DEBUG_SYNTAX);

	switch_console_set_complete("add amrwb_debug on");
	switch_console_set_complete("add amrwb_debug off");
#endif

	{
		/* 20 ms frames, 1 to 4 per packet: ptime 20, 40, 60 or 80 */
		int ms;
#ifndef AMRWB_PASSTHROUGH
		switch_codec_implementation_t *impl;
#endif

		default_fmtp_oa = generate_fmtp(pool, 1);
		default_fmtp_be = generate_fmtp(pool, 0);

		SWITCH_ADD_CODEC(codec_interface, "AMR-WB / Octet Aligned");
		for (ms = 20; ms <= 80; ms += 20) {
			switch_core_codec_add_implementation(pool, codec_interface,
												 SWITCH_CODEC_TYPE_AUDIO, 100, "AMR-WB", default_fmtp_oa,
												 16000, 16000, 23850, ms * 1000, 16 * ms, 32 * ms, 0, 1, 1,
												 switch_amrwb_init, switch_amrwb_encode, switch_amrwb_decode, switch_amrwb_destroy);
		}
#ifndef AMRWB_PASSTHROUGH
		for (impl = codec_interface->implementations; impl; impl = impl->next) {
			impl->codec_control = switch_amrwb_control;
		}
#endif

		SWITCH_ADD_CODEC(codec_interface, "AMR-WB / Bandwidth Efficient");
		for (ms = 20; ms <= 80; ms += 20) {
			switch_core_codec_add_implementation(pool, codec_interface,
												 SWITCH_CODEC_TYPE_AUDIO, 110, "AMR-WB", default_fmtp_be,
												 16000, 16000, 23850, ms * 1000, 16 * ms, 32 * ms, 0, 1, 1,
												 switch_amrwb_init, switch_amrwb_encode, switch_amrwb_decode, switch_amrwb_destroy);
		}
#ifndef AMRWB_PASSTHROUGH
		for (impl = codec_interface->implementations; impl; impl = impl->next) {
			impl->codec_control = switch_amrwb_control;
		}
#endif
	}

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
