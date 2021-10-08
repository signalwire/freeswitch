/*
 * SpanDSP - a series of DSP components for telephony
 *
 * tone_generate_tests.c
 *
 * Written by Steve Underwood <steveu@coppice.org>
 *
 * Copyright (C) 2001 Steve Underwood
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

/*! \page tone_generate_tests_page Tone generation tests
\section tone_generate_tests_page_sec_1 What does it do?
*/

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <string.h>
#include <time.h>
#include <sndfile.h>

#include "spandsp.h"
#include "spandsp-sim.h"

#define OUTPUT_FILE_NAME    "tone_generate.wav"

int main(int argc, char *argv[])
{
    tone_gen_descriptor_t tone_desc;
    tone_gen_state_t tone_state;
    int i;
    int16_t amp[16384];
    int len;
    SNDFILE *outhandle;

    if ((outhandle = sf_open_telephony_write(OUTPUT_FILE_NAME, 1)) == NULL)
    {
        fprintf(stderr, "    Cannot open audio file '%s'\n", OUTPUT_FILE_NAME);
        exit(2);
    }

    /* Try a tone pair */
    tone_gen_descriptor_init(&tone_desc,
                             440,
                             -10,
                             620,
                             -15,
                             100,
                             200,
                             300,
                             400,
                             false);
    tone_gen_init(&tone_state, &tone_desc);
    for (i = 0;  i < 1000;  i++)
    {
        len = tone_gen(&tone_state, amp, 160);
        printf("Generated %d samples\n", len);
        if (len <= 0)
            break;
        sf_writef_short(outhandle, amp, len);
    }
    tone_gen_release(&tone_state);

    /* Try a different tone pair */
    tone_gen_descriptor_init(&tone_desc,
                             350,
                             -10,
                             440,
                             -15,
                             400,
                             300,
                             200,
                             100,
                             true);
    tone_gen_init(&tone_state, &tone_desc);
    for (i = 0;  i < 1000;  i++)
    {
        len = tone_gen(&tone_state, amp, 160);
        printf("Generated %d samples\n", len);
        if (len <= 0)
            break;
        sf_writef_short(outhandle, amp, len);
    }
    tone_gen_release(&tone_state);

    /* Try a different tone pair */
    tone_gen_descriptor_init(&tone_desc,
                             400,
                             -10,
                             450,
                             -10,
                             100,
                             200,
                             300,
                             400,
                             true);
    tone_gen_init(&tone_state, &tone_desc);
    for (i = 0;  i < 1000;  i++)
    {
        len = tone_gen(&tone_state, amp, 160);
        printf("Generated %d samples\n", len);
        if (len <= 0)
            break;
        sf_writef_short(outhandle, amp, len);
    }
    tone_gen_release(&tone_state);

    /* Try a single tone */
    tone_gen_descriptor_init(&tone_desc,
                             400,
                             -10,
                             0,
                             0,
                             100,
                             200,
                             300,
                             400,
                             true);
    tone_gen_init(&tone_state, &tone_desc);
    for (i = 0;  i < 1000;  i++)
    {
        len = tone_gen(&tone_state, amp, 160);
        printf("Generated %d samples\n", len);
        if (len <= 0)
            break;
        sf_writef_short(outhandle, amp, len);
    }
    tone_gen_release(&tone_state);

    /* Try a single non-repeating tone */
    tone_gen_descriptor_init(&tone_desc,
                             820,
                             -10,
                             0,
                             0,
                             2000,
                             0,
                             0,
                             0,
                             false);
    tone_gen_init(&tone_state, &tone_desc);
    for (i = 0;  i < 1000;  i++)
    {
        len = tone_gen(&tone_state, amp, 160);
        printf("Generated %d samples\n", len);
        if (len <= 0)
            break;
        sf_writef_short(outhandle, amp, len);
    }
    tone_gen_release(&tone_state);

    /* Try a single non-repeating tone at 0dBm0 */
    tone_gen_descriptor_init(&tone_desc,
                             820,
                             0,
                             0,
                             0,
                             2000,
                             0,
                             0,
                             0,
                             false);
    tone_gen_init(&tone_state, &tone_desc);
    for (i = 0;  i < 1000;  i++)
    {
        len = tone_gen(&tone_state, amp, 160);
        printf("Generated %d samples\n", len);
        if (len <= 0)
            break;
        sf_writef_short(outhandle, amp, len);
    }
    tone_gen_release(&tone_state);

    /* Try an AM modulated tone at a modest modulation level (25%) */
    tone_gen_descriptor_init(&tone_desc,
                             425,
                             -10,
                             -50,
                             25,
                             100,
                             200,
                             300,
                             400,
                             true);
    tone_gen_init(&tone_state, &tone_desc);
    for (i = 0;  i < 1000;  i++)
    {
        len = tone_gen(&tone_state, amp, 160);
        printf("Generated %d samples\n", len);
        if (len <= 0)
            break;
        sf_writef_short(outhandle, amp, len);
    }
    tone_gen_release(&tone_state);

    /* Try an AM modulated tone at maximum modulation level (100%) */
    tone_gen_descriptor_init(&tone_desc,
                             425,
                             -10,
                             -50,
                             100,
                             100,
                             200,
                             300,
                             400,
                             true);
    tone_gen_init(&tone_state, &tone_desc);
    for (i = 0;  i < 1000;  i++)
    {
        len = tone_gen(&tone_state, amp, 160);
        printf("Generated %d samples\n", len);
        if (len <= 0)
            break;
        sf_writef_short(outhandle, amp, len);
    }
    tone_gen_release(&tone_state);

    if (sf_close_telephony(outhandle))
    {
        fprintf(stderr, "    Cannot close audio file '%s'\n", OUTPUT_FILE_NAME);
        exit (2);
    }

    return 0;
}
/*- End of function --------------------------------------------------------*/
/*- End of file ------------------------------------------------------------*/
