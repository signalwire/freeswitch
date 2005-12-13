/*

$Log$
Revision 1.18  2003/10/21 18:08:11  markster
Fix include order

Revision 1.5  2003/10/21 18:08:11  markster
Fix include order

Revision 1.4  2003/10/21 02:57:29  markster
FreeBSD patch, take 2

Revision 1.3  2003/10/16 21:11:30  martinp
Revert the previous patch since it's braking compilation

Revision 1.1  2003/02/12 13:59:15  matteo
Initial revision

Revision 1.2  2000/01/05 08:20:39  markster
Some OSS fixes and a few lpc changes to make it actually work

 * Revision 1.2  1996/08/20  20:35:41  jaf
 * Added functions for allocating and initializing lpc10_encoder_state
 * and lpc10_decoder_state structures.
 *
 * Revision 1.1  1996/08/19  22:31:40  jaf
 * Initial revision
 *

*/

#ifdef P_R_O_T_O_T_Y_P_E_S
extern int lpcini_(void);
/* comlen contrl_ 12 */
/*:ref: initlpcenc_ 14 0 */
/*:ref: initlpcdec_ 14 0 */
#endif

/*  -- translated by f2c (version 19951025).
   You must link the resulting object file with the libraries:
	-lf2c -lm   (in that order)
*/

#include <stdlib.h>
#include "f2c.h"

/* Common Block Declarations */

struct {
    integer order, lframe;
    logical corrp;
} contrl_;

#define contrl_1 contrl_

/* ***************************************************************** */

/* $Log$
 * Revision 1.18  2003/10/21 18:08:11  markster
 * Fix include order
 *
/* Revision 1.5  2003/10/21 18:08:11  markster
/* Fix include order
/*
/* Revision 1.4  2003/10/21 02:57:29  markster
/* FreeBSD patch, take 2
/*
/* Revision 1.3  2003/10/16 21:11:30  martinp
/* Revert the previous patch since it's braking compilation
/*
/* Revision 1.1  2003/02/12 13:59:15  matteo
/* Initial revision
/*
/* Revision 1.2  2000/01/05 08:20:39  markster
/* Some OSS fixes and a few lpc changes to make it actually work
/*
 * Revision 1.2  1996/08/20  20:35:41  jaf
 * Added functions for allocating and initializing lpc10_encoder_state
 * and lpc10_decoder_state structures.
 *
 * Revision 1.1  1996/08/19  22:31:40  jaf
 * Initial revision
 * */
/* Revision 1.1  1996/03/28  00:04:05  jaf */
/* Initial revision */


/* ***************************************************************** */

/* Initialize COMMON block variables used by LPC-10 encoder and decoder, */
/* and call initialization routines for both of them. */

