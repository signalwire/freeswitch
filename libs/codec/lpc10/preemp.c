/*

$Log: preemp.c,v $
Revision 1.1  2004/05/04 11:16:43  csoutheren
Initial version

Revision 1.1  2000/06/05 04:45:12  robertj
Added LPC-10 2400bps codec

 * Revision 1.1  1996/08/19  22:30:58  jaf
 * Initial revision
 *

*/

#ifdef P_R_O_T_O_T_Y_P_E_S
extern int preemp_(real *inbuf, real *pebuf, integer *nsamp, real *coef, real *z__);
#endif

/*  -- translated by f2c (version 19951025).
   You must link the resulting object file with the libraries:
	-lf2c -lm   (in that order)
*/

#include "f2c.h"

/* ******************************************************************* */

/* 	PREEMP Version 55 */

/* $Log: preemp.c,v $
 * Revision 1.1  2004/05/04 11:16:43  csoutheren
 * Initial version
 *
 * Revision 1.1  2000/06/05 04:45:12  robertj
 * Added LPC-10 2400bps codec
 *
 * Revision 1.1  1996/08/19  22:30:58  jaf
 * Initial revision
 *
 */
/* Revision 1.3  1996/03/14  23:16:29  jaf */
/* Just added a few comments about which array indices of the arguments */
/* are used, and mentioning that this subroutine has no local state. */

/* Revision 1.2  1996/03/11  23:23:34  jaf */
/* Added a bunch of comments to an otherwise simple subroutine. */

/* Revision 1.1  1996/02/07 14:48:48  jaf */
/* Initial revision */


/* ******************************************************************* */

/*   Preemphasize speech with a single-zero filter. */
/*  (When coef = .9375, preemphasis is as in LPC43.) */

/* Inputs: */
/*  NSAMP  - Number of samples to filter */
/*  INBUF  - Input speech buffer */
/*           Indices 1 through NSAMP are read. */
/*  COEF   - Preemphasis coefficient */
/* Input/Output: */
/*  Z      - Filter state */
/* Output: */
/*  PEBUF  - Preemphasized speech buffer (can be equal to INBUF) */
/*           Indices 1 through NSAMP are modified. */

/* This subroutine has no local state. */

/* Subroutine */ int preemp_(real *inbuf, real *pebuf, integer *nsamp, real *
	coef, real *z__)
{
    /* System generated locals */
    integer i__1;

    /* Local variables */
    real temp;
    integer i__;

/*       Arguments */
/*       Local variables */

/*       None of these need to have their values saved from one */
/*       invocation to the next. */

/*       Logically, this subroutine computes the output sequence */
/*       pebuf(1:nsamp) defined by: */

/*       pebuf(i) = inbuf(i) - coef * inbuf(i-1) */

/*       where inbuf(0) is defined by the value of z given as input to */
/*       this subroutine. */

/*       What is this filter's frequency response and phase response? */

/*       Why is this filter applied to the speech? */

/*       Could it be more efficient to apply multiple filters */
/*       simultaneously, by combining them into one equivalent filter? */

/*       Are there ever cases when "factoring" one high-order filter into 
*/
/*       multiple smaller-order filter actually reduces the number of */
/*       arithmetic operations needed to perform them? */
/*       When I first read this subroutine, I didn't understand why the */
/*       variable temp was used.  It seemed that the statements in the do 
*/
/*       loop could be replaced with the following: */

/*           pebuf(i) = inbuf(i) - coef * z */
/*           z = inbuf(i) */

/*       The reason for temp is so that even if pebuf and inbuf are the */
/*       same arrays in memory (i.e., they are aliased), then this */
/*       subroutine will still work correctly.  I didn't realize this */
/*       until seeing the comment after PEBUF above that says "(can be */
/*       equal to INBUF)". */
    /* Parameter adjustments */
    --pebuf;
    --inbuf;

    /* Function Body */
    i__1 = *nsamp;
    for (i__ = 1; i__ <= i__1; ++i__) {
	temp = inbuf[i__] - *coef * *z__;
	*z__ = inbuf[i__];
	pebuf[i__] = temp;
/* L10: */
    }
    return 0;
} /* preemp_ */

