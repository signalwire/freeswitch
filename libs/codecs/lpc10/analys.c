/*

$Log$
Revision 1.16  2004/06/26 03:50:14  markster
Merge source cleanups (bug #1911)

Revision 1.15  2003/09/19 01:20:22  markster
Code cleanups (bug #66)

Revision 1.2  2003/09/19 01:20:22  markster
Code cleanups (bug #66)

Revision 1.1.1.1  2003/02/12 13:59:14  matteo
mer feb 12 14:56:57 CET 2003

Revision 1.2  2000/01/05 08:20:39  markster
Some OSS fixes and a few lpc changes to make it actually work

 * Revision 1.2  1996/08/20  20:16:01  jaf
 * Removed all static local variables that were SAVE'd in the Fortran
 * code, and put them in struct lpc10_encoder_state that is passed as an
 * argument.
 *
 * Removed init function, since all initialization is now done in
 * init_lpc10_encoder_state().
 *
 * Revision 1.1  1996/08/19  22:29:08  jaf
 * Initial revision
 *

*/

#include "f2c.h"

#ifdef P_R_O_T_O_T_Y_P_E_S
extern int analys_(real *speech, integer *voice, integer *pitch, real *rms, real *rc, struct lpc10_encoder_state *st);
/* comlen contrl_ 12 */
/*:ref: preemp_ 14 5 6 6 4 6 6 */
/*:ref: onset_ 14 7 6 4 4 4 4 4 4 */
/*:ref: placev_ 14 11 4 4 4 4 4 4 4 4 4 4 4 */
/*:ref: lpfilt_ 14 4 6 6 4 4 */
/*:ref: ivfilt_ 14 5 6 6 4 4 6 */
/*:ref: tbdm_ 14 8 6 4 4 4 6 4 4 4 */
/*:ref: voicin_ 14 12 4 6 6 4 4 6 6 4 6 4 4 4 */
/*:ref: dyptrk_ 14 6 6 4 4 4 4 4 */
/*:ref: placea_ 14 9 4 4 4 4 4 4 4 4 4 */
/*:ref: dcbias_ 14 3 4 6 6 */
/*:ref: energy_ 14 3 4 6 6 */
/*:ref: mload_ 14 6 4 4 4 6 6 6 */
/*:ref: invert_ 14 4 4 6 6 6 */
/*:ref: rcchk_ 14 3 4 6 6 */
/*:ref: initonset_ 14 0 */
/*:ref: initvoicin_ 14 0 */
/*:ref: initdyptrk_ 14 0 */
/* Rerunning f2c -P may change prototypes or declarations. */
#endif

/*  -- translated by f2c (version 19951025).
   You must link the resulting object file with the libraries:
	-lf2c -lm   (in that order)
*/

/* Common Block Declarations */

extern struct {
    integer order, lframe;
    logical corrp;
} contrl_;

#define contrl_1 contrl_

/* Table of constant values */

static integer c__10 = 10;
static integer c__181 = 181;
static integer c__720 = 720;
static integer c__3 = 3;
static integer c__90 = 90;
static integer c__156 = 156;
static integer c__307 = 307;
static integer c__462 = 462;
static integer c__312 = 312;
static integer c__60 = 60;
static integer c__1 = 1;

/* ****************************************************************** */

/* 	ANALYS Version 55 */

/* $Log$
 * Revision 1.16  2004/06/26 03:50:14  markster
 * Merge source cleanups (bug #1911)
 *
/* Revision 1.15  2003/09/19 01:20:22  markster
/* Code cleanups (bug #66)
/*
/* Revision 1.2  2003/09/19 01:20:22  markster
/* Code cleanups (bug #66)
/*
/* Revision 1.1.1.1  2003/02/12 13:59:14  matteo
/* mer feb 12 14:56:57 CET 2003
/*
/* Revision 1.2  2000/01/05 08:20:39  markster
/* Some OSS fixes and a few lpc changes to make it actually work
/*
 * Revision 1.2  1996/08/20  20:16:01  jaf
 * Removed all static local variables that were SAVE'd in the Fortran
 * code, and put them in struct lpc10_encoder_state that is passed as an
 * argument.
 *
 * Removed init function, since all initialization is now done in
 * init_lpc10_encoder_state().
 *
 * Revision 1.1  1996/08/19  22:29:08  jaf
 * Initial revision
 * */
