//#define UDPTL_DEBUG
/* 
 * FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
 * Copyright (C) 2009, Steve Underwood <steveu@coppice.org>
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
 * Contributor(s):
 * 
 * Steve Underwood <steveu@coppice.org>
 *
 * udptl.c -- UDPTL handling for T.38
 *
 */

#include "mod_spandsp.h"

#include "udptl.h"

#define FALSE 0
#ifndef TRUE
#define TRUE (!FALSE)
#endif

static int decode_length(const uint8_t *buf, int limit, int *len, int *pvalue)
{
	if (*len >= limit)
		return -1;
	if ((buf[*len] & 0x80) == 0) {
		*pvalue = buf[(*len)++];
		return 0;
	}
	if ((buf[*len] & 0x40) == 0) {
		if (*len >= limit - 1)
			return -1;
		*pvalue = (buf[(*len)++] & 0x3F) << 8;
		*pvalue |= buf[(*len)++];
		return 0;
	}
	*pvalue = (buf[(*len)++] & 0x3F) << 14;
	/* Indicate we have a fragment */
	return 1;
}

/*- End of function --------------------------------------------------------*/

static int decode_open_type(const uint8_t *buf, int limit, int *len, const uint8_t ** p_object, int *p_num_octets)
{
	int octet_cnt;
#if 0
	int octet_idx;
	int stat;
	const uint8_t **pbuf;

	*p_num_octets = 0;
	for (octet_idx = 0;; octet_idx += octet_cnt) {
		if ((stat = decode_length(buf, limit, len, &octet_cnt)) < 0)
			return -1;
		if (octet_cnt > 0) {
			*p_num_octets += octet_cnt;

			pbuf = &p_object[octet_idx];
			/* Make sure the buffer contains at least the number of bits requested */
			if ((*len + octet_cnt) > limit)
				return -1;

			/* Was told the buffer was large enough, but in reality it didn't exist. FS-5202 */
			if ( buf[*len] == 0 )
			  return -1;

			*pbuf = &buf[*len];
			*len += octet_cnt;
		}
		if (stat == 0)
			break;
	}
#else
	/* We do not deal with fragments, so there is no point in looping through them. Just say that something
       fragmented is bad. */
	if (decode_length(buf, limit, len, &octet_cnt) != 0)
		return -1;
	*p_num_octets = octet_cnt;
	if (octet_cnt > 0) {
		/* Make sure the buffer contains at least the number of bits requested */
		if ((*len + octet_cnt) > limit)
			return -1;
		*p_object = &buf[*len];
		*len += octet_cnt;
	}
#endif
	return 0;
}

/*- End of function --------------------------------------------------------*/

static int encode_length(uint8_t *buf, int *len, int value)
{
	int multiplier;

	if (value < 0x80) {
		/* 1 octet */
		buf[(*len)++] = (uint8_t)value;
		return value;
	}
	if (value < 0x4000) {
		/* 2 octets */
		/* Set the first bit of the first octet */
		buf[(*len)++] = ((0x8000 | value) >> 8) & 0xFF;
		buf[(*len)++] = value & 0xFF;
		return value;
	}
	/* Fragmentation */
	multiplier = (value < 0x10000) ? (value >> 14) : 4;
	/* Set the first 2 bits of the octet */
	buf[(*len)++] = (uint8_t) (0xC0 | multiplier);
	return multiplier << 14;
}

/*- End of function --------------------------------------------------------*/

static int encode_open_type(uint8_t *buf, int *len, const uint8_t *data, int num_octets)
{
	int enclen;
	int octet_idx;
	uint8_t zero_byte;

	/* If open type is of zero length, add a single zero byte (10.1) */
	if (num_octets == 0) {
		zero_byte = 0;
		data = &zero_byte;
		num_octets = 1;
	}
	/* Encode the open type */
	for (octet_idx = 0;; num_octets -= enclen, octet_idx += enclen) {
		if ((enclen = encode_length(buf, len, num_octets)) < 0)
			return -1;
		if (enclen > 0) {
			memcpy(&buf[*len], &data[octet_idx], enclen);
			*len += enclen;
		}
		if (enclen >= num_octets)
			break;
	}

	return 0;
}

