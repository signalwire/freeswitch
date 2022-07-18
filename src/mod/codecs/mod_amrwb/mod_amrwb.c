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
 *
 * XML Parameters
 *
 * default-bitrate
 *		Bitrate mode that will be used if mode-set-overwrite and mode-set-overwrite-with-default-bitrate are set).
 * volte
 *		If set, configures codec for use on cellular networks.
 * adjust-bitrate
 *		Vary bitrate according to feedback from RTCP.
 * force-oa
 *		Configure codec in octet aligned mode.
 * force-be
 *		Configure codec in bandwidth efficient mode.
 * mode-set-overwrite
 *		When answering a call, use codec bitrate modes from mode-set param, instead of mirroring the OFFER.
 * mode-set-overwrite-with-default-bitrate
 *		If mode-set-overwrite is on, then use default-bitrate mode instead of mode-set.
 * invite-prefer-oa
 *		When answering a call, if AMR-WB is offered in 2 modes (octet aligned and bandwidth efficient), select octet aligned.
 * invite-prefer-be
 *		When answering a call, if AMR-WB is offered in 2 modes (octet aligned and bandwidth efficient), select bandwidth efficient.
 * mode-set
 *		Provides bitrate modes to be used with mode-set-overwrite (if mode-set-overwrite-with-default-bitrate is off).
 * debug
 *		If on, print extra codec info (CMR, ToC, last frame flag) at the FS's DEBUG level.
 * silence-supp-off
 *		If true, then SDP has 'silenceSupp:off - - - -' to turn silence suppression off (no CNG).
 * fmtp-extra
 *		Append any extra info to fmtp entry for AMR-WB.
 *
 */

#include "switch.h"

SWITCH_MODULE_LOAD_FUNCTION(mod_amrwb_load);
SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_amrwb_unload);
SWITCH_MODULE_DEFINITION(mod_amrwb, mod_amrwb_load, mod_amrwb_unload, NULL);

switch_mutex_t *global_lock;
int global_debug;
char AMRWB_CONFIGURATION[2000];

#ifndef AMRWB_PASSTHROUGH
#include "opencore-amrwb/dec_if.h" /*AMR-WB decoder API*/
#include "vo-amrwbenc/enc_if.h" /*AMR-WB encoder API*/

#include "bitshift.h"
#include "amrwb_be.h"

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
	switch_byte_t enc_modes;
	switch_byte_t enc_mode;
	uint32_t change_period;
	switch_byte_t max_ptime;
	switch_byte_t ptime;
	switch_byte_t channels;
	switch_byte_t flags;
	int max_red;
	int debug;
};

#define SWITCH_AMRWB_DEFAULT_BITRATE AMRWB_BITRATE_24K

static struct {
	switch_byte_t default_bitrate;
	switch_byte_t volte;
	switch_byte_t adjust_bitrate;
	switch_byte_t force_oa; /*force OA when originating*/
	switch_byte_t force_be;
	switch_byte_t mode_set_overwrite;
	switch_byte_t mode_set_overwrite_with_default_bitrate;
	switch_byte_t invite_prefer_oa;
	switch_byte_t invite_prefer_be;
	struct amrwb_context context;
	char *fmtp_extra;
	switch_byte_t silence_supp_off;
} globals;

const int switch_amrwb_frame_sizes[] = {17, 23, 32, 36, 40, 46, 50, 58, 60, 5, 0, 0, 0, 0, 1, 1};

#define SWITCH_AMRWB_OUT_MAX_SIZE 61
#define SWITCH_AMRWB_MODES 10 /* Silence Indicator (SID) included */

#define invalid_frame_type (index > SWITCH_AMRWB_MODES && index != 0xe && index != 0xf) /* include SPEECH_LOST and NO_DATA*/

static switch_bool_t switch_amrwb_unpack_oa(unsigned char *buf, uint8_t *tmp, int encoded_data_len)
{
	uint8_t *tocs;
	int index;
	int framesz;

	buf++;/* CMR skip */
	tocs = buf;
	index = ((tocs[0]>>3) & 0xf);
	buf++; /* point to voice payload */

	if (invalid_frame_type) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "AMRWB decoder (OA): Invalid TOC: 0x%x", index);
		return SWITCH_FALSE;
	}
	framesz = switch_amrwb_frame_sizes[index];
	if (framesz > encoded_data_len - 1) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "AMRWB decoder (OA): Invalid frame size: %d\n", framesz);
		return SWITCH_FALSE;
	}
	tmp[0] = tocs[0];
	memcpy(&tmp[1], buf, framesz);

	return SWITCH_TRUE;
}

