/*---------------------------------------------------------------------------*\

  FILE........: c2dec.c
  AUTHOR......: David Rowe
  DATE CREATED: 23/8/2010

  Decodes a file of bits to a file of raw speech samples using codec2. Demo
  program for codec2.

  NOTE: the bit file is not packed, 51 bits/frame actually consumes 51
  bytes/frame on disk.  If you are using this for a real world
  application you may want to pack the 51 bytes into 7 bytes.

\*---------------------------------------------------------------------------*/

/*
  Copyright (C) 2010 David Rowe

  All rights reserved.

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU Lesser General Public License version 2.1, as
  published by the Free Software Foundation.  This program is
  distributed in the hope that it will be useful, but WITHOUT ANY
  WARRANTY; without even the implied warranty of MERCHANTABILITY or
  FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public
  License for more details.

  You should have received a copy of the GNU Lesser General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/

#include "codec2.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

int main(int argc, char *argv[])
{
    static const int bitsSize = ((CODEC2_BITS_PER_FRAME + 7) / 8);
    void *codec2;
    FILE *fin;
    FILE *fout;
    short buf[CODEC2_SAMPLES_PER_FRAME];
    unsigned char  bits[bitsSize];

    if (argc != 3) {
	printf("usage: %s InputBitFile OutputRawSpeechFile\n", argv[0]);
	exit(1);
    }
 
    if ( (fin = fopen(argv[1],"rb")) == NULL ) {
	fprintf(stderr, "Error opening input bit file: %s: %s.\n",
         argv[1], strerror(errno));
	exit(1);
    }

    if ( (fout = fopen(argv[2],"wb")) == NULL ) {
	fprintf(stderr, "Error opening output speech file: %s: %s.\n",
         argv[2], strerror(errno));
	exit(1);
    }

    codec2 = codec2_create();

    while(fread(bits, sizeof(char), bitsSize, fin) == bitsSize) {
	codec2_decode(codec2, buf, bits);
	fwrite(buf, sizeof(short), CODEC2_SAMPLES_PER_FRAME, fout);
    }

    codec2_destroy(codec2);

    fclose(fin);
    fclose(fout);

    return 0;
}
