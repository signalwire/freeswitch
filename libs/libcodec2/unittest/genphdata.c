/*
  genphdata.c

  Generates test phase data for vqtrainph testing.
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

#define NVEC 100000
#define D    2
#define E    8

int main(void) {
    FILE *f=fopen("testph.flt", "wb");
    int   i, m, L, index;
    float angle, noisey_angle, pitch, Wo;
    COMP  c;
    COMP  sparse_pe[MAX_AMP];

    #ifdef TEST1
    for(i=0; i<D*E; i++) {
	c.real = cos(i*TWO_PI/(M*D));
	c.imag = sin(i*TWO_PI/(M*D));
	fwrite(&c, sizeof(COMP), 1, f);
    }
    #endif

    #ifdef TEST2
    /* 
       Bunch of random phases, should get std dev per element of
       pi/(sqrt(3)*pow(2,b/D)), or 0.321 for (b=5, D=2):
       
       ./vqtrainph testph.flt 2 32 test.txt
    */

    for(i=0; i<NVEC; i++) {
	angle = PI*(1.0 - 2.0*rand()/RAND_MAX);
	c.real = cos(angle);
	c.imag = sin(angle);
	fwrite(&c, sizeof(COMP), 1, f);
    }
    #endif

    #define TEST3
    #ifdef TEST3
    /* 
       Data for testing training in sparse phases. No correlation, so
       should be same performance as TEST2.  Attempting to train a
       MAX_AMP/4 = 20 (first 1 kHz) phase quantiser.

    */

    angle = 0;
    for(i=0; i<NVEC; i++) {
	pitch = P_MIN + (P_MAX-P_MIN)*((float)rand()/RAND_MAX);
	//pitch = 40;
	Wo = TWO_PI/pitch;
	L = floor(PI/Wo); 
	//printf("pitch %f Wo %f L %d\n", pitch, Wo, L);

	for(m=0; m<MAX_AMP; m++) {
	    sparse_pe[m].real = 0.0;
	    sparse_pe[m].imag = 0.0;
	}

	angle += PI/8;
	for(m=1; m<=L; m++) {
	    noisey_angle = angle + (PI/16)*(1.0 - 2.0*rand()/RAND_MAX);	    
	    //angle = (PI/16)*(1.0 - 2.0*rand()/RAND_MAX);	    
	    index = MAX_AMP*m*Wo/PI;
	    assert(index < MAX_AMP);
	    sparse_pe[index].real = cos(noisey_angle);
	    sparse_pe[index].imag = sin(noisey_angle);
	}

	fwrite(&sparse_pe, sizeof(COMP), MAX_AMP/4, f);
    }
	    
    #endif

    return 0;
}
