/*

$Log: synths.c,v $
Revision 1.1  2004/05/04 11:16:43  csoutheren
Initial version

Revision 1.1  2000/06/05 04:45:12  robertj
Added LPC-10 2400bps codec

 * Revision 1.2  1996/08/20  20:42:59  jaf
 * Removed all static local variables that were SAVE'd in the Fortran
 * code, and put them in struct lpc10_decoder_state that is passed as an
 * argument.
 *
 * Removed init function, since all initialization is now done in
 * init_lpc10_decoder_state().
 *
 * Revision 1.1  1996/08/19  22:30:33  jaf
 * Initial revision
 *

*/

#ifdef P_R_O_T_O_T_Y_P_E_S
extern int synths_(integer *voice, integer *pitch, real *rms, real *rc, real *speech, integer *k, struct lpc10_decoder_state *st);
/* comlen contrl_ 12 */
/*:ref: pitsyn_ 14 12 4 4 4 6 6 4 4 4 6 6 4 6 */
/*:ref: irc2pc_ 14 5 6 6 4 6 6 */
/*:ref: bsynz_ 14 7 6 4 4 6 6 6 6 */
/*:ref: deemp_ 14 2 6 4 */
/*:ref: initpitsyn_ 14 0 */
/*:ref: initbsynz_ 14 0 */
/*:ref: initdeemp_ 14 0 */
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

/* Table of constant values */

static real c_b2 = .7f;

/* ***************************************************************** */

/* 	SYNTHS Version 54 */

/* $Log: synths.c,v $
/* Revision 1.1  2004/05/04 11:16:43  csoutheren
 * Initial version
 *
 * Revision 1.1  2000/06/05 04:45:12  robertj
 * Added LPC-10 2400bps codec
 *
 * Revision 1.2  1996/08/20  20:42:59  jaf
 * Removed all static local variables that were SAVE'd in the Fortran
 * code, and put them in struct lpc10_decoder_state that is passed as an
 * argument.
 *
 * Removed init function, since all initialization is now done in
 * init_lpc10_decoder_state().
 *
 * Revision 1.1  1996/08/19  22:30:33  jaf
 * Initial revision
 *
 */
/* Revision 1.5  1996/03/26  19:31:58  jaf */
/* Commented out trace statements. */

/* Revision 1.4  1996/03/25  19:41:01  jaf */
/* Changed so that MAXFRM samples are always returned in the output array */
/* SPEECH. */

/* This required delaying the returned samples by MAXFRM sample times, */
/* and remembering any "left over" samples returned by PITSYN from one */
/* call of SYNTHS to the next. */

/* Changed size of SPEECH from 2*MAXFRM to MAXFRM.  Removed local */
/* variable SOUT.  Added local state variables BUF and BUFLEN. */

/* Revision 1.3  1996/03/25  19:20:10  jaf */
/* Added comments about the range of possible return values for argument */
/* K, and increased the size of the arrays filled in by PITSYN from 11 to */
/* 16, as has been already done inside of PITSYN. */

/* Revision 1.2  1996/03/22  00:18:18  jaf */
/* Added comments explaining meanings of input and output parameters, and */
/* indicating which array indices can be read or written. */

/* Added entry INITSYNTHS, which does nothing except call the */
/* corresponding initialization entries for subroutines PITSYN, BSYNZ, */
/* and DEEMP. */

/* Revision 1.1  1996/02/07 14:49:44  jaf */
/* Initial revision */


/* ***************************************************************** */

/* The note below is from the distributed version of the LPC10 coder. */
/* The version of the code below has been modified so that SYNTHS always */
/* has a constant frame length output of MAXFRM. */

/* Also, BSYNZ and DEEMP need not be modified to work on variable */
/* positions within an array.  It is only necessary to pass the first */
/* index desired as the array argument.  What actually gets passed is the */
/* address of that array position, which the subroutine treats as the */
/* first index of the array. */

/* This technique is used in subroutine ANALYS when calling PREEMP, so it */
/* appears that multiple people wrote different parts of this LPC10 code, */
/* and that they didn't necessarily have equivalent knowledge of Fortran */
/* (not surprising). */

/*  NOTE: There is excessive buffering here, BSYNZ and DEEMP should be */
/*        changed to operate on variable positions within SOUT.  Also, */
/*        the output length parameter is bogus, and PITSYN should be */
/*        rewritten to allow a constant frame length output. */

/* Input: */
/*  VOICE  - Half frame voicing decisions */
/*           Indices 1 through 2 read. */
/* Input/Output: */
/*  PITCH  - Pitch */
/*           PITCH is restricted to range 20 to 156, inclusive, */
/*           before calling subroutine PITSYN, and then PITSYN */
/*           can modify it further under some conditions. */
/*  RMS    - Energy */
/*           Only use is for debugging, and passed to PITSYN. */
/*           See comments there for how it can be modified. */
/*  RC     - Reflection coefficients */
/*           Indices 1 through ORDER restricted to range -.99 to .99, */
/*           before calling subroutine PITSYN, and then PITSYN */
/*           can modify it further under some conditions. */
/* Output: */
/*  SPEECH - Synthesized speech samples. */
/*           Indices 1 through the final value of K are written. */
/*  K      - Number of samples placed into array SPEECH. */
/*           This is always MAXFRM. */

