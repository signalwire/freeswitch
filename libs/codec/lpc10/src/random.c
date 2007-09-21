/*

$Log: random.c,v $
Revision 1.1  2004/05/04 11:16:43  csoutheren
Initial version

Revision 1.1  2000/06/05 04:45:12  robertj
Added LPC-10 2400bps codec

 * Revision 1.2  1996/08/20  20:41:32  jaf
 * Removed all static local variables that were SAVE'd in the Fortran
 * code, and put them in struct lpc10_decoder_state that is passed as an
 * argument.
 *
 * Removed init function, since all initialization is now done in
 * init_lpc10_decoder_state().
 *
 * Revision 1.1  1996/08/19  22:30:49  jaf
 * Initial revision
 *

*/

#ifdef P_R_O_T_O_T_Y_P_E_S
extern integer random_(struct lpc10_decoder_state *st);
#endif

/*  -- translated by f2c (version 19951025).
   You must link the resulting object file with the libraries:
	-lf2c -lm   (in that order)
*/

#include "f2c.h"

/* ********************************************************************** */

/* 	RANDOM Version 49 */

/* $Log: random.c,v $
 * Revision 1.1  2004/05/04 11:16:43  csoutheren
 * Initial version
 *
 * Revision 1.1  2000/06/05 04:45:12  robertj
 * Added LPC-10 2400bps codec
 *
 * Revision 1.2  1996/08/20  20:41:32  jaf
 * Removed all static local variables that were SAVE'd in the Fortran
 * code, and put them in struct lpc10_decoder_state that is passed as an
 * argument.
 *
 * Removed init function, since all initialization is now done in
 * init_lpc10_decoder_state().
 *
 * Revision 1.1  1996/08/19  22:30:49  jaf
 * Initial revision
 *
 */
/* Revision 1.3  1996/03/20  16:13:54  jaf */
/* Rearranged comments a little bit, and added comments explaining that */
/* even though there is local state here, there is no need to create an */
/* ENTRY for reinitializing it. */

/* Revision 1.2  1996/03/14  22:25:29  jaf */
/* Just rearranged the comments and local variable declarations a bit. */

/* Revision 1.1  1996/02/07 14:49:01  jaf */
/* Initial revision */


/* ********************************************************************* */

/*  Pseudo random number generator based on Knuth, Vol 2, p. 27. */

/* Function Return: */
/*  RANDOM - Integer variable, uniformly distributed over -32768 to 32767 */

/* This subroutine maintains local state from one call to the next. */
/* In the context of the LPC10 coder, there is no reason to reinitialize */
/* this local state when switching between audio streams, because its */
/* results are only used to generate noise for unvoiced frames. */

integer random_(struct lpc10_decoder_state *st)
{
    /* Initialized data */

    integer *j;
    integer *k;
    shortint *y;

    /* System generated locals */
    integer ret_val;

/* 	Parameters/constants */
/*       Local state */
/*   The following is a 16 bit 2's complement addition, */
/*   with overflow checking disabled */

    j = &(st->j);
    k = &(st->k);
    y = &(st->y[0]);

    y[*k - 1] += y[*j - 1];
    ret_val = y[*k - 1];
    --(*k);
    if (*k <= 0) {
	*k = 5;
    }
    --(*j);
    if (*j <= 0) {
	*j = 5;
    }
    return ret_val;
} /* random_ */

