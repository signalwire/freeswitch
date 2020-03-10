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
 * The amr codec itself is not distributed with this module.
 *
 * mod_amr.c -- GSM-AMR Codec Module
 *
 */

#include "switch.h"

SWITCH_MODULE_LOAD_FUNCTION(mod_amr_load);
SWITCH_MODULE_DEFINITION(mod_amr, mod_amr_load, NULL, NULL);

#ifndef AMR_PASSTHROUGH
#include "opencore-amrnb/interf_enc.h"
#include "opencore-amrnb/interf_dec.h"

#include "bitshift.h"
#include "amr_be.h"
/*
 * Check section 8.1 of rfc3267 for possible sdp options.
 *
 * SDP Example
 *
 * a=fmtp:97 mode-set=0,2,5,7; mode-change-period=2; mode-change-neighbor=1
 *
 *                                 Class A   total speech
 *                Index   Mode       bits       bits
 *                ----------------------------------------
 *                  0     AMR 4.75   42         95
 *                  1     AMR 5.15   49        103
 *                  2     AMR 5.9    55        118
 *                  3     AMR 6.7    58        134
 *                  4     AMR 7.4    61        148
 *                  5     AMR 7.95   75        159
 *                  6     AMR 10.2   65        204
 *                  7     AMR 12.2   81        244
 *                  8     AMR SID    39         39
 *
 *        Table 1.  The number of class A bits for the AMR codec.
 *
 */

typedef enum {
	AMR_OPT_OCTET_ALIGN = (1 << 0),
	AMR_OPT_CRC = (1 << 1),
	AMR_OPT_MODE_CHANGE_NEIGHBOR = (1 << 2),
	AMR_OPT_ROBUST_SORTING = (1 << 3),
	AMR_OPT_INTERLEAVING = (1 << 4)
} amr_flag_t;

typedef enum {
	GENERIC_PARAMETER_AMR_MAXAL_SDUFRAMES = 0,
	GENERIC_PARAMETER_AMR_BITRATE,
	GENERIC_PARAMETER_AMR_GSMAMRCOMFORTNOISE,
	GENERIC_PARAMETER_AMR_GSMEFRCOMFORTNOISE,
	GENERIC_PARAMETER_AMR_IS_641COMFORTNOISE,
	GENERIC_PARAMETER_AMR_PDCEFRCOMFORTNOISE
} amr_param_t;

typedef enum {
	AMR_BITRATE_475 = 0,
	AMR_BITRATE_515,
	AMR_BITRATE_590,
	AMR_BITRATE_670,
	AMR_BITRATE_740,
	AMR_BITRATE_795,
	AMR_BITRATE_1020,
	AMR_BITRATE_1220
} amr_bitrate_t;

typedef enum {
	AMR_DTX_DISABLED = 0,
	AMR_DTX_ENABLED
} amr_dtx_t;

/*! \brief Various codec settings */
struct amr_codec_settings {
	int dtx_mode;
	uint32_t change_period;
	switch_byte_t max_ptime;
	switch_byte_t ptime;
	switch_byte_t channels;
	switch_byte_t flags;
	switch_byte_t enc_modes;
	switch_byte_t enc_mode;
	int max_red;

};
typedef struct amr_codec_settings amr_codec_settings_t;

struct amr_context {
	void *encoder_state;
	void *decoder_state;
	switch_byte_t enc_modes;
	switch_byte_t enc_mode;
	amr_codec_settings_t codec_settings;
	switch_byte_t flags;
	int dtx_mode;
	int max_red;
	int debug;
};

#define SWITCH_AMR_DEFAULT_BITRATE AMR_BITRATE_1220

static struct {
	switch_byte_t default_bitrate;
	switch_byte_t volte; /* enable special fmtp for VoLTE compliance */
	switch_byte_t adjust_bitrate;
	int debug;
} globals;

const int switch_amr_frame_sizes[] = {12,13,15,17,19,20,26,31,5,0};

