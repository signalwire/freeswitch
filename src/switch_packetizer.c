/*
 * FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
 * Copyright (C) 2005-2020, Anthony Minessale II <anthm@freeswitch.org>
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
 * Seven Du <dujinfang@gmail.com>
 *
 * switch_packetizer H264 packetizer
 *
 */

#include <switch.h>
#define MAX_NALUS 256

typedef struct our_h264_nalu_s {
	const uint8_t *start;
	const uint8_t *eat;
	uint32_t len;
} our_h264_nalu_t;

typedef struct h264_packetizer_s {
	switch_packetizer_bitstream_t type;
	uint32_t slice_size;
	int nalu_current_index;
	our_h264_nalu_t nalus[MAX_NALUS];
	uint8_t *extradata;
	uint32_t extradata_size;
	uint8_t *sps;
	uint8_t *pps;
	uint32_t sps_len;
	uint32_t pps_len;
	int sps_sent;
	int pps_sent;
} h264_packetizer_t;

/*  ff_avc_find_startcode is not exposed in the ffmpeg lib but you can use it
	Either include the avc.h which available in the ffmpeg source, or
	just add the declaration like we does following to avoid include that whole avc.h
	The function is implemented in avc.h, guess we'll get rid of this later if we can directly use libx264

#include <libavformat/avc.h>
*/

const uint8_t *ff_avc_find_startcode(const uint8_t *p, const uint8_t *end);

static const uint8_t *fs_avc_find_startcode_internal(const uint8_t *p, const uint8_t *end)
{
	const uint8_t *a = p + 4 - ((intptr_t)p & 3);

	for (end -= 3; p < a && p < end; p++) {
		if (p[0] == 0 && p[1] == 0 && p[2] == 1)
			return p;
	}

	for (end -= 3; p < end; p += 4) {
		uint32_t x = *(const uint32_t*)p;
		if ((x - 0x01010101) & (~x) & 0x80808080) {
			if (p[1] == 0) {
				if (p[0] == 0 && p[2] == 1)
					return p;
				if (p[2] == 0 && p[3] == 1)
					return p+1;
			}
			if (p[3] == 0) {
				if (p[2] == 0 && p[4] == 1)
					return p+2;
				if (p[4] == 0 && p[5] == 1)
					return p+3;
			}
		}
	}

	for (end += 3; p < end; p++) {
		if (p[0] == 0 && p[1] == 0 && p[2] == 1)
			return p;
	}

	return end + 3;
}

static const uint8_t *fs_avc_find_startcode(const uint8_t *p, const uint8_t *end)
{
	const uint8_t *out= fs_avc_find_startcode_internal(p, end);

	if (p < out && out < end && !out[-1]) {
		out--;
	}

	return out;
}

SWITCH_DECLARE(switch_packetizer_t *) switch_packetizer_create(switch_packetizer_bitstream_t type, uint32_t slice_size)
{
	h264_packetizer_t *context = malloc(sizeof(h264_packetizer_t));
	memset(context, 0, sizeof(h264_packetizer_t));
	context->slice_size = slice_size;
	context->type = type;
	return (switch_packetizer_t *)context;
}

