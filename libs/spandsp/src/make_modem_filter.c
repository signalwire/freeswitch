/*
 * SpanDSP - a series of DSP components for telephony
 *
 * make_modem_filter.c - Create coefficient sets for pulse shaping
 *                       various modem rx and tx signals.
 *
 * Written by Steve Underwood <steveu@coppice.org>
 *
 * Copyright (C) 2008 Steve Underwood
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

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#include <inttypes.h>
#include <stdlib.h>
#include <unistd.h>
#if defined(HAVE_TGMATH_H)
#include <tgmath.h>
#endif
#if defined(HAVE_MATH_H)
#include <math.h>
#endif
#include "floating_fudge.h"
#include <string.h>
#include <stdio.h>
#include <time.h>
#include <fcntl.h>
#if defined(__sunos)  ||  defined(__solaris)  ||  defined(__sun)
#include <getopt.h>
#endif

#include "spandsp/telephony.h"
#include "spandsp/complex.h"
#include "filter_tools.h"

#if !defined(FALSE)
#define FALSE 0
#endif
#if !defined(TRUE)
#define TRUE (!FALSE)
#endif

//#define SAMPLE_RATE         8000.0
#define MAX_COEFFS_PER_FILTER   128
#define MAX_COEFF_SETS          384

static void make_tx_filter(int coeff_sets,
                           int coeffs_per_filter,
                           double carrier,
                           double baud_rate,
                           double excess_bandwidth,
                           int fixed_point,
                           const char *tag)
{
    int i;
    int j;
    int x;
    int total_coeffs;
    double alpha;
    double beta;
    double gain;
    double scaling;
    double peak;
    double coeffs[MAX_COEFF_SETS*MAX_COEFFS_PER_FILTER + 1];

    total_coeffs = coeff_sets*coeffs_per_filter + 1;
    alpha = baud_rate/(2.0*(double) (coeff_sets*baud_rate));
    beta = excess_bandwidth;

    compute_raised_cosine_filter(coeffs, total_coeffs, TRUE, FALSE, alpha, beta);

    /* Find the DC gain of the filter, and adjust the filter to unity gain. */
    gain = 0.0;
    for (i = coeff_sets/2;  i < total_coeffs;  i += coeff_sets)
        gain += coeffs[i];
    /* Normalise the gain to 1.0 */
    for (i = 0;  i < total_coeffs;  i++)
        coeffs[i] /= gain;
    gain = 1.0;

    if (fixed_point)
    {
        peak = -1.0;
        for (i = 0;  i < total_coeffs;  i++)
        {
            if (fabs(coeffs[i]) > peak)
                peak = fabs(coeffs[i]);
        }
        scaling = 32767.0;
        if (peak >= 1.0)
        {
            scaling /= peak;
            gain = 1.0/peak;
        }
        for (i = 0;  i < total_coeffs;  i++)
            coeffs[i] *= scaling;
    }
    
    /* Churn out the data as a C source code header file, which can be directly included by the
       modem code. */
    printf("#define TX_PULSESHAPER%s_GAIN        %ff\n", tag, gain);
    printf("#define TX_PULSESHAPER%s_COEFF_SETS  %d\n", tag, coeff_sets);
    printf("static const %s tx_pulseshaper%s[TX_PULSESHAPER%s_COEFF_SETS][%d] =\n",
           (fixed_point)  ?  "int16_t"  :  "float",
           tag,
           tag,
           coeffs_per_filter);
    printf("{\n");
    for (j = 0;  j < coeff_sets;  j++)
    {
        x = j;
        printf("    {\n");
        if (fixed_point)
            printf("        %8d,     /* Filter %d */\n", (int) coeffs[x], j);
        else
            printf("        %15.10ff,     /* Filter %d */\n", coeffs[x], j);
        for (i = 1;  i < coeffs_per_filter - 1;  i++)
        {
            x = i*coeff_sets + j;
            if (fixed_point)
                printf("        %8d,\n", (int) coeffs[x]);
            else
                printf("        %15.10ff,\n", coeffs[x]);
        }
        x = i*coeff_sets + j;
        if (fixed_point)
            printf("        %8d\n", (int) coeffs[x]);
        else
            printf("        %15.10ff\n", coeffs[x]);
        if (j < coeff_sets - 1)
            printf("    },\n");
        else
            printf("    }\n");
    }
    printf("};\n");
}
/*- End of function --------------------------------------------------------*/

