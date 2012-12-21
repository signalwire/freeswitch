/*---------------------------------------------------------------------------*\
                                                                             
  FILE........: fdmdv_demod.c
  AUTHOR......: David Rowe  
  DATE CREATED: April 30 2012
                                                                             
  Given an input raw file (8kHz, 16 bit shorts) of FDMDV modem samples
  outputs a file of bits.  The output file is assumed to be arranged
  as codec frames of 56 bits (7 bytes) which are received as two 28
  bit modem frames.

  Demod states can be optionally logged to an Octave file for display
  using the Octave script fdmdv_demod_c.m.  This is useful for
  checking demod performance.
                                                                             
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
#include "octave.h"

#define BITS_PER_CODEC_FRAME (2*FDMDV_BITS_PER_FRAME)
#define BYTES_PER_CODEC_FRAME (BITS_PER_CODEC_FRAME/8)

/* lof of information we want to dump to Octave */

#define MAX_FRAMES 50*60 /* 1 minute at 50 symbols/s */

int main(int argc, char *argv[])
{
    FILE         *fin, *fout;
    struct FDMDV *fdmdv;
    char          packed_bits[BYTES_PER_CODEC_FRAME];
    int           rx_bits[FDMDV_BITS_PER_FRAME];
    int           codec_bits[2*FDMDV_BITS_PER_FRAME];
    COMP          rx_fdm[FDMDV_MAX_SAMPLES_PER_FRAME];
    short         rx_fdm_scaled[FDMDV_MAX_SAMPLES_PER_FRAME];
    int           i, bit, byte, c;
    int           nin, nin_prev;
    int           sync_bit;
    int           state, next_state;
    int           f;
    FILE         *foct = NULL;
    struct FDMDV_STATS stats;
    float        *rx_fdm_log;
    int           rx_fdm_log_col_index;
    COMP          rx_symbols_log[FDMDV_NSYM][MAX_FRAMES];
    int           coarse_fine_log[MAX_FRAMES];
    float         rx_timing_log[MAX_FRAMES];
    float         foff_log[MAX_FRAMES];
    int           sync_bit_log[MAX_FRAMES];
    int           rx_bits_log[FDMDV_BITS_PER_FRAME*MAX_FRAMES];
    float         snr_est_log[MAX_FRAMES];
    float        *rx_spec_log;
    int           max_frames_reached;

    if (argc < 3) {
	printf("usage: %s InputModemRawFile OutputBitFile [OctaveDumpFile]\n", argv[0]);
	printf("e.g    %s hts1a_fdmdv.raw hts1a.c2\n", argv[0]);
	exit(1);
    }

    if (strcmp(argv[1], "-")  == 0) fin = stdin;
    else if ( (fin = fopen(argv[1],"rb")) == NULL ) {
	fprintf(stderr, "Error opening input modem sample file: %s: %s.\n",
         argv[1], strerror(errno));
	exit(1);
    }

    if (strcmp(argv[2], "-") == 0) fout = stdout;
    else if ( (fout = fopen(argv[2],"wb")) == NULL ) {
	fprintf(stderr, "Error opening output bit file: %s: %s.\n",
         argv[2], strerror(errno));
	exit(1);
    }

    /* malloc some of the bigger variables to prevent out of stack problems */

    rx_fdm_log = (float*)malloc(sizeof(float)*FDMDV_MAX_SAMPLES_PER_FRAME*MAX_FRAMES);
    assert(rx_fdm_log != NULL);
    rx_spec_log = (float*)malloc(sizeof(float)*FDMDV_NSPEC*MAX_FRAMES);
    assert(rx_spec_log != NULL);

    fdmdv = fdmdv_create();
    f = 0;
    state = 0;
    nin = FDMDV_NOM_SAMPLES_PER_FRAME;
    rx_fdm_log_col_index = 0;
    max_frames_reached = 0;

    while(fread(rx_fdm_scaled, sizeof(short), nin, fin) == nin)
    {
	for(i=0; i<nin; i++) {
	    rx_fdm[i].real = (float)rx_fdm_scaled[i]/FDMDV_SCALE;
            rx_fdm[i].imag = 0;
        }
	nin_prev = nin;
	fdmdv_demod(fdmdv, rx_bits, &sync_bit, rx_fdm, &nin);

	/* log data for optional Octave dump */

	if (f < MAX_FRAMES) {
	    fdmdv_get_demod_stats(fdmdv, &stats);

	    /* log modem states for later dumping to Octave log file */

	    memcpy(&rx_fdm_log[rx_fdm_log_col_index], rx_fdm, sizeof(float)*nin_prev);
	    rx_fdm_log_col_index += nin_prev;

	    for(c=0; c<FDMDV_NSYM; c++)
		rx_symbols_log[c][f] = stats.rx_symbols[c];
	    foff_log[f] = stats.foff;
	    rx_timing_log[f] = stats.rx_timing;
	    coarse_fine_log[f] = stats.fest_coarse_fine;
	    sync_bit_log[f] = sync_bit;
	    memcpy(&rx_bits_log[FDMDV_BITS_PER_FRAME*f], rx_bits, sizeof(int)*FDMDV_BITS_PER_FRAME);
	    snr_est_log[f] = stats.snr_est;

	    fdmdv_get_rx_spectrum(fdmdv, &rx_spec_log[f*FDMDV_NSPEC], rx_fdm, nin_prev);

	    f++;
	}
	
	if ((f == MAX_FRAMES) && !max_frames_reached) {
	    fprintf(stderr,"MAX_FRAMES exceed in Octave log, log truncated\n");
	    max_frames_reached = 1;
	}

	/* state machine to output codec bits only if we have a 0,1
	   sync bit sequence */

	next_state = state;
	switch (state) {
	case 0:
	    if (sync_bit == 0) {
		next_state = 1;
		memcpy(codec_bits, rx_bits, FDMDV_BITS_PER_FRAME*sizeof(int));
	    }
	    else
		next_state = 0;
	    break;
	case 1:
	    if (sync_bit == 1) {
		memcpy(&codec_bits[FDMDV_BITS_PER_FRAME], rx_bits, FDMDV_BITS_PER_FRAME*sizeof(int));

		/* pack bits, MSB received first  */

		bit = 7; byte = 0;
		memset(packed_bits, 0, BYTES_PER_CODEC_FRAME);
		for(i=0; i<BITS_PER_CODEC_FRAME; i++) {
		    packed_bits[byte] |= (codec_bits[i] << bit);
		    bit--;
		    if (bit < 0) {
			bit = 7;
			byte++;
		    }
		}
		assert(byte == BYTES_PER_CODEC_FRAME);

		fwrite(packed_bits, sizeof(char), BYTES_PER_CODEC_FRAME, fout);
	    }
	    next_state = 0;
	    break;
	}	
	state = next_state;

	/* if this is in a pipeline, we probably don't want the usual
	   buffering to occur */

        if (fout == stdout) fflush(stdout);
        if (fin == stdin) fflush(stdin);         
    }

    /* Optional dump to Octave log file */

    if (argc == 4) {

	/* make sure 3rd arg is not just the pipe command */

	if (strcmp(argv[3],"|")) {
	    if ((foct = fopen(argv[3],"wt")) == NULL ) {
		fprintf(stderr, "Error opening Octave dump file: %s: %s.\n",
			argv[3], strerror(errno));
		exit(1);
	    }
	    octave_save_float(foct, "rx_fdm_log_c", rx_fdm_log, 1, rx_fdm_log_col_index, FDMDV_MAX_SAMPLES_PER_FRAME);  
	    octave_save_complex(foct, "rx_symbols_log_c", (COMP*)rx_symbols_log, FDMDV_NSYM, f, MAX_FRAMES);  
	    octave_save_float(foct, "foff_log_c", foff_log, 1, f, MAX_FRAMES);  
	    octave_save_float(foct, "rx_timing_log_c", rx_timing_log, 1, f, MAX_FRAMES);  
	    octave_save_int(foct, "coarse_fine_log_c", coarse_fine_log, 1, f);  
	    octave_save_int(foct, "rx_bits_log_c", rx_bits_log, 1, FDMDV_BITS_PER_FRAME*f);
	    octave_save_int(foct, "sync_bit_log_c", sync_bit_log, 1, f);  
	    octave_save_float(foct, "snr_est_log_c", snr_est_log, 1, f, MAX_FRAMES);  
	    octave_save_float(foct, "rx_spec_log_c", rx_spec_log, f, FDMDV_NSPEC, FDMDV_NSPEC);  
	    fclose(foct);
	}
    }

    //fdmdv_dump_osc_mags(fdmdv);

    fclose(fin);
    fclose(fout);
    free(rx_fdm_log);
    free(rx_spec_log);
    fdmdv_destroy(fdmdv);

    return 0;
}

