/*

$Log: decode_.c,v $
Revision 1.1  2004/05/04 11:16:42  csoutheren
Initial version

Revision 1.2  2002/02/15 03:57:55  yurik
Warnings removed during compilation, patch courtesy of Jehan Bing, jehan@bravobrava.com

Revision 1.1  2000/06/05 04:45:12  robertj
Added LPC-10 2400bps codec

 * Revision 1.2  1996/08/20  20:22:39  jaf
 * Removed all static local variables that were SAVE'd in the Fortran
 * code, and put them in struct lpc10_decoder_state that is passed as an
 * argument.
 *
 * Removed init function, since all initialization is now done in
 * init_lpc10_decoder_state().
 *
 * Revision 1.1  1996/08/19  22:32:38  jaf
 * Initial revision
 *

*/

#ifdef P_R_O_T_O_T_Y_P_E_S
extern int decode_(integer *ipitv, integer *irms, integer *irc, integer *voice, integer *pitch, real *rms, real *rc, struct lpc10_decoder_state *st);
/* comlen contrl_ 12 */
/*:ref: ham84_ 14 3 4 4 4 */
/*:ref: median_ 4 3 4 4 4 */
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

static integer c__2 = 2;

/* ***************************************************************** */

/* 	DECODE Version 54 */

/* $Log: decode_.c,v $
/* Revision 1.1  2004/05/04 11:16:42  csoutheren
/* Initial version
/*
/* Revision 1.2  2002/02/15 03:57:55  yurik
/* Warnings removed during compilation, patch courtesy of Jehan Bing, jehan@bravobrava.com
/*
/* Revision 1.1  2000/06/05 04:45:12  robertj
/* Added LPC-10 2400bps codec
/*
 * Revision 1.2  1996/08/20  20:22:39  jaf
 * Removed all static local variables that were SAVE'd in the Fortran
 * code, and put them in struct lpc10_decoder_state that is passed as an
 * argument.
 *
 * Removed init function, since all initialization is now done in
 * init_lpc10_decoder_state().
 *
 * Revision 1.1  1996/08/19  22:32:38  jaf
 * Initial revision
 * */
/* Revision 1.5  1996/05/23  20:06:03  jaf */
/* Assigned PITCH a "default" value on the first call, since otherwise it */
/* would be left uninitialized. */

/* Revision 1.4  1996/03/26  19:35:18  jaf */
/* Commented out trace statements. */

/* Revision 1.3  1996/03/21  21:10:50  jaf */
/* Added entry INITDECODE to reinitialize the local state of subroutine */
/* DECODE. */

/* Revision 1.2  1996/03/21  21:04:50  jaf */
/* Determined which local variables should be saved from one invocation */
/* to the next, and guessed initial values for some that should have been */
/* saved, but weren't given initial values.  Many of the arrays are */
/* "constants", and many local variables are only used if the "global" */
/* variable CORRP is .TRUE. */

/* Added comments explaining which indices of array arguments are read or */
/* written. */

/* Revision 1.1  1996/02/12 03:21:10  jaf */
/* Initial revision */


/* ***************************************************************** */

/*   This subroutine provides error correction and decoding */
/*   for all LPC parameters */

/* Input: */
/*  IPITV  - Index value of pitch */
/*  IRMS   - Coded Energy */
/*  CORRP  - Error correction: */
/*    If FALSE, parameters are decoded directly with no delay.  If TRUE, */
/*    most important parameter bits are protected by Hamming code and */
/*    median smoothed.  This requires an additional frame of delay. */
/* Input/Output: */
/*  IRC    - Coded Reflection Coefficients */
/*           Indices 1 through ORDER always read, then written. */
/* Output: */
/*  VOICE  - Half frame voicing decisions */
/*           Indices 1 through 2 written. */
/*  PITCH  - Decoded pitch */
/*  RMS    - Energy */
/*  RC     - Reflection coefficients */
/*           Indices 1 through ORDER written. */

/*  NOTE: Zero RC's should be done more directly, but this would affect */
/*   coded parameter printout. */

