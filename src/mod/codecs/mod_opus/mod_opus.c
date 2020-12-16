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
 * Brian K. West <brian@freeswitch.org>
 * Noel Morgan <noel@vwci.com>
 * Dragos Oancea <droancea@yahoo.com>
 *
 * mod_opus.c -- The OPUS ultra-low delay audio codec (http://www.opus-codec.org/)
 *
 */

#include "switch.h"
#include "opus.h"

#define SWITCH_OPUS_MIN_BITRATE 6000
#define SWITCH_OPUS_MAX_BITRATE 510000

#define SWITCH_OPUS_MIN_FEC_BITRATE 12400

SWITCH_MODULE_LOAD_FUNCTION(mod_opus_load);
SWITCH_MODULE_DEFINITION(mod_opus, mod_opus_load, NULL, NULL);

/*! \brief Various codec settings */
struct opus_codec_settings {
	int useinbandfec;
	int usedtx;
	int maxaveragebitrate;
	int maxplaybackrate;
	int stereo;
	int cbr;
	int sprop_maxcapturerate;
	int sprop_stereo;
	int maxptime;
	int minptime;
	int ptime;
	int samplerate;
};
typedef struct opus_codec_settings opus_codec_settings_t;

static opus_codec_settings_t default_codec_settings = {
	/*.useinbandfec */ 1,
	/*.usedtx */ 1,
	/*.maxaveragebitrate */ 30000,
	/*.maxplaybackrate */ 48000,
	/*.stereo*/ 0,
	/*.cbr*/ 0,
	/*.sprop_maxcapturerate*/ 0,
	/*.sprop_stereo*/ 0,
	/*.maxptime*/ 40,
	/*.minptime*/ 10,
	/*.ptime*/ 0,
	/*.samplerate*/ 0
};

static opus_codec_settings_t default_codec_settings_8k = {
	/*.useinbandfec */ 1,
	/*.usedtx */ 1,
	/*.maxaveragebitrate */ 14000,
	/*.maxplaybackrate */ 8000,
	/*.stereo*/ 0,
	/*.cbr*/ 0,
	/*.sprop_maxcapturerate*/ 8000,
	/*.sprop_stereo*/ 0,
	/*.maxptime*/ 120,
	/*.minptime*/ 10,
	/*.ptime*/ 0,
	/*.samplerate*/ 0
};

static opus_codec_settings_t default_codec_settings_16k = {
	/*.useinbandfec */ 1,
	/*.usedtx */ 1,
	/*.maxaveragebitrate */ 20000,
	/*.maxplaybackrate */ 16000,
	/*.stereo*/ 0,
	/*.cbr*/ 0,
	/*.sprop_maxcapturerate*/ 16000,
	/*.sprop_stereo*/ 0,
	/*.maxptime*/ 60,
	/*.minptime*/ 10,
	/*.ptime*/ 0,
	/*.samplerate*/ 0
};

struct dec_stats {
	uint32_t fec_counter;
	uint32_t plc_counter;
	uint32_t frame_counter;
};
typedef struct dec_stats dec_stats_t;

struct enc_stats {
	uint32_t frame_counter;
	uint32_t encoded_bytes;
	uint32_t encoded_msec;
	uint32_t fec_counter;
};
typedef struct enc_stats enc_stats_t;

struct codec_control_state {
	int keep_fec;
	opus_int32 current_bitrate;
	opus_int32 wanted_bitrate;
	uint32_t increase_step;
	uint32_t decrease_step;
};
typedef struct codec_control_state codec_control_state_t;

struct opus_context {
	OpusEncoder *encoder_object;
	OpusDecoder *decoder_object;
	uint32_t enc_frame_size;
	uint32_t dec_frame_size;
	uint32_t old_plpct;
	uint32_t debug;
	uint32_t use_jb_lookahead;
	opus_codec_settings_t codec_settings;
	int look_check;
	int look_ts;
	dec_stats_t decoder_stats;
	enc_stats_t encoder_stats;
	codec_control_state_t control_state;
};

struct {
	int use_vbr;
	int use_dtx;
	int complexity;
	int maxaveragebitrate;
	int maxplaybackrate;
	int sprop_maxcapturerate;
	int plpct;
	int asymmetric_samplerates;
	int bitrate_negotiation;
	int keep_fec;
	int fec_decode;
	int adjust_bitrate;
	int debuginfo;
	uint32_t use_jb_lookahead;
	switch_mutex_t *mutex;
} opus_prefs;

static struct {
	int debug;
} globals;

static switch_bool_t switch_opus_acceptable_rate(int rate)
{
	if (rate != 8000 && rate != 12000 && rate != 16000 && rate != 24000 && rate != 48000) {
		return SWITCH_FALSE;
	}
	return SWITCH_TRUE;
}

static uint32_t switch_opus_encoder_set_audio_bandwidth(OpusEncoder *encoder_object,int enc_samplerate)
{
		if (enc_samplerate == 8000) { /* Audio Bandwidth: 0-4000Hz  Sampling Rate: 8000Hz */
			opus_encoder_ctl(encoder_object, OPUS_SET_BANDWIDTH(OPUS_BANDWIDTH_NARROWBAND));
			opus_encoder_ctl(encoder_object, OPUS_SET_MAX_BANDWIDTH(OPUS_BANDWIDTH_NARROWBAND));
			opus_encoder_ctl(encoder_object, OPUS_SET_SIGNAL(OPUS_SIGNAL_VOICE));
			return OPUS_BANDWIDTH_NARROWBAND;
		} else if (enc_samplerate == 12000) { /* Audio Bandwidth: 0-6000Hz  Sampling Rate: 12000Hz */
			opus_encoder_ctl(encoder_object, OPUS_SET_BANDWIDTH(OPUS_BANDWIDTH_MEDIUMBAND));
			opus_encoder_ctl(encoder_object, OPUS_SET_MAX_BANDWIDTH(OPUS_BANDWIDTH_MEDIUMBAND));
			opus_encoder_ctl(encoder_object, OPUS_SET_SIGNAL(OPUS_SIGNAL_VOICE));
			return OPUS_BANDWIDTH_MEDIUMBAND;
		} else if (enc_samplerate == 16000) { /* Audio Bandwidth: 0-8000Hz  Sampling Rate: 16000Hz */
			opus_encoder_ctl(encoder_object, OPUS_SET_BANDWIDTH(OPUS_BANDWIDTH_WIDEBAND));
			opus_encoder_ctl(encoder_object, OPUS_SET_MAX_BANDWIDTH(OPUS_BANDWIDTH_WIDEBAND));
			opus_encoder_ctl(encoder_object, OPUS_SET_SIGNAL(OPUS_SIGNAL_VOICE));
			return OPUS_BANDWIDTH_WIDEBAND;
		} else if (enc_samplerate == 24000) {  /* Audio Bandwidth: 0-12000Hz Sampling Rate: 24000Hz */
			opus_encoder_ctl(encoder_object, OPUS_SET_BANDWIDTH(OPUS_BANDWIDTH_SUPERWIDEBAND));
			opus_encoder_ctl(encoder_object, OPUS_SET_MAX_BANDWIDTH(OPUS_BANDWIDTH_SUPERWIDEBAND));
			opus_encoder_ctl(encoder_object, OPUS_SET_SIGNAL(OPUS_AUTO));
			return OPUS_BANDWIDTH_SUPERWIDEBAND;
		}
		/* Audio Bandwidth: 0-20000Hz Sampling Rate: 48000Hz */
		opus_encoder_ctl(encoder_object, OPUS_SET_BANDWIDTH(OPUS_AUTO));
		opus_encoder_ctl(encoder_object, OPUS_SET_MAX_BANDWIDTH(OPUS_BANDWIDTH_FULLBAND));
		opus_encoder_ctl(encoder_object, OPUS_SET_SIGNAL(OPUS_AUTO));
		return OPUS_BANDWIDTH_FULLBAND;
}

static switch_bool_t switch_opus_show_audio_bandwidth(int audiobandwidth,char *audiobandwidth_str)
{
	if (audiobandwidth == OPUS_BANDWIDTH_NARROWBAND) {
		strncpy(audiobandwidth_str, "NARROWBAND",11);
		return SWITCH_TRUE;
	} else if (audiobandwidth == OPUS_BANDWIDTH_MEDIUMBAND) {
		strncpy(audiobandwidth_str, "MEDIUMBAND",11);
		return SWITCH_TRUE;
	} else if (audiobandwidth == OPUS_BANDWIDTH_WIDEBAND) {
		strncpy(audiobandwidth_str,"WIDEBAND",9);
		return SWITCH_TRUE;
	} else if (audiobandwidth == OPUS_BANDWIDTH_SUPERWIDEBAND) {
		strncpy(audiobandwidth_str, "SUPERWIDEBAND",14);
		return SWITCH_TRUE;
	} else if (audiobandwidth == OPUS_BANDWIDTH_FULLBAND) {
		strncpy(audiobandwidth_str, "FULLBAND",9);
		return SWITCH_TRUE;
	}
	return SWITCH_FALSE;
}

static switch_status_t switch_opus_fmtp_parse(const char *fmtp, switch_codec_fmtp_t *codec_fmtp)
{
	if (codec_fmtp) {
		opus_codec_settings_t local_settings = { 0 };
		opus_codec_settings_t *codec_settings = &local_settings;

		if (codec_fmtp->private_info) {
			codec_settings = codec_fmtp->private_info;
			if (zstr(fmtp)) {
				memcpy(codec_settings, &default_codec_settings, sizeof(*codec_settings));
			}
		}

		if (fmtp) {
			int x, argc;
			char *argv[10];
			char *fmtp_dup = strdup(fmtp);

			switch_assert(fmtp_dup);

			argc = switch_separate_string(fmtp_dup, ';', argv, (sizeof(argv) / sizeof(argv[0])));
			for (x = 0; x < argc; x++) {
				char *data = argv[x];
				char *arg;
				switch_assert(data);
				while (*data == ' ') {
					data++;
				}

				if ((arg = strchr(data, '='))) {
					*arg++ = '\0';

					if (!strcasecmp(data, "useinbandfec")) {
						codec_settings->useinbandfec = switch_true(arg);
					}

					if (!strcasecmp(data, "usedtx")) {
						codec_settings->usedtx = switch_true(arg);
					}

					if (!strcasecmp(data, "cbr")) {
						codec_settings->cbr = switch_true(arg);
					}

					if (!strcasecmp(data, "maxptime")) {
						codec_settings->maxptime = atoi(arg);
					}

					if (!strcasecmp(data, "minptime")) {
						codec_settings->minptime = atoi(arg);
					}

					if (!strcasecmp(data, "ptime")) {
						codec_settings->ptime = atoi(arg);
						codec_fmtp->microseconds_per_packet = codec_settings->ptime * 1000;
					}

					if (!strcasecmp(data, "stereo")) {
						codec_settings->stereo = atoi(arg);
						codec_fmtp->stereo = codec_settings->stereo;
					}

					if (!strcasecmp(data, "sprop-stereo")) {
						codec_settings->sprop_stereo = atoi(arg);
					}

					if (!strcasecmp(data, "maxaveragebitrate")) {
						codec_settings->maxaveragebitrate = atoi(arg);
						if (codec_settings->maxaveragebitrate < SWITCH_OPUS_MIN_BITRATE || codec_settings->maxaveragebitrate > SWITCH_OPUS_MAX_BITRATE) {
							codec_settings->maxaveragebitrate = 0; /* values outside the range between 6000 and 510000 SHOULD be ignored */
						}
					}

					if (!strcasecmp(data, "maxplaybackrate")) {
						codec_settings->maxplaybackrate = atoi(arg);
						if (!switch_opus_acceptable_rate(codec_settings->maxplaybackrate)) {
							codec_settings->maxplaybackrate = 0; /* value not supported */
						}
					}
					if (!strcasecmp(data, "sprop-maxcapturerate")) {
						codec_settings->sprop_maxcapturerate = atoi(arg);
						if (!switch_opus_acceptable_rate(codec_settings->sprop_maxcapturerate)) {
							codec_settings->sprop_maxcapturerate = 0; /* value not supported */
						}
					}
				}
			}
			free(fmtp_dup);
		}
		return SWITCH_STATUS_SUCCESS;
	}
	return SWITCH_STATUS_FALSE;
}

static char *gen_fmtp(opus_codec_settings_t *settings, switch_memory_pool_t *pool)
{
	char buf[256] = { 0 };

	snprintf(buf + strlen(buf), sizeof(buf) - strlen(buf), "useinbandfec=%d; ", settings->useinbandfec);

	if (settings->usedtx) {
		snprintf(buf + strlen(buf), sizeof(buf) - strlen(buf), "usedtx=1; ");
	}

	if (settings->cbr) {
		snprintf(buf + strlen(buf), sizeof(buf) - strlen(buf), "cbr=1; ");
	}

	if (settings->maxaveragebitrate) {
		snprintf(buf + strlen(buf), sizeof(buf) - strlen(buf), "maxaveragebitrate=%d; ", settings->maxaveragebitrate);
	}

	if (settings->maxplaybackrate) {
		snprintf(buf + strlen(buf), sizeof(buf) - strlen(buf), "maxplaybackrate=%d; ", settings->maxplaybackrate);
	}

	if (settings->sprop_maxcapturerate) {
		snprintf(buf + strlen(buf), sizeof(buf) - strlen(buf), "sprop-maxcapturerate=%d; ", settings->sprop_maxcapturerate);
	}

	if (settings->ptime) {
		snprintf(buf + strlen(buf), sizeof(buf) - strlen(buf), "ptime=%d; ", settings->ptime);
	}

	if (settings->minptime) {
		snprintf(buf + strlen(buf), sizeof(buf) - strlen(buf), "minptime=%d; ", settings->minptime);
	}

	if (settings->maxptime) {
		snprintf(buf + strlen(buf), sizeof(buf) - strlen(buf), "maxptime=%d; ", settings->maxptime);
	}

	if (settings->stereo) {
		snprintf(buf + strlen(buf), sizeof(buf) - strlen(buf), "stereo=%d; ", settings->stereo);
	}

	if (settings->sprop_stereo) {
		snprintf(buf + strlen(buf), sizeof(buf) - strlen(buf), "sprop-stereo=%d; ", settings->sprop_stereo);
	}

	if (end_of(buf) == ' ') {
		*(end_of_p(buf) - 1) = '\0';
	}

	return switch_core_strdup(pool, buf);

}

static switch_bool_t switch_opus_has_fec(const uint8_t* payload,int payload_length_bytes)
{
	/* nb_silk_frames: number of silk-frames (10 or 20 ms) in an opus frame:  0, 1, 2 or 3 */
	/* computed from the 5 MSB (configuration) of the TOC byte (payload[0]) */
	/* nb_opus_frames: number of opus frames in the packet */
	/* computed from the 2 LSB (p0p1) of the TOC byte */
	/* p0p1 = 0  => nb_opus_frames = 1 */
	/* p0p1 = 1 or 2  => nb_opus_frames = 2 */
	/* p0p1 = 3  =>  given by the 6 LSB of payload[1] */

	int nb_silk_frames, nb_opus_frames, n, i;
	opus_int16 frame_sizes[48];
	const unsigned char *frame_data[48];

	if (payload == NULL || payload_length_bytes <= 0) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "corrupted packet (invalid size)\n");
		return SWITCH_FALSE;
	}
	if (payload[0] & 0x80) {
		/* this scares users and its harmless so commenting it */
		//switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "FEC in CELT_ONLY mode ?!\n");
		return SWITCH_FALSE;
	}

	if ((nb_opus_frames = opus_packet_parse(payload, payload_length_bytes, NULL, frame_data, frame_sizes, NULL)) <= 0) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "OPUS_INVALID_PACKET ! nb_opus_frames: %d\n", nb_opus_frames);
		return SWITCH_FALSE;
	}

	if ((payload[0] >> 3 ) < 12) { /* config in silk-only : NB,MB,WB */
		nb_silk_frames = (payload[0] >> 3) & 0x3;
		if(nb_silk_frames  == 0) {
			nb_silk_frames = 1;
		}
		if ((nb_silk_frames == 1) && (nb_opus_frames == 1)) {
			for (n = 0; n <= (payload[0]&0x4) ; n++) { /* mono or stereo: 10,20 ms */
				if (frame_data[0][0] & (0x80 >> ((n + 1) * (nb_silk_frames + 1) - 1))) {
					return SWITCH_TRUE; /* frame has FEC */
				}
			}
		} else {
			opus_int16 LBRR_flag = 0 ;
			for (i=0 ; i < nb_opus_frames; i++ ) { /* only mono Opus frames */
				LBRR_flag = (frame_data[i][0] >> (7 - nb_silk_frames)) & 0x1;
				if (LBRR_flag) {
					return SWITCH_TRUE; /* one of the silk frames has FEC */
				}
			}
		}

		return SWITCH_FALSE;
	}

	return  SWITCH_FALSE;
}

