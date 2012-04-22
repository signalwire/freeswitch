/*****************************************************************************/
/* BroadVoice(R)16 (BV16) Floating-Point ANSI-C Source Code                  */
/* Revision Date: August 19, 2009                                            */
/* Version 1.0                                                               */
/*****************************************************************************/

/*****************************************************************************/
/* Copyright 2000-2009 Broadcom Corporation                                  */
/*                                                                           */
/* This software is provided under the GNU Lesser General Public License,    */
/* version 2.1, as published by the Free Software Foundation ("LGPL").       */
/* This program is distributed in the hope that it will be useful, but       */
/* WITHOUT ANY SUPPORT OR WARRANTY; without even the implied warranty of     */
/* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the LGPL for     */
/* more details.  A copy of the LGPL is available at                         */
/* http://www.broadcom.com/licenses/LGPLv2.1.php,                            */
/* or by writing to the Free Software Foundation, Inc.,                      */
/* 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.                 */
/*****************************************************************************/


/*****************************************************************************
  bv16strct.h : BV16 data structures

  $Log: bv16strct.h,v $
  Revision 1.1.1.1  2009/11/19 12:10:48  steveu
  Start from Broadcom's code

  Revision 1.1.1.1  2009/11/17 14:06:02  steveu
  start

******************************************************************************/

#include "typedef.h"
#include "bv16cnst.h"
#include "bvcommon.h"

#if !defined(_BV16STRCT_H_)
#define  _BV16STRCT_H_

struct bv16_decode_state_s
{
    Float stsym[LPCO];
    Float ltsym[LTMOFF];
    Float lsppm[LPCO*LSPPORDER];
    Float lgpm[LGPORDER];
    Float lsplast[LPCO];
    Float prevlg[2];
    Float lmax;                     /* level-adaptation */
    Float lmin;
    Float lmean;
    Float x1;
    Float level;
    int16_t pp_last;
    int16_t ngfae;
    Float bq_last[3];
    int16_t nggalgc;
    Float estl_alpha_min;
    int16_t cfecount;
    uint32_t idum;
    Float E;
    Float per;
    Float atplc[LPCO + 1];
    Float ma_a;
    Float b_prv[2];
    Float xq[XQOFF];
    int pp_prv;
};

struct bv16_encode_state_s
{
    Float x[XOFF];                  /* 8kHz down-sampled signal memory */
    Float xwd[XDOFF];               /* memory of DECF:1 decimated version of xw() */
    Float dq[XOFF];                 /* quantized short-term pred error */
    Float dfm[DFO];                 /* decimated xwd() filter memory */
    Float stpem[LPCO];              /* ST Pred. Error filter memory */
    Float stwpm[LPCO];              /* ST Weighting all-Pole Memory */
    Float stsym[LPCO];              /* ST Synthesis filter Memory */
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
    Float old_A[LPCO + 1];
    Float stnfz[NSTORDER];
    Float stnfp[NSTORDER];
};

struct BV16_Bit_Stream
{
    int16_t lspidx[2];
    int16_t ppidx;
    int16_t bqidx;
    int16_t gidx;
    int16_t qvidx[FRSZ/VDIM];
};

#endif /* BV16STRCT_H */