/* This subroutine maintains local state from one call to the next.  If */
/* you want to switch to using a new audio stream for this filter, or */
/* reinitialize its state for any other reason, call the ENTRY */
/* INITDECODE. */

/* Subroutine */ int decode_(integer *ipitv, integer *irms, 
	integer *irc, integer *voice, integer *pitch, real *rms, real *rc,
			     struct lpc10_decoder_state *st)
{
    /* Initialized data */

    logical *first;
    static integer ethrs = 2048;
    static integer ethrs1 = 128;
    static integer ethrs2 = 1024;
    static integer ethrs3 = 2048;
    static integer ivtab[32] = { 24960,24960,24960,24960,25480,25480,25483,
	    25480,16640,1560,1560,1560,16640,1816,1563,1560,24960,24960,24859,
	    24856,26001,25881,25915,25913,1560,1560,7800,3640,1561,1561,3643,
	    3641 };
    static real corth[32]	/* was [4][8] */ = { 32767.f,10.f,5.f,0.f,
	    32767.f,8.f,4.f,0.f,32.f,6.4f,3.2f,0.f,32.f,6.4f,3.2f,0.f,32.f,
	    11.2f,6.4f,0.f,32.f,11.2f,6.4f,0.f,16.f,5.6f,3.2f,0.f,16.f,5.6f,
	    3.2f,0.f };
    static integer detau[128] = { 0,0,0,3,0,3,3,31,0,3,3,21,3,3,29,30,0,3,3,
	    20,3,25,27,26,3,23,58,22,3,24,28,3,0,3,3,3,3,39,33,32,3,37,35,36,
	    3,38,34,3,3,42,46,44,50,40,48,3,54,3,56,3,52,3,3,1,0,3,3,108,3,78,
	    100,104,3,84,92,88,156,80,96,3,3,74,70,72,66,76,68,3,62,3,60,3,64,
	    3,3,1,3,116,132,112,148,152,3,3,140,3,136,3,144,3,3,1,124,120,128,
	    3,3,3,3,1,3,3,3,1,3,1,1,1 };
    static integer rmst[64] = { 1024,936,856,784,718,656,600,550,502,460,420,
	    384,352,328,294,270,246,226,206,188,172,158,144,132,120,110,102,
	    92,84,78,70,64,60,54,50,46,42,38,34,32,30,26,24,22,20,18,17,16,15,
	    14,13,12,11,10,9,8,7,6,5,4,3,2,1,0 };
    static integer detab7[32] = { 4,11,18,25,32,39,46,53,60,66,72,77,82,87,92,
	    96,101,104,108,111,114,115,117,119,121,122,123,124,125,126,127,
	    127 };
    static real descl[8] = { .6953f,.625f,.5781f,.5469f,.5312f,.5391f,.4688f,
	    .3828f };
    integer *ivp2h;
    static integer deadd[8] = { 1152,-2816,-1536,-3584,-1280,-2432,768,-1920 }
	    ;
    static integer qb[8] = { 511,511,1023,1023,1023,1023,2047,4095 };
    static integer nbit[10] = { 8,8,5,5,4,4,4,4,3,2 };
    static integer zrc[10] = { 0,0,0,0,0,3,0,2,0,0 };
    static integer bit[5] = { 2,4,8,16,32 };
    integer *iovoic;
    integer *iavgp;
    integer *iptold;
    integer *erate;
    integer *drc;
    integer *dpit;
    integer *drms;

    /* System generated locals */
    integer i__1, i__2;

    /* Builtin functions */
    integer pow_ii(integer *, integer *);

