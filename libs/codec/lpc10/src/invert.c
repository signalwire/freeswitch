/*

$Log: invert.c,v $
Revision 1.1  2004/05/04 11:16:42  csoutheren
Initial version

Revision 1.1  2000/06/05 04:45:12  robertj
Added LPC-10 2400bps codec

 * Revision 1.1  1996/08/19  22:32:00  jaf
 * Initial revision
 *

*/

#ifdef P_R_O_T_O_T_Y_P_E_S
extern int invert_(integer *order, real *phi, real *psi, real *rc);
#endif

/*  -- translated by f2c (version 19951025).
   You must link the resulting object file with the libraries:
	-lf2c -lm   (in that order)
*/

#include "f2c.h"

/* **************************************************************** */

/* 	INVERT Version 45G */

/* $Log: invert.c,v $
 * Revision 1.1  2004/05/04 11:16:42  csoutheren
 * Initial version
 *
 * Revision 1.1  2000/06/05 04:45:12  robertj
 * Added LPC-10 2400bps codec
 *
 * Revision 1.1  1996/08/19  22:32:00  jaf
 * Initial revision
 *
 */
/* Revision 1.3  1996/03/18  20:52:47  jaf */
/* Just added a few comments about which array indices of the arguments */
/* are used, and mentioning that this subroutine has no local state. */

/* Revision 1.2  1996/03/13  16:51:32  jaf */
/* Comments added explaining that none of the local variables of this */
/* subroutine need to be saved from one invocation to the next. */

/* Eliminated a comment from the original, describing a local array X */
/* that appeared nowhere in the code. */

/* Revision 1.1  1996/02/07 14:47:20  jaf */
/* Initial revision */


/* **************************************************************** */

/*  Invert a covariance matrix using Choleski decomposition method. */

/* Input: */
/*  ORDER            - Analysis order */
/*  PHI(ORDER,ORDER) - Covariance matrix */
/*                    Indices (I,J) read, where ORDER .GE. I .GE. J .GE. 1.*/
/*                     All other indices untouched. */
/*  PSI(ORDER)       - Column vector to be predicted */
/*                     Indices 1 through ORDER read. */
/* Output: */
/*  RC(ORDER)        - Pseudo reflection coefficients */
/*                    Indices 1 through ORDER written, and then possibly read.
*/
/* Internal: */
/*  V(ORDER,ORDER)   - Temporary matrix */
/*                     Same indices written as read from PHI. */
/*                     Many indices may be read and written again after */
/*                     initially being copied from PHI, but all indices */
/*                     are written before being read. */

/*  NOTE: Temporary matrix V is not needed and may be replaced */
/*    by PHI if the original PHI values do not need to be preserved. */

/* Subroutine */ int invert_(integer *order, real *phi, real *psi, real *rc)
{
    /* System generated locals */
    integer phi_dim1, phi_offset, i__1, i__2, i__3;
    real r__1, r__2;

    /* Local variables */
    real save;
    integer i__, j, k;
    real v[100]	/* was [10][10] */;

/*       Arguments */
/* $Log: invert.c,v $
 * Revision 1.1  2004/05/04 11:16:42  csoutheren
 * Initial version
 *
 * Revision 1.1  2000/06/05 04:45:12  robertj
 * Added LPC-10 2400bps codec
 *
 * Revision 1.1  1996/08/19  22:32:00  jaf
 * Initial revision
 *
 */
/* Revision 1.3  1996/03/29  22:03:47  jaf */
/* Removed definitions for any constants that were no longer used. */

/* Revision 1.2  1996/03/26  19:34:33  jaf */
/* Added comments indicating which constants are not needed in an */
/* application that uses the LPC-10 coder. */

/* Revision 1.1  1996/02/07  14:43:51  jaf */
/* Initial revision */

/*   LPC Configuration parameters: */
/* Frame size, Prediction order, Pitch period */
/* 	Parameters/constants */
/*       Local variables that need not be saved */
/*  Decompose PHI into V * D * V' where V is a triangular matrix whose */
/*   main diagonal elements are all 1, V' is the transpose of V, and */
/*   D is a vector.  Here D(n) is stored in location V(n,n). */
    /* Parameter adjustments */
    --rc;
    --psi;
    phi_dim1 = *order;
    phi_offset = phi_dim1 + 1;
    phi -= phi_offset;

    /* Function Body */
    i__1 = *order;
    for (j = 1; j <= i__1; ++j) {
	i__2 = *order;
	for (i__ = j; i__ <= i__2; ++i__) {
	    v[i__ + j * 10 - 11] = phi[i__ + j * phi_dim1];
	}
	i__2 = j - 1;
	for (k = 1; k <= i__2; ++k) {
	    save = v[j + k * 10 - 11] * v[k + k * 10 - 11];
	    i__3 = *order;
	    for (i__ = j; i__ <= i__3; ++i__) {
		v[i__ + j * 10 - 11] -= v[i__ + k * 10 - 11] * save;
	    }
	}
/*  Compute intermediate results, which are similar to RC's */
	if ((r__1 = v[j + j * 10 - 11], abs(r__1)) < 1e-10f) {
	    goto L100;
	}
	rc[j] = psi[j];
	i__2 = j - 1;
	for (k = 1; k <= i__2; ++k) {
	    rc[j] -= rc[k] * v[j + k * 10 - 11];
	}
	v[j + j * 10 - 11] = 1.f / v[j + j * 10 - 11];
	rc[j] *= v[j + j * 10 - 11];
/* Computing MAX */
/* Computing MIN */
	r__2 = rc[j];
	r__1 = min(r__2,.999f);
	rc[j] = max(r__1,-.999f);
    }
    return 0;
/*  Zero out higher order RC's if algorithm terminated early */
L100:
    i__1 = *order;
    for (i__ = j; i__ <= i__1; ++i__) {
	rc[i__] = 0.f;
    }
/*  Back substitute for PC's (if needed) */
/* 110	DO J = ORDER,1,-1 */
/* 	   PC(J) = RC(J) */
/* 	   DO I = 1,J-1 */
/* 	      PC(J) = PC(J) - PC(I)*V(J,I) */
/* 	   END DO */
/* 	END DO */
    return 0;
} /* invert_ */

