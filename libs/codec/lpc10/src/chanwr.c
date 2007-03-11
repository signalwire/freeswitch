/*

$Log: chanwr.c,v $
Revision 1.1  2004/05/04 11:16:42  csoutheren
Initial version

Revision 1.1  2000/06/05 04:45:12  robertj
Added LPC-10 2400bps codec

 * Revision 1.2  1996/08/20  20:20:24  jaf
 * Removed all static local variables that were SAVE'd in the Fortran
 * code, and put them in struct lpc10_encoder_state that is passed as an
 * argument.
 *
 * Revision 1.1  1996/08/19  22:40:31  jaf
 * Initial revision
 *

*/

#ifdef P_R_O_T_O_T_Y_P_E_S
extern int chanwr_(integer *order, integer *ipitv, integer *irms, integer *irc, integer *ibits, struct lpc10_encoder_state *st);
extern int chanrd_(integer *order, integer *ipitv, integer *irms, integer *irc, integer *ibits);
#endif

/*  -- translated by f2c (version 19951025).
   You must link the resulting object file with the libraries:
	-lf2c -lm   (in that order)
*/

#include "f2c.h"

/* *********************************************************************** */

/* 	CHANL Version 49 */

/* $Log: chanwr.c,v $
 * Revision 1.1  2004/05/04 11:16:42  csoutheren
 * Initial version
 *
 * Revision 1.1  2000/06/05 04:45:12  robertj
 * Added LPC-10 2400bps codec
 *
 * Revision 1.2  1996/08/20  20:20:24  jaf
 * Removed all static local variables that were SAVE'd in the Fortran
 * code, and put them in struct lpc10_encoder_state that is passed as an
 * argument.
 *
 * Revision 1.1  1996/08/19  22:40:31  jaf
 * Initial revision
 *
 */
/* Revision 1.3  1996/03/21  15:14:57  jaf */
/* Added comments about which indices of argument arrays are read or */
/* written, and about the one bit of local state in CHANWR.  CHANRD */
/* has no local state. */

/* Revision 1.2  1996/03/13  18:55:10  jaf */
/* Comments added explaining which of the local variables of this */
/* subroutine need to be saved from one invocation to the next, and which */
/* do not. */

/* Revision 1.1  1996/02/07 14:43:31  jaf */
/* Initial revision */


/* *********************************************************************** */

/* CHANWR: */
/*   Place quantized parameters into bitstream */

/* Input: */
/*  ORDER  - Number of reflection coefficients (not really variable) */
/*  IPITV  - Quantized pitch/voicing parameter */
/*  IRMS   - Quantized energy parameter */
/*  IRC    - Quantized reflection coefficients */
/*           Indices 1 through ORDER read. */
/* Output: */
/*  IBITS  - Serial bitstream */
/*           Indices 1 through 54 written. */
/*           Bit 54, the SYNC bit, alternates from one call to the next. */

/* Subroutine CHANWR maintains one bit of local state from one call to */
/* the next, in the variable ISYNC.  I believe that this one bit is only */
/* intended to allow a receiver to resynchronize its interpretation of */
/* the bit stream, by looking for which of the 54 bits alternates every */
/* frame time.  This is just a simple framing mechanism that is not */
/* useful when other, higher overhead framing mechanisms are used to */
/* transmit the coded frames. */

/* I'm not going to make an entry to reinitialize this bit, since it */
/* doesn't help a receiver much to know whether the first sync bit is a 0 */
/* or a 1.  It needs to examine several frames in sequence to have */
/* reasonably good assurance that its framing is correct. */


/* CHANRD: */
/*   Reconstruct parameters from bitstream */

/* Input: */
/*  ORDER  - Number of reflection coefficients (not really variable) */
/*  IBITS  - Serial bitstream */
/*           Indices 1 through 53 read (SYNC bit is ignored). */
/* Output: */
/*  IPITV  - Quantized pitch/voicing parameter */
/*  IRMS   - Quantized energy parameter */
/*  IRC    - Quantized reflection coefficients */
/*           Indices 1 through ORDER written */

/* Entry CHANRD has no local state. */