// for H264
SWITCH_DECLARE(switch_status_t) switch_packetizer_feed_extradata(switch_packetizer_t *packetizer, void *data, uint32_t size)
{
	h264_packetizer_t *context = (h264_packetizer_t *)packetizer;
	uint8_t *p;
	int left = size;
	int n_sps = 0;
	int n_pps = 0;
	int sps_len;
	int pps_len;
	int i;

	if (left < 10) return SWITCH_STATUS_FALSE;

	if (context->extradata) {
		context->sps = NULL;
		context->pps = NULL;
		context->sps_len = 0;
		context->pps_len = 0;
		free(context->extradata);
		context->extradata = NULL;
	}

	context->extradata = malloc(size);
	if (!context->extradata) return SWITCH_STATUS_MEMERR;
	memcpy(context->extradata, data, size);

/*
0x0000 | 01 64 00 1E FF E1 00 1F 67 64 00 1E AC C8 60 33  // E1: 1SPS  00 1F: SPS 31byte
0x0010 | 0E F9 E6 FF C1 C6 01 C4 44 00 00 03 00 04 00 00
0x0020 | 03 00 B8 3C 58 B6 68 01 00 05 68 E9 78 47 2C     // 01: 1PPS  00 05: PPS 5byte
*/

	p = context->extradata;

	if (*p != 1) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "NOT supported version: %d\n", *p);
		return SWITCH_STATUS_FALSE;
	}
	p += 5;
	left -= 5;

	if (left < 0) return SWITCH_STATUS_FALSE;

	//sps
	n_sps = *p & 0x1f;
	p += 1;
	left -= 1;

	for (i = 0; i < n_sps; i++) {
		sps_len = ntohs(*(uint16_t *)p);
		p += sizeof(uint16_t);
		left -= sizeof(uint16_t);

		if (left < sps_len) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "corrupted data %d < %u\n", left, sps_len);
			return SWITCH_STATUS_FALSE;
		}

		if (!context->sps) { // we only need the first one
			context->sps = p;
			context->sps_len = sps_len;
		}

		p += sps_len;
		left -= sps_len;
	}

	if (left < 0) return SWITCH_STATUS_FALSE;

	n_pps = *p & 0x1f;
	p += 1;
	left -= 1;

	for (i = 0; i < n_pps; i++) {
		pps_len = ntohs(*(uint16_t *)p);
		p += sizeof(uint16_t);
		left -= sizeof(uint16_t);

		if (left < pps_len) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "corrupted data %d < %u\n", left, pps_len);
			return SWITCH_STATUS_FALSE;
		}

		if (!context->pps) { // we only need the first one
			context->pps = p;
			context->pps_len = pps_len;
		}
		p += pps_len;
		left -= pps_len;
	}

	return SWITCH_STATUS_SUCCESS;
}

SWITCH_DECLARE(switch_status_t) switch_packetizer_feed(switch_packetizer_t *packetizer, void *data, uint32_t size)
{
	h264_packetizer_t *context = (h264_packetizer_t *)packetizer;
	const uint8_t *p = data;
	const uint8_t *end = p + size;
	int i = 0;

	// reset everytime
	memset(context->nalus, 0, MAX_NALUS * sizeof(our_h264_nalu_t));
	context->nalu_current_index = 0;

	// switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "size = %u %x %x %x %x %x\n", size, *p, *(p+1), *(p+2), *(p+3), *(p+4));

	if (context->type == SPT_H264_SIZED_BITSTREAM) {
		int left = size;
		uint32_t len;

		while (left > 0) {
			if (left < sizeof(uint32_t)) return SWITCH_STATUS_MORE_DATA;
			len = htonl(*(uint32_t *)p);
			left -= sizeof(uint32_t);
			left -= len;
			if (left < 0) return SWITCH_STATUS_MORE_DATA;
			p += sizeof(uint32_t);

			context->nalus[i].start = p;
			context->nalus[i].eat = p;
			context->nalus[i].len = len;

			p += len;

			// switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "#%d %x len=%u\n", i, *context->nalus[i].start, context->nalus[i].len);
			i++;
		}

		return SWITCH_STATUS_SUCCESS;
	}

	if (context->type == SPT_H264_SIGNALE_NALU) {
		context->nalus[0].start = data;
		context->nalus[0].eat = data;
		context->nalus[0].len = size;

		return SWITCH_STATUS_SUCCESS;
	}

	// SPT_H264_BITSTREAM
	while ((p = fs_avc_find_startcode(p, end)) < end) {
		if (!context->nalus[i].start) {
			while (!(*p++)) ; /* eat the sync bytes, what ever 0 0 1 or 0 0 0 1 */
			context->nalus[i].start = p;
			context->nalus[i].eat = p;
		} else {
			context->nalus[i].len = p - context->nalus[i].start;
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "#%d %x len=%u\n", i, *context->nalus[i].start, context->nalus[i].len);
			while (!(*p++)) ; /* eat the sync bytes, what ever 0 0 1 or 0 0 0 1 */
			i++;
			context->nalus[i].start = p;
			context->nalus[i].eat = p;
		}
		if (i >= MAX_NALUS - 2) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "TOO MANY SLICES!\n");
			break;
		}

	}

	context->nalus[i].len = p - context->nalus[i].start;

	return SWITCH_STATUS_SUCCESS;
}

