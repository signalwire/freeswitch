/*
 * FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
 * Copyright (C) 2005-2023, Anthony Minessale II <anthm@freeswitch.org>
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
 * Claude Lamblin
 * Julien Chavanton <jchavanton@gmail.com>
 *
 */

#include "switch.h"
#include "opus.h"
#include "opus_parse.h"
/* Tables for LBRR_sympbol decoding */

static const opus_int16 silk_LBRR_flags_2_PDFCum[3] = { 53, 106, 256 }; 				  /* 256 - silk_LBRR_flags_2_iCDF[i] ; silk_LBRR_flags_2_iCDF[ 3 ] = { 203, 150, 0 }; */
static const opus_int16 silk_LBRR_flags_3_PDFCum[7] = { 41, 61, 90, 131, 146, 174, 256 }; /* 256 - silk_LBRR_flags_3_iCDF[i] ; silk_LBRR_flags_3_iCDF[ 7 ] = { 215, 195, 166, 125, 110, 82, 0 }; */

/* Get the number of VAD flags - i.e. number of 20 ms frame - from the config
 * in a silk-only  or hybrid opus frame  mono or stereo
 * 5 MSB TOC byte (see table 2 of IETF RFC6716  clause 3.1)
 * if 10 ms frame (config=0, 4, 8, 12, 14) : return 1
 * if CELT_only frame no VAD flag =>return 0 */
static opus_int16 switch_opus_get_nb_flags_in_silk_frame(int16_t config)
{
	opus_int16 silk_frame_nb_flags;

	if (config > 15) {
		/* CELT_only frame no VAD flag nor LBRR flag */
		silk_frame_nb_flags = 0;
	} else {
		silk_frame_nb_flags = 1;  /* default */

		if (config < 12) {
			/* silk-only NB, MB or WB
			 * The least two significant bits give the number of VAD flags inside the silk frame 1, 2 or 3 */
			silk_frame_nb_flags = config & 0x3;

			if (silk_frame_nb_flags == 0) { /* 0  => 10ms frame : one  VAD flag */
				silk_frame_nb_flags++;
			}
		}
	}

	return silk_frame_nb_flags;
}

/* Get the time in ms corresponding to one VAD flag from the config
 * in a silk-only  or hybrid opus frame  mono or stereo
 * 5 MSB TOC byte (see table 2 of IETF RFC6716  clause 3.1)
 * if CELT_only frame (config >15) no VAD flag =>return FALSE
 * if 10 ms frame (config=0, 4, 8, 12, 14) : return 10
 * otherwise return 20 */
static opus_int16 switch_opus_get_silk_frame_ms_per_flag(int16_t config, opus_int16 silk_frame_nb_flags)
{
	opus_int16 silk_size_frame_ms_per_flag;

	if (config > 15) {
		/* CELT_only frame no VAD flag nor LBRR flag */
		/* switch_opus_get_silk_frame_ms_per_flag: code not written for CELT-only mode */

		return 0;
	}

	silk_size_frame_ms_per_flag = 20;  /* default*/
	if (silk_frame_nb_flags == 1) {    /* could be 10 or 20 ms */
		if ((config & 0x01) == 0) {
			silk_size_frame_ms_per_flag = 10;
		}
	}

	return silk_size_frame_ms_per_flag;
}

/* Code written only for mono, silk-only or hybrid mode
 * for CELT-only frame no vad flags for LBRR flag the routine must not be called
 * for stereo : the mid frame VAD_flags and the LBRR_flag could be obtained
 * yet, to get the LBRR_flags of the mid frame the routine should be modified
 * to skip the side VAD flags and the side LBRR flag and to get the mid LBRR_symbol */