/*   IBITS is 54 bits of LPC data ordered as follows: */
/* 	R1-0, R2-0, R3-0,  P-0,  A-0, */
/* 	R1-1, R2-1, R3-1,  P-1,  A-1, */
/* 	R1-2, R4-0, R3-2,  A-2,  P-2, R4-1, */
/* 	R1-3, R2-2, R3-3, R4-2,  A-3, */
/* 	R1-4, R2-3, R3-4, R4-3,  A-4, */
/* 	 P-3, R2-4, R7-0, R8-0,  P-4, R4-4, */
/* 	R5-0, R6-0, R7-1,R10-0, R8-1, */
/* 	R5-1, R6-1, R7-2, R9-0,  P-5, */
/* 	R5-2, R6-2,R10-1, R8-2,  P-6, R9-1, */
/* 	R5-3, R6-3, R7-3, R9-2, R8-3, SYNC */
/* Subroutine */ int chanwr_0_(int n__, integer *order, integer *ipitv, 
	integer *irms, integer *irc, integer *ibits,
			       struct lpc10_encoder_state *st)
{
    /* Initialized data */

    integer *isync;
    static integer bit[10] = { 2,4,8,8,8,8,16,16,16,16 };
    static integer iblist[53] = { 13,12,11,1,2,13,12,11,1,2,13,10,11,2,1,10,
	    13,12,11,10,2,13,12,11,10,2,1,12,7,6,1,10,9,8,7,4,6,9,8,7,5,1,9,8,
	    4,6,1,5,9,8,7,5,6 };

    /* System generated locals */
    integer i__1;

    /* Local variables */
    integer itab[13], i__;

/*       Arguments */
/*       Parameters/constants */
/*       These arrays are not Fortran PARAMETER's, but they are defined */
/*       by DATA statements below, and their contents are never altered. 
*/
/*       Local variables that need not be saved */
/*       Local state */
/*       ISYNC is only used by CHANWR, not by ENTRY CHANRD. */

    /* Parameter adjustments */
    --irc;
    --ibits;

    /* Function Body */
    switch(n__) {
	case 1: goto L_chanrd;
	}

    isync = &(st->isync);

/* ***********************************************************************
 */
/* 	Place quantized parameters into bitstream */
/* ***********************************************************************
 */
/*   Place parameters into ITAB */
    itab[0] = *ipitv;
    itab[1] = *irms;
    itab[2] = 0;
    i__1 = *order;
    for (i__ = 1; i__ <= i__1; ++i__) {
	itab[i__ + 2] = irc[*order + 1 - i__] & 32767;
    }
/*   Put 54 bits into IBITS array */
    for (i__ = 1; i__ <= 53; ++i__) {
	ibits[i__] = itab[iblist[i__ - 1] - 1] & 1;
	itab[iblist[i__ - 1] - 1] /= 2;
    }
    ibits[54] = *isync & 1;
    *isync = 1 - *isync;
    return 0;
/* ***********************************************************************
 */
/* 	Reconstruct parameters from bitstream */
/* ***********************************************************************
 */

L_chanrd:
/*   Reconstruct ITAB */
    for (i__ = 1; i__ <= 13; ++i__) {
	itab[i__ - 1] = 0;
    }
    for (i__ = 1; i__ <= 53; ++i__) {
	itab[iblist[54 - i__ - 1] - 1] = (itab[iblist[54 - i__ - 1] - 1] << 1)
		 + ibits[54 - i__];
    }
/*   Sign extend RC's */
    i__1 = *order;
    for (i__ = 1; i__ <= i__1; ++i__) {
	if ((itab[i__ + 2] & bit[i__ - 1]) != 0) {
	    itab[i__ + 2] -= bit[i__ - 1] << 1;
	}
    }
/*   Restore variables */
    *ipitv = itab[0];
    *irms = itab[1];
    i__1 = *order;
    for (i__ = 1; i__ <= i__1; ++i__) {
	irc[i__] = itab[*order + 4 - i__ - 1];
    }
    return 0;
} /* chanwr_ */

/* Subroutine */ int chanwr_(integer *order, integer *ipitv, integer *irms, 
	integer *irc, integer *ibits, struct lpc10_encoder_state *st)
{
    return chanwr_0_(0, order, ipitv, irms, irc, ibits, st);
    }

/* Subroutine */ int chanrd_(integer *order, integer *ipitv, integer *irms, 
	integer *irc, integer *ibits)
{
    return chanwr_0_(1, order, ipitv, irms, irc, ibits, 0);
    }
