/*
 * SpanDSP - a series of DSP components for telephony
 *
 * bell_mf_tx_tests.c - Test the Bell MF generator.
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
 * $Id: bell_mf_tx_tests.c,v 1.14 2008/11/30 10:17:31 steveu Exp $
 */

/*! \file */

/*! \page bell_mf_tx_tests_page Bell MF generation tests
\section bell_mf_tx_tests_page_sec_1 What does it do?
???.

\section bell_mf_tx_tests_page_sec_2 How does it work?
???.
*/

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <string.h>
#include <time.h>
#include <audiofile.h>

//#if defined(WITH_SPANDSP_INTERNALS)
#define SPANDSP_EXPOSE_INTERNAL_STRUCTURES
//#endif

#include "spandsp.h"
#include "spandsp-sim.h"

#define OUTPUT_FILE_NAME    "bell_mf.wav"

int main(int argc, char *argv[])
{
    bell_mf_tx_state_t *gen;
    int16_t amp[16384];
    int len;
    AFfilehandle outhandle;
    int outframes;
    int add_digits;

    if ((outhandle = afOpenFile_telephony_write(OUTPUT_FILE_NAME, 1)) == AF_NULL_FILEHANDLE)
    {
        fprintf(stderr, "    Cannot open wave file '%s'\n", OUTPUT_FILE_NAME);
        exit(2);
    }

    gen = bell_mf_tx_init(NULL);
    len = bell_mf_tx(gen, amp, 16384);
    printf("Generated %d samples\n", len);
    outframes = afWriteFrames(outhandle,
                              AF_DEFAULT_TRACK,
                              amp,
                              len);
    if (bell_mf_tx_put(gen, "123", -1))
        printf("Ooops\n");
    len = bell_mf_tx(gen, amp, 16384);
    printf("Generated %d samples\n", len);
    outframes = afWriteFrames(outhandle,
                              AF_DEFAULT_TRACK,
                              amp,
                              len);
    if (bell_mf_tx_put(gen, "456", -1))
        printf("Ooops\n");
    len = bell_mf_tx(gen, amp, 160);
    printf("Generated %d samples\n", len);
    outframes = afWriteFrames(outhandle,
                              AF_DEFAULT_TRACK,
                              amp,
                              len);
    if (bell_mf_tx_put(gen, "789", -1))
        printf("Ooops\n");
    len = bell_mf_tx(gen, amp, 160);
    printf("Generated %d samples\n", len);
    outframes = afWriteFrames(outhandle,
                              AF_DEFAULT_TRACK,
                              amp,
                              len);
    if (bell_mf_tx_put(gen, "*#", -1))
        printf("Ooops\n");
    len = bell_mf_tx(gen, amp, 160);
    printf("Generated %d samples\n", len);
    outframes = afWriteFrames(outhandle,
                              AF_DEFAULT_TRACK,
                              amp,
                              len);
    add_digits = 1;
    do
    {
        len = bell_mf_tx(gen, amp, 160);
        printf("Generated %d samples\n", len);
        if (len > 0)
        {
            outframes = afWriteFrames(outhandle,
                                      AF_DEFAULT_TRACK,
                                      amp,
                                      len);
        }
        if (add_digits)
        {
            if (bell_mf_tx_put(gen, "1234567890", -1))
            {
                printf("Digit buffer full\n");
                add_digits = 0;
            }
        }
    }
    while (len > 0);

    bell_mf_tx_init(gen);
    len = bell_mf_tx(gen, amp, 16384);
    printf("Generated %d samples\n", len);
    outframes = afWriteFrames(outhandle,
                              AF_DEFAULT_TRACK,
                              amp,
                              len);
    if (bell_mf_tx_put(gen, "123", -1))
        printf("Ooops\n");
    len = bell_mf_tx(gen, amp, 16384);
    printf("Generated %d samples\n", len);
    outframes = afWriteFrames(outhandle,
                              AF_DEFAULT_TRACK,
                              amp,
                              len);
    if (bell_mf_tx_put(gen, "456", -1))
        printf("Ooops\n");
    len = bell_mf_tx(gen, amp, 160);
    printf("Generated %d samples\n", len);
    outframes = afWriteFrames(outhandle,
                              AF_DEFAULT_TRACK,
                              amp,
                              len);
    if (bell_mf_tx_put(gen, "789", -1))
        printf("Ooops\n");
    len = bell_mf_tx(gen, amp, 160);
    printf("Generated %d samples\n", len);
    outframes = afWriteFrames(outhandle,
                              AF_DEFAULT_TRACK,
                              amp,
                              len);
    if (bell_mf_tx_put(gen, "0*#", -1))
        printf("Ooops\n");
    len = bell_mf_tx(gen, amp, 160);
    printf("Generated %d samples\n", len);
    outframes = afWriteFrames(outhandle,
                              AF_DEFAULT_TRACK,
                              amp,
                              len);
    if (bell_mf_tx_put(gen, "ABC", -1))
        printf("Ooops\n");
    len = bell_mf_tx(gen, amp, 160);
    printf("Generated %d samples\n", len);
    outframes = afWriteFrames(outhandle,
                              AF_DEFAULT_TRACK,
                              amp,
                              len);
    add_digits = 1;
    do
    {
        len = bell_mf_tx(gen, amp, 160);
        printf("Generated %d samples\n", len);
        if (len > 0)
        {
            outframes = afWriteFrames(outhandle,
                                      AF_DEFAULT_TRACK,
                                      amp,
                                      len);
        }
        if (add_digits)
        {
            if (bell_mf_tx_put(gen, "1234567890", -1))
            {
                printf("Digit buffer full\n");
                add_digits = 0;
            }
        }
    }
    while (len > 0);

    if (afCloseFile(outhandle) != 0)
    {
        fprintf(stderr, "    Cannot close wave file '%s'\n", OUTPUT_FILE_NAME);
        exit (2);
    }

    return  0;
}
/*- End of function --------------------------------------------------------*/
/*- End of file ------------------------------------------------------------*/