/* this is only useful for fs = 8000 hz, the map is only used
 * at the beginning of the call. */
static int switch_opus_get_fec_bitrate(int fs, int loss)
{
	int threshold_bitrates_8k[25] = {
		15600,15200,15200,15200,14800,
		14800,14800,14800,14400,14400,
		14400,14000,14000,14000,13600,
		13600,13600,13600,13200,13200,
		13200,12800,12800,12800,12400
	};

	int threshold_bitrates_16k[25] = {
		20400, 20400, 20000, 20000, 19600,
		19600, 19600, 19200, 19200, 18800,
		18800, 18800, 18400, 18400, 18000,
		18000, 18000, 17600, 17600, 17200,
		17200, 17200, 16800, 16800, 16400
	};

	if (loss <= 0){
		return SWITCH_STATUS_FALSE;
	}

	if (fs == 8000) {
		if (loss >=25) {
			return threshold_bitrates_8k[24];
		} else {
			return threshold_bitrates_8k[loss-1];
		}
	} else if (fs == 16000) {
		if (loss >=25) {
			return threshold_bitrates_16k[24];
		} else {
			return threshold_bitrates_16k[loss-1];
		}
	}

	return SWITCH_STATUS_FALSE ;
}

static switch_status_t switch_opus_info(void * encoded_data, uint32_t len, uint32_t samples_per_second, char *print_text)
{
	int nb_samples, nb_opus_frames, nb_channels;
	int audiobandwidth;
	char audiobandwidth_str[32] = {0};
	opus_int16 frame_sizes[48];
	const unsigned char *frame_data[48];
	char has_fec = 0;
	uint8_t * payload = encoded_data;

	if (!encoded_data) {
		/* print stuff, even if encoded_data is NULL. eg: "PLC correction" */
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "%s", print_text);
		return SWITCH_STATUS_FALSE;
	}

	audiobandwidth = opus_packet_get_bandwidth(encoded_data);

	if (!switch_opus_show_audio_bandwidth(audiobandwidth,audiobandwidth_str)) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "%s: OPUS_INVALID_PACKET !\n", print_text);
	}

	if ((nb_opus_frames = opus_packet_parse(encoded_data, len, NULL, frame_data, frame_sizes, NULL)) <= 0 ) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "%s: OPUS_INVALID_PACKET ! frames: %d\n", print_text, nb_opus_frames);
		return SWITCH_STATUS_FALSE;
	}

	nb_samples = opus_packet_get_samples_per_frame(encoded_data, samples_per_second) * nb_opus_frames;

	has_fec = switch_opus_has_fec(payload, len);

	nb_channels = opus_packet_get_nb_channels(payload);

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "%s: opus_frames [%d] samples [%d] audio bandwidth [%s] bytes [%d] FEC[%s] channels[%d]\n",
							print_text, nb_opus_frames, nb_samples, audiobandwidth_str, len, has_fec ? "yes" : "no", nb_channels);

	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t switch_opus_init(switch_codec_t *codec, switch_codec_flag_t flags, const switch_codec_settings_t *codec_settings)
{
	struct opus_context *context = NULL;
	int encoding = (flags & SWITCH_CODEC_FLAG_ENCODE);
	int decoding = (flags & SWITCH_CODEC_FLAG_DECODE);
	switch_codec_fmtp_t codec_fmtp, codec_fmtp_only_remote = { 0 };
	opus_codec_settings_t opus_codec_settings = { 0 };
	opus_codec_settings_t opus_codec_settings_remote = { 0 };

	if (!(encoding || decoding) || (!(context = switch_core_alloc(codec->memory_pool, sizeof(*context))))) {
		return SWITCH_STATUS_FALSE;
	}

	context->enc_frame_size = codec->implementation->actual_samples_per_second * (codec->implementation->microseconds_per_packet / 1000) / 1000;

	memset(&codec_fmtp, '\0', sizeof(struct switch_codec_fmtp));
	codec_fmtp.private_info = &opus_codec_settings;
	switch_opus_fmtp_parse(codec->fmtp_in, &codec_fmtp);
	if (opus_prefs.asymmetric_samplerates || opus_prefs.bitrate_negotiation) {
		/* save the remote fmtp values, before processing */
		codec_fmtp_only_remote.private_info = &opus_codec_settings_remote;
		switch_opus_fmtp_parse(codec->fmtp_in, &codec_fmtp_only_remote);
	}

	/* If bitrate negotiation is allowed, verify whether remote is asking for a smaller maxaveragebitrate */
	if (opus_prefs.maxaveragebitrate &&
		 (!opus_prefs.bitrate_negotiation || (opus_prefs.maxaveragebitrate < opus_codec_settings_remote.maxaveragebitrate) || !opus_codec_settings_remote.maxaveragebitrate)) {
		opus_codec_settings.maxaveragebitrate = opus_prefs.maxaveragebitrate;
	} else {
		opus_codec_settings.maxaveragebitrate = opus_codec_settings_remote.maxaveragebitrate;
	}

	/* If asymmetric sample rates are allowed, verify whether remote is asking for a smaller maxplaybackrate */
	if (opus_prefs.maxplaybackrate &&
		 (!opus_prefs.asymmetric_samplerates || (opus_prefs.maxplaybackrate < opus_codec_settings_remote.maxplaybackrate) || !opus_codec_settings_remote.maxplaybackrate)) {
		opus_codec_settings.maxplaybackrate = opus_prefs.maxplaybackrate;
	} else {
		opus_codec_settings.maxplaybackrate=opus_codec_settings_remote.maxplaybackrate;
	}

	/* If asymmetric sample rates are allowed, verify whether remote is asking for a smaller sprop_maxcapturerate */
	if (opus_prefs.sprop_maxcapturerate &&
		 (!opus_prefs.asymmetric_samplerates || (opus_prefs.sprop_maxcapturerate < opus_codec_settings_remote.sprop_maxcapturerate) || !opus_codec_settings_remote.sprop_maxcapturerate)) {
		opus_codec_settings.sprop_maxcapturerate = opus_prefs.sprop_maxcapturerate;
	} else {
		opus_codec_settings.sprop_maxcapturerate = opus_codec_settings_remote.sprop_maxcapturerate;
	}

	opus_codec_settings.useinbandfec = opus_prefs.fec_decode;

	opus_codec_settings.cbr = !opus_prefs.use_vbr;

	opus_codec_settings.usedtx = opus_prefs.use_dtx;

	codec->fmtp_out = gen_fmtp(&opus_codec_settings, codec->memory_pool);

	if (encoding) {
		/* come up with a way to specify these */
		int bitrate_bps = OPUS_AUTO;
		int use_vbr = opus_codec_settings.cbr ? 0 : opus_prefs.use_vbr  ;
		int complexity = opus_prefs.complexity;
		int plpct = opus_prefs.plpct;
		int err;
		int enc_samplerate = opus_codec_settings.samplerate ? opus_codec_settings.samplerate : codec->implementation->actual_samples_per_second;

		if (opus_prefs.asymmetric_samplerates) {
		 /* If an entity receives an fmtp: maxplaybackrate=R1,sprop-maxcapturerate=R2 and sends an fmtp with:
		 * maxplaybackrate=R3,sprop-maxcapturerate=R4
		 * then it should start the encoder at sample rate: min(R1, R4) and the decoder at sample rate: min(R3, R2)*/
			if (codec_fmtp.private_info) {
				opus_codec_settings_t *settings = codec_fmtp_only_remote.private_info;
				if (opus_codec_settings.sprop_maxcapturerate || (settings && settings->maxplaybackrate)) {
					enc_samplerate = opus_codec_settings.sprop_maxcapturerate; /*R4*/
					if (settings && settings->maxplaybackrate < enc_samplerate && settings->maxplaybackrate) {
						enc_samplerate = settings->maxplaybackrate; /*R1*/
						context->enc_frame_size = enc_samplerate * (codec->implementation->microseconds_per_packet / 1000) / 1000;
						switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Opus encoder will be created at sample rate %d hz\n",enc_samplerate);
					} else {
						enc_samplerate = codec->implementation->actual_samples_per_second;
					}
				}
			}
		}

		context->encoder_object = opus_encoder_create(enc_samplerate,
													  codec->implementation->number_of_channels,
													  codec->implementation->number_of_channels == 1 ? OPUS_APPLICATION_VOIP : OPUS_APPLICATION_AUDIO, &err);

		if (err != OPUS_OK) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Cannot create encoder: %s\n", opus_strerror(err));
			return SWITCH_STATUS_GENERR;
		}

		/* https://tools.ietf.org/html/rfc7587  */
		if (opus_codec_settings.maxaveragebitrate) {
			opus_encoder_ctl(context->encoder_object, OPUS_SET_BITRATE(opus_codec_settings.maxaveragebitrate));
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Opus encoder: set bitrate based on maxaveragebitrate value found in SDP or local config [%dbps]\n", opus_codec_settings.maxaveragebitrate);
		} else {
			opus_encoder_ctl(context->encoder_object, OPUS_SET_BANDWIDTH(OPUS_AUTO));
			opus_encoder_ctl(context->encoder_object, OPUS_SET_BITRATE(bitrate_bps)); /* OPUS_AUTO */
			opus_encoder_ctl(context->encoder_object, OPUS_GET_BITRATE(&bitrate_bps)); /* return average bps for this audio bandwidth */
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Opus encoder: set bitrate to local settings [%dbps]\n", bitrate_bps);
		}
		/* Another fmtp setting from https://tools.ietf.org/html/rfc7587 - "RTP Payload Format for the Opus Speech and Audio Codec" */
		if (opus_codec_settings.maxplaybackrate) {
			opus_int32 audiobandwidth;
			char audiobandwidth_str[32] = {0};

			audiobandwidth = switch_opus_encoder_set_audio_bandwidth(context->encoder_object,opus_codec_settings.maxplaybackrate);
			if (!switch_opus_show_audio_bandwidth(audiobandwidth,audiobandwidth_str)) {
				snprintf(audiobandwidth_str, sizeof(audiobandwidth_str), "%s", "OPUS_AUTO");
			}
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Opus encoder: set audio bandwidth to [%s] based on maxplaybackrate value found in SDP or local config [%dHz]\n",audiobandwidth_str,opus_codec_settings.maxplaybackrate);
		}

		if (use_vbr) {
			/* VBR is default*/
			opus_encoder_ctl(context->encoder_object, OPUS_SET_VBR(use_vbr));
		} else {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Opus encoder: CBR mode enabled\n");
			opus_encoder_ctl(context->encoder_object, OPUS_SET_VBR(0));
		}

		if (complexity) {
			opus_encoder_ctl(context->encoder_object, OPUS_SET_COMPLEXITY(complexity));
		}

		if (plpct) {
			opus_encoder_ctl(context->encoder_object, OPUS_SET_PACKET_LOSS_PERC(plpct));
		}

		if (opus_codec_settings.useinbandfec) {
			/* FEC on the encoder: start the call with a preconfigured packet loss percentage */
			int loss_percent = opus_prefs.plpct ;
			opus_encoder_ctl(context->encoder_object, OPUS_SET_INBAND_FEC(opus_codec_settings.useinbandfec));
			opus_encoder_ctl(context->encoder_object, OPUS_SET_PACKET_LOSS_PERC(loss_percent));
			if (opus_prefs.keep_fec){
				int fec_bitrate = switch_opus_get_fec_bitrate(enc_samplerate,loss_percent);
				 /* keep a bitrate for which the encoder will always add FEC */
				if (fec_bitrate != SWITCH_STATUS_FALSE) {
					opus_encoder_ctl(context->encoder_object, OPUS_SET_BITRATE(fec_bitrate));
					/* will override the maxaveragebitrate set in opus.conf.xml  */
					opus_codec_settings.maxaveragebitrate = fec_bitrate;
				}
				context->control_state.keep_fec = opus_prefs.keep_fec;
			}
		}

		if (opus_codec_settings.usedtx) {
			opus_encoder_ctl(context->encoder_object, OPUS_SET_DTX(opus_codec_settings.usedtx));
		}

		if (opus_prefs.adjust_bitrate) {
			switch_set_flag(codec, SWITCH_CODEC_FLAG_HAS_ADJ_BITRATE);
		}
	}

	if (decoding) {
		int err;
		int dec_samplerate = codec->implementation->actual_samples_per_second;

		if (opus_prefs.asymmetric_samplerates) {
			if (codec_fmtp.private_info) {
				opus_codec_settings_t *settings = codec_fmtp_only_remote.private_info;
				if (opus_codec_settings.maxplaybackrate || (settings && settings->sprop_maxcapturerate)) {
					dec_samplerate = opus_codec_settings.maxplaybackrate; /* R3 */
					if (settings && dec_samplerate > settings->sprop_maxcapturerate && settings->sprop_maxcapturerate) {
						dec_samplerate = settings->sprop_maxcapturerate; /* R2 */
						context->dec_frame_size = dec_samplerate*(codec->implementation->microseconds_per_packet / 1000) / 1000;
						switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Opus decoder will be created at sample rate %d hz\n",dec_samplerate);
					} else {
						dec_samplerate = codec->implementation->actual_samples_per_second;
					}
				}
			}
		}

		context->decoder_object = opus_decoder_create(dec_samplerate, (!context->codec_settings.sprop_stereo ? codec->implementation->number_of_channels : 2), &err);

		switch_set_flag(codec, SWITCH_CODEC_FLAG_HAS_PLC);

		if (err != OPUS_OK) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Cannot create decoder: %s\n", opus_strerror(err));

			if (context->encoder_object) {
				opus_encoder_destroy(context->encoder_object);
				context->encoder_object = NULL;
			}

			return SWITCH_STATUS_GENERR;
		}
	}

	context->codec_settings = opus_codec_settings;
	codec->private_info = context;

	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t switch_opus_destroy(switch_codec_t *codec)
{
	struct opus_context *context = codec->private_info;

	if (context) {
		if (context->decoder_object) {
			switch_core_session_t *session = codec->session;
			if (session) {
				switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG,"Opus decoder stats: Frames[%d] PLC[%d] FEC[%d]\n",
										context->decoder_stats.frame_counter, context->decoder_stats.plc_counter-context->decoder_stats.fec_counter, context->decoder_stats.fec_counter);
			}
			opus_decoder_destroy(context->decoder_object);
			context->decoder_object = NULL;
		}
		if (context->encoder_object) {
			switch_core_session_t *session = codec->session;
			int avg_encoded_bitrate = 0;
			if (session) {

				if (context->encoder_stats.encoded_bytes > 0 && (context->encoder_stats.encoded_msec > 1000)) {
					avg_encoded_bitrate = (context->encoder_stats.encoded_bytes * 8) / (context->encoder_stats.encoded_msec / 1000);
				}

				switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG,
						"Opus encoder stats: Frames[%d] Bytes encoded[%d] Encoded length ms[%d] Average encoded bitrate bps[%d]\n",
						context->encoder_stats.frame_counter, context->encoder_stats.encoded_bytes, context->encoder_stats.encoded_msec, avg_encoded_bitrate);

				if (globals.debug || context->debug > 1) {
					switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG,
							"Opus encoder stats: FEC frames (only for debug mode) [%d]\n", context->encoder_stats.fec_counter);
				}
			}
			opus_encoder_destroy(context->encoder_object);
			context->encoder_object = NULL;
		}
	}

	codec->private_info = NULL;
	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t switch_opus_encode(switch_codec_t *codec,
										  switch_codec_t *other_codec,
										  void *decoded_data,
										  uint32_t decoded_data_len,
										  uint32_t decoded_rate, void *encoded_data, uint32_t *encoded_data_len, uint32_t *encoded_rate,
										  unsigned int *flag)
{
	struct opus_context *context = codec->private_info;
	int bytes = 0;
	int len = (int) *encoded_data_len;

	if (!context) {
		return SWITCH_STATUS_FALSE;
	}

	bytes = opus_encode(context->encoder_object, (void *) decoded_data, context->enc_frame_size, (unsigned char *) encoded_data, len);

	if (globals.debug || context->debug > 1) {
		int samplerate = context->enc_frame_size * 1000 / (codec->implementation->microseconds_per_packet / 1000);
		switch_opus_info(encoded_data, bytes, samplerate, "encode");
	}

	if (bytes > 0) {
		*encoded_data_len = (uint32_t) bytes;

		context->encoder_stats.frame_counter++;
		if (context->enc_frame_size > 0) {
			context->encoder_stats.encoded_msec += codec->implementation->microseconds_per_packet / 1000;
		}
		context->encoder_stats.encoded_bytes += (uint32_t)bytes;

		if (globals.debug || context->debug > 1) {
			// This stat is expensive, so get it only when in debug mode
			if (switch_opus_has_fec((uint8_t *)encoded_data, bytes)) {
				context->encoder_stats.fec_counter++;
			}
		}

		return SWITCH_STATUS_SUCCESS;
	}

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR,
					  "Encoder Error: %s Decoded Datalen %u Codec NumberChans %u Len %u DecodedDate %p EncodedData %p ContextEncoderObject %p!\n",
					  opus_strerror(bytes),decoded_data_len,codec->implementation->number_of_channels,len,(void *) decoded_data,
					  (void *) encoded_data,(void *) context->encoder_object);

	return SWITCH_STATUS_GENERR;
}


