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

 * Revision 1.2  1996/08/20  20:25:29  jaf
 * Removed all static local variables that were SAVE'd in the Fortran
 * code, and put them in struct lpc10_encoder_state that is passed as an
 * argument.
 *
 * Removed init function, since all initialization is now done in
 * init_lpc10_encoder_state().
 *
 * Revision 1.1  1996/08/19  22:32:26  jaf
 * Initial revision
 *

*/

/*  -- translated by f2c (version 19951025).
   You must link the resulting object file with the libraries:
	-lf2c -lm   (in that order)
*/

#include "f2c.h"

#ifdef P_R_O_T_O_T_Y_P_E_S
extern int dyptrk_(real *amdf, integer *ltau, integer *minptr, integer *voice, integer *pitch, integer *midx, struct lpc10_encoder_state *st);
/* comlen contrl_ 12 */
#endif

/* Common Block Declarations */

extern struct {
    integer order, lframe;
    logical corrp;
} contrl_;

#define contrl_1 contrl_

/* ********************************************************************* */

/* 	DYPTRK Version 52 */

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
 * Revision 1.2  1996/08/20  20:25:29  jaf
 * Removed all static local variables that were SAVE'd in the Fortran
 * code, and put them in struct lpc10_encoder_state that is passed as an
 * argument.
 *
 * Removed init function, since all initialization is now done in
 * init_lpc10_encoder_state().
 *
 * Revision 1.1  1996/08/19  22:32:26  jaf
 * Initial revision
 * */
/* Revision 1.5  1996/03/26  19:35:35  jaf */
/* Commented out trace statements. */

/* Revision 1.4  1996/03/19  18:03:22  jaf */
/* Replaced the initialization "DATA P/60*DEPTH*0/" with "DATA P/120*0/", */
/* because apparently Fortran (or at least f2c) can't handle expressions */
/* like that. */

/* Revision 1.3  1996/03/19  17:38:32  jaf */
/* Added comments about the local variables that should be saved from one */
/* invocation to the next.  None of them were given initial values in the */
/* original code, but from my testing, it appears that initializing them */
/* all to 0 works. */

/* Added entry INITDYPTRK to reinitialize these local variables. */

/* Revision 1.2  1996/03/13  16:32:17  jaf */
/* Comments added explaining which of the local variables of this */
/* subroutine need to be saved from one invocation to the next, and which */
/* do not. */

/* WARNING!  Some of them that should are never given initial values in */
/* this code.  Hopefully, Fortran 77 defines initial values for them, but */
/* even so, giving them explicit initial values is preferable. */

/* Revision 1.1  1996/02/07 14:45:14  jaf */
/* Initial revision */


/* ********************************************************************* */

/*   Dynamic Pitch Tracker */

/* Input: */
/*  AMDF   - Average Magnitude Difference Function array */
/*           Indices 1 through LTAU read, and MINPTR */
/*  LTAU   - Number of lags in AMDF */
/*  MINPTR - Location of minimum AMDF value */
/*  VOICE  - Voicing decision */
/* Output: */
/*  PITCH  - Smoothed pitch value, 2 frames delayed */
/*  MIDX   - Initial estimate of current frame pitch */
/* Compile time constant: */
/*  DEPTH  - Number of frames to trace back */

/* This subroutine maintains local state from one call to the next.  If */
/* you want to switch to using a new audio stream for this filter, or */
/* reinitialize its state for any other reason, call the ENTRY */
/* INITDYPTRK. */

/* Subroutine */ int dyptrk_(real *amdf, integer *ltau, integer *
	minptr, integer *voice, integer *pitch, integer *midx,
			       struct lpc10_encoder_state *st)
{
    /* Initialized data */

    real *s;
    integer *p;
    integer *ipoint;
    real *alphax;

    /* System generated locals */
    integer i__1;

