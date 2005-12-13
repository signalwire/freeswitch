/*

$Log$
Revision 1.15  2004/06/26 03:50:14  markster
Merge source cleanups (bug #1911)

Revision 1.14  2003/02/12 13:59:15  matteo
mer feb 12 14:56:57 CET 2003

Revision 1.1.1.1  2003/02/12 13:59:15  matteo
mer feb 12 14:56:57 CET 2003

Revision 1.2  2000/01/05 08:20:39  markster
Some OSS fixes and a few lpc changes to make it actually work

 * Revision 1.2  1996/08/20  20:28:05  jaf
 * Removed all static local variables that were SAVE'd in the Fortran
 * code, and put them in struct lpc10_encoder_state that is passed as an
 * argument.
 *
 * Removed init function, since all initialization is now done in
 * init_lpc10_encoder_state().
 *
 * Revision 1.1  1996/08/19  22:32:04  jaf
 * Initial revision
 *

*/

/*  -- translated by f2c (version 19951025).
   You must link the resulting object file with the libraries:
	-lf2c -lm   (in that order)
*/

#include "f2c.h"

#ifdef P_R_O_T_O_T_Y_P_E_S
extern int hp100_(real *speech, integer *start, integer *end,
		  struct lpc10_encoder_state *st);
extern int inithp100_(void);
#endif

/* ********************************************************************* */

/*      HP100 Version 55 */

/* $Log$
 * Revision 1.15  2004/06/26 03:50:14  markster
 * Merge source cleanups (bug #1911)
 *
/* Revision 1.14  2003/02/12 13:59:15  matteo
/* mer feb 12 14:56:57 CET 2003
/*
/* Revision 1.1.1.1  2003/02/12 13:59:15  matteo
/* mer feb 12 14:56:57 CET 2003
/*
/* Revision 1.2  2000/01/05 08:20:39  markster
/* Some OSS fixes and a few lpc changes to make it actually work
/*
 * Revision 1.2  1996/08/20  20:28:05  jaf
 * Removed all static local variables that were SAVE'd in the Fortran
 * code, and put them in struct lpc10_encoder_state that is passed as an
 * argument.
 *
 * Removed init function, since all initialization is now done in
 * init_lpc10_encoder_state().
 *
 * Revision 1.1  1996/08/19  22:32:04  jaf
 * Initial revision
 * */
/* Revision 1.6  1996/03/15  16:45:25  jaf */
/* Rearranged a few comments. */

/* Revision 1.5  1996/03/14  23:20:54  jaf */
/* Added comments about when INITHP100 should be used. */

/* Revision 1.4  1996/03/14  23:08:08  jaf */
/* Added an entry named INITHP100 that initializes the local state of */
/* subroutine HP100. */

/* Revision 1.3  1996/03/14  22:09:20  jaf */
/* Comments added explaining which of the local variables of this */
/* subroutine need to be saved from one invocation to the next, and which */
/* do not. */

/* Revision 1.2  1996/02/12  15:05:54  jaf */
/* Added lots of comments explaining why I changed one line, which was a */
/* declaration with initializations. */

/* Revision 1.1  1996/02/07 14:47:12  jaf */
/* Initial revision */


/* ********************************************************************* */

/*    100 Hz High Pass Filter */

/* Jan 92 - corrected typo (1.937148 to 1.935715), */
/*          rounded coefficients to 7 places, */
/*          corrected and merged gain (.97466**4), */
/*          merged numerator into first two sections. */

/* Input: */
/*  start, end - Range of samples to filter */
/* Input/Output: */
/*  speech(end) - Speech data. */
/*                Indices start through end are read and modified. */

/* This subroutine maintains local state from one call to the next.  If */
/* you want to switch to using a new audio stream for this filter, or */
/* reinitialize its state for any other reason, call the ENTRY */
/* INITHP100. */
/* Subroutine */ int hp100_(real *speech, integer *start, integer *end,
	struct lpc10_encoder_state *st)
{
    /* Temporary local copies of variables in lpc10_encoder_state.
       I've only created these because it might cause the loop below
       to execute a bit faster to access local variables, rather than
       variables in the lpc10_encoder_state structure.  It is just a
       guess that it will be faster. */

    real z11;
    real z21;
    real z12;
    real z22;

    /* System generated locals */
    integer i__1;

    /* Local variables */
    integer i__;
    real si, err;

/*       Arguments */
/*       Local variables that need not be saved */
/*       Local state */
    /* Parameter adjustments */
    if (speech) {
	--speech;
	}

    /* Function Body */

    z11 = st->z11;
    z21 = st->z21;
    z12 = st->z12;
    z22 = st->z22;

    i__1 = *end;
    for (i__ = *start; i__ <= i__1; ++i__) {
	si = speech[i__];
	err = si + z11 * 1.859076f - z21 * .8648249f;
	si = err - z11 * 2.f + z21;
	z21 = z11;
	z11 = err;
	err = si + z12 * 1.935715f - z22 * .9417004f;
	si = err - z12 * 2.f + z22;
	z22 = z12;
	z12 = err;
	speech[i__] = si * .902428f;
    }

    st->z11 = z11;
    st->z21 = z21;
    st->z12 = z12;
    st->z22 = z22;

    return 0;
} /* hp100_ */
