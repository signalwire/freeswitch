/*
 * SpanDSP - a series of DSP components for telephony
 *
 * t38_non_ecm_buffer_tests.c - Tests for the T.38 non-ECM image data buffer module.
 *
 * Written by Steve Underwood <steveu@coppice.org>
 *
 * Copyright (C) 2008 Steve Underwood
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
 * $Id: t38_non_ecm_buffer_tests.c,v 1.5 2009/04/25 14:17:47 steveu Exp $
 */

/*! \file */

/*! \page t38_non_ecm_buffer_tests_page T.38 non-ECM buffer tests
\section t38_non_ecm_buffer_tests_page_sec_1 What does it do?
These tests exercise the flow controlling non-ECM image data buffer
module, used for T.38 gateways.
*/

#if defined(HAVE_CONFIG_H)
#include <config.h>
#endif

#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <string.h>
#include <assert.h>
#include <errno.h>

//#if defined(WITH_SPANDSP_INTERNALS)
#define SPANDSP_EXPOSE_INTERNAL_STRUCTURES
//#endif

#include "spandsp.h"

int main(int argc, char *argv[])
{
    t38_non_ecm_buffer_state_t buffer;
    logging_state_t logging;
    uint8_t buf[1024];
    int bit;
    int n;
    int log_bits;
    int i;

    log_bits = FALSE;
    span_log_init(&logging, SPAN_LOG_FLOW, NULL);
    span_log_set_protocol(&logging, "Buffer");

    t38_non_ecm_buffer_init(&buffer, TRUE, 0);
    memset(buf, 0, sizeof(buf));
    memset(buf + 10, 0x55, 10);
    buf[25] = 0x80;
    memset(buf + 30, 0x55, 10);
    t38_non_ecm_buffer_inject(&buffer, buf, 50);
    t38_non_ecm_buffer_push(&buffer);
    n = 0;
    do
    {
        bit = t38_non_ecm_buffer_get_bit((void *) &buffer);
        if (log_bits)
            printf("Rx bit %d - %d\n", n++, bit);
    }
    while (bit >= 0);
    t38_non_ecm_buffer_report_input_status(&buffer, &logging);
    t38_non_ecm_buffer_report_output_status(&buffer, &logging);

    t38_non_ecm_buffer_init(&buffer, TRUE, 0);
    memset(buf, 0, sizeof(buf));
    memset(buf + 10, 0x55, 10);
    buf[25] = 0x40;
    memset(buf + 30, 0x55, 10);
    t38_non_ecm_buffer_inject(&buffer, buf, 50);
    t38_non_ecm_buffer_push(&buffer);
    n = 0;
    do
    {
        bit = t38_non_ecm_buffer_get_bit((void *) &buffer);
        if (log_bits)
            printf("Rx bit %d - %d\n", n++, bit);
    }
    while (bit >= 0);
    t38_non_ecm_buffer_report_input_status(&buffer, &logging);
    t38_non_ecm_buffer_report_output_status(&buffer, &logging);

    t38_non_ecm_buffer_init(&buffer, TRUE, 400);
    memset(buf, 0, sizeof(buf));
    memset(buf + 10, 0x55, 10);
    buf[25] = 0x01;
    memset(buf + 30, 0x55, 10);
    t38_non_ecm_buffer_inject(&buffer, buf, 50);
    t38_non_ecm_buffer_push(&buffer);
    n = 0;
    do
    {
        bit = t38_non_ecm_buffer_get_bit((void *) &buffer);
        if (log_bits)
            printf("Rx bit %d - %d\n", n++, bit);
    }
    while (bit >= 0);
    t38_non_ecm_buffer_report_input_status(&buffer, &logging);
    t38_non_ecm_buffer_report_output_status(&buffer, &logging);

    t38_non_ecm_buffer_init(&buffer, TRUE, 400);
    /* Get some initial bits from an empty buffer. These should be ones */
    for (i = 0;  i < 1000;  i++)
    {
        bit = t38_non_ecm_buffer_get_bit((void *) &buffer);
        if (log_bits)
            printf("Rx bit %d - %d\n", n++, bit);
        if (bit != 1)
        {
            printf("Tests failed\n");
            exit(2);
        }
    }
    /* Now put some zeros into the buffer, but no EOL. We should continue
       getting ones out. */
    memset(buf, 0, sizeof(buf));
    t38_non_ecm_buffer_inject(&buffer, buf, 20);
    for (i = 0;  i < 1000;  i++)
    {
        bit = t38_non_ecm_buffer_get_bit((void *) &buffer);
        if (log_bits)
            printf("Rx bit %d - %d\n", n++, bit);
        if (bit != 1)
        {
            printf("Tests failed\n");
            exit(2);
        }
    }
    /* Now add a one, to make an EOL. We should see the zeros come out. */
    buf[0] = 0x01;
    t38_non_ecm_buffer_inject(&buffer, buf, 1);
    for (i = 0;  i < 1000;  i++)
    {
        bit = t38_non_ecm_buffer_get_bit((void *) &buffer);
        if (log_bits)
            printf("Rx bit %d - %d\n", n++, bit);
        if (bit != 0)
        {
            printf("Tests failed\n");
            exit(2);
        }
    }
    /* Now add another line. We should see the first line come out. This means just the
       eighth bit from now will be a one. */
    buf[0] = 0x00;
    buf[4] = 0x01;
    t38_non_ecm_buffer_inject(&buffer, buf, 5);
    for (i = 0;  i < 1000;  i++)
    {
        bit = t38_non_ecm_buffer_get_bit((void *) &buffer);
        if (log_bits)
            printf("Rx bit %d - %d\n", n++, bit);
        if (i != 7  &&  bit != 0)
        {
            printf("Tests failed (%d)\n", i);
            exit(2);
        }
    }
    t38_non_ecm_buffer_report_input_status(&buffer, &logging);
    t38_non_ecm_buffer_report_output_status(&buffer, &logging);

    printf("Tests passed\n");
    return  0;
}
/*- End of function --------------------------------------------------------*/
/*- End of file ------------------------------------------------------------*/
