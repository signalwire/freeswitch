/*

$Log: mload.c,v $
Revision 1.1  2004/05/04 11:16:43  csoutheren
Initial version

Revision 1.1  2000/06/05 04:45:12  robertj
Added LPC-10 2400bps codec

 * Revision 1.1  1996/08/19  22:31:25  jaf
 * Initial revision
 *

*/

#ifdef P_R_O_T_O_T_Y_P_E_S
extern int mload_(integer *order, integer *awins, integer *awinf, real *speech, real *phi, real *psi);
#endif

/*  -- translated by f2c (version 19951025).
   You must link the resulting object file with the libraries:
	-lf2c -lm   (in that order)
*/

#include "f2c.h"

/* ***************************************************************** */

/* 	MLOAD Version 48 */

/* $Log: mload.c,v $
/* Revision 1.1  2004/05/04 11:16:43  csoutheren
/* Initial version
/*
/* Revision 1.1  2000/06/05 04:45:12  robertj
/* Added LPC-10 2400bps codec
/*
 * Revision 1.1  1996/08/19  22:31:25  jaf
 * Initial revision
 * */
/* Revision 1.5  1996/03/27  23:59:51  jaf */
/* Added some more accurate comments about which indices of the argument */
/* array SPEECH are read.  I thought that this might be the cause of a */
/* problem I've been having, but it isn't. */

/* Revision 1.4  1996/03/26  19:16:53  jaf */
/* Commented out the code at the end that copied the lower triangular */
/* half of PHI into the upper triangular half (making the resulting */
/* matrix symmetric).  The upper triangular half was never used by later */
/* code in subroutine ANALYS. */

/* Revision 1.3  1996/03/18  21:16:00  jaf */
/* Just added a few comments about which array indices of the arguments */
/* are used, and mentioning that this subroutine has no local state. */

/* Revision 1.2  1996/03/13  16:47:41  jaf */
/* Comments added explaining that none of the local variables of this */
/* subroutine need to be saved from one invocation to the next. */

/* Revision 1.1  1996/02/07 14:48:01  jaf */
/* Initial revision */


/* ***************************************************************** */

/* Load a covariance matrix. */

/* Input: */
/*  ORDER            - Analysis order */
/*  AWINS            - Analysis window start */
/*  AWINF            - Analysis window finish */
/*  SPEECH(AWINF)    - Speech buffer */
/*                     Indices MIN(AWINS, AWINF-(ORDER-1)) through */
/*                             MAX(AWINF, AWINS+(ORDER-1)) read. */
/*                     As long as (AWINF-AWINS) .GE. (ORDER-1), */
/*                     this is just indices AWINS through AWINF. */
/* Output: */
/*  PHI(ORDER,ORDER) - Covariance matrix */
/*                    Lower triangular half and diagonal written, and read.*/
/*                     Upper triangular half untouched. */
/*  PSI(ORDER)       - Prediction vector */
/*                     Indices 1 through ORDER written, */
/*                     and most are read after that. */

/* This subroutine has no local state. */

/* Subroutine */ int mload_(integer *order, integer *awins, integer *awinf, 
	real *speech, real *phi, real *psi)
{
    /* System generated locals */
    integer phi_dim1, phi_offset, i__1, i__2;

    /* Local variables */
    integer c__, i__, r__, start;

/*       Arguments */
/*       Local variables that need not be saved */
/*   Load first column of triangular covariance matrix PHI */
    /* Parameter adjustments */
    --psi;
    phi_dim1 = *order;
    phi_offset = phi_dim1 + 1;
    phi -= phi_offset;
    --speech;

    /* Function Body */
    start = *awins + *order;
    i__1 = *order;
    for (r__ = 1; r__ <= i__1; ++r__) {
	phi[r__ + phi_dim1] = 0.f;
	i__2 = *awinf;
	for (i__ = start; i__ <= i__2; ++i__) {
	    phi[r__ + phi_dim1] += speech[i__ - 1] * speech[i__ - r__];
	}
    }
/*   Load last element of vector PSI */
    psi[*order] = 0.f;
    i__1 = *awinf;
    for (i__ = start; i__ <= i__1; ++i__) {
	psi[*order] += speech[i__] * speech[i__ - *order];
    }
/*   End correct to get additional columns of PHI */
    i__1 = *order;
    for (r__ = 2; r__ <= i__1; ++r__) {
	i__2 = r__;
	for (c__ = 2; c__ <= i__2; ++c__) {
	    phi[r__ + c__ * phi_dim1] = phi[r__ - 1 + (c__ - 1) * phi_dim1] - 
		    speech[*awinf + 1 - r__] * speech[*awinf + 1 - c__] + 
		    speech[start - r__] * speech[start - c__];
	}
    }
/*   End correct to get additional elements of PSI */
    i__1 = *order - 1;
    for (c__ = 1; c__ <= i__1; ++c__) {
	psi[c__] = phi[c__ + 1 + phi_dim1] - speech[start - 1] * speech[start 
		- 1 - c__] + speech[*awinf] * speech[*awinf - c__];
    }
/*   Copy lower triangular section into upper (why bother?) */
/*       I'm commenting this out, since the upper triangular half of PHI 
*/
/*       is never used by later code, unless a sufficiently high level of 
*/
/*       tracing is turned on. */
/* 	DO R = 1,ORDER */
/* 	   DO C = 1,R-1 */
/* 	      PHI(C,R) = PHI(R,C) */
/* 	   END DO */
/* 	END DO */
    return 0;
} /* mload_ */