#define SWITCH_AMR_OUT_MAX_SIZE 32
#define SWITCH_AMR_MODES 9 /* plus SID */

static switch_bool_t switch_amr_unpack_oa(unsigned char *buf, uint8_t *tmp, int encoded_data_len)
{
	uint8_t *tocs;
	int index;
	int framesz;

	if (!buf) {
		return SWITCH_FALSE;
	}
	buf++; /* CMR skip */
	tocs = buf;
	index = ((tocs[0]>>3) & 0xf);
	buf++; /* point to voice payload */
	if (index > SWITCH_AMR_MODES) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "AMR decoder (OA): Invalid Table Of Contents (TOC): 0x%x\n", index);
		return SWITCH_FALSE;
	}
	framesz = switch_amr_frame_sizes[index];
	if (framesz > encoded_data_len - 1) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "AMR decoder (OA): Invalid frame size: %d\n", framesz);
		return SWITCH_FALSE;
	}
	tmp[0] = tocs[0];
	memcpy(&tmp[1], buf, framesz);

	return SWITCH_TRUE;
}

static switch_bool_t switch_amr_pack_oa(unsigned char *shift_buf, int n)
{

	/* nothing here for now. This is where interleaving code would go */
	return SWITCH_TRUE;
}

static switch_bool_t switch_amr_info(unsigned char *encoded_buf, int encoded_data_len, int payload_format, char *print_text)
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
		if (index > SWITCH_AMR_MODES) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "AMR decoder (OA): Invalid Table Of Contents (TOC): 0x%x\n", index);
			return SWITCH_FALSE;
		}
		framesz = switch_amr_frame_sizes[index];
		not_last_frame = (tocs[0] >> 7) & 1;
		q = (tocs[0] >> 2) & 1;
		ft = tocs[0] >> 3 ;
		ft &= ~(1 << 5); /* Frame Type*/

	} else {
		/* BE */
		memcpy(shift_tocs, encoded_buf, 2);
		/* shift for BE */
		switch_amr_array_lshift(4, shift_tocs, 2);
		not_last_frame = (shift_tocs[0] >> 7) & 1;
		q = (shift_tocs[0] >> 2) & 1;
		ft = shift_tocs[0] >> 3 ;
		ft &= ~(1 << 5); /* Frame Type */
		index = (shift_tocs[0] >> 3) & 0x0f;
		if (index > SWITCH_AMR_MODES) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "AMR decoder (BE): Invalid Table Of Contents (TOC): 0x%x\n", index);
			return SWITCH_FALSE;
		}
		framesz = switch_amr_frame_sizes[index];
	}

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "%s (%s): FT: [0x%x] Q: [0x%x] Frame flag: [%d]\n",
													print_text, payload_format ? "OA":"BE", ft, q, not_last_frame);
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "%s (%s): AMR encoded voice payload sz: [%d] : | encoded_data_len: [%d]\n",
													print_text, payload_format ? "OA":"BE", framesz, encoded_data_len);

	return SWITCH_TRUE;
}

#endif