    /* Local variables */
    integer pbar;
    real sbar;
    integer path[2], iptr, i__, j;
    real alpha, minsc, maxsc;

/*       Arguments */
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
 * Revision 1.2  1996/08/20  20:25:29  jaf
 * Removed all static local variables that were SAVE'd in the Fortran
 * code, and put them in struct lpc10_encoder_state that is passed as an
 * argument.
 *
 * Removed init function, since all initialization is now done in
 * init_lpc10_encoder_state().
 *
 * Revision 1.1  1996/08/19  22:32:26  jaf
 * Initial revision
 * */
/* Revision 1.3  1996/03/29  22:05:55  jaf */
/* Commented out the common block variables that are not needed by the */
/* embedded version. */

/* Revision 1.2  1996/03/26  19:34:50  jaf */
/* Added comments indicating which constants are not needed in an */
/* application that uses the LPC-10 coder. */

/* Revision 1.1  1996/02/07  14:44:09  jaf */
/* Initial revision */

/*   LPC Processing control variables: */

/* *** Read-only: initialized in setup */

/*  Files for Speech, Parameter, and Bitstream Input & Output, */
/*    and message and debug outputs. */

/* Here are the only files which use these variables: */

/* lpcsim.f setup.f trans.f error.f vqsetup.f */

/* Many files which use fdebug are not listed, since it is only used in */
/* those other files conditionally, to print trace statements. */
/* 	integer fsi, fso, fpi, fpo, fbi, fbo, pbin, fmsg, fdebug */
/*  LPC order, Frame size, Quantization rate, Bits per frame, */
/*    Error correction */
/* Subroutine SETUP is the only place where order is assigned a value, */
/* and that value is 10.  It could increase efficiency 1% or so to */
/* declare order as a constant (i.e., a Fortran PARAMETER) instead of as 
*/
/* a variable in a COMMON block, since it is used in many places in the */
/* core of the coding and decoding routines.  Actually, I take that back. 
*/
/* At least when compiling with f2c, the upper bound of DO loops is */
/* stored in a local variable before the DO loop begins, and then that is 
*/
/* compared against on each iteration. */
/* Similarly for lframe, which is given a value of MAXFRM in SETUP. */
/* Similarly for quant, which is given a value of 2400 in SETUP.  quant */
/* is used in only a few places, and never in the core coding and */
/* decoding routines, so it could be eliminated entirely. */
/* nbits is similar to quant, and is given a value of 54 in SETUP. */
/* corrp is given a value of .TRUE. in SETUP, and is only used in the */
/* subroutines ENCODE and DECODE.  It doesn't affect the speed of the */
/* coder significantly whether it is .TRUE. or .FALSE., or whether it is 
*/
/* a constant or a variable, since it is only examined once per frame. */
/* Leaving it as a variable that is set to .TRUE.  seems like a good */
/* idea, since it does enable some error-correction capability for */
/* unvoiced frames, with no change in the coding rate, and no noticeable 
*/
/* quality difference in the decoded speech. */
/* 	integer quant, nbits */
/* *** Read/write: variables for debugging, not needed for LPC algorithm 
*/

/*  Current frame, Unstable frames, Output clip count, Max onset buffer, 
*/
/*    Debug listing detail level, Line count on listing page */

/* nframe is not needed for an embedded LPC10 at all. */
/* nunsfm is initialized to 0 in SETUP, and incremented in subroutine */
/* ERROR, which is only called from RCCHK.  When LPC10 is embedded into */
/* an application, I would recommend removing the call to ERROR in RCCHK, 
*/
/* and remove ERROR and nunsfm completely. */
/* iclip is initialized to 0 in SETUP, and incremented in entry SWRITE in 
*/
/* sread.f.  When LPC10 is embedded into an application, one might want */
/* to cause it to be incremented in a routine that takes the output of */
/* SYNTHS and sends it to an audio device.  It could be optionally */
/* displayed, for those that might want to know what it is. */
/* maxosp is never initialized to 0 in SETUP, although it probably should 
*/
/* be, and it is updated in subroutine ANALYS.  I doubt that its value */
/* would be of much interest to an application in which LPC10 is */
/* embedded. */
/* listl and lincnt are not needed for an embedded LPC10 at all. */
/* 	integer nframe, nunsfm, iclip, maxosp, listl, lincnt */
/* 	common /contrl/ fsi, fso, fpi, fpo, fbi, fbo, pbin, fmsg, fdebug */
/* 	common /contrl/ quant, nbits */
/* 	common /contrl/ nframe, nunsfm, iclip, maxosp, listl, lincnt */
/* 	Parameters/constants */
/*       Local variables that need not be saved */
/*       Note that PATH is only used for debugging purposes, and can be */
/*       removed. */
/*       Local state */
/*       It would be a bit more "general" to define S(LTAU), if Fortran */
/*       allows the argument of a function to be used as the dimension of 
*/
/*       a local array variable. */
/*       IPOINT is always in the range 0 to DEPTH-1. */
/*       WARNING! */

/*       In the original version of this subroutine, IPOINT, ALPHAX, */
/*       every element of S, and potentially any element of P with the */
/*       second index value .NE. IPTR were read without being given */
/*       initial values (all indices of P with second index equal to */
/*       IPTR are all written before being read in this subroutine). */

/*       From examining the code carefully, it appears that all of these 
*/
/*       should be saved from one invocation to the next. */

/*       I've run lpcsim with the "-l 6" option to see all of the */
/*       debugging information that is printed out by this subroutine */
/*       below, and it appears that S, P, IPOINT, and ALPHAX are all */
/*       initialized to 0 (these initial values would likely be different 
*/
/*       on different platforms, compilers, etc.).  Given that the output 
*/
/*       of the coder sounds reasonable, I'm going to initialize these */
/*       variables to 0 explicitly. */

