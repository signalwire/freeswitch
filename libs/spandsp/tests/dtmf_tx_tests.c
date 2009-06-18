/*
 * SpanDSP - a series of DSP components for telephony
 *
 * dtmf_tx_tests.c - Test the DTMF generator.
 *
 * Written by Steve Underwood <steveu@coppice.org>
 *
 * Copyright (C) 2003 Steve Underwood
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
 * $Id: dtmf_tx_tests.c,v 1.23 2009/05/30 15:23:13 steveu Exp $
 */

/*! \file */

/*! \page dtmf_tx_tests_page DTMF generation tests
\section dtmf_tx_tests_page_sec_1 What does it do?
???.

\section dtmf_tx_tests_page_sec_2 How does it work?
???.
*/

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <fcntl.h>
#include <time.h>
#include <sndfile.h>

//#if defined(WITH_SPANDSP_INTERNALS)
#define SPANDSP_EXPOSE_INTERNAL_STRUCTURES
//#endif

#include "spandsp.h"
#include "spandsp-sim.h"

#define OUTPUT_FILE_NAME    "dtmf.wav"

int main(int argc, char *argv[])
{
    dtmf_tx_state_t *gen;
    int16_t amp[16384];
    int len;
    SNDFILE *outhandle;
    int outframes;
    int add_digits;

    if ((outhandle = sf_open_telephony_write(OUTPUT_FILE_NAME, 1)) == NULL)
    {
        fprintf(stderr, "    Cannot open audio file '%s'\n", OUTPUT_FILE_NAME);
        exit(2);
    }

    gen = dtmf_tx_init(NULL);
    len = dtmf_tx(gen, amp, 16384);
    printf("Generated %d samples\n", len);
    outframes = sf_writef_short(outhandle, amp, len);
    if (dtmf_tx_put(gen, "123", -1))
    {
        printf("Ooops\n");
        exit(2);
    }
    len = dtmf_tx(gen, amp, 16384);
    printf("Generated %d samples\n", len);
    outframes = sf_writef_short(outhandle, amp, len);
    if (dtmf_tx_put(gen, "456", -1))
    {
        printf("Ooops\n");
        exit(2);
    }
    len = dtmf_tx(gen, amp, 160);
    printf("Generated %d samples\n", len);
    outframes = sf_writef_short(outhandle, amp, len);
    if (dtmf_tx_put(gen, "789", -1))
    {
        printf("Ooops\n");
        exit(2);
    }
    len = dtmf_tx(gen, amp, 160);
    printf("Generated %d samples\n", len);
    outframes = sf_writef_short(outhandle, amp, len);
    if (dtmf_tx_put(gen, "*#", -1))
    {
        printf("Ooops\n");
        exit(2);
    }
    len = dtmf_tx(gen, amp, 160);
    printf("Generated %d samples\n", len);
    outframes = sf_writef_short(outhandle, amp, len);
    add_digits = 1;
    do
    {
        len = dtmf_tx(gen, amp, 160);
        printf("Generated %d samples\n", len);
        if (len > 0)
        {
            outframes = sf_writef_short(outhandle, amp, len);
        }
        if (add_digits)
        {
            if (dtmf_tx_put(gen, "1234567890", -1))
            {
                printf("Digit buffer full\n");
                add_digits = 0;
            }
        }
    }
    while (len > 0);

    dtmf_tx_init(gen);
    len = dtmf_tx(gen, amp, 16384);
    printf("Generated %d samples\n", len);
    outframes = sf_writef_short(outhandle, amp, len);
    if (dtmf_tx_put(gen, "123", -1))
    {
        printf("Ooops\n");
        exit(2);
    }
    len = dtmf_tx(gen, amp, 16384);
    printf("Generated %d samples\n", len);
    outframes = sf_writef_short(outhandle, amp, len);
    if (dtmf_tx_put(gen, "456", -1))
    {
        printf("Ooops\n");
        exit(2);
    }
    len = dtmf_tx(gen, amp, 160);
    printf("Generated %d samples\n", len);
    outframes = sf_writef_short(outhandle, amp, len);
    if (dtmf_tx_put(gen, "789", -1))
    {
        printf("Ooops\n");
        exit(2);
    }
    len = dtmf_tx(gen, amp, 160);
    printf("Generated %d samples\n", len);
    outframes = sf_writef_short(outhandle, amp, len);
    if (dtmf_tx_put(gen, "0*#", -1))
    {
        printf("Ooops\n");
        exit(2);
    }
    len = dtmf_tx(gen, amp, 160);
    printf("Generated %d samples\n", len);
    outframes = sf_writef_short(outhandle, amp, len);
    if (dtmf_tx_put(gen, "ABCD", -1))
    {
        printf("Ooops\n");
        exit(2);
    }
    len = dtmf_tx(gen, amp, 160);
    printf("Generated %d samples\n", len);
    outframes = sf_writef_short(outhandle, amp, len);

    /* Try modifying the level and length of the digits */
    printf("Try different levels and timing\n");
    dtmf_tx_set_level(gen, -20, 5);
    dtmf_tx_set_timing(gen, 100, 200);
    if (dtmf_tx_put(gen, "123", -1))
    {
        printf("Ooops\n");
        exit(2);
    }
    do
    {
        len = dtmf_tx(gen, amp, 160);
        printf("Generated %d samples\n", len);
        if (len > 0)
            outframes = sf_writef_short(outhandle, amp, len);
    }
    while (len > 0);
    printf("Restore normal levels and timing\n");
    dtmf_tx_set_level(gen, -10, 0);
    dtmf_tx_set_timing(gen, 50, 55);
    if (dtmf_tx_put(gen, "A", -1))
    {
        printf("Ooops\n");
        exit(2);
    }

    add_digits = TRUE;
    do
    {
        len = dtmf_tx(gen, amp, 160);
        printf("Generated %d samples\n", len);
        if (len > 0)
        {
            outframes = sf_writef_short(outhandle, amp, len);
        }
        if (add_digits)
        {
            if (dtmf_tx_put(gen, "1234567890", -1))
            {
                printf("Digit buffer full\n");
                add_digits = FALSE;
            }
        }
    }
    while (len > 0);

    if (sf_close(outhandle) != 0)
    {
        fprintf(stderr, "    Cannot close audio file '%s'\n", OUTPUT_FILE_NAME);
        exit(2);
    }

    return  0;
}
/*- End of function --------------------------------------------------------*/
/*- End of file ------------------------------------------------------------*/