static void make_rx_filter(int coeff_sets,
                           int coeffs_per_filter,
                           double carrier,
                           double baud_rate,
                           double excess_bandwidth,
                           int fixed_point,
                           const char *tag)
{
    int i;
    int j;
    int k;
    int m;
    int x;
    int total_coeffs;
    double alpha;
    double beta;
    double gain;
    double peak;
    double coeffs[MAX_COEFF_SETS*MAX_COEFFS_PER_FILTER + 1];
#if 0
    complex_t co[MAX_COEFFS_PER_FILTER];
#else
    double cox[MAX_COEFFS_PER_FILTER];
#endif

    total_coeffs = coeff_sets*coeffs_per_filter + 1;
    alpha = baud_rate/(2.0*(double) (coeff_sets*SAMPLE_RATE));
    beta = excess_bandwidth;
    carrier *= 2.0*3.1415926535/SAMPLE_RATE;

    compute_raised_cosine_filter(coeffs, total_coeffs, TRUE, FALSE, alpha, beta);

    /* Find the DC gain of the filter, and adjust the filter to unity gain. */
    gain = 0.0;
    for (i = coeff_sets/2;  i < total_coeffs;  i += coeff_sets)
        gain += coeffs[i];
    /* Normalise the gain to 1.0 */
    for (i = 0;  i < total_coeffs;  i++)
        coeffs[i] /= gain;
    gain = 1.0;

    if (fixed_point)
    {
        peak = -1.0;
        for (i = 0;  i < total_coeffs;  i++)
        {
            if (fabs(coeffs[i]) > peak)
                peak = fabs(coeffs[i]);
        }
        gain = 32767.0;
        if (peak >= 1.0)
            gain /= peak;
        for (i = 0;  i < total_coeffs;  i++)
            coeffs[i] *= gain;
    }

    /* Churn out the data as a C source code header file, which can be directly included by the
       modem code. */
    printf("#define RX_PULSESHAPER%s_GAIN        %ff\n", tag, gain);
    printf("#define RX_PULSESHAPER%s_COEFF_SETS  %d\n", tag, coeff_sets);
#if 0
    printf("static const %s rx_pulseshaper%s[RX_PULSESHAPER%s_COEFF_SETS][%d] =\n",
           (fixed_point)  ?  "complexi16_t"  :  "complexf_t",
           tag,
           tag,
           coeffs_per_filter);
    printf("{\n");
    for (j = 0;  j < coeff_sets;  j++)
    {
        /* Complex modulate the filter, to make it a complex pulse shaping bandpass filter
           centred at the nominal carrier frequency. Use the same phase for all the coefficient
           sets. This means the modem can step the carrier in whole samples, and not worry about
           the fractional sample shift caused by selecting amongst the various coefficient sets. */
        for (i = 0;  i < coeffs_per_filter;  i++)
        {
            m = i - (coeffs_per_filter >> 1);
            x = i*coeff_sets + j;
            co[i].re = coeffs[x]*cos(carrier*m);
            co[i].im = coeffs[x]*sin(carrier*m);
        }
        printf("    {\n");
        if (fixed_point)
            printf("        {%8d, %8d},     /* Filter %d */\n", (int) co[i].re, (int) co[i].im, j);
        else
            printf("        {%15.10ff, %15.10ff},     /* Filter %d */\n", co[0].re, co[0].im, j);
        for (i = 1;  i < coeffs_per_filter - 1;  i++)
        {
            if (fixed_point)
                printf("        {%8d, %8d},\n", (int) co[i].re, (int) co[i].im);
            else
                printf("        {%15.10ff, %15.10ff},\n", co[i].re, co[i].im);
        }
        if (fixed_point)
            printf("        {%8d, %8d}\n", (int) co[i].re, (int) co[i].im);
        else
            printf("        {%15.10ff, %15.10ff}\n", co[i].re, co[i].im);
        if (j < coeff_sets - 1)
            printf("    },\n");
        else
            printf("    }\n");
    }
    printf("};\n");
#else
    for (k = 0;  k < 2;  k++)
    {
        printf("static const %s rx_pulseshaper%s_%s[RX_PULSESHAPER%s_COEFF_SETS][%d] =\n",
               (fixed_point)  ?  "int16_t"  :  "float",
               tag,
               (k == 0)  ?  "re"  :  "im",
               tag,
               coeffs_per_filter);
        printf("{\n");
        for (j = 0;  j < coeff_sets;  j++)
        {
            /* Complex modulate the filter, to make it a complex pulse shaping bandpass filter
               centred at the nominal carrier frequency. Use the same phase for all the coefficient
               sets. This means the modem can step the carrier in whole samples, and not worry about
               the fractional sample shift caused by selecting amongst the various coefficient sets. */
            for (i = 0;  i < coeffs_per_filter;  i++)
            {
                m = i - (coeffs_per_filter >> 1);
                x = i*coeff_sets + j;
                if (k == 0)
                    cox[i] = coeffs[x]*cos(carrier*m);
                else
                    cox[i] = coeffs[x]*sin(carrier*m);
            }
            printf("    {\n");
            if (fixed_point)
                printf("        %8d,     /* Filter %d */\n", (int) cox[0], j);
            else
                printf("        %15.10ff,     /* Filter %d */\n", cox[0], j);
            for (i = 1;  i < coeffs_per_filter - 1;  i++)
            {
                if (fixed_point)
                    printf("        %8d,\n", (int) cox[i]);
                else
                    printf("        %15.10ff,\n", cox[i]);
            }
            if (fixed_point)
                printf("        %8d\n", (int) cox[i]);
            else
                printf("        %15.10ff\n", cox[i]);
            if (j < coeff_sets - 1)
                printf("    },\n");
            else
                printf("    }\n");
        }
        printf("};\n");
    }
#endif
}
/*- End of function --------------------------------------------------------*/

