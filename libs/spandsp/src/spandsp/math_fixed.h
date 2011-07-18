/*
 * SpanDSP - a series of DSP components for telephony
 *
 * math_fixed.h
 *
 * Written by Steve Underwood <steveu@coppice.org>
 *
 * Copyright (C) 2010 Steve Underwood
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
 */

#if !defined(_MATH_FIXED_H_)
#define _MATH_FIXED_H_

/*! \page math_fixed_page Fixed point math functions

\section math_fixed_page_sec_1 What does it do?

\section math_fixed_page_sec_2 How does it work?
*/

#if defined(__cplusplus)
extern "C"
{
#endif

#if defined(SPANDSP_USE_FIXED_POINT)
SPAN_DECLARE(uint16_t) sqrtu32_u16(uint32_t x);
#endif

SPAN_DECLARE(uint16_t) fixed_reciprocal16(uint16_t x, int *shift);

SPAN_DECLARE(uint16_t) fixed_divide16(uint16_t y, uint16_t x);

SPAN_DECLARE(uint16_t) fixed_divide32(uint32_t y, uint16_t x);

SPAN_DECLARE(int16_t) fixed_log10_16(uint16_t x);

SPAN_DECLARE(int32_t) fixed_log10_32(uint32_t x);

SPAN_DECLARE(uint16_t) fixed_sqrt16(uint16_t x);

SPAN_DECLARE(uint16_t) fixed_sqrt32(uint32_t x);

/*! Evaluate an approximate 16 bit fixed point sine.
    \brief Evaluate an approximate 16 bit fixed point sine.
    \param x A 16 bit unsigned angle, in 360/65536 degree steps.
    \return sin(x)*32767. */
SPAN_DECLARE(int16_t) fixed_sin(uint16_t x);

/*! Evaluate an approximate 16 bit fixed point cosine.
    \brief Evaluate an approximate 16 bit fixed point cosine.
    \param x A 16 bit unsigned angle, in 360/65536 degree steps.
    \return cos(x)*32767. */
SPAN_DECLARE(int16_t) fixed_cos(uint16_t x);

/*! Evaluate an approximate 16 bit fixed point sine.
    \brief Evaluate an approximate 16 bit fixed point sine.
    \param y .
    \param x .
    \return The 16 bit unsigned angle, in 360/65536 degree steps. */
SPAN_DECLARE(uint16_t) fixed_atan2(int16_t y, int16_t x);

#if defined(__cplusplus)
}
#endif

#endif
/*- End of file ------------------------------------------------------------*/
