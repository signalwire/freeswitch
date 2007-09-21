/*

$Log: placev.c,v $
Revision 1.1  2004/05/04 11:16:43  csoutheren
Initial version

Revision 1.2  2001/10/16 21:21:14  yurik
Removed warnings on Windows CE. Submitted by Jehan Bing, jehan@bravobrava.com

Revision 1.1  2000/06/05 04:45:12  robertj
Added LPC-10 2400bps codec

 * Revision 1.1  1996/08/19  22:31:02  jaf
 * Initial revision
 *

*/

#ifdef P_R_O_T_O_T_Y_P_E_S
extern int placev_(integer *osbuf, integer *osptr, integer *oslen, integer *obound, integer *vwin, integer *af, integer *lframe, integer *minwin, integer *maxwin, integer *dvwinl, integer *dvwinh);
#endif

/*  -- translated by f2c (version 19951025).
   You must link the resulting object file with the libraries:
	-lf2c -lm   (in that order)
*/

#include "f2c.h"

/* ****************************************************************** */

/* 	PLACEV Version 48 */

/* $Log: placev.c,v $
 * Revision 1.1  2004/05/04 11:16:43  csoutheren
 * Initial version
 *
 * Revision 1.2  2001/10/16 21:21:14  yurik
 * Removed warnings on Windows CE. Submitted by Jehan Bing, jehan@bravobrava.com
 *
 * Revision 1.1  2000/06/05 04:45:12  robertj
 * Added LPC-10 2400bps codec
 *
 * Revision 1.1  1996/08/19  22:31:02  jaf
 * Initial revision
 *
 */
/* Revision 1.6  1996/03/19  20:42:19  jaf */
/* Added some conditions satisfied by the output values in VWIN. */

/* Revision 1.5  1996/03/19  18:37:56  jaf */
/* Strengthened the specification of which indices of VWIN are read and */
/* written. */

/* Revision 1.4  1996/03/15  16:38:33  jaf */
/* One tiny comment added. */

/* Revision 1.3  1996/03/15  16:36:13  jaf */
/* Added comments giving In/Out status of arguments. */

/* Revision 1.2  1996/03/12  23:56:01  jaf */
/* Comments added explaining that none of the local variables of this */
/* subroutine need to be saved from one invocation to the next. */

/* Revision 1.1  1996/02/07 14:48:39  jaf */
/* Initial revision */


/* ****************************************************************** */

/* Input: */
/*  OSBUF	Buffer which holds sorted indexes of onsets */
/*       	I believe that only indices 1 through OSPTR-1 can be read. */
/*  OSLEN */
/*  OSPTR	Free pointer into OSBUF */
/*  AF */
/*  LFRAME */
/*  MINWIN */
/*  MAXWIN */
/*  DVWINL */
/*  DVWINH	(This argument is never used.  Should it be?) */
/* Input/Output: */
/*  VWIN		Buffer of Voicing Window Positions (Modified) */
/*       	Index (2,AF-1) is read. */
/*       	Indices (1,AF) and (2,AF) are written, */
/*       	and then possibly read. */
/*       	All other indices are unused. */
/*              In all cases, the final values will satsify the condition:*/
/*               VWIN(2,AF)-VWIN(1,AF)+1 .LE. MAXWIN */
/*               I'm not certain yet, but they may also satisfy: */
/*               MINWIN .LE. VWIN(2,AF)-VWIN(1,AF)+1 */
/* Output: */
/*  OBOUND	This variable is set by this procedure and used */
/* 		in placing analysis windows (PLACEA).  Bit 1 */
/* 		indicates whether an onset bounds the left side */
/* 		of the voicing window, and bit 2 indicates whether */
/* 		an onset bounds the right side of the voicing window. */

/* This subroutine has no local state. */