static switch_bool_t switch_amrwb_pack_oa(unsigned char *shift_buf, int n)
{
/* Interleaving code here */
	return SWITCH_TRUE;
}

static switch_bool_t switch_amrwb_info(switch_codec_t *codec, unsigned char *encoded_buf, int encoded_data_len, int payload_format, char *print_text)
{
	uint8_t *tocs;
	int framesz, index, not_last_frame, q, ft;
	uint8_t shift_tocs[2] = {0x00, 0x00};

	if (!encoded_buf) {
		return SWITCH_FALSE;
	}

	/* payload format can be OA (octed-aligned) or BE (bandwidth efficient)*/
	if (payload_format) {
		/* OA */
		encoded_buf++; /* CMR skip */
		tocs = encoded_buf;
		index = (tocs[0] >> 3) & 0x0f;
		if (invalid_frame_type) {
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(codec->session), SWITCH_LOG_ERROR, "AMRWB decoder (OA): Invalid TOC 0x%x\n", index);
			return SWITCH_FALSE;
		}
		framesz = switch_amrwb_frame_sizes[index];
		not_last_frame = (tocs[0] >> 7) & 1;
		q = (tocs[0] >> 2) & 1;
		ft = tocs[0] >> 3;
		ft &= ~(1 << 5); /* Frame Type */
	} else {
		/* BE */
		memcpy(shift_tocs, encoded_buf, 2);
		/* shift for BE */
		switch_amr_array_lshift(4, shift_tocs, 2);
		not_last_frame = (shift_tocs[0] >> 7) & 1;
		q = (shift_tocs[0] >> 2) & 1;
		ft = shift_tocs[0] >> 3;
		ft &= ~(1 << 5); /* Frame Type */
		index = (shift_tocs[0] >> 3) & 0x0f;
		if (invalid_frame_type) {
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(codec->session), SWITCH_LOG_ERROR, "AMRWB decoder (BE): Invalid TOC 0x%x\n", index);
			return SWITCH_FALSE;
		}
		framesz = switch_amrwb_frame_sizes[index];
	}

	switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(codec->session), SWITCH_LOG_DEBUG, 
			"%s (%s): FT: [0x%x] Q: [0x%x] Frame flag: [%d]\n",
			print_text, payload_format ? "OA":"BE", ft, q, not_last_frame);
	switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(codec->session), SWITCH_LOG_DEBUG, 
			"%s (%s): AMRWB encoded voice payload sz: [%d] : | encoded_data_len: [%d]\n", 
			print_text, payload_format ? "OA":"BE", framesz, encoded_data_len);

	return SWITCH_TRUE;
}
#endif

