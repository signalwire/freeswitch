/*
 * broadvoice - a library for the BroadVoice 16 and 32 codecs
 *
 * bvcommon.h -
 *
 * Adapted by Steve Underwood <steveu@coppice.org> from code which is
 * Copyright 2000-2009 Broadcom Corporation
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
 * $Id: bvcommon.h,v 1.1.1.1 2009/11/19 12:10:48 steveu Exp $
 */

#include "typedef.h"

#ifndef  BVCOMMON_H
#define  BVCOMMON_H

/* Function Prototypes */

void apfilter(const Float *a,       /* (i) a[m+1] prediction coefficients (m=10) */
              int m,                /* (i) LPC order */
              Float *x,             /* (i) input signal */
              Float *y,             /* (o) output signal */
              int lg,               /* (i) size of filtering */
              Float *mem,           /* (i/o) input memory */
              int16_t update);      /* (i) flag for memory update */

void azfilter(const Float *a,       /* (i) prediction coefficients */
              int m,                /* (i) LPC order */
              Float *x,             /* (i) input signal vector */
              Float *y,             /* (o) output signal vector */
              int lg,               /* (i) size of filtering */
              Float *mem,           /* (i/o) filter memory before filtering */
              int16_t update);      /* (i) flag for memory update */

void Autocor(Float *r,              /* (o) : Autocorrelations */
             Float *x,              /* (i) : Input signal */
             const Float *window,   /* (i) : LPC Analysis window */
             int l_window,          /* (i) : window length */
             int m);                /* (i) : LPC order */

void Levinson(Float *r,	            /* (i): autocorrelation coefficients */
              Float *a,	            /* (o): LPC coefficients */
              Float *old_a,	        /* (i/o): LPC coefficients of previous frame */
              int m);               /* (i): LPC order */

void a2lsp(Float pc[],              /* (i) input the np+1 predictor coeff. */
           Float lsp[],             /* (o) line spectral pairs */
           Float old_lsp[]);        /* (i/o) old lsp[] (in case not found 10 roots) */

void lsp2a(Float *lsp,              /* (i) LSP vector */
           Float *a);               /* (o) LPC coefficients */

void stblz_lsp(Float *lsp, int order);

int stblchck(Float *x, int vdim);

/* LPC to LSP Conversion */
extern Float grid[];

/* LPC bandwidth expansion */
extern Float bwel[];

/* LPC WEIGHTING FILTER */
extern Float STWAL[];

/* ----- Basic Codec Parameters ----- */
#define LPCO  8 /* LPC Order  */
#define Ngrd 60 /* LPC to LSP Conversion */

#define LSPMIN  0.00150 /* minimum LSP frequency,      6/12 Hz for BV16/BV32 */
#define LSPMAX  0.99775 /* maximum LSP frequency, 3991/7982 Hz for BV16/BV32 */
#define DLSPMIN 0.01250 /* minimum LSP spacing,      50/100 Hz for BV16/BV32 */
#define STBLDIM 3       /* dimension of stability enforcement */

#endif /* BVCOMMON_H */
