/*
 * broadvoice - a library for the BroadVoice 16 and 32 codecs
 *
 * bv32cnst.h - 
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
 * $Id: bv32cnst.h,v 1.1.1.1 2009/11/19 12:10:48 steveu Exp $
 */

#include "typedef.h"

#ifndef	__BV32__
#define __BV32__

/* ----- Basic Codec Parameters ----- */
#define SF		16			/* input Sampling Frequency (in kHz) */
#define FRSZ	80	        /* the FRame SiZe */
#define MAXPP	265	        /* MAXimum Pitch Period */
#define MINPP	10	        /* Half of MINimum Pitch Period */
#define NSF	2			    /* number of subframes per frame */
#define PWSZ	240 	    /* Pitch analysis Window SiZe for 8kHz lowband */
#define SFRSZ	(FRSZ/NSF)	/* SubFrame SiZe */
#define WINSZ	160	        /* Half of LPC analysis WINdow SiZe	*/
#define MAXPP1	(MAXPP+1)	/* Half of MAXimum Pitch Period + 1a */

/* NFC VQ coding parameters */
#define VDIM		4	    /* excitation vector dimension */
#define CBSZ		32	    /* codebook size */
#define PPCBSZ		32
#define LGPORDER	16	    /* Log-Gain Predictor OODER */
#define LGPECBSZ	32	    /* Log-Gain Prediction Error CodeBook SiZe */
#define LSPPORDER	8	    /* LSP MA Predictor ORDER */
#define LSPECBSZ1	128	    /* codebook size of 1st-stage LSP VQ */
#define SVD1		3	    /* split VQ dimension 1 */
#define LSPECBSZ21	32	    /* codebook size of 2nd-stage LSP split VQ */
#define SVD2		5	    /* split VQ dimension 2 */
#define LSPECBSZ22	32	    /* codebook size of 2nd stage LSP split VQ */

#define NVPSF		(FRSZ/VDIM)
#define NVPSSF		(SFRSZ/VDIM)

/* Packetloss Concealment */
#define ScPLCGmin   0.1
#define ScPLCGmax   0.9
#define PePLCGmin   0.5
#define PePLCGmax   0.9
#define ScPLCG_b    ((ScPLCGmin - ScPLCGmax)/(PePLCGmax - PePLCGmin))
#define ScPLCG_a    (ScPLCGmin - ScPLCG_b*PePLCGmax)
#define HoldPLCG    8
#define AttnPLCG    50
#define AttnFacPLCG (1.0/(Float)AttnPLCG)

/* Pre-emphasis filter coefficients */
#define PEAPFC      0.75
#define PEAZFC	    0.5

#define INVSFRSZ (1./(Float)SFRSZ)
#define FECNSF      2       /* number of FEC subframes per frame */

#define Minlg       -2.0    /* minimum log-gain */
#define TMinlg      0.25    /* minimum linear gain */
#define GPO         16      /* order of MA prediction */

/* Level Estimation */
#define estl_alpha  (8191.0/8192.0)
#define estl_beta   (1023.0/1024.0)
#define estl_beta1  (1.0 - estl_beta)
#define estl_a      (511.0/512.0)
#define estl_a1     (1 - estl_a)
#define estl_TH     0.2

/* Log-Gain Limitation */
#define LGLB   -24      /* Log-Gain Lower Bound */
#define GCLB    -8      /* Log-Gain Change Lower Bound */
#define NGB     18      /* Number of Gain Bins */
#define NGCB    11      /* Number of Gain Change Bins */
#define MinE  -2.0

#define PFO     1   /* preemphasis filter order */

#define LTMOFF MAXPP1	/* Long-Term filter Memory OFFset */

/* Parameters related to the gain decoder trap */
#define NCLGLIM_TRAPPED        50  /* 0.125 sec */
#define LEVEL_CONVERGENCE_TIME 100 /* 0.25 sec */

/* front-end highpass filter */
#define HPO     2       /* High-pass filter order */

/* LPC weighting filter */
#define	LTWFL	0.5

/* Minimum gain threshold */
#define TMinE (SFRSZ*0.25)

/* coarse pitch search */
#define cpp_Qvalue  3
#define cpp_scale   (1<<cpp_Qvalue)

/* ------ Decimation Parameters ----- */
#define DECF    8   /* DECimation Factor for coarse pitch period search   */
#define FRSZD   (FRSZ/DECF)    /* FRame SiZe in DECF:1 lowband domain        */
#define MAXPPD  (MAXPP/DECF)   /* MAX Pitch in DECF:1, if MAXPP!=4n, ceil()  */
#define MINPPD  ((int) (MINPP/DECF))   /* MINimum Pitch Period in DECF:1     */
#define PWSZD   (PWSZ/DECF) /* Pitch ana. Window SiZe in DECF:1 domain    */
#define DFO 4
#define MAXPPD1 (MAXPPD + 1)
#define LXD     (MAXPPD1 + PWSZD)
#define XDOFF   (LXD - FRSZD)
#define HMAXPPD (MAXPPD/2)
#define M1      (MINPPD - 1)
#define M2      MAXPPD1
#define HDECF   (DECF/2)
#define INVDECF (1.0F/(float)(DECF)) /* INVerse of DECF (decimation factor) */

/* coarse pitch */
#define MPTH4   0.3     /* value to use for MPTH[] with index >= 4 */
#define DEVTH	0.25	/* pitch period DEViation THreshold 	*/
#define TH1	    0.73    /* first threshold for cor*cor/energy 	*/
#define TH2	    0.4     /* second threshold for cor*cor/energy 	*/
#define LPTH1   0.78    /* Last Pitch cor*cor/energy THreshold 1 */
#define LPTH2   0.43    /* Last Pitch cor*cor/energy THreshold 2 */
#define MPDTH   0.06    /* Multiple Pitch Deviation THreshold */
#define SMDTH   0.095   /* Sub-Multiple pitch Deviation THreshold */
#define SMDTH1  (1.0 - SMDTH)
#define SMDTH2  (1.0 + SMDTH)
#define MPR1    (1.0 - MPDTH)    /* Multiple Pitch Range lower threshold */
#define MPR2    (1.0 + MPDTH)    /* Multiple Pitch Range upper threshold */
#define MAX_NPEAKS	7

/* buffer offset and length */
#define XOFF    MAXPP1         /* offset for x() frame      */
#define LX      (XOFF+FRSZ)    /* Length of x() buffer      */

#endif
