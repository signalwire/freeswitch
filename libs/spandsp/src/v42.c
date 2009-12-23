/*
 * SpanDSP - a series of DSP components for telephony
 *
 * v42.c
 *
 * Written by Steve Underwood <steveu@coppice.org>
 *
 * Copyright (C) 2004 Steve Underwood
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
 *
 * $Id: v42.c,v 1.51 2009/11/04 15:52:06 steveu Exp $
 */

/* THIS IS A WORK IN PROGRESS. IT IS NOT FINISHED. */

/*! \file */

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
#include <string.h>
#include <errno.h>

#include "spandsp/telephony.h"
#include "spandsp/logging.h"
#include "spandsp/async.h"
#include "spandsp/hdlc.h"
#include "spandsp/schedule.h"
#include "spandsp/queue.h"
#include "spandsp/v42.h"

#include "spandsp/private/logging.h"
#include "spandsp/private/schedule.h"
#include "spandsp/private/hdlc.h"
#include "spandsp/private/v42.h"

#if !defined(FALSE)
#define FALSE   0
#endif
#if !defined(TRUE)
#define TRUE    (!FALSE)
#endif

#define LAPM_FRAMETYPE_MASK         0x03

#define LAPM_FRAMETYPE_I            0x00
#define LAPM_FRAMETYPE_I_ALT        0x02
#define LAPM_FRAMETYPE_S            0x01
#define LAPM_FRAMETYPE_U            0x03

/* Timer values */

#define T_WAIT_MIN                  2000
#define T_WAIT_MAX                  10000
/* Detection phase timer */
#define T_400                       750000
/* Acknowledgement timer - 1 second between SABME's */
#define T_401                       1000000
/* Replay delay timer (optional) */
#define T_402                       1000000
/* Inactivity timer (optional). No default - use 10 seconds with no packets */
#define T_403                       10000000
/* Max retries */
#define N_400                       3
/* Max octets in an information field */
#define N_401                       128

#define LAPM_DLCI_DTE_TO_DTE        0
#define LAPM_DLCI_LAYER2_MANAGEMENT 63

static void t401_expired(span_sched_state_t *s, void *user_data);
static void t403_expired(span_sched_state_t *s, void *user_data);

SPAN_DECLARE(void) lapm_reset(lapm_state_t *s);
SPAN_DECLARE(void) lapm_restart(lapm_state_t *s);

static void lapm_link_down(lapm_state_t *s);

static __inline__ void lapm_init_header(uint8_t *frame, int command)
{
    /* Data link connection identifier (0) */
    /* Command/response (0 if answerer, 1 if originator) */
    /* Extended address (1) */
    frame[0] = (LAPM_DLCI_DTE_TO_DTE << 2) | ((command)  ?  0x02  :  0x00) | 0x01;
}
/*- End of function --------------------------------------------------------*/

static int lapm_tx_frame(lapm_state_t *s, uint8_t *frame, int len)
{
    if ((s->debug & LAPM_DEBUG_LAPM_DUMP))
        lapm_dump(s, frame, len, s->debug & LAPM_DEBUG_LAPM_RAW, TRUE);
    /*endif*/
    hdlc_tx_frame(&s->hdlc_tx, frame, len);
    return 0;
}
/*- End of function --------------------------------------------------------*/

static void t400_expired(span_sched_state_t *ss, void *user_data)
{
    v42_state_t *s;
    
    /* Give up trying to detect a V.42 capable peer. */
    s = (v42_state_t *) user_data;
    s->t400_timer = -1;
    s->lapm.state = LAPM_UNSUPPORTED;
    if (s->lapm.status_callback)
        s->lapm.status_callback(s->lapm.status_callback_user_data, s->lapm.state);
    /*endif*/
}
/*- End of function --------------------------------------------------------*/

static void lapm_send_ua(lapm_state_t *s, int pfbit)
{
    uint8_t frame[3];

    lapm_init_header(frame, !s->we_are_originator);
    frame[1] = (uint8_t) (0x63 | (pfbit << 4));
    frame[2] = 0;
    span_log(&s->logging, SPAN_LOG_FLOW, "Sending unnumbered acknowledgement\n");
    lapm_tx_frame(s, frame, 3);
}
/*- End of function --------------------------------------------------------*/

static void lapm_send_sabme(span_sched_state_t *ss, void *user_data)
{
    lapm_state_t *s;
    uint8_t frame[3];

    s = (lapm_state_t *) user_data;
    if (s->t401_timer >= 0)
    {
fprintf(stderr, "Deleting T401 q [%p] %d\n", (void *) s, s->t401_timer);
        span_schedule_del(&s->sched, s->t401_timer);
        s->t401_timer = -1;
    }
    /*endif*/
    if (++s->retransmissions > N_400)
    {
        /* 8.3.2.2 Too many retries */
        s->state = LAPM_RELEASE;
        if (s->status_callback)
            s->status_callback(s->status_callback_user_data, s->state);
        /*endif*/
        return;
    }
    /*endif*/
fprintf(stderr, "Setting T401 a1 [%p]\n", (void *) s);
    s->t401_timer = span_schedule_event(&s->sched, T_401, lapm_send_sabme, s);
    lapm_init_header(frame, s->we_are_originator);
    frame[1] = 0x7F;
    frame[2] = 0;
    span_log(&s->logging, SPAN_LOG_FLOW, "Sending SABME (set asynchronous balanced mode extended)\n");
    lapm_tx_frame(s, frame, 3);
}
/*- End of function --------------------------------------------------------*/

