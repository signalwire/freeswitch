/*

$Log: placea.c,v $
Revision 1.1  2004/05/04 11:16:43  csoutheren
Initial version

Revision 1.2  2001/10/16 21:21:14  yurik
Removed warnings on Windows CE. Submitted by Jehan Bing, jehan@bravobrava.com

Revision 1.1  2000/06/05 04:45:12  robertj
Added LPC-10 2400bps codec

 * Revision 1.1  1996/08/19  22:31:07  jaf
 * Initial revision
 *

*/

#ifdef P_R_O_T_O_T_Y_P_E_S
extern int placea_(integer *ipitch, integer *voibuf, integer *obound, integer *af, integer *vwin, integer *awin, integer *ewin, integer *lframe, integer *maxwin);
#endif

/*  -- translated by f2c (version 19951025).
   You must link the resulting object file with the libraries:
	-lf2c -lm   (in that order)
*/

#include "f2c.h"

/* *********************************************************************** */

/* 	PLACEA Version 48 */

/* $Log: placea.c,v $
/* Revision 1.1  2004/05/04 11:16:43  csoutheren
/* Initial version
/*
/* Revision 1.2  2001/10/16 21:21:14  yurik
/* Removed warnings on Windows CE. Submitted by Jehan Bing, jehan@bravobrava.com
/*
/* Revision 1.1  2000/06/05 04:45:12  robertj
/* Added LPC-10 2400bps codec
/*
 * Revision 1.1  1996/08/19  22:31:07  jaf
 * Initial revision
 * */
/* Revision 1.5  1996/03/19  20:41:55  jaf */
/* Added some conditions satisfied by the output values in EWIN. */

/* Revision 1.4  1996/03/19  20:24:17  jaf */
/* Added some conditions satisfied by the output values in AWIN. */

/* Revision 1.3  1996/03/18  21:40:04  jaf */
/* Just added a few comments about which array indices of the arguments */
/* are used, and mentioning that this subroutine has no local state. */

/* Revision 1.2  1996/03/13  16:43:09  jaf */
/* Comments added explaining that none of the local variables of this */
/* subroutine need to be saved from one invocation to the next. */

/* Revision 1.1  1996/02/07 14:48:31  jaf */
/* Initial revision */


/* *********************************************************************** */
/* Input: */
/*  IPITCH */
/*  VOIBUF */
/*          Indices (2,AF-2), (1,AF-1), (2,AF-1), (1,AF), and (2,AF) read.*/
/*           All other indices untouched. */
/*  OBOUND */
/*  AF */
/*  VWIN */
/*           Indices (1,AF) and (2,AF) read. */
/*           All other indices untouched. */
/*  LFRAME */
/*  MAXWIN */
/* Input/Output: */
/*  AWIN */
/*           Index (1,AF-1) read. */
/*           Indices (1,AF) and (2,AF) written, and then read. */
/*           All other indices untouched. */
/*           In all cases (except possibly one), the final values will */
/*           satisfy the condition: AWIN(2,AF)-AWIN(1,AF)+1 = MAXWIN. */
/*           In that other case, */
/*           AWIN(1,AF)=VWIN(1,AF) and AWIN(2,AF)=VWIN(2,AF). */
/* Output: */
/*  EWIN */
/*           Indices (1,AF) and (2,AF) written. */
/*           All other indices untouched. */
/*           In all cases, the final values will satisfy the condition: */
/*           AWIN(1,AF) .LE. EWIN(1,AF) .LE. EWIN(2,AF) .LE. AWIN(2,AF) */
/*           In other words, the energy window is a sub-window of */
/*           the analysis window. */

/* This subroutine has no local state. */