static switch_status_t amrwb_parse_fmtp_cb(const char *fmtp, switch_codec_fmtp_t *codec_fmtp)
{
	/* Must return IGNORE for FS to skip this codec */
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Considering fmtp\n");

	if (!zstr(fmtp)) {
		int x, argc;
		char *argv[10];
		char *fmtp_dup = strdup(fmtp);

		/* If there is no octet-align param on fmtp then default is 0 (bandwidth efficient). */
		int oa = 0;

		if (!fmtp_dup) {
			return SWITCH_STATUS_FALSE;
		}

		argc = switch_separate_string(fmtp_dup, ';', argv, (sizeof(argv) / sizeof(argv[0])));
		for (x = 0; x < argc; x++) {
			char *data = argv[x];
			char *arg;
			while (*data == ' ') {
				data++;
			}

			if ((arg = strchr(data, '='))) {
				*arg++ = '\0';

				if (!strcasecmp(data, "octet-align")) {
					oa = switch_true(arg);
				}
			}
		}
		free(fmtp_dup);

		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "AMR-WB fmtp mode: %s\n", oa ? "octet aligned" : "bandwidth efficient");
		if ((oa == 0 && globals.invite_prefer_oa) || (oa == 1 && globals.invite_prefer_be)) {
			return SWITCH_STATUS_IGNORE;
		}
	}

	/* Must return FALSE for FS to continue as if callback was not called */
	return SWITCH_STATUS_FALSE;
}

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
	char *argv[10];
	char fmtptmp[128];
	switch_core_session_t *session = codec->session;

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

		/* octet-align = 0  - per RFC - if there's no `octet-align` FMTP value then BE is employed */
		switch_clear_flag(context, AMRWB_OPT_OCTET_ALIGN);

		if (codec->fmtp_in) {
			argc = switch_separate_string(codec->fmtp_in, ';', argv, (sizeof(argv) / sizeof(argv[0])));
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
						if (atoi(arg)) {
							switch_set_flag(context, AMRWB_OPT_INTERLEAVING);
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
						char *m_argv[SWITCH_AMRWB_MODES-1]; /* AMRWB has 9 modes */
						m_argc = switch_separate_string(arg, ',', m_argv, (sizeof(m_argv) / sizeof(m_argv[0])));
						for (y = 0; y < m_argc; y++) {
							context->enc_modes |= (1 << atoi(m_argv[y]));
							context->enc_mode = atoi(m_argv[y]);
						}
					}
				}
			}
		}

		if (globals.force_oa) {
			switch_set_flag(context, AMRWB_OPT_OCTET_ALIGN);
		}

		if (globals.force_be) {
			switch_clear_flag(context, AMRWB_OPT_OCTET_ALIGN);
		}

		if (context->enc_modes && !globals.mode_set_overwrite) {

			/* If inbound fmtp has mode-set and XML overwrite is not set, then mirror these mode-set */

			/* choose the highest mode (bitrate) for high audio quality. */
			for (i = SWITCH_AMRWB_MODES-2; i > -1; i--) {
				if (context->enc_modes & (1 << i)) {
					context->enc_mode = (switch_byte_t) i;
					break;
				}
			}

			/* re-create mode-set */
			fmtptmp_pos = switch_snprintf(fmtptmp, sizeof(fmtptmp), "mode-set=");
			for (i = 0; SWITCH_AMRWB_MODES-1 > i; ++i) {
				if (context->enc_modes & (1 << i)) {
					fmtptmp_pos += switch_snprintf(fmtptmp + fmtptmp_pos, sizeof(fmtptmp) - fmtptmp_pos, fmtptmp_pos > strlen("mode-set=") ? ",%d" : "%d", i);
				}
			}

		} else {

			/* It is inbound fmtp with no mode-set or outbound */

			if (globals.mode_set_overwrite_with_default_bitrate) {
				fmtptmp_pos = switch_snprintf(fmtptmp, sizeof(fmtptmp), "mode-set=%d", globals.default_bitrate);
			} else {
				char modes[100] = { 0 };
				int i = 0, j = 0;

				for (i = 0; SWITCH_AMRWB_MODES-1 > i; ++i) {
					if (globals.context.enc_modes & (1 << i)) {
						j++;
						snprintf(modes + strlen(modes), sizeof(modes) - strlen(modes), j > 1 ? ",%d" : "%d", i);
					}
				}

				fmtptmp_pos = switch_snprintf(fmtptmp, sizeof(fmtptmp), "mode-set=%s", modes);
			}
		}

		if (globals.adjust_bitrate) {
			switch_set_flag(codec, SWITCH_CODEC_FLAG_HAS_ADJ_BITRATE);
		}

		if (!globals.volte) {
			fmtptmp_pos += switch_snprintf(fmtptmp + fmtptmp_pos, sizeof(fmtptmp) - fmtptmp_pos, "; octet-align=%d",
					switch_test_flag(context, AMRWB_OPT_OCTET_ALIGN) ? 1 : 0);
		} else {
			fmtptmp_pos += switch_snprintf(fmtptmp + fmtptmp_pos, sizeof(fmtptmp) - fmtptmp_pos, "; octet-align=%d; max-red=0; mode-change-capability=2",
					switch_test_flag(context, AMRWB_OPT_OCTET_ALIGN) ? 1 : 0);
		}

		if (!zstr(globals.fmtp_extra)) {
			fmtptmp_pos += switch_snprintf(fmtptmp + fmtptmp_pos, sizeof(fmtptmp) - fmtptmp_pos, "; %s", globals.fmtp_extra);
		}

		if (globals.silence_supp_off) {
			switch_channel_t *channel = NULL;
			if (session) {
				channel = switch_core_session_get_channel(session);
				switch_assert(channel);
				switch_channel_set_variable(channel, "suppress_cng", "true");
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Turning CNG off (silence suppression off, suppress_cng=true) due to silence-supp-off=true\n");
			} else {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Cannot turn silence suppression off - session missing\n");
			}
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
	int n;
	unsigned char *shift_buf = encoded_data;

	if (!context) {
		return SWITCH_STATUS_FALSE;
	}

	n = E_IF_encode(context->encoder_state, context->enc_mode, (int16_t *) decoded_data, (switch_byte_t *) encoded_data + 1, 0);
	if (n < 0) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "AMRWB encoder: E_IF_encode() ERROR!\n");
		return SWITCH_STATUS_FALSE;
	}

	/* set CMR + TOC (F + 3 bits of FT), 1111 = CMR: No mode request */
	*(switch_byte_t *) encoded_data = 0xf0;
	*encoded_data_len  = n;

	if (switch_test_flag(context, AMRWB_OPT_OCTET_ALIGN)) {
		switch_amrwb_pack_oa(shift_buf, n);  /* the payload is OA as it
												comes out of the encoding function */
		*encoded_data_len = n + 1;
	} else {
		switch_amrwb_pack_be(shift_buf, n);
	}

	switch_mutex_lock(global_lock);
	if (global_debug) {
		switch_amrwb_info(codec, shift_buf, *encoded_data_len, switch_test_flag(context, AMRWB_OPT_OCTET_ALIGN) ? 1 : 0, "AMRWB encoder");
	}
	switch_mutex_unlock(global_lock);

	return SWITCH_STATUS_SUCCESS;