static switch_status_t switch_opus_decode(switch_codec_t *codec,
										  switch_codec_t *other_codec,
										  void *encoded_data,
										  uint32_t encoded_data_len,
										  uint32_t encoded_rate, void *decoded_data, uint32_t *decoded_data_len, uint32_t *decoded_rate,
										  unsigned int *flag)
{
	struct opus_context *context = codec->private_info;
	int samples = 0;
	int fec = 0, plc = 0;
	int32_t frame_size = 0, last_frame_size = 0;
	uint32_t frame_samples;

	if (!context) {
		return SWITCH_STATUS_FALSE;
	}

	frame_samples = *decoded_data_len / 2 / (!context->codec_settings.sprop_stereo ? codec->implementation->number_of_channels : 2);
	frame_size = frame_samples - (frame_samples % (codec->implementation->actual_samples_per_second / 400));

	if (*flag & SFF_PLC) {
		switch_core_session_t *session = codec->session;
		switch_jb_t *jb = NULL;

		plc = 1;

		encoded_data = NULL;

		if ((opus_prefs.use_jb_lookahead || context->use_jb_lookahead) && context->codec_settings.useinbandfec && session) {
			switch_channel_t *channel = switch_core_session_get_channel(session);

			if (!context->look_check) {
				context->look_ts = switch_true(switch_channel_get_variable_dup(channel, "jb_use_timestamps", SWITCH_FALSE, -1));
				context->look_check = 1;
			}
			if (codec->cur_frame && (jb = switch_core_session_get_jb(session, SWITCH_MEDIA_TYPE_AUDIO))) {
				switch_frame_t frame = { 0 };
				uint8_t buf[SWITCH_RTP_MAX_BUF_LEN];
				uint32_t ts = 0;
				uint16_t seq = 0;

				if (context->look_ts) {
					ts = codec->cur_frame->timestamp;
				} else {
					seq = codec->cur_frame->seq;
				}

				frame.data = buf;
				frame.buflen = sizeof(buf);

				if (globals.debug || context->debug) {
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Missing %s %u Checking JB\n", seq ? "SEQ" : "TS", seq ? seq : ts);
				}

				if (switch_jb_peek_frame(jb, ts, seq, 1, &frame) == SWITCH_STATUS_SUCCESS) {
					if (globals.debug || context->debug) {
						switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Lookahead frame found: %u:%u\n",
										  frame.timestamp, frame.seq);
					}


					if ((fec = switch_opus_has_fec(frame.data, frame.datalen))) {
						encoded_data = frame.data;
						encoded_data_len = frame.datalen;
					}

					if (globals.debug || context->debug) {
						if (fec) {
							switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "FEC info available in packet with SEQ: %d LEN: %d\n",
											  frame.seq, frame.datalen);
						} else {
							switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "NO FEC info in this packet with SEQ: %d LEN: %d\n",
											  frame.seq, frame.datalen);
						}
					}
				}
			}
		}


		opus_decoder_ctl(context->decoder_object, OPUS_GET_LAST_PACKET_DURATION(&last_frame_size));
		if (last_frame_size) frame_size = last_frame_size;

		if (globals.debug || context->debug) {
			if (opus_prefs.use_jb_lookahead || context->use_jb_lookahead) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "MISSING FRAME: %s\n", fec ? "Look-ahead FEC" : "PLC");
			} else {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "MISSING FRAME: OPUS_PLC\n");
			}
		}

		*flag &= ~SFF_PLC;
	}

	if (globals.debug || context->debug > 1) {
		int samplerate = context->dec_frame_size * 1000 / (codec->implementation->microseconds_per_packet / 1000);
		switch_opus_info(encoded_data, encoded_data_len,
						 samplerate ? samplerate : codec->implementation->actual_samples_per_second,
						 !encoded_data ? "PLC correction" : fec ?  "FEC correction" : "decode");
	}

	if (plc) {
		context->decoder_stats.plc_counter++;
	}
	if (fec) {
		context->decoder_stats.fec_counter++;
	}
	/* a frame for which we decode FEC will be counted twice */
	context->decoder_stats.frame_counter++;

	samples = opus_decode(context->decoder_object, encoded_data, encoded_data_len, decoded_data, frame_size, fec);

	if (samples < 0) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Decoder Error: %s fs:%u plc:%s!\n",
						  opus_strerror(samples), frame_size, plc ? "true" : "false");
		return SWITCH_STATUS_NOOP;
	}

	*decoded_data_len = samples * 2 * (!context->codec_settings.sprop_stereo ? codec->implementation->number_of_channels : 2);

	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t switch_opus_encode_repacketize(switch_codec_t *codec,
										  switch_codec_t *other_codec,
										  void *decoded_data,
										  uint32_t decoded_data_len,
										  uint32_t decoded_rate, void *encoded_data, uint32_t *encoded_data_len, uint32_t *encoded_rate,
										  unsigned int *flag)
{
	struct opus_context *context = codec->private_info;
	int len = (int) *encoded_data_len;
	OpusRepacketizer *rp = opus_repacketizer_create();
	int16_t *dec_ptr_buf = decoded_data;
	/* work inside the available buffer to avoid other buffer allocations. *encoded_data_len will be SWITCH_RECOMMENDED_BUFFER_SIZE */
	unsigned char *enc_ptr_buf =  (unsigned char *)encoded_data + (len / 2);
	uint32_t nb_frames = codec->implementation->microseconds_per_packet / 20000 ; /* requested ptime: 20 ms * nb_frames */
	uint32_t frame_size, i;
	int bytes = 0, want_fec = 0, toggle_fec = 0;
	opus_int32 ret = 0;
	opus_int32 total_len = 0;
	switch_status_t status = SWITCH_STATUS_SUCCESS;

	if (!context) {
		switch_goto_status(SWITCH_STATUS_FALSE, end);
	}
	opus_encoder_ctl(context->encoder_object, OPUS_GET_INBAND_FEC(&want_fec));
	if (want_fec && context->codec_settings.useinbandfec) {
		/* if FEC might be used , pack only 2 frames like: 80 ms = 2 x 40 ms , 120 ms = 2 x 60 ms  */
		/* the first frame in the resulting payload must have FEC, the other must not ( avoid redundant FEC payload ) */
		nb_frames = 2;
		if (codec->implementation->microseconds_per_packet / 1000 == 100) { /* 100 ms = 20 ms * 5 . because there is no 50 ms frame in Opus */
				nb_frames = 5;
		}
	}
	frame_size = (decoded_data_len / 2) / nb_frames;
	if((frame_size * nb_frames) != context->enc_frame_size) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR,"Encoder Error: Decoded Datalen %u Number of frames: %u Encoder frame size: %u\n",decoded_data_len,nb_frames,context->enc_frame_size);
		switch_goto_status(SWITCH_STATUS_GENERR, end);
	}
	opus_repacketizer_init(rp);
	dec_ptr_buf = (int16_t *)decoded_data;
	for (i = 0; i < nb_frames; i++) {
		 /* set inband FEC ON or OFF for the next Opus frame */
		if (i == (nb_frames - 1) && want_fec) {
			/* When FEC is enabled for Opus frame N, LBRR is stored during regular encoding of */
			/* this Opus frame N, and this LBRR data will be packed with the regular encoding */
			/* data of Opus frame N+1. We enable FEC on our last Opus frame which is to be packed, just */
			/* to actually have it stored in the first Opus frame, that is when switch_opus_encode_repacketize() */
			/* is called again to pack the next big 80,100 or 120 ms frame. */
			toggle_fec = 1; /* FEC ON for the last frame */
		}

		opus_encoder_ctl(context->encoder_object, OPUS_SET_INBAND_FEC(toggle_fec));
		bytes = opus_encode(context->encoder_object, (opus_int16 *) dec_ptr_buf, frame_size, enc_ptr_buf, len);

		if (bytes < 0) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Encoder Error: %s Decoded Datalen %u Codec NumberChans %u" \
							  "Len %u DecodedDate %p EncodedData %p ContextEncoderObject %p enc_frame_size: %d\n",
							  opus_strerror(bytes), decoded_data_len, codec->implementation->number_of_channels, len,
							  (void *) decoded_data, (void *) encoded_data, (void *) context->encoder_object, context->enc_frame_size);
			switch_goto_status(SWITCH_STATUS_GENERR, end);
		}
		/* enc_ptr_buf : Opus API manual:  "The application must ensure this pointer remains valid until the next call to opus_repacketizer_init() or opus_repacketizer_destroy()." */
		ret = opus_repacketizer_cat(rp, enc_ptr_buf, bytes);
		if (ret != OPUS_OK) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Opus encoder: error while repacketizing (cat) : %s !\n",opus_strerror(ret));
			switch_goto_status(SWITCH_STATUS_GENERR, end);
		}
		enc_ptr_buf += bytes;
		total_len += bytes;
		dec_ptr_buf += frame_size ;
	}
	/* this will never happen, unless there is a huge and unsupported number of frames */
	if (total_len + opus_repacketizer_get_nb_frames(rp) > len / 2) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Opus encoder: error while repacketizing: not enough buffer space\n");
		switch_goto_status(SWITCH_STATUS_GENERR, end);
	}
	ret = opus_repacketizer_out(rp, encoded_data, total_len+opus_repacketizer_get_nb_frames(rp));

	if (globals.debug || context->debug) {
		int samplerate = context->enc_frame_size * 1000 / (codec->implementation->microseconds_per_packet / 1000);
		switch_opus_info(encoded_data, ret, samplerate, "encode_repacketize");
	}

	if (ret <= 0) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Opus encoder: error while repacketizing (out) : %s ! packed nb_frames: %d\n", opus_strerror(ret), opus_repacketizer_get_nb_frames(rp));
		switch_goto_status(SWITCH_STATUS_GENERR, end);
	}
	if (want_fec) {
		opus_encoder_ctl(context->encoder_object, OPUS_SET_INBAND_FEC(want_fec)); /*restore FEC state*/
	}
	*encoded_data_len = (uint32_t) ret;