/* Subroutine */ int placea_(integer *ipitch, integer *voibuf, integer *
	obound, integer *af, integer *vwin, integer *awin, integer *ewin, 
	integer *lframe, integer *maxwin)
{
    /* System generated locals */
    real r__1;

    /* Builtin functions */
    integer i_nint(real *);

    /* Local variables */
    logical allv, winv;
    integer i__, j, k, l, hrange;
    logical ephase;
    integer lrange;

/*       Arguments */
/*       Local variables that need not be saved */
    /* Parameter adjustments */
    ewin -= 3;
    awin -= 3;
    vwin -= 3;
    --voibuf;

    /* Function Body */
    lrange = (*af - 2) * *lframe + 1;
    hrange = *af * *lframe;
/*   Place the Analysis window based on the voicing window */
/*   placement, onsets, tentative voicing decision, and pitch. */

/*   Case 1:  Sustained Voiced Speech */
/*   If the five most recent voicing decisions are */
/*   voiced, then the window is placed phase-synchronously with the */
/*   previous window, as close to the present voicing window if possible. 
*/
/*   If onsets bound the voicing window, then preference is given to */
/*   a phase-synchronous placement which does not overlap these onsets. */

/*   Case 2:  Voiced Transition */
/*   If at least one voicing decision in AF is voicied, and there are no 
*/
/*   onsets, then the window is placed as in case 1. */

/*   Case 3:  Unvoiced Speech or Onsets */
/*   If both voicing decisions in AF are unvoiced, or there are onsets, */
/*   then the window is placed coincident with the voicing window. */

/*   Note:  During phase-synchronous placement of windows, the length */
/*   is not altered from MAXWIN, since this would defeat the purpose */
/*   of phase-synchronous placement. */
/* Check for case 1 and case 2 */
    allv = voibuf[((*af - 2) << 1) + 2] == 1;
    allv = allv && voibuf[((*af - 1) << 1) + 1] == 1;
    allv = allv && voibuf[((*af - 1) << 1) + 2] == 1;
    allv = allv && voibuf[(*af << 1) + 1] == 1;
    allv = allv && voibuf[(*af << 1) + 2] == 1;
    winv = voibuf[(*af << 1) + 1] == 1 || voibuf[(*af << 1) + 2] == 1;
    if (allv || winv && *obound == 0) {
/* APHASE:  Phase synchronous window placement. */
/* Get minimum lower index of the window. */
	i__ = (lrange + *ipitch - 1 - awin[((*af - 1) << 1) + 1]) / *ipitch;
	i__ *= *ipitch;
	i__ += awin[((*af - 1) << 1) + 1];
/* L = the actual length of this frame's analysis window. */
	l = *maxwin;
/* Calculate the location where a perfectly centered window would star
t. */
	k = (vwin[(*af << 1) + 1] + vwin[(*af << 1) + 2] + 1 - l) / 2;
/* Choose the actual location to be the pitch multiple closest to this
. */
	r__1 = (real) (k - i__) / *ipitch;
	awin[(*af << 1) + 1] = i__ + i_nint(&r__1) * *ipitch;
	awin[(*af << 1) + 2] = awin[(*af << 1) + 1] + l - 1;
/* If there is an onset bounding the right of the voicing window and t
he */
/* analysis window overlaps that, then move the analysis window backwa
rd */
/* to avoid this onset. */
	if (*obound >= 2 && awin[(*af << 1) + 2] > vwin[(*af << 1) + 2]) {
	    awin[(*af << 1) + 1] -= *ipitch;
	    awin[(*af << 1) + 2] -= *ipitch;
	}
/* Similarly for the left of the voicing window. */
	if ((*obound == 1 || *obound == 3) && awin[(*af << 1) + 1] < vwin[(*
		af << 1) + 1]) {
	    awin[(*af << 1) + 1] += *ipitch;
	    awin[(*af << 1) + 2] += *ipitch;
	}
/* If this placement puts the analysis window above HRANGE, then */
/* move it backward an integer number of pitch periods. */
	while(awin[(*af << 1) + 2] > hrange) {
	    awin[(*af << 1) + 1] -= *ipitch;
	    awin[(*af << 1) + 2] -= *ipitch;
	}
/* Similarly if the placement puts the analysis window below LRANGE. 
*/
	while(awin[(*af << 1) + 1] < lrange) {
	    awin[(*af << 1) + 1] += *ipitch;
	    awin[(*af << 1) + 2] += *ipitch;
	}
/* Make Energy window be phase-synchronous. */
	ephase = TRUE_;
/* Case 3 */
    } else {
	awin[(*af << 1) + 1] = vwin[(*af << 1) + 1];
	awin[(*af << 1) + 2] = vwin[(*af << 1) + 2];
	ephase = FALSE_;
    }
/* RMS is computed over an integer number of pitch periods in the analysis
 */
/*window.  When it is not placed phase-synchronously, it is placed as clos
e*/
/* as possible to onsets. */
    j = (awin[(*af << 1) + 2] - awin[(*af << 1) + 1] + 1) / *ipitch * *ipitch;
    if (j == 0 || ! winv) {
	ewin[(*af << 1) + 1] = vwin[(*af << 1) + 1];
	ewin[(*af << 1) + 2] = vwin[(*af << 1) + 2];
    } else if (! ephase && *obound == 2) {
	ewin[(*af << 1) + 1] = awin[(*af << 1) + 2] - j + 1;
	ewin[(*af << 1) + 2] = awin[(*af << 1) + 2];
    } else {
	ewin[(*af << 1) + 1] = awin[(*af << 1) + 1];
	ewin[(*af << 1) + 2] = awin[(*af << 1) + 1] + j - 1;
    }
    return 0;
} /* placea_ */

