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

#include <inttypes.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <time.h>
#include <fcntl.h>
#include <math.h>
#if defined(HAVE_STDBOOL_H)
#include <stdbool.h>
#else
#include "spandsp/stdbool.h"
#endif
#if defined(__sunos)  ||  defined(__solaris)  ||  defined(__sun)
#include <getopt.h>
#endif

#if defined (_MSC_VER)
 #define __inline__ __inline
#endif

#include "spandsp/telephony.h"
#include "spandsp/complex.h"
#include "filter_tools.h"

//#define SAMPLE_RATE         8000.0
#define MAX_COEFFS_PER_FILTER   128
#define MAX_COEFF_SETS          384

static void make_tx_filter(int coeff_sets,
                           int coeffs_per_filter,
                           double carrier,
                           double baud_rate,
                           double excess_bandwidth,
                           const char *tag)
{
    int i;
    int j;
    int x;
    int total_coeffs;
    double alpha;
    double beta;
    double floating_gain;
    double fixed_gain;
    double fixed_scaling;
    double peak;
    double coeffs[MAX_COEFF_SETS*MAX_COEFFS_PER_FILTER + 1];

    total_coeffs = coeff_sets*coeffs_per_filter + 1;
    alpha = baud_rate/(2.0*(double) (coeff_sets*baud_rate));
    beta = excess_bandwidth;

    compute_raised_cosine_filter(coeffs, total_coeffs, true, false, alpha, beta);

    /* Find the DC gain of the filter, and adjust the filter to unity gain. */
    floating_gain = 0.0;
    for (i = coeff_sets/2;  i < total_coeffs;  i += coeff_sets)
        floating_gain += coeffs[i];
    /* Normalise the gain to 1.0 */
    for (i = 0;  i < total_coeffs;  i++)
        coeffs[i] /= floating_gain;
    floating_gain = 1.0;
    fixed_gain = 1.0;

    peak = -1.0;
    for (i = 0;  i < total_coeffs;  i++)
    {
        if (fabs(coeffs[i]) > peak)
            peak = fabs(coeffs[i]);
    }
    fixed_scaling = 32767.0f;
    if (peak >= 1.0)
    {
        fixed_scaling /= peak;
        fixed_gain = 1.0/peak;
    }

    /* Churn out the data as a C source code header file, which can be directly included by the
       modem code. */
    printf("/* THIS FILE WAS AUTOMATICALLY GENERATED - ANY MODIFICATIONS MADE TO THIS");
    printf("   FILE MAY BE OVERWRITTEN DURING FUTURE BUILDS OF THE SOFTWARE */\n");
    printf("\n");
    printf("#if defined(SPANDSP_USE_FIXED_POINT)\n");
    printf("#define TX_PULSESHAPER%s_SCALE(x)    ((int16_t) (%f*x + ((x >= 0.0)  ?  0.5  :  -0.5)))\n", tag, fixed_scaling);
    printf("#define TX_PULSESHAPER%s_GAIN        %ff\n", tag, fixed_gain);
    printf("#else\n");
    printf("#define TX_PULSESHAPER%s_SCALE(x)    (x)\n", tag);
    printf("#define TX_PULSESHAPER%s_GAIN        %ff\n", tag, floating_gain);
    printf("#endif\n");
    printf("#define TX_PULSESHAPER%s_COEFF_SETS  %d\n", tag, coeff_sets);
    printf("\n");
    printf("#if defined(SPANDSP_USE_FIXED_POINT)\n");
    printf("static const int16_t tx_pulseshaper%s[TX_PULSESHAPER%s_COEFF_SETS][%d] =\n",
           tag,
           tag,
           coeffs_per_filter);
    printf("#else\n");
    printf("static const float tx_pulseshaper%s[TX_PULSESHAPER%s_COEFF_SETS][%d] =\n",
           tag,
           tag,
           coeffs_per_filter);
    printf("#endif\n");
    printf("{\n");
    for (j = 0;  j < coeff_sets;  j++)
    {
        x = j;
        printf("    {\n");
        printf("        TX_PULSESHAPER%s_SCALE(%15.10ff),     /* Filter %d */\n", tag, coeffs[x], j);
        for (i = 1;  i < coeffs_per_filter - 1;  i++)
        {
            x = i*coeff_sets + j;
            printf("        TX_PULSESHAPER%s_SCALE(%15.10ff),\n", tag, coeffs[x]);
        }
        x = i*coeff_sets + j;
        printf("        TX_PULSESHAPER%s_SCALE(%15.10ff)\n", tag, coeffs[x]);
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
    double floating_gain;
    double fixed_gain;
    double fixed_scaling;
    double peak;
    double coeffs[MAX_COEFF_SETS*MAX_COEFFS_PER_FILTER + 1];
    double cox[MAX_COEFFS_PER_FILTER];

    total_coeffs = coeff_sets*coeffs_per_filter + 1;
    alpha = baud_rate/(2.0*(double) (coeff_sets*SAMPLE_RATE));
    beta = excess_bandwidth;
    carrier *= 2.0*3.1415926535/SAMPLE_RATE;

    compute_raised_cosine_filter(coeffs, total_coeffs, true, false, alpha, beta);

    /* Find the DC gain of the filter, and adjust the filter to unity gain. */
    floating_gain = 0.0;
    for (i = coeff_sets/2;  i < total_coeffs;  i += coeff_sets)
        floating_gain += coeffs[i];
    /* Normalise the gain to 1.0 */
    for (i = 0;  i < total_coeffs;  i++)
        coeffs[i] /= floating_gain;
    floating_gain = 1.0;
    fixed_gain = 1.0;

    peak = -1.0;
    for (i = 0;  i < total_coeffs;  i++)
    {
        if (fabs(coeffs[i]) > peak)
            peak = fabs(coeffs[i]);
    }
    fixed_scaling = 32767.0f;
    if (peak >= 1.0)
    {
        fixed_scaling /= peak;
        fixed_gain = 1.0/peak;
    }

    /* Churn out the data as a C source code header file, which can be directly included by the
       modem code. */
    printf("#if defined(SPANDSP_USE_FIXED_POINT)\n");
    printf("#define RX_PULSESHAPER%s_SCALE(x)    ((int16_t) (%f*x + ((x >= 0.0)  ?  0.5  :  -0.5)))\n", tag, fixed_scaling);
    printf("#define RX_PULSESHAPER%s_GAIN        %ff\n", tag, fixed_gain);
    printf("#else\n");
    printf("#define RX_PULSESHAPER%s_SCALE(x)    (x)\n", tag);
    printf("#define RX_PULSESHAPER%s_GAIN        %ff\n", tag, floating_gain);
    printf("#endif\n");
    printf("#define RX_PULSESHAPER%s_COEFF_SETS  %d\n", tag, coeff_sets);
    for (k = 0;  k < 2;  k++)
    {
        printf("\n");
        printf("#if defined(SPANDSP_USE_FIXED_POINT)\n");
        printf("static const int16_t rx_pulseshaper%s_%s[RX_PULSESHAPER%s_COEFF_SETS][%d] =\n",
               tag,
               (k == 0)  ?  "re"  :  "im",
               tag,
               coeffs_per_filter);
        printf("#else\n");
        printf("static const float rx_pulseshaper%s_%s[RX_PULSESHAPER%s_COEFF_SETS][%d] =\n",
               tag,
               (k == 0)  ?  "re"  :  "im",
               tag,
               coeffs_per_filter);
        printf("#endif\n");
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
            printf("        RX_PULSESHAPER%s_SCALE(%15.10ff),     /* Filter %d */\n", tag, cox[0], j);
            for (i = 1;  i < coeffs_per_filter - 1;  i++)
                    printf("        RX_PULSESHAPER%s_SCALE(%15.10ff),\n", tag, cox[i]);
            printf("        RX_PULSESHAPER%s_SCALE(%15.10ff)\n", tag, cox[i]);
            if (j < coeff_sets - 1)
                printf("    },\n");
            else
                printf("    }\n");
        }
        printf("};\n");
    }
}
/*- End of function --------------------------------------------------------*/