static switch_status_t switch_amr_init(switch_codec_t *codec, switch_codec_flag_t flags, const switch_codec_settings_t *codec_settings)
{
#ifdef AMR_PASSTHROUGH
	codec->flags |= SWITCH_CODEC_FLAG_PASSTHROUGH;
	if (codec->fmtp_in) {
		codec->fmtp_out = switch_core_strdup(codec->memory_pool, codec->fmtp_in);
	}
	return SWITCH_STATUS_SUCCESS;
#else

	struct amr_context *context = NULL;
	switch_codec_fmtp_t codec_fmtp;
	amr_codec_settings_t amr_codec_settings = { 0 };
	int encoding, decoding;
	int x, i, argc;
	char *argv[10];
	char fmtptmp[128];

	encoding = (flags & SWITCH_CODEC_FLAG_ENCODE);
	decoding = (flags & SWITCH_CODEC_FLAG_DECODE);

	if (!(encoding || decoding) || (!(context = switch_core_alloc(codec->memory_pool, sizeof(struct amr_context))))) {
		return SWITCH_STATUS_FALSE;
	} else {

		memset(&codec_fmtp, '\0', sizeof(struct switch_codec_fmtp));
		codec_fmtp.private_info = &amr_codec_settings;

		/* "mode" may mean two different things:
		 * "Octed Aligned" or "Bandwidth Efficient" encoding mode ,
		 * or the actual bitrate which is set with FMTP param "mode-set". */
		/* https://tools.ietf.org/html/rfc4867 */

		/* we set the default mode (bitrate) just in case "mode-set" is not present in FMTP. */
		/* bitrate = value read from config file or default mode 7 */
		context->enc_mode = globals.default_bitrate;

		/* set octet-align = 0 - it must be 0 by default when there is no "octet-align" param in FMTP, per RFC :
		 *
		 *     "octet-align: Permissible values are 0 and 1.  If 1, octet-aligned
		 *     operation SHALL be used.  If 0 or if not present,
		 *     bandwidth-efficient operation is employed."
		 *
		 */
		switch_clear_flag(context, AMR_OPT_OCTET_ALIGN);

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
							switch_set_flag(context, AMR_OPT_OCTET_ALIGN);
						}
					} else if (!strcasecmp(data, "mode-change-neighbor")) {
						if (atoi(arg)) {
							switch_set_flag(context, AMR_OPT_MODE_CHANGE_NEIGHBOR);
						}
					} else if (!strcasecmp(data, "crc")) {
						if (atoi(arg)) {
							switch_set_flag(context, AMR_OPT_CRC);
						}
					} else if (!strcasecmp(data, "robust-sorting")) {
						if (atoi(arg)) {
							switch_set_flag(context, AMR_OPT_ROBUST_SORTING);
						}
					} else if (!strcasecmp(data, "interleaving")) {
						if (atoi(arg)) {
							switch_set_flag(context, AMR_OPT_INTERLEAVING);
						}
					} else if (!strcasecmp(data, "mode-change-period")) {
						context->codec_settings.change_period = atoi(arg);
					} else if (!strcasecmp(data, "ptime")) {
						context->codec_settings.ptime = (switch_byte_t) atoi(arg);
					} else if (!strcasecmp(data, "channels")) {
						context->codec_settings.channels = (switch_byte_t) atoi(arg);
					} else if (!strcasecmp(data, "maxptime")) {
						context->codec_settings.max_ptime = (switch_byte_t) atoi(arg);
					} else if (!strcasecmp(data, "max-red")) {
						context->codec_settings.max_red = atoi(arg);
					} else if (!strcasecmp(data, "mode-set")) {
						int y, m_argc;
						char *m_argv[SWITCH_AMR_MODES-1];
						m_argc = switch_separate_string(arg, ',', m_argv, (sizeof(m_argv) / sizeof(m_argv[0])));
						for (y = 0; y < m_argc; y++) {
							context->enc_modes |= (1 << atoi(m_argv[y]));
						}
					}
				}
			}
		}

		if (context->enc_modes) {
			/* choose the highest mode (bitrate) for high audio quality */
			for (i = 7; i > -1; i--) {
				if (context->enc_modes & (1 << i)) {
					context->enc_mode = (switch_byte_t) i;
					break;
				}
			}
		}

		if (globals.adjust_bitrate) {
			switch_set_flag(codec, SWITCH_CODEC_FLAG_HAS_ADJ_BITRATE);
		}

		if (!globals.volte) {
			switch_snprintf(fmtptmp, sizeof(fmtptmp), "octet-align=%d; mode-set=%d", switch_test_flag(context, AMR_OPT_OCTET_ALIGN) ? 1 : 0,
							context->enc_mode);
		} else {
			/* some UEs reject the call with 488 if mode-change-capability is not 2 */
			switch_snprintf(fmtptmp, sizeof(fmtptmp), "octet-align=%d; mode-set=%d; max-red=0; mode-change-capability=2",
							switch_test_flag(context, AMR_OPT_OCTET_ALIGN) ? 1 : 0, context->enc_mode);
		}
		codec->fmtp_out = switch_core_strdup(codec->memory_pool, fmtptmp);

		context->encoder_state = NULL;
		context->decoder_state = NULL;

		if (encoding) {
			context->encoder_state = Encoder_Interface_init(context->dtx_mode);
		}

		if (decoding) {
			context->decoder_state = Decoder_Interface_init();
		}

		context->codec_settings = amr_codec_settings;

		codec->private_info = context;

		return SWITCH_STATUS_SUCCESS;
	}
