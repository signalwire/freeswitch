/*---------------------------------------------------------------------------*\
                                                                             
  FILE........: fdmdv_put_test_bits.c
  AUTHOR......: David Rowe  
  DATE CREATED: 1 May 2012
                                                                             
  Using a file of packed test bits as input, determines bit error
  rate.  Useful for testing fdmdv_demod.

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
    FILE         *fin;
    struct FDMDV *fdmdv;
    char          packed_bits[BYTES_PER_CODEC_FRAME];
    int           rx_bits[2*FDMDV_BITS_PER_FRAME];
    int           i, bit, byte;
    int           test_frame_sync, bit_errors, total_bit_errors, total_bits, ntest_bits;

    if (argc < 2) {
	printf("usage: %s InputBitFile\n", argv[0]);
	printf("e.g    %s test.c2\n", argv[0]);
	exit(1);
    }

    if (strcmp(argv[1], "-") == 0) fin = stdin;
    else if ( (fin = fopen(argv[1],"rb")) == NULL ) {
	fprintf(stderr, "Error opening input bit file: %s: %s.\n",
         argv[1], strerror(errno));
	exit(1);
    }

    fdmdv = fdmdv_create();
    total_bit_errors = 0;
    total_bits = 0;

    while(fread(packed_bits, sizeof(char), BYTES_PER_CODEC_FRAME, fin) == BYTES_PER_CODEC_FRAME) {
	/* unpack bits, MSB first */

	bit = 7; byte = 0;
	for(i=0; i<BITS_PER_CODEC_FRAME; i++) {
	    rx_bits[i] = (packed_bits[byte] >> bit) & 0x1;
	    //printf("%d 0x%x %d\n", i, packed_bits[byte], rx_bits[i]);
	    bit--;
	    if (bit < 0) {
		bit = 7;
		byte++;
	    }
	}
	assert(byte == BYTES_PER_CODEC_FRAME);

	fdmdv_put_test_bits(fdmdv, &test_frame_sync, &bit_errors, &ntest_bits, rx_bits);
	if (test_frame_sync == 1) {
	    total_bit_errors += bit_errors;
	    total_bits = total_bits + ntest_bits;
	    printf("+");
	}
	else
	    printf("-");
  	fdmdv_put_test_bits(fdmdv, &test_frame_sync, &bit_errors, &ntest_bits, &rx_bits[FDMDV_BITS_PER_FRAME]);
	if (test_frame_sync == 1) {
	    total_bit_errors += bit_errors;
	    total_bits = total_bits + ntest_bits;
	    printf("+");
	}
	else
	    printf("-");
	
	/* if this is in a pipeline, we probably don't want the usual
	   buffering to occur */

        if (fin == stdin) fflush(stdin);
    }

    fclose(fin);
    fdmdv_destroy(fdmdv);

    printf("\nbits %d  errors %d  BER %1.4f\n", total_bits, total_bit_errors, (float)total_bit_errors/(1E-6+total_bits) );
    return 0;
}

