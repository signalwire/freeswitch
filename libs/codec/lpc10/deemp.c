/*

$Log: deemp.c,v $
Revision 1.1  2004/05/04 11:16:42  csoutheren
Initial version

Revision 1.1  2000/06/05 04:45:12  robertj
Added LPC-10 2400bps codec

 * Revision 1.2  1996/08/20  20:23:46  jaf
 * Removed all static local variables that were SAVE'd in the Fortran
 * code, and put them in struct lpc10_decoder_state that is passed as an
 * argument.
 *
 * Removed init function, since all initialization is now done in
 * init_lpc10_decoder_state().
 *
 * Revision 1.1  1996/08/19  22:32:34  jaf
 * Initial revision
 *

*/

#ifdef P_R_O_T_O_T_Y_P_E_S
extern int deemp_(real *x, integer *n, struct lpc10_decoder_state *st);
#endif

/*  -- translated by f2c (version 19951025).
   You must link the resulting object file with the libraries:
	-lf2c -lm   (in that order)
*/

#include "f2c.h"

/* ***************************************************************** */

/* 	DEEMP Version 48 */

/* $Log: deemp.c,v $
/* Revision 1.1  2004/05/04 11:16:42  csoutheren
/* Initial version
/*
/* Revision 1.1  2000/06/05 04:45:12  robertj
/* Added LPC-10 2400bps codec
/*
 * Revision 1.2  1996/08/20  20:23:46  jaf
 * Removed all static local variables that were SAVE'd in the Fortran
 * code, and put them in struct lpc10_decoder_state that is passed as an
 * argument.
 *
 * Removed init function, since all initialization is now done in
 * init_lpc10_decoder_state().
 *
 * Revision 1.1  1996/08/19  22:32:34  jaf
 * Initial revision
 * */
/* Revision 1.3  1996/03/20  15:54:37  jaf */
/* Added comments about which indices of array arguments are read or */
/* written. */

/* Added entry INITDEEMP to reinitialize the local state variables, if */
/* desired. */

/* Revision 1.2  1996/03/14  22:11:13  jaf */
/* Comments added explaining which of the local variables of this */
/* subroutine need to be saved from one invocation to the next, and which */
/* do not. */

/* Revision 1.1  1996/02/07 14:44:53  jaf */
/* Initial revision */


/* ***************************************************************** */

/*  De-Emphasize output speech with   1 / ( 1 - .75z**-1 ) */
/*    cascaded with 200 Hz high pass filter */
/*    ( 1 - 1.9998z**-1 + z**-2 ) / ( 1 - 1.75z**-1 + .78z**-2 ) */

/*  WARNING!  The coefficients above may be out of date with the code */
/*  below.  Either that, or some kind of transformation was performed */
/*  on the coefficients above to create the code below. */

/* Input: */
/*  N  - Number of samples */
/* Input/Output: */
/*  X  - Speech */
/*       Indices 1 through N are read before being written. */

/* This subroutine maintains local state from one call to the next.  If */
/* you want to switch to using a new audio stream for this filter, or */
/* reinitialize its state for any other reason, call the ENTRY */
/* INITDEEMP. */

/* Subroutine */ int deemp_(real *x, integer *n, struct lpc10_decoder_state *st)
{
    /* Initialized data */

    real *dei1;
    real *dei2;
    real *deo1;
    real *deo2;
    real *deo3;

    /* System generated locals */
    integer i__1;
    real r__1;

    /* Local variables */
    integer k;
    real dei0;

/*       Arguments */
/*       Local variables that need not be saved */
/*       Local state */
/*       All of the locals saved below were not given explicit initial */
/*       values in the original code.  I think 0 is a safe choice. */
    /* Parameter adjustments */
    if (x) {
	--x;
	}

    /* Function Body */

    dei1 = &(st->dei1);
    dei2 = &(st->dei2);
    deo1 = &(st->deo1);
    deo2 = &(st->deo2);
    deo3 = &(st->deo3);

    i__1 = *n;
    for (k = 1; k <= i__1; ++k) {
	dei0 = x[k];
	r__1 = x[k] - *dei1 * 1.9998f + *dei2;
	x[k] = r__1 + *deo1 * 2.5f - *deo2 * 2.0925f + *deo3 * .585f;
	*dei2 = *dei1;
	*dei1 = dei0;
	*deo3 = *deo2;
	*deo2 = *deo1;
	*deo1 = x[k];
    }
    return 0;
} /* deemp_ */