#endif
}

static switch_status_t switch_amr_destroy(switch_codec_t *codec)
{
#ifndef AMR_PASSTHROUGH
	struct amr_context *context = codec->private_info;

	if (context->encoder_state) {
		Encoder_Interface_exit(context->encoder_state);
	}
	if (context->decoder_state) {
		Decoder_Interface_exit(context->decoder_state);
	}
	codec->private_info = NULL;
#endif
	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t switch_amr_encode(switch_codec_t *codec,
										 switch_codec_t *other_codec,
										 void *decoded_data,
										 uint32_t decoded_data_len,
										 uint32_t decoded_rate, void *encoded_data, uint32_t *encoded_data_len, uint32_t *encoded_rate,
										 unsigned int *flag)
{
#ifdef AMR_PASSTHROUGH
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "This codec is only usable in passthrough mode!\n");
	return SWITCH_STATUS_FALSE;
#else
	struct amr_context *context = codec->private_info;
	int n;
	unsigned char *shift_buf = encoded_data;

	if (!context) {
		return SWITCH_STATUS_FALSE;
	}

	n = Encoder_Interface_Encode(context->encoder_state, context->enc_mode, (int16_t *) decoded_data, (switch_byte_t *) encoded_data + 1, 0);
	if (n < 0) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "AMR encoder: Encoder_Interface_Encode() ERROR!\n");
		return SWITCH_STATUS_FALSE;
	}

	/* set CMR + TOC (F + 3 bits of FT) , 1111 = CMR: No mode request */
	*(switch_byte_t *) encoded_data = 0xf0;
	*encoded_data_len  = n;

	if (switch_test_flag(context, AMR_OPT_OCTET_ALIGN)) {
		/* this function does nothing for now - the payload is already OA
		as it comes out of the encoding function */
		switch_amr_pack_oa(shift_buf, n);
		*encoded_data_len = n + 1;
	} else {
		switch_amr_pack_be(shift_buf, n);
		/* mode 0 = 95 speech bits -> 96 bytes - 1 padding bit */
		/* mode 1 = 103 speech bits -> 104 bytes - 1 padding bit */
		/* mode 5 = 159 speech bits -> 160 bytes - 1 padding bit */
		if ((context->enc_mode == 5 ) || (context->enc_mode == 1) || (context->enc_mode == 0)) {
			/* modes 0,1 and 5 have only 1 padding bit and due to shifting
			 * we may have an extra 0 byte at the end of the voice payload (bit stuffing) */
			if (shift_buf[n-1] != 0x0) {
				*encoded_data_len  = n + 1;
			}
		}
	}

	if (globals.debug) {
		switch_amr_info(shift_buf, *encoded_data_len, switch_test_flag(context, AMR_OPT_OCTET_ALIGN) ? 1 : 0, "AMR encoder");
	}

	return SWITCH_STATUS_SUCCESS;
#endif
}

static switch_status_t switch_amr_decode(switch_codec_t *codec,
										 switch_codec_t *other_codec,
										 void *encoded_data,
										 uint32_t encoded_data_len,
										 uint32_t encoded_rate, void *decoded_data, uint32_t *decoded_data_len, uint32_t *decoded_rate,
										 unsigned int *flag)
{
#ifdef AMR_PASSTHROUGH
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "This codec is only usable in passthrough mode!\n");
	return SWITCH_STATUS_FALSE;
