/*

$Log: onset.c,v $
Revision 1.1  2004/05/04 11:16:43  csoutheren
Initial version

Revision 1.2  2002/02/15 03:57:55  yurik
Warnings removed during compilation, patch courtesy of Jehan Bing, jehan@bravobrava.com

Revision 1.1  2000/06/05 04:45:12  robertj
Added LPC-10 2400bps codec

 * Revision 1.2  1996/08/20  20:37:55  jaf
 * Removed all static local variables that were SAVE'd in the Fortran
 * code, and put them in struct lpc10_encoder_state that is passed as an
 * argument.
 *
 * Removed init function, since all initialization is now done in
 * init_lpc10_encoder_state().
 *
 * Revision 1.1  1996/08/19  22:31:18  jaf
 * Initial revision
 *

*/

#ifdef P_R_O_T_O_T_Y_P_E_S
extern int onset_(real *pebuf, integer *osbuf, integer *osptr, integer *oslen, integer *sbufl, integer *sbufh, integer *lframe, struct lpc10_encoder_state *st);
#endif

/*  -- translated by f2c (version 19951025).
   You must link the resulting object file with the libraries:
	-lf2c -lm   (in that order)
*/

#include "f2c.h"

/* Table of constant values */

static real c_b2 = 1.f;

/* ****************************************************************** */

/* 	ONSET Version 49 */

/* $Log: onset.c,v $
 * Revision 1.1  2004/05/04 11:16:43  csoutheren
 * Initial version
 *
 * Revision 1.2  2002/02/15 03:57:55  yurik
 * Warnings removed during compilation, patch courtesy of Jehan Bing, jehan@bravobrava.com
 *
 * Revision 1.1  2000/06/05 04:45:12  robertj
 * Added LPC-10 2400bps codec
 *
 * Revision 1.2  1996/08/20  20:37:55  jaf
 * Removed all static local variables that were SAVE'd in the Fortran
 * code, and put them in struct lpc10_encoder_state that is passed as an
 * argument.
 *
 * Removed init function, since all initialization is now done in
 * init_lpc10_encoder_state().
 *
 * Revision 1.1  1996/08/19  22:31:18  jaf
 * Initial revision
 *
 */
/* Revision 1.5  1996/03/15  16:41:01  jaf */
/* Just rearranged INITONSET assignment orders to be consistent with */
/* order of DATA statements in ONSET. */

/* Revision 1.4  1996/03/15  15:48:27  jaf */
/* Changed some comments, and only reordered the DATA statements (their */
/* meaning wasn't changed). */

/* Revision 1.3  1996/03/14  23:53:06  jaf */
/* Added an entry INITONSET that reinitializes the local state variables */
/* of subroutine ONSET. */

/* Rearranged quite a few comments, adding more explaining which */
/* arguments were inputs, and how the modified ones can be changed. */

/* Revision 1.2  1996/03/12  23:53:00  jaf */
/* Lots of comments added about the local state of this subroutine that */
/* must be saved from one invocation to the next. */

/* One constant 180 replaced with LFRAME, which should be "more general", */
/* even though it would probably require many more changes than this to */
/* get this coder to work for other frame sizes. */

/* Revision 1.1  1996/02/07 14:48:09  jaf */
/* Initial revision */


/* ****************************************************************** */

/* 	Floating point version */


/*   Detection of onsets in (or slightly preceding) the futuremost frame */
/*   of speech. */


/* Input: */
/*  PEBUF(SBUFL:SBUFH)  - Preemphasized speech */
/*                        Indices SBUFH-LFRAME through SBUFH are read. */
/*  OSLEN  - Maximum number of onsets that can be stored in OSBUF. */
/*  SBUFL, SBUFH        - Range of PEBUF */
/*  LFRAME              - length of a frame, in samples */
/* Input/Output: */
/*  OSBUF(OSLEN) - Buffer which holds sorted indexes of onsets */
/*                 Indices A through B are modified, where A */
/*                 is the original value of OSPTR, and B is the final */
/*                 value of OSPTR-1.  B is at most OSLEN. */
/*  OSPTR        - Free pointer into OSBUF */
/*                 Initial value should be .LE. OSLEN+1. */
/*                 If so, final value grows by one for each new onset */
/*                 found, and final value will be .LE. OSLEN+1. */

/* This subroutine maintains local state from one call to the next.  If */
/* you want to switch to using a new audio stream for this subroutine, or */
/* reinitialize its state for any other reason, call the ENTRY INITONSET. */