/* Revision 1.9  1996/05/23  19:41:07  jaf */
/* Commented out some unnecessary lines that were reading uninitialized */
/* values. */

/* Revision 1.8  1996/03/27  23:57:55  jaf */
/* Added some comments about which indices of the local buffers INBUF, */
/* LPBUF, etc., get read or modified by some of the subroutine calls.  I */
/* just did this while trying to figure out the discrepancy between the */
/* embedded code compiled with all local variables implicitly saved, and */
/* without. */

/* I added some debugging write statements in hopes of finding a problem. */
/* None of them ever printed anything while running with the long input */
/* speech file dam9.spd provided in the distribution. */

/* Revision 1.7  1996/03/27  18:06:20  jaf */
/* Commented out access to MAXOSP, which is just a debugging variable */
/* that was defined in the COMMON block CONTRL in contrl.fh. */

/* Revision 1.6  1996/03/26  19:31:33  jaf */
/* Commented out trace statements. */

/* Revision 1.5  1996/03/21  15:19:35  jaf */
/* Added comments for ENTRY PITDEC. */

/* Revision 1.4  1996/03/19  20:54:27  jaf */
/* Added a line to INITANALYS.  See comments there. */

/* Revision 1.3  1996/03/19  20:52:49  jaf */
/* Rearranged the order of the local variables quite a bit, to separate */
/* them into groups of "constants", "locals that don't need to be saved */
/* from one call to the next", and "local that do need to be saved from */
/* one call to the next". */

/* Several locals in the last set should have been given initial values, */
/* but weren't.  I gave them all initial values of 0. */

/* Added a separate ENTRY INITANALYS that initializes all local state */
/* that should be, and also calls the corresponding entries of the */
/* subroutines called by ANALYS that also have local state. */

/* There used to be DATA statements in ANALYS.  I got rid of most of */
/* them, and added a local logical variable FIRST that calls the entry */
/* INITANALYS on the first call to ANALYS.  This is just so that one need */
/* not remember to call INITANALYS first in order for the state to be */
/* initialized. */

/* Revision 1.2  1996/03/11  23:29:32  jaf */
/* Added several comments with my own personal questions about the */
/* Fortran 77 meaning of the parameters passed to the subroutine PREEMP. */

/* Revision 1.1  1996/02/07  14:42:29  jaf */
/* Initial revision */


/* ****************************************************************** */

/* SUBROUTINE ANALYS */

/* Input: */
/*  SPEECH */
/*       Indices 1 through LFRAME read. */
/* Output: */
/*  VOICE */
/*       Indices 1 through 2 written. */
/*  PITCH */
/*       Written in subroutine DYPTRK, and then perhaps read and written */
/*       some more. */
/*  RMS */
/*       Written. */
/*  RC */
/*       Indices 1 through ORDER written (ORDER defined in contrl.fh). */

/* This subroutine maintains local state from one call to the next.  If */
/* you want to switch to using a new audio stream for this filter, or */
/* reinitialize its state for any other reason, call the ENTRY */
/* INITANALYS. */


/* ENTRY PITDEC */

/* Input: */
/*  PITCH   - Encoded pitch index */
/* Output: */
/*  PTAU    - Decoded pitch period */

/* This entry has no local state.  It accesses a "constant" array */
/* declared in ANALYS. */