/* Subroutine */ int lpcini_(void)
{

/* $Log$
 * Revision 1.18  2003/10/21 18:08:11  markster
 * Fix include order
 *
/* Revision 1.5  2003/10/21 18:08:11  markster
/* Fix include order
/*
/* Revision 1.4  2003/10/21 02:57:29  markster
/* FreeBSD patch, take 2
/*
/* Revision 1.3  2003/10/16 21:11:30  martinp
/* Revert the previous patch since it's braking compilation
/*
/* Revision 1.1  2003/02/12 13:59:15  matteo
/* Initial revision
/*
/* Revision 1.2  2000/01/05 08:20:39  markster
/* Some OSS fixes and a few lpc changes to make it actually work
/*
 * Revision 1.2  1996/08/20  20:35:41  jaf
 * Added functions for allocating and initializing lpc10_encoder_state
 * and lpc10_decoder_state structures.
 *
 * Revision 1.1  1996/08/19  22:31:40  jaf
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
/* $Log$
 * Revision 1.18  2003/10/21 18:08:11  markster
 * Fix include order
 *
/* Revision 1.5  2003/10/21 18:08:11  markster
/* Fix include order
/*
/* Revision 1.4  2003/10/21 02:57:29  markster
/* FreeBSD patch, take 2
/*
/* Revision 1.3  2003/10/16 21:11:30  martinp
/* Revert the previous patch since it's braking compilation
/*
/* Revision 1.1  2003/02/12 13:59:15  matteo
/* Initial revision
/*
/* Revision 1.2  2000/01/05 08:20:39  markster
/* Some OSS fixes and a few lpc changes to make it actually work
/*
 * Revision 1.2  1996/08/20  20:35:41  jaf
 * Added functions for allocating and initializing lpc10_encoder_state
 * and lpc10_decoder_state structures.
 *
 * Revision 1.1  1996/08/19  22:31:40  jaf
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
    contrl_1.order = 10;
    contrl_1.lframe = 180;
    contrl_1.corrp = TRUE_;
    return 0;
} /* lpcini_ */



/* Allocate memory for, and initialize, the state that needs to be
   kept from encoding one frame to the next for a single
   LPC-10-compressed audio stream.  Return 0 if malloc fails,
   otherwise return pointer to new structure. */

struct lpc10_encoder_state *
create_lpc10_encoder_state()
{
    struct lpc10_encoder_state *st;

    st = (struct lpc10_encoder_state *)
	malloc((unsigned) sizeof (struct lpc10_encoder_state));
    if (st != 0) {
	init_lpc10_encoder_state(st);
    }
    return (st);
}



void init_lpc10_encoder_state(struct lpc10_encoder_state *st)
{
    int i;

    lpcini_();

    /* State used only by function hp100 */
    st->z11 = 0.0f;
    st->z21 = 0.0f;
    st->z12 = 0.0f;
    st->z22 = 0.0f;
    
    /* State used by function analys */
    for (i = 0; i < 540; i++) {
	st->inbuf[i] = 0.0f;
	st->pebuf[i] = 0.0f;
    }
    for (i = 0; i < 696; i++) {
	st->lpbuf[i] = 0.0f;
    }
    for (i = 0; i < 312; i++) {
	st->ivbuf[i] = 0.0f;
    }
    st->bias = 0.0f;
    /* integer osbuf[10];  /* no initial value necessary */
    st->osptr = 1;
    for (i = 0; i < 3; i++) {
	st->obound[i] = 0;
    }
    st->vwin[4] = 307;
    st->vwin[5] = 462;
    st->awin[4] = 307;
    st->awin[5] = 462;
    for (i = 0; i < 8; i++) {
	st->voibuf[i] = 0;
    }
    for (i = 0; i < 3; i++) {
	st->rmsbuf[i] = 0.0f;
    }
    for (i = 0; i < 30; i++) {
	st->rcbuf[i] = 0.0f;
    }
    st->zpre = 0.0f;


    /* State used by function onset */
    st->n = 0.0f;
    st->d__ = 1.0f;
    /* real fpc;   /* no initial value necessary */
    for (i = 0; i < 16; i++) {
	st->l2buf[i] = 0.0f;
    }
    st->l2sum1 = 0.0f;
    st->l2ptr1 = 1;
    st->l2ptr2 = 9;
    /* integer lasti;    /* no initial value necessary */
    st->hyst = FALSE_;

    /* State used by function voicin */
    st->dither = 20.0f;
    st->maxmin = 0.0f;
    for (i = 0; i < 6; i++) {
	st->voice[i] = 0.0f;
    }
    st->lbve = 3000;
    st->fbve = 3000;
    st->fbue = 187;
    st->ofbue = 187;
    st->sfbue = 187;
    st->lbue = 93;
    st->olbue = 93;
    st->slbue = 93;
    st->snr = (real) (st->fbve / st->fbue << 6);

    /* State used by function dyptrk */
    for (i = 0; i < 60; i++) {
	st->s[i] = 0.0f;
    }
    for (i = 0; i < 120; i++) {
	st->p[i] = 0;
    }
    st->ipoint = 0;
    st->alphax = 0.0f;

    /* State used by function chanwr */
    st->isync = 0;

}



/* Allocate memory for, and initialize, the state that needs to be
   kept from decoding one frame to the next for a single
   LPC-10-compressed audio stream.  Return 0 if malloc fails,
   otherwise return pointer to new structure. */

struct lpc10_decoder_state *
create_lpc10_decoder_state()
{
    struct lpc10_decoder_state *st;

    st = (struct lpc10_decoder_state *)
	malloc((unsigned) sizeof (struct lpc10_decoder_state));
    if (st != 0) {
	init_lpc10_decoder_state(st);
    }
    return (st);
}



void init_lpc10_decoder_state(struct lpc10_decoder_state *st)
{
    int i;

    lpcini_();

    /* State used by function decode */
    st->iptold = 60;
    st->first = TRUE_;
    st->ivp2h = 0;
    st->iovoic = 0;
    st->iavgp = 60;
    st->erate = 0;
    for (i = 0; i < 30; i++) {
	st->drc[i] = 0;
    }
    for (i = 0; i < 3; i++) {
	st->dpit[i] = 0;
	st->drms[i] = 0;
    }

    /* State used by function synths */
    for (i = 0; i < 360; i++) {
	st->buf[i] = 0.0f;
    }
    st->buflen = 180;

    /* State used by function pitsyn */
    /* ivoico;   /* no initial value necessary as long as first_pitsyn is initially TRUE_ */
    /* ipito;   /* no initial value necessary as long as first_pitsyn is initially TRUE_ */
    st->rmso = 1.0f;
    /* rco[10];   /* no initial value necessary as long as first_pitsyn is initially TRUE_ */
    /* integer jsamp;   /* no initial value necessary as long as first_pitsyn is initially TRUE_ */
    st->first_pitsyn = TRUE_;

    /* State used by function bsynz */
    st->ipo = 0;
    for (i = 0; i < 166; i++) {
	st->exc[i] = 0.0f;
	st->exc2[i] = 0.0f;
    }
    st->lpi1 = 0.0f;
    st->lpi2 = 0.0f;
    st->lpi3 = 0.0f;
    st->hpi1 = 0.0f;
    st->hpi2 = 0.0f;
    st->hpi3 = 0.0f;
    st->rmso_bsynz = 0.0f;

    /* State used by function random */
    st->j = 2;
    st->k = 5;
    st->y[0] = (shortint) -21161;
    st->y[1] = (shortint) -8478;
    st->y[2] = (shortint) 30892;
    st->y[3] = (shortint) -10216;
    st->y[4] = (shortint) 16950;

    /* State used by function deemp */
    st->dei1 = 0.0f;
    st->dei2 = 0.0f;
    st->deo1 = 0.0f;
    st->deo2 = 0.0f;
    st->deo3 = 0.0f;
}
