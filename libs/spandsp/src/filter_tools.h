/*
 * SpanDSP - a series of DSP components for telephony
 *
 * filter_tools.h - A collection of routines used for filter design.
 *
 * Written by Steve Underwood <steveu@coppice.org>
 *
 * Copyright (C) 2008 Steve Underwood
 *
 * Based on:
 * mkshape -- design raised cosine FIR filter
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
 * $Id: filter_tools.h,v 1.3 2008/04/17 14:26:56 steveu Exp $
 */

#if !defined(_FILTER_TOOLS_H_)
#define _FILTER_TOOLS_H_

#if defined(__cplusplus)
extern "C"
{
#endif

void ifft(complex_t data[], int len);
void apply_hamming_window(double coeffs[], int len);
void truncate_coeffs(double coeffs[], int len, int bits, int hilbert);

void compute_raised_cosine_filter(double coeffs[],
                                  int len,
                                  int root,
                                  int sinc_compensate,
                                  double alpha,
                                  double beta);

void compute_hilbert_transform(double coeffs[], int len);

    
#if defined(__cplusplus)
}
#endif

#endif
/*- End of file ------------------------------------------------------------*/
