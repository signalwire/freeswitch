/*
 * SpanDSP - a series of DSP components for telephony
 *
 * mmx_sse_decs.h - Pull in the appropriate systems headers for the MMX/SSE settings.
 *
 * Written by Steve Underwood <steveu@coppice.org>
 *
 * Copyright (C) 2009 Steve Underwood
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
 * $Id: mmx_sse_decs.h,v 1.1 2009/07/12 09:23:09 steveu Exp $
 */

#if !defined(_MMX_SSE_DECS_H_)
#define _MMX_SSE_DECS_H_

#if defined(SPANDSP_USE_MMX)
#include <mmintrin.h>
#endif
#if defined(SPANDSP_USE_SSE)
#include <xmmintrin.h>
#endif
#if defined(SPANDSP_USE_SSE2)
#include <emmintrin.h>
#endif
#if defined(SPANDSP_USE_SSE3)
#include <pmmintrin.h>
#endif
#if defined(SPANDSP_USE_SSSE3)
#include <tmmintrin.h>
#endif
#if defined(SPANDSP_USE_SSE4_1)
#include <smmintrin.h>
#endif
#if defined(SPANDSP_USE_SSE4_2)
#include <nmmintrin.h>
#endif
#if defined(SPANDSP_USE_SSE4A)
#include <ammintrin.h>
#endif
#if defined(SPANDSP_USE_SSE5)
#include <bmmintrin.h>
#endif

#endif

/*- End of include ---------------------------------------------------------*/
