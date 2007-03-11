/*

$Log: vparms.c,v $
Revision 1.1  2004/05/04 11:16:43  csoutheren
Initial version

Revision 1.2  2002/02/15 03:57:55  yurik
Warnings removed during compilation, patch courtesy of Jehan Bing, jehan@bravobrava.com

Revision 1.1  2000/06/05 04:45:12  robertj
Added LPC-10 2400bps codec

 * Revision 1.1  1996/08/19  22:30:04  jaf
 * Initial revision
 *

*/

#ifdef P_R_O_T_O_T_Y_P_E_S
extern int vparms_(integer *vwin, real *inbuf, real *lpbuf, integer *buflim, integer *half, real *dither, integer *mintau, integer *zc, integer *lbe, integer *fbe, real *qs, real *rc1, real *ar_b__, real *ar_f__);
#endif

/*  -- translated by f2c (version 19951025).
   You must link the resulting object file with the libraries:
	-lf2c -lm   (in that order)
*/

#include "f2c.h"

/* Table of constant values */

static real c_b2 = 1.f;

/* ********************************************************************* */

/* 	VPARMS Version 50 */

/* $Log: vparms.c,v $
 * Revision 1.1  2004/05/04 11:16:43  csoutheren
 * Initial version
 *
 * Revision 1.2  2002/02/15 03:57:55  yurik
 * Warnings removed during compilation, patch courtesy of Jehan Bing, jehan@bravobrava.com
 *
 * Revision 1.1  2000/06/05 04:45:12  robertj
 * Added LPC-10 2400bps codec
 *
 * Revision 1.1  1996/08/19  22:30:04  jaf
 * Initial revision
 *
 */
/* Revision 1.6  1996/03/29  18:01:16  jaf */
/* Added some more comments about the range of INBUF and LPBUF that can */
/* be read.  Note that it is possible for index VWIN(2)+1 to be read from */
/* INBUF, which might be outside of its defined range, although that will */
/* require more careful checking. */

/* Revision 1.5  1996/03/19  00:02:02  jaf */
/* I just noticed that the argument DITHER is modified inside of this */
/* subroutine.  Comments were added explaining the possible final values. */

/* Revision 1.4  1996/03/18  22:22:59  jaf */
/* Finishing the job I said I did with the last check-in comments. */

/* Revision 1.3  1996/03/18  22:22:17  jaf */
/* Just added a few comments about which array indices of the arguments */
/* are used, and mentioning that this subroutine has no local state. */

/* Revision 1.2  1996/03/13  15:02:58  jaf */
/* Comments added explaining that none of the local variables of this */
/* subroutine need to be saved from one invocation to the next. */

/* Revision 1.1  1996/02/07 14:50:42  jaf */
/* Initial revision */


/* ********************************************************************* */

/*  Calculate voicing parameters: */

/* Input: */
/*  VWIN   - Voicing window limits */
/*           Indices 1 through 2 read. */
/*  INBUF  - Input speech buffer */
/*           Indices START-1 through STOP read, */
/*          where START and STOP are defined in the code (only written once).
*/
/*           Note that STOP can be as large as VWIN(2)+1 ! */
/*  LPBUF  - Low pass filtered speech */
/*           Indices START-MINTAU through STOP+MINTAU read, */
/*          where START and STOP are defined in the code (only written once).
*/
/*  BUFLIM - Array bounds for INBUF and LPBUF */
/*           Indices 1 through 4 read. */
/*  HALF   - Half frame (1 or 2) */
/*  MINTAU - Lag corresponding to minimum AMDF value (pitch estimate) */
/* Input/Output: */
/*  DITHER - Zero crossing threshold */
/*           The resulting value might be the negation of the input */
/*           value.  It might always be the same as the input value, */
/*           if the DO loop below always executes an even number of times. */
/* Output: (all of them are written on every call) */
/*  ZC     - Zero crossing rate */
/*  LBE    - Low band energy (sum of magnitudes - SM) */
/*  FBE    - Full band energy (SM) */
/*  QS     - Ratio of 6 dB/oct preemphasized energy to full band energy */
/*  RC1    - First reflection coefficient */
/*  AR_B   - Product of the causal forward and reverse pitch */
/*           prediction gains */
/*  AR_F   - Product of the noncausal forward and reverse pitch */
/*           prediction gains */
/* Internal: */
/*  OLDSGN - Previous sign of dithered signal */
/*  VLEN   - Length of voicing window */
/*  START  - Lower address of current half of voicing window */
/*  STOP   - Upper address of current half of voicing window */
/*  E_0    - Energy of LPF speech (sum of squares - SS) */
/*  E_B    - Energy of LPF speech backward one pitch period (SS) */
/*  E_F    - Energy of LPF speech forward one pitch period (SS) */
/*  R_B    - Autocovariance of LPF speech backward one pitch period */
/*  R_F    - Autocovariance of LPF speech forward one pitch period */
/*  LP_RMS - Energy of LPF speech (sum of magnitudes - SM) */
/*  AP_RMS - Energy of all-pass speech (SM) */
/*  E_PRE  - Energy of 6dB preemphasized speech (SM) */
/*  E0AP   - Energy of all-pass speech (SS) */

/* This subroutine has no local state. */