static int lapm_ack_packet(lapm_state_t *s, int num)
{
    lapm_frame_queue_t *f;
    lapm_frame_queue_t *prev;

    for (prev = NULL, f = s->txqueue;  f;  prev = f, f = f->next)
    {
        if ((f->frame[1] >> 1) == num)
        {
            /* Cancel each packet, as necessary */
            if (prev)
                prev->next = f->next;
            else
                s->txqueue = f->next;
            /*endif*/
            span_log(&s->logging,
                     SPAN_LOG_FLOW,
                     "-- ACKing packet %d. New txqueue is %d (-1 means empty)\n",
                     (f->frame[1] >> 1),
                     (s->txqueue)  ?  (s->txqueue->frame[1] >> 1)  :  -1);
            s->last_frame_peer_acknowledged = num;
            free(f);
            /* Reset retransmission count if we actually acked something */
            s->retransmissions = 0;
            return 1;
        }
        /*endif*/
    }
    /*endfor*/
    return 0;
}
/*- End of function --------------------------------------------------------*/

static void lapm_ack_rx(lapm_state_t *s, int ack)
{
    int i;
    int cnt;

    /* This might not be acking anything new */
    if (s->last_frame_peer_acknowledged == ack)
        return;
    /*endif*/
    /* It should be acking something that is actually outstanding */
    if ((s->last_frame_peer_acknowledged < s->next_tx_frame  &&  (ack < s->last_frame_peer_acknowledged  ||  ack > s->next_tx_frame))
        ||
        (s->last_frame_peer_acknowledged > s->next_tx_frame  &&  (ack > s->last_frame_peer_acknowledged  ||  ack < s->next_tx_frame)))
    {
        /* ACK was outside our window --- ignore */
        span_log(&s->logging, SPAN_LOG_FLOW, "ACK received outside window, ignoring\n");
        return;
    }
    /*endif*/
    
    /* Cancel each packet, as necessary */
    span_log(&s->logging,
             SPAN_LOG_FLOW,
             "-- ACKing all packets from %d to (but not including) %d\n",
             s->last_frame_peer_acknowledged,
             ack);
    for (cnt = 0, i = s->last_frame_peer_acknowledged;  i != ack;  i = (i + 1) & 0x7F) 
        cnt += lapm_ack_packet(s, i);
    /*endfor*/
    s->last_frame_peer_acknowledged = ack;
    if (s->txqueue == NULL)
    {
        span_log(&s->logging, SPAN_LOG_FLOW, "-- Since there was nothing left, stopping timer T_401\n");
        /* Something was ACK'd.  Stop timer T_401. */
fprintf(stderr, "T401 a2 is %d [%p]\n", s->t401_timer, (void *) s);
        if (s->t401_timer >= 0)
        {
fprintf(stderr, "Deleting T401 a3 [%p] %d\n", (void *) s, s->t401_timer);
            span_schedule_del(&s->sched, s->t401_timer);
            s->t401_timer = -1;
        }
        /*endif*/
    }
    /*endif*/
    if (s->t403_timer >= 0)
    {
        span_log(&s->logging, SPAN_LOG_FLOW, "-- Stopping timer T_403, since we got an ACK\n");
        if (s->t403_timer >= 0)
        {
fprintf(stderr, "Deleting T403 b %d\n", s->t403_timer);
            span_schedule_del(&s->sched, s->t403_timer);
            s->t403_timer = -1;
        }
        /*endif*/
    }
    /*endif*/
    if (s->txqueue)
    {
        /* Something left to transmit. Start timer T_401 again if it is stopped */
        span_log(&s->logging,
                 SPAN_LOG_FLOW,
                 "-- Something left to transmit (%d). Restarting timer T_401\n",
                 s->txqueue->frame[1] >> 1);
        if (s->t401_timer < 0)
        {
fprintf(stderr, "Setting T401 b [%p]\n", (void *) s);
            s->t401_timer = span_schedule_event(&s->sched, T_401, t401_expired, s);
        }
        /*endif*/
    }
    else
    {
        span_log(&s->logging, SPAN_LOG_FLOW, "-- Nothing left, starting timer T_403\n");
        /* Nothing to transmit. Start timer T_403. */
fprintf(stderr, "Setting T403 c\n");
        s->t403_timer = span_schedule_event(&s->sched, T_403, t403_expired, s);
    }
    /*endif*/
}
/*- End of function --------------------------------------------------------*/

static void lapm_reject(lapm_state_t *s)
{
    uint8_t frame[4];

    lapm_init_header(frame, !s->we_are_originator);
    frame[1] = (uint8_t) (0x00 | 0x08 | LAPM_FRAMETYPE_S);
    /* Where to start retransmission */
    frame[2] = (uint8_t) ((s->next_expected_frame << 1) | 0x01);
    span_log(&s->logging, SPAN_LOG_FLOW, "Sending REJ (reject (%d)\n", s->next_expected_frame);
    lapm_tx_frame(s, frame, 4);
}
/*- End of function --------------------------------------------------------*/

static void lapm_rr(lapm_state_t *s, int pfbit)
{
    uint8_t frame[4];

    lapm_init_header(frame, !s->we_are_originator);
    frame[1] = (uint8_t) (0x00 | 0x00 | LAPM_FRAMETYPE_S);
    frame[2] = (uint8_t) ((s->next_expected_frame << 1) | pfbit);
    /* Note that we have already ACKed this */
    s->last_frame_we_acknowledged = s->next_expected_frame;
    span_log(&s->logging, SPAN_LOG_FLOW, "Sending RR (receiver ready) (%d)\n", s->next_expected_frame);
    lapm_tx_frame(s, frame, 4);
}
/*- End of function --------------------------------------------------------*/