/*- End of function --------------------------------------------------------*/

int udptl_rx_packet(udptl_state_t *s, const uint8_t buf[], int len)
{
	int stat;
	int i;
	int j;
	int k;
	int l;
	int m;
	int x;
	int limit;
	int which;
	int ptr;
	int count;
	int total_count;
	int seq_no;
	const uint8_t *msg = NULL;
	const uint8_t *data = NULL;
	int msg_len;
	int repaired[16];
	const uint8_t *bufs[16] = {0};
	int lengths[16];
	int span;
	int entries;

	ptr = 0;
	/* Decode seq_number */
	if (ptr + 2 > len)
		return -1;
	seq_no = (buf[0] << 8) | buf[1];
	ptr += 2;
	/* Break out the primary packet */
	if ((stat = decode_open_type(buf, len, &ptr, &msg, &msg_len)) != 0)
		return -1;
	/* Decode error_recovery */
	if (ptr + 1 > len)
		return -1;
	/* Our buffers cannot tolerate overlength packets */
	if (msg_len > LOCAL_FAX_MAX_DATAGRAM)
		return -1;
	/* Update any missed slots in the buffer */
	for (i = s->rx_seq_no; seq_no > i; i++) {
		x = i & UDPTL_BUF_MASK;
		s->rx[x].buf_len = -1;
		s->rx[x].fec_len[0] = 0;
		s->rx[x].fec_span = 0;
		s->rx[x].fec_entries = 0;
	}
	/* Save the new packet. Pure redundancy mode won't use this, but some systems will switch
	   into FEC mode after sending some redundant packets. */
	x = seq_no & UDPTL_BUF_MASK;
    if (msg_len > 0)
    	memcpy(s->rx[x].buf, msg, msg_len);
	s->rx[x].buf_len = msg_len;
	s->rx[x].fec_len[0] = 0;
	s->rx[x].fec_span = 0;
	s->rx[x].fec_entries = 0;
	if ((buf[ptr++] & 0x80) == 0) {
		/* Secondary packet mode for error recovery */
		/* We might have the packet we want, but we need to check through
		   the redundant stuff, and verify the integrity of the UDPTL.
		   This greatly reduces our chances of accepting garbage. */
		total_count = 0;
		do {
			if ((stat = decode_length(buf, len, &ptr, &count)) < 0)
				return -1;
			if ((total_count + count) >= 16) {
				/* There is too much stuff here to be real, and it would overflow the bufs array
				   if we continue */
				return -1;
			}
			for (i = 0; i < count; i++) {
				if (decode_open_type(buf, len, &ptr, &bufs[total_count + i], &lengths[total_count + i]) != 0)
					return -1;
			}
			total_count += count;
		}
		while (stat > 0);
		/* We should now be exactly at the end of the packet. If not, this is a fault. */
		if (ptr != len)
			return -1;
		if (seq_no > s->rx_seq_no) {
			/* We received a later packet than we expected, so we need to check if we can fill in the gap from the
			   secondary packets. */
			/* Step through in reverse order, so we go oldest to newest */
			for (i = total_count; i > 0; i--) {
				if (seq_no - i >= s->rx_seq_no) {
					/* This one wasn't seen before */
					/* Decode the secondary packet */
#if defined(UDPTL_DEBUG)
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Secondary %d, len %d\n", seq_no - i, lengths[i - 1]);
#endif
					/* Save the new packet. Redundancy mode won't use this, but some systems will switch into
					   FEC mode after sending some redundant packets, and this may then be important. */
					x = (seq_no - i) & UDPTL_BUF_MASK;
					if (lengths[i - 1] > 0)
						memcpy(s->rx[x].buf, bufs[i - 1], lengths[i - 1]);						
					s->rx[x].buf_len = lengths[i - 1];
					s->rx[x].fec_len[0] = 0;
					s->rx[x].fec_span = 0;
					s->rx[x].fec_entries = 0;
					if (s->rx_packet_handler(s->user_data, bufs[i - 1], lengths[i - 1], seq_no - i) < 0)
						switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Bad IFP\n");
				}
			}
		}
	} else {
		/* FEC mode for error recovery */

		/* Decode the FEC packets */
		/* The span is defined as an unconstrained integer, but will never be more
		   than a small value. */
		if (ptr + 2 > len)
			return -1;
		if (buf[ptr++] != 1)
			return -1;
		span = buf[ptr++];

		x = seq_no & UDPTL_BUF_MASK;

		s->rx[x].fec_span = span;

		memset(repaired, 0, sizeof(repaired));
		repaired[x] = TRUE;

		/* The number of entries is defined as a length, but will only ever be a small
		   value. Treat it as such. */
		if (ptr + 1 > len)
			return -1;
		entries = buf[ptr++];
		s->rx[x].fec_entries = entries;

		/* Decode the elements */
		for (i = 0; i < entries; i++) {
			if ((stat = decode_open_type(buf, len, &ptr, &data, &s->rx[x].fec_len[i])) != 0)
				return -1;
			if (s->rx[x].fec_len[i] > LOCAL_FAX_MAX_DATAGRAM)
				return -1;

			/* Save the new FEC data */
            if (s->rx[x].fec_len[i])
    			memcpy(s->rx[x].fec[i], data, s->rx[x].fec_len[i]);
#if 0
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "FEC: ");
			for (j = 0; j < s->rx[x].fec_len[i]; j++)
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "%02X ", data[j]);
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "\n");
#endif
		}
		/* We should now be exactly at the end of the packet. If not, this is a fault. */
		if (ptr != len)
			return -1;
		/* See if we can reconstruct anything which is missing */
		/* TODO: this does not comprehensively hunt back and repair everything that is possible */
		for (l = x; l != ((x - (16 - span * entries)) & UDPTL_BUF_MASK); l = (l - 1) & UDPTL_BUF_MASK) {
			if (s->rx[l].fec_len[0] <= 0)
				continue;
			for (m = 0; m < s->rx[l].fec_entries; m++) {
				limit = (l + m) & UDPTL_BUF_MASK;
				for (which = -1, k = (limit - s->rx[l].fec_span * s->rx[l].fec_entries) & UDPTL_BUF_MASK; k != limit;
					 k = (k + s->rx[l].fec_entries) & UDPTL_BUF_MASK) {
					if (s->rx[k].buf_len <= 0)
						which = (which == -1) ? k : -2;
				}
				if (which >= 0) {
					/* Repairable */
					for (j = 0; j < s->rx[l].fec_len[m]; j++) {
						s->rx[which].buf[j] = s->rx[l].fec[m][j];
						for (k = (limit - s->rx[l].fec_span * s->rx[l].fec_entries) & UDPTL_BUF_MASK; k != limit;
							 k = (k + s->rx[l].fec_entries) & UDPTL_BUF_MASK)
							s->rx[which].buf[j] ^= (s->rx[k].buf_len > j) ? s->rx[k].buf[j] : 0;
					}
					s->rx[which].buf_len = s->rx[l].fec_len[m];
					repaired[which] = TRUE;
				}
			}
		}
		/* Now play any new packets forwards in time */
		for (l = (x + 1) & UDPTL_BUF_MASK, j = seq_no - UDPTL_BUF_MASK; l != x; l = (l + 1) & UDPTL_BUF_MASK, j++) {
			if (repaired[l]) {
#if defined(UDPTL_DEBUG)
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Fixed packet %d, len %d\n", j, l);
#endif
				if (s->rx_packet_handler(s->user_data, s->rx[l].buf, s->rx[l].buf_len, j) < 0)
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Bad IFP\n");
			}
		}
	}
	/* If packets are received out of sequence, we may have already processed this packet from the error
	   recovery information in a packet already received. */
	if (seq_no >= s->rx_seq_no) {
		/* Decode the primary packet */
#if defined(UDPTL_DEBUG)
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Primary packet %d, len %d\n", seq_no, msg_len);
#endif
		if (s->rx_packet_handler(s->user_data, msg, msg_len, seq_no) < 0)
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Bad IFP\n");
	}

	s->rx_seq_no = (seq_no + 1) & 0xFFFF;
	return 0;
}

