/*
 * broadvoice - a library for the BroadVoice 16 and 32 codecs
 *
 * broadvoice.h - The head guy amongst the headers
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
 * $Id: broadvoice.h,v 1.1.1.1 2009/11/19 12:10:48 steveu Exp $
 */

/*! \file */

#if !defined(_BROADVOICE_PRIVATE_BROADVOICE_H_)
#define _BROADVOICE_PRIVATE_BROADVOICE_H_

typedef	double Float;

#define LPCO        8                       /* LPC Order */
#define HPO         2                       /* Front end 150Hz high-pass filter order */
#define DFO         4

#define BV16_FRSZ       40                  /* Frame size */
#define BV16_MAXPP      137                 /* MAXimum Pitch Period */
#define BV16_PWSZ       120	                /* Pitch analysis Window SiZe */
#define BV16_XQOFF      (BV16_MAXPP + 1)    /* xq() offset before current subframe */
#define BV16_XOFF       (BV16_MAXPP + 1)    /* Offset for x() frame */
#define BV16_LTMOFF     (BV16_MAXPP + 1)    /* Long-Term filter Memory OFFset */
#define BV16_LSPPORDER  8                   /* LSP MA Predictor ORDER */
#define	BV16_NSTORDER   8                   /* Pole-zero NFC shaping filter */
#define BV16_LGPORDER   8                   /* Log-Gain Predictor OODER */
#define BV16_DECF       4                   /* DECimation Factor for coarse pitch period search */
#define BV16_XDOFF      (BV16_LXD - BV16_FRSZD)

#define BV16_FRSZD      (BV16_FRSZ/BV16_DECF)         /* FRame SiZe in DECF:1 lowband domain */
#define BV16_PWSZD      (BV16_PWSZ/BV16_DECF)         /* Pitch ana. Window SiZe in DECF:1 domain */
#define BV16_MAXPPD     (BV16_MAXPP/BV16_DECF)        /* MAX Pitch in DECF:1, if MAXPP!=4n, ceil()  */
#define BV16_LXD        (BV16_MAXPPD + 1 + BV16_PWSZD)

#define BV32_FRSZ       80                  /* Frame size */
#define BV32_MAXPP      265                 /* MAXimum Pitch Period */
#define BV32_PWSZ       240                 /* Pitch analysis Window SiZe for 8kHz lowband */
#define BV32_XOFF       (BV32_MAXPP + 1)    /* offset for x() frame */
#define BV32_LTMOFF     (BV32_MAXPP + 1)    /* Long-Term filter Memory OFFset */
#define BV32_LSPPORDER	8	                /* LSP MA Predictor ORDER */
#define BV32_PFO        1                   /* Preemphasis filter order */
#define BV32_LGPORDER	16                  /* Log-Gain Predictor OODER */
#define BV32_DECF       8                   /* DECimation Factor for coarse pitch period search   */
#define BV32_XDOFF      (BV32_LXD - BV32_FRSZD)

#define BV32_FRSZD      (BV32_FRSZ/BV32_DECF)         /* FRame SiZe in DECF:1 lowband domain */
#define BV32_PWSZD      (BV32_PWSZ/BV32_DECF)         /* Pitch ana. Window SiZe in DECF:1 domain */
#define BV32_MAXPPD     (BV32_MAXPP/BV32_DECF)        /* MAX Pitch in DECF:1, if MAXPP!=4n, ceil()  */
#define BV32_LXD        (BV32_MAXPPD + 1 + BV32_PWSZD)

struct bv16_decode_state_s
{
    Float stsym[LPCO];
    Float ltsym[BV16_LTMOFF];
    Float lsppm[LPCO*BV16_LSPPORDER];
    Float lgpm[BV16_LGPORDER];
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
    Float xq[BV16_XQOFF];
    int pp_prv;
};

struct bv16_encode_state_s
{
    Float x[BV16_XOFF];             /* 8kHz down-sampled signal memory */
    Float xwd[BV16_XDOFF];          /* memory of DECF:1 decimated version of xw() */
    Float dq[BV16_XOFF];            /* quantized short-term pred error */
    Float dfm[DFO];                 /* decimated xwd() filter memory */
    Float stpem[LPCO];              /* ST Pred. Error filter memory */
    Float stwpm[LPCO];              /* ST Weighting all-Pole Memory */
    Float stsym[LPCO];              /* ST Synthesis filter Memory */
    Float ltsym[BV16_MAXPP + 1 + BV16_FRSZ];  /* long-term synthesis filter memory */
    Float ltnfm[BV16_MAXPP + 1 + BV16_FRSZ];  /* long-term noise feedback filter memory */
    Float lsplast[LPCO];
    Float lsppm[LPCO*BV16_LSPPORDER];    /* LSP Predictor Memory */
    Float lgpm[BV16_LGPORDER];
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
    Float stnfz[BV16_NSTORDER];
    Float stnfp[BV16_NSTORDER];
};

struct bv32_decode_state_s
{
    Float stsym[LPCO];
    Float ltsym[BV32_LTMOFF];
    Float lsppm[LPCO*BV32_LSPPORDER];
    Float lgpm[BV32_LGPORDER];
    Float lsplast[LPCO];
    Float dezfm[BV32_PFO];
    Float depfm[BV32_PFO];
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
    Float x[BV32_XOFF];
    Float xwd[BV32_XDOFF];          /* Memory of DECF:1 decimated version of xw() */
    Float dq[BV32_XOFF];            /* Quantized short-term pred error */
    Float dfm[DFO];                 /* Decimated xwd() filter memory */
    Float stpem[LPCO];              /* ST Pred. Error filter memory, low-band */
    Float stwpm[LPCO];              /* ST Weighting all-Pole Memory, low-band */
    Float stnfm[LPCO];              /* ST Noise Feedback filter Memory, Lowband */
    Float stsym[LPCO];              /* ST Synthesis filter Memory, Lowband */
    Float ltsym[BV32_MAXPP + 1 + BV32_FRSZ];  /* Long-term synthesis filter memory */
    Float ltnfm[BV32_MAXPP + 1 + BV32_FRSZ];  /* Long-term noise feedback filter memory */
    Float lsplast[LPCO];
    Float lsppm[LPCO*BV32_LSPPORDER];    /* LSP Predictor Memory */
    Float lgpm[BV32_LGPORDER];
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

#endif
/*- End of file ------------------------------------------------------------*/
