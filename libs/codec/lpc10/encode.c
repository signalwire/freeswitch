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

 * Revision 1.1  1996/08/19  22:32:21  jaf
 * Initial revision
 *

*/

/*  -- translated by f2c (version 19951025).
   You must link the resulting object file with the libraries:
	-lf2c -lm   (in that order)
*/

#include "f2c.h"

#ifdef P_R_O_T_O_T_Y_P_E_S
extern int encode_(integer *voice, integer *pitch, real *rms, real *rc, integer *ipitch, integer *irms, integer *irc);
/* comlen contrl_ 12 */
#endif

/* Common Block Declarations */

extern struct {
    integer order, lframe;
    logical corrp;
} contrl_;

#define contrl_1 contrl_

/* Table of constant values */

static integer c__2 = 2;

/* ***************************************************************** */

/* 	ENCODE Version 54 */

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
 * Revision 1.1  1996/08/19  22:32:21  jaf
 * Initial revision
 * */
/* Revision 1.5  1996/03/26  19:35:50  jaf */
/* Commented out trace statements. */

/* Revision 1.4  1996/03/21  00:26:29  jaf */
/* Added the comment that this subroutine has no local state. */

/* In the last check-in, I forgot to mention that I had added comments */
/* explaining which indices of array arguments are read or written. */

/* Revision 1.3  1996/03/21  00:22:39  jaf */
/* Added comments explaining that all local arrays are effectively */
/* constants. */

/* Revision 1.2  1996/03/13  18:48:33  jaf */
/* Comments added explaining that none of the local variables of this */
/* subroutine need to be saved from one invocation to the next. */

/* Revision 1.1  1996/02/07 14:45:29  jaf */
/* Initial revision */


/* ***************************************************************** */

/*  Quantize LPC parameters for transmission */

/* INPUTS: */
/*  VOICE  - Half frame voicing decisions */
/*           Indices 1 through 2 read. */
/*  PITCH  - Pitch */
/*  RMS    - Energy */
/*  RC     - Reflection coefficients */
/*           Indices 1 through ORDER read. */
/*  CORRP  - Error Correction: TRUE = yes, FALSE = none */
/*           (this is defined in file control.fh) */
/* OUTPUTS: */
/*  IPITCH - Coded pitch and voicing */
/*  IRMS   - Quantized energy */
/*  IRC    - Quantized reflection coefficients */
/*           Indices 1 through MAX(ORDER,2) written. */
/*           If CORRP is .TRUE., then indices 1 through 10 written */
/*           for unvoiced frames. */

/* This subroutine has no local state. */

