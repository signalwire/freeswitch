/*
 * SpanDSP - a series of DSP components for telephony
 *
 * complex_vector_float.h
 *
 * Written by Steve Underwood <steveu@coppice.org>
 *
 * Copyright (C) 2003 Steve Underwood
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
 * $Id: complex_vector_float.h,v 1.8 2008/04/17 14:27:00 steveu Exp $
 */

#if !defined(_SPANDSP_COMPLEX_VECTOR_FLOAT_H_)
#define _SPANDSP_COMPLEX_VECTOR_FLOAT_H_

#if defined(__cplusplus)
extern "C"
{
#endif

static __inline__ void cvec_copyf(complexf_t z[], const complexf_t x[], int n)
{
    int i;
    
    for (i = 0;  i < n;  i++)
        z[i] = x[i];
}
/*- End of function --------------------------------------------------------*/

static __inline__ void cvec_copy(complex_t z[], const complex_t x[], int n)
{
    int i;
    
    for (i = 0;  i < n;  i++)
        z[i] = x[i];
}
/*- End of function --------------------------------------------------------*/

#if defined(HAVE_LONG_DOUBLE)
static __inline__ void cvec_copyl(complexl_t z[], const complexl_t x[], int n)
{
    int i;
    
    for (i = 0;  i < n;  i++)
        z[i] = x[i];
}
/*- End of function --------------------------------------------------------*/
#endif

static __inline__ void cvec_zerof(complexf_t z[], int n)
{
    int i;
    
    for (i = 0;  i < n;  i++)
        z[i] = complex_setf(0.0f, 0.0f);
}
/*- End of function --------------------------------------------------------*/

static __inline__ void cvec_zero(complex_t z[], int n)
{
    int i;
    
    for (i = 0;  i < n;  i++)
        z[i] = complex_set(0.0, 0.0);
}
/*- End of function --------------------------------------------------------*/

#if defined(HAVE_LONG_DOUBLE)
static __inline__ void cvec_zerol(complexl_t z[], int n)
{
    int i;
    
    for (i = 0;  i < n;  i++)
        z[i] = complex_setl(0.0, 0.0);
}
/*- End of function --------------------------------------------------------*/
#endif

static __inline__ void cvec_setf(complexf_t z[], complexf_t *x, int n)
{
    int i;
    
    for (i = 0;  i < n;  i++)
        z[i] = *x;
}
/*- End of function --------------------------------------------------------*/

static __inline__ void cvec_set(complex_t z[], complex_t *x, int n)
{
    int i;
    
    for (i = 0;  i < n;  i++)
        z[i] = *x;
}
/*- End of function --------------------------------------------------------*/

#if defined(HAVE_LONG_DOUBLE)
static __inline__ void cvec_setl(complexl_t z[], complexl_t *x, int n)
{
    int i;
    
    for (i = 0;  i < n;  i++)
        z[i] = *x;
}
/*- End of function --------------------------------------------------------*/
#endif

#if defined(__cplusplus)
}
#endif

#endif
/*- End of file ------------------------------------------------------------*/
