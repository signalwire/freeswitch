/*
 * SpanDSP - a series of DSP components for telephony
 *
 * v42_tests.c
 *
 * Written by Steve Underwood <steveu@coppice.org>
 *
 * Copyright (C) 2004, 2011 Steve Underwood
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

/* THIS IS A WORK IN PROGRESS. IT IS NOT FINISHED. */

/*! \page v42_tests_page V.42 tests
\section v42_tests_page_sec_1 What does it do?
These tests connect two instances of V.42 back to back. V.42 frames are
then exchanged between them.
*/

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>

//#if defined(WITH_SPANDSP_INTERNALS)
#define SPANDSP_EXPOSE_INTERNAL_STRUCTURES
//#endif

#include "spandsp.h"

v42_state_t caller;
v42_state_t answerer;
int variable_length;

int rx_next[3] = {0};
int tx_next[3] = {0};

static void v42_status(void *user_data, int status)
{
    int x;

    x = (intptr_t) user_data;
    if (status < 0)
        printf("%d: Status is '%s' (%d)\n", x, signal_status_to_str(status), status);
    else
        printf("%d: Status is '%s' (%d)\n", x, lapm_status_to_str(status), status);
}
/*- End of function --------------------------------------------------------*/

static int v42_get_frames(void *user_data, uint8_t *msg, int len)
{
    int i;
    int j;
    int k;
    int x;

    if (len < 0)
    {
        v42_status(user_data, len);
        return 0;
    }
    x = (intptr_t) user_data;
    if (variable_length)
    {
        j = make_mask32(len);
        do
            k = j & rand();
        while (k > len);
    }
    else
    {
        k = len;
    }
    for (i = 0;  i < k;  i++)
        msg[i] = tx_next[x]++;
    return k;
}
/*- End of function --------------------------------------------------------*/

static void v42_put_frames(void *user_data, const uint8_t *msg, int len)
{
    int i;
    int x;

    if (len < 0)
    {
        v42_status(user_data, len);
        return;
    }
    x = (intptr_t) user_data;
    for (i = 0;  i < len;  i++)
    {
        if (msg[i] != (rx_next[x] & 0xFF))
        {
            printf("%d: Mismatch 0x%02X 0x%02X\n", x, msg[i], rx_next[x] & 0xFF);
            exit(2);
        }
        rx_next[x]++;
    }
    printf("%d: Got frame len %d\n", x, len);
}
/*- End of function --------------------------------------------------------*/

int main(int argc, char *argv[])
{
    int i;
    int bit;
    int insert_caller_bit_errors;
    int insert_answerer_bit_errors;
    int opt;

    insert_caller_bit_errors = FALSE;
    insert_answerer_bit_errors = FALSE;
    variable_length = FALSE;
    while ((opt = getopt(argc, argv, "bv")) != -1)
    {
        switch (opt)
        {
        case 'b':
            insert_caller_bit_errors = 11000;
            insert_answerer_bit_errors = 10000;
            break;
        case 'v':
            variable_length = TRUE;
            break;
        default:
            //usage();
            exit(2);
            break;
        }
    }

    v42_init(&caller, TRUE, TRUE, v42_get_frames, v42_put_frames, (void *) 1);
    v42_init(&answerer, FALSE, TRUE, v42_get_frames, v42_put_frames, (void *) 2);
    v42_set_status_callback(&caller, v42_status, (void *) 1);
    v42_set_status_callback(&answerer, v42_status, (void *) 2);
    v42_restart(&caller);
    v42_restart(&answerer);

    span_log_set_level(&caller.logging, SPAN_LOG_SHOW_SEVERITY | SPAN_LOG_SHOW_PROTOCOL | SPAN_LOG_SHOW_TAG | SPAN_LOG_DEBUG);
    span_log_set_tag(&caller.logging, "caller");
    span_log_set_level(&answerer.logging, SPAN_LOG_SHOW_SEVERITY | SPAN_LOG_SHOW_PROTOCOL | SPAN_LOG_SHOW_TAG | SPAN_LOG_DEBUG);
    span_log_set_tag(&answerer.logging, "answerer");

    for (i = 0;  i < 1000000;  i++)
    {
        bit = v42_tx_bit(&caller);
        if (insert_caller_bit_errors  &&  i%insert_caller_bit_errors == 0)
            bit ^= 1;
        v42_rx_bit(&answerer, bit);
        bit = v42_tx_bit(&answerer);
        if (insert_answerer_bit_errors  &&  i%insert_answerer_bit_errors == 0)
            bit ^= 1;
        v42_rx_bit(&caller, bit);
    }
    return  0;
}
/*- End of function --------------------------------------------------------*/
/*- End of file ------------------------------------------------------------*/
