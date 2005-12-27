/*

$Log: bsynz.c,v $
Revision 1.1  2004/05/04 11:16:42  csoutheren
Initial version

Revision 1.2  2002/02/15 03:57:55  yurik
Warnings removed during compilation, patch courtesy of Jehan Bing, jehan@bravobrava.com

Revision 1.1  2000/06/05 04:45:12  robertj
Added LPC-10 2400bps codec

 * Revision 1.2  1996/08/20  20:18:55  jaf
 * Removed all static local variables that were SAVE'd in the Fortran
 * code, and put them in struct lpc10_decoder_state that is passed as an
 * argument.
 *
 * Removed init function, since all initialization is now done in
 * init_lpc10_decoder_state().
 *
 * Revision 1.1  1996/08/19  22:32:58  jaf
 * Initial revision
 *

*/

#ifdef P_R_O_T_O_T_Y_P_E_S
extern int bsynz_(real *coef, integer *ip, integer *iv, real *sout, real *rms, real *ratio, real *g2pass, struct lpc10_decoder_state *st);
/* comlen contrl_ 12 */
/*:ref: random_ 4 0 */
#endif

/*  -- translated by f2c (version 19951025).
   You must link the resulting object file with the libraries:
	-lf2c -lm   (in that order)
*/

#include "f2c.h"

/* Common Block Declarations */

extern struct {
    integer order, lframe;
    logical corrp;
} contrl_;

#define contrl_1 contrl_

/* ***************************************************************** */

/* 	BSYNZ Version 54 */

/* $Log: bsynz.c,v $
/* Revision 1.1  2004/05/04 11:16:42  csoutheren
/* Initial version
/*
/* Revision 1.2  2002/02/15 03:57:55  yurik
/* Warnings removed during compilation, patch courtesy of Jehan Bing, jehan@bravobrava.com
/*
/* Revision 1.1  2000/06/05 04:45:12  robertj
/* Added LPC-10 2400bps codec
/*
 * Revision 1.2  1996/08/20  20:18:55  jaf
 * Removed all static local variables that were SAVE'd in the Fortran
 * code, and put them in struct lpc10_decoder_state that is passed as an
 * argument.
 *
 * Removed init function, since all initialization is now done in
 * init_lpc10_decoder_state().
 *
 * Revision 1.1  1996/08/19  22:32:58  jaf
 * Initial revision
 * */
/* Revision 1.4  1996/03/27  18:11:22  jaf */
/* Changed the range of NOISE printed out in the debugging statements, */
/* even though they are commented out.  I didn't discover this until I */
/* tried comparing two different versions of the LPC-10 coder, each with */
/* full tracing enabled. */

/* Revision 1.3  1996/03/26  19:33:23  jaf */
/* Commented out trace statements. */

/* Revision 1.2  1996/03/20  17:12:54  jaf */
/* Added comments about which indices of array arguments are read or */
/* written. */

/* Rearranged local variable declarations to indicate which need to be */
/* saved from one invocation to the next.  Added entry INITBSYNZ to */
/* reinitialize the local state variables, if desired. */

/* Revision 1.1  1996/02/07 14:43:15  jaf */
/* Initial revision */


/* ***************************************************************** */

/*   Synthesize One Pitch Epoch */

/* Input: */
/*  COEF  - Predictor coefficients */
/*          Indices 1 through ORDER read. */
/*  IP    - Pitch period (number of samples to synthesize) */
/*  IV    - Voicing for the current epoch */
/*  RMS   - Energy for the current epoch */
/*  RATIO - Energy slope for plosives */
/*  G2PASS- Sharpening factor for 2 pass synthesis */
/* Output: */
/*  SOUT  - Synthesized speech */
/*          Indices 1 through IP written. */

/* This subroutine maintains local state from one call to the next.  If */
/* you want to switch to using a new audio stream for this filter, or */
/* reinitialize its state for any other reason, call the ENTRY */
/* INITBSYNZ. */

/* Subroutine */ int bsynz_(real *coef, integer *ip, integer *iv, 
	real *sout, real *rms, real *ratio, real *g2pass,
			    struct lpc10_decoder_state *st)
{
    /* Initialized data */

    integer *ipo;
    real *rmso;
    static integer kexc[25] = { 8,-16,26,-48,86,-162,294,-502,718,-728,184,
	    672,-610,-672,184,728,718,502,294,162,86,48,26,16,8 };
    real *exc;
    real *exc2;
    real *lpi1;
    real *lpi2;
    real *lpi3;
    real *hpi1;
    real *hpi2;
    real *hpi3;