/* Subroutine */ int synths_(integer *voice, integer *pitch, real *
	rms, real *rc, real *speech, integer *k, struct lpc10_decoder_state *st)
{
    /* Initialized data */

    real *buf;
    integer *buflen;

    /* System generated locals */
    integer i__1;
    real r__1, r__2;

    /* Local variables */
    real rmsi[16];
    integer nout, ivuv[16], i__, j;
    extern /* Subroutine */ int deemp_(real *, integer *, struct lpc10_decoder_state *);
    real ratio;
    integer ipiti[16];
    extern /* Subroutine */ bsynz_(real *, integer *, 
	    integer *, real *, real *, real *, real *, struct lpc10_decoder_state *), irc2pc_(real *, real *
	    , integer *, real *, real *);
    real g2pass;
    real pc[10];
    extern /* Subroutine */ int pitsyn_(integer *, integer *, integer *, real 
	    *, real *, integer *, integer *, integer *, real *, real *, 
	    integer *, real *, struct lpc10_decoder_state *);
    real rci[160]	/* was [10][16] */;

/* $Log: synths.c,v $
 * Revision 1.1  2004/05/04 11:16:43  csoutheren
 * Initial version
 *
 * Revision 1.1  2000/06/05 04:45:12  robertj
 * Added LPC-10 2400bps codec
 *
 * Revision 1.2  1996/08/20  20:42:59  jaf
 * Removed all static local variables that were SAVE'd in the Fortran
 * code, and put them in struct lpc10_decoder_state that is passed as an
 * argument.
 *
 * Removed init function, since all initialization is now done in
 * init_lpc10_decoder_state().
 *
 * Revision 1.1  1996/08/19  22:30:33  jaf
 * Initial revision
 *
 */
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
/* $Log: synths.c,v $
 * Revision 1.1  2004/05/04 11:16:43  csoutheren
 * Initial version
 *
 * Revision 1.1  2000/06/05 04:45:12  robertj
 * Added LPC-10 2400bps codec
 *
 * Revision 1.2  1996/08/20  20:42:59  jaf
 * Removed all static local variables that were SAVE'd in the Fortran
 * code, and put them in struct lpc10_decoder_state that is passed as an
 * argument.
 *
 * Removed init function, since all initialization is now done in
 * init_lpc10_decoder_state().
 *
 * Revision 1.1  1996/08/19  22:30:33  jaf
 * Initial revision
 *
 */
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
/*       Parameters/constants */
/*       Local variables that need not be saved */
/*       Local state */
/*       BUF is a buffer of speech samples that would have been returned 
*/
/*       by the older version of SYNTHS, but the newer version doesn't, */
/*       so that the newer version can always return MAXFRM samples on */
/*       every call.  This has the effect of delaying the return of */
/*       samples for one additional frame time. */

/*       Indices 1 through BUFLEN contain samples that are left over from 
*/
/*       the last call to SYNTHS.  Given the way that PITSYN works, */
/*       BUFLEN should always be in the range MAXFRM-MAXPIT+1 through */
/*       MAXFRM, inclusive, after a call to SYNTHS is complete. */

/*       On the first call to SYNTHS (or the first call after */
/*       reinitializing with the entry INITSYNTHS), BUFLEN is MAXFRM, and 
*/
/*       a frame of silence is always returned. */
    /* Parameter adjustments */
    if (voice) {
	--voice;
	}
    if (rc) {
	--rc;
	}
    if (speech) {
	--speech;
	}

    /* Function Body */
    buf = &(st->buf[0]);
    buflen = &(st->buflen);

/* Computing MAX */
    i__1 = min(*pitch,156);
    *pitch = max(i__1,20);
    i__1 = contrl_1.order;
    for (i__ = 1; i__ <= i__1; ++i__) {
/* Computing MAX */
/* Computing MIN */
	r__2 = rc[i__];
	r__1 = min(r__2,.99f);
	rc[i__] = max(r__1,-.99f);
    }
    pitsyn_(&contrl_1.order, &voice[1], pitch, rms, &rc[1], &contrl_1.lframe, 
	    ivuv, ipiti, rmsi, rci, &nout, &ratio, st);
    if (nout > 0) {
	i__1 = nout;
	for (j = 1; j <= i__1; ++j) {

/*             Add synthesized speech for pitch period J to the en
d of */
/*             BUF. */

	    irc2pc_(&rci[j * 10 - 10], pc, &contrl_1.order, &c_b2, &g2pass);
	    bsynz_(pc, &ipiti[j - 1], &ivuv[j - 1], &buf[*buflen], &rmsi[j - 1]
		    , &ratio, &g2pass, st);
	    deemp_(&buf[*buflen], &ipiti[j - 1], st);
	    *buflen += ipiti[j - 1];
	}

/*          Copy first MAXFRM samples from BUF to output array SPEECH 
*/
/*          (scaling them), and then remove them from the beginning of
 */
/*          BUF. */

	for (i__ = 1; i__ <= 180; ++i__) {
	    speech[i__] = buf[i__ - 1] / 4096.f;
	}
	*k = 180;
	*buflen += -180;
	i__1 = *buflen;
	for (i__ = 1; i__ <= i__1; ++i__) {
	    buf[i__ - 1] = buf[i__ + 179];
	}
    }
    return 0;
} /* synths_ */
