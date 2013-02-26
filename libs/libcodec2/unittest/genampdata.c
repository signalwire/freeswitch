/*
  genampdata.c

  Generates test sparse amplitude data for vqtrainsp testing.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <ctype.h>
#include <assert.h>
#include "../src/defines.h"

typedef struct {
    float real;
    float imag;
} COMP;

#define NVEC 200000
#define D    2
#define E    8

int main(void) {
    FILE *f=fopen("testamp.flt", "wb");
    int   i, j, m, L, index;
    float amp, noisey_amp, pitch, Wo;
    float sparse_pe[MAX_AMP];

    #ifdef TEST1
    /* D fixed amplitude vectors of E elements long,
       with D=2, E=8:

       $ ./vqtrainsp testamp.flt 2 8 test.txt

       test.txt should be same as training data.
    */
    for(i=0; i<E; i++) {
	amp = i+1;
	for(j=0; j<D; j++)
	    fwrite(&amp, sizeof(float), 1, f);
    }
    #endif

    #ifdef TEST2
    /* 
       Bunch of amps uniformly distributed between -1 and 1.  With e
       entry "codebook" (1 dimensional vector or scalar):

       $ ./vqtrainsp testamp.flt 1 e test.txt


       should get std dev of 1/(e*sqrt(3))
    */

    for(i=0; i<NVEC; i++) {
	amp = 1.0 - 2.0*rand()/RAND_MAX;
	fwrite(&amp, sizeof(float), 1, f);
    }
    #endif

    #define TEST3
    #ifdef TEST3
    /* 
       Data for testing training of spare amplitudes.  Similar to TEST1, each
       sparse vector is set to the same amplitude.

       /vqtrainsp testamp.flt 20 8 test.txt

    */

    for(i=0; i<NVEC; i++) {
	for(amp=1.0; amp<=8.0; amp++) {
	    pitch = P_MIN + (P_MAX-P_MIN)*((float)rand()/RAND_MAX);
	    Wo = TWO_PI/pitch;
	    L = floor(PI/Wo); 
	    //printf("pitch %f Wo %f L %d\n", pitch, Wo, L);

	    for(m=0; m<MAX_AMP; m++) {
		sparse_pe[m] = 0.0;
	    }

	    for(m=1; m<=L; m++) {
		index = MAX_AMP*m*Wo/PI;
		assert(index < MAX_AMP);
		noisey_amp = amp + 0.2*(1.0 - 2.0*rand()/RAND_MAX);
		sparse_pe[index] = noisey_amp;
		#ifdef DBG
		if (m < MAX_AMP/8)
		    printf(" %4.3f ", noisey_amp);
		#endif
	    }
            #ifdef DBG
	    printf("\n");
	    #endif

	    fwrite(sparse_pe, sizeof(float), MAX_AMP/8, f);
	}
    }

    #endif

    return 0;
}