/* Subroutine */ int vparms_(integer *vwin, real *inbuf, real *lpbuf, integer 
	*buflim, integer *half, real *dither, integer *mintau, integer *zc, 
	integer *lbe, integer *fbe, real *qs, real *rc1, real *ar_b__, real *
	ar_f__)
{
    /* System generated locals */
    integer inbuf_offset, lpbuf_offset, i__1;
    real r__1, r__2;

    /* Builtin functions */
    double r_sign(real *, real *);
    integer i_nint(real *);

    /* Local variables */
    integer vlen, stop, i__;
    real e_pre__;
    integer start;
    real ap_rms__, e_0__, oldsgn, lp_rms__, e_b__, e_f__, r_b__, r_f__, e0ap;

/*       Arguments */
/*       Local variables that need not be saved */
/*   Calculate zero crossings (ZC) and several energy and correlation */
/*   measures on low band and full band speech.  Each measure is taken */
/*   over either the first or the second half of the voicing window, */
/*   depending on the variable HALF. */
    /* Parameter adjustments */
    --vwin;
    --buflim;
    lpbuf_offset = buflim[3];
    lpbuf -= lpbuf_offset;
    inbuf_offset = buflim[1];
    inbuf -= inbuf_offset;

    /* Function Body */
    lp_rms__ = 0.f;
    ap_rms__ = 0.f;
    e_pre__ = 0.f;
    e0ap = 0.f;
    *rc1 = 0.f;
    e_0__ = 0.f;
    e_b__ = 0.f;
    e_f__ = 0.f;
    r_f__ = 0.f;
    r_b__ = 0.f;
    *zc = 0;
    vlen = vwin[2] - vwin[1] + 1;
    start = vwin[1] + (*half - 1) * vlen / 2 + 1;
    stop = start + vlen / 2 - 1;

/* I'll use the symbol HVL in the table below to represent the value */
/* VLEN/2.  Note that if VLEN is odd, then HVL should be rounded down, */
/* i.e., HVL = (VLEN-1)/2. */

/* HALF  START          STOP */

/* 1     VWIN(1)+1      VWIN(1)+HVL */
/* 2     VWIN(1)+HVL+1  VWIN(1)+2*HVL */

/* Note that if VLEN is even and HALF is 2, then STOP will be */
/* VWIN(1)+VLEN = VWIN(2)+1.  That could be bad, if that index of INBUF */
/* is undefined. */

    r__1 = inbuf[start - 1] - *dither;
    oldsgn = (real)r_sign(&c_b2, &r__1);
    i__1 = stop;
    for (i__ = start; i__ <= i__1; ++i__) {
	lp_rms__ += (r__1 = lpbuf[i__], abs(r__1));
	ap_rms__ += (r__1 = inbuf[i__], abs(r__1));
	e_pre__ += (r__1 = inbuf[i__] - inbuf[i__ - 1], abs(r__1));
/* Computing 2nd power */
	r__1 = inbuf[i__];
	e0ap += r__1 * r__1;
	*rc1 += inbuf[i__] * inbuf[i__ - 1];
/* Computing 2nd power */
	r__1 = lpbuf[i__];
	e_0__ += r__1 * r__1;
/* Computing 2nd power */
	r__1 = lpbuf[i__ - *mintau];
	e_b__ += r__1 * r__1;
/* Computing 2nd power */
	r__1 = lpbuf[i__ + *mintau];
	e_f__ += r__1 * r__1;
	r_f__ += lpbuf[i__] * lpbuf[i__ + *mintau];
	r_b__ += lpbuf[i__] * lpbuf[i__ - *mintau];
	r__1 = inbuf[i__] + *dither;
	if (r_sign(&c_b2, &r__1) != oldsgn) {
	    ++(*zc);
	    oldsgn = -oldsgn;
	}
	*dither = -(*dither);
    }
/*   Normalized short-term autocovariance coefficient at unit sample delay
 */
    *rc1 /= max(e0ap,1.f);
/*  Ratio of the energy of the first difference signal (6 dB/oct preemphas
is)*/
/*   to the energy of the full band signal */
/* Computing MAX */
    r__1 = ap_rms__ * 2.f;
    *qs = e_pre__ / max(r__1,1.f);
/*   aR_b is the product of the forward and reverse prediction gains, */
/*   looking backward in time (the causal case). */
    *ar_b__ = r_b__ / max(e_b__,1.f) * (r_b__ / max(e_0__,1.f));
/*  aR_f is the same as aR_b, but looking forward in time (non causal case
).*/
    *ar_f__ = r_f__ / max(e_f__,1.f) * (r_f__ / max(e_0__,1.f));
/*   Normalize ZC, LBE, and FBE to old fixed window length of 180. */
/*   (The fraction 90/VLEN has a range of .58 to 1) */
    r__2 = (real) (*zc << 1);
    r__1 = r__2 * (90.f / vlen);
    *zc = i_nint(&r__1);
/* Computing MIN */
    r__1 = lp_rms__ / 4 * (90.f / vlen);
    i__1 = i_nint(&r__1);
    *lbe = min(i__1,32767);
/* Computing MIN */
    r__1 = ap_rms__ / 4 * (90.f / vlen);
    i__1 = i_nint(&r__1);
    *fbe = min(i__1,32767);
    return 0;
} /* vparms_ */

