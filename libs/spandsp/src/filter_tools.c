/*
 * SpanDSP - a series of DSP components for telephony
 *
 * filter_tools.c - A collection of routines used for filter design.
 *
 * Written by Steve Underwood <steveu@coppice.org>
 *
 * Copyright (C) 2008 Steve Underwood
 *
 * This includes some elements based on the mkfilter package by
 * A.J. Fisher, University of York <fisher@minster.york.ac.uk>,  November 1996
 *
 * All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License version 2.1,
 * as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 * $Id: filter_tools.c,v 1.11 2009/10/05 16:33:25 steveu Exp $
 */
 
#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#include <inttypes.h>
#include <stdlib.h>
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

#include "spandsp/telephony.h"
#include "spandsp/complex.h"
#include "filter_tools.h"

#if !defined(FALSE)
#define FALSE 0
#endif
#if !defined(TRUE)
#define TRUE (!FALSE)
#endif

#define MAXPZ	    8192
#define SEQ_LEN     8192
#define MAX_FFT_LEN SEQ_LEN

static complex_t circle[MAX_FFT_LEN/2];

static __inline__ complex_t expj(double theta)
{
    return complex_set(cos(theta), sin(theta));
}
/*- End of function --------------------------------------------------------*/

static __inline__ double fix(double x)
{
    /* Nearest integer */
    return (x >= 0.0)  ?  floor(0.5 + x)  :  -floor(0.5 - x);
}
/*- End of function --------------------------------------------------------*/

static void fftx(complex_t data[], complex_t temp[], int n)
{
    int i;
    int h;
    int p;
    int t;
    int i2;
    complex_t wkt;

    if (n > 1)
    {
        h = n/2;
        for (i = 0;  i < h;  i++)
        {
            i2 = i*2;
            temp[i] = data[i2];         /* Even */
            temp[h + i] = data[i2 + 1]; /* Odd */
        }
        fftx(&temp[0], &data[0], h);
        fftx(&temp[h], &data[h], h);
        p = 0;
        t = MAX_FFT_LEN/n;
        for (i = 0;  i < h;  i++)
        {
            wkt = complex_mul(&circle[p], &temp[h + i]);
            data[i] = complex_add(&temp[i], &wkt);
            data[h + i] = complex_sub(&temp[i], &wkt);
            p += t;
        }
    }
}
/*- End of function --------------------------------------------------------*/

void ifft(complex_t data[], int len)
{
    int i;
    double x;
    complex_t temp[MAX_FFT_LEN];

    /* A very slow and clunky FFT, that's just fine for filter design. */
    for (i = 0;  i < MAX_FFT_LEN/2;  i++)
    {
        x = (2.0*3.1415926535*i)/(double) MAX_FFT_LEN;
        circle[i] = expj(x);
    }
    fftx(data, temp, len);
}
/*- End of function --------------------------------------------------------*/

void compute_raised_cosine_filter(double coeffs[],
                                  int len,
                                  int root,
                                  int sinc_compensate,
                                  double alpha,
                                  double beta)
{
    double f;
    double x;
    double f1;
    double f2;
    double tau;
    complex_t vec[SEQ_LEN];
    int i;
    int j;
    int h;
    
    f1 = (1.0 - beta)*alpha;
    f2 = (1.0 + beta)*alpha;
    tau = 0.5/alpha;
    /* (Root) raised cosine */
    for (i = 0;  i <= SEQ_LEN/2;  i++)
    {
        f = (double) i/(double) SEQ_LEN;
        if (f <= f1)
            vec[i] = complex_set(1.0, 0.0);
        else if (f <= f2)
            vec[i] = complex_set(0.5*(1.0 + cos((3.1415926535*tau/beta)*(f - f1))), 0.0);
        else
            vec[i] = complex_set(0.0, 0.0);
    }
    if (root)
    {
        for (i = 0;  i <= SEQ_LEN/2;  i++)
            vec[i].re = sqrt(vec[i].re);
    }
    if (sinc_compensate)
    {
        for (i = 1;  i <= SEQ_LEN/2;  i++)
	    {
            x = 3.1415926535*(double) i/(double) SEQ_LEN;
	        vec[i].re *= (x/sin(x));
	    }
    }
    for (i = 0;  i <= SEQ_LEN/2;  i++)
        vec[i].re *= tau;
    for (i = 1;  i < SEQ_LEN/2;  i++)
        vec[SEQ_LEN - i] = vec[i];
    ifft(vec, SEQ_LEN);
    h = (len - 1)/2;
    for (i = 0;  i < len;  i++)
    {
        j = (SEQ_LEN - h + i)%SEQ_LEN;
        coeffs[i] = vec[j].re/(double) SEQ_LEN;
    }
}
/*- End of function --------------------------------------------------------*/

void compute_hilbert_transform(double coeffs[], int len)
{
    double x;
    int i;
    int h;

    h = (len - 1)/2;
    coeffs[h] = 0.0;
    for (i = 1;  i <= h;  i++)
    {
        if ((i & 1))
        {
            x = 1.0/(double) i;
            coeffs[h + i] = -x;
            coeffs[h - i] = x;
        }
    }
}
/*- End of function --------------------------------------------------------*/

void apply_hamming_window(double coeffs[], int len)
{
    double w;
    int i;
    int h;

    h = (len - 1)/2;
    for (i = 1;  i <= h;  i++)
    {
        w = 0.53836 - 0.46164*cos(2.0*3.1415926535*(double) (h + i)/(double) (len - 1.0));
        coeffs[h + i] *= w;
        coeffs[h - i] *= w;
    }
}
/*- End of function --------------------------------------------------------*/

void truncate_coeffs(double coeffs[], int len, int bits, int hilbert)
{
    double x;
    double fac;
    double max;
    double scale;
    int h;
    int i;

    fac = pow(2.0, (double) (bits - 1.0));
    h = (len - 1)/2;
    max = (hilbert)  ?  coeffs[h - 1]  :  coeffs[h];	/* Max coeff */
    scale = (fac - 1.0)/(fac*max);
    for (i = 0;  i < len;  i++)
    {
        x = coeffs[i]*scale;           /* Scale coeffs so max is (fac - 1.0)/fac */
        coeffs[i] = fix(x*fac)/fac;    /* Truncate */
    }
}
/*- End of function --------------------------------------------------------*/
/*- End of file ------------------------------------------------------------*/