/* Subroutine */ int placev_(integer *osbuf, integer *osptr, integer *oslen, 
	integer *obound, integer *vwin, integer *af, integer *lframe, integer 
	*minwin, integer *maxwin, integer *dvwinl, integer *dvwinh)
{
    /* System generated locals */
    integer i__1, i__2;

    /* Local variables */
    logical crit;
    integer i__, q, osptr1, hrange, lrange;

/*       Arguments */
/*       Local variables that need not be saved */
/*   Variables */
/*    LRANGE, HRANGE  Range in which window is placed */
/*    OSPTR1     OSPTR excluding samples in 3F */
/*       Local state */
/*       None */
/*   Voicing Window Placement */

/*         __________________ __________________ ______________ */
/*        |                  |                  | */
/*        |        1F        |        2F        |        3F ... */
/*        |__________________|__________________|______________ */

/*    Previous | */
/*      Window | */
/*  ...________| */

/*             |                                | */
/*      ------>| This window's placement range  |<------ */
/*             |                                | */

/*   There are three cases.  Note that these are different from those */
/*   given in the LPC-10e phase 1 report. */

/*   1.  If there are no onsets in this range, then the voicing window */
/*   is centered in the pitch window.  If such a placement is not within 
*/
/*   the window's placement range, then the window is placed in the left- 
*/
/*   most portion of the placement range.  Its length is always MAXWIN. */

/*   2.  If the first onset is in 2F and there is sufficient room to place
 */
/*   the window immediately before this onset, then the window is placed 
*/
/*   there, and its length is set to the maximum possible under these */
/*   constraints. */

/* 	"Critical Region Exception":  If there is another onset in 2F */
/* 	such that a window can be placed between the two onsets, the */
/* 	window is placed there (ie, as in case 3). */

/*   3.  Otherwise, the window is placed immediately after the onset.  The
 */
/*   window's length */
/*  is the longest length that can fit in the range under these constraint
s,*/
/*  except that the window may be shortened even further to avoid overlapp
ing*/
/*  other onsets in the placement range.  In any case, the window's length
*/
/*   is at least MINWIN. */

/*   Note that the values of MINWIN and LFRAME must be chosen such */
/*   that case 2 = false implies case 3 = true.   This means that */
/*   MINWIN <= LFRAME/2.  If this were not the case, then a fourth case */
/*   would have to be added for when the window cannot fit either before 
*/
/*   or after the onset. */

/*   Note also that onsets which weren't in 2F last time may be in 1F this
 */
/*  time, due to the filter delays in computing onsets.  The result is tha
t*/
/*   occasionally a voicing window will overlap that onset.  The only way 
*/
/*   to circumvent this problem is to add more delay in processing input 
*/
/*   speech.  In the trade-off between delay and window-placement, window 
*/
/*   placement lost. */
/* Compute the placement range */
    /* Parameter adjustments */
    --osbuf;
    vwin -= 3;

    /* Function Body */
/* Computing MAX */
    i__1 = vwin[((*af - 1) << 1) + 2] + 1, i__2 = (*af - 2) * *lframe + 1;
    lrange = max(i__1,i__2);
    hrange = *af * *lframe;
/* Compute OSPTR1, so the following code only looks at relevant onsets. */
    for (osptr1 = *osptr - 1; osptr1 >= 1; --osptr1) {
	if (osbuf[osptr1] <= hrange) {
	    goto L90;
	}
    }
L90:
    ++osptr1;
/* Check for case 1 first (fast case): */
    if (osptr1 <= 1 || osbuf[osptr1 - 1] < lrange) {
/* Computing MAX */
	i__1 = vwin[((*af - 1) << 1) + 2] + 1;
	vwin[(*af << 1) + 1] = max(i__1,*dvwinl);
	vwin[(*af << 1) + 2] = vwin[(*af << 1) + 1] + *maxwin - 1;
	*obound = 0;
    } else {
/* Search backward in OSBUF for first onset in range. */
/* This code relies on the above check being performed first. */
	for (q = osptr1 - 1; q >= 1; --q) {
	    if (osbuf[q] < lrange) {
		goto L100;
	    }
	}
L100:
	++q;
/* Check for case 2 (placement before onset): */
/* Check for critical region exception: */
	i__1 = osptr1 - 1;
	for (i__ = q + 1; i__ <= i__1; ++i__) {
	    if (osbuf[i__] - osbuf[q] >= *minwin) {
		crit = TRUE_;
		goto L105;
	    }
	}
	crit = FALSE_;
L105:
/* Computing MAX */
	i__1 = (*af - 1) * *lframe, i__2 = lrange + *minwin - 1;
	if (! crit && osbuf[q] > max(i__1,i__2)) {
	    vwin[(*af << 1) + 2] = osbuf[q] - 1;
/* Computing MAX */
	    i__1 = lrange, i__2 = vwin[(*af << 1) + 2] - *maxwin + 1;
	    vwin[(*af << 1) + 1] = max(i__1,i__2);
	    *obound = 2;
/* Case 3 (placement after onset) */
	} else {
	    vwin[(*af << 1) + 1] = osbuf[q];
L110:
	    ++q;
	    if (q >= osptr1) {
		goto L120;
	    }
	    if (osbuf[q] > vwin[(*af << 1) + 1] + *maxwin) {
		goto L120;
	    }
	    if (osbuf[q] < vwin[(*af << 1) + 1] + *minwin) {
		goto L110;
	    }
	    vwin[(*af << 1) + 2] = osbuf[q] - 1;
	    *obound = 3;
	    return 0;
L120:
/* Computing MIN */
	    i__1 = vwin[(*af << 1) + 1] + *maxwin - 1;
	    vwin[(*af << 1) + 2] = min(i__1,hrange);
	    *obound = 1;
	}
    }
    return 0;
} /* placev_ */

