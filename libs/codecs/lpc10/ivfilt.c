/*

$Log$
Revision 1.16  2004/06/26 03:50:14  markster
Merge source cleanups (bug #1911)

Revision 1.15  2003/09/19 01:20:22  markster
Code cleanups (bug #66)

Revision 1.2  2003/09/19 01:20:22  markster
Code cleanups (bug #66)

Revision 1.1.1.1  2003/02/12 13:59:15  matteo
mer feb 12 14:56:57 CET 2003

Revision 1.2  2000/01/05 08:20:39  markster
Some OSS fixes and a few lpc changes to make it actually work

 * Revision 1.1  1996/08/19  22:31:53  jaf
 * Initial revision
 *

*/

/*  -- translated by f2c (version 19951025).
   You must link the resulting object file with the libraries:
	-lf2c -lm   (in that order)
*/

#include "f2c.h"

#ifdef P_R_O_T_O_T_Y_P_E_S
extern int ivfilt_(real *lpbuf, real *ivbuf, integer *len, integer *nsamp, real *ivrc);
#endif

/* ********************************************************************* */

/* 	IVFILT Version 48 */

/* $Log$
 * Revision 1.16  2004/06/26 03:50:14  markster
 * Merge source cleanups (bug #1911)
 *
/* Revision 1.15  2003/09/19 01:20:22  markster
/* Code cleanups (bug #66)
/*
/* Revision 1.2  2003/09/19 01:20:22  markster
/* Code cleanups (bug #66)
/*
/* Revision 1.1.1.1  2003/02/12 13:59:15  matteo
/* mer feb 12 14:56:57 CET 2003
/*
/* Revision 1.2  2000/01/05 08:20:39  markster
/* Some OSS fixes and a few lpc changes to make it actually work
/*
 * Revision 1.1  1996/08/19  22:31:53  jaf
 * Initial revision
 * */
/* Revision 1.3  1996/03/15  21:36:29  jaf */
/* Just added a few comments about which array indices of the arguments */
/* are used, and mentioning that this subroutine has no local state. */

/* Revision 1.2  1996/03/13  00:01:00  jaf */
/* Comments added explaining that none of the local variables of this */
/* subroutine need to be saved from one invocation to the next. */

/* Revision 1.1  1996/02/07 14:47:34  jaf */
/* Initial revision */


/* ********************************************************************* */

/*   2nd order inverse filter, speech is decimated 4:1 */

/* Input: */
/*  LEN    - Length of speech buffers */
/*  NSAMP  - Number of samples to filter */
/*  LPBUF  - Low pass filtered speech buffer */
/*           Indices LEN-NSAMP-7 through LEN read. */
/* Output: */
/*  IVBUF  - Inverse filtered speech buffer */
/*           Indices LEN-NSAMP+1 through LEN written. */
/*  IVRC   - Inverse filter reflection coefficients (for voicing) */
/*          Indices 1 and 2 both written (also read, but only after writing).
*/

/* This subroutine has no local state. */

/* Subroutine */ int ivfilt_(real *lpbuf, real *ivbuf, integer *len, integer *
	nsamp, real *ivrc)
{
    /* System generated locals */
    integer i__1;

    /* Local variables */
    integer i__, j, k;
    real r__[3], pc1, pc2;

/* 	Arguments */
/*       Local variables that need not be saved */
/*       Local state */
/*       None */
/*  Calculate Autocorrelations */
    /* Parameter adjustments */
    --ivbuf;
    --lpbuf;
    --ivrc;

    /* Function Body */
    for (i__ = 1; i__ <= 3; ++i__) {
	r__[i__ - 1] = 0.f;
	k = (i__ - 1) << 2;
	i__1 = *len;
	for (j = (i__ << 2) + *len - *nsamp; j <= i__1; j += 2) {
	    r__[i__ - 1] += lpbuf[j] * lpbuf[j - k];
	}
    }
/*  Calculate predictor coefficients */
    pc1 = 0.f;
    pc2 = 0.f;
    ivrc[1] = 0.f;
    ivrc[2] = 0.f;
    if (r__[0] > 1e-10f) {
	ivrc[1] = r__[1] / r__[0];
	ivrc[2] = (r__[2] - ivrc[1] * r__[1]) / (r__[0] - ivrc[1] * r__[1]);
	pc1 = ivrc[1] - ivrc[1] * ivrc[2];
	pc2 = ivrc[2];
    }
/*  Inverse filter LPBUF into IVBUF */
    i__1 = *len;
    for (i__ = *len + 1 - *nsamp; i__ <= i__1; ++i__) {
	ivbuf[i__] = lpbuf[i__] - pc1 * lpbuf[i__ - 4] - pc2 * lpbuf[i__ - 8];
    }
    return 0;
} /* ivfilt_ */

