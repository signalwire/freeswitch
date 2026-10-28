/*
 * FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
 * Copyright (C) 2005-2026, Anthony Minessale II <anthm@freeswitch.org>
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
 * amr_payload.h -- RTP payload format for AMR and AMR-WB (RFC 4867), shared by mod_amr and mod_amrwb
 * (the two copies of this file are identical).
 *
 *  - octet-aligned and bandwidth-efficient packets with any number of frames (TOC with F bits)
 *  - the optional interleaving header (ILL/ILP, section 4.4.1) and a receive-side de-interleaver
 *
 * A frame is kept in the codec "storage" layout the encoders produce and the decoders take:
 * speech bits MSB first, the last byte zero-padded; the TOC byte is not part of `data`.
 */

#ifndef AMR_PAYLOAD_H
#define AMR_PAYLOAD_H

#include <switch.h>

#define AMR_PL_MAX_FRAME_BYTES 61       /* AMR-WB 23.85: 477 bits */
#define AMR_PL_MAX_FRAMES 32             /* frames in one packet (640 ms of audio) */
#define AMR_PL_MAX_GROUP 64              /* frame-blocks in one interleaving group */
#define AMR_PL_FT_NO_DATA 15

typedef struct {
	const char *name;
	const int *frame_bits;   /* speech bits per frame type 0..15; -1 = frame type not allowed */
	int max_speech_ft;       /* highest speech mode (7 for AMR, 8 for AMR-WB) */
	int samples_per_frame;   /* 160 or 320 */
} amr_pl_desc_t;

typedef struct {
	uint8_t ft;
	uint8_t q;
	uint8_t data[AMR_PL_MAX_FRAME_BYTES];
} amr_pl_frame_t;

typedef struct {
	uint8_t cmr;
	int interleaved;          /* ILL/ILP header present */
	uint8_t ill, ilp;
	int nframes;
	amr_pl_frame_t frames[AMR_PL_MAX_FRAMES];
} amr_pl_packet_t;

/* bytes needed for the speech bits of frame type `ft` (0 for NO_DATA / SPEECH_LOST) */
int amr_pl_frame_bytes(const amr_pl_desc_t *d, int ft);

/* parse an RTP payload; returns SWITCH_STATUS_SUCCESS only if the whole packet is well formed */
switch_status_t amr_pl_parse(const amr_pl_desc_t *d, const uint8_t *buf, int len, int octet_aligned, int interleaved, amr_pl_packet_t *pkt);

/* build an RTP payload; returns its length, or -1 if it does not fit `cap` or is invalid */
int amr_pl_build(const amr_pl_desc_t *d, const amr_pl_packet_t *pkt, int octet_aligned, uint8_t *out, int cap);

/* frame as produced by the encoders ([TOC][speech...]) -> amr_pl_frame_t; -1 on error */
int amr_pl_frame_from_storage(const amr_pl_desc_t *d, const uint8_t *storage, int len, amr_pl_frame_t *f);

/* amr_pl_frame_t -> decoder input ([TOC][speech...]); returns bytes written */
int amr_pl_frame_to_storage(const amr_pl_desc_t *d, const amr_pl_frame_t *f, uint8_t *storage);

/*
 * Receive-side de-interleaver (RFC 4867 4.4.1). A packet with ILL=L, ILP=p carrying N frames holds
 * frame-blocks p, p+(L+1), p+2(L+1), ... of a group of N*(L+1). Frames come out in time order,
 * one group late; frames of packets that never arrived come out as lost.
 */
typedef struct {
	int active;                  /* a group is being collected */
	int ill, n;                  /* parameters of the current group */
	uint32_t received;           /* bitmask of ILPs received in the current group */
	int last_ilp;
	amr_pl_frame_t slot[AMR_PL_MAX_GROUP];
	uint8_t present[AMR_PL_MAX_GROUP];
	/* frames ready to decode, in order */
	amr_pl_frame_t queue[AMR_PL_MAX_GROUP * 2];
	uint8_t queue_lost[AMR_PL_MAX_GROUP * 2];
	int qhead, qlen;
	uint32_t groups, lost_frames, discarded;
} amr_pl_deinterleaver_t;

void amr_pl_dil_init(amr_pl_deinterleaver_t *dil);
/* feed one parsed packet that carries an interleaving header */
void amr_pl_dil_push(amr_pl_deinterleaver_t *dil, const amr_pl_packet_t *pkt);
/* next frame in time order: returns 1 and sets *lost, or 0 if none is ready */
int amr_pl_dil_pop(amr_pl_deinterleaver_t *dil, amr_pl_frame_t *f, int *lost);

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
