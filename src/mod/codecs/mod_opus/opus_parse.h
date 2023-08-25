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
 * Julien Chavanton <jchavanton@gmail.com>
 *
 */

#ifndef SWITCH_OPUS_PARSE_H
#define SWITCH_OPUS_PARSE_H

typedef enum { false, true } bool_t;

typedef struct opus_packet_info {
	int16_t vad;
	int16_t vad_ms;
	int16_t fec;
	int16_t fec_ms;
	bool_t stereo;
	/* number of opus frames in the packet */
	int16_t frames;
	int16_t config;
	int16_t channels;
	int16_t ms_per_frame;
	int32_t ptime_ts;
	bool_t valid;
	/* number of silk_frames in an opus frame */
	int16_t frames_silk;
	/* VAD flag of all 20 ms silk-frames of the packet; LSB the first frame, MSB: the last */
	int16_t vad_flags_per_silk_frame;
	/* LBRR (FEC) flag of all 20 ms silk-frames of the packet; LSB the first frame, MSB: the last */
	int16_t fec_flags_per_silk_frame;
} opus_packet_info_t;

switch_bool_t switch_opus_packet_parse(const uint8_t *payload, int payload_length_bytes, opus_packet_info_t *packet_info, switch_bool_t debug);

#endif /* SWITCH_OPUS_PARSE_H */

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
