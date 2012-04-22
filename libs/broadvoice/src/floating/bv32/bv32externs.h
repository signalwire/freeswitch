/*
 * broadvoice - a library for the BroadVoice 16 and 32 codecs
 *
 * bv32externs.h - 
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
 * $Id: bv32externs.h,v 1.1.1.1 2009/11/19 12:10:48 steveu Exp $
 */

#include "typedef.h"
#include "bv32cnst.h"
#include "bvcommon.h"

/* Pointers */
extern const Float bv32_winl[];
extern const Float bv32_sstwin[];
extern const int bv32_idxord[];
extern const Float bv32_hpfa[];
extern const Float bv32_hpfb[];
extern const Float bv32_adf[];
extern const Float bv32_bdf[];
extern const Float bv32_x[];
extern const Float bv32_x2[];
extern Float bv32_invk[];
extern const Float bv32_MPTH[];


/* LSP Quantization */
extern const Float bv32_lspecb1[LSPECBSZ1*LPCO];
extern const Float bv32_lspecb21[LSPECBSZ21*SVD1];
extern const Float bv32_lspecb22[LSPECBSZ22*SVD2];
extern const Float bv32_lspmean[LPCO];
extern const Float bv32_lspp[LSPPORDER*LPCO];

/* Pitch Predictor Codebook */
extern const Float bv32_pp9cb[];

/* Log-Gain Quantization */
extern const Float bv32_lgpecb[LGPECBSZ];
extern const Float bv32_lgp[LGPORDER];
extern const Float bv32_lgmean;

/* Log-Gain Limitation */
extern const Float bv32_lgclimit[];

/* Excitation Codebook */
extern const Float bv32_cccb[CBSZ*VDIM];

extern const Float bv32_lgpecb_nh[];
extern const Float bv32_a_pre[];
extern const Float bv32_b_pre[];

/* Function Prototypes */

extern Float bv32_estlevel(Float lg,
                           Float *level,
                           Float *lmax,
                           Float *lmin,
                           Float *lmean,
                           Float *x1);

extern void bv32_excdec_w_LT_synth(Float *ltsymd,  /* long-term synthesis filter memory at decoder*/
                                   int16_t *idx,   /* excitation codebook index array for current subframe */
                                   Float *gainq,   /* quantized linear gains for sub-subframes */
                                   Float *b,       /* coefficient of 3-tap pitch predictor */
                                   int16_t pp,     /* pitch period (# of 8 kHz samples) */
                                   Float *EE);

extern Float bv32_gaindec(Float *lgq,
                          int16_t gidx,
                          Float *lgpm,
                          Float *prevlg,
                          Float level,
                          int16_t *nclglim,
                          int16_t lctimer);

extern void bv32_gainplc(Float E,
                         Float *lgeqm,
                         Float *lgqm);

extern void bv32_lspdec(Float *lspq,
                        int16_t *lspidx,
                        Float *lsppm,
                        Float *lspq_last);

extern void bv32_lspplc(Float *lspq,
                        Float *lsppm);

extern int bv32_coarsepitch(Float *xw,
                            Float *xwd,
                            Float *dfm,
                            int cpplast);


extern int bv32_refinepitch(Float *x,
                            int cpp,
                            Float *ppt);

extern int bv32_pitchtapquan(Float *x,
                             int pp,
                             Float *b);

extern void bv32_excquan(
        Float *qv,    /* output quantized excitation signal vector */
        int16_t *idx,   /* quantizer codebook index for uq[] vector */
        Float *d,     /* input prediction residual signal vector */
        Float *h,     /* noise feedback filter coefficient array */
        Float *b,     /* coefficient of 3-tap pitch predictor */
        Float beta,   /* coefficient of weighted 3-tap pitch predictor */
        Float *ltsym, /* long-term synthesis filter memory */
        Float *ltnfm, /* long-term noise feedback filter memory */
        Float *stnfm, /* short-term noise feedback filter memory */
        Float *cb,    /* scalar quantizer codebook */
        int pp);    /* pitch period (# of 8 kHz samples) */

extern int bv32_gainquan(Float *gainq,
                         Float lg,
                         Float *lgpm,
                         Float *prevlg,
                         Float level);

extern void bv32_lspquan(Float *lspq,
                         int16_t *lspidx,
                         Float *lsp,
                         Float *lsppm);

extern void bv32_pp3dec(int16_t idx, Float *b);
