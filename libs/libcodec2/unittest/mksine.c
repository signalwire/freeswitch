/*
  mksine.c
  David Rowe 
  10 Nov 2010

  Creates a file of sine wave samples.
*/

#include <stdlib.h>
#include <stdio.h>
#include <math.h>

#define TWO_PI     6.283185307
#define N          8000
#define FS         8000.0
#define AMP        1000.0

int main(int argc, char *argv[]) {
    FILE *f;
    int   i;
    float freq;
    short buf[N];

    if (argc != 3) {
	printf("usage: %s outputFile frequencyHz\n", argv[0]);
	exit(1);
    }

    f = fopen(argv[1] ,"wb");
    freq = atof(argv[2]);

    for(i=0; i<N; i++)
	buf[i] = AMP*cos(freq*i*(TWO_PI/FS));

    fwrite(buf, sizeof(short), N, f);

    return 0;
}
