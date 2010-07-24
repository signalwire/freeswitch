/*
 * SpanDSP - a series of DSP components for telephony
 *
 * tone_detect_tests.c
 *
 * Written by Steve Underwood <steveu@coppice.org>
 *
 * Copyright (C) 2007 Steve Underwood
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

/*! \page tone_detect_tests_page Tone detection tests
\section tone_detect_tests_page_sec_1 What does it do?
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

//#if defined(WITH_SPANDSP_INTERNALS)
#define SPANDSP_EXPOSE_INTERNAL_STRUCTURES
//#endif

#include "spandsp.h"

#define DEC_SAMPLE_RATE     800
#define DEC_RATIO           10
#define BLOCK_LEN           56
#define PG_WINDOW           56
#define FREQ1               440.0f
#define FREQ2               480.0f

static int periodogram_tests(void)
{
    int i;
    int j;
    int k;
    int len;
    complexf_t coeffs[PG_WINDOW/2];
    complexf_t camp[BLOCK_LEN];
    complexf_t last_result;
    complexf_t result;
    complexf_t phase_offset;
    float freq_error;
    float pg_scale;
    float level;
    float scale1;
    float scale2;
    int32_t phase_rate1;
    int32_t phase_rate2;
    uint32_t phase_acc1;
    uint32_t phase_acc2;
    awgn_state_t noise_source_re;
    awgn_state_t noise_source_im;

    phase_rate1 = DEC_RATIO*dds_phase_ratef(FREQ1 - 5.0f);
    phase_rate2 = DEC_RATIO*dds_phase_ratef(FREQ2);
    phase_acc1 = 0;
    phase_acc2 = 0;
    len = periodogram_generate_coeffs(coeffs, FREQ1, DEC_SAMPLE_RATE, PG_WINDOW);
    if (len != PG_WINDOW/2)
    {
        printf("Test failed\n");
        return -1;
    }
    pg_scale = periodogram_generate_phase_offset(&phase_offset, FREQ1, DEC_SAMPLE_RATE, PG_WINDOW);
    scale1 = dds_scaling_dbm0f(-6.0f);
    scale2 = dds_scaling_dbm0f(-6.0f);

    for (k = -50;  k < 0;  k++)
    {
        printf("Setting noise to %ddBm0\n", k);
        awgn_init_dbm0(&noise_source_re, 1234567, (float) k);
        awgn_init_dbm0(&noise_source_im, 7654321, (float) k);
        last_result = complex_setf(0.0f, 0.0f);
        for (i = 0;  i < 100;  i++)
        {
            for (j = 0;  j < PG_WINDOW;  j++)
            {
                result = dds_complexf(&phase_acc1, phase_rate1);
                camp[j].re = result.re*scale1;
                camp[j].im = result.im*scale1;
                result = dds_complexf(&phase_acc2, phase_rate2);
                camp[j].re += result.re*scale2;
                camp[j].im += result.im*scale2;
                camp[j].re += awgn(&noise_source_re);
                camp[j].im += awgn(&noise_source_im);
            }
            result = periodogram(coeffs, camp, PG_WINDOW);
            level = sqrtf(result.re*result.re + result.im*result.im);
            freq_error = periodogram_freq_error(&phase_offset, pg_scale, &last_result, &result);
            last_result = result;
            if (i == 0)
                continue;

            printf("Signal level = %.5f, freq error = %.5f\n", level, freq_error);
            if (level < scale1*0.8f  ||  level > scale1*1.2f)
            {
                printf("Test failed - %ddBm0 of noise, signal is %f (%f)\n", k, level, scale1);
                return -1;
            }
            if (freq_error < -10.0f  ||  freq_error > 10.0f)
            {
                printf("Test failed - %ddBm0 of noise, %fHz error\n", k, freq_error);
                return -1;
            }
        }
    }
    return  0;
}
/*- End of function --------------------------------------------------------*/

int main(int argc, char *argv[])
{
    if (periodogram_tests())
        exit(2);
    printf("Tests passed\n");
    return  0;
}
/*- End of function --------------------------------------------------------*/
/*- End of file ------------------------------------------------------------*/
