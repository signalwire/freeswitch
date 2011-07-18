/*
 * SpanDSP - a series of DSP components for telephony
 *
 * noise_tests.c
 *
 * Written by Steve Underwood <steveu@coppice.org>
 *
 * Copyright (C) 2005 Steve Underwood
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

/*! \page noise_tests_page Noise generator tests
\section noise_tests_page_sec_1 What does it do?
*/

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sndfile.h>

//#if defined(WITH_SPANDSP_INTERNALS)
#define SPANDSP_EXPOSE_INTERNAL_STRUCTURES
//#endif

#include "spandsp.h"
#include "spandsp-sim.h"

#if !defined(M_PI)
# define M_PI           3.14159265358979323846  /* pi */
#endif

#define OUT_FILE_NAME   "noise.wav"

/* Some simple sanity tests for the noise generation routines */

int main (int argc, char *argv[])
{
    int i;
    int j;
    int level;
    int clip_high;
    int clip_low;
    int quality;
    int total_samples;
    int seed = 1234567;
    int outframes;
    int16_t value;
    double total;
    double x;
    double p;
    double o;
    int bins[65536];
    int16_t amp[1024];
    noise_state_t noise_source;
    SNDFILE *outhandle;

    if ((outhandle = sf_open_telephony_write(OUT_FILE_NAME, 1)) == NULL)
    {
        fprintf(stderr, "    Cannot create audio file '%s'\n", OUT_FILE_NAME);
        exit(2);
    }

    for (quality = 7;  quality <= 20;  quality += (20 - 7))
    {
        /* Generate AWGN at several RMS levels between -50dBOv and 0dBOv. Noise is
           generated for a large number of samples (1,000,000), and the RMS value
           of the noise is calculated along the way. If the resulting level is
           close to the requested RMS level, at least the scaling of the noise
           should be Ok. At high levels some clipping may distort the result a
           little. */
        printf("Testing AWGN power, with quality %d\n", quality);
        for (level = -50;  level <= 0;  level += 5)
        {
            clip_high = 0;
            clip_low = 0;
            total = 0.0;
            noise_init_dbov(&noise_source, seed, (float) level, NOISE_CLASS_AWGN, quality);
            total_samples = 1000000;
            for (i = 0;  i < total_samples;  i++)
            {
                value = noise(&noise_source);
                if (value == 32767)
                    clip_high++;
                else if (value == -32768)
                    clip_low++;
                total += ((double) value)*((double) value);
            }
            printf ("RMS = %.3f (expected %d) %.2f%% error [clipped samples %d+%d]\n",
                    10.0*log10((total/total_samples)/(32768.0*32768.0) + 1.0e-10),
                    level,
                    100.0*(1.0 - sqrt(total/total_samples)/(pow(10.0, level/20.0)*32768.0)),
                    clip_low,
                    clip_high);
            if (level < -5  &&  fabs(10.0*log10((total/total_samples)/(32768.0*32768.0) + 1.0e-10) - level) > 0.2)
            {
                printf("Test failed\n");
                exit(2);
            }
        }
    }

    /* Now look at the statistical spread of the results, by collecting data in
       bins from a large number of samples. Use a fairly high noise level, but
       low enough to avoid significant clipping. Use the Gaussian model to
       predict the real probability, and present the results for graphing. */
    quality = 7;
    printf("Testing the statistical spread of AWGN, with quality %d\n", quality);
    memset(bins, 0, sizeof(bins));
    clip_high = 0;
    clip_low = 0;
    level = -15;
    noise_init_dbov(&noise_source, seed, (float) level, NOISE_CLASS_AWGN, quality);
    total_samples = 10000000;
    for (i = 0;  i < total_samples;  i++)
    {
        value = noise(&noise_source);
        if (value == 32767)
            clip_high++;
        else if (value == -32768)
            clip_low++;
        bins[value + 32768]++;
    }
    /* Find the RMS power level to expect */
    o = pow(10.0, level/20.0)*(32768.0*0.70711);
    for (i = 0;  i < 65536 - 10;  i++)
    {
        x = i - 32768;
        /* Find the real probability for this bin */
        p = (1.0/(o*sqrt(2.0*M_PI)))*exp(-(x*x)/(2.0*o*o));
        /* Now do a little smoothing on the real data to get a reasonably
           steady answer */
        x = 0;
        for (j = 0;  j < 10;  j++)
            x += bins[i + j];
        x /= 10.0;
        x /= total_samples;
        /* Now send it out for graphing. */
        if (p > 0.0000001)
            printf("%6d %.7f %.7f\n", i - 32768, x, p);
    }

    printf("Generating AWGN at -15dBOv to file\n");
    for (j = 0;  j < 50;  j++)
    {
        for (i = 0;  i < 1024;  i++)
            amp[i] = noise(&noise_source);
        outframes = sf_writef_short(outhandle, amp, 1024);
        if (outframes != 1024)
        {
            fprintf(stderr, "    Error writing audio file\n");
            exit(2);
        }
    }

    /* Generate Hoth noise at several RMS levels between -50dBm and 0dBm. Noise
       is generated for a large number of samples (1,000,000), and the RMS value
       of the noise is calculated along the way. If the resulting level is
       close to the requested RMS level, at least the scaling of the noise
       should be Ok. At high levels some clipping may distort the result a
       little. */
    quality = 7;
    printf("Testing Hoth noise power, with quality %d\n", quality);
    for (level = -50;  level <= 0;  level += 5)
    {
        clip_high = 0;
        clip_low = 0;
        total = 0.0;
        noise_init_dbov(&noise_source, seed, (float) level, NOISE_CLASS_HOTH, quality);
        total_samples = 1000000;
        for (i = 0;  i < total_samples;  i++)
        {
            value = noise(&noise_source);
            if (value == 32767)
                clip_high++;
            else if (value == -32768)
                clip_low++;
            total += ((double) value)*((double) value);
        }
        printf ("RMS = %.3f (expected %d) %.2f%% error [clipped samples %d+%d]\n",
                10.0*log10((total/total_samples)/(32768.0*32768.0) + 1.0e-10),
                level,
                100.0*(1.0 - sqrt(total/total_samples)/(pow(10.0, level/20.0)*32768.0)),
                clip_low,
                clip_high);
        if (level < -5  &&  fabs(10.0*log10((total/total_samples)/(32768.0*32768.0) + 1.0e-10) - level) > 0.2)
        {
            printf("Test failed\n");
            exit(2);
        }
    }
    
    quality = 7;
    printf("Generating Hoth noise at -15dBOv to file\n");
    level = -15;
    noise_init_dbov(&noise_source, seed, (float) level, NOISE_CLASS_HOTH, quality);
    for (j = 0;  j < 50;  j++)
    {
        for (i = 0;  i < 1024;  i++)
            amp[i] = noise(&noise_source);
        outframes = sf_writef_short(outhandle, amp, 1024);
        if (outframes != 1024)
        {
            fprintf(stderr, "    Error writing audio file\n");
            exit(2);
        }
    }

    if (sf_close_telephony(outhandle))
    {
        fprintf(stderr, "    Cannot close audio file '%s'\n", OUT_FILE_NAME);
        exit(2);
    }
    
    printf("Tests passed.\n");
    return  0;
}
/*- End of function --------------------------------------------------------*/
/*- End of file ------------------------------------------------------------*/
