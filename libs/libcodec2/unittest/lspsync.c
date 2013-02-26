/*
   lspsync.c
   David Rowe 24 May 2012

   Attempt at using LSP information to provide frame sync.  If we have
   correct frame alignment, LSPs will not need sorting.

   However this method as tested appears unreliable, often several
   sync positions per frame are found, even with a F=10 memory.  For
   F=6, about 87% relaible.  This might be useful if combined with a
   another sync method, for example a single alternating sync bit per
   frame.

*/

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include "codec2.h"
#include "defines.h"
#include "quantise.h"

#define F 6                /* look at LSP ordering in F-1 frames         */
#define CORRECT_OFFSET 10  /* LSPs start 10 bits int frame qt 2400 bit/s */


static int check_candidate(char bits[], int offset)
{
    int          i;
    int          lsp_indexes[LPC_ORD];
    float        lsps[LPC_ORD];
    unsigned int nbit = offset;
    int          swaps;
   
    for(i=0; i<LSP_SCALAR_INDEXES; i++) {
	lsp_indexes[i] = unpack(bits, &nbit, lsp_bits(i));
    }
    decode_lsps_scalar(lsps, lsp_indexes, LPC_ORD);
    swaps = check_lsp_order(lsps, LPC_ORD);

    return swaps;
}

int main(int argc, char *argv[]) {
    struct CODEC2 *c2;
    int            i,offset, nsamples, nbits, nbytes, frames;
    short         *speech;
    char          *bits;
    FILE          *fin;
    int            swaps, pass, fail, match;

    c2 = codec2_create(CODEC2_MODE_2400);
    nsamples = codec2_samples_per_frame(c2);
    nbits = codec2_bits_per_frame(c2);
    nbytes = nbits/8;
    speech = (short*)malloc(nsamples*sizeof(short));

    /* keep FRAMES frame memory of bit stream */

    bits = (char*)malloc(F*nbytes*sizeof(char));
    for(i=0; i<F*nbytes; i++)
	bits[i] = 0;

    fin = fopen("../raw/hts1a.raw", "rb");
    assert(fin != NULL);
    match = pass = fail = frames = 0;

    /* prime memeory with first frame to ensure we don't start
       checking until we have two frames of coded bits */

    fread(speech, sizeof(short), nsamples, fin);
    frames++;
    codec2_encode(c2, &bits[(F-2)*nbytes], speech);

    /* OK start looking for correct frame offset */

    while(fread(speech, sizeof(short), nsamples, fin) == nsamples) {
	frames++;
	codec2_encode(c2, &bits[(F-1)*nbytes], speech);

	for(offset=0; offset<nbits; offset++) {
	    swaps = check_candidate(bits, offset);
	    if (swaps == 0) {
	    
		/* OK found a candidate .. lets check a F-1 frames in total */

		for(i=0; i<(F-1); i++)
		    swaps += check_candidate(bits, offset + nbits*i);
		
		if (swaps == 0) {
		    printf("frame %d offset: %d swaps: %d\n", frames, offset, swaps);
		    match++;
		    if (offset == CORRECT_OFFSET)
			pass++;
		    else
			fail++;
		}
	    }
	}
       
	/* update F frame memory of bits */

	for(i=0; i<nbytes*(F-1); i++)
	    bits[i] = bits[i+nbytes];
    }

    fclose(fin);
    free(speech);
    free(bits);
    codec2_destroy(c2);

    printf("passed %f %%\n", (float)pass*100.0/match);

    return 0;
}
