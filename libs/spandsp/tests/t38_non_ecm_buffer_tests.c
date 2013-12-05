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
 */

/*! \file */

/*! \page t38_non_ecm_buffer_tests_page T.38 non-ECM buffer tests
\section t38_non_ecm_buffer_tests_page_sec_1 What does it do?
These tests exercise the flow controlling non-ECM image data buffer
module, used for T.38 gateways.
*/

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <string.h>
#include <assert.h>
#include <errno.h>

#define SPANDSP_EXPOSE_INTERNAL_STRUCTURES

#include "spandsp.h"

/* A pattern of widening gaps between ones, until at 11 apart an
   EOL should be registered */
static const uint8_t spreader[] =
{
    0x55,
    0x55,
    0x55,
    0x55,
    0x55,
    0x55,
    0x55,
    0x55,       /* 1 apart */
    0x22,       /* 2 and 3 apart */
    0x10,       /* 4 apart */
    0x40,       /* 5 apart */
    0x80,       /* 6 apart */
    0x80,       /* 7 apart */
    0x40,       /* 8 apart */
    0x10,       /* 9 apart */
    0x02,       /* 10 apart */
    0x00,
    0x25        /* 11 apart */
};

static int bit_no;

static int xxx(t38_non_ecm_buffer_state_t *s, logging_state_t *l, int log_bits, int n, int expected)
{
    int i;
    int j;
    int bit;

    t38_non_ecm_buffer_inject(s, &spreader[n], 1);
    if (expected >= 0)
    {
        for (i = 0;  i < 128;  i++)
        {
            bit = t38_non_ecm_buffer_get_bit((void *) s);
            if (log_bits)
                printf("Rx bit %d - %d\n", bit_no, bit);
            if (bit != expected)
            {
                printf("Tests failed - %d %d %d\n", bit_no, bit, expected);
                exit(2);
            }
            bit_no++;
        }
    }
    else
    {
        j = -1;
        for (i = 0;  i < 256;  i++)
        {
            bit = t38_non_ecm_buffer_get_bit((void *) s);
            if (log_bits)
                printf("Rx bit %d - %d\n", bit_no, bit);
            if (j < 0)
            {
                if (bit == 1)
                    j = 18*8 - 5;
            }
            else
            {
                expected = (spreader[j >> 3] >> (7 - (j & 7))) & 1;
                if (bit != expected)
                {
                    printf("Tests failed - %d %d %d\n", bit_no, bit, expected);
                    exit(2);
                }
                j++;
                if (j >= 18*8)
                    j = 0;
            }
            bit_no++;
            if (j == 17*8)
                return 0;
        }
    }
    return 0;
}
/*- End of function --------------------------------------------------------*/