/*- End of function --------------------------------------------------------*/

int udptl_build_packet(udptl_state_t *s, uint8_t buf[], const uint8_t msg[], int msg_len)
{
	uint8_t fec[LOCAL_FAX_MAX_DATAGRAM];
	int i;
	int j;
	int seq;
	int entry;
	int entries;
	int span;
	int m;
	int len;
	int limit;
	int high_tide;
	int len_before_entries;
	int previous_len;

	/* UDPTL cannot cope with zero length messages, and our buffering for redundancy limits their
	   maximum length. */
	if (msg_len < 1 || msg_len > LOCAL_FAX_MAX_DATAGRAM)
		return -1;
	seq = s->tx_seq_no & 0xFFFF;

	/* Map the sequence number to an entry in the circular buffer */
	entry = seq & UDPTL_BUF_MASK;

	/* We save the message in a circular buffer, for generating FEC or
	   redundancy sets later on. */
	s->tx[entry].buf_len = msg_len;
	memcpy(s->tx[entry].buf, msg, msg_len);

	/* Build the UDPTL packet */

	len = 0;
	/* Encode the sequence number */
	buf[len++] = (seq >> 8) & 0xFF;
	buf[len++] = seq & 0xFF;

	/* Encode the primary packet */
	if (encode_open_type(buf, &len, msg, msg_len) < 0)
		return -1;

	/* Encode the appropriate type of error recovery information */
	switch (s->error_correction_scheme) {
	case UDPTL_ERROR_CORRECTION_NONE:
		/* Encode the error recovery type */
		buf[len++] = 0x00;
		/* The number of entries will always be zero, so it is pointless allowing
		   for the fragmented case here. */
		if (encode_length(buf, &len, 0) < 0)
			return -1;
		break;
	case UDPTL_ERROR_CORRECTION_REDUNDANCY:
		/* Encode the error recovery type */
		buf[len++] = 0x00;
		if (s->tx_seq_no > s->error_correction_entries)
			entries = s->error_correction_entries;
		else
			entries = s->tx_seq_no;
		len_before_entries = len;
		/* The number of entries will always be small, so it is pointless allowing
		   for the fragmented case here. */
		if (encode_length(buf, &len, entries) < 0)
			return -1;
		/* Encode the elements */
		for (m = 0; m < entries; m++) {
			previous_len = len;
			j = (entry - m - 1) & UDPTL_BUF_MASK;
			if (encode_open_type(buf, &len, s->tx[j].buf, s->tx[j].buf_len) < 0)
				return -1;

			/* If we have exceeded the far end's max datagram size, don't include this last chunk,
			   and stop trying to add more. */
			if (len > s->far_max_datagram_size) {
				len = previous_len;
				if (encode_length(buf, &len_before_entries, m) < 0)
					return -1;
				break;
            }
		}
		break;
	case UDPTL_ERROR_CORRECTION_FEC:
		span = s->error_correction_span;
		entries = s->error_correction_entries;
		if (seq < s->error_correction_span * s->error_correction_entries) {
			/* In the initial stages, wind up the FEC smoothly */
			entries = seq / s->error_correction_span;
			if (seq < s->error_correction_span)
				span = 0;
		}
		/* Encode the error recovery type */
		buf[len++] = 0x80;
		/* Span is defined as an inconstrained integer, which it dumb. It will only
		   ever be a small value. Treat it as such. */
		buf[len++] = 1;
		buf[len++] = (uint8_t) span;
		len_before_entries = len;
		/* The number of entries is defined as a length, but will only ever be a small
		   value. Treat it as such. */
		buf[len++] = (uint8_t) entries;
		for (m = 0; m < entries; m++) {
			previous_len = len;
			/* Make an XOR'ed entry the maximum length */
			limit = (entry + m) & UDPTL_BUF_MASK;
			high_tide = 0;
			for (i = (limit - span * entries) & UDPTL_BUF_MASK; i != limit; i = (i + entries) & UDPTL_BUF_MASK) {
				if (high_tide < s->tx[i].buf_len) {
					for (j = 0; j < high_tide; j++)
						fec[j] ^= s->tx[i].buf[j];
					for (; j < s->tx[i].buf_len; j++)
						fec[j] = s->tx[i].buf[j];
					high_tide = s->tx[i].buf_len;
				} else {
					for (j = 0; j < s->tx[i].buf_len; j++)
						fec[j] ^= s->tx[i].buf[j];
				}
			}
			if (encode_open_type(buf, &len, fec, high_tide) < 0)
				return -1;

			/* If we have exceeded the far end's max datagram size, don't include this last chunk,
			   and stop trying to add more. */
			if (len > s->far_max_datagram_size) {
				len = previous_len;
				buf[len_before_entries] = (uint8_t) m;
				break;
			}
		}
		break;
	}

	s->tx_seq_no++;
	return len;
}

