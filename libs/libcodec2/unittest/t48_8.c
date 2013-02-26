/* 
   t48_8.c
   David Rowe
   May 10 2012

   Unit test for 48 to 8 kHz sample rate conversion functions.  I
   evaluated output by plotting using Octave and looking for jaggies:

     pl("../unittest/out48.raw",1,3000)
     pl("../unittest/out8.raw",1,3000)

   Listening to it also shows up anything nasty:

     $ play -s -2 -r 48000 out48.raw
     $ play -s -2 -r 8000 out8.raw

  */

#include <assert.h>
#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include "fdmdv.h"

#define N8                        160 /* procssing buffer size at 8 kHz */
#define N48             (N8*FDMDV_OS)
#define MEM8 (FDMDV_OS_TAPS/FDMDV_OS)
#define FRAMES                     50
#define TWO_PI            6.283185307
#define FS                       8000

#define SINE

int main() {
    float in8k[MEM8 + N8];
    float out48k[N48];
    short out48k_short[N48];
    FILE *f48;

    float in48k[FDMDV_OS_TAPS + N48];
    float out8k[N48];
    short out8k_short[N8];
    FILE *f8;

    int i,f,t,t1;
    float freq = 800.0;

    f48 = fopen("out48.raw", "wb");
    assert(f48 != NULL);
    f8 = fopen("out8.raw", "wb");
    assert(f8 != NULL);
    
    /* clear filter memories */

    for(i=0; i<MEM8; i++)
	in8k[i] = 0.0;
    for(i=0; i<FDMDV_OS_TAPS; i++)
	in48k[i] = 0.0;

    t = t1 = 0;
    for(f=0; f<FRAMES; f++) {

#ifdef DC
	for(i=0; i<N8; i++)
	    in8k[MEM8+i] = 16000.0;
#endif
#ifdef SINE
	for(i=0; i<N8; i++,t++)
	    in8k[MEM8+i] = 16000.0*cos(TWO_PI*t*freq/FS);
#endif

	/* upsample  */

	fdmdv_8_to_48(out48k, &in8k[MEM8], N8);
	/*
	for(i=0; i<MEM8; i++)
	    in8k[i] = in8k[i+N8];
	*/

	/* save 48k to disk for plotting and check out */

	for(i=0; i<N48; i++)
	    out48k_short[i] = (short)out48k[i];
	fwrite(out48k_short, sizeof(short), N48, f48);
	
	/* add a 10 kHz spurious signal for fun, we want down sampler to
	   knock this out */

	for(i=0; i<N48; i++,t1++)
	    in48k[i+FDMDV_OS_TAPS] = out48k[i] + 16000.0*cos(TWO_PI*t1*1E4/FS);

	/* downsample */

	fdmdv_48_to_8(out8k, &in48k[FDMDV_OS_TAPS], N8);
	/*
	for(i=0; i<FDMDV_OS_TAPS; i++)
	    in48k[i] = in48k[i+N48];
	*/

	/* save 8k to disk for plotting and check out */

	for(i=0; i<N8; i++)
	    out8k_short[i] = (short)out8k[i];
	fwrite(out8k_short, sizeof(short), N8, f8);
	
    }

    fclose(f48);
    fclose(f8);
    return 0;

}
