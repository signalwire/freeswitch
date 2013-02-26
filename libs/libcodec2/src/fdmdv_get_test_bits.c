/*---------------------------------------------------------------------------*\
                                                                             
  FILE........: fdmdv_get_test_bits.c
  AUTHOR......: David Rowe  
  DATE CREATED: 1 May 2012
                                                                             
  Generates a file of packed test bits, useful for input to fdmdv_mod.

\*---------------------------------------------------------------------------*/


/*
  Copyright (C) 2012 David Rowe

  All rights reserved.

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU Lesser General Public License version 2, as
  published by the Free Software Foundation.  This program is
  distributed in the hope that it will be useful, but WITHOUT ANY
  WARRANTY; without even the implied warranty of MERCHANTABILITY or
  FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public
  License for more details.

  You should have received a copy of the GNU Lesser General Public License
  along with this program; if not, see <http://www.gnu.org/licenses/>.
*/

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <errno.h>

#include "fdmdv.h"

#define BITS_PER_CODEC_FRAME (2*FDMDV_BITS_PER_FRAME)
#define BYTES_PER_CODEC_FRAME (BITS_PER_CODEC_FRAME/8)

int main(int argc, char *argv[])
{
    FILE         *fout;
    struct FDMDV *fdmdv;
    char          packed_bits[BYTES_PER_CODEC_FRAME];
    int           tx_bits[2*FDMDV_BITS_PER_FRAME];
    int           n, i, bit, byte;
    int           numBits, nCodecFrames;

    if (argc < 3) {
	printf("usage: %s OutputBitFile numBits\n", argv[0]);
	printf("e.g    %s test.c2 1400\n", argv[0]);
	exit(1);
    }

    if (strcmp(argv[1], "-") == 0) fout = stdout;
    else if ( (fout = fopen(argv[1],"wb")) == NULL ) {
	fprintf(stderr, "Error opening output bit file: %s: %s.\n",
         argv[1], strerror(errno));
	exit(1);
    }

    numBits = atoi(argv[2]);
    nCodecFrames = numBits/BITS_PER_CODEC_FRAME;

    fdmdv = fdmdv_create();

    for(n=0; n<nCodecFrames; n++) {

	fdmdv_get_test_bits(fdmdv, tx_bits);
	fdmdv_get_test_bits(fdmdv, &tx_bits[FDMDV_BITS_PER_FRAME]);
	
	/* pack bits, MSB received first  */

	bit = 7; byte = 0;
	memset(packed_bits, 0, BYTES_PER_CODEC_FRAME);
	for(i=0; i<BITS_PER_CODEC_FRAME; i++) {
	    packed_bits[byte] |= (tx_bits[i] << bit);
	    bit--;
	    if (bit < 0) {
		bit = 7;
		byte++;
	    }
	}
	assert(byte == BYTES_PER_CODEC_FRAME);

	fwrite(packed_bits, sizeof(char), BYTES_PER_CODEC_FRAME, fout);
 
	/* if this is in a pipeline, we probably don't want the usual
	   buffering to occur */

        if (fout == stdout) fflush(stdout);
    }

    fclose(fout);
    fdmdv_destroy(fdmdv);

    return 0;
}