/* Subroutine */ int onset_(real *pebuf, integer *osbuf, integer *
	osptr, integer *oslen, integer *sbufl, integer *sbufh, integer *
	lframe, struct lpc10_encoder_state *st)
{
    /* Initialized data */

    real *n;
    real *d__;
    real *l2buf;
    real *l2sum1;
    integer *l2ptr1;
    integer *l2ptr2;
    logical *hyst;

    /* System generated locals */
    integer pebuf_offset, i__1;
    real r__1;

    /* Builtin functions */
    double r_sign(real *, real *);

    /* Local variables */
    integer i__;
    integer *lasti;
    real l2sum2;
    real *fpc;

/*       Arguments */
/* $Log: onset.c,v $
 * Revision 1.1  2004/05/04 11:16:43  csoutheren
 * Initial version
 *
 * Revision 1.2  2002/02/15 03:57:55  yurik
 * Warnings removed during compilation, patch courtesy of Jehan Bing, jehan@bravobrava.com
 *
 * Revision 1.1  2000/06/05 04:45:12  robertj
 * Added LPC-10 2400bps codec
 *
 * Revision 1.2  1996/08/20  20:37:55  jaf
 * Removed all static local variables that were SAVE'd in the Fortran
 * code, and put them in struct lpc10_encoder_state that is passed as an
 * argument.
 *
 * Removed init function, since all initialization is now done in
 * init_lpc10_encoder_state().
 *
 * Revision 1.1  1996/08/19  22:31:18  jaf
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
/*       Parameters/constants */
/*   Parameters for onset detection algorithm: */
/*    L2		Threshold for filtered slope of FPC (function of L2WID!) */
/*    L2LAG	Lag due to both filters which compute filtered slope of FPC */
/*    L2WID	Width of the filter which computes the slope of FPC */
/*    OSHYST	The number of samples of slope(FPC) which must be below */
/* 	        the threshold before a new onset may be declared. */
/*       Local variables that need not be saved */
/*       Local state */
/*   Variables */
/*    N, D       Numerator and denominator of prediction filters */
/*    FPC        Current prediction coefs */
/*    L2BUF, L2SUM1, L2SUM2    State of slope filter */
/*       The only "significant" change I've made is to change L2SUM2 out 
*/
/*       of the list of local variables that need to be saved, since it */
/*       didn't need to be. */
/*       L2SUM1 need not be, but avoiding saving it would require a small 
*/
/*       change to the body of the code.  See comments below for an */
/*       example of how the code could be changed to avoid saving L2SUM1. 
*/
/*       FPC and LASTI are saved from one invocation to the next, but */
/*       they are not given initial values.  This is acceptable, because 
*/
/*       FPC will be assigned a value the first time that this function */
/*       is called after D is initialized to 1, since the formula to */
/*       change D will not change it to 0 in one step, and the IF (D */
/*       .NE. 0) statement will execute its THEN part, initializing FPC. 
*/

/*       LASTI's value will not be used until HYST is .TRUE., and */
/*       whenever HYST is changed from its initial value of .FALSE., */
/*       LASTI is assigned a value. */
/*       In a C version of this coder, it would be nice if all of these */
/*       saved things, in this and all other subroutines, could be stored 
*/
/*       in a single struct lpc10_coder_state_t, initialized with a call 
*/
/*       to a function like lpc10_init(&lpc10_coder_state).  In this way, 
*/
/*       a program that used these functions could conveniently alternate 
*/
/*       coding more than one distinct audio stream. */

    n = &(st->n);
    d__ = &(st->d__);
    fpc = &(st->fpc);
    l2buf = &(st->l2buf[0]);
    l2sum1 = &(st->l2sum1);
    l2ptr1 = &(st->l2ptr1);
    l2ptr2 = &(st->l2ptr2);
    lasti = &(st->lasti);
    hyst = &(st->hyst);

    /* Parameter adjustments */
    if (osbuf) {
	--osbuf;
	}
    if (pebuf) {
	pebuf_offset = *sbufl;
	pebuf -= pebuf_offset;
	}

    /* Function Body */

/*       The following line subtracted a hard-coded "180" from LASTI, */
/*       instead of using a variable like LFRAME or a constant like */
/*       MAXFRM.  I changed it to LFRAME, for "generality". */
    if (*hyst) {
	*lasti -= *lframe;
    }
    i__1 = *sbufh;
    for (i__ = *sbufh - *lframe + 1; i__ <= i__1; ++i__) {
/*   Compute FPC; Use old FPC on divide by zero; Clamp FPC to +/- 1. 
*/
	*n = (pebuf[i__] * pebuf[i__ - 1] + (*n) * 63.f) / 64.f;
/* Computing 2nd power */
	r__1 = pebuf[i__ - 1];
	*d__ = (r__1 * r__1 + (*d__) * 63.f) / 64.f;
	if ((*d__) != 0.f) {
	    if (abs(*n) > (*d__)) {
		*fpc = (real)r_sign(&c_b2, n);
	    } else {
		*fpc = (*n) / (*d__);
	    }
	}
/*   Filter FPC */
/*       In order to allow L2SUM1 not to be saved from one invocation 
of */
/*       this subroutine to the next, one could change the sequence of
 */
/*       assignments below, up to the IF statement, to the following. 
 In */
/*       addition, the initial value of L2PTR2 should be changed to */
/*       L2WID/2 instead of L2WID/2+1. */

/*       L2SUM1 = L2BUF(L2PTR2) */
/*       L2PTR2 = MOD(L2PTR2,L2WID)+1 */
/*       L2SUM1 = L2SUM1 - L2BUF(L2PTR2) + FPC */
/*       L2BUF(L2PTR2) = L2SUM1 */

/* *       The following lines didn't change from the original: */
/*       L2SUM2 = L2BUF(L2PTR1) */
/*       L2BUF(L2PTR1) = FPC */
/*       L2PTR1 = MOD(L2PTR1,L2WID)+1 */

	l2sum2 = l2buf[*l2ptr1 - 1];
	*l2sum1 = *l2sum1 - l2buf[*l2ptr2 - 1] + *fpc;
	l2buf[*l2ptr2 - 1] = *l2sum1;
	l2buf[*l2ptr1 - 1] = *fpc;
	*l2ptr1 = *l2ptr1 % 16 + 1;
	*l2ptr2 = *l2ptr2 % 16 + 1;
	if ((r__1 = *l2sum1 - l2sum2, abs(r__1)) > 1.7f) {
	    if (! (*hyst)) {
/*   Ignore if buffer full */
		if (*osptr <= *oslen) {
		    osbuf[*osptr] = i__ - 9;
		    ++(*osptr);
		}
		*hyst = TRUE_;
	    }
	    *lasti = i__;
/*       After one onset detection, at least OSHYST sample times m
ust go */
/*       by before another is allowed to occur. */
	} else if ((*hyst) && i__ - *lasti >= 10) {
	    *hyst = FALSE_;
	}
    }
    return 0;
} /* onset_ */