#endif
}

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
	unsigned char *buf = encoded_data;
	uint8_t tmp[SWITCH_AMRWB_OUT_MAX_SIZE];

	if (!context) {
		return SWITCH_STATUS_FALSE;
	}

	switch_mutex_lock(global_lock);
	if (global_debug) {
		switch_amrwb_info(codec, buf, encoded_data_len, switch_test_flag(context, AMRWB_OPT_OCTET_ALIGN) ? 1 : 0, "AMRWB decoder");
	}
	switch_mutex_unlock(global_lock);

	if (switch_test_flag(context, AMRWB_OPT_OCTET_ALIGN)) {
		/* Octed Aligned */
		if (!switch_amrwb_unpack_oa(buf, tmp, encoded_data_len)) {
			return SWITCH_STATUS_FALSE;
		}
	} else {
		/* Bandwidth Efficient */
		if (!switch_amrwb_unpack_be(buf, tmp, encoded_data_len)) {
			return SWITCH_STATUS_FALSE;
		}
	}

	D_IF_decode(context->decoder_state, tmp, (int16_t *) decoded_data, 0);

	*decoded_data_len = codec->implementation->decoded_bytes_per_packet;

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
	int debug = 0;

	switch_mutex_lock(global_lock);
	debug = global_debug;
	switch_mutex_unlock(global_lock);

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

			if (!strcasecmp(cmd, "increase")) {
				if (context->enc_mode < SWITCH_AMRWB_MODES - 1) {
					int mode_step = 2; /*this is the mode, not the actual bitrate*/
					context->enc_mode = context->enc_mode + mode_step;
					if (debug || context->debug) {
						switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG,
								"AMRWB encoder: Adjusting mode to %d (increase)\n", context->enc_mode);
					}
				}
			} else if (!strcasecmp(cmd, "decrease")) {
				if (context->enc_mode > 0) {
					int mode_step = 2; /*this is the mode, not the actual bitrate*/
					context->enc_mode = context->enc_mode - mode_step;
					if (debug || context->debug) {
						switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG,
								"AMRWB encoder: Adjusting mode to %d (decrease)\n", context->enc_mode);
					}
				}
			} else if (!strcasecmp(cmd, "default")) {
					context->enc_mode = globals.default_bitrate;
					if (debug || context->debug) {
						switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG,
								"AMRWB encoder: Adjusting mode to %d (default)\n", context->enc_mode);
					}
			} else {
				/*minimum bitrate (AMRWB mode)*/
				context->enc_mode = 0;
				if (debug || context->debug) {
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG,
							"AMRWB encoder: Adjusting mode to %d (minimum)\n", context->enc_mode);
				}
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
#ifndef AMRWB_PASSTHROUGH
	int i = 0, j =0;
