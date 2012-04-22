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
  bv16externs.c : BV16 Fixed-Point externs

  $Log: bv16externs.h,v $
  Revision 1.1.1.1  2009/11/19 12:10:48  steveu
  Start from Broadcom's code

  Revision 1.1.1.1  2009/11/17 14:06:02  steveu
  start

******************************************************************************/

#include "typedef.h"
#include "bv16cnst.h"
#include "bvcommon.h"

/* POINTERS */
extern const Float bv16_winl[WINSZ];
extern const Float bv16_sstwin[1 + LPCO];
extern const Float bv16_gfsz[];
extern const Float bv16_gfsp[];
extern const int bv16_idxord[];
extern const Float bv16_hpfa[];
extern const Float bv16_hpfb[];
extern const Float bv16_adf[];
extern const Float bv16_bdf[];
extern const Float bv16_x[];
extern const Float bv16_x2[];
extern const Float bv16_MPTH[];

/* LSP Quantization */
extern const Float bv16_lspecb1[LSPECBSZ1*LPCO];
extern const Float bv16_lspecb2[LSPECBSZ2*LPCO];
extern const Float bv16_lspmean[LPCO];
extern const Float bv16_lspp[LSPPORDER*LPCO];

/* Pitch Predictor Codebook */
extern const Float bv16_pp9cb[PPCBSZ*9];

/* Log-Gain Quantization */
extern const Float bv16_lgpecb[LGPECBSZ];
extern const Float bv16_lgp[LGPORDER];
extern const Float bv16_lgmean;

/* Log-Gain Limitation */
extern const Float bv16_lgclimit[];

/* Excitation Codebook */
extern const Float bv16_cccb[CBSZ*VDIM];

extern const Float bv16_lgpecb_nh[];

/* Function Prototypes */
extern Float estlevel(Float lg,
                      Float *level,
                      Float *lmax,
                      Float *lmin,
                      Float *lmean,
                      Float *x1,
                      int16_t ngfae,
                      int16_t nggalgc,
                      Float *estl_alpha_min);

extern void excdec_w_LT_synth(
        Float *ltsym, /* long-term synthesis filter memory at decoder*/
        int16_t *idx,   /* excitation codebook index array for current subframe */
        Float gainq,  /* quantized linear gains for sub-subframes */
        Float *b,     /* coefficient of 3-tap pitch predictor */
        int16_t pp,     /* pitch period */
        const Float *cb,    /* scalar quantizer codebook */
        Float *EE);

extern Float gaindec(Float *lgq,
                     int16_t gidx,
                     Float *lgpm,
                     Float *prevlg,		/* previous log gains (last two frames) */
                     Float level,
                     int16_t *nggalgc,
                     Float *lg_el);

extern Float gaindec_fe(Float lgq_last,
                        Float *lgpm);

void gainplc(Float E,
             Float *lgeqm,
             Float *lgqm);

extern void lspdec(
        Float *lspq,
        int16_t *lspidx,
        Float *lsppm,
        Float *lspq_last);

extern void lspplc(
        Float *lspq,
        Float *lsppm);

extern int coarsepitch(
        Float *xw,
        Float *xwd,
        Float *dfm,
        int	cpplast);

extern int refinepitch(
        Float *x,
        int cpp,
        Float *ppt);

extern int pitchtapquan(
        Float *x,
        int	pp,
        Float *b,
        Float *re);

extern void excquan(
        int16_t *idx,   /* quantizer codebook index for uq[] vector */
        Float *s,     /* input speech signal vector */
        Float *aq,    /* short-term predictor coefficient array */
        Float *fsz,   /* short-term noise feedback filter - numerator */
        Float *fsp,   /* short-term noise feedback filter - denominator */
        Float *b,     /* coefficient of 3-tap pitch predictor */
        Float beta,   /* coefficient of 1-tap LT noise feedback filter */
        Float *stsym,  /* filter memory before filtering of current vector */
        Float *ltsym, /* long-term synthesis filter memory */
        Float *ltnfm, /* long-term noise feedback filter memory */
        Float *stnfz,
        Float *stnfp,
        Float *cb,    /* scalar quantizer codebook */
        int pp);    /* pitch period (# of 8 kHz samples) */

extern int gainquan(
        Float *gainq,
        Float lg,
        Float *lgpm,
        Float *prevlg,
        Float level);

extern void lspquan(
        Float *lspq,
        int16_t *lspidx,
        Float *lsp,
        Float *lsppm);

extern void bv16_pp3dec(int16_t idx, Float *b);
