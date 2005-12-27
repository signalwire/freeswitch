/*

$Log: dcbias.c,v $
Revision 1.1  2004/05/04 11:16:42  csoutheren
Initial version

Revision 1.1  2000/06/05 04:45:12  robertj
Added LPC-10 2400bps codec

 * Revision 1.1  1996/08/19  22:40:23  jaf
 * Initial revision
 *

*/

#ifdef P_R_O_T_O_T_Y_P_E_S
extern int dcbias_(integer *len, real *speech, real *sigout);
#endif

/*  -- translated by f2c (version 19951025).
   You must link the resulting object file with the libraries:
	-lf2c -lm   (in that order)
*/

#include "f2c.h"

/* ********************************************************************* */

/* 	DCBIAS Version 50 */

/* $Log: dcbias.c,v $
/* Revision 1.1  2004/05/04 11:16:42  csoutheren
/* Initial version
/*
/* Revision 1.1  2000/06/05 04:45:12  robertj
/* Added LPC-10 2400bps codec
/*
 * Revision 1.1  1996/08/19  22:40:23  jaf
 * Initial revision
 * */
/* Revision 1.3  1996/03/18  21:19:22  jaf */
/* Just added a few comments about which array indices of the arguments */
/* are used, and mentioning that this subroutine has no local state. */

/* Revision 1.2  1996/03/13  16:44:53  jaf */
/* Comments added explaining that none of the local variables of this */
/* subroutine need to be saved from one invocation to the next. */

/* Revision 1.1  1996/02/07 14:44:21  jaf */
/* Initial revision */


/* ********************************************************************* */

/* Calculate and remove DC bias from buffer. */

/* Input: */
/*  LEN    - Length of speech buffers */
/*  SPEECH - Input speech buffer */
/*           Indices 1 through LEN read. */
/* Output: */
/*  SIGOUT - Output speech buffer */
/*           Indices 1 through LEN written */

/* This subroutine has no local state. */

/* Subroutine */ int dcbias_(integer *len, real *speech, real *sigout)
{
    /* System generated locals */
    integer i__1;

    /* Local variables */
    real bias;
    integer i__;

/* 	Arguments */
/*       Local variables that need not be saved */
    /* Parameter adjustments */
    --sigout;
    --speech;

    /* Function Body */
    bias = 0.f;
    i__1 = *len;
    for (i__ = 1; i__ <= i__1; ++i__) {
	bias += speech[i__];
    }
    bias /= *len;
    i__1 = *len;
    for (i__ = 1; i__ <= i__1; ++i__) {
	sigout[i__] = speech[i__] - bias;
    }
    return 0;
} /* dcbias_ */