/* Subroutine */ int encode_(integer *voice, integer *pitch, real *rms, real *
	rc, integer *ipitch, integer *irms, integer *irc)
{
    /* Initialized data */

    static integer enctab[16] = { 0,7,11,12,13,10,6,1,14,9,5,2,3,4,8,15 };
    static integer entau[60] = { 19,11,27,25,29,21,23,22,30,14,15,7,39,38,46,
	    42,43,41,45,37,53,49,51,50,54,52,60,56,58,26,90,88,92,84,86,82,83,
	    81,85,69,77,73,75,74,78,70,71,67,99,97,113,112,114,98,106,104,108,
	    100,101,76 };
    static integer enadd[8] = { 1920,-768,2432,1280,3584,1536,2816,-1152 };
    static real enscl[8] = { .0204f,.0167f,.0145f,.0147f,.0143f,.0135f,.0125f,
	    .0112f };
    static integer enbits[8] = { 6,5,4,4,4,4,3,3 };
    static integer entab6[64] = { 0,0,0,0,0,0,1,1,1,1,1,1,1,2,2,2,2,2,2,2,3,3,
	    3,3,3,3,3,4,4,4,4,4,4,4,5,5,5,5,5,6,6,6,6,6,7,7,7,7,7,8,8,8,8,9,9,
	    9,10,10,11,11,12,13,14,15 };
    static integer rmst[64] = { 1024,936,856,784,718,656,600,550,502,460,420,
	    384,352,328,294,270,246,226,206,188,172,158,144,132,120,110,102,
	    92,84,78,70,64,60,54,50,46,42,38,34,32,30,26,24,22,20,18,17,16,15,
	    14,13,12,11,10,9,8,7,6,5,4,3,2,1,0 };

    /* System generated locals */
    integer i__1, i__2;

    /* Builtin functions */
    integer pow_ii(integer *, integer *);

    /* Local variables */
    integer idel, nbit, i__, j, i2, i3, mrk;

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
 * Revision 1.1  1996/08/19  22:32:21  jaf
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
 * Revision 1.1  1996/08/19  22:32:21  jaf
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
/*       Parameters/constants */
/*       These arrays are not Fortran PARAMETER's, but they are defined */
/*       by DATA statements below, and their contents are never altered. 
*/
/*       Local variables that need not be saved */
    /* Parameter adjustments */
    --irc;
    --rc;
    --voice;

    /* Function Body */
/*  Scale RMS and RC's to integers */
    *irms = *rms;
    i__1 = contrl_1.order;
    for (i__ = 1; i__ <= i__1; ++i__) {
	irc[i__] = rc[i__] * 32768.f;
    }
/* 	IF(LISTL.GE.3)WRITE(FDEBUG,800)VOICE,PITCH,IRMS,(IRC(I),I=1,ORDER) */
/* 800	FORMAT(1X,/,' <<ENCODE IN>>',T32,2I3,I6,I5,T50,10I8) */
/*  Encode pitch and voicing */
    if (voice[1] != 0 && voice[2] != 0) {
	*ipitch = entau[*pitch - 1];
    } else {
	if (contrl_1.corrp) {
	    *ipitch = 0;
	    if (voice[1] != voice[2]) {
		*ipitch = 127;
	    }
	} else {
	    *ipitch = (voice[1] << 1) + voice[2];
	}
    }
/*  Encode RMS by binary table search */
    j = 32;
    idel = 16;
    *irms = min(*irms,1023);
    while(idel > 0) {
	if (*irms > rmst[j - 1]) {
	    j -= idel;
	}
	if (*irms < rmst[j - 1]) {
	    j += idel;
	}
	idel /= 2;
    }
    if (*irms > rmst[j - 1]) {
	--j;
    }
    *irms = 31 - j / 2;
/*  Encode RC(1) and (2) as log-area-ratios */
    for (i__ = 1; i__ <= 2; ++i__) {
	i2 = irc[i__];
	mrk = 0;
	if (i2 < 0) {
	    i2 = -i2;
	    mrk = 1;
	}
	i2 /= 512;
	i2 = min(i2,63);
	i2 = entab6[i2];
	if (mrk != 0) {
	    i2 = -i2;
	}
	irc[i__] = i2;
    }
/*  Encode RC(3) - (10) linearly, remove bias then scale */
    i__1 = contrl_1.order;
    for (i__ = 3; i__ <= i__1; ++i__) {
	i2 = irc[i__] / 2;
	i2 = (i2 + enadd[contrl_1.order + 1 - i__ - 1]) * enscl[
		contrl_1.order + 1 - i__ - 1];
/* Computing MIN */
	i__2 = max(i2,-127);
	i2 = min(i__2,127);
	nbit = enbits[contrl_1.order + 1 - i__ - 1];
	i3 = 0;
	if (i2 < 0) {
	    i3 = -1;
	}
	i2 /= pow_ii(&c__2, &nbit);
	if (i3 == -1) {
	    --i2;
	}
	irc[i__] = i2;
    }
/*          Protect the most significant bits of the most */
/*     important parameters during non-voiced frames. */
/*     RC(1) - RC(4) are protected using 20 parity bits */
/*     replacing RC(5) - RC(10). */
    if (contrl_1.corrp) {
	if (*ipitch == 0 || *ipitch == 127) {
	    irc[5] = enctab[(irc[1] & 30) / 2];
	    irc[6] = enctab[(irc[2] & 30) / 2];
	    irc[7] = enctab[(irc[3] & 30) / 2];
	    irc[8] = enctab[(*irms & 30) / 2];
	    irc[9] = enctab[(irc[4] & 30) / 2] / 2;
	    irc[10] = enctab[(irc[4] & 30) / 2] & 1;
	}
    }
/* 	IF(LISTL.GE.3)WRITE(FDEBUG,801)VOICE,IPITCH,IRMS,(IRC(J),J=1,ORDER) */
/* 801	FORMAT(1X,'<<ENCODE OUT>>',T32,2I3,I6,I5,T50,10I8) */
    return 0;
} /* encode_ */