static void usage(void)
{
    fprintf(stderr, "Usage: make_modem_rx_filter -m <V.17 | V.22bis | V.22bis1200 | V.22bis2400 | V.27ter2400 | V.27ter4800 | V.29> [-r] [-t]\n");
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
    double carrier;
    double baud_rate;
    double rx_excess_bandwidth;
    double tx_excess_bandwidth;
    const char *rx_tag;
    const char *tx_tag;
    const char *modem;

    transmit_modem = false;
    modem = "";
    while ((opt = getopt(argc, argv, "m:rt")) != -1)
    {
        switch (opt)
        {
        case 'm':
            modem = optarg;
            break;
        case 'r':
            transmit_modem = false;
            break;
        case 't':
            transmit_modem = true;
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
    else if (strcmp(modem, "V.34_2400") == 0)
    {
        rx_coeff_sets = 192;
        rx_coeffs_per_filter = 27;
        rx_excess_bandwidth = 0.25;
        tx_coeff_sets = 10;
        tx_coeffs_per_filter = 9;
        tx_excess_bandwidth = 0.12;
        carrier = 1600.0;
        baud_rate = 2400.0;
        rx_tag = "_2400_low_carrier";
        tx_tag = "_2400";
    }
    else if (strcmp(modem, "V.34_2400_high") == 0)
    {
        rx_coeff_sets = 192;
        rx_coeffs_per_filter = 27;
        rx_excess_bandwidth = 0.25;
        tx_coeff_sets = 10;
        tx_coeffs_per_filter = 9;
        tx_excess_bandwidth = 0.12;
        carrier = 1800.0;
        baud_rate = 2400.0;
        rx_tag = "_2400_high_carrier";
        tx_tag = "_2400";
    }
    else if (strcmp(modem, "V.34_2743") == 0)
    {
        rx_coeff_sets = 192;
        rx_coeffs_per_filter = 27;
        rx_excess_bandwidth = 0.25;
        tx_coeff_sets = 35;
        tx_coeffs_per_filter = 9;
        tx_excess_bandwidth = 0.12;
        carrier = 1646.0;
        baud_rate = 2400.0*8.0/7.0;
        rx_tag = "_2743_low_carrier";
        tx_tag = "_2743";
    }
    else if (strcmp(modem, "V.34_2743_high") == 0)
    {
        rx_coeff_sets = 192;
        rx_coeffs_per_filter = 27;
        rx_excess_bandwidth = 0.25;
        tx_coeff_sets = 35;
        tx_coeffs_per_filter = 9;
        tx_excess_bandwidth = 0.12;
        carrier = 1829.0;
        baud_rate = 2400.0*8.0/7.0;
        rx_tag = "_2743_high_carrier";
        tx_tag = "_2743";
    }
    else if (strcmp(modem, "V.34_2800") == 0)
    {
        rx_coeff_sets = 192;
        rx_coeffs_per_filter = 27;
        rx_excess_bandwidth = 0.25;
        tx_coeff_sets = 20;
        tx_coeffs_per_filter = 9;
        tx_excess_bandwidth = 0.12;
        carrier = 1680.0;
        baud_rate = 2400.0*7.0/6.0;
        rx_tag = "_2800_low_carrier";
        tx_tag = "_2800";
    }
    else if (strcmp(modem, "V.34_2800_high") == 0)
    {
        rx_coeff_sets = 192;
        rx_coeffs_per_filter = 27;
        rx_excess_bandwidth = 0.25;
        tx_coeff_sets = 20;
        tx_coeffs_per_filter = 9;
        tx_excess_bandwidth = 0.12;
        carrier = 1867.0;
        baud_rate = 2400.0*7.0/6.0;
        rx_tag = "_2800_high_carrier";
        tx_tag = "_2800";
    }
    else if (strcmp(modem, "V.34_3000") == 0)
    {
        rx_coeff_sets = 192;
        rx_coeffs_per_filter = 27;
        rx_excess_bandwidth = 0.25;
        tx_coeff_sets = 8;
        tx_coeffs_per_filter = 9;
        tx_excess_bandwidth = 0.12;
        carrier = 1800.0;
        baud_rate = 2400.0*5.0/4.0;
        rx_tag = "_3000_low_carrier";
        tx_tag = "_3000";
    }
    else if (strcmp(modem, "V.34_3000_high") == 0)
    {
        rx_coeff_sets = 192;
        rx_coeffs_per_filter = 27;
        rx_excess_bandwidth = 0.25;
        tx_coeff_sets = 8;
        tx_coeffs_per_filter = 9;
        tx_excess_bandwidth = 0.12;
        carrier = 2000.0;
        baud_rate = 2400.0*5.0/4.0;
        rx_tag = "_3000_high_carrier";
        tx_tag = "_3000";
    }
    else if (strcmp(modem, "V.34_3200") == 0)
    {
        rx_coeff_sets = 192;
        rx_coeffs_per_filter = 27;
        rx_excess_bandwidth = 0.25;
        tx_coeff_sets = 5;
        tx_coeffs_per_filter = 9;
        tx_excess_bandwidth = 0.12;
        carrier = 1829.0;
        baud_rate = 2400.0*4.0/3.0;
        rx_tag = "_3200_low_carrier";
        tx_tag = "_3200";
    }
    else if (strcmp(modem, "V.34_3200_high") == 0)
    {
        rx_coeff_sets = 192;
        rx_coeffs_per_filter = 27;
        rx_excess_bandwidth = 0.25;
        tx_coeff_sets = 5;
        tx_coeffs_per_filter = 9;
        tx_excess_bandwidth = 0.12;
        carrier = 1920.0;
        baud_rate = 2400.0*4.0/3.0;
        rx_tag = "_3200_high_carrier";
        tx_tag = "_3200";
    }
    else if (strcmp(modem, "V.34_3429") == 0)
    {
        /* There is only one carrier frequency defined for this baud rate */
        rx_coeff_sets = 192;
        rx_coeffs_per_filter = 27;
        rx_excess_bandwidth = 0.25;
        tx_coeff_sets = 7;
        tx_coeffs_per_filter = 9;
        tx_excess_bandwidth = 0.12;
        //carrier = 1959.0;
        carrier = 1959.0;
        baud_rate = 2400.0*10.0/7.0;
        rx_tag = "_3429";
        tx_tag = "_3429";
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
                       tx_tag);
    }
    else
    {
        make_rx_filter(rx_coeff_sets,
                       rx_coeffs_per_filter,
                       carrier,
                       baud_rate,
                       rx_excess_bandwidth,
                       rx_tag);
    }
    return 0;
}
/*- End of function --------------------------------------------------------*/
/*- End of file ------------------------------------------------------------*/
