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
  bv16cnst.h : BV16 constants

  $Log: bv16cnst.h,v $
  Revision 1.1.1.1  2009/11/19 12:10:48  steveu
  Start from Broadcom's code

  Revision 1.1.1.1  2009/11/17 14:06:02  steveu
  start

******************************************************************************/

#include "typedef.h"

#ifndef  BV16CNST_H
#define  BV16CNST_H


/* ----- Basic Codec Parameters ----- */
#define FRSZ 	40 		/* frame size */
#define WINSZ	160		/* lpc analysis WINdow SiZe  */
#define MAXPP   137     /* MAXimum Pitch Period         		  */
#define MINPP   10      /* MINimum Pitch Period          		  */
#define PWSZ	120	    /* Pitch analysis Window SiZe   */
#define MAXPP1 (MAXPP+1)/* MAXimum Pitch Period + 1  	  */

/* Quantization parameters */
#define VDIM		  4	/* excitation vector dimension */
#define CBSZ		 16	/* excitation codebook size */
#define PPCBSZ     32	/* pitch predictor codebook size */
#define LGPORDER	  8	/* Log-Gain Predictor OODER */
#define LGPECBSZ	 16	/* Log-Gain Prediction Error CodeBook SiZe */
#define LSPPORDER   8	/* LSP MA Predictor ORDER */
#define LSPECBSZ1 128	/* codebook size of 1st-stage LSP VQ */
#define LSPECBSZ2  64	/* codebook size of 2nd-stage LSP VQ; 1-bit for sign */

#define INVFRSZ (1./(Float)FRSZ)

/* Excitation gain quantization */
#define LGMEAN   11.45752 /* log2-gain mean               */
#define GPO       8       /* order of MA prediction       */
#define NG       18       /* number of relative gain bins */
#define GLB     -24.      /* lower relative gain bound    */
#define NGC      12       /* number of gain change bins   */
#define GCLB     -8.      /* lower gain change bound      */
#define Minlg     0.      /* minimum log-gain             */
#define TMinlg    1.      /* minimum linear gain          */
#define LGCBSZ   16       /* size of codebook             */

/* Definitions for periodicity to gain scaling mapping */
#define ScPLCGmin 0.1
#define ScPLCGmax 0.9
#define PePLCGmin 0.5
#define PePLCGmax 0.9
#define ScPLCG_b ((ScPLCGmin-ScPLCGmax)/(PePLCGmax-PePLCGmin))
#define ScPLCG_a (ScPLCGmin-ScPLCG_b*PePLCGmax)
#define HoldPLCG  8
#define AttnPLCG 50
#define AttnFacPLCG (1.0/(Float)AttnPLCG)

/* Level Estimation */
#define estl_alpha  (4095./4096.)
#define estl_alpha1 (255./256.)
#define estl_beta   (511./512.)
#define estl_beta1  (1.-estl_beta)
#define estl_a      (255./256)
#define estl_a1     (1-estl_a)
#define estl_TH     0.2
#define Nfdm        100 /* Max number of frames with fast decay of Lmin */

/* Log-Gain Limitation */
#define  LGLB   -24         /* Relative (to input level) Log-Gain Lower Bound */
#define  LGCLB  -8          /* Log-Gain Change Lower Bound */
#define  NGB    18          /* Number of Gain Bins */
#define  NGCB   12          /* Number of Gain Change Bins */

/* Buffer offsets and sizes */
#define XOFF    MAXPP1       /* offset for x() frame      */
#define LX      (XOFF+FRSZ)  /* Length of x() buffer      */
#define XQOFF   (MAXPP1)     /* xq() offset before current subframe */
#define LXQ     (XQOFF+FRSZ) /* Length of xq() buffer */
#define LTMOFF	 (MAXPP1)	  /* Long-Term filter Memory OFFset */

/* Long-term postfilter */
#define DPPQNS 4             /* Delta pitch period for search             */
#define NINT  20             /* length of filter interpolation            */
#define ATHLD1 0.55          /* threshold on normalized pitch correlation */
#define ATHLD2 0.80          /* threshold on normalized pitch correlation */
#define ScLTPF 0.3           /* scaling of LTPF coefficient               */

/* coarse pitch search */
#define	cpp_Qvalue	2
#define	cpp_scale	(1<<cpp_Qvalue)
#define	MAX_NPEAKS 7
#define MPTH4   0.3     /* value to use for MPTH[] with index >= 4 */
#define DEVTH   0.25	  /* pitch period DEViation THreshold 	*/
#define TH1	    0.73    /* first threshold for cor*cor/energy 	*/
#define TH2	    0.4     /* second threshold for cor*cor/energy 	*/
#define LPTH1   0.79    /* Last Pitch cor*cor/energy THreshold 1 */
#define LPTH2   0.43    /* Last Pitch cor*cor/energy THreshold 2 */
#define MPDTH   0.065   /* Multiple Pitch Deviation THreshold */
#define SMDTH   0.095   /* Sub-Multiple pitch Deviation THreshold */
#define SMDTH1  (1.0-SMDTH)
#define SMDTH2  (1.0+SMDTH)
#define MPR1    (1.0-MPDTH)    /* Multiple Pitch Range lower threshold */
#define MPR2    (1.0+MPDTH)    /* Multiple Pitch Range upper threshold */

/* Decimation parameters */
#define DECF    4   /* DECimation Factor for coarse pitch period search   */
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

/* Front-end 150 Hz highpass filter */
#define HPO     2       /* High-pass filter order */

/* LPC weighting filter */
#define	LTWFL	0.5

/* pole-zero NFC shaping filter */
#define	NSTORDER 8

#endif /* BV16CNST_H */
