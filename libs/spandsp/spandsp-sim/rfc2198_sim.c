/*
 * SpanDSP - a series of DSP components for telephony
 *
 * rfc2198_sim.c - Simulate the behaviour of RFC2198 (or UDPTL) redundancy.
 *
 * Written by Steve Underwood <steveu@coppice.org>
 *
 * Copyright (C) 2007 Steve Underwood
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

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#include <stdlib.h>
#include <unistd.h>
#include <inttypes.h>
#include <string.h>
#include <time.h>
#include <stdio.h>
#include <fcntl.h>
#if defined(HAVE_TGMATH_H)
#include <tgmath.h>
#endif
#if defined(HAVE_MATH_H)
#define GEN_CONST
#include <math.h>
#endif
#include "floating_fudge.h"

#include "spandsp.h"
#include "spandsp/g1050.h"
#include "spandsp/rfc2198_sim.h"

#define PACKET_LOSS_TIME    -1

#define FALSE 0
#define TRUE (!FALSE)

SPAN_DECLARE(rfc2198_sim_state_t *) rfc2198_sim_init(int model,
                                                     int speed_pattern,
                                                     int packet_size,
                                                     int packet_rate,
                                                     int redundancy_depth)
{
    rfc2198_sim_state_t *s;

    if ((s = (rfc2198_sim_state_t *) malloc(sizeof(*s))) == NULL)
        return NULL;
    memset(s, 0, sizeof(*s));

    s->g1050 = g1050_init(model, speed_pattern, packet_size, packet_rate);
    s->redundancy_depth = redundancy_depth;
    return s;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) rfc2198_sim_put(rfc2198_sim_state_t *s,
                                  const uint8_t buf[],
                                  int len,
                                  int seq_no,
                                  double departure_time)
{
    uint8_t buf2[8192];
    uint8_t *p;
    uint16_t *q;
    int slot;
    int i;

    /* Save the packet in the history buffer */
    memcpy(s->tx_pkt[s->next_pkt], buf, len);
    s->tx_pkt_len[s->next_pkt] = len;
    s->tx_pkt_seq_no[s->next_pkt] = seq_no;
    
    /* Construct the redundant packet */
    p = buf2;
    slot = s->next_pkt;
    q = (uint16_t *) p;
    *q = s->redundancy_depth;
    p += sizeof(uint16_t);
    for (i = 0;  i < s->redundancy_depth;  i++)
    {
        q = (uint16_t *) p;
        *q = s->tx_pkt_len[slot];
        p += sizeof(uint16_t);
        memcpy(p, s->tx_pkt[slot], s->tx_pkt_len[slot]);
        p += s->tx_pkt_len[slot];
        slot = (slot - 1) & 0x1F;
    }
    s->next_pkt = (s->next_pkt + 1) & 0x1F;
    return g1050_put(s->g1050, buf2, p - buf2, seq_no, departure_time);
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) rfc2198_sim_get(rfc2198_sim_state_t *s,
                                  uint8_t buf[],
                                  int max_len,
                                  double current_time,
                                  int *seq_no,
                                  double *departure_time,
                                  double *arrival_time)
{
    int len;
    int lenx;
    int seq_nox;
    int i;
#if defined(_MSC_VER)
    uint8_t *bufx = (uint8_t *) _alloca(s->redundancy_depth*1024);
#else
    uint8_t bufx[s->redundancy_depth*1024];
#endif
    uint8_t *p;
    uint16_t *q;
    int redundancy_depth;
    
    if (s->rx_queued_pkts)
    {
        /* We have some stuff from the last g1050_get() still to deliver */
        s->rx_queued_pkts--;
        memcpy(buf, s->rx_pkt[s->rx_queued_pkts], s->rx_pkt_len[s->rx_queued_pkts]);
        *seq_no = s->rx_pkt_seq_no[s->rx_queued_pkts];
        return s->rx_pkt_len[s->rx_queued_pkts];
    }
    len = g1050_get(s->g1050, bufx, s->redundancy_depth*1024, current_time, &seq_nox, departure_time, arrival_time);
    if (len > 0)
    {
        p = bufx;
        q = (uint16_t *) p;
        redundancy_depth = *q;
        p += sizeof(uint16_t);
        i = 0;
        if (seq_nox > s->next_seq_no)
        {
            /* Some stuff is missing. Try to fill it in. */
            s->rx_queued_pkts = seq_nox - s->next_seq_no;
            if (s->rx_queued_pkts >= redundancy_depth)
                s->rx_queued_pkts = redundancy_depth - 1;
            for (i = 0;  i < s->rx_queued_pkts;  i++)
            {
                q = (uint16_t *) p;
                s->rx_pkt_len[i] = *q;
                p += sizeof(uint16_t);
                memcpy(s->rx_pkt[i], p, s->rx_pkt_len[i]);
                s->rx_pkt_seq_no[i] = seq_nox - i;
                p += s->rx_pkt_len[i];
            }
        }
        *seq_no = seq_nox - i;
        q = (uint16_t *) p;
        lenx = *q;
        p += sizeof(uint16_t);
        memcpy(buf, p, lenx);
        s->next_seq_no = seq_nox + 1;
    }
    else
    {
        lenx = len;
    }
    return lenx;
}
/*- End of function --------------------------------------------------------*/
/*- End of file ------------------------------------------------------------*/
