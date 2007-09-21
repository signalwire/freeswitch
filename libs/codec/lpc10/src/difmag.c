/*

$Log: difmag.c,v $
Revision 1.1  2004/05/04 11:16:42  csoutheren
Initial version

Revision 1.1  2000/06/05 04:45:12  robertj
Added LPC-10 2400bps codec

 * Revision 1.1  1996/08/19  22:32:31  jaf
 * Initial revision
 *

*/

#ifdef P_R_O_T_O_T_Y_P_E_S
extern int difmag_(real *speech, integer *lpita, integer *tau, integer *ltau, integer *maxlag, real *amdf, integer *minptr, integer *maxptr);
#endif

/*  -- translated by f2c (version 19951025).
   You must link the resulting object file with the libraries:
	-lf2c -lm   (in that order)
*/

#include "f2c.h"

/* ********************************************************************** */

/* 	DIFMAG Version 49 */

/* $Log: difmag.c,v $
 * Revision 1.1  2004/05/04 11:16:42  csoutheren
 * Initial version
 *
 * Revision 1.1  2000/06/05 04:45:12  robertj
 * Added LPC-10 2400bps codec
 *
 * Revision 1.1  1996/08/19  22:32:31  jaf
 * Initial revision
 *
 */
/* Revision 1.3  1996/03/15  23:09:39  jaf */
/* Just added a few comments about which array indices of the arguments */
/* are used, and mentioning that this subroutine has no local state. */

/* Revision 1.2  1996/03/13  14:41:31  jaf */
/* Comments added explaining that none of the local variables of this */
/* subroutine need to be saved from one invocation to the next. */

/* Revision 1.1  1996/02/07 14:45:04  jaf */
/* Initial revision */


/* ********************************************************************* */

/*  Compute Average Magnitude Difference Function */

/* Inputs: */
/*  SPEECH - Low pass filtered speech */
/*           Indices MIN_N1 through MAX_N1+LPITA-1 are read, where */
/*      MIN_N1 = (MAXLAG - MAX_TAU)/2+1  MAX_TAU = max of TAU(I) for I=1,LTAU
*/
/*      MAX_N1 = (MAXLAG - MIN_TAU)/2+1  MIN_TAU = min of TAU(I) for I=1,LTAU
*/
/*  LPITA  - Length of speech buffer */
/*  TAU    - Table of lags */
/*           Indices 1 through LTAU read. */
/*  LTAU   - Number of lag values to compute */
/*  MAXLAG - Maximum possible lag value */
/* Outputs: */
/*  (All of these outputs are also read, but only after being written.) */
/*  AMDF   - Average Magnitude Difference for each lag in TAU */
/*           Indices 1 through LTAU written */
/*  MINPTR - Index of minimum AMDF value */
/*  MAXPTR - Index of maximum AMDF value */

/* This subroutine has no local state. */

/* Subroutine */ int difmag_(real *speech, integer *lpita, integer *tau, 
	integer *ltau, integer *maxlag, real *amdf, integer *minptr, integer *
	maxptr)
{
    /* System generated locals */
    integer i__1, i__2;
    real r__1;

    /* Local variables */
    integer i__, j, n1, n2;
    real sum;

/*       Arguments */
/*       Local variables that need not be saved */
/*       Local state */
/*       None */
    /* Parameter adjustments */
    --amdf;
    --tau;
    --speech;

    /* Function Body */
    *minptr = 1;
    *maxptr = 1;
    i__1 = *ltau;
    for (i__ = 1; i__ <= i__1; ++i__) {
	n1 = (*maxlag - tau[i__]) / 2 + 1;
	n2 = n1 + *lpita - 1;
	sum = 0.f;
	i__2 = n2;
	for (j = n1; j <= i__2; j += 4) {
	    sum += (r__1 = speech[j] - speech[j + tau[i__]], abs(r__1));
	}
	amdf[i__] = sum;
	if (amdf[i__] < amdf[*minptr]) {
	    *minptr = i__;
	}
	if (amdf[i__] > amdf[*maxptr]) {
	    *maxptr = i__;
	}
    }
    return 0;
} /* difmag_ */