    /* Local variables */
    extern /* Subroutine */ int ham84_(integer *, integer *, integer *);
    integer ipit, iout, i__, icorf, index, ivoic, ixcor, i1, i2, i4;
    extern integer median_(integer *, integer *, integer *);
    integer ishift, errcnt, lsb;

/* $Log: decode_.c,v $
/* Revision 1.1  2004/05/04 11:16:42  csoutheren
/* Initial version
/*
/* Revision 1.2  2002/02/15 03:57:55  yurik
/* Warnings removed during compilation, patch courtesy of Jehan Bing, jehan@bravobrava.com
/*
/* Revision 1.1  2000/06/05 04:45:12  robertj
/* Added LPC-10 2400bps codec
/*
 * Revision 1.2  1996/08/20  20:22:39  jaf
 * Removed all static local variables that were SAVE'd in the Fortran
 * code, and put them in struct lpc10_decoder_state that is passed as an
 * argument.
 *
 * Removed init function, since all initialization is now done in
 * init_lpc10_decoder_state().
 *
 * Revision 1.1  1996/08/19  22:32:38  jaf
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
/* $Log: decode_.c,v $
/* Revision 1.1  2004/05/04 11:16:42  csoutheren
/* Initial version
/*
/* Revision 1.2  2002/02/15 03:57:55  yurik
/* Warnings removed during compilation, patch courtesy of Jehan Bing, jehan@bravobrava.com
/*
/* Revision 1.1  2000/06/05 04:45:12  robertj
/* Added LPC-10 2400bps codec
/*
 * Revision 1.2  1996/08/20  20:22:39  jaf
 * Removed all static local variables that were SAVE'd in the Fortran
 * code, and put them in struct lpc10_decoder_state that is passed as an
 * argument.
 *
 * Removed init function, since all initialization is now done in
 * init_lpc10_decoder_state().
 *
 * Revision 1.1  1996/08/19  22:32:38  jaf
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

/*       Parameters/constants */

/*       The variables below that are not Fortran PARAMETER's are */
/*       initialized with DATA statements, and then never modified. */
/*       The following are used regardless of CORRP's value. */

/*       DETAU, NBIT, QB, DEADD, DETAB7, RMST, DESCL */

/*       The following are used only if CORRP is .TRUE. */

/*       ETHRS, ETHRS1, ETHRS2, ETHRS3, IVTAB, BIT, CORTH, ZRC */

/*       Local variables that need not be saved */

/*       The following are used regardless of CORRP's value */
/*       The following are used only if CORRP is .TRUE. */

/*       Local state */

/*       The following are used regardless of CORRP's value */
/*       The following are used only if CORRP is .TRUE. */
/*       I am guessing the initial values for IVP2H, IOVOIC, DRC, DPIT, */
/*       and DRMS.  They should be checked to see if they are reasonable. 
*/
/*       I'm also guessing for ERATE, but I think 0 is the right initial 
*/
/*       value. */
    /* Parameter adjustments */
    if (irc) {
	--irc;
	}
    if (voice) {
	--voice;
	}
    if (rc) {
	--rc;
	}

    /* Function Body */

