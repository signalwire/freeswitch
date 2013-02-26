/* 
   de.c
   David Rowe
   Sep 26 2012

   Takes audio from a file, de-emphasises, and sends to output file.
*/

#include <assert.h>
#include <errno.h>
#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "lpc.h"

#define N 80

int main(int argc, char *argv[]) {
    FILE  *fin, *fout;
    short  buf[N];
    float  Sn[N], Sn_de[N];
    float  de_mem = 0.0;
    int    i;

    if (argc != 3) {
	printf("usage: de InputRawSpeechFile OutputRawSpeechFile\n");
	printf("e.g    de input.raw output.raw");
	exit(1);
    }
 
    if (strcmp(argv[1], "-")  == 0) fin = stdin;
    else if ( (fin = fopen(argv[1],"rb")) == NULL ) {
	fprintf(stderr, "Error opening input speech file: %s: %s.\n",
         argv[1], strerror(errno));
	exit(1);
    }

    if (strcmp(argv[2], "-") == 0) fout = stdout;
    else if ( (fout = fopen(argv[2],"wb")) == NULL ) {
	fprintf(stderr, "Error opening output speech file: %s: %s.\n",
         argv[2], strerror(errno));
	exit(1);
    }

    while(fread(buf, sizeof(short), N, fin) == N) {
	for(i=0; i<N; i++)
	    Sn[i] = buf[i];
	de_emp(Sn_de, Sn, &de_mem, N);
	for(i=0; i<N; i++)
	    buf[i] = Sn_de[i];
	fwrite(buf, sizeof(short), N, fout);
    }

    fclose(fin);
    fclose(fout);

    return 0;
}
