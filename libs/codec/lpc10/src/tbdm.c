/*

$Log: tbdm.c,v $
Revision 1.1  2004/05/04 11:16:43  csoutheren
Initial version

Revision 1.2  2002/02/15 03:57:55  yurik
Warnings removed during compilation, patch courtesy of Jehan Bing, jehan@bravobrava.com

Revision 1.1  2000/06/05 04:45:12  robertj
Added LPC-10 2400bps codec

 * Revision 1.1  1996/08/19  22:30:26  jaf
 * Initial revision
 *

*/

#ifdef P_R_O_T_O_T_Y_P_E_S
extern int tbdm_(real *speech, integer *lpita, integer *tau, integer *ltau, real *amdf, integer *minptr, integer *maxptr, integer *mintau);
/*:ref: difmag_ 14 8 6 4 4 4 4 6 4 4 */
#endif

/*  -- translated by f2c (version 19951025).
   You must link the resulting object file with the libraries:
	-lf2c -lm   (in that order)
*/

#include "f2c.h"

/* ********************************************************************** */

/* 	TBDM Version 49 */

/* $Log: tbdm.c,v $
 * Revision 1.1  2004/05/04 11:16:43  csoutheren
 * Initial version
 *
 * Revision 1.2  2002/02/15 03:57:55  yurik
 * Warnings removed during compilation, patch courtesy of Jehan Bing, jehan@bravobrava.com
 *
 * Revision 1.1  2000/06/05 04:45:12  robertj
 * Added LPC-10 2400bps codec
 *
 * Revision 1.1  1996/08/19  22:30:26  jaf
 * Initial revision
 *
 */
/* Revision 1.3  1996/03/18  22:14:00  jaf */
/* Just added a few comments about which array indices of the arguments */
/* are used, and mentioning that this subroutine has no local state. */

/* Revision 1.2  1996/03/13  14:48:37  jaf */
/* Comments added explaining that none of the local variables of this */
/* subroutine need to be saved from one invocation to the next. */

/* Revision 1.1  1996/02/07 14:49:54  jaf */
/* Initial revision */


/* ********************************************************************* */

/*TURBO DIFMAG: Compute High Resolution Average Magnitude Difference Function
*/

/* Note: There are several constants in here that appear to depend on a */
/* particular TAU table.  That's not a problem for the LPC10 coder, but */
/* watch out if you change the contents of TAU in the subroutine ANALYS. */

/* Input: */
/*  SPEECH - Low pass filtered speech */
/*           Indices 1 through MAX+LPITA-1 are read, where: */
/*           MAX = (TAU(LTAU)-TAU(1))/2+1 */
/*           (If TAU(1) .LT. 39, then larger indices could be read */
/*           by the last call to DIFMAG below.) */
/*  LPITA  - Length of speech buffer */
/*  TAU    - Table of lags, sorted in increasing order. */
/*           Indices 1 through LTAU read. */
/*  LTAU   - Number of lag values to compute */
/* Output: */
/*  AMDF   - Average Magnitude Difference for each lag in TAU */
/*          Indices 1 through LTAU written, and several might then be read.*/
/*  MINPTR - Index of minimum AMDF value */
/*  MAXPTR - Index of maximum AMDF value within +/- 1/2 octave of min */
/*  MINTAU - Lag corresponding to minimum AMDF value */

/* This subroutine has no local state. */

/* Subroutine */ int tbdm_(real *speech, integer *lpita, integer *tau, 
	integer *ltau, real *amdf, integer *minptr, integer *maxptr, integer *
	mintau)
{
    /* System generated locals */
    integer i__1, i__2, i__3, i__4;

    /* Local variables */
    real amdf2[6];
    integer minp2, ltau2, maxp2, i__;
    extern /* Subroutine */ int difmag_(real *, integer *, integer *, integer 
	    *, integer *, real *, integer *, integer *);
    integer minamd, ptr, tau2[6];

/* 	Arguments */
/* 	REAL SPEECH(LPITA+TAU(LTAU)), AMDF(LTAU) */
/*   Stupid TOAST doesn't understand expressions */
/*       Local variables that need not be saved */
/*       Local state */
/*       None */
/*   Compute full AMDF using log spaced lags, find coarse minimum */
    /* Parameter adjustments */
    --speech;
    --amdf;
    --tau;

    /* Function Body */
    difmag_(&speech[1], lpita, &tau[1], ltau, &tau[*ltau], &amdf[1], minptr, 
	    maxptr);
    *mintau = tau[*minptr];
    minamd = (integer)amdf[*minptr];
/*   Build table containing all lags within +/- 3 of the AMDF minimum */
/*    excluding all that have already been computed */
    ltau2 = 0;
    ptr = *minptr - 2;
/* Computing MAX */
    i__1 = *mintau - 3;
/* Computing MIN */
    i__3 = *mintau + 3, i__4 = tau[*ltau] - 1;
    i__2 = min(i__3,i__4);
    for (i__ = max(i__1,41); i__ <= i__2; ++i__) {
	while(tau[ptr] < i__) {
	    ++ptr;
	}
	if (tau[ptr] != i__) {
	    ++ltau2;
	    tau2[ltau2 - 1] = i__;
	}
    }
/*   Compute AMDF of the new lags, if there are any, and choose one */
/*    if it is better than the coarse minimum */
    if (ltau2 > 0) {
	difmag_(&speech[1], lpita, tau2, &ltau2, &tau[*ltau], amdf2, &minp2, &
		maxp2);
	if (amdf2[minp2 - 1] < (real) minamd) {
	    *mintau = tau2[minp2 - 1];
	    minamd = (integer)amdf2[minp2 - 1];
	}
    }
/*   Check one octave up, if there are any lags not yet computed */
    if (*mintau >= 80) {
	i__ = *mintau / 2;
	if ((i__ & 1) == 0) {
	    ltau2 = 2;
	    tau2[0] = i__ - 1;
	    tau2[1] = i__ + 1;
	} else {
	    ltau2 = 1;
	    tau2[0] = i__;
	}
	difmag_(&speech[1], lpita, tau2, &ltau2, &tau[*ltau], amdf2, &minp2, &
		maxp2);
	if (amdf2[minp2 - 1] < (real) minamd) {
	    *mintau = tau2[minp2 - 1];
	    minamd = (integer)amdf2[minp2 - 1];
	    *minptr += -20;
	}
    }
/*   Force minimum of the AMDF array to the high resolution minimum */
    amdf[*minptr] = (real) minamd;
/*   Find maximum of AMDF within 1/2 octave of minimum */
/* Computing MAX */
    i__2 = *minptr - 5;
    *maxptr = max(i__2,1);
/* Computing MIN */
    i__1 = *minptr + 5;
    i__2 = min(i__1,*ltau);
    for (i__ = *maxptr + 1; i__ <= i__2; ++i__) {
	if (amdf[i__] > amdf[*maxptr]) {
	    *maxptr = i__;
	}
    }
    return 0;
} /* tbdm_ */