    s = &(st->s[0]);
    p = &(st->p[0]);
    ipoint = &(st->ipoint);
    alphax = &(st->alphax);


    /* Parameter adjustments */
    if (amdf) {
	--amdf;
	}

    /* Function Body */

/*   Calculate the confidence factor ALPHA, used as a threshold slope in 
*/
/*   SEESAW.  If unvoiced, set high slope so that every point in P array 
*/
/*  is marked as a potential pitch frequency.  A scaled up version (ALPHAX
)*/
/*   is used to maintain arithmetic precision. */
    if (*voice == 1) {
	*alphax = *alphax * .75f + amdf[*minptr] / 2.f;
    } else {
	*alphax *= .984375f;
    }
    alpha = *alphax / 16;
    if (*voice == 0 && *alphax < 128.f) {
	alpha = 8.f;
    }
/* SEESAW: Construct a pitch pointer array and intermediate winner functio
n*/
/*   Left to right pass: */
    iptr = *ipoint + 1;
    p[iptr * 60 - 60] = 1;
    i__ = 1;
    pbar = 1;
    sbar = s[0];
    i__1 = *ltau;
    for (i__ = 1; i__ <= i__1; ++i__) {
	sbar += alpha;
	if (sbar < s[i__ - 1]) {
	    s[i__ - 1] = sbar;
	    p[i__ + iptr * 60 - 61] = pbar;
	} else {
	    sbar = s[i__ - 1];
	    p[i__ + iptr * 60 - 61] = i__;
	    pbar = i__;
	}
    }
/*   Right to left pass: */
    i__ = pbar - 1;
    sbar = s[i__];
    while(i__ >= 1) {
	sbar += alpha;
	if (sbar < s[i__ - 1]) {
	    s[i__ - 1] = sbar;
	    p[i__ + iptr * 60 - 61] = pbar;
	} else {
	    pbar = p[i__ + iptr * 60 - 61];
	    i__ = pbar;
	    sbar = s[i__ - 1];
	}
	--i__;
    }
/*   Update S using AMDF */
/*   Find maximum, minimum, and location of minimum */
    s[0] += amdf[1] / 2;
    minsc = s[0];
    maxsc = minsc;
    *midx = 1;
    i__1 = *ltau;
    for (i__ = 2; i__ <= i__1; ++i__) {
	s[i__ - 1] += amdf[i__] / 2;
	if (s[i__ - 1] > maxsc) {
	    maxsc = s[i__ - 1];
	}
	if (s[i__ - 1] < minsc) {
	    *midx = i__;
	    minsc = s[i__ - 1];
	}
    }
/*   Subtract MINSC from S to prevent overflow */
    i__1 = *ltau;
    for (i__ = 1; i__ <= i__1; ++i__) {
	s[i__ - 1] -= minsc;
    }
    maxsc -= minsc;
/*   Use higher octave pitch if significant null there */
    j = 0;
    for (i__ = 20; i__ <= 40; i__ += 10) {
	if (*midx > i__) {
	    if (s[*midx - i__ - 1] < maxsc / 4) {
		j = i__;
	    }
	}
    }
    *midx -= j;
/*   TRACE: look back two frames to find minimum cost pitch estimate */
    j = *ipoint;
    *pitch = *midx;
    for (i__ = 1; i__ <= 2; ++i__) {
	j = j % 2 + 1;
	*pitch = p[*pitch + j * 60 - 61];
	path[i__ - 1] = *pitch;
    }

/*       The following statement subtracts one from IPOINT, mod DEPTH.  I 
*/
/*       think the author chose to add DEPTH-1, instead of subtracting 1, 
*/
/*       because then it will work even if MOD doesn't work as desired on 
*/
/*       negative arguments. */

    *ipoint = (*ipoint + 1) % 2;
    return 0;
} /* dyptrk_ */
