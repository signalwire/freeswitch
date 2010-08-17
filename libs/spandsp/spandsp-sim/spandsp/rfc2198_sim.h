/*
 * SpanDSP - a series of DSP components for telephony
 *
 * rfc2198_sim.h - Simulate the behaviour of RFC2198 (or UDPTL) redundancy.
 *
 * Written by Steve Underwood <steveu@coppice.org>
 *
 * Copyright (C) 2007 Steve Underwood
 *
 * All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License version 2.1,
 * as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

/*! \file */

/*! \page rfc2198_model_page RFC2198 simulation
\section rfc2198_model_page_sec_1 What does it do?
*/

#if !defined(_RFC2198_SIM_H_)
#define _RFC2198_SIM_H_

/*! The definition of an element in the packet queue */
typedef struct rfc2198_sim_queue_element_s
{
    struct rfc2198_sim_queue_element_s *next;
    struct rfc2198_sim_queue_element_s *prev;
    int seq_no;
    double departure_time;
    double arrival_time;
    int len;
    uint8_t pkt[];
} rfc2198_sim_queue_element_t;

/*! The model definition for a complete end-to-end path */
typedef struct
{
    int redundancy_depth;
    int next_seq_no;
    g1050_state_t *g1050;
    rfc2198_sim_queue_element_t *first;
    rfc2198_sim_queue_element_t *last;
    uint8_t tx_pkt[32][1024];
    int tx_pkt_len[32];
    int tx_pkt_seq_no[32];
    int next_pkt;
    uint8_t rx_pkt[32][1024];
    int rx_pkt_len[32];
    int rx_pkt_seq_no[32];
    int rx_queued_pkts;
} rfc2198_sim_state_t;

#ifdef  __cplusplus
extern "C"
{
#endif

SPAN_DECLARE(rfc2198_sim_state_t *) rfc2198_sim_init(int model,
                                                     int speed_pattern,
                                                     int packet_size,
                                                     int packet_rate,
                                                     int redundancy_depth);

SPAN_DECLARE(int) rfc2198_sim_put(rfc2198_sim_state_t *s,
                                  const uint8_t buf[],
                                  int len,
                                  int seq_no,
                                  double departure_time);

SPAN_DECLARE(int) rfc2198_sim_get(rfc2198_sim_state_t *s,
                                  uint8_t buf[],
                                  int max_len,
                                  double current_time,
                                  int *seq_no,
                                  double *departure_time,
                                  double *arrival_time);

#ifdef  __cplusplus
}
#endif

#endif
/*- End of file ------------------------------------------------------------*/