/* Subroutine */ int analys_(real *speech, integer *voice, integer 
	*pitch, real *rms, real *rc, struct lpc10_encoder_state *st)
{
    /* Initialized data */

    static integer tau[60] = { 20,21,22,23,24,25,26,27,28,29,30,31,32,33,34,
	    35,36,37,38,39,40,42,44,46,48,50,52,54,56,58,60,62,64,66,68,70,72,
	    74,76,78,80,84,88,92,96,100,104,108,112,116,120,124,128,132,136,
	    140,144,148,152,156 };
    static integer buflim[4] = { 181,720,25,720 };
    static real precoef = .9375f;

    /* System generated locals */
    integer i__1;

    /* Local variables */
    real amdf[60];
    integer half;
    real abuf[156];
    real *bias;
    extern /* Subroutine */ int tbdm_(real *, integer *, integer *, integer *,
	     real *, integer *, integer *, integer *);
    integer *awin;
    integer midx, ewin[6]	/* was [2][3] */;
    real ivrc[2], temp;
    real *zpre;
    integer *vwin;
    integer i__, j, lanal;
    extern /* Subroutine */ int rcchk_(integer *, real *, real *), mload_(
	    integer *, integer *, integer *, real *, real *, real *);
    real *inbuf, *pebuf;
    real *lpbuf, *ivbuf;
    real *rcbuf;
    integer *osbuf;
    extern /* Subroutine */ int onset_(real *, integer *, integer *, integer *
	    , integer *, integer *, integer *, struct lpc10_encoder_state *);
    integer *osptr;
    extern int dcbias_(integer *, real *, real *);
    integer ipitch;
    integer *obound;
    extern /* Subroutine */ int preemp_(real *, real *, integer *, real *, 
	    real *), voicin_(integer *, real *, real *, integer *, integer *, 
	    real *, real *, integer *, real *, integer *, integer *, integer *,
	    struct lpc10_encoder_state *);
    integer *voibuf;
    integer mintau;
    real *rmsbuf;
    extern /* Subroutine */ int lpfilt_(real *, real *, integer *, integer *),
	     ivfilt_(real *, real *, integer *, integer *, real *), energy_(
	    integer *, real *, real *), invert_(integer *, real *, real *, 
	    real *);
    integer minptr, maxptr;
    extern /* Subroutine */ int dyptrk_(real *, integer *, integer *, integer 
	    *, integer *, integer *, struct lpc10_encoder_state *);
    real phi[100]	/* was [10][10] */, psi[10];

/* $Log$
 * Revision 1.16  2004/06/26 03:50:14  markster
 * Merge source cleanups (bug #1911)
 *
/* Revision 1.15  2003/09/19 01:20:22  markster
/* Code cleanups (bug #66)
/*
/* Revision 1.2  2003/09/19 01:20:22  markster
/* Code cleanups (bug #66)
/*
/* Revision 1.1.1.1  2003/02/12 13:59:14  matteo
/* mer feb 12 14:56:57 CET 2003
/*
/* Revision 1.2  2000/01/05 08:20:39  markster
/* Some OSS fixes and a few lpc changes to make it actually work
/*
 * Revision 1.2  1996/08/20  20:16:01  jaf
 * Removed all static local variables that were SAVE'd in the Fortran
 * code, and put them in struct lpc10_encoder_state that is passed as an
 * argument.
 *
 * Removed init function, since all initialization is now done in
 * init_lpc10_encoder_state().
 *
 * Revision 1.1  1996/08/19  22:29:08  jaf
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
/*       Arguments to ANALYS */
/* $Log$
 * Revision 1.16  2004/06/26 03:50:14  markster
 * Merge source cleanups (bug #1911)
 *
/* Revision 1.15  2003/09/19 01:20:22  markster
/* Code cleanups (bug #66)
/*
/* Revision 1.2  2003/09/19 01:20:22  markster
/* Code cleanups (bug #66)
/*
/* Revision 1.1.1.1  2003/02/12 13:59:14  matteo
/* mer feb 12 14:56:57 CET 2003
/*
/* Revision 1.2  2000/01/05 08:20:39  markster
/* Some OSS fixes and a few lpc changes to make it actually work
/*
 * Revision 1.2  1996/08/20  20:16:01  jaf
 * Removed all static local variables that were SAVE'd in the Fortran
 * code, and put them in struct lpc10_encoder_state that is passed as an
 * argument.
 *
 * Removed init function, since all initialization is now done in
 * init_lpc10_encoder_state().
 *
 * Revision 1.1  1996/08/19  22:29:08  jaf
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
/*       Arguments to entry PITDEC (below) */
/* 	Parameters/constants */
/*  Constants */
/*    NF =     Number of frames */
/*    AF =     Frame in which analysis is done */
/*    OSLEN =  Length of the onset buffer */
/*    LTAU =   Number of pitch lags */
/*    SBUFL, SBUFH =   Start and end index of speech buffers */
/*    LBUFL, LBUFH =   Start and end index of LPF speech buffer */
/*   MINWIN, MAXWIN = Min and Max length of voicing (and analysis) windows
*/
/*    PWLEN, PWINH, PWINL = Length, upper and lower limits of pitch window
 */
/*    DVWINL, DVWINH = Default lower and upper limits of voicing window */
/*       The tables TAU and BUFLIM, and the variable PRECOEF, are not */
/*       Fortran PARAMETER's, but they are initialized with DATA */
/*       statements, and never modified.  Thus, they need not have SAVE */
/*       statements for them to keep their values from one invocation to 
*/
/*       the next. */
/*       Local variables that need not be saved */
/*       Local state */
/*  Data Buffers */
/*    INBUF	Raw speech (with DC bias removed each frame) */
/*    PEBUF	Preemphasized speech */
/*    LPBUF	Low pass speech buffer */
/*    IVBUF	Inverse filtered speech */
/*    OSBUF	Indexes of onsets in speech buffers */
/*    VWIN	Voicing window indices */
/*    AWIN	Analysis window indices */
/*    EWIN	Energy window indices */
/*    VOIBUF	Voicing decisions on windows in VWIN */
/*    RMSBUF	RMS energy */
/*    RCBUF	Reflection Coefficients */

/*  Pitch is handled separately from the above parameters. */
/*  The following variables deal with pitch: */
/*    MIDX	Encoded initial pitch estimate for analysis frame */
/*    IPITCH	Initial pitch computed for frame AF (decoded from MIDX) */
/*    PITCH 	The encoded pitch value (index into TAU) for the present */
/* 		frame (delayed and smoothed by Dyptrack) */
    /* Parameter adjustments */
    if (speech) {
	--speech;
	}
    if (voice) {
	--voice;
	}
    if (rc) {
	--rc;
	}

    /* Function Body */

/*   Calculations are done on future frame due to requirements */
/*   of the pitch tracker.  Delay RMS and RC's 2 frames to give */
/*   current frame parameters on return. */
/*   Update all buffers */

    inbuf = &(st->inbuf[0]);
    pebuf = &(st->pebuf[0]);
    lpbuf = &(st->lpbuf[0]);
    ivbuf = &(st->ivbuf[0]);
    bias = &(st->bias);
    osbuf = &(st->osbuf[0]);
    osptr = &(st->osptr);
    obound = &(st->obound[0]);
    vwin = &(st->vwin[0]);
    awin = &(st->awin[0]);
    voibuf = &(st->voibuf[0]);
    rmsbuf = &(st->rmsbuf[0]);
    rcbuf = &(st->rcbuf[0]);
    zpre = &(st->zpre);

    i__1 = 720 - contrl_1.lframe;
    for (i__ = 181; i__ <= i__1; ++i__) {
	inbuf[i__ - 181] = inbuf[contrl_1.lframe + i__ - 181];
	pebuf[i__ - 181] = pebuf[contrl_1.lframe + i__ - 181];
    }
    i__1 = 540 - contrl_1.lframe;
    for (i__ = 229; i__ <= i__1; ++i__) {
	ivbuf[i__ - 229] = ivbuf[contrl_1.lframe + i__ - 229];
    }
    i__1 = 720 - contrl_1.lframe;
    for (i__ = 25; i__ <= i__1; ++i__) {
	lpbuf[i__ - 25] = lpbuf[contrl_1.lframe + i__ - 25];
    }
    j = 1;
    i__1 = (*osptr) - 1;
    for (i__ = 1; i__ <= i__1; ++i__) {
	if (osbuf[i__ - 1] > contrl_1.lframe) {
	    osbuf[j - 1] = osbuf[i__ - 1] - contrl_1.lframe;
	    ++j;
	}
    }
    *osptr = j;
    voibuf[0] = voibuf[2];
    voibuf[1] = voibuf[3];
    for (i__ = 1; i__ <= 2; ++i__) {
	vwin[(i__ << 1) - 2] = vwin[((i__ + 1) << 1) - 2] - contrl_1.lframe;
	vwin[(i__ << 1) - 1] = vwin[((i__ + 1) << 1) - 1] - contrl_1.lframe;
	awin[(i__ << 1) - 2] = awin[((i__ + 1) << 1) - 2] - contrl_1.lframe;
	awin[(i__ << 1) - 1] = awin[((i__ + 1) << 1) - 1] - contrl_1.lframe;
/*       EWIN(*,J) is unused for J .NE. AF, so the following shift is 
*/
/*       unnecessary.  It also causes error messages when the C versio
n */
/*       of the code created from this by f2c is run with Purify.  It 
*/
/*       correctly complains that uninitialized memory is being read. 
*/
/* 	   EWIN(1,I) = EWIN(1,I+1) - LFRAME */
/* 	   EWIN(2,I) = EWIN(2,I+1) - LFRAME */
	obound[i__ - 1] = obound[i__];
	voibuf[i__ * 2] = voibuf[(i__ + 1) * 2];
	voibuf[(i__ << 1) + 1] = voibuf[((i__ + 1) << 1) + 1];
	rmsbuf[i__ - 1] = rmsbuf[i__];
	i__1 = contrl_1.order;
	for (j = 1; j <= i__1; ++j) {
	    rcbuf[j + i__ * 10 - 11] = rcbuf[j + (i__ + 1) * 10 - 11];
	}
    }
/*   Copy input speech, scale to sign+12 bit integers */
/*   Remove long term DC bias. */
/*       If the average value in the frame was over 1/4096 (after current 
*/
/*       BIAS correction), then subtract that much more from samples in */
/*       next frame.  If the average value in the frame was under */
/*       -1/4096, add 1/4096 more to samples in next frame.  In all other 
*/
/*       cases, keep BIAS the same. */
    temp = 0.f;
    i__1 = contrl_1.lframe;
    for (i__ = 1; i__ <= i__1; ++i__) {
	inbuf[720 - contrl_1.lframe + i__ - 181] = speech[i__] * 4096.f - 
		(*bias);
	temp += inbuf[720 - contrl_1.lframe + i__ - 181];
    }
    if (temp > (real) contrl_1.lframe) {
	*bias += 1;
    }
    if (temp < (real) (-contrl_1.lframe)) {
	*bias += -1;
    }
/*   Place Voicing Window */
    i__ = 721 - contrl_1.lframe;
    preemp_(&inbuf[i__ - 181], &pebuf[i__ - 181], &contrl_1.lframe, &precoef, 
	    zpre);
    onset_(pebuf, osbuf, osptr, &c__10, &c__181, &c__720, &contrl_1.lframe, st);

/*       MAXOSP is just a debugging variable. */

/* 	MAXOSP = MAX( MAXOSP, OSPTR ) */

    placev_(osbuf, osptr, &c__10, &obound[2], vwin, &c__3, &contrl_1.lframe, 
	    &c__90, &c__156, &c__307, &c__462);
/*        The Pitch Extraction algorithm estimates the pitch for a frame 
*/
/*   of speech by locating the minimum of the average magnitude difference
 */
/*   function (AMDF).  The AMDF operates on low-pass, inverse filtered */
/*   speech.  (The low-pass filter is an 800 Hz, 19 tap, equiripple, FIR 
*/
/*   filter and the inverse filter is a 2nd-order LPC filter.)  The pitch 
*/
/*   estimate is later refined by dynamic programming (DYPTRK).  However, 
*/
/*   since some of DYPTRK's parameters are a function of the voicing */
/*  decisions, a voicing decision must precede the final pitch estimation.
*/
/*   See subroutines LPFILT, IVFILT, and TBDM. */
/*       LPFILT reads indices LBUFH-LFRAME-29 = 511 through LBUFH = 720 */
/*       of INBUF, and writes indices LBUFH+1-LFRAME = 541 through LBUFH 
*/
/*       = 720 of LPBUF. */
    lpfilt_(&inbuf[228], &lpbuf[384], &c__312, &contrl_1.lframe);
/*       IVFILT reads indices (PWINH-LFRAME-7) = 353 through PWINH = 540 
*/
/*       of LPBUF, and writes indices (PWINH-LFRAME+1) = 361 through */
/*       PWINH = 540 of IVBUF. */
    ivfilt_(&lpbuf[204], ivbuf, &c__312, &contrl_1.lframe, ivrc);
/*       TBDM reads indices PWINL = 229 through */
/*       (PWINL-1)+MAXWIN+(TAU(LTAU)-TAU(1))/2 = 452 of IVBUF, and writes 
*/
/*       indices 1 through LTAU = 60 of AMDF. */
    tbdm_(ivbuf, &c__156, tau, &c__60, amdf, &minptr, &maxptr, &mintau);
/*        Voicing decisions are made for each half frame of input speech. 
*/
/*   An initial voicing classification is made for each half of the */
/*   analysis frame, and the voicing decisions for the present frame */
/*   are finalized.  See subroutine VOICIN. */
/*        The voicing detector (VOICIN) classifies the input signal as */
/*   unvoiced (including silence) or voiced using the AMDF windowed */
/*   maximum-to-minimum ratio, the zero crossing rate, energy measures, */
/*   reflection coefficients, and prediction gains. */
/*        The pitch and voicing rules apply smoothing and isolated */
/*   corrections to the pitch and voicing estimates and, in the process, 
*/
/*   introduce two frames of delay into the corrected pitch estimates and 
*/
/*   voicing decisions. */
    for (half = 1; half <= 2; ++half) {
	voicin_(&vwin[4], inbuf, lpbuf, buflim, &half, &amdf[minptr - 1], &
		amdf[maxptr - 1], &mintau, ivrc, obound, voibuf, &c__3, st);
    }
/*   Find the minimum cost pitch decision over several frames */
/*   given the current voicing decision and the AMDF array */
    dyptrk_(amdf, &c__60, &minptr, &voibuf[7], pitch, &midx, st);
    ipitch = tau[midx - 1];
/*   Place spectrum analysis and energy windows */
    placea_(&ipitch, voibuf, &obound[2], &c__3, vwin, awin, ewin, &
	    contrl_1.lframe, &c__156);
/*  Remove short term DC bias over the analysis window, Put result in ABUF
*/
    lanal = awin[5] + 1 - awin[4];
    dcbias_(&lanal, &pebuf[awin[4] - 181], abuf);
/*       ABUF(1:LANAL) is now defined.  It is equal to */
/*       PEBUF(AWIN(1,AF):AWIN(2,AF)) corrected for short term DC bias. */
/*   Compute RMS over integer number of pitch periods within the */
/*   analysis window. */
/*   Note that in a hardware implementation this computation may be */
/*   simplified by using diagonal elements of PHI computed by MLOAD. */
    i__1 = ewin[5] - ewin[4] + 1;
    energy_(&i__1, &abuf[ewin[4] - awin[4]], &rmsbuf[2]);
/*   Matrix load and invert, check RC's for stability */
    mload_(&contrl_1.order, &c__1, &lanal, abuf, phi, psi);
    invert_(&contrl_1.order, phi, psi, &rcbuf[20]);
    rcchk_(&contrl_1.order, &rcbuf[10], &rcbuf[20]);
/*   Set return parameters */
    voice[1] = voibuf[2];
    voice[2] = voibuf[3];
    *rms = rmsbuf[0];
    i__1 = contrl_1.order;
    for (i__ = 1; i__ <= i__1; ++i__) {
	rc[i__] = rcbuf[i__ - 1];
    }
    return 0;
} /* analys_ */