end:
	if (rp) {
		opus_repacketizer_destroy(rp);
	}

	return status;
}

static switch_status_t opus_load_config(switch_bool_t reload)
{
	char *cf = "opus.conf";
	switch_xml_t cfg, xml = NULL, param, settings;
	switch_status_t status = SWITCH_STATUS_SUCCESS;

	if (!(xml = switch_xml_open_cfg(cf, &cfg, NULL))) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Opening of %s failed\n", cf);
		return status;
	}

	memset(&opus_prefs, 0, sizeof(opus_prefs));
	opus_prefs.use_jb_lookahead = 1;
	opus_prefs.keep_fec = 1;
	opus_prefs.use_dtx = 0;
	opus_prefs.plpct = 20;
	opus_prefs.use_vbr = 0;
	opus_prefs.fec_decode = 1;

	if ((settings = switch_xml_child(cfg, "settings"))) {
		for (param = switch_xml_child(settings, "param"); param; param = param->next) {
			char *key = (char *) switch_xml_attr_soft(param, "name");
			char *val = (char *) switch_xml_attr_soft(param, "value");

			if (!strcasecmp(key, "use-vbr") && !zstr(val)) {
				opus_prefs.use_vbr = atoi(val);
			} else if (!strcasecmp(key, "use-dtx")) {
				opus_prefs.use_dtx = atoi(val);
			} else if (!strcasecmp(key, "complexity")) {
				opus_prefs.complexity = atoi(val);
			} else if (!strcasecmp(key, "packet-loss-percent")) {
				opus_prefs.plpct = atoi(val);
			} else if (!strcasecmp(key, "asymmetric-sample-rates")) {
				opus_prefs.asymmetric_samplerates = atoi(val);
			} else if (!strcasecmp(key, "bitrate-negotiation")) {
				opus_prefs.bitrate_negotiation = atoi(val);
			} else if (!strcasecmp(key, "use-jb-lookahead")) {
				opus_prefs.use_jb_lookahead = switch_true(val);
			} else if (!strcasecmp(key, "keep-fec-enabled")) { /* encoder */
				opus_prefs.keep_fec = atoi(val);
			} else if (!strcasecmp(key, "advertise-useinbandfec")) { /*decoder, has meaning only for FMTP: useinbandfec=1 by default */
				opus_prefs.fec_decode = atoi(val);
			} else if (!strcasecmp(key, "adjust-bitrate")) { /* encoder, this setting will make the encoder adjust its bitrate based on a feedback loop (RTCP). This is not "VBR".*/
				opus_prefs.adjust_bitrate = atoi(val);
			} else if (!strcasecmp(key, "maxaveragebitrate")) {
				opus_prefs.maxaveragebitrate = atoi(val);
				if (opus_prefs.maxaveragebitrate < SWITCH_OPUS_MIN_BITRATE || opus_prefs.maxaveragebitrate > SWITCH_OPUS_MAX_BITRATE) {
					opus_prefs.maxaveragebitrate = 0; /* values outside the range between 6000 and 510000 SHOULD be ignored */
				}
			} else if (!strcasecmp(key, "maxplaybackrate")) {
				opus_prefs.maxplaybackrate = atoi(val);
				if (!switch_opus_acceptable_rate(opus_prefs.maxplaybackrate)) {
					opus_prefs.maxplaybackrate = 0; /* value not supported */
				}
			} else if (!strcasecmp(key, "sprop-maxcapturerate")) {
				opus_prefs.sprop_maxcapturerate = atoi(val);
				if (!switch_opus_acceptable_rate(opus_prefs.sprop_maxcapturerate)) {
					opus_prefs.sprop_maxcapturerate = 0; /* value not supported */
				}
			}
		}
	}

	switch_xml_free(xml);

	return status;
}

