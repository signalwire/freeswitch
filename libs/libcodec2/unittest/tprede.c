/* 
   tpre_de.c
   David Rowe
   Sep 24 2012

   Unit test to generate the combined impulse response of pre & de-emphasis filters.

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
#include "lpc.h"

#define N                        10 
#define F                        10

int main() {
    FILE  *fprede;
    float  Sn[N], Sn_pre[N], Sn_de[N];
    float  pre_mem = 0.0, de_mem = 0.0;
    int    i, f;

    fprede = fopen("prede.txt", "wt");
    assert(fprede != NULL);
    
    for(i=0; i<N; i++)
	Sn[i] = 0.0;

    Sn[0]= 1.0;

    for(f=0; f<F; f++) {
	pre_emp(Sn_pre, Sn, &pre_mem, N);
	de_emp(Sn_de, Sn_pre, &de_mem, N);
	for(i=0; i<N; i++) {
	    fprintf(fprede, "%f\n", Sn_de[i]);		
	}
	Sn[0] = 0.0;
    }

    fclose(fprede);

    return 0;
}
