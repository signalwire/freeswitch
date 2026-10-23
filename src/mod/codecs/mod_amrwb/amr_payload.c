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
 * amr_payload.c -- RTP payload format for AMR and AMR-WB (RFC 4867), see amr_payload.h
 * (the copies of this file in mod_amr and mod_amrwb are identical).
 */

#include "amr_payload.h"

/* ---- bit access, MSB first ---- */

typedef struct {
	const uint8_t *buf;
	int bits;   /* total bits available */
	int pos;    /* next bit */
} bit_reader_t;

static int br_get(bit_reader_t *br, int n, uint32_t *val)
{
	uint32_t v = 0;
	int i;

	if (n < 0 || br->pos + n > br->bits) {
		return -1;
	}
	for (i = 0; i < n; i++, br->pos++) {
		v = (v << 1) | ((br->buf[br->pos >> 3] >> (7 - (br->pos & 7))) & 1);
	}
	*val = v;
	return 0;
}

typedef struct {
	uint8_t *buf;
	int cap_bits;
	int pos;
} bit_writer_t;

static int bw_put(bit_writer_t *bw, int n, uint32_t val)
{
	int i;

	if (bw->pos + n > bw->cap_bits) {
		return -1;
	}
	for (i = n - 1; i >= 0; i--, bw->pos++) {
		uint8_t mask = (uint8_t)(1 << (7 - (bw->pos & 7)));
		if ((val >> i) & 1) {
			bw->buf[bw->pos >> 3] |= mask;
		} else {
			bw->buf[bw->pos >> 3] &= (uint8_t)~mask;
		}
	}
	return 0;
}

/* ---- frames ---- */

static int ft_allowed(const amr_pl_desc_t *d, int ft)
{
	return ft >= 0 && ft <= 15 && d->frame_bits[ft] >= 0;
}

int amr_pl_frame_bytes(const amr_pl_desc_t *d, int ft)
{
	if (!ft_allowed(d, ft)) {
		return -1;
	}
	return (d->frame_bits[ft] + 7) / 8;
}

/* clear the padding bits after the last speech bit, so equal frames compare equal */
static void clear_padding(const amr_pl_desc_t *d, amr_pl_frame_t *f)
{
	int bits = d->frame_bits[f->ft], bytes = (bits + 7) / 8;

	if (bits % 8) {
		f->data[bytes - 1] &= (uint8_t)(0xff << (8 - bits % 8));
	}
	if (bytes < AMR_PL_MAX_FRAME_BYTES) {
		memset(f->data + bytes, 0, AMR_PL_MAX_FRAME_BYTES - bytes);
	}
}

int amr_pl_frame_from_storage(const amr_pl_desc_t *d, const uint8_t *storage, int len, amr_pl_frame_t *f)
{
	int bytes;

	if (!storage || len < 1) {
		return -1;
	}
	f->ft = (storage[0] >> 3) & 0x0f;
	f->q = (storage[0] >> 2) & 0x01;
	if ((bytes = amr_pl_frame_bytes(d, f->ft)) < 0 || len < 1 + bytes) {
		return -1;
	}
	memcpy(f->data, storage + 1, bytes);
	clear_padding(d, f);
	return 0;
}

int amr_pl_frame_to_storage(const amr_pl_desc_t *d, const amr_pl_frame_t *f, uint8_t *storage)
{
	int bytes = amr_pl_frame_bytes(d, f->ft);

	if (bytes < 0) {
		bytes = 0;
	}
	storage[0] = (uint8_t)((f->ft << 3) | (f->q << 2));
	memcpy(storage + 1, f->data, bytes);
	return 1 + bytes;
}

/* ---- packets ---- */