SWITCH_DECLARE(switch_status_t) switch_packetizer_read(switch_packetizer_t *packetizer, switch_frame_t *frame)
{
	h264_packetizer_t *context = (h264_packetizer_t *)packetizer;
	uint32_t slice_size = context->slice_size;
	our_h264_nalu_t *nalu = &context->nalus[context->nalu_current_index];
	uint8_t nalu_hdr = 0;
	uint8_t nalu_type = 0;
	uint8_t nri = 0;
	int left = nalu->len - (nalu->eat - nalu->start);
	uint8_t *p = frame->data;
	uint8_t start = nalu->start == nalu->eat ? 0x80 : 0;
	int n = nalu->len / slice_size + 1;
	int real_slice_size = nalu->len / n + 1 + 2;

	if (nalu->start == NULL) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "INVALID BITSTREAM\n");
		return SWITCH_STATUS_FALSE;
	}

	nalu_hdr = *(uint8_t *)(nalu->start);
	nalu_type = nalu_hdr & 0x1f;
	nri = nalu_hdr & 0x60;

	if (real_slice_size > slice_size) real_slice_size = slice_size;
	if (frame->buflen < slice_size) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "frame buffer too small %u < %u\n", frame->buflen, slice_size);
		return SWITCH_STATUS_FALSE;
	}

	if (context->type == SPT_H264_BITSTREAM || SPT_H264_SIZED_BITSTREAM) {
		if (nalu_type == 0x05) {
			// insert SPS/PPS before
			if (context->sps && !context->sps_sent) {
				memcpy(frame->data, context->sps, context->sps_len);
				frame->datalen = context->sps_len;
				frame->m = 0;
				context->sps_sent = 1;
				return SWITCH_STATUS_MORE_DATA;
			} else if (context->pps && !context->pps_sent) {
				memcpy(frame->data, context->pps, context->pps_len);
				frame->datalen = context->pps_len;
				frame->m = 0;
				context->pps_sent = 1;
				return SWITCH_STATUS_MORE_DATA;
			}
		} else if (nalu_type == 0x07) {
			context->sps_sent = 1;
		} else if (nalu_type == 0x08) {
			context->pps_sent = 1;
		}
	}

	if (nalu->len <= slice_size) {
		memcpy(frame->data, nalu->start, nalu->len);
		frame->datalen = nalu->len;
		context->nalu_current_index++;

		switch_clear_flag(frame, SFF_CNG);

		if (context->nalus[context->nalu_current_index].len) {
			frame->m = 0;
			return SWITCH_STATUS_MORE_DATA;
		}

		frame->m = 1;

		if (nalu_type == 0x05) {
			context->sps_sent = 0;
			context->pps_sent = 0;
		}

		return SWITCH_STATUS_SUCCESS;
	}

	if (left <= (real_slice_size - 2)) {
		p[0] = nri | 28; // FU-A
		p[1] = 0x40 | nalu_type;
		memcpy(p+2, nalu->eat, left);
		nalu->eat += left;
		frame->datalen = left + 2;
		context->nalu_current_index++;

		if (!context->nalus[context->nalu_current_index].len) {
			frame->m = 1;
			return SWITCH_STATUS_SUCCESS;
		}

		return SWITCH_STATUS_MORE_DATA;
	}

	p[0] = nri | 28; // FU-A
	p[1] = start | nalu_type;
	if (start) nalu->eat++;
	memcpy(p+2, nalu->eat, real_slice_size - 2);
	nalu->eat += (real_slice_size - 2);
	frame->datalen = real_slice_size;
	frame->m = 0;
	return SWITCH_STATUS_MORE_DATA;
}

SWITCH_DECLARE(void) switch_packetizer_close(switch_packetizer_t **packetizer)
{
	h264_packetizer_t *context = (h264_packetizer_t *)(*packetizer);
	if (context->extradata) free(context->extradata);
	free(context);
	*packetizer = NULL;
}