static switch_bool_t switch_opus_get_VAD_LBRR_flags(const uint8_t *buf, opus_int16 buf_size, opus_int16 silk_frame_nb_flags,
										   opus_int16 *VAD_flags, opus_int16 *LBRR_flags, opus_int16 *nb_VAD1, opus_int16 *nb_FEC)
{
	const opus_int16 *ptr_pdf_cum;
	opus_int nb_pdf_symbol;
	opus_uint16 LBRR_symbol;
	opus_int16 val, nb_bit, compl_nb_bit, mask, mask2;
	opus_int16 *ptr_flags;
	opus_int16 LBRR_flag;
	opus_int16 nb_vad, nb_fec;
	int i;

	if (buf_size <= 0) {
		return SWITCH_FALSE;
	}

	nb_vad = 0;
	nb_fec = 0;

	/* Get VAD_FLAGS & LBRR_FLAG
	 * silk_frame_nb_flags  = 1  (10 or 20 ms), the two MSB of the first byte are the VAD flag and the LBRR flag
	 * silk_frame_nb_flags  = 2  (40 ms), the three MSB of the first byte are the two VAD flags and the LBRR flag
	 * silk_frame_nb_flags  = 3  (60 ms), the four MSB of the first byte are the three VAD flags and the LBRR flag
	 * compute the number of MSB to analyze */
	nb_bit = silk_frame_nb_flags + 1;

	/* number of right shifts to apply to the first byte to only have the bits of LBRR flag and of the VAD flags */
	compl_nb_bit = 8 - nb_bit;
	mask = (1 << nb_bit) - 1;

	/* The bits of the silk_frame_nb_flags VAD flags and the LBRR flag are the MSB of the first byte
	 * silk_frame_nb_flags  = 1  (10 or 20 ms),  VAD_flags(0) | LBRR_flag
	 * silk_frame_nb_flags  = 2  (40 ms), VAD_flags(0) | VAD_flags(1) | LBRR_flag
	 * silk_frame_nb_flags  = 3  (60 ms), VAD_flags(0) | VAD_flags(1) | VAD_flags(2) |LBRR_flag */
	val = (buf[0] >> compl_nb_bit) & mask;

	LBRR_flag = val & 0x1; /* LBRR_FLAG LSB */

	/* get VAD_flags  */
	ptr_flags = VAD_flags + silk_frame_nb_flags;
	for (i = 0; i < silk_frame_nb_flags; i++) {
		LBRR_flags[i] = 0; /* init */
		val >>= 1;
		*(--ptr_flags) = val & 0x1;
	}

	if (LBRR_flag != 0) { /* there is at least one LBRR frame */
		if (silk_frame_nb_flags == 1) {
			LBRR_flags[0] = 1;
			nb_fec = 1;
		} else if (buf_size < 2) {
			/* not enough data in the buffer to get LBRR_symbol and LBRR_flags */
			return SWITCH_FALSE;
		} else { /* get LBRR_symbol  then LBRR_flags */
			/* LBRR symbol is encoded with range encoder : range on 8 bits
			 * silk_frame_nb_flags  = 2  ; 3 possible values for LBRR_flags(1) | LBRR_flags(0))=  01, 10, 11
			 * silk_frame_nb_flags  = 3  ; 7 possible values for LBRR_flags(2) | LBRR_flags(1) | LBRR_flags(0))=  001, 010, 011, 100, 101, 110, 111 */
			mask2 = (1 << compl_nb_bit) - 1;
			/* get next 8 bits: (8-nb_bit) LSB of the first byte  and nb_bit MSB of the second byte */
			val = (((buf[0]) & mask2) << nb_bit) | ((buf[1] >> compl_nb_bit) & mask);

			if (silk_frame_nb_flags == 2) {
				nb_pdf_symbol = 3;
				ptr_pdf_cum = silk_LBRR_flags_2_PDFCum;
			} else {
				nb_pdf_symbol = 7;
				ptr_pdf_cum = silk_LBRR_flags_3_PDFCum;
			}

			LBRR_symbol = 0;
			for (i = 1; i <= nb_pdf_symbol; i++) {
				if (val < *ptr_pdf_cum++) {
					LBRR_symbol = i;
					break;
				}
			}

			for (i = 0; i < silk_frame_nb_flags; i++) {
				LBRR_flags[i] = LBRR_symbol & 0x01;
				LBRR_symbol >>= 1;
				nb_fec += LBRR_flags[i];
			}
		}
	}

	for (i = 0; i < silk_frame_nb_flags; i++) {
		nb_vad += VAD_flags[i];
	}

	*nb_VAD1 = nb_vad;
	*nb_FEC = nb_fec;

	return SWITCH_TRUE;
}

/* Parse the packet to retrieve informations about its content
 * RFC6716: Definition of the Opus Audio Codec
 * return: SWITCH_FALSE if there was a problem found parsing the packet, the info returned should be ignored.
 * */