int main(int argc, char *argv[])
{
    t38_non_ecm_buffer_state_t buffer;
    logging_state_t logging;
    uint8_t buf[1024];
    int bit;
    int n;
    int log_bits;
    int i;

    log_bits = (argc > 1);
    printf("T.38 non-ECM rate adapting buffer tests.\n");
    span_log_init(&logging, SPAN_LOG_FLOW, NULL);
    span_log_set_protocol(&logging, "Buffer");

    printf("1 - Impose no minimum for the bits per row\n");
    t38_non_ecm_buffer_init(&buffer, true, 0);
    n = 0;
    bit_no = 0;
    /* We should get ones until the buffers recognises an EOL */
    printf("    We should get ones here\n");
    for (i = 0;  i < 17;  i++)
        xxx(&buffer, &logging, log_bits, i, 1);
    printf("    We should change to zeros here\n");
    xxx(&buffer, &logging, log_bits, i, 0);
    for (i = 0;  i < 17;  i++)
        xxx(&buffer, &logging, log_bits, i, 0);
    printf("    We should get the first row here\n");
    xxx(&buffer, &logging, log_bits, i, -1);
    for (i = 0;  i < 17;  i++)
        xxx(&buffer, &logging, log_bits, i, 0);
    printf("    We should get the second row here\n");
    xxx(&buffer, &logging, log_bits, i, -1);
    for (i = 0;  i < 17;  i++)
        xxx(&buffer, &logging, log_bits, i, 0);
    printf("    We should get the third row here\n");
    xxx(&buffer, &logging, log_bits, i, -1);
    printf("    Done\n");
    t38_non_ecm_buffer_report_input_status(&buffer, &logging);
    t38_non_ecm_buffer_report_output_status(&buffer, &logging);

    printf("2 - Impose no minimum for the bits per row, different alignment\n");
    t38_non_ecm_buffer_init(&buffer, true, 0);
    n = 0;
    memset(buf, 0, sizeof(buf));
    /* The first one in this should be seen as the first EOL */
    memset(buf + 10, 0x55, 10);
    /* EOL 2 */
    buf[25] = 0x20;
    /* EOL 3 */
    memset(buf + 30, 0x55, 10);
    /* EOL 4 */
    buf[45] = 0x10;
    t38_non_ecm_buffer_inject(&buffer, buf, 50);
    t38_non_ecm_buffer_push(&buffer);
    for (;;)
    {
        bit = t38_non_ecm_buffer_get_bit((void *) &buffer);
        if (log_bits)
            printf("Rx bit %d - %d\n", n, bit);
        n++;
        if (bit == SIG_STATUS_END_OF_DATA)
        {
            if (n != 337)
            {
                printf("Tests failed\n");
                exit(2);
            }
            break;
        }
        if (n >= 18  &&  n <= 96)
        {
            if (bit == (n & 1))
            {
                printf("Tests failed\n");
                exit(2);
            }
        }
        else if (n >= 178  &&  n <= 256)
        {
            if (bit == (n & 1))
            {
                printf("Tests failed\n");
                exit(2);
            }
        }
        else if (n == 139  ||  n == 300)
        {
            if (bit != 1)
            {
                printf("Tests failed\n");
                exit(2);
            }
        }
        else
        {
            if (bit != 0)
            {
                printf("Tests failed\n");
                exit(2);
            }
        }
    }
    t38_non_ecm_buffer_report_input_status(&buffer, &logging);
    t38_non_ecm_buffer_report_output_status(&buffer, &logging);

    printf("3 - Demand a fairly high minimum for the bits per row\n");
    t38_non_ecm_buffer_init(&buffer, true, 400);
    n = 0;
    memset(buf, 0, sizeof(buf));
    /* The first one in this should be seen as the first EOL */
    memset(buf + 10, 0x55, 10);
    /* EOL 2 */
    buf[25] = 0x08;
    /* EOL 3 */
    memset(buf + 30, 0x55, 10);
    /* EOL 4 */
    buf[45] = 0x04;
    t38_non_ecm_buffer_inject(&buffer, buf, 50);
    t38_non_ecm_buffer_push(&buffer);
    for (;;)
    {
        bit = t38_non_ecm_buffer_get_bit((void *) &buffer);
        if (log_bits)
            printf("Rx bit %d - %d\n", n, bit);
        n++;
        if (bit == SIG_STATUS_END_OF_DATA)
        {
            if (n != 1273)
            {
                printf("Tests failed\n");
                exit(2);
            }
            break;
        }
        if (n >= 18  &&  n <= 96)
        {
            if (bit == (n & 1))
            {
                printf("Tests failed\n");
                exit(2);
            }
        }
        else if (n >= 834  &&  n <= 912)
        {
            if (bit == (n & 1))
            {
                printf("Tests failed\n");
                exit(2);
            }
        }
        else if (n == 429  ||  n == 1238)
        {
            if (bit != 1)
            {
                printf("Tests failed\n");
                exit(2);
            }
        }
        else
        {
            if (bit != 0)
            {
                printf("Tests failed\n");
                exit(2);
            }
        }
    }
    t38_non_ecm_buffer_report_input_status(&buffer, &logging);
    t38_non_ecm_buffer_report_output_status(&buffer, &logging);

    printf("4 - Take some time to get to the first row of the image, output ahead\n");
    t38_non_ecm_buffer_init(&buffer, true, 400);
    n = 0;
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
    printf("    Initial ones OK\n");
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
    printf("    Continuing initial ones OK\n");
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
    printf("    First EOL caused zeros to output OK\n");
    /* Now add another line. We should see the first line come out. This means just the
       23rd bit from now will be a one. */
    buf[0] = 0x00;
    buf[4] = 0x01;
    t38_non_ecm_buffer_inject(&buffer, buf, 5);
    for (i = 0;  i < 1000;  i++)
    {
        bit = t38_non_ecm_buffer_get_bit((void *) &buffer);
        if (log_bits)
            printf("Rx bit %d - %d\n", n++, bit);
        if ((i == 23  &&  bit == 0)  ||  (i != 23  &&  bit != 0))
        {
            printf("Tests failed (%d)\n", i);
            exit(2);
        }
    }
    printf("    Second EOL caused the first row to output OK\n");
    /* Now inject an RTC - 6 EOLs */
    memset(buf, 0, sizeof(buf));
    /* T.4 1D style */
    for (i = 10;  i < 19;  i += 3)
    {
        buf[i] = 0x00;
        buf[i + 1] = 0x10;
        buf[i + 2] = 0x01;
    }
    /* T.4 2D style */
    buf[25 + 0] = 0x00;
    buf[25 + 1] = 0x18;
    buf[25 + 2] = 0x00;
    buf[25 + 3] = 0xC0;
    buf[25 + 4] = 0x06;
    buf[25 + 5] = 0x00;
    buf[25 + 6] = 0x30;
    buf[25 + 7] = 0x01;
    buf[25 + 8] = 0x80;
    buf[25 + 9] = 0x0C;
    t38_non_ecm_buffer_inject(&buffer, buf, 50);
    t38_non_ecm_buffer_push(&buffer);
    for (i = 0;  i < 1000;  i++)
    {
        bit = t38_non_ecm_buffer_get_bit((void *) &buffer);
        if (log_bits)
            printf("Rx bit %d - %d\n", n++, bit);
        if (i == 7
            ||
            i == 400 + 11 + 0*12
            ||
            i == 400 + 11 + 1*12
            ||
            i == 400 + 11 + 2*12
            ||
            i == 400 + 11 + 3*12
            ||
            i == 400 + 11 + 4*12
            ||
            i == 400 + 11 + 5*12
            ||
            i == 400 + 11 + 60 + 400 + 4 + 0*13
            ||
            i == 400 + 11 + 60 + 400 + 4 + 0*13 + 1
            ||
            i == 400 + 11 + 60 + 400 + 4 + 1*13
            ||
            i == 400 + 11 + 60 + 400 + 4 + 1*13 + 1
            ||
            i == 400 + 11 + 60 + 400 + 4 + 2*13
            ||
            i == 400 + 11 + 60 + 400 + 4 + 2*13 + 1
            ||
            i == 400 + 11 + 60 + 400 + 4 + 3*13
            ||
            i == 400 + 11 + 60 + 400 + 4 + 3*13 + 1
            ||
            i == 400 + 11 + 60 + 400 + 4 + 4*13
            ||
            i == 400 + 11 + 60 + 400 + 4 + 4*13 + 1
            ||
            i == 400 + 11 + 60 + 400 + 4 + 5*13
            ||
            i == 400 + 11 + 60 + 400 + 4 + 5*13 + 1)
        {
            if (bit == 0)
            {
                printf("Tests failed (%d)\n", i);
                exit(2);
            }
        }
        else
        {
            if (bit == 1)
            {
                printf("Tests failed (%d)\n", i);
                exit(2);
            }
        }
    }
    printf("    RTC output OK\n");
    t38_non_ecm_buffer_report_input_status(&buffer, &logging);
    t38_non_ecm_buffer_report_output_status(&buffer, &logging);

    printf("5 - Take some time to get to the first row of the image, output behind\n");
    t38_non_ecm_buffer_init(&buffer, true, 400);
    n = 0;
    /* Inject some ones. */
    memset(buf, 0xFF, 100);
    t38_non_ecm_buffer_inject(&buffer, buf, 100);
    /* Inject some zeros */
    memset(buf, 0, sizeof(buf));
    t38_non_ecm_buffer_inject(&buffer, buf, 100);
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
    printf("    Initial ones OK\n");
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
    printf("    First EOL caused zeros to output OK\n");
    /* Now add another line. We should see the first line come out. This means just the
       23rd bit from now will be a one. */
    buf[0] = 0x00;
    buf[4] = 0x01;
    t38_non_ecm_buffer_inject(&buffer, buf, 5);
    for (i = 0;  i < 1000;  i++)
    {
        bit = t38_non_ecm_buffer_get_bit((void *) &buffer);
        if (log_bits)
            printf("Rx bit %d - %d\n", n++, bit);
        if ((i == 23  &&  bit == 0)  ||  (i != 23  &&  bit != 0))
        {
            printf("Tests failed (%d)\n", i);
            exit(2);
        }
    }
    printf("    Second EOL caused the first row to output OK\n");
    /* Now inject an RTC - 6 EOLs */
    memset(buf, 0, sizeof(buf));
    /* T.4 1D style */
    for (i = 10;  i < 19;  i += 3)
    {
        buf[i] = 0x00;
        buf[i + 1] = 0x10;
        buf[i + 2] = 0x01;
    }
    /* T.4 2D style */
    buf[25 + 0] = 0x00;
    buf[25 + 1] = 0x18;
    buf[25 + 2] = 0x00;
    buf[25 + 3] = 0xC0;
    buf[25 + 4] = 0x06;
    buf[25 + 5] = 0x00;
    buf[25 + 6] = 0x30;
    buf[25 + 7] = 0x01;
    buf[25 + 8] = 0x80;
    buf[25 + 9] = 0x0C;
    t38_non_ecm_buffer_inject(&buffer, buf, 50);
    t38_non_ecm_buffer_push(&buffer);
    for (i = 0;  i < 1000;  i++)
    {
        bit = t38_non_ecm_buffer_get_bit((void *) &buffer);
        if (log_bits)
            printf("Rx bit %d - %d\n", n++, bit);
        if (i == 7
            ||
            i == 400 + 11 + 0*12
            ||
            i == 400 + 11 + 1*12
            ||
            i == 400 + 11 + 2*12
            ||
            i == 400 + 11 + 3*12
            ||
            i == 400 + 11 + 4*12
            ||
            i == 400 + 11 + 5*12
            ||
            i == 400 + 11 + 60 + 400 + 4 + 0*13
            ||
            i == 400 + 11 + 60 + 400 + 4 + 0*13 + 1
            ||
            i == 400 + 11 + 60 + 400 + 4 + 1*13
            ||
            i == 400 + 11 + 60 + 400 + 4 + 1*13 + 1
            ||
            i == 400 + 11 + 60 + 400 + 4 + 2*13
            ||
            i == 400 + 11 + 60 + 400 + 4 + 2*13 + 1
            ||
            i == 400 + 11 + 60 + 400 + 4 + 3*13
            ||
            i == 400 + 11 + 60 + 400 + 4 + 3*13 + 1
            ||
            i == 400 + 11 + 60 + 400 + 4 + 4*13
            ||
            i == 400 + 11 + 60 + 400 + 4 + 4*13 + 1
            ||
            i == 400 + 11 + 60 + 400 + 4 + 5*13
            ||
            i == 400 + 11 + 60 + 400 + 4 + 5*13 + 1)
        {
            if (bit == 0)
            {
                printf("Tests failed (%d)\n", i);
                exit(2);
            }
        }
        else
        {
            if (bit == 1)
            {
                printf("Tests failed (%d)\n", i);
                exit(2);
            }
        }
    }
    printf("    RTC output OK\n");
    t38_non_ecm_buffer_report_input_status(&buffer, &logging);
    t38_non_ecm_buffer_report_output_status(&buffer, &logging);

    printf("6 - TCF without leading ones\n");
    t38_non_ecm_buffer_init(&buffer, false, 400);
    n = 0;
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
    printf("    Initial ones from an empty TCF buffer OK\n");
    /* Now send some TCF through, and see that it comes out */
    memset(buf, 0x00, sizeof(buf));
    t38_non_ecm_buffer_inject(&buffer, buf, 500);
    t38_non_ecm_buffer_push(&buffer);
    for (i = 0;  i < 500*8;  i++)
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
    printf("    Passthrough of TCF OK\n");
    /* Check the right number of bits was buffered */
    bit = t38_non_ecm_buffer_get_bit((void *) &buffer);
    if (log_bits)
        printf("Rx bit %d - %d\n", n++, bit);
    if (bit != SIG_STATUS_END_OF_DATA)
    {
        printf("Tests failed\n");
        exit(2);
    }
    printf("    End of data seen OK\n");
    t38_non_ecm_buffer_report_input_status(&buffer, &logging);
    t38_non_ecm_buffer_report_output_status(&buffer, &logging);

    printf("7 - TCF with leading ones\n");
    t38_non_ecm_buffer_init(&buffer, false, 400);
    n = 0;
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
    printf("    Initial ones from an empty TCF buffer OK\n");

    /* Now send some initial ones, and see that we continue to get all ones
       as the stuffing. */
    memset(buf, 0xFF, 500);
    t38_non_ecm_buffer_inject(&buffer, buf, 500);
    for (i = 0;  i < 500*8;  i++)
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
    printf("    Sustaining ones OK\n");

    /* Now send some initial ones, and some TCF through, and see that only
       the TCF comes out */
    memset(buf, 0x00, sizeof(buf));
    memset(buf, 0xFF, 100);
    /* End the ones mid byte */
    buf[100] = 0xF0;
    t38_non_ecm_buffer_inject(&buffer, buf, 500);
    t38_non_ecm_buffer_push(&buffer);
    for (i = 0;  i < 400*8;  i++)
    {
        bit = t38_non_ecm_buffer_get_bit((void *) &buffer);
        if (log_bits)
            printf("Rx bit %d - %d\n", n++, bit);
        if ((i < 4  &&  bit == 0)  ||  (i >= 4  &&  bit != 0))
        {
            printf("Tests failed\n");
            exit(2);
        }
    }
    printf("    Passthrough of TCF OK\n");
    /* Check the right number of bits was buffered */
    bit = t38_non_ecm_buffer_get_bit((void *) &buffer);
    if (log_bits)
        printf("Rx bit %d - %d\n", n++, bit);
    if (bit != SIG_STATUS_END_OF_DATA)
    {
        printf("Tests failed\n");
        exit(2);
    }
    printf("    End of data seen OK\n");
    t38_non_ecm_buffer_report_input_status(&buffer, &logging);
    t38_non_ecm_buffer_report_output_status(&buffer, &logging);

    printf("Tests passed\n");
    return 0;
}
/*- End of function --------------------------------------------------------*/
/*- End of file ------------------------------------------------------------*/
