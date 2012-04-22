/*
 * SpanDSP - a series of DSP components for telephony
 *
 * pcap_parse.h
 *
 * Written by Steve Underwood <steveu@coppice.org>
 *
 * Copyright (C) 2009 Steve Underwood
 *
 * All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2, as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

/*! \file */

#if !defined(_SPANDSP_PCAP_PARSE_H_)
#define _SPANDSP_PCAP_PARSE_H_

#if defined(__cplusplus)
extern "C"
{
#endif

typedef int (pcap_timing_update_handler_t)(void *user_data, struct timeval *ts);
typedef int (pcap_packet_handler_t)(void *user_data, const uint8_t *pkt, int len);

int pcap_scan_pkts(const char *file,
                   uint32_t src_addr,
                   uint16_t src_port,
                   uint32_t dest_addr,
                   uint16_t dest_port,
                   pcap_timing_update_handler_t *timing_update_handler,
                   pcap_packet_handler_t *packet_handler,
                   void *user_data);

#if defined(__cplusplus)
}
#endif

#endif
/*- End of file ------------------------------------------------------------*/