#else
	struct amr_context *context = codec->private_info;
	unsigned char *buf = encoded_data;
	uint8_t tmp[SWITCH_AMR_OUT_MAX_SIZE];

	if (!context) {
		return SWITCH_STATUS_FALSE;
	}

	if (globals.debug) {
		switch_amr_info(buf, encoded_data_len, switch_test_flag(context, AMR_OPT_OCTET_ALIGN) ? 1 : 0, "AMR decoder");
	}

	if (switch_test_flag(context, AMR_OPT_OCTET_ALIGN)) {
		/* Octed Aligned */
		if (!switch_amr_unpack_oa(buf, tmp, encoded_data_len)) {
			return SWITCH_STATUS_FALSE;
		}
	} else {
		/* Bandwidth Efficient */
		if (!switch_amr_unpack_be(buf, tmp, encoded_data_len)) {
			return SWITCH_STATUS_FALSE;
		}
	}

	Decoder_Interface_Decode(context->decoder_state, tmp, (int16_t *) decoded_data, 0);
	*decoded_data_len = codec->implementation->decoded_bytes_per_packet;

	return SWITCH_STATUS_SUCCESS;
#endif
}

static switch_status_t switch_amr_control(switch_codec_t *codec,
										   switch_codec_control_command_t cmd,
										   switch_codec_control_type_t ctype,
										   void *cmd_data,
										   switch_codec_control_type_t atype,
										   void *cmd_arg,
										   switch_codec_control_type_t *rtype,
										   void **ret_data)
{
#ifdef AMR_PASSTHROUGH
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "This codec is only usable in passthrough mode!\n");
	return SWITCH_STATUS_FALSE;
#else
	struct amr_context *context = codec->private_info;

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
				if (context->enc_mode < SWITCH_AMR_MODES - 1) {
					int mode_step = 2; /*this is the mode, not the actual bitrate*/
					context->enc_mode = context->enc_mode + mode_step;
					if (globals.debug || context->debug) {
						switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG,
								"AMR encoder: Adjusting mode to %d (increase)\n", context->enc_mode);
					}
				}
			} else if (!strcasecmp(cmd, "decrease")) {
				if (context->enc_mode > 0) {
					int mode_step = 2; /*this is the mode, not the actual bitrate*/
					context->enc_mode = context->enc_mode - mode_step;
					if (globals.debug || context->debug) {
						switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG,
								"AMR encoder: Adjusting mode to %d (decrease)\n", context->enc_mode);
					}
				}
			} else if (!strcasecmp(cmd, "default")) {
					context->enc_mode = globals.default_bitrate;
					if (globals.debug || context->debug) {
						switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG,
								"AMR encoder: Adjusting mode to %d (default)\n", context->enc_mode);
					}
			} else {
				/*minimum bitrate (AMR mode)*/
				context->enc_mode = 0;
				if (globals.debug || context->debug) {
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG,
							"AMR encoder: Adjusting mode to %d (minimum)\n", context->enc_mode);
				}
			}
		}
		break;
	default:
		break;
	}

	return SWITCH_STATUS_SUCCESS;
#endif
}