switch_status_t amr_pl_parse(const amr_pl_desc_t *d, const uint8_t *buf, int len, int octet_aligned, int interleaved, amr_pl_packet_t *pkt)
{
	int i;

	memset(pkt, 0, sizeof(*pkt));

	if (!buf || len < 1) {
		return SWITCH_STATUS_FALSE;
	}
	if (interleaved && !octet_aligned) {
		/* section 3.8: interleaving exists only in octet-aligned mode */
		return SWITCH_STATUS_FALSE;
	}

	if (octet_aligned) {
		int pos = 0;

		pkt->cmr = buf[pos++] >> 4;

		if (interleaved) {
			if (pos >= len) return SWITCH_STATUS_FALSE;
			pkt->interleaved = 1;
			pkt->ill = buf[pos] >> 4;
			pkt->ilp = buf[pos] & 0x0f;
			pos++;
			if (pkt->ilp > pkt->ill) {
				/* 4.4.1: ILP greater than ILL, the payload SHOULD be discarded */
				return SWITCH_STATUS_FALSE;
			}
		}

		/* table of contents: one octet per frame, F=1 while more follow */
		for (;;) {
			uint8_t toc;
			amr_pl_frame_t *f;

			if (pos >= len || pkt->nframes >= AMR_PL_MAX_FRAMES) return SWITCH_STATUS_FALSE;
			toc = buf[pos++];
			f = &pkt->frames[pkt->nframes++];
			f->ft = (toc >> 3) & 0x0f;
			f->q = (toc >> 2) & 0x01;
			if (!ft_allowed(d, f->ft)) return SWITCH_STATUS_FALSE;
			if (!(toc & 0x80)) break;
		}

		/* speech data: every frame octet-aligned, in TOC order */
		for (i = 0; i < pkt->nframes; i++) {
			amr_pl_frame_t *f = &pkt->frames[i];
			int bytes = amr_pl_frame_bytes(d, f->ft);

			if (pos + bytes > len) return SWITCH_STATUS_FALSE;
			memcpy(f->data, buf + pos, bytes);
			clear_padding(d, f);
			pos += bytes;
		}
	} else {
		bit_reader_t br = { buf, len * 8, 0 };
		uint32_t v;

		if (br_get(&br, 4, &v)) return SWITCH_STATUS_FALSE;
		pkt->cmr = (uint8_t)v;

		/* table of contents: 6 bits per frame (F, FT, Q) */
		for (;;) {
			uint32_t fbit, ft, q;
			amr_pl_frame_t *f;

			if (pkt->nframes >= AMR_PL_MAX_FRAMES) return SWITCH_STATUS_FALSE;
			if (br_get(&br, 1, &fbit) || br_get(&br, 4, &ft) || br_get(&br, 1, &q)) return SWITCH_STATUS_FALSE;
			f = &pkt->frames[pkt->nframes++];
			f->ft = (uint8_t)ft;
			f->q = (uint8_t)q;
			if (!ft_allowed(d, f->ft)) return SWITCH_STATUS_FALSE;
			if (!fbit) break;
		}

		/* speech bits, frames back to back, no padding in between */
		for (i = 0; i < pkt->nframes; i++) {
			amr_pl_frame_t *f = &pkt->frames[i];
			int b, bits = d->frame_bits[f->ft];

			memset(f->data, 0, sizeof(f->data));
			for (b = 0; b < bits; b++) {
				if (br_get(&br, 1, &v)) return SWITCH_STATUS_FALSE;
				if (v) f->data[b >> 3] |= (uint8_t)(1 << (7 - (b & 7)));
			}
		}
	}

	return SWITCH_STATUS_SUCCESS;
}

int amr_pl_build(const amr_pl_desc_t *d, const amr_pl_packet_t *pkt, int octet_aligned, uint8_t *out, int cap)
{
	int i;

	if (!pkt || pkt->nframes < 1 || pkt->nframes > AMR_PL_MAX_FRAMES || (pkt->interleaved && !octet_aligned)) {
		return -1;
	}
	for (i = 0; i < pkt->nframes; i++) {
		if (!ft_allowed(d, pkt->frames[i].ft)) return -1;
	}

	if (octet_aligned) {
		int pos = 0;

		if (cap < 1) return -1;
		out[pos++] = (uint8_t)(pkt->cmr << 4);
		if (pkt->interleaved) {
			if (pos >= cap || pkt->ilp > pkt->ill) return -1;
			out[pos++] = (uint8_t)((pkt->ill << 4) | (pkt->ilp & 0x0f));
		}
		for (i = 0; i < pkt->nframes; i++) {
			const amr_pl_frame_t *f = &pkt->frames[i];
			if (pos >= cap) return -1;
			out[pos++] = (uint8_t)(((i < pkt->nframes - 1) ? 0x80 : 0) | (f->ft << 3) | (f->q << 2));
		}
		for (i = 0; i < pkt->nframes; i++) {
			const amr_pl_frame_t *f = &pkt->frames[i];
			int bytes = amr_pl_frame_bytes(d, f->ft);
			if (pos + bytes > cap) return -1;
			memcpy(out + pos, f->data, bytes);
			pos += bytes;
		}
		return pos;
	} else {
		bit_writer_t bw;
		int total_bits = 4 + 6 * pkt->nframes, bytes;

		for (i = 0; i < pkt->nframes; i++) total_bits += d->frame_bits[pkt->frames[i].ft];
		bytes = (total_bits + 7) / 8;
		if (bytes > cap) return -1;

		memset(out, 0, bytes);
		bw.buf = out;
		bw.cap_bits = bytes * 8;
		bw.pos = 0;

		bw_put(&bw, 4, pkt->cmr);
		for (i = 0; i < pkt->nframes; i++) {
			const amr_pl_frame_t *f = &pkt->frames[i];
			bw_put(&bw, 1, (i < pkt->nframes - 1) ? 1 : 0);
			bw_put(&bw, 4, f->ft);
			bw_put(&bw, 1, f->q);
		}
		for (i = 0; i < pkt->nframes; i++) {
			const amr_pl_frame_t *f = &pkt->frames[i];
			int b, bits = d->frame_bits[f->ft];
			for (b = 0; b < bits; b++) {
				bw_put(&bw, 1, (f->data[b >> 3] >> (7 - (b & 7))) & 1);
			}
		}
		/* the rest of the last octet is zero padding (already zero) */
		return bytes;
	}
}