static void usage(void)
{
    fprintf(stderr, "Usage: make_modem_rx_filter -m <V.17 | V.22bis | V.22bis1200 | V.22bis2400 | V.27ter2400 | V.27ter4800 | V.29> [-i] [-r] [-t]\n");
}
/*- End of function --------------------------------------------------------*/

int main(int argc, char **argv)
{
    int rx_coeff_sets;
    int rx_coeffs_per_filter;
    int tx_coeff_sets;
    int tx_coeffs_per_filter;
    int opt;
    int transmit_modem;
    int fixed_point;
    double carrier;
    double baud_rate;
    double rx_excess_bandwidth;
    double tx_excess_bandwidth;
    const char *rx_tag;
    const char *tx_tag;
    const char *modem;

    fixed_point = FALSE;
    transmit_modem = FALSE;
    modem = "";
    while ((opt = getopt(argc, argv, "im:rt")) != -1)
    {
        switch (opt)
        {
        case 'i':
            fixed_point = TRUE;
            break;
        case 'm':
            modem = optarg;
            break;
        case 'r':
            transmit_modem = FALSE;
            break;
        case 't':
            transmit_modem = TRUE;
            break;
        default:
            usage();
            exit(2);
            break;
        }
    }
    if (strcmp(modem, "V.17") == 0  ||  strcmp(modem, "V.32bis") == 0)
    {
        /* This applies to V.32bis as well as V.17 */
        rx_coeff_sets = 192;
        rx_coeffs_per_filter = 27;
        rx_excess_bandwidth = 0.5;
        tx_coeff_sets = 10;
        tx_coeffs_per_filter = 9;
        tx_excess_bandwidth = 0.25;
        carrier = 1800.0;
        baud_rate = 2400.0;
        rx_tag = "";
        tx_tag = "";
    }
    else if (strcmp(modem, "V.22bis") == 0)
    {
        /* This is only intended to apply to transmit. */
        rx_coeff_sets = 12;
        rx_coeffs_per_filter = 27;
        rx_excess_bandwidth = 0.75;
        tx_coeff_sets = 40;
        tx_coeffs_per_filter = 9;
        tx_excess_bandwidth = 0.75;
        carrier = 1200.0;
        baud_rate = 600.0;
        rx_tag = "";
        tx_tag = "";
    }
    else if (strcmp(modem, "V.22bis1200") == 0)
    {
        /* This is only intended to apply to receive. */
        rx_coeff_sets = 12;
        rx_coeffs_per_filter = 27;
        rx_excess_bandwidth = 0.75;
        tx_coeff_sets = 40;
        tx_coeffs_per_filter = 9;
        tx_excess_bandwidth = 0.75;
        carrier = 1200.0;
        baud_rate = 600.0;
        rx_tag = "_1200";
        tx_tag = "_1200";
    }
    else if (strcmp(modem, "V.22bis2400") == 0)
    {
        /* This is only intended to apply to receive. */
        rx_coeff_sets = 12;
        rx_coeffs_per_filter = 27;
        rx_excess_bandwidth = 0.75;
        tx_coeff_sets = 40;
        tx_coeffs_per_filter = 9;
        tx_excess_bandwidth = 0.75;
        carrier = 2400.0;
        baud_rate = 600.0;
        rx_tag = "_2400";
        tx_tag = "_2400";
    }
    else if (strcmp(modem, "V.27ter2400") == 0)
    {
        rx_coeff_sets = 12;
        rx_coeffs_per_filter = 27;
        rx_excess_bandwidth = 0.5;
        tx_coeff_sets = 20;
        tx_coeffs_per_filter = 9;
        tx_excess_bandwidth = 0.5;
        carrier = 1800.0;
        baud_rate = 1200.0;
        rx_tag = "_2400";
        tx_tag = "_2400";
    }
    else if (strcmp(modem, "V.27ter4800") == 0)
    {
        rx_coeff_sets = 8;
        rx_coeffs_per_filter = 27;
        rx_excess_bandwidth = 0.5;
        tx_coeff_sets = 5;
        tx_coeffs_per_filter = 9;
        tx_excess_bandwidth = 0.5;
        carrier = 1800.0;
        baud_rate = 1600.0;
        rx_tag = "_4800";
        tx_tag = "_4800";
    }
    else if (strcmp(modem, "V.29") == 0)
    {
        rx_coeff_sets = 48;
        rx_coeffs_per_filter = 27;
        rx_excess_bandwidth = 0.5;
        tx_coeff_sets = 10;
        tx_coeffs_per_filter = 9;
        tx_excess_bandwidth = 0.25;
        carrier = 1700.0;
        baud_rate = 2400.0;
        rx_tag = "";
        tx_tag = "";
    }
    else
    {
        usage();
        exit(2);
    }
    if (transmit_modem)
    {
        make_tx_filter(tx_coeff_sets,
                       tx_coeffs_per_filter,
                       carrier,
                       baud_rate,
                       tx_excess_bandwidth,
                       fixed_point,
                       tx_tag);
    }
    else
    {
        make_rx_filter(rx_coeff_sets,
                       rx_coeffs_per_filter,
                       carrier,
                       baud_rate,
                       rx_excess_bandwidth,
                       fixed_point,
                       rx_tag);
    }
    return 0;
}
/*- End of function --------------------------------------------------------*/
/*- End of file ------------------------------------------------------------*/