static switch_status_t switch_opus_keep_fec_enabled(switch_codec_t *codec)
{
	struct opus_context *context = codec->private_info;
	opus_int32 current_bitrate;
	opus_int32 current_loss;
	uint32_t LBRR_threshold_bitrate,LBRR_rate_thres_bps,real_target_bitrate ;
	opus_int32 a32,b32;
	uint32_t fs = context->enc_frame_size * 1000 / (codec->implementation->microseconds_per_packet / 1000);
	float frame_rate =(float)(1000 / (codec->implementation->microseconds_per_packet / 1000));
	uint32_t step = (codec->implementation->microseconds_per_packet / 1000) != 60 ? 8000 / (codec->implementation->microseconds_per_packet / 1000 ) : 134 ;

	opus_encoder_ctl(context->encoder_object, OPUS_GET_BITRATE(&current_bitrate));
	opus_encoder_ctl(context->encoder_object, OPUS_GET_PACKET_LOSS_PERC(&current_loss));

	if (current_loss == 0) {
		opus_encoder_ctl(context->encoder_object, OPUS_SET_BITRATE(opus_prefs.maxaveragebitrate));

		return SWITCH_STATUS_SUCCESS;
	}

	if (fs == 8000) {
		LBRR_rate_thres_bps = 12000; /*LBRR_NB_MIN_RATE_BPS*/
	} else if (fs == 12000) {
		LBRR_rate_thres_bps = 14000; /*LBRR_MB_MIN_RATE_BPS*/
	} else {
		LBRR_rate_thres_bps = 16000; /*LBRR_WB_MIN_RATE_BPS*/
	}
	/*see opus-1.1/src/opus_encoder.c , opus_encode_native() */
	real_target_bitrate =  (uint32_t)(8 * (current_bitrate * context->enc_frame_size / ( fs * 8 ) - 1) * frame_rate );
	/*check if the internally used bitrate is above the threshold defined in opus-1.1/silk/control_codec.c  */
	a32 =  LBRR_rate_thres_bps * (125 -(((current_loss) < (25)) ? (current_loss) :  (25)));
	b32 =  ((opus_int32)((0.01) * ((opus_int64)1 << (16)) + 0.5));
	LBRR_threshold_bitrate =  (a32 >> 16) * (opus_int32)((opus_int16)b32) + (((a32 & 0x0000FFFF) * (opus_int32)((opus_int16)b32)) >> 16);

	if (!real_target_bitrate || !LBRR_threshold_bitrate) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Opus encoder: error while controlling FEC params\n");

		return SWITCH_STATUS_FALSE;
	}
	/* Is there any FEC at the current bitrate and requested packet loss ?
	 * If yes, then keep the current bitrate. If not, modify bitrate to keep FEC on. */
	if (real_target_bitrate > LBRR_threshold_bitrate) {
		/*FEC is already enabled, do nothing*/
		if (globals.debug || context->debug) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Opus encoder: FEC is enabled\n");
		}
		return SWITCH_STATUS_SUCCESS;
	} else {
		while (real_target_bitrate <= LBRR_threshold_bitrate) {
			current_bitrate += step;
			real_target_bitrate =  (uint32_t)(8 * (current_bitrate * context->enc_frame_size / ( fs * 8 ) - 1) * frame_rate);
		}

		opus_encoder_ctl(context->encoder_object,OPUS_SET_BITRATE(current_bitrate));

		if (globals.debug || context->debug) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Opus encoder: increased bitrate to [%d] to keep FEC enabled\n", current_bitrate);
		}

		return SWITCH_STATUS_SUCCESS;
	}
}

