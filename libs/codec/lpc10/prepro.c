/*

$Log: prepro.c,v $
Revision 1.1  2004/05/04 11:16:43  csoutheren
Initial version

Revision 1.1  2000/06/05 04:45:12  robertj
Added LPC-10 2400bps codec

 * Revision 1.2  1996/08/20  20:40:51  jaf
 * Removed all static local variables that were SAVE'd in the Fortran
 * code, and put them in struct lpc10_encoder_state that is passed as an
 * argument.
 *
 * Removed init function, since all initialization is now done in
 * init_lpc10_encoder_state().
 *
 * Revision 1.1  1996/08/19  22:30:54  jaf
 * Initial revision
 *

*/

#ifdef P_R_O_T_O_T_Y_P_E_S
extern int prepro_(real *speech, integer *length,
		   struct lpc10_encoder_state *st)
/*:ref: hp100_ 14 3 6 4 4 */
/*:ref: inithp100_ 14 0 */
#endif

/*  -- translated by f2c (version 19951025).
   You must link the resulting object file with the libraries:
	-lf2c -lm   (in that order)
*/

#include "f2c.h"

/* Table of constant values */

static integer c__1 = 1;

/* ********************************************************************* */

/* 	PREPRO Version 48 */

/* $Log: prepro.c,v $
/* Revision 1.1  2004/05/04 11:16:43  csoutheren
/* Initial version
/*
/* Revision 1.1  2000/06/05 04:45:12  robertj
/* Added LPC-10 2400bps codec
/*
 * Revision 1.2  1996/08/20  20:40:51  jaf
 * Removed all static local variables that were SAVE'd in the Fortran
 * code, and put them in struct lpc10_encoder_state that is passed as an
 * argument.
 *
 * Removed init function, since all initialization is now done in
 * init_lpc10_encoder_state().
 *
 * Revision 1.1  1996/08/19  22:30:54  jaf
 * Initial revision
 * */
/* Revision 1.3  1996/03/14  23:22:56  jaf */
/* Added comments about when INITPREPRO should be used. */

/* Revision 1.2  1996/03/14  23:09:27  jaf */
/* Added an entry named INITPREPRO that initializes the local state of */
/* this subroutine, and those it calls (if any). */

/* Revision 1.1  1996/02/07  14:48:54  jaf */
/* Initial revision */


/* ********************************************************************* */

/*    Pre-process input speech: */

/* Inputs: */
/*  LENGTH - Number of SPEECH samples */
/* Input/Output: */
/*  SPEECH(LENGTH) - Speech data. */
/*                   Indices 1 through LENGTH are read and modified. */

/* This subroutine has no local state maintained from one call to the */
/* next, but HP100 does.  If you want to switch to using a new audio */
/* stream for this filter, or reinitialize its state for any other */
/* reason, call the ENTRY INITPREPRO. */

/* Subroutine */ int prepro_(real *speech, integer *length,
			     struct lpc10_encoder_state *st)
{
    extern /* Subroutine */ int hp100_(real *, integer *, integer *, struct lpc10_encoder_state *);

/*       Arguments */
/*   High Pass Filter at 100 Hz */
    /* Parameter adjustments */
    if (speech) {
	--speech;
	}

    /* Function Body */
    hp100_(&speech[1], &c__1, length, st);
    return 0;
} /* prepro_ */
