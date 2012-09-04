/*
 * SpanDSP - a series of DSP components for telephony
 *
 * awgn_tests.c
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

/*! \page awgn_tests_page AWGN tests
\section awgn_tests_page_sec_1 What does it do?
*/

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

//#if defined(WITH_SPANDSP_INTERNALS)
#define SPANDSP_EXPOSE_INTERNAL_STRUCTURES
//#endif

#include "spandsp.h"

#if !defined(M_PI)
# define M_PI           3.14159265358979323846  /* pi */
#endif

#define OUT_FILE_NAME   "awgn.wav"

/* Some simple sanity tests for the Gaussian noise generation routines */

int main(int argc, char *argv[])
{
    int i;
    int j;
    int clip_high;
    int clip_low;
    int total_samples;
    int idum = 1234567;
    int16_t value;
    double total;
    double x;
    double p;
    double o;
    double error;
    int bins[65536];
    awgn_state_t noise_source;

    /* Generate noise at several RMS levels between -50dBm and 0dBm. Noise is
       generated for a large number of samples (1,000,000), and the RMS value
       of the noise is calculated along the way. If the resulting level is
       close to the requested RMS level, at least the scaling of the noise
       should be Ok. At high level some clipping may distort the result a
       little. */
    for (j = -50;  j <= 0;  j += 5)
    {
        clip_high = 0;
        clip_low = 0;
        total = 0.0;
        awgn_init_dbm0(&noise_source, idum, (float) j);
        total_samples = 1000000;
        for (i = 0;  i < total_samples;  i++)
        {
            value = awgn(&noise_source);
            if (value == 32767)
                clip_high++;
            else if (value == -32768)
                clip_low++;
            total += ((double) value)*((double) value);
        }
        error = 100.0*(1.0 - sqrt(total/total_samples)/noise_source.rms);
        printf("RMS = %.3f (expected %d) %.2f%% error [clipped samples %d+%d]\n",
               10.0*log10((total/total_samples)/(32768.0*32768.0) + 1.0e-10) + DBM0_MAX_POWER,
               j,
               error,
               clip_low,
               clip_high);
        /* We don't check the result at 0dBm0, as there will definitely be a lot of error due to clipping */
        if (j < 0  &&  fabs(error) > 0.2)
        {
            printf("Test failed.\n");
            exit(2);
        }
    }
    /* Now look at the statistical spread of the results, by collecting data in
       bins from a large number of samples. Use a fairly high noise level, but
       low enough to avoid significant clipping. Use the Gaussian model to
       predict the real probability, and present the results for graphing. */
    memset(bins, 0, sizeof(bins));
    clip_high = 0;
    clip_low = 0;
    awgn_init_dbm0(&noise_source, idum, -15);
    total_samples = 10000000;
    for (i = 0;  i < total_samples;  i++)
    {
        value = awgn(&noise_source);
        if (value == 32767)
            clip_high++;
        else if (value == -32768)
            clip_low++;
        bins[value + 32768]++;
    }
    o = noise_source.rms;
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
        printf("%6d %.7f %.7f\n", i - 32768, x, p);
    }
    
    printf("Tests passed.\n");
    return 0;
}
/*- End of function --------------------------------------------------------*/
/*- End of file ------------------------------------------------------------*/