static switch_status_t switch_opus_control(switch_codec_t *codec,
										   switch_codec_control_command_t cmd,
										   switch_codec_control_type_t ctype,
										   void *cmd_data,
										   switch_codec_control_type_t atype,
										   void *cmd_arg,
										   switch_codec_control_type_t *rtype,
										   void **ret_data)
{
	struct opus_context *context = codec->private_info;

	switch(cmd) {
	case SCC_CODEC_SPECIFIC:
		{
			const char *command = (const char *)cmd_data;
			const char *arg = (const char *)cmd_arg;
			switch_codec_control_type_t reply_type = SCCT_STRING;
			const char *reply = "ERROR INVALID COMMAND";

			if (!zstr(command)) {
				if (!strcasecmp(command, "jb_lookahead")) {
					if (!zstr(arg)) {
						context->use_jb_lookahead = switch_true(arg);
					}
					reply = context->use_jb_lookahead ? "LOOKAHEAD ON" : "LOOKAHEAD OFF";
				}
			}

			if (rtype) {
				*rtype = reply_type;

				*ret_data = (void *)reply;
			}

		}
		break;
	case SCC_DEBUG:
		{
			int32_t level = *((uint32_t *) cmd_data);
			context->debug = level;
		}
		break;
	case SCC_AUDIO_PACKET_LOSS:
		{
			uint32_t plpct = *((uint32_t *) cmd_data);
			uint32_t calc;

			if (plpct > 100) {
				plpct = 100;
			}

			if (opus_prefs.keep_fec) {
				opus_encoder_ctl(context->encoder_object, OPUS_SET_PACKET_LOSS_PERC(plpct));
			} else {
				calc = plpct % 10;
				plpct = plpct - calc + ( calc ? 10 : 0);
			}
			if (plpct != context->old_plpct) {
				if (opus_prefs.keep_fec) {
					if (plpct > 10) {
					/* this will increase bitrate a little bit, just to keep FEC enabled */
						switch_opus_keep_fec_enabled(codec);
					}
				} else {
					/* this can have no effect because FEC is F(bitrate,packetloss), let the codec decide if FEC is to be used or not */
					opus_encoder_ctl(context->encoder_object, OPUS_SET_PACKET_LOSS_PERC(plpct));
				}

				if (globals.debug || context->debug) {
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Opus encoder: Adjusting packet loss percent from %d%% to %d%%!\n",
									  context->old_plpct, plpct);
				}
			}
			if (opus_prefs.adjust_bitrate) {
				/* make bitrate adjust the step , but keep it as a  multiple of 400 (see OpusFAQ).
				 * usual RTCP interval is 5 seconds  which is long time - the step should be bigger. */
				/* step's value should depend on packet loss too, to decrease more abrubtly
				 * at high packet loss. */
				int base_step = 400; /*bps*/
				int range = context->codec_settings.maxaveragebitrate - SWITCH_OPUS_MIN_BITRATE;
				float steps = (float)((float)(range / 100) / base_step);
				int br_step = (int)(round(steps) * base_step) * plpct;
				if (globals.debug || context->debug) {
					switch_log_printf(SWITCH_CHANNEL_LOG,
							SWITCH_LOG_DEBUG, "Opus encoder: bitrate increase/decrease step now is: %d bps, range:%d\n", br_step, range);
				}
				context->control_state.increase_step = context->control_state.decrease_step = br_step;
			}
			context->old_plpct = plpct;
		}
		break;
	case SCC_AUDIO_ADJUST_BITRATE:
		{
			const char *cmd = (const char *)cmd_data;

			if (!zstr(cmd)) {
				opus_int32 current_bitrate=context->control_state.current_bitrate;
				if (!strcasecmp(cmd, "increase")) {
					/* https://wiki.xiph.org/OpusFAQ
					"[...]Opus scales from about 6 to 512 kb/s, in increments of 0.4 kb/s (one byte with 20 ms frames).
					Opus can have more than 1200 possible bitrates[...]" */
					int br_step = context->control_state.increase_step?context->control_state.increase_step:400;
					opus_encoder_ctl(context->encoder_object, OPUS_GET_BITRATE(&current_bitrate));
					if (opus_prefs.maxaveragebitrate > current_bitrate) {
						opus_encoder_ctl(context->encoder_object, OPUS_SET_BITRATE(current_bitrate+br_step));
						if ((context->control_state.keep_fec) && (current_bitrate > SWITCH_OPUS_MIN_FEC_BITRATE)) {
							opus_prefs.keep_fec = 1; /* enable back FEC if it was disabled by SCC_AUDIO_ADJUST_BITRATE, we have enough network bandwidth now */
						}
						if (globals.debug || context->debug) {
							switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Opus encoder: Adjusting bitrate to %d (increase)\n", current_bitrate+br_step);
						}
					}
				} else if (!strcasecmp(cmd, "decrease")) {
					int br_step = context->control_state.decrease_step?context->control_state.decrease_step:400;
					opus_encoder_ctl(context->encoder_object, OPUS_GET_BITRATE(&current_bitrate));
					if (current_bitrate > SWITCH_OPUS_MIN_BITRATE) {
						if (context->control_state.keep_fec) {
							/* no point to try to keep FEC enabled anymore,
							 * we're low on network bandwidth (that's why we ended up here) */
							opus_prefs.keep_fec = 0;
						}
						opus_encoder_ctl(context->encoder_object, OPUS_SET_BITRATE(current_bitrate-br_step));
						if (globals.debug || context->debug) {
							switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Opus encoder: Adjusting bitrate to %d (decrease)\n", current_bitrate-br_step);
						}
					}
				} else if (!strcasecmp(cmd, "default")) {
					/*restore default bitrate */
					opus_encoder_ctl(context->encoder_object, OPUS_SET_BITRATE(opus_prefs.maxaveragebitrate));
					if (context->control_state.keep_fec) {
						opus_prefs.keep_fec = 1; /* enable back FEC, we have enough network bandwidth now */
					}
					if (globals.debug || context->debug) {
						switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Opus encoder: Adjusting bitrate to %d (configured maxaveragebitrate)\n", opus_prefs.maxaveragebitrate);
					}
				} else {
					/* set Opus minimum bitrate */
					opus_encoder_ctl(context->encoder_object, OPUS_SET_BITRATE(SWITCH_OPUS_MIN_BITRATE));
					if (context->control_state.keep_fec) {
						opus_prefs.keep_fec = 0; /* do not enforce FEC anymore, we're low on network bandwidth */
					}
					if (globals.debug || context->debug) {
						switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Opus encoder: Adjusting bitrate to %d (minimum)\n", SWITCH_OPUS_MIN_BITRATE);
					}
				}
			}
		}
		break;
	default:
		break;
	}

	return SWITCH_STATUS_SUCCESS;
}
#define OPUS_DEBUG_SYNTAX "<on|off>"
SWITCH_STANDARD_API(mod_opus_debug)
{
	if (zstr(cmd)) {
		stream->write_function(stream, "-USAGE: %s\n", OPUS_DEBUG_SYNTAX);
	} else {
		if (!strcasecmp(cmd, "on")) {
			globals.debug = 1;
			stream->write_function(stream, "OPUS Debug: on\n");
			stream->write_function(stream, "Library version: %s\n",opus_get_version_string());
		} else if (!strcasecmp(cmd, "off")) {
			globals.debug = 0;
			stream->write_function(stream, "OPUS Debug: off\n");
		} else {
			stream->write_function(stream, "-USAGE: %s\n", OPUS_DEBUG_SYNTAX);
		}
	}

	return SWITCH_STATUS_SUCCESS;
}