    iptold = &(st->iptold);
    first = &(st->first);
    ivp2h = &(st->ivp2h);
    iovoic = &(st->iovoic);
    iavgp = &(st->iavgp);
    erate = &(st->erate);
    drc = &(st->drc[0]);
    dpit = &(st->dpit[0]);
    drms = &(st->drms[0]);

/* DATA statements for "constants" defined above. */
/* 	IF (LISTL.GE.3) WRITE(FDEBUG,800) IPITV,IRMS,(IRC(J),J=1,ORDER) */
/* 800	FORMAT(1X,' <<ERRCOR IN>>',T32,6X,I6,I5,T50,10I8) */
/*  If no error correction, do pitch and voicing then jump to decode */
    i4 = detau[*ipitv];
    if (! contrl_1.corrp) {
	voice[1] = 1;
	voice[2] = 1;
	if (*ipitv <= 1) {
	    voice[1] = 0;
	}
	if (*ipitv == 0 || *ipitv == 2) {
	    voice[2] = 0;
	}
	*pitch = i4;
	if (*pitch <= 4) {
	    *pitch = *iptold;
	}
	if (voice[1] == 1 && voice[2] == 1) {
	    *iptold = *pitch;
	}
	if (voice[1] != voice[2]) {
	    *pitch = *iptold;
	}
	goto L900;
    }
/*  Do error correction pitch and voicing */
    if (i4 > 4) {
	dpit[0] = i4;
	ivoic = 2;
	*iavgp = (*iavgp * 15 + i4 + 8) / 16;
    } else {
	ivoic = i4;
	dpit[0] = *iavgp;
    }
    drms[0] = *irms;
    i__1 = contrl_1.order;
    for (i__ = 1; i__ <= i__1; ++i__) {
	drc[i__ * 3 - 3] = irc[i__];
    }
/*  Determine index to IVTAB from V/UV decision */
/*  If error rate is high then use alternate table */
    index = (*ivp2h << 4) + (*iovoic << 2) + ivoic + 1;
    i1 = ivtab[index - 1];
    ipit = i1 & 3;
    icorf = i1 / 8;
    if (*erate < ethrs) {
	icorf /= 64;
    }
/*  Determine error rate:  4=high    1=low */
    ixcor = 4;
    if (*erate < ethrs3) {
	ixcor = 3;
    }
    if (*erate < ethrs2) {
	ixcor = 2;
    }
    if (*erate < ethrs1) {
	ixcor = 1;
    }
/*  Voice/unvoice decision determined from bits 0 and 1 of IVTAB */
    voice[1] = icorf / 2 & 1;
    voice[2] = icorf & 1;
/*  Skip decoding on first frame because present data not yet available */
    if (*first) {
	*first = FALSE_;
/*          Assign PITCH a "default" value on the first call, since */
/*          otherwise it would be left uninitialized.  The two lines 
*/
/*          below were copied from above, since it seemed like a */
/*          reasonable thing to do for the first call. */
	*pitch = i4;
	if (*pitch <= 4) {
	    *pitch = *iptold;
	}
	goto L500;
    }
/*  If bit 4 of ICORF is set then correct RMS and RC(1) - RC(4). */
/*    Determine error rate and correct errors using a Hamming 8,4 code */
/*    during transition or unvoiced frame.  If IOUT is negative, */
/*    more than 1 error occurred, use previous frame's parameters. */
    if ((icorf & bit[3]) != 0) {
	errcnt = 0;
	lsb = drms[1] & 1;
	index = (drc[22] << 4) + drms[1] / 2;
	ham84_(&index, &iout, &errcnt);
	drms[1] = drms[2];
	if (iout >= 0) {
	    drms[1] = (iout << 1) + lsb;
	}
	for (i__ = 1; i__ <= 4; ++i__) {
	    if (i__ == 1) {
		i1 = ((drc[25] & 7) << 1) + (drc[28] & 1);
	    } else {
		i1 = drc[(9 - i__) * 3 - 2] & 15;
	    }
	    i2 = drc[(5 - i__) * 3 - 2] & 31;
	    lsb = i2 & 1;
	    index = (i1 << 4) + i2 / 2;
	    ham84_(&index, &iout, &errcnt);
	    if (iout >= 0) {
		iout = (iout << 1) + lsb;
		if ((iout & 16) == 16) {
		    iout += -32;
		}
	    } else {
		iout = drc[(5 - i__) * 3 - 1];
	    }
	    drc[(5 - i__) * 3 - 2] = iout;
	}
/*  Determine error rate */
	*erate = (integer)(*erate * .96875f + errcnt * 102);
    }
/*  Get unsmoothed RMS, RC's, and PITCH */
    *irms = drms[1];
    i__1 = contrl_1.order;
    for (i__ = 1; i__ <= i__1; ++i__) {
	irc[i__] = drc[i__ * 3 - 2];
    }
    if (ipit == 1) {
	dpit[1] = dpit[2];
    }
    if (ipit == 3) {
	dpit[1] = dpit[0];
    }
    *pitch = dpit[1];
/*  If bit 2 of ICORF is set then smooth RMS and RC's, */
    if ((icorf & bit[1]) != 0) {
	if ((i__1 = drms[1] - drms[0], (real) abs(i__1)) >= corth[ixcor + 3] 
		&& (i__2 = drms[1] - drms[2], (real) abs(i__2)) >= corth[
		ixcor + 3]) {
	    *irms = median_(&drms[2], &drms[1], drms);
	}
	for (i__ = 1; i__ <= 6; ++i__) {
	    if ((i__1 = drc[i__ * 3 - 2] - drc[i__ * 3 - 3], (real) abs(i__1))
		     >= corth[ixcor + (i__ + (2 << 2)) - 5] && (i__2 = drc[i__ *
		     3 - 2] - drc[i__ * 3 - 1], (real) abs(i__2)) >= corth[
		    ixcor + (i__ + (2 << 2)) - 5]) {
		irc[i__] = median_(&drc[i__ * 3 - 1], &drc[i__ * 3 - 2], &drc[
			i__ * 3 - 3]);
	    }
	}
    }
/*  If bit 3 of ICORF is set then smooth pitch */
    if ((icorf & bit[2]) != 0) {
	if ((i__1 = dpit[1] - dpit[0], (real) abs(i__1)) >= corth[ixcor - 1] 
		&& (i__2 = dpit[1] - dpit[2], (real) abs(i__2)) >= corth[
		ixcor - 1]) {
	    *pitch = median_(&dpit[2], &dpit[1], dpit);
	}
    }
/*  If bit 5 of ICORF is set then RC(5) - RC(10) are loaded with */
/*  values so that after quantization bias is removed in decode */
/*  the values will be zero. */
L500:
    if ((icorf & bit[4]) != 0) {
	i__1 = contrl_1.order;
	for (i__ = 5; i__ <= i__1; ++i__) {
	    irc[i__] = zrc[i__ - 1];
	}
    }
/*  House keeping  - one frame delay */
    *iovoic = ivoic;
    *ivp2h = voice[2];
    dpit[2] = dpit[1];
    dpit[1] = dpit[0];
    drms[2] = drms[1];
    drms[1] = drms[0];
    i__1 = contrl_1.order;
    for (i__ = 1; i__ <= i__1; ++i__) {
	drc[i__ * 3 - 1] = drc[i__ * 3 - 2];
	drc[i__ * 3 - 2] = drc[i__ * 3 - 3];
    }
L900:
/* 	IF (LISTL.GE.3)WRITE(FDEBUG,801)VOICE,PITCH,IRMS,(IRC(J),J=1,ORDER) */
/* 801	FORMAT(1X,'<<ERRCOR OUT>>',T32,2I3,I6,I5,T50,10I8) */
/*   Decode RMS */
    *irms = rmst[(31 - *irms) * 2];
/*  Decode RC(1) and RC(2) from log-area-ratios */
/*  Protect from illegal coded value (-16) caused by bit errors */
    for (i__ = 1; i__ <= 2; ++i__) {
	i2 = irc[i__];
	i1 = 0;
	if (i2 < 0) {
	    i1 = 1;
	    i2 = -i2;
	    if (i2 > 15) {
		i2 = 0;
	    }
	}
	i2 = detab7[i2 * 2];
	if (i1 == 1) {
	    i2 = -i2;
	}
	ishift = 15 - nbit[i__ - 1];
	irc[i__] = i2 * pow_ii(&c__2, &ishift);
    }
/*  Decode RC(3)-RC(10) to sign plus 14 bits */
    i__1 = contrl_1.order;
    for (i__ = 3; i__ <= i__1; ++i__) {
	i2 = irc[i__];
	ishift = 15 - nbit[i__ - 1];
	i2 *= pow_ii(&c__2, &ishift);
	i2 += qb[i__ - 3];
	irc[i__] = (integer)(i2 * descl[i__ - 3] + deadd[i__ - 3]);
    }
/* 	IF (LISTL.GE.3) WRITE(FDEBUG,811) IRMS, (IRC(I),I=1,ORDER) */
/* 811	FORMAT(1X,'<<DECODE OUT>>',T45,I4,1X,10I8) */
/*  Scale RMS and RC's to reals */
    *rms = (real) (*irms);
    i__1 = contrl_1.order;
    for (i__ = 1; i__ <= i__1; ++i__) {
	rc[i__] = irc[i__] / 16384.f;
    }
    return 0;
} /* decode_ */