    /* System generated locals */
    integer i__1, i__2;
    real r__1, r__2;

    /* Builtin functions */
    double sqrt(doublereal);

    /* Local variables */
    real gain, xssq;
    integer i__, j, k;
    real noise[166], pulse;
    integer px;
    real sscale;
    extern integer random_(struct lpc10_decoder_state *);
    real xy, sum, ssq;
    real lpi0, hpi0;

/* $Log: bsynz.c,v $
/* Revision 1.1  2004/05/04 11:16:42  csoutheren
/* Initial version
/*
/* Revision 1.2  2002/02/15 03:57:55  yurik
/* Warnings removed during compilation, patch courtesy of Jehan Bing, jehan@bravobrava.com
/*
/* Revision 1.1  2000/06/05 04:45:12  robertj
/* Added LPC-10 2400bps codec
/*
 * Revision 1.2  1996/08/20  20:18:55  jaf
 * Removed all static local variables that were SAVE'd in the Fortran
 * code, and put them in struct lpc10_decoder_state that is passed as an
 * argument.
 *
 * Removed init function, since all initialization is now done in
 * init_lpc10_decoder_state().
 *
 * Revision 1.1  1996/08/19  22:32:58  jaf
 * Initial revision
 * */
/* Revision 1.3  1996/03/29  22:03:47  jaf */
/* Removed definitions for any constants that were no longer used. */

/* Revision 1.2  1996/03/26  19:34:33  jaf */
/* Added comments indicating which constants are not needed in an */
/* application that uses the LPC-10 coder. */

/* Revision 1.1  1996/02/07  14:43:51  jaf */
/* Initial revision */

/*   LPC Configuration parameters: */
/* Frame size, Prediction order, Pitch period */
/*       Arguments */
/* $Log: bsynz.c,v $
/* Revision 1.1  2004/05/04 11:16:42  csoutheren
/* Initial version
/*
/* Revision 1.2  2002/02/15 03:57:55  yurik
/* Warnings removed during compilation, patch courtesy of Jehan Bing, jehan@bravobrava.com
/*
/* Revision 1.1  2000/06/05 04:45:12  robertj
/* Added LPC-10 2400bps codec
/*
 * Revision 1.2  1996/08/20  20:18:55  jaf
 * Removed all static local variables that were SAVE'd in the Fortran
 * code, and put them in struct lpc10_decoder_state that is passed as an
 * argument.
 *
 * Removed init function, since all initialization is now done in
 * init_lpc10_decoder_state().
 *
 * Revision 1.1  1996/08/19  22:32:58  jaf
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
/*       Function return value definitions */
/* 	Parameters/constants */
/*       KEXC is not a Fortran PARAMETER, but it is an array initialized 
*/
/*       with a DATA statement that is never modified. */
/*       Local variables that need not be saved */
/*       NOISE is declared with range (1:MAXPIT+MAXORD), but only indices 
*/
/*       ORDER+1 through ORDER+IP are ever used, and I think that IP */
/*       .LE. MAXPIT.  Why not declare it to be in the range (1:MAXPIT) */
/*       and use that range? */
/*       Local state */
/*       I believe that only indices 1 through ORDER of EXC need to be */
/*       saved from one invocation to the next, but we may as well save */
/*       the whole array. */
/*       None of these local variables were given initial values in the */
/*       original code.  I'm guessing that 0 is a reasonable initial */
/*       value for all of them. */
    /* Parameter adjustments */
    if (coef) {
	--coef;
	}
    if (sout) {
	--sout;
	}