/* ---- de-interleaver ---- */

void amr_pl_dil_init(amr_pl_deinterleaver_t *dil)
{
	memset(dil, 0, sizeof(*dil));
}

static void dil_enqueue(amr_pl_deinterleaver_t *dil, const amr_pl_frame_t *f, int lost)
{
	int cap = AMR_PL_MAX_GROUP * 2, idx;

	if (dil->qlen == cap) {
		/* we are not being drained fast enough: drop the oldest frame */
		dil->qhead = (dil->qhead + 1) % cap;
		dil->qlen--;
	}
	idx = (dil->qhead + dil->qlen) % cap;
	if (f) {
		dil->queue[idx] = *f;
	} else {
		memset(&dil->queue[idx], 0, sizeof(dil->queue[idx]));
		dil->queue[idx].ft = AMR_PL_FT_NO_DATA;
	}
	dil->queue_lost[idx] = (uint8_t)lost;
	dil->qlen++;
}

static void dil_flush(amr_pl_deinterleaver_t *dil)
{
	int i, g;

	if (!dil->active) {
		return;
	}
	g = dil->n * (dil->ill + 1);
	for (i = 0; i < g; i++) {
		if (dil->present[i]) {
			dil_enqueue(dil, &dil->slot[i], 0);
		} else {
			dil_enqueue(dil, NULL, 1);
			dil->lost_frames++;
		}
	}
	dil->active = 0;
	dil->groups++;
}

void amr_pl_dil_push(amr_pl_deinterleaver_t *dil, const amr_pl_packet_t *pkt)
{
	int k, ill = pkt->ill, p = pkt->ilp, n = pkt->nframes;

	if (!pkt->interleaved || p > ill || n < 1 || n * (ill + 1) > AMR_PL_MAX_GROUP) {
		dil->discarded++;
		return;
	}

	/* a new group starts when the layout changes, or the index does not move forward
	   (4.4.1: N and ILL must not change inside a group; ILP runs 0..ILL) */
	if (dil->active && (ill != dil->ill || n != dil->n || p <= dil->last_ilp || (dil->received & (1u << p)))) {
		dil_flush(dil);
	}

	if (!dil->active) {
		dil->active = 1;
		dil->ill = ill;
		dil->n = n;
		dil->received = 0;
		dil->last_ilp = -1;
		memset(dil->present, 0, sizeof(dil->present));
	}

	for (k = 0; k < n; k++) {
		int idx = p + k * (ill + 1);
		dil->slot[idx] = pkt->frames[k];
		dil->present[idx] = 1;
	}
	dil->received |= 1u << p;
	dil->last_ilp = p;

	if (dil->received == ((1u << (ill + 1)) - 1)) {
		/* every packet of the group is here */
		dil_flush(dil);
	}
}

int amr_pl_dil_pop(amr_pl_deinterleaver_t *dil, amr_pl_frame_t *f, int *lost)
{
	int cap = AMR_PL_MAX_GROUP * 2;

	if (!dil->qlen) {
		return 0;
	}
	*f = dil->queue[dil->qhead];
	*lost = dil->queue_lost[dil->qhead];
	dil->qhead = (dil->qhead + 1) % cap;
	dil->qlen--;
	return 1;
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