switch_bool_t switch_opus_packet_parse(const uint8_t *payload, int payload_length_bytes, opus_packet_info_t *packet_info, switch_bool_t debug)
{
	int f;
	int32_t samplerate;
	int i, shift_silk, silk_frame_packet;
	int16_t vad_flags_per_silk_frame, fec_flags_per_silk_frame;
	opus_int16 frame_sizes[48];
	const unsigned char *frame_data[48];
	opus_int16 packet_LBBR_FLAGS[3 * 48] = { 0 }, packet_VAD_FLAGS[3 * 48] = { 0 };
	opus_int16 *ptr_LBBR_FLAGS, *ptr_VAD_FLAGS;
	opus_int16 silk_frame_nb_flags, silk_size_frame_ms_per_flag;
	opus_int16 silk_frame_nb_fec, silk_frame_nb_vad1;
	opus_int sample_per_frame;

	packet_info->config = 0;
	packet_info->fec = 0;
	packet_info->fec_ms = 0;
	packet_info->vad = 0;
	packet_info->vad_ms = 0;
	packet_info->stereo = FALSE;
	packet_info->frames = 0;
	packet_info->channels = 1; /* as stereo is set to FALSE */
	packet_info->ms_per_frame = 0;
	packet_info->ptime_ts = 0;

	if (payload == NULL || payload_length_bytes <= 0) {
		if (debug) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "opus_packet_parse: payload null.");
		}

		return SWITCH_FALSE;
	}

	/* In CELT_ONLY mode, packets should not have FEC. */
	if (payload[0] & 0x80) {
		/* opus_packet_parse: CELT_ONLY mode, we do not support this mode. */
		return SWITCH_FALSE;
	} else {
		int mode = (payload[0] >> 3);

		if (mode <= 3) {
			samplerate = 8000;
		} else if (mode <= 7) {
			samplerate = 12000;
		} else if (mode <= 11) {
			samplerate = 16000;
		} else if (mode <= 13) {
			samplerate = 24000;
		} else if (mode <= 15) {
			samplerate = 48000;
		} else {
			/* opus_packet_parse: CELT_ONLY mode, we do not support this mode. */
			return SWITCH_FALSE;
		}
		if (debug) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "opus_packet_parse: mode[%d]s[%d]c[%d] [%d]Hz\n", mode, (payload[0] >> 2) & 0x1, (payload[0]) & 0x3, samplerate);
		}
	}

	if (payload[0] & 0x04) {
		packet_info->stereo = TRUE;
		packet_info->channels = 2;
	}

	packet_info->config = payload[0] >> 3;
	sample_per_frame = opus_packet_get_samples_per_frame(payload, samplerate);
	packet_info->ms_per_frame = sample_per_frame * 1000 / samplerate;
	if (packet_info->ms_per_frame < 10 || packet_info->ms_per_frame > 120) {
		if (debug) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "opus_packet_parse: invalid packet.");
		}

		return SWITCH_FALSE;
	}

	packet_info->frames = opus_packet_parse(payload, payload_length_bytes, NULL, frame_data, frame_sizes, NULL);
	if (packet_info->frames < 0) {
		packet_info->frames = 0;
		if (debug) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "opus_packet_parse: opus_packet_parse found no frame.\n");
		}

		return SWITCH_FALSE;
	}

	packet_info->ptime_ts = packet_info->frames * sample_per_frame;

	/* +---------------+-----------+-----------+-------------------+
	   | Configuration | Mode      | Bandwidth | Frame Sizes       |
	   | Number(s)     |           |           |                   |
	   +---------------+-----------+-----------+-------------------+
	   | 0...3         | SILK-only | NB        | 10, 20, 40, 60 ms |
	   | 4...7         | SILK-only | MB        | 10, 20, 40, 60 ms |
	   | 8...11        | SILK-only | WB        | 10, 20, 40, 60 ms |
	   | 12...13       | Hybrid    | SWB       | 10, 20 ms         |
	   | 14...15       | Hybrid    | FB        | 10, 20 ms         |
	   | 16...19       | CELT-only | NB        | 2.5, 5, 10, 20 ms |
	   | 20...23       | CELT-only | WB        | 2.5, 5, 10, 20 ms |
	   | 24...27       | CELT-only | SWB       | 2.5, 5, 10, 20 ms |
	   | 28...31       | CELT-only | FB        | 2.5, 5, 10, 20 ms |
	   +---------------+-----------+-----------+-------------------+ */

	if (!packet_info->stereo) {
		/* The routines opus_get_nb_flags_in_silk_frame and opus_get_silk_frame_ms_per_flag are also valid for stereo frames
		 * yet the routine opus_get_VAD_LBRR_flags is currently only for mono frame */
		silk_frame_nb_flags = switch_opus_get_nb_flags_in_silk_frame(packet_info->config); /* =1 for 10 or 20 ms frame;  = 2 for 40 ms; = 3 for 60 ms */
		if (!silk_frame_nb_flags) {
			/* We should not go there as CELT_ONLY is already tested above */
			return SWITCH_FALSE;
		}

		packet_info->frames_silk = silk_frame_nb_flags;
		silk_size_frame_ms_per_flag = switch_opus_get_silk_frame_ms_per_flag(packet_info->config, silk_frame_nb_flags); /* 10 or 20 ms frame*/
		if (!silk_size_frame_ms_per_flag) {
			/* we should not go there as CELT_ONLY is already tested above */
			return SWITCH_FALSE;
		}

		ptr_LBBR_FLAGS = packet_LBBR_FLAGS;
		ptr_VAD_FLAGS = packet_VAD_FLAGS;

		for (f = 0; f < packet_info->frames; f++) {
			if (!switch_opus_get_VAD_LBRR_flags(frame_data[f], frame_sizes[f], silk_frame_nb_flags, ptr_VAD_FLAGS, ptr_LBBR_FLAGS,
				&silk_frame_nb_vad1, &silk_frame_nb_fec)) {
				continue; /* ignore frame as it was abnormal */
			}

			packet_info->vad += silk_frame_nb_vad1;
			packet_info->fec += silk_frame_nb_fec;
			packet_info->vad_ms += silk_frame_nb_vad1 * silk_size_frame_ms_per_flag;
			packet_info->fec_ms += silk_frame_nb_fec * silk_size_frame_ms_per_flag;

			ptr_VAD_FLAGS += silk_frame_nb_flags;
			ptr_LBBR_FLAGS += silk_frame_nb_flags;
		}

		/* store the VAD & LBRR flags of all 20 ms silk-frames of the packet; LSB the first frame, MSB: the last */
		vad_flags_per_silk_frame = 0;
		fec_flags_per_silk_frame = 0;
		silk_frame_packet = packet_info->frames * packet_info->frames_silk;
		if (silk_frame_packet > 15) {
			if (debug) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "opus_packet_parse: more than %d 20-ms frames in the packet ; only first 15 silk-frames data will be stored (pb silkFastAccelerate)\n", silk_frame_packet);
			}

			silk_frame_packet = 15;
		}

		ptr_LBBR_FLAGS = packet_LBBR_FLAGS;
		ptr_VAD_FLAGS = packet_VAD_FLAGS;
		shift_silk = 0;
		for (i = 0; i < silk_frame_packet; i++) {
			vad_flags_per_silk_frame += (*ptr_VAD_FLAGS) << shift_silk;
			fec_flags_per_silk_frame += (*ptr_LBBR_FLAGS) << shift_silk;
			shift_silk++;
			ptr_LBBR_FLAGS++; ptr_VAD_FLAGS++;
		}

		packet_info->vad_flags_per_silk_frame = vad_flags_per_silk_frame;
		packet_info->fec_flags_per_silk_frame = fec_flags_per_silk_frame;

		return SWITCH_TRUE;
	}

	if (packet_info->config != 1 && packet_info->config != 5 && packet_info->config != 9) {
		if (debug) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "opus_packet_parse: the current parser implementation does not support muliple SILK frames for VAD or FEC detection.\n");
		}

		return SWITCH_FALSE;
	}

	/*
	 *  Parse the VAD and LBRR flags in each Opus frame
	 * */
	for (f = 0; f < packet_info->frames; f++) {
		if (frame_sizes[f] <= 0) continue;

		if (frame_data[f][0] & 0x80) {
			packet_info->vad++;
		}

		if (frame_data[f][0] & 0x40) {
			packet_info->fec++;
		}

		if (debug) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "opus_packet_parse: LP layer opus_frame[%d] VAD[%d] FEC[%d]\n", f + 1, (frame_data[f][0] & 0x80) >> 7, (frame_data[f][0] & 0x40) >> 6);
		}
	}

	packet_info->vad_ms = packet_info->vad * packet_info->ms_per_frame;
	packet_info->fec_ms = packet_info->fec * packet_info->ms_per_frame;

	return SWITCH_TRUE;
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