#endif

	snprintf(buf + strlen(buf), sizeof(buf) - strlen(buf), "octet-align=%d; ", octet_align);

#ifndef AMRWB_PASSTHROUGH
	// ENGDESK-15706
	if (globals.context.enc_modes && !globals.mode_set_overwrite) {
			for (i = 0; SWITCH_AMRWB_MODES-1 > i; ++i) {
				if (globals.context.enc_modes & (1 << i)) {
					j++;
					snprintf(buf + strlen(buf), sizeof(buf) - strlen(buf), j > 1 ? ",%d" : "mode-set=%d", i);
				}
			}
	} else {
		snprintf(buf + strlen(buf), sizeof(buf) - strlen(buf), "mode-set=%d", globals.default_bitrate);
	}
	snprintf(buf + strlen(buf), sizeof(buf) - strlen(buf), "; ");

	if (globals.volte) {
		snprintf(buf + strlen(buf), sizeof(buf) - strlen(buf), "max-red=0; mode-change-capability=2; ");
	}
#endif

	if (!zstr(globals.fmtp_extra)) {
		snprintf(buf + strlen(buf), sizeof(buf) - strlen(buf), "%s", globals.fmtp_extra);
	}

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
			switch_mutex_lock(global_lock);
			global_debug = 1;
			switch_mutex_unlock(global_lock);
			stream->write_function(stream, "AMRWB Debug: on\n");
		} else if (!strcasecmp(cmd, "off")) {
			switch_mutex_lock(global_lock);
			global_debug = 0;
			switch_mutex_unlock(global_lock);
			stream->write_function(stream, "AMRWB Debug: off\n");
		} else {
			stream->write_function(stream, "-USAGE: %s\n", AMRWB_DEBUG_SYNTAX);
		}
	}
	return SWITCH_STATUS_SUCCESS;
}
#endif

static void mod_amrwb_configuration_snprintf(void) {
	char modes[100] = { 0 };
	int i = 0, j = 0;
	int debug = 0;

	snprintf(modes + strlen(modes), sizeof(modes) - strlen(modes), "[");
	for (i = 0; SWITCH_AMRWB_MODES-1 > i; ++i) {
		if (globals.context.enc_modes & (1 << i)) {
			j++;
			snprintf(modes + strlen(modes), sizeof(modes) - strlen(modes), j > 1 ? ",%d" : "%d", i);
		}
	}
	snprintf(modes + strlen(modes), sizeof(modes) - strlen(modes), "]");

	switch_mutex_lock(global_lock);
	debug = global_debug;
	switch_mutex_unlock(global_lock);

	snprintf(AMRWB_CONFIGURATION, sizeof(AMRWB_CONFIGURATION),
			"modes: %s, "
			"mode-set-overwrite: %d, "
			"mode-set-overwrite-with-default-bitrate: %d, "
			"default-bitrate: %d, "
			"volte: %d, "
			"adjust-bitrate: %d, "
			"force-oa: %d, "
			"force-be: %d, "
			"invite-prefer-oa: %d, "
			"invite-prefer-be: %d, "
			"fmtp-extra: [%s], "
			"debug: %d, "
			"silence-supp-off: %d\n",
			modes,
			globals.mode_set_overwrite,
			globals.mode_set_overwrite_with_default_bitrate,
			globals.default_bitrate,
			globals.volte,
			globals.adjust_bitrate,
			globals.force_oa,
			globals.force_be,
			globals.invite_prefer_oa,
			globals.invite_prefer_be,
			!zstr(globals.fmtp_extra) ? globals.fmtp_extra : "",
			debug,
			globals.silence_supp_off
	);
}