static char *generate_fmtp(switch_memory_pool_t *pool , int octet_align)
{
	char buf[256] = { 0 };

	snprintf(buf + strlen(buf), sizeof(buf) - strlen(buf), "octet-align=%d; ", octet_align);

#ifndef AMR_PASSTHROUGH

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

#ifndef AMR_PASSTHROUGH

#define AMRWB_DEBUG_SYNTAX "<on|off>"
SWITCH_STANDARD_API(mod_amr_debug)
{
		if (zstr(cmd)) {
			stream->write_function(stream, "-USAGE: %s\n", AMRWB_DEBUG_SYNTAX);
		} else {
			if (!strcasecmp(cmd, "on")) {
				globals.debug = 1;
				stream->write_function(stream, "AMR Debug: on\n");
			} else if (!strcasecmp(cmd, "off")) {
				globals.debug = 0;
				stream->write_function(stream, "AMR Debug: off\n");
			} else {
				stream->write_function(stream, "-USAGE: %s\n", AMRWB_DEBUG_SYNTAX);
			}
		}
	return SWITCH_STATUS_SUCCESS;
}
#endif

/* Registration */
SWITCH_MODULE_LOAD_FUNCTION(mod_amr_load)
{
	switch_codec_interface_t *codec_interface;
	char *default_fmtp_oa = NULL;
	char *default_fmtp_be = NULL;

#ifndef AMR_PASSTHROUGH
	switch_api_interface_t *commands_api_interface;
	char *cf = "amr.conf";
	switch_xml_t cfg, xml, settings, param;

	memset(&globals, 0, sizeof(globals));
	globals.default_bitrate = SWITCH_AMR_DEFAULT_BITRATE;

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
			}
		}
	}
#endif

/* connect my internal structure to the blank pointer passed to me */
	*module_interface = switch_loadable_module_create_module_interface(pool, modname);

#ifndef AMR_PASSTHROUGH
	SWITCH_ADD_API(commands_api_interface, "amr_debug", "Set AMR Debug", mod_amr_debug, AMRWB_DEBUG_SYNTAX);

	switch_console_set_complete("add amr_debug on");
	switch_console_set_complete("add amr_debug off");
#endif

	SWITCH_ADD_CODEC(codec_interface, "AMR / Octet Aligned");

	default_fmtp_oa = generate_fmtp(pool, 1);
	switch_core_codec_add_implementation(pool, codec_interface, SWITCH_CODEC_TYPE_AUDIO,	/* enumeration defining the type of the codec */
										 96,	/* the IANA code number */
										 "AMR",	/* the IANA code name */
										 default_fmtp_oa,	/* default fmtp to send (can be overridden by the init function) */
										 8000,	/* samples transferred per second */
										 8000,	/* actual samples transferred per second */
										 12200,	/* bits transferred per second */
										 20000,	/* number of microseconds per frame */
										 160,	/* number of samples per frame */
										 320,	/* number of bytes per frame decompressed */
										 0,	/* number of bytes per frame compressed */
										 1,	/* number of channels represented */
										 1,	/* number of frames per network packet */
										 switch_amr_init,	/* function to initialize a codec handle using this implementation */
										 switch_amr_encode,	/* function to encode raw data into encoded data */
										 switch_amr_decode,	/* function to decode encoded data into raw data */
										 switch_amr_destroy);	/* deinitalize a codec handle using this implementation */

	SWITCH_ADD_CODEC(codec_interface, "AMR / Bandwidth Efficient");

	default_fmtp_be = generate_fmtp(pool, 0);
	switch_core_codec_add_implementation(pool, codec_interface, SWITCH_CODEC_TYPE_AUDIO,	/* enumeration defining the type of the codec */
										 97,	/* the IANA code number */
										 "AMR",	/* the IANA code name */
										 default_fmtp_be,	/* default fmtp to send (can be overridden by the init function) */
										 8000,	/* samples transferred per second */
										 8000,	/* actual samples transferred per second */
										 12200,	/* bits transferred per second */
										 20000,	/* number of microseconds per frame */
										 160,	/* number of samples per frame */
										 320,	/* number of bytes per frame decompressed */
										 0,	/* number of bytes per frame compressed */
										 1,	/* number of channels represented */
										 1,	/* number of frames per network packet */
										 switch_amr_init,	/* function to initialize a codec handle using this implementation */
										 switch_amr_encode,	/* function to encode raw data into encoded data */
										 switch_amr_decode,	/* function to decode encoded data into raw data */
										 switch_amr_destroy);	/* deinitalize a codec handle using this implementation */

	codec_interface->implementations->codec_control = switch_amr_control;

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
