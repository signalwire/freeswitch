/*
 * SpanDSP - a series of DSP components for telephony
 *
 * v42_tests.c
 *
 * Written by Steve Underwood <steveu@coppice.org>
 *
 * Copyright (C) 2004 Steve Underwood
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
 *
 * $Id: v42_tests.c,v 1.28 2008/11/30 10:17:31 steveu Exp $
 */

/* THIS IS A WORK IN PROGRESS. IT IS NOT FINISHED. */

/*! \page v42_tests_page V.42 tests
\section v42_tests_page_sec_1 What does it do?
These tests connect two instances of V.42 back to back. V.42 frames are
then exchanged between them.
*/

#if defined(HAVE_CONFIG_H)
#include <config.h>
#endif

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>

//#if defined(WITH_SPANDSP_INTERNALS)
#define SPANDSP_EXPOSE_INTERNAL_STRUCTURES
//#endif

#include "spandsp.h"

v42_state_t caller;
v42_state_t answerer;

int rx_next[3] = {0};
int tx_next[3] = {0};

static void v42_status(void *user_data, int status)
{
    int x;
    
    x = (intptr_t) user_data;
    printf("%d: Status is '%s' (%d)\n", x, lapm_status_to_str(status), status);
    //if (status == LAPM_DATA)
    //    lapm_tx_iframe((x == 1)  ?  &caller.lapm  :  &answerer.lapm, "ABCDEFGHIJ", 10, 1);
}

static void v42_frames(void *user_data, const uint8_t *msg, int len)
{
    int i;
    int x;
    
    x = (intptr_t) user_data;
    for (i = 0;  i < len;  i++)
    {
        if (msg[i] != (rx_next[x] & 0xFF))
            printf("%d: Mismatch 0x%02X 0x%02X\n", x, msg[i], rx_next[x] & 0xFF);
        rx_next[x]++;
    }
    printf("%d: Got frame len %d\n", x, len);
}

int main(int argc, char *argv[])
{
    int i;
    int bit;
    uint8_t buf[1024];

    v42_init(&caller, TRUE, TRUE, v42_frames, (void *) 1);
    v42_init(&answerer, FALSE, TRUE, v42_frames, (void *) 2);
    v42_set_status_callback(&caller, v42_status, (void *) 1);
    v42_set_status_callback(&answerer, v42_status, (void *) 2);
    span_log_set_level(&caller.logging, SPAN_LOG_SHOW_SEVERITY | SPAN_LOG_SHOW_PROTOCOL | SPAN_LOG_DEBUG);
    span_log_set_tag(&caller.logging, "caller");
    span_log_set_level(&caller.lapm.logging, SPAN_LOG_SHOW_SEVERITY | SPAN_LOG_SHOW_PROTOCOL | SPAN_LOG_DEBUG);
    span_log_set_tag(&caller.lapm.logging, "caller");
    span_log_set_level(&caller.lapm.sched.logging, SPAN_LOG_SHOW_SEVERITY | SPAN_LOG_SHOW_PROTOCOL | SPAN_LOG_DEBUG);
    span_log_set_tag(&caller.lapm.sched.logging, "caller");
    span_log_set_level(&answerer.logging, SPAN_LOG_SHOW_SEVERITY | SPAN_LOG_SHOW_PROTOCOL | SPAN_LOG_DEBUG);
    span_log_set_tag(&answerer.logging, "answerer");
    span_log_set_level(&answerer.lapm.logging, SPAN_LOG_SHOW_SEVERITY | SPAN_LOG_SHOW_PROTOCOL | SPAN_LOG_DEBUG);
    span_log_set_tag(&answerer.lapm.logging, "answerer");
    span_log_set_level(&answerer.lapm.sched.logging, SPAN_LOG_SHOW_SEVERITY | SPAN_LOG_SHOW_PROTOCOL | SPAN_LOG_DEBUG);
    span_log_set_tag(&answerer.lapm.sched.logging, "answerer");
    for (i = 0;  i < 100000;  i++)
    {
        bit = v42_tx_bit(&caller);
        v42_rx_bit(&answerer, bit);
        bit = v42_tx_bit(&answerer);
        //if (i%10000 == 0)
        //    bit ^= 1;
        v42_rx_bit(&caller, bit);
        span_schedule_update(&caller.lapm.sched, 4);
        span_schedule_update(&answerer.lapm.sched, 4);
        buf[0] = tx_next[1];
        if (lapm_tx(&caller.lapm, buf, 1) == 1)
            tx_next[1]++;
        buf[0] = tx_next[2];
        if (lapm_tx(&answerer.lapm, buf, 1) == 1)
            tx_next[2]++;
    }
    return  0;
}
/*- End of function --------------------------------------------------------*/
/*- End of file ------------------------------------------------------------*/
