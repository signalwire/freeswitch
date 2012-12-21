/*
  create_interleaver.c
  David Rowe
  May 27 2012

  Creates an interleaver for Codec 2.
*/

#include <assert.h>
#include <stdlib.h>
#include <stdio.h>

int main(int argc, char * argv[]) {
    int   m,i, src_bit, dest_bit;
    FILE *f;
    int  *interleaver;
    
    if (argc != 3) {
	printf("usage: %s InterleaverBits InterleaverFile\n", argv[0]);
	exit(1);
    }

    m = atoi(argv[1]);
    f = fopen(argv[2],"wt");
    assert(f != NULL);


    interleaver = (int*)malloc(m*sizeof(int));
    assert(interleaver != NULL);
    for(i=0; i<m; i++)
	interleaver[i] = -1;

    src_bit = 0;
    while(src_bit != m) {
	dest_bit = ((float)rand()/RAND_MAX)*m;
	if (interleaver[dest_bit] == -1) {
	    interleaver[dest_bit] = src_bit;
	    src_bit++;
	}
    }

    for(i=0; i<m; i++) {
	fprintf(f, "%d\n", interleaver[i]);
    }

    fclose(f);
    return 0;
}