    /* Function Body */
    ipo = &(st->ipo);
    exc = &(st->exc[0]);
    exc2 = &(st->exc2[0]);
    lpi1 = &(st->lpi1);
    lpi2 = &(st->lpi2);
    lpi3 = &(st->lpi3);
    hpi1 = &(st->hpi1);
    hpi2 = &(st->hpi2);
    hpi3 = &(st->hpi3);
    rmso = &(st->rmso_bsynz);

/*                  MAXPIT+MAXORD=166 */
/*  Calculate history scale factor XY and scale filter state */
/* Computing MIN */
    r__1 = *rmso / (*rms + 1e-6f);
    xy = min(r__1,8.f);
    *rmso = *rms;
    i__1 = contrl_1.order;
    for (i__ = 1; i__ <= i__1; ++i__) {
	exc2[i__ - 1] = exc2[*ipo + i__ - 1] * xy;
    }
    *ipo = *ip;
    if (*iv == 0) {
/*  Generate white noise for unvoiced */
	i__1 = *ip;
	for (i__ = 1; i__ <= i__1; ++i__) {
	    exc[contrl_1.order + i__ - 1] = (real) (random_(st) / 64);
	}
/*  Impulse doublet excitation for plosives */
/*       (RANDOM()+32768) is in the range 0 to 2**16-1.  Therefore the
 */
/*       following expression should be evaluated using integers with 
at */
/*       least 32 bits (16 isn't enough), and PX should be in the rang
e */
/*       ORDER+1+0 through ORDER+1+(IP-2) .EQ. ORDER+IP-1. */
	px = (random_(st) + 32768) * (*ip - 1) / 65536 + contrl_1.order + 1;
	r__1 = *ratio / 4 * 1.f;
	pulse = r__1 * 342;
	if (pulse > 2e3f) {
	    pulse = 2e3f;
	}
	exc[px - 1] += pulse;
	exc[px] -= pulse;
/*  Load voiced excitation */
    } else {
	sscale = (real)sqrt((real) (*ip)) / 6.928f;
	i__1 = *ip;
	for (i__ = 1; i__ <= i__1; ++i__) {
	    exc[contrl_1.order + i__ - 1] = 0.f;
	    if (i__ <= 25) {
		exc[contrl_1.order + i__ - 1] = sscale * kexc[i__ - 1];
	    }
	    lpi0 = exc[contrl_1.order + i__ - 1];
	    r__2 = exc[contrl_1.order + i__ - 1] * .125f + *lpi1 * .75f;
	    r__1 = r__2 + *lpi2 * .125f;
	    exc[contrl_1.order + i__ - 1] = r__1 + *lpi3 * 0.f;
	    *lpi3 = *lpi2;
	    *lpi2 = *lpi1;
	    *lpi1 = lpi0;
	}
	i__1 = *ip;
	for (i__ = 1; i__ <= i__1; ++i__) {
	    noise[contrl_1.order + i__ - 1] = random_(st) * 1.f / 64;
	    hpi0 = noise[contrl_1.order + i__ - 1];
	    r__2 = noise[contrl_1.order + i__ - 1] * -.125f + *hpi1 * .25f;
	    r__1 = r__2 + *hpi2 * -.125f;
	    noise[contrl_1.order + i__ - 1] = r__1 + *hpi3 * 0.f;
	    *hpi3 = *hpi2;
	    *hpi2 = *hpi1;
	    *hpi1 = hpi0;
	}
	i__1 = *ip;
	for (i__ = 1; i__ <= i__1; ++i__) {
	    exc[contrl_1.order + i__ - 1] += noise[contrl_1.order + i__ - 1];
	}
    }
/*   Synthesis filters: */
/*    Modify the excitation with all-zero filter  1 + G*SUM */
    xssq = 0.f;
    i__1 = *ip;
    for (i__ = 1; i__ <= i__1; ++i__) {
	k = contrl_1.order + i__;
	sum = 0.f;
	i__2 = contrl_1.order;
	for (j = 1; j <= i__2; ++j) {
	    sum += coef[j] * exc[k - j - 1];
	}
	sum *= *g2pass;
	exc2[k - 1] = sum + exc[k - 1];
    }
/*   Synthesize using the all pole filter  1 / (1 - SUM) */
    i__1 = *ip;
    for (i__ = 1; i__ <= i__1; ++i__) {
	k = contrl_1.order + i__;
	sum = 0.f;
	i__2 = contrl_1.order;
	for (j = 1; j <= i__2; ++j) {
	    sum += coef[j] * exc2[k - j - 1];
	}
	exc2[k - 1] = sum + exc2[k - 1];
	xssq += exc2[k - 1] * exc2[k - 1];
    }
/*  Save filter history for next epoch */
    i__1 = contrl_1.order;
    for (i__ = 1; i__ <= i__1; ++i__) {
	exc[i__ - 1] = exc[*ip + i__ - 1];
	exc2[i__ - 1] = exc2[*ip + i__ - 1];
    }
/*  Apply gain to match RMS */
    r__1 = *rms * *rms;
    ssq = r__1 * *ip;
    gain = (real)sqrt(ssq / xssq);
    i__1 = *ip;
    for (i__ = 1; i__ <= i__1; ++i__) {
	sout[i__] = gain * exc2[contrl_1.order + i__ - 1];
    }
    return 0;
} /* bsynz_ */
