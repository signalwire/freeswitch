/*
 * SpanDSP - a series of DSP components for telephony
 *
 * dds_tests.c
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
 * $Id: dds_tests.c,v 1.24 2008/05/13 13:17:25 steveu Exp $
 */

/*! \file */

/*! \page dds_tests_page Direct digital synthesis tests
\section dds_tests_page_sec_1 What does it do?
???.

\section dds_tests_page_sec_2 How does it work?
???.
*/

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <memory.h>
#include <audiofile.h>

#include "spandsp.h"

#define OUTPUT_FILE_NAME            "dds.wav"
#define OUTPUT_FILE_NAME_COMPLEX    "complex_dds.wav"

#define SAMPLES_PER_CHUNK           8000

int main(int argc, char *argv[])
{
    int i;
    uint32_t phase;
    int32_t phase_inc;
    int outframes;
    complexf_t camp;
    int16_t buf[2*SAMPLES_PER_CHUNK];
    AFfilehandle outhandle;
    AFfilesetup filesetup;
    power_meter_t meter;
    power_meter_t meter_i;
    power_meter_t meter_q;
    int scale;

    power_meter_init(&meter, 10);

    printf("Non-complex DDS tests.\n");
    if ((filesetup = afNewFileSetup()) == AF_NULL_FILESETUP)
    {
        fprintf(stderr, "    Failed to create file setup\n");
        exit(2);
    }
    afInitSampleFormat(filesetup, AF_DEFAULT_TRACK, AF_SAMPFMT_TWOSCOMP, 16);
    afInitRate(filesetup, AF_DEFAULT_TRACK, (float) SAMPLE_RATE);
    afInitFileFormat(filesetup, AF_FILE_WAVE);
    afInitChannels(filesetup, AF_DEFAULT_TRACK, 1);

    if ((outhandle = afOpenFile(OUTPUT_FILE_NAME, "w", filesetup)) == AF_NULL_FILEHANDLE)
    {
        fprintf(stderr, "    Cannot create wave file '%s'\n", OUTPUT_FILE_NAME);
        exit(2);
    }

    phase = 0;

    printf("Test with 123.456789Hz.\n");
    phase_inc = dds_phase_rate(123.456789f);
    scale = dds_scaling_dbm0(-10.0f);
    for (i = 0;  i < SAMPLES_PER_CHUNK;  i++)
    {
        buf[i] = alaw_to_linear(linear_to_alaw((dds(&phase, phase_inc)*scale) >> 15));
        power_meter_update(&meter, buf[i]);
    }
    outframes = afWriteFrames(outhandle,
                              AF_DEFAULT_TRACK,
                              buf,
                              SAMPLES_PER_CHUNK);
    if (outframes != SAMPLES_PER_CHUNK)
    {
        fprintf(stderr, "    Error writing wave file\n");
        exit(2);
    }
    printf("Level is %fdBOv/%fdBm0\n", power_meter_current_dbov(&meter), power_meter_current_dbm0(&meter));
    if (fabs(power_meter_current_dbm0(&meter) + 10.0f) > 0.05f)
    {
        printf("Test failed.\n");
        exit(2);
    }

    printf("Test with 12.3456789Hz.\n");
    phase_inc = dds_phase_rate(12.3456789f);
    for (i = 0;  i < SAMPLES_PER_CHUNK;  i++)
    {
        buf[i] = alaw_to_linear(linear_to_alaw(dds(&phase, phase_inc)));
        power_meter_update(&meter, buf[i]);
    }
    outframes = afWriteFrames(outhandle,
                              AF_DEFAULT_TRACK,
                              buf,
                              SAMPLES_PER_CHUNK);
    if (outframes != SAMPLES_PER_CHUNK)
    {
        fprintf(stderr, "    Error writing wave file\n");
        exit(2);
    }
    printf("Level is %fdBOv/%fdBm0\n", power_meter_current_dbov(&meter), power_meter_current_dbm0(&meter));
    /* Use a wider tolerance for this very low frequency - the power meter will ripple */
    if (fabs(power_meter_current_dbov(&meter) + 3.02f) > 0.2f)
    {
        printf("Test failed.\n");
        exit(2);
    }

    printf("Test with 2345.6789Hz.\n");
    phase_inc = dds_phase_rate(2345.6789f);
    scale = dds_scaling_dbov(-10.0f);
    for (i = 0;  i < SAMPLES_PER_CHUNK;  i++)
    {
        buf[i] = alaw_to_linear(linear_to_alaw((dds(&phase, phase_inc)*scale) >> 15));
        power_meter_update(&meter, buf[i]);
    }
    outframes = afWriteFrames(outhandle,
                              AF_DEFAULT_TRACK,
                              buf,
                              SAMPLES_PER_CHUNK);
    if (outframes != SAMPLES_PER_CHUNK)
    {
        fprintf(stderr, "    Error writing wave file\n");
        exit(2);
    }
    printf("Level is %fdBOv/%fdBm0\n", power_meter_current_dbov(&meter), power_meter_current_dbm0(&meter));
    if (fabs(power_meter_current_dbov(&meter) + 10.0f) > 0.05f)
    {
        printf("Test failed.\n");
        exit(2);
    }

    printf("Test with 3456.789Hz.\n");
    phase_inc = dds_phase_rate(3456.789f);
    for (i = 0;  i < SAMPLES_PER_CHUNK;  i++)
    {
        buf[i] = alaw_to_linear(linear_to_alaw(dds(&phase, phase_inc)));
        power_meter_update(&meter, buf[i]);
    }
    outframes = afWriteFrames(outhandle,
                              AF_DEFAULT_TRACK,
                              buf,
                              SAMPLES_PER_CHUNK);
    if (outframes != SAMPLES_PER_CHUNK)
    {
        fprintf(stderr, "    Error writing wave file\n");
        exit(2);
    }
    printf("Level is %fdBOv/%fdBm0\n", power_meter_current_dbov(&meter), power_meter_current_dbm0(&meter));
    if (fabs(power_meter_current_dbov(&meter) + 3.02f) > 0.05f)
    {
        printf("Test failed.\n");
        exit(2);
    }

    if (afCloseFile(outhandle) != 0)
    {
        fprintf(stderr, "    Cannot close wave file '%s'\n", OUTPUT_FILE_NAME);
        exit(2);
    }
    afFreeFileSetup(filesetup);

    printf("Complex DDS tests,\n");
    if ((filesetup = afNewFileSetup()) == AF_NULL_FILESETUP)
    {
        fprintf(stderr, "    Failed to create file setup\n");
        exit(2);
    }
    afInitSampleFormat(filesetup, AF_DEFAULT_TRACK, AF_SAMPFMT_TWOSCOMP, 16);
    afInitRate(filesetup, AF_DEFAULT_TRACK, (float) SAMPLE_RATE);
    afInitFileFormat(filesetup, AF_FILE_WAVE);
    afInitChannels(filesetup, AF_DEFAULT_TRACK, 2);

    if ((outhandle = afOpenFile(OUTPUT_FILE_NAME_COMPLEX, "w", filesetup)) == AF_NULL_FILEHANDLE)
    {
        fprintf(stderr, "    Cannot create wave file '%s'\n", OUTPUT_FILE_NAME_COMPLEX);
        exit(2);
    }

    power_meter_init(&meter_i, 7);
    power_meter_init(&meter_q, 7);

    phase = 0;
    phase_inc = dds_phase_ratef(123.456789f);
    for (i = 0;  i < SAMPLES_PER_CHUNK;  i++)
    {
        camp = dds_complexf(&phase, phase_inc);
        buf[2*i] = camp.re*10000.0f;
        buf[2*i + 1] = camp.im*10000.0f;
        power_meter_update(&meter_i, buf[2*i]);
        power_meter_update(&meter_q, buf[2*i]);
    }
    outframes = afWriteFrames(outhandle,
                              AF_DEFAULT_TRACK,
                              buf,
                              SAMPLES_PER_CHUNK);
    if (outframes != SAMPLES_PER_CHUNK)
    {
        fprintf(stderr, "    Error writing wave file\n");
        exit(2);
    }
    printf("Level is %fdBOv/%fdBm0, %fdBOv/%fdBm0\n",
           power_meter_current_dbov(&meter_i),
           power_meter_current_dbm0(&meter_i),
           power_meter_current_dbov(&meter_q),
           power_meter_current_dbm0(&meter_q));
    if (fabs(power_meter_current_dbov(&meter_i) + 13.42f) > 0.05f
        ||
        fabs(power_meter_current_dbov(&meter_q) + 13.42f) > 0.05f)
    {
        printf("Test failed.\n");
        exit(2);
    }

    if (afCloseFile(outhandle) != 0)
    {
        fprintf(stderr, "    Cannot close wave file '%s'\n", OUTPUT_FILE_NAME_COMPLEX);
        exit(2);
    }
    afFreeFileSetup(filesetup);

    printf("Tests passed.\n");
    return  0;
}
/*- End of function --------------------------------------------------------*/
/*- End of file ------------------------------------------------------------*/
