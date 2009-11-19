/*
 * broadvoice - a library for the BroadVoice 16 and 32 codecs
 *
 * bv32strct.h - 
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
 * $Id: bv32strct.h,v 1.1.1.1 2009/11/19 12:10:48 steveu Exp $
 */

#include "typedef.h"
#include "bv32cnst.h"
#include "bvcommon.h"

#if !defined(_BV32STRCT_H_)
#define _BV32STRCT_H_

struct bv32_decode_state_s
{
    Float stsym[LPCO];
    Float ltsym[LTMOFF];
    Float lsppm[LPCO*LSPPORDER];
    Float lgpm[LGPORDER];
    Float lsplast[LPCO];
    Float dezfm[PFO];
    Float depfm[PFO];
    int16_t cfecount;
    uint32_t idum;
    Float E;
    Float scplcg;
    Float per;
    Float atplc[LPCO + 1];
    int16_t pp_last;
    Float prevlg[2];
    Float lgq_last;
    Float bq_last[3];
    Float lmax;                     /* level-adaptation */
    Float lmin;
    Float lmean;
    Float x1;
    Float level;
    int16_t nclglim;
    int16_t lctimer;
};

struct bv32_encode_state_s
{
    Float x[XOFF];
    Float xwd[XDOFF];               /* memory of DECF:1 decimated version of xw() */
    Float dq[XOFF];                 /* quantized short-term pred error */
    Float dfm[DFO];                 /* decimated xwd() filter memory */
    Float stpem[LPCO];              /* ST Pred. Error filter memory, low-band */
    Float stwpm[LPCO];              /* ST Weighting all-Pole Memory, low-band */
    Float stnfm[LPCO];              /* ST Noise Feedback filter Memory, Lowband */
    Float stsym[LPCO];              /* ST Synthesis filter Memory, Lowband */
    Float ltsym[MAXPP1 + FRSZ];     /* long-term synthesis filter memory */
    Float ltnfm[MAXPP1 + FRSZ];     /* long-term noise feedback filter memory */
    Float lsplast[LPCO];
    Float lsppm[LPCO*LSPPORDER];    /* LSP Predictor Memory */
    Float lgpm[LGPORDER];
    Float hpfzm[HPO];
    Float hpfpm[HPO];
    Float prevlg[2];
    Float lmax;                     /* level-adaptation */
    Float lmin;
    Float lmean;
    Float x1;
    Float level;
    int cpplast;                    /* pitch period pf the previous frame */
    Float allast[LPCO + 1];
};

struct BV32_Bit_Stream
{
    int16_t lspidx[3];
    int16_t ppidx;
    int16_t bqidx;
    int16_t gidx[2];
    int16_t qvidx[NVPSF];
};

#endif
