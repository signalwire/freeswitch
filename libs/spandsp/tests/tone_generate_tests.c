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
 *
 * $Id: tone_generate_tests.c,v 1.21 2008/11/30 10:17:31 steveu Exp $
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
#include <audiofile.h>

//#if defined(WITH_SPANDSP_INTERNALS)
#define SPANDSP_EXPOSE_INTERNAL_STRUCTURES
//#endif

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
    AFfilehandle outhandle;
    int outframes;

    if ((outhandle = afOpenFile_telephony_write(OUTPUT_FILE_NAME, 1)) == AF_NULL_FILEHANDLE)
    {
        fprintf(stderr, "    Cannot open wave file '%s'\n", OUTPUT_FILE_NAME);
        exit(2);
    }

    /* Try a tone pair */
    make_tone_gen_descriptor(&tone_desc,
                             440,
                             -10,
                             620,
                             -15,
                             100,
                             200,
                             300,
                             400,
                             FALSE);
    tone_gen_init(&tone_state, &tone_desc);

    for (i = 0;  i < 1000;  i++)
    {
        len = tone_gen(&tone_state, amp, 160);
        printf("Generated %d samples\n", len);
        if (len <= 0)
            break;
        outframes = afWriteFrames(outhandle,
                                  AF_DEFAULT_TRACK,
                                  amp,
                                  len);
    }
    
    /* Try a different tone pair */
    make_tone_gen_descriptor(&tone_desc,
                             350,
                             -10,
                             440,
                             -15,
                             400,
                             300,
                             200,
                             100,
                             TRUE);
    tone_gen_init(&tone_state, &tone_desc);

    for (i = 0;  i < 1000;  i++)
    {
        len = tone_gen(&tone_state, amp, 160);
        printf("Generated %d samples\n", len);
        if (len <= 0)
            break;
        outframes = afWriteFrames(outhandle,
                                  AF_DEFAULT_TRACK,
                                  amp,
                                  len);
    }

    /* Try a different tone pair */
    make_tone_gen_descriptor(&tone_desc,
                             400,
                             -10,
                             450,
                             -10,
                             100,
                             200,
                             300,
                             400,
                             TRUE);
    tone_gen_init(&tone_state, &tone_desc);

    for (i = 0;  i < 1000;  i++)
    {
        len = tone_gen(&tone_state, amp, 160);
        printf("Generated %d samples\n", len);
        if (len <= 0)
            break;
        outframes = afWriteFrames(outhandle,
                                  AF_DEFAULT_TRACK,
                                  amp,
                                  len);
    }

    /* Try a single tone */
    make_tone_gen_descriptor(&tone_desc,
                             400,
                             -10,
                             0,
                             0,
                             100,
                             200,
                             300,
                             400,
                             TRUE);
    tone_gen_init(&tone_state, &tone_desc);

    for (i = 0;  i < 1000;  i++)
    {
        len = tone_gen(&tone_state, amp, 160);
        printf("Generated %d samples\n", len);
        if (len <= 0)
            break;
        outframes = afWriteFrames(outhandle,
                                  AF_DEFAULT_TRACK,
                                  amp,
                                  len);
    }

    /* Try a single non-repeating tone */
    make_tone_gen_descriptor(&tone_desc,
                             820,
                             -10,
                             0,
                             0,
                             2000,
                             0,
                             0,
                             0,
                             FALSE);
    tone_gen_init(&tone_state, &tone_desc);

    for (i = 0;  i < 1000;  i++)
    {
        len = tone_gen(&tone_state, amp, 160);
        printf("Generated %d samples\n", len);
        if (len <= 0)
            break;
        outframes = afWriteFrames(outhandle,
                                  AF_DEFAULT_TRACK,
                                  amp,
                                  len);
    }

    /* Try a single non-repeating tone at 0dBm0 */
    make_tone_gen_descriptor(&tone_desc,
                             820,
                             0,
                             0,
                             0,
                             2000,
                             0,
                             0,
                             0,
                             FALSE);
    tone_gen_init(&tone_state, &tone_desc);

    for (i = 0;  i < 1000;  i++)
    {
        len = tone_gen(&tone_state, amp, 160);
        printf("Generated %d samples\n", len);
        if (len <= 0)
            break;
        outframes = afWriteFrames(outhandle,
                                  AF_DEFAULT_TRACK,
                                  amp,
                                  len);
    }

    /* Try an AM modulated tone at a modest modulation level (25%) */
    make_tone_gen_descriptor(&tone_desc,
                             425,
                             -10,
                             -50,
                             25,
                             100,
                             200,
                             300,
                             400,
                             TRUE);
    tone_gen_init(&tone_state, &tone_desc);

    for (i = 0;  i < 1000;  i++)
    {
        len = tone_gen(&tone_state, amp, 160);
        printf("Generated %d samples\n", len);
        if (len <= 0)
            break;
        outframes = afWriteFrames(outhandle,
                                  AF_DEFAULT_TRACK,
                                  amp,
                                  len);
    }
    
    /* Try an AM modulated tone at maximum modulation level (100%) */
    make_tone_gen_descriptor(&tone_desc,
                             425,
                             -10,
                             -50,
                             100,
                             100,
                             200,
                             300,
                             400,
                             TRUE);
    tone_gen_init(&tone_state, &tone_desc);

    for (i = 0;  i < 1000;  i++)
    {
        len = tone_gen(&tone_state, amp, 160);
        printf("Generated %d samples\n", len);
        if (len <= 0)
            break;
        outframes = afWriteFrames(outhandle,
                                  AF_DEFAULT_TRACK,
                                  amp,
                                  len);
    }
    
    if (afCloseFile(outhandle) != 0)
    {
        fprintf(stderr, "    Cannot close wave file '%s'\n", OUTPUT_FILE_NAME);
        exit (2);
    }

    return  0;
}
/*- End of function --------------------------------------------------------*/
/*- End of file ------------------------------------------------------------*/
