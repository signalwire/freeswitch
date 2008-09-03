/*
 * SpanDSP - a series of DSP components for telephony
 *
 * vector_int.h
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
 * $Id: vector_int.h,v 1.10 2008/04/17 14:27:01 steveu Exp $
 */

#if !defined(_SPANDSP_VECTOR_INT_H_)
#define _SPANDSP_VECTOR_INT_H_

#if defined(__cplusplus)
extern "C"
{
#endif

int32_t vec_dot_prodi16(const int16_t x[], const int16_t y[], int n);

/*! \brief Find the minimum and maximum values in a vector.
    \param x The vector to be searched.
    \param n The number of elements in the vetor.
    \param out A two element vector. The first will receive the 
           maximum. The second will receive the minimum. This parameter
           may be set to NULL.
    \return The absolute maximum value. Since the range of negative numbers
            exceeds the range of positive one, the returned integer is longer
            than the ones being searched. */
int32_t vec_min_maxi16(const int16_t x[], int n, int16_t out[]);

static __inline__ int vec_norm2i16(const int16_t *vec, int len)
{
    int i;
    int sum;

    sum = 0;
    for (i = 0;  i < len;  i++)
        sum += vec[i]*vec[i];
    return sum;
}
/*- End of function --------------------------------------------------------*/

static __inline__ void vec_sari16(int16_t *vec, int len, int shift)
{
    int i;

    for (i = 0;  i < len;  i++)
        vec[i] >>= shift;
}
/*- End of function --------------------------------------------------------*/

static __inline__ int vec_max_bitsi16(const int16_t *vec, int len)
{
    int i;
    int max;
    int v;
    int b;

    max = 0;
    for (i = 0;  i < len;  i++)
    {
        v = abs(vec[i]);
        if (v > max)
            max = v;
    }
    b = 0;
    while (max != 0)
    {
        b++;
        max >>= 1;
    }
    return b;
}
/*- End of function --------------------------------------------------------*/

#if defined(__cplusplus)
}
#endif

#endif
/*- End of file ------------------------------------------------------------*/