static void t401_expired(span_sched_state_t *ss, void *user_data)
{
    lapm_state_t *s;
    
    s = (lapm_state_t *) user_data;
fprintf(stderr, "Expiring T401 a4 [%p]\n", (void *) s);
    s->t401_timer = -1;
    if (s->txqueue)
    {
        /* Retransmit first packet in the queue, setting the poll bit */
        span_log(&s->logging, SPAN_LOG_FLOW, "-- Timer T_401 expired, What to do...\n");
        /* Update N(R), and set the poll bit */
        s->txqueue->frame[2] = (uint8_t)((s->next_expected_frame << 1) | 0x01);
        s->last_frame_we_acknowledged = s->next_expected_frame;
        s->solicit_f_bit = TRUE;
        if (++s->retransmissions <= N_400)
        {
            /* Reschedule timer T401 */
            span_log(&s->logging, SPAN_LOG_FLOW, "-- Retransmitting %d bytes\n", s->txqueue->len);
            lapm_tx_frame(s, s->txqueue->frame, s->txqueue->len);
            span_log(&s->logging, SPAN_LOG_FLOW, "-- Scheduling retransmission (%d)\n", s->retransmissions);
fprintf(stderr, "Setting T401 d [%p]\n", (void *) s);
            s->t401_timer = span_schedule_event(&s->sched, T_401, t401_expired, s);
        }
        else
        {
            span_log(&s->logging, SPAN_LOG_FLOW, "-- Timeout occured\n");
            s->state = LAPM_RELEASE;
            if (s->status_callback)
                s->status_callback(s->status_callback_user_data, s->state);
            lapm_link_down(s);
            lapm_restart(s);
        }
        /*endif*/
    }
    else
    {
        span_log(&s->logging, SPAN_LOG_FLOW, "Timer T_401 expired. Nothing to send...\n");
    }
    /*endif*/
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(const char *) lapm_status_to_str(int status)
{
    switch (status)
    {
    case LAPM_DETECT:
        return "LAPM_DETECT";
    case LAPM_ESTABLISH:
        return "LAPM_ESTABLISH";
    case LAPM_DATA:
        return "LAPM_DATA";
    case LAPM_RELEASE:
        return "LAPM_RELEASE";
    case LAPM_SIGNAL:
        return "LAPM_SIGNAL";
    case LAPM_SETPARM:
        return "LAPM_SETPARM";
    case LAPM_TEST:
        return "LAPM_TEST";
    case LAPM_UNSUPPORTED:
        return "LAPM_UNSUPPORTED";
    }
    /*endswitch*/
    return "???";
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) lapm_tx(lapm_state_t *s, const void *buf, int len)
{
    return queue_write(s->tx_queue, buf, len);
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) lapm_release(lapm_state_t *s)
{
    s->state = LAPM_RELEASE;
    return 0;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) lapm_loopback(lapm_state_t *s, int enable)
{
    s->state = LAPM_TEST;
    return 0;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) lapm_break(lapm_state_t *s, int enable)
{
    s->state = LAPM_SIGNAL;
    return 0;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) lapm_tx_iframe(lapm_state_t *s, const void *buf, int len, int cr)
{
    lapm_frame_queue_t *f;

    if ((f = malloc(sizeof(*f) + len + 4)) == NULL)
    {
        span_log(&s->logging, SPAN_LOG_FLOW, "Out of memory\n");
        return -1;
    }
    /*endif*/

    lapm_init_header(f->frame, (s->peer_is_originator)  ?  cr  :  !cr);
    f->next = NULL;
    f->len = len + 4;
    f->frame[1] = (uint8_t) (s->next_tx_frame << 1);
    f->frame[2] = (uint8_t) (s->next_expected_frame << 1);
    memcpy(f->frame + 3, buf, len);
    s->next_tx_frame = (s->next_tx_frame + 1) & 0x7F;
    s->last_frame_we_acknowledged = s->next_expected_frame;
    /* Clear poll bit */
    f->frame[2] &= ~0x01;
    if (s->tx_last)
        s->tx_last->next = f;
    else
        s->txqueue = f;
    /*endif*/
    s->tx_last = f;
    /* Immediately transmit unless we're in a recovery state */
    if (s->retransmissions == 0)
        lapm_tx_frame(s, f->frame, f->len);
    /*endif*/
    if (s->t403_timer >= 0)
    {
        span_log(&s->logging, SPAN_LOG_FLOW, "Stopping T_403 timer\n");
fprintf(stderr, "Deleting T403 c %d\n", s->t403_timer);
        span_schedule_del(&s->sched, s->t403_timer);
        s->t403_timer = -1;
    }
    /*endif*/
    if (s->t401_timer < 0)
    {
        span_log(&s->logging, SPAN_LOG_FLOW, "Starting timer T_401\n");
        s->t401_timer = span_schedule_event(&s->sched, T_401, t401_expired, s);
fprintf(stderr, "Setting T401 e %d [%p]\n", s->t401_timer, (void *) s);
    }
    else
    {
        span_log(&s->logging, SPAN_LOG_FLOW, "Timer T_401 already running (%d)\n", s->t401_timer);
    }
    /*endif*/
    return 0;
}
/*- End of function --------------------------------------------------------*/

static void t403_expired(span_sched_state_t *ss, void *user_data)
{
    lapm_state_t *s;

    s = (lapm_state_t *) user_data;
    span_log(&s->logging, SPAN_LOG_FLOW, "Timer T_403 expired. Sending RR and scheduling T_403 again\n");
    s->t403_timer = -1;
    s->retransmissions = 0;
    /* Solicit an F-bit in the other end's RR */
    s->solicit_f_bit = TRUE;
    lapm_rr(s, 1);
    /* Restart ourselves */
fprintf(stderr, "Setting T403 f\n");
    s->t401_timer = span_schedule_event(&s->sched, T_401, t401_expired, s);
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(void) lapm_dump(lapm_state_t *s, const uint8_t *frame, int len, int showraw, int txrx)
{
    const char *type;
    char direction_tag[2];
    
    direction_tag[0] = txrx  ?  '>'  :  '<';
    direction_tag[1] = '\0';
    if (showraw)
        span_log_buf(&s->logging, SPAN_LOG_FLOW, direction_tag, frame, len);
    /*endif*/

    switch ((frame[1] & LAPM_FRAMETYPE_MASK))
    {
    case LAPM_FRAMETYPE_I:
    case LAPM_FRAMETYPE_I_ALT:
        span_log(&s->logging, SPAN_LOG_FLOW, "%c Information frame:\n", direction_tag[0]);
        break;
    case LAPM_FRAMETYPE_S:
        span_log(&s->logging, SPAN_LOG_FLOW, "%c Supervisory frame:\n", direction_tag[0]);
        break;
    case LAPM_FRAMETYPE_U:
        span_log(&s->logging, SPAN_LOG_FLOW, "%c Unnumbered frame:\n", direction_tag[0]);
        break;
    }
    /*endswitch*/
    
    span_log(&s->logging,
             SPAN_LOG_FLOW,
             "%c DLCI: %2d  C/R: %d  EA: %d\n",
             direction_tag[0],
             (frame[0] >> 2),
             (frame[0] & 0x02)  ?  1  :  0,
             (frame[0] & 0x01),
             direction_tag[0]);
    switch ((frame[1] & LAPM_FRAMETYPE_MASK))
    {
    case LAPM_FRAMETYPE_I:
    case LAPM_FRAMETYPE_I_ALT:
        /* Information frame */
        span_log(&s->logging,
                 SPAN_LOG_FLOW, 
                 "%c N(S): %03d\n",
                 direction_tag[0],
                 (frame[1] >> 1));
        span_log(&s->logging,
                 SPAN_LOG_FLOW, 
                 "%c N(R): %03d   P: %d\n",
                 direction_tag[0],
                 (frame[2] >> 1),
                 (frame[2] & 0x01));
        span_log(&s->logging,
                 SPAN_LOG_FLOW, 
                 "%c %d bytes of data\n",
                 direction_tag[0],
                 len - 4);
        break;
    case LAPM_FRAMETYPE_S:
        /* Supervisory frame */
        switch (frame[1] & 0x0C)
        {
        case 0x00:
            type = "RR (receive ready)";
            break;
        case 0x04:
            type = "RNR (receive not ready)";
            break;
        case 0x08:
            type = "REJ (reject)";
            break;
        case 0x0C:
            type = "SREJ (selective reject)";
            break;
        default:
            type = "???";
            break;
        }
        /*endswitch*/
        span_log(&s->logging,
                 SPAN_LOG_FLOW,
                 "%c S: %03d [ %s ]\n",
                 direction_tag[0],
                 frame[1],
                 type);
        span_log(&s->logging,
                 SPAN_LOG_FLOW,
                 "%c N(R): %03d P/F: %d\n",
                 direction_tag[0],
                 frame[2] >> 1,
                 frame[2] & 0x01);
        span_log(&s->logging,
                 SPAN_LOG_FLOW,
                 "%c %d bytes of data\n",
                 direction_tag[0],
                 len - 4);
        break;
    case LAPM_FRAMETYPE_U:
        /* Unnumbered frame */
        switch (frame[1] & 0xEC)
        {
        case 0x00:
            type = "UI (unnumbered information)";
            break;
        case 0x0C:
            type = "DM (disconnect mode)";
            break;
        case 0x40:
            type = "DISC (disconnect)";
            break;
        case 0x60:
            type = "UA (unnumbered acknowledgement)";
            break;
        case 0x6C:
            type = "SABME (set asynchronous balanced mode extended)";
            break;
        case 0x84:
            type = "FRMR (frame reject)";
            break;
        case 0xAC:
            type = "XID (exchange identification)";
            break;
        case 0xE0:
            type = "TEST (test)";
            break;
        default:
            type = "???";
            break;
        }
        /*endswitch*/
        span_log(&s->logging,
                 SPAN_LOG_FLOW,
                 "%c   M: %03d [ %s ] P/F: %d\n",
                 direction_tag[0],
                 frame[1],
                 type,
                 (frame[1] >> 4) & 1);
        span_log(&s->logging,
                 SPAN_LOG_FLOW,
                 "%c %d bytes of data\n",
                 direction_tag[0],
                 len - 3);
        break;
    }
    /*endswitch*/
}
/*- End of function --------------------------------------------------------*/

static void lapm_link_up(lapm_state_t *s)
{
    uint8_t buf[1024];
    int len;

    lapm_reset(s);
    /* Go into connection established state */
    s->state = LAPM_DATA;
    if (s->status_callback)
        s->status_callback(s->status_callback_user_data, s->state);
    /*endif*/
    if (!queue_empty(s->tx_queue))
    {
        if ((len = queue_read(s->tx_queue, buf, s->n401)) > 0)
            lapm_tx_iframe(s, buf, len, TRUE);
        /*endif*/
    }
    /*endif*/
    if (s->t401_timer >= 0)
    {
fprintf(stderr, "Deleting T401 x [%p] %d\n", (void *) s, s->t401_timer);
        span_schedule_del(&s->sched, s->t401_timer);
        s->t401_timer = -1;
    }
    /*endif*/
    /* Start the T403 timer */
fprintf(stderr, "Setting T403 g\n");
    s->t403_timer = span_schedule_event(&s->sched, T_403, t403_expired, s);
}
/*- End of function --------------------------------------------------------*/

static void lapm_link_down(lapm_state_t *s)
{
    lapm_reset(s);

    if (s->status_callback)
        s->status_callback(s->status_callback_user_data, s->state);
    /*endif*/
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(void) lapm_reset(lapm_state_t *s)
{
    lapm_frame_queue_t *f;
    lapm_frame_queue_t *p;

    /* Having received a SABME, we need to reset our entire state */
    s->next_tx_frame = 0;
    s->last_frame_peer_acknowledged = 0;
    s->next_expected_frame = 0;
    s->last_frame_we_acknowledged = 0;
    s->window_size_k = 15;
    s->n401 = 128;
    if (s->t401_timer >= 0)
    {
fprintf(stderr, "Deleting T401 d [%p] %d\n", (void *) s, s->t401_timer);
        span_schedule_del(&s->sched, s->t401_timer);
        s->t401_timer = -1;
    }
    /*endif*/
    if (s->t403_timer >= 0)
    {
fprintf(stderr, "Deleting T403 e %d\n", s->t403_timer);
        span_schedule_del(&s->sched, s->t403_timer);
        s->t403_timer = -1;
    }
    /*endif*/
    s->busy = FALSE;
    s->solicit_f_bit = FALSE;
    s->state = LAPM_RELEASE;
    s->retransmissions = 0;
    /* Discard anything waiting to go out */
    for (f = s->txqueue;  f;  )
    {
        p = f;
        f = f->next;
        free(p);
    }
    /*endfor*/
    s->txqueue = NULL;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE_NONSTD(void) lapm_receive(void *user_data, const uint8_t *frame, int len, int ok)
{
    lapm_state_t *s;
    lapm_frame_queue_t *f;
    int sendnow;
    int octet;
    int s_field;
    int m_field;

fprintf(stderr, "LAPM receive %d %d\n", ok, len);
    if (!ok  ||  len == 0)
        return;
    /*endif*/
    s = (lapm_state_t *) user_data;
    if (len < 0)
    {
        /* Special conditions */
        span_log(&s->logging, SPAN_LOG_DEBUG, "V.42 rx status is %s (%d)\n", signal_status_to_str(len), len);
        return;
    }
    /*endif*/

    if ((s->debug & LAPM_DEBUG_LAPM_DUMP))
        lapm_dump(s, frame, len, s->debug & LAPM_DEBUG_LAPM_RAW, FALSE);
    /*endif*/
    octet = 0;
    /* We do not expect extended addresses */
    if ((frame[octet] & 0x01) == 0)
        return;
    /*endif*/
    /* Check for DLCIs we do not recognise */
    if ((frame[octet] >> 2) != LAPM_DLCI_DTE_TO_DTE)
        return;
    /*endif*/
    octet++;
    switch (frame[octet] & LAPM_FRAMETYPE_MASK)
    {
    case LAPM_FRAMETYPE_I:
    case LAPM_FRAMETYPE_I_ALT:
        if (s->state != LAPM_DATA)
        {
            span_log(&s->logging, SPAN_LOG_FLOW, "!! Got an I-frame while link state is %d\n", s->state);
            break;
        }
        /*endif*/
        /* Information frame */
        if (len < 4)
        {
            span_log(&s->logging, SPAN_LOG_FLOW, "!! Received short I-frame (expected 4, got %d)\n", len);
            break;
        }
        /*endif*/
        /* Make sure this is a valid packet */
        if ((frame[1] >> 1) == s->next_expected_frame)
        {
            /* Increment next expected I-frame */
            s->next_expected_frame = (s->next_expected_frame + 1) & 0x7F;
            /* Handle their ACK */
            lapm_ack_rx(s, frame[2] >> 1);
            if ((frame[2] & 0x01))
            {
                /* If the Poll/Final bit is set, send the RR immediately */
                lapm_rr(s, 1);
            }
            /*endif*/
            s->iframe_receive(s->iframe_receive_user_data, frame + 3, len - 4);
            /* Send an RR if one wasn't sent already */
            if (s->last_frame_we_acknowledged != s->next_expected_frame) 
                lapm_rr(s, 0);
            /*endif*/
        }
        else
        {
            if (((s->next_expected_frame - (frame[1] >> 1)) & 127) < s->window_size_k)
            {
                /* It's within our window -- send back an RR */
                lapm_rr(s, 0);
            }
            else
            {
                lapm_reject(s);
            }
            /*endif*/
        }
        /*endif*/
        break;
    case LAPM_FRAMETYPE_S:
        if (s->state != LAPM_DATA)
        {
            span_log(&s->logging, SPAN_LOG_FLOW, "!! Got S-frame while link down\n");
            break;
        }
        /*endif*/
        if (len < 4)
        {
            span_log(&s->logging, SPAN_LOG_FLOW, "!! Received short S-frame (expected 4, got %d)\n", len);
            break;
        }
        /*endif*/
        s_field = frame[octet] & 0xEC;
        switch (s_field)
        {
        case 0x00:
            /* RR (receive ready) */
            s->busy = FALSE;
            /* Acknowledge frames as necessary */
            lapm_ack_rx(s, frame[2] >> 1);
            if ((frame[2] & 0x01))
            {
                /* If P/F is one, respond with an RR with the P/F bit set */
                if (s->solicit_f_bit)
                {
                    span_log(&s->logging, SPAN_LOG_FLOW, "-- Got RR response to our frame\n");
                }
                else
                {
                    span_log(&s->logging, SPAN_LOG_FLOW, "-- Unsolicited RR with P/F bit, responding\n");
                    lapm_rr(s, 1);
                }
                /*endif*/
                s->solicit_f_bit = FALSE;
            }
            /*endif*/
            break;
        case 0x04:
            /* RNR (receive not ready) */
            span_log(&s->logging, SPAN_LOG_FLOW, "-- Got receiver not ready\n");
            s->busy = TRUE;
            break;   
        case 0x08:
            /* REJ (reject) */
            /* Just retransmit */
            span_log(&s->logging, SPAN_LOG_FLOW, "-- Got reject requesting packet %d...  Retransmitting.\n", frame[2] >> 1);
            if ((frame[2] & 0x01))
            {
                /* If it has the poll bit set, send an appropriate supervisory response */
                lapm_rr(s, 1);
            }
            /*endif*/
            sendnow = FALSE;
            /* Resend the appropriate I-frame */
            for (f = s->txqueue;  f;  f = f->next)
            {
                if (sendnow  ||  (f->frame[1] >> 1) == (frame[2] >> 1))
                {
                    /* Matches the request, or follows in our window */
                    sendnow = TRUE;
                    span_log(&s->logging,
                             SPAN_LOG_FLOW,
                             "!! Got reject for frame %d, retransmitting frame %d now, updating n_r!\n",
                             frame[2] >> 1,
                             f->frame[1] >> 1);
                    f->frame[2] = (uint8_t) (s->next_expected_frame << 1);
                    lapm_tx_frame(s, f->frame, f->len);
                }
                /*endif*/
            }
            /*endfor*/
            if (!sendnow)
            {
                if (s->txqueue)
                {
                    /* This should never happen */
                    if ((frame[2] & 0x01) == 0  ||  (frame[2] >> 1))
                    {
                        span_log(&s->logging,
                                 SPAN_LOG_FLOW,
                                 "!! Got reject for frame %d, but we only have others!\n",
                                 frame[2] >> 1);
                    }
                    /*endif*/
                }
                else
                {
                    /* Hrm, we have nothing to send, but have been REJ'd.  Reset last_frame_peer_acknowledged, next_tx_frame, etc */
                    span_log(&s->logging, SPAN_LOG_FLOW, "!! Got reject for frame %d, but we have nothing -- resetting!\n", frame[2] >> 1);
                    s->last_frame_peer_acknowledged =
                    s->next_tx_frame = frame[2] >> 1;
                    /* Reset t401 timer if it was somehow going */
                    if (s->t401_timer >= 0)
                    {
fprintf(stderr, "Deleting T401 f [%p] %d\n", (void *) s, s->t401_timer);
                        span_schedule_del(&s->sched, s->t401_timer);
                        s->t401_timer = -1;
                    }
                    /*endif*/
                    /* Reset and restart t403 timer */
                    if (s->t403_timer >= 0)
                    {
fprintf(stderr, "Deleting T403 g %d\n", s->t403_timer);
                        span_schedule_del(&s->sched, s->t403_timer);
                        s->t403_timer = -1;
                    }
                    /*endif*/
fprintf(stderr, "Setting T403 h\n");
                    s->t403_timer = span_schedule_event(&s->sched, T_403, t403_expired, s);
                }
                /*endif*/
            }
            /*endif*/
            break;
        case 0x0C:
            /* SREJ (selective reject) */
            break;
        default:
            span_log(&s->logging,
                     SPAN_LOG_FLOW,
                     "!! XXX Unknown Supervisory frame sd=0x%02x,pf=%02xnr=%02x vs=%02x, va=%02x XXX\n",
                     s_field,
                     frame[2] & 0x01,
                     frame[2] >> 1,
                     s->next_tx_frame,
                     s->last_frame_peer_acknowledged);
            break;
        }
        /*endswitch*/
        break;
    case LAPM_FRAMETYPE_U:
        if (len < 3)
        {
            span_log(&s->logging, SPAN_LOG_FLOW, "!! Received too short unnumbered frame\n");
            break;
        }
        /*endif*/
        m_field = frame[octet] & 0xEC;
        switch (m_field)
        {
        case 0x00:
            /* UI (unnumbered information) */
            switch (frame[++octet] & 0x7F)
            {
            case 0x40:
                /* BRK */
                span_log(&s->logging, SPAN_LOG_FLOW, "BRK - option %d, length %d\n", (frame[octet] >> 5), frame[octet + 1]);
                octet += 2;
                break;
            case 0x60:
                /* BRKACK */
                span_log(&s->logging, SPAN_LOG_FLOW, "BRKACK\n");
                break;
            default:
                /* Unknown */
                span_log(&s->logging, SPAN_LOG_FLOW, "Unknown UI type\n");
                break;
            }
            /*endswitch*/
            break;
        case 0x0C:
            /* DM (disconnect mode) */
            if ((frame[octet] & 0x10))
            {
                span_log(&s->logging, SPAN_LOG_FLOW, "-- Got Unconnected Mode from peer.\n");
                /* Disconnected mode, try again */
                lapm_link_down(s);
                lapm_restart(s);
            }
            else
            {
                span_log(&s->logging, SPAN_LOG_FLOW, "-- DM (disconnect mode) requesting SABME, starting.\n");
                /* Requesting that we start */
                lapm_restart(s);
            }
            /*endif*/
            break;
        case 0x40:
            /* DISC (disconnect) */
            span_log(&s->logging, SPAN_LOG_FLOW, "-- Got DISC (disconnect) from peer.\n");
            /* Acknowledge */
            lapm_send_ua(s, (frame[octet] & 0x10));
            lapm_link_down(s);
            break;
        case 0x60:
            /* UA (unnumbered acknowledgement) */
            if (s->state == LAPM_ESTABLISH)
            {
                span_log(&s->logging, SPAN_LOG_FLOW, "-- Got UA (unnumbered acknowledgement) from %s peer. Link up.\n", (frame[0] & 0x02)  ?  "xxx"  :  "yyy");
                lapm_link_up(s);
            }
            else
            {
                span_log(&s->logging, SPAN_LOG_FLOW, "!! Got a UA (unnumbered acknowledgement) in state %d\n", s->state);
            }
            /*endif*/
            break;
        case 0x6C:
            /* SABME (set asynchronous balanced mode extended) */
            span_log(&s->logging, SPAN_LOG_FLOW, "-- Got SABME (set asynchronous balanced mode extended) from %s peer.\n", (frame[0] & 0x02)  ?  "yyy"  :  "xxx");
            if ((frame[0] & 0x02))
            {
                s->peer_is_originator = TRUE;
                if (s->we_are_originator)
                {
                    /* We can't both be originators */
                    span_log(&s->logging, SPAN_LOG_FLOW, "We think we are the originator, but they think so too.");
                    break;
                }
                /*endif*/
            }
            else
            {
                s->peer_is_originator = FALSE;
                if (!s->we_are_originator)
                {
                    /* We can't both be answerers */
                    span_log(&s->logging, SPAN_LOG_FLOW, "We think we are the answerer, but they think so too.\n");
                    break;
                }
                /*endif*/
            }
            /*endif*/
            /* Send unnumbered acknowledgement */
            lapm_send_ua(s, (frame[octet] & 0x10));
            lapm_link_up(s);
            break;
        case 0x84:
            /* FRMR (frame reject) */
            span_log(&s->logging, SPAN_LOG_FLOW, "!! FRMR (frame reject).\n");
            break;
        case 0xAC:
            /* XID (exchange identification) */
            span_log(&s->logging, SPAN_LOG_FLOW, "!! XID (exchange identification) frames not supported\n");
            break;
        case 0xE0:
            /* TEST (test) */
            span_log(&s->logging, SPAN_LOG_FLOW, "!! TEST (test) frames not supported\n");
            break;
        default:
            span_log(&s->logging, SPAN_LOG_FLOW, "!! Don't know what to do with M=%X u-frames\n", m_field);
            break;
        }
        /*endswitch*/
        break;
    }
    /*endswitch*/
}
/*- End of function --------------------------------------------------------*/

static void lapm_hdlc_underflow(void *user_data)
{
    lapm_state_t *s;
    uint8_t buf[1024];
    int len;

    s = (lapm_state_t *) user_data;
    span_log(&s->logging, SPAN_LOG_FLOW, "HDLC underflow\n");
    if (s->state == LAPM_DATA)
    {
        if (!queue_empty(s->tx_queue))
        {
            if ((len = queue_read(s->tx_queue, buf, s->n401)) > 0)
                lapm_tx_iframe(s, buf, len, TRUE);
            /*endif*/
        }
        /*endif*/
    }
    /*endif*/
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(void) lapm_restart(lapm_state_t *s)
{
#if 0
    if (s->state != LAPM_RELEASE)
    {
        span_log(&s->logging, SPAN_LOG_FLOW, "!! lapm_restart: Not in 'Link Connection Released' state\n");
        return;
    }
    /*endif*/
#endif
    span_log_init(&s->logging, SPAN_LOG_NONE, NULL);
    span_log_set_protocol(&s->logging, "LAP.M");
    hdlc_tx_init(&s->hdlc_tx, FALSE, 1, TRUE, lapm_hdlc_underflow, s);
    hdlc_rx_init(&s->hdlc_rx, FALSE, FALSE, 1, lapm_receive, s);
    /* TODO: This is a bodge! */
    s->t401_timer = -1;
    s->t402_timer = -1;
    s->t403_timer = -1;
    lapm_reset(s);
    /* TODO: Maybe we should implement T_WAIT? */
    lapm_send_sabme(NULL, s);
}
/*- End of function --------------------------------------------------------*/

#if 0
static void lapm_init(lapm_state_t *s)
{
    lapm_restart(s);
}
/*- End of function --------------------------------------------------------*/
#endif

static void negotiation_rx_bit(v42_state_t *s, int new_bit)
{
    /* DC1 with even parity, 8-16 ones, DC1 with odd parity, 8-16 ones */
    //uint8_t odp = "0100010001 11111111 0100010011 11111111";
    /* V.42 OK E , 8-16 ones, C, 8-16 ones */
    //uint8_t adp_v42 = "0101000101  11111111  0110000101  11111111";
    /* V.42 disabled E, 8-16 ones, NULL, 8-16 ones */
    //uint8_t adp_nov42 = "0101000101  11111111  0000000001  11111111";

    /* There may be no negotiation, so we need to process this data through the
       HDLC receiver as well */
    if (new_bit < 0)
    {
        /* Special conditions */
        span_log(&s->logging, SPAN_LOG_DEBUG, "V.42 rx status is %s (%d)\n", signal_status_to_str(new_bit), new_bit);
        return;
    }
    /*endif*/
    new_bit &= 1;
    s->rxstream = (s->rxstream << 1) | new_bit;
    switch (s->rx_negotiation_step)
    {
    case 0:
        /* Look for some ones */
        if (new_bit)
            break;
        /*endif*/
        s->rx_negotiation_step = 1;
        s->rxbits = 0;
        s->rxstream = ~1;
        s->rxoks = 0;
        break;
    case 1:
        /* Look for the first character */
        if (++s->rxbits < 9)
            break;
        /*endif*/
        s->rxstream &= 0x3FF;
        if (s->calling_party  &&  s->rxstream == 0x145)
        {
            s->rx_negotiation_step++;
        }
        else if (!s->calling_party  &&  s->rxstream == 0x111)
        {
            s->rx_negotiation_step++;
        }
        else
        {
            s->rx_negotiation_step = 0;
        }
        /*endif*/
        s->rxbits = 0;
        s->rxstream = ~0;
        break;
    case 2:
        /* Look for 8 to 16 ones */
        s->rxbits++;
        if (new_bit)
            break;
        /*endif*/
        if (s->rxbits >= 8  &&  s->rxbits <= 16)
            s->rx_negotiation_step++;
        else
            s->rx_negotiation_step = 0;
        /*endif*/
        s->rxbits = 0;
        s->rxstream = ~1;
        break;
    case 3:
        /* Look for the second character */
        if (++s->rxbits < 9)
            break;
        /*endif*/
        s->rxstream &= 0x3FF;
        if (s->calling_party  &&  s->rxstream == 0x185)
        {
            s->rx_negotiation_step++;
        }
        else if (s->calling_party  &&  s->rxstream == 0x001)
        {
            s->rx_negotiation_step++;
        }
        else if (!s->calling_party  &&  s->rxstream == 0x113)
        {
            s->rx_negotiation_step++;
        }
        else
        {
            s->rx_negotiation_step = 0;
        }
        /*endif*/
        s->rxbits = 0;
        s->rxstream = ~0;
        break;
    case 4:
        /* Look for 8 to 16 ones */
        s->rxbits++;
        if (new_bit)
            break;
        /*endif*/
        if (s->rxbits >= 8  &&  s->rxbits <= 16)
        {
            if (++s->rxoks >= 2)
            {
                /* HIT */
                s->rx_negotiation_step++;
                if (s->calling_party)
                {
                    if (s->t400_timer >= 0)
                    {
fprintf(stderr, "Deleting T400 h %d\n", s->t400_timer);
                        span_schedule_del(&s->lapm.sched, s->t400_timer);
                        s->t400_timer = -1;
                    }
                    /*endif*/
                    s->lapm.state = LAPM_ESTABLISH;
                    if (s->lapm.status_callback)
                        s->lapm.status_callback(s->lapm.status_callback_user_data, s->lapm.state);
                    /*endif*/
                }
                else
                {
                    s->odp_seen = TRUE;
                }
                /*endif*/
                break;
            }
            /*endif*/
            s->rx_negotiation_step = 1;
            s->rxbits = 0;
            s->rxstream = ~1;
        }
        else
        {
            s->rx_negotiation_step = 0;
            s->rxbits = 0;
            s->rxstream = ~0;
        }
        /*endif*/
        break;
    case 5:
        /* Parked */
        break;
    }
    /*endswitch*/
}
/*- End of function --------------------------------------------------------*/

static int v42_support_negotiation_tx_bit(v42_state_t *s)
{
    int bit;

    if (s->calling_party)
    {
        if (s->txbits <= 0)
        {
            s->txstream = 0x3FE22;
            s->txbits = 36;
        }
        else if (s->txbits == 18)
        {
            s->txstream = 0x3FF22;
        }
        /*endif*/
        bit = s->txstream & 1;
        s->txstream >>= 1;
        s->txbits--;
    }
    else
    {
        if (s->odp_seen  &&  s->txadps < 10)
        {
            if (s->txbits <= 0)
            {
                if (++s->txadps >= 10)
                {
                    if (s->t400_timer >= 0)
                    {
fprintf(stderr, "Deleting T400 i %d\n", s->t400_timer);
                        span_schedule_del(&s->lapm.sched, s->t400_timer);
                        s->t400_timer = -1;
                    }
                    /*endif*/
                    s->lapm.state = LAPM_ESTABLISH;
                    if (s->lapm.status_callback)
                        s->lapm.status_callback(s->lapm.status_callback_user_data, s->lapm.state);
                    /*endif*/
                    s->txstream = 1;
                }
                else
                {
                    s->txstream = 0x3FE8A;
                    s->txbits = 36;
                }
                /*endif*/
            }
            else if (s->txbits == 18)
            {
                s->txstream = 0x3FE86;
            }
            /*endif*/
            bit = s->txstream & 1;
            s->txstream >>= 1;
            s->txbits--;
        }
        else
        {
            bit = 1;
        }
        /*endif*/
    }
    /*endif*/
    return bit;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(void) v42_rx_bit(void *user_data, int bit)
{
    v42_state_t *s;

    s = (v42_state_t *) user_data;
    if (s->lapm.state == LAPM_DETECT)
        negotiation_rx_bit(s, bit);
    else
        hdlc_rx_put_bit(&s->lapm.hdlc_rx, bit);
    /*endif*/
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) v42_tx_bit(void *user_data)
{
    v42_state_t *s;
    int bit;

    s = (v42_state_t *) user_data;
    if (s->lapm.state == LAPM_DETECT)
        bit = v42_support_negotiation_tx_bit(s);
    else
        bit = hdlc_tx_get_bit(&s->lapm.hdlc_tx);
    /*endif*/
    return bit;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(void) v42_set_status_callback(v42_state_t *s, v42_status_func_t callback, void *user_data)
{
    s->lapm.status_callback = callback;
    s->lapm.status_callback_user_data = user_data;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(void) v42_restart(v42_state_t *s)
{
    span_schedule_init(&s->lapm.sched);

    s->lapm.we_are_originator = s->calling_party;
    lapm_restart(&s->lapm);
    if (s->detect)
    {
        s->txstream = ~0;
        s->txbits = 0;
        s->rxstream = ~0;
        s->rxbits = 0;
        s->rxoks = 0;
        s->txadps = 0;
        s->rx_negotiation_step = 0;
        s->odp_seen = FALSE;
fprintf(stderr, "Setting T400 i\n");
        s->t400_timer = span_schedule_event(&s->lapm.sched, T_400, t400_expired, s);
        s->lapm.state = LAPM_DETECT;
    }
    else
    {
        s->lapm.state = LAPM_ESTABLISH;
    }
    /*endif*/
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(v42_state_t *) v42_init(v42_state_t *s, int calling_party, int detect, v42_frame_handler_t frame_handler, void *user_data)
{
    int alloced;
    
    if (frame_handler == NULL)
        return NULL;
    /*endif*/
    alloced = FALSE;
    if (s == NULL)
    {
        if ((s = (v42_state_t *) malloc(sizeof(*s))) == NULL)
            return NULL;
        alloced = TRUE;
    }
    memset(s, 0, sizeof(*s));
    s->calling_party = calling_party;
    s->detect = detect;
    s->lapm.iframe_receive = frame_handler;
    s->lapm.iframe_receive_user_data = user_data;
    s->lapm.debug |= (LAPM_DEBUG_LAPM_RAW | LAPM_DEBUG_LAPM_DUMP | LAPM_DEBUG_LAPM_STATE);
    s->lapm.t401_timer =
    s->lapm.t402_timer =
    s->lapm.t403_timer = -1;

    if ((s->lapm.tx_queue = queue_init(NULL, 16384, 0)) == NULL)
    {
        if (alloced)
            free(s);
        return NULL;
    }
    /*endif*/
    span_log_init(&s->logging, SPAN_LOG_NONE, NULL);
    span_log_set_protocol(&s->logging, "V.42");
    v42_restart(s);
    return s;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) v42_release(v42_state_t *s)
{
    return 0;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) v42_free(v42_state_t *s)
{
    free(s);
    return 0;
}
/*- End of function --------------------------------------------------------*/
/*- End of file ------------------------------------------------------------*/
