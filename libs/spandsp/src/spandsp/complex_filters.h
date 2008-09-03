/*
 * SpanDSP - a series of DSP components for telephony
 *
 * complex_filters.h
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
 * $Id: complex_filters.h,v 1.12 2008/04/17 14:27:00 steveu Exp $
 */

#if !defined(_SPANDSP_COMPLEX_FILTERS_H_)
#define _SPANDSP_COMPLEX_FILTERS_H_

typedef struct filter_s filter_t;

typedef float (*filter_step_func_t)(filter_t *fi, float x);

/*! Filter state */
typedef struct
{
    int                 nz;
    int                 np;
    filter_step_func_t  fsf;
} fspec_t;

struct filter_s
{
    fspec_t             *fs;
    float               sum;
    int                 ptr;		/* for moving average filters only */
    float               v[];
};

typedef struct
{
    filter_t            *ref;
    filter_t            *imf;
} cfilter_t;

#if defined(__cplusplus)
extern "C"
{
#endif

filter_t *filter_create(fspec_t *fs);
void filter_delete(filter_t *fi);
float filter_step(filter_t *fi, float x);

cfilter_t *cfilter_create(fspec_t *fs);
void cfilter_delete(cfilter_t *cfi);
complexf_t cfilter_step(cfilter_t *cfi, const complexf_t *z);

#if defined(__cplusplus)
}
#endif

#endif
/*- End of file ------------------------------------------------------------*/