/*- End of function --------------------------------------------------------*/

int udptl_set_error_correction(udptl_state_t *s, int ec_scheme, int span, int entries)
{
	switch (ec_scheme) {
	case UDPTL_ERROR_CORRECTION_FEC:
	case UDPTL_ERROR_CORRECTION_REDUNDANCY:
	case UDPTL_ERROR_CORRECTION_NONE:
		s->error_correction_scheme = ec_scheme;
		break;
	case -1:
		/* Just don't change the scheme */
		break;
	default:
		return -1;
	}
	if (span >= 0)
		s->error_correction_span = span;
	if (entries >= 0)
		s->error_correction_entries = entries;
	return 0;
}

/*- End of function --------------------------------------------------------*/

int udptl_get_error_correction(udptl_state_t *s, int *ec_scheme, int *span, int *entries)
{
	if (ec_scheme)
		*ec_scheme = s->error_correction_scheme;
	if (span)
		*span = s->error_correction_span;
	if (entries)
		*entries = s->error_correction_entries;
	return 0;
}

/*- End of function --------------------------------------------------------*/

int udptl_set_local_max_datagram(udptl_state_t *s, int max_datagram)
{
	s->local_max_datagram_size = max_datagram;
	return 0;
}

/*- End of function --------------------------------------------------------*/

