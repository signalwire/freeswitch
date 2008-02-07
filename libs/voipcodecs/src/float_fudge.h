/*
 * SpanDSP - a series of DSP components for telephony
 *
 * float_fudge.h - A bunch of shims, to use double maths
 *                 functions on platforms which lack the
 *                 float versions with an 'f' at the end.
 *
 * Written by Steve Underwood <steveu@coppice.org>
 *
 * Copyright (C) 2004 Steve Underwood
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
 *
 * $Id: float_fudge.h,v 1.2 2007/08/13 11:35:32 steveu Exp $
 */

#if !defined(_FLOAT_FUDGE_H_)
#define _FLOAT_FUDGE_H_

#if defined(__USE_DOUBLE_MATH__)

#if defined(__cplusplus)
extern "C"
{
#endif

static __inline__ float sinf(float x)
{
	return (float) sin((double) x);
}

static __inline__ float cosf(float x)
{
	return (float) cos((double) x);
}

static __inline__ float tanf(float x)
{
	return (float) tan((double) x);
}

static __inline__ float asinf(float x)
{
	return (float) asin((double) x);
}

static __inline__ float acosf(float x)
{
	return (float) acos((double) x);
}

static __inline__ float atanf(float x)
{
	return (float) atan((double) x);
}

static __inline__ float atan2f(float y, float x)
{
	return (float) atan2((double) y, (double) x);
}

static __inline__ float ceilf(float x)
{
	return (float) ceil((double) x);
}

static __inline__ float floorf(float x)
{
	return (float) floor((double) x);
}

static __inline__ float expf(float x)
{
    return (float) expf((double) x);
}

static __inline__ float logf(float x)
{
	return (float) logf((double) x);
}

static __inline__ float log10f(float x)
{
    return (float) log10((double) x);
}

static __inline__ float powf(float x, float y)
{
    return (float) pow((double) x, (double) y);
}

static __inline__ int rintf(float x)
{
	return (int) rint((double) x);
}

static __inline__ long int lrintf(float x)
{
    return (long int) lrint((double) x);
}

#if defined(__cplusplus)
}
#endif

#endif

#endif

/*- End of file ------------------------------------------------------------*/
