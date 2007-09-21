/*

$Log: median.c,v $
Revision 1.1  2004/05/04 11:16:43  csoutheren
Initial version

Revision 1.1  2000/06/05 04:45:12  robertj
Added LPC-10 2400bps codec

 * Revision 1.1  1996/08/19  22:31:31  jaf
 * Initial revision
 *

*/

#ifdef P_R_O_T_O_T_Y_P_E_S
extern integer median_(integer *d1, integer *d2, integer *d3);
#endif

/*  -- translated by f2c (version 19951025).
   You must link the resulting object file with the libraries:
	-lf2c -lm   (in that order)
*/

#include "f2c.h"

/* ********************************************************************* */

/* 	MEDIAN Version 45G */

/* $Log: median.c,v $
 * Revision 1.1  2004/05/04 11:16:43  csoutheren
 * Initial version
 *
 * Revision 1.1  2000/06/05 04:45:12  robertj
 * Added LPC-10 2400bps codec
 *
 * Revision 1.1  1996/08/19  22:31:31  jaf
 * Initial revision
 *
 */
/* Revision 1.2  1996/03/14  22:30:22  jaf */
/* Just rearranged the comments and local variable declarations a bit. */

/* Revision 1.1  1996/02/07 14:47:53  jaf */
/* Initial revision */


/* ********************************************************************* */

/*  Find median of three values */

/* Input: */
/*  D1,D2,D3 - Three input values */
/* Output: */
/*  MEDIAN - Median value */

integer median_(integer *d1, integer *d2, integer *d3)
{
    /* System generated locals */
    integer ret_val;

/*       Arguments */
    ret_val = *d2;
    if (*d2 > *d1 && *d2 > *d3) {
	ret_val = *d1;
	if (*d3 > *d1) {
	    ret_val = *d3;
	}
    } else if (*d2 < *d1 && *d2 < *d3) {
	ret_val = *d1;
	if (*d3 < *d1) {
	    ret_val = *d3;
	}
    }
    return ret_val;
} /* median_ */

