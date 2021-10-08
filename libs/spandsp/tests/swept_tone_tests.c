/*
 * SpanDSP - a series of DSP components for telephony
 *
 * swept_tone_tests.c
 *
 * Written by Steve Underwood <steveu@coppice.org>
 *
 * Copyright (C) 2009 Steve Underwood
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

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <memory.h>
#include <sndfile.h>

#include "spandsp.h"
#include "spandsp-sim.h"

#define OUTPUT_FILE_NAME        "swept_tone.wav"

#define BLOCK_LEN               160

int main(int argc, char *argv[])
{
    int i;
    int j;
    int outframes;
    int len;
    SNDFILE *outhandle;
    power_meter_t meter;
    swept_tone_state_t *s;
    int16_t buf[BLOCK_LEN];

    power_meter_init(&meter, 10);

    if ((outhandle = sf_open_telephony_write(OUTPUT_FILE_NAME, 1)) == NULL)
    {
        fprintf(stderr, "    Cannot create audio file '%s'\n", OUTPUT_FILE_NAME);
        exit(2);
    }

    printf("Test with swept tone.\n");
    s = swept_tone_init(NULL, 200.0f, 3900.0f, -10.0f, 60*SAMPLE_RATE, true);
    for (j = 0;  j < 60*SAMPLE_RATE;  j += BLOCK_LEN)
    {
        len = swept_tone(s, buf, BLOCK_LEN);
        for (i = 0;  i < len;  i++)
            power_meter_update(&meter, buf[i]);
        outframes = sf_writef_short(outhandle, buf, len);
        if (outframes != len)
        {
            fprintf(stderr, "    Error writing audio file\n");
            exit(2);
        }
#if 0
        printf("Current freq %.1fHz, Level is %fdBOv/%fdBm0\n",
               swept_tone_current_frequency(s),
               power_meter_current_dbov(&meter),
               power_meter_current_dbm0(&meter));
#else
        printf("%.1f %f\n",
               swept_tone_current_frequency(s),
               power_meter_current_dbm0(&meter));
#endif
    }

    if (sf_close_telephony(outhandle))
    {
        fprintf(stderr, "    Cannot close audio file '%s'\n", OUTPUT_FILE_NAME);
        exit(2);
    }

    swept_tone_free(s);

    power_meter_release(&meter);

    printf("Tests passed.\n");
    return 0;
}
/*- End of function --------------------------------------------------------*/
/*- End of file ------------------------------------------------------------*/
