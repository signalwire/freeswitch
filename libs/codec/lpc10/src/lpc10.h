/*

$Log: lpc10.h,v $
Revision 1.1  2004/05/04 11:30:49  csoutheren
Initial version

Revision 1.1  2000/06/05 04:45:11  robertj
Added LPC-10 2400bps codec

 * Revision 1.1  1996/08/19  22:47:31  jaf
 * Initial revision
 *

*/

#ifndef __LPC10_H__
#define __LPC10_H__

#define LPC10_SAMPLES_PER_FRAME 180
#define LPC10_BITS_IN_COMPRESSED_FRAME 54


typedef short		INT16;
typedef int		INT32;


/* The initial values for every member of this structure is 0, except
   where noted in comments. */

/* These two lines are copied from f2c.h.  There should be a more
   elegant way of doing this than having the same declarations in two
   files. */

typedef float real;
typedef INT32 integer;
typedef INT32 logical;
typedef INT16 shortint;

struct lpc10_encoder_state {
    /* State used only by function hp100 */
    real z11;
    real z21;
    real z12;
    real z22;
    
    /* State used by function analys */
    real inbuf[540], pebuf[540];
    real lpbuf[696], ivbuf[312];
    real bias;
    integer osbuf[10];  /* no initial value necessary */
    integer osptr;     /* initial value 1 */
    integer obound[3];
    integer vwin[6]	/* was [2][3] */;   /* initial value vwin[4] = 307; vwin[5] = 462; */
    integer awin[6]	/* was [2][3] */;   /* initial value awin[4] = 307; awin[5] = 462; */
    integer voibuf[8]	/* was [2][4] */;
    real rmsbuf[3];
    real rcbuf[30]	/* was [10][3] */;
    real zpre;


    /* State used by function onset */
    real n;
    real d__;   /* initial value 1.f */
    real fpc;   /* no initial value necessary */
    real l2buf[16];
    real l2sum1;
    integer l2ptr1;   /* initial value 1 */
    integer l2ptr2;   /* initial value 9 */
    integer lasti;    /* no initial value necessary */
    logical hyst;   /* initial value FALSE_ */

    /* State used by function voicin */
    real dither;   /* initial value 20.f */
    real snr;
    real maxmin;
    real voice[6]	/* was [2][3] */;   /* initial value is probably unnecessary */
    integer lbve, lbue, fbve, fbue;
    integer ofbue, sfbue;
    integer olbue, slbue;
    /* Initial values:
	lbve = 3000;
	fbve = 3000;
	fbue = 187;
	ofbue = 187;
	sfbue = 187;
	lbue = 93;
	olbue = 93;
	slbue = 93;
	snr = (real) (fbve / fbue << 6);
	*/

    /* State used by function dyptrk */
    real s[60];
    integer p[120]	/* was [60][2] */;
    integer ipoint;
    real alphax;

    /* State used by function chanwr */
    integer isync;

};


struct lpc10_decoder_state {

    /* State used by function decode */
    integer iptold;   /* initial value 60 */
    logical first;   /* initial value TRUE_ */
    integer ivp2h;
    integer iovoic;
    integer iavgp;   /* initial value 60 */
    integer erate;
    integer drc[30]	/* was [3][10] */;
    integer dpit[3];
    integer drms[3];

    /* State used by function synths */
    real buf[360];
    integer buflen;   /* initial value 180 */

    /* State used by function pitsyn */
    integer ivoico;   /* no initial value necessary as long as first_pitsyn is initially TRUE_ */
    integer ipito;   /* no initial value necessary as long as first_pitsyn is initially TRUE_ */
    real rmso;   /* initial value 1.f */
    real rco[10];   /* no initial value necessary as long as first_pitsyn is initially TRUE_ */
    integer jsamp;   /* no initial value necessary as long as first_pitsyn is initially TRUE_ */
    logical first_pitsyn;   /* initial value TRUE_ */

    /* State used by function bsynz */
    integer ipo;
    real exc[166];
    real exc2[166];
    real lpi1;
    real lpi2;
    real lpi3;
    real hpi1;
    real hpi2;
    real hpi3;
    real rmso_bsynz;

    /* State used by function random */
    integer j;   /* initial value 2 */
    integer k;   /* initial value 5 */
    shortint y[5];  /* initial value { -21161,-8478,30892,-10216,16950 } */

    /* State used by function deemp */
    real dei1;
    real dei2;
    real deo1;
    real deo2;
    real deo3;

};



/*

  Calling sequence:

  Call create_lpc10_encoder_state(), which returns a pointer to an
  already initialized lpc10_encoder_state structure.

  lpc10_encode reads indices 0 through (LPC10_SAMPLES_PER_FRAME-1) of
  array speech[], and writes indices 0 through
  (LPC10_BITS_IN_COMPRESSED_FRAME-1) of array bits[], and both reads
  and writes the lpc10_encoder_state structure contents.  The
  lpc10_encoder_state structure should *not* be initialized for every
  frame of encoded speech.  Once at the beginning of execution, done
  automatically for you by create_lpc10_encoder_state(), is enough.

  init_lpc10_encoder_state() reinitializes the lpc10_encoder_state
  structure.  This might be useful if you are finished processing one
  sound sample, and want to reuse the same lpc10_encoder_state
  structure to process another sound sample.  There might be other
  uses as well.

  Note that the comments in the lpc10/lpcenc.c file imply that indices
  1 through 180 of array speech[] are read.  These comments were
  written for the Fortran version of the code, before it was
  automatically converted to C by the conversion program f2c.  f2c
  seems to use the convention that the pointers to arrays passed as
  function arguments point to the first index used in the Fortran
  code, whatever index that might be (usually 1), and then it modifies
  the pointer inside of the function, like so:

  if (speech) {
      --speech;
  }

  So that the code can access the first value at index 1 and the last
  at index 180.  This makes the translated C code "closer" to the
  original Fortran code.

  The calling sequence for the decoder is similar to the encoder.  The
  only significant difference is that the array bits[] is read
  (indices 0 through (LPC10_BITS_IN_COMPRESSED_FRAME-1)), and the
  array speech[] is written (indices 0 through
  (LPC10_SAMPLES_PER_FRAME-1)).
  
  */

struct lpc10_encoder_state * create_lpc10_encoder_state ();
void init_lpc10_encoder_state (struct lpc10_encoder_state *st);
int lpc10_encode (real *speech, INT32 *bits, struct lpc10_encoder_state *st);

struct lpc10_decoder_state * create_lpc10_decoder_state ();
void init_lpc10_decoder_state (struct lpc10_decoder_state *st);
int lpc10_decode (INT32 *bits, real *speech, struct lpc10_decoder_state *st);

#endif /* __LPC10_H__ */