SWITCH_MODULE_LOAD_FUNCTION(mod_opus_load)
{
	switch_codec_interface_t *codec_interface;
	switch_api_interface_t *commands_api_interface;
	int samples = 480;
	int bytes = 960;
	int mss = 10000;
	int x = 0;
	int rate = 48000;
	int bits = 0;
	char *dft_fmtp = NULL;
	opus_codec_settings_t settings = { 0 };
	switch_status_t status = SWITCH_STATUS_SUCCESS;

	if ((status = opus_load_config(SWITCH_FALSE)) != SWITCH_STATUS_SUCCESS) {
		return status;
	}

	/* connect my internal structure to the blank pointer passed to me */
	*module_interface = switch_loadable_module_create_module_interface(pool, modname);

	SWITCH_ADD_CODEC(codec_interface, "OPUS (STANDARD)");
	SWITCH_ADD_API(commands_api_interface, "opus_debug", "Set OPUS Debug", mod_opus_debug, OPUS_DEBUG_SYNTAX);

	switch_console_set_complete("add opus_debug on");
	switch_console_set_complete("add opus_debug off");

	codec_interface->parse_fmtp = switch_opus_fmtp_parse;

	/* 48 khz */
	settings = default_codec_settings;

	settings.useinbandfec = opus_prefs.fec_decode;

	settings.cbr = !opus_prefs.use_vbr;

	settings.usedtx = opus_prefs.use_dtx;

	if (opus_prefs.maxaveragebitrate) {
		settings.maxaveragebitrate = opus_prefs.maxaveragebitrate;
	}

	if (opus_prefs.maxplaybackrate) {
		settings.maxplaybackrate = opus_prefs.maxplaybackrate;
	}

	if (opus_prefs.sprop_maxcapturerate) {
		settings.sprop_maxcapturerate = opus_prefs.sprop_maxcapturerate;
	}

	for (x = 0; x < 3; x++) {

		settings.ptime = mss / 1000;
		settings.samplerate = rate;
		settings.stereo = 0;
		dft_fmtp = gen_fmtp(&settings, pool);

		switch_core_codec_add_implementation(pool, codec_interface, SWITCH_CODEC_TYPE_AUDIO,	/* enumeration defining the type of the codec */
											 116,	/* the IANA code number */
											 "opus",/* the IANA code name */
											 dft_fmtp,	/* default fmtp to send (can be overridden by the init function) */
											 48000,	/* samples transferred per second */
											 rate,	/* actual samples transferred per second */
											 bits,	/* bits transferred per second */
											 mss,	/* number of microseconds per frame */
											 samples,	/* number of samples per frame */
											 bytes,	/* number of bytes per frame decompressed */
											 0,	/* number of bytes per frame compressed */
											 1,/* number of channels represented */
											 1,	/* number of frames per network packet */
											 switch_opus_init,	/* function to initialize a codec handle using this implementation */
											 switch_opus_encode,	/* function to encode raw data into encoded data */
											 switch_opus_decode,	/* function to decode encoded data into raw data */
											 switch_opus_destroy);	/* deinitalize a codec handle using this implementation */

		codec_interface->implementations->codec_control = switch_opus_control;

		settings.stereo = 1;

		dft_fmtp = gen_fmtp(&settings, pool);
		switch_core_codec_add_implementation(pool, codec_interface, SWITCH_CODEC_TYPE_AUDIO,	/* enumeration defining the type of the codec */
											 116,	/* the IANA code number */
											 "opus",/* the IANA code name */
											 dft_fmtp,	/* default fmtp to send (can be overridden by the init function) */
											 rate,	/* samples transferred per second */
											 rate,	/* actual samples transferred per second */
											 bits,	/* bits transferred per second */
											 mss,	/* number of microseconds per frame */
											 samples,	/* number of samples per frame */
											 bytes * 2,	/* number of bytes per frame decompressed */
											 0,	/* number of bytes per frame compressed */
											 2,/* number of channels represented */
											 1,	/* number of frames per network packet */
											 switch_opus_init,	/* function to initialize a codec handle using this implementation */
											 switch_opus_encode,	/* function to encode raw data into encoded data */
											 switch_opus_decode,	/* function to decode encoded data into raw data */
											 switch_opus_destroy);	/* deinitalize a codec handle using this implementation */
		codec_interface->implementations->codec_control = switch_opus_control;

		bytes *= 2;
		samples *= 2;
		mss *= 2;
	}

	/* 16 khz */
	samples = 480;
	bytes = 320;
	mss = 10000;
	rate = 16000;

	settings = default_codec_settings_16k;

	settings.useinbandfec = opus_prefs.fec_decode;

	settings.cbr = !opus_prefs.use_vbr;

	settings.usedtx = opus_prefs.use_dtx;

	if (opus_prefs.maxaveragebitrate) {
		settings.maxaveragebitrate = opus_prefs.maxaveragebitrate;
	}
	if (opus_prefs.maxplaybackrate) {
		settings.maxplaybackrate = opus_prefs.maxplaybackrate;
	}
	if (opus_prefs.sprop_maxcapturerate) {
		settings.sprop_maxcapturerate = opus_prefs.sprop_maxcapturerate;
	}

	for (x = 0; x < 3; x++) {
		settings.stereo = 0;
		settings.ptime = mss / 1000;
		settings.samplerate = rate;
		dft_fmtp = gen_fmtp(&settings, pool);

		switch_core_codec_add_implementation(pool, codec_interface, SWITCH_CODEC_TYPE_AUDIO,	/* enumeration defining the type of the codec */
											 116,	/* the IANA code number */
											 "opus",/* the IANA code name */
											 dft_fmtp,	/* default fmtp to send (can be overridden by the init function) */
											 48000,	/* samples transferred per second */
											 rate,	/* actual samples transferred per second */
											 bits,	/* bits transferred per second */
											 mss,	/* number of microseconds per frame */
											 samples,	/* number of samples per frame */
											 bytes,	/* number of bytes per frame decompressed */
											 0,	/* number of bytes per frame compressed */
											 1,/* number of channels represented */
											 1,	/* number of frames per network packet */
											 switch_opus_init,	/* function to initialize a codec handle using this implementation */
											 switch_opus_encode,	/* function to encode raw data into encoded data */
											 switch_opus_decode,	/* function to decode encoded data into raw data */
											 switch_opus_destroy);	/* deinitalize a codec handle using this implementation */
		codec_interface->implementations->codec_control = switch_opus_control;
		settings.stereo = 1;
		dft_fmtp = gen_fmtp(&settings, pool);
		switch_core_codec_add_implementation(pool, codec_interface, SWITCH_CODEC_TYPE_AUDIO,	/* enumeration defining the type of the codec */
											 116,	/* the IANA code number */
											 "opus",/* the IANA code name */
											 dft_fmtp,	/* default fmtp to send (can be overridden by the init function) */
											 48000,	/* samples transferred per second */
											 rate,	/* actual samples transferred per second */
											 bits,	/* bits transferred per second */
											 mss,	/* number of microseconds per frame */
											 samples,	/* number of samples per frame */
											 bytes * 2,	/* number of bytes per frame decompressed */
											 0,	/* number of bytes per frame compressed */
											 2,/* number of channels represented */
											 1,	/* number of frames per network packet */
											 switch_opus_init,	/* function to initialize a codec handle using this implementation */
											 switch_opus_encode,	/* function to encode raw data into encoded data */
											 switch_opus_decode,	/* function to decode encoded data into raw data */
											 switch_opus_destroy);	/* deinitalize a codec handle using this implementation */
		codec_interface->implementations->codec_control = switch_opus_control;
		if (x == 1) { /*20 ms * 3  = 60 ms */
			settings.stereo = 0;
			settings.ptime = mss * 3 / 1000;
			dft_fmtp = gen_fmtp(&settings, pool);
			switch_core_codec_add_implementation(pool, codec_interface, SWITCH_CODEC_TYPE_AUDIO,	/* enumeration defining the type of the codec */
												 116,	/* the IANA code number */
												 "opus",/* the IANA code name */
												 dft_fmtp,	/* default fmtp to send (can be overridden by the init function) */
												 48000,	/* samples transferred per second */
												 rate,	/* actual samples transferred per second */
												 bits,	/* bits transferred per second */
												 mss * 3,	/* number of microseconds per frame */
												 samples * 3,	/* number of samples per frame */
												 bytes * 3,	/* number of bytes per frame decompressed */
												 0,	/* number of bytes per frame compressed */
												 1,/* number of channels represented */
												 1,	/* number of frames per network packet */
												 switch_opus_init,	/* function to initialize a codec handle using this implementation */
												 switch_opus_encode,	/* function to encode raw data into encoded data */
												 switch_opus_decode,	/* function to decode encoded data into raw data */
												 switch_opus_destroy);	/* deinitalize a codec handle using this implementation */
			codec_interface->implementations->codec_control = switch_opus_control;
		}

		bytes *= 2;
		samples *= 2;
		mss *= 2;
	}

	/* 8 khz */
	samples = 480;
	bytes = 160;
	mss = 10000;
	rate = 8000;

	settings = default_codec_settings_8k;

	settings.useinbandfec = opus_prefs.fec_decode;

	settings.cbr = !opus_prefs.use_vbr;

	settings.usedtx = opus_prefs.use_dtx;

	if (opus_prefs.maxaveragebitrate) {
		settings.maxaveragebitrate = opus_prefs.maxaveragebitrate;
	}
	if (opus_prefs.maxplaybackrate) {
		settings.maxplaybackrate = opus_prefs.maxplaybackrate;
	}
	if (opus_prefs.sprop_maxcapturerate) {
		settings.sprop_maxcapturerate = opus_prefs.sprop_maxcapturerate;
	}

	for (x = 0; x < 3; x++) {
		settings.stereo = 0;
		settings.ptime = mss / 1000;
		settings.samplerate = rate;
		dft_fmtp = gen_fmtp(&settings, pool);

		switch_core_codec_add_implementation(pool, codec_interface, SWITCH_CODEC_TYPE_AUDIO,	/* enumeration defining the type of the codec */
											 116,	/* the IANA code number */
											 "opus",/* the IANA code name */
											 dft_fmtp,	/* default fmtp to send (can be overridden by the init function) */
											 48000,	/* samples transferred per second */
											 rate,	/* actual samples transferred per second */
											 bits,	/* bits transferred per second */
											 mss,	/* number of microseconds per frame */
											 samples,	/* number of samples per frame */
											 bytes,	/* number of bytes per frame decompressed */
											 0,	/* number of bytes per frame compressed */
											 1,/* number of channels represented */
											 1,	/* number of frames per network packet */
											 switch_opus_init,	/* function to initialize a codec handle using this implementation */
											 switch_opus_encode,	/* function to encode raw data into encoded data */
											 switch_opus_decode,	/* function to decode encoded data into raw data */
											 switch_opus_destroy);	/* deinitalize a codec handle using this implementation */
		codec_interface->implementations->codec_control = switch_opus_control;
		settings.stereo = 1;
		dft_fmtp = gen_fmtp(&settings, pool);
		switch_core_codec_add_implementation(pool, codec_interface, SWITCH_CODEC_TYPE_AUDIO,	/* enumeration defining the type of the codec */
											 116,	/* the IANA code number */
											 "opus",/* the IANA code name */
											 dft_fmtp,	/* default fmtp to send (can be overridden by the init function) */
											 48000,	/* samples transferred per second */
											 rate,	/* actual samples transferred per second */
											 bits,	/* bits transferred per second */
											 mss,	/* number of microseconds per frame */
											 samples,	/* number of samples per frame */
											 bytes * 2,	/* number of bytes per frame decompressed */
											 0,	/* number of bytes per frame compressed */
											 2,/* number of channels represented */
											 1,	/* number of frames per network packet */
											 switch_opus_init,	/* function to initialize a codec handle using this implementation */
											 switch_opus_encode,	/* function to encode raw data into encoded data */
											 switch_opus_decode,	/* function to decode encoded data into raw data */
											 switch_opus_destroy);	/* deinitalize a codec handle using this implementation */
		codec_interface->implementations->codec_control = switch_opus_control;
		if (x == 1) { /*20 ms * 3  = 60 ms */
			int nb_frames;
			settings.stereo = 0;
			settings.ptime = mss * 3 / 1000;
			dft_fmtp = gen_fmtp(&settings, pool);
			switch_core_codec_add_implementation(pool, codec_interface, SWITCH_CODEC_TYPE_AUDIO,	/* enumeration defining the type of the codec */
												 116,	/* the IANA code number */
												 "opus",/* the IANA code name */
												 dft_fmtp,	/* default fmtp to send (can be overridden by the init function) */
												 48000,	/* samples transferred per second */
												 rate,	/* actual samples transferred per second */
												 bits,	/* bits transferred per second */
												 mss * 3,	/* number of microseconds per frame */
												 samples * 3,	/* number of samples per frame */
												 bytes * 3,	/* number of bytes per frame decompressed */
												 0,	/* number of bytes per frame compressed */
												 1,/* number of channels represented */
												 1,	/* number of frames per network packet */
												 switch_opus_init,	/* function to initialize a codec handle using this implementation */
												 switch_opus_encode,	/* function to encode raw data into encoded data */
												 switch_opus_decode,	/* function to decode encoded data into raw data */
												 switch_opus_destroy);	/* deinitalize a codec handle using this implementation */
			codec_interface->implementations->codec_control = switch_opus_control;

			for (nb_frames = 4; nb_frames <= 6; nb_frames++) {
				/*20 ms * nb_frames  = 80 ms , 100 ms , 120 ms */
				settings.stereo = 0;
				settings.ptime = mss * nb_frames / 1000;
				dft_fmtp = gen_fmtp(&settings, pool);
				switch_core_codec_add_implementation(pool, codec_interface, SWITCH_CODEC_TYPE_AUDIO,	/* enumeration defining the type of the codec */
													 116,	/* the IANA code number */
													 "opus",/* the IANA code name */
													 dft_fmtp,	/* default fmtp to send (can be overridden by the init function) */
													 48000,	/* samples transferred per second */
													 rate,	/* actual samples transferred per second */
													 bits,	/* bits transferred per second */
													 mss * nb_frames,	/* number of microseconds per frame */
													 samples * nb_frames,	/* number of samples per frame */
													 bytes * nb_frames,	/* number of bytes per frame decompressed */
													 0,	/* number of bytes per frame compressed */
													 1,/* number of channels represented */
													 1,	/* number of frames per network packet */
													 switch_opus_init,	/* function to initialize a codec handle using this implementation */
													 switch_opus_encode_repacketize,	/* function to encode raw data into encoded data */
													 switch_opus_decode,	/* function to decode encoded data into raw data */
													 switch_opus_destroy);	/* deinitalize a codec handle using this implementation */
				codec_interface->implementations->codec_control = switch_opus_control;

			}

		}

		bytes *= 2;
		samples *= 2;
		mss *= 2;
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
