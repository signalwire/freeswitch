/*
  polar2rect.c
  David Rowe 28 July 2013

  Convert a file of sparse phases in polar (angle) format to a file in rect
  format.
*/

#include <assert.h>
#include <math.h>
#include <stdlib.h>
#include <stdio.h>

typedef struct {
    float real;
    float imag;
} COMP;

int main(int argc, char *argv[]) {
    FILE *fpolar;
    FILE *frect;
    float polar;
    COMP  rect;

    if (argc != 3) {
	printf("usage: %s polarFile rectFile\n", argv[0]);
	exit(0);
    }

    fpolar = fopen(argv[1], "rb");
    assert(fpolar != NULL);
    frect = fopen(argv[2], "wb");
    assert(frect != NULL);

    while (fread(&polar, sizeof(float), 1, fpolar) != 0) {
	if (polar == 0.0) {
	    /* this values indicates the VQ training should ignore
	       this vector element.  It's not a valid phase as it
	       doesn't have mangitude of 1.0 */
	    rect.real = 0.0;
	    rect.imag = 0.0;
	}
	else {
	    rect.real = cos(polar);
	    rect.imag = sin(polar);
	}
	fwrite(&rect, sizeof(COMP), 1, frect);
    }

    fclose(fpolar);
    fclose(frect);

    return 0;
}
