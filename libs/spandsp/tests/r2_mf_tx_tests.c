/*
 * SpanDSP - a series of DSP components for telephony
 *
 * r2_mf_tx_tests.c - Test the Bell MF generator.
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
 */

/*! \file */

/*! \page r2_mf_tx_tests_page R2 MF generation tests
\section r2_mf_tx_tests_page_sec_1 What does it do?
???.

\section r2_mf_tx_tests_page_sec_2 How does it work?
???.
*/

/* Enable the following definition to enable direct probing into the FAX structures */
//#define WITH_SPANDSP_INTERNALS

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <string.h>
#include <time.h>
#include <sndfile.h>

//#if defined(WITH_SPANDSP_INTERNALS)
#define SPANDSP_EXPOSE_INTERNAL_STRUCTURES
//#endif

#include "spandsp.h"
#include "spandsp-sim.h"

#define OUTPUT_FILE_NAME    "r2_mf.wav"

int main(int argc, char *argv[])
{
    r2_mf_tx_state_t gen;
    int16_t amp[1000];
    int len;
    SNDFILE *outhandle;
    int digit;
    const char *digits = "0123456789BCDEF";

    if ((outhandle = sf_open_telephony_write(OUTPUT_FILE_NAME, 1)) == NULL)
    {
        fprintf(stderr, "    Cannot open audio file '%s'\n", OUTPUT_FILE_NAME);
        exit(2);
    }

    r2_mf_tx_init(&gen, FALSE);
    for (digit = 0;  digits[digit];  digit++)
    {
        r2_mf_tx_put(&gen, digits[digit]);
        len = r2_mf_tx(&gen, amp, 1000);
        printf("Generated %d samples of %c\n", len, digits[digit]);
        if (len > 0)
            sf_writef_short(outhandle, amp, len);
        r2_mf_tx_put(&gen, 0);
        len = r2_mf_tx(&gen, amp, 1000);
        printf("Generated %d samples\n", len);
        if (len > 0)
            sf_writef_short(outhandle, amp, len);
    }

    r2_mf_tx_init(&gen, TRUE);
    for (digit = 0;  digits[digit];  digit++)
    {
        r2_mf_tx_put(&gen, digits[digit]);
        len = r2_mf_tx(&gen, amp, 1000);
        printf("Generated %d samples of %c\n", len, digits[digit]);
        if (len > 0)
            sf_writef_short(outhandle, amp, len);
        r2_mf_tx_put(&gen, 0);
        len = r2_mf_tx(&gen, amp, 1000);
        printf("Generated %d samples\n", len);
        if (len > 0)
            sf_writef_short(outhandle, amp, len);
    }

    if (sf_close_telephony(outhandle))
    {
        fprintf(stderr, "    Cannot close audio file '%s'\n", OUTPUT_FILE_NAME);
        exit (2);
    }

    return 0;
}
/*- End of function --------------------------------------------------------*/
/*- End of file ------------------------------------------------------------*/