int udptl_get_local_max_datagram(udptl_state_t *s)
{
	return s->local_max_datagram_size;
}

/*- End of function --------------------------------------------------------*/

int udptl_set_far_max_datagram(udptl_state_t *s, int max_datagram)
{
	s->far_max_datagram_size = max_datagram;
	return 0;
}

/*- End of function --------------------------------------------------------*/

int udptl_get_far_max_datagram(udptl_state_t *s)
{
	return s->far_max_datagram_size;
}

/*- End of function --------------------------------------------------------*/

udptl_state_t *udptl_init(udptl_state_t *s, int ec_scheme, int span, int entries, udptl_rx_packet_handler_t rx_packet_handler, void *user_data)
{
	int i;

	if (rx_packet_handler == NULL)
		return NULL;

	if (s == NULL) {
		if ((s = (udptl_state_t *) malloc(sizeof(*s))) == NULL)
			return NULL;
	}
	memset(s, 0, sizeof(*s));

	s->error_correction_scheme = ec_scheme;
	s->error_correction_span = span;
	s->error_correction_entries = entries;

	s->far_max_datagram_size = LOCAL_FAX_MAX_DATAGRAM;
	s->local_max_datagram_size = LOCAL_FAX_MAX_DATAGRAM;

	memset(&s->rx, 0, sizeof(s->rx));
	memset(&s->tx, 0, sizeof(s->tx));
	for (i = 0; i <= UDPTL_BUF_MASK; i++) {
		s->rx[i].buf_len = -1;
		s->tx[i].buf_len = -1;
	}

	s->rx_packet_handler = rx_packet_handler;
	s->user_data = user_data;

	return s;
}

/*- End of function --------------------------------------------------------*/

int udptl_release(udptl_state_t *s)
{
	return 0;
}

/*- End of function --------------------------------------------------------*/
/*- End of file ------------------------------------------------------------*/