#define AMRWB_SHOW_SYNTAX ""
SWITCH_STANDARD_API(mod_amrwb_show)
{
	if (stream && stream->write_function) {
		mod_amrwb_configuration_snprintf();
		stream->write_function(stream, AMRWB_CONFIGURATION);
	}
	return SWITCH_STATUS_SUCCESS;
}

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
	globals.mode_set_overwrite_with_default_bitrate = 1;

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
				if (!strcasecmp(var, "force-be")) {
					globals.force_be = (switch_byte_t) atoi(val);
				}
				if (!strcasecmp(var, "mode-set-overwrite")) {
					globals.mode_set_overwrite = (switch_byte_t) atoi(val);
				}
				if (!strcasecmp(var, "mode-set-overwrite-with-default-bitrate")) {
					globals.mode_set_overwrite_with_default_bitrate = (switch_byte_t) atoi(val);
				}
				if (!strcasecmp(var, "invite-prefer-oa")) {
					globals.invite_prefer_oa = (switch_byte_t) atoi(val);
				}
				if (!strcasecmp(var, "invite-prefer-be")) {
					globals.invite_prefer_be = (switch_byte_t) atoi(val);
				}
				if (!strcasecmp(var, "mode-set")) {
					int y, m_argc;
					char *m_argv[SWITCH_AMRWB_MODES-1]; /* AMRWB has 9 modes */
					m_argc = switch_separate_string(val, ',', m_argv, (sizeof(m_argv) / sizeof(m_argv[0])));
					for (y = 0; y < m_argc; y++) {
						globals.context.enc_modes |= (1 << atoi(m_argv[y]));
						globals.context.enc_mode = atoi(m_argv[y]);
					}
				}
				if (!strcasecmp(var, "debug")) {
					global_debug = (switch_byte_t) atoi(val);
				}
				if (!strcasecmp(var, "fmtp-extra")) {
					globals.fmtp_extra = switch_core_strdup(pool, val);
					switch_assert(globals.fmtp_extra);
				}
				if (!strcasecmp(var, "silence-supp-off")) {
					globals.silence_supp_off = (switch_byte_t) atoi(val);
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
	SWITCH_ADD_API(commands_api_interface, "amrwb_show", "Show AMR-WB configuration", mod_amrwb_show, AMRWB_SHOW_SYNTAX);

	SWITCH_ADD_CODEC(codec_interface, "AMR-WB / Octet Aligned");
	codec_interface->parse_fmtp = amrwb_parse_fmtp_cb;

	default_fmtp_oa = generate_fmtp(pool, 1);

	switch_core_codec_add_implementation(pool, codec_interface,
										 SWITCH_CODEC_TYPE_AUDIO, 100, "AMR-WB", default_fmtp_oa,
										 16000, 16000, 23850, 20000, 320, 640, 0, 1, 1,
										 switch_amrwb_init, switch_amrwb_encode, switch_amrwb_decode, switch_amrwb_destroy);
#ifndef AMRWB_PASSTHROUGH
	codec_interface->implementations->codec_control = switch_amrwb_control;
#endif

	SWITCH_ADD_CODEC(codec_interface, "AMR-WB / Bandwidth Efficient");
	codec_interface->parse_fmtp = amrwb_parse_fmtp_cb;

	default_fmtp_be = generate_fmtp(pool, 0);

	switch_core_codec_add_implementation(pool, codec_interface,
										 SWITCH_CODEC_TYPE_AUDIO, 110, "AMR-WB", default_fmtp_be,
										 16000, 16000, 23850, 20000, 320, 640, 0, 1, 1,
										 switch_amrwb_init, switch_amrwb_encode, switch_amrwb_decode, switch_amrwb_destroy);
#ifndef AMRWB_PASSTHROUGH
	codec_interface->implementations->codec_control = switch_amrwb_control;
#endif

	switch_mutex_init(&global_lock, SWITCH_MUTEX_NESTED, pool);
	mod_amrwb_configuration_snprintf();
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "AMRWB config: %s", AMRWB_CONFIGURATION);

	/* indicate that the module should continue to be loaded */
	return SWITCH_STATUS_SUCCESS;
}

SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_amrwb_unload) {
	switch_mutex_destroy(global_lock);
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
